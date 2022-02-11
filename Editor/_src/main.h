#ifndef __MAIN_H_
#define __MAIN_H_


#ifdef _MSC_VER
using namespace std;
#endif

#include <algorithm>

#define CAM_NEAR_RANGE .2f

//----------------------------

extern S_game_configuration game_configuration;

//----------------------------
                              //globals
extern PI3D_driver driver;
extern PIGraph igraph;
extern class ISND_driver *isound;
extern C_all_texts all_txt;

#ifdef EDITOR
extern PC_editor editor;
#else
#define editor NULL
#endif

//----------------------------
//----------------------------
// Error reporting function/empty macro for various problems - also to be passed into
// C_I3D_cache::Open and C_I3D_cache::Create functions.
// Reports specified error (text message) to Log window.
// Note: context parameter must be pointer to C_editor, or NULL.
//----------------------------

#ifdef EDITOR

void ErrReport(const char *cp, void *context);
void LogReport(const char *cp, void *context, dword color = 0);

#else //!EDITOR && NDEBUG

void LogFunc(const char *cp);
inline void ErrReport(const char *cp, void *context){ LogFunc(cp); }
inline void LogReport(const char *cp, void *context, dword color = 0){ LogFunc(cp); }

#endif   //!EDITOR

inline void LOG(const char *cp, dword c=0){ LogReport(cp, editor, c); }
inline void LOG(int i){ LogReport(C_fstr("%i", i), editor); }
inline void LOG(float f){ LogReport(C_fstr("%.2f", f), editor); }
inline void LOG(const S_vector &v){ LogReport(C_fstr("[%.2f, %.2f, %.2f]", v.x, v.y, v.z), editor); }

//----------------------------
                              //debugging lines and point
#ifdef EDITOR
inline void DebugLine(const S_vector &from, const S_vector &to, int type, dword color = 0xffffffff, int time = 500){
   PC_editor_item_DebugLine ei = (PC_editor_item_DebugLine)editor->FindPlugin("DebugLine");
   if(ei)
      ei->AddLine(from, to, (E_DEBUGLINE_TYPE)type, color, time);
}
inline void DebugLineClear(){
   PC_editor_item_DebugLine ei = (PC_editor_item_DebugLine)editor->FindPlugin("DebugLine");
   if(ei)
      ei->ClearAll();
}
inline void DebugVector(const S_vector &from, const S_vector &dir, int type, dword color = 0xffffffff, int time = 500){
   DebugLine(from, from+dir, type, color, time);
}
inline void DebugPoint(const S_vector &from, float radius, int type, dword color = 0xffffffff, int time = 500){
   PC_editor_item_DebugLine ei = (PC_editor_item_DebugLine)editor->FindPlugin("DebugLine");
   if(ei)
      ei->AddPoint(from, radius, (E_DEBUGLINE_TYPE)type, color, time);
}
#define PRINT(cp) editor->PRINT(cp)
#define DEBUG(cp) editor->DEBUG(cp)

#else//EDITOR

inline void DebugLine(const S_vector &from, const S_vector &dir, int type, dword color = 0xffffffff, int time = 500){}
inline void DebugPoint(const S_vector &from, float radius, int type, dword color = 0xffffffff, int time = 500){}
inline void DebugLineClear(){}
#define PRINT(cp) LOG(cp)
#define DEBUG(cp)

#endif//!EDITOR

//----------------------------

#ifdef EDITOR

class C_editor_item_Mission_spec: public C_editor_item_Mission{
public:
   virtual class C_game_mission *GetMission() = 0;
};
typedef C_editor_item_Mission_spec *PC_editor_item_Mission_spec;

#endif

//----------------------------
//----------------------------
                              //mission loader - options
enum E_MISSION_OPEN_FLAGS{
   OPEN_MERGE_MODE = 1,       //merge with current scene
                              //created frames:
   OPEN_MERGE_SOUNDS = 2,          
   OPEN_MERGE_LIGHTS = 4,
   OPEN_MERGE_MODELS = 8,
   OPEN_MERGE_VOLUMES = 0x10,
   OPEN_MERGE_DUMMYS = 0x20,
   OPEN_MERGE_CAMERAS = 0x40,
   OPEN_MERGE_TARGETS = 0x80,
   OPEN_MERGE_OCCLUDERS = 0x100,
                              
   OPEN_MERGE_LIGHTMAPS = 0x10000, //lightmap info
   OPEN_MERGE_SCRIPTS = 0x20000,   
   OPEN_MERGE_ACTORS = 0x40000,
   OPEN_MERGE_CHECKPOINTS = 0x80000,

   OPEN_MERGE_SETTINGS = 0x100000,     //scene settings (ranges, FOV, abckground)

   OPEN_NO_SHOW_PROGRESS = 0x200000,   //don't show progress indicator
   OPEN_NO_EDITOR = 0x400000, //don't load editor info
   OPEN_LOG = 0x800000,       //log loading
   OPEN_MODEL = 0x1000000,    //opening model, not affecting scene
};

//----------------------------

enum E_MISSION_IO{            //file I/O statuse, used for C_mission::Open and Save
   MIO_OK,                    //operation succeeded
   MIO_CORRUPT,               //file corrupted
   MIO_ACCESSFAIL,            //access failed (e.g. write operation on read-only file)
   MIO_NOFILE,                //cannot find specified file
   MIO_CANCELLED,             //cancelled by user
};

//----------------------------
                              //fast fix: C_smart_ptr without defined operator&, which causes problems with list<>
template<class T>
class C_smart_ptr_1{
protected:
   T *ptr;
public:
//----------------------------
// Default constructor - initialize to NULL.
   inline C_smart_ptr_1(): ptr(0){}

//----------------------------
// Constructor from pointer to T. If non-NULL, reference is increased.
   inline C_smart_ptr_1(T *tp): ptr(tp){ if(ptr) ptr->AddRef(); }

//----------------------------
// Constructor from reference to smartpointer. If ptr non-NULL, reference is increased.
   inline C_smart_ptr_1(const C_smart_ptr_1<T> &sp): ptr(sp.ptr){ if(ptr) ptr->AddRef(); }

//----------------------------
// Destructor - releasing reference if non-NULL pointer.
   inline ~C_smart_ptr_1(){ if(ptr) ptr->Release(); }

//----------------------------
// Assignment from pointer to T. References to non-NULL pointers adjusted.
   inline C_smart_ptr_1 &operator =(T *tp){
      if(ptr!=tp){
         if(tp) tp->AddRef();
         if(ptr) ptr->Release();
         ptr = tp;
      }
      return *this;
   }

//----------------------------
// Assignment from reference to smartpointer. References to non-NULL pointers adjusted.
   inline C_smart_ptr_1 &operator =(const C_smart_ptr_1 &sp){
      if(ptr!=sp.ptr){
         if(sp.ptr) sp.ptr->AddRef();
         if(ptr) ptr->Release();
         ptr = sp.ptr;
      }
      return *this;
   }

//----------------------------
// Three pointer-access operators. Const and non-const versions.
   inline operator T *(){ return ptr; }
   inline operator const T *() const{ return ptr; }
   inline T &operator *(){ return *ptr; }
   inline const T &operator *() const{ return *ptr; }
   inline T *operator ->(){ return ptr; }
   inline const T *operator ->() const{ return ptr; }

//----------------------------
// Boolean comparison of pointer.
   inline bool operator !() const{ return (!ptr); }
   inline bool operator ==(const T *tp) const{ return (ptr==tp); }
   inline bool operator ==(const C_smart_ptr_1<T> &s) const{ return (ptr == s.ptr); }
   inline bool operator !=(const T *tp) const{ return (ptr!=tp); }
   inline bool operator !=(const C_smart_ptr_1<T> &s) const{ return (ptr != s.ptr); }

//----------------------------
// Comparison of pointers.
   inline bool operator <(const C_smart_ptr_1<T> &s) const{ return (ptr < s.ptr); }
};

//----------------------------
                              //mission class
#define ANIM_CACHE_SIZE 128

class C_mission: public C_class_to_be_ticked{
   friend class C_edit_Mission;
protected:
                              //fixed data, not changing frequently (const-like for given mission):
                              //camera settings
   float cam_range, cam_edit_range;
   float cam_fov;
   mutable dword mission_crc;
   C_str mission_name;        //filename of currently loaded mission


                              //temp (counters, etc)
                              //loader progress indicator data
   class C_loader_progress{
   public:
      int last_bit_i;         //last progress (percent)
      int last_render_time;   //last progress display time
      C_smart_ptr<IImage> back_img; //saved copy of backbuffer image
   } load_progress;


                              //composites:
   PI3D_scene scene;          //3D scene on which this class operates


                              //run-time:
   dword last_render_time;    //time of last scene rendering (I3D_scene::GetLastRenderTime)




   dword GraphCallback(dword msg, dword par1, dword par2);
   static GRAPHCALLBACK GraphCallback_thunk;


   static bool I3DAPI cbLoadScene(I3D_LOADMESSAGE msg, dword context, dword prm2, void *context1);

   void InitializeLoadProgress();
   void CloseLoadProgress(){
      load_progress.back_img = NULL;
   }
   void DisplayLoadProgress(float progress);

public:
   class C_frm: public C_smart_ptr_1<I3D_frame>{
   public:
      C_frm(PI3D_frame f): C_smart_ptr_1<I3D_frame>(f){}
   };
   typedef set<C_frm, less<CPI3D_frame> > t_model_frames;

   struct S_load_context{
      C_chunk ck;
      map<C_str, PI3D_frame> frame_map;
      bool merge_mode;
      dword open_flags;
      PI3D_camera cam;
      float ortho_scale;
      bool is_ortho;
      bool cancelled;         //set to true if user presses ESC
      PI3D_model loaded_model;
      t_model_frames model_frames;  //frames to be added to model after loading (due to renaming problems)
      PI3D_container container;
      float cam_range;
      float cam_edit_range;
      float cam_fov;
                              //load callback
      I3D_LOAD_CB_PROC *load_cb_proc;
      void *load_cb_context;
#ifdef EDITOR
      PC_editor_item_Modify e_modify;
      PC_editor_item_Log e_log;
      PC_editor_item e_undo, e_script;
      C_vector<C_str> err_list;
#endif
      C_str open_dir;
      S_load_context():
#ifdef EDITOR
         e_log(NULL),
         e_undo(NULL),
         e_modify(NULL),
#endif
         merge_mode(false),
         open_flags(0),
         cam_range(100.0f),
         cam_edit_range(0.0f),
         cam_fov(PI*.5f),
         is_ortho(false),
         ortho_scale(1.0f),
         last_progress_pos(0),
         loaded_model(NULL),
         container(NULL),
         cancelled(false),
         cam(NULL),
         ck(256*1024)
      {}

      int last_progress_pos;

//----------------------------
// This func returns true is user wants to cancel loading.
      bool ShowProgress(){
         int pos = ck.GetHandle()->tellg();
         if(pos<last_progress_pos || (pos-last_progress_pos) > 128){
            last_progress_pos = pos;
            float f = (float)pos / (float)ck.GetHandle()->filesize();
            cancelled = (*load_cb_proc)((I3D_LOADMESSAGE)10000, *(dword*)&f, 1, load_cb_context);
            return cancelled;
         }
         return false;
      }

#ifdef EDITOR
      void Log(const char *t, PI3D_frame frm, dword color){
         if(e_log && find(err_list.begin(), err_list.end(), t)==err_list.end()){
            if(!err_list.size()){
               C_fstr str("*** Loading scene: %s ***", (const char*)open_dir);
               e_log->AddText(str, color);
            }
            err_list.push_back(t);
            e_log->AddText(t, color);
            if(frm){
               PC_editor_item_Selection e_slct = (PC_editor_item_Selection)editor->FindPlugin("Selection");
               if(e_slct)
                  e_slct->FlashFrame(frm, 0, 0xffff0000);
            }
         }
      }
      void ErrLog(const char *t, PI3D_frame frm){ Log(t, frm, 0xff0000); }
      void WarnLog(const char *t, PI3D_frame frm){ Log(t, frm, 0x008000); }
#else //EDITOR
      inline void ErrLog(const char *msg, PI3D_frame){ LogFunc(msg); }
      inline void WarnLog(const char *msg, PI3D_frame){ LogFunc(msg); }
#endif//!EDITOR
   };
protected:

   virtual bool LoadChunk(CK_TYPE, S_load_context&);

public:
   inline float GetCameraRange() const{ return cam_range; }
   inline float GetCameraEditRange() const{ return cam_edit_range; }
   inline float GetCameraFOV() const{ return cam_fov; }


public:
   C_mission();
   virtual ~C_mission();

   virtual E_MISSION_IO Open(const char *dir, dword open_flags, I3D_LOAD_CB_PROC* = NULL, void *load_cb_context = NULL);
   virtual void Close(dword close_flags = 0);
   E_MISSION_IO Save(const char *name, dword save_group) const;

//----------------------------
// Load model, use mission's scene for adding model's volumes and sectors to (no other changes to mission are done).
   E_MISSION_IO LoadModel(PI3D_model, const char *dir, I3D_LOAD_CB_PROC* = NULL, void *load_cb_context = NULL) const;

//----------------------------
// Get name of currently open mission.
   inline const C_str &GetName() const{ return mission_name; }

//----------------------------
// Get CRC of currently open mission.
   inline dword GetCRC() const{ return mission_crc; }

//----------------------------
// Get CRC of mission on disk.
   static bool GetMissionCRC(const C_str &mission_name, dword &out_crc);

   virtual dword GetID() const{ return 'MISS'; }
   virtual void Render();
   virtual void Tick(const S_tick_context &tc);
   inline PI3D_scene GetScene() const{ return scene; }

//----------------------------
// Function called when screen is reset (e.g. resolution is changed).
   virtual void ResetScreen();
};

//----------------------------
//----------------------------

extern C_I3D_sound_cache sound_cache;
//----------------------------
// cache base class, having pushed pointer to C_mission, allowing using C_mission for loading files
class C_loader_mission_cache{
protected:
   C_vector<C_mission*> work_missions;
public:
   void PushWorkMission(C_mission *m){
      work_missions.push_back(m);
   }
   void PopWorkMission(C_mission *m){
      for(int i=work_missions.size(); i--; ){
         if(work_missions[i]==m){
            work_missions.erase(work_missions.begin()+i);
            break;
         }
      }
      assert(i!=-1);
   }
   void Close(){ work_missions.clear(); }
};

//----------------------------
// anim cache - allowing loading animations from either .bin files, or .i3d files
class C_I3D_special_anim_cache: public C_I3D_cache<I3D_animation_set>, public C_loader_mission_cache{
   virtual void Duplicate(PI3D_animation_set dst, PI3D_animation_set src){}
   virtual PI3D_animation_set CreateNew(PI3D_scene scn) const{
      return scn->GetDriver()->CreateAnimationSet();
   }
   virtual I3D_RESULT OpenFile(PI3D_animation_set, const char *dir, const char *filename, dword flags,
      I3D_LOAD_CB_PROC *cb_proc, void *cb_context);
};

extern C_I3D_special_anim_cache anim_cache;

//----------------------------
// model cache, allowing loading files from either model directory, or from missions
class C_I3D_model_cache_special: public C_I3D_model_cache, public C_loader_mission_cache{
public:
   virtual I3D_RESULT OpenFile(PI3D_model, const char *dir, const char *filename, dword flags,
      I3D_LOAD_CB_PROC *cb_proc, void *cb_context);
};

extern C_I3D_model_cache_special model_cache;

//----------------------------
                              //structure attached to frame,
                              // containing info about frame's properties
                              //it's put into frame's user data, by
                              // I3D_frame::SetData()

struct S_frame_info{
   C_smart_ptr<C_v_machine> vm;
   class C_actor *actor;      //NULL if no actor set on frame

   S_frame_info():
      actor(NULL)
   {}

   bool IsNull() const{
      return
         !vm &&
         !actor;
   }
};

typedef S_frame_info *PS_frame_info;
                              //get existing S_frame_info from frame,
                              // or NULL if not set
inline PS_frame_info GetFrameInfo(CPI3D_frame frm){
   return (PS_frame_info)frm->GetData();
}

                              //get existing S_frame_info from frame,
                              // if not set, create new and attach
inline PS_frame_info CreateFrameInfo(PI3D_frame frm){
   PS_frame_info fi = (PS_frame_info)frm->GetData();
   if(!fi){ fi = new S_frame_info; frm->SetData((dword)fi); }
   return fi;
}

inline C_actor *GetFrameActor(CPI3D_frame frm){
   PS_frame_info fi = GetFrameInfo(frm);
   return fi ? fi->actor : NULL;
}

//----------------------------
// Set frame info, if it's contents is zero,
// delete it and remove from frame.
// Return new pointer, which may be NULL.
inline PS_frame_info SetFrameInfo(PI3D_frame frm, S_frame_info *fi){
   if(fi && fi->IsNull()){ delete fi; fi = NULL; }
   frm->SetData((dword)fi);
   return fi;
}

//----------------------------
//----------------------------
// Named controller slot indicies.
//----------------------------
enum E_CONTROLLER_SLOT{
                              //basic movement
   CS_TURN_LEFT,
   CS_MOVE_FORWARD,
   CS_TURN_RIGHT,
   CS_MOVE_BACK,
   CS_MOVE_LEFT,
   CS_MOVE_RIGHT,
   CS_JUMP,
                              //stay mode change
   CS_STAY_UP, //obsolote
   CS_STAY_DOWN,
//   CS_RESET_AIM,

   CS_RUN,
   //CS_STRAFE,
   CS_SLANT_LEFT,
   CS_SLANT_RIGHT,
   CS_FIRE,
   CS_ALT_FIRE,
   CS_RELOAD,
   CS_FREE_LOOK, //obsolote
   CS_INV_SCROLL_LEFT,
   CS_INV_SCROLL_RIGHT,
   //CS_INV_USE,
   CS_INV_HIDE,
   CS_USE,
//   CS_SHIELD,

   //CS_PLAYER_SWITCH,
   CS_SAVEGAME,
   CS_LOADGAME,

   CS_QUICKSAVE,
   CS_QUICKLOAD,

                              //weapons
   CS_WEAPON_1,
   CS_WEAPON_2,
   CS_WEAPON_3,
   CS_WEAPON_4,
   CS_WEAPON_5,
   CS_WEAPON_6,
   CS_WEAPON_7,
   CS_WEAPON_NO,
   CS_UNUSED1,

                              //camera management
   CS_CAM_ZOOM_IN,
   CS_CAM_ZOOM_OUT,
   CS_CAM_ZOOM_UP,
   CS_CAM_ZOOM_DOWN,
                              //game management
   //CS_GAME_MOUSE_MODE,
   //CS_HELP,
   CS_SCREENSHOT,
//   CS_PANEL_IN,
//   CS_PANEL_OUT,
   CS_GAME_ESCAPE,
   CS_OBJECTIVES,

   CS_DEBUG,
   CS_FAST_QUIT,

   CS_POP_UP_SELECT,
   CS_POP_UP_UP,
   CS_POP_UP_DOWN,
   //CS_SWITCH_PANEL,

   CS_DROP,

   CS_LAST
};

//----------------------------
//----------------------------
#ifdef _WINDOWS_
#error "windows.h cannot be included!"
#endif
//----------------------------
#endif