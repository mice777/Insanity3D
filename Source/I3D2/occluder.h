#ifndef __OCCLUDER_H
#define __OCCLUDER_H

#include "frame.h"
#include "common.h"


//----------------------------
                              //distance of tolerance, in meters
                              // each clippping plane has this additional space
                              // as safe region, to avoid occluding objects 
                              // which are very close to occluder
const float SAFE_DIST = .005f;

//----------------------------

class I3D_occluder: public I3D_frame{

   I3D_OCCLUDERTYPE occ_type;

#define OCCF_RESET_BBOX       1
#define OCCF_DEBUG_OCCLUDING  2  //debug flag set when it's active occluder, for Draw-ing in different color

   mutable dword occ_flags;

   mutable C_bound_volume bound;

                              //run-time:

   S_view_frustum_base vf;    //mesh occluder: clipping volume made from countours and faced planes
   //S_expanded_view_frustum exp_vf;
   C_vector<S_vector> v_trans;
   C_vector<S_vector> f_normals;//faces' normals, scaled by area of face

                              //conic view frustum - defined by cut cone volume
   struct S_cone_frustum{
      S_plane ncp;               //near clipping plane
      S_vector top;              //top point of cone
      S_normal cone_axis;        //cone's axis
      float angle, tan_angle;
   };
   S_cone_frustum cf;        //sphere occluder: clipping volume made from near plane and contours

   PI3D_sector work_sector;  //sector we're working in, set each time 'ComputeOccludingFrustum' is called

//----------------------------
// Build bounding volume of occluder, based on current type.
   void BuildBounds();


                              //convex hull
   C_vector<S_vector> vertices;
   C_vector<I3D_face> faces;

   I3D_occluder(PI3D_driver);
   ~I3D_occluder();

   friend I3D_driver;        //creation
   friend class I3D_scene;

//----------------------------
// Transform 'vertices' to 'v_trans' using matrix.
   void TransformVertices();

//----------------------------
// Compute frustum for mesh-based occluder, put result into 'vf'.
   bool ComputeMeshOccludingFrustum(PI3D_scene);

//----------------------------
// Compute frustum for sphere-based occluder, put result into 'cf'.
   bool ComputeSphereOccludingFrustum(PI3D_scene);

public:
   I3D_occluder &operator =(const I3D_occluder&);
   void Draw1(PI3D_scene, bool strong) const;

   bool ComputeOccludingFrustum(PI3D_scene);
   inline const C_bound_volume &GetBound() const{ return bound; }

   inline dword GetOccFlags() const{ return occ_flags; }
   inline I3D_OCCLUDERTYPE GetOccluderType1() const{ return occ_type; }
   inline const S_view_frustum_base &GetViewFrustum() const{
      assert(occ_type==I3DOCCLUDER_MESH);
      return vf;
   }

//----------------------------
// Collision with ray - for debugging purposes.
// Initially, 'best_dist' is used to specify length of collision ray.
// If collision is detected, the returned value is true and 'best_dist' is updated with new hit distance.
   bool TestCollision(const S_vector &from, const S_vector &norm_dir, float &best_dist);

//----------------------------
// Check if bound volume (in world coordinates) is fully inside of occluder's frustum.
// If inside, return clipping flag in 'clip' parameter;
   bool IsOccluding(const I3D_bsphere&, bool &clip) const;
   bool IsOccluding(const C_bound_volume &bvol, const S_matrix &mat, bool &clip) const;

   inline void SetWorkSector(PI3D_sector ws1){ work_sector = ws1; }
   inline PI3D_sector GetWorkSector() const{ return work_sector; }

                              //overriden virtual functions
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(DebugDraw)(PI3D_scene scene) const{
      Draw1(scene, true);
      return I3D_OK;
   }

   virtual void PropagateDirty(){
      I3D_frame::PropagateDirty();
      occ_flags |= OCCF_RESET_BBOX;
      frm_flags &= ~(FRMFLAGS_BSPHERE_TRANS_VALID);
   }
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(Build)(const S_vector *verts, dword num_verts);

   I3DMETHOD_(dword,NumVertices)() const{ return vertices.size(); }
   I3DMETHOD_(const S_vector*,LockVertices)(){ return &vertices.front(); }
   I3DMETHOD_(void,UnlockVertices)(){}

   I3DMETHOD(SetOccluderType)(I3D_OCCLUDERTYPE);
   I3DMETHOD_(I3D_OCCLUDERTYPE,GetOccluderType)() const;

   I3DMETHOD_(dword,NumFaces)() const{ return faces.size(); }
   I3DMETHOD_(const I3D_face*,GetFaces)() const{ return &faces.front(); }
};

//----------------------------

#ifdef _DEBUG
inline PI3D_occluder I3DCAST_OCCLUDER(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_OCCLUDER ? NULL : static_cast<PI3D_occluder>(f); }
inline CPI3D_occluder I3DCAST_COCCLUDER(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_OCCLUDER ? NULL : static_cast<CPI3D_occluder>(f); }
#else
inline PI3D_occluder I3DCAST_OCCLUDER(PI3D_frame f){ return static_cast<PI3D_occluder>(f); }
inline CPI3D_occluder I3DCAST_COCCLUDER(CPI3D_frame f){ return static_cast<CPI3D_occluder>(f); }
#endif

//----------------------------
#endif
