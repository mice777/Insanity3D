#ifndef __VOLUME_H_
#define __VOLUME_H_

#include "frame.h"

//----------------------------
//----------------------------

class I3D_volume: public I3D_frame{

#define VOLF_RESET         1  //internal cache reset
#define VOLF_DEBUG_TRIGGER 2  //debugging - set when collided, to be painted in special color
                              

   mutable dword vol_flags;

   I3D_VOLUMETYPE volume_type;
   PI3D_frame owner;

   S_vector non_uniform_scale;   //valid for box and rectangle types
   mutable S_matrix g_matrix;    //resulting world geometry matrix = matrix * non_uniform_scale

                              //user-defined material ID
   dword material_id;

                              //user-defined category/collide bits (compared during test to accept/reject collision
                              // higher 1 bits are 'collide', lower 16 are 'category'
   dword collide_category_bits;

//----------------------------
                              //pre-computed cache:

                              //bounding sphere in world coordinates - valid for all volume types
   mutable I3D_bsphere w_sphere;
   mutable float sphere_pos_dot; //w_sphere.pos.Square()
   mutable I3D_bbox bbox;     //for sphere it is AABB, for others it is OBB


                              //cylinder & capcyl:
   mutable float world_radius, world_half_length;


                              //box & rect volumes:
   mutable S_vector normal[3], world_side[3];
   mutable float world_side_size[3]; //world_side_size - for edge calculation
   mutable float world_side_dot[3]; //world_side.Dot(world_side)

                              //d_min and d_max are box plane distances to
                              //coplanar plane through (0,0,0)
   mutable float d_max[3], d_min[3];

//----------------------------
                              //dynamic collisions help (accessed by I3D_scene):
public:
   mutable int curr_octtree_level;
   struct S_octtree_cell: public C_link_list_element<S_octtree_cell>{
      PI3D_volume vol;
      S_vector_i pos;         //cell coords (in its level)

      S_octtree_cell(PI3D_volume v = NULL):
         vol(v)
      {
      }
   };
   mutable C_buffer<C_buffer<S_octtree_cell> > dyn_cells; //dynamic oct-tree level-rings nodes

                              //linked list used for keeping list of 'dirty' volumes, needing update
   mutable struct S_dirty_node: public C_link_list_element<S_dirty_node>{
      PI3D_volume this_vol;
   } dirty_node;
                              //linked list used during tests, marking volumes which has 'test_col_tag' valid
   mutable struct S_tag_node: public C_link_list_element<S_tag_node>{
      PI3D_volume this_vol;
      dword test_col_tag;  //accessed by I3D_scene::C_dyn_vol_tree_tester
   } tag_node;
private:
//----------------------------

   ~I3D_volume();

   friend class I3D_scene;
   friend class I3D_scene_imp;
   friend struct S_bsp_triface;      //access to geomatrix
   friend class C_bsp_tree;         //read-only access

public:
                              // *** compute contacts ***
//----------------------------
// SPHERE : SPHERE
   void ContactSphereSphere(const struct I3D_contact_data &cd) const;

//----------------------------
// CYLINDER : CYLINDER
   void ContactCylinderCylinder(const I3D_contact_data &cd) const;

//----------------------------
// CYLINDER : SPHERE
   void ContactCylinderSphere(const I3D_contact_data &cd, CPI3D_volume vol, bool reverse) const;

//----------------------------
// BOX : SHERE (this is BOX, 'vol' is SPHERE)
//    (this func is also used for RECT : SPHERE).
   void ContactBoxSphere(const I3D_contact_data &cd, CPI3D_volume vol, bool reverse) const;

//----------------------------
// Check if sphere statically collides with this volume.
   bool IsSphereStaticCollision(const S_vector &pos, float radius) const;

//----------------------------
// BOX : BOX
   void ContactBoxBox(const I3D_contact_data &cd) const;

//----------------------------
// BOX : CYLINDER (this is BOX, 'vol' is CYLINDER)
   void ContactBoxCylinder(const I3D_contact_data &cd, CPI3D_volume vol, bool reverse) const;

//----------------------------
// RECT : SPHERE (this is RECT, 'vol' is SPHERE)
   void ContactRectSphere(const I3D_contact_data &cd) const;

//----------------------------
// Get geometry matrix, build if not valid.
   const S_matrix &GetGeoMatrix() const;

public:
   I3D_volume(CPI3D_scene);
   CPI3D_scene scene;

   I3D_volume &operator =(const I3D_volume&);

//----------------------------
// Reset dynamic volume - mark dirty, update dynamic collision tree next time when collision is tested.
   void ResetDynamicVolume() const;

   bool Prepare() const;
   void Draw1(PI3D_scene, bool strong, bool force_bsp = false, const dword *force_color = NULL) const;

   inline const I3D_bsphere &GetBoundSphere() const{ return w_sphere; }
   inline const I3D_bbox &GetBoundBox() const{ return bbox; }
   inline const S_vector *GetNormals() const{ return normal; }
   inline const float *GetWorldSize() const{ return world_side_size; }
   inline dword GetCollideCategoryBits() const{ return collide_category_bits; }

//----------------------------
// Compute volume's OBB.
   //void ComputeBoundBox(PI3D_bbox) const;
   bool ClipLineByBox(const struct S_trace_help &th, float &d_in) const;

                              //line - sphere
   void CheckCol_L_S_dyn(I3D_collision_data &cd);
                              //line - capcyl
   void CheckCol_L_CC_dyn(I3D_collision_data &cd);
                              //line - cyl
   void CheckCol_L_C_dyn(I3D_collision_data &cd);
                              //line - box
   bool CheckCol_L_B_dyn(I3D_collision_data &cd, bool passive);
                              //line - rectangle
   void CheckCol_L_R_dyn(I3D_collision_data &cd);

                              //sphere - sphere
   void CheckCol_S_S_dyn(const S_trace_help &th) const;
                              //sphere - capcyl
   void CheckCol_S_CC_dyn(const S_trace_help &th) const;
                              //sphere - box
   bool CheckCol_S_B_dyn(const S_trace_help &th, bool passive) const;
                              //sphere - rectangle
   void CheckCol_S_R_dyn(const S_trace_help &th) const;

   inline I3D_VOLUMETYPE GetVolumeType1() const{ return volume_type; }
   inline PI3D_frame GetOwner1() const{ return owner; }
   inline dword GetVolFlags() const{ return vol_flags; }

//----------------------------
// Returns true if the type is sphere or cylinder - like.
   inline bool IsSphericalType() const{
      switch(volume_type){
      case I3DVOLUME_SPHERE:
      case I3DVOLUME_CYLINDER:
      case I3DVOLUME_CAPCYL:
         return true;
      }
      return false;
   }

   void PropagateDirty(){
      I3D_frame::PropagateDirty();
      ResetDynamicVolume();
   }


                              //overriden virtual functions
   I3DMETHOD_(void,SetOn)(bool);
   I3DMETHOD_(void,GetChecksum)(float &matrix_sum, float &vertc_sum, dword &num_v) const;
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(DebugDraw)(PI3D_scene scene) const{
      Prepare();
      Draw1(scene, true);
      return I3D_OK;
   }
   I3DMETHOD_(dword,SetFlags)(dword new_flags, dword flags_mask);

   /*
   virtual void SetMatrixDirty() const{
      I3D_frame::SetMatrixDirty();
      Reset();
   }
   */
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(SetNUScale)(const S_vector &scl);
   I3DMETHOD_(const S_vector&,GetNUScale)() const{ return non_uniform_scale; }

   I3DMETHOD(SetVolumeType)(I3D_VOLUMETYPE t);

   I3DMETHOD_(void,SetOwner)(PI3D_frame o){ owner = o; }
   I3DMETHOD_(void,SetCollisionMaterial)(dword id){ material_id = id; }

   I3DMETHOD_(I3D_VOLUMETYPE,GetVolumeType)() const{ return volume_type; }
   I3DMETHOD_(PI3D_frame,GetOwner)() const{ return owner; }
   I3DMETHOD_(dword,GetCollisionMaterial)() const{ return material_id; }

   I3DMETHOD_(void,SetCategoryBits)(dword d){
      collide_category_bits &= 0xffff0000;
      collide_category_bits |= d&0xffff;
   }
   I3DMETHOD_(dword,GetCategoryBits)() const{ return collide_category_bits&0xffff; }
   I3DMETHOD_(void,SetCollideBits)(dword d){
      collide_category_bits &= 0x0000ffff;
      collide_category_bits |= d<<16;
   }
   I3DMETHOD_(dword,GetCollideBits)() const{ return collide_category_bits>>16; }
};

//----------------------------

#ifdef _DEBUG
inline PI3D_volume I3DCAST_VOLUME(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_VOLUME ? NULL : static_cast<PI3D_volume>(f); }
inline CPI3D_volume I3DCAST_CVOLUME(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_VOLUME ? NULL : static_cast<CPI3D_volume>(f); }
#else
inline PI3D_volume I3DCAST_VOLUME(PI3D_frame f){ return static_cast<PI3D_volume>(f); }
inline CPI3D_volume I3DCAST_CVOLUME(CPI3D_frame f){ return static_cast<CPI3D_volume>(f); }
#endif

//----------------------------

#endif
