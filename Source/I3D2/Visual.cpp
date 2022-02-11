/**//*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Visual.cpp
   Content: Visual frame.
--------------------------------------------------------*/

#include "all.h"
#include "visual.h"
#include "light.h"
#include "scene.h"
#include "camera.h"
#include "mesh.h"

//----------------------------

//#define ENABLE_ADVANCED_FOG
#ifndef GL
#define USE_SHARED_DEST_VB    //allocate dest VB on portions of huge VBs (VB alloc manager)
#endif


                              //report if too many lights shine on visual
                              // undef to disable also in debug build
#if defined _DEBUG || 1
# define DEBUG_REPORT_TOO_MANY_LIGHTS
#endif

//----------------------------
//----------------------------

static C_str LoadFile(const char *fn){
   C_str ret;
   C_cache fl;
   if(fl.open(fn, CACHE_READ)){
      ret.Assign(NULL, fl.filesize());
      fl.read(&ret[0], fl.filesize());
   }
   return ret;
}

//----------------------------

void I3D_dest_vertex_buffer::CreateD3DVB(dword fvf_flags_in, dword max_vertices_in, bool force_xyz_format
#ifndef GL
   , bool allow_hw_trans
#endif
   ){

   DestroyD3DVB();

   max_vertices = max_vertices_in;
   fvf_flags = fvf_flags_in;
   dword d3d_usage = D3DUSAGE_WRITEONLY;
   //d3d_usage |= D3DUSAGE_DYNAMIC;
   D3DPOOL mem_pool = D3DPOOL_DEFAULT;

#ifndef GL
   allow_hw_trans = (allow_hw_trans && drv->IsDirectTransform());
#endif

   if(!force_xyz_format){
                              //include transformed or untransformed 
                              // destination vertex format
#ifndef GL
      if(!allow_hw_trans){
         fvf_flags &= ~D3DFVF_POSITION_MASK;
         fvf_flags |= D3DFVF_XYZRHW;

                              //include specular component (due to vertex fog)
                              // (and possibly specular effects)
         fvf_flags |= D3DFVF_SPECULAR;
      }else
#endif
      {
         bool use_b1 = ((fvf_flags&D3DFVF_POSITION_MASK) == D3DFVF_XYZB1);
         fvf_flags &= ~D3DFVF_POSITION_MASK;
         if(!use_b1) 
            fvf_flags |= D3DFVF_XYZ;
         else
            fvf_flags |= D3DFVF_XYZB1;
      }
   }
#ifndef GL   
   if(!drv->IsHardware()){
      if(!drv->IsDirectTransform())
         d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
   }
   if(!allow_hw_trans){
                              //software transforming will be used
      d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
      mem_pool = D3DPOOL_SYSTEMMEM;
   }
#endif
   size_of_vertex = byte(::GetSizeOfVertex(fvf_flags));

#ifdef USE_SHARED_DEST_VB
   drv->AllocVertexBuffer(fvf_flags, mem_pool, d3d_usage, max_vertices, &D3D_vertex_buffer, &D3D_vertex_buffer_index, this, true);
#else
   HRESULT hr;
   hr = drv->GetDevice1()->CreateVertexBuffer(max_vertices*size_of_vertex, d3d_usage, fvf_flags, mem_pool, (IDirect3DVertexBuffer9**)&D3D_vertex_buffer, NULL);
   CHECK_D3D_RESULT("CreateVertexBuffer", hr);
   D3D_vertex_buffer_index = 0;
#endif
}

//----------------------------

void I3D_dest_vertex_buffer::DestroyD3DVB(){

   if(D3D_vertex_buffer){
#ifdef USE_SHARED_DEST_VB
      drv->FreeVertexBuffer(D3D_vertex_buffer, D3D_vertex_buffer_index, this);
#else
      D3D_vertex_buffer->Release();
#endif
      D3D_vertex_buffer = NULL;
      D3D_vertex_buffer_index = 0;
   }
}

//----------------------------
//----------------------------
                              
I3D_visual::I3D_visual(PI3D_driver d):
   I3D_frame(d),
   last_render_time(0),
   vis_flags(0),
   material_id(0),
   hide_dist(0.0f),
   alpha(255),
#ifndef GL
   vertex_buffer(drv),
#endif
   last_auto_lod(-1)
{
   drv->AddCount(I3D_CLID_VISUAL);
   type = FRAME_VISUAL;
   brightness[I3D_VIS_BRIGHTNESS_AMBIENT] = 128;
   brightness[I3D_VIS_BRIGHTNESS_NOAMBIENT] = 128;
   brightness[I3D_VIS_BRIGHTNESS_EMISSIVE] = 0;

   enum_mask = ENUMF_VISUAL;

   bound.bound_local.bbox.Invalidate();
   bound.bound_local.bsphere.pos.Zero();
   bound.bound_local.bsphere.radius = 0.0f;
   hr_bound.bound_local.bbox.Invalidate();

}

//----------------------------

I3D_visual::~I3D_visual(){

   drv->DecCount(I3D_CLID_VISUAL);
#ifndef GL
   vertex_buffer.DestroyD3DVB();
#endif
}

//----------------------------

/*
bool I3D_visual::ComputeBounds(){

   if(!(vis_flags&VISF_BOUNDS_VALID)){
      if(bound.bound_local.bbox.IsValid()){
                              //compute bounding sphere
         S_vector bbox_diagonal = (bound.bound_local.bbox.max - bound.bound_local.bbox.min) * .5f;
         bound.bound_local.bsphere.pos = bound.bound_local.bbox.min + bbox_diagonal;
         bound.bound_local.bsphere.radius = bbox_diagonal.Magnitude();
      }else{
         bound.bound_local.bsphere.pos.Zero();
         bound.bound_local.bsphere.radius = 0.0f;
      }
      vis_flags |= VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
   }
   return true;
}
*/

//----------------------------

bool I3D_visual::SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const{

   CPI3D_mesh_base mb = ((PI3D_visual)this)->GetMesh();
   if(mb)
      return mb->SaveCachedInfo(ck, mats);
   return false;
}

//----------------------------

bool I3D_visual::LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){
   return false;
}

//----------------------------

I3D_RESULT I3D_visual::Duplicate(CPI3D_frame frm){

   switch(frm->GetType1()){
   case FRAME_VISUAL:
      {
         CPI3D_visual vis = I3DCAST_CVISUAL(frm);
         material_id = vis->material_id;
         hide_dist = vis->hide_dist;
         alpha = vis->alpha;
         memcpy(brightness, vis->brightness, sizeof(brightness));
      }
      break;
   }
   I3D_RESULT ir = I3D_frame::Duplicate(frm);
   return ir;
}

//----------------------------

void I3D_visual::PrepareDestVB(I3D_mesh_base *mb, dword num_txt_stages){

   dword numv = mb->NumVertices1();
   if(!numv)
      return;

   bool use_3d_envcoords = false;
   vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
   vis_flags &= ~(VISF_USE_ENVMAP | VISF_USE_DETMAP | VISF_USE_EMBMMAP | VISF_USE_BUMPMAP);

   PI3D_texture_base tp_env = NULL, tp_det = NULL, txt_embm = NULL, txt_bump = NULL;
   S_vector2 detailmap_scale(0, 0);
   {
                              //search for any detail-mapped material
      for(int j=mb->NumFGroups1(); j--; ){
         PI3D_material mat = mb->GetFGroups1()[j].mat;
         if(!tp_env)
            tp_env = mat->GetTexture(MTI_ENVIRONMENT);
         if(!tp_det){
            tp_det = mat->GetTexture(MTI_DETAIL);
            if(tp_det)
               detailmap_scale = mat->GetDetailScale();
         }
         if(!txt_embm)
            txt_embm = mat->GetTexture(MTI_EMBM);
         if(!txt_bump)
            txt_bump = mat->GetTexture(MTI_NORMAL);
      }
      dword i = num_txt_stages;
      if(tp_det && (drv->GetFlags2()&DRVF2_DETAILMAPPING)){
         ++i;
         vis_flags |= VISF_USE_DETMAP;
      }
#if defined _DEBUG && 0
      if(txt_bump && i<drv->MaxSimultaneousTextures()){
         ++i;
         vis_flags |= VISF_USE_BUMPMAP;
      }
#endif
      if(tp_env && i<drv->MaxSimultaneousTextures() && (drv->GetFlags2()&DRVF2_ENVMAPPING) && (drv->GetFlags()&DRVF_SINGLE_PASS_MODULATE2X)){
         ++i;
         vis_flags |= VISF_USE_ENVMAP;
         use_3d_envcoords = (tp_env->GetTextureType()==TEXTURE_CUBIC);
         if(txt_embm && i < drv->MaxSimultaneousTextures() && (drv->GetFlags2()&DRVF2_USE_EMBM)){
            vis_flags |= VISF_USE_EMBMMAP;
            ++i;
         }
      }
#if defined _DEBUG && 0
      if(!tp_env && txt_embm && i<drv->MaxSimultaneousTextures()){
         vis_flags |= VISF_USE_EMBMMAP;
         ++i;
      }
#endif
   }

#ifndef GL
   if(drv->IsDirectTransform()){
      dword fvf = mb->vertex_buffer.GetFVFlags() & (D3DFVF_POSITION_MASK|D3DFVF_NORMAL|D3DFVF_PSIZE|D3DFVF_DIFFUSE|D3DFVF_SPECULAR);
      dword ntxt = num_txt_stages;
      if(vis_flags&VISF_USE_BUMPMAP)
         fvf |= D3DFVF_TEXTURE_SPACE;
      //if(vis_flags&VISF_USE_DETMAP) ++ntxt;
      fvf |= (ntxt<<D3DFVF_TEXCOUNT_SHIFT);
      vertex_buffer.CreateD3DVB(fvf, numv, true);

                              //copy src buffer to dest
      D3D_lock<byte> v_src(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.D3D_vertex_buffer_index,
         numv, mb->vertex_buffer.GetSizeOfVertex(), D3DLOCK_READONLY);
      D3D_lock<byte> v_dst(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.D3D_vertex_buffer_index, numv,
         vertex_buffer.GetSizeOfVertex(), 0);
      v_src.AssureLock();
      v_dst.AssureLock();
      {
         byte *src = v_src, *dst = v_dst;
         dword src_stride = mb->vertex_buffer.GetSizeOfVertex();
         dword dst_stride = vertex_buffer.GetSizeOfVertex();
         for(int i=numv; i--; dst += dst_stride, src += src_stride)
            memcpy(dst, src, src_stride);
      }
      vertex_buffer.vs_decl = drv->GetVSDeclaration(vertex_buffer.GetFVFlags());
   }else{
                              //create dest VB for Process-Vertices
      dword fvf = D3DFVF_DIFFUSE;

      dword ntxt = num_txt_stages;
      if(vis_flags&VISF_USE_DETMAP){
         ++ntxt;
      }
      if(vis_flags&VISF_USE_ENVMAP){
         if(vis_flags&VISF_USE_EMBMMAP)
            ++ntxt;
         if(use_3d_envcoords)
            fvf |= D3DFVF_TEXCOORDSIZE3(ntxt);
         ++ntxt;
      }
      fvf |= ntxt<<D3DFVF_TEXCOUNT_SHIFT;

      vertex_buffer.CreateD3DVB(fvf, mb->NumVertices1(), false, false);
      /*
      if(vis_flags&VISF_USE_DETMAP)
         GenerateDetailMapUV(num_txt_stages, detailmap_scale);
         */
      vertex_buffer.vs_decl = drv->GetVSDeclaration(mb->vertex_buffer.GetFVFlags());
   }
#endif
   vis_flags |= VISF_DEST_PREPARED;
}

//----------------------------

int I3D_visual::GetAutoLOD(const S_preprocess_context &pc, PI3D_mesh_base mb) const{

   int num_lods = mb->NumAutoLODs();
   if(!num_lods)
      return -1;
                              //get distance to camera
   float factor = (pc.viewer_pos - matrix(3)).Magnitude();
   //drv->DEBUG(C_fstr("distance: %.2f", factor));
                              //multiply by object's scale
   float scale = matrix(0).Magnitude();
   if(scale<MRG_ZERO)
      return 0;
   factor /= scale;
   factor *= pc.LOD_factor;

   const float *dist_range = mb->GetAutoLODDists();
   if(factor<dist_range[0])
      return -1;
   if(factor>=dist_range[1])
      factor = 1.0f;
   else
      factor = (factor - dist_range[0]) / (dist_range[1] - dist_range[0]);

   const C_auto_lod *auto_lods = mb->GetAutoLODs();
   const float lowest_quality = auto_lods[num_lods-1].ratio;
   factor *= (1.0f - lowest_quality);
   factor = 1.0f - Max(0.0f, Min(1.0f, factor));
   factor -= lowest_quality*.5f;

   for(int i=0; i<num_lods; i++){
      if(auto_lods[i].ratio < factor)
         break;
   }
   assert(i!=-1);
   --i;
   return i;
}

//----------------------------
#ifdef GL

class C_this_gl_program: public C_gl_program{
public:
   C_this_gl_program(I3D_driver &d):
      C_gl_program(d)
   {}
   int u_mat_view_proj, u_sampler, a_pos, a_normal, a_tex0, u_lights, u_num_lights;
   virtual void BuildStoreAttribId(dword index, dword id){
      *(((int*)&u_mat_view_proj)+index) = id;
   }
};

#endif
//----------------------------

static void FillLightVSConstants(PI3D_driver drv, const I3D_driver::S_vs_shader_entry &se, dword num_light_params, const S_vectorw *light_params){

   dword prev_index = se.vscs_light_param[0] - 1;
   dword num = 0;
   for(dword i=0; i<num_light_params; i++){
      dword indx = se.vscs_light_param[i];
      assert(indx < 96);
      if(indx == prev_index+1){
         ++num;
         ++prev_index;
      }else{
         drv->SetVSConstant(se.vscs_light_param[i-num], &light_params[i-num], num);
         prev_index = indx;
         num = 1;
      }
   }
   drv->SetVSConstant(se.vscs_light_param[i-num], &light_params[i-num], num);
}

//----------------------------

#ifdef DEBUG_REPORT_TOO_MANY_LIGHTS
# define DEBUG_CHECK_LIGHT_COUNT \
   if(light_count==MAX_VS_LIGHTS){ \
      drv->DEBUG(C_fstr("vis %s: too many lights", (const char*)name), 2); \
      i=0; \
      continue; \
   } 
#else
# define DEBUG_CHECK_LIGHT_COUNT \
   if(light_count==MAX_VS_LIGHTS){ \
      i=0; continue; \
   } 
#endif

//----------------------------

dword I3D_visual::PrepareVSLighting(CPI3D_material mat, I3D_driver::S_vs_shader_entry_in &se_in, const S_render_primitive &rp, const S_matrix &m_trans,
   const S_matrix &m_inv_trans, dword flags, S_vectorw light_params[], C_buffer<E_VS_FRAGMENT> *save_light_fragments, C_buffer<S_vectorw> *save_light_params) const{

   dword light_count = 0;
   dword num_light_params = 0;
   //const S_vector v_up(.0f, 1.0f, .0f);

   if(flags&VSPREP_MAKELIGHTING){

      const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
      const I3D_bbox &bbox = bound.bound_local.bbox;
      const S_vector &diffuse = mat->GetDiffuse1();

      float f_tmp, *inv_scale = NULL;

      S_vectorw light_value;
      S_vector &light_base = light_value;
      light_base = mat->GetEmissive1();
      if(brightness[I3D_VIS_BRIGHTNESS_EMISSIVE]){
         float em = float(brightness[I3D_VIS_BRIGHTNESS_EMISSIVE]) * R_128;
         light_base.x += em;
         light_base.y += em;
         light_base.z += em;
      }
      light_value.w = mat->GetAlpha1() * (float)rp.alpha * R_255;

      dword begin_index = se_in.num_fragments;

      se_in.AddFragment(VSF_LIGHT_BEGIN);

      //dword light_flags = I3DLIGHTMODE_VERTEX;
      //if(flags&VSPREP_LIGHT_LM_DYNAMIC)
      //   light_flags = I3DLIGHTMODE_DYNAMIC_LM;

                              //for VISF_USE_OVERRIDE_LIGHT set user defined lighr and compute only lm dynamic adjust
      if(vis_flags&VISF_USE_OVERRIDE_LIGHT){
         //S_vector light_color(override_light[0] * R_128, override_light[1] * R_128, override_light[2] * R_128);
         S_vector light_color;
         GetOverrideLight(light_color);
         light_base += light_color;
         flags |= VSPREP_LIGHT_LM_DYNAMIC;
      }

      const C_vector<PI3D_light> &sct_lights = !(flags&VSPREP_LIGHT_LM_DYNAMIC) ?
         rp.sector->GetLights_vertex() : rp.sector->GetLights_dyn_LM();
      const PI3D_light *lpptr = sct_lights.size() ? &sct_lights.front() : NULL;

      for(int i=sct_lights.size(); i--; ){
         PI3D_light lp = *lpptr++;
         assert(!lp->IsFogType());

                              //check if light is on
         if(!lp->IsOn1())
            continue;

         I3D_LIGHTTYPE lt = lp->GetLightType1();
         switch(lt){

         case I3DLIGHT_AMBIENT:
            light_base += lp->color * diffuse * (lp->power * float(brightness[I3D_VIS_BRIGHTNESS_AMBIENT]) * R_128);
            break;

         case I3DLIGHT_POINTAMBIENT:
            {
               const S_matrix &lmat = lp->GetMatrixDirect();

               S_vector dir = lmat(3) - bsphere.pos;
               float dist_2 = dir.Dot(dir);
               if(dist_2 > lp->range_f_scaled_2)
                  break;
               float a;
               if(dist_2 < (lp->range_n_scaled*lp->range_n_scaled)){
                  a = 1.0f;
               }else{
                  if(IsAbsMrgZero(lp->range_delta_scaled))
                     break;
                  float dist = I3DSqrt(dist_2);
                  a = (lp->range_f_scaled - dist) / lp->range_delta_scaled;
               }
               light_base += lp->color * diffuse * (a * lp->power * float(brightness[I3D_VIS_BRIGHTNESS_AMBIENT]) * R_128);
            }
            break;

         case I3DLIGHT_DIRECTIONAL:
         case I3DLIGHT_POINT:
         case I3DLIGHT_SPOT:
         //case I3DLIGHT_LAYEREDFOG:
         //case I3DLIGHT_POINTAMBIENT:
            {
               S_vector color = lp->color * diffuse * (lp->power * drv->vertex_light_mult * float(brightness[I3D_VIS_BRIGHTNESS_NOAMBIENT])*R_128);
               if(color.IsNull())
                  break;
               switch(lt){
               case I3DLIGHT_DIRECTIONAL:
                  {
#if 1
                     if(mat->Is2Sided1()){
                        light_base += color * .75f;
                        continue;
                     }
#endif
                     DEBUG_CHECK_LIGHT_COUNT;

                     S_vectorw *vw = &light_params[num_light_params];
                     if(!(flags&VSPREP_WORLD_SPACE)){
                        vw[0] = lp->normalized_dir % m_inv_trans;
                        vw[0].Normalize();
                     }else{
                        vw[0] = lp->normalized_dir;
                     }
                     vw[1] = color;
                     vw[0].w = 0.0f;
                     vw[1].w = 0.0f;
                     num_light_params += 2;
                     se_in.AddFragment((flags&VSPREP_WORLD_SPACE) ? VSF_LIGHT_DIR_WS : VSF_LIGHT_DIR_OS);
                  }
                  break;

               case I3DLIGHT_POINT:
               case I3DLIGHT_SPOT:
               //case I3DLIGHT_POINTAMBIENT:
                  {
                     const S_matrix &lmat = lp->GetMatrix();
                     S_vector light_dir = lmat(3) - bsphere.pos;
                     float light_dist_2 = light_dir.Square();
#if 1
                     if(mat->Is2Sided1()){
                              //treat as classical point light style
                        if(light_dist_2 > lp->range_f_scaled_2)
                           break;
                        float a;
                        if(light_dist_2 < (lp->range_n_scaled*lp->range_n_scaled)){
                           a = 1.0f;
                        }else{
                           if(IsAbsMrgZero(lp->range_delta_scaled))
                              continue;
                           float dist = I3DSqrt(light_dist_2);
                           a = (lp->range_f_scaled - dist) / lp->range_delta_scaled;
                        }
                        light_base += color * a * .75f;

                        continue;
                     }
#endif

                                 //fast reject lights too far from object
                     float r_sum = lp->range_f_scaled + bsphere.radius;
                     if((r_sum*r_sum) <= light_dist_2)
                        continue;

                     S_vector light_loc_pos = lmat(3) * m_inv_trans;
                              //detailed bounding-box test
                     {
                        if(!inv_scale){
                           f_tmp = m_inv_trans(0).Magnitude();
                           inv_scale = &f_tmp;
                        }
                        float r = lp->range_f_scaled * *inv_scale;
                        if(((bbox.min.x - light_loc_pos.x) > r) ||
                           ((bbox.min.y - light_loc_pos.y) > r) ||
                           ((bbox.min.z - light_loc_pos.z) > r) ||
                           ((light_loc_pos.x - bbox.max.x) > r) ||
                           ((light_loc_pos.y - bbox.max.y) > r) ||
                           ((light_loc_pos.z - bbox.max.z) > r))
                           continue;
                     }
                     if(lt==I3DLIGHT_SPOT && lp->cone_out<(PI*.99f)){
                              //check if our bounding sphere is within the spot area
                        if(light_dist_2 > bsphere.radius*bsphere.radius){
                              //light pos is outside our bounding sphere, check cone
                              //get closest point from sphere to cone's axis
                           float f = -lp->normalized_dir.Dot(light_dir);
                           S_vector p = lmat(3) + lp->normalized_dir * f;
                           //drv->DebugPoint(p);

                              //get point on line lying on cone's body,
                              // which leads from cone's top and is closest to sphere
                           S_vector dir = (bsphere.pos - p);
                           float dir_size = dir.Magnitude();

                           if(!IsAbsMrgZero(dir_size)){
                              f *= lp->outer_tan;
                              S_vector point_on_cone = p + dir * (f / dir_size);
                              //drv->DebugPoint(point_on_cone);

                              S_vector cone_line_dir = point_on_cone - lmat(3);
                              float cone_line_dir_size_2 = cone_line_dir.Square();

                              if(!IsAbsMrgZero(cone_line_dir_size_2)){
                              //get closest point from sphere to this line
                                 float f1 = -cone_line_dir.Dot(light_dir) / cone_line_dir_size_2;
                                 S_vector closest_point_on_cone = lmat(3) + cone_line_dir * f1;

                                 //drv->DebugPoint(closest_point_on_cone);
                                 S_vector dir_tmp = closest_point_on_cone - bsphere.pos;
                                 float d_sphere_to_cone_2 = dir_tmp.Square();
                                 //drv->DEBUG(I3DSqrt(d_sphere_to_cone_2));
                                 if(d_sphere_to_cone_2 > (bsphere.radius*bsphere.radius)){
                                    if(f <= dir_size){
                                       continue;
                                    }
                                 }
                              }
                           }
                        }
                     }

                     DEBUG_CHECK_LIGHT_COUNT;

                     S_vectorw *vw = &light_params[num_light_params];
                              //constant regs:
                              // c[+0].xyz ... light pos
                              // c[+0].w ... light far range
                              // c[+1].xyz ... light color
                              // c[+1].w ... light range delta
                              //spot:
                              // c[+2].xyz ... light normalized dir
                              // c[+2].w ... light outer cone cos

                     S_vector &c_color = vw[1];
                     c_color = color;
                     float range_delta = lp->range_delta_scaled;
                     float &c_far_range = vw[0].w;
                     E_VS_FRAGMENT fid;
                     if(!(flags&VSPREP_WORLD_SPACE)){
                        if(!inv_scale){
                           f_tmp = m_inv_trans(0).Magnitude();
                           inv_scale = &f_tmp;
                        }
                        vw[0] = light_loc_pos;
                        c_far_range = lp->range_f_scaled * (*inv_scale);
                        range_delta *= (*inv_scale);

                        if(lt==I3DLIGHT_POINT){
                           /*
                           if(md && md->two_sided){
                                       //use ambient for 2-sided mats,
                                       // but scaled to half
                              fid = I3D_driver::VSF_LIGHT_POINT_AMB_OS;
                              c_color *= .5f;
                           }else
                              */
                              fid = VSF_LIGHT_POINT_OS;
                        }else{
                           fid = VSF_LIGHT_SPOT_OS;
                        }
                     }else{
                        vw[0] = lmat(3);
                        c_far_range = lp->range_f_scaled;
                        fid = (lt==I3DLIGHT_POINT) ? VSF_LIGHT_POINT_WS : VSF_LIGHT_SPOT_WS;
                     }
                     se_in.AddFragment(fid);

                     float &c_r_range_delta = vw[1].w;
                     if(!IsAbsMrgZero(range_delta))
                        c_r_range_delta = 1.0f / range_delta;
                     else
                        c_r_range_delta = 1e+8f;

                     num_light_params += 2;

                     if(lt==I3DLIGHT_SPOT){
                        S_vectorw &vws = light_params[num_light_params++];
                        S_vector &dir = vws;
                        dir = lp->normalized_dir;
                        if(!(flags&VSPREP_WORLD_SPACE)){
                           dir %= m_inv_trans;
                           dir.Normalize();
                        }
                              //get light multiplier causing maximal intensity to be at inner range
                        float cone_delta = lp->inner_cos - lp->outer_cos;
                        float dir_mult;
                        if(!IsAbsMrgZero(cone_delta))
                           dir_mult = 1.0f / cone_delta;
                        else
                           dir_mult = 1e+8f;
                        //vws.w = (PI*2.0f) / lp->cone_out;
                        vws.w = -lp->outer_cos * dir_mult;
                              //pre-multiply by .5
                        //dir *= .5f;
                        dir *= dir_mult;
                     }
                  }
                  break;
               }
               ++light_count;
               /*
                              //check for light count overflow
   #ifndef DEBUG_REPORT_TOO_MANY_LIGHTS
               if(light_count==MAX_VS_LIGHTS) i = 0;
   #endif
               */
            }
            break;
         }
      }
      //drv->DEBUG((int)light_count);

      if(drv->IsDirectTransform() && !light_count && (light_base).IsNull() && light_value.w==1.0f){
         se_in.PopFragment();
      }else{
         rp.flags |= RP_FLAG_DIFFUSE_VALID;

         if(flags&VSPREP_LIGHT_MUL_ALPHA)
            se_in.AddFragment(VSF_LIGHT_END_ALPHA);
         else
            se_in.AddFragment(VSF_LIGHT_END);

         light_base *= drv->vertex_light_mult;
         drv->SetVSConstant(VSC_AMBIENT, &light_value);
      }

                              //save light fragments now
      if(save_light_fragments){
         save_light_fragments->assign(&se_in.fragment_code[begin_index], &se_in.fragment_code[se_in.num_fragments]);
                              //save light params
         save_light_params->assign(num_light_params+1);
         memcpy(save_light_params->begin(), light_params, num_light_params*sizeof(S_vectorw));
         save_light_params->back() = light_value;
      }
   }else
   if(save_light_fragments){
                              //use saved light fragments now
      dword num = save_light_fragments->size();
      if(num){
                              //light count = num fragments minus 2 (VSF_LIGHT_BEGIN and VSF_LIGHT_END)
         light_count += num - 2;
         rp.flags |= RP_FLAG_DIFFUSE_VALID;
         for(dword i=0; i<num; i++)
            se_in.AddFragment((*save_light_fragments)[i]);
         num_light_params += save_light_params->size()-1;
      }
   }

   if(drv->GetFlags()&DRVF_USEFOG){
#ifdef ENABLE_ADVANCED_FOG
      const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);

      S_matrix m_tmp;

      se_in.AddFragment(VSF_FOG_BEGIN);

      const C_vector<PI3D_light> &sct_fogs = rp.sector->GetFogs();
      if(sct_fogs.size()){
         const PI3D_light *fpptr = &sct_fogs.front();
         for(int i=sct_fogs.size(); i--; ){
            PI3D_light lp = *fpptr++;
            assert(lp->IsFogType());

            if(!(lp->IsOn1() && (lp->GetLightFlags()&I3DLIGHTMODE_VERTEX)))
               continue;

            I3D_LIGHTTYPE lt = lp->GetLightType1();
            switch(lt){
            case I3DLIGHT_LAYEREDFOG:
               {
                  const S_matrix &m = lp->GetMatrix();

                                    //fast reject lights too far from object
                  S_vector l_dist = bsphere.pos - m(3);
                  float dist_2 = l_dist.Dot(l_dist);
                  float r_sum = lp->range_f_scaled + bsphere.radius;
                  if((r_sum*r_sum) <= dist_2)
                     continue;
                  {              //todo: optimize - use this plane later
                     S_plane pl1;
                     pl1.normal = -m(1);
                     float light_scale = pl1.normal.Magnitude();
                     float height = lp->range_n * light_scale;
                     pl1.normal /= light_scale;
                     pl1.d = -m(3).Dot(pl1.normal) + height;
                     float dist_to_plane = bsphere.pos.DistanceToPlane(pl1);
                     if(dist_to_plane < -bsphere.radius || dist_to_plane > bsphere.radius+height)
                        continue;
                  }

                  DEBUG_CHECK_LIGHT_COUNT;

                  float r_light_scale;
                  S_vectorw *vw = (S_vectorw*)&light_params[num_light_params];
                  num_light_params += 3;

                  float power = lp->power;
                  S_plane &pl = *(S_plane*)&vw[0];
                  if(!(flags&VSPREP_WORLD_SPACE)){
                     se_in.AddFragment(VSF_FOG_LAYERED_OS);

                     S_vector loc_pos = m(3) * m_inv_trans;
                     pl.normal = -m(1) % m_inv_trans;
                     float height = lp->range_n * pl.normal.Magnitude();
                     r_light_scale = !IsAbsMrgZero(height) ? (1.0f / height) : 1e+8f;
                     pl.normal *= power;
                     pl.d = -(loc_pos.Dot(pl.normal)) + height * power;

                     vw[2] = loc_pos;
                     vw[2].w = lp->range_f_scaled * m_inv_trans(2).Magnitude();
                  }else{
                     se_in.AddFragment(VSF_FOG_LAYERED_WS);
                     pl.normal = -m(1);
                     float height = lp->range_n * pl.normal.Magnitude();
                     r_light_scale = !IsAbsMrgZero(height) ? (1.0f / height) : 1e+8f;
                     pl.normal *= power;
                     pl.d = -(m(3).Dot(pl.normal)) + height * power;

                     vw[2] = m(3);
                     vw[2].w = lp->range_f_scaled;
                  }
                                          //keep range squared
                  vw[2].w *= vw[2].w;
                                    //scale
                  pl.normal *= r_light_scale;
                  pl.d *= r_light_scale;

                  vw[1] = lp->color * lp->power;
                  vw[1].w = lp->power;


                  rp.flags |= RP_FLAG_FOG_VALID;
                                 //check for light count overflow
                  if(++light_count==MAX_VS_LIGHTS) i = 0;
               }
               break;

            case I3DLIGHT_FOG:
               {
                  se_in.AddFragment((flags&VSPREP_WORLD_SPACE) ? VSF_FOG_HEIGHT_WS : VSF_FOG_HEIGHT_OS);
                  rp.flags |= RP_FLAG_FOG_VALID;
                                       //setup fog constants
                  S_vectorw v4;
                  v4.w = 0.0f;
                  float &fog_range_n = v4.x, &fog_range_f = v4.y, &fog_range_r_delta = v4.z;
                  fog_range_f = lp->range_f;
                  if(!FloatToInt(fog_range_f))
                     fog_range_f = rp.scene->GetActiveCamera1()->GetFCP();
                  fog_range_n = lp->range_n * fog_range_f;
                  fog_range_r_delta = 1.0f / Max(.01f, fog_range_f - fog_range_n);
                  drv->SetVSConstant(VSC_FOGPARAMS, &v4);
               }
               break;

            default:
               continue;
            }
         }
      }
      if(rp.flags&RP_FLAG_FOG_VALID){
                              //ok, write result of fog
         se_in.AddFragment(VSF_FOG_END);
      }else{
                              //no fog light, remove preparation instruction
         se_in.PopFragment();
      }
#else//ENABLE_ADVANCED_FOG
      PI3D_light lp = rp.sector->GetFogLight();
      if(lp && lp->IsOn1() && (lp->I3D_light::GetLightFlags()&I3DLIGHTMODE_VERTEX)){
         se_in.AddFragment(!(flags&VSPREP_WORLD_SPACE) ?
            VSF_FOG_SIMPLE_OS :
            VSF_FOG_SIMPLE_WS);
         rp.flags |= RP_FLAG_FOG_VALID;
                              //setup fog constants
         S_vectorw v4;
         float &fog_range_n = v4.x, &fog_range_f = v4.y, &fog_range_r_delta = v4.z;
         v4.w = 0.0f;
         fog_range_f = lp->range_f;
         if(IsAbsMrgZero(fog_range_f))
            fog_range_f = rp.scene->GetActiveCamera1()->GetFCP();
         fog_range_n = lp->range_n * fog_range_f;
         fog_range_r_delta = 1.0f / Max(.01f, fog_range_f - fog_range_n);
         drv->SetVSConstant(VSC_FOGPARAMS, &v4);
      }
#endif//!ENABLE_ADVANCED_FOG
   }
                              //enable fog, if it was computed
   drv->EnableFog(rp.flags&RP_FLAG_FOG_VALID);
   return num_light_params;
}

//----------------------------
#ifdef GL
dword I3D_visual::GlPrepareVSLighting(CPI3D_material mat, const S_render_primitive &rp, const S_matrix &m_trans, const S_matrix &m_inv_trans, dword flags,
   S_vectorw light_params[], C_buffer<S_vectorw> *save_light_params) const{

   dword light_count = 0;

   if(flags&VSPREP_MAKELIGHTING){
      dword num_light_params = 0;

      const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
      const I3D_bbox &bbox = bound.bound_local.bbox;
      const S_vector &diffuse = mat->GetDiffuse1();

      float f_tmp, *inv_scale = NULL;

      S_vectorw light_value;
      S_vector &light_base = light_value;
      light_base = mat->GetEmissive1();
      if(brightness[I3D_VIS_BRIGHTNESS_EMISSIVE]){
         float em = float(brightness[I3D_VIS_BRIGHTNESS_EMISSIVE]) * R_128;
         light_base.x += em;
         light_base.y += em;
         light_base.z += em;
      }
      light_value.w = mat->GetAlpha1() * (float)rp.alpha * R_255;
                              //for VISF_USE_OVERRIDE_LIGHT set user defined lighr and compute only lm dynamic adjust
      if(vis_flags&VISF_USE_OVERRIDE_LIGHT){
         //S_vector light_color(override_light[0] * R_128, override_light[1] * R_128, override_light[2] * R_128);
         S_vector light_color;
         GetOverrideLight(light_color);
         light_base += light_color;
         flags |= VSPREP_LIGHT_LM_DYNAMIC;
      }

      const C_vector<PI3D_light> &sct_lights = !(flags&VSPREP_LIGHT_LM_DYNAMIC) ?
         rp.sector->GetLights_vertex() : rp.sector->GetLights_dyn_LM();
      const PI3D_light *lpptr = sct_lights.size() ? &sct_lights.front() : NULL;

      for(int i=sct_lights.size(); i--; ){
         PI3D_light lp = *lpptr++;
         assert(!lp->IsFogType());

                              //check if light is on
         if(!lp->IsOn1())
            continue;

         I3D_LIGHTTYPE lt = lp->GetLightType1();
         switch(lt){

         case I3DLIGHT_AMBIENT:
            light_base += lp->color * diffuse * (lp->power * float(brightness[I3D_VIS_BRIGHTNESS_AMBIENT]) * R_128);
            break;

         case I3DLIGHT_POINTAMBIENT:
            {
               const S_matrix &lmat = lp->GetMatrixDirect();

               S_vector dir = lmat(3) - bsphere.pos;
               float dist_2 = dir.Dot(dir);
               if(dist_2 > lp->range_f_scaled_2)
                  break;
               float a;
               if(dist_2 < (lp->range_n_scaled*lp->range_n_scaled)){
                  a = 1.0f;
               }else{
                  if(IsAbsMrgZero(lp->range_delta_scaled))
                     break;
                  float dist = I3DSqrt(dist_2);
                  a = (lp->range_f_scaled - dist) / lp->range_delta_scaled;
               }
               light_base += lp->color * diffuse * (a * lp->power * float(brightness[I3D_VIS_BRIGHTNESS_AMBIENT]) * R_128);
            }
            break;

         case I3DLIGHT_DIRECTIONAL:
         case I3DLIGHT_POINT:
         //case I3DLIGHT_SPOT:
            {
               S_vector color = lp->color * diffuse * (lp->power * float(brightness[I3D_VIS_BRIGHTNESS_NOAMBIENT])*R_128);
               if(color.IsNull())
                  break;
               switch(lt){
               case I3DLIGHT_DIRECTIONAL:
                  {
#if 1
                     if(mat->Is2Sided1()){
                        light_base += color * .75f * .5f;
                        continue;
                     }
#endif
                     DEBUG_CHECK_LIGHT_COUNT;

                     S_vectorw *vw = &light_params[num_light_params];
                     if(!(flags&VSPREP_WORLD_SPACE)){
                        vw[0] = lp->normalized_dir % m_inv_trans;
                        vw[0].Normalize();
                     }else{
                        vw[0] = lp->normalized_dir;
                     }
                     vw[1] = color;
                     vw[0].w = 0.0f;
                     vw[1].w = 0.0f;
                     num_light_params += 2;
                     ++light_count;
                  }
                  break;

               case I3DLIGHT_POINT:
               //case I3DLIGHT_SPOT:
                  {
                     const S_matrix &lmat = lp->GetMatrix();
                     S_vector light_dir = lmat(3) - bsphere.pos;
                     float light_dist_2 = light_dir.Square();
#if 1
                     if(mat->Is2Sided1()){
                              //treat as classical point light style
                        if(light_dist_2 > lp->range_f_scaled_2)
                           break;
                        float a;
                        if(light_dist_2 < (lp->range_n_scaled*lp->range_n_scaled)){
                           a = 1.0f;
                        }else{
                           if(IsAbsMrgZero(lp->range_delta_scaled))
                              continue;
                           float dist = I3DSqrt(light_dist_2);
                           a = (lp->range_f_scaled - dist) / lp->range_delta_scaled;
                        }
                        light_base += color * a * .75f * .5f;

                        continue;
                     }
#endif

                                 //fast reject lights too far from object
                     float r_sum = lp->range_f_scaled + bsphere.radius;
                     if((r_sum*r_sum) <= light_dist_2)
                        continue;

                     S_vector light_loc_pos = lmat(3) * m_inv_trans;
                              //detailed bounding-box test
                     {
                        if(!inv_scale){
                           f_tmp = m_inv_trans(0).Magnitude();
                           inv_scale = &f_tmp;
                        }
                        float r = lp->range_f_scaled * *inv_scale;
                        if(((bbox.min.x - light_loc_pos.x) > r) ||
                           ((bbox.min.y - light_loc_pos.y) > r) ||
                           ((bbox.min.z - light_loc_pos.z) > r) ||
                           ((light_loc_pos.x - bbox.max.x) > r) ||
                           ((light_loc_pos.y - bbox.max.y) > r) ||
                           ((light_loc_pos.z - bbox.max.z) > r))
                           continue;
                     }
                     /*
                     if(lt==I3DLIGHT_SPOT && lp->cone_out<(PI*.99f)){
                              //check if our bounding sphere is within the spot area
                        if(light_dist_2 > bsphere.radius*bsphere.radius){
                              //light pos is outside our bounding sphere, check cone
                              //get closest point from sphere to cone's axis
                           float f = -lp->normalized_dir.Dot(light_dir);
                           S_vector p = lmat(3) + lp->normalized_dir * f;
                           //drv->DebugPoint(p);

                              //get point on line lying on cone's body,
                              // which leads from cone's top and is closest to sphere
                           S_vector dir = (bsphere.pos - p);
                           float dir_size = dir.Magnitude();

                           if(!IsAbsMrgZero(dir_size)){
                              f *= lp->outer_tan;
                              S_vector point_on_cone = p + dir * (f / dir_size);
                              //drv->DebugPoint(point_on_cone);

                              S_vector cone_line_dir = point_on_cone - lmat(3);
                              float cone_line_dir_size_2 = cone_line_dir.Square();

                              if(!IsAbsMrgZero(cone_line_dir_size_2)){
                              //get closest point from sphere to this line
                                 float f1 = -cone_line_dir.Dot(light_dir) / cone_line_dir_size_2;
                                 S_vector closest_point_on_cone = lmat(3) + cone_line_dir * f1;

                                 //drv->DebugPoint(closest_point_on_cone);
                                 S_vector dir_tmp = closest_point_on_cone - bsphere.pos;
                                 float d_sphere_to_cone_2 = dir_tmp.Square();
                                 //drv->DEBUG(I3DSqrt(d_sphere_to_cone_2));
                                 if(d_sphere_to_cone_2 > (bsphere.radius*bsphere.radius)){
                                    if(f <= dir_size){
                                       continue;
                                    }
                                 }
                              }
                           }
                        }
                     }
                     */
                     DEBUG_CHECK_LIGHT_COUNT;

                     S_vectorw *vw = &light_params[num_light_params];
                              //constant regs:
                              // c[+0].xyz ... light pos
                              // c[+0].w ... light far range
                              // c[+1].xyz ... light color
                              // c[+1].w ... light range delta
                              //spot:
                              // c[+2].xyz ... light normalized dir
                              // c[+2].w ... light outer cone cos

                     S_vector &c_color = vw[1];
                     c_color = color;
                     float range_delta = lp->range_delta_scaled;
                     float &c_far_range = vw[0].w;
                     if(!(flags&VSPREP_WORLD_SPACE)){
                        if(!inv_scale){
                           f_tmp = m_inv_trans(0).Magnitude();
                           inv_scale = &f_tmp;
                        }
                        vw[0] = light_loc_pos;
                        c_far_range = lp->range_f_scaled * (*inv_scale);
                        range_delta *= (*inv_scale);
                     }else{
                        vw[0] = lmat(3);
                        c_far_range = lp->range_f_scaled;
                     }
                     float &c_r_range_delta = vw[1].w;
                     if(!IsAbsMrgZero(range_delta))
                        c_r_range_delta = 1.0f / range_delta;
                     else
                        c_r_range_delta = 1e+8f;

                     num_light_params += 2;
                     ++light_count;
                     /*
                     if(lt==I3DLIGHT_SPOT){
                        S_vectorw &vws = light_params[num_light_params++];
                        S_vector &dir = vws;
                        dir = lp->normalized_dir;
                        if(!(flags&VSPREP_WORLD_SPACE)){
                           dir %= m_inv_trans;
                           dir.Normalize();
                        }
                              //get light multiplier causing maximal intensity to be at inner range
                        float cone_delta = lp->inner_cos - lp->outer_cos;
                        float dir_mult;
                        if(!IsAbsMrgZero(cone_delta))
                           dir_mult = 1.0f / cone_delta;
                        else
                           dir_mult = 1e+8f;
                        //vws.w = (PI*2.0f) / lp->cone_out;
                        vws.w = -lp->outer_cos * dir_mult;
                              //pre-multiply by .5
                        //dir *= .5f;
                        dir *= dir_mult;
                     }
                     */
                  }
                  break;
               }
               /*
                              //check for light count overflow
   #ifndef DEBUG_REPORT_TOO_MANY_LIGHTS
               if(light_count==MAX_VS_LIGHTS) i = 0;
   #endif
               */
            }
            break;
         }
      }
      light_params[num_light_params++] = light_value;

      if(light_count || !(light_base).IsNull() || light_value.w!=1.0f){
         rp.flags |= RP_FLAG_DIFFUSE_VALID;
      }

      if(save_light_params){
                              //save light params
         save_light_params->assign(num_light_params);
         memcpy(save_light_params->begin(), light_params, num_light_params*sizeof(S_vectorw));
      }
   }else
   if(save_light_params && save_light_params->size()){
                              //light count = num fragments minus 2 (VSF_LIGHT_BEGIN and VSF_LIGHT_END)
      light_count = (save_light_params->size()-1)/2;
      rp.flags |= RP_FLAG_DIFFUSE_VALID;
   }
   /*
   if(drv->GetFlags()&DRVF_USEFOG){
      PI3D_light lp = rp.sector->GetFogLight();
      if(lp && lp->IsOn1() && (lp->I3D_light::GetLightFlags()&I3DLIGHTMODE_VERTEX)){
         se_in.AddFragment(!(flags&VSPREP_WORLD_SPACE) ?
            VSF_FOG_SIMPLE_OS :
            VSF_FOG_SIMPLE_WS);
         rp.flags |= RP_FLAG_FOG_VALID;
                              //setup fog constants
         S_vectorw v4;
         float &fog_range_n = v4.x, &fog_range_f = v4.y, &fog_range_r_delta = v4.z;
         v4.w = 0.0f;
         fog_range_f = lp->range_f;
         if(IsAbsMrgZero(fog_range_f))
            fog_range_f = rp.scene->GetActiveCamera1()->GetFCP();
         fog_range_n = lp->range_n * fog_range_f;
         fog_range_r_delta = 1.0f / Max(.01f, fog_range_f - fog_range_n);
         drv->SetVSConstant(VSC_FOGPARAMS, &v4);
      }
   }
                              //enable fog, if it was computed
   drv->EnableFog(rp.flags&RP_FLAG_FOG_VALID);
   */
   return light_count;
}
#endif
//----------------------------

const I3D_driver::S_vs_shader_entry *I3D_visual::PrepareVertexShader(CPI3D_material mat, dword num_txt_stages, I3D_driver::S_vs_shader_entry_in &se_in,
   const S_render_primitive &rp, E_RENDERVIEW_MODE rv_mode, dword flags, const S_matrix *m_trans,
   C_buffer<E_VS_FRAGMENT> *save_light_fragments, C_buffer<S_vectorw> *save_light_params
#ifdef GL
   , C_buffer<S_vectorw> *gl_save_light_params
#endif
   ) const{

   bool alt_m_trans = (m_trans!=NULL);
   if(!m_trans)
      m_trans = &matrix;

                              //add transform code fragment
   if(flags&VSPREP_TRANSFORM)
      se_in.AddFragment(VSF_TRANSFORM);

   bool object_space = !(flags&VSPREP_WORLD_SPACE);

   S_vectorw light_params[MAX_VS_LIGHTS * 3];
   dword num_light_params = 0;
#ifdef GL
   S_vectorw gl_light_params[MAX_VS_LIGHTS * 2 + 1];
   dword gl_num_lights = 0;
#endif
#ifndef GL
   if(rv_mode!=RV_SHADOW_CASTER)
#endif
   {
                              //prepare lighting info
      S_matrix m_tmp;
      if(alt_m_trans)
         m_tmp = ~(*m_trans);
      const S_matrix &inv_matrix = !alt_m_trans ? GetInvMatrix1() : m_tmp;
      num_light_params = PrepareVSLighting(mat, se_in, rp, *m_trans, inv_matrix, flags, light_params, save_light_fragments, save_light_params);
#ifdef GL
      gl_num_lights = GlPrepareVSLighting(mat, rp, *m_trans, inv_matrix, flags, gl_light_params, gl_save_light_params);
#endif
   }

   dword stage = num_txt_stages;

   if(flags&VSPREP_COPY_UV)
      se_in.CopyUV();
   bool do_env_mapping = false;
   bool do_detailmap = false;
   bool do_bumpmap = false;
   if(!(flags&VSPREP_NO_DETAIL_MAPS)
#ifndef GL
      && rv_mode!=RV_SHADOW_CASTER
#endif
      ){
      if(vis_flags&VISF_USE_DETMAP){
         do_detailmap = true;
         if(do_detailmap){
            se_in.AddFragment((E_VS_FRAGMENT)(VSF_PICK_UV_0));
            se_in.AddFragment(VSF_MULTIPLY_UV_BY_XY);
            se_in.AddFragment((E_VS_FRAGMENT)(VSF_STORE_UV_0 + stage));
            ++stage;
            do_detailmap = true;
         }
      }
#if defined _DEBUG && 0
      if((vis_flags&VISF_USE_BUMPMAP) && stage<drv->NumSimultaneousTextures()){
         se_in.AddFragment(VSF_PICK_UV_0);
         se_in.AddFragment((E_VS_FRAGMENT)(VSF_STORE_UV_0 + stage));
         ++stage;
         do_bumpmap = true;

         se_in.AddFragment(VSF_BUMP_OS);
         CPI3D_frame lp = rp.scene->FindFrame("ldir", ENUMF_LIGHT);
         if(lp){
            S_vectorw ldir = lp->GetWorldDir();
            S_vectorw lpos = lp->GetWorldPos();
            if(object_space){
               (S_vector&)ldir %= GetInvMatrix1();
               (S_vector&)lpos *= GetInvMatrix1();
            }
            //ldir.x = 1.0f;
            ldir.w = 0.0f;
            lpos.w = 0.0f;
            drv->SetVSConstant(94, &lpos);
            drv->SetVSConstant(95, &ldir);
         }
      }
#endif
      if((vis_flags&VISF_USE_ENVMAP) && stage<drv->NumSimultaneousTextures()){
         CPI3D_texture_base tp_env = mat->GetTexture1(MTI_ENVIRONMENT);
         bool use_cube_map = (tp_env && tp_env->GetTextureType()==TEXTURE_CUBIC);
         dword env_code = VSF_GENERATE_ENVUV_WS - (int)object_space;
         if(use_cube_map)
            env_code += (VSF_GENERATE_ENVUV_CUBE_OS - VSF_GENERATE_ENVUV_OS);

                                 //prepare embm's uvs
         if(stage<drv->NumSimultaneousTextures()-1){
            if(vis_flags&VISF_USE_EMBMMAP){
               se_in.AddFragment(VSF_PICK_UV_0);
               se_in.AddFragment(VSF_MULTIPLY_UV_BY_Z);
               se_in.AddFragment((E_VS_FRAGMENT)(VSF_STORE_UV_0 + stage));
               ++stage;
            }else
            if(vis_flags&VISF_USE_BUMPMAP){
               se_in.AddFragment(VSF_PICK_UV_0);
               se_in.AddFragment((E_VS_FRAGMENT)(VSF_STORE_UV_0 + stage));
               ++stage;
               se_in.AddFragment(VSF_BUMP_OS);
               CPI3D_frame lp = rp.scene->FindFrame("ldir", ENUMF_LIGHT);
               if(lp){
                  S_vectorw ldir = lp->GetWorldDir();
                  S_vectorw lpos = lp->GetWorldPos();
                  if(object_space){
                     (S_vector&)ldir %= GetInvMatrix1();
                     (S_vector&)lpos *= GetInvMatrix1();
                  }
                  //ldir.x = 1.0f;
                  ldir.w = 0.0f;
                  lpos.w = 0.0f;
                  drv->SetVSConstant(94, &lpos);
                  drv->SetVSConstant(95, &ldir);
               }
            }
         }
                              //add envmap computation
         se_in.AddFragment((E_VS_FRAGMENT)env_code);
         se_in.AddFragment((E_VS_FRAGMENT)((!use_cube_map ? VSF_STORE_UV_0 : VSF_STORE_UVW_0) + stage));
         ++stage;
         do_env_mapping = true;
      }

      if((do_env_mapping || do_detailmap || do_bumpmap) && !(flags&(VSPREP_NO_COPY_UV|VSPREP_COPY_UV)))
         se_in.CopyUV();
   }

#ifndef GL
   if(drv->GetFlags2()&DRVF2_TEXCLIP_ON){
      se_in.AddFragment(object_space ? VSF_TEXKILL_PROJECT_OS : VSF_TEXKILL_PROJECT_WS);
      se_in.AddFragment((E_VS_FRAGMENT)(VSF_STORE_UVW_0+stage));
   }
#endif
#ifdef GL
   C_this_gl_program *glp = (C_this_gl_program*)(C_gl_program*)drv->gl_shader_programs[drv->GL_PROGRAM_VISUAL];
   if(!glp){
      glp = new C_this_gl_program(*drv);
      drv->gl_shader_programs[drv->GL_PROGRAM_VISUAL] = glp;
      glp->Release();
      C_str vs = LoadFile("bin\\gl_visual.vs.c");
      C_str ps = LoadFile("bin\\gl_visual.ps.c");
      glp->Build(vs, ps, "u_mat_view_proj\0u_sampler\0a_pos\0a_normal\0a_tex0\0u_lights\0u_num_lights\0", vs.Size(), ps.Size());
   }
   glp->Use();
#endif

                              //create/get shader and set to D3D
   I3D_driver::S_vs_shader_entry *se = drv->GetVSHandle(se_in);
   drv->SetVertexShader(se->vs);
   
                              //fill-in constants

                              //feed lighting constants
   if(save_light_params){
      assert(save_light_params->size());
      if(save_light_params->size() > 1)
         FillLightVSConstants(drv, *se, save_light_params->size()-1, save_light_params->begin());
      drv->SetVSConstant(VSC_AMBIENT, &save_light_params->back());
   }else
   if(num_light_params)
      FillLightVSConstants(drv, *se, num_light_params, (S_vectorw*)light_params);

#ifdef GL
   if(gl_save_light_params){
      glUniform4fv(glp->u_lights, gl_save_light_params->size(), &gl_save_light_params->begin()->x);
   }else
      glUniform4fv(glp->u_lights, gl_num_lights*2+1, &gl_light_params[0].x);
   glUniform1i(glp->u_num_lights, gl_num_lights);
#endif

   if(do_env_mapping){
                           //environment mapping (camera position relative to object)
      S_vectorw cam_pos = rp.scene->GetActiveCamera1()->GetMatrixDirect()(3);
      if(object_space)
         (S_vector&)cam_pos *= GetInvMatrix1();
      cam_pos.w = 0.0f;
      drv->SetVSConstant(se->vscs_cam_loc_pos, &cam_pos);
   }

                              //fill-in transformation matrix
   if(flags&VSPREP_FEED_MATRIX)
      rp.scene->SetRenderMatrix(*m_trans);
#ifndef GL
   if(!drv->IsDirectTransform() && (vis_flags&VISF_USE_DETMAP)){
                              //upload detailmap uv scale now
      CPI3D_mesh_base mb = GetMesh();
      if(mb){
         const I3D_face_group *fg = mb->GetFGroupVector().begin();
         S_vectorw uv_scale;
         (S_vector2&)uv_scale = fg->GetMaterial1()->GetDetailScale();
         uv_scale.z = 0;
         uv_scale.w = 0;
         drv->SetVSConstant(VSC_UV_SHIFT_SCALE, &uv_scale);
      }
   }
   if(drv->GetFlags2()&DRVF2_TEXCLIP_ON){
                              //feed in texkill plane transformed to local coordinates
      S_plane pl = drv->GetTexkillPlane();
      if(object_space){
         if(!alt_m_trans)
            pl = pl * GetInvMatrix1();
         else{
            pl = pl * ~(*m_trans);
         }
      }
      drv->SetVSConstant(VSC_TEXKILL, &pl);
   }
#endif
                              //enable clipping, if requested
   //drv->SetClipping(rp.flags&RP_FLAG_CLIP);
   return se;
}

//----------------------------
#ifndef GL
void I3D_visual::SetupSpecialMapping(CPI3D_material mat, const S_render_primitive *rp, dword num_txt_stages) const{

   dword stage = num_txt_stages;
   bool set_uv_scale = false;

   if((vis_flags&VISF_USE_DETMAP) && stage<drv->NumSimultaneousTextures()){
      CPI3D_texture_base tp_det = mat->GetTexture1(MTI_DETAIL);
      if(!tp_det){
         if(vis_flags&VISF_USE_ENVMAP){
            drv->SetupTextureStage(stage, D3DTOP_MODULATE);
            drv->SetTexture1(stage, NULL);
            ++stage;
         }
      }else{
         drv->SetupTextureStage(stage, D3DTOP_MODULATE2X);
         drv->SetTexture1(stage, tp_det);
         ++stage;
         set_uv_scale = true;
      }
   }

   if(vis_flags&VISF_USE_ENVMAP){
                           //setup env-map
      CPI3D_texture_base tp_env = mat->GetTexture1(MTI_ENVIRONMENT);
      if(tp_env){
         CPI3D_texture_base txt_embm = mat->GetTexture1(MTI_EMBM);
         if((vis_flags&VISF_USE_EMBMMAP) && txt_embm){
                                 //use primary UV coords for bump mapping
            if(!drv->IsDirectTransform())
               drv->SetTextureCoordIndex(stage, 0);

            if(!(txt_embm->GetTxtFlags()&TXTF_ALPHA)){
               drv->SetupTextureStage(stage, D3DTOP_BUMPENVMAP);
            }else{
               //drv->SetupTextureStage(stage, D3DTOP_BUMPENVMAPLUMINANCE);
               drv->SetupTextureStage(stage, D3DTOP_BUMPENVMAP);
            }
            drv->SetTexture1(stage, txt_embm);

            drv->SetEMBMScale(stage, mat->GetEMBMOpacity1());
            ++stage;
            set_uv_scale = true;
         }
         //if(!drv->IsDirectTransform()) drv->SetTextureCoordIndex(stage, num_txt_stages);
         drv->SetupTextureStage(stage, D3DTOP_MODULATE2X);
         drv->SetTexture1(stage, tp_env);
         ++stage;
      }
   }

   if(set_uv_scale){
      S_vectorw uv_scale;
      (S_vector2&)uv_scale = mat->GetDetailScale();
      uv_scale.z = mat->GetEMBMScale();
      uv_scale.w = 0.0f;
      drv->SetVSConstant(VSC_UV_SHIFT_SCALE, &uv_scale);
   }
   drv->DisableTextureStage(stage);
}
#endif
//----------------------------

void I3D_visual::SetupSpecialMappingPS(CPI3D_material mat, I3D_driver::S_ps_shader_entry_in &se_ps, dword num_txt_stages) const{

   dword stage = num_txt_stages;
   bool set_uv_scale = false;

   if((vis_flags&VISF_USE_DETMAP) && stage<drv->NumSimultaneousTextures()){
      CPI3D_texture_base tp_det = mat->GetTexture1(MTI_DETAIL);
      if(tp_det){
         se_ps.Tex(stage);
         se_ps.Mod2X(stage);
         drv->SetTexture1(stage, tp_det);
         set_uv_scale = true;
      }
      ++stage;
   }
#if defined _DEBUG && 0
   if((vis_flags&VISF_USE_BUMPMAP) && stage<drv->NumSimultaneousTextures()){
      CPI3D_texture_base tp_normal = mat->GetTexture1(MTI_NORMAL);
      if(tp_normal){
         se_ps.Tex(stage);
         se_ps.AddFragment(PSF_TEST);
         drv->SetTexture1(stage, tp_normal);
      }
      ++stage;
   }
#endif
   if((vis_flags&VISF_USE_ENVMAP) && stage<drv->NumSimultaneousTextures()){
      CPI3D_texture_base tp_env = mat->GetTexture1(MTI_ENVIRONMENT);
      if(tp_env){
                           //setup bump-map
         if(stage<drv->NumSimultaneousTextures()-1){
            if((vis_flags&VISF_USE_EMBMMAP)){
               CPI3D_texture_base tb_bump = mat->GetTexture1(MTI_EMBM);
               if(tb_bump){
                                       //use primary UV coords for bump mapping
                  se_ps.Tex(stage);
                  drv->SetTexture1(stage, tb_bump);
                  ++stage;
                  drv->SetEMBMScale(stage, mat->GetEMBMOpacity1());

                  if(!(tb_bump->GetTxtFlags()&TXTF_ALPHA)){
                     se_ps.TexBem(stage);
                  }else{
                     //se_ps.TexBeml(stage);
                     se_ps.TexBem(stage);
                  }
                  set_uv_scale = true;
               }else{
                  ++stage;
                  se_ps.Tex(stage);
               }
               se_ps.Mod2X(stage);
            }else
            if((vis_flags&VISF_USE_BUMPMAP)){
               CPI3D_texture_base tp_normal = mat->GetTexture1(MTI_NORMAL);
               if(tp_normal){
                  se_ps.Tex(stage);
                  drv->SetTexture1(stage, tp_normal);
                  se_ps.AddFragment(PSF_TEST);
                  ++stage;
                  se_ps.Tex(stage);
               }else{
                  ++stage;
                  se_ps.Tex(stage);
                  se_ps.Mod2X(stage);
               }
            }else{
               se_ps.Tex(stage);
               se_ps.Mod2X(stage);
            }
         }else{
            se_ps.Tex(stage);
            se_ps.Mod2X(stage);
         }
         drv->SetTexture1(stage, tp_env);
      }else{
         //drv->SetTexture1(stage, NULL);
         if((vis_flags&VISF_USE_EMBMMAP) && stage<=drv->NumSimultaneousTextures()-2){
            ++stage;
            //drv->SetTexture1(stage, NULL);
         }
      }
      ++stage;
   }/*else
   if(vis_flags&VISF_USE_EMBMMAP){
      CPI3D_texture_base tb_bump = mat->GetTexture1(MTI_EMBM);
      if(tb_bump){
                              //use primary UV coords for bump mapping
         se_ps.Tex(stage);
         drv->SetTexture1(stage, tb_bump);
         ++stage;
         se_ps.TexBem(stage);
         //set_uv_scale = true;
      }else{
         ++stage;
         se_ps.Tex(stage);
      }
   }*/
   if(set_uv_scale){
      S_vectorw uv_scale;
      (S_vector2&)uv_scale = mat->GetDetailScale();
      uv_scale.z = mat->GetEMBMScale();
      uv_scale.w = 0.0f;
      drv->SetVSConstant(VSC_UV_SHIFT_SCALE, &uv_scale);
   }
   drv->DisableTextures(stage);
#ifndef GL
   if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
      se_ps.TexKill(stage);
#endif
}

//----------------------------

void I3D_visual::AddMirrorData(S_preprocess_context &pc, CPI3D_material mat, int auto_lod){

   if(pc.mode != RV_MIRROR){
      assert(drv->CanRenderMirrors());
                              //save mirror data
      dword mid = mat->GetMirrorID1();
      t_mirror_data::iterator it = pc.mirror_data.find(mid);
      if(it==pc.mirror_data.end()){
         it = pc.mirror_data.insert(pair<int, t_mirror_visuals>(mid, t_mirror_visuals())).first;
         (*it).second.reserve(16);
      }
      t_mirror_visuals &mv = (*it).second;
      mv.push_back(new S_mirror_data(auto_lod, alpha, this, pc, I3DBLEND_ALPHABLEND));
      S_mirror_data *md = mv.back();
      md->rp.user1 = 0;
      //if(mat->IsDiffuseAlpha()) md->blend_mode |= I3DBLEND_VERTEXALPHA;
      md->Release();
   }
}

//----------------------------

void I3D_visual::AddPrimitives1(I3D_mesh_base *mb, S_preprocess_context &pc){

   int curr_auto_lod = -1;
   const C_auto_lod *auto_lods = mb->GetAutoLODs();
   {
      curr_auto_lod = drv->force_lod_index;
      if(curr_auto_lod==-1){
                              //compute automatic LOD based on distance
         curr_auto_lod = GetAutoLOD(pc, mb);
      }else{
         curr_auto_lod = Min(curr_auto_lod, (int)mb->NumAutoLODs()-1);
      }
   }
                           //get LOD's vertex count and face source
   const C_buffer<I3D_face_group> *fgrps;
   dword vertex_count;
   if(curr_auto_lod>=0){
      fgrps = &auto_lods[curr_auto_lod].fgroups;
      vertex_count = auto_lods[curr_auto_lod].vertex_count;
      pc.scene->render_stats.triangle += auto_lods[curr_auto_lod].GetIndexBuffer().NumFaces1();
   }else{
      fgrps = &mb->GetFGroupVector();
      vertex_count = mb->vertex_buffer.NumVertices();
      pc.scene->render_stats.triangle += mb->GetIndexBuffer().NumFaces1();
   }
   if(!vertex_count)
      return;
   pc.scene->render_stats.vert_trans += vertex_count;

                           //make sure destination VB is allocated
   if(NeedPrepareDestVb())
      PrepareDestVB(mb);

                           //add primitives to sort list
   {
      const I3D_face_group &fg = fgrps->front();
      CPI3D_material mat = fg.GetMaterial1();

      S_render_primitive &p = pc.prim_list.push_back(S_render_primitive(curr_auto_lod, alpha, this, pc));
   
      bool is_alpha = false;
      dword sort_value;
                           //determine blending mode
      p.blend_mode = I3DBLEND_OPAQUE;

      if(alpha!=255){
         p.blend_mode = mat->IsAddMode() ? I3DBLEND_ADD : I3DBLEND_ALPHABLEND;
         is_alpha = true;
         sort_value = PRIM_SORT_ALPHA_NOZWRITE;
         ++pc.alpha_nozwrite;
      }else
      if(!mat->IsTransl()){
         sort_value = PRIM_SORT_OPAQUE;
         ++pc.opaque;
      }else
      if(mat->IsCkeyAlpha1()){
         p.blend_mode = I3DBLEND_ALPHABLEND;
         sort_value = PRIM_SORT_CKEY_ALPHA;
         ++pc.ckey_alpha;
         is_alpha = true;
      }else
      if(mat->GetMirrorID1() != -1){
         if(drv->CanRenderMirrors()){
            pc.prim_list.pop_back();
            AddMirrorData(pc, mat, curr_auto_lod);
            //continue;
            return;
         }else{
            //p.blend_mode = I3DBLEND_OPAQUE;
            sort_value = PRIM_SORT_OPAQUE;
            ++pc.opaque;
         }
      }else{
         p.blend_mode = mat->IsAddMode() ? I3DBLEND_ADD : I3DBLEND_ALPHABLEND;
         is_alpha = true;
         sort_value = PRIM_SORT_ALPHA_NOZWRITE;
         ++pc.alpha_nozwrite;

      }
      //p.user = ii;

      //if(mp->IsDiffuseAlpha()) p.blend_mode |= I3DBLEND_VERTEXALPHA;

      if(!is_alpha){
                              //sort by material
         sort_value |= (mat->GetSortID()&PRIM_SORT_MAT_MASK)<<PRIM_SORT_MAT_SHIFT;
      }else{
                              //sort by distance
         const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
         const S_matrix &m = pc.scene->GetActiveCamera1()->GetViewMatrix1();
         float depth =
            bsphere.pos[0] * m(0, 2) +
            bsphere.pos[1] * m(1, 2) +
            bsphere.pos[2] * m(2, 2) +
                             m(3, 2);
         sort_value |= ((FloatToInt(depth * 100.0f))&PRIM_SORT_DIST_MASK) << PRIM_SORT_DIST_SHIFT;
      }
      p.sort_value = sort_value;
   }
}

//----------------------------
#ifndef GL
void I3D_visual::DrawPrimitiveVisual(I3D_mesh_base *mb, const S_preprocess_context &pc, const S_render_primitive &rp){

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   //const I3D_face_group *fg;
   const I3D_face_group *fgrps;
   dword vertex_count;
   dword base_index;
   dword num_fg;
#ifdef USE_STRIPS
   const S_fgroup_strip_info *strp_info;
#endif

   if(rp.curr_auto_lod>=0){
      const C_auto_lod &al = mb->GetAutoLODs()[rp.curr_auto_lod];
      //fg = &al.fgroups[rp.user];
      fgrps = al.fgroups.begin();
      num_fg = al.fgroups.size();
      vertex_count = al.vertex_count;
      base_index = al.GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(al.GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = al.GetStripInfo();
#endif
   }else
   {
                              //direct mesh
      //fg = &mb->GetFGroupVector()[rp.user];
      fgrps = mb->GetFGroupVector().begin();
      num_fg = mb->GetFGroupVector().size();
      vertex_count = mb->vertex_buffer.NumVertices();
      base_index = mb->GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(mb->GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = mb->GetStripInfo();
#endif
   }

   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
   drv->SetFVF(vertex_buffer.GetFVFlags());

   if(pc.mode!=RV_SHADOW_CASTER)
      drv->SetupBlend(rp.blend_mode);

   for(dword fgi=num_fg; fgi--; ){
      const I3D_face_group *fg = &fgrps[fgi];
      CPI3D_material mat = fg->GetMaterial1();
#ifdef USE_STRIPS
      const S_fgroup_strip_info &si = strp_info[fgi];
#endif

      if(pc.mode!=RV_SHADOW_CASTER){
         drv->SetRenderMat(mat, 0, (float)rp.alpha * R_255);
         //drv->SetupBlend(rp.blend_mode);
         drv->SetupTextureStage(0, drv->vertex_light_blend_op);
         SetupSpecialMapping(mat, &rp, 1);

         drv->EnableAnisotropy(0, true);
      }else{
         if(mat->IsTextureAlpha() || mat->IsCkeyAlpha1()){
            CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
            drv->SetTexture1(0, tb);
            if(tb){
               dword ar = (tb->GetTxtFlags()&TXTF_MIPMAP) ? 0x60 : 0xe0;
               ar = ar * rp.alpha / 255;
               drv->SetAlphaRef(ar);
            }
         }else{
            drv->SetTexture1(0, NULL);
         }
         drv->EnableNoCull(mat->Is2Sided1());
         if(rp.alpha!=0xff){
            dword save_tf = drv->last_texture_factor;
            dword fc = ((save_tf&0xff) * rp.alpha) / 0xff;
            drv->SetTextureFactor(0xff000000 | (fc<<16) | (fc<<8) | fc);
#ifdef USE_STRIPS
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
               base_index*3 + si.base_index, si.num_indicies-2);
#else
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0,
               vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
#endif
            CHECK_D3D_RESULT("DrawIP", hr);

            drv->SetTextureFactor(save_tf);
            continue;
         }
      }

                              //debug mode - without textures
      if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES)){
                                 //use factor (50% gray) instead of texture
         //drv->SetTexture1(0, NULL);  //nvidia drivers bugs if texture is set to NULL
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
         drv->SetTextureFactor(0xff808080);

#ifdef USE_STRIPS
         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
            base_index*3 + si.base_index, si.num_indicies-2);
#else
         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
            (base_index + fg->base_index) * 3, fg->num_faces);
#endif
         CHECK_D3D_RESULT("DrawIP", hr);
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
         return;
      }
#ifdef USE_STRIPS
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         base_index * 3 + si.base_index, si.num_indicies-2);
#else
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
#endif
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}
#endif
//----------------------------

void I3D_visual::DrawPrimitiveVisualPS(I3D_mesh_base *mb, const S_preprocess_context &pc, const S_render_primitive &rp){

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   const I3D_face_group *fgrps;
   dword vertex_count;
   dword base_index;
   dword num_fg;
#ifdef USE_STRIPS
   const S_fgroup_strip_info *strp_info;
#endif
   if(rp.curr_auto_lod < 0){
                              //direct mesh
      fgrps = mb->GetFGroupVector().begin();
      num_fg = mb->GetFGroupVector().size();
      vertex_count = mb->vertex_buffer.NumVertices();
      base_index = mb->GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(mb->GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = mb->GetStripInfo();
#endif
#ifdef GL
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mb->GetIndexBuffer().ibo);
#endif
   }else{
      const C_auto_lod &al = mb->GetAutoLODs()[rp.curr_auto_lod];
      fgrps = al.fgroups.begin();
      num_fg = al.fgroups.size();
      vertex_count = al.vertex_count;
      base_index = al.GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(al.GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = al.GetStripInfo();
#endif
#ifdef GL
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, al.GetIndexBuffer().ibo);
#endif
   }

#ifdef GL
   C_this_gl_program *glp = (C_this_gl_program*)(C_gl_program*)drv->gl_shader_programs[drv->GL_PROGRAM_VISUAL];
   glUniform1i(glp->u_sampler, 0); CHECK_GL_RESULT("glUniform1i");
   drv->SetStreamSource(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.GetSizeOfVertex());
   drv->SetVSDecl(mb->vertex_buffer.vs_decl);
   glBindBuffer(GL_ARRAY_BUFFER, mb->vertex_buffer.vbo);
   
   glUniformMatrix4fv(glp->u_mat_view_proj, 1, false, rp.scene->curr_render_matrix.m[0]);
   glEnableVertexAttribArray(glp->a_pos);  CHECK_GL_RESULT("glEnableVertexAttribArray");
   glEnableVertexAttribArray(glp->a_tex0);  CHECK_GL_RESULT("glEnableVertexAttribArray");
   glVertexAttribPointer(glp->a_pos, 3, GL_FLOAT, false, mb->vertex_buffer.GetSizeOfVertex(), 0); CHECK_GL_RESULT("glVertexAttribPointer");
   glVertexAttribPointer(glp->a_tex0, 2, GL_FLOAT, false, mb->vertex_buffer.GetSizeOfVertex(), (void*)GetVertexUvOffset(mb->vertex_buffer.GetFVFlags())); CHECK_GL_RESULT("glVertexAttribPointer");
   if(glp->a_normal!=-1){
      glEnableVertexAttribArray(glp->a_normal);  CHECK_GL_RESULT("glEnableVertexAttribArray");
      glVertexAttribPointer(glp->a_normal, 3, GL_FLOAT, false, mb->vertex_buffer.GetSizeOfVertex(), (void*)GetVertexNormalOffset(mb->vertex_buffer.GetFVFlags())); CHECK_GL_RESULT("glVertexAttribPointer");
   }
#else
   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
   drv->SetVSDecl(vertex_buffer.vs_decl);
#endif
#ifndef GL
   if(pc.mode!=RV_SHADOW_CASTER)
#endif
      drv->SetupBlend(rp.blend_mode);

   bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);

   for(dword fgi=num_fg; fgi--; ){
      const I3D_face_group *fg = &fgrps[fgi];
      const I3D_material *mat = fg->GetMaterial1();
#ifdef USE_STRIPS
      const S_fgroup_strip_info &si = strp_info[fgi];
#endif

      I3D_driver::S_ps_shader_entry_in se_ps;

#ifndef GL
      if(pc.mode==RV_SHADOW_CASTER){
         if(mat->IsTextureAlpha() || mat->IsCkeyAlpha1()){
            se_ps.Tex(0);
            se_ps.AddFragment(PSF_SHADOW_CAST);
            CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
            drv->SetTexture1(0, tb);
            if(tb){
               dword ar = (tb->GetTxtFlags()&TXTF_MIPMAP) ? 0x60 : 0xe0;
               ar = ar * rp.alpha / 255;
               drv->SetAlphaRef(ar);
            }
            drv->DisableTextures(1);
         }else{
            se_ps.AddFragment(PSF_COLOR_COPY);
            drv->DisableTextures(0);
         }
#ifndef GL
         if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
            se_ps.TexKill(1);
#endif
         drv->EnableNoCull(mat->Is2Sided1());
         if(rp.alpha!=0xff){
            drv->SetPixelShader(se_ps);
            float o = (pc.shadow_opacity * (float)rp.alpha) * R_255;
            const S_vectorw c(o, o, o, 1.0f);
            drv->SetPSConstant(PSC_COLOR, &c);

#ifdef USE_STRIPS
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
               base_index * 3 + si.base_index, si.num_indicies-2);
#else
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0,
               vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
#endif
            CHECK_D3D_RESULT("DrawIP", hr);

            const S_vectorw c1(pc.shadow_opacity, pc.shadow_opacity, pc.shadow_opacity, 1.0f);
            drv->SetPSConstant(PSC_COLOR, &c1);
            continue;
         }
      }else
#endif
      {
         if(mat->GetTexture1(MTI_DIFFUSE) && (drv->GetFlags2()&DRVF2_DRAWTEXTURES)){
            se_ps.Tex(0);
            drv->SetRenderMat(mat, 0, (float)rp.alpha * R_255);
            se_ps.AddFragment(has_diffuse ? PSF_MODX2_t0_v0 : PSF_COPY_BLACK_t0a);
            SetupSpecialMappingPS(mat, se_ps, 1);
            drv->EnableAnisotropy(0, true);
         }else{
            se_ps.AddFragment(has_diffuse ? PSF_v0_MUL2X : PSF_COPY_BLACK);
            SetupSpecialMappingPS(mat, se_ps, 1);
#ifdef GL
            glBindTexture(GL_TEXTURE_2D, 0);
#endif
         }
      }
      drv->SetPixelShader(se_ps);
#ifdef USE_STRIPS
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         base_index * 3 + si.base_index, si.num_indicies-2);
#else
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
#ifndef GL
         vertex_buffer.D3D_vertex_buffer_index,
#else
         mb->vertex_buffer.D3D_vertex_buffer_index,
#endif
         0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
#endif
      CHECK_D3D_RESULT("DrawIP", hr);
#ifdef GL
      glDrawElements(GL_TRIANGLES, fg->num_faces*3, GL_UNSIGNED_SHORT, (void*)(fg->base_index*6)); CHECK_GL_RESULT("glDrawElements");
#endif
   }
}

//----------------------------

void I3D_visual::RenderSolidMesh(PI3D_scene scene, bool debug, const I3D_cylinder *volume_cylinder) const{

                              //re-render all our primitives,
   CPI3D_mesh_base mb = GetMesh();
   if(!mb)
      return;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   {
      I3D_driver::S_vs_shader_entry_in se;
      se.AddFragment(VSF_TEXT_PROJECT);
      if(debug)
         se.AddFragment(VSF_SIMPLE_DIR_LIGHT);
#ifndef GL
      if(drv->GetFlags2()&DRVF2_TEXCLIP_ON){
         se.AddFragment(VSF_TEXKILL_PROJECT_OS);
         se.AddFragment((E_VS_FRAGMENT)(VSF_STORE_UVW_0+2));
      }
#endif
      S_render_primitive rp;
      rp.scene = scene;
      rp.sector = NULL;
      dword flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV | VSPREP_NO_DETAIL_MAPS;
      //if(!debug){
         for(PI3D_frame f=GetParent1(); f && f->GetType1()!=FRAME_SECTOR; f = f->GetParent1());
         if(f)
            rp.sector = I3DCAST_SECTOR(f);
      //}
      PrepareVertexShader(NULL, 1, se, rp, RV_NORMAL, flags);
#ifndef GL
      if(drv->GetFlags2()&DRVF2_TEXCLIP_ON){
                              //feed in texkill plane transformed to local coordinates
         S_plane pl = drv->GetTexkillPlane() * GetInvMatrix1();
         drv->SetVSConstant(VSC_TEXKILL, &pl);
      }
#endif
   }

   //drv->SetClipping(true);

   const I3D_face_group *fg;
   int curr_auto_lod = last_auto_lod;
   if(curr_auto_lod>=0){
      fg = &mb->GetAutoLODs()[curr_auto_lod].fgroups[0];
   }else
      fg = &mb->GetFGroupVector()[0];

   IDirect3DIndexBuffer9 *ib;
   dword vertex_count;
   dword ib_base;
   dword face_count;
   //const I3D_face_group *fgrps;
   dword num_fg;
#ifdef USE_STRIPS
   const S_fgroup_strip_info *strp_info;
#endif

   if(curr_auto_lod>=0){
                           //automatic LOD
      const C_auto_lod &al = mb->GetAutoLODs()[curr_auto_lod];
      //fgrps = al.fgroups.begin();
      num_fg = al.fgroups.size();
      ib = al.GetIndexBuffer().GetD3DIndexBuffer();
      vertex_count = al.vertex_count;
      ib_base = (al.GetIndexBuffer().D3D_index_buffer_index + fg->base_index);
      face_count = fg->num_faces;
#ifdef USE_STRIPS
      strp_info = al.GetStripInfo();
#endif
   }else
   {
                           //direct mesh
      //fgrps = mb->GetFGroupVector().begin();
      num_fg = mb->GetFGroupVector().size();
      ib = mb->GetIndexBuffer().GetD3DIndexBuffer();
      vertex_count = mb->vertex_buffer.NumVertices();
      ib_base = mb->GetIndexBuffer().D3D_index_buffer_index;
      face_count = mb->GetIndexBuffer().NumFaces1();
#ifdef USE_STRIPS
      strp_info = mb->GetStripInfo();
#endif
   }

   drv->EnableNoCull(fg->mat->Is2Sided1());

   if(!volume_cylinder || mb->HasStrips()){
      drv->SetIndices(ib);

      dword base_index;
#ifndef GL
      if(!drv->IsDirectTransform()){
         drv->SetStreamSource(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.GetSizeOfVertex());
         base_index = mb->vertex_buffer.D3D_vertex_buffer_index;
      }else
#endif
      {
#ifndef GL
         drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
         base_index = vertex_buffer.D3D_vertex_buffer_index;
#else
         drv->SetStreamSource(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.GetSizeOfVertex());
         base_index = mb->vertex_buffer.D3D_vertex_buffer_index;
#endif
      }
#ifndef GL
      drv->SetVSDecl(const_cast<IDirect3DVertexDeclaration9*>((const IDirect3DVertexDeclaration9*)vertex_buffer.vs_decl));
#else
      drv->SetVSDecl(const_cast<IDirect3DVertexDeclaration9*>((const IDirect3DVertexDeclaration9*)mb->vertex_buffer.vs_decl));
#endif

#ifdef USE_STRIPS
      if(mb->HasStrips()){
                              //must render each group separately as strip
         for(dword i=num_fg; i--; ){
            const S_fgroup_strip_info &si = strp_info[i];

            d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, base_index, 0,
               vertex_count, ib_base*3 + si.base_index, si.num_indicies-2);
         }
         return;
      }
#endif
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, base_index, 0, vertex_count, ib_base*3, face_count);
      CHECK_D3D_RESULT("DrawIP", hr);
      return;
   }
   IDirect3DVertexBuffer9 *vb_src = mb->vertex_buffer.GetD3DVertexBuffer();
   drv->SetStreamSource(vb_src, mb->vertex_buffer.GetSizeOfVertex());
   drv->SetFVF(mb->vertex_buffer.GetFVFlags());

   if(face_count >= (IB_TEMP_SIZE/sizeof(I3D_triface)))
      return;

#ifdef USE_STRIPS
   if(mb->HasStrips()){
                              //todo: support for stripped meshes
      assert(0);
      return;
   }
#endif
                              //render only faces inside of volume cylinder

                              //transform cylinder to object's local coords
   const S_matrix &m_inv = GetInvMatrix();
   I3D_cylinder vc_loc;
   vc_loc.pos = volume_cylinder->pos * m_inv;
   vc_loc.dir = volume_cylinder->dir % m_inv;
   float inv_scale = m_inv(0).Magnitude();
   vc_loc.radius = volume_cylinder->radius * inv_scale;

   int num_hit_faces = 0;

   I3D_triface *src, *dst;
   dword vsize = mb->vertex_buffer.GetSizeOfVertex();
   S_vector *verts;

                              //lock source and dest faces and vertices
   hr = ib->Lock(ib_base*sizeof(I3D_triface), face_count*sizeof(I3D_triface), (void**)&src, D3DLOCK_READONLY);
   CHECK_D3D_RESULT("Lock", hr);
#if 0
                              //allow appending to the buffer
   if(face_count+drv->ib_shadow_base_index <= (IB_TEMP_SIZE/sizeof(I3D_triface))){
      hr = drv->ib_shadow->Lock(drv->ib_shadow_base_index*sizeof(I3D_triface), face_count*sizeof(I3D_triface),
         (byte**)&dst, D3DLOCK_NOOVERWRITE);
   }else
#endif
   {
      drv->ib_temp_base_index = 0;
      hr = drv->ib_temp->Lock(0, face_count*sizeof(I3D_triface), (void**)&dst, D3DLOCK_DISCARD);
   }
   CHECK_D3D_RESULT("Lock", hr);
   hr = vb_src->Lock(mb->vertex_buffer.D3D_vertex_buffer_index*vsize, vertex_count*vsize, (void**)&verts, D3DLOCK_READONLY);
   CHECK_D3D_RESULT("Lock", hr);

#if 1
   const S_vector vc0 = vc_loc.pos;
   const S_vector vc1 = vc_loc.pos + vc_loc.dir;

   for(int i=face_count; i--; ){
      const I3D_triface &fc = src[i];
      const S_vector *v[3];
      v[0] = (S_vector*)(((byte*)verts)+fc[0]*vsize);
      v[1] = (S_vector*)(((byte*)verts)+fc[1]*vsize);
      v[2] = (S_vector*)(((byte*)verts)+fc[2]*vsize);

      S_plane pl;
      pl.normal.GetNormal(*v[0], *v[1], *v[2]);
                              //skip culled faces
      float dir_dot_norm = pl.normal.Dot(vc_loc.dir);
      if(CHECK_ZERO_GREATER(dir_dot_norm))
         continue;

      pl.d = -pl.normal.Dot(*v[0]);
      float dist_vc0 = vc0.Dot(pl.normal) + pl.d;
      float dist_vc1 = vc1.Dot(pl.normal) + pl.d;
                              //if both ends out in the same hemi-space, skip face
      if(CHECK_SAME_SIGNS(dist_vc0, dist_vc1)){
         float len = I3DSqrt(pl.normal.Square());
         float r = vc_loc.radius * len;
         if(dist_vc0>=r && dist_vc1>=r)
            continue;
         if(dist_vc0<=-r && dist_vc1<=-r)
            continue;
      }
                              //check all 3 sides of triangle
      word vi0 = 0;
      for(dword j=3; j--; ){
         word vi1 = vi0;
         vi0 = word(j);
                              //get edge's normal
         S_vector e_dir = (*v[vi1]) - (*v[vi0]);
         S_plane pl_e;
         pl_e.normal.x = e_dir.y * pl.normal.z - e_dir.z * pl.normal.y;
         pl_e.normal.y = e_dir.z * pl.normal.x - e_dir.x * pl.normal.z;
         pl_e.normal.z = e_dir.x * pl.normal.y - e_dir.y * pl.normal.x;

         pl_e.d = -v[vi0]->Dot(pl_e.normal);
         dist_vc0 = vc0.Dot(pl_e.normal) + pl_e.d;
         dist_vc1 = vc1.Dot(pl_e.normal) + pl_e.d;
                              //if both ends out in the same hemi-space, skip face
         if(CHECK_SAME_SIGNS(dist_vc0, dist_vc1)){
            float len = I3DSqrt(pl_e.normal.Square());
            float r = vc_loc.radius * len;
            if(dist_vc0>=r && dist_vc1>=r)
               goto skip_face;
         }
      }
                           //add face for rendering
      dst[num_hit_faces++] = fc;
   skip_face:
      {}
   }

//----------------------------
#else
   float vc_loc_radius_2 = vc_loc.radius * vc_loc.radius;
   float vc_loc_size_2 = vc_loc.dir.Magnitude();
   vc_loc_size_2 *= vc_loc_size_2;

   for(int i=face_count; i--; ){
      const I3D_triface &sf = src[i];
      bool is_hit = true;

      int num_hit_edges = 0;
      dword hit_mask = 0;
      const S_vector *tri_verts[3];
      word vi0 = sf[0];
      for(int j=3; j--; ){
         word vi1 = vi0;
         vi0 = sf[j];
         const S_vector &v0 = *(S_vector*)(((byte*)verts)+vi0*vsize);
         tri_verts[j] = &v0;
                           //check if this edge collides
         bool is_col = true;
                           //determine collision now
         const S_vector &v1 = *(S_vector*)(((byte*)verts)+vi1*vsize);
         float u1, u2;
         S_vector edge_dir = v1-v0;
         if(LineIntersection(v0, edge_dir, vc_loc.pos, vc_loc.dir, u1, u2)){
            is_col = (u2>=0.0f && u2<=1.0f);
            if(is_col){
                        //check distance
               S_vector p0 = v0 + edge_dir * u1;
               S_vector p1 = vc_loc.pos + vc_loc.dir * u2;
               float dist2 = (p0-p1).Square();
               is_col = (dist2 < vc_loc_radius_2);
            }
         }
         if(is_col){
            ++num_hit_edges;
            hit_mask |= (1<<j);
         }
      }
      switch(num_hit_edges){
      case 0:
         {
                           //not hit by edge, still may be hit
            S_plane pl;
            pl.normal.GetNormal(*tri_verts[0], *tri_verts[1], *tri_verts[2]);
                           //check if face is ok
            float d = pl.normal.Dot(vc_loc.dir);
            if(CHECK_ZERO_GREATER(d)) continue;
            //pl.normal.Normalize();
            pl.d = -pl.normal.Dot(*tri_verts[0]);
            S_vector ip;
            if(pl.Intersection(vc_loc.pos, vc_loc.dir, ip)){
               is_hit = IsPointInTriangle(ip, *tri_verts[0], *tri_verts[1], *tri_verts[2], pl.normal);
               if(is_hit){
                  float dist_2 = (ip-vc_loc.pos).Square();
                  is_hit = (dist_2 < vc_loc_size_2);
               }
            }
         }
         break;
      case 1:
         {
                           //hit by one edge, check if really hit
            S_plane pl;
            pl.normal.GetNormal(*tri_verts[0], *tri_verts[1], *tri_verts[2]);
            float d = pl.normal.Dot(vc_loc.dir);
            if(CHECK_ZERO_GREATER(d)) continue;
            pl.d = -pl.normal.Dot(*tri_verts[0]);
            S_vector ip;
            if(pl.Intersection(vc_loc.pos, vc_loc.dir, ip)){
               S_vector n;
               float d;
               const S_vector &face_normal = pl.normal;
               if(!(hit_mask&1)){
                  S_vector v((*tri_verts[0]) - (*tri_verts[1]));
                  n.x = face_normal.y * v.z - face_normal.z * v.y;
                  n.y = face_normal.z * v.x - face_normal.x * v.z;
                  n.z = face_normal.x * v.y - face_normal.y * v.x;
                  d = (*tri_verts[0]).Dot(n) - ip.Dot(n);
                  if(CHECK_ZERO_LESS(d))
                     is_hit = false;
               }
               if(is_hit && !(hit_mask&2)){
                  S_vector v((*tri_verts[1]) - (*tri_verts[2]));
                  n.x = face_normal.y * v.z - face_normal.z * v.y;
                  n.y = face_normal.z * v.x - face_normal.x * v.z;
                  n.z = face_normal.x * v.y - face_normal.y * v.x;
                  d = (*tri_verts[1]).Dot(n) - ip.Dot(n);
                  if(CHECK_ZERO_LESS(d))
                     is_hit = false;
               }
               if(is_hit && !(hit_mask&4)){
                  S_vector v((*tri_verts[2]) - (*tri_verts[0]));
                  n.x = face_normal.y * v.z - face_normal.z * v.y;
                  n.y = face_normal.z * v.x - face_normal.x * v.z;
                  n.z = face_normal.x * v.y - face_normal.y * v.x;
                  d = (*tri_verts[2]).Dot(n) - ip.Dot(n);
                  if(CHECK_ZERO_LESS(d))
                     is_hit = false;
               }
            }
         }
         break;
      }
                           //add face if hit
      if(is_hit)
         dst[num_hit_faces++] = sf;
   }
#endif

   hr = drv->ib_temp->Unlock();
   CHECK_D3D_RESULT("Unlock", hr);
   hr = ib->Unlock();
   CHECK_D3D_RESULT("Unlock", hr);
   hr = vb_src->Unlock();
   CHECK_D3D_RESULT("Unlock", hr);

   if(num_hit_faces){
      drv->SetIndices(drv->ib_temp);

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mb->vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         drv->ib_temp_base_index*3, num_hit_faces);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
   drv->ib_temp_base_index += num_hit_faces;
}

//----------------------------

void I3D_visual::GetChecksum(float &matrix_sum, float &vertc_sum, dword &num_v) const{

   I3D_frame::GetChecksum(matrix_sum, vertc_sum, num_v);
                              //mesh verticies (if any)
   PI3D_mesh_base mb = const_cast<PI3D_mesh_base>(GetMesh());
   if(!mb)
      return;
#if 0
   /*num_v = mb->NumVertices1();
                              //make checksum on all mesh vertices

   const S_vector *pick_verts = (const S_vector*)mb->vertex_buffer.Lock(D3DLOCK_READONLY);
   dword vstride = mb->vertex_buffer.GetSizeOfVertex();

   for(dword i = num_v; i--; ){
      const S_vector &vx = *(const S_vector*)pick_verts;
      for(int ii = 3; ii--; )
         vertc_sum += vx[ii];
      pick_verts = (const S_vector *) (((byte*)pick_verts) + vstride);
   }
   mb->vertex_buffer.Unlock();*/
#else

   dword num_faces = mb->NumFaces1();
   //PI3D_triface faces = mb->index_buffer.Lock(D3DLOCK_READONLY);
   C_buffer<I3D_triface> face_buf(num_faces);
   PI3D_triface faces = face_buf.begin();
   mb->GetFaces(faces);

   const S_vector *pick_verts = (const S_vector*)mb->vertex_buffer.Lock(D3DLOCK_READONLY);
   dword vstride = mb->vertex_buffer.GetSizeOfVertex();

                              //check all faces
   num_v = num_faces*3; //3 vertices for each face
   for(dword ii = 0; ii < num_faces; ii++, ++faces){
      const I3D_triface &fc = *faces;
      for(dword jj = 3; jj--; ){
         const S_vector &vx = *(const S_vector*)(((byte*)pick_verts) + fc[jj] * vstride);
         for(dword kk = 3; kk--; )
            vertc_sum += vx[kk];
      }
   }
   mb->vertex_buffer.Unlock();
   //mb->index_buffer.Unlock();
#endif
}


//----------------------------
//----------------------------

