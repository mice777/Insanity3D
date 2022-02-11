/*
   Physics actor base class - this is a base actor for implementing any physically-modelled actors,
   which are made of bodies and optionally joints.

   This actor may be created directly (without inheriting from this, in which case it provides basic
   physics behavior, and destroying itself when it becomes idle.
*/

#ifndef __PHYSACTOR_H
#define __PHYSACTOR_H

#include "GameMission.h"
#include "systables.h"
#include <IPhysics.h>

//----------------------------

class C_actor_physics: public C_actor{
   struct S_sound{
      C_smart_ptr<I3D_sound> snd;
      S_vector pos;

                              //convention: first is hit frame's mat, second is this actor's body mat
      pair<dword, dword> mat_ids;

      float volume;
      dword count;
   };
   mutable C_vector<S_sound> sounds;

protected:
                              //body/joint container
   C_vector<C_smart_ptr<IPH_body> > bodies;
   C_vector<C_smart_ptr<IPH_joint> > joints;
   mutable dword idle_count;
   bool idle;

//----------------------------

   virtual void Tick(const struct S_tick_context &tc);

//----------------------------

   virtual void OnPhysIdle();

//----------------------------

   virtual void GameBegin();

//----------------------------

   virtual void GameEnd(bool undo_game_changes);

//----------------------------

   virtual void Push(const S_vector &dir){}

//----------------------------

   virtual bool ContactQuery(CPI3D_frame dst_frm) const{
      return true;
   }

//----------------------------

   virtual bool ContactReport(PI3D_frame dst_frm, const S_vector &pos, const S_vector &normal, float depth,
      bool play_sounds = true);

//----------------------------
// Close physics - deactivate bodies and joints.
   void Close(){
      bodies.clear();
      joints.clear();
   }

//----------------------------

public:
   C_actor_physics(C_game_mission &gm, PI3D_frame in_frm, E_ACTOR_TYPE at):
      C_actor(gm, in_frm ? in_frm : gm.GetScene()->CreateFrame(FRAME_MODEL), at),
      idle_count(0)
   {
      if(!in_frm)
         frame->Release();
   }

//----------------------------
// Init from frame, or provided template.
// weight ... weight of volumes (if 0, using info from tables)
// frm_root ... root frame, on which bodies will be searched (if NULL, actor's frame is used)
   bool Init(const S_phys_template *templ, float weight = 0.0f, PI3D_frame frm_root = NULL);

//----------------------------
   void AddForceAtPos(const S_vector &pos, const S_vector &dir, CPI3D_volume at_vol = NULL);
};

typedef C_actor_physics *PC_actor_physics;

//----------------------------

#endif