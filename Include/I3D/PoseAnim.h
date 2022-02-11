#ifndef __POSE_ANIM_H
#define __POSE_ANIM_H

#include <C_buffer.h>

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Structure of single key, into which we load data and keep it for editing.
//----------------------------

struct S_pose_key{
                              //fixed poses for model for one key.
   C_smart_ptr<I3D_animation_set> anim_set;
                              //key time - when pose should be fully reached.
                              //must be: prev_anim.time < time > next_anim.time
   int time;
                              //easy in/out smooth
   float easy_to, easy_from;

   float smooth;             //0 ... 1
                              //notify string. no notify if empty
   C_str notify;
   C_str notify_link;         //name of frame to notify.

   S_pose_key():
      time(0),
      easy_to(0.0f), easy_from(0.0f),
      smooth(1.0f)
   {}

   S_pose_key(const S_pose_key &pk){
      operator =(pk);
   }

   S_pose_key &operator =(const S_pose_key &pk){

      time = pk.time;
      easy_to = pk.easy_to;
      easy_from = pk.easy_from;
      smooth = pk.smooth;
      anim_set = pk.anim_set;
      notify = pk.notify;
      notify_link = pk.notify_link;
      return (*this);
   }
};

//----------------------------
// Structure for store animation raw data (keys and list of frames which are animmated).
// Used for loading editing and converting to KeyFrame animation.
//----------------------------

struct S_pose_anim{
                              //Set of poses
   C_buffer<S_pose_key> key_list;
   void *weight_map;          //hidden implementation

   S_vector edit_pos;
   S_quat edit_rot;

//----------------------------

   S_pose_anim();
   ~S_pose_anim();
   S_pose_anim(const S_pose_anim &pa);
   S_pose_anim &operator =(const S_pose_anim&);

//----------------------------

   dword GetEndTime() const{

      return key_list.size() ? key_list.back().time : 0;
   }

//----------------------------

   void GetAnimFrames(C_buffer<C_str> &names_list) const;

//----------------------------
// Set weight associated with given link name.
   void SetWeight(const C_str &link, float f);

//----------------------------
// Get weight associated with given link name.
   float GetWeight(const C_str &link) const;

//----------------------------
   void EnumWeights(void(*EnumProc)(const C_str &link, float val, dword context), dword context) const;

//----------------------------

   static int FindAnimId(CPI3D_animation_set as, const C_str &link){

      assert(as);
      for(int i = as->NumAnimations(); i--; ){
         //if(strcmp(as->GetAnimLink(i), link) == 0)
         if(as->GetAnimLink(i)==link)
            break;
      }
      return i;
   }

//----------------------------

   bool IsInFramesList(const C_str &link_name) const{

      if(!key_list.size())
         return false;
      return (FindAnimId(key_list[0].anim_set, link_name) != -1);

   }

//----------------------------
//contain id of key and pose for frame.
//id should be same as position in C_vector.
   struct S_key_inf{
      dword key_id;          //index into key_list
      PI3D_anim_pose ap;   //pose for frame
   };

//----------------------------

   bool ConvertToAnimSet(class I3D_animation_set *as, class I3D_driver *drv) const;

//----------------------------
private:

   bool LoadInternal(class C_chunk &ck, I3D_driver *drv);

public:

//----------------------------
// Load animations from given chunk.
   bool Load(class C_chunk &ck, I3D_driver *drv);

//----------------------------
// Load animation into animation set.
// It's static function (just for providing namespace).
   static bool LoadAnimSet(C_chunk &ck, PI3D_animation_set as, PI3D_driver drv, S_vector *edit_pos = NULL, S_quat *edit_rot = NULL){

      S_pose_anim pa;
      bool load_ok = pa.Load(ck, drv);
      if(!load_ok)
         return false;
      bool ok = pa.ConvertToAnimSet(as, drv);
      if(ok){
         if(edit_pos)
            *edit_pos = pa.edit_pos;
         if(edit_rot)
            *edit_rot = pa.edit_rot;
      }
      return ok;
   }
};

//----------------------------

#endif //__POSE_ANIM_H