#define NO_STARTUP
#include "../all.h"
#include <igraph2.h>

#pragma comment (lib, "dta_read")

static const char *lib_name[2] = {
   "igraph2.dll",
   "igraph2d.dll"
};

typedef PIGraph __stdcall t_IGraphCreate(PIG_INIT, HINSTANCE);

//----------------------------

PIGraph I2DAPI IGraphCreate(PIG_INIT init_data, bool use_debug_version){

   const char *dll_filename = lib_name[use_debug_version];

   HINSTANCE hi = LoadLibrary(dll_filename);
   if(!hi)
      return NULL;

   FARPROC fp = GetProcAddress(hi, "_IGraphCreate@8");
   if(!fp)
      return NULL;
   t_IGraphCreate *p_Create = (t_IGraphCreate*)fp;

   return (*p_Create)(init_data, hi);
}

//----------------------------