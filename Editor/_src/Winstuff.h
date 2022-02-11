#ifndef __WINSTUFF_H_
#define __WINSTUFF_H_

#include <C_str.hpp>
#include <insanity\os.h>

//----------------------------
bool __stdcall WinSendCrashReport(const char*);

//----------------------------
// Let user visually select one item from a list of items. The returned value is
// index of item, or -1 if selection is cancelled.
// The input parameter points to list of strings to choose from.
int WinSelectItem(const char *title, const char *item_list, int curr_sel = -1);

//----------------------------
// Let user re-type name. The provided string on input fills in default name, on output, if successfull,
// contains new name.
bool WinGetName(const char *title, C_str&, void *hwnd = NULL);

//----------------------------
                              //multi-thread safe logging
class C_log_mt_safe{
   void *critical_section;
public:
   FILE *f_log;
   C_str log_name;

   C_log_mt_safe();
   ~C_log_mt_safe();

//----------------------------
// Add string to queue.
   void WriteString(const char*);
};

//----------------------------

struct S_ctrl_help_info{
   word ctrl_id;
   const char *text;
};

//----------------------------
// Display help (hint) for a dialog-box control.
void DisplayControlHelp(void *hwnd, word ctrl_id, const S_ctrl_help_info *hi);

//----------------------------
#endif