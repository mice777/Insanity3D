#include <rules.h>
#include <cmdline.h>

//----------------------------


class C_command_line: public C_command_line_base{
   bool ShowHelp(const char **cl);
   virtual const S_scan_data *GetScanData() const;
public:
#ifdef EDITOR
   bool debug_game_mode;
#endif
   C_str cmdline_mission;
   bool debug_use_debug_dlls;
   bool run_config;
   bool debug_profile;
   bool disable_suspend;

   C_command_line();
};

//----------------------------
