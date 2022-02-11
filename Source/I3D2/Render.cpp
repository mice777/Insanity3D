/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Render.cpp
   Content: Rendering pipeline.
--------------------------------------------------------*/

#include "all.h"
#include "light.h"
#include "camera.h"
#include "scene.h"
#include "model.h"
#include "visual.h"
#include "occluder.h"
#include "dummy.h"
#include "joint.h"
#include "volume.h"
#include "sng_mesh.h"
#include "soundi3d.h"
#include "mesh.h"
#ifndef GL
#include "stripifier.h"
#endif


#define MAX_COMPUTED_OCCLUDERS 14

#define SHADOW_LENGTH 8.0f    //length of cast shadows
const float HIDE_NEAR_DIST_RATIO = .8f;   //ratio of hide dist, from which visuals start to fade out

                              //debugging:
//#define DEBUG_FORCE_CLIP      //always pass clip flags to AddPrimitives - D3D VB bug?!

//#define DEBUG_NO_CLIP_PLANES  //disable hw clipping planes

//#define DEBUG_MIRROR_SPECIAL_BACKGROUND //fill mirror's background to special color

#ifdef GL
#define DEBUG_NO_CLIP_PLANES  //disable hw clipping planes
#endif

//----------------------------

void I3D_scene::PreprocessSounds(int time){

   if(!sounds.size())
      return;
                              //make sure camera's matrix is up to date
   current_camera->ChangedMatrix();
   PI3D_sound *sptr = &sounds.front();
   for(int i=sounds.size(); i--; ){
      PI3D_sound snd = *sptr++;
#if defined USE_PREFETCH && 0
      if(i) Prefetch1(*sptr);
#endif
      PI3D_frame prnt = snd;
      while(prnt=prnt->parent, prnt){
         if(prnt==primary_sector){
            snd->ChangedMatrix();
            snd->UpdateSound(time);
            break;
         }
      }
   }
}

//----------------------------

void I3D_scene::PreprocessOccluders(const S_preprocess_context &pc, PI3D_frame root,
   C_sort_list<PI3D_occluder> &collect_list, PI3D_sector curr_sector){

   const C_vector<PI3D_frame> &frm_list = root->children;
   const PI3D_frame *frms = &frm_list.front();
   int i = frm_list.size();         //can't be zero!
   do{
      PI3D_frame frm = *(frms++);

      switch(frm->type){
      case FRAME_SECTOR:
         {
            PI3D_sector sct = I3DCAST_SECTOR(frm);
            if(sct->IsVisible() && frm->NumChildren1()){
               PreprocessOccluders(pc, frm, collect_list, sct);
            }
         }
         continue;

      case FRAME_OCCLUDER:
         {
            if(frm->frm_flags&FRMFLAGS_CHANGE_MAT){
                              //re-compute world matrix
               frm->UpdateMatrices();
               frm->matrix.Make(frm->f_matrix, root->matrix);
               frm->frm_flags &= ~(FRMFLAGS_INV_MAT_VALID | FRMFLAGS_CHANGE_MAT | FRMFLAGS_BSPHERE_TRANS_VALID);
            }
                              //determine if it's visible
            PI3D_occluder occ = I3DCAST_OCCLUDER(frm);

            occ->bound.GetBoundSphereTrans(occ, FRMFLAGS_BSPHERE_TRANS_VALID);
            bool clip_vf;
            bool is_visible =
               curr_sector->IsBoundVolumeVisible(occ->bound, frm->matrix,
                  current_camera->GetCurrSector(), pc, &clip_vf,
                  NULL);
            if(!is_visible)
               break;

            {                 //put into temporary list, compute frustum when it's definite
               assert(frm->frm_flags&FRMFLAGS_BSPHERE_TRANS_VALID);
               const S_vector &occ_pos = occ->bound.bsphere_world.pos;
               float dist_to_cam_2 = (current_camera->matrix(3) - occ_pos).Square();
               occ->SetWorkSector(curr_sector);
               collect_list.Add(occ, FloatToInt(dist_to_cam_2 * 10.0f));
            }
            if(!frm->num_hr_occl)
               continue;
         }
         break;

      case FRAME_VISUAL:
      case FRAME_MODEL:
      case FRAME_DUMMY:
         if(!frm->IsOn1())
            continue;
                              //flow...
      default: 
         {
                              //don't update matrix if no occluders are linked to this frame
            if(!frm->num_hr_occl)
               continue;
            if(frm->frm_flags&FRMFLAGS_CHANGE_MAT){
                                    //update matrix
               frm->UpdateMatrices();
               frm->matrix.Make(frm->f_matrix, root->matrix);
               frm->frm_flags &= ~(FRMFLAGS_INV_MAT_VALID | FRMFLAGS_CHANGE_MAT | FRMFLAGS_BSPHERE_TRANS_VALID);
            }
         }
      }
      if(frm->NumChildren1())
         PreprocessOccluders(pc, frm, collect_list, curr_sector);
   } while(--i);
}

//----------------------------

#define SAVE_LIGHT_MARK if(prep_flags&E_PREP_SET_HR_LIGHT_DIRTY) frm->frm_flags |= FRMFLAGS_HR_LIGHT_RESET;
#define PROPAGATE_LIGHT_MARK if(frm->frm_flags&FRMFLAGS_HR_LIGHT_RESET){\
   new_prep_flags |= E_PREP_SET_HR_LIGHT_DIRTY;\
   frm->frm_flags &= ~FRMFLAGS_HR_LIGHT_RESET; }

void I3D_scene::PreprocessChildren(PI3D_frame root, S_preprocess_context &pc, dword prep_flags){

   const C_vector<PI3D_frame> &frm_list = root->children;
   const PI3D_frame *frms = &frm_list.front();

   int num_hr_vis = root->num_hr_vis;
   assert(num_hr_vis);

   do{
      dword new_prep_flags = prep_flags;
      PI3D_frame frm = *frms++;
                              //stop as soon as we exhaust frames containing visuals
      num_hr_vis -= frm->num_hr_vis;

#ifdef _DEBUG
      //++drv->scene_stats.num_vis_computed;
#endif
                              //make proper frame matrix
      bool change_mat_loc = (frm->frm_flags&FRMFLAGS_CHANGE_MAT);

      if(change_mat_loc){
         frm->UpdateMatrices();
         frm->matrix.Make(frm->f_matrix, root->matrix);
         frm->frm_flags &= ~(FRMFLAGS_INV_MAT_VALID | FRMFLAGS_CHANGE_MAT | FRMFLAGS_BSPHERE_TRANS_VALID);
      }

                              //propagate light-reset on this frame
      switch(frm->type){

      case FRAME_MODEL:
         {
            bool is_on = (frm->frm_flags&FRMFLAGS_ON);
            if(!frm->num_hr_vis || (prep_flags&E_PREP_NO_VISUALS)){
               SAVE_LIGHT_MARK;
               break;
            }

            bool is_caster = (frm->frm_flags&I3D_FRMF_SHADOW_CAST) &&
               (prep_flags&E_PREP_ADD_SHD_CASTERS);

            if(frm->frm_flags&I3D_FRMF_FORCE_SHADOW){
               if(pc.mode==RV_NORMAL && !is_caster){
                  SAVE_LIGHT_MARK;
                  break;
               }
            }else
            if(!is_on){
               SAVE_LIGHT_MARK;
               break;
            }

            PI3D_model mod = I3DCAST_MODEL(frm);

            bool do_hr_test = (frm->num_hr_vis >= 2 || is_caster);
            
                              //visibility tests
            if(do_hr_test){
               bool off_sector = (pc.curr_sector!=current_camera->GetCurrSector());
               if(off_sector || !(new_prep_flags&E_PREP_HR_NO_CLIP)){
                  if(!(frm->frm_flags&FRMFLAGS_HR_BOUND_VALID))
                     mod->hr_bound.MakeHierarchyBounds(frm);
                  mod->hr_bound.GetBoundSphereTrans(frm, FRMFLAGS_HR_BSPHERE_VALID);

                  bool clip_vf, clip_occ;
                  bool is_visible =
                     pc.curr_sector->IsBoundVolumeVisible(mod->hr_bound, frm->matrix,
                        current_camera->GetCurrSector(), pc,
                        !(new_prep_flags&E_PREP_HR_NO_CLIP) ? &clip_vf : NULL,
                        (new_prep_flags&E_PREP_TEST_OCCLUDERS) ? &clip_occ : NULL);
                  if(!is_visible){
#ifndef GL
                     if(is_caster){
                              //shadow casters may cast shadow even if their bounding box is out of view
                              // brief check - expanded bounding sphere, enclosing any possible shadow from the model
                        I3D_bsphere bsphere;
                        bsphere.pos = frm->matrix(3);
                        bsphere.radius = Max(mod->hr_bound.bsphere_world.radius, SHADOW_LENGTH);

                        bool shd_visible = pc.curr_sector->IsBoundSphereVisible(bsphere,
                           current_camera->GetCurrSector(), pc,
                           &clip_vf, (new_prep_flags&E_PREP_TEST_OCCLUDERS) ? &clip_occ : NULL);
                        if(shd_visible){
                           mod->frm_flags |= FRMFLAGS_SHD_CASTER_NONVIS;
                           pc.shadow_casters.push_back(mod);
                           new_prep_flags &= ~E_PREP_ADD_SHD_CASTERS;
                        }
                     }
#endif
                     SAVE_LIGHT_MARK;
                     break;
                  }
                  if(!(new_prep_flags&E_PREP_HR_NO_CLIP) && !clip_vf)
                     new_prep_flags |= E_PREP_HR_NO_CLIP;
                  if((new_prep_flags&E_PREP_TEST_OCCLUDERS) && !clip_occ)
                     new_prep_flags &= ~E_PREP_TEST_OCCLUDERS;
               }
            }
#ifndef GL
            if(is_caster){
               mod->frm_flags &= ~FRMFLAGS_SHD_CASTER_NONVIS;
               pc.shadow_casters.push_back(mod);
               new_prep_flags &= ~E_PREP_ADD_SHD_CASTERS;
               if(pc.mode==RV_NORMAL && !is_on && (frm->frm_flags&I3D_FRMF_FORCE_SHADOW)){
                  SAVE_LIGHT_MARK;
                  break;
               }
            }
#endif
            PROPAGATE_LIGHT_MARK;
            PreprocessChildren(frm, pc, new_prep_flags);
         }
         break;

      case FRAME_VISUAL:
         {
            --num_hr_vis;
            PI3D_visual vis = I3DCAST_VISUAL(frm);
                              //if matrix changed, schedule to recompute lights
            if(change_mat_loc) 
               vis->vis_flags &= ~VISF_DEST_LIGHT_VALID;

            if(!(frm->frm_flags&FRMFLAGS_ON) || (prep_flags&E_PREP_NO_VISUALS)){
               SAVE_LIGHT_MARK;
               break;
            }
                              //visibility tests
            float force_alpha = pc.force_alpha;

                              //hide distance test
            if(I3DFloatAsInt(vis->hide_dist)!=0){
               float d_2 = (current_camera->matrix(3) - frm->matrix(3)).Square();
               d_2 *= pc.LOD_factor*pc.LOD_factor;
               if(d_2 > vis->hide_dist*vis->hide_dist){
                  SAVE_LIGHT_MARK;
                  break;
               }
               float hide_near = vis->hide_dist * HIDE_NEAR_DIST_RATIO;
               if(d_2 > hide_near*hide_near){
                  float d = I3DSqrt(d_2) - hide_near;
                  force_alpha *= 1.0f - d / (vis->hide_dist - hide_near);
               }
            }
            
                              //hierarchy bounding test
            if(frm->num_hr_vis >= 2){
               if(!(new_prep_flags&E_PREP_HR_NO_CLIP) ||
                  (new_prep_flags&E_PREP_TEST_OCCLUDERS) ||
                  pc.curr_sector!=current_camera->GetCurrSector()){

                  if(!(frm->frm_flags&FRMFLAGS_HR_BOUND_VALID))
                     vis->hr_bound.MakeHierarchyBounds(frm);

                  vis->hr_bound.GetBoundSphereTrans(frm, FRMFLAGS_HR_BSPHERE_VALID);
                  bool clip_vf, clip_occ;
                  bool is_visible =
                     pc.curr_sector->IsBoundVolumeVisible(vis->hr_bound, frm->matrix,
                        current_camera->GetCurrSector(), pc,
                        !(new_prep_flags&E_PREP_HR_NO_CLIP) ? &clip_vf : NULL,
                        (new_prep_flags&E_PREP_TEST_OCCLUDERS) ? &clip_occ : NULL);
                  if(!is_visible){
                     SAVE_LIGHT_MARK;
                     break;
                  }
                  if(!(new_prep_flags&E_PREP_HR_NO_CLIP) && !clip_vf)
                     new_prep_flags |= E_PREP_HR_NO_CLIP;
                  if((new_prep_flags&E_PREP_TEST_OCCLUDERS) && !clip_occ)
                     new_prep_flags &= ~E_PREP_TEST_OCCLUDERS;
               }
            }
                              //propagate light reset and set to visual
            if((frm->frm_flags&FRMFLAGS_HR_LIGHT_RESET) || (prep_flags&E_PREP_SET_HR_LIGHT_DIRTY)){
               new_prep_flags |= E_PREP_SET_HR_LIGHT_DIRTY;
               frm->frm_flags &= ~FRMFLAGS_HR_LIGHT_RESET;
               vis->vis_flags &= ~VISF_DEST_LIGHT_VALID;
            }

            if(!(vis->vis_flags&VISF_BOUNDS_VALID))
               vis->ComputeBounds();
            vis->bound.GetBoundSphereTrans(frm, FRMFLAGS_BSPHERE_TRANS_VALID);

            bool clip_occ, clip_vf, *clipp = &clip_vf;
            if(new_prep_flags&E_PREP_HR_NO_CLIP){
               clip_vf = false;
               clipp = NULL;
            }
            bool is_visible =
               pc.curr_sector->IsBoundVolumeVisible(vis->bound, frm->matrix, current_camera->GetCurrSector(), pc, clipp,
                  (new_prep_flags&E_PREP_TEST_OCCLUDERS) ? &clip_occ : NULL);
            if(is_visible){

                              //visual is visible
               /*
#ifdef DEBUG_FORCE_CLIP
               clip_vf = true;
#endif
               ++debug_count[clip_vf];
               */
                              //stats
               ++scene_stats.frame_count[FRAME_VISUAL];
               //scene_stats.num_vis_computed += (int)clip_vf;
               ++scene_stats.num_vis_computed;

               //pc.clip = clip_vf;
               if(*(dword*)&force_alpha==FLOAT_ONE_BITMASK){
                  vis->AddPrimitives(pc);
                  if(frm->num_hr_vis)
                     PreprocessChildren(frm, pc, new_prep_flags);
               }else{
                  byte save_alpha = vis->alpha;
                  vis->alpha = (byte)FloatToInt((float)vis->alpha * force_alpha);

                  vis->ResetLightInfo();
                  vis->AddPrimitives(pc);
                  vis->ResetLightInfo();

                  vis->alpha = save_alpha;

                              //process children (with current forced alpha)
                  float save_force = pc.force_alpha;
                  pc.force_alpha = force_alpha;

                  if(frm->num_hr_vis)
                     PreprocessChildren(frm, pc, new_prep_flags);
                  
                  pc.force_alpha = save_force;
               }

#ifndef GL
               if(pc.mode!=RV_SHADOW_CASTER)
#endif
               {
                  vis->last_render_time = last_render_time;
#ifndef GL
                  if(frm->frm_flags&I3D_FRMF_SHADOW_RECEIVE)
                     pc.shadow_receivers.push_back(vis);
#endif
               }
            }else{
                              //not visible (out of view)
               if(frm->num_hr_vis)
                  PreprocessChildren(frm, pc, new_prep_flags);
            }
         }
         break;

      case FRAME_SECTOR:
         {
            PI3D_sector sct = I3DCAST_SECTOR(frm);
                              //if sector is not visible, children are also not
            if(!sct->IsVisible())
               break;
            if(sct->IsSimple()){
                              //it's visible if parent sector is visible
               if(prep_flags&E_PREP_NO_VISUALS)
                  break;
            }

            ++scene_stats.frame_count[FRAME_SECTOR];
                              //save sector & global light settings
            PI3D_sector save_sct = pc.curr_sector;
                              //set our global settings
            pc.curr_sector = sct;
            new_prep_flags &= (E_PREP_ADD_SHD_CASTERS | E_PREP_TEST_OCCLUDERS);

                              //update all lights
            if(sct->frm_flags&FRMFLAGS_HR_LIGHT_RESET){
               sct->UpdateAllLights();
               new_prep_flags |= E_PREP_SET_HR_LIGHT_DIRTY;
            }

            assert(frm->num_hr_vis);
            PreprocessChildren(frm, pc, new_prep_flags);
                              //restore global settings
            pc.curr_sector = save_sct;
         }
         break;

      case FRAME_DUMMY:
         if(!(frm->frm_flags&FRMFLAGS_ON)){
            SAVE_LIGHT_MARK;
            break;
         }
                              //flow...
      default:
         assert(frm->num_hr_vis);
         PROPAGATE_LIGHT_MARK;
         PreprocessChildren(frm, pc, new_prep_flags);
      }
      assert(num_hr_vis >= 0);
   } while(num_hr_vis);
}

//----------------------------

void I3D_scene::Preprocess(S_preprocess_context &pc, dword prep_flags){

   PROFILE(drv, PROF_PREPROCESS);

   assert(!pc.occ_list.size());

   if(!primary_sector->NumChildren1())
      return;

   if(drv->GetFlags2()&DRVF2_USE_OCCLUSION)
   if(primary_sector->num_hr_occl){
                              //collect all visible occluders
      C_sort_list<PI3D_occluder> occ_list1(primary_sector->num_hr_occl);
      pc.curr_sector = primary_sector;
      PreprocessOccluders(pc, primary_sector, occ_list1, primary_sector);
      pc.curr_sector = NULL;
                              //sort them by distance to camera
      occ_list1.Sort();
      pc.occ_list.reserve(occ_list1.Count());
                              //process sorted occluders back-to-front, try to hide occluded ones
      for(dword i=0; i<occ_list1.Count(); i++){
                              //this loop must go upwards
         PI3D_occluder occ1 = occ_list1[i];

         bool occluded = false;
         for(dword j=0; j<pc.occ_list.size(); j++){
            PI3D_occluder occ = pc.occ_list[j];

                              //test if occ1 is occluded by occ
            bool clip;

            occ1->bound.GetBoundSphereTrans(occ1, FRMFLAGS_BSPHERE_TRANS_VALID);
            occluded = occ->IsOccluding(occ1->bound, occ1->matrix, clip);
            if(occluded)
               break;
         }
         if(!occluded){
                              //compute frustum now
            bool b;
                              //we could make ComputeClipPlanes virtual method
                              // of I3D_occluder_base, but is would make mismatch
                              // with user's include files, as they omit I3D_occluder_base
            b = occ1->ComputeOccludingFrustum(this);
            if(b){
                              //definitely add to list
                              //set debug flags
               occ1->occ_flags |= OCCF_DEBUG_OCCLUDING;
               ++scene_stats.frame_count[FRAME_OCCLUDER];
               pc.occ_list.push_back(occ1);
            }
         }else{
                              //invisible occluder, if it's sector, set it invisible
            if(occ1->GetType1()==FRAME_SECTOR){
               PI3D_sector sct = I3DCAST_SECTOR(occ1);
               sct->SetSctFlags(sct->GetSctFlags() & ~SCTF_VISIBLE);
            }
         }
      }
      if(pc.occ_list.size()){
                              //limit number of occluders
         if(pc.occ_list.size() > MAX_COMPUTED_OCCLUDERS)
            pc.occ_list.erase(pc.occ_list.begin()+MAX_COMPUTED_OCCLUDERS, pc.occ_list.end());

                              //hide portals behind occluders
         //current_camera->GetCurrSector()->SolveOcclusion(pc.occ_list);
      }
   }
   pc.curr_sector = primary_sector;

   if(pc.occ_list.size())
      prep_flags |= E_PREP_TEST_OCCLUDERS;
   if(pc.curr_sector->num_hr_vis){
      PreprocessChildren(pc.curr_sector, pc, prep_flags);
   }
   //PreprocessSectors(pc.curr_sector, pc);
   assert(pc.prim_list.size() == (pc.opaque + pc.ckey_alpha + pc.alpha_zwrite + pc.alpha_nozwrite + pc.alpha_noz));
}

//----------------------------

void I3D_scene::DrawListPS(const S_preprocess_context &pc){

   int i = 0;
   int j;

                              // --opaque--
   for(j=pc.opaque; i<j; ++i){
      const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[i]];
      PI3D_light lf = ct.sector->GetFogLight();
      if(lf) drv->SetFogColor(lf->dw_color);
      ct.vis->DrawPrimitivePS(pc, ct);
   }
                              
                              // --color-keyed--
   if(pc.ckey_alpha){
      //drv->SetupAlphaTest(true);
      i = pc.ckey_alpha;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
         if(lf) drv->SetFogColor(lf->dw_color);
         ct.vis->DrawPrimitivePS(pc, ct);
      }while(i);
      //drv->SetupAlphaTest(false);
      j += pc.ckey_alpha;
   }
                         
                              // --alpha with z-writes--
   if(pc.alpha_zwrite){
      i = pc.alpha_zwrite;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
                              //avoid blending ADD mode with fog
         if(lf){
            bool add_mode = ((ct.blend_mode&0xffff) == I3DBLEND_ADD);
            drv->SetFogColor(add_mode ? 0 : lf->dw_color);
         }
         ct.vis->DrawPrimitivePS(pc, ct);
      }while(i);
      j += pc.alpha_zwrite;
   }
#ifndef GL
   if((drv->GetFlags()&DRVF_USESHADOWS) &&
      pc.shadow_casters.size() && pc.shadow_receivers.size() &&
      (drv->GetFlags()&DRVF_CANRENDERSHADOWS)){

      RenderShadows(pc);
   }
#endif

                              // --alpha without z-writes--
   if(pc.alpha_nozwrite){
      drv->EnableZWrite(false);
      i = pc.alpha_nozwrite;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
                              //avoid blending ADD mode with fog
         if(lf){
            bool add_mode = ((ct.blend_mode&0xffff) == I3DBLEND_ADD);
            drv->SetFogColor(add_mode ? 0 : lf->dw_color);
         }
         ct.vis->DrawPrimitivePS(pc, ct);
      }while(i);
      j += pc.alpha_nozwrite;
   }

   drv->EnableZWrite(true);

                              // --alpha with no z-buffer--
   if(pc.alpha_noz){
      //bool use_zb = (drv->GetFlags()&DRVF_USEZB);
      //if(use_zb) drv->SetState(RS_USEZB, false);
      drv->EnableZBUsage(false);
      i = pc.alpha_noz;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
                              //avoid blending ADD mode with fog
         if(lf){
            bool add_mode = ((ct.blend_mode&0xffff) == I3DBLEND_ADD);
            drv->SetFogColor(add_mode ? 0 : lf->dw_color);
         }
         ct.vis->DrawPrimitivePS(pc, ct);
      }while(i);
      j += pc.alpha_noz;
      //if(use_zb) drv->SetState(RS_USEZB, true);
      drv->EnableZBUsage(true);
   }
   drv->EnableFog(false);
}

//----------------------------

void I3D_scene::DrawList(const S_preprocess_context &pc){

   PROFILE(drv, PROF_DRAW);
#ifndef GL
   if(drv->CanUsePixelShader())
#endif
   {
      DrawListPS(pc);
      return;
   }
#ifndef GL
   int i = 0;
   int j;

                              // --opaque--
   for(j=pc.opaque; i<j; ++i){
      const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[i]];
                              //setup fog
      PI3D_light lf = ct.sector->GetFogLight();
      if(lf) drv->SetFogColor(lf->dw_color);
      ct.vis->DrawPrimitive(pc, ct);
   }
                              
                              // --color-keyed--
   if(pc.ckey_alpha){
      //drv->SetupAlphaTest(true);
      i = pc.ckey_alpha;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
         if(lf) drv->SetFogColor(lf->dw_color);
         ct.vis->DrawPrimitive(pc, ct);
      }while(i);
      //drv->SetupAlphaTest(false);
      j += pc.ckey_alpha;
   }
                         
                              // --alpha with z-writes--
   if(pc.alpha_zwrite){
      i = pc.alpha_zwrite;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
                              //avoid blending ADD mode with fog
         if(lf){
            bool add_mode = ((ct.blend_mode&0xffff) == I3DBLEND_ADD);
            drv->SetFogColor(add_mode ? 0 : lf->dw_color);
         }
         ct.vis->DrawPrimitive(pc, ct);
      }while(i);
      j += pc.alpha_zwrite;
   }
#ifndef GL
   if((drv->GetFlags()&DRVF_USESHADOWS) &&
      pc.shadow_casters.size() && pc.shadow_receivers.size() &&
      (drv->GetFlags()&DRVF_CANRENDERSHADOWS)){

      RenderShadows(pc);
   }
#endif
                              // --alpha without z-writes--
   if(pc.alpha_nozwrite){
      drv->EnableZWrite(false);
      i = pc.alpha_nozwrite;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
                              //avoid blending ADD mode with fog
         if(lf){
            bool add_mode = ((ct.blend_mode&0xffff) == I3DBLEND_ADD);
            drv->SetFogColor(add_mode ? 0 : lf->dw_color);
         }
         ct.vis->DrawPrimitive(pc, ct);
      }while(i);
      j += pc.alpha_nozwrite;
   }

   drv->EnableZWrite(true);

                              // --alpha with no z-buffer--
   if(pc.alpha_noz){
      //bool use_zb = (drv->GetFlags()&DRVF_USEZB);
      //if(use_zb) drv->SetState(RS_USEZB, false);
      drv->EnableZBUsage(false);
      i = pc.alpha_noz;
      do{
         --i;
         const S_render_primitive &ct = pc.prim_list[pc.prim_sort_list[j+i]];
                              //setup fog
         PI3D_light lf = ct.sector->GetFogLight();
                              //avoid blending ADD mode with fog
         if(lf){
            bool add_mode = ((ct.blend_mode&0xffff) == I3DBLEND_ADD);
            drv->SetFogColor(add_mode ? 0 : lf->dw_color);
         }
         ct.vis->DrawPrimitive(pc, ct);
      }while(i);
      j += pc.alpha_noz;
      //if(use_zb) drv->SetState(RS_USEZB, true);
      drv->EnableZBUsage(true);
   }

   drv->EnableFog(false);
   drv->DisableTextureStage(1);
#endif
}

//----------------------------

PI3D_light I3D_scene::SelectShadowLight(PI3D_sector sct, const S_vector &visual_pos,
   PI3D_frame last_light, float dist_to_cam, float &opacity) const{

   PI3D_light lp = NULL;
   float lp_power = 0.0f;
   float amb_power = 0.0f;

   const C_vector<PI3D_light> &sct_lights = sct->GetLights_shadow();
   const PI3D_light *lptr = &sct_lights.front();
   for(int li=sct_lights.size(); li--; ){
      PI3D_light lp1 = *lptr++;
      if(!lp1->IsOn1())
         continue;
      const S_vector &c = lp1->color;

      if(lp1->GetLightType1() == I3DLIGHT_AMBIENT){
         float power = (c[0] * .3f + c[1] * .6f + c[2] * .1f) * lp1->power;
         amb_power += power;
         continue;
      }

      {
                        //compute this light's power
         float power = (c[0] * .3f + c[1] * .6f + c[2] * .1f) * lp1->power;

         switch(lp1->GetLightType1()){
         case I3DLIGHT_POINT:
            {
               const S_matrix &m_light = lp1->GetMatrix();

               const float nr = lp1->range_n_scaled;
               const float fr = lp1->range_f_scaled;
               float ldist_2 = (m_light(3) - visual_pos).Square();
                        //consider attenuation
               if(ldist_2 >= fr*fr)
                  continue;
               if(ldist_2 >= nr*nr){
                  float ldist = I3DSqrt(ldist_2);
                  power *= (fr - ldist) / (fr - nr);
               }
            }
            break;
         case I3DLIGHT_DIRECTIONAL:
            power *= .2f;
            break;
         default:
            continue;
         }
                        //give current light greater priority
         if(lp1==last_light)
            power *= 1.2f;

         if(lp){
                           //check if it's stronger than light we already have
            if(lp_power >= power)
               continue;
         }
         lp = lp1;
         lp_power = power;
      }
   }
   if(!lp)
      return NULL;

#if 0
                              //apply fog intensity
   const C_vector<PI3D_light> &sct_fogs = sct->GetFogs();
   lptr = sct_fogs.begin();
   for(li=sct_fogs.size(); li--; ){
      PI3D_light lp1 = *lptr++;
      if(!lp1->IsOn1())
         continue;
      if(lp1->GetLightType1() == I3DLIGHT_FOG){
                        //compute fog on per-object basis
         float fog_range_f = lp1->range_f;
         if(IsAbsMrgZero(fog_range_f))
            fog_range_f = current_camera->GetFCP();
         float fog_range_n = lp1->range_n * fog_range_f;
         if(dist_to_cam >= fog_range_f)
            break;
         if(dist_to_cam >= fog_range_n)
            opacity *= (fog_range_f - dist_to_cam) / (fog_range_f - fog_range_n);
      }
   }
   if(li!=-1)
      return NULL;
#endif

                        //apply ambient - consider its relative ratio to shadow light
   float f = lp_power + amb_power * .333f;
   if(I3DFabs(f) > .001f){
      float amb_ratio = lp_power / f;
      opacity *= amb_ratio;
   }
   if(IsAbsMrgZero(opacity))
      return NULL;
   return lp;
}

//----------------------------
// Compute best matrix from set of projected countour points.
static void GetBestShadowMatrix(const S_vector &normal, const S_vector *projected_contour,
   const S_vector *contour_points, dword num_cpts,
   S_matrix &ret, I3D_bbox &best_bbox){
                        //proposed transformation matrix
   S_matrix prop_matrix;
   prop_matrix.Identity();
   prop_matrix(2) = normal;

   float best_size = 1e+16f;
   for(int i0=num_cpts; i0--; ){
      const S_vector &v0 = projected_contour[i0];
      for(int i1=i0; i1--; ){
         const S_vector &v1 = projected_contour[i1];
         S_vector dir_v0_v1 = v1 - v0;
                           //if verts are too close (identical), skip this vertex
         if(IsMrgZeroLess(dir_v0_v1.Dot(dir_v0_v1)))
            continue;
                           //compute proposed matrix
         prop_matrix(0) = S_normal(dir_v0_v1);
         prop_matrix(1) = prop_matrix(2).Cross(prop_matrix(0));

         S_matrix prop_matrix_inv = ~prop_matrix;
                           //process all points, build bounds (projected to 2D)
         I3D_bbox rc_bbox;
         rc_bbox.Invalidate();

         S_vector v_trans[6];
         TransformVertexArray(contour_points, sizeof(S_vector), num_cpts,
            v_trans, sizeof(S_vector), prop_matrix_inv);

         for(int l=num_cpts; l--; ){
            const S_vector &vt = v_trans[l];
            rc_bbox.min.Minimal(vt);
            rc_bbox.max.Maximal(vt);
         }
                        //save result, if better than previous
         float curr_size = (rc_bbox.max.x-rc_bbox.min.x) * (rc_bbox.max.y-rc_bbox.min.y);
         if(best_size > curr_size){
            best_size = curr_size;
            best_bbox = rc_bbox;
            ret = prop_matrix;
         }
      }
   }
}

//----------------------------
#ifndef GL

void I3D_scene::RenderShadows(const S_preprocess_context &pc){
                              //no shadows in night-view (we've some bug with switching rendertargets)
   if(drv->GetFlags2()&DRVF2_NIGHT_VISION)
      return;
   PROFILE(drv, PROF_SHADOWS);

   assert(drv->tp_shadow);
   shd_camera->Duplicate(current_camera);

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   int debug_show_pos = 0;

   C_render_target<> save_rt = drv->GetCurrRenderTarget();

   for(int sci=pc.shadow_casters.size(); sci--; ){
      PI3D_model mod_sc = pc.shadow_casters[sci];

      const S_matrix &tm = mod_sc->GetMatrix();
      if(!(mod_sc->frm_flags&FRMFLAGS_HR_BOUND_VALID))
         mod_sc->hr_bound.MakeHierarchyBounds(mod_sc);
      const I3D_bbox &bb = mod_sc->hr_bound.bound_local.bbox;
      if(!bb.IsValid())
         continue;

      float opacity = 1.0f;
      S_vector bb_center = ((bb.max + bb.min) * .5f) * tm;
                              //get distance to camera
      float dist_to_cam_2 = (bb_center - pc.viewer_pos).Square();
                              //beyond the max range?
      if(dist_to_cam_2 >= drv->shadow_range_f*drv->shadow_range_f)
         continue;
      float dist_to_cam = I3DSqrt(dist_to_cam_2);
                              //fade shadow depending on distance
      if(dist_to_cam >= drv->shadow_range_n){
         opacity *= (drv->shadow_range_f - dist_to_cam) / (drv->shadow_range_f - drv->shadow_range_n);
      }

      assert(mod_sc->shadow_info);
      I3D_model::C_shadow_info &si = *mod_sc->shadow_info;

                              //find its sector
      PI3D_frame sct_prnt = mod_sc;
      while((sct_prnt=sct_prnt->GetParent(), sct_prnt) && sct_prnt->GetType1()!=FRAME_SECTOR);
      assert(sct_prnt);
      PI3D_sector sct = I3DCAST_SECTOR(sct_prnt);

                              //select light
      PI3D_light lp = SelectShadowLight(sct, bb_center, si.GetLastLight(), dist_to_cam, opacity);
                              //no such light?
      if(!lp){
         si.Reset();
         continue;
      }
      lp->UpdateLight();
      //drv->DebugPoint(lp->GetWorldPos(), .5f, 1);

      S_normal ldir;
      switch(lp->GetLightType1()){
      case I3DLIGHT_DIRECTIONAL: ldir = lp->GetWorldDir(); break;
      default: ldir = bb_center - lp->GetMatrixDirect()(3);
      }

                              //check if we're going to make transition
      if(si.GetLastLight()!=lp){
         if(si.GetLastLight()){
                              //setup dir vector to what it was last frame
            si.trans_base_dir = si.last_dir;
            //si.trans_base_length = si.last_length;
                              //setup starting of transition
            si.transition_count = last_render_time;
         }
         si.SetLastLight(lp);
      }

      if(si.transition_count){
         const int transition_count = 1000;
                              //make smooth transition
         int render_time_delta = last_render_time - si.transition_count;
         if(render_time_delta >= transition_count){
                              //end of transition
            si.transition_count = 0;
         }else{
            float f = (float)render_time_delta / (float)transition_count;
            float fi = 1.0f - f;
            ldir = ldir*f + si.trans_base_dir*fi;
            //shadow_length = shadow_length*f + si.trans_base_length*fi;
         }
      }

                              //save current values
      si.last_dir = ldir;
      //si.last_length = shadow_length;

      int i;
                              //get bbox contour
      S_vector contour_points[6];
      dword num_cpts;
      ComputeContour(bb, tm, ldir, contour_points, num_cpts, true);
      assert(num_cpts);
                              //find matrix for smallest rectangle
         
                              //get projection plane, going through object's center
      S_plane pl;
      pl.normal = ldir;
      pl.d = -bb_center.Dot(pl.normal);

                              //find bbox point fahrtest from the plane
      S_vector bb_full[8], bb_trans[8];
      bb.Expand(bb_full);
      TransformVertexArray(bb_full, sizeof(S_vector), 8, bb_trans, sizeof(S_vector), mod_sc->GetMatrix());
      float d_bb_min = 1e+16f;
      for(i=8; i--; ){
         const S_vector &v = bb_trans[i];
         float d = v.DistanceToPlane(pl);
         if(d_bb_min > d){
            d_bb_min = d;
         }
      }
      d_bb_min = pl.d - d_bb_min;
                              //construct contour frustum
      S_view_frustum vf;
      vf.view_pos = ldir;
      vf.num_clip_planes = 0;
      for(i=num_cpts; i--; ){
         S_plane &cpl = vf.clip_planes[vf.num_clip_planes];
         cpl.normal.GetNormal(contour_points[i], contour_points[(i+1)%num_cpts],
            contour_points[i] + ldir);
         if(cpl.normal.Square() < .001f)
            continue;
         cpl.normal.Normalize();
         cpl.d = -(contour_points[i].Dot(cpl.normal));
         vf.frustum_pts[vf.num_clip_planes] = contour_points[i];
         ++vf.num_clip_planes;
      }
                              //add front and back clipping planes
      vf.clip_planes[vf.num_clip_planes].normal = -pl.normal;
      vf.clip_planes[vf.num_clip_planes].d = -d_bb_min;
      ++vf.num_clip_planes;
      vf.clip_planes[vf.num_clip_planes] = pl;
      vf.clip_planes[vf.num_clip_planes].d -= SHADOW_LENGTH;
      ++vf.num_clip_planes;

      //C_vector<PI3D_visual> hit_receivers;
      PI3D_visual *hit_receivers = (PI3D_visual*)alloca(pc.shadow_receivers.size() * sizeof(PI3D_visual*));
      dword num_receivers = 0;
      //hit_receivers.reserve(pc.shadow_receivers.size());

                              //determine which receivers are really hit
      {
         const PI3D_visual *recs = &pc.shadow_receivers.front();
         for(i=pc.shadow_receivers.size(); i--; ){
            PI3D_visual vis = *recs++;
            if(!(vis->vis_flags&VISF_BOUNDS_VALID))
               vis->ComputeBounds();
            const I3D_bsphere &bs = vis->bound.GetBoundSphereTrans(vis, FRMFLAGS_BSPHERE_TRANS_VALID);
            bool clip;
            bool in = SphereInVF(vf, bs, clip);
            if(!in)
               continue;
            if(clip){
                                 //detailed bbox test
               S_vector vis_countours[6];
               dword num_vis_cpts;
               ComputeContour(vis->bound.bound_local.bbox, vis->GetMatrix(), ldir, vis_countours, num_vis_cpts, true);
               assert(num_vis_cpts);
                                 //check only with contour clip planes of vf (last 2 are front and back planes)
               //in = CheckFrustumIntersection(vf.clip_planes, vf.num_clip_planes - 2, vis_countours, num_vis_cpts);
               in = CheckFrustumIntersection(vf.clip_planes, vf.num_clip_planes - 2, vf.frustum_pts, vis_countours, num_vis_cpts, vf.view_pos, true);
               if(!in)
                  continue;
            }
            //hit_receivers.push_back(vis);
            hit_receivers[num_receivers++] = vis;
         }
      }
                              //if there're no objects hit, skip this caster
      if(!num_receivers)
         continue;

                              //project bounding box onto the plane
      S_vector projected_contour[6];
      for(i=num_cpts; i--; )
         pl.Intersection(contour_points[i], ldir, projected_contour[i]);

                           //find smallest rectangle
      I3D_bbox best_bbox;
      S_matrix best_mat;

      GetBestShadowMatrix(pl.normal, projected_contour, contour_points, num_cpts, best_mat, best_bbox);

      float mod_scale = mod_sc->GetMatrix()(0).Magnitude();
      float bb_diag = (bb.max - bb.min).Magnitude() * mod_scale;
      shd_camera->SetRange(0.0f, bb_diag);
      shd_camera->SetOrthogonal(true);

                              //compute scale
      S_vector bb_size = best_bbox.max - best_bbox.min;
      float fside = Max(bb_size.x, bb_size.y); 
      fside *= 1.0f + 3.0f / drv->rt_shadow.rt[1]->SizeX1();
      float ortho_scale = 2.0f / fside;
      shd_camera->SetOrthoScale(ortho_scale);

                              //re-compute texture matrix
                              // note: Y is swapped due to texture adressing from upper-left corner
      S_matrix m_txt; m_txt.Identity();
      m_txt(0) = best_mat(0) * fside;
      m_txt(1) = best_mat(1) * (-fside);
      m_txt(2) = pl.normal;
                              //position just in front of bbox from light's view dir
      m_txt(3) = bb_center + ldir * (pl.d - d_bb_min);


                              //create shadow affect cylinder
      I3D_cylinder vc;
      vc.pos = m_txt(3);
      vc.dir = m_txt(2) * SHADOW_LENGTH;
      vc.radius = 0.0f;
      for(i=num_cpts; i--; )
         vc.radius = Max(vc.radius, contour_points[i].DistanceToLine(vc.pos, vc.dir));

      if(mod_sc->frm_flags&FRMFLAGS_SHD_CASTER_NONVIS){
                              //caster wasn't rendered, do detailed check if shadow will really be visible
         I3D_bsphere bsphere;
         bsphere.pos = vc.pos + vc.dir*.5f;
         float d = vc.dir.Magnitude()*.5f;
         float r = vc.radius;
         bsphere.radius = I3DSqrt(d*d + r*r);

         PI3D_frame sct = mod_sc;
         while((sct=sct->GetParent1(), sct) && sct->GetType1()!=FRAME_SECTOR);
         assert(sct);

         bool clip_vf;
         bool shd_visible = I3DCAST_SECTOR(sct)->IsBoundSphereVisible(bsphere,
            current_camera->GetCurrSector(), pc, &clip_vf, NULL);
         if(!shd_visible)
            continue;
      }
      ++scene_stats.dyn_shd_casters;

      S_matrix m_txt_inv = ~m_txt;
                              //add half-texel to correct rounding errors
      float half_texel = .5f / (float)drv->rt_shadow.rt[1]->SizeX1();

      m_txt_inv(3, 0) += .5f + half_texel;
      m_txt_inv(3, 1) += .5f + half_texel;
                              //Z's origin close to bbox center, but slightly towards the light,
                              // so that shadow is not cut too much, but is cut on objects in light direction
      m_txt_inv(3, 2) -= bb_diag * .35f;
      const float r_sl = 1.0f / SHADOW_LENGTH;
      for(i=4; i--; ){
         m_txt_inv(i, 2) *= r_sl;
      }
#if 0
      {
                              //debug!
         for(i=0; i<num_cpts; i++){
            drv->DEBUG(projected_contour[i] * m_txt_inv);
            drv->DEBUG(contour_points[i] * m_txt_inv);
         }
      }
#endif
      drv->EnableFog(false);

      if(drv->GetFlags()&DRVF_DEBUGDRAWSHADOWS){
         SetRenderMatrix(I3DGetIdentityMatrix());
                              //render bbox contour
         DebugDrawContour(contour_points, num_cpts, 0xffff0000);

         S_vector v[4] = { m_txt(3) - m_txt(0)*.5f - m_txt(1)*.5f, v[0] + m_txt(0), v[1] + m_txt(1), v[0] + m_txt(1) };
         DebugDrawContour(v, 4, 0xff00ff00);
                              //volume
         for(int i=num_cpts; i--; ){
            S_vector v = projected_contour[i] + ldir * (pl.d - d_bb_min);
            DrawLine(v, v + ldir * (SHADOW_LENGTH + (d_bb_min - pl.d)), 0x8000ffff);
         }
                              //axes
         DrawLine(v[2] - m_txt(1)*.5f, v[2] - m_txt(1)*.5f + m_txt(0)*.2f, 0xffff0000);
         DrawLine(v[2] - m_txt(0)*.5f, v[2] - m_txt(0)*.5f + m_txt(1)*.2f, 0xff00ff00);
         DrawLine((v[2] + v[0]) * .5f, (v[2] + v[0]) * .5f + m_txt(2)*.2f, 0xff0000ff);
      }
      HRESULT hr;
                              //prepare view and preprocess shadow caster
      shd_camera->SetPos(m_txt(3));
      shd_camera->SetDir1(m_txt(2), -m_txt(1));
      shd_camera->UpdateMatrices();
      shd_camera->UpdateCameraMatrices(1.0f);
      shd_camera->SetCurrSector(sct);
      SetMViewProj(shd_camera);
                              //update matrices
      hr = d3d_dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&shd_camera->GetProjectionMatrix1());
      CHECK_D3D_RESULT("SetTransform", hr);

      C_smart_ptr<I3D_camera> save_curr_cam = current_camera;
      SetActiveCamera(shd_camera);

      bool was_zb = (drv->GetFlags()&DRVF_USEZB);
      if(was_zb) drv->SetState(RS_USEZB, false);
      ((PI3D_texture)(PI3D_texture_base)drv->rt_shadow)->Manage(I3D_texture::MANAGE_CREATE_ONLY);

                              //disassociate textures, so that we do not accidentally using render targets if set
      drv->SetTexture1(0, NULL);
      drv->SetTexture1(1, NULL);

      drv->SetRenderTarget(drv->rt_shadow);

      drv->UpdateViewport(I3D_rectangle(0, 0, drv->rt_shadow->SizeX1(), drv->rt_shadow->SizeY1()));
      hr = d3d_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
      CHECK_D3D_RESULT("Clear", hr);
      drv->UpdateViewport(I3D_rectangle(1, 1, drv->rt_shadow->SizeX1() - 1, drv->rt_shadow->SizeY1() - 1));

      S_preprocess_context &pc_shd = pc_shadows;
      pc_shd.Prepare(pc.viewer_pos);
      pc_shd.render_flags = pc.render_flags;
      pc_shd.curr_sector = sct;
      pc_shd.LOD_factor = pc.LOD_factor * 2.0f;
      pc_shd.mode = RV_SHADOW_CASTER;
      current_camera->PrepareViewFrustum(pc_shd.view_frustum, pc_shd.vf_sphere, 1.0f);
      PreprocessChildren(mod_sc, pc_shd, 0);

                              //render into the texture
      pc_shd.Sort();
#if 0
      {                       //render shadow viewport in shadow texture
         SetRenderMatrix(I3D_identity_matrix);

         S_vector v[4] = { m_txt(3) - m_txt(0)*.5f - m_txt(1)*.5f, v[0] + m_txt(0), v[1] + m_txt(1), v[0] + m_txt(1) };
         DebugDrawContour(v, 4, S_vector(0, 1, 0));
                              //render bbox contour
         DebugDrawContour(contour_points, num_cpts, S_vector(1, 0, 0));
      }
#endif
      pc_shd.shadow_opacity = opacity;
      drv->SetupAlphaTest(false);

      if(drv->CanUsePixelShader() && 1){
         int shd_tp_index = 0;

         drv->SetupBlend(I3DBLEND_OPAQUE);
         const S_vectorw col(opacity, opacity, opacity, 1.0f);
         drv->SetPSConstant(PSC_COLOR, &col);

         DrawList(pc_shd);

#if 1
         if(dist_to_cam < drv->shadow_range_n*.5f){
                              //blur the texture
            bool was_dt = (drv->GetFlags()&DRVF_DITHER);
            if(was_dt) drv->SetState(RS_DITHER, false);

            bool was_lf = (drv->GetFlags()&DRVF_LINFILTER);
            //if(was_lf) drv->SetState(RS_LINEARFILTER, false);
            if(!was_lf) drv->SetState(RS_LINEARFILTER, true);
            //drv->rt_shadow.rt[1]->Manage(I3D_texture::MANAGE_CREATE_ONLY);
            ((PI3D_texture)(PI3D_texture_base)drv->rt_shadow.rt[1])->Manage(I3D_texture::MANAGE_CREATE_ONLY);

            HRESULT hr;
            drv->SetRenderTarget(drv->rt_shadow, 1);

            drv->UpdateViewport(I3D_rectangle(0, 0, drv->rt_shadow.rt[1]->SizeX1(), drv->rt_shadow.rt[1]->SizeY1()));
            hr = d3d_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            CHECK_D3D_RESULT("Clear", hr);
            drv->UpdateViewport(I3D_rectangle(1, 1, drv->rt_shadow.rt[1]->SizeX1() - 1, drv->rt_shadow.rt[1]->SizeY1() - 1));

            drv->SetStreamSource(drv->vb_blur, sizeof(I3D_driver::S_blur_vertex));
            drv->SetIndices(drv->ib_blur);
            drv->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

            drv->SetupBlend(I3DBLEND_ADD);

            I3D_driver::S_ps_shader_entry_in se_ps;
            se_ps.Tex(0);
            se_ps.AddFragment(PSF_SHADOW_RECEIVE);
            drv->SetTexture1(0, drv->rt_shadow);
            drv->SetPixelShader(se_ps);

            if(drv->GetFlags()&DRVF_WIREFRAME)
               d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

            //drv->SetClipping(true);
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
               I3D_driver::NUM_SHADOW_BLUR_SAMPLES * 4,
               0, I3D_driver::NUM_SHADOW_BLUR_SAMPLES * 2);
            CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
            drv->SetState(RS_LINEARFILTER, was_lf);

            if(was_dt) drv->SetState(RS_DITHER, true);
            if(drv->GetFlags()&DRVF_WIREFRAME)
               d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

            shd_tp_index = 1;
         }
#endif
                                 //restore rt back
         drv->SetRenderTarget(save_rt);

         if(was_zb) drv->SetState(RS_USEZB, true);
         drv->UpdateScreenViewport(viewport);

         SetActiveCamera(save_curr_cam);
         SetMViewProj(NULL, true);
         hr = d3d_dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&current_camera->GetProjectionMatrix1());
         CHECK_D3D_RESULT("SetTransform", hr);

         I3D_driver::S_ps_shader_entry_in se_ps;
                                 //use multi-texturing for shadow fading
                                 // stage 1 contains 1-D clamped texture
         if(!(drv->GetFlags()&DRVF_DEBUGDRAWSHADOWS)){
            se_ps.Tex(0);
            drv->SetTexture1(0, drv->rt_shadow.rt[shd_tp_index]);
            se_ps.AddFragment(PSF_t0_COPY);
            se_ps.AddFragment(PSF_r0_B_2_A);

            drv->SetupBlend(I3DBLEND_INVMODULATE);
            //drv->SetupBlend(I3DBLEND_ALPHABLEND);
                              //use alpha-testing for rejecting transparent pixels
            drv->SetupAlphaTest(true);
            drv->SetAlphaRef(0x08);
#if 1
                              //enable blending with stage 1 (clip to positive direction only)
            se_ps.Tex(1);
            drv->SetTexture1(1, drv->tp_shadow);
            se_ps.AddFragment(PSF_MOD_r0_t1);
#endif
            drv->DisableTextures(2);
         }else{
            //drv->SetupBlend(I3DBLEND_ALPHABLEND);
            drv->SetupBlend(I3DBLEND_OPAQUE);

            se_ps.AddFragment(PSF_COLOR_COPY);
            static const S_vectorw col(0.0f, 1.0f, 0.0f, .5f);
            drv->SetPSConstant(PSC_COLOR, &col);
            drv->DisableTextures(0);
         }
         if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
            se_ps.TexKill(2);
         drv->SetPixelShader(se_ps);

         for(i=0; i<2; i++){
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
         }
                              //set fog color to zero (since we're just fading out by fog power)
                              // (help for I3D_visual::RenderSolidMesh)
         drv->SetFogColor(0);

         drv->EnableZWrite(false);
                                 //re-render all receivers using this shadow
         for(int ri=num_receivers; ri--; ){
            PI3D_visual vis = hit_receivers[ri];
                                 //compute texture transformation matrix
            S_matrix tm = vis->GetMatrix() * m_txt_inv;
            tm.Transpose();
                                 //re-use last row for light direction, in local coords
            tm(3) = ldir % vis->GetInvMatrix1();
            drv->SetVSConstant(VSC_MAT_TRANSFORM_1, &tm, 4);

            //PR_BEG; //for(int i=100; i--; )
            vis->RenderSolidMesh(this, false, &vc);
            //PR_END;
         }
         drv->EnableZWrite(true);
         drv->SetupAlphaTest(false);

         for(i=0; i<2; i++){
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
         }

         if(drv->GetFlags()&DRVF_DEBUGDRAWSHADOWS){
                                 //render shadow texture
            drv->EnableFog(false);
            drv->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
            drv->SetTexture1(0, drv->rt_shadow.rt[shd_tp_index]);
            const int NUM_X = 8, NUM_Y = 6;
            float sx = (float)drv->GetGraphInterface()->Scrn_sx() / NUM_X;
            float sy = (float)drv->GetGraphInterface()->Scrn_sy() / NUM_Y;
            int x = debug_show_pos % NUM_X;
            int y = debug_show_pos / NUM_X;
            struct S_vertex{
               float x, y, z, w;
               float u, v;
            } v[4] = {
               x*sx,       y*sy,       0, 1,    0, 0,
               x*sx+sx-1,  y*sy,       0, 1,    1, 0,
               x*sx,       y*sy+sy-1,  0, 1,    0, 1,
               x*sx+sx-1,  y*sy+sy-1,  0, 1,    1, 1,
            };
            drv->SetupBlend(I3DBLEND_OPAQUE);

            I3D_driver::S_ps_shader_entry_in se_ps;
            se_ps.Tex(0);
            se_ps.AddFragment(PSF_t0_COPY_inv);
            //se_ps.AddFragment(PSF_t0_COPY);
            drv->SetPixelShader(se_ps);

            bool use_zb = (drv->GetFlags()&DRVF_USEZB);
            if(use_zb) drv->SetState(RS_USEZB, false);
            bool is_wire = (drv->GetFlags()&DRVF_WIREFRAME);
            if(is_wire) drv->SetState(RS_WIREFRAME, false);

            hr = d3d_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(S_vertex));
            CHECK_D3D_RESULT("DrawPrimitiveUP", hr);

            if(use_zb) drv->SetState(RS_USEZB, true);
            if(is_wire) drv->SetState(RS_WIREFRAME, true);
            drv->ResetStreamSource();
            ++debug_show_pos;

            {
               S_matrix m;
               m.SetDir(vc.dir, 0.0f);
               swap(m(2), m(1));
               m(3) = vc.pos;
               DebugDrawCylinder(m, vc.radius, vc.radius, SHADOW_LENGTH, 0x800000ff);
               SetRenderMatrix(I3DGetIdentityMatrix());
               DrawLine(vc.pos, vc.pos+vc.dir, 0xffff0000);
            }
         }

//----------------------------
      }else{
//----------------------------

         int shd_tp_index = 0;

         drv->DisableTextureStage(1);
         drv->SetupBlend(I3DBLEND_OPAQUE);
         drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
         dword a = FloatToInt(opacity*255.0f);
         drv->SetTextureFactor(0xff000000 | (a<<16) | (a<<8) | a);

         drv->SetTexture(NULL);
         DrawList(pc_shd);

#if 1
         if(dist_to_cam < drv->shadow_range_n*.5f){
                              //blur the texture
            bool was_dt = (drv->GetFlags()&DRVF_DITHER);
            if(was_dt) drv->SetState(RS_DITHER, false);

            bool was_lf = (drv->GetFlags()&DRVF_LINFILTER);
            //if(was_lf) drv->SetState(RS_LINEARFILTER, false);
            if(!was_lf) drv->SetState(RS_LINEARFILTER, true);
            ((PI3D_texture)(PI3D_texture_base)drv->rt_shadow.rt[1])->Manage(I3D_texture::MANAGE_CREATE_ONLY);

            HRESULT hr;
            drv->SetRenderTarget(drv->rt_shadow, 1);

            drv->UpdateViewport(I3D_rectangle(0, 0, drv->rt_shadow.rt[1]->SizeX1(), drv->rt_shadow.rt[1]->SizeY1()));
            hr = d3d_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            CHECK_D3D_RESULT("Clear", hr);
            drv->UpdateViewport(I3D_rectangle(1, 1, drv->rt_shadow.rt[1]->SizeX1() - 1, drv->rt_shadow.rt[1]->SizeY1() - 1));

            drv->SetTexture1(0, drv->rt_shadow);

            drv->SetStreamSource(drv->vb_blur, sizeof(I3D_driver::S_blur_vertex));
            drv->SetIndices(drv->ib_blur);
            drv->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
            drv->SetupTextureStage(1, D3DTOP_MODULATE);

            drv->SetupBlend(I3DBLEND_ADDALPHA);

            d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

            if(drv->GetFlags()&DRVF_WIREFRAME)
               d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

            //drv->SetClipping(true);
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
               I3D_driver::NUM_SHADOW_BLUR_SAMPLES * 4,
               0, I3D_driver::NUM_SHADOW_BLUR_SAMPLES * 2);
            CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
            drv->SetState(RS_LINEARFILTER, was_lf);

            if(was_dt) drv->SetState(RS_DITHER, true);
            if(drv->GetFlags()&DRVF_WIREFRAME)
               d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

            shd_tp_index = 1;
         }else{
            d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
         }
#else
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
#endif
                                 //restore rt back
         drv->SetRenderTarget(save_rt);

         if(was_zb) drv->SetState(RS_USEZB, true);
         drv->UpdateScreenViewport(viewport);

         SetActiveCamera(save_curr_cam);
         SetMViewProj(NULL, true);
         hr = d3d_dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&current_camera->GetProjectionMatrix1());
         CHECK_D3D_RESULT("SetTransform", hr);

                                 //use multi-texturing for shadow fading
                                 // stage 1 contains 1-D clamped texture
         drv->SetTexture1(0, drv->rt_shadow.rt[shd_tp_index]);
         if(!(drv->GetFlags()&DRVF_DEBUGDRAWSHADOWS)){
            drv->SetupBlend(I3DBLEND_INVMODULATE);

            //drv->SetupAlphaTest(true);
            //drv->SetAlphaRef(0xc0);
#if 1
                                 //enable blending with stage 1 (clip to positive direction only)
            drv->SetTexture1(1, drv->tp_shadow);
            drv->SetupTextureStage(1, D3DTOP_MODULATE);
            drv->DisableTextureStage(2);
#else
            drv->DisableTextureStage(1);
#endif
         }else{
            drv->SetupBlend(I3DBLEND_ALPHABLEND);
            drv->SetupTextureStage(0, D3DTOP_MODULATE);
            d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
            d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
            drv->SetTextureFactor(0x8000ff00);
            drv->DisableTextureStage(1);
         }
         for(i=0; i<2; i++){
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
         }
                              //set fog color to zero (since we're just fading out by fog power)
                              // (help for I3D_visual::RenderSolidMesh)
         drv->SetFogColor(0);

         drv->EnableZWrite(false);
                                 //re-render all receivers using this shadow
         for(int ri=num_receivers; ri--; ){
            PI3D_visual vis = hit_receivers[ri];
                                 //compute texture transformation matrix
            S_matrix tm = vis->GetMatrix() * m_txt_inv;
            tm.Transpose();
                                 //re-use last row for light direction, in local coords
            tm(3) = ldir.RotateByMatrix(vis->GetInvMatrix1());
            drv->SetVSConstant(VSC_MAT_TRANSFORM_1, &tm, 4);

            //PR_BEG; //for(int i=100; i--; )
            vis->RenderSolidMesh(this, false, &vc);
            //PR_END;
         }
         drv->EnableZWrite(true);
         //drv->SetupAlphaTest(false);

         for(i=0; i<2; i++){
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            CHECK_D3D_RESULT("SetSamplerState", hr);
         }

         if(drv->GetFlags()&DRVF_DEBUGDRAWSHADOWS){
            d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

                                 //render shadow texture
            drv->DisableTextureStage(1);
            drv->EnableFog(false);
            drv->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
            const int NUM_X = 8, NUM_Y = 6;
            float sx = (float)drv->GetGraphInterface()->Scrn_sx() / NUM_X;
            float sy = (float)drv->GetGraphInterface()->Scrn_sy() / NUM_Y;
            int x = debug_show_pos % NUM_X;
            int y = debug_show_pos / NUM_X;
            struct S_vertex{
               float x, y, z, w;
               float u, v;
            } v[4] = {
               x*sx,       y*sy,       0, 1,    0, 0,
               x*sx+sx-1,  y*sy,       0, 1,    1, 0,
               x*sx,       y*sy+sy-1,  0, 1,    0, 1,
               x*sx+sx-1,  y*sy+sy-1,  0, 1,    1, 1,
            };
            drv->SetupBlend(I3DBLEND_OPAQUE);
            drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
            d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_COMPLEMENT);

            bool use_zb = (drv->GetFlags()&DRVF_USEZB);
            if(use_zb) drv->SetState(RS_USEZB, false);
            bool is_wire = (drv->GetFlags()&DRVF_WIREFRAME);
            if(is_wire) drv->SetState(RS_WIREFRAME, false);

            hr = d3d_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(S_vertex));
            CHECK_D3D_RESULT("DrawPrimitiveUP", hr);

            if(use_zb) drv->SetState(RS_USEZB, true);
            if(is_wire) drv->SetState(RS_WIREFRAME, true);
            drv->ResetStreamSource();
            ++debug_show_pos;

            {
               S_matrix m;
               m.SetDir(vc.dir, 0.0f);
               swap(m(2), m(1));
               m(3) = vc.pos;
               DebugDrawCylinder(m, vc.radius, vc.radius, SHADOW_LENGTH, 0x800000ff);
               SetRenderMatrix(I3DGetIdentityMatrix());
               DrawLine(vc.pos, vc.pos+vc.dir, 0xffff0000);
            }
            d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
         }
      }
   }
                              //re-set current cam, so that z-bias is not set
   SetMViewProj(NULL);
}

//----------------------------

void I3D_scene::RenderMirrors(const S_preprocess_context &pc){

   if(!pc.mirror_data.size())
      return;

   PROFILE(drv, PROF_MIRROR);

   HRESULT hr;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   drv->SetStencilRef(1);
   drv->SetStencilFail(D3DSTENCILOP_KEEP);
   drv->SetStencilZFail(D3DSTENCILOP_KEEP);

   bool is_wire = (drv->GetFlags()&DRVF_WIREFRAME);
   bool zw_enabled = drv->IsZWriteEnabled();

                              //process all mirror groups
   t_mirror_data::const_iterator it;
   for(it = pc.mirror_data.begin(); it!=pc.mirror_data.end(); it++){
      const t_mirror_visuals &mv = (*it).second;

                              //setup stencil rendering
      hr = d3d_dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&current_camera->GetProjectionMatrix1());
      CHECK_D3D_RESULT("SetTransform", hr);
      if(is_wire) d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

                              //setup D3D to clear color-buffer to background color
                              // (for mirror rendering),
                              // as well as write to stencil buffer
      drv->StencilEnable(true);
      drv->SetStencilFunc(D3DCMP_ALWAYS);
      drv->SetStencilPass(D3DSTENCILOP_REPLACE);

      dword clear_color = pc.scene->GetBgndClearColor();
#ifdef DEBUG_MIRROR_SPECIAL_BACKGROUND
      clear_color = 0xffff0000;
#endif
      if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
         clear_color = 0xffff0000;

      if(drv->CanUsePixelShader()){
         I3D_driver::S_ps_shader_entry_in se_ps;
         se_ps.AddFragment(PSF_COLOR_COPY);
         drv->SetPixelShader(se_ps);

         S_vectorw c4(((clear_color>>16)&255)*R_255, ((clear_color>>8)&255)*R_255, ((clear_color>>0)&255)*R_255, (float)(clear_color>>24)*R_255);
         drv->SetPSConstant(PSC_COLOR, &c4);
      }else{
         drv->SetTexture1(0, NULL);
         drv->DisableTextureStage(1);

         drv->SetTextureFactor(clear_color);
         drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
      }
      drv->SetupBlend(I3DBLEND_OPAQUE);

      drv->EnableFog(false);
      drv->EnableNoCull(false);
                              //render bits into stencil buffer,
                              // no z-writes
      if(zw_enabled) drv->EnableZWrite(false);

      S_plane world_plane;
                              //render area of all mirror visuals into stencil buffer
                              // also clear area to special color
      for(int i=mv.size(); i--; ){
         const S_mirror_data &md = *mv[i];
         PI3D_visual vis = md.rp.vis;

         I3D_driver::S_vs_shader_entry_in se;
         se.AddFragment(VSF_TRANSFORM);
         se.AddFragment(VSF_TEXT_PROJECT);

         I3D_driver::S_vs_shader_entry *se1 = drv->GetVSHandle(se);
         drv->SetVertexShader(se1->vs);

         //drv->SetClipping(md.rp.flags&RP_FLAG_CLIP);

         PI3D_mesh_base mb = vis->GetMesh();

         SetRenderMatrix(vis->matrix);

         IDirect3DVertexBuffer9 *vb_src = mb->vertex_buffer.GetD3DVertexBuffer();
         drv->SetStreamSource(vb_src, mb->vertex_buffer.GetSizeOfVertex());

         const dword fgi = md.rp.user1;
         int curr_auto_lod = vis->last_auto_lod;
         const I3D_face_group *fg;
         if(curr_auto_lod < 0){
            fg = &mb->GetFGroupVector()[fgi];
         }else{
            fg = &mb->GetAutoLODs()[curr_auto_lod].fgroups[fgi];
         }

         IDirect3DIndexBuffer9 *ib;
         dword vertex_count;
         dword ib_base;

#ifdef USE_STRIPS
         const S_fgroup_strip_info *si = NULL;
#endif
         if(curr_auto_lod < 0){
                                 //direct mesh
            ib = mb->GetIndexBuffer().GetD3DIndexBuffer();
            vertex_count = mb->vertex_buffer.NumVertices();
            ib_base = mb->GetIndexBuffer().D3D_index_buffer_index;
#ifdef USE_STRIPS
            if(mb->HasStrips())
               si = &mb->GetStripInfo()[fgi];
#endif
         }else{
                                 //automatic LOD
            const C_auto_lod &al = mb->GetAutoLODs()[curr_auto_lod];
            ib = al.GetIndexBuffer().GetD3DIndexBuffer();
            vertex_count = al.vertex_count;
            ib_base = al.GetIndexBuffer().D3D_index_buffer_index + fg->base_index;
#ifdef USE_STRIPS
            if(al.HasStrips())
               si = &al.GetStripInfo()[fgi];
#endif
         }

         drv->SetIndices(ib);
         if(vis->vertex_buffer.vs_decl){
            drv->SetVSDecl(vis->vertex_buffer.vs_decl);
         }else{
            assert(0);
            //drv->SetFVF(mb->vertex_buffer.GetFVFlags());
         }

#ifdef USE_STRIPS
         if(si)
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, mb->vertex_buffer.D3D_vertex_buffer_index, 0,
               vertex_count, ib_base*3 + si->base_index, si->num_indicies-2);
         else
#endif
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mb->vertex_buffer.D3D_vertex_buffer_index, 0,
               vertex_count, (ib_base + fg->base_index) * 3, fg->num_faces);
         CHECK_D3D_RESULT("DrawIP", hr);

                              //compute mirror plane from 1st triangle
         if(!i){
            const void *verts = mb->vertex_buffer.LockConst();
            dword vstride = mb->vertex_buffer.GetSizeOfVertex();
            const I3D_triface *faces;
            hr = ib->Lock(0, 0, (void**)&faces, D3DLOCK_READONLY);
            CHECK_D3D_RESULT("Lock", hr);
            I3D_triface fc;
#ifdef USE_STRIPS
            if(si){
               C_destripifier destrip;
               destrip.Begin(&faces[ib_base + fg->base_index][0]);
               do{
                  destrip.Next();
               }while(destrip.IsDegenerate());
               fc = destrip.GetFace();
            }else
#endif
               fc = faces[ib_base + fg->base_index];
            const S_vector &v0 = *(S_vector*)(((byte*)verts) + fc[0] * vstride);
            const S_vector &v1 = *(S_vector*)(((byte*)verts) + fc[1] * vstride);
            const S_vector &v2 = *(S_vector*)(((byte*)verts) + fc[2] * vstride);

            world_plane.ComputePlane(v0, v1, v2);

            ib->Unlock();
            mb->vertex_buffer.UnlockConst();

            world_plane = world_plane * vis->matrix;
         }
      }
      if(zw_enabled) drv->EnableZWrite(true);

                              //setup stencil-testing
      drv->SetStencilFunc(D3DCMP_EQUAL);
      drv->SetStencilPass(D3DSTENCILOP_KEEP);

                              //clear the z-buffer in mirrored area -
                              // draw pixels only into stenciled area
                              // with zero z and w set to far clipping plane
      {
         d3d_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

         const I3D_rectangle &viewport = pc.scene->GetViewport1();
         float rhw = 1.0f / pc.scene->GetActiveCamera1()->GetFCP();

         S_vectorw *p_dst;
         hr = drv->vb_clear->Lock(0, sizeof(S_vectorw)*4, (void**)&p_dst, 0);
         if(SUCCEEDED(hr)){
            for(dword i=0; i<4; i++){
               p_dst[i].w = rhw;
               p_dst[i].z = .999f;
               p_dst[i].x = (float)viewport[(i&1) << 1];
               p_dst[i].y = (float)viewport[(i&2) + 1];
            }
            drv->vb_clear->Unlock();
         }
         drv->SetStreamSource(drv->vb_clear, sizeof(S_vectorw));
         drv->SetFVF(D3DFVF_XYZRHW);

         hr = d3d_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
         CHECK_D3D_RESULT("DrawPrimitive", hr);
         d3d_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
      }
      if(!drv->CanUsePixelShader()){
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
      }

      drv->StencilEnable(false);
      if(is_wire) d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

      {
                              //render mirrored image now
                              //setup camera to look from behind the mirror
         PI3D_camera cam = I3DCAST_CAMERA(CreateFrame(FRAME_CAMERA));
         C_smart_ptr<I3D_camera> cam_save = current_camera;
         cam->SetFOV(cam_save->GetFOV1());
         cam->SetRange(cam_save->GetNCP(), cam_save->GetFCP());
         current_camera = cam;
         cam->SetPos(cam_save->GetWorldPos());
         cam->SetRot(cam_save->GetWorldRot());
         cam->UpdateMatrices();
         cam->Mirror(world_plane);
         //cam->SetCurrSector(cam_save->GetCurrSector());

         drv->SetDefaultCullMode(D3DCULL_CW);

         drv->StencilEnable(true);
         drv->SetStencilFunc(D3DCMP_EQUAL);

         S_plane pl = -world_plane;
         S_preprocess_context pc_rec(this);
         RenderView(pc.render_flags, RV_MIRROR, &pl, 1, &pc_rec);

         drv->SetDefaultCullMode(D3DCULL_CCW);
         current_camera = cam_save;

         pc.scene->SetMViewProj(cam_save);
         cam->Release();
                              //setup stencil to clear to zero again
         //drv->SetStencilFunc(D3DCMP_EQUAL);
         drv->SetStencilPass(D3DSTENCILOP_DECR);
      }

      if(drv->GetFlags2()&DRVF2_DRAWTEXTURES){
                              //finally, render all visuals in their native mode
                              // (but with z-buffer always on)
                              // this also clears stencil buffer for next mirror
         for(i=mv.size(); i--; ){
            const S_mirror_data &md = *mv[i];
            PI3D_visual vis = md.rp.vis;

            PI3D_light lf = md.rp.sector->GetFogLight();
            if(lf) drv->SetFogColor(lf->dw_color);

            if(drv->CanUsePixelShader()){
               vis->DrawPrimitivePS(pc, md.rp);
            }else
               vis->DrawPrimitive(pc, md.rp);
         }
      }
      drv->StencilEnable(false);
   }
}
#endif
//----------------------------

struct S_dd_context{
   I3D_FRAME_TYPE ft;
   PI3D_scene scene;
   bool sector_visible;
   dword enum_mask;
   const C_vector<PI3D_occluder> *occ_list;
   const S_view_frustum *vf;
   dword render_time;
   dword drv_flags, drv_flags2;
   //dword debug_draw_mat;
   const I3D_driver::t_col_mat_info *col_mat_info;
   S_preprocess_context *pc;
};


//----------------------------

I3DENUMRET I3DAPI I3D_scene::cbDebugDraw(PI3D_frame frm, dword c){

   S_dd_context *ddc = (S_dd_context*)c;
   switch(frm->GetType1()){

   case FRAME_MODEL:
      {
         if(!frm->IsOn1())
            return I3DENUMRET_SKIPCHILDREN;
         PI3D_model mod = I3DCAST_MODEL(frm);
         if((ddc->drv_flags&DRVF_DRAWHRBBOX) && (frm->num_hr_vis)){
            if(!(frm->GetFlags()&FRMFLAGS_HR_BOUND_VALID))
               mod->hr_bound.MakeHierarchyBounds(frm);
            mod->hr_bound.GetBoundSphereTrans(frm, FRMFLAGS_HR_BSPHERE_VALID);
            mod->hr_bound.DebugDrawBounds(ddc->scene, frm, FRMFLAGS_HR_BSPHERE_VALID);
         }
      }
      break;

   case FRAME_VISUAL:
      {
         PI3D_visual vis = I3DCAST_VISUAL(frm);

         if(!frm->IsOn1())
            return I3DENUMRET_SKIPCHILDREN;

         if((vis->GetDriver1()->GetFlags()&DRVF_DRAWVISUALS) && vis->last_render_time != ddc->render_time)
            break;

         if((ddc->drv_flags2&DRVF2_DRAWJOINTS) && (ddc->drv_flags2&DRVF2_DRAWVOLUMES) &&
            vis->GetVisualType1()==I3D_VISUAL_SINGLEMESH){
            ((I3D_object_singlemesh*)vis)->DebugDrawVolBox(ddc->scene);
         }

         if(ddc->drv_flags&(DRVF_DRAWBBOX | DRVF_DRAWHRBBOX)){
            if(ddc->drv_flags&DRVF_DRAWBBOX){
               if(!(vis->vis_flags&VISF_BOUNDS_VALID)) vis->ComputeBounds();
               vis->bound.DebugDrawBounds(ddc->scene, frm, FRMFLAGS_BSPHERE_TRANS_VALID);
            }
            if((ddc->drv_flags&DRVF_DRAWHRBBOX) && frm->num_hr_vis){
               if(!(frm->frm_flags&FRMFLAGS_HR_BOUND_VALID))
                  vis->hr_bound.MakeHierarchyBounds(frm);
               vis->hr_bound.DebugDrawBounds(ddc->scene, frm, FRMFLAGS_HR_BSPHERE_VALID);
            }
         }
         if(
#ifndef GL
            ((ddc->drv_flags&DRVF_DEBUGDRAWSHDRECS) && (frm->frm_flags&I3D_FRMF_SHADOW_RECEIVE)) ||
#endif
             ((ddc->drv_flags2&DRVF2_DEBUGDRAWSTATIC) && (frm->frm_flags&I3D_FRMF_STATIC_COLLISION)) ||
             //(ddc->debug_draw_mat==vis->GetCollisionMaterial() && (vis->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION))
             (ddc->col_mat_info && (vis->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION))
             )
         {
            bool b_shadow = 
#ifndef GL
               ((ddc->drv_flags&DRVF_DEBUGDRAWSHDRECS) && (frm->frm_flags&I3D_FRMF_SHADOW_RECEIVE));
#else
               false;
#endif
            bool b_static = ((ddc->drv_flags2&DRVF2_DEBUGDRAWSTATIC) && (frm->frm_flags&I3D_FRMF_STATIC_COLLISION));
            //bool b_debug_mat = (ddc->debug_draw_mat==vis->GetCollisionMaterial() && (vis->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION));
            bool b_debug_mat = (ddc->col_mat_info && (vis->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION));
            dword texture_factor;

                              //debug_mat override static(becouse every visual with material IS static)
            if(b_shadow&&b_debug_mat)
               texture_factor = 0xc070a070;
            else
            if(b_shadow&&b_static)
               texture_factor = 0xc080f000;
            else
            if(b_shadow)
               texture_factor = 0xc0008000;
            else
            if(b_debug_mat){
               //texture_factor = 0xc0800080;
               texture_factor = 0xc0000000;  //default color
               I3D_driver::t_col_mat_info::const_iterator it = ddc->col_mat_info->find(vis->GetCollisionMaterial());
               if(it!=ddc->col_mat_info->end())
                  texture_factor = it->second.color;
            }else
               texture_factor = 0xc0808000;

                              //render shadow receiver
            PI3D_driver drv = ddc->scene->GetDriver1();
#ifndef GL
            IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
            if(!drv->CanUsePixelShader()){
               drv->SetTexture1(0, NULL);
               drv->DisableTextureStage(1);
               //drv->SetupTextureStage(0, D3DTOP_MODULATE);
               drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
               d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

               d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
               d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
               drv->SetTextureFactor(texture_factor);
            }else
#endif
            {
               I3D_driver::S_ps_shader_entry_in se_ps;
               se_ps.AddFragment(PSF_MOD_COLOR_v0);
               drv->SetPixelShader(se_ps);

               S_vectorw c4(((texture_factor>>16)&255)*R_255, ((texture_factor>>8)&255)*R_255, ((texture_factor>>0)&255)*R_255, (float)(texture_factor>>24)*R_255);
               drv->SetPSConstant(PSC_COLOR, &c4);
            }

            drv->SetupBlend(I3DBLEND_ALPHABLEND);
            
                              //setup camera vector in visual's local coords
            {
               const S_matrix &m_cam = ddc->scene->GetActiveCamera1()->GetMatrix();
               S_vectorw vw = m_cam(2);
               vw.w = 0.0f;
               vw.Invert();
               vw %= vis->GetInvMatrix();
               drv->SetVSConstant(VSC_MAT_TRANSFORM_1, &vw);
            }

            vis->RenderSolidMesh(ddc->scene, true);
#ifndef GL
            if(!drv->CanUsePixelShader()){
               d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
               d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
               d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            }
#endif
         }
      }
      break;

   case FRAME_LIGHT:
      {
         PI3D_light lp = I3DCAST_LIGHT(frm);
         lp->Draw1(ddc->scene, ddc->vf, false);
      }
      break;
   case FRAME_SOUND:
      {
         PI3D_sound snd = I3DCAST_SOUND(frm);
         snd->Draw1(ddc->scene, ddc->vf, false);
      }
      break;

   case FRAME_CAMERA:
      I3DCAST_CAMERA(frm)->Draw1(ddc->scene, false);
      break;

   case FRAME_OCCLUDER:
      I3DCAST_OCCLUDER(frm)->Draw1(ddc->scene, false);
      break;

   case FRAME_JOINT:
      {
                              //check if parent visual SM was rendered
         for(PI3D_frame prnt = frm; prnt = prnt->GetParent1(), prnt; ){
            if(prnt->GetType()==FRAME_VISUAL){
               PI3D_visual vis = I3DCAST_VISUAL(prnt);
               if(!(vis->GetDriver1()->GetFlags()&DRVF_DRAWVISUALS) || vis->last_render_time == ddc->render_time)
                  I3DCAST_JOINT(frm)->Draw1(ddc->scene, false);
               break;
            }
         }
      }
      break;

   case FRAME_DUMMY:
      if(!frm->IsOn1())
         return I3DENUMRET_SKIPCHILDREN;
      if(ddc->drv_flags2&DRVF2_DRAWDUMMYS)
         I3DCAST_DUMMY(frm)->Draw1(ddc->scene, false);
      break;

   case FRAME_SECTOR:
      {
         PI3D_sector sct = I3DCAST_SECTOR(frm);
         bool save_visible = ddc->sector_visible;
         ddc->sector_visible = sct->IsVisible();
                        //optim: skip drawing children if not visible
         sct->EnumFrames(cbDebugDraw, c, ddc->enum_mask);
         ddc->sector_visible = save_visible;
         return I3DENUMRET_SKIPCHILDREN;
      }
      break;
   case FRAME_VOLUME:
      {
         PI3D_volume vol = I3DCAST_VOLUME(frm);
                              //check if this, or parent frames are visible
         bool on = vol->IsOn1();
         for(PI3D_frame prnt=vol; prnt=prnt->GetParent1(), prnt; ){
            if(!prnt->IsOn1()){
               on = false;
               break;
            }
         }
         if(on && vol->Prepare()){

            bool clip;
                              //view frustum check
            bool out = !SphereInVF(*ddc->vf,
               vol->GetBoundSphere(), clip);
            if(out)
               break;
                              //occlusion test (front to back!)
            for(dword i=0; i<ddc->occ_list->size(); i++){
               PI3D_occluder occ = (*ddc->occ_list)[i];

               bool occluded = occ->IsOccluding(vol->GetBoundSphere(), clip);
               if(occluded)
                  break;
            }
            if(i != ddc->occ_list->size())
               break;

            if(ddc->col_mat_info){
               dword color = 0x40000000;  //default color
               I3D_driver::t_col_mat_info::const_iterator it = ddc->col_mat_info->find(vol->GetCollisionMaterial());
               if(it!=ddc->col_mat_info->end())
                  color = it->second.color;
               vol->Draw1(ddc->scene, false, false, &color);
            }else
            if((ddc->drv_flags2&DRVF2_DRAWVOLUMES) ||
                              //draw dynamic volimes when DRVF_DEBUGDRAWDYNAMIC is set
               ((ddc->drv_flags&DRVF_DEBUGDRAWDYNAMIC) && !(frm->frm_flags&I3D_FRMF_STATIC_COLLISION)) ||
                              //draw static volumes when DRVF2_DEBUGDRAWSTATIC is set
               ((ddc->drv_flags2&DRVF2_DEBUGDRAWSTATIC) && (frm->frm_flags&I3D_FRMF_STATIC_COLLISION)) ){

               vol->Draw1(ddc->scene, false);
            }
         }
      }
      break;
   default:
      frm->DebugDraw(ddc->scene);
   }
   return I3DENUMRET_OK;
}

//----------------------------

void I3D_scene::DebugDrawFrames(I3D_FRAME_TYPE ft, dword enum_mask, const S_view_frustum &vf, const C_vector<PI3D_occluder> &occ_list){

   PI3D_driver drv = GetDriver1();
   //const D3DCAPS9 *caps = drv->GetCaps();

   bool zw_en = drv->IsZWriteEnabled();
   if(zw_en) drv->EnableZWrite(false);
                              //ugly re-use backdrop's context
   S_preprocess_context &pc = pc_backdrop;
   pc.Prepare(current_camera->GetMatrixDirect()(3));

   S_dd_context ddc;

   ddc.pc = &pc;
   ddc.ft = ft;
   ddc.scene = this;
   ddc.sector_visible = GetPrimarySector1()->IsVisible();
   ddc.enum_mask = enum_mask;
   ddc.occ_list = &occ_list;
   ddc.vf = &vf;
   ddc.render_time = drv->GetRenderTime();
   ddc.drv_flags = drv->GetFlags();
   ddc.drv_flags2 = drv->GetFlags2();
   //ddc.debug_draw_mat = drv->debug_draw_mat;
   ddc.col_mat_info = drv->debug_draw_mats ? &drv->GetColMatInfo() : NULL;

   EnumFrames(cbDebugDraw, (dword)&ddc, enum_mask);

   if(zw_en) drv->EnableZWrite(true);
}

//----------------------------

void I3D_scene::RenderView(dword render_flags, E_RENDERVIEW_MODE rv_mode,
#ifndef GL
   const S_plane *add_clip_planes, dword num_add_clip_planes,
#endif
   S_preprocess_context *use_pc){

   HRESULT hr;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   current_camera->UpdateCameraMatrices(aspect_ratio);

   S_preprocess_context &pc = use_pc ? *use_pc : pc_main;
   pc.Prepare(current_camera->GetMatrixDirect()(3));
   pc.render_flags = render_flags;
   pc.curr_sector = NULL;
   pc.mode = rv_mode;

   drv->UpdateScreenViewport(viewport);

   if(!current_camera->PrepareViewFrustum(pc.view_frustum, pc.vf_sphere, GetInvAspectRatio()))
      return;
#ifndef GL
   switch(rv_mode){
   case RV_NORMAL:
   case RV_SHADOW_CASTER:
   case RV_CAM_TO_TEXTURE:
      assert(!num_add_clip_planes);
      break;
   case RV_MIRROR:
      assert(num_add_clip_planes==1);
                              //replace front clipping plane by 
      //pc.view_frustum.clip_planes[pc.view_frustum.num_clip_planes++] = add_clip_planes[0];
      pc.view_frustum.clip_planes[VF_FRONT] = add_clip_planes[0];
      break;
   }
#endif
   /*
   for(dword i=0; i<num_add_clip_planes; i++){
      pc.view_frustum.clip_planes[pc.view_frustum.num_clip_planes++] = add_clip_planes[i];
   }
   */
                              //camera sector visible also without portal test
   //if(rv_mode!=RV_MIRROR)
   SetupCameraSector();
   SetMViewProj(NULL);
   current_camera->GetCurrSector()->SetVisible(pc.view_frustum);

   bool render_backdrop = false;
   if(primary_sector->IsVisible()){
      render_backdrop = (backdrop_sector->IsOn1() && (drv->drv_flags&DRVF_CLEAR) && (backdrop_sector->NumChildren1()!=0));
   }
   
                              //clear background/depth/stencil
   if(rv_mode!=RV_MIRROR){
      PROFILE(drv, PROF_CLEAR);
                              //setup projection matrix now (W-buffer clearing needs it?)
      d3d_dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&current_camera->GetProjectionMatrix1());
      dword clear_flags = 0;
      /*                      //Bug fix: always clear, because sector may be clipped by far clipping plane
      bool do_clear = false;
      if(drv->GetFlags()&DRVF_CLEAR){
         do_clear = (primary_sector->IsVisible() ||
                                 //clear background if wireframe on
            (drv->drv_flags&DRVF_WIREFRAME) ||
                                 //clear background if visuals not drawn
            !(drv->drv_flags&DRVF_DRAWVISUALS));
      }
      */
      bool do_clear = (drv->GetFlags()&DRVF_CLEAR);

      if(do_clear)
         clear_flags |= D3DCLEAR_TARGET;
      if(drv->drv_flags&DRVF_USEZB)
         clear_flags |= D3DCLEAR_ZBUFFER;
                              //clear also stencil buffer, if there's one
      if((drv->GetFlags()&DRVF_HASSTENCIL))
         clear_flags |= D3DCLEAR_STENCIL;
      if(clear_flags){
         hr = d3d_dev->Clear(0, NULL, clear_flags, dw_color_bgnd, 1.0f, 0);
         CHECK_D3D_RESULT("Clear", hr);
      }
#ifdef GL
      clear_flags = 0;
      if(do_clear)
         clear_flags |= GL_COLOR_BUFFER_BIT;
      if(drv->drv_flags&DRVF_USEZB)
         clear_flags |= GL_DEPTH_BUFFER_BIT;
      if((drv->GetFlags()&DRVF_HASSTENCIL))
         clear_flags |= GL_STENCIL_BUFFER_BIT;
      glClear(clear_flags);
      CHECK_GL_RESULT("Clear");
#endif
   }

   const C_vector<C_smart_ptr<I3D_frame> > &frames = cont.GetFrameVector();

   if(render_backdrop){
      PROFILE(drv, PROF_CLEAR);
                              //draw backdrop image
      drv->EnableZBUsage(false);
      PI3D_camera bd_cam = backdrop_camera;
      bd_cam->SetFOV(current_camera->GetFOV1());
      bd_cam->UpdateCameraMatrices(aspect_ratio);
      d3d_dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&bd_cam->GetProjectionMatrix1());
      {                       //update view matrix - rotation part only
         S_matrix &mdst = *(S_matrix*)&bd_cam->GetViewMatrix1();
         const S_matrix &msrc = current_camera->GetViewMatrix1();
         mdst(0) = msrc(0);
         mdst(1) = msrc(1);
         mdst(2) = msrc(2);
      }
      {
         S_matrix &mdst = *(S_matrix*)&bd_cam->matrix;
         const S_matrix &msrc = current_camera->matrix;
         mdst(0) = msrc(0);
         mdst(1) = msrc(1);
         mdst(2) = msrc(2);
      }
      SetMViewProj(bd_cam);
      C_smart_ptr<I3D_camera> save_curr_cam = current_camera;
      SetActiveCamera(bd_cam);

      S_preprocess_context &bd_pc = pc_backdrop;
      bd_pc.Prepare(S_vector(0, 0, 0));
      bd_pc.render_flags = render_flags;
      bd_pc.curr_sector = backdrop_sector;
      bd_pc.Reserve(frames.size() * 4);
      current_camera->PrepareViewFrustum(bd_pc.view_frustum, bd_pc.vf_sphere, GetInvAspectRatio());

                              //transform all backdrop objects
      dword prep_flags = 0;

                              //update all lights
      if(backdrop_sector->frm_flags&FRMFLAGS_HR_LIGHT_RESET){
         backdrop_sector->UpdateAllLights();
         prep_flags |= E_PREP_SET_HR_LIGHT_DIRTY;
      }

      assert(backdrop_sector->NumChildren1());
      if(backdrop_sector->num_hr_vis)
         PreprocessChildren(backdrop_sector, bd_pc, prep_flags);

                              //sort face groups by pivot distance from [0,0,0]
      bd_pc.SortByDistance();
      DrawList(bd_pc);
                              //put everything back
      SetActiveCamera(save_curr_cam);
      drv->EnableZBUsage(true);
   }

   SetMViewProj(NULL);

#ifndef DEBUG_NO_CLIP_PLANES
   if(num_add_clip_planes){
      assert(num_add_clip_planes==1);
      drv->SetClippingPlane(add_clip_planes[0], m_view_proj_hom);
   }
   //drv->SetClippingPlane(*(S_plane*)drv->debug_float, m_view_proj_hom);
#endif
#if defined _DEBUG && 0
   drv->SetClippingPlane(S_plane(S_vector(-1, 0, 0), .5f), S_matrix());
   num_add_clip_planes = 1;
#endif

                              //update matrices
   const S_matrix &m_proj = current_camera->GetProjectionMatrix1();
   d3d_dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&m_proj);
   

   C_vector<PI3D_occluder> occ_list;
   if(drv->drv_flags&DRVF_DRAWVISUALS){

      dword prep_flags = 0;
                              //alloc enough members for storing
                              // pre-processed info
      pc.Reserve(frames.size() * 4);

      if(primary_sector->IsVisible()){
         pc.curr_sector = primary_sector;
         ++scene_stats.frame_count[FRAME_SECTOR];
      }else{
         prep_flags |= E_PREP_NO_VISUALS;
      }

      if(primary_sector->frm_flags&FRMFLAGS_HR_LIGHT_RESET){
         primary_sector->UpdateAllLights();
         prep_flags |= E_PREP_SET_HR_LIGHT_DIRTY;
      }
#ifndef GL
      pc.shadow_receivers.reserve(proposed_num_shd_receivers * 2);
#endif
      Preprocess(pc, prep_flags | E_PREP_ADD_SHD_CASTERS);
      current_camera->GetCurrSector()->ResetRecursive();
#ifndef GL
      proposed_num_shd_receivers = pc.shadow_receivers.size();
      RenderMirrors(pc);
#endif
      pc.Sort();
      DrawList(pc);
   }else{
      current_camera->GetCurrSector()->ResetRecursive();
   }

   drv->EnableNoCull(false);

   SetMViewProj(NULL, true);

   if((drv->GetFlags2() & (DRVF2_DRAWLIGHTS | DRVF2_DRAWSOUNDS | DRVF2_DRAWCAMERAS|
      DRVF2_DRAWDUMMYS | DRVF2_DRAWOCCLUDERS | DRVF2_DRAWJOINTS | DRVF2_DRAWVOLUMES |
      DRVF2_DEBUGDRAWSTATIC)) ||
      (drv->GetFlags() & (DRVF_DRAWHRBBOX | DRVF_DRAWBBOX | 
#ifndef GL
      DRVF_DEBUGDRAWSHDRECS |
#endif
      DRVF_DEBUGDRAWDYNAMIC)) ||
      drv->debug_draw_mats){

      dword draw_flags = ENUMF_SECTOR | ENUMF_DUMMY | ENUMF_VISUAL | ENUMF_MODEL;
      dword f = drv->GetFlags();
      dword f2 = drv->GetFlags2();
      if(f2&DRVF2_DRAWLIGHTS) draw_flags |= ENUMF_LIGHT;
      if(f2&DRVF2_DRAWSOUNDS) draw_flags |= ENUMF_SOUND;
      if(f2&DRVF2_DRAWCAMERAS) draw_flags |= ENUMF_CAMERA;
      //if(f2&DRVF2_DRAWDUMMYS) draw_flags |= ENUMF_DUMMY;
      if(f2&DRVF2_DRAWOCCLUDERS) draw_flags |= ENUMF_OCCLUDER;
      if(f2&DRVF2_DRAWJOINTS) draw_flags |= ENUMF_JOINT | ENUMF_VISUAL;
      if(f2&DRVF2_DEBUGDRAWSTATIC) draw_flags |= ENUMF_VISUAL;
      if((f2&(DRVF2_DRAWVOLUMES|DRVF2_DEBUGDRAWSTATIC)) || (f&DRVF_DEBUGDRAWDYNAMIC)) draw_flags |= ENUMF_VOLUME;
      if(f&DRVF_DRAWBBOX) draw_flags |= ENUMF_VISUAL;
      //if(f&DRVF_DRAWHRBBOX) draw_flags |= ENUMF_VISUAL | ENUMF_MODEL;
#ifndef GL
      if(f&DRVF_DEBUGDRAWSHDRECS) draw_flags |= ENUMF_VISUAL;
#endif
      if(drv->debug_draw_mats)
         draw_flags |= ENUMF_VISUAL | ENUMF_VOLUME;
      //if(draw_flags&ENUMF_VOLUME)
         //BuildVolTree();
      if(draw_flags&ENUMF_VOLUME)
         BuildDynamicVolTree();

      DebugDrawFrames(FRAME_NULL, draw_flags, pc.view_frustum, occ_list);
   }

   if(drv->GetFlags()&DRVF_DEBUGDRAWBSP)
      bsp_tree.DebugDraw(pc, bsp_draw_help);

   DebugDrawDynCols();

   if(drv->drv_flags&(DRVF_DRAWSECTORS | DRVF_DRAWPORTALS)){
      for(int i=sectors.size(); i--; ){
         PI3D_sector sct = sectors[i];
         sct->Draw1(this, drv->drv_flags&DRVF_DRAWSECTORS, drv->drv_flags&DRVF_DRAWPORTALS);
      }
   }

#ifndef DEBUG_NO_CLIP_PLANES
   if(num_add_clip_planes)
      drv->DisableClippingPlane();
#endif//DEBUG_NO_CLIP_PLANES
}

//----------------------------

I3D_RESULT I3D_scene::Render(dword flags){

   //drv->SetNightVision(drv->debug_int[0]);

   if(!current_camera){
      DEBUG_LOG("I3D_scene::Render: no active camera, cannot render");
      return I3DERR_INVALIDPARAMS;
   }

                              //save due to sound update
   PI3D_sector old_cam_sector = current_camera->GetCurrSector();

   I3D_RESULT ir = drv->BeginScene();
   if(I3D_FAIL(ir)){
      DEBUG_LOG("I3D_scene::Render: BeginScene failed");
      return ir;
   }

   {
   PROFILE(drv, PROF_RENDER);

   int render_time_delta = drv->render_time - last_render_time;
   assert((render_time_delta >= 0) && (render_time_delta < 1000*60*60*72));
   last_render_time = drv->render_time;

#ifndef GL
   if(!drv->CanUsePixelShader())
      drv->DisableTextureStage(1);

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   if(drv->GetFlags2()&DRVF2_DEBUG_SHOW_OVERDRAW){
                              //set-up overdraw rendering (using stencil buffer)

      drv->StencilEnable(true);
      drv->SetStencilFunc(D3DCMP_ALWAYS);

      drv->SetStencilPass(D3DSTENCILOP_INCRSAT);
      drv->SetStencilFail(D3DSTENCILOP_KEEP);
      drv->SetStencilZFail(D3DSTENCILOP_INCRSAT);
   }
#endif
                              //render the screen using current camera
   //drv->BeginNightVisionRender();
   RenderView(flags, RV_NORMAL);
   //drv->EndNightVisionRender();
#ifndef GL
   if(drv->GetFlags2()&DRVF2_DEBUG_SHOW_OVERDRAW){

      d3d_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);

      bool was_zb = (drv->GetFlags()&DRVF_USEZB);
      if(was_zb) drv->SetState(RS_USEZB, false);
      bool is_wire = (drv->GetFlags()&DRVF_WIREFRAME);
      if(is_wire) drv->SetState(RS_WIREFRAME, false);

      if(drv->CanUsePixelShader())
         drv->DisableTextures(0);
      else{
         drv->DisableTextureStage(1);
         drv->SetTexture1(0, NULL);
      }

      S_vectorw vertices[4];
      S_vectorw bar[4];
      for(dword i=0; i<4; i++){
         S_vectorw &v = vertices[i];
         v.x = (float)viewport[(i&1)*2];
         v.y = (float)viewport[(i&2)+1];
         v.z = 0.0f;
         v.w = 1.0f;
         {
            S_vectorw &v = bar[i];
            v.x = viewport.l + (viewport.r-viewport.l) * (!(i&1) ? .01f : .04f);
            v.z = 0.0f;
            v.w = 1.0f;
         }
      }
      static const word indx[] = {0, 1, 2, 2, 1, 3};

      static const dword colors[] = {
         //0x0000ff, 0x808080, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xff0000
         0x004000, 0x008000, 0x00c000, 0x00ff00, 0x40ff00, 0x80c000, 0xc08000, 0xff4000, 0xff0000
         //0x00ff00, 0xff00ff, 0x0000ff, 0x00ffff, 0xffff00, 0x808080, 0x008080, 0xff0000,
      };
      const dword MAX_SHOW = sizeof(colors)/sizeof(dword);

      drv->SetStencilPass(D3DSTENCILOP_KEEP);
      drv->SetStencilFail(D3DSTENCILOP_KEEP);
      drv->SetStencilZFail(D3DSTENCILOP_KEEP);

      drv->SetStencilFunc(D3DCMP_EQUAL);
      for(i=1; i<=MAX_SHOW; i++){
         drv->SetStencilRef(i);
         if(i == MAX_SHOW)
            drv->SetStencilFunc(D3DCMP_LESSEQUAL);
                              //draw quad over the screen
         DrawTriangles(vertices, 4, I3DVC_XYZRHW, indx, 6, 0xff000000 | colors[i-1]);
      }
      drv->StencilEnable(false);
                              //render bars (on top of all that)
      for(i=1; i<=MAX_SHOW; i++){
         bar[0].y = bar[1].y = viewport.t + viewport.b * ((float)i*.01f);
         bar[2].y = bar[3].y = viewport.t + viewport.b * ((float)i*.01f + .004f);
         DrawTriangles(bar, 4, I3DVC_XYZRHW, indx, 6, 0xff000000 | colors[i-1]);
      }

      if(was_zb) drv->SetState(RS_USEZB, true);
      if(is_wire) drv->SetState(RS_WIREFRAME, true);
   }
#endif
                              //re-compute sounds
   if(drv->GetSoundInterface()){
      //if(primary_sector->num_hr_snd)
         //PreprocessSounds(primary_sector, render_time_delta);
      PreprocessSounds(render_time_delta);
   }
   if(flags&I3DRENDER_UPDATELISTENER)
      UpdateSoundProperties(render_time_delta, old_cam_sector);

   if(drv->GetFlags()&DRVF_DRAWLMTEXTURES)
      drv->DrawLMaps(viewport);

#if defined _DEBUG && 0
   {
                              //save
      C_render_target<> save_rt = drv->GetCurrRenderTarget();
      C_smart_ptr<I3D_camera> scam = GetActiveCamera1();
      SetActiveCamera(rt_env.cam);
      bool is_aspect;
      const I3D_rectangle save_vp = GetViewport1(&is_aspect);
      SetViewport(I3D_rectangle(0, 0, rt_env->SizeX(), rt_env->SizeY()), true);
                              //render
      for(int i=0; i<6; i++){
         C_cube_render_target::E_SIDE side = (C_cube_render_target::E_SIDE)i;
         rt_env.SetupRenderTarget(drv, side);
         rt_env.SetupCamera(S_vector(0, 0, 0), side);
         RenderView(flags, RV_NORMAL);
      }

                              //restore
      SetViewport(save_vp, is_aspect);
      drv->SetRenderTarget(save_rt);
      SetActiveCamera(scam);
      SetMViewProj(NULL);

#if 1
      for(int j=0; j<6; j++){
         drv->SetTexture(rt_env.rt[j]);
         struct S_vertex{
            S_vectorw v;
            dword diffuse;
            I3D_text_coor tx;
         };
         S_vertex vx[4];
         for(int i=0; i<4; i++){
            S_vertex &v = vx[i];
            v.v.w = 1.0f;
            v.v.z = 0.0f;
            v.v.x = 1;
            v.v.y = 1;
            switch(j){
            case 0: v.v.x -= 1; break;
            case 1: v.v.x += 1; break;
            case 2: v.v.y += 1; break;
            case 3: v.v.y -= 1; break;
            case 4: v.v.x += 2; break;
            }
            v.v.x *= 100;
            v.v.y *= 100;
            v.tx.u = 0;
            v.tx.v = 0;
            //v.diffuse = 255<<(i*8);
            //v.diffuse |= 0x80000000;
            v.diffuse = 0xffffffff;
            if(i&1){
               v.v.x += 100;
               v.tx.u = 1;
            }
            if(i&2){
               v.v.y += 100;
               v.tx.v = 1;
            }
         }
         static const word indx[] = {0, 1, 2, 2, 1, 3};
         //drv->SetTexture1(0, NULL);
         DrawTriangles(vx, 4, I3DVC_XYZRHW | I3DVC_DIFFUSE | (1<<I3DVC_TEXCOUNT_SHIFT), indx, 6, 0xffffffff);
      }
#endif

   }
#endif

#if !defined NDEBUG || 0

   float f = PR_GET;
   if(f) drv->DEBUG(C_fstr("%.3f", f));
   if(debug_count[0] || debug_count[1] || debug_count[2] || debug_count[3]){
      for(int i=0; i<4; i++){
         drv->DEBUG(C_fstr("[%i]: %i", i, debug_count[i]));
         debug_count[i] = 0;
      }
   }
#endif

   }
   drv->EndScene();
   return I3D_OK;
}

//----------------------------

