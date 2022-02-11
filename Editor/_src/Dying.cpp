#include "all.h"
#include "gamemission.h"
#include <IPhysics.h>

//----------------------------

enum E_BODY_PART{
   BODY_NULL,
   BODY_base,
   BODY_back2,
   BODY_back1,

   BODY_neck,
   BODY_l_arm,
   BODY_l_elbow,
   BODY_l_hand,
   BODY_l_palm,

   BODY_r_arm,
   BODY_r_elbow,
   BODY_r_hand,
   BODY_r_palm,

   BODY_l_thigh,
   BODY_l_shin,
   BODY_l_foot,
   BODY_r_thigh,

   BODY_r_shin,
   BODY_r_foot,
   BODY_tail,
   BODY_eyes,
   BODY_brest,
   BODY_LAST
};

struct S_body_part{
   const char *name;          //name of bodypart
   float vulnerability;       //multiplier
   const char *hit_pose_name; //name of pose played when this bodypart is hit
};

static S_body_part body_part_info[BODY_LAST] = {
   {NULL},
   {"base",    1.0f, "body"},
   {"back2",   2.0f, "body"},
   {"back1",   1.0f, "body"},

   {"neck",    4.0f, "head"},
   {"l_arm",   .9f,  "l_arm"},
   {"l_elbow", .8f,  "l_hand"},
   {"l_hand",  .75f, "l_hand"},
   {"l_palm",  .5f,  "l_hand"},

   {"r_arm",   .9f,  "r_arm"},
   {"r_elbow", .8f,  "r_hand"},
   {"r_hand",  .75f, "r_hand"},
   {"r_palm",  .5f,  "r_hand"},

   {"l_thigh", .9f,  "l_leg"},
   {"l_shin",  .8f,  "l_leg"},
   {"l_foot",  .6f,  "l_leg"},
   {"r_thigh", .9f,  "r_leg"},

   {"r_shin",  .8f,  "r_leg"},
   {"r_foot",  .6f,  "r_leg"},
   {"tail1",   .2f,  "body"},
   {"eyes",    4.0f, "body"},
   {"brest",   1.0f, "body"},
};


//----------------------------

                              //axis daya used with S_die_part_info struct
enum AXIS_ORDER{           //axis shuffling for ball joint (due to euler-angle transformations)
   AXIS_012 = 0,
   AXIS_021 = 1,
   AXIS_102 = 2,
   AXIS_120 = 3,
   AXIS_201 = 4,
   AXIS_210 = 5,
   AXIS_INIT_ROT_X_90 = 0x100,   //init-time reference axis for the part
   AXIS_INIT_ROT_X_N90 = 0x200,
   AXIS_INIT_ROT_Y_90 = 0x300,
   AXIS_INIT_ROT_Y_N90 = 0x400,
   AXIS_INIT_ROT_Z_90 = 0x500,
   AXIS_INIT_ROT_Z_N90 = 0x600,
   AXIS_NO_LIMIT = 0x10000,      //do not limit ball joint (debug-time usage)
};

//----------------------------
                              //struct for definition of rigid-body physics body parts
                              // (pointer to array of these are in S_body_params)
struct S_die_part_info{
   E_BODY_PART part;
   I3D_VOLUMETYPE vol_type;
   E_BODY_PART link;
   IPH_JOINT_TYPE jt;
                           //hinge: joint axis (x, y or z), relative to current joint
                           //ball: axis ordering of mapping to limiting euler angles
   dword axis;
   float lo_stop0, hi_stop0;  //in half-angle units for axis 0
   float lo_stop1, hi_stop1;  //for axis 1
   float lo_stop2, hi_stop2;  //for axis 2
   float cfm0, cfm1, cfm2;
};

//----------------------------

class C_die_physics{
   C_vector<C_smart_ptr<IPH_body> > bodies;
   C_vector<C_smart_ptr<IPH_joint> > joints;
   C_smart_ptr<IPH_world> world;
   bool test_mode;

   dword idle_count;
   bool is_idle;

//----------------------------

   void SetIdle(bool b){
#ifndef DIE_PHYS_NEVER_IDLE
                              //de-activate all bodies
      for(dword i=bodies.size(); i--; ){
         bodies[i]->Enable(!b);
      }
      is_idle = b;
#endif
   }

public:
   C_die_physics(C_actor *act, C_game_mission &mission, const S_die_part_info *part_info):
      test_mode(false),
      is_idle(false),
      idle_count(0)
   {
      world = mission.GetPhysicsWorld();
      PI3D_scene scn = mission.GetScene();
      //PC_actor act = body->GetActor();
      PI3D_model mod = act->GetModel();

      dword i;
                              //count num of parts first
      for(i=0; part_info[i].part; i++);
      bodies.reserve(i);

      for(i=0; part_info[i].part; i++){
         const S_die_part_info &bpi = part_info[i];
         E_BODY_PART bp = bpi.part;
                              //find the part in the body
         const char *pname = body_part_info[bp].name;
         PI3D_frame frm = mod->FindChildFrame(pname);
         if(!frm){
            act->ReportActorError(C_xstr("dying: can't find part '%'") % pname);
            break;
         }
                              //create appropriate volume representation of the part
         PI3D_volume vol = I3DCAST_VOLUME(scn->CreateFrame(FRAME_VOLUME));
         vol->SetVolumeType(bpi.vol_type);
         PIPH_body body = world->CreateBody();
         body->SetUserData(dword(act));
         //body->Enable(false);

         switch(frm->GetType()){
         case FRAME_VISUAL:
            if(I3DCAST_CVISUAL(frm)->GetVisualType()!=I3D_VISUAL_SINGLEMESH){
               assert(0);
               break;
            }
                              //flow...
         case FRAME_JOINT:
            {
               S_vector vol_pos;
               S_vector vol_dir;
               float vol_radius, vol_half_len;
               I3D_RESULT ir;
               if(frm->GetType()==FRAME_JOINT){
                  ir = I3DCAST_CJOINT(frm)->GetVolume(vol_pos, vol_dir, vol_radius, vol_half_len);
               }else{
                  ir = CPI3D_visual_singlemesh(frm)->GetVolume(vol_pos, vol_dir, vol_radius, vol_half_len);
               }
               if(I3D_FAIL(ir)){
                  act->ReportActorError(C_xstr("Dying physics: can't find volume on frame '%'") %frm->GetName());
                  break;
               }
               const S_matrix &fm = frm->GetMatrix();
               float scl = fm(0).Magnitude();

               const float VOL_ENLARGE = .0f;

               switch(bpi.vol_type){
               case I3DVOLUME_CAPCYL:
                  {
                     float r = vol_radius * scl;
                     float l = vol_half_len * scl;
                     vol->SetNUScale(S_vector(r, r, l));
                     vol->SetDir(vol_dir);
                  }
                  break;
               case I3DVOLUME_SPHERE:
                  {
                     float r = I3DSqrt(vol_half_len*vol_half_len + vol_radius*vol_radius);
                     r += r*VOL_ENLARGE;
                     vol->SetScale(r * scl);
                  }
                  break;
               default: assert(0);
               }
               vol->SetPos(vol_pos);
            }
            break;
         default: assert(0);
         }
#ifdef _DEBUG
         vol->SetName(C_xstr("<phys %>") % pname);
#endif
         vol->LinkTo(frm);
         body->SetFrame(frm, IPH_STEFRAME_USE_TRANSFORM, 1000.0f);
         //vol->SetOwner(mod);

         bool linked = false;
         if(bpi.link){
                              //find the link-to part (must be already created)
            for(int j=i; j--; ){
               if(part_info[j].part==bpi.link)
                  break;
            }
            //assert(j!=-1);
                              //should always be found (maybe not during testing parts)
            if(j!=-1){
               linked = true;
               S_quat save_rot = frm->GetWorldRot();

               PIPH_body prnt_body = bodies[j];
               PIPH_joint jnt = world->CreateJoint(bpi.jt);
               assert(jnt);
               jnt->Attach(body, prnt_body);

               static const S_quat init_rot[] = {
                  S_quat(S_vector(1, 0, 0),  PI*.5f),
                  S_quat(S_vector(1, 0, 0), -PI*.5f),
                  S_quat(S_vector(0, 1, 0),  PI*.5f),
                  S_quat(S_vector(0, 1, 0), -PI*.5f),
                  S_quat(S_vector(0, 0, 1),  PI*.5f),
                  S_quat(S_vector(0, 0, 1), -PI*.5f)
               };

               dword init_rot_i = (bpi.axis&0xff00) >> 8;
               const S_matrix &m_prnt = frm->GetParent()->GetMatrix();
                              //temporaly set volume to rotation where part would be in reference rotation
                              // (reference rot = zero local rot, or one of predefined base rots)
               {
                              //get parent's rotation
                  S_quat rot = m_prnt;

                  if(init_rot_i){
                              //put rotation to some rotation relative to parent
                              // this is done so that euler stops are relative to this 'offset' rotation,
                              // which is usually in middle of possible part movement
                     const S_quat &q = init_rot[init_rot_i-1];
                     rot = q * rot;
                  }
                              //set temp volume pos/rot
                  body->SetPosRot(frm->GetWorldPos(), rot);
#if 0
                  DebugLine(pos, pos+body->GetFrame()->GetMatrix()(0), 0, 0xffff0000);
                  DebugLine(pos, pos+body->GetFrame()->GetMatrix()(1), 0, 0xff00ff00);
                  DebugLine(pos, pos+body->GetFrame()->GetMatrix()(2), 0, 0xff0000ff);
#endif
               }
               //DebugLine(body->GetPos(), prnt_body->GetPos(), 0);
                              //setup anchor point to position of this frame
               const S_vector &a_pos = frm->GetWorldPos();
               //DebugPoint(a_pos, .2f, 0);
               //jnt->SetLocalAnchor2((a_pos - prnt_body->GetPos()) % ~prnt_body->GetRot());
               jnt->SetAnchor(a_pos);

               //DebugLine(a_pos, body->GetPos(), 0, 0xffff0000);
               //DebugLine(a_pos, prnt_body->GetPos(), 0, 0xff00ff00);
               //DebugLine(a_pos, link->GetPos(), 0, 0xff00ff00);
               dword num_axes = 1;

               switch(jnt->GetType()){
               case IPHJOINTTYPE_BALL:
                  {
                     if(bpi.axis&AXIS_NO_LIMIT)
                        break;
                     static const byte axis_order[6][3] = {
                        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
                     };

                     PIPH_joint_ball jb = (PIPH_joint_ball)jnt;
                     num_axes = 3;
                     jb->SetNumAxes(num_axes);

                     S_matrix m_axes = !init_rot_i ? m_prnt : S_matrix(init_rot[init_rot_i-1]) * m_prnt;

                     for(dword i=0; i<num_axes; i++){
                              //get (shuffled) euler axis set onto the joint
                        dword axis_index = axis_order[bpi.axis&0xff][i];
                              //get world axis
                        const S_vector &axis = m_axes(i);
                              //set axis on joint (except of axis1, which is automatically computed by joint)
                        if(axis_index!=1)
                           jb->SetAxis(axis_index, axis);
                        //DebugLine(m(3), m(3)+axis, 0, 0xff000000 | (0xff<<((2-i)*8)));

                              //setup params
                        float lo = !i ? bpi.lo_stop0 : i==1 ? bpi.lo_stop1 : bpi.lo_stop2;
                        float hi = !i ? bpi.hi_stop0 : i==1 ? bpi.hi_stop1 : bpi.hi_stop2;

                        float delta = hi - lo;
                        lo += delta*.1f;
                        hi -= delta*.1f;

                        jnt->SetLoStop(PI*lo, axis_index);
                        jnt->SetHiStop(PI*hi, axis_index);
                     }
                  }
                  break;

               case IPHJOINTTYPE_HINGE:
                  {
                     S_vector axis;
                     axis.Zero();
                     for(dword i=3; i--; ){
                        if(bpi.axis&(1<<i))
                           axis[i] = 1.0f;
                        else
                        if(bpi.axis&(0x100<<i))
                           axis[i] = -1.0f;
                     }
                     axis %= m_prnt;
                     //DebugLine(frm->GetWorldPos(), frm->GetWorldPos()+axis, 0);

                     PIPH_joint_hinge jh = (PIPH_joint_hinge)jnt;
                     jh->SetAxis(axis);
                     jnt->SetLoStop(PI*bpi.lo_stop0);
                     jnt->SetHiStop(PI*bpi.hi_stop0);
                  }
                  break;
               }
               float weight = body->GetWeight();
               for(dword i=0; i<num_axes; i++){
                  jnt->SetCFM(.02f, i);
                  jnt->SetBounce(.2f, i);
                  jnt->SetStopCFM(.06f, i);
                  jnt->SetStopERP(.1f, i);

                  jnt->SetMaxForce(weight*.003f, i);
                  //jnt->SetMaxForce(.004f, i);
                  //jnt->SetMaxForce(.004f, i);
               }

               joints.push_back(jnt);
               jnt->Release();
                              //restore to current rotation
               body->SetPosRot(frm->GetWorldPos(), save_rot);
            }
         }
         if(!linked){
#if defined EDITOR
            if(0){
                              //fix the root body
               PIPH_joint j = world->CreateJoint(IPHJOINTTYPE_FIXED);
               j->Attach(body, NULL);
               ((PIPH_joint_fixed)j)->SetFixed();
               joints.push_back(j);
               j->Release();
               test_mode = true;
            }
#endif
         }
         vol->Release();
         bodies.push_back(body);
         body->Release();
      }
   }
   ~C_die_physics(){
                              //explicitly clear vectors before world is destroyed
      bodies.clear();
      joints.clear();
   }

//----------------------------

   enum{
      SG_BODY_INFO,
         SG_BODY_POS_ROT,
         SG_BODY_AVEL,
         SG_BODY_LVEL,
      SG_IDLE_CNT,
      SG_IS_IDLE,
   };

//----------------------------

   void SaveGame(C_chunk &ck) const{

      ck
         (SG_IS_IDLE, is_idle)
         (SG_IDLE_CNT, idle_count)
         ;
                              //save all body info
                              // note: let loop go upwards, because we need pos/rot be applied from top of hierarchy down
      for(dword i=0; i<bodies.size(); i++){
         CPIPH_body b = bodies[i];
         ck <<= SG_BODY_INFO;
         ck.Write(&i, sizeof(int));
         pair<S_vector, S_quat> pr;
         b->GetPosRot(pr.first, pr.second);
         ck
            (SG_BODY_POS_ROT, pr)
            (SG_BODY_LVEL, b->GetLinearVelocity())
            (SG_BODY_AVEL, b->GetAngularVelocity())
            ;
         --ck;
      }
   }
   void ApplySavedGame(C_chunk &ck) throw(C_except){

      while(ck)
      switch(++ck){
      case SG_IS_IDLE: ck >> is_idle; break;
      case SG_IDLE_CNT: ck >> idle_count; break;
      case SG_BODY_INFO:
         {
            dword indx = ck.ReadDword();
            assert(indx < bodies.size());
            PIPH_body b = bodies[indx];
            pair<S_vector, S_quat> pr;
            
            while(ck)
            switch(++ck){
            case SG_BODY_POS_ROT: ck >> pr; break;
            case SG_BODY_LVEL: b->SetLinearVelocity(ck.RVectorChunk()); break; 
            case SG_BODY_AVEL: b->SetAngularVelocity(ck.RVectorChunk()); break;
            default: assert(0); --ck;
            }
            b->SetPosRot(pr.first, pr.second);
            --ck;
         }
         break;
      default: assert(0); --ck;
      }
      SetIdle(is_idle);
   }

//----------------------------
// Returns true when physics is idle.
   bool Tick(int time, C_actor *body){

      if(is_idle)
         return false;

                              //detect if anything is moving
      bool is_idle_now = true;
      for(dword i=0; i<bodies.size(); i++){
         PIPH_body b = bodies[i];
         float lvel2 = b->GetLinearVelocity().Square();
         float avel2 = b->GetAngularVelocity().Square();
         const float LK = .1f, AK = .4f;
         if(lvel2 > LK*LK || avel2 > AK*AK){
            is_idle_now = false;
            break;
         }
      }
      if(is_idle_now){
         if((idle_count += time) >= 500){
            SetIdle(true);
         }
      }else{
         idle_count = 0;
      }
      return is_idle;
   }

//----------------------------

   void Hit(PI3D_frame hit_frm, const S_vector &hit_pos, const S_vector &hit_dir, int power){

      PIPH_body body = NULL;
                              //determine which body was hit
      for(dword i=bodies.size(); i--; ){
         if(bodies[i]->GetFrame()==hit_frm){
            body = bodies[i];
            break;
         }
      }
      if(!body){
         if(hit_frm->GetType()==FRAME_VOLUME)
            body = world->GetVolumeBody(I3DCAST_CVOLUME(hit_frm));
         if(!body){
                              //get body closest to the point
            float best_d = 1e+16f;
            for(dword i=bodies.size(); i--; ){
               const S_vector &p = bodies[i]->GetFrame()->GetWorldPos();
               float d2 = (p-hit_pos).Magnitude();
               if(best_d > d2){
                  best_d = d2;
                  body = bodies[i];
               }
            }
         }
      }
      assert(body);
      if(body){
         //DebugPoint(hit_pos, .05f, 0); DebugLine(hit_pos, hit_pos+S_normal(hit_dir), 0);
         body->AddForceAtPos(hit_pos, S_normal(hit_dir) * (float)power * 4.0f);
         if(is_idle)
            SetIdle(false);
         idle_count = 0;
      }
   }

//----------------------------

   void ResetIdleCount(){
      idle_count = 0;
   }
};

//----------------------------

static const S_die_part_info die_part_info[] = {
   {BODY_base, I3DVOLUME_SPHERE},
   {BODY_back2, I3DVOLUME_SPHERE, BODY_base, IPHJOINTTYPE_BALL, 0, -.3f, .3f, -.2f, .2f, -.2f, .2f},

   {BODY_neck, I3DVOLUME_SPHERE, BODY_back2, IPHJOINTTYPE_BALL, 0, -.3f, .3f, -.4f, .4f, -.2f, .2f},

   {BODY_l_arm, I3DVOLUME_CAPCYL, BODY_back2, IPHJOINTTYPE_BALL, AXIS_210 | AXIS_INIT_ROT_Z_90,
      -.6f, .3f, -.1f, .1f, -.48f, .6f},
   {BODY_l_elbow, I3DVOLUME_CAPCYL, BODY_l_arm, IPHJOINTTYPE_HINGE, 1, -.7f, .1f},

   {BODY_r_arm, I3DVOLUME_CAPCYL, BODY_back2, IPHJOINTTYPE_BALL, AXIS_210 | AXIS_INIT_ROT_Z_N90,
      -.6f, .3f, -.0f, .0f, -.6f, .48f},
   {BODY_r_elbow, I3DVOLUME_CAPCYL, BODY_r_arm, IPHJOINTTYPE_HINGE, 0x1, -.7f, .1f},
   
   {BODY_l_thigh, I3DVOLUME_CAPCYL, BODY_base, IPHJOINTTYPE_BALL, AXIS_210,
      -.5f, .3f, -.0f, .0f, -.4f, .15f},
   {BODY_l_shin, I3DVOLUME_CAPCYL, BODY_l_thigh, IPHJOINTTYPE_HINGE, 1, -.05f, .8f},

   {BODY_r_thigh, I3DVOLUME_CAPCYL, BODY_base, IPHJOINTTYPE_BALL, AXIS_210,
      -.5f, .3f, -.0f, .0f, -.15f, .4f},
   {BODY_r_shin, I3DVOLUME_CAPCYL, BODY_r_thigh, IPHJOINTTYPE_HINGE, 1, -.1f, .8f},
   /**/
#if 0
                        //palms and feets (optional)
   {BODY_l_hand, I3DVOLUME_SPHERE, BODY_l_elbow, IPHJOINTTYPE_BALL, 0, -.3f, .3f, -.15f, .15f, -.4f, .4f},
   {BODY_r_hand, I3DVOLUME_SPHERE, BODY_r_elbow, IPHJOINTTYPE_BALL, 0, -.3f, .3f, -.15f, .15f, -.4f, .4f},
   /*
   {BODY_l_foot, I3DVOLUME_SPHERE, BODY_l_shin, IPHJOINTTYPE_HINGE, 4, -.0f, .1f},
   {BODY_r_foot, I3DVOLUME_SPHERE, BODY_r_shin, IPHJOINTTYPE_HINGE, 4},
   /**/
#endif
   {BODY_NULL}
};

//----------------------------

class C_dier: public C_actor{
   C_die_physics *phys;
public:
   C_dier(C_game_mission &gm, PI3D_frame frm):
      C_actor(gm, frm, ACTOR_DIER),
      phys(NULL)
   {
                              //remove our collision
      DeleteVolumes(GetModel());
   }

//----------------------------

   virtual void GameBegin(){

      phys = new C_die_physics(this, mission, die_part_info);
   }

//----------------------------

   virtual void GameEnd(){

      delete phys;
      phys = NULL;
   }

//----------------------------

   virtual void Tick(const struct S_tick_context &tc){
      if(phys)
         phys->Tick(tc.time, this);
   }
};

//----------------------------

PC_actor CreateDierActor(C_game_mission &gm, PI3D_frame frm){
   return new C_dier(gm, frm);
}

//----------------------------

