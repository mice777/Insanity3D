#ifndef __C_SPLINE_HPP
#define __C_SPLINE_HPP


/*----------------------------------------------------------------\
   Copyright (c) Lonely Cat Games  All rights reserved.

   Template bezier curve class

Abstract:

 Spline template, holding info about all objects describing
 curve and computing an object on arbitrary position.

Functions:
   void Init(const T[3]);
   void Init(const T&, const T&, const T&);
      - initialize class
   void Evaluate(float t, T&);
      - evaluate vector at position t (t is in range 0.0 to 1.0)

Notes:
 Supports any classes for which following functions are defined:

 T& operator * (float f) const;
 T& operator + (const T&) const;
|\----------------------------------------------------------------*/

template<class T>
class C_spline{
   T p[3];
public:
//----------------------------
// Initialize points of curve, from an array.
   void Init(const T p1[3]){
      for(int i=0; i<3; i++) p[i] = p1[i];
   }

//----------------------------
// Initialize 4 points of curve, by 4 values.
   void Init(const T &p0, const T &p1, const T &p2){
      p[0] = p0, p[1] = p1, p[2] = p2;
   }

//----------------------------
// Evaluate point on curve. The value 't' is normalized position on curve (0 .. 1),
// 'p_ret' is value which contains result after return.
   void Evaluate(float t, T &p_ret) const{

      float t_inv = 1.0f - t;
      p_ret = p[0] * (t_inv * t_inv) +
         p[1] * (2.0f * t * t_inv) +
         p[2] * t * t;
   }
};

//----------------------------

#endif