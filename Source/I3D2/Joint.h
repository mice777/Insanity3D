#ifndef __JOINT_H
#define __JOINT_H

#include "frame.h"

#define USE_CAPCYL_JNT_VOLUMES

//----------------------------

class I3D_joint: public I3D_frame{

#define JOINTF_REGION_INDX_MASK  0xff  //mask of bits in flags specifying region index
#define JOINTF_BBOX_VALID        0x100 //bounding-box is valid
#define JOINTF_VBOX_VALID        0x200 //volume-box is valid
#define JOINTF_FIXED_POSITION    0x400 //position cannot be adjusted
#define JOINTF_BREAK             0x800 //this joint breaks SM to segments

   dword joint_flags;

   S_matrix bbox_matrix;      //bounding box' matrix relative to this joint (i.e. local matrix)
   I3D_bbox bbox;             //bounding-box used for SM vertex assignment

   float draw_size;
   float draw_len;
   S_quat draw_rot;

#ifdef USE_CAPCYL_JNT_VOLUMES
                              //volume capped cylinder
   S_vector vol_pos, vol_dir;
   float vol_radius, vol_half_len;
#else
   I3D_bbox volume_box;      //volume-box - used for collision detection
   S_matrix volume_box_matrix;
#endif

                              //distance at which joint is no more used (for FOV=65 and scale=1) (0=no hiding)
                              // valid only for joints with JOINTF_BREAK set
   float hide_dist;

   void PropagateDirty();

   ~I3D_joint();
public:
   I3D_joint(PI3D_driver drv);

   bool AddRegion(const I3D_bbox&, const S_matrix&, int index = -1);

   bool SetVolumeBox(const I3D_bbox &bb, const S_matrix &m);

   inline bool IsBBoxValid() const{ return (joint_flags&JOINTF_BBOX_VALID); }
   inline const I3D_bbox &GetBBox1() const{ return bbox; }
   inline const S_matrix &GetBBoxMatrix1() const{ return bbox_matrix; }
   inline int GetRegionIndex() const{
      byte b = byte(joint_flags&JOINTF_REGION_INDX_MASK);
      return (b==0xff) ? -1 : b;
   }

//----------------------------
   inline bool IsVolumeBoxValid() const{ return (joint_flags&JOINTF_VBOX_VALID); }

#ifdef USE_CAPCYL_JNT_VOLUMES
   inline const S_vector &GetVolumePos() const{ return vol_pos; }
   inline const S_vector &GetVolumeDir() const{ return vol_dir; }
   inline float GetVolumeRadius() const{ return vol_radius; }
   inline float GetVolumeHalfLen() const{ return vol_half_len; }
#else
   inline const I3D_bbox &GetVolumeBox1() const{ return volume_box; }
   inline const S_matrix &GetVolumeBoxMatrix1() const{ return volume_box_matrix; }
#endif

//----------------------------
   inline void SetDrawSize(float sz){ draw_size = sz; }
   inline void SetDrawLength(float sz){ draw_len = sz; }
   inline void SetDrawRot(const S_quat &r){ draw_rot = r; }

   inline dword GetJointFlags() const{ return joint_flags; }
   inline void SetHideDistance(float f){ hide_dist = f; }
   inline float GetHideDistance() const{ return hide_dist; }

   inline float GetIconSize() const{
      //return !IsBBoxValid() ? .5f : ((bbox.max-bbox.min).Magnitude() * bbox_matrix.GetScale().Sum() * .4f);
      return draw_size;
   }

   void LockPosition(bool b){
      joint_flags &= ~JOINTF_FIXED_POSITION;
      if(b) joint_flags |= JOINTF_FIXED_POSITION;
   }

   void SetBreak(bool b){
      joint_flags &= ~JOINTF_BREAK;
      if(b) joint_flags |= JOINTF_BREAK;
   }

   void Draw1(PI3D_scene, bool strong) const;

   I3DMETHOD(DebugDraw)(PI3D_scene scene) const{
      Draw1(scene, true);
      return I3D_OK;
   }
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(SetPos)(const S_vector &v);
   /*
   I3DMETHOD(SetScale)(float);            
   I3DMETHOD(SetRot)(const S_quat&);
   I3DMETHOD(SetDir)(const S_vector&, float roll = 0.0f);
   I3DMETHOD(SetDir1)(const S_vector &dir, const S_vector &up);
   */
   //virtual void SetMatrixDirty() const;

   //I3DMETHOD_(void,_Update)(){ UpdateMatrices(); }
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(const I3D_bbox*,GetBBox)() const{ return (joint_flags&JOINTF_BBOX_VALID) ? &bbox : NULL; }
   I3DMETHOD_(const S_matrix*,GetBBoxMatrix)() const{ return (joint_flags&JOINTF_BBOX_VALID) ? &bbox_matrix : NULL; }

#ifdef USE_CAPCYL_JNT_VOLUMES
   //I3DMETHOD_(const I3D_bbox*,GetVolumeBox)() const{ return NULL; }
   //I3DMETHOD_(const S_matrix*,GetVolumeBoxMatrix)() const{ return NULL; }
   I3DMETHOD_(I3D_RESULT,GetVolume)(S_vector &pos, S_vector &dir_normal, float &radius, float &half_len) const{
      if(!(joint_flags&JOINTF_VBOX_VALID))
         return I3DERR_NOTINITIALIZED;
      pos = vol_pos;
      dir_normal = vol_dir;
      radius = vol_radius;
      half_len = vol_half_len;
      return I3D_OK;
   }
#else
   I3DMETHOD_(const I3D_bbox*,GetVolumeBox)() const{ return (joint_flags&JOINTF_VBOX_VALID) ? &volume_box : NULL; }
   I3DMETHOD_(const S_matrix*,GetVolumeBoxMatrix)() const{ return (joint_flags&JOINTF_VBOX_VALID) ? &volume_box_matrix : NULL; }
#endif
};

//----------------------------

#ifdef _DEBUG
inline PI3D_joint I3DCAST_JOINT(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_JOINT ? NULL : static_cast<PI3D_joint>(f); }
inline CPI3D_joint I3DCAST_CJOINT(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_JOINT ? NULL : static_cast<CPI3D_joint>(f); }
#else
inline PI3D_joint I3DCAST_JOINT(PI3D_frame f){ return static_cast<PI3D_joint>(f); }
inline CPI3D_joint I3DCAST_CJOINT(CPI3D_frame f){ return static_cast<CPI3D_joint>(f); }
#endif

//----------------------------

#endif