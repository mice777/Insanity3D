/*--------------------------------------------------------
   Copyright (c) Lonely Cat Games  All rights reserved.
   Windows registry access functions.

  Note: all open/create functions take parameter which specifies
   if key is to be opened in HKEY_LOCAL_MACHINE or
   HKEY_CURRENT_USER area of system registry. The selection
   is done through enumerated type E_REGKEY_ROOT.
--------------------------------------------------------*/

#ifndef __WIN_REG_H_
#define __WIN_REG_H_

#include <rules.h>

#ifdef _DEBUG
# pragma comment (lib,"base_lib_d")
#else
# pragma comment (lib,"base_lib")
#endif

//----------------------------

enum E_REGKEY_ROOT{
   E_REGKEY_CURRENT_USER,     //data specific to currently logged user
   E_REGKEY_LOCAL_MACHINE,    //data specific to machine state (all users)
   E_REGKEY_CLASSES_ROOT,
   E_REGKEY_LAST,
   E_REGKEY_FORCE_DWORD = 0x7fffffff,
};
   
//----------------------------
// Create or open specified key. If operation fails, the return value is -1.
int RegkeyCreate(const char *name, E_REGKEY_ROOT = E_REGKEY_LOCAL_MACHINE);

//----------------------------
// Open specified key. If operation fails, the return value is -1.
int RegkeyOpen(const char *name, E_REGKEY_ROOT rk_root = E_REGKEY_LOCAL_MACHINE);

//----------------------------
// Close opened key.
void RegkeyClose(int key);

//----------------------------
// Check if specified key exists.
bool RegkeyExist(const char *name, E_REGKEY_ROOT rk_root = E_REGKEY_LOCAL_MACHINE);

//----------------------------
// Read/write data from/to registry.
bool RegkeyWdata(int key, const char *valname, const void *mem, dword len);
bool RegkeyWdword(int key, const char *valname, dword data);
bool RegkeyWtext(int key, const char *valname, const char *text);
int RegkeyRdata(int key, const char *valname, void *mem1, dword len1);
int RegkeyDataSize(int key, const char *valname);
bool RegkeyDelval(int key, const char *valname);
bool RegkeyDelkey(int key, const char *valname);

//----------------------------
#endif
