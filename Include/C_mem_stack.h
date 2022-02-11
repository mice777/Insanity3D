//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Memory-allocation class, performing allocation of small memory blocks on arenas.
// It is capable to alloc storage, and later free (without actual deallocation of pre-allocated memory),
// rewinding the current pointer, and walking through allocated blocks.
//----------------------------

#include <rules.h>

class C_mem_stack{
   enum{                      //contants
                              //each stack arena pointer points to a block of this many bytes
      MEMSTACK_ARENA_SIZE = 16384,
                              //
      EFFICIENT_ALIGNMENT = 16,
   };

//----------------------------
// Round something up to be a multiple of the EFFICIENT_ALIGNMENT.
   static dword GetEfficientSize(dword num){
      return ((num-1) | (EFFICIENT_ALIGNMENT-1)) + 1;
   }
   static dword RountUpOffsetToEfficientSize(const void *arena, dword ofs){
      return GetEfficientSize(((dword)arena) + ofs) - ((dword)arena);
   }

//----------------------------

   struct S_arena{
      S_arena *next;          //next arena in linked list
      dword used;               //total number of bytes used in this arena, counting this header
      S_arena():
         next(NULL),
         used(sizeof(S_arena))
      {}
   };

   S_arena *first;            //head of the arena linked list. 0 if no arenas yet
   S_arena *last;             //arena where blocks are currently being allocated

                              //used for iterator
   S_arena *current_arena;
   dword current_ofs;
//----------------------------
public:
   C_mem_stack():
      first(NULL),
      last(NULL),
      current_arena(NULL),
      current_ofs(0)
   {}
   ~C_mem_stack(){
      Deallocate();
   }

//----------------------------
// Allocate a block in the last arena, allocating a new arena if necessary.
// It is a runtime error if num_bytes is larger than the arena size.
   void *Alloc(dword num_bytes){
      
      const dword MAX_ALLOC_SIZE =
         MEMSTACK_ARENA_SIZE - sizeof(S_arena) - EFFICIENT_ALIGNMENT + 1;

      if(num_bytes > MAX_ALLOC_SIZE)
         throw C_except("C_mem_stack::Alloc: num_bytes too large");

                              //allocate or move to a new arena if necessary
      if(!first){
                              //allocate the first arena if necessary
         first = last = new(new char[MEMSTACK_ARENA_SIZE])S_arena;
         first->used = RountUpOffsetToEfficientSize(first, first->used);
      }else{
                              //we already have one or more arenas, see if a new arena must be used
         if((last->used + num_bytes) > MEMSTACK_ARENA_SIZE){
            if(!last->next)
	            last->next = new(new char[MEMSTACK_ARENA_SIZE])S_arena;
            last = last->next;
            last->used = sizeof(S_arena);
            last->used = RountUpOffsetToEfficientSize(last, last->used);
         }
      }
                              //allocate an area in the arena
      char *c = ((char*)last) + last->used;
      last->used = RountUpOffsetToEfficientSize(last, last->used + num_bytes);
      return c;
   }

//----------------------------
// Free all blocks in all arenas. This does not deallocate the arenas themselves, so future Alloc()s will reuse them.
   void FreeAll(){
      last = first;
      if(first){
         first->used = sizeof(S_arena);
         first->used = RountUpOffsetToEfficientSize(first, first->used);
      }
   }

//----------------------------
// Deallocate all storage.
   void Deallocate(){
      for(S_arena *a = first, *next; a; a = next){
         next = a->next;
         delete[] (char*)a;
      }
      first = last = current_arena = NULL;
      current_ofs = 0;
   }

//----------------------------
// Rewind the obstack iterator, and return the address of the first allocated block.
// Return NULL if there are no allocated blocks.
   void *Rewind(){
      current_arena = first;
      current_ofs = sizeof(S_arena);
      if(current_arena){
          current_ofs = RountUpOffsetToEfficientSize(current_arena, current_ofs);
          return ((char*)current_arena) + current_ofs;
      }
      return NULL;
   }

//----------------------------
// Return the address of the next allocated block. 'num_bytes' is the size of the previous block.
// This returns NULL if there are no more arenas.
// The sequence of 'num_bytes' parameters passed to Next() during a traversal of the list
// must exactly match the parameters passed to Alloc().
// This functions like alloc, except that no new storage is ever allocated.
   void *Next(dword num_bytes){
      if(!current_arena)
         return NULL;
      current_ofs += num_bytes;
      current_ofs = RountUpOffsetToEfficientSize(current_arena, current_ofs);
      if(current_ofs >= current_arena->used){
         current_arena = current_arena->next;
         if(!current_arena)
            return NULL;
         current_ofs = sizeof(S_arena);
         current_ofs = RountUpOffsetToEfficientSize(current_arena, current_ofs);
      }
      return ((char*)current_arena) + current_ofs;
   }
};

