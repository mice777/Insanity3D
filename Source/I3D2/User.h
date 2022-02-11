#include "frame.h"

//----------------------------

class I3D_user: public I3D_frame{
   dword add_data;

   I3D_user &operator =(const I3D_user&);
   ~I3D_user();
public:
   I3D_user(PI3D_driver);

   I3DMETHOD(Duplicate)(CPI3D_frame);
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

                              //additional user data
   I3DMETHOD_(void,SetData2)(dword dw){ add_data=dw; }
   I3DMETHOD_(dword,GetData2)() const{ return add_data; }
};

//----------------------------

#ifdef _DEBUG
inline PI3D_user I3DCAST_USER(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_USER ? NULL : static_cast<PI3D_user>(f); }
inline CPI3D_user I3DCAST_CUSER(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_USER ? NULL : static_cast<CPI3D_user>(f); }
#else
inline PI3D_user I3DCAST_USER(PI3D_frame f){ return static_cast<PI3D_user>(f); }
inline CPI3D_user I3DCAST_CUSER(CPI3D_frame f){ return static_cast<CPI3D_user>(f); }
#endif

//----------------------------
