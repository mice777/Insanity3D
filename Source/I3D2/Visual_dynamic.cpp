#include "all.h"
#include "visual.h"
#include "mesh.h"

//----------------------------

class I3D_visual_dynamic: public I3D_visual{

#define DYF_IN_LOCKED_VERTS      1     //set while vertices are locked
#define DYF_IN_LOCKED_FACES      2     //set while faces are locked

   dword dyn_flags;

   C_smart_ptr<I3D_mesh_base> mesh;

//----------------------------

   inline void AddPrimitives1(I3D_mesh_base *mb, S_preprocess_context &pc, bool vertex_alpha){

      int curr_auto_lod = -1;
      const C_auto_lod *auto_lods = mb->GetAutoLODs();
#ifndef GL
      if(pc.mode!=RV_SHADOW_CASTER)
#endif
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
      int vertex_count;
      if(curr_auto_lod>=0){
         fgrps = &auto_lods[curr_auto_lod].fgroups;
         vertex_count = auto_lods[curr_auto_lod].vertex_count;
      }else
      {
         fgrps = &mb->GetFGroupVector();
         vertex_count = mb->vertex_buffer.NumVertices();
      }
      if(!vertex_count)
         return;

                              //add primitives to sort list
      for(int ii=fgrps->size(); ii--; ){
         const I3D_face_group &fg = (*fgrps)[ii];
         dword face_count = fg.NumFaces1();
         assert(face_count);

                              //driver stats
         pc.scene->render_stats.triangle += face_count;

         pc.prim_list.push_back(S_render_primitive(curr_auto_lod, alpha, this, pc));
         S_render_primitive &p = pc.prim_list.back();
      
         p.user1 = ii;

         bool is_alpha = false;
         dword sort_value;
                              //determine blending mode
         p.blend_mode = I3DBLEND_OPAQUE;
         CPI3D_material mat = fg.GetMaterial1();

         if(alpha != 255){
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
         }else{
            p.blend_mode = mat->IsAddMode() ? I3DBLEND_ADD : I3DBLEND_ALPHABLEND;
            is_alpha = true;
            sort_value = PRIM_SORT_ALPHA_NOZWRITE;
            ++pc.alpha_nozwrite;
         }
         //if(mp->IsDiffuseAlpha() || vertex_alpha) p.blend_mode |= I3DBLEND_VERTEXALPHA;

         if(!is_alpha){
                              //sort by material
            sort_value |= (mat->GetSortID()&PRIM_SORT_MAT_MASK)<<PRIM_SORT_MAT_SHIFT;
         }else{
                              //sort by distance
            const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
            const S_matrix &m_view = pc.scene->GetActiveCamera1()->GetViewMatrix1();
            float depth =
               bsphere.pos[0] * m_view(0, 2) +
               bsphere.pos[1] * m_view(1, 2) +
               bsphere.pos[2] * m_view(2, 2) +
                                m_view(3, 2);
            sort_value |= ((FloatToInt(depth * 100.0f))&PRIM_SORT_DIST_MASK) << PRIM_SORT_DIST_SHIFT;
         }
         p.sort_value = sort_value;
      }
                              //make sure destination VB is allocated
      if(NeedPrepareDestVb())
         PrepareDestVB(mb);
                              //driver's stats
      pc.scene->render_stats.vert_trans += vertex_count;
   }

//----------------------------
public:
   I3D_visual_dynamic(PI3D_driver d):
      I3D_visual(d),
      dyn_flags(0)
   {
      visual_type = I3D_VISUAL_DYNAMIC;
   }
   ~I3D_visual_dynamic(){
      mesh = NULL;
   }

public:
                              //specialized methods of I3D_visual:
   virtual bool ComputeBounds();
   virtual void PrepareDestVB(I3D_mesh_base*, dword num_txt_sets = 1);
   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD_(PI3D_mesh_base,GetMesh)(){ return mesh; }
   I3DMETHOD_(CPI3D_mesh_base,GetMesh)() const{ return mesh; }
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(I3D_RESULT,Build)(const void *verts, dword num_verts, dword vc_flags,
      const I3D_triface *faces, dword num_faces,
      const I3D_face_group *fgroups, dword num_fgroups, dword flags);
   /*
   I3DMETHOD_(I3D_RESULT,BuildAutoLODs)(float min_dist, float max_dist, dword num_lods, float lowest_quality){
      return I3DERR_UNSUPPORTED;
   }
   */
   I3DMETHOD_(I3D_RESULT,DuplicateMesh)(CPI3D_mesh_base, dword vc_flags);
   I3DMETHOD_(I3D_RESULT,ResetLightInfo)(){
      if(!mesh)
         return I3DERR_NOTINITIALIZED;
      vis_flags &= ~VISF_DEST_LIGHT_VALID;
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(void*,LockVertices)(dword lock_flags){

      if(!mesh)
         return NULL;
      if(dyn_flags&DYF_IN_LOCKED_VERTS)
         return NULL;
      if(lock_flags&(~(I3DLOCK_VERTEX | I3DLOCK_NO_LIGHT_RESET))){
         DEBUG_LOG("invalid flags passed to I3D_visual_dynamic::LockVertices");
         return NULL;
      }

      dword d3d_lock_flags = (lock_flags&I3DLOCK_VERTEX) ? 0 : D3DLOCK_READONLY;
      void *vp;
      /*
      if(drv->IsDirectTransform()){
         if(!vertex_buffer.GetD3DVertexBuffer())
            PrepareDestVB(mesh);
         vp = (void*)vertex_buffer.Lock(d3d_lock_flags);
      }else{
      */
         vp = (void*)mesh->vertex_buffer.Lock(d3d_lock_flags);
      //}

      if(vp){
         dyn_flags |= DYF_IN_LOCKED_VERTS;

         if(lock_flags&(I3DLOCK_XYZ | I3DLOCK_NORMAL | I3DLOCK_DIFFUSE)){
            if(!(lock_flags&I3DLOCK_NO_LIGHT_RESET))
               vis_flags &= ~VISF_DEST_LIGHT_VALID;
         }
         if(lock_flags&I3DLOCK_UV)
            vis_flags &= ~VISF_DEST_UV0_VALID;

         if(lock_flags&I3DLOCK_XYZ)
            vis_flags &= ~VISF_BOUNDS_VALID;
      }
      return vp;
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,UnlockVertices)(){

      if(!(dyn_flags&DYF_IN_LOCKED_VERTS))
         return I3DERR_INVALIDPARAMS;
      if(!mesh)
         return I3DERR_NOTINITIALIZED;
      dyn_flags &= ~DYF_IN_LOCKED_VERTS;

      mesh->vertex_buffer.Unlock();
#ifndef GL
      if(drv->IsDirectTransform() && vertex_buffer.GetD3DVertexBuffer()){
                              //in direct mode, re-copy changed values to vetex dest buffer now
                              // (not an optimal way to do it, but we must retain sysmem copy)
         dword numv = mesh->NumVertices1();
         //vertex_buffer.DestroyD3DVB(drv);
                                 //copy data
         D3D_lock<byte> v_src(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.D3D_vertex_buffer_index,
            numv, mesh->vertex_buffer.GetSizeOfVertex(), D3DLOCK_READONLY);
         D3D_lock<byte> v_dst(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.D3D_vertex_buffer_index, numv,
            vertex_buffer.GetSizeOfVertex(), 0);
         v_src.AssureLock();
         v_dst.AssureLock();
         memcpy(v_dst, v_src, numv*vertex_buffer.GetSizeOfVertex());
      }
#endif
      return I3D_OK;
   }

//----------------------------

   /*
   I3DMETHOD_(PI3D_triface,LockFaces)(dword lock_flags){

      if(!mesh)
         return NULL;
      if(dyn_flags&DYF_IN_LOCKED_FACES)
         return NULL;
      if(lock_flags&(~I3DLOCK_INDEX)){
         DEBUG_LOG("invalid flags passed to I3D_visual_dynamic::LockFaces");
         return NULL;
      }
      //PI3D_triface fp = (PI3D_triface)mesh->LockFaces();
      PI3D_triface fp = const_cast<I3D_source_index_buffer&>(mesh->GetIndexBuffer()).Lock(!lock_flags ? D3DLOCK_READONLY : 0);
      if(fp)
         dyn_flags |= DYF_IN_LOCKED_FACES;
      return fp;
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,UnlockFaces)(){
      if(!(dyn_flags&DYF_IN_LOCKED_FACES))
         return I3DERR_INVALIDPARAMS;
      if(!mesh)
         return I3DERR_NOTINITIALIZED;
      dyn_flags &= ~DYF_IN_LOCKED_FACES;
      //mesh->UnlockFaces();
      const_cast<I3D_source_index_buffer&>(mesh->GetIndexBuffer()).Unlock();
      return I3D_OK;
   }
   */

//----------------------------

   I3DMETHOD_(I3D_RESULT,Render)() const{

      if(!mesh)
         return I3DERR_INVALIDPARAMS;
                              //allow only rendering of XYZRHW formats
      dword fvf_flags = mesh->vertex_buffer.GetFVFlags();
      if((fvf_flags&D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW)
         return I3DERR_INVALIDPARAMS;

      PROFILE(drv, PROF_RENDER);

      drv->SetFVF(fvf_flags);
      IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
      HRESULT hr;

      dword vertex_count = mesh->vertex_buffer.NumVertices();
      dword base_index = mesh->GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(mesh->GetIndexBuffer().GetD3DIndexBuffer());
      //drv->SetClipping(true);
      drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.GetSizeOfVertex());

      drv->SetupBlend(I3DBLEND_ALPHABLEND);
      drv->EnableFog(false);

      if(drv->CanUsePixelShader()){
         for(dword i=mesh->NumFGroups1(); i--; ){
            const I3D_face_group *fg = &mesh->GetFGroupVector()[i];
            CPI3D_material mat = fg->GetMaterial1();
            if(!mat)
               return I3DERR_INVALIDPARAMS;
            drv->SetRenderMat(mat, 0, .1f);

            I3D_driver::S_ps_shader_entry_in se_ps;
            dword stage = 0;
            if(drv->GetFlags2()&DRVF2_DRAWTEXTURES){
               se_ps.Tex(0);
               if(fvf_flags&D3DFVF_DIFFUSE)
                  se_ps.AddFragment(PSF_MOD_t0_v0);
               else
                  se_ps.AddFragment(PSF_t0_COPY);
               ++stage;
            }else{
               se_ps.AddFragment(PSF_v0_COPY);
            }
#ifndef GL
            if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
               se_ps.TexKill(stage);
#endif
                              //apply environment effect
            SetupSpecialMappingPS(mat, se_ps, 1);
            drv->SetPixelShader(se_ps);
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mesh->vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
            CHECK_D3D_RESULT("DrawIP", hr);
         }
      }
#ifndef GL
      else{
         if(fvf_flags&D3DFVF_DIFFUSE){
            drv->SetupTextureStage(0, D3DTOP_MODULATE);
         }else{
            drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         }
      
         for(dword i=mesh->NumFGroups1(); i--; ){
            const I3D_face_group *fg = &mesh->GetFGroupVector()[i];
            CPI3D_material mat = fg->GetMaterial1();
            if(!mat)
               return I3DERR_INVALIDPARAMS;
            drv->SetRenderMat(mat, 0, .1f);

                              //apply environment effect
            SetupSpecialMapping(mat, NULL, 1);
                              //debug mode - without textures
            if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
               drv->SetTexture1(0, NULL);
            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mesh->vertex_buffer.D3D_vertex_buffer_index, 0,
               vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
            CHECK_D3D_RESULT("DrawIP", hr);
         }
      }
#endif
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(dword,NumVertices)() const{ return mesh ? mesh->NumVertices1() : 0; }
   I3DMETHOD_(dword,NumFGroups)() const{ return mesh ? mesh->NumFGroups1() : 0; }
   I3DMETHOD_(dword,NumFaces)() const{ return mesh ? mesh->NumFaces1() : 0; }
   I3DMETHOD_(CPI3D_face_group,GetFGroups)() const{ return !mesh ? NULL : mesh->GetFGroups(); }
};

//----------------------------
#define DL_BASE "I3D_visual_dynamic::Build: "

I3D_RESULT I3D_visual_dynamic::Build(const void *verts, dword num_verts, dword vc_flags,
   const I3D_triface *faces, dword num_faces,
   const I3D_face_group *fgroups, dword num_fgroups,
   dword flags){

                              //make data validity checks
   if(!num_verts || !num_faces || !num_fgroups){
      DEBUG_LOG(DL_BASE"source data cannot be NULL");
      return I3DERR_INVALIDPARAMS;
   }
   if(num_faces != fgroups[num_fgroups-1].base_index+fgroups[num_fgroups-1].num_faces){
      DEBUG_LOG(DL_BASE"last face group must use last faces up to end");
      return I3DERR_INVALIDPARAMS;
   }

   if((vc_flags&I3DVC_XYZ) && (vc_flags&I3DVC_XYZRHW)){
      DEBUG_LOG(DL_BASE"input vertex format must contain XYZ or XYZRHW vertex flag");
      return I3DERR_INVALIDPARAMS;
   }

   if((vc_flags&I3DVC_NORMAL) && (vc_flags&I3DVC_DIFFUSE)){
      DEBUG_LOG(DL_BASE"cannot specify both I3DVC_NORMAL and I3DVC_DIFFUSE");
      return I3DERR_INVALIDPARAMS;
   }
   if(dyn_flags&(DYF_IN_LOCKED_VERTS|DYF_IN_LOCKED_FACES)){
      DEBUG_LOG(DL_BASE"cannot build while vertices or faces are locked");
      return I3DERR_INVALIDPARAMS;
   }

#ifdef _DEBUG
   int i;
                              //check if fgroups ok
   for(i=num_fgroups; i--; ){
      if((fgroups[i].base_index + fgroups[i].num_faces) > num_faces || !fgroups[i].mat){
         DEBUG_LOG(DL_BASE"invalid face group");
         assert(0);
         return I3DERR_INVALIDPARAMS;
      }
   }
                              //check if indicies ok
   for(i=num_faces; i--; ){
      const I3D_triface &fc = faces[i];
      if(fc[0]>=num_verts || fc[1]>=num_verts || fc[2]>=num_verts){
         assert(0);
         DEBUG_LOG(DL_BASE"vertex index out of range");
         return I3DERR_INVALIDPARAMS;
      }
   }
#endif
   dword fvf = ConvertFlags(vc_flags);
   if(fvf==-1){
      DEBUG_LOG(DL_BASE"invalid vertex component flags");
      return I3DERR_INVALIDPARAMS;
   }
                              //create mesh (if not yet)
   if(!mesh || mesh->vertex_buffer.GetFVFlags()!=fvf){
      mesh = new I3D_mesh_base(drv, fvf);
      mesh->Release();
   }
                              //assign vertices
   mesh->vertex_buffer.SetVertices(verts, num_verts);
                              //assign fgroups
   mesh->SetFGroups(fgroups, num_fgroups);
                              //assign faces
   bool use_strips = (!(vc_flags&I3DVC_XYZRHW));
   mesh->SetFaces(faces, use_strips);

   vis_flags &= ~(VISF_USE_ENVMAP | VISF_USE_DETMAP | VISF_USE_EMBMMAP | VISF_USE_BUMPMAP | VISF_BOUNDS_VALID);
   int num_txt_sets = (vc_flags&I3DVC_TEXCOUNT_MASK) >> I3DVC_TEXCOUNT_SHIFT;
   if(num_fgroups==1){
      CPI3D_material mat = fgroups[0].mat;
      if(mat->GetTexture1(MTI_ENVIRONMENT) && num_txt_sets>=2){
         vis_flags |= VISF_USE_ENVMAP;
         if(mat->GetTexture1(MTI_EMBM) && num_txt_sets>=3)
            vis_flags |= VISF_USE_EMBMMAP;
      }
      if(mat->GetTexture1(MTI_DETAIL) && num_txt_sets>=2)
         vis_flags |= VISF_USE_DETMAP;
   }

                              //invalidate hierarchy bounds
   frm_flags &= ~FRMFLAGS_HR_BOUND_VALID;
   {
      PI3D_frame frm1a = this;
      do{
         frm1a->SetFrameFlags(frm1a->GetFrameFlags() & ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID));
         frm1a = frm1a->GetParent1();
      }while(frm1a);
   }
#ifndef GL
   vertex_buffer.DestroyD3DVB();
#endif
   return I3D_OK;
}

//----------------------------

bool I3D_visual_dynamic::ComputeBounds(){

   if(!mesh){
      bound.bound_local.bbox.Invalidate();
      bound.bound_local.bsphere.pos.Zero();
      bound.bound_local.bsphere.radius = 0.0f;
   }else{
      mesh->ComputeBoundingVolume();
      bound.bound_local = mesh->GetBoundingVolume();
   }
   //return I3D_visual::ComputeBounds();
   vis_flags |= VISF_BOUNDS_VALID;
   frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
   return true;
}

//----------------------------

void I3D_visual_dynamic::PrepareDestVB(I3D_mesh_base *mb, dword num_txt_sets){

#ifndef GL
   dword numv = mb->NumVertices1();
   if(!numv)
      return;

   /*
   envmap_uv_set = -1;
   vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);

   if(num_txt_sets < drv->MaxSimultaneousTextures()){
                           //search for any env-mapped material
      for(int j=mb->NumFGroups1(); j--; ){
         PI3D_material mat = mb->GetFGroups1()[j].mat;
         PI3D_texture_base tp = mat->GetTexture(MTI_ENVIRONMENT);
         if(tp){
            envmap_uv_set = num_txt_sets++;
            break;
         }
      }
   }
   */
   dword mesh_fvf = mesh->vertex_buffer.GetFVFlags();

   if(drv->IsDirectTransform()){
      vertex_buffer.CreateD3DVB(mesh_fvf, numv, true);

      assert(vertex_buffer.GetSizeOfVertex() == mb->vertex_buffer.GetSizeOfVertex());
                                 //copy data
      D3D_lock<byte> v_src(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.D3D_vertex_buffer_index,
         numv, mb->vertex_buffer.GetSizeOfVertex(), D3DLOCK_READONLY);
      D3D_lock<byte> v_dst(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.D3D_vertex_buffer_index, numv,
         vertex_buffer.GetSizeOfVertex(), 0);
      v_src.AssureLock();
      v_dst.AssureLock();
      memcpy(v_dst, v_src, numv*vertex_buffer.GetSizeOfVertex());
      vertex_buffer.vs_decl = drv->GetVSDeclaration(vertex_buffer.GetFVFlags());
   }
#ifndef GL
   else{

      dword fvflags = num_txt_sets<<D3DFVF_TEXCOUNT_SHIFT;
                              //include diffuse only if normal, diffuse or alpha present
      if((mesh_fvf&D3DFVF_NORMAL) || (mesh_fvf&D3DFVF_DIFFUSE) || ((mesh_fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZB1)){
         fvflags |= D3DFVF_DIFFUSE;
      }
                              //create dest VB
      vertex_buffer.CreateD3DVB(fvflags, numv, false, false);
      vertex_buffer.vs_decl = drv->GetVSDeclaration(mb->vertex_buffer.GetFVFlags());
   }
#endif
#endif
   vis_flags |= VISF_DEST_PREPARED;
}

//----------------------------

I3D_RESULT I3D_visual_dynamic::Duplicate(CPI3D_frame frm){

   if(frm==this)
      return I3D_OK;
   if(frm->GetType1()!=FRAME_VISUAL)
      return I3D_frame::Duplicate(frm);

   CPI3D_visual vis = I3DCAST_CVISUAL(frm);
   mesh = NULL;
   CPI3D_mesh_base mb = vis->GetMesh();
   if(mb){
      dword fvf = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
      if(vis->GetVisualType1()==visual_type)
         fvf = ((I3D_visual_dynamic*)vis)->mesh->vertex_buffer.GetFVFlags();
      mesh = new I3D_mesh_base(drv, fvf);
      mesh->Clone(mb, true);
      mesh->Release();
   }
   return I3D_visual::Duplicate(vis);
}

//----------------------------

I3D_RESULT I3D_visual_dynamic::DuplicateMesh(CPI3D_mesh_base mb, dword vc_flags){

   dword fvf = ConvertFlags(vc_flags);
   if(fvf==-1)
      return I3DERR_INVALIDPARAMS;
   mesh = new I3D_mesh_base(drv, fvf);
   mesh->Clone(mb, true);
   mesh->Release();
   return I3D_OK;
}

//----------------------------

void I3D_visual_dynamic::AddPrimitives(S_preprocess_context &pc){

   if(!mesh)
      return;
#ifndef GL
                              //don't allow in shadow casting
   if(pc.mode==RV_SHADOW_CASTER)
      return;
#endif

   dword fvf = mesh->vertex_buffer.GetFVFlags();
   bool vertex_alpha = ((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZB1);

   AddPrimitives1(mesh, pc, vertex_alpha);
}

//----------------------------
#ifndef GL
void I3D_visual_dynamic::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   {
      dword vertex_count;
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }
      if(rp.curr_auto_lod>=0)
         vertex_count = mesh->GetAutoLODs()[rp.curr_auto_lod].vertex_count;
      else
         vertex_count = mesh->vertex_buffer.NumVertices();

                              //transform vertices
      I3D_driver::S_vs_shader_entry_in se;

      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX;
      if(pc.mode!=RV_SHADOW_CASTER){
         dword fvf = mesh->vertex_buffer.GetFVFlags();
         bool vertex_alpha = ((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZB1);
                              //check if lighting requested
         if(vertex_alpha || drv->IsDirectTransform() || !(vis_flags&VISF_DEST_LIGHT_VALID)){
            if(fvf&D3DFVF_NORMAL){
                                 //use lighting engine to make lighting
               prep_flags |= VSPREP_MAKELIGHTING;
               if(vertex_alpha)
                  prep_flags |= VSPREP_LIGHT_MUL_ALPHA;
            }else
            if(fvf&D3DFVF_DIFFUSE){
               if(!vertex_alpha){
                              //plain copy of diffuse
                  se.AddFragment(VSF_DIFFUSE_COPY);
               }else{
                  se.AddFragment(VSF_DIFFUSE_MUL_ALPHA);
               }
            }else
            if((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZB1)
               se.AddFragment(VSF_COPY_ALPHA);
            if(drv->IsDirectTransform())
               prep_flags |= VSPREP_COPY_UV;
            else
               vis_flags |= VISF_DEST_LIGHT_VALID;
         }
      }
                              //bug in PV - always copy uv
      //if(!(vis_flags&VISF_DEST_UV0_VALID)){
         prep_flags |= VSPREP_COPY_UV;
         //vis_flags |= VISF_DEST_UV0_VALID;
      //}

      PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

      if(!drv->IsDirectTransform()){
         drv->DisableTextureStage(1);

         drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.GetSizeOfVertex());
         drv->SetFVF(mesh->vertex_buffer.GetFVFlags());

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

void I3D_visual_dynamic::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   dword vertex_count;
   if(rp.curr_auto_lod < 0)
      vertex_count = mesh->vertex_buffer.NumVertices();
   else
      vertex_count = mesh->GetAutoLODs()[rp.curr_auto_lod].vertex_count;

                              //transform vertices
   I3D_driver::S_vs_shader_entry_in se;

   dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV;
#ifndef GL
   if(pc.mode!=RV_SHADOW_CASTER)
#endif
   {
      dword fvf = mesh->vertex_buffer.GetFVFlags();
      bool vertex_alpha = ((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZB1);

                              //check if lighting requested
      if(fvf&D3DFVF_NORMAL){
                              //use lighting engine to make lighting
         prep_flags |= VSPREP_MAKELIGHTING;
         if(vertex_alpha)
            prep_flags |= VSPREP_LIGHT_MUL_ALPHA;
      }else
      if(fvf&D3DFVF_DIFFUSE){
         if(!vertex_alpha){
                        //plain copy of diffuse
            se.AddFragment(VSF_DIFFUSE_COPY);
         }else{
            se.AddFragment(VSF_DIFFUSE_MUL_ALPHA);
         }
      }else
      if((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZB1)
         se.AddFragment(VSF_COPY_ALPHA);
   }

   PrepareVertexShader(mesh->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

   DrawPrimitiveVisualPS(mesh, pc, rp);
}

//----------------------------
//----------------------------

I3D_visual *CreateDynamic(PI3D_driver drv){
   //I3D_visual_dynamic *v = new I3D_visual_dynamic(drv);
   //v->Release();
   return new I3D_visual_dynamic(drv);
}

//----------------------------
//----------------------------
