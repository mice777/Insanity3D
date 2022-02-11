#define WIN32_LEAN_AND_MEAN
#define NOMINMAX          //Macros min(a,b) and max(a,b)
#include <windows.h>
#include <dta_read.h>
#include <smartptr.h>
#pragma warning(push,3)
#include <vector>
#include <map>
#pragma warning(pop)
#include <C_str.hpp>
#include <zlib.h>

using namespace std;

/*--------------------------------------------------------
   Copyright (c) 2002 Lonely Cat Games,  All rights reserved.
   Data reader - read-only access to data packages.
   Contents:
      - data initalizer class
      - I/O functions simulating standard open, close, read and seek

   Access:
      28. 3. 1999    Michal Bacik
      - re-designed to support openinng the same file multiple times - there was a bug
         that each file kept only one current position regardles of number of open handles
         on them
      13. 9. 2000    Michal Bacik
      - support for file time, info read from DTA file.

--------------------------------------------------------*/

#include <rules.h>
#include <c_unknwn.hpp>

#define COMPCNT_VERSION '6'

//#define DTACREATE_SHOWINFO 1  //show dta file scanning during content creation
//#define DTACREATE_NO_CNT 2    //do not use cached contents file


typedef class C_dta_read *LPC_dta_read;

#define DTA_SEEK_SET 0
#define DTA_SEEK_CUR 1
#define DTA_SEEK_END 2

//----------------------------

class C_dta_handle_base: public C_unknown{
public:
   virtual bool GetTime(dword time[2]) const = 0;
   virtual dword Read(void *buf, dword sz) = 0;
   virtual dword Write(const void *buf, dword sz){ return (dword)-1; }
   virtual dword Seek(int pos, dword whence) = 0;
   virtual PC_dta_read GetDataFile(){ return NULL; }
};

typedef C_dta_handle_base *PC_dta_handle_base;

//----------------------------

class C_dta_handle_win32: public C_dta_handle_base{
   HANDLE h;                  //valid if file opened by Windows file system
public:
   C_dta_handle_win32(HANDLE h1):
      h(h1)
   {}
   ~C_dta_handle_win32(){
      bool ok;
      ok = CloseHandle(h);
      assert(ok);
   }

   virtual bool GetTime(dword time[2]) const{
      return GetFileTime(h, NULL, NULL, (LPFILETIME)time);
   }
   virtual dword Read(void *buf, dword sz){
      dword ret;
      if(!ReadFile(h, buf, sz, &ret, 0))
         ret = (dword)-1;
      return ret;
   }
   virtual dword Write(const void *buf, dword sz){
      dword ret;
      if(!WriteFile(h, buf, sz, &ret, 0))
         ret = (dword)-1;
      return ret;
   }
   virtual dword Seek(int pos, dword whence){
      return SetFilePointer(h, pos, NULL, whence);
   }
};

//----------------------------

                              //statics

                              //list of open dta
                              //filled front-to-back
                              //entries are searched for file back-to-front, so
                              //later opened, earlier searched
static vector<class C_dta_read_base*> *dta_list;

//----------------------------

class C_dta_read_base: public C_dta_read{
protected:
   dword ref;
public:
   C_dta_read_base():
      ref(1)
   {}
   ~C_dta_read_base(){
                                 //remove from static list
      if(dta_list)
      for(int i=dta_list->size(); i--; ){
         if((*dta_list)[i]==this){
            dta_list->erase(dta_list->begin()+i);
            if(!dta_list->size()){
               delete dta_list;
               dta_list = NULL;
            }
            break;
         }
      }
   }

   virtual dword AddRef(){ return ++ref; }
   virtual dword Release() = 0;

   virtual PC_dta_handle_base OpenHandle(const C_str &fname) = 0;
};

//----------------------------
//----------------------------

class C_dta_read_zip: public C_dta_read_base{

   HANDLE dta_handle;         //file handle opened by operating system

                              //lock used in calls to ZLib
   CRITICAL_SECTION lock;

#pragma pack(1)

   struct LocalFileHeader{
      dword signature;       // 0x04034b50 ("PK..")
      word versionNeeded;      // version needed to extract
      word flags;           //    
      word compression;     // compression method
      word lastModTime;     // last mod file time
      word lastModDate;     // last mod file date
      dword crc;          //
      dword compressedSize;     // 
      dword uncompressedSize;   // 
      word filenameLen;     // length of the filename field following this structure
      word extraFieldLen;      // length of the extra field following the filename field
   };

   struct DataDesc{
      dword signature;       // 0x08074b50
      dword crc;             //
      dword compressedSize;     //
      dword uncompressedSize;   //
   };

   struct DirFileHeader{
      dword signature;       // 0x02014b50
      word versionUsed;     //
      word versionNeeded;      //
      word flags;           //
      word compression;     // compression method
      word lastModTime;     //
      word lastModDate;     //
      dword crc;          //
      dword compressedSize;     //
      dword uncompressedSize;   //
      word filenameLen;     // length of the filename field following this structure
      word extraFieldLen;      // length of the extra field following the filename field
      word commentLen;         // length of the file comment field following the extra field
      word diskStart;       // the number of the disk on which this file begins
      word internal;        // internal file attributes
      dword external;        // external file attributes
      dword localOffset;     // relative offset of the local file header
   };

   struct DirEndRecord{
      dword signature;       // 0x06054b50
      word thisDisk;        // number of this disk
      word dirStartDisk;    // number of the disk containing the start of the central directory
      word numEntriesOnDisk;   // # of entries in the central directory on this disk
      word numEntriesTotal; // total # of entries in the central directory
      dword dirSize;         // size of the central directory
      dword dirStartOffset;     // offset of the start of central directory on the disk where the central directory begins
      word commentLen;         // zip file comment length
   };
#pragma pack()

public:
   struct S_file_entry{
      enum E_compression{
         C_STORE,
         C_DEFLATE,
      } compression;
      dword sz_compressed;    //compressed file size
      dword sz_uncompressed;  //uncompressed file size
      dword file_time;        //in MS-DOS format
      dword local_offset;     //offset info local header
      bool encrypted;
   };
private:

   typedef map<C_str, S_file_entry> t_file_map;
   t_file_map file_map;

   //C_buffer<char> password;

   friend class C_dta_handle_zip;
//----------------------------

   class C_dta_handle_zip: public C_dta_handle_base{
      S_file_entry *fe;
      C_smart_ptr<C_dta_read_zip> dta;
      dword dta_file_pos;

      byte *buf;

      dword Decompress(const byte *src, byte *dst, dword sz_src, dword sz_dst){

         int err;
                              //decompression stream
         z_stream d_stream;

         memset(&d_stream, 0, sizeof(d_stream) );

         d_stream.zalloc = (alloc_func)NULL;
         d_stream.zfree   = (free_func)NULL;
         d_stream.opaque = (voidpf)NULL;
   
         d_stream.next_in = (byte*)src;
         d_stream.avail_in = sz_src + 1;

         err = inflateInit2(&d_stream, -MAX_WBITS);
         if(err != Z_OK){
            assert(("ZIP file decompression (inflateInit2) failed.", 0));
            return 0;
         }
         while(true){
            d_stream.next_out = dst;
            d_stream.avail_out = sz_dst;

            err = inflate(&d_stream, Z_FINISH);
            if(err == Z_STREAM_END)
               break;
            if(err != Z_OK){
               switch(err){
               case Z_MEM_ERROR: assert(("ZIP file decompression failed (memory error).", 0));
               case Z_BUF_ERROR: assert(("ZIP file decompression failed (buffer error).", 0));
               case Z_DATA_ERROR: assert(("ZIP file decompression failed (data error).", 0));
               default: assert(("ZIP file decompression failed (unknown error).", 0));
               }
               return 0;
            }
         }
         err = inflateEnd(&d_stream);
         if(err != Z_OK){
            assert(("ZIP file decompression (inflateEnd) failed.", 0));
            return 0;
         }
         return d_stream.total_out;
      }
   public:
      C_dta_handle_zip(S_file_entry *fe1, C_dta_read_zip *d, HANDLE h):
         fe(fe1),
         dta(d),
         dta_file_pos(0)
      {
                              //temporary decompress file
         buf = new byte[fe->sz_uncompressed];

                              //read local header
         LocalFileHeader lhdr;
         SetFilePointer(h, fe->local_offset, NULL, FILE_BEGIN);
         dword rd;
         bool ok = ::ReadFile(h, &lhdr, sizeof(lhdr), &rd, NULL);
         assert(ok && rd==sizeof(lhdr));
         assert(fe->sz_compressed == lhdr.compressedSize);

         dword source_pos = fe->local_offset + sizeof(LocalFileHeader) + lhdr.filenameLen + lhdr.extraFieldLen;
         SetFilePointer(h, source_pos, NULL, FILE_BEGIN);
         if(fe->encrypted){
            assert(("Decrypt", 0));
         }
         switch(fe->compression){
         case S_file_entry::C_STORE:
            ok = ReadFile(h, buf, fe->sz_uncompressed, &rd, NULL);
            assert(ok && rd==fe->sz_uncompressed);
            break;
         case S_file_entry::C_DEFLATE:
            {
               byte *src = new byte[fe->sz_compressed+1];
               ok = ReadFile(h, src, fe->sz_compressed, &rd, NULL);
               assert(ok && rd==fe->sz_compressed);
               Decompress(src, buf, fe->sz_compressed, fe->sz_uncompressed);
               delete[] src;
            }
            break;
         default:
            assert(0);
         }
      }
      ~C_dta_handle_zip(){
         delete[] buf;
      }
      virtual bool GetTime(dword time[2]) const{
         return DosDateTimeToFileTime(word(fe->file_time>>16), word(fe->file_time&0xffff), (LPFILETIME)time);
      }
      virtual dword Read(void *dst, dword sz){
                              //validate read length
         sz = Min((int)sz, (int)fe->sz_uncompressed - (int)dta_file_pos);
         memcpy(dst, buf+dta_file_pos, sz);
         dta_file_pos += sz;
         return sz;
      }
      virtual dword Seek(int pos, dword whence){
         switch(whence){
         case DTA_SEEK_CUR:
            pos += dta_file_pos;
            break;
         case DTA_SEEK_END:
            pos += fe->sz_uncompressed;
            break;
         }
                              //clamp to area of file
         pos = Max(Min(pos, (int)fe->sz_uncompressed), 0);
         dta_file_pos = pos;
         return dta_file_pos;
      }
      virtual PC_dta_read GetDataFile(){ return dta; }
   };
//----------------------------
public:
   C_dta_read_zip()
   {
      InitializeCriticalSection(&lock);
   }
   ~C_dta_read_zip(){
      DeleteCriticalSection(&lock);
   }

   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

   bool Open(const char *name, const char *full_name, char drive, HANDLE h, dword flags){

      DirEndRecord er;
      er.numEntriesTotal = 0;
      er.dirStartOffset = 0;
      long sz = GetFileSize(h, NULL);
      int i;

      for(dword pos = sz - sizeof(DirEndRecord); pos>0; pos--){
         SetFilePointer(h, pos, NULL, FILE_BEGIN);
         dword sig = 0, rd;
         ::ReadFile(h, &sig, sizeof(sig), &rd, NULL);
         if(sig==0x06054b50){
                              //found. read the end record and return.
            SetFilePointer(h, pos, NULL, FILE_BEGIN);
            ::ReadFile(h, &er, sizeof(DirEndRecord), &rd, NULL);

                              //fail, if multi-volume zip
            if(er.thisDisk != 0 || er.dirStartDisk != 0 || er.numEntriesOnDisk != er.numEntriesTotal)
               goto fail;
            break;
         }
      }

      SetFilePointer(h, er.dirStartOffset, NULL, FILE_BEGIN);

      for(i=0; i<er.numEntriesTotal; i++){
         DirFileHeader header;
         dword rd;
         if(!::ReadFile(h, &header, sizeof(header), &rd, NULL) || rd!=sizeof(header))
            goto fail;
                              //skip empty files (directories)
         if(header.compressedSize){
            char *name = new char[header.filenameLen+1];
            if(!::ReadFile(h, name, header.filenameLen, &rd, NULL) || rd!=header.filenameLen){
               delete[] name;
               goto fail;
            }
            name[header.filenameLen] = 0;
            C_str filename = name;
            filename.ToLower();
            for(dword i=filename.Size(); i--; ){
               if(filename[i]=='/')
                  filename[i] = '\\';
            }

            bool ok = true;

            S_file_entry fe;
            fe.file_time = *(dword*)&header.lastModTime;
            fe.sz_compressed = header.compressedSize;
            fe.sz_uncompressed = header.uncompressedSize;
            fe.local_offset = header.localOffset;
            fe.encrypted = (header.flags&1);
            switch(header.compression){
            case 0:
               fe.compression = S_file_entry::C_STORE;
               break;
            case 8:
               fe.compression = S_file_entry::C_DEFLATE;
               break;
            default:
               ok = false;
            }
            if(ok)
               file_map.insert(pair<C_str, S_file_entry>(filename, fe));

            delete[] name;

            SetFilePointer(h, header.extraFieldLen + header.commentLen, NULL, FILE_CURRENT);
         }else{
            SetFilePointer(h, header.extraFieldLen + header.commentLen + header.filenameLen, NULL, FILE_CURRENT);
         }
      }
      dta_handle = h;
      if(!dta_list)
         dta_list = new vector<C_dta_read_base*>;
      dta_list->push_back(this);
      return true;

   fail:
      CloseHandle(h);
      return false;
   }

   virtual PC_dta_handle_base OpenHandle(const C_str &fname){

      t_file_map::iterator it = file_map.find(fname);
      if(it==file_map.end())
         return NULL;
      EnterCriticalSection(&lock);
      C_dta_handle_zip *h = new C_dta_handle_zip(&(*it).second, this, dta_handle);
      LeaveCriticalSection(&lock);
      return h;
   }
};

//----------------------------
//----------------------------
                              //create dta object - open dta file,
                              // if open fails, creation fails too and NULL is returned
LPC_dta_read dtaCreate(const char *name, dword flags){

   C_dta_read_base *dta = NULL;

   char temp_name[512];
   const char *full_name;
   char drive;

                              //check if full path in filename
   if(name[0] && name[1]==':' && name[2]=='\\'){
      drive = name[0];
      full_name = name;
   }else{
      GetCurrentDirectory(sizeof(temp_name), temp_name);
      drive = temp_name[0];
      full_name = name;
   }

                              //open file
   HANDLE h = CreateFile(full_name, GENERIC_READ, FILE_SHARE_READ, NULL,
      OPEN_EXISTING,
      //FILE_FLAG_SEQUENTIAL_SCAN,
      FILE_FLAG_RANDOM_ACCESS,
      NULL);
                              //open error?
   if(h == INVALID_HANDLE_VALUE)
      return false;

   dword format_id, rd;
   if(!::ReadFile(h, &format_id, sizeof(format_id), &rd, NULL) || rd!=sizeof(format_id)){
      CloseHandle(h);
      return false;
   }
   SetFilePointer(h, 0, NULL, FILE_BEGIN);

   /*
   switch(format_id){
   case '!raR':
      {
         C_dta_read_rar *dta_rar;
         dta = dta_rar = new C_dta_read_rar;
         if(!dta_rar->Open(name, full_name, drive, h, flags, info_title)){
            dta->Release();
            dta = NULL;
         }
      }
      break;
   default:
      */
      switch(format_id&0xffff){
      case 'KP':
         {
            C_dta_read_zip *dta_zip;
            dta = dta_zip = new C_dta_read_zip;
            if(!dta_zip->Open(name, full_name, drive, h, flags)){
               dta->Release();
               dta = NULL;
            }
         }
         break;
      }
      /*
      break;
   }
   */
   return dta;
}

//----------------------------
//----------------------------

class C_dta_stream_imp: public C_dta_stream{
   dword ref;
   C_smart_ptr<C_dta_handle_base> handle;
public:
   C_dta_stream_imp(PC_dta_handle_base h):
      ref(1),
      handle(h)
   {}

   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

//----------------------------
// Read from stream.
   virtual int Read(void *buffer, dword len){
      return handle->Read(buffer, len);
   }

//----------------------------
// Seek within the stream.
   virtual int Seek(int pos, int whence){
      return handle->Seek(pos, whence);
   }

//----------------------------
// Get size of stream.
   virtual int GetSize() const{
      int save = ((C_dta_handle_base*)(const C_dta_handle_base*)handle)->Seek(0, DTA_SEEK_CUR);
      int len = ((C_dta_handle_base*)(const C_dta_handle_base*)handle)->Seek(0, DTA_SEEK_END);
      ((C_dta_handle_base*)(const C_dta_handle_base*)handle)->Seek(save, DTA_SEEK_SET);
      return len;
   }

//----------------------------
// Get time of file.
   virtual bool GetTime(dword time[2]) const{
      return handle->GetTime(time);
   }

//----------------------------
// Write to the file (valid for writable stream).
   virtual int Write(const void *buffer, dword len){
      return handle->Write(buffer, len);
   }

//----------------------------
// Get DTA pack where opened file resides.
   virtual PC_dta_read GetDtaPack() const{
      return ((C_dta_handle_base*)(const C_dta_handle_base*)handle)->GetDataFile();
   }
};

//----------------------------
//----------------------------

static PC_dta_handle_base dtaOpen(const char *name){

   PC_dta_handle_base handle = NULL;

   C_str str(name);
   str.ToLower();
   if(dta_list){
      for(int i = dta_list->size(); i--; ){
                              //search all dta files
         handle = (*dta_list)[i]->OpenHandle(str);
         if(handle)
            break;
      }
   }
   if(!handle){
      HANDLE h = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
      if(h==INVALID_HANDLE_VALUE)
         return NULL;
      handle = new C_dta_handle_win32(h);
   }
   return handle;
}

//----------------------------

inline PC_dta_handle_base dtaOpenWrite(const char *name){

   HANDLE h = CreateFile(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
   if(h==INVALID_HANDLE_VALUE)
      return NULL;
   return new C_dta_handle_win32(h);
}

//----------------------------

PC_dta_stream DtaCreateStream(const char *filename, bool for_write){

   PC_dta_handle_base hb;
   if(!for_write)
      hb = dtaOpen(filename);
   else
      hb = dtaOpenWrite(filename);
   if(!hb)
      return NULL;
   PC_dta_stream str = new C_dta_stream_imp(hb);
   hb->Release();
   return str;
}

//----------------------------
//----------------------------
