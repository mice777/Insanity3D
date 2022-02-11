#ifndef __C_LINKLIST_H
#define __C_LINKLIST_H

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Linked list system.
// Compact system without additional memory allocation.
// Entire data are kept in provided class, which must become
// the parent class of class using the list.
//
// Intented for situation, where performance and low memory usage
// is more priority over code readability.
//----------------------------

#include <rules.h>

//----------------------------
                              //forward declaration of linked list class
template<class T>
class C_link_list;


//----------------------------
                              //linked list element (to be used as base class of list-element classes)
template<class T>
class C_link_list_element{
   typedef C_link_list_element<T> _t;
protected:
                              //linked-list pointers
   _t *ptr_to_next, **ptr_to_me;
   friend C_link_list<T>;
public:
   C_link_list_element():
      ptr_to_next(NULL),
      ptr_to_me(NULL)
   {}
   virtual ~C_link_list_element(){
                              //remove from the list when destroyed
      if(ptr_to_next) ptr_to_next->ptr_to_me = ptr_to_me;
      if(ptr_to_me) *ptr_to_me = ptr_to_next;
   }

//----------------------------
// Remove from the list.
   void RemoveFromList(){
      if(ptr_to_next) ptr_to_next->ptr_to_me = ptr_to_me;
      if(ptr_to_me) *ptr_to_me = ptr_to_next;
      ptr_to_me = NULL;
      ptr_to_next = NULL;
   }

//----------------------------
   inline bool IsInList() const{ return (ptr_to_me!=NULL); }

//----------------------------
// Get access to next element in the list.
   inline T *operator ++() const{ return (T*)ptr_to_next; }
};

//----------------------------
                              //linked list - keeping pointer to 1st element
template<class T>
class C_link_list{
   typedef C_link_list_element<T> _t;
   _t *ptr;
public:
   C_link_list():
      ptr(NULL)
   {}
//----------------------------
// Destructor - this just clears pointer of 1st element to this class.
   ~C_link_list(){
      ClearFast();
   }

//----------------------------
// Add element into the list.
   void Add(T *el){
      if(el->ptr_to_me)
         el->RemoveFromList();
      el->ptr_to_next = ptr;
      el->ptr_to_me = &ptr;
      if(el->ptr_to_next)
         el->ptr_to_next->ptr_to_me = &el->ptr_to_next;
      ptr = (T*)el;
   }

//----------------------------
// Clear (fast) - removing first element from the list.
   void ClearFast(){
      if(ptr)
         ptr->ptr_to_me = NULL;
      ptr = NULL;
   }

//----------------------------
// Clear full - unlink all members of the list.
   void ClearFull(){
      for(_t *p=ptr; p; ){
         _t *pp = p;
         p = ++*p;
         pp->RemoveFromList();
      }
      assert(!ptr);
   }

//----------------------------
// Get access to the beginning of the list.
   operator T*(){ return (T*)ptr; }
   operator const T*() const{ return (T*)ptr; }

//----------------------------
// Get number of members in the list (slow!).
   dword Count() const{
      dword i = 0;
      for(_t *tp=ptr; tp; tp=++*tp)
         ++i;
      return i;
   }
};

//----------------------------

#endif