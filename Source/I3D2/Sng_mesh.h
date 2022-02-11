#ifndef __SNG_MESH_H
#define __SNG_MESH_H

#include "visual.h"

#define USE_CAPCYL_JNT_VOLUMES

//----------------------------

class I3D_object_singlemesh: public I3D_visual{
#define SMESH_FLAGS_VBOX_VALID    1       //volume-box is valid
protected:
   dword smesh_flags;

#ifdef USE_CAPCYL_JNT_VOLUMES
                              //volume capped cylinder
   S_vector vol_pos, vol_dir;
   float vol_radius, vol_half_len;
#else
   S_matrix volume_box_matrix;
   I3D_bbox volume_box;      //volume-box - used for collision detection
#endif

public:
   I3D_object_singlemesh(PI3D_driver d):
      I3D_visual(d),
      smesh_flags(0),
      vol_half_len(0),
      vol_radius(0)
   {
   }

   void DebugDrawVolBox(PI3D_scene) const;

   inline bool IsVolumeBoxValid() const{ return (smesh_flags&SMESH_FLAGS_VBOX_VALID); }
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
#ifdef USE_CAPCYL_JNT_VOLUMES
   //I3DMETHOD_(const I3D_bbox*,GetVolumeBox)() const{ return NULL; }
   //I3DMETHOD_(const S_matrix*,GetVolumeBoxMatrix)() const{ return NULL; }
   I3DMETHOD_(I3D_RESULT,GetVolume)(S_vector &pos, S_vector &dir_normal, float &radius, float &half_len) const{
      if(!(smesh_flags&SMESH_FLAGS_VBOX_VALID))
         return I3DERR_NOTINITIALIZED;
      pos = vol_pos;
      dir_normal = vol_dir;
      radius = vol_radius;
      half_len = vol_half_len;
      return I3D_OK;
   }
#else
   I3DMETHOD_(const I3D_bbox*,GetVolumeBox)() const{ return (smesh_flags&SMESH_FLAGS_VBOX_VALID) ? &volume_box : NULL; }
   I3DMETHOD_(const S_matrix*,GetVolumeBoxMatrix)() const{ return (smesh_flags&SMESH_FLAGS_VBOX_VALID) ? &volume_box_matrix : NULL; }
#endif
};

//----------------------------

#endif//__SNG_MESH_H