#include <windows.h>
#include <c_str.hpp>
#pragma warning(push, 3)
#include <set>
#include <vector>
#pragma warning(pop)
#include <malloc.h>
#include <iexcpt.h>
#include <assert.h>
#define DTA_READ_STATIC
#include <C_cache.h>

using namespace std;

#pragma warning(disable:4073)
#pragma init_seg(lib)

#pragma optimize("y", off)

//----------------------------

#define USE_LEAK_AND_CHECK_MODE  //use mode for checking memory leaks and corruption
//#define USE_GUARD_PAGES_BEGIN
//#define USE_GUARD_PAGES_END   //use mode for guarding ends of blocks by non-access pages


#ifdef USE_LEAK_AND_CHECK_MODE

//----------------------------
#define USE_SYNCHRONIZATION

//#define FILL_MASK 0xffcecece  //float NAN, integer garbage, pointer garbage
#define FILL_MASK ((0<<31) | (0xff << 23) | 0x2ecece) //float NAN, integer garbage, pointer garbage
//#define FILL_MASK ((1<<31) | (0x00 << 23) | 0x4ecece) //float denormal, integer garbage, pointer garbage
#define MEM_GUARD_DWORDS 4    //number of guard dwords on both sides on allocated memory

   
//----------------------------

/*
void __cdecl AssertDialog(void*, void*, unsigned){
   assert(0);
}
*/
void __fastcall SetBreakAlloc(dword){}

//----------------------------

static bool in_alloc;         //set to true during internall alloc/free
static bool de_init;          //true if shutting down (no more complaints)

typedef set<void*> t_blocks;
static t_blocks *curr_alloc_blocks; //currently allocated memory pointers

static dword alloc_id;
static dword break_alloc_id = 0xffffffff;
static void *break_alloc_ptr = NULL;

//----------------------------

struct S_mem_header{
   dword alloc_id;            //-1 = internal block
   dword size;
   t_call_stack *call_stack;
};


#ifdef USE_SYNCHRONIZATION
static CRITICAL_SECTION csect;
#endif

//----------------------------

static void DumpMemoryLeaksInternal(){
   if(de_init)
      return;
#ifdef USE_SYNCHRONIZATION
   EnterCriticalSection(&csect);
#endif
   if(break_alloc_id!=-2){
      if(curr_alloc_blocks){
         if(curr_alloc_blocks->size()){
            t_blocks::const_iterator it;
            dword sz = 0;
            C_str ids;
            int num_ids = 0;
            for(it=curr_alloc_blocks->begin(); it!=curr_alloc_blocks->end(); it++){
               void *vp = (*it);
               vp = (byte*)vp - sizeof(S_mem_header) - MEM_GUARD_DWORDS*4;
               const S_mem_header &header = *(S_mem_header*)vp;
               assert(header.alloc_id != -1);
               sz += header.size;
               if(num_ids<8){
                  if(ids.Size())
                     ids += ", ";
                  ids += C_fstr("%i", header.alloc_id);
               }
               ++num_ids;
            }
            C_fstr msg("Memory leaks detected - %i block(s), %i bytes\r\nAllocation IDs: ", curr_alloc_blocks->size(), sz);
            const char *title = "Memory leak warning";

            msg += ids;

            HINSTANCE hi = LoadLibrary("iexcpt.dll");

            for(it=curr_alloc_blocks->begin(); it!=curr_alloc_blocks->end(); it++){
               void *vp = (*it);
               vp = (byte*)vp - sizeof(S_mem_header) - MEM_GUARD_DWORDS*4;
               const S_mem_header &header = *(S_mem_header*)vp;
               t_call_stack *call_stack = header.call_stack;
               if(!call_stack){
                  UserException(title, msg);
                  break;
               }else{
                  t_call_stack cs_tmp;
                                 //strip bottom 2 pointers (one is ours, next is operator new
                  if(call_stack->size() >= 2){
                     //call_stack->erase(call_stack->begin(), call_stack->begin()+2);
                     cs_tmp.assign(call_stack->begin()+2, call_stack->end());
                     call_stack = &cs_tmp;
                  }
                  EXCPT_RETURN_CODE ret = ShowCallStackDialog(C_fstr("%s - Block #%i (%i bytes)", title, header.alloc_id, header.size), msg, *call_stack);
                  if(ret!=EXCPT_IGNORE)
                     break;
               }
            }
            FreeLibrary(hi);
         }
         delete curr_alloc_blocks;
      }
   }
   de_init = true;
#ifdef USE_SYNCHRONIZATION
   LeaveCriticalSection(&csect);
   DeleteCriticalSection(&csect);
#endif
}

//----------------------------

BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,  // handle to DLL module
  DWORD fdwReason,     // reason for calling function
  LPVOID lpvReserved   // reserved
  ){

   switch(fdwReason){
   case DLL_PROCESS_ATTACH:
#ifdef USE_SYNCHRONIZATION
      InitializeCriticalSection(&csect);
#endif
      _set_sbh_threshold(1016);
      break;

   case DLL_PROCESS_DETACH:
      DumpMemoryLeaksInternal();
      break;
   }
   return true;
}

//----------------------------
extern"C"{
void __declspec(dllexport) *__cdecl _DebugAlloc(size_t);
void __declspec(dllexport) __cdecl _DebugFree(void*);
void __declspec(dllexport) *__cdecl _DebugRealloc(void*, size_t);
void __declspec(dllexport) __cdecl _SetBreakAlloc(dword);
void __declspec(dllexport) __cdecl _SetBreakAllocPtr(void*);
void __declspec(dllexport) __cdecl _DumpMemoryLeaks();
void __declspec(dllexport) __cdecl _DumpMemInfo(dword min_size);
}

//----------------------------

void __declspec(dllexport) __cdecl _SetBreakAlloc(dword id){
   break_alloc_id = id;
}

//----------------------------

void __declspec(dllexport) __cdecl _SetBreakAllocPtr(void *vp){
   break_alloc_ptr = vp;
}

//----------------------------

                              //fill allocated and freed mem by garbage
                              // requires:
                              // non-debug memory allocation functions


void __declspec(dllexport) *__cdecl _DebugAlloc(size_t sz){

   if(!sz)
      return NULL;

#ifdef USE_SYNCHRONIZATION
   EnterCriticalSection(&csect);
#endif

   void *vp;

   if(in_alloc){
      vp = malloc(sz + sizeof(S_mem_header) + MEM_GUARD_DWORDS*4);
      if(!vp)
         return NULL;
      S_mem_header &header = *(S_mem_header*)vp;
      header.alloc_id = 0xffffffff;
      vp = (byte*)vp + sizeof(S_mem_header) + MEM_GUARD_DWORDS*4;
   }else{
      in_alloc = true;

      vp = malloc(sz + sizeof(S_mem_header) + MEM_GUARD_DWORDS*4*2);
      if(!vp)
         return NULL;
      S_mem_header &header = *(S_mem_header*)vp;
      header.alloc_id = ++alloc_id;
      header.call_stack = NULL;
      header.size = sz;
      if(header.alloc_id == break_alloc_id){
         //break_alloc_id = -2;
         __asm int 3
      }
      header.call_stack = new t_call_stack;
      GetCallStack(*header.call_stack);
      vp = (byte*)vp + sizeof(S_mem_header);

      __asm{
         push ecx

         mov eax, FILL_MASK
         mov edi, vp

         mov ecx, sz
         add ecx, MEM_GUARD_DWORDS*4
         push ecx
         shr ecx, 2

         jz new_no_fill
         rep stosd

      new_no_fill:
         pop ecx
         and ecx, 3
         jz new_no_fill1
         rep stosb

      new_no_fill1:

         mov ecx, MEM_GUARD_DWORDS
         rep stosd

         pop ecx
      }
      vp = (byte*)vp + MEM_GUARD_DWORDS*4;
      if(vp==break_alloc_ptr){
         __asm int 3
      }
      if(!curr_alloc_blocks)
         curr_alloc_blocks = new t_blocks;
      curr_alloc_blocks->insert(vp);
      in_alloc = false;
   }
#ifdef USE_SYNCHRONIZATION
   LeaveCriticalSection(&csect);
#endif
   return vp;
}

//----------------------------

void __declspec(dllexport) __cdecl _DebugFree(void *vp1){

   if(de_init)
      return;

#ifdef USE_SYNCHRONIZATION
   EnterCriticalSection(&csect);
#endif

   void *vp = vp1;
   if(vp){
      vp = (byte*)vp - sizeof(S_mem_header) - MEM_GUARD_DWORDS*4;
      const S_mem_header &header = *(S_mem_header*)vp;

      if(header.alloc_id != 0xffffffff){

         in_alloc = true;

         bool ok_mem = false;
         t_blocks::iterator it;
         if(curr_alloc_blocks){
            it = curr_alloc_blocks->find(vp1);
            if(it!=curr_alloc_blocks->end())
               ok_mem = true;
         }
         if(ok_mem){

            size_t sz = header.size;
            void *mem = (byte*)vp + sizeof(S_mem_header);
            byte corrupt = 0;
            __asm{
               push ecx

               mov eax, FILL_MASK
                                 //check guard bytes
               mov edi, mem
               mov ecx, MEM_GUARD_DWORDS
               rep scasd
               jz ok_mem1
               mov corrupt, 1
               jmp del_no_fill1

            ok_mem1:
               mov ecx, MEM_GUARD_DWORDS
               mov edi, mem
               add edi, sz
               add edi, MEM_GUARD_DWORDS*4
               rep scasd
               jz ok_mem2
               mov corrupt, 2
               jmp del_no_fill1

            ok_mem2:
                                 //fill by garbage again
               mov edi, vp
               add edi, MEM_GUARD_DWORDS*4
               mov ecx, sz
               shr ecx, 2
               jz del_no_fill
               rep stosd
            del_no_fill:

               mov ecx, sz
               and ecx, 3
               jz del_no_fill1
               rep stosb

            del_no_fill1:
               pop ecx
            }
            if(corrupt){
               mem = (byte*)mem + MEM_GUARD_DWORDS*4;
            
               C_fstr msg(
                  "Memory corrupted: 0x%x, size: %i (written %s block)\r\n"
                  "Memory dump:"
                  ,
                  (byte*)mem, sz,
                  (corrupt==1) ? "before" : "past");
               for(int i=0; i<Min((int)sz, 32); i += 8){
                  C_str bytes;
                  char chars[9];
                  const byte *bp = (const byte*)mem + i;
                  for(int x=0; x<8; x++){
                     byte c = bp[x];
                     bytes += C_fstr("%.2x ", c);
                     if(!isprint(c))
                        c = '.';
                     chars[x] = c;
                  }
                  chars[8] = 0;
                  msg += C_fstr("\r\n%.2x   %s  %s", i, (const char*)bytes, chars);
               }
               //bool ignore = UserException("Memory corruption warning", msg);
               EXCPT_RETURN_CODE rc = ShowCallStackDialog("Memory corruption warning", msg, *header.call_stack);

               vp = NULL;
               //if(!ignore)
               if(rc!=EXCPT_IGNORE)
                  _exit(0);
            }
            curr_alloc_blocks->erase(it);
            delete header.call_stack;
         }else{
            bool ignore = UserException("Memory corruption warning", C_fstr("Freeing non-allocated memory: 0x%x", vp1));
            vp = NULL;
            if(ignore)
               _exit(0);
         }
         in_alloc = false;
      }
   }
   free(vp);
#ifdef USE_SYNCHRONIZATION
   LeaveCriticalSection(&csect);
#endif
}

//----------------------------

void __declspec(dllexport) *__cdecl _DebugRealloc(void *vp_old, size_t sz){

   assert(vp_old && sz);
   void *vp = _DebugAlloc(sz);
   if(!vp)
      return NULL;

   dword copy_size;
   {
      void *vp1 = (byte*)vp_old - sizeof(S_mem_header) - MEM_GUARD_DWORDS*4;
      const S_mem_header &header = *(S_mem_header*)vp1;
      copy_size = Min((dword)sz, header.size);
   }
   memcpy(vp, vp_old, copy_size);
   _DebugFree(vp_old);

   return vp;
}

//----------------------------

void __declspec(dllexport) __cdecl _DumpMemoryLeaks(){

   DumpMemoryLeaksInternal();
}

//----------------------------

void __declspec(dllexport) __cdecl _DumpMemInfo(dword min_size){

   if(!curr_alloc_blocks)
      return;
   in_alloc = true;

   t_blocks::const_iterator it;

   C_cache ck;
   ck.open("memory.log", CACHE_WRITE);

   dword total = 0;
   static const char title[] = " #      Block ID      size\n---------------\n";
   ck.write(title, sizeof(title)-1);

   int i = 0;
   for(it=curr_alloc_blocks->begin(); it!=curr_alloc_blocks->end(); it++, ++i){
      void *vp = (*it);
      vp = (byte*)vp - sizeof(S_mem_header) - MEM_GUARD_DWORDS*4;
      const S_mem_header &header = *(S_mem_header*)vp;
      assert(header.alloc_id != -1);

      if(header.size >= min_size){
         C_str s(C_xstr("#4%  #8%    #8%  (% KB)\n") %i %header.alloc_id %header.size %(header.size/1024));
         ck.write((const char*)s, s.Size());
      }
      total += header.size;
   }
   C_str s(C_xstr("--------------\nMemory blocks: %, total memory: %  (% KB)\n") %curr_alloc_blocks->size()
      %total %(total/1024));
   ck.write((const char*)s, s.Size());
   in_alloc = false;
}

//----------------------------

#endif//USE_LEAK_AND_CHECK_MODE

#ifdef USE_GUARD_PAGES_END

//----------------------------
                              //different algo - using guard pages
extern"C"{
void __declspec(dllexport) *__cdecl _DebugAlloc(size_t);
void __declspec(dllexport) __cdecl _DebugFree(void*);
void __declspec(dllexport) *__cdecl _DebugRealloc(void*, size_t);
}

const dword PAGE_SIZE = 0x1000;

//----------------------------

void __declspec(dllexport) *__cdecl _DebugAlloc(size_t sz){
   if(!sz)
      return NULL;
   dword alloc_pages = ((sz + 4 + PAGE_SIZE - 1) / PAGE_SIZE) + 1;
   byte *base = (byte*)VirtualAlloc(NULL, alloc_pages*PAGE_SIZE, MEM_RESERVE, PAGE_READWRITE);
   if(!base)
      return NULL;
   VirtualAlloc(base, (alloc_pages-1)*PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
   VirtualAlloc(base + (alloc_pages-1)*PAGE_SIZE, PAGE_SIZE, MEM_COMMIT, PAGE_NOACCESS);
   base += PAGE_SIZE - (sz%PAGE_SIZE);
   ((dword*)base)[-1] = sz;
   return base;
}

//----------------------------

void __declspec(dllexport) __cdecl _DebugFree(void *vp){

   if(!vp)
      return;
   void *base = (void*)((dword(vp) - 4) & ~(PAGE_SIZE-1));
   bool ok;
   ok = VirtualFree(base, 0, MEM_RELEASE);
   assert(ok);
}

//----------------------------

void __declspec(dllexport) *__cdecl _DebugRealloc(void *vp_old, size_t sz){

   assert(vp_old && sz);
   void *vp = DebugAlloc(sz);
   if(!vp)
      return NULL;
   dword copy_size;
   {
      dword sz_old = ((dword*)vp_old)[-1];
      copy_size = Min((dword)sz, sz_old);
   }
   memcpy(vp, vp_old, copy_size);
   DebugFree(vp_old);
   return vp;
}

//----------------------------
#endif//USE_GUARD_PAGES_END


