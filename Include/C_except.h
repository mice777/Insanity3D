/*******************************************************
* Copyright (c) Lonely Cat Games  All rights reserved.
* File: c_except.h
* Purpose: exception handling - classes declaration
* Created by: Michal Bacik
*******************************************************/

#ifndef __C_EXCEPT_H
#define __C_EXCEPT_H

#include <C_str.hpp>
#include <exception>
#ifdef USE_EXCEPT_STACK
#include <IExcpt.h>
#endif

//----------------------------
// General exception handler - derived from C++ exception class, in order to contain compatible base.
class C_except: public std::exception{
protected:
   C_str description;         //description of exception
#ifdef USE_EXCEPT_STACK
   t_call_stack call_stack;   //call stack collected at constructor (optional)
#endif

public:
//----------------------------
// Create exception - constructors.
   C_except(){
#ifdef USE_EXCEPT_STACK
      ::GetCallStack(call_stack);
#endif
   }
   C_except(const C_str &desc, bool get_call_stack = true):
      exception(desc),
      description(desc)
   {
#ifdef USE_EXCEPT_STACK
      if(get_call_stack)
         ::GetCallStack(call_stack);
#endif
   }
   C_except(const C_except &e){ operator =(e); }
   C_except &operator =(const C_except &e){
      description = e.description;
#ifdef USE_EXCEPT_STACK
      call_stack = e.call_stack;
#endif
      exception::operator =(e);
      return *this;
   }

#ifdef USE_EXCEPT_STACK
//----------------------------
// Get call stack.
   const t_call_stack &GetCallStack() const{ return call_stack; }

//----------------------------
// Show dialog with call stack.
   EXCPT_RETURN_CODE ShowCallStackDialog() const{
      if(call_stack.size())
         return ::ShowCallStackDialog("Insanity exception", what(), call_stack);
      return EXCPT_IGNORE;
   }
#endif//USE_EXCEPT_STACK

//----------------------------
   const C_str &GetDesc() const{ return description; }
};

//----------------------------
// Memory allocation failure.
class C_except_mem: public C_except{
public:
   dword fail_size;           //requested size, which caused the allocation to fail

   C_except_mem(dword sz = 0):
      fail_size(sz),
      C_except(C_fstr("memory allocation failed (%i bytes)", sz))
   {
   }
   C_except_mem(const char *desc, dword sz = 0):
      fail_size(sz),
      C_except(C_fstr("memory allocation failed: %s (%i bytes)", desc, sz))
   {
   }
   C_except_mem(const C_except_mem &e){ operator =(e); }
   void operator =(const C_except_mem &e){
      C_except::operator =(e);
      fail_size = e.fail_size;
   }
};

//----------------------------
// File I/O problem
class C_except_file: public C_except{
   C_except_file(const C_except_file&);
   void operator =(const C_except_file&);
public:
   C_except_file(const char *fname):
      C_except(C_fstr("failed to open file: %s", fname))
   {
   }
   C_except_file():
      C_except("failed to open file")
   {
   }
};

//----------------------------

#endif//__C_EXCEPT_H