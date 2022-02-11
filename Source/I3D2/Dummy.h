#include "frame.h"

//----------------------------

class I3D_dummy: public I3D_frame{
   ~I3D_dummy();
public:
   I3D_dummy(PI3D_driver);
   I3D_dummy &operator =(const I3D_dummy&);
                              //overridden virtual functions
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(DebugDraw)(PI3D_scene scene) const{
      Draw1(scene, true);
      return I3D_OK;
   }

   void Draw1(PI3D_scene, bool strong) const;
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   //I3DMETHOD_(const I3D_bbox*,GetBBox)() const;
   //I3DMETHOD_(void,SetBBox)(const I3D_bbox &bb);
};

//----------------------------

#ifdef _DEBUG
inline PI3D_dummy I3DCAST_DUMMY(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_DUMMY ? NULL : static_cast<PI3D_dummy>(f); }
inline CPI3D_dummy I3DCAST_CDUMMY(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_DUMMY ? NULL : static_cast<CPI3D_dummy>(f); }
#else
inline PI3D_dummy I3DCAST_DUMMY(PI3D_frame f){ return static_cast<PI3D_dummy>(f); }
inline CPI3D_dummy I3DCAST_CDUMMY(CPI3D_frame f){ return static_cast<CPI3D_dummy>(f); }
#endif

//----------------------------
