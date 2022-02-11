#include "all.h"
#include "cpu.h"


//----------------------------

static void InitCPU(int &init, dword &features, dword &ext_features, dword &processor){
      
   __asm{
      pushad
      pushfd                  //save EFLAGS to stack
      pop     eax             //store EFLAGS in EAX
      mov     edx, eax        //save for testing later
      xor     eax, 0x200000   //switch bit 21
      push    eax             //copy "changed" value to stack
      popfd                   //save "changed" EAX to EFLAGS
      pushfd
      pop     eax
      xor     eax, edx        //see if bit changeable
      jnz     short foundit   //if so, mark 
      mov     eax, -1
      jmp     short around

      ALIGN   4
  foundit:
      // Load up the features and (where appropriate) extended features flags
      mov     eax, 1          //check for processor features
      CPUID
      mov edi, features
      mov     [edi], edx //store features bits
      mov     eax, 0x80000000 //check for support of extended functions.
      CPUID
      cmp     eax, 0x80000001 //make sure function 0x80000001 supported.
      jb      short around
      mov     eax, 0x80000001 //select function 0x80000001
      CPUID
      mov edi, processor
      mov     [edi], eax   //store processor family/model/step
      mov edi, ext_features
      mov     [edi], edx//store extende features bits
      mov     eax, 1             //set "Has CPUID" flag to true

  around:
      mov edi, init
      mov     [edi], eax
      popad
   }
}

//----------------------------

static dword GetCPUManufacturer(){

   dword res;
   __asm{
      push ecx
                        //wuery manufacturer string
      mov     eax,0     //function 0 = manufacturer string
      CPUID

         //These tests could probably just check the 'ebx' part of the string,
         // but the entire string is checked for completeness.  Plus, this function
         // should not be used in time-critical code, because the CPUID instruction
         // serializes the processor. (That is, it flushes out the instruction pipeline.)

                        //test for 'AuthenticAMD'
      cmp     ebx, 'htuA'
      jne     short not_amd
      cmp     edx, 'itne'
      jne     short not_amd
      cmp     ecx, 'DMAc'
      jne     short not_amd
      mov     eax, MFG_AMD
      jmp     short next_test

                        //test for 'GenuineIntel'
   not_amd:
      cmp     ebx, 'uneG'
      jne     short not_intel
      cmp     edx, 'Ieni'
      jne     short not_intel
      cmp     ecx, 'letn'
      jne     short not_intel
      mov     eax, MFG_INTEL
      jmp     short next_test

                              //test for 'CyrixInstead'
   not_intel:
      cmp     ebx, 'iryC'
      jne     short not_cyrix
      cmp     edx, 'snIx'
      jne     short not_cyrix
      cmp     ecx, 'deat'
      jne     short not_cyrix
      mov     eax, MFG_CYRIX
      jmp     short next_test

                              //test for 'CentaurHauls'
   not_cyrix:
      cmp     ebx, 'tneC'
      jne     short not_centaur
      cmp     edx, 'Hrua'
      jne     short not_centaur
      cmp     ecx, 'slua'
      jne     short not_centaur
      mov     eax, MFG_CENTAUR
      jmp     short next_test

   not_centaur:
      mov     eax, MFG_UNKNOWN

   next_test:
      mov     [res],eax       //store result of previous tests

      pop ecx
   }
   return res;
}

//----------------------------

dword GetCPUCaps(CPUCAPS cap){

   dword res = 0;

   static dword features = 0;
   static dword ext_features = 0;
   static dword processor = 0;
   static int init = 0;

   __try{
                              //detect CPUID presence once, since all other requests depend on it
      if(!init)
         InitCPU(init, features, ext_features, processor);

                              //no CPUID, so no CPUID functions are supported
      if(init == -1)
         return 0;

                              //perform the requested tests
      switch(cap){
                              //synthesized Capabilities
      case HAS_CPUID:
                              //always true if this code gets executed
         res = true;
         break;

      case CPU_MFG:
         res = GetCPUManufacturer();
         break;

      case CPU_TYPE:
           // Return a member of the CPUTYPES enumeration
           // Note: do NOT use this for determining presence of chip features, such
           // as MMX and 3DNow!  Instead, use GetCPUCaps (HAS_MMX) and GetCPUCaps (HAS_3DNOW),
           // which will accurately detect the presence of these features on all chips which
           // support them.
         switch(GetCPUCaps(CPU_MFG)){
         case MFG_AMD:
            switch((processor >> 8) & 0xf){
                              //extract family code
            case 4: // Am486/AM5x86
               res = AMD_Am486;
               break;

            case 5: // K6
               switch((processor >> 4) & 0xf){
                              //extract model code
               case 0: res = AMD_K5; break;
               case 1: res = AMD_K5; break;
               case 2: res = AMD_K5; break;
               case 3: res = AMD_K5; break;
               case 4: res = AMD_K6_MMX; break;
               case 5: res = AMD_K6_MMX; break;
               case 6: res = AMD_K6_MMX; break;
               case 7: res = AMD_K6_MMX; break;
               case 8: res = AMD_K6_2; break;
               case 9: res = AMD_K6_3; break;
               }
               break;

            case 6: // K7 Athlon
               res = AMD_K7;
               break;
            }
            break;

         case MFG_INTEL:
            switch((processor >> 8) & 0xf){
                              //extract family code
            case 4:
               switch((processor >> 4) & 0xf){
                              //extract model code
               case 0: res = INTEL_486DX;  break;
               case 1: res = INTEL_486DX;  break;
               case 2: res = INTEL_486SX;  break;
               case 3: res = INTEL_486DX2; break;
               case 4: res = INTEL_486SL;  break;
               case 5: res = INTEL_486SX2; break;
               case 7: res = INTEL_486DX2E;break;
               case 8: res = INTEL_486DX4; break;
               }
               break;

            case 5:
               switch((processor >> 4) & 0xf){
                              //extract model code
               case 1: res = INTEL_Pentium;    break;
               case 2: res = INTEL_Pentium;    break;
               case 3: res = INTEL_Pentium;    break;
               case 4: res = INTEL_Pentium_MMX;break;
               }
               break;

            case 6:
               switch((processor >> 4) & 0xf){
                              //extract model code
               case 1: res = INTEL_Pentium_Pro;break;
               case 3: res = INTEL_Pentium_II; break;
               case 5: res = INTEL_Pentium_II; break;  // actual differentiation depends on cache settings
               case 6: res = INTEL_Celeron;    break;
               case 7: res = INTEL_Pentium_III;break;  // actual differentiation depends on cache settings
               }
               break;
            }
            break;

         case MFG_CYRIX:
            res = UNKNOWN;
            break;

         case MFG_CENTAUR:
            res = UNKNOWN;
            break;
         }
         break;

                              //feature Bit Test Capabilities
      case HAS_FPU:     res = (features >> 0) & 1;       break;   // bit 0 = FPU
      case HAS_VME:     res = (features >> 1) & 1;       break;   // bit 1 = VME
      case HAS_DEBUG:   res = (features >> 2) & 1;       break;   // bit 2 = Debugger extensions
      case HAS_PSE:     res = (features >> 3) & 1;       break;   // bit 3 = Page Size Extensions
      case HAS_TSC:     res = (features >> 4) & 1;       break;   // bit 4 = Time Stamp Counter
      case HAS_MSR:     res = (features >> 5) & 1;       break;   // bit 5 = Model Specific Registers
      case HAS_MCE:     res = (features >> 6) & 1;       break;   // bit 6 = Machine Check Extensions
      case HAS_CMPXCHG8:res = (features >> 7) & 1;       break;   // bit 7 = CMPXCHG8 instruction
      case HAS_MMX:     res = (features >> 23) & 1;      break;   // bit 23 = MMX
      case HAS_3DNOW:   res = (ext_features >> 31) & 1;  break;   // bit 31 (ext) = 3DNow!
      case HAS_SIMD:
         {
                              //bit 25 = SIMD
            res = (features&0x2000000);
            if(res){
               _asm{
                              //check OS support for SIMD
                              // execute a SIMD instruction (xorps xmm0, xmm0)
                              // if it fails, exception is thrown and zero is returned
                  __emit 0x0f
                  __emit 0x57
                  __emit 0xc0
               }
            }
            break;
         }
         break;
      }
   }
   __except(EXCEPTION_EXECUTE_HANDLER){
      return 0;
   }
   return res;
}

//----------------------------
