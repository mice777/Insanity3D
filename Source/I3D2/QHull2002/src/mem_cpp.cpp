#include <stdio.h>

extern "C"{
#include <malloc.h>
#include <memory.h>
#include "mem.h"
}


void *malloc_(size_t insize){
   return new char[insize];
}

void *calloc_(size_t s0, size_t s1){
   size_t sz = s0 * s1;
   void *mem = malloc_(sz);
   memset(mem, 0, sz);
   return mem;
}

void free_(void *object) {
   delete[] (char*)object;
}




