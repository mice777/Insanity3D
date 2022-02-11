#include "all.h"
#include "common.h"
#include "Properties.h"

//----------------------------

class C_edit_Create: public C_editor_item_Create{
   virtual const char *GetName() const{ return "Create"; }

//----------------------------

   enum E_ACTION_CREATE{         
      E_CREATE_MODEL = 4000,     // - create model and initialize it by loading from a file
      E_CREATE_LIGHT,            
      E_CREATE_SOUND,             
      E_CREATE_VOLUME,            
      E_CREATE_CAMERA,            
      E_CREATE_DUMMY,
      E_CREATE_DUMMY_FRAMED,
      E_CREATE_TARGET,
      E_CREATE_OCCLUDER,
      E_CREATE_SECTOR,
      E_CREATE_VISUAL,

      E_CREATE_DUPLICATE_SELECTION, // - duplicate all duplicate-able frames from current selection
      E_CREATE_DUPLICATE_REPEAT, 
      E_CREATE_DELETE_SELECTION, // - delete all delete-able frames from current selection
      //E_CREATE_DELETE_FRAME,     //(PI3D_frame) - delete specified frame
      //E_CREATE_FRAME_NAMED,      // - internal (undo) - re-create frame
      //E_CREATE_CHILD_NAMED,      // - internal (undo) - re-create child of model
      //E_CREATE_DELETE_NAMED,     //(const char *frm_name) - internal (undo)
      //E_CREATE_PICK_OCCL_VERTEX, // - user-pick mode
      //E_CREATE_VERT_SELECTION,   // - undo info for occluder creation
      //E_CREATE_DEL_VERT_SEL,     // - undo info for occluder creation
      E_CREATE_RELOAD_TEXTURE,   //reload texture under cursor
      E_CREATE_RELOAD_MODEL,     //reload model under cursor

      E_CREATE_RELOAD_TEXTURE_PICK,
      E_CREATE_RELOAD_MODEL_PICK,
   };

   enum{
      UNDO_CREATE,            //recreate deleted frame
      UNDO_DESTROY,
   };

//----------------------------
   C_str last_model_path;
   C_str last_mission_path;
   C_str last_sound_path;
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Selection> e_slct;
   bool single_volume;
   bool create_static_vol;

   C_smart_ptr<C_toolbar> toolbar;

//----------------------------

   struct S_edit_Modify_undo_volume{
      int num_frames;
      struct S_frm{
         I3D_VOLUMETYPE vtype;
         int material_id;
         char frm_name[1];      
      } frm_info[1];
   };

//----------------------------

   /*
   struct S_edit_Create_1{
      S_vector pos;
      S_quat rot;
      float scale;
      S_vector nu_scale;
      dword frm_type;
      dword modify_flags;
      char name_buf[1];          //[frame_name, link_name, add_data] - separated by '\0'
   };
   */

//----------------------------

   void SaveDeleteUndo(PI3D_frame frm){

                              //store to undo class
      e_undo->Begin(this, UNDO_DESTROY, frm);
      e_undo->End();
   }

//----------------------------

   struct S_dlg_name{
      PIGraph igraph;
      const char *frm_name;
   };

   static BOOL CALLBACK dlgName(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            S_dlg_name *hlp = (S_dlg_name*)lParam;
            InitDlg(hlp->igraph, hwnd);
            SetDlgItemText(hwnd, IDC_EDIT, hlp->frm_name);
            ShowWindow(hwnd, SW_SHOW);
            SetWindowLong(hwnd, GWL_USERDATA, lParam);
         }
         return 1;
      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL: EndDialog(hwnd, 0); break;
         case IDOK:
            {
               S_dlg_name *hlp = (S_dlg_name*)GetWindowLong(hwnd, GWL_USERDATA);
               SendDlgItemMessage(hwnd, IDC_EDIT, WM_GETTEXT, 256, (LPARAM)hlp->frm_name);
               EndDialog(hwnd, 1);
            }
            break;
         }
         break;
      }
      return 0;
   }

//----------------------------

   dword dlgColType(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      int i;

      switch(uMsg){
      case WM_INITDIALOG:
         {
            InitDlg(ed->GetIGraph(), hwnd);
            CheckDlgButton(hwnd, IDC_RADIO_COLBOX, true);
                              //get selection and determine if we're going
                              // to offer user uncheck single volume
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            EnableWindow(GetDlgItem(hwnd, IDC_CHECK_VOL_SINGLE), (sel_list.size()>1));
            bool check = (sel_list.size()>1) ? single_volume : true;
            CheckDlgButton(hwnd, IDC_CHECK_VOL_SINGLE, check ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_VOL_STATIC, create_static_vol ? BST_CHECKED : BST_UNCHECKED);

            ShowWindow(hwnd, SW_SHOW);
         }
         return 1;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDOK:
            {
               static const int check_id[] = {
                  IDC_RADIO_COLBOX, IDC_RADIO_COLSHP, IDC_RADIO_COLRECT, IDC_RADIO_COLCCYL, IDC_RADIO_COLCYL
               };
               i = sizeof(check_id)/4;
               while(i--)
               if(IsDlgButtonChecked(hwnd, check_id[i])){
                  EndDialog(hwnd, check_id[i]);
                  break;
               }
            }
            break;

         case IDC_CHECK_VOL_SINGLE:
            single_volume = IsDlgButtonChecked(hwnd, LOWORD(wParam));
            break;

         case IDC_CHECK_VOL_STATIC:
            create_static_vol = IsDlgButtonChecked(hwnd, LOWORD(wParam));
            break;

         case IDCANCEL:
            EndDialog(hwnd, -1);
            break;
         }
         break;
      }
      return 0;
   }

//----------------------------

   static BOOL CALLBACK dlgColType_thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      }
      C_edit_Create *ec = (C_edit_Create*)GetWindowLong(hwnd, GWL_USERDATA);
      if(ec)
         return ec->dlgColType(hwnd, uMsg, wParam, lParam);
      return 0;
   }

//----------------------------

   static BOOL CALLBACK dlgOccType(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
      int i;

      switch(uMsg){
      case WM_INITDIALOG:
         {
            C_edit_Create *ec = (C_edit_Create*)lParam;
            //SetWindowLong(hwnd, GWL_USERDATA, lParam);

            InitDlg(ec->ed->GetIGraph(), hwnd);
            CheckDlgButton(hwnd, IDC_RADIO_OCCMESH, true);
            ShowWindow(hwnd, SW_SHOW);
         }
         return 1;
      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDOK:
            {
               static const int check_id[] = {
                  IDC_RADIO_OCCMESH, IDC_RADIO_OCCSHP
               };
               for(i = sizeof(check_id)/4; i--; )
               if(IsDlgButtonChecked(hwnd, check_id[i])){
                  EndDialog(hwnd, i);
                  break;
               }
            }
            break;
         case IDCANCEL: EndDialog(hwnd, -1); break;
         }
         break;
      }
      return 0;
   }

//----------------------------
                              //vars for repeat duplicate
   S_vector last_dup_pos;
   C_str last_dup_frm;

//----------------------------

   bool Duplicate(PI3D_frame frm, S_vector *new_pos = NULL){

      PI3D_frame new_frm = NULL;
      dword sub_type = 0;
      switch(frm->GetType()){
      case FRAME_VISUAL:
         //sub_type = I3DCAST_VISUAL(frm)->GetVisualType();
         //break;
         return false;
      }

      PI3D_scene scene = ed->GetScene();
      new_frm = scene->CreateFrame(frm->GetType(), sub_type);
      if(new_frm){
                        //prompt name
         int i;
         char buf[256];

         strcpy(buf, frm->GetName());

                              //try to get new name
         MakeSceneName(scene, buf);
         if(!new_pos){        //don't ask name if repeated duplication
            for(;;){
               S_dlg_name hlp = {ed->GetIGraph(), buf};
               i = DialogBoxParam(GetHInstance(), "GET_MODELNAME", (HWND)ed->GetIGraph()->GetHWND(), dlgName, (LPARAM)&hlp);
               if(!i) break;
                              //check name duplication
               if(!IsNameInScene(scene, buf))
                  break;
            }
         }else
            i = true;
         if(!i){
            new_frm->Release();
            new_frm = NULL;
         }else{
            /*
            I3D_RESULT ir =
               new_frm->Duplicate(frm);
            if(I3D_FAIL(ir)){
               new_frm->Release();
               new_frm = NULL;
            }
            */
            {
               dword modify_flags = E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION |
                  E_MODIFY_FLG_LINK | E_MODIFY_FLG_CREATE;

               if(frm->GetScale()!=1.0f)
                  modify_flags |= E_MODIFY_FLG_SCALE;

               switch(frm->GetType()){
               case FRAME_VOLUME:
                  {
                     PI3D_volume vol1 = I3DCAST_VOLUME(frm);
                     if(vol1->GetNUScale()!=S_vector(1, 1, 1))
                        modify_flags |= E_MODIFY_FLG_NU_SCALE;
                     modify_flags |= E_MODIFY_FLG_VOLUME_TYPE | E_MODIFY_FLG_COL_MAT;
                  }
                  break;
               case FRAME_LIGHT: modify_flags |= E_MODIFY_FLG_LIGHT; break;
               case FRAME_SOUND: modify_flags |= E_MODIFY_FLG_SOUND; break;
               case FRAME_VISUAL: modify_flags |= E_MODIFY_FLG_VISUAL | E_MODIFY_FLG_COL_MAT; break;
               }
               if(frm->GetFlags())
                  modify_flags |= E_MODIFY_FLG_FRM_FLAGS;

               e_modify->AddFrameFlags(new_frm, modify_flags);

               if(frm->GetType()==FRAME_MODEL){
                              //for models, open plain model first
                  C_I3D_cache_base<I3D_model> &model_cache = ed->GetModelCache();
                  const C_str &fn = I3DCAST_CMODEL(frm)->GetFileName();
                  model_cache.Open(I3DCAST_MODEL(new_frm), fn, ed->GetScene(), 0, OPEN_ERR_LOG(ed, fn));
               }else
                  new_frm->Duplicate(frm);

               {
                           //can't duplicate user value
                  struct S_hlp{
                     static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword){
                        frm->SetData(0);
                        return I3DENUMRET_OK;
                     }
                  };
                  S_hlp::cbEnum(new_frm, 0);
                  new_frm->EnumFrames(S_hlp::cbEnum, 0);
               }

               switch(frm->GetType()){
               case FRAME_MODEL:
                  {
                                       //set same flags on duplicated children than those of original
                     struct S_hlp{
                        PI3D_model mod;
                        C_editor_item_Modify *e_modify;
                        static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

                           S_hlp *hp = (S_hlp*)c;
                           PI3D_frame f_orig = hp->mod->FindChildFrame(frm->GetOrigName());
                           assert(f_orig);
                           dword flags = hp->e_modify->GetFrameFlags(f_orig);
                           if(flags)
                              hp->e_modify->AddFrameFlags(frm, flags);
                                       //finish model duplication - dup frame params now
                           frm->Duplicate(f_orig);
                           return I3DENUMRET_OK;
                        }
                     } hlp = {I3DCAST_MODEL(frm), e_modify};
                     new_frm->EnumFrames(S_hlp::cbEnum, (dword)&hlp);

                     new_frm->SetPos(frm->GetPos());
                     new_frm->SetRot(frm->GetRot());
                     new_frm->SetScale(frm->GetScale());
                  }
                  break;
               }

               new_frm->SetName(buf);
                              //
               SaveDeleteUndo(new_frm);

               new_frm->LinkTo(frm->GetParent());
               if(new_pos){
                  if(frm->GetParent()){
                     S_matrix m = frm->GetParent()->GetMatrix();
                     m.Invert();
                     *new_pos = *new_pos * m;
                  }
                  new_frm->SetPos(*new_pos);
               }

                              //add this frame to selection
               e_slct->AddFrame(new_frm);

               scene->AddFrame(new_frm);
               ed->SetModified();

                              //broadcast duplicate action
               ed->Broadcast((void(C_editor_item::*)(void*,void*))&C_editor_item::OnFrameDuplicate, frm, new_frm);
            }
         }
      }
      if(!new_frm){
         ed->Message("Duplication failed", 0, EM_WARNING);
         return false;
      }else{
         last_dup_pos = frm->GetWorldPos();
         last_dup_frm = new_frm->GetName();
         new_frm->Release();
         return true;
      }
   }

//----------------------------

   bool Browse(I3D_FRAME_TYPE ft, C_str &fname){

      fname = NULL;

      C_str *last_path = NULL;
      const char *title = NULL;
      const char *extensions = NULL;
      const char *def_ext = NULL;

      OPENFILENAME on;
      memset(&on, 0, sizeof(on));
      on.lStructSize = sizeof(on);

      struct S_hlp{
         PIGraph igraph;
         PI3D_driver driver;
         PI3D_scene ed_scene;
         C_edit_Create *ec;
         PC_editor_item_Mission e_mission;
         char buf[MAX_PATH];
         bool mission_model;
         const char *last_mission_path;
         C_str cwd;
         bool in;       //re-entering block

         enum E_mode{
            MODE_MODEL, MODE_SOUND
         } mode;
                        //MODE_MODEL:
         PI3D_scene scene;
         PI3D_camera cam;
         PI3D_model mod;
         C_I3D_cache_base<I3D_model> *cache_model;
         bool timer_on;
         float cam_orbit_angle;

                        //MODE_SOUND:
         C_I3D_cache_base<I3D_sound> *sound_cache;
         PI3D_sound snd;
         PISND_sound play_snd;

         S_hlp():
            timer_on(false),
            in(false),
            mission_model(false),
            e_mission(NULL)
         {}

         void Render(HWND hwnd, bool clear = false){
            if(!clear){
               if(mod){
                  S_quat q;
                  q.Make(S_vector(0, 1, 0), cam_orbit_angle);
                  mod->SetRot(q);
                  //mod->Update();
               }
               scene->Render(0);
            }
            hwnd = GetDlgItem(hwnd, IDC_PREVIEW);

            if(clear){
               igraph->ClearViewport();
            }
            igraph->UpdateScreen(0, hwnd);
         }

         static void cbLoadErr(const char *msg, void *context){
            C_str &err = *(C_str*)context;
            err += msg;
            err += "\r\n";
         }

         static UINT CALLBACK cbOpen(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
            switch(uMsg){
            case WM_INITDIALOG:
               {
                  OPENFILENAME *ofn = (OPENFILENAME*)lParam;
                  S_hlp *hp = (S_hlp*)ofn->lCustData;
                  SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)hp);
                  InitDlg(hp->igraph, hwnd);

                  switch(hp->mode){
                  case MODE_MODEL:
                     {
                        HWND hwnd_prv = GetDlgItem(hwnd, IDC_PREVIEW);
                        RECT rc;
                        GetClientRect(hwnd_prv, &rc);
                        //hp->scene->SetViewport(0, 0, rc.right-rc.left-4, rc.bottom-rc.top-4);

                        CheckDlgButton(hwnd, IDC_CHECK_PRV_SHOW, hp->ec->mod_show_prv ? BST_CHECKED : BST_UNCHECKED);
                        CheckDlgButton(hwnd, IDC_CHECK_PRV_ANIMATE, hp->ec->mod_anim_prv ? BST_CHECKED : BST_UNCHECKED);

                        if(!hp->ec->mod_show_prv) EnableWindow(GetDlgItem(hwnd, IDC_CHECK_PRV_ANIMATE), false);
                        if(!hp->e_mission)
                           ShowWindow(GetDlgItem(hwnd, IDC_MODEL_MISSION), SW_HIDE);
                     }
                     break;
                  case MODE_SOUND:
                     {
                        CheckDlgButton(hwnd, IDC_CHECK_SND_PRV_SHOW, hp->ec->snd_show_prv ? BST_CHECKED : BST_UNCHECKED);
                        CheckDlgButton(hwnd, IDC_CHECK_SND_PRV_LOOP, hp->ec->snd_loop_prv ? BST_CHECKED : BST_UNCHECKED);
                        if(!hp->ec->snd_show_prv) EnableWindow(GetDlgItem(hwnd, IDC_CHECK_SND_PRV_LOOP), false);
                     }
                     break;
                  }
                  return 1;
               }
               break;

            case WM_COMMAND:
               switch(LOWORD(wParam)){
               case IDC_CHECK_PRV_SHOW:
               case IDC_CHECK_PRV_ANIMATE:
               case IDC_CHECK_SND_PRV_SHOW:
               case IDC_CHECK_SND_PRV_LOOP:
                  {
                     S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                     switch(LOWORD(wParam)){
                     case IDC_CHECK_PRV_SHOW:
                        hp->ec->mod_show_prv = IsDlgButtonChecked(hwnd, LOWORD(wParam));
                        if(!hp->ec->mod_show_prv){
                           if(hp->timer_on){
                              KillTimer(hwnd, 1);
                              hp->timer_on = false;
                           }
                           hp->Render(hwnd, true);
                        }
                        EnableWindow(GetDlgItem(hwnd, IDC_CHECK_PRV_ANIMATE), hp->ec->mod_show_prv);
                        break;
                     case IDC_CHECK_PRV_ANIMATE:
                        hp->ec->mod_anim_prv = IsDlgButtonChecked(hwnd, LOWORD(wParam));
                        if(hp->ec->mod_anim_prv){
                           if(!hp->timer_on){
                              hp->timer_on = SetTimer(hwnd, 1, 50, NULL);
                           }
                        }else{
                           if(hp->timer_on){
                              KillTimer(hwnd, 1);
                              hp->timer_on = false;
                              hp->cam_orbit_angle = 0.0f;
                              hp->Render(hwnd);
                           }
                        }
                        break;
                     case IDC_CHECK_SND_PRV_SHOW:
                        hp->ec->snd_show_prv = IsDlgButtonChecked(hwnd, LOWORD(wParam));
                        if(!hp->ec->snd_show_prv){
                           if(hp->play_snd) hp->play_snd->Stop();
                        }
                        EnableWindow(GetDlgItem(hwnd, IDC_CHECK_SND_PRV_LOOP), hp->ec->snd_show_prv);
                        break;
                     case IDC_CHECK_SND_PRV_LOOP:
                        hp->ec->snd_loop_prv = IsDlgButtonChecked(hwnd, LOWORD(wParam));
                        if(hp->play_snd){
                           PISND_source src = hp->snd->GetSoundSource();
                           hp->play_snd->PlaySource(src, ISND_NOVOLUME | 
                              (hp->ec->snd_loop_prv ? ISND_LOOP : 0));
                        }
                        break;
                     }
                  }
                  break;

               case IDC_MODEL_MISSION:
                  {
                     S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                     //strcpy(hp->buf, hp->last_mission_path);
                     C_str mission_name = hp->last_mission_path;
                     if(hp->e_mission->BrowseMission(hwnd, mission_name)){
                        strcpy(hp->buf, mission_name);
                        hp->mission_model = true;
                        //PostMessage(GetParent(hwnd), WM_COMMAND, MAKELPARAM(IDABORT, BN_CLICKED), (LPARAM)GetDlgItem(GetParent(hwnd), IDABORT));
                        EndDialog(GetParent(hwnd), true);
                     }
                  }
                  break;
               }
               break;

            case WM_NOTIFY:
               {
                  OFNOTIFY *on = (OFNOTIFY*)lParam;
                  switch(on->hdr.code){
                  case CDN_SELCHANGE:
                     {
                        S_hlp *hp = (S_hlp*)on->lpOFN->lCustData;
                        switch(hp->mode){
                        case MODE_MODEL:
                           {
                              if(!hp->ec->mod_show_prv || hp->in) break;
                              hp->in = true;

                              char buf[MAX_PATH];
                              SendMessage(GetParent(hwnd), CDM_GETFILEPATH, MAX_PATH, (LPARAM)&buf);
                              hp->cache_model->GetRelativeDirectory(buf);
                              int len = strlen(buf);
                              if(len>=4 && !stricmp(&buf[len-4], ".i3d"))
                                 buf[len-4] = 0;
                                             //try to open the file and render scene
                              {
                                 if(hp->mod){
                                    hp->mod->LinkTo(NULL);
                                    hp->mod->Release();
                                    hp->mod = NULL;
                                 }
                                 hp->Render(hwnd, true);
                                 hp->mod = I3DCAST_MODEL(hp->scene->CreateFrame(FRAME_MODEL));
                                 hp->mod->LinkTo(hp->scene->GetPrimarySector());
                                 _chdir(hp->cwd);
                                 SetCursor(LoadCursor(NULL, IDC_WAIT));
                                 C_str errors;
                                 I3D_RESULT ir = hp->cache_model->Open(hp->mod, buf, hp->ed_scene, 0,
                                    S_hlp::cbLoadErr, &errors);
                                 if(errors.Size() && ir!=I3DERR_NOFILE){
                                    SetDlgItemText(hwnd, IDC_ERRORS, errors);
                                    ShowWindow(GetDlgItem(hwnd, IDC_ERRORS), SW_SHOW);
                                    ShowWindow(GetDlgItem(hwnd, IDC_ERR_TITLE), SW_SHOW);
                                    hp->cache_model->EraseItem(buf);
                                 }else{
                                    ShowWindow(GetDlgItem(hwnd, IDC_ERRORS), SW_HIDE);
                                    ShowWindow(GetDlgItem(hwnd, IDC_ERR_TITLE), SW_HIDE);
                                 }

                                 SetCursor(LoadCursor(NULL, IDC_ARROW));
                                 if(I3D_SUCCESS(ir)){
                                    if(hp->mod->GetAnimationSet())
                                       hp->mod->SetAnimation(0, hp->mod->GetAnimationSet(), I3DANIMOP_LOOP);

                                    I3D_bound_volume bvol;
                                    hp->scene->GetPrimarySector()->ComputeHRBoundVolume(&bvol);
                                    const I3D_bbox &bbox = bvol.bbox;
                                    float cam_dist;
                                    S_vector cam_look_at;
                                    if(bbox.IsValid()){
                                       S_vector diag = bbox.max - bbox.min;
                                       cam_look_at = bbox.min + diag*.5f;
                                       cam_dist = diag.Magnitude() * 1.2f;
                                       cam_dist = Max(cam_dist, 2.0f);
                                    }else{
                                       cam_look_at.Zero();
                                       cam_dist = 0.0f;
                                    }
                              
                                    S_vector pos = cam_look_at + S_vector(.8f, .4f, -.8f) * cam_dist;
                                    hp->cam->SetPos(pos);
                                    S_vector dir = cam_look_at - pos;
                                    if(!dir.IsNull())
                                       hp->cam->SetDir(dir);

                                    hp->cam_orbit_angle = 0.0f;

                                    hp->Render(hwnd);

                                    if(hp->ec->mod_anim_prv && !hp->timer_on){
                                       hp->timer_on = SetTimer(hwnd, 1, 50, NULL);
                                    }
                                 }else{
                                    if(hp->timer_on){
                                       KillTimer(hwnd, 1);
                                       hp->timer_on = false;
                                    }
                                 }
                              }
                              hp->in = false;
                           }
                           break;
                        case MODE_SOUND:
                           {
                              if(hp->in) break;
                              hp->in = true;

                              char buf[MAX_PATH];
                              //SendMessage(GetParent(hwnd), CDM_GETSPEC, MAX_PATH, (LPARAM)&buf);
                              SendMessage(GetParent(hwnd), CDM_GETFILEPATH, MAX_PATH, (LPARAM)&buf);
                              hp->sound_cache->GetRelativeDirectory(buf);
                                                //strip extension
                              for(int i=strlen(buf); i--; ){
                                 if(buf[i]=='.'){
                                    buf[i] = 0;
                                    break;
                                 }
                              }

                              if(hp->snd){
                                 hp->snd->LinkTo(NULL);
                                 hp->snd->Release();
                                 hp->snd = NULL;
                              }
                              hp->snd = I3DCAST_SOUND(hp->ed_scene->CreateFrame(FRAME_SOUND));
                              C_str str;

                              _chdir(hp->cwd);
                              I3D_RESULT ir = hp->sound_cache->Open(hp->snd, buf, hp->ed_scene);
                              if(I3D_SUCCESS(ir)){
                                 PISND_source src = hp->snd->GetSoundSource();
                                 if(src){
                                    const S_wave_format &fmt = *src->GetFormat();

                                    str = C_fstr(
                                       "Channels: %i\nBits: %i\nFrequency: %i Hz\nLength: %.2f s",
                                       fmt.num_channels,
                                       fmt.bytes_per_sample*8,
                                       fmt.samples_per_sec,
                                       (float)fmt.size / (fmt.samples_per_sec*fmt.bytes_per_sample));
                                    if(hp->ec->snd_show_prv && hp->play_snd){
                                       hp->play_snd->PlaySource(src, ISND_NOVOLUME | 
                                          (hp->ec->snd_loop_prv ? ISND_LOOP : 0));
                                    }
                                 }
                              }
                              if(!str.Size()){
                                 if(hp->play_snd) hp->play_snd->Stop();
                              }
                              SetDlgItemText(hwnd, IDC_SOUND_DETAILS, str);

                              hp->in = false;
                           }
                           break;
                        }
                     }
                     break;

                  case CDN_FILEOK:
                     {
                        S_hlp *hp = (S_hlp*)on->lpOFN->lCustData;
                        if(hp->mode==MODE_MODEL)      //hide our rendering
                           hp->ed_scene->Render(0);
                     }
                     break;
                  }
               }
               break;

            case WM_TIMER:
               {
                  S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                  if(hp->timer_on && !hp->in){
                     hp->in = true;
                     hp->cam_orbit_angle -= PI*.025f;
                     hp->mod->Tick(50);
                     hp->Render(hwnd);
                     hp->in = false;
                  }
               }
               break;
            }
            return 0;
         }
      };

      S_hlp hlp;
      hlp.igraph = ed->GetIGraph();
      hlp.ed_scene = ed->GetScene();
      hlp.ec = this;
      hlp.buf[0] = 0;
      hlp.last_mission_path = last_mission_path;
      {
         char buf[MAX_PATH];
         _getcwd(buf, sizeof(buf));
         hlp.cwd = buf;
      }

      switch(ft){
      case FRAME_MODEL:
         last_path = &last_model_path; 
         if(!last_model_path.Size())
            //last_model_path = ed->GetModelCache().NumDirs() ? ed->GetModelCache().GetDir(0) : "";
            last_model_path = ed->GetModelCache().GetDir();
         title = "Open model";
         extensions =
            "Insanity 3D files (*.i3d)\0*.i3d\0"
            "All files\0*.*\0";

         hlp.mode = S_hlp::MODE_MODEL;
         {              //init rendering
            hlp.cache_model = &ed->GetModelCache();
            hlp.mod = NULL;

            PI3D_driver drv = ed->GetDriver();
            hlp.driver = drv;
            hlp.scene = drv->CreateScene();
            hlp.scene->SetBgndColor(S_vector(.2f, .2f, .4f));
            //hlp.scene->SetViewport(0, 0, 128, 128);

            hlp.cam = I3DCAST_CAMERA(ed->GetScene()->CreateFrame(FRAME_CAMERA));
            hlp.cam->SetFOV(PI*.4f);
            hlp.scene->AddFrame(hlp.cam);
            hlp.scene->SetActiveCamera(hlp.cam);
            hlp.cam->Release();

            PI3D_light lp = I3DCAST_LIGHT(ed->GetScene()->CreateFrame(FRAME_LIGHT));
            lp->SetLightType(I3DLIGHT_AMBIENT);
            lp->SetColor(S_vector(1, 1, 1));
            lp->SetPower(.4f);
            hlp.scene->AddFrame(lp);
            hlp.scene->GetPrimarySector()->AddLight(lp);
            lp->Release();

            lp = I3DCAST_LIGHT(ed->GetScene()->CreateFrame(FRAME_LIGHT));
            lp->SetLightType(I3DLIGHT_DIRECTIONAL);
            lp->SetColor(S_vector(1, 1, 1));
            lp->SetPower(2.0f);
            lp->SetDir(S_vector(1, -1, 1));
            //lp->Update();
            hlp.scene->AddFrame(lp);
            hlp.scene->GetPrimarySector()->AddLight(lp);
            lp->Release();
         }
         on.lpTemplateName = "OPEN_MODEL";
         hlp.e_mission = (PC_editor_item_Mission)ed->FindPlugin("Mission");
         break;

      case FRAME_SOUND: 
         last_path = &last_sound_path; 
         if(!last_sound_path.Size())
            //last_sound_path = ed->GetSoundCache().NumDirs() ? ed->GetSoundCache().GetDir(0) : "";
            last_sound_path = ed->GetSoundCache().GetDir();
         title = "Open sound";
         extensions = "Wave files (*.wav, *.ogg)\0*.wav;*.ogg\0""All files\0*.*\0";
         //def_ext = "wav";

         {
            PI3D_driver drv = ed->GetDriver();
            hlp.driver = drv;
            hlp.mode = S_hlp::MODE_SOUND;
            hlp.sound_cache = &ed->GetSoundCache();
            hlp.snd = NULL;
            hlp.play_snd = NULL;
            PISND_driver isound = drv->GetSoundInterface();
            if(isound) isound->CreateSound(&hlp.play_snd);
         }
         on.lpTemplateName = "OPEN_SOUND";
         ed->GetScene()->AttenuateSounds();
         break;
      default: assert(0);
      }

      on.hwndOwner = (HWND)ed->GetIGraph()->GetHWND();
      on.lpstrFilter = extensions;
      on.nFilterIndex = 1;
      on.lpstrFile = hlp.buf;
      on.nMaxFile = 256;
      on.lpstrInitialDir = *last_path;
      on.lpstrTitle = title;
      on.Flags |= OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK | OFN_EXPLORER;
      on.lpstrDefExt = def_ext;

      on.lpfnHook = S_hlp::cbOpen;
      on.hInstance = GetHInstance();
      on.lCustData = (LPARAM)&hlp;

      bool b = GetOpenFileName(&on);

      switch(hlp.mode){
      case S_hlp::MODE_MODEL:
         hlp.scene->Release();
         if(hlp.mod) hlp.mod->Release();
         break;
      case S_hlp::MODE_SOUND:
         if(hlp.snd) hlp.snd->Release();
         if(hlp.play_snd) hlp.play_snd->Release();
         break;
      }
      if(b){
         if(!hlp.mission_model){
            (*last_path) = hlp.buf;
            for(dword i=last_path->Size(); i--; )
               if((*last_path)[i]=='\\')
                  break;
            (*last_path)[i+1] = 0;

            switch(ft){
            case FRAME_MODEL:
               if(!ed->GetModelCache().GetRelativeDirectory(hlp.buf))
                  b = false;
               break;
            case FRAME_SOUND:
               if(!ed->GetSoundCache().GetRelativeDirectory(hlp.buf))
                  b = false;
               break;
            }
         }else{
            last_mission_path = hlp.buf;
            C_fstr tmp("+%s", hlp.buf);
            strcpy(hlp.buf, tmp);
         }
         if(b && !hlp.mission_model){
                     //strip extension
            for(int i=strlen(hlp.buf); i--; ){
               if(hlp.buf[i]=='.'){
                  hlp.buf[i] = 0;
                  break;
               }
            }
         }
      }
      if(b)
         fname = hlp.buf;
      return b;
   }

//----------------------------

   virtual bool BrowseModel(C_str &filename){
      return Browse(FRAME_MODEL, filename);
   }

//----------------------------

   virtual bool BrowseSound(C_str &filename){
      return Browse(FRAME_SOUND, filename);
   }

//----------------------------

   virtual bool DeleteFrame(PI3D_frame frm){

                              //keep one ref of deletef frame
      C_smart_ptr<I3D_frame> keep_ref = frm;
                              //broadcast delete action
      ed->Broadcast((void(C_editor_item::*)(void*))&C_editor_item::OnFrameDelete, frm);

                              //check if this frame was explicitly created
      dword modify_flags = e_modify->GetFrameFlags(frm);
      //bool model_child = false;
      if(!(modify_flags&E_MODIFY_FLG_CREATE)){
#if 1
         return false;
#else
                              //we're able to delete child of model (forever)
         PI3D_frame prnt = frm;
         while((prnt=prnt->GetParent(), prnt) && prnt->GetType()!=FRAME_MODEL);
                              //if no parent model, quit
         if(!prnt) return false;
                              //if parent model has no parent (i.e. was just deleted), quit
         if(!prnt->GetParent()) return false;
         PI3D_model mod = I3DCAST_MODEL(prnt);
         const PI3D_frame *frmp = mod->GetFrames();
         for(int i=mod->NumFrames(); i--; )
            if(frmp[i]==frm) break;
         if(i==-1) return false;

                              //ask user if it's ok to delete this frame
         int st = MessageBox((HWND)ed->GetIGraph()->GetHWND(), C_fstr("Object %s is part of model.\nDo you really want to delete it?",
            frm->GetName()), "", MB_YESNOCANCEL);
         if(st!=IDYES)
            return false;

         model_child = true;

                              //add to list of deleted frames
         e_modify->Action(E_MODIFY_ADD_MODEL_DEL_CHILD,
            &pair<PI3D_frame, const char*>(mod, frm->GetOrigName()));
#endif
      }

      {
                              //perform action on specialized frames to un-link from
                              // world
                              //this must be done after frame has saved its undo data
         struct S_hlp{
            C_editor_item_Modify *e_modify;
            PC_editor_item_Selection e_slct;
            static I3DENUMRET I3DAPI cbDelFrame(PI3D_frame frm, dword c){

               S_hlp *hp = (S_hlp*)c;
               hp->e_modify->RemoveFrame(frm);
               switch(frm->GetType()){
               case FRAME_LIGHT:
                  {
                     PI3D_light lp = I3DCAST_LIGHT(frm);
                     while(lp->NumLightSectors())
                        lp->GetLightSectors()[0]->RemoveLight(lp);
                  }
                  break;

               case FRAME_SOUND:
                  {
                     PI3D_sound sp = I3DCAST_SOUND(frm);
                     while(sp->NumSoundSectors())
                        sp->GetSoundSectors()[0]->RemoveSound(sp);
                  }
                  break;
               }
                              //remove child of deleted item from selection
               hp->e_slct->RemoveFrame(frm);
               return I3DENUMRET_OK;
            }
         } hlp;
         hlp.e_modify = e_modify;
         hlp.e_slct = e_slct;
         //S_hlp::cbDelFrame(frm, (dword)&hlp);
         frm->EnumFrames(S_hlp::cbDelFrame, (dword)&hlp, ENUMF_ALL, NULL);

         //e_modify->RemoveFlags(frm, modify_flags);
                              //remove 'create' flags, so that frame is successfully reset
         e_modify->RemoveFlags(frm, E_MODIFY_FLG_CREATE);
                              //reset flags (this saves undo for modified values)
         e_modify->RemoveFlagsAndReset(frm, modify_flags);
                              //remove from selection
         e_slct->RemoveFrame(frm);

         C_chunk &ck = e_undo->Begin(this, UNDO_CREATE, frm);
         ck.Write(modify_flags);
         if(frm->GetParent())
            ck.Write(frm->GetParent()->GetName());
         else
            ck.Write("");

         switch(frm->GetType()){
         case FRAME_LIGHT:
            {
               PI3D_light lp = I3DCAST_LIGHT(frm);
                              //store names of sectors
               PI3D_sector const *spp = lp->GetLightSectors();
               for(dword i=lp->NumLightSectors(); i--; )
                  ck.Write(spp[i]->GetName());
               ck.Write("");
                              //remove from light sectors
               while(lp->NumLightSectors())
                  lp->GetLightSectors()[0]->RemoveLight(lp);
            }
            break;

         case FRAME_SOUND:
            {
               PI3D_sound sp = I3DCAST_SOUND(frm);
                              //store names of sectors
               PI3D_sector const *spp = sp->GetSoundSectors();
               for(dword i=sp->NumSoundSectors(); i--; )
                  ck.Write(spp[i]->GetName());
               ck.Write("");

               while(sp->NumSoundSectors())
                  sp->GetSoundSectors()[0]->RemoveSound(sp);

               sp->SetOn(false);
            }
            break;

         }
         e_undo->End();
      }
                              //remove references from scene
      frm->LinkTo(NULL);
      ed->GetScene()->RemoveFrame(frm);
      ed->SetModified();

      return true;
   }

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      switch(id){
      case UNDO_CREATE:
         {
            if(!ed->CanModify()) break;
            PI3D_scene scn = ed->GetScene();

            dword modify_flags = ck.ReadDword();
            PI3D_frame link_to = scn->GetPrimarySector();
            C_str pname = ck.ReadString();
            if(pname.Size()){
               link_to = scn->FindFrame(pname);
               assert(link_to);
            }
            frm->LinkTo(link_to);

            switch(frm->GetType()){
            case FRAME_LIGHT:
               {
                  PI3D_light lp = I3DCAST_LIGHT(frm);
                              //init light sectors
                  while(true){
                     C_str n = ck.ReadString();
                     if(!n.Size())
                        break;
                     PI3D_sector sct = I3DCAST_SECTOR(scn->FindFrame(n, ENUMF_SECTOR));
                     if(sct)
                        sct->AddLight(lp);
                  }
               }
               break;

            case FRAME_SOUND:
               {
                  PI3D_sound sp = I3DCAST_SOUND(frm);
                              //init sound sectors
                  while(true){
                     C_str n = ck.ReadString();
                     if(!n.Size())
                        break;
                     PI3D_sector sct = I3DCAST_SECTOR(scn->FindFrame(n, ENUMF_SECTOR));
                     if(sct)
                        sct->AddSound(sp);
                  }
               }
               break;
               /*
            case FRAME_DUMMY:
               {
                  PI3D_dummy dum;
                  frm = dum = I3DCAST_DUMMY(scn->CreateFrame(FRAME_DUMMY));
                  dum->SetBBox(*(PI3D_bbox)add_data);
               }
               break;

            case FRAME_VOLUME:
               {
                  PI3D_volume vol;
                  frm = vol = I3DCAST_VOLUME(scn->CreateFrame(FRAME_VOLUME));
                  const S_edit_Modify_undo_volume::S_frm &fi = *(S_edit_Modify_undo_volume::S_frm*)add_data;
                  vol->SetVolumeType(fi.vtype);
               }
               break;

            case FRAME_OCCLUDER:
               {
                  PI3D_occluder occ;
                  frm = scn->CreateFrame(FRAME_OCCLUDER);
                  occ = I3DCAST_OCCLUDER(frm);
                  dword numv = ((dword*)add_data)[0];
                  I3D_OCCLUDERTYPE ot = (I3D_OCCLUDERTYPE)((dword*)add_data)[1];
                  occ->SetOccluderType(ot);
                  S_vector *vp = (S_vector*)((byte*)add_data + 2*sizeof(dword));
                  occ->Build(vp, numv);
               }
               break;

            case FRAME_CAMERA:
               {
                  frm = scn->CreateFrame(nm.frm_type);
               }
               break;

            case FRAME_VISUAL:
               {
                  assert(id==E_CREATE_CHILD_NAMED);
                  assert(link_to);
                              //find parent model (which used to be our parent)
                  for(PI3D_frame prnt = link_to; prnt && prnt->GetType()!=FRAME_MODEL; prnt=prnt->GetParent());
                  assert(prnt);
                  if(!prnt) break;
                  PI3D_model mod = I3DCAST_MODEL(prnt);
                              //re-load original model
                  PI3D_model orig = I3DCAST_MODEL(scn->CreateFrame(FRAME_MODEL));
                  const C_str &fname = mod->GetFileName();
                  //ed->GetModelCache().GetRelativeDirectory(&fname[0]);
                  ed->GetModelCache().Open(orig, fname, 0, 0);
                  for(int i=strlen(nm.name_buf); i && nm.name_buf[i-1]!='.'; i--);
                  PI3D_frame frm_orig = orig->FindChildFrame(nm.name_buf+i);
                  assert(frm_orig && frm_orig->GetType()==FRAME_VISUAL);
                  if(frm_orig && frm_orig->GetType()==FRAME_VISUAL){
                     frm = scn->CreateFrame(FRAME_VISUAL,
                        I3DCAST_VISUAL(frm_orig)->GetVisualType());
                     assert(frm);
                     if(frm)
                        frm->Duplicate(frm_orig);
                  }
                  orig->Release();
               }
               break;
               */
            }
            /*
                              //make frame's name
            char buf[256];
            strcpy(buf, nm.name_buf);
            MakeSceneName(scn, buf, false);
            frm->SetName(buf);

                           //put to specified location
            frm->SetPos(nm.pos);
            frm->SetRot(nm.rot);

                           //link to specified frame
                           //if failed to locate link frame - link to primary
            if(!link_to) link_to = scn->GetPrimarySector();
            frm->LinkTo(link_to);
            */

                              //add to modify
            e_modify->AddFrameFlags(frm, modify_flags);

            /*
            if(nm.modify_flags&E_MODIFY_FLG_SCALE)
               frm->SetScale(nm.scale);
            if(nm.modify_flags&E_MODIFY_FLG_NU_SCALE){
               switch(frm->GetType()){
               //case FRAME_VISUAL: I3DCAST_VISUAL(frm)->SetNUScale(nm.nu_scale); break;
               case FRAME_VOLUME: I3DCAST_VOLUME(frm)->SetNUScale(nm.nu_scale); break;
               }
            }
            */
            e_slct->FlashFrame(frm);

            /*
            if(id==E_CREATE_CHILD_NAMED){
               for(PI3D_frame prnt = link_to; prnt && prnt->GetType()!=FRAME_MODEL; prnt=prnt->GetParent());
               assert(prnt);
               I3DCAST_MODEL(prnt)->AddFrame(frm);
            }else{
               scn->AddFrame(frm);
            }
            */
            scn->AddFrame(frm);

            ed->SetModified();
            SaveDeleteUndo(frm);

            //ei->AddFrame(frm);

            ed->Message(C_fstr("Create: %s", (const char*)frm->GetName()));
            //frm->Release();
         }
         break;

      case UNDO_DESTROY:
         DeleteFrame(frm);
         break;
      }
   }

//----------------------------

public:
   bool mod_show_prv, mod_anim_prv, snd_show_prv, snd_loop_prv;

   C_edit_Create():
      single_volume(false),
      create_static_vol(true),
      mod_show_prv(true), mod_anim_prv(true), snd_show_prv(true), snd_loop_prv(true),
      e_undo(NULL),
      e_modify(NULL),
      e_slct(NULL)
   {}

//----------------------------

   virtual bool Init(){
                              //find required plugins
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      if(!e_undo || !e_slct || !e_modify){
         e_undo = NULL;
         e_slct = NULL;
         e_modify = NULL;
         return false;
      }

#define MB "%25 &Create\\%0 3D object\\"
      ed->AddShortcut(this, E_CREATE_MODEL, MB"%&Model\tShift+M", K_M, SKEY_SHIFT);
      ed->AddShortcut(this, E_CREATE_LIGHT, MB"&Light\tShift+L", K_L, SKEY_SHIFT);
      ed->AddShortcut(this, E_CREATE_SOUND, MB"&Sound\tShift+S", K_S, SKEY_SHIFT);
      ed->AddShortcut(this, E_CREATE_VOLUME, MB"&Volume\tShift+V", K_V, SKEY_SHIFT);
      ed->AddShortcut(this, E_CREATE_CAMERA, MB"&Camera\tShift+C", K_C, SKEY_SHIFT);
      ed->AddShortcut(this, E_CREATE_DUMMY, MB"&Dummy\tShift+D", K_D, SKEY_SHIFT);
      ed->AddShortcut(this, E_CREATE_DUMMY_FRAMED, MB"&Dummy framed\tCtrl+Y", K_Y, SKEY_CTRL);
      ed->AddShortcut(this, E_CREATE_OCCLUDER, MB"&Occluder\tShift+O", K_O, SKEY_SHIFT);
      //ed->AddShortcut(this, E_CREATE_VISUAL, MB"Visual", K_NOKEY, 0);

#undef MB
#define MB "%25 &Create\\"
      ed->AddShortcut(this, E_CREATE_DUPLICATE_SELECTION, MB"%80 %i &Duplicate\tIns", K_INS, 0);
      ed->AddShortcut(this, E_CREATE_DUPLICATE_REPEAT, MB"%81Repeat duplicate\tShift+Ins", K_INS, SKEY_SHIFT);
      ed->AddShortcut(this, E_CREATE_DELETE_SELECTION, MB"%81 &Delete\tDel", K_DEL, 0);
      ed->AddShortcut(this, E_CREATE_RELOAD_TEXTURE, MB"%82 %i Reload\\&Texture\tCtrl+R", K_R, SKEY_CTRL);
      ed->AddShortcut(this, E_CREATE_RELOAD_MODEL, MB"%82 %i Reload\\&Model\tShift+R", K_R, SKEY_SHIFT);

                              //initialize toolbar
      toolbar = ed->GetToolbar("Create");
      {
         S_toolbar_button tbs[] = {
            {E_CREATE_MODEL,  0, "Create model"},
            {E_CREATE_LIGHT,  1, "Create light"},
            {E_CREATE_SOUND,  2, "Create sound"},
            {E_CREATE_VOLUME, 3, "Create volume"},
            {E_CREATE_CAMERA, 4, "Create camera"},
            {E_CREATE_DUMMY,  5, "Create dummy"},
            {E_CREATE_OCCLUDER, 6, "Create occluder"},
            {0, -1},
         };
         toolbar->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_CREATE", GetHInstance(), 1);
      }

      return true;
   }

//----------------------------

   virtual void Close(){
      if(e_slct)
         e_slct->RemoveNotify(this);

      e_undo = NULL;
      e_slct = NULL;
      e_modify = NULL;

      //CancelOcclPickMode(false);
   }

//----------------------------

   virtual dword Action(int id, void *context){    

      switch(id){

      case E_CREATE_MODEL:
      case E_CREATE_LIGHT:
      case E_CREATE_SOUND:
      case E_CREATE_CAMERA:
      case E_CREATE_VOLUME:
      case E_CREATE_DUMMY:
      case E_CREATE_DUMMY_FRAMED:
      case E_CREATE_OCCLUDER:
      case E_CREATE_VISUAL:
         {
            if(!ed->CanModify())
               break;
            PI3D_frame frm = NULL;
            char frm_name[256];
            PI3D_scene scene = ed->GetScene();
            bool setup_pos = true;
            dword modify_flags = E_MODIFY_FLG_POSITION | E_MODIFY_FLG_CREATE;

            S_vector pos;
            S_quat rot;
            float scl = 1.0f;
            S_vector nu_scl;
            PI3D_frame link = NULL;

                              //create and initialize frame
            switch(id){
            case E_CREATE_MODEL:
               {
                  C_str file_name;
                  if(BrowseModel(file_name)){
                                    //create and load model
                     PI3D_model model;
                     model = I3DCAST_MODEL(scene->CreateFrame(FRAME_MODEL));
                     I3D_RESULT ir = ed->GetModelCache().Open(model, file_name,
                        ed->GetScene(), 0, OPEN_ERR_LOG(ed, file_name));
                     if(I3D_SUCCESS(ir)){
                        frm = model;
                        if(model->GetAnimationSet())
                           model->SetAnimation(0, model->GetAnimationSet(), I3DANIMOP_LOOP);
                        //SetupVolumesStatic(model, true);
                        SetupVolumesOwner(model, model);

                              //create suggested frame name
                        for(dword i=file_name.Size(); i--; )
                           if(file_name[i]=='\\')
                              break;
                        strcpy(frm_name, &file_name[i+1]);
                     }else{
                                    //load failed
                        model->Release();
                        ed->Message("Load model failed", 0, EM_WARNING);
                     }
                  }
               }
               break;

            case E_CREATE_VISUAL:
               {
                              //let user select sub-type
                  int id = ChooseItemFromList(ed->GetIGraph(), NULL, "Select visual type", "Particle\0Flare\0");
                  if(id==-1)
                     break;
                  dword sub_type = 0;
                  const char *fn = NULL;
                  switch(id){
                  case 0: sub_type = I3D_VISUAL_PARTICLE; fn = "particle"; break;
                  case 1: sub_type = I3D_VISUAL_FLARE; fn = "flare"; break;
                  default: assert(0);
                  }
                  strcpy(frm_name, fn);
                  PI3D_visual vis = I3DCAST_VISUAL(scene->CreateFrame(FRAME_VISUAL, sub_type));
                  frm = vis;

                  PI3D_material mat = ed->GetDriver()->CreateMaterial();
                  vis->SetMaterial(mat);
                  mat->Release();
               }
               break;

            case E_CREATE_SOUND:
               {
                  C_str file_name;
                  if(BrowseSound(file_name)){
                     PI3D_sound snd = I3DCAST_SOUND(ed->GetScene()->CreateFrame(FRAME_SOUND));
                     I3D_RESULT ir = ed->GetSoundCache().Open(snd, file_name, ed->GetScene(), 0,
                        OPEN_ERR_LOG(ed, file_name));
                     if(I3D_SUCCESS(ir)){
                        snd->SetSoundType(I3DSOUND_POINT);
                        snd->SetLoop(true);
                        frm = snd;
                              //create suggested frame name
                        for(dword i=file_name.Size(); i--; ) if(file_name[i]=='\\') break;
                        strcpy(frm_name, &file_name[i+1]);
                        for(i=strlen(frm_name); i--; ) if(frm_name[i]=='.') break;
                        if(i!=-1) frm_name[i] = 0;
                     }else{
                                    //load failed
                        snd->Release();
                        ed->Message("Load sound failed", 0, EM_WARNING);
                     }
                  }
               }
               break;

            case E_CREATE_LIGHT:
               {
                  strcpy(frm_name, "light");
                  PI3D_light lp = I3DCAST_LIGHT(scene->CreateFrame(FRAME_LIGHT));
                  lp->SetLightType(I3DLIGHT_POINT);
                  frm = lp;
               }
               break;

            case E_CREATE_CAMERA:
               {
                  strcpy(frm_name, "camera");
                  frm = scene->CreateFrame(FRAME_CAMERA);
               }
               break;

            case E_CREATE_VOLUME:
               {
                  struct S_hlp{
                     C_vector<PI3D_frame> sel_frm;
                     int ctrl_type;
                  } *hlp = (S_hlp*)context;
                  const C_vector<PI3D_frame> &sel_list =
                     hlp ? hlp->sel_frm :
                     e_slct->GetCurSel();;
                  if(!sel_list.size()){
                     ed->Message("Selection required.", 0, EM_WARNING);
                     break;
                  }
                  int vol_ctrl_id;
                  if(!hlp){
                     vol_ctrl_id = DialogBoxParam(GetHInstance(), "CREATE_GET_COLTYPE", (HWND)ed->GetIGraph()->GetHWND(),
                        dlgColType_thunk, (LPARAM)this);
                     if(vol_ctrl_id==-1)
                        break;
                  }else{
                     vol_ctrl_id = hlp->ctrl_type;
                  }
                  if(sel_list.size()>1 && !single_volume){
                     C_vector<PI3D_frame> sl = sel_list;
                     e_slct->Clear();

                     S_hlp hlp;
                     hlp.ctrl_type = vol_ctrl_id;
                     for(dword i=0; i<sl.size(); i++){
                        hlp.sel_frm.clear();
                        hlp.sel_frm.push_back(sl[i]);
                        Action(id, &hlp);
                     }
                     break;
                  }

                  I3D_bbox bbox;
                  bbox.Invalidate();
                  PI3D_frame sel0 = sel_list[0];
                  S_matrix mat0 = sel0->GetMatrix();
                  mat0.Invert();
                  for(dword i=0; i<sel_list.size(); i++){
                     PI3D_frame sf = sel_list[i];
                     I3D_bbox bbox1;
                     switch(sf->GetType()){
                     case FRAME_VISUAL:
                        bbox1 = I3DCAST_VISUAL(sf)->GetBoundVolume().bbox;
                        break;
                     default: 
                        {
                           I3D_bound_volume bvol;
                           sf->ComputeHRBoundVolume(&bvol);
                           bbox1 = bvol.bbox;
                        }
                     }
                     if(i){
                        if(bbox1.IsValid()){
                           S_matrix m = sf->GetMatrix() * mat0;
                           for(int j=0; j<8; j++){
                              S_vector v =
                                 S_vector(bbox1[j&1].x, bbox1[(j&2)/2].y, bbox1[(j&4)/4].z);
                              v = v * m;
                              bbox.min.Minimal(v);
                              bbox.max.Maximal(v);
                           }
                        }
                     }else
                        bbox = bbox1;
                  }
                  if(!bbox.IsValid()){                              
                     bbox.min = S_vector(-1, -1, -1);
                     bbox.max = S_vector(1, 1, 1);
                  }

                  I3D_VOLUMETYPE vt = I3DVOLUME_NULL;
                  PI3D_volume vol = I3DCAST_VOLUME(scene->CreateFrame(FRAME_VOLUME));
                  if(create_static_vol)
                     vol->SetFlags(I3D_FRMF_STATIC_COLLISION, I3D_FRMF_STATIC_COLLISION);

                  S_vector diag = bbox.max - bbox.min;
                  pos = bbox.min + diag*.5f;
                  switch(vol_ctrl_id){
                  case IDC_RADIO_COLBOX:
                     {
                        vt = I3DVOLUME_BOX;

                        nu_scl = S_vector(Max(.001f, (bbox.max.x-bbox.min.x)/2.0f),
                           Max(.001f, (bbox.max.y-bbox.min.y)/2.0f),
                           Max(.001f, (bbox.max.z-bbox.min.z)/2.0f));
                        modify_flags |= E_MODIFY_FLG_NU_SCALE;
                     }
                     break;

                  case IDC_RADIO_COLSHP: 
                     {
                        vt = I3DVOLUME_SPHERE;
                        scl = diag.Magnitude() * .5f;
                        modify_flags |= E_MODIFY_FLG_SCALE;
                     }
                     break;

                  case IDC_RADIO_COLRECT:
                     {
                        vt = I3DVOLUME_RECTANGLE;

                        nu_scl = S_vector(Max(.001f, (bbox.max.x-bbox.min.x)/2.0f),
                           Max(.001f, (bbox.max.y-bbox.min.y)/2.0f),
                           Max(.001f, (bbox.max.z-bbox.min.z)/2.0f));
                        if(nu_scl.x<nu_scl.z || nu_scl.y<nu_scl.z){
                           if(nu_scl.x<nu_scl.y){
                                             //x's smallest
                                             //turn PI/2 by y and swap x with z
                              nu_scl.x = nu_scl.z;
                              rot.Make(S_vector(0, 1, 0), PI*.5f);
                           }else{
                                             //y's smallest
                                             //turn PI/2 by x and swap y with z
                              nu_scl.y = nu_scl.z;
                              rot.Make(S_vector(1, 0, 0), PI*.5f);
                           }
                           modify_flags |= E_MODIFY_FLG_ROTATION;
                        }
                        nu_scl.z = 1.0f;
                        modify_flags |= E_MODIFY_FLG_NU_SCALE;
                     }
                     break;

                  case IDC_RADIO_COLCCYL:
                  case IDC_RADIO_COLCYL:
                     {
                        vt = I3DVOLUME_CAPCYL;
                        nu_scl = (bbox.max-bbox.min) * .5f;
                        nu_scl.Maximal(S_vector(.001f, .001f, .001f));
                        if(nu_scl.x > nu_scl.z){
                           swap(nu_scl.x, nu_scl.z);
                           modify_flags |= E_MODIFY_FLG_ROTATION;
                           rot.Make(S_vector(0, 1, 0), PI*.5f);
                        }
                        if(nu_scl.y > nu_scl.z){
                           swap(nu_scl.y, nu_scl.z);
                           modify_flags |= E_MODIFY_FLG_ROTATION;
                           rot.Make(S_vector(1, 0, 0), PI*.5f);
                        }
                        float r = (nu_scl.x+nu_scl.y) * .5f;
                        nu_scl.x = nu_scl.y = r;
                        if(vol_ctrl_id==IDC_RADIO_COLCCYL)
                           nu_scl.z = Max(0.0f, nu_scl.z - r*.707f);
                        modify_flags |= E_MODIFY_FLG_NU_SCALE;
                     }
                     break;

                  default: assert(0);
                  }
                  vol->SetVolumeType(vt);
                  link = sel0;
                  modify_flags |= E_MODIFY_FLG_VOLUME_TYPE | E_MODIFY_FLG_LINK;

                  while(sel0 && sel0->GetType()!=FRAME_MODEL) sel0 = sel0->GetParent();
                  vol->SetOwner(sel0);
                  //vol->SetHierarchical(true);

                  setup_pos = false;

                  strcpy(frm_name, "volume");
                  frm = vol;
               }
               break;

            case E_CREATE_DUMMY:
            case E_CREATE_DUMMY_FRAMED:
               {
                  strcpy(frm_name, "dummy");
                  PI3D_dummy dum;
                  frm = dum = I3DCAST_DUMMY(scene->CreateFrame(FRAME_DUMMY));
                  //dum->SetBBox(I3D_bbox(S_vector(-.5f, -.5f, -.5f), S_vector(.5f, .5f, .5f)));
                  if(id==E_CREATE_DUMMY_FRAMED){
                     PI3D_frame sel_frm = e_slct->GetSingleSel();
                     if(!sel_frm){
                        frm->Release();
                        frm=NULL;
                        break;
                     }else{
                        frm->LinkTo(sel_frm->GetParent());
                        frm->SetPos(sel_frm->GetPos());
                        frm->SetRot(sel_frm->GetRot());
                        frm->SetScale(sel_frm->GetScale());
                        setup_pos = false;
                     }

                  }

               }
               break;

            case E_CREATE_OCCLUDER:
               {
                  int i;
                  C_vector<S_vector> v_pool;
                  I3D_OCCLUDERTYPE ot;

                  /*
                              //if we're already in pick mode, skip type dialog
                  PC_editor_item e_medit = ed->FindPlugin("MouseEdit");
                  if(e_medit && e_medit->Action(E_MOUSE_GET_USER_PICK)==(dword)this){
                              //collect selected vertices into single pool
                     for(i=occl_pick_verts.size(); i--; ){
                        const S_occl_pick_vertex &opv = occl_pick_verts[i];
                        PI3D_mesh_base mb = opv.vis->GetMesh();
                        const S_vector *vp = mb->LockVertices();
                        const S_vector &v = *(const S_vector*)(((byte*)vp) + opv.vertex_index * mb->GetSizeOfVertex());
                        const S_matrix &m = opv.vis->GetMatrix();
                        v_pool.push_back(v * m);
                        mb->UnlockVertices();
                     }
                     CancelOcclPickMode();
                     ot = I3DOCCLUDER_MESH;
                  }else*/
                  {
                                 //let user select the type of occluder
                     i = DialogBoxParam(GetHInstance(), "CREATE_GET_OCCTYPE", (HWND)ed->GetIGraph()->GetHWND(),
                        dlgOccType, (LPARAM)this);
                     if(i==-1)
                        break;
                     ot = (I3D_OCCLUDERTYPE)i;

                     switch(ot){
                     case I3DOCCLUDER_MESH:
                        {
                              //collect all vertices of current selection, in world coordinates
                           const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                           for(i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()!=FRAME_VISUAL)
                                 continue;
                              PI3D_visual vis = I3DCAST_VISUAL(frm);
                              const S_matrix &mat = vis->GetMatrix();

                              PI3D_mesh_base mb = vis->GetMesh();
                              if(mb){
                                 const S_vector *vp = mb->LockVertices();
                                 dword vs = mb->GetSizeOfVertex();
                                 for(dword i=0; i<mb->NumVertices(); i++, (byte*&)vp += vs){
                                    v_pool.push_back((*vp) * mat);
                                 }
                                 mb->UnlockVertices();
                              }
                           }
                        }
                        break;
                     }
                  }

                  bool ok = true;

                  switch(ot){
                  case I3DOCCLUDER_MESH:
                     if(v_pool.size()){
                              //get geometric center of points
                        I3D_bbox bb;
                        bb.Invalidate();
                        for(i=v_pool.size(); i--; ){
                           bb.min.Minimal(v_pool[i]);
                           bb.max.Maximal(v_pool[i]);
                        }
                        pos = bb.min + (bb.max-bb.min) * .5f;
                        for(i=v_pool.size(); i--; )
                           v_pool[i] -= pos;

                              //optimize pool
                              //- remove vertices too close to each other
                        float MIN_DIST = (bb.max - bb.min).Magnitude() * .05f;
                        MIN_DIST *= MIN_DIST;
                        for(dword i=v_pool.size(); i--; ){
                           S_vector &v0 = v_pool[i];
                           for(int j=i; j--; ){
                              S_vector &v1 = v_pool[j];
                              float scl = (v0 - v1).Square();
                              if(scl < MIN_DIST){
                                       //retail point further away from center
                                 if(v1.Square() < v0.Square())
                                    v1 = v0;
                                 v0 = v_pool.back();
                                 v_pool.pop_back();
                                 break;
                              }
                           }
                        }
                        if(v_pool.size() >= 4)
                           setup_pos = false;
                        else
                           v_pool.clear();
                     }
                     if(!v_pool.size()){
                        /*
                              //setup vertex picking mode
                        S_MouseEdit_pick mep = {
                           this, E_CREATE_PICK_OCCL_VERTEX, LoadCursor(GetHInstance(), "IDC_MAKE_OCCLUDER")
                        };
                        ed->FindPlugin("MouseEdit")->Action(E_MOUSE_SET_USER_PICK, &mep);
                        ed->RegForRender(this, 1000);
                        ok = false;
                        */
                        v_pool.push_back(S_vector(-1, 0, -1));
                        v_pool.push_back(S_vector( 1, 0, -1));
                        v_pool.push_back(S_vector( 1, 0,  1));
                        v_pool.push_back(S_vector(-1, 0,  1));
                        v_pool.push_back(S_vector( 0, 1,  0));
                        setup_pos = true;
                     }
                     break;
                  case I3DOCCLUDER_SPHERE:
                     {
                        const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

                        I3D_bbox bbox;
                        bbox.Invalidate();
                        for(dword i=0; i<sel_list.size(); i++){
                           PI3D_frame sf = sel_list[i];
                           I3D_bbox bbox1;
                           switch(sf->GetType()){
                           case FRAME_VISUAL:
                              bbox1 = I3DCAST_VISUAL(sf)->GetBoundVolume().bbox;
                              break;
                           default: 
                              {
                                 I3D_bound_volume bvol;
                                 sf->ComputeHRBoundVolume(&bvol);
                                 bbox1 = bvol.bbox;
                              }
                           }
                           if(bbox1.IsValid()){
                              const S_matrix &m = sf->GetMatrix();
                              for(int j=0; j<8; j++){
                                 S_vector v =
                                    S_vector(bbox1[j&1].x, bbox1[(j&2)/2].y, bbox1[(j&4)/4].z);
                                 v = v * m;
                                 bbox.min.Minimal(v);
                                 bbox.max.Maximal(v);
                              }
                           }
                        }
                        if(bbox.IsValid()){
                           S_vector diag = bbox.max - bbox.min;

                           scl = diag.Magnitude() * .5f;
                           modify_flags |= E_MODIFY_FLG_SCALE;

                           pos = bbox.min + diag*.5f;
                           setup_pos = false;
                        }
                     }
                     break;
                  }

                  if(ok){
                              //create occluder
                     strcpy(frm_name, "occluder");
                     frm = scene->CreateFrame(FRAME_OCCLUDER);
                     PI3D_occluder occ = I3DCAST_OCCLUDER(frm);

                     occ->SetOccluderType(ot);

                     switch(ot){
                     case I3DOCCLUDER_MESH:
                        {
                                    //build occluder
                           I3D_RESULT ir;
                           ir = occ->Build(&v_pool.front(), v_pool.size());
                           /*
                           if(I3D_SUCCESS(ir)){
                                    //optimize redundant vertices
                              bool rebuild = false;

                              dword num_verts = occ->NumVertices();
                              const S_vector *verts = occ->LockVertices();

                              const dword num_faces = occ->NumFaces();
                              const I3D_face *faces = occ->GetFaces();
                              C_vector<C_vector<S_vector> > vertex_face_normals(num_verts);
                              C_vector<S_vector> face_normals(num_faces);
                              for(dword fi=num_faces; fi--; ){
                                 const I3D_face &fc = faces[fi];
                                 S_vector &n = face_normals[fi];
                                 n.GetNormal(verts[fc.points[0]], verts[fc.points[1]], verts[fc.points[2]]);
                                 n.Normalize();
                              }
                              for(fi=num_faces; fi--; ){
                                 const I3D_face &fc = faces[fi];
                                 for(dword fci=fc.num_points; fci--; ){
                                    word vi = fc.points[fci];
                                    C_vector<S_vector> &v_normals = vertex_face_normals[vi];
                                    v_normals.push_back(face_normals[fi]);
                                 }
                              }
                              occ->UnlockVertices();
                                       //go through vertices, remove ones which have minimal normal angles
                              const float ANGLE_THRESH = PI*.1f;
                              for(dword vi=num_verts; vi--; ){
                                 C_vector<S_vector> &v_normals = vertex_face_normals[vi];
                                 float max_angle = 0.0f;
                                 for(dword ni=v_normals.size(); ni--; ){
                                    const S_vector &n0 = v_normals[ni];
                                    for(dword ni1=ni; ni1--; ){
                                       const S_vector &n1 = v_normals[ni1];
                                       float a = n0.AngleTo(n1);
                                       max_angle = Max(max_angle, a);
                                    }
                                 }
                                 if(max_angle < ANGLE_THRESH){
                                          //remove this vertex
                                    rebuild = true;
                                    v_pool[i] = v_pool.back(); v_pool.pop_back();
                                 }
                              }
                              if(rebuild){
                                 ir = occ->Build(v_pool.begin(), v_pool.size());
                              }
                           }
                           */
                           if(I3D_FAIL(ir)){
                              ed->Message("Failed to create occluder from given data!");
                              occ->Release();
                              occ = NULL;
                              frm = NULL;
                           }
                        }
                        break;
                     }
                     if(!occ)
                        break;

                     ed->GetScene()->SetFrameSector(frm);
                  }
               }
               break;

            }
            if(!frm)
               break;
                  
            MakeSceneName(ed->GetScene(), frm_name);
                              //prompt user for name
            while(true){
               S_dlg_name hlp = {ed->GetIGraph(), frm_name};
               bool b = DialogBoxParam(GetHInstance(), "GET_MODELNAME", (HWND)ed->GetIGraph()->GetHWND(),
                  dlgName, (LPARAM)&hlp);
               if(!b){
                  frm->Release();
                  frm = NULL;
                  break;
               }
                              //check name duplication
               if(!IsNameInScene(ed->GetScene(), frm_name))
                  break;
            }
            if(frm){   
               frm->SetName(frm_name);

               if(setup_pos){
                              //put somewhere in front of camera
                  PI3D_camera cam = ed->GetScene()->GetActiveCamera();
                  assert(cam);
                  const S_vector &cdir = cam->GetWorldDir();
                  pos = cam->GetWorldPos() + cdir * 2.0f;
                  rot.SetDir(S_vector(cdir.x, 0.0f, cdir.z), 0.0f);
                  modify_flags |= E_MODIFY_FLG_ROTATION;
                  ed->GetScene()->SetFrameSector(frm);
               }

                              //clear selection and select this model
               if(!context)
                  e_slct->Clear();
               e_slct->AddFrame(frm);

               C_edit_Properties_ed *e_prop = (C_edit_Properties_ed*)ed->FindPlugin("Properties");
               switch(frm->GetType()){
               case FRAME_LIGHT:
                  {
                              //set light to current sector
                     PI3D_frame sct = frm->GetParent();
                     if(sct && sct->GetType()==FRAME_SECTOR){
                        I3DCAST_SECTOR(sct)->AddLight(I3DCAST_LIGHT(frm));
                     }
                     e_prop->ShowFrameSheet(FRAME_LIGHT);
                     e_prop->Activate();
                  }
                  break;

               case FRAME_SOUND:
                  e_prop->ShowFrameSheet(FRAME_SOUND);
                  e_prop->Activate();
                  break;
                  /*
               case FRAME_MODEL:
                  e_prop->ShowFrameSheet(FRAME_MODEL);
                  e_prop->Activate();
                  break;
                  */
               }

               ed->SetModified();
               SaveDeleteUndo(frm);

               e_modify->AddFrameFlags(frm, modify_flags);

                              //apply special modifications now
               if(modify_flags&E_MODIFY_FLG_NU_SCALE){
                  assert(frm->GetType()==FRAME_VOLUME);
                  I3DCAST_VOLUME(frm)->SetNUScale(nu_scl);
               }
               if(modify_flags&E_MODIFY_FLG_POSITION)
                  frm->SetPos(pos);
               if(modify_flags&E_MODIFY_FLG_ROTATION)
                  frm->SetRot(rot);
               if(modify_flags&E_MODIFY_FLG_SCALE)
                  frm->SetScale(scl);
               if(modify_flags&E_MODIFY_FLG_LINK)
                  frm->LinkTo(link);

               ed->GetScene()->AddFrame(frm);
               frm->Release();
            }
         }
         break;

      case E_CREATE_DUPLICATE_SELECTION:
         {
            C_vector<PI3D_frame> sel_list = e_slct->GetCurSel();
            e_slct->Clear();

            for(dword i=0; i<sel_list.size(); i++){
               if(!Duplicate(sel_list[i]))
                  break;
            }
            ed->Message(C_fstr("Duplicated %i frame(s)", i), 0, EM_MESSAGE);
         }
         break;

      case E_CREATE_DUPLICATE_REPEAT:
         {
            PI3D_frame frm = NULL;
            if(last_dup_frm.Size())
               frm = ed->GetScene()->FindFrame(last_dup_frm);
            if(frm){
               e_slct->Clear();
                              //compute new position
               S_vector pos = frm->GetWorldPos() + (frm->GetWorldPos() - last_dup_pos);
               Duplicate(frm, &pos);
            }else
               ed->Message("Cannot repeat duplication", 0, EM_WARNING);
         }
         break;

      case E_CREATE_DELETE_SELECTION:
         {
            if(!ed->CanModify()) break;

            PC_editor_item_MouseEdit e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
            const S_MouseEdit_user_mode *umode = e_medit->GetUserMode();
            if(umode){
               umode->ei->Action(umode->modes[S_MouseEdit_user_mode::MODE_DELETE].action_id);
               break;
            }

            const C_vector<PI3D_frame> sel_list = e_slct->GetCurSel();
            int num = 0;
            for(dword i=0; i<sel_list.size(); i++){
               if(DeleteFrame(sel_list[i]))
                  ++num;
            }
            ed->Message(C_fstr("%i object(s) deleted", num));
         }
         break;

      case E_CREATE_RELOAD_TEXTURE:
      case E_CREATE_RELOAD_MODEL:
         {
            PC_editor_item_MouseEdit e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
            if(e_medit){
               e_medit->SetUserPick(this, id==E_CREATE_RELOAD_TEXTURE ? E_CREATE_RELOAD_TEXTURE_PICK : E_CREATE_RELOAD_MODEL_PICK, LoadCursor(GetHInstance(), "IDC_CURSOR_RELOAD_TXT"));
               switch(id){
               case E_CREATE_RELOAD_TEXTURE:
                  ed->Message("Click on texture to reload");
                  break;
               case E_CREATE_RELOAD_MODEL:
                  ed->Message("Click on model to reload");
                  break;
               }
            }
         }
         break;

      case E_CREATE_RELOAD_TEXTURE_PICK:
         {
            S_MouseEdit_picked *mpck = (S_MouseEdit_picked*)context;
            PI3D_scene scn = ed->GetScene();

            I3D_collision_data cd(mpck->pick_from, mpck->pick_dir,
               I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_RAY,
               NULL);

            if(scn->TestCollision(cd)){
               CPI3D_frame frm = cd.GetHitFrm();
               int fi = cd.GetFaceIndex();
               if(fi!=-1){
                  if(frm->GetType()==FRAME_VISUAL){
                     PI3D_mesh_base mb = I3DCAST_VISUAL(const_cast<PI3D_frame>(frm))->GetMesh();
                     if(mb){
                        CPI3D_face_group fgs = mb->GetFGroups();
                        for(dword i=mb->NumFGroups(); i--; ){
                           if(fgs[i].base_index <= (dword)fi){
                              CPI3D_material mat = ((PI3D_face_group)&fgs[i])->GetMaterial();
                              for(dword i=0; i<4; i++){
                                 PI3D_texture tp = ((PI3D_material)mat)->GetTexture((I3D_MATERIAL_TEXTURE_INDEX)i);
                                 if(tp) tp->Reload();
                              }
                              ed->Message("Texture(s) reloaded");
                              break;
                           }
                        }
                     }
                  }
               }
            }
         }
         return true;

      case E_CREATE_RELOAD_MODEL_PICK:
         {
            S_MouseEdit_picked *mpck = (S_MouseEdit_picked*)context;
            if(mpck->frm_picked){
               PI3D_frame frm = TrySelectTopModel(mpck->frm_picked, false);
               if(frm->GetType()==FRAME_MODEL){
                  PI3D_model mod = I3DCAST_MODEL(frm);
                  const C_str &filename = mod->GetFileName();
                  C_I3D_cache_base<I3D_model> &model_cache = ed->GetModelCache();
                  //model_cache.GetRelativeDirectory(&filename[0]);
                  model_cache.EraseItem(filename);
                  model_cache.Open(mod, filename, ed->GetScene(), 0, OPEN_ERR_LOG(ed, filename));
                  e_slct->FlashFrame(frm);
                  ed->Message(C_fstr("Model '%s' reloaded", (const char*)frm->GetName()));
               }
            }
         }
         return true;
      }
      return 0;
   }

//----------------------------

   void Render(){
   }

//----------------------------
#define SAVE_VERSION 0x03

   virtual bool LoadState(C_chunk &ck){
                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version!=SAVE_VERSION)
         return false;
                              //read other variables
      ck.Read(&mod_show_prv, sizeof(byte));
      ck.Read(&mod_anim_prv, sizeof(byte));
      ck.Read(&snd_show_prv, sizeof(byte));
      ck.Read(&snd_loop_prv, sizeof(byte));
      ck.Read(&single_volume, sizeof(byte));
      ck.Read(&create_static_vol, sizeof(byte));
      ck.ReadString(CT_NAME, last_model_path);
      ck.ReadString(CT_NAME, last_mission_path);
      ck.ReadString(CT_NAME, last_sound_path);

      return true; 
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

                              //write version
      byte version = SAVE_VERSION;
      ck.Write(&version, sizeof(byte));
                              //write other variables
      ck.Write(&mod_show_prv, sizeof(byte));
      ck.Write(&mod_anim_prv, sizeof(byte));
      ck.Write(&snd_show_prv, sizeof(byte));
      ck.Write(&snd_loop_prv, sizeof(byte));
      ck.Write(&single_volume, sizeof(byte));
      ck.Write(&create_static_vol, sizeof(byte));
      ck.WStringChunk(CT_NAME, last_model_path);
      ck.WStringChunk(CT_NAME, last_mission_path);
      ck.WStringChunk(CT_NAME, last_sound_path);
      return true; 
   }
};

//----------------------------

void CreateCreate(PC_editor ed){
   PC_editor_item ei = new C_edit_Create;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
