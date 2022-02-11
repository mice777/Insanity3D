/*******************************************************
* Copyright (c) Lonely Cat Games  All rights reserved.
* File: c_cache.h
* Purpose: cached read/write using file or memory
* Created by: Michal Bacik
*******************************************************/
#ifndef __C_CACHE_H
#define __C_CACHE_H

#include <dta_read.h>
#include <memory.h>
#include <c_except.h>

enum E_CACHE_MODE{
   CACHE_NO,
   CACHE_READ,
   CACHE_WRITE,
   CACHE_READ_MEM,
   CACHE_WRITE_MEM,
};

#pragma pack(push,4)

class C_cache{
   enum{DEFAULT_CACHE_SIZE = 0x8000 };

                              //cache implementation
                              //virtual base
   class C_cache_base{
   protected:
      dword buffer_size;
      dword curr_pos;         //real file pos

      virtual byte *AllocMem(dword sz){
         return new byte[sz];
      }
      virtual byte *ReallocMem(void *bp, dword old_size, dword new_size){
         byte *new_mem = new byte[new_size];
         memcpy(new_mem, bp, Min(new_size, old_size));
         delete[] bp;
         return new_mem;
      }
      virtual void FreeMem(byte *bp){
         delete[] bp;
      }
      C_except ReadErr(){
         return C_except("C_cache: cannot read from cache");
      }
      C_except WriteErr(){
         return C_except("C_cache: cannot write to cache");
      }
      C_except EofErr(){
         return C_except("C_cache: unexpected end-of-file encountered");
      }
   public:
      C_cache_base(dword bs):
         buffer_size(bs),
         curr_pos(0)
      {}
      virtual ~C_cache_base(){}
      virtual E_CACHE_MODE GetMode() const = 0;
      virtual bool Open(const char *fname, dword buffer_size){ return false; }
      virtual bool Open(byte **mem, dword *size, dword init_size){ return false; }
      virtual dword Read(void *mem, dword len){ throw ReadErr(); }
      virtual bool Write(const void *mem, dword len){ throw WriteErr(); }
      virtual bool Flush(){ return false; }
      virtual byte ReadByte(){ throw ReadErr(); }
      virtual word ReadWord(){ throw ReadErr(); }
      virtual dword ReadDword(){ throw ReadErr(); }
      virtual bool IsEof() const = 0;
      virtual dword GetFileSize() const = 0;
      virtual dword GetCurrPos() const = 0;
      virtual void SetPos(dword pos) = 0;
      virtual PC_dta_stream GetStream() = 0;
   };
                              //read cache
   class C_cache_file: public C_cache_base{
   protected:
      PC_dta_stream dta;
      byte *base, *top, *curr;

      C_cache_file(dword bs):
         C_cache_base(bs),
         base(NULL),
         dta(NULL)
      {
      }
      ~C_cache_file(){
         if(dta) dta->Release();
         FreeMem(base);
      }
      void InitMem(dword buffer_size){
         curr = top = base = AllocMem(buffer_size);
      }
   public:
      virtual dword GetFileSize() const{
         return dta->GetSize();
      }
      virtual PC_dta_stream GetStream(){ return dta; }
   };

   class C_cache_read: public C_cache_file{
   public:
      C_cache_read(dword bs):
         C_cache_file(bs)
      {}

      virtual E_CACHE_MODE GetMode() const{ return CACHE_READ; }
      virtual bool Open(const char *fname, dword buffer_size){
         dta = DtaCreateStream(fname);
         if(!dta)
            return false;
         InitMem(buffer_size);
         return true;
      }
      virtual dword Read(void *mem, dword len){
         dword read_len = 0;
         const dword wanted_len = len;
         dword rl = 0;
         while(curr+len>top){
            dword rl1 = top-curr;
            memcpy(mem, curr, rl1);
            len -= rl1;
            mem = (byte*)mem + rl1;
            dword sz = dta->Read(curr=base, buffer_size);
            top = base + sz;
            curr_pos += sz;
            rl += rl1;
            if(!sz){
               read_len = rl;
               goto check_read_size;
               //return rl;
            }
         }
         memcpy(mem, curr, len);
         curr += len;
         read_len = rl + len;
         //return rl + len;
check_read_size:
         if(read_len != wanted_len)
            throw EofErr();
         return read_len;
      }
      virtual byte ReadByte(){
         if(curr==top){
            dword sz = dta->Read(curr=base, buffer_size);
            if(sz < sizeof(byte))
               throw EofErr();
            top = base + sz;
            curr_pos += sz;
         }
         return *curr++;
      }
      virtual word ReadWord(){
         if(curr+sizeof(word) > top){
            if(curr!=top){
               byte buf[2];
               dword rl = top-curr;
               memcpy(buf, curr, rl);
               dword sz = dta->Read(curr=base, buffer_size);
               if(sz < sizeof(word)-rl)
                  throw EofErr();
               top = base + sz;
               curr_pos += sz;
               dword rl1 = sizeof(word)-rl;
               memcpy(buf+rl, curr, rl1);
               curr += rl1;
               return *(word*)buf;
            }
            dword sz = dta->Read(curr=base, buffer_size);
            if(sz < sizeof(word))
               throw EofErr();
            top = base + sz;
            curr_pos += sz;
         }
         return *((word*&)curr)++;
      }
      virtual dword ReadDword(){
         if(curr+sizeof(dword) > top){
            if(curr!=top){
               byte buf[4];
               dword rl = top-curr;
               memcpy(buf, curr, rl);
               dword sz = dta->Read(curr=base, buffer_size);
               if(sz < sizeof(dword)-rl)
                  throw EofErr();
               top = base + sz;
               curr_pos += sz;
               dword rl1 = sizeof(dword)-rl;
               memcpy(buf+rl, curr, rl1);
               curr += rl1;
               return *(dword*)buf;
            }
            dword sz = dta->Read(curr=base, buffer_size);
            if(sz < sizeof(dword))
               throw EofErr();
            top = base + sz;
            curr_pos += sz;
         }
         return *((dword*&)curr)++;
      }
      virtual bool IsEof() const{
         return (curr==top && GetFileSize()==GetCurrPos());
      }
      virtual dword GetCurrPos() const{ return curr_pos+curr-top; }
      virtual void SetPos(dword pos){
         if(pos<GetCurrPos()){
            if(pos >= curr_pos-(top-base)){
               curr = top-(curr_pos-pos);
            }else{
               curr = top = base;
               dta->Seek(pos, DTA_SEEK_SET);
               curr_pos = pos;
            }
         }else
         if(pos > curr_pos){
            curr = top = base;
            dta->Seek(pos, DTA_SEEK_SET);
            curr_pos = pos;
         }else
            curr = top + pos - curr_pos;
      }
   };
                              //write cache
   class C_cache_write: public C_cache_file{
   public:
      C_cache_write(dword bs):
         C_cache_file(bs)
      {}
      ~C_cache_write(){
         if(dta)
            Flush();
      }
      virtual E_CACHE_MODE GetMode() const{ return CACHE_WRITE; }
      virtual bool Open(const char *fname, dword buffer_size){
         dta = DtaCreateStream(fname, true);
         if(!dta)
            return false;
         InitMem(buffer_size);
         return true;
      }
      virtual bool Write(const void *mem, dword len){
                              //put into cache
         while((curr-base)+len > buffer_size){
            dword sz = buffer_size - (curr-base);
            memcpy(curr, mem, sz);
            curr += sz;
            top = curr;
            mem = (byte*)mem + sz;
            len -= sz;
            Flush();
         }
         memcpy(curr, mem, len);
         curr += len;
         if(top<curr)
            top = curr;
         return true;
      }
      virtual bool Flush(){
         if(top>base){
            dword written = dta->Write(base, top-base);
            if(written != dword(top-base)){
                              //avoid flushing when closed
               curr_pos += top-base;
               curr = top = base;
               throw C_except("C_cache: write failed");
            }
         }
         curr_pos += top-base;
         curr = top = base;
         return true;
      }
      virtual bool IsEof() const{ return true; }
      virtual dword GetCurrPos() const{ return dta->Seek(0, DTA_SEEK_CUR) + curr - base; }
      virtual void SetPos(dword pos){
         int delta = pos - curr_pos - (curr-base);
         if(delta<0){
            if(curr-base >= -delta)
               curr += delta;
            else{
               Flush();
               dta->Seek(curr_pos = pos, DTA_SEEK_SET);
            }
         }else{
            if(((dword)delta) <= (buffer_size-(curr-base))){
                              //fill by zeros
               int delta1 = pos - curr_pos - (top-base);
               if(delta1>0)
                  memset(top, 0, delta1);
                              //read original contents (if any)
               int file_len = dta->Seek(0, DTA_SEEK_END);
               if(file_len > curr_pos+top-base){
                           //read-in the rest
                  dta->Seek(curr_pos+top-base, DTA_SEEK_SET);
                  dta->Read(curr, pos - curr_pos-(top-base));
               }
               dta->Seek(curr_pos, DTA_SEEK_SET);
               curr += delta;
               if(top<curr)
                  top = curr;
            }else{
               Flush();
               dta->Seek(curr_pos = pos, DTA_SEEK_SET);
            }
         }
      }
   };

   class C_cache_mem: public C_cache_base{
   protected:
      byte *base;
      C_cache_mem(dword sz):
         C_cache_base(sz)
      {}
   public:
      virtual dword GetCurrPos() const{ return curr_pos; }
      virtual PC_dta_stream GetStream(){ throw C_except("C_cache: memory-based I/O has no stream"); }
   };
                              //read mem cache
   class C_cache_read_mem: public C_cache_mem{
      dword mem_size;
   public:
      C_cache_read_mem(dword bs):
         C_cache_mem(bs),
         mem_size(0)
      {}
      virtual E_CACHE_MODE GetMode() const{ return CACHE_READ_MEM; }
      virtual bool Open(byte **mem, dword *size, dword init_size){
         base = *mem;
         mem_size = *size;
         return true;
      }
      virtual dword Read(void *mem, dword len){
         dword rl = Min(len, mem_size-curr_pos);
         memcpy(mem, base+curr_pos, rl);
         curr_pos += rl;
         return rl;
      }
      virtual byte ReadByte(){
         if(mem_size-curr_pos < sizeof(byte))
            throw EofErr();
         return base[curr_pos++];
      }
      virtual word ReadWord(){
         if(mem_size-curr_pos < sizeof(word))
            throw EofErr();
         curr_pos += sizeof(word);
         return *(word*)(base+curr_pos-sizeof(word));
      }
      virtual dword ReadDword(){
         if(mem_size-curr_pos < sizeof(dword))
            throw EofErr();
         curr_pos += sizeof(dword);
         return *(dword*)(base+curr_pos-sizeof(dword));
      }
      virtual bool IsEof() const{
         return (curr_pos==mem_size);
      }
      virtual void SetPos(dword pos){ curr_pos = Max(0ul, Min(mem_size, pos)); }
      virtual dword GetFileSize() const{ return mem_size; }
   };
                              //write mem cache
   class C_cache_write_mem: public C_cache_mem{
      byte **memp;
      dword *sizep;
      dword curr_mem_size;
   public:
      C_cache_write_mem(dword bs):
         C_cache_mem(bs),
         curr_mem_size(0)
      {}
      ~C_cache_write_mem(){
                              //store pointers
                              // note: user is responsible for freeing memory
         *memp = base;
         *sizep = curr_pos;
      }
      virtual E_CACHE_MODE GetMode() const{ return CACHE_WRITE_MEM; }
      virtual bool Open(byte **mem, dword *size, dword init_size){
         sizep = size;
         memp = mem;
         base = NULL;
         if(init_size)
            base = AllocMem(curr_mem_size = init_size);
         return true;
      }
      virtual bool Write(const void *mem, dword len){
                              //check if we need re-allocate
         if(len + curr_pos > curr_mem_size)
            base = ReallocMem(base, curr_pos, curr_mem_size = Max((len + curr_pos)*2, buffer_size));
         memcpy(base+curr_pos, mem, len);
         curr_pos += len;
         return true;
      }
      virtual bool IsEof() const{ return true; }
      virtual void SetPos(dword pos){ curr_pos = Max(0ul, Min(curr_mem_size, pos)); }
      virtual dword GetFileSize() const{ return curr_pos; }
   };
                              //pointer holding implementation class, or NULL
   C_cache_base *imp;

   dword buffer_size;

public:
                              //virtual functions provide that this class may be used freely
                              //between DLLs, as only on module manages allocation
   virtual byte *AllocMem(dword sz){
      return new byte[sz];
   }
   virtual void FreeMem(byte *bp){
      delete[] bp;
   }
   C_cache():
      buffer_size(DEFAULT_CACHE_SIZE),
      imp(NULL)
   {}
   C_cache(dword sz):
      buffer_size(sz),
      imp(NULL)
   {}
   ~C_cache(){
      close();
   }

//----------------------------

   bool open(const char *fname, E_CACHE_MODE m){

      close();
      switch(m){
      case CACHE_READ:
         imp = new C_cache_read(buffer_size);
         break;
      case CACHE_WRITE:
         imp = new C_cache_write(buffer_size);
         break;
      default:
         throw C_except(C_fstr("Invalid cache mode: %i (file '%s')", m, fname));
      }
      if(!imp->Open(fname, buffer_size)){
         close();
         return false;
      }
      return true;
   }

//----------------------------
// Open cache for read from / write to memory. Memory is managed by virtual functions
// of C_cache.
// In case of writing to memory, user is responsible for freeing memory after closing cache
// by calling C_cache::FreeMem method.
// When opening cache for write to memory, parameter default_size specifies initial size of
// buffer being allocated.
//----------------------------
   bool open(byte **mem, dword *size1, E_CACHE_MODE m, dword default_size = 0){

      close();
      switch(m){
      case CACHE_READ_MEM:
         imp = new C_cache_read_mem(buffer_size);
         break;
      case CACHE_WRITE_MEM:
         imp = new C_cache_write_mem(buffer_size);
         break;
      default: 
         throw C_except(C_fstr("Invalid cache mode: %i", m));
      }
      if(!imp->Open(mem, size1, default_size)){
         close();
         return false;
      }
      return true;
   }

//----------------------------

   void close(){
      delete imp;
      imp = NULL;
   }

   inline E_CACHE_MODE GetMode() const{ return !imp ? CACHE_NO : imp->GetMode(); }

//----------------------------
// Read data from the cache. Return number of bytes read.
   dword read(void *mem, dword len){

      if(!imp)
         throw C_except("C_cache::read: not initialized");
      return imp->Read(mem, len);
   }

//----------------------------
// Read data - versions reading basic types.
   byte ReadByte(){
      if(!imp)
         throw C_except("C_cache::ReadByte: not initialized");
      return imp->ReadByte();
   }

   word ReadWord(){
      if(!imp)
         throw C_except("C_cache::ReadWord: not initialized");
      return imp->ReadWord();
   }

   dword ReadDword(){
      if(!imp)
         throw C_except("C_cache::ReadDword: not initialized");
      return imp->ReadDword();
   }

//----------------------------

   bool write(const void *mem, dword len){

      if(!imp)
         throw C_except("C_cache::write: not initialized");
      return imp->Write(mem, len);
   }

//----------------------------
// Write data - versions writing basic types.
   void WriteByte(byte b){
      if(!imp)
         throw C_except("C_cache::WriteByte: not initialized");
      imp->Write(&b, sizeof(byte));
   }

   void WriteWord(word w){
      if(!imp)
         throw C_except("C_cache::WriteWord: not initialized");
      imp->Write(&w, sizeof(word));
   }

   void WriteDword(dword d){
      if(!imp)
         throw C_except("C_cache::WriteDword: not initialized");
      imp->Write(&d, sizeof(dword));
   }

//----------------------------

   C_cache &getline(char *buf, int len, char='\n'){

      if(!imp)
         throw C_except("C_cache::getline: not initialized");
      int num_chars = imp->GetFileSize() - imp->GetCurrPos();
      len = Min(len-1, num_chars);
      while(len--){
         char c = imp->ReadByte();
         if(c=='\r'){
            imp->ReadByte();
            break;
         }
         if(c=='\n' || c==0)
            break;
         *buf++ = c;
      }
      *buf = 0;
      return *this;
   }

   C_cache &flush(){
      if(!imp)
         throw C_except("C_cache::flush: not initialized");
      imp->Flush();
      return *this;
   }

   bool eof() const{
      if(!imp)
         throw C_except("C_cache::eof: not initialized");
      return imp->IsEof();
   }

   dword filesize() const{
      if(!imp)
         throw C_except("C_cache::filesize: not initialized");
      return imp->GetFileSize();
   }

   dword tellg() const{
      if(!imp)
         throw C_except("C_cache::tellg: not initialized");
      return imp->GetCurrPos();
   }

   //inline bool fail() const{ return b_fail; }

   //inline bool bad() const{ return fail(); }

   //inline bool good() const{ return !fail(); }

   bool seekg(dword pos){
      if(!imp)
         throw C_except("C_cache::seekg: not initialized");
      imp->SetPos(pos);
      return true;
   }

   inline bool seekp(dword pos){ return seekg(pos); }

   //inline int GetHandle() const{ return h; }
   PC_dta_stream GetStream(){
      if(!imp)
         throw C_except("C_cache::GetStream: not initialized");
      return imp->GetStream();
   }
};

#pragma pack(pop)

//----------------------------

#endif   //__C_CACHE_H
