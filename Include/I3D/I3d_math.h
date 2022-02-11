/*--------------------------------------------------------
   Copyright (c) Lonely Cat Games  All rights reserved.

   File: I3D_math.h     Version: 2.0
   Content: Insanity 3D math classes include file
--------------------------------------------------------*/

#ifndef __MATH_3D_
#define __MATH_3D_
#pragma once

#include <rules.h>
#include <math.h>

#define PI 3.1415926535897932384626433832795029f   //PI definition
#define MRG_ZERO 1e-8f        //marginal zero - thresh values, under which number may be considered zero

//----------------------------

                              //checking against marginal zero
#if defined _MSC_VER

#define MRG_ZERO_BITMASK 0x322BCC77
#pragma warning(disable:4035) //disable func returns no value, we're returning in eax

//Return true if f is less then MRG_ZERO. interval(-inf, MRG_ZERO)
inline bool IsMrgZeroLess(float f){
   __asm{
      xor eax, eax
      cmp f, MRG_ZERO_BITMASK
      setl al
   }
}

//Return true if f is above MRG_ZERO. interval (-MRG_ZERO, MRG_ZERO).
inline bool IsAbsMrgZero(float f){
   __asm{
      fld f
      fabs
      fstp f
      xor eax, eax
      cmp f, MRG_ZERO_BITMASK
      setl al
   }
}
inline float I3DFabs(float f){
   *(dword*)&f &= 0x7fffffff;
   return f;
}
#pragma warning(default:4035)
#else                         //_MSC_VER
                              //compare float value againt near-zero
inline bool IsMrgZeroLess(float f){ return (f<MRG_ZERO); }
                              //compare absolute float value againt near-zero
inline bool IsAbsMrgZero(float f){ return ((float)fabs(f)<MRG_ZERO); }
inline float I3DFabs(float f){ return (float)fabs(f); }
#endif                        //!_MSC_VER

inline float I3DSqrt(float f){ return (float)sqrt(f); }

//----------------------------

//----------------------------
// rectangle - specifying top-left corner and size of rectangle
struct S_rect{
   int x, y, sx, sy;
   S_rect(){}
   S_rect(int _x, int _y, int _sx, int _sy):
      x(_x), y(_y), sx(_sx), sy(_sy)
   {}
//----------------------------
// Get center of rectangle.
   inline int CenterX() const{ return x + sx/2; }
   inline int CenterY() const{ return y + sy/2; }
//----------------------------
// Get right/bottom point of rectangle (point is 1 pixel out of rectangle area).
   inline int Right() const{ return x + sx; }
   inline int Bottom() const{ return y + sy; }

//----------------------------
// Expand/compact rectangle by given pixels on all 4 sides.
   inline void Expand(int n = 1){
      x -= n; y -= n; sx += 2*n; sy += 2*n;
   }
   inline void Compact(int n = 1){
      x += n; y += n; sx -= 2*n; sy -= 2*n;
   }

//----------------------------
// Check if rectange is empty (width or height is zero or less).
   inline bool IsEmpty() const{ return (sx<=0 || sy<=0); }

//----------------------------
// Set this rectangle to intersection of 2 rectangles. Resulting rect size may be also negative.
   void SetIntersection(const S_rect &rc1, const S_rect &rc2);

//----------------------------
// Set this rectangle to union of 2 rectangles.
   void SetUnion(const S_rect &rc1, const S_rect &rc2);

//----------------------------
// Center rectangle to provided rectangle size.
   void Center(int size_x, int size_y);
};


//----------------------------
                              //2d vector
struct S_vector2{
#pragma warning(push)
#pragma warning(disable:4201)
   union{
      struct{
         float x, y;
      };
      float f[2];
   };
#pragma warning(pop)
   inline S_vector2(){}
   inline S_vector2(float x1, float y1): x(x1), y(y1){}
   inline float &operator [](int i){ return f[i]; }
   inline const float &operator [](int i) const{ return f[i]; }
   bool operator ==(const S_vector2 &v) const{ return(x==v.x && y==v.y); }
   bool operator !=(const S_vector2 &v) const{ return !(*this==v); }
   inline S_vector2 operator *(float f) const{ return S_vector2(x*f, y*f); }
   inline S_vector2 &operator *=(float f){
      x *= f, y *= f;
      return *this;
   }
   inline S_vector2 operator -() const{ return S_vector2(-x, -y); }
   inline S_vector2 operator /(float f) const{ return S_vector2(x/f, y/f); }
   inline S_vector2 &operator /=(float f){
      x /= f, y /= f;
      return *this;
   }
   inline S_vector2 operator +(const S_vector2& v) const{ return S_vector2(x+v.x, y+v.y); }
   inline S_vector2 &operator +=(const S_vector2& v){
      x += v.x, y += v.y;
      return *this;
   }
   inline S_vector2 operator -(const S_vector2& v) const{ return S_vector2(x-v.x, y-v.y); }
   inline S_vector2& operator -=(const S_vector2& v){
      x -= v.x, y -= v.y;
      return *this;
   }
   inline void Zero(){ x=0.0f, y=0.0f; }
   inline float Magnitude() const{ return (float)sqrt(x*x + y*y); }

//----------------------------
// Normalize vector - make its length to be one. If the vector is too small to normalize, the returned value is false.
   bool Normalize(){
      float m=Magnitude();
      if(m<MRG_ZERO) return false;
      operator /=(m);
      return true;
   }
   inline float Dot(const S_vector2 &v) const{ return x*v.x + y*v.y; }
   inline float Square() const{ return x*x + y*y; }
   inline float Sum() const{ return x+y; }
   inline void Invert(){ x=-x, y=-y; }
   inline bool IsNull() const{ return ((x*x + y*y) < (MRG_ZERO*MRG_ZERO)); }
   float AngleTo(const S_vector2&) const;

//----------------------------
   inline float PositionOnLine(const S_vector2 &p, const S_vector2 &dir) const{
      float len_2 = dir.Square();
      if(len_2<MRG_ZERO) return 0.0f;
      return dir.Dot((*this) -p) / len_2;
   }

//----------------------------
// >0 - right, 0 - on line, <0 - left
//line is given by point and direction vector
//using line equation: ax+by+c, where
//(a,b,c) is vector vertical to line
//(x,y) is any point on the plane
//note: x goes right & y goes down
   float DistanceToLine(const S_vector2 &p, const S_vector2 &dir) const{
      S_vector2 n(-dir.y, dir.x); //get right-turned vector
      n.Normalize();
      return Dot(n) - p.Dot(n);
   }
   inline S_vector2 operator *(const struct S_matrix2&) const;
   inline S_vector2 &operator *=(const struct S_matrix2&);

                              //Get intersection point of 2 lines, both specified by 
                              // a starting point and a direction vector.
                              //If point is found, the return value is true, otherwise 
                              // the lines are parallel or coincident.
   bool GetIntersection(const S_vector2 &p1, const S_vector2 &d1,
      const S_vector2 &p2, const S_vector2 &d2){

      float den = d2.y * d1.x - d2.x * d1.y;
      if(IsAbsMrgZero(den)) return false;
      float u = (d2.x * (p1.y - p2.y) - d2.y * (p1.x - p2.x)) / den;
      x = p1.x + d1.x * u;
      y = p1.y + d1.y * u;
      return true;
   }
};

//----------------------------

struct S_matrix2{
   float m[2][2];

   inline float &operator()(int y, int x){ return m[y][x]; }
   inline const float &operator()(int y, int x) const{ return m[y][x]; }
   inline S_vector2 &operator()(int r){ return (S_vector2&)m[r][0]; }
   inline const S_vector2 &operator()(int r) const{ return (S_vector2&)m[r][0]; }
   void Identity(){
      m[0][0] = 0.0f;
      m[0][1] = 0.0f;
      m[1][0] = 0.0f;
      m[1][1] = 1.0f;
   }
   void Zero(){
      m[0][0] = 0.0f;
      m[1][1] = 0.0f;
      m[0][1] = 0.0f;
      m[1][0] = 0.0f;
   }
                              //base is at (0, 1), PI/2 is at (-1, 0)
   void SetDir(float a){
      m[0][0] = m[1][1] = (float)cos(a);
      m[1][0] = -(m[0][1] = (float)sin(a));
   }
   S_matrix2 operator *(const S_matrix2 &m1){
      S_matrix2 r;
      r.m[0][0] = m[0][0]*m1.m[0][0] + m[0][1]*m1.m[1][0];
      r.m[0][1] = m[0][0]*m1.m[0][1] + m[0][1]*m1.m[1][1];
      r.m[1][0] = m[1][0]*m1.m[0][0] + m[1][1]*m1.m[1][0];
      r.m[1][1] = m[1][0]*m1.m[0][1] + m[1][1]*m1.m[1][1];
      return r;
   }
};

inline S_vector2 S_vector2::operator *(const struct S_matrix2 &m) const{
   return S_vector2(x*m(0, 0)+y*m(1, 0), x*m(0, 1)+y*m(1, 1));
}

inline S_vector2 &S_vector2::operator *=(const struct S_matrix2 &m){
   x = x*m(0, 0) + y*m(1, 0);
   y = x*m(0, 1) + y*m(1, 1);
   return *this;
}

//----------------------------
                              //3D vector
struct S_vector{
   union{
      struct{
         float x, y, z;
      };
      float f[3];
   };
   inline S_vector(){}
   inline S_vector(float x1, float y1, float z1): x(x1), y(y1), z(z1) {}

   inline float &operator [](int i){ return f[i]; }
   inline const float &operator [](int i) const{ return f[i]; }
   inline bool operator ==(const S_vector &v) const{ return(x==v.x && y==v.y && z==v.z); }
   inline bool operator !=(const S_vector &v) const{ return !(*this==v); }
   inline S_vector operator *(float f) const{ return S_vector(x*f, y*f, z*f); }
   inline S_vector &operator *=(const S_vector &v){
      x *= v.x, y *= v.y, z *= v.z;
      return (*this);
   }
   inline S_vector &operator *=(float f){
      x *= f, y *= f, z *= f;
      return (*this);
   }
   S_vector &operator *=(const struct S_matrix&);
   S_vector operator *(const struct S_matrix&) const;
   inline S_vector operator -() const{ return S_vector(-x, -y, -z); }
   inline S_vector operator /(float f) const{
      f = 1.0f / f;
      return S_vector(x*f, y*f, z*f);
   }
   inline S_vector &operator /=(float f){
      f = 1.0f / f;
      x *= f, y *= f, z *= f;
      return *this;
   }
   inline S_vector operator +(const S_vector &v) const{ return S_vector(x+v.x, y+v.y, z+v.z); }
   inline S_vector &operator +=(const S_vector &v){
      x += v.x, y += v.y, z += v.z;
      return *this;
   }
   inline S_vector operator -(const S_vector &v) const{ return S_vector(x-v.x, y-v.y, z-v.z); }
   inline S_vector &operator -=(const S_vector &v){
      x -= v.x, y -= v.y, z -= v.z;
      return *this;
   }

//----------------------------
// Rotate vector by matrix (3x3 part of matrix used only).
   S_vector RotateByMatrix(const struct S_matrix&) const;
   inline S_vector operator %(const struct S_matrix &m) const{
      return RotateByMatrix(m);
   }
   inline S_vector &operator %=(const struct S_matrix &m){
      *this = RotateByMatrix(m);
      return *this;
   }

   inline void Zero(){ x=0.0f, y=0.0f, z=0.0f; }

   inline S_vector operator *(const S_vector &v) const{ return S_vector(x*v.x, y*v.y, z*v.z); }

   inline float Dot(const S_vector &v) const{ return x*v.x + y*v.y + z*v.z; }
   inline float Square() const{ return x*x + y*y + z*z; }

   inline float Magnitude() const{
      float sq = Square();
      if(IsMrgZeroLess(sq)) return 0.0f;
      return (float)sqrt(sq);
   }

//----------------------------
// Normalize vector - make its length to be one. If the vector is too small to normalize, the returned value is false.
   bool Normalize();

   inline S_vector Cross(const S_vector &v) const{
      return S_vector(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x);
   }
   inline float Sum() const{ return x+y+z; }
   inline void Invert(){ x=-x, y=-y, z=-z; }
   inline bool IsNull() const{ return ((x*x + y*y + z*z) < (MRG_ZERO*MRG_ZERO)); }
                              // >0 - sharp, 0 - PI/2, <0 - not sharp
   float AngleTo(const S_vector&) const;
                              // >0 - in front of, 0 - on, <0 - behind
                              //plane is given by point and normal
                              //using plane equation: ax+by+cz+d, where
                              //(a,b,c) is plane normal (length=1 !) and
                              //(x,y,z) is any point on the plane
   float DistanceToPlane(const S_vector &p, const S_vector &n) const{
      return Dot(n) - p.Dot(n);
   }
   inline float DistanceToPlane(const struct S_plane&) const;
//----------------------------
// Compute position of vector to line. The returned value is a multiplier, which
// gives closest position to vector, when used in formula: p + dir * multiplier.
   inline float PositionOnLine(const S_vector &p, const S_vector &dir) const{
      float len_2 = dir.Square();
      if(len_2<MRG_ZERO) return 0.0f;
      return dir.Dot((*this) -p) / len_2;
   }

//----------------------------
//Compute distance to line.
// Line is given by a starting point and a direction vector.
// If direction vector is zero, the return value is negative
// to mark an error.
   inline float DistanceToLine(const S_vector &p, const S_vector &dir) const{
      float len_2 = dir.Square();
      if(len_2<MRG_ZERO) return -1e+16f;
      float u = dir.Dot((*this) -p) / len_2;
      return ((p + dir * u) - *this).Magnitude();
   }
   inline void GetNormal(const S_vector &u1, const S_vector &v1, const S_vector &w1){
      float ux = w1.x - v1.x, uy = w1.y - v1.y, uz = w1.z - v1.z;
      float vx = u1.x - v1.x, vy = u1.y - v1.y, vz = u1.z - v1.z;
      x = uy * vz - uz * vy;
      y = uz * vx - ux * vz;
      z = ux * vy - uy * vx;
   }
   inline S_vector Min(const S_vector &v) const{
      return S_vector(x<v.x ? x : v.x, y<v.y ? y : v.y, z<v.z ? z : v.z);
   }
   inline S_vector Max(const S_vector &v) const{
      return S_vector(x>v.x ? x : v.x, y>v.y ? y : v.y, z>v.z ? z : v.z);
   }
   inline S_vector &Minimal(const S_vector &v){
      x = (x<v.x) ? x : v.x;
      y = (y<v.y) ? y : v.y;
      z = (z<v.z) ? z : v.z;
      return *this;
   }
   inline S_vector &Maximal(const S_vector &v){
      x = (x>v.x) ? x : v.x;
      y = (y>v.y) ? y : v.y;
      z = (z>v.z) ? z : v.z;
      return *this;
   }
   inline float GetBrightness() const{
      return x*.3f + y*.6f + z*.1f;
   }
};

typedef S_vector *PS_vector;
typedef const S_vector *CPS_vector;

//----------------------------

struct S_normal: public S_vector{
   inline S_normal(){}

                              //Construct unit-length normal from 2 direction vectors (using cross-product).
   inline S_normal(const S_vector &u, const S_vector &v){
      x = u.y * v.z - u.z * v.y;
      y = u.z * v.x - u.x * v.z;
      z = u.x * v.y - u.y * v.x;
      if(!Normalize())
         Zero();
   }

                              //Construct unit-length normal from vector.
   inline S_normal(const S_vector &v):
      S_vector(v)
   {
      if(!Normalize())
         Zero();
   }
};

typedef S_normal *PS_normal;
typedef const S_normal *CPS_normal;

//----------------------------
                              //homogeneous vector, containing w
struct S_vectorw: public S_vector{
   float w;
   S_vectorw(){}
   S_vectorw(float x1, float y1, float z1, float w1): S_vector(x1, y1, z1), w(w1){}
   S_vectorw(const S_vector &v): S_vector(v), w(1.0f){}
   S_vectorw operator *(const struct S_matrix&) const;

   inline bool operator ==(const S_vectorw &v) const{ return(S_vector::operator==(v) && w==v.w); }
   inline bool operator !=(const S_vectorw &v) const{ return !(*this==v); }
   inline S_vectorw operator *(float f) const{ return S_vectorw(x*f, y*f, z*f, w*f); }
   inline S_vectorw &operator *=(const S_vectorw &v){
      x *= v.x, y *= v.y, z *= v.z, w *= v.w;
      return (*this);
   }
   inline S_vectorw &operator *=(float f){
      x *= f, y *= f, z *= f, w *= f;
      return (*this);
   }
   inline S_vectorw operator -() const{ return S_vectorw(-x, -y, -z, -w); }
   inline S_vectorw operator /(float f) const{
      f = 1.0f / f;
      return S_vectorw(x*f, y*f, z*f, w*f);
   }
   inline S_vectorw &operator /=(float f){
      f = 1.0f / f;
      x *= f, y *= f, z *= f, w *= f;
      return *this;
   }
   inline S_vectorw operator +(const S_vectorw &v) const{ return S_vectorw(x+v.x, y+v.y, z+v.z, w+v.w); }
   inline S_vectorw &operator +=(const S_vectorw &v){
      x += v.x, y += v.y, z += v.z, w += v.w;
      return *this;
   }
   inline S_vectorw operator -(const S_vectorw &v) const{ return S_vectorw(x-v.x, y-v.y, z-v.z, w-v.w); }
   inline S_vectorw &operator -=(const S_vectorw &v){
      x -= v.x, y -= v.y, z -= v.z, w -= v.w;
      return *this;
   }

   inline void Zero(){ x=0.0f, y=0.0f, z=0.0f; w=0.0f;}

   inline S_vectorw operator *(const S_vectorw &v) const{ return S_vectorw(x*v.x, y*v.y, z*v.z, w*v.w); }

   inline float Dot(const S_vectorw &v) const{ return x*v.x + y*v.y + z*v.z + w*v.w; }
   inline float Square() const{ return x*x + y*y + z*z + w*w; }

   inline float Magnitude() const{
      float sq = Square();
      if(IsMrgZeroLess(sq)) return 0.0f;
      return (float)sqrt(sq);
   }

   inline float Sum() const{ return x+y+z+w; }
   inline void Invert(){ x=-x, y=-y, z=-z, w=-w; }
   inline bool IsNull() const{ return ((x*x + y*y + z*z + w*w) < (MRG_ZERO*MRG_ZERO)); }
   inline S_vectorw Min(const S_vectorw &v) const{
      return S_vectorw(x<v.x ? x : v.x, y<v.y ? y : v.y, z<v.z ? z : v.z, w<v.w ? w : v.w);
   }
   inline S_vectorw Max(const S_vectorw &v) const{
      return S_vectorw(x>v.x ? x : v.x, y>v.y ? y : v.y, z>v.z ? z : v.z, w>v.w ? w : v.w);
   }
   inline S_vectorw &Minimal(const S_vectorw &v){
      x = (x<v.x) ? x : v.x;
      y = (y<v.y) ? y : v.y;
      z = (z<v.z) ? z : v.z;
      w = (w<v.w) ? w : v.w;
      return *this;
   }
   inline S_vectorw &Maximal(const S_vectorw &v){
      x = (x>v.x) ? x : v.x;
      y = (y>v.y) ? y : v.y;
      z = (z>v.z) ? z : v.z;
      w = (w>v.w) ? w : v.w;
      return *this;
   }
};

//----------------------------
struct S_quat;
                              //simplified projection matrix, only specific non-zero values are kept
struct S_projection_matrix{
   float m0_0, m1_1, m2_2, m3_2;
   bool orthogonal;
};

const S_matrix &I3DGetIdentityMatrix();
                              //matrix 4x4
struct S_matrix{
   float m[4][4];
   S_matrix(){}   

   inline float &operator()(int y, int x){ return m[y][x]; }
   inline const float &operator()(int y, int x) const{ return m[y][x]; }
   inline S_vector &operator()(int r){ return (S_vector&)m[r][0]; }
   inline const S_vector &operator()(int r) const{ return (S_vector&)m[r][0]; }
   void Identity();
   void Zero();
   bool Invert();
   void Transpose();
   void CopyTransposed(const S_matrix &m);

//----------------------------
// Set rotation part of matrix from given direction vector and rotation around this vector.
// If the direction vector is too small, the returned value is false.
   bool SetDir(const S_vector &dir, float roll = 0.0f);

//----------------------------
// Set rotation part of matrix from given direction vector and rotation around this vector.
// If any of input vectors is too small, the returned value is false.
   bool SetDir(const S_vector &dir, const S_vector &up);

//----------------------------
// Multiplication assignment.
   S_matrix &operator *=(const S_matrix&);
   inline S_matrix &operator *=(const S_quat &q){ return operator *=(S_matrix(q)); }

   S_matrix operator *(const S_matrix&) const;
   S_matrix Mult4X4(const S_matrix&) const;
//----------------------------
// Special optimized 4x4 matrix multiply by projection matrix.
   S_matrix MultByProj(const S_projection_matrix &m_proj) const;

//----------------------------
// Rotate matrix by matrix (3x3 part of matrices used only).
   S_matrix operator %(const S_matrix&) const;
   S_matrix &operator %=(const S_matrix &m){
      *this = operator %(m);
      return *this;
   }

   S_vector GetScale() const;

//----------------------------
// Make simple rotations around basic 3 axes, the rest of matrix is set to identity.
   void RotationX(float angle);
   void RotationY(float angle);
   void RotationZ(float angle);

//----------------------------
// Construct matrix from quaternion. The fourth row is set to (0, 0, 0, 1).
#ifdef _MSC_VER
   //explicit
#endif
      S_matrix(const S_quat&);

//----------------------------
// Set 3x3 rotation part of matrix to be rotation represented by given quaternion.
// This method doesn't change any other part of the matrix.
   void SetRot(const S_quat &rot);
   
//----------------------------
// Get inversed matrix.
   inline S_matrix operator ~() const{
      S_matrix rtn = *this;
      rtn.Invert();
      return rtn;
   }

//----------------------------
// Make resulting matrix by multiplaying two input matrices, using 3x4 parts.
   S_matrix &Make(const S_matrix&, const S_matrix&);

//----------------------------
// Make resulting matrix by multiplaying two input matrices, using 4x4 parts.
   S_matrix &Make4X4(const S_matrix&, const S_matrix&);

//----------------------------
// Same as Make4X4, but transpose matrix after transformations.
   S_matrix &Make4X4Transposed(const S_matrix&, const S_matrix&);
};

typedef S_matrix *PS_matrix;
typedef const S_matrix *CPS_matrix;

//----------------------------
                              //3D quaternion
struct S_quat{
   float s;
   S_vector v;
                              //constructors
   inline S_quat(){}
   inline S_quat(float s1, float x1, float y1, float z1): s(s1){
      v.x = x1, v.y = y1, v.z = z1;
   }
   inline
#ifdef _MSC_VER
      //explicit
#endif
      S_quat(float s1, const S_vector &v1): s(s1), v(v1) {}

   inline
#ifdef _MSC_VER
      //explicit
#endif
      S_quat(const S_vector &axis, float angle){ Make(axis, angle); }
#ifdef _MSC_VER
   //explicit
#endif
      S_quat(const S_matrix&);

   void Make(const S_vector &axis, float angle);
   void Make(const S_matrix &m);

   S_matrix ToMatrix() const;

//----------------------------
// Set rotation from given direction vector and rotation around this vector.
// If the direction vector is too small, the returned value is false.
   bool SetDir(const S_vector &dir, float roll = 0.0f);

//----------------------------
// Set rotation from given direction vector and rotation around this vector.
// If any of input vectors is too small, the returned value is false.
   bool SetDir(const S_vector &dir, const S_vector &up);

//----------------------------
   void Inverse(S_vector &axis, float &angle) const;
   //S_matrix RotationMatrix() const;
   inline S_quat operator +(const S_quat &q) const{ return S_quat(s+q.s, v+q.v); }
   inline S_quat operator -(const S_quat &q) const{ return S_quat(s-q.s, v-q.v); }
   inline S_quat operator -() const{ return S_quat(-s, -v); }
   inline S_quat operator ~() const{ return S_quat(s, -v); }
   inline S_quat operator *(const S_quat &q) const{
      return S_quat(s * q.s - v.Dot(q.v), q.v * s + v * q.s + v.Cross(q.v));
   }

//----------------------------
// Make indentity quaternion.
   inline void Identity(){ s = 1.0f; v.Zero(); }

//----------------------------
// Get direction vector from quaternion.
   inline S_vector GetDir() const{
      float sx = v.x * 2.0f;
      float sy = v.y * 2.0f;
      float sz = v.z * 2.0f;
      return S_vector((v.x*sz) - (s*sy), (v.y*sz) + (s*sx), 1.0f - (v.x*sx) - (v.y*sy));
   }

//----------------------------
// Get angle of rotation which this quaterion represents.
   inline float GetAngle() const{ return (float)acos(Max(-1.0f, Min(1.0f, s))) * 2.0f; }

//----------------------------
// Assignment operators.
   inline S_quat &operator +=(const S_quat &q){ s += q.s; v += q.v; return *this; }
   inline S_quat &operator *=(float f){ s *= f; v *= f; return *this; }
   inline S_quat &operator *=(const S_quat &q){
      float s1 = s;
      s = s * q.s - v.Dot(q.v);
      v = q.v * s1 + v * q.s + v.Cross(q.v);
      return *this;
   }
   inline S_quat &operator *=(const S_matrix &m){ return operator *=(S_quat(m)); }

   inline S_quat operator *(float f) const{
      return S_quat(s * f, v * f);
   }

   inline float Square() const{ return v.Square() + s*s; }

   inline float Magnitude() const{
      float sq = Dot(*this);
      if(IsMrgZeroLess(sq)) return 0.0f;
      return (float)sqrt(sq);
   }
//----------------------------
// Normalize quaternion - make its length to be one. If the quaternion is too small to normalize,
// the returned value is false.
   bool Normalize(){
      float m = Magnitude();
      if(IsMrgZeroLess(m))
         return false;
      operator *=(1.0f / m);
      return true;
   }

   inline bool operator ==(const S_quat &q) const{ return(s==q.s && v==q.v); }
   inline float Dot(const S_quat q) const{ return s*q.s + v.x*q.v.x + v.y*q.v.y + v.z*q.v.z; }
   S_quat Slerp(const S_quat &q, float t, bool shorten=false) const;
   void SlerpFast(const S_quat &q, float t, S_quat &result) const;

//----------------------------
// Scale quaternion - this is the same as linear interpolation (Slerp) with unit quaternion.
   void Scale(float);
   void ScaleFast(float);

   S_quat RotateByMatrix(const struct S_matrix&) const;
};

typedef S_quat *PS_quat;
typedef const S_quat *CPS_quat;

//----------------------------
                              //plane - parametric
struct S_plane{
   S_vector normal;
   float d;
   S_plane(){}
   S_plane(const S_vector &n, float d1): normal(n), d(d1){}
   S_plane(float x, float y, float z, float d1): normal(x, y, z), d(d1){}
   S_plane(const S_vector &v0, const S_vector &v1, const S_vector &v2){
      ComputePlane(v0, v1, v2);
   }

//----------------------------
// Compute plane from provided 3 points lying on plane.
   void ComputePlane(const S_vector &v0, const S_vector &v1, const S_vector &v2){
      normal.GetNormal(v0, v1, v2);
      normal.Normalize();
      d = -normal.Dot(v0);
   }

//----------------------------
// Compute intersection with another plane.
// Return: xpt=point on line
//    xdir=normalized line vector
   bool IntersectionLine(const S_plane&, S_vector &xpt, S_vector &xdir) const;

//----------------------------
// Get intersection of line and plane - return ratio on line, where intersection occured.
   bool IntersectionPosition(const S_vector &pos, const S_vector &dir, float &ret) const{
      float f = normal.Dot(dir);
      if(I3DFabs(f)<MRG_ZERO){
         ret = 0.0f;
         return false;
      }
      ret = -(normal.Dot(pos)+d) / f;
      return true;
   }

//----------------------------
// Get intersection of line and plane - return intersection point.
   inline bool Intersection(const S_vector &pos, const S_vector &dir, S_vector &ret) const{
      float f = 0.0f;
      if(!IntersectionPosition(pos, dir, f))
         return false;
      ret = pos + dir * f;
      return true;
   }

//----------------------------
// Compute plane transformed by a matrix.
   S_plane operator *(const S_matrix &m) const{
      S_plane rtn;
      rtn.normal = normal % m;
      rtn.normal.Normalize();
      S_vector p = (-normal * d) * m;
      rtn.d = -p.Dot(rtn.normal);
      return rtn;
   }

   S_plane operator %(const S_matrix &m) const{
      S_plane rtn;
      rtn.normal = normal % m;
      rtn.normal.Normalize();
      S_vector p = (-normal * d) % m;
      rtn.d = -p.Dot(rtn.normal);
      return rtn;
   }


//----------------------------
// Compute inversive plane.
   inline S_plane operator -() const{ return S_plane(-normal.x, -normal.y, -normal.z, -d); }

//----------------------------
// Multiply all elements of plane by scalar.
   S_plane &operator *=(float f){
      normal *= f;
      d *= f;
      return *this;
   }

//----------------------------
// Invert plane.
   inline void Invert(){
      normal.Invert();
      d = -d;
   }
};

typedef S_plane *PS_plane;
typedef const S_plane *CPS_plane;

//----------------------------

inline float S_vector::DistanceToPlane(const S_plane &pl) const{
   return x*pl.normal.x + y*pl.normal.y + z*pl.normal.z + pl.d;
}

//----------------------------

struct S_int_random{
   int i;
   S_int_random(int max);
   inline operator int() const{ return i; }
};

//----------------------------

struct S_float_random{
   float f;
   S_float_random();
   S_float_random(float max);
   inline operator float() const{ return f; }
};

//----------------------------
                              //math functions
//----------------------------
// Get intersection (closest connection) of 2 lines in 3D.
// The returned u1 and u2 are positions on line1 and line2 where 2 closest points are located.
// If lines are perpendicular, the call fails.
bool LineIntersection(const S_vector &p1, const S_vector &dir1,
   const S_vector &p2, const S_vector &dir2,
   float &u1, float &u2);

//----------------------------

#endif
