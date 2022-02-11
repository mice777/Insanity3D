#include "all.h"
#include "common.h"
#include <win_res.h>

//----------------------------

#define SELECTION_NUM_TABS 5

static struct S_objsel_column{
   const char *title;
   int image;                 //one-based, 0 = no image
} objsel_list[] = {
   {""},
   {"Name"},
   {"Actor"},
   {"Script"},
   {"Type"},
};

static int objsel_list_width_defaults[SELECTION_NUM_TABS] = {
   16,
   150,
   50,
   50,
   50,
};

//----------------------------

class C_edit_Selection_imp: public C_editor_item_Selection{
   virtual const char *GetName() const{ return "Selection"; }
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_MouseEdit> e_medit;

   t_GetFrameScript *PGetFrameScript;
   t_GetFrameActor *PGetFrameActor;

   int objsel_list_width[SELECTION_NUM_TABS];

   bool view_axis_on;
//----------------------------

   enum E_FRM_IMAGE{
      FI_UNKNOWN, 
      FI_VISUAL,
      FI_LIGHT_POINT,
      FI_SOUND_POINT,
      FI_MODEL,
      FI_reserved,
      FI_SECTOR,
      FI_DUMMY,
      FI_VOLUME_BOX,
      FI_CAMERA,
      FI_USER,
      FI_JOINT,
      FI_OCCLUDER,
      FI_LIGHT_SPOT,
      FI_LIGHT_DIR,
      FI_LIGHT_AMB,
      FI_LIGHT_FOG,
      FI_LIGHT_LAYERFOG,
      FI_SOUND_AMB,
      FI_SOUND_SPOT,
      FI_SOUND_POINTAMB,
      FI_VOLUME_SPHERE,
      FI_VOLUME_RECT,
      FI_VOLUME_CAPCYL,
   };

   enum E_SEL_GROUP{
      LM_VISUAL,
      LM_LIGHT,
      LM_SOUND,
      LM_MODEL,

      LM_TARGET,
      LM_VOLUME,
      LM_SECTOR,
      LM_DUMMY,

      LM_CAMERA,
      LM_OCCLUDER,
      LM_JOINT,
      LM_USER,
                                 //other (modes)
      LM_HIERARCHY,
      LM_HIDE,

      LM_LAST
   };

   enum{
      UNDO_SELECT,
      UNDO_DESELECT,
      UNDO_INVERT,
      UNDO_CLEAR,
   };

//----------------------------

   enum E_ACTION_SELECTION2{
      E_SELECTION_CLEAR2,
      E_SELECTION_INVERT2,

      E_SELECTION_BROWSE,
      E_SELECTION_PARENTS,
      E_SELECTION_CHILDREN,
      E_SELECTION_CLEAR_FLASH,
      //E_SELECTION_CONFIG,
      E_SELECTION_TOGGLE_LINK,

      E_SELECTION_SEL_FLASH_NEXT,
      E_SELECTION_SEL_FLASH_PREV,
      E_SELECTION_TOGGLE_AXIS,
   };

//----------------------------

   inline static bool *GetListMode(){
      static bool list_mode[LM_LAST] = {
         1, 1, 1, 1,
         1, 1, 1, 1,
         1, 1, 1, 1,
         1, 0,
      };
      return list_mode;
   }
#define list_mode GetListMode()

//----------------------------
                              //phase 1 - collect one hierarchy level
   static I3DENUMRET I3DAPI dlgObjAdd(PI3D_frame frm, dword hl){
      C_vector<C_smart_ptr<I3D_frame> > *hier_level = (C_vector<C_smart_ptr<I3D_frame> >*)hl;
      hier_level->push_back(frm);
      return list_mode[LM_HIERARCHY] ? I3DENUMRET_SKIPCHILDREN : I3DENUMRET_OK;
   }

//----------------------------

   static bool cbFrameLess(PI3D_frame frm1a, PI3D_frame frm2a){
      return stricmp(frm1a->GetName(), frm2a->GetName()) < 0;
   }

//----------------------------

   bool AddFrame(HWND hwnd, PI3D_frame frm, const C_vector<C_smart_ptr<I3D_frame> > &sel_list,
      int &focused, dword slct_tab, bool also_hidden){

      bool add = false;
      switch(frm->GetType()){
      case FRAME_VISUAL: if(list_mode[LM_VISUAL]) add = also_hidden || frm->IsOn(); break;
      case FRAME_LIGHT: if(list_mode[LM_LIGHT]) add = true; break;
      case FRAME_SOUND: if(list_mode[LM_SOUND]) add = true; break;
      case FRAME_MODEL: if(list_mode[LM_MODEL]) add = also_hidden || frm->IsOn(); break;
      case FRAME_SECTOR: if(list_mode[LM_SECTOR]) add = also_hidden || frm->IsOn(); break;
      case FRAME_DUMMY: if(list_mode[LM_DUMMY]) add = also_hidden || frm->IsOn(); break;
      case FRAME_VOLUME: if(list_mode[LM_VOLUME]) add = true; break;
      case FRAME_CAMERA: if(list_mode[LM_CAMERA]) add = true; break;
      case FRAME_OCCLUDER: if(list_mode[LM_OCCLUDER]) add = true; break;
      case FRAME_JOINT: if(list_mode[LM_JOINT]) add = true; break;
      case FRAME_USER: if(list_mode[LM_USER]) add = true; break;
      }
      if(add){
         HWND hwnd_list = GetDlgItem(hwnd, IDC_LIST);

         E_FRM_IMAGE ti = FI_UNKNOWN;
         dword tmp[2];
         const char *st = NULL;

         switch(frm->GetType()){
         case FRAME_VISUAL:
            ti = FI_VISUAL;
            tmp[0] = I3DCAST_VISUAL(frm)->GetVisualType();
            tmp[1] = 0;
            st = (const char*)tmp;
            break;
         case FRAME_LIGHT:
            ti = FI_LIGHT_POINT;
            switch(I3DCAST_LIGHT(frm)->GetLightType()){
            case I3DLIGHT_POINT: ti = FI_LIGHT_POINT; st = "Point"; break;
            case I3DLIGHT_SPOT: ti = FI_LIGHT_SPOT; st = "Spot"; break;
            case I3DLIGHT_DIRECTIONAL: ti = FI_LIGHT_DIR; st = "Dir"; break;
            case I3DLIGHT_AMBIENT: ti = FI_LIGHT_AMB; st = "Amb"; break;
            case I3DLIGHT_POINTAMBIENT: ti = FI_LIGHT_AMB; st = "PAmb"; break;
            case I3DLIGHT_FOG: ti = FI_LIGHT_FOG; st = "Fog"; break;
            case I3DLIGHT_LAYEREDFOG: ti = FI_LIGHT_LAYERFOG; st = "LFog"; break;
            }
            break;
         case FRAME_SOUND:
            ti = FI_SOUND_POINT;
            switch(I3DCAST_SOUND(frm)->GetSoundType()){
            case I3DSOUND_POINT: ti = FI_SOUND_POINT; st = "Point"; break;
            case I3DSOUND_SPOT: ti = FI_SOUND_SPOT; st = "Spot"; break;
            case I3DSOUND_AMBIENT: ti = FI_SOUND_AMB; st = "Amb"; break;
            case I3DSOUND_POINTAMBIENT: ti = FI_SOUND_POINTAMB; st = "PAmb"; break;
            }
            break;
         case FRAME_MODEL:
            {
               ti = FI_MODEL;
               st = I3DCAST_MODEL(frm)->GetFileName();
               for(int i=strlen(st); i--; ){
                  if(st[i]=='\\'){
                     st += i + 1;
                     break;
                  }
               }
            }
            break;
         case FRAME_SECTOR: ti = FI_SECTOR; break;
         case FRAME_USER: ti = FI_USER; break;
         case FRAME_DUMMY: ti = FI_DUMMY; break;
         case FRAME_JOINT: ti = FI_JOINT; break;
         case FRAME_VOLUME:
            st = "<unknown>";
            ti = FI_VOLUME_BOX;
            switch(I3DCAST_VOLUME(frm)->GetVolumeType()){
            case I3DVOLUME_BOX: ti = FI_VOLUME_BOX; st = "Box"; break;
            case I3DVOLUME_RECTANGLE: ti = FI_VOLUME_RECT; st = "Rect"; break;
            case I3DVOLUME_SPHERE: ti = FI_VOLUME_SPHERE; st = "Sphere"; break;
            case I3DVOLUME_CYLINDER: ti = FI_VOLUME_CAPCYL; st = "Cylinder"; break;
            case I3DVOLUME_CAPCYL: ti = FI_VOLUME_CAPCYL; st = "CapCyl"; break;
            }
            break;
         case FRAME_CAMERA: ti = FI_CAMERA; break;
         case FRAME_OCCLUDER:
            ti = FI_OCCLUDER;
            switch(I3DCAST_OCCLUDER(frm)->GetOccluderType()){
            case I3DOCCLUDER_MESH: st = "Mesh"; break;
            case I3DOCCLUDER_SPHERE: st = "Sphere"; break;
            }
            break;
         }
         {
            char buf[256];
            for(dword j=0; j<slct_tab; j++) buf[j]=' ';
            strcpy(buf+j, frm->GetName());
            int count = SendMessage(hwnd_list, LVM_GETITEMCOUNT, 0, 0);

            static LVITEM lvi = { LVIF_PARAM | LVIF_STATE | LVIF_IMAGE};
            lvi.iItem = count;
            lvi.lParam = (LPARAM)frm;
            lvi.state = 0;
            lvi.iImage = ti;
                              //check if this is selected
            for(j=sel_list.size(); j--; )
               if(frm==sel_list[j]) break;
            if(j != -1){
               lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
               focused = count;
            }
            SendMessage(hwnd_list, LVM_INSERTITEM, 0, (LPARAM)&lvi);
                              //set item's subtype
            {
               static LVITEM lvi = { LVIF_TEXT, 0, 4};
               lvi.iItem = count;
               lvi.pszText = (char*)st;
               lvi.cchTextMax = strlen(buf);
               SendMessage(hwnd_list, LVM_SETITEM, 0, (LPARAM)&lvi);
            }
                              //set item's text
            {
               static LVITEM lvi = { LVIF_TEXT, 0, 1};
               lvi.iItem = count;
               lvi.pszText = buf;
               lvi.cchTextMax = strlen(buf);
               SendMessage(hwnd_list, LVM_SETITEM, 0, (LPARAM)&lvi);
            }
                              //set item's sctipt
            if(PGetFrameScript){
               const char *scr_name = (*PGetFrameScript)(frm);
               if(scr_name){
                  static LVITEM lvi = { LVIF_TEXT, 0, 3};
                  lvi.iItem = count;
                  lvi.pszText = (char*)scr_name;
                  lvi.cchTextMax = strlen(scr_name);
                  SendMessage(hwnd_list, LVM_SETITEM, 0, (LPARAM)&lvi);
               }
            }
                              //set item's actor
            if(PGetFrameActor){
               const char *cp = (*PGetFrameActor)(frm);
               if(cp){
                  static LVITEM lvi = { LVIF_TEXT, 0, 2};
                  lvi.iItem = count;
                  lvi.pszText = (char*)cp;
                  lvi.cchTextMax = strlen(cp);
                  SendMessage(hwnd_list, LVM_SETITEM, 0, (LPARAM)&lvi);
               }
            }
         }
      }
      return add;
   }

//----------------------------

   void AddLevel(PI3D_frame root, HWND hwnd, const C_vector<C_smart_ptr<I3D_frame> > &sel_list, int &focused,
      dword slct_tab = 0){

      C_vector<C_smart_ptr<I3D_frame> > hl;
      root->EnumFrames(dlgObjAdd, (dword)&hl, ENUMF_ALL, NULL);
                                 //sort level by name
      sort(hl.begin(), hl.end(), cbFrameLess);

      for(dword i=0; i<hl.size(); i++){
         PI3D_frame frm = hl[i];
         bool add = AddFrame(hwnd, frm, sel_list, focused, slct_tab, false);

         if(add || !list_mode[LM_HIDE])
         if(list_mode[LM_HIERARCHY]){
                                 //repeat on children
            C_vector<C_smart_ptr<I3D_frame> > hier_level;
            frm->EnumFrames(dlgObjAdd, (dword)&hier_level,
               ENUMF_ALL, NULL);

            AddLevel(frm, hwnd, sel_list, focused, slct_tab+2);
         }
      }
   }

//----------------------------
// Add objects into selection list.
// Params:
//    hwnd ... handle of selection dialog window
//    in_list ... optional list from which selection may be made (may be NULL, in which case scene hierarchy is used)
//    curr_sel ... list of currently selected frames
   void AddObjsToList(HWND hwnd, C_vector<C_smart_ptr<I3D_frame> > *in_list,
      const C_vector<C_smart_ptr<I3D_frame> > &curr_sel){

      dword save_gwl = GetWindowLong(hwnd, GWL_USERDATA);
      SetWindowLong(hwnd, GWL_USERDATA, 0);

      PI3D_scene scene = ed->GetScene();
      SendDlgItemMessage(hwnd, IDC_LIST, LVM_DELETEALLITEMS, 0, 0);

      int focused = -1;

      if(!in_list){           //fill-in from scene hierarchy
                              //first is primary sector
         AddFrame(hwnd, scene->GetPrimarySector(), sel_list, focused, 0, false);
                              //then any other frames
         AddLevel(scene->GetPrimarySector(), hwnd, sel_list, focused, 0);
                              //second is backdrop sector
         if(scene->GetBackdropSector()->NumChildren()){
            AddFrame(hwnd, scene->GetBackdropSector(), sel_list, focused, 0, false);
            AddLevel(scene->GetBackdropSector(), hwnd, sel_list, focused, 0);
         }
      }else{
                              //fill-in from provided list
         for(dword i=0; i<in_list->size(); i++){
            AddFrame(hwnd, (*in_list)[i], sel_list, focused, 0, true);
         }
      }
      if(focused!=-1)
         SendDlgItemMessage(hwnd, IDC_LIST, LVM_ENSUREVISIBLE, focused, false);
      SetWindowLong(hwnd, GWL_USERDATA, save_gwl);
   }

//----------------------------

   static void ShowGroupButtons(HWND hwnd, bool enable_hierarchy = true){

      CheckDlgButton(hwnd, IDC_CHECK_VIS, list_mode[LM_VISUAL]);
      CheckDlgButton(hwnd, IDC_CHECK_LIGHT, list_mode[LM_LIGHT]);
      CheckDlgButton(hwnd, IDC_CHECK_MODEL, list_mode[LM_MODEL]);
      CheckDlgButton(hwnd, IDC_CHECK_SND, list_mode[LM_SOUND]);
      CheckDlgButton(hwnd, IDC_CHECK_OCC, list_mode[LM_OCCLUDER]);
      CheckDlgButton(hwnd, IDC_CHECK_VOL, list_mode[LM_VOLUME]);
      CheckDlgButton(hwnd, IDC_CHECK_DUM, list_mode[LM_DUMMY]);
      CheckDlgButton(hwnd, IDC_CHECK_SCT, list_mode[LM_SECTOR]);
      CheckDlgButton(hwnd, IDC_CHECK_CAM, list_mode[LM_CAMERA]);
      CheckDlgButton(hwnd, IDC_CHECK_JOINT, list_mode[LM_JOINT]);
      CheckDlgButton(hwnd, IDC_CHECK_USER, list_mode[LM_USER]);
      if(enable_hierarchy){
         EnableWindow(GetDlgItem(hwnd, IDC_CHECK_HIER), true);
         CheckDlgButton(hwnd, IDC_CHECK_HIER, list_mode[LM_HIERARCHY]);
      }else{
         EnableWindow(GetDlgItem(hwnd, IDC_CHECK_HIER), false);
         CheckDlgButton(hwnd, IDC_CHECK_HIER, false);
      }
      CheckDlgButton(hwnd, IDC_CHECK_HIDE, list_mode[LM_HIDE]);
      EnableWindow(GetDlgItem(hwnd, IDC_CHECK_HIDE), list_mode[LM_HIERARCHY]);
   }

//----------------------------

   static bool StringMatch(const char *cp1, int l1, const char *cp2, int l2){

      if(!l1 || !l2) return (l1==l2);
      for(; ; ++cp1, ++cp2){
         switch(*cp2){
         case 0: return !(*cp1);
         case '*': return true;
         case '?':
            if(!*cp1) return false;
            break;
         default: if(*cp1 != *cp2) return false;
         }
      }
   }

//----------------------------

   struct S_sel_help{
      PIGraph igraph;
      C_vector<C_smart_ptr<I3D_frame> > *in_list;
      C_vector<C_smart_ptr<I3D_frame> > *out_list;
      C_edit_Selection_imp *e_slct;
      S_sel_help(): in_list(NULL){}
   };

//----------------------------

   static BOOL CALLBACK dlgObjSel(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            S_sel_help *hlp = (S_sel_help*)lParam;
            InitDlg(hlp->igraph, hwnd);
            ShowGroupButtons(hwnd, (hlp->in_list==NULL));
      
            C_vector<C_smart_ptr<I3D_frame> > *sel_list = hlp->out_list;
            SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)hlp);

            InitCommonControls();
            HIMAGELIST il;
            il = ImageList_LoadBitmap(GetHInstance(), "objselect_list", 12, 1, 0x808000);
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)il);

            for(int i=0; i < (sizeof(objsel_list)/sizeof(S_objsel_column)); i++){
               LVCOLUMN lvc;
               memset(&lvc, 0, sizeof(lvc));
               lvc.mask = LVCF_WIDTH;
               lvc.cx = hlp->e_slct->objsel_list_width[i];
               if(objsel_list[i].title){
                  lvc.pszText = (char*)objsel_list[i].title;
                  lvc.cchTextMax = strlen(objsel_list[i].title);
                  lvc.mask |= LVCF_TEXT;
               }
               if(objsel_list[i].image){
                  lvc.mask |= LVCFMT_IMAGE;
                  lvc.mask |= LVCF_FMT;
                  lvc.fmt = LVCFMT_IMAGE;
                  lvc.iImage = 2;
               }
               SendDlgItemMessage(hwnd, IDC_LIST, LVM_INSERTCOLUMN, i , (dword)&lvc);
            }
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETEXTENDEDLISTVIEWSTYLE, 
               LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP,
               LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP/* | LVS_EX_GRIDLINES*/);
               //LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            /*
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETEXTENDEDLISTVIEWSTYLE, 
               LVS_EX_SUBITEMIMAGES, LVS_EX_SUBITEMIMAGES);
               */
            SetDlgItemInt(hwnd, IDC_NUM_SEL, sel_list->size(), false);

            hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *sel_list);

            ShowWindow(hwnd, SW_SHOW);
         }
         return 1;

      case WM_HELP:
         break;

      case WM_NOTIFY:
         {
            NMHDR *nm = (NMHDR*)lParam;
            switch(wParam){
            case IDC_LIST:
               switch(nm->code){
                  /*
               case NM_CUSTOMDRAW:
                  {
                     NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW*)lParam;
                     //SetBkColor(cd->nmcd.hdc, 0);
                     //Rectangle(cd->nmcd.hdc, cd->nmcd.rc.left, cd->nmcd.rc.top, cd->nmcd.rc.right, cd->nmcd.rc.bottom);
                     HBRUSH hbr = CreateSolidBrush(0);
                     FillRect(cd->nmcd.hdc, &cd->nmcd.rc, hbr);
                     DeleteObject(hbr);
                  }
                  break;
                  */
               case LVN_COLUMNCLICK:
                  {
                     struct S_hlp1{
                        int sort_type;
                        C_edit_Selection_imp *e_slct;
                        t_GetFrameScript *PGetFrameScript;
                        t_GetFrameActor *PGetFrameActor;

                        static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort){

                           PI3D_frame f1 = (PI3D_frame)lParam1;
                           PI3D_frame f2 = (PI3D_frame)lParam2;
                           S_hlp1 *hp = (S_hlp1*)lParamSort;
                           switch(hp->sort_type){
                           case 0:     //type
                              {
                                 dword ft1 = f1->GetType();
                                 dword ft2 = f2->GetType();
                                 if(ft1!=ft2)
                                    return (ft1<ft2 ? -1 : 1);
                                 return 0;
                              }
                              break;
                           case 2:     //actor
                              if(hp->PGetFrameActor){
                                 const char *cp1 = (*(hp->PGetFrameActor))(f1);
                                 const char *cp2 = (*(hp->PGetFrameActor))(f2);
                                 if(!cp1 && !cp2)
                                    return stricmp(f1->GetName(), f2->GetName());
                                 if(!cp1) return -1;
                                 if(!cp2) return 1;
                                 int i = stricmp(cp1, cp2);
                                 if(!i) i = stricmp(f1->GetName(), f2->GetName());
                                 return i;
                              }
                              break;
                           case 3:     //script
                              if(hp->PGetFrameScript){
                                 const char *scr_1 = (*(hp->PGetFrameScript))(f1);
                                 const char *scr_2 = (*(hp->PGetFrameScript))(f2);
                                 if(!scr_1 && !scr_2) return stricmp(f1->GetName(), f2->GetName());
                                 if(!scr_1) return -1;
                                 if(!scr_2) return 1;
                                 int i = stricmp(scr_1, scr_2);
                                 if(!i) i = stricmp(f1->GetName(), f2->GetName());
                                 return i;
                              }
                              break;
                           case 4:     //subtype
                              if(f1->GetType() < f2->GetType()) return -1;
                              if(f1->GetType() > f2->GetType()) return -1;
                              {
                                 switch(f1->GetType()){
                                 case FRAME_VISUAL:
                                    {
                                       dword t1 = I3DCAST_VISUAL(f1)->GetVisualType();
                                       dword t2 = I3DCAST_VISUAL(f2)->GetVisualType();
                                       return (t1<t2) ? -1 : (t1>t2) ? 1 :
                                          strcmp(f1->GetName(), f2->GetName());
                                    }
                                    break;
                                 case FRAME_LIGHT:
                                    {
                                       I3D_LIGHTTYPE lt1 = I3DCAST_LIGHT(f1)->GetLightType();
                                       I3D_LIGHTTYPE lt2 = I3DCAST_LIGHT(f2)->GetLightType();
                                       return (lt1<lt2) ? -1 : (lt1>lt2) ? 1 : strcmp(f1->GetName(), f2->GetName());
                                    }
                                    break;
                                 case FRAME_SOUND:
                                    {
                                       I3D_SOUNDTYPE st1 = I3DCAST_SOUND(f1)->GetSoundType();
                                       I3D_SOUNDTYPE st2 = I3DCAST_SOUND(f2)->GetSoundType();
                                       return (st1<st2) ? -1 : (st1>st2) ? 1 : strcmp(f1->GetName(), f2->GetName());
                                    }
                                    break;
                                 case FRAME_MODEL:
                                    {
                                       const char *n1 = I3DCAST_MODEL(f1)->GetFileName();
                                       const char *n2 = I3DCAST_MODEL(f2)->GetFileName();
                                       int i = stricmp(n1, n2);
                                       if(!i) i = strcmp(f1->GetName(), f2->GetName());
                                       return i;
                                    }
                                    break;
                                 case FRAME_VOLUME:
                                    {
                                       I3D_VOLUMETYPE vt1 = I3DCAST_VOLUME(f1)->GetVolumeType();
                                       I3D_VOLUMETYPE vt2 = I3DCAST_VOLUME(f2)->GetVolumeType();
                                       return (vt1<vt2) ? -1 : (vt1>vt2) ? 1 : strcmp(f1->GetName(), f2->GetName());
                                    }
                                    break;
                                 default:
                                    return strcmp(f1->GetName(), f2->GetName());
                                 }
                              }
                              break;
                           }
                           return 0;
                        }
                     } hlp1;
                     NMLISTVIEW *nml = (NMLISTVIEW*)lParam;
                     if(nml->iSubItem==1){   //sort by frame
                        S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                        hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
                     }else{
                        S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                        hlp1.sort_type = nml->iSubItem;
                        hlp1.e_slct = hlp->e_slct;
                        hlp1.PGetFrameScript = hlp->e_slct->PGetFrameScript;
                        hlp1.PGetFrameActor = hlp->e_slct->PGetFrameActor;

                        SendDlgItemMessage(hwnd, IDC_LIST, LVM_SORTITEMS, (LPARAM)&hlp1, (dword)S_hlp1::CompareFunc);
                     }
                  }
                  break;
               }
               break;
            }
         }
         break;

      case WM_COMMAND:

         switch(HIWORD(wParam)){

         case EN_CHANGE:
            switch(LOWORD(wParam)){
            case IDC_INC_SEL:
               {
                  char buf[256];
                  SendDlgItemMessage(hwnd, IDC_INC_SEL, WM_GETTEXT, sizeof(buf)-1, (LPARAM)buf);
                  int len = strlen(buf);
                  if(!len)
                     break;
                              //space at the end is special, if present, the text typed
                              // is matched exactly (without the space), otherwise
                              // wild-cards are used
                  if(buf[len-1] == ' ')
                     buf[len-1] = 0;
                  else
                     buf[len++] = '*';
                  bool model_search = (buf[0]=='.');
                              //change contents of selection
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->out_list->clear();
                  int count = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0);
                  LVITEM lvi;
                  memset(&lvi, 0, sizeof(lvi));
                  lvi.mask = LVIF_PARAM | LVIF_STATE;
                  lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
                  int focus_index = -1;
                  int i;
                  for(i=0; i<count; i++){
                     lvi.iItem = i;
                     SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEM, 0, (LPARAM)&lvi);
                     PI3D_frame frm = (PI3D_frame)lvi.lParam;
                     const char *fn = frm->GetName();
                     bool b;
                     if(model_search){
                        const char *mn = strchr(fn, '.');
                        if(mn)
                           fn = mn+1;
                        b = StringMatch(fn, strlen(fn), buf+1, len-1);
                     }else
                        b = StringMatch(fn, strlen(fn), buf, len);
                     bool curr_state = (lvi.state&LVIS_SELECTED);
                     if(b != curr_state){
                        lvi.state &= ~(LVIS_SELECTED | LVIS_FOCUSED);
                        if(b){
                           lvi.state |= LVIS_SELECTED;
                           if(focus_index==-1){
                              lvi.state |= LVIS_FOCUSED;
                              focus_index = i;
                           }
                        }
                        SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
                     }else
                     if(curr_state && focus_index==-1){
                        focus_index = i;
                     }
                     if(b) hlp->out_list->push_back(frm);
                  }
                  if(focus_index!=-1)
                     SendDlgItemMessage(hwnd, IDC_LIST, LVM_ENSUREVISIBLE, focus_index, false);
                  SetDlgItemInt(hwnd, IDC_NUM_SEL, hlp->out_list->size(), false);
               }
               break;
            }
            break;

         case BN_CLICKED:
            switch(LOWORD(wParam)){

            case IDC_BUTTON_GRP_ALL:
               {
                  for(int i=0; i<LM_HIERARCHY; i++) list_mode[i] = true;
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  ShowGroupButtons(hwnd, (hlp->in_list==NULL));
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;

            case IDC_BUTTON_GRP_NONE:
               {
                  for(int i=0; i<LM_HIERARCHY; i++) list_mode[i] = false;
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  ShowGroupButtons(hwnd, (hlp->in_list==NULL));
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;

            case IDC_BUTTON_SEL_ALL:
            case IDC_BUTTON_SEL_NONE:
            case IDC_BUTTON_SEL_INV:
               {
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->out_list->clear();
                  int count = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0);
                  LVITEM lvi;
                  memset(&lvi, 0, sizeof(lvi));
                  lvi.mask = LVIF_PARAM | LVIF_STATE;
                  lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
                  for(int i=0; i<count; i++){
                     lvi.iItem = i;
                     SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEM, 0, (LPARAM)&lvi);
                     PI3D_frame frm = (PI3D_frame)lvi.lParam;
                     switch(LOWORD(wParam)){
                     case IDC_BUTTON_SEL_ALL:
                        lvi.state |= LVIS_SELECTED;
                        break;
                     case IDC_BUTTON_SEL_NONE:
                        lvi.state &= ~LVIS_SELECTED;
                        break;
                     case IDC_BUTTON_SEL_INV:
                        lvi.state ^= LVIS_SELECTED;
                        break;
                     }
                     SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
                     if(lvi.state&LVIS_SELECTED)
                        hlp->out_list->push_back(frm);
                  }
                  SetDlgItemInt(hwnd, IDC_NUM_SEL, hlp->out_list->size(), false);
               }
               break;

            case IDC_CHECK_VIS:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_VISUAL]=IsDlgButtonChecked(hwnd, IDC_CHECK_VIS);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_LIGHT:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_LIGHT]=IsDlgButtonChecked(hwnd, IDC_CHECK_LIGHT);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_MODEL:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_MODEL]=IsDlgButtonChecked(hwnd, IDC_CHECK_MODEL);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_SND:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_SOUND]=IsDlgButtonChecked(hwnd, IDC_CHECK_SND);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;

            case IDC_CHECK_OCC:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_OCCLUDER]=IsDlgButtonChecked(hwnd, IDC_CHECK_OCC);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_VOL:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_VOLUME]=IsDlgButtonChecked(hwnd, IDC_CHECK_VOL);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_DUM:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_DUMMY]=IsDlgButtonChecked(hwnd, IDC_CHECK_DUM);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_SCT:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_SECTOR]=IsDlgButtonChecked(hwnd, IDC_CHECK_SCT);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_CAM:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_CAMERA]=IsDlgButtonChecked(hwnd, IDC_CHECK_CAM);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
            case IDC_CHECK_JOINT:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_JOINT]=IsDlgButtonChecked(hwnd, IDC_CHECK_JOINT);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
            case IDC_CHECK_USER:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_USER]=IsDlgButtonChecked(hwnd, IDC_CHECK_USER);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
            case IDC_CHECK_HIER:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_HIERARCHY]=IsDlgButtonChecked(hwnd, IDC_CHECK_HIER);
                  EnableWindow(GetDlgItem(hwnd, IDC_CHECK_HIDE), list_mode[LM_HIERARCHY]);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;
            case IDC_CHECK_HIDE:
               if(HIWORD(wParam)==BN_CLICKED){
                  list_mode[LM_HIDE]=IsDlgButtonChecked(hwnd, IDC_CHECK_HIDE);
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                  hlp->e_slct->AddObjsToList(hwnd, hlp->in_list, *hlp->out_list);
               }
               break;

            case IDCLOSE:
            case IDCANCEL:
               EndDialog(hwnd, 0);
               break;

            case IDOK:
               {
                  S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
                                    //collect selection and select
                  hlp->out_list->clear();
                  int count = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0);
                  LVITEM lvi;
                  memset(&lvi, 0, sizeof(lvi));
                  lvi.mask = LVIF_PARAM | LVIF_STATE;
                  lvi.stateMask = LVIS_SELECTED;
                  for(int i=0; i<count; i++){
                     lvi.iItem = i;
                     SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEM, 0, (LPARAM)&lvi);
                     PI3D_frame frm = (PI3D_frame)lvi.lParam;
                     if(lvi.state&LVIS_SELECTED) hlp->out_list->push_back(frm);
                  }
                  EndDialog(hwnd, 1);
               }
               break;
            }
            break;
         }
         break;

      case WM_DESTROY:
         {
            S_sel_help *hlp = (S_sel_help*)GetWindowLong(hwnd, GWL_USERDATA);
            for(int i=0; i < (sizeof(objsel_list)/sizeof(S_objsel_column)); i++){
               hlp->e_slct->objsel_list_width[i] =
                  SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETCOLUMNWIDTH, i , 0);
            }
         }
         break;
      }
      return 0;
   }

//----------------------------

   BOOL dlgObjSelModeless(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            if(sel_dlg_pos.x==0x80000000)
               InitDlg(ed->GetIGraph(), hwnd);
            else{
               SetWindowPos(hwnd, NULL, sel_dlg_pos.x, sel_dlg_pos.y, 0, 0,
                  SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
            }
            ShowGroupButtons(hwnd);
      
            InitCommonControls();
            HIMAGELIST il;
            il = ImageList_LoadBitmap(GetHInstance(), "objselect_list", 12, 1, 0x808000);
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)il);

            for(int i=0; i < (sizeof(objsel_list)/sizeof(S_objsel_column)); i++){
               LVCOLUMN lvc;
               memset(&lvc, 0, sizeof(lvc));
               lvc.mask = LVCF_WIDTH;
               lvc.cx = objsel_list_width[i];
               if(objsel_list[i].title){
                  lvc.pszText = (char*)objsel_list[i].title;
                  lvc.cchTextMax = strlen(objsel_list[i].title);
                  lvc.mask |= LVCF_TEXT;
               }
               if(objsel_list[i].image){
                  lvc.mask |= LVCFMT_IMAGE;
                  lvc.mask |= LVCF_FMT;
                  lvc.fmt = LVCFMT_IMAGE;
                  lvc.iImage = 2;
               }
               SendDlgItemMessage(hwnd, IDC_LIST, LVM_INSERTCOLUMN, i , (dword)&lvc);
            }
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETEXTENDEDLISTVIEWSTYLE, 
               LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES, 
               LVS_EX_FULLROWSELECT);
               //LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            SetDlgItemInt(hwnd, IDC_NUM_SEL, sel_list.size(), false);

            AddObjsToList(hwnd, NULL, sel_list);

            ShowWindow(hwnd, SW_SHOW);
         }
         return 1;

      case WM_NOTIFY:
         {
            NMHDR *nm = (NMHDR*)lParam;
            switch(wParam){
            case IDC_LIST:
               switch(nm->code){
               case LVN_COLUMNCLICK:
                  {
                     struct S_hlp1{
                        int sort_type;
                        C_edit_Selection_imp *e_slct;
                        t_GetFrameScript *PGetFrameScript;
                        t_GetFrameActor *PGetFrameActor;

                        static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort){

                           PI3D_frame f1 = (PI3D_frame)lParam1;
                           PI3D_frame f2 = (PI3D_frame)lParam2;
                           S_hlp1 *hp = (S_hlp1*)lParamSort;
                           switch(hp->sort_type){
                           case 0:     //type
                           case 4:     //subtype
                              {
                                 dword ft1 = f1->GetType();
                                 dword ft2 = f2->GetType();
                                 if(ft1!=ft2)
                                    return (ft1<ft2 ? -1 : 1);
                                 return 0;
                              }
                              break;
                           case 2:     //actor
                              if(hp->PGetFrameActor){
                                 const char *cp1 = (*(hp->PGetFrameActor))(f1);
                                 const char *cp2 = (*(hp->PGetFrameActor))(f2);
                                 if(!cp1 && !cp2) return stricmp(f1->GetName(), f2->GetName());
                                 if(!cp1) return -1;
                                 if(!cp2) return 1;
                                 int i = stricmp(cp1, cp2);
                                 if(!i) i = stricmp(f1->GetName(), f2->GetName());
                                 return i;
                              }
                              break;
                           case 3:     //script
                              if(hp->PGetFrameScript){
                                 const char *scr_1 = (*(hp->PGetFrameScript))(f1);
                                 const char *scr_2 = (*(hp->PGetFrameScript))(f2);
                                 if(!scr_1 && !scr_2) return stricmp(f1->GetName(), f2->GetName());
                                 if(!scr_1) return -1;
                                 if(!scr_2) return 1;
                                 int i = stricmp(scr_1, scr_2);
                                 if(!i) i = stricmp(f1->GetName(), f2->GetName());
                                 return i;
                              }
                              break;
                           }
                           return 0;
                        }
                     } hlp1;
                     NMLISTVIEW *nml = (NMLISTVIEW*)lParam;
                     if(nml->iSubItem==1){   //sort by frame
                        AddObjsToList(hwnd, NULL, sel_list);
                     }else{
                        hlp1.sort_type = nml->iSubItem;
                        hlp1.e_slct = this;
                        hlp1.PGetFrameScript = PGetFrameScript;
                        hlp1.PGetFrameActor = PGetFrameActor;

                        SendDlgItemMessage(hwnd, IDC_LIST, LVM_SORTITEMS, (LPARAM)&hlp1, (dword)S_hlp1::CompareFunc);
                     }
                  }
                  break;

               case LVN_ITEMCHANGED:
                  {
                     const NMLISTVIEW *lv = (NMLISTVIEW*)lParam;
                     if(lv->uChanged&LVIF_STATE){
                        if((lv->uNewState&LVIS_SELECTED) != (lv->uOldState&LVIS_SELECTED)){
                           PI3D_frame frm = (PI3D_frame)lv->lParam;
                           //bool st;
                           if(lv->uNewState&LVIS_SELECTED){
                              AddFrame(frm);  
                           }else{
                              RemoveFrame(frm);
                           }
                           //assert(st);
                        }
                     }
                  }
                  break;
               }
               break;
            }
         }
         break;

      case WM_COMMAND:
         switch(HIWORD(wParam)){
         case EN_CHANGE:
            switch(LOWORD(wParam)){
            case IDC_INC_SEL:
               {
                  char buf[256];
                  SendDlgItemMessage(hwnd, IDC_INC_SEL, WM_GETTEXT, sizeof(buf)-1, (LPARAM)buf);
                  int len = strlen(buf);
                  if(!len)
                     break;
                              //space at the end is special, if present, the text typed
                              // is matched exactly (without the space), otherwise
                              // wild-cards are used
                  if(buf[len-1] == ' ')
                     buf[len-1] = 0;
                  else
                     buf[len++] = '*';
                  bool model_search = (buf[0]=='.');

                              //change contents of selection
                  //Clear();

                  int count = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0);
                  LVITEM lvi;
                  memset(&lvi, 0, sizeof(lvi));
                  lvi.mask = LVIF_PARAM | LVIF_STATE;
                  lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
                  int focus_index = -1;
                  int i;
                  for(i=0; i<count; i++){
                     lvi.iItem = i;
                     SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEM, 0, (LPARAM)&lvi);
                     PI3D_frame frm = (PI3D_frame)lvi.lParam;
                     const char *fn = frm->GetName();
                     bool b;
                     if(model_search){
                        const char *mn = strchr(fn, '.');
                        if(mn)
                           fn = mn+1;
                        b = StringMatch(fn, strlen(fn), buf+1, len-1);
                     }else
                        b = StringMatch(fn, strlen(fn), buf, len);
                     bool curr_state = (lvi.state&LVIS_SELECTED);
                     if(b != curr_state){
                        lvi.state &= ~(LVIS_SELECTED | LVIS_FOCUSED);
                        if(b){
                           lvi.state |= LVIS_SELECTED;
                           if(focus_index==-1){
                              lvi.state |= LVIS_FOCUSED;
                              focus_index = i;
                           }
                        }
                        SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
                     }else
                     if(curr_state && focus_index==-1){
                        focus_index = i;
                     }
                     //if(b) AddFrame(frm);
                  }
                  if(focus_index!=-1)
                     SendDlgItemMessage(hwnd, IDC_LIST, LVM_ENSUREVISIBLE, focus_index, false);
                  SetDlgItemInt(hwnd, IDC_NUM_SEL, sel_list.size(), false);
               }
               break;
            }
            break;

         case BN_CLICKED:
            {
               switch(LOWORD(wParam)){
               case IDC_BUTTON_GRP_ALL:
               case IDC_BUTTON_GRP_NONE:
                  {
                     bool on = (LOWORD(wParam)==IDC_BUTTON_GRP_ALL);
                     for(int i=0; i<LM_HIERARCHY; i++) list_mode[i] = on;
                     ShowGroupButtons(hwnd);
                     sel_win_reset = RESET_FULL;
                  }
                  break;

               case IDC_BUTTON_SEL_ALL:
               case IDC_BUTTON_SEL_NONE:
               case IDC_BUTTON_SEL_INV:
                  {
                     Clear();
                     int count = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0);
                     LVITEM lvi;
                     memset(&lvi, 0, sizeof(lvi));
                     lvi.mask = LVIF_PARAM | LVIF_STATE;
                     lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
                     for(int i=0; i<count; i++){
                        lvi.iItem = i;
                        SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEM, 0, (LPARAM)&lvi);
                        PI3D_frame frm = (PI3D_frame)lvi.lParam;
                        switch(LOWORD(wParam)){
                        case IDC_BUTTON_SEL_ALL:
                           lvi.state |= LVIS_SELECTED;
                           break;
                        case IDC_BUTTON_SEL_NONE:
                           lvi.state &= ~LVIS_SELECTED;
                           break;
                        case IDC_BUTTON_SEL_INV:
                           lvi.state ^= LVIS_SELECTED;
                           break;
                        }
                        SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
                        if(lvi.state&LVIS_SELECTED)
                           AddFrame(frm);
                     }
                     SetDlgItemInt(hwnd, IDC_NUM_SEL, sel_list.size(), false);
                  }
                  break;

               case IDC_CHECK_HIER:
                  EnableWindow(GetDlgItem(hwnd, IDC_CHECK_HIDE), IsDlgButtonChecked(hwnd, LOWORD(wParam)));
                              //flow...
               case IDC_CHECK_VIS:
               case IDC_CHECK_LIGHT:
               case IDC_CHECK_MODEL:
               case IDC_CHECK_SND:
               case IDC_CHECK_OCC:
               case IDC_CHECK_VOL:
               case IDC_CHECK_DUM:
               case IDC_CHECK_SCT:
               case IDC_CHECK_CAM:
               case IDC_CHECK_JOINT:
               case IDC_CHECK_USER:
               case IDC_CHECK_HIDE:
                  {
                     static const struct{
                        word ctrl;
                        E_SEL_GROUP grp;
                     } s_map[] = {
                        IDC_CHECK_VIS, LM_VISUAL,
                        IDC_CHECK_LIGHT, LM_LIGHT,
                        IDC_CHECK_MODEL, LM_MODEL,
                        IDC_CHECK_SND, LM_SOUND,
                        IDC_CHECK_OCC, LM_OCCLUDER,
                        IDC_CHECK_VOL, LM_VOLUME,
                        IDC_CHECK_DUM, LM_DUMMY,
                        IDC_CHECK_SCT, LM_SECTOR,
                        IDC_CHECK_CAM, LM_CAMERA,
                        IDC_CHECK_JOINT, LM_JOINT,
                        IDC_CHECK_USER, LM_USER,
                        IDC_CHECK_HIER, LM_HIERARCHY,
                        IDC_CHECK_HIDE, LM_HIDE,
                        0
                     };
                     bool checked = IsDlgButtonChecked(hwnd, LOWORD(wParam));
                     for(int i=0; s_map[i].ctrl; i++){
                        if(s_map[i].ctrl==LOWORD(wParam)){
                           list_mode[s_map[i].grp] = checked;
                           sel_win_reset = RESET_FULL;
                           break;
                        }
                     }
                  }
                  break;

               case IDCANCEL:
                  //SetFocus((HWND)ed->GetIGraph()->GetHWND());
                  DestroySelWindow();
                  break;

               case IDCLOSE:
               case IDOK:
                  DestroySelWindow();
                  break;

                  /*
               case IDOK:
                  {
                                 //collect selection and select
                     sel_list.clear();
                     int count = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEMCOUNT, 0, 0);
                     LVITEM lvi;
                     memset(&lvi, 0, sizeof(lvi));
                     lvi.mask = LVIF_PARAM | LVIF_STATE;
                     lvi.stateMask = LVIS_SELECTED;
                     for(int i=0; i<count; i++){
                        lvi.iItem = i;
                        SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETITEM, 0, (LPARAM)&lvi);
                        PI3D_frame frm = (PI3D_frame)lvi.lParam;
                        if(lvi.state&LVIS_SELECTED)
                           sel_list.push_back(frm);
                     }
                     DestroySelWindow();
                  }
                  break;
                  */
               }
            }
            break;
         }
         break;

      case WM_DESTROY:
         {
            RECT rc;
            GetWindowRect(hwnd, &rc);
            sel_dlg_pos.x = rc.left;
            sel_dlg_pos.y = rc.top;
            for(int i=0; i < (sizeof(objsel_list)/sizeof(S_objsel_column)); i++){
               objsel_list_width[i] = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETCOLUMNWIDTH, i , 0);
            }
         }
         break;
      }
      return 0;
   }

//----------------------------

   static BOOL CALLBACK dlgObjSelModeless_thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      C_edit_Selection_imp *es = (C_edit_Selection_imp*)GetWindowLong(hwnd, GWL_USERDATA);
      if(es)
         return es->dlgObjSelModeless(hwnd, uMsg, wParam, lParam);
      return 0;
   }

//----------------------------

   struct S_flash_item{
      C_smart_ptr<I3D_frame> frm;
      int count_down;
      bool persistent;
      dword color;

      S_flash_item():
         persistent(false)
      {}
      S_flash_item(PI3D_frame f1, dword c, dword time):
         frm(f1),
         color(c),
         count_down(time),
         persistent(time==0)
      {
         //ResetCount();
      }
      void ResetCount(){
         count_down = 250;
      }
   };

   C_vector<C_smart_ptr<I3D_frame> > sel_list;
   C_vector<S_flash_item> flash_list;
   int sb_index;              //index in status bar

                              //list of plugins to which we send notifications
   typedef C_vector<pair<C_smart_ptr<C_editor_item>, dword> > t_notify_list;
   t_notify_list notify_list;

   void SendNotify(bool own_notify = true){
      for(dword i=0; i<notify_list.size(); i++){
         notify_list[i].first->Action(notify_list[i].second, &sel_list);
      }
      if(own_notify && hwnd_sel)
         sel_win_reset = RESET_SMART;
   }

//----------------------------

   void Message(){
      ed->Message(C_fstr("Selected: %i", sel_list.size()), sb_index);

      bool any_sel = (sel_list.size()!=0);
      ed->EnableMenu(this, E_SELECTION_CLEAR2, any_sel);
      ed->EnableMenu(this, E_SELECTION_PARENTS, any_sel);
      ed->EnableMenu(this, E_SELECTION_CHILDREN, any_sel);
   }

   bool draw_links;

   /*
//----------------------------
// helper function - build list of names separated by '\0',
// last name is followed by two '\0' characters
   static void BuildNameList(const C_vector<C_smart_ptr<I3D_frame> > &frm_list, char **buf, dword *len){

      for(dword i=0, nlen=0; i<frm_list.size(); i++){
         nlen += frm_list[i]->GetName().Size() + 1;
      }
      *buf = new char[nlen + 1];
      for(i=0, nlen=0; i<frm_list.size(); i++){
         const C_str &cp = frm_list[i]->GetName();
         strcpy(&(*buf)[nlen], cp);
         nlen += cp.Size() + 1;
      }
      (*buf)[nlen] = 0;
      *len = nlen+1;
   }
   */

//----------------------------

   HWND hwnd_sel;
   enum{
      RESET_NO,
      RESET_FULL,
      RESET_SMART,
   } sel_win_reset;
   POINT sel_dlg_pos;

   void DestroySelWindow(){
      if(hwnd_sel){
         ed->GetIGraph()->RemoveDlgHWND(hwnd_sel);
         DestroyWindow(hwnd_sel);
         hwnd_sel = NULL;
      }
   }

//----------------------------

   virtual const C_vector<PI3D_frame> &GetCurSel() const{ return (const C_vector<PI3D_frame>&)sel_list; }

//----------------------------

   virtual PI3D_frame GetSingleSel() const{

      return (sel_list.size()==1) ? ((PI3D_frame)(CPI3D_frame)sel_list.front()) : NULL;
   }

//----------------------------

   virtual void Clear(){

      if(!e_medit) e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      const S_MouseEdit_user_mode *musr = e_medit->GetUserMode();
      if(musr){
         musr->ei->Action(musr->modes[S_MouseEdit_user_mode::MODE_CLEAR_SEL].action_id);
      }else
      if(sel_list.size()){
                        //save undo info
         for(dword i=sel_list.size(); i--; ){
            e_undo->Begin(this, UNDO_SELECT, sel_list[i]);
            e_undo->End();
         }
                        //clear selection
         sel_list.clear();
         Message();
         SendNotify();
      }
   }

//----------------------------

   virtual void Invert(){

      if(!e_medit) e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      const S_MouseEdit_user_mode *musr = e_medit->GetUserMode();
      if(musr){
         musr->ei->Action(musr->modes[S_MouseEdit_user_mode::MODE_INV_SEL].action_id);
         return;
      }
                        //save undo info 
      /*
      if(e_undo->IsTopEntry(this, UNDO_INVERT)){
         e_undo->PopTopEntry();
      }else
      */
      {
         e_undo->Begin(this, UNDO_INVERT);
         e_undo->End();
      }
                        //invert selection
      C_vector<C_smart_ptr<I3D_frame> > all_list;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
            C_vector<C_smart_ptr<I3D_frame> > &sel_list = *(C_vector<C_smart_ptr<I3D_frame> >*)c;
            sel_list.push_back(frm);
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&all_list);
      for(int i=all_list.size(); i--; ){
         if(find(sel_list.begin(), sel_list.end(), all_list[i])!=sel_list.end()){
            all_list[i] = all_list.back();
            all_list.pop_back();
         }
      }
      sel_list = all_list;
      Message();
      SendNotify();
   }

//----------------------------

   virtual void AddFrame(PI3D_frame frm){

      if(!frm){
         ed->Message("Selection::AddFrame: can't add NULL frame");
         return;
      }
      int j = FindPointerIndex((void**)(sel_list.size() ? &sel_list.front() : NULL), sel_list.size(), frm);
      if(j==-1){
                        //save undo info
         e_undo->Begin(this, UNDO_DESELECT, frm);
         e_undo->End();

         sel_list.push_back(frm);

         Message();

         SendNotify();
      }else{
         ed->Message("Selection::AddFrame: frame already in selection!");
      }
   }

//----------------------------

   virtual void RemoveFrame(PI3D_frame frm){

      int j = FindPointerIndex((void**)(sel_list.size() ? &sel_list.front() : NULL), sel_list.size(), frm);
      if(j!=-1){
                        //save undo info
         e_undo->Begin(this, UNDO_SELECT, frm);
         e_undo->End();

         sel_list[j] = sel_list.back(); sel_list.pop_back();

         Message();
         SendNotify();
      }
   }

//----------------------------

   virtual void FlashFrame(PI3D_frame frm, dword time, dword color){

      for(int j=flash_list.size(); j--; )
         if(frm==flash_list[j].frm)
            break;
      if(j==-1){
         flash_list.push_back(S_flash_item(frm, color, time));
      }else
         flash_list[j].ResetCount();
   }

//----------------------------

   virtual bool Prompt(C_vector<PI3D_frame> &prompt_list){

      C_vector<C_smart_ptr<I3D_frame> > tmp_list;
      tmp_list.reserve(prompt_list.size());
      for(int i=prompt_list.size(); i--; )
         tmp_list.push_back(prompt_list[i]);
      C_vector<C_smart_ptr<I3D_frame> > new_list;

      S_sel_help hlp;
      hlp.igraph = ed->GetIGraph();
      hlp.in_list = &tmp_list;
      hlp.out_list = &new_list;
      hlp.e_slct = this;

      bool b = DialogBoxParam(GetHInstance(), "IDD_OBJSELECT", (HWND)ed->GetIGraph()->GetHWND(), dlgObjSel, (LPARAM)&hlp);
      if(b){
         prompt_list.clear();
         prompt_list.reserve(new_list.size());
         for(i=new_list.size(); i--; )
            prompt_list.push_back(new_list[i]);
      }
      return b;
   }

//----------------------------

   virtual void AddNotify(PC_editor_item ei, dword msg){

      for(dword i=notify_list.size(); i--; )
         if(ei==notify_list[i].first)
            return;
      notify_list.push_back(pair<C_smart_ptr<C_editor_item>, dword>(ei, msg));
   }

//----------------------------

   virtual void RemoveNotify(PC_editor_item ei){

      for(dword i=notify_list.size(); i--; ){
         if(ei==notify_list[i].first){
            notify_list[i] = notify_list.back();
            notify_list.pop_back();
            break;
         }
      }
   }

//----------------------------

   virtual dword GetStatusBarIndex() const{ return sb_index; }

//----------------------------

   virtual void SetGetScriptNameFunc(t_GetFrameScript *f){ PGetFrameScript = f; }
   virtual void SetGetActorNameFunc(t_GetFrameActor *f){ PGetFrameActor = f; }

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      switch(id){
      case UNDO_SELECT:
         AddFrame(frm);
         break;

      case UNDO_DESELECT:
         RemoveFrame(frm);
         break;

      case UNDO_INVERT:
         Invert();
         break;

      case UNDO_CLEAR:
         Clear();
         break;

      default: assert(0);
      }
   }

//----------------------------

   virtual void AfterLoad(){

      Message();
      SendNotify();
   }

//----------------------------

   virtual void BeforeFree(){
      sel_list.clear();
      flash_list.clear();
      Message();
   }

//----------------------------

   virtual void LoadFromMission(C_chunk &ck){

      sel_list.clear();
      while(ck)
      switch(++ck){
      case CT_NAME:
         {
            C_str str = ck.RStringChunk();
            PI3D_frame frm = ed->GetScene()->FindFrame(str);
            if(frm) 
               sel_list.push_back(frm);
         }
         break;
      default: --ck;
      }
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==2){
                           //save current selection
         if(sel_list.size()){
            ck <<= CT_EDITOR_PLUGIN;
            ck.WStringChunk(CT_NAME, GetName());
            for(dword i=0; i<sel_list.size(); i++) 
               ck.WStringChunk(CT_NAME, sel_list[i]->GetName());
            --ck;
         }
      }
   }

//----------------------------

   virtual void OnFrameDelete(PI3D_frame frm){
                        //check if this frame (or its children) are in our flash list
      struct S_hlp{
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

            C_vector<C_edit_Selection_imp::S_flash_item> &flash_list = *(C_vector<C_edit_Selection_imp::S_flash_item>*)c;
            for(int i=flash_list.size(); i--; ){
               if(frm==flash_list[i].frm && flash_list[i].persistent){
                  flash_list[i] = flash_list.back(); flash_list.pop_back();
                  break;
               }
            }
            return I3DENUMRET_OK;
         }
      };
      S_hlp::cbEnum(frm, (dword)&flash_list);
      frm->EnumFrames(S_hlp::cbEnum, (dword)&flash_list);
   }

//----------------------------
public:
   C_edit_Selection_imp():
      hwnd_sel(NULL),
      sel_win_reset(RESET_NO),
      view_axis_on(true),
      PGetFrameScript(NULL),
      PGetFrameActor(NULL),
      draw_links(true)
   {
      sel_dlg_pos.x = 0x80000000;
      memcpy(objsel_list_width, objsel_list_width_defaults, sizeof(objsel_list_width));
   }

   virtual bool Init(){
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      if(!e_undo){
         return false;
      }

      ed->AddShortcut(this, E_SELECTION_BROWSE, "%10 &Edit\\%40 %i Select by &name\tN", K_N, 0);
      ed->AddShortcut(this, E_SELECTION_CLEAR2, "&Edit\\%40 Clear selection\tCtrl+N", K_N, SKEY_CTRL);
      ed->AddShortcut(this, E_SELECTION_INVERT2, "&Edit\\%40 Invert selection", K_NOKEY, 0);
      ed->AddShortcut(this, E_SELECTION_PARENTS, "&Edit\\%40 Select parent\tPgUp", K_PAGEUP, 0);
      ed->AddShortcut(this, E_SELECTION_CHILDREN, "&Edit\\%40 Select children\tPgDn", K_PAGEDOWN, 0);
      ed->AddShortcut(this, E_SELECTION_TOGGLE_LINK, "&Edit\\%40 Draw &links\tCtrl+Alt+L", K_L, SKEY_CTRL | SKEY_ALT);
      //ed->AddShortcut(this, E_SELECTION_CONFIG, "&Edit\\%40 Selection confi&g", 0, 0);
      ed->AddShortcut(this, E_SELECTION_CLEAR_FLASH, "%30 &Debug\\%51 Clear fla&shing", K_NOKEY, 0);
      ed->AddShortcut(this, E_SELECTION_SEL_FLASH_PREV, "%30 &Debug\\%52 Select prev flashing\tAlt+[", K_LBRACKET, SKEY_ALT);
      ed->AddShortcut(this, E_SELECTION_SEL_FLASH_NEXT, "%30 &Debug\\%52 Select next flashing\tAlt+]", K_RBRACKET, SKEY_ALT);

      ed->AddShortcut(this, E_SELECTION_TOGGLE_AXIS, "%30 &View\\&Axis\tCtrl+Alt+A", K_A, SKEY_CTRL | SKEY_ALT);

      ed->CheckMenu (this, E_SELECTION_TOGGLE_LINK, draw_links);
      ed->CheckMenu(this, E_SELECTION_TOGGLE_AXIS, view_axis_on);

      sb_index = ed->CreateStatusIndex(80);

      return true;
   
   }
   virtual void Close(){ 

      DestroySelWindow();
      sel_list.clear();
      flash_list.clear();
      e_undo = NULL;
      e_medit = NULL;

      notify_list.clear();
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){

         /*
      //case E_SELECTION_GETVECTOR: return (dword)&sel_list;

      case E_SELECTION_CLEAR:
         Clear();
         break;
         */

      case E_SELECTION_INVERT2:
         Invert();
         break;

      case E_SELECTION_BROWSE:
         {
#if 0
            if(hwnd_sel){
               SetFocus(hwnd_sel);
               break;
            }
            hwnd_sel = CreateDialogParam(GetHInstance(), "IDD_OBJSELECT", (HWND)ed->GetIGraph()->GetHWND(),
               dlgObjSelModeless_thunk, (LPARAM)this);
            ed->GetIGraph()->AddDlgHWND(hwnd_sel);
#else
            int i;
            C_vector<C_smart_ptr<I3D_frame> > new_list;
            new_list = sel_list;

            S_sel_help hlp;
            hlp.igraph = ed->GetIGraph();
            hlp.out_list = &new_list;
            hlp.e_slct = this;
            i = DialogBoxParam(GetHInstance(), "IDD_OBJSELECT", (HWND)ed->GetIGraph()->GetHWND(), dlgObjSel, (LPARAM)&hlp);
            if(i){            //unselect old and select new
                              //clear current selection
               Clear();

                              //save undo info
               e_undo->Begin(this, UNDO_CLEAR);
               e_undo->End();

               sel_list = new_list;

               Message();
               SendNotify();
            }
#endif
         }
         break;

         /*
      case E_SELECTION_PROMPT:
         break;
         */

      case E_SELECTION_CLEAR2:
         if(sel_list.size()){
            Clear();
            ed->Message("Selection cleared");
         }
         break;

      case E_SELECTION_CLEAR_FLASH:
         flash_list.clear();
         break;

      case E_SELECTION_PARENTS:
         {
            C_vector<C_smart_ptr<I3D_frame> > new_list;
            for(int i=sel_list.size(); i--; ){
               PI3D_frame prnt = sel_list[i]->GetParent();
               if(prnt){
                  for(int j=new_list.size(); j--; ) if(prnt==new_list[j]) break;
                  if(j==-1) new_list.push_back(prnt);
               }
            }
                              //clear current selection
            Clear();

                              //save undo info
            e_undo->Begin(this, UNDO_CLEAR);
            e_undo->End();

            sel_list = new_list;

            Message();
            SendNotify();
         }
         break;

      case E_SELECTION_CHILDREN:
         {
            C_vector<C_smart_ptr<I3D_frame> > new_list;
            for(int i=sel_list.size(); i--; ){
               for(int ii=sel_list[i]->NumChildren(); ii--; ){
                  PI3D_frame chld = sel_list[i]->GetChildren()[ii];
                  for(int j=new_list.size(); j--; ) if(chld==new_list[j]) break;
                  if(j==-1) new_list.push_back(chld);
               }
            }
                              //clear current selection
            Clear();

                              //save undo info
            e_undo->Begin(this, UNDO_CLEAR);
            e_undo->End();

            sel_list = new_list;

            Message();
            SendNotify();
         }
         break;

      case E_SELECTION_SEL_FLASH_NEXT:
      case E_SELECTION_SEL_FLASH_PREV:
         {
            if(!flash_list.size()){
               ed->Message("Flash list is empty");
               break;
            }
            PI3D_frame curr = (sel_list.size()==1) ? sel_list.front() : NULL;
            for(int i=flash_list.size(); i--; ){
               if(curr==flash_list[i].frm)
                  break;
            }
            Clear();
                              //save undo info
            e_undo->Begin(this, UNDO_CLEAR);
            e_undo->End();

            switch(id){
            case E_SELECTION_SEL_FLASH_NEXT: i = (i+1)%flash_list.size(); break;
            case E_SELECTION_SEL_FLASH_PREV: i = (i+flash_list.size()-1)%flash_list.size(); break;
            default: assert(0);
            }
            sel_list.push_back(flash_list[i].frm);
            ed->Message(C_fstr("Selected flashing frame: '%s'", (const char*)sel_list.front()->GetName()));

            Message();
            SendNotify();
         }
         break;

         /*
      case E_SELECTION_FLASH_FRAME:
      case E_SELECTION_BLINK_FRAME:
         {
            PI3D_frame frm = (PI3D_frame)context;
            for(int j=flash_list.size(); j--; )
               if(flash_list[j].frm==frm) break;
            if(j==-1){
               flash_list.push_back(S_flash_item(frm));
               if(id==E_SELECTION_BLINK_FRAME)
                  flash_list.back().persistent = true;
            }else
               flash_list[j].ResetCount();
         }
         break;
         */

      case E_SELECTION_TOGGLE_LINK:
         {
            draw_links = !draw_links;
            ed->CheckMenu (this, E_SELECTION_TOGGLE_LINK, draw_links);
            ed->Message(C_fstr("Draw links %s", draw_links ? "on" : "off"));
         }
         break;

         /*
      case E_SELECTION_CONFIG:
         {
            config_tab->Edit(GetTemplate(), ed->GetIGraph()->GetHWND(), cbTabCfg,
               (dword)this, 
               TABEDIT_IMPORT | TABEDIT_EXPORT | TABEDIT_INFO,
               NULL, NULL);
         }
         break;

      case E_SELECTION_ADD_NOTIFY:
         {
            const pair<PC_editor_item, dword> &np = *(pair<PC_editor_item, dword>*)context;
            for(int i=notify_list.size(); i--; )
               if(notify_list[i].first==np.first) break;
            if(i==-1){
               notify_list.push_back(np);
               np.first->AddRef();
               return true;
            }
         }
         break;

      case E_SELECTION_DEL_NOTIFY:
         {
            PC_editor_item ei = (PC_editor_item)context;
            for(int i=notify_list.size(); i--; )
               if(notify_list[i].first==ei) break;
            if(i!=-1){
               ei->Release();
               notify_list[i] = notify_list.back();
               notify_list.pop_back();
            }
         }
         break;
         */

      case E_SELECTION_TOGGLE_AXIS:
         view_axis_on = !view_axis_on;
         ed->Message(C_fstr("Draw axis %s", view_axis_on ? "enabled" : "disabled"));
         ed->CheckMenu(this, id, view_axis_on);
         break;

      //case E_SELECTION_GET_SB_INDEX:
         //return sb_index;
      }
      return 0;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){
                              //process flashing selection
      for(int i=flash_list.size(); i--; ){
         if(!flash_list[i].persistent){
            flash_list[i].count_down -= time;
            if(flash_list[i].count_down <= 0){
               flash_list[i] = flash_list.back();
               flash_list.pop_back();
            }
         }
      }
      if(sel_win_reset){
         if(hwnd_sel)
            AddObjsToList(hwnd_sel, NULL, sel_list);
         sel_win_reset = RESET_NO;
      }
   }

//----------------------------

   virtual void Render(){

      int i;

      PI3D_scene scene = ed->GetScene();
      PI3D_driver drv = ed->GetDriver();

      bool is_wire = drv->GetState(RS_WIREFRAME);
      if(is_wire) drv->SetState(RS_WIREFRAME, false);

      if(sel_list.size() || flash_list.size()){

         I3D_bbox temp_bb;
                              //line indicies
         static word bbox_list[] = {
            0, 1, 2, 3, 4, 5, 6, 7,
            0, 2, 1, 3, 4, 6, 5, 7,
            0, 4, 1, 5, 2, 6, 3, 7,
         }, pivot_list[] = {
            0, 1, 2, 3, 4, 5
         };

         if(sel_list.size()){

            bool loc_system = false;
            bool uniform_scale = true;
            byte rot_axis = 0;
            if(!e_medit) e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
            if(e_medit){
               loc_system = e_medit->IsLocSystem();
               uniform_scale = e_medit->IsUniformScale();
               rot_axis = (byte)e_medit->GetRotationAxis();
            }
      
            S_matrix mi = I3DGetIdentityMatrix();
            //static const S_vector color(1, 1, 1);
            //static const S_vector rot_color(1, 1, 0);
      
            bool single_sel = (sel_list.size()==1);

            S_vector pivot_all;
            pivot_all.Zero();

            for(i=sel_list.size(); i--; ){
               PI3D_frame frm = sel_list[i];

               const S_matrix &m = frm->GetMatrix();
               const I3D_bbox *lvp = &temp_bb;

               bool draw_pivot = false;

               switch(frm->GetType()){
               case FRAME_JOINT:
               case FRAME_LIGHT: case FRAME_SOUND: case FRAME_VOLUME:
               case FRAME_CAMERA: case FRAME_OCCLUDER:
               case FRAME_SECTOR: case FRAME_DUMMY:
                  frm->DebugDraw(scene);
                  lvp = NULL;
                  draw_pivot = single_sel; 
                  scene->SetRenderMatrix(m);
                  break;

                  /*
               case FRAME_DUMMY:
                  lvp = I3DCAST_DUMMY(frm)->GetBBox();
                  break;
                  */

               case FRAME_VISUAL:
                  {
                     PI3D_visual vis = I3DCAST_VISUAL(frm);
                     /*
                     switch(vis->GetVisualType()){
                     case I3D_VISUAL_NURBS:
                     case I3D_VISUAL_SINGLEMESH:
                     */
                        vis->DebugDraw(scene);
                        //break;
                     //}
                     lvp = &vis->GetBoundVolume().bbox;
                  }
                  break;
               case FRAME_MODEL:
                  {
                     PI3D_model mod = I3DCAST_MODEL(frm);
                     lvp = &mod->GetHRBoundVolume().bbox;
                     if(!lvp->IsValid()){
                        temp_bb.min=S_vector(-.25f, -.25f, -.25f);
                        temp_bb.max=S_vector(.25f, .25f, .25f);
                        lvp = &temp_bb;
                     }
                  }
                  break;
               default:
                  temp_bb.Invalidate();
                  {
                     I3D_bound_volume bvol;
                     frm->ComputeHRBoundVolume(&bvol);
                     temp_bb = bvol.bbox;
                  }
                  if(!temp_bb.IsValid()){
                     temp_bb.min=S_vector(-.1f, -.1f, -.1f);
                     temp_bb.max=S_vector(.1f, .1f, .1f);
                  }
                  break;
               }
               if(lvp){
                                    //check if this frame is in flash list
                  {
                     for(int j=flash_list.size(); j--; )
                     if(frm==flash_list[j].frm){
                        if(flash_list[j].persistent && (ed->GetIGraph()->ReadTimer()%500) >= 250)
                           break;
                     }
                     if(j!=-1) continue;
                  }

                  scene->SetRenderMatrix(m);
                  S_vector bbox[8];
                  for(int ii=0; ii<8; ii++)
                     bbox[ii] = S_vector((*lvp)[ii&1].x, (*lvp)[(ii&2)/2].y, (*lvp)[(ii&4)/4].z);

                  //byte alpha = 255 - 255.0f * config_tab->ItemF(TAB_F_CFG_ALPHA);
                  //byte alpha = 0;
                  scene->DrawLines(bbox, 8, bbox_list, sizeof(bbox_list)/sizeof(word));
                  if(single_sel){
                     draw_pivot = true;
                  }else{
                     pivot_all += frm->GetWorldPos();
                  }
               }
               if(draw_pivot){
                  const byte A = 0x60;
                  byte opacity[3] = {A, A, A};
                  if(loc_system){
                     if(rot_axis&1) opacity[0] = 0xff;
                     if(rot_axis&2) opacity[1] = 0xff;
                     if(rot_axis&4) opacity[2] = 0xff;
                  }
                  float let_scl;
                  S_vector scale;
                  if(lvp){
                     let_scl = lvp->max.Magnitude();
                     scale = S_vector(lvp->max * 1.2f);
                  }else{
                     PI3D_camera cam = scene->GetActiveCamera();
                     let_scl = (cam->GetWorldPos()-m(3)).Magnitude() * .1f;
                     scale = S_vector(let_scl, let_scl, let_scl);
                     let_scl *= 2.0f;
                  }
                  for(int i=0; i<3; i++)
                     scale[i] = Max(scale[i], let_scl*.1f);
                  ed->DrawAxes(m, false, scale, let_scl * .15f, opacity);
               }

               if(single_sel && !loc_system){
                                    //draw axes
                  const S_matrix &m = frm->GetMatrix();
                  float len = 0.0f;
                  S_matrix rm;
                  if(!loc_system){
                     mi(3) = m(3);
                     rm = mi;
                     len = lvp ? (lvp->max-lvp->min).Magnitude()/(2.0f*1.732f) : 1.0f;
                  }else
                     rm = m;
                  scene->SetRenderMatrix(rm);

                  const dword color = 0xffffff00;
                  if(rot_axis&1){
                     if(loc_system) len = lvp ? (lvp->max.x-lvp->min.x)/2 : 1.0f;
                     S_vector v1(len*1.5f, 0, 0);
                     scene->DrawLine(S_vector(len*.5f, 0, 0), v1, color);
                  }
                  if(rot_axis&2){      //Y
                     if(loc_system) len = lvp ? (lvp->max.y-lvp->min.y)/2 : 1.0f;
                     S_vector v1(0, len*1.5f, 0);
                     scene->DrawLine(S_vector(0, len*.5f, 0), v1, color);
                  }
                  if(rot_axis&4){      //Z
                     if(loc_system) len = lvp ? (lvp->max.z-lvp->min.z)/2 : 1.0f;
                     S_vector v1(0, 0, len*1.5f);
                     scene->DrawLine(S_vector(0, 0, len*.5f), v1, color);
                  }
               }

               if(draw_links){
                                 //transform 8 corner points of bbox and draw connection
                                 // to center of parent bbox (if it's not primary sector
                  PI3D_frame frm_parent = frm->GetParent();
                  if(!(!frm_parent || (frm_parent->GetType()==FRAME_SECTOR && I3DCAST_SECTOR(frm_parent)->IsPrimary()))){
                     scene->SetRenderMatrix(I3DGetIdentityMatrix());
                                 //get center of parent frame
                     S_vector v_prnt;
                     switch(frm_parent->GetType()){
                     case FRAME_VISUAL:
                        {
                                 //center to bbox
                           const S_matrix &m = frm_parent->GetMatrix();
                           const I3D_bound_volume &bvol = I3DCAST_VISUAL(frm_parent)->GetBoundVolume();
                           I3D_bbox bb;
                           bb.min = bvol.bbox.min * m;
                           bb.max = bvol.bbox.max * m;
                           v_prnt = bb.min + (bb.max - bb.min) * .5f;
                        }
                        break;
                     default:
                        v_prnt = frm_parent->GetWorldPos();
                     }

                     //byte alpha = 0;
                     //static const S_vector color(.9f, .6f, .0f);
                     if(lvp){
                        S_vector v_t[9];
                        (*lvp).Expand(v_t);
                        for(int i=8; i--; ) v_t[i] = v_t[i] * m;
                        v_t[8] = v_prnt;
                        static const word indx[] = {
                           0, 8, 1, 8, 2, 8, 3, 8, 4, 8, 5, 8, 6, 8, 7, 8,
                        };
                        scene->DrawLines(v_t, 9, indx, sizeof(indx)/sizeof(word), 0xffe8a000);
                     }else{
                        scene->DrawLine(v_prnt, frm->GetWorldPos(), 0xffe8a000);
                     }
                  }
               }
            }

            if(!single_sel){
                                    //draw pivot
               pivot_all /= (float)sel_list.size();
               S_vector v[6];
               v[0] = pivot_all + S_vector(.3f, 0, 0);
               v[1] = pivot_all + S_vector(-.3f, 0, 0);
               v[2] = pivot_all + S_vector(0, .3f, 0);
               v[3] = pivot_all + S_vector(0, -.3f, 0);
               v[4] = pivot_all + S_vector(0, 0, .3f);
               v[5] = pivot_all + S_vector(0, 0, -.3f);
               scene->SetRenderMatrix(I3DGetIdentityMatrix());

               scene->DrawLines(v, 6, pivot_list, sizeof(pivot_list)/sizeof(word), 0xffff0000);
            }
         }

                              //render flashing items
         for(i=flash_list.size(); i--; ){
                                 //blink persistent
            if(flash_list[i].persistent && (ed->GetIGraph()->ReadTimer()%500) < 250) continue;
            PI3D_frame frm = flash_list[i].frm;

            const I3D_bbox *lvp;
            const S_matrix *m = &frm->GetMatrix();

            switch(frm->GetType()){
            case FRAME_VOLUME:
               {
                  lvp = &temp_bb;
                  PI3D_volume vol = I3DCAST_VOLUME(frm);
                  switch(vol->GetVolumeType()){
                  case I3DVOLUME_SPHERE:
                     {
                        temp_bb.min = S_vector(-1, -1, -1);
                        temp_bb.max = S_vector(1, 1, 1);
                     }
                     break;
                  case I3DVOLUME_RECTANGLE:
                  case I3DVOLUME_BOX:
                     {
                        const S_vector &s = vol->GetNUScale();
                        temp_bb.min = -s;
                        temp_bb.max = s;
                        if(vol->GetVolumeType()==I3DVOLUME_RECTANGLE){
                           temp_bb.min.z = -.01f;
                           temp_bb.max.z =  .01f;
                        }
                     }
                     break;
                  default:
                     temp_bb.Invalidate();
                  }
               }
               break;
               /*
            case FRAME_DUMMY:
               lvp = I3DCAST_DUMMY(frm)->GetBBox();
               break;
               */

            case FRAME_VISUAL:
               {
                  PI3D_visual vis = I3DCAST_VISUAL(frm);
                  lvp = &vis->GetBoundVolume().bbox;
               }
               break;
            case FRAME_MODEL:
               {
                  PI3D_model mod = I3DCAST_MODEL(frm);
                  lvp = &mod->GetHRBoundVolume().bbox;
               }
               break;
            default:
               temp_bb.Invalidate();
               {
                  I3D_bound_volume bvol;
                  frm->ComputeHRBoundVolume(&bvol);
                  temp_bb = bvol.bbox;
               }
               lvp = &temp_bb;
               break;
            }
            if(!lvp->IsValid()){
               temp_bb.min = S_vector(-.25f, -.25f, -.25f);
               temp_bb.max = S_vector(.25f, .25f, .25f);
               lvp = &temp_bb;
            }
            scene->SetRenderMatrix(*m);
            S_vector bbox[8];
            /*
            static const S_vector flash_color[] = {
               S_vector(0, 1, 0),
               S_vector(1, 0, 0),
            };
            */
            static const dword flash_color[] = { 0xff00ff00, 0xffff0000 };
            static const dword flash_color1[] = { 0x40004c00, 0x404c0000 };
            for(int ii=0; ii<8; ii++)
               bbox[ii] = S_vector((*lvp)[ii&1].x, (*lvp)[(ii&2)/2].y, (*lvp)[(ii&4)/4].z);
            scene->DrawLines(bbox, 8, bbox_list, sizeof(bbox_list)/sizeof(word), flash_color[flash_list[i].persistent]);

            static const word bbox_list1[] = {
               1, 3, 5,  5, 3, 7,  0, 4, 6,  0, 6, 2,
               3, 2, 6,  3, 6, 7,  0, 1, 4,  5, 4, 1,
               5, 7, 6,  5, 6, 4,  0, 2, 1,  1, 2, 3
            };

            scene->DrawTriangles(bbox, 8, I3DVC_XYZ, bbox_list1, sizeof(bbox_list1)/sizeof(word),
               flash_color1[flash_list[i].persistent]);
         }
      }

      if(view_axis_on){
         const I3D_rectangle &vp = scene->GetViewport();

         float dx = float(vp.r - vp.l);
         float dy = float(vp.b - vp.t);
         ed->GetDriver()->SetViewport(
            I3D_rectangle(FloatToInt(vp.l + dx * .0f), FloatToInt(vp.t + dy * .9f),
            FloatToInt(vp.l + dx * .1f),
            FloatToInt(vp.t + dy * 1.0f)));

         bool is_zb = drv->GetState(RS_USEZB);
         if(is_zb) drv->SetState(RS_USEZB, false);

         PI3D_camera cam = scene->GetActiveCamera();
         if(cam){
            float fov = cam->GetFOV();
            float scale = fov * .8f;
            S_matrix m = cam->GetMatrix();
            m(3) += m(2) * 3.0f;
            ed->DrawAxes(m, true, S_vector(scale, scale, scale), scale*.5f);
         }

         if(is_zb) drv->SetState(RS_USEZB, true);
         ed->GetDriver()->SetViewport(vp);
      }
      if(is_wire) drv->SetState(RS_WIREFRAME, true);
   }

//----------------------------

#define SAVE_VERSION 4

   virtual bool LoadState(C_chunk &ck){

      byte version = 0;
      ck.Read((char*)&version, sizeof(version));
      if(version!=SAVE_VERSION)
         return false;

      //config_tab->Open((dword)ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
      int i = 0;
      ck.Read((char*)&i, sizeof(int));
      ck.Read((char*)objsel_list_width, sizeof(int)*i);
      ck.Read(&draw_links, sizeof(byte));
      ck.Read(&view_axis_on, sizeof(bool));
      ck.Read(&sel_dlg_pos, sizeof(sel_dlg_pos));

      ed->CheckMenu (this, E_SELECTION_TOGGLE_LINK, draw_links);
      ed->CheckMenu(this, E_SELECTION_TOGGLE_AXIS, view_axis_on);
      return true;
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      byte version = SAVE_VERSION;
      ck.Write((char*)&version, sizeof(version));
      //config_tab->Save((dword)ck.GetHandle(), TABOPEN_FILEHANDLE);
                              //save current widths
      int i = SELECTION_NUM_TABS;
      ck.Write((char*)&i, sizeof(int));
      ck.Write((char*)objsel_list_width, sizeof(int)*SELECTION_NUM_TABS);
      ck.Write(&draw_links, sizeof(byte));
      ck.Write(&view_axis_on, sizeof(bool));
      ck.Write(&sel_dlg_pos, sizeof(sel_dlg_pos));
      return true;
   }
};

//----------------------------

void CreateSelection(PC_editor ed){
   PC_editor_item ei = new C_edit_Selection_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
