#ifndef __C_CHUNK_H_
#define __C_CHUNK_H_
/*//----------------------------
Copyright (c) Lonely Cat Games  All rights reserved.

The C_chunk class us used for writing and reading chunk-based files.
It performs its I/O operations using C_cache class.
The class may be either uninitialized, or in read or write mode, at a time.

Many functions are available for reading and writing from/to most used types.
Additionaly, the class utilizes following operators for convenient access:

operator bool - determine if current size (using method Size()) is not zero - i.e. if there're more data in curr chunk
operator ++ (RAscend) - ascend into chunk for reading, and return the chunk type as CK_TYPE
operator <<=(CK_TYPE) (WAscend) - ascend into chunk for writing
operator -- (Descend) - descend from current chunk - works in both read and write modes
operator ()(CK_TYPE, T &data) (W???Chunk) - overloaded bunch of operator(), equivalent to W???Chunk, but automatically
   recognizing the type of second operand, and saving proper size of data
operator >>(T&) (R???Chunk) - overloaded bunch of operator>>, equivalent to R???Chunk, but automatically
   recognizing the type of the operand, and reading proper size of data

Exceptions:
The class throws C_except exception when any fault is encountered:
   - reading beyond the chunk size
   - ascending in wrong I/O mode
Additionally, C_cache class throws its own exceptions when error is encountered

//----------------------------*/

#include <rules.h>
#include <c_str.hpp>
#include <C_buffer.h>
#include <i3d\i3d_math.h>
#include <c_cache.h>
#include <malloc.h>

using namespace std;

                              //chunk identificator type
typedef word CK_TYPE;

//----------------------------

class C_chunk{
                              //simple stack class for storing file positions for ascended chunks
   class C_stack: public C_buffer<dword>{
      dword num_items;
   public:
      C_stack():
         num_items(0)
      {}
      void push_back(dword dw){
         dword curr_cap = C_buffer<dword>::size();
         if(curr_cap==num_items){
            dword alloc_len = Max(32ul, curr_cap*2);
            resize(alloc_len);
         }
         operator [](num_items) = dw;
         ++num_items;
      }
      void pop_back(){
         assert(num_items);
         --num_items;
      }

   //----------------------------
   // overriden functions of C_buffer
      dword size() const{
         return num_items;
      }

      dword back() const{ assert(num_items); return operator [](num_items-1); }
   } level;

   C_cache cache;
   bool out, init;
   CK_TYPE last_ck_t;         //last ascended chunk type (in both read and write modes)
#pragma pack(push, 1)
   struct S_ckhdr{
      word type;
      dword size;
   };
#pragma pack(pop)

   bool OpenFile(const char *fname, bool write_access){
      if(cache.open(fname, !write_access ? CACHE_READ : CACHE_WRITE)){
         out = write_access;
         return (init = true);
      }
      return false;
   }
   bool OpenMem(byte **mem, dword *size, bool write_access){
      if(cache.open(mem, (dword*)size, !write_access ? CACHE_READ_MEM : CACHE_WRITE_MEM)){
         out = write_access;
         return (init = true);
      }
      return false;
   }
   inline dword GetBackLevel() const{
      if(!level.size())
         throw C_except("C_chunk error: not ascended in chunk");
      return level.back();
   }
   inline void AssertIn() const{
      if(out)
         throw C_except("C_chunk: not in read mode");
   }
   inline void AssertOut() const{
      if(!out)
         throw C_except("C_chunk: not in write mode");
   }
   C_except EofErr(){
      return C_except("C_chunk::Read: eof encountered");
   }

//----------------------------
// Assert chunk has at least 'sz' number of data in it left from current position.
// Throw eof exception if not true.
   void AssertChunkSize(dword sz){
      dword bl_left = level.size() ? Max(0, int(GetBackLevel() - cache.tellg())) : (cache.filesize() - cache.tellg());
      if(bl_left < sz)
         throw EofErr();
   }

public:
   C_chunk():
      init(false),
      last_ck_t(0)
   {}

//----------------------------
// Init class with specified cache size
   C_chunk(dword sz):
      cache(sz),
      init(false)
   {}

   ~C_chunk(){ Close(); }
                              //read
//----------------------------
// Open disk file for reading.
   bool ROpen(const char *fname){
      Close();
      return OpenFile(fname, false);
   }

//----------------------------
// Open memory location for reading.
   bool ROpen(const byte *mem, dword size){
      Close();
      return OpenMem((byte**)&mem, &size, false);
   }

//----------------------------
// Enter chunk in read mode and retrieve chunk type.
// The returned value is false if chunk can't be entered (end of file, or end of chunk).
   CK_TYPE RAscend(){

      AssertIn();
      S_ckhdr chkdr;
      if(level.size() && Size() < sizeof(S_ckhdr))
         throw EofErr();
      dword rl = cache.read((char*)&chkdr, sizeof(S_ckhdr));
      if(rl < sizeof(S_ckhdr))
         throw EofErr();
      level.push_back(chkdr.size+cache.tellg() - sizeof(S_ckhdr));
      return (last_ck_t = chkdr.type);
   }
   CK_TYPE operator ++(){
      return RAscend();
   }

//----------------------------
// Read data.
   inline void Read(void *data, dword sz){

      AssertIn();
      if(level.size()){    //don't read data beyond the block size
         dword bl_left = Max(0, int(level.back() - cache.tellg()));
         if(sz > bl_left)
            throw EofErr();
      }
      if(cache.read((char*)data, sz) != sz)
         throw EofErr();
   }

//----------------------------
   inline S_vector ReadVector(){ S_vector v; Read(&v, sizeof(S_vector)); return v; }

   inline S_vectorw ReadVectorw(){ S_vectorw v; Read(&v, sizeof(S_vectorw)); return v; }
   
   inline S_quat ReadQuaternion(){ S_quat q; Read(&q, sizeof(q)); return q; }

   inline dword ReadDword(){ AssertChunkSize(sizeof(dword)); return cache.ReadDword(); }
   inline int ReadInt(){ return ReadDword(); }
   inline float ReadFloat(){ dword dw = ReadDword(); return *(float*)&dw; }

   inline word ReadWord(){ AssertChunkSize(sizeof(word)); return cache.ReadWord(); }

   inline byte ReadByte(){ AssertChunkSize(sizeof(byte)); return cache.ReadByte(); }

   inline bool ReadBool(){ return ReadByte(); }

//----------------------------
// Get size left from current location to the end of current chunk.
   inline dword Size() const{ return GetBackLevel()-cache.tellg(); }
   operator bool() const{ return (Size() != 0); }

//----------------------------
// Open disk file for writting.
   inline bool WOpen(const char *fname){ return OpenFile(fname, true); }

//----------------------------
// Open memory location for writting.
   inline bool WOpen(byte **mem, dword *size){ return OpenMem(mem, size, true); }

//----------------------------
// Create chunk of specified type in write mode.
   void WAscend(CK_TYPE ckt){
      AssertOut();
      S_ckhdr chkdr;
      chkdr.type = ckt;
      level.push_back(cache.tellg());
                              //write partial header, write rest later
      cache.write(&chkdr, sizeof(S_ckhdr));
      last_ck_t = ckt;
   }
   C_chunk &operator <<=(CK_TYPE ckt){
      WAscend(ckt);
      return *this;
   }

//----------------------------
// Write data.
   template<class T>
   void Write(const T &t){
      AssertOut();
      cache.write(&t, sizeof(T));
   }

//----------------------------
// Write raw string data.
   template<>
   void Write(const C_str &s){
      Write((const char*)s, s.Size()+1);
   }
   //template<>
   void Write(const char *cp){
      Write(cp, strlen(cp)+1);
   }
   inline void Write(const void *data, dword sz){
      AssertOut();
      cache.write(data, sz);
   }

//----------------------------
// Get pointer of C_cache we're working on.
   inline C_cache *GetHandle(){ return &cache; }

   void SetOpenModeByHandle(){
      init = false;
      switch(cache.GetMode()){
      case CACHE_READ: case CACHE_READ_MEM:
         out = false;
         init = true;
         break;
      case CACHE_WRITE: case CACHE_WRITE_MEM:
         out = true;
         init = true;
         break;
      default:
         throw C_except("C_chunk: invalid cache");
      }
   }
   
//----------------------------
// Descend currently entered chunk - works for both read and write modes.
   void Descend(){
      if(!out){
                              //skip to rest of chunk
         cache.seekp(GetBackLevel());
      }else{               
                              //write chunk header
         dword curr_pos = cache.tellg();
         dword chk_pos = GetBackLevel();
         dword chk_size = curr_pos - chk_pos;
         cache.seekp(chk_pos + offsetof(S_ckhdr, size));
         cache.write(&chk_size, sizeof(dword));
         cache.seekp(curr_pos);
      }
      level.pop_back();
   }
   void operator --(){ Descend(); }

//----------------------------
// Close class - descend all currently opened chunks (in write mode).
   void Close(){
      if(IsInit()){
         if(out){
            while(level.size())
               Descend();
         }
         cache.close();
         init = false;
      }
   }

//----------------------------

   CK_TYPE GetLastAscendChunk() const{ return last_ck_t; }

//----------------------------
// Check if class is initialized (opened).
   inline bool IsInit() const{ return init; }

//----------------------------
// Template function (operator() ) for writing any type in its chunk.
   template<class T>
   C_chunk &operator ()(CK_TYPE ct, const T &t){
      WAscend(ct);
      Write(&t, sizeof(T));
      Descend();
      return *this;
   }

//----------------------------
// Following clas of Write???Chunk functions create specified chunk and write associated data into that.
   void WStringChunk(CK_TYPE ct, const char *cp){
      WAscend(ct);
      Write(cp, strlen(cp)+1);
      Descend();
   }
   //template<>
   C_chunk &operator ()(CK_TYPE ct, const char *cp){ WStringChunk(ct, cp); return *this; }
   template<>
   C_chunk &operator ()(CK_TYPE ct, const C_str &s){
      WAscend(ct);
      Write((const char*)s, s.Size()+1);
      Descend();
      return *this;
   }

//----------------------------

   void WVectorChunk(CK_TYPE ct, const S_vector &v){
      WAscend(ct);
      Write(&v, sizeof(S_vector));
      Descend();
   }

//----------------------------

   void WVectorwChunk(CK_TYPE ct, const S_vectorw &v){
      WAscend(ct);
      Write(&v, sizeof(S_vectorw));
      Descend();
   }

//----------------------------

   void WRotChunk(CK_TYPE ct, const S_quat &q){
      WAscend(ct);
      Write(&q, sizeof(q));
      Descend();
   }

//----------------------------

   void WFloatChunk(CK_TYPE ct, float f){
      WAscend(ct);
      Write(&f, sizeof(float));
      Descend();
   }

//----------------------------

   void WIntChunk(CK_TYPE ct, int i){
      WAscend(ct);
      Write(&i, sizeof(int));
      Descend();
   }

//----------------------------

   void WWordChunk(CK_TYPE ct, word w){
      WAscend(ct);
      Write(&w, sizeof(w));
      Descend();
   }

//----------------------------

   void WByteChunk(CK_TYPE ct, byte b){
      WAscend(ct);
      Write(&b, sizeof(byte));
      Descend();
   }

//----------------------------

   void WBoolChunk(CK_TYPE ct, bool b){
      WAscend(ct);
      Write(&b, sizeof(byte));
      Descend();
   }

//----------------------------

   void WArrayChunk(CK_TYPE ct, const void *mem, dword len){
      WAscend(ct);
      Write(mem, len);
      Descend();
   }
   C_chunk &operator ()(CK_TYPE ct, const void *mem, dword len){ WArrayChunk(ct, mem, len); return *this; }

//----------------------------
// Following clas of Read???Chunk functions read specified data from already ascended chunk

   C_str ReadString(){

      char buf[256];
      for(int i=0; i<256; ){
         buf[i] = cache.ReadByte();
         if(!buf[i++])
            break;
      }
      buf[i-1] = 0;
      return buf;
   }

//----------------------------
// Further read method additionally descend out of the chunk after reading the data.

   bool ReadString(CK_TYPE ct, C_str &name){

      if(RAscend()==ct){
         char buf[256];
         Read(buf, Min((dword)sizeof(buf), Size()));
         name = buf;
         Descend();
         return true;
      }
      Descend();
      name = NULL;
      return false;
   }

//----------------------------
   template<class T>
   C_chunk &operator >>(T &t){
      Read(&t, sizeof(T));
      Descend();
      return *this;
   }


   C_str RStringChunk(){
                              //read entire chunk into buffer
      dword ck_size = Size();
      char *buf = (char*)alloca(ck_size);
      Read(buf, ck_size);
      Descend();
      return buf;
   }
   C_chunk &operator >>(C_str &s){ s = RStringChunk(); return *this; }

//----------------------------

   S_vector RVectorChunk(){
      S_vector v;
      Read(&v, sizeof(S_vector));
      Descend();
      return v;
   }

//----------------------------

   S_vectorw RVectorwChunk(){
      S_vectorw v;
      Read(&v, sizeof(S_vectorw));
      Descend();
      return v;
   }

//----------------------------
   
   S_quat RQuaternionChunk(){
      AssertChunkSize(sizeof(S_quat));
      S_quat q;
      Read(&q, sizeof(q));
      Descend();
      return q;
   }

//----------------------------

   inline float RFloatChunk(){
      AssertChunkSize(sizeof(float));
      dword dw = cache.ReadDword();
      Descend();
      return *(float*)&dw;
   }
   C_chunk &operator >>(float &f){
      f = RFloatChunk();
      return *this;
   }

//----------------------------

   inline int RIntChunk(){
      AssertChunkSize(sizeof(int));
      int i = cache.ReadDword();
      Descend();
      return i;
   }
   C_chunk &operator >>(int &i){
      i = RIntChunk();
      return *this;
   }
   C_chunk &operator >>(dword &dw){
      dw = RIntChunk();
      return *this;
   }

//----------------------------

   inline word RWordChunk(){
      AssertChunkSize(sizeof(word));
      word w = cache.ReadWord();
      Descend();
      return w;
   }
   C_chunk &operator >>(word &w){
      w = RWordChunk();
      return *this;
   }
   C_chunk &operator >>(short &s){
      s = RWordChunk();
      return *this;
   }

//----------------------------

   inline byte RByteChunk(){
      AssertChunkSize(sizeof(byte));
      byte b = cache.ReadByte();
      Descend();
      return b;
   }
   C_chunk &operator >>(byte &b){
      b = RByteChunk();
      return *this;
   }
   C_chunk &operator >>(char &c){
      c = RByteChunk();
      return *this;
   }

//----------------------------

   inline bool RBoolChunk(){
      AssertChunkSize(sizeof(byte));
      bool b = cache.ReadByte();
      Descend();
      return b;
   }
   C_chunk &operator >>(bool &b){
      b = RBoolChunk();
      return *this;
   }

//----------------------------

   void RArrayChunk(void *mem, dword len){
      Read(mem, len);
      Descend();
   }
};

//----------------------------
#endif
