#include "pch.h"

//----------------------------

bool SCGetPath(C_str &vss_exe, C_str &vss_database){

   char name[256];
                           //get path to source safe
   int hk = RegkeyOpen("Software\\Microsoft\\SourceSafe");
   if(hk==-1)
      return false;
   int len = RegkeyRdata(hk, "SCCServerPath", name, sizeof(name));
   if(len==-1){
      RegkeyClose(hk);
      return false;
   }
   while(len && name[len-1]!='\\') --len;
   strcpy(&name[len], "ss.exe");
   vss_exe = name;

   len = RegkeyRdata(hk, "Current Database", name, sizeof(name));
   RegkeyClose(hk);
   if(len==-1)
      return false;
   vss_database = name;

   return true;
}

//----------------------------

static bool SCCommand(const char *in_cmdline){

   C_str vss_exe, vss_data;
   if(!SCGetPath(vss_exe, vss_data))
      return false;

   C_fstr cmdline("\"%s\" %s", (const char*)vss_exe, (const char*)in_cmdline);

   PROCESS_INFORMATION pi;
   memset(&pi, 0, sizeof(pi));

   STARTUPINFO si;
   memset(&si, 0, sizeof(si));
   si.cb = sizeof(si);
   si.dwFlags = STARTF_USESHOWWINDOW;// | STARTF_USESTDHANDLES;
   si.wShowWindow = SW_HIDE;

   SECURITY_ATTRIBUTES lsa;
   lsa.nLength = sizeof(lsa);
   lsa.lpSecurityDescriptor = NULL;
   lsa.bInheritHandle = true;

#if defined _DEBUG && 0
   AllocConsole();
   CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
#endif

   char env[1024];
   int len = 0;

   bool user_ok = GetEnvironmentVariable("SSUSER", env, sizeof(env));
   if(!user_ok){
                              //try use current user name
      dword name_size = sizeof(env);
      user_ok = GetUserName(env, &name_size);
      if(!user_ok)
         return false;
   }

   if(env[0]){
      C_str user = env;
      strcpy(env+len, "SSUSER=");
      strcat(env+len, user);
      len += strlen(env+len) + 1;
   }

   strcpy(env+len, "SSDIR=");
   strcat(env+len, vss_data);
   len += strlen(env+len) + 1;
   env[len] = 0;
   bool ok = CreateProcess(vss_exe,
      (char*)(const char*)cmdline,
      &lsa,                //process security attributes
      &lsa,                //thread security attributes
      true,                //handle inheritance flag
      0,                   //creation flags
      (void*)(const char*)env,
      NULL,                //pointer to current directory name
      &si,
      &pi);
   if(!ok)
      return false;

   dword wr = WaitForSingleObject(pi.hProcess, 30000);
   switch(wr){
   case WAIT_TIMEOUT:
      return false;
   default:
      return false;
   case WAIT_OBJECT_0: break;
   }
   dword cr;
   bool b = GetExitCodeProcess(pi.hProcess, &cr);
   assert(b && cr!=STILL_ACTIVE);
   b = CloseHandle(pi.hProcess);
   assert(b);
   b = CloseHandle(pi.hThread);
   assert(b);
   return (cr==0);
}

//----------------------------

bool SCGetFile(const char *project, const char *filename){

   C_str filepath = filename;
   for(dword i=filepath.Size(); i--; ){
      if(filepath[i]=='\\'){
         filepath[i] = 0;
         break;
      }
   }
   C_fstr cmdline("Get \"$\\%s\\%s\" -GKA -GL\"%s\"", project, filename, (const char*)filepath);
   return SCCommand(cmdline);
}

//----------------------------

bool SCCheckOutFile(const char *project, const char *filename){

   C_str filepath = filename;
   for(dword i=filepath.Size(); i--; ){
      if(filepath[i]=='\\'){
         filepath[i] = 0;
         break;
      }
   }
   C_fstr cmdline("Checkout \"$\\%s\\%s\" -GKA -C- -GL\"%s\"", project, filename, (const char*)filepath);
   return SCCommand(cmdline);
}

//----------------------------

bool SCCheckInFile(const char *project, const char *filename, bool keep_checked_out){

   C_str filepath = filename;
   for(dword i=filepath.Size(); i--; ){
      if(filepath[i]=='\\'){
         filepath[i] = 0;
         break;
      }
   }
   C_fstr cmdline("Checkin \"$\\%s\\%s\" -GKA -C- -%s -GL\"%s\"",
      project, filename,
      keep_checked_out ? "K" : "K-",
      (const char*)filepath);
   return SCCommand(cmdline);
}

//----------------------------
//----------------------------
