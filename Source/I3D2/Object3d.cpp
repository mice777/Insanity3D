/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Object.cpp
   Content: Object frame.
--------------------------------------------------------*/

#include "all.h"
#include "object.h"
#include "scene.h"
#include "mesh.h"

                              //debug:
//#define DEBUG_NO_CACHE_LIGHT  //don't cache light info in vertices

//----------------------------

I3D_visual *CreateObject(PI3D_driver drv){
   return new I3D_object(drv);
}

//----------------------------
//----------------------------
                              //object frame class
I3D_object::I3D_object(PI3D_driver d):
   I3D_visual(d),
   mesh(NULL)
{
   visual_type = I3D_VISUAL_OBJECT;
}

//----------------------------

I3D_object::~I3D_object(){
   SetMeshInternal(NULL);
}

//----------------------------

void I3D_object::SetMeshInternal(I3D_mesh_base *mb){

   if(!mb || !mb->NumVertices1()){
      if(mesh) mesh->Release();
      mesh = NULL;
      return;
   }

   dword new_fvf = mb->vertex_buffer.GetFVFlags() & (D3DFVF_POSITION_MASK|D3DFVF_NORMAL|D3DFVF_TEX1|D3DFVF_TEXTURE_SPACE);
   //mb->vertex_buffer.SetFVFlags(new_fvf);

   //if(mb->vertex_buffer.GetFVFlags()==(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1)){
#ifdef USE_STRIPS
   if(mb->vertex_buffer.GetFVFlags()==new_fvf && mb->HasStrips())
#else
   if(mb->vertex_buffer.GetFVFlags()==new_fvf)
#endif
   {
      if(mb) mb->AddRef();
      if(mesh) mesh->Release();
      mesh = mb;
   }else{
      if(mesh) mesh->Release();
      mesh = drv->CreateMesh(I3DVC_XYZ | I3DVC_NORMAL | (1<<I3DVC_TEXCOUNT_SHIFT));
      mesh->Clone(mb, true);
      //mesh->Release();
   }
#ifndef GL
   vertex_buffer.DestroyD3DVB();
#endif
   vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID | VISF_BOUNDS_VALID);
   frm_flags &= ~FRMFLAGS_HR_BOUND_VALID;
}

//----------------------------

bool I3D_object::LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){

   PI3D_mesh_base mb = drv->CreateMesh(I3DVC_XYZ | I3DVC_NORMAL | (1<<I3DVC_TEXCOUNT_SHIFT));
   bool ok = mb->LoadCachedInfo(ck, lc, mats);
   if(ok)
      SetMesh(mb);
   mb->Release();
   return ok;
}

//----------------------------

I3D_RESULT I3D_object::Duplicate(CPI3D_frame frm){

   if(frm==this)
      return I3D_OK;
   if(frm->GetType1()!=FRAME_VISUAL)
      return I3D_frame::Duplicate(frm);

   CPI3D_visual vis = I3DCAST_CVISUAL(frm);
   SetMeshInternal(const_cast<PI3D_visual>(vis)->GetMesh());
   return I3D_visual::Duplicate(vis);
}

//----------------------------

void I3D_object::SetMesh(PI3D_mesh_base m){

   if(m) m->AddRef();
   if(mesh) mesh->Release();
   mesh = m;
#ifndef GL
   vertex_buffer.DestroyD3DVB();
#endif
   vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID | VISF_BOUNDS_VALID);
   frm_flags &= ~FRMFLAGS_HR_BOUND_VALID;
}

//----------------------------

void I3D_object::PrepareDestVB(I3D_mesh_base *mb, dword num_txt_sets){

   //mesh->ComputeBoundingVolume();
   I3D_visual::PrepareDestVB(mb, num_txt_sets);
   vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
}

//----------------------------

void I3D_object::SetUpdate(){

   vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
}

//----------------------------
//----------------------------

bool I3D_object::ComputeBounds(){

   if(!mesh){
      bound.bound_local.bbox.Invalidate();
      bound.bound_local.bsphere.pos.Zero();
      bound.bound_local.bsphere.radius = 0.0f;
      return true;
   }
   mesh->ComputeBoundingVolume();
   bound.bound_local = mesh->GetBoundingVolume();

   vis_flags |= VISF_BOUNDS_VALID;
   frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;

   return true;
}

//----------------------------

void I3D_object::AddPrimitives(S_preprocess_context &pc){

   if(!mesh)
      return;
   AddPrimitives1(mesh, pc);
}

//----------------------------
#ifndef GL
void I3D_object::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   {
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }

      dword vertex_count;
      const C_auto_lod *auto_lods = mesh->GetAutoLODs();
      if(rp.curr_auto_lod < 0){
         vertex_count = mesh->vertex_buffer.NumVertices();
      }else{
         vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;
      }
                              //transform vertices
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX;

      C_buffer<E_VS_FRAGMENT> *save_lf = NULL;
      C_buffer<S_vectorw> *save_lp = NULL;

      if(drv->IsDirectTransform()){
         if(pc.mode!=RV_SHADOW_CASTER){
            if(!(vis_flags&VISF_DEST_LIGHT_VALID)){
               prep_flags |= VSPREP_MAKELIGHTING;
               vis_flags |= VISF_DEST_LIGHT_VALID;
            }
            save_lf = &save_light_fragments;
            save_lp = &save_light_params;
         }
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

      I3D_driver::S_vs_shader_entry_in se;
      PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags, NULL, save_lf, save_lp);

      if(!drv->IsDirectTransform()){
         drv->DisableTextureStage(1);

         drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.GetSizeOfVertex());
         drv->SetVSDecl(vertex_buffer.vs_decl);
#if defined _DEBUG && 0
         {
            struct S_vertex{
               S_vectorw xyzw;
               dword diffuse;
               dword specular;
               S_vector2 uv;
            };
            D3D_lock<byte> v_dst(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.D3D_vertex_buffer_index, vertex_count,
               sizeof(S_vertex), 0);
            v_dst.AssureLock();
            S_vertex *vp = (S_vertex*)(byte*)v_dst;
            vp = vp;
         }
#endif

         HRESULT hr;
         hr = drv->GetDevice1()->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index,
            vertex_buffer.D3D_vertex_buffer_index, vertex_count,
            vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
         CHECK_D3D_RESULT("ProcessVertices", hr);
         drv->SetVertexShader(NULL);
      }
   }
   DrawPrimitiveVisual(mesh, pc, rp);
}
#endif
//----------------------------

void I3D_object::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

                              //prepare VS
   dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV;

   if(last_auto_lod != rp.curr_auto_lod){
      last_auto_lod = rp.curr_auto_lod;
      vis_flags &= ~VISF_DEST_LIGHT_VALID;
   }
   I3D_driver::S_vs_shader_entry_in se;

#ifndef GL
   if(pc.mode==RV_SHADOW_CASTER){
      PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);
   }else
#endif
   {
      if(!(vis_flags&VISF_DEST_LIGHT_VALID)){
         prep_flags |= VSPREP_MAKELIGHTING;
         vis_flags |= VISF_DEST_LIGHT_VALID;
      }
#if defined _DEBUG && 0
                              //debug - test performing rotation by quaternion
      IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

      prep_flags &= ~(VSPREP_TRANSFORM | VSPREP_FEED_MATRIX);
      se.AddFragment(VSF_TEST);
      drv->SetVSConstant(VSC_MAT_TRANSFORM_0, &rp.scene->GetViewProjHomMatTransposed(), 4);

      S_matrix m = matrix;
      m.Transpose();
      drv->SetVSConstant(40, &m, 4);

      S_quat q = matrix;
      S_vectorw vq = q.v;
      vq.w = q.s;
      S_vectorw pos = matrix(3);
      pos.w = matrix(0).Magnitude();

      drv->SetVSConstant(50, &vq);
      drv->SetVSConstant(51, &pos);
#endif
      PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags, NULL, &save_light_fragments, &save_light_params
#ifdef GL
         , &gl_save_light_params
#endif
         );
   }
   DrawPrimitiveVisualPS(mesh, pc, rp);
}

//----------------------------
//----------------------------


