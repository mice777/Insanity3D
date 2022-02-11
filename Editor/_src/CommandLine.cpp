#include "all.h"
#include "commandline.h"
#include <process.h>

//----------------------------

const C_command_line_base::S_scan_data *C_command_line::GetScanData() const{
   static const S_scan_data scan_data[] = {
      {"help", CMD_NOP, 0, (t_Callback)&C_command_line::ShowHelp},
      {"releasedll", CMD_NOP},
      {"debugdll", CMD_BOOL, offsetof(C_command_line, debug_use_debug_dlls)},
      {"mission", CMD_STRING, offsetof(C_command_line, cmdline_mission)},
#ifdef EDITOR
      {"game_mode", CMD_BOOL, offsetof(C_command_line, debug_game_mode)},
#endif
      {"no_suspend", CMD_BOOL, offsetof(C_command_line, disable_suspend)},
      {"config", CMD_BOOL, offsetof(C_command_line, run_config)},
      {"profile", CMD_BOOL, offsetof(C_command_line, debug_profile)},
      NULL
   };
   return scan_data;
}

//----------------------------
                              //macros causing to include specified help message
                              // only if EDITOR is #defined
#ifdef EDITOR
#define CLHELP_EDIT_TEXT(t) t
#else
#define CLHELP_EDIT_TEXT(t)
#endif


#ifdef _DEBUG
#define CLHELP_DEBUG_TEXT(t) t
#else
#define CLHELP_DEBUG_TEXT(t)
#endif

//----------------------------

C_command_line::C_command_line():
   C_command_line_base(3),
#ifdef EDITOR
   debug_game_mode(false),
#endif
   disable_suspend(false),
   run_config(false),
   debug_profile(false),
#ifdef _DEBUG
   debug_use_debug_dlls(true)
#else
   debug_use_debug_dlls(false)
#endif
{
}

//----------------------------

bool C_command_line::ShowHelp(const char **cl){

   OsMessageBox(NULL,
      "General:\n"
                  "  debugdll\tuse debug engine dlls\n"
                  "  mission \"name\"\tset specified mission\n"
      ,
      C_fstr("%s - command-line help", "Insanity Demo help"),
      MBOX_OK);
   return false;
}

//----------------------------
//----------------------------
