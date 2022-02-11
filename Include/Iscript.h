#ifndef __VMACHINE_H
#define __VMACHINE_H
#pragma once

/*----------------------------------------------------------------\
   Copyright (c) Lonely Cat Games  All rights reserved.

   Script language compiler  and  Virtual Machine
Abstract:
 C_script class - object used for compiling source scripts from disk and
 keeping compiled programs in memory.

 C_v_machine class - object capable of loading program from C_script and
 run it in virtual processor.

|\----------------------------------------------------------------*/

#include <rules.h>
#include <C_buffer.h>

#define ISLAPI
#define ISLMETHOD_(type,method) virtual type method


//----------------------------
                              //return values
typedef long ISL_RESULT;
enum{
   ISL_OK,
   ISL_UPTODATE,
   ISL_SUSPENDED,             //execution of script is suspended
                              //script compiler
   ISLERR_NOFILE = 0x80000001,
   ISLERR_BADSCRIPT,                //an error encountered durnig compilation
   ISLERR_GENERIC,                  //internal error
   ISLERR_INVALIDOPTIONS,           //invalid command-line options
                              //load program
   ISLERR_LINKERR = 0x80000010,     //internal linker error (during Load)
   ISLERR_NOEXTSYM,                 //external symbol cannot be located (during Load)
   ISLERR_NOPRG,                    //no program is currently loaded
                              //run program
   ISLERR_INVOPCODE = 0x80000020,   //invalid opcode encountered during program run
   ISLERR_STACKCORRUPT,             //stack corrupted during program run
   ISLERR_INVADDRESS,               //invalid address specified
   ISLERR_INVPARAM,                 //invalid input parameter(s)
   ISLERR_NOT_FOUND,                //object not found
   ISLERR_BAD_TABLE_INDEX,          //invalid index into table
   ISLERR_DIVIDE_BY_ZERO,           //dividing by zero encountered
   ISLERR_NEVERENDING_LOOP,         //neverending loop detected (too many continuous instructions)
};

#define ISL_SUCCESS(n) ((long)n>=0)
#define ISL_FAIL(n) ((long)n<0)

                              //compile flags:
#define ISLCOMP_VERBOSE    1  //dump debug informations
#define ISLCOMP_PRECOMPILE 2  //save precompile info and use if available
#define ISLCOMP_FORCE_PRECOMPILED 4 //use precompiled version also if source not exist
#define ISLCOMP_PRECOMPILE_HIDDEN 8 //set hidden flag onto precompiled files

typedef class C_script *PC_script;
typedef const C_script *CPC_script;

#ifndef ISL_INTERFACE
class C_script{
public:
   ISLMETHOD_(dword,AddRef)() = 0;
   ISLMETHOD_(dword,Release)() = 0;

//----------------------------
// Error reporting function type.
   typedef void (ISLAPI T_report_err)(const char *msg, void *context, int l, int r, bool warn);

//----------------------------
// Compile source file from disk.
   ISLMETHOD_(ISL_RESULT,Compile)(const char *fname, dword flags = 0,
      const char *cmd_line = NULL,
      T_report_err *cb_err = NULL, void *cb_context = NULL) = 0;

//----------------------------
// Release current program.
   ISLMETHOD_(void,Free)() = 0;

//----------------------------
// Get current status of script.
   ISLMETHOD_(ISL_RESULT,GetStatus)() const = 0;

//----------------------------
// Set name of this script - used for identification purposes.
   ISLMETHOD_(void,SetName)(const char *n) = 0;

//----------------------------
// Get name of this script.
   ISLMETHOD_(const char*,GetName()) const = 0;

//----------------------------
// Return address of a global variable. Writing to memory pointed to by returned pointer
// is not allowed.
   ISLMETHOD_(const void*,GetAddress)(const char *var_name) const = 0;

//----------------------------
// Return pointer to compiled table template.
   ISLMETHOD_(const class C_table_template*,GetTableTemplate)() const = 0;

//----------------------------
// Dump listing of assembled code (in pseudo-instructions).
   ISLMETHOD_(bool,Dump)(void(ISLAPI *func)(const char*, void *context), void *context) const = 0;
};
#endif

PC_script ISLAPI CreateScript();

//----------------------------
                              //symbol information used with script loader -
                              // table of all external symbols, to which
                              // relocations may refer
                              //symbols may be made of chains - if name==NULL and address!=NULL,
                              // address is pointer to linked array of VM_LOAD_SYMBOL
struct VM_LOAD_SYMBOL{
   void *address;             //NULL for last symbol in table
   const char *name;          //name of symbol
};

//----------------------------

enum VM_RUN{                  //run flags
   VMR_FNCNAME = 1,           //start given by func name
   VMR_ADDRESS = 2,           //start given by address
   VMR_SAVEDCONTEXT = 3,      //use saved context
   VMR_ADDRESS_MODE_MASK = 0xf,
};

#ifndef ISL_INTERFACE
class C_v_machine{
public:
   ISLMETHOD_(dword,AddRef)() = 0;
   ISLMETHOD_(dword,Release)() = 0;

//----------------------------
// Load program from script. Function accepts pointer to an array of
// external symbols (relocation table).
   ISLMETHOD_(ISL_RESULT,Load)(CPC_script, const VM_LOAD_SYMBOL*) = 0;

//----------------------------
// Reload data segment (file-scope script variables).
   ISLMETHOD_(ISL_RESULT,ReloadData)() = 0;

//----------------------------
// Unload current program.
   ISLMETHOD_(void,Unload)() = 0;

//----------------------------
// Get current status of machine.
   ISLMETHOD_(ISL_RESULT,GetStatus)() const = 0;

//----------------------------
// Run loaded program. The beginning address is determined by 'start' parameter, the format
// of address is given by flags (on or more values of VM_RUN type). Optional parameters
// passed to called function are accepted.
   ISLMETHOD_(ISL_RESULT,Run)(dword *retval, void *start, dword flags, dword num_pars=0, ...) = 0;

//----------------------------
// Return address of a global variable, or a function.
   ISLMETHOD_(void*,GetAddress)(const char *var_name) const = 0;

//----------------------------
// Enumerate all symbols.
   ISLMETHOD_(ISL_RESULT,EnumSymbols)(void (ISLAPI *cb_proc)(const char *name, void *address, bool is_data, void *context), void *cb_context) const = 0;

//----------------------------
// Return name of the program (the same as name of loaded script, or NULL if not loaded).
   ISLMETHOD_(const char*,GetName)() const = 0;

//----------------------------
// Get currently loaded script, of NULL if not loaded.
   ISLMETHOD_(CPC_script,GetScript)() = 0;

//----------------------------
// Set user 32-bit value.
   ISLMETHOD_(void,SetData)(dword) = 0;

//----------------------------
// Get user 32-bit value.
   ISLMETHOD_(dword,GetData)() const = 0;

//----------------------------
// Save current context - stack variables of running program. To be called only
// from running program - from an extern function called by script.
// The returned ID is used as parameter to Run() function, with VMR_SAVEDCONTEXT flag.
// The value of 0 is invalid context value, and will never be returned, so it may be used
// by application to mark NULL context ID.
   ISLMETHOD_(ISL_RESULT,SaveContext)(dword *ret_id) = 0;

//----------------------------
// Clear saved context. If 'id' is zero, all contexts are cleared.
   ISLMETHOD_(ISL_RESULT,ClearSavedContext)(dword id) = 0;

//----------------------------
// Get/Set saved context data into/from buffer. To be used for example for saving the data into savegame.
   ISLMETHOD_(ISL_RESULT,GetSavedContext)(dword id, C_buffer<byte> &buf) const = 0;
   ISLMETHOD_(ISL_RESULT,SetSavedContext)(dword *ret_id, const byte *buf, dword buf_size) = 0;

//----------------------------
// Get/Set global vm's data. To be used for example for saving the data into savegame.
// This function works with same global data space as ReloadData function.
   ISLMETHOD_(ISL_RESULT,GetGlobalVariables)(C_buffer<byte> &buf) const = 0;
   ISLMETHOD_(ISL_RESULT,SetGlobalVariables)(const byte *buf, dword buf_size) = 0;

//----------------------------
// Get table associated with VM. This is returned only if script is loaded, and contains table template.
   ISLMETHOD_(class C_table*,GetTable)() = 0;

//----------------------------
// Set or clear breakpoint in specified file, on specified line.
   ISLMETHOD_(ISL_RESULT,SetBreakpoint)(const char *file, dword line, bool on) = 0;
};
#endif

typedef class C_v_machine *PC_v_machine;
typedef const C_v_machine *CPC_v_machine;

PC_v_machine ISLAPI CreateVM();
//----------------------------

#endif
