#ifndef __C_SORTLIST_HPP
#define __C_SORTLIST_HPP

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Sort list class - system using radix sort for sorting
// data indexed by integer key.
//----------------------------

#include <rules.h>
#include <memory.h>


#ifdef _DEBUG
# pragma comment (lib,"base_lib_d")
#else
# pragma comment (lib,"base_lib")
#endif

//----------------------------
                              //sorting structure - a data item with associated sorting value (int).
struct S_sort_entry{
   int sort_value;
   void *data;
};

//----------------------------
// Sorting function - used to sort data.
void __cdecl RadixSort32(S_sort_entry*, int);

//----------------------------

template<class T>
class C_sort_list{
   S_sort_entry *base;
   dword def_res, res_num, count;

   C_sort_list& operator =(const C_sort_list&);
   C_sort_list(const C_sort_list&);

   bool ReAllocate(dword new_res){
      new_res = Max(def_res, new_res);
      S_sort_entry *new_base = new S_sort_entry[new_res];
      if(!new_base)
         return false;
      memcpy(new_base, base, count*sizeof(S_sort_entry));
      res_num = new_res;
      delete[] base;
      base = new_base;
      return true;
   }
public:
   C_sort_list():
      base(NULL),
      res_num(0), def_res(16), count(0)
   {
   }
   C_sort_list(dword res):
      base(NULL),
      res_num(0), count(0)
   {
      Reserve(res);
   }
   ~C_sort_list(){
      delete[] base;
   }

//----------------------------
// Reserve storage.
   inline void Reserve(dword r){
      def_res = Max(4ul, r);
   }

//----------------------------
// Add item with associated sort value.
   bool Add(const T &t, int sort_val){
      if(count==res_num){
         if(!ReAllocate(res_num*2))
            return false;
      }
      base[count].sort_value = sort_val;
      base[count++].data = (void*)t;
      return true;
   }

//----------------------------
// Clear contents of the list.
   inline void Clear(){
      count = 0;
   }

//----------------------------
// Get number of items in the list.
   inline dword Count() const{
      return count;
   }

//----------------------------
// Sort list.
   inline void Sort(dword num = 0){
      RadixSort32(base, num ? Min(count, num) : count);
   }

//----------------------------
// Get item on fiven index.
   inline T &operator [](dword i){
      return (T&)base[i].data;
   }
   inline const T &operator [](dword i) const{
      assert(i<count);
      return (T&)base[i].data;
   }

//----------------------------
// Get sort value on given index.
   inline int GetSortValue(int i) const{
      assert(i<count);
      return base[i].sort_value;
   }

//----------------------------
// Set (modify) item or its sort value.
   inline void SetSortValue(int i, int sv){
      assert(i<count);
      base[i].sort_value = sv;
   }
   inline void SetData(int i, const T &t){
      assert(i<count);
      base[i].data = (void*)t;
   }
};

//----------------------------

#endif
