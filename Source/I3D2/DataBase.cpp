/*--------------------------------------------------------
   Copyright (c) 1999, 2000 Michal Bacik. 
   All rights reserved.

   File: Database.cpp
   Content: Caching database system.
--------------------------------------------------------*/

#include "all.h"
#include "database.h"

//----------------------------

//#define WRITE_IMMEDIATELY     //don't do that, it's really slow

                              //profiling:
//#define PROF_BEG BegProf()
//#define PROF_END MessageBox(NULL, C_fstr("%.2f", EndProf()), "Dabatase profiling:", MB_OK);

//----------------------------

#define HEADER_ID 0x34424449  //"IDB4"

//----------------------------

static const char version_info[] = "--- Insanity 3D database version 1.4 ---";

//----------------------------

struct S_directory_entry{
   dword first_block;
   dword size;
   __int64 last_access_time_; //time when the information was last accessed (read or written)
   __int64 file_time_;        //time of stored information
};

typedef map<C_str, S_directory_entry> t_directory;

#define directory (*((t_directory*)_directory))

//----------------------------
//----------------------------

C_database::C_database():
   file_handle(-1),
   in_directory_write(false),
   directory_dirty(false),
   _directory(new t_directory),
   alloc_map_dirty(false)
{
}

//----------------------------

C_database::~C_database(){

   delete (t_directory*)_directory;
}

//----------------------------

static bool WinCreateDirectoryTree(const char *dst){
   
   SECURITY_ATTRIBUTES sa;
   memset(&sa, 0, sizeof(sa));
   sa.nLength = sizeof(sa);
   dword cpos, npos = 0;
   C_str str = dst;
   while(true){
      cpos = npos;
      while(str[++npos]) if(str[npos]=='\\') break;
      if(cpos){
         str[cpos] = 0;
         bool st = CreateDirectory(str, &sa);
         if(!st)
            return false;
         str[cpos] = '\\';
      }
      if(!str[npos])
         break;
   }
   return true;
}

//----------------------------

bool C_database::Open(const char *fname, dword max_size, int max_keep_days){

   Close();

                              //open file
   file_handle = (int)CreateFile(fname, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL,
      OPEN_ALWAYS, FILE_FLAG_RANDOM_ACCESS, NULL);

   if(file_handle==(int)INVALID_HANDLE_VALUE){
      file_handle = -1;
                              //try to create specified directory
      bool b = WinCreateDirectoryTree(fname);
      if(!b)
         return false;
                              //try to open the file again
      file_handle = (int)CreateFile(fname, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL,
         OPEN_ALWAYS, FILE_FLAG_RANDOM_ACCESS, NULL);

      if(file_handle==(int)INVALID_HANDLE_VALUE){
                              //open definitely failed
         file_handle = -1;
         return false;
      }
   }
   curr_file_ptr = 0;

                              //check if the file is of desired size
   dword size = GetFileSize((HANDLE)file_handle, NULL);
   if(size==max_size){
      dword rb;

      S_db_header hdr;
      ReadFile((HANDLE)file_handle, &hdr, sizeof(hdr), &rb, NULL);
      curr_file_ptr += rb;
      if(rb==sizeof(hdr) && hdr.ID==HEADER_ID){
         assert(hdr.dir_block < hdr.num_blocks);
                              //read alloc map
         //alloc_map.assign(hdr.num_blocks, 0);
         alloc_map.clear();
         alloc_map.resize(hdr.num_blocks, 0);
         SetFilePointer((HANDLE)file_handle, curr_file_ptr = 1*256, NULL, FILE_BEGIN);
         ReadFile((HANDLE)file_handle, &alloc_map[0], hdr.num_blocks*sizeof(dword), &rb, NULL);
         assert(rb == hdr.num_blocks*sizeof(dword));
         curr_file_ptr += rb;

         dir_block = hdr.dir_block;
         dir_size = hdr.dir_size;

         if(ReadDirectory(max_keep_days))
            return true;
      }
   }

                              //create new file
   {
      dword wb;
                              //set file size and initialize internal structure
      SetFilePointer((HANDLE)file_handle, curr_file_ptr = max_size, NULL, FILE_BEGIN);
      SetEndOfFile((HANDLE)file_handle);
      dword size = GetFileSize((HANDLE)file_handle, NULL);
      if(size!=max_size){
                              //cannot create file of desired size, erase and quit
         CloseHandle((HANDLE)file_handle);
         file_handle = -1;
         DeleteFile(fname);
         return false;
      }

                              //create and write header
      S_db_header hdr;
      hdr.ID = HEADER_ID;
                              //determine number of blocks
      hdr.num_blocks = max_size / 256;

      hdr.dir_block = 0;
      hdr.dir_size = 0;
      dir_block = 0;
      dir_size = 0;

                              //write header
      SetFilePointer((HANDLE)file_handle, curr_file_ptr = 0, NULL, FILE_BEGIN);
      WriteFile((HANDLE)file_handle, &hdr, sizeof(hdr), &wb, NULL);
      curr_file_ptr += wb;
      WriteFile((HANDLE)file_handle, &version_info, sizeof(version_info), &wb, NULL);
      curr_file_ptr += wb;

                              //by default all are free
      //alloc_map = vector<dword>(hdr.num_blocks, 0);
      alloc_map.clear();
      alloc_map.resize(hdr.num_blocks, 0);

                              //mark system blocks
      dword alloc_map_blocks = hdr.num_blocks * 4 / 256;
      alloc_map[0] = 1;
      for(dword i=0; i<alloc_map_blocks; i++){
         alloc_map[1+i] = 1;
      }
      alloc_map_dirty = true;
   }

   bool b;
   b = ReadDirectory(max_keep_days);
   assert(b);

   return true;
}

//----------------------------

bool C_database::Close(){
   
   if(file_handle==-1)
      return false;

                              //close all opened handles
   while(open_handles.size())
      CloseRecord(&open_handles[0]->cache);

   Flush();

   CloseHandle((HANDLE)file_handle);
   file_handle = -1;
   directory.clear();
   alloc_map.clear();
   open_handles.clear();

   return true;
}

//----------------------------

void C_database::Flush(){

#ifndef WRITE_IMMEDIATELY
   WriteDirectory();
   WriteAllocMap();
#endif
}

//----------------------------

inline bool IsLeapYear(int year){

   return (((year % 4) == 0) &&
      (((year % 100) != 0) ||
      ((year % 400) == 0)));
}

//----------------------------

static int GetMonthDays(int month, int year){

   static const int monthdays[] = {
      31, 28, 31, 30, 31, 30,
      31, 31, 30, 31, 30, 31
   };
   int ndays = monthdays[month - 1];
   if(month == 2 && IsLeapYear(year))
      ndays++;
   return ndays;
}

//----------------------------

static void TimeAddDays(SYSTEMTIME &st, int days){

   if(days < 0){
      assert(days >= -31);
      st.wDayOfWeek = word(st.wDayOfWeek + (days%7) + 7);
      st.wDayOfWeek %= 7;

      st.wDay = word(st.wDay + days);
      while((short)st.wDay <= 0){
         if(!--st.wMonth){
            st.wMonth = 12;
            --st.wYear;
         }
         st.wDay = word(st.wDay + GetMonthDays(st.wMonth, st.wYear));
      }
   }else{
      assert(0);
   }
}

//----------------------------

bool C_database::ReadDirectory(int max_keep_days){

   bool rtn = true;
   directory.clear();
   if(dir_block){
      FILETIME check_time;
      if(max_keep_days){
         SYSTEMTIME st;
         GetSystemTime(&st);
         TimeAddDays(st, -max_keep_days);
         SystemTimeToFileTime(&st, &check_time);
      }

                              //read into contiguous memory
      byte *mem = new byte[dir_size];
      if(!ReadData(dir_block, mem, dir_size)){
         rtn = false;
      }else{
                              //scan what's in
         dword mem_size = dir_size;
         byte *curr_mem = mem;
         C_str name;
         while(mem_size){
            dword name_size = *((byte*)curr_mem)++;
            --mem_size;
            if(!name_size)
               name_size = 256;
            name.Assign((const char*)curr_mem, name_size);
            
            curr_mem += name.Size() + 1;
            mem_size -= name.Size() + 1;

            const S_directory_entry *de = (S_directory_entry*)curr_mem;
                              //erase old directory entries
            if(max_keep_days){
               if(CompareFileTime(&check_time, (FILETIME*)&de->last_access_time_) > 0){
                  //cout<<"fail: " <<(const char*)name <<endl;
                  FreeBlocks(de->first_block);
                  directory_dirty = true;
                  de = NULL;
               }
            }
            if(de)
               directory[name] = *de;
            curr_mem += sizeof(S_directory_entry);
            mem_size -= sizeof(S_directory_entry);
            if((int&)mem_size < 0){ //corrupted, not loading
               rtn = false;
               break;
            }
         }
      }
      delete[] mem;
   }
   if(!rtn){
      directory.clear();
   }
   return rtn;
}

//----------------------------

bool C_database::WriteDirectory(){

   if(in_directory_write){
      return false;
   }
   if(!directory_dirty)
      return true;

   in_directory_write = true;
   if(dir_block){
      FreeBlocks(dir_block);
      dir_block = 0;
   }
   if(directory.size()){
      while(true){
         directory_dirty = false; 
                              //put into contiguous memory
         C_cache c;
         byte *mem = NULL;
         dword mem_size = 0;
         c.open(&mem, &mem_size, CACHE_WRITE_MEM,
            directory.size() * (sizeof(S_directory_entry) + 192));
         t_directory::const_iterator it;
         for(it=directory.begin(); it!=directory.end(); ++it){
                              //write NULL-terminated name
            dword size = (*it).first.Size();
            assert(size <= 256);
            c.write((const char*)&size, sizeof(byte));
            c.write((const char*)(*it).first, size + 1);
            c.write((const char*)&(*it).second, sizeof(S_directory_entry));
         }
         c.close();

         dir_size = mem_size;
         bool ok;
         ok = WriteData(mem, mem_size, &dir_block);
         assert(ok);
         c.FreeMem(mem);
         mem = NULL;

                              //write dir_block into header
         dword wb;
         if(curr_file_ptr!=offsetof(S_db_header, dir_block)){
            SetFilePointer((HANDLE)file_handle, curr_file_ptr = offsetof(S_db_header, dir_block), NULL, FILE_BEGIN);
         }
         WriteFile((HANDLE)file_handle, &dir_block, sizeof(dword), &wb, NULL);
         curr_file_ptr += wb;
         WriteFile((HANDLE)file_handle, &dir_size, sizeof(dword), &wb, NULL);
         curr_file_ptr += wb;

         if(!directory_dirty)
            break;
                              //dir changed during write, do it again
         FreeBlocks(dir_block);
         dir_block = 0;
      }
   }
   in_directory_write = false;

   return true;
}

//----------------------------

bool C_database::WriteAllocMap(){

   if(!alloc_map_dirty)
      return true;

   alloc_map_dirty = false;
   dword wb;
   if(curr_file_ptr!=1*256){
      SetFilePointer((HANDLE)file_handle, curr_file_ptr = 1*256, NULL, FILE_BEGIN);
   }
   WriteFile((HANDLE)file_handle, &alloc_map[0], alloc_map.size()*sizeof(dword), &wb, NULL);
   curr_file_ptr += wb;

   return true;
}

//----------------------------

bool C_database::ReadData(dword block, void *mem, dword mem_size){

                              //read data into memory
   while(mem_size){
      dword cont_block = block;
      dword cont_size = 0;
      while(true){
         cont_size = Min(cont_size+256, mem_size);
                              //cehck errors
         assert(cont_block < alloc_map.size());
         if(cont_block >= alloc_map.size())
            return false;

         dword n_block = alloc_map[cont_block] >> 8;

                              //check errors
         assert(n_block < alloc_map.size());
         if(n_block >= alloc_map.size())
            return false;

         if(!n_block || n_block!=cont_block+1){
            if(cont_size){
                              //check errors
               assert(cont_size <= mem_size);
               if(cont_size > mem_size)
                  return false;

               ReadSingleBlock(block, mem, cont_size);
               (byte*&)mem += cont_size;
               mem_size -= cont_size;
            }
            block = n_block;
            break;
         }
         cont_block = n_block;
      }
   }
   assert(block==0);
   if(block != 0)
      return false;

   return true;
}

//----------------------------

bool C_database::WriteData(const void *mem, dword mem_size, dword *beg_block1){

   dword beg_block = 0;
                              //write data onto disk (in 256b blocks)
   const byte *curr_mem = (const byte*)mem;
   int prev_block = 0;
   while(mem_size){
      dword block_size = Min(256ul, mem_size);
      prev_block = WriteSingleBlock(prev_block, curr_mem, block_size);
                              //save 1st block (pointer to beginning of data)
      if(!beg_block)
         beg_block = prev_block;
      curr_mem += block_size;
      mem_size -= block_size;
   }

   *beg_block1 = beg_block;
   return true;
}

//----------------------------

C_cache *C_database::CreateRecord(const C_str &name, const __int64 file_time_, dword init_size){

   if(!IsOpen())
      return NULL;

   EraseRecord(name, true);
   open_handles.push_back(new S_record_handle);
   S_record_handle *handle = open_handles.back();
   handle->write = true;
   handle->name = name;
   //memcpy(handle->file_time, file_time, sizeof(dword)*2);
   handle->file_time_ = file_time_;
                              //init cache
   handle->cache.open(&handle->mem, &handle->mem_size, CACHE_WRITE_MEM, init_size);

   return &handle->cache;
}

//----------------------------

C_cache *C_database::OpenRecord(const C_str &name, const __int64 file_time_){

   assert(name.Size() <= 256);
   if(!IsOpen())
      return NULL;

   t_directory::const_iterator it;
   it = directory.find(name);
   if(it==directory.end())
      return false;

   //if(file_time)
   {
                              //check if entry is the same as specified time
      const __int64 &ft1 = (*it).second.file_time_;
      //if((file_time[0] != ft1[0]) || (file_time[1] != ft1[1]))
      if(file_time_ != ft1)
         return false;
   }
   
   open_handles.push_back(new S_record_handle);
   S_record_handle *handle = open_handles.back();
   handle->write = false;
   handle->name = name;
                              //read data
   dword size = (*it).second.size;
   handle->alloc_mem = new byte[size];

   bool ok = ReadData((*it).second.first_block, handle->alloc_mem, size);
   if(!ok){
                              //failed to read data, free all and return an error
      delete handle;
      open_handles.pop_back();

      EraseRecord(name, true);

      return NULL;
   }
                              //init cache
   handle->cache.open(&handle->alloc_mem, &size, CACHE_READ_MEM);

                              //set new access time for the file
   {
      GetSystemTimeAsFileTime((FILETIME*)&((*it).second.last_access_time_));
      directory_dirty = true;
   }

   return &handle->cache;
}

//----------------------------

bool C_database::EraseRecord(const C_str &name){

   if(!IsOpen()) return NULL;

   return EraseRecord(name, true);
}

//----------------------------

bool C_database::EraseRecord(const C_str &name, bool write_back){
   
   t_directory::iterator it;
   it = directory.find(name);
   if(it==directory.end())
      return false;

   FreeBlocks((*it).second.first_block);
   directory.erase(it);
   directory_dirty = true;

#ifdef WRITE_IMMEDIATELY
   if(write_back){
      WriteDirectory();
      WriteAllocMap();
   }
#endif
   return true;
}

//----------------------------

bool C_database::CloseRecord(C_cache *c){

   S_record_handle *handle = (S_record_handle*)(((byte*)c) - offsetof(S_record_handle, cache));
   for(int i=open_handles.size(); i--; )
   if(open_handles[i]==handle){
                              //write data onto disk (in 256b blocks)
      handle->cache.close();

      bool write = handle->write;
      if(write){
         dword first_block;
         WriteData(handle->mem, handle->mem_size, &first_block);
         handle->cache.FreeMem(handle->mem);
                              //put entry into directory
         CreateDirectoryEntry(handle->name, first_block, handle->mem_size, handle->file_time_);
      }

      delete handle;
      open_handles[i] = open_handles.back(); open_handles.pop_back();

#ifdef WRITE_IMMEDIATELY
      if(write){
                              //keep file up-to-date
         WriteDirectory();
         WriteAllocMap();
      }
#endif
      return true;
   }
   return false;
}

//----------------------------

bool C_database::CreateDirectoryEntry(const C_str &name, dword first_block, dword size,
   const __int64 file_time_){

   S_directory_entry de = { first_block, size};
   //memcpy(de.file_time, file_time, sizeof(dword)*2);
   de.file_time_ = file_time_;
   {                          //store current time
      SYSTEMTIME st;
      GetSystemTime(&st);
      SystemTimeToFileTime(&st, (FILETIME*)&de.last_access_time_);
   }
   directory[name] = de;
   directory_dirty = true;
   return true;
}

//----------------------------

dword C_database::AllocSingleBlock(dword prev_block){

                              //find free block
   for(dword i=prev_block+1; i<alloc_map.size(); i++)
   if(!alloc_map[i]){
                              //link previous block to this
      if(prev_block){
         assert(alloc_map[prev_block] == 1);
         alloc_map[prev_block] = 1 | (i<<8);
      }
      alloc_map[i] = 1;
      alloc_map_dirty = true;
      return i;
   }
   for(i=0; i<prev_block; i++)
   if(!alloc_map[i]){
                              //link previous block to this
      if(prev_block){
         assert(alloc_map[prev_block] == 1);
         alloc_map[prev_block] = 1 | (i<<8);
      }
      alloc_map[i] = 1;
      alloc_map_dirty = true;
      return i;
   }

                              //find oldest entry in directory and free
   __int64 time = -1;
   t_directory::const_iterator it, for_erase = directory.end();
   for(it=directory.begin(); it!=directory.end(); ++it){
      if(CompareFileTime((FILETIME*)&time, (FILETIME*)&(*it).second.last_access_time_) == 1){
         //memcpy(time, (*it).second.last_access_time, sizeof(time));
         time = (*it).second.last_access_time_;
         for_erase = it;
      }
   }
   if(for_erase!=directory.end()){
      EraseRecord((*for_erase).first);
      return AllocSingleBlock(prev_block);
   }
   //todo("out of free blocks");
   assert(0);
   return 0;
}

//----------------------------

bool C_database::FreeBlocks(dword block){

   while(block){
      if(block >= alloc_map.size())
         return false;
      dword next_block = alloc_map[block]>>8;
      alloc_map[block] = 0;
      block = next_block;
   }
   alloc_map_dirty = true;
   return true;
}

//----------------------------

dword C_database::ReadSingleBlock(dword block, void *mem, dword block_size){

   if(curr_file_ptr!=block*256)
      SetFilePointer((HANDLE)file_handle, curr_file_ptr = block*256, NULL, FILE_BEGIN);
   dword rb;
   ReadFile((HANDLE)file_handle, mem, block_size, &rb, NULL);
   curr_file_ptr += rb;
                              //return next block
   dword next_block = alloc_map[block]>>8;
   return next_block;
}

//----------------------------

dword C_database::WriteSingleBlock(dword prev_block, const void *mem, dword block_size){

   assert(block_size<=256);
                              //find new free block
   dword new_block = AllocSingleBlock(prev_block);
   if(curr_file_ptr!=new_block*256)
      SetFilePointer((HANDLE)file_handle, curr_file_ptr = new_block*256, NULL, FILE_BEGIN);
   dword wb;
   WriteFile((HANDLE)file_handle, mem, block_size, &wb, NULL);
   curr_file_ptr += wb;

   return new_block;
}

//----------------------------

void C_database::GetStats(dword &bytes_total, dword &bytes_used){

   if(!IsOpen()){
      bytes_total = bytes_used = 0;
   }else{
      bytes_total = alloc_map.size() * 256 - dir_size;
      bytes_used = 0;
      for(int i=alloc_map.size(); i--; ){
         if(alloc_map[i])
            ++bytes_used;
      }
      bytes_used *= 256;
      bytes_used -= dir_size;
   }
}

//----------------------------
