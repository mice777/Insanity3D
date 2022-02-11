#ifndef __C_BEZIER_HPP
#define __C_BEZIER_HPP

/*----------------------------------------------------------------\
   Template bezier curve class, copyright (c) 1998 Michal Bacik

Abstract:

 Bezier curve template, holding info about all objects describing
 bezier curve and computing an object on arbitrary position.

Functions:
   void Init(const T[4]);
   void Init(const T&, const T&, const T&, const T&);
      - initialize class
   void Evaluate(float t, T&);
      - evaluate vector at position t (t is in range 0.0 to 1.0)

Notes:
 Supports any classes for which following functions are defined:

 T& operator * (float f) const;
 T& operator + (const T&) const;
|\----------------------------------------------------------------*/


template<class T>
class C_bezier_curve{
   T p[4];
public:
//----------------------------
// Initialize 4 points of curve, from an array.
   void Init(const T p1[4]){
      for(int i=0; i<4; i++) p[i] = p1[i];
   }

//----------------------------
// Initialize 4 points of curve, by 4 values.
   void Init(const T &p0, const T &p1, const T &p2, const T &p3){
      p[0] = p0, p[1] = p1, p[2] = p2, p[3] = p3;
   }

//----------------------------
// Get access to control points.
   const T &operator [](int i) const{ return p[i]; }

//----------------------------
// Evaluate point on curve. The value 't' is normalized position on curve (0 .. 1),
// 'p_ret' is value which contains result after return.
   void Evaluate(float t, T &p_ret) const{

      float t_sqr, t_inv_sqr, t_inv = 1.0f - t;
      float val[4];
      val[0] = t_inv * (t_inv_sqr = t_inv * t_inv);
      val[1] = 3.0f * t * t_inv_sqr;
      val[2] = 3.0f * t_inv * (t_sqr = t * t);
      val[3] = t * t_sqr;

      p_ret = p[0] * val[0] +
           p[1] * val[1] +
           p[2] * val[2] +
           p[3] * val[3];
   }
};
//----------------------------

#endif
