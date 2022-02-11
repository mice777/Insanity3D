#ifndef __MODEL_H
#define __MODEL_H

#include "frame.h"
#include "container.h"
#include "scene.h"

//----------------------------

class I3D_model: public I3D_frame{
public:
#ifndef GL
   class C_shadow_info{
                                 //last light used to majke shadow
                                 // this pointer is not ref-counted, so it can't be referenced!
      class I3D_light *last_light;
      C_shadow_info(const C_shadow_info&);
      void operator =(const C_shadow_info&);
   public:

                                 //during transition, this is last successive dir of 'last_light'
      S_normal trans_base_dir;

                                 //this holds direction used in rendering of previous frame
      S_normal last_dir;

                                 //counter of transition, 0 if not animating
      dword transition_count;

      int shadow_select_count;   //counter for re-selecting shadow light

      void SetLastLight(I3D_light*);
      inline I3D_light *GetLastLight() const{ return last_light; }

      C_shadow_info():
         shadow_select_count(0),
         last_light(NULL)
      {
         Reset();
      }
      ~C_shadow_info(){
         SetLastLight(NULL);
      }
      void Reset(){
         SetLastLight(NULL);
         transition_count = 0;
      }
   };
#endif
protected:
   CPI3D_scene scn;
   I3D_container *in_container;//container which ticks us (may be NULL if we're not animated)

   I3D_container cont;

   C_bound_volume hr_bound;

#ifndef GL
   C_shadow_info *shadow_info;
#endif
   friend class I3D_scene;
   friend class I3D_scene_imp;

public:
   I3D_model(CPI3D_scene s):
      scn(s),
      in_container(NULL),
#ifndef GL
      shadow_info(NULL),
#endif
      I3D_frame(s->GetDriver1())
   {}

   inline PI3D_container GetContainer1(){ return &cont; }
   inline CPI3D_scene GetScene() const{ return scn; }

                              //some over-riding
   I3DMETHOD(EnumFrames)(I3D_ENUMPROC*p, dword user, dword flags = ENUMF_ALL, const char *mask = NULL) const = 0;
   I3DMETHOD_(PI3D_frame,FindChildFrame)(const char *name, dword flags) const = 0;

   I3DMETHOD_(dword,SetFlags)(dword new_flags, dword flags_mask) = 0;
public:
   I3DMETHOD_(const I3D_bound_volume&,GetHRBoundVolume)() const{
      if(!(frm_flags&FRMFLAGS_HR_BOUND_VALID))
         ((PI3D_model)this)->hr_bound.MakeHierarchyBounds((PI3D_frame)this);
      return hr_bound.bound_local; 
   }

   I3DMETHOD_(I3D_RESULT,SetAnimation)(dword stage, CPI3D_animation_set, dword options, float scale_factor, float blend_factor, float time_scale) = 0;
   I3DMETHOD_(I3D_RESULT,SetPose)(PI3D_animation_set) = 0;
   I3DMETHOD_(I3D_RESULT,SetAnimScaleFactor)(dword stage, float scale_factor) = 0;
   I3DMETHOD_(float,GetAnimScaleFactor)(dword stage = 0) const = 0;
   I3DMETHOD_(I3D_RESULT,SetAnimBlendFactor)(dword stage, float blend_factor) = 0;
   I3DMETHOD_(float,GetAnimBlendFactor)(dword stage = 0) const = 0;
   I3DMETHOD_(I3D_RESULT,SetAnimSpeed)(dword stage, float speed_factor) = 0;
   I3DMETHOD_(float,GetAnimSpeed)(dword stage = 0) const = 0;

   I3DMETHOD_(int,GetAnimEndTime)(dword stage = 0) const = 0;
   I3DMETHOD_(int,GetAnimCurrTime)(dword stage = 0) const = 0;
   I3DMETHOD_(I3D_RESULT,SetAnimCurrTime)(dword stage, dword time) = 0;
   I3DMETHOD_(I3D_RESULT,MoveAnimStage)(dword from, dword to) = 0;

   I3DMETHOD(StopAnimation)(dword stage) = 0;
   I3DMETHOD_(PI3D_animation_set,GetAnimationSet)() = 0;
   I3DMETHOD_(CPI3D_animation_set,GetAnimationSet)() const = 0;
   I3DMETHOD_(I3D_RESULT,SetAnimBasePos)(PI3D_frame frm, const S_vector *pos) = 0;
   I3DMETHOD_(I3D_RESULT,SetAnimBaseScale)(PI3D_frame frm, const float *scl) = 0;

   I3D_CONTAINER( = 0;)
   I3DMETHOD_(PI3D_container,GetContainer)(){ return &cont; }
   I3DMETHOD_(CPI3D_container,GetContainer)() const{ return &cont; }

   virtual bool IsAnimated() const = 0;
};

//----------------------------

#ifdef _DEBUG
inline PI3D_model I3DCAST_MODEL(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_MODEL ? NULL : static_cast<PI3D_model>(f); }
inline CPI3D_model I3DCAST_CMODEL(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_MODEL ? NULL : static_cast<CPI3D_model>(f); }
#else
inline PI3D_model I3DCAST_MODEL(PI3D_frame f){ return static_cast<PI3D_model>(f); }
inline CPI3D_model I3DCAST_CMODEL(CPI3D_frame f){ return static_cast<CPI3D_model>(f); }
#endif

//----------------------------

PI3D_model CreateModel(CPI3D_scene);

//----------------------------

#endif