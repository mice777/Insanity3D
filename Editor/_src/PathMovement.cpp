#include "all.h"
#include "systables.h"
#include "checkpoint.h"
#include "actors.h"
#include "gamemission.h"
#include <sortlist.hpp>

//----------------------------
//----------------------------

#define SHORTCUT_TEST_INTERVAL 2.0f //distance after which we re-test for shortcut
#define SHORTCUT_TEST_TIME 300      //how often we check for possible shortcuts
//#define NEXT_CP__TEST_INTERVAL 2  //how often we check if next CP is seen
#define IDLE_DIST_INTERVAL .3f
#define NO_NEXT_CP_SEEN_TIME 1000   //thresh time for which not seeing next CP is accepted
#define MOVE_TIME_IDLE 2000         //time of no progress in movement after which we make new decissions
#define PENAULY_FOR_UNSKIPPING 2500 //how long we can't consider make shortcut, if precious decission failed
                              //debugging:

#ifdef _DEBUG

# define PM_DEBUG(n)
//# define PM_DEBUG(n) LOG(n)
//#define DEBUG_DRAW_MOVE_PATH  //draw lines for debugging of pathfinding and AI movement

#else
 #define PM_DEBUG(n)
#endif

//----------------------------
// Get angle among 2 2D vectors.
float Angle2Da(const S_vector2 &v1, const S_vector2 &v2){

   float f1, f2;
   f1 = GetAngle2D(v1.x, v1.y);
   f2 = GetAngle2D(v2.x, v2.y);
   float f = f2 - f1;
   if(f>PI)
      f -= (PI*2.0f);
   else
   if(f <= - PI)
      f += (PI*2.0f);
   return f;
}

//----------------------------
                              //movement class:
                              // responsible for finding path,
                              // keeping path,
                              // keeping failed checkpoints during path moving
class C_path_movement_imp: public C_path_movement{

   C_game_mission &mission;
   PI3D_frame frm_watch;      //frame which is looking
   dword ct_flags;            //combination of CPT_??? flags, which specify connection types we accept

                              //set of CPs which can't be selected as sources (set when pathfinding fails)
   set<CPC_checkpoint> invalid_sources;
                              //set of connections, which can't be used (set when pathfinding fails)
   struct S_cp_pair{
      CPC_checkpoint first, second;
      bool operator <(const S_cp_pair &p) const{
         return (first<p.first ||
            (first==p.first && second<p.second));
      }
   };
   set<S_cp_pair> invalid_connections;

   bool valid;

   typedef C_vector<CPC_checkpoint> t_move_path;
                              //preprocessed values:
   t_move_path move_path;     //path of checkpoints we plan to move on
                              //current index on the path (in range from 0 to num_cpts)
                              // it is index to next CP we're running to, or equals to num_cpts if going to dest
   int curr_indx;

   S_vector next_dst;         //next destination - a position of CP or dest_pos
   S_vector curr_pos;         //current position of moving object
   S_vector dest_pos;         //destination of path

                              //idle testing
   S_vector last_idle_test_pos;     //position when we last checked to see next dest
   int idle_count;            //counter for no progress

                              //next CP watch testing
   S_vector last_ncp_test_pos;//position when we last looked to the next CP
   int no_next_cp_see_count;  //counter for not seeing next CP

                              //shortchut testing
   int next_cp_seen_count;    //counter for checking view to next CP
   enum E_SHORTCUT_TEST_PHASE{
      ST_HEAD,
      ST_FEETS,
   } shortcut_test_phase;
   bool last_skipped;         //true if we took shortcut recently
   int skip_disable_count;    //countdown for which we can't consider shortcuts
   S_vector last_sc_test_pos; //position when we last checked for shortcut
   int last_sc_test_time;     //time since we last time made shortcut test - as this is tied to last_sc_test_pos so we may detect if CP is seen, but unreachable

   bool first_run;            //true until GetMovePos is called first time

                              //simple pathfinding only (direct path, no checkpoints):
   float last_trap_detect_dist;  //distance to destination position

//----------------------------
                              //forbidden functions
   C_path_movement_imp &operator =(const C_path_movement_imp &mv);
   C_path_movement_imp(const C_path_movement_imp&);

//----------------------------

   void AddInvalidConnection(CPC_checkpoint cp1, CPC_checkpoint cp2){

      if(cp1<cp2)
         swap(cp1, cp2);
      S_cp_pair cpp = {cp1, cp2};
      invalid_connections.insert(cpp);
   }

//----------------------------

   bool IsInvalidConnection(CPC_checkpoint cp1, CPC_checkpoint cp2) const{

      if(!invalid_connections.size())
         return false;
      if(cp1<cp2)
         swap(cp1, cp2);
      S_cp_pair cpp = {cp1, cp2};
      return (invalid_connections.find(cpp)!=invalid_connections.end());
   }

//----------------------------
// Internal callback 'wave' function.
   void FindPathWave(const C_vector<C_smart_ptr<C_checkpoint> > &cpts, C_vector<CPC_checkpoint> &wave1,
      CPC_checkpoint cp_to, CPC_checkpoint cp_start){

      C_vector<CPC_checkpoint> new_wave;

      for(int i=wave1.size(); i--; ){
         CPC_checkpoint cd = wave1[i];
         if(cd == cp_to)
            continue;

                              //process all connected CP's
         for(int j=cd->NumConnections(); j--; ){
            const C_checkpoint::S_connection &cd_con = cd->GetConnection(j);

                              //check if type matches
            dword ct = cd_con.type;
            if(!(ct&ct_flags))
               continue;

            CPC_checkpoint cd1 = cd_con.con_cp;

            float dist = cd->curr_distance_help + cd_con.distance;

                              //add cost of opening doors, using ladders, etc
#if 0//H&D1
            if(flags&CP_CON_USE_LADDER) dist += 8.0f;
            if(flags&CP_CON_USE_DOOR) dist += 5.0f;
#endif//H&D1

                              //if dist is worse than what we already have found, fast quit
            if(cp_start->cp_prev_help && cp_start->curr_distance_help <= dist) 
               continue;

                              //if dist is worse than this cp has already assigned, fast quit
            if(cd1->cp_prev_help && cd1->curr_distance_help <= dist)
               continue;
                              //check if this connection is in our invalid connection list
            if(IsInvalidConnection(cd, cd1))
               continue;

                              //disconnect
            if(cd1->cp_prev_help){
               dword k;
               for(k=i+1; k<wave1.size(); k++)
               if(wave1[k] == cd1){
                  wave1[k] = wave1.back(); wave1.pop_back();
                  break;
               }
               for(k=0; k<new_wave.size(); k++)
               if(new_wave[k] == cd1){
                  new_wave[k] = new_wave.back(); new_wave.pop_back();
                  break;
               }
            }
                              //connect to this
            cd1->cp_prev_help = cd;
            cd1->curr_distance_help = dist;
                              //add to new wave
            new_wave.push_back(cd1);
         }
      }
      if(new_wave.size())
         FindPathWave(cpts, new_wave, cp_to, cp_start);
   }

//----------------------------
// Find connection between 2 specified checkpoints in provided CP grid.
   bool FindPathInternal(CPC_checkpoint cp_from, CPC_checkpoint cp_to, float *dist = NULL){

      if(cp_from==cp_to){
         assert(0);
         move_path.push_back(cp_to);
         if(dist)
            *dist = 0.0f;
         return true;
      }
      const C_vector<C_smart_ptr<C_checkpoint> > &cpts = mission.GetCheckpoints();

      int i;
      for(i=cpts.size(); i--; )
         cpts[i]->cp_prev_help = NULL;

      C_vector<CPC_checkpoint> wave;
                              //search from end to start
      wave.push_back(cp_to);
      cp_to->curr_distance_help = 0.0f;
      cp_to->cp_prev_help = cp_from;
      FindPathWave(cpts, wave, cp_from, cp_from);

      if(!cp_from->cp_prev_help)
         return false;
                              //collect path
      CPC_checkpoint cp_p = cp_from;
      while(true){
         move_path.push_back(cp_p);
         if(cp_p == cp_to)
            break;
         cp_p = cp_p->cp_prev_help;
      }
                              //compute size
      if(dist)
         *dist = move_path.front()->curr_distance_help;

      return true;
   }

//----------------------------

   virtual bool FindPath(CPC_checkpoint c0, CPC_checkpoint c1, float &dist){

      move_path.clear();
      return FindPathInternal(c0, c1, &dist);
   }

//----------------------------
// Get position of next CP, or NULL if we're at the end.
   const S_vector *GetNextCPPosition(){

      if(curr_indx == (int)move_path.size())
         return NULL;
      if(curr_indx == (int)move_path.size()-1)
         return &dest_pos;
                              //this assert triggers, debug it!!!
      assert(curr_indx<(int)move_path.size());
      if(curr_indx >= (int)move_path.size())
         return NULL;
      return &move_path[curr_indx+1]->GetPos();
   }

//----------------------------
// Set pathfinder to next CP.
// This func also sets 'next_dst' to either next CP, or destination position.
// This func returns true if we're already at last CP of the path.
   bool SetToNextCP(){

                              //test head next time
      shortcut_test_phase = ST_HEAD;
      if(curr_indx == (int)move_path.size()){
         PM_DEBUG("SetToNextCP: in last CP");
         return true;
      }
                              //check connection validity
      dword ct = GetNextCPConType();
      if(ct!=CPT_NULL){
         if(!(ct&ct_flags)){
            ++curr_indx;
            return false;
         }
      }
      if(++curr_indx == (int)move_path.size()){
         PM_DEBUG("SetToNextCP: last CP, going to dest");
         next_dst = dest_pos;
      }else{
         next_dst = move_path[curr_indx]->GetPos();
      }
      return true;
   }

//----------------------------
// Recompute path, if path failed. This involves keeping failed connections
// in internal list and not using them anymore.
   bool Recompute(PI3D_frame actor_frame){

      PM_DEBUG("recomputing");

      bool b = false;
      if(curr_indx<0){
         assert(0);
      }else
      if(curr_indx == 0){
         if(move_path.size()){
            CPC_checkpoint cp_failed = move_path[0];
            PM_DEBUG("marking CP not to be source");
            invalid_sources.insert(cp_failed);
            move_path.clear();
         }
         b = FindPath(curr_pos, dest_pos, actor_frame, false, 0.0f);
         if(b && !move_path.size())
            return false;
      }else{
         CPC_checkpoint cp_src, cp_failed, cp_dest;
         if(curr_indx >= (int)move_path.size()){
            if(move_path.size()<2)
               return false;
            if(curr_indx > (int)move_path.size())   //shouldn't happen, but does :( (no time to find out why)
               return false;

            cp_src = move_path[curr_indx-2];
            cp_failed = move_path[curr_indx-1];
         }else{
            cp_src = move_path[curr_indx-1];
            cp_failed = move_path[curr_indx];
         }
         cp_dest = move_path[move_path.size()-1];

         move_path.clear();
                              //remove failed connection
         PM_DEBUG("removing failed connection");
         AddInvalidConnection(cp_src, cp_failed);

         b = FindPathInternal(cp_src, cp_dest);
      }
      if(b){
         assert(move_path.size());
         curr_indx = 0;
         next_dst = move_path[0]->GetPos();
         last_skipped = false;
         skip_disable_count = 0;
         last_sc_test_pos = curr_pos;
         next_cp_seen_count = 0;
         last_sc_test_time = 0;
         shortcut_test_phase = ST_HEAD;
         first_run = true;
         last_ncp_test_pos = curr_pos;
         no_next_cp_see_count = 0;
         return true;
      }
      PM_DEBUG("can't recompute, quitting");
      valid = false;
      return false;
   }

//----------------------------
// Unskip last taken shortcut, and set penaulty.
   void UnskipShortcut(){

      assert(last_skipped && curr_indx!=-1);
      PM_DEBUG("unskip shortcut");
                              //unskip point and set penaulty
      skip_disable_count = PENAULY_FOR_UNSKIPPING;
      --curr_indx;
      next_dst = move_path[curr_indx]->GetPos();
      idle_count = 0;
      last_skipped = false;
      shortcut_test_phase = ST_HEAD;
   }

//----------------------------
// Check resulting feet collision normal and determine if we might be able
// to skip it.
   inline static bool CheckFeetColNormal(float col_y){
      return (col_y >= .6f);
   }

//----------------------------
// Get feet position of human, specified by 'root' frame. The feet position is put some
// distance above actual feet position, as well as position of given checkpoint is lowered
// to be on the same level as feets.
   static void GetFeetPos(PI3D_frame root, S_vector &feet_pos, S_vector &cp_pos){

      const S_matrix m_root = root->GetMatrix();
      feet_pos = m_root(3);
      feet_pos += m_root(1) * .2f;
      cp_pos -= m_root(1) * .8f;
   }

//----------------------------
// Get flags of connection following next CP.
   dword GetNextCPConType() const{

      if(curr_indx < (int)move_path.size()-1){
         CPC_checkpoint cp0 = move_path[curr_indx];
         CPC_checkpoint cp1 = move_path[curr_indx+1];
         for(int i=cp0->NumConnections(); i--; )
            if(cp0->GetConnection(i).con_cp==cp1)
               break;
         assert(i!=-1);
         if(i!=-1)
            return cp0->GetConnection(i).type;
      }
      return CPT_NULL;
   }

//----------------------------
// Check if view clear from actor to CP specified by 'dest_cp'.
// This function checks head and feet collisions.
   bool CheckIfViewObstructed(PC_actor actor, PI3D_frame frm_watch,
      const S_matrix &m_inv_root, const S_vector &dest_cp){

      const S_vector &src = frm_watch->GetWorldPos();
      S_vector dest = dest_cp;

                              //detect collision from eyes (watch frame) to CP directly
#ifdef DEBUG_DRAW_MOVE_PATH
      DebugLine(src, dest, 2, 0xff00ff00, 1500);
#endif
      float col_dist;
      bool b = actor->WatchObstacle(src, dest, 1, NULL, &col_dist);
      if(b){
         float ratio = col_dist / (src-dest).Magnitude();
         if(ratio > .8f)
            b = false;
      }
      if(!b){
                              //way free, detect from feets to some distance above terrain at CP position
         S_vector feet_pos;
         GetFeetPos(actor->GetFrame(), feet_pos, dest);
#ifdef DEBUG_DRAW_MOVE_PATH
         DebugLine(feet_pos, dest, 2, 0xff00ff00, 1500);
#endif
         S_vector norm;
         b = actor->WatchObstacle(feet_pos, dest, 1, &norm, &col_dist);
         if(b){
            float ratio = col_dist / (feet_pos-dest).Magnitude();
            if(ratio > .8f)
               b = false;
         }
         if(b){
                              //collision detected, check returned normal, if it's ok for walking on
            b = !CheckFeetColNormal((norm%m_inv_root).y);
         }
      }
      return b;
   }

//----------------------------
public:
   C_path_movement_imp(C_game_mission &mission1, PI3D_frame frm_watch1, dword ct_flags1):
      mission(mission1),
      frm_watch(frm_watch1),
      ct_flags(ct_flags1),
      curr_indx(-1),
      idle_count(0),
      no_next_cp_see_count(0),
      next_cp_seen_count(0),
      shortcut_test_phase(ST_HEAD),
      last_skipped(false),
      skip_disable_count(0),
      last_sc_test_time(0),
      first_run(true),
      last_trap_detect_dist(0),
      valid(true)
   {}

//----------------------------
// Find path between two points. Both positions are specified at the floor level.
   virtual bool FindPath(const S_vector &from, const S_vector &to, PI3D_frame actor_frame,
      bool debug_draw_path = false, float direct_path_thresh = 2.0f, float max_distance_ratio = 3.0f){

#ifdef DEBUG_DRAW_MOVE_PATH
      debug_draw_path = true;
#endif
#ifdef EDITOR
      if(debug_draw_path)
         DebugLineClear();
#endif
      const C_vector<C_smart_ptr<C_checkpoint> > &cpts = mission.GetCheckpoints();

      //const S_matrix &mat_player = actor_frame->GetMatrix();

      static const S_vector down_dir(0, -5, 0);

      S_vector src_pos = from;
      dest_pos = to;
                              //put both points above terrain
      src_pos.y += 1.0f;
      dest_pos.y += 1.0f;

                              //fall destination down, 1m above collision
      {
         I3D_collision_data cd(dest_pos, S_vector(0, -3.0f, 0), I3DCOL_LINE);
         cd.frm_ignore = actor_frame;
         //DebugLine(cd.from, cd.from+cd.dir, 0);
         if(mission.TestCollision(cd)){
            dest_pos = cd.ComputeHitPos();
            dest_pos.y += 1.0f;
         }
      }
      //DebugPoint(src_pos, .2f, 0, 0xffff0000); DebugPoint(dest_pos, .2f, 0, 0xff00ff00);

      move_path.clear();

      float direct_dist = (src_pos - dest_pos).Magnitude();
#if 0
      DebugLine(src_pos, dest_pos, 1);
      LOG(direct_dist);
#endif
                              //find closest visible check points
      dword num_cp = cpts.size();
      bool direct_path = (!num_cp);
      if(!direct_path && direct_path_thresh && direct_dist < direct_path_thresh)
         direct_path = true;

                              //find closest checkpoints to given start/end points
      CPC_checkpoint cp_from = NULL, cp_to = NULL;

      if(!direct_path){

                              //sort cpts by dist to from/to position
         for(int j=0; j<2; j++){
            C_sort_list<CPC_checkpoint> sl_dist;
            S_vector v(!j ? src_pos : dest_pos);

            for(int i=num_cp; i--; ){
               CPC_checkpoint cp = cpts[i];
               if(cp->GetType() != CP_PATH)
                  continue;
                                 //check if this cp may be selected as source
               if(invalid_sources.find(cp)!=invalid_sources.end())
                  continue;

               const float MAX_DIST = 25.0f;
               float dist2 = (cp->GetPos() - v).Square();
               if(dist2 < MAX_DIST*MAX_DIST)
                  sl_dist.Add(cp, I3DFloatAsInt(dist2));
            }
            if(!sl_dist.Count())
               return false;
            sl_dist.Sort();
            {
                                 //use closest visible checkpoint
               int indx = -1;
               const int MAX_CP_SEEN_TEST = 32;
                                 //check a few closest CPs, if none is seen, use the closest one
               for(i=0; i<Min(MAX_CP_SEEN_TEST, (int)sl_dist.Count()); i++){
                  CPC_checkpoint cp = sl_dist[i];
                                 //check if CP visible
                  S_vector rpos;
                  S_vector v1 = v;
                  S_vector dir = v-cp->GetPos();
                  if(j){
                                 //move towards CP
                     float dist2 = dir.Square();
                     const float move_dist = .4f;
                     if(dist2 > (move_dist*move_dist)){
                        float dist = I3DSqrt(dist2);
                        dir /= dist;
                        v1 += dir*move_dist;
                        dir *= (dist-move_dist);
                     }
                  }
                  I3D_collision_data cd(cp->GetPos(), dir, I3DCOL_LINE);
                  cd.frm_ignore = actor_frame;
                  if(!mission.TestCollision(cd)){
                     indx = i;
                     break;
                  }
               }
                                 //no seen? use first one
               if(indx==-1)
                  indx = 0;
               (!j ? cp_from : cp_to) = sl_dist[indx];
            }
         }
         if(cp_from && cp_to && cp_from==cp_to)
            direct_path = true;
      }

      if(direct_path){
                              //direct path, not using checkpoints
         curr_indx = 0;
         next_dst = dest_pos;
         first_run = true;
         curr_pos = src_pos;
         next_cp_seen_count = 0;
         shortcut_test_phase = ST_HEAD;
         no_next_cp_see_count = NO_NEXT_CP_SEEN_TIME;
         //last_trap_detect_pos = curr_pos;
         last_trap_detect_dist = Max(0.0f, (dest_pos - src_pos).Magnitude() - .5f);
         return true;
      }

      bool ok = false;
      curr_indx = -1;
      if(cp_from && cp_to){
         float path_dist;
         ok = FindPathInternal(cp_from, cp_to, &path_dist);

         if(ok){

         PM_DEBUG(C_fstr("Findpath: from [%.2f, %.2f, %.2f], to[%.2f, %.2f, %.2f]",
            from.x, from.y, from.z, to.x, to.y, to.z));
         PM_DEBUG(C_fstr("%i keys", move_path.size()));
                              //check ratio against direct path
         if(!IsMrgZeroLess(direct_dist)){
            float ratio = path_dist / direct_dist;
            PM_DEBUG(C_fstr("Findpath: distance ratio = %.2f", ratio));
            if(ratio > max_distance_ratio)
               return false;
         }

         curr_indx = 0;
         next_dst = move_path[0]->GetPos();
         last_skipped = false;
         skip_disable_count = 0;
         last_sc_test_pos = src_pos;
         curr_pos = src_pos;
         next_cp_seen_count = 0;
         first_run = true;
         last_sc_test_time = 0;
         shortcut_test_phase = ST_HEAD;
         last_ncp_test_pos = curr_pos;
         no_next_cp_see_count = 0;
         last_idle_test_pos = curr_pos;
         idle_count = 0;
                              //adjust destination point, so that it's reachable
         {
            const S_vector &last_cp = move_path.back()->GetPos();
            S_vector dir = last_cp - dest_pos;
            if(dir.Magnitude() < .4f){
               PM_DEBUG(C_fstr("Findpath: dest close to CP, using CP"));
               dest_pos = last_cp;
            }else{

               I3D_collision_data cd(last_cp, dest_pos-last_cp, I3DCOL_LINE);
               cd.frm_ignore = actor_frame;
               //DebugLine(cd.from, cd.from+cd.dir, 0);
               bool col = mission.TestCollision(cd);
               if(col){
                  dir.Normalize();
                  dest_pos += dir*.4f;
               }
               {
                              //fall it down
                  I3D_collision_data cd(dest_pos, down_dir, I3DCOL_LINE);
                  cd.frm_ignore = actor_frame;
                  //DebugLine(cd.from, cd.from+cd.dir, 0);
                  col = mission.TestCollision(cd);
                  if(col){
#ifdef EDITOR//DEBUG_DRAW_MOVE_PATH
                     if(debug_draw_path)
                        DebugLine(dest_pos, cd.ComputeHitPos(), 0, 0x4000ff00);
#endif
                     dest_pos = cd.ComputeHitPos();
                     dest_pos.y += 1.0f;
                     //DebugPoint(dest_pos, .3f, 0);
                     PM_DEBUG(C_fstr("Findpath: dest adjusted to dist %.2f from last CP", (dest_pos-last_cp).Magnitude()));

                  }else{
                     PM_DEBUG(C_fstr("Findpath: dest in air, using CP"));
#ifdef EDITOR//DEBUG_DRAW_MOVE_PATH
                     if(debug_draw_path)
                        DebugLine(dest_pos, dest_pos + down_dir, 0, 0xc0ff0000);
#endif
                                 //oops, nothing under feets
                     dest_pos = last_cp;
                  }
               }
            }
         }
#ifdef EDITOR//DEBUG_DRAW_MOVE_PATH
         if(debug_draw_path){
            DebugLine(src_pos, move_path[0]->GetPos(), 0, 0xc0ff0000);
            for(dword i=0; i<move_path.size()-1; i++){
               DebugLine(move_path[i]->GetPos(),
                  move_path[i+1]->GetPos(), 0, 0xc0ffff00);
            }
            DebugLine(move_path.back()->GetPos(), dest_pos, 0, 0xc00000ff);
         }
#endif
         }
      }
      return ok;
   }

//----------------------------

public:

//----------------------------
// Get next position to move to, return true if reached.
   virtual E_MOVEMENT_STATUS GetMovePos(S_vector &ret, S_vector &next_dest, int time,
      PC_actor actor, float desired_move_speed, bool timeouts = true){

      if(!valid)
         return MS_INVALID;
      PI3D_frame actor_frame = actor->GetFrame();

      int i;
                              //go to the next key
      const S_matrix &m_inv_root = actor_frame->GetInvMatrix();
      S_vector next_dst_loc = next_dst * m_inv_root;
      S_vector curr_pos_loc = curr_pos * m_inv_root;
                              //get dist
      float dist_2d;
      {
                              //get checkpoint positions in human's local coords (2d)
         S_vector2 dir(next_dst_loc.x-curr_pos_loc.x, next_dst_loc.z-curr_pos_loc.z);
         dist_2d = dir.Magnitude();
      }
      if(dist_2d<1.2f && I3DFabs(next_dst_loc.y-curr_pos_loc.y)<2.0f){
         bool next = false;
         const S_vector *next_cp_pos = GetNextCPPosition();
         if(next_cp_pos){
            if(dist_2d>=.7f || I3DFabs(next_dst_loc.y-curr_pos_loc.y)>=2.0f){
               last_sc_test_time += time;
               if(last_sc_test_time > SHORTCUT_TEST_TIME){
                  last_sc_test_time = 0;

                  if(!CheckIfViewObstructed(actor, frm_watch, m_inv_root, *next_cp_pos)){
                     PM_DEBUG("head & feets ok, going to next");
                     next = true;
                  }
               }
            }else
               next = true;
         }else
         if(dist_2d<.7f && I3DFabs(next_dst_loc.y-curr_pos_loc.y)<2.0f){
            PM_DEBUG("done");
            ret = dest_pos;
            next_dest = dest_pos;
            return MS_DEST_REACHED;
         }
         if(next){
                              //check special flags
#if 0//H&D1
            dword cflg = GetNextCPConFlags();
            if(cflg&CP_CON_USE_LADDER){
               PM_DEBUG("ladder connection on next CP");
               *use_lad_door = 1;
            }else
            if(cflg&CP_CON_USE_DOOR){
               PM_DEBUG("door connection on next CP");
               *use_lad_door = 2;
            }
#endif//H&D1
            if(!SetToNextCP()){
               if(!Recompute(actor_frame))
                  return MS_INVALID;
               return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
            }
            last_skipped = false;
            skip_disable_count = 0;
            goto out1;
         }
      }

      if(!move_path.size()){
                              //simple movement without CPs
         ret = dest_pos;
         next_dest = dest_pos;

                              //we must check under-feet collisions time-to-time
         const float min_check_dist = .5f;

                              //check how far we're currently to destination
         float curr_dest = (dest_pos - curr_pos).Magnitude();
         float test_dest = curr_dest - min_check_dist * 2.0f;
                              //also consider current speed
         test_dest -= desired_move_speed * .4f;
         test_dest = Max(0.0f, test_dest);

                              //detect collisions under feets not tested yet in some distance ahead
         if(last_trap_detect_dist > test_dest){
            S_normal dir = dest_pos - curr_pos;
            I3D_collision_data cd;
            cd.from = dest_pos - dir * last_trap_detect_dist;
            cd.from.y -= .5f;
            cd.dir = S_vector(0, -2, 0);
            cd.frm_ignore = actor_frame;
            cd.flags = I3DCOL_LINE;

            while(last_trap_detect_dist > test_dest){
               //DebugLine(cd.from, cd.from+cd.dir, 0);
               if(!mission.TestCollision(cd)){
                              //problem - hole, stop further movement
                  //if(!Recompute(actor_frame))
                     return MS_INVALID;
                  //return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
               }
               dword mat_style = tab_materials->ItemE(TAB_E_MAT_CATEGORY, GetMaterialID(cd.GetHitFrm()));

               float angle_cos = cd.GetHitNormal().y;
               const float MIN_NORMAL_Y = .75f;
               const float MIN_STAIR_NORMAL_Y = .5f;

               bool is_stairs = (mat_style == MATSTYLE_STAIRS);
               if(angle_cos < (is_stairs ? MIN_STAIR_NORMAL_Y : MIN_NORMAL_Y)){
                              //too steep surface, don't step it
                  return MS_INVALID;
               }


               //PRINT(cd.GetHitDistance());
               cd.from.y += .5f - cd.GetHitDistance();
               cd.from += dir * min_check_dist;
                              //adjust source y by hit position
               last_trap_detect_dist -= min_check_dist;
            }
         }
                              //also check view to the destination
         no_next_cp_see_count += time;
         if(timeouts && no_next_cp_see_count >= NO_NEXT_CP_SEEN_TIME/2){
            bool b = CheckIfViewObstructed(actor, frm_watch, m_inv_root, next_dst);
            if(b){
               PM_DEBUG("not seeing dest");
               return MS_INVALID;
            }
            no_next_cp_see_count = 0;
         }
         goto out;
      }
                              //check if we see next dest
      {
         bool b = true;
         bool do_look = false;
         int no_see_timeout = NO_NEXT_CP_SEEN_TIME;
         if(!last_skipped) no_see_timeout *= 2;

         if(timeouts && (no_next_cp_see_count+time) >= no_see_timeout){
            do_look = true;
            last_ncp_test_pos = curr_pos;
         }else{
            float last_watch_dist = (last_ncp_test_pos - curr_pos).Magnitude();
                              //watch more often if not seen this for long
            const float watch_dist_thresh =
               ((no_next_cp_see_count+time) < NO_NEXT_CP_SEEN_TIME) ? 2.0f : .75f;
            if(last_watch_dist > watch_dist_thresh){
               last_ncp_test_pos = curr_pos;
               do_look = true;
            }
         }
         if(do_look){
            b = CheckIfViewObstructed(actor, frm_watch, m_inv_root, next_dst);
         }
         if(b){               //view not clear
            if(timeouts && (no_next_cp_see_count+=time) >= no_see_timeout){
               PM_DEBUG("not seeing next CP for long");
                           //locked...?
               if(last_skipped){
                  UnskipShortcut();
               }else
               if(!Recompute(actor_frame))
                  return MS_INVALID;
               return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
            }
         }else
            no_next_cp_see_count = 0;
      }

      if(skip_disable_count && (skip_disable_count-=time)<=0){
         skip_disable_count = 0;
         PM_DEBUG("skip penaulty expired");
      }

      if(skip_disable_count){
         if((skip_disable_count-=time) <= 0){
            skip_disable_count = 0;
            PM_DEBUG("skip penaulty expired");
         }

         float dist = (last_sc_test_pos - curr_pos).Magnitude();
         if(dist >= SHORTCUT_TEST_INTERVAL){
            last_sc_test_pos = curr_pos;
            last_sc_test_time = 0;
         }else
         if((last_sc_test_time += time) >= MOVE_TIME_IDLE){
            if(!Recompute(actor_frame))
               return MS_INVALID;
            return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
         }
      }else
      if(curr_indx < (int)move_path.size()){

                              //check if it's time to test for shortcut
         float dist_from_last_test = (last_sc_test_pos - curr_pos).Magnitude();
         const float shortcut_test_dist = Min(5.0f, desired_move_speed) * ((float)SHORTCUT_TEST_TIME*.001f);
         if(first_run || dist_from_last_test >= shortcut_test_dist){
            last_sc_test_pos = curr_pos;
            last_sc_test_time = 0;  //reset (idle) counter

            bool do_test = true;

            dword next_con_type = CPT_DEFAULT;
                                 //get next connection's type
            if(curr_indx < (int)move_path.size()-1){
               CPC_checkpoint cp0 = move_path[curr_indx];
               CPC_checkpoint cp1 = move_path[curr_indx+1];
               const C_checkpoint::S_connection &con = cp0->GetConnection(cp0->GetConnectionIndex(cp1));
               next_con_type = con.type;
            }
                                 //check if this connection type may be skipped
            if(next_con_type!=CPT_DEFAULT)
               do_test = false;

            if(do_test && last_skipped){
               bool b = CheckIfViewObstructed(actor, frm_watch, m_inv_root, next_dst);
                              //check if we still see next key
               if(b){
                  UnskipShortcut();
                  return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
               }else{
                              //check distance
                  if(!curr_indx)
                     do_test = false;
                  else{
                     float dist_to_next = (next_dst - curr_pos).Magnitude();
                     float skiped_dist = (next_dst - move_path[curr_indx-1]->GetPos()).Magnitude();
                     if(dist_to_next >= skiped_dist)
                        do_test = false;
                     else{
                              //check angle
                        S_vector next_dir(next_dst - curr_pos);
                        S_vector skipped_dir(next_dst - move_path[curr_indx-1]->GetPos());
                        //DebugLine(next_dst, curr_pos, 0, 0xffff00ff);
                        //DebugLine(next_dst, move_path[curr_indx-1]->GetPos(), 0, 0xffff00ff);
                        float angle = Angle2Da(S_vector2(next_dir.x, next_dir.z), S_vector2(skipped_dir.x, skipped_dir.z));
                        assert(angle <= PI);
                        if(I3DFabs(angle) > PI*.5f)
                           do_test = false;
                     }
                  }
               }
            }
            if(do_test){
               S_vector next_cp_pos = *GetNextCPPosition();
               i = first_run;
               bool b;
               do{            //for first time always check both head & feets
                  if(shortcut_test_phase==ST_HEAD){
                              //head test
#ifdef DEBUG_DRAW_MOVE_PATH
                     DebugLine(frm_watch->GetWorldPos(), next_cp_pos, 2, 0xffff0000, 1500);
#endif
                     b = actor->WatchObstacle(frm_watch->GetWorldPos(), next_cp_pos);
                  }else{
                              //feet test
                     S_vector norm;
                     S_vector feet_pos;
                     GetFeetPos(actor_frame, feet_pos, next_cp_pos);
#ifdef DEBUG_DRAW_MOVE_PATH
                     DebugLine(feet_pos, next_cp_pos, 2, 0xffff0000, 1500);
#endif
                     b = actor->WatchObstacle(feet_pos, next_cp_pos, 1, &norm);
                     if(b){
                        b = !CheckFeetColNormal((norm%m_inv_root).y);
                     }
                  }

                  if(!b){
                     (byte&)shortcut_test_phase ^= 1;
                     if(shortcut_test_phase==ST_HEAD){
                              //can't skip special connection type
#if 0//H&D1
                        dword cflg = GetNextCPConFlags();
                        if(!(cflg&(CP_CON_USE_LADDER|CP_CON_USE_DOOR))){
#endif//H&D1
                              //tested both head and feets, everything's ok
                           PM_DEBUG("taking shortcut");
                              //skip to next CP
                           if(!SetToNextCP()){
                              if(!Recompute(actor_frame))
                                 return MS_INVALID;
                              return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
                           }
                           last_skipped = true;
                        //}
                     }
                  }else{
                     shortcut_test_phase = ST_HEAD;
                  }
               }while(!b && i--);
            }
            first_run = false;
         }else                //check if even moving
         if(dist_from_last_test<IDLE_DIST_INTERVAL && (last_sc_test_time += time) >= MOVE_TIME_IDLE){
            PM_DEBUG("no progress for a long time, making decission");
                              //locked...?
            if(last_skipped)
               UnskipShortcut();
            else
            if(!Recompute(actor_frame))
               return MS_INVALID;
            return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
         }
      }
                              //check for idle
      if(timeouts){
         float dist2 = (last_idle_test_pos - curr_pos).Square();
         const float idle_distance = Min(5.0f, desired_move_speed) * .6f;
         //if(dist > IDLE_DIST_INTERVAL)
         if(dist2 > (idle_distance*idle_distance))
         {
            last_idle_test_pos = curr_pos;
            idle_count = 0;
         }else
         if((idle_count += time) >= MOVE_TIME_IDLE){
            PM_DEBUG("no progress for a long time, making decission");
                              //locked...?
            if(last_skipped)
               UnskipShortcut();
            else
            if(!Recompute(actor_frame))
               return MS_INVALID;
            return GetMovePos(ret, next_dest, time, actor, desired_move_speed, timeouts);
         }
      }
   out1:
                              //go to the next computed destination
      ret = next_dst;
      next_dest = (curr_indx+1) >= (int)move_path.size() ? dest_pos :
         move_path[curr_indx+1]->GetPos();
   out:

#ifdef DEBUG_DRAW_MOVE_PATH
                              //blue - direct connection from current pos to dest which we're returning
      DebugLine(curr_pos, ret, 1, 0xff0000ff);
#endif
      return MS_MOVING;
   }

//----------------------------
// Feedback - setting current position.
   virtual void SetCurrPos(const S_vector &p){
      assert(valid);
      curr_pos = p;
   }

//----------------------------

   virtual const S_vector &GetDest() const{
      assert(valid);
      return dest_pos;
   }

//----------------------------

   virtual CPC_checkpoint GetCurrCP() const{

      assert(valid);
      if(!valid)
         return NULL;
      if(!curr_indx)
         return NULL;
      return move_path[curr_indx-1];
   }
   virtual CPC_checkpoint GetNextCP() const{

      assert(valid);
      if(!valid)
         return NULL;
      if(curr_indx == (int)move_path.size())
         return NULL;
      return move_path[curr_indx];
   }

//----------------------------

   virtual dword GetCurrConType() const{

      assert(valid);
      if(!curr_indx)
         return 0;
      if(curr_indx >= (int)move_path.size())
         return 0;
      assert(curr_indx && curr_indx<=(int)move_path.size());
      CPC_checkpoint cp0 = move_path[curr_indx-1];
      CPC_checkpoint cp1 = move_path[curr_indx];
      const C_checkpoint::S_connection &con = cp0->GetConnection(cp0->GetConnectionIndex(cp1));
      return con.type;
   }
};

//----------------------------

C_path_movement *CreatePathMovement(C_game_mission &mission, PI3D_frame frm_watch, dword ct_flags){
   return new C_path_movement_imp(mission, frm_watch, ct_flags);
}

//----------------------------
//----------------------------
