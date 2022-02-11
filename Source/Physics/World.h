#ifndef __WORLD_H
#define __WORLD_H

#include "common.h"

//----------------------------

class IPH_world: public C_unknown{
protected:
   C_link_list<IPH_body> bodies;
   C_link_list<IPH_joint> joints;
   dword num_bodies;
   dword num_joints;
   friend IPH_body;
   friend IPH_joint;

public:
   IPH_world():
      num_bodies(0),
      num_joints(0)
   {
   }

//----------------------------
   virtual void SetGravity(const S_vector &v) = 0;
   virtual const S_vector &GetGravity() const = 0;

//----------------------------
   virtual void SetERP(float) = 0;
   virtual float GetERP() const = 0;

//----------------------------
   virtual void SetCFM(float) = 0;
   virtual float GetCFM() const = 0;

//----------------------------
// Setup collision material characteristics.
   virtual void SetMaterials(const IPH_surface_param *mats, dword num_mats) = 0;

//----------------------------
   virtual void Tick(int time, PI3D_scene scn, IPH_ContactQuery *c_query = NULL,
      IPH_ContactReport *c_report = NULL, void *context = NULL) = 0;

//----------------------------
   virtual PIPH_body CreateBody() = 0;
   virtual PIPH_joint CreateJoint(IPH_JOINT_TYPE) = 0;

//----------------------------
// Find body associated with given volume.
   virtual PIPH_body GetVolumeBody(CPI3D_volume) const = 0;

//----------------------------
   virtual void SetErrorHandler(IPH_MessageFunction *func) = 0;
   virtual void SetDebugLineFunc(IPH_DebugLine *func) = 0;
   virtual void SetDebugPointFunc(IPH_DebugPoint *func) = 0;
   virtual void SetDebugTextFunc(IPH_DebugText *func) = 0;

//----------------------------
// Check if specified joint type may have only one body set.
   virtual bool JointCanAttachToOneBody(IPH_JOINT_TYPE jt) const = 0;

//----------------------------
   virtual dword NumBodies() const = 0;
   virtual dword NumJoints() const = 0;
   virtual dword NumContacts() const = 0;

//----------------------------
   virtual void AddVolume(PI3D_volume vol, PIPH_body body) = 0;
   virtual void RemoveVolume(PI3D_volume vol) = 0;
};

//----------------------------
#endif
