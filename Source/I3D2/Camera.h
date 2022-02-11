#ifndef __CAMERA_H
#define __CAMERA_H

#include "frame.h"

//----------------------------

class I3D_camera: public I3D_frame{

#define CAMF_RESET_PROJ    1
#define CAMF_RESET_VIEW    2
#define CAMF_RESET_BIAS    4
#define CAMF_ORTHOGONAL    8

   mutable S_matrix m_proj;
   mutable S_projection_matrix m_proj_simple,
      m_proj_simple_biased;   //same as m_proj_simple, but used with z-bias rendering (to avoid z-fighting of coplanar polygons)
#ifdef GL
   mutable S_projection_matrix gl_m_proj_simple, gl_m_proj_simple_biased;
#endif
   mutable S_matrix m_view;
                              //perspective transform
   float fov_a, ncp, fcp;     //fov angle & clipping planes
                              //orthogonal transform
   float ortho_scale;

   mutable dword cam_flags;
   class I3D_sector *curr_sector;   //current sector this camera is in (ref-counted)

   ~I3D_camera();
public:
   I3D_camera(PI3D_driver);

//----------------------------
// Get computed matrices. Computation is done by UpdateCameraMatrices.
   inline const S_matrix &GetViewMatrix1() const{ assert(!(cam_flags&CAMF_RESET_VIEW)); return m_view; }
   inline const S_matrix &GetProjectionMatrix1() const{ assert(!(cam_flags&CAMF_RESET_PROJ)); return m_proj; }
   inline const S_projection_matrix &GetProjectionMatrixSimple() const{ assert(!(cam_flags&CAMF_RESET_PROJ)); return m_proj_simple; }
#ifdef GL
   inline const S_projection_matrix &GlGetProjectionMatrixSimple() const{ assert(!(cam_flags&CAMF_RESET_PROJ)); return gl_m_proj_simple; }
   inline const S_projection_matrix &GlGetProjectionMatrixBiasedSimple() const{ assert(!(cam_flags&CAMF_RESET_BIAS)); return gl_m_proj_simple_biased; }
#endif

//----------------------------
// Get biased projection matrix. This matrix is used for rendering Z-biased geometry.
// The matrix is the same as m_proj, except that near and far clipping planes are slightly moved to viewer.
   inline const S_projection_matrix &GetProjectionMatrixBiasedSimple() const{
      assert(!(cam_flags&CAMF_RESET_BIAS));
      return m_proj_simple_biased;
   }

   inline void GetRange1(float &n, float &f) const{
      n = ncp;
      f = fcp;
   }
   inline float GetNCP() const{ return ncp; }
   inline float GetFCP() const{ return fcp; }
   inline void ResetProjectionMatrix(){ cam_flags |= CAMF_RESET_PROJ | CAMF_RESET_BIAS; }
   inline void ResetViewMatrix() const{ cam_flags |= CAMF_RESET_VIEW; }
                              //sectors
   void SetCurrSector(PI3D_sector s);
   inline PI3D_sector GetCurrSector() const{ return curr_sector; }
   inline float GetFOV1() const{ return fov_a; }
   inline float GetOrthoScale1() const{ return ortho_scale; }
//----------------------------
// Update camera's matrices - m_proj and m_view.
   void UpdateCameraMatrices(float aspect_ratio) const;

//----------------------------
// Mirror camera - put behind mirror plane.
   void Mirror(const S_plane&);

   void Draw1(PI3D_scene, bool strong) const;


//----------------------------
   bool PrepareViewFrustum(S_view_frustum &vf, I3D_bsphere &vf_sphere, float inv_aspect) const;
public:
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(DebugDraw)(PI3D_scene) const;
   virtual void PropagateDirty(){
      I3D_frame::PropagateDirty();
      ResetViewMatrix();
   }
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
                              //setup:
   I3DMETHOD(SetFOV)(float);
   I3DMETHOD(SetRange)(float near1, float far1);
   I3DMETHOD(SetOrthoScale)(float scale);
   I3DMETHOD(SetOrthogonal)(bool);

   I3DMETHOD_(float,GetFOV)() const{ return fov_a; }
   I3DMETHOD_(void,GetRange)(float &near1, float &far1) const;
   I3DMETHOD_(float,GetOrthoScale)() const{ return ortho_scale; }
   I3DMETHOD_(bool,GetOrthogonal)() const{ return (cam_flags&CAMF_ORTHOGONAL); }

   I3DMETHOD_(const S_matrix&,GetViewMatrix)() const;
   I3DMETHOD_(const S_matrix&,GetProjectionMatrix)() const;
   I3DMETHOD_(I3D_sector*,GetSector)() const{ return curr_sector; }
};

//----------------------------

#ifdef _DEBUG
inline PI3D_camera I3DCAST_CAMERA(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_CAMERA ? NULL : static_cast<PI3D_camera>(f); }
inline CPI3D_camera I3DCAST_CCAMERA(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_CAMERA ? NULL : static_cast<CPI3D_camera>(f); }
#else
inline PI3D_camera I3DCAST_CAMERA(PI3D_frame f){ return static_cast<PI3D_camera>(f); }
inline CPI3D_camera I3DCAST_CCAMERA(CPI3D_frame f){ return static_cast<CPI3D_camera>(f); }
#endif

//----------------------------

#endif
