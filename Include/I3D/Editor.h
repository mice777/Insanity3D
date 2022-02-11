#ifndef __EDITOR_H
#define __EDITOR_H

#include <IGraph2.h>
#include <i3d\i3d2.h>
#include <c_unknwn.hpp>
#include <smartptr.h>
#include <i3d\I3D_cache.h>
#include <C_vector.h>

#ifdef _MSC_VER
using namespace std;
#endif

/*****************************************************************************************
   Copyright (c) Lonely Cat Games  All rights reserved.

                                    - Insanity editor -  
   Written by: Michal Bacik
   Date: november 1999
   Purpose: Full editing support for Insanity 3D engine. Non-portable, not to be
     included in final release of game.
   Note: All referenced to editor must be removed in release compilation of product.
   
   Interfaces:

   C_editor_item - editor plugin, interface performing simple or complex editing tasks,
      for example "Selection" plugin takes care about keeping, loading, saving, rendering
      current selection of no, one or more 3D objects in 3D scene. Plugins may communicate
      one with another by C_editor_item::Action() method.
      
   C_editor - editor, managing all installed plugins. Communicates with game and plugins,
      registers plugins, manages menus, sends user input to plugins, etc.
*****************************************************************************************/



//----------------------------
                              //editor item (plug-in) base class
class C_editor_item{
   dword ref;
   friend class C_editor_imp;
protected:
   class C_editor *ed;
public:
   C_editor_item(): ref(1){}
   virtual ~C_editor_item(){ ed = NULL; }
   dword AddRef(){ return ++ref; }
   dword Release(){ if(--ref) return ref; Close(); delete this; return 0; }

//----------------------------
// Get name, by which plugin is identified.
   virtual const char *GetName() const = 0;

//----------------------------
// Initialize the plugin. Required by all plugins. Return true if successful, or false to 
// disable plugin installation.
   virtual bool Init() = 0;

//----------------------------
// Close plugin - called before the plugin is uninstalled.
   virtual void Close(){}

//----------------------------
// General method for handling installed menu or keyboard shortcuts installed in editor by this plugin.
   virtual dword Action(int id, void *context = NULL){ return 0; }

//----------------------------
// Update. Called in each frame.
   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt)
   {}
//----------------------------
// Render special items. It is assumed that I3D_driver::BeginScene was already called.
   virtual void Render(){}

//----------------------------
// Load/save state of the plugin. See C_editor::LoadState/SaveState for details.
   virtual bool LoadState(class C_chunk&){ return false; }
   virtual bool SaveState(class C_chunk&) const{ return false; }

//----------------------------
// Function called by Undo plugin, with chunk and saved frame.
   virtual void Undo(dword undo_id, PI3D_frame frm, C_chunk &ck){}

                              //broadcast actions:

//----------------------------
// Called on all plugins after scene has been successfully loaded (resp. before freeing scene).
   virtual void AfterLoad(){}
   virtual void BeforeFree(){}

//----------------------------
// Called when 3D device is being reset.
   virtual void OnDeviceReset(){}

//----------------------------
// Picking broadcast - let plugins override it, they may write to 'frm_picked' and 'pdist' members.
   virtual void OnPick(struct S_MouseEdit_picked*){}

//----------------------------
// Activate/deactivate editor.
   virtual void SetActive(bool active){}

//----------------------------
// Scene validation, plugin returning false marks error and stop further processing.
   virtual bool Validate(){ return true; }

//----------------------------
// Save mission binary data. The data are saved in multiple phases
   virtual void MissionSave(C_chunk &ck, dword phase){}

//----------------------------
// Scene is loaded - found chunk for plugin.
   virtual void LoadFromMission(C_chunk &ck){}

//----------------------------
// Query if scene may be modified.
// If 'quiet' is true, no message box or anything else may be displayed.
   enum E_QUERY_MODIFY{
      QM_OK,
      QM_NO_RDONLY,
      QM_NO_LOCKED,
   };
   virtual E_QUERY_MODIFY QueryModify(bool quiet){ return QM_OK; }

//----------------------------
// Broadcast before frame is being deleted.
   virtual void OnFrameDelete(PI3D_frame){}

//----------------------------
// Broadcast after frame was duplicated.
   virtual void OnFrameDuplicate(PI3D_frame frm_old, PI3D_frame frm_new){}

//----------------------------
// Switch to subobject edit mode.
   virtual bool SubObjectEdit(){ return false; }

//----------------------------
// Cancel sub-object edit mode.
   virtual void SubObjEditCancel(){}
};
typedef C_editor_item *PC_editor_item;
typedef const C_editor_item *CPC_editor_item;

//----------------------------
                              //Following are descriptions of plugins, with enumeration
                              // of their actions, parameters (optional) and return values
                              // of the actions (optional).
                              //Each decription contains short info about plugin's
                              // purpose, dependence list, followed by symbolic names
                              // of the actions.
//----------------------------
//----------------------------
                              //Plugin: "Undo"
                              //Dependence list: no
                              //Purpose: undo/redo functionality - keeping and performing
                              //    reactions on actions made by plugins
                              //Notes: doesn't need any specialized information about any
                              //    particular plugin

class C_editor_item_Undo: public C_editor_item{
public:
//----------------------------
// Begin storing undo information - the returned value is ref to chunk, where data are to be stored.
// The same data chunk is passed to C_editor_item::Undo method of 'ei' interface, when undo is called.
// 'frm' is associated frame (reference is kept by undo plugin). It is passed to C_editor_item::Undo.
// 'frm' may be NULL if not needed, it's just a convenience for association of undo data with most common undo type.
   virtual C_chunk &Begin(PC_editor_item ei, dword undo_id, PI3D_frame frm = NULL) = 0;

//----------------------------
// Finish storing.
   virtual void End() = 0;

//----------------------------
// Store data directly (old way).
   virtual void Save(PC_editor_item ei, dword undo_id, const void *buf, dword buf_size) = 0;

//----------------------------
// Check if top entry is specified plugin's id. Optionally open specified chunk by the entry data, if returned value is true;
   virtual bool IsTopEntry(PC_editor_item, dword undo_id, C_chunk *ck = NULL) const = 0;

//----------------------------
// Clear all undo buffers.
   virtual void Clear() = 0;

//----------------------------
// Remove top-most entry.
   virtual void PopTopEntry() = 0;
};

typedef C_editor_item_Undo *PC_editor_item_Undo;

//----------------------------
//----------------------------
                              //Plugin: "Selection"
                              //Purpose: keeps current selection
                              //Notifications: each time selection changes, notification is sent
                              // to registered plugins, using registered action ID and passing
                              // C_vectorPI3D_frame>* as context parameter
class C_editor_item_Selection: public C_editor_item{
public:
//----------------------------
// Get current selection;
   virtual const C_vector<PI3D_frame> &GetCurSel() const = 0;

//----------------------------
// Get single selected frame, or NULL if not single selection.
   virtual PI3D_frame GetSingleSel() const = 0;

//----------------------------
// Clear selection.
   virtual void Clear() = 0;

//----------------------------
// Invert selection.
   virtual void Invert() = 0;

//----------------------------
// Add frame to selection.
   virtual void AddFrame(PI3D_frame) = 0;

//----------------------------
// Remove frame from selection.
   virtual void RemoveFrame(PI3D_frame) = 0;

//----------------------------
// Flash selected frame (visual effect).
// If time is zero, flashing is performed until not cleared, or mission reloaded.
   virtual void FlashFrame(PI3D_frame frm, dword time = 250, dword color = 0xff00ff00) = 0;

//----------------------------
// Let user select from list of frames. If user presses OK, the returned value is true and list contains selected
// frames upon return.
   virtual bool Prompt(C_vector<PI3D_frame> &lst) = 0;

//----------------------------
// Add plugin to notification list. All registered plugins are notified when selection changes.
   virtual void AddNotify(PC_editor_item, dword message) = 0;

//----------------------------
// Remove plugin from notify list.
   virtual void RemoveNotify(PC_editor_item) = 0;

//----------------------------
// Return index into Status Bar, where info about selection is written (usable as param into C_editor::Message).
   virtual dword GetStatusBarIndex() const = 0;

//----------------------------
// Set function which returns frame's script/actor name.
   typedef const char *(t_GetFrameScript)(PI3D_frame);
   typedef const char *(t_GetFrameActor)(PI3D_frame);
   virtual void SetGetScriptNameFunc(t_GetFrameScript*) = 0;
   virtual void SetGetActorNameFunc(t_GetFrameActor*) = 0;

};

typedef C_editor_item_Selection *PC_editor_item_Selection;

//----------------------------
//----------------------------
                              //Plugin: "Modify"
                              //Purpose: keeps modifications on frames
                              //Notifications: each time modification on frame is set (through AddFrameFlags),
                              // notification is sent to registered plugins, 
                              // using registered action ID and passing
                              // pair<PI3D_frame, dword flags>* as context parameter

enum E_MODIFY_FLAGS{          //modification flags associated with frame:
   E_MODIFY_FLG_POSITION   = 0x1,      //I3D_frame
   E_MODIFY_FLG_ROTATION   = 0x2,      // ''
   E_MODIFY_FLG_SCALE      = 0x4,      // ''
   E_MODIFY_FLG_NU_SCALE   = 0x8,      //I3D_visual
   E_MODIFY_FLG_LINK       = 0x10,     //I3D_frame
   E_MODIFY_FLG_LIGHT      = 0x20,     //I3D_light
   E_MODIFY_FLG_SOUND      = 0x40,     //I3D_sound
   //
   //
   //
   E_MODIFY_FLG_VISUAL     = 0x400,    //I3D_visual
   E_MODIFY_FLG_SECTOR     = 0x800,    //I3D_sector
   E_MODIFY_FLG_COL_MAT    = 0x1000,   //I3D_visual, I3D_volume
   E_MODIFY_FLG_BRIGHTNESS = 0x2000,   //brightness
   E_MODIFY_FLG_VOLUME_TYPE= 0x4000,   //I3D_volume
   E_MODIFY_FLG_FRM_FLAGS  = 0x8000,   //I3D_frame

   E_MODIFY_FLG_HIDE       = 0x100000,  //hidden frame
   E_MODIFY_FLG_CREATE     = 0x200000,  //frame created in editor
   E_MODIFY_FLG_TEMPORARY  = 0x400000,  //temporary frame, modifications not saved on disk
   E_MODIFY_FLG_ALL        = 0x7fffffff,
};

//----------------------------

class C_editor_item_Modify: public C_editor_item{
public:
//----------------------------
// Add flags associated with frame, save original values.
   virtual void AddFrameFlags(PI3D_frame, dword flags, bool save_undo = true) = 0;

//----------------------------
// Remove flags associated with frame.
   virtual void RemoveFlags(PI3D_frame, dword flags) = 0;

//----------------------------
// Remove flags associated with frame, and reset to values before the flags were set.
// Return flags which were reset.
   virtual dword RemoveFlagsAndReset(PI3D_frame, dword flags) = 0;

//----------------------------
// Remove frame from modify list.
   virtual bool RemoveFrame(PI3D_frame) = 0;

//----------------------------
// Get frame's flags.
   virtual dword GetFrameFlags(CPI3D_frame) const = 0;

//----------------------------
// Replace one frame for another (due casting).
   virtual bool ReplaceFrame(PI3D_frame frm_orig, PI3D_frame frm_new) = 0;

//----------------------------
// Reset modifications on selection, bringing up dialog with choices.
   virtual void ResetFlagsOnSelection() = 0;

//----------------------------
// Get name of frame's parent, saved in modify list.
   virtual const char *GetFrameSavedLink(CPI3D_frame) const = 0;

//----------------------------
// Enum modified frames.
   typedef void t_modify_enum(PI3D_frame, dword flags, dword context);
   virtual void EnumModifies(t_modify_enum*, dword context) = 0;

//----------------------------
// Add notification plugin to call when modification is made on frame(s).
   virtual void AddNotify(PC_editor_item, dword message) = 0;

//----------------------------
// Remove plugin from receiving notifications.
   virtual void RemoveNotify(PC_editor_item) = 0;

//----------------------------
// Add lock flags associated with frame.
   virtual void AddLockFlags(PI3D_frame frm, dword flags) = 0;

//----------------------------
// Remove lock flags associated with frame.
   virtual void RemoveLockFlags(PI3D_frame frm, dword flags) = 0;

//----------------------------
// Get lock flags associated with frame.
   virtual dword GetLockFlags(CPI3D_frame frm) const = 0;

//----------------------------
// Clear all lock info.
   virtual void ClearAllLocks() = 0;
};

typedef C_editor_item_Modify *PC_editor_item_Modify;

//----------------------------
//----------------------------
                              //Plugin: "Create"
                              //Purpose: create and destroy various frame types
class C_editor_item_Create: public C_editor_item{
public:
//----------------------------
// Browse for model/sound filename in common directories.
   virtual bool BrowseModel(C_str &filename) = 0;
   virtual bool BrowseSound(C_str &filename) = 0;

//----------------------------
// Delete specified frame from scene.
   virtual bool DeleteFrame(PI3D_frame frm) = 0;
};

typedef C_editor_item_Create *PC_editor_item_Create;

//----------------------------
//----------------------------
                              //Plugin: "MouseEdit"
                              //Dependence list: "Undo", "Selection", "Modify"
                              //Purpose: main edit utility, picking objects, making
                              //    basic 3d operations on selection (position, rotation, scale), 
                              //    linking, camera movement
                              //Notes:
                              // User pick mode: any plugin may set exclusive mode, when picking
                              //    is done for this plugin. When in this mode, cursor is changed
                              //    and any mouse-click evaluates to single frame (or NULL),
                              //    which is sent to registered plugin as:
                              //    pick_plugin->Action(pick_action, (S_MouseEdit_picked*)pick_info);
                              //    When registered plugin returns false on this call,
                              //    it is unregistered from further picking, otherwise it
                              //    remains registered with last parameters.
                              // Override picking: a message EDIT_ACTION_PICK with pointer to S_MouseEdit_picked
                              //    is broadcast to all plugins by "MouseEdit" plugin to let them
                              //    override results of clicking on frame.
                              //    Plugins may write to 'frm_picked', 'pick_fist' and 'pick_action'
                              //    to pick non-visual frames, or override default action
                              //    resulting from picking a frame.

struct S_MouseEdit_picked{    //struct passed to a plugin, when picking occured
   PI3D_frame frm_picked;    //NULL if no frame collided
   int screen_x, screen_y;    //screen pick (mouse) position
   S_vector pick_from, pick_dir; //picking ray, pick_dir is normalized
   float pick_dist;           //distance of collision, valid only if frm_picked is not NULL
   S_vector pick_norm;        //normal of collision, valid only if frm_picked is not NULL
   enum E_PICK_ACTION{        //action being performed (controlled by Ctrl/Shift/Alt held during pick)
      PA_NULL,                // - no action
      PA_DEFAULT,             // - classical pick
      PA_TOGGLE,              // - toggle current selection
      PA_LINK,                // - link picked
      PA_VOLUME,              // - select volume
      PA_EDIT_ONLY,           // - only ediable frames
      PA_NONPICKABLE,         // - default test plus nonpickable visuals
   } pick_action;
};

struct S_MouseEdit_rectangular_pick{
   S_plane vf[5];             //planes of viewing frustum
   S_vector hull_pts[4];      //four points on corners of frustum
   S_vector cpos;             //camera position
   bool full_in;              //flag specifying if object must be fully in (true), or partially in (false), in order to get selected
};

struct S_MouseEdit_user_mode{ //struct used with E_MOUSE_SET_USER_MODE
   class C_editor_item *ei;   //plugin which receives events
   struct S_mode{
      void *hcursor;          //cursor to use with this mode
      int action_id;          //ID to call ei->Action with
   };
   enum E_MODE{               //user action called with params:
      MODE_DEFAULT,           //(void)
      MODE_SELECT,            //(S_MouseEdit_picked*)
      MODE_MOVE_XZ,           //(S_vector *delta) - camera XZ coordinates
      MODE_LINK,              //(S_MouseEdit_picked*)
      MODE_MOVE_Y,            //(S_vector *delta) - screen Y
      MODE_SCALE,             //(float) relative scale
      MODE_ROTATE,            //(S_quat*)
      MODE_PICK_SPEC,         //(S_MouseEdit_picked*)
      MODE_DOUBLECLICK,       //(S_MouseEdit_picked*)
      MODE_DELETE,            //(void)
      MODE_CLEAR_SEL,         //(void)
      MODE_INV_SEL,           //(void)
      MODE_SEL_IN_RECT,       //(S_MouseEdit_rectangular_pick) - relect objects in given region
      MODE_LAST
   };
   S_mode modes[MODE_LAST];

   S_MouseEdit_user_mode():
      ei(NULL)
   {
      memset(modes, 0, sizeof(modes));
   }
};

class C_editor_item_MouseEdit: public C_editor_item{
public:

//----------------------------
// Get selected rotation axis, represented by first 3 bits, one for each axis.
   virtual dword GetRotationAxis() const = 0;

//----------------------------
// Check if 'local system' flag is on.
   virtual bool IsLocSystem() const = 0;

//----------------------------
// Check if 'uniform scale' flag is on.
   virtual bool IsUniformScale() const = 0;

//----------------------------
// Get movement speed.
   virtual float GetMoveSpeed() const = 0;

//----------------------------
// Set user pick mode, or cancel user mode if NULL passed.
// Params:
//    ei ... plugin called on pick
//    action_id ... action sent to plugin
//    hcursor ... handle to windows cursor, or NULL for default
   virtual void SetUserPick(PC_editor_item ei, int action_id, void *hcursor = NULL) = 0;

//----------------------------
// Get currently set user-pick plugin, or NULL if no.
   virtual PC_editor_item GetUserPick() = 0;

//----------------------------
// Set user edit mode, or cancel if NULL passed.
   virtual void SetUserMode(const S_MouseEdit_user_mode*) = 0;

//----------------------------
// Get currently set user edit mode, or NULL if no.
   virtual const S_MouseEdit_user_mode *GetUserMode() = 0;

//----------------------------
   virtual void SetEditMode() = 0;
   virtual void SetViewMode() = 0;

//----------------------------
// Get editing mode [0=edit, 1=view].
   virtual dword GetCurrentMode() const = 0;

//----------------------------
// Get editing camera;
   virtual PI3D_camera GetEditCam() = 0;
};

typedef C_editor_item_MouseEdit *PC_editor_item_MouseEdit;

//----------------------------
//----------------------------
                              //Plugin: "Stats"
                              //Purpose: statistics about various items
class C_editor_item_Stats: public C_editor_item{
public:
//----------------------------
   virtual void RenderStats(float l, float t, float sx, float sy, const char *text, float text_size) const = 0;

//----------------------------
// Register current net interface for access to net statistics.
// Call with NULL for unregister.
   virtual void RegisterNet(class I_net *pnet) = 0;
};

typedef C_editor_item_Stats *PC_editor_item_Stats;

//----------------------------
//----------------------------
                              //Plugin: "Properties"
                              //Run-time use: "MouseEdit", "Create"
                              //Purpose: provide properties window

class C_editor_item_Properties: public C_editor_item{
public:
//----------------------------
// Show the properties window.
   virtual void Show(bool) = 0;

//----------------------------
// Add window to sheet.
   virtual void AddSheet(void *hwnd, bool show = false) = 0;

//----------------------------
// Remove window from sheet.
   virtual bool RemoveSheet(void *hwnd) = 0;

//----------------------------
// Display (activate) given window in the Properties dialog tab.
   virtual void ShowSheet(void *hwnd) = 0;

//----------------------------
// Set focus to Property window.
   virtual void Activate() = 0;

//----------------------------
// Return true if Properties dialog is the active window.
   virtual bool IsActive() const = 0;
};

typedef C_editor_item_Properties *PC_editor_item_Properties;

//----------------------------
//----------------------------
                              //Plugin: "Log"
                              //Purpose: show log
class C_editor_item_Log: public C_editor_item{
public:
//----------------------------
// Add text line to log window.
   virtual void AddText(const char *txt, dword color = 0) = 0;

//----------------------------
// Add rich-text.
   virtual void AddRichText(const char *txt) = 0;

//----------------------------
// Clear the log window.
   virtual void Clear() = 0;

//----------------------------
// Show log sheet.
//   virtual void Show() = 0;

};

typedef C_editor_item_Log *PC_editor_item_Log;

//----------------------------
//----------------------------
                              //Plugin: "Exit"
                              //Purpose: exit from editor
class C_editor_item_Exit: public C_editor_item{
public:
//----------------------------
// Get exit state - check if user wishes to exit.
   virtual bool GetState() const = 0;

//----------------------------
// Set exist state.
   virtual void SetState(bool) = 0;
};

typedef C_editor_item_Exit *PC_editor_item_Exit;

//----------------------------
//----------------------------
                              //Plugin: "DebugLine"
                              //Purpose: draw debugging lines and points of following types:
                              // - one-time
                              // - persistent
                              // - fading

enum E_DEBUGLINE_TYPE{
   DL_PERMANENT,
   DL_ONE_TIME,
   DL_TIMED,
   DL_FLASHING,
};

class C_editor_item_DebugLine: public C_editor_item{
public:
//----------------------------
   virtual void AddLine(const S_vector &v0, const S_vector &v1, E_DEBUGLINE_TYPE type, dword color = 0xffffffff, dword time = 500) = 0;

//----------------------------
   virtual void AddPoint(const S_vector &v, float radius, E_DEBUGLINE_TYPE type, dword color = 0xffffffff, dword time = 500) = 0;

//----------------------------
   virtual void ClearAll() = 0;
};

typedef C_editor_item_DebugLine *PC_editor_item_DebugLine;

//----------------------------
//----------------------------
                              //Plugin: "Help"

class C_editor_item_Help: public C_editor_item{
public:
//----------------------------
// Add help info about plugin.
   virtual void AddHelp(PC_editor_item ei, dword help_id, const char *menu_title, const char *help_string) = 0;

//----------------------------
// Remove help.
   virtual void RemoveHelp(PC_editor_item ei, dword help_id) = 0;
};

typedef C_editor_item_Help *PC_editor_item_Help;

//----------------------------
//----------------------------
                              //Plugin: "BSPTool"

class C_editor_item_BSPTool: public C_editor_item{
public:
//----------------------------
// Build BSP tree on scene.
   virtual bool Create() = 0;
};

typedef C_editor_item_BSPTool *PC_editor_item_BSPTool;

//----------------------------
//----------------------------
                              //Plugin: "Mission"
class C_editor_item_Mission: public C_editor_item{
public:
//----------------------------
// Browse mission filename in common directories.
   virtual bool BrowseMission(void *hwnd, C_str &mission_name) const{ return false; }

//----------------------------
// Save current mission.
   virtual bool Save(bool ask_to_save = true, bool ask_bsp_build = false, const char *override_name = NULL) = 0;

//----------------------------
// Reload currently opened mission.
   virtual bool Reload(bool ask = true) = 0;

//----------------------------
// Get mission name.
   virtual const C_str &GetMissionName() const = 0;

//----------------------------
// Lock/unlock editing.
   virtual void EditLock(bool b = true){}
   virtual bool IsEditLocked() const{ return false; }
};

typedef C_editor_item_Mission *PC_editor_item_Mission;

//----------------------------
//----------------------------
                              //Plugin: "HelpLight"
                              //Purpose: adding helping light(s) into scenes
class C_edit_HelpLight: public C_editor_item{
public:
   virtual void AddHelpLight(bool on) = 0;
   virtual bool IsHelpLightOn() const = 0;
};

typedef C_edit_HelpLight *PC_edit_HelpLight;

//----------------------------
//----------------------------
                              //Plugin: "TimeScale"
                              //Purpose: slowing, speeding or stopping game time
class C_editor_item_TimeScale: public C_editor_item{
public:
//----------------------------
// Get current time scale.
   virtual float GetScale() const = 0;
};

typedef C_editor_item_TimeScale *PC_editor_item_TimeScale;

//----------------------------
//----------------------------

class C_editor_item_Console: public C_editor_item{
public:
//----------------------------
// Output given text into one-time-show console.
   virtual void Debug(const char *s, dword color = 0xc0ffffff) = 0;

//----------------------------
// Output given text into scrolling console.
   virtual void Print(const char *s, dword color = 0xffffffff) = 0;
};

//----------------------------
//----------------------------
                              //Toolbar system:

                              //single toolbar button definition
struct S_toolbar_button{
   dword action_id;           //action ID to call on editor plugin
   int img_index;             //image index, -1 = separator
   const char *tooltip;       //tooltip, or NULL
};

//----------------------------
                              //toolbar class - may be created using IEditor class,
                              // and directly accessed by plugins
class C_toolbar: public C_unknown{
public:
//----------------------------
// Get name of toolbar.
   virtual const C_str &GetName() const = 0;

//----------------------------
// Add buttons to the toolbar.
//    hinstance ... if NULL, bitmap_name is name of BMP file, otherwise it's name of bitmap resource
//    rel_pos ... percentual position from the left side, where these buttons will be added
   virtual bool AddButtons(PC_editor_item owner, const S_toolbar_button *butons, dword num_buttons,
      const char *bitmap_name, void *hinstance = NULL, dword rel_pos = 0) = 0;

//----------------------------
   virtual void SetButtonPressed(PC_editor_item owner, dword action_id, bool pressed) = 0;
   virtual void SetButtonTooltip(PC_editor_item owner, dword action_id, const char *tooltip) = 0;
   virtual void EnableButton(PC_editor_item owner, dword action_id, bool enabled) = 0;

};

typedef C_toolbar *PC_toolbar;
typedef const C_toolbar *CPC_toolbar;

//----------------------------
//----------------------------

enum E_EDIT_MESSAGE{
   EM_MESSAGE,
   EM_WARNING,
   EM_ERROR,
};

enum E_EDIT_ACTION{           //pre-defined actions
   EDIT_ACTION_FEED_MATERIALS=0x40000000,//(pair<int, const char*>[]) - feed material table, terminated by pair<>(-1, NULL)
   EDIT_ACTION_FEED_ENVIRONMENTS,//(pair<int, const char*>[]) - feed sound environment table, terminated by pair<>(-1, NULL)
};

//----------------------------
                              //temporary shortcuts - overriding main shortcuts
struct S_tmp_shortcut{
   int id;                    //action ID being sent to plugin (0 is terminator)
   IG_KEY key;                //shortcut
   byte key_modifier;         //SKEY_? flags
   const char *desc;          //description of shortcut (optional)
};

//----------------------------

class C_editor{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

//----------------------------
// Function for processing input from menu (item selected), or keyboard press.
   virtual void MenuHit(int menu_id, IG_KEY key, byte key_modifier) = 0;

//----------------------------
// Update editor - tick all plugins.
   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt) = 0;

//----------------------------
// Render - call Render method of plugins.
   virtual void Render() = 0;

//----------------------------
// Create place in the status bar area. Using the returned index, caller may display texts in the area using the Message method.
   virtual int CreateStatusIndex(int width) = 0;

//----------------------------
// Send message to user, using the status bar. Usualy the status bar is refreshed in Tick. If you want immediate
// update (e.g. if you display progress indicator of a longer operation), set 'immediate' to true.
   virtual void Message(const char *str, int sb_index = 0, E_EDIT_MESSAGE = EM_MESSAGE, bool immediate = false) = 0;

//----------------------------
// Broadcast acton (old system) - call Action method of plugins with given ID.
   virtual void BroadcastAction(E_EDIT_ACTION, void *context) = 0;

//----------------------------
// Enumerate all installed plugins, using provided callback.
   virtual void EnumPlugins(bool(*)(PC_editor_item, void *context), void *context) = 0;

//----------------------------
// Load/Save editor state into given file. The editor calls LoadState/SaveState of all installed plugins
// for giving them possibility to save their own data.
// The state is saved when used leaves editor, and is reloaded when editor starts.
   virtual bool LoadState(const char *bin_name) = 0;
   virtual bool SaveState(const char *bin_name) const = 0;

//----------------------------
// Broadcast call to virtual function of all plugins.
   virtual void Broadcast(void (C_editor_item::*)()) = 0;
   virtual void Broadcast(void (C_editor_item::*)(void*), void *context) = 0;
   virtual void Broadcast(void (C_editor_item::*)(void*,void*), void *context1, void *context2) = 0;

//----------------------------
// Check/uncheck modify flag.
// The editor keeps flag is anything in the mission was changed, so that some plugin may ask user to save the changes.
   virtual void SetModified(bool = true) = 0;

//----------------------------
// Check if edited scene is modified since last loading.
   virtual bool IsModified() const = 0;

//----------------------------
// Query if modifications can be made. The editor calls QueryModify method on all plugins, and if any
// returns other result than QM_OK, the returned value is false, and no changes on the mission should be made.
// This method is to be called by plugins before they modify scene.
// Note that calling this method may cause some dialogs to be displayed (e.g. Check out file / make writable).
// If 'quite' is true, no such dialogs are allowed.
   virtual bool CanModify(bool quiet = false) = 0;

//----------------------------
// Install/Remove plugin from system.
   virtual bool InstallPlugin(PC_editor_item) = 0;
   virtual bool RemovePlugin(PC_editor_item) = 0;

//----------------------------
// Install/Uninstall single shortcut, either in menu, or keyboard hook, or both.
// Note that the shortcut will not be installed if such keyboard combination is already used.
// If it is used, a warning message is displayed, telling which plugins try to use the same shortcut.
   virtual bool AddShortcut(PC_editor_item ei, int id, const char *menu_item, IG_KEY key = K_NOKEY, byte key_modifier = 0) = 0;
   virtual bool DelShortcut(PC_editor_item ei, int id = 0) = 0;

//----------------------------
// Install/uninstall temporary shortcuts. Temp shortcuts are used by specialized edit modes of some plugins.
// These shortcuts are not checked, so they may override global shortcuts installed by AddShortcut.
// It is strongly recommended that plugin uses the shortcuts only when in special editing mode,
// and removes the shortcuts as soon as the editing mode is finished.
   virtual bool InstallTempShortcuts(PC_editor_item ei, const S_tmp_shortcut*) = 0;
   virtual bool RemoveTempShortcuts(PC_editor_item ei) = 0;

//----------------------------
// Get interface we work with.
   virtual PIGraph GetIGraph() const = 0;
   virtual PI3D_driver GetDriver() const = 0;
   virtual PI3D_scene GetScene() const = 0;

//----------------------------
// Find particular plugin, by its name.
   virtual PC_editor_item FindPlugin(const char *plugin_name) = 0;
//----------------------------
// Functions to be called by plug-ins to make checkmarks on their menu items or enable/disable menu items.
   virtual void CheckMenu(PC_editor_item, int id, bool) = 0;
   virtual void EnableMenu(PC_editor_item, int id, bool) = 0;
   
//----------------------------
// Activate/deactivate editor. The editor should be deactivated when user enters into "game mode", tesing
// the game without use of the editor.
// This method calls SetActive method of all plugins.
   virtual void SetActive(bool) = 0;
   virtual bool IsActive() const = 0;

//----------------------------
// Get cache interfaces, which editor works with.
   virtual C_I3D_cache_base<I3D_model> &GetModelCache() = 0;
   virtual C_I3D_cache_base<I3D_sound> &GetSoundCache() = 0;

//----------------------------
// Get HWND of statusbar.
   virtual void *GetStatusBarHWND() = 0;

//----------------------------
// Draw axes with letters on specified position.
   virtual void DrawAxes(const S_matrix &m, bool sort, S_vector line_scale = S_vector(1, 1, 1),
      float letter_scale = .5f, byte opacity[3] = NULL) = 0;
   virtual class C_poly_text *GetTexts() = 0;

//----------------------------
// Get pointer to C_toolbar class (create if not yet).
// Parameters:
//    suggested_height ... number of buttons to make vertically (hint for 1st time init) (0 = automatic)
   virtual PC_toolbar GetToolbar(const C_str &name, int suggest_x = 0, int suggest_y = 0, dword suggested_height = 0) = 0;

//----------------------------
// Output debug message in various formats.
// PRINT outputs into scrolling console, DEBUG outputs into one-time-show console.
   virtual void PRINT(const char *cp, dword color = 0xffffffff) = 0;
   virtual void PRINT(int) = 0;
   virtual void PRINT(float) = 0;
   virtual void PRINT(const S_vector&) = 0;
   virtual void PRINT(const S_vector2&) = 0;
   virtual void PRINT(const S_quat&) = 0;

   virtual void DEBUG(const char *cp, dword color = 0xc0ffffff) = 0;
   virtual void DEBUG(int) = 0;
   virtual void DEBUG(dword) = 0;
   virtual void DEBUG(float) = 0;
   virtual void DEBUG(const S_vector&) = 0;
   virtual void DEBUG(const S_vector2&) = 0;
   virtual void DEBUG(const S_quat&) = 0;

//----------------------------
// Save mission data to binary chunk. This method is the main function saving the mission binary file.
// It is assumed that the chunk class is already initialized, and some sub-chunk is already created.
// This method simply calls MissionSave method of all plugins in several phases.
   virtual void MissionSave(C_chunk &ck) = 0;

//----------------------------
// Set customized resource caches used by the editor.
   virtual void SetModelCache(C_I3D_cache_base<I3D_model> *mc) = 0;
   virtual void SetSoundCache(C_I3D_cache_base<I3D_sound> *sc) = 0;
};

typedef C_editor *PC_editor;
typedef const C_editor *CPC_editor;

//----------------------------
// Editor creation.
// Note on plugins initialization:
// - plugins are initialized by C_editor::InstallPlugin
// - plugin DLLs are searched in ".\Plugins\*.dll" path, in each found DLL
//    an initialization function is searched, which is declared as:
//    extern "C" void Initialize(LPC_editor)
//----------------------------
PC_editor CreateEditor(PI3D_driver, PI3D_scene);

//----------------------------

#endif
