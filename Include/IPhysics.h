#ifndef __IPHYSICS_H_
#define __IPHYSICS_H_

#include <i3d\i3d_math.h>
#include <i3d\i3d2.h>
#include <C_buffer.h>

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Physics system header file.
//----------------------------

enum{
   IPH_STEFRAME_USE_TRANSFORM = 1,  //use frame's pos/rot for initial body pos/rot
   IPH_STEFRAME_HR_VOLUMES = 2,     //use all frames volumes, not only direct children
   IPH_STEFRAME_OFF_VOLUMES = 4,    //use also volumes which are not on
   IPH_STEFRAME_DENSITY_AS_WEIGHT = 8,//use density parameter as weight of object
};

#ifndef PHYS_INTERNAL
class IPH_body{
protected:
   virtual ~IPH_body() = 0;
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

//----------------------------
   virtual void SetPosRot(const S_vector &pos, const S_quat &rot) = 0;
   virtual void GetPosRot(S_vector &pos, S_quat &rot) const = 0;

//----------------------------
   virtual void AddForceAtPos(const S_vector &pos, const S_vector &dir) = 0;

//----------------------------
   virtual S_vector GetVelAtPos(const S_vector &pos) const = 0;

//----------------------------
// Check if this body is connected with other body through joint.
   virtual bool IsConnected(const IPH_body *body) const = 0;

//----------------------------
// Set current frame. The method returns false if frame cannot be set (e.g. the total mass is zero).
   virtual bool SetFrame(PI3D_frame frm, dword flags = 0, float density = 1000.0f) = 0;
   virtual PI3D_frame GetFrame() = 0;
   virtual CPI3D_frame GetFrame() const = 0;

//----------------------------
   virtual void Enable(bool) = 0;
   virtual bool IsEnabled() const = 0;

//----------------------------
   virtual void SetUserData(dword data) = 0;
   virtual dword GetUserData() const = 0;

//----------------------------
   virtual void SetLinearVelocity(const S_vector &v) = 0;
   virtual const S_vector &GetLinearVelocity() const = 0;

//----------------------------
   virtual void SetAngularVelocity(const S_vector &v) = 0;
   virtual const S_vector &GetAngularVelocity() const = 0;

//----------------------------
   virtual float GetDensity() const = 0;
   virtual float GetWeight() const = 0;

//----------------------------
   virtual const C_buffer<C_smart_ptr<I3D_volume> > &GetVolumes() const = 0;

//----------------------------
// Get body mass center, relative to body rotation.
   virtual const S_vector &GetMassCenter() const = 0;
};
#endif

typedef class IPH_body *PIPH_body; 
typedef const IPH_body *CPIPH_body;

//----------------------------
//----------------------------

enum IPH_JOINT_TYPE{         
   IPHJOINTTYPE_NULL,
   IPHJOINTTYPE_BALL,
   IPHJOINTTYPE_HINGE,
   IPHJOINTTYPE_SLIDER,
   IPHJOINTTYPE_CONTACT,
   IPHJOINTTYPE_UNIVERSAL,
   IPHJOINTTYPE_HINGE2,
   IPHJOINTTYPE_FIXED,
   IPHJOINTTYPE_LAST
};

#ifndef PHYS_INTERNAL
class IPH_joint{
protected:
   virtual ~IPH_joint() = 0;
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

//----------------------------
   virtual IPH_JOINT_TYPE GetType() const = 0;

//----------------------------
   virtual void SetUserData(dword data) = 0;
   virtual dword GetUserData() const = 0;

//----------------------------
   virtual void Attach(PIPH_body body1, PIPH_body body2) = 0;

//----------------------------
// get body to which this joint is attached. 'index' may be 0 or 1.
   virtual PIPH_body GetBody(dword index) const = 0;

//----------------------------
// Set/Get anchor point (in world coords). For types where it doesn't make sense,
//  the returned value is false/S_vector(0, 0, 0).
   virtual bool SetAnchor(const S_vector &a) = 0;
   //virtual const S_vector &GetAnchor() const = 0;
   virtual bool SetLocalAnchor(dword body_index, const S_vector &a) = 0;

//----------------------------
// Set low and high stop params.
   virtual bool SetLoStop(float val, dword axis = 0) = 0;
   virtual bool SetHiStop(float val, dword axis = 0) = 0;

//----------------------------
// Set desired velocity (angular or linear), and maximal force applied to achieve this.
   virtual bool SetDesiredVelocity(float val, dword axis = 0) = 0;
   virtual bool SetMaxForce(float val, dword axis = 0) = 0;

//----------------------------
   virtual bool SetFudgeFactor(float val, dword axis = 0) = 0;

//----------------------------
// Set the bouncyness of the stops. This is a restitution parameter in the range 0.0 ... 1.0.
// 0.0 means the stops are not bouncy at all, 1.0 means maximum bouncyness.
// The default value is 0.0.
   virtual bool SetBounce(float val, dword axis = 0) = 0;

//----------------------------
// Set the constraint force mixing (CFM) value used when not at a stop.
   virtual bool SetCFM(float val, dword axis = 0) = 0;

//----------------------------
// The error reduction parameter (ERP) used by the stops. The allowed range is 0.0 ... 1.0.
   virtual bool SetStopERP(float val, dword axis = 0) = 0;

   virtual bool SetStopCFM(float val, dword axis = 0) = 0;

//----------------------------
// Suspension (currently implemented only on hinge2 joint).
   virtual bool SetSuspensionERP(float val, dword axis = 0) = 0;
   virtual bool SetSuspensionCFM(float val, dword axis = 0) = 0;

//----------------------------
// Name association.
   virtual void SetName(const C_str &name) = 0;
   virtual const C_str &GetName() const = 0;

//----------------------------
   virtual void Reserved0() = 0;
   virtual void Reserved1() = 0;
   virtual void Reserved2() = 0;
   virtual void Reserved3() = 0;
   virtual void Reserved4() = 0;
   virtual void Reserved5() = 0;
   virtual void Reserved6() = 0;
   virtual void Reserved7() = 0;
   virtual void Reserved8() = 0;
   virtual void Reserved9() = 0;
};
#endif

typedef class IPH_joint *PIPH_joint;
typedef const IPH_joint *CPIPH_joint;

//----------------------------

#ifndef PHYS_INTERNAL
class IPH_joint_ball: public IPH_joint{
public:
   enum E_AXIS_MODE{
      AXIS_GLOBAL,            //keep axis in world coordinates
      AXIS_BODY1,             //anchor axis to body 1
      AXIS_BODY2,             //anchor axis to body 2
      AXIS_AUTO               //automatically determine body (euler mode only)
   };

//----------------------------
// Set number of limiting axes.
   virtual void SetNumAxes(dword num) = 0;

//----------------------------
// Set mode of limiting axes - either 'user' mode or 'euler' mode must be set.
   virtual void SetUserMode(bool) = 0;

//----------------------------
// Set controlling axis.
   virtual void SetAxis(dword index, const S_vector &a, E_AXIS_MODE mode = AXIS_AUTO) = 0;

//----------------------------
// Set current angle around specified axis. This function is to be called only in user mode.
   virtual void SetAngle(dword index, float angle) = 0;
};
#endif

typedef class IPH_joint_ball *PIPH_joint_ball;
typedef const IPH_joint_ball *CPIPH_joint_ball;

//----------------------------
//----------------------------

#ifndef PHYS_INTERNAL
class IPH_joint_hinge: public IPH_joint{
public:

//----------------------------
   virtual void SetAxis(const S_vector &a) = 0;

//----------------------------
// Get the hinge angle and the time derivative of this value.
// The angle is measured between the two bodies, or between the body and the static environment.
// The angle will be between -PI ... PI.
   virtual float GetAngle() const = 0;
   virtual float GetAngleRate() const = 0;
};
#endif

typedef class IPH_joint_hinge *PIPH_joint_hinge;
typedef const IPH_joint_hinge *CPIPH_joint_hinge;

//----------------------------
//----------------------------

#ifndef PHYS_INTERNAL
class IPH_joint_slider: public IPH_joint{
public:
//----------------------------
   virtual void SetAxis(const S_vector &a) = 0;

//----------------------------
// Get the slider linear position (i.e. the slider's 'extension') and the time derivative of this value.
   virtual float GetSlidePos() const = 0;
   virtual float GetSlidePosRate() const = 0;

};
#endif

typedef class IPH_joint_slider *PIPH_joint_slider;
typedef const IPH_joint_slider *CPIPH_joint_slider;

//----------------------------
//----------------------------

#ifndef PHYS_INTERNAL
class IPH_joint_universal: public IPH_joint{
public:
//----------------------------
   virtual void SetAxis(dword index, const S_vector &a) = 0;
};
#endif

typedef class IPH_joint_universal *PIPH_joint_universal;
typedef const IPH_joint_universal *CPIPH_joint_universal;

//----------------------------
//----------------------------

#ifndef PHYS_INTERNAL
class IPH_joint_hinge2: public IPH_joint{
public:
//----------------------------
   virtual void SetAxis(dword index, const S_vector &a) = 0;

//----------------------------
   virtual float GetAngle() const = 0;
   virtual float GetAngleRate(dword index) const = 0;
};
#endif

typedef class IPH_joint_hinge2 *PIPH_joint_hinge2;
typedef const IPH_joint_hinge2 *CPIPH_joint_hinge2;

//----------------------------
//----------------------------

#ifndef PHYS_INTERNAL
class IPH_joint_fixed: public IPH_joint{
public:
   virtual void SetFixed() = 0;
};
#endif

typedef class IPH_joint_fixed *PIPH_joint_fixed;
typedef const IPH_joint_fixed *CPIPH_joint_fixed;

//----------------------------
//----------------------------

enum{
                              //if set, the contact surface is bouncy - in other words the bodies will bounce
                              // off each other
                              //the exact amount of bouncyness is controlled by the 'bounce' parameter
   IPH_CONTACT_BOUNCE    = 4,
                              //useful to make surfaces soft
   IPH_CONTACT_SOFT_ERP	= 8,
                              //useful to make surfaces soft
   IPH_CONTACT_SOFT_CFM	= 0x10,
                              //force-dependent-slip
   IPH_CONTACT_SLIP		= 0x180,
};

struct IPH_surface_param{
   dword mode;
   float coulomb_friction;

                              //restitution parameter (0 ... 1)
                              //0 means the surfaces are not bouncy at all, 1 is maximum bouncyness
   float bounce;
                              //the minimum incoming velocity necessary for bounce (in m/s)
                              //incoming velocities below this will effectively have a bounce parameter of 0
   float bounce_vel;

   float soft_erp;
   float soft_cfm;

                              //the coefficient of force-dependent-slip
                              // FDS is an effect that causes the contacting surfaces to side past each other
                              // with a velocity that is proportional to the force that is being applied 
                              // tangentially to that surface
                              //setting the slip to higher value (~100) causes the contact be ice-like
   float slip;

                              //value specifying how rough is the surface
                              // (it is used to point contact normal against linear velocity vector of body)
   float roughness;

   IPH_surface_param():
      mode(IPH_CONTACT_SOFT_CFM),
      coulomb_friction(1.0f),
      bounce(0.1f),
      bounce_vel(0),
      soft_erp(.2f),
      soft_cfm(.00005f),
      slip(1.0f),
      roughness(0.0f)
   {}
};

//----------------------------
//----------------------------

typedef bool IPH_ContactQuery(CPI3D_volume src_vol, PIPH_body src_body, CPI3D_frame dst_frm, PIPH_body dst_body, void *context);
typedef bool IPH_ContactReport(CPI3D_volume src_vol, PIPH_body src_body, CPI3D_frame dst_frm, PIPH_body dst_body,
   const S_vector &pos, const S_vector &normal, float depth, void *context);

typedef void IPH_MessageFunction(const char *msg);
typedef void IPH_DebugLine(const S_vector &from, const S_vector &to, dword type, dword color);
typedef void IPH_DebugPoint(const S_vector &p, float radius, dword type, dword color);
typedef void IPH_DebugText(const char *cp, dword color);

//----------------------------

#ifndef PHYS_INTERNAL
class IPH_world{
protected:
   virtual ~IPH_world() = 0;
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

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
};
#endif

typedef class IPH_world *PIPH_world;
typedef const IPH_world *CPIPH_world;

PIPH_world IPHCreateWorld();

//----------------------------

#endif