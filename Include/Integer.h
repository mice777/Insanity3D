#ifndef __INTEGER_H_
#define __INTEGER_H_

#include <rules.h>


//----------------------------
// Get index of the most significant bit in the mask,
// or -1 if such bit doesn't exist.
inline int FindHighestBit(dword mask){

   int base = 0;
   if(mask&0xffff0000){
      base = 16;
      mask >>= 16;
   }
   if(mask&0x0000ff00){
      base += 8;
      mask >>= 8;
   }
   if(mask&0x000000f0){
      base += 4;
      mask >>= 4;
   }
   static const int lut[] = {-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};
   return base + lut[mask];
}

//----------------------------
// Get index of the least significant bit in the mask,
// or -1 if such bit doesn't exist.
inline int FindLowestBit(dword mask){

   if(!mask)
      return 0;

   int base = 0;

   if(!(mask&0xffff)){
      base = 16;
      mask >>= 16;
   }
   if(!(mask&0x00ff)){
      base += 8;
      mask >>= 8;
   }
   if(!(mask & 0x000f)){
      base += 4;
      mask >>= 4;
   }
   static const int lut[] = {-1, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};
   return base + lut[mask&15];
}

//----------------------------
// Get number of set bits in the mask.
inline int CountBits(dword mask){

   mask = (mask&0x55555555) + ((mask&0xaaaaaaaa) >> 1);
   mask = (mask&0x33333333) + ((mask&0xcccccccc) >> 2);
   mask = (mask&0x0f0f0f0f) + ((mask&0xf0f0f0f0) >> 4);
   mask = (mask&0x00ff00ff) + ((mask&0xff00ff00) >> 8);
   mask = (mask&0x0000ffff) + ((mask&0xffff0000) >> 16);
   return mask;
}

//----------------------------
// Reverse bit order.
inline void ReverseBits(dword &mask){

   mask = ((mask >>  1) & 0x55555555) | ((mask <<  1) & 0xaaaaaaaa);
   mask = ((mask >>  2) & 0x33333333) | ((mask <<  2) & 0xcccccccc);
   mask = ((mask >>  4) & 0x0f0f0f0f) | ((mask <<  4) & 0xf0f0f0f0);
   mask = ((mask >>  8) & 0x00ff00ff) | ((mask <<  8) & 0xff00ff00);
   mask = ((mask >> 16) & 0x0000ffff) | ((mask << 16) & 0xffff0000) ;
}

//----------------------------
// Check if value is a power-of-two.
inline bool IsPowerOfTwo(dword mask){
   return ((mask & (mask-1)) == 0);
}

//----------------------------
// Return the next power-of-two of a 32bit number
inline dword NextPowerOfTwo(dword v){

   v -= 1;
   v |= v >> 16;
   v |= v >> 8;
   v |= v >> 4;
   v |= v >> 2;
   v |= v >> 1;
   return v + 1;
}

//----------------------------

#endif
