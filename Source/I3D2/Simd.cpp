/*--------------------------------------------------------
   Copyright (c) 1999, 2000 Michal Bacik. 
   All rights reserved.

   File: Simd.cpp
   Content: Streaming SIMD support.
--------------------------------------------------------*/

#include "all.h"
#include "simd.h"
//#include <windows.h>
//#include <I3D\I3D_math.h>

#ifndef _USE_INTEL_COMPILER
#error "Intel compiler required"
#endif

//----------------------------

__declspec(naked) void __stdcall PIII_Mult00_4x4_4x4(const void *src1, const void *src2, void *dst){

//#define MOVPS movaps
#define MOVPS movups

   __asm{
      push esi
      push edi

      mov esi, dword ptr [esp+8+4]   ;src1
      mov eax, dword ptr [esp+8+0ch] ;dst
      mov edi, dword ptr [esp+8+8]   ;src2

      movss xmm0, dword ptr [esi]
      MOVPS xmm1, xmmword ptr [edi]
      shufps xmm0, xmm0, 0
      movss xmm2, dword ptr [esi+4]
      mulps xmm0, xmm1
      shufps xmm2, xmm2, 0
      MOVPS xmm3, xmmword ptr [edi+10h]
      movss xmm7, dword ptr [esi+8]
      mulps xmm2, xmm3
      shufps   xmm7, xmm7, 0
      addps xmm0, xmm2
      MOVPS xmm4, xmmword ptr [edi+20h]
      movss xmm2, dword ptr [esi+0Ch]
      mulps xmm7, xmm4
      shufps   xmm2, xmm2, 0
      addps xmm0, xmm7
      MOVPS xmm5, xmmword ptr [edi+30h]
      movss xmm6, dword ptr [esi+10h]
      mulps xmm2, xmm5
      movss xmm7, dword ptr [esi+14h]
      shufps   xmm6, xmm6, 0
      addps xmm0, xmm2
      shufps   xmm7, xmm7, 0
      movlps   qword ptr [eax], xmm0
      movhps   qword ptr [eax+8], xmm0
      mulps xmm7, xmm3
      movss xmm0, dword ptr [esi+18h]
      mulps xmm6, xmm1
      shufps   xmm0, xmm0, 0
      addps xmm6, xmm7
      mulps xmm0, xmm4
      movss xmm2, dword ptr [esi+24h]
      addps xmm6, xmm0
      movss xmm0, dword ptr [esi+1Ch]
      movss xmm7, dword ptr [esi+20h]
      shufps   xmm0, xmm0, 0
      shufps   xmm7, xmm7, 0
      mulps xmm0, xmm5
      mulps xmm7, xmm1
      addps xmm6, xmm0
      shufps   xmm2, xmm2, 0
      movlps   qword ptr [eax+10h], xmm6
      movhps   qword ptr [eax+18h], xmm6
      mulps xmm2, xmm3
      movss xmm6, dword ptr [esi+28h]
      addps xmm7, xmm2
      shufps   xmm6, xmm6, 0
      movss xmm2, dword ptr [esi+2Ch]
      mulps xmm6, xmm4
      shufps   xmm2, xmm2, 0
      addps xmm7, xmm6
      mulps xmm2, xmm5
      movss xmm0, dword ptr [esi+34h]
      addps xmm7, xmm2
      shufps   xmm0, xmm0, 0
      movlps   qword ptr [eax+20h], xmm7
      movss xmm2, dword ptr [esi+30h]
      movhps   qword ptr [eax+28h], xmm7
      mulps xmm0, xmm3
      shufps   xmm2, xmm2, 0
      movss xmm6, dword ptr [esi+38h]
      mulps xmm2, xmm1
      shufps   xmm6, xmm6, 0
      addps xmm2, xmm0
      mulps xmm6, xmm4
      movss xmm7, dword ptr [esi+3Ch]
      shufps   xmm7, xmm7, 0
      addps xmm2, xmm6
      mulps xmm7, xmm5
      addps xmm2, xmm7
      MOVPS xmmword ptr [eax+30h], xmm2
      pop edi
      pop esi
      ret 12
   }
}

//----------------------------

#pragma warning(disable:4035)

__declspec(naked) bool __stdcall SIMD_SphereInVF(const S_SIMD_view_frustum &vf,
   const I3D_bsphere &bsphere, bool &clip){

   __asm{
                              //eax - free
                              //ebx - clip flags
                              //esi - vf*
                              //edi - loop counter
      mov eax, [esp+8]     //get bsphere
      prefetcht0 [eax]

      push ebx
      push esi
      push edi

      mov esi, [esp+4+12]     //get SIMD view frustum

                              //XMM0 - 4X bsphere.pos.x
                              //XMM1 - 4X bsphere.pos.y
                              //XMM2 - 4X bsphere.pos.z
                              //XMM3 - 4X bsphere.radius
      movups xmm0, [eax]
      prefetcht0 [esi]
      movaps xmm1, xmm0
      movaps xmm2, xmm0
      movaps xmm3, xmm0
      shufps xmm0, xmm0, 0x00
      shufps xmm1, xmm1, 0x55
      shufps xmm2, xmm2, 0xaa
      shufps xmm3, xmm3, 0xff

                              //default clipping = no
      mov ebx, 0xf

                              //get number of planes
      mov edi, [esi + (SIZE S_plane) * MAX_FRUSTUM_CLIP_PLANES]
      shr edi, 2
      jz out
   loop:
      prefetcht0 [esi + 16]
                              //make x
      movups xmm4, [esi + 0]  
      prefetcht0 [esi + 16 + MAX_FRUSTUM_CLIP_PLANES*4]
      mulps xmm4, xmm0
                              //make y
      prefetcht0 [esi + 16 + MAX_FRUSTUM_CLIP_PLANES*8]
      movups xmm5, [esi + MAX_FRUSTUM_CLIP_PLANES*4]
      prefetcht0 [esi + 16 + MAX_FRUSTUM_CLIP_PLANES*12]
      mulps xmm5, xmm1
                              //add x + y
      addps xmm4, xmm5
                              //make z
      movups xmm5, [esi + MAX_FRUSTUM_CLIP_PLANES*8]
      mulps xmm5, xmm2
                              //add (x + y) + z
      addps xmm4, xmm5
                              //add d
      movups xmm5, [esi + MAX_FRUSTUM_CLIP_PLANES*12]
      addps xmm4, xmm5
                              //distances to 4 planes are in xmm4

                              //update clip flags
      movaps xmm5, xmm3
      addps xmm5, xmm4
      movmskps eax, xmm5
      and ebx, eax
                              //check if out
      subps xmm4, xmm3
      movmskps eax, xmm4
      cmp eax, 0xf
      jnz false1

                              //move to next four floats
      add esi, 16             
      dec edi
      jnz loop
   out:
                              //store clipping
      mov eax, [esp+12+12]
      cmp ebx, 0xf
      setnz [eax]
                              //Z = 0
      cmp ebx, ebx
   false1:
      setz al
      
      pop edi
      pop esi
      pop ebx

      and eax, 1

      ret 12
   }
#ifdef _USE_INTEL_COMPILER
   return 0;
#endif
}

//----------------------------

/*

// mmtest.cpp

#include <windows.h>

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>

#include <xmmintrin.h>

void PII_Mult00_4x4_4x1(float *src1, float *src2, float *dst)
{
   StartRecordTime;
   __asm mov  edx, DWORD PTR src1
   __asm mov  ecx, DWORD PTR src2
   __asm mov  eax, DWORD PTR dst
   e4(0, 0, 1, 4, 1, 0, 0)
   e4(1, 0, 1, 4, 1, 0, 0)
   e4(2, 0, 1, 4, 1, 0, 0)
   e4(3, 0, 1, 4, 1, 0, 0)
   StopRecordTime;
}

__declspec(naked) void PIII_Mult00_3x3_3x3(float *src1, float *src2, float *dst)
{
   StartRecordTime;
   __asm {
      mov     ecx,  dword ptr [esp+8]       ; src2
      mov     edx,  dword ptr [esp+4]       ; src1
      mov     eax,  dword ptr [esp+0Ch]     ; dst
            
      movss   xmm2, dword ptr [ecx+32]
      movhps  xmm2, qword ptr [ecx+24]
            
      movss   xmm3, dword ptr [edx]
      movss   xmm4, dword ptr [edx+4]
            
      movss   xmm0, dword ptr [ecx]
      movhps  xmm0, qword ptr [ecx+4]
      shufps  xmm2, xmm2, 0x36
      shufps  xmm3, xmm3, 0
            
      movss   xmm1, dword ptr [ecx+12]
      movhps  xmm1, qword ptr [ecx+16]
            
      shufps  xmm4, xmm4, 0
      mulps   xmm3, xmm0
      movss   xmm5, dword ptr [edx+8]
      movss   xmm6, dword ptr [edx+12]
      mulps   xmm4, xmm1
      shufps  xmm5, xmm5, 0
      mulps   xmm5, xmm2
      shufps  xmm6, xmm6, 0
      mulps   xmm6, xmm0
      addps   xmm3, xmm4
            
      movss   xmm7, dword ptr [edx+16]
      movss   xmm4, dword ptr [edx+28]
            
      shufps  xmm7, xmm7, 0
      addps   xmm3, xmm5
      mulps   xmm7, xmm1
            
      shufps  xmm4, xmm4, 0
            
      movss   xmm5, dword ptr [edx+20]
      shufps  xmm5, xmm5, 0
      mulps   xmm4, xmm1
            
      mulps   xmm5, xmm2
      addps   xmm6, xmm7
            
      movss   xmm1, dword ptr [edx+24]
            
      movss   dword ptr [eax]  , xmm3
      movhps  qword ptr [eax+4], xmm3
            
      addps   xmm6, xmm5
      shufps  xmm1, xmm1, 0
            
      movss   xmm5, dword ptr [edx+32]
      mulps   xmm1, xmm0
      shufps  xmm5, xmm5, 0
            
      movss   dword ptr [eax+12], xmm6
      mulps   xmm5, xmm2
      addps   xmm1, xmm4
      movhps  qword ptr [eax+16], xmm6
      addps   xmm1, xmm5
      shufps  xmm1, xmm1, 0x8F
            
      movhps  qword ptr [eax+24], xmm1
      movss   dword ptr [eax+32], xmm1
   }
   StopRecordTime;
   __asm ret
}

__declspec(naked) void PIII_Mult00_3x3_3x1(float *src1, float *src2, float *dst)
{
   StartRecordTime;
   __asm {
      mov   edx,  dword ptr [esp+4]       ; src1
      mov   ecx,  dword ptr [esp+8]       ; src2
      movss xmm1, dword ptr [edx]
      mov   eax,  dword ptr [esp+0Ch]     ; dst
      movhps   xmm1, qword ptr [edx+4]
      movaps   xmm5, xmm1
      movss xmm3, dword ptr [edx+12]
      movhps   xmm3, qword ptr [edx+24]
      movss xmm4, dword ptr [ecx]
      shufps   xmm5, xmm3, 128
      movlps   xmm0, qword ptr [edx+16]
      shufps   xmm4, xmm4, 0
      movhps   xmm0, qword ptr [edx+28]
      shufps   xmm1, xmm0, 219
      movss xmm2, dword ptr [ecx+4]
      movaps   xmm3, xmm1
      shufps   xmm1, xmm0, 129
      shufps   xmm2, xmm2, 0
      movss xmm0, dword ptr [ecx+8]
      mulps xmm4, xmm5
      mulps xmm2, xmm1
      shufps   xmm0, xmm0, 0
      addps xmm4, xmm2
      mulps xmm0, xmm3
      addps xmm4, xmm0
      movss dword ptr [eax], xmm4
      movhps   qword ptr [eax+4], xmm4
   }
   StopRecordTime;
   __asm ret
}

__declspec(naked) void PIII_Mult10_3x3_3x1(float *src1, float *src2, float *dst)
{
   StartRecordTime;
   __asm {
      mov   ecx,  dword ptr [esp+8]       ; src2
      mov   edx,  dword ptr [esp+4]       ; src1
      mov   eax,  dword ptr [esp+0Ch]     ; dst

      movss    xmm0, dword ptr [ecx]

      movss xmm5, dword ptr [edx]
      movhps   xmm5, qword ptr [edx+4]

      shufps   xmm0, xmm0, 0

      movss xmm1, dword ptr [ecx+4]

      movss xmm3, dword ptr [edx+12]
      movhps   xmm3, qword ptr [edx+16]

      shufps   xmm1, xmm1, 0

      mulps xmm0, xmm5
      mulps xmm1, xmm3

      movss xmm2, dword ptr [ecx+8]
      shufps   xmm2, xmm2, 0

      movss xmm4, dword ptr [edx+24]
      movhps   xmm4, qword ptr [edx+28]

      addps xmm0, xmm1
      mulps xmm2, xmm4

      addps xmm0, xmm2

      movss dword ptr [eax], xmm0
      movhps   qword ptr [eax+4], xmm0
   }
   StopRecordTime;
   __asm ret
}

__declspec(naked) void PIII_Mult00_4x4_4x1(float *src1, float *src2, float *dst)
{
   StartRecordTime;
   __asm {
      mov   ecx,  dword ptr [esp+ 8]       ; src2
      mov   edx,  dword ptr [esp+ 4]       ; src1

      movlps   xmm6, qword ptr [ecx   ]
      movlps   xmm0, qword ptr [edx   ]

      shufps   xmm6, xmm6, 0x44
      movhps   xmm0, qword ptr [edx+16]
      mulps xmm0, xmm6
      movlps   xmm7, qword ptr [ecx+ 8]

      movlps   xmm2, qword ptr [edx+ 8]
      shufps   xmm7, xmm7, 0x44
      movhps   xmm2, qword ptr [edx+24]

      mulps xmm2, xmm7
      movlps   xmm1, qword ptr [edx+32]
      movhps   xmm1, qword ptr [edx+48]
      
      mulps xmm1, xmm6
      movlps   xmm3, qword ptr [edx+40]
      addps xmm0, xmm2
      movhps   xmm3, qword ptr [edx+56]

      mov   eax,  dword ptr [esp+12]       ; dst

      mulps xmm3, xmm7

      movaps   xmm4, xmm0
      addps xmm1, xmm3

      shufps   xmm4, xmm1, 0x88
      shufps   xmm0, xmm1, 0xDD

      addps xmm0, xmm4

      movaps   xmmword ptr [eax], xmm0
   }
   StopRecordTime;
   __asm ret
}

__declspec(naked) void PIII_Mult10_4x4_4x1(float *src1, float *src2, float *dst)
{
   StartRecordTime;
   __asm {
      mov   ecx,  dword ptr [esp+8]       ; src2
      mov   edx,  dword ptr [esp+4]       ; src1

      movss xmm0, dword ptr [ecx]
      mov   eax,  dword ptr [esp+0Ch]     ; dst
      shufps   xmm0, xmm0, 0

      movss xmm1, dword ptr [ecx+4]
      mulps xmm0, xmmword ptr [edx]
      shufps   xmm1, xmm1, 0

      movss xmm2, dword ptr [ecx+8]
      mulps xmm1, xmmword ptr [edx+16]
      shufps   xmm2, xmm2, 0

      movss xmm3, dword ptr [ecx+12]
      mulps xmm2, xmmword ptr [edx+32]
      shufps   xmm3, xmm3, 0

      addps xmm0, xmm1

      mulps xmm3, xmmword ptr [edx+48]

      addps xmm2, xmm3
      addps xmm0, xmm2

      movaps   xmmword ptr [eax], xmm0
   }
   StopRecordTime;
   __asm ret
}

int Ra;
int Ca;
int Rb;
int Cb;
int StrideA; // Stride form one row of A to the next (in bytes)
int StrideB; // Stride form one row of B to the next (in bytes)
void MatrixMult(float *MatrixA, float *MatrixB, float *MatrixO)
{
   StartRecordTime;
   __asm {
      pushad
      Matrix_of_Results_Setup:
      mov   ecx, 0 ; Counter for rows in A - Ra
      Row_of_Results_Loop:
      mov   ebx, 0 ; Counter for columns in B - Cb
      Dot_Product_Setup:
      mov   eax, 0 ; Counter for single dot product - Ca or Rb
      mov   esi, MatrixA ; Load pointer to An0
      mov   edi, MatrixB ; Load pointer to B00 
      lea   edi, [edi+ebx*4] ; Adjust pointer horizontally to correct batch of 24
      xorps xmm2, xmm2 ; zero out accumulators for pass of 24 results
      xorps xmm3, xmm3
      xorps xmm4, xmm4
      xorps xmm5, xmm5
      xorps xmm6, xmm6
      xorps xmm7, xmm7
      Dot_Product_Loop:
      mov   edx, [esi+eax*4]
      shl   edx, 1
      cmp   edx, 0
      je    Sparse_Entry_Escape
      movss xmm0, [esi+eax*4]
      shufps   xmm0, xmm0, 0x0
      movaps   xmm1, [edi]
      mulps xmm1, xmm0
      addps xmm2, xmm1
      movaps   xmm1, [edi+16]
      mulps xmm1, xmm0
      addps xmm3, xmm1
      movaps   xmm1, [edi+32]
      mulps xmm1, xmm0
      addps xmm4, xmm1
      movaps   xmm1, [edi+48]
      mulps xmm1, xmm0
      addps xmm5, xmm1
      movaps   xmm1, [edi+64]
      mulps xmm1, xmm0
      addps xmm6, xmm1
      movaps   xmm1, [edi+80]
      mulps xmm1, xmm0
      addps xmm7, xmm1
      Sparse_Entry_Escape:
      add   edi, StrideB ; Move down a row in B
      inc   eax
      cmp   eax, Ca ; Can compare to Ca or Rb since they must be equal
      jl    Dot_Product_Loop
       ; End_Dot_Product_Loop
      mov   eax, MatrixO ; Load pointer to On0
      lea   eax, [eax+ebx*4] ; Adjust pointer horizontally to correct batch of 24
      movaps   [eax], xmm2 ; store to Output
      movaps   [eax+16], xmm3
      movaps   [eax+32], xmm4
      movaps   [eax+48], xmm5
      movaps   [eax+64], xmm6
      movaps   [eax+80], xmm7
      add   ebx, 24 ; Move over to next batch of 24
      cmp   ebx, Cb ; Check to see if row is complete 
      jl    Dot_Product_Setup
       ; End_Row_of_Results_Loop
      mov   eax, MatrixA
      add   eax, StrideA
      mov   MatrixA, eax
      mov   eax, MatrixO
      add   eax, StrideB
      mov   MatrixO, eax
      inc   ecx
      cmp   ecx, Ra
      jl    Row_of_Results_Loop
       ; End_Matrix_Matrix_Multiply_Loop
      popad
   }
   StopRecordTime;
}

void PIII_Inverse_4x4(float* src)
{
   __m128   minor0, minor1, minor2, minor3;
   __m128   row0,   row1,   row2,   row3;
   __m128   det,    tmp1;

   tmp1  = _mm_loadh_pi(_mm_loadl_pi(tmp1, (__m64*)(src) ), (__m64*)(src+ 4));
   row1  = _mm_loadh_pi(_mm_loadl_pi(row1, (__m64*)(src+8)), (__m64*)(src+12));

   row0  = _mm_shuffle_ps(tmp1, row1, 0x88);
   row1  = _mm_shuffle_ps(row1, tmp1, 0xDD);

   tmp1  = _mm_loadh_pi(_mm_loadl_pi(tmp1, (__m64*)(src+ 2)), (__m64*)(src+ 6));
   row3  = _mm_loadh_pi(_mm_loadl_pi(row3, (__m64*)(src+10)), (__m64*)(src+14));

   row2  = _mm_shuffle_ps(tmp1, row3, 0x88);
   row3  = _mm_shuffle_ps(row3, tmp1, 0xDD);
   // -----------------------------------------------
   tmp1  = _mm_mul_ps(row2, row3);
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

   minor0   = _mm_mul_ps(row1, tmp1);
   minor1   = _mm_mul_ps(row0, tmp1);

   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

   minor0   = _mm_sub_ps(_mm_mul_ps(row1, tmp1), minor0);
   minor1   = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor1);
   minor1   = _mm_shuffle_ps(minor1, minor1, 0x4E);
   // -----------------------------------------------
   tmp1  = _mm_mul_ps(row1, row2);
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

   minor0   = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor0);
   minor3   = _mm_mul_ps(row0, tmp1);

   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

   minor0   = _mm_sub_ps(minor0, _mm_mul_ps(row3, tmp1));
   minor3   = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor3);
   minor3   = _mm_shuffle_ps(minor3, minor3, 0x4E);
   // -----------------------------------------------
   tmp1  = _mm_mul_ps(_mm_shuffle_ps(row1, row1, 0x4E), row3);
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
   row2  = _mm_shuffle_ps(row2, row2, 0x4E);

   minor0   = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor0);
   minor2   = _mm_mul_ps(row0, tmp1);

   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

   minor0   = _mm_sub_ps(minor0, _mm_mul_ps(row2, tmp1));
   minor2   = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor2);
   minor2   = _mm_shuffle_ps(minor2, minor2, 0x4E);
   // -----------------------------------------------
   tmp1  = _mm_mul_ps(row0, row1);
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0xB1); 

   minor2   = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor2);
   minor3   = _mm_sub_ps(_mm_mul_ps(row2, tmp1), minor3);
   
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
   
   minor2   = _mm_sub_ps(_mm_mul_ps(row3, tmp1), minor2);
   minor3   = _mm_sub_ps(minor3, _mm_mul_ps(row2, tmp1));
   // -----------------------------------------------
   tmp1  = _mm_mul_ps(row0, row3);
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
   
   minor1   = _mm_sub_ps(minor1, _mm_mul_ps(row2, tmp1));
   minor2   = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor2);   
   
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
   
   minor1   = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor1);
   minor2   = _mm_sub_ps(minor2, _mm_mul_ps(row1, tmp1));   
   // -----------------------------------------------
   tmp1  = _mm_mul_ps(row0, row2);
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
   
   minor1   = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor1);
   minor3   = _mm_sub_ps(minor3, _mm_mul_ps(row1, tmp1));   
   
   tmp1  = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
   
   minor1   = _mm_sub_ps(minor1, _mm_mul_ps(row3, tmp1));
   minor3   = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor3);   
   // -----------------------------------------------
   det   = _mm_mul_ps(row0, minor0);
   det   = _mm_add_ps(_mm_shuffle_ps(det, det, 0x4E), det);
   det   = _mm_add_ss(_mm_shuffle_ps(det, det, 0xB1), det);
   tmp1  = _mm_rcp_ss(det);

   det   = _mm_sub_ss(_mm_add_ss(tmp1, tmp1), _mm_mul_ss(det, _mm_mul_ss(tmp1, tmp1)));
   det   = _mm_shuffle_ps(det, det, 0x00);

   minor0   = _mm_mul_ps(det, minor0);
   _mm_storel_pi((__m64*)(src), minor0);
   _mm_storeh_pi((__m64*)(src+2), minor0);

   minor1   = _mm_mul_ps(det, minor1);
   _mm_storel_pi((__m64*)(src+4), minor1);
   _mm_storeh_pi((__m64*)(src+6), minor1);

   minor2   = _mm_mul_ps(det, minor2);
   _mm_storel_pi((__m64*)(src+ 8), minor2);
   _mm_storeh_pi((__m64*)(src+10), minor2);

   minor3   = _mm_mul_ps(det, minor3);
   _mm_storel_pi((__m64*)(src+12), minor3);
   _mm_storeh_pi((__m64*)(src+14), minor3);
}

char* testname;
_MM_ALIGN16 float m31[] = {   11,12,13,
            21,22,23,
            31,32,33};
_MM_ALIGN16 float m32[] = {   1,2,3,
            0,1,2,
            -1,0,1};
_MM_ALIGN16 float m33[] = {-2,34,70, -2,64,130, -2,94,190};

_MM_ALIGN16 float v4[] = {0, 1, 2, 3};

_MM_ALIGN16 float m41[] = {   11,12,13,14,
            21,22,23,24,
            31,32,33,34,
            41,42,43,44};
_MM_ALIGN16 float m42[] = {   1,2,3,4,
            0,1,2,3,
            -1,0,1,2,
            0,1,2,3};
_MM_ALIGN16 float m43[] = {-2,48,98,148,-2,88,178,268,-2,128,258,388,-2,168,338,508};
_MM_ALIGN16 float m44[16];
_MM_ALIGN16 float m45[16];
_MM_ALIGN16 float m46[16];

_MM_ALIGN16 float m61[] = {   11,12,13,14,15,16,
            21,22,23,24,25,26,
            31,32,33,34,35,36,
            41,42,43,44,45,46,
            51,52,53,54,55,56,
            61,62,63,64,65,66};

_MM_ALIGN16 float m62[] = {   1,2,3,4,5,6,
            0,1,2,3,4,5,
            -1,0,1,2,3,4,
            0,1,2,3,4,5,
            1,2,3,4,5,6,
            2,3,4,5,6,7};
_MM_ALIGN16 float m63[36];
_MM_ALIGN16 float m64[36];
_MM_ALIGN16 float m65[36];
_MM_ALIGN16 float m66[36];

  */

//----------------------------
