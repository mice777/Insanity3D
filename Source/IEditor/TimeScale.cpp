#include "all.h"
#include "common.h"

//----------------------------

static const float time_scales[] = {0.0f, .05f, .1f, .2f, .5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f};

//----------------------------

class C_editor_item_TimeScale_imp: public C_editor_item_TimeScale{
   virtual const char *GetName() const{ return "TimeScale"; }

//----------------------------

   enum E_ACTION_TIMESCALE{
      E_TIMESCALE_0 = 20000,     //0 - stop timer
      E_TIMESCALE_5,
      E_TIMESCALE_10,
      E_TIMESCALE_20,   
      E_TIMESCALE_50,   
      E_TIMESCALE_100,           //100%
      E_TIMESCALE_200,  
      E_TIMESCALE_500,  
      E_TIMESCALE_1000,
      E_TIMESCALE_2000,          //2000%

      E_DEBUG_SIM_FPS_OFF,
      E_DEBUG_SIM_FPS_5,
      E_DEBUG_SIM_FPS_10,
      E_DEBUG_SIM_FPS_15,
      E_DEBUG_SIM_FPS_20,
      E_DEBUG_SIM_FPS_50,
      E_DEBUG_SIM_FPS_100,
   };

//----------------------------

   int time_scale_id;

//----------------------------

   int simulate_fps;          //0 = off
   float sim_fps_last_timer;
                              //value of last frame (valid if simulating FPS)
                              // kept for correction of time scale, if real FPS is lower than simulated
   float last_frame_time;

   void MarkFPSSimMenu(){
      ed->CheckMenu(this, E_DEBUG_SIM_FPS_5,  (simulate_fps== 5));
      ed->CheckMenu(this, E_DEBUG_SIM_FPS_10, (simulate_fps==10));
      ed->CheckMenu(this, E_DEBUG_SIM_FPS_15, (simulate_fps==15));
      ed->CheckMenu(this, E_DEBUG_SIM_FPS_20, (simulate_fps==20));
      ed->CheckMenu(this, E_DEBUG_SIM_FPS_50, (simulate_fps==50));
      ed->CheckMenu(this, E_DEBUG_SIM_FPS_100, (simulate_fps==100));
   }

//----------------------------

   virtual float GetScale() const{

      float ret = time_scales[time_scale_id];
                              //if simulated fps is on, make sure fps is exactly that big
                              // otherwise slow down returned scale
      if(simulate_fps){
         float sleep_time = (1000.0f/(float)simulate_fps);
         if(sleep_time < last_frame_time)
            ret = sleep_time / last_frame_time;
      }
      return ret;
   }

//----------------------------
public:
   C_editor_item_TimeScale_imp():
      simulate_fps(0),
      time_scale_id(5),       //default is 100%
      last_frame_time(0)
   {}

   virtual bool Init(){
                              //add menu items (and keyboard short-cuts)
#define MB "%30 &Debug\\%80 &Time scale\\"
      ed->AddShortcut(this, E_TIMESCALE_0,   MB"Stop\tNum0", K_NUM0, 0);
      ed->AddShortcut(this, E_TIMESCALE_5,   MB"5 %\tNum1", K_NUM1, 0);
      ed->AddShortcut(this, E_TIMESCALE_10,  MB"10 %\tNum2", K_NUM2, 0);
      ed->AddShortcut(this, E_TIMESCALE_20,  MB"20 %\tNum3", K_NUM3, 0);
      ed->AddShortcut(this, E_TIMESCALE_50,  MB"50 %\tNum4", K_NUM4, 0);
      ed->AddShortcut(this, E_TIMESCALE_100, MB"100 %\tNum5", K_NUM5, 0);
      ed->AddShortcut(this, E_TIMESCALE_200, MB"2 x\tNum6", K_NUM6, 0);
      ed->AddShortcut(this, E_TIMESCALE_500, MB"5 x\tNum7", K_NUM7, 0);
      ed->AddShortcut(this, E_TIMESCALE_1000, MB"10 x\tNum8", K_NUM8, 0);
      ed->AddShortcut(this, E_TIMESCALE_2000, MB"20 x\tNum9", K_NUM9, 0);

#undef MB
#define MB "%30 &Debug\\%60Simulate &FPS\\"
      ed->AddShortcut(this, E_DEBUG_SIM_FPS_OFF, MB"off", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUG_SIM_FPS_5, MB"5", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUG_SIM_FPS_10, MB"10", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUG_SIM_FPS_15, MB"15", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUG_SIM_FPS_20, MB"20", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUG_SIM_FPS_50, MB"50", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUG_SIM_FPS_100, MB"100", K_NOKEY, 0);

      ed->CheckMenu(this, E_TIMESCALE_100, true);
      return true;
   }
   //virtual void Close(){ }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(simulate_fps){
                              //slow down so that we get desired FPS
         float sleep_time = (1000.0f/(float)simulate_fps);
                              //sleep for the time,
                              // minus time really elapsed
         last_frame_time = ed->GetIGraph()->ReadTimer() - sim_fps_last_timer;
         sleep_time -= last_frame_time;
         sleep_time -= .3f;
         int sleep_time_i = FloatToInt(sleep_time);
         if(sleep_time_i >= 1){
            Sleep(sleep_time_i);
         }
         sim_fps_last_timer = ed->GetIGraph()->ReadTimer() + (float)fmod(sleep_time, 1.0f);
      }
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){
      case E_TIMESCALE_0:
      case E_TIMESCALE_5:
      case E_TIMESCALE_10:
      case E_TIMESCALE_20:
      case E_TIMESCALE_50:
      case E_TIMESCALE_100:
      case E_TIMESCALE_200:
      case E_TIMESCALE_500:
      case E_TIMESCALE_1000:
      case E_TIMESCALE_2000:
         {
            ed->CheckMenu(this, time_scale_id, false);
            time_scale_id = id - E_TIMESCALE_0;
            ed->CheckMenu(this, time_scale_id, true);
            ed->Message(C_fstr("Time scale set to %i%%", (int)(time_scales[time_scale_id]*100.0f)));
         }
         break;

      case E_DEBUG_SIM_FPS_OFF:
      case E_DEBUG_SIM_FPS_5:
      case E_DEBUG_SIM_FPS_10:
      case E_DEBUG_SIM_FPS_15:
      case E_DEBUG_SIM_FPS_20:
      case E_DEBUG_SIM_FPS_50:
      case E_DEBUG_SIM_FPS_100:
         {
            static const int sim_fps_table[] = {
               0, 5, 10, 15, 20, 50, 100
            };
            simulate_fps = sim_fps_table[id-E_DEBUG_SIM_FPS_OFF];
            sim_fps_last_timer = (float)ed->GetIGraph()->ReadTimer();
            MarkFPSSimMenu();
         }
         break;
      }
      return 0;
   }

//----------------------------

   //virtual void Render(){}

//----------------------------

   enum{
      CT_SCALE,
      CT_CIM_FPS,
   };

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      ck
         (CT_SCALE, time_scale_id)
         (CT_CIM_FPS, simulate_fps)
         ;
      return true;
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){
                              //compatibility (remove soon)
      if(ck.Size()==4) return false;

      ed->CheckMenu(this, E_TIMESCALE_0 + time_scale_id, false);
      while(ck)
      switch(++ck){
      case CT_SCALE: ck >> time_scale_id; break;
      case CT_CIM_FPS: ck >> simulate_fps; break;
      default: --ck;
      }

      ed->CheckMenu(this, E_TIMESCALE_0 + time_scale_id, true);
      MarkFPSSimMenu();

      sim_fps_last_timer = (float)ed->GetIGraph()->ReadTimer();
      return true;
   }
};

//----------------------------

void CreateTimeScale(PC_editor ed){
   PC_editor_item ei = new C_editor_item_TimeScale_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
