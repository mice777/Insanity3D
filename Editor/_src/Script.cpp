#include "all.h"
#include "gamemission.h"
#include "script.h"
#include "game_cam.h"
#include "GameMission.h"
#include <c_linklist.h>
#include <list>

//----------------------------

extern const char SCRIPT_CMDLINE[];
const char SCRIPT_CMDLINE[] = "/iMissions /ICommon.i";

//----------------------------

//#define SCRIPT_LOG(t) LOG(t)
#define SCRIPT_LOG(t)

//----------------------------

#define SCRIPT_ERR(fnc_name, msg) script_man->ReportError(fnc_name, msg)

static const char SCRIPT_DIR[] = "Missions";

#define INIT_FUNCTION(fnc_name) PI3D_frame frm = script_man_imp.GetCurrFrame();\
   if(!frm){ SCRIPT_ERR(fnc_name, "no current frame"); return; }

#define _INIT_FUNCTION(fnc_name, rtn) PI3D_frame frm = script_man_imp.GetCurrFrame();\
   if(!frm){ SCRIPT_ERR(fnc_name, "no current frame"); return rtn; }

#define INIT_ACTOR(fnc_name) INIT_FUNCTION(fnc_name) \
   S_frame_info *fi = GetFrameInfo(frm); \
   if(!fi || !fi->actor){ \
   SCRIPT_ERR(fnc_name, C_fstr("no actor set on frame '%s'", (const char*)frm->GetName())); return; } \
   PC_actor act = fi->actor;

#define _INIT_ACTOR(fnc_name, rtn) _INIT_FUNCTION(fnc_name, rtn) \
   S_frame_info *fi = GetFrameInfo(frm); \
   if(!fi || !fi->actor){ \
   SCRIPT_ERR(fnc_name, C_fstr("no actor set on frame '%s'", (const char*)frm->GetName())); return rtn; } \
   PC_actor act = fi->actor;

//----------------------------
//----------------------------

extern const VM_LOAD_SYMBOL script_symbols[];

//----------------------------

class C_script_manager_imp: public C_script_manager{
public:
                              //mode specifying how is the thread suspended
   enum E_SUSPEND_MODE{
      SUSPEND_NULL,
      SUSPEND_PAUSE,          //Pause function
      SUSPEND_FADE_VOL,       //FadeVolume function
      SUSPEND_MOVE,           //MoveObject function
      SUSPEND_FLASH,          //FlashObject function
      SUSPEND_TASK,           //WaitSuccess, WaitFail functions
      SUSPEND_SAVEGAME,       //SaveGame function
      SUSPEND_USER,           //user mode, not processed by script manager
   };

//----------------------------
                              //suspended script - inherited from linked list, to be usable confortably in list
   class C_thread_context: public C_link_list_element<C_thread_context>{
      dword ref;

      C_thread_context(const C_thread_context &sc);
      C_thread_context &operator =(const C_thread_context &sc);

      friend class C_script_manager_imp;

      C_smart_ptr<C_v_machine> vm;
      dword save_context_id;

   //----------------------------
   public:
      void AddRef(){ ++ref; }
      void Release(){ if(!--ref) delete this; }

      E_SUSPEND_MODE mode;
      C_smart_ptr<I3D_frame> frm;
      union U_data{
                                 //PAUSE:
         struct S_pause{
         } pause;
                                 //FADE_VOL:
         struct S_fade_vol{
            float r_init_time;   //reciprocal of initialization time
            float dest_volume;   //destinatio volume
            float volume_delta;  //delta of volume change
         } fade_vol;
                                 //FLASH:
         struct S_flash{
            dword attack_count;
            dword stay_count;
            dword decay_count;
            float flash_value;
            enum E_FLASH_MODE{
               FLASH_ATTACK,
               FLASH_STAY,
               FLASH_DECAY,
            } mode;
         } flash;
                                 //TASK:
         struct S_task{
            dword ids;
            enum E_TASK_CONDITION{
               TC_SUCCESS,
               TC_FAIL,
            } condition;
         }task;
      } data1;
                                 //MOVE:
      struct S_move_obj: public C_unknown{
         S_vector dest_pos, delta_pos;
         float r_init_time;      //reciprocal of init time
         S_quat dest_rot, src_rot;
         C_ease_interpolator ease;
         C_smart_ptr<I3D_frame> frm;   //frame being moved (NULL if it is scripted frame being moved)
      };
                                 //DETECT:
      struct S_detect: public C_unknown{
         dword detect_condition;
         C_str what;
         PI3D_frame frm_where;
         float range;
      };
      C_unknown *data;
   
      int counter;

//----------------------------
      C_thread_context(PI3D_frame f, E_SUSPEND_MODE m):
         ref(1),
         frm(f),
         mode(m),
         counter(0),
         data(NULL),
         save_context_id(0)
      {
         memset(&data, 0, sizeof(data));

         PS_frame_info fi = GetFrameInfo(frm);
         vm = fi->vm;
      }

      ~C_thread_context(){
         if(data) data->Release();;
      }
   };
private:
//----------------------------

                              //linked list of all thread contexts
   C_link_list<C_thread_context> script_threads;

                              //signals are accumulated and sent synchronously when script execution stops
   struct S_signal_data{
      PI3D_frame from;
      C_str frm_receiver;
      int signal_id;
   };
   typedef list<S_signal_data> t_signals;
   t_signals signals;

//----------------------------

   static void ISLAPI CompileErr(const char *msg, void *context, int l, int r, bool warn){
      ErrReport(msg, editor);
   }

//----------------------------
// Resume script from given context, and release it.
   E_SCRIPT_RESULT ResumeThread(C_thread_context *tc, PC_game_mission gm){

      C_smart_ptr<I3D_frame> save_curr_run_frame = curr_run_frame;
      C_smart_ptr<C_game_mission> save_curr_run_mission = curr_run_mission;

      //assert(!curr_run_mission);
      tc->RemoveFromList();

      PI3D_frame frm = tc->frm;
      assert(frm);
                                 //find specified function
      curr_run_frame = frm;
      curr_run_mission = gm;

      E_SCRIPT_RESULT result;

      //dword r_edi, r_edi1;
      //__asm mov r_edi, edi
      ISL_RESULT ir = tc->vm->Run(NULL, (void*)tc->save_context_id, VMR_SAVEDCONTEXT);
      //__asm mov r_edi1, edi
      //assert(r_edi==r_edi1);
      if(ISL_SUCCESS(ir)){
         switch(ir){
         case ISL_OK:
            result = SR_OK;
            break;
         case ISL_SUSPENDED:
            result = SR_SUSPENDED;
            break;
         default: assert(0); result = SR_RUN_ERROR;
         }
      }else{
         const char *err_msg = "unknown";
         switch(ir){
         case ISLERR_DIVIDE_BY_ZERO: err_msg = "divide-by-zero"; break;
         case ISLERR_NEVERENDING_LOOP: err_msg = "neverending loop"; break;
         case ISLERR_BAD_TABLE_INDEX: err_msg = "bad table index"; break;
         case ISLERR_INVADDRESS: err_msg = "invalid address"; break;
         case ISLERR_INVOPCODE: err_msg = "invalid opcode"; break;
         }
         ErrReport(C_fstr("script error: '%s' ('%s' set on frame '%s')",
            err_msg, tc->vm->GetScript()->GetName(), (const char*)frm->GetName()), editor);
         result = SR_RUN_ERROR;
      }
      //curr_run_frame = NULL;
      //curr_run_mission = NULL;
      curr_run_frame = save_curr_run_frame;
      curr_run_mission = save_curr_run_mission;

      AfterScriptCall(gm);
      tc->Release();
      return result;
   }

//----------------------------
// Function called after returning from any script call. It writes logs, sends signals, etc.
   void AfterScriptCall(PC_game_mission gm){

      while(signals.size()){
                                 //must make copy of this and erase it from signal list,
                                 // so that we're not cycled by calling next script
         S_signal_data sd = signals.front();
         signals.pop_front();
         struct S_hlp{
            C_game_mission *gm;
            int signal_id;
            dword num_sent;
            C_script_manager_imp *_this;
            static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

               S_hlp *hp = (S_hlp*)c;
                                    //try to run OnSignal function
               C_script_manager::E_SCRIPT_RESULT sr = hp->_this->RunFunction(frm, "OnSignal", hp->gm, hp->signal_id);
               if(sr>=0)
                  ++hp->num_sent;
                                    //try to call actor's OnSignal function
               PS_frame_info fi = GetFrameInfo(frm);
               if(fi && fi->actor)
                  fi->actor->OnSignal(hp->signal_id);
               return I3DENUMRET_OK;
            }
         } hlp = {gm, sd.signal_id, 0, this};
         gm->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp, ENUMF_ALL | ENUMF_WILDMASK, sd.frm_receiver);
         if(!hlp.num_sent){
            char buf[5];
            buf[4] = 0;
            *(dword*)buf = sd.signal_id;
            ErrReport(C_fstr("SendSignal: signal %s did not find any recipient (sent to '%s')", sd.signal_id < 0x1000000 ? (const char*)C_fstr("%i", sd.signal_id) : (const char*)C_fstr("'%s'", buf), (const char*)sd.frm_receiver), editor);
         }
      }
   }

//----------------------------
public:

   ~C_script_manager_imp(){

      ClearAllThreads(NULL);
   }

//----------------------------

   virtual E_SCRIPT_RESULT LoadScript(PI3D_frame frm, const char *scriptname, C_game_mission *gm, bool run_main){

      assert(frm && scriptname);

      C_v_machine *vm = CreateVM();

      S_frame_info *fi = CreateFrameInfo(frm);
      fi->vm = vm;
      SetFrameInfo(frm, fi);

      C_script_manager::E_SCRIPT_RESULT ret = SR_OK;

      C_str orig_name = scriptname;
      orig_name.ToLower();
                                 //check if we have the script already cached
      t_script_map::iterator it = script_map.find(orig_name);
      S_script_source *ss;
      if(it!=script_map.end()){
                                 //cached version found, use this
         ss = &(*it).second;
      }else{
         ss = &(*script_map.insert(pair<C_str, S_script_source>(orig_name, S_script_source())).first).second;
                                 //load script now
         C_fstr filename("%s\\%s.scr", SCRIPT_DIR, scriptname);
         PC_script scr = CreateScript();

         scr->SetName(orig_name);
         ss->load_result = scr->Compile(filename,
   #if defined EDITOR && 1
            ISLCOMP_PRECOMPILE | ISLCOMP_PRECOMPILE_HIDDEN,
   #else
            ISLCOMP_FORCE_PRECOMPILED,
   #endif
            //0,
            SCRIPT_CMDLINE,
            CompileErr);
         ss->scr = scr;
         scr->Release();
      }
      ++ss->use_count;
   
      if(ISL_SUCCESS(ss->load_result)){
   #if defined _DEBUG && 0
         C_vector<char> msg;
         struct S_hlp{
            static void Dump(const char *cp, void *c){
               C_vector<char> &msg = *(C_vector<char>*)c;
               msg.insert(msg.end(), cp, cp+strlen(cp));
            }
         };
         scr->Dump(S_hlp::Dump, &msg);
         msg.push_back(0);
         ErrReport(msg.begin(), editor);
   #endif
         ISL_RESULT ir = vm->Load(ss->scr, GetLinkSymbols());
         if(ISL_FAIL(ir)){
            ret = SR_LINK_ERROR;
         }
      }else{
         //vm->SetScript(ss->scr);
         vm->Load(ss->scr, NULL);
         ret = SR_LOAD_ERROR;
      }
      vm->Release();

      if(ret)
         return ret;

      if(!run_main)
         return SR_OK;
      return RunFunction(frm, "Main", gm);
   }

//----------------------------

   virtual E_SCRIPT_RESULT UnloadScript(PI3D_frame frm){

      E_SCRIPT_RESULT sr = ClearAllThreads(frm);
      if(sr<0)
         return sr;

      PS_frame_info fi = GetFrameInfo(frm);
      assert(fi && fi->vm);
                                 //remove script
      CPC_script scr = fi->vm->GetScript();
      const C_str &sname = scr->GetName();
      t_script_map::iterator it = script_map.find(sname);
      assert(it!=script_map.end());
      S_script_source &ss = (*it).second;
      if(!--ss.use_count){
         script_map.erase(it);
      }
      fi->vm = NULL;
      SetFrameInfo(frm, fi);
      return SR_OK;
   }

//----------------------------

   virtual const VM_LOAD_SYMBOL *GetLinkSymbols() const{
      return script_symbols;
   }

//----------------------------

   virtual void ShutDown(){
      ClearAllThreads(NULL);
   }

//----------------------------
// Create new thread context, and add it into linked list.
   C_thread_context *CreateThreadContext(E_SUSPEND_MODE mode){

      PI3D_frame frm = GetCurrFrame();
      if(!frm){
         SCRIPT_LOG("CreateThreadContext: no current frm");
         return NULL;
      }
      PS_frame_info fi = GetFrameInfo(frm);
      assert(fi && fi->vm);

      C_thread_context *sc = new C_thread_context(frm, mode);
      sc->vm = fi->vm;
      ISL_RESULT ir;
      ir = fi->vm->SaveContext(&sc->save_context_id);
      assert(ISL_SUCCESS(ir));
                                 //push to front, so that when processing in ::Tick, we don't cause neverending loop
      script_threads.Add(sc);
      return sc;
   }

//----------------------------

   virtual void SuspendThread(){
      CreateThreadContext(SUSPEND_USER);
   }

//----------------------------
// Store signal data, to be processed synchronously.
   void SaveSignalData(const char *whom, int id){

      t_signals::iterator it;
      it = signals.insert(signals.end(), S_signal_data());
      (*it).from = curr_run_frame;
      (*it).frm_receiver = whom;
      (*it).signal_id = id;
   }

//----------------------------

   virtual E_SCRIPT_RESULT ClearAllThreads(PI3D_frame frm){

                                 //check all threads, erase those of this frame
      for(C_smart_ptr<C_thread_context> tc=script_threads; tc; tc = ++*tc){
         if(!frm || frm==tc->frm){
            tc->vm->ClearSavedContext(tc->save_context_id);
            tc->Release();
         }
      }
      return SR_OK;
   }

//----------------------------

   virtual bool ResumeThread(PI3D_frame frm, PC_game_mission gm){

      for(C_smart_ptr<C_thread_context> it=script_threads; it; ){
         C_thread_context *tc = it;
         it = ++*it;
         if(frm==tc->frm){
            if(tc->mode==SUSPEND_USER){
                                 //continue in program
               ResumeThread(tc, gm);
               return true;
            }
         }
      }
      return false;
   }

//----------------------------

   virtual void Tick(int time, PC_game_mission gm){

                                 //process all suspended scripts
                                 // go from back to front, so that
                                 // newly pushed suspends are not
                                 // processed in this loop
      for(C_smart_ptr<C_thread_context> it=script_threads; it; ){
         C_thread_context *tc = it;
         it = ++*it;

         bool del = false;       //thread should be deleted
         bool resume = false;    //thread should be resumed (and deleted)

         switch(tc->mode){
         case SUSPEND_PAUSE:
            if((tc->counter -= time) <= 0)
               resume = true;
            break;

         case SUSPEND_FADE_VOL:
            {
               float vol;
               if((tc->counter -= time) <= 0){
                  vol = tc->data1.fade_vol.dest_volume;
                  resume = true;
               }else{
                  float k = (float)tc->counter * tc->data1.fade_vol.r_init_time;
                  vol = tc->data1.fade_vol.dest_volume + tc->data1.fade_vol.volume_delta * k;
               }
               assert(tc->frm->GetType()==FRAME_SOUND);
               I3DCAST_SOUND(tc->frm)->SetVolume(vol);
            }
            break;

         case SUSPEND_FLASH:
            {
               int &cnt = tc->counter;
               float f = 0.0f;
               bool set = false;
               if((cnt -= time) < 0){
                  switch(tc->data1.flash.mode){
                  case C_thread_context::U_data::S_flash::FLASH_ATTACK:
                     cnt = tc->data1.flash.stay_count;
                     tc->data1.flash.mode = C_thread_context::U_data::S_flash::FLASH_STAY;
                     f = 1.0f;
                     set = true;
                     if(cnt)
                        break;
                                 //flow...
                  case C_thread_context::U_data::S_flash::FLASH_STAY:
                     cnt = tc->data1.flash.decay_count;
                     tc->data1.flash.mode = C_thread_context::U_data::S_flash::FLASH_DECAY;
                     if(cnt)
                        break;
                                 //flow...
                  case C_thread_context::U_data::S_flash::FLASH_DECAY:
                     resume = true;
                     f = 0.0f;
                     set = true;
                     break;
                  }
               }else{
                  switch(tc->data1.flash.mode){
                  case C_thread_context::U_data::S_flash::FLASH_ATTACK:
                     f = 1.0f - (float)cnt / (float)tc->data1.flash.attack_count;
                     set = true;
                     break;
                  case C_thread_context::U_data::S_flash::FLASH_DECAY:
                     f = (float)cnt / (float)tc->data1.flash.decay_count;
                     set = true;
                     break;
                  }
               }
               if(set){
                  f = ease_in_out[f] * tc->data1.flash.flash_value;
                  if(tc->frm->GetType()==FRAME_MODEL)
                     SetModelBrightness(I3DCAST_MODEL(tc->frm), f, I3D_VIS_BRIGHTNESS_EMISSIVE);
               }
            }
            break;

         case SUSPEND_MOVE:
            {
               C_thread_context::S_move_obj *data = (C_thread_context::S_move_obj*)tc->data;
               PI3D_frame move_frm = data->frm;
               if(!move_frm)
                  move_frm = tc->frm;
               if((tc->counter -= time) <= 0){
                  move_frm->SetPos(data->dest_pos);
                  move_frm->SetRot(data->dest_rot);
                  resume = true;
               }else{
                                 //compute interpolated pos/rot
                  float k = 1.0f - (float)tc->counter * data->r_init_time;
                  k = data->ease[k];
                  S_vector pos = data->dest_pos + data->delta_pos * (1.0f - k);
                  move_frm->SetPos(pos);
                  move_frm->SetRot(data->src_rot.Slerp(data->dest_rot, k, true));
               }
            }
            break;

         case SUSPEND_USER:
            break;

         default: assert(0);   
         }
         if(del || resume){
                                 //time for script to move the script on
                                 // if the script is suspended again,
                                 // the data are put at back of suspend list,
                                 // so we do erase this suspend context before the call
                                 // but we must keep scontext alive until the func returns
            if(del)
               tc->Release();
            else
               ResumeThread(tc, gm);
         }
      }
   }

//----------------------------

   virtual E_SCRIPT_RESULT RunFunction(PI3D_frame frm, const char *fnc_name, C_game_mission *gm,
      dword dw_param = 0, dword *ret_val = NULL, void *fnc_address = NULL){

      assert(frm && gm);
                                 //check if frame has script
      PS_frame_info fi = GetFrameInfo(frm);
      if(!fi || !fi->vm)
         return SR_NO_SCRIPT;
                                 //find specified function
      void *fp;
      if(fnc_name){
         fp = fi->vm->GetAddress(fnc_name);
         if(!fp)
            return SR_NO_FUNC;
      }else{
         fp = fnc_address;
      }

      C_smart_ptr<I3D_frame> save_curr_run_frame = curr_run_frame;
      C_smart_ptr<C_game_mission> save_curr_run_mission = curr_run_mission;
      //assert(!curr_run_mission);

      E_SCRIPT_RESULT result;

      curr_run_frame = frm;
      curr_run_mission = gm;

      ISL_RESULT ir = fi->vm->Run(ret_val, fp, VMR_ADDRESS, 1, dw_param);
      if(ISL_SUCCESS(ir)){
         switch(ir){
         case ISL_OK:
            result = SR_OK;
            break;
         case ISL_SUSPENDED:
            result = SR_SUSPENDED;
            break;
         default: assert(0); result = SR_RUN_ERROR;
         }
      }else{
         const char *err_msg = "unknown";
         switch(ir){
         case ISLERR_DIVIDE_BY_ZERO: err_msg = "divide-by-zero"; break;
         case ISLERR_NEVERENDING_LOOP: err_msg = "neverending loop"; break;
         case ISLERR_BAD_TABLE_INDEX: err_msg = "bad table index"; break;
         case ISLERR_INVADDRESS: err_msg = "invalid address"; break;
         case ISLERR_INVOPCODE: err_msg = "invalid opcode"; break;
         }
         ErrReport(C_fstr("script error: '%s' ('%s' set on frame '%s')",
            err_msg, fi->vm->GetScript()->GetName(), (const char*)frm->GetName()), editor);
         result = SR_RUN_ERROR;
      }

      curr_run_frame = save_curr_run_frame;
      curr_run_mission = save_curr_run_mission;

      AfterScriptCall(gm);

      return result;
   }

//----------------------------
private:
   enum E_SAVEGAME_SCRIPTMAN{
      SG_SAVED_THREAD,
         SG_FRM_NAME, //string
         SG_SUSPEND_DATA, //C_thread_context::SaveGame
         //CT_SG_SCRIPT_DATE = 0x1003, //dword[2] filetime (dtaGetTime)
      SG_GLOBALS,
      SG_MASTER_GLOBALS,
   };

   static I3DENUMRET I3DAPI SGSaveGlobals(PI3D_frame frm, dword c){
      PS_frame_info fi = GetFrameInfo(frm);
      if(fi && fi->vm){
         C_buffer<byte> data;
         fi->vm->GetGlobalVariables(data);
         if(data.size()){
            C_chunk &ck = *(C_chunk*)c;
            const C_str &fn = frm->GetName();

            ck <<= SG_GLOBALS;
            ck.Write((const char*)fn, fn.Size()+1);
            ck.Write(data.begin(), data.size());
            --ck;
         }
      }
      return I3DENUMRET_OK;
   }
public:
//----------------------------

   virtual void ReportError(const char *fnc_name, const char *msg) const{

      CPI3D_frame frm = GetCurrFrame();
      ErrReport(C_fstr("%s: %s (frame '%s')", fnc_name, msg, frm ? (const char*)frm->GetName() : "<no>"), editor);
   }

//----------------------------

   virtual void FreeScript(PI3D_frame frm, PC_game_mission gm, bool run_exit){
   
      if(run_exit)
         RunFunction(frm, "Exit", gm);
      UnloadScript(frm);
   }

//----------------------------
};
static C_script_manager_imp script_man_imp;

C_script_manager *script_man = &script_man_imp;

//----------------------------
//----------------------------
typedef const char *t_string;

//----------------------------
                              //script functions - implementation
static float __stdcall RandomF(){
   return S_float_random();
}

//----------------------------

static int __stdcall RandomI(int max){
   if(!max)
      return 0;
   return S_int_random(max);
}

//----------------------------// debugging //----------------------------

static void __stdcall LogS(t_string cp){

#ifdef EDITOR
   if(editor){
      PC_editor_item_Log ei = (PC_editor_item_Log)editor->FindPlugin("Log");
      if(ei)
         ei->AddText(cp, 0);
      //script_man_imp.debug_logs.push_back(cp);
   }
#endif
}

static void __stdcall LogI(int i){
   LogS(C_fstr("%i", i));
}

static void __stdcall LogF(float f){
   LogS(C_fstr("%.3f", f));
}

//----------------------------// timer //----------------------------

static void __stdcall Pause(int time){

                              //if zero time, we've nothing to do
   if(!time)
      return;
   C_script_manager_imp::C_thread_context *tc = script_man_imp.CreateThreadContext(C_script_manager_imp::SUSPEND_PAUSE);
   tc->counter = time;
}

//----------------------------

static void __stdcall SendSignal(t_string frm_receiver, int signal_id){

   script_man_imp.SaveSignalData(frm_receiver, signal_id);
}

//----------------------------

static void __stdcall SetObjectOn(bool on_off, t_string what){

   PI3D_frame frm;
   if(*what){
      frm = script_man_imp.GetCurrMission()->GetScene()->FindFrame(what);
      if(!frm){
         SCRIPT_ERR("SetObjectOn", C_fstr("can't find frame: '%s'", what));
         return;
      }
   }else{
      frm = script_man_imp.GetCurrFrame();
   }
   frm->SetOn(on_off);
}

//----------------------------

static void __stdcall SetObjectsParent(t_string what, t_string on){

   PI3D_frame frm;
   if(*what){
      frm = script_man_imp.GetCurrMission()->GetScene()->FindFrame(what);
      if(!frm){
         SCRIPT_ERR("SetObjectOn", C_fstr("can't find frame: %s", what));
         return;
      }
   }else{
      frm = script_man_imp.GetCurrFrame();
   }
   PI3D_frame f_on = script_man_imp.GetCurrMission()->GetScene()->FindFrame(on);
   frm->LinkTo(f_on);
}

//----------------------------

static bool __stdcall IsObjectOn(t_string what){

   PI3D_frame frm;
   if(*what){
      frm = script_man_imp.GetCurrMission()->GetScene()->FindFrame(what);
      if(!frm){
         SCRIPT_ERR("IsObjectOn", C_fstr("can't find frame: %s", what));
         return false;
      }
   }else{
      frm = script_man_imp.GetCurrFrame();
   }
   return frm->IsOn();
}

//----------------------------

static void __stdcall PlaySoundAmbient(t_string filename, float volume){

   script_man_imp.GetCurrMission()->PlayAmbientSound(filename, volume);
}

//----------------------------

static void __stdcall PlaySound(t_string filename, float min_dist, float max_dist, t_string where, float volume){

   PI3D_frame frm;
   if(*where){
      if(where[0]=='+')
         frm = script_man_imp.GetCurrFrame()->FindChildFrame(where+1);
      else
         frm = script_man_imp.GetCurrMission()->GetScene()->FindFrame(where);
      if(!frm){
         SCRIPT_ERR("PlaySound", C_fstr("can't find frame: %s", where));
         return;
      }
   }else{
      frm = script_man_imp.GetCurrFrame();
   }
   assert(filename);
   script_man_imp.GetCurrMission()->PlaySound(filename, min_dist, max_dist, frm, S_vector(0, 0, 0), volume);
}

//----------------------------

static void __stdcall FadeVolume(int time, float volume, t_string what){

   PI3D_frame frm;
   if(*what){
      if(what[0]=='+')
         frm = script_man_imp.GetCurrFrame()->FindChildFrame(what+1);
      else
         frm = script_man_imp.GetCurrMission()->GetScene()->FindFrame(what);
      if(!frm){
         SCRIPT_ERR("FadeVolume", C_fstr("can't find frame: %s", what));
         return;
      }
   }else{
      frm = script_man_imp.GetCurrFrame();
   }
   if(frm->GetType()!=FRAME_SOUND){
      SCRIPT_ERR("FadeVolume", "function requires frame of sound type");
      return;
   }
   if(!time){
      I3DCAST_SOUND(frm)->SetVolume(volume);
   }else{
      C_script_manager_imp::C_thread_context *tc = script_man_imp.CreateThreadContext(C_script_manager_imp::SUSPEND_FADE_VOL);
      tc->counter = time;
      tc->data1.fade_vol.r_init_time = 1.0f / (float)time;
      tc->data1.fade_vol.dest_volume = volume;
      tc->data1.fade_vol.volume_delta = I3DCAST_SOUND(frm)->GetVolume() - volume;
   }
}

//----------------------------

static void __stdcall SetVolume(float volume, t_string what){

   FadeVolume(0, volume, what);
}

//----------------------------

static void __stdcall MissionOver(E_MISSION_RESULT mr, t_string msg_id){
   script_man_imp.GetCurrMission()->MissionOver(mr, *msg_id ? msg_id : NULL);
}

//----------------------------

static void __stdcall EnableActor(bool on_off, t_string actor_name){
   
   if(*actor_name){
      int num_done = 0;
      const C_vector<C_smart_ptr<C_actor> > &actors = script_man_imp.GetCurrMission()->GetActorsVector();
      for(int i=actors.size(); i--; ){
         PC_actor act = (PC_actor)(CPC_actor)actors[i];
         if(act->GetName().Match(actor_name)){
            act->Enable(on_off);
            ++num_done;
         }
      }
      if(!num_done){
         SCRIPT_ERR("EnableActor", C_fstr("no actor matching to string: '%s'", actor_name));
      }
   }else{
      INIT_ACTOR("EnableActor");
      act->Enable(on_off);
   }
}

//----------------------------

static void __stdcall FlashObject(dword attack, dword stay, dword decay, float flash_value, t_string name){

   PI3D_frame frm;
   if(*name){
      frm = script_man_imp.GetCurrMission()->GetScene()->FindFrame(name);
      if(!frm){
         SCRIPT_ERR("FlashObject", C_fstr("can't find frame: %s", name));
         return;
      }
   }else{
      frm = script_man_imp.GetCurrFrame();
   }
   if(frm->GetType()!=FRAME_MODEL){
      SCRIPT_ERR("FlashObject", "only models may be flashed");
      return;
   }
   C_script_manager_imp::C_thread_context *tc = script_man_imp.CreateThreadContext(C_script_manager_imp::SUSPEND_FLASH);
   tc->data1.flash.attack_count = attack;
   tc->data1.flash.stay_count = stay;
   tc->data1.flash.decay_count = decay;
   tc->counter = attack;
   tc->data1.flash.flash_value = flash_value;
   tc->data1.flash.mode = C_script_manager_imp::C_thread_context::U_data::S_flash::FLASH_ATTACK;
}

//----------------------------

static void __stdcall SetBrightness(float value, t_string name){

   PC_game_mission gmis = script_man_imp.GetCurrMission();
   PI3D_frame frm;
   if(*name){
      frm = gmis->GetScene()->FindFrame(name);
      if(!frm){
         SCRIPT_ERR("SetBrightness", C_fstr("can't find frame: %s", name));
         return;
      }
   }else{
      frm = script_man_imp.GetCurrFrame();
   }
   if(frm->GetType()!=FRAME_MODEL){
      SCRIPT_ERR("SetBrightness", "only models may be changed");
      return;
   }
   SetModelBrightness(I3DCAST_MODEL(frm), value, I3D_VIS_BRIGHTNESS_NOAMBIENT);
}

//----------------------------

static void __stdcall SetAnimation(t_string filename, int stage, int blend_op, float scale, float blend, float speed){

   PI3D_frame frm = script_man_imp.GetCurrFrame();
   PC_game_mission gmis = script_man_imp.GetCurrMission();
   PI3D_animation_set as;
   I3D_RESULT ir = anim_cache.Create(filename, &as, gmis->GetScene());
   if(I3D_FAIL(ir)){
      ErrReport(C_fstr("SetAnimation: failed to load anim '%s'", filename), editor);
      return;
   }
   if(frm->GetType()!=FRAME_MODEL){
      ErrReport("SetAnimation: can set anims only on models", editor);
      as->Release();
      return;
   }
   PI3D_model mod = I3DCAST_MODEL(frm);
   mod->SetAnimation(stage, as, blend_op | I3DANIMOP_LOOP, scale, blend, speed);
   as->Release();
}

//----------------------------
//----------------------------
#define DEFINE_SYMBOL(name) {&name, #name}

const VM_LOAD_SYMBOL script_symbols[] = {
   DEFINE_SYMBOL(LogS),
   DEFINE_SYMBOL(LogI),
   DEFINE_SYMBOL(LogF),
   DEFINE_SYMBOL(Pause),

   DEFINE_SYMBOL(RandomF),
   DEFINE_SYMBOL(RandomI),
   DEFINE_SYMBOL(PlaySoundAmbient),
   DEFINE_SYMBOL(PlaySound),
   DEFINE_SYMBOL(FadeVolume),
   DEFINE_SYMBOL(SetVolume),
   DEFINE_SYMBOL(SetObjectOn),
   DEFINE_SYMBOL(SetObjectsParent),
   DEFINE_SYMBOL(IsObjectOn),
   DEFINE_SYMBOL(SendSignal),
   DEFINE_SYMBOL(SetAnimation),

   DEFINE_SYMBOL(MissionOver),

   DEFINE_SYMBOL(FlashObject),
   DEFINE_SYMBOL(SetBrightness),

   DEFINE_SYMBOL(EnableActor),

   NULL
};

//----------------------------// Human script methods end //----------------------------
//----------------------------
