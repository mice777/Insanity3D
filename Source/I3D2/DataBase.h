#ifndef __DATABASE_H
#define __DATABASE_H

//----------------------------

class C_database{
   struct S_db_header{
      dword ID;
      dword num_blocks;
      dword dir_block;
      dword dir_size;
   };
   struct S_record_handle{
      C_cache cache;          //cache returned to user
      byte *mem;
      dword mem_size;
      byte *alloc_mem;
      C_str name;
      bool write;
      __int64 file_time_;      //valid only when 'write' == true

      S_record_handle():
         alloc_mem(NULL)
      {}
      S_record_handle(const S_record_handle&);
      S_record_handle &operator =(const S_record_handle&);
      ~S_record_handle(){
         delete[] alloc_mem;
      }
   };

   int file_handle;           //-1 = not opened
   dword curr_file_ptr;
   C_buffer<dword> alloc_map;
   C_vector<S_record_handle*> open_handles;
   dword dir_block;           //first block of directory
   dword dir_size;            //in bytes, as saved on disk
   void *_directory;          //hidden implementation of directory
   bool alloc_map_dirty;
   bool directory_dirty;
   bool in_directory_write;

   dword AllocSingleBlock(dword prev_block);
   bool FreeBlocks(dword first_block);
   dword WriteSingleBlock(dword prev_block, const void *mem, dword block_size);

//----------------------------
// Read single block of data. The returned value is an index of next block.
   dword ReadSingleBlock(dword block, void *mem, dword block_size);

   bool WriteAllocMap();
   bool CreateDirectoryEntry(const C_str &name, dword first_block, dword size, const __int64 file_time);
   bool ReadDirectory(int max_keep_days);
   bool WriteDirectory();
   bool EraseRecord(const C_str&, bool write_back);

//----------------------------
// Read 'mem_size' bytes beginning on specified 'block' into 'mem'.
   bool ReadData(dword block, void *mem, dword mem_size);
//----------------------------
// Write 'mem_size' bytes beginning on 'mem' into file, return beg block in 'beg_block'.
   bool WriteData(const void *mem, dword mem_size, dword *beg_block);
public:
   C_database();
   ~C_database();

                              //open or create dbase file
   bool Open(const char *fname, dword max_size, int max_keep_days);
                              //close dbase
   bool Close();
   inline bool IsOpen() const{ return (file_handle!=-1); }
                              //flush in-memory data on disk
   void Flush();

                              //open record in dbase
   C_cache *OpenRecord(const C_str&, const __int64 file_time);
                              //create new record (overwrite if exists)
   C_cache *CreateRecord(const C_str&, const __int64 file_time, dword init_size = 0);
                              //erase existing record
   bool EraseRecord(const C_str&);
                              //close opened record, flush data to disk
   bool CloseRecord(C_cache*);

   void GetStats(dword &bytes_total, dword &bytes_used);
};

//----------------------------

#endif//__DATABASE_H