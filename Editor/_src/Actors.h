#ifndef __ACTORS_H
#define __ACTORS_H


enum E_ACTOR_TYPE{            //actor class ID
   ACTOR_NULL,
   ACTOR_EFFECT,
   ACTOR_PLAYER,
   ACTOR_DIER,
   ACTOR_PHYSICS,
   ACTOR_VEHICLE,

   ACTOR_LAST
};
  
//----------------------------

struct S_actor_type_info{
   const char *friendly_name; //user-friendly name
   bool allow_edit_creation;  //true to enable creation in editor
};
                              //user-friendly names of actors
extern const S_actor_type_info actor_type_info[];

//----------------------------
                              //C_actor - base class for any interactive entity
                              // in a 3D scene. Each C_actor is accociated with
                              // one particular I3D_frame object.
class C_actor: public C_unknown{
   E_ACTOR_TYPE actor_type;
                              //send message to master onlu from time to time
protected:
   C_smart_ptr<C_table> tab;
   class C_game_mission &mission;
                              //frame which represents this actor in 3D, 
                              // we're keeping ref of this
                              //Note: PC_actor (without keeping ref) is stored in
                              // frame's user value, which is a pointer to S_frame_info
   C_smart_ptr<I3D_frame> frame;
   mutable S_vector loc_center_pos;
   mutable bool center_pos_valid;

   C_actor(C_game_mission&, PI3D_frame, E_ACTOR_TYPE at);

public:
//----------------------------
// Get actor's table.
   inline CPC_table GetTable() const{ return tab; }

//----------------------------
// Setup actors children volumes info: category/collide bits.
// This also sets up owner of all volumes to be the actor's frame.
   void SetupVolumes(dword category_bits, dword collide_bits = 0xffff);

//----------------------------
public:
   ~C_actor();

   inline E_ACTOR_TYPE GetActorType() const{ return actor_type; }

   const C_str &GetName() const{ return frame->GetName(); }

//----------------------------
// Return the frame on which this actor operates. This is always non-NULL valid frame.
   inline PI3D_frame GetFrame(){ return frame; }
   inline CPI3D_frame GetFrame() const{ return frame; }
   inline PI3D_model GetModel(){ assert(frame->GetType()==FRAME_MODEL); return I3DCAST_MODEL(frame); }
   inline CPI3D_model GetModel() const{ assert(frame->GetType()==FRAME_MODEL); return I3DCAST_CMODEL(frame); }

   virtual void Tick(const struct S_tick_context&){}

//----------------------------
// Called in call to C_game_mission::GameBegin, to initialize in-game items of actor (icons, etc).
   virtual void GameBegin();

//----------------------------
// Called by C_game_mission::GameEnd.
   virtual void GameEnd();

//----------------------------
// Report error in actor to log, display actor's name and type, and error message.
   void ReportActorError(const char*) const;

//----------------------------
// Get center position of actor's frame, in world coordinates.
   virtual const S_vector GetCenterPos() const;

//----------------------------
// Called when script sends signal to actor.
   virtual void OnSignal(int signal_id);

//----------------------------
// Enable/disable actor.
   virtual void Enable(bool on_off);
   virtual bool IsEnabled() const{ return true; }

//----------------------------
// Check if actor is destroyed. For humans it means death, for other actors it's broken
// state, for some actors it's not defined (false).
   virtual bool IsDestroyed() const{ return false; }

//----------------------------
// Setup sector of actor's frame. Overridable version.
   virtual bool SetFrameSector();

//----------------------------
// Watch - test collision between 2 3D points.
// If a collision with actor is detected and the actor is friend of us,
// he's asked to move out of way - in that case the func returns false (no obstacle in way).
// 'move_necessity' is a value in range 0.0 ... 1.0
// The returned value is the obstacle in the way, or NULL if way is free.
// Note: this function is implemented and utilized by AI actor types (human, enemy).
   virtual CPI3D_frame WatchObstacle(const S_vector &from,
      const S_vector &to, float move_necessity = 1.0f, S_vector *ret_norm = NULL, float *ret_dist = NULL);

//----------------------------
   virtual void Die(C_actor *killer = NULL){ ReportActorError("Die() called"); }

//----------------------------
// Get table template associated with actor.
   virtual const C_table_template *GetTableTemplate() const{ return NULL; }

//----------------------------
// Create and assign table from template. This can't be done in C_actor constructor,
// since virtual table of child class is not set up yet (can't access GetTableTemplate).
   void AssignTableTemplate();
};

typedef C_actor *PC_actor;
typedef const C_actor *CPC_actor;

//----------------------------
//----------------------------
                              //"Effect"

enum{
   EFFECT_AUTO_DESTROY = 1,   //destroy when all anims/particles/sounds are gone
   EFFECT_FADE_OFF = 2,       //destroy when faden off
   EFFECT_ANIM_SCALE = 8,     //animate scale in desired speed
};

struct S_effect_init{
                              //initial position, rotation and scale, in world coords, and link-to frame
   S_vector pos;
   S_quat rot;
   float scale;
   PI3D_frame link_to;        //if NULL, it's linked to sector it's located in

                              //particle model filename
                              // (applied only if frame was created by actor - not provided by caller)
   const char *particle_name;
   dword mode_flags;          //combination of EFFECT_? values

                              //EFFECT_FADE_OFF parameters: time to fade on, wait before fade, and actual fade-off time
                              // (when all are zero, using defaults)
   int fade_come, fade_stay, fade_leave;

                              //EFFECT_ANIM_SCALE parameters: change in scale per second
   float scale_add;

   S_effect_init(){
      memset(this, 0, sizeof(S_effect_init));
      rot.Identity();
      scale = 1.0f;
   }
};

//----------------------------
//----------------------------

#endif
