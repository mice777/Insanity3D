#include <IPhysics.h>
#include <C_linklist.h>
#include <C_unknwn.hpp>
#include "joint.h"
#include "body.h"
#include "world.h"

//----------------------------
//----------------------------

/*
design note: the general principle for giving a joint the option of connecting
to the static environment (i.e. the absolute frame) is to check the second
body (joint->node[1].body), and if it is zero then behave as if its body
transform is the identity.

*/

//----------------------------
// Given a unit length "normal" vector n, generate vectors p and q vectors
// that are an orthonormal basis for the plane space perpendicular to n.
// i.e. this makes p,q such that n,p,q are all perpendicular to each other.
// q will equal n x p. if n is not unit length then p will be unit length but
// q wont be.
void PlaneSpace(const S_vector &n, S_vector &p, S_vector &q){

   if(I3DFabs(n[2]) > M_SQRT1_2){
                              //choose p in y-z plane
      dReal a = n[1]*n[1] + n[2]*n[2];
      dReal k = 1.0f / I3DSqrt(a);
      p[0] = 0;
      p[1] = -n[2]*k;
      p[2] = n[1]*k;
                              //set q = n x p
      q[0] = a*k;
      q[1] = -n[0]*p[2];
      q[2] = n[0]*p[1];
   }else {
                              //choose p in x-y plane
      dReal a = n[0]*n[0] + n[1]*n[1];
      dReal k = 1.0f / I3DSqrt(a);
      p[0] = -n[1]*k;
      p[1] = n[0]*k;
      p[2] = 0;
                              //set q = n x p
      q[0] = -n[2]*p[1];
      q[1] = n[2]*p[0];
      q[2] = a*k;
   }
}

//----------------------------
// Set a 3x3 submatrix of A to a matrix such that submatrix(A)*b = a x b.
// A is stored by rows, and has `skip' elements per row. the matrix is
// assumed to be already zero, so this does not write zero elements!
// if (plus,minus) is (+,-) then a positive version will be written.
// if (plus,minus) is (-,+) then a negative version will be written.
static void dCROSSMAT(float *A, const float *a, dword skip, float plus, float minus){
   A[1] = minus * a[2];
   A[2] = plus * a[1];
   A[(skip)+0] = plus * a[2];
   A[(skip)+2] = minus * a[0];
   A[2*(skip)+0] = minus * a[1];
   A[2*(skip)+1] = plus * a[0];
}
//----------------------------
//----------------------------

IPH_joint::IPH_joint(PIPH_world w):
   ref(1),
   tag(0),
   world(w),
   user_data(0)
{
   flags = 0;
   feedback = NULL;
   node[0].joint = this;
   node[0].body = 0;
   node[0].next = 0;
   node[1].joint = this;
   node[1].body = 0;
   node[1].next = 0;

   ++w->num_joints;
   w->joints.Add(this);
}

//----------------------------

IPH_joint::~IPH_joint(){

   RemoveJointReferencesFromAttachedBodies();
   if(world)
      --world->num_joints;
}

//----------------------------

void IPH_joint::SetBall(Info2 *info, const S_vector &anchor1, const S_vector &anchor2) const{
   
                              //anchor points in global coordinates with respect to body PORs.
   S_vector a1, a2;
   
   int s = info->rowskip;
   
                              //set jacobian
   info->J1l[0] = 1;
   info->J1l[s+1] = 1;
   info->J1l[2*s+2] = 1;
   a1 = anchor1 % node[0].body->GetMatrix();
   dCROSSMAT(info->J1a, &a1.x, s, -1, +1);
   if(node[1].body){
      info->J2l[0] = -1;
      info->J2l[s+1] = -1;
      info->J2l[2*s+2] = -1;
      a2 = anchor2 % node[1].body->GetMatrix();
      dCROSSMAT(info->J2a, &a2.x, s, +1, -1);
   }
   
                              //set right hand side
   dReal k = info->fps * info->erp;
   if(node[1].body){
      *(S_vector*)info->_c = (a2 + node[1].body->GetPos() - a1 - node[0].body->GetPos()) * k;
   }else{
      *(S_vector*)info->_c = (anchor2 - a1 - node[0].body->GetPos()) * k;
   }
}

//----------------------------

void IPH_joint::SetBall2(Info2 *info, const S_vector &anchor1, const S_vector &anchor2, const S_vector &axis, dReal erp1) const{

                              //anchor points in global coordinates with respect to body PORs
   S_vector a1, a2;

   int i, s = info->rowskip;
   
                              // get vectors normal to the axis. in setBall() axis,q1,q2 is [1 0 0],
                              // [0 1 0] and [0 0 1], which makes everything much easier.
   S_vector q1, q2;
   PlaneSpace(axis, q1, q2);
   
                              //set jacobian
   *(S_vector*)info->J1l = axis;
   for(i=0; i<3; i++) info->J1l[s+i] = q1[i];
   for(i=0; i<3; i++) info->J1l[2*s+i] = q2[i];
   a1 = anchor1 % node[0].body->GetMatrix();
   *(S_vector*)info->J1a = a1.Cross(axis);
   *(S_vector*)(info->J1a+s) = a1.Cross(q1);
   *(S_vector*)(info->J1a+2*s) = a1.Cross(q2);
   if(node[1].body){
      for (i=0; i<3; i++) info->J2l[i] = -axis[i];
      for (i=0; i<3; i++) info->J2l[s+i] = -q1[i];
      for (i=0; i<3; i++) info->J2l[2*s+i] = -q2[i];
      a2 = anchor2 % node[1].body->GetMatrix();
      *(S_vector*)(info->J2a) = -a2.Cross(axis);
      *(S_vector*)(info->J2a+s) = -a2.Cross(q1);
      *(S_vector*)(info->J2a+2*s) = -a2.Cross(q2);
   }else{
      a2.Zero();
   }
   
                              //set right hand side - measure error along (axis,q1,q2)
   dReal k1 = info->fps * erp1;
   dReal k = info->fps * info->erp;
   
   a1 += node[0].body->GetPos();
   if(node[1].body) {
      a2 += node[1].body->GetPos();
      info->_c[0] = k1 * (axis.Dot(a2) - axis.Dot(a1));
      info->_c[1] = k * (q1.Dot(a2) - q1.Dot(a1));
      info->_c[2] = k * (q2.Dot(a2) - q2.Dot(a1));
   }else{
      info->_c[0] = k1 * (axis.Dot(anchor2) - axis.Dot(a1));
      info->_c[1] = k * (q1.Dot(anchor2) - q1.Dot(a1));
      info->_c[2] = k * (q2.Dot(anchor2) - q2.Dot(a1));
   }
}

//----------------------------

void IPH_joint::RemoveJointReferencesFromAttachedBodies(){

   for(int i=0; i<2; i++){
      PIPH_body body = node[i].body;
      if(body){
         dxJointNode *n = body->firstjoint;
         dxJointNode *last = NULL;
         while(n){
            if(n->joint == this){
               if(last)
                  last->next = n->next;
               else
                  body->firstjoint = n->next;
               break;
            }
            last = n;
            n = n->next;
         }
      }
   }
   node[0].body = 0;
   node[0].next = 0;
   node[1].body = 0;
   node[1].next = 0;
}

//----------------------------

void IPH_joint::Attach(PIPH_body body1, PIPH_body body2){

#ifdef _DEBUG
                              //check arguments
   assert(("can't have body1==body2", body1 == 0 || body1 != body2));
   assert(("joint and bodies must be in same world", ((!body1 || body1->world == world) && (!body2 || body2->world == world))));
   
                              //check if the joint can not be attached to just one body
   assert(("joint can not be attached to just one body", !((flags & JF_TWOBODIES) && ((body1 != 0) ^ (body2 != 0)))));
#endif

                              //remove any existing body attachments
   if(node[0].body || node[1].body)
      RemoveJointReferencesFromAttachedBodies();
   
                              //if a body is zero, make sure that it is body2, so 0 --> node[1].body
   if(body1==0){
      body1 = body2;
      body2 = 0;
      flags &= ~JF_REVERSE;
   }else{
      flags |= JF_REVERSE;
   }
   
                              //attach to new bodies
   node[0].body = body1;
   node[1].body = body2;
   if(body1){
      node[1].next = body1->firstjoint;
      body1->firstjoint = &node[1];
   }else
      node[1].next = NULL;
   if(body2){
      node[0].next = body2->firstjoint;
      body2->firstjoint = &node[0];
   }else{
      node[0].next = NULL;
   }
}

//----------------------------

void IPH_joint::SetAnchors(const S_vector &ap, S_vector &anchor1, S_vector &anchor2) const{

   if(node[0].body){
      //S_vector q = ap - node[0].body->GetPos();
      //dMULTIPLY1_331(&anchor1.x, node[0].body->GetRotMatrix(), &q.x);
      anchor1 = (ap - node[0].body->GetPos()) % node[0].body->GetInvMatrix();
      if(node[1].body){
         /*
         S_vector q = ap - node[1].body->GetPos();
         dMULTIPLY1_331 (&anchor2.x, node[1].body->GetRotMatrix(), &q.x);
         */
         anchor2 = (ap - node[1].body->GetPos()) % node[1].body->GetInvMatrix();
      }else{
         anchor2 = ap;
      }
   }
}

//----------------------------

void IPH_joint::SetAxes(const S_vector &ax, S_vector *axis1, S_vector *axis2) const{

   if(node[0].body){
      S_normal n_ax = ax;
      *axis1 = n_ax % node[0].body->GetInvMatrix();
      if(axis2){
         if(node[1].body){
            *axis2 = n_ax % node[1].body->GetInvMatrix();
         }else{
            *axis2 = n_ax;
         }
      }
   }
}

//----------------------------

void IPH_joint::GetAnchor1(S_vector &result, const S_vector &anchor1) const{

   if(node[0].body) {
      //dMULTIPLY0_331(&result.x, node[0].body->GetRotMatrix(), &anchor1.x);
      //result += node[0].body->GetPos();
      result = anchor1 * node[0].body->GetMatrix();
   }
}

//----------------------------

void IPH_joint::GetAxis(S_vector &result, const S_vector &axis1) const{
   
   if(node[0].body)
      result = axis1 % node[0].body->GetMatrix();
}

//----------------------------
//----------------------------

class IPH_joint_ball: public IPH_joint{
   dword num_axes;            //number of axes (0 ... 3)
   bool user_mode;
   int rel[3];                //what the axes are relative to (global,b1,b2)
   S_vector axis[3];          //three axes
   dxJointLimitMotor limot[3];//limit+motor info for axes
   float angle[3];            //user-supplied angles for axes
                              //these vectors are used for calculating euler angles
   S_vector reference[2];     //original axis, relative to body n

                              //ball params:
   bool is_ball;
                              //anochors relative to bodies
   S_vector anchor[2];


   friend class IPH_joint_ball;

//----------------------------

   virtual IPH_JOINT_TYPE GetType() const{ return IPHJOINTTYPE_BALL; }

//----------------------------

   virtual bool SetAnchor(const S_vector &a){

      is_ball = true;
      SetAnchors(a, anchor[0], anchor[1]);
      return true;
   }

//----------------------------

   virtual void SetNumAxes(dword n){

      if(n > 3)
         throw C_except(C_xstr("IPH_joint_ball: invalid number of axes: %") % n);

      if(!user_mode){
         num_axes = 3;
      }else{
         num_axes = n;
      }
   }

//----------------------------

   virtual void SetUserMode(bool b){

      user_mode = b;
      if(!user_mode){
         num_axes = 3;
         SetEulerReferenceVectors();
      }
   }

//----------------------------

   enum E_AMOTOR_AXIS_MODE{
      AXIS_GLOBAL,
      AXIS_BODY1,
      AXIS_BODY2,
      AXIS_AUTO,
   };

   virtual void SetAxis(dword index, const S_vector &a, E_AMOTOR_AXIS_MODE mode){

      if(index > num_axes)
         throw C_except(C_xstr("IPH_joint_ball: invalid axis: %") % index);
      if(!user_mode && index==1)
         throw C_except("IPH_joint_ball: cannot specify axis 1 in euler mode");
      if(mode==AXIS_AUTO){
         if(user_mode)
            throw C_except("IPH_joint_ball: AXIS_AUTO allowed only in euler mode");
         mode = !index ? AXIS_BODY1 : AXIS_BODY2;
         if(index==2 && !GetBody(1))
            mode = AXIS_GLOBAL;
      }
      SetAMotorAxis(index, mode, a);
   }

//----------------------------

   virtual void SetAngle(dword index, float a){

      if(index > num_axes)
         throw C_except(C_xstr("IPH_joint_ball: invalid axis: %") % index);
      if(!user_mode)
         throw C_except("IPH_joint_ball: SetAngle may be called only in user mode");

      angle[index] = a;
   }

//----------------------------

   void GetInfo1(Info1 *info){

      if(is_ball){
         info->m = 3;
         info->nub = 3;
      }else{
         info->m = 0;
         info->nub = 0;
      }
   
                                 //compute the axes and angles, if in euler mode
      if(!user_mode){
         S_vector ax[3];
         ComputeGlobalAxes(ax);
         ComputeEulerAngles(ax);
      }
   
                                 //see if we're powered or at a joint limit for each axis
      for(dword i=0; i<num_axes; i++){
         if(limot[i].TestRotationalLimit(angle[i]) || limot[i].fmax > 0)
            ++info->m;
      }
   }

//----------------------------

   void GetInfo2(Info2 *info) const{

      int row = 0;
      if(is_ball){
         SetBall(info, anchor[0], anchor[1]);
         row = 3;
      }

                                 //compute the axes (if not global)
      S_vector ax[3];
      ComputeGlobalAxes(ax);

                              //in euler angle mode we do not actually constrain the angular velocity
                              // along the axes axis[0] and axis[2] (although we do use axis[1]) :
                              //
                              //    to get			constrain w2-w1 along		...not
                              //    ------			---------------------		------
                              //    d(angle[0])/dt = 0	ax[1] x ax[2]			ax[0]
                              //    d(angle[1])/dt = 0	ax[1]
                              //    d(angle[2])/dt = 0	ax[0] x ax[1]			ax[2]
                              //
                              // constraining w2-w1 along an axis 'a' means that a'*(w2-w1)=0.
                              // to prove the result for angle[0], write the expression for angle[0] from
                              // GetInfo1 then take the derivative. to prove this for angle[2] it is
                              // easier to take the euler rate expression for d(angle[2])/dt with respect
                              // to the components of w and set that to 0.
      S_vector *axptr[3];
      axptr[0] = &ax[0];
      axptr[1] = &ax[1];
      axptr[2] = &ax[2];

      S_vector ax0_cross_ax1;
      S_vector ax1_cross_ax2;
      if(!user_mode){
         ax0_cross_ax1 = ax[0].Cross(ax[1]);
         axptr[2] = &ax0_cross_ax1;
         ax1_cross_ax2 = ax[1].Cross(ax[2]);
         axptr[0] = &ax1_cross_ax2;
      }
   
      for(dword i=0; i<num_axes; i++){
         if(limot[i].AddLimot(this, info, row, *(axptr[i]), true))
            ++row;
      }
   }

//----------------------------
// Compute the 3 axes in global coordinates.
   void ComputeGlobalAxes(S_vector ax[3]) const{

      if(!user_mode){
                                 //special handling for euler mode
         ax[0] = axis[0] % node[0].body->GetMatrix();
         if(node[1].body){
            ax[2] = axis[2] % node[1].body->GetMatrix();
         }else
            ax[2] = axis[2];
         ax[1] = ax[2].Cross(ax[0]);
         ax[1].Normalize();
      }else{
         for(dword i=0; i<num_axes; i++){
            switch(rel[i]){
            case 1:           //relative to b1
               ax[i] = axis[i] % node[0].body->GetMatrix();
               break;
            case 2:
                                 //relative to b2
               ax[i] = axis[i] % node[1].body->GetMatrix();
               break;
            default:
                                 //global - just copy it
               ax[i] = axis[i];
            }
         }
      }
   }


//----------------------------

   void ComputeEulerAngles(S_vector ax[3]){

                                       // assumptions:
                                       //   global axes already calculated --> ax
                                       //   axis[0] is relative to body 1 --> global ax[0]
                                       //   axis[2] is relative to body 2 --> global ax[2]
                                       //   ax[1] = ax[2] x ax[0]
                                       //   original ax[0] and ax[2] are perpendicular
                                       //   reference1 is perpendicular to ax[0] (in body 1 frame)
                                       //   reference2 is perpendicular to ax[2] (in body 2 frame)
                                       //   all ax[] and reference vectors are unit length
   
                                 //calculate references in global frame
      S_vector ref1, ref2;
      ref1 = reference[0] % node[0].body->GetMatrix();
      if(node[1].body)
         ref2 = reference[1] % node[1].body->GetMatrix();
      else
         ref2 = reference[1];
   
                              //get q perpendicular to both ax[0] and ref1, get first euler angle
      S_vector q = ax[0].Cross(ref1);
      angle[0] = -atan2(ax[2].Dot(q), ax[2].Dot(ref1));
   
                              //get q perpendicular to both ax[0] and ax[1], get second euler angle
      q = ax[0].Cross(ax[1]);
      angle[1] = -atan2(ax[2].Dot(ax[0]), ax[2].Dot(q));
   
                              //get q perpendicular to both ax[1] and ax[2], get third euler angle
      q = ax[1].Cross(ax[2]);
      angle[2] = -atan2(ref2.Dot(ax[1]), ref2.Dot(q));
   }

//----------------------------
// Set the reference vectors as follows:
//   * reference1 = current axis[2] relative to body 1
//   * reference2 = current axis[0] relative to body 2
// this assumes that:
//    * axis[0] is relative to body 1
//    * axis[2] is relative to body 2
   void SetEulerReferenceVectors(){

      PIPH_body b0 = node[0].body, b1 = node[1].body;
      if(b0 && b1){
         {
            S_vector r = axis[2] % b1->GetMatrix();
            reference[0] = r % b0->GetInvMatrix();
         }
         {
            S_vector r = axis[0] % b0->GetMatrix();
            reference[1] = r % b1->GetInvMatrix();
         }
      }
   }

//----------------------------

   void SetAMotorAxis(int anum, int in_rel, const S_vector &a){

      assert(anum >= 0 && anum <= 2 && in_rel >= 0 && in_rel <= 2);
      if (anum < 0) anum = 0;
      if (anum > 2) anum = 2;
      rel[anum] = in_rel;

                              //x,y,z is always in global coordinates regardless of rel, so we may have
                              // to convert it to be relative to a body
      S_vector r = a;
      if(in_rel > 0){
         if(in_rel==1){
            axis[anum] = r % node[0].body->GetInvMatrix();
         }else{
            axis[anum] = r % node[1].body->GetInvMatrix();
         }
      }else{
         axis[anum] = r;
      }
      axis[anum].Normalize();
      if(!user_mode)
         SetEulerReferenceVectors();
   }

//----------------------------

   virtual dxJointLimitMotor *GetLimot(dword index){
      if(index<3) return &limot[index];
      return NULL;
   }

//----------------------------

public:
   IPH_joint_ball(PIPH_world w):
      IPH_joint(w),
      user_mode(false),
      num_axes(3),
      is_ball(false)
   {
      for(int i=0; i<3; i++){
         rel[i] = 0;
         axis[i].Zero();
         limot[i].Init(w);
         angle[i] = 0;
      }
      for(i=0; i<2; i++){
         anchor[i].Zero();
         reference[i].Zero();
      }
      //flags |= JF_TWOBODIES;
   }
};

//----------------------------

PIPH_joint CreateJointBall(PIPH_world w){
   return new IPH_joint_ball(w);
}

//----------------------------
//----------------------------
// slider. if body2 is 0 then qrel is the absolute rotation of body1 and
// offset is the position of body1 center along axis1.

class IPH_joint_slider: public IPH_joint{
   S_vector axis;

   S_vector axis1;            //axis w.r.t first body
   S_quat qrel;               //initial relative rotation body1 -> body2
   S_vector offset;           //point relative to body2 that should be aligned with body1 center along axis1
   dxJointLimitMotor limot;	//limit and motor information

//----------------------------
   virtual void SetAxis(const S_vector &a){
      axis = a;
      SetSliderAxis(a);
   }

//----------------------------

   virtual float GetSlidePos() const{
                              //get axis1 in global coordinates
      S_vector ax1, q;
      ax1 = axis1 % node[0].body->GetMatrix();
   
      if(node[1].body){
                              //get body2 + offset point in global coordinates
         q = offset % node[1].body->GetMatrix();
         q = node[0].body->GetPos() - q - node[1].body->GetPos();
      }else{
         q = node[0].body->GetPos() - offset;
      }
      return ax1.Dot(q);
   }

//----------------------------

   virtual float GetSlidePosRate() const{

                              //get axis1 in global coordinates
      S_vector ax1 = axis1 % node[0].body->GetMatrix();
   
      if(node[1].body){
         return ax1.Dot(node[0].body->GetLVel()) - ax1.Dot(node[1].body->GetLVel());
      }else{
         return ax1.Dot(node[0].body->GetLVel());
      }
   }

//----------------------------

   void SetSliderAxis(const S_vector &a){

      SetAxes(a, &axis1, NULL);

                                 //compute initial relative rotation body1 -> body2, or env -> body1
                                 // also compute center of body1 w.r.t body 2
      if(node[1].body){
         qrel = node[0].body->GetRot() * ~node[1].body->GetRot();
         S_vector c = node[0].body->GetPos() - node[1].body->GetPos();
         offset = c % node[1].body->GetInvMatrix();
      }else{
                                 //set qrel to the transpose of the first body's q
         qrel = node[0].body->GetRot();
         offset = node[0].body->GetPos();
      }
   }

//----------------------------

   virtual IPH_JOINT_TYPE GetType() const{ return IPHJOINTTYPE_SLIDER; }

//----------------------------

   virtual void GetInfo1(Info1 *info){

      info->nub = 5;

                                 //see if joint is powered
      if(limot.fmax > 0)
         info->m = 6;	// powered slider needs an extra constraint row
      else
         info->m = 5;

                                 //see if we're at a joint limit.
      limot.limit = 0;
      if((limot.lostop > -dInfinity || limot.histop < dInfinity) && limot.lostop <= limot.histop){
                                 //measure joint position
         dReal pos = GetSlidePos();
         if(pos <= limot.lostop){
            limot.limit = 1;
            limot.limit_err = pos - limot.lostop;
            info->m = 6;
         }else
         if(pos >= limot.histop){
            limot.limit = 2;
            limot.limit_err = pos - limot.histop;
            info->m = 6;
         }
      }
   }

//----------------------------

   virtual void GetInfo2(Info2 *info) const{
   
      int i, s = info->rowskip;
      int s2=2*s,s3=3*s,s4=4*s;
   
                              //pull out pos and R for both bodies. also get the `connection' vector pos2-pos1
   
      const S_matrix *R1, *R2;
      S_vector c;
      const S_vector &pos1 = node[0].body->GetPos();
      const S_vector *pos2;
      R1 = &node[0].body->GetMatrix();
      if(node[1].body){
         pos2 = &node[1].body->GetPos();
         R2 = &node[1].body->GetMatrix();
         c = *pos2 - pos1;
      }else{
         pos2 = NULL;
         R2 = NULL;
         c.Zero();
      }
   
                              //3 rows to make body rotations equal
      info->J1a[0] = 1;
      info->J1a[s+1] = 1;
      info->J1a[s2+2] = 1;
      if(node[1].body){
         info->J2a[0] = -1;
         info->J2a[s+1] = -1;
         info->J2a[s2+2] = -1;
      }
   
                              // remaining two rows. we want: vel2 = vel1 + w1 x c ... but this would
                              // result in three equations, so we project along the planespace vectors
                              // so that sliding along the slider axis is disregarded. for symmetry we
                              // also substitute (w1+w2)/2 for w1, as w1 is supposed to equal w2.
   
      S_vector p, q;	         //plane space of ax1
                              //joint axis in global coordinates (unit length)
      S_vector ax1 = axis1 % *R1;
      PlaneSpace(ax1, p, q);
      if(node[1].body){
         S_vector tmp = c.Cross(p) * .5f;
         for (i=0; i<3; i++) info->J2a[s3+i] = tmp[i];
         for (i=0; i<3; i++) info->J2a[s3+i] = tmp[i];
         tmp = c.Cross(q) * .5f;
         for (i=0; i<3; i++) info->J2a[s4+i] = tmp[i];
         for (i=0; i<3; i++) info->J2a[s4+i] = tmp[i];
         for (i=0; i<3; i++) info->J2l[s3+i] = -p[i];
         for (i=0; i<3; i++) info->J2l[s4+i] = -q[i];
      }
      for(i=0; i<3; i++)
         info->J1l[s3+i] = p[i];
      for(i=0; i<3; i++)
         info->J1l[s4+i] = q[i];
   
                              //compute the right hand side. the first three elements will result in
                              // relative angular velocity of the two bodies - this is set to bring them
                              // back into alignment. the correcting angular velocity is 
                              //   |angular_velocity| = angle/time = erp*theta / stepsize
                              //                      = (erp*fps) * theta
                              //    angular_velocity  = |angular_velocity| * u
                              //                      = (erp*fps) * theta * u
                              // where rotation along unit length axis u by theta brings body 2's frame
                              // to qrel with respect to body 1's frame. using a small angle approximation
                              // for sin(), this gives
                              //    angular_velocity  = (erp*fps) * 2 * v
                              // where the quaternion of the relative rotation between the two bodies is
                              //    q = [cos(theta/2) sin(theta/2)*u] = [s v]

                              // get qerr = relative rotation (rotation error) between two bodies
      S_quat qerr;
      if(node[1].body){
         S_quat qq;
         qq = node[0].body->GetRot() * ~node[1].body->GetRot();
         qerr = qq * ~qrel;
      }else{
         qerr = node[0].body->GetRot() * ~qrel;
      }
      if(qerr.s < 0.0f){
         qerr.v.Invert();
      }
      S_vector e = qerr.v % node[0].body->GetMatrix();
      dReal k = info->fps * info->erp;
      info->_c[0] = 2*k * e[0];
      info->_c[1] = 2*k * e[1];
      info->_c[2] = 2*k * e[2];
   
                              //compute last two elements of right hand side. we want to align the offset
                              // point (in body 2's frame) with the center of body 1.
      if(node[1].body){
                              //offset point in global coordinates
         S_vector ofs = offset % *R2;
         c += ofs;
         info->_c[3] = k * p.Dot(c);
         info->_c[4] = k * q.Dot(c);
      }else{
                              //offset point in global coordinates
         S_vector ofs = offset - pos1;
         info->_c[3] = k * p.Dot(ofs);
         info->_c[4] = k * q.Dot(ofs);
      }
   
                                 //if the slider is powered, or has joint limits, add in the extra row
      limot.AddLimot(this, info, 5, ax1, false);
   }

//----------------------------

   virtual dxJointLimitMotor *GetLimot(dword index){
      switch(index){
      case 0: return &limot; break;
      }
      return NULL;
   }

public:
   IPH_joint_slider(PIPH_world w):
      IPH_joint(w),
      axis(1, 0, 0)
   {
      axis1.Zero();
      axis1[0] = 1;
      qrel.Identity();
      offset.Zero();
      limot.Init(w);
   }
};

PIPH_joint CreateJointSlider(PIPH_world w){

   return new IPH_joint_slider(w);
}

//----------------------------
//----------------------------

class IPH_joint_fixed: public IPH_joint{

                              //relative offset of body 0 relative to body1 (or world if body1 is not set)
   S_vector offset;
   S_quat rot;

//----------------------------

   virtual IPH_JOINT_TYPE GetType() const{ return IPHJOINTTYPE_FIXED; }

//----------------------------

   virtual void SetFixed(){

                              //compute the offset between the bodies
      if(node[0].body){
         if(node[1].body){
            S_vector delta = node[0].body->GetPos() - node[1].body->GetPos();
            offset = delta % node[0].body->GetInvMatrix();

            rot = node[1].body->GetRot() * ~node[0].body->GetRot();
         }else{
            offset = node[0].body->GetPos();
            rot = ~node[0].body->GetRot();
         }
      }
   }

//----------------------------

   virtual void GetInfo1 (Info1 *info){

      info->m = 6;
      info->nub = 6;
   }

//----------------------------

   virtual void GetInfo2(Info2 *info) const{
   
      int s = info->rowskip;
   
                                 //set jacobian
      info->J1l[0] = 1;
      info->J1l[s+1] = 1;
      info->J1l[2*s+2] = 1;
      info->J1a[3*s] = 1;
      info->J1a[4*s+1] = 1;
      info->J1a[5*s+2] = 1;

      PIPH_body body0 = node[0].body;
      PIPH_body body1 = node[1].body;

                                 //set right hand side for the first three rows (linear)
      float k = info->fps * info->erp;

      if(body1){
         S_vector ofs = offset % body0->GetMatrix();
         dCROSSMAT (info->J1a, &ofs.x, s, +1, -1);
         info->J2l[0] = -1;
         info->J2l[s+1] = -1;
         info->J2l[2*s+2] = -1;
         info->J2a[3*s] = -1;
         info->J2a[4*s+1] = -1;
         info->J2a[5*s+2] = -1;
      //}
      //if(body1){
         *((S_vector*)info->_c) = (body1->GetPos() - body0->GetPos() + ofs) * k;
      }else{
         *((S_vector*)info->_c) = (offset - body0->GetPos()) * k;
      }
   
                                 //set right hand side for the next three rows (angular). this code is
                                 // borrowed from the slider, so look at the comments there.
                                 // @@@ make a function common to both the slider and this joint !!!
   
                                 //get qerr = relative rotation (rotation error) between two bodies
      S_quat qerr = body0->GetRot();
      if(body1)
         qerr *= ~body1->GetRot();
      qerr *= rot;
      if(qerr.s < 0.0f){
         qerr.v.Invert();
      }
      S_vector e = qerr.v % body0->GetMatrix();
      info->_c[3] = 2*k * e[0];
      info->_c[4] = 2*k * e[1];
      info->_c[5] = 2*k * e[2];
   }

//----------------------------
public:
   IPH_joint_fixed(PIPH_world w):
      IPH_joint(w)
   {
      offset.Zero();
      rot.Identity();
   }
};

//----------------------------

PIPH_joint CreateJointFixed(PIPH_world w){
   return new IPH_joint_fixed(w);
}

//----------------------------
//----------------------------

class IPH_joint_universal: public IPH_joint{

   S_vector anchor[2];      // anchor w.r.t first body
   S_vector axis1;      // axis w.r.t first body
   S_vector axis2;      // axis w.r.t second body

//----------------------------

   virtual IPH_JOINT_TYPE GetType() const{ return IPHJOINTTYPE_UNIVERSAL; }

//----------------------------

   virtual bool SetAnchor(const S_vector &a){
      SetAnchors(a, anchor[0], anchor[1]);
      return true;
   }

//----------------------------
   
   virtual bool SetLocalAnchor(dword bi, const S_vector &a){
      assert(bi<2);
      anchor[bi] = a;
      return true;
   }

//----------------------------

   virtual void SetAxis(dword index, const S_vector &a){
      assert(index < 2);
      S_vector &axis = !index ? axis1 : axis2;
      if(node[index].body){
         S_normal n_a = a;
         axis = n_a % node[index].body->GetInvMatrix();
      }
   }
//----------------------------

   virtual void GetInfo1(Info1 *info){
     info->nub = 4;
     info->m = 4;
   }

//----------------------------

   virtual void GetInfo2(Info2 *info) const{

                                 //set the three ball-and-socket rows
      SetBall(info, anchor[0], anchor[1]);

                              //set the universal joint row. the angular velocity about an axis
                              // perpendicular to both joint axes should be equal. thus the constraint
                              // equation is
                              //    p*w1 - p*w2 = 0
                              // where p is a vector normal to both joint axes, and w1 and w2
                              // are the angular velocity vectors of the two bodies.

                              //length 1 joint axis in global coordinates, from each body
      S_vector ax2;
                              //length 1 vector perpendicular to ax1 and ax2. Neither body can rotate
                              // about this.
   
      S_vector ax1 = axis1 % node[0].body->GetMatrix();
      if(node[1].body){
         ax2 = axis2 % node[1].body->GetMatrix();
      }else{
         ax2 = axis2;
      }
   
                              //if ax1 and ax2 are almost parallel, p won't be perpendicular to them.
                              // Is there some more robust way to do this?
      S_vector p = ax1.Cross(ax2);
      p.Normalize();
   
      int s3 = 3*info->rowskip;
   
      info->J1a[s3+0] = p[0];
      info->J1a[s3+1] = p[1];
      info->J1a[s3+2] = p[2];
   
      if (node[1].body) {
         info->J2a[s3+0] = -p[0];
         info->J2a[s3+1] = -p[1];
         info->J2a[s3+2] = -p[2];
      }
   
                                // compute the right hand side of the constraint equation. set relative
                                // body velocities along p to bring the axes back to perpendicular.
                                // If ax1, ax2 are unit length joint axes as computed from body1 and
                                // body2, we need to rotate both bodies along the axis p.  If theta
                                // is the angle between ax1 and ax2, we need an angular velocity
                                // along p to cover the angle erp * (theta - Pi/2) in one step:
                                //
                                //   |angular_velocity| = angle/time = erp*(theta - Pi/2) / stepsize
                                //                      = (erp*fps) * (theta - Pi/2)
                                //
                                // if theta is close to Pi/2, 
                                // theta - Pi/2 ~= cos(theta), so
                                //    |angular_velocity|  = (erp*fps) * (ax1 dot ax2)

      info->_c[3] = info->fps * info->erp * - ax1.Dot(ax2);
   }

//----------------------------

public:
   IPH_joint_universal(PIPH_world w):
      IPH_joint(w),
      axis1(1, 0, 0),
      axis2(0, 1, 0)
   {
      for(int i=2; i--; ){
         anchor[i].Zero();
      }
   }
};

//----------------------------

PIPH_joint CreateJointUniversal(PIPH_world w){
   return new IPH_joint_universal(w);
}

//----------------------------
//----------------------------

class IPH_joint_hinge: public IPH_joint{

   S_vector anchor[2];
   S_vector axis1;      // axis w.r.t first bod
   S_vector axis2;      // axis w.r.t second bod
   S_quat qrel;      // initial relative rotation body1 -> body
   dxJointLimitMotor limot;	// limit and motor information


//----------------------------
// given two bodies (body1,body2), the hinge axis that they are connected by
// w.r.t. body1 (axis), and the initial relative orientation between them
// (q_initial), return the relative rotation angle. the initial relative
// orientation corresponds to an angle of zero. if body2 is 0 then measure the
// angle between body1 and the static frame.
//
// this will not return the correct angle if the bodies rotate along any axis
// other than the given hinge axis.
   static dReal GetHingeAngle(CPIPH_body body1, CPIPH_body body2, const S_vector &axis, const S_quat &q_initial){

                                // the angle between the two bodies is extracted from the quaternion that
                                // represents the relative rotation between them. recall that a quaternion
                                // q is:
                                //    [s,v] = [ cos(theta/2) , sin(theta/2) * u ]
                                // where s is a scalar and v is a 3-vector. u is a unit length axis and
                                // theta is a rotation along that axis. we can get theta/2 by:
                                //    theta/2 = atan2 ( sin(theta/2) , cos(theta/2) )
                                // but we can't get sin(theta/2) directly, only its absolute value, i.e.:
                                //    |v| = |sin(theta/2)| * |u|
                                //        = |sin(theta/2)|
                                // using this value will have a strange effect. recall that there are two
                                // quaternion representations of a given rotation, q and -q. typically as
                                // a body rotates along the axis it will go through a complete cycle using
                                // one representation and then the next cycle will use the other
                                // representation. this corresponds to u pointing in the direction of the
                                // hinge axis and then in the opposite direction. the result is that theta
                                // will appear to go "backwards" every other cycle. here is a fix: if u
                                // points "away" from the direction of the hinge (motor) axis (i.e. more
                                // than 90 degrees) then use -q instead of q. this represents the same
                                // rotation, but results in the cos(theta/2) value being sign inverted.

                                 //get qrel = relative rotation between the two bodies
      S_quat qrel;
      if(body2){
         S_quat qq;
         qq = body1->GetRot() * ~body2->GetRot();
         qrel = qq * ~q_initial;
      }else{
                              //pretend body2->q is the identity
         qrel = body1->GetRot() * ~q_initial;
      }
   
                              // extract the angle from the quaternion. cost2 = cos(theta/2),
                              // sint2 = |sin(theta/2)|
      dReal cost2 = qrel.s;
      dReal sint2 = qrel.v.Magnitude();
      dReal theta = (qrel.v.Dot(axis) >= 0) ?	// @@@ padding assumptions
         (2 * (float)atan2(sint2,cost2)) :		// if u points in direction of axis
         (2 * (float)atan2(sint2,-cost2));		// if u points in opposite direction
   
                              // the angle we get will be between 0..2*pi, but we want to return angles
                              // between -pi..pi
      if(theta > PI)
         theta -= 2.0f*PI;
   
                                 //the angle we've just extracted has the wrong sign
      theta = -theta;
   
      return theta;
   }

//----------------------------

   virtual IPH_JOINT_TYPE GetType() const{ return IPHJOINTTYPE_HINGE; }

//----------------------------

   virtual void GetInfo1(Info1 *info){

      info->nub = 5;    
   
                                 //see if joint is powered
      if(limot.fmax > 0)
         info->m = 6;	         //powered hinge needs an extra constraint row
      else
         info->m = 5;
   
                                 //see if we're at a joint limit.
      if((limot.lostop >= -PI || limot.histop <= PI) && limot.lostop <= limot.histop){
         dReal angle = GetHingeAngle(node[0].body, node[1].body, axis1, qrel);
         if(limot.TestRotationalLimit (angle))
            info->m = 6;
      }
   }

//----------------------------

   virtual void GetInfo2(Info2 *info) const{

                                 //set the three ball-and-socket rows
      SetBall(info, anchor[0], anchor[1]);

     // set the two hinge rows. the hinge axis should be the only unconstrained
     // rotational axis, the angular velocity of the two bodies perpendicular to
     // the hinge axis should be equal. thus the constraint equations are
     //    p*w1 - p*w2 = 0
     //    q*w1 - q*w2 = 0
     // where p and q are unit vectors normal to the hinge axis, and w1 and w2
     // are the angular velocity vectors of the two bodies.

      S_vector p, q;  // plane space vectors for ax1
                              //length 1 joint axis in global coordinates, from 1st body
      S_vector ax1 = axis1 % node[0].body->GetMatrix();

      PlaneSpace(ax1, p, q);

      int s3 = 3*info->rowskip;
      int s4 = 4*info->rowskip;
   
      info->J1a[s3+0] = p[0];
      info->J1a[s3+1] = p[1];
      info->J1a[s3+2] = p[2];
      info->J1a[s4+0] = q[0];
      info->J1a[s4+1] = q[1];
      info->J1a[s4+2] = q[2];
   
      if(node[1].body){
         info->J2a[s3+0] = -p[0];
         info->J2a[s3+1] = -p[1];
         info->J2a[s3+2] = -p[2];
         info->J2a[s4+0] = -q[0];
         info->J2a[s4+1] = -q[1];
         info->J2a[s4+2] = -q[2];
      }
   
     // compute the right hand side of the constraint equation. set relative
     // body velocities along p and q to bring the hinge back into alignment.
     // if ax1,ax2 are the unit length hinge axes as computed from body1 and
     // body2, we need to rotate both bodies along the axis u = (ax1 x ax2).
     // if `theta' is the angle between ax1 and ax2, we need an angular velocity
     // along u to cover angle erp*theta in one step :
     //   |angular_velocity| = angle/time = erp*theta / stepsize
     //                      = (erp*fps) * theta
     //    angular_velocity  = |angular_velocity| * (ax1 x ax2) / |ax1 x ax2|
     //                      = (erp*fps) * theta * (ax1 x ax2) / sin(theta)
     // ...as ax1 and ax2 are unit length. if theta is smallish,
     // theta ~= sin(theta), so
     //    angular_velocity  = (erp*fps) * (ax1 x ax2)
     // ax1 x ax2 is in the plane space of ax1, so we project the angular
     // velocity to p and q to find the right hand side.
   
      S_vector ax2;
      if(node[1].body){
         ax2 = axis2 % node[1].body->GetMatrix();
      }else{
         ax2 = axis2;
      }
      S_vector b = ax1.Cross(ax2);
      dReal k = info->fps * info->erp;
      info->_c[3] = k * b.Dot(p);
      info->_c[4] = k * b.Dot(q);
   
                                 //if the hinge is powered, or has joint limits, add in the stuff
      limot.AddLimot(this, info, 5, ax1, true);
   }

//----------------------------

   virtual dxJointLimitMotor *GetLimot(dword index){
      switch(index){
      case 0: return &limot; break;
      }
      return NULL;
   }

//----------------------------
// Compute initial relative rotation body1 -> body2, or env -> body1.
   void ComputeInitialRelativeRotation(){

      if(node[0].body){
         if(node[1].body){
            qrel = node[0].body->GetRot() * ~node[1].body->GetRot();
         }else{
                                 //set joint->qrel to the transpose of the first body q
            qrel = node[0].body->GetRot();
         }
      }
   }

//----------------------------

   virtual bool SetAnchor(const S_vector &a){
      SetAnchors(a, anchor[0], anchor[1]);
      ComputeInitialRelativeRotation();
      return true;
   }

//----------------------------

   virtual bool SetLocalAnchor(dword bi, const S_vector &a){
      assert(bi<2);
      anchor[bi] = a;
      return true;
   }

//----------------------------

   virtual void SetAxis(const S_vector &a){
      SetAxes(a, &axis1, &axis2);
      ComputeInitialRelativeRotation();
   }

//----------------------------

   virtual float GetAngle() const{
      if(node[0].body)
         return GetHingeAngle(node[0].body, node[1].body, axis1, qrel);
      return 0.0f;
   }
   virtual float GetAngleRate() const{

      if(node[0].body){
         S_vector axis = axis1 % node[0].body->GetMatrix();
         float rate = axis.Dot(node[0].body->GetAVel());
         if(node[1].body)
            rate -= axis.Dot(node[1].body->GetAVel());
         return rate;
      }
      return 0.0f;
   }

public:
   IPH_joint_hinge(PIPH_world w):
      IPH_joint(w)
   {
      for(int i=2; i--; ){
         anchor[i].Zero();
      }
      axis1.Zero();
      axis1[0] = 1;
      axis2.Zero();
      axis2[0] = 1;
      qrel.Identity();
      limot.Init(w);
   }
};

//----------------------------

PIPH_joint CreateJointHinge(PIPH_world w){
   return new IPH_joint_hinge(w);
}

//----------------------------
//----------------------------

// macro that computes ax1,ax2 = axis 1 and 2 in global coordinates (they are
// relative to body 1 and 2 initially) and then computes the constrained
// rotational axis as the cross product of ax1 and ax2.
// the sin and cos of the angle between axis 1 and 2 is computed, this comes
// from dot and cross product rules.

#define HINGE2_GET_AXIS_INFO(_axis, sin_angle,cos_angle) \
   S_vector ax1 = axis[0] % node[0].body->GetMatrix(); \
   S_vector ax2 = axis[1] % node[1].body->GetMatrix(); \
   _axis = ax1.Cross(ax2); \
   sin_angle = _axis.Magnitude(); \
   cos_angle = ax1.Dot(ax2);


class IPH_joint_hinge2: public IPH_joint{

   S_vector anchor[2];
   S_vector axis[2];
   dReal c0, s0;         // cos,sin of desired angle between axis 1,2
   S_vector v1, v2;     // angle ref vectors embedded in first body
   dxJointLimitMotor limot1;  // limit+motor info for axis 1
   dxJointLimitMotor limot2;  // limit+motor info for axis 2
   dReal susp_erp, susp_cfm;   // suspension parameters (erp,cfm)

//----------------------------

   float MeasureAngle() const{

      S_vector a1 = axis[1] % node[1].body->GetMatrix();
      S_vector a2 = a1 % node[0].body->GetInvMatrix();
      dReal x = v1.Dot(a2);
      dReal y = v2.Dot(a2);
      return -(float)atan2(y, x);
   }

//----------------------------
// compute vectors v1 and v2 (embedded in body1), used to measure angle
// between body 1 and body 2
   void MakeV1andV2(){

      if(node[0].body){
                                 //get axis 1 and 2 in global coords
         S_vector ax1 = axis[0] % node[0].body->GetMatrix();
         S_vector ax2 = axis[1] % node[1].body->GetMatrix();
      
                                 //don't do anything if the axis[0] or axis[1] vectors are zero or the same
         if((ax1[0]==0 && ax1[1]==0 && ax1[2]==0) ||
            (ax2[0]==0 && ax2[1]==0 && ax2[2]==0) ||
            (ax1[0]==ax2[0] && ax1[1]==ax2[1] && ax1[2]==ax2[2]))
            return;
      
                                 //modify axis 2 so it's perpendicular to axis 1
         dReal k = ax1.Dot(ax2);
         ax2 -= ax1 * k;
         ax2.Normalize();
      
                                 //make v1 = modified axis[1], v2 = axis[0] x (modified axis[1])
         S_vector v = ax1.Cross(ax2);
         const S_matrix mt = node[0].body->GetInvMatrix();
         v1 = ax2 % mt;
         v2 = v % mt;
      }
   }

//----------------------------

   virtual IPH_JOINT_TYPE GetType() const{ return IPHJOINTTYPE_HINGE2; }

//----------------------------

   virtual dxJointLimitMotor *GetLimot(dword index){
      switch(index){
      case 0: return &limot1; break;
      case 1: return &limot2; break;
      }
      return NULL;
   }

//----------------------------

   virtual bool SetAnchor(const S_vector &a){
      SetAnchors(a, anchor[0], anchor[1]);
      MakeV1andV2();
      return true;
   }

//----------------------------

   virtual bool SetLocalAnchor(dword bi, const S_vector &a){
      assert(bi<2);
      anchor[bi] = a;
      return true;
   }

//----------------------------
   virtual void SetAxis(dword index, const S_vector &a){

      assert(index < 2);

      if(node[index].body){
         S_normal n_a = a;
         axis[index] = n_a % node[index].body->GetInvMatrix();
      
                                 //compute the sin and cos of the angle between axis 1 and axis 2
         S_vector ax;
         HINGE2_GET_AXIS_INFO(ax, s0, c0);
      }
      MakeV1andV2();
   }

//----------------------------
   virtual float GetAngle() const{
      if(node[0].body)
         return MeasureAngle();
      return 0.0f;
   }

//----------------------------

   virtual float GetAngleRate(dword index) const{

      assert(index < 2);

      if(node[0].body){
         if(!index){
            S_vector _axis = axis[0] % node[0].body->GetMatrix();
            dReal rate = _axis.Dot(node[0].body->GetAVel());
            if(node[1].body)
               rate -= _axis.Dot(node[1].body->GetAVel());
            return rate;
         }else{
            if(node[1].body){
               S_vector _axis = axis[1] % node[1].body->GetMatrix();
               dReal rate = _axis.Dot(node[0].body->GetAVel());
               if(node[1].body)
                  rate -= _axis.Dot(node[1].body->GetAVel());
               return rate;
            }
         }
      }
      return 0.0f;
   }

//----------------------------

   virtual bool SetSuspensionERP(float val, dword axis){
      if(axis) return false;
      susp_erp = val;
      return true;
   }
   virtual bool SetSuspensionCFM(float val, dword axis){
      if(axis) return false;
      susp_cfm = val;
      return true;
   }

//----------------------------

   virtual void GetInfo1(Info1 *info){

      info->m = 4;
      info->nub = 4;

                              //see if we're powered or at a joint limit for axis 1
      int atlimit=0;
      if((limot1.lostop >= -PI || limot1.histop <= PI) && limot1.lostop <= limot1.histop){
         dReal angle = MeasureAngle();
         if(limot1.TestRotationalLimit (angle))
            atlimit = 1;
      }
      if(atlimit || limot1.fmax > 0)
         info->m++;
   
                              //see if we're powering axis 2 (we currently never limit this axis)
      limot2.limit = 0;
      if (limot2.fmax > 0) info->m++;
   }

//----------------------------

   virtual void GetInfo2(Info2 *info) const{

                              //get information we need to set the hinge row
      dReal s,c;
      S_vector q;
      HINGE2_GET_AXIS_INFO(q, s, c);
                              //quicker: divide q by s ?
      q.Normalize();

                              //set the three ball-and-socket rows (aligned to the suspension axis ax1)
      SetBall2(info, anchor[0], anchor[1], (S_vector&)ax1, susp_erp);

                              //set the hinge row
      int s3 = 3*info->rowskip;
      info->J1a[s3+0] = q[0];
      info->J1a[s3+1] = q[1];
      info->J1a[s3+2] = q[2];
      if (node[1].body) {
         info->J2a[s3+0] = -q[0];
         info->J2a[s3+1] = -q[1];
         info->J2a[s3+2] = -q[2];
      }
   
     // compute the right hand side for the constrained rotational DOF.
     // axis 1 and axis 2 are separated by an angle `theta'. the desired
     // separation angle is theta0. sin(theta0) and cos(theta0) are recorded
     // in the joint structure. the correcting angular velocity is:
     //   |angular_velocity| = angle/time = erp*(theta0-theta) / stepsize
     //                      = (erp*fps) * (theta0-theta)
     // (theta0-theta) can be computed using the following small-angle-difference
     // approximation:
     //   theta0-theta ~= tan(theta0-theta)
     //                 = sin(theta0-theta)/cos(theta0-theta)
     //                 = (c*s0 - s*c0) / (c*c0 + s*s0)
     //                 = c*s0 - s*c0         assuming c*c0 + s*s0 ~= 1
     // where c = cos(theta), s = sin(theta)
     //       c0 = cos(theta0), s0 = sin(theta0)

      dReal k = info->fps * info->erp;
      info->_c[3] = k * (c0 * s - s0 * c);

                              //if the axis[0] hinge is powered, or has joint limits, add in more stuff
      int row = 4 + (int)limot1.AddLimot(this, info, 4, ax1, true);

                              //if the axis2 hinge is powered, add in more stuff
      limot2.AddLimot(this, info, row, ax2, true);

                              //set parameter for the suspension
      info->cfm[0] = susp_cfm;
   }

//----------------------------
public:
   IPH_joint_hinge2(PIPH_world w):
      IPH_joint(w)
   {
      for(dword i=2; i--; ){
         anchor[i].Zero();
         axis[i].Zero();
         axis[i][i] = 1;
      }
      c0 = 0;
      s0 = 0;
   
      v1.Zero();
      v1[0] = 1;
      v2.Zero();
      v2[1] = 1;
   
      limot1.Init(w);
      limot2.Init(w);
   
      susp_erp = w->GetERP();
      susp_cfm = w->GetCFM();
   
      flags |= JF_TWOBODIES;
   }
};

//----------------------------

PIPH_joint CreateJointHinge2(PIPH_world w){

   return new IPH_joint_hinge2(w);
}

//----------------------------
//----------------------------
// dxJointLimitMotor

void IPH_joint::dxJointLimitMotor::Init(PIPH_world world){

   vel = 0;
   fmax = 0;
   lostop = -dInfinity;
   histop = dInfinity;
   fudge_factor = 1;
   normal_cfm = world->GetCFM();
   stop_erp = world->GetERP();
   stop_cfm = world->GetCFM();
   bounce = 0;
   limit = 0;
   limit_err = 0;
}

//----------------------------

void IPH_joint::dxJointLimitMotor::Set(int num, dReal value){

   switch(num){
   case dParamLoStop:
      if(value <= histop)
         lostop = value;
      break;
   case dParamHiStop:
      if(value >= lostop)
         histop = value;
      break;
   case dParamVel:
      vel = value;
      break;
   case dParamFMax:
      if(value >= 0.0f)
         fmax = value;
      break;
   case dParamFudgeFactor:
      if(value >= 0.0f && value <= 1.0f)
         fudge_factor = value;
      break;
   case dParamBounce:
      bounce = value;
      break;
   case dParamCFM:
      normal_cfm = value;
      break;
   case dParamStopERP:
      stop_erp = value;
      break;
   case dParamStopCFM:
      stop_cfm = value;
      break;
   }
}

//----------------------------

dReal IPH_joint::dxJointLimitMotor::Get(int num) const{
   switch(num){
   case dParamLoStop: return lostop;
   case dParamHiStop: return histop;
   case dParamVel: return vel;
   case dParamFMax: return fmax;
   case dParamFudgeFactor: return fudge_factor;
   case dParamBounce: return bounce;
   case dParamCFM: return normal_cfm;
   case dParamStopERP: return stop_erp;
   case dParamStopCFM: return stop_cfm;
   default: return 0;
   }
}

//----------------------------

bool IPH_joint::dxJointLimitMotor::TestRotationalLimit(dReal angle){

   if(angle <= lostop){
      limit = 1;
      limit_err = angle - lostop;
      return true;
   }else
   if(angle >= histop){
      limit = 2;
      limit_err = angle - histop;
      return true;
   }else{
      limit = 0;
      return false;
   }
}

//----------------------------

bool IPH_joint::dxJointLimitMotor::AddLimot(CPIPH_joint joint, Info2 *info, int row, const S_vector &ax1, bool rotational) const{

   int srow = row * info->rowskip;
   
   // if the joint is powered, or has joint limits, add in the extra row
   int powered = fmax > 0;
   if(powered || limit){
      dReal *J1 = rotational ? info->J1a : info->J1l;
      dReal *J2 = rotational ? info->J2a : info->J2l;
      
      J1[srow+0] = ax1[0];
      J1[srow+1] = ax1[1];
      J1[srow+2] = ax1[2];
      if(joint->node[1].body){
         J2[srow+0] = -ax1[0];
         J2[srow+1] = -ax1[1];
         J2[srow+2] = -ax1[2];
      }
      
                              // linear limot torque decoupling step:
                              //
                              // if this is a linear limot (e.g. from a slider), we have to be careful
                              // that the linear constraint forces (+/- ax1) applied to the two bodies
                              // do not create a torque couple. in other words, the points that the
                              // constraint force is applied at must lie along the same ax1 axis.
                              // a torque couple will result in powered or limited slider-jointed free
                              // bodies from gaining angular momentum.
                              // the solution used here is to apply the constraint forces at the point
                              // halfway between the body centers. there is no penalty (other than an
                              // extra tiny bit of computation) in doing this adjustment. note that we
                              // only need to do this if the constraint connects two bodies.
      
      S_vector ltd;	         // Linear Torque Decoupling vector (a torque)
      if(!rotational && joint->node[1].body){
         S_vector c = (joint->node[1].body->GetPos() - joint->node[0].body->GetPos()) * .5f;
         ltd = c.Cross(ax1);
         info->J1a[srow+0] = ltd[0];
         info->J1a[srow+1] = ltd[1];
         info->J1a[srow+2] = ltd[2];
         info->J2a[srow+0] = ltd[0];
         info->J2a[srow+1] = ltd[1];
         info->J2a[srow+2] = ltd[2];
      }
      
                              // if we're limited low and high simultaneously, the joint motor is
                              // ineffective
      if(limit && (lostop == histop))
         powered = 0;
      
      if(powered){
         info->cfm[row] = normal_cfm;
         if(!limit){
            info->_c[row] = vel;
            info->lo[row] = -fmax;
            info->hi[row] = fmax;
         }else{
            // the joint is at a limit, AND is being powered. if the joint is
            // being powered into the limit then we apply the maximum motor force
            // in that direction, because the motor is working against the
            // immovable limit. if the joint is being powered away from the limit
            // then we have problems because actually we need *two* lcp
            // constraints to handle this case. so we fake it and apply some
            // fraction of the maximum force. the fraction to use can be set as
            // a fudge factor.
            
            dReal fm = fmax;
            if(vel > 0)
               fm = -fm;
            
                              //if we're powering away from the limit, apply the fudge factor
            if((limit==1 && vel > 0) || (limit==2 && vel < 0))
               fm *= fudge_factor;
            
            if(rotational){
               joint->node[0].body->AddTorque(S_vector(-fm*ax1[0],-fm*ax1[1], -fm*ax1[2]));
               if(joint->node[1].body)
                  joint->node[1].body->AddTorque(S_vector(fm*ax1[0],fm*ax1[1],fm*ax1[2]));
            }else{
               joint->node[0].body->AddForce(S_vector(-fm*ax1[0], -fm*ax1[1], -fm*ax1[2]));
               if(joint->node[1].body){
                  joint->node[1].body->AddForce(S_vector(fm*ax1[0],fm*ax1[1],fm*ax1[2]));
                  
                              //linear limot torque decoupling step: refer to above discussion
                  joint->node[0].body->AddTorque(S_vector(-fm*ltd[0],-fm*ltd[1], -fm*ltd[2]));
                  joint->node[1].body->AddTorque(S_vector(-fm*ltd[0],-fm*ltd[1], -fm*ltd[2]));
               }
            }
         }
      }
      
      if(limit){
         dReal k = info->fps * stop_erp;
         info->_c[row] = -k * limit_err;
         info->cfm[row] = stop_cfm;
         
         if(lostop == histop){
                              //limited low and high simultaneously
            info->lo[row] = -dInfinity;
            info->hi[row] = dInfinity;
         }else{
            if(limit == 1){
                              //low limit
               info->lo[row] = 0;
               info->hi[row] = dInfinity;
            }else{
                              //high limit
               info->lo[row] = -dInfinity;
               info->hi[row] = 0;
            }
            
                              //deal with bounce
            if(bounce > 0){
                              //calculate joint velocity
               dReal vel;
               if(rotational){
                  vel = joint->node[0].body->GetAVel().Dot(ax1);
                  if(joint->node[1].body)
                     vel -= joint->node[1].body->GetAVel().Dot(ax1);
               }else{
                  vel = joint->node[0].body->GetLVel().Dot(ax1);
                  if(joint->node[1].body)
                     vel -= joint->node[1].body->GetLVel().Dot(ax1);
               }
               
                              //only apply bounce if the velocity is incoming, and if the
                              //resulting c[] exceeds what we already have.
               if(limit == 1){
                              //low limit
                  if(vel < 0){
                     dReal newc = -bounce * vel;
                     if(newc > info->_c[row])
                        info->_c[row] = newc;
                  }
               }else{
                              //high limit - all those computations are reversed
                  if(vel > 0){
                     dReal newc = -bounce * vel;
                     if(newc < info->_c[row])
                        info->_c[row] = newc;
                  }
               }
            }
         }
      }
      return true;
   }
   return false;
}

//----------------------------
                              //contact

class IPH_joint_contact: public IPH_joint{
   int the_m;			// number of rows computed by getInfo1

   virtual IPH_JOINT_TYPE GetType() const{ return IPHJOINTTYPE_CONTACT; }

//----------------------------

   virtual void GetInfo1(Info1 *info){
                                 //make sure mu's >= 0, then calculate number of constraint rows and number
                                 // of unbounded rows
      int m = 1, nub=0;
      if(contact.surface.mu < 0)
         contact.surface.mu = 0;
      if(contact.surface.mode & dContactMu2){
         if(contact.surface.mu > 0) m++;
         if(contact.surface.mu2 < 0) contact.surface.mu2 = 0;
         if(contact.surface.mu2 > 0) m++;
         if(contact.surface.mu  == dInfinity) nub ++;
         if(contact.surface.mu2 == dInfinity) nub ++;
      }else{
         if(contact.surface.mu > 0)
            m += 2;
         if(contact.surface.mu == dInfinity)
            nub += 2;
      }
   
      the_m = m;
      info->m = m;
      info->nub = nub;
   }

//----------------------------

   void GetInfo2(Info2 *info) const{

      int s = info->rowskip;
      int s2 = 2*s;
   
                              //get normal, with sign adjusted for body1/body2 polarity
      S_vector normal;
      if(flags & JF_REVERSE){
         normal = contact.geom.normal;
      }else{
         normal = -contact.geom.normal;
      }
   
                              //c1, c2 = contact points with respect to body PORs
      S_vector c2(0, 0, 0);
      S_vector c1 = contact.geom.pos - node[0].body->GetPos();
   
                              //set jacobian for normal
      info->J1l[0] = normal[0];
      info->J1l[1] = normal[1];
      info->J1l[2] = normal[2];
      *(S_vector*)info->J1a = c1.Cross(normal);
      if(node[1].body){
         c2 = contact.geom.pos - node[1].body->GetPos();
         info->J2l[0] = -normal[0];
         info->J2l[1] = -normal[1];
         info->J2l[2] = -normal[2];
         *(S_vector*)info->J2a = -c2.Cross(normal);
      }
   
                              //set right hand side and cfm value for normal
      dReal erp = info->erp;
      if(contact.surface.mode&dContactSoftERP)
         erp = contact.surface.soft_erp;
      dReal k = info->fps * erp;
      info->_c[0] = k * contact.geom.depth;
      if(contact.surface.mode & dContactSoftCFM)
         info->cfm[0] = contact.surface.soft_cfm;
   
                              //deal with bounce
      if(contact.surface.mode&dContactBounce){
                              //calculate outgoing velocity (-ve for incoming contact)
         dReal outgoing = ((S_vector*)info->J1l)->Dot(node[0].body->GetLVel()) + ((S_vector*)info->J1a)->Dot(node[0].body->GetAVel());
         if(node[1].body){
            outgoing += ((S_vector*)info->J2l)->Dot(node[1].body->GetLVel()) + ((S_vector*)info->J2a)->Dot(node[1].body->GetAVel());
         }
                              // only apply bounce if the outgoing velocity is greater than the
                              // threshold, and if the resulting c[0] exceeds what we already have.
         if(contact.surface.bounce_vel >= 0 && (-outgoing) > contact.surface.bounce_vel){

            dReal newc = - contact.surface.bounce * outgoing;
            if(newc > info->_c[0])
               info->_c[0] = newc;
         }
      }
   
                              //set LCP limits for normal
      info->lo[0] = 0;
      info->hi[0] = dInfinity;
   
                              //now do jacobian for tangential forces
      S_vector t1, t2;	      // wo vectors tangential to normal
   
                              //first friction direction
      if(the_m >= 2){
         if(contact.surface.mode&dContactFDir1) {	// use fdir1 
            t1[0] = contact.fdir1[0];
            t1[1] = contact.fdir1[1];
            t1[2] = contact.fdir1[2];
            t2 = normal.Cross(t1);
         }else{
            PlaneSpace(normal, t1, t2);
         }
         info->J1l[s+0] = t1[0];
         info->J1l[s+1] = t1[1];
         info->J1l[s+2] = t1[2];
         *(S_vector*)(info->J1a+s) = c1.Cross(t1);
         if(node[1].body) {
            info->J2l[s+0] = -t1[0];
            info->J2l[s+1] = -t1[1];
            info->J2l[s+2] = -t1[2];
            *(S_vector*)(info->J2a+s) = -c2.Cross(t1);
         }
                              //set right hand side
         if(contact.surface.mode&dContactMotion1){
            info->_c[1] = contact.surface.motion1;
         }
                              // set LCP bounds and friction index. this depends on the approximation
                              // mode
         info->lo[1] = -contact.surface.mu;
         info->hi[1] = contact.surface.mu;
         if(contact.surface.mode&dContactApprox1_1)
            info->findex[1] = 0;
      
                              //set slip (constraint force mixing)
         if(contact.surface.mode&dContactSlip1)
            info->cfm[1] = contact.surface.slip1;
   
                              //second friction direction
         if(the_m >= 3){
            info->J1l[s2+0] = t2[0];
            info->J1l[s2+1] = t2[1];
            info->J1l[s2+2] = t2[2];
            *(S_vector*)(info->J1a+s2) = c1.Cross(t2);
            if(node[1].body){
               info->J2l[s2+0] = -t2[0];
               info->J2l[s2+1] = -t2[1];
               info->J2l[s2+2] = -t2[2];
               *(S_vector*)(info->J2a+s2) = -c2.Cross(t2);
            }
                              //set right hand side
            if(contact.surface.mode&dContactMotion2){
               info->_c[2] = contact.surface.motion2;
            }
                              //set LCP bounds and friction index. this depends on the approximation
                              // mode
            if(contact.surface.mode&dContactMu2){
               info->lo[2] = -contact.surface.mu2;
               info->hi[2] = contact.surface.mu2;
            }else{
               info->lo[2] = -contact.surface.mu;
               info->hi[2] = contact.surface.mu;
            }
            if(contact.surface.mode&dContactApprox1_2)
               info->findex[2] = 0;
      
                              //set slip (constraint force mixing)
            if(contact.surface.mode&dContactSlip2)
               info->cfm[2] = contact.surface.slip2;
         }
      }
   }

//----------------------------
public:
   dContact contact;
   IPH_joint_contact(PIPH_world w, const dContact *c):
      IPH_joint(w),
      contact(*c)
   {
   }
};

//----------------------------

PIPH_joint CreateJointContact(PIPH_world w, dxJointGroup &jg, const dContact *c){

   return new(jg.stack.Alloc(sizeof(IPH_joint_contact)))IPH_joint_contact(w, c);
}

//----------------------------
//----------------------------

//****************************************************************************
// hinge 2. note that this joint must be attached to two bodies for it to work

/*
void dJointGetHinge2Axis1 (PIPH_joint j, S_vector &result){

   dxJointHinge2 *joint = (dxJointHinge2*)j;

  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
   if (joint->node[0].body) {
      //dMULTIPLY0_331 (result, joint->node[0].body->GetRotMatrix(), &joint->axis1.x);
      result = joint->axis1 % joint->node[0].body->GetMatrix();
  }
}

//----------------------------

void dJointGetHinge2Axis2(PIPH_joint j, S_vector &result){

   dxJointHinge2 *joint = (dxJointHinge2*)j;

  dUASSERT(joint,"bad joint argument");
  dUASSERT(result,"bad result argument");
   if (joint->node[1].body) {
      //dMULTIPLY0_331 (result,joint->node[1].body->GetRotMatrix(), &joint->axis2.x);
      result = joint->axis2 % joint->node[1].body->GetMatrix();
   }
}
*/

//****************************************************************************
// angular motor


//----------------------------


/*
void dJointGetAMotorAxis(PIPH_joint j, int anum, S_vector &result){

   dxJointAMotor *joint = (dxJointAMotor*)j;

  assert(joint && anum >= 0 && anum < 3);
  if (anum < 0) anum = 0;
  if (anum > 2) anum = 2;
  if (joint->rel[anum] > 0) {
      if (joint->rel[anum]==1) {
         //dMULTIPLY0_331 (result,joint->node[0].body->GetRotMatrix(), &joint->axis[anum].x);
         result = joint->axis[anum] % joint->node[0].body->GetMatrix();
      }else{
         //dMULTIPLY0_331 (result,joint->node[1].body->GetRotMatrix(), &joint->axis[anum].x);
         result = joint->axis[anum] % joint->node[1].body->GetMatrix();
      }
  }
  else {
    result[0] = joint->axis[anum][0];
    result[1] = joint->axis[anum][1];
    result[2] = joint->axis[anum][2];
  }
}

//----------------------------

int dJointGetAMotorAxisRel(PIPH_joint j, int anum){

   dxJointAMotor *joint = (dxJointAMotor*)j;
   
   assert(joint && anum >= 0 && anum < 3);
   if (anum < 0) anum = 0;
   if (anum > 2) anum = 2;
   return joint->rel[anum];
}
*/

//----------------------------
//----------------------------

void dxJointGroup::Empty(){

  // the joints in this group are detached starting from the most recently
  // added (at the top of the stack). this helps ensure that the various
  // linked lists are not traversed too much, as the joints will hopefully
  // be at the start of those lists.
  // if any group joints have their world pointer set to 0, their world was
  // previously destroyed. no special handling is required for these joints.

   int i;
   PIPH_joint *jlist = (PIPH_joint*)alloca(num * sizeof(PIPH_joint));
   PIPH_joint j = (PIPH_joint)stack.Rewind();
   for(i=0; i < num; i++){
      jlist[i] = j;
      switch(j->GetType()){
      case IPHJOINTTYPE_CONTACT: j = (PIPH_joint)stack.Next(sizeof(IPH_joint_contact)); break;
      default: assert(0);
      }
   }
   for(i=num-1; i >= 0; i--)
      jlist[i]->~IPH_joint();
   num = 0;
   stack.FreeAll();
}

//----------------------------
