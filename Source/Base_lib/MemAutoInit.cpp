//#define FILL_MASK 0xffcecece  //float NAN, integer garbage, pointer garbage
//#define FILL_MASK ((1<<31) | (0x00 << 23) | 0x4ecece) //float denormal, integer garbage, pointer garbage
#define FILL_MASK ((0<<31) | (0xff << 23) | 0x2ecece) //float NAN, integer garbage, pointer garbage

//----------------------------
//----------------------------
                              //fill local vars by garbage
                              // requires:
                              // - Gh compiler switch
                              // - disable optimizations (Debug build)

extern"C"
void __cdecl _penter();

__declspec(naked) void __cdecl _penter(){

   __asm{

      push edi
      push ecx
      push eax

      mov edi, esp
      mov ecx, 0xfc           //keep upper 24 bits clear
                              //try to extract size of local vars
      mov eax, [esp+12]
      cmp word ptr[eax+3], 0xec83   //check for sub esp, imm8
      jne no_sz_1
      mov cl, [eax+5]
      jmp ok_sz

   no_sz_1:
      cmp word ptr[eax+3], 0xec81   //check for sub esp, imm32
      jnz no_sz_2
      mov ecx, [eax+5]
      jmp ok_sz

   no_sz_2:
                              //check for mov eax, imm32 / call
      cmp byte ptr[eax+3], 0xb8
      jne no_sz_3
      cmp byte ptr[eax+8], 0xe8
      jne no_sz_3
      mov ecx, [eax+4]
                              //for now, limit fill size to 4KB (todo later: implement __chkstk)
      cmp ecx, 0xff0
      jb no_sz_3
      mov ecx, 0xff0

      jmp ok_sz

   no_sz_3:
      jmp skip_fill

   ok_sz:
      sub edi, ecx
      shr ecx, 2
      mov eax, FILL_MASK
      rep stosd

   skip_fill:
      pop eax
      pop ecx
      pop edi
      mov dword ptr [esp-4], FILL_MASK
      mov dword ptr [esp-8], FILL_MASK
      mov dword ptr [esp-12], FILL_MASK

      ret
   }
}

//----------------------------
