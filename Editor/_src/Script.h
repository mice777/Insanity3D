#ifndef __SCRIPT_H_
#define __SCRIPT_H_

//----------------------------

#ifdef EDITOR

enum E_ACTION_SCRIPT{
   E_SCRIPT_LIST,             // - init script list sheet
   E_SCRIPT_TOGGLE_NAMES,     // - togle drawing script names
   E_SCRIPT_CONFIG,           // - configure plugin
   E_SCRIPT_EDIT_SELECTION,   // - assign script, or edit script on selected frame
   E_SCRIPT_ASSIGN_EDITTAB,   // - edit script's table on selected frame
   E_SCRIPT_IS_UP_TO_DATE,    //(const char *name); ret: bool - check if script is up-to-date
   E_SCRIPT_COMPILE,          //(const char *name); ret: bool success - compile selected script
   E_SCRIPT_EDIT_MASTER,      //- edit master script

   E_SCRIPT_SLCT_NOTIFY,      // - internal (selection change notification)
   E_SCRIPT_SET_NAMED,        // - internal (undo)
   E_SCRIPT_REMOVE_NAMED,     // - internal (undo)
   E_SCRIPT_SET_TABLE,        // - internal (undo)
};

#endif

//----------------------------
//----------------------------
class C_game_mission;

//----------------------------

class C_script_manager{
protected:
   C_smart_ptr<I3D_frame> curr_run_frame;
   C_game_mission *curr_run_mission;

   bool load_debug_dlls;

                              //info kept about each loaded script
   struct S_script_source{
      C_smart_ptr<C_script> scr;
      int use_count;
      ISL_RESULT load_result;

      S_script_source(): use_count(0){}
   };
   friend class C_edit_Script;
                              //mapping of script name to actual script
   typedef map<C_str, S_script_source> t_script_map;
   t_script_map script_map;

//----------------------------
// Protected constructor - creation allowed only by inherited classes
   C_script_manager():
      curr_run_mission(NULL),
      curr_run_frame(NULL),
      load_debug_dlls(false)
   {}
public:
   virtual ~C_script_manager(){}

   enum E_SCRIPT_RESULT{
      SR_OK,                  //operation successful
      SR_SUSPENDED,           //script run, but is suspended now
      SR_LOAD_ERROR = 0x80000001,   //specified script can't be loaded
      SR_LINK_ERROR = 0x80000002,   //failed to link script properly
      SR_NO_SCRIPT = 0x80000003,    //specified frame has no script
      SR_NO_FUNC = 0x80000004,      //specified function wasn't found
      SR_REENTERED = 0x80000005,    //script was re-entered, other script hasn't returned yet
      SR_RUN_ERROR = 0x80000006,    //run-time error encountered (divide by zero, crash, etc)
   };

//----------------------------
// Load script on specified frame.
// Parameters:
//    frm - name of frame to set script on
//    scriptname - name of script
//    gm - current game mission
//    run_main - flag specifying if Main function should be run
   virtual E_SCRIPT_RESULT LoadScript(PI3D_frame frm, const char *scriptname, C_game_mission *gm, bool run_main) = 0;
   virtual E_SCRIPT_RESULT UnloadScript(PI3D_frame frm) = 0;

//----------------------------
// Shut down everything.
   virtual void ShutDown() = 0;

//----------------------------
// Clear threads of specified frame, or all threads if 'frm' is NULL.
   virtual E_SCRIPT_RESULT ClearAllThreads(PI3D_frame frm) = 0;

//----------------------------
// Get pointer to link symbols.
   virtual const VM_LOAD_SYMBOL *GetLinkSymbols() const = 0;

//----------------------------
// Run specified function of the script.
// Parameters:
//    frm - name of frame to set script on
//    fnc_name - name of function to run
//    gm - current game mission
   virtual E_SCRIPT_RESULT RunFunction(PI3D_frame frm, const char *fnc_name, C_game_mission *gm,
      dword dw_param = 0, dword *ret_val = NULL, void *fnc_address = NULL) = 0;

//----------------------------
// Free specified script of specified frame.
   virtual void FreeScript(PI3D_frame, C_game_mission*, bool run_exit) = 0;

//----------------------------
// Suspend thread for currently run script. The script may be resumed later by calling ResumeThread.
   virtual void SuspendThread() = 0;

//----------------------------
// Resume user thread of given frame.
   virtual bool ResumeThread(PI3D_frame frm, C_game_mission *gm) = 0;

//----------------------------
// Cet currently executing frame - useful for script functions acting on
// current frame.   
   inline PI3D_frame GetCurrFrame(){ return curr_run_frame; }
   inline CPI3D_frame GetCurrFrame() const{ return curr_run_frame; }

//----------------------------
// Get currently run mission - useful for script functions acting on
// mission's resources (actors, scene, sounds, etc).
   inline C_game_mission *GetCurrMission() const{ return curr_run_mission; }

//----------------------------
// Update all scripts.
   virtual void Tick(int time, C_game_mission* = NULL) = 0;

//----------------------------
// Report script error - display name of currently run frame, function name, error message, etc.
   virtual void ReportError(const char *fnc_name, const char *msg) const = 0;

//----------------------------
// Set debug mode through this function.
   inline void SetDebugMode(bool b){ load_debug_dlls = b; }

//----------------------------
};

extern C_script_manager *script_man;

//----------------------------
//----------------------------

#endif
