//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Assert macro, providing similar functionality that standard assert.
// Designed to work with IExcept library (which is capable to extract
// information when int3 (BREAKPOINT exception) occurs.
//----------------------------
#undef  assert

#ifdef  NDEBUG

#define assert(exp) ((void)0)

#else

#define assert(exp) if(!(exp)){\
   const char *__f__ = __FILE__, *__e__ = #exp; int __l__ = __LINE__; __asm push __e__ __asm push __l__ __asm push __f__ __asm push 0x12345678 \
   __asm int 3 \
   __asm add esp, 16 }

#endif

