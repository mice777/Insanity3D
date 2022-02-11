#include "all.h"
#include "resource.h"
#include "common.h"
#include <i3d\CameraPath.h>
#include <insanity\3dtexts.h>
#include <insanity\sprite.h>

//----------------------------

//#define NAME_FONT_SIZE 20
#define NOTIFY_TEXT_TIME 500

//----------------------------

static const S_ctrl_help_info help_texts[] = {
   IDC_ANIM_NAME1,   "Name of camera path animation.",
   IDC_ANIM_NAME,    "Name of camera path animation. Type here to change the name.\nNote that name must be unique from others already created animations.",
   IDC_ADD_KEY,      "Add a new key right after currently selected key. The key is created on position of curretly active camera.",
   IDC_DEL_KEY,      "Delete currently selected key. Note that you cannot delete the last key.",
   IDC_KEY_NUM,      "Information about current key and total number of keys.",
   IDC_FIRST_KEY,    "Go to first key.",
   IDC_LAST_KEY,     "Go to last key.",
   IDC_PREV_KEY,     "Go to previous key.",
   IDC_NEXT_KEY,     "Go to next key.",
   IDC_KEY_DATA,     "Data associated with currently selected key.",
   IDC_NOTE0,        "Note string used for special notifications.",
   IDC_NOTE1,        "Note string used for special notifications.",
   IDC_NOTE2,        "Note string used for special notifications.",
   IDC_NOTE3,        "Note string used for special notifications.",
   /*
   IDC_EASY_FROM_TITLE, "Easy-from value associated with key. A value of 0.0 produces linear time warping, while value of 1.0 produces slow acceleration from the key.",
   IDC_EASY_FROM,    "Type easy-from value in range from 0 to 1.",
   IDC_EASY_TO_TITLE,"Easy-to value associated with key. A value of 0.0 produces linear time warping, while value of 1.0 produces slow de-acceleration to the key.",
   IDC_EASY_TO,      "Type easy-to value in range from 0 to 1.",
   IDC_SMOOTH_TEXT,  "Smoothness of the key, specifying how position/rotation/scale will be adjusted so that passing this key is smooth relatively to neighbor keys. Value of 0.0 means no smoothness, while value os 1.0 makes maximal smooth effect.",
   IDC_SMOOTH,       "Type new smooth value in range from 0 to 1.",
   */
   IDC_EASINESS_TEXT,"Easiness of the key. A value of 0.0 produces linear time warping, while value of 1.0 produces slow deacceleration to the key, and slow acceleration from the key.",
   IDC_EASINESS1,    "Easiness of the key. A value of 0.0 produces linear time warping, while value of 1.0 produces slow deacceleration to the key, and slow acceleration from the key.\nType easy-from value in range from 0 to 1.",
   IDC_SPEED_TITLE,  "Speed of the key, specifying how fast movement will pass through connection from this key to the next keym, in units per second.",
   IDC_SPEED,        "Type new speed value. The value of 0 is a special value causing the key to jump directly to the next key.",
   IDC_PAUSE_TITLE,  "Pause, for which processing animation will be suspended in current key.",
   IDC_PAUSE,        "Type new pause value. The value is in milliseconds.",
   IDC_PLAY_TOGGLE,  "Toggle animation playback.",
   IDC_RESCALE_SPEED,"Tool for rescaling the speed of range of keys.",
   IDCLOSE,          "Close the camera path animation editor.",
   0
};

//----------------------------

static const word note_ids[] = {
   IDC_NOTE0, IDC_NOTE1, IDC_NOTE2, IDC_NOTE3
};
static const int pos_ids[] = {
   IDC_NOTE_POS0, IDC_NOTE_POS1, IDC_NOTE_POS2, IDC_NOTE_POS3
};

//----------------------------

class C_edit_camerapath: public C_editor_item{
   virtual const char *GetName() const{ return "CameraPath"; }

   enum{
      E_CAMPATH_SELECT,//select active cam-path key
      E_CAMPATH_CLOSE,        //close editor
      E_CAMPATH_CREATE_KEY,   //create new position key in current path
      E_CAMPATH_DELETE_KEY,   //delete current key
      E_CAMPATH_IMPORT,       //delete current key
      E_CAMPATH_DRAW_IN_PLAY,

      E_CAMPATH_PLAY_TOGGLE,  //toggle play animation
      E_CAMPATH_NEXT_KEY,
      E_CAMPATH_PREV_KEY,
      E_CAMPATH_FIRST_KEY,
      E_CAMPATH_LAST_KEY,

      E_CAMPATH_EASINESS_INC,
      E_CAMPATH_EASINESS_DEC,
      /*
      E_CAMPATH_SMOOTH_INC,
      E_CAMPATH_SMOOTH_DEC,
      E_CAMPATH_EASY_TO_INC,
      E_CAMPATH_EASY_TO_DEC,
      E_CAMPATH_EASY_FROM_INC,
      E_CAMPATH_EASY_FROM_DEC,
      */
      E_CAMPATH_SPEED_INC,
      E_CAMPATH_SPEED_DEC,
      E_CAMPATH_TYPE_NOTE,
      E_CAMPATH_RESCALE_SPEED,
      /*
      E_CAMPATH_FORWARD,      //one step forward
      E_CAMPATH_FFORWARD,
      E_CAMPATH_BACKWARD,     //one step backward
      E_CAMPATH_FBACKWARD,
      E_CAMPATH_HOME,
      E_CAMPATH_END,
      */
      E_CAMPATH_ACTIVATE_EDIT_WIN,

      E_CAMPATH_SET_ACTIVE_PATH, //(dword *index)
      E_CAMPATH_SET_ACTIVE_KEY,  //(dword *index)
      /*
      E_CAMPATH_SET_EASY_FROM,   //(float)
      E_CAMPATH_SET_EASY_TO,     //(float)
      E_CAMPATH_SET_SMOOTH,      //(float)
      */
      E_CAMPATH_SET_EASINESS,    //(float)
      E_CAMPATH_SET_SPEED,       //(float)
      E_CAMPATH_SET_NOTE,        //(const char *)
      E_CAMPATH_SET_PAUSE,       //(dword)
      E_CAMPATH_SET_SPEEDS,      //(S_rescale_undo*)
      E_CAMPATH_SET_NOTE_POS,    //(float)

      E_CAMPATH_MODIFY_NOTIFY,
      E_CAMPATH_CREATE_KEY1,  //(S_create_undo*)
      E_CAMPATH_DELETE_KEY1,  //(dword index)
      E_CAMPATH_RENAME_ANIM,  //(const char *name)
   };

   struct S_rescale_undo{
      dword first_key;
      dword num_keys;
      float speeds[1];
   };

   struct S_create_undo{
      dword index;
      S_vector pos, dir, up;
      //float easy_to, easy_from, smooth;
      float easiness;
      float speed;
      dword pause;
      char note[1];
   };

   C_smart_ptr<C_editor_item_MouseEdit> e_mouseedit;
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Properties> e_props;
   C_smart_ptr<C_editor_item_TimeScale> e_timescale;
   C_smart_ptr<I3D_camera> cam;

   C_smart_ptr<I3D_material> mat_icon;

   HWND hwnd_edit;            //NULL if sheet is not displayed

   bool run;
   bool draw_in_playback;

   C_smart_ptr<C_camera_path> cam_path;

   C_vector<S_camera_path_data> bezier_paths;
   int active_path;           //currently edited path, or -1
   dword active_key;
   bool cam_dirty;            //true if camera changed

//----------------------------

   PI3D_material GetIcon(){

      if(!mat_icon){
                              //load checkpoint texture
         I3D_CREATETEXTURE ct;
         memset(&ct, 0, sizeof(ct));
         ct.flags = 0;

         C_cache ck;
         if(OpenResource(GetHInstance(), "BINARY", "Sphere", ck)){
            ct.flags |= TEXTMAP_DIFFUSE | TEXTMAP_TRANSP | TEXTMAP_MIPMAP | TEXTMAP_USE_CACHE | TEXTMAP_TRUECOLOR;
            ct.ck_diffuse = &ck;
         }
         PI3D_texture tp;
         ed->GetDriver()->CreateTexture(&ct, &tp);

         PI3D_material mat = ed->GetDriver()->CreateMaterial();
         mat->SetTexture(MTI_DIFFUSE, tp);
         tp->Release();
         mat_icon = mat;
         mat->Release();
      }
      return mat_icon;
   }

//----------------------------

   C_smart_ptr<C_text> notify_text;
   int notify_text_count;

//----------------------------

   void ShowNotifyText(const char *txt, float x, float y, float sz, dword color){

      S_text_create ct;
      ct.tp = txt;
      ct.x = x;
      ct.y = y;
      ct.color = color;
      ct.size = sz;
      PC_poly_text texts = ed->GetTexts();
      notify_text = texts->CreateText(ct);
      notify_text->Release();

      notify_text_count = NOTIFY_TEXT_TIME;
   }

//----------------------------

   static bool NotifyCallback(const C_str &note, void *context){

      C_edit_camerapath *_this = (C_edit_camerapath*)context;
      _this->ShowNotifyText(C_fstr("Notify: \"%s\"", (const char*)note),
         .02f, .73f, .024f, 0xffffffff);
      return true;
   }

//----------------------------

   void SetCamPosRot(const S_vector &pos, const S_vector &dir, const S_vector &up){

      C_smart_ptr<I3D_camera> save_cam = cam;
      cam = NULL;
      save_cam->SetPos(pos);
      save_cam->SetDir1(dir, up);
      e_modify->AddFrameFlags(save_cam, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION);
      cam = save_cam;
   }

//----------------------------

   void TickBezierPath(int time){

      S_vector pos, dir, up;
      if(cam_path->Tick(time, pos, dir, up, draw_in_playback ? NotifyCallback : NULL, this, true))
         SetCamPosRot(pos, dir, up);
   }

//----------------------------
// Feed paths into the dialog's list control.
   void InitList(HWND hwnd){

      SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_RESETCONTENT, 0, 0);
      for(dword i=bezier_paths.size(); i--; ){
         const S_camera_path_data &pd = bezier_paths[i];
         int indx = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_ADDSTRING, 0, (LPARAM)(const char*)pd.name);
         SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_SETITEMDATA, indx, i);
      }
      if(active_path!=-1){
         SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_SELECTSTRING, 0, (LPARAM)(const char*)bezier_paths[active_path].name);
         EnableWindow(GetDlgItem(hwnd, IDC_DELETE), true);
         EnableWindow(GetDlgItem(hwnd, IDC_DUPLICATE), true);
         EnableWindow(GetDlgItem(hwnd, IDOK), true);
      }
   }

//----------------------------

   BOOL dlgPathSelect(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            InitDlg(ed->GetIGraph(), hwnd);
            InitList(hwnd);
         }
         return true;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDC_CAMPATH_LIST:
            switch(HIWORD(wParam)){
            case LBN_SELCHANGE:
               {
                  int csel = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETCURSEL, 0, 0);
                  if(csel!=-1)
                     csel = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETITEMDATA, csel, 0);
                  bool on = (csel!=-1);
                  EnableWindow(GetDlgItem(hwnd, IDC_DELETE), on);
                  EnableWindow(GetDlgItem(hwnd, IDC_DUPLICATE), on);
                  EnableWindow(GetDlgItem(hwnd, IDOK), on);
               }
               break;
            }
            break;

         case IDC_NEW:
         case IDC_DUPLICATE:
            {
               if(!ed->CanModify())
                  break;
                              //let user create a new path
               C_str str;
               int dup_src = -1;
               if(LOWORD(wParam)==IDC_DUPLICATE){
                  dup_src = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETCURSEL, 0, 0);
                  if(dup_src!=-1)
                     dup_src = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETITEMDATA, dup_src, 0);
                  assert(dup_src!=-1);
                  if(dup_src==-1)
                     break;
                  str = bezier_paths[dup_src].name;
               }
               while(true){
                  if(!SelectName(ed->GetIGraph(), hwnd, "Enter Camera Path name", str))
                     return 0;
                  if(!str.Size())
                     continue;
                  for(int i=bezier_paths.size(); i--; ){
                     if(bezier_paths[i].name.Matchi(str))
                        break;
                  }
                  if(i==-1)
                     break;
               }
                              //add the new path
               bezier_paths.push_back(S_camera_path_data());
               S_camera_path_data &pd = bezier_paths.back();
               if(dup_src!=-1){
                  pd = bezier_paths[dup_src];
               }else{
                              //create 1st key
                  pd.keys.insert(pd.keys.begin(), S_cam_path_key());
                  S_cam_path_key &key = pd.keys.back();
                              //put somewhere in front of camera
                  const S_matrix &m = ed->GetScene()->GetActiveCamera()->GetMatrix();
                  key.pos = m(3);
                  key.dir = m(2);
                  key.up = m(1);
               }
               pd.name = str;

               SetActivePath(bezier_paths.size() - 1);
               ed->SetModified();

               if(dup_src==-1){
                              //create 1st key on path
                  CreateKey();
               }

               EndDialog(hwnd, 1);
            }
            break;

         case IDC_DELETE:
            {
               if(!ed->CanModify())
                  break;

               int csel = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETCURSEL, 0, 0);
               if(csel==-1)
                  break;
               csel = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETITEMDATA, csel, 0);
               if(csel==-1)
                  break;
               if(MessageBox(hwnd, C_fstr("Are you sure to delete path '%s'?", (const char*)bezier_paths[csel].name), "Delete camera path", MB_YESNO)!=IDYES)
                  break;

               SetActivePath(-1);
               bezier_paths[csel] = bezier_paths.back(); bezier_paths.pop_back();
               InitList(hwnd);
               ed->SetModified();
            }
            break;

         case IDOK:
            {
               int csel = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETCURSEL, 0, 0);
               if(csel==-1)
                  break;
               dword ap = SendDlgItemMessage(hwnd, IDC_CAMPATH_LIST, LB_GETITEMDATA, csel, 0);
               SetActivePath(ap);
               EndDialog(hwnd, 1);
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

//----------------------------

   static BOOL CALLBACK dlgPathSelectThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      C_edit_camerapath *pd = (C_edit_camerapath*)GetWindowLong(hwnd, GWL_USERDATA);
      if(pd)
         return pd->dlgPathSelect(hwnd, uMsg, wParam, lParam);
      return 0;
   }

//----------------------------

   struct S_wnd_pos: public POINT{
      bool valid;
      S_wnd_pos():
         valid(false)
      {
      }
   };
   S_wnd_pos edit_wnd_pos;

//----------------------------

   static void DisplayHelp(HWND hwnd, word ctrl_id, const S_ctrl_help_info *hi){
      RECT rc;
      GetWindowRect(GetDlgItem(hwnd, ctrl_id), &rc);
      POINT pt;
      pt.x = rc.left;
      pt.y = rc.bottom;

      ScreenToClient(hwnd, &pt);
      pt.x -= 40;
      pt.y += 2;
      const char *txt = "<no help>";
      for(dword i=0; hi[i].ctrl_id; i++){
         if(hi[i].ctrl_id==ctrl_id){
            txt = hi[i].text;
            break;
         }
      }
      OsDisplayHelpWindow(txt, hwnd, pt.x, pt.y, 150);
   }

//----------------------------

   BOOL dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            if(!edit_wnd_pos.valid){
               RECT rc;
               GetWindowRect(hwnd, &rc);

               RECT rc_app;
               GetWindowRect((HWND)ed->GetIGraph()->GetHWND(), &rc_app);
                                    //by default, put it to bottom, align with main window
               int x = rc_app.left;
               int y = GetSystemMetrics(SM_CYSCREEN);
               y -= (rc.bottom - rc.top);
               y = Min(y, (int)rc_app.bottom);

               edit_wnd_pos.x = x;
               edit_wnd_pos.y = y;
               edit_wnd_pos.valid = true;
            }
            SetWindowPos(hwnd, NULL, edit_wnd_pos.x, edit_wnd_pos.y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
         }
         break;

      case WM_MOVE:
         {
            RECT rc;
            if(GetWindowRect(hwnd, &rc)){
               edit_wnd_pos.x = rc.left;
               edit_wnd_pos.y = rc.top;
               edit_wnd_pos.valid = true;
            }
         }
         break;

      case WM_HELP:
         {
            LPHELPINFO hi = (LPHELPINFO)lParam;
            DisplayHelp(hwnd, (word)hi->iCtrlId, help_texts);
         }
         return true;

      case WM_COMMAND:
         {
            WORD idc = LOWORD(wParam);
            switch(idc){
               /*
            case IDCANCEL:
               SetActiveWindow((HWND)ed->GetIGraph()->GetHWND());
               break;
               */

            case IDCLOSE:
               Action(E_CAMPATH_CLOSE);
               break;

            case IDC_ADD_KEY: Action(E_CAMPATH_CREATE_KEY); break;
            case IDC_DEL_KEY: Action(E_CAMPATH_DELETE_KEY); break;
            case IDC_FIRST_KEY: Action(E_CAMPATH_FIRST_KEY); break;
            case IDC_LAST_KEY: Action(E_CAMPATH_LAST_KEY); break;
            case IDC_PREV_KEY: Action(E_CAMPATH_PREV_KEY); break;
            case IDC_NEXT_KEY: Action(E_CAMPATH_NEXT_KEY); break;
            case IDC_PLAY_TOGGLE:
               {
                  Action(E_CAMPATH_PLAY_TOGGLE);
               }
               break;
            case IDC_RESCALE_SPEED: Action(E_CAMPATH_RESCALE_SPEED); break;

            case IDC_ANIM_NAME:
               switch(HIWORD(wParam)){
               case EN_CHANGE:
                  if(hwnd_edit){
                     S_camera_path_data &pd = bezier_paths[active_path];
                     if(!ed->CanModify()){
                        hwnd_edit = NULL;
                        SetDlgItemText(hwnd, idc, pd.name);
                        hwnd_edit = hwnd;
                        break;
                     }
                     char buf[256];
                     GetDlgItemText(hwnd, IDC_ANIM_NAME, buf, sizeof(buf));
                     if(*buf){
                              //check if no other anim has this name
                        for(int i=bezier_paths.size(); i--; ){
                           if(bezier_paths[i].name==buf)
                              break;
                        }
                        if(i!=-1)
                           break;
                              //save undo (if not yet)
                        bool undo_on = (!e_undo->IsTopEntry(this, E_CAMPATH_RENAME_ANIM));
                        if(undo_on)
                           e_undo->Save(this, E_CAMPATH_RENAME_ANIM, (void*)(const char*)pd.name, pd.name.Size()+1);
                        pd.name = buf;
                        ed->SetModified();
                     }
                  }
                  break;
               }
               break;

            case IDC_NOTE0:
            case IDC_NOTE1:
            case IDC_NOTE2:
            case IDC_NOTE3:
               switch(HIWORD(wParam)){
               case EN_CHANGE:
                  if(hwnd_edit){
                     static const word note_ids[] = { IDC_NOTE0, IDC_NOTE1, IDC_NOTE2, IDC_NOTE3};
                     for(dword ni=0; note_ids[ni]!=LOWORD(wParam); ni++);

                     S_camera_path_data &pd = bezier_paths[active_path];
                     S_cam_path_key &key = pd.keys[active_key];
                     if(!ed->CanModify()){
                        hwnd_edit = NULL;
                        SetDlgItemText(hwnd, idc, key.notes[ni].text);
                        hwnd_edit = hwnd;
                        break;
                     }
                     char buf[256];
                     GetDlgItemText(hwnd, idc, buf, sizeof(buf));

                              //save undo (if not yet)
                     bool undo_on = (!e_undo->IsTopEntry(this, E_CAMPATH_SET_NOTE));
                     if(undo_on)
                        e_undo->Save(this, E_CAMPATH_SET_NOTE,
                        (void*)(const char*)key.notes[ni].text, key.notes[ni].text.Size()+1);
                     key.notes[ni].text = buf;
                     SetDlgItemText(hwnd, pos_ids[ni], *buf ? FloatStrip(C_fstr("%.2f", key.notes[ni].pos)) : "");
                     ed->SetModified();
                  }
                  break;
               }
               break;

            case IDC_NOTE_POS0:
            case IDC_NOTE_POS1:
            case IDC_NOTE_POS2:
            case IDC_NOTE_POS3:
               switch(HIWORD(wParam)){
               case EN_CHANGE:
                  if(hwnd_edit){
                     static const word note_ids[] = { IDC_NOTE_POS0, IDC_NOTE_POS1, IDC_NOTE_POS2, IDC_NOTE_POS3};
                     for(dword ni=0; note_ids[ni]!=LOWORD(wParam); ni++);
                     if(!ed->CanModify()){
                        RedrawKeyData();
                        break;
                     }
                     char buf[256];
                     GetDlgItemText(hwnd, idc, buf, sizeof(buf));
                     float f;
                     if(sscanf(buf, "%f", &f)==1){
                        S_camera_path_data &pd = bezier_paths[active_path];
                        S_cam_path_key &key = pd.keys[active_key];

                        float &val = key.notes[ni].pos;
                        if(val != f){
                              //save undo, if not yet
                           bool undo_on = (!e_undo->IsTopEntry(this, E_CAMPATH_SET_NOTE_POS));
                           if(undo_on){
                              struct S_undo{
                                 dword note_index;
                                 float pos;
                              } undo = { ni, val};
                              e_undo->Save(this, E_CAMPATH_SET_NOTE_POS, &undo, sizeof(undo));
                           }
                           val = f;
                           ed->SetModified();
                        }
                     }
                  }
                  break;
               }
               break;

               /*
            case IDC_EASY_FROM:
            case IDC_EASY_TO:
            case IDC_SMOOTH:
            */
            case IDC_EASINESS1:
            case IDC_SPEED:
            case IDC_PAUSE:
               switch(HIWORD(wParam)){
               case EN_CHANGE:
                  if(hwnd_edit){
                     if(!ed->CanModify()){
                        RedrawKeyData();
                        break;
                     }
                     char buf[256];
                     GetDlgItemText(hwnd, idc, buf, sizeof(buf));
                     float f;
                     if(sscanf(buf, "%f", &f)==1){
                        S_camera_path_data &pd = bezier_paths[active_path];
                        S_cam_path_key &key = pd.keys[active_key];

                        if(idc==IDC_PAUSE){
                           dword tm = FloatToInt(f);
                           if(key.pause!=tm){
                              bool undo_on = (!e_undo->IsTopEntry(this, E_CAMPATH_SET_PAUSE));
                              if(undo_on)
                                 e_undo->Save(this, E_CAMPATH_SET_PAUSE, &key.pause, sizeof(dword));
                              key.pause = tm;
                              ed->SetModified();
                           }
                           break;
                        }
                        float *val;
                        dword undo_id;
                        switch(idc){
                           /*
                        case IDC_EASY_FROM: val = &key.easy_from; undo_id = E_CAMPATH_SET_EASY_FROM; break;
                        case IDC_EASY_TO: val = &key.easy_to; undo_id = E_CAMPATH_SET_EASY_TO; break;
                        case IDC_SMOOTH: val = &key.smooth; undo_id = E_CAMPATH_SET_SMOOTH; break;
                        */
                        case IDC_EASINESS1: val = &key.easiness; undo_id = E_CAMPATH_SET_EASINESS; break;
                        case IDC_SPEED: val = &key.speed; undo_id = E_CAMPATH_SET_SPEED; break;
                        default: assert(0); val = NULL; undo_id = 0;
                        }
                        if(*val != f){
                              //save undo, if not yet
                           bool undo_on = (!e_undo->IsTopEntry(this, undo_id));
                           if(undo_on)
                              e_undo->Save(this, undo_id, val, sizeof(float));
                           *val = f;
                           ed->SetModified();
                        }
                     }
                  }
                  break;

               case EN_KILLFOCUS:
                              //redraw changed values to proper format
                  RedrawKeyData();
                  break;
               }
               break;
            }
         }
         break;

      case WM_NOTIFY:
         {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            switch(nmhdr->idFrom){
               /*
            case IDC_SPIN_EASY_FROM:
            case IDC_SPIN_EASY_TO:
            case IDC_SPIN_SMOOTH:
            */
            case IDC_SPIN_EASINESS:
            case IDC_SPIN_SPEED:
            case IDC_SPIN_NOTE0:
            case IDC_SPIN_NOTE1:
            case IDC_SPIN_NOTE2:
            case IDC_SPIN_NOTE3:
               {
                  switch(nmhdr->code){
                  case UDN_DELTAPOS:
                     {
                        int id_buddy = 0;
                        switch(nmhdr->idFrom){
                           /*
                        case IDC_SPIN_EASY_FROM: id_buddy = IDC_EASY_FROM; break;
                        case IDC_SPIN_EASY_TO: id_buddy = IDC_EASY_TO; break;
                        case IDC_SPIN_SMOOTH: id_buddy = IDC_SMOOTH; break;
                        */
                        case IDC_SPIN_EASINESS: id_buddy = IDC_EASINESS1; break;
                        case IDC_SPIN_SPEED: id_buddy = IDC_SPEED; break;
                        case IDC_SPIN_NOTE0: id_buddy = IDC_NOTE_POS0; break;
                        case IDC_SPIN_NOTE1: id_buddy = IDC_NOTE_POS1; break;
                        case IDC_SPIN_NOTE2: id_buddy = IDC_NOTE_POS2; break;
                        case IDC_SPIN_NOTE3: id_buddy = IDC_NOTE_POS3; break;
                        default: assert(0);
                        }

                        NMUPDOWN *ud = (NMUPDOWN*)lParam;
                        int delta = -ud->iDelta;
                        char buf[64];
                        if(GetDlgItemText(hwnd, id_buddy, buf, sizeof(buf))){
                           float num;
                           if(sscanf(buf, "%f", &num)==1){
                              float new_num;
                              if(nmhdr->idFrom==IDC_SPIN_SPEED){
                                 new_num = Max(0.0f, num + (float)delta * 1.0f);
                              }else{
                                 new_num = Max(0.0f, Min(1.0f, num + (float)delta * .1f));
                              }
                              if(num!=new_num)
                                 SetDlgItemText(hwnd, id_buddy, FloatStrip(C_fstr("%.2f", new_num)));
                           }
                        }
                     }
                     return 1;//return 1 to disallow Windows to change spiner's position
                  }
               }
               break;
            }
         }
         break;

         /*
      case WM_CLOSE:
         Action(E_CAMPATH_CLOSE);
         break;
         */
      }
      return 0;
   }

   static BOOL CALLBACK dlgProc_Thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      C_edit_camerapath *_this = (C_edit_camerapath*)GetWindowLong(hwnd, GWL_USERDATA);
      if(_this)
         return _this->dlgProc(hwnd, uMsg, wParam, lParam);
      return 0;
   }

//----------------------------

   void InitEditWindowData(){

      HWND hwnd = hwnd_edit;
      assert(hwnd && active_path!=-1);
      const S_camera_path_data &pd = bezier_paths[active_path];

      HWND hwnd_save = hwnd_edit;
      hwnd_edit = NULL;
      SetDlgItemText(hwnd, IDC_ANIM_NAME, pd.name);

      hwnd_edit = hwnd_save;
   }

//----------------------------

   void SetActiveKey(dword index, bool force = false){

      if(active_key==index && !force)
         return;

      assert(active_path!=-1);
      active_key = index;

      HWND hwnd = hwnd_edit;
      assert(hwnd);
      const S_camera_path_data &pd = bezier_paths[active_path];
      const S_cam_path_key &key = pd.keys[active_key];

      hwnd_edit = NULL;
      SetDlgItemText(hwnd, IDC_KEY_NUM, C_fstr("%i / %i", active_key, pd.keys.size()));
      /*
      SetDlgItemText(hwnd, IDC_EASY_FROM, FloatStrip(C_fstr("%.2f", key.easy_from)));
      SetDlgItemText(hwnd, IDC_EASY_TO, FloatStrip(C_fstr("%.2f", key.easy_to)));
      SetDlgItemText(hwnd, IDC_SMOOTH, FloatStrip(C_fstr("%.2f", key.smooth)));
      */
      SetDlgItemText(hwnd, IDC_EASINESS1, FloatStrip(C_fstr("%.2f", key.easiness)));
      SetDlgItemText(hwnd, IDC_SPEED, FloatStrip(C_fstr("%.2f", key.speed)));
      for(dword ni=0; ni<CAMPATH_MAX_NOTES_PER_KEY; ni++){
         const C_str &str = key.notes[ni].text;
         SetDlgItemText(hwnd, note_ids[ni], str);
         SetDlgItemText(hwnd, pos_ids[ni], str.Size() ? FloatStrip(C_fstr("%.2f", key.notes[ni].pos)) : "");
      }
      SetDlgItemText(hwnd, IDC_PAUSE, C_fstr("%i", key.pause));
      hwnd_edit = hwnd;

                              //setup camera from key data
      SetCamPosRot(key.pos, key.dir, key.up);
   }

//----------------------------

   void RedrawKeyData(){
      SetActiveKey(active_key, true);
   }

//----------------------------

   void SetActivePath(int indx, bool save_undo = true){

      if(active_path==indx)
         return;

      StopPlay();

      if(active_path!=-1){
         //cp_map.clear();
         e_modify->RemoveNotify(this);
         cam->LinkTo(NULL);
         ed->RemoveTempShortcuts(this);
         e_undo->Save(this, E_CAMPATH_SET_ACTIVE_KEY, &active_key, sizeof(dword));
         active_key = 0;

         //ed->GetIGraph()->RemoveDlgHWND(hwnd_edit);
         //ShowWindow(hwnd_edit, SW_HIDE);
         e_props->RemoveSheet(hwnd_edit);
      }
      if(save_undo)
         e_undo->Save(this, E_CAMPATH_SET_ACTIVE_PATH, &active_path, sizeof(dword));
      active_path = indx;

      if(active_path!=-1){
         e_modify->AddNotify(this, E_CAMPATH_MODIFY_NOTIFY);

         cam->LinkTo(ed->GetScene()->GetPrimarySector());
         cam->Duplicate(ed->GetScene()->GetActiveCamera());
         cam->SetPos(S_vector(0, 0, 0));
         cam->SetDir(S_vector(0, 0, 1));
         cam->SetName("<campath>");

         S_tmp_shortcut shortcuts[] = {
            {E_CAMPATH_CREATE_KEY, K_INS, SKEY_ALT, "Insert new key"},
            {E_CAMPATH_DELETE_KEY, K_DEL, SKEY_ALT, "Delete current key"},
            {E_CAMPATH_PLAY_TOGGLE, K_P, 0, "Play camera path"},
            {E_CAMPATH_PREV_KEY, K_CURSORLEFT, 0, "Prev key"},
            {E_CAMPATH_NEXT_KEY, K_CURSORRIGHT, 0, "Next key"},
            {E_CAMPATH_FIRST_KEY, K_HOME, 0, "First key"},
            {E_CAMPATH_LAST_KEY, K_END, 0, "Last key"},
            {E_CAMPATH_ACTIVATE_EDIT_WIN, K_F3, 0, "Activate animator window"},

            {E_CAMPATH_EASINESS_INC, K_GREYPLUS, 0, "Inrease easiness"},
            {E_CAMPATH_EASINESS_DEC, K_GREYMINUS, 0, "Decrease easiness"},
            /*
            {E_CAMPATH_SMOOTH_INC, K_GREYPLUS, 0, "Inrease smooth"},
            {E_CAMPATH_SMOOTH_DEC, K_GREYMINUS, 0, "Decrease smooth"},
            {E_CAMPATH_EASY_TO_INC, K_GREYPLUS, SKEY_CTRL, "Inrease easy to"},
            {E_CAMPATH_EASY_TO_DEC, K_GREYMINUS, SKEY_CTRL, "Decrease easy to"},
            {E_CAMPATH_EASY_FROM_INC, K_GREYPLUS, SKEY_CTRL|SKEY_ALT, "Inrease easy from"},
            {E_CAMPATH_EASY_FROM_DEC, K_GREYMINUS, SKEY_CTRL|SKEY_ALT, "Decrease easy from"},
            */
            {E_CAMPATH_SPEED_INC, K_GREYPLUS, SKEY_SHIFT, "Inrease speed"},
            {E_CAMPATH_SPEED_DEC, K_GREYMINUS, SKEY_SHIFT, "Decrease speed"},
            {E_CAMPATH_TYPE_NOTE, K_N, SKEY_SHIFT, "Type Note"},
            {E_CAMPATH_CLOSE, K_F8, SKEY_CTRL|SKEY_ALT|SKEY_SHIFT, "Close campath edit mode"},
            /*
            {E_CAMPATH_FORWARD, K_GREYPLUS, 0, "Forward"},
            {E_CAMPATH_BACKWARD, K_GREYMINUS, 0, "Backward"},
            {E_CAMPATH_FFORWARD, K_GREYPLUS, SKEY_SHIFT, "Fast forward"},
            {E_CAMPATH_FBACKWARD, K_GREYMINUS, SKEY_SHIFT, "Fast backward"},
            {E_CAMPATH_HOME, K_GREYMINUS, SKEY_CTRL, "Home"},
            {E_CAMPATH_END, K_GREYPLUS, SKEY_CTRL, "End"},
            */
            {0}
         };
         ed->InstallTempShortcuts(this, shortcuts);

                              //create and init editor window
         if(!hwnd_edit){
            hwnd_edit = CreateDialogParam(GetHInstance(), "IDD_CAMPATH_EDIT", (HWND)ed->GetIGraph()->GetHWND(), dlgProc_Thunk, (LPARAM)this);
         }
         SetActiveKey(0, true);
         SetDlgItemText(hwnd_edit, IDC_PLAY_TOGGLE, "P&lay");
         //ed->GetIGraph()->AddDlgHWND(hwnd_edit);
         //ShowWindow(hwnd_edit, SW_SHOWNOACTIVATE);
         e_props->AddSheet(hwnd_edit);
         InitEditWindowData();
      }
   }

//----------------------------

   void CreateKey(const S_create_undo *cd = NULL){

      if(!ed->CanModify())
         return;

      assert(active_path!=-1);
      S_camera_path_data &pd = bezier_paths[active_path];

      dword insert_pos = cd ? cd->index : active_key+1;//pd.keys.size();
      assert((dword)insert_pos <= pd.keys.size());

      pd.keys.insert(pd.keys.begin()+insert_pos, S_cam_path_key());

      S_cam_path_key &key = pd.keys[insert_pos];

      if(cd){
         key.pos = cd->pos;
         key.dir = cd->dir;
         key.up = cd->up;
         /*
         key.easy_to = cd->easy_to;
         key.easy_from = cd->easy_from;
         key.smooth = cd->smooth;
         */
         key.easiness = cd->easiness;
         key.speed = cd->speed;
         key.pause = cd->pause;
         key.notes[0].text = cd->note;
      }else{
                              //put somewhere in front of camera
         const S_matrix &m = ed->GetScene()->GetActiveCamera()->GetMatrix();
         key.pos = m(3);
         key.dir = m(2);
         key.up = m(1);
                              //copy some of data from current key
         const S_cam_path_key &ckey = pd.keys[active_key];
         //key.smooth = ckey.smooth;
         key.easiness = ckey.easiness;
         key.speed = ckey.speed;
      }
      ed->SetModified();

                              //make the new key active
      SetActiveKey(insert_pos, true);
                              //add undo info
      e_undo->Save(this, E_CAMPATH_DELETE_KEY1, &active_key, sizeof(dword));
   }

//----------------------------

   static float GetAngle(float x, float y){

      if(!x) return y>0 ? PI/2.0f : 3*PI/2.0f;
      float val;
      if(x > 0.0f){
         val = atanf(y/x);
         if(val < 0.0f)
            val += (PI*2.0f);
         return val;
      }else{
         val = PI + atanf(-y/-x);
         if(val < 0.0f) val += (PI*2.0f);
         return val;
      }
   }

//----------------------------

   S_vector GetTranslateVector(int rx, int ry, bool move_y){

      const float speed = e_mouseedit->GetMoveSpeed();
      PI3D_camera cam = ed->GetScene()->GetActiveCamera();
      S_vector delta;
      if(!move_y){
         const S_vector &cdir = cam->GetMatrix()(2);
         float zx_angle = GetAngle(cdir.z, -cdir.x);
         delta = S_vector(((float)cos(zx_angle)*rx + (float)sin(zx_angle)*ry) * speed / 200.0f,
            0, ((float)sin(zx_angle)*rx - (float)cos(zx_angle)*ry) * speed / 200.0f);
      }else{
         delta = S_vector(0, -ry * speed / 200.0f, 0);
      }
      return delta;
   }

//----------------------------
// Stop playback. Return true if animation was playing.
   bool StopPlay(){
      if(run){
         Action(E_CAMPATH_PLAY_TOGGLE);
         return true;
      }
      return false;
   }

//----------------------------

   virtual void AfterLoad(){
      e_modify->AddFrameFlags(cam, E_MODIFY_FLG_TEMPORARY);
   }

//----------------------------

   virtual void BeforeFree(){

      StopPlay();
      SetActivePath(-1, false);
      cam_path->Close();
      bezier_paths.clear();
      e_modify->RemoveFrame(cam);
   }

//----------------------------

   virtual bool Validate(){

      bool ok = true;
      PC_editor_item_Log e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
                        //validate all camera paths
      for(dword pi=0; pi<bezier_paths.size(); pi++){
         const S_camera_path_data &cp = bezier_paths[pi];
         for(dword ki=0; ki<cp.keys.size(); ki++){
            const S_cam_path_key &k = cp.keys[ki];
            C_str msg;
            /*
            if(k.easy_from<0.0f || k.easy_from>1.0f) msg = "invalid easy from";
            if(k.easy_to<0.0f || k.easy_to>1.0f) msg = "invalid easy to";
            if(k.smooth<0.0f || k.smooth>1.0f) msg = "invalid smooth";
            */
            if(k.easiness<0.0f || k.easiness>1.0f) msg = "invalid easiness";
            if(k.speed<0.0f) msg = "invalid speed";
            if(msg.Size()){
               ok = false;
               e_log->AddText(C_fstr("Camera path '%s': invalid key %i: %s", (const char*)cp.name, ki, (const char*)msg));
               Action(E_CAMPATH_SET_ACTIVE_PATH, &pi);
               Action(E_CAMPATH_SET_ACTIVE_KEY, &ki);
               break;
            }
         }
      }
      return ok;
   }

//----------------------------

   virtual void LoadFromMission(C_chunk &ck){

      bezier_paths.clear();

      int edit_i = -1;
      int curr_edit_pos = -1;
      while(ck)
      switch(++ck){
      case CT_CAMPATH_LAST_EDITED:
         edit_i = ck.RIntChunk();
         break;
      case CT_CAMPATH_CURR_EDIT_POS:
         curr_edit_pos = ck.RIntChunk();
         break;
      case CT_CAMERA_PATH:
         {
            bezier_paths.push_back(S_camera_path_data());
            S_camera_path_data &pd = bezier_paths.back();
            if(!LoadCameraPath(ck, pd))
               return;
            --ck;
         }
         break;
      default: assert(0); --ck;
      }
      SetActivePath(edit_i, false);
      if(curr_edit_pos!=-1)
         SetActiveKey(curr_edit_pos);
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==1){
         if(!bezier_paths.size())
            return;
         ck <<= CT_CAMERA_PATHS;

         ck(CT_CAMPATH_LAST_EDITED, (int)active_path);
                           //save all camera paths
         for(dword i=0; i<bezier_paths.size(); i++){
            const S_camera_path_data &pd = bezier_paths[i];
            ck <<= CT_CAMERA_PATH;
            {
               ck.WStringChunk(CT_CAMPATH_NAME, pd.name);
               ck <<= CT_CAMPATH_KEYS;
               {
                  dword num = pd.keys.size();
                  ck.Write(&num, sizeof(dword));
                  for(dword i=0; i<num; i++){
                     ck <<= CT_CAMPATH_KEY;
                     {
                        const S_cam_path_key &nd = pd.keys[i];
                        ck.WVectorChunk(CT_CAMPATH_POS, nd.pos);
                        ck.WVectorChunk(CT_CAMPATH_DIR, nd.dir);
                        ck.WVectorChunk(CT_CAMPATH_UPVECTOR, nd.up);
                        for(dword ni=0; ni<CAMPATH_MAX_NOTES_PER_KEY; ni++){
                           const S_cam_path_key::S_note &nt = nd.notes[ni];
                           if(nt.text.Size()){
                              ck <<= CT_CAMPATH_NOTE;
                              ck.Write(&nt.pos, sizeof(float));
                              ck.Write((const char*)nt.text, nt.text.Size()+1);
                              --ck;
                           }
                        }
                        ck.WFloatChunk(CT_CAMPATH_SPEED, nd.speed);
                        if(nd.easiness) ck.WFloatChunk(CT_CAMPATH_EASINESS, nd.easiness);
                        /*
                        if(nd.smooth!=1.0f) ck.WFloatChunk(CT_CAMPATH_SMOOTH, nd.smooth);
                        if(nd.easy_from) ck.WFloatChunk(CT_CAMPATH_EASY_FROM, nd.easy_from);
                        if(nd.easy_to) ck.WFloatChunk(CT_CAMPATH_EASY_TO, nd.easy_to);
                        */
                        if(nd.pause) ck.WIntChunk(CT_CAMPATH_PAUSE, nd.pause);
                     }
                     --ck;
                  }
               }
               --ck;
            }
            --ck;
         }
         if(active_path!=-1)
            ck.WIntChunk(CT_CAMPATH_CURR_EDIT_POS, active_key);
         --ck;
      }
   }

//----------------------------

   virtual void OnPick(S_MouseEdit_picked *mp){

      if(active_path==-1)
         return;
      const S_vector &from = mp->pick_from, &dir = mp->pick_dir;

      float &best_dist = mp->pick_dist;
      int best_pt = -1;
      if(!mp->frm_picked)
         best_dist = 1e+16f;
      const S_camera_path_data &pd = bezier_paths[active_path];
      const float scale = .5f;

      for(dword i=pd.keys.size(); i--; ){
         const S_cam_path_key &key = pd.keys[i];
                        //get distance of the CP from direction C_vector
         float u = dir.Dot(key.pos - from);
         if(u>=0.0f && u < best_dist){
            S_vector pol = from + dir * u;
            float dist_to_line = (pol - key.pos).Magnitude();
            if(dist_to_line < (scale * .5f)){
               best_dist = u;
               best_pt = i;
               mp->frm_picked = cam;
            }
         }
      }
      switch(mp->pick_action){
      case S_MouseEdit_picked::PA_DEFAULT:
      case S_MouseEdit_picked::PA_TOGGLE:
         if(best_pt != -1){
            if(active_key!=(dword)best_pt){
               e_undo->Save(this, E_CAMPATH_SET_ACTIVE_KEY, &active_key, sizeof(dword));
               SetActiveKey(best_pt);
               ed->Message(C_fstr("Setting active key to index %i", best_pt));
            }
         }
         break;
      }
   }

//----------------------------
public:
   C_edit_camerapath():
      active_path(-1),
      //name_font(NULL),
      draw_in_playback(true),
      hwnd_edit(NULL),
      cam_dirty(false),
      run(false)
   {
      cam_path = CreateCameraPath(); cam_path->Release();
   }
   ~C_edit_camerapath(){
      cam = NULL;
   }

//----------------------------

   virtual bool Init(){

      e_mouseedit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      e_timescale = (PC_editor_item_TimeScale)ed->FindPlugin("TimeScale");
      if(!e_mouseedit || !e_undo || !e_modify || !e_props || !e_timescale)
         return false;

//#define MENU_BASE "%80 &Game\\Camera Path\\"
#define MENU_BASE "%85 &Anim\\Camera &Path\\"
      ed->AddShortcut(this, E_CAMPATH_SELECT, MENU_BASE"&Select camera path\tCtrl+Shift+F8", K_F8, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, E_CAMPATH_IMPORT, MENU_BASE"&Import", K_NOKEY, 0);
      ed->AddShortcut(this, E_CAMPATH_DRAW_IN_PLAY, MENU_BASE"&Draw path when play", K_NOKEY, 0);
      //ed->AddShortcut(this, E_CAMPATH_CLOSE, MENU_BASE"&Close edit mode\tCtrl+Shift+Alt+F8", K_F8, SKEY_CTRL|SKEY_SHIFT|SKEY_ALT);
      /*
      ed->AddShortcut(this, E_CAMPATH_PLAY_TOGGLE, MENU_BASE"Play camera path\tGrey*", K_GREYMULT, 0);
      ed->AddShortcut(this, E_CAMPATH_FORWARD, MENU_BASE"Camera forward\t(Shift+)Grey+", K_GREYPLUS, 0);
      ed->AddShortcut(this, E_CAMPATH_FFORWARD, NULL, K_GREYPLUS, SKEY_SHIFT);
      ed->AddShortcut(this, E_CAMPATH_BACKWARD, MENU_BASE"Camera forward\t(Shift+)Grey-", K_GREYMINUS, 0);
      ed->AddShortcut(this, E_CAMPATH_FBACKWARD, NULL, K_GREYMINUS, SKEY_SHIFT);
      */

      cam = I3DCAST_CAMERA(ed->GetScene()->CreateFrame(FRAME_CAMERA));
      cam->Release();

      ed->CheckMenu(this, E_CAMPATH_DRAW_IN_PLAY, draw_in_playback);

      return true;
   }

//----------------------------

   virtual void Close(){
      //SetActivePath(-1, false);
      /*
      if(name_font){
         OsDestroyFont(name_font);
         name_font = NULL;
      }
      */
      if(hwnd_edit){
         DestroyWindow(hwnd_edit);
         hwnd_edit = NULL;
      }
   }

//----------------------------

   virtual dword Action(int id, void *context = NULL){

      switch(id){

      case E_CAMPATH_DRAW_IN_PLAY:
         draw_in_playback = !draw_in_playback;
         ed->CheckMenu(this, id, draw_in_playback);
         break;

      case E_CAMPATH_SELECT:
         {
            DialogBoxParam(GetHInstance(),
               "IDD_CAMPATH_SELECT",
               (HWND)ed->GetIGraph()->GetHWND(),
               dlgPathSelectThunk, (LPARAM)this);
         }
         break;

      case E_CAMPATH_IMPORT:
         {
            if(!ed->CanModify())
               return false;

            C_str filename;
            if(!GetBrowsePath(ed, "Import camera path - browse binary file", filename, "Missions",
               "Mission binary files (*.bin)\0*.bin\0" "All files\0*.*\0"))
               break;
            HWND hwnd = (HWND)ed->GetIGraph()->GetHWND();
                              //parse the file, collect anim names
            C_chunk ck;
            if(!ck.ROpen(filename) || (++ck != CT_BASECHUNK)){
               MessageBox(hwnd, C_fstr("Failed to open file: '%s'", (const char*)filename), "Import error", MB_OK);
               break;
            }
            C_vector<S_camera_path_data> import_anims;

            while(ck){
               switch(++ck){
               case CT_CAMERA_PATHS:
                  while(ck){
                     switch(++ck){
                     case CT_CAMERA_PATH:
                        {
                           import_anims.push_back(S_camera_path_data());
                           LoadCameraPath(ck, import_anims.back());
                        }
                        break;
                     }
                     --ck;
                  }
                  break;
               default:
                  --ck;
               }
            }
            if(!import_anims.size()){
               MessageBox(hwnd, C_fstr("No camera paths in file: '%s'", (const char*)filename), "Import error", MB_OK);
               break;
            }
                              //sort anims by name
            struct S_hlp{
               static bool cbLess(const S_camera_path_data &a1, const S_camera_path_data &a2){
                  return (a1.name < a2.name);
               }
            };
            sort(import_anims.begin(), import_anims.end(), S_hlp::cbLess);
                              //let user choose anim to import
            C_vector<char> choose_buf;
            for(dword i=0; i<import_anims.size(); i++){
               const C_str &name = import_anims[i].name;
               choose_buf.insert(choose_buf.end(), (const char*)name, (const char*)name+name.Size()+1);
            }
            choose_buf.push_back(0);

            int indx = ChooseItemFromList(ed->GetIGraph(), NULL, "Choose animation to import", &choose_buf.front());
            if(indx==-1)
               break;

            S_camera_path_data &ae = import_anims[indx];

                              //check if anim with such name already exists
            while(true){
               for(i=bezier_paths.size(); i--; ){
                  if(bezier_paths[i].name==ae.name)
                     break;
               }
               if(i==-1)
                  break;
               if(!SelectName(ed->GetIGraph(), NULL, "Enter animation name", ae.name))
                  return false;
            }
                              //add the anim into ours anims
            bezier_paths.push_back(ae);
            ed->SetModified();
                              //choose to edit it
            SetActivePath(bezier_paths.size() - 1);
         }
         break;

      case E_CAMPATH_SET_ACTIVE_PATH:
         {
            dword path = *(dword*)context;
            Action(E_CAMPATH_CLOSE);
            SetActivePath(path);
         }
         break;

      case E_CAMPATH_CLOSE:
         {
            if(active_path!=-1){
               SetActivePath(-1);
               PC_editor_item_Selection e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
               e_slct->RemoveFrame(cam);
            }
         }
         break;

      case E_CAMPATH_RESCALE_SPEED:
         {
            if(active_path==-1) break;
            if(!ed->CanModify()) break;
            StopPlay();

                              //ask user for scale value
            float scale = 1.0f;
            C_str str("1.0");
            while(true){
               if(!SelectName(ed->GetIGraph(), NULL, "Enter scale coeficient", str))
                  return 0;
               if(sscanf(str, "%f", &scale)==1)
                  break;
            }

            S_camera_path_data &pd = bezier_paths[active_path];
            dword first_key = 0;
            dword last_key = pd.keys.size() - 1;
                              //ask user for range
            str = C_fstr("%i - %i", first_key, last_key);
            while(true){
               if(!SelectName(ed->GetIGraph(), NULL, "Enter range of keys to scale", str))
                  return 0;
               if(sscanf(str, "%u - %u", &first_key, &last_key)!=2)
                  continue;
               if(first_key <= last_key && last_key < pd.keys.size())
                  break;
            }
                              //save undo
            C_vector<dword> undo_buf;
            undo_buf.push_back(first_key);
            undo_buf.push_back(last_key-first_key+1);

            for(dword i = first_key; i<last_key+1; i++){
               S_cam_path_key &key = pd.keys[i];
               undo_buf.push_back(I3DFloatAsInt(key.speed));
               key.speed *= scale;
            }
            e_undo->Save(this, E_CAMPATH_SET_SPEEDS, &undo_buf.front(), undo_buf.size()*sizeof(dword));
            ed->SetModified();
            ed->Message(C_fstr("Speed of %i keys rescaled by coeficient %.2f.", last_key-first_key+1, scale));
            RedrawKeyData();
         }
         break;

      case E_CAMPATH_SET_SPEEDS:
         {
            S_rescale_undo *ubuf = (S_rescale_undo*)context;
            C_vector<dword> undo_buf;
            undo_buf.push_back(ubuf->first_key);
            undo_buf.push_back(ubuf->num_keys);
            S_camera_path_data &pd = bezier_paths[active_path];
            for(dword i = 0; i<ubuf->num_keys; i++){
               S_cam_path_key &key = pd.keys[ubuf->first_key + i];
               undo_buf.push_back(I3DFloatAsInt(key.speed));
               key.speed = ubuf->speeds[i];
            }
            e_undo->Save(this, E_CAMPATH_SET_SPEEDS, &undo_buf.front(), undo_buf.size()*sizeof(dword));
            ed->SetModified();
            ed->Message(C_fstr("Speed of %i keys restored.", ubuf->num_keys));
            RedrawKeyData();
         }
         break;

      case E_CAMPATH_ACTIVATE_EDIT_WIN:
         if(active_path!=-1){
            ed->Message("Activating campath editor");
            e_props->ShowSheet(hwnd_edit);
            e_props->Activate();
         }
         break;

      case E_CAMPATH_CREATE_KEY:
      case E_CAMPATH_CREATE_KEY1:
         {
            if(active_path==-1) break;
            StopPlay();
            if(!ed->CanModify()) break;
            e_undo->Save(this, E_CAMPATH_SET_ACTIVE_KEY, &active_key, sizeof(dword));

            const S_create_undo *cd = (S_create_undo*)context;
            CreateKey(cd);
            ed->Message(C_fstr("Created new key on index %i", active_key));
         }
         break;

      case E_CAMPATH_DELETE_KEY:
      case E_CAMPATH_DELETE_KEY1:
         {
            StopPlay();
            if(!ed->CanModify()) break;

            dword del_index = active_key;
            if(id==E_CAMPATH_DELETE_KEY1)
               del_index = *(dword*)context;
            S_camera_path_data &pd = bezier_paths[active_path];

            if(pd.keys.size()==1){
               ed->Message("Cannot delete last key!");
               break;
            }
                              //save undo
            const S_cam_path_key &key = pd.keys[del_index];
            dword save_size = sizeof(S_create_undo) + key.notes[0].text.Size();
            S_create_undo *cd = (S_create_undo*)new byte[save_size];
            cd->index = del_index;
            cd->pos = key.pos;
            cd->dir = key.dir;
            cd->up = key.up;
            /*
            cd->easy_to = key.easy_to;
            cd->easy_from = key.easy_from;
            cd->smooth = key.smooth;
            */
            cd->easiness = key.easiness;
            cd->speed = key.speed;
            cd->pause = key.pause;
            strcpy(cd->note, key.notes[0].text);
            e_undo->Save(this, E_CAMPATH_CREATE_KEY1, cd, save_size);
            delete[] (byte*)cd;
               
            pd.keys.erase(pd.keys.begin()+del_index);
            SetActiveKey(Min(active_key, (dword)pd.keys.size()-1), true);
            ed->Message(C_fstr("Deleted key on index %i", del_index));
            ed->SetModified();
         }
         break;

      case E_CAMPATH_RENAME_ANIM:
      case E_CAMPATH_SET_NOTE:
         {
            StopPlay();

            if(!ed->CanModify()) break;
            const char *new_name = (const char*)context;
            S_camera_path_data &pd = bezier_paths[active_path];
            S_cam_path_key &key = pd.keys[active_key];
            C_str *str;
            switch(id){
            case E_CAMPATH_RENAME_ANIM: str = &pd.name; break;
            case E_CAMPATH_SET_NOTE: str = &key.notes[0].text; break;
            default: assert(0); str = NULL;
            }
            e_undo->Save(this, id, (void*)(const char*)(*str), str->Size()+1);
            *str = new_name;
            ed->SetModified();
            switch(id){
            case E_CAMPATH_RENAME_ANIM:
               ed->Message(C_fstr("Renaming current camera path to '%s'", new_name));
               InitEditWindowData();
               break;
            case E_CAMPATH_SET_NOTE:
               ed->Message(C_fstr("Setting note string to '%s'", new_name));
               RedrawKeyData();
               break;
            }
         }
         break;

      case E_CAMPATH_MODIFY_NOTIFY:
         {
            if(!ed->CanModify(true))
               break;
            if(run)
               break;
            pair<PI3D_frame, dword> *p = (pair<PI3D_frame, dword>*)context;
            if(p->first==cam){
               dword flags = p->second;
               if(flags&(E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION))
                  cam_dirty = true;
            }
         }
         break;

      case E_CAMPATH_PLAY_TOGGLE:
         {
            if(active_path==-1) break;
            run = !run;
            if(run){
                              //set proper position within the path
               if(!cam_path->SetKeyPosition(active_key)){
                  run = false;
                  break;
               }
            }else{
               SetActiveKey(cam_path->GetCurrentKey(), true);
            }
            assert(hwnd_edit);
            SetDlgItemText(hwnd_edit, IDC_PLAY_TOGGLE, run ? "Stop p&lay" : "P&lay");
            ed->Message(C_fstr("%s camera path", run ? "Playing" : "Stopping"));
         }
         break;

      case E_CAMPATH_EASINESS_INC:
      case E_CAMPATH_EASINESS_DEC:
         /*
      case E_CAMPATH_SMOOTH_INC:
      case E_CAMPATH_SMOOTH_DEC:
      case E_CAMPATH_EASY_TO_INC:
      case E_CAMPATH_EASY_TO_DEC:
      case E_CAMPATH_EASY_FROM_INC:
      case E_CAMPATH_EASY_FROM_DEC:
      */
      case E_CAMPATH_SPEED_INC:
      case E_CAMPATH_SPEED_DEC:
         {
            if(!ed->CanModify()) break;
            if(active_path==-1) break;
            S_camera_path_data &pd = bezier_paths[active_path];
            S_cam_path_key &key = pd.keys[active_key];

            float *val;
            float delta = .1f;
            dword undo_id;
            float clip_min = 0.0f;
            float clip_max = 1.0f;
            switch(id){
            case E_CAMPATH_EASINESS_DEC: delta = -delta;   //flow...
            case E_CAMPATH_EASINESS_INC: val = &key.easiness; undo_id = E_CAMPATH_SET_EASINESS; break;
               /*
            case E_CAMPATH_SMOOTH_DEC: delta = -delta;   //flow...
            case E_CAMPATH_SMOOTH_INC: val = &key.smooth; undo_id = E_CAMPATH_SET_SMOOTH; break;
            case E_CAMPATH_EASY_TO_DEC: delta = -delta;  //flow...
            case E_CAMPATH_EASY_TO_INC: val = &key.easy_to; undo_id = E_CAMPATH_SET_EASY_TO; break;
            case E_CAMPATH_EASY_FROM_DEC: delta = -delta;//flow...
            case E_CAMPATH_EASY_FROM_INC: val = &key.easy_from; undo_id = E_CAMPATH_SET_EASY_FROM; break;
               */
            case E_CAMPATH_SPEED_DEC: delta = -delta;     //flow...
            case E_CAMPATH_SPEED_INC: delta *= 10.0f; clip_max = 1e+5f; val = &key.speed; undo_id = E_CAMPATH_SET_SPEED; break;
            default: assert(0); val = NULL; undo_id = 0;
            }
            float f = Max(clip_min, Min(clip_max, *val + delta));
            if(*val != f){
                              //save undo, if not yet
               bool undo_on = (!e_undo->IsTopEntry(this, undo_id));
               if(undo_on)
                  e_undo->Save(this, undo_id, val, sizeof(float));
               *val = f;
               ed->SetModified();
               RedrawKeyData();
               ed->Message(C_fstr("Changing data of active key to %.2f", f));
            }
         }
         break;

      case E_CAMPATH_TYPE_NOTE:
         {
            if(!ed->CanModify()) break;
            if(active_path==-1) break;
            S_camera_path_data &pd = bezier_paths[active_path];
            S_cam_path_key &key = pd.keys[active_key];
            C_str str = key.notes[0].text;
            if(!SelectName(ed->GetIGraph(), NULL, C_fstr("Enter note string for key %i", active_key), str))
               break;
                              //save undo
            e_undo->Save(this, E_CAMPATH_SET_NOTE,
               (void*)(const char*)key.notes[0].text, key.notes[0].text.Size()+1);
                              //store new key
            key.notes[0].text = str;
            ed->SetModified();
            RedrawKeyData();
            ed->Message(C_fstr("Setting note string to '%s'", (const char*)str));
         }
         break;

         /*
      case E_CAMPATH_FORWARD:
      case E_CAMPATH_BACKWARD:
      case E_CAMPATH_FFORWARD:
      case E_CAMPATH_FBACKWARD:
         {
            if(active_path==-1) break;
            run = false;
            bool forward = (id==E_CAMPATH_FORWARD || id==E_CAMPATH_FFORWARD);
            bool fast = (id==E_CAMPATH_FFORWARD || id==E_CAMPATH_FBACKWARD);
            TickBezierPath((!fast ? 100 : 2000) * (forward ? 1 : -1));
         }
         break;

      case E_CAMPATH_HOME:
      case E_CAMPATH_END:
         cam_path->SetPosition((id==E_CAMPATH_HOME) ? 0 : -1);
         break;
         */

      case E_CAMPATH_PREV_KEY:
      case E_CAMPATH_NEXT_KEY:
      case E_CAMPATH_FIRST_KEY:
      case E_CAMPATH_LAST_KEY:
         {
            if(StopPlay()) break;
            if(active_path==-1) break;
            const S_camera_path_data &pd = bezier_paths[active_path];
            int k = active_key;
            const char *msg = NULL;
            switch(id){
            case E_CAMPATH_PREV_KEY: k = Max(0, k-1); msg = "previous"; break;
            case E_CAMPATH_NEXT_KEY: k = Min((int)pd.keys.size()-1, k+1); msg = "next"; break;
            case E_CAMPATH_FIRST_KEY: k = 0; msg = "first"; break;
            case E_CAMPATH_LAST_KEY: k = pd.keys.size()-1; msg = "last"; break;
            default: assert(0);
            }
            ed->Message(C_fstr("Campath: going to key #%i", k));
            if(active_key!=(dword)k){
               e_undo->Save(this, E_CAMPATH_SET_ACTIVE_KEY, &active_key, sizeof(dword));
               SetActiveKey(k);
            }
         }
         break;

      case E_CAMPATH_SET_ACTIVE_KEY:
         {
            StopPlay();
            dword indx = *(dword*)context;
            e_undo->Save(this, E_CAMPATH_SET_ACTIVE_KEY, &active_key, sizeof(dword));
            SetActiveKey(indx);
            ed->Message(C_fstr("Setting active key to index %i", indx));
         }
         break;

         /*
      case E_CAMPATH_SET_EASY_FROM:
      case E_CAMPATH_SET_EASY_TO:
      case E_CAMPATH_SET_SMOOTH:
      */
      case E_CAMPATH_SET_EASINESS:
      case E_CAMPATH_SET_SPEED:
         {
            StopPlay();
            assert(hwnd_edit && active_path!=-1);
            const float f = *(float*)context;
            S_camera_path_data &pd = bezier_paths[active_path];
            S_cam_path_key &key = pd.keys[active_key];
            float *val;
            switch(id){
               /*
            case E_CAMPATH_SET_EASY_FROM: val = &key.easy_from; break;
            case E_CAMPATH_SET_EASY_TO: val = &key.easy_to; break;
            case E_CAMPATH_SET_SMOOTH: val = &key.smooth; break;
            */
            case E_CAMPATH_SET_EASINESS: val = &key.easiness; break;
            case E_CAMPATH_SET_SPEED: val = &key.speed; break;
            default: assert(0); val = NULL;
            }
                              //save undo
            e_undo->Save(this, id, val, sizeof(float));
            *val = f;
            RedrawKeyData();
            ed->Message("Changing parameter of active key.");
         }
         break;

      case E_CAMPATH_SET_NOTE_POS:
         {
            StopPlay();
            assert(hwnd_edit && active_path!=-1);
            S_camera_path_data &pd = bezier_paths[active_path];
            S_cam_path_key &key = pd.keys[active_key];
            struct S_undo{
               dword note_index;
               float pos;
            } *undo = (S_undo*)context;
            swap(key.notes[undo->note_index].pos, undo->pos);
                              //save undo
            e_undo->Save(this, id, undo, sizeof(S_undo));
            RedrawKeyData();
            ed->Message("Changing parameter of active key.");
         }
         break;

      case E_CAMPATH_SET_PAUSE:
         {
            StopPlay();
            assert(hwnd_edit && active_path!=-1);
            const dword t = *(dword*)context;
            S_camera_path_data &pd = bezier_paths[active_path];
            S_cam_path_key &key = pd.keys[active_key];
                              //save undo
            e_undo->Save(this, id, &key.pause, sizeof(dword));
            key.pause = t;
            RedrawKeyData();
            ed->Message(C_fstr("Setting pause time of active key to %i", t));
         }
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(cam_dirty){
         if(active_path!=-1){
            S_camera_path_data &pd = bezier_paths[active_path];
            S_cam_path_key &key = pd.keys[active_key];
            const S_matrix &m = cam->GetMatrix();
            key.pos = m(3);
            key.dir = S_normal(m(2));
            key.up = S_normal(m(1));
            ed->SetModified();
                              //flash red "REC", so that user knows he's animating
            dword color = 0xffff0000;
            if(ed->GetIGraph()->ReadTimer()&0x100)
               color = 0x40ff0000;
            ShowNotifyText("REC", .95f, .73f, .027f, color);
         }
         cam_dirty = false;
      }
      if(notify_text){
         if((notify_text_count -= time) <= 0)
            notify_text = NULL;
      }

      if(!ed->IsActive() || active_path==-1)
         return;

      if(run){
         float time_scale = e_timescale->GetScale();
         static float time_rest;
         float scaled_time = (float)time * time_scale + time_rest;
         time_rest = (float)fmod(scaled_time, 1.0f);
         time = FloatToInt((float)floor(scaled_time));

         TickBezierPath(run ? time : 0);
         if(run && cam_path->IsAtEnd()){
            run = false;
            SetActiveKey(bezier_paths[active_path].keys.size() - 1);
            ed->Message("Camera animation finished.");
         }
      }
   }

//----------------------------

   virtual void Render(){

      if(!ed->IsActive() || active_path==-1)
         return;
      if(run && !draw_in_playback)
         return;
      PI3D_scene scn = ed->GetScene();

      const float scale = .5f;
      //const float max_vis_dist = 200.0f;
      //const byte alpha = 0xff;
      const S_vector &cam_dir = scn->GetActiveCamera()->GetWorldDir();

      const S_camera_path_data &pd = bezier_paths[active_path];
      cam_path->Build(pd);

      for(dword i=pd.keys.size(); i--; ){
         const S_cam_path_key &key = pd.keys[i];
         bool selected = (i==active_key && !run);

         S_vector pos = key.pos;
                              //offset slected in order to avoid z-fight
         if(selected)
            pos -= cam_dir * .01f;
         scn->DrawSprite(pos, GetIcon(),
            selected ? 0xffff0000 : 0xff808080,
            selected ? scale*1.2f : scale);
         if(selected){
            scn->DrawLine(key.pos, key.pos + key.dir, 0x80ffffff);
            scn->DrawLine(key.pos, key.pos + key.up, 0x800000ff);
         }
                              //draw key's notes
         for(dword ni=CAMPATH_MAX_NOTES_PER_KEY; ni--; ){
            const S_cam_path_key::S_note &note = key.notes[ni];
            if(note.text.Size()){
               S_vector pos;
               if(cam_path->ComputePosition(i, note.pos, &pos)){
                  pos -= cam_dir * .02f;
                  scn->DrawSprite(pos, GetIcon(), 0xff00ff00, .3f);
               }
            }
         }
      }
      cam_path->DebugRender(scn);

      cam->DebugDraw(scn);
   }

//----------------------------
#define VERSION 2

   virtual bool LoadState(C_chunk &ck){

                              //check version
      word version = VERSION-1;
      ck.Read(&version, sizeof(word));
      if(version!=VERSION)
         return false;

                              //window pos
      ck.Read(&edit_wnd_pos.valid, sizeof(bool));
      if(edit_wnd_pos.valid){
         ck.Read(&edit_wnd_pos.x, sizeof(int));
         ck.Read(&edit_wnd_pos.y, sizeof(int));
      }
      ck.Read(&draw_in_playback, sizeof(bool));
      ed->CheckMenu(this, E_CAMPATH_DRAW_IN_PLAY, draw_in_playback);
      /*
      bool _vis = false;
      ck.Read(&_vis, sizeof(_vis));
      ck.ReadString(CT_NAME, curr_bezier_path);

      SetVisible(_vis);
      */
      return true;
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

                              //write version
      word version = VERSION;
      ck.Write(&version, sizeof(word));

      ck.Write(&edit_wnd_pos.valid, sizeof(bool));
      if(edit_wnd_pos.valid){
         ck.Write(&edit_wnd_pos.x, sizeof(int));
         ck.Write(&edit_wnd_pos.y, sizeof(int));
      }
      ck.Write(&draw_in_playback, sizeof(bool));
      return true;
   }
};

//----------------------------

void InitCamPathPlugin(PC_editor editor){
   C_edit_camerapath *em = new C_edit_camerapath; editor->InstallPlugin(em); em->Release();
}

//----------------------------
