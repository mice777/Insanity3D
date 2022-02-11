#include "all.h"
#include "common.h"
#include <c_cache.h>


#define SEED_VERSION 0xd

//----------------------------

enum E_ALIGN_MODE{
   ALIGN_Y_UP,
   ALIGN_Y_NORMAL,
   ALIGN_Z_NORMAL,
   ALIGN_LAST
};

//----------------------------

static const S_ctrl_help_info help_texts[] = {
   IDC_SEED_SET_NAME,   "Name of active set. You may choose another set from this list.",
   IDC_SEED_CREATE_SET, "Add new set to list of the sets.",
   IDC_SEED_RENAME_SET, "Rename currently selected set.",
   IDC_SEED_DELETE_SET, "Delete currently selected set from the list of sets.",
   IDC_SEED_MODELS,     "List of models in current set.\nIf you make a selection in this box, only selected models will be put into the scene.",
   IDC_SEED_ADD_MODELS, "Add models to the set from current selection.",
   IDC_SEED_BROWSE_MODEL, "Add model to the set, using a browser dialog.",
   IDC_SEED_DELETE_MODEL, "Delete selected models from the set.",
   IDC_SET_DATA,        "Data associated with the set.",
   IDC_SEED_ROTATION,   "Check to allow random rotation around align axis for models placed to the scene.",
   IDC_ALIGN_MODE_TITLE,"Mode how placed models will be aligned to scene location.",
   IDC_ALIGN_MODE,      "Mode how placed models will be aligned to scene location.",
   IDC_SCALE_TITLE,     "Random scale of models put into the scene.",
   IDC_SCALE,           "Type to change random scale of models in the set. The real scale will be 1.0 +/- random number up to value typed here.",
   IDC_SEED_PUT,        "Switch to mode where models are put to scene by mouse clicking.",
   IDCLOSE,             "Close the Seed dialog.",
   0
};

//----------------------------

static const char DEF_FILE_NAME[] = "editor\\seeds.txt";

//----------------------------

class C_edit_Seed: public C_editor_item{
   virtual const char *GetName() const{ return "Seed"; }

//----------------------------

   enum E_ACTION_SEED{
      E_SEED_TOGGLE_DIALOG = 13000, //toggle seed dialog on/off
      E_SEED_PICK_MODEL,         // - internal callback, pick model
      E_SEED_PUT_MODEL,          // - internal callback, put random model
   };

   enum{
      UNDO_DELETE,
   };

//----------------------------

   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_MouseEdit> e_medit;

   HWND hwnd_seed;

   struct S_set: public C_unknown{
      C_str name;
      E_ALIGN_MODE align_mode;
      float random_scale;

      C_vector<C_str> model_names;

      bool random_rotation;

//----------------------------

      int FindModelByName(const C_str &name) const{
         for(int i=model_names.size();i--;)
            if(!stricmp(model_names[i], name))
               return i;
         return -1;
      }

//----------------------------

      bool AddModel(const char *modelfilename, bool sort = false){

         assert(*modelfilename);

         if(ModelNameInSet(modelfilename))
            return false;

         model_names.push_back(modelfilename);
         C_str &n = model_names.back();
         n.ToLower();
         bool make = true;
         for(dword i=0; i<n.Size(); i++){
            if(isalnum(n[i])){
               if(make){
                  n[i] = (char)toupper(n[i]);
                  make = false;
               }
            }else{
               make = true;
            }
         }
         if(sort)
            std::sort(model_names.begin(), model_names.end());
         return true;
      }

//----------------------------

      bool ModelNameInSet(const char *name) const{
         for(int i=model_names.size(); i--; )
            if(!stricmp(model_names[i], name))
               return true;
         return false;
      }

//----------------------------

      void DeleteModel(dword index){

         assert(index < model_names.size());
         //model_names[index] = model_names.back();
         //model_names.pop_back();
         model_names.erase(model_names.begin()+index);
      }

//----------------------------

      S_set(const C_str &n1):
         name(n1),
         random_rotation(true),
         random_scale(0.0f),
         align_mode(ALIGN_Y_UP)
      {
      }

      S_set(const S_set &ss);
      void operator =(const S_set&);
   };

   typedef S_set *PS_set;
   typedef const S_set *CPS_set;

   C_vector<C_smart_ptr<S_set> > sets;

   int selected_set;          //index of currently selected set, or -1 of no selected

   dword def_file_time[2];    //time of read definition file

//----------------------------

   static bool cbSetLess(CPS_set ss1, CPS_set ss2){
      return (stricmp(ss1->name, ss2->name) < 0);
   }

//----------------------------

   bool AddModel(CPI3D_model mod){

      assert(mod);
      if(selected_set == -1){
         ed->Message("Please select set to add model to.");
         return false;
      }
      const C_str &fname = mod->GetFileName();
      /*
      char buf[256];
      strcpy(buf, mod->GetFileName());
      ed->GetModelCache().GetRelativeDirectory(buf);
      */
      if(!sets[selected_set]->AddModel(fname, true)){
         ed->Message("This model already exists in set.");
         return false;
      }
      return true;
   }

//----------------------------

   PS_set AddSet(const C_str &name, bool sort){

      PS_set newset = new S_set(name);
      sets.push_back(newset);
      if(sort){
         std::sort(sets.begin(), sets.end(), cbSetLess);
         for(selected_set = sets.size(); selected_set--; ){
            if(newset==sets[selected_set])
               break;
         }
         assert(selected_set != -1);
      }
      newset->Release();
      return newset;
   }

//----------------------------

   BOOL dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

	   switch(uMsg){

      case WM_INITDIALOG:
         {
            static const char *align_mode_names[] = {
               "Y up",
               "Y to normal",
               "Z to normal",
            };
            for(dword i=0; i<ALIGN_LAST; i++){
               SendDlgItemMessage(hwnd, IDC_ALIGN_MODE, CB_ADDSTRING, 0, (LPARAM)align_mode_names[i]);
            }
         }
         break;

      case WM_HELP:
         {
            LPHELPINFO hi = (LPHELPINFO)lParam;
            DisplayHelp(hwnd, hi->iCtrlId, help_texts);
            return 1;
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){

         case IDC_SCALE:
            switch(HIWORD(wParam)){
            case EN_CHANGE:
               if(hwnd_seed){
                  if(!IsFileWritable()){
                     hwnd_seed = NULL;
                     SetDlgItemText(hwnd, IDC_SCALE, FloatStrip(C_fstr("%f", sets[selected_set]->random_scale)));
                     hwnd_seed = hwnd;
                     break;
                  }
                  char buf[128];
                  GetDlgItemText(hwnd, IDC_SCALE, buf, sizeof(buf));
                  float f;
                  if(sscanf(buf, "%f", &f)==1){
                     sets[selected_set]->random_scale = f;
                     SaveSets();
                  }
               }
               break;
            }
            break;

         case IDC_ALIGN_MODE:
            switch(HIWORD(wParam)){
            case CBN_SELCHANGE:
               if(hwnd_seed && selected_set != -1){
                  if(!IsFileWritable()){
                     SendDlgItemMessage(hwnd, IDC_ALIGN_MODE, CB_SETCURSEL, sets[selected_set]->align_mode, 0);
                     break;
                  }
                  dword sel_i = SendDlgItemMessage(hwnd, IDC_ALIGN_MODE, CB_GETCURSEL, 0, 0);
                  sets[selected_set]->align_mode = (E_ALIGN_MODE)sel_i;
                  SaveSets();
               }
               break;
            }
            break;

         case IDC_SEED_SET_NAME:
            {
               switch(HIWORD(wParam)){
               case CBN_SELCHANGE:
                  if(hwnd_seed){
                     selected_set = SendDlgItemMessage(hwnd, IDC_SEED_SET_NAME, CB_GETCURSEL, 0, 0);
                     SetupDialogParams();
                  }
                  break;
               }
            }
            break;

         case IDC_SEED_ROTATION:
            if(selected_set != -1){
               if(!IsFileWritable()){
                  SendDlgItemMessage(hwnd, IDC_SEED_ROTATION, BM_SETCHECK, sets[selected_set]->random_rotation ? BST_CHECKED : BST_UNCHECKED, 0);
                  break;
               }
               sets[selected_set]->random_rotation = !sets[selected_set]->random_rotation;
               SaveSets();
            }
            break;

         case IDC_SEED_CREATE_SET:
            {
               if(!IsFileWritable()) break;
               C_str name;
               while(true){
                  if(!SelectName(ed->GetIGraph(), NULL, "Enter name of new set", name))
                     return 0;
                              //check if name is unique
                  for(int i=sets.size(); i--; ){
                     if(sets[i]->name==name)
                        break;
                  }
                  if(i==-1)
                     break;
               }
               AddSet(name, true);
               SaveSets();
               SetupDialogParams();
            }
            break;

         case IDC_SEED_RENAME_SET:
            if(selected_set != -1){
               if(!IsFileWritable()) break;
               C_str name = sets[selected_set]->name;
               while(true){
                  if(!SelectName(ed->GetIGraph(), NULL, "Rename set", name))
                     return 0;
                              //check if name is unique
                  for(int i=sets.size(); i--; ){
                     if(sets[i]->name==name)
                        break;
                  }
                  if(i==-1)
                     break;
               }
               sets[selected_set]->name = name;
                              //re-sort
               sort(sets.begin(), sets.end(), cbSetLess);
               for(selected_set = sets.size(); selected_set--; ){
                  if(sets[selected_set]->name==name)
                     break;
               }
               SaveSets();
               SetupDialogParams();
               /*
               if(MessageBox(hwnd, "Are you sure to delete currently selected set?", "Delete set", MB_YESNO)==IDYES){
                  sets.erase(&sets[selected_set]);
                  selected_set = Min(selected_set, (int)sets.size() - 1);
                  SetupDialogParams();
                  SaveSets();
               }
               */
            }
            break;

         case IDC_SEED_DELETE_SET:
            if(selected_set != -1){
               if(!IsFileWritable()) break;
               if(MessageBox(hwnd, "Are you sure to delete currently selected set?", "Delete set", MB_YESNO)==IDYES){
                  //sets.erase(&sets[selected_set]);
                  sets.erase(sets.begin() + selected_set);
                  selected_set = Min(selected_set, (int)sets.size() - 1);
                  SetupDialogParams();
                  SaveSets();
               }
            }
            break;

            /*
         case IDC_SEED_PICK_MODEL:
            {
               S_MouseEdit_pick mousepick;
               mousepick.ei = this;
               mousepick.action_id = E_SEED_PICK_MODEL;
               mousepick.hcursor = LoadCursor(GetHInstance(), "IDC_SEED_PICKCURSOR");
               e_medit->Action(E_MOUSE_SET_USER_PICK, &mousepick);
            }
            break;
            */

         case IDC_SEED_ADD_MODELS:
            {
               if(!IsFileWritable()) break;
                              //add models from current selection
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               dword num_added = 0;
               for(dword i=sel_list.size(); i--; ){
                  CPI3D_frame frm = sel_list[i];
                  if(frm->GetType()!=FRAME_MODEL)
                     continue;
                  if(AddModel(I3DCAST_CMODEL(frm)))
                     ++num_added;
               }
               if(num_added){
                  SaveSets();
                  SetupDialogParams();
               }
               ed->Message(C_fstr("%i models added to the set from current selection", num_added));
            }
            break;

         case IDC_SEED_BROWSE_MODEL:
            {
               if(!IsFileWritable())
                  break;
               PC_editor_item_Create e_create = (PC_editor_item_Create)ed->FindPlugin("Create");
               C_str fname;
               if(e_create->BrowseModel(fname)){
                  if(sets[selected_set]->AddModel(fname, true)){
                     SaveSets();
                     SetupDialogParams();
                  }
               }
            }
            break;

         case IDC_SEED_PUT:
            {
               if(selected_set == -1){
                  ed->Message("Please select set to put from.");
                  break;
               }
               e_medit->SetUserPick(this, E_SEED_PUT_MODEL, LoadCursor(GetHInstance(), "IDC_SEED_PUTCURSOR"));
            }
            break;

         case IDC_SEED_DELETE_MODEL:
            {
               if(!IsFileWritable()) break;
               assert(selected_set != -1);
                              //delete all selected
               bool any = false;
               for(dword i=SendDlgItemMessage(hwnd, IDC_SEED_MODELS, LB_GETCOUNT, 0, 0); i--; ){
                  if(SendDlgItemMessage(hwnd, IDC_SEED_MODELS, LB_GETSEL, i, 0)){
                     sets[selected_set]->DeleteModel(i);
                     any = true;
                  }
               }
               if(any){
                  SaveSets();
                  SetupDialogParams();
               }
            }
            break;

         case IDCLOSE:
            {
               if(hwnd_seed)
                  Action(E_SEED_TOGGLE_DIALOG, NULL);
               return true;
            }
            break;
         }
         break;
	   }
      return 0;
   }

//----------------------------

   static BOOL CALLBACK dlgProc_Thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      C_edit_Seed *_this = (C_edit_Seed*)GetWindowLong(hwnd, GWL_USERDATA);
      if(_this)
         return _this->dlgProc(hwnd, uMsg, wParam, lParam);
      return 0;
   }

//----------------------------

   bool SetupDialogParams(){

                              //dialog must be present
      if(!hwnd_seed)
         return false;

      HWND hwnd = hwnd_seed;
                              //temporary clear the variable to mark that we're filling the contents
      hwnd_seed = NULL;

                              //clear set names
      SendDlgItemMessage(hwnd, IDC_SEED_SET_NAME, CB_RESETCONTENT, 0, 0);

                              //feed set names
      for(dword i = sets.size(); i--; ){
         SendDlgItemMessage(hwnd, IDC_SEED_SET_NAME, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)sets[i]->name);
      }
   
      SendDlgItemMessage(hwnd, IDC_SEED_SET_NAME, CB_SETCURSEL, selected_set, 0);

      if(selected_set != -1){
         CPS_set ss = sets[selected_set];

         SendDlgItemMessage(hwnd, IDC_SEED_ROTATION, BM_SETCHECK, ss->random_rotation ? BST_CHECKED : BST_UNCHECKED, 0);

                              //feed model names
         SendDlgItemMessage(hwnd, IDC_SEED_MODELS, LB_RESETCONTENT, 0, 0);
         for(dword j = 0; j<ss->model_names.size(); j++){
            int ii = SendDlgItemMessage(hwnd, IDC_SEED_MODELS, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)ss->model_names[j]);
            SendDlgItemMessage(hwnd, IDC_SEED_MODELS, LB_SETITEMDATA, ii, j);
         }
         SendDlgItemMessage(hwnd, IDC_ALIGN_MODE, CB_SETCURSEL, ss->align_mode, 0);
         SetDlgItemText(hwnd, IDC_SCALE, FloatStrip(C_fstr("%f", ss->random_scale)));
      }else{
         SendDlgItemMessage(hwnd, IDC_SEED_MODELS, LB_RESETCONTENT, 0, 0);
         SendDlgItemMessage(hwnd, IDC_SEED_ROTATION, BM_SETCHECK, BST_UNCHECKED, 0);
         SendDlgItemMessage(hwnd, IDC_ALIGN_MODE, CB_SETCURSEL, (WPARAM)-1, 0);
         SetDlgItemText(hwnd, IDC_SCALE, "");
      }
      hwnd_seed = hwnd;
      return true;
   }

//----------------------------

   int FindNearUpNameFromScene(char *buf, int start, int speed = 1) const{

      assert(buf);
      assert(start>=0);
      char *nbuf = new char [strlen(buf)+8];
      sprintf(nbuf,"%s%.2d", buf, start);

      if(!ed->GetScene()->FindFrame(nbuf)){
         if(start == 0)return start;
         sprintf(nbuf,"%s%.2d", buf, start-1);
         bool found = ed->GetScene()->FindFrame(nbuf);
         if(found) return start;
         return FindNearDownNameFromScene(buf, start-1);
      }
      return FindNearUpNameFromScene(buf, start+speed, speed*2);
   }

//----------------------------

   int FindNearDownNameFromScene(char *buf, int start, int speed = 1) const{

      assert(buf);
      assert(start>=0);

      char *nbuf = new char [strlen(buf)+8];
      sprintf(nbuf,"%s%.2d", buf, start);

      if(ed->GetScene()->FindFrame(nbuf)){
         sprintf(nbuf,"%s%.2d", buf, start+1);
         bool found = ed->GetScene()->FindFrame(nbuf);
         if(!found) return start+1;
         return FindNearUpNameFromScene(buf, start+1);
      }
      return FindNearDownNameFromScene(buf, start-speed, speed*2);
   }

//----------------------------

   void GetNewPosRot(E_ALIGN_MODE align_mode, PI3D_model model, const S_vector &from, const S_vector &pick_dir,
      bool rrot, float pick_dist, const S_vector &pick_norm, PI3D_frame pick_frm) const{

      S_vector dest_pos = from + pick_dir * pick_dist;
      model->SetPos(dest_pos);

      float roll = !rrot ? 0.0f : S_float_random() * PI * 2.0f;

      switch(align_mode){
      case ALIGN_Y_UP:
         {
            S_quat rot;
            rot.Make(S_vector(0, 1, 0), roll);
            model->SetRot(rot);
         }
         break;

      case ALIGN_Y_NORMAL:
         {
            S_matrix m;
            m.SetDir(-pick_norm, roll);
            model->SetDir1(m(1), -m(2));
         }
         break;

      case ALIGN_Z_NORMAL:
         {
            //S_matrix m;
            //m.SetDir(-pick_norm, roll);
            //model->SetDir1(m(1), -m(2));
            S_quat rot;
            rot.SetDir(-pick_norm, roll);
            model->SetRot(rot);
         }
         break;

      default: assert(0);
      }
      model->LinkTo(pick_frm, I3DLINK_UPDMATRIX);
   }

//----------------------------

   void PutModel(S_MouseEdit_picked *pick_info){

      if(!hwnd_seed || selected_set == -1){
         ed->Message("Please select active set.", 0, EM_WARNING);
         return;
      }

      dword num_models = sets[selected_set]->model_names.size();
      if(!num_models){
         ed->Message("Cannot choose model from empty set.", 0, EM_WARNING);
         return;
      }
                              //check if there's selection, if there is, make choose from this
      C_vector<dword> selected_models;
      for(dword i=SendDlgItemMessage(hwnd_seed, IDC_SEED_MODELS, LB_GETCOUNT, 0, 0); i--; ){
         if(SendDlgItemMessage(hwnd_seed, IDC_SEED_MODELS, LB_GETSEL, i, 0))
            selected_models.push_back(i);
      }

      dword model_index;

      if(selected_models.size())
         model_index = selected_models[S_int_random(selected_models.size())];
      else
         model_index = S_int_random(num_models);

      PS_set set = sets[selected_set];
                              //create and load model
      PI3D_model mod = I3DCAST_MODEL(ed->GetScene()->CreateFrame(FRAME_MODEL));

      const C_str &model_name = set->model_names[model_index];
      if(I3D_FAIL(ed->GetModelCache().Open(mod, model_name, ed->GetScene(), 0, NULL, NULL))){
         mod->Release();
         ed->Message(C_fstr("Can't open model: '%s'.", (const char*)model_name));
         if(IsFileWritable(false)){
            if(MessageBox((HWND)ed->GetIGraph()->GetHWND(),
               C_fstr("The model '%s' in set '%s' failed to load. Do you want to remove this model from the set?", (const char*)model_name, (const char*)set->name),
               "Model load fail", MB_YESNO)==IDYES){

               set->DeleteModel(model_index);
               SaveSets();
               SetupDialogParams();
            }
         }
         return;
      }
      if(mod->GetAnimationSet())
         mod->SetAnimation(0, mod->GetAnimationSet(), I3DANIMOP_LOOP);
      SetupVolumesStatic(mod, true);

                              //set modify flags, so we will be saved. 
                              //note: flags must be saved before any changes to frames are applied(because Modify plugging store original valus at this moment).
      ((PC_editor_item_Modify)ed->FindPlugin("Modify"))->AddFrameFlags(mod,
         E_MODIFY_FLG_CREATE | E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE | E_MODIFY_FLG_LINK);


                              //make random scale
      if(set->random_scale){
         float scale = 1.0f -set->random_scale + S_float_random(set->random_scale)*2.0f;
         mod->SetScale(scale);
      }

      GetNewPosRot(set->align_mode, mod, pick_info->pick_from, pick_info->pick_dir,
         sets[selected_set]->random_rotation,
         pick_info->pick_dist, pick_info->pick_norm, pick_info->frm_picked);

      char buf[256];
      for(i=model_name.Size(); i--; ){
         char c = model_name[i];
         if(c=='\\' || c=='+')
            break;
      }
      strcpy(buf, &model_name[i+1]);
      MakeSceneName(ed->GetScene(), buf);
      mod->SetName(buf);

      ed->GetScene()->SetFrameSector(mod);

      ed->SetModified();

      {                          //save undo
         PC_editor_item_Undo e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
         if(e_undo){
            e_undo->Begin(this, UNDO_DELETE, mod);
            e_undo->End();
         }
      }
                                                          
      e_slct->Clear();
      e_slct->AddFrame(mod);

      ed->GetScene()->AddFrame(mod);
      mod->Release();
   }

//----------------------------
// Check if seeds definition file is writable
   bool IsFileWritable(bool show_warning = true){

      if(!OsIsFileReadOnly(DEF_FILE_NAME)){
                              //check versions
         C_cache ck;
         if(!ck.open(DEF_FILE_NAME, CACHE_READ))
            return false;
         dword ft[2];
         ck.GetStream()->GetTime(ft);
         if(memcmp(ft, def_file_time, sizeof(ft))){
                              //reload the file
            LoadSets();
            selected_set = Min(selected_set, (int)sets.size() - 1);
            SetupDialogParams();
            ed->Message("Seeds definition file reloaded");
            return false;
         }
         return true;
      }
      if(show_warning){
         MessageBox((HWND)ed->GetIGraph()->GetHWND(),
            "The file 'Editor\\Seeds.txt' is not writtable. Please make sure you have access rights for this file.",
            "Cannot write to file", MB_OK);
      }
      return false;
   }

//----------------------------

   void LoadSets(){

      sets.clear();

      memset(def_file_time, 0, sizeof(def_file_time));

      C_cache ck;
      if(ck.open(DEF_FILE_NAME, CACHE_READ)){
         PS_set last_set = NULL;
         do{
            char line[1024], *cp = line;
            ck.getline(line, sizeof(line));
            if(line[0]==';' || !line[0])
               continue;
            char kw[64];
            int n;
            if(sscanf(cp, "%s%n", kw, &n)!=1)
               continue;
            cp += n+1;
            C_str str_kw = kw;
            char data[256];

            if(str_kw.Match("Set")){
                           //make new set
               sscanf(cp, "\"%[^\"]s\"", data);
               last_set = AddSet(data, false);
            }else
            if(str_kw.Match("Model")){
                           //add model to last set
               if(last_set){
                  sscanf(cp, "\"%[^\"]s\"", data);
                  last_set->AddModel(data);
               }
            }else
            if(str_kw.Match("random_rotation")){
                           //modify set flags
               if(last_set){
                  sscanf(cp, "%s", data);
                  last_set->random_rotation = atoi(data);
               }
            }else
            if(str_kw.Match("align")){
               if(last_set){
                  sscanf(cp, "%s", data);
                  last_set->align_mode = (E_ALIGN_MODE)atoi(data);
               }
            }else
            if(str_kw.Match("random_scale")){
               if(last_set){
                  float scale;
                  if(sscanf(cp, "%f", &scale)==1)
                     last_set->random_scale = scale;
               }
            }
         }while(!ck.eof());

         ck.GetStream()->GetTime(def_file_time);

         ck.close();
         sort(sets.begin(), sets.end(), cbSetLess);
      }
   }

//----------------------------

   void SaveSets(){

                              //write in text format
      C_cache ck;
      if(ck.open(DEF_FILE_NAME, CACHE_WRITE)){

         static const char title[] =
            "; This file was created by Insanity Editor.\r\n"
            "; Do not modify this file manualy!\r\n\r\n";
         ck.write(title, sizeof(title)-1);
         for(dword i=0; i<sets.size(); i++){
            const S_set &set = *sets[i];

            C_fstr set_name("Set \"%s\"\r\n", (const char*)set.name);
            ck.write((const char*)set_name, set_name.Size());
            C_fstr param("random_rotation %i\r\n", set.random_rotation);
            ck.write((const char*)param, param.Size());

            C_fstr align("align %i\r\n", set.align_mode);
            ck.write((const char*)align, align.Size());

            if(set.random_scale){
               C_fstr scale("random_scale %s\r\n", (const char*)FloatStrip(C_fstr("%f", set.random_scale)));
               ck.write((const char*)scale, scale.Size());
            }

            for(dword j=0; j<set.model_names.size(); j++){
               const C_str &mname = set.model_names[j];
               C_fstr model("Model \"%s\"\n", (const char*)mname);
               ck.write((const char*)model, model.Size());
            }
            ck.write("\r\n", 2);
         }

         ck.close();
         ck.open(DEF_FILE_NAME, CACHE_READ);
         ck.GetStream()->GetTime(def_file_time);
      }else{
         MessageBox((HWND)ed->GetIGraph()->GetHWND(),
            "The file 'Editor\\Seeds.txt' couldn't be opened for writing. Please make sure you have access rights for this file.",
            "Cannot write to file", MB_OK);
      }
   }

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      switch(id){
      case UNDO_DELETE:
         {
                              //let Create plugin do the work
            PC_editor_item_Create e_create = (PC_editor_item_Create)ed->FindPlugin("Create");
            if(e_create)
               e_create->DeleteFrame(frm);
         }
         break;
      }
   }

//----------------------------
public:
   C_edit_Seed():
      selected_set(-1),
      hwnd_seed(NULL)
   {
      memset(def_file_time, 0, sizeof(def_file_time));
   }

//----------------------------

   virtual bool Init(){

      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      if(!e_slct || !e_medit){
         return false;
      }

      ed->AddShortcut(this, E_SEED_TOGGLE_DIALOG, "&Create\\&Seed\tCtrl+E", K_E, SKEY_CTRL);
      return true;
   }

//----------------------------

   virtual void Close(){ 

      if(hwnd_seed){
         DestroyWindow(hwnd_seed);
         hwnd_seed = NULL;
      }
      sets.clear();
   }

//----------------------------

   //virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){};

//----------------------------

   dword Action(int id, void *context){

      switch(id){

         /*
      case E_SEED_PICK_MODEL:
         {
            S_MouseEdit_picked *picked = (S_MouseEdit_picked *)context;
            I3D_frame *frm = picked->frm_picked;
            frm = TrySelectTopModel(frm, true);
            if(!frm || frm->GetType()!=FRAME_MODEL){
               ed->Message("Please click on model.");
               return true;
            }
            if(AddModel(I3DCAST_MODEL(frm))){
               SaveSets();
               SetupDialogParams();
            }
            return true;
         }
         break;
         */

      case E_SEED_PUT_MODEL:
         {
            if(!ed->CanModify()) break;
            S_MouseEdit_picked *picked = (S_MouseEdit_picked *)context;
            if(picked->frm_picked)
               PutModel(picked);
            return true;
         }
         break;

      case E_SEED_TOGGLE_DIALOG:
         {
            PC_editor_item_Properties e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
            if(hwnd_seed){
               e_props->RemoveSheet(hwnd_seed);
               DestroyWindow(hwnd_seed);
               hwnd_seed = NULL;
            }else{
               hwnd_seed = CreateDialogParam(GetHInstance(), "IDD_SEED", NULL, dlgProc_Thunk, (LPARAM)this);
               if(hwnd_seed){
                  e_props->AddSheet(hwnd_seed, true);
                  SetupDialogParams();
               }
            }
         }
         return true;
      }
      return false;
   }

   //virtual void Render(){}

//----------------------------

   virtual bool LoadState(C_chunk &ck){

      LoadSets();

      dword version;
      version = (dword)-1;
      ck.Read(&version, sizeof(version));
      if(version != SEED_VERSION)
         return false;

      ck.Read(&selected_set, sizeof(selected_set));

      selected_set = Min(selected_set, (int)sets.size() - 1);
      SetupDialogParams();
      return true; 
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      dword version = SEED_VERSION;

      ck.Write(&version, sizeof(version));

      ck.Write(&selected_set, sizeof(selected_set));

      return true; 
   }
};

//----------------------------

void CreateSeed(PC_editor editor){
   C_edit_Seed *sd = new C_edit_Seed; editor->InstallPlugin(sd); sd->Release();    
}

//----------------------------