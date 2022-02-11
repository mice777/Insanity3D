extern"C"{
#include "ogg\os_types.h"
}
#include <memory.h>

//----------------------------

void *DebugAlloc(size_t);
void DebugFree(void*);
void *DebugRealloc(void*, size_t);

//----------------------------

void *DebugAlloc_c(size_t sz){ return DebugAlloc(sz); }
void DebugFree_c(void *vp){ DebugFree(vp); }
void *DebugRealloc_c(void *vp, size_t sz){ return DebugRealloc(vp, sz); }

void *DebugCAlloc_c(size_t sz){

   void *vp = DebugAlloc(sz);
   memset(vp, 0, sz);
   return vp;
}

//----------------------------
