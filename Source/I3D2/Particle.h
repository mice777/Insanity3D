#include "visual.h"

//----------------------------

class I3D_particle: public I3D_visual{
public:
   I3D_particle(PI3D_driver d):
      I3D_visual(d)
   {}
   virtual void Tick(int time) = 0;
};

typedef I3D_particle *PI3D_particle;
typedef const I3D_particle *CPI3D_particle;

//----------------------------
