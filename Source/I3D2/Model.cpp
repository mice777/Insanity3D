/*--------------------------------------------------------
   Copyright (c) 1999, 2000 Michal Bacik. 
   All rights reserved.

   File: Model.cpp
   Content: Model frame.
--------------------------------------------------------*/

#include "all.h"
#include "common.h"
#include "model.h"
#include "particle.h"
#include "scene.h"
#include "light.h"
#include "anim.h"
#include <list>

//----------------------------
                              //virtual table for container callback to model
static const S_container_vcall vc_container_model = {
   (S_container_vcall::t_Open)&I3D_model::Open,
   (S_container_vcall::t_Close)&I3D_model::Close,
   (S_container_vcall::t_AddFrame)&I3D_model::AddFrame,
   (S_container_vcall::t_RemoveFrame)&I3D_model::RemoveFrame,
};

//----------------------------
#ifndef GL
void I3D_model::C_shadow_info::SetLastLight(I3D_light *lp){
   if(lp) lp->AddRef();
   if(last_light) last_light->Release();
   last_light = lp;
}
#endif
//----------------------------
//----------------------------
                              //results of animation, used with animation blending
class C_anim_results_blend: public C_anim_results{
public:
   float weight;
   C_anim_results_blend():
      weight(1.0f)
   { flags = 0; }

//----------------------------
// Scale all valid results by factor;
   void Scale(float scale_factor){
      if(flags&ANIMRES_POS_OK)
         pos *= scale_factor;
      if(flags&ANIMRES_ROT_OK)
         //rot.Scale(scale_factor);
         rot.ScaleFast(scale_factor);
      if(flags&ANIMRES_POW_OK)
         //pow = 1.0f + (pow-1.0f) * scale_factor;
         pow *= scale_factor;
      weight *= scale_factor;
   }

//----------------------------
// Mix given values into this values. 'options' specify mode (combination of I3DANIMOP_? flags).
// This class represents left side of equation, 'results' represents right side.
   inline void MergeWith(C_anim_results_blend &results, dword options, float blend_factor){

      if(results.flags&ANIMRES_POS_OK){
         switch(options&I3DANIMOP_APPLY_MASK){
         case I3DANIMOP_DAMP_ADD:
         case I3DANIMOP_ADD:
            if(flags&ANIMRES_POS_OK){
               pos += results.pos;
               break;
            }
                              //flow...
         case I3DANIMOP_SET:
            pos = results.pos;
            break;

         case I3DANIMOP_BLEND:
            if(flags&ANIMRES_POS_OK)
               pos = pos * (1.0f-blend_factor) + results.pos * blend_factor;
            else
               pos = results.pos * blend_factor;
            break;

         default:
            assert(0);
         }
         flags |= ANIMRES_POS_OK;
      }

      if(results.flags&ANIMRES_ROT_OK){
         switch(options&I3DANIMOP_APPLY_MASK){
         case I3DANIMOP_DAMP_ADD:
         case I3DANIMOP_ADD:
            if(flags&ANIMRES_ROT_OK){
               rot = rot * results.rot;
               break;
            }
                              //flow...
         case I3DANIMOP_SET:
            rot = results.rot;
            break;

         case I3DANIMOP_BLEND:
            if(flags&ANIMRES_ROT_OK){
               //rot = rot.Slerp(results.rot, blend_factor, true);
               rot.SlerpFast(results.rot, blend_factor, rot);
            }else{
               rot = results.rot;
               //rot.Scale(blend_factor);
               rot.ScaleFast(blend_factor);
            }
            break;

         default:
            assert(0);
         }
         flags |= ANIMRES_ROT_OK;
      }

      if(results.flags&ANIMRES_POW_OK){
         switch(options&I3DANIMOP_APPLY_MASK){
         case I3DANIMOP_DAMP_ADD:
         case I3DANIMOP_ADD:
            if(flags&ANIMRES_POW_OK){
               pow *= results.pow;
               break;
            }
                              //flow...
         case I3DANIMOP_SET:
            pow = results.pow;
            break;

         case I3DANIMOP_BLEND:
            if(flags&ANIMRES_POW_OK)
               pow = pow * (1.0f-blend_factor) + results.pow * blend_factor;
            else
               pow = (1.0f-blend_factor) + results.pow * blend_factor;
            break;

         default:
            assert(0);
         }
         flags |= ANIMRES_POW_OK;
      }

      switch(options&I3DANIMOP_APPLY_MASK){
      case I3DANIMOP_DAMP_ADD:
      case I3DANIMOP_ADD:
         weight += results.weight;
         break;

      case I3DANIMOP_SET:
         weight = results.weight;
         break;

      case I3DANIMOP_BLEND:
         weight = weight * (1.0f-blend_factor) + results.weight * blend_factor;
         break;

      default:
         assert(0);
      }
   }
};

//----------------------------
//----------------------------
                              //base interpolator, used for interpolation of animation (I3D_animation_base)
class C_interpolator_base: public C_unknown{
protected:
   C_smart_ptr<I3D_animation_base> anim;
   float weight;
public:
   C_interpolator_base(CPI3D_animation_base an, float w):
      anim((PI3D_animation_base)an),
      weight(w)
   {}

   virtual void Restart(){}

   inline float GetWeight() const{ return weight; }

//----------------------------
// Evaluate interpolator in provided time. Store results into 'results'.
   virtual void Evaluate(float curr_time, C_anim_results_blend &results,
      const S_vector &base_pos, const float base_scl) = 0;

   //inline PI3D_animation_base GetAnim(){ return anim; }
   inline CPI3D_animation_base GetAnim() const{ return anim; }
};

//----------------------------

class C_note_data{
public:
   C_str note;
   dword curr_time;
   dword note_time;
   dword stage_index;
   C_smart_ptr<I3D_frame> frm;
   C_note_data(){}
   C_note_data(PI3D_frame f, const C_str &s, dword ct, dword nt, dword si):
      frm(f),
      note(s),
      curr_time(ct),
      note_time(nt),
      stage_index(si)
   {}
};

//----------------------------
                              //specialized interpolator, used for interpolation of keyframe animation
class C_interpolator_KF: public C_interpolator_base{
   C_anim_ctrl_bezier_vector ctrl_position;
   C_anim_ctrl_bezier_rot ctrl_rotation;
   C_anim_ctrl_power ctrl_power;

   //C_anim_ctrl_visibility ctrl_visibility;
   C_anim_ctrl_power ctrl_visibility;

//----------------------------
// Special note controller, used for iteration through note track.
   class C_anim_ctrl_note{
                                 //index to next key
      dword next_key;
   public:
      C_anim_ctrl_note():
         next_key(0)
      {}

      void Restart(){ next_key = 0; }

      void Tick(float curr_time, dword stage_index, PI3D_frame frm, CPI3D_keyframe_anim anim, C_vector<C_note_data> &note_data){

         {
            const C_buffer<I3D_keyframe_anim::S_anim_note> &note_keys = anim->GetNoteKeysVector();
            if(next_key < note_keys.size()){
               int i_curr_time = FloatToInt(curr_time-.5f);
               while(i_curr_time >= note_keys[next_key].time){

                  //I3D_note_callback nc = { i_curr_time, note_keys[next_key].time, stage_index, note_keys[next_key].note};
                                 //do callback, if desired
                  //if(cb)
                     //cb(I3DCB_ANIM_NOTE, (dword)frm, (dword)&nc, cbc);
                  note_data.push_back(C_note_data(frm, note_keys[next_key].note, i_curr_time, note_keys[next_key].time, stage_index));
                  if(++next_key == note_keys.size())
                     break;
               }
            }
         }
      }

//----------------------------

      inline void AdvanceTime(int curr_time, CPI3D_keyframe_anim anim){

         const C_buffer<I3D_keyframe_anim::S_anim_note> &note_keys = anim->GetNoteKeysVector();
         while(next_key<note_keys.size() && curr_time >= note_keys[next_key].time)
            ++next_key;
      }
   } ctrl_note;

public:
   C_interpolator_KF(PI3D_keyframe_anim ab, float df):
      C_interpolator_base(ab, df)
   {
      //ctrl_rot.Restart(ab->GetBaseRotation());
   }

   virtual void Restart(){
      assert(anim);
      ctrl_position.Restart();
      //ctrl_rot.Restart(((PI3D_keyframe_anim)(PI3D_animation_base)anim)->GetBaseRotation());
      ctrl_rotation.Restart();
      ctrl_power.Restart();

      ctrl_note.Restart();
      ctrl_visibility.Restart();
   }

//----------------------------

   virtual void Evaluate(float curr_time, C_anim_results_blend &results,
      const S_vector &base_pos, const float base_scl){

      CPI3D_keyframe_anim ka = (CPI3D_keyframe_anim)(CPI3D_animation_base)anim;

      const C_buffer<I3D_anim_vector_bezier> &pkeys = ka->GetPositionKeysVector();
      if(pkeys.size()){
         ctrl_position.Evaluate(pkeys, curr_time, results.pos);
         results.pos -= base_pos;
         results.flags |= ANIMRES_POS_OK;
      }
      const C_buffer<I3D_anim_quat_bezier> &rot_keys = ka->GetRotationKeysVector();
      if(rot_keys.size()){
         ctrl_rotation.Evaluate(rot_keys, curr_time, results.rot);
         results.flags |= ANIMRES_ROT_OK;
      }
      const C_buffer<I3D_anim_power> &power_keys = ka->GetPowerKeysVector();
      if(power_keys.size()){
         ctrl_power.Evaluate(power_keys, curr_time, results.pow);
         results.pow -= base_scl;
         results.flags |= ANIMRES_POW_OK;
      }
      results.weight = weight;
   }

//----------------------------

   inline void ProcessSpecialTracks(float curr_time, PI3D_frame frm, dword stage_index, C_vector<C_note_data> &note_data){

                           //make visibility and note controllers
      CPI3D_keyframe_anim ka = (CPI3D_keyframe_anim)(CPI3D_animation_base)anim;
      ctrl_note.Tick(curr_time, stage_index, frm, ka, note_data);

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

//----------------------------

   void AdvanceTime(int time){
      CPI3D_keyframe_anim ka = (CPI3D_keyframe_anim)(CPI3D_animation_base)anim;
      ctrl_note.AdvanceTime(time, ka);
   }
};

//----------------------------

class C_interpolator_pose: public C_interpolator_base{
public:
   C_interpolator_pose(CPI3D_animation_base ab, float df):
      C_interpolator_base(ab, df)
   {
   }

   virtual void Evaluate(float curr_time, C_anim_results_blend &results,
      const S_vector &base_pos, const float base_scl){

      CPI3D_anim_pose ap = (CPI3D_anim_pose)(CPI3D_animation_base)anim;
      dword pose_flags = ap->GetFlags();

      if(pose_flags&I3D_anim_pose::APOSF_POS){
         results.pos = ap->GetPos1();
         results.pos -= base_pos;
         results.flags |= ANIMRES_POS_OK;
      }
      if(pose_flags&I3D_anim_pose::APOSF_ROT){
         results.rot = ap->GetRot1();
         results.flags |= ANIMRES_ROT_OK;
      }
      if(pose_flags&I3D_anim_pose::APOSF_POW){
         results.pow = ap->GetPower1();
         results.pow -= base_scl;
         results.flags |= ANIMRES_POW_OK;
      }
      results.weight = weight;
   }
};

//----------------------------
                              //animation stage - holding data associated with single stage
                              // (options, factors, current time, etc)
class C_anim_stage: public C_unknown{
   C_anim_stage(const C_anim_stage&);
   bool operator =(const C_anim_stage&);
public:
   dword stage_index;
   dword options;             //I3DANIMOP_??? flags
   float curr_time, end_time; //current and end anim time
   float scale_factor;        //scale factor of stage
   float blend_factor;        //blend factor (used only with BLEND op)
   float speed_factor;        //speed factor of stage (used by keyframe interpolators)


   C_buffer<C_smart_ptr<C_interpolator_base> > interpolators;
                              //run-time:
   bool is_unit_scale, is_unit_blend;  //little optim - set if value is 1.0f

   C_anim_stage(dword si, dword o, float scf, float blf, float spf, dword play_time):
      stage_index(si),
      options(o),
      curr_time(0.0f), end_time((float)play_time),
      scale_factor(scf), is_unit_scale(scf==1.0f),
      blend_factor(blf), is_unit_blend(blf==1.0f),
      speed_factor(spf)
   {
   }

//----------------------------
// Restart all interpolators.
   void RestartInterpolators(){
      for(dword i=interpolators.size(); i--; )
         interpolators[i]->Restart();
   }

//----------------------------

   void SetCurrTime(int time){
      //RestartInterpolators();
      curr_time = (float)time;
                              //advance special interpolators
      for(dword i=interpolators.size(); i--; ){
         C_interpolator_base *intp = interpolators[i];
         intp->Restart();
         if(intp->GetAnim()->GetType1()==I3DANIM_KEYFRAME){
            ((C_interpolator_KF*)intp)->AdvanceTime(time);
         }
      }
   }

//----------------------------
// Stage is finished with animation
// Returns true if stage continues to run, false if playback finished.
   bool FinishStage(){

                              //at the end - determine what to do next
      if(options&I3DANIMOP_LOOP){
         RestartInterpolators();
                              //leave fractional time which looped
         curr_time = (float)fmod(curr_time, end_time);
      }else{
         curr_time = end_time;
         if(!(options&I3DANIMOP_KEEPDONE))
            return false;
      }
      return true;
   }
};

//----------------------------

class C_frm_anim_processor: public C_unknown{
   C_frm_anim_processor(const C_frm_anim_processor &ap);
   void operator =(const C_frm_anim_processor &ap);
public:
   PI3D_frame frm;
   S_vector base_pos;         //referencial base position
   float base_scl;            //referencial base scale

   typedef pair<C_smart_ptr<C_interpolator_base>, C_smart_ptr<C_anim_stage> > t_proc_intp;
   typedef list<t_proc_intp> t_proc_intps;
   t_proc_intps intps;

   C_frm_anim_processor(): frm(NULL){}
   C_frm_anim_processor(PI3D_frame f):
      frm(f),
      base_pos(0, 0, 0),
      base_scl(1.0f)
   {}


//----------------------------
// Evaluate single animation stage. This means merging the stage with provided results.
// The stage may be mixed directly with current results and recursively call this func again,
//    or if it is braced, it may be mixed with next stage first.
// The method returns true if it is still running.
// Note about precedence:
//    - all stages are evauated left-to-right by default
//    - if options for specified stage contain number of braces,
//       such number of successive stages are blended together prior to blending with current results
//    - param 'brace_count' is recursively passed to next stages,
//       so that they know if they should blend with successive stages
   //bool EvalSingleStage(t_anim_stages::iterator it, PI3D_CALLBACK cb, void *cbc, int time,
   void EvalSingleStage(C_vector<C_note_data> &note_data, C_frm_anim_processor::t_proc_intps::iterator it,
      C_anim_results_blend &results, dword brace_count = 0);
};

//----------------------------
//----------------------------
                              //model class
class I3D_model_imp: public I3D_model{
   C_smart_ptr<I3D_animation_set> anim_set;  //loaded anim set

                              //dynamic list of anim stages, mapped to their indices
   typedef map<dword, C_smart_ptr<C_anim_stage> > t_anim_stages;
   t_anim_stages anim_stages;

   C_vector<C_smart_ptr<C_frm_anim_processor> > anim_procs;

   C_buffer<pair<PI3D_frame, S_vector> > frm_base_positions;
   C_buffer<pair<PI3D_frame, float> > frm_base_scales;

//----------------------------

   static void modelSetSingleName(const C_str &name, PI3D_frame fp){

      const C_str &str = fp->GetName1();
      dword j = str.Size();
      const char *base;
      if(j){
         while(j && str[j-1]!='.') --j;
         base = &str[j];
      }else base="";
      fp->SetName(C_fstr("%s.%s", (const char*)name, base));
   }

//----------------------------

   static void modelUnsetSingleName(PI3D_frame fp){

      const C_str &str=fp->GetName1();
      dword j = str.Size();
      while(j && str[j-1]!='.') --j;
      fp->SetName(&str[j]);
   }

//----------------------------

   static void cbModelDup(PI3D_model origm, PI3D_model newm){

      int i;

      const C_vector<C_smart_ptr<I3D_frame> > &chld = origm->GetContainer1()->GetFrameVector();
                                 //sort list by number of partents to root
      C_sort_list<int> slist;
      for(i=chld.size(); i--; ){
         CPI3D_frame frm = chld[i];
         for(int nump=0; frm=frm->GetParent(), frm; nump++ );
         slist.Add(i, nump);
      }
      slist.Sort();

      map<CPI3D_frame, PI3D_frame> link_map;
      map<CPI3D_frame, PI3D_frame>::iterator it;

      link_map[origm] = newm;

#if 0
      for(i=0; i<(int)chld.size(); i++){
         int si = slist[i];
         CPI3D_frame orig = chld[si];
         PI3D_frame dupc = NULL;

         CPI3D_scene scn = newm->GetScene();
         switch(orig->GetType1()){
         case FRAME_VISUAL:
            dupc = scn->CreateFrame(FRAME_VISUAL, I3DCAST_CVISUAL(orig)->GetVisualType1());
            break;
         default:
            dupc = scn->CreateFrame(orig->GetType1());
            assert(dupc);
         }
         if(dupc){
            it = link_map.find(orig->GetParent());
            //assert(it!=link_map.end()); //JV: not necessary true, owned frame can be linked outside of model.
            if(it!=link_map.end()){
               PI3D_frame prnt = (*it).second;
               assert(prnt);
               dupc->LinkTo(prnt);
            }

            newm->AddFrame(dupc);
            dupc->Release();

            link_map[orig] = dupc;
         }
      }
      for(i=chld.size(); i--; ){
         int si = slist[i];
         CPI3D_frame orig = chld[si];
         PI3D_frame dupc = link_map[orig];
         dupc->Duplicate(orig);
         dupc->SetFrameFlags(dupc->GetFrameFlags()|FRMFLAGS_SUBMODEL);
      }
#else
      for(i=0; i<(int)chld.size(); i++){
         int si = slist[i];
         CPI3D_frame orig = chld[si];
         PI3D_frame dupc = NULL;

         CPI3D_scene scn = newm->GetScene();
         switch(orig->GetType1()){
         case FRAME_VISUAL:
            dupc = scn->CreateFrame(FRAME_VISUAL, I3DCAST_CVISUAL(orig)->GetVisualType1());
            break;
         default:
            dupc = scn->CreateFrame(orig->GetType1());
            assert(dupc);
         }
         if(dupc){
            it = link_map.find(orig->GetParent());
            //assert(it!=link_map.end()); //JV: not necessary true, owned frame can be linked outside of model.
            if(it!=link_map.end()){
               PI3D_frame prnt = (*it).second;
               assert(prnt);
               dupc->LinkTo(prnt);
            }

            newm->AddFrame(dupc);
            dupc->Release();

            link_map[orig] = dupc;

                              //duplicate model now (because it may contain frames which other frames are linekd to)
            if(dupc->GetType1()==FRAME_MODEL){
               dupc->Duplicate(orig);
               dupc->SetFrameFlags(dupc->GetFrameFlags()|FRMFLAGS_SUBMODEL);
                              //add all children to duplication map
               struct S_hlp{
                  static void AddToMap(PI3D_frame dup, CPI3D_frame orig, map<CPI3D_frame, PI3D_frame> &link_map){
                              //add all dup's children
                     const C_vector<PI3D_frame> &children = dup->GetChildren1();
                     for(dword i=dup->NumChildren1(); i--; ){
                        PI3D_frame c_dup = children[i];
                        CPI3D_frame c_orig = orig->FindChildFrame(c_dup->GetOrigName());
                        assert(c_orig);
                        link_map[c_orig] = c_dup;
                              //recursively call on child
                        AddToMap(c_dup, c_orig, link_map);
                     }
                  }
               };
               S_hlp::AddToMap(dupc, orig, link_map);
            }
         }
      }
                              //duplicate frames (except model)
      for(i=chld.size(); i--; ){
         int si = slist[i];
         CPI3D_frame orig = chld[si];
         if(orig->GetType()==FRAME_MODEL)
            continue;
         PI3D_frame dupc = link_map[orig];
         dupc->Duplicate(orig);
         dupc->SetFrameFlags(dupc->GetFrameFlags()|FRMFLAGS_SUBMODEL);
      }
#endif
   }

//----------------------------

   void StopAllAnimations(){

      anim_stages.clear();
      anim_procs.clear();
   }

public:
   I3D_model_imp(CPI3D_scene s):
      I3D_model(s)
   {
      drv->AddCount(I3D_CLID_MODEL);
      type = FRAME_MODEL;
      enum_mask = ENUMF_MODEL;

      cont.master_interface = (C_unknown*)this;
      cont.vcall = &vc_container_model;
   }
   ~I3D_model_imp(){

      Close();
#ifndef GL
      delete shadow_info;
#endif
      drv->DecCount(I3D_CLID_MODEL);
      if(in_container)
         in_container->RemoveAnimFrame(this);
   }

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(I3D_RESULT,EnumFrames)(I3D_ENUMPROC *p, dword user, dword flags, const char *mask) const{
      return I3D_frame::EnumFrames(p, user, flags | ENUMF_MODEL_ORIG, mask);
   }

//----------------------------

   I3DMETHOD_(PI3D_frame,FindChildFrame)(const char *name, dword flags) const{
      return I3D_frame::FindChildFrame(name, flags | ENUMF_MODEL_ORIG);
   }

//----------------------------
   I3DMETHOD_(dword,SetFlags)(dword new_flags, dword flags_mask){
#ifndef GL

      if(flags_mask&I3D_FRMF_SHADOW_CAST){
         if((frm_flags^new_flags)&I3D_FRMF_SHADOW_CAST){
            if(frm_flags&I3D_FRMF_SHADOW_CAST){
               assert(shadow_info);
               delete shadow_info;
               shadow_info = NULL;
            }else{
               assert(!shadow_info);
               shadow_info = new C_shadow_info;
            }
         }
      }
#endif
      return I3D_frame::SetFlags(new_flags, flags_mask);
   }
//----------------------------

   I3DMETHOD_(I3D_RESULT,AddFrame)(PI3D_frame frm){

      switch(frm->GetType1()){
      case FRAME_VISUAL:
         if(I3DCAST_VISUAL(frm)->GetVisualType1() != I3D_VISUAL_PARTICLE) break;
         cont.AddAnimFrame(frm);
         if(in_container)
            in_container->AddAnimFrame(this);
         break;
      case FRAME_MODEL:
         {
                                 //check if it's already in some other scene
            I3D_model_imp *mod = (I3D_model_imp*)frm;
            if(mod->in_container){
                                 //can't be in 2 scenes at the same time
               return I3DERR_GENERIC;
            }
            mod->in_container = &cont;
                                 //if it contains animation, create interpolator
            if(mod->IsAnimated()){
               cont.AddAnimFrame(frm);
               if(in_container)
                  in_container->AddAnimFrame(this);
            }
         }
         break;
      }
      cont.frames.push_back(frm);
      frm->frm_flags |= FRMFLAGS_SUBMODEL;
      modelSetSingleName(name, frm);

      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,RemoveFrame)(PI3D_frame frm){

      for(int i=0; i<(int)cont.frames.size(); i++)
      if(frm==cont.frames[i]){
                              //remove anim processor, if set
         for(dword pi=anim_procs.size(); pi--; ){
            if(anim_procs[pi]->frm==frm){
               anim_procs[pi] = anim_procs.back(); anim_procs.pop_back();
            }
         }
         for(pi=frm_base_positions.size(); pi--; ){
            if(frm_base_positions[pi].first==frm){
               frm_base_positions[pi] = frm_base_positions.back();
               frm_base_positions.resize(frm_base_positions.size()-1);
            }
         }
         for(pi=frm_base_scales.size(); pi--; ){
            if(frm_base_scales[pi].first==frm){
               frm_base_scales[pi] = frm_base_scales.back();
               frm_base_scales.resize(frm_base_scales.size()-1);
            }
         }

         frm->AddRef();          //released later
         cont.frames[i] = cont.frames.back(); cont.frames.pop_back();

         switch(frm->GetType1()){
         case FRAME_VISUAL:
            if(I3DCAST_VISUAL(frm)->GetVisualType1() != I3D_VISUAL_PARTICLE)
               break;
            cont.RemoveAnimFrame(frm);
            if(in_container && !cont.NumInterpolators1() && !anim_stages.size())
               in_container->RemoveAnimFrame(this);
            break;

         case FRAME_MODEL:
            {
               I3D_model_imp *mod = (I3D_model_imp*)frm;
               if(mod->in_container==&cont){
                  if(mod->IsAnimated()){
                     cont.RemoveAnimFrame(frm);
                     if(in_container && !IsAnimated())
                        in_container->RemoveAnimFrame(frm);
                  }
                  mod->in_container = NULL;
               }
            }
            break;
         }
         frm->frm_flags &= ~FRMFLAGS_SUBMODEL;
         if(frm->Release()){     //don't change name on destroyed frames
            modelUnsetSingleName(frm);
         }
         return I3D_OK;
      }
      return I3DERR_OBJECTNOTFOUND;
   }

//----------------------------

   I3DMETHOD_(void,SetName)(const C_str &n){

      I3D_frame::SetName(n);
      for(int i=cont.frames.size(); i--; )
         modelSetSingleName(name, cont.frames[i]);
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,Duplicate)(CPI3D_frame frm){

      if(frm==this)
         return I3D_OK;
      if(frm->GetType1()==FRAME_MODEL){
         I3D_model_imp *mp = (I3D_model_imp*)frm;

         Close();

         cont.filename = mp->cont.filename;
         cont.user_data = mp->cont.user_data;

         cbModelDup(mp, this);
         anim_set = mp->anim_set;
                                 //must set flags this way - special processing inside
         SetFlags(frm->GetFlags(), (dword)-1);
      }
      return I3D_frame::Duplicate(frm);
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,SetAnimation)(dword stage, CPI3D_animation_set, dword options, float scale_factor, float blend_factor, float time_scale);

   I3DMETHOD_(I3D_RESULT,SetPose)(PI3D_animation_set);

//----------------------------

   I3DMETHOD_(I3D_RESULT,SetAnimScaleFactor)(dword stage, float scale_factor){

      assert(!_isnan(scale_factor));
      t_anim_stages::iterator it = anim_stages.find(stage);
      if(it==anim_stages.end()){
         //DEBUG_LOG(C_fstr("I3D_model::SetAnimScaleFactor: stage %i not initialized", stage));
         return I3DERR_OBJECTNOTFOUND;
      }
      C_anim_stage *stg = (*it).second;
      stg->scale_factor = scale_factor;
      stg->is_unit_scale = (scale_factor==1.0f);
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(float,GetAnimScaleFactor)(dword stage = 0) const{

      t_anim_stages::const_iterator it = anim_stages.find(stage);
      if(it==anim_stages.end())
         return 0.0f;
      const C_anim_stage *stg = (*it).second;
      return stg->scale_factor;
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,SetAnimBlendFactor)(dword stage, float blend_factor){

      assert(!_isnan(blend_factor));
      t_anim_stages::iterator it = anim_stages.find(stage);
      if(it==anim_stages.end()){
         //DEBUG_LOG(C_fstr("I3D_model::SetAnimBlendFactor: stage %i not initialized", stage));
         return I3DERR_OBJECTNOTFOUND;
      }
      C_anim_stage *stg = (*it).second;
      stg->blend_factor = blend_factor;
      stg->is_unit_blend = (blend_factor==1.0f);
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(float,GetAnimBlendFactor)(dword stage = 0) const{

      t_anim_stages::const_iterator it = anim_stages.find(stage);
      if(it==anim_stages.end())
         return 0.0f;
      const C_anim_stage *stg = (*it).second;
      return stg->blend_factor;
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,SetAnimSpeed)(dword stage, float speed_factor){

      assert(!_isnan(speed_factor));
      t_anim_stages::iterator it = anim_stages.find(stage);
      if(it==anim_stages.end()){
         //DEBUG_LOG(C_fstr("I3D_model::SetAnimSpeed: stage %i not initialized", stage));
         return I3DERR_OBJECTNOTFOUND;
      }
      C_anim_stage *stg = (*it).second;
      stg->speed_factor = speed_factor;
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(float,GetAnimSpeed)(dword stage = 0) const{

      t_anim_stages::const_iterator it = anim_stages.find(stage);
      if(it==anim_stages.end())
         return 0.0f;
      const C_anim_stage *stg = (*it).second;
      return stg->speed_factor;
   }

//----------------------------

   I3DMETHOD_(int,GetAnimEndTime)(dword stage = 0) const{

      t_anim_stages::const_iterator it = anim_stages.find(stage);
      if(it==anim_stages.end()){
         //DEBUG_LOG(C_fstr("I3D_model::GetAnimEndTime: stage %i not initialized", stage));
         return -1;
      }
      const C_anim_stage *stg = (*it).second;
      return FloatToInt(stg->end_time);
   }

//----------------------------

   I3DMETHOD_(int,GetAnimCurrTime)(dword stage = 0) const{

      t_anim_stages::const_iterator it = anim_stages.find(stage);
      if(it==anim_stages.end()){
         //DEBUG_LOG(C_fstr("I3D_model::GetAnimCurrTime: stage %i not initialized", stage));
         return -1;
      }
      const C_anim_stage *stg = (*it).second;
      return FloatToInt(stg->curr_time);
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,SetAnimCurrTime)(dword stage, dword time){

      t_anim_stages::iterator it = anim_stages.find(stage);
      if(it==anim_stages.end()){
         //DEBUG_LOG(C_fstr("I3D_model::SetAnimCurrTime: stage %i not initialized", stage));
         return I3DERR_OBJECTNOTFOUND;
      }
      C_anim_stage *stg = (*it).second;
      stg->SetCurrTime(time);
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,MoveAnimStage)(dword from, dword to){

      if(from==to)
         return I3D_OK;
      t_anim_stages::iterator it = anim_stages.find(from);
      if(it==anim_stages.end()){
         //DEBUG_LOG(C_fstr("I3D_model::MoveAnimStage: stage %i not found", from));
         return I3DERR_OBJECTNOTFOUND;
      }
      C_smart_ptr<C_anim_stage> stg = (*it).second;
      anim_stages.erase(it);

      StopAnimation(to);
      anim_stages[to] = stg;
      stg->stage_index = to;

                              //re-sort interpolators in all frame processors
      for(dword pi=anim_procs.size(); pi--; ){
         C_frm_anim_processor *ap = anim_procs[pi];

         C_frm_anim_processor::t_proc_intps &ips = ap->intps;
         C_frm_anim_processor::t_proc_intps::iterator it, it1;
         for(it=ips.begin(); it!=ips.end(); it++){
            if((*it).second==stg){
               if(from > to){
                  while(it!=ips.begin()){
                     it1 = it; --it1;
                     if((*it1).second->stage_index < to)
                        break;
                     swap((*it), (*it1));
                     it = it1;
                  }
               }else{
                  while(true){
                     it1 = it; ++it1;
                     if(it1==ips.end() || (*it1).second->stage_index > to)
                        break;
                     swap((*it), (*it1));
                     it = it1;
                  }
               }
               break;
            }
         }
      }
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD(StopAnimation)(dword stage);

   I3DMETHOD_(I3D_RESULT,Open)(const char* fname, dword flags = 0, I3D_LOAD_CB_PROC* = NULL, void *context = NULL);

   I3DMETHOD_(void,Close)(){

      StopAllAnimations();
                              //release all our frames
      while(cont.frames.size())
         RemoveFrame(cont.frames.back());
                              //unlink all child frames
                              // MB: no, Close may affect only owned frames
      //while(children.size())
         //children.back()->LinkTo(NULL);
      anim_set = NULL;
   }

   I3DMETHOD_(const C_str&,GetFileName)() const{ return cont.GetFileName(); }
   I3DMETHOD_(void,StoreFileName)(const C_str &fn){ cont.StoreFileName(fn); }
   I3DMETHOD_(dword,NumFrames)() const{ return cont.NumFrames1(); }
   I3DMETHOD_(PI3D_frame const*,GetFrames)(){ return cont.GetFrames(); }
   I3DMETHOD_(CPI3D_frame const*,GetFrames)() const{ return cont.GetFrames(); }
   I3DMETHOD_(I3D_RESULT,AddInterpolator)(PI3D_interpolator i){ return cont.AddInterpolator(i); }
   I3DMETHOD_(I3D_RESULT,RemoveInterpolator)(PI3D_interpolator i){ return cont.RemoveInterpolator(i); }
   I3DMETHOD_(dword,NumInterpolators)() const{ return cont.NumInterpolators1(); }
   I3DMETHOD_(PI3D_interpolator const*,GetInterpolators)(){ return cont.GetInterpolators(); }
   I3DMETHOD_(CPI3D_interpolator const*,GetInterpolators)() const{ return cont.GetInterpolators(); }
   I3DMETHOD_(I3D_RESULT,Tick)(int time, PI3D_CALLBACK=NULL, void *cb_context = NULL);
   I3DMETHOD_(const C_str&,GetUserData)() const{ return cont.GetUserData(); }
   I3DMETHOD_(void,SetUserData)(const C_str &s){ cont.SetUserData(s); }

   I3DMETHOD_(PI3D_animation_set,GetAnimationSet)(){ return anim_set; }
   I3DMETHOD_(CPI3D_animation_set,GetAnimationSet)() const{ return anim_set; }

   I3DMETHOD_(I3D_RESULT,SetAnimBasePos)(PI3D_frame frm, const S_vector *pos){
      for(dword i=frm_base_positions.size(); i--; ){
         if(frm_base_positions[i].first==frm){
            if(pos)
               frm_base_positions[i].second = *pos;
            else{
               frm_base_positions[i] = frm_base_positions.back();
               frm_base_positions.resize(frm_base_positions.size()-1);
            }
            return I3D_OK;
         }
      }
      if(pos){
                              //check if frame is in our interpolators
         for(i=cont.frames.size(); i--; ){
            if(frm==cont.frames[i]){
               frm_base_positions.resize(frm_base_positions.size()+1);
               frm_base_positions.back() = pair<PI3D_frame, S_vector>(frm, *pos);
               return I3D_OK;
            }
         }
      }
      return I3DERR_OBJECTNOTFOUND;
   }

   I3DMETHOD_(I3D_RESULT,SetAnimBaseScale)(PI3D_frame frm, const float *scl){
      for(dword i=frm_base_scales.size(); i--; ){
         if(frm_base_scales[i].first==frm){
            if(scl)
               frm_base_scales[i].second = *scl;
            else{
               frm_base_scales[i] = frm_base_scales.back();
               frm_base_scales.resize(frm_base_scales.size()-1);
            }
            return I3D_OK;
         }
      }
      if(scl){
                              //check if frame is in our interpolators
         for(i=cont.frames.size(); i--; ){
            if(frm==cont.frames[i]){
               frm_base_scales.resize(frm_base_scales.size()+1);
               frm_base_scales.back() = pair<PI3D_frame, float>(frm, *scl);
               return I3D_OK;
            }
         }
      }
      return I3DERR_OBJECTNOTFOUND;
   }

   virtual bool IsAnimated() const{
      return (cont.NumInterpolators1() || anim_stages.size());
   }
};

//----------------------------

I3D_RESULT I3D_model_imp::Open(const char *fname, dword flags, I3D_LOAD_CB_PROC *cb_proc, void *cb_context){

   PI3D_animation_set as = drv->CreateAnimationSet();

   I3D_RESULT ir = cont.Open(fname, flags, cb_proc, cb_context, (PI3D_scene)scn, this, as, drv);
   if(I3D_SUCCESS(ir)){
                              //mark all children models
      for(dword i=0; i<cont.frames.size(); i++)
         cont.frames[i]->frm_flags |= FRMFLAGS_SUBMODEL;
      SetName(name);
   }
   if(as->NumLinks())
      anim_set = as;

   as->Release();

   return ir;
}

//----------------------------
//----------------------------

I3D_RESULT I3D_model_imp::SetAnimation(dword stage, CPI3D_animation_set ap,
   dword options, float scale_factor, float blend_factor, float speed_factor){

   if(!ap)
      return I3DERR_INVALIDPARAMS;
                              //allow assigning animations only on frames in our container
   typedef map<C_str, PI3D_frame> t_fmap;
   t_fmap frm_map;
   C_vector<C_smart_ptr<I3D_frame> > &own_frames = cont.GetFrameVector();
   for(int i=own_frames.size(); i--; ){
      PI3D_frame frm = own_frames[i];
      frm_map[frm->GetOrigName()] = frm;
   }

   StopAnimation(stage);

   bool was_empty = (!anim_stages.size());
                              //create stage on the index
   C_anim_stage *anim_stg = (*anim_stages.insert(pair<dword, C_anim_stage*>(
      stage, new C_anim_stage(stage, options, scale_factor, blend_factor, speed_factor, ap->GetTime()))).first).second;
   anim_stg->Release();

   C_vector<C_smart_ptr<C_interpolator_base> > tmp_intps;
   tmp_intps.reserve(frm_map.size());

   const I3D_animation_set::S_anim_link *lp = ap->GetAnimLinks();
   for(i=0; i<(int)ap->NumLinks(); i++){
      const I3D_animation_set::S_anim_link &al = lp[i];
      t_fmap::const_iterator it = frm_map.find(al.link);
      if(it!=frm_map.end()){
         PI3D_frame frm = (*it).second;
         CPI3D_animation_base anim = al.anim;
         C_interpolator_base *intp = NULL;
         switch(anim->GetType1()){
         case I3DANIM_KEYFRAME:
            intp = new C_interpolator_KF((PI3D_keyframe_anim)anim, al.weight);
            break;
         case I3DANIM_POSE:
            intp = new C_interpolator_pose(anim, al.weight);
            break;
         default: assert(0);
         }
         tmp_intps.push_back(intp);
                              //assign to frame's anim processor (create if not present)
         C_frm_anim_processor *ap = NULL;
         for(dword pi=anim_procs.size(); pi--; ){
            if(anim_procs[pi]->frm == frm){
               ap = anim_procs[pi];
            }
         }
         if(!ap){
            ap = new C_frm_anim_processor(frm);
            anim_procs.push_back(ap);
            ap->Release();
                              //associate base position and scale
            for(dword i=frm_base_positions.size(); i--; ){
               if(frm_base_positions[i].first==frm){
                  ap->base_pos = frm_base_positions[i].second;
                  break;
               }
            }
            for(i=frm_base_scales.size(); i--; ){
               if(frm_base_scales[i].first==frm){
                  ap->base_scl = frm_base_scales[i].second;
                  break;
               }
            }
         }
                              //insert interpolator to processor (sort by stage index)
         C_frm_anim_processor::t_proc_intps::iterator its;
         for(its=ap->intps.begin(); its!=ap->intps.end(); its++){
            if((*its).second->stage_index >= stage){
               assert((*its).second->stage_index != stage);
               break;
            }
         }
         ap->intps.insert(its, C_frm_anim_processor::t_proc_intp(intp, anim_stg));

         intp->Release();
      }
   }
   if(!tmp_intps.size()){
      StopAnimation(stage);
      return I3D_OK;
   }
                              //store interpolators into buffer
   anim_stg->interpolators.assign(&tmp_intps.front(), (&tmp_intps.back())+1);
   //anim_stg->interpolators.assign(&tmp_intps[0], tmp_intps.end());

                              //add to upper-level container, if not yet
   if(in_container && was_empty)
      in_container->AddAnimFrame(this);
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_model_imp::SetPose(PI3D_animation_set ap){

                              //allow assigning animations only on frames in our container
   typedef map<C_str, PI3D_frame> t_fmap;
   t_fmap frm_map;
   C_vector<C_smart_ptr<I3D_frame> > &own_frames = cont.GetFrameVector();
   for(int i=own_frames.size(); i--; ){
      PI3D_frame frm = own_frames[i];
      frm_map[frm->GetOrigName()] = frm;
   }

   const I3D_animation_set::S_anim_link *lp = ap->GetAnimLinks();
   for(i=0; i<(int)ap->NumLinks(); i++){
      const I3D_animation_set::S_anim_link &al = lp[i];
      t_fmap::const_iterator it = frm_map.find(al.link);
      if(it!=frm_map.end()){
         PI3D_frame frm = (*it).second;
         CPI3D_animation_base anim = al.anim;

         switch(anim->GetType1()){
         case I3DANIM_KEYFRAME:
            {
               PI3D_keyframe_anim ka = (PI3D_keyframe_anim)anim;
               const C_buffer<I3D_anim_pos_bezier> &pk = ka->GetPositionKeysVector();
               if(pk.size())
                  frm->SetPos(pk.front().v);
               const C_buffer<I3D_anim_quat_bezier> &rk = ka->GetRotationKeysVector();
               if(rk.size())
                  frm->SetRot(rk.front().q);
               const C_buffer<I3D_anim_power> &sk = ka->GetPowerKeysVector();
               if(sk.size())
                  frm->SetScale(sk.front().power);
            }
            break;
         case I3DANIM_POSE:
            {
               PI3D_anim_pose ap = (PI3D_anim_pose)anim;
               dword flg = ap->GetFlags();
               if(flg&I3D_anim_pose::APOSF_POS)
                  frm->SetPos(ap->GetPos1());
               if(flg&I3D_anim_pose::APOSF_ROT)
                  frm->SetRot(ap->GetRot1());
               if(flg&I3D_anim_pose::APOSF_POW)
                  frm->SetScale(ap->GetPower1());
            }
            break;
         default: assert(0);
         }
      }
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_model_imp::StopAnimation(dword stage){ 

   t_anim_stages::iterator it = anim_stages.find(stage);
   if(it==anim_stages.end())
      return I3DERR_OBJECTNOTFOUND;
   C_anim_stage *stg = (*it).second;

                              //erase stage from all processors
   for(dword pi=anim_procs.size(); pi--; ){
      C_frm_anim_processor *ap = anim_procs[pi];
      C_frm_anim_processor::t_proc_intps::iterator it;
      for(it = ap->intps.begin(); it!=ap->intps.end(); it++){
         if(stg==(*it).second){
            ap->intps.erase(it);
                              //if no more interpolators in the processor, erase it
            if(!ap->intps.size()){
               anim_procs[pi] = anim_procs.back(); anim_procs.pop_back();
            }
            break;
         }
      }

   }

   anim_stages.erase(it);

                              //remove from upper-level container, if no anims set
   if(in_container && !IsAnimated())
      in_container->RemoveAnimFrame(this);
   return I3D_OK; 
};

//----------------------------

void C_frm_anim_processor::EvalSingleStage(C_vector<C_note_data> &note_data,
   t_proc_intps::iterator it, C_anim_results_blend &results, dword brace_count){

   C_interpolator_base *intp = (*it).first;
   C_anim_stage *stg = (*it).second;
   dword stg_index = stg->stage_index;
   dword stg_opts = stg->options;

   bool is_damp = ((stg_opts&I3DANIMOP_APPLY_MASK) == I3DANIMOP_DAMP_ADD);

   dword new_brace_count = stg_opts&I3DANIMOP_BRACE_MASK;
   if(new_brace_count){
      new_brace_count >>= I3DANIMOP_BRACE_SHIFT;
      assert(!brace_count || (new_brace_count >= brace_count));
                              //try to blend with next stage first, then with current results
      C_frm_anim_processor::t_proc_intps::iterator it_next = it; ++it_next;
      if(it_next == intps.end())
         goto process_simple;

      //C_anim_stage *stg = (*it).second;
      dword next_stg_index = (*it_next).second->stage_index;
      dword index_delta = next_stg_index - stg_index;
                              //check if next stage is inside of our brackets
      if(index_delta > new_brace_count)
         goto process_simple;

      if(intp->GetAnim()->GetType1()==I3DANIM_KEYFRAME)
         ((C_interpolator_KF*)intp)->ProcessSpecialTracks(stg->curr_time, frm, stg_index, note_data);
                              //evaluate current results into temporary storage
      C_anim_results_blend tmp_res;
      intp->Evaluate(stg->curr_time, tmp_res, base_pos, base_scl);
      if(!stg->is_unit_scale)
         tmp_res.Scale(stg->scale_factor);
                              //eval next stage(s) with these results
      EvalSingleStage(note_data, it_next, tmp_res, new_brace_count - index_delta + 1);
                              //now merge bracketed results with current ones
      if(is_damp)
         results.Scale(1.0f-tmp_res.weight);
      results.MergeWith(tmp_res, stg->options, stg->blend_factor);
                        //make sure we skip stages already blended
      it = it_next;
      while(it!=intps.end() && (*it).second->stage_index < stg_index+index_delta)
         ++it;

      if(brace_count)
         brace_count = Max(0, (int)brace_count - (int)new_brace_count);
   }else{
process_simple:
      if(intp->GetAnim()->GetType1()==I3DANIM_KEYFRAME)
         ((C_interpolator_KF*)intp)->ProcessSpecialTracks(stg->curr_time, frm, stg_index, note_data);

                              //evaluate all interpolators
      C_anim_results_blend tmp_res;
      intp->Evaluate(stg->curr_time, tmp_res, base_pos, base_scl);
      if(!stg->is_unit_scale)
         tmp_res.Scale(stg->scale_factor);
      if(is_damp && tmp_res.weight)
         results.Scale(1.0f-tmp_res.weight);
      results.MergeWith(tmp_res, stg_opts, stg->blend_factor);
   }

                              //make next stage, if not at the end
   ++it;
   if(it!=intps.end()){
      if(brace_count){
                              //check if we'are allowed to blend with next (precedence)
         dword index_delta = (*it).second->stage_index - stg_index;
         if(index_delta < brace_count){
            EvalSingleStage(note_data, it, results, brace_count-index_delta);
         }
      }else{
         EvalSingleStage(note_data, it, results);
      }
   }
}

//----------------------------

I3D_RESULT I3D_model_imp::Tick(int time, PI3D_CALLBACK cb, void *cbc){

   PROFILE(drv, PROF_ANIMS);

   bool anim_running = false;
   if(anim_stages.size()){
                              //advance all stages by time
      C_vector<C_anim_stage*> killed_stages;
      C_vector<C_note_data> note_data;

      t_anim_stages::iterator it, it_next;
      for(it = anim_stages.begin(); it!=anim_stages.end(); it++){
         C_anim_stage *stg = (*it).second;
         if(stg->curr_time != stg->end_time){
            stg->curr_time += (float)time * stg->speed_factor;
            if(stg->curr_time >= stg->end_time){
                              //let special interpolators finish their work to the end
               float save_curr_time = stg->curr_time;
               stg->curr_time = stg->end_time;
               for(dword pi=anim_procs.size(); pi--; ){
                  C_frm_anim_processor *ap = anim_procs[pi];
                  for(C_frm_anim_processor::t_proc_intps::iterator it = ap->intps.begin(); it!=ap->intps.end(); it++){
                     if(stg==(*it).second){
                        C_interpolator_base *intp = (*it).first;
                        if(intp->GetAnim()->GetType()==I3DANIM_KEYFRAME)
                           ((C_interpolator_KF*)intp)->ProcessSpecialTracks(stg->curr_time, ap->frm, stg->stage_index, note_data);
                        break;
                     }
                  }
               }
               stg->curr_time = save_curr_time;

               if(!stg->FinishStage())
                  killed_stages.push_back(stg);
            }
         }else
         if((stg->options&I3DANIMOP_LOOP) && stg->end_time){
            stg->FinishStage();
         }
      }
                              //eval all frames' processors
      for(dword pi=anim_procs.size(); pi--; ){
         C_anim_results_blend res;
         C_frm_anim_processor *ap = anim_procs[pi];
         assert(ap->intps.size());

         ap->EvalSingleStage(note_data, ap->intps.begin(), res);

                              //apply results onto frame
         PI3D_frame frm = ap->frm;
         if(res.flags&ANIMRES_POS_OK)
            frm->SetPos(res.pos + ap->base_pos);
         if(res.flags&ANIMRES_ROT_OK)
            frm->SetRot(res.rot);
         if(res.flags&ANIMRES_POW_OK)
            frm->SetScale(res.pow + ap->base_scl);
      }

                              //remove all killed stages
      for(dword ki=killed_stages.size(); ki--; ){
         C_smart_ptr<C_anim_stage> stg = killed_stages[ki];
         StopAnimation(stg->stage_index);
         if((stg->options&I3DANIMOP_FINISH_NOTIFY) && cb)
            cb(I3DCB_ANIM_FINISH, 0, stg->stage_index, cbc);
      }

                              //process note data (front to back)
      if(cb){
         for(dword ni=0; ni<note_data.size(); ni++){
            C_note_data &nd = note_data[ni];
            I3D_note_callback nc = { nd.curr_time, nd.note_time, nd.stage_index, nd.note};
            cb(I3DCB_ANIM_NOTE, (dword)(PI3D_frame)nd.frm, (dword)&nc, cbc);
         }
      }

      anim_running = (anim_stages.size());
   }

   I3D_RESULT irc = cont.Tick(time, cb, cbc);
   if(irc==I3D_DONE)
      return !anim_running ? I3D_DONE : I3D_OK;
   return irc;
}

//----------------------------

PI3D_model CreateModel(CPI3D_scene s){
   return new I3D_model_imp(s);
}

//----------------------------
//----------------------------

