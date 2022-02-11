#include "sortlist.hpp"
#include <malloc.h>

static dword temp_counters[256*4];

//----------------------------

#define MAX_STACK_ALLOC 0x8000

//----------------------------

#pragma warning(disable:4731)

//----------------------------
// Build indicies into tables by count.
// Also clear the counter table (256 dword values).
static void __cdecl MakeCountIndicies(dword *indx, dword *count){

   __asm{
                              //eax = counter
                              //ecx = index counter
                              //edx = zero
                              //esi = source pointer
                              //edi = dest pointer
      xor ecx, ecx
      xor edx, edx
      mov esi, count
      mov edi, indx
      xor eax, eax
l:
      mov [edi+eax*4], ecx
      add ecx, [esi+eax*4]
      mov [esi+eax*4], edx
      inc al
      jnz l
   }
}

//-------------------------------------
// Make counts of four bytes of dword sort values.
static void __cdecl MakeCountsOfBytes(const S_sort_entry *se, dword num, dword *temp_counts){

   __asm{
                              //eax = <free>
                              //ebx = <free>
                              //edx = <free>
                              //esi = source
                              //edi = counter
      mov esi, se
      mov edi, num
      xor eax, eax
      xor ebx, ebx
      push ebp
      mov ebp, temp_counts
   l: 
      mov edx, [esi]
      add esi, SIZE S_sort_entry
      mov ecx, edx
      mov al, dl
      shr ecx, 16
      inc dword ptr [ebp + 0x000 + eax*4]
      mov bl, cl
      xor ch, 80h
      mov al, dh
      inc dword ptr [ebp + 0x800 + ebx*4]
      shr ecx, 8
      inc dword ptr [ebp + 0x400 + eax*4]
      inc dword ptr [ebp + 0xc00 + ecx*4]

      dec edi
      jnz l

      pop ebp
   }
}
//-------------------------------------
// Sort by byte 0.
static void __cdecl SortByRadix0(S_sort_entry *dst, const S_sort_entry *src, dword *indx, dword num){

   __asm{
                              //eax = <worker>
                              //ebx = counter
                              //edx = index buffer
                              //esi = src
                              //edi = dst
                              //ebp = <free>
      mov edi, dst
      mov esi, src
      mov edx, indx
      mov ebx, num
      push ebp
      xor eax, eax
   l:
                              //get and copy sort value
      mov ecx, [esi]
      mov al, cl
      mov ebp, [edx+eax*4]
      inc dword ptr[edx+eax*4]
      mov [edi+ebp*8], ecx
                              //copy data
      mov ecx, [esi+4]
      mov [edi+ebp*8+4], ecx
                              //next iteration
      add esi, SIZE S_sort_entry
      dec ebx
      jnz l

      pop ebp
   }
}

//-------------------------------------
// Sort by byte 1.
static void __cdecl SortByRadix1(S_sort_entry *dst, const S_sort_entry *src, dword *indx, dword num){

   __asm{
                              //eax = <worker>
                              //ebx = counter
                              //edx = index buffer
                              //esi = src
                              //edi = dst
                              //ebp = <free>
      mov edi, dst
      mov esi, src
      mov edx, indx
      mov ebx, num
      push ebp
      xor eax, eax
   l:
                              //get and copy sort value
      mov ecx,[esi]
      mov al, ch
      mov ebp, [edx+eax*4]
      inc dword ptr[edx+eax*4]
      mov [edi+ebp*8], ecx
                              //copy data
      mov ecx, [esi+4]
      mov [edi+ebp*8+4], ecx
                              //next iteration
      add esi, SIZE S_sort_entry
      dec ebx
      jnz l

      pop ebp
   }
}

//-------------------------------------
// Sort by byte 2.
static void __cdecl SortByRadix2(S_sort_entry *dst, const S_sort_entry *src, dword *indx, dword num){

   __asm{
                              //eax = <worker>
                              //ebx = counter
                              //edx = index buffer
                              //esi = src
                              //edi = dst
                              //ebp = <free>
      mov edi, dst
      mov esi, src
      mov edx, indx
      mov ebx, num
      push ebp
      xor eax, eax
   l:
                              //get and copy sort value
      mov al, [esi+2]
      mov ecx, [esi]
      mov ebp, [edx+eax*4]
      inc dword ptr[edx+eax*4]
      mov [edi+ebp*8], ecx
                              //copy data
      mov ecx, [esi+4]
      mov [edi+ebp*8+4], ecx
                              //next iteration
      add esi, SIZE S_sort_entry
      dec ebx
      jnz l

      pop ebp
   }
}

//-------------------------------------
// Sort by byte 3 (signed).
static void __cdecl SortByRadix3(S_sort_entry *dst, const S_sort_entry *src, dword *indx, dword num){

   __asm{
                              //eax = <worker>
                              //ebx = counter
                              //edx = index buffer
                              //esi = src
                              //edi = dst
                              //ebp = <free>
      mov edi, dst
      mov esi, src
      mov edx, indx
      mov ebx, num
      push ebp
      xor eax, eax
   l:
                              //get and copy sort value
      mov al, [esi+3]
      xor al, 80h
      mov ecx, [esi]
      mov ebp, [edx+eax*4]
      inc dword ptr[edx+eax*4]
      mov [edi+ebp*8], ecx
                              //copy data
      mov ecx, [esi+4]
      mov [edi+ebp*8+4], ecx
                              //next iteration
      add esi, SIZE S_sort_entry
      dec ebx
      jnz l

      pop ebp
   }
}

//----------------------------
// Full int sorting.
// Skips sorting certain radix, if all values are similar.
//void RadixSort32(S_sort_entry *data, int count, dword *temp_counters)
void __cdecl RadixSort32(S_sort_entry *data, int count1){

   dword count = count1;
   if(!count)
      return;
   dword index[256];
   MakeCountsOfBytes(data, count, temp_counters);
   S_sort_entry *temp;
   if(count1*sizeof(S_sort_entry) <= MAX_STACK_ALLOC)
      temp = (S_sort_entry*)alloca(count1*sizeof(S_sort_entry));
   else
      temp = new S_sort_entry[count];
   S_sort_entry *src_ptr = data, *dst_ptr = temp;

                              //sort by byte 0
   if(temp_counters[0 + 0]==count)
      temp_counters[0 + 0] = 0;
   else
   if(temp_counters[0 + 0xff]==count)
      temp_counters[0 + 0xff] = 0;
   else{
      MakeCountIndicies(index, temp_counters + 0);
      SortByRadix0(dst_ptr, src_ptr, index, count);
      Swap((int&)src_ptr, (int&)dst_ptr);
   }

                              //sort by byte 1
   if(temp_counters[256 + 0]==count)
      temp_counters[256 + 0] = 0;
   else
   if(temp_counters[256 + 0xff]==count)
      temp_counters[256 + 0xff] = 0;
   else{
      MakeCountIndicies(index, temp_counters + 256);
      SortByRadix1(dst_ptr, src_ptr, index, count);
      Swap((int&)src_ptr, (int&)dst_ptr);
   }

                              //sort by byte 2
   if(temp_counters[512 + 0]==count)
      temp_counters[512 + 0] = 0;
   else
   if(temp_counters[512 + 0xff]==count)
      temp_counters[512 + 0xff] = 0;
   else{
      MakeCountIndicies(index, temp_counters + 512);
      SortByRadix2(dst_ptr, src_ptr, index, count);
      Swap((int&)src_ptr, (int&)dst_ptr);
   }

                              //sort by byte 3
   if(temp_counters[768 + 0x80]==count)
      temp_counters[768 + 0x80] = 0;
   else
   if(temp_counters[768 + 0x7f]==count)
      temp_counters[768 + 0x7f] = 0;
   else{
      MakeCountIndicies(index, temp_counters + 768);
      SortByRadix3(dst_ptr, src_ptr, index, count);
      Swap((int&)src_ptr, (int&)dst_ptr);
   }
                              //make sure that data end up back in user's memory
   if(src_ptr!=data)
      memcpy(data, temp, sizeof(S_sort_entry)*count);
   if(count1*sizeof(S_sort_entry) > MAX_STACK_ALLOC)
   delete[] temp;
}

//--------------------------------