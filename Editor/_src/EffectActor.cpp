#include "all.h"
#include "GameMission.h"



#define ALIVE_CHECK_COUNT 300 //how often we check if we're still alive

//----------------------------
//----------------------------

class C_effect: public C_actor{
   
                              //initial values:
   dword mode_flags;
   float scale_add;


                              //runtime:
   int auto_destroy_count;    //used with EFFECT_AUTO_DESTROY
   float r_fade_come, r_fade_leave;
   int fade_come_count;
   int fade_stay;
   int fade_leave_count;
   dword life_time;           //how long this actor already lives (due to advancing particle after loadgame)
   C_str particle_name;       //valid only if it is particle type

//----------------------------
// Get sector in which the frame is contained.
   PI3D_sector GetSector() const{
      PI3D_frame sct_prnt = frame->GetParent();
      while(sct_prnt && sct_prnt->GetType()!=FRAME_SECTOR)
         sct_prnt = sct_prnt->GetParent();
      return I3DCAST_SECTOR(sct_prnt);
   }

//----------------------------
// Apply alpha values to all visuals children.
   void ApplyAlpha(float alpha){

      assert(frame->GetType()==FRAME_MODEL);
      PI3D_model mod = I3DCAST_MODEL(frame);
      for(int i=mod->NumChildren(); i--; ){
         PI3D_frame frm = mod->GetChildren()[i];
         if(frm->GetType()==FRAME_VISUAL)
            I3DCAST_VISUAL(frm)->SetVisualAlpha(alpha);
      }
   }

//----------------------------

   enum{
      SG_MODE_FLG,
      SG_POS,
      SG_ROT,
      SG_SCL,
      SG_LINK,

      SG_DESTR_CNT,
      SG_FADE_COME,
      SG_FADE_COME_CNT,
      SG_FADE_LEAVE,
      SG_FADE_LEAVE_CNT,
      SG_FADE_STAY,
      SG_SCL_ADD,
      SG_LIFE_TIME,

      SG_SND,
         SG_SND_FNAME,
         SG_SND_TYPE,
         SG_SND_R_N,
         SG_SND_R_F,
         SG_SND_VOL,
         SG_SND_CURR_T,
         SG_SND_FLGS,

      SG_PRT_NAME,

      SG_MODEL,
   };

//----------------------------

   virtual bool WantSaveGame() const{

      switch(frame->GetType()){
      case FRAME_SOUND:
         if(!I3DCAST_CSOUND(frame)->IsPlaying())
            return false;
         break;
      }
      return true;
   }

//----------------------------

   virtual void SaveGame(C_chunk &ck) const{

                              //save raw frame type as the 1st thing
      dword ft = frame->GetType();
      dword st = (ft==FRAME_VISUAL) ? I3DCAST_CVISUAL(frame)->GetVisualType() : NULL;
      ck.Write(&ft, sizeof(dword));
      ck.Write(&st, sizeof(dword));
      ck
         (SG_PRT_NAME, particle_name)
         (SG_MODE_FLG, mode_flags)
         (SG_POS, frame->GetPos())
         (SG_ROT, frame->GetRot())
         (SG_SCL, frame->GetScale())
         (SG_DESTR_CNT, auto_destroy_count)
         (SG_FADE_COME, r_fade_come)
         (SG_FADE_COME_CNT, fade_come_count)
         (SG_FADE_LEAVE, r_fade_leave)
         (SG_FADE_LEAVE_CNT, fade_leave_count)
         (SG_FADE_STAY, fade_stay)
         (SG_SCL_ADD, scale_add)
         (SG_LIFE_TIME, life_time)
         ;
      if(frame->GetParent())
         ck(SG_LINK, frame->GetParent()->GetName());
                              //write frame-specific data
      switch(frame->GetType()){
      case FRAME_SOUND:
         {
            ck <<= SG_SND;
            CPI3D_sound snd = I3DCAST_CSOUND(frame);
            float n, f;
            snd->GetRange(n, f);
            ck
               (SG_SND_FNAME, snd->GetFileName())
               (SG_SND_TYPE, (dword)snd->GetSoundType())
               (SG_SND_R_N, n)
               (SG_SND_R_F, f)
               (SG_SND_VOL, snd->GetVolume())
               (SG_SND_CURR_T, snd->GetCurrTime())
               (SG_SND_FLGS, snd->GetOpenFlags())
               ;
            --ck;
         }
         break;

      case FRAME_MODEL:
         ck(SG_MODEL, I3DCAST_CMODEL(frame)->GetFileName());
         break;

      case FRAME_VISUAL:
         break;

      default: assert(0);
      }
   }

//----------------------------
public:
   void Init(C_chunk &ck){

      while(ck)
      switch(++ck){
      case SG_MODE_FLG: ck >> mode_flags; break;
      case SG_POS: frame->SetPos(ck.RVectorChunk()); break;
      case SG_ROT: frame->SetRot(ck.RQuaternionChunk()); break;
      case SG_SCL: frame->SetScale(ck.RFloatChunk()); break;
      case SG_LINK:
         {
            C_str n = ck.RStringChunk();
            PI3D_frame prnt = mission.GetScene()->FindFrame(n);
            if(prnt)
               frame->LinkTo(prnt);
            else{
               assert(0);
               mission.GetScene()->SetFrameSector(frame);
            }
         }
         break;
      case SG_DESTR_CNT: ck >> auto_destroy_count; break;
      case SG_FADE_COME: ck >> r_fade_come; break;
      case SG_FADE_COME_CNT: ck >> fade_come_count; break;
      case SG_FADE_LEAVE: ck >> r_fade_leave; break;
      case SG_FADE_LEAVE_CNT: ck >> fade_leave_count; break;
      case SG_FADE_STAY: ck >> fade_stay; break;
      case SG_SCL_ADD: ck >> scale_add; break;
      case SG_LIFE_TIME: ck >> life_time; break;
      case SG_PRT_NAME:
         ck >> particle_name;
         if(particle_name.Size())
            InitParticle();
         break;
      case SG_SND:
         {
            assert(frame->GetType()==FRAME_SOUND);
            PI3D_sound snd = I3DCAST_SOUND(frame);
            C_str fname;
            float n = 1, f = 10;
            dword curr_t = 0, flags = 0;
            while(ck)
            switch(++ck){
            case SG_SND_FNAME: ck >> fname; break;
            case SG_SND_TYPE: snd->SetSoundType((I3D_SOUNDTYPE)ck.RIntChunk()); break;
            case SG_SND_R_N: ck >> n; break;
            case SG_SND_R_F: ck >> f; break;
            case SG_SND_VOL: snd->SetVolume(ck.RFloatChunk()); break;
            case SG_SND_CURR_T: ck >> curr_t; break;
            case SG_SND_FLGS: ck >> flags; break;
            default: assert(0); --ck;
            }
            --ck;
            snd->SetRange(n, f);
            sound_cache.Open(snd, fname, mission.GetScene(), flags, ErrReport, editor);
            snd->SetCurrTime(curr_t);
         }
         break;

      case SG_MODEL:
         {
            assert(frame->GetType()==FRAME_MODEL);
            PI3D_model mod = I3DCAST_MODEL(frame);
            model_cache.Open(mod, ck.RStringChunk(), mission.GetScene(), 0, ErrReport, editor);
                              //effect has never volumes
            DeleteVolumes(mod);
         }
         break;

      default: assert(0); --ck;
      }
      if(prt_tick_helper)
         prt_tick_helper->Tick(life_time);
   }

//----------------------------

   bool InitParticle(){

      assert(frame->GetType()==FRAME_VISUAL && I3DCAST_VISUAL(frame)->GetVisualType()==I3D_VISUAL_PARTICLE);
      PI3D_scene scn = mission.GetScene();
      PI3D_model mod = I3DCAST_MODEL(scn->CreateFrame(FRAME_MODEL));

      model_cache.Open(mod, particle_name, scn, 0, ErrReport, editor);
      PI3D_visual prt = I3DCAST_VISUAL(mod->FindChildFrame(NULL, ENUMF_VISUAL));
      if(!prt){
         mod->Release();
         return false;
      }
      dword data = frame->GetData();
      frame->Duplicate(prt);
      frame->SetData(data);
      mod->Release();
      return true;
   }

//----------------------------
private:
//----------------------------
                              //helper model for updating particles (particles are not ticked directly)
   C_smart_ptr<I3D_model> prt_tick_helper;

public:
   C_effect(C_game_mission &m1, PI3D_frame in_frm):
      C_actor(m1, in_frm ? in_frm : m1.GetScene()->CreateFrame(FRAME_VISUAL, I3D_VISUAL_PARTICLE), ACTOR_EFFECT),
      life_time(0),
      mode_flags(0),
      r_fade_come(1.0f), fade_come_count(0),
      fade_stay(0),
      r_fade_leave(1.0f), fade_leave_count(0),
      auto_destroy_count(0),
      scale_add(0.0f)
   {
      if(!in_frm)
         frame->Release();       //release one ref of CreateModel used above

      if(frame->GetType()==FRAME_VISUAL && I3DCAST_CVISUAL(frame)->GetVisualType()==I3D_VISUAL_PARTICLE){
                              //create helper ticker for particle
         prt_tick_helper = I3DCAST_MODEL(mission.GetScene()->CreateFrame(FRAME_MODEL));
         prt_tick_helper->AddFrame(frame);
         prt_tick_helper->Release();
      }
   }
   ~C_effect(){
      /*
#ifdef EDITOR
      if(editor)
         editor->FindPlugin("Modify")->RemoveFrame(frame);
#endif
         */
   }

//----------------------------

   bool Init(const S_effect_init &ei){

      mode_flags = ei.mode_flags;

      if(ei.particle_name){
         particle_name = ei.particle_name;
         if(!InitParticle())
            return false;
      }
      frame->SetPos(ei.pos);
      frame->SetRot(ei.rot);
      frame->SetScale(ei.scale);
      if(ei.link_to){
         frame->LinkTo(ei.link_to);
      }else{
         mission.GetScene()->SetFrameSectorPos(frame, GetCenterPos());
      }

                              //validity checks
      if(mode_flags&EFFECT_AUTO_DESTROY){
                              //check if auto-destroy gives sense
         switch(frame->GetType()){
         case FRAME_MODEL:
            if(!I3DCAST_MODEL(frame)->GetAnimationSet())
               ReportActorError("model with no anim used with EFFECT_AUTO_DESTROY");
            break;
         case FRAME_SOUND: break;
         case FRAME_VISUAL:
            switch(I3DCAST_CVISUAL(frame)->GetVisualType()){
            case I3D_VISUAL_PARTICLE:
               break;
            default:
               ReportActorError("unsupported visual type for EFFECT_AUTO_DESTROY");
            }
            break;
         default:
            ReportActorError("unsupported frame type for EFFECT_AUTO_DESTROY");
         }
      }else{
                              //check if effect will be automatically destroyed other way
         if(!(mode_flags&EFFECT_FADE_OFF)){
            ReportActorError("effect will be never destroyed!");
         }
      }


      if(mode_flags&EFFECT_FADE_OFF){
         if(frame->GetType()==FRAME_MODEL){
            if(!ei.fade_come && !ei.fade_stay && !ei.fade_leave){
               r_fade_come = 0;
               fade_come_count = 0;
               fade_stay = 5000;
               fade_leave_count = 5000;
               r_fade_leave = (float)fade_leave_count;
            }else{
               fade_come_count = ei.fade_come;
               r_fade_come = (float)fade_come_count;
               fade_stay = ei.fade_stay;
               fade_leave_count = ei.fade_leave;
               r_fade_leave = (float)fade_leave_count;
               if(r_fade_come)
                  ApplyAlpha(0.0f);
            }
            if(r_fade_leave)
               r_fade_leave = 1.0f / r_fade_leave;
            if(r_fade_come)
               r_fade_come = 1.0f / r_fade_come;
         }
      }
      if(mode_flags&EFFECT_ANIM_SCALE){
         scale_add = ei.scale_add;
      }
      return true;
   }

//----------------------------

   virtual void GameEnd(){

      C_actor::GameEnd();
      mission.DestroyActor(this);
   }

//----------------------------

   virtual void Tick(const struct S_tick_context&);

//----------------------------

};

//----------------------------

void C_effect::Tick(const S_tick_context &tc){

   life_time += tc.time;
   if(prt_tick_helper)
      prt_tick_helper->Tick(tc.time);

   bool is_auto_destroy = false;
   bool alive = false;

   if(mode_flags&EFFECT_AUTO_DESTROY){
      if((auto_destroy_count -= tc.time) <= 0){
         auto_destroy_count = ALIVE_CHECK_COUNT;
         is_auto_destroy = true;

         switch(frame->GetType()){
         case FRAME_SOUND: alive = I3DCAST_SOUND(frame)->IsPlaying(); break;
         case FRAME_VISUAL:
            {
               CPI3D_visual vis = I3DCAST_VISUAL(frame);
               switch(vis->GetVisualType()){
               case I3D_VISUAL_PARTICLE:
                  alive = (vis->GetProperty(I3DPROP_PRTC_I_NUMELEMENTS) != 0);
                  break;
               default: assert(0);
               }
            }
            break;
         default: assert(0);
         }
      }
   }
   if(mode_flags&EFFECT_FADE_OFF){
      if(fade_come_count){
         fade_come_count = Max(0, fade_come_count-tc.time);
         ApplyAlpha(1.0f - (float)fade_come_count * r_fade_come);
      }else
      if(fade_stay){
         fade_stay = Max(0, fade_stay-tc.time);
      }else{
         is_auto_destroy = true;
         if((fade_leave_count -= tc.time) > 0){
            alive = true;
            ApplyAlpha((float)fade_leave_count * r_fade_leave);
         }
      }
   }
   if(mode_flags&EFFECT_ANIM_SCALE){
      float tsec = (float)tc.time * .001f;
      frame->SetScale(frame->GetScale() + scale_add * tsec);
   }

   if(is_auto_destroy && !alive){
      mission.DestroyActor(this);
      return;
	}
   if(frame->GetType()==FRAME_MODEL){
      I3DCAST_MODEL(frame)->Tick(tc.time);
   }
}

//----------------------------
//----------------------------

PC_actor CreateEffectActor(C_game_mission &gm, PI3D_frame in_frm, const S_effect_init *id, C_chunk *ck_savegame){

   C_effect *ea = NULL;
   if(id){
      ea = new C_effect(gm, in_frm);
      if(!ea->Init(*id)){
         ea->Release();
         ea = NULL;
      }
   }else
   if(ck_savegame){
      assert(!in_frm);
      dword ft, st;
      ck_savegame->Read(&ft, sizeof(dword));
      ck_savegame->Read(&st, sizeof(dword));
      PI3D_frame frm = gm.GetScene()->CreateFrame(ft, st);
      ea = new C_effect(gm, frm);
      ea->Init(*ck_savegame);
      frm->Release();
   }else{
      ea = NULL;
      assert(0);
   }
   return ea;
}

//----------------------------
