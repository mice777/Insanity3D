#ifndef __C_STR_HPP
#define __C_STR_HPP
#pragma once

/*----------------------------------------------------------------\
Copyright (c) Lonely Cat Games  All rights reserved.
Abstract:
 C_str class - reference-counting string class, minimizing memory copying
 as possible, keeping reference to other string after copying.
 Safe access - making its own copy on non-constant access.
 Keeping valid NULL-terminated "C" string even if not initialized.
 Allowing wild-card compare with other string.
 Support for single-byte and double-byte contents.
 Usable for shared memory data.

 C_fstr - inherited from C_str, having additional constructor for
 creating string from formatted string and variable-length arguments.

Revision 17 feb 2003 by Michal : added C_xstr (type-safe formatted string, replacement for C_fstr).

|\----------------------------------------------------------------*/
#include <string.h>
#include <rules.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <insanity\assert.h>
#include <new.h>
#include <stddef.h>
#include <exception>          //can't use C_except, because it uses C_str

#define CFSTRING_MAX_FMT_BUFFER 1024  //size of formatting buffer, in characters

#pragma warning(disable:4996)

//----------------------------
//String - keeping reference to string data.
class C_str{

//----------------------------
// String base - concrete string data. This class' size is
// variable, depending on size of string it contains.
#pragma pack(push,1)
   class C_rep{
      dword ref;
      size_t size;             //size without terminating null character
      bool wide;
      char data[2];
   public:
      inline C_rep(const void *cp, size_t len, bool w): ref(1), size(len), wide(w){
         memcpy(data, cp, len);
         *(word*)(data+len) = 0;
      }
      inline C_rep(size_t len, bool w): ref(1), size(len), wide(w){ }
      inline C_rep(const void *cp1, const void *cp2, size_t l1, size_t l2, bool w): ref(1), size(l1+l2), wide(w){
         memcpy(data, cp1, l1);
         memcpy(data+l1, cp2, l2);
         *(word*)(data+size) = 0;
      }
      virtual void DeleteThis(){
         this->~C_rep();
         delete[] (byte*)this;
      }

      inline void AddRef(){ ++ref; }
      inline dword Release(){ return --ref; }
      inline dword Count() const{ return ref; }

      inline char *GetData(){ return data; }
      inline const char *GetData() const{ return data; }
      inline wchar_t *GetDataW(){ return (wchar_t*)data; }
      inline const wchar_t *GetDataW() const{ return (const wchar_t*)data; }
      inline size_t Size() const{ return size; }
      inline bool IsWide() const{ return wide; }
   };
#pragma pack(pop)

   C_rep *rep;

   inline void *AllocRep(size_t sz) const{ return new byte[sizeof(C_rep) + sz]; }
   inline void ReleaseRep(){
      if(rep && !rep->Release()){
                              //virtual deletion function of rep is called, so it is freed in those module,
                              // which allocated the string
         rep->DeleteThis();
      }
   }
   inline void ReplaceRep(C_rep *r){ ReleaseRep(); rep = r; }

//----------------------------
// Make sure this string is the only owner of actual string data.
   void MakeUniqueCopy(){
      if(rep->Count()!=1){
                              //use placement new for proper allocation and initialization of string data.
         C_rep *new_rep = new(AllocRep(rep->Size())) C_rep(rep->GetData(), rep->Size(), IsWide());
         ReplaceRep(new_rep);
      }
   }
   typedef wchar_t wchar;
public:
//----------------------------
// Empty constructor.
   inline C_str(): rep(NULL) {}

//----------------------------
// Constructor from C string.
   inline C_str(const char *cp){
      if(!cp) rep = NULL;
      else{ size_t len = strlen(cp); rep = new(AllocRep(len)) C_rep(cp, len, false); }
   }
   explicit inline C_str(const wchar *cp){
      if(!cp) rep = NULL;
      else{ size_t len = wcslen(cp) * 2; rep = new(AllocRep(len)) C_rep(cp, len, true); }
   }

//----------------------------
// Copy constructor.
   inline C_str(const C_str &s){ rep=s.rep; if(rep) rep->AddRef(); }

//----------------------------
// Destructor.
   inline ~C_str(){ ReleaseRep(); }

//----------------------------
// Getting pointer to C string. This is always valid NULL-terminated string,
// even if the string is empty. It is valid until any other operation is done on
// contents of string.
   inline operator const char *() const{ if(!rep) return (const char*)&rep; assert(!IsWide()); return rep->GetData(); }
   inline operator const wchar *() const{ if(!rep) return (const wchar*)&rep; assert(IsWide() || !Size()); return rep->GetDataW(); }

//----------------------------
// Assignment operator - fast copy of reference from given string.
   C_str &operator =(const C_str &s){
      if(s.rep) s.rep->AddRef();
      ReplaceRep(s.rep);
      return (*this);
   }

//----------------------------
// Assignment from C string.
   C_str &operator =(const char *cp){
      C_rep *new_rep;
      if(!cp) new_rep = NULL;
      else{ size_t len = strlen(cp); new_rep = new((AllocRep(len))) C_rep(cp, len, false); }
      ReplaceRep(new_rep);
      return (*this);
   }

//----------------------------
// Assignment from C wide string.
   C_str &AssignW(const wchar *cp){
      C_rep *new_rep;
      if(!cp) new_rep = NULL;
      else{ size_t len = wcslen(cp) * 2; new_rep = new((AllocRep(len))) C_rep(cp, len, true); }
      ReplaceRep(new_rep);
      return (*this);
   }

//----------------------------
// Assign raw data to string. Data may be anything, no '\0' character at the end is searched.
// If 'cp' is NULL, the string allocates uninitialized buffer for holding the string,
// which may be filled further by array-access functions.
   C_str &Assign(const char *cp, dword size){
      C_rep *new_rep;
      if(!cp) new_rep = new((AllocRep(size))) C_rep(size, false);
      else new_rep = new((AllocRep(size))) C_rep(cp, size, false);
      ReplaceRep(new_rep);
      return (*this);
   }

//----------------------------
// Concanetate string with another C string and store result in this string.
   C_str &operator +=(const char *cp){
      if(!rep) return operator =(cp);
      assert(!IsWide());
      size_t len = rep->Size(), len1 = strlen(cp);
      C_rep *new_rep = new((AllocRep(len + len1))) C_rep(rep->GetData(), cp, len, len1, false);
      ReplaceRep(new_rep);
      return (*this);
   }

//----------------------------
// Concanetate string with another C string and store result in this string.
   C_str &operator +=(const wchar *cp){
      if(!rep) return AssignW(cp);
      assert(IsWide());
      size_t len = rep->Size(), len1 = wcslen(cp) * 2;
      C_rep *new_rep = new((AllocRep(len + len1))) C_rep(rep->GetData(), cp, len, len1, true);
      ReplaceRep(new_rep);
      return (*this);
   }

//----------------------------
// Concanetate string with another string and store result in this string.
   C_str &operator +=(const C_str &s){
      if(!rep) return operator =(s);
      assert(!s.rep || (IsWide()==s.IsWide()));
      size_t len = Size(), len1 = s.Size();
      if(IsWide()){
         len *= 2;
         len1 *= 2;
      }
      C_rep *new_rep = new((AllocRep(len + len1))) C_rep(rep->GetData(), s.rep->GetData(), len, len1, IsWide());
      ReplaceRep(new_rep);
      return (*this);
   }

//----------------------------
// Add this string with another string and return result as a string.
   C_str operator +(const C_str &s) const{
      if(!rep) return s;
      assert(!s.rep || (IsWide()==s.IsWide()));
      size_t l1 = Size(), l2 = s.Size();
      if(!l2) return *this;
      C_str ret;
      ret.rep = new((AllocRep(l1 + l2))) C_rep(rep->GetData(), s.rep->GetData(), l1, l2, IsWide());
      return ret;
   }

//----------------------------
// Add this string with another C string and return result as a string.
   C_str operator +(const char *cp) const{
      if(!rep) return cp;
      assert(!IsWide());
      size_t l1 = Size(), l2 = strlen(cp);
      C_str ret;
      ret.rep = new((AllocRep(l1 + l2))) C_rep(rep->GetData(), cp, l1, l2, false);
      return ret;
   }
   C_str operator +(const wchar *cp) const{
      if(!rep) return C_str(cp);
      assert(IsWide());
      size_t l1 = Size() * 2, l2 = wcslen(cp) * 2;
      C_str ret;
      ret.rep = new((AllocRep(l1 + l2))) C_rep(rep->GetData(), cp, l1, l2, true);
      return ret;
   }

//----------------------------
// Get const character reference inside of string on particular position.
   inline const char &operator [](dword pos) const{ assert(pos<(Size()+1)); if(!rep) return *(char*)&rep; assert(!IsWide()); return rep->GetData()[pos]; }
   inline const char &operator [](int pos) const{ return operator[](dword(pos)); }
   inline const wchar &GetW(dword pos) const{ assert(pos<(Size()+1)); if(!rep) return *(wchar*)&rep; assert(IsWide()); return rep->GetDataW()[pos]; }

//----------------------------
// Get non-const character reference inside of string on particular position.
// If contents of string is shared among multiple strings, it is made unique.
   char &operator [](dword pos){ assert(pos<(Size()+1)); if(!rep) return *(char*)&rep; assert(!IsWide()); MakeUniqueCopy(); return rep->GetData()[pos]; }
   char &operator [](int pos){ return operator[](dword(pos)); }
   wchar &GetW(dword pos){ assert(pos<(Size()+1)); if(!rep) return *(wchar*)&rep; assert(IsWide()); MakeUniqueCopy(); return rep->GetDataW()[pos]; }

//----------------------------
// Compare this string with another C string for equality. Slow, depends on sizes of both strings.
   inline bool operator ==(const char *cp) const{ return !cp ? (!Size()) : !strcmp(*this, cp); }
   inline bool operator !=(const char *cp) const{ return !cp ? Size() : strcmp(*this, cp); }
   inline bool operator ==(const wchar *cp) const{ return !cp ? (!Size()) : !wcscmp(*this, cp); }
   inline bool operator !=(const wchar *cp) const{ return !cp ? Size() : wcscmp(*this, cp); }

//----------------------------
// Compare this string with another string for equality. Optimizations based on sizes of strings.
   bool operator ==(const C_str &s) const{ if(rep==s.rep) return true; if(Size()!=s.Size()) return false;
      return !memcmp((const char*)*this, (const char*)s, Size()); }
   inline bool operator !=(const C_str &s) const{ return (!operator==(s)); }

//----------------------------
// Compare this string with another string.
   inline bool operator <(const C_str &s) const{
      if(rep && s.rep){
         assert(IsWide() == s.IsWide());
         if(IsWide())
            return wcscmp(*this, s);
      }
      return (strcmp(*this, s) < 0);
   }

//----------------------------
// Compare this string with another C strings.
   inline bool operator <(const char *s) const{ return (strcmp(*this, s) < 0); }
   inline bool operator <(const wchar *s) const{ return (wcscmp(*this, s) < 0); }

//----------------------------
// Get size of string data, without terminating NULL character.
   inline dword Size() const{ if(!rep) return 0; size_t sz = rep->Size(); if(IsWide()) sz /= 2; return (dword)sz; }

//----------------------------
// Compare this string with another wild-char string. Following wildcards are recognized:
//    * - ignore the contents of string to the end
//    ? - ignore character on current position
   bool Match(const C_str &s) const{
      //dword l1 = Size(), l2 = s.Size();
      //if(!l1 || !l2) return (l1==l2);
      for(const char *cp1=operator const char*(), *cp2=s; ; ++cp1, ++cp2){
         switch(*cp2){
         case 0: return !(*cp1);
         case '*': return true;
         case '?':
            if(!*cp1) return false;
            break;
         default: if(*cp1 != *cp2) return false;
         }
      }
   }

//----------------------------
// Wildcard compare, but ignore case.
   bool Matchi(const C_str &s) const{
      //dword l1 = Size(), l2 = s.Size();
      //if(!l1 || !l2) return (l1==l2);
      for(const char *cp1=operator const char*(), *cp2=s; ; ++cp1, ++cp2){
         switch(*cp2){
         case 0: return !(*cp1);
         case '*': return true;
         case '?':
            if(!*cp1) return false;
            break;
         default: if(tolower(*cp1) != tolower(*cp2)) return false;
         }
      }
   }

//----------------------------
// Check if string is in wide-char format.
   bool IsWide() const{ return (!rep ? false : rep->IsWide()); }

//----------------------------
// Convert string to wide.
   C_str ToWide(){ if(!Size() || IsWide()) return *this; const char *cp = *this;
      size_t sz = Size() + 1; wchar *buf = new wchar[sz];
      for(dword i=0; i<sz; i++) buf[i] = cp[i];
      C_str ret(buf); delete[] buf;
      return ret;
   }

//----------------------------
// Convert all characters to lower case.
   void ToLower(){
      dword l = Size();
      if(!l)
         return;
      MakeUniqueCopy();
      if(!IsWide()){
         char *buf = rep->GetData();
         while(l--)
            buf[l] = (char)tolower(buf[l]);
      }else{
         wchar *buf = rep->GetDataW();
         while(l--){
            wchar &c = buf[l];
            if(c<256)
               c = (char)tolower(c);
         }
      }
   }

//----------------------------
// Convert all characters to upper case.
   void ToUpper(){
      dword l = Size();
      if(!l)
         return;
      MakeUniqueCopy();
      if(!IsWide()){
         char *buf = rep->GetData();
         while(l--)
            buf[l] = (char)toupper(buf[l]);
      }else{
         wchar *buf = rep->GetDataW();
         while(l--){
            wchar &c = buf[l];
            if(c<256)
               c = (char)toupper(c);
         }
      }
   }
};

//----------------------------
// Type-safe format string.
// The fromat string is passed in constructor, while parameters are passed in successive 'feeding' calls, using the operator %.
//    The types of parameters are deteced automatically.
// Formatted parameter begins with '%' or with '#' in the format string. If it begins with '#', details about format should be specified, teminated by the 
//    '%' character (for example width, flags, precission). These parameters are fully compatilbe with printf-style formatting.
// To output '%' or '#' characters, use two of the together (for example "##" or "%%").
// The formatted string is accessible after all necessary parameters are fed, using provided operator C_str&(), or operator const char*(). These operators
//    'build' the string from the format string and provided parameters, and put the class into 'built' mode, when no more parameters may be fed.
class C_xstr{
   mutable C_str str;         //initially holds format string, when build, holds the result
   enum{                      //info about held type
      t_char,
      t_int,
      t_dword,
      t_float,
      t_double,
      t_string,
      t_last
   };
   typedef byte _t;           //variable type using to hold the type

   enum{ TMP_STORAGE = (sizeof(int)+sizeof(_t)) * 6};
   byte stor[TMP_STORAGE];    //local space used without allocating by new (up to N int-sized params)
                              //pointer to current storage, and next position (used during feeding params)
   byte *curr_stor;
   dword buf_pos;
   mutable dword alloc_size;  //currently new-allocated buffer (pointed to by curr_stor), if 0, no alloc made
   mutable bool built;        //true if string was already built (if yes, no further feeding possible)

//----------------------------
// Virtual allocation, directing alloc/free to same module.
   virtual byte *Alloc(size_t sz) const{ return new byte[sz]; }
   virtual void Free(byte *vp) const{ delete[] vp; }

//----------------------------
// Get size of data of particular type.
   dword GetDataSize(_t t, const void *dt) const{
      switch(t){
      case t_char: return sizeof(char);
      case t_int: return sizeof(int);
      case t_dword: return sizeof(unsigned int);
      case t_float: return sizeof(float);
      case t_double: return sizeof(double);
      case t_string: return strlen((const char*)dt) + 1;
      }
      assert(0);
      return 0;
   }
//----------------------------
// Add data of given type into feed buffer (manage allocation).
   void AddData(_t type, const void *data, dword data_size = 0){
      if(built)
         throw std::exception("C_xstr already built, can't feed more parameters");
      assert(!built);
      if(!data_size)
         data_size = GetDataSize(type, data);
      dword sz = sizeof(_t) + data_size;
      dword buf_sz = alloc_size ? alloc_size : TMP_STORAGE;
      if(buf_pos + sz > buf_sz){
         if(buf_pos+sizeof(_t) <= buf_sz)
            *(_t*)(curr_stor+buf_pos) = t_last;
         if(!alloc_size){
            alloc_size = TMP_STORAGE*2;
            if(alloc_size<sz)
               alloc_size = sz;
            curr_stor = Alloc(alloc_size);
            buf_pos = 0;
         }else{
            alloc_size = (alloc_size*2+3) & -4;
            if(alloc_size<buf_pos+sz)
               alloc_size = buf_pos+sz;
            byte *ns = Alloc(alloc_size);
            memcpy(ns, curr_stor, buf_pos);
            Free(curr_stor);
            curr_stor = ns;
         }
      }
      byte *dt = curr_stor + buf_pos;
      *(_t*)dt = type;
      memcpy(dt+sizeof(_t), data, data_size);
      buf_pos += sz;
   }
//----------------------------
// Get data from the feed buffer, advance input pointers to next data.
   const byte *GetData(const byte *&cs, dword &cp) const{
      dword buf_sz = (cs==stor) ? TMP_STORAGE : buf_pos;
      if(cp+sizeof(_t)>buf_sz || *(_t*)(cs+cp)==t_last){
         if(cs!=stor)
            throw std::exception("C_xstr: cannot build string, not enough parameters!");
         cs = curr_stor;
         cp = 0;
      }
      const byte *ret = cs + cp;
      cp += sizeof(_t) + GetDataSize(*(_t*)ret, ret+sizeof(_t));
      return ret;
   }
//----------------------------
// Build string from given format string and fed parameters (if not built yet).
   void Build() const{
      if(built)
         return;
      built = true;

      dword buf_size = (str.Size() + 100) & -4;
      char *buf = new char[buf_size];
      dword buf_pos = 0;
                              //iterators for getting fed params
      const byte *cs = stor;
      dword cp = 0;

      try{
         const char *p = str;
         while(true){
            const char *c = p++;
            char orig_c = *c;
            dword add_size = 1;
            char tmp[512];
            if(*c=='%' || *c=='#'){
               if(*p==*c){
                              //detect intended # and % chars
                  ++p;
               }else{
                              //format parameter
                  char fs[64], *fp = fs;
                  *fp++ = '%';
                  if(*c=='#'){
                              //# is used for additional format params
                     while(*p!='%'){
                        if(!*p)
                           throw std::exception("C_xstr: # without matching % in format string");
                        *fp++ = *p++;
                        if(fp == fs+sizeof(fs)-4)
                           throw std::exception("C_xstr: format params too long");
                     }
                     ++p;
                  }
                  const byte *dt = GetData(cs, cp);
                  static const char tc[] = { 'c', 'i', 'i', 'f', 'e', 's' };
                  _t type = *((_t*&)dt)++;
                  *fp++ = tc[type]; *fp++ = '%'; *fp++ = 'n'; *fp = 0;

                  c = tmp;
                  switch(type){
                  case t_char: sprintf(tmp, fs, *(char*)dt, &add_size); break;
                  case t_int: sprintf(tmp, fs, *(int*)dt, &add_size); break;
                  case t_dword: sprintf(tmp, fs, *(unsigned int*)dt, &add_size); break;
                  case t_float: sprintf(tmp, fs, *(float*)dt, &add_size); break;
                  case t_double: sprintf(tmp, fs, *(double*)dt, &add_size); break;
                  case t_string: c = (const char*)dt; add_size = strlen(c); break;
                  default: assert(0);
                  }
                  assert(type==t_string || (add_size < sizeof(tmp)-1));
               }
            }
                              //realloc buf if necessary
            if(buf_pos+add_size > buf_size){
               buf_size = ((buf_pos + add_size) * 2 + 3) & -4;
               char *nb = new char[buf_size];
               memcpy(nb, buf, buf_pos);
               delete[] buf;
               buf = nb;
            }
                              //store next char, or formatted parameter
            if(add_size==1)
               buf[buf_pos] = *c;
            else
               memcpy(buf+buf_pos, c, add_size);
                              //detect end of string
            if(!orig_c)
               break;
            buf_pos += add_size;
         }
      }catch(...){
         delete[] buf;
         throw;
      }
      str = buf;
      delete[] buf;
      if(alloc_size){
         Free(curr_stor);
         alloc_size = 0;
      }
   }
public:
   C_xstr(const C_str &cp):
      str(cp),
      built(false),
      curr_stor(stor), buf_pos(0), alloc_size(0)
   {
   }
   ~C_xstr(){
      if(alloc_size)
         Free(curr_stor);
   }

//----------------------------
// Parameter feeding for basic types, using operator %.
   inline C_xstr &operator %(char d){ AddData(t_char, &d); return *this; }
   inline C_xstr &operator %(int d){ AddData(t_int, &d); return *this; }
   inline C_xstr &operator %(dword d){ AddData(t_dword, &d); return *this; }
   inline C_xstr &operator %(unsigned int d){ operator %(dword(d)); return *this; }
   inline C_xstr &operator %(float d){ AddData(t_float, &d); return *this; }
   inline C_xstr &operator %(double d){ AddData(t_double, &d); return *this; }
   inline C_xstr &operator %(const char *d){ AddData(t_string, d); return *this; }
   inline C_xstr &operator %(const C_str &d){ AddData(t_string, (const char*)d, d.Size()+1); return *this; }
   inline C_xstr &operator %(const C_xstr &d){ operator %(C_str(d)); return *this; }

//----------------------------
// Get acces to built formatted string.
   operator const C_str&() const{
      Build();
      return str;
   }
   operator const char*() const{
      Build();
      return str;
   }
};

//----------------------------
// Old type of formatted string, kept due to compatibility.
class C_fstr: public C_str{
public:
   C_fstr(const char *cp, ...){
      va_list arglist;
      va_start(arglist, cp);
      char buf[CFSTRING_MAX_FMT_BUFFER];
#ifdef _MSC_VER
#ifndef NDEBUG
      int num = 
#endif
         _vsnprintf(buf, sizeof(buf), cp, arglist);
      assert(num>=0);         //not enough big buffer? report it
#else
      vsprintf(buf, cp, arglist);
#endif
      va_end(arglist);
      C_str::operator=(buf);
   }
};

//----------------------------

#endif
