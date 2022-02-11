#ifndef _IPH_COMMON_H_
#define _IPH_COMMON_H_

#include "error.h"
#include <insanity\assert.h>
#include <I3D\I3D_math.h>

//----------------------------

#define REF_COUTNER dword ref; \
   public: \
   virtual dword AddRef(){ return ++ref; } \
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; } \
   private:

#define REF_COUTNER1 \
   public: \
   virtual dword AddRef(){ return ++ref; } \
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; } \
   private:


//----------------------------

#if defined(WIN32) && (defined(_MSC_VER) || defined(MINGW))

static union{
   unsigned char __c[4];
   float __f;
} __ode_huge_valf = {{0,0,0x80,0x7f}};

#define _INFINITY4 (__ode_huge_valf.__f)

#else

#define _INFINITY4 HUGE_VALF

#endif


#define dInfinity _INFINITY4

inline bool IsAbsMrgZero2(float f){ return ((float)fabs(f)<1e-30); }


/* the efficient alignment. most platforms align data structures to some
 * number of bytes, but this is not always the most efficient alignment.
 * for example, many x86 compilers align to 4 bytes, but on a pentium it is
 * important to align doubles to 8 byte boundaries (for speed), and the 4
 * floats in a SIMD register to 16 byte boundaries. many other platforms have
 * similar behavior. setting a larger alignment can waste a (very) small
 * amount of memory. NOTE: this number must be a power of two.
 */

const dword EFFICIENT_ALIGNMENT = 16;

/* constants */

/* pi and 1/sqrt(2) are defined here if necessary because they don't get
 * defined in <math.h> on some platforms (like MS-Windows)
 */

#define M_SQRT1_2 0.7071067811865475244008443621048490f


typedef float dReal;


/* round an integer up to a multiple of 4, except that 0 and 1 are unmodified
 * (used to compute matrix leading dimensions)
 */
#define dPAD(a) (((a) > 1) ? ((((a)-1)|3)+1) : (a))

#define DENSITY_MULTIPLIER 1.0f

//----------------------------
// Set 3x3 rotation part of matrix to such matrix, which produces a vector's cross-product when multiplied.
inline void SetCrossMatrix(S_matrix &m, const S_vector &v){

   m(0)[1] = -v.z;
   m(0)[2] =  v.y;
   m(1)[0] =  v.z;
   m(1)[2] = -v.x;
   m(2)[0] = -v.y;
   m(2)[1] =  v.x;
}

//----------------------------

enum{
   dContactMu2		= 0x001,
   dContactFDir1		= 0x002,
   dContactBounce	= 0x004,
   dContactSoftERP	= 0x008,
   dContactSoftCFM	= 0x010,
   dContactMotion1	= 0x020,
   dContactMotion2	= 0x040,
   dContactSlip1		= 0x080,
   dContactSlip2		= 0x100,
   
   dContactApprox0	= 0x0000,
   dContactApprox1_1	= 0x1000,
   dContactApprox1_2	= 0x2000,
   dContactApprox1	= 0x3000
};

//----------------------------

struct dSurfaceParameters{
                              //must always be defined
   dword mode;
   dReal mu;
   
                              //only defined if the corresponding flag is set in mode
   dReal mu2;
   dReal bounce;
   dReal bounce_vel;
   dReal soft_erp;
   dReal soft_cfm;
   dReal motion1,motion2;
   dReal slip1,slip2;
};

//----------------------------
                              //contact info set by collision functions
struct dContactGeom{
   S_vector pos;
   S_vector normal;
   float depth;
};

//----------------------------
                              //contact info used by contact joint
struct dContact{
   dSurfaceParameters surface;
   dContactGeom geom;
   S_vector fdir1;
};

//----------------------------

// Implicit conversion if float to int, with rounding to nearest.
#if defined _MSC_VER
inline int FloatToInt(float f){
   __asm{
      fld f
      fistp f 
   }
   return *(int*)&f;
}

#else

inline int FloatToInt(float f){
   return (int)f;
}

#endif

//----------------------------

#endif
