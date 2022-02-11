/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

#include <IPhysics.h>
#include "matrix.h"

//----------------------------
/* matrix multiplication. all matrices are stored in standard row format.
 * the digit refers to the argument that is transposed:
 *   0:   A = B  * C   (sizes: A:p*r B:p*q C:q*r)
 *   1:   A = B' * C   (sizes: A:p*r B:q*p C:q*r)
 *   2:   A = B  * C'  (sizes: A:p*r B:p*q C:r*q)
 * case 1,2 are equivalent to saying that the operation is A=B*C but
 * B or C are stored in standard column format.
 */
/*
void dMultiply0 (dReal *A, const dReal *B, const dReal *C, int p, int q, int r){
  int i,j,k,qskip,rskip,rpad;
  assert(A && B && C && p>0 && q>0 && r>0);
  qskip = dPAD(q);
  rskip = dPAD(r);
  rpad = rskip - r;
  dReal sum;
  const dReal *b,*c,*bb;
  bb = B;
  for (i=p; i; i--) {
    for (j=0 ; j<r; j++) {
      c = C + j;
      b = bb;
      sum = 0;
      for (k=q; k; k--, c+=rskip) sum += (*(b++))*(*c);
      *(A++) = sum; 
    }
    A += rpad;
    bb += qskip;
  }
}
*/

//----------------------------

bool dFactorCholesky(dReal *A, int n){

   int i,j,k,nskip;
   dReal sum,*a,*b,*aa,*bb,*cc,*recip;
   assert(n > 0 && A);
   nskip = dPAD (n);
   recip = (dReal*)alloca(n * sizeof(dReal));
   aa = A;
   for (i=0; i<n; i++) {
      bb = A;
      cc = A + i*nskip;
      for (j=0; j<i; j++) {
         sum = *cc;
         a = aa;
         b = bb;
         for (k=j; k; k--) sum -= (*(a++))*(*(b++));
         *cc = sum * recip[j];
         bb += nskip;
         cc++;
      }
      sum = *cc;
      a = aa;
      for (k=i; k; k--, a++) sum -= (*a)*(*a);
      if (sum <= 0.0f)
         return false;
      *cc = I3DSqrt(sum);
      recip[i] = 1.0f / *cc;
      aa += nskip;
   }
   return true;
}

//----------------------------
/* solve for x: L*L'*x = b, and put the result back into x.
 * L is size n*n, b is size n*1. only the lower triangle of L is considered.
 */
/*
void dSolveCholesky (const dReal *L, dReal *b, int n){

  int i,k,nskip;
  dReal sum,*y;
  assert(n > 0 && L && b);
  nskip = dPAD (n);
  y = (dReal*) alloca (n*sizeof(dReal));
  for (i=0; i<n; i++) {
    sum = 0;
    for (k=0; k < i; k++) sum += L[i*nskip+k]*y[k];
    y[i] = (b[i]-sum)/L[i*nskip+i];
  }
  for (i=n-1; i >= 0; i--) {
    sum = 0;
    for (k=i+1; k < n; k++) sum += L[k*nskip+i]*b[k];
    b[i] = (y[i]-sum)/L[i*nskip+i];
  }
}
*/

//----------------------------
/* compute the inverse of the n*n positive definite matrix A and put it in
 * Ainv. this is not especially fast. this returns 1 on success (A was
 * positive definite) or 0 on failure (not PD).
 */
/*
bool dInvertPDMatrix (const dReal *A, dReal *Ainv, int n){

   int i,j,nskip;
   dReal *L,*x;
   assert(n > 0 && A && Ainv);
   nskip = dPAD (n);
   L = (dReal*)alloca(nskip*n*sizeof(dReal));
   memcpy(L, A, nskip*n*sizeof(dReal));
   x = (dReal*)alloca(n*sizeof(dReal));
   if(!dFactorCholesky(L, n))
      return false;
                              //make sure all padding elements set to 0
   dSetZero(Ainv, n*nskip);
   for(i=0; i<n; i++){
      for(j=0; j<n; j++)
         x[j] = 0;
      x[i] = 1;
      dSolveCholesky (L,x,n);
      for(j=0; j<n; j++)
         Ainv[j*nskip+i] = x[j];
   }
   return true;  
}
*/

//----------------------------

/***** this has been replaced by a faster version
void dSolveL1T (const dReal *L, dReal *b, int n, int nskip)
{
  int i,j;
  assert(L && b && n >= 0 && nskip >= n);
  dReal sum;
  for (i=n-2; i>=0; i--) {
    sum = 0;
    for (j=i+1; j<n; j++) sum += L[j*nskip+i]*b[j];
    b[i] -= sum;
  }
}
*/

//----------------------------

static void dVectorScale(dReal *a, const dReal *d, int n){
   assert(a && d && n >= 0);
   for(int i=0; i<n; i++) a[i] *= d[i];
}

//----------------------------

void dSolveLDLT(const dReal *L, const dReal *d, dReal *b, int n, int nskip){
   assert(L && d && b && n > 0 && nskip >= n);
   dSolveL1 (L,b,n,nskip);
   dVectorScale (b,d,n);
   dSolveL1T (L,b,n,nskip);
}

//----------------------------
/* given an L*D*L' factorization of an n*n matrix A, return the updated
 * factorization L2*D2*L2' of A plus the following "top left" matrix:
 *
 *    [ b a' ]     <-- b is a[0]
 *    [ a 0  ]     <-- a is a[1..n-1]
 *
 *   - L has size n*n, its leading dimension is nskip. L is lower triangular
 *     with ones on the diagonal. only the lower triangle of L is referenced.
 *   - d has size n. d contains the reciprocal diagonal elements of D.
 *   - a has size n.
 * the result is written into L, except that the left column of L and d[0]
 * are not actually modified. see ldltaddTL.m for further comments. 
 */
static void dLDLTAddTL(dReal *L, dReal *d, const dReal *a, int n, int nskip){

   int j,p;
   dReal *W1,*W2,W11,W21,alpha1,alpha2,alphanew,gamma1,gamma2,k1,k2,Wp,ell,dee;
   assert(L && d && a && n > 0 && nskip >= n);
   
   if (n < 2) return;
   W1 = (dReal*)alloca (n*sizeof(dReal));
   W2 = (dReal*)alloca (n*sizeof(dReal));
   
   W1[0] = 0;
   W2[0] = 0;
   for(j=1; j<n; j++)
      W1[j] = W2[j] = a[j] * M_SQRT1_2;
   W11 = ((0.5)*a[0]+1)*M_SQRT1_2;
   W21 = ((0.5)*a[0]-1)*M_SQRT1_2;
   
   alpha1=1;
   alpha2=1;
   
   dee = d[0];
   alphanew = alpha1 + (W11*W11)*dee;
   if(!IsAbsMrgZero2(alphanew))
      dee /= alphanew;
   else{
      dee = 0.0f;
   }
   gamma1 = W11 * dee;
   dee *= alpha1;
   alpha1 = alphanew;
   alphanew = alpha2 - (W21*W21)*dee;
   if(!IsAbsMrgZero2(alphanew))
      dee /= alphanew;
   else{
      dee = 0.0f;
   }
   gamma2 = W21 * dee;
   alpha2 = alphanew;
   k1 = 1.0f - W21*gamma1;
   k2 = W21*gamma1*W11 - W21;
   for(p=1; p<n; p++){
      Wp = W1[p];
      ell = L[p*nskip];
      W1[p] =    Wp - W11*ell;
      W2[p] = k1*Wp +  k2*ell;
   }
   
   for (j=1; j<n; j++) {
      dee = d[j];
      alphanew = alpha1 + (W1[j]*W1[j])*dee;
      if(!IsAbsMrgZero2(alphanew))
         dee /= alphanew;
      else{
         dee = 0.0f;
      }
      gamma1 = W1[j] * dee;
      dee *= alpha1;
      alpha1 = alphanew;
      alphanew = alpha2 - (W2[j]*W2[j])*dee;
      if(!IsAbsMrgZero2(alphanew))
         dee /= alphanew;
      else{
         dee = 0.0f;
      }
      gamma2 = W2[j] * dee;
      dee *= alpha2;
      d[j] = dee;
      alpha2 = alphanew;
      
      k1 = W1[j];
      k2 = W2[j];
      for (p=j+1; p<n; p++) {
         ell = L[p*nskip+j];
         Wp = W1[p] - k1 * ell;
         ell += gamma1 * Wp;
         W1[p] = Wp;
         Wp = W2[p] - k2 * ell;
         ell -= gamma2 * Wp;
         W2[p] = Wp;
         L[p*nskip+j] = ell;
      }
   }
}

//----------------------------
/* given an n*n matrix A (with leading dimension nskip), remove the r'th row
 * and column by moving elements. the new matrix will have the same leading
 * dimension. the last row and column of A are untouched on exit.
 */
static void dRemoveRowCol(dReal *A, int n, int nskip, int r){

   int i;
   assert(A && n > 0 && nskip >= n && r >= 0 && r < n);
   if(r >= n-1)
      return;
   if(r > 0){
      for(i=0; i<r; i++)
         memmove (A+i*nskip+r,A+i*nskip+r+1,(n-r-1)*sizeof(dReal));
      for(i=r; i<(n-1); i++)
         memcpy (A+i*nskip,A+i*nskip+nskip,r*sizeof(dReal));
   }
   for(i=r; i<(n-1); i++)
      memcpy (A+i*nskip+r,A+i*nskip+nskip+r+1,(n-r-1)*sizeof(dReal));
}

//----------------------------

// macros for dLDLTRemove() for accessing A - either access the matrix
// directly or access it via row pointers. we are only supposed to reference
// the lower triangle of A (it is symmetric), but indexes i and j come from
// permutation vectors so they are not predictable. so do a test on the
// indexes - this should not slow things down too much, as we don't do this
// in an inner loop.

#define _GETA(i,j) (A[i][j])
//#define _GETA(i,j) (A[(i)*nskip+(j)])
#define GETA(i,j) ((i > j) ? _GETA(i,j) : _GETA(j,i))

//----------------------------

void dLDLTRemove(dReal **A, const int *p, dReal *L, dReal *d, int n1, int n2, int r, int nskip){

   int i;
   assert(A && p && L && d && n1 > 0 && n2 > 0 && r >= 0 && r < n2 &&
      n1 >= n2 && nskip >= n1);

#ifdef _DEBUG
   for (i=0; i<n2; i++) assert(p[i] >= 0 && p[i] < n1);
#endif
   
   if(r==n2-1){
      return;		// deleting last row/col is easy
   }else
   if(r==0){
      dReal *a = (dReal*) alloca (n2 * sizeof(dReal));
      for (i=0; i<n2; i++) a[i] = -GETA(p[i],p[0]);
      a[0] += 1.0f;
      dLDLTAddTL (L,d,a,n2,nskip);
   }else{
      dReal *t = (dReal*) alloca (r * sizeof(dReal));
      dReal *a = (dReal*) alloca ((n2-r) * sizeof(dReal));
      for(i=0; i<r; i++){
         if(!IsAbsMrgZero2(d[i]))
            t[i] = L[r*nskip+i] / d[i];
         else
            t[i] = 0.0f;
      }
      for(i=0; i<(n2-r); i++)
         a[i] = FastDot(L+(r+i)*nskip,t,r) - GETA(p[r+i],p[r]);
      a[0] += 1.0f;
      dLDLTAddTL (L + r*nskip+r, d+r, a, n2-r, nskip);
   }
   
                              //snip out row/column r from L and d
   dRemoveRowCol (L,n2,nskip,r);
   if(r < (n2-1))
      memmove (d+r,d+r+1,(n2-r-1)*sizeof(dReal));
}

//----------------------------

#if 0

float __stdcall FastDot(const float *a, const float *b, dword n){
   
   float sum = 0.0f;
   while(n >= 2){
      float p0 = a[0];
      float q0 = b[0];
      float m0 = p0 * q0;
      float p1 = a[1];
      float q1 = b[1];
      float m1 = p1 * q1;
      sum += m0;
      sum += m1;
      a += 2;
      b += 2;
      n -= 2;
   }
   assert(n < 2);
   if(n){
      sum += *a * *b;
      ++a;
      ++b;
   }
   return sum;
}

#else
//----------------------------

__declspec(naked) float __stdcall FastDot(const float *a, const float *b, dword n){

   static const float f_zero = 0.0f;
   __asm{
                              //regs:
                              // ESI = a
                              // EDI = b
                              // ecx = n
      push esi
      push edi
      push ecx
      mov esi, [esp+4+12]
      mov edi, [esp+8+12]
      mov ecx, [esp+12+12]

      fld f_zero
                              //make 4-multiplies
      shr ecx, 2
      jz no_4
   l4:
      fld dword ptr[esi]
      fmul dword ptr[edi]
      faddp st(1), st

      fld dword ptr[esi+4]
      fmul dword ptr[edi+4]
      faddp st(1), st

      fld dword ptr[esi+8]
      fmul dword ptr[edi+8]
      faddp st(1), st

      fld dword ptr[esi+12]
      fmul dword ptr[edi+12]
      faddp st(1), st

      add esi, 0x10
      add edi, 0x10

      dec ecx
      jnz l4

   no_4:
      test dword ptr[esp+12+12], 2
      jz no_2

      fld dword ptr[esi]
      fmul dword ptr[edi]
      faddp st(1), st

      fld dword ptr[esi+4]
      fmul dword ptr[edi+4]
      faddp st(1), st

      add esi, 8
      add edi, 8

   no_2:

      test dword ptr[esp+12+12], 1
      jz no_1

      fld dword ptr[esi]
      fmul dword ptr[edi]
      faddp st(1), st
   no_1:

      pop ecx
      pop edi
      pop esi
      ret 12
   }
}

#endif

//----------------------------
