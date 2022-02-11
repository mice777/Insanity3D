#include <windows.h>
#include <rules.h>
#include <win_reg.h>

#pragma comment(lib,"advapi32")

//----------------------------

static const HKEY root_keys[] = {
   HKEY_CURRENT_USER,
   HKEY_LOCAL_MACHINE,
   HKEY_CLASSES_ROOT
};

//----------------------------

int RegkeyCreate(const char *name, E_REGKEY_ROOT rk_root){

   if(rk_root>=E_REGKEY_LAST)
      return -1;

   HKEY key;
   DWORD disp;
   int i = RegCreateKeyEx(root_keys[rk_root], name,
      0, "", REG_OPTION_NON_VOLATILE,
      KEY_EXECUTE | KEY_WRITE | KEY_QUERY_VALUE,
      NULL, &key, &disp);
   if(i != ERROR_SUCCESS) return -1;
   return (int)key;
}

//----------------------------

int RegkeyOpen(const char *name, E_REGKEY_ROOT rk_root){

   HKEY key;
   int i = RegOpenKeyEx(root_keys[rk_root], name, 0,
      KEY_EXECUTE | KEY_WRITE | KEY_QUERY_VALUE,
      &key);
   if(i != ERROR_SUCCESS) return -1;
   return (int)key;
}

//----------------------------

void RegkeyClose(int key){

   RegCloseKey((HKEY)key);
}

//----------------------------

bool RegkeyExist(const char *name, E_REGKEY_ROOT rk_root){

   int key = RegkeyOpen(name, rk_root);
   if(key == -1)
      return false;
   RegkeyClose(key);
   return true;
}

//----------------------------

bool RegkeyWdata(int key, const char *valname, const void *mem, dword len){

   int i = RegSetValueEx((HKEY)key, valname, 0, REG_BINARY, (byte*)mem, len);
   return (i == ERROR_SUCCESS);
}

//----------------------------

bool RegkeyWdword(int key, const char *valname, dword data){

   int i = RegSetValueEx((HKEY)key, valname, 0, REG_DWORD, (byte*)&data, sizeof(dword));
   return (i == ERROR_SUCCESS);
}

//----------------------------

bool RegkeyWtext(int key, const char *valname, const char *text){

   int i = RegSetValueEx((HKEY)key, valname, 0, REG_SZ, (byte*)text, strlen(text)+1);
   return (i == ERROR_SUCCESS);
}

//----------------------------

int RegkeyRdata(int key, const char *valname, void *mem1, dword len1){

   DWORD len = len1;
   int i = RegQueryValueEx((HKEY)key, valname, 0, NULL, (byte*)mem1, &len);
   if(i == ERROR_SUCCESS) return len;
   if(i == ERROR_MORE_DATA){
      byte *tmp = new byte[len];
      if(!tmp) return -1;
      i = RegQueryValueEx((HKEY)key, valname, 0, NULL, tmp, &len);
      if(i == ERROR_SUCCESS){
         memcpy(mem1, tmp, len1);
         delete[] tmp;
         return len1;
      }
      delete[] tmp;
   }
   return -1;
}

//----------------------------

int RegkeyDataSize(int key, const char *valname){

   DWORD len = 0;
   int i = RegQueryValueEx((HKEY)key, valname, 0, NULL, NULL, &len);
   if(i == ERROR_SUCCESS || i == ERROR_MORE_DATA) return len;
   return -1;
}

//----------------------------

bool RegkeyDelval(int key, const char *valname){

   int i = RegDeleteValue((HKEY)key, valname);
   return (i == ERROR_SUCCESS);
}

//----------------------------

bool RegkeyDelkey(int key, const char *valname){

   int i = RegDeleteKey((HKEY)key, valname);
   return (i == ERROR_SUCCESS);
}

//----------------------------

