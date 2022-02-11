#ifndef _IPH_JOINT_H_
#define _IPH_JOINT_H_

#include "common.h"
#include <C_mem_stack.h>

//----------------------------

// there are two of these nodes in the joint, one for each connection to a
// body. these are node of a linked list kept by each body of it's connecting
// joints. but note that the body pointer in each node points to the body that
// makes use of the *other* node, not this node. this trick makes it a bit
// easier to traverse the body/joint graph.

struct dxJointNode{
   PIPH_joint joint;          //pointer to enclosing IPH_joint object
   PIPH_body body;            //*other* body this joint is connected to
   dxJointNode *next;         //next node in body's list of connected joints
};

//----------------------------

enum{
                              //parameters for limits and motors
   dParamLoStop = 0,
   dParamHiStop,
   dParamVel,
   dParamFMax,
   dParamFudgeFactor,
   dParamBounce,
   dParamCFM,
   dParamStopERP,
   dParamStopCFM,
                              //parameters for suspension
   dParamSuspensionERP,
   dParamSuspensionCFM,
};

//----------------------------

class IPH_joint: public C_link_list_element<IPH_joint>{
protected:
   dword user_data;
   C_str name;
   PIPH_world world;          //world this object is in
   friend class IPH_world_imp;
   friend class dxJointGroup;

   REF_COUTNER
protected:

//----------------------------
   int tag;                   //used by dynamics algorithms
                              //joint flags
   enum{
                              //if this flag is set, the joint was allocated in a joint group
      JF_INGROUP = 1,
                              // if this flag is set, the joint was attached with arguments (0,body).
                              // our convention is to treat all attaches as (body,0), i.e. so node[0].body
                              // is always nonzero, so this flag records the fact that the arguments were
                              // swapped.
      JF_REVERSE = 2,
                              // if this flag is set, the joint can not have just one body attached to it,
                              // it must have either zero or two bodies attached.
      JF_TWOBODIES = 4
   };

//----------------------------

   void CheckRange(float val, float min, float max, const char *fnc_name){
      if(val < min || val > max)
         throw C_except(C_xstr("%: value out of range: #.2% (allowed range is #.2% ... #.2%)") % fnc_name % val % min % max);
   }

//----------------------------

   bool SetLimotParam(int param, dword axis, float val){
      IPH_joint::dxJointLimitMotor *limot = GetLimot(axis);
      if(!limot)
         return false;
      limot->Set(param, val);
      return false;
   }

public:
   IPH_joint(PIPH_world w);
   ~IPH_joint();
public:

//----------------------------
   virtual IPH_JOINT_TYPE GetType() const = 0;

//----------------------------
   virtual void SetUserData(dword data){ user_data = data; }
   virtual dword GetUserData() const{ return user_data; }

//----------------------------
   virtual void Attach(PIPH_body b1, PIPH_body b2);

//----------------------------
   virtual PIPH_body GetBody(dword index) const{
      return GetBody1(index);
   }

//----------------------------
   virtual bool SetAnchor(const S_vector &a){ return false; }
   //virtual const S_vector &GetAnchor() const{ static const S_vector zero(0, 0, 0); return zero; }
   virtual bool SetLocalAnchor(dword body_index, const S_vector &a){ return false; }

//----------------------------

   virtual bool SetLoStop(float val, dword axis){ return SetLimotParam(dParamLoStop, axis, val); }
   virtual bool SetHiStop(float val, dword axis){ return SetLimotParam(dParamHiStop, axis, val); }
   virtual bool SetDesiredVelocity(float val, dword axis){ return SetLimotParam(dParamVel, axis, val); }
   virtual bool SetMaxForce(float val, dword axis){ return SetLimotParam(dParamFMax, axis, val); }
   virtual bool SetFudgeFactor(float val, dword axis){ return SetLimotParam(dParamFudgeFactor, axis, val); }
   virtual bool SetBounce(float val, dword axis){ CheckRange(val, 0, 1, "SetBounce"); return SetLimotParam(dParamBounce, axis, val); }
   virtual bool SetCFM(float val, dword axis){ return SetLimotParam(dParamCFM, axis, val); }
   virtual bool SetStopERP(float val, dword axis){ CheckRange(val, 0, 1, "SetStopERP"); return SetLimotParam(dParamStopERP, axis, val); }
   virtual bool SetStopCFM(float val, dword axis){ return SetLimotParam(dParamStopCFM, axis, val); }
   virtual bool SetSuspensionERP(float val, dword axis){ return false; }
   virtual bool SetSuspensionCFM(float val, dword axis){ return false; }

//----------------------------
   virtual void SetName(const C_str &n){ name = n; }
   virtual const C_str &GetName() const{ return name; }

//----------------------------

  // naming convention: the "first" body this is connected to is node[0].body,
  // and the "second" body is node[1].body. if this joint is only connected
  // to one body then the second body is NULL

//----------------------------
                              //info returned by getInfo1 function
   struct Info1{
                              //the constraint dimension (<=6), i.e. that is the total number of rows in the jacobian
      int m;
                              //number of unbounded variables (which have lo,hi = -/+ infinity).
      int nub;
   };
   
//----------------------------
                              //info returned by getInfo2 function
   struct Info2{
                              //integrator parameters: frames per second (1/stepsize)
      float fps;
                              //default error reduction parameter (0..1).
      float erp;
      
                              //for the first and second body, pointers to two (linear and angular)
                              //n*3 jacobian sub matrices, stored by rows. these matrices will have
                              //been initialized to 0 on entry. if the second body is zero then the
                              //J2xx pointers may be 0.
      float *J1l, *J1a, *J2l, *J2a;
      
                              //elements to jump from one row to the next in J's
      //int rowskip;
      enum{ rowskip = 8};
      
                              //right hand sides of the equation J*v = c + cfm * lambda
                              //cfm is the "constraint-force-mixing" vector, it is set to a constant value (typically very small or zero) value on entry
      float *cfm;
                              //c is set to zero on entry
      float *_c;
      
                              //lo and hi limits for variables (set to -/+ infinity on entry).
      float *lo, *hi;
      
                              //findex vector for variables. see the LCP solver interface for a
                              //description of what this does. this is set to -1 on entry.
                              //note that the returned indexes are relative to the first index of
                              //the constraint
      int *findex;
   };

//----------------------------
// common limit and motor information for a single joint axis of movement
   class dxJointLimitMotor{
   public:
      dReal vel,fmax;         //powered joint: velocity, max force
      dReal lostop,histop;    //joint limits, relative to initial position
      dReal fudge_factor;     //when powering away from joint limits
      dReal normal_cfm;       //cfm to use when not at a stop
      dReal stop_erp,stop_cfm;//erp and cfm for when at joint limit
      dReal bounce;           //restitution factor
      // variables used between getInfo1() and getInfo2()
      int limit;              //0=free, 1=at lo limit, 2=at hi limit
      dReal limit_err;        //if at limit, amount over limit
   
      void Init(PIPH_world);

      void Set(int num, dReal value);

      dReal Get(int num) const;
      bool TestRotationalLimit (dReal angle);
      bool AddLimot(CPIPH_joint joint, Info2 *info, int row, const S_vector &ax1, bool rotational) const;
   };

//----------------------------
// Set three "ball-and-socket" rows in the constraint equation, and the corresponding right hand side.
   void SetBall(Info2 *info, const S_vector &anchor1, const S_vector &anchor2) const;

//----------------------------
// This is like SetBall(), except that 'axis' is a unit length vector (in global coordinates) that should be used 
// for the first jacobian position row (the other two row vectors will be derived from this).
// 'erp1' is the erp value to use along the axis.
   void SetBall2(Info2 *info, const S_vector &anchor1, const S_vector &anchor2, const S_vector &axis, dReal erp1) const;

//----------------------------
// Compute anchor points relative to bodies.
   void SetAnchors(const S_vector &ap, S_vector &anchor1, S_vector &anchor2) const;

//----------------------------

   void GetAnchor1(S_vector &result, const S_vector &anchor1) const;

//----------------------------
// Compute axes relative to bodies. axis2 can be 0.
   void SetAxes(const S_vector &ax, S_vector *axis1, S_vector *axis2) const;

   void GetAxis(S_vector &result, const S_vector &axis1) const;

//----------------------------
// Remove the joint from neighbour lists of all connected bodies.
   void RemoveJointReferencesFromAttachedBodies();

//----------------------------

   dword flags;               //JF_xxx flags

   dxJointNode node[2];       //connections to bodies. node[1].body can be 0

                              //joint force feedback information
   struct dJointFeedback{
      S_vector f1;		// force applied to body 1
      S_vector t1;		// torque applied to body 1
      S_vector f2;		// force applied to body 2
      S_vector t2;		// torque applied to body 2
   };
   dJointFeedback *feedback;	//optional feedback structure

//----------------------------

   PIPH_body GetBody1(int index) const{
      if(index >= 0 && index < 2)
         return node[index].body;
      return NULL;
   }

//----------------------------

   void SetFeedback(dJointFeedback *f){ feedback = f; }
   dJointFeedback *GetFeedback(){ return feedback; }

//----------------------------

   virtual void GetInfo1(Info1 *info) = 0;
   virtual void GetInfo2(Info2 *info) const = 0;
   virtual class dxJointLimitMotor *GetLimot(dword index){ return NULL; }

   virtual void Reserved3(){}
   virtual void Reserved4(){}
   virtual void Reserved5(){}
   virtual void Reserved6(){}
   virtual void Reserved7(){}
   virtual void Reserved8(){}
   virtual void Reserved9(){}
};

//----------------------------

// joint group. NOTE: any joints in the group that have their world destroyed
// will have their world pointer set to 0.

class dxJointGroup{
public:
   int num;                   //number of joints on the stack
   C_mem_stack stack;         //a stack of (possibly differently sized) IPH_joint objects

   dxJointGroup():
      num(0)
   {}
   ~dxJointGroup(){
      Empty();
   }

   void Empty();
};

//----------------------------
//----------------------------

#endif
