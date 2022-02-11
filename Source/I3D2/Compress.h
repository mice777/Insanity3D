#ifndef __COMPRESS_H
#define __COMPRESS

#include "rules.h"

/*----------------------------------------------------------------\
Copyright (c) 2000 Michal Bacik  All rights reserved
Abstract:
 Compression template class - allows compression of any class, for which comparison and assignment operators are defined.

Functions:
   template<class T>
   bool Compress(T *in, dword in_size, vector<byte> &out);
         - compress data
         Parameters:
            in - pointer to data to compress
            in_size - number of elements to compress
            out - vector containing data after function returns
         Return value:
            true if compression successful

   template<class T>
   bool Decompress(void *in, T **out, dword *num_elements);
         - decompress data
         Parameters:
            in - pointer to data to de-compress
            out - pointer which will contain pointer to T containing
               decompressed data. Use delete[] out to free memory
            num_elements - pointer which will contain number of elements
               decomressed
         Return value:
            true if de-compression successful
|\----------------------------------------------------------------*/

namespace compression{

#pragma pack(push,1)
struct S_compress_header{
   dword id;
   dword orig_elements;
   dword data_len;
};
#pragma pack(pop)


#define COMPRESS_ID 0x31454c52

}

//----------------------------

template<class T>
bool Compress(const T *in, dword in_size, C_vector<byte> &v){

   v.reserve(sizeof(T)*in_size);
   for(int j=sizeof(compression::S_compress_header); j--; ) v.push_back(0);
   compression::S_compress_header &hdr = *(compression::S_compress_header*)&v.front();
   hdr.id = COMPRESS_ID;
   hdr.orig_elements = in_size;

   while(in_size){
                              //count same members
      T t1 = *in;
      for(dword i=1; i<in_size && i<128 && in[i]==t1; i++);
      if(i>1){
                              //store compressed
         v.push_back(byte(i+127));
         for(int j=sizeof(T); j--; )
            v.push_back(0);
         //*(T*)(v.end()-sizeof(T)) = t1;
         *(T*)((&v.front() + v.size()) - sizeof(T)) = t1;
      }else{
                              //find 2 same members
         for(i=2; i<in_size && i<129 && !(in[i]==in[i-1]); i++);
         --i;
         v.push_back(byte(i-1));
         for(dword k=0; k<i; k++){
            for(int j=sizeof(T); j--; )
               v.push_back(0);
            //*(T*)(v.end()-sizeof(T)) = in[k];
            *(T*)((&v.front() + v.size()) - sizeof(T)) = in[k];
         }
      }
      in += i;
      in_size -= i;
   }
   {
      compression::S_compress_header &hdr = *(compression::S_compress_header*)&v.front();
      hdr.data_len = v.size();;
   }
   return true;
}

//----------------------------

template<class T>
bool Decompress(void *in, T **out, dword *num_elements, bool header_only = false){

   *out = NULL;
   *num_elements = 0;
   assert(in);
   if(!in) return false;
   compression::S_compress_header &hdr = *(compression::S_compress_header*)in;
   if(hdr.id!=COMPRESS_ID) return false;
   dword num = hdr.orig_elements;
   *num_elements = hdr.orig_elements;
   if(header_only) return true;
   *out = new T[num];
   assert(*out);
   T *tp = *out;
   in = ((byte*)in)+sizeof(compression::S_compress_header);

   while(num){
      int i = *(byte*)in;
      in = ((byte*)in)+1;
      if(i&0x80){          //compressed
         T t1 = *(T*)in;
         in = ((T*)in)+1;
         i -= 127;
         assert(i<=num);      //we can't overflow
         if(i>num) goto fail;
         for(int j=i; j--; ) tp[j] = t1;
      }else{
         ++i;
         assert(i<=num);      //we can't overflow
         if(i>num) goto fail;
         memcpy(tp, in, i*sizeof(T));
         in = ((T*)in)+i;
      }
      tp += i;
      num -= i;
   }
   return true;
fail:                         //error!
   delete[] *out;
   *out = NULL;
   return false;
}

//----------------------------
#include <c_cache.h>

template<class T>
bool Decompress(C_cache *ck, T **out, dword *num_elements){

   *out = NULL;
   assert(ck);
   if(!ck) return false;
   compression::S_compress_header hdr;
   ck->read(&hdr, sizeof(hdr));
   if(hdr.id!=COMPRESS_ID) return false;
   dword num = hdr.orig_elements;
   *num_elements = num;
   *out = new T[num];
   assert(*out);
   T *tp = *out;

   while(num){
      dword code = ck->ReadByte();
      if(code&0x80){          //compressed
         T t1;
         ck->read(&t1, sizeof(T));
         code -= 127;
         assert(code<=num);   //we can't overflow
         if(code>num)
            goto fail;
         for(dword j=code; j--; )
            *tp++ = t1;
            //tp[j] = t1;
      }else{
         ++code;
         assert(code<=num);   //we can't overflow
         if(code>num)
            goto fail;
         ck->read(tp, code*sizeof(T));
         tp += code;
      }
      num -= code;
   }
   return true;
fail:                         //error!
   delete[] *out;
   *out = NULL;
   return false;
}

//----------------------------

#endif