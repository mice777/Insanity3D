#ifndef __ANIM_H
#define __ANIM_H

#include "frame.h"
#include "driver.h"

//----------------------------

class I3D_animation_base{
   I3D_ANIMATION_TYPE type;
protected:
   dword ref;
   PI3D_driver drv;

   I3D_animation_base(PI3D_driver d1, I3D_ANIMATION_TYPE t1):
      ref(1),
      drv(d1),
      type(t1)
   {
      drv->AddCount(I3D_CLID_ANIMATION_BASE);
   }
   ~I3D_animation_base(){
      drv->DecCount(I3D_CLID_ANIMATION_BASE);
   }
public:
   inline I3D_ANIMATION_TYPE GetType1() const{ return type; }
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD_(I3D_ANIMATION_TYPE,GetType)() const{ return type; }
};

//----------------------------

class I3D_keyframe_anim: public I3D_animation_base{
public:
   struct S_anim_note{
      int time;
      C_str note;
      S_anim_note(){}
      S_anim_note(int t1, const char *cp1): time(t1), note(cp1) {}
   };
private:
   
   int end_time;
   //S_quat base_rot;           //keep base rotation, since rotation track values are relative
                              //anim keys for pos/rot/not/vis/pow
   C_buffer<I3D_anim_pos_bezier> pos_keys;
   //C_buffer<I3D_anim_rot> rot_keys;
   C_buffer<I3D_anim_quat_bezier> rot_keys;
   C_buffer<S_anim_note> note_keys;
   C_buffer<I3D_anim_visibility> vis_keys;
   C_buffer<I3D_anim_power> power_keys;

   friend I3D_scene;
   friend I3D_driver;
   friend I3D_model;
   friend class I3D_interpolator;
   friend class I3D_interpolator_PRS;

   I3D_keyframe_anim(PI3D_driver);
   I3D_keyframe_anim(const I3D_keyframe_anim&);
   I3D_keyframe_anim& operator=(const I3D_keyframe_anim&);
public:
   inline const C_buffer<I3D_anim_pos_bezier> &GetPositionKeysVector() const{ return pos_keys; }
   //inline const C_buffer<I3D_anim_rot> &GetRotationKeysVector() const{ return rot_keys; }
   inline const C_buffer<I3D_anim_quat_bezier> &GetRotationKeysVector() const{ return rot_keys; }
   inline const C_buffer<I3D_anim_power> &GetPowerKeysVector() const{ return power_keys; }

   inline const C_buffer<S_anim_note> &GetNoteKeysVector() const{ return note_keys; }
   inline const C_buffer<I3D_anim_visibility> &GetVisibilityKeysVector() const{ return vis_keys; }

   //inline const S_quat &GetBaseRotation() const{ return base_rot; }
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(void,Clear)();
   I3DMETHOD(SetPositionKeys)(I3D_anim_pos_tcb*, dword num_keys);
   I3DMETHOD(SetRotationKeys)(I3D_anim_rot*, dword num_keys);
   I3DMETHOD(SetRotationKeys1)(I3D_anim_quat*, dword num_keys);
   I3DMETHOD(SetNoteKeys)(I3D_anim_note*, dword num_keys);
   I3DMETHOD(SetVisibilityKeys)(I3D_anim_visibility*, dword num_keys);
   I3DMETHOD(SetPowerKeys)(I3D_anim_power*, dword num_keys);
   I3DMETHOD(SetEndTime)(dword);

   I3DMETHOD(GetPositionKeys)(const I3D_anim_pos_bezier **kp, dword *nk) const{
      *kp = pos_keys.begin();
      *nk = pos_keys.size();
      return I3D_OK;
   }
   I3DMETHOD(GetRotationKeys)(const I3D_anim_quat_bezier **kp, dword *nk) const{
      *kp = rot_keys.begin();
      *nk = rot_keys.size();
      return I3D_OK;
   }
   I3DMETHOD(GetPowerKeys)(const I3D_anim_power **kp, dword *nk) const{
      *kp = power_keys.begin();
      *nk = power_keys.size();
      return I3D_OK;
   }
   I3DMETHOD(GetNoteKeys)(const I3D_anim_note **kp, dword *nk) const{
#if 0
      *kp = note_keys.begin();
      *nk = note_keys.size();
      return I3D_OK;
#endif
      return I3DERR_UNSUPPORTED;
   }
   I3DMETHOD(GetVisibilityKeys)(const I3D_anim_visibility **kp, dword *nk) const{
      *kp = vis_keys.begin();
      *nk = vis_keys.size();
      return I3D_OK;
   }

   I3DMETHOD_(dword,GetEndTime)() const{ return end_time; }
};

//----------------------------

class I3D_anim_pose: public I3D_animation_base{
public:
   enum{
      APOSF_POS = 1,          //position defined
      APOSF_ROT = 2,          //rotation defined
      APOSF_POW = 4,          //power defined
   };
private:
   PI3D_driver drv;
   dword apos_flags;

   S_vector position;
   S_quat rotation;
   float power;

   I3D_anim_pose(PI3D_driver d1):
      I3D_animation_base(d1, I3DANIM_POSE),
      apos_flags(0)
   {
   }
   friend I3D_driver;
public:
   inline dword GetFlags() const{ return apos_flags; }
   inline const S_vector &GetPos1() const{ return position; }
   inline const S_quat &GetRot1() const{ return rotation; }
   inline const float GetPower1() const{ return power; }
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(void,Clear)(){
      apos_flags = 0;
   }

   I3DMETHOD_(void,SetPos)(const S_vector *v){
      apos_flags &= ~APOSF_POS;
      if(v){
         position = *v;
         apos_flags |= APOSF_POS;
      }
   }
   I3DMETHOD_(void,SetPower)(const float *f){
      apos_flags &= ~APOSF_POW;
      if(f){
         power = *f;
         apos_flags |= APOSF_POW;
      }

   }
   I3DMETHOD_(void,SetRot)(const S_quat *q){
      apos_flags &= ~APOSF_ROT;
      if(q){
         rotation = *q;
         if(rotation.s < 0.0f)
            rotation = -rotation;
         apos_flags |= APOSF_ROT;
      }
   }
   I3DMETHOD_(const S_vector*,GetPos)() const{
      return (apos_flags&APOSF_POS) ? &position : NULL;
   }
   I3DMETHOD_(const S_quat*,GetRot)() const{
      return (apos_flags&APOSF_ROT) ? &rotation : NULL;
   }
   I3DMETHOD_(const float*,GetPower)() const{
      return (apos_flags&APOSF_POW) ? &power : NULL;
   }
};

//----------------------------

class I3D_animation_set{
protected:
   dword ref;
public:
   struct S_anim_link{
      C_smart_ptr<I3D_animation_base> anim;
      C_str link;
      float weight;           //per-frame weight, used depending on operation performed in animation stage

      S_anim_link():
         weight(1.0f)
      {}
   };
private:
   C_vector<S_anim_link> anims;
   PI3D_driver drv;

   I3D_animation_set(PI3D_driver);

   friend I3D_driver;
public:
   ~I3D_animation_set();
   const S_anim_link *GetAnimLinks() const{ return &anims[0]; }
   dword NumLinks() const{ return anims.size(); }
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(Open)(const char *fname, dword flags = 0, PI3D_LOAD_CB_PROC = NULL, void *cb_context = NULL);
   I3DMETHOD_(dword,GetTime)() const;
   
   I3DMETHOD_(void,AddAnimation)(PI3D_animation_base anim, const char *link, float damp_factor = 0.0f);
   I3DMETHOD(RemoveAnimation)(CPI3D_animation_base);

   I3DMETHOD_(dword,NumAnimations)() const{ return anims.size(); }
   //I3DMETHOD_(PI3D_animation_base,GetAnimation)(int i){ return anims[i].anim; }
   I3DMETHOD_(CPI3D_animation_base,GetAnimation)(int i) const{ return anims[i].anim; }
   I3DMETHOD_(const C_str&,GetAnimLink)(int i) const{ return anims[i].link; }
};

//----------------------------

class I3D_interpolator{
protected:
   dword ref;

   PI3D_driver drv;

   C_smart_ptr<I3D_frame> frm;  //frame to which it applies
public:
   I3D_interpolator(PI3D_driver d):
      ref(1),
      drv(d)
   {
      drv->AddCount(I3D_CLID_INTERPOLATOR);
   }

   ~I3D_interpolator(){
      drv->DecCount(I3D_CLID_INTERPOLATOR);
   }

   inline PI3D_frame GetFrame1(){ return frm; }
   inline CPI3D_frame GetFrame1() const{ return frm; }
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
                              //controll
   I3DMETHOD_(I3D_RESULT,Tick)(int time, PI3D_CALLBACK, void *cb_context) = 0;
                              //setup
   I3DMETHOD_(void,SetFrame)(PI3D_frame in_frm){
      frm = in_frm;
   }

   I3DMETHOD_(I3D_RESULT,SetAnimation)(CPI3D_keyframe_anim, dword options){
      return I3DERR_UNSUPPORTED;
   }
   I3DMETHOD(SetCurrTime)(dword time){ return I3DERR_UNSUPPORTED; }
                              //retrieve
   //I3DMETHOD_(PI3D_animation_base,GetAnimation)(){ return NULL; }
   I3DMETHOD_(CPI3D_animation_base,GetAnimation)() const{ return NULL; }
   I3DMETHOD_(PI3D_frame,GetFrame)(){ return frm; }
   I3DMETHOD_(CPI3D_frame,GetFrame)() const{ return frm; }
   I3DMETHOD_(dword,GetOptions)() const{ return 0; }
   I3DMETHOD_(dword,GetEndTime)() const{ return 0; }
   I3DMETHOD_(dword,GetCurrTime)() const{ return 0; }
};

//----------------------------
//----------------------------
                              //internal interpolators and animation processing system

                              //animation processing controls - classes responsible for iteration through
                              // various animation tracks and animate during time
//----------------------------
// Base of animation controller utilizing easiness.
class C_anim_ctrl_easiness{
protected:
   dword next_key_index;
                              //easy times (0..1)
   float easy_in_end, easy_out_beg, easy_out_range;
   bool use_ease;             //true to use easiness
public:
   C_anim_ctrl_easiness():
      next_key_index(0)
   {}

//----------------------------
// Setup ease sub-interpolator.
   void EaseSetup(float from, float to){
      use_ease = (from!=0.0f || to!=0.0f);
      if(!use_ease)
         return;

      easy_in_end = from;
      easy_out_range = to;
      float f = easy_in_end + easy_out_range;
      if(f>1.0f){
         easy_in_end /= f;
         easy_out_range /= f;
      }
      easy_out_beg = 1.0f - easy_out_range;
   }

//----------------------------
// Evaluate easiness - convert linear key time (0.0 ... 1.0) to
// easy-interpoated value.
   inline float GetEase(float time) const{
      if(use_ease){
         if(time<easy_in_end){
            return ((float)cos(PI + time/easy_in_end * PI * .5f) + 1.0f) *
               easy_in_end;
         }else
         if(time>easy_out_beg){
            float out = 1.0f - time;
            return easy_out_beg + easy_out_range *
               (float)cos(-out/easy_out_range * PI * .5f);
         }
      }
      return time;
   }

//----------------------------
// Restart controller.
   inline void Restart(){ next_key_index = 0; }
};

//----------------------------
// Bezier C_vector controller, used for iteration through position track.
class C_anim_ctrl_bezier_vector: public C_anim_ctrl_easiness{
public:
   C_bezier_curve<S_vector> bezier;

   void Evaluate(const C_buffer<I3D_anim_vector_bezier> &keys, float curr_time, S_vector &v){

      if(next_key_index >= keys.size()){
         v.Zero();
         return;
      }

      int i_curr_time = FloatToInt(curr_time-.5f);
      bool overflow = false;
      if(i_curr_time >= keys[next_key_index].time){
         do{
            if(++next_key_index == keys.size()){
               overflow = true;
               --next_key_index;
               break;
            }
         }while(i_curr_time >= keys[next_key_index].time);

         const I3D_anim_vector_bezier &curr = keys[next_key_index], &prev = keys[next_key_index-1];
         bezier.Init(prev.v, prev.out_tan, curr.in_tan, curr.v);
                              //compute easiness
         EaseSetup(prev.easy_from, keys[next_key_index].easy_to);
      }else{
         if(!next_key_index){
            v.Zero();
            return;
         }
      }
      float t;
      if(overflow){
         t = 1.0f;
      }else{
         t = (curr_time - (float)keys[next_key_index-1].time) / (float)(keys[next_key_index].time - keys[next_key_index-1].time);
         t = GetEase(t);
      }
      bezier.Evaluate(t, v);
   }
};

//----------------------------
// Bezier rotation controller, used for iteration through rotation track.
class C_anim_ctrl_bezier_rot: public C_anim_ctrl_easiness{
   float rc_t_segment_delta;  //reciprocal of current's segment time delta
   C_bezier_curve<S_quat> bezier;
   //float warp_time_constant;
public:

   void Evaluate(const C_buffer<I3D_anim_quat_bezier> &keys, float curr_time, S_quat &result);
};

//----------------------------
// Power controller - used for iteration through power track.
class C_anim_ctrl_power{
   dword next_key;
public:
   C_anim_ctrl_power():
      next_key(0)
   {}

   void Restart(){ next_key = 0; }

   void Evaluate(const C_buffer<I3D_anim_power> &keys, float curr_time, float &pow){

      int i_curr_time = FloatToInt(curr_time-.5f);
      if(next_key >= keys.size()){
         pow = keys.back().power;
         return;
      }
      bool last = false;
      while(i_curr_time >= keys[next_key].time){
         if(++next_key == keys.size()){
            last = true;
            pow = keys.back().power;
            break;
         }
      }
      if(!next_key){
         const I3D_anim_power &key1 = keys[next_key];
         const int &t1 = key1.time;
         float f = curr_time / (float)t1;
         pow = 1.0f + (key1.power - 1.0f) * f;
         return;
      }
      if(last)
         pow = keys[next_key-1].power;
      else{
         const I3D_anim_power &key0 = keys[next_key-1], &key1 = keys[next_key];
         const int &t0 = key0.time;
         const int &t1 = key1.time;
         float f = (curr_time - (float)t0) / (float)(t1-t0);
         pow = key1.power - key0.power;
         pow *= f;
         pow += key0.power;
      }
   }
};

//----------------------------
// Note controller, used for iteration through note track.
class C_anim_ctrl_note{
                              //index to next key
   dword next_key;
public:
   C_anim_ctrl_note():
      next_key(0)
   {}

   void Restart(){ next_key = 0; }

   void Tick(float curr_time, dword stage_index, PI3D_frame frm, CPI3D_keyframe_anim anim, PI3D_CALLBACK cb, void *cbc){

      {
         const C_buffer<I3D_keyframe_anim::S_anim_note> &note_keys = anim->GetNoteKeysVector();
         if(next_key < note_keys.size()){
            int i_curr_time = FloatToInt(curr_time-.5f);
            while(i_curr_time >= note_keys[next_key].time){

               I3D_note_callback nc = { i_curr_time, note_keys[next_key].time, stage_index, note_keys[next_key].note};
                              //do callback, if desired
               if(cb)
                  cb(I3DCB_ANIM_NOTE, (dword)frm, (dword)&nc, cbc);
               if(++next_key == note_keys.size())
                  break;
            }
         }
      }
   }
};

//----------------------------
// Visibility controller, used for iteration through visibility track.
/*
class C_anim_ctrl_visibility{
                              //index to next key
   dword next_key;
public:
   C_anim_ctrl_visibility():
      next_key(0)
   {}

   void Restart(){ next_key = 0; }

   void Tick(float curr_time, PI3D_frame frm, CPI3D_keyframe_anim anim){

      const C_buffer<I3D_anim_visibility> &vis_keys = anim->GetVisibilityKeysVector();
      if(next_key<vis_keys.size()){
         int i_curr_time = FloatToInt(curr_time-.5f);
         while(i_curr_time >= vis_keys[next_key].time){
            frm->SetOn(vis_keys[next_key].on);
            if(++next_key == vis_keys.size())
               break;
         }
      }
   }
};
*/

//----------------------------
//----------------------------
                              //results of animation processing
                              // initially values are not valid,
                              // after processing some values may become valid
class C_anim_results{
#define ANIMRES_POS_OK  1
#define ANIMRES_ROT_OK  2
#define ANIMRES_POW_OK  4
public:
   S_vector pos;
   S_quat rot;
   float pow;
   dword flags;

   C_anim_results():
      flags(0)
   {
   }
};

//----------------------------
//----------------------------

#endif
