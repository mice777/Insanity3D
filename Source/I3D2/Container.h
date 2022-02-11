#ifndef __CONTAINER_H
#define __CONTAINER_H

//#include "anim.h"

//----------------------------

struct S_container_vcall{
   typedef I3D_RESULT (I3DAPI C_unknown::*t_Open)(const char*, dword, PI3D_LOAD_CB_PROC, void*);
   typedef void (I3DAPI C_unknown::*t_Close)();
   typedef I3D_RESULT (I3DAPI C_unknown::*t_AddFrame)(PI3D_frame);
   typedef I3D_RESULT (I3DAPI C_unknown::*t_RemoveFrame)(PI3D_frame);

   t_Open cbOpen;
   t_Close cbClose;
   t_AddFrame cbAddFrame;
   t_RemoveFrame cbRemoveFrame;
};

//----------------------------
                              //container - keeping references to frames and interpolators,
                              // allows for loading scene files
class I3D_container{
   C_vector<class I3D_interpolator*> interpolators;

   I3D_container &operator =(const I3D_container&);

   friend class I3D_scene;
   friend class I3D_model;
   friend class I3D_model_imp;

                              //callbacks to master interface
   C_unknown *master_interface;
   const S_container_vcall *vcall;
protected:
   C_vector<C_smart_ptr<I3D_frame> > frames;
   C_str filename;            //loaded file
   C_str user_data;

//----------------------------
// Clear contents of animator.
   void ClearInterpolators();

public:
   I3D_container():
      master_interface(NULL)
   {}

   inline dword NumInterpolators1() const{ return interpolators.size(); }
   inline const PI3D_interpolator *GetInterpolators1() const{ return (PI3D_interpolator*)&interpolators.front(); }

   inline const C_vector<I3D_interpolator*> &GetInterpolatorVector() const{ return interpolators; }
   inline dword NumFrames1() const{ return frames.size(); }

   C_vector<C_smart_ptr<I3D_frame> > &GetFrameVector(){ return frames; }
   const C_vector<C_smart_ptr<I3D_frame> > &GetFrameVector() const{ return frames; }
   I3D_RESULT AddAnimFrame(PI3D_frame frm);
   I3D_RESULT RemoveAnimFrame(PI3D_frame frm);

//----------------------------
// Open scene internal.
   I3D_RESULT Open(const char *filename, dword flags, PI3D_LOAD_CB_PROC cb_proc,
      void *cb_context, PI3D_scene scene, PI3D_frame root, PI3D_animation_set anim_set, PI3D_driver);

   I3D_RESULT OpenFromChunk(C_chunk &ck, dword flags, PI3D_LOAD_CB_PROC cb_proc,
      void *cb_context, PI3D_scene scene, PI3D_frame root, PI3D_animation_set anim_set, PI3D_driver);

public:
   I3DMETHOD_(I3D_RESULT,Open)(const char *fname, dword flags, PI3D_LOAD_CB_PROC cb_proc, void *cb_context){
      return (master_interface->*vcall->cbOpen)(fname, flags, cb_proc, cb_context);
   }
   I3DMETHOD_(void,Close)(){
      (master_interface->*vcall->cbClose)();
   }
   I3DMETHOD_(const C_str&,GetFileName)() const{ return filename; }
   I3DMETHOD_(void,StoreFileName)(const C_str &fn){ filename = fn; }

   I3DMETHOD_(I3D_RESULT,AddFrame)(PI3D_frame frm){ return (master_interface->*vcall->cbAddFrame)(frm); }
   I3DMETHOD_(I3D_RESULT,RemoveFrame)(PI3D_frame frm){ return (master_interface->*vcall->cbRemoveFrame)(frm); }

   I3DMETHOD_(dword,NumFrames)() const{ return frames.size(); }
   I3DMETHOD_(PI3D_frame const*,GetFrames)(){ return (const PI3D_frame*)(frames.size() ? &frames.front() : NULL); }
   I3DMETHOD_(CPI3D_frame const*,GetFrames)() const{ return (const PI3D_frame*)&frames.front(); }

   I3DMETHOD_(I3D_RESULT,AddInterpolator)(PI3D_interpolator);
   I3DMETHOD_(I3D_RESULT,RemoveInterpolator)(PI3D_interpolator);
   I3DMETHOD_(dword,NumInterpolators)() const{ return NumInterpolators1(); }
   I3DMETHOD_(PI3D_interpolator const*,GetInterpolators)(){ return GetInterpolators1(); }
   I3DMETHOD_(CPI3D_interpolator const*,GetInterpolators)() const{ return GetInterpolators1(); }

   I3DMETHOD_(I3D_RESULT,Tick)(int time, PI3D_CALLBACK cb, void *cbc);

   I3DMETHOD_(const C_str&,GetUserData)() const{ return user_data; }
   I3DMETHOD_(void,SetUserData)(const C_str &s){ user_data = s; }
};

//----------------------------

#endif
