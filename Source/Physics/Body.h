#ifndef __BODY_H
#define __BODY_H

#include "common.h"
#include "mass.h"

//----------------------------

class IPH_body: public C_link_list_element<IPH_body>{
   class IPH_world *world;

   REF_COUTNER;

                              //static data (set once, not changing):
   C_smart_ptr<I3D_frame> frame;
   S_vector mass_center;      //in global scale, rotated relative to body's rotation
   C_buffer<C_smart_ptr<I3D_volume> > volumes;
   float col_check_radius;
   dword num_parents;         //number of parents which frame has
   dword user_data;
   C_mass mass;             //mass parameters about POR
   S_matrix inv_i_tensor;     //inverse of mass.i_tensor
   float density;             //density in kg/m3


                              //dynamic values:
   S_quat rot;                //rotation
   S_matrix matrix;           //rotation + translation
   S_vector lvel, avel;       //linear and angular velocity of POR


                              //run-time & helpers:
   int tag;                   //used by dynamics algorithms
                              //list of attached joints
   struct dxJointNode *firstjoint;
                              //body flags
   enum{
      //BF_FiniteRotation = 1,    // use finite rotations
      //BF_FiniteRotationAxis = 2,   // use finite rotations only along axis
      BF_Disabled = 4,        // body is disabled
      BF_NoGravity = 8        // body is not influenced by gravity
   };
   dword flags;
   S_vector f_acc, t_acc;     //force and torque accumulators - accumulating force, cleared during Tick
   float r_mass;              //1 / mass.mass
   

   //S_vector finite_rot_axis;  //finite rotation axis, unit length or 0=none

   friend IPH_joint;
   friend class IPH_world_imp;

//----------------------------
// Apply world position and rotation onto frame (may need inverse trasnform).
   void SetFramePosRot(const S_vector &pos, const S_quat &rot){

                              //if frame's linked, account for parent transformation
      PI3D_frame prnt = frame->GetParent();
      if(prnt && prnt->GetType()!=FRAME_SECTOR){
         const S_matrix &m_inv = prnt->GetInvMatrix();
         frame->SetPos(pos * m_inv);
         frame->SetRot(rot * m_inv);
      }else{
         frame->SetPos(pos);
         frame->SetRot(rot);
      }
   }

//----------------------------
public:

   inline const S_vector &GetMassCenter1() const{ return mass_center; }
   inline const S_matrix &GetMatrix() const{ return matrix; }

   inline S_matrix GetInvMatrix() const{
                              //since our matrix is always unit-scaled, just return transposed matrix
      S_matrix m = matrix;
      m.Transpose();
      return m;
   }
   inline const S_vector &GetPos() const{ return matrix(3); }
   inline const S_quat &GetRot() const{ return rot; }
   inline const S_vector &GetLVel() const{ return lvel; }
   inline const S_vector &GetAVel() const{ return avel; }

//----------------------------

   inline void AddForce(const S_vector &f){ f_acc += f; }
   inline void AddTorque(const S_vector &t){ t_acc += t; }

//----------------------------

   virtual void SetPosRot(const S_vector &pos1, const S_quat &rot1){

      rot = rot1;
      rot.Normalize();
      matrix.SetRot(rot);
      matrix(3) = pos1 + mass_center % rot1;
      if(frame)
         SetFramePosRot(pos1, rot1);
   }

//----------------------------

   virtual void GetPosRot(S_vector &pos1, S_quat &rot1) const{
      rot1 = rot;
      pos1 = GetPos();
      pos1 -= mass_center % rot1;
   }

//----------------------------

   void SetMass(const C_mass &m);

//----------------------------

   virtual void AddForceAtPos(const S_vector &f_pos, const S_vector &force){

      f_acc += force;
      S_vector q = f_pos - GetPos();
      t_acc += q.Cross(force);
   }

//----------------------------

   virtual S_vector GetVelAtPos(const S_vector &pos) const{

      S_vector p = pos - GetPos();
      return lvel + avel.Cross(p);
   }

//----------------------------
   virtual bool IsConnected(CPIPH_body body) const{
      return IsConnectedExcluding(body, IPHJOINTTYPE_CONTACT);
   }

//----------------------------

   virtual bool SetFrame(PI3D_frame frm, dword flags = 0, float density = 1000.0f);

   virtual PI3D_frame GetFrame(){ return frame; }
   virtual CPI3D_frame GetFrame() const{ return frame; }

//----------------------------

   virtual void Enable(bool b);

   virtual bool IsEnabled() const{
      return !(flags&BF_Disabled);
   }

//----------------------------
   virtual void SetUserData(dword data){ user_data = data; }
   virtual dword GetUserData() const{ return user_data; }

//----------------------------
   virtual void SetLinearVelocity(const S_vector &v){ lvel = v; }
   virtual const S_vector &GetLinearVelocity() const{ return lvel; }

//----------------------------
   virtual void SetAngularVelocity(const S_vector &v){ avel = v; }
   virtual const S_vector &GetAngularVelocity() const{ return avel; }

//----------------------------
   virtual float GetDensity() const{ return density; }
   virtual float GetWeight() const{ return mass.GetWeight() * DENSITY_MULTIPLIER; }

//----------------------------
   virtual const C_buffer<C_smart_ptr<I3D_volume> > &GetVolumes() const{ return volumes; }

//----------------------------
   virtual const S_vector &GetMassCenter() const{ return mass_center; }

public:
   IPH_body(IPH_world *w);
   ~IPH_body();

//----------------------------

   //const C_buffer<C_smart_ptr<I3D_volume> > &GetVolumes() const{ return volumes; }

//----------------------------
// Apply body's position and rotation onto I3D_frame.
   void UpdateFrameTransform(){

      if(frame){
                              //get body's pos/rot
         S_vector pos;
         S_quat rot;
         GetPosRot(pos, rot);
                              //apply this world pos/rot onto frame
         SetFramePosRot(pos, rot);
      }
   }

//----------------------------

   inline float GetColCheckRadius() const{ return col_check_radius; }
   inline dword GetNumParents() const{ return num_parents; }

//----------------------------

#if 0
   dword GetNumJoints() const{
      dword count = 0;
      for(dxJointNode *n=firstjoint; n; n=n->next, count++);
      return count;
   }

   dJointID dBodyGetJoint(dword index){
      int i=0;
      for (dxJointNode *n=b->firstjoint; n; n=n->next, i++) {
         if (i == index) return n->joint;
      }
      return 0;
   }

   void SetGravityMode(int mode){
      if (mode) flags &= ~dxBodyNoGravity;
      else flags |= dxBodyNoGravity;
   }

   bool GetGravityMode() const{
      return ((flags & dxBodyNoGravity) == 0);
   }
#endif

//----------------------------

   void MoveAndRotate(dReal h);

//----------------------------

   bool IsConnected1(CPIPH_body b2) const;
   bool IsConnectedExcluding(CPIPH_body b2, int joint_type) const;

//----------------------------
//----------------------------
};

//----------------------------
#endif