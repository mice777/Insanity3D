#include <IPhysics.h>
#include <C_linklist.h>
#include <C_unknwn.hpp>
#include "body.h"
#include "world.h"
#include "matrix.h"
#include "joint.h"
#include <C_vector.h>

//----------------------------
//----------------------------

static void dWtoDQ(const S_vector &w, const S_quat &r, float dq[4]){

   assert(dq);
   const float *q = &r.s;
   dq[0] = .5f * (- w[0]*q[1] - w[1]*q[2] - w[2]*q[3]);
   dq[1] = .5f * (  w[0]*q[0] + w[1]*q[3] - w[2]*q[2]);
   dq[2] = .5f * (- w[0]*q[3] + w[1]*q[0] + w[2]*q[1]);
   dq[3] = .5f * (  w[0]*q[2] - w[1]*q[1] + w[2]*q[0]);
}

//----------------------------
//----------------------------

IPH_body::IPH_body(PIPH_world w):
   ref(1),
   world(w),
   col_check_radius(0.0f),
   mass_center(0, 0, 0),
   num_parents(0),
   user_data(0),
   tag(0),
   firstjoint(NULL),
   flags(0),
   r_mass(1.0f),
   density(1000.0f),
   f_acc(0, 0, 0),
   t_acc(0, 0, 0)
{
   world->AddRef();

   mass.Identity();
   rot.Identity();
   matrix.Identity();
   lvel.Zero();
   avel.Zero();
   inv_i_tensor.Identity();

   world->bodies.Add(this);
   ++world->num_bodies;
}

//----------------------------

IPH_body::~IPH_body(){

   SetFrame(NULL);
   world->Release();

                              //detach all neighbouring joints
   for(dxJointNode *n = firstjoint; n; ){
                              //sneaky trick to speed up removal of joint references (black magic)
      n->joint->node[(n == n->joint->node)].body = NULL;

      dxJointNode *next = n->next;
      n->next = NULL;
      n->joint->RemoveJointReferencesFromAttachedBodies();
      n = next;
   }
   if(world && IsEnabled())
      --world->num_bodies;
}

//----------------------------

void IPH_body::Enable(bool b){

   if(b!=IsEnabled()){
      flags &= ~BF_Disabled;
      if(!b)
         flags |= BF_Disabled;
                              //add/remove from list
      if(b){
         world->bodies.Add(this);
         ++world->num_bodies;
      }else{
         RemoveFromList();
         --world->num_bodies;
      }
   }
}

//----------------------------

void IPH_body::SetMass(const C_mass &m){

   mass = m;
   /*
   if(dInvertPDMatrix(&m.GetInertiaMatrix()(0).x, inv_I, 3)==0){
      assert(("inertia must be positive definite", 0));
      dRSetIdentity(inv_I);
   }
   /**/
   r_mass = 1.0f / mass.GetWeight();
   inv_i_tensor = ~m.GetInertiaMatrix();
}

//----------------------------

bool IPH_body::IsConnected1(CPIPH_body b2) const{

                              //look through b1's neighbour list for b2
   for(dxJointNode *n=firstjoint; n; n=n->next) {
      if(n->body == b2)
         return true;
   }
   return false;
}

//----------------------------

bool IPH_body::IsConnectedExcluding(CPIPH_body b2, int joint_type) const{

                              //look through b1's neighbour list for b2
   for(dxJointNode *n=firstjoint; n; n=n->next) {
      if(n->joint->GetType() != joint_type && n->body == b2)
         return true;
   }
   return false;
}

//----------------------------

bool IPH_body::SetFrame(PI3D_frame frm, dword flags, float in_density){

   if(frm==frame)
      return true;

   if(frame){
      for(dword i=volumes.size(); i--; )
         world->RemoveVolume(volumes[i]);
      volumes.clear();
      col_check_radius = 0.0f;
      num_parents = 0;
   }
   if(!frm){
      frame = NULL;
      return true;
   }
   density = in_density;
                              //collect all body's volumes
   {
      C_vector<PI3D_volume> vols;
      dword i;
      if(flags&IPH_STEFRAME_HR_VOLUMES){
         struct S_hlp{
            C_vector<PI3D_volume> *vols;
            bool also_off;
            static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
               S_hlp *hp = (S_hlp*)c;
               if(!hp->also_off && !frm->IsOn())
                  return I3DENUMRET_SKIPCHILDREN;
               if(frm->GetType()==FRAME_VOLUME)
                  hp->vols->push_back(I3DCAST_VOLUME(frm));
               return I3DENUMRET_OK;
            }
         } hlp;
         hlp.vols = &vols;
         hlp.also_off = (flags&IPH_STEFRAME_OFF_VOLUMES);
         frm->EnumFrames(S_hlp::cbEnum, (dword)&hlp, ENUMF_ALL);
      }else{
         const PI3D_frame *chlds = frm->GetChildren();
         for(i=frm->NumChildren(); i--; ){
            PI3D_frame f = chlds[i];
            if(f->GetType()==FRAME_VOLUME){
               if(!(flags&IPH_STEFRAME_OFF_VOLUMES)){
                              //check if this frame, or any of its parents up to 'frm' is not off
                  for(CPI3D_frame prnt = f; prnt && prnt!=frm; prnt = prnt->GetParent()){
                     if(!prnt->IsOn()){
                        f = NULL;
                        break;
                     }
                  }
               }
               if(f)
                  vols.push_back(I3DCAST_VOLUME(f));
            }
         }
      }
      if(frm->GetType()==FRAME_VOLUME){
         if((flags&IPH_STEFRAME_OFF_VOLUMES) || frm->IsOn())
            vols.push_back(I3DCAST_VOLUME(frm));
      }
      volumes.assign(vols.size());
      for(i=vols.size(); i--; )
         volumes[i] = vols[i];
   }

   const S_matrix &frm_inv = frm->GetInvMatrix();
   float frm_scl = frm->GetWorldScale();

   if(volumes.size())
      col_check_radius = 1e+8f;

                           //compute mass
   C_mass mass;
   mass.Zero();

   for(dword i=volumes.size(); i--; ){
      PI3D_volume vol = volumes[i];

      world->AddVolume(vol, this);

      //vol->SetOwner(frame);
                     //add individual mass of this volume
      C_mass vm;
      float ccr = 0.0f;

      const S_matrix &m = vol->GetMatrix();
      float scl = m(0).Magnitude();

      switch(vol->GetVolumeType()){
      case I3DVOLUME_SPHERE:
         vm.SetSphere(density/DENSITY_MULTIPLIER, scl);
         ccr = scl;
         break;
      case I3DVOLUME_BOX:
         {
            S_vector sides = vol->GetNUScale();
            ccr = Min(sides.x, Min(sides.y, sides.z));
            ccr *= scl;
            sides *= 2.0f * scl;
            vm.SetBox(density/DENSITY_MULTIPLIER, sides);
         }
         break;
      case I3DVOLUME_CAPCYL:
         {
            S_vector sides = vol->GetNUScale();
            ccr = sides.x*scl;
            vm.SetCappedCylinder(density/DENSITY_MULTIPLIER, sides.x*scl, sides.z*scl*2.0f);
         }
         break;
      default: assert(0);
      }
      col_check_radius = Min(col_check_radius, ccr);
      vm.Rotate(vol->GetWorldRot());
      vm.Translate((m(3) * frm_inv) * frm_scl);
                              //add to the entire mass
      mass.Add(vm);
   }
   if(IsMrgZeroLess(mass.GetWeight()))
      return false;

   if(flags&IPH_STEFRAME_DENSITY_AS_WEIGHT){
                              //treat input density as weight

                              //get current real weight
      float curr_weight = mass.GetWeight()*DENSITY_MULTIPLIER;
                              //get scale we want to get
      float w_scale = density / curr_weight;
                              //compute density
      density /= mass.GetVolume();
                              //adjust the mass
      mass.Adjust(w_scale * mass.GetWeight());
   }

   frame = frm;

   {
      for(PI3D_frame f1=frm->GetParent(); f1; f1=f1->GetParent())
         ++num_parents;
   }

   mass_center = mass.GetCenter();
                              //move mass to (0, 0, 0)
   mass.Translate(-mass_center);
   SetMass(mass);

                              //set the mass to body
   if(!(flags&IPH_STEFRAME_USE_TRANSFORM)){
      UpdateFrameTransform();
   }else{
      SetPosRot(frame->GetWorldPos(), frm->GetWorldRot());
   }
   return true;
}

/*
//----------------------------
// Return sin(x)/x. this has a singularity at 0 so special handling is needed for small arguments.
inline dReal sinc(dReal x){

  // if |x| < 1e-4 then use a taylor series expansion. this two term expansion
  // is actually accurate to one LS bit within this range if double precision
  // is being used - so don't worry!
   if(I3DFabs(x) < 1.0e-4f)
      return 1.0f - x * x * 0.166666666666666666667f;
   return (float)sin(x) / x;
}
*/

//----------------------------
// given a body b, apply its linear and angular rotation over the time
// interval h, thereby adjusting its position and orientation.

void IPH_body::MoveAndRotate(dReal h){

                              //handle linear velocity
   matrix(3) += lvel * h;

   /*
   if(flags&BF_FiniteRotation){
      S_vector irv;           //infitesimal rotation vector
      S_quat q;          //quaternion for finite rotation
      
      if(flags&BF_FiniteRotationAxis){
         // split the angular velocity vector into a component along the finite
         // rotation axis, and a component orthogonal to it.
         S_vector frv, irv;    // finite rotation vector
         dReal k = finite_rot_axis.Dot(avel);
         frv[0] = finite_rot_axis[0] * k;
         frv[1] = finite_rot_axis[1] * k;
         frv[2] = finite_rot_axis[2] * k;
         irv[0] = avel[0] - frv[0];
         irv[1] = avel[1] - frv[1];
         irv[2] = avel[2] - frv[2];
         
         // make a rotation quaternion q that corresponds to frv * h.
         // compare this with the full-finite-rotation case below.
         h *= 0.5f;
         dReal theta = k * h;
         q.s = (float)cos(theta);
         dReal s = sinc(theta) * h;
         q.v = frv * s;
      }else{
                              //make a rotation quaternion q that corresponds to w * h
         //dReal wlen = dSqrt(avel[0]*avel[0] + avel[1]*avel[1] + avel[2]*avel[2]);
         dReal wlen = avel.Magnitude();
         h *= .5f;
         dReal theta = wlen * h;
         q.s = (float)cos(theta);
         dReal s = sinc(theta) * h;
         q.v = avel * s;
      }
      
                              //do the finite rotation
      //S_quat q2; dQMultiply0(&q2.s, &q.s, &rot.s);
      //rot = q2;
      rot = q * rot;
      
      // do the infitesimal rotation if required
      if(flags&BF_FiniteRotationAxis){
         dReal dq[4];
         dWtoDQ(irv, rot, dq);
         for(int j=0; j<4; j++)
            (&rot.s)[j] += h * dq[j];
      }
   }else*/
   {
      S_quat _rot = ~rot;
                              //the normal way - do an infitesimal rotation
      dReal dq[4];
      dWtoDQ(avel, _rot, dq);
      for(int j=0; j<4; j++)
         (&_rot.s)[j] += h * dq[j];
      rot = ~_rot;
   }
                              //normalize the quaternion and convert it to a rotation matrix
   rot.Normalize();
   matrix.SetRot(rot);
   //dQtoR(~rot, _rot_matrix);
}

//----------------------------
//----------------------------
