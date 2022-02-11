#include "pch.h"

//----------------------------

bool OsCreateDirectory(const char *cp){

   SECURITY_ATTRIBUTES sa;
   memset(&sa, 0, sizeof(sa));
   sa.nLength = sizeof(sa);
   return CreateDirectory(cp, &sa);
}

//----------------------------

bool OsRemoveDirectory(const char *dir){

   return RemoveDirectory(dir);
}

//----------------------------

bool OsCreateDirectoryTree(const char *dst){

   if(!*dst)
      return false;
   
   SECURITY_ATTRIBUTES sa;
   memset(&sa, 0, sizeof(sa));
   sa.nLength = sizeof(sa);
   dword cpos, npos = 0;
   C_str str = dst;
   for(;;){
      cpos = npos;
      while(str[++npos]) if(str[npos]=='\\') break;
      if(cpos){
         str[cpos] = 0;
         //bool st = 
         CreateDirectory(str, &sa);
         /*
         if(!st)
            return false;
            */
         str[cpos] = '\\';
      }
      if(!str[npos])
         break;
   }
   return true;
}

//----------------------------

bool OsDeleteFile(const char *cp){

   return DeleteFile(cp);
}

//----------------------------

bool OsDeleteDirectory(const char *dir){

   return RemoveDirectory(dir);
}

//----------------------------

bool OsGetComputerName(C_str &str){

   char buf[255];
   dword sz = sizeof(buf);
   if(!GetComputerName(buf, &sz)){
      str = NULL;
      return false;
   }
   str = buf;
   return true;
}

//----------------------------

bool OsMoveFile(const char *src, const char *dst){

   bool ok;
   ok = DeleteFile(dst);
   if(!ok){
                              //try to clear the read-only flag
      SetFileAttributes(dst, GetFileAttributes(dst) & ~FILE_ATTRIBUTE_READONLY);
   }
   ok = MoveFile(src, dst);
   if(!ok){
      OsCreateDirectoryTree(dst);
      DeleteFile(dst);
      ok = MoveFile(src, dst);
   }
   return ok;
}

//----------------------------

bool OsIsFileReadOnly(const char *fname){

   dword dw = GetFileAttributes(fname);
   if(dw==0xffffffff) return false;
   return (dw&FILE_ATTRIBUTE_READONLY);
}

//----------------------------

bool OsMakeFileReadOnly(const char *fname, bool rdonly){

   dword dw = GetFileAttributes(fname);
   if(dw==0xffffffff)
      return false;
   bool is_rdonly = (dw&FILE_ATTRIBUTE_READONLY);
   if(is_rdonly==rdonly)
      return false;
   dw ^= FILE_ATTRIBUTE_READONLY;
   return SetFileAttributes(fname, dw);
}

//----------------------------

bool OsMakeFileHidden(const char *fname, bool hidden){

   dword dw = GetFileAttributes(fname);
   if(dw==0xffffffff)
      return false;
   bool is_hidden = (dw&FILE_ATTRIBUTE_HIDDEN);
   if(is_hidden==hidden)
      return false;
   dw ^= FILE_ATTRIBUTE_HIDDEN;
   return SetFileAttributes(fname, dw);
}

//----------------------------

void OsSleep(int time){
   Sleep(time);
}

//----------------------------

bool OsIsCDROMDrive(const char *path){

   char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
   _splitpath(path, drive, dir, fname, ext);
   UINT dt = GetDriveType(drive);
   return (dt==DRIVE_CDROM);
}

//----------------------------

void OsCollectFiles(const char *path1, const char *wildmask, C_buffer<C_str> &ret, bool dirs, bool files){

   C_fstr path = path1;
   if(path.Size() && path[path.Size()-1]!='\\')
      path += "\\";
   C_fstr findname("%s%s", (const char*)path, wildmask);

   C_vector<C_str> vec_files;
   WIN32_FIND_DATA fd;
   HANDLE h = FindFirstFile(findname, &fd);
   if(h!=INVALID_HANDLE_VALUE){
      do{
         if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
            if(fd.cFileName[0]=='.')
               continue;
            if(!dirs)
               continue;
         }else{
            if(!files)
               continue;
         }
         vec_files.push_back(C_fstr("%s%s", (const char*)path, fd.cFileName));
      }while(FindNextFile(h, &fd));
      FindClose(h);
   }
   ret.assign(&vec_files.front(), (&vec_files.back())+1);
}

//----------------------------

void OsCreateUniqueFileName(const char *path, const char *ext, C_str &str){

                              //make sure name contains valid characters only
   for(dword i=str.Size(); i--; ){
      char &c = str[i];
      if(!isalnum(c) && !isspace(c))
         c = '_';
   }
   if(!str.Size())
      str = "name";

                              //make sure such name doesn't exist
   dword digit_index = str.Size();
   while(digit_index && isdigit(str[digit_index])) --digit_index;
   int digit_base = atoi(&str[digit_index]);
   for(;;){
      C_fstr full_path("%s\\%s.%s", path, (const char*)str, ext);
      FILE *fp = fopen(full_path, "rb");
      if(!fp)
         break;
      fclose(fp);
                              //modify name
      str[digit_index] = 0;
      str = C_fstr("%s%i", (const char*)str, ++digit_base);
   }
}

//----------------------------

bool OsIsDirExist(const char *path){

   HANDLE hFile = CreateFile(path,
              GENERIC_READ | GENERIC_WRITE, 
              FILE_SHARE_READ | FILE_SHARE_WRITE,
              NULL,
              OPEN_EXISTING,
              FILE_FLAG_BACKUP_SEMANTICS,
              NULL
           );
   if(hFile == INVALID_HANDLE_VALUE)
      return false;
#ifdef _DEBUG
   bool b =
#endif
   CloseHandle(hFile);
   assert(b);
   return true;
}

//----------------------------

void OsCreteUniqueDirName(const char *path, C_str &str){

                              //make sure name contains valid characters only
   for(dword i=str.Size(); i--; ){
      char &c = str[i];
      if(!isalnum(c) && !isspace(c))
         c = '_';
   }
   if(!str.Size())
      str = "name";
                              //make sure such name doesn't exist
   dword digit_index = str.Size();
   while(digit_index && isdigit(str[digit_index])) --digit_index;
   int digit_base = atoi(&str[digit_index]);
   for(;;){
      C_fstr full_path("%s\\%s", path, (const char*)str);

      if(!OsIsDirExist(full_path))
         break; 
                              //modify name
      str[digit_index] = 0;
      str = C_fstr("%s%i", (const char*)str, ++digit_base);
   }
}

//----------------------------

void OsGetCurrentTimeDate(S_date_time &dt){

   SYSTEMTIME st;
   GetLocalTime(&st);
   dt.year = st.wYear;
   dt.month = st.wMonth;
   dt.day = st.wDay;
   dt.hour = st.wHour;
   dt.minute = st.wMinute;
   dt.second = st.wSecond;
}

//----------------------------

bool OsGetFileTimeDate(const char *fname, S_date_time &dt){

   PC_dta_stream dta = DtaCreateStream(fname);
   if(!dta)
      return false;

   FILETIME ftime;
   bool ret = dta->GetTime((dword*)&ftime);
   dta->Release();
   if(ret){
      SYSTEMTIME st;
      FileTimeToSystemTime(&ftime, &st);
      dt.year = st.wYear;
      dt.month = st.wMonth;
      dt.day = st.wDay;
      dt.hour = st.wHour;
      dt.minute = st.wMinute;
      dt.second = st.wSecond;
   }
   return ret;
}

//----------------------------

bool OsGetExecutableVersion(const char *fname, int &major, int &minor){

   FILE *fp = fopen(fname, "rb");
   if(!fp)
      return false;
   IMAGE_DOS_HEADER dh;
   fread(&dh, sizeof(dh), 1, fp);

   int seek = dh.e_lfanew + sizeof(dword) + sizeof(IMAGE_FILE_HEADER);
   fseek(fp, seek, SEEK_SET);
   IMAGE_OPTIONAL_HEADER oh;
   fread(&oh, sizeof(oh), 1, fp);
   major = oh.MajorImageVersion;
   minor = oh.MinorImageVersion;

   fclose(fp);
   return true;
}

//----------------------------

bool OsDisplayFile(const char *filename){

   PROCESS_INFORMATION pi;
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
   C_fstr cmdline(" %s\\%s", buf, filename);

   bool ok = CreateProcess((const char *) notepad_exe,
      (char*)(const char*)cmdline,
      &lsa,                //process security attributes
      &lsa,                //thread security attributes
      true,                //handle inheritance flag
      0,                   //creation flags
      (void*)(const char*)env,
      NULL,                //pointer to current directory name
      &si,
      &pi);

   return ok;
}

//----------------------------
//----------------------------
