//#include "../all.h"
#include <windows.h>
#define __IGRAPH2_H
#include <i3d\i3d2.h>



static const char *lib_name[2] = {
   "i3d2.dll",
   "i3d2_d.dll"
};

typedef I3D_RESULT I3DAPI __stdcall t_I3DCreate(PI3D_driver*, CPI3DINIT, const char*);

//----------------------------

I3D_RESULT I3DAPI I3DCreate(PI3D_driver *drvp, CPI3DINIT id, bool debug){

   const char *dll_filename = lib_name[debug];

   HINSTANCE hi = LoadLibrary(dll_filename);
   if(!hi)
      return I3DERR_GENERIC;

   FARPROC fp = GetProcAddress(hi, "_I3DCreate_internal@12");
   if(!fp)
      return I3DERR_GENERIC;
   t_I3DCreate *p_I3DCreate = (t_I3DCreate*)fp;

   return (*p_I3DCreate)(drvp, id, dll_filename);
}

//----------------------------