#pragma warning(push,3)
#include <vector>
#pragma warning(pop)
#pragma warning(disable: 4127)
#include <C_str.hpp>
#include <i3d\i3d_math.h>     //due to S_vector
#include <c_cache.h>
#ifdef _DEBUG
#include <exception>
#endif

#ifdef _MSC_VER
using namespace std;
#endif

//----------------------------
//----------------------------

void FatalError(const char *msg);
#define THROW(what) FatalError(what)


//----------------------------

typedef byte ENUM_TYPE;

//----------------------------

                              //table template definition
class C_table_template{
public:
   char *caption;
   CPC_table_element te;

   //const void *GetGUID() const{ return guid; }
   const C_table_element *GetElements() const{ return te; }
   const char *GetCaption() const{ return caption; }
   bool IsDataType(dword index) const{
      switch(te[index].type){
      case TE_NULL: case TE_BRANCH: case TE_ARRAY: return false;
      default: return true;
      }
   };
   dword GetItemTableIndex(dword index) const{
      return te[index].tab_index;
   }
   dword ArraySize(dword index) const{ return te[index].array_size; }
   dword BranchDepth(dword index) const{ return te[index].branch_depth; }
   dword NumItems(bool count_all) const{
      int i=0, num=0;
      while(te[i].type!=TE_NULL){
         if(count_all || IsDataType(i)) ++num;
         ++i;
      }
      return num;
   };
   E_TABLE_ELEMENT_TYPE GetItemType(dword index) const{ return te[index].type; }
   dword SizeOf(dword index) const{
      static const byte sizes[TE_LAST]={
         0, sizeof(bool), sizeof(int), sizeof(float),
         sizeof(ENUM_TYPE), sizeof(char), sizeof(byte)*3, sizeof(float)*3
      };
      switch(te[index].type){
      case TE_ARRAY: return te[index].array_size * SizeOf(index+1);
      case TE_STRING: return te[index].string_size;
      case TE_BRANCH: return 0;
      case TE_NULL: return 0;
      default: return sizes[te[index].type];
      }
   };
   float GetDefaultVal(dword index) const{
      return te[index].flt_default;
   }
   void *operator new(size_t sz, int i){
      return new byte[sz + sizeof(C_table_element)*(i-1)];
   }
};

//----------------------------

class C_table_base{
public:
   TABMETHOD_(dword,AddRef)() = 0;
   TABMETHOD_(dword,Release)() = 0;

   TABMETHOD_(bool,Load)(const void *src, dword flags) = 0;
   TABMETHOD_(bool,Save)(const void *dst, dword flags) const = 0;
   TABMETHOD_(bool,IsLoaded)() const = 0;

   TABMETHOD_(dword,NumItems)() const = 0;
   TABMETHOD_(dword,ArrayLen)(dword index) const = 0;
   TABMETHOD_(dword,StringSize)(dword index) const = 0;
   TABMETHOD_(dword,SizeOf)(dword index) const = 0;

   TABMETHOD_(void*,Item)(dword index) = 0;
                              //get various items
   TABMETHOD_(bool&,ItemB)(dword index) = 0;
   TABMETHOD_(int&,ItemI)(dword index) = 0;
   TABMETHOD_(float&,ItemF)(dword index) = 0;
   TABMETHOD_(byte&,ItemE)(dword index) = 0;
   TABMETHOD_(char*,ItemS)(dword index) = 0;
   TABMETHOD_(struct S_vector&,ItemV)(dword index) = 0;

   TABMETHOD_(bool,GetItemB)(dword index) const = 0;
   TABMETHOD_(int,GetItemI)(dword index) const = 0;
   TABMETHOD_(float,GetItemF)(dword index) const = 0;
   TABMETHOD_(byte,GetItemE)(dword index) const = 0;
   TABMETHOD_(const char*,GetItemS)(dword index) const = 0;
   TABMETHOD_(const struct S_vector&,GetItemV)(dword index) const = 0;

                              //get various items from arrays
   TABMETHOD_(bool&,ItemB)(dword index, dword ai) = 0;
   TABMETHOD_(int&,ItemI)(dword index, dword ai) = 0;
   TABMETHOD_(float&,ItemF)(dword index, dword ai) = 0;
   TABMETHOD_(byte&,ItemE)(dword index, dword ai) = 0;
   TABMETHOD_(char*,ItemS)(dword index, dword ai) = 0;
   TABMETHOD_(struct S_vector&,ItemV)(dword index, dword ai) = 0;

   TABMETHOD_(bool,GetItemB)(dword index, dword ai) const = 0;
   TABMETHOD_(int,GetItemI)(dword index, dword ai) const = 0;
   TABMETHOD_(float,GetItemF)(dword index, dword ai) const = 0;
   TABMETHOD_(byte,GetItemE)(dword index, dword ai) const = 0;
   TABMETHOD_(const char*,GetItemS)(dword index, dword ai) const = 0;
   TABMETHOD_(const struct S_vector&,GetItemV)(dword index, dword ai) const = 0;


   TABMETHOD_(E_TABLE_ELEMENT_TYPE,GetItemType)(dword index) const = 0;
   //TABMETHOD_(const void*,GetGUID)() const = 0;

                              //table editor - returns HWND
   TABMETHOD_(void*,Edit)(const class C_table_template*, void *hwnd_parent,
      table_callback cb_proc, dword cb_user, dword create_flags = 0,
      int *pos_size = NULL) = 0;

   TABMETHOD_(bool,IsModified)() const = 0;
};

//----------------------------

class C_table: public C_table_base{
                              //C_unk:
   dword ref;

   bool lock;
   mutable bool modified;
   dword num_items;
   byte guid[16];
#pragma pack(push,1)
   struct S_desc_item{
      dword offset;
      byte type;              //real type: E_TABLE_ELEMENT_TYPE
      union{
         byte max_string_size;   //for TE_STRING
      };
      word array_len;
      S_desc_item():
         offset((dword)-1),
         type(TE_NULL),
         max_string_size(0),
         array_len(0)
      {}
   } *desc;
#pragma pack(pop)
   byte *data;                //pointer to table data
   dword data_size;
   static const char *type_names[TE_LAST];
                              //save header
#define HDRFLAGS_COMPRESS     1
#define HDRFLAGS_MODIFY_DATA  2     //format of modify data: null-terminated string, word year, month, day, hour, minute
   struct S_file_header{
      dword ID;
      bool lock;
      dword num_items, data_size;
      dword flags;
      dword reserved;
   };

   friend PC_table __declspec(dllexport) TABAPI CreateTable();

   C_table():
      ref(1),
      lock(false),
      modified(false),
      num_items(0),
      desc(NULL),
      data(NULL)
   {
      memset(&guid, 0, sizeof(guid));
   }
   ~C_table(){
      Close();
   }

   void Close();
   int InsertItem(PC_table_template tt, dword templ_index, int &storage,
      vector<byte> &v_defaults, dword array_len=0);

public:
   TABMETHOD_(dword,AddRef)(){ return ++ref; }
   TABMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   TABMETHOD_(bool,Load)(const void *src, dword flags);
   TABMETHOD_(bool,Save)(const void *dst, dword flags) const;
   TABMETHOD_(bool,IsLoaded)() const{
      return (desc && data);
   }

   TABMETHOD_(dword,NumItems)() const{ return num_items; }
   TABMETHOD_(dword,ArrayLen)(dword index) const{
      return !desc ? 0xffffffff : index>=num_items ? 0xffffffff :
         desc[index].array_len;
   }
   TABMETHOD_(dword,StringSize)(dword index) const{
      return !desc ? 0xffffffff : index>=num_items ? 0xffffffff :
         desc[index].type==TE_STRING ? desc[index].max_string_size : 0xffffffff;
   }
   TABMETHOD_(dword,SizeOf)(dword index) const{
      if(!desc || index>=num_items) return 0xffffffff;
      static const byte sizes[TE_LAST]={
         0, sizeof(bool), sizeof(int), sizeof(float),
         sizeof(ENUM_TYPE), sizeof(char), sizeof(byte)*3, sizeof(float)*3
      };
      E_TABLE_ELEMENT_TYPE type = (E_TABLE_ELEMENT_TYPE)desc[index].type;
      return type==TE_STRING ? desc[index].max_string_size : sizes[type];
   }

#pragma optimize("y", off)
   inline void *GetItem(dword index, E_TABLE_ELEMENT_TYPE assert_type){
      assert(desc && data);
      if(!desc || !data){
         THROW("C_table::Item(dword): reading from uninitialized table");
         return NULL;
      }
      assert(index>=0 && index<num_items);
      if(index>=num_items){
         THROW(C_fstr("C_table::Item(dword): reading out of range (index = %i, num_items = %i)", index, num_items));
         return NULL;
      }
      int offset = desc[index].offset;
      if(offset==-1){
         THROW(C_fstr("C_table::Item(dword): reading from uninitialized index (%i)", index));
         return NULL;
      }
      if(assert_type && desc[index].type!=assert_type){
         THROW(C_fstr("C_table::Item(dword %i): wrong type %s, expecting %s", index,
            type_names[desc[index].type], type_names[assert_type]));
         return NULL;
      }
      return data + offset;
   }
   const void *GetItem(dword index, E_TABLE_ELEMENT_TYPE assert_type) const{
      return (const void*)const_cast<C_table*>(this)->GetItem(index, assert_type);
   }

   TABMETHOD_(void*,Item)(dword index){ return C_table::GetItem(index, TE_NULL); }
                              //get various items
   bool  &TABAPI ItemB(dword i){ return *(bool*)C_table::GetItem(i, TE_BOOL); }
   int   &TABAPI ItemI(dword i){ return *(int*)C_table::GetItem(i, TE_INT); }
   float &TABAPI ItemF(dword i){ return *(float*)C_table::GetItem(i, TE_FLOAT); }
   byte  &TABAPI ItemE(dword i){ return *(byte*)C_table::GetItem(i, TE_ENUM); }
   char  *TABAPI ItemS(dword i){ return (char*)C_table::GetItem(i, TE_STRING); }
   S_vector &TABAPI ItemV(dword i){ return *(S_vector*)C_table::GetItem(i, TE_COLOR_VECTOR); }

   bool  TABAPI GetItemB(dword i) const{ return *(bool*)C_table::GetItem(i, TE_BOOL); }
   int   TABAPI GetItemI(dword i) const{ return *(int*)C_table::GetItem(i, TE_INT); }
   float TABAPI GetItemF(dword i) const{ return *(float*)C_table::GetItem(i, TE_FLOAT); }
   byte  TABAPI GetItemE(dword i) const{ return *(byte*)C_table::GetItem(i, TE_ENUM); }
   const char *TABAPI GetItemS(dword i) const{ return (char*)C_table::GetItem(i, TE_STRING); }
   const S_vector &TABAPI GetItemV(dword i) const{ return *(S_vector*)C_table::GetItem(i, TE_COLOR_VECTOR); }

                           //get various items from arrays
   bool &TABAPI ItemB(dword i, dword ai){
      void *vp = GetItem(i, TE_BOOL);
      if(!vp || (ai && ai>=desc[i].array_len)){
         THROW(C_fstr("C_table::ItemB(dword, dword): reading out of range (index = %i, array_len = %i)", ai, desc[i].array_len));
         return *(bool*)NULL;
      }
      return ((bool*)vp)[ai];
   }
   int &TABAPI ItemI(dword i, dword ai){
      void *vp = GetItem(i, TE_INT);
      if(!vp || (ai && ai>=desc[i].array_len)){
         THROW(C_fstr("C_table::ItemI(dword, dword): reading out of range (index = %i, array_len = %i)", ai, desc[i].array_len));
         return *(int*)NULL;
      }
      return ((int*)vp)[ai];
   }
   float &TABAPI ItemF(dword i, dword ai){
      void *vp = GetItem(i, TE_FLOAT);
      if(!vp || (ai && ai>=desc[i].array_len)){
         THROW(C_fstr("C_table::ItemF(dword, dword): reading out of range (index = %i, array_len = %i)", ai, desc[i].array_len));
         return *(float*)NULL;
      }
      return ((float*)vp)[ai];
   }
   byte &TABAPI ItemE(dword i, dword ai){
      void *vp = GetItem(i, TE_ENUM);
      if(!vp || (ai && ai>=desc[i].array_len)){
         THROW(C_fstr("C_table::ItemE(dword, dword): reading out of range (index = %i, array_len = %i)", ai, desc[i].array_len));
         return *(byte*)NULL;
      }
      return ((byte*)vp)[ai];
   }
   char *TABAPI ItemS(dword i, dword ai){
      void *vp = GetItem(i, TE_STRING);
      if(!vp || (ai && ai>=desc[i].array_len)){
         THROW(C_fstr("C_table::ItemS(dword, dword, dword): reading out of range (index = %i, array_len = %i)", ai, desc[i].array_len));
         return *(char**)NULL;
      }
      //assert(desc[i].max_string_size==str_size);
      return ((char*)vp)+ai*desc[i].max_string_size;
   }
   S_vector &TABAPI ItemV(dword i, dword ai){
      void *vp = GetItem(i, TE_COLOR_VECTOR);
      if(!vp || (ai && ai>=desc[i].array_len)){
         THROW(C_fstr("C_table::ItemV(dword, dword): reading out of range (index = %i, array_len = %i)", ai, desc[i].array_len));
         return *(S_vector*)NULL;
      }
      return ((S_vector*)vp)[ai];
   }

   TABMETHOD_(bool,  GetItemB)(dword index, dword ai) const{ return const_cast<PC_table>(this)->ItemB(index, ai); }
   TABMETHOD_(int,   GetItemI)(dword index, dword ai) const{ return const_cast<PC_table>(this)->ItemI(index, ai); }
   TABMETHOD_(float, GetItemF)(dword index, dword ai) const{ return const_cast<PC_table>(this)->ItemF(index, ai); }
   TABMETHOD_(byte,  GetItemE)(dword index, dword ai) const{ return const_cast<PC_table>(this)->ItemE(index, ai); }
   TABMETHOD_(const char*,GetItemS)(dword index, dword ai) const{ return const_cast<PC_table>(this)->ItemS(index, ai); }
   TABMETHOD_(const struct S_vector&,GetItemV)(dword index, dword ai) const{ return const_cast<PC_table>(this)->ItemV(index, ai); }
#pragma optimize("", on)

   TABMETHOD_(E_TABLE_ELEMENT_TYPE,GetItemType)(dword index) const{
      return !IsLoaded() ? TE_NULL : index>=num_items ? TE_NULL : (E_TABLE_ELEMENT_TYPE)desc[index].type;
   }
   //TABMETHOD_(const void*,GetGUID)() const{ return guid; }

#ifdef TABBLER_EDIT_SUPPORT
                              //table editor - returns HWND
   TABMETHOD_(void*,Edit)(const class C_table_template*, void *hwnd_parent,
      table_callback cb_proc, dword cb_user, dword create_flags = 0,
      int *pos_size = NULL);

#else
   TABMETHOD_(void*,Edit)(const class C_table_template*, void*,
      table_callback, dword, dword, const char*, int*){
      return NULL;
   }

   TABMETHOD_(void*,Edit)(C_table*, void*, table_callback, dword, dword,
      const char *, int*){
      return NULL;
   }
#endif

   TABMETHOD_(bool,IsModified)() const;

//----------------------------

                              //history - just skip data in file (no more saved)
   void ReadModifyData(C_cache *ch, const S_file_header &hdr){

      if(hdr.flags&HDRFLAGS_MODIFY_DATA){
         char buf[256];
         char c;
         int i = 0;
         do{
            ch->read(&c, 1);
            buf[i++] = c;
         }while(c);

         struct{
            word wYear, wMonth, wDay, wHour, wMinute;
         } modify_time;
         ch->read((byte*)&modify_time, sizeof(modify_time));
      }
   }

   inline void SetModify(){ modified = true; }
};

//----------------------------
//----------------------------
