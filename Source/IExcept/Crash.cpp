#include <windows.h>
#include <C_str.hpp>
#include <tlhelp32.h>
#include <iexcpt.h>
#pragma warning(push, 3)
#include <vector>
#include <list>
#include <algorithm>
#pragma warning(pop)
#define DTA_READ_STATIC
#include <dta_read.h>
#include <process.h>
#include <cfloat>
#include <imagehlp.h>
#include <direct.h>
#include <Insanity\assert.h>
#include "resource.h"
#include <insanity\os.h>

using namespace std;

//#pragma comment(lib, "th32")

static const char *dll_name = "iexcpt.dll";   //name of this DLL for resource location

#define MAX_RECURSE 30         //maximal number of function to recurse up the stack

//#define USE_WIN_STACKWALK     //use StackWalk from Win32 API

#ifdef _DEBUG
//#define DEBUG_CRASH_TEST
#endif

static int nest_count = 0;

//----------------------------

static HINSTANCE GetHInstance(){
   return GetModuleHandle(dll_name);
}

//----------------------------

                              //network address is in resources
static bool (__stdcall *send_function)(const char*);
static void (__stdcall *callback_function)();

//----------------------------

struct S_dlg_init{
   bool allow_close;
   bool allow_ignore;
   const char *log_dump;
   const char *window_title;
};

//----------------------------

static BOOL CALLBACK dlgCrash(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         const S_dlg_init *di = (S_dlg_init*)lParam;
         RECT rc;
         GetWindowRect(hwnd, &rc);
         int x = (GetSystemMetrics(SM_CXSCREEN)-(rc.right-rc.left))/2;
         int y = (GetSystemMetrics(SM_CYSCREEN)-(rc.bottom-rc.top))/2;
         SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOSIZE);

         EnableWindow(GetDlgItem(hwnd, IDCLOSE), di->allow_close);
         EnableWindow(GetDlgItem(hwnd, IDC_SEND), (send_function!=NULL));
         EnableWindow(GetDlgItem(hwnd, IDC_IGNORE), di->allow_ignore);

         SetWindowText(hwnd, di->window_title);

         SetDlgItemText(hwnd, IDC_EDIT, di->log_dump);
         ShowWindow(hwnd, SW_SHOW);
         return true;
      }
      break;
   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDC_SEND:
      case IDC_SAVE_TO_FILE:
      case IDC_DEBUG:
      case IDCLOSE:
      case IDCANCEL:
      case IDC_IGNORE:
         EndDialog(hwnd, LOWORD(wParam));
         break;
      }
      break;
   }
   return 0;
}

//----------------------------

                              //info about single module
struct S_module_info{
   C_str mod_name;         //name of module (exe|dll)
   C_str exe_path;         //full path to module
   byte *base_addr;           //base address
   dword mod_size;            //size
   //dword file_time[2];
   //void *entry_point;         //entry-point address (valid only for executables)
   bool is_exe;
                              //single function info
   struct S_function{
      C_str name;          //function name
      byte *addr;             //function address (relocated)
      inline bool operator < (const S_function &f) const{
         return (addr<f.addr);
      }
   };
   //mutable vector<S_function> func_list;  //list of all functions
   mutable list<S_function> func_list;  //list of all functions
   mutable bool db_info_searched;         //flag set if this module has searched for info

   S_module_info():
      db_info_searched(false),
      is_exe(false)
      //entry_point(NULL)
   {}

//----------------------------

   static BOOL CALLBACK SymEnumSymbolsCallback(PCSTR SymbolName, ULONG SymbolAddress, ULONG SymbolSize, PVOID UserContext){

      list<S_function> &func_list = *(list<S_function>*)UserContext;
      func_list.push_back(S_function());
      S_function &fnc_info = func_list.back();
      fnc_info.name = SymbolName;
      fnc_info.addr = (byte*)SymbolAddress;
      //OutputDebugString(C_fstr("%s\n", SymbolName));
      return true;
   }

//----------------------------

   bool OpenDebugInfo() const{

      {
         C_str debug_info_path;
         {
            C_dta_stream *dta = DtaCreateStream(exe_path);
            if(dta){
               IMAGE_DOS_HEADER dh;
               if(dta->Read(&dh, sizeof(dh)) == sizeof(dh)){
                  dword seek = dh.e_lfanew + sizeof(dword);
                  dta->Seek(seek, DTA_SEEK_SET);
                  IMAGE_FILE_HEADER img_hdr;
                  if(dta->Read(&img_hdr, sizeof(img_hdr)) == sizeof(img_hdr)){
                     IMAGE_OPTIONAL_HEADER oh;
                     if(dta->Read(&oh, sizeof(oh)) == sizeof(oh)){
                        IMAGE_SECTION_HEADER sct_hdr;
                        dword last_section_end = 0;
                        bool has_debug_section = false;
                        for(dword si=0; si<img_hdr.NumberOfSections; si++){
                           if(dta->Read(&sct_hdr, sizeof(sct_hdr)) == sizeof(sct_hdr)){
                              dword sect_end = sct_hdr.PointerToRawData + sct_hdr.SizeOfRawData;
                              last_section_end = Max(last_section_end, sect_end);
                              if(!strcmp((const char*)sct_hdr.Name, ".debug"))
                                 has_debug_section = true;
                           }
                        }
                        if(!has_debug_section){
                           dta->Seek(last_section_end, DTA_SEEK_SET);
                              //check NB10 debug info
                           dword nb_hdr[4];
                           if(dta->Read(nb_hdr, sizeof(nb_hdr)) == sizeof(nb_hdr) && nb_hdr[0]=='01BN'){
                                 //skip NB10 debug info
                              char buf[512];
                              for(dword i=0; i<sizeof(buf)-1; i++){
                                 if(!dta->Read(&buf[i], sizeof(char)))
                                    break;
                                 if(!buf[i])
                                    break;
                              }
                              buf[i] = 0;
                                 //now strip actual debug info filename
                              while(i--){
                                 if(buf[i]=='\\'){
                                    buf[i] = 0;
                                    debug_info_path = buf;
                                    break;
                                 }
                              }
                           }
                        }
                     }
                  }
               }
               dta->Release();
            }
         }
         HANDLE hproc = GetCurrentProcess();
         SymSetOptions(SYMOPT_UNDNAME);

         {
                              //try again in executable's dir
            C_str s = exe_path;
            for(dword i=s.Size(); i--; ){
               if(s[i]=='\\'){
                  s[i] = 0;
                  debug_info_path = C_fstr("%s;%s", (const char*)debug_info_path, (const char*)s);
                  break;
               }
            }
         }
         bool ok;
         ok = SymInitialize(hproc, !debug_info_path.Size() ? NULL : (char*)(const char*)debug_info_path, false);
         if(ok){
            ok = SymLoadModule(hproc, NULL, (char*)(const char*)exe_path, NULL, (dword)base_addr, mod_size);
            if(ok){
               ok = SymEnumerateSymbols(hproc, (dword)base_addr, SymEnumSymbolsCallback, &func_list);
            }
            SymUnloadModule(hproc, (dword)base_addr);
            SymCleanup(hproc);
            func_list.sort();
            return ok;
         }
      }
      /*
      dword i;

      char *buf = NULL;
      {
                              //parse map file
         char fname[256];

         if(!buf){
            strcpy(fname, exe_path);
            for(i=exe_path.Size(); i--; ){
               if(fname[i]=='.')
                  break;
            }
            if(i)
               strcpy(&fname[i], ".map");

            char cwd[512];
            _getcwd(cwd, sizeof(cwd));

            i = (dword)-1;
            if(strlen(cwd) <= strlen(fname)){
               if(!strnicmp(cwd, fname, strlen(cwd))){
                  i = strlen(cwd);
                  if(fname[i]=='\\')
                     ++i;
               }
            }

            if(i!=-1){
               int h = dtaOpen(&fname[i]);
               if(h!=-1){
                  int sz = dtaSeek(h, 0, DTA_SEEK_END);
                  dtaSeek(h, 0, DTA_SEEK_SET);
                  buf = new char[sz+1];
                  dtaRead(h, buf, sz);
                  buf[sz] = 0;
                  dtaClose(h);
               }
            }
         }
                              //if failed, try full path
         if(!buf){
            int h = dtaOpen(fname);
            if(h!=-1){
               int sz = dtaSeek(h, 0, DTA_SEEK_END);
               dtaSeek(h, 0, DTA_SEEK_SET);
               buf = new char[sz+1];
               dtaRead(h, buf, sz);
               buf[sz] = 0;
               dtaClose(h);
            }
         }
      }
      if(!buf){
         HANDLE hproc = GetCurrentProcess();
         bool ok = SymInitialize(hproc, NULL, false);
         if(ok){
            SymSetOptions(SYMOPT_DEFERRED_LOADS);
            ok = SymLoadModule(hproc, NULL, (char*)(const char*)exe_path, NULL, (dword)base_addr, mod_size);
            if(ok){
               ok = SymEnumerateSymbols(hproc, (dword)base_addr, SymEnumSymbolsCallback, &func_list);
            }
            SymUnloadModule(hproc, (dword)base_addr);
            SymCleanup(hproc);
         }
         return ok;
      }

                           //file found
      enum E_INFO{
         INFO_MODULE_NAME,
         INFO_TIMESTAMP,
         INFO_LOAD_ADDRESS,
         INFO_ADDRESS,
         INFO_REST,
      } next_info = INFO_MODULE_NAME;
      int reloc_delta = 0;

      char *next_line = buf;

      bool result = true;
      for(;;){
         if(!*next_line)
            break;
         char *line = next_line;
         next_line = strchr(line, '\n');
         if(!next_line)
            next_line = line + strlen(line);
         else{
            if(next_line!=line && next_line[-1]=='\r')
               next_line[-1] = 0;
            *next_line++ = 0;
         }
         while(*line && isspace(*line))
            ++line;
         if(!*line)
            continue;

         switch(next_info){
         case INFO_MODULE_NAME:
            {
               C_str mname = mod_name;
                              //check if module name matches this string
               for(i=mname.Size(); i--; ){
                  if(mname[i]=='.'){
                     mname[i] = 0;
                     mname = (const char*)mname;
                     break;
                  }
               }
               C_str str = line;
               str.ToLower();
               if(!(mname==str)){
                              //module name mismatch?
                  result = false;
                  goto finish;
               }
               next_info = INFO_TIMESTAMP;
            }
            break;

         case INFO_TIMESTAMP:
            {
#if 0
               dword timestamp;
               char dayname[12], mnt[12];
               int year, day, hr, min, sec;
               int num = sscanf(str, "Timestamp is %x (%s %s %i %i:%i:%i %i)", &timestamp, &dayname, &mnt, &day, &hr, &min, &sec, &year);
               if(num!=8){
                  result = false;
                  goto finish;
               }
               static const char *month_names[] = {
                  "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"
               };
               for(int i=12; i--; ){
                  if(!stricmp(month_names[i], mnt))
                     break;
               }
               if(i!=-1){
                              //check date
                  FILETIME ft;
                  SYSTEMTIME st;
                  st.wYear = year;
                  st.wMonth = i+1;
                  st.wDayOfWeek = 0;
                  st.wDay = day;
                  st.wHour = hr;
                  st.wMinute = min;
                  st.wSecond = sec;
                  st.wMilliseconds = 0;
                  SystemTimeToFileTime(&st, &ft);

                  __int64 time_delta = (*(__int64*)&ft) - (*(__int64*)file_time);
                  if(time_delta < __int64(0x4000000) && time_delta>-__int64(0x4000000)){
                     next_info = INFO_LOAD_ADDRESS;
                     break;
                  }
               }
               result = false;
               goto finish;
#endif
               next_info = INFO_LOAD_ADDRESS;
            }
            break;

         case INFO_LOAD_ADDRESS:
            {
               static const char search_text[] = "Preferred load address is";
               if(!strnicmp(line, search_text, sizeof(search_text)-1)){
                              //read relocation base
                  line += sizeof(search_text) - 1;

                  byte *reloc_base;
                  sscanf(line, "%x", &reloc_base);
                  reloc_delta = base_addr - reloc_base;
                  next_info = INFO_ADDRESS;
                  break;
               }
               result = false;
               goto finish;
            }
            break;

         case INFO_ADDRESS:
            if(!strnicmp(line, "Address", 7)){
                              //found what we want, skip 1 more line
               next_info = INFO_REST;
            }
            break;

         case INFO_REST:
                              //parse data
            {
                              //scan address and function name
               byte *seg, *offs;
               int num = sscanf(line, "%x:%x", &seg, &offs);
               if(num!=2)
                  break;
               func_list.push_back(S_function());
               S_function &fnc_info = func_list.back();
               const int FIRST_CHAR = 20;
               int line_len = next_line - line;
               if(line_len > FIRST_CHAR){
                  int i, j = -1;
                  for(i=FIRST_CHAR; line[i]; i++)
                  if(line[i]==' '){
                     line[i] = 0;
                     j = i+1;
                     while(line[j] && isspace(line[j]))
                        ++j;
                     sscanf(&line[j], "%x", &offs);
                     while(line[j] && !isspace(line[j])) ++j;
                     while(line[j] && isspace(line[j])) ++j;
                     while(line[j] && !isspace(line[j])) ++j;
                     while(line[j] && isspace(line[j])) ++j;
                     break;
                  }
                  if(j==-1)
                     fnc_info.name = &line[FIRST_CHAR];
                  else{
                     fnc_info.name = &line[FIRST_CHAR];
                  }
               }
               fnc_info.addr = offs + reloc_delta;
            }
            break;
         }
      }
   finish:
      delete[] buf;
      func_list.sort();
      return result;
      */
      return false;
   }
                              //check if address is inside of this module address space
   inline bool ContainAddress(void *addr) const{

      byte *bp = (byte*)addr;
      return (bp>=base_addr && bp<base_addr+mod_size);
   }

//----------------------------
                              //prepare formatted info for single address
   void *FormatInfo(void *addr, C_str &str/*, bool *is_main*/) const{

      if(!db_info_searched){
         db_info_searched = true;
         OpenDebugInfo();
      }

      byte *bp = (byte*)addr;
      //*is_main = false;
                              //find which function it may be
      for(list<S_function>::reverse_iterator it=func_list.rbegin(); it!=func_list.rend(); it++){
         const S_function &fi = (*it);
         if(bp>=fi.addr){
            const C_str &fnc_name = fi.name;
            char un_name[4096];
            UnDecorateSymbolName(fnc_name, un_name, sizeof(un_name),
               UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | 
               //UNDNAME_NO_THISTYPE
               UNDNAME_NO_ACCESS_SPECIFIERS |
               UNDNAME_NO_MEMBER_TYPE
               );

            str = C_fstr("%s, offset 0x%x", un_name, bp-fi.addr);
            /*
            if(fnc_name.Matchi("_main*")) *is_main = true;
            else
            if(fnc_name.Matchi("main")) *is_main = true;
            else
            if(fnc_name.Matchi("?Main*")) *is_main = true;
            else
            if(fnc_name.Matchi("_WinMain*"))
               *is_main = true;
            */
            return fi.addr;
         }
      }
      str = "<unknown function>";
      return NULL;
   }
};

                              //make it static, so that we don't re-build it for each call to report things
                              // (it may be also made local, but for speed reasons of exception dialog, it's here)
static vector<S_module_info> mod_info;

//----------------------------
                              //build list of modules
static void BuildModuleInfo(vector<S_module_info> &mod_info){

   HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);

   MODULEENTRY32 m32;
   m32.dwSize = sizeof(m32);
   if(Module32First(h, &m32)){
      do{
                              //check if already scanned
         for(int i=mod_info.size(); i--; ){
            if(mod_info[i].base_addr == (byte*)m32.hModule)
               break;
         }
         if(i!=-1)
            continue;

         mod_info.push_back(S_module_info());
         S_module_info &mi = mod_info.back();
         mi.mod_name = m32.szModule;
         mi.mod_name.ToLower();
         mi.exe_path = m32.szExePath;
         mi.exe_path.ToLower();
         mi.base_addr = (byte*)m32.hModule;
         mi.mod_size = m32.modBaseSize;
         /*
         memset(mi.file_time, 0, sizeof(mi.file_time));
         HANDLE h = CreateFile(mi.exe_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
         if(h!=INVALID_HANDLE_VALUE){
            FILETIME ft;
            GetFileTime(h, NULL, NULL, &ft);
            FileTimeToLocalFileTime(&ft, (LPFILETIME)mi.file_time);
            CloseHandle(h);
         }
         */
                              //if exe, save entry point (main)
         if(mi.mod_name.Size()>=4 && !stricmp(&mi.mod_name[mi.mod_name.Size()-4], ".exe")){
            mi.is_exe = true;
            /*
            C_dta_stream *dta = DtaCreateStream(mi.exe_path);
            if(dta){
               IMAGE_DOS_HEADER dh;
               if(dta->Read(&dh, sizeof(dh)) == sizeof(dh)){
                  dword seek = dh.e_lfanew + sizeof(dword);
                  dta->Seek(seek, DTA_SEEK_SET);
                  IMAGE_FILE_HEADER img_hdr;
                  if(dta->Read(&img_hdr, sizeof(img_hdr)) == sizeof(img_hdr)){
                     IMAGE_OPTIONAL_HEADER oh;
                     if(dta->Read(&oh, sizeof(oh)) == sizeof(oh)){
                        mi.entry_point = (void*)oh.AddressOfEntryPoint;
                     }
                  }
               }
            }
            */
         }

      } while(Module32Next(h, &m32));
   }
}

//----------------------------
                              //find module in which given address is contained
static const S_module_info *FindModule(void *addr, const vector<S_module_info> &mod_info){

   const S_module_info *mods = &mod_info.front();
   for(dword i=mod_info.size(); i--; ){
      const S_module_info *mi = mods++;
      if(mi->ContainAddress(addr))
         return mi;
   }
   return NULL;
}

//----------------------------

static const struct{
   dword id;
   const char *name;
} except_code[] = {
   0, "unknown exception",
   EXCEPTION_ACCESS_VIOLATION, "ACCESS_VIOLATION",
   EXCEPTION_IN_PAGE_ERROR, "IN_PAGE_ERROR",
   EXCEPTION_INVALID_HANDLE, "INVALID_HANDLE",
   EXCEPTION_ILLEGAL_INSTRUCTION, "ILLEGAL_INSTRUCTION",
   EXCEPTION_NONCONTINUABLE_EXCEPTION, "NONCONTINUABLE_EXCEPTION",
   EXCEPTION_INVALID_DISPOSITION, "INVALID_DISPOSITION",
   EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "ARRAY_BOUNDS_EXCEEDED",
   EXCEPTION_FLT_DENORMAL_OPERAND, "FLT_DENORMAL_OPERAND",
   EXCEPTION_FLT_DIVIDE_BY_ZERO, "FLOAT_DIVIDE_BY_ZERO",
   EXCEPTION_FLT_INEXACT_RESULT, "FLOAT_INEXACT_RESULT",
   EXCEPTION_FLT_INVALID_OPERATION, "FLOAT_INVALID_OPERATION",
   EXCEPTION_FLT_OVERFLOW, "FLOAT_OVERFLOW",
   EXCEPTION_FLT_STACK_CHECK, "FLOAT_STACK_CHECK",
   EXCEPTION_FLT_UNDERFLOW, "FLOAT_UNDERFLOW",
   EXCEPTION_INT_DIVIDE_BY_ZERO, "INTEGER_DIVIDE_BY_ZERO",
   EXCEPTION_INT_OVERFLOW, "INTEGER_OVERFLOW",
   EXCEPTION_PRIV_INSTRUCTION, "PRIVILEGED_INSTRUCTION",
   EXCEPTION_STACK_OVERFLOW, "STACK_OVERFLOW",
   EXCEPTION_BREAKPOINT, "BREAKPOINT",
   (dword)-1
};

//----------------------------

static dword GetExcCodeIndex(dword id){
   dword code_indx = 0;
   do{
      ++code_indx;
      if(except_code[code_indx].id==-1){
         code_indx = 0;       //set unknown
         break;
      }
   } while(except_code[code_indx].id != id);
   return code_indx;
}

//----------------------------

void GetCallStack(LPEXCEPTION_POINTERS ep, t_call_stack &call_stack){

   ++nest_count;

   bool modules_built = false;

   if(!mod_info.size()){
      BuildModuleInfo(mod_info);
      modules_built = true;
   }

   CONTEXT *cp = ep->ContextRecord;
#ifndef USE_WIN_STACKWALK
   dword *reg_ebp = (dword*)cp->Ebp;
#endif
   dword *reg_esp = (dword*)cp->Esp;

   C_str str_path;

   void *fnc_address = ep->ExceptionRecord->ExceptionAddress;

   HANDLE h_process = GetCurrentProcess();
#ifdef USE_WIN_STACKWALK
                              //structure for stack walking
   STACKFRAME sf;
   //memset(&sf, 0, sizeof(sf));

   sf.AddrPC.Offset = (dword)ep->ExceptionRecord->ExceptionAddress;
   sf.AddrPC.Segment = 0;
   sf.AddrPC.Mode = AddrModeFlat;

   sf.AddrFrame.Offset = cp->Ebp;
   sf.AddrFrame.Segment = 0;
   sf.AddrFrame.Mode = AddrModeFlat;

   sf.AddrStack.Offset = cp->Esp;
   sf.AddrStack.Segment = 0;
   sf.AddrStack.Mode = AddrModeFlat;

   sf.AddrReturn.Mode = AddrModeFlat;

   sf.FuncTableEntry = NULL;
   sf.Params[0] = sf.Params[1] = sf.Params[2] = sf.Params[3] = NULL;
   sf.Far = false;
   sf.Virtual = false;
   memset(&sf.Reserved, 0, sizeof(sf)-offsetof(STACKFRAME, Reserved));

   HANDLE h_thread = GetCurrentThread();
#endif

   vector<void*> _call_stack;
   _call_stack.reserve(MAX_RECURSE);

   bool was_in_exe = false;

   for(int k=0; k<MAX_RECURSE; k++){
      if(fnc_address){
         const S_module_info *mi = FindModule(fnc_address, mod_info);
         if(!mi && !modules_built){
                              //check if valid pointer
            char buf;
            if(ReadProcessMemory(h_process, fnc_address, &buf, sizeof(char), NULL)){
                              //re-build module info again
               BuildModuleInfo(mod_info);
               modules_built = true;
               mi = FindModule(fnc_address, mod_info);
            }
         }
         if(!mi){
            if(k) break;
                              //it may be called virtual function on destroyed interface
                              //try function directly on stack
            fnc_address = (void*)reg_esp[0];
            mi = FindModule(fnc_address, mod_info);
            if(!mi)
               break;
         }
         bool is_in_exe = mi->is_exe;
         if(was_in_exe && !is_in_exe)
            break;
         was_in_exe = is_in_exe;

         _call_stack.push_back(fnc_address);
      }

#ifdef USE_WIN_STACKWALK
      if(!StackWalk(IMAGE_FILE_MACHINE_I386, h_process, h_thread, &sf, NULL, NULL,
         SymFunctionTableAccess, SymGetModuleBase, NULL))
         break;
      if(!sf.AddrFrame.Offset)
         break;
      fnc_address = (void*)sf.AddrReturn.Offset;
#else
      if(!reg_ebp)
         break;
                              //unwind:
                              // - caller's address at stack+4
                              // - caller's ebp at stack+0
      if(!ReadProcessMemory(h_process, reg_ebp+1, &fnc_address, sizeof(void*), NULL))
         break;
      /*
      __asm{
         mov eax, reg_ebp
         mov eax, [eax+4]
         mov fnc_address, eax
      }
      */
      if(!fnc_address && reg_esp){
                              //give it a blind try on ESP
                              // (if stack frame is not used, this may find the ebp and address on the stack)
         for(int i=0; i<10; i++){
            __asm{
               mov edx, i
               shl edx, 2

               mov eax, reg_esp
               mov eax, [eax + edx]
               mov reg_ebp, eax

               mov eax, reg_esp
               mov eax, [eax+edx+4]
               mov fnc_address, eax
            }
            /*
            void *mem = ((byte*)reg_esp) + i*4;
            void *buf[2];
            if(!ReadProcessMemory(GetCurrentProcess(), mem, buf, sizeof(buf), NULL))
               continue;
            reg_ebp = (dword*)buf[0];
            fnc_address = (void*)buf[1];
            */

            if(reg_ebp && fnc_address && FindModule(fnc_address, mod_info))
               break;
         }
         if(i==10)
            break;
      }else{
         reg_ebp = (dword*)reg_ebp[0];
      }
#endif
   }
   call_stack.assign(&_call_stack.front(), (&_call_stack.back())+1);

   --nest_count;
}

//----------------------------

static BOOL CALLBACK dlgWait(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         RECT rc;
         GetWindowRect(hwnd, &rc);
         int x = (GetSystemMetrics(SM_CXSCREEN)-(rc.right-rc.left))/2;
         int y = (GetSystemMetrics(SM_CYSCREEN)-(rc.bottom-rc.top))/2;
         SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

         SetWindowLong(hwnd, GWL_USERDATA, 0);
         //SetTimer(hwnd, 1, 400, NULL);
      }
      return 1;

      /*
   case WM_TIMER:
      {
         dword i = GetWindowLong(hwnd, GWL_USERDATA);
         const dword MAX_I = 5;
         if(++i == 5)
            i = 0;
         char buf[MAX_I + 1];
         memset(buf, '.', i);
         buf[i] = 0;
         SetDlgItemText(hwnd, IDC_DOTS, buf);

         SetWindowLong(hwnd, GWL_USERDATA, i);
      }
      break;
      */
   }
   return 0;
}

//----------------------------

void FormatCrashInfo(const t_call_stack &call_stack, LPEXCEPTION_POINTERS ep, const char *user_message, char **log_dump){

   HWND hwnd = CreateDialog(GetHInstance(), "IDD_WAIT", NULL, dlgWait);
   UpdateWindow(hwnd);
   SetFocus(hwnd);

   const S_module_info *last_module = NULL;
   C_str str_path;

   bool modules_built = false;

   for(dword k=0; k<call_stack.size(); k++){
      void *fnc_address = call_stack[k];
      const S_module_info *mi = FindModule(fnc_address, mod_info);
      if(!mi){
                              //re-build module info again
         BuildModuleInfo(mod_info);
         modules_built = true;
         mi = FindModule(fnc_address, mod_info);
         assert(mi);
      }

      C_str fmt_text;   //formatted info line
      //bool is_main;
      mi->FormatInfo(fnc_address, fmt_text/*, &is_main*/);

      if(last_module!=mi){
                           //dump module name
         if(last_module){
            char *buf = new char[4096*MAX_RECURSE];
            sprintf(buf, "Module: %s\r\n%s", (const char*)last_module->mod_name, (const char*)str_path);
            str_path = buf;
            delete[] buf;
         }
         last_module = mi;
      }
                           //dump info line
      char *buf = new char[4096*MAX_RECURSE];
      sprintf(buf, "     %s\r\n%s", (const char*)fmt_text, (const char*)str_path);
      str_path = buf;
      delete[] buf;
      //if(is_main)
         //break;
   }

   if(last_module){
                              //dump module name
      char *buf = new char[4096*MAX_RECURSE];
      sprintf(buf, "Module: %s\r\n%s", (const char*)last_module->mod_name, (const char*)str_path);
      str_path = buf;
      delete[] buf;
   }

                              //get info about machine and time/date
   char cname[256];
   dword sz = 256;
   GetComputerName(cname, &sz);
   SYSTEMTIME st;
   GetLocalTime(&st);

   C_str code_name;

   if(!user_message && ep){
      CONTEXT *cp = ep->ContextRecord;
                              //get exception code name
      dword code_indx = 0;
      code_indx = GetExcCodeIndex(ep->ExceptionRecord->ExceptionCode);
      code_name = C_fstr(
         "Address: %x:%.8x, Exception code: %.8x (%s)\r\n"
         "Registers:\r\n"
         "eax = %.8x, "
         "ebx = %.8x, "
         "ecx = %.8x, "
         "edx = %.8x\r\n"
         "esi = %.8x, "
         "edi = %.8x, "
         "ebp = %.8x, "
         "esp = %.8x"
         ,
         cp->SegCs, cp->Eip, ep->ExceptionRecord->ExceptionCode, except_code[code_indx].name,
         cp->Eax,
         cp->Ebx,
         cp->Ecx,
         cp->Edx,
         cp->Esi,
         cp->Edi,
         cp->Ebp,
         cp->Esp
         );
   }else{
      code_name = user_message;
   }
   //SYSTEM_INFO si;
   //GetSystemInfo(&si);
   OSVERSIONINFO ver;
   ver.dwOSVersionInfoSize = sizeof(ver);
   GetVersionEx(&ver);
   MEMORYSTATUS mem;
   mem.dwLength = sizeof(mem);
   GlobalMemoryStatus(&mem);

   *log_dump = new char[4096 + 4096*MAX_RECURSE];
   sprintf(*log_dump,
      "Machine: '%s', OS ver: %i.%i (build %i) %s\r\n"
      "Memory: %i MB (used %i%%)\r\n"
      "Date: %i-%i-%i %i:%.2i\r\n"
      "%s\r\n"
      "\r\n"
      "Execution path:\r\n"
      "%s"
      ,
      cname, ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber&0xffff, ver.szCSDVersion,
      mem.dwTotalPhys / (1024*1024), mem.dwMemoryLoad,
      st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute,
      (const char*)code_name,
      (const char*)str_path
      );

   DestroyWindow(hwnd);
}

//----------------------------

static void BuildCrashInfo(LPEXCEPTION_POINTERS ep, const char *user_message, char **log_dump){

   //vector<S_module_info> mod_info;
   t_call_stack call_stack;
   GetCallStack(ep, call_stack);

   FormatCrashInfo(call_stack, ep, user_message, log_dump);
}

//----------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved){

   switch(fdwReason){
   case DLL_PROCESS_ATTACH:
      CoInitialize(NULL);
      break;
   case DLL_THREAD_ATTACH: break;
   case DLL_THREAD_DETACH: break;
   case DLL_PROCESS_DETACH:
      CoUninitialize();
      //mod_info.clear();
      break;
   }
   return true;
}
 
//----------------------------

EXCPT_RETURN_CODE DisplayDialog(const char *title, bool allow_close, bool allow_ignore, const char *log_dump){

                              //by default, handle the exception
   EXCPT_RETURN_CODE ret = EXCPT_CLOSE;
   bool loop = true;
   while(loop){
      HINSTANCE hinst = GetModuleHandle(dll_name);
      assert(hinst);

      S_dlg_init di = {
         allow_close, allow_ignore, log_dump, title ? title : "Program crash"
      };
      int i = DialogBoxParam(hinst, "IDD_CRASH", NULL, (DLGPROC)dlgCrash, (LPARAM)(const char*)&di);
      if(i==-1){
                              //dialog box failed, write log and try diplay it in notepad.
         const char *CRASH_FILENAME = "crash_failed.log";
         FILE *fh = fopen(CRASH_FILENAME, "wb");
         if(fh){
            fwrite(log_dump, 1, strlen(log_dump), fh);
            fclose(fh);
         }
         
         /*PROCESS_INFORMATION pi;
         memset(&pi, 0, sizeof(pi));

         STARTUPINFO si;
         memset(&si, 0, sizeof(si));
         si.cb = sizeof(si);
         si.dwFlags = STARTF_USESHOWWINDOW;// | STARTF_USESTDHANDLES;
         si.wShowWindow = SW_SHOW;

         SECURITY_ATTRIBUTES lsa;
         lsa.nLength = sizeof(lsa);
         lsa.lpSecurityDescriptor = NULL;
         lsa.bInheritHandle = true;

         char env[1024];

         char buf[1024];
         GetSystemDirectory(buf, 1024);
         C_str notepad_exe = C_fstr("%s\\notepad.exe",buf);
         GetCurrentDirectory(1024, buf);
         C_fstr cmdline(" %s\\%s", buf, CRASH_FILENAME);

         CreateProcess((const char *) notepad_exe,
            (char*)(const char*)cmdline,
            &lsa,                //process security attributes
            &lsa,                //thread security attributes
            true,                //handle inheritance flag
            0,                   //creation flags
            (void*)(const char*)env,
            NULL,                //pointer to current directory name
            &si,
            &pi);
         OsDisplayFile(CRASH_FILENAME);
         */
         break;
      }
      switch(i){
      case IDC_SEND:
         if(send_function){
            if(!(*send_function)(log_dump))
               MessageBox(NULL, "Failed to send crash report.", "Crash Report", MB_OK);
            else{
               loop = false;
               allow_close = true;
            }
         }
         break;

      case IDC_SAVE_TO_FILE:
         {
            char buf[256];
            buf[0] = 0;
            OPENFILENAME of;
            memset(&of, 0, sizeof(of));
            of.lStructSize = sizeof(of);
            of.hInstance = GetModuleHandle(NULL);
            of.lpstrFilter = "Crash report (*.err)\0*.err\0";
            of.lpstrFile = buf;
            of.nMaxFile = sizeof(buf);
            of.lpstrFileTitle = buf;
            of.nMaxFileTitle = sizeof(buf);
            of.lpstrDefExt = "err";
            of.lpstrTitle = "Save Crash Report";
            of.Flags = OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN;
            bool b = GetSaveFileName(&of);
            if(!b) break;

            FILE *fp = fopen(buf, "wb");
            if(fp){
               fwrite(log_dump, 1, strlen(log_dump), fp);
               fclose(fp);
               //loop = false;
            }else{
               MessageBox(NULL, "Save failed", "Crash Report", MB_OK);
               allow_close = true;
            }
         }
         break;

      case IDC_DEBUG:
         ret = EXCPT_DEBUG;
         loop = false;
         break;

      case IDC_IGNORE:
         ret = EXCPT_IGNORE;
         loop = false;
         break;

      default:
         loop = false;
      }
   }
   return ret;
}

//----------------------------

static EXCPT_RETURN_CODE ExceptHandle(LPEXCEPTION_POINTERS ep, const char *title, const char *user_message,
   bool allow_close, bool allow_ignore){

   if(callback_function/* && !user_message*/)
      (*callback_function)();

   char *log_dump;
   BuildCrashInfo(ep, user_message, &log_dump);
                              //fast log into a file
   if(!user_message){
      FILE *fh = fopen("crash.log", "wb");
      if(fh){
         fwrite(log_dump, 1, strlen(log_dump), fh);
         fclose(fh);
      }
   }

   EXCPT_RETURN_CODE ret = DisplayDialog(title, allow_close, allow_ignore, log_dump);
   delete[] log_dump;
   return ret;
}

//----------------------------

static long __stdcall ExceptHandle(LPEXCEPTION_POINTERS ep){

   //SetUnhandledExceptionFilter(NULL);
   //while(true);

   if(nest_count){            //this is our fault
#ifndef USE_WIN_STACKWALK
      CONTEXT *cp = ep->ContextRecord;
      if(*(byte*)cp->Eip == 0x8b){
         static int dummy[2];
                              //that's probably mov eax, [mem]
         cp->Eax = (dword)&dummy;
         return EXCEPTION_CONTINUE_EXECUTION;
      }
#endif
      return EXCEPTION_CONTINUE_SEARCH;
   }
   ++nest_count;

                              //disable leak warning
   HINSTANCE hi_dd = LoadLibrary("DebugMem.dll");
   if(hi_dd){
      void *fp = GetProcAddress(hi_dd, "_SetBreakAlloc");
      if(fp){
         typedef void (__cdecl t_fnc)(dword);
         t_fnc *fnc = (t_fnc*)fp;
         (*fnc)(dword(-2));
      }
      FreeLibrary(hi_dd);
   }

   const char *title = NULL;
   const char *msg = NULL;
   C_str tmp;
   bool allow_ignore = false;
   if(ep->ExceptionRecord->ExceptionCode==EXCEPTION_BREAKPOINT){
      dword *code = (dword*)ep->ContextRecord->Esp;
      if(code[0]==0x12345678){
         title = "Assertion failed";
         tmp = C_fstr("\r\nAssertion failed: %s\r\nfile: %s\r\nline: %i", (const char*)code[3], (const char*)code[1], code[2]);
         msg = tmp;
         allow_ignore = true;
                              //skip beyond the int 3 instruction
         ep->ContextRecord->Eip += 1;
         ep->ExceptionRecord->ExceptionAddress = (void*)ep->ContextRecord->Eip;
      }
   }
   EXCPT_RETURN_CODE ret = ExceptHandle(ep, title, msg, true, allow_ignore);
   --nest_count;

   switch(ret){
   case EXCPT_DEBUG:
      SetUnhandledExceptionFilter(NULL);
      return EXCEPTION_CONTINUE_SEARCH;
   case EXCPT_IGNORE:
      return EXCEPTION_CONTINUE_EXECUTION;
   }
   return EXCEPTION_EXECUTE_HANDLER;
}

//----------------------------

void __declspec(dllexport) __stdcall InitializeExceptions(dword fpu_cw){

   SetUnhandledExceptionFilter(ExceptHandle);
                              //setup FPU control word
   _control87(fpu_cw, _MCW_EM);

#ifdef DEBUG_CRASH_TEST
   *(char*)NULL = 0;
#endif

#if 0
#define __assert(exp) if(!(exp)){ const char *f = __FILE__, *e = #exp; int l = __LINE__; __asm push e __asm push l __asm push f __asm push 0x12345678 __asm nop }
   __assert(0);
                              //test it in debugger
   EXCEPTION_POINTERS ep;
   EXCEPTION_RECORD er;
   CONTEXT cr;
   memset(&ep, 0, sizeof(ep));
   memset(&er, 0, sizeof(er));
   memset(&cr, 0, sizeof(cr));
   ep.ExceptionRecord = &er;
   ep.ContextRecord = &cr;
   __asm{
      mov cr.Ebp, ebp
      call l1
   l1:
      pop cr.Eip
      mov cr.Esp, esp
   }
   ep.ExceptionRecord->ExceptionAddress = (void*)cr.Eip;
   ep.ExceptionRecord->ExceptionCode = 0x80000003;
   ExceptHandle(&ep, NULL, NULL, true, false);
#endif
}

//----------------------------

void __declspec(dllexport) __stdcall SetExceptionSendFunc(bool (__stdcall *send_fnc)(const char*)){

   send_function = send_fnc;
   //allow_close = (send_function==NULL);
}

//----------------------------

void __declspec(dllexport) __stdcall SetExceptionCallback(void (__stdcall *fnc)()){

   callback_function = fnc;
}

//----------------------------
//#pragma optimize("y", off)

EXCPT_RETURN_CODE __declspec(dllexport) __stdcall UserException(const char *title, const char *msg){

   EXCEPTION_POINTERS ep;
   EXCEPTION_RECORD er;
   CONTEXT cr;
   memset(&ep, 0, sizeof(ep));
   memset(&er, 0, sizeof(er));
   memset(&cr, 0, sizeof(cr));
   ep.ExceptionRecord = &er;
   ep.ContextRecord = &cr;
   er.ExceptionAddress = InitializeExceptions;
   __asm{
      mov eax, [ebp+0]
      mov cr.Ebp, eax
      mov eax, [ebp+4]
      mov cr.Esp, eax
      mov eax, [ebp+4]
      lea edi, er
      mov [edi]er.ExceptionAddress, eax
   }
   EXCPT_RETURN_CODE rtn = ExceptHandle(&ep, title, msg, true, true);
   if(rtn == EXCPT_DEBUG){
      rtn = EXCPT_CLOSE;
      __asm int 3
   }
   return rtn;
}

//----------------------------
// Collect call stack with addresses from current location up to top callers.
void __declspec(dllexport) __stdcall GetCallStack(t_call_stack &call_stack){

   EXCEPTION_POINTERS ep;
   EXCEPTION_RECORD er;
   CONTEXT cr;
   memset(&ep, 0, sizeof(ep));
   memset(&er, 0, sizeof(er));
   memset(&cr, 0, sizeof(cr));
   ep.ExceptionRecord = &er;
   ep.ContextRecord = &cr;
   er.ExceptionAddress = InitializeExceptions;
   __asm{
      mov eax, [ebp+0]
      mov cr.Ebp, eax
      mov eax, [ebp+4]
      mov cr.Esp, eax
      mov eax, [ebp+4]
      lea edi, er
      mov [edi]er.ExceptionAddress, eax
   }
   t_call_stack cs_tmp;
   GetCallStack(&ep, cs_tmp);
   call_stack = cs_tmp;
}

//----------------------------
// Display call stack dialog.
// Return true if user chooses Close, false if user chooses Ignore.
EXCPT_RETURN_CODE __declspec(dllexport) __stdcall ShowCallStackDialog(const char *title,
   const char *user_message, const t_call_stack &call_stack){

   //if(callback_function) (*callback_function)();

   char *log_dump;
   //BuildCrashInfo(NULL, user_message, &log_dump);
   FormatCrashInfo(call_stack, NULL, user_message, &log_dump);

                              //fast log into a file
   if(!user_message){
      FILE *fh = fopen("crash.log", "wb");
      if(fh){
         fwrite(log_dump, 1, strlen(log_dump), fh);
         fclose(fh);
      }
   }

   EXCPT_RETURN_CODE ret = DisplayDialog(title, true, true, log_dump);
   delete[] log_dump;

   return ret;
}

//----------------------------
