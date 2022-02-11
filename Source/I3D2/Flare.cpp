/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.
   All rights reserved.

   File: billbrd.cpp
   Content: I3D_object_billboard - billboard frame, frame
      inherited from I3D_object, making billboard effect
      by 'looking' always towards active camera.
--------------------------------------------------------*/

#include "all.h"
#include "scene.h"
#include "camera.h"
#include "visual.h"
#include "mesh.h"

//#define DEBUG_NO_COLLISIONS   //don't test collisions

//----------------------------

#define DBASE_VERSION 0x0001

#define CHECK_COL_TIME 400


//----------------------------
//----------------------------

class I3D_flare: public I3D_visual{

#define FLRF_COLLIDING        1        //currently some occlusion between flare and camera

   dword flare_flags;

                              //parameters:
   C_smart_ptr<I3D_material> mat;
   float scale_begin_dist;    //distance when scaling and fading starts
   float rotation_ratio;      //ratio of rotation, multiplied by distance and 2*PI gives rotation angle
   float fade_normal_angle;   //angle, when normal-based rotation fades out
   int occlusion_fade_time;   //fade time used for occlusion fading, if zero, occlusion tests are disabled

   int last_check_col_time;   //last time we checked collisions
   int occlusion_fade_count;  //set to 0 for invisible (occluded) flare, set to -1 for fully visible (not occluded) flare
   float pivot;               //pivot displacement along the view Z axis

                              
   struct S_user_context{     //struct passed between calls to AddPrimitives, DrawPrimitive
      S_matrix g_matrix;
      S_vectorw diffuse;
   };

public:
   I3D_flare(PI3D_driver d):
      I3D_visual(d),
      scale_begin_dist(5.0f),
      rotation_ratio(.02f),
      fade_normal_angle(0.0f),
      occlusion_fade_time(250),
      last_check_col_time(drv->GetRenderTime() - CHECK_COL_TIME - S_int_random(CHECK_COL_TIME)),
      occlusion_fade_count(-1),
      pivot(0.0f)
   {
      visual_type = I3D_VISUAL_FLARE;
   }

public:
                              //specialized methods of I3D_visual:

#define FLARE_BSPHERE_RADIUS .70711f

   virtual bool ComputeBounds(){

      bound.bound_local.bbox.min = S_vector(-FLARE_BSPHERE_RADIUS*.5f, -FLARE_BSPHERE_RADIUS*.5f, -FLARE_BSPHERE_RADIUS*.5f);
      bound.bound_local.bbox.max = S_vector( FLARE_BSPHERE_RADIUS*.5f,  FLARE_BSPHERE_RADIUS*.5f,  FLARE_BSPHERE_RADIUS*.5f);
      bound.bound_local.bsphere.pos.Zero();
      bound.bound_local.bsphere.radius = FLARE_BSPHERE_RADIUS*.5f;

      //return I3D_visual::ComputeBounds();
      vis_flags |= VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
      return true;
   }

//----------------------------

   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
//----------------------------

   I3DMETHOD(Duplicate)(CPI3D_frame frm){

      if(frm==this)
         return I3D_OK;
      if(frm->GetType1()!=FRAME_VISUAL)
         return I3D_frame::Duplicate(frm);
      CPI3D_visual vis = I3DCAST_CVISUAL(frm);
      switch(vis->GetVisualType1()){
      case I3D_VISUAL_FLARE:
         {
            I3D_flare *flr = (I3D_flare*)vis;
            SetMaterial(flr->GetMaterial());
            scale_begin_dist = flr->scale_begin_dist;
            rotation_ratio = flr->rotation_ratio;
            fade_normal_angle = flr->fade_normal_angle;
            occlusion_fade_time = flr->occlusion_fade_time;
            pivot = flr->pivot;
         }
         break;
      default:
         SetMeshInternal(const_cast<PI3D_visual>(vis)->GetMesh());
      }
      return I3D_visual::Duplicate(vis);
   }

//----------------------------

   void SetMeshInternal(PI3D_mesh_base mesh){

      if(mesh){
         SetMaterial(mesh->GetFGroups1()->mat);
      }else{
         PI3D_material mat = drv->CreateMaterial();
         SetMaterial(mat);
         mat->Release();
      }
   }

//----------------------------

   bool SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const{

      word version = DBASE_VERSION;
      ck->write(&version, sizeof(word));
      for(int i=mats.size(); i--; ){
         if(mats[i]==mat)
            break;
      }
      assert(i!=-1);
      if(i==-1)
         return false;
      ck->write(&i, sizeof(word));
      return true;
   }

//----------------------------

   virtual bool LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){
      {
         //word v = 0;
         //ck->read((char*)&v, sizeof(word));
         word v = ck->ReadWord();
         if(v!=DBASE_VERSION)
            return false;
      }
      //word mat_id = 0;
      //ck->read((char*)&mat_id, sizeof(word));
      word mat_id = ck->ReadWord();
      mat = mats[mat_id];
      return true;
   }

//----------------------------

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(SetProperty)(dword index, dword value){

      switch(index){
      case I3DPROP_FLRE_F_SCALE_DIST: scale_begin_dist = I3DIntAsFloat(value); break;
      case I3DPROP_FLRE_F_ROTATE_RATIO: rotation_ratio = I3DIntAsFloat(value); break;
      case I3DPROP_FLRE_F_NORMAL_ANGLE: fade_normal_angle = I3DIntAsFloat(value); break;
      case I3DPROP_FLRE_I_FADE_TIME: occlusion_fade_time = value; break;
      case I3DPROP_FLRE_F_PIVOT: pivot = I3DIntAsFloat(value); break;
      default:
         assert(0);
         return I3DERR_INVALIDPARAMS;
      }
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(dword,GetProperty)(dword index) const{
      switch(index){
      case I3DPROP_FLRE_F_SCALE_DIST: return I3DFloatAsInt(scale_begin_dist);
      case I3DPROP_FLRE_F_ROTATE_RATIO: return I3DFloatAsInt(rotation_ratio);
      case I3DPROP_FLRE_F_NORMAL_ANGLE: return I3DFloatAsInt(fade_normal_angle);
      case I3DPROP_FLRE_I_FADE_TIME: return occlusion_fade_time;
      case I3DPROP_FLRE_F_PIVOT: return I3DFloatAsInt(pivot);
      }
      assert(0);
      return 0;
   }

   I3DMETHOD_(I3D_RESULT,SetMaterial)(PI3D_material mat1){
      mat = mat1;
      if(!mat)
         SetOn(false);
      return I3D_OK;
   }
   I3DMETHOD_(PI3D_material,GetMaterial()){ return mat; }
   I3DMETHOD_(CPI3D_material,GetMaterial()) const{ return mat; }
};

typedef I3D_flare *PI3D_flare;
typedef const I3D_flare *CPI3D_flare;

//----------------------------

void I3D_flare::AddPrimitives(S_preprocess_context &pc){

   if(pc.mode!=RV_NORMAL)
      return;

   if(!mat)
      return;
   if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
      return;

   PI3D_camera cam = pc.scene->GetActiveCamera1();
   const S_matrix &m_camera = cam->GetMatrix();

   S_vector dir_to_cam = m_camera(3) - matrix(3);
                              //check distance to camera
   float dist_to_cam = dir_to_cam.Magnitude();
   float dist_to_near = -m_camera(2).Dot(dir_to_cam) - cam->GetNCP();

   if(dist_to_near <= pivot)
      return;

   float mat_alpha = mat->GetAlpha();

   if((dword&)fade_normal_angle){
                              //check angle to direction normal
      float angle = (m_camera(3) - matrix(3)).AngleTo(matrix(2));
      if(angle >= fade_normal_angle){
         occlusion_fade_count = 0;
         return;
      }
      mat_alpha *= 1.0f - angle / fade_normal_angle;
   }

#ifndef DEBUG_NO_COLLISIONS
   if(occlusion_fade_time){
                              //check collisions now
      int cct_delta = drv->GetRenderTime() - last_check_col_time;
      if(cct_delta > CHECK_COL_TIME){
         last_check_col_time = drv->GetRenderTime();

         struct S_hlp{
            static bool I3DAPI cbResp2(I3D_cresp_data &rd){
                              //don't detect with specially marked collisions
               assert(rd.GetHitFrm());
               return !(rd.GetHitFrm()->GetFrameFlags()&I3D_FRMF_NO_FLARE_COLLISION);
            }
         };

         I3D_collision_data cd;
         cd.from = matrix(3);
         cd.dir = dir_to_cam;
         cd.flags = I3DCOL_LINE | I3DCOL_FORCE2SIDE;
         cd.callback = S_hlp::cbResp2;
         bool col = pc.scene->TestCollision(cd);        
         if(col){
            flare_flags |= FLRF_COLLIDING;
            if(occlusion_fade_count == -1)
               occlusion_fade_count = occlusion_fade_time;
         }else{
            flare_flags &= ~FLRF_COLLIDING;
            if(!occlusion_fade_count)
               ++occlusion_fade_count;
         }
      }

                              //process occlusion fading
      int render_time_delta = drv->GetRenderTime() - last_render_time;
   
      if(!(flare_flags&FLRF_COLLIDING)){
         if(occlusion_fade_count != -1){
                                 //fade on
            occlusion_fade_count += render_time_delta;
            if(occlusion_fade_count >= occlusion_fade_time)
               occlusion_fade_count = -1;
            else
               mat_alpha *= (float)occlusion_fade_count / (float)occlusion_fade_time;
         }
      }else{
                                 //fade off
         occlusion_fade_count = Max(0, occlusion_fade_count - render_time_delta);
         if(!occlusion_fade_count)
            return;
         mat_alpha *= (float)occlusion_fade_count / (float)occlusion_fade_time;
      }
   }
#endif//!DEBUG_NO_COLLISIONS

                              //add to sort list
   pc.prim_list.push_back(S_render_primitive(0, alpha, this, pc));
   S_render_primitive &p = pc.prim_list.back();
                              //determine blending mode
   //p.blend_mode = I3DBLEND_VERTEXALPHA;
   p.blend_mode = I3DBLEND_ALPHABLEND;
   if(occlusion_fade_time){
      p.sort_value = PRIM_SORT_ALPHA_NOZ;
      ++pc.alpha_noz;
   }else{
      p.sort_value = PRIM_SORT_ALPHA_NOZWRITE;
      ++pc.alpha_nozwrite;
   }

   if(mat->IsAddMode())
      p.blend_mode = I3DBLEND_ADD;
   pc.scene->render_stats.vert_trans += 4;

                              //create user context, to be passed to DrawPrimitive
   S_user_context *uc = new S_user_context;
   p.user1 = (dword)uc;
   uc->g_matrix(0, 3) = 0.0f;
   uc->g_matrix(1, 3) = 0.0f;
   uc->g_matrix(2, 3) = 0.0f;
   uc->g_matrix(3, 3) = 1.0f;
                              //scale alpha by distance
   if((dist_to_near - pivot) < scale_begin_dist){
      mat_alpha *= (dist_to_near - pivot) / scale_begin_dist;
   }
   mat_alpha *= (float)alpha * R_255;

   {                          //compute matrix
      float scale = matrix(0).Magnitude();
      if(dist_to_near < scale_begin_dist){
         scale *= .5f + (.5f * dist_to_near / scale_begin_dist);
      }
      scale *= (dist_to_near - pivot) / dist_to_near;

      const S_matrix *mp = &m_camera;
      if((dword&)rotation_ratio){
                              //apply rotation by Z
         S_matrix m_tmp;
         m_tmp.RotationZ(dist_to_near * rotation_ratio * 2.0f * PI);
         m_tmp = m_tmp % m_camera;
         mp = &m_tmp;
      }

      uc->g_matrix(0) = (*mp)(0) * scale;
      uc->g_matrix(1) = (*mp)(1) * scale;
      uc->g_matrix(2) = (*mp)(2) * scale;
      uc->g_matrix(3) = matrix(3);
      if(!occlusion_fade_time){
                              //apply pivot
         uc->g_matrix(3) += dir_to_cam * (1.0f / dist_to_cam) * pivot;
      }
   }

                              //compute dest color,
                              // which will be assigned during the rendering
   uc->diffuse = mat->GetDiffuse();
   uc->diffuse.w = mat_alpha;
}

//----------------------------
#ifndef GL
void I3D_flare::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   S_user_context *uc = (S_user_context*)rp.user1;
   assert(uc);

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   drv->SetupBlend(rp.blend_mode);
   drv->SetupTextureStage(0, D3DTOP_MODULATE);

   I3D_driver::S_vs_shader_entry_in se;
   se.AddFragment(VSF_LIGHT_BEGIN);
   se.AddFragment(VSF_LIGHT_END);
   se.AddFragment(VSF_MAKE_RECT_UV);
   PrepareVertexShader(NULL, 1, se, rp, pc.mode, VSPREP_TRANSFORM | VSPREP_FEED_MATRIX, &uc->g_matrix);

   drv->DisableTextureStage(1);
   drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
   drv->EnableNoCull(true);   //use no culling due to mirrors


                              //setup flare's color
   HRESULT hr;
   drv->SetVSConstant(VSC_AMBIENT, &uc->diffuse);

   drv->SetStreamSource(drv->vb_rectangle, sizeof(S_vertex_rectangle));
   drv->SetVSDecl(drv->vs_decl_rectangle);

   hr = d3d_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   CHECK_D3D_RESULT("DrawPrimitive", hr);

   delete uc;
}
#endif
//----------------------------

void I3D_flare::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   S_user_context *uc = (S_user_context*)rp.user1;
   assert(uc);

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   I3D_driver::S_vs_shader_entry_in se;
   I3D_driver::S_ps_shader_entry_in se_ps;

   se.AddFragment(VSF_MAKE_RECT_UV);
   PrepareVertexShader(NULL, 1, se, rp, pc.mode, VSPREP_TRANSFORM | VSPREP_FEED_MATRIX, &uc->g_matrix);

   PI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
   if(tb){
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_MOD_t0_CONSTCOLOR);
      drv->SetTexture1(0, tb);
      drv->DisableTextures(1);
   }else{
      se_ps.AddFragment(PSF_COLOR_COPY);
      drv->DisableTextures(0);
   }
#ifndef GL
   if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
      se_ps.TexKill(1);
#endif
   drv->SetPixelShader(se_ps);

   drv->SetupBlend(rp.blend_mode);

   drv->EnableNoCull(true);   //use no culling due to mirrors

                              //setup flare's color
   HRESULT hr;
   drv->SetPSConstant(PSC_COLOR, &uc->diffuse);

   drv->SetStreamSource(drv->vb_rectangle, sizeof(S_vertex_rectangle));
   drv->SetVSDecl(drv->vs_decl_rectangle);

   hr = d3d_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   CHECK_D3D_RESULT("DrawPrimitive", hr);

   delete uc;
}

//----------------------------

extern const S_visual_property props_Flare[] = {
                              //I3DPROP_FLRE_F_SCALE_DIST
   {I3DPROP_FLOAT, "Scale distance", "Distance to camera, from which flare starts to scale down."},
                              //I3DPROP_FLRE_F_ROTATE_RATIO
   {I3DPROP_FLOAT, "Rotate ratio", "Ratio how many times is flare rotated as distance to camera changes, in turns/meter."},
                              //I3DPROP_FLRE_F_NORMAL_ANGLE
   {I3DPROP_FLOAT, "Normal angle", "Angle from local Z axis at which flare is fade out. Set to 0 to disable this fading. Angle is in radians."},
                              //I3DPROP_FLRE_I_FADE_TIME
   {I3DPROP_INT, "Fade time", "Time how long flare fades in/out when occluded. Set to 0 to disable and use Z-buffered rendering."},
                              //I3DPROP_FLRE_F_PIVOT
   {I3DPROP_FLOAT, "Pivot distance", "Pivot displacement used for preventing flare to cut to close objects."},
   {I3DPROP_NULL}
};

I3D_visual *CreateFlare(PI3D_driver drv){
   return new I3D_flare(drv);
}

//----------------------------
//----------------------------


