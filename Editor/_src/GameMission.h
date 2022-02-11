#ifndef __GAMEMISSION_H
#define __GAMEMISSION_H

#include "main.h"
#include "actors.h"
#include "checkpoint.h"
#include <IPhysics.h>
#include <insanity\PhysTemplate.h>

//----------------------------

enum E_MISSION_RESULT{        //result of the mission
   MR_FAILURE,                //mission successful
   MR_SUCCESS,                //mission unsuccessful
   MR_UNDETERMINATE,          //the result is undeterminate
};

//----------------------------
//----------------------------
                              //specialized C_mission - game itself
                              //additional features:
                              // - list of actors
                              // - level time counter
                              // - checkpoints
                              // - menu and icons
                              // - sounds currently playing in game
                              // - game camera
                              // - aim model class
#define TICK_CLASS_ID_GAME_MISSION 'GMIS'

class C_game_mission: public C_mission{
public:
   enum E_MISSION_STATE{
      MS_EDIT,                //edit mode
      MS_IN_GAME,             //game mode
      MS_QUIT_REQUEST,        //request to quit (GameOver)
      MS_GAME_OVER,           //game over, displaying menu
   };

   inline E_MISSION_STATE GetState() const{ return mission_state; }

protected:
   PC_table tab_config;


   C_vector<C_smart_ptr<C_actor> > actors;
   E_MISSION_STATE mission_state;
   dword level_time;          //since game begun, increased at Tick

public:
   enum E_TAB{
      TAB_E_WEATHER_EFFECT = 256,   //0=no, 1=rain, 2=snow
      TAB_I_WEATHER_DENSITY = 257,  //density, 1 unit per m3
      TAB_S20_TERRDET_MASK = 258,   //mask for terrain visuals
      TAB_S20_TERRDET_MATMASK = 259,//mask for terrain material
      TAB_E_TERRDET_MODELSET = 260, //index into model set (model sets defined in material table)
      TAB_F_TERRDET_VISIBILITY = 261,  //visibility distance, until which detail objects are placed
      TAB_B_TERRDET_USE = 262,      //use terrain detail system
      TAB_S16_HERO_MODEL_NAME = 265,//name of hero model (default is "Anu")
                              //player's inventory
      TAB_E_GMIS_INV_ITEM_TYPE = 268, 
      TAB_I_GMIS_INV_ITEM_AMOUNT = 269,
                              //mission tasks
      TAB_S32_GMIS_TASK_NAME_ID = 270, //id into text file for task
      TAB_B_GMIS_TASK_INIT_STATE = 271,//true for visible from init or false for add it later by script
   };

//----------------------------
// Fast access to few internals.
   inline CPC_table GetConfigTab() const{ return tab_config; }

//----------------------------
// Determine if we're in true game mode.
   inline bool IsInGame() const{ return mission_state!=MS_EDIT; }

//----------------------------
// Get time, in milliseconds, from beginning of mission.
   inline dword GetLevelTime() const{ return level_time; }
//----------------------------
// Hope the caller knows why to call this function!
   inline void SetLevelTime(dword dw){
      assert(!IsInGame());
      level_time = dw;
   }

//----------------------------
// Get C_vector of checkpoints.
   virtual const C_vector<C_smart_ptr<C_checkpoint> > &GetCheckpoints() const = 0;

//----------------------------
// Find checkpoint identified by name.
   virtual CPC_checkpoint FindCheckpoint(const char*) const = 0;

//----------------------------
// Prepare checkpoint grid - precompute connection data.
   virtual void PrepareCheckpoints() = 0;

//----------------------------
// Create new checkpoint, add to list.
   virtual PC_checkpoint CreateCheckpoint() = 0;

//----------------------------
// Destoroy a checkpoint - remove from list.
   virtual void DestroyCheckpoint(CPC_checkpoint) = 0;

//----------------------------
// Return reference to C_vector of actors
   inline const C_vector<C_smart_ptr<C_actor> > &GetActorsVector() const{return actors ; }

public:
   C_game_mission();

//----------------------------
// Called right before game beginning, to initialize in-game menus, actors,
// setup order of players, generate IDs, etc.
// Sets mission_state to MS_IN_GAME.
   virtual void GameBegin() = 0;

//----------------------------
// Called right before ending game, to uninitialize items initialized by GameBegin.
// Sets mission_state to MS_EDIT.
// If 'undo_game_changes' is true, any changes caused by game run are restored back to state before GameBegin.
   virtual void GameEnd() = 0;

//----------------------------
// Provide game camera interface.
   virtual class C_game_camera *GetGameCamera() = 0;
   virtual const C_game_camera *GetGameCamera() const = 0;

//----------------------------
// Finish the game. This func may be called anytime, it just sets internal state
// and finishes the game at appropriate time.
// 'msg_id' specifies text written to message window, describing the purpose of finishing the mission,
// if it is NULL, no message is written.
   virtual bool MissionOver(E_MISSION_RESULT, const char *msg_id) = 0;

//----------------------------
// Create specified actor. Parameter frm may be valid frame of NULL, in which case
// actor is responsible for creating its own frame.
// Parameter 'data' points to initialization structure, which meaning depends
// on type of actor being created.
   virtual PC_actor CreateActor(PI3D_frame frm, E_ACTOR_TYPE, const void *data = NULL) = 0;

//----------------------------
// Destroy specified actor.
   virtual void DestroyActor(PC_actor) = 0;

//----------------------------
// Find actor specified by name of his frame.
   virtual PC_actor FindActor(const char *name) = 0;
   virtual const C_actor *FindActor(const char *name) const = 0;

//----------------------------
// Play a non-looped 3D sound. All such sounds are kept in internal C_vector,
// and released when they finish playing, or when mission is closed.
   virtual PI3D_sound PlaySound(const char *snd_name, float min_dist, float max_dist,
      PI3D_frame link_to = NULL, const S_vector &pos = S_vector(0.0f, 0.0f, 0.0f),
      float volume = 1.0f) = 0;

//----------------------------
// Play simple ambient sound.
   virtual I3D_RESULT PlayAmbientSound(const char *snd_name, float volume = 1.0f) = 0;

// Play action sound depending on given material IDs.
// Each combination of IDs may have defined special sound.
// If more sound variations are available, a random is selected.
// This function also logically emits sounds to all AI percievers (human, detector) by calling EmitListen.
//    'mat_id2' may have two special values: E_ACTION_ID_HIT and E_ACTION_ID_STEP, which specify that
//       2nd material is not real material, but a group of sounds for special actions
   enum{ E_ACTION_ID_HIT = 0x10000000, E_ACTION_ID_STEP = 0x20000000 };
   virtual PI3D_sound PlayActionSound(const S_vector &pos, dword mat_id1, dword mat_id2, CPC_actor emmitor, float volume = 1.0f) = 0;

//----------------------------
// Mission-global collision testing.
   bool TestCollision(I3D_collision_data &cd) const{
      return scene->TestCollision(cd);
   }

//----------------------------
// Get physics 'world'.
   virtual class IPH_world *GetPhysicsWorld() = 0;

//----------------------------
   virtual bool LoadPhysicsTemplate(const char *fname, S_phys_template &t) const = 0;
};

typedef C_game_mission *PC_game_mission;

PC_game_mission CreateGameMission();

//----------------------------
//----------------------------

//----------------------------

#endif

