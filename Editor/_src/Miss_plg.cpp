#include "all.h"
#include "..\..\Source\IEditor\resource.h"
#include <insanity\SourceControl.h>
#include "game_cam.h"
#include "GameMission.h"
#include "winstuff.h"
#include "loader.h"
#include "common.h"
#include <Insanity\3DTexts.h>
//#include "script.h"

#include <windows.h>
#include <commctrl.h>

//#define SAFE_FRAME 40         //draw safe frame with specified size

#define NUM_FAVORITES 8
#define MODIFY_CHANGE_COUNT 5000 //counter how often we check for outside-editor changes to binary

//----------------------------


#ifdef EDITOR

//----------------------------
                              //editor plugin: Mission
class C_edit_Mission: public C_editor_item_Mission{
   virtual const char *GetName() const{ return "Mission"; }

//----------------------------

   /*
   enum{
      MS_ASK_SAVE = 0x1,
      MS_BUILD_BSP = 0x2,
   };
   */
                                 //Plugin: "Mission"
                                 //Dependence list: 
                                 //Purpose: open/close edited scene
   enum E_ACTION_MISSION_CONT{
      E_MISSION_OPEN,
      E_MISSION_SAVE,            //(dword save flags(MS_? defined above)); ret: (bool)success
      E_MISSION_RELOAD,
      E_MISSION_MERGE,
      E_MISSION_LOG_TOGGLE,
      E_MISSION_PROPERTIES,      
      E_MISSION_AUTOSAVE_CONFIG,
      E_MISSION_SAVE_ASK_TOGGLE, // - togle save query messagebox

      E_MISSION_ACTOR_CREATE_EDIT,  // - create actor on selection, or edit their tables
      E_MISSION_ACTOR_DESTROY,   // - create actor on selection
      E_MISSION_ACTOR_DRAW_NAMES,// - toggle draw actor's names

      E_MISSION_GCAM_DIST_P,     
      E_MISSION_GCAM_DIST_N,
      //E_MISSION_GCAM_ANGLE_P,
      //E_MISSION_GCAM_ANGLE_N,
      E_MISSION_GCAM_ACQUIRE,    //toggle game camera acquirement
      E_MISSION_GCAM_IS_ACQUIRED,//ret: bool; check if cam is acquired
      //E_MISSION_GCAM_DIST_SET,   //(float)
      //E_MISSION_GCAM_DIST_GET,   //ret: float
      //E_MISSION_GCAM_ANGLE_GET,  //(dword)
      //E_MISSION_GCAM_ANGLE_SET,  //ret: dword

      E_MISSION_SET_MODEL,       //(const char *mission_name) - set model for preview

      E_MISSION_MODE_EDIT,       //go to edit mode
      E_MISSION_MODE_GAME,       //go to game mode
      E_MISSION_MODE_NET_GAME,   //go to network game mode
      //E_MISSION_LOCK_EDIT,       //lock editing

      E_MISSION_RT_ERR_MESSAGE,  //(const char*) - write run-time error message

      E_MISSION_SET_ACTOR,       // - internal (undo)
      E_MISSION_SET_TABLE,       // - internal (undo)

      E_MISSION_GET,
      E_MISSION_CHECK_IN,
      E_MISSION_CHECK_OUT,       //ret: 0=false, 1=ok, 2=ok and reloaded
      E_MISSION_SMART_RELOAD,    //when reloading scene, keep objects (textures) in memory to speed up loading

      E_MISSION_RELOAD_TEXTS,    //reload all texts

      E_MISSION_PROFILE_NO,      //disable profiling
      E_MISSION_PROFILE_GAME,    //profiling game
      E_MISSION_PROFILE_ENGINE,  //profiling engine
      E_MISSION_HELP,
   };

//----------------------------

   static const dword *GetFavoriteCtrlIDs(){

      static const dword favorite_id_map[NUM_FAVORITES] = {
         IDC_FAV0, IDC_FAV1, IDC_FAV2, IDC_FAV3, IDC_FAV4, IDC_FAV5, IDC_FAV6, IDC_FAV7
      };
      return favorite_id_map;
   }

//----------------------------

   static bool GetFileName(const char *title, const char *filter,
      const char *def_ext, char *buf1, dword bufsize, const char *init_dir){

#pragma comment(lib, "comdlg32.lib")

      OPENFILENAME on;
      memset(&on, 0, sizeof(on));
      on.lStructSize = sizeof(on);
      on.hwndOwner = (HWND)igraph->GetHWND();
      on.lpstrFilter = filter;
      on.nFilterIndex = 1;
      on.lpstrFile = buf1;
      buf1[0] = 0;
      on.nMaxFile = bufsize;
      on.lpstrInitialDir = init_dir;
      on.lpstrTitle = title;
      on.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
      on.lpstrDefExt = def_ext;
      return (GetOpenFileName(&on)!=0);
   }

//----------------------------

   C_str mission_name;
   C_str last_mission_name;

   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_MouseEdit> e_mouseedit;
   
   bool make_log;
   bool ask_to_save;
   bool smart_reload;
   bool debug_cam_acq;
   bool enabled;              //mission I/O enabled (may be false if previewing model)
   C_str favorites[NUM_FAVORITES];
   bool edit_lock;
   int modify_change_countdown;

   float gcam_dist;
   //dword gcam_angle;

                              //flags for merging:
   dword merge_load;
   dword merge_erase;

   C_smart_ptr<C_game_mission> mission;

                              //actor table editing:
   const C_table_template *edited_template;
   C_vector<C_smart_ptr_1<I3D_frame> > multiple_edited_frames;
   HWND hwnd_tab_edit;

//----------------------------

   dword GraphCallback(dword msg, dword par1, dword par2){

      switch(msg){
      case CM_ACTIVATE:
         if(par1){
            modify_change_countdown = 0;
         }
         break;
      }
      return 0;
   }

   static dword I3DAPI GraphCallback_thunk(dword msg, dword par1, dword par2, void *context){

      C_edit_Mission *ei = (C_edit_Mission*)context;
      return ei->GraphCallback(msg, par1, par2);
   }

//----------------------------

   static void TABAPI cbTable(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){

      switch(msg){

      case TCM_CLOSE:
         {
            C_edit_Mission *es = (C_edit_Mission*)cb_user;
            es->edited_template = NULL;
            es->ed->GetIGraph()->RemoveDlgHWND(es->hwnd_tab_edit);
            es->hwnd_tab_edit = NULL;
            es->ed->GetIGraph()->EnableSuspend(true);
            es->multiple_edited_frames.clear();
            memcpy(es->tab_pos_size, (LPRECT)prm2, sizeof(int)*4);
         }
         break;

      case TCM_IMPORT:
         prm2 = 0xffffffff;
                              //flow...
      case TCM_MODIFY:
         if(prm2!=-1){
            C_edit_Mission *es = (C_edit_Mission*)cb_user;
            es->ed->SetModified();
                              //update tables on frame(s)
            for(int ii = es->multiple_edited_frames.size(); ii--; ){
               PI3D_frame frm = es->multiple_edited_frames[ii];

               PS_frame_info fi = GetFrameInfo(frm);
               assert(fi && fi->actor && fi->actor->GetTable());
               PC_table dst = const_cast<PC_table>(fi->actor->GetTable());

               if(prm2==-1){
                  /*
                  for(int i=tab->NumItems(); i--; ){
                     memcpy(dst->Item(i), tab->Item(i), tab->SizeOf(i));
                  }
                  */
               }else{
                  bool is_array = (tab->ArrayLen(prm2));
                  switch(tab->GetItemType(prm2)){
                  case TE_STRING:
                     if(is_array)
                        strcpy(dst->ItemS(prm2, prm3), tab->ItemS(prm2, prm3));
                     else
                        strcpy(dst->ItemS(prm2), tab->ItemS(prm2));
                     break;
                  case TE_ENUM:
                     if(is_array)
                        dst->ItemE(prm2, prm3) = tab->ItemE(prm2, prm3);
                     else
                        dst->ItemE(prm2) = tab->ItemE(prm2);
                     break;
                  case TE_BOOL:
                     if(is_array)
                        dst->ItemB(prm2, prm3) = tab->ItemB(prm2, prm3);
                     else
                        dst->ItemB(prm2) = tab->ItemB(prm2);
                     break;
                  case TE_COLOR_VECTOR:
                     if(is_array)
                        dst->ItemV(prm2, prm3) = tab->ItemV(prm2, prm3);
                     else
                        dst->ItemV(prm2) = tab->ItemV(prm2);
                     break;
                  case TE_INT:
                     if(is_array) dst->ItemI(prm2, prm3) = tab->ItemI(prm2, prm3);
                     else dst->ItemI(prm2) = tab->ItemI(prm2);
                     break;

                  case TE_FLOAT:
                     if(is_array) dst->ItemF(prm2, prm3) = tab->ItemF(prm2, prm3);
                     else dst->ItemF(prm2) = tab->ItemF(prm2);
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

//----------------------------

   static bool AddDirs(HWND hwnd_tc, const C_str &root_dir, dword base_dir_size,
      const C_str &curr_dir, HTREEITEM &curr_i, HTREEITEM tc_parent = TVI_ROOT){

      bool has_any_mission = false;

      WIN32_FIND_DATA fd;
      HANDLE h = FindFirstFile(C_fstr("%s*.*", (const char*)root_dir), &fd);
      if(h!=INVALID_HANDLE_VALUE)
      do{
         if(fd.cFileName[0]=='.')
            continue;
         if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
            continue;
                              //make it nice - to lower case, except first letters
         for(int i=strlen(fd.cFileName); i--; )
            fd.cFileName[i] = char((!i ? toupper : tolower)(fd.cFileName[i]));

         bool has_mission = (GetFileAttributes(C_fstr("%s%s\\scene.i3d", (const char*)root_dir, fd.cFileName)) != 0xffffffff);
         if(has_mission)
            has_any_mission = true;

         C_fstr str("%s%s", (const char*)&root_dir[base_dir_size], fd.cFileName);
         bool recurse = (str.Size()<curr_dir.Size() && !strnicmp(str, curr_dir, str.Size()));

                              //add this dir
         TVINSERTSTRUCT itm;
         memset(&itm, 0, sizeof(itm));
         itm.hParent = tc_parent;
         itm.hInsertAfter = TVI_SORT;
         itm.item.mask = TVIF_TEXT;
         itm.item.pszText = fd.cFileName;
         itm.item.state = 0;
         itm.item.stateMask = 0xff;
         if(has_mission){
            itm.item.mask |= TVIF_STATE;
            itm.item.state |= TVIS_BOLD;
         }
         bool active = str.Matchi(curr_dir);
         if(recurse){
            itm.item.mask |= TVIF_STATE;
            itm.item.state |= TVIS_EXPANDED;
         }
         HTREEITEM hi = (HTREEITEM)SendMessage(hwnd_tc, TVM_INSERTITEM, 0, (LPARAM)&itm);
         if(active){
            SendMessage(hwnd_tc, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hi);
            curr_i = hi;
         }
                              // //if our item may be inside of this dir, go to sub-dir
         if(AddDirs(hwnd_tc, C_fstr("%s%s\\", (const char*)root_dir, fd.cFileName),
            base_dir_size, curr_dir, curr_i, hi)){

            has_any_mission = true;
         }else
         if(!has_mission){
                              //no missions in this branch, remove it
            SendMessage(hwnd_tc, TVM_DELETEITEM, 0, (LPARAM)hi);
         }
      }while(FindNextFile(h, &fd));
      FindClose(h);

      return has_any_mission;
   }

//----------------------------

   static bool GetFullDir(HWND hwnd, C_str &str, bool &is_valid){

      is_valid = true;
      str = NULL;
      HTREEITEM hi = (HTREEITEM)SendDlgItemMessage(hwnd, IDC_LIST1, TVM_GETNEXTITEM, TVGN_CARET, 0);
      if(hi){
         do{
            TVITEM tvi;
            char buf[MAX_PATH];
            tvi.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_STATE;
            tvi.hItem = hi;
            tvi.stateMask = 0xff;
            tvi.pszText = buf;
            tvi.cchTextMax = sizeof(buf);
            if(SendDlgItemMessage(hwnd, IDC_LIST1, TVM_GETITEM, 0, (LPARAM)&tvi)){
               if(!str.Size() && !(tvi.state&TVIS_BOLD))
                  is_valid = false;
               str = C_fstr("%s%s%s", buf, str.Size() ? "\\" : "", (const char*)str);
            }else
               return false;
         }while(hi = (HTREEITEM)SendDlgItemMessage(hwnd, IDC_LIST1, TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)hi), hi);
         return true;
      }
      return false;
   }

//----------------------------
// Process mission selection. The returned value is:
// 0 - some error occured
// 1 - mission name saved
// 2 - mission name the same
   int Clicked(HWND hwnd){

      C_str dir;
      bool is_valid;
      if(!GetFullDir(hwnd, dir, is_valid) || !is_valid)
         return false;

      if(!mission_name.Matchi(dir)){
         mission_name = dir;
         return 1;
      }
      return 2;
   }

//----------------------------

   void SetFavoriteButtonName(HWND hwnd, int i){
      const C_str &s = favorites[i];
      SetDlgItemText(hwnd, GetFavoriteCtrlIDs()[i], C_fstr("&%i. %s", i+1, s.Size() ? (const char*)s : ""));
      EnableWindow(GetDlgItem(hwnd, GetFavoriteCtrlIDs()[i]), (s.Size()!=0));
   }

//----------------------------

   struct S_dlg_init{
      C_edit_Mission *_this;
      bool load_models;
   };

//----------------------------

   BOOL dlgSelect(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      static HBITMAP curr_thumbnail;

      switch(uMsg){
      case WM_INITDIALOG:
         {
            OsCenterWindow(hwnd, (HWND)igraph->GetHWND());

            SendDlgItemMessage(hwnd, IDC_STATIC_CURR_DIR, WM_SETTEXT, 0, (LPARAM)(const char*)mission_name);
            if(last_mission_name.Size())
               SendDlgItemMessage(hwnd, IDC_LAST_MISSION, WM_SETTEXT, 0, (LPARAM)(const char*)last_mission_name);
            else
               EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OPEN_LAST), false);

            S_dlg_init *di = (S_dlg_init*)lParam;
            const char *base_dir = !di->load_models ? "Missions\\" : "Missions\\Models\\";
            HTREEITEM curr_i = NULL;
            AddDirs(GetDlgItem(hwnd, IDC_LIST1),
               base_dir,
               strlen(base_dir),
               mission_name, curr_i);
            if(curr_i)
               SendDlgItemMessage(hwnd, IDC_LIST1, TVM_ENSUREVISIBLE, 0, (LPARAM)curr_i);

            for(int i=0; i<NUM_FAVORITES; i++)
               SetFavoriteButtonName(hwnd, i);
         }
         return 1;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDC_FAV0:
         case IDC_FAV1:
         case IDC_FAV2:
         case IDC_FAV3:
         case IDC_FAV4:
         case IDC_FAV5:
         case IDC_FAV6:
         case IDC_FAV7:
            {
               bool set_mode = IsDlgButtonChecked(hwnd, IDC_FAV_SET);
               if(!set_mode){
                  for(int i=NUM_FAVORITES; i--; ){
                     if(GetFavoriteCtrlIDs()[i]==LOWORD(wParam)){
                        const C_str &fav = favorites[i];
                        if(fav.Size()){
                           if(mission_name!=fav){
                              mission_name = fav;
                              EndDialog(hwnd, 1);
                           }else{
                              EndDialog(hwnd, 0);
                           }
                        }
                        break;
                     }
                  }
                  break;
               }

               C_str dir;
               bool is_valid;
               if(!GetFullDir(hwnd, dir, is_valid) || !is_valid)
                  dir = NULL;
               for(int i=NUM_FAVORITES; i--; ){
                  if(GetFavoriteCtrlIDs()[i]==LOWORD(wParam)){
                     favorites[i] = dir;
                     SetFavoriteButtonName(hwnd, i);
                     break;
                  }
               }
               CheckDlgButton(hwnd, IDC_FAV_SET, BST_UNCHECKED);
            }
                              //flow...
         case IDC_FAV_SET:
            {
               bool down = IsDlgButtonChecked(hwnd, IDC_FAV_SET);
                              //enable all favorite windows
               for(int i=NUM_FAVORITES; i--; )
                  EnableWindow(GetDlgItem(hwnd, GetFavoriteCtrlIDs()[i]), down ? true : (favorites[i].Size()!=0));
               EnableWindow(GetDlgItem(hwnd, IDOK), !down);
               EnableWindow(GetDlgItem(hwnd, IDCANCEL), !down);
               EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_OPEN_LAST), !down);
               ShowWindow(GetDlgItem(hwnd, IDC_FAV_INFO), down ? SW_SHOW : SW_HIDE);
            }
            break;

         case IDCANCEL:
            EndDialog(hwnd, 0);
            break;
         case IDOK:
            {
               switch(Clicked(hwnd)){
               case 0:
                              //expand/collapse
                  {
                     HTREEITEM hi = (HTREEITEM)SendDlgItemMessage(hwnd, IDC_LIST1, TVM_GETNEXTITEM, TVGN_CARET, 0);
                     if(hi){
                        SendDlgItemMessage(hwnd, IDC_LIST1, TVM_EXPAND, TVE_TOGGLE, (LPARAM)hi);
                     }
                  }
                  break;
               case 1: EndDialog(hwnd, 1); break;
               case 2: EndDialog(hwnd, 2); break;
               }
            }
            break;
         case IDC_BUTTON_OPEN_LAST:
            {
               mission_name = last_mission_name;
               EndDialog(hwnd, 1);
            }
            break;
         }
         break;

      case WM_NOTIFY:
         if(wParam==IDC_LIST1){
            LPNMHDR hdr = (LPNMHDR)lParam;
            switch(hdr->code){
            case NM_DBLCLK:
               {
                  if(IsDlgButtonChecked(hwnd, IDC_FAV_SET))
                     break;
                  switch(Clicked(hwnd)){
                  case 1: 
                     EndDialog(hwnd, 1);
                     break;
                  case 2:
                     EndDialog(hwnd, 2);
                     break;
                  }
               }
               break;
            case TVN_SELCHANGED:
               {
                  C_str dir;
                  bool is_valid;
                  GetFullDir(hwnd, dir, is_valid);
                  SendDlgItemMessage(hwnd, IDC_STATIC_NEW_DIR, WM_SETTEXT, 0, (LPARAM)(const char*)dir);

                  S_dlg_init *di = (S_dlg_init*)GetWindowLong(hwnd, GWL_USERDATA);
                              //try to display thumbnail
                  if(curr_thumbnail){
                     DeleteObject(curr_thumbnail);
                     curr_thumbnail = NULL;
                  }

                  C_chunk ck;
                  try{
                     if(ck.ROpen(C_fstr("Missions\\%s%s\\scene.bin",
                        di->load_models ? "models\\" : "", (const char*)dir))){
                        if(++ck == CT_BASECHUNK){
                           while(ck){
                              if(++ck==CT_THUMBNAIL){
                                 PIImage img = igraph->CreateImage();
                                 img->Open(*ck.GetHandle(), IMGOPEN_NOREMAP | IMGOPEN_SYSMEM);

                                 RECT rc = {0, 0, 155, 116};
                                 MapDialogRect(hwnd, &rc);
                                 int sx = rc.right;
                                 int sy = rc.bottom;

                                 PIImage img1 = igraph->CreateImage();
                                 img1->Open(NULL, IMGOPEN_SYSMEM | IMGOPEN_EMPTY, sx, sy, img->GetPixelFormat());
                                 img1->CopyStretched(img);

                                 img->Release();

                                 HDC hdc = GetDC(hwnd);

                                 void *data;
                                 dword pitch;
                                 img1->Lock(&data, &pitch);

                                 BITMAPINFOHEADER bi;
                                 memset(&bi, 0, sizeof(bi));
                                 bi.biSize = sizeof(bi);
                                 bi.biWidth = sx;
                                 bi.biHeight = -sy;
                                 bi.biPlanes = 1;
                                 bi.biBitCount = word(img1->GetPixelFormat()->bytes_per_pixel * 8);
                                 bi.biCompression = BI_RGB;

                                 curr_thumbnail = CreateDIBitmap(hdc, &bi, CBM_INIT, data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
                                 ReleaseDC(hwnd, hdc);

                                 img1->Unlock();
                                 img1->Release();
                                 break;
                              }
                              --ck;
                           }
                        }
                        ck.Close();
                     }
                  }catch(const C_except &exc){
                     MessageBox(hwnd,
                        C_fstr("Failed to load mission data, the mission may be corrupted.\nDetails: %s", (const char*)exc.what()),
                        "Loading thumbnail", MB_OK);
                  }
                  SendDlgItemMessage(hwnd, IDC_THUMBNAIL, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)curr_thumbnail);
               }
               break;
            }
         }
         break;

      case WM_DESTROY:
         if(curr_thumbnail){
            DeleteObject(curr_thumbnail);
            curr_thumbnail = NULL;
         }
         break;
      }
      return 0;
   }

   static BOOL CALLBACK dlgSelectThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      S_dlg_init *di = (S_dlg_init*)GetWindowLong(hwnd, GWL_USERDATA);
      if(!di)
         return 0;
      return di->_this->dlgSelect(hwnd, uMsg, wParam, lParam);
   }

//----------------------------

   void GetDlgButtonState(HWND hwnd){

      dword ml = 0, me = 0;

      if(IsDlgButtonChecked(hwnd, IDC_MRG_SETTINGS)) ml |= OPEN_MERGE_SETTINGS;

      if(IsDlgButtonChecked(hwnd, IDC_MRG_LIGHT_L)) ml |= OPEN_MERGE_LIGHTS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_SOUND_L)) ml |= OPEN_MERGE_SOUNDS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_MODEL_L)) ml |= OPEN_MERGE_MODELS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_VOLUME_L)) ml |= OPEN_MERGE_VOLUMES;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_DUMMY_L)) ml |= OPEN_MERGE_DUMMYS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_CAMERA_L)) ml |= OPEN_MERGE_CAMERAS;
      //if(IsDlgButtonChecked(hwnd, IDC_MRG_TARGET_L)) ml |= OPEN_MERGE_TARGETS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_OCCLUDER_L)) ml |= OPEN_MERGE_OCCLUDERS;

      if(IsDlgButtonChecked(hwnd, IDC_MRG_LMAP)) ml |= OPEN_MERGE_LIGHTMAPS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_SCRIPT_L)) ml |= OPEN_MERGE_SCRIPTS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_CHECKPOINTS)) ml |= OPEN_MERGE_CHECKPOINTS;

      merge_load = ml;

      if(IsDlgButtonChecked(hwnd, IDC_MRG_LIGHT_E)) me |= OPEN_MERGE_LIGHTS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_SOUND_E)) me |= OPEN_MERGE_SOUNDS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_MODEL_E)) me |= OPEN_MERGE_MODELS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_VOLUME_E)) me |= OPEN_MERGE_VOLUMES;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_DUMMY_E)) me |= OPEN_MERGE_DUMMYS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_CAMERA_E)) me |= OPEN_MERGE_CAMERAS;
      //if(IsDlgButtonChecked(hwnd, IDC_MRG_TARGET_E)) me |= OPEN_MERGE_TARGETS;
      if(IsDlgButtonChecked(hwnd, IDC_MRG_OCCLUDER_E)) me |= OPEN_MERGE_OCCLUDERS;

      if(IsDlgButtonChecked(hwnd, IDC_MRG_SCRIPT_E)) me |= OPEN_MERGE_SCRIPTS;

      merge_erase = me;
   }


//----------------------------

   static BOOL CALLBACK dlgMerge(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            SetWindowLong(hwnd, GWL_USERDATA, lParam);
            C_edit_Mission *em = (C_edit_Mission*)lParam;
            OsCenterWindow(hwnd, (HWND)igraph->GetHWND());

            dword ml = em->merge_load;
            CheckDlgButton(hwnd, IDC_MRG_SETTINGS, bool(ml&OPEN_MERGE_SETTINGS));

            CheckDlgButton(hwnd, IDC_MRG_LIGHT_L, bool(ml&OPEN_MERGE_LIGHTS));
            CheckDlgButton(hwnd, IDC_MRG_SOUND_L, bool(ml&OPEN_MERGE_SOUNDS));
            CheckDlgButton(hwnd, IDC_MRG_MODEL_L, bool(ml&OPEN_MERGE_MODELS));
            CheckDlgButton(hwnd, IDC_MRG_VOLUME_L, bool(ml&OPEN_MERGE_VOLUMES));
            CheckDlgButton(hwnd, IDC_MRG_DUMMY_L, bool(ml&OPEN_MERGE_DUMMYS));
            CheckDlgButton(hwnd, IDC_MRG_CAMERA_L, bool(ml&OPEN_MERGE_CAMERAS));
            //CheckDlgButton(hwnd, IDC_MRG_TARGET_L, bool(ml&OPEN_MERGE_TARGETS));
            CheckDlgButton(hwnd, IDC_MRG_OCCLUDER_L, bool(ml&OPEN_MERGE_OCCLUDERS));

            CheckDlgButton(hwnd, IDC_MRG_LMAP, bool(ml&OPEN_MERGE_LIGHTMAPS));
            CheckDlgButton(hwnd, IDC_MRG_SCRIPT_L, bool(ml&OPEN_MERGE_SCRIPTS));
            CheckDlgButton(hwnd, IDC_MRG_CHECKPOINTS, bool(ml&OPEN_MERGE_CHECKPOINTS));

            dword me = em->merge_erase;
            CheckDlgButton(hwnd, IDC_MRG_LIGHT_E, bool(me&OPEN_MERGE_LIGHTS));
            CheckDlgButton(hwnd, IDC_MRG_SOUND_E, bool(me&OPEN_MERGE_SOUNDS));
            CheckDlgButton(hwnd, IDC_MRG_MODEL_E, bool(me&OPEN_MERGE_MODELS));
            CheckDlgButton(hwnd, IDC_MRG_VOLUME_E, bool(me&OPEN_MERGE_VOLUMES));
            CheckDlgButton(hwnd, IDC_MRG_DUMMY_E, bool(me&OPEN_MERGE_DUMMYS));
            CheckDlgButton(hwnd, IDC_MRG_CAMERA_E, bool(me&OPEN_MERGE_CAMERAS));
            //CheckDlgButton(hwnd, IDC_MRG_TARGET_E, bool(me&OPEN_MERGE_TARGETS));
            CheckDlgButton(hwnd, IDC_MRG_OCCLUDER_E, bool(me&OPEN_MERGE_OCCLUDERS));

            CheckDlgButton(hwnd, IDC_MRG_SCRIPT_E, bool(me&OPEN_MERGE_SCRIPTS));

            ShowWindow(hwnd, SW_SHOW);
         }
         return 1;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL:
            {
               C_edit_Mission *em = (C_edit_Mission*)GetWindowLong(hwnd, GWL_USERDATA);
               em->GetDlgButtonState(hwnd);
               EndDialog(hwnd, 0);
            }
            break;
         case IDOK:
            {
               C_edit_Mission *em = (C_edit_Mission*)GetWindowLong(hwnd, GWL_USERDATA);
               em->GetDlgButtonState(hwnd);
               EndDialog(hwnd, true);
            }
            break;
         }
         break;
      }
      return 0;
   }

//----------------------------

   C_smart_ptr<C_table> tab_auto_save;
   int auto_save_count;       

   int tab_pos_size[4];

   int last_create_actor_index;

   bool mission_open_ok;      //true if mission was loaded successfully


   struct S_name_texture{
                              //name of actor type stored in texture - for editing purposes
      C_smart_ptr<I3D_texture> tp;
                              //size of initialized part of texture
      int tp_size[2];
      S_name_texture(){}
      /*
      S_name_texture(const S_name_texture &nt){ operator =(nt); }
      S_name_texture &operator =(const S_name_texture &nt){
         tp = nt.tp;
         memcpy(tp_size, nt.tp_size, sizeof(tp_size));
         return *this;
      }
      */
   };
   map<E_ACTOR_TYPE, S_name_texture> actor_name_textures;
   void *h_font;
   bool draw_actor_names;
   int sb_rt_msg_pos;         //position in statusbar for run-time error messages

                              //property template definition
   enum{
      TAB_CV_SCN_BGND = 0,          //bgnd color
      TAB_F_SCN_CAM_EDIT_RANGE = 1, //range of camera during editing
      TAB_I_SCN_CAM_FOV = 2,        //camera FOV
      TAB_F_SCN_CAM_RANGE = 3,      //camera far range
      TAB_B_SCN_CAM_ORTHO = 4,      //orthogonal camera view
      TAB_F_SCN_CAM_ORTHO_SCL = 5,  //orthogonal camera scale
      TAB_F_SCN_BDROP_RANGE_N = 6,  //backdrop near range
      TAB_F_SCN_BDROP_RANGE_F = 7,  //backdrop far range
   };

   static CPC_table_template GetPropTemplate(){

      static const C_table_element te[] = {
         {TE_BRANCH, 0, "Camera", 5, (int)"FOV=%[0], range=%[1]", 0, "Camera setting (FOV, range, type)."},
            {TE_INT, TAB_I_SCN_CAM_FOV, "FOV", 10, 135, 0, "Field Of View (in degrees)."},
            {TE_FLOAT, TAB_F_SCN_CAM_RANGE, "Range", 0, 0, 0, "Camera visibility range (back clipping plane)."},
            {TE_FLOAT, TAB_F_SCN_CAM_EDIT_RANGE, "Edit range", 0, 0, 0, "Camera range in edit mode. Leave zero to use the same range as in-game. This is only for editing purposes."},
            {TE_BOOL, TAB_B_SCN_CAM_ORTHO, "Orthogonal", 0, 0, 0, "Switch between orthogonal or perspective view mode."},
            {TE_FLOAT, TAB_F_SCN_CAM_ORTHO_SCL, "Orthogonal scale", 0, 0, 1, "Scale of orthogonal projection. This value is used only in orthogonal mode."},
         {TE_BRANCH, 0, "Background", 2, 0, 0, "Setting of background (color, range)."},
            {TE_COLOR_VECTOR, TAB_CV_SCN_BGND,     "Background color", 0, 0, 0, "Color into which screen is cleared each frame before rendering."},
            {TE_BRANCH, 0, "Backdrop range", 2, (dword)"n: %[0], f: %[1]", 0, "Range of camera in backdrop sector. Setting this value is usabvle only if you have visuals linked to backdrop sector."},
               {TE_FLOAT, TAB_F_SCN_BDROP_RANGE_N, "near", 0, 0, 1},
               {TE_FLOAT, TAB_F_SCN_BDROP_RANGE_F, "far", 0, 0, 100},
         {TE_NULL}
      };
      static const C_table_template templ_scene_property = { "Scene properties", te };
      return &templ_scene_property;
   }

//----------------------------

   enum{
      TAB_B_AUTOSAVE_ON = 0,     //enable auto-save
      TAB_I_AUTOSAVE_TIME = 1,   //how often
      TAB_S32_AUTOSAVE_DIR = 2,  //where
   };

   static CPC_table_template GetAutoSaveTemplate(){

      static const C_table_element te[] = {
         {TE_BOOL, TAB_B_AUTOSAVE_ON, "Enable", 0, 0, 0, "Enable or disable auto-save feature."},
         {TE_INT, TAB_I_AUTOSAVE_TIME, "Time (sec)", 0, 0, 180, "Time period, how often mission will be automatically saved. This time is reset each time user saves mission manually."},
         {TE_STRING, TAB_S32_AUTOSAVE_DIR, "Directory", 32, 0, 0, "Directory, where mission will be automatically saved. This directory is relative to \"Missions\\\" directory."},
         {TE_NULL}
      };
      static const C_table_template templ_as_property = { "Auto-save properties", te };
      return &templ_as_property;
   }

//----------------------------

   enum{
      TAB_B_DEBUG_LOG_HIT,
      TAB_B_DEBUG_LOG_AI,
      TAB_B_DEBUG_LOG_SIGNAL,
      TAB_LAST
   };

   static CPC_table_template GetDebugLogTemplate(){

      static const C_table_element te[] = {
         {TE_BOOL, TAB_B_DEBUG_LOG_HIT, "Body hit", 0, 0, 0, "Log when body receives hit."},
         {TE_BOOL, TAB_B_DEBUG_LOG_AI, "AI", 0, 0, 0, "Log all AI decissions."},
         {TE_BOOL, TAB_B_DEBUG_LOG_SIGNAL, "Signals", 0, 0, 0, "Log all signals sent through script."},
         {TE_NULL}
      };
      static const C_table_template templ_property = { "Debug log items", te };
      return &templ_property;
   }

//----------------------------

   static void TABAPI cbProp(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){

      switch(msg){
      case TCM_CLOSE:
         {
            C_edit_Mission *em = (C_edit_Mission*)cb_user;
            //em->ed->GetIGraph()->DeleteDlgHWND(em->hwnd_prop);
            //em->hwnd_prop = NULL;
            memcpy(em->tab_pos_size, (LPRECT)prm2, sizeof(int)*4);
         }
         break;
      case TCM_MODIFY:
         {
            C_edit_Mission *em = (C_edit_Mission*)cb_user;
            PI3D_scene scene = em->ed->GetScene();
            switch(prm2){
            case TAB_CV_SCN_BGND:
               scene->SetBgndColor(tab->ItemV(TAB_CV_SCN_BGND));
               scene->Render(); em->ed->GetIGraph()->UpdateScreen();
               em->ed->SetModified();
               break;
            case TAB_I_SCN_CAM_FOV:
            case TAB_F_SCN_CAM_RANGE:
            case TAB_F_SCN_CAM_EDIT_RANGE:
            case TAB_B_SCN_CAM_ORTHO:
            case TAB_F_SCN_CAM_ORTHO_SCL:
               {
                  switch(prm2){
                  case TAB_I_SCN_CAM_FOV:
                     em->mission->cam_fov = tab->ItemI(TAB_I_SCN_CAM_FOV)*PI/180.0f;
                     break;
                  case TAB_F_SCN_CAM_RANGE:
                     em->mission->cam_range = tab->ItemF(TAB_F_SCN_CAM_RANGE);
                     break;
                  case TAB_F_SCN_CAM_EDIT_RANGE:
                     em->mission->cam_edit_range = tab->ItemF(TAB_F_SCN_CAM_EDIT_RANGE);
                     break;
                  }
                  PI3D_camera cam = scene->GetActiveCamera();
                  if(cam){
                     float r = em->mission->cam_edit_range;
                     if(!r)
                        r = em->mission->cam_range;
                     cam->SetRange(CAM_NEAR_RANGE, r);
                     cam->SetFOV(em->mission->cam_fov);
                     cam->SetOrthogonal(tab->ItemB(TAB_B_SCN_CAM_ORTHO));
                     cam->SetOrthoScale(tab->ItemF(TAB_F_SCN_CAM_ORTHO_SCL));
                     em->ed->SetModified();
                  }
                  scene->Render(); em->ed->GetIGraph()->UpdateScreen();
               }
               break;
            case TAB_F_SCN_BDROP_RANGE_N:
            case TAB_F_SCN_BDROP_RANGE_F:
               {
                  scene->SetBackdropRange(tab->ItemF(TAB_F_SCN_BDROP_RANGE_N), tab->ItemF(TAB_F_SCN_BDROP_RANGE_F));
                  em->ed->SetModified();
                  scene->Render(); em->ed->GetIGraph()->UpdateScreen();
               }
               break;
            }
            em->ed->SetModified();
         }
         break;
      }
   }
   
   C_str FormatLoadTime(int save_time){
      return C_fstr("Scene loaded in %.2f seconds",
         (float)(ed->GetIGraph()->ReadTimer()-save_time)/1000.0f);
   }

   void WriteStatusMessage(E_MISSION_IO mio,
      const char *fail_msg, const char *ok_msg, bool allow_msgbox = true){

      C_str str;
      switch(mio){
      case MIO_OK:
         str = ok_msg;
         //ed->GetIGraph()->SetAppName(C_fstr("%s - %s", (const char*)app_name, (const char*)mission_name));
         break;
      case MIO_CORRUPT:
         str = C_fstr("%s: file corrupted", (fail_msg));
         break;
      case MIO_NOFILE:
         str = C_fstr("%s: file not found", (fail_msg));
         break;
      case MIO_ACCESSFAIL:
         str = C_fstr("%s: can't read or write file", (fail_msg));
         break;
      case MIO_CANCELLED:
         str = C_fstr("%s: cancelled", (fail_msg));
         break;
      default: assert(0);
      }
      if(mio!=MIO_OK && allow_msgbox)
         MessageBox((HWND)ed->GetIGraph()->GetHWND(), str, "Mission open/write", MB_OK);
      ed->Message(str);
   }

//----------------------------

   void DrawActorName(const C_actor *act){

      E_ACTOR_TYPE type = act->GetActorType();
      map<E_ACTOR_TYPE, S_name_texture>::iterator it = actor_name_textures.find(type);
      if(it==actor_name_textures.end()){
                              //init texture
         if(!h_font)
            h_font = OsCreateFont(12);
         actor_name_textures[type] = S_name_texture();
         it = actor_name_textures.find(type);

         PI3D_texture tp = CreateStringTexture(actor_type_info[type].friendly_name,
            (*it).second.tp_size, ed->GetDriver(), h_font);
         (*it).second.tp = tp;
         tp->Release();
      }
      const S_name_texture &nt = (*it).second;

                              //find (or create) texture associated with actor
      CPI3D_texture tp = nt.tp;
      assert(tp);
      DrawStringTexture(act->GetFrame(), 0.0f, ed->GetScene(), tp, nt.tp_size, 0x68ff80ff, 20, 30, 2.0f, .12f);
   }

//----------------------------

   bool DestroyActor(PI3D_frame frm){

      const S_frame_info *fi = GetFrameInfo(frm);
      if(fi && fi->actor){

                              //save table contens from script
         CPC_table tab = fi->actor->GetTable();
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
            dword new_data_len = frm->GetName().Size() + 1 + TabDataLen + sizeof(size_t);
            char *pData = new char[new_data_len];

            assert(pData);

            strcpy(pData, frm->GetName());
            memcpy(pData+frm->GetName().Size()+1, &TabDataLen, sizeof(size_t));
            memcpy(pData+frm->GetName().Size()+1+sizeof(size_t), pTabData, TabDataLen);

            e_undo->Save(this, E_MISSION_SET_TABLE, pData, new_data_len);
            delete [] pData;
            tab_cache.FreeMem(pTabData);
         }

         {           //save undo info
         }

         mission->DestroyActor(fi->actor);
         return true;
      }
      return false;
   }

//----------------------------

   PC_actor CreateActor(PI3D_frame frm, E_ACTOR_TYPE at){

      PS_frame_info fi = GetFrameInfo(frm);
      if(e_undo){
                     //save undo info
      }
      if(fi && fi->actor)
         mission->DestroyActor(fi->actor);

      return mission->CreateActor(frm, at);
   }

//----------------------------

   void SetupCameraRange(){

      float r = 0.0f;
      if(!mission->IsInGame())
         r = mission->cam_edit_range;
      if(!r)
         r = mission->cam_range;
      PI3D_camera cam = ed->GetScene()->GetActiveCamera();
      if(cam)
         cam->SetRange(CAM_NEAR_RANGE, r);
   }

//----------------------------
// Check BSP tree status. Return value: false ... canceled, true ... built or user doesn't want to build
   bool CheckBSPTree(){

      if(!ed->GetScene()->IsBspBuild()){

         static const char *restricted_dir[] = { "models*", "anims*", "system*", NULL };
         C_str miss_name = mission->GetName();
         miss_name.ToLower();
         for(dword i1 = 0; restricted_dir[i1]; i1++){
            if(miss_name.Match(restricted_dir[i1]))
               return true;
         }
         int id = OsMessageBox(ed->GetIGraph()->GetHWND(), "BSP tree is not built, do you want to do it now?",
            "Checking bsp.", MBOX_YESNOCANCEL);
         if(id==MBOX_RETURN_CANCEL)
            return false;
         if(id==MBOX_RETURN_YES){
            PC_editor_item_BSPTool ei = (PC_editor_item_BSPTool)ed->FindPlugin("BSPTool");
            if(!ei){
               ed->Message("Failed to locate BPS build plugin");
               return false;
            }
            if(!ei->Create())
               return false;
         }
      }
      return true;
   }

//----------------------------

   virtual bool BrowseMission(void *hwnd, C_str &mn) const{

      C_str save_name = mission_name;
      C_str save_name1 = last_mission_name;
      const_cast<C_edit_Mission*>(this)->mission_name = mn;
      const_cast<C_edit_Mission*>(this)->last_mission_name = NULL;
      S_dlg_init di = {const_cast<C_edit_Mission*>(this), true};
      int i = DialogBoxParam(GetModuleHandle(NULL), "IDD_OPEN_MISSION",
         (HWND)hwnd, dlgSelectThunk, (LPARAM)&di);
      if(i)
         mn = mission_name;
      const_cast<C_edit_Mission*>(this)->mission_name = save_name;
      const_cast<C_edit_Mission*>(this)->last_mission_name = save_name1;
      return i;
   }

//----------------------------

   virtual bool Save(bool ask, bool ask_bsp_build = false, const char *override_name = NULL){

      if(!enabled)
         return true;
      if(edit_lock){
         if(!ask)
            MessageBox((HWND)ed->GetIGraph()->GetHWND(), "Scene is locked - cannot save.", "Mission editor", MB_OK);
         return true;
      }
      if(!ask_to_save && !ask)
         return true;

      if(ask_bsp_build){
         if(!CheckBSPTree())
            return false;
      }

      if(ed->IsModified() || !ask){
         bool ok = false;
         if(ask){
            switch(OsMessageBox(ed->GetIGraph()->GetHWND(), "Scene modified. Save changes?", "", MBOX_YESNOCANCEL)){
            case MBOX_RETURN_YES:
               ok = true;
               break;
            case MBOX_RETURN_CANCEL:
               return false;
            }
         }else
            ok = true;

         if(ok){
            E_MISSION_IO mio = mission->Save(mission_name, 0);
            WriteStatusMessage(mio, "Save failed", "Scene saved");
            ok = (mio==MIO_OK);
         }
      }
      if(ed->IsModified())
         ed->SetModified(false);
      return true;
   }

//----------------------------

   virtual const C_str &GetMissionName() const{

      return mission->GetName();
   }

//----------------------------

   virtual void EditLock(bool b = true){
      if(edit_lock != b){
         edit_lock = b;
                           //make lock flag appear in statusbar
         ed->CanModify(true);
      }
   }
   virtual bool IsEditLocked() const{ return edit_lock; }

//----------------------------

   virtual class C_game_mission *GetMission(){ return mission; }
   virtual void SetViewModel(const C_str &name){

      enabled = false;
      ed->EnableMenu(this, E_MISSION_OPEN, enabled);
      ed->EnableMenu(this, E_MISSION_SAVE, enabled);
      ed->EnableMenu(this, E_MISSION_RELOAD, enabled);
      ed->EnableMenu(this, E_MISSION_MERGE, enabled);

      PI3D_scene scn = ed->GetScene();

      mission->InitializeLoadProgress();
      scn->Open(name, I3DLOAD_PROGRESS | I3DLOAD_ERRORS, mission->cbLoadScene, mission);
      mission->CloseLoadProgress();
                        //setup lighting
      for(int i=0; i<2; i++){
         PI3D_light lp = I3DCAST_LIGHT(scn->CreateFrame(FRAME_LIGHT));
         if(!i){
                              //directional
            lp->SetLightType(I3DLIGHT_DIRECTIONAL);
            lp->SetDir(S_vector(1, -1, 1), 0.0);
            lp->SetColor(S_vector(1, 1, 1));
         }else{
                              //ambient
            lp->SetLightType(I3DLIGHT_AMBIENT);
            lp->SetColor(S_vector(.5, .5, .5));
         }
         lp->SetMode(I3DLIGHTMODE_VERTEX | I3DLIGHTMODE_DYNAMIC_LM);
         scn->GetPrimarySector()->AddLight(lp);
         scn->AddFrame(lp);
         lp->Release();
      }
      scn->SetBgndColor(S_vector(.3f, .4f, .55f));
                        //setup camera to see entire model

      PI3D_camera cam = e_mouseedit->GetEditCam();
      scn->SetActiveCamera(cam);

      I3D_bound_volume bvol;
      scn->GetPrimarySector()->ComputeHRBoundVolume(&bvol);
      const I3D_bbox &bbox = bvol.bbox;
      float cam_dist;
      S_vector cam_look_at;
      float diag_size = 0.0f;
      if(bbox.IsValid()){
         S_vector diag = bbox.max - bbox.min;
         cam_look_at = bbox.min + diag*.5f;
         diag_size = diag.Magnitude();
         cam_dist = diag_size * .6f;
         cam_dist = Max(cam_dist, 2.0f);
      }else{
         cam_look_at.Zero();
         cam_dist = 0.0f;
      }
      S_vector pos = cam_look_at + S_vector(.8f, .4f, -.8f) * cam_dist;
      float nr, fr;
      cam->GetRange(nr, fr);
      fr = Max(200.0f, cam_dist + diag_size * .6f);
      cam->SetRange(CAM_NEAR_RANGE, fr);
      cam->SetFOV(PI*.4f);
      cam->SetPos(pos);
      cam->SetDir(cam_look_at - pos);
   }
   virtual void SetMissionName(const C_str &name){ mission_name = name; }

//----------------------------

   virtual bool Reload(bool ask = true){

      if(ed->IsModified() && !Save(ask))
         return false;
      int save_time = ed->GetIGraph()->ReadTimer();

      //PC_editor_item usability = ed->FindPlugin("Usability");
                              //if re-loading, keep textures to avoid reloading
      set<C_smart_ptr_1<I3D_texture> > txts;
      if(smart_reload && mission->GetName()==mission_name){
                              //collect all textures and keep refs
         PI3D_scene scn = mission->GetScene();

         struct S_hlp{
            static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
               PI3D_visual vis = I3DCAST_VISUAL(frm);
               PI3D_mesh_base mb = vis->GetMesh();
               set<C_smart_ptr_1<I3D_texture> > &txts = *(set<C_smart_ptr_1<I3D_texture> >*)c;
               if(mb){
                  CPI3D_face_group fgrps = mb->GetFGroups();
                  for(dword i=mb->NumFGroups(); i--; ){
                     const I3D_face_group &fg = fgrps[i];
                     PI3D_material mat = ((I3D_face_group&)fg).GetMaterial();
                     if(mat){
                        for(dword ti=4; ti--; ){
                           PI3D_texture txt = mat->GetTexture((I3D_MATERIAL_TEXTURE_INDEX)ti);
                           if(txt){
                              txts.insert(txt);
                           }
                        }
                     }
                  }
               }else{
                  PI3D_material mat = vis->GetMaterial();
                  if(mat){
                     for(dword ti=4; ti--; ){
                        PI3D_texture txt = mat->GetTexture((I3D_MATERIAL_TEXTURE_INDEX)ti);
                        if(txt){
                           txts.insert(txt);
                        }
                     }
                  }
               }
               return I3DENUMRET_OK;
            }
         };
         scn->EnumFrames(S_hlp::cbEnum, (dword)&txts, ENUMF_VISUAL);
         /*
         if(usability)
            usability->Action(E_USABILITY_RELOAD, (void*)(const char*)mission_name);
            */
      }else{
         /*
         if(usability)
            usability->Action(E_USABILITY_LOAD, (void*)(const char*)mission_name);
            */
      }


      E_MISSION_IO mio = mission->Open(mission_name, (make_log ? OPEN_LOG : 0));
      mission_open_ok = (mio==MIO_OK);
      if(!mission_open_ok)
         EditLock();

      WriteStatusMessage(mio, "Load failed", FormatLoadTime(save_time), false);
      if(mio == MIO_CORRUPT)
         RestoreBackup(mission_name);
      else{
         SetupCameraRange();
      }
#if SAFE_FRAME
      igraph->Rectangle(0, 0, igraph->Scrn_sx(), igraph->Scrn_sy(), 0);
      mission.GetScene()->SetViewport(SAFE_FRAME, SAFE_FRAME, igraph->Scrn_sx()-SAFE_FRAME, igraph->Scrn_sy()-SAFE_FRAME);
#endif   //SAFE_FRAME
      return mission_open_ok;
   }

//----------------------------

   virtual float GetGCamDist() const{ return gcam_dist; }

//----------------------------

   virtual void SetGCamDist(float f){ gcam_dist = f; }

//----------------------------

   virtual bool IsGCamAcquired() const{ return debug_cam_acq; }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==0){
         PI3D_scene scene = ed->GetScene();
         {
                           //scene properties
            S_vector v = scene->GetBgndColor();
            ck.WVectorChunk(CT_SCENE_BGND_COLOR, v);
            ck.WFloatChunk(CT_CAMERA_FOV, mission->GetCameraFOV());
            ck.WFloatChunk(CT_CAMERA_RANGE, mission->GetCameraRange());
            ck.WFloatChunk(CT_CAMERA_EDIT_RANGE, mission->GetCameraEditRange());

            PI3D_camera cam = scene->GetActiveCamera();
            if(cam){
               if(cam->GetOrthogonal()){
                  ck <<= CT_CAMERA_ORTHOGONAL;
                  --ck;
               }
               ck.WFloatChunk(CT_CAMERA_ORTHO_SCALE, cam->GetOrthoScale());
                                    //backdrop
               {
                  float r[2];
                  scene->GetBackdropRange(r[0], r[1]);
                  ck <<= CT_SCENE_BACKDROP;
                  ck.Write((char*)r, sizeof(r));
                  --ck;
               }
            }
         }
                           //save mission table
         ck <<= CT_MISSION_TABLE;
         mission->GetConfigTab()->Save(ck.GetHandle(), TABOPEN_FILEHANDLE);
         --ck;
      }else
      if(phase==2){
                           //save actors
         const C_vector<C_smart_ptr<C_actor> > &actors = mission->GetActorsVector();

         for(dword i=0; i<actors.size(); i++){
            const C_actor *actor = actors[i];
            E_ACTOR_TYPE at = actor->GetActorType();
                           //check if it may be written onto disk
            if(actor_type_info[at].allow_edit_creation){
               ck <<= CT_ACTOR;
               ck.WStringChunk(CT_NAME, actor->GetName());
               ck.Write((const char*)&at, sizeof(word));
                           //let actor write its data
               //actor->MissionSave(ck);

                           //write actor's table
               CPC_table tab = actor->GetTable();
               if(tab){
               //S_frame_info *fi = GetFrameInfo(actor->GetFrame());
               //if(fi->tab && actor->GetTableTemplate() && (!fi->script || !fi->script->GetTableTemplate()))
                  ck <<= CT_ACTOR_TABLE;
                  tab->Save(ck.GetHandle(), TABOPEN_FILEHANDLE);
                  --ck;
               }
               --ck;
            }
         }
      }
   }

//----------------------------

   virtual void OnFrameDelete(PI3D_frame frm){
      DestroyActor(frm);
   }

//----------------------------

   virtual void OnFrameDuplicate(PI3D_frame frm_old, PI3D_frame frm_new){

      PS_frame_info fi_old = GetFrameInfo(frm_old);
      if(fi_old && fi_old->actor){
         PC_actor actor_new = CreateActor(frm_new, fi_old->actor->GetActorType());

         CPC_table tab_old = fi_old->actor->GetTable();
         if(tab_old){
                        //copy table (if any)
            PC_table tab_new = const_cast<PC_table>(actor_new->GetTable());
                           //if they have the same scripts, the should also have both tables
            assert(tab_new);
            tab_new->Load(tab_old, TABOPEN_UPDATE | TABOPEN_DUPLICATE);
         }
      }
   }

//----------------------------

   virtual void AfterLoad(){
      auto_save_count = 0;
      modify_change_countdown = 0;
      igraph->GetTimer(1, 1);
      if(edit_lock){
         edit_lock = false;
         ed->CanModify(true);
      }
   }

//----------------------------

   virtual void BeforeFree(){

      if(hwnd_tab_edit)
         DestroyWindow(hwnd_tab_edit);
                        //always flush out model cache in editor
      model_cache.Clear();
      anim_cache.Clear();
                        //no old sins into new mission
      PC_editor_item_Log ei = (PC_editor_item_Log)ed->FindPlugin("Log");
      if(ei)
         ei->Clear();
   }

//----------------------------

   virtual void OnDeviceReset(){
                           //destroy all current name textures
      actor_name_textures.clear();
   }

//----------------------------

   virtual E_QUERY_MODIFY QueryModify(bool quiet){

                        //if locked, no editing possible
      if(edit_lock)
         return QM_NO_LOCKED;

                        //check if current mission is readonly
      C_fstr filename("missions\\%s\\scene.bin", (const char*)mission_name);
      if(OsIsFileReadOnly(filename)){
         if(!quiet){
            int i = Action(E_MISSION_CHECK_OUT);
            switch(i){
            case 0: return QM_NO_RDONLY;
            case 1: return QM_OK;
            case 2: return QM_NO_RDONLY;
            default: assert(0);
            }
         }
         return QM_NO_RDONLY;
      }
      return QM_OK;
   }

//----------------------------
public:
   C_edit_Mission(C_game_mission &m1):
      mission(&m1),
      edit_lock(false),
      mission_open_ok(false),
      debug_cam_acq(false),
      draw_actor_names(true),
      smart_reload(true),
      make_log(false),
      hwnd_tab_edit(NULL),
      enabled(true),
      modify_change_countdown(MODIFY_CHANGE_COUNT),
      h_font(NULL),
      gcam_dist(0.0f),
      //gcam_angle(1),
      merge_load(0),
      last_create_actor_index(-1),
      merge_erase(0),
      ask_to_save(true)
   {
      memset(tab_pos_size, 0, sizeof(tab_pos_size));
      mission_name = "!Test";

      tab_auto_save = CreateTable();
      tab_auto_save->Load(GetAutoSaveTemplate(), TABOPEN_TEMPLATE);
      tab_auto_save->Release();
   }

   ~C_edit_Mission(){
      if(h_font)
         OsDestroyFont(h_font);
   }

   virtual bool Init(){

      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_mouseedit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      if(!e_slct || !e_undo || !e_mouseedit)
         return false;

      ed->AddShortcut(this, E_MISSION_HELP, "%100 %i &Help\\%0 %a &Help\tF1", K_F1, 0);

#define MENU_BASE "%0 &File\\"
      ed->AddShortcut(this, E_MISSION_OPEN, MENU_BASE"%0 &Open mission\tO", K_O, 0);
      ed->AddShortcut(this, E_MISSION_SAVE, MENU_BASE"%0 &Save mission\tS", K_S, 0);
      ed->AddShortcut(this, E_MISSION_MERGE, MENU_BASE"%0 &Merge mission", K_NOKEY, 0);
      ed->AddShortcut(this, E_MISSION_RELOAD, MENU_BASE"%0 &Reload mission\tL", K_L, 0);
      ed->AddShortcut(this, E_MISSION_SAVE_ASK_TOGGLE, MENU_BASE"%0 As&k to save", K_NOKEY, 0);
      ed->AddShortcut(this, E_MISSION_SMART_RELOAD, MENU_BASE"%0 Fast reload mode", K_NOKEY, 0);
      ed->AddShortcut(this, E_MISSION_AUTOSAVE_CONFIG, MENU_BASE"%0 &Autosave config", K_NOKEY, 0);
      ed->AddShortcut(this, E_MISSION_PROPERTIES, MENU_BASE"%30 %i Scene &properties", K_NOKEY, 0);

#undef MENU_BASE
#define MENU_BASE "%30 &Debug\\"
      ed->AddShortcut(this, E_MISSION_LOG_TOGGLE, MENU_BASE"%80 %i Scene open &log", K_NOKEY, 0);

#undef MENU_BASE
#define MENU_BASE "%30 &Debug\\%70 Game camera\\"
      ed->AddShortcut(this, E_MISSION_GCAM_DIST_P, MENU_BASE"Distance mode\\Previous\t-", K_MINUS, 0);
      ed->AddShortcut(this, E_MISSION_GCAM_DIST_N, MENU_BASE"Distance mode\\Next\t=", K_EQUALS, 0);
      //ed->AddShortcut(this, E_MISSION_GCAM_ANGLE_P, MENU_BASE"Angle\\Up\tBackslash", K_BACKSLASH, 0);
      //ed->AddShortcut(this, E_MISSION_GCAM_ANGLE_N, MENU_BASE"Angle\\Down\tBackspace", K_BACKSPACE, 0);
      ed->AddShortcut(this, E_MISSION_GCAM_ACQUIRE, MENU_BASE"Acquire\tF8", K_F8, 0);

#undef MENU_BASE
#define MENU_BASE "%80 &Game\\"
      ed->AddShortcut(this, E_MISSION_ACTOR_CREATE_EDIT, MENU_BASE"%0 &Create actor (edit table)\tA", K_A, 0);
      ed->AddShortcut(this, E_MISSION_ACTOR_DESTROY, MENU_BASE"%a &Destroy actor", K_NOKEY, 0);
      ed->AddShortcut(this, E_MISSION_ACTOR_DRAW_NAMES, MENU_BASE"Draw actor &names\tCtrl+Shift+Alt+A", K_A, SKEY_CTRL|SKEY_ALT|SKEY_SHIFT);

      ed->AddShortcut(this, E_MISSION_MODE_EDIT, MENU_BASE"%i &Edit mode\tF5", K_F5, 0);
      ed->AddShortcut(this, E_MISSION_MODE_GAME, MENU_BASE"&Game mode\tF6", K_F6, 0);
      ed->AddShortcut(this, E_MISSION_RELOAD_TEXTS, MENU_BASE"&Texts\\&Reload texts", K_NOKEY, 0);

      C_str vss_exe, vss_database;
      if(SCGetPath(vss_exe, vss_database)){
#undef MENU_BASE
#define MENU_BASE "&File\\%10 &Version control\\"
         ed->AddShortcut(this, E_MISSION_GET, MENU_BASE"&Get latest", K_NOKEY, 0);
         ed->AddShortcut(this, E_MISSION_CHECK_OUT, MENU_BASE"Check &out mission\tCtrl+O", K_O, SKEY_CTRL);
         ed->AddShortcut(this, E_MISSION_CHECK_IN, MENU_BASE"Check &in mission\tCtrl+I", K_I, SKEY_CTRL);
      }

      ed->CheckMenu(this, E_MISSION_SAVE_ASK_TOGGLE, ask_to_save);
      ed->CheckMenu(this, E_MISSION_ACTOR_DRAW_NAMES, draw_actor_names);
      ed->CheckMenu(this, E_MISSION_SMART_RELOAD, smart_reload);

      if(debug_cam_acq)
         Action(E_MISSION_GCAM_ACQUIRE);

      sb_rt_msg_pos = ed->CreateStatusIndex(300);

      ed->GetIGraph()->AddCallback(GraphCallback_thunk, this);

      {
                              //initialize toolbar
         /*
         HWND hwnd = (HWND)ed->GetIGraph()->GetHWND();
         RECT rc;
         GetClientRect(hwnd, &rc);
         rc.left = rc.right;
         ClientToScreen(hwnd, (POINT*)&rc);
         int x_pos = rc.left - 90, y_pos = rc.top;

         PC_toolbar tb = ed->GetToolbar("File", x_pos, y_pos, is_vss ? 2 : 1);
         */
         PC_toolbar tb = ed->GetToolbar("Standard");
         S_toolbar_button tbs[] = {
            {E_MISSION_OPEN,  0, "Open mission"},
            {E_MISSION_RELOAD,2, "Reload mission"},
            {E_MISSION_SAVE,  1, "Save mission"},
            {0, -1},
            {E_MISSION_GET,   3, "Get mission from SourceSafe"},
            {E_MISSION_CHECK_OUT,  4, "Check out"},
            {E_MISSION_CHECK_IN,  5, "Check in"},
            {0, -1},
            //{E_MISSION_PROPERTIES,  6, "Scene properties"},
         };
         tb->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_FILE", GetModuleHandle(NULL), 0);
      }

      return true;
   }

//----------------------------

   virtual void Close(){ 
      ed->GetIGraph()->RemoveCallback(GraphCallback_thunk, this);
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(tab_auto_save->ItemB(TAB_B_AUTOSAVE_ON) && !edit_lock){
         if((auto_save_count += time) >= tab_auto_save->ItemI(TAB_I_AUTOSAVE_TIME)*1000){
            auto_save_count = 0;

            if(ed->IsModified()){
               ed->Message("Auto-saving...", 0, EM_MESSAGE, true);
               C_fstr dest_dir("%s\\%s", tab_auto_save->ItemS(TAB_S32_AUTOSAVE_DIR), (const char*)mission_name);
               E_MISSION_IO mio = mission->Save(dest_dir, 0);
               WriteStatusMessage(mio, "AutoSave failed", C_fstr("Scene saved to %s", (const char*)dest_dir));
            }
         }
      }
      if(ed->IsActive()){

                              //check if mission changed outside the editor
         if((modify_change_countdown -= time) <= 0){
            modify_change_countdown = MODIFY_CHANGE_COUNT;

            dword mission_crc;
            if(mission_open_ok &&
               mission->GetMissionCRC(mission_name, mission_crc)){
               if(mission_crc!=mission->GetCRC()){
                  MessageBox((HWND)ed->GetIGraph()->GetHWND(), "Mission binary file changed outside the Editor. Click OK to reload the file now.", "Insanity editor", MB_OK);
                                 //reload without attempting to save
                  Reload(false);
               }
            }
         }
      }
   }

//----------------------------

   void RestoreBackup(const char *miss_name){

                              //check if file exist
      C_fstr backup_str("missions\\_backup\\%s\\scene.bin", miss_name);
      {
         C_cache ck;
         if(!ck.open(backup_str, CACHE_READ))
            return;
      }
      if(MessageBox((HWND)ed->GetIGraph()->GetHWND(), "Restore from backup file?", "Load failed", MB_YESNO) == IDYES){

         C_fstr bin_str("missions\\%s\\scene.bin", miss_name);
                                    //return old file
         OsMoveFile(backup_str, bin_str);
                                 //reload without attempting to save
         Reload(false);
      }
   }

//----------------------------

   virtual dword Action(int id, void *context = NULL){

      switch(id){
      case E_MISSION_HELP:
         OsShell("..\\Docs\\Insanity3d.chm");
         break;

      case E_MISSION_OPEN:
         {
            ed->GetScene()->AttenuateSounds();
            C_str save_name = mission_name;
            S_dlg_init di = {this, false};
            int i = DialogBoxParam(GetModuleHandle(NULL), "IDD_OPEN_MISSION",
               (HWND)ed->GetIGraph()->GetHWND(), dlgSelectThunk, (LPARAM)&di);
            if(i==1){
               C_str new_name = mission_name;
               mission_name = save_name;
               if(!Save(true))
                  break;
               mission_name = new_name;
               int save_time = ed->GetIGraph()->ReadTimer();
               //mission->Close();
               E_MISSION_IO mio = mission->Open(mission_name, (make_log ? OPEN_LOG : 0));
               mission_open_ok = (mio==MIO_OK);
               if(!mission_open_ok)
                  EditLock();

               WriteStatusMessage(mio, "Load failed", FormatLoadTime(save_time), false);
               last_mission_name = save_name;
               if(mio == MIO_CORRUPT)
                  RestoreBackup(mission_name);
               else{
                  SetupCameraRange();
#if 0
                  PC_editor_item usability = ed->FindPlugin("Usability");
                  if(usability)
                     usability->Action(E_USABILITY_LOAD, (void*)(const char*)mission_name);
#endif
               }
            }
         }
         break;

      case E_MISSION_RELOAD:
         Reload();
         break;

      case E_MISSION_SAVE:
         Save(false, false);
         break;

      case E_MISSION_MERGE:
         {
            if(!ed->CanModify()) break;
                              //show merge dialog - choose elements to import/erase
            ed->GetScene()->AttenuateSounds();
            int i = DialogBoxParam(GetModuleHandle(NULL), "IDD_MERGE",
               (HWND)ed->GetIGraph()->GetHWND(), dlgMerge, (LPARAM)this);
            if(!i) break;

            char fname[MAX_PATH];
            bool b = true;
            if(merge_load){
                              //prompt for filename
               b = GetFileName("Merge binary file", "Mission files (*.bin)\0*.bin\0All files\0*.*\0",
                  "bin", fname, sizeof(fname), C_fstr("missions\\%s", (const char*)mission_name));
            }
            if(b){
                              //first erase old things (using plugin, so that we can undo
               dword erase_flags = 0;
               if(merge_erase&OPEN_MERGE_SOUNDS) erase_flags |= ENUMF_SOUND;
               if(merge_erase&OPEN_MERGE_LIGHTS) erase_flags |= ENUMF_LIGHT;
               if(merge_erase&OPEN_MERGE_MODELS) erase_flags |= ENUMF_MODEL;
               if(merge_erase&OPEN_MERGE_VOLUMES) erase_flags |= ENUMF_VOLUME;
               if(merge_erase&OPEN_MERGE_DUMMYS) erase_flags |= ENUMF_DUMMY;
               if(merge_erase&OPEN_MERGE_CAMERAS) erase_flags |= ENUMF_CAMERA;
               if(merge_erase&OPEN_MERGE_OCCLUDERS) erase_flags |= ENUMF_OCCLUDER;
               
               struct S_hlp{
                  PC_editor_item_Create e_create;
                  PC_editor_item_Modify e_modify;
                  dword count[FRAME_LAST];
                  static I3DENUMRET I3DAPI cbErase(PI3D_frame frm, dword c){

                     S_hlp *hp = (S_hlp*)c;
                              //check if frame was explicitly created
                     dword f_flags = hp->e_modify->GetFrameFlags(frm);
                     if(f_flags&E_MODIFY_FLG_CREATE){
                              //ok to erase
                        dword type = frm->GetType();
                        if(hp->e_create->DeleteFrame(frm)){
                              //stats
                           ++hp->count[type];
                        }
                     }

                     return I3DENUMRET_OK;
                  }
               } hlp;
               memset(&hlp, 0, sizeof(hlp));
               hlp.e_create = (PC_editor_item_Create)ed->FindPlugin("Create");
               hlp.e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
               ed->GetScene()->EnumFrames(S_hlp::cbErase, (dword)&hlp, erase_flags);

               /*
                              //show stats
               static const char *frame_name[FRAME_LAST] = {
                  NULL, "visual", "light", "camera",
                  "sound", "sector", "dummy", "target",
                  "user", "model", "joint", "volume",
                  "occluder",
               };
               C_fstr str("Deleted objects:\n");
               for(int i=0; i<FRAME_LAST; i++){
                  if(hlp.count[i]){
                     str = C_fstr("%s%s ... %i\n", (const char*)str, frame_name[i], hlp.count[i]);
                  }
               }
               */

               if(merge_load){
                              //perform merge
                  E_MISSION_IO mio = mission->Open(fname,
                     (make_log ? OPEN_LOG : 0) |
                     merge_load | OPEN_MERGE_MODE);
                  WriteStatusMessage(mio, "Merge failed", "Merge succeeded");
                  if(mio==MIO_OK)
                     editor->SetModified();
                  PC_editor_item e_chkp = editor->FindPlugin("CheckPoint");
                  if(e_chkp)
                     e_chkp->AfterLoad();
                  SetupCameraRange();
               }
               /*

               str = C_fstr("%s\nYou may UNDO all changes.", (const char*)str);
               MessageBox((HWND)ed->GetIGraph()->GetHWND(), str, "Merge results", MB_OK);
               */
            }
         }
         break;

      case E_MISSION_SAVE_ASK_TOGGLE:
         ask_to_save = !ask_to_save;
         ed->CheckMenu(this, id, ask_to_save);
         ed->Message(C_fstr("Ask to save scene option %s", ask_to_save ? "on" : "off"));
         break;

      case E_MISSION_SMART_RELOAD:
         smart_reload = !smart_reload;
         ed->CheckMenu(this, id, smart_reload);
         ed->Message(C_fstr("Fast reload mode %s", smart_reload ? "on" : "off"));
         break;

      case E_MISSION_AUTOSAVE_CONFIG:
         {
            tab_auto_save->Edit(GetAutoSaveTemplate(), ed->GetIGraph()->GetHWND(), 
               NULL,
               (dword)this, 
               TABEDIT_IMPORT | TABEDIT_EXPORT,
               tab_pos_size);
         }
         break;

      case E_MISSION_GET:
         {
            if(MessageBox((HWND)ed->GetIGraph()->GetHWND(), "Are you sure to get latest version?", "Get latest version", MB_OKCANCEL) != IDOK)
               break;
            C_fstr filename("missions\\%s\\scene.bin", (const char*)mission_name);
            ed->Message(C_fstr("Getting '%s'...", (const char*)filename), 0, EM_MESSAGE, true);
            OsMakeFileReadOnly(filename, true);
            bool ok = SCGetFile("Insanity3d\\Editor", filename);
            if(!ok){
               ed->Message(C_fstr("Failed to get: '%s'", (const char*)filename));
               break;
            }
                        //file checked out, must reload now (if different)
            dword mission_crc;
            if(mission->GetMissionCRC(mission_name, mission_crc)){
               if(mission_crc!=mission->GetCRC()){
                  MessageBox((HWND)ed->GetIGraph()->GetHWND(), "Mission must be reloaded now.", "Insanity editor", MB_OK);
                                 //reload without attempting to save
                  Reload(false);
                  return 2;
               }
            }
            ed->Message(C_fstr("Mission get: '%s'.", (const char*)filename));
         }
         break;

      case E_MISSION_CHECK_OUT:
         {
            C_fstr filename("missions\\%s\\scene.bin", (const char*)mission_name);
            bool is_rdonly = OsIsFileReadOnly(filename);
                              //ask user what to do
            struct S_hlp{
               const char *filename;
               bool can_make_wr;
               static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

                  switch(uMsg){
                  case WM_INITDIALOG:
                     {
                        S_hlp *hp = (S_hlp*)lParam;
                        OsCenterWindow(hwnd, (HWND)igraph->GetHWND());
                        SetDlgItemText(hwnd, IDC_EDIT, (const char*)hp->filename);
                        EnableWindow(GetDlgItem(hwnd, IDC_WRITABLE), hp->can_make_wr);
                        ShowWindow(hwnd, SW_SHOW);
                     }
                     return 1;
                  case WM_COMMAND:
                     if(HIWORD(wParam)==BN_CLICKED)
                        EndDialog(hwnd, LOWORD(wParam));
                     break;
                  }
                  return 0;
               }
            } hlp;
            hlp.filename = filename;
            hlp.can_make_wr = is_rdonly;
            int i = DialogBoxParam(GetModuleHandle(NULL), "IDD_CHECKOUT", (HWND)ed->GetIGraph()->GetHWND(),
               S_hlp::dlgProc, (LPARAM)&hlp);

            switch(i){
            case IDOK:
               {
                              //try to check out the file
                  ed->Message(C_fstr("Checking out '%s'...", (const char*)filename), 0, EM_MESSAGE, true);
                  if(!is_rdonly)
                     OsMakeFileReadOnly(filename, true);
                  bool ok = SCCheckOutFile("Insanity3d\\Editor", filename);
                  if(!ok){
                     ed->Message(C_fstr("Failed to check out: '%s'", (const char*)filename));
                     break;
                  }
                              //file checked out, must reload now (if different)
                  dword mission_crc;
                  if(mission->GetMissionCRC(mission_name, mission_crc)){
                     if(mission_crc!=mission->GetCRC()){
                        MessageBox((HWND)ed->GetIGraph()->GetHWND(), "Mission must be reloaded now.", "Insanity editor", MB_OK);
                                       //reload without attempting to save
                        Reload(false);
                        return 2;
                     }
                  }
               }
               ed->Message(C_fstr("Mission checked out: '%s'.", (const char*)filename));
               return 1;

            case IDC_WRITABLE:
               if(OsMakeFileReadOnly(filename, false))
                  return 1;
               break;

            default:
               ed->Message("Check out canceled.");
            }
         }
         break;

      case E_MISSION_CHECK_IN:
         {
            bool tmp = ask_to_save;
            ask_to_save = true;
            if(!Save(true, false)){
               ask_to_save = tmp;
               break;
            }
            ask_to_save = tmp;
            C_fstr filename("missions\\%s\\scene.bin", (const char*)mission_name);

                              //ask user what to do
            struct S_hlp{
               const char *filename;
               bool keep;
               static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

                  switch(uMsg){
                  case WM_INITDIALOG:
                     {
                        S_hlp *hp = (S_hlp*)lParam;
                        SetWindowLong(hwnd, GWL_USERDATA, lParam);
                        OsCenterWindow(hwnd, (HWND)igraph->GetHWND());
                        SetDlgItemText(hwnd, IDC_EDIT, hp->filename);
                        CheckDlgButton(hwnd, IDC_CHECKIN_KEEP, hp->keep);
                        ShowWindow(hwnd, SW_SHOW);
                     }
                     return 1;
                  case WM_COMMAND:
                     if(HIWORD(wParam)==BN_CLICKED){
                        switch(LOWORD(wParam)){
                        case IDOK:
                        case IDCANCEL:
                           {
                              S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                              hp->keep = IsDlgButtonChecked(hwnd, IDC_CHECKIN_KEEP);
                              EndDialog(hwnd, LOWORD(wParam));
                           }
                           break;
                        }
                     }
                     break;
                  }
                  return 0;
               }
            } hlp;
            bool keep_checked_out = false;
            hlp.filename = filename;
            hlp.keep = keep_checked_out;
            int i = DialogBoxParam(GetModuleHandle(NULL), "IDD_CHECKIN", (HWND)ed->GetIGraph()->GetHWND(),
               S_hlp::dlgProc, (LPARAM)&hlp);
            keep_checked_out = hlp.keep;
            if(i==IDOK){
                              //try to check in the file
               ed->Message(C_fstr("Checking in '%s'...", (const char*)filename), 0, EM_MESSAGE, true);
               bool ok = SCCheckInFile("Insanity3d\\Editor", filename, keep_checked_out);
               if(!ok){
                  ed->Message(C_fstr("Failed to check in: '%s'", (const char*)filename));
               }else{
                  ed->Message(C_fstr("Mission checked in: '%s'.", (const char*)filename));
               }
            }else{
               ed->Message("Check in canceled.");
            }
         }
         break;

      case E_MISSION_LOG_TOGGLE:
         make_log = !make_log;
         ed->CheckMenu(this, E_MISSION_LOG_TOGGLE, make_log);
         break;

      case E_MISSION_PROPERTIES:
         {
            PC_table tab_prop = CreateTable();
            tab_prop->Load(GetPropTemplate(), TABOPEN_TEMPLATE);

            {
               tab_prop->ItemV(TAB_CV_SCN_BGND) = ed->GetScene()->GetBgndColor();
               tab_prop->ItemI(TAB_I_SCN_CAM_FOV) = int(mission->cam_fov * 180.0f/PI);
               tab_prop->ItemF(TAB_F_SCN_CAM_RANGE) = mission->cam_range;
               tab_prop->ItemF(TAB_F_SCN_CAM_EDIT_RANGE) = mission->cam_edit_range;

               PI3D_camera cam = ed->GetScene()->GetActiveCamera();
               if(cam){
                  //tab_prop->ItemI(TAB_I_SCN_CAM_FOV) = FloatToInt(cam->GetFOV() * 180.0f/PI);
                  //float rn;
                  //cam->GetRange(rn, tab_prop->ItemF(TAB_F_SCN_CAM_RANGE));
                  tab_prop->ItemB(TAB_B_SCN_CAM_ORTHO) = cam->GetOrthogonal();
                  tab_prop->ItemF(TAB_F_SCN_CAM_ORTHO_SCL) = cam->GetOrthoScale();
               }
               ed->GetScene()->GetBackdropRange(tab_prop->ItemF(TAB_F_SCN_BDROP_RANGE_N),
                  tab_prop->ItemF(TAB_F_SCN_BDROP_RANGE_F));

               tab_prop->Edit(GetPropTemplate(), ed->GetIGraph()->GetHWND(), cbProp,
                  (dword)this, 
                  /*TABEDIT_MODELESS | */TABEDIT_IMPORT | TABEDIT_EXPORT,
                  tab_pos_size);
               //if(hwnd_prop) ed->GetIGraph()->AddDlgHWND(hwnd_prop);
            }
            tab_prop->Release();
         }
         break;

      case E_MISSION_ACTOR_CREATE_EDIT:
         {
                              //create actors on selection
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
            bool any_actor = false;
            const C_table_template *tt = NULL;
            C_smart_ptr<C_table> conjuct_tab;

            for(dword i=0; i<sel_list.size(); i++){
               PS_frame_info fi = GetFrameInfo(sel_list[i]);
               if(fi && fi->actor){
                  if(!i){
                     tt = fi->actor->GetTableTemplate();
                     if(tt){
                        CPC_table atab = fi->actor->GetTable();
                        assert(atab);
                        conjuct_tab = CreateTable();
                        conjuct_tab->Load(atab, TABOPEN_DUPLICATE | TABOPEN_UPDATE);
                        conjuct_tab->Release();
                     }
                  }else{
                     CPC_table tab = fi->actor->GetTable();
                     /*
                     if(conjuct_tab && memcmp(conjuct_tab->GetGUID(), tab->GetGUID(), 16)){
                        conjuct_tab = NULL;
                     }else*/
                     if(conjuct_tab){
                              //make non-common items undeterminate
                        for(dword i=conjuct_tab->NumItems(); i--; ){
                           E_TABLE_ELEMENT_TYPE tt = conjuct_tab->GetItemType(i);
                           dword sz = conjuct_tab->SizeOf(i);
                           if(!sz)
                              continue;
                           if(conjuct_tab->ArrayLen(i)){
                              for(dword ai=conjuct_tab->ArrayLen(i); ai--; ){
                                 if(memcmp((byte*)conjuct_tab->Item(i)+sz*ai, (byte*)((PC_table)tab)->Item(i)+sz*ai, sz)){
                                    switch(tt){
                                    case TE_BOOL: (byte&)conjuct_tab->ItemB(i, ai) = TAB_INV_BOOL; break;
                                    case TE_INT: conjuct_tab->ItemI(i, ai) = TAB_INV_INT; break;
                                    case TE_FLOAT: conjuct_tab->ItemF(i, ai) = TAB_INV_FLOAT; break;
                                    case TE_STRING: conjuct_tab->ItemS(i, ai)[0] = TAB_INV_STRING[0]; break;
                                    case TE_ENUM: conjuct_tab->ItemE(i, ai) = TAB_INV_ENUM; break;
                                    }
                                 }
                              }
                           }else{
                              if(memcmp(conjuct_tab->Item(i), ((PC_table)tab)->Item(i), sz)){
                                 switch(tt){
                                 case TE_BOOL: (byte&)conjuct_tab->ItemB(i) = TAB_INV_BOOL; break;
                                 case TE_INT: conjuct_tab->ItemI(i) = TAB_INV_INT; break;
                                 case TE_FLOAT: conjuct_tab->ItemF(i) = TAB_INV_FLOAT; break;
                                 case TE_STRING: conjuct_tab->ItemS(i)[0] = TAB_INV_STRING[0]; break;
                                 case TE_ENUM: conjuct_tab->ItemE(i) = TAB_INV_ENUM; break;
                                 }
                              }
                           }
                        }
                     }
                  }
                  any_actor = true;
                  if(!tt)
                     tt = fi->actor->GetTableTemplate();
               }
            }
            if(any_actor){
               if(conjuct_tab && tt){

                  hwnd_tab_edit = (HWND)conjuct_tab->Edit(tt, igraph->GetHWND(),
                     cbTable,
                     (dword)this,
                     TABEDIT_CENTER | TABEDIT_MODELESS | TABEDIT_IMPORT | TABEDIT_EXPORT,
                     tab_pos_size);
                  if(hwnd_tab_edit){
                     igraph->AddDlgHWND(hwnd_tab_edit);
                     ed->GetIGraph()->EnableSuspend(false);
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
               ed->Message("Actor already assigned, delete it before assigning new");
               break;
            }


                              //let user select actor to create
            char buf[2048];
            char indx[ACTOR_LAST];
            int len = 0;
            int j;
            for(i=1, j=0; i<ACTOR_LAST; i++){
               if(!actor_type_info[i].friendly_name) break;    //undefined name? break out
               if(actor_type_info[i].allow_edit_creation){
                  strcpy(&buf[len], actor_type_info[i].friendly_name);
                  len += strlen(buf+len) + 1;
                  indx[j++] = (char)i;
               }
            }
            buf[len] = 0;
            i = WinSelectItem("Create actor:", buf, last_create_actor_index);
            if(i==-1)
               break;
            E_ACTOR_TYPE at = (E_ACTOR_TYPE)indx[i];
            last_create_actor_index = i;

                              //create new actor(s)
            int count = 0;
            for(i=sel_list.size(); i--; ){
               PI3D_frame frm = sel_list[i];
               if(CreateActor(frm, at))
                  ++count;
            }
            ed->Message(C_fstr("Actors created: %i", count));
            if(count)
               ed->SetModified();
         }
         break;

      case E_MISSION_ACTOR_DESTROY:
         {
            if(!ed->CanModify())
               break;
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            if(!sel_list.size()){
               ed->Message("Actor destruction: selection required");
               break;
            }
            int i, count;
            for(i=sel_list.size(), count = 0; i--; ){
               PI3D_frame frm = sel_list[i];
               if(DestroyActor(frm))
                  ++count;
            }
            ed->Message(C_fstr("Actors destroyed: %i", count));
            if(count)
               ed->SetModified();
         }
         break;

      case E_MISSION_ACTOR_DRAW_NAMES:
         draw_actor_names = !draw_actor_names;
         ed->CheckMenu(this, E_MISSION_ACTOR_DRAW_NAMES, draw_actor_names);
         ed->Message(C_fstr("Draw actor names %s", draw_actor_names ? "on" : "off"));
         break;

      case E_MISSION_MODE_EDIT:
         if(mission->IsInGame()){
            mission->GameEnd();
            SetupCameraRange();
         }
         ed->SetActive(true);
         ed->Message("Edit mode");
         SetTickClass(mission);
         //ed->FindPlugin("Properties")->Action(E_PROP_SHOW);
         break;

      case E_MISSION_MODE_GAME:
         if(!mission->IsInGame()){
                              //save changes now
            if(!Save(true, true))
               break;
                              //check if BSP tree is built, if not, ask user to do it now
            if(!CheckBSPTree())
               break;

            mission->GameBegin();
            ed->SetActive(false);
            ed->Message("Game mode");

            EditLock();
            SetupCameraRange();
            SetTickClass(mission);
            return true;
         }
         break;

      case E_MISSION_RELOAD_TEXTS:
         {
            all_txt.Close();
            all_txt.AddFile("text\\english\\system.txt");
            bool TextOpenAll(const char *language_name);
            TextOpenAll("english");
            ed->Message("Texts reloaded");
         }
         break;

      case E_MISSION_GCAM_DIST_P:
         if(!ed->IsActive()){
            float d = mission->GetGameCamera()->GetDistance();
            d = Min(8.0f, d + 2.0f);
            mission->GetGameCamera()->SetDistance(d);
         }
         break;

      case E_MISSION_GCAM_DIST_N:
         if(!ed->IsActive()){
            float d = mission->GetGameCamera()->GetDistance();
            d = Max(0.0f, d - 2.0f);
            mission->GetGameCamera()->SetDistance(d);
         }
         break;

      //case E_MISSION_GCAM_ANGLE_SET: gcam_angle = (dword)context; break;
      //case E_MISSION_GCAM_ANGLE_GET: return gcam_angle;

         /*
      case E_MISSION_GCAM_ANGLE_P:
         if(!ed->IsActive())
            mission->GetGameCamera()->SetAngleMode(byte((mission->GetGameCamera()->GetAngleMode() + 2) % 3));
         break;

      case E_MISSION_GCAM_ANGLE_N:
         if(!ed->IsActive())
            mission->GetGameCamera()->SetAngleMode(byte((mission->GetGameCamera()->GetAngleMode() + 1) % 3));
         break;
         */

      case E_MISSION_GCAM_ACQUIRE:
         {
            debug_cam_acq = !debug_cam_acq;
            ed->CheckMenu(this, E_MISSION_GCAM_ACQUIRE, debug_cam_acq);
            if(!ed->IsActive() && mission->GetGameCamera()){
               mission->GetScene()->SetActiveCamera(debug_cam_acq ? mission->GetGameCamera()->GetCamera() :
                  e_mouseedit->GetEditCam());
                              //always set to view mode in acquired cam mode,
                              // so that mouse is hidden
               if(debug_cam_acq)
                  e_mouseedit->SetViewMode();
            }
            ed->Message(C_fstr("Game camera %s", debug_cam_acq ? "acquired" : "unacquired"));
         }
         break;

      case E_MISSION_RT_ERR_MESSAGE:
         ed->Message((const char*)context, sb_rt_msg_pos, EM_ERROR);
         break;

      case E_MISSION_SET_TABLE:
         {
            const char *name = (const char*)context;
            void *pTabDataLen = (byte*)(context) + strlen(name)+1;
            dword TabDataLen = *(dword*)(pTabDataLen);
            byte *pTabData = (byte*)pTabDataLen + sizeof(size_t);
         
            PI3D_frame frm = NULL;
            if(strlen(name)){
               frm = mission->GetScene()->FindFrame(name);
               if(!frm){
                  ed->Message(C_fstr("C_edit_Script(E_SCRIPT_SET_TABLE): can't find frame %s", name));
                  break;
               }

               S_frame_info *fi = CreateFrameInfo(frm);
               PC_table tab = const_cast<PC_table>(fi->actor->GetTable());
               assert(tab);
               if(tab){
                  C_cache tab_cache;
                  bool b = tab_cache.open(&pTabData, &TabDataLen, CACHE_READ_MEM);
                  assert(b);
                  b = tab->Load(&tab_cache, TABOPEN_FILEHANDLE);
                  assert(b);
                  tab_cache.close();
               }
            }
         }
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Render(){

      if(ed->IsActive()){
         if(draw_actor_names){
            const C_vector<C_smart_ptr<C_actor> > &actors = mission->GetActorsVector();
            for(int i=actors.size(); i--; ){  
               DrawActorName(actors[i]);
            }
         }       
      }
   }

//----------------------------

   enum E_CHUNKTYPE{
      CT_MISSION_NAME = 0x1000,  //string
      CT_MAKE_LOG,            //bool
      CT_TAB_POS_SIZE,        //tab_pos_size
      CT_TAB_AUTO_SAVE,       //table auto_save
      CT_ASK_TO_SAVE,         //bool
      CT_DEBUG_CAM_ACQ,       //bool
      CT_MERGE_LOAD,          //dword
      CT_MERGE_ERASE,         //dword
      CT_LAST_CREATE_ACTOR_INDX, //dword
      CT_DRAW_ACTOR_NAMES,    //bool
      CT_GCAM_DIST,           //float
      CT_free,
      CT_LAST_MISSION,        //string
      CT_free1,
      CT_FAVORITE,            //string
      CT_SMART_RELOAD,        //bool
   };

//----------------------------
#define VERSION 4

   virtual bool LoadState(C_chunk &ck){

                              //check version
      word version = VERSION-1;
      ck.Read(&version, sizeof(word));
      if(version==VERSION){
         dword fav_indx = 0;
         while(ck)
         switch(++ck){
         case CT_MISSION_NAME:
            mission_name = ck.RStringChunk();
            break;
         case CT_MAKE_LOG:
            make_log = ck.RByteChunk();
            break;
         case CT_TAB_POS_SIZE:
            ck.Read(&tab_pos_size, sizeof(tab_pos_size));
            --ck;
            break;
         case CT_TAB_AUTO_SAVE:
            tab_auto_save->Load(ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
            --ck;
            break;
         case CT_ASK_TO_SAVE: ask_to_save = ck.RByteChunk(); break;
         case CT_DEBUG_CAM_ACQ: debug_cam_acq = ck.RByteChunk(); break;
         case CT_MERGE_LOAD: merge_load = ck.RIntChunk(); break;
         case CT_MERGE_ERASE: merge_erase = ck.RIntChunk(); break;
         case CT_LAST_CREATE_ACTOR_INDX: last_create_actor_index = ck.RIntChunk(); break;
         case CT_DRAW_ACTOR_NAMES: draw_actor_names = ck.RByteChunk(); break;
         case CT_GCAM_DIST: gcam_dist = ck.RFloatChunk(); break;
         //case CT_GCAM_ANGLE: gcam_angle = ck.RByteChunk(); break;
         case CT_LAST_MISSION: last_mission_name = ck.RStringChunk(); break;
         case CT_FAVORITE:
            {
               C_str str = ck.RStringChunk();
               if(fav_indx<NUM_FAVORITES)
                  favorites[fav_indx++] = str;
            }
            break;
         case CT_SMART_RELOAD:
            smart_reload = ck.RByteChunk();
            break;

         default:
            --ck;
         }
      }else
         return false;

      ed->CheckMenu(this, E_MISSION_ACTOR_DRAW_NAMES, draw_actor_names);
      ed->CheckMenu(this, E_MISSION_LOG_TOGGLE, make_log);
      ed->CheckMenu(this, E_MISSION_SAVE_ASK_TOGGLE, ask_to_save);
      ed->CheckMenu(this, E_MISSION_GCAM_ACQUIRE, debug_cam_acq);
      ed->CheckMenu(this, E_MISSION_SMART_RELOAD, smart_reload);
         
      return true;
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

                              //write version
      word version = VERSION;
      ck.Write(&version, sizeof(word));

      ck.WStringChunk(CT_MISSION_NAME, mission_name);
      ck.WByteChunk(CT_MAKE_LOG, make_log);
      ck <<= CT_TAB_POS_SIZE;
         ck.Write(&tab_pos_size, sizeof(tab_pos_size));
      --ck;
                              //write auto-save table
      ck <<= CT_TAB_AUTO_SAVE;
         tab_auto_save->Save(ck.GetHandle(), TABOPEN_FILEHANDLE);
      --ck;

      ck.WByteChunk(CT_ASK_TO_SAVE, ask_to_save);
      ck.WByteChunk(CT_DEBUG_CAM_ACQ, debug_cam_acq);
      ck.WByteChunk(CT_DRAW_ACTOR_NAMES, draw_actor_names);
      ck.WIntChunk(CT_MERGE_LOAD, merge_load);
      ck.WIntChunk(CT_MERGE_ERASE, merge_erase);
      ck.WIntChunk(CT_LAST_CREATE_ACTOR_INDX, last_create_actor_index);

                              //save game cam
      ck.WFloatChunk(CT_GCAM_DIST, gcam_dist);
      //ck.WByteChunk(CT_GCAM_ANGLE, (byte)gcam_angle);
                              //save focus name
      ck.WStringChunk(CT_LAST_MISSION, last_mission_name);

      for(int i=0; i<NUM_FAVORITES; i++)
         ck.WStringChunk(CT_FAVORITE, favorites[i]);

      ck.WByteChunk(CT_SMART_RELOAD, smart_reload);

      return true;
   }
};

//----------------------------

void InitMissionPlugin(PC_editor editor, C_game_mission &m1){
   C_edit_Mission *em = new C_edit_Mission(m1); editor->InstallPlugin(em); em->Release();
}

#endif                        //EDITOR

//----------------------------
