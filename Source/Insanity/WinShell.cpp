#include "pch.h"

#pragma comment(lib, "shell32")

//----------------------------

bool OsShell(const char *cmd){

   HINSTANCE hi =
      ShellExecute(NULL, NULL, cmd, NULL, NULL, SW_SHOWDEFAULT);
   return (((dword)hi) > 32);   
}

//----------------------------
