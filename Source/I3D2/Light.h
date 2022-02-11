#ifndef __LIGHT_H_
#define __LIGHT_H_

#include "frame.h"

//----------------------------

class I3D_light: public I3D_frame{

#define LFLG_MODE_MASK     0x00ff   //mask in which light mode is stored
#define LFLG_DIRTY         0x0100   //light needs update params - set by SetUpdate
#define LFLG_COLOR_DIRTY   0x0200   //color is changed and need update

   I3D_LIGHTTYPE ltype;
   C_vector<PI3D_sector> light_sectors;
   mutable dword lflags;              //lower

   dword last_render_time;


   friend I3D_driver;         //creation
   friend I3D_sector;         //light in sector
   friend I3D_scene;          //rendering

   I3D_light(PI3D_driver);
public:
   float power;
   S_vector color;
   float range_n, range_f;
   float cone_in, cone_out;

                              //--- run-time values ---
   S_vector normalized_dir;
   dword dw_color;            //dword RGB value for fog and ambient
   float range_n_scaled, range_f_scaled;
   float range_f_scaled_2;
   float range_delta_scaled;
                              //spotlight:
   float inner_cos, outer_cos, outer_tan;

   ~I3D_light();
   const I3D_light& operator =(const I3D_light&);


   inline dword GetLightFlags() const{ return lflags; }
                              //vertex light:

//----------------------------
                              //update cached changes of light, called
   void UpdateLight();

//----------------------------
// Mark light dirty and notify all sectors about it.
   void MarkDirty() const;

                              //mark visuals of 1 or all sector(s) for update
   void MarkVisuals(PI3D_sector = NULL);

   inline I3D_LIGHTTYPE GetLightType1() const{ return ltype; }
   inline const C_vector<PI3D_sector> &GetLightSectorsVector() const{ return light_sectors; }
   inline bool IsFogType() const{
      return (ltype==I3DLIGHT_FOG || ltype==I3DLIGHT_POINTFOG || ltype==I3DLIGHT_LAYEREDFOG);
   }
                              //for amb and fog
   inline void ForceUpdate(){ lflags |= LFLG_DIRTY; }

//----------------------------
// Draw light - for debugging purposes. If 'vf' is not NULL, light's icon is clipped
// against view frustum and fast rejected if out.
   void Draw1(PI3D_scene, const struct S_view_frustum *vf, bool strong) const;

public:
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(DebugDraw)(PI3D_scene scene) const{
      Draw1(scene, NULL, true);
      return I3D_OK;
   }
   virtual void PropagateDirty(){
      MarkDirty();
      I3D_frame::PropagateDirty();
   }
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
                              //override I3D_frame's functions
   I3DMETHOD_(void,SetOn)(bool);

   I3DMETHOD(SetLightType)(I3D_LIGHTTYPE);
   I3DMETHOD_(void,SetColor)(const S_vector&);
   I3DMETHOD_(void,SetPower)(float);
   I3DMETHOD(SetMode)(dword);
   I3DMETHOD_(void,SetRange)(float r_near, float r_far);
   I3DMETHOD(SetCone)(float inner, float outer);
   I3DMETHOD(SetSpecularColor)(const S_vector &v){ return I3DERR_UNSUPPORTED; }
   I3DMETHOD(SetSpecularPower)(float f){ return I3DERR_UNSUPPORTED; }

   I3DMETHOD_(dword,NumLightSectors)() const{ return light_sectors.size(); }
   I3DMETHOD_(PI3D_sector const*,GetLightSectors)() const{ return &light_sectors.front(); }

   I3DMETHOD_(I3D_LIGHTTYPE,GetLightType)() const{ return ltype; }
   I3DMETHOD_(const S_vector&,GetColor)() const{ return color; }
   I3DMETHOD_(float,GetPower)() const{ return power; }
   I3DMETHOD_(dword,GetMode)() const{ return lflags&LFLG_MODE_MASK; }
   I3DMETHOD_(void,GetRange)(float &r_near, float &r_far) const{
      r_near = range_n;
      r_far = range_f;
   }
   I3DMETHOD_(void,GetCone)(float &inner, float &outer) const{
      inner = cone_in;
      outer = cone_out;
   }
   I3DMETHOD_(const S_vector&,GetSpecularColor)() const{ static S_vector v(0, 0, 0); return v; }
   I3DMETHOD_(float,GetSpecularPower)() const{ return 0; }
};

//----------------------------
                              //internal lighting flags, in addiction to those declared in i3d.h
#define I3D_LIGHT_MATRIX_BLEND1  0x10000

//----------------------------

#ifdef _DEBUG
inline PI3D_light I3DCAST_LIGHT(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_LIGHT ? NULL : static_cast<PI3D_light>(f); }
inline CPI3D_light I3DCAST_CLIGHT(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_LIGHT ? NULL : static_cast<CPI3D_light>(f); }
#else
inline PI3D_light I3DCAST_LIGHT(PI3D_frame f){ return static_cast<PI3D_light>(f); }
inline CPI3D_light I3DCAST_CLIGHT(CPI3D_frame f){ return static_cast<CPI3D_light>(f); }
#endif

//----------------------------
#endif
