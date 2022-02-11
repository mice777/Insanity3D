#define TABBLER_FULL
#include <tabler2.h>
#include "ITabCore.h"
#include <insanity\assert.h>


//----------------------------

bool Compress(void *in, size_t in_size, void **out, size_t *out_size, bool decompress);

//----------------------------

const char *C_table::type_names[TE_LAST] = {
   "NULL",
   "BOOL",
   "INT",
   "FLOAT",
   "ENUM",
   "STRING",
   "COLOR_RGB",
   "COLOR_VECTOR",
   "BRANCH",
   "ARRAY",
};

//----------------------------

void C_table::Close(){

   delete[] desc; desc = NULL;
   delete[] data; data = NULL;
   num_items = 0;
   memset(&guid, 0, sizeof(guid));
};

//----------------------------

int C_table::InsertItem(PC_table_template tt, dword templ_indx, int &storage,
   vector<byte> &v_defaults, dword array_len){

   E_TABLE_ELEMENT_TYPE type = tt->GetItemType(templ_indx);
   int i, j, align, size_of;
   switch(type){
   case TE_ARRAY:
      {
         int array_size = tt->ArraySize(templ_indx);
         i = Max(1ul, array_len);
         j = InsertItem(tt, templ_indx+1, storage, v_defaults, array_size*i);
         return j;
      }
   case TE_BRANCH:
      i = tt->BranchDepth(templ_indx++);
      while(i--){
         templ_indx = InsertItem(tt, templ_indx, storage, v_defaults, array_len);
         if(templ_indx==-1)
            break;
      }
      return templ_indx;
   case TE_NULL:
      return -1;
   case TE_STRING:
      align = 1;
      break;
   case TE_COLOR_RGB:
      align = 1;
      break;
   case TE_COLOR_VECTOR:
      align = sizeof(float);
      break;
   default:
      align = tt->SizeOf(templ_indx);
   }

   size_of = tt->SizeOf(templ_indx);

                           //align storage
   i = storage;
   storage += align-1; storage &= -align;
                           //pad zero bytes
   i = storage-i;
   while(i--)
      v_defaults.push_back(0);

   union{
      int u_i;
      float u_f;
      byte u_b[4];
   } au;
   i = array_len ? array_len : 1;
   while(i--)
   switch(type){
   case TE_INT:
      au.u_i = (int)tt->GetDefaultVal(templ_indx);
      v_defaults.push_back(au.u_b[0]);
      v_defaults.push_back(au.u_b[1]);
      v_defaults.push_back(au.u_b[2]);
      v_defaults.push_back(au.u_b[3]);
      break;

   case TE_FLOAT:
      au.u_f = tt->GetDefaultVal(templ_indx);
      v_defaults.push_back(au.u_b[0]);
      v_defaults.push_back(au.u_b[1]);
      v_defaults.push_back(au.u_b[2]);
      v_defaults.push_back(au.u_b[3]);
      break;

   case TE_BOOL:
      v_defaults.push_back((bool)tt->GetDefaultVal(templ_indx));
      break;

   case TE_COLOR_RGB:
      {
         dword color = tt->te[templ_indx].int_min;
         v_defaults.push_back(color>>16);
         v_defaults.push_back(color>>8);
         v_defaults.push_back(color>>0);
      }
      break;

   case TE_ENUM:
      {
         assert(size_of==sizeof(ENUM_TYPE));
         au.u_i = (int)tt->GetDefaultVal(templ_indx);
         for(int i=0; i<sizeof(ENUM_TYPE); i++)
            v_defaults.push_back(au.u_b[i]);
      }
      break;

   case TE_STRING:
      {
         int ii = 0;
         float def = tt->GetDefaultVal(templ_indx);
         const char *cp = *(const char**)&def;
         if(cp){
            ii = Min((int)strlen(cp), size_of);
            for(int i=0; i<ii; i++){
               v_defaults.push_back(cp[i]);
            }
         }
         for(; ii<size_of; ii++) v_defaults.push_back(0);
      }
      break;
   default:
      for(int ii=0; ii<size_of; ii++) v_defaults.push_back(0);
   }

   int tab_indx = tt->GetItemTableIndex(templ_indx);
   if(desc[tab_indx].type){
                              //fatal error - item on index specified twice
      FatalError(C_xstr("Table '%': index % initialized multiple times") %tt->caption %tab_indx);
   }
   desc[tab_indx].type = type;
   desc[tab_indx].array_len = array_len;
   desc[tab_indx].offset = storage;
   desc[tab_indx].type = type;
   if(type==TE_STRING)
      desc[tab_indx].max_string_size = size_of;
   if(array_len) size_of *= array_len;
   storage += size_of;
   return templ_indx + 1;
}

//----------------------------

bool TABAPI C_table::Load(const void *src, dword flags){

   if(flags&TABOPEN_UPDATE){
      if(!IsLoaded()){
                           //open without update
         return Load(src, flags & ~TABOPEN_UPDATE);
      }
      C_table *tmp_table = CreateTable();
      bool b = tmp_table->Load(src, flags & ~TABOPEN_UPDATE);
      if(b){
                           //copy matching members
         int numi = NumItems(), tmp_numi = tmp_table->NumItems();
                              //bugs!  1) not sizeof   2) not to clear anything
         int i = Min(numi, tmp_numi);
         while(i--){
            E_TABLE_ELEMENT_TYPE type = GetItemType(i);
            if(type==tmp_table->GetItemType(i)){
                           //copy member
               int sz = SizeOf(i);
               int al = ArrayLen(i);
               if(!al) al = 1;
               int tmp_sz = tmp_table->SizeOf(i);
               int tmp_al = tmp_table->ArrayLen(i);
               if(!tmp_al) tmp_al = 1;
               sz = Min(sz*al, tmp_sz*tmp_al);
               memcpy(data+desc[i].offset,
                  tmp_table->data+tmp_table->desc[i].offset, sz);
                           //crop string
               if(type==TE_STRING && sz)
                  data[desc[i].offset + sz - 1] = 0;
            }
         }
      }
      tmp_table->Release();
      return b;
   }else
   if(flags&TABOPEN_TEMPLATE){
      PC_table_template tt = (PC_table_template)src;
      if(!tt) return false;
      Close();
                           //get maximal template index
      int i;
      int max_index = -1;
      for(i=0; tt->GetItemType(i)!=TE_NULL; i++)
         if(tt->IsDataType(i)) max_index = Max(max_index, (int)tt->GetItemTableIndex(i));
      if(max_index == -1) return false;
      num_items = max_index + 1;
      desc = new S_desc_item[num_items];
      if(!desc){
         Close();
         return false;
      }
      //memcpy(guid, tt->GetGUID(), sizeof(guid));
      int storage = 0;
      vector<byte> v_defaults;
                              //guess size...
      v_defaults.reserve(num_items*sizeof(int)*2);
      for(i=0; ; ){
         E_TABLE_ELEMENT_TYPE type = tt->GetItemType(i);
         if(type==TE_NULL)
            break;
         i = InsertItem(tt, i, storage, v_defaults);
         if(i==-1){
            delete[] desc;
            return false;
         }
      }

      assert(v_defaults.size() == (dword)storage);
      data = new byte[data_size=storage];
      if(!data){
         Close();
         return false;
      }
      memcpy(data, &v_defaults[0], data_size);
      return true;
   }else
   if(flags&(TABOPEN_FILENAME | TABOPEN_FILEHANDLE)){
      C_cache cache, *ch;
      if(flags&TABOPEN_FILENAME){
                           //open file for reading
         if(!cache.open((const char*)src, CACHE_READ))
            return false;
         ch = &cache;
      }else
         ch = (C_cache*)src;
      Close();
      bool ok=false;
      for(;;){
                           //read header
         S_file_header hdr;
         if(ch->read((byte*)&hdr, sizeof(hdr)) != sizeof(hdr)) break;
                           //check header ('TABL')
         if(hdr.ID!=0x4c424154) break;

                           //read GUID
         if(ch->read((byte*)&guid, sizeof(guid)) != sizeof(guid)) break;

         lock = hdr.lock;

         ReadModifyData(ch, hdr);
                           //alloc & read descriptor
         desc = new S_desc_item[num_items=hdr.num_items];
         if(!ch->read((byte*)desc, num_items*sizeof(S_desc_item))) break;

                           //alloc & read data
         data = new byte[data_size=hdr.data_size];
         if(ch->read(data, data_size)!=data_size) break;
         if(hdr.flags&HDRFLAGS_COMPRESS){
            byte *compr_mem;
            size_t decomp_size;
            Compress(data, data_size, (void**)&compr_mem, &decomp_size, true);
            delete[] data;
            data = compr_mem;
            data_size = decomp_size;
         }

         ok=true;
         break;
      }

      if(flags&TABOPEN_FILENAME) cache.close();
      if(!ok) Close();
      return ok;
   }else
   if(flags&TABOPEN_DUPLICATE){
      Close();
      C_table *tp = (C_table*)src;
      lock = tp->lock;
      memcpy(guid, tp->guid, sizeof(guid));
      desc = new S_desc_item[num_items=tp->num_items];
      memcpy(desc, tp->desc, num_items*sizeof(S_desc_item));
      data = new byte[data_size=tp->data_size];
      memcpy(data, tp->data, data_size);
      return true;
   }
   return false;
};

//----------------------------

bool C_table::Save(const void *dst, dword flags) const{

                           //check if we have what to save
   if(!desc || !data)
      return false;

   if(flags&(TABOPEN_FILENAME | TABOPEN_FILEHANDLE)){
      C_cache cache, *ch;
      if(flags&TABOPEN_FILENAME){
                           //open file for writting
         if(!cache.open((const char*)dst, CACHE_WRITE)) return false;
         ch=&cache;
      }else
         ch = (C_cache*)dst;

                           //compress data
      byte *compr_mem;
      size_t comp_size;
      Compress(data, data_size, (void**)&compr_mem, &comp_size, false);
                           //write header
      S_file_header hdr={
         0x4c424154,       //'TABL'
         lock,
         num_items, comp_size,
         HDRFLAGS_COMPRESS,
         0
      };

      ch->write((char*)&hdr, sizeof(hdr));

                           //write GUID
      ch->write((char*)&guid, sizeof(guid));

                           //write descriptor
      ch->write((char*)desc, num_items*sizeof(S_desc_item));

                           //write data
      ch->write((char*)compr_mem, comp_size);
      delete[] (byte*)compr_mem;

      if(flags&TABOPEN_FILENAME) cache.close();
   }else{
      return false;
   }
   modified = false;
   return true;
};

//----------------------------

bool C_table::IsModified() const{
   return modified;
}

//------------------------

PC_table __declspec(dllexport) TABAPI CreateTable(){

   return new C_table;
}

//------------------------
