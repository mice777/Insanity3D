/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.
   All rights reserved.

   File: Anim_3d.cpp
   Content: All animation classes, and all about animations.
--------------------------------------------------------*/

#include "all.h"
#include "anim.h"
#include "light.h"
#include "particle.h"
#include "model.h"
#include "soundi3d.h"

//----------------------------
//----------------------------

static const S_quat ZERO_QUATERNION(1.0f, 0.0f, 0.0f, 0.0f);

//----------------------------
//----------------------------

I3D_keyframe_anim::I3D_keyframe_anim(PI3D_driver d):
   I3D_animation_base(d, I3DANIM_KEYFRAME),
   end_time(0)
{
   //base_rot.Identity();
}

//----------------------------

void I3D_keyframe_anim::Clear(){

   end_time = 0;
   //base_rot.Identity();
                              //clear all keys
   pos_keys.clear();
   rot_keys.clear();
   note_keys.clear();
}

//----------------------------
// Convert 3 position key into tangent values for middle key.
static void Tcb2Bezier(
   const I3D_anim_vector_tcb &prev_key,
   const I3D_anim_vector_tcb &curr_key,
   const I3D_anim_vector_tcb &next_key,
   I3D_bezier<S_vector> &bezier){
/*
   float speed_ratio;
   if(!border_key){
      float speed_in, speed_out;
      speed_in=(curr_key.v - prev_key.v).Magnitude() / (float)(curr_key.time - prev_key.time);
      speed_out=(next_key.v - curr_key.v).Magnitude() / (float)(next_key.time - curr_key.time);
      //speed_ratio=speed_in/speed_out;
      speed_ratio = float(curr_key.time - prev_key.time) / float(next_key.time - curr_key.time);
   }else speed_ratio=1.0f;
   */

   S_vector delta = prev_key.v - next_key.v;
   bezier.in_tan = delta;
   bezier.out_tan = -delta;
   float size, mag;
   float tension = (1.0f - curr_key.tension) / 3;
   float continuity = -curr_key.continuity;
   S_vector dir;
                        //compute in-tangent
   dir = prev_key.v - curr_key.v;
   bezier.in_tan += (dir-bezier.in_tan) * continuity;
   mag = bezier.in_tan.Magnitude();
   if(mag > MRG_ZERO){
      size = tension * dir.Magnitude();
      bezier.in_tan *= (size/mag);
      /*
      if(speed_ratio<1.0f) bezier.in_tan *= speed_ratio;
      else bezier.in_tan *= (2.0f-(1.0f/speed_ratio));
      */
      bezier.in_tan += curr_key.v;
   }else bezier.in_tan = curr_key.v;
                        //compute out-tangent
   dir = next_key.v - curr_key.v;
   bezier.out_tan += (dir-bezier.out_tan) * continuity;
   mag = bezier.out_tan.Magnitude();
   if(mag > MRG_ZERO){
      size = tension * dir.Magnitude();
      bezier.out_tan *= (size/mag);
      /*
      speed_ratio = 1.0f/speed_ratio;
      if(speed_ratio<1.0f) bezier.out_tan *= speed_ratio;
      else bezier.out_tan *= (2.0f-(1.0f/speed_ratio));
      */
      bezier.out_tan += curr_key.v;
   }else bezier.out_tan = curr_key.v;
}

//----------------------------

I3D_RESULT I3D_keyframe_anim::SetPositionKeys(I3D_anim_pos_tcb *keys, dword num_keys){

   if(num_keys<=1)
      return I3DERR_INVALIDPARAMS;

   pos_keys.clear();
   if(!keys || !num_keys)
      return I3D_OK;

   pos_keys.assign(num_keys);
   bool looping = (num_keys>2 && keys[0].v == keys[num_keys-1].v);
   for(dword i=0; i<num_keys; i++){
                              //setup key base
      I3D_anim_vector_bezier &key = pos_keys[i];
      key.time = keys[i].time;
      key.v = keys[i].v;
      key.easy_from = keys[i].easy_from;
      key.easy_to = keys[i].easy_to;
                              //convert TCB into BEZIER
      //Tcb2Bezier(keys[i ? i-1 : i], keys[i], keys[(i<num_keys-1) ? i+1 : i], key);
      dword pi = Max(0, (int)i-1), ni = Min(num_keys-1, i+1);
      if(looping){
         if(!i) pi = num_keys - 2;
         if(i==num_keys-1) ni = 1;
      }
      Tcb2Bezier(keys[pi], keys[i], keys[ni], key);
   }
   return I3D_OK;
}

//----------------------------

static void InitRotationBezier(const S_quat &prev_rot, const S_quat &curr_rot, const S_quat &next_rot,
   I3D_bezier<S_quat> &bezier, float smoothness){

   S_quat delta = prev_rot - next_rot;
   bezier.in_tan = delta;
   bezier.out_tan = -delta;

   float mag;
   S_quat dir;
                        //compute in-tangent
   dir = prev_rot - curr_rot;
   bezier.in_tan += (dir - bezier.in_tan) * (1.0f - smoothness);
   mag = bezier.in_tan.Magnitude();
   if(mag > MRG_ZERO){
      float size = I3DSqrt(dir.Dot(dir)) / 3.0f;
      bezier.in_tan *= (size / mag);
      bezier.in_tan += curr_rot;
      if(!bezier.in_tan.Normalize())
         bezier.in_tan = curr_rot;
   }else
      bezier.in_tan = curr_rot;

                        //compute out-tangent
   dir = next_rot - curr_rot;
   bezier.out_tan += (dir - bezier.out_tan) * (1.0f - smoothness);
   mag = bezier.out_tan.Magnitude();
   if(mag > MRG_ZERO){
      float size = dir.Magnitude() / 3.0f;
      bezier.out_tan *= (size / mag);
      bezier.out_tan += curr_rot;
      if(!bezier.out_tan.Magnitude())
         bezier.out_tan = curr_rot;
   }else
      bezier.out_tan = curr_rot;
}

//----------------------------

I3D_RESULT I3D_keyframe_anim::SetRotationKeys(I3D_anim_rot *keys, dword num_keys){

   if(num_keys<=1)
      return I3DERR_INVALIDPARAMS;

   rot_keys.clear();
   if(!keys || !num_keys) return I3D_OK;

   C_vector<I3D_anim_quat_bezier> tmp_keys;
   C_vector<float> smoothness;
   tmp_keys.reserve(num_keys);
   smoothness.reserve(num_keys);
   S_quat curr_rot;
   curr_rot.Identity();
   const float max_angle = 120.0f/180.0f*PI;
   for(dword i=0; i<num_keys; i++){
                              //compute current key
      const I3D_anim_rot &key = keys[i];
                              //check for overly rotated keys, insert additional keys if such detected
      if(i && I3DFabs(key.angle) > max_angle){
         dword num_add_keys = FloatToInt(I3DFabs(key.angle) / max_angle + .5f);
         float single_angle = key.angle / (float)num_add_keys;
         assert(single_angle <= max_angle);
         const I3D_anim_rot &prev = keys[i-1];
         for(dword ai=0; ai<num_add_keys; ai++){
            float ratio = (float)(ai+1) / (float)num_add_keys;

            curr_rot *= S_quat(key.axis, single_angle);
            //assert(curr_rot.s >= 0.0f);
            tmp_keys.push_back(I3D_anim_quat_bezier());
            I3D_anim_quat_bezier &bez_rot = tmp_keys.back();
            bez_rot.time = prev.time + FloatToInt((key.time - prev.time) * ratio);
            bez_rot.easy_from = prev.easy_from + (key.easy_from - prev.easy_from) * ratio;
            bez_rot.easy_to = prev.easy_to + (key.easy_to - prev.easy_to) * ratio;
            bez_rot.q = curr_rot;
            float smth = prev.smoothness + (key.smoothness - prev.smoothness) * ratio;
            smoothness.push_back(smth);
         }
      }else{
         curr_rot *= S_quat(key.axis, key.angle);
         //if(curr_rot.s < 0.0f) curr_rot = -curr_rot;
                              //store (converted) key
         tmp_keys.push_back(I3D_anim_quat_bezier());
         I3D_anim_quat_bezier &bez_rot = tmp_keys.back();
         bez_rot.time = key.time;
         bez_rot.easy_from = key.easy_from;
         bez_rot.easy_to = key.easy_to;
         bez_rot.q = curr_rot;
         smoothness.push_back(key.smoothness);
      }
   }
   bool looping = (IsMrgZeroLess((tmp_keys.back().q - tmp_keys.front().q).Square()));
   num_keys = tmp_keys.size();
                              //create bezier tangents
   for(i=0; i<num_keys; i++){
      dword pi = Max(0, (int)i-1), ni = Min(num_keys-1, i+1);
      if(looping){
         if(!i) pi = num_keys-2;
         if(i==num_keys-1) ni = 1;
      }
      assert(pi<num_keys && ni<num_keys);
      InitRotationBezier(tmp_keys[pi].q, tmp_keys[i].q, tmp_keys[ni].q, tmp_keys[i], smoothness[i]);
   }
   rot_keys.assign(&tmp_keys.front(), (&tmp_keys.back())+1);

   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_keyframe_anim::SetRotationKeys1(I3D_anim_quat *keys, dword num_keys){

   if(num_keys<=1)
      return I3DERR_INVALIDPARAMS;

   rot_keys.clear();
   if(!keys || !num_keys) return I3D_OK;

   rot_keys.assign(num_keys);

   bool looping = (IsMrgZeroLess((keys[0].q - keys[num_keys-1].q).Square()));
   for(dword i=0; i<num_keys; i++){
      const I3D_anim_quat &key = keys[i];
                              //store (converted) key
      I3D_anim_quat_bezier &bez_rot = rot_keys[i];
      bez_rot.time = key.time;
      bez_rot.easy_from = key.easy_from;
      bez_rot.easy_to = key.easy_to;
      bez_rot.q = key.q;
                              //make sure we're always taking shorter path (orientate axes to similar direction)
      if(i && (bez_rot.q.Dot(rot_keys[i-1].q) < 0.0f))
         bez_rot.q = -bez_rot.q;
   }

   for(i=0; i<num_keys; i++){
                              //create bezier tangent
      dword pi = Max(0, (int)i-1), ni = Min(num_keys-1, i+1);
      if(looping){
         if(!i) pi = num_keys-2;
         if(i==num_keys-1) ni = 1;
      }
      InitRotationBezier(rot_keys[pi].q, rot_keys[i].q, rot_keys[ni].q, rot_keys[i], keys[i].smoothness);
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_keyframe_anim::SetNoteKeys(I3D_anim_note *keys, dword num_keys){

   if(!num_keys)
      return I3DERR_INVALIDPARAMS;

   note_keys.clear();
   if(!keys || !num_keys)
      return I3D_OK;

   //note_keys.reserve(num_keys);
   note_keys.assign(num_keys);
   for(dword i=0; i<num_keys; i++){
      //note_keys.push_back(S_anim_note(keys[i].time, keys[i].note));
      note_keys[i].time = keys[i].time;
      note_keys[i].note = keys[i].note;
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_keyframe_anim::SetVisibilityKeys(I3D_anim_visibility *keys, dword num_keys){

   if(!num_keys)
      return I3DERR_INVALIDPARAMS;

   vis_keys.clear();
   if(!keys || !num_keys)
      return I3D_OK;

   //vis_keys.reserve(num_keys);
   vis_keys.assign(keys, keys+num_keys);
   //for(dword i=0; i<num_keys; i++)
      //vis_keys.push_back(keys[i]);
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_keyframe_anim::SetPowerKeys(I3D_anim_power *keys, dword num_keys){

   if(!num_keys)
      return I3DERR_INVALIDPARAMS;

   power_keys.clear();
   if(!keys || !num_keys)
      return I3D_OK;

   power_keys.assign(num_keys);
   for(dword i=0; i<num_keys; i++)
      power_keys[i] = keys[i];
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_keyframe_anim::SetEndTime(dword e){

   end_time = e;
   return I3D_OK;
}

//----------------------------
//----------------------------

class I3D_interpolator_particle: public I3D_interpolator{
public:
   I3D_interpolator_particle(PI3D_driver d): I3D_interpolator(d)
   {}

   I3DMETHOD(Tick)(int time, PI3D_CALLBACK cb, void *cbc){
      if(!frm->IsOn1())
         return I3D_OK;
      PI3D_particle prt = (PI3D_particle)((PI3D_frame)frm);
      prt->Tick(Min(2000, time));
      return I3D_OK;
   }
};

//----------------------------
                              //interpolator used to update model by Tick-ing it
                              // this interpolator is contained in scenes and models
                              // owning model frames, so that when these are Ticked,
                              // models owned by them are Ticked too
class I3D_interpolator_model: public I3D_interpolator{
public:
   I3D_interpolator_model(PI3D_driver d): I3D_interpolator(d)
   {}
   I3DMETHOD(Tick)(int time, PI3D_CALLBACK cb, void *cbc){
      return I3DCAST_MODEL((PI3D_frame)frm)->Tick(time, cb, cbc);
   }
};

//----------------------------
// Compute 1/sqrt(s) using a tangent line approximation.
inline float isqrt_approx_in_neighborhood(float s){
#define NEIGHBORHOOD 0.959066f
#define SCALE 1.000311f
   //const float ADDITIVE_CONSTANT = SCALE / sqrt(NEIGHBORHOOD);
#define ADDITIVE_CONSTANT 1.02143514576f
   //const float FACTOR = SCALE * (-0.5 / (NEIGHBORHOOD * sqrt(NEIGHBORHOOD)));
#define FACTOR -0.532515565f
   return ADDITIVE_CONSTANT + (s - NEIGHBORHOOD) * FACTOR;
}

//----------------------------
// Normalize a quaternion using the above approximation.
inline void FastNormalize(S_quat &q){

   float s = q.Square();
   float k = isqrt_approx_in_neighborhood(s);

   if(s <= 0.91521198){
      k *= isqrt_approx_in_neighborhood(k * k * s);
      if(s <= 0.65211970){
         k *= isqrt_approx_in_neighborhood(k * k * s);
      }
   }
   q *= k;
   //assert(I3DFabs(1.0f-q.Square()) < .01f);
   if(I3DFabs(1.0f-q.Square()) > .01f)
      FastNormalize(q);
}

//----------------------------

void C_anim_ctrl_bezier_rot::Evaluate(const C_buffer<I3D_anim_quat_bezier> &keys, float curr_time, S_quat &result){

   dword num_keys = keys.size();
   assert(next_key_index<=num_keys);
   if(next_key_index == num_keys){
                              //already after end of track, return last keys's rotation
      result = keys.back().q;
      return;
   }
   int i_curr_time = FloatToInt(curr_time-.5f);
   if(i_curr_time >= keys[next_key_index].time){
                              //passed through another key
                              //find segment in which rotation is now
      do{
         if(++next_key_index == num_keys){
                              //passed through last key
            result = keys.back().q;
            return;
         }
      }while(i_curr_time >= keys[next_key_index].time);

      const I3D_anim_quat_bezier &curr = keys[next_key_index], &prev = keys[next_key_index-1];
      bezier.Init(prev.q, prev.out_tan, curr.in_tan, curr.q);
                              //compute easiness
      EaseSetup(prev.easy_from, curr.easy_to);
      rc_t_segment_delta = 1.0f / float(curr.time - prev.time);

      /*
                              //compute counter-warp time constant
      {
         float cos_alpha = prev.q.Dot(curr.q);
         cos_alpha = 1.0f - ((1.0f - cos_alpha) * .25f);
#define ATTENUATION 0.82279687f
#define WORST_CASE_SLOPE 0.58549219f
         float factor = 1.0f - ATTENUATION * cos_alpha;
         factor *= factor;
         warp_time_constant = WORST_CASE_SLOPE * factor;
      }
      /**/
   }
   if(!next_key_index){
      result = keys.front().q;
      return;
   }

   const I3D_anim_quat_bezier &prev = keys[next_key_index-1];
   float t = (curr_time - prev.time) * rc_t_segment_delta;
   t = GetEase(t);

#if 0
                              //old style - full slerp
   result = ((S_quat*)&bezier)[0].Slerp(((S_quat*)&bezier)[3], t, true);
   return;
#endif

                              //warp time to correct errors caused by linear interpolation
   /*
   if(t <= 0.5f){
      t = t * (warp_time_constant * t * (2.0f * t - 3.0f) + 1.0f + warp_time_constant);
   }else{
      t = 1.0f - t;
      t = 1.0f - t * (warp_time_constant * t * (2.0f * t - 3.0f) + 1.0f + warp_time_constant);
   }
   /**/

   bezier.Evaluate(t, result);
   FastNormalize(result);

   //result = ((S_quat*)&bezier)[0] * (1.0f-t) + ((S_quat*)&bezier)[3] * t;
   //result.Normalize();
}

//----------------------------
//----------------------------
                              //position/rotation/scale interpolator
class I3D_interpolator_PRS: public I3D_interpolator{

                              //keyframe animation stage processing
   class C_KF_stage{
   public:
      C_smart_ptr_const<I3D_keyframe_anim> anim;
      dword options;
      float curr_time, end_time;

      C_anim_ctrl_bezier_vector ctrl_position;
      C_anim_ctrl_bezier_rot ctrl_rotation;
      C_anim_ctrl_power ctrl_power;

      C_anim_ctrl_note ctrl_note;
      //C_anim_ctrl_visibility ctrl_visibility;
      C_anim_ctrl_power ctrl_visibility;

      C_KF_stage():
         options(0),
         curr_time(0.0f), end_time(0.0f)
      {
      }

      void SetAnim(CPI3D_keyframe_anim ab){
         anim = ab;
      }

      virtual void Restart(){
         curr_time = 0.0f;
         if(anim){
            ctrl_position.Restart();
            ctrl_rotation.Restart();
            ctrl_power.Restart();

            ctrl_note.Restart();
            ctrl_visibility.Restart();
         }
      }

//----------------------------
// Update stage by given time, evaluate values and mix with with current results.
// If the returned value is true, the stage still continues to run.
      virtual bool Evaluate(int time, dword stage_index, PI3D_frame frm,
         dword options, C_anim_results &results, PI3D_CALLBACK cb, void *cbc){

         CPI3D_keyframe_anim ka = (CPI3D_keyframe_anim)(CPI3D_animation_base)anim;

         bool done = false;
         curr_time += time;
         if(curr_time >= end_time){
            curr_time = end_time;
            done = true;
         }
                              //make position
         const C_buffer<I3D_anim_pos_bezier> &pos_keys = ka->GetPositionKeysVector();
         if(pos_keys.size()){
            ctrl_position.Evaluate(pos_keys, curr_time, results.pos);
            results.flags |= ANIMRES_POS_OK;
         }
                        //make rotation
         const C_buffer<I3D_anim_quat_bezier> &rot_keys = ka->GetRotationKeysVector();
         if(rot_keys.size()){
            ctrl_rotation.Evaluate(rot_keys, curr_time, results.rot);
            results.flags |= ANIMRES_ROT_OK;
         }
                              //power
         const C_buffer<I3D_anim_power> &power_keys = ka->GetPowerKeysVector();
         if(power_keys.size()){
            ctrl_power.Evaluate(power_keys, curr_time, results.pow);
            results.flags |= ANIMRES_POW_OK;
         }
         {
                              //make visibility and note controllers
            //ctrl_visibility.Tick(curr_time, frm, ka);
            ctrl_note.Tick(curr_time, stage_index, frm, ka, cb, cbc);

            const C_buffer<I3D_anim_visibility> &vis_keys = ka->GetVisibilityKeysVector();
            if(vis_keys.size()){
               float vis;
               ctrl_visibility.Evaluate(vis_keys, curr_time, vis);
               switch(frm->GetType()){
               case FRAME_VISUAL:
                  I3DCAST_VISUAL(frm)->SetVisualAlpha(vis);
                  break;
               }
               frm->SetOn(vis>0.0f);
            }
         }
                              //run until time's not over
         return (!done);
      }
   } stage;

//----------------------------
// Stage is finished with animation
   void FinishAnim(dword options, bool &is_running, PI3D_CALLBACK cb, void *cbc){

      is_running = true;
                           //at the end - determine what to do next
      if(stage.options&I3DANIMOP_LOOP){
         stage.Restart();
      }else{
         if((options&I3DANIMOP_FINISH_NOTIFY) && cb)
            cb(I3DCB_ANIM_FINISH, (dword)(PI3D_frame)frm, 0, cbc);
         if(!(stage.options&I3DANIMOP_KEEPDONE))
            is_running = false;
      }
   }

//----------------------------
public:
   I3D_interpolator_PRS(PI3D_driver d1):
      I3D_interpolator(d1)
   {
   }

public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

//----------------------------

   I3D_RESULT I3DAPI Tick(int time, PI3D_CALLBACK cb, void *cbc){

      if(!frm)
         return I3DERR_NOTINITIALIZED;

                                 //since callback gives user possibility to
                                 // release the frame, we must hold ref to our interface
      ++ref;

      C_anim_results res;
      bool running = stage.Evaluate(time, 0, frm, 0, res, cb, cbc);
      if(!running)
         FinishAnim(stage.options, running, cb, cbc);

      if(res.flags&ANIMRES_POS_OK)
         frm->SetPos(res.pos);

      if(res.flags&ANIMRES_ROT_OK)
         frm->SetRot(res.rot);

      if(res.flags&ANIMRES_POW_OK){
         PI3D_frame f = frm;
         switch(f->GetType1()){
         case FRAME_LIGHT:
            I3DCAST_LIGHT(f)->SetPower(res.pow);
            break;
         case FRAME_SOUND:
            I3DCAST_SOUND(f)->SetVolume(res.pow);
            break;
         default: 
            frm->SetScale(res.pow);
         }
      }
      if(!--ref)
         delete this;
      return (running ? I3D_OK : I3D_DONE);
   }

//----------------------------

   I3DMETHOD(SetAnimation)(CPI3D_keyframe_anim, dword);

   I3DMETHOD(SetCurrTime)(dword time){
      if(!stage.anim || stage.anim->GetType1()!=I3DANIM_KEYFRAME)
         return I3DERR_INVALIDPARAMS;
                              //27.06.2002: restart stages to set correct next_key_index;
      stage.Restart(); //must be before set curr_time, because reset also time.

      stage.curr_time = (float)time;
      return I3D_OK;
   }

   I3DMETHOD_(CPI3D_animation_base,GetAnimation)() const{
      return stage.anim;
   }

   I3DMETHOD_(dword,GetOptions)() const{
      return stage.options;
   }

   I3DMETHOD_(dword,GetEndTime)() const{
      if(!stage.anim)
         return 0;
      return FloatToInt(stage.end_time);
   }

   I3DMETHOD_(dword,GetCurrTime)() const{
      if(!stage.anim)
         return 0;
      return FloatToInt(stage.curr_time);
   }
};

//----------------------------

I3D_RESULT I3D_interpolator_PRS::SetAnimation(CPI3D_keyframe_anim ap, dword options){

   stage.SetAnim(ap);

   if(ap){
      stage.end_time = (float)ap->end_time;
      stage.options = options;
   }
   return I3D_OK;
}

//----------------------------
//----------------------------
                              //animation set
I3D_animation_set::I3D_animation_set(PI3D_driver d):
   ref(1),
   drv(d)
{
   drv->AddCount(I3D_CLID_ANIMATION_SET);
}

//----------------------------

I3D_animation_set::~I3D_animation_set(){
   drv->DecCount(I3D_CLID_ANIMATION_SET);
}

//----------------------------

void I3D_animation_set::AddAnimation(PI3D_animation_base anim, const char *link, float weight){
   anims.push_back(S_anim_link());
   S_anim_link &al = anims.back();
   al.anim = anim;
   al.link = link;
   al.weight = weight;
}

//----------------------------

I3D_RESULT I3D_animation_set::RemoveAnimation(CPI3D_animation_base ap){

   for(dword i=0; i<anims.size(); i++){
      if(anims[i].anim == ap){
         anims[i]=anims.back(); anims.pop_back();
         return I3D_OK;
      }
   }
   return I3DERR_OBJECTNOTFOUND;
}

//----------------------------

dword I3D_animation_set::GetTime() const{
                              //time of anims is synced to max of them
                              //so return time of 1st anim
   if(!anims.size())
      return 0;
   CPI3D_animation_base ab = anims.front().anim;
   if(ab->GetType1()!=I3DANIM_KEYFRAME)
      return 0;
   return ((PI3D_keyframe_anim)ab)->GetEndTime();
}

//----------------------------
//----------------------------

PI3D_interpolator I3D_driver::CreateInterpolator(){
   
   return new I3D_interpolator_PRS(this);
}

//----------------------------
//----------------------------

void I3D_container::ClearInterpolators(){
   for(dword i=interpolators.size(); i--; )
      interpolators[i]->Release();
   interpolators.clear();
}

//----------------------------

I3D_RESULT I3D_container::AddInterpolator(PI3D_interpolator ip){

                              //don't need the same interpolator twice in list
   if(interpolators.size()){
      int i = FindPointerInArray((void**)&interpolators.front(), interpolators.size(), ip);
      if(i!=-1)
         return I3DERR_INVALIDPARAMS;
   }
   interpolators.push_back(ip);
   ip->AddRef();
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_container::RemoveInterpolator(PI3D_interpolator ip){

   for(int i=interpolators.size(); i--; )
   if(interpolators[i]==ip){
      ip->Release();
      interpolators[i] = interpolators.back(); interpolators.pop_back();
      return I3D_OK;
   }
   return I3DERR_OBJECTNOTFOUND;
}

//----------------------------

I3D_RESULT I3D_container::AddAnimFrame(PI3D_frame frm){

   RemoveAnimFrame(frm);

   PI3D_interpolator ib;
   switch(frm->GetType1()){
   case FRAME_VISUAL:
      if(!(I3DCAST_VISUAL(frm)->GetVisualType1()==I3D_VISUAL_PARTICLE))
         return I3DERR_INVALIDPARAMS;
      ib = new I3D_interpolator_particle(frm->GetDriver1());
      break;

   case FRAME_MODEL:
      ib = new I3D_interpolator_model(frm->GetDriver1());
      break;

   default:
      return I3DERR_INVALIDPARAMS;
   }
   interpolators.push_back(ib);
   ib->SetFrame(frm);
   //ib->Release();
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_container::RemoveAnimFrame(PI3D_frame frm){

   for(int i=interpolators.size(); i--; ){
      if(interpolators[i]->GetFrame()==frm){
         interpolators[i]->Release();
         interpolators[i] = interpolators.back(); interpolators.pop_back();
         return I3D_OK;
      }
   }
   return I3DERR_OBJECTNOTFOUND;
}

//----------------------------

I3D_RESULT I3D_container::Tick(int time, PI3D_CALLBACK cb, void *cbc){

   for(dword i=0; i<interpolators.size(); i++){
      PI3D_interpolator *ip = (PI3D_interpolator*)&interpolators.front();
      PI3D_interpolator intp = ip[i];
      I3D_RESULT ir = intp->Tick(time, cb, cbc);
      if(ir==I3D_DONE){
         if(i<interpolators.size() && intp==interpolators[i]){
            intp->Release();
            interpolators[i] = interpolators.back(); interpolators.pop_back();
            --i;
         }
      }
   }
   if(!NumInterpolators1())
      return I3D_DONE;
   return I3D_OK;
}

//----------------------------
//----------------------------
                              //scene animation
void I3D_scene::AddAnimation(CPI3D_keyframe_anim ap, PI3D_frame fp, dword options){

   assert(ap);
   PI3D_interpolator intp = drv->CreateInterpolator();
   intp->SetFrame(fp);
   if(ap)
      intp->SetAnimation(ap, options);
   AddInterpolator(intp);
   intp->Release();
}

//----------------------------

void I3D_scene::DelAnimation(CPI3D_frame frm){

   PI3D_interpolator const *intps = cont.GetInterpolators1();
   for(int j=cont.NumInterpolators1(); j--; )
   if(intps[j]->GetFrame() == frm){
      RemoveInterpolator(intps[j]);
   }
}

//----------------------------
//----------------------------
