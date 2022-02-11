#include "all.h"
#include "..\..\Source\IEditor\resource.h"
#include "loader.h"
#include "winstuff.h"
#include "gamemission.h"
#include "script.h"
#include <windows.h>
#include <commctrl.h>
#include <editctrl.h>
#include <insanity\sourcecontrol.h>
#include <Insanity\3DTexts.h>

//----------------------------

const char SCRIPT_DIR[] = "Missions";


#ifdef EDITOR                 //script editor plugin - only in edit mode

extern const char SCRIPT_CMDLINE[];

//----------------------------
                              //names of callback functions possible in scripts
                              // (for highlight in editor)
static const char *fnc_callback_names[] = {
   "Main", "Exit", "AI",
   "GameBegin", "GameEnd", "CinematicEnd", "TestCinematic",
   "OnHit", "OnCollision", "OnSignal", "OnUse", "OnDestroy",
   "OnNote", "CanBeUsed",
   "OnWatchCome", "OnWatchLeave", "OnListenCome",
   NULL
};

//----------------------------
                              //colors of script names
static const dword script_name_colors[] = {
   0x0000c8c8,                //own
   0x00009800,                //shared
};

//----------------------------
//----------------------------
                              //Plugin: "Script"
                              //Dependence list: "Properties", "Selection", "Undo"
                              //Purpose: manage and edit scripts
//----------------------------

static const struct{
   const char *title;
   int image;                 //one-based, 0 = no image
   dword width;
} scrsel_list[] = {
   {"Name", 0, 140},
   {"#", 0, 24},
};

//----------------------------
                              
#define WM_USER_INIT_SHEET (WM_USER+100)

                              //editor plugin: Script
class C_edit_Script: public C_editor_item{
   virtual const char *GetName() const{ return "Script"; }

   C_smart_ptr<C_edit_control> ec_edit, ec_errors;

   HWND ec_hwnd, ec_err_hwnd; //NULL if no file is open
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Properties> e_props;

   C_smart_ptr<C_game_mission> mission;

   void *h_font;              //keep font for drawing script names in 3D

   bool draw_script_names;

                              //position and size of edit control
   int ec_pos_size[4];
   int ec_err_height;

   C_str last_search_string;
   bool search_whole;
   bool search_case;

                              //search struct - must be in class scope
   struct S_file_info{
      C_str fname;
      dword indx;
      dword line, row;
      inline bool operator <(const S_file_info &fi) const{
         return (strcmp(fname, fi.fname) < 0);
      }
   };

//----------------------------

   virtual void OnFrameDelete(PI3D_frame frm){
      DeleteScriptFromFrame(frm);
   }

//----------------------------

   virtual void OnFrameDuplicate(PI3D_frame frm_old, PI3D_frame frm_new){

      S_frame_info *fi_old = GetFrameInfo(frm_old);
      if(fi_old && fi_old->vm){
         AssignScriptToFrame(frm_new, fi_old->vm->GetName(), fi_old->vm->GetTable());
      }
   }

//----------------------------

   virtual void AfterLoad(){

      HighlightButtons();

      //assert(!scripted_frames.size());
                        //collect all frames having script
      struct S_hlp{
         C_vector<C_smart_ptr<I3D_frame> > *scripted_frames;
         C_vector<C_smart_ptr<S_script_info> > *scr_list;

         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
            S_frame_info *fi = GetFrameInfo(frm);
            if(fi && fi->vm){
               S_hlp *hp = (S_hlp*)c;
               for(int i=hp->scripted_frames->size(); i--; )
                  if(frm==(*hp->scripted_frames)[i])
                     break;
               if(i==-1){
                  hp->scripted_frames->push_back(frm);
               }
               const C_str &name = fi->vm->GetName();
               for(i=hp->scr_list->size(); i--; ){
                  if((*hp->scr_list)[i]->full_name.Matchi(name)){
                     ++(*hp->scr_list)[i]->use_count;
                     break;
                  }
               }
               if(i==-1){
                  hp->scr_list->push_back(new C_edit_Script::S_script_info);
                  C_edit_Script::S_script_info *si = hp->scr_list->back();
                  ++si->use_count;
                  si->SetName(name);
                  si->Release();
               }
            }
            return I3DENUMRET_OK;
         }
      } hlp = {&scripted_frames, &scr_list};
      S_hlp::cbEnum(ed->GetScene()->GetPrimarySector(), (dword)&hlp);
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
      redraw_sheet = true;

      if(last_edit_script.Size()){
         EditScript(last_edit_script, false);
         last_edit_script = NULL;
      }
   }

//----------------------------

   virtual void BeforeFree(){

      if(hwnd_tab_edit)
         DestroyWindow(hwnd_tab_edit);

      last_edit_script = NULL;
      if(ec_hwnd){
         last_edit_script = (const char*)ec_edit->GetUserData();
         ec_edit->Save();
         DestroyWindow(ec_hwnd);
      }
      SendDlgItemMessage(hwnd_list, IDC_LIST, LVM_DELETEALLITEMS, 0, 0);
      //scr_list.clear();
      //scripted_frames.clear();
      struct S_hlp{
         C_vector<C_smart_ptr<I3D_frame> > *scripted_frames;
         C_vector<C_smart_ptr<S_script_info> > *scr_list;
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
            S_frame_info *fi = GetFrameInfo(frm);
            if(fi && fi->vm){
               S_hlp *hp = (S_hlp*)c;
                        //remove from scripted frames
               for(int i=hp->scripted_frames->size(); i--; )
               if(frm==(*hp->scripted_frames)[i]){
                  (*hp->scripted_frames)[i] = hp->scripted_frames->back(); hp->scripted_frames->pop_back();
                  break;
               }
                        //remove from script list
               const C_str &name = fi->vm->GetName();
               for(i=hp->scr_list->size(); i--; ){
                  if((*hp->scr_list)[i]->full_name.Matchi(name)){
                     if(!--(*hp->scr_list)[i]->use_count){
                        (*hp->scr_list)[i] = hp->scr_list->back();
                        hp->scr_list->pop_back();
                     }
                     break;
                  }
               }
            }
            return I3DENUMRET_OK;
         }
      } hlp = {&scripted_frames, &scr_list};
      S_hlp::cbEnum(ed->GetScene()->GetPrimarySector(), (dword)&hlp);
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
      scripted_frames.clear();
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==2){
                           //save scripts of all frames in scene
         struct S_hlp{
            C_chunk *ck;
            PC_game_mission gm;
            static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
               S_frame_info *fi = GetFrameInfo(frm);
               if(fi && fi->vm){
                  S_hlp *hp = (S_hlp*)c;
                  C_chunk &ck = *hp->ck;
                  ck <<= CT_SCRIPT;
                  ck.WStringChunk(CT_NAME, frm->GetName());
                  const C_str &mis_name = hp->gm->GetName();
                  C_str scr_name = fi->vm->GetName();
                  if(scr_name.Matchi(C_fstr("%s\\*", (const char*)mis_name))){
                     scr_name = C_fstr("*%s", &scr_name[mis_name.Size()+1]);
                  }
                  ck.WStringChunk(CT_SCRIPT_NAME, scr_name);
                           //save frame's table (if script has template and table is present)
                  PC_table tab = fi->vm->GetTable();
                  if(tab){
                     ck <<= CT_SCRIPT_TABLE;
                     tab->Save(ck.GetHandle(), TABOPEN_FILEHANDLE);
                     --ck;
                  }
                  --ck;
               }
               return I3DENUMRET_OK;
            }
         } hlp;
         hlp.ck = &ck;
         hlp.gm = mission;
         S_hlp::cbEnum(ed->GetScene()->GetPrimarySector(), (dword)&hlp);
         mission->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
      }
   }

//----------------------------

   virtual void OnDeviceReset(){
                          //destroy all current name textures
      for(int i=scr_list.size(); i--; )
         scr_list[i]->tp_name = NULL;
   }

//----------------------------
public:
                              //unique scripts loaded with current scene
   struct S_script_info: public C_unknown{
      C_str friendly_name;
      C_str full_name;        //script's full name (relative to script dir, without extension, lowercase)
      int use_count;
      int status;             //0=unknown, 1=compile ok, 2=compile fail
      bool writtable;
                              //name of script stored in texture - for editing purposes
      C_smart_ptr<I3D_texture> tp_name;
                              //size of initialized part of texture
      int tp_size[2];

      S_script_info():
         use_count(0),
         writtable(false),
         status(0)
      {}
      S_script_info(const S_script_info &si);
      S_script_info &operator =(const S_script_info &si);
      void SetName(const C_str &name){

         C_str str = name;
                              //capitalize 1st letter
         bool cap = true;
         for(dword i=0; i<str.Size(); i++){
            char &c = str[i];
            bool al = isalpha(c);
            if(!al){
               cap = true;
            }else
            if(cap){
               c = (char)toupper(c);
               cap = false;
            }else
               c = (char)tolower(c);
         }
         full_name = str;
         full_name.ToLower();
         writtable = !OsIsFileReadOnly(C_fstr("%s\\%s.scr", SCRIPT_DIR, (const char*)full_name));
                              //no path in friendly name
         for(dword ci=str.Size(); ci--; ){
            if(str[ci]=='\\')
               break;
         }
         ++ci;
         friendly_name = &str[ci];
      }
   };
private:
                              //list of loaded scripts - unsorted
   C_vector<C_smart_ptr<S_script_info> > scr_list;

   C_str selected_script;     //full-name of selected script (lower case)

   C_str last_edit_script;

                              //frames in current scenes containing scripts
                              // (due to names rendering)
   C_vector<C_smart_ptr<I3D_frame> > scripted_frames;

//----------------------------
   C_smart_ptr<C_table> config_tab;
   enum{
      TAB_F_CFG_MIN_DIST,
      TAB_F_CFG_MAX_DIST,
      TAB_I_CFG_FONT_SIZE,
      TAB_I_CFG_OPACITY,
   };
   
   static CPC_table_template GetTemplate(){

      static const C_table_element te[] = {
         {TE_BRANCH, 0, "Distances", 2, 0, 0, "Setup of distances used for rendering of script names above scripted frames."},
            {TE_FLOAT, TAB_F_CFG_MIN_DIST, "Fade dist", 0, 0, 10},
            {TE_FLOAT, TAB_F_CFG_MAX_DIST, "Max dist", 0, 0, 30},
         {TE_INT, TAB_I_CFG_FONT_SIZE, "Font height", 0, 0, 12, "Font height used for displaying script name (in pixels)."},
         {TE_INT, TAB_I_CFG_OPACITY, "Opacity", 0, 100, 60, "Opacity for displaying script name (in percent)."},
         {TE_NULL}
      };
      static const C_table_template templ_config = { "Script plugin configuration", te};
      return &templ_config;
   }

//----------------------------

   void DrawScriptName(PI3D_frame frm, const C_str &name){
      
                              //find (or create) texture associated with script
      for(int i=scr_list.size(); i--; )
         if(scr_list[i]->full_name==name)
            break;
      assert(i!=-1);
      if(i==-1)
         return;
      const C_str &friendly_name = scr_list[i]->friendly_name;
      PI3D_texture tp = scr_list[i]->tp_name;
      if(!tp){
         if(!h_font)
            CreateFont();
         scr_list[i]->tp_name = tp =
            CreateStringTexture(friendly_name, scr_list[i]->tp_size, ed->GetDriver(), h_font);
         if(tp)
            tp->Release();
      }
      if(tp){
         DrawStringTexture(frm, 0.0f, ed->GetScene(), tp, scr_list[i]->tp_size,
            //config_tab->ItemI(TAB_I_CFG_FONT_SIZE) + 2,
            (FloatToInt((float)config_tab->ItemI(TAB_I_CFG_OPACITY) * .01f * 255.0f)<<24) | script_name_colors[0],
            config_tab->ItemF(TAB_F_CFG_MIN_DIST),
            config_tab->ItemF(TAB_F_CFG_MAX_DIST),
            2.0f, 0.0f);
      }
   }

//----------------------------

   bool GetCreateFileName(C_str &str, const char *title){

      char buf[MAX_PATH];
      OPENFILENAME on;
      memset(&on, 0, sizeof(on));
      on.lStructSize = sizeof(on);
      on.hwndOwner = (HWND)ed->GetIGraph()->GetHWND();
      on.lpstrFilter = "Script files (*.scr)\0*.scr\0""All files\0*.*\0";
      on.nFilterIndex = 1;
      on.lpstrFile = buf;
      buf[0] = 0;
      on.nMaxFile = sizeof(buf);
      on.lpstrInitialDir = str;
      on.lpstrTitle = title;
      on.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR |
         OFN_CREATEPROMPT;
         //OFN_NOTESTFILECREATE;
      on.lpstrDefExt = "scr";
      bool b = (GetOpenFileName(&on)!=0);
      if(b){
                              //create non-existing files
         FILE *fp = fopen(buf, "rb");
         if(!fp)
            fp = fopen(buf, "ab");
         fclose(fp);

                              //strip current dir
         char cwd[MAX_PATH];
         getcwd(cwd, sizeof(cwd));
                              //append script dir
         C_fstr dir("%s\\%s\\", cwd, SCRIPT_DIR);
         int i = 0;
         if(!strnicmp(dir, buf, dir.Size())) i = dir.Size();
         str = &buf[i];
                              //no extension in names
         for(dword ci=str.Size(); ci--; ){
            if(str[ci]=='.'){
               str[ci] = 0;
               str = (const char*)str;
               break;
            }
         }

      }
      return b;
   }

//----------------------------
   
   void DeleteSelectedScript(){

      if(!selected_script.Size())
         return;
      if(!ed->CanModify())
         return;
                              //find in our list
      for(int i=scr_list.size(); i--; )
         if(scr_list[i]->full_name==selected_script)
            break;
      assert(i!=-1);
      int count = scr_list[i]->use_count;
      if(count>=1){
                              //phase 1 - remove from all used
         int id = MessageBox(hwnd_list, 
            C_fstr("Remove script '%s' from all frames?", (const char*)selected_script),
            "Delete script", MB_YESNOCANCEL);
         if(id==IDYES){
                              //delete from all frames
            struct S_hlp{
               C_edit_Script *ei;
               C_str name;
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                  S_frame_info *fi = GetFrameInfo(frm);
                  S_hlp *hp = (S_hlp*)c;
                  if(fi && fi->vm && hp->name.Matchi(fi->vm->GetName())){
                     hp->ei->DeleteScriptFromFrame(frm);
                  }
                  return I3DENUMRET_OK;
               }
            } hlp;
            hlp.ei = this;
            hlp.name = selected_script;
            S_hlp::cbEnum(mission->GetScene()->GetPrimarySector(), (dword)&hlp);
            mission->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
            ed->SetModified();
         }
      }else{
                              //phase 2 - remove from sheet
                              // not used anywhere, so just remove our reference
         C_str curr_name = scr_list[i]->friendly_name;
         scr_list[i] = scr_list.back(); scr_list.pop_back();

                              //select next adjacent script
         int next_i = -1;
         for(i=scr_list.size(); i--; ){
            const C_str &name = scr_list[i]->friendly_name;
            if(curr_name<name){
               if(next_i==-1)
                  next_i = i;
               else
               if(name<scr_list[next_i]->friendly_name)
                  next_i = i;
            }
         }
         if(next_i==-1){
            for(i=scr_list.size(); i--; ){
               const C_str &name = scr_list[i]->friendly_name;
               if(name<curr_name){
                  if(next_i==-1)
                     next_i = i;
                  else
                  if(scr_list[next_i]->friendly_name<name)
                     next_i = i;
               }
            }
         }
         selected_script = NULL;
         if(next_i!=-1)
            selected_script = scr_list[next_i]->full_name;

         redraw_sheet = true;
      }
   }

//----------------------------

   void CreateErrLog(C_cache *ck, const char *title){

      ec_errors = NULL;
      if(!ec_edit)
         return;
      ec_errors = CreateEditControl(); ec_errors->Release();
      ec_errors->Init("editor\\err_ctrl.mac", "editor\\err_ctrl.cfg");
      ec_errors->SetCallback(ECErrCallBack_thunk, this);
      int pos_size[4];
                              //place error window below edit window
      {
         RECT rc;
         GetWindowRect(ec_hwnd, &rc);
         pos_size[0] = rc.left;
         pos_size[1] = rc.bottom;
         pos_size[2] = rc.right - rc.left;
         pos_size[3] = ec_err_height;
      }

      ECTRL_RESULT er;
      er = ec_errors->Open(ck, title,
         //hwnd_list,
         ec_hwnd,
         ECTRL_OPEN_READONLY,
         (void**)&ec_err_hwnd, pos_size);
      assert(ECTRL_SUCCESS(er));
      //ed->GetIGraph()->AddDlgHWND(ec_err_hwnd);
   }

//----------------------------

   bool CloseEditedScript(){

      if(ec_edit){
         ECTRL_RESULT er = ec_edit->Save(true);
         if(ECTRL_FAIL(er))
            return false;
         ECCallBack("close");
         assert(!ec_edit);
      }
      return true;
   }

//----------------------------
// Edit script with specified name. The name is given without extension,
// and is relative to script root dir SCRIPT_DIR.
   void EditScript(const C_str &name, bool activate = true){

      ECTRL_RESULT er;
      if(!CloseEditedScript())
         return;
      ec_edit = CreateEditControl(); ec_edit->Release();
      er = ec_edit->Init("editor\\editctrl.mac", "editor\\editctrl.cfg");
      if(er!=ECTRL_OK){
         const char *msg = "unknown error";
         switch(er){
         case ECTRLERR_MACROOPENFAIL: msg = "cannot open macro file 'editor\\editctrl.mac'"; break;
         case ECTRLERR_CONFIGOPENFAIL: msg = "cannot open CONFIG file 'editor\\editctrl.cfg'"; break;
            break;
         }
         ErrReport(msg, editor);
         return;
      }
                              //add function names for highlighting
      const VM_LOAD_SYMBOL *syms = script_man->GetLinkSymbols();
      while(syms->address){
         if(syms->name)
            ec_edit->AddHighlightWord(1, (syms++)->name);
         else
            syms = (VM_LOAD_SYMBOL*)syms->address;
      }
      for(int i=0; fnc_callback_names[i]; i++)
         ec_edit->AddHighlightWord(3, fnc_callback_names[i]);


      ec_edit->SetCallback(ECCallBack_thunk, this);
      C_fstr scr_name("%s\\%s.scr", SCRIPT_DIR, (const char*)name);
      dword open_flags = 0;
      if(OsIsFileReadOnly(scr_name))
         open_flags = ECTRL_OPEN_READONLY;
      if(!activate)
         open_flags |= ECTRL_OPEN_HIDDEN;
      HWND hwnd_active = GetActiveWindow();
      er = ec_edit->Open(scr_name, 
         //(HWND)ed->GetIGraph()->GetHWND(),
         NULL,
         open_flags, (void**)&ec_hwnd, ec_pos_size);
      if(ECTRL_FAIL(er)){
         MessageBox(hwnd_list, C_fstr("Failed to open file %s for editing", (const char*)scr_name),
            "Edit script", MB_OK);
         ec_edit = NULL;
         ec_hwnd = NULL;
         return;
      }
      char *buf = new char[name.Size()+1];
      strcpy(buf, name);
      ec_edit->SetUserData((dword)buf);
      ed->GetIGraph()->AddDlgHWND(ec_hwnd);

      if(!activate){
         SetActiveWindow(hwnd_active);
         SetWindowPos(ec_hwnd, (HWND)ed->GetIGraph()->GetHWND(), 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
      }
   }

   /*
//----------------------------
// CHeck if specified file is newer than given filename.
   bool IsFileNewerThan(int handle, const char *fname) const{

      bool newer = false;
      int h_src = dtaOpen(fname);
      if(h_src!=-1){
         dword src_time[2], dst_time[2];
         if(dtaGetTime(h_src, src_time) && dtaGetTime(handle, dst_time)){
            newer = (CompareFileTime((FILETIME*)src_time, (FILETIME*)dst_time) < 0);
         }
         dtaClose(h_src);
      }
      return newer;
   }

//----------------------------
// Check if script is up-to-date.
   bool IsUpToDate(const C_str &name) const{

      bool up_to_date = false;
      int h_dst = dtaOpen(C_fstr("%s\\%s.dll", SCRIPT_DIR, (const char*)name));
      if(h_dst!=-1){
         up_to_date = IsFileNewerThan(h_dst, C_fstr("%s\\%s.scr", SCRIPT_DIR, (const char*)name));
         for(int i=0; up_to_date; i++){
            const char *dep_file = dependency_files[i];
            if(!dep_file)
               break;
            up_to_date = IsFileNewerThan(h_dst, C_fstr("%s\\%s", SCRIPT_DIR, dep_file));
         }
         dtaClose(h_dst);
      }
      return up_to_date;
   }
   */

//----------------------------
// Compile script. If 'check_date' is true, date of script source and compiled destination is checked
// to see if compiling is necessary.
   bool CompileScript(const C_str &name, bool display_errors){

      /*
      if(check_date){
         if(IsUpToDate(name)){
            ed->Message("Script is up to date.");
            return true;
         }
      }
      */
                              //destroy edited table's window
      if(hwnd_tab_edit)
         DestroyWindow(hwnd_tab_edit);

      struct S_hlp{
         static void ISLAPI CompileErr(const char *msg, void *context, int l, int r, bool warn){
            C_vector<char> &errors = *(C_vector<char>*)context;
            errors.insert(errors.end(), msg, msg+strlen(msg));
            errors.push_back('\n');
         }

         PC_script scr;
         C_vector<PI3D_frame> *scr_frms;
         static I3DENUMRET I3DAPI cbCollect(PI3D_frame frm, dword c){
            S_frame_info *fi = GetFrameInfo(frm);
            S_hlp *hp = (S_hlp*)c;
            if(fi && fi->vm){
               if(!hp->scr || fi->vm->GetScript()==hp->scr)
                  hp->scr_frms->push_back(frm);
            }
            return I3DENUMRET_OK;
         }
      };
      C_vector<char> errors;
                              //find the script
      C_script_manager::t_script_map::iterator it = script_man->script_map.find(name);
      if(it==script_man->script_map.end()){
         ed->Message("Cannot compile script - not assigned");
         return false;
      }
      C_script_manager::S_script_source &ss = (*it).second;
      PC_script scr = ss.scr;

                              //collect all frames containing the script
      C_vector<PI3D_frame> scr_frms;
      scr_frms.reserve(ss.use_count);
      {
         S_hlp hlp;
         hlp.scr_frms = &scr_frms;
         hlp.scr = scr;
         PI3D_scene scene = mission->GetScene();
         S_hlp::cbCollect(scene->GetPrimarySector(), (dword)&hlp);
         scene->EnumFrames(S_hlp::cbCollect, (dword)&hlp);
      }
      dword i;

                              //call Exit on all scripted frms
      for(i=scr_frms.size(); i--; ){
         PI3D_frame frm = scr_frms[i];
         script_man->ClearAllThreads(frm);
         script_man->RunFunction(frm, "Exit", mission);
      }

      C_fstr filename("%s\\%s.scr", SCRIPT_DIR, (const char*)name);
      ss.load_result = scr->Compile(filename,
         ISLCOMP_PRECOMPILE | ISLCOMP_PRECOMPILE_HIDDEN,
         //0,
         SCRIPT_CMDLINE,
         S_hlp::CompileErr, &errors);

      int num_re = 0;
                              //reload all the frames
      for(i=scr_frms.size(); i--; ){
         PI3D_frame frm = scr_frms[i];
         S_frame_info *fi = GetFrameInfo(frm);
         PC_v_machine vm = fi->vm;
         C_smart_ptr<C_table> old_tab = vm->GetTable();
         vm->Unload();
         if(ISL_SUCCESS(ss.load_result)){
            ISL_RESULT ir = vm->Load(scr, script_man->GetLinkSymbols());
            if(ISL_FAIL(ir)){
               S_hlp::CompileErr(C_fstr("Script %s: link fail\n", (const char*)name), &errors, 0, 0, false);
            }else{
               script_man->RunFunction(frm, "Main", mission);
            }
            e_slct->FlashFrame(frm);
            num_re++;
            if(old_tab){
               PC_table new_tab = vm->GetTable();
               if(new_tab)
                  new_tab->Load((PC_table)old_tab, TABOPEN_DUPLICATE | TABOPEN_UPDATE);
            }
         }
      }
                              //close current errors
      if(ec_err_hwnd){
         DestroyWindow(ec_err_hwnd);
         assert(!ec_err_hwnd);
      }
                              //report errors
      if(errors.size()){
         errors.push_back(0);
         ed->Message("Compile errors encountered.", 0, EM_ERROR, true);
         if(display_errors){
            C_cache ck;
            const char *beg = &(*errors.begin());
            dword sz = errors.size();
            if(ck.open((byte**)&beg, &sz, CACHE_READ_MEM)){
               CreateErrLog(&ck, C_fstr("Compile errors: %s", (const char*)name));
            }
         }
         return false;
      }else{
         ed->Message("Compilation succeeded.");
      }

      ed->GetIGraph()->GetTimer(1, 1);
      return true;
   }

//----------------------------

   void DumpScript(const C_str &name){

                              //find the script
      C_script_manager::t_script_map::iterator it = script_man->script_map.find(name);
      if(it==script_man->script_map.end()){
         ed->Message("Cannot dump script");
         return;
      }
      C_script_manager::S_script_source &ss = (*it).second;
      PC_script scr = ss.scr;

      struct S_hlp{
         static void ISLAPI Func(const char *msg, void *context){
            LOG(msg);
         }
      };
      scr->Dump(S_hlp::Func, NULL);
   }

//----------------------------

   void GoToError(int line){

      assert(ec_errors);
      int len = 0;
      ec_errors->GetTextLine(line, NULL, &len);
      char *buf = new char[len];
      if(ec_errors->GetTextLine(line, &buf, &len)){
                           //scan - find (%i): error sequence
         for(int i=0; i<len; i++){
            if(buf[i]=='('){
               buf[i] = 0;
               int lnum, rnum, sz;
               if(sscanf(&buf[i+1], "%i, %i):%n", &lnum, &rnum, &sz)==2){
                  i += sz;
                  assert(ec_edit);
                  ec_edit->SetCurrPos(lnum-1, rnum-1);
                  SetActiveWindow(ec_hwnd);
               }
               break;
            }
         }
         ed->Message(buf);
      }
      delete[] buf;
   }

//----------------------------

   static bool ECErrCallBack_thunk(const char *msg, void *context){
      C_edit_Script *ei = (C_edit_Script*)context;
      return ei->ECErrCallBack(msg);
   }

//----------------------------

   bool ECErrCallBack(const char *msg){

      assert(ec_errors);
      if(!strcmp(msg, "close")){
         //ei->ed->GetIGraph()->RemoveDlgHWND(ei->ec_err_hwnd);
         int ps[4];
         ec_errors->GetPosSize(ps);
         ec_err_height = ps[3];

         ec_err_hwnd = NULL;
         ec_errors->SetCallback(NULL, 0);
         ec_errors->Close(true);
         ec_errors = NULL;
      }else
      if(!strcmp(msg, "errortag")){
         int l, r;
         ec_errors->GetCurrPos(l, r);
         GoToError(l);
      }else
      if(!strcmp(msg, "edit_window")){
         SetActiveWindow(ec_hwnd);
      }else
      {
         //assert(0);
      }
      return true;
   }

//----------------------------

   static bool ECCallBack_thunk(const char *msg, void *context){
      C_edit_Script *ei = (C_edit_Script*)context;
      return ei->ECCallBack(msg);
   }

//----------------------------
// Display help from include file.
   void DisplayHelp1(const char *wrd){

      bool ok = false;
      PC_dta_stream h = DtaCreateStream("Missions\\common.i");
      if(h){
         int sz = h->GetSize();
         C_vector<char> buf(sz+2);
         h->Read(&(*buf.begin()), sz);
         h->Release();

         for(const char *cp=&(*buf.begin()); *cp; ){
            const char *eol = strchr(cp, '\n');
            if(!eol) eol = cp + strlen(cp);

            if(*cp=='/' && *++cp=='/' && *++cp=='~'){
               ++cp;
               int len = eol - cp;
               if(len && cp[len-1]=='\r') --len;
               if(!strncmp(wrd, cp, len)){
                              //read next lines until empty line
                  cp = eol + 1;
                  if(!*cp)
                     break;
                  C_vector<char> buf;
                  while(true){
                     const char *eol = strchr(cp, '\n');
                     if(!eol) eol = cp + strlen(cp);
                     if(*cp=='\n' || *cp==0 || *cp=='\r')
                        break;
                     if(!(cp[0]=='/' && cp[1]=='/')){
                              //insert definition itself at beginning
                        dword num = 0;
                        while(true){
                           buf.insert(buf.begin() + num, cp, eol+1);
                           num += eol+1-cp;
                           cp = eol + 1;
                              //read next line until blank
                           eol = strchr(cp, '\n');
                           if(!eol) eol = cp + strlen(cp);
                           if(*cp=='\n' || *cp==0 || *cp=='\r')
                              break;
                        }
                        break;
                     }
                     cp += 2;
                     buf.insert(buf.end(), cp, eol+1);
                     cp = eol + 1;
                  }
                  buf.push_back(0);
                  dword sz = buf.size();
                  const char *beg = &(*buf.begin());
                  C_cache ck;
                  if(ck.open((byte**)&beg, &sz, CACHE_READ_MEM)){
                     CreateErrLog(&ck, C_fstr("Help: %s", wrd));
                  }
                  ok = true;
                  break;
               }
            }
            cp = eol + 1;
         }
      }
      if(!ok){
         if(ec_errors)
            ec_errors->Close(true);
         ed->Message(C_fstr("No help about '%s' is available.", wrd));
      }
   }

//----------------------------
// Display help from help file.
   void DisplayHelp(const char *wrd){

      bool ok = false;
      PC_dta_stream h = DtaCreateStream("Editor\\Editor.hlp");
      if(h){
         int sz = h->GetSize();
         C_vector<char> buf(sz+2);
         h->Read(&(*buf.begin()), sz);
         h->Release();

         bool phase = 0;
         for(const char *cp=&(*buf.begin()); *cp; ){
            const char *eol = strchr(cp, '\n');
            if(!eol) eol = cp + strlen(cp);

            switch(*cp){
            case '\n':
            case '\r':
               phase = 0;
               break;
            case ';':
            case '/':
               break;
            default:
               if(!phase){
                  int len = eol-cp;
                  if(len && cp[len-1]=='\r') --len;
                  if(strncmp(wrd, cp, len)){
                     ++phase;
                     break;
                  }
                              //read next lines until empty line
                  cp = eol + 1;
                  if(!*cp || *cp=='\r'){
                     cp = NULL;
                     break;
                  }
                  const char *beg = cp, *end = NULL;
                  while(!end){
                     const char *eol = strchr(cp, '\n');
                     if(!eol) eol = cp + strlen(cp);

                     switch(*cp){
                     case '\n':
                     case '\r':
                     case 0:
                        end = cp - 1;
                        break;
                     default:
                        cp = eol + 1;
                     }
                  }
                  C_cache ck;
                  dword sz = end-beg;
                  if(ck.open((byte**)&beg, &sz, CACHE_READ_MEM)){
                     CreateErrLog(&ck, C_fstr("Help: %s", wrd));
                  }
                  ok = true;
                  cp = NULL;
               }
            }
            if(!cp)
               break;
            cp = eol + 1;
         }
      }
      if(!ok){
         DisplayHelp1(wrd);
         /*
         if(ec_errors)
            ec_errors->Close(true);
         ed->Message(C_fstr("No help about '%s' is available.", wrd));
         */
      }
   }

//----------------------------

   bool ECCallBack(const char *msg){

      assert(ec_edit);
      if(!strcmp(msg, "compile")){
         const char *name = (const char*)ec_edit->GetUserData();
         assert(name);
         CompileScript(name, true);
                              //re-init sheet
         SendMessage(hwnd_list, WM_USER_INIT_SHEET, 0, 0);
      }else
      if(!strcmp(msg, "close")){
         ec_edit->GetPosSize(ec_pos_size);
         SetFocus((HWND)igraph->GetHWND());
         ed->GetIGraph()->RemoveDlgHWND(ec_hwnd);
         ec_hwnd = NULL;
         ec_edit->SetCallback(NULL, 0);
         char *name = (char*)ec_edit->GetUserData();
         assert(name);
         delete[] name;
         ec_edit->SetUserData(0);

         ec_edit->Close(true);
         ec_edit = NULL;

         if(ec_errors)
            ec_errors->Close(true);
      }else
      if(!strcmp(msg, "escape")){
         if(ec_hwnd)
            SetFocus((HWND)igraph->GetHWND());
      }else
      if(!strcmp(msg, "prev_err")){
         if(ec_errors){
            int l, r;
            ec_errors->GetCurrPos(l, r);
            if(--l < 0)
               l = ec_errors->GetNumLines() - 1;
            ec_errors->SetCurrPos(l, r);
            GoToError(l);
         }
      }else
      if(!strcmp(msg, "next_err")){
         if(ec_errors){
            int l, r;
            ec_errors->GetCurrPos(l, r);
            if(++l >= ec_errors->GetNumLines())
               l = 0;
            ec_errors->SetCurrPos(l, r);
            GoToError(l);
         }
      }else
      if(!strcmp(msg, "errors")){
         if(ec_err_hwnd)
            SetActiveWindow(ec_err_hwnd);
      }else
      if(!strcmp(msg, "help")){
         int l, r;
         ec_edit->GetCurrPos(l, r);
         int len = 0;
         ec_edit->GetTextLine(l, NULL, &len);
         char *buf = new char[len];
         if(ec_edit->GetTextLine(l, &buf, &len) && isalpha(buf[r])){
                              //get word under cursor
            while(r && isalpha(buf[r-1]))
               --r;
            char wrd[256];
            if(sscanf(buf+r, "%256s", wrd)==1){
               int l = strlen(wrd);
               for(int i=0; i<l && isalpha(wrd[i]); i++);
               if(i){
                  wrd[i] = 0;
                  DisplayHelp(wrd);
                  SetActiveWindow(ec_hwnd);
               }
            }
         }
         delete[] buf;
      }else
      if(!strcmp(msg, "save_failed")){
         OsMessageBox(ed->GetIGraph()->GetHWND(), C_fstr("Failed to save script. Please make sure the file may be modified."),
            "Saving file...", MBOX_OK);
      }else{
         //assert(0);
      }
      return true;
   }

//----------------------------

   static BOOL CALLBACK dlgSheet_thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      C_edit_Script *ei = (C_edit_Script*)GetWindowLong(hwnd, GWL_USERDATA);
      if(!ei){
         if(uMsg!=WM_INITDIALOG)
            return 0;
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         ei = (C_edit_Script*)lParam;
         assert(ei);
      }
      return ei->dlgSheet(hwnd, uMsg, wParam, lParam);
   }

//----------------------------

   bool dlgSheet(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){

      case WM_INITDIALOG:
         {
                              //make columns
            for(int i=0; i < (sizeof(scrsel_list)/sizeof(scrsel_list[0])); i++){
               LVCOLUMN lvc;
               memset(&lvc, 0, sizeof(lvc));
               lvc.mask = LVCF_WIDTH;
               lvc.cx = scrsel_list[i].width;
               if(scrsel_list[i].title){
                  lvc.pszText = (char*)scrsel_list[i].title;
                  lvc.cchTextMax = strlen(scrsel_list[i].title);
                  lvc.mask |= LVCF_TEXT;
               }
               SendDlgItemMessage(hwnd, IDC_LIST, LVM_INSERTCOLUMN, i , (dword)&lvc);
            }
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETEXTENDEDLISTVIEWSTYLE, 
               LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES, 
               LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
         }
         return 1;

      case WM_USER_INIT_SHEET:
         {
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_DELETEALLITEMS, 0, 0);
                              //feed in scripts
            for(int i=scr_list.size(); i--; ){
               const S_script_info &si = *scr_list[i];

               static LVITEM lvi = { LVIF_PARAM | LVIF_STATE | LVIF_IMAGE};
               lvi.iItem = 0;
               lvi.lParam = (LPARAM)(S_script_info*)&si;
               lvi.state = 0;
               lvi.iImage = si.status;
               if(si.full_name==selected_script)
                  lvi.state |= LVIS_SELECTED | LVIS_FOCUSED;
               SendDlgItemMessage((HWND)hwnd, IDC_LIST, LVM_INSERTITEM, 0, (LPARAM)&lvi);

               {
                  static LVITEM lvi = { LVIF_TEXT, 0, 0};
                  lvi.iItem = 0;
                  C_str str;
                  const char *sname = si.friendly_name;
                  if(!si.writtable){
                     str = C_fstr("%s [read-only]", sname);
                     sname = str;
                  }
                  lvi.pszText = (char*)sname;
                  lvi.cchTextMax = strlen(lvi.pszText);
                  SendDlgItemMessage((HWND)hwnd, IDC_LIST, LVM_SETITEM, 0, (LPARAM)&lvi);
               }
               {
                  static LVITEM lvi = { LVIF_TEXT, 0, 1};
                  lvi.iItem = 0;
                  C_fstr str("%i", si.use_count);
                  lvi.pszText = (char*)(const char*)str;
                  lvi.cchTextMax = str.Size();
                  SendDlgItemMessage((HWND)hwnd, IDC_LIST, LVM_SETITEM, 0, (LPARAM)&lvi);
               }
            }
                              //keep them sorted
            struct S_hlp{
               static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort){
                  const C_edit_Script::S_script_info *si1 = (C_edit_Script::S_script_info*)lParam1;
                  const C_edit_Script::S_script_info *si2 = (C_edit_Script::S_script_info*)lParam2;
                  return strcmp(si1->friendly_name, si2->friendly_name);
               }
            };
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SORTITEMS, NULL, (LPARAM)S_hlp::CompareFunc);
            /*
            if(selected_script.Size()){
               for(i=SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0); i--; ){
                  SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETSELECTIONMARK, 0, i);
                  break;
               }
            }
            */
         }
         break;

      case WM_HELP:
         {
            HELPINFO *hi = (HELPINFO*)lParam;
            static S_ctrl_help_info help_texts[] = {
               IDC_BUTTON_SCRIPT_OPEN, "Open new script into the script sheet. This will either open existing file from disk, or allow user to create a new file.",
               IDC_BUTTON_SCRIPT_EDIT, "Open text exitor for currently selected script.",
               IDC_BUTTON_SCRIPT_DEL,  "Delete currently selected script from the sheet.",
               IDC_BUTTON_SCRIPT_CHECK_OUT,  "Check out selected script from SourceSafe database.",
               IDC_BUTTON_SCRIPT_CHECK_IN,  "Check in selected script into SourceSafe database.",
               IDC_BUTTON_SCRIPT_SEARCH,  "Search string in all scripts.",
               IDC_BUTTON_SCRIPT_COMP,  "Compile selected script.",
               IDC_BUTTON_SCRIPT_ASSIGN,  "Assign selected script to all selected frames.",
               IDC_BUTTON_SCRIPT_REMOVE,  "Remove script(s) from all selected frames.",
               IDC_BUTTON_SCRIPT_SELECT,  "Select all frames which have assigned selected script.",
               IDC_BUTTON_SCRIPT_DUMP,  "Dump script code into Log window.",
               IDC_BUTTON_SCRIPT_CLOSE,  "Close script editor.",
               IDC_LIST,               "List of scripts loaded in the script editor and used by frames in the scene.\nThe left part shows name of script, and right column shows how many frames in scene have assigned particular script.",
               0              //terminator
            };
            DisplayControlHelp(hwnd, (word)hi->iCtrlId, help_texts);
         }
         break;

      case WM_NOTIFY:
         {
            LPNMHDR pnmh = (LPNMHDR)lParam; 
            switch(pnmh->code){
            case LVN_KEYDOWN:
               {
                  LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN)lParam; 
                  switch(pnkd->wVKey){
                  case VK_DELETE:
                     if(!ed->CanModify()) break;
                     DeleteSelectedScript();
                     break;
                  }
               }
               break;

            case NM_DBLCLK :
               {
                  if(selected_script.Size())
                     EditScript(selected_script);
               }
               break;
            case LVN_ITEMCHANGED:
               {
                  NMLISTVIEW *lv = (NMLISTVIEW*)lParam;
                  if(lv->uChanged&LVIF_STATE){
                     S_script_info *si = (S_script_info*)lv->lParam;
                     assert(si);
                     if(lv->uNewState&LVIS_SELECTED){
                        selected_script = si->full_name;
                     }else
                     if(selected_script==si->full_name){
                        selected_script = NULL;
                     }
                  }
               }
               break;
            }
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDC_BUTTON_SCRIPT_CLOSE:
            {
               e_props->RemoveSheet(hwnd);
               in_prop = false;
            }
            break;

         case IDC_BUTTON_SCRIPT_OPEN:
            {
               C_fstr str("%s\\%s", SCRIPT_DIR, (const char*)mission->GetName());
               str.ToLower();
               if(GetCreateFileName(str, "Open script")){
                              //check if such script is already open
                  for(int i=scr_list.size(); i--; ){
                     if(scr_list[i]->full_name==str)
                        break;
                  }
                  if(i==-1){
                     i = scr_list.size();
                     scr_list.push_back(new S_script_info);
                     S_script_info *si = scr_list.back();
                     si->SetName(str);
                     si->Release();
                  }
                  selected_script = scr_list[i]->full_name;
                  redraw_sheet = true;
               }
            }
            break;

         case IDC_BUTTON_SCRIPT_CHECK_OUT:
         case IDC_BUTTON_SCRIPT_CHECK_IN:
            {
               bool out = (LOWORD(wParam)==IDC_BUTTON_SCRIPT_CHECK_OUT);
               if(!selected_script.Size()){
                  ed->Message("Please select script to make selection by.");
                  break;
               }
               for(int i=scr_list.size(); i--; ){
                  S_script_info &si = *scr_list[i];
                  if(si.full_name==selected_script){
                     if(si.writtable==out){
                        ed->Message(out ?
                           "This script is writtable, can't check out." :
                           "This script is read-only, can't check in."
                           );
                     }else{
                        C_fstr fname("%s\\%s.scr", SCRIPT_DIR, (const char*)si.full_name);
                        bool ok;
                        if(out)
                           ok = SCCheckOutFile("Insanity3d\\Editor", fname);
                        else
                           ok = SCCheckInFile("Insanity3d\\Editor", fname, false);
                        if(!ok){
                           ed->Message(C_fstr("Failed to %s: '%s'", out ? "check out" : "check in", (const char*)fname));
                           break;
                        }else{
                           si.writtable = !OsIsFileReadOnly(fname);
                           SendMessage(hwnd, WM_USER_INIT_SHEET, 0, 0);
                        }
                     }
                     break;
                  }
               }
               assert(i!=-1);
            }
            break;

         case IDC_BUTTON_SCRIPT_SEARCH:
            {
                              //get search word
               struct S_hlp{
                  C_str str;
                  bool whole;
                  bool case_sens;

                  static BOOL CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

                     switch(uMsg){
                     case WM_INITDIALOG:
                        {
                           OsCenterWindow(hwnd, (HWND)igraph->GetHWND());
                           SetWindowLong(hwnd, GWL_USERDATA, lParam);

                           const S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);

                           SetDlgItemText(hwnd, IDC_EDIT1, hp->str);
                           CheckDlgButton(hwnd, IDC_SEARCH_WHOLE, hp->whole ? BST_CHECKED : BST_UNCHECKED);
                           CheckDlgButton(hwnd, IDC_SEARCH_CASE, hp->case_sens ? BST_CHECKED : BST_UNCHECKED);
                        }
                        return 1;

                     case WM_COMMAND:
                        switch(LOWORD(wParam)){
                        case IDOK:
                           {
                              S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                              char buf[256];
                              if(GetDlgItemText(hwnd, IDC_EDIT1, buf, sizeof(buf))){
                                 hp->str = buf;
                                 hp->whole = IsDlgButtonChecked(hwnd, IDC_SEARCH_WHOLE);
                                 hp->case_sens = IsDlgButtonChecked(hwnd, IDC_SEARCH_CASE);
                                 EndDialog(hwnd, 1);
                              }
                           }
                           break;
                        case IDCANCEL:
                           EndDialog(hwnd, 0);
                           break;
                        }
                        break;
                     }
                     return 0;
                  }
               } hlp;
               hlp.str = last_search_string;
               hlp.whole = search_whole;
               hlp.case_sens = search_case;

               if(ec_edit){
                              //save changes
                  ECTRL_RESULT er = ec_edit->Save(true);
                  if(ECTRL_FAIL(er))
                     break;
               }

               int ret = DialogBoxParam(GetModuleHandle(NULL), "IDD_SEARCH", (HWND)igraph->GetHWND(),
                  S_hlp::DialogProc, (LPARAM)&hlp);
               if(ret && hlp.str.Size()){
                              //we got string and params, search now
                  last_search_string = hlp.str;
                  search_whole = hlp.whole;
                  search_case = hlp.case_sens;

                  C_vector<S_file_info> files;

                  for(dword i=scr_list.size(); i--; ){
                     C_cache ck;
                     C_fstr filename("%s\\%s.scr", SCRIPT_DIR, (const char*)scr_list[i]->full_name);
                     if(ck.open(filename, CACHE_READ)){
                        dword line_count = 0;
                        bool found = false;
                        for(; !ck.eof() && !found; ++line_count){
                           char line[512];
                           ck.getline(line, sizeof(line));
                              //search the line
                           dword llen = strlen(line);
                           int max_indx = llen - last_search_string.Size();
                           if(max_indx < 0)
                              continue;
                           for(dword ci=0; ci<=(dword)max_indx; ci++){
                              if(!(search_case ? strncmp : strnicmp)(&line[ci], last_search_string, last_search_string.Size())){
                                                      //word found
                                 if(search_whole){
                                    if(ci && isalnum(line[ci-1]))
                                       continue;
                                    if(ci+last_search_string.Size()<llen-1 && isalnum(line[ci+last_search_string.Size()]))
                                       continue;
                                 }
                                 files.push_back(S_file_info());
                                 S_file_info &fi = files.back();
                                 fi.fname = scr_list[i]->friendly_name;
                                 fi.line = line_count;
                                 fi.row = ci;
                                 fi.indx = i;
                                 found = true;
                              }
                           }
                        }
                     }
                  }
                  if(files.size()){
                              //sort found files
                     sort(files.begin(), files.end());
                              //present user with choice
                     C_vector<char> selection;
                     int n = -1;
                     for(dword i=0; i<files.size(); i++){
                        const S_file_info &fi = files[i];
                        const C_str &s = fi.fname;
                        selection.insert(selection.end(), (const char *)s, (const char*)s+s.Size()+1);
                        if(selected_script.Size() && scr_list[fi.indx]->full_name.Matchi(selected_script))
                           n = i;
                     }
                     selection.push_back(0);
                     n = WinSelectItem("Select script", &(*selection.begin()), n);
                     if(n!=-1){
                        const S_file_info &fi = files[n];
                        selected_script = scr_list[fi.indx]->full_name;
                        EditScript(selected_script);
                        if(ec_edit)
                           ec_edit->SetCurrPos(fi.line, fi.row);

                              //select script in the list
                        {
                           HWND hwnd_lv = GetDlgItem(hwnd_list, IDC_LIST);
                           assert(hwnd_lv);
                           LVITEM lvi;
                           memset(&lvi, 0, sizeof(lvi));
                                       //unselect all first
                           lvi.state = 0;
                           lvi.stateMask = LVIS_SELECTED;
                           SendMessage(hwnd_lv, LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&lvi);
                                       //find the edited script
                           for(dword i=SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0); i--; ){
                              lvi.iItem = i;
                              lvi.mask = LVIF_PARAM | LVIF_STATE;
                              SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM)&lvi);
                              S_script_info *si = (S_script_info*)lvi.lParam;
                              if(si->full_name==selected_script){
                                       //found, make it selected
                                 lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
                                 lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
                                 SendMessage(hwnd_lv, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
                                 break;
                              }
                           }

                        }
                     }
                  }else{
                     MessageBox(hwnd,
                        C_fstr("String '%s' was not found in any script.", (const char*)last_search_string), "Search in scripts", MB_OK);
                  }
               }
            }
            break;

         case IDC_BUTTON_SCRIPT_DEL:
            DeleteSelectedScript();
            break;

         case IDC_BUTTON_SCRIPT_ASSIGN:
         case IDC_BUTTON_SCRIPT_REMOVE:
            {
               if(!ed->CanModify()) break;
               C_str name;
               if(LOWORD(wParam)==IDC_BUTTON_SCRIPT_ASSIGN){
                  name = selected_script;
                  if(!name.Size()){
                     ed->Message("Please select script to assign");
                     break;
                  }
               }
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               int num = 0;
               for(int i=sel_list.size(); i--; ){
                  PI3D_frame frm = sel_list[i];
                  S_frame_info *fi = GetFrameInfo(frm);

                  C_v_machine *vm_old = fi ? fi->vm : NULL;
                  C_smart_ptr<C_table> tab_old = vm_old ? vm_old->GetTable() : NULL;

                  if(fi && fi->vm){
                     if(name.Matchi(fi->vm->GetScript()->GetName()))
                        continue;
                           //ask for replacement
                     int i;
                     if(!name.Size()){
                        i = MessageBox(hwnd,
                           C_fstr("Frame '%s': remove '%s'?", (const char*)frm->GetName(), (const char*)fi->vm->GetName()),
                           "Remove script", MB_YESNOCANCEL);
                     }else{
                        i = MessageBox(hwnd,
                           C_fstr("Frame '%s': replace '%s' by '%s'?", (const char*)frm->GetName(),
                           (const char*)fi->vm->GetName(), (const char*)name),
                           "Replace script", MB_YESNOCANCEL);
                     }
                     if(i==IDCANCEL)
                        break;
                     if(i!=IDYES)
                        continue;
                  }else
                  if(!name.Size())
                     continue;

                  DeleteScriptFromFrame(frm);
                  if(name.Size())
                     AssignScriptToFrame(frm, name, tab_old);
                  ++num;
               }
               redraw_sheet = true;
               ed->SetModified();
               if(LOWORD(wParam)==IDC_BUTTON_SCRIPT_ASSIGN){
                  ed->Message(C_fstr("Script '%s' assigned to %i frame(s)", 
                     (const char*)name, num));
               }else{
                  ed->Message(C_fstr("Script removed from %i frame(s)", num));
               }
            }
            break;

         case IDC_BUTTON_SCRIPT_SELECT:
            {
               if(!selected_script.Size()){
                  ed->Message("Please select script to make selection by.");
                  break;
               }
                              //clear current selection
               e_slct->Clear();

                              //select all frames using this script
               struct S_hlp{
                  C_edit_Script *ei;
                  C_str name;
                  PC_editor_item_Selection e_slct;
                  int num;
                  static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                     S_frame_info *fi = GetFrameInfo(frm);
                     S_hlp *hp = (S_hlp*)c;
                     if(fi && fi->vm && hp->name.Matchi(fi->vm->GetName())){
                        hp->e_slct->AddFrame(frm);
                        ++hp->num;
                     }
                     return I3DENUMRET_OK;
                  }
               } hlp;
               hlp.ei = this;
               hlp.name = selected_script;
               hlp.e_slct = e_slct;
               hlp.num = 0;
               S_hlp::cbEnum(mission->GetScene()->GetPrimarySector(), (dword)&hlp);
               mission->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
               ed->Message(C_fstr("%i frame(s) selected.", hlp.num));
            }
            break;

         case IDC_BUTTON_SCRIPT_EDIT:
            {
               if(selected_script.Size())
                  EditScript(selected_script);
            }
            break;

         case IDC_BUTTON_SCRIPT_COMP:
            {
               if(!selected_script.Size()){
                  ed->Message("Please select script to compile.");
                  break;
               }
               CompileScript(selected_script, false);
                              //re-init sheet
               //SendMessage(hwnd_list, WM_USER_INIT_SHEET, 0, 0);
            }
            break;

         case IDC_BUTTON_SCRIPT_DUMP:
            {
               if(!selected_script.Size()){
                  ed->Message("Please select script to compile.");
                  break;
               }
               DumpScript(selected_script);
            }
            break;

         case IDCANCEL:
            SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
            break;
         }
         break;
      }
      return 0;
   }

//----------------------------

   void HighlightButtons(){

      if(!hwnd_list || !in_prop)
         return;

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      EnableWindow(GetDlgItem(hwnd_list, IDC_BUTTON_SCRIPT_ASSIGN), (sel_list.size()!=0));
      EnableWindow(GetDlgItem(hwnd_list, IDC_BUTTON_SCRIPT_REMOVE), (sel_list.size()!=0));
   }

//----------------------------

   HWND hwnd_list;            //handle of sheet containing list of scripts
   bool in_prop;

   void CloseWindow(){
      if(hwnd_list){
         if(in_prop){
            e_props->RemoveSheet(hwnd_list);
            in_prop = false;
         }
         DestroyWindow(hwnd_list);
         hwnd_list = NULL;
      }
   }

//----------------------------

   void DestroyFont(){
      if(h_font) {
         OsDestroyFont(h_font);
         h_font = NULL;
      }
   }

//----------------------------

   void CreateFont(){
      DestroyFont();
                              //create and init font
      h_font = OsCreateFont(config_tab->ItemI(TAB_I_CFG_FONT_SIZE));
                              //destroy all current name textures
      for(int i=scr_list.size(); i--; )
         scr_list[i]->tp_name = NULL;
   }

//----------------------------

   const C_table_template *edited_template;
   C_vector<C_smart_ptr<I3D_frame> > multiple_edited_frames;
   HWND hwnd_tab_edit;
   bool reset_tab_editor;

   static void TABAPI cbTable(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){

      switch(msg){

      case TCM_CLOSE:
         {
            C_edit_Script *es = (C_edit_Script*)cb_user;
            es->edited_template = NULL;
            //es->ed->GetIGraph()->RemoveDlgHWND(es->hwnd_tab_edit);
            es->e_props->RemoveSheet(es->hwnd_tab_edit);
            es->hwnd_tab_edit = NULL;
            es->ed->GetIGraph()->EnableSuspend(true);
            es->multiple_edited_frames.clear();
         }
         break;

      case TCM_IMPORT:
         prm2 = 0xffffffff;
                              //flow...
      case TCM_MODIFY:
         {
            C_edit_Script *es = (C_edit_Script*)cb_user;
            es->ed->SetModified();
                              //update tables on frame(s)
            for(int ii = es->multiple_edited_frames.size(); ii--; ){
               PI3D_frame frm = es->multiple_edited_frames[ii];

               PS_frame_info fi = GetFrameInfo(frm);
               assert(fi && fi->vm && fi->vm->GetTable());
               PC_table dst = fi->vm->GetTable();

               if(prm2==-1){
                  for(int i=tab->NumItems(); i--; ){
                     memcpy(&dst->ItemI(i), &tab->ItemI(i), tab->SizeOf(i));
                  }
               }else{
                  bool is_array = (tab->ArrayLen(prm2));
                  switch(tab->GetItemType(prm2)){
                  case TE_STRING:
                     if(is_array)
                        strcpy(dst->ItemS(prm2, prm3), tab->GetItemS(prm2, prm3));
                     else
                        strcpy(dst->ItemS(prm2), tab->GetItemS(prm2));
                     break;
                  case TE_ENUM:
                     if(is_array)
                        dst->ItemE(prm2, prm3) = tab->GetItemE(prm2, prm3);
                     else
                        dst->ItemE(prm2) = tab->GetItemE(prm2);
                     break;
                  case TE_BOOL:
                     if(is_array)
                        dst->ItemB(prm2, prm3) = tab->GetItemB(prm2, prm3);
                     else
                        dst->ItemB(prm2) = tab->GetItemB(prm2);
                     break;
                  case TE_COLOR_VECTOR:
                     if(is_array)
                        dst->ItemV(prm2, prm3) = tab->GetItemV(prm2, prm3);
                     else
                        dst->ItemV(prm2) = tab->GetItemV(prm2);
                     break;
                  case TE_FLOAT:
                     if(is_array)
                        dst->ItemF(prm2, prm3) = tab->GetItemF(prm2, prm3);
                     else
                        dst->ItemF(prm2) = tab->GetItemF(prm2);
                     break;
                  case TE_INT:
                     if(is_array)
                        dst->ItemI(prm2, prm3) = tab->GetItemI(prm2, prm3);
                     else
                        dst->ItemI(prm2) = tab->GetItemI(prm2);
                     break;
                  default:
                     assert(0);
                  }
               }
            }
         }
         break;
      }
   }

   bool redraw_sheet;         //true to redraw sheet in next Tick

//----------------------------
public:

   void AssignScriptToFrame(PI3D_frame frm, const char *name, PC_table copy_table = NULL){

                              //save undo info
      e_undo->Save(this, E_SCRIPT_REMOVE_NAMED, (void*)(const char*)frm->GetName(), frm->GetName().Size()+1);

      script_man->LoadScript(frm, name, mission, true);
      S_frame_info *fi = GetFrameInfo(frm);
      if(copy_table && fi->vm->GetTable()){
         fi->vm->GetTable()->Load(copy_table, TABOPEN_DUPLICATE | TABOPEN_UPDATE);
      }
                              //inc use counter
      for(int ii = scr_list.size(); ii--; ){
         if(scr_list[ii]->full_name==name){
            ++scr_list[ii]->use_count;
            break;
         }
      }

                              //add to list of scripted frames
      for(int i=scripted_frames.size(); i--; )
         if(frm==scripted_frames[i])
            break;
      assert(i==-1);
      scripted_frames.push_back(frm);
      redraw_sheet = true;
   }

//----------------------------

   void DeleteScriptFromFrame(PI3D_frame frm){

                              //delete frame's script
      S_frame_info *fi = GetFrameInfo(frm);
      if(fi && fi->vm){
                              //save table contens from script
         PC_table tab = fi->vm->GetTable();
         if(tab){
            C_cache tab_cache;
            byte *pTabData = NULL;
            dword TabDataLen = 0;

            bool b = tab_cache.open(&pTabData, &TabDataLen, CACHE_WRITE_MEM);
            assert(b);

            b = tab->Save(&tab_cache, TABOPEN_FILEHANDLE);
            assert(b);
            tab_cache.close();

                        //save framename and tabdata as undo
            dword new_data_len = frm->GetName().Size()+1+TabDataLen+sizeof(size_t);
            char *pData = new char[new_data_len];

            assert(pData);

            strcpy(pData, frm->GetName());
            memcpy(pData+frm->GetName().Size()+1, &TabDataLen, sizeof(size_t));
            memcpy(pData+frm->GetName().Size()+1+sizeof(size_t), pTabData, TabDataLen);

            e_undo->Save(this, E_SCRIPT_SET_TABLE, pData, new_data_len);
            delete [] pData;
            tab_cache.FreeMem(pTabData);
         }

         const C_str &name = fi->vm->GetName();
                        //save undo info
         {
            int len = name.Size() + 1 + frm->GetName().Size() + 1;
            char *cp = new char[len];
            strcpy(cp, frm->GetName());
            strcpy(cp+frm->GetName().Size()+1, name);
            e_undo->Save(this, E_SCRIPT_SET_NAMED, cp, len);
            delete[] cp;
         }
                        //dec use counter
         for(int i=scr_list.size(); i--; ){
            if(scr_list[i]->full_name.Matchi(name)){
               --scr_list[i]->use_count;
               break;
            }
         }
         script_man->FreeScript(frm, mission, true);

                              //delete frame from list
         for(i=scripted_frames.size(); i--; )
            if(frm==scripted_frames[i])
               break;
         assert(i!=-1);
         scripted_frames[i] = scripted_frames.back(); scripted_frames.pop_back();

         redraw_sheet = true;
      }
   }

public:
   C_edit_Script(C_game_mission &m1):
      mission(&m1),
      hwnd_list(NULL),
      in_prop(false),
      edited_template(NULL),
      hwnd_tab_edit(NULL),
      reset_tab_editor(false),
      h_font(NULL),
      draw_script_names(true),
      search_whole(false),
      search_case(false),
      ec_hwnd(NULL), ec_err_hwnd(NULL)
   {
      ec_pos_size[0] = ec_pos_size[1] = 10;
      ec_pos_size[2] = ec_pos_size[3] = 300;
      ec_err_height = 150;
      config_tab = CreateTable();
      config_tab->Load(GetTemplate(), TABOPEN_TEMPLATE);
      config_tab->Release();
   }
   ~C_edit_Script(){
      DestroyFont();
   }

   virtual bool Init(){

      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      if(!e_props || !e_slct || !e_undo){
         e_props = NULL;
         e_slct = NULL;
         e_undo = NULL;
         return false;
      }
                              //be sensible on selection change
      e_slct->AddNotify(this, E_SCRIPT_SLCT_NOTIFY);

#define MB "%i %78 Sc&ript\\"
      ed->AddShortcut(this, E_SCRIPT_LIST, MB"&List\tCtrl+S", K_S, SKEY_CTRL);
      ed->AddShortcut(this, E_SCRIPT_EDIT_SELECTION, MB"&Edit script (&assign)\tE", K_E, 0);
      ed->AddShortcut(this, E_SCRIPT_ASSIGN_EDITTAB, MB"Edit table\tT", K_T, 0);
      ed->AddShortcut(this, E_SCRIPT_TOGGLE_NAMES, MB"Show &names\tCtrl+Alt+Shift+S", K_S, SKEY_SHIFT | SKEY_ALT | SKEY_CTRL);
      ed->AddShortcut(this, E_SCRIPT_CONFIG, MB"%i C&onfigure", K_NOKEY, 0);
      ed->AddShortcut(this, E_SCRIPT_EDIT_MASTER, MB"Edit &Master script", K_NOKEY, 0);

      hwnd_list = CreateDialogParam(GetModuleHandle(NULL), "IDD_SCRIPT_EDIT",
         (HWND)ed->GetIGraph()->GetHWND(),
         dlgSheet_thunk, (LPARAM)this);
      
      ed->CheckMenu(this, E_SCRIPT_TOGGLE_NAMES, true);

      return true;
   }

   virtual void Close(){

      if(hwnd_tab_edit)
         DestroyWindow(hwnd_tab_edit);
      if(ec_hwnd)
         DestroyWindow(ec_hwnd);

      if(ec_edit){
         ec_edit->Close(true);
         assert(!ec_edit);
      }
      if(ec_errors){
         ec_errors->Close(true);
         assert(!ec_errors);
      }
      CloseWindow();
      e_props = NULL;
      e_slct = NULL;
      e_undo = NULL;
   }

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(redraw_sheet && hwnd_list){
         redraw_sheet = false;
         SendMessage(hwnd_list, WM_USER_INIT_SHEET, 0, 0);
      }
      if(reset_tab_editor){
         if(hwnd_tab_edit)
            DestroyWindow(hwnd_tab_edit);
         reset_tab_editor = false;
      }
   }

   virtual dword Action(int id, void *context){

      switch(id){
      case E_SCRIPT_LIST:
         {
            if(in_prop){
               e_props->RemoveSheet(hwnd_list);
               in_prop = false;
               break;
            }
                              //add our sheet into property window
            in_prop = true;
            e_props->AddSheet(hwnd_list, true);
            HighlightButtons();
         }
         break;

      case E_SCRIPT_EDIT_SELECTION:
         {
                              //check if we currently edit some script
            if(ec_hwnd){
               SetActiveWindow(ec_hwnd);
               break;
            }
                              //get single selection
            PI3D_frame frm = e_slct->GetSingleSel();
            if(!frm){
               ed->Message("Single selection required");
               break;
            }
            S_frame_info *fi = GetFrameInfo(frm);
            if(fi && fi->vm){
               //ed->Message("Selected frame has no script");
               ed->Message(C_fstr("Editing script: %s", fi->vm->GetName()));
               EditScript(fi->vm->GetName());
               break;
            }
                              //select and assign script
            {
               if(!ed->CanModify()) break;

               C_str str;
               str = C_fstr("%s\\%s", SCRIPT_DIR, (const char*)mission->GetName());
               str.ToLower();
               if(GetCreateFileName(str, "Assign script to selected frames")){
                              //check if such script is already open
                  for(int i=scr_list.size(); i--; ){
                     if(scr_list[i]->full_name.Matchi(str))
                        break;
                  }
                  if(i==-1){
                     i = scr_list.size();
                     scr_list.push_back(new S_script_info);
                     S_script_info *si = scr_list.back();
                     si->SetName(str);
                     si->Release();
                  }
                  selected_script = scr_list[i]->full_name;
                  if(!in_prop)
                     Action(E_SCRIPT_LIST, 0);

                  const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                  for(i=sel_list.size(); i--; ){
                     AssignScriptToFrame(sel_list[i], selected_script);
                  }
                  redraw_sheet = true;
               }
               ed->SetModified(true);
            }
         }
         break;

      case E_SCRIPT_EDIT_MASTER:
         {
            if(ec_hwnd)
               DestroyWindow(ec_hwnd);
            EditScript("master");
         }
         break;

      case E_SCRIPT_ASSIGN_EDITTAB:
         {
            if(!ed->CanModify())
               break;
            if(hwnd_tab_edit){
                              //set focus to currently edited table
               SetFocus(hwnd_tab_edit);
               SetActiveWindow(hwnd_tab_edit);
               break;
            }
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            if(!sel_list.size()){
               ed->Message("Selection required");
               break;
            }

            bool any_script = false;
            const C_table_template *tt = NULL;
            CPC_script base_scr = NULL;
            C_smart_ptr<C_table> conjuct_tab;

            for(dword i=0; i<sel_list.size(); i++){
               PS_frame_info fi = GetFrameInfo(sel_list[i]);
               if(fi && fi->vm){
                  any_script = true;

                  PC_table frm_tab = fi->vm->GetTable();
                  if(!frm_tab)
                     continue;

                  if(!i){
                     base_scr = fi->vm->GetScript();
                     tt = base_scr->GetTableTemplate();
                     if(tt){
                        conjuct_tab = CreateTable();
                        conjuct_tab->Load(frm_tab, TABOPEN_DUPLICATE | TABOPEN_UPDATE);
                        conjuct_tab->Release();
                     }
                  }else{
                     if(conjuct_tab && frm_tab){
                        if(base_scr!=fi->vm->GetScript()){
                           conjuct_tab = NULL;
                        }else{
                              //check mis-matching elements and specially mark
                           for(int i=conjuct_tab->NumItems(); i--; ){
                              E_TABLE_ELEMENT_TYPE et = conjuct_tab->GetItemType(i);
                              dword sz = conjuct_tab->SizeOf(i);
                              if(sz && memcmp(conjuct_tab->Item(i), frm_tab->Item(i), sz)){
                                 switch(et){
                                 case TE_INT: conjuct_tab->ItemI(i) = TAB_INV_INT; break;
                                 case TE_FLOAT: conjuct_tab->ItemF(i) = TAB_INV_FLOAT; break;
                                 case TE_BOOL: (byte&)conjuct_tab->ItemB(i) = TAB_INV_BOOL; break;
                                 case TE_ENUM: conjuct_tab->ItemE(i) = TAB_INV_ENUM; break;
                                 case TE_STRING: conjuct_tab->ItemS(i)[0] = TAB_INV_STRING[0]; break;
                                 default: assert(0);
                                 }
                              }
                           }
                        }
                     }
                  }
                  if(!tt)
                     tt = fi->vm->GetScript()->GetTableTemplate();
               }
            }
            if(any_script){
               if(conjuct_tab && tt){

                  hwnd_tab_edit = (HWND)conjuct_tab->Edit(tt, igraph->GetHWND(),
                     cbTable,
                     (dword)this,
                     TABEDIT_HIDDEN | TABEDIT_EXACTHEIGHT |
                     TABEDIT_CENTER | TABEDIT_MODELESS | TABEDIT_IMPORT | TABEDIT_EXPORT,
                     NULL);
                  if(hwnd_tab_edit){
                     ed->GetIGraph()->EnableSuspend(false);
                     HWND hwnd = hwnd_tab_edit;

                     e_props->AddSheet(hwnd, true);
                     e_props->Activate();
                     SetFocus(hwnd);

                     edited_template = tt;
                              //collect all selected frames and keep it in smart ptr
                     multiple_edited_frames.clear();
                     for(int k=sel_list.size(); k--; ){
                        PI3D_frame frm = sel_list[k];
                        multiple_edited_frames.push_back(frm);
                     }
                  }
                  break;
               }
            }
            ed->Message(C_fstr("Can't edit script table (%s)", base_scr ? "not same tables" : "no table"));
         }
         break;

      case E_SCRIPT_TOGGLE_NAMES:
         draw_script_names = !draw_script_names;
         ed->CheckMenu(this, E_SCRIPT_TOGGLE_NAMES, draw_script_names);
         ed->Message(C_fstr("Draw script names %s", draw_script_names ? "on" : "off"));
         break;

      case E_SCRIPT_CONFIG:
         {
            config_tab->Edit(GetTemplate(), ed->GetIGraph()->GetHWND(), 
               NULL, 0,
               TABEDIT_IMPORT | TABEDIT_EXPORT,
               NULL);
            CreateFont();
         }
         break;

      case E_SCRIPT_SLCT_NOTIFY:
         {
            HighlightButtons();
            reset_tab_editor = true;

                              //check if selection is single and has script
                              // if so, select the script
            C_vector<PI3D_frame> &sel_list = *(C_vector<PI3D_frame>*)context;
            if(in_prop && sel_list.size()==1){
               PI3D_frame frm = sel_list.front();
               PS_frame_info fi = GetFrameInfo(frm);
               if(fi && fi->vm){
                  HWND hwnd_lv = GetDlgItem(hwnd_list, IDC_LIST);
                  assert(hwnd_lv);
                  LVITEM lvi;
                  memset(&lvi, 0, sizeof(lvi));
                              //unselect all first
                  lvi.state = 0;
                  lvi.stateMask = LVIS_SELECTED;
                  SendMessage(hwnd_lv, LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&lvi);
                              //find the script assigned to the frame
                  for(dword i=SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0); i--; ){
                     lvi.iItem = i;
                     lvi.mask = LVIF_PARAM | LVIF_STATE;
                     SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM)&lvi);
                     S_script_info *si = (S_script_info*)lvi.lParam;
                     if(si->full_name==fi->vm->GetScript()->GetName()){
                              //found, make it selected
                        lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
                        lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
                        SendMessage(hwnd_lv, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
                        break;
                     }
                  }
               }
            }
         }
         break;

         /*
      case E_SCRIPT_IS_UP_TO_DATE:
         return IsUpToDate((const char*)context);
         */

      case E_SCRIPT_COMPILE:
         return CompileScript((const char*)context, false);

      case E_SCRIPT_SET_NAMED:
         {
            const char *cp = (const char*)context;
            int len = strlen(cp);
            PI3D_frame frm = mission->GetScene()->FindFrame(cp);
            if(!frm){
               ed->Message(C_fstr("C_edit_Script(E_SCRIPT_SET_NAMED): can't find frame %s", cp));
               break;
            }
            cp += len + 1;

            S_frame_info *fi;
            fi = CreateFrameInfo(frm);
            assert(!fi->vm);

            AssignScriptToFrame(frm, cp);
         }
         break;

      case E_SCRIPT_REMOVE_NAMED:
         {
            const char *frm_name = (const char*)context;
            PI3D_frame frm = mission->GetScene()->FindFrame(frm_name);
            if(!frm){
               ed->Message(C_fstr("C_edit_Script(E_SCRIPT_REMOVE_NAMED): can't find frame %s", frm_name));
               break;
            }
            DeleteScriptFromFrame(frm);
         }
         break;

      case E_SCRIPT_SET_TABLE:
         {
            /*
            char *name = (char *)context;
            void *pTabDataLen = (byte *)(context)+strlen(name)+1;
            size_t TabDataLen = *(dword *)(pTabDataLen);
            byte *pTabData = (byte *)pTabDataLen+sizeof(size_t);
         
            PI3D_frame frm = NULL;
            if(strlen(name)){
               frm = mission->GetScene()->FindFrame(name);
               if(!frm){
                  ed->Message(C_fstr("C_edit_Script(E_SCRIPT_SET_TABLE): can't find frame %s", name));
                  break;
               }

               S_frame_info *fi = CreateFrameInfo(frm);
               PC_table tab = fi->vm->GetTable();
            
               C_cache tab_cache;
               bool b = tab_cache.open(&pTabData, &TabDataLen, CACHE_READ_MEM);
               assert(b);

               if(!tab){
                  tab = CreateTable();
                  fi->vm->tab = tab;
                  tab->Release();
               }

               b = tab->Open((dword)&tab_cache, TABOPEN_FILEHANDLE);
               assert(b);
               tab_cache.close();
            }
            */
         }
         break;
      }
      return 0;
   }

   virtual void Render(){

      if(ed->IsActive() && draw_script_names){
         for(int i=scripted_frames.size(); i--; ){
            PI3D_frame frm = scripted_frames[i];
            S_frame_info *fi = GetFrameInfo(frm);
            if(fi && fi->vm){
               DrawScriptName(frm, fi->vm->GetName());
            }else{
                              //script not here? remove from our list
               scripted_frames[i] = scripted_frames.back(); scripted_frames.pop_back();
            }
         }
      }
   }

//----------------------------

   enum E_CK_STATE{
      CK_EC_POS_SIZE = 1000,  //int[4]
      CK_EC_ERR_HEIGHT,       //int
      CK_DRAW_NAMES,          //bool
      CK_CONFIG_TAB,          //C_table
      CK_SEARCH_WHOLE,
      CK_SEARCH_CASE,
   };

#define VERSION 7

   virtual bool LoadState(C_chunk &ck){

                              //check version
      byte version = VERSION-1;
      ck.Read(&version, sizeof(byte));
      if(version != VERSION){
         if(version != VERSION-1)
            return false;
                              //read table
         config_tab->Load(ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
                              //read other variables
         ck.Read(&draw_script_names, sizeof(byte));
         ck.Read(&ec_pos_size, sizeof(ec_pos_size));
         ck.Read(&ec_err_height, sizeof(ec_err_height));
      }else{
         while(ck)
         switch(++ck){
         case CK_EC_POS_SIZE:
            ck.Read(&ec_pos_size, sizeof(ec_pos_size));
            --ck;
            break;
         case CK_EC_ERR_HEIGHT: ec_err_height = ck.RIntChunk(); break;
         case CK_DRAW_NAMES: draw_script_names = ck.RByteChunk(); break;
         case CK_SEARCH_WHOLE: search_whole = ck.RByteChunk(); break;
         case CK_SEARCH_CASE: search_case = ck.RByteChunk(); break;
         case CK_CONFIG_TAB:
            config_tab->Load(ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
            --ck;
            break;
         default:
            --ck;
         }
      }
      DestroyFont();
      ed->CheckMenu(this, E_SCRIPT_TOGGLE_NAMES, draw_script_names);

      return true; 
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{ 

                              //write version
      byte version = VERSION;
      ck.Write(&version, sizeof(byte));

      if(ec_edit)
         ec_edit->GetPosSize((int*)ec_pos_size);

      ck <<= CK_CONFIG_TAB;
         config_tab->Save(ck.GetHandle(), TABOPEN_FILEHANDLE);
      --ck;

      ck <<= CK_EC_POS_SIZE;
         ck.Write(&ec_pos_size, sizeof(ec_pos_size));
      --ck;

      ck.WIntChunk(CK_EC_ERR_HEIGHT, ec_err_height);
      ck.WByteChunk(CK_DRAW_NAMES, draw_script_names);
      ck.WByteChunk(CK_SEARCH_WHOLE, search_whole);
      ck.WByteChunk(CK_SEARCH_CASE, search_case);

      //ck.Write((const char*)selected_script, selected_script.Size()+1);
      return false; 
   }
};

//----------------------------

void InitPlugin_Script(PC_editor editor, C_game_mission &m1){
   { C_edit_Script *ei = new C_edit_Script(m1); editor->InstallPlugin(ei); ei->Release(); }
}

#endif                        //EDITOR

//----------------------------

