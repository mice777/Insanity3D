/*--------------------------------------------------------
   Copyright (c) 2002 Lonely Cat Games
   All rights reserved.

   File: Math_3d.cpp
   Content: Insanity math functions.
--------------------------------------------------------*/
#include <memory.h>

#include <I3D\i3d_math.h>


#define USE_ASM               //use inline assembly
//#define USE_D3DX

#ifdef USE_D3DX
#include <D3dx9math.h>

#pragma comment(lib, "D3dx9")
#endif

#include <insanity\assert.h>

//----------------------------
//----------------------------

static const float I3D_identity_matrix[] = {
   1.0f, 0.0f, 0.0f, 0.0f,
   0.0f, 1.0f, 0.0f, 0.0f,
   0.0f, 0.0f, 1.0f, 0.0f,
   0.0f, 0.0f, 0.0f, 1.0f,
};

//----------------------------

const S_matrix &I3DGetIdentityMatrix(){
   return *(S_matrix*)I3D_identity_matrix;
}

//----------------------------
//----------------------------
                              //2D vector class
float S_vector2::AngleTo(const S_vector2& v) const{

   float f = (float)sqrt(x*x + y*y) * (float)sqrt(v.x*v.x + v.y*v.y);
   if(I3DFabs(f) < MRG_ZERO)
      return 0.0f;
   float acos_val = (x*v.x + y*v.y) / f;
   acos_val = Max(acos_val, -1.0f);
   acos_val = Min(acos_val, 1.0f);
   return (float)acos(acos_val);
}

//----------------------------
//----------------------------
                              //matrix class
void S_matrix::Zero(){

   memset(this, 0, sizeof(S_matrix));
}

//----------------------------

void S_matrix::Identity(){

   memcpy(this, I3D_identity_matrix, sizeof(S_matrix));
}

//----------------------------

void S_matrix::RotationX(float angle){

   Zero();

   float c = (float)cos(angle);
   float s = (float)sin(angle);

   m[1][1] = c;
   m[2][2] = c;
   m[1][2] = -s;
   m[2][1] = s;
   m[0][0] = 1.0f;
   m[3][3] = 1.0f;
}

//----------------------------

void S_matrix::RotationY(float angle){

   Zero();

   float c = (float)cos(angle);
   float s = (float)sin(angle);

   m[0][0] = c;
   m[2][2] = c;
   m[0][2] = -s;
   m[2][0] = s;
   m[1][1] = 1.0f;
   m[3][3] = 1.0f;
}

//----------------------------

void S_matrix::RotationZ(const float angle){

   Zero();

   float c = (float)cos(angle);
   float s = (float)sin(angle);

   m[0][0] = c;
   m[1][1] = c;
   m[0][1] = -s;
   m[1][0] = s;
   m[2][2] = 1.0f;
   m[3][3] = 1.0f;
}

//----------------------------
// This function uses Cramer's Rule to calculate the matrix inverse.
bool S_matrix::Invert(){

                              //do not use D3DX, it divides by zero
#if defined USE_D3DX && 0

   return (D3DXMatrixInverse((D3DXMATRIX*)this, NULL, (D3DXMATRIX*)this)!=NULL);

#else

#define x03 x01
#define x13 x11
#define x23 x21
#define x33 x31
#define z00 x02
#define z10 x12
#define z20 x22
#define z30 x32
#define z01 x03
#define z11 x13
#define z21 x23
#define z31 x33

   float x00, x01, x02;
   float x10, x11, x12;
   float x20, x21, x22;
   float x30, x31, x32;

   float y01, y02, y03, y12, y13, y23;

   float z02, z03, z12, z13, z22, z23, z32, z33;

                              //read 1st two columns of matrix into registers 
   x00 = m[0][0];
   x01 = m[0][1];
   x10 = m[1][0];
   x11 = m[1][1];
   x20 = m[2][0];
   x21 = m[2][1];
   x30 = m[3][0];
   x31 = m[3][1];

                              //compute all six 2x2 determinants of 1st two columns 
   y01 = x00 * x11 - x10 * x01;
   y02 = x00 * x21 - x20 * x01;
   y03 = x00 * x31 - x30 * x01;
   y12 = x10 * x21 - x20 * x11;
   y13 = x10 * x31 - x30 * x11;
   y23 = x20 * x31 - x30 * x21;

                              //read 2nd two columns of matrix into registers 
   x02 = m[0][2];
   x03 = m[0][3];
   x12 = m[1][2];
   x13 = m[1][3];
   x22 = m[2][2];
   x23 = m[2][3];
   x32 = m[3][2];
   x33 = m[3][3];

                              //compute all 3x3 cofactors for 2nd two columns 
   z33 = x02 * y12 - x12 * y02 + x22 * y01;
   z23 = x12 * y03 - x32 * y01 - x02 * y13;
   z13 = x02 * y23 - x22 * y03 + x32 * y02;
   z03 = x22 * y13 - x32 * y12 - x12 * y23;
   z32 = x13 * y02 - x23 * y01 - x03 * y12;
   z22 = x03 * y13 - x13 * y03 + x33 * y01;
   z12 = x23 * y03 - x33 * y02 - x03 * y23;
   z02 = x13 * y23 - x23 * y13 + x33 * y12;

                              //compute all six 2x2 determinants of 2nd two columns 
   y01 = x02 * x13 - x12 * x03;
   y02 = x02 * x23 - x22 * x03;
   y03 = x02 * x33 - x32 * x03;
   y12 = x12 * x23 - x22 * x13;
   y13 = x12 * x33 - x32 * x13;
   y23 = x22 * x33 - x32 * x23;

                              //read 1st two columns of matrix into registers 
   x00 = m[0][0];
   x01 = m[0][1];
   x10 = m[1][0];
   x11 = m[1][1];
   x20 = m[2][0];
   x21 = m[2][1];
   x30 = m[3][0];
   x31 = m[3][1];

                              //compute all 3x3 cofactors for 1st column 
   z30 = x11 * y02 - x21 * y01 - x01 * y12;
   z20 = x01 * y13 - x11 * y03 + x31 * y01;
   z10 = x21 * y03 - x31 * y02 - x01 * y23;
   z00 = x11 * y23 - x21 * y13 + x31 * y12;

                              //compute 4x4 determinant & its reciprocal 
   float rcp = x30 * z30 + x20 * z20 + x10 * z10 + x00 * z00;

   const float MIN_DET = 1e-20f;
   if(I3DFabs(rcp) < MIN_DET){
      //throw C_except("invalid matrix inversion");
      //return false;
      rcp = 1.0f / MIN_DET;
   }else
      rcp = 1.0f / rcp;

                              //compute all 3x3 cofactors for 2nd column 
   z31 = x00 * y12 - x10 * y02 + x20 * y01;
   z21 = x10 * y03 - x30 * y01 - x00 * y13;
   z11 = x00 * y23 - x20 * y03 + x30 * y02;
   z01 = x20 * y13 - x30 * y12 - x10 * y23;

                              //multiply all 3x3 cofactors by reciprocal 
   m[0][0] = z00 * rcp;
   m[1][0] = z01 * rcp;
   m[0][1] = z10 * rcp;
   m[2][0] = z02 * rcp;
   m[0][2] = z20 * rcp;
   m[3][0] = z03 * rcp;
   m[0][3] = z30 * rcp;
   m[1][1] = z11 * rcp;
   m[2][1] = z12 * rcp;
   m[1][2] = z21 * rcp;
   m[3][1] = z13 * rcp;
   m[1][3] = z31 * rcp;
   m[2][2] = z22 * rcp;
   m[3][2] = z23 * rcp;
   m[2][3] = z32 * rcp;
   m[3][3] = z33 * rcp;
#endif

   return true;
}

//----------------------------

void S_matrix::CopyTransposed(const S_matrix &m){

#if defined USE_D3DX && 1
   D3DXMatrixTranspose((D3DXMATRIX*)this, (D3DXMATRIX*)&m);
#else
   operator =(m);
   Transpose();
#endif
}

//----------------------------

void S_matrix::Transpose(){

#if defined USE_D3DX && 1

   D3DXMatrixTranspose((D3DXMATRIX*)this, (D3DXMATRIX*)this);

#else

#if defined _MSC_VER && defined USE_ASM && 1

   __asm{
                              //edx = matrix pointer
      mov edx, this
                              //load 4
      mov eax, [edx + 0x04]
      mov ebx, [edx + 0x10]
      mov esi, [edx + 0x08]
      mov edi, [edx + 0x20]
                              //exchange 4
      mov [edx + 0x04], ebx
      mov [edx + 0x10], eax
      mov [edx + 0x08], edi
      mov [edx + 0x20], esi
                              //load 4
      mov eax, [edx + 0x0c]
      mov ebx, [edx + 0x30]
      mov esi, [edx + 0x18]
      mov edi, [edx + 0x24]
                              //exchange 4
      mov [edx + 0x0c], ebx
      mov [edx + 0x30], eax
      mov [edx + 0x18], edi
      mov [edx + 0x24], esi
                              //load 4
      mov eax, [edx + 0x1c]
      mov ebx, [edx + 0x34]
      mov esi, [edx + 0x2c]
      mov edi, [edx + 0x38]
                              //exchange 4
      mov [edx + 0x1c], ebx
      mov [edx + 0x34], eax
      mov [edx + 0x2c], edi
      mov [edx + 0x38], esi
   }
#else

   Swap(m[0][1], m[1][0]);
   Swap(m[0][2], m[2][0]);
   Swap(m[0][3], m[3][0]);

   Swap(m[1][2], m[2][1]);
   Swap(m[1][3], m[3][1]);

   Swap(m[2][3], m[3][2]);

#endif

#endif//USE_D3DX
}

//----------------------------

S_matrix S_matrix::operator *(const S_matrix &mat) const{

   S_matrix rtn;
   rtn.Make(*this, mat);
   return rtn;
}

//----------------------------

#define e3(i, j, l, m, n, a, b) \
   flm (0, i, j, m, n, a, b)   \
   flma(1, i, j, m, n, a, b)   \
   flma(2, i, j, m, n, a, b)   \
   __asm fstp dword ptr [eax + mi(l, 0, i, j)]

//----------------------------

S_matrix &S_matrix::Make(const S_matrix &m1, const S_matrix &m2){

                              //do not use D3DX version, ours is faster (operates on 3x4 matrix)
#if defined USE_D3DX && 0

   D3DXMatrixMultiply((D3DXMATRIX*)this, (D3DXMATRIX*)&m1, (D3DXMATRIX*)&m2);

#else

#if defined _MSC_VER && defined USE_ASM

   __asm{
                              //eax = m1
                              //ebx = m2
                              //edi = this
      mov eax, m1
      mov ebx, m2
      mov edi, this
                              //mc(0, 0)
      fld dword ptr  [eax + 0x00]
      mov dword ptr[edi + 0x0c], 0     //mc(0, 3)
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x00]
                              //mc(0, 1)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x04]
                              //mc(0, 2)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x08]

                              //mc(1, 0)
      fld dword ptr  [eax + 0x10]
      mov dword ptr[edi + 0x1c], 0     //mc(1, 3)
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x10]
                              //mc(1, 1)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x14]
                              //mc(1, 2)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x18]

                              //mc(2, 0)
      fld dword ptr  [eax + 0x20]
      mov dword ptr[edi + 0x2c], 0     //mc(2, 3)
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x20]
                              //mc(2, 1)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x24]
                              //mc(2, 2)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x28]

                              //mc(3, 0)
      fld dword ptr  [eax + 0x30]
      mov dword ptr[edi + 0x3c], 0x3f800000     //mc(3, 3) = 1.0f
      fmul dword ptr [ebx + 0x00]
      fadd dword ptr [ebx + 0x30]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x30]
                              //mc(3, 1)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x04]
      fadd dword ptr [ebx + 0x34]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x34]
                              //mc(3, 2)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x08]
      fadd dword ptr [ebx + 0x38]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x38]
   }

#else

   for(int x=0; x<3; x++){
      for(int y=0; y<3; y++){
         m[y][x] =
            m1.m[y][0] * m2.m[0][x] +
            m1.m[y][1] * m2.m[1][x] +
            m1.m[y][2] * m2.m[2][x];
      }
      m[3][x] =
         m1.m[3][0] * m2.m[0][x] +
         m1.m[3][1] * m2.m[1][x] +
         m1.m[3][2] * m2.m[2][x] +
                   m2.m[3][x];
   }
                              //setup last column
   m[0][3] = 0.0f;
   m[1][3] = 0.0f;
   m[2][3] = 0.0f;
   m[3][3] = 1.0f;

#endif

#endif//USE_D3DX

   return *this;
}

//----------------------------

S_matrix S_matrix::operator %(const struct S_matrix &m1) const{

   S_matrix rtn;
#if defined _MSC_VER && defined USE_ASM

   __asm{
                              //eax = m1
                              //ebx = m2
                              //edi = this
      mov eax, this
      mov ebx, m1
      lea edi, rtn
                              //mc(0, 0)
      fld dword ptr  [eax + 0x00]
      mov dword ptr[edi + 0x0c], 0     //mc(0, 3)
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x00]
                              //mc(0, 1)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x04]
                              //mc(0, 2)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x08]

                              //mc(1, 0)
      fld dword ptr  [eax + 0x10]
      mov dword ptr[edi + 0x1c], 0     //mc(1, 3)
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x10]
                              //mc(1, 1)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x14]
                              //mc(1, 2)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x18]

                              //mc(2, 0)
      fld dword ptr  [eax + 0x20]
      mov dword ptr[edi + 0x2c], 0     //mc(2, 3)
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x20]
                              //mc(2, 1)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x24]
                              //mc(2, 2)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x28]

                              //mc(3, 0)
      fld dword ptr  [eax + 0x30]
      mov dword ptr[edi + 0x3c], 0x3f800000     //mc(3, 3) = 1.0f
      fmul dword ptr [ebx + 0x00]
      //fadd dword ptr [ebx + 0x30]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr  [edi + 0x30]
                              //mc(3, 1)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x04]
      //fadd dword ptr [ebx + 0x34]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr  [edi + 0x34]
                              //mc(3, 2)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x08]
      //fadd dword ptr [ebx + 0x38]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr  [edi + 0x38]
   }

#else

   for(int x=0; x<3; x++){
      for(int y=0; y<4; y++){
         rtn.m[y][x] =
            m[y][0] * m1.m[0][x] +
            m[y][1] * m1.m[1][x] +
            m[y][2] * m1.m[2][x];
      }
   }
                              //setup last column
   rtn.m[0][3] = 0.0f;
   rtn.m[1][3] = 0.0f;
   rtn.m[2][3] = 0.0f;
   rtn.m[3][3] = 1.0f;

#endif

   return rtn;
}

//----------------------------

S_matrix S_matrix::Mult4X4(const struct S_matrix &mat) const{

   S_matrix ret;
   ret.Make4X4(*this, mat);
   return ret;
}

//----------------------------

S_matrix S_matrix::MultByProj(const S_projection_matrix &proj) const{
   S_matrix ret;

   ret.m[0][0] = m[0][0] * proj.m0_0;
   ret.m[0][1] = m[0][1] * proj.m1_1;
   ret.m[0][2] = m[0][2] * proj.m2_2 + m[0][3] * proj.m3_2;

   ret.m[1][0] = m[1][0] * proj.m0_0;
   ret.m[1][1] = m[1][1] * proj.m1_1;
   ret.m[1][2] = m[1][2] * proj.m2_2 + m[1][3] * proj.m3_2;

   ret.m[2][0] = m[2][0] * proj.m0_0;
   ret.m[2][1] = m[2][1] * proj.m1_1;
   ret.m[2][2] = m[2][2] * proj.m2_2 + m[2][3] * proj.m3_2;

   ret.m[3][0] = m[3][0] * proj.m0_0;
   ret.m[3][1] = m[3][1] * proj.m1_1;
   ret.m[3][2] = m[3][2] * proj.m2_2 + m[3][3] * proj.m3_2;

   int copy_col = !proj.orthogonal ? 2 : 3;
   ret.m[0][3] = m[0][copy_col];
   ret.m[1][3] = m[1][copy_col];
   ret.m[2][3] = m[2][copy_col];
   ret.m[3][3] = m[3][copy_col];
   return ret;
}

//----------------------------

S_matrix &S_matrix::Make4X4(const S_matrix &m1, const S_matrix &m2){

#ifdef USE_D3DX

   D3DXMatrixMultiply((D3DXMATRIX*)this, (D3DXMATRIX*)&m1, (D3DXMATRIX*)&m2);

#else

#if defined _MSC_VER && defined USE_ASM && 1
   
                              //*this = m1 * m2
   __asm{
                              //eax = m1
                              //ebx = m2
                              //edi = this
      mov eax, m1
      mov ebx, m2
      mov edi, this
                              //mc(0, 0)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x00]
                              //mc(0, 1)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x04]
                              //mc(0, 2)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x08]
                              //mc(0, 3)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x0c]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x0c]

                              //mc(1, 0)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x10]
                              //mc(1, 1)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x14]
                              //mc(1, 2)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x18]
                              //mc(1, 3)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x0c]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x1c]

                              //mc(2, 0)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x20]
                              //mc(2, 1)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x24]
                              //mc(2, 2)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x28]
                              //mc(2, 3)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x0c]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x2c]

                              //mc(3, 0)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x00]
      fadd dword ptr [ebx + 0x30]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x30]
                              //mc(3, 1)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x04]
      fadd dword ptr [ebx + 0x34]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x34]
                              //mc(3, 2)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x08]
      fadd dword ptr [ebx + 0x38]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x38]
                              //mc(3, 3)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x0c]
      fadd dword ptr [ebx + 0x3c]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x3c]
   }

#else

   for(int x=0; x<4; x++){
      for(int y=0; y<3; y++)
      m[y][x] = m1.m[y][0] * m2.m[0][x] +
                m1.m[y][1] * m2.m[1][x] +
                m1.m[y][2] * m2.m[2][x];
      m[3][x] = m1.m[3][0] * m2.m[0][x] +
                m1.m[3][1] * m2.m[1][x] +
                m1.m[3][2] * m2.m[2][x] +
                             m2.m[3][x];
   }

#endif

#endif
   return *this;
}

//----------------------------

S_matrix &S_matrix::Make4X4Transposed(const S_matrix &m1, const S_matrix &m2){

#ifdef USE_D3DX

   D3DXMatrixMultiplyTranspose((D3DXMATRIX*)this, (D3DXMATRIX*)&m1, (D3DXMATRIX*)&m2);

#else

#if defined _MSC_VER && defined USE_ASM && 1
   
                              //*this = T(m1*m2)
   __asm{
                              //eax = m1
                              //ebx = m2
                              //edi = this
      mov eax, m1
      mov ebx, m2
      mov edi, this
                              //mc(0, 0)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x00]
                              //mc(0, 1)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x10]
                              //mc(0, 2)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x20]
                              //mc(0, 3)
      fld dword ptr  [eax + 0x00]
      fmul dword ptr [ebx + 0x0c]
      fld dword ptr  [eax + 0x04]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x08]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x30]

                              //mc(1, 0)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x04]
                              //mc(1, 1)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x14]
                              //mc(1, 2)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x24]
                              //mc(1, 3)
      fld dword ptr  [eax + 0x10]
      fmul dword ptr [ebx + 0x0c]
      fld dword ptr  [eax + 0x14]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x18]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x34]

                              //mc(2, 0)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x08]
                              //mc(2, 1)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x18]
                              //mc(2, 2)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x28]
                              //mc(2, 3)
      fld dword ptr  [eax + 0x20]
      fmul dword ptr [ebx + 0x0c]
      fld dword ptr  [eax + 0x24]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x28]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x38]

                              //mc(3, 0)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x00]
      fadd dword ptr [ebx + 0x30]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fstp dword ptr [edi + 0x0c]
                              //mc(3, 1)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x04]
      fadd dword ptr [ebx + 0x34]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fstp dword ptr [edi + 0x1c]
                              //mc(3, 2)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x08]
      fadd dword ptr [ebx + 0x38]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      fstp dword ptr [edi + 0x2c]
                              //mc(3, 3)
      fld dword ptr  [eax + 0x30]
      fmul dword ptr [ebx + 0x0c]
      fadd dword ptr [ebx + 0x3c]
      fld dword ptr  [eax + 0x34]
      fmul dword ptr [ebx + 0x1c]
      faddp st(1), st
      fld dword ptr  [eax + 0x38]
      fmul dword ptr [ebx + 0x2c]
      faddp st(1), st
      fstp dword ptr [edi + 0x3c]
   }

#else

   for(int x=0; x<4; x++){
      for(int y=0; y<3; y++)
      m[x][y] = m1.m[y][0] * m2.m[0][x] +
                m1.m[y][1] * m2.m[1][x] +
                m1.m[y][2] * m2.m[2][x];
      m[x][3] = m1.m[3][0] * m2.m[0][x] +
                m1.m[3][1] * m2.m[1][x] +
                m1.m[3][2] * m2.m[2][x] +
                             m2.m[3][x];
   }

#endif

#endif//USE_D3DX
   return *this;
}

//----------------------------

S_matrix &S_matrix::operator *=(const S_matrix &m){

   S_matrix tmp;
   tmp.Make(*this, m);
   *this = tmp;
   return *this;
}

//----------------------------

bool S_matrix::SetDir(const S_vector &dir, float roll){

                              //set direction, using default up vector (0,1,0)
   SetDir(dir, S_vector(0, 1, 0));

                              //set roll
   if(!IsAbsMrgZero(roll)){
      S_matrix mz;
      mz.RotationZ(roll);
      *this = mz * *this;
   }
   return true;
}

//----------------------------

bool S_matrix::SetDir(const S_vector &dir2, const S_vector &up2){

   S_vector n_dir = dir2;
   {
                              //normalize in dir
      float d = n_dir.Square();
      if(!IsAbsMrgZero(1.0f-d)){
         if(!n_dir.Normalize()){
            Identity();
            return false;
         }
      }
   }
                              //copy up-vector, for case it points to this class
   const S_vector in_up = up2;

                              //init last column to [0, 0, 0, 1]
   m[0][3] = 0.0f;
   m[1][3] = 0.0f;
   m[2][3] = 0.0f;
   m[3][3] = 1.0f;
                              //zero translation
   m[3][0] = 0.0f;
   m[3][1] = 0.0f;
   m[3][2] = 0.0f;

   S_vector &right = (*this)(0);
   S_vector &up    = (*this)(1);
   S_vector &dir   = (*this)(2);

                              //setup view dir from provided vector
   dir = n_dir;

                              //set right to be perpendicular to dir-vector and up-vector
   right = in_up.Cross(dir);

                              //normalize right-vector (if it fails, set to some reasonable default
                              // (actually it may fail only if dir points to (0,1,0) or (0,-1,0)
   if(!right.Normalize())
      right = S_vector(1, 0, 0);

                              //compute third vector to be perpendicular to the two
                              // (no need to normalize, since it's product of 2 perpendicular unit-lenght vectors)
   up = dir.Cross(right);

   return true;
}

//----------------------------

S_vector S_matrix::GetScale() const{

   S_vector ret;
   ret[0] = operator()(0).Magnitude();
   ret[1] = operator()(1).Magnitude();
   ret[2] = operator()(2).Magnitude();
   return ret;
}

//----------------------------

S_matrix::S_matrix(const S_quat &q){

                              //set 3x3 part
   SetRot(q);
                              //init the rest of matrix
                                                   m[0][3] = 0.0f;
                                                   m[1][3] = 0.0f;
                                                   m[2][3] = 0.0f;

   m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}

//----------------------------

void S_matrix::SetRot(const S_quat &q){

   float sx = q.v.x * 2.0f;
   float sy = q.v.y * 2.0f;
   float sz = q.v.z * 2.0f;
   float wx = q.s * sx;
   float wy = q.s * sy;
   float wz = q.s * sz;
   float xx = q.v.x * sx;
   float xy = q.v.x * sy;
   float xz = q.v.x * sz;
   float yy = q.v.y * sy;
   float yz = q.v.y * sz;
   float zz = q.v.z * sz;
   
   m[0][0] = 1.0f - yy - zz;
   m[0][1] = xy - wz;
   m[0][2] = xz + wy;
   m[1][0] = xy + wz;
   m[1][1] = 1.0f - xx - zz;
   m[1][2] = yz - wx;
   m[2][0] = xz - wy;
   m[2][1] = yz + wx;
   m[2][2] = 1.0f - xx - yy;
}

//----------------------------
//----------------------------
                              //vector class
float S_vector::AngleTo(const S_vector &v) const{

   float f = Magnitude() * v.Magnitude();
   if(f<MRG_ZERO) return 0.0f;
   float acos_val = Dot(v) / f;
   return (float)acos(::Min(::Max(acos_val, -1.0f), 1.0f));
}

//----------------------------

bool S_vector::Normalize(){
   float ax, ay, az, l;
   ax = I3DFabs(x);
   ay = I3DFabs(y);
   az = I3DFabs(z);
   if(ay > ax){
      if(az > ay){
         goto z_largest;
      }else{
                           //ay is largest
         if(ay<1e-30f)
            return false;
         l = 1.0f / ay;
         x *= l;
         z *= l;
         l = 1.0f / I3DSqrt(x*x + z*z + 1.0f);
         x *= l;
         //y = _copysign(l, y);
         y = (y<0.0f) ? -l : l;
         z *= l;
      }
   }else{
      if(az > ax){
z_largest:
                           //az is largest
         if(az<1e-30f)
            return false;
         l = 1.0f / az;
         x *= l;
         y *= l;
         l = 1.0f / I3DSqrt(x*x + y*y + 1.0f);
         x *= l;
         y *= l;
         //z = _copysign(l, z);
         z = (z<0.0f) ? -l : l;
      }else{
                           //ax is largest
         if(ax<1e-30f)
            return false;
         l = 1.0f / ax;
         y *= l;
         z *= l;
         l = 1.0f / I3DSqrt(y*y + z*z + 1.0f);
         //x = _copysign(l, x);
         x = (x<0.0f) ? -l : l;
         y *= l;
         z *= l;
      }
   }
                           //sanity test (remove when proved)
#ifdef _DEBUG
   //if(I3DFabs(1.0f-Square()) > 1e-6f){ __asm int 3 }
#endif
   return true;
}

//----------------------------

S_vector &S_vector::operator *=(const S_matrix &m){

#if defined _MSC_VER && defined USE_ASM

   __asm{
                              //eax = this
                              //edx = matrix
      mov eax, this
      mov edx, m

      fld dword ptr [eax+8]
      fld dword ptr [eax+4]
      fld dword ptr [eax+0]
      push eax                //local variable

                              //X
      fld st(0)
      fmul dword ptr [edx+0x00]
      fadd dword ptr [edx+0x30]
      fld st(2)
      fmul dword ptr [edx+0x10]
      faddp st(1), st
      fld st(3)
      fmul dword ptr [edx+0x20]
      faddp st(1), st
      fstp dword ptr [eax+0]
                              //Y
      fld st(0)
      fmul dword ptr [edx+0x04]
      fadd dword ptr [edx+0x34]
      fld st(2)
      fmul dword ptr [edx+0x14]
      faddp st(1), st
      fld st(3)
      fmul dword ptr [edx+0x24]
      faddp st(1), st
      fstp dword ptr [eax+4]
                              //Z
      fmul dword ptr [edx+0x08]
      fadd dword ptr [edx+0x38]
      fld st(1)
      fmul dword ptr [edx+0x18]
      faddp st(1), st
      fld st(2)
      fmul dword ptr [edx+0x28]
      faddp st(1), st
      fstp dword ptr [eax+8]

      fstp dword ptr[esp]
      fstp dword ptr[esp]

      pop eax                 //clean stack
   }

   return *this;
#else
   S_vector v;
   for(int i=0; i<3; i++)
      v[i] =
         f[0] * m.m[0][i] +
         f[1] * m.m[1][i] +
         f[2] * m.m[2][i] +
                m.m[3][i];
   return (*this=v);
#endif
}

//----------------------------

S_vector S_vector::operator *(const struct S_matrix &m) const{

#if defined _DEBUG && 0
   if(LogCallStack){
                              //call-stack logging
      t_call_stack cs;
      GetCallStack(cs);
      LogCallStack(&cs);
   }
#endif

   S_vector rtn;

#if defined _MSC_VER && defined USE_ASM

   __asm{
      mov eax, this
      mov edx, m
      lea esi, rtn

                              //X
      fld dword ptr [eax+0]
      fmul dword ptr [edx+0x00]
      fadd dword ptr [edx+0x30]
      fld dword ptr [eax+4]
      fmul dword ptr [edx+0x10]
      faddp st(1), st
      fld dword ptr [eax+8]
      fmul dword ptr [edx+0x20]
      faddp st(1), st
      fstp dword ptr [esi+0]
                              //Y
      fld dword ptr [eax+0]
      fmul dword ptr [edx+0x04]
      fadd dword ptr [edx+0x34]
      fld dword ptr [eax+4]
      fmul dword ptr [edx+0x14]
      faddp st(1), st
      fld dword ptr [eax+8]
      fmul dword ptr [edx+0x24]
      faddp st(1), st
      fstp dword ptr [esi+4]
                              //Z
      fld dword ptr [eax+0]
      fmul dword ptr [edx+0x08]
      fadd dword ptr [edx+0x38]
      fld dword ptr [eax+4]
      fmul dword ptr [edx+0x18]
      faddp st(1), st
      fld dword ptr [eax+8]
      fmul dword ptr [edx+0x28]
      faddp st(1), st
      fstp dword ptr [esi+8]
   }

#else //_MSC_VER

   for(int i=0; i<3; i++)
      rtn[i] =
         f[0] * m.m[0][i] +
         f[1] * m.m[1][i] +
         f[2] * m.m[2][i] +
                m.m[3][i];

#endif   //!_MSC_VER
   return rtn;
}

//----------------------------

S_vector S_vector::RotateByMatrix(const S_matrix &m) const{

   S_vector rtn;

#if defined _MSC_VER && defined USE_ASM

   __asm{
      mov eax, this
      mov edx, m
      lea esi, rtn

      fld dword ptr [eax]
      fmul dword ptr [edx+0x00]
      fld dword ptr [eax+4]
      fmul dword ptr [edx+0x10]
      faddp st(1), st
      fld dword ptr [eax+8]
      fmul dword ptr [edx+0x20]
      faddp st(1), st
      fstp dword ptr [esi]
      fld dword ptr [eax]
      fmul dword ptr [edx+0x04]
      fld dword ptr [eax+4]
      fmul dword ptr [edx+0x14]
      faddp st(1), st
      fld dword ptr [eax+8]
      fmul dword ptr [edx+0x24]
      faddp st(1), st
      fstp dword ptr [esi+4]
      fld dword ptr [eax]
      fmul dword ptr [edx+0x08]
      fld dword ptr [eax+4]
      fmul dword ptr [edx+0x18]
      faddp st(1), st
      fld dword ptr [eax+8]
      fmul dword ptr [edx+0x28]
      faddp st(1), st
      fstp dword ptr [esi+8]
   }

#else

   for(int i=0; i<3; i++)
      rtn[i] =
         f[0] * m.m[0][i] +
         f[1] * m.m[1][i] +
         f[2] * m.m[2][i];
#endif

   return rtn;
}

//----------------------------
//----------------------------

#pragma warning(push)
#pragma warning(disable:4701)
S_vectorw S_vectorw::operator *(const struct S_matrix &m) const{

   S_vectorw rtn;
   for(int i=0; i<4; i++)
      rtn[i] =
         f[0] * m.m[0][i] +
         f[1] * m.m[1][i] +
         f[2] * m.m[2][i] +
         f[3] * m.m[3][i];
   return rtn;
}
#pragma warning(pop)

//----------------------------
//----------------------------
                              //quaternions
//----------------------------

void S_quat::Make(const S_vector &axis, float angle){

   float mag = axis.Magnitude();
   if(mag>MRG_ZERO){
      float half_angle = angle * .5f;
      mag = (float)sin(half_angle) / mag;
      v = axis * mag;
      s = (float)cos(half_angle);
   }else{
      Identity();
   }
}

//----------------------------
static const int nxt[3] = {1, 2, 0};

static void MatrixToQuaternion(const S_matrix &m, S_quat &q){


#if 0
                              //really buggy version:
   S_vector scl = m.GetScale();
   if(scl[0]*scl[1]*scl[2] < MRG_ZERO){
      return;
   }
   scl[0] = 1.0f / scl[0];
   scl[1] = 1.0f / scl[1];
   scl[2] = 1.0f / scl[2];

   float trace = m(0, 0) * scl[0] + m(1, 1) * scl[1] + m(2, 2) * scl[2];

                              //check the diagonal
   if(trace > 0.0f){
      float ss = sqrt(trace + 1.0f);
      s = ss * .5f;
      ss = .5f / ss;

      v.x = (m.m[2][1] - m.m[1][2]) * ss;
      v.y = (m.m[0][2] - m.m[2][0]) * ss;
      v.z = (m.m[1][0] - m.m[0][1]) * ss;
   }else{
                              //diagonal is negative
      float q[4];
      int i = 0;
      if(m.m[1][1] > m.m[0][0]) i = 1;
      if(m.m[2][2] > m.m[i][i]) i = 2;
      int j = nxt[i];
      int k = nxt[j];

      float ss = sqrt ((m.m[i][i] - (m.m[j][j] + m.m[k][k])) + 1.0f);

      q[i] = ss * .5f;
      if(!IsAbsMrgZero(ss))
         ss = .5f / ss;

      q[3] = (m.m[j][k] - m.m[k][j]) * ss;
      q[j] = (m.m[i][j] + m.m[j][i]) * ss;
      q[k] = (m.m[i][k] + m.m[k][i]) * ss;

      v.x = q[0];
      v.y = q[1];
      v.z = q[2];
      s   = q[3];
   }

#else

                              //!!! buggy code: several matrices had problems !!!
   S_vector scl = m.GetScale();

   if(scl[0]*scl[1]*scl[2] < MRG_ZERO){
      q.Identity();                              
      return;
   }
   scl[0] = 1.0f / scl[0];
   scl[1] = 1.0f / scl[1];
   scl[2] = 1.0f / scl[2];

   float trace = m(0, 0) * scl[0] + m(1, 1) * scl[1] + m(2, 2) * scl[2];
   float f = .5f * (trace - 1.0f);
   float angle = -(float)acos(Max(-1.0f, Min(1.0f, f)));
   S_vector axis(m(1, 2) * scl[1] - m(2, 1) * scl[2],
      m(2, 0) * scl[2] - m(0, 2) * scl[0],
      m(0, 1) * scl[0] - m(1, 0) * scl[1]);
                              //now follows version of Make(axis, angle)
   float half_angle = angle * .5f;
   float ss = (float)sin(half_angle);
   float mag = axis.Magnitude();

   if(mag>MRG_ZERO){
      q.s = (float)cos(half_angle);
      mag = (1.0f / mag) * ss;
      q.v = axis * mag;
   }else{
      /*
      const float &m0 = m(0, 0);
      const float &m1 = m(1, 1);
      const float &m2 = m(2, 2);
                              //get greatest element (i)
      int i = bool(m0<m1 || m0<m2);
      if(i) i += bool(m1<m2);
      v.Zero();
      v[i] = ss;
      */
      if(trace > 0.0f){
         float ss = (float)sqrt(trace + 1.0f);
         q.s = ss * .5f;
         ss = .5f / ss;

         q.v.x = (m.m[2][1] - m.m[1][2]) * ss;
         q.v.y = (m.m[0][2] - m.m[2][0]) * ss;
         q.v.z = (m.m[1][0] - m.m[0][1]) * ss;
      }else{
                                 //agonal is negative
         float qq[4];
         int i = 0;
         if(m.m[1][1] > m.m[0][0]) i = 1;
         if(m.m[2][2] > m.m[i][i]) i = 2;
         int j = nxt[i];
         int k = nxt[j];

         float ss = (float)sqrt((m.m[i][i] - (m.m[j][j] + m.m[k][k])) + 1.0f);

         qq[i] = ss * .5f;
         if(!IsAbsMrgZero(ss))
            ss = .5f / ss;

         qq[3] = (m.m[j][k] - m.m[k][j]) * ss;
         qq[j] = (m.m[i][j] + m.m[j][i]) * ss;
         qq[k] = (m.m[i][k] + m.m[k][i]) * ss;

         q.v.x = qq[0];
         q.v.y = qq[1];
         q.v.z = qq[2];
         q.s   = qq[3];
                              //finally normalize the quaternion
         {
            float s = (q.v.Dot(q.v) + q.s*q.s);
            s = 1.0f / (float)sqrt(s);
            q.v *= s;
            q.s *= s;
         }
      }
   }

#endif
}

//----------------------------

S_quat::S_quat(const S_matrix &m){

   MatrixToQuaternion(m, *this);
}

//----------------------------

void S_quat::Make(const S_matrix &m){

   MatrixToQuaternion(m, *this);
}

//----------------------------

bool S_quat::SetDir(const S_vector &dir, float roll){

   S_matrix m;
   //if(!m.SetDir(dir, roll)) return false;
   m.SetDir(dir, roll);
   MatrixToQuaternion(m, *this);
   return true;
}

//----------------------------

bool S_quat::SetDir(const S_vector &dir, const S_vector &up){

   S_matrix m;
   //if(!m.SetDir(dir, up)) return false;
   m.SetDir(dir, up);
   MatrixToQuaternion(m, *this);
   return true;
}

//----------------------------

void S_quat::Inverse(S_vector &axis, float &angle) const{

   float sin_a;

   if(s >= 1.0f){
      angle = 0.0f;
      sin_a = 0.0f;
   }else
   if(s <= -1.0f){
      angle = PI * 2.0f;
      sin_a = 1.0f;
   }else{
      angle = (float)acos(s) * 2.0f;
      sin_a = (float)sin(angle * .5f);
   }
   if(sin_a >= MRG_ZERO){
      axis = v/sin_a;
      float len = axis.Dot(axis);
      if(len>=MRG_ZERO)
         axis /= (float)sqrt(len);
      else{
         axis = (axis.x) ? S_vector(1.0f, 0.0f, 0.0f) :
                (axis.y) ? S_vector(0.0f, 1.0f, 0.0f) :
                           S_vector(0.0f, 0.0f, 1.0f);
      }
   }else
      axis = S_vector(1.0f, 0.0f, 0.0f);
}

//----------------------------

S_quat S_quat::Slerp(const S_quat &q, float t, bool shorten) const{

   float cos_a = Dot(q);

   cos_a = Max(-1.0f, Min(1.0f, cos_a));
   float angle = (float)acos(cos_a);
   float sin_a = (float)sin(angle);
   if(I3DFabs(sin_a)<1e-6f)
      return (*this);

   float flipped = false;
                              //select shorter path
   if(shorten && cos_a < 0.0f){
      angle -= PI;
      flipped = true;
   }

   float inv_sin_a = 1.0f / sin_a;
   float c0 = (float)sin((1.0f - t) * angle) * inv_sin_a;
   float c1 = (float)sin(t * angle) * (flipped ? -inv_sin_a : inv_sin_a);

   S_quat ret;
   ret.v.x = v.x*c0 + q.v.x*c1;
   ret.v.y = v.y*c0 + q.v.y*c1;
   ret.v.z = v.z*c0 + q.v.z*c1;
   ret.s   =   s*c0 +   q.s*c1;
   return ret;
}

//----------------------------

// Compute 1/sqrt(s) using a tangent line approximation.
inline float isqrt_approx_in_neighborhood(float s){
#define NEIGHBORHOOD 0.959066f
#define SCALE 1.000311f
   //const float ADDITIVE_CONSTANT = SCALE / sqrt(NEIGHBORHOOD);
#define ADDITIVE_CONSTANT 1.02143514576f
   //const float FACTOR = SCALE * (-0.5 / (NEIGHBORHOOD * sqrt(NEIGHBORHOOD)));
#define FACTOR -0.532515565f
   return ADDITIVE_CONSTANT + (s - NEIGHBORHOOD) * FACTOR;
}

//----------------------------
// Normalize a quaternion using the above approximation.
inline void FastNormalize(S_quat &q){

   float s = q.Square();
   float k = isqrt_approx_in_neighborhood(s);

   if(s <= 0.91521198){
      k *= isqrt_approx_in_neighborhood(k * k * s);
      if(s <= 0.65211970){
         k *= isqrt_approx_in_neighborhood(k * k * s);
      }
   }
   q *= k;
}

//----------------------------

void S_quat::SlerpFast(const S_quat &q, float t, S_quat &result) const{

   /*
   float inv_t = 1.0f - t;
                              //make sure we're taking shorter path to destination
   float angle_between_axes = v.Dot(q.v);
   if(angle_between_axes < 0.0f){
      inv_t = -inv_t;
   }
   result = (*this) * inv_t + q * t;
   */
   result = (*this) * (1.0f - t) + q * t;
   //FastNormalize(result);
   result.Normalize();
}

//----------------------------

void S_quat::Scale(float scale){

#if 0
   *this = S_quat(1, 0, 0, 0).Slerp(*this, s);
#else
   float cos_a = s;
   cos_a = Max(-1.0f, Min(1.0f, cos_a));
   float angle = (float)acos(cos_a);
   float sin_a = (float)sin(angle);
   if(I3DFabs(sin_a)<1e-6f)
      return;
   float flipped = false;
   if(cos_a < 0.0f){
      angle -= PI;
      flipped = true;
   }
   float inv_sin_a = 1.0f / sin_a;
   float c0 = (float)sin((1.0f - scale) * angle) * inv_sin_a;
   float c1 = (float)sin(scale * angle) * (flipped ? -inv_sin_a : inv_sin_a);

   s = c0 + s * c1;
   v *= c1;
#endif
}

//----------------------------

void S_quat::ScaleFast(float t){

   //result = (*this) * (t-1) + q * t;
   operator *=(t);
   if(s > 0.0f)
      s += (1.0f - t);
   else
      s -= (1.0f - t);
   //FastNormalize(*this);
   Normalize();
}

//----------------------------

S_quat S_quat::RotateByMatrix(const struct S_matrix &m) const{

   S_vector axis;
   float angle;
   Inverse(axis, angle);
   return S_quat(axis.RotateByMatrix(m), angle);
}

//----------------------------
//----------------------------
                              //code from Graphics Gems

bool S_plane::IntersectionLine(const S_plane &pl, S_vector &xpt, S_vector &xdir) const{

                              //inverse of 2x2 matrix determinant
   float invdet;

                              //get line normal
   xdir = normal.Cross(pl.normal);
                              //holds the squares of the coordinates of xdir
   S_vector dir2 = xdir * xdir;

   if(dir2.z>dir2.y && dir2.z>dir2.x && dir2.z>MRG_ZERO){
                              //get a point on the XY plane
      invdet = 1.0f / xdir.z;
                              // solve < pl1.x * xpt.x + pl1.y * xpt.y = - pl1.d >
                              //       < pl2.x * xpt.x + pl2.y * xpt.y = - pl2.d >
      xpt.x =    normal.y * pl.d - pl.normal.y *    d;
      xpt.y = pl.normal.x *    d -    normal.x * pl.d;
      xpt.z = 0.0f;
   }else
   if(dir2.y > dir2.x && dir2.y > MRG_ZERO) {
                              //get a point on the XZ plane
      invdet = 1.0f / xdir.y;
                              // solve < pl1.x * xpt.x + pl1.z * xpt.z = -pl1.w >
                              //       < pl2.x * xpt.x + pl2.z * xpt.z = -pl2.w >
      xpt.x = pl.normal.z *    d -    normal.z * pl.d;
      xpt.y = 0.0f;
      xpt.z =    normal.x * pl.d - pl.normal.x *    d;
   }else
   if(dir2.x>MRG_ZERO){
                              //get a point on the YZ plane
      invdet = 1.0f / xdir.x;
                              // solve < pl1.y * xpt.y + pl1.z * xpt.z = - pl1.w >
                              //       < pl2.y * xpt.y + pl2.z * xpt.z = - pl2.w >
      xpt.x = 0.0f;
      xpt.y =    normal.z * pl.d - pl.normal.z *    d;
      xpt.z = pl.normal.y *    d -    normal.y * pl.d;
   }else
      return false;           //no intersection point

   xpt *= invdet;
   xdir *= 1.0f / (float)sqrt(dir2.Sum());

   return true;
}

//----------------------------
//----------------------------

bool LineIntersection(const S_vector &p1, const S_vector &dir1,
   const S_vector &p2, const S_vector &dir2, float &u1, float &u2){

   S_vector dir3(p1 - p2);

   float d_1_1 = dir1.Dot(dir1);
   float d_2_2 = dir2.Dot(dir2);
   float d_1_2 = dir1.Dot(dir2);

   float denom = d_1_1 * d_2_2 - d_1_2 * d_1_2;
                              //too small values expected
   if(IsAbsMrgZero(denom))
      return false;

   float d_2_3 = dir2.Dot(dir3);
   float d_1_3 = dir1.Dot(dir3);

   float numer = d_2_3 * d_1_2 - d_1_3 * d_2_2;
   u1 = numer / denom;
   u2 = (d_2_3 + d_1_2 * u1) / d_2_2;

   return true;
}

//----------------------------
//----------------------------

void TransformVertexArray(const S_vector *verts_in, dword in_stride, dword num_verts,
   S_vector *verts_out, dword out_stride, const S_matrix &mat){

   assert(verts_in != verts_out);

   if(!num_verts)
      return;

#if defined _MSC_VER && defined USE_ASM

   __asm{
      push ecx
                              //setup registers:
                              //eax - out_stride
                              //ebx - mat
                              //ecx - loop
                              //edx - free
                              //esi - verts_in
                              //edi - verts_out
                              //st(2) = mat(3, 0)
                              //st(1) = mat(3, 1)
                              //st(0) = mat(3, 2)
      mov ebx, mat
      mov ecx, num_verts
      fld dword ptr[ebx+0x30]
      mov esi, verts_in
      fld dword ptr[ebx+0x34]
      mov edi, verts_out
      fld dword ptr[ebx+0x38]
      mov eax, out_stride
      push eax                //make local variable
      
   ALIGN 16
   main_loop:
                              //x
      fld dword ptr  [esi + 0x00]
      fmul dword ptr [ebx + 0x00]
      fld dword ptr  [esi + 0x04]
      fmul dword ptr [ebx + 0x10]
      faddp st(1), st
      fld dword ptr  [esi + 0x08]
      fmul dword ptr [ebx + 0x20]
      faddp st(1), st
      fadd dword ptr  [ebx + 0x30]
      fstp dword ptr [edi + 0x00]

                              //y
      fld dword ptr  [esi + 0x00]
      fmul dword ptr [ebx + 0x04]
      fld dword ptr  [esi + 0x04]
      fmul dword ptr [ebx + 0x14]
      faddp st(1), st
      fld dword ptr  [esi + 0x08]
      fmul dword ptr [ebx + 0x24]
      faddp st(1), st
      fadd dword ptr  [ebx + 0x34]
      fstp dword ptr [edi + 0x04]

                              //z
      fld dword ptr  [esi + 0x00]
      fmul dword ptr [ebx + 0x08]
      fld dword ptr  [esi + 0x04]
      fmul dword ptr [ebx + 0x18]
      faddp st(1), st
      fld dword ptr  [esi + 0x08]
      fmul dword ptr [ebx + 0x28]
      faddp st(1), st
      add esi, in_stride
      fadd dword ptr [ebx + 0x38]
      fstp dword ptr [edi + 0x08]

                              //next vertex
      add edi, eax

                              //loop
      dec ecx
      jne main_loop
                              //free fpu
      fstp dword ptr[esp]
      fstp dword ptr[esp]
      fstp dword ptr[esp]

                              //clean stack
      pop eax

      pop ecx
   }

#else //_MSC_VER

   for(int i=num_verts; i--; ){
      *verts_out = *verts_in * mat;

      verts_in = (S_vector*)(((byte*)verts_in)+in_stride);
      verts_out = (S_vector*)(((byte*)verts_out)+out_stride);
   }
#endif
}

//----------------------------
//----------------------------
                              //random umber generator
dword GetTickTime();
static dword rnd_seed = GetTickTime();

//----------------------------
// Get random 32-bit number.
//----------------------------
static dword GetRandomDWord(){

   /*
   static dword last_ret_val = 0xa5a5a5a5;
   static dword last_val;
   static dword last_pseudo_val;
   LARGE_INTEGER li;
   QueryPerformanceCounter(&li);
   dword ah = li.LowPart;
   word offs = (word)last_val & 0xfffc;
   dword val = (ah << 8) + li.LowPart;
   val ^= _rotl(last_pseudo_val = (last_pseudo_val*0x41c64e6d+0x3039), 16);
   offs ^= val;
   val = _rotl(val, 16) ^ (li.LowPart >> 8) ^ _rotl(last_pseudo_val = (last_pseudo_val*0x41c64e6d+0x3039), 16);
   last_val = offs ^ val;
   last_ret_val = val;
   return _rotl(val, 15) ^ last_ret_val;
   */
   dword hi = ((rnd_seed = rnd_seed * 214013l + 2531011l) >> 16) << 16;
   return ((rnd_seed = rnd_seed * 214013l + 2531011l) >> 16) | hi;
}

//----------------------------

inline int FindLastBit(dword val){

   int base = 0;
   if(val&0xffff0000){
      base = 16;
      val >>= 16;
   }
   if(val&0x0000ff00){
      base += 8;
      val >>= 8;
   }
   if(val&0x000000f0){
      base += 4;
      val >>= 4;
   }
   static const int lut[] = {-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};
   return base + lut[val];
}

//----------------------------

S_int_random::S_int_random(int max){
   
   switch(max){
   case 0: i = GetRandomDWord(); return;
   case 1: i = 0; return;
   }
                                       //setup mask - max. possible number
                                       //with all low bits set
   dword mask = (1 << (FindLastBit(max - 1) + 1)) - 1;
   if(!mask) mask = 0xffffffff;
   dword val;
   do{
                                       //get 32-bit random number
      val = GetRandomDWord();
                                       //loop until it fits to the max
   }while((val&mask) >= (dword)max);
   i = val & mask;
}

//----------------------------

const float R_ffff = 1.0f / 65535.0f;

S_float_random::S_float_random(){
   f = (float)(GetRandomDWord() & 0xffff) * R_ffff;
}

//----------------------------

S_float_random::S_float_random(float max){
   f = (float)(GetRandomDWord() & 0xffff) * max * R_ffff;
}

//----------------------------
//----------------------------
