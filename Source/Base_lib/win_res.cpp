#include <windows.h>
#include <rules.h>
#include <C_cache.h>
#include <Win_Res.h>



//----------------------------

bool OpenResource(void *hinstance, const char *res_group, const char *res_name, C_cache &ck){

   HINSTANCE hi = (HINSTANCE)hinstance;
   if(!hi)
      hi = GetModuleHandle(NULL);
   HRSRC hrsrc = FindResource(hi, res_name, res_group);
   if(!hrsrc)
      return false;
   HGLOBAL hgl = LoadResource(hi, hrsrc);
   if(!hgl)
      return false;
   const void *data = LockResource(hgl);
   if(!data)
      return false;
   dword sz = SizeofResource(hi, hrsrc);
   return ck.open((byte**)&data, &sz, CACHE_READ_MEM);
}

//----------------------------
