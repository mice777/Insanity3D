/*--------------------------------------------------------
   Copyright (c) 1999, 2000 Michal Bacik. 
   All rights reserved.

   File: D3D_err.cpp
   Content: D3D error-reporting functions.
--------------------------------------------------------*/
#include "pch.h"
#include <dxerr9.h>
#include <Windows.h>

//----------------------------

void D3D_Fatal(const char *text, dword hr, const char *file, dword line){
   DXTrace((char*)file, line, hr, (char*)text, true);
}

//#ifdef GL

void GL_Fatal(const char *text, dword code, const char *file, dword line){
   C_fstr s("%s \nCode: %i\nFile: %s\nLine:%i", text, code, file, line);
   MessageBox(NULL, s, "Gl error", MB_OK);
   exit(0);
}

//#endif
//----------------------------
