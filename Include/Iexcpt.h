#ifndef __IEXCEPT_H
#define __IEXCEPT_H

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Exception handling for Win32 API
//    - catching standard and FPU hardware exceptions
//----------------------------

#include <rules.h>
#include <C_buffer.h>

#pragma comment(lib, "iexcpt")

                              //return values from exception dialog
enum EXCPT_RETURN_CODE{
   EXCPT_CLOSE,
   EXCPT_IGNORE,
   EXCPT_DEBUG,
};

//----------------------------
// Initialize exception handler, and setup FPU control word (used to
// enable required FPU exceptions - see _ctrlfp() for details).
void __declspec(dllexport) __stdcall InitializeExceptions(dword fpu_cw = -1);

//----------------------------
// Setup function, which will be called with error string,
// when user hits 'Send' button.
// The user function should return true, if sending succeeded.
void __declspec(dllexport) __stdcall SetExceptionSendFunc(bool (__stdcall *)(const char*));

//----------------------------
// Setup function, which will be called once when crash occurs.
void __declspec(dllexport) __stdcall SetExceptionCallback(void (__stdcall *)());

//----------------------------
// Function for user exception.
extern"C"
EXCPT_RETURN_CODE __declspec(dllexport) __stdcall UserException(const char *title, const char *msg);

//----------------------------
//----------------------------

typedef C_buffer<void*> t_call_stack;

//----------------------------

void __declspec(dllexport) __stdcall GetCallStack(t_call_stack &call_stack);
EXCPT_RETURN_CODE __declspec(dllexport) __stdcall ShowCallStackDialog(const char *title,
   const char *user_message, const t_call_stack &call_stack);

//----------------------------
//----------------------------
#endif
