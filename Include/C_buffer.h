#ifndef __C_BUFFER_H
#define __C_BUFFER_H

#include <rules.h>
#include <insanity\assert.h>

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// C_buffer - template class for implementing fixed-sized buffer with automatic
// allocation, de-allocation, member-access.
//----------------------------

template<class T>
class C_buffer{
   T *buf;
   dword sz;

//----------------------------
// Realloc storage to store specified number of elements.
   virtual void realloc(dword n){
      if(n!=sz){
         T *nb = n ? new T[n] : NULL;
         for(dword i=Min(n, sz); i--; )
            nb[i] = buf[i];
         delete[] buf;
         buf = nb;
         sz = n;
      }
   }
public:
   C_buffer(): buf(NULL), sz(0){}
   inline C_buffer(const T *first, const T *last): buf(NULL), sz(0){ assign(first, last); }
   inline C_buffer(dword n, const T &x): buf(NULL), sz(0){ assign(n, x); }
   inline C_buffer(dword n): buf(NULL), sz(0){ assign(n); }
   inline C_buffer(const C_buffer &b): buf(NULL), sz(0){ assign(b.begin(), b.end()); }
   ~C_buffer(){
      realloc(0);
   }
   inline C_buffer &operator =(const C_buffer &b){ assign(b.begin(), b.end()); return *this; }

//----------------------------
// Assign elements - defined as pointers to first and last members in array.
   void assign(const T *first, const T *last){
      realloc(last-first);
      for(dword i=sz; i--; )
         buf[i] = first[i];
   }

//----------------------------
// Assign elements - reserve space, fill with specified item.
   void assign(dword n, const T &x){
      realloc(n);
      for(dword i=sz; i--; )
         buf[i] = x;
   }

//----------------------------
// Reserve space for specified number of items.
   void assign(dword n){
      realloc(n);
   }

//----------------------------
// Get pointers.
   inline const T *begin() const{ return buf; }
   inline T *begin(){ return buf; }
   inline const T *end() const{ return buf + sz; }
   inline T *end(){ return buf + sz; }

//----------------------------
// Get references.
   inline T &front(){ assert(sz); return *buf; }
   inline const T &front() const{ assert(sz); return *buf; }
   inline T &back(){ assert(sz); return buf[sz-1]; }
   inline const T &back() const{ assert(sz); return buf[sz-1]; }

//----------------------------
// Clear contents.
   void clear(){ realloc(0); }

//----------------------------
// Access elements.
   inline const T &operator[](dword i) const{ assert(i<sz); return buf[i]; }
   inline T &operator[](dword i){ assert(i<sz); return buf[i]; }

//----------------------------
// Resize buffer, fill new items with provided value.
   void resize(dword n, const T &x = T()){
      dword i = sz;
      realloc(n);
      for(; i<sz; i++)
         buf[i] = x;
   }

//----------------------------
// Get size of the buffer.
   inline dword size() const{ return sz; }
};

//----------------------------
#endif