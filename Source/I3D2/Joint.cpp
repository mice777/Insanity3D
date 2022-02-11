/*--------------------------------------------------------
   Copyright (c) 1999, 2000 Michal Bacik. 
   All rights reserved.

   File: Joint.cpp
   Content: Joint frame.
--------------------------------------------------------*/

#include "all.h"
#include "joint.h"
#include "visual.h"
#include "scene.h"

//----------------------------

I3D_joint::I3D_joint(PI3D_driver d):
   I3D_frame(d),
   joint_flags(0),
   hide_dist(0.0f),
   vol_radius(0.0f),
   vol_half_len(0.0f),
   draw_size(.5f),
   draw_len(.5f)
{
   draw_rot.Identity();
   bbox.min = S_vector(-.1f, -.1f, -.1f);
   bbox.max = S_vector( .1f,  .1f,  .1f);

   drv->AddCount(I3D_CLID_JOINT);
   type = FRAME_JOINT;
   enum_mask = ENUMF_JOINT;
}

//----------------------------

I3D_joint::~I3D_joint(){
   drv->DecCount(I3D_CLID_JOINT);
}

//----------------------------

I3D_RESULT I3D_joint::Duplicate(CPI3D_frame frm){

   if(frm->GetType1()==FRAME_JOINT){
      CPI3D_joint bp = I3DCAST_CJOINT(frm);

      joint_flags = bp->joint_flags;
      bbox_matrix = bp->bbox_matrix;
      bbox = bp->bbox;
#ifdef USE_CAPCYL_JNT_VOLUMES
      vol_pos = bp->vol_pos;
      vol_dir = bp->vol_dir;
      vol_radius = bp->vol_radius;
      vol_half_len = bp->vol_half_len;
#else
      volume_box_matrix = bp->volume_box_matrix;
      volume_box = bp->volume_box;
#endif
      hide_dist = bp->hide_dist;
      draw_size = bp->draw_size;
      draw_len = bp->draw_len;
      draw_rot = bp->draw_rot;
   }
   return I3D_frame::Duplicate(frm);
}

//----------------------------

void I3D_joint::PropagateDirty(){

   I3D_frame::PropagateDirty();
   /*
   for(PI3D_frame frm = GetParent1(); frm; frm=frm->GetParent1()){
      if(frm->GetType1()==FRAME_VISUAL && I3DCAST_VISUAL(frm)->GetVisualType1()==I3D_VISUAL_SINGLEMESH){
         frm->SetFrameFlags(frm->GetFrameFlags() & ~FRMFLAGS_SM_BOUND_VALID);
         break;
      }
   }
   */
   //if(!drv->IsDirectTransform())
   {
                              //affect parent SM - reset light and SM bounds
      for(PI3D_frame frm = GetParent1(); frm; frm=frm->GetParent1()){
         if(frm->GetType1()==FRAME_VISUAL && I3DCAST_VISUAL(frm)->GetVisualType1()==I3D_VISUAL_SINGLEMESH){
            I3DCAST_VISUAL(frm)->ResetLightInfo();
            frm->SetFrameFlags(frm->GetFrameFlags() & ~FRMFLAGS_SM_BOUND_VALID);
            break;
         }
      }
   }
}

//----------------------------

I3D_RESULT I3D_joint::SetPos(const S_vector &v){

   if(joint_flags&JOINTF_FIXED_POSITION){
      /*                      //test - affect
      PI3D_frame prnt = GetParent();
      S_vector loc = v % prnt->GetMatrix();
      S_quat r;
      r.SetDir(loc, prnt->GetMatrix()(1));
      GetParent()->SetRot(r);
      */
      return I3DERR_UNSUPPORTED;
   }
   //PropagateDirty();
   return I3D_frame::SetPos(v);
}

//----------------------------

/*
I3D_RESULT I3D_joint::SetScale(float s){ PropagateDirty(); return I3D_frame::SetScale(s); }
I3D_RESULT I3D_joint::SetRot(const S_quat &r){ PropagateDirty(); return I3D_frame::SetRot(r); }
I3D_RESULT I3D_joint::SetDir(const S_vector &d, float roll){ PropagateDirty(); return I3D_frame::SetDir(d, roll); }
I3D_RESULT I3D_joint::SetDir1(const S_vector &dir, const S_vector &up){ PropagateDirty(); return I3D_frame::SetDir1(dir, up); }
*/

//----------------------------

bool I3D_joint::AddRegion(const I3D_bbox &bb1, const S_matrix &m1, int index){

   if(index<-1 || index>=255)
      return false;
   if(joint_flags&JOINTF_BBOX_VALID)
      return false;
   bbox = bb1;
   bbox_matrix = m1;
   joint_flags |= JOINTF_BBOX_VALID;
   joint_flags &= ~JOINTF_REGION_INDX_MASK;
   joint_flags |= index&JOINTF_REGION_INDX_MASK;
   //draw_size = (bbox.max-bbox.min).Magnitude() * m1(0).Magnitude() * 1.0f;
   return true;
}

//----------------------------

bool I3D_joint::SetVolumeBox(const I3D_bbox &bb, const S_matrix &m){

   if(joint_flags&JOINTF_VBOX_VALID)
      return false;
#ifdef USE_CAPCYL_JNT_VOLUMES
   vol_pos = ((bb.min+bb.max) * .5f) * m;
   dword i = 0;
   if((bb.max.x-bb.min.x) < (bb.max.y-bb.min.y))
      ++i;
   if((bb.max[i]-bb.min[i]) < (bb.max.z-bb.min.z))
      i = 2;
   vol_dir.Zero();
   vol_dir[i] = (bb.max[i] - bb.min[i]) * .5f;
   vol_dir %= m;
   vol_half_len = vol_dir.Magnitude();
   vol_dir /= vol_half_len;
   ++i %= 3;
   float a = bb.max[i] - bb.min[i];
   ++i %= 3;
   float b = bb.max[i] - bb.min[i];
#if 1
   vol_radius = (a + b) * .25f;
   vol_radius *= m(0).Magnitude();
   vol_half_len -= vol_radius;
#else
   vol_radius = I3DSqrt(a*b / PI);
   vol_radius *= m(0).Magnitude();
   vol_half_len -= vol_radius;
#endif
   assert(vol_half_len >= 0.0f);
#else
   volume_box = bb;
   volume_box_matrix = m;
#endif//USE_CAPCYL_JNT_VOLUMES

   joint_flags |= JOINTF_VBOX_VALID;
   return true;
}

//----------------------------

static const dword color = 0xffb000;
static const dword volume_color = 0xb04c4c;
static const word sides_i[] = {
   1, 3, 7,  7, 5, 1,
   4, 6, 2,  2, 0, 4,
   2, 6, 7,  7, 3, 2,
   4, 0, 1,  1, 5, 4,

   0, 2, 3,  3, 1, 0,
   7, 6, 5,  6, 4, 5,
};

struct S_joint_vertex{
   S_vector xyz;
   S_vector normal;
};

static const float joint_vertices[][6] = {
                              //left side
   {-1, -1, .1f, -1, 0, 0}, {-1,  1, .1f, -1, 0, 0}, { 0,  1,   1, -1, 0, 0},
                              //right side
   { 1, -1, .1f,  1, 0, 0}, { 1,  1, .1f,  1, 0, 0}, { 0,  1,   1,  1, 0, 0},
                              //top side
};

//----------------------------

void I3D_joint::Draw1(PI3D_scene scene, bool strong) const{

   bool zw_on = drv->IsZWriteEnabled();
   drv->EnableZWrite(false);

   drv->SetTexture1(0, NULL);

   dword alpha = strong ? 0x20 : 0x0c;

   float size = GetIconSize() * matrix(0).Magnitude();

                              //draw connection to parent
   if(GetParent1()){
      const S_vector &v0 = GetWorldPos();
      const S_vector &v1 = GetParent1()->GetWorldPos();
      scene->SetRenderMatrix(I3DGetIdentityMatrix());
      scene->DrawLine(v0, v1, (alpha<<24) | color);
   }
  
   S_vector bbox_full[8];
   switch(GetRegionIndex()){
   case 2:
   case 3:
      if(IsBBoxValid()){
         S_matrix m = GetBBoxMatrix1() * GetMatrix();
         m(0) *= (bbox.max.x - bbox.min.x) * .5f;
         m(1) *= (bbox.max.y - bbox.min.y) * .5f;
         m(2) *= (bbox.max.z - bbox.min.z) * .5f;
         scene->DebugDrawSphere(m, 1.0f, (strong ? 0x20000000 : 0x10000000) | color);
      }
      break;
   default:
      /*
      {
         PI3D_visual vis = scene->GetDebugDrawVisual("joint");
         if(vis){
            S_matrix m = matrix;
            m = S_matrix(draw_rot) % m;
            m(3) = matrix(3);
            S_matrix m_inv = ~m;
            m(0) *= draw_size;
            m(1) *= draw_size;
            m(2) *= draw_len;
            scene->RenderDebugDrawVisual(vis, m, m_inv);
         }
      }
      /**/
      if(IsBBoxValid()){

         GetBBox1().Expand(bbox_full);
         scene->SetRenderMatrix(GetBBoxMatrix1() * GetMatrix());

                                 //draw sides
         scene->DrawTriangles(bbox_full, 8, I3DVC_XYZ, sides_i, sizeof(sides_i)/sizeof(word),
            (strong ? 0x20000000 : 0x10000000) | color);

                              //draw edges
         scene->DebugDrawBBox(bbox_full, (alpha<<24) | color);

         {                     //draw direction line
            S_vector v[2];
            v[0].Zero();
            v[0].z = GetBBox1().max.z;
            v[1] = v[0];
            v[1].z += GetBBox1().max.z - GetBBox1().min.z;
            static const word dir_list[2] = {0, 1};
            scene->DrawLines(v, 2, dir_list, 2, (alpha<<24) | 0xff00ff);
         }
      }
      /**/
      break;
   }

   if((drv->GetFlags2()&DRVF2_DRAWVOLUMES) && IsVolumeBoxValid()){
#ifdef USE_CAPCYL_JNT_VOLUMES
                              //draw capped cylinder volume
      S_matrix m = GetMatrix();
                              //use crazy temp matrix made of volume's pos and dir
                              // (slow, but saves redundant data stored in joint)
      S_matrix m1; m1.Identity();
      m1(2) = vol_dir;
      m1(1) = vol_dir.Cross(S_vector(vol_dir.x, vol_dir.z, vol_dir.y));
      if(I3DFabs(1.0f-m1(1).Square()) > .04f) m1(1) = vol_dir.Cross(S_vector(vol_dir.y, vol_dir.x, vol_dir.z));
      //assert(I3DFabs(1.0f-m1(1).Square()) < .04f);
      //if(m1(1).IsNull()) m1(1) = vol_dir.Cross(S_vector(vol_dir.z, vol_dir.y, vol_dir.x));
      m1(0).GetNormal(m1(1), m1(3), m1(2));
      m1(3) = vol_pos;

      dword alpha = strong ? 0xff : 0x80;
      scene->DebugDrawCylinder(m1 * m, vol_radius, vol_half_len, (alpha<<24) | 0xff0000, true);
#else
      GetVolumeBox1().Expand(bbox_full);
      scene->SetRenderMatrix(GetVolumeBoxMatrix1() * GetMatrix());

      //dword color = strong ? 0xff0000 : dw_volume_color;
                              //draw sides
      scene->DrawTriangles(bbox_full, 8, I3DVC_XYZ, sides_i, sizeof(sides_i)/sizeof(word), 0x40ff0000);

      dword alpha = strong ? 0xff : 0x80;
                              //draw edges
      scene->DebugDrawBBox(bbox_full, (alpha<<24) | 0xff0000);
#endif
   }
   scene->DrawIcon(GetWorldPos(), 11, (alpha<<24) | 0xffffff, size);
   drv->EnableZWrite(zw_on);
}

//----------------------------

/*
void I3D_joint::SetMatrixDirty() const{

   I3D_frame::SetMatrixDirty();

   if(!drv->IsDirectTransform()){
                              //affect parent SM - reset light and SM bounds
      for(PI3D_frame frm = GetParent1(); frm; frm=frm->GetParent1()){
         if(frm->GetType1()==FRAME_VISUAL && I3DCAST_VISUAL(frm)->GetVisualType1()==I3D_VISUAL_SINGLEMESH){
            I3DCAST_VISUAL(frm)->ResetLightInfo();
            break;
         }
      }
   }
}
*/

//----------------------------
//----------------------------
