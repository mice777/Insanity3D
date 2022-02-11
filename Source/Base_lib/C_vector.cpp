//----------------------------
// Multi-platform mobile library
// (c) Lonely Cat Games
//----------------------------
#include <C_vector.h>
#include <assert.h>
#include <memory.h>

//----------------------------

C_vector_any::C_vector_any(int es, int gs):
   elem_size(es),
   res_size(0), used_size(0),
   grow_size(gs ? gs : VECTOR_SIZE_GROW/es),
   array(NULL)
{
   assert(grow_size);
}

//----------------------------

C_vector_any::~C_vector_any(){
   delete[] array;
}

//----------------------------

void C_vector_any::_Clear(){

   for(int i=used_size; i--; )
      _Destruct(array + i*elem_size);
   used_size = 0;
   _Reserve(res_size);
}

//----------------------------

void C_vector_any::_CopyVector(const C_vector_any &v){

   if(this==&v)
      return;
   _Clear();
   int i = v.used_size;
   _Reserve(i);
   used_size = i;
   while(i--){
      int offs = i*elem_size;
      _Construct(array+offs);
      _Copy(array+offs, v.array + offs);
   }
}

//----------------------------

void *C_vector_any::_End(){
   return array + used_size*elem_size;
}

//----------------------------

void C_vector_any::_Insert(dword dst_i, const void *src, dword num){

   if(!num)
      return;
   assert(dst_i <= (dword)used_size);
   _Reserve(used_size+num);

   used_size += num;
   for(int i=used_size-num-dst_i; i--; )
      memcpy(_At(dst_i+num+i), _At(dst_i+i), elem_size);

   byte *dst = (byte*)_At(dst_i);
   for(dword i=0; i<num; i++, dst += elem_size, src = (byte*)src+elem_size){
      _Construct(dst);
      _Copy(dst, src);
   }
}

//----------------------------

void C_vector_any::_PushBack(const void *val){

   if(used_size == res_size){
      res_size = Max(1ul, res_size+grow_size);
      byte *new_a = new byte[res_size*elem_size];
      memcpy(new_a, array, used_size*elem_size);
      delete[] array;
      array = new_a;
   }
   void *ptr = _End();
   _Construct(ptr);
   _Copy(ptr, val);
   ++used_size;
}

//----------------------------

void C_vector_any::_PopBack(){

   assert(used_size);
   --used_size;
   _Destruct(_End());
}

//----------------------------

void C_vector_any::_Reserve(dword s){

   s = Max(s, Max(res_size, used_size));
   if(res_size!=s){
      res_size = s;
      byte *new_a = new byte[res_size*elem_size];
      memcpy(new_a, array, used_size*elem_size);
      delete[] array;
      array = new_a;
   }
}

//----------------------------

void *C_vector_any::_At(dword i){
   assert(i<=used_size);
   return array + i*elem_size;
}

//----------------------------

void C_vector_any::_Erase(dword indx, dword num){

   if(!num)
      return;
   assert(indx+num <= (dword)used_size);
   byte *bp = (byte*)_At(indx), *base = bp;

   for(dword i=0; i<num; i++, bp += elem_size)
      _Destruct(bp);

   memcpy(base, bp, (used_size-indx-num)*elem_size);
   used_size -= num;
}

//----------------------------

void C_vector_any::_Resize(dword n, const void *val){

   if(n<used_size){
      byte *ptr = (byte*)_End();
      for(int i=used_size-n; i--; ){
         ptr -= elem_size;
         _Destruct(ptr);
      }
      used_size = n;
   }else{
      _Reserve(n);
      while(used_size<n)
         _PushBack(val);
   }
}

//----------------------------
