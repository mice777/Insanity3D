
//----------------------------
// Description: Camera following particular object (frame) in 3D scene
// Features:     
//    - adjustable height and distance to focused frame
//    - different frames for look-at position and look-at direction allowed
//    - smooth flying animation
//    - collision with environment
//    - zooming
//    - animatins (explosion shaking, etc.)
//----------------------------

#include "all.h"
#include "game_cam.h"
#include "main.h"

//----------------------------

#define DISTANCE_ANIM_SPEED 4.0f  //speed of distance change animation
#define MIN_VIEW_DISTANCE .1f    //minimal distance beyond real distance doesn't go

#define FADE_VIEW_DIST_MIN .5f
#define FADE_VIEW_DIST_MAX .75f

//#define NEW_STYLE

#ifdef _DEBUG
//#define DEBUG_DRAW_LINES        //when defined, draw camera's movement and aim lines
//#define DISABLE_ADD_ANIM          //additional animation like shaking, etc.

# ifdef EDITOR
#  define CAM_DEBUG_MODEL
# endif

#endif

//----------------------------
                              //camera height angles, 3 phases
static const float VIEW_ANGLES[] = { PI*.0f, PI*.13f, PI*.26f };

//----------------------------

class C_game_camera_imp: public C_game_camera{
   PI3D_scene scene;

   C_smart_ptr<I3D_camera> cam;  //camera being animated by this class

                              //support for additional animiation of entire camera (shaking, etc):
   C_smart_ptr<I3D_dummy> anim_dummy;         //dummy at [0, 0, 0] for animation (camera is linked to this)
   C_smart_ptr<I3D_interpolator> anim_interp; //interpolator performing this

#ifndef NEW_STYLE
   C_smart_ptr<I3D_interpolator> fly_interp;  //for smooth fly
   C_smart_ptr<I3D_keyframe_anim> fly_anim;   //computed fly animation
#endif
   
#ifdef CAM_DEBUG_MODEL
   C_smart_ptr<I3D_model> debug_mod; //camera visual debug model - linked to cam; used for visual debugging in edit mode
#endif

                              //our target - direction and position
   C_smart_ptr<I3D_frame> focus_dir, focus_pos;
   float focus_alpha;         //last alpha of focus frame, for detecting changes

   S_quat delta_rot;          //additional direction C_vector mixed to ideal look dir

   float req_view_distance;   //required view distance
   float view_distance;       //real view distance
   byte mode_angle;           //angle of elevation - used for setting up ellevation_rot

                              //run-time help
   S_quat ellevation_rot;     //current angle mode converted to rotation

//----------------------------
// Get ideal camera position and dir for focused frames.
// For computing direction:
//    if 'use_prev_cam_pos' is true, 'dir' is computed from current camera position to the target;
//    if it is false, 'dir' is computed from ideal computed position.
   void GetIdealPos(S_vector &pos, S_vector &dir, bool use_prev_cam_pos) const{

                              //update position
      const S_vector &look_at_pos = focus_pos->GetMatrix()(3);
      if(view_distance > MRG_ZERO){
                              //apply appropriate distance from camera
         const S_matrix &m_dir = focus_dir->GetMatrix();
         S_vector look_dir = (ellevation_rot * delta_rot).GetDir();
                                 //limit z, so that cam doesn't look back (MB: no, collides with cam effects)
         //look_dir.z = Max(look_dir.z, .1f);
         //look_dir = look_dir.RotateByMatrix(m_dir);
         look_dir %= m_dir;
         pos = look_at_pos - look_dir * view_distance / look_dir.Magnitude();
                              //check collisions from look_at to ideal place
#ifdef DEBUG_DRAW_LINES
         DebugLine(look_at_pos, pos, E_DEBUGLINE_ADD_TIMED);
#endif
#if 1
         CheckCol(look_at_pos, pos, I3DCOL_FORCE2SIDE);
#endif
      }else{
         pos = look_at_pos;
      }

                              //update dir
      const float DIR_FADE_MIN_DIST = 1.0f;

      const S_vector &curr_pos = !use_prev_cam_pos ? pos : cam->GetWorldPos();
      dir = S_normal(look_at_pos - curr_pos);

      if(dir.IsNull()){
                              //look straight, if at 1st person view (ideal pos == look dest)
         dir = focus_dir->GetWorldDir();
      }
      if(view_distance < DIR_FADE_MIN_DIST){
         float ratio = view_distance / DIR_FADE_MIN_DIST;
         dir.y = dir.y*ratio + delta_rot.GetDir().y*(1.0f-ratio);
         dir.Normalize();
      }
#ifdef DEBUG_DRAW_LINES
      DebugLine(look_at_pos, pos, E_DEBUGLINE_ADD_TIMED, 0xfa0000fa);
#endif
   }

//----------------------------
// Animate view distance in time, set alpha of focused frame when too close.
   void UpdateViewDistance(int time){

      float tsec = (float)time * .001f;
      if(view_distance < req_view_distance){

         view_distance += tsec * DISTANCE_ANIM_SPEED;
         view_distance = Min(view_distance, req_view_distance);
#ifdef MIN_VIEW_DISTANCE
         if(view_distance < MIN_VIEW_DISTANCE)
            view_distance = MIN_VIEW_DISTANCE;
#endif
      }else
      if(view_distance > req_view_distance){

         view_distance -= tsec * DISTANCE_ANIM_SPEED;
#ifdef MIN_VIEW_DISTANCE
         if(view_distance < MIN_VIEW_DISTANCE){
            view_distance = (req_view_distance == 0.0f) ? 0.0f : MIN_VIEW_DISTANCE;
            if(!view_distance)
               SetFocusAlpha(1.0f);
         }
#endif
      }
      float alpha = 0.0f;
      if(view_distance){
         alpha = 1.0f;
         float real_dist = (cam->GetWorldPos() - focus_pos->GetWorldPos()).Magnitude();
         if(real_dist < FADE_VIEW_DIST_MAX){
            alpha = 0.0f;
            if(real_dist > FADE_VIEW_DIST_MIN)
               alpha = (real_dist-FADE_VIEW_DIST_MIN) / (FADE_VIEW_DIST_MAX-FADE_VIEW_DIST_MIN);
         }
      }
      SetFocusAlpha(alpha);
   }

//----------------------------
#ifndef NEW_STYLE
   bool CheckTargetVisible() const{

      const S_vector &look_at_pos = focus_pos->GetMatrix()(3);

      I3D_collision_data cd;
      cd.from = cam->GetWorldPos();
      cd.dir = look_at_pos - cd.from;
      float len = cd.dir.Magnitude();
      if(len < .4f)
         return true;
      cd.dir *= (len - .4f) / len;
      cd.flags = I3DCOL_LINE | I3DCOL_FORCE2SIDE;
      cd.frm_ignore = focus_dir;
      //cd.collide_bits = VOLCAT_ALL - (VOLCAT_SEEN_THROUGH);

      //DebugLine(cd.from, cd.from + cd.dir, 1);
      bool b = TestCollision(scene, cd);
      return !b;
   }
#endif//!NEW_STYLE

//----------------------------

   bool CheckCol(const S_vector &from, S_vector &to, dword add_flags = 0) const{

      I3D_collision_data cd;
      cd.from = from;
      cd.dir = to - from;
      float len = cd.dir.Magnitude();
      if(len < CAM_NEAR_RANGE)
         return false;
      cd.flags = I3DCOL_MOVING_SPHERE | add_flags;
      cd.frm_ignore = focus_dir;
      cd.radius = CAM_NEAR_RANGE;
      //cd.collide_bits = VOLCAT_ALL - (VOLCAT_SEEN_THROUGH);

      //DebugLine(cd.from, cd.from+cd.dir, 1);

      bool b = TestCollision(scene, cd);
      if(b){
         to = (cd.flags&I3DCOL_SOLVE_SLIDE) ? cd.GetDestination() : cd.ComputeHitPos();
         //DebugLine(cd.from, to, 1, 0xffff0000);
#if defined _DEBUG && 0
         DEBUG(C_fstr("frm: %s, dist: %.2f", (const char*)cd.GetHitFrm()->GetName(), cd.GetHitDistance()));
#endif
      }
      return b;
   }

//----------------------------

#ifndef NEW_STYLE
   void PrepareFlyAnim(const S_vector &pos, const S_vector &dir){

                              //prepare and set animation
      fly_anim->Clear();
      const int time = Max(FloatToInt(view_distance * 100.0f), 100);
      const S_vector &curr_pos = cam->GetPos();

      {
                              //setup position
         I3D_anim_pos_tcb keys[2];
         keys[0].Clear();
         keys[0].time = 0;
         keys[1].Clear();
         keys[1].time = time;

         keys[0].v = curr_pos;
         keys[1].v = pos;
         keys[1].easy_to = 1.0f;

         fly_anim->SetPositionKeys(keys, 2);
      }

      {
                              //setup directions
         //I3D_anim_rot keys[2];
         I3D_anim_quat keys[2];

         keys[0].Clear();
         keys[0].time = 0;
         keys[1].Clear();
                              //make direction faster than position
         keys[1].time = FloatToInt((float)time * .125f);

         //S_quat rot0, rot1;
         S_quat& rot0 = keys[0].q, &rot1 = keys[1].q;

         rot0.SetDir(cam->GetDir(), 0.0f);
         rot1.SetDir(dir, 0.0f);

         //DEBUG(rot0.v.Dot(rot1.v));
         //DebugLine(cam->GetWorldPos(), cam->GetWorldPos()+cam->GetDir()*10, 1, 0xffff0000);
         //DebugLine(cam->GetWorldPos(), cam->GetWorldPos()+dir*10, 1, 0xff00ff00);
         /*
         rot1 = ~rot0 * rot1;

         rot0.Inverse(keys[0].axis, keys[0].angle);
         rot1.Inverse(keys[1].axis, keys[1].angle);
         */
         fly_anim->SetRotationKeys1(keys, 2);
      }
      fly_anim->SetEndTime(time);
      fly_interp->SetCurrTime(0);
      fly_interp->SetAnimation(fly_anim, 0);
   }
#endif//!NEW_STYLE

//----------------------------

   void SetFocusAlpha(float alpha){

      if(focus_alpha!=alpha){
         if(focus_dir){
            struct S_hlp{
               static I3DENUMRET I3DAPI Proc(PI3D_frame f, dword c){
                  I3DCAST_VISUAL(f)->SetVisualAlpha(I3DIntAsFloat(c));
                  return I3DENUMRET_OK;
               }
            };
            focus_dir->EnumFrames(S_hlp::Proc, I3DFloatAsInt(alpha), ENUMF_VISUAL);
         }
         focus_alpha = alpha;
      }
   }

//----------------------------

   enum E_SAVEGAME_CHUNK_TYPE{
      //SG_POS,
      //SG_ROT,
      SG_DIST,
      SG_DELTA_ROT,
      SG_ANGLE,
      SG_FOC_DIR,
      SG_FOC_POS,
   };

//----------------------------

   virtual void SaveGame(C_chunk &ck) const{

      ck
         //(SG_POS, GetPos())
         //(SG_ROT, GetRot())
         (SG_DIST, GetDistance())
         (SG_DELTA_ROT, GetDeltaRot())
         (SG_ANGLE, GetAngleMode())
         ;
      if(focus_dir)
         ck(SG_FOC_DIR, focus_dir->GetName());
      if(focus_pos)
         ck(SG_FOC_POS, focus_pos->GetName());
   }

//----------------------------

   virtual void ApplySavedGame(PI3D_scene scn, C_chunk &ck){

      PI3D_frame foc_dir = NULL, foc_pos = NULL;
      while(ck)
      switch(++ck){
      //case SG_POS: cam->SetPos(ck.RVectorChunk()); break;
      //case SG_ROT: cam->SetRot(ck.RQuaternionChunk()); break;
      case SG_DIST: SetDistance(ck.RFloatChunk(), true); break;
      case SG_DELTA_ROT: SetDeltaRot(ck.RQuaternionChunk()); break;
      case SG_ANGLE: SetAngleMode(ck.RByteChunk()); break;
      case SG_FOC_DIR:
         foc_dir = scn->FindFrame(ck.RStringChunk());
         if(!foc_dir)
            throw C_except("game_cam: can't find focus dir frame");
         break;
      case SG_FOC_POS:
         foc_pos = scn->FindFrame(ck.RStringChunk());
         if(!foc_pos)
            throw C_except("game_cam: can't find focus pos frame");
         break;
      default: assert(0); --ck;
      }
      SetFocus(foc_pos, foc_dir);
      ResetPosition();
   }

//----------------------------

public:
   C_game_camera_imp(PI3D_scene s):
      scene(s),
      focus_dir(NULL),
      focus_pos(NULL),
      focus_alpha(1.0f),

      req_view_distance(0.0f),
      view_distance(0.0f)
   {
      delta_rot.Identity();
      SetAngleMode(1);

      anim_dummy = I3DCAST_DUMMY(scene->CreateFrame(FRAME_DUMMY));
      anim_dummy->LinkTo(scene->GetPrimarySector());
#ifdef EDITOR
      anim_dummy->SetName("<cam dummy>");
#endif
      anim_dummy->Release();

#ifndef NEW_STYLE
      fly_interp = driver->CreateInterpolator(); 
      fly_interp->Release();
      fly_anim = (PI3D_keyframe_anim)driver->CreateAnimation(I3DANIM_KEYFRAME); 
      fly_anim->Release();
#endif
   }

   ~C_game_camera_imp(){

      SetFocus(NULL, NULL);
   }

//----------------------------

   virtual PI3D_camera GetCamera(){ return cam; }
   virtual CPI3D_camera GetCamera() const{ return cam; }

//----------------------------

   virtual void SetDeltaDir(const S_vector &dir, const S_vector &up){
      delta_rot.SetDir(dir, up);
   }

//----------------------------

   virtual void SetDeltaRot(const S_quat &rot){
      delta_rot = rot;
   }

//----------------------------

   virtual const S_quat &GetDeltaRot() const{ return delta_rot; }

//----------------------------

   virtual float GetDistance() const{ return req_view_distance; }

//----------------------------

   virtual float GetRealDistance() const{ return view_distance; }

//----------------------------

   virtual byte GetAngleMode() const{ return mode_angle; }

//----------------------------

   virtual CPI3D_frame GetFocus() const{ return focus_pos; }

//----------------------------

   virtual CPI3D_frame GetFocusDir() const{ return focus_dir; }

//----------------------------

   virtual void SetCamera(PI3D_camera c1){

      cam = c1;
#ifndef NEW_STYLE
      if(fly_interp){
         fly_interp->SetAnimation(0, NULL);
         fly_interp->SetFrame(cam);
      }
#endif
      if(cam){
         cam->LinkTo(anim_dummy);
      }

#ifdef CAM_DEBUG_MODEL
                                 //create or destroy debug model
      if(cam){
         if(!debug_mod){
            debug_mod = I3DCAST_MODEL(scene->CreateFrame(FRAME_MODEL));
            debug_mod->Release();
            debug_mod->SetName("<Camera debug model>");
            model_cache.Open(debug_mod, "system\\_camera", scene, 0, ErrReport, editor);
            debug_mod->LinkTo(cam);
         }
      }else{
         debug_mod = NULL;
      }
#endif
   }

//----------------------------

   virtual void SetFocus(PI3D_frame fpos, PI3D_frame fdir){

      SetFocusAlpha(1.0f);
      focus_pos = fpos;
      focus_dir = fdir;
      if(!focus_dir)
         focus_dir = focus_pos;
   }

//----------------------------

   virtual void SetDistance(float f, bool immediate){

      req_view_distance = f;
      if(immediate){
         view_distance = req_view_distance;
      }
   }

//----------------------------

   virtual void ResetPosition(){

      if(!focus_pos)
         return;
      view_distance = req_view_distance;
      S_vector pos, dir;
      GetIdealPos(pos, dir, false);
      assert(!dir.IsNull());
      cam->SetPos(pos);
      cam->SetDir(dir);
   }

//----------------------------

   virtual void SetAngleMode(byte m){

      assert(m<3);
      mode_angle = m;

      float angle = VIEW_ANGLES[mode_angle];
      ellevation_rot.SetDir(S_vector(0.0f, -(float)sin(angle), (float)cos(angle)), 0.0f);
   }

//----------------------------

   virtual void SetAnim(const char *name, bool loop){

      PI3D_animation_set as;
      I3D_RESULT ir = anim_cache.Create(name, &as, scene, ErrReport, editor);
      if(I3D_SUCCESS(ir)){
         if(as->NumAnimations()){
            CPI3D_animation_base ab = as->GetAnimation(0);
            if(ab->GetType()==I3DANIM_KEYFRAME){
               if(!anim_interp){
                  anim_interp = driver->CreateInterpolator();
                  anim_interp->Release();
               }
               anim_interp->SetFrame(anim_dummy);
               anim_interp->SetAnimation((CPI3D_keyframe_anim)ab, loop ? I3DANIMOP_LOOP : 0);
            }
         }
         as->Release();
      }
   }

//----------------------------

   virtual void Tick(int time){

      if(!focus_dir || !focus_pos || !cam)
         return;

#if 0
      DEBUG(C_fstr("focus_dir: '%s', focus_pos: '%s'", (const char*)focus_dir->GetName(), (const char*)focus_pos->GetName()));
#endif
      assert(("Game camera not linked to it's anim_dummy", cam->GetParent() == anim_dummy));

                              //animate view distance
      UpdateViewDistance(time);

                              //setup ideal position
      if(view_distance < MRG_ZERO){
         const S_matrix &m_pos  = focus_pos->GetMatrix();
         const S_matrix &m_dir1 = focus_dir->GetMatrix();
         //S_vector dir = delta_dir % m_dir1;
         S_matrix m_rot = S_matrix(delta_rot) % m_dir1;
         S_vector pos = m_pos(3) - (m_rot(2) * .06f);
         cam->SetPos(pos);
         cam->SetDir1(m_rot(2), m_rot(1));
         return;
      }
#ifndef NEW_STYLE
      S_vector pos, dir;
      GetIdealPos(pos, dir, true);

                                 //there is always some delta, because human breathing change head dir every frame.
      //if((cam->GetPos() - pos).Magnitude() > MRG_ZERO || cam->GetDir().AngleTo(dir) > MRG_ZERO)
      {
         PrepareFlyAnim(pos, dir);
      }

                                 //move
      const S_vector last_pos = cam->GetWorldPos();
      assert(fly_interp);
      fly_interp->Tick(time);
      const S_vector wanted_pos = cam->GetWorldPos();

      S_vector to = wanted_pos;

      bool b = CheckCol(last_pos, to, I3DCOL_SOLVE_SLIDE);
      if(b){
         to *= cam->GetParent()->GetInvMatrix();
         cam->SetPos(to);
      }

      float dist_to = (last_pos - to).Magnitude();
      float dist_want = (to - wanted_pos).Magnitude();
      bool move_blocked = (dist_to < dist_want);

      bool target_visible = CheckTargetVisible();
                              //reset if stuck
      if(move_blocked || !target_visible){
         S_vector pos, dir;
         GetIdealPos(pos, dir, false);
         cam->SetPos(pos);
         cam->SetDir(dir);
         fly_interp->SetAnimation(0, NULL);
      }
#else//!NEW_STYLE
      {
         S_vector pos, dir, up;
         GetIdealPos(pos, dir, false);
                              //keep up-C_vector slightly positioned forward, so that cam doesn't turn opposite dir when elevation > 90 degrees
         const S_matrix &m_dir = focus_dir->GetMatrix();
         up = S_vector(0, 1, .5f) % m_dir;

         cam->SetPos(pos);
         cam->SetDir1(dir, up);
      }
#endif

#ifndef DISABLE_ADD_ANIM
      if(anim_interp){
         const S_vector cam_old_pos = cam->GetWorldPos();
         anim_interp->Tick(time);
         //S_vector cam_new_pos = cam->GetWorldPos();
         S_vector cam_new_pos = (cam->GetParent()->GetMatrix() * cam->GetLocalMatrix())(3);
         //DebugLine(cam_new_pos, cam_old_pos, 0);
#if 1
         bool b = CheckCol(cam_old_pos, cam_new_pos);
         if(b){
            //DebugLine(cam_old_pos, cam_new_pos);
            cam_new_pos *= cam->GetParent()->GetInvMatrix();
            cam->SetPos(cam_new_pos);
         }
#endif
      }
#endif//!DISABLE_ADD_ANIM
   }

//----------------------------
};

//----------------------------

C_game_camera *CreateGameCamera(PI3D_scene s){
   return new C_game_camera_imp(s);
}

//----------------------------
//----------------------------
                                                        