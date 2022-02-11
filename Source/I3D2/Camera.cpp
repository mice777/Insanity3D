/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.
   All rights reserved.

   File: Camera.cpp
   Content: Camera frame.
--------------------------------------------------------*/

#include "all.h"
#include "camera.h"
#include "scene.h"
#include "visual.h"
#include "sector.h"

//----------------------------
                              //camera class
I3D_camera::I3D_camera(PI3D_driver d):
   I3D_frame(d),
   fov_a(PI*.5f),
   ncp(1.0f), fcp(1000.0f),
   ortho_scale(1.0f),
   curr_sector(NULL),
   cam_flags(CAMF_RESET_PROJ | CAMF_RESET_VIEW | CAMF_RESET_BIAS)
{
   drv->AddCount(I3D_CLID_CAMERA);
   type = FRAME_CAMERA;
   enum_mask = ENUMF_CAMERA;
   m_view.Identity();
}

//----------------------------

I3D_camera::~I3D_camera(){
   drv->DecCount(I3D_CLID_CAMERA);
   SetCurrSector(NULL);
}

//----------------------------

void I3D_camera::SetCurrSector(PI3D_sector s){
   if(s) s->AddRef();
   if(curr_sector) curr_sector->Release();
   curr_sector = s;
}

//----------------------------

I3D_RESULT I3D_camera::Duplicate(CPI3D_frame frm){

   if(frm->GetType1()==FRAME_CAMERA){
      CPI3D_camera cp = I3DCAST_CCAMERA(frm);
      fov_a = cp->fov_a;
      ncp = cp->ncp;
      fcp = cp->fcp;
      ortho_scale = cp->ortho_scale;
      SetOrthogonal(cp->GetOrthogonal());

      cam_flags |= CAMF_RESET_PROJ | CAMF_RESET_VIEW | CAMF_RESET_BIAS;
   }
   return I3D_frame::Duplicate(frm);
}

//----------------------------

I3D_RESULT I3D_camera::SetFOV(float angle){

   if(IsMrgZeroLess(angle) || angle >= (PI*.99f))
      return I3DERR_INVALIDPARAMS;
   fov_a = angle;
   if(!(cam_flags&CAMF_ORTHOGONAL))
      cam_flags |= (CAMF_RESET_PROJ | CAMF_RESET_BIAS);
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_camera::SetRange(float n, float f){

   if((f-n)<.001f)
      return I3DERR_INVALIDPARAMS;
   ncp = n;
   fcp = f;
   cam_flags |= CAMF_RESET_PROJ | CAMF_RESET_BIAS;
   return I3D_OK;
}

//----------------------------

void I3D_camera::GetRange(float &n, float &f) const{
   n = ncp;
   f = fcp;
}

//----------------------------

const S_matrix &I3D_camera::GetViewMatrix() const{ return GetViewMatrix1(); }

//----------------------------

const S_matrix &I3D_camera::GetProjectionMatrix() const{ return m_proj; }

//----------------------------

I3D_RESULT I3D_camera::SetOrthoScale(float scale){

   if(scale <= 0.0f)
      return I3DERR_INVALIDPARAMS;
   ortho_scale = scale;
   if(cam_flags&CAMF_ORTHOGONAL)
      cam_flags |= CAMF_RESET_PROJ | CAMF_RESET_BIAS;
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_camera::SetOrthogonal(bool b){

   if(b){
      if(!(cam_flags&CAMF_ORTHOGONAL))
         cam_flags |= (CAMF_ORTHOGONAL | CAMF_RESET_PROJ | CAMF_RESET_BIAS);
   }else{
      if(cam_flags&CAMF_ORTHOGONAL){
         cam_flags &= ~CAMF_ORTHOGONAL;
         cam_flags |= CAMF_RESET_PROJ | CAMF_RESET_BIAS;
      }
   }
   return I3D_OK;
}

//----------------------------

void I3D_camera::UpdateCameraMatrices(float aspect_ratio) const{

   if(cam_flags&CAMF_RESET_PROJ){
                              //critical error
      if(IsAbsMrgZero(fov_a))
         return;

      S_matrix &m = m_proj;
      m.Zero();
      float range = fcp - ncp;

      if(!(cam_flags&CAMF_ORTHOGONAL)){
                              //perspective projection matrix
         float fov_a_half = fov_a * .5f;
         float ca = (float)cos(fov_a_half), sa = (float)sin(fov_a_half);
         float w = ca / sa;
         float h = ca / (sa * aspect_ratio);
         float Q = fcp / range;
         m(0, 0) = w;
         m(1, 1) = h;
         m(2, 2) = Q;
         m(2, 3) = 1.0f;
         m(3, 2) = -Q * ncp;
#ifdef GL
         gl_m_proj_simple.m0_0 = w;
         gl_m_proj_simple.m1_1 = h;
         gl_m_proj_simple.m2_2 = -(ncp+fcp)/range;
         gl_m_proj_simple.m3_2 = ncp + fcp*ncp/range;
#endif
      }else{
                              //orthogonal projection matrix
         m(0, 0) = ortho_scale;
         m(1, 1) = ortho_scale / aspect_ratio;
         m(2, 2) = 1.0f / range;
         m(3, 2) = -ncp / range;
         m(3, 3) = 1.0f;
#ifdef GL
         gl_m_proj_simple.m0_0 = ortho_scale;
         gl_m_proj_simple.m1_1 = ortho_scale / aspect_ratio;
         gl_m_proj_simple.m2_2 = 2.0f / fcp;
         gl_m_proj_simple.m3_2 = -fcp/range;
#endif
      }
      m_proj_simple.m0_0 = m(0, 0);
      m_proj_simple.m1_1 = m(1, 1);
      m_proj_simple.m2_2 = m(2, 2);
      m_proj_simple.m3_2 = m(3, 2);
      m_proj_simple.orthogonal = (cam_flags&CAMF_ORTHOGONAL);
#ifdef GL
      gl_m_proj_simple.orthogonal = (cam_flags&CAMF_ORTHOGONAL);
#endif
      cam_flags &= ~CAMF_RESET_PROJ;
   }

                              //compute the biased matrix, if it is dirty
   if(cam_flags&CAMF_RESET_BIAS){

      float range = fcp - ncp;

      float delta = ncp * .005f;
      float n = ncp + delta;
      float f = fcp + delta;

      if(!(cam_flags&CAMF_ORTHOGONAL)){
                              //perspective projection matrix
         float fov_a_half = fov_a * .5f;
         float ca = (float)cos(fov_a_half), sa = (float)sin(fov_a_half);
         float w = ca / sa;
         float h = ca / (sa * aspect_ratio);
         float Q = f / range;
         m_proj_simple_biased.m0_0 = w;
         m_proj_simple_biased.m1_1 = h;
         m_proj_simple_biased.m2_2 = Q;
         m_proj_simple_biased.m3_2 = -Q * n;
#ifdef GL
         gl_m_proj_simple_biased.m0_0 = w;
         gl_m_proj_simple_biased.m1_1 = h;
         gl_m_proj_simple_biased.m2_2 = -(n+f)/range;
         gl_m_proj_simple_biased.m3_2 = n + f*n/range;
#endif
      }else{
                              //orthogonal projection matrix
         m_proj_simple_biased.m0_0 = ortho_scale;
         m_proj_simple_biased.m1_1 = ortho_scale / aspect_ratio;
         m_proj_simple_biased.m2_2 = 1.0f / range;
         m_proj_simple_biased.m3_2 = -n / range;
#ifdef GL
         gl_m_proj_simple_biased.m0_0 = ortho_scale;
         gl_m_proj_simple_biased.m1_1 = ortho_scale / aspect_ratio;
         gl_m_proj_simple_biased.m2_2 = 2.0f / f;
         gl_m_proj_simple_biased.m3_2 = -f/range;
#endif
      }
      m_proj_simple_biased.orthogonal = (cam_flags&CAMF_ORTHOGONAL);
#ifdef GL
      gl_m_proj_simple_biased.orthogonal = (cam_flags&CAMF_ORTHOGONAL);
#endif

      cam_flags &= ~CAMF_RESET_BIAS;
   }

   if((cam_flags&CAMF_RESET_VIEW) || ChangedMatrix()){
      const S_matrix &mat = GetMatrix();
                              //direct inverse frame matrix
      m_view(0, 0) = mat(0, 0);
      m_view(0, 1) = mat(1, 0);
      m_view(0, 2) = mat(2, 0);

      m_view(1, 0) = mat(0, 1);
      m_view(1, 1) = mat(1, 1);
      m_view(1, 2) = mat(2, 1);

      m_view(2, 0) = mat(0, 2);
      m_view(2, 1) = mat(1, 2);
      m_view(2, 2) = mat(2, 2);

      float scl = mat(0).Square();
      if(!IsAbsMrgZero(1.0f-scl)){
         scl = 1.0f / I3DSqrt(scl);
         m_view(0) *= scl;
         m_view(1) *= scl;
         m_view(2) *= scl;
      }
                              //rotate position by 3x3 part of view matrix
      m_view(3) = -(mat(3).RotateByMatrix(m_view));

      cam_flags &= ~CAMF_RESET_VIEW;
   }
}

//----------------------------

bool I3D_camera::PrepareViewFrustum(S_view_frustum &vf, I3D_bsphere &vf_sphere, float inv_aspect) const{

   vf.num_clip_planes = 6;

   const S_matrix &m = GetMatrix();
   if(m(0).Dot(m(0)) < (MRG_ZERO))
      return false;
   const S_vector &cam_pos = m(3);

   vf.view_pos = cam_pos;

   int i;

   if(!(cam_flags&CAMF_ORTHOGONAL)){
      float sin_fov = (float)sin(fov_a*.5f);
      float cos_fov = (float)cos(fov_a*.5f);

      static const E_VIEW_FRUSTUM_SIDE side_map[4] = {
         VF_LEFT, VF_RIGHT, VF_BOTTOM, VF_TOP
      };

                              //get all planes
      for(i=0; i<2; i++){
         E_VIEW_FRUSTUM_SIDE side_max = side_map[i*2+1];
         E_VIEW_FRUSTUM_SIDE side_min = side_map[i*2];

         S_vector &n_max = vf.clip_planes[side_max].normal;
         float &d_max = vf.clip_planes[side_max].d;
         S_vector &n_min = vf.clip_planes[side_min].normal;
         float &d_min = vf.clip_planes[side_min].d;

         if(!i){              //x
            n_max.x = cos_fov;
            n_min.x = -n_max.x;
            n_max.y = 0.0f;
            n_min.y = 0.0f;
            n_max.z = -sin_fov;
            n_min.z = n_max.z;
         }else{               //y
            n_max.x = 0.0f;
            n_min.x = 0.0f;
            n_max.y = cos_fov;
            n_max.z = -sin_fov;
            n_max.z *= inv_aspect;
            n_max.Normalize();

            n_min.y = -n_max.y;
            n_min.z = n_max.z;
         }
                              //rotate by view
         n_max %= m;
         n_min %= m;

         d_max = - n_max.Dot(cam_pos);
         d_min = - n_min.Dot(cam_pos);
      }
   }else{
                              //orthogonal
      float o_scale = 1.0f / ortho_scale;
                              //horizontal
      S_normal mx = m(0);
      vf.clip_planes[VF_RIGHT].normal = mx;
      vf.clip_planes[VF_RIGHT].d = -cam_pos.Dot(vf.clip_planes[VF_RIGHT].normal);

      vf.clip_planes[VF_LEFT].normal = -mx;
      vf.clip_planes[VF_LEFT].d = -vf.clip_planes[VF_RIGHT].d;

      vf.clip_planes[VF_RIGHT].d -= o_scale;
      vf.clip_planes[VF_LEFT].d -= o_scale;
                              //vertical
      S_normal my = m(1);
      vf.clip_planes[VF_TOP].normal = my;
      vf.clip_planes[VF_TOP].d = -cam_pos.Dot(vf.clip_planes[VF_TOP].normal);

      vf.clip_planes[VF_BOTTOM].normal = -my;
      vf.clip_planes[VF_BOTTOM].d = -vf.clip_planes[VF_TOP].d;

      vf.clip_planes[VF_TOP].d -= o_scale * inv_aspect;
      vf.clip_planes[VF_BOTTOM].d -= o_scale * inv_aspect;

   }
   S_normal mz(m(2));
   {                       //z
      S_vector &n_max = vf.clip_planes[VF_BACK].normal;
      float &d_max = vf.clip_planes[VF_BACK].d;
      S_vector &n_min = vf.clip_planes[VF_FRONT].normal;
      float &d_min = vf.clip_planes[VF_FRONT].d;

                           //we look along z axis, so use direction vector
      n_max = mz;
      n_min = -n_max;

      d_max = - n_max.Dot(cam_pos);
      d_min = - d_max;// + current_camera->GetNCP();
      d_max -= fcp;
   }

                              //prepare bounding sphere
   if(!(cam_flags&CAMF_ORTHOGONAL)){
      {
                              //get intersection of 2 planes
         const S_plane &pl1 = vf.clip_planes[VF_RIGHT];
         const S_plane &pl2 = vf.clip_planes[VF_TOP];
         S_vector pt, dir;
         bool b = pl2.IntersectionLine(pl1, pt, dir);
         assert(b);
                              //get intersection of line and far plane
         float u = vf.clip_planes[VF_BACK].normal.Dot(dir);
         assert(!IsAbsMrgZero(u));
         u = -(vf.clip_planes[VF_BACK].normal.Dot(pt) + vf.clip_planes[VF_BACK].d) / u;
         S_vector v_fcp = pt + dir * u;

         S_vector l_half = cam_pos + (v_fcp - cam_pos) * .5f;

         S_vector n; n.GetNormal(cam_pos + mz*fcp, cam_pos, v_fcp); n.Normalize();
         n.GetNormal(cam_pos + n, cam_pos, l_half); n.Normalize();
         float u1, u2;
         b = LineIntersection(cam_pos, mz, l_half, n, u1, u2);
         assert(b);
         if(u1 >= fcp){
            vf_sphere.pos = cam_pos + mz * fcp;
            vf_sphere.radius = (vf_sphere.pos - v_fcp).Magnitude();
         }else{
            vf_sphere.pos = cam_pos + mz * u1;
            vf_sphere.radius = (cam_pos - vf_sphere.pos).Magnitude();
         }
      }
   }else{
                              //todo: compute proper bounding sphere
      vf_sphere.pos.Zero();
      vf_sphere.radius = 1e+16f;
   }
                              //get 4 edge points on fcp
                              // for portal transforms
   static const int plane_index[] = { 
      VF_BOTTOM,
      VF_LEFT,
      VF_TOP,
      VF_RIGHT,
      VF_BOTTOM,
   };
                              //unoptimized...
   for(i=0; i<4; i++){
                              //get intersection of 2 planes
      S_vector pt, dir;
      const S_plane &pl1 = vf.clip_planes[plane_index[i]];
      const S_plane &pl2 = vf.clip_planes[plane_index[i+1]];
      pl2.IntersectionLine(pl1, pt, dir);
                              //get intersection of line and back clipping plane
      const S_plane &pl = vf.clip_planes[VF_BACK];
      float u = pl.normal.Dot(dir);
      assert(!IsAbsMrgZero(u));
      u = -(pl.normal.Dot(pt) + pl.d) / u;
                              //store vertex of intersection
      vf.frustum_pts[i] = cam_pos + dir;
   }
   //pc.exp_frustum.Make(vf);
   return true;
}

//----------------------------

void I3D_camera::Mirror(const S_plane &pl){

   GetMatrix();
   S_matrix &m = matrix;
                              //make mirror
   m(0) = m(0) - pl.normal * (m(0).Dot(pl.normal) * 2.0f);
   m(1) = m(1) - pl.normal * (m(1).Dot(pl.normal) * 2.0f);
   m(2) = m(2) - pl.normal * (m(2).Dot(pl.normal) * 2.0f);
   m(3) = m(3) - pl.normal * (m(3).DistanceToPlane(pl) * 2.0f);

}

//----------------------------

void I3D_camera::Draw1(PI3D_scene scene, bool strong) const{

   byte alpha(byte(strong ? 0xff : 0x60));

   const S_matrix &m = GetMatrix();
                              //draw cone
   {
      scene->SetRenderMatrix(m);

      S_vector v1;
      float f = fov_a*.5f;
      v1.x = (float)sin(f);
      v1.z = (float)cos(f);
      v1.y = v1.x * .75f;
      v1 *= 2.0f / v1.Magnitude();
      S_vector v[5];
      v[0].Zero();
      v[1] = v1;
      v[2] = v1, v[2].y = -v[2].y;
      v[3] = v1, v[3].x = -v[3].x;
      v[4] = v[3], v[4].y = -v[4].y;
      static const word indx[] = {0, 1, 0, 2, 0, 3, 0, 4, 1, 2, 2, 4, 4, 3, 3, 1};

      scene->DrawLines(v, 5, indx, sizeof(indx)/sizeof(word), (alpha<<24) | 0xcccccc);
   }
   scene->DrawIcon(GetWorldPos(), 9, (alpha<<24) | 0xffffff);

   /*
   {
      PI3D_visual vis = scene->GetDebugDrawVisual("joint");
      if(vis)
         scene->RenderDebugDrawVisual(vis, matrix, GetInvMatrix());
   }
   /**/
}

//----------------------------

I3D_RESULT I3D_camera::DebugDraw(PI3D_scene scene) const{

   Draw1(scene, true);
   return I3D_OK;
}

//----------------------------
//----------------------------
