#include "all.h"
#include "actors.h"
#include "GameMission.h"



//----------------------------

#define DEFAULT_MAX_USE_DIST 1.5f   //default maximal use distance
#define DEFAULT_MAX_USE_ANGLE (PI*.4f) //default maximal use angle

//----------------------------

const S_actor_type_info actor_type_info[ACTOR_LAST] = {
// friendly name     create   
   {"NULL",           0},
   {"Effect",         0},
   {"Player",         0},
   {"Dier",           1},
   {"Physics",        1},
   {"Vehicle",        1},
};


//----------------------------
// Function used by editor - retrieve user-friendly ACTOR name from a frame.
const char *GetFrameActorName(PI3D_frame frm){

   S_frame_info *fi = GetFrameInfo(frm);
   if(fi && fi->actor)
      return actor_type_info[fi->actor->GetActorType()].friendly_name;
   return NULL;
}

//----------------------------

C_actor::C_actor(C_game_mission &m1, PI3D_frame in_frm, E_ACTOR_TYPE in_type):
   frame(in_frm),
   actor_type(in_type),
   center_pos_valid(false),
   mission(m1)
{
   assert(in_frm);

   S_frame_info *fi = CreateFrameInfo(frame);
   assert(fi && frame && !fi->actor);
   fi->actor = this;
   SetFrameInfo(frame, fi);
}

//----------------------------

C_actor::~C_actor(){

   S_frame_info *fi = GetFrameInfo(frame);
   assert(fi);
   if(fi){
      fi->actor = NULL;
      fi = SetFrameInfo(frame, fi);
   }
}

//----------------------------

bool C_actor::SetFrameSector(){
   return mission.GetScene()->SetFrameSector(frame);
}

//----------------------------

void C_actor::GameBegin(){
}

//----------------------------

void C_actor::GameEnd(){
}

//----------------------------

void C_actor::AssignTableTemplate(){

   tab = CreateTable();
   tab->Load(GetTableTemplate(), TABOPEN_TEMPLATE);
   tab->Release();
}

//----------------------------

CPI3D_frame C_actor::WatchObstacle(const S_vector &from, const S_vector &to,
   float move_necessity, S_vector *ret_norm, float *ret_dist){

   S_vector to1 = to;
   S_vector dir = to - from;

   CPI3D_frame obstacle = NULL;
   I3D_collision_data cd;
   cd.from = from;
   cd.dir = dir;
   cd.frm_ignore = frame;
   cd.flags = I3DCOL_LINE;
   //DebugLine(cd.from, cd.from+cd.dir, 0);
   if(mission.TestCollision(cd)){
      obstacle = cd.GetHitFrm();
      if(ret_norm)
         *ret_norm = cd.GetHitNormal();
      if(ret_dist)
         *ret_dist = cd.GetHitDistance();
   }
   if(obstacle){
      //OUTTEXT(obstacle->GetName());
                              //check what's in the way (find actor)
      if(obstacle->GetType()==FRAME_VOLUME){
         PI3D_frame owner = I3DCAST_CVOLUME(obstacle)->GetOwner();
         if(owner)
            obstacle = owner;
      }
      /*
      PC_actor fa = GetFrameActor(obstacle);
      if(fa && fa->AskToStepAway(from, to1, this, move_necessity))
         return NULL;
         */
   }
   return obstacle;
}

//----------------------------

void C_actor::ReportActorError(const char *err_msg) const{

   ErrReport(C_xstr("Actor '%' (%): %")
      % frame->GetName()
      % actor_type_info[actor_type].friendly_name
      % err_msg
      , editor);
}

//----------------------------

void C_actor::SetupVolumes(dword category_bits, dword collide_bits){

   struct S_hlp{
      PI3D_frame owner;
      dword cat, col;

      static I3DENUMRET I3DAPI cbVol(PI3D_frame frm, dword c){
         S_hlp *hp = (S_hlp*)c;
         PI3D_volume vol = I3DCAST_VOLUME(frm);
         //vol->SetOn(true);
         vol->SetOwner(hp->owner);
         vol->SetCategoryBits(hp->cat);
         vol->SetCollideBits(hp->col);

         return I3DENUMRET_OK;
      }
   } hlp = {frame, category_bits, collide_bits};
   frame->EnumFrames(S_hlp::cbVol, (dword)&hlp, ENUMF_VOLUME);
}

//----------------------------

const S_vector C_actor::GetCenterPos() const{

   if(!center_pos_valid){
      assert(frame);
      loc_center_pos = GetFrameLocalCenter(frame);
            
      /*I3D_bound_volume bvol_tmp;
      const I3D_bound_volume *bvol;
      switch(frame->GetType()){
      case FRAME_VISUAL:
         bvol = &I3DCAST_CVISUAL(frame)->GetHRBoundVolume();
         break;
      case FRAME_MODEL:
         bvol = &I3DCAST_CMODEL(frame)->GetHRBoundVolume();
         break;
      default:
         frame->ComputeHRBoundVolume(&bvol_tmp);
         bvol = &bvol_tmp;
      }
      if(!bvol->bbox.IsValid()){
         loc_center_pos.Zero();
      }else{
         loc_center_pos = bvol->bbox.min + (bvol->bbox.max - bvol->bbox.min) * .5f;
      }*/
      center_pos_valid = true;
   }
   return loc_center_pos * frame->GetMatrix();
}

//----------------------------

//----------------------------

void C_actor::OnSignal(int signal_id){

                              //just write message, because inherited actors should overload this function
   //ReportActorError(C_fstr("unprocessed signal %i from '%s'", signal_id, frm_caller));
}

//----------------------------

void C_actor::Enable(bool on_off){

                              //just write warning
   ReportActorError("unprocessed Enable() function call");
}

//----------------------------
//----------------------------


