#ifndef __CHECKPOINT_H
#define __CHECKPOINT_H

//----------------------------

typedef class C_checkpoint *PC_checkpoint;
typedef const C_checkpoint *CPC_checkpoint;

enum E_CHECKPOINT_TYPE{
   CP_PATH,                   //pathfinding
   CP_USER,                   //other (guard paths, etc)
};

                              //type of checkpoint connection (note: use different bits for each enum!)
enum{    
   CPT_NULL = 0,
   CPT_DEFAULT = 1,
   CPT_DISABLED = 2,
};

class C_checkpoint: public C_unknown{
                              //index of CP, computed at GameBegin
   int index;

                              //help for findpath function:
   mutable float curr_distance_help;
   mutable CPC_checkpoint cp_prev_help;

   friend class C_path_movement_imp;
   friend class C_game_mission_imp;
   friend class C_edit_checkpoint;
//----------------------------
//----------------------------
public:
   struct S_connection{
      PC_checkpoint con_cp;
      float distance;         //to connected checkpoint, computed at GameBegin
      dword type;

      S_connection():
         type(CPT_DEFAULT)
      {}
   };

#ifdef EDITOR
   mutable bool help_visited;         //help during development
#endif
private:
   C_vector<S_connection> connections;
   E_CHECKPOINT_TYPE type;
   S_vector pos;
   C_str name;

   ~C_checkpoint();
   C_checkpoint(const C_checkpoint&);
   C_checkpoint &operator =(const C_checkpoint&);
public:

   C_checkpoint():
#ifdef EDITOR
      help_visited(false),
#endif
      type(CP_PATH)
   {}

//----------------------------
// Return number of connections to other checkpoints.
   inline dword NumConnections() const{ return connections.size(); }

//----------------------------
// Connect to a checkpoint.
   void Connect(PC_checkpoint, dword = CPT_DEFAULT);

//----------------------------
// Disconnect from a checkpoint.
   void Disconnect(PC_checkpoint);

//----------------------------
// Get index of connection to a checkpoint within our connection list.
// The returned value is -1 if connection doesn't exist.
   int GetConnectionIndex(CPC_checkpoint cp) const{
      for(int i=connections.size(); i--; )
         if(connections[i].con_cp==cp) break;
      return i;
   }

//----------------------------
// Check if connection to a checkpoint exists.
   bool IsConnected(CPC_checkpoint cp) const{
      return (GetConnectionIndex(cp) != -1);
   }

//----------------------------
// Get access to connection area.
   const S_connection &GetConnection(int i) const{
      assert(i>=0 && i<(int)connections.size());
      return connections[i];
   }
   
   S_connection &GetConnection(int i){
      assert(i>=0 && i<(int)connections.size());
      return connections[i];
   }

   inline void SetPos(const S_vector &p){ pos = p; }
   inline const S_vector &GetPos() const{ return pos; }

   inline void SetType(E_CHECKPOINT_TYPE t){ type = t; }
   inline E_CHECKPOINT_TYPE GetType() const{ return type; }
   
   inline const C_str &GetName() const{ return name; }
   void SetName(const C_str &n){ name = n; }
   int GetIndex() const{ return index; }
};

//----------------------------

                              //class responsible for finding path and traversing it
                              // it makes complete finding path among 2 locations in scene,
                              // processes movement, makes shortcuts, recalculates different path
                              // if movement fails
class C_path_movement: public C_unknown{
public:
//----------------------------
// Find path between 2 points.
// Parameters:
//    from ... source position
//    to ... destination position
//    actor_frame ... frame of testing actor
//    debug_draw_path ... true to visualize found path by lines
//    direct_path_thresh ... thresh distance, below which pathfinding doesn't use checkpoints
//    max_distance_ratio ... ratio of direct distance and found path, until which path is accepted as valid
   virtual bool FindPath(const S_vector &from, const S_vector &to, PI3D_frame actor_frame,
      bool debug_draw_path = false, float direct_path_thresh = 2.0f,
      float max_distance_ratio = 3.0f) = 0;

//----------------------------
// Find raw path between 2 checkpoints.
   virtual bool FindPath(CPC_checkpoint c0, CPC_checkpoint c1, float &dist) = 0;

//----------------------------
// Get destination of current path.
   virtual const S_vector &GetDest() const = 0;

//----------------------------
   enum E_MOVEMENT_STATUS{
      MS_MOVING,              //ok, moving on
      MS_DEST_REACHED,        //destination reached
      MS_INVALID,             //invalid pathfinder - all paths failed
   };

//----------------------------
// Calculate position where to move to. This func also returns next position where movement will continue
// after reaching the position.
// Parameter 'desired_move_speed' is the speed by which user of this class would ideally move,
//    it is used to compute timeouts and other optimizations in movement.
   virtual E_MOVEMENT_STATUS GetMovePos(S_vector &ret, S_vector &next_dest, int time,
      C_actor *actor, float desired_move_speed, bool timeouts = true) = 0;

//----------------------------
// Set new position after successful movement of actor.
   virtual void SetCurrPos(const S_vector&) = 0;

//----------------------------
// Get current CP we're running to, or NULL if race is over.
   virtual CPC_checkpoint GetCurrCP() const = 0;
   virtual CPC_checkpoint GetNextCP() const = 0;

//----------------------------
// Get current checkpoint connection type, on which we currently move.
   virtual dword GetCurrConType() const = 0;
};

typedef C_path_movement *PC_path_movement;

PC_path_movement CreatePathMovement(class C_game_mission&, PI3D_frame frm_watch, dword ct_flags = CPT_DEFAULT);

//----------------------------
#endif
