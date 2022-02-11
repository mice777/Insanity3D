/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Volume.cpp
   Content: Volume frame and collision testing functions.
--------------------------------------------------------*/

#include "all.h"
#include "visual.h"
#include "mesh.h"
#include "volume.h"
#include "scene.h"
#include "model.h"
#include "sng_mesh.h"
#include "joint.h"
#include "bsp.h"


//----------------------------
                              //used to reset FPU in debugger - just step over the function
                              // and debugging may continue after FPU gets invalidated
extern void FPUReset();
void FPUReset(){
   __asm{
      push eax
      push ecx
      push edx                                         
   }
   _fpreset();
   __asm{
      pop edx
      pop ecx
      pop eax
   }
}

//----------------------------

I3D_volume::I3D_volume(CPI3D_scene s):
   scene(s),
   I3D_frame(s->GetDriver1()),
   material_id(0),
   owner(NULL),
   volume_type(I3DVOLUME_NULL),
   vol_flags(0),
   curr_octtree_level(-1),
   //test_col_tag(0),
   collide_category_bits(0xffffffff),
   non_uniform_scale(S_vector(1.0f, 1.0f, 1.0f))
{
   type = FRAME_VOLUME;
   enum_mask = ENUMF_VOLUME;
   drv->AddCount(I3D_CLID_VOLUME);

   ((PI3D_scene)scene)->AddRef();
   ((PI3D_scene)scene)->AddXRef();

   g_matrix.Identity();
   dirty_node.this_vol = this;
   tag_node.this_vol = this;
}

//----------------------------

I3D_volume::~I3D_volume(){

   drv->DecCount(I3D_CLID_VOLUME);

   scene->RemoveFromDynamicVolume(this, false);

   PI3D_scene scn = (PI3D_scene)scene;
   scene = NULL;
                              //must be called last (possibly releases scene)
   scn->ReleaseXRef();
   scn->Release();
}

//----------------------------

I3D_RESULT I3D_volume::Duplicate(CPI3D_frame frm){

   if(frm==this)
      return I3D_OK;

   if(frm->GetType1()==FRAME_VOLUME){
      CPI3D_volume vp = I3DCAST_CVOLUME(frm);     
      volume_type = vp->volume_type;
      owner = NULL;
      vol_flags = vp->vol_flags;
      non_uniform_scale = vp->non_uniform_scale;
      material_id = vp->material_id;
   }
   I3D_RESULT ir = I3D_frame::Duplicate(frm);
   vol_flags |= VOLF_RESET;
   if((GetFrameFlags()&I3D_FRMF_STATIC_COLLISION) || IsSphericalType())
      scene->UpdateDynamicVolume(this);
   return ir;
}

//----------------------------

void I3D_volume::SetOn(bool on){

   if(on != IsOn1()){
      I3D_frame::SetOn(on);
      //scene->ResetDynamicVolTree();
      scene->UpdateDynamicVolume(this);
   }
}

//----------------------------

dword I3D_volume::SetFlags(dword new_flags, dword flags_mask){

   if(flags_mask&I3D_FRMF_STATIC_COLLISION)
      scene->UpdateDynamicVolume(this);
   return I3D_frame::SetFlags(new_flags, flags_mask);
}

//----------------------------

I3D_RESULT I3D_volume::SetNUScale(const S_vector &scl){

                              //we don't support negative scales
   if(scl.x < 0.0f || scl.y < 0.0f || scl.z < 0.0f)
      return I3DERR_INVALIDPARAMS;

   frm_flags &= ~FRMFLAGS_G_MAT_VALID;
   //frm_flags |= FRMFLAGS_UPDATE_NEEDED;
   frm_flags |= FRMFLAGS_CHANGE_MAT;
   non_uniform_scale = scl;
   return I3D_OK;
}

//----------------------------

void I3D_volume::ResetDynamicVolume() const{

                              //can't call this when destructing
   if(!scene)
      return;

   //if(vol_flags&VOLF_RESET) return;
   vol_flags |= VOLF_RESET;

                              //reset only for static spherical types
   if(!(GetFrameFlags()&I3D_FRMF_STATIC_COLLISION) || IsSphericalType())
      scene->UpdateDynamicVolume(const_cast<PI3D_volume>(this));
}

//----------------------------

I3D_RESULT I3D_volume::SetVolumeType(I3D_VOLUMETYPE t){

   volume_type = t;
   ResetDynamicVolume();
   //if(curr_octtree_level!=-1)
      //scene->RemoveFromDynamicVolume(this, false);
   return I3D_OK;
}

//----------------------------

/*
void I3D_volume::ComputeBoundBox(PI3D_bbox bbox1) const{

   int i;
   bbox1->Invalidate();

   switch(volume_type){
   case I3DVOLUME_SPHERE:
      for(i=0; i<3; i++){
         bbox1->min[i] = Min(bbox1->min[i], w_sphere.pos[i] - w_sphere.radius);
         bbox1->max[i] = Max(bbox1->max[i], w_sphere.pos[i] + w_sphere.radius);
      }
      break;
   case I3DVOLUME_RECTANGLE:
   case I3DVOLUME_BOX:
                              //cube have 8 corners, each bbox corner enlarges itself and 3 axes
      for(i=0; i<4; i++){
         S_vector vmin = bbox.min;
         S_vector vmax = bbox.max;
         if(i<3){
            vmin -= normal[i] * (d_min[i] + d_max[i]);
            vmax += normal[i] * (d_min[i] + d_max[i]);
         }
         for(int ii=0; ii<3; ii++){
            bbox1->min[ii] = Min(bbox1->min[ii], Min(vmin[ii], vmax[ii]));
            bbox1->max[ii] = Max(bbox1->max[ii], Max(vmax[ii], vmin[ii]));
         }
      }
      break;
   default: assert(0);
   }
}
*/

//----------------------------

const S_matrix &I3D_volume::GetGeoMatrix() const{

   if(ChangedMatrix() || !(frm_flags&FRMFLAGS_G_MAT_VALID)){
      if(GetParent1()) matrix.Make(f_matrix, GetParent1()->GetMatrix());
      else matrix = f_matrix;
      frm_flags &= ~(FRMFLAGS_G_MAT_VALID | FRMFLAGS_CHANGE_MAT | FRMFLAGS_BSPHERE_TRANS_VALID);
   }
   if(!(frm_flags&FRMFLAGS_G_MAT_VALID)){
      g_matrix(0) = matrix(0) * non_uniform_scale[0];
      g_matrix(1) = matrix(1) * non_uniform_scale[1];
      g_matrix(2) = matrix(2) * non_uniform_scale[2];
      g_matrix(3) = matrix(3);
      frm_flags |= FRMFLAGS_G_MAT_VALID;
   }
   return g_matrix;
}

//----------------------------

bool I3D_volume::Prepare() const{

   if(!volume_type)
      return false;
   ChangedMatrix();
   if(!(vol_flags&VOLF_RESET))
      return true;
   int i;

   const S_matrix &m = GetMatrixDirect();
   w_sphere.pos = m(3);
   sphere_pos_dot = w_sphere.pos.Dot(w_sphere.pos);

   switch(volume_type){
   case I3DVOLUME_SPHERE:
      {
         w_sphere.radius = m(0).Magnitude();
         bbox.Invalidate();
         for(i=0; i<3; i++){
            bbox.min[i] = Min(bbox.min[i], w_sphere.pos[i] - w_sphere.radius);
            bbox.max[i] = Max(bbox.max[i], w_sphere.pos[i] + w_sphere.radius);
         }
      }
      break;

   case I3DVOLUME_CYLINDER:
   case I3DVOLUME_CAPCYL:
      {
         const S_matrix &m = GetMatrixDirect();
         float scl = m(0).Magnitude();
         world_radius = scl * non_uniform_scale.x;
         world_half_length = non_uniform_scale.z * scl;
         switch(volume_type){
         case I3DVOLUME_CYLINDER:
            w_sphere.radius = I3DSqrt(world_half_length * world_radius);
            break;
         case I3DVOLUME_CAPCYL:
            w_sphere.radius = world_half_length + world_radius;
            break;
         }
         bbox.Invalidate();
         for(i=0; i<3; i++){
            bbox.min[i] = Min(bbox.min[i], w_sphere.pos[i] - w_sphere.radius);
            bbox.max[i] = Max(bbox.max[i], w_sphere.pos[i] + w_sphere.radius);
         }
         normal[2] = matrix(2);
         normal[2].Normalize();
      }
      break;

   case I3DVOLUME_BOX:
      {
         const S_matrix &m = GetGeoMatrix();
                           //get bounding box and normals for 3 sides
         for(i=0; i<3; i++)
            normal[i] = m(i);

         for(i=0; i<3; i++){
            world_side[i] = normal[i] * 2.0f;
            world_side_dot[i] = world_side[i].Dot(world_side[i]);
            world_side_size[i] = world_side[i].Magnitude();
            //world_side_size_2[i] *= world_side_size_2[i];
         }
         bbox.max = normal[0] + normal[1] + normal[2];
         bbox.min = -bbox.max;
         bbox.min += m(3);
         bbox.max += m(3);
         for(i=0; i<3; i++){
            world_side_dot[i] = world_side[i].Dot(world_side[i]);
            world_side_size[i] = world_side[i].Magnitude();
            //world_side_size_2[i] *= world_side_size_2[i];

            normal[i].Normalize();
            d_min[i] = normal[i].Dot(bbox.min);
            d_max[i] = -normal[i].Dot(bbox.max);
         }
#if 0
                           //debug output
         {
            hr_scene->DEBUG_CODE(0, (dword)&bbox);

            S_vector vx[2]={ S_vector(0, 0, 0), normal[0]};
            hr_scene->DEBUG_CODE(0, (dword)vx);

            S_vector vy[2]={ S_vector(0, 0, 0), normal[1]};
            hr_scene->DEBUG_CODE(0, (dword)vy);

            S_vector vz[2]={ S_vector(0, 0, 0), normal[2]};
            hr_scene->DEBUG_CODE(0, (dword)vz);
         }
#endif
      }
                           //make bounding sphere from bounding box
      {
         S_vector mag_half = (bbox.max-bbox.min)*.5f;
         w_sphere.radius = mag_half.Magnitude();
         //w_sphere.pos = bbox.min + mag_half;
      }
      break;

   case I3DVOLUME_RECTANGLE:
      {
         const S_matrix &m = GetGeoMatrix();
                              //get bounding box and normal for 3 sides
         for(i=0; i<2; i++){
            normal[i] = m(i);
            world_side[i] = normal[i] * 2.0f;
            world_side_dot[i] = world_side[i].Dot(world_side[i]);
            world_side_size[i] = world_side[i].Magnitude();
            //world_side_size_2[i] *= world_side_size_2[i];
         }
                           //z-normal is computed - avoiding non-uniform scale squashing
         normal[2].GetNormal(S_vector(0.0f, 0.0f, 0.0f), normal[0], normal[1]);

         world_side[2].Zero();
         world_side_size[2] = 0.0f;

                           //z-axis is ignored
         bbox.max = normal[0] + normal[1];
         bbox.min = -bbox.max;
         bbox.min += m(3);
         bbox.max += m(3);

         for(i=0; i<3; i++){
            normal[i].Normalize();
            d_min[i] = normal[i].Dot(bbox.min);
            d_max[i] = -normal[i].Dot(bbox.max);
         }

#if 0
         {
            hr_scene->DEBUG_CODE(0, (dword)&bbox);

            S_vector vx[2]={ S_vector(0, 0, 0), normal[0]};
            hr_scene->DEBUG_CODE(0, (dword)vx);

            S_vector vy[2]={ S_vector(0, 0, 0), normal[1]};
            hr_scene->DEBUG_CODE(0, (dword)vy);

            S_vector vz[2]={ S_vector(0, 0, 0), normal[2]};
            hr_scene->DEBUG_CODE(0, (dword)vz);
         }
#endif
      }
                           //make bounding sphere from bounding box
      {
         S_vector mag_half = (bbox.max-bbox.min)*.5f;
         w_sphere.radius = mag_half.Magnitude();
         //w_sphere.pos = bbox.min + mag_half;
      }
      break;
   default: 
      assert(0);
   }

   vol_flags &= ~VOLF_RESET;
   return true;
}

//----------------------------
//----------------------------

static const word bbox_list[] = {
   0, 1, 2, 3, 4, 5, 6, 7,
   0, 2, 1, 3, 4, 6, 5, 7,
   0, 4, 1, 5, 2, 6, 3, 7
};

//----------------------------

static const S_vector box_verts[] = {
   S_vector(-1, -1, -1),
   S_vector(1, -1, -1),
   S_vector(-1, 1, -1),
   S_vector(1, 1, -1),
   S_vector(-1, -1, 1),
   S_vector(1, -1, 1),
   S_vector(-1, 1, 1),
   S_vector(1, 1, 1),
};

static const word box_fill_indicies[] = {
   1, 3, 7, 1, 7, 5,
   0, 4, 6, 0, 6, 2,
   2, 6, 7, 2, 7, 3,
   0, 1, 5, 0, 5, 4,
   0, 2, 3, 0, 3, 1,
   5, 7, 6, 5, 6, 4
};

static const word box_line_indicies[] = {
   0, 1, 0, 2, 0, 4, 1, 3, 1, 5, 2, 3, 2, 6,
   3, 7, 4, 5, 4, 6, 5, 7, 6, 7
};

//----------------------------

static void DrawVolBox(dword color_line, dword color_tri, PI3D_scene scene){

   if(color_line)
      scene->DrawLines(box_verts, 12, box_line_indicies, sizeof(box_line_indicies)/sizeof(word), color_line);
   scene->DrawTriangles(box_verts, 12, I3DVC_XYZ, box_fill_indicies, sizeof(box_fill_indicies)/sizeof(word), color_tri);
}

//----------------------------

static void DrawVolBox1(PI3D_scene scene, const I3D_bbox &bbox,
   CPS_vector normal, const float *d_min, const float *d_max, dword color){

   scene->SetRenderMatrix(I3DGetIdentityMatrix());
   S_vector bbox1[8];
   static const byte amap[4] = {0, 1, 2, 4};
   for(int ii=0; ii<4; ii++){
      bbox1[amap[ii]] = bbox.min;
      bbox1[7-amap[ii]] = bbox.max;
      if(ii>0){
         bbox1[amap[ii]] -= normal[ii-1] * (d_min[ii-1] + d_max[ii-1]);
         bbox1[7-amap[ii]] += normal[ii-1] * (d_min[ii-1] + d_max[ii-1]);
      }
   }
   scene->DrawLines(bbox1, 8, bbox_list, sizeof(bbox_list)/sizeof(word), color);
}

//----------------------------

static const S_vector rect_edges[4] = {
   S_vector(-1.0f, -1.0f, 0.0f),
   S_vector( 1.0f, -1.0f, 0.0f),
   S_vector( 1.0f,  1.0f, 0.0f),
   S_vector(-1.0f,  1.0f, 0.0f),
};

static const word rect_fill_indicies[] = {
   0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2
};

static const word rect_line_indicies[] = {
   0, 1, 1, 2, 2, 3, 3, 0
};

//----------------------------

static const dword draw_colors[2] = { //[trigger]
   0xbc4c4c, 0xff8080,
};

void I3D_volume::Draw1(PI3D_scene scene, bool strong, bool force_bsp, const dword *force_color) const{

   if(!volume_type) return;



   bool zw_en = drv->IsZWriteEnabled();
   if(zw_en) drv->EnableZWrite(false);

   dword color;
   if(force_bsp){
      assert(drv->GetFlags()&DRVF_DEBUGDRAWBSP);
      color = 0x500000ff;
   }else
   if(force_color){
      color = *force_color;
   }else{
      color = strong ? 0xff0000 : draw_colors[bool(vol_flags&VOLF_DEBUG_TRIGGER)];
      vol_flags &= ~VOLF_DEBUG_TRIGGER;

      if(frm_flags&I3D_FRMF_STATIC_COLLISION)
         color += 0x004c00;
   }

   drv->SetTexture(NULL);

   switch(volume_type){
   case I3DVOLUME_SPHERE:
      if(!force_color)
         color |= strong ? 0xff000000 : 0x80000000;
      scene->DebugDrawSphere(GetMatrix(), 1.0f, color);
      break;

   case I3DVOLUME_CYLINDER:
   case I3DVOLUME_CAPCYL:
      if(!force_color)
         color |= strong ? 0xff000000 : 0x80000000;
      scene->DebugDrawCylinder(GetMatrix(), non_uniform_scale.x, non_uniform_scale.z, color, (volume_type==I3DVOLUME_CAPCYL));
      break;

   case I3DVOLUME_BOX:
      {
         scene->SetRenderMatrix(GetGeoMatrix());
         dword color_line = color;
         dword color_tri = color;
         if(!force_color){
            color_line |= strong ? 0xff000000 : 0x80000000;
            color_tri |= strong ? 0x40000000 : 0x20000000;
         }
         DrawVolBox(!force_bsp ? color_line : 0, color_tri, scene);
      }
      break;
   case I3DVOLUME_RECTANGLE:
      {
         //drv->SetWorldMatrix(GetMatrix());
         if(!force_color)
            color |= strong ? 0x40000000 : 0x20000000;

         scene->SetRenderMatrix(GetGeoMatrix());
         scene->DrawTriangles(rect_edges, 4, I3DVC_XYZ, rect_fill_indicies, sizeof(rect_fill_indicies)/sizeof(word),
            color);
         if(!force_bsp){
            dword color_line = color;
            if(!force_color)
               color_line |= strong ? 0xff000000 : 0x80000000;
            scene->DrawLines(rect_edges, 4, rect_line_indicies, sizeof(rect_line_indicies)/sizeof(word), color_line);
         }
      }
      break;
   default: assert(0);
   }
   if(zw_en) drv->EnableZWrite(true);
}

//----------------------------

void I3D_volume::GetChecksum(float &matrix_sum, float &vertc_sum, dword &num_v) const{

   I3D_frame::GetChecksum(matrix_sum, vertc_sum, num_v);

   switch(volume_type){
   case I3DVOLUME_BOX:
      num_v = 8;
      break;
   case I3DVOLUME_RECTANGLE:
      num_v = 4;
      break;
   }
}

//----------------------------
//----------------------------

