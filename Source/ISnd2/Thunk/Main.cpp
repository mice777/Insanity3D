#include "../all.h"
#include <ISound2.h>



static const char *lib_name[2] = {
   "isound2.dll",
   "isound2d.dll"
};

typedef PISND_driver t_Create(PISND_INIT);

//----------------------------

PISND_driver ISNDAPI ISoundCreate(PISND_INIT init_data, bool use_debug_version){

   const char *dll_filename = lib_name[use_debug_version];

   HINSTANCE hi = LoadLibrary(dll_filename);
   if(!hi)
      return NULL;

   FARPROC fp = GetProcAddress(hi, "ISoundCreate");
   if(!fp)
      return NULL;
   t_Create *p_Create = (t_Create*)fp;
   return (*p_Create)(init_data);
}

//----------------------------