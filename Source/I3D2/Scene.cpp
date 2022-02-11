/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Scene.cpp
   Content: Scene class.
--------------------------------------------------------*/

#include "all.h"
#include "visual.h"
#include "light.h"
#include "camera.h"
#include "scene.h"
#include "model.h"
#include "volume.h"
#include "dummy.h"
#include "user.h"
#include "joint.h"
#include "occluder.h"
#include <win_res.h>
#include "soundi3d.h"
#include "mesh.h"
#include "sng_mesh.h"
#ifndef GL
#include "stripifier.h"
#endif

//----------------------------

#pragma warning(disable: 4725)//instruction may be inaccurate on some Pentiums

//#define VOLUME_STATISTICS   //not implemented now (writes to parent class) - need to solve (if those stats are needed)

#define DYNAMIC_CELL_SCALE 8.0f  //must be 2^n
#define MIN_DYNAMIC_LEVELS 2  //this is level size equal to (2^n) / DYNAMIC_CELL_SCALE
#define MAX_DYNAMIC_LEVELS 5 // ''


#ifndef NDEBUG
int debug_count[4];
#endif

static const S_vector V_ZERO(.0f, .0f, .0f);

//----------------------------

/*
void S_expanded_view_frustum::Make(const S_view_frustum &vf){

   for(int i=vf.num_clip_planes; i--; ){
      const S_plane &pl = vf.clip_planes[i];
      nx[i] = pl.normal.x;
      ny[i] = pl.normal.y;
      nz[i] = pl.normal.z;
      d[i]  = pl.d;
   }
   num_clip_planes = vf.num_clip_planes;
   int ncp = (num_clip_planes + 3) & -4;
                              //pad last elements with the 1st one
   i = ncp - vf.num_clip_planes;
   const S_plane &pl = vf.clip_planes[0];
   while(i--){
      nx[vf.num_clip_planes + i] = pl.normal.x;
      ny[vf.num_clip_planes + i] = pl.normal.y;
      nz[vf.num_clip_planes + i] = pl.normal.z;
      d[vf.num_clip_planes + i] = pl.d;
   }
}
*/

//----------------------------
//----------------------------
                              //virtual table for container callback to scene
static const S_container_vcall vc_container_scene = {
   (S_container_vcall::t_Open)&I3D_scene::Open,
   (S_container_vcall::t_Close)&I3D_scene::Close,
   (S_container_vcall::t_AddFrame)&I3D_scene::AddFrame,
   (S_container_vcall::t_RemoveFrame)&I3D_scene::RemoveFrame,
};

//----------------------------

//C_render_target<6, I3D_texture> rt_env;

//----------------------------
                              //scene
I3D_scene::I3D_scene(PI3D_driver d):
   ref(1),
   xref(0),
   primary_sector(NULL),
   backdrop_sector(NULL),
   backdrop_camera(NULL),
   ssp_count_down(0),
   last_render_time(0),
   proposed_num_shd_receivers(0),
   drv(d),
   scn_flags(0),
   pc_main(NULL),
   pc_backdrop(NULL),
   pc_shadows(NULL)
{
   drv->AddCount(I3D_CLID_SCENE);
   pc_main.scene = this;
   pc_backdrop.scene = this;
   pc_shadows.scene = this;

   cont.master_interface = (C_unknown*)this;
   cont.vcall = &vc_container_scene;

   primary_sector = new I3D_sector(drv, true);
   primary_sector->SetName("Primary sector");

   backdrop_sector = new I3D_sector(drv, true);
   backdrop_sector->SetName("Backdrop sector");

   backdrop_camera = I3DCAST_CAMERA(CreateFrame(FRAME_CAMERA));
   backdrop_camera->SetName("backdrop camera");
   backdrop_camera->SetCurrSector(backdrop_sector);
   backdrop_camera->SetRange(1.0f, 20.0f);
   backdrop_camera->UpdateCameraMatrices(.75f);

   shd_camera = I3DCAST_CAMERA(CreateFrame(FRAME_CAMERA));
   shd_camera->Release();

#if defined _DEBUG && 0
   rt_env.Init(this, 128, TEXTMAP_NOMIPMAP | TEXTMAP_HINTDYNAMIC | TEXTMAP_RENDERTARGET | TEXTMAP_NO_SYSMEM_COPY);
#endif

                              //reset stats count
   memset(&render_stats, 0, sizeof(render_stats));
   memset(&scene_stats, 0, sizeof(scene_stats));

   //drv->RegResetCallback(
}

//----------------------------

I3D_scene::~I3D_scene(){

   Close();

   if(primary_sector) primary_sector->Release();
   if(backdrop_sector) backdrop_sector->Release();
   if(backdrop_camera) backdrop_camera->Release();

   drv->DecCount(I3D_CLID_SCENE);
   drv = NULL;

   bsp_tree.Close();
   /*
   for(int i=sounds.size(); i--; ){
      assert(sounds[i]->owning_scene==this);
      sounds[i]->owning_scene = NULL;
   }
   */
}

//----------------------------

#if defined _DEBUG && 0
I3D_RESULT I3D_scene::C_cube_render_target::Init(PI3D_scene scn, dword size, dword ct_flags){

   PI3D_driver drv = scn->GetDriver();
   I3D_CREATETEXTURE ct;
   memset(&ct, 0, sizeof(ct));
   ct.flags = ct_flags;// | TEXTMAP_CUBEMAP;
   ct.size_x = size;
   ct.size_y = size;

   I3D_RESULT ir;
   PI3D_texture tp;
   for(int i=0; i<6; i++){
      ir = drv->CreateTexture(&ct, &tp);
      if(I3D_FAIL(ir)){
         Close();
         return ir;
      }
      rt[i] = tp;
      tp->Release();
   }
   IDirect3DSurface9 *_zb;
   ir = drv->CreateDepthBuffer(size, size, rt[0]->GetPixelFormat(), &_zb);
   if(SUCCEEDED(ir)){
      zb = _zb;
      _zb->Release();
      cam = I3DCAST_CAMERA(scn->CreateFrame(FRAME_CAMERA));
      cam->SetFOV(PI*.5f);
      cam->Release();
   }else{
      Close();
   }
   return ir;
}

//----------------------------

void I3D_scene::C_cube_render_target::SetupCamera(const S_vector &pos, E_SIDE side, const S_matrix *m_rot){

   S_vector z, y;
   switch(side){
   case SIDE_LEFT: z = S_vector(-1, 0, 0); y = S_vector(0, 1, 0); break;
   case SIDE_RIGHT: z = S_vector(1, 0, 0); y = S_vector(0, 1, 0); break;
   case SIDE_DOWN: z = S_vector(0, -1, 0); y = S_vector(0, 0, 1); break;
   case SIDE_UP: z = S_vector(0, 1, 0); y = S_vector(0, 0, -1); break;
   case SIDE_FRONT: z = S_vector(0, 0, 1); y = S_vector(0, 1, 0); break;
   case SIDE_BACK: z = S_vector(0, 0, -1); y = S_vector(0, 1, 0); break;
   default: assert(0);
   }
   cam->SetPos(pos);
   if(m_rot){
      z %= *m_rot;
      y %= *m_rot;
   }
   cam->SetDir1(z, y);
}

//----------------------------

void I3D_scene::C_cube_render_target::SetupRenderTarget(PI3D_driver drv, E_SIDE side){

   PI3D_texture tp = rt[side];
   tp->Manage(I3D_texture::MANAGE_CREATE_ONLY);
   /*
   D3DCUBEMAP_FACES cm;
   switch(side){
   case SIDE_LEFT: cm = D3DCUBEMAP_FACE_NEGATIVE_X; break;
   case SIDE_RIGHT: cm = D3DCUBEMAP_FACE_POSITIVE_X; break;
   case SIDE_DOWN: cm = D3DCUBEMAP_FACE_NEGATIVE_Y; break;
   case SIDE_UP: cm = D3DCUBEMAP_FACE_POSITIVE_Y; break;
   case SIDE_BACK: cm = D3DCUBEMAP_FACE_POSITIVE_Z; break;
   case SIDE_FRONT: cm = D3DCUBEMAP_FACE_NEGATIVE_Z; break;
   default: assert(0); cm = D3DCUBEMAP_FACE_POSITIVE_X;
   }
   */
   IDirect3DSurface9 *surf;
   //((IDirect3DCubeTexture8*)tp->GetD3DTexture())->GetCubeMapSurface(cm, 0, &surf);
   ((IDirect3DTexture8*)tp->GetD3DTexture())->GetSurfaceLevel(0, &surf);
   assert(surf);
   drv->SetRenderTarget(C_render_target<>(surf, zb));
   surf->Release();
}

#endif

//----------------------------

bool I3D_scene::Init(){

   SetBgndColor(S_vector(0, 0, 0));
   PIGraph ig = drv->GetGraphInterface();
   SetViewport(I3D_rectangle(0, 0, ig->Scrn_sx(), ig->Scrn_sy()), !(ig->GetFlags()&IG_FULLSCREEN));

                              //init sound listener
   PISND_driver isp = drv->GetSoundInterface();
   if(isp){
      isp->SetListenerRolloffFactor(0.0f);
      {
#if defined USE_EAX && defined _DEBUG && 0
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_ENVIRONMENT,
         //EAX_ENVIRONMENT_BATHROOM);
         EAX_ENVIRONMENT_DRUGGED);
         //EAX_ENVIRONMENT_GENERIC);
         //EAX_ENVIRONMENT_PADDEDCELL);
      //isp->UpdateListener();
#endif
      }
   }
   return true;
}

//----------------------------

I3D_RESULT I3D_scene::SetBgndColor(const S_vector &c){

   color_bgnd = c;
   S_vector v;
   LightUpdateColor(drv, c, 1.0f, &v, &dw_color_bgnd, true);
#ifdef GL
   glClearColor(color_bgnd.x, color_bgnd.y, color_bgnd.z, 0);
   CHECK_GL_RESULT("ClearColor");
#endif
   return I3D_OK;
}

//----------------------------

static const float default_material[] = {
   1.0f, 1.0f, 1.0f, 1.0f,    //diffuse
   1.0f, 1.0f, 1.0f, 1.0f,    //ambient
   0.0f, 0.0f, 0.0f, 1.0f, 0.0f, //specular
   0.0f, 0.0f, 0.0f, 0.0f, 0.0f  //emmisive
};

S_vector I3D_scene::GetLightness(const S_vector &pos, const S_vector *norm, dword light_flags) const{

   const I3D_bsphere bsphere(pos, 0.0f);
   CPI3D_sector sct = const_cast<I3D_scene*>(this)->GetSector(pos);
   if(!sct)
      return S_vector(0, 0, 0);
   S_vector norm_tmp;

   S_vector ret(0, 0, 0);

   const C_vector<PI3D_light> &lights = sct->GetLights1();
   for(int i=lights.size(); i--; ){
      PI3D_light lp = lights[i];
      if(!lp->IsOn1())
         continue;
      if(!(lp->GetLightFlags()&light_flags))
         continue;

      lp->UpdateLight();

      switch(lp->GetLightType1()){
      case I3DLIGHT_AMBIENT:
         ret += lp->color * lp->power;
         break;

      case I3DLIGHT_POINT:
      case I3DLIGHT_POINTAMBIENT:
         {
            const S_vector &lpos = lp->GetMatrix()(3);
            S_vector dir_to_l = lpos - pos;
            float dist = dir_to_l.Magnitude();
            const float &nr = lp->range_n_scaled, &fr = lp->range_f_scaled;
            if(dist>=fr)
               continue;
            float p = lp->power;
            if(norm && lp->GetLightType1()==I3DLIGHT_POINT){
               dir_to_l /= dist;
               float dt = norm->Dot(dir_to_l);
               if(dt <= 0.0f)
                  continue;
               p *= dt;
            }
            if(dist>nr)
               p *= (fr-dist) / (fr-nr);
            ret += lp->color * p;
         }
         break;

      case I3DLIGHT_DIRECTIONAL:
         {
            float p = lp->power;
            if(norm){
               float dt = norm->Dot(-lp->normalized_dir);
               if(dt <= 0.0f)
                  continue;
               p *= dt;
            }
            ret += lp->color * p;
         }
         break;
      case I3DLIGHT_SPOT:
         {
            float p = lp->power;
                              //direction to light
            const S_vector &lpos = lp->GetMatrix()(3);
            S_vector dir_to_l = lpos - pos;
                              //get distanece - fast reject if out of it
            float ldist_2 = dir_to_l.Dot(dir_to_l);
            if(ldist_2>=lp->range_f_scaled_2)
               continue;

            float ldist = I3DSqrt(ldist_2);
                           //compute attenuation
            if(ldist>lp->range_n_scaled) 
               p *= (lp->range_f_scaled - ldist) / lp->range_delta_scaled;
                           //cone
            float r_dist = 1.0f/ldist;
            {
               float a = -lp->normalized_dir.Dot(dir_to_l) * r_dist;
               if(a <= lp->outer_cos)
                  continue;
               if(a<lp->inner_cos)
                  p *= (a - lp->outer_cos) / (lp->inner_cos - lp->outer_cos);
            }
            p *= r_dist;
                              //compute angle, if normal provided
            if(norm){
                              //fast reject out-going rays
               float dt = norm->Dot(dir_to_l);
               if(dt <= 0.0f)
                  continue;
               p *= dt;
            }
            ret += lp->color * p;
         }
         break;
      }
   }
   return ret;
}

//----------------------------

void I3D_scene::SetBackdropRange(float n1, float f1){
   assert(backdrop_camera);
   backdrop_camera->SetRange(n1, f1);
   backdrop_camera->UpdateMatrices();
}

//----------------------------

void I3D_scene::GetBackdropRange(float &n1, float &f1) const{
   assert(backdrop_camera);
   backdrop_camera->GetRange(n1, f1);
}

//----------------------------

void I3D_scene::Close(){

   for(dword i=0; i<cont.frames.size(); i++){
      PI3D_frame frm = cont.frames[i];
      switch(frm->GetType1()){
      case FRAME_MODEL:
         {
            PI3D_model mod = I3DCAST_MODEL(frm);
            if(mod->in_container==&cont){
               if(mod->IsAnimated())
                  cont.RemoveAnimFrame(frm);
               mod->in_container = NULL;
            }
         }
         break;
      case FRAME_SOUND:
         I3DCAST_SOUND(frm)->VirtualPause();
         break;
      }
      frm->LinkTo(NULL);
   }
   cont.frames.clear();

   AttenuateSounds();

   if(current_camera)
      current_camera->SetCurrSector(NULL);
   SetActiveCamera(NULL);
                              //release sectors
   for(i=sectors.size(); i--; ){
      sectors[i]->Disconnect(NULL);
   }
   sectors.clear();
   current_camera = NULL;
   primary_sector->Disconnect(NULL);
   primary_sector->SetEnvironmentID(0);
   primary_sector->SetTemperature(0.0f);
   backdrop_sector->Disconnect(NULL);
   backdrop_sector->SetEnvironmentID(0);
   backdrop_sector->SetTemperature(0.0f);

                              //free lights of sectors
   while(primary_sector->NumLights()){
      I3D_RESULT ir;
      ir = primary_sector->RemoveLight(primary_sector->GetLight(0));
      assert(I3D_SUCCESS(ir));
   }
   while(backdrop_sector->NumLights()){
      I3D_RESULT ir;
      ir = backdrop_sector->RemoveLight(backdrop_sector->GetLight(0));
      assert(I3D_SUCCESS(ir));
   }

                              //release animations
   cont.ClearInterpolators();

   SetBgndColor(S_vector(0, 0, 0));

   bsp_tree.Clear();
   bsp_draw_help.Clear();
   //spheres_space.clear();
}

//----------------------------

PI3D_sector I3D_scene::GetSector(const S_vector &v, PI3D_sector suggest_sector){

   if(suggest_sector){
      if(suggest_sector!=primary_sector){
         if(suggest_sector->CheckPoint(v))
            return suggest_sector;
      }else
         suggest_sector = NULL;
   }

   for(int i=sectors.size(); i--; ){
      PI3D_sector sct = sectors[i];
      if(sct==suggest_sector || sct->IsSimple())
         continue;
      if(sct->CheckPoint(v))
         return sct;
   }
   return primary_sector;
}

//----------------------------

CPI3D_sector I3D_scene::GetSector(const S_vector &v, CPI3D_sector suggest_sector) const{
   return (PI3D_sector)const_cast<PI3D_scene>(this)->GetSector(v, (CPI3D_sector)suggest_sector);
}

//----------------------------

I3D_RESULT I3D_scene::SetFrameSectorPos(PI3D_frame frm, const S_vector &pos){

   PI3D_frame prnt = frm->GetParent1();
                              //in order to change sector, frame must be
                              //already linked to one, or to NULL
   if(prnt)
   switch(prnt->GetType1()){
   case FRAME_SECTOR:
      if(prnt == GetBackdropSector())
         return I3DERR_CANTSETSECTOR;
      break;
   default:
      return I3DERR_CANTSETSECTOR;
   }
   PI3D_frame sct = GetSector(pos);
   {
                              //check if this sector is not our child
      PI3D_frame scr_prnt = sct;
      while(scr_prnt=scr_prnt->GetParent1(), scr_prnt){
         if(scr_prnt==frm){
            sct = primary_sector;
            break;
         }
      }
   }
   if(sct != prnt){
      frm->LinkTo(sct, I3DLINK_UPDMATRIX);
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_scene::SetFrameSector(PI3D_frame frm){

   const S_vector &pos = frm->GetWorldPos();

   return SetFrameSectorPos(frm, pos);
}

//----------------------------

I3D_RESULT I3D_scene::EnumFrames(I3D_ENUMPROC *proc, dword user, dword flags, const char *mask) const{

   I3D_RESULT ir =
      primary_sector->EnumFrames(proc, user, flags, mask);

   if(SUCCEEDED(ir))
   if(!(flags&ENUMF_NO_BACKDROP))
      ir = backdrop_sector->EnumFrames(proc, user, flags, mask);
   return ir;
}

//----------------------------

/*
I3D_RESULT I3D_scene::EnumFramesEx(I3D_enum &en, dword flags, const char *mask) const{

   I3D_RESULT ir = primary_sector->EnumFramesEx(en, flags, mask);

   if(SUCCEEDED(ir))
   if(!(flags&ENUMF_NO_BACKDROP))
      ir = backdrop_sector->EnumFramesEx(en, flags, mask);
   return ir;
}
*/

//----------------------------

static I3DENUMRET I3DAPI cbFindFrm(PI3D_frame frm, dword user){

   (*(PI3D_frame*)user)=frm;
   return I3DENUMRET_CANCEL;
}

//----------------------------

PI3D_frame I3D_scene::FindFrame(const char *name, dword f) const{

   PI3D_frame frm = NULL;
   EnumFrames(cbFindFrm, (dword)&frm, f, name);
   if(!frm){
      if(name){
         if(f&ENUMF_WILDMASK){
            if(primary_sector->GetName1().Match(name)) frm = primary_sector;
            else
            if(!(f&ENUMF_NO_BACKDROP)){
               if(backdrop_sector->GetName1().Match(name)) 
                  frm = backdrop_sector;
            }
         }else{
            if(!strcmp(primary_sector->GetName1(), name)) frm = primary_sector;
            else
            if(!(f&ENUMF_NO_BACKDROP)){
               if(!strcmp(backdrop_sector->GetName1(), name)) 
                  frm = backdrop_sector;
            }
         }
      }
   }
   return frm;
}

//----------------------------
                              //scene frames
I3D_RESULT I3D_scene::AddFrame(PI3D_frame frm){

   switch(frm->GetType1()){
   case FRAME_VISUAL:
      if(I3DCAST_VISUAL(frm)->GetVisualType1() != I3D_VISUAL_PARTICLE) break;
      cont.AddAnimFrame(frm);
      break;
   case FRAME_MODEL:
      {
                              //check if it's already in some other scene
         PI3D_model mod = I3DCAST_MODEL(frm);
         if(mod->in_container){
                              //can't be in 2 scenes at the same time
            return I3DERR_GENERIC;
         }
         mod->in_container = &cont;
                              //if it contains animation, create interpolator
         if(mod->IsAnimated())
            cont.AddAnimFrame(frm);
      }
      break;
   }
   cont.frames.push_back(frm);

   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_scene::RemoveFrame(PI3D_frame frm){

   for(dword i=0; i<cont.frames.size(); i++)
   if(frm==cont.frames[i]){

      switch(frm->GetType1()){
      case FRAME_VISUAL:
         if(I3DCAST_VISUAL(frm)->GetVisualType1() != I3D_VISUAL_PARTICLE) break;
         cont.RemoveAnimFrame(frm);
         break;
      case FRAME_MODEL:
         {
            PI3D_model mod = I3DCAST_MODEL(frm);
            if(mod->in_container==&cont){
               if(mod->IsAnimated())
                  cont.RemoveAnimFrame(frm);
               mod->in_container = NULL;
            }
         }
         break;
      case FRAME_SECTOR:
         {
            for(int j=sectors.size(); j--; )
               if(sectors[j]==frm) break;
            if(j!=-1){
               sectors[j] = sectors.back(); sectors.pop_back();
            }
            PI3D_sector sct = I3DCAST_SECTOR(frm);
            sct->Disconnect(this);
            if(current_camera && current_camera->GetCurrSector()==sct)
               current_camera->SetCurrSector(NULL);
         }
         break;
      }
      cont.frames[i] = cont.frames.back(); cont.frames.pop_back();
      return I3D_OK;
   }
   return I3DERR_OBJECTNOTFOUND;
}

//----------------------------

PI3D_frame I3D_scene::CreateFrame(dword t, dword sub_type) const{

   switch(t){
   case FRAME_VISUAL: 
      {
         dword num_plg = num_visual_plugins;
                                    //scan through registered plugins
         for(dword i=0; i<num_plg; i++)
         if(visual_plugins[i].visual_type == sub_type)
            return (*visual_plugins[i].create_proc)(drv);
      }
      return NULL;
   case FRAME_CAMERA: return new I3D_camera(drv);
   case FRAME_LIGHT: return new I3D_light(drv);
   case FRAME_DUMMY: return new I3D_dummy(drv);
   case FRAME_USER: return new I3D_user(drv);
   case FRAME_MODEL: return CreateModel(this);
   case FRAME_JOINT: return new I3D_joint(drv);
   case FRAME_VOLUME: return new I3D_volume(this);
   case FRAME_OCCLUDER: return new I3D_occluder(drv);
   case FRAME_SECTOR:
      {
         PI3D_sector sct = new I3D_sector(drv);
         sectors.push_back(sct);
         return sct;
      }
   case FRAME_SOUND:
      {
         PI3D_sound sp = new I3D_sound(this);
         assert(sp);
         sounds.push_back(sp);
         drv->sounds.push_back(sp);
         return sp;
      }
   }
   return NULL;
}

//----------------------------

I3D_RESULT I3D_scene::SetActiveCamera(PI3D_camera cam){

   current_camera = cam;
   return I3D_OK;
}

//----------------------------
                              //nested sectors not supported by this function!
void I3D_scene::SetupCameraSector(){

                              //if no camera, there's nothing to update
   if(!current_camera)
      return;

   PI3D_sector old_sct = current_camera->GetCurrSector();
                              //determine sector we're in
   const S_vector &cpos = current_camera->GetWorldPos();

#if 1
   if(old_sct){
                              //try old sector first, maybe we're lucky
      if(old_sct->CheckPoint(cpos)){
         if(!current_camera->GetCurrSector()->IsPrimary1())
            return;
      }
   }
                              //determine sector
   for(int i=sectors.size(); i--; ){
      PI3D_sector sct = sectors[i];
      if(sct->IsSimple())
         continue;
      if(sectors[i]->CheckPoint(cpos))
         break;
   }
                              //note: cast to PI3D_sector, so that compiler doesn't generate smart-pointers
   PI3D_sector curr_sct = i!=-1 ? (PI3D_sector)sectors[i] : (PI3D_sector)primary_sector;
   current_camera->SetCurrSector(curr_sct);

#else
                              //nested version
   struct S_hlp{
      static I3DENUMRET I3DAPI enum_cam(PI3D_frame frm, dword dwcam){

         PI3D_camera cam = (PI3D_camera)dwcam;
         PI3D_sector sct = I3DCAST_SECTOR(frm);
         if(sct->CheckPoint(cam->GetWorldPos())){
                                    //maybe this
            cam->SetCurrSector(sct);
            return I3DENUMRET_OK;   //enum further - maybe in sub-sector
         }
         return I3DENUMRET_SKIPCHILDREN;
      }
   };
   if(old_sct){
                              //try old sector first, maybe we're lucky
      if(old_sct->CheckPoint(cpos)){
         if(current_camera->GetCurrSector()->IsPrimary1())
            EnumFrames(S_hlp::enum_cam, (dword)(PI3D_camera)current_camera, ENUMF_SECTOR);
         else{
                              //nested sectors
            old_sct->EnumFrames(S_hlp::enum_cam, (dword)(PI3D_camera)current_camera, ENUMF_SECTOR, NULL);
         }
         //goto report;
         return;
      }
      current_camera->SetCurrSector(NULL);
   }
                              //determine sector
   EnumFrames(S_hlp::enum_cam, (dword)(PI3D_camera)current_camera, ENUMF_SECTOR);
   if(!current_camera->curr_sector)
      current_camera->SetCurrSector(primary_sector);

#endif
}

//----------------------------

I3D_RESULT I3D_scene::UnmapScreenPoint(int x1, int y1, S_vector &pos, S_vector &dir) const{

   if(!current_camera)
      return I3DERR_NOTINITIALIZED;

   if(x1<viewport.l || x1>=viewport.r || y1<viewport.t || y1>=viewport.b)
      return I3DERR_INVALIDPARAMS;

   current_camera->UpdateCameraMatrices(aspect_ratio);

   const S_matrix &cm = current_camera->GetMatrix();
                              //get camera position
   pos = cm(3);

                              //get centered screen position
   float x =  (((float)x1) - viewport_center[0]) / viewport_size_half[0];
   float y = -(((float)y1) - viewport_center[1]) / viewport_size_half[0];

   if(!current_camera->GetOrthogonal()){
                              //adjust height
      //if(!(scn_flags&SCNF_VIEWPORT_ASPECT))
         y *= (viewport_size_half[0]/-viewport_size_half[1]*.75f);
      y *= 1.333333f;
                              //perspective camera
      S_matrix m = current_camera->GetProjectionMatrix1();
      m(0, 3) = 0.0f; m(1, 3) = 0.0f; m(2, 3) = 0.0f; m(3, 3) = 1.0f;
      m = current_camera->GetViewMatrix1() * m;
      m.Invert();


      float z;
      {
         float ncp, fcp;
         current_camera->GetRange1(ncp, fcp);
         z = 0.0f;
         if(fcp>MRG_ZERO){
            float delta = fcp - ncp;
            if(delta>MRG_ZERO){
               z = fcp / delta;
            }
         }
      }
      dir = S_vector(x, y, z).RotateByMatrix(m);
   }else{
                              //orthogonal camera
      float s = 1.0f/current_camera->GetOrthoScale1();
      pos += cm(0) * (x*s) + cm(1) * (y*s);
      dir = cm(2);
   }

   dir.Normalize();

   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_scene::TransformPointsToScreen(const S_vector *in, S_vectorw *out, dword num) const{

   if(!current_camera)
      return I3DERR_GENERIC;

   current_camera->UpdateCameraMatrices(aspect_ratio);
   ((I3D_scene*)this)->SetMViewProj(NULL);

   const S_matrix &mat = m_view_proj_hom;
   while(num--){
      float rhw = fabs(in->x*mat.m[0][3] + in->y*mat.m[1][3] + in->z*mat.m[2][3] + mat.m[3][3]);
      if(IsMrgZeroLess(rhw)){
         out->x = out->y = out->z = out->w = 0.0f;
      }else{
         rhw = 1.0f / rhw;
         out->w = rhw;
         out->x = (in->x*mat.m[0][0] + in->y*mat.m[1][0] + in->z*mat.m[2][0] + mat.m[3][0]) * rhw * viewport_size_half[0] + viewport_center[0];
         out->y = (in->x*mat.m[0][1] + in->y*mat.m[1][1] + in->z*mat.m[2][1] + mat.m[3][1]) * rhw * viewport_size_half[1] + viewport_center[1];
         out->z = (in->x*mat.m[0][2] + in->y*mat.m[1][2] + in->z*mat.m[2][2] + mat.m[3][2]) * rhw;
      }
   }

   return I3D_OK;
}

//----------------------------

void I3D_scene::SetMViewProj(const I3D_camera *cam, bool z_bias){

   if(!cam)
      cam = current_camera;
   const S_matrix &m_view = cam->GetViewMatrix1();
   const S_projection_matrix &m_proj_simple = !z_bias ? cam->GetProjectionMatrixSimple() : cam->GetProjectionMatrixBiasedSimple();
   m_view_proj_hom = m_view.MultByProj(m_proj_simple);
   mt_view_proj_hom.CopyTransposed(m_view_proj_hom);
#ifdef GL
   const S_projection_matrix &gl_m_proj_simple = !z_bias ? cam->GlGetProjectionMatrixSimple() : cam->GlGetProjectionMatrixBiasedSimple();
   gl_m_view_proj_hom = m_view.MultByProj(gl_m_proj_simple);
#endif
}

//----------------------------

I3D_RESULT I3D_scene::SetViewport(const I3D_rectangle &rc, bool aspect){

   if(rc.r<=rc.l || rc.b<=rc.t ||
      rc.l<0 || rc.t<0 ||
      rc.r>(int)drv->GetGraphInterface()->Scrn_sx() || rc.b>(int)drv->GetGraphInterface()->Scrn_sy())
      return I3DERR_INVALIDPARAMS;
   viewport = rc;

   viewport_size_half[0] = (rc.r - rc.l) * .5f;
   viewport_size_half[1] = -(rc.b - rc.t) * .5f;
   
   viewport_center[0] = viewport.l + viewport_size_half[0];
   viewport_center[1] = viewport.t - viewport_size_half[1];

   if(aspect){
      scn_flags |= SCNF_VIEWPORT_ASPECT;
      aspect_ratio = (float)(rc.b-rc.t) / (float)(rc.r-rc.l);
   }else{
      scn_flags &= ~SCNF_VIEWPORT_ASPECT;
      aspect_ratio = .75f;
   }
   if(current_camera){
      current_camera->ResetProjectionMatrix();
      current_camera->ResetViewMatrix();
   }

   return I3D_OK;
}

//----------------------------
#define SECTOR_SOUND_FADE 1000

void I3D_scene::UpdateSoundProperties(int time, PI3D_sector prev_sector){

   PISND_driver isp = drv->GetSoundInterface();
   if(!isp)
      return;
                              //check current sector, update if necessary
   assert(current_camera);
   PI3D_sector sct = current_camera->GetCurrSector();
   if(!sct)
      return;

   bool sector_switch = (prev_sector!=sct);
   if(sector_switch){
      if(!prev_sector)
         ssp_count_down = 0;
      else{
                              //start fade
         if(!ssp_count_down){
                              //setup source fade values
            const S_sound_env_properties *ep = drv->GetEnvProperty(prev_sector->GetSoundEnvID());
            if(!ep)
               ep = drv->GetEnvProperty(0);
            ssp_fade = *ep;
         }
         ssp_count_down = SECTOR_SOUND_FADE;
      }
   }

   if(ssp_count_down || (drv->GetFlags()&DRVF_SOUND_ENV_RESET)){

      drv->drv_flags &= ~DRVF_SOUND_ENV_RESET;

#if defined USE_EAX && 1

      const S_sound_env_properties *ssp_dst = drv->GetEnvProperty(sct->GetSoundEnvID());
      if(!ssp_dst)
         ssp_dst = drv->GetEnvProperty(0);

                              //compute faded values
      S_sound_env_properties ssp;
      if(ssp_count_down){
         if((ssp_count_down -= time) <= 0){
            ssp_count_down = 0;
         }
         ssp.MakeBlend(*ssp_dst, ssp_fade, (float)ssp_count_down/(float)SECTOR_SOUND_FADE);
      }else
         ssp = *ssp_dst;

                              //setup room (lf, hf)
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_ROOM,
         FloatToInt(EAXLISTENER_MINROOM + ssp.room_level_lf * (EAXLISTENER_MAXROOM - EAXLISTENER_MINROOM)));
      
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_ROOMHF,
         FloatToInt(EAXLISTENER_MINROOMHF + ssp.room_level_hf * (EAXLISTENER_MAXROOMHF - EAXLISTENER_MINROOMHF)));
      
                              //setup decay (time, hf)
      float decay_time = Max(EAXLISTENER_MINDECAYTIME, Min(EAXLISTENER_MAXDECAYTIME,
         (float)ssp.decay_time*.001f));
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_DECAYTIME, I3DFloatAsInt(decay_time));

      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_DECAYHFRATIO,
         I3DFloatAsInt(EAXLISTENER_MINDECAYHFRATIO + ssp.decay_hf_ratio * (EAXLISTENER_MAXDECAYHFRATIO - EAXLISTENER_MINDECAYHFRATIO)));

                              //setup reflections (amount, delay)
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_REFLECTIONS,
         FloatToInt(EAXLISTENER_MINREFLECTIONS + ssp.reflections * (EAXLISTENER_MAXREFLECTIONS - EAXLISTENER_MINREFLECTIONS)));

      float refl_delay = Max(EAXLISTENER_MINREFLECTIONSDELAY, Min(EAXLISTENER_MAXREFLECTIONSDELAY,
         (float)ssp.reflections_delay*.001f));
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_REFLECTIONSDELAY, I3DFloatAsInt(refl_delay));

                              //setup reverb (amount, delay)
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_REVERB,
         FloatToInt(EAXLISTENER_MINREVERB + ssp.reverb * (EAXLISTENER_MAXREVERB - EAXLISTENER_MINREVERB)));

      float rev_delay = Max(EAXLISTENER_MINREVERBDELAY, Min(EAXLISTENER_MAXREVERBDELAY,
         (float)ssp.reverb_delay*.001f));
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_REVERBDELAY, I3DFloatAsInt(rev_delay));
      
                              //setup diffusion
      isp->SetEAXProperty(DSPROPERTY_EAXLISTENER_ENVIRONMENTDIFFUSION, I3DFloatAsInt(ssp.env_diffusion));

#endif //USE_EAX
   }
   const S_matrix &m = current_camera->GetMatrixDirect();
   isp->SetListenerPos(m(3));
   isp->SetListenerDir(m(2), m(1));

   isp->UpdateListener();
}

//----------------------------

void I3D_scene::AttenuateSounds(){

   if(!sounds.size())
      return;
   PI3D_sound *sptr = &*sounds.begin();
   for(int i=sounds.size(); i--; ){
      PI3D_sound snd = *sptr++;
      snd->VirtualPause();
   }
}

//----------------------------

I3D_RESULT I3D_scene::SetRenderMatrix(const S_matrix &m){

   S_matrix mt;
   mt.Make4X4Transposed(m, m_view_proj_hom);
   drv->SetVSConstant(VSC_MAT_TRANSFORM_0, &mt, 4);
#ifdef GL
   curr_render_matrix.Make4X4(m, gl_m_view_proj_hom);
#endif
   return I3D_OK;
}

//----------------------------

static const word indx[] = {0, 1};

I3D_RESULT I3D_scene::DrawLine(const S_vector &v1, const S_vector &v2, dword color) const{

   S_vector v[2] = {v1, v2};
   return DrawLines(v, 2, indx, 2, color);
}

//----------------------------

I3D_RESULT I3D_scene::DrawLines(const S_vector *v, dword numv, const word *indx, dword numi, dword color) const{

   if(!numi)
      return I3D_OK;

   PROFILE(drv, PROF_RENDER);

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   const dword vstride = sizeof(S_vector);

   if(numv*vstride > VB_TEMP_SIZE || numi*sizeof(word) > IB_TEMP_SIZE)
      return I3DERR_INVALIDPARAMS;

   if((color>>24) != 0xff){
      drv->SetupBlend(I3DBLEND_ALPHABLEND);
   }else
      drv->SetupBlend(I3DBLEND_OPAQUE);

   drv->SetupAlphaTest(false);

   S_vectorw c4(((color>>16)&255)*R_255, ((color>>8)&255)*R_255, ((color>>0)&255)*R_255, ((color>>24)&255)*R_255);

   I3D_driver::S_vs_shader_entry_in se;//(vs_decl_line);
   se.AddFragment(VSF_TRANSFORM);

   if(drv->CanUsePixelShader()){
      I3D_driver::S_ps_shader_entry_in se_ps;
      se_ps.AddFragment(PSF_COLOR_COPY);
      drv->DisableTextures(0);
      drv->SetPixelShader(se_ps);

      drv->SetPSConstant(PSC_COLOR, &c4);
   }
#ifndef GL
   else{
      drv->SetupTextureStage(0, D3DTOP_MODULATE);
      drv->DisableTextureStage(1);
      drv->SetTexture(NULL);
                              //copy color through VS
      se.AddFragment(VSF_LIGHT_BEGIN);
      se.AddFragment(VSF_LIGHT_END);
      drv->SetVSConstant(VSC_AMBIENT, &c4);
   }
#endif

   //drv->SetClipping(true);
   drv->SetFVF(D3DFVF_XYZ);

   HRESULT hr;

   I3D_driver::S_vs_shader_entry *see = drv->GetVSHandle(se);
   if(!see)
      return I3DERR_GENERIC;
   drv->SetVertexShader(see->vs);
   
   {
      IDirect3DVertexBuffer9 *vb = drv->vb_temp;
      IDirect3DIndexBuffer9 *ib = drv->ib_temp;

      void *vp;
      hr = vb->Lock(0, numv*vstride, &vp, D3DLOCK_DISCARD);
      CHECK_D3D_RESULT("Lock", hr);

      memcpy(vp, v, numv*vstride);

      hr = vb->Unlock();
      CHECK_D3D_RESULT("Unlock", hr);

      hr = ib->Lock(0, numi*sizeof(word), &vp, D3DLOCK_DISCARD);
      CHECK_D3D_RESULT("Lock", hr);

      memcpy(vp, indx, numi*sizeof(word));

      hr = ib->Unlock();
      CHECK_D3D_RESULT("Unlock", hr);

      drv->SetStreamSource(vb, vstride);
      drv->SetIndices(ib);

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_LINELIST, 0, 0, numv, 0, numi/2);
      CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
   }
#ifdef GL
   class C_lines_gl_program: public C_gl_program{
      virtual void BuildStoreAttribId(dword index, dword id){
         *(((int*)&u_mat_view_proj)+index) = id;
      }
   public:
      C_lines_gl_program(I3D_driver &d):
         C_gl_program(d)
      {}
      int u_mat_view_proj, a_pos, u_color;
   };
   C_lines_gl_program *glp = (C_lines_gl_program*)(C_gl_program*)drv->gl_shader_programs[drv->GL_PROGRAM_DRAW_LINES];
   if(!glp){
      glp = new C_lines_gl_program(*drv);
      drv->gl_shader_programs[drv->GL_PROGRAM_DRAW_LINES] = glp;
      glp->Release();
      glp->Build(
         "attribute vec4 a_pos;"
         "uniform mat4 u_mat_view_proj;"
         "void main(){"
         "  gl_Position = u_mat_view_proj * a_pos;"
         "}"
         ,
         "precision lowp float;"
         "uniform vec4 u_color;"
         "void main() {"
         "  gl_FragColor = u_color;"
         "}"
         ,
         "u_mat_view_proj\0" "a_pos\0" "u_color\0"
         );
   }
   glp->Use();
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   glUniformMatrix4fv(glp->u_mat_view_proj, 1, false, curr_render_matrix.m[0]); CHECK_GL_RESULT("glUniformMatrix4fv");
   glUniform4fv(glp->u_color, 1, c4.f); CHECK_GL_RESULT("glUniform4fv");
   glEnableVertexAttribArray(glp->a_pos);  CHECK_GL_RESULT("glEnableVertexAttribArray");
   glVertexAttribPointer(glp->a_pos, 3, GL_FLOAT, false, 0, v); CHECK_GL_RESULT("glVertexAttribPointer");
   glDrawElements(GL_LINES, numi, GL_UNSIGNED_SHORT, indx); CHECK_GL_RESULT("glDrawElements");
#endif
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_scene::DrawTriangles(const void *v, dword numv, dword vc_flags, const word *indx, dword numi, dword color) const{

   PROFILE(drv, PROF_RENDER);

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   dword fvf = ConvertFlags(vc_flags);
   if(fvf==-1)
      return I3DERR_INVALIDPARAMS;

   dword vstride = GetSizeOfVertex(fvf);
   if(numv*vstride > VB_TEMP_SIZE || numi*sizeof(word) > IB_TEMP_SIZE)
      return I3DERR_INVALIDPARAMS;

   dword alpha = color>>24;

   if(alpha!=0xff){
      //drv->SetupAlphaTest(true);
      //drv->SetAlphaRef(0x20 * alpha / 255);
      drv->SetupBlend(I3DBLEND_ALPHABLEND);
   }else
      drv->SetupBlend(I3DBLEND_OPAQUE);
   drv->SetupAlphaTest(false);

   //drv->SetClipping(true);
   drv->SetFVF(fvf);

   dword numf = numi / 3;

   IDirect3DVertexBuffer9 *vb = drv->vb_temp;
   IDirect3DIndexBuffer9 *ib = drv->ib_temp;
   {
                              //fill-in vertex and index buffers
      void *vp;
      hr = vb->Lock(0, numv*vstride, &vp, D3DLOCK_DISCARD);
      CHECK_D3D_RESULT("Lock", hr);

      memcpy(vp, v, numv*vstride);

      hr = vb->Unlock();
      CHECK_D3D_RESULT("Unlock", hr);

      hr = ib->Lock(0, numi*sizeof(word), &vp, D3DLOCK_DISCARD);
      CHECK_D3D_RESULT("Lock", hr);

      memcpy(vp, indx, numi * sizeof(word));

      hr = ib->Unlock();
      CHECK_D3D_RESULT("Unlock", hr);

      drv->SetStreamSource(vb, vstride);
      drv->SetIndices(ib);
   }

   I3D_driver::S_ps_shader_entry_in se_ps;

#ifndef GL
   if(!drv->CanUsePixelShader()){
      drv->SetupTextureStage(0, D3DTOP_MODULATE);
      drv->DisableTextureStage(1);
   }
#endif

   if(!(vc_flags&I3DVC_XYZRHW)){
      I3D_driver::S_vs_shader_entry_in se;
      se.AddFragment(VSF_TRANSFORM);

      if(drv->CanUsePixelShader()){
         if(drv->last_texture[0])
            se_ps.Tex(0);
         if(vc_flags&I3DVC_DIFFUSE)
            se_ps.AddFragment(drv->last_texture[0] ? PSF_MOD_t0_v0 : PSF_v0_COPY);
         else{
            se_ps.AddFragment(drv->last_texture[0] ? PSF_MOD_t0_CONSTCOLOR : PSF_COLOR_COPY);
            S_vectorw c4(((color>>16)&255)*R_255, ((color>>8)&255)*R_255, ((color>>0)&255)*R_255, (float)alpha*R_255);
            drv->SetPSConstant(PSC_COLOR, &c4);
         }
         drv->SetPixelShader(se_ps);
      }else
#ifndef GL
      {
         if(vc_flags&I3DVC_DIFFUSE)
            se.AddFragment(VSF_DIFFUSE_COPY);
         else{
            se.AddFragment(VSF_LIGHT_BEGIN);
            se.AddFragment(VSF_LIGHT_END);
            S_vectorw c4(((color>>16)&255)*R_255, ((color>>8)&255)*R_255, ((color>>0)&255)*R_255, (float)alpha*R_255);
            drv->SetVSConstant(VSC_AMBIENT, &c4);
         }
      }
#endif
      if(vc_flags&I3DVC_TEXCOUNT_MASK)
         se.CopyUV(0, 0);
      drv->SetVertexShader(drv->GetVSHandle(se)->vs);

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, numv, 0, numf);
      CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
   }else{
#ifndef GL
      if(!drv->CanUsePixelShader()){
         if(!(vc_flags&I3DVC_DIFFUSE)){
                           //setup color factor
            drv->SetTextureFactor(color);
            if(drv->last_texture[0]){
               d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
               d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
            }else{
               d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
               d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
               drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
            }
         }
      }else
#endif
      {
         if(drv->last_texture[0]){
            se_ps.Tex(0);
            if(vc_flags&I3DVC_DIFFUSE)
               se_ps.AddFragment(PSF_MOD_t0_v0);
            else
               se_ps.AddFragment(PSF_MOD_t0_CONSTCOLOR);
         }else{
            if(vc_flags&I3DVC_DIFFUSE)
               se_ps.AddFragment(PSF_v0_COPY);
            else
               se_ps.AddFragment(PSF_COLOR_COPY);
         }
         if(!(vc_flags&I3DVC_DIFFUSE)){
            S_vectorw c4(((color>>16)&255)*R_255, ((color>>8)&255)*R_255, ((color>>0)&255)*R_255, (float)alpha*R_255);
            drv->SetPSConstant(PSC_COLOR, &c4);
         }
         drv->SetPixelShader(se_ps);
      }

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, numv, 0, numf);
      CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
#ifndef GL
      if(!(vc_flags&I3DVC_DIFFUSE) && !drv->CanUsePixelShader()){
         if(drv->last_texture[0]){
            d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
            d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
         }else{
            d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
         }
      }
#endif
   }
#ifdef GL
   class C_this_gl_program: public C_gl_program{
      virtual void BuildStoreAttribId(dword index, dword id){
         *(((int*)&a_pos)+index) = id;
      }
   public:
      C_this_gl_program(I3D_driver &d):
         C_gl_program(d)
      {}
      int a_pos, u_color;
      int u_mat_view_proj;
   };
   C_this_gl_program *glp = (C_this_gl_program*)(C_gl_program*)drv->gl_shader_programs[drv->GL_PROGRAM_DRAW_TRIANGLES];
   if(!glp){
      glp = new C_this_gl_program(*drv);
      drv->gl_shader_programs[drv->GL_PROGRAM_DRAW_TRIANGLES] = glp;
      glp->Release();
      glp->Build(
         "attribute vec4 a_pos;"
         "uniform mat4 u_mat_view_proj;"
         "void main(){"
         "  gl_Position = u_mat_view_proj * a_pos;"
         "}"
         ,
         "precision lowp float;"
         "uniform vec4 u_color;"
         "void main() {"
         "  gl_FragColor = u_color;"
         "}"
         ,
         "a_pos\0" "u_color\0"
         "u_mat_view_proj\0"
         );
   }
   glp->Use();
   //glBindTexture(GL_TEXTURE_2D, 0);
   if(vc_flags&I3DVC_XYZRHW){
                           //undo points from screen space to viewport space
      S_matrix mat;
      mat.Identity();
      float sx = drv->GetGraphInterface()->Scrn_sx(), sy = drv->GetGraphInterface()->Scrn_sy();
      mat(0, 0) = 2.0f / sx;
      mat(1, 1) = -2.0f / sy;
      mat(3, 0) = -1.0f;
      mat(3, 1) = 1.0f;
      glUniformMatrix4fv(glp->u_mat_view_proj, 1, false, mat.m[0]); CHECK_GL_RESULT("glUniformMatrix4fv");
   }else
      glUniformMatrix4fv(glp->u_mat_view_proj, 1, false, curr_render_matrix.m[0]); CHECK_GL_RESULT("glUniformMatrix4fv");

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

   S_vectorw c4(((color>>16)&255)*R_255, ((color>>8)&255)*R_255, ((color>>0)&255)*R_255, ((color>>24)&255)*R_255);
   glUniform4fv(glp->u_color, 1, c4.f); CHECK_GL_RESULT("glUniform4fv");
   glEnableVertexAttribArray(glp->a_pos);  CHECK_GL_RESULT("glEnableVertexAttribArray");
   glVertexAttribPointer(glp->a_pos, (vc_flags&I3DVC_XYZ) ? 3 : 4, GL_FLOAT, false, vstride, v); CHECK_GL_RESULT("glVertexAttribPointer");
   glDrawElements(GL_TRIANGLES, numi, GL_UNSIGNED_SHORT, indx); CHECK_GL_RESULT("glDrawElements");
#endif
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_scene::DrawSprite(const S_vector &pos, PI3D_material mat, dword color, float size, const I3D_text_coor in_uv[2]) const{

   if(!current_camera)
      return I3DERR_INVALIDPARAMS;

   PROFILE(drv, PROF_RENDER);

   const S_matrix &m = current_camera->GetMatrix();
   HRESULT hr;

   dword alpha = color>>24;

   {
      IDirect3DVertexBuffer9 *vb = drv->vb_temp;

      struct S_vertex{
         S_vector v;
         I3D_text_coor tex;
      };
      S_vertex *verts;

      hr = vb->Lock(0, 4*sizeof(S_vertex), (void**)&verts, D3DLOCK_DISCARD);
      CHECK_D3D_RESULT("Lock", hr);

      for(int j=0; j<4; j++){
         S_vector &v = verts[j].v;
         if(in_uv){
            verts[j].tex.x = in_uv[j&1].x;
            verts[j].tex.y = in_uv[j>>1].y;
         }else{
            verts[j].tex.x = (float)(j&1);
            verts[j].tex.y = (float)(j>>1);
         }
         v = pos;
         v -= m(0) * (size*.5f);
         v -= m(1) * (size*.5f);
         if(j&1)
            v += m(0) * size;
         if(!(j&2))
            v += m(1) * size;
      }
      hr = vb->Unlock();
      CHECK_D3D_RESULT("Unlock", hr);

      drv->SetStreamSource(vb, sizeof(S_vertex));
   }

   drv->SetTexture1(0, mat ? mat->GetTexture1(MTI_DIFFUSE) : NULL);
   if(mat && mat->IsCkeyAlpha1()) drv->SetAlphaRef(mat->GetAlphaRef());

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   S_vectorw c4 = mat ? mat->GetDiffuse1() : S_vectorw(1, 1, 1, 1);
   c4.x *= (float)((color>>16)&0xff) * R_255;
   c4.y *= (float)((color>>8)&0xff) * R_255;
   c4.z *= (float)((color>>0)&0xff) * R_255;
   c4.w = alpha * R_255;

   I3D_driver::S_vs_shader_entry_in se;//(vs_decl_icon);
   se.AddFragment(VSF_TRANSFORM);
   se.CopyUV(0, 0);

   if(drv->CanUsePixelShader()){
      I3D_driver::S_ps_shader_entry_in se_ps;
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_MOD_t0_CONSTCOLOR);
      drv->SetPixelShader(se_ps);

      drv->SetPSConstant(PSC_COLOR, &c4);
   }
#ifndef GL
   else{
      drv->SetupTextureStage(0, D3DTOP_MODULATE);
      drv->DisableTextureStage(1);
                              //copy color through VS
      se.AddFragment(VSF_LIGHT_BEGIN);
      se.AddFragment(VSF_LIGHT_END);
      drv->SetVSConstant(VSC_AMBIENT, &c4);
   }
#endif

   drv->SetupBlend((alpha!=0xff) ? I3DBLEND_ALPHABLEND : (mat && mat->IsTransl() ? I3DBLEND_ALPHABLEND : I3DBLEND_OPAQUE));
   
   drv->SetupAlphaTest(true);
   dword ar = (0x20 * alpha)>>8;
   drv->SetAlphaRef(ar);
   
   //drv->SetClipping(true);

   drv->SetVertexShader(drv->GetVSHandle(se)->vs);
   drv->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);

   drv->SetVSConstant(VSC_MAT_TRANSFORM_0, &mt_view_proj_hom, 4);

   //hr = d3d_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(S_vertex));
   hr = d3d_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   CHECK_D3D_RESULT("DrawPrimitive", hr);

   //drv->SetupAlphaTest(false);
   drv->ResetStreamSource();
   return I3D_OK;
}

//----------------------------

void I3D_scene::DrawIcon(const S_vector &pos, int res_id, dword color, float size){

   PI3D_material mat = drv->GetIcon(res_id);
   if(mat)
      DrawSprite(pos, mat, color, size);
}

//----------------------------

#define SPHERE_NUM_VERTS 24
static S_vector sphere_circle[3][SPHERE_NUM_VERTS];
static word sphere_indicies[3][SPHERE_NUM_VERTS*2];
static word cylinder_indicies[3][SPHERE_NUM_VERTS*2];

static struct S_auto_init_sphere{
   S_auto_init_sphere(){
                              //init volume sphere
      for(int j=0; j<3; j++){
         for(int i=0; i<SPHERE_NUM_VERTS; i++){
            sphere_indicies[j][i*2+0] = word(SPHERE_NUM_VERTS*j+i+0);
            sphere_indicies[j][i*2+1] = word(SPHERE_NUM_VERTS*j+i+1);
                                  
            float s = (float)sin(PI*.5f + (PI*2.0f)*i / SPHERE_NUM_VERTS),
                  c = (float)cos(PI*.5f + (PI*2.0f)*i / SPHERE_NUM_VERTS);

            sphere_circle[j][i].x = (j ? s : 0);
            sphere_circle[j][i].y = (!j ? s : j==2 ? c : 0);
            sphere_circle[j][i].z = ((j<2) ? c : 0);
         }
         sphere_indicies[j][i*2-1] = word(SPHERE_NUM_VERTS*j);
      }
      memcpy(cylinder_indicies[0], sphere_indicies[0], sizeof(cylinder_indicies[0])*2);
      for(int i=SPHERE_NUM_VERTS; i--; ){
         cylinder_indicies[2][i*2+0] = word(i);
         cylinder_indicies[2][i*2+1] = word(SPHERE_NUM_VERTS + i);
      }
   }
} auto_init_sphere;

//----------------------------

void I3D_scene::DebugDrawSphere(const S_matrix &m1, float radius, dword color){

                              //scale by radius
   S_matrix m = I3DGetIdentityMatrix();
   m(0, 0) = m(1, 1) = m(2, 2) = radius;
   SetRenderMatrix(m * m1);
   DrawLines(sphere_circle[0], SPHERE_NUM_VERTS*3, sphere_indicies[0], SPHERE_NUM_VERTS*2*3, color);
}

//----------------------------

void I3D_scene::DebugDrawCylinder(const S_matrix &m1, float radius1, float radius2, float height, dword color){

   S_vector circle[2][SPHERE_NUM_VERTS];
   for(int i=SPHERE_NUM_VERTS; i--; ){
      circle[0][i].x = sphere_circle[2][i].x * radius1;
      circle[0][i].z = sphere_circle[2][i].y * radius1;
      circle[0][i].y = 0.0f;

      circle[1][i].x = sphere_circle[2][i].x * radius2;
      circle[1][i].z = sphere_circle[2][i].y * radius2;
      circle[1][i].y = height;
   }
   SetRenderMatrix(m1);
   DrawLines(circle[0], SPHERE_NUM_VERTS*2, cylinder_indicies[0], SPHERE_NUM_VERTS*2*3, color);
}

//----------------------------

void I3D_scene::DebugDrawCylinder(const S_matrix &m1, float radius, float height, dword color, bool capped){

   S_matrix m;
   m.Identity();
   m(0, 0) = m(1, 1) = m(2, 2) = radius;
                              //draw 2 rings
   for(int i=2; i--; ){
      m(3).z = !i ? -height : i==1 ? height : 0.0f;
      SetRenderMatrix(m * m1);
      DrawLines(sphere_circle[2], SPHERE_NUM_VERTS, sphere_indicies[0], SPHERE_NUM_VERTS*2, color);
      if(capped){
                              //draw half spheres at caps
         if(i!=2){
            const word *beg = sphere_indicies[0];
            if(i) 
               beg += SPHERE_NUM_VERTS;
            DrawLines(sphere_circle[0], SPHERE_NUM_VERTS, beg, SPHERE_NUM_VERTS*1, color);
            DrawLines(sphere_circle[1], SPHERE_NUM_VERTS, beg, SPHERE_NUM_VERTS*1, color);
         }
      }
   }
   SetRenderMatrix(m1);
                              //draw 4 vertical lines
   for(i=4; i--; ){
      float x = 0;
      float y = 0;
      if(i&2)
         x = (i&1) ? radius : -radius;
      else
         y = (i&1) ? radius : -radius;
      S_vector v0(x, y, height);
      S_vector v1(x, y, -height);
      DrawLine(v0, v1, color);
   }
}

//----------------------------

void I3D_scene::DebugDrawArrow(const S_matrix &m1, float lenght, dword color, int axis){

   static const S_vector dir_lines[3][7] = {
                              //x-oriented
      S_vector(0, 0, 0), S_vector(1, 0, 0),
      S_vector(1.3f, 0.0f, 0.0f),
      S_vector(1.0f, .1f, .1f), S_vector(1.0f, -.1f, .1f), S_vector(1.0f, -.1f, -.1f), S_vector(1.0f, .1f, -.1f),
                              //y-oriented
      S_vector(0, 0, 0), S_vector(0, 1, 0),
      S_vector(0.0f, 1.3f, 0.0f),
      S_vector(.1f, 1.0f, .1f), S_vector(-.1f, 1.0f, .1f), S_vector(-.1f, 1.0f, -.1f), S_vector(.1f, 1.0f, -.1f),
                              //z-oriented
      S_vector(0, 0, 0), S_vector(0, 0, 1),
      S_vector(0.0f, 0.0f, 1.3f),
      S_vector(.1f, .1f, 1.0f), S_vector(-.1f, .1f, 1.0f), S_vector(-.1f, -.1f, 1.0f), S_vector(.1f, -.1f, 1.0f),
   };
   static const word indx[] = {
      0, 1,
      2, 3, 2, 4, 2, 5, 2, 6,
      3, 4, 4, 5, 5, 6, 6, 3
   };
   S_matrix m = I3DGetIdentityMatrix();
   m(axis, axis) = lenght;
   SetRenderMatrix(m * m1);
   DrawLines(dir_lines[axis], sizeof(dir_lines[0])/sizeof(S_vector), indx, sizeof(indx)/sizeof(word), color);
}

//----------------------------

void I3D_scene::DebugDrawBBox(const S_vector *bbox_full, dword color){

   static word bbox_list[] = {
      0, 1, 2, 3, 4, 5, 6, 7,
      0, 2, 1, 3, 4, 6, 5, 7,
      0, 4, 1, 5, 2, 6, 3, 7
   };
   DrawLines(bbox_full, 8, bbox_list, sizeof(bbox_list)/sizeof(word), color);
}

//----------------------------

void I3D_scene::DebugDrawContour(const S_vector *contour_points, dword num_cpts, dword color){

   for(dword i=0; i<num_cpts; i++)
      DrawLine(contour_points[i], contour_points[(i+1)%num_cpts], color);
}

//----------------------------

void I3D_scene::DestroyBspTree(){

   bsp_tree.Clear();
   bsp_draw_help.Clear();
}

//----------------------------
bool I3D_scene::SaveBsp(C_cache *cp) const{

   I3D_RESULT ir = I3D_OK;
   ir = bsp_tree.Save(cp);
   return I3D_SUCCESS(ir);
}

//----------------------------

bool I3D_scene::LoadBsp(C_cache *cp, I3D_LOAD_CB_PROC *cbP, void *context, dword check_flags){

   I3D_RESULT ir = I3D_OK;
   try{
      ir = bsp_tree.Load(this, cp, cbP, context, check_flags);
   }catch(...){
      if(cbP){
         (*cbP)(CBM_ERROR, (dword)"LoadBsp: Exception caught - possibly data corrupted.", 0, context);
      }
      ir = I3DERR_FILECORRUPTED;
   }
   if(I3D_FAIL(ir))
      bsp_tree.Clear();

   return I3D_SUCCESS(ir);
}

//----------------------------

bool I3D_scene::IsBspBuild() const{
   return bsp_tree.IsValid();
}

//----------------------------
/*
PI3D_visual I3D_scene::GetDebugDrawVisual(const char *name){

   if(!debug_mesh_model){
      debug_mesh_model = I3DCAST_MODEL(CreateFrame(FRAME_MODEL));
      debug_mesh_model->Release();
      C_chunk ck;
      if(OpenResource(drv->GetHInstance(), "BINARY", "DEBUGDRAW", *ck.GetHandle())){
         ck.SetOpenModeByHandle();
         debug_mesh_model->GetContainer1()->OpenFromChunk(ck, 0, NULL, NULL, this, debug_mesh_model, NULL, drv);
      }
   }
   PI3D_frame frm = debug_mesh_model->FindChildFrame(name, ENUMF_VISUAL);
   return I3DCAST_VISUAL(frm);
}

//----------------------------

void I3D_scene::RenderDebugDrawVisual(PI3D_visual vis, const S_matrix &matrix, const S_matrix &m_inv){

   PI3D_mesh_base mb = vis->GetMesh();
   if(mb){
      I3D_driver::S_vs_shader_entry_in se;
      se.AddFragment(VSF_TRANSFORM);
      se.AddFragment(VSF_SIMPLE_DIR_LIGHT);
      I3D_driver::S_vs_shader_entry *se1 = drv->GetVSHandle(se);
      drv->SetVertexShader(se1->vs);

      drv->SetTexture1(0, NULL);
      drv->DisableTextureStage(1);

      IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
      SetRenderMatrix(matrix);
      drv->SetClipping(true);
      drv->SetupBlend(I3DBLEND_OPAQUE);

                        //setup camera vector in visual's local coords
      {
         const S_matrix &m_cam = current_camera->GetMatrix();
         S_vectorw vw = m_cam(2);
         vw.w = 0.0f;
         vw.Invert();
         vw %= m_inv;
         drv->SetVSConstant(VSC_MAT_TRANSFORM_1, &vw);
      }

      IDirect3DVertexBuffer9 *vb_src = mb->vertex_buffer.GetD3DVertexBuffer();
      drv->SetStreamSource(vb_src, mb->vertex_buffer.GetSizeOfVertex());
      //const I3D_face_group *fg = &mb->GetFGroupVector()[0];
      IDirect3DIndexBuffer9 *ib = mb->GetIndexBuffer().GetD3DIndexBuffer();
      dword vertex_count = mb->vertex_buffer.NumVertices();
      dword ib_base = mb->GetIndexBuffer().D3D_index_buffer_index * 3;
      dword face_count = mb->GetIndexBuffer().NumFaces1();
      drv->EnableNoCull(false);

      drv->SetIndices(ib);
      HRESULT hr;
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mb->vertex_buffer.D3D_vertex_buffer_index, 0,
         vertex_count, ib_base, face_count);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}
/**/

//----------------------------

void I3D_scene::GetStaticFrames(C_vector<PI3D_frame> &frm_list) const{

   static struct S_hlp{

      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

         if(!(frm->GetFlags()&I3D_FRMF_STATIC_COLLISION))
            return I3DENUMRET_OK;

         C_vector<PI3D_frame> &frmlist = *(C_vector<PI3D_frame> *)c;

         switch(frm->GetType1()){
         case FRAME_VOLUME:
            {
               PI3D_volume vol = I3DCAST_VOLUME(frm);
               switch(vol->GetVolumeType1()){
               case I3DVOLUME_BOX:
               case I3DVOLUME_RECTANGLE:
                  frmlist.push_back(vol);
                  break;
               default:
                  break;     //unsupport type for build in bsp
               }
            }
            break;
         case FRAME_VISUAL:
            frmlist.push_back(frm);
            break;
         default:
            assert(0);
         }
         return I3DENUMRET_OK;
      }
   }hlp;
   frm_list.clear();
   EnumFrames(S_hlp::cbEnum, (dword)&frm_list, ENUMF_VOLUME | ENUMF_VISUAL);
}

//----------------------------

void I3D_scene::StoreFileName(const C_str &fn){ cont.StoreFileName(fn); }
const C_str &I3D_scene::GetFileName() const{ return cont.GetFileName(); }
dword I3D_scene::NumFrames() const{ return cont.NumFrames1(); }
PI3D_frame const *I3D_scene::GetFrames(){ return cont.GetFrames(); }
CPI3D_frame const *I3D_scene::GetFrames() const{ return cont.GetFrames(); }
I3D_RESULT I3D_scene::AddInterpolator(PI3D_interpolator i){ return cont.AddInterpolator(i); }
I3D_RESULT I3D_scene::RemoveInterpolator(PI3D_interpolator i){ return cont.RemoveInterpolator(i); }
dword I3D_scene::NumInterpolators() const{ return cont.NumInterpolators1(); }
PI3D_interpolator const *I3D_scene::GetInterpolators(){ return cont.GetInterpolators1(); }
CPI3D_interpolator const *I3D_scene::GetInterpolators() const{ return cont.GetInterpolators1(); }
I3D_RESULT I3D_scene::Tick(int time, PI3D_CALLBACK cb, void *cbc){
   PROFILE(drv, PROF_ANIMS);
   return cont.Tick(time, cb, cbc);
}
const C_str &I3D_scene::GetUserData() const{ return cont.GetUserData(); }
void I3D_scene::SetUserData(const C_str &s){ cont.SetUserData(s); }

//----------------------------
//----------------------------
void TestColLineCylinder(const S_vector &in_pos, const S_vector &z_axis,
   float radius, float half_length, PI3D_frame col_frm, I3D_collision_data &cd, bool capped);

#define L_F_DEBUG(n)
//#define L_F_DEBUG(n) drv->DEBUG(n)

//----------------------------
// Check if moving sphere has any chance to hit the bounding-sphere.
inline bool FastTest_S_S(const S_trace_help &th, const I3D_bsphere &sphere){

   const float radius_sum = th.radius + sphere.radius;

                              //get normalized position of bsphere on tested line
   float pol = th.dir_norm.Dot(sphere.pos - th.from);

                              //cull bsphere out of ends
   if(pol < -radius_sum || pol-th.dir_magn > radius_sum)
      return false;
                              //compute point on line
   S_vector pt = th.from;
                              //clamp pol to be on the dir range
   if(!CHECK_ZERO_LESS(pol)){
      if(FLOAT_BITMASK(pol) >= FLOAT_BITMASK(th.dir_magn))
         pt += th.dir;
      else
         pt += th.dir_norm * pol;
   }
                              //get distance to bsphere
   float d_2 = (sphere.pos-pt).Square();
   float r_2 = radius_sum*radius_sum;
   return (FLOAT_BITMASK(d_2) <= FLOAT_BITMASK(r_2));
}

//----------------------------
                              //hidden implementation of I3D_scene
                              // this class mainly deals with collision testing
class I3D_scene_imp: public I3D_scene, public I3D_driver::C_reset_callback{

   struct S_dyn_vol_bound{
      S_vector_i min, max;

      void Invalidate(){
         min.x = min.y = min.z = 0x7fffffff;
         max.x = max.y = max.z = 0x80000000;
      }

      void Shrink(){
         for(int i=3; i--; ){
            min[i] >>= 1;
            max[i] >>= 1;
         }
      }
   };

//----------------------------

   virtual void ResetCallback(I3D_driver::E_RESET_MSG msg){
      switch(msg){
      case I3D_driver::RESET_RECREATE:
#ifdef GL
         glClearColor(color_bgnd.x, color_bgnd.y, color_bgnd.z, 0);
#endif
         break;
      }
   }

//----------------------------
public:

   typedef I3D_volume::S_octtree_cell t_cell;

//----------------------------
                              //dynamic collision tester class
   class C_dynamic_collision_info{
   public:
      class C_octtree_level{
         typedef C_link_list<t_cell> t_list;
      public:
         const dword hash_tab_size;
         dword num_volumes;
                              //last member - this struct is variable-sized
         t_list hash_table[1];

         C_octtree_level(dword level):
            num_volumes(0),
            hash_tab_size(GetHashTableSize(level))
         {
            memset(hash_table, 0, sizeof(t_list)*hash_tab_size);
         }

         static dword GetHashTableSize(dword level){
                              //the smaller level, the greater hash-table
                              // use some reasonable minimal and maximal table size
            return Max(0x100, Min(0x8000, 0x40000 >> level));
         }

         C_link_list<t_cell> &GetCell(int x, int y, int z){
            dword index = dword(x*113 + z*13 + y) & (hash_tab_size-1);
            return hash_table[index];
         }
         C_link_list<t_cell> &GetCell(const S_vector_i &i){ return GetCell(i.x, i.y, i.z); }

         void *operator new(size_t sz, dword level){
            sz += sizeof(t_list) * (GetHashTableSize(level)-1);
            return new byte[sz];
         }
         void operator delete(void *vp){
            delete[] (byte*)vp;
         }
         void operator delete(void *vp, dword){
            delete[] (byte*)vp;
         }
      };
                              //the number of levels determines the max-level
      C_buffer<C_octtree_level*> levels;

      C_dynamic_collision_info()
      {}

      ~C_dynamic_collision_info(){
         SetNumLevels(0);
      }

      void SetNumLevels(dword num){
                              //free redundant tables
         for(dword i=num; i<levels.size(); i++)
            delete levels[i];
         i = levels.size();
         levels.resize(num, NULL);
                              //alloc new tables
         for(; i<num; i++)
            levels[i] = new(i) C_octtree_level(i);
      }

   //----------------------------
   // Determine the oct-tree level from a bounding-sphere of volume.
      static dword GetLevel(float side_size){

                                    //find level such that .5 * 2^level < sz <= 2^level
#if 0
         int level;
                                    //sz = (0.5 .. 1.0) * 2^level (definition of frexp)
         frexp(side_size*DYNAMIC_CELL_SCALE, &level);
                                    //clip lower value to some mininal level
         return Max(level, 0);
#else
         side_size *= DYNAMIC_CELL_SCALE;
         if(side_size < 16.0f){
                              //0 - 16
            if(side_size < 4.0f){
                              //0 - 4
               if(side_size < 2.0f){
                              //0 - 2
                  if(side_size < 1.0f)
                     return 0;
                  return 1;
               }
                              //2-4
               return 2;
            }
                              //4 - 16
            if(side_size < 8.0f)
               return 3;
            return 4;
         }
                              //16 - n
         if(side_size < 64.0f){
                              //32 - 256
            if(side_size < 32.0f)
               return 5;
            return 6;
         }
                              //64 - n
         if(side_size < 256.0f){
                              //64 - 256
            if(side_size < 128.0f)
               return 7;
            return 8;
         }
                              //256 - n
         if(side_size < 512.0f)
            return 9;
                              //the rest must fix to this (max) level
         return 10;
#endif
      }

   //----------------------------
   // Get reciprocal of dynamic cell size on given level.
      static float GetCellInvSize(dword level){
         //DYNAMIC_CELL_SCALE
                              //we could return 1/DYNAMIC_CELL_SCALE * 2^level
                              // (using look-up table should be faster and more precise.
         static const float cell_sizes[] = {
            DYNAMIC_CELL_SCALE/1.0f, DYNAMIC_CELL_SCALE/2.0f, DYNAMIC_CELL_SCALE/4.0f, DYNAMIC_CELL_SCALE/8.0f,
            DYNAMIC_CELL_SCALE/16.0f, DYNAMIC_CELL_SCALE/32.0f, DYNAMIC_CELL_SCALE/64.0f, DYNAMIC_CELL_SCALE/128.0f,
            DYNAMIC_CELL_SCALE/256.0f, DYNAMIC_CELL_SCALE/512.0f
         };
         assert(level < sizeof(cell_sizes)/sizeof(float));
         return cell_sizes[level];
      }
   //----------------------------

      dword GetLevel(CPI3D_volume vol){

         switch(vol->GetVolumeType1()){
         case I3DVOLUME_BOX:
         case I3DVOLUME_RECTANGLE:
            {
               const float *sz = vol->GetWorldSize();
               const I3D_bbox &bb1 = vol->GetBoundBox();
               const S_vector *n = vol->GetNormals();
               I3D_bbox bb;
               bb.Invalidate();
               for(int i=0; i<8; i++){
                  S_vector v = bb1.min;
                  if(i&1) v += n[0]*sz[0];
                  if(i&2) v += n[1]*sz[1];
                  if(i&4) v += n[2]*sz[2];
                  bb.min.Minimal(v);
                  bb.max.Maximal(v);
               }
               float sz1 = Max(bb.max.x-bb.min.x, Max(bb.max.y-bb.min.y, bb.max.z-bb.min.z));
               //float sz1 = Max(sz[0], Max(sz[1], sz[2]));
               return GetLevel(sz1);
            }
            break;
         default:
            return GetLevel(vol->GetBoundSphere().radius * 2.0f);
         }
      }

   //----------------------------
      typedef I3D_volume::S_tag_node t_mark_vol_node;
                              //linked list of all volumes which were marked in any collision test
                              // the list is cleared at destructor
                              // entry in the list marks that its 'I3D_volume::S_tag_node::test_col_tag' flags
                              // are valid, and specify all levels, in which volume was already tested
                              //this is to avoid duplicated tests of volumes on any level
      typedef C_link_list<t_mark_vol_node> t_marked_volumes;
      mutable t_marked_volumes marked_volumes;

   //----------------------------

      typedef pair<PI3D_volume, dword> t_saved_marked_vol;

   //----------------------------
   // Save markded volume list into provided buffer - this is done due to recursive collision tests.
   // The call also clears the 'marked_volumes' list for the next use by C_dyn_vol_tree_tester.
      void SaveAndClearMargedList(C_vector<t_saved_marked_vol> &saved_marks){

         saved_marks.reserve(128);
         for(t_mark_vol_node *p=marked_volumes; p; ){
            t_mark_vol_node *pp = p;
            p = ++*p;
            saved_marks.push_back(t_saved_marked_vol(pp->this_vol, pp->test_col_tag));
            pp->RemoveFromList();
         }
      }

   //----------------------------
   // Restore the volume tags in marked list from a buffer after collision test is done.
      void RestoreMarkedList(const C_vector<t_saved_marked_vol> &saved_marks){

         assert(!marked_volumes);
         for(dword i=saved_marks.size(); i--; ){
            const t_saved_marked_vol &mv = saved_marks[i];
            marked_volumes.Add(&mv.first->tag_node);
            mv.first->tag_node.test_col_tag = mv.second;
         }
      }
   };
private:
   mutable C_dynamic_collision_info dyn_cols;

                              //'dirty' volumes (those called UpdateDynamicVolume)
                              // this list is cleaned when collision is tested (i.e. BuildDynamicVolTree was called)
   mutable C_link_list<I3D_volume::S_dirty_node> dirty_volumes;

                              //statistics
   enum{
      COL_STATS_S_S, COL_STATS_S_R, COL_STATS_S_B, COL_STATS_S_F,
      COL_STATS_L_S, COL_STATS_L_R, COL_STATS_L_B, COL_STATS_L_F,
      COL_STATS_LAST
   };
   mutable dword col_stats[COL_STATS_LAST];
   mutable I3D_stats_voltree voltree_stats;

//----------------------------
public:
   struct S_debug_cell{
      dword level;
      S_vector_i pos;
      dword color;
      S_debug_cell(){}
      S_debug_cell(dword l, const S_vector_i &v, dword c = 0xffffffff):
         level(l), pos(v), color(c)
      {}
      bool operator <(const S_debug_cell &c) const{
         if(level<c.level) return true; if(level>c.level) return false;
         if(pos.x<c.pos.x) return true; if(pos.x>c.pos.x) return false;
         if(pos.y<c.pos.y) return true; if(pos.y>c.pos.y) return false;
         return (pos.z<c.pos.z);
      }
   };
private:
   typedef set<S_debug_cell> t_debug_cells;
   mutable t_debug_cells debug_cells;


//----------------------------
// Prepare data in the collision structure (before testing).
// Return true if preparation was successful.
   bool PrepareCollisionTest(I3D_collision_data &cd) const{

                                 //reset output values
      cd.hit_distance = 1e+16f;
      cd.hit_frm = NULL;
      cd.face_index = -1;

      switch(cd.flags&0xffff){
      case I3DCOL_STATIC_SPHERE:
      case I3DCOL_STATIC_BOX:
         return true;
      }

      cd.destination = cd.from + cd.dir;
      cd.dir_magnitude = cd.dir.Magnitude();
                                 //zero length direction? preparation fails
      if(IsMrgZeroLess(cd.dir_magnitude))
         return false;

      cd.normalized_dir = cd.dir / cd.dir_magnitude;

                                 //if line segment is used, limit current hit distance to dir magnitude
      if(!(cd.flags&I3DCOL_RAY))
         cd.hit_distance = cd.dir_magnitude;


                                 //debugging support - draw lines of the testing
      if(drv->GetFlags2()&DRVF2_DRAWCOLTESTS){
         static const dword colors[] = { 0xffffff00, 0xff00ff00, 0xffff0000 };
         dword ci = 0;
         switch(cd.flags&0xffff){
         case I3DCOL_MOVING_SPHERE:
         case I3DCOL_MOVING_GROUP:
            ci = 1;
            break;
         case I3DCOL_EXACT_GEOMETRY:
         case I3DCOL_EDIT_FRAMES:
         case I3DCOL_NOMESH_VISUALS:
         case I3DCOL_VOLUMES:
            ci = 2;
            break;
         }
         drv->DebugLine(cd.from, cd.from + cd.dir, 1, colors[0]);
      }
      return true;
   }

//----------------------------
                                 //sphere group support
   struct S_sphere_hlp{
      float radius;              //radius of sphere
      S_vector vol_delta;        //delta position against master frame
      PI3D_volume vol;           //pointer to sphere volume
   };

//----------------------------
// Recursive function for collecting spheres in moving sphere group.
   static void CollectSpheresRecursive(PI3D_frame root, const S_vector &master_pos, S_sphere_hlp sphere_list[], dword &num_sphere_list){

      const C_vector<PI3D_frame> &children = root->GetChildren1();
      PI3D_frame const *cptr = &children.front();
      for(int i=children.size(); i--; ){
         PI3D_frame frm = *cptr++;
         if(frm->GetType1()==FRAME_VOLUME && frm->IsOn1()){
            PI3D_volume vol = I3DCAST_VOLUME(frm);
            if(vol->GetVolumeType1()==I3DVOLUME_SPHERE){
               //sphere_list.push_back(S_sphere_hlp());
               assert(num_sphere_list<MAX_MOVING_SPHERE_NUM);
               if(num_sphere_list==MAX_MOVING_SPHERE_NUM) return;
               S_sphere_hlp &sh = sphere_list[num_sphere_list++];

               vol->Prepare();
               const I3D_bsphere bs = vol->GetBoundSphere();
               sh.radius = bs.radius;
               sh.vol_delta = bs.pos - master_pos;
               sh.vol = vol;
            }
         }
         if(frm->NumChildren1())
            CollectSpheresRecursive(frm, master_pos, sphere_list, num_sphere_list);
      }
   }

//----------------------------
// Collect spheres in moving sphere group.
   static void CollectGroupInfo(I3D_collision_data &cd, S_sphere_hlp sphere_list[], dword &num_sphere_list){

      if(cd.frm_root){
                                 //get info about spheres group
         CollectSpheresRecursive(cd.frm_root, cd.frm_root->GetWorldPos(), sphere_list, num_sphere_list);
      }else
      if(cd.vol_group && cd.num_vols){
         const S_vector &master_pos = cd.from;
         for(dword i = 0; i < cd.num_vols; i++ ){
                                 //do not test invalid volumes
            PI3D_volume vol = cd.vol_group[i];
            if(!vol)
               continue;
            assert(num_sphere_list<MAX_MOVING_SPHERE_NUM);
            if(num_sphere_list==MAX_MOVING_SPHERE_NUM) return;

            S_sphere_hlp &sh = sphere_list[num_sphere_list++];
            //if(vol->GetVolFlags()&VOLF_RESET)
            vol->Prepare();
            const I3D_bsphere bs = vol->GetBoundSphere();
            sh.radius = bs.radius;
            sh.vol_delta = bs.pos - master_pos;
            sh.vol = vol;
         }
      }
   }

//----------------------------
// Detect collision of ray with bounding box.
// Parameters:
//    mat - matrix for bbox transformation
//    bb - bounding box
//    from, norm_dir - ray
//    len - length of ray used for testing
//    hit_dist_ret - when not NULL, it is filled with collision distance if collision detected
// Return value:
//    true if collision was detected
// Note:
//    If 'from' is inside of bounding box, collision is detected and closest distance is set to 0.0
   static bool pickBBoxTest(const S_matrix &mat, const I3D_bbox &bb1, const S_vector &from1, const S_vector &norm_dir1,
      const float len1, float *hit_dist_ret = NULL){

                                 //consider we're in the box
      bool in = true;
                                 //cached distances, keeping min/max extremes
      float hmin = 0.0f;
      float lmax = 1.0e+10f;
                                 //rotate the bounding box by matrix
      I3D_bbox bbox;
      bbox.min = bb1.min * mat;
      bbox.max = bb1.max * mat;
                                 //check all 3 axes
      for(int j=0; j<3; j++){
         float d_min =  mat(j).Dot(bbox.min);
         float d_max = -mat(j).Dot(bbox.max);

                                 //distances in which our ray collides with
                                 //min/max sides of box
         float u_min, u_max;

         float dot_pos = mat(j).Dot(from1);

         u_max = -(dot_pos + d_max);
         u_min = -(dot_pos - d_min);

         float f = mat(j).Dot(norm_dir1);

                                 //check if we aren't parallel with this side
         if(IsAbsMrgZero(f)){
                                 //check if any of 'from' or 'to' is inside
                                 //fast quit 1 - missing parallel side
            if(u_min>=0.0f || u_max<0.0f){
               return false;
            }
            continue;
         }
         float r_dot_dir = 1.0f / f;
         u_max *= r_dot_dir;
         u_min *= r_dot_dir;

                                 //determine which side (min/max) we've hit first
         if(u_max < u_min)
            swap(u_max, u_min);

                                 //both sides behind our back, exit
         if(u_max <= 0.0f)
            return false;

         if(u_min > 0.0f){
                                 //both sides in front of us, we can't be in
            hmin = Max(hmin, u_min);
            in = false;
         }
         lmax = Min(lmax, u_max);
      }
                                 //we're inside we're colliding in lmax distance
      if(in){
         if(hit_dist_ret)
            *hit_dist_ret = 0.0f;
         return true;
      }
                                 //test if our collision computations end up on the box
      if(hmin > lmax)
         return false;

                                 //we've hit the box
                                 //check if we have already better collision detected
      if(len1 <= hmin)
         return false;

                                 //collision detected
      if(hit_dist_ret)
         *hit_dist_ret = hmin;
      return true;
   }

//----------------------------
                              //helper class for oct-tree collision testing
                              // - used for actual iteration of oct-tree, based on provided bounding-box info
                              // - designed to work for any test against dynamic volumes
                              // - performing actual test by calling (pure) virtual function 'DoTest'
   class C_dyn_vol_tree_tester{

      dword collide_bits;     //lower 16 bits used only
#ifdef _DEBUG
      dword num_cells_tested;
      dword avg_level;
      dword num_tests;
#endif

   //----------------------------
   // Test bounding range of cells in a dynamic-collision oct-tree level.
   //    bb ... real AABB of tested line
   //    bound ... cell bounds of cells to test
   //    level ... current level we test
   //    do_flag_clear ... mark tested volumes, clear flags after test
      void TestDynamicLevel(const S_dyn_vol_bound &bound, dword level){

         C_dynamic_collision_info::C_octtree_level &l = *dyn_cols.levels[level];
         assert(level < 32);
                              //make bit, by which we mark this level
         const dword level_bit = 1<<level;
#ifdef _DEBUG
         ++num_cells_tested;
         avg_level += level;
#endif

                              //iterate through the cell range
         S_vector_i it;
         it.x = bound.min.x;
         do{
            it.y = bound.min.y;
            do{
               it.z = bound.min.z;
               do{
                  bool go_deeper = false;
                  bool any_test = false;
                              //test all cells associated with the position
                  for(t_cell *cp = l.GetCell(it); cp; cp = ++*cp){
                     PI3D_volume vol = cp->vol;
                              //if off or ignored, skip
                     if(frm_ignore && ((frm_ignore==vol->GetOwner1()) || (frm_ignore==vol)))
                        continue;
                     if(frm_ignore1 && vol==frm_ignore1)
                        continue;
                              //since we use hash table, the cell we get may be on different coordinates
                     if(cp->pos!=it)
                        continue;
                              //check if collide bits match
                     if(!(vol->GetCollideCategoryBits()&collide_bits))
                        continue;
                     //if(!vol->IsOn1())
                        //continue;
                     assert(vol->IsOn1());
                              //if volume was already tested, skip it
                              // (this is done by writing into 'tag_node.test_col_tag' flags of volume
                     if(!vol->tag_node.IsInList()){
                        dyn_cols.marked_volumes.Add(&vol->tag_node);
                        vol->tag_node.test_col_tag = 0;
                     }
                     if(vol->tag_node.test_col_tag&level_bit)
                        continue;
                              //set the level bit
                     vol->tag_node.test_col_tag |= level_bit;

                              //check if it's volume's bottom-most level
                     if(vol->curr_octtree_level==(int)level){
                              //do the test directly on this volume, this is its level
                        DoTest(vol);
                        any_test = true;
#ifdef _DEBUG
                        ++num_tests;
#endif
                     }else{
                              //volume is on sub-level
                              // mark to go into sub-level of the cell
                        go_deeper = true;
                     }
                  }
                  if(debug_cells){
                     static const dword colors[4] = { 0x30ffffff, 0x8000ff00, 0x80ff0000, 0x80ffff00};
                     debug_cells->insert(S_debug_cell(level, it, colors[int(any_test)*2 | int(go_deeper)]));
                  }
                  if(go_deeper){
                              //a request to nest into deeper level of this cell
                     assert(level);
                     dword sub_level = level - 1;
                              //compute bounds in sub-level coords
                     S_dyn_vol_bound b_in;
                     b_in.min = it;
                     for(int i=3; i--; ){
                        b_in.min[i] *= 2;
                        b_in.max[i] = b_in.min[i] + 1;
                     }
                              //compute real bounding-box influence
                     const float r_cell_size = dyn_cols.GetCellInvSize(sub_level);
                     S_dyn_vol_bound b_full;
                     {
                        b_full.min.Floor(test_bounds[0]*r_cell_size);
                        b_full.max = b_full.min;
                        S_vector_i vi;
                        vi.Floor(test_bounds[1]*r_cell_size);
                        b_full.min.Minimal(vi);
                        b_full.max.Maximal(vi);
                     }
                              //clip sub-level area by real bounds
                     b_in.min.Maximal(b_full.min);
                     b_in.max.Minimal(b_full.max);
                              //test the sub-level cells
                     if(b_in.min.x <= b_in.max.x && b_in.min.y <= b_in.max.y && b_in.min.z <= b_in.max.z)
                        TestDynamicLevel(b_in, sub_level);
                  }
               } while(++it.z <= bound.max.z);
            } while(++it.y <= bound.max.y);
         } while(++it.x <= bound.max.x);
      }

   //----------------------------

   public:
                              //bounding box of the testing (in world coords)
      I3D_bbox test_bounds;
      C_dynamic_collision_info &dyn_cols;
      set<S_debug_cell> *debug_cells;

      CPI3D_frame frm_ignore; //ignored frame (if set as owner or if it's volume itself)
      CPI3D_frame frm_ignore1;//ignored frame (if it's volume itself)

      C_dyn_vol_tree_tester(C_dynamic_collision_info &dc, t_debug_cells *deb, dword cbits):
         dyn_cols(dc),
         debug_cells(deb),
         collide_bits(cbits),
#ifdef _DEBUG
         num_cells_tested(0),
         avg_level(0),
         num_tests(0),
#endif
         frm_ignore(NULL),
         frm_ignore1(NULL)
      {}
      ~C_dyn_vol_tree_tester(){
         ClearMarkedList();
      }

   //----------------------------
   // Test single cell (and possibly all sub-lebels) on current level.
   // Calling function must perform its own clearing of I3D_volume::test_col_tag, which are set inside.
      void TestDynamicCell(const S_vector_i &pos){

         dword level = dyn_cols.levels.size()-1;
         S_dyn_vol_bound bound;
         bound.min = pos;
         bound.max = pos;
         TestDynamicLevel(bound, level);
      }

   //----------------------------
   // Hi-level function, starting the test. Member 'test_bounds' must be properly initialized before calling this.
      void Test(){

         if(!dyn_cols.levels.size())
            return;
         dword level = dyn_cols.levels.size()-1;
                              //get bounds of the line test
         const float r_cell_size = dyn_cols.GetCellInvSize(level);
                              //determine line's bounds in current level
         S_dyn_vol_bound bound;
         bound.Invalidate();
         for(int i=2; i--; ){
            S_vector_i vi;
            vi.Floor(test_bounds[i]*r_cell_size);
            bound.min.Minimal(vi);
            bound.max.Maximal(vi);
         }
         TestDynamicLevel(bound, level);
      }

   //----------------------------
   // Clear the linked-list of marked volumes.
      void ClearMarkedList(){
         dyn_cols.marked_volumes.ClearFull();
      }

   //----------------------------
   // A place-holder function, letting inherited classes do the actual test against given volume.
      virtual void DoTest(PI3D_volume vol) const = 0;

#ifdef _DEBUG
   //----------------------------
      void DumpStats(PI3D_driver drv){
         drv->DEBUG(C_xstr(
            "num_cells_tested: %\n"
            "avg_level: #.2%\n"
            "num_tests: %\n"
            )
            %(int)num_cells_tested
            %(float(avg_level)/float(Max(0ul, num_cells_tested)))
            %(int)num_tests
            );
      }
#endif
   };

//----------------------------
// Check moving sphere (or group) against volumes.
   void CheckColSDyn(const S_trace_help &th, dword cbits) const{

      class C_tester: public C_dyn_vol_tree_tester{
         const S_trace_help &th;
      public:
         C_tester(C_dynamic_collision_info &dc, t_debug_cells *deb, const S_trace_help &th1, dword cbits):
            C_dyn_vol_tree_tester(dc, deb, cbits),
            th(th1)
         {}

         virtual void DoTest(PI3D_volume vol) const{

            switch(vol->GetVolumeType1()){

            case I3DVOLUME_SPHERE:
               vol->CheckCol_S_S_dyn(th);
               break;

            case I3DVOLUME_CYLINDER:
               break;
            case I3DVOLUME_CAPCYL:
               vol->CheckCol_S_CC_dyn(th);
               break;

            case I3DVOLUME_BOX:
               if(FastTest_S_S(th, vol->GetBoundSphere()))
                  vol->CheckCol_S_B_dyn(th, false);
               break;

            case I3DVOLUME_RECTANGLE:
               if(FastTest_S_S(th, vol->GetBoundSphere()))
                  vol->CheckCol_S_R_dyn(th);
               break;

            default: assert(0);
            }

         }
      } tester(dyn_cols,
         (drv->GetFlags2()&DRVF2_DRAWCOLTESTS) ? &debug_cells : NULL, th, cbits);
      tester.frm_ignore = th.frm_ignore;
                              //countruct AABB of line's influence
      I3D_bbox &bb = tester.test_bounds;
      S_vector to = th.from + th.dir;
      bb.min = th.from;
      bb.max = bb.min;
      bb.min.Minimal(to);
      bb.max.Maximal(to);
                              //enlarge bounds by moving sphere radius
      float r = th.radius;
      bb.min -= S_vector(r, r, r);
      bb.max += S_vector(r, r, r);
                              //do actual testing
      tester.Test();
#if defined _DEBUG && 0
      tester.DumpStats(drv);
#endif
   }

//----------------------------
// Clip velocity by plane. Both (velocity and plane) has their origin in O.
   static void ClipVelocity(S_vector &velocity, const S_vector &normal){

	   float backoff = velocity.Dot(normal);
      velocity -= normal*backoff;
   }

//----------------------------

   static bool SolveCollision2(S_vector &solve_dir, const S_vector clip_planes[], const dword move_clip_planes_size, const S_vector &orig_dir){

                                 //make direction parallel to all planes if possible
      const S_vector *p_clip = clip_planes;

      for(dword i = 0; i<move_clip_planes_size; i++){
         float f1 = solve_dir.Dot(p_clip[i]);
         if(f1 > .1f)
            continue;            //moving outside plane, can't block current way

                                 //clip move to be parallel to plane
         ClipVelocity(solve_dir, p_clip[i]);

                                 //check for 2nd plane which can block dir
         for(dword j = 0; j<move_clip_planes_size; j++){

            if(i == j)
               continue;

            float f2 = solve_dir.Dot(p_clip[j]);
            if(f2 > .1f)
               continue;         //moving outside plane, can't block current way

                                 //try clip move to be parallel to plane
            ClipVelocity(solve_dir, p_clip[j]);

                                 //check if it is still parallel/back with first clip plane
            float f3 = solve_dir.Dot(p_clip[i]);
            if(f3 > -MRG_ZERO)
               continue;

                                 //2 planes forming line, slide along it

            const S_normal line_dir = p_clip[i].Cross(p_clip[j]);
            float d = line_dir.Dot(orig_dir);
            solve_dir = line_dir * d;

                                 //check for 3rd plane which can block dir
            for(dword k = 0; k<move_clip_planes_size; k++){

               if(i == k || j == k)
                  continue;

               float f4 = solve_dir.Dot(p_clip[k]);
               if(f4 > 1e-6)
                  continue;      //moving outside plane, can't block current way

                                 //blocked by 3 planes, stop move
               solve_dir.Zero();
               //NEW_COL_DEBUG("blocked by 3 planes!");
               return false;
            }
         }
                                 //all planes parallel or back, end current test.
         break;                  
      }

                                 // if new dir is against the original dir, stop to move to avoid occilations in corners
   #if 1
      float f5 = solve_dir.Dot(orig_dir);
	   if(CHECK_ZERO_LESS(f5))
      {
         solve_dir.Zero();
         //NEW_COL_DEBUG("velocity against primary dir!");
		   return false;
	   }
   #endif
      return true;
   }

//----------------------------

                              //how much parts of move dir we accept
   enum { MAX_NUM_BUMPS = 6, };


//----------------------------
//Default collision response, slide along the clip planes.
   static void DefaultColResponse(const S_vector &from, const S_vector &solve_dir_in, S_vector clip_planes[], dword &move_clip_planes_size,
      const S_vector &orig_dir, I3D_cresp_data &rd){

                              //if new plane is near parallel to some prewious, don't clip it again
      for(int i = move_clip_planes_size; i--; ){
         float f = rd.GetHitNormal().Dot(clip_planes[i]);
         if(f > .99f)
            break;
      }
      if(i==-1){        
         assert(("array out of range", move_clip_planes_size < MAX_NUM_BUMPS));
         clip_planes[move_clip_planes_size++] = rd.GetHitNormal();
      }
      S_vector solve_dir = solve_dir_in;

      bool stop_move = !SolveCollision2(solve_dir, clip_planes, move_clip_planes_size, orig_dir);
      //bool stop_move = !SolveCollision(solve_dir, clip_planes, orig_dir, primary_dir);
      rd.SetDestination(stop_move ? from : from + solve_dir);
   }

//----------------------------
                              //maximal number of moving volumes in a group of volumes
   enum{ MAX_MOVING_SPHERE_NUM = 32 };

   mutable C_vector<S_collision_info> trace_hlp_cache;

//----------------------------

   static int CompareCollisions(const void *_c1, const void *_c2){
      const S_collision_info &c1 = *(S_collision_info*)_c1;
      const S_collision_info &c2 = *(S_collision_info*)_c2;
      return (c1<c2) ? -1 : (c2<c1) ? 1 : 0;
   }

//----------------------------
// Perform test of moving sphere of group of spheres.
   void TestColMovingSphere(I3D_collision_data &cd) const{

                                 //sphere group support:
      S_sphere_hlp sphere_list[MAX_MOVING_SPHERE_NUM];
      dword num_sphere_list = 0;

      bool is_group = ((cd.flags&0xffff) == I3DCOL_MOVING_GROUP);
      if(is_group){
         CollectGroupInfo(cd, sphere_list, num_sphere_list);
         if(!num_sphere_list)
            return;
      }

      if(dirty_volumes)
         BuildDynamicVolTree();

      bool collided = false;          //if any collision accured

      //S_vector primary_dir = cd.dir; //primary dir, keep it to the end
      S_vector orig_dir = cd.dir;       //currenly solved segment

      S_vector solve_dir = cd.dir;      //current segment modified by clip planes
      S_vector solved_from = cd.from;
      S_vector solved_to = cd.from+cd.dir;
                              //we used this list for skip response col twice on one frame
                              //size of list could be > MAX_NUM_BUMPS, because we want prevent from test same face twice
                              //and due one 'bump test' can ce tested more then one face (ad test_next_collision)
      //S_collision_info affect_list[MAX_NUM_BUMPS]; 
      //dword affect_list_size = 0;
      C_vector<S_collision_info> affect_list;
      affect_list.reserve(MAX_NUM_BUMPS);

                                 //planes which was used for clipping of current move segment
      S_vector move_clip_planes[MAX_NUM_BUMPS];
      dword move_clip_planes_size = 0;

      trace_hlp_cache.reserve(32);
      S_trace_help trace_hlp(trace_hlp_cache);
      //trace_hlp.collision_list.reserve(128); //ususally small number of collisions expected (10-20)
      trace_hlp.radius = cd.radius;
      trace_hlp.frm_ignore = cd.frm_ignore;
      trace_hlp.flags = cd.flags;
      trace_hlp.collide_bits = cd.collide_bits;
      for(int i = MAX_NUM_BUMPS; i--; ){

         trace_hlp.collision_list.clear();
         trace_hlp.from = solved_from;
         trace_hlp.to = solved_to;
         if(!trace_hlp.Prepare())
            break;
                                 //sphere group support:
         if(is_group){
            for(int kk = num_sphere_list; kk--; ){
               const S_sphere_hlp &sh = sphere_list[kk];
               trace_hlp.from = solved_from + sh.vol_delta;
               trace_hlp.to = solved_to + sh.vol_delta;
               trace_hlp.radius = sh.radius;
               trace_hlp.vol_source = sh.vol;
               bsp_tree.TraceSphere(trace_hlp);
   //PR_BEG;
               //trace_hlp.PrepareMoveSpace(); //keep it after trace_hlp.Prepare(), it use dir_magnitude
               CheckColSDyn(trace_hlp, cd.collide_bits);
   //PR_END;
            }
         }else{
            //trace_hlp.vol_source = cd.vol; //we don't pass sphere volume to single sphere tests(we do not use any sphere volume in this test)
            bsp_tree.TraceSphere(trace_hlp);
            //trace_hlp.PrepareMoveSpace();    //keep it after trace_hlp.Prepare(), it use dir_magnitude
            CheckColSDyn(trace_hlp, cd.collide_bits);
         }
                                 //if any collison accure
         if(!trace_hlp.collision_list.size())
            break;               //no collision, entire segment pass, end test

                                 //sort collected collisions from closest to farthest
         //sort(trace_hlp.collision_list.begin(), trace_hlp.collision_list.end());
         qsort(trace_hlp.collision_list.begin(), trace_hlp.collision_list.size(), sizeof(S_collision_info), &CompareCollisions);

                                 //get closest collision, which plane was not tested
         const S_collision_info *p_col = &trace_hlp.collision_list.front();
         const int num_collisions = trace_hlp.collision_list.size();
         int responsing_col_id = 0;
   test_next_collision:
         for(int j = responsing_col_id; j < num_collisions; j++){
            const int affect_list_size = affect_list.size();
            const S_collision_info *p_affect = affect_list_size ? &affect_list.front() : NULL;
            for(int k = affect_list_size; k--; ){
                                 //skip faces already tested
               if((p_col[j].triface_id != -1) && (p_col[j].triface_id == p_affect[k].triface_id))
                  break;
                                 //skip planes on dynamic frames which was tested
               if((p_col[j].plane_id == -1) && (p_col[j].hit_frm == p_affect[k].hit_frm))
                  break;
            }
            if(k == -1)
               break;
         }
         if(j == num_collisions)
            break;               //no new plane found, end of tests

                                 //remember last response, user may want reject it
         responsing_col_id = j;

                                 //put little back from collision plane
         trace_hlp.collision_list[j].hit_distance = Max(.0f, p_col[j].hit_distance-.001f);
         const S_collision_info &ci = p_col[j];
         S_vector curr_solved_from = solved_from;

                                 //part of segment was passed successfully
         if(ci.hit_distance > MRG_ZERO){
                                 //actualy covered some distance, response only rest of line
            //affect_list_size = 0;
            affect_list.clear();
            affect_list.reserve(MAX_NUM_BUMPS);
            move_clip_planes_size = 0;
            S_vector solved_dir_norm = solved_to - solved_from;
            float len = solved_dir_norm.Magnitude();
            assert(!IsAbsMrgZero(len));
            solved_dir_norm.Normalize();
            curr_solved_from += (solved_dir_norm * ci.hit_distance);
            float rest_len = (len-ci.hit_distance);
            if(IsAbsMrgZero(rest_len)){
               solved_to = curr_solved_from;
               collided = true;
               break;
            }else{
                                 //can't be negative, ci.hit_distance must be less then entire dir
               //assert(CHECK_ZERO_GREATER(rest_len));
               assert(rest_len > -1e-6f);
               if(!CHECK_ZERO_GREATER(rest_len)){
                  //rest_len = .0f;
                  solved_to = curr_solved_from;
                  collided = true;
                  break;
               }
               solve_dir = solved_dir_norm * rest_len;
               //assert(_fpclass(solve_dir.y) != _FPCLASS_QNAN);
               orig_dir = solve_dir;
            }
         }

         affect_list.push_back(ci);
         //assert(("array out of range", affect_list_size < MAX_NUM_BUMPS));
         //affect_list[affect_list_size++] = ci;

                                 //TODO: hit_distance should contain distance from origin when first touch
         {
            if((cd.flags&I3DCOL_SOLVE_SLIDE) || cd.callback){
               I3D_cresp_data resp_data;
               resp_data.hit_frm = (PI3D_frame)ci.hit_frm;
               resp_data.hit_normal = ci.plane.normal;
               resp_data.face_normal = ci.modified_face_normal ? ci.face_normal : ci.plane.normal;
               resp_data.hit_distance = ci.hit_distance;
               resp_data.vol_source = ci.vol_source;
               resp_data.cresp_context = cd.cresp_context;
               resp_data.from = solved_from;
               resp_data.dir = solved_to - solved_from;
               //assert(_fpclass(solve_dir.y) != _FPCLASS_QNAN);
               resp_data.destination = curr_solved_from + solve_dir;
               resp_data.face_index = ci.face_index;
               bool accepted(true); //we always accept collision, unless user reject it
               if(cd.callback){
                  accepted = cd.callback(resp_data);
                  if(!accepted){
                                       //remove hit plane, which was ignored by response
                     //--affect_list_size;
                     affect_list.pop_back();
                                       //continue with next collision
                     responsing_col_id++;
                     goto test_next_collision;
                  }
               }
#if 1
                                 //if new plane is near parallel to some prewious, don't clip it again
               for(int i = move_clip_planes_size; i--; ){
                  float f = resp_data.GetHitNormal().Dot(move_clip_planes[i]);
                  if(f > .99f){
                              //note: this piece if code solve stuck place with lot af triangle with similar normal(tiny faces, triangle fans).
                              //if we could find better solution, we could have fixed affect_list size.
                                       //continue with next collision
                     responsing_col_id++;
                     goto test_next_collision;
                  }
               }
#endif
               if((cd.flags&I3DCOL_SOLVE_SLIDE) && accepted){
                  DefaultColResponse(curr_solved_from, solve_dir, move_clip_planes, move_clip_planes_size, orig_dir, resp_data);
               }

               solved_to = resp_data.GetDestination();
               solved_from = curr_solved_from;
               solve_dir = solved_to-solved_from;
               //assert(_fpclass(solve_dir.y) != _FPCLASS_QNAN);

                                 //set cd.hit_distance when sphere collide first time.
               if(!collided && accepted)
                  cd.hit_distance = ci.hit_distance;
                                 //keep cd.hit_distance on its own since it is set only when sphere collide first time.
               cd.SetReturnValues(cd.hit_distance, (PI3D_frame)ci.hit_frm, 
                  resp_data.modify_normal ? resp_data.hit_normal : ci.plane.normal);
               cd.face_index = ci.face_index;

               collided |= accepted;
                                 //only sliding function need retest new direction, custom sliding must user handle by own.
               if(!(cd.flags&I3DCOL_SOLVE_SLIDE))
                  break;
            }else{
                                 //first contact test, setup return values
               solved_to = curr_solved_from;
               cd.SetReturnValues(ci.hit_distance, (PI3D_frame)ci.hit_frm, ci.plane.normal);
               cd.face_index = ci.face_index;
               collided = true;
               break;
            }
         }
                                 //FIXME:this can be computed elsewhere
         //bool end_move = IsAbsMrgZero(solve_dir.Magnitude());
         bool end_move = IsAbsMrgZero(solve_dir.Square());

         if(end_move)
            break;
      }
      if(i == -1){
                                 //last segmnet didn't pass clearly
         //NEW_COL_DEBUG(C_fstr("sphere move: collided with more then %i planes, quit", NUM_BUMPS));
         solved_to = solved_from;
      }
      cd.destination = solved_to;
   }

//----------------------------

   struct S_pick_edit_frms_hlp{
      I3D_collision_data *cd;
      CPI3D_scene _this;
   };

//----------------------------
// Enumeration callback for testing line against editing frames (and no-mesh visuals).
   static I3DENUMRET I3DAPI cbPickEditFrames(PI3D_frame frm, dword c){

                              //if frame is off, skip it and all its children
      if(!frm->IsOn1())
         return I3DENUMRET_SKIPCHILDREN;

      S_pick_edit_frms_hlp *hp = (S_pick_edit_frms_hlp*)c;
      I3D_collision_data &cd = *hp->cd;
      bool only_nomesh_visuals = ((cd.flags&0xffff) == I3DCOL_NOMESH_VISUALS);
      dword df2 = frm->GetDriver1()->GetFlags2();

      float icon_size = .5f;
   
      switch(frm->GetType1()){
      case FRAME_JOINT:
         if(!only_nomesh_visuals){
            if(!(df2&DRVF2_DRAWJOINTS))
               return I3DENUMRET_OK;

            PI3D_joint jnt = I3DCAST_JOINT(frm);
            if(jnt->IsBBoxValid()){
               float hit_dist = 0.0f;
               S_matrix m = jnt->GetBBoxMatrix1() * jnt->GetMatrix();
               bool b;
               switch(jnt->GetRegionIndex()){
               case 2:
               case 3:
                  {
                     const I3D_bbox &bb = jnt->GetBBox1();
                     I3D_bsphere bs;
                     bs.pos = m(3);
                     bs.radius = jnt->GetMatrix()(0).Magnitude() * (bb.max-bb.min).Magnitude() * .5f;
                     b = pickBSphereTest(bs, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                     if(b)
                        hit_dist = (cd.from - bs.pos).Magnitude();
                  }
                  break;
               default:
                  b = pickBBoxTest(m, jnt->GetBBox1(), cd.from, cd.GetNormalizedDir(), cd.GetHitDistance(), &hit_dist);
               }
               if(b)
                  cd.SetReturnValues(hit_dist, frm, V_ZERO);
               return I3DENUMRET_OK;
            }
            icon_size = jnt->GetIconSize() * jnt->GetMatrix()(0).Magnitude() * .6f;
         }
         break;

      case FRAME_OCCLUDER:
         if(!only_nomesh_visuals){
            if(!(df2&DRVF2_DRAWOCCLUDERS))
               return I3DENUMRET_OK;

                        //perform bounding-box test
            PI3D_occluder occ = I3DCAST_OCCLUDER(frm);

            float hit_dist = 0.0f;
            bool b = false;

            switch(occ->GetOccluderType()){
            case I3DOCCLUDER_MESH:
                        //first make bounding-box test
               b = pickBBoxTest(frm->GetMatrix(), occ->GetBound().bound_local.bbox,
                  cd.from, cd.GetNormalizedDir(), cd.GetHitDistance(), &hit_dist);
               if(b){
                        //bb test passed, make detailed test agains
                  hit_dist = cd.GetHitDistance();
                  b = occ->TestCollision(cd.from, cd.GetNormalizedDir(), hit_dist);
               }
               break;
            case I3DOCCLUDER_SPHERE:
               {
                                    //check if it's better than what we already have                        
                  float d = (occ->GetWorldPos() - cd.from).Dot(cd.GetNormalizedDir());
                  if(d < cd.GetHitDistance()){
                     S_vector pol = cd.from + cd.GetNormalizedDir() * d;
                     float dd = (pol - occ->GetWorldPos()).Magnitude();
                                    //check if picked
                     if(dd < occ->GetWorldScale()){
                        hit_dist = d;
                        b = true;
                     }
                  }
               }
               break;
            default: assert(0);
            }
            if(b)
               cd.SetReturnValues(hit_dist, frm, V_ZERO);
            return I3DENUMRET_OK;
         }
         break;

      case FRAME_DUMMY:
         if(!(df2&DRVF2_DRAWDUMMYS))
            return I3DENUMRET_OK;
         //icon_size = frm->GetMatrix().GetScale().Magnitude() * .1f;
         icon_size = (frm->GetMatrix()(3) - hp->_this->GetActiveCamera1()->GetMatrixDirect()(3)).Magnitude() * .05f;
         break;

      case FRAME_VISUAL:
         if(only_nomesh_visuals){
                     //pick known "unpickable" visual types
            PI3D_visual vis = I3DCAST_VISUAL(frm);
            switch(vis->GetVisualType1()){
            case I3D_VISUAL_PARTICLE:
            case I3D_VISUAL_BILLBOARD:
            case I3D_VISUAL_FLARE:
            case I3D_VISUAL_DISCHARGE:
               {
                  /*
                  for(PI3D_frame prnt = frm->GetParent1(); prnt && prnt->IsOn1(); prnt = prnt->GetParent1());
                  if(prnt)
                     break;
                     */
                  if(!(vis->GetVisFlags()&VISF_BOUNDS_VALID))
                     vis->ComputeBounds();
                  float hit_dist;
                  bool b = pickBSphereTest(vis->bound.GetBoundSphereTrans(vis, FRMFLAGS_BSPHERE_TRANS_VALID),
                     cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                  if(b){
                     hit_dist = 0.0f;
                     b = pickBBoxTest(vis->GetMatrix(), vis->GetBoundVolume().bbox,
                        cd.from, cd.GetNormalizedDir(), cd.GetHitDistance(), &hit_dist);
                     if(b)
                        cd.SetReturnValues(hit_dist, frm, V_ZERO);
                  }
               }
               break;
            }
         }
         return I3DENUMRET_OK;

      case FRAME_LIGHT: if(!(df2&DRVF2_DRAWLIGHTS)) return I3DENUMRET_OK; break;
      case FRAME_SOUND: if(!(df2&DRVF2_DRAWSOUNDS)) return I3DENUMRET_OK; break;
      case FRAME_CAMERA: if(!(df2&DRVF2_DRAWCAMERAS)) return I3DENUMRET_OK; break;

      default:                //ignore unknown frames
         return I3DENUMRET_OK;
      }
      if(!only_nomesh_visuals){
                              //test distance to line
         const S_vector &pos = frm->GetWorldPos();
         float u;
         u = (pos - cd.from).Dot(cd.GetNormalizedDir()) / (cd.GetNormalizedDir().Dot(cd.GetNormalizedDir()));
         if(u>=0.0f && (!cd.GetHitFrm() || u < cd.GetHitDistance())){
            S_vector pol = cd.from + cd.GetNormalizedDir() * u;
            float d2 = (pol-pos).Square();
            if(d2 < icon_size*icon_size){
                              //picked!
               cd.SetReturnValues(u, frm, V_ZERO);
            }
         }
      }
      return I3DENUMRET_OK;
   }

//----------------------------
// Recursive function for testing collision agains all volumes in the scene hierarchy.
   static void TestColEditVolumes(PI3D_frame root, I3D_collision_data &cd){

      const C_vector<PI3D_frame> &children = root->GetChildren1();
      PI3D_frame const *cptr = &children.front();
      for(int i=children.size(); i--; ){
         PI3D_frame frm = *cptr++;
         switch(frm->GetType1()){
         case FRAME_VOLUME:
            {
               if(!frm->IsOn1())
                  continue;
               PI3D_volume vol = I3DCAST_VOLUME(frm);
               vol->Prepare();
               switch(vol->GetVolumeType1()){
               case I3DVOLUME_NULL: break;
               case I3DVOLUME_SPHERE: vol->CheckCol_L_S_dyn(cd); break;
               case I3DVOLUME_CYLINDER:
               case I3DVOLUME_CAPCYL:
                  vol->CheckCol_L_CC_dyn(cd);
                  break;
               case I3DVOLUME_BOX: vol->CheckCol_L_B_dyn(cd, false); break;
               case I3DVOLUME_RECTANGLE: vol->CheckCol_L_R_dyn(cd); break;
               default: assert(0);
               }
            }
            break;
         case FRAME_DUMMY:
         case FRAME_MODEL:
         case FRAME_VISUAL:
            if(!frm->IsOn1())
               continue;
            break;
         }
         if(frm->NumChildren1())
            TestColEditVolumes(frm, cd);
      }
   }

//----------------------------
// Test collision of edit frames and no-mesh visuals.
   void TestColEditFrames(I3D_collision_data &cd) const{

                                 //try picking non-visuals 
      dword enum_flags = 0;
      switch(cd.flags&0xffff){
      case I3DCOL_NOMESH_VISUALS:
         enum_flags |= ENUMF_VISUAL;
         break;

      case I3DCOL_EDIT_FRAMES:
         break;

      default:
         assert(0);
      }
      S_pick_edit_frms_hlp hlp;
      hlp.cd = &cd;
      hlp._this = this;
      primary_sector->EnumFrames(cbPickEditFrames, (dword)&hlp, ENUMF_ALL);
   }

//----------------------------

   static bool BoxSphereIntersection(CPI3D_volume const box, CPI3D_volume const sphere){

      assert(box && sphere);
      assert(box->GetVolumeType1() == I3DVOLUME_BOX);
      assert(sphere->GetVolumeType1() == I3DVOLUME_SPHERE);

      const I3D_bsphere &bs = sphere->GetBoundSphere();
      S_vector sphere_pos = sphere->GetWorldPos();
      float sphere_radius  = bs.radius;
                                 //transform sphere into box local coordinates
      const S_matrix &m_inv = box->GetInvMatrix();
      sphere_pos *= m_inv;
      sphere_radius *= m_inv(0).Magnitude();

      I3D_bbox bbox;
      const S_vector& box_nu_scale = box->GetNUScale();
      bbox.min = -box_nu_scale;
      bbox.max = box_nu_scale;

                                 //find the square of the distance from the sphere to the box, using James Arvo algoritm
      float s(0); float d(0);
      for( dword i=3 ; i--;){

         if( sphere_pos[i] < bbox.min[i] ){
            s = sphere_pos[i] - bbox.min[i];
            d += s*s; 
         }else
         if( sphere_pos[i] > bbox.max[i] ){ 
            s = sphere_pos[i] - bbox.max[i];
            d += s*s;
         }
      }
      return d <= sphere_radius*sphere_radius;

   }

//----------------------------
// Testing static box against dynamic volumes.
   void TestColBoxSpheresDyn(I3D_collision_data &cd) const{

   }

//----------------------------

   static void cbEnumSMandJoints(PI3D_frame root, I3D_collision_data &cd){

      const C_vector<PI3D_frame> &children = root->GetChildren1();
      PI3D_frame const *chp = &children.front();
      for(int i=children.size(); i--; ){
         PI3D_frame frm = *chp++;

                                 //check if this frame is ignored
         if(frm == cd.frm_ignore)
            continue;

                                 // ...or invisible
         if(!frm->IsOn1()){
            if(!(cd.flags&I3DCOL_INVISIBLE))
               continue;
         }
                                 // ... or no-collision
         if(frm->GetFrameFlags() & I3D_FRMF_NOCOLLISION)
            continue;

                                 //determine what to do now
         switch(frm->GetType1()){
         case FRAME_VISUAL:
            {
               PI3D_visual vis = I3DCAST_VISUAL(frm);
               if(vis->GetVisualType1()==I3D_VISUAL_SINGLEMESH){
                                 //make test on entire SM, skip children if failed
                  if(!(vis->frm_flags&FRMFLAGS_HR_BOUND_VALID))
                     vis->hr_bound.MakeHierarchyBounds(frm);
                  vis->hr_bound.GetBoundSphereTrans(frm, FRMFLAGS_HR_BSPHERE_VALID);

                  //++col_stats[COL_STATS_L_S];
                  if(!pickBSphereTest(vis->hr_bound.bsphere_world, cd.from, cd.GetNormalizedDir(), cd.hit_distance))
                     continue;

                                    //singlemesh is what we're interested in
                  I3D_object_singlemesh *sm = (I3D_object_singlemesh*)vis;

                                    //test on SM volume box
                  if(sm->IsVolumeBoxValid()){
                     //++col_stats[COL_STATS_L_B];
#ifdef USE_CAPCYL_JNT_VOLUMES
                     const S_matrix &m = sm->GetMatrix();
                     float scl = m(0).Magnitude();
                     S_vector z_axis = sm->GetVolumeDir() % m;
                     if(!IsAbsMrgZero(1.0f-scl))
                        z_axis.Normalize();
                     TestColLineCylinder(sm->GetVolumePos()*m, z_axis, sm->GetVolumeRadius()*scl, sm->GetVolumeHalfLen()*scl, sm, cd, true);
#else
                     const I3D_bbox &bbox = sm->GetVolumeBox1();
                     const S_matrix &mat = sm->GetVolumeBoxMatrix1();

                     float hit_dist;
                     bool b = pickBBoxTest(mat * frm->GetMatrix(), bbox, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance(), &hit_dist);
                     if(b){
                                 //we don't compute exact normal, just return opposite of direction vector
                        const S_vector hit_norm = -cd.dir;
                        if(cd.callback){
                           I3D_cresp_data rd(frm, hit_norm, hit_dist, cd.cresp_context);
                           rd.CopyInputValues(cd);
                           if(cd.callback(rd))
                              cd.SetReturnValues(hit_dist, frm, hit_norm);
                        }else
                           cd.SetReturnValues(hit_dist, frm, hit_norm);
                     }
#endif
                  }
                  break;
               }
            }
            break;

         case FRAME_JOINT:
            {
               PI3D_joint jnt = I3DCAST_JOINT(frm);
               if(jnt->IsVolumeBoxValid()){
                  //++col_stats[COL_STATS_L_B];
#ifdef USE_CAPCYL_JNT_VOLUMES
                  const S_matrix &m = jnt->GetMatrix();
                  float scl = m(0).Magnitude();
                  S_vector z_axis = jnt->GetVolumeDir() % m;
                  if(!IsAbsMrgZero(1.0f-scl))
                     z_axis.Normalize();
                  TestColLineCylinder(jnt->GetVolumePos()*m, z_axis, jnt->GetVolumeRadius()*scl, jnt->GetVolumeHalfLen()*scl, jnt, cd, true);
#else
                  const I3D_bbox &bbox = jnt->GetVolumeBox1();
                  const S_matrix &mat = jnt->GetVolumeBoxMatrix1();
                  float hit_dist;
                  bool b = pickBBoxTest(mat * frm->GetMatrix(), bbox, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance(), &hit_dist);
                  if(b){
                                 //we don't compute exact normal, just return opposite of direction vector
                     const S_vector hit_norm = -cd.dir;
                     if(cd.callback){
                        I3D_cresp_data rd(frm, hit_norm, hit_dist, cd.cresp_context);
                        rd.CopyInputValues(cd);
                        if(cd.callback(rd))
                           cd.SetReturnValues(hit_dist, frm, hit_norm);
                     }else
                        cd.SetReturnValues(hit_dist, frm, hit_norm);
                  }
#endif
               }
                                 //if it's 'break' joint, do not check volumes
               if(jnt->GetJointFlags()&JOINTF_BREAK)
                  continue;
            }
            break;

         case FRAME_MODEL:
            {
                                 //hierarchy test on model bounds
               PI3D_model mod = I3DCAST_MODEL(frm);
               if(frm->num_hr_vis >= 2 && frm->num_hr_jnt){
                  if(!(frm->GetFrameFlags()&FRMFLAGS_HR_BOUND_VALID))
                     mod->hr_bound.MakeHierarchyBounds(frm);
                  mod->hr_bound.GetBoundSphereTrans(frm, FRMFLAGS_HR_BSPHERE_VALID);
                  //++col_stats[COL_STATS_L_S];
                  const I3D_bsphere &bs_world = mod->hr_bound.bsphere_world;
                  float r = bs_world.radius;
                  if(!FastTest_L_S(cd, bs_world.pos, r, r*r))
                     continue;
               }
            }
            break;
         }
                                 //recurse only if the frame has any joints
         if(frm->num_hr_jnt)
            cbEnumSMandJoints(frm, cd);
      }
   }

//----------------------------
// Test collision against single-mesh joints.
   void TestVolumeSM(I3D_collision_data &cd) const{

      cbEnumSMandJoints(primary_sector, cd);
   }

//----------------------------
// Test line collision against dynamic volumes.
   void TestColLineDynamic(I3D_collision_data &cd) const{

      if((IsAbsMrgZero(cd.hit_distance)))
         return;
                              //should be called prior any other tests
      assert(!cd.GetHitFrm());

      if(dirty_volumes)
         BuildDynamicVolTree();

      if(dyn_cols.levels.size()){

         class C_tester: public C_dyn_vol_tree_tester{
            I3D_collision_data &cd;
         public:
            C_tester(C_dynamic_collision_info &dc, t_debug_cells *deb, I3D_collision_data &cd1):
               C_dyn_vol_tree_tester(dc, deb, cd1.collide_bits),
               cd(cd1)
            {}

            virtual void DoTest(PI3D_volume vol) const{

               switch(vol->GetVolumeType1()){
               case I3DVOLUME_SPHERE:
#ifdef VOLUME_STATISTICS
                  ++col_stats[COL_STATS_L_S];
#endif
                  vol->CheckCol_L_S_dyn(cd);
                  break;

               case I3DVOLUME_CYLINDER:
               case I3DVOLUME_CAPCYL:
#ifdef VOLUME_STATISTICS
                  ++col_stats[COL_STATS_L_S];
#endif
                  vol->CheckCol_L_CC_dyn(cd);
                  break;

               case I3DVOLUME_BOX:
                  {
                     const I3D_bsphere &bs = vol->GetBoundSphere();
                     if(FastTest_L_S(cd, bs.pos, bs.radius, (bs.radius*bs.radius))){
#ifdef VOLUME_STATISTICS
                        ++col_stats[COL_STATS_L_B];
#endif
                        vol->CheckCol_L_B_dyn(cd, false);
                     }
                  }
                  break;
               case I3DVOLUME_RECTANGLE:
#ifdef VOLUME_STATISTICS
                  ++col_stats[COL_STATS_L_R];
#endif
                  vol->CheckCol_L_R_dyn(cd);
                  break;

               default: assert(0);
               }
            }
         } tester(dyn_cols,
            (drv->GetFlags2()&DRVF2_DRAWCOLTESTS) ? &debug_cells : NULL, cd);
         tester.frm_ignore = cd.frm_ignore;

                              //countruct AABB of line's influence
         I3D_bbox &bb = tester.test_bounds;
         S_vector to = cd.from + cd.dir;
         bb.min = cd.from;
         bb.max = bb.min;
         bb.min.Minimal(to);
         bb.max.Maximal(to);

#if 0
         tester.Test();
#else
                              //test using sloped integer line, not a box
                              // it is much faster, because of reduced num of tested cells, as well as
                              // directioned search, instead of box-based search (stops ASAP collision is found)
         dword level = dyn_cols.levels.size()-1;
         const float r_cell_size = dyn_cols.GetCellInvSize(level);

                              //determine which axis is the main
         dword main_axis = 0;
         if(I3DFabs(cd.dir.x) < I3DFabs(cd.dir.y))
            ++main_axis;
         if(I3DFabs(cd.dir[main_axis]) < I3DFabs(cd.dir.z))
            main_axis = 2;

                              //start from current cell (keep cell pos with fractional part)
         S_vector pos = cd.from * r_cell_size;

                              //compute slope
         S_vector slope = cd.normalized_dir / cd.normalized_dir[main_axis];
         if(cd.normalized_dir[main_axis] < 0.0f)
            slope.Invert();

                              //get fractional part of main axis, towards the movement direction
         float max_axis_fract = pos[main_axis];
         max_axis_fract -= (float)FloatToInt(max_axis_fract - .5f);
         if(slope[main_axis] > 0.0f){
            max_axis_fract = 1.0f - max_axis_fract;
         }
         //drv->DEBUG(max_axis_fract);

                                 //determine number of steps
         int num_steps = FloatToInt(pos[main_axis] -.5f);
         int d = FloatToInt((cd.from[main_axis] + cd.dir[main_axis]) * r_cell_size - .5f);
         num_steps = d - num_steps;
         if(slope[main_axis] < 0.0f)
            num_steps = -num_steps;
         if(num_steps < 1){
                              //one or two cells in main axis (but still may end up in neighbour)
                              // use standard method
            tester.Test();
         }else{
            ++num_steps;
            //drv->DEBUG(num_steps);
            dword axis1 = next_tri_indx[main_axis];
            dword axis2 = next_tri_indx[axis1];

                              //step all
            while(true){
                              //do the test on given cell
               S_vector_i i_pos;
               i_pos.Floor(pos);
               tester.TestDynamicCell(i_pos);
               if(!--num_steps)
                  break;
                              //quit ASAP collision detected
               if(cd.GetHitFrm()) break;
               {
                              //check if we migrate to neighbour at boundary
                  S_vector bound = pos + slope * max_axis_fract;
                  S_vector_i i1;
                  i1.Floor(bound);
                  if(i_pos[axis1]!=i1[axis1] || i_pos[axis2]!=i1[axis2]){
                              //hitting neighbour during step, test it
                     if(i_pos[axis1]!=i1[axis1] && i_pos[axis2]!=i1[axis2]){
                        S_vector_i i2 = i_pos;
                        i2[axis1] = i1[axis1];
                        tester.TestDynamicCell(i2);
                        i2[axis1] = i_pos[axis1];
                        i2[axis2] = i1[axis2];
                        tester.TestDynamicCell(i2);
                     }
                     i1[main_axis] = i_pos[main_axis];
                     tester.TestDynamicCell(i1);
                     i_pos = i1;
                     if(cd.GetHitFrm()) break;
                  }
               }
               if(num_steps>1){
                  pos += slope;
               }else{
                              //for last step, end up exactly at dest position
                  pos = (cd.from+cd.dir) * r_cell_size;
               }
                              //check where we migrated
               S_vector_i i1;
               i1.Floor(pos);
               if(i_pos[axis1]!=i1[axis1] || i_pos[axis2]!=i1[axis2]){
                  if(i_pos[axis1]!=i1[axis1] && i_pos[axis2]!=i1[axis2]){
                              //both changed, make extra 2 cells
                     S_vector_i i2 = i1;
                     i2[axis1] = i_pos[axis1];
                     tester.TestDynamicCell(i2);
                     i2[axis1] = i1[axis1];
                     i2[axis2] = i_pos[axis2];
                     tester.TestDynamicCell(i2);
                  }
                  i_pos[main_axis] = i1[main_axis];
                  tester.TestDynamicCell(i_pos);
                  if(cd.GetHitFrm()) break;
               }
            }
         }
#if defined _DEBUG && 0
         tester.DumpStats(drv);
#endif
#endif
      }
   }

//----------------------------

   bool TestColStaticSphereDynamic(I3D_collision_data &cd) const{

      if(dirty_volumes)
         BuildDynamicVolTree();

      class C_tester: public C_dyn_vol_tree_tester{
         const I3D_collision_data &cd;
         mutable I3D_cresp_data rd;
         mutable bool any_col;
      public:
         C_tester(C_dynamic_collision_info &dc, t_debug_cells *deb, const I3D_collision_data &cd1):
            C_dyn_vol_tree_tester(dc, deb, cd1.collide_bits),
            any_col(false),
            cd(cd1)
         {
            rd.CopyInputValues(cd);
         }

         virtual void DoTest(PI3D_volume vol) const{

            bool col = false;
                              //fast bounding-sphere test first
            const I3D_bsphere &bs = vol->GetBoundSphere();
            float d2 = (bs.pos - cd.from).Square();
            float r_sum = cd.radius + bs.radius;
            if(d2 >= r_sum*r_sum)
               return;

            switch(vol->GetVolumeType1()){
            case I3DVOLUME_SPHERE:
               col = true;
               break;

            case I3DVOLUME_CYLINDER:
                                    //ignore for now
               break;

            case I3DVOLUME_CAPCYL:
            case I3DVOLUME_BOX:
            case I3DVOLUME_RECTANGLE:
               col = vol->IsSphereStaticCollision(cd.from, cd.radius);
               break;

            default: assert(0);
            }
            if(col){
                              //do callback
               if(cd.callback){
                  rd.SetReturnValues(0, vol, S_vector(0, 0, 0));
                  if(!cd.callback(rd))
                     col = false;
               }
            }
            any_col |= col;
         }

      //----------------------------
         inline bool IsAnyCol() const{ return any_col; }

      } tester(dyn_cols,
         (drv->GetFlags2()&DRVF2_DRAWCOLTESTS) ? &debug_cells : NULL, cd);
      tester.frm_ignore = cd.frm_ignore;
                              //countruct AABB of line's influence
      I3D_bbox &bb = tester.test_bounds;
      bb.min = cd.from;
      bb.max = bb.min;
                              //enlarge bounds by moving sphere radius
      float r = cd.radius;
      bb.min -= S_vector(r, r, r);
      bb.max += S_vector(r, r, r);
                              //do actual testing
      tester.Test();

      return tester.IsAnyCol();
   }

//----------------------------
// Refresh dynamic volume tree info - update dirty volumes, make tree.
   virtual void BuildDynamicVolTree() const{

      if(!dirty_volumes)
         return;

      scn_flags |= SCNF_IN_DYN_TREE_UPDATE;

      typedef C_dynamic_collision_info::C_octtree_level t_level;

                                 //update the oct-tree info

                                 //determine maxmimum level first
      int max_level = -1;
      for(I3D_volume::S_dirty_node *it=dirty_volumes; it; ){
         PI3D_volume vol = it->this_vol;
         it = ++*it;

         RemoveFromDynamicVolume(vol, true);
         bool ok = false;
         if(vol->GetVolumeType1())
         if(!(vol->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION) || vol->IsSphericalType()){
                                 //check if in the hierarchy, and is on
            for(CPI3D_frame p=vol; p; p=p->GetParent1()){
               if(!p->IsOn1())
                  break;
               if(p==primary_sector){
                  vol->Prepare();
                  int new_level = dyn_cols.GetLevel(vol);
                  max_level = Max(max_level, new_level);
                  ok = true;
                  break;
               }
            }
         }
         if(!ok)
            vol->dirty_node.RemoveFromList();
      }
      for(dword i=dyn_cols.levels.size(); i--; ){
         if(dyn_cols.levels[i]->num_volumes)
            break;
      }
      i = Max(i+1, (dword)max_level+1);
      i = Max(i, (dword)MIN_DYNAMIC_LEVELS);
      i = Min(i, (dword)MAX_DYNAMIC_LEVELS+1);
      if(i!=dyn_cols.levels.size()){
                                 //resize - from top-level, add all volumes among dirty, and remove them
         if(dyn_cols.levels.size()){
            C_dynamic_collision_info::C_octtree_level &l = *dyn_cols.levels.back();
            set<PI3D_volume> vols;

            for(int j=l.hash_tab_size; j--; ){
               for(t_cell *cp = l.hash_table[j]; cp; cp = ++*cp)
                  vols.insert(cp->vol);
            }
            for(set<PI3D_volume>::iterator it=vols.begin(); it!=vols.end(); it++){
               PI3D_volume vol = *it;
               RemoveFromDynamicVolume(vol, true);
               dirty_volumes.Add(&vol->dirty_node);
               /*
#ifdef _DEBUG
               dword new_level = dyn_cols.GetLevel(vol);
               assert(new_level < i);
#endif
               */
            }
#if defined _DEBUG && 0
                                 //validity check - make sure the top level is clean
            for(j=l.HAS_TAB_SIZE; j--; ){
               assert(!l.hash_table[j]);
            }
            assert(!l.num_volumes);
#endif
         }
         dyn_cols.SetNumLevels(i);
      }

                                 //pass 2 - update dirty volumes
      for(it=dirty_volumes; it; ){
         PI3D_volume vol = it->this_vol;
         I3D_volume::S_dirty_node *it1 = it;
         it = ++*it;
         it1->RemoveFromList();
                                 //make sure there's reason for the volume to be in the list
         //assert(!(vol->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION) || vol->IsSphericalType() || curr_level!=0x80000000);

         if(!(vol->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION) || vol->IsSphericalType()){
                                 //add to levels now
            dword new_level = dyn_cols.GetLevel(vol);
            new_level = Min(new_level, (dword)MAX_DYNAMIC_LEVELS);
            assert(new_level < dyn_cols.levels.size());

            vol->dyn_cells.assign(dyn_cols.levels.size() - new_level);
                                 //compute cell-based bounds of the volume
            S_dyn_vol_bound bound;

            S_vector p_min, p_max;

            switch(vol->GetVolumeType1()){
            case I3DVOLUME_BOX:
            case I3DVOLUME_RECTANGLE:
               {
                  const I3D_bbox &bb1 = vol->GetBoundBox();
                  const S_vector *n = vol->GetNormals();
                  const float *sz = vol->GetWorldSize();
                  p_min = S_vector(1e+16f, 1e+16f, 1e+16f);
                  p_max = S_vector(-1e+16f, -1e+16f, -1e+16f);
                  for(int i=0; i<8; i++){
                     S_vector v = bb1.min;
                     if(i&1) v += n[0]*sz[0];
                     if(i&2) v += n[1]*sz[1];
                     if(i&4) v += n[2]*sz[2];
                     p_min.Minimal(v);
                     p_max.Maximal(v);
                  }
               }
               break;
            default:
               {
                  const I3D_bsphere &bs_vol = vol->GetBoundSphere();
                  p_min = bs_vol.pos - S_vector(bs_vol.radius, bs_vol.radius, bs_vol.radius);
                  p_max = bs_vol.pos + S_vector(bs_vol.radius, bs_vol.radius, bs_vol.radius);
               }
            }
            const float r_cell_size = dyn_cols.GetCellInvSize(new_level);
            bound.min.Floor(p_min * r_cell_size);
            bound.max.Floor(p_max * r_cell_size);

            ++dyn_cols.levels[new_level]->num_volumes;
            const dword num_levels = dyn_cols.levels.size();
                                 //put the volume into this level and all higher levels
            for(i=new_level; ; ){
               t_level &l = *dyn_cols.levels[i];
               C_buffer<t_cell> &vol_cells = vol->dyn_cells[i-new_level];
                              //alloc the vol's cell info
                              // alloc minimally 8 cells, to save from re-allocations 
               dword num_cells = (bound.max[0]-bound.min[0]+1) * (bound.max[1]-bound.min[1]+1) * (bound.max[2]-bound.min[2]+1);
               vol_cells.assign(Max(8ul, num_cells));
               t_cell *cp = vol_cells.begin();
                              //add to all (1, 2, 4 or 8) cells in the level
               for(int x=bound.min.x; x <= bound.max.x; x++){
                  for(int y=bound.min.y; y <= bound.max.y; y++){
                     for(int z=bound.min.z; z <= bound.max.z; z++){
                        C_link_list<t_cell> &cell_list = l.GetCell(x, y, z);
                        cell_list.Add(cp);
                        cp->vol = vol;
                        cp->pos.x = x;
                        cp->pos.y = y;
                        cp->pos.z = z;
                        ++cp;
                     }
                  }
               }
               if(++i >= num_levels)
                  break;
               bound.Shrink();
            }
            vol->curr_octtree_level = new_level;
         }else{
            vol->dyn_cells.clear();
         }
      }
      assert(!dirty_volumes);
      scn_flags &= ~SCNF_IN_DYN_TREE_UPDATE;

#if defined _DEBUG && 0
      {
         for(int i=dyn_cols.levels.size(); i--; ){
            const t_level &l = dyn_cols.levels[i];
            for(int j=t_level::HAS_TAB_SIZE; j--; ){
               for(const t_cell *cp=l.hash_table[j]; cp; cp = ++*cp){
                  CPI3D_volume vol = cp->vol;
                  assert(vol->curr_octtree_level <= i);
               }
            }
         }
      }
#endif
   }


//----------------------------
// Remove volume from dynamic tree - forget all references.
// If 'smart' is true, no memory de-allocation will happen, only linked list of cells will be updated.
   virtual void RemoveFromDynamicVolume(PI3D_volume vol, bool smart) const{

      typedef C_dynamic_collision_info::C_octtree_level t_level;
                                 //remove from current level(s)
      int curr_level = vol->curr_octtree_level;
      if(curr_level!=-1 && dyn_cols.levels.size()){
         assert(dyn_cols.levels[curr_level]->num_volumes);
         --dyn_cols.levels[curr_level]->num_volumes;
         for(int i=0; i<(int)vol->dyn_cells.size(); i++){
            C_buffer<t_cell> &cells = vol->dyn_cells[i];
            dword sz = cells.size();
            for(dword j=0; j<sz; j++){
               t_cell &cell = cells[j];
                              //fast quit, because we alloc 8 cells, but usually only 1, 2, or 4 may be used
               if(!cell.IsInList())
                  break;
               cell.RemoveFromList();
            }
         }
         vol->curr_octtree_level = -1;
                                 //make sure no level references this volume now
#if defined _DEBUG && 0
         {
            for(int i=dyn_cols.levels.size(); i--; ){
               const t_level &l = dyn_cols.levels[i];
               for(int j=t_level::HAS_TAB_SIZE; j--; ){
                  for(const t_cell *cp=l.hash_table[j]; cp; cp = ++*cp){
                     assert(cp->vol != vol);
                  }
               }
            }
         }
#endif
      }
      if(!smart){
                              //also forget 'dirty' volume
         vol->dirty_node.RemoveFromList();
         vol->dyn_cells.clear();
      }
   }

//----------------------------
// Draw single cell of the dynamic-volume oct-tree.
   void DebugDrawDynVolCell(dword level, const S_vector_i &pos, dword color) const{

      static const word indx[] = {
         0,1, 4,5, 2,3, 6,7, 0,2, 1,3, 4,6, 5,7, 0,4, 1,5, 2,6, 3,7
      };
      const float cell_size = 1.0f/DYNAMIC_CELL_SCALE * float(1<<level);
                                 //draw box in the cell
      S_vector vv[8];
      for(int i=0; i<8; i++){
         S_vector &v = vv[i];
         v = S_vector(pos.x*cell_size, pos.y*cell_size, pos.z*cell_size);
         if(i&1) v.x += cell_size;
         if(i&2) v.y += cell_size;
         if(i&4) v.z += cell_size;
      }
      DrawLines(vv, 8, indx, 24, color);
   }

//----------------------------
// Visualize info about dynamic collisions.
   virtual void DebugDrawDynCols(){

      static const dword colors[] = {
         0x000000, 0x0000ff, 0x00c000, 0x008080,
         0x800000, 0x800080, 0x808000, 0x555555,
      };

      if(drv->GetFlags()&DRVF_DEBUGDRAWDYNAMIC){
         SetRenderMatrix(I3DGetIdentityMatrix());
                                 //draw all levels
         for(int i=dyn_cols.levels.size(); i--; ){
            const C_dynamic_collision_info::C_octtree_level &l = *dyn_cols.levels[i];
            dword cell_color = colors[i&7];

            for(int j=l.hash_tab_size; j--; ){
               for(const t_cell *cp = l.hash_table[j]; cp; cp = ++*cp){
                  CPI3D_volume vol = cp->vol;
                  bool this_vol_level = (vol->curr_octtree_level==i);
                  dword color = cell_color;
                  if(this_vol_level)
                     color |= 0xc0000000;
                  else
                     color |= 0x40000000;
                  DebugDrawDynVolCell(i, cp->pos, color);
               }
            }
         }
      }
      if(debug_cells.size()){
         SetRenderMatrix(I3DGetIdentityMatrix());
                              //draw 'triggered' cells
         for(set<S_debug_cell>::const_iterator it=debug_cells.begin(); it!=debug_cells.end(); it++){
            const S_debug_cell &c = *it;
            DebugDrawDynVolCell(c.level, c.pos, c.color);
         }
         debug_cells.clear();
      }
   }

//----------------------------
// Put volume among 'dirty' volumes, and set flag. Update will happen before actual collision testing.
   virtual void UpdateDynamicVolume(PI3D_volume vol) const{

                                 //make sure only dynamic volumes are passed here
      //assert(!(vol->GetFrameFlags()&I3D_FRMF_STATIC_COLLISION) || vol->IsSphericalType());

      if(scn_flags&SCNF_IN_DYN_TREE_UPDATE)
         return;
      dirty_volumes.Add(&vol->dirty_node);
   }

//----------------------------
//Perform ray collision test directly on geometry - higher bounding tests are assumed to be already done.
   static void TestLineAgainstMesh(PI3D_visual vis, PI3D_frame pick_frm, PI3D_mesh_base mb, I3D_collision_data &cd){

      bool collision_ok = false;
      S_vector loc_hit_normal;   //collision normal in visual's local coords
      bool loc_hit_normal_inv = false;
                              
      int num_fgroups = mb->NumFGroups1();
      if(!num_fgroups)
         return;
   
      const S_matrix &matrix = vis->matrix;
      const S_matrix &m_inv = vis->GetInvMatrix1();

      int hit_face_index = -1;

                                 //inverse-map vectors to local coords
      S_vector loc_from = cd.from * m_inv;
      S_vector loc_to = (cd.from + cd.GetNormalizedDir() * cd.GetHitDistance()) * m_inv;
      S_vector loc_dir_normalized = loc_to - loc_from;
      float loc_hit_dist;
      if(!cd.GetHitFrm() && cd.GetHitDistance() >= 1e+16f){
         loc_hit_dist = 1e+16f;
         loc_dir_normalized.Normalize();
      }else{
         loc_hit_dist = loc_dir_normalized.Magnitude();
         if(IsMrgZeroLess(loc_hit_dist))
            return;
         loc_dir_normalized /= loc_hit_dist;
      }

      const S_vector *pick_verts = (const S_vector*)mb->vertex_buffer.Lock(D3DLOCK_READONLY);
      dword vstride = mb->vertex_buffer.GetSizeOfVertex();
                                 //assumption: texture coordinates are at end of vertex
      const I3D_text_coor *txt_coords = (const I3D_text_coor*)(((byte*)pick_verts) + vstride - sizeof(I3D_text_coor));

#ifndef USE_STRIPS
      CPI3D_triface faces_base = mb->GetIndexBuffer().LockForReadOnly(), faces = faces_base;
#else
      const word *indicies_base = (word*)mb->GetIndexBuffer().LockForReadOnly();
      CPI3D_triface faces = (CPI3D_triface)indicies_base;
#endif

      I3D_texel_collision tc;
      tc.loc_pos = true;
                                 //check all face groups
      const I3D_face_group *fgrps = mb->GetFGroups1();
      for(int ii=0; ii<num_fgroups; ii++){
         const I3D_face_group &fg = fgrps[ii];
         tc.mat = fg.GetMaterial1();
                                 //check all faces in group
         bool two_side_mat = (tc.mat->Is2Sided1() || (cd.flags&I3DCOL_FORCE2SIDE));

         dword num_fg_faces = fg.NumFaces1();
#ifdef USE_STRIPS
         C_destripifier destrip;
         I3D_triface fc;
         bool has_strips = mb->HasStrips();
         if(has_strips){
            const S_fgroup_strip_info &si = mb->GetStripInfo()[ii];
            num_fg_faces = si.num_indicies - 2;
            destrip.Begin(indicies_base + si.base_index);
         }
#endif
         for(dword jj = 0; jj < num_fg_faces; jj++){
            bool invert_side = false;

#ifndef USE_STRIPS
            const I3D_triface &fc = *faces++;
#else
            if(has_strips){
               destrip.Next();

                              //ignore degenerate strip faces
               if(destrip.IsDegenerate())
                  continue;
               fc = destrip.GetFace();
            }else{
               fc = *faces++;
            }
#endif
            tc.v[0] = (const S_vector*)(((byte*)pick_verts) + fc[0] * vstride);
            tc.v[1] = (const S_vector*)(((byte*)pick_verts) + fc[1] * vstride);
            tc.v[2] = (const S_vector*)(((byte*)pick_verts) + fc[2] * vstride);
            S_vector pl_normal;
            pl_normal.GetNormal(*tc.v[0], *tc.v[1], *tc.v[2]);
                                 //don't normalize - it's working also without that
            //pl_normal.Normalize();
            float u = -pl_normal.Dot(loc_dir_normalized);
            if(IsMrgZeroLess(u)){
               if(!two_side_mat)
                  continue;
               if(IsAbsMrgZero(u))
                  continue;
               invert_side = true;
            }
            float pl_d = -(*tc.v[0]).Dot(pl_normal);

            float f_dist = loc_from.x*pl_normal.x + loc_from.y*pl_normal.y + loc_from.z*pl_normal.z + pl_d;
            L_F_DEBUG(C_fstr("distance of 'from' of plane: %.2f", f_dist));
            float curr_dist = - f_dist / -u;
            L_F_DEBUG(C_fstr("point on plane at length %.2f", curr_dist));
                                 //don't look back
            if(CHECK_ZERO_LESS(curr_dist)){
               continue;
            }
                                 //reject more far collisions than
                                 //what we already have
            if(FLOAT_BITMASK(loc_hit_dist) <= FLOAT_BITMASK(curr_dist)){
               L_F_DEBUG("Fast quit 3 - line segment off the plane");
               continue;
            }
            tc.point_on_plane = loc_from + loc_dir_normalized * curr_dist;

            if(!IsPointInTriangle(tc.point_on_plane, *tc.v[0], *tc.v[1], *tc.v[2], pl_normal))
               continue;

            //PI3D_driver drv = pick_frm->GetDriver1();
                              //face collision succeeded
            if(cd.flags&I3DCOL_COLORKEY){
                                 //detailed texture check
               CPI3D_texture_base tb = tc.mat->GetTexture1();
               if(tb && (tc.mat->I3D_material::IsCkey() || tc.mat->IsCkeyAlpha1() || cd.texel_callback)){
                  const I3D_text_coor &uv0 = *(const I3D_text_coor*)(((byte*)txt_coords) + fc[0] * vstride);
                  const I3D_text_coor &uv1 = *(const I3D_text_coor*)(((byte*)txt_coords) + fc[1] * vstride);
                  const I3D_text_coor &uv2 = *(const I3D_text_coor*)(((byte*)txt_coords) + fc[2] * vstride);

                  if(tc.ComputeUVFromPoint(uv0, uv1, uv2)){

                     if(tc.mat->I3D_material::IsCkey() || tc.mat->IsCkeyAlpha1()){
                                 //check color-keyed texels
                        const byte *rle = tb->GetRLEMask();
                        assert(rle);
                        dword sx = tb->SizeX1(), sy = tb->SizeY1();
                        int x = Min((int)(sx * tc.tex.x), (int)sx-1);
                        int y = Min((int)(sy * tc.tex.y), (int)sy-1);
                        //drv->DEBUG(C_fstr("pick:  %.2f     %.2f", tex.u, tex.v));
                        const byte *mem = &rle[*(dword*)(&rle[y*4])];
                        bool full = false;
                        do{
                           byte b = *mem++;
                           if(!b){     //eol
                              full = false;
                              break;
                           }
                           full = (b&0x80);
                           x -= (b&0x7f);
                        }while(x>=0);
                        if(!full)
                           continue;
                     }
                                                //ask user if we furher process this
                     if(cd.texel_callback){
                        //tc.face_index = word(faces - faces_base);
                        tc.face_index = fg.base_index + jj;
                                             //due to this, we need to put hit data
                                             // into world coords,
                                             // performance is not critical, because this
                                             // feature is expected to be called bu light-mapping only
                        S_vector hit_normal = pl_normal % matrix;
                        if(invert_side)
                           hit_normal.Invert();

                        S_vector col_pt = (loc_from + loc_dir_normalized * curr_dist) * matrix;
                        float hit_dist = (col_pt - cd.from).Magnitude();

                        I3D_cresp_data rd(pick_frm, hit_normal, hit_dist, cd.cresp_context);
                        rd.CopyInputValues(cd);
                        rd.texel_data = &tc;
                        rd.face_index = fg.base_index + jj;
                        if(!cd.texel_callback(rd)){
                           mb->vertex_buffer.Unlock();
                           mb->GetIndexBuffer().Unlock();
                           return;
                        }
                     }
                  }
               }
            }
                                 //collision detected on this face, save data
            loc_hit_dist = curr_dist;
            collision_ok = true;
            loc_hit_normal = pl_normal;
            loc_hit_normal_inv = invert_side;
            hit_face_index = fg.base_index + jj;
         }
      }
      mb->vertex_buffer.Unlock();
      mb->GetIndexBuffer().Unlock();

      if(collision_ok){
         S_vector hit_normal = loc_hit_normal.RotateByMatrix(matrix);
         if(loc_hit_normal_inv)
            hit_normal.Invert();

         S_vector col_pt = (loc_from + loc_dir_normalized * loc_hit_dist) * matrix;
         float hit_dist = (col_pt - cd.from).Magnitude();
                                 //check if we want this
         if(cd.callback){
            I3D_cresp_data rd(pick_frm, hit_normal, hit_dist, cd.cresp_context);
            rd.face_index = hit_face_index;
            rd.CopyInputValues(cd);
            if(!cd.callback(rd))
               return;
         }
                                 //map collision distance and normal back to world coords
         cd.SetReturnValues(hit_dist, pick_frm, hit_normal);
         cd.face_index = hit_face_index;
         assert(hit_dist>=0.0f);
      }
   }

//----------------------------
// Enumeration function for recursive test on single-mesh joints.
   static I3DENUMRET I3DAPI cbEnumSMJoints(PI3D_frame frm, dword dw_cd){

      I3D_collision_data &cd = *(I3D_collision_data*) dw_cd;
      PI3D_joint jnt = I3DCAST_JOINT(frm);
      if(!jnt->IsVolumeBoxValid())
         return I3DENUMRET_OK;

#ifdef USE_CAPCYL_JNT_VOLUMES
      const S_matrix &m = jnt->GetMatrix();
      float scl = m(0).Magnitude();
      S_vector z_axis = jnt->GetVolumeDir() % m;
      if(!IsAbsMrgZero(1.0f-scl))
         z_axis.Normalize();
      TestColLineCylinder(jnt->GetVolumePos()*m, z_axis, jnt->GetVolumeRadius()*scl, jnt->GetVolumeHalfLen()*scl, jnt, cd, true);
#else

                                 //make test on bounding-box
      const I3D_bbox &bbox = jnt->GetVolumeBox1();
      const S_matrix &mat = jnt->GetVolumeBoxMatrix1();

      float hit_dist;
      bool b = pickBBoxTest(mat * jnt->GetMatrix(), bbox, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance(), &hit_dist);
      if(b){
                                 //we don't compute exact normal, just return opposite of direction vector
         const S_vector hit_norm = -cd.dir;
                                 //call-back user
         if(cd.callback){
            I3D_cresp_data rd(frm, hit_norm, hit_dist, cd.cresp_context);
            rd.CopyInputValues(cd);
            if(!cd.callback(rd)){
               return I3DENUMRET_OK;
            }
         }
         cd.SetReturnValues(hit_dist, frm, hit_norm);
      }
#endif
      return I3DENUMRET_OK;
   }

//----------------------------
// Recursive function for testling line collision against raw geometry.
   static I3DENUMRET cbGeoCol(PI3D_frame root, I3D_collision_data &cd){

      const C_vector<PI3D_frame> &children = root->GetChildren1();

                                 //call on all children of root
      const PI3D_frame *child_p = &children.front();
      for(int i=children.size(); i--; ){
         PI3D_frame frm = child_p[i];

         if(frm->GetType1()!=FRAME_VISUAL){

            switch(frm->GetType1()){
            case FRAME_MODEL:
               if((!(cd.flags&I3DCOL_INVISIBLE) && !frm->IsOn1()) ||
                  (frm->GetFrameFlags()&I3D_FRMF_NOCOLLISION))
                  continue;

                                 //hierarchy test on model
               if(frm->num_hr_vis){
                  PI3D_model mod = I3DCAST_MODEL(frm);
                  if(!(frm->GetFrameFlags()&FRMFLAGS_HR_BOUND_VALID))
                     mod->hr_bound.MakeHierarchyBounds(frm);
                  bool b = pickBSphereTest(mod->hr_bound.GetBoundSphereTrans(frm, FRMFLAGS_HR_BSPHERE_VALID), cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                  if(b)
                     b = pickBBoxTest(mod->GetMatrix(), mod->hr_bound.bound_local.bbox, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                  if(!b) continue;
               }
               break;
            case FRAME_DUMMY:
               if((!(cd.flags&I3DCOL_INVISIBLE) && !frm->IsOn1()) ||
                  (frm->GetFrameFlags()&I3D_FRMF_NOCOLLISION))
                  continue;
               break;
            }
         }else{
            if((!(cd.flags&I3DCOL_INVISIBLE) && !frm->IsOn1()) ||
               (frm->GetFrameFlags()&I3D_FRMF_NOCOLLISION))
               continue;

            PI3D_visual vis = I3DCAST_VISUAL(frm);

            switch(vis->GetVisualType1()){
            case I3D_VISUAL_BILLBOARD:
               break;
                                 //flow...
            default:
               {
                  PI3D_mesh_base mb = vis->GetMesh();
                  if(!mb)
                     break;
                                 //make sure all matrices are computed
                  const S_matrix &matrix = frm->GetMatrix();
                                 //hierarchy bound test
                  if(frm->num_hr_vis){
                     if(!(frm->GetFrameFlags()&FRMFLAGS_HR_BOUND_VALID))
                        vis->hr_bound.MakeHierarchyBounds(vis);

                     bool b = pickBSphereTest(vis->hr_bound.GetBoundSphereTrans(vis, FRMFLAGS_HR_BSPHERE_VALID), cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                     if(b)
                        b = pickBBoxTest(matrix, vis->hr_bound.bound_local.bbox, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                     if(!b)
                        continue;
                  }
                  bool b = true;
                  {
                                    //perform check on bounding volume
                     if(!(vis->vis_flags&VISF_BOUNDS_VALID))
                        vis->ComputeBounds();
                     b = pickBSphereTest(vis->bound.GetBoundSphereTrans(vis, FRMFLAGS_BSPHERE_TRANS_VALID), cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                     if(b)
                        b = pickBBoxTest(vis->matrix, mb->GetBoundingVolume().bbox, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance());
                  }
                  if(b){
                     TestLineAgainstMesh(vis, vis, mb, cd);
                  }
               }
               break;

            case I3D_VISUAL_SINGLEMESH:
               {
                  I3D_object_singlemesh *sm = (I3D_object_singlemesh*)vis;
                  if(sm->IsVolumeBoxValid()){
#ifdef USE_CAPCYL_JNT_VOLUMES
                     const S_matrix &m = sm->GetMatrix();
                     float scl = m(0).Magnitude();
                     S_vector z_axis = sm->GetVolumeDir() % m;
                     if(!IsAbsMrgZero(1.0f-scl))
                        z_axis.Normalize();

                     TestColLineCylinder(sm->GetVolumePos()*m, z_axis, sm->GetVolumeRadius()*scl, sm->GetVolumeHalfLen()*scl, sm, cd, true);
#else
                                 //make test on bounding-box
                     const I3D_bbox &bbox = sm->GetVolumeBox1();
                     const S_matrix &mat = sm->GetVolumeBoxMatrix1();

                     float hit_dist;
                     bool b = pickBBoxTest(mat * vis->GetMatrix(), bbox, cd.from, cd.GetNormalizedDir(), cd.GetHitDistance(), &hit_dist);
                     if(b){
                                 //we don't compute exact normal, just return opposite of direction vector
                        const S_vector hit_norm = -cd.dir;
                                 //call-back user
                        if(cd.callback){
                           I3D_cresp_data rd(frm, hit_norm, hit_dist, cd.cresp_context);
                           rd.CopyInputValues(cd);
                           if(cd.callback(rd))
                              cd.SetReturnValues(hit_dist, frm, hit_norm);
                        }else{
                           cd.SetReturnValues(hit_dist, frm, hit_norm);
                        }
                     }
#endif
                  }
                  vis->EnumFrames(cbEnumSMJoints, (dword)&cd, ENUMF_JOINT);
               }
               break;
            }
         }
                                 //repeat on children
         if(frm->num_hr_vis)
            cbGeoCol(frm, cd);
      }
      return I3DENUMRET_OK;
   }

//----------------------------
// Test of line against raw geometry.
   void TestColGeo(I3D_collision_data &cd) const{

      if(drv->GetFlags()&DRVF_DRAWVISUALS)
         cbGeoCol(primary_sector, cd);
   }

//----------------------------

   void GenerateContactsOnDynVols(const I3D_contact_data &cd) const{

      class C_tester: public C_dyn_vol_tree_tester{
         const I3D_contact_data &cd;
      public:
         C_tester(C_dynamic_collision_info &dc, t_debug_cells *deb, const I3D_contact_data &cd1):
            C_dyn_vol_tree_tester(dc, deb, cd1.vol_src->GetCollideCategoryBits()>>16),
            cd(cd1)
         {}

         virtual void DoTest(PI3D_volume vol) const{

                              //fast bounding-sphere test first
            if(!SphereCollide(vol->GetBoundSphere(), cd.vol_src->GetBoundSphere()))
               return;

            switch(vol->GetVolumeType1()){
            case I3DVOLUME_SPHERE:
               switch(cd.vol_src->GetVolumeType1()){
               case I3DVOLUME_SPHERE: vol->ContactSphereSphere(cd); break;
               case I3DVOLUME_CAPCYL: cd.vol_src->ContactCylinderSphere(cd, vol, true); break;
               case I3DVOLUME_BOX: cd.vol_src->ContactBoxSphere(cd, vol, true); break;
               default: assert(0);
               }
               break;

            case I3DVOLUME_CAPCYL:
               switch(cd.vol_src->GetVolumeType1()){
               case I3DVOLUME_SPHERE: vol->ContactCylinderSphere(cd, cd.vol_src, false); break;
               case I3DVOLUME_CAPCYL: vol->ContactCylinderCylinder(cd); break;
               case I3DVOLUME_BOX: cd.vol_src->ContactBoxCylinder(cd, vol, true); break;
               default: assert(0);
               }
               break;

            case I3DVOLUME_BOX:
               switch(cd.vol_src->GetVolumeType1()){
               case I3DVOLUME_SPHERE: vol->ContactBoxSphere(cd, cd.vol_src, false); break;
               case I3DVOLUME_CAPCYL: vol->ContactBoxCylinder(cd, cd.vol_src, false); break;
               case I3DVOLUME_BOX: vol->ContactBoxBox(cd); break;
               default: assert(0);
               }
               break;

            case I3DVOLUME_RECTANGLE:
               switch(cd.vol_src->GetVolumeType1()){
               case I3DVOLUME_SPHERE: vol->ContactBoxSphere(cd, cd.vol_src, false); break;
               case I3DVOLUME_BOX: vol->ContactBoxBox(cd); break;
               default: assert(0);
               }
               break;

            default: assert(0);
            }
         }
      } tester(dyn_cols,
         (drv->GetFlags2()&DRVF2_DRAWCOLTESTS) ? &debug_cells : NULL, cd);
      tester.frm_ignore = cd.frm_ignore;
      tester.frm_ignore1 = cd.vol_src;
                              //countruct AABB of volume
      I3D_bbox &bb = tester.test_bounds;
      switch(cd.vol_src->GetVolumeType1()){
      case I3DVOLUME_SPHERE:
      case I3DVOLUME_CYLINDER:
      case I3DVOLUME_CAPCYL:
         {
            const I3D_bsphere &bs = cd.vol_src->GetBoundSphere();
            bb.min = bs.pos;
            bb.max = bs.pos;
            float r = bs.radius;
            bb.min -= S_vector(r, r, r);
            bb.max += S_vector(r, r, r);
         }
         break;
      case I3DVOLUME_BOX:
      case I3DVOLUME_RECTANGLE:
         {
            const float *sz = cd.vol_src->GetWorldSize();
            const I3D_bbox &bb1 = cd.vol_src->GetBoundBox();
            const S_vector *n = cd.vol_src->GetNormals();
            bb.Invalidate();
            for(int i=0; i<8; i++){
               S_vector v = bb1.min;
               if(i&1) v += n[0]*sz[0];
               if(i&2) v += n[1]*sz[1];
               if(i&4) v += n[2]*sz[2];
               bb.min.Minimal(v);
               bb.max.Maximal(v);
            }
         }
         break;
      default: assert(0);
      }
                              //do actual testing
      tester.Test();
   }

//----------------------------

public:
   I3D_scene_imp(PI3D_driver d):
      I3D_scene(d)
   {
      memset(&col_stats, 0, sizeof(col_stats));
      memset(&voltree_stats, 0, sizeof(voltree_stats));
      drv->RegResetCallback(this);
   }
   ~I3D_scene_imp(){
      drv->UnregResetCallback(this);
   }

   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){
      if(--ref){
         if(ref==xref){
            ++ref;
            Close();
            if(!--ref)
               delete this;
         }
         return ref;
      }
      delete this;
      return 0;
   }

//----------------------------
// Test collision - main exported function.
   I3DMETHOD_(bool,TestCollision)(I3D_collision_data &cd) const{

      PROFILE(drv, PROF_COLLISIONS);

      dword flags = cd.flags;
      dword mode = flags & 0xffff;

                                 //validity check input flags

      if(!mode || mode >= I3DCOL_MODE_LAST){
         assert(("invalid mode", 0));
         throw C_except("I3D_scene::TestCollision: invalid mode - make sure lower 16 bits contain valid mode, do not combine modes");
         return false;
      }
      if(flags&I3DCOL_RAY){
                                 //ray allowed on line only
         switch(mode){
         case 0:
            /*
         case I3DCOL_MOVING_SPHERE:
         case I3DCOL_MOVING_GROUP:
         case I3DCOL_LINE:
         */
         case I3DCOL_EDIT_FRAMES:
         case I3DCOL_NOMESH_VISUALS:
         case I3DCOL_VOLUMES:
         case I3DCOL_EXACT_GEOMETRY:
            break;
         default:
            assert(("I3DCOL_RAY flag allowed only for moving tests", 0));
            throw C_except("I3D_scene::TestCollision: I3DCOL_RAY not allowed for this mode");
            return false;
         }
      }
      if(flags&I3DCOL_JOINT_VOLUMES){
         switch(mode){
         case I3DCOL_LINE:
            break;
         default:
            assert(("I3DCOL_JOINT_VOLUMES flag allowed only for line test", 0));
            throw C_except("I3D_scene::TestCollision: I3DCOL_JOINT_VOLUMES flag allowed only for line test");
         }
      }
      if(flags&I3DCOL_SOLVE_SLIDE){
                                 //solving slide only on moving sphere tests
         switch(mode){
         case I3DCOL_MOVING_SPHERE:
         case I3DCOL_MOVING_GROUP:
            break;
         default:
            assert(("I3DCOL_SOLVE_SLIDE flag allowed only for moving sphere source", 0));
            throw C_except("I3D_scene::TestCollision: I3DCOL_SOLVE_SLIDE may be specified only for moving sphere tests");
            return false;
         }
      }
                                 //init values of cd structure (exit if preparation fails)
      if(!PrepareCollisionTest(cd))
         return false;

                              //recursive call ? (marked_volumes list is non-empty only during next switch statement)
                              // we need to save the list, along with tag, and re-create it after the switch
      C_vector<C_dynamic_collision_info::t_saved_marked_vol> saved_marks;
      if(dyn_cols.marked_volumes)
         dyn_cols.SaveAndClearMargedList(saved_marks);

                                 //perform actual test, depending on requested mode
      switch(mode){

      case I3DCOL_MOVING_SPHERE:
      case I3DCOL_MOVING_GROUP:
         TestColMovingSphere(cd);
         break;

      case I3DCOL_STATIC_SPHERE:
         {
            bool any_col;
            any_col = bsp_tree.SphereStaticIntersection(cd);
            any_col |= TestColStaticSphereDynamic(cd);
         }
         break;

      case I3DCOL_VOLUMES:
         TestColEditVolumes(primary_sector, cd);
         break;

      case I3DCOL_EDIT_FRAMES:
      case I3DCOL_NOMESH_VISUALS:
         TestColEditFrames(cd);
         break;

      case I3DCOL_STATIC_BOX:
         TestColBoxSpheresDyn(cd);
         break;

      case I3DCOL_EXACT_GEOMETRY:
         TestColGeo(cd);
         break;

      case I3DCOL_LINE:
         TestColLineDynamic(cd);
                                 //test joints volumes if requested
         if(cd.flags&I3DCOL_JOINT_VOLUMES){
            TestVolumeSM(cd);
         }
         bsp_tree.RayIntersection(cd);
         break;

      default:
         assert(0);
         throw C_except("invalid mode");
         return false;
      }
      dyn_cols.RestoreMarkedList(saved_marks);
                                 //finalize testing - normalize hit normal (if detected)
      if(cd.hit_frm)
         cd.hit_normal.Normalize();
                                 //return true if detected
      return (cd.hit_frm);
   }

//----------------------------

   I3DMETHOD_(void,GenerateContacts)(const I3D_contact_data &cd) const{

      if(!cd.vol_src)
         return;
                                 //reporting callback must be present
      if(!cd.cb_report){
         return;
      }

                                 //prepare dynamic volume tree
      if(dirty_volumes)
         BuildDynamicVolTree();

                                 //make sure matrix is computed
      cd.vol_src->GetMatrix();

                                 //prepare the source
      cd.vol_src->Prepare();

                              //recursive call ? (marked_volumes list is non-empty only during next switch)
                              // we need to save the list, along with tag, and re-create it after the switch
      C_vector<C_dynamic_collision_info::t_saved_marked_vol> saved_marks;
      if(dyn_cols.marked_volumes)
         dyn_cols.SaveAndClearMargedList(saved_marks);

                                 //first check static spheres
      GenerateContactsOnDynVols(cd);

                              //now check against BSP
      bsp_tree.GenerateContacts(cd, drv);

      dyn_cols.RestoreMarkedList(saved_marks);
   }

//----------------------------

   I3DMETHOD(GetStats)(I3DSTATSINDEX indx, void *data){

      switch(indx){
      case I3DSTATS_VOLUME:
         {
            PI3D_stats_volume st = (PI3D_stats_volume)data;
            st->sphere_sphere = col_stats[COL_STATS_S_S];
            st->sphere_rectangle = col_stats[COL_STATS_S_R];
            st->sphere_box = col_stats[COL_STATS_S_B];
            st->sphere_face = col_stats[COL_STATS_S_F];
            st->line_sphere = col_stats[COL_STATS_L_S];
            st->line_rectangle = col_stats[COL_STATS_L_R];
            st->line_box = col_stats[COL_STATS_L_B];
            st->line_face = col_stats[COL_STATS_L_F];
            memset(&col_stats, 0, sizeof(col_stats));
         }
         break;

      case I3DSTATS_RENDER:
         {
            PI3D_stats_render st = (PI3D_stats_render)data;
            *st = render_stats;
            memset(&render_stats, 0, sizeof(render_stats));
         }
         break;

      case I3DSTATS_SCENE:
         {
            PI3D_stats_scene st = (PI3D_stats_scene)data;
            *st = scene_stats;
            memset(&scene_stats, 0, sizeof(scene_stats));
         }
         break;

      case I3DSTATS_SOUND:
         {
            PISND_driver lpis = drv->GetSoundInterface();
            PI3D_stats_sound st = (PI3D_stats_sound)data;
            if(lpis){
               set<PISND_source> snd_srcs;
               st->snds_audible = scene_stats.frame_count[FRAME_SOUND];
               scene_stats.frame_count[FRAME_SOUND] = 0;
                                 //count in-card sounds
               st->snds_in_card = 0;
               st->mem_used = 0;
               for(int i=sounds.size(); i--; ){
                  PI3D_sound snd = sounds[i];
                  if(snd->GetVoice()){
                     if(snd->GetSndFlags()&SNDF_HARDWARE)
                        ++st->snds_in_card;
                  }
                  if(snd->GetSoundSource()){
                     if(!snd->IsStreaming())
                        snd_srcs.insert(snd->GetSoundSource());
                     else{
                        const S_wave_format &wf = *snd->GetSoundSource()->GetFormat();
                        st->mem_used += (((int)(wf.samples_per_sec*wf.bytes_per_sample*1.0f)) & -16);
                     }
                  }
               }
                                 //collect size of samples
               for(set<PISND_source>::const_iterator it = snd_srcs.begin(); it!=snd_srcs.end(); ++it){
                  const S_wave_format *wf = (*it)->GetFormat();
                  if(wf)
                     st->mem_used += wf->size;
               }
                                 //get info from sound driver
               dword caps[6];
               lpis->GetCaps(caps);
               st->buf_hw_all = caps[0];
               st->buf_hw_free = caps[1];
               st->buf_3d_all = caps[2];
               st->buf_3d_free = caps[3];
               st->hw_mem_all = caps[4];
               st->hw_mem_free = caps[5];
            }else{
               memset(st, 0, sizeof(I3D_stats_sound));
            }
            st->snds_all = sounds.size();
         }
         break;

      case I3DSTATS_BSP:
         {
            PI3D_stats_bsp st = (PI3D_stats_bsp)data;
            bsp_tree.GetStats(*st);
         }
         break;
      case I3DSTATS_VOLTREE:
         {
            PI3D_stats_voltree st = (PI3D_stats_voltree)data;
            *st = voltree_stats;
            voltree_stats.moving_static = 0;
         }
         break;

      default:
         return I3DERR_INVALIDPARAMS;
      }
      return I3D_OK;
   }
};

//----------------------------
//----------------------------

PI3D_scene CreateScene(PI3D_driver d){
   I3D_scene_imp *sp = new I3D_scene_imp(d);
   sp->Init();
   return sp;
}

//----------------------------
//----------------------------
