//----------------------------
// Copyright (c) 2002 Lonely Cat Games
// File: Main.cpp
// Content: Insanity DLL load/unload processing.
//----------------------------
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX          //Macros min(a,b) and max(a,b)
#include <windows.h>
#include <rules.h>
#include <malloc.h>

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
      }
      break;

   case DLL_PROCESS_DETACH:
      break;
   }
   return true;
}

//----------------------------
