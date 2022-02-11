*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   Content: Discharge frame.
   Author: Michal Bacik
--------------------------------------------------------*/

#include "all.h"
#include "object.h"
#include "camera.h"
#include "mesh.h"
#include "procedural.h"


#define MAX_TICK_COUNT 500

//----------------------------
//----------------------------

class I3D_discharge: public I3D_visual{

   struct S_vertex{
      float x;                   //X (may be -1 or 1)
      float step_index;          //index of step (*2)
      //S_vector2 uv;
      short u, v;
   };

   struct S_data{
      dword life_time_min, life_time_rnd;
      float length;           //length along Z axis
      float width;            //width in X axis
      dword num_rays;
      dword num_steps;
      float delta_pos_base, delta_pos_rnd;
      float delta_attenuate;       //maximal delta position attenuation at ends
      float delta_attenuate_ratio; //ratio in length, during which delta position is attenuation
      float tiling;           //v tiling along entire length
      float fade_on_off_ratio;//ratio in lifetime, for which opacity is fading
      float opacity;

      void SetupDefaults(){
         life_time_min = 1000;
         life_time_rnd = 0;
         length = 1.0f;
         width = .03f;
         num_rays = 1;
         num_steps = 8;
         delta_pos_base = .1f;
         delta_pos_rnd = 0.0f;
         delta_attenuate = 1.0f;
         delta_attenuate_ratio = .2f;
         tiling = 1.0f;
         fade_on_off_ratio = .25f;
         opacity = 1.0f;
      }
   } data;
#ifdef GL
   I3D_dest_vertex_buffer vertex_buffer;
#endif

   struct S_ray{
      float r_init_life_time; //reciprocal of initial life counter
      int life_count;
      struct S_ray_step{
         S_vector pos;
         float opacity;
         S_vector dir;
         float unused;
      };
      C_buffer<S_ray_step> steps;

      S_ray(): life_count(0){}
      void Init(dword num_steps){
         steps.assign(num_steps);
      }
   };
   C_buffer<S_ray> rays;

   C_smart_ptr<I3D_material> mat;
   C_smart_ptr<IDirect3DVertexBuffer9> vb;

//----------------------------

   void ResetBBox(){
      vis_flags &= ~VISF_BOUNDS_VALID;
      frm_flags &= ~(FRMFLAGS_BSPHERE_TRANS_VALID | FRMFLAGS_HR_BSPHERE_VALID | FRMFLAGS_HR_BOUND_VALID);
   }

//----------------------------

   void ResetVB(){
      vb = NULL;
   }

//----------------------------
// Get time delta since last render. The returned value is not grater than some optimal value,
// so that we don't waste time on updating unnecessary elements.
   int GetTickTime() const{
      int count = drv->GetRenderTime();
      int delta = count - last_render_time;
      return Max(0, Min(MAX_TICK_COUNT, delta));
   }

//----------------------------

   void InitRays(){

      rays.assign(data.num_rays);
      for(dword i=data.num_rays; i--; ){
         rays[i].Init(data.num_steps);
      }
      if(!vertex_buffer.vs_decl){
         C_vector<D3DVERTEXELEMENT9> els;
         //els.push_back(S_vertex_element(0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION));
         els.push_back(S_vertex_element(0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_POSITION));
         els.push_back(S_vertex_element(8, D3DDECLTYPE_SHORT2, D3DDECLUSAGE_TEXCOORD, 0));
         els.push_back(D3DVERTEXELEMENT9_END);
         vertex_buffer.vs_decl = drv->GetVSDeclaration(els);
      }
   }

//----------------------------

   void AnimateRays(){

      int time = GetTickTime();
      if(!time)
         return;
      for(dword i=data.num_rays; i--; ){
         S_ray &ray = rays[i];
         if((ray.life_count -= time) <= 0){
                              //re-init the ray
            ray.life_count = data.life_time_min;
            if(data.life_time_rnd)
               ray.life_count += S_int_random(data.life_time_rnd);
            ray.r_init_life_time = 1.0f;
            if(ray.life_count)
               ray.r_init_life_time = 1.0f / (float)ray.life_count;
            //ray.life_count = 0x7fffffff; //debug!
                              //init delta positions
            for(dword si=data.num_steps; si--; ){
               S_ray::S_ray_step &st = ray.steps[si];
               float y = (float)si / (float)(data.num_steps-1);
               float border_ratio = (y<.5f) ? y : 1.0f-y;
               st.pos.Zero();
               st.pos.z = y * data.length;
               S_vector delta(S_float_random(2)-1, S_float_random(2)-1, S_float_random(2)-1);
               delta.Normalize();
               float delta_scale = data.delta_pos_base + S_float_random(data.delta_pos_rnd);
               st.opacity = 1.0f;
                              //attenuate delta at borders
               if(border_ratio < data.delta_attenuate_ratio){
                  float k = border_ratio / data.delta_attenuate_ratio;
                  //drv->PRINT(k);
                  delta_scale = delta_scale*(1.0f - data.delta_attenuate) + delta_scale*k*data.delta_attenuate;
                  st.opacity = k;
               }
               //drv->PRINT(st.opacity);
               delta *= delta_scale;
               st.pos += delta;

               st.unused = 0.0f;
            }
                              //init directions
            for(si=1; si<data.num_steps-1; si++){
               ray.steps[si].dir = S_normal(ray.steps[si+1].pos - ray.steps[si-1].pos);
            }
            ray.steps[0].dir = S_normal(ray.steps[1].pos - ray.steps[0].pos);
            ray.steps[data.num_steps-1].dir = S_normal(ray.steps[data.num_steps-1].pos - ray.steps[data.num_steps-2].pos);
         }
      }
   }

//----------------------------
public:
   I3D_discharge(PI3D_driver d):
#ifdef GL
      vertex_buffer(d),
#endif
      I3D_visual(d)
   {
      visual_type = I3D_VISUAL_DISCHARGE;
      data.SetupDefaults();
   }

   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive &rp);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive &rp);
   virtual bool ComputeBounds(){
      bound.bound_local.bbox.min.Zero();
      bound.bound_local.bbox.min.x = -(data.delta_pos_base + data.delta_pos_rnd+data.width);
      bound.bound_local.bbox.min.y = bound.bound_local.bbox.min.x;
      bound.bound_local.bbox.min.z = bound.bound_local.bbox.min.x;

      bound.bound_local.bbox.max = -bound.bound_local.bbox.min;
      bound.bound_local.bbox.max.z += data.length;

      {
         S_vector bbox_diagonal = (bound.bound_local.bbox.max - bound.bound_local.bbox.min) * .5f;
         bound.bound_local.bsphere.pos = bound.bound_local.bbox.min + bbox_diagonal;
         bound.bound_local.bsphere.radius = bbox_diagonal.Magnitude();
      }

      vis_flags |= VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
      return true;
   }

//----------------------------
                              //create vertex buffer
   virtual void PrepareDestVB(I3D_mesh_base*, dword num_txt_stages = 1){

      HRESULT hr;
      dword d3d_usage = D3DUSAGE_WRITEONLY;
      D3DPOOL mem_pool = D3DPOOL_DEFAULT;
#ifndef GL
      if(!drv->IsHardware()){
         if(!drv->IsDirectTransform()){
            d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
            mem_pool = D3DPOOL_SYSTEMMEM;
         }
      }
#endif
      dword max_verts = data.num_steps * 2;
      {
         IDirect3DVertexBuffer9 *vb_tmp;
         hr = drv->GetDevice1()->CreateVertexBuffer(max_verts*sizeof(S_vertex), d3d_usage, 0, mem_pool, &vb_tmp, NULL);
         CHECK_D3D_RESULT("CreateVertexBuffer", hr);
         vb = vb_tmp;
         vb_tmp->Release();
      }
                              //fill-in vertices
      D3D_lock<byte> v_dst(vb, 0, 128, vertex_buffer.GetSizeOfVertex(), 0);
      v_dst.AssureLock();
      S_vertex *verts = (S_vertex*)(byte*)v_dst;

      for(dword si=0, vi=0; si<data.num_steps; si++){
         float y = (float)si / (float)(data.num_steps-1);
         {
            S_vertex &v = verts[vi++];
            v.x = -data.width;
            //v.uv.x = 0.0f;
            //v.uv.y = y * data.tiling;
            v.u = 0;
            v.v = (short)FloatToInt(y * data.tiling * 256.0f);
            v.step_index = (float)(si*2);
         }
         {
            S_vertex &v = verts[vi++];
            v.x = data.width;
            v.u = 256;
            v.v = (short)FloatToInt(y * data.tiling * 256.0f);
            //v.uv.x = 1.0f;
            //v.uv.y = y * data.tiling;
            v.step_index = (float)(si*2);
         }
      }
      assert(vi==max_verts);
      vis_flags |= VISF_DEST_PREPARED;
   }

//----------------------------

   I3DMETHOD(Duplicate)(CPI3D_frame frm){

      if(frm==this)
         return I3D_OK;
      if(frm->GetType1()!=FRAME_VISUAL)
         return I3D_frame::Duplicate(frm);
      CPI3D_visual vis = I3DCAST_CVISUAL(frm);
      if(vis->GetVisualType()==I3D_VISUAL_DISCHARGE){
         I3D_discharge *dis = (I3D_discharge*)vis;
         mat = dis->mat;
         data = dis->data;
         ResetBBox();
         ResetVB();
         rays.clear();
      }else
      if(vis->GetMesh()){
         mat = vis->GetMesh()->GetFGroups1()->mat;
      }else
      if(vis->GetMaterial()){
         mat = const_cast<PI3D_visual>(vis)->GetMaterial();
      }
      return I3D_visual::Duplicate(vis);
   }

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

//----------------------------

   I3DMETHOD(SetProperty)(dword index, dword value){

      switch(index){
      case 0: data.life_time_min = value; break;
      case 1: data.life_time_rnd = value; break;
      case 2: data.length = I3DIntAsFloat(value); break;
      case 3: data.width = I3DIntAsFloat(value); break;
      case 4: data.num_rays = value; rays.clear(); break;
      case 5: data.num_steps = value; break;
      //case 6: data.num_steps_rnd = value; break;
      case 7: data.delta_pos_base = I3DIntAsFloat(value); break;
      case 8: data.delta_pos_rnd = I3DIntAsFloat(value); break;
      case 9: data.delta_attenuate = I3DIntAsFloat(value); break;
      case 10: data.delta_attenuate_ratio = I3DIntAsFloat(value); break;
      case 11: data.tiling = I3DIntAsFloat(value); break;
      case 12: data.fade_on_off_ratio = Min(.5f, I3DIntAsFloat(value)); break;
      case 13: data.opacity = I3DIntAsFloat(value); break;
      default:
         return I3DERR_INVALIDPARAMS;
      }
      ResetVB();
      rays.clear();
      ResetBBox();
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(dword,GetProperty)(dword index) const{

      switch(index){
      case 0: return data.life_time_min;
      case 1: return data.life_time_rnd;
      case 2: return I3DFloatAsInt(data.length);
      case 3: return I3DFloatAsInt(data.width);
      case 4: return data.num_rays;
      case 5: return data.num_steps;
      //case 6: return data.num_steps_rnd;
      case 7: return I3DFloatAsInt(data.delta_pos_base);
      case 8: return I3DFloatAsInt(data.delta_pos_rnd);
      case 9: return I3DFloatAsInt(data.delta_attenuate);
      case 10: return I3DFloatAsInt(data.delta_attenuate_ratio);
      case 11: return I3DFloatAsInt(data.tiling);
      case 12: return I3DFloatAsInt(data.fade_on_off_ratio);
      case 13: return I3DFloatAsInt(data.opacity);
      }
      return 0;
   }

//----------------------------
   /*
   virtual bool AttachProcedural(I3D_procedural_base *pb){
      if(pb->GetID()!=PROCID_MORPH_3D)
         return false;
      proc = (I3D_procedural_morph*)pb;
      return true;
   }
   */

   I3DMETHOD_(I3D_RESULT,SetMaterial)(PI3D_material mat1){
      mat = mat1;
      return I3D_OK;
   }
   I3DMETHOD_(PI3D_material,GetMaterial()){ return mat; }
   I3DMETHOD_(CPI3D_material,GetMaterial()) const{ return mat; }
};

//----------------------------

void I3D_discharge::AddPrimitives(S_preprocess_context &pc){

#ifndef GL
   if(pc.mode==RV_SHADOW_CASTER) return;
   if(drv->GetFlags2()&DRVF2_TEXCLIP_ON) return;
#endif

   if(!mat)
      return;
   if(data.num_steps<2 || data.num_steps>25)
      return;
   if(!rays.size())
      InitRays();

   AnimateRays();

   pc.prim_list.push_back(S_render_primitive(0, alpha, this, pc));
   S_render_primitive &p = pc.prim_list.back();
   
   p.blend_mode = I3DBLEND_ADD;

   PI3D_camera cam = pc.scene->GetActiveCamera1();
                              
                              //compute sorting depth
   const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
   const S_vector &w_pos = bsphere.pos;
   const S_matrix &m_view = cam->GetViewMatrix1();
   float depth =
         w_pos[0] * m_view(0, 2) +
         w_pos[1] * m_view(1, 2) +
         w_pos[2] * m_view(2, 2) +
                    m_view(3, 2);

   dword sort_base = ((FloatToInt(depth * 100.0f))&PRIM_SORT_DIST_MASK) << PRIM_SORT_DIST_SHIFT;
   p.sort_value = PRIM_SORT_ALPHA_NOZWRITE | sort_base;
   ++pc.alpha_nozwrite;

                              //add rendering stats
   pc.scene->render_stats.triangle += (data.num_steps-1)*2 * data.num_rays;
   pc.scene->render_stats.vert_trans += data.num_steps*2 * data.num_rays;
}

//----------------------------
#ifndef GL
void I3D_discharge::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   HRESULT hr;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   PI3D_camera cam = pc.scene->GetActiveCamera1();

   I3D_driver::S_vs_shader_entry_in se;
   se.AddFragment(VSF_TRANSFORM_DISCHARGE);
   const I3D_driver::S_vs_shader_entry *se_out = PrepareVertexShader(NULL, 1, se, rp, pc.mode,
      VSPREP_WORLD_SPACE | VSPREP_NO_DETAIL_MAPS);

                              //store transposed projection matrix
   drv->SetVSConstant(VSC_MAT_TRANSFORM_0, &rp.scene->GetViewProjHomMatTransposed(), 4);
                              //store our matrix
   {
      S_matrix tm = matrix;
      tm.Transpose();
      drv->SetVSConstant(se_out->vscs_mat_frame, &tm, 3);
   }

   if(!vb)
      PrepareDestVB(NULL);

   S_vectorw cam_loc_pos = cam->GetMatrixDirect()(3) * GetInvMatrix();
   cam_loc_pos.w = 0.0f;
   drv->SetVSConstant(se_out->vscs_cam_loc_pos, &cam_loc_pos);

   drv->SetStreamSource(vb, sizeof(S_vertex));
   drv->SetVSDecl(vertex_buffer.vs_decl);

   drv->SetTexture1(0, mat->GetTexture1());
   drv->EnableNoCull(true);
   drv->DisableTextureStage(1);

   if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
      drv->SetTexture1(0, NULL);
   drv->SetupBlend(rp.blend_mode);
   drv->SetupTextureStage(0, D3DTOP_MODULATE);

   S_vectorw color = mat->GetDiffuse();
   float base_alpha = color.w * data.opacity;

                              //render all rays
   for(dword ri=data.num_rays; ri--; ){
      const S_ray &ray = rays[ri];
                              //setup ray's constants
      drv->SetVSConstant(VSC_MAT_BLEND_BASE+0, ray.steps.begin(), data.num_steps*2);

                              //setup color
      color.w = base_alpha;
      float k = (float)ray.life_count * ray.r_init_life_time;
      if(k < data.fade_on_off_ratio){
         color.w *= k/data.fade_on_off_ratio;
      }else
      if(k > (1.0f-data.fade_on_off_ratio)){
         color.w *= (1.0f-k)/(1.0f-data.fade_on_off_ratio);
      }
      
      drv->SetVSConstant(VSC_AMBIENT, &color);


      hr = d3d_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, (data.num_steps-1)*2);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}
#endif
//----------------------------

void I3D_discharge::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   HRESULT hr;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   PI3D_camera cam = pc.scene->GetActiveCamera1();

   I3D_driver::S_vs_shader_entry_in se;
   se.AddFragment(VSF_TRANSFORM_DISCHARGE);
   const I3D_driver::S_vs_shader_entry *se_out = PrepareVertexShader(NULL, 1, se, rp, pc.mode, VSPREP_WORLD_SPACE | VSPREP_NO_DETAIL_MAPS);

                              //store transposed projection matrix
   drv->SetVSConstant(VSC_MAT_TRANSFORM_0, &rp.scene->GetViewProjHomMatTransposed(), 4);
                              //store our matrix
   {
      S_matrix tm = matrix;
      tm.Transpose();
      drv->SetVSConstant(se_out->vscs_mat_frame, &tm, 3);
   }

   if(NeedPrepareDestVb())
      PrepareDestVB(NULL);

   S_vectorw cam_loc_pos = cam->GetMatrixDirect()(3) * GetInvMatrix();
   cam_loc_pos.w = 0.0f;
   drv->SetVSConstant(se_out->vscs_cam_loc_pos, &cam_loc_pos);

   drv->SetStreamSource(vb, sizeof(S_vertex));
   drv->SetVSDecl(vertex_buffer.vs_decl);

   drv->EnableNoCull(true);

   I3D_driver::S_ps_shader_entry_in se_ps;

   if(drv->GetFlags2()&DRVF2_DRAWTEXTURES){
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_MOD_t0_v0);
      drv->SetTexture1(0, mat->GetTexture1());
      drv->DisableTextures(1);
   }else{
      se_ps.AddFragment(PSF_v0_COPY);
      drv->DisableTextures(0);
   }
   //if(drv->GetFlags2()&DRVF2_TEXCLIP_ON) se_ps.TexKill(1);

   drv->SetPixelShader(se_ps);

   drv->SetupBlend(rp.blend_mode);

   S_vectorw color = mat->GetDiffuse();
   float base_alpha = color.w * data.opacity;

                              //render all rays
   for(dword ri=data.num_rays; ri--; ){
      const S_ray &ray = rays[ri];
                              //setup ray's constants
      drv->SetVSConstant(VSC_MAT_BLEND_BASE+0, ray.steps.begin(), data.num_steps*2);

                              //setup color
      color.w = base_alpha;
      float k = (float)ray.life_count * ray.r_init_life_time;
      if(k < data.fade_on_off_ratio){
         color.w *= k/data.fade_on_off_ratio;
      }else
      if(k > (1.0f-data.fade_on_off_ratio)){
         color.w *= (1.0f-k)/(1.0f-data.fade_on_off_ratio);
      }
      
      drv->SetVSConstant(VSC_AMBIENT, &color);

      hr = d3d_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, (data.num_steps-1)*2);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}

//----------------------------

extern const S_visual_property props_Discharge[] = {
   {I3DPROP_INT, "Life len", "Base time - length of element's life."},
   {I3DPROP_INT, "Life len thresh", "Random time - length of element's life."},
   {I3DPROP_FLOAT, "Length", "Length along Z axis."},
   {I3DPROP_FLOAT, "Width", "Width along X axis."},
   {I3DPROP_INT, "Num rays", "Number of rays."},
   {I3DPROP_INT, "Num steps", "Number of steps (base)."},
   {I3DPROP_NULL},//{I3DPROP_INT, "Num steps rnd", "Number of steps (random addiction)."},
   {I3DPROP_FLOAT, "Delta pos", "Delta position (base)."},
   {I3DPROP_FLOAT, "Delta pos rnd", "Delta position (random addiction)."},
   {I3DPROP_FLOAT, "Delta attn", "Max. attenuation of delta position at the borders."},
   {I3DPROP_FLOAT, "Delta attn ratio", "Ratio (in ray's length), when delta attenuation starts."},
   {I3DPROP_FLOAT, "Tiling", "Tiling of texture's V along the length of the ray. Tiling of U is automatical from 0 to 1 along the width of the ray."},
   {I3DPROP_FLOAT, "Fade out ratio", NULL},
   {I3DPROP_FLOAT, "Opacity", NULL},
   {I3DPROP_NULL}
};

I3D_visual *CreateDischarge(PI3D_driver drv){
   return new I3D_discharge(drv);
}

//----------------------------
//----------------------------


