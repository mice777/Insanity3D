/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games

   File: Main.cpp
   Content: Insanity 3D DLL load/unload processing.
--------------------------------------------------------*/

#include "all.h"

//----------------------------

#pragma comment(lib, "advapi32.lib")   //due to D3DX library

void SetBreakAlloc(dword);

//----------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL, dword fdwReason, void *lpvReserved){

   switch(fdwReason){
   case DLL_THREAD_ATTACH:
      break;

   case DLL_THREAD_DETACH:
      break;

   case DLL_PROCESS_ATTACH:
      {
                              //this func switches to MSVC mem allocation, instead of system allocation
         _set_sbh_threshold(1016);
         //SetBreakAlloc(13491);
      }
      break;

   case DLL_PROCESS_DETACH:
#ifdef _DEBUG
      OutputDebugString("I3D2 DLL has been unloaded\n");
#endif
      break;
   }
   return true;
}

//----------------------------

