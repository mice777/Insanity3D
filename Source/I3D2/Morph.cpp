/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Morph.cpp
   Content: Morph frame.
--------------------------------------------------------*/

#include "all.h"
#include "mesh.h"
#include "procedural.h"
#include "visual.h"
#include "light.h"
#include "camera.h"
#include "scene.h"

//----------------------------
//----------------------------

                              //use 2 weights, one for applying morph ratio,
                              // the other for source selection (index)
//#define FVF_VERTEX_MORPH (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2)
#define FVF_VERTEX_MORPH (D3DFVF_XYZB2 | D3DFVF_NORMAL | D3DFVF_TEX1)

struct I3D_vertex_morph{
   S_vector xyz;
   float weight;
   float index;
   S_vector normal;
   I3D_text_coor uv;
};

//----------------------------
                              //morphing mesh
class I3D_morph_mesh: public I3D_mesh_base{

#define MMSHF_RESET_VERTICES     1     //reset weight info in vertices

   dword mesh_flags;

   struct S_region{
      I3D_bbox bbox;
      S_matrix m_bb_inv;
   };
   C_vector<S_region> regions;

public:
   I3D_morph_mesh(PI3D_driver d):
      I3D_mesh_base(d, FVF_VERTEX_MORPH),
      mesh_flags(0)
   {}

                              //source of morphing
   C_smart_ptr<I3D_procedural_morph> procedural;
//----------------------------
// Save/Load mesh - vertices and faces after optimization, vertex normals.
   //virtual bool SaveCachedInfo(class C_cache*) const;
   //virtual bool LoadCachedInfo(class C_cache*, class C_loader &lc);

//----------------------------
// Attach procedural object.
   bool AttachProcedural(PI3D_procedural_base pb){

                                 //check if procedural is of expected type
      if(pb->GetID() == PROCID_MORPH_3D){
         procedural = (I3D_procedural_morph*)pb;
         mesh_flags |= MMSHF_RESET_VERTICES;
                                 //init vertices - setup weights
         return true;
      }
      return false;
   }

   inline const I3D_procedural_morph *GetProcedural() const{ return procedural; }

//----------------------------
// Add region into list.
   bool AddRegion(const I3D_bbox &bb, const S_matrix &m){

      regions.push_back(S_region());
      S_region &rg = regions.back();
      rg.bbox = bb;
      rg.m_bb_inv = ~m;
      return true;
   }

   inline int NumRegions() const{ return regions.size(); }

//----------------------------
// Prepare morphing - setup weights, etc. This is done before 1st rendering,
// only if MMSHF_RESET_VERTICES is set.
   void PrepareMorphing(){

      dword num_verts = NumVertices1();
      C_vector<word> v_map(num_verts);

      I3D_vertex_morph *verts = (I3D_vertex_morph*)vertex_buffer.Lock();

      MakeVertexMapping(&verts->xyz, sizeof(I3D_vertex_morph), num_verts, &*v_map.begin());

      if(!procedural){
                                 //just reset to zero
         for(int i=num_verts; i--; ){
            I3D_vertex_morph &v = verts[i];
            v.weight = 0.0f;
            v.index = 0.0f;
         }
      }else{
         dword num_channels = procedural->GetNumChannels();
         for(dword i=0; i<num_verts; i++){
            I3D_vertex_morph &v = verts[i];
            if(v_map[i]==i){
                                 //original vertex
               if(regions.size()){
                  v.weight = 0.0f;
                                    //check if vertex is in any region
                  for(int ri=regions.size(); ri--; ){
                     if(IsPointInBBox(v.xyz, regions[ri].bbox, regions[ri].m_bb_inv, v.weight))
                        break;
                  }
               }else{
                  v.weight = 1.0f;
               }
               v.index = (float)S_int_random(num_channels);
            }else{
                                 //shared vertex
               const I3D_vertex_morph &vs = verts[v_map[i]];
               v.weight = vs.weight;
               v.index = vs.index;
            }
         }     
      }
      vertex_buffer.Unlock();
   }

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
};

//----------------------------
//----------------------------

class I3D_object_morph: public I3D_visual{
   C_smart_ptr<I3D_morph_mesh> mesh;

   enum E_MODE{
      MODE_SINGLE_CHANNEL,    //all verts morphed by single channel (#0)
      MODE_PER_REGION_CHANNEL,//each region morphed by random channel
      MODE_PER_VERTEX_CHANNEL,//each vertex morphed by random channel
      MODE_LAST
   } mode;

   int channel;               //procedural channel we're using if MODE_SINGLE_CHANNEL
   float power;
public:
   I3D_object_morph(PI3D_driver d):
      I3D_visual(d),
      power(1.0f),
      mode(MODE_SINGLE_CHANNEL),
      channel(-1)
   {
      visual_type = I3D_VISUAL_MORPH;
   }

//----------------------------

   I3DMETHOD(Duplicate)(CPI3D_frame frm){

      if(frm==this)
         return I3D_OK;
      if(frm->GetType1()!=FRAME_VISUAL)
         return I3D_frame::Duplicate(frm);

      CPI3D_visual vis = I3DCAST_CVISUAL(frm);
      switch(vis->GetVisualType1()){
      case I3D_VISUAL_MORPH:
         {
            const I3D_object_morph *mrph = (I3D_object_morph*)vis;
            mesh = (I3D_morph_mesh*)mrph->GetMesh();
            channel = mesh->procedural ? S_int_random(mesh->procedural->GetNumChannels()) : -1;
            mode = mrph->mode;
            power = mrph->power;
         }
         break;
      default:
         SetMeshInternal(const_cast<PI3D_visual>(vis)->GetMesh());
      }
      return I3D_visual::Duplicate(vis);
   }

//----------------------------

   I3DMETHOD_(PI3D_mesh_base,GetMesh)(){ return mesh; }
   I3DMETHOD_(CPI3D_mesh_base,GetMesh)() const{ return mesh; }

//----------------------------

   I3DMETHOD(SetProperty)(dword index, dword value){

      switch(index){
      case I3DPROP_MRPH_F_POWER:
         power = I3DIntAsFloat(value);
         power = Max(0.0f, Min(1.0f, power));
         break;
      case I3DPROP_MRPH_E_MODE:
         if(value >= MODE_LAST) 
            return I3DERR_INVALIDPARAMS;
         mode = (E_MODE)value;
         break;
      default: return I3DERR_INVALIDPARAMS;
      }
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(dword,GetProperty)(dword index) const{

      switch(index){
      case I3DPROP_MRPH_F_POWER: return I3DFloatAsInt(power);
      case I3DPROP_MRPH_E_MODE: return mode;
      }
      return 0xcdcdcdcd;
   }

//----------------------------

   virtual void AddPrimitives(S_preprocess_context &pc){

      if(!mesh)
         return;
      AddPrimitives1(mesh, pc);
   }

//----------------------------

#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
//----------------------------

   virtual bool ComputeBounds(){

      if(!mesh){
         bound.bound_local.bbox.Invalidate();
         bound.bound_local.bsphere.pos.Zero();
         bound.bound_local.bsphere.radius = 0.0f;
      }else{
         mesh->ComputeBoundingVolume();
         bound.bound_local = mesh->GetBoundingVolume();
                                 //expand by maximum of morphing radius
         if(mesh->procedural){
            S_vector proc_scale = mesh->procedural->GetMaxMorphScale();
            I3D_bbox &bbox = bound.bound_local.bbox;
            bbox.min -= proc_scale;
            bbox.max += proc_scale;
            bound.bound_local.bsphere.radius += proc_scale.Magnitude();
         }
      }
      //return I3D_visual::ComputeBounds();
      vis_flags |= VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
      return true;
   }

//----------------------------

   virtual bool LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){
      mesh = new I3D_morph_mesh(drv); mesh->Release();
      bool ok = mesh->LoadCachedInfo(ck, lc, mats);
      if(!ok)
         mesh = NULL;
      return ok;

   }

//----------------------------

   virtual void SetMeshInternal(PI3D_mesh_base mb){
      mesh = NULL;
      if(mb){
         mesh = new I3D_morph_mesh(drv);
         mesh->Clone(mb, true);
         mesh->Release();
      }
   }

//----------------------------

   virtual bool AttachProcedural(PI3D_procedural_base pb){

      if(mesh && pb->GetID()==PROCID_MORPH_3D){
         if(!mesh->AttachProcedural(pb))
            return false;
         channel = S_int_random(mesh->procedural->GetNumChannels());
         return true;
      }
      return I3D_visual::AttachProcedural(pb);
   }

//----------------------------

   virtual bool AddRegion(const I3D_bbox &bb, const S_matrix &m, int index){

      if(mesh)
         return mesh->AddRegion(bb, m);
      return false;
   }

//----------------------------

   virtual void PrepareDestVB(I3D_mesh_base *mb, dword num_txt_sets){

      mesh->PrepareMorphing();
      I3D_visual::PrepareDestVB(mb, num_txt_sets);
   }

//----------------------------

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
};

//----------------------------
//----------------------------
#ifndef GL
void I3D_object_morph::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   HRESULT hr;

   {
      IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }
      dword vertex_count;
      if(rp.curr_auto_lod < 0)
         vertex_count = mesh->vertex_buffer.NumVertices();
      else
         vertex_count = mesh->GetAutoLODs()[rp.curr_auto_lod].vertex_count;

                              //transform vertices
      dword prep_flags = VSPREP_FEED_MATRIX;

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

      I3D_driver::S_vs_shader_entry_in se;

      I3D_procedural_morph *proc = (I3D_procedural_morph*)mesh->GetProcedural();
      if(proc){
         proc->Evaluate();
         switch(mode){
         case MODE_SINGLE_CHANNEL:
         case MODE_PER_REGION_CHANNEL: //to do!
            {
               se.AddFragment(VSF_TRANSFORM_MORPH1_OS);
                                    //setup morphing constants
               assert(channel!=-1);
               S_vectorw vw = proc->GetMorphingValues()[channel];
               vw *= power;
               hr = d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE, (const float*)&vw, 1);
               CHECK_D3D_RESULT("SetVertexShaderConstantF", hr);
            }
            break;
         case MODE_PER_VERTEX_CHANNEL:
            {
               se.AddFragment(VSF_TRANSFORM_MORPH2_OS);
               int numch = proc->GetNumChannels();
               S_vectorw *vw = (S_vectorw*)alloca(numch * sizeof(S_vectorw));
               memcpy(vw, proc->GetMorphingValues(), numch * sizeof(S_vectorw));
               for(int i=numch; i--; ){
                  vw[i] *= power;
               }
               hr = d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE, (const float*)vw, numch);
               CHECK_D3D_RESULT("SetVertexShaderConstantF", hr);
            }
            break;
         default: assert(0);
         }
      }else{
         se.AddFragment(VSF_TRANSFORM);
      }
      PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

      if(!drv->IsDirectTransform()){
         drv->DisableTextureStage(1);

         drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), sizeof(I3D_vertex_morph));
         drv->SetVSDecl(vertex_buffer.vs_decl);

         hr = d3d_dev->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index,
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

void I3D_object_morph::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   HRESULT hr;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   dword vertex_count;
   if(rp.curr_auto_lod < 0)
      vertex_count = mesh->vertex_buffer.NumVertices();
   else
      vertex_count = mesh->GetAutoLODs()[rp.curr_auto_lod].vertex_count;

                           //transform vertices
   dword prep_flags = VSPREP_COPY_UV | VSPREP_MAKELIGHTING;
   I3D_driver::S_vs_shader_entry_in se;

   bool object_space =
#ifndef GL
      !(drv->GetFlags2()&DRVF2_TEXCLIP_ON);
#else
      false;
#endif
#ifdef _DEBUG
   //if(drv->debug_int[0]) object_space = false;
#endif

   I3D_procedural_morph *proc = (I3D_procedural_morph*)mesh->GetProcedural();
   if(proc){
      proc->Evaluate();
      switch(mode){
      case MODE_SINGLE_CHANNEL:
      case MODE_PER_REGION_CHANNEL: //to do!
         {
            se.AddFragment(object_space ? VSF_TRANSFORM_MORPH1_OS : VSF_TRANSFORM_MORPH1_WS);
                                 //setup morphing constants
            assert(channel!=-1);
            S_vectorw vw = proc->GetMorphingValues()[channel];
            vw *= power;
            hr = d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE, (const float*)&vw, 1);
            CHECK_D3D_RESULT("SetVertexShaderConstantF", hr);
         }
         break;
      case MODE_PER_VERTEX_CHANNEL:
         {
            se.AddFragment(object_space ? VSF_TRANSFORM_MORPH2_OS : VSF_TRANSFORM_MORPH2_WS);
            int numch = proc->GetNumChannels();
            S_vectorw *vw = (S_vectorw*)alloca(numch * sizeof(S_vectorw));
            memcpy(vw, proc->GetMorphingValues(), numch * sizeof(S_vectorw));
            for(int i=numch; i--; ){
               vw[i] *= power;
            }
            hr = d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE, (const float*)vw, numch);
            CHECK_D3D_RESULT("SetVertexShaderConstantF", hr);
         }
         break;
      default: assert(0);
      }
   }else{
      se.AddFragment(VSF_TRANSFORM);
   }
   if(object_space){
      PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags | VSPREP_FEED_MATRIX);
   }else{
      PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags | VSPREP_WORLD_SPACE);
      d3d_dev->SetVertexShaderConstantF(VSC_MAT_TRANSFORM_0, (const float*)&rp.scene->GetViewProjHomMatTransposed(), 4);
      S_matrix mt = matrix;
      mt.Transpose();
      d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE+(MAX_VS_BLEND_MATRICES-1)*3, (const float*)&mt, 3);
   }
   
   DrawPrimitiveVisualPS(mesh, pc, rp);
}

//----------------------------
//----------------------------
extern const S_visual_property props_Morph[] = {
                              //I3DPROP_MRPH_F_POWER
   {I3DPROP_FLOAT, "Morph power", "Power of morphing. This value must be in range from 0 to 1."},
                              //I3DPROP_MRPH_E_MODE
   {I3DPROP_ENUM, "Channel\0Single\0Regions\0Vertices\0", "Morphing mode."},
   {I3DPROP_NULL}
};

I3D_visual *CreateMorph(PI3D_driver drv){
   return new I3D_object_morph(drv);
}

//----------------------------
//----------------------------

