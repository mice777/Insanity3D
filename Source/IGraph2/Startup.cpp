#include "all.h"
#include <windows.h>
#include <rules.h>
#include <malloc.h>
#include <new.h>
#include <c_except.h>
#include <math.h>
#include <insanity\os.h>

//----------------------------

                              //external reference, for linking this file into executable
#ifdef _MSC_VER
extern"C" extern byte _startup;
byte _startup;

#else

byte __startup;

#endif

//----------------------------

int __cdecl NewHandler(size_t sz){
   throw C_except_mem(sz);
}

//----------------------------

/*
int __cdecl _matherr(struct _exception *err_info){

   const char *cp;
   switch(err_info->type){
   case DOMAIN:    cp="domain";    break;
   case SING:      cp="sing";      break;
   case OVERFLOW:  cp="owerflow";  break;
   case UNDERFLOW: cp="underflow"; break;
   case TLOSS:     cp="tloss";     break;
   case PLOSS:     cp="ploss";     break;
   default:        cp="unknown";
   }
   throw C_except(C_fstr("%s error in %s [%f] [%f]", cp, err_info->name, err_info->arg1, err_info->arg2));
};
*/

//----------------------------
                              //program entry point
int Main(void *hinst, const char *cmd_line);

//----------------------------
                              //windows main function
int __stdcall WinMain(HINSTANCE hi, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){

   _set_printf_count_output(1);
   {
                              //set current directory to the location of executable
      char *buf = (char*)malloc(MAX_PATH);
      int i = GetModuleFileName(hi, buf, MAX_PATH);
      while(i && buf[i-1]!='\\') --i;
      buf[i] = 0;
      SetCurrentDirectory(buf);
      free(buf);
   }
   _PNH save_handler = _set_new_handler(NewHandler);

   int ret = 0;
   try{
      ret = Main(hi, lpCmdLine);
   }catch(const C_except &e){
      //e.ShowCallStackDialog();
      OsMessageBox(NULL, e.what(), "Exception", MBOX_OK);
   }
   _set_new_handler(save_handler);
   return ret;
}

//----------------------------


