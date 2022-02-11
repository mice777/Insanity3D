#include "all.h"                              
#include "gamemission.h"
#include "PhysicsActor.h"

//----------------------------

#define TURN_SPEED 3000

#ifdef _DEBUG

#define DEBUG_SHOW_INFO       //show info about vehicle (speed, etc)

#define DEBUG_NO_IDLE

#endif

#define DEBUG_DIRECT_KEYS

//----------------------------
//----------------------------

#include "game_cam.h"

                           //speed per AUTO_TRANSLATE_UP t/sec
static const float wanted_speed[] = {
   -3.5,
   0,
   2, 8, 16, 32, 50, 72
};

class C_vehicle_imp: public C_actor_physics{

   C_smart_ptr<I3D_sound> snd_engine;
   C_smart_ptr<I3D_visual> brake_light;
                              //wheel joints, first are front, last are back
   C_smart_ptr<IPH_joint_hinge2> jnt_wheels[4];

   bool enabled;

   S_vector last_pos;
   int last_gear;
   float last_frequency;

//----------------------------

   void Enable(bool b){

      if(enabled==b)
         return;
      for(int i=bodies.size(); i--; )
         bodies[i]->Enable(b);
      enabled = b;
   }

//----------------------------

   virtual void OnPhysIdle(){

#ifndef DEBUG_NO_IDLE
      Enable(false);
#endif
      /*
      bodies.clear();
      joints.clear();
      */
      idle_count = 0;
   }

//----------------------------

   virtual bool ContactReport(PI3D_frame dst_frm, const S_vector &pos, const S_vector &normal, float depth,
      bool play_sounds = true){

      return C_actor_physics::ContactReport(dst_frm, pos, normal, depth, false);
   }

//----------------------------

public:
   C_vehicle_imp(C_game_mission &gm1, PI3D_frame in_frm):
      C_actor_physics(gm1, in_frm, ACTOR_VEHICLE),
      enabled(false),
      last_pos(0, 0, 0),
      last_gear(0)
      //emit_sound_count(0),
      //wheel_turn(0.0f)
   {
      //InitVals();
   }

//----------------------------

   virtual void GameBegin(){

      SetupVolumes(0x1);

      C_game_camera *gcam = mission.GetGameCamera();
      gcam->SetFocus(frame->FindChildFrame("engine"), frame);
      gcam->SetDistance(10.0f);

      PI3D_model mod = GetModel();
      if(mod){
         S_phys_template templ;
         const C_str &mname = mod->GetFileName();
         if(!mission.LoadPhysicsTemplate(C_xstr("Models\\%\\scene.bin") %&mname[1], templ))
            ReportActorError("Model is not listed in active objects");
         else{
            C_actor_physics::Init(&templ);
            assert(joints.size() >= 4);
            enabled = true;
            Enable(false);
                              //find all wheels
            for(dword i=joints.size(); i--; ){
               PIPH_joint jnt = joints[i];
               if(jnt->GetType()==IPHJOINTTYPE_HINGE2){
                              //apply steering on the front joint
                  PIPH_joint_hinge2 jh = (PIPH_joint_hinge2)jnt;

                  if(jh->GetName()=="joint_l_f")
                     jnt_wheels[0] = jh;
                  else
                  if(jh->GetName()=="joint_r_f")
                     jnt_wheels[1] = jh;
                  else
                  if(jh->GetName()=="joint_l_b")
                     jnt_wheels[2] = jh;
                  else
                  if(jh->GetName()=="joint_r_b")
                     jnt_wheels[3] = jh;
               }
            }
            assert(jnt_wheels[0] && jnt_wheels[1] && jnt_wheels[2] && jnt_wheels[3]);

            const float force_steer = 50.0f;

            jnt_wheels[0]->SetMaxForce(force_steer);
            jnt_wheels[1]->SetMaxForce(force_steer);
         }
      }

      {
         if(!snd_engine){
            snd_engine = I3DCAST_SOUND(mission.GetScene()->CreateFrame(FRAME_SOUND));
            snd_engine->Release();
         }
         snd_engine->SetSoundType(I3DSOUND_POINT);
         snd_engine->SetOn(false);

         const char *cp = "Test\\Motor";

         sound_cache.Open(snd_engine, cp, mission.GetScene(), I3DLOAD_SOUND_FREQUENCY, ErrReport, editor);
         snd_engine->SetRange(2.0f, 40.0f);
         snd_engine->SetLoop(true);
         PI3D_frame dum = frame->FindChildFrame("engine");
         if(!dum) dum = frame;
         snd_engine->LinkTo(dum);
      }
      brake_light = I3DCAST_VISUAL(frame->FindChildFrame("light_brake", ENUMF_VISUAL));

      last_pos = frame->GetWorldPos();
      last_gear = 0;
      last_frequency = 1.0f;
   }

//----------------------------

   virtual void GameEnd(bool){

      //OnPhysIdle();
      bodies.clear();
      joints.clear();
      enabled = false;
      snd_engine = NULL;
   }

//----------------------------

   virtual void Tick(const S_tick_context &tc){

      float tsec = (float)tc.time * .001f;

      float curr_speed = 0.0f;
      bool moving_backward = false;
      if(tc.time){
         S_vector move_dir = last_pos - frame->GetWorldPos();
         float dist = move_dir.Magnitude();
         float t = tsec / 3.6f;
         curr_speed = dist / t;

         moving_backward = (move_dir.Dot(frame->GetWorldDir()) > 0.0f);
      }

#ifdef DEBUG_DIRECT_KEYS
      int want_dir = 0;       //0=stay, 1=forward, -1=backward
      float steer = 0.0f;
      bool brake = false;

      if(tc.p_ctrl){
         if(tc.p_ctrl->Get(CS_MOVE_FORWARD)){
            want_dir = 1;
         }else
         if(tc.p_ctrl->Get(CS_MOVE_BACK)){
            want_dir = -1;
         }
         if(tc.p_ctrl->Get(CS_MOVE_LEFT)){
            steer = .5f;
         }else
         if(tc.p_ctrl->Get(CS_MOVE_RIGHT)){
            steer = -.5f;
         }
         if(tc.p_ctrl->Get(CS_FIRE)){
            brake = true;
         }
      }

      const int max_gear = 5;

      static struct S_gear_table{
         float _vel, force, trans_speed;
      } gear_tab[max_gear+3] = {
         {-2.0f, 20, 15},              //-1
         {0, 0},              //0
         {2.3f, 33, 15},      //1
         {4.5f, 22, 35},      //2
         {8.0f, 16, 55},      //3
         {17.0f, 18, 80},      //4
         {24.0f, 10,   },      //5
      };

                              //determine gear
      int gear = 0;
      if(want_dir){
         if(want_dir > 0){
            if(moving_backward && curr_speed > 1.0f){
               want_dir = 0;
               brake = true;
            }else{
               for(gear=1; gear<max_gear; gear++){
                  if(curr_speed < gear_tab[gear+1].trans_speed)
                     break;
               }
                              //don't gear down too early
               if(last_gear>gear && curr_speed > (gear_tab[gear+1].trans_speed*.7f))
                  ++gear;
               else
               if(gear > last_gear+1)
                  gear = last_gear+1;
               //gear = 1;
            }
         }else{
            if(!moving_backward && curr_speed > 1.0f){
               want_dir = 0;
               brake = true;
            }else{
               gear = -1;
            }
         }
      }
      last_gear = gear;

      //if(motor)
      {
         if((want_dir || steer) && !enabled)
            Enable(true);
         //float turn = 0.0f;
                              //forces applied to joint motor under various circumstances
         const float brake_force = 100.0f;
         const float neutral_force = 3.0f;

         float vel = 0.0f, force;
         if(!want_dir){
            force = brake ? brake_force : neutral_force;
         }else{
            //vel = gear_tab[gear+1].vel;
            force = gear_tab[gear+1].force;
            //DEBUG("***");
            //DEBUG(vel);
            //DEBUG(curr_speed * .25f);
            float rel_speed = Min(curr_speed, gear_tab[gear+1].trans_speed);
            vel = .5f + rel_speed * .3f;
            vel *= (float)want_dir;
         }
                              //process all wheels
         for(int i=4; i--; ){
            PIPH_joint_hinge2 jnt = jnt_wheels[i];
            if(i<2){
               float curr_angle = jnt->GetAngle();
               float v = steer - curr_angle;
               float k = tsec * 2.0f;
               if(curr_angle * steer < 0.0f)
                  k *= 2.0f;
               v = Max(-k, Min(k, v));
               v *= 10.0f;
               jnt->SetDesiredVelocity(v);
            }
                           //apply motor onto joint
            jnt->SetDesiredVelocity(-vel, 1);
            jnt->SetMaxForce(force, 1);

            //turn += jnt->GetAngleRate(1);
         }
         //turn /= 4.0f;
         if(snd_engine){
            //float m = I3DFabs(turn);
            float m = 1.0f;
            if(gear){
               m = curr_speed*.4f / I3DFabs(gear_tab[gear+1]._vel);
               m = Max(.7f, m);
            }
            //DEBUG(m);
            float delta = m - last_frequency;
            const float max_change = tsec * 3.0f;
            if(I3DFabs(delta) > max_change){
               delta = (delta<0.0f) ? -max_change : max_change;
            }
            last_frequency += delta;

            //if(m > .1f){
               if(!snd_engine->IsOn())
                  snd_engine->SetOn(true);
               snd_engine->SetFrequency(last_frequency);
               //LOG(m);
               //snd_engine->SetVolume((m / 5.0f)*1.0f);
            //}else{
               //snd_engine->SetOn(false);
            //}
         }
         if(brake_light)
            brake_light->SetBrightness(I3D_VIS_BRIGHTNESS_EMISSIVE, brake ? 1.0f : 0.0f);
      }
#endif   //DEBUG_DIRECT_KEYS

#ifdef DEBUG_SHOW_INFO
      DEBUG(C_xstr("Speed: #.1% km/h") %curr_speed);
      DEBUG(C_xstr("Gear: %") %gear);
      //DEBUG(C_xstr("Moving back: %") %moving_backward);
#endif

      last_pos = frame->GetWorldPos();

      C_actor_physics::Tick(tc);
      if(bodies.size())
         mission.GetScene()->SetFrameSectorPos(frame, bodies[0]->GetFrame()->GetWorldPos());
   }
};

//----------------------------

PC_actor CreateVehicleActor(C_game_mission &gm1, PI3D_frame in_frm){

   C_vehicle_imp *vp = new C_vehicle_imp(gm1, in_frm);
   return vp;
}

//----------------------------
