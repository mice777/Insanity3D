#include "all.h"
#include "PhysicsActor.h"

#ifdef _DEBUG

//#define DEBUG_NO_IDLE         //never detect idle state

#endif

                              //time, after which idle physics freezes (actually calls OnIdle method)
#define PHYS_IDLE_TIME 1000
                              //distance, under which sounds are concantated
const float SND_CONCAT_DIST = 1.0f;
                              //sound life len, under which concantation is to succeed
const dword SND_CONCAT_LIFE_LEN = 100;
                              //conversion value of linear mass velocity at contact point into sounds volume
const float SND_POW_TO_VOL = .05f;

const float HIT_POW_TO_HARM = 6.0f;

//----------------------------

void C_actor_physics::Tick(const S_tick_context &tc){

   if(!bodies.size())
      return;
   if(idle)
      return;

   int i;

   for(i=sounds.size(); i--; ){
      S_sound &s = sounds[i];
      if(!s.snd){
                              //create new sound
         s.snd = mission.PlayActionSound(s.pos, s.mat_ids.first, s.mat_ids.second, this, s.volume);   
         if(!s.snd){
            s = sounds.back(); sounds.pop_back();
            continue;
         }
      }else
      if(!s.snd->IsPlaying()){
                              //if not playing, kill
         s = sounds.back(); sounds.pop_back();
         continue;
      }
                              //add counter
      s.count += tc.time;
   }

                           //check if idle
   for(i=bodies.size(); i--; ){
      PIPH_body body = bodies[i];
      const S_vector &lv = body->GetLinearVelocity();
      const S_vector &av = body->GetAngularVelocity();
      if(!(lv.Square() < 1e-2f && av.Square() < 1e-2f))
         break;
   }
   if(i==-1){
#ifndef DEBUG_NO_IDLE
      if((idle_count += tc.time) > PHYS_IDLE_TIME){
         OnPhysIdle();
         return;
      }
#endif
   }else{
      idle_count = 0;
   }
}

//----------------------------

void C_actor_physics::OnPhysIdle(){
   /*
   idle = true;
   for(dword i=bodies.size(); i--; )
      bodies[i]->Enable(false);
      */
}

//----------------------------

void C_actor_physics::GameBegin(){

   Init(NULL);
}

//----------------------------

void C_actor_physics::GameEnd(bool undo_game_changes){

   Close();
}

//----------------------------
// Get scaled velocity, by which body applies power at specified point.
// The velocity takes into account distribution from mass, velocity and weight.
static S_vector GetBodyHitVelocity(CPIPH_body body, const S_vector &pos, const S_vector &hit_normal){

                              //consider the linear speed of body (vel at pos may be pretty inaccurate)
   S_vector vel = body->GetLinearVelocity();
   //S_vector vel = body->GetVelAtPos(pos);
                              //multiply by relative position of contact point on the movement line
   S_normal dir_to_center = pos - body->GetMassCenter() * body->GetFrame()->GetMatrix();
   //float mass_apply = -dir_to_center.Dot(hit_normal)*.5f + .5f;
   float mass_apply = dir_to_center.Dot(S_normal(vel))*.5f + .5f;
   //DebugLine(pos, pos-dir_to_center, 1);
                              //apply weight (slightly, for avoiding hard hits by heavy objects)
   return vel * mass_apply * (1.0f + body->GetWeight()*.02f);
}

//----------------------------

bool C_actor_physics::ContactReport(PI3D_frame dst_frm, const S_vector &pos, const S_vector &normal, float depth,
   bool play_sounds){

   PIPH_world world = mission.GetPhysicsWorld();

   CPIPH_body b1 = bodies[0];
   S_vector vel = GetBodyHitVelocity(b1, pos, normal);
                              //find body2
   CPIPH_body b2 = NULL;
   if(dst_frm->GetType()==FRAME_VOLUME){
      b2 = world->GetVolumeBody(I3DCAST_VOLUME(dst_frm));
      if(b2)
         vel -= GetBodyHitVelocity(b2, pos, normal);
   }
   //float pow = -vel.Dot(normal);
   float pow = vel.Magnitude();
   //PRINT(pow);

   if(play_sounds){
      float vol = Min(1.0f, pow * SND_POW_TO_VOL);
      if(vol > .05f){
         //PRINT(pow);
         //DebugLine(pos, pos+vel, 0);
         pair<dword, dword> mat_ids;
         mat_ids.first = GetMaterialID(dst_frm);
         mat_ids.second = GetMaterialID(b1->GetVolumes()[0]);

                                 //check with sounds
         int closest_con = -1;
         float closest_d2 = 1e+16f;
         for(int i=sounds.size(); i--; ){
            const S_sound &s = sounds[i];
            if(s.mat_ids == mat_ids && s.count < SND_CONCAT_LIFE_LEN){
               float d_2 = (s.pos-pos).Square();
               if(closest_d2 > d_2){
                  closest_d2 = d_2;
                  closest_con = i;
               }
            }
         }
         if(closest_d2 < SND_CONCAT_DIST*SND_CONCAT_DIST){
                                 //concanetate the sound
            //PRINT("sound concanetated");
            S_sound &s = sounds[closest_con];
            s.pos = (s.pos * s.volume + pos * vol) / (s.volume + vol);
            s.volume = Min(1.0f, s.volume+vol);
            if(s.snd){
                                 //update existing sound
               s.snd->SetPos(s.pos);
               s.snd->SetVolume(s.volume);
            }
         }else{
                                 //add new sound entry
            //PRINT("new sound added");
            sounds.push_back(S_sound());
            S_sound &s = sounds.back();
            s.pos = pos;
            s.volume = vol;
            s.mat_ids = mat_ids;
            s.count = 0;
         }
      }
   }
   return true;
}

//----------------------------

void C_actor_physics::AddForceAtPos(const S_vector &pos, const S_vector &dir, CPI3D_volume at_vol){

                              //find body, on which we'll apply the force
   PIPH_body body = NULL;
   if(at_vol){
      for(dword i=bodies.size(); !body && i--; ){
         const C_buffer<C_smart_ptr<I3D_volume> > &vols = bodies[i]->GetVolumes();
         for(int j=vols.size(); j--; ){
            if(vols[j]==at_vol){
               body = bodies[i];
               break;
            }
         }
      }
      assert(body);
   }else{
      //assert(bodies.size()==1);
      body = bodies[0];
   }
   body->AddForceAtPos(pos, dir);
}

//----------------------------

bool C_actor_physics::Init(const S_phys_template *templ, float weight, PI3D_frame frm_root){

   PIPH_world world = mission.GetPhysicsWorld();
   if(!frm_root)
      frm_root = frame;

                           //init...
   if(templ && !templ->IsEmpty()){
      assert(!weight);
                           //...from given template
      if(!templ->InitSimulation(bodies, joints, world, NULL, frm_root))
         return false;
      for(int i=bodies.size(); i--; ){
         PIPH_body body = bodies[i];
                                       //keep this pointer in user data
         body->SetUserData((dword)this);
      }
   }else{
                           //...directly from model
      PIPH_body body = world->CreateBody();
      bodies.push_back(body);
      body->Release();

      float density = 2000.0f;
      dword setf_flags = IPH_STEFRAME_USE_TRANSFORM | IPH_STEFRAME_HR_VOLUMES;

      if(weight){
         density = weight;
         setf_flags |= IPH_STEFRAME_DENSITY_AS_WEIGHT;
      }else{
                              //read density from material table
         PI3D_volume vol;
         if(frm_root->GetType()==FRAME_VOLUME)
            vol = I3DCAST_VOLUME(frm_root);
         else
            vol = I3DCAST_VOLUME(frm_root->FindChildFrame(NULL, ENUMF_VOLUME));
         if(vol){
            dword mat_id = vol->GetCollisionMaterial();
            density = tab_materials->ItemF(TAB_F_MAT_DENSITY, mat_id);
         }
      }

                           //initialize the body from this frame
      if(!body->SetFrame(frm_root, setf_flags, density)){
         ReportActorError("cannot initialize rigid body (no mass?)");
         return false;
      }

                           //keep this pointer in user data
      body->SetUserData((dword)this);
   }
   idle_count = 0;
   return true;
}

//----------------------------
//----------------------------

PC_actor CreateActorPhysics(C_game_mission &gm, PI3D_frame in_frm){

   return new C_actor_physics(gm, in_frm, ACTOR_PHYSICS);
}

//----------------------------
//----------------------------
