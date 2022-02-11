#include <windows.h>
#include <rules.h>
#include <insanity\assert.h>

//----------------------------

//#pragma warning(disable:4073)
//#pragma init_seg(lib)
#pragma comment(lib, "DebugMem.lib")

extern"C"{
void __declspec(dllimport) *__cdecl _DebugAlloc(size_t);
void __declspec(dllimport) __cdecl _DebugFree(void*);
void __declspec(dllimport) *__cdecl _DebugRealloc(void*, size_t);
void __declspec(dllimport) __cdecl _SetBreakAlloc(dword);
void __declspec(dllimport) __cdecl _SetBreakAllocPtr(void*);
void __declspec(dllimport) __cdecl _DumpMemoryLeaks();
int force_debug_alloc_include;
}

//----------------------------

void *__cdecl operator new(size_t sz){

   if(!sz)
      return NULL;
   return _DebugAlloc(sz);
}

//----------------------------

void __cdecl operator delete(void *vp){

   if(!vp)
      return;
   _DebugFree(vp);
}

//----------------------------

void *DebugAlloc(size_t sz){
   if(!sz)
      return NULL;
   return _DebugAlloc(sz);
}

//----------------------------

void DebugFree(void *vp){

   if(!vp)
      return;
   _DebugFree(vp);
}

//----------------------------

void *DebugRealloc(void *vp, size_t sz){

   if(!vp)
      return DebugAlloc(sz);
   if(!sz){
      DebugFree(vp);
      return NULL;
   }
   return _DebugRealloc(vp, sz);
}

//----------------------------

void SetBreakAlloc(dword id){
   //if(_SetBreakAlloc)
      _SetBreakAlloc(id);
}

//----------------------------

void SetBreakAllocPtr(void *vp){
   //if(_SetBreakAllocPtr)
      _SetBreakAllocPtr(vp);
}

//----------------------------

void DumpMemoryLeaks(){
   //if(_DumpMemoryLeaks)
      _DumpMemoryLeaks();
}

//----------------------------



