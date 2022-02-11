/*--------------------------------------------------------
   Copyright (c) 2000 Lonely Cat Games.
   All rights reserved.
   Author: Michal Bacik
--------------------------------------------------------*/

#include "all.h"
#include "object.h"
#include "camera.h"
#include "mesh.h"

//----------------------------
//----------------------------

class I3D_object_camview: public I3D_object, public I3D_driver::C_reset_callback{
   C_render_target<1, I3D_texture_base> rt;
   C_smart_ptr<I3D_camera> cam;

   float camview_alpha;
   dword alpha_factor;
   C_str cam_name;
   dword txt_x, txt_y;

//----------------------------

   void SetTextureSize(dword x, dword y){

      if(txt_x!=x || txt_y!=y){
         txt_x = x;
         txt_y = y;
         rt.Close();
      }
   }

//----------------------------

   void SetCameraName(const char *name){

      cam_name = name;
      cam = NULL;
   }

//----------------------------

   virtual void ResetCallback(I3D_driver::E_RESET_MSG msg){

      switch(msg){
      case I3D_driver::RESET_RELEASE:
         rt.Close();
         break;
      }
   }

//----------------------------

   bool RenderIntoTexture(PI3D_scene scn, dword render_flags){

      HRESULT hr;
      if(!rt){
         drv->InitRenderTarget(rt,
            TEXTMAP_NOMIPMAP | //TEXTMAP_HINTDYNAMIC |
            TEXTMAP_RENDERTARGET | TEXTMAP_NO_SYSMEM_COPY, txt_x, txt_y);
         if(!rt){
            drv->DEBUG(C_fstr("CamView '%s': can't create render target", (const char*)name), 2);
         }
      }
      if(!cam){
         cam = I3DCAST_CAMERA(scn->FindFrame(cam_name, ENUMF_CAMERA));
         if(!cam){
            drv->DEBUG(C_fstr("CamView '%s': can't find camera '%s'", (const char*)name, (const char*)cam_name), 2);
         }
         if(!cam)
            return false;
      }

                        //render view to the texture
      if(rt){
         C_render_target<> save_rt = drv->GetCurrRenderTarget();
         drv->SetRenderTarget(rt);

         bool is_aspect;
         I3D_rectangle vp = scn->GetViewport1(&is_aspect);
         C_smart_ptr<I3D_camera> cam_save = scn->GetActiveCamera1();
         scn->SetActiveCamera(NULL);
         scn->SetViewport(I3D_rectangle(0, 0, rt->SizeX1(), rt->SizeY1()));
         scn->SetActiveCamera(cam);
         S_preprocess_context pc_rec(scn);
         scn->RenderView(render_flags, RV_CAM_TO_TEXTURE,
#ifndef GL
            NULL, 0,
#endif
            &pc_rec);

         IDirect3DDevice9 *pDev8 = drv->GetDevice1();
                           //restore back
         scn->SetViewport(vp, is_aspect);
         drv->UpdateScreenViewport(vp);
         scn->SetActiveCamera(cam_save);
         scn->SetMViewProj(cam_save);
         hr = pDev8->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&cam_save->GetProjectionMatrix1());
         CHECK_D3D_RESULT("SetTransform", hr);
         drv->SetRenderTarget(save_rt);
      }
      return true;
   }

//----------------------------

public:
   I3D_object_camview(PI3D_driver d):
      I3D_object(d),
      camview_alpha(.5f), alpha_factor(128),
      txt_x(256), txt_y(256)
   {
      visual_type = I3D_VISUAL_CAMVIEW;
      drv->RegResetCallback(this);
   }

   ~I3D_object_camview(){
      drv->UnregResetCallback(this);
   }


   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

//----------------------------

   I3DMETHOD(Duplicate)(CPI3D_frame frm){

      if(frm->GetType1()==FRAME_VISUAL && I3DCAST_CVISUAL(frm)->GetVisualType1()==GetVisualType()){
         I3D_object_camview *cv = (I3D_object_camview*)frm;
         camview_alpha = cv->camview_alpha;
         alpha_factor = cv->alpha_factor;
         SetCameraName(cv->cam_name);
         SetTextureSize(cv->txt_x, cv->txt_y);
      }
      return I3D_object::Duplicate(frm);
   }

//----------------------------

   I3DMETHOD(SetProperty)(dword index, dword value){

      switch(index){
      case 0:
         camview_alpha = I3DIntAsFloat(value);
         alpha_factor = Max(0, Min(255, FloatToInt(camview_alpha*255.0f)));
         break;
      case 1: SetTextureSize(value, txt_y); break;
      case 2: SetTextureSize(txt_x, value); break;
      case 3: SetCameraName((const char*)value); break;
      default:
         return I3DERR_INVALIDPARAMS;
      }
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(dword,GetProperty)(dword index) const{
      
      switch(index){
      case 0: return I3DFloatAsInt(camview_alpha);
      case 1: return txt_x;
      case 2: return txt_y;
      case 3: return (dword)(const char*)cam_name;
      }
      return 0xcdcdcdcd;
   }
};

//----------------------------

void I3D_object_camview::AddPrimitives(S_preprocess_context &pc){

#ifndef GL
   if(pc.mode==RV_SHADOW_CASTER)
      return;
#endif
   PI3D_mesh_base mb = GetMesh();
   if(!mb)
      return;
   assert(mb);
   const I3D_face_group &fg = *mb->GetFGroupVector().begin();
   dword vertex_count = mb->vertex_buffer.NumVertices();
   assert(vertex_count);
   dword face_count = fg.NumFaces1();
   pc.scene->render_stats.triangle += face_count;
   pc.scene->render_stats.vert_trans += vertex_count;

   int curr_auto_lod = drv->force_lod_index;
   if(curr_auto_lod==-1){
                           //compute automatic LOD based on distance
      curr_auto_lod = GetAutoLOD(pc, mb);
   }else{
      curr_auto_lod = Min(curr_auto_lod, (int)mb->NumAutoLODs()-1);
   }

   pc.prim_list.push_back(S_render_primitive(curr_auto_lod, alpha, this, pc));
   S_render_primitive &p = pc.prim_list.back();
   
   /*
   if(I3DFloatAsInt(alpha)!=FLOAT_ONE_BITMASK){
      p.blend_mode = I3DBLEND_ALPHABLEND | I3DBLEND_VERTEXALPHA;
      p.sort_value = PRIM_SORT_ALPHA_NOZWRITE;
      ++pc.alpha_nozwrite;
      {
                              //sort by distance
         const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
         const S_matrix &m = pc.scene->GetActiveCamera1()->GetViewMatrix1();
         float depth =
            bsphere.pos[0] * m(0, 2) +
            bsphere.pos[1] * m(1, 2) +
            bsphere.pos[2] * m(2, 2) +
                             m(3, 2);
         p.sort_value |= ((FloatToInt(depth * 100.0f))&PRIM_SORT_DIST_MASK) << PRIM_SORT_DIST_SHIFT;
      }
   }else*/
   {
      p.blend_mode = I3DBLEND_OPAQUE;
      p.sort_value = PRIM_SORT_OPAQUE;
      ++pc.opaque;
   }
   if(NeedPrepareDestVb())
      PrepareDestVB(mb);
}

//----------------------------
#ifdef USE_STRIPS
#define DRAW_PRIM \
   hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count, \
      base_index * 3 + si.base_index, si.num_indicies-2); \
   CHECK_D3D_RESULT("DrawIP", hr);
#else
#define DRAW_PRIM \
   hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count, \
      (base_index + fg->base_index) * 3, fg->num_faces); \
   CHECK_D3D_RESULT("DrawIP", hr);
#endif

#ifndef GL
void I3D_object_camview::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   I3D_mesh_base *mb = mesh;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;


   bool cam_mode = (pc.mode==RV_NORMAL);

   {
      if(cam_mode && cam_name.Size()){
         if(!RenderIntoTexture(pc.scene, pc.render_flags))
            cam_mode = false;
      }

      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }

      dword vertex_count;
      const C_auto_lod *auto_lods = mb->GetAutoLODs();
      if(rp.curr_auto_lod < 0){
         vertex_count = mb->vertex_buffer.NumVertices();
      }else{
         vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;
      }

                              //transform vertices
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_DETAIL_MAPS;
      //CPI3D_material mat = mb->GetFGroupVector()[0].GetMaterial1();

      I3D_driver::S_vs_shader_entry_in se;//(vs_decl_object);
      if(drv->IsDirectTransform()){
         prep_flags |= VSPREP_MAKELIGHTING;
         prep_flags |= VSPREP_COPY_UV;
         //if(mat->GetTexture1(MTI_EMBM))
            se.CopyUV(0, 1);
      }else{
         {
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

      PrepareVertexShader(mb->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

      if(!drv->IsDirectTransform()){
         drv->DisableTextureStage(1);

         drv->SetStreamSource(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.GetSizeOfVertex());
         drv->SetVSDecl(vertex_buffer.vs_decl);

         hr = d3d_dev->ProcessVertices(mb->vertex_buffer.D3D_vertex_buffer_index,
            vertex_buffer.D3D_vertex_buffer_index, vertex_count,
            vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
         CHECK_D3D_RESULT("ProcessVertices", hr);
         drv->SetVertexShader(NULL);
      }
   }
   const I3D_face_group *fg;
   int vertex_count;
   int base_index;
#ifdef USE_STRIPS
   const S_fgroup_strip_info *strp_info;
#endif
   if(rp.curr_auto_lod < 0){
                              //direct mesh
      fg = &mb->GetFGroupVector()[0];
      vertex_count = mb->vertex_buffer.NumVertices();
      base_index = mb->GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(mb->GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = mb->GetStripInfo();
#endif
   }else{
      const C_auto_lod &al = mb->GetAutoLODs()[rp.curr_auto_lod];
      fg = &al.fgroups[0];
      vertex_count = al.vertex_count;
      base_index = al.GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(al.GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = al.GetStripInfo();
#endif
   }
   CPI3D_material mat = fg->GetMaterial1();

   {
      drv->SetupBlend(rp.blend_mode);
      drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
                           //apply environment effect
      //SetupSpecialMapping(mat, &rp, 1);
      drv->DisableTextureStage(1);
      drv->EnableAnisotropy(0, true);
   }

                           //debug mode - without textures
   if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
      drv->SetTexture1(0, NULL);

   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
   drv->SetFVF(vertex_buffer.GetFVFlags());
#ifdef USE_STRIPS
   const S_fgroup_strip_info &si = strp_info[0];
#endif

   if(cam_mode){
      bool is_wire = (drv->GetFlags()&DRVF_WIREFRAME);
      if(is_wire) d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

      int stage = 0;
      /*
      if(mat->IsTextureAlpha()){
         drv->SetupTextureStage(stage, drv->vertex_light_blend_op);
         drv->SetRenderMat(mat, stage++, alpha);
         drv->SetupBlend(I3DBLEND_ALPHABLEND);
      }else*/
         drv->SetupBlend(I3DBLEND_OPAQUE);

      CPI3D_texture_base txt_embm = mat->GetTexture1(MTI_EMBM);
      if(txt_embm){
         if(!drv->IsDirectTransform())
            drv->SetTextureCoordIndex(1, stage);
         drv->SetupTextureStage(stage, D3DTOP_BUMPENVMAP);
         drv->SetEMBMScale(stage, mat->GetEMBMOpacity1());
         drv->SetTexture1(stage++, txt_embm);
      }
      drv->SetTexture1(stage, rt);
      drv->SetupTextureStage(stage++, D3DTOP_SELECTARG1);

      drv->DisableTextureStage(stage);

      DRAW_PRIM;

      drv->SetTextureCoordIndex(1, 1);
      drv->DisableTextureStage(1);
      if(is_wire) d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
   }else{
      drv->SetupBlend(I3DBLEND_OPAQUE);
      drv->SetupTextureStage(0, D3DTOP_SELECTARG2);
      drv->SetTextureFactor(0xff808080);
      d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

      DRAW_PRIM;
      d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
   }
                              //re-render with original texture
   if(mat->GetTexture1(MTI_DIFFUSE)){
      drv->SetRenderMat(mat, 0, camview_alpha);
      drv->DisableTextureStage(1);

      drv->SetupBlend(I3DBLEND_ALPHABLEND);
      drv->SetupTextureStage(0, drv->vertex_light_blend_op);

      DRAW_PRIM;
   }
}
#endif
//----------------------------

void I3D_object_camview::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   I3D_mesh_base *mb = mesh;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr; hr = 0;

   bool cam_mode = (pc.mode==RV_NORMAL);

   if(cam_mode && cam_name.Size()){
      if(!RenderIntoTexture(pc.scene, pc.render_flags))
         cam_mode = false;
   }

                              //transform vertices
   dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_DETAIL_MAPS | VSPREP_COPY_UV |
      VSPREP_MAKELIGHTING;

   I3D_driver::S_vs_shader_entry_in se;//(vs_decl_object);

   se.CopyUV(0, 1);

   PrepareVertexShader(mb->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

   const I3D_face_group *fg;
   dword vertex_count;
   int base_index;
#ifdef USE_STRIPS
   const S_fgroup_strip_info *strp_info;
#endif
   if(rp.curr_auto_lod < 0){
                              //direct mesh
      fg = &mb->GetFGroupVector()[0];
      vertex_count = mb->vertex_buffer.NumVertices();
      base_index = mb->GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(mb->GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = mb->GetStripInfo();
#endif
   }else{
      const C_auto_lod &al = mb->GetAutoLODs()[rp.curr_auto_lod];
      fg = &al.fgroups[0];
      vertex_count = al.vertex_count;
      base_index = al.GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(al.GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
      strp_info = al.GetStripInfo();
#endif
   }
   CPI3D_material mat = fg->GetMaterial1();

   I3D_driver::S_ps_shader_entry_in se_ps;

   /*
   if(drv->GetFlags2()&DRVF2_DRAWTEXTURES){
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_t0_COPY);
      drv->EnableAnisotropy(0, true);

      //SetupSpecialMapping(mat, &rp, 1);
   }else{
      se_ps.AddFragment(PSF_WHITE);
   }
   */

   drv->SetupBlend(rp.blend_mode);
#ifndef GL
   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
   drv->SetVSDecl(vertex_buffer.vs_decl);
#else
   drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.GetSizeOfVertex());
   drv->SetVSDecl(mesh->vertex_buffer.vs_decl);
#endif

#ifdef USE_STRIPS
   const S_fgroup_strip_info &si = strp_info[0];
#endif

   if(cam_mode){
      bool is_wire = (drv->GetFlags()&DRVF_WIREFRAME);
      if(is_wire) d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

      int stage = 0;
      /*
      if(mat->IsTextureAlpha()){
         drv->SetupTextureStage(stage, drv->vertex_light_blend_op);
         drv->SetRenderMat(mat, stage++, alpha);
         drv->SetupBlend(I3DBLEND_ALPHABLEND);
      }else*/
      drv->SetupBlend(I3DBLEND_OPAQUE);

      CPI3D_texture_base txt_embm = mat->GetTexture1(MTI_EMBM);
      if(txt_embm){
         se_ps.Tex(stage);
         drv->SetTexture1(stage, txt_embm);
         ++stage;

         se_ps.TexBem(stage);
         se_ps.AddFragment(PSF_t1_COPY);
         drv->SetEMBMScale(stage, mat->GetEMBMOpacity1());
      }else{
         se_ps.Tex(stage);
         se_ps.AddFragment(PSF_t0_COPY);
      }
      drv->SetTexture1(stage, rt);

      drv->SetPixelShader(se_ps);

#ifndef GL
      DRAW_PRIM;
#endif
      if(is_wire) d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
   }else{
      drv->SetupBlend(I3DBLEND_OPAQUE);

      se_ps.AddFragment(PSF_COLOR_COPY);
      static const S_vectorw c(.5f, .5f, .5f, 1);
      drv->SetPSConstant(PSC_COLOR, &c);

      drv->SetPixelShader(se_ps);
#ifndef GL
      DRAW_PRIM;
#endif
   }
                              //re-render with original texture
   if(mat->GetTexture1(MTI_DIFFUSE)){
      bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);

      I3D_driver::S_ps_shader_entry_in se_ps;
      se_ps.Tex(0);
      se_ps.AddFragment(has_diffuse ? PSF_MODX2_t0_v0 : PSF_COPY_BLACK_t0a);
      drv->SetRenderMat(mat, 0, camview_alpha);

      drv->SetPixelShader(se_ps);

      drv->SetupBlend(I3DBLEND_ALPHABLEND);
#ifndef GL
      DRAW_PRIM;
#endif
   }
}

//----------------------------

extern const S_visual_property props_CameraView[] = {
   { I3DPROP_FLOAT, "Alpha" },
   //{I3DPROP_NULL, NULL, NULL},
   {I3DPROP_INT, "Texture X", "Dimension of texture's X side."},
   {I3DPROP_INT, "Texture Y", "Dimension of texture's Y side."},
   {I3DPROP_STRING, "Camera name", "Name of camera in scene's hierarchy, which will be used for camera rendering."},

   {I3DPROP_NULL}
};

I3D_visual *CreateCameraView(PI3D_driver drv){
   return new I3D_object_camview(drv);
}

//----------------------------
//----------------------------


 