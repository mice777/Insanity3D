#ifndef __EASE_HPP_
#define __EASE_HPP_
/*
   Ease interpolator.
   Written by Michal Bacik, spring 1998.

   Abstract:

   Creates interpolation in time range 0.0f ... 1.0f. In and out easiness may be set up independently.
   Complies with 3D Studio v.4.0 and MAX easy interpolators.

   Updated 24.7.2002: Utilization of one dimensional bezier curve to compute easiness.
*/

#include <math\bezier.hpp>

class C_ease_interpolator: protected C_bezier_curve<float>{
public:
//----------------------------
//Default constructor - sets maximal interpolation
//for both 'from' and 'to' ends.
   inline C_ease_interpolator(){
      Setup(1.0f, 1.0f);
   }
   inline C_ease_interpolator(float in, float out){
      Setup(in, out);
   }

//----------------------------
//Setup of interpolator - sets 'from' and 'to'
// ends. Value 0.0f means linear interpolation,
// value 1.0f means lin. acceleration interpolation.
   inline void Setup(float ip, float op){
      C_bezier_curve<float>::Init(0.0f, .3333f * (1.0f - ip), 1.0f - .3333f * (1.0f - op), 1.0f);
   }

//----------------------------
//Getting interpolated value on time t. 't' must be in range 0.0f to 1.0f.
   inline float operator[](float t) const{
      float ret;
      C_bezier_curve<float>::Evaluate(t, ret);
      return ret;
   }
};

//----------------------------

#endif
