#include "rules.h"
#pragma warning(push,3)
#include <vector>
#pragma warning(pop)

//----------------------------

#ifdef _MSC_VER
using namespace std;
#endif


#define ID 0x4d4f4349

//----------------------------

bool Compress(void *in, size_t in_size, void **out, size_t *out_size, bool decompress){

   *out = NULL;
   *out_size = 0;
#pragma pack(push, 1)
   struct S_header{
      dword id;
      dword pack_len, unpk_len;
      char mark;
   };
#pragma pack(pop)
   int i;
   byte *bp;

   if(!decompress){
                              //find less-used byte
      int byte_use[256];
      memset(byte_use, 0, sizeof(byte_use));
      bp = (byte*)in;
      for(i=in_size; i--;) ++byte_use[bp[i]];
      int min_use = in_size + 1;
      byte mark = 0;
      for(i=0; i<256; i++)
         if(min_use>byte_use[i]){
            min_use = byte_use[mark=i];
            if(!min_use) break;
         }

                              //start packing
      vector<byte> vect;
                              //reserve space for header
      for(i=0; i<sizeof(S_header); i++) vect.push_back(0);

                              //check for zero-size
      if(in_size){
         byte curr_byte = bp[i=0];
         int count = 1;
         while(++i<(int)in_size){
            byte next = bp[i];
            if(next==curr_byte) ++count;
            else{
                              //store data
               if(curr_byte==mark || count>3){
                  while(count>256){
                     vect.push_back(mark); vect.push_back(255); vect.push_back(curr_byte);
                     count -= 256;
                  }
                  if(count){
                     vect.push_back(mark); vect.push_back(count-1); vect.push_back(curr_byte);
                  }
               }else
                  while(count--) vect.push_back(curr_byte);
               curr_byte = next;
               count = 1;
            }
         }
                                 //store rest
         if(curr_byte==mark || count>3){
            while(count>256){
               vect.push_back(mark); vect.push_back(255); vect.push_back(curr_byte);
               count -= 256;
            }
            if(count){
               vect.push_back(mark); vect.push_back(count-1); vect.push_back(curr_byte);
            }
         }else
            while(count--) vect.push_back(curr_byte);
      }

                              //fill-in header
      S_header hdr;
      hdr.id = ID;
      hdr.pack_len = vect.size();
      hdr.unpk_len = in_size;
      hdr.mark = mark;
                              //store header
      memcpy(&vect[0], &hdr, sizeof(hdr));

                              //copy output
      *out = new byte[*out_size = vect.size()];
      if(*out){
         memcpy(*out, &vect[0], vect.size());
         return true;
      }
   }else{
                              //check validity
      if(in_size<sizeof(S_header)) return false;
      S_header *hdr = (S_header*)in;
      if(hdr->id!=ID || hdr->pack_len!=in_size) return false;

                              //alloc mem
      bp = ((byte*)in)+sizeof(S_header);
      byte *bp_out = new byte[hdr->unpk_len];
      if(!bp_out) return false;
      *out = bp_out;
      *out_size = hdr->unpk_len;

                              //decompress
      i = hdr->pack_len-sizeof(S_header);
      byte mark = hdr->mark;
      while(i--){
         byte b = *bp++;
         if(b==mark){
            i -= 2;
            int count = (*bp++) + 1;
            byte rep = *bp++;
            memset(bp_out, rep, count);
            bp_out += count;
         }else
            *bp_out++ = b;
      }
      return true;
   }
   return false;
}

//----------------------------
