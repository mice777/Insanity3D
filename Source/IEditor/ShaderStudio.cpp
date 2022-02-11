#include "all.h"
#include "common.h"
#include <editctrl.h>
#include <insanity\sourcecontrol.h>
#include <d3dx9.h>

//----------------------------

const char SHADER_DIR[] = "Shaders";

//----------------------------
/*
                              //names of callback functions possible in shaders
                              // (for highlight in editor)
static const char *fnc_callback_names[] = {
   "Main", "Exit", "AI",
   "GameBegin", "GameEnd", "CinematicEnd", "TestCinematic",
   "OnHit", "OnCollision", "OnSignal", "OnUse", "OnDestroy",
   "OnNote", "OnIdle", "CanBeUsed",
   "OnWatchCome", "OnWatchLeave", "OnListenCome",
   NULL
};
*/

//----------------------------
//----------------------------

static const struct{
   const char *title;
   int image;                 //one-based, 0 = no image
   dword width;
} sel_list[] = {
   {"Name", 0, 140},
   {"#", 0, 24},
};

//----------------------------
                              
#define WM_USER_INIT_SHEET (WM_USER+100)


class C_edit_ShaderStudio: public C_editor_item{
   virtual const char *GetName() const{ return "ShaderStudio"; }

//----------------------------

   enum{
      E_MAIN_ACTION,
      E_SLCT_NOTIFY,
   };

//----------------------------

   C_smart_ptr<C_edit_control> ec_edit, ec_errors;

   HWND ec_hwnd, ec_err_hwnd; //NULL if no file is open
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Properties> e_props;

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

   virtual void AfterLoad(){

      HighlightButtons();

                        //collect all frames having shader
      struct S_hlp{
         C_vector<C_smart_ptr<S_shader_info> > *shd_list;

         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
            PI3D_visual vis = I3DCAST_VISUAL(frm);
            switch(vis->GetVisualType()){
            case 'SHDR':
               {
                  S_hlp *hp = (S_hlp*)c;
                  PI3D_visual_shader vs = (PI3D_visual_shader)vis;
                  PI3D_shader shd = vs->GetShader();
                  if(shd){
                     const C_str &name = shd->GetName();
                     for(int i=hp->shd_list->size(); i--; ){
                        if((*hp->shd_list)[i]->full_name.Matchi(name)){
                           ++(*hp->shd_list)[i]->use_count;
                           break;
                        }
                     }
                     if(i==-1){
                        hp->shd_list->push_back(new C_edit_ShaderStudio::S_shader_info);
                        C_edit_ShaderStudio::S_shader_info *si = hp->shd_list->back();
                        ++si->use_count;
                        si->SetName(name);
                        si->Release();
                        si->shd = shd;
                     }
                  }
               }
               break;
            }
            return I3DENUMRET_OK;
         }
      } hlp = {&shd_list};
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp, ENUMF_VISUAL);
      redraw_sheet = true;

      if(last_edit_shader.Size()){
         EditShader(last_edit_shader, false);
         last_edit_shader = NULL;
      }
   }

//----------------------------

   virtual void BeforeFree(){

      last_edit_shader = NULL;
      if(ec_hwnd){
         last_edit_shader = (const char*)ec_edit->GetUserData();
         ec_edit->Save();
         DestroyWindow(ec_hwnd);
      }
      SendDlgItemMessage(hwnd_list, IDC_LIST, LVM_DELETEALLITEMS, 0, 0);
      shd_list.clear();
      struct S_hlp{
         C_vector<C_smart_ptr<S_shader_info> > *shd_list;
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
      /*
            S_frame_info *fi = GetFrameInfo(frm);
            if(fi && fi->vm){
               S_hlp *hp = (S_hlp*)c;
                        //remove from scripted frames
               for(int i=hp->scripted_frames->size(); i--; )
               if((*hp->scripted_frames)[i]==frm){
                  (*hp->scripted_frames)[i] = hp->scripted_frames->back(); hp->scripted_frames->pop_back();
                  break;
               }
                        //remove from script list
               const C_str &name = fi->vm->GetName();
               for(i=hp->shd_list->size(); i--; ){
                  if((*hp->shd_list)[i]->full_name.Matchi(name)){
                     if(!--(*hp->shd_list)[i]->use_count){
                        (*hp->shd_list)[i] = hp->shd_list->back();
                        hp->shd_list->pop_back();
                     }
                     break;
                  }
               }
            }
      */
            return I3DENUMRET_OK;
         }
      } hlp = {&shd_list};
      //S_hlp::cbEnum(ed->GetScene()->GetPrimarySector(), (dword)&hlp);
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp, ENUMF_VISUAL);
   }

//----------------------------
public:
                              //unique shaders loaded with current scene
   struct S_shader_info: public C_unknown{
      C_str friendly_name;
      C_str full_name;        //shader's full name (relative to shader dir, without extension, lowercase)
      int use_count;
      int status;             //0=unknown, 1=compile ok, 2=compile fail
      bool writtable;
      C_smart_ptr<I3D_shader> shd;

      S_shader_info():
         use_count(0),
         writtable(false),
         status(0)
      {}
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
         writtable = !OsIsFileReadOnly(C_fstr("%s\\%s.fx", SHADER_DIR, (const char*)full_name));
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
                              //list of loaded shaders - unsorted
   C_vector<C_smart_ptr<S_shader_info> > shd_list;

   C_str selected_shader;     //full-name of selected shader (lower case)

   C_str last_edit_shader;

                              //frames in current scenes containing shaders
                              // (due to names rendering)
   //C_vector<C_smart_ptr<I3D_frame> > scripted_frames;

//----------------------------

   bool GetCreateFileName(C_str &str, const char *title){

      char buf[MAX_PATH];
      OPENFILENAME on;
      memset(&on, 0, sizeof(on));
      on.lStructSize = sizeof(on);
      on.hwndOwner = (HWND)ed->GetIGraph()->GetHWND();
      on.lpstrFilter = "Shader files (*.fx)\0*.fx\0""All files\0*.*\0";
      on.nFilterIndex = 1;
      on.lpstrFile = buf;
      buf[0] = 0;
      on.nMaxFile = sizeof(buf);
      on.lpstrInitialDir = str;
      on.lpstrTitle = title;
      on.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR |
         OFN_CREATEPROMPT;
         //OFN_NOTESTFILECREATE;
      on.lpstrDefExt = "fx";
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
                              //append shader dir
         C_fstr dir("%s\\%s\\", cwd, SHADER_DIR);
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
   
   /*
   void DeleteSelectedShader(){

      if(!selected_shader.Size())
         return;
      if(!ed->CanModify())
         return;
                              //find in our list
      for(int i=shd_list.size(); i--; )
         if(shd_list[i]->full_name==selected_shader)
            break;
      assert(i!=-1);
      int count = shd_list[i]->use_count;
      if(count>=1){
                              //phase 1 - remove from all used
         int id = MessageBox(hwnd_list, 
            C_fstr("Remove shader '%s' from all frames?", (const char*)selected_shader),
            "Delete shader", MB_YESNOCANCEL);
         if(id==IDYES){
                              //delete from all frames
            struct S_hlp{
               C_edit_ShaderStudio *ei;
               C_str name;
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                  S_frame_info *fi = GetFrameInfo(frm);
                  S_hlp *hp = (S_hlp*)c;
                  if(fi && fi->vm && hp->name.Matchi(fi->vm->GetName())){
                     hp->ei->DeleteShaderFromFrame(frm);
                  }
                  return I3DENUMRET_OK;
               }
            } hlp;
            hlp.ei = this;
            hlp.name = selected_shader;
            S_hlp::cbEnum(ed->GetScene()->GetPrimarySector(), (dword)&hlp);
            ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
            ed->SetModified();
         }
      }else{
                              //phase 2 - remove from sheet
                              // not used anywhere, so just remove our reference
         C_str curr_name = shd_list[i]->friendly_name;
         shd_list[i] = shd_list.back(); shd_list.pop_back();

                              //select next adjacent shader
         int next_i = -1;
         for(i=shd_list.size(); i--; ){
            const C_str &name = shd_list[i]->friendly_name;
            if(curr_name<name){
               if(next_i==-1)
                  next_i = i;
               else
               if(name<shd_list[next_i]->friendly_name)
                  next_i = i;
            }
         }
         if(next_i==-1){
            for(i=shd_list.size(); i--; ){
               const C_str &name = shd_list[i]->friendly_name;
               if(name<curr_name){
                  if(next_i==-1)
                     next_i = i;
                  else
                  if(shd_list[next_i]->friendly_name<name)
                     next_i = i;
               }
            }
         }
         selected_shader = NULL;
         if(next_i!=-1)
            selected_shader = shd_list[next_i]->full_name;

         redraw_sheet = true;
      }
   }
   */

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

   bool CloseEditedShader(){

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
// Edit shader with specified name. The name is given without extension,
// and is relative to shader root dir SHADER_DIR.
   void EditShader(const C_str &name, bool activate = true){

      ECTRL_RESULT er;
      if(!CloseEditedShader())
         return;
      ec_edit = CreateEditControl(); ec_edit->Release();
      ec_edit->Init("Editor\\Shaders\\Editor.mac", "Editor\\Shaders\\Editor.cfg");
      /*
                              //add function names for highlighting
      const VM_LOAD_SYMBOL *syms = script_man->GetLinkSymbols();
      while(syms->address){
         if(syms->name)
            ec_edit->AddHighlightWord(1, (syms++)->name);
         else
            syms = (VM_LOAD_SYMBOL*)syms->address;
      }
      */
      //for(int i=0; fnc_callback_names[i]; i++)
         //ec_edit->AddHighlightWord(3, fnc_callback_names[i]);


      ec_edit->SetCallback(ECCallBack_thunk, this);
      C_fstr scr_name("%s\\%s.fx", SHADER_DIR, (const char*)name);
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
            "Edit shader", MB_OK);
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
// Check if shader is up-to-date.
   bool IsUpToDate(const C_str &name) const{

      bool up_to_date = false;
      int h_dst = dtaOpen(C_fstr("%s\\%s.dll", SHADER_DIR, (const char*)name));
      if(h_dst!=-1){
         up_to_date = IsFileNewerThan(h_dst, C_fstr("%s\\%s.fx", SHADER_DIR, (const char*)name));
         for(int i=0; up_to_date; i++){
            const char *dep_file = dependency_files[i];
            if(!dep_file)
               break;
            up_to_date = IsFileNewerThan(h_dst, C_fstr("%s\\%s", SHADER_DIR, dep_file));
         }
         dtaClose(h_dst);
      }
      return up_to_date;
   }
   */

//----------------------------
// Compile shader. If 'check_date' is true, date of shader source and compiled destination is checked
// to see if compiling is necessary.
   bool CompileShader(const C_str &name, bool display_errors){

      struct S_hlp{
         static void I3DAPI CompileErr(const char *msg, void *context, int l, int r, bool warn){
            C_vector<char> &errors = *(C_vector<char>*)context;
            errors.insert(errors.end(), msg, msg+strlen(msg));
            errors.push_back('\n');
         }

         /*
         PC_shader shd;
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
         */
      };
      C_vector<char> errors;
      /*
                              //find the shader
      C_script_manager::t_script_map::iterator it = script_man->script_map.find(name);
      if(it==script_man->script_map.end()){
      }
      */
      PI3D_shader shd = NULL;
      for(int i=shd_list.size(); i--; ){
         if(shd_list[i]->full_name.Matchi(name)){
            shd = shd_list[i]->shd;
            break;
         }
      }
      if(!shd){
         ed->Message("Cannot compile shader - not found");
         return false;
      }
      C_fstr filename("%s\\%s.fx", SHADER_DIR, (const char*)name);

                              //compile first on a temp script, so that we don't lose tables if compiling fails
      bool ok = shd->Compile(filename,
         //ISLCOMP_PRECOMPILE | ISLCOMP_PRECOMPILE_HIDDEN,
         0,
         S_hlp::CompileErr, &errors);
      if(ok){
         /*
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
                  if(new_tab){
                     //new_tab->Open((dword)(PC_table)old_tab, TABOPEN_DUPLICATE | TABOPEN_UPDATE);
                     CopyOldTableCompatible(new_tab, old_tab, scr->GetTableTemplate(), tt_old_names);
                  }
               }
            }
         }
         */
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
               int lnum, sz;
               if(sscanf(&buf[i+1], "%i):%n", &lnum, &sz)==1){
                  i += sz;
                  assert(ec_edit);

                  int l, r;
                  ec_edit->GetCurrPos(l, r);
                  ec_edit->SetCurrPos(lnum-1, r);
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
      C_edit_ShaderStudio *ei = (C_edit_ShaderStudio*)context;
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
      C_edit_ShaderStudio *ei = (C_edit_ShaderStudio*)context;
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
         CompileShader(name, true);
                              //re-init sheet
         SendMessage(hwnd_list, WM_USER_INIT_SHEET, 0, 0);
      }else
      if(!strcmp(msg, "close")){
         ec_edit->GetPosSize(ec_pos_size);
         SetFocus((HWND)ed->GetIGraph()->GetHWND());
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
            SetFocus((HWND)ed->GetIGraph()->GetHWND());
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
         OsMessageBox(ed->GetIGraph()->GetHWND()
            , C_fstr("Failed to save shader. Please make sure the file may be modified."),
            "Saving file...", MBOX_OK);
      }else{
         //assert(0);
      }
      return true;
   }

//----------------------------

   static BOOL CALLBACK dlgSheet_thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      C_edit_ShaderStudio *ei = (C_edit_ShaderStudio*)GetWindowLong(hwnd, GWL_USERDATA);
      if(!ei){
         if(uMsg!=WM_INITDIALOG)
            return 0;
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         ei = (C_edit_ShaderStudio*)lParam;
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
            for(int i=0; i < (sizeof(sel_list)/sizeof(sel_list[0])); i++){
               LVCOLUMN lvc;
               memset(&lvc, 0, sizeof(lvc));
               lvc.mask = LVCF_WIDTH;
               lvc.cx = sel_list[i].width;
               if(sel_list[i].title){
                  lvc.pszText = (char*)sel_list[i].title;
                  lvc.cchTextMax = strlen(sel_list[i].title);
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
                              //feed in shaders
            for(int i=shd_list.size(); i--; ){
               const S_shader_info &si = *shd_list[i];

               static LVITEM lvi = { LVIF_PARAM | LVIF_STATE | LVIF_IMAGE};
               lvi.iItem = 0;
               lvi.lParam = (LPARAM)(S_shader_info*)&si;
               lvi.state = 0;
               lvi.iImage = si.status;
               if(si.full_name==selected_shader)
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
                  const C_edit_ShaderStudio::S_shader_info *si1 = (C_edit_ShaderStudio::S_shader_info*)lParam1;
                  const C_edit_ShaderStudio::S_shader_info *si2 = (C_edit_ShaderStudio::S_shader_info*)lParam2;
                  return strcmp(si1->friendly_name, si2->friendly_name);
               }
            };
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SORTITEMS, NULL, (LPARAM)S_hlp::CompareFunc);
            /*
            if(selected_shader.Size()){
               for(i=SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0); i--; ){
                  SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETSELECTIONMARK, 0, i);
                  break;
               }
            }
            */
         }
         break;

         /*
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
               IDC_BUTTON_SCRIPT_CLOSE,  "Close script editor.",
               IDC_LIST,               "List of scripts loaded in the script editor and used by frames in the scene.\nThe left part shows name of script, and right column shows how many frames in scene have assigned particular script.",
               0              //terminator
            };
            DisplayControlHelp(hwnd, (word)hi->iCtrlId, help_texts);
         }
         break;
         */

      case WM_NOTIFY:
         {
            LPNMHDR pnmh = (LPNMHDR)lParam; 
            switch(pnmh->code){
               /*
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
               */

            case NM_DBLCLK:
               {
                  if(selected_shader.Size())
                     EditShader(selected_shader);
               }
               break;
            case LVN_ITEMCHANGED:
               {
                  NMLISTVIEW *lv = (NMLISTVIEW*)lParam;
                  if(lv->uChanged&LVIF_STATE){
                     S_shader_info *si = (S_shader_info*)lv->lParam;
                     assert(si);
                     if(lv->uNewState&LVIS_SELECTED){
                        selected_shader = si->full_name;
                     }else
                     if(selected_shader==si->full_name){
                        selected_shader = NULL;
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
               C_fstr str = SHADER_DIR;
               str.ToLower();
               if(GetCreateFileName(str, "Open shader")){
                              //check if such shader is already open
                  for(int i=shd_list.size(); i--; ){
                     if(shd_list[i]->full_name==str)
                        break;
                  }
                  if(i==-1){
                     i = shd_list.size();
                     shd_list.push_back(new S_shader_info);
                     S_shader_info *si = shd_list.back();
                     si->SetName(str);
                     si->Release();
                     si->shd = ed->GetDriver()->CreateShader();
                     si->shd->Release();
                  }
                  selected_shader = shd_list[i]->full_name;
                  redraw_sheet = true;
               }
            }
            break;

         case IDC_BUTTON_SCRIPT_CHECK_OUT:
         case IDC_BUTTON_SCRIPT_CHECK_IN:
            {
               bool out = (LOWORD(wParam)==IDC_BUTTON_SCRIPT_CHECK_OUT);
               if(!selected_shader.Size()){
                  ed->Message("Please select shader to make selection by.");
                  break;
               }
               for(int i=shd_list.size(); i--; ){
                  S_shader_info &si = *shd_list[i];
                  if(si.full_name==selected_shader){
                     if(si.writtable==out){
                        ed->Message(out ?
                           "This shader is writtable, can't check out." :
                           "This shader is read-only, can't check in."
                           );
                     }else{
                        C_fstr fname("%s\\%s.fx", SHADER_DIR, (const char*)si.full_name);
                        bool ok;
                        if(out)
                           ok = SCCheckOutFile("Atlantica", fname);
                        else
                           ok = SCCheckInFile("Atlantica", fname, false);
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

            /*
         case IDC_BUTTON_SCRIPT_SEARCH:
            {
                              //get search word
               struct S_hlp{
                  C_str str;
                  bool whole;
                  bool case_sens;
                  HWND hwnd_parent;

                  static BOOL CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

                     switch(uMsg){
                     case WM_INITDIALOG:
                        {
                           const S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);

                           OsCenterWindow(hwnd, hp->hwnd_parent);
                           SetWindowLong(hwnd, GWL_USERDATA, lParam);

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
               hlp.hwnd_parent = (HWND)ed->GetIGraph()->GetHWND();

               if(ec_edit){
                              //save changes
                  ECTRL_RESULT er = ec_edit->Save(true);
                  if(ECTRL_FAIL(er))
                     break;
               }

               int ret = DialogBoxParam(GetModuleHandle(NULL), "IDD_SEARCH", (HWND)ed->GetIGraph()->GetHWND(),
                  S_hlp::DialogProc, (LPARAM)&hlp);
               if(ret && hlp.str.Size()){
                              //we got string and params, search now
                  last_search_string = hlp.str;
                  search_whole = hlp.whole;
                  search_case = hlp.case_sens;

                  C_vector<S_file_info> files;

                  for(dword i=shd_list.size(); i--; ){
                     C_cache ck;
                     C_fstr filename("%s\\%s.fx", SHADER_DIR, (const char*)shd_list[i]->full_name);
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
                                 fi.fname = shd_list[i]->friendly_name;
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
                        if(selected_shader.Size() && shd_list[fi.indx]->full_name.Matchi(selected_shader))
                           n = i;
                     }
                     selection.push_back(0);
                     n = ChooseItemFromList(ed->GetIGraph(), NULL, "Select shader", &(*selection.begin()), n);
                     if(n!=-1){
                        const S_file_info &fi = files[n];
                        selected_shader = shd_list[fi.indx]->full_name;
                        EditShader(selected_shader);
                        if(ec_edit)
                           ec_edit->SetCurrPos(fi.line, fi.row);

                              //select shader in the list
                        {
                           HWND hwnd_lv = GetDlgItem(hwnd_list, IDC_LIST);
                           assert(hwnd_lv);
                           LVITEM lvi;
                           memset(&lvi, 0, sizeof(lvi));
                                       //unselect all first
                           lvi.state = 0;
                           lvi.stateMask = LVIS_SELECTED;
                           SendMessage(hwnd_lv, LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&lvi);
                                       //find the edited shader
                           for(dword i=SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0); i--; ){
                              lvi.iItem = i;
                              lvi.mask = LVIF_PARAM | LVIF_STATE;
                              SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM)&lvi);
                              S_shader_info *si = (S_shader_info*)lvi.lParam;
                              if(si->full_name==selected_shader){
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
                        C_fstr("String '%s' was not found in any shader.", (const char*)last_search_string), "Search in shaders", MB_OK);
                  }
               }
            }
            break;

         case IDC_BUTTON_SCRIPT_DEL:
            DeleteSelectedShader();
            break;

         case IDC_BUTTON_SCRIPT_ASSIGN:
         case IDC_BUTTON_SCRIPT_REMOVE:
            {
               if(!ed->CanModify()) break;
               C_str name;
               if(LOWORD(wParam)==IDC_BUTTON_SCRIPT_ASSIGN){
                  name = selected_shader;
                  if(!name.Size()){
                     ed->Message("Please select shader to assign");
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
               if(!selected_shader.Size()){
                  ed->Message("Please select shader to make selection by.");
                  break;
               }
                              //clear current selection
               e_slct->Clear();

                              //select all frames using this shader
               struct S_hlp{
                  C_edit_ShaderStudio *ei;
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
               hlp.name = selected_shader;
               hlp.e_slct = e_slct;
               hlp.num = 0;
               S_hlp::cbEnum(mission->GetScene()->GetPrimarySector(), (dword)&hlp);
               mission->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
               ed->Message(C_fstr("%i frame(s) selected.", hlp.num));
            }
            break;
            */

         case IDC_BUTTON_SCRIPT_EDIT:
            {
               if(selected_shader.Size())
                  EditShader(selected_shader);
            }
            break;

         case IDC_BUTTON_SCRIPT_COMP:
            {
               if(!selected_shader.Size()){
                  ed->Message("Please select shader to compile.");
                  break;
               }
               CompileShader(selected_shader, false);
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

   HWND hwnd_list;            //handle of sheet containing list of shaders
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

   const C_table_template *edited_template;
   C_vector<C_smart_ptr<I3D_frame> > multiple_edited_frames;

   bool redraw_sheet;         //true to redraw sheet in next Tick

//----------------------------
public:
   C_edit_ShaderStudio():
      hwnd_list(NULL),
      in_prop(false),
      edited_template(NULL),
      search_whole(false),
      search_case(false),
      ec_hwnd(NULL), ec_err_hwnd(NULL)
   {
      ec_pos_size[0] = ec_pos_size[1] = 10;
      ec_pos_size[2] = ec_pos_size[3] = 300;
      ec_err_height = 150;
   }
   ~C_edit_ShaderStudio(){
   }

   virtual bool Init(){

      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      if(!e_props || !e_slct || !e_undo){
         return false;
      }
                              //be sensible on selection change
      e_slct->AddNotify(this, E_SLCT_NOTIFY);

      ed->AddShortcut(this, E_MAIN_ACTION, "&Create\\S&hader Studio\tI", K_I, 0);

      hwnd_list = CreateDialogParam(GetHInstance(), "IDD_SHADER_EDIT",
         (HWND)ed->GetIGraph()->GetHWND(),
         dlgSheet_thunk, (LPARAM)this);
      
      return true;
   }

   virtual void Close(){

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
   }

   virtual dword Action(int id, void *context){

      switch(id){
      case E_MAIN_ACTION:
         {
                              //if shader is being edited, activate it
            if(ec_hwnd){
               SetActiveWindow(ec_hwnd);
               break;
            }
                              //if selected frame is shaded visual, start editing it
            PI3D_frame sel_frm = e_slct->GetSingleSel();
            if(sel_frm && sel_frm->GetType()==FRAME_VISUAL){
               PI3D_visual vis = I3DCAST_VISUAL(sel_frm);
               switch(vis->GetVisualType()){
               case 'SHDR':
                  {
                     PI3D_visual_shader vs = (PI3D_visual_shader)vis;
                     PI3D_shader shd = vs->GetShader();
                     if(shd){
                        EditShader(shd->GetName());
                     }
                  }
                  break;
               default:
                  sel_frm = NULL;
               }
               if(sel_frm)
                  break;
            }

                              //toggle sheet
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

         /*
      case E_SLCT_NOTIFY:
         {
            HighlightButtons();

                              //check if selection is single and has shader
                              // if so, select the shader
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
                              //find the shader assigned to the frame
                  for(dword i=SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0); i--; ){
                     lvi.iItem = i;
                     lvi.mask = LVIF_PARAM | LVIF_STATE;
                     SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM)&lvi);
                     S_shader_info *si = (S_shader_info*)lvi.lParam;
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

                              //broadcast actions:

      case EDIT_ACTION_FRAME_DELETE:
         DeleteScriptFromFrame((PI3D_frame)context);
         break;


      case EDIT_ACTION_FRAME_DUPLICATE:
         {
            const pair<PI3D_frame, PI3D_frame> &p = *(pair<PI3D_frame, PI3D_frame>*)context;
            PI3D_frame f_old = p.first;
            PI3D_frame f_new = p.second;
            S_frame_info *fi_old = GetFrameInfo(f_old);
            if(fi_old && fi_old->vm){
               AssignScriptToFrame(f_new, fi_old->vm->GetName(), fi_old->vm->GetTable());
            }
         }
         break;

      case EDIT_ACTION_MISSION_SAVE_2:
         {
            C_chunk *ck = (C_chunk*)context;
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
                        tab->Save((dword)ck.GetHandle(), TABOPEN_FILEHANDLE);
                        --ck;
                     }
                     --ck;
                  }
                  return I3DENUMRET_OK;
               }
            } hlp;
            hlp.ck = ck;
            hlp.gm = mission;
            S_hlp::cbEnum(ed->GetScene()->GetPrimarySector(), (dword)&hlp);
            mission->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
         }
         break;
         */
      }
      return 0;
   }

   virtual void Render(){
   }

//----------------------------

   enum E_CK_STATE{
      CK_EC_POS_SIZE = 1000,  //int[4]
      CK_EC_ERR_HEIGHT,       //int
      CK_SEARCH_WHOLE,
      CK_SEARCH_CASE,
   };

   virtual bool SaveState(C_chunk &ck) const{ 

      if(ec_edit)
         ec_edit->GetPosSize((int*)ec_pos_size);

      ck <<= CK_EC_POS_SIZE;
         ck.Write(&ec_pos_size, sizeof(ec_pos_size));
      --ck;

      ck.WIntChunk(CK_EC_ERR_HEIGHT, ec_err_height);
      ck.WByteChunk(CK_SEARCH_WHOLE, search_whole);
      ck.WByteChunk(CK_SEARCH_CASE, search_case);

      //ck.Write((const char*)selected_script, selected_script.Size()+1);
      return false; 
   }

   virtual bool LoadState(C_chunk &ck){

      while(ck)
      switch(++ck){
      case CK_EC_POS_SIZE:
         ck.Read(&ec_pos_size, sizeof(ec_pos_size));
         --ck;
         break;
      case CK_EC_ERR_HEIGHT: ec_err_height = ck.RIntChunk(); break;
      case CK_SEARCH_WHOLE: search_whole = ck.RByteChunk(); break;
      case CK_SEARCH_CASE: search_case = ck.RByteChunk(); break;
      default:
         --ck;
      }
      return true; 
   }
};

//----------------------------

void CreateShaderStudio(PC_editor editor){
   C_edit_ShaderStudio *ei = new C_edit_ShaderStudio; editor->InstallPlugin(ei); ei->Release();
}

//----------------------------
//----------------------------

