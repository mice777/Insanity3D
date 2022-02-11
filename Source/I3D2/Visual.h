#ifndef __VISUAL_H
#define __VISUAL_H

#include "frame.h"
#include "scene.h"

//----------------------------

class I3D_mesh_base;

//----------------------------
                              //destination vertex format may hold following items:
                              // - xyz, or xyz+rhw, zero or more betas
                              // - diffuse
                              // - specular
                              // - zero or more texture coordinates

class I3D_dest_vertex_buffer{
   PI3D_driver drv;
   mutable dword last_render_time;

   IDirect3DVertexBuffer9 *D3D_vertex_buffer;
   dword fvf_flags;
   dword max_vertices;
   byte size_of_vertex;
public:
   dword D3D_vertex_buffer_index;
   C_smart_ptr<IDirect3DVertexDeclaration9> vs_decl;

   I3D_dest_vertex_buffer(PI3D_driver d):
      drv(d),
      D3D_vertex_buffer(NULL), 
      D3D_vertex_buffer_index(0),
      max_vertices(0),
      size_of_vertex(0), fvf_flags(0)
   {}
   void CreateD3DVB(dword fvf_flags, dword max_vertices, bool force_xyz_format = false
#ifndef GL
      , bool allow_hw_trans = true
#endif
      );
   void DestroyD3DVB();

   inline dword NumVertices() const{ return max_vertices; }
   inline IDirect3DVertexBuffer9 *GetD3DVertexBuffer() const{
      last_render_time = drv->GetRenderTime();
      return D3D_vertex_buffer;
   }

   inline dword GetSizeOfVertex() const{ return size_of_vertex; }
   inline dword GetFVFlags() const{ return fvf_flags; }
   inline dword GetLastRenderTime() const{ return last_render_time; }

//----------------------------

   void *Lock(dword d3d_lock_flags = 0){

      if(!max_vertices)
         return NULL;
      assert(D3D_vertex_buffer);
      void *vp;
      HRESULT hr = D3D_vertex_buffer->Lock(D3D_vertex_buffer_index * size_of_vertex,
         max_vertices * size_of_vertex,
         &vp,
         d3d_lock_flags);
      CHECK_D3D_RESULT("Lock", hr);
      if(FAILED(hr))
         return NULL;
      return vp;
   }

//----------------------------

   bool Unlock(){
      if(!max_vertices)
         return false;
      assert(D3D_vertex_buffer);
      HRESULT hr = D3D_vertex_buffer->Unlock();
      CHECK_D3D_RESULT("Unlock", hr);
      return (SUCCEEDED(hr));
   }
};

//----------------------------

                              //flags used in call to I3D_visual::PrepareVertexShader
enum{
   VSPREP_MAKELIGHTING = 1,   //do lighting on vertices
   VSPREP_COPY_UV = 2,        //copy primary uv coordinates (set #0)
   VSPREP_WORLD_SPACE = 4,    //working in worldspace, rather than in objectspace
   VSPREP_TRANSFORM = 8,      //add simple transforming code
   VSPREP_FEED_MATRIX = 0x10, //feed transposed matrix
   VSPREP_NO_COPY_UV = 0x20,  //disable copying of primary uv under any circumstances (if not set, uv0 may be copied due to subsequent uvs written)
   VSPREP_NO_DETAIL_MAPS = 0x40, //disable detail maps (environment, detail, ...)
   
   VSPREP_LIGHT_MUL_ALPHA = 0x100,  //multiply resulting diffuse alpha by in-vertex alpha (works only if VSPREP_MAKELIGHTING also included)
   
   VSPREP_LIGHT_LM_DYNAMIC = 0x400, //do lighting using LM dynamic lights (works only if VSPREP_MAKELIGHTING also included)

   VSPREP_DEBUG = 0x10000,    //debugging
};

//----------------------------

class I3D_visual: public I3D_frame{

#define VISF_BOUNDS_VALID     0x01  //bounding volume is valid
#define VISF_DEST_UV0_VALID   0x02  //cached coords in dest VB are valid (no need to copy)
#define VISF_DEST_LIGHT_VALID 0x04  //cached diffuse in dest VB is valid (no need to compute)
//#define VISF_FOG_VALID        0x08  //fog info in vertices is valid (use in rendering)
#define VISF_DEST_VERTS_VALID 0x10  //cached dest processed vertices are valid
#define VISF_USE_ENVMAP       0x20  //use environment mapping
#define VISF_USE_DETMAP       0x40  //use detail mapping
#define VISF_USE_EMBMMAP      0x80  //use embm mapping
#define VISF_USE_BUMPMAP      0x100 //use bump mapping
//#define VISF_DIFFUSE_VALID    0x200 //diffuse written into oD0 (pixel-shader mode only)
#define VISF_USE_OVERRIDE_LIGHT 0x400 //use user defined light instead of standart lighting
#define VISF_DEST_PREPARED 0x800

                              //user-defined material ID
   dword material_id;
   friend class I3D_scene;
   friend class I3D_scene_imp;
   friend class I3D_frame;

protected:
   mutable dword vis_flags;
   mutable C_bound_volume hr_bound;

   dword visual_type;         //four-cc code

#ifndef GL
   I3D_dest_vertex_buffer vertex_buffer;
#endif

                              //last LOD used to transform vertices
                              // (help for non-direct transforms, to detect that dest VB is dirty when LOD changes)
   int last_auto_lod;

   float hide_dist;           //distance at which visual is no more rendered (for FOV=65 and scale=1) (0=no hiding)
   byte alpha;
   byte brightness[3];

   byte override_light[3]; //rgb part of light used instead of standard vertex lighting. 

   dword last_render_time;

   bool GenerateDetailMapUV(dword num_txt_stages, const S_vector2 &scale);

public:
   mutable C_bound_volume bound;

   I3D_visual(PI3D_driver);
   ~I3D_visual();
   I3D_visual& operator =(const I3D_visual&);

   inline dword GetVisualType1() const{ return visual_type; }

   inline void SetHideDistance(float f){ hide_dist = f; }
   inline void ResetLightInfo() const{ vis_flags &= ~VISF_DEST_LIGHT_VALID; }
   inline dword GetVisFlags() const{ return vis_flags; }

//----------------------------
// Select level of detail in a vector of LODS, where each LOD is specified
// by a quality ration (0.0f=worst, 1.0f=best).
// The selection is made by distance of this visual to camera.
// The returned value is LOD index to use,
//    -1 if base (non-LOD) mesh should be used,
   int GetAutoLOD(const S_preprocess_context&, I3D_mesh_base *mb) const;

//----------------------------
// Add data for mirror surface, into preprocess context.
   void AddMirrorData(S_preprocess_context &pc, CPI3D_material, int auto_lod);

//----------------------------
// Determine automatic LOD and fill primitives into render list.
// Doesn't transform vertices in any way.
   void AddPrimitives1(I3D_mesh_base *mb, S_preprocess_context &pc);

//----------------------------
// Render mesh using currently set shader.
#ifndef GL
   void DrawPrimitiveVisual(I3D_mesh_base *mb, const S_preprocess_context &pc, const S_render_primitive &rp);
#endif
   void DrawPrimitiveVisualPS(I3D_mesh_base *mb, const S_preprocess_context &pc, const S_render_primitive &rp);
//----------------------------
// Prepare lighting info for vertex shader.
   dword PrepareVSLighting(CPI3D_material mat, I3D_driver::S_vs_shader_entry_in &se_in, const S_render_primitive &rp, const S_matrix &m_trans,
      const S_matrix &m_inv_trans, dword flags, S_vectorw light_params[], C_buffer<E_VS_FRAGMENT> *save_light_fragments, C_buffer<S_vectorw> *save_light_params) const;

#ifdef GL
// Returns number of lights.
   dword GlPrepareVSLighting(CPI3D_material mat, const S_render_primitive &rp, const S_matrix &m_trans, const S_matrix &m_inv_trans, dword flags,
      S_vectorw light_params[], C_buffer<S_vectorw> *save_light_params) const;
#endif
//----------------------------
// Prepare VS for visual - add necessary fragments, create and setup shader,
// fill-in constants.
// If the call succeeds, the returned value is shader data. It is valid pointer
// until end of this frame.
   const I3D_driver::S_vs_shader_entry *PrepareVertexShader(CPI3D_material, dword num_txt_stages,
      I3D_driver::S_vs_shader_entry_in &se, const S_render_primitive &rp, E_RENDERVIEW_MODE rv_mode,
      dword flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX, const S_matrix *m_trans = NULL,
      C_buffer<E_VS_FRAGMENT> *save_light_fragments = NULL, C_buffer<S_vectorw> *save_light_params = NULL
#ifdef GL
      , C_buffer<S_vectorw> *gl_save_light_params = NULL
#endif
      ) const;

//----------------------------
// Render mesh without setting anything (for shadow rendering and debugging purposes).
   void RenderSolidMesh(PI3D_scene, bool debug = false, const I3D_cylinder* = NULL) const;

//----------------------------
// Set up additional mapping params right before rendering (envmap, detailmap, etc).
// This sets textures, stages, and disables stages beyond used stages.
// 'num_txt_stages' means number of stages used by visual core.
#ifndef GL
   void SetupSpecialMapping(CPI3D_material mat, const S_render_primitive *rp, dword num_txt_stages = 1) const;
#endif

//----------------------------
// // PS version, returning number of texture stages utilized.
   void SetupSpecialMappingPS(CPI3D_material mat,// const S_render_primitive *rp,
      I3D_driver::S_ps_shader_entry_in &se_ps, dword num_txt_stages = 1) const;

public:
   I3DMETHOD_(void,GetChecksum)(float &matrix_sum, float &vertc_sum, dword &num_v) const;
   I3DMETHOD(DebugDraw)(PI3D_scene) const{ return I3DERR_UNSUPPORTED; }
   I3DMETHOD(Duplicate)(CPI3D_frame);
   virtual void PropagateDirty(){
      ResetLightInfo();
      I3D_frame::PropagateDirty();
   }

public:
   I3DMETHOD_(dword,GetVisualType)() const{ return visual_type; }

   I3DMETHOD_(const I3D_bound_volume&,GetBoundVolume)() const{
      if(!(vis_flags&VISF_BOUNDS_VALID)) ((PI3D_visual)this)->ComputeBounds();
      return bound.bound_local; 
   }
   I3DMETHOD_(const I3D_bound_volume&,GetHRBoundVolume)() const{
      if(!(frm_flags&FRMFLAGS_HR_BOUND_VALID))
         hr_bound.MakeHierarchyBounds(((PI3D_visual)this));
      return hr_bound.bound_local;

   }
   I3DMETHOD_(dword,GetLastRenderTime)() const{ return last_render_time; }
   I3DMETHOD_(PI3D_mesh_base,GetMesh)(){ return NULL; }
   I3DMETHOD_(CPI3D_mesh_base,GetMesh)() const{ return NULL; }

   I3DMETHOD_(I3D_RESULT,SetMaterial)(PI3D_material){ return I3DERR_UNSUPPORTED; }
   I3DMETHOD_(PI3D_material,GetMaterial()){ return NULL; }
   I3DMETHOD_(CPI3D_material,GetMaterial()) const{ return NULL; }

   I3DMETHOD(SetProperty)(dword index, dword value){ return I3DERR_UNSUPPORTED; }
   I3DMETHOD_(dword,GetProperty)(dword index) const{ return 0xffcdcdcd; }
   I3DMETHOD_(dword,GetCollisionMaterial)() const{ return material_id; }
   I3DMETHOD_(void,SetCollisionMaterial)(dword id){ material_id = id; }

   I3DMETHOD_(void,SetVisualAlpha)(float f){
      int a = Max(0, Min(255, FloatToInt(f * 255.0f)));
      if(alpha!=a){
         alpha = (byte)a;
         ResetLightInfo();
      }
   }
   I3DMETHOD_(float,GetVisualAlpha)() const{
      return float(alpha) * R_255;
   }

   I3DMETHOD_(void,SetBrightness)(I3D_VISUAL_BRIGHTNESS i, float b){
      assert(dword(i)<3);
      int a = FloatToInt(b * 128.0f);
      brightness[i] = (byte)Max(0, Min(255, a));
      ResetLightInfo();
   }
   I3DMETHOD_(float,GetBrightness)(I3D_VISUAL_BRIGHTNESS i) const{
      assert(dword(i)<3);
      return float(brightness[i]) * R_128;
   }

//----------------------------

   I3DMETHOD_(void,SetOverrideLight)(const S_vector *lcolor){

      if(lcolor){
         vis_flags |= VISF_USE_OVERRIDE_LIGHT;
                              //encode rgb parts to byte (color range .0f, 2.0f)
         override_light[0] = (byte) Max(0, Min(255, FloatToInt(lcolor->x * 128.0f)));
         override_light[1] = (byte) Max(0, Min(255, FloatToInt(lcolor->y * 128.0f)));
         override_light[2] = (byte) Max(0, Min(255, FloatToInt(lcolor->z * 128.0f)));
      }else
         vis_flags &= ~VISF_USE_OVERRIDE_LIGHT;
   }

//----------------------------

   I3DMETHOD_(bool,GetOverrideLight)(S_vector &lcolor) const{

      if(vis_flags&VISF_USE_OVERRIDE_LIGHT){
         for(int i = 3; i--; )
            lcolor[i] = override_light[i] * R_128;
      }
      return (vis_flags&VISF_USE_OVERRIDE_LIGHT);
   }

   //I3DMETHOD_(void,SetBrightness)(float f){ brightness = f; }
   //I3DMETHOD_(float,GetBrightness)() const{ return brightness; }
//----------------------------
                              //non-user methods
   virtual bool ComputeBounds() = 0;
   virtual void AddPrimitives(S_preprocess_context&) = 0;
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&) = 0;
#else
   I3DMETHOD_(void,VisReserved10)(){}
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&) = 0;

//----------------------------
// Prepare destination VB for transforming. 'num_txt_stages' specifies # of stages used by visual core.
   virtual void PrepareDestVB(I3D_mesh_base*, dword num_txt_stages = 1);
   virtual bool AttachProcedural(class I3D_procedural_base*){ return false; }
   virtual bool AddRegion(const I3D_bbox&, const S_matrix&, int index = -1){ return false; }
   virtual bool SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const;
   virtual bool LoadCachedInfo(C_cache *ck, class C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats);
   virtual void SetMeshInternal(I3D_mesh_base*){}
   virtual bool FinalizeLoad(C_loader &ld, bool ck_load_ok, const void* = NULL){ return true; }
   I3DMETHOD_(void,VisReserved11)(){}
   I3DMETHOD_(void,VisReserved12)(){}
   I3DMETHOD_(void,VisReserved13)(){}
   I3DMETHOD_(void,VisReserved14)(){}
   I3DMETHOD_(void,VisReserved15)(){}

   inline bool NeedPrepareDestVb() const{
      return
#ifndef GL
         (!vertex_buffer.GetD3DVertexBuffer());
#else
         !(vis_flags&VISF_DEST_PREPARED);
#endif
   }
};

//----------------------------

#ifdef _DEBUG
inline PI3D_visual I3DCAST_VISUAL(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_VISUAL ? NULL : static_cast<PI3D_visual>(f); }
inline CPI3D_visual I3DCAST_CVISUAL(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_VISUAL ? NULL : static_cast<CPI3D_visual>(f); }
#else
inline PI3D_visual I3DCAST_VISUAL(PI3D_frame f){ return static_cast<PI3D_visual>(f); }
inline CPI3D_visual I3DCAST_CVISUAL(CPI3D_frame f){ return static_cast<CPI3D_visual>(f); }
#endif

//----------------------------

#endif
