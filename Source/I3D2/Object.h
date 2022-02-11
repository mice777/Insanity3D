#ifndef __OBJECT_H
#define __OBJECT_H

#include "visual.h"

//----------------------------

class I3D_object: public I3D_visual{

   virtual void SetMeshInternal(PI3D_mesh_base mb);

                              //cached light info (used in direct transform mode)
   C_buffer<E_VS_FRAGMENT> save_light_fragments;
   C_buffer<S_vectorw> save_light_params;
#ifdef GL
   C_buffer<S_vectorw> gl_save_light_params;
#endif
protected:

   class I3D_mesh_base *mesh;
   ~I3D_object();

public:
   inline PI3D_mesh_base GetMesh1(){ return mesh; }

   I3DMETHOD_(PI3D_mesh_base,GetMesh)(){ return mesh; }
   I3DMETHOD_(CPI3D_mesh_base,GetMesh)() const{ return mesh; }
public:
   I3DMETHOD(Duplicate)(CPI3D_frame);

   virtual bool ComputeBounds();
   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
   virtual void PrepareDestVB(I3D_mesh_base*, dword = 1);
   virtual bool LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats);
public:
   I3D_object(PI3D_driver);

   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(void,SetMesh)(PI3D_mesh_base mb);

   I3DMETHOD_(void,SetUpdate)();
};

//----------------------------

#endif
