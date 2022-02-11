/*--------------------------------------------------------
   Copyright (c) Lonely Cat Games
   All rights reserved.

   File: billbrd.cpp
   Content: I3D_object_billboard - billboard frame, frame
      inherited from I3D_object, making billboard effect
      by 'looking' always towards active camera.
   Author: Michal Bacik
--------------------------------------------------------*/

#include "all.h"
#include "object.h"
#include "mesh.h"

//----------------------------

#if 1

#define DEFAULT_AXIS 2
#define DEFAULT_MODE 0

#else

#define DEFAULT_AXIS 1
#define DEFAULT_MODE BBRDF_AXIS_MODE

#endif

//----------------------------
//----------------------------

class I3D_object_billboard: public I3D_object{
#define BBRDF_AXIS_MODE    1     //rotate by axis, rather than look to camera

   dword bbrd_flags;

   dword rot_axis;            //0=x, 1=y, 2=z

                              //run-time:
   S_matrix g_matrix;
public:
   I3D_object_billboard(PI3D_driver d);

public:
                              //specialized methods of I3D_visual:
   virtual bool ComputeBounds();

   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(SetProperty)(dword index, dword value);
   I3DMETHOD_(dword,GetProperty)(dword index) const;
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(SetMode)(dword axis, bool axis_align){
      if(axis>3) return I3DERR_INVALIDPARAMS;
      rot_axis = axis;
      bbrd_flags &= ~BBRDF_AXIS_MODE;
      if(axis_align)
         bbrd_flags |= BBRDF_AXIS_MODE;
      return I3D_OK;
   }

   I3DMETHOD_(void,GetMode)(dword &axis, bool &axis_align) const{
      axis = rot_axis;
      axis_align = (bbrd_flags&BBRDF_AXIS_MODE);
   }
};

//----------------------------

I3D_object_billboard::I3D_object_billboard(PI3D_driver d):
   I3D_object(d),
   rot_axis(DEFAULT_AXIS),
   bbrd_flags(DEFAULT_MODE)
{
   visual_type = I3D_VISUAL_BILLBOARD;
   g_matrix = I3DGetIdentityMatrix();
}

//----------------------------

I3D_RESULT I3D_object_billboard::Duplicate(CPI3D_frame frm){

   if(frm==this) return I3D_OK;
   if(frm->GetType1()!=FRAME_VISUAL)
      return I3D_frame::Duplicate(frm);

   CPI3D_visual vis = I3DCAST_CVISUAL(frm);
   switch(vis->GetVisualType1()){
   case I3D_VISUAL_BILLBOARD:
      {
         I3D_object_billboard *bobj = (I3D_object_billboard*)vis;
         bbrd_flags = bobj->bbrd_flags;
         rot_axis = bobj->rot_axis;
      }
      break;
   }
   return I3D_object::Duplicate(vis);
}

//----------------------------

bool I3D_object_billboard::ComputeBounds(){

   if(!mesh){
      bound.bound_local.bbox.Invalidate();
      bound.bound_local.bsphere.pos.Zero();
      bound.bound_local.bsphere.radius = 0.0f;
      return true;
   }

   PI3D_mesh_base mb = mesh;
   mb->ComputeBoundingVolume();
   bound.bound_local = mb->GetBoundingVolume();

   bound.bound_local.bsphere.radius += bound.bound_local.bsphere.pos.Magnitude();
   bound.bound_local.bsphere.pos.Zero();

   for(int i=0; i<3; i++){
      float f = bound.bound_local.bsphere.radius;
      bound.bound_local.bbox.min.Minimal(S_vector(-f, -f, -f));
      bound.bound_local.bbox.max.Maximal(S_vector(f, f, f));
   }
   vis_flags |= VISF_BOUNDS_VALID;
   frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
   return true;
}

//----------------------------

static float Angle2D(const S_vector2 &v1){

   float a;
   float fx = I3DFabs(v1.x);
   float fy = I3DFabs(v1.y);

   if(fy>fx){
      a = fy<MRG_ZERO ? 0.0f : (float)atan(-v1.x/v1.y);
      if(v1.y<0.0f) a += PI;
   }else{
      a = fx>=MRG_ZERO ?
         (float)atan(-v1.y/-v1.x) + PI*.5f :
         v1.x<0.0f ? PI*.5f : -PI*.5f;
      if(v1.x>0.0f) a += PI;
   }
   return a;
}

//----------------------------

static float Angle2D(const S_vector2 &v1, const S_vector2 &v2){

   float f1 = Angle2D(v1);
   float f2 = Angle2D(v2);
   float f = f2 - f1;
   if(f>PI)
      f -= (PI*2.0f);
   else
   if(f <= - PI)
      f += (PI*2.0f);
   return f;
}

//----------------------------

I3D_RESULT I3D_object_billboard::SetProperty(dword index, dword value){

   switch(index){
   case 0:
      bbrd_flags &= ~BBRDF_AXIS_MODE;
      if(value)
         bbrd_flags |= BBRDF_AXIS_MODE;
      break;
   case 1:
      if(value >= 3)
         return I3DERR_INVALIDPARAMS;
      rot_axis = value;
      break;
   default: return I3DERR_INVALIDPARAMS;
   }
   return I3D_OK;
}

//----------------------------

dword I3D_object_billboard::GetProperty(dword index) const{

   switch(index){
   case 0: return (bbrd_flags&BBRDF_AXIS_MODE);
   case 1: return rot_axis; break;
   }
   return 0xcdcdcdcd;
}

//----------------------------

void I3D_object_billboard::AddPrimitives(S_preprocess_context &pc){

#ifndef GL
   if(pc.mode==RV_SHADOW_CASTER)
      return;
#endif
   if(!mesh)
      return;
   AddPrimitives1(mesh, pc);
}

//----------------------------
#ifndef GL
void I3D_object_billboard::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   PI3D_mesh_base mb = mesh;

   {
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }
      dword vertex_count;
      const C_auto_lod *auto_lods = mb->GetAutoLODs();
      if(rp.curr_auto_lod < 0)
         vertex_count = mb->vertex_buffer.NumVertices();
      else
         vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;


      PI3D_camera cam = pc.scene->GetActiveCamera1();
      const S_matrix &m_cam = cam->GetMatrixDirect();
                              //billboard - look always to camera
                              // use camera matrix, and our position
      if(!(bbrd_flags&BBRDF_AXIS_MODE)){
         S_vector scale = matrix.GetScale();
         switch(rot_axis){
         case 0:
            g_matrix(0) = m_cam(2) * scale[0];
            g_matrix(1) = m_cam(0) * scale[1];
            g_matrix(2) = m_cam(1) * scale[2];
            break;
         case 1:
            g_matrix(0) = m_cam(1) * scale[0];
            g_matrix(1) = m_cam(2) * scale[1];
            g_matrix(2) = m_cam(0) * scale[2];
            break;
         case 2:
            g_matrix(0) = m_cam(0) * scale[0];
            g_matrix(1) = m_cam(1) * scale[1];
            g_matrix(2) = m_cam(2) * scale[2];
            break;
         }
         g_matrix(3) = matrix(3);
      }else{
                                 //billboard - look always to camera
         g_matrix = matrix;

                                 //get visual's local direction
         const S_vector &loc_dir = f_matrix((rot_axis+1)%3);
         S_vector cam_look = matrix(3) - m_cam(3);

         S_matrix mi = GetInvMatrix1();
         cam_look = cam_look.RotateByMatrix(mi);
         int aa1 = (rot_axis+2)%3;
         int aa2 = (rot_axis+1)%3;
         float a = Angle2D(S_vector2(cam_look[aa1], cam_look[aa2]), S_vector2(loc_dir[aa1], loc_dir[aa2]));

         S_matrix m_rot;
         switch(rot_axis){
         case 0: m_rot.RotationX(-a + PI*.5f); break;
         case 1: m_rot.RotationY(a); break;
         case 2: m_rot.RotationZ(-a); break;
         }

         S_matrix tmp = f_matrix * m_rot;
         tmp(3) = f_matrix(3);
         g_matrix = tmp * GetParent1()->GetMatrix();
      }

                              //transform vertices
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX;

      if(drv->IsDirectTransform()){
         if(pc.mode!=RV_SHADOW_CASTER)
            prep_flags |= VSPREP_MAKELIGHTING;
         prep_flags |= VSPREP_COPY_UV;
      }else{
         if(pc.mode!=RV_SHADOW_CASTER){
#ifdef DEBUG_NO_CACHE_LIGHT
            vis_flags &= ~VISF_DEST_LIGHT_VALID;  
#endif
            if(!(vis_flags&VISF_DEST_LIGHT_VALID)){
               prep_flags |= VSPREP_MAKELIGHTING;
               vis_flags |= VISF_DEST_LIGHT_VALID;
            }
         }
                              //bug in PV - always copy uv
         //if(!(vis_flags&VISF_DEST_UV0_VALID)){
            prep_flags |= VSPREP_COPY_UV;
            //vis_flags |= VISF_DEST_UV0_VALID;
         //}
      }

      PI3D_mesh_base mb = mesh;
      I3D_driver::S_vs_shader_entry_in se;//(vs_decl);
      PrepareVertexShader(mb->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags, &g_matrix);

      if(!drv->IsDirectTransform()){
         drv->DisableTextureStage(1);

         drv->SetStreamSource(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.GetSizeOfVertex());
         drv->SetVSDecl(vertex_buffer.vs_decl);

         HRESULT hr;
         hr = drv->GetDevice1()->ProcessVertices(mb->vertex_buffer.D3D_vertex_buffer_index,
            vertex_buffer.D3D_vertex_buffer_index, vertex_count,
            vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
         CHECK_D3D_RESULT("ProcessVertices", hr);
         drv->SetVertexShader(NULL);
      }
   }
   DrawPrimitiveVisual(mb, pc, rp);
}
#endif
//----------------------------

void I3D_object_billboard::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   PI3D_mesh_base mb = mesh;

   PI3D_camera cam = pc.scene->GetActiveCamera1();
   const S_matrix &m_cam = cam->GetMatrixDirect();
                           //billboard - look always to camera
                           // use camera matrix, and our position
   if(!(bbrd_flags&BBRDF_AXIS_MODE)){
      S_vector scale = matrix.GetScale();
      switch(rot_axis){
      case 0:
         g_matrix(0) = m_cam(2) * scale[0];
         g_matrix(1) = m_cam(0) * scale[1];
         g_matrix(2) = m_cam(1) * scale[2];
         break;
      case 1:
         g_matrix(0) = m_cam(1) * scale[0];
         g_matrix(1) = m_cam(2) * scale[1];
         g_matrix(2) = m_cam(0) * scale[2];
         break;
      case 2:
         g_matrix(0) = m_cam(0) * scale[0];
         g_matrix(1) = m_cam(1) * scale[1];
         g_matrix(2) = m_cam(2) * scale[2];
         break;
      }
      g_matrix(3) = matrix(3);
   }else{
                              //billboard - look always to camera
      g_matrix = matrix;

                              //get visual's local direction
      const S_vector &loc_dir = f_matrix((rot_axis+1)%3);
      S_vector cam_look = matrix(3) - m_cam(3);

      S_matrix mi = GetInvMatrix1();
      cam_look = cam_look.RotateByMatrix(mi);
      int aa1 = (rot_axis+2)%3;
      int aa2 = (rot_axis+1)%3;
      float a = Angle2D(S_vector2(cam_look[aa1], cam_look[aa2]), S_vector2(loc_dir[aa1], loc_dir[aa2]));

      S_matrix m_rot;
      switch(rot_axis){
      case 0: m_rot.RotationX(-a + PI*.5f); break;
      case 1: m_rot.RotationY(a); break;
      case 2: m_rot.RotationZ(-a); break;
      }

      S_matrix tmp = f_matrix * m_rot;
      tmp(3) = f_matrix(3);
      g_matrix = tmp * GetParent1()->GetMatrix();
   }

                           //transform vertices
   dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_MAKELIGHTING | VSPREP_COPY_UV;

   I3D_driver::S_vs_shader_entry_in se;
   PrepareVertexShader(mb->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags, &g_matrix);

   DrawPrimitiveVisualPS(mb, pc, rp);
}

//----------------------------
//----------------------------

extern const S_visual_property props_Billboard[] = {
                              //I3DPROP_BBRD_AXIS_MODE
   {I3DPROP_BOOL, "Axis mode", "True to use axis-mode particle."},
                              //I3DPROP_BBRD_ROT_AXIS
   {I3DPROP_ENUM, "Rot axis\0X\0Y\0Z\0", "Rotation axis for axis mode."},
   {I3DPROP_NULL}
};

I3D_visual *CreateBillboard(PI3D_driver drv){
   return new I3D_object_billboard(drv);
}

//----------------------------


