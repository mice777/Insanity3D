#include "all.h"
#include "common.h"
#include "iexcpt.h"



//----------------------------

class C_edit_View: public C_editor_item{
   virtual const char *GetName() const{ return "View"; }

   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Modify> e_modify;

   enum{
      UNDO_SET_FLAG,
      UNDO_SET_COL_MAT,
   };

   enum{
      E_HR_BOX, E_BOUND_BOX, E_CLEAR, E_WIRE,
      E_FILTER, E_MIPMAP, E_DITHER, E_FOG,
      E_LIGHTMAP, E_SECTORS, E_PORTALS, E_VOLUMES,
      E_CAMERAS, E_VISUALS, E_LIGHTS, E_SOUNDS,
      E_LM_TXT, E_DUMMYS, E_OCCLUDERS, E_TEXTURES,
      E_DRAWJOINTS, E_USE_OCLUSION, E_ENVMAPPING, E_EMBMPMAPPING,
      E_DRAWMIRRORS, E_USESHADOWS, E_DEBUGDRAWSHADOWS, E_DEBUGDRAWSHDREC,
      E_DEBUGDRAWBSP, E_DEBUGDRAWHRDYNAMIC, E_DEBUGDRAWSTATIC, E_DETAILMAPPING,
      E_DRAWCOLS, E_DRAW_OVERDRAW,
      E_LAST
   };
   byte state[E_LAST];
   static I3D_RENDERSTATE i3d_map[];

   int lod_quality;
   float lod_scale;

                              //LOD editor
   int curr_lod_range;

//----------------------------

   const byte *GetDefaultState(){
      static const byte default_state[] = {
                              //hr_bbox, bound_box, clear_bgnd, wire
         false, false, true, false,
                              //filter, mipmap, dither, fog
         true, true, true, true,  
                              //lmapping, sectors, portals, volumes
         true, false, false, false,
                              //cameras, visuals, lights, sounds
         false, true, false, false,  
                              //lm textures, dummys, occluders, textures
         false, false, false, true,
                              //joints, use_occlusion, env_mapping, embmmapping
         false, true, true, true,
                              //mirrors, use shd, debug shd, debug shd rec
         true, true, false, false,
                              //bsp tree, dynamic tree, static frames, detailmapping
         false, false, false, true,
                              //draw cols, overdraw
         false, false,
      };
      return default_state;
   }

//----------------------------

   void ApplyState(const byte *state){

      for(int i=0; i<E_LAST; i++){
         ed->CheckMenu(this, 1000+i, state[i]);
         ed->GetDriver()->SetState(i3d_map[i], state[i]);
      }
      ed->GetDriver()->SetState(RS_LOD_INDEX, lod_quality);
      //ed->GetDriver()->SetState(RS_DEBUGMATVISUALIZE, curr_mat_draw);
      ed->GetDriver()->SetState(RS_DEBUG_DRAW_MATS, mat_draw_enabled);
   }

//----------------------------

                              //material table
   typedef map<dword, C_str> t_mat_tab;
   t_mat_tab mat_table;
   dword curr_assign_mat;       //material ID from table, -1 uninitialized/invalid
   bool mat_draw_enabled;     //enable/disable drawing

   C_smart_ptr<C_toolbar> toolbar;

   const char *GetDrawMatName() const{

      if(curr_assign_mat==-1)
         return NULL;
      t_mat_tab::const_iterator it;
      for(it=mat_table.begin(); it!=mat_table.end(); it++){
         if((*it).first==curr_assign_mat)
            break;
      }
      if(it==mat_table.end())
         return NULL;
      return (*it).second;
   }

//----------------------------

   static BOOL CALLBACK dlgProcLod(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){

	   switch(message){
      case WM_INITDIALOG:
         {
            SetWindowLong(hwnd, GWL_USERDATA, lParam);
            C_edit_View *ei = (C_edit_View*)lParam;
                              //setup slider
            HWND hwnd_sld = GetDlgItem(hwnd, IDC_SLIDER_LOD);
            SendMessage(hwnd_sld, TBM_SETRANGEMIN, false, -1);
            SendMessage(hwnd_sld, TBM_SETRANGEMAX, false, ei->curr_lod_range);
            SendMessage(hwnd_sld, TBM_SETPAGESIZE, 0, 10);
            SendMessage(hwnd_sld, TBM_SETPOS, false, ei->lod_quality);
                              //setup numbers
            SetDlgItemInt(hwnd, IDC_EDIT_LOD_RANGE, ei->curr_lod_range, true);
            SetDlgItemInt(hwnd, IDC_EDIT_LOD, ei->lod_quality, true);
                              //setup LOD scale slider
            hwnd_sld = GetDlgItem(hwnd, IDC_SLIDER_LOD_SCALE);
            SendMessage(hwnd_sld, TBM_SETRANGEMIN, false, 0);
            SendMessage(hwnd_sld, TBM_SETRANGEMAX, false, 100);
            SendMessage(hwnd_sld, TBM_SETPOS, true, FloatToInt(ei->lod_scale * 10.0f));

            SetDlgItemText(hwnd, IDC_LOD_SCALE, C_fstr("%.2f", ei->lod_scale));
         }
         return 1;

      case WM_HSCROLL:
         {
            HWND hwnd_sld = (HWND)lParam;
            C_edit_View *ei = (C_edit_View*)GetWindowLong(hwnd, GWL_USERDATA);
            int i = SendMessage(hwnd_sld, TBM_GETPOS, 0, 0);
            if(hwnd_sld==GetDlgItem(hwnd, IDC_SLIDER_LOD)){
               ei->ed->GetDriver()->SetState(RS_LOD_INDEX, i);
               ei->lod_quality = i;
               ei->ed->Message(C_fstr("LOD quality: %i", i));
               SetDlgItemInt(hwnd, IDC_EDIT_LOD, i, true);
            }else{
               float f = (float)i * .1f;
               ei->lod_scale = f;
               ei->ed->GetDriver()->SetState(RS_LOD_SCALE, I3DFloatAsInt(f));
               ei->ed->Message(C_fstr("LOD scale: %.2f", f));
               SetDlgItemText(hwnd, IDC_LOD_SCALE, C_fstr("%.2f", f));
            }
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCLOSE:
            {
               C_edit_View *ei = (C_edit_View*)GetWindowLong(hwnd, GWL_USERDATA);
               ei->Action(VIEW_LOD_TOOLBAR, 0);
            }
            break;
         case IDC_EDIT_LOD_RANGE:
         case IDC_EDIT_LOD:
            switch(HIWORD(wParam)){
            case EN_CHANGE:
               {
                  C_edit_View *ei = (C_edit_View*)GetWindowLong(hwnd, GWL_USERDATA);
                  int i = GetDlgItemInt(hwnd, LOWORD(wParam), NULL, true);
                  switch(LOWORD(wParam)){
                  case IDC_EDIT_LOD_RANGE:
                     ei->curr_lod_range = i;
                     SendDlgItemMessage(hwnd, IDC_SLIDER_LOD, TBM_SETRANGEMAX, true, i);
                     break;
                  case IDC_EDIT_LOD:
                     {
                        ei->ed->GetDriver()->SetState(RS_LOD_INDEX, i);
                        ei->lod_quality = i;
                        ei->ed->Message(C_fstr("LOD quality: %i", i));
                        SendDlgItemMessage(hwnd, IDC_SLIDER_LOD, TBM_SETPOS, true, i);
                     }
                     break;
                  }
               }
               break;
            }
            break;
         case IDC_LOD_SET_DEFAULT:
            {
               C_edit_View *ei = (C_edit_View*)GetWindowLong(hwnd, GWL_USERDATA);
               ei->lod_scale = 1.0f;
               SetDlgItemText(hwnd, IDC_LOD_SCALE, "1.00");
               SendDlgItemMessage(hwnd, IDC_SLIDER_LOD_SCALE, TBM_SETPOS, true, FloatToInt(ei->lod_scale * 10.0f));
               ei->ed->GetDriver()->SetState(RS_LOD_SCALE, I3DFloatAsInt(1.0f));
               ei->ed->Message("LOD scale reset");
            }
            break;
         }
      }
      return 0;
   }

//----------------------------
   HWND hwnd_lod;

   void ToggleLODDialog(){

      PC_editor_item_Properties e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      if(hwnd_lod){
         e_props->RemoveSheet(hwnd_lod);
         DestroyWindow(hwnd_lod);
         hwnd_lod = NULL;
      }else{
         hwnd_lod = CreateDialogParam(GetHInstance(), "IDD_LOD_CONTROL", NULL, dlgProcLod, (LPARAM)this);
         if(hwnd_lod){
            e_props->AddSheet(hwnd_lod, true);
            //SetDialogByParams();
         }
      }                                                       
   }

//----------------------------

   enum{
      VIEW_HR_BOXES = 1000,
      VIEW_BOUND_BOXES,
      VIEW_BGND_CLEAR,
      VIEW_WIRE_FRAME,

      VIEW_USE_FILTERING,
      VIEW_USE_MIPMAPPING,
      VIEW_USE_DITHERING,
      VIEW_USE_FOG,

      VIEW_USE_LMAPS,
      VIEW_SECTORS,
      VIEW_PORTALS,
      VIEW_VOLUMES,

      VIEW_CAMERAS,
      VIEW_VISUALS,
      VIEW_LIGHTS,
      VIEW_SOUNDS,

      VIEW_LM_TEXTURES,
      VIEW_DUMMIES,
      VIEW_OCCLUDERS,
      VIEW_TEXTURES,

      VIEW_JOINTS,
      VIEW_USE_OCCLUSION,
      VIEW_ENV_MAPPING,
      VIEW_EMBM_MAPPING,

      VIEW_MIRRORS,
      VIEW_SHADOWS,
      VIEW_SHD_CASTERS,
      VIEW_SHD_RECEIVERS,

      VIEW_BSP_TREE,
      VIEW_DYN_COL_HR,
      VIEW_STATIC_COLS,
      VIEW_DETAIL_MAPPING,

      VIEW_COL_TESTS,
      VIEW_OVERDRAW,
      VIEW_CRASH,

      VIEW_LOD_BETTER = 1100,
      VIEW_LOD_WORSE,
      VIEW_LOD_DEFAULT,
      VIEW_LOD_SCL_LESS,
      VIEW_LOD_SCL_MORE,
      VIEW_LOD_SCL_DEF,
      VIEW_LOD_TOOLBAR,
      VIEW_VIZ_MATS,
      VIEW_VIZ_MATS_OFF,
      VIEW_MATS_ASSIGN,
      VIEW_MAT_ASSIGN_PICK,
      VIEW_MAT_TOGGLE_REC_SHD,
   };

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      if(!ed->CanModify()) return;

      switch(id){
      case UNDO_SET_FLAG:
         {
            bool on = ck.ReadBool();
                              //save undo
            C_chunk &ck = e_undo->Begin(this, id, frm);
            //ck.Write(frm->GetName());
            ck.Write(!on);
            e_undo->End();

            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_FRM_FLAGS);

            frm->SetFlags(on ? I3D_FRMF_SHADOW_RECEIVE : 0, I3D_FRMF_SHADOW_RECEIVE);
         }
         break;

      case UNDO_SET_COL_MAT:
         {
            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_COL_MAT);

            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            switch(frm->GetType()){
            case FRAME_VOLUME:
               ck_undo.Write((dword)I3DCAST_VOLUME(frm)->GetCollisionMaterial());
               I3DCAST_VOLUME(frm)->SetCollisionMaterial(ck.ReadDword());
               break;
            case FRAME_VISUAL:
               ck_undo.Write((dword)I3DCAST_VISUAL(frm)->GetCollisionMaterial());
               I3DCAST_VISUAL(frm)->SetCollisionMaterial(ck.ReadDword());
               break;
            }
            e_undo->End();
         }
         break;

      default: assert(0);
      }
      //e_slct->FlashFrame(frm);
      ed->SetModified();
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==0){
                           //save screen thumbnail
         const int SIZE_X = 256;
         const int SIZE_Y = 192;
         PI3D_scene scene = ed->GetScene();
         PIGraph igraph = ed->GetIGraph();
         scene->Render(0);
         PIImage bb_img = igraph->CreateBackbufferCopy(SIZE_X, SIZE_Y, true);
         if(!bb_img){
            ed->GetDriver()->EvictResources();
            bb_img = igraph->CreateBackbufferCopy(SIZE_X, SIZE_Y, true);
         }
         if(bb_img){
            ck <<= CT_THUMBNAIL;
            bb_img->SaveShot(ck.GetHandle(), "png");
            --ck;
            bb_img->Release();
         }
      }
   }

//----------------------------

public:
   C_edit_View():
      lod_quality(-1),
      lod_scale(1.0f),
      curr_assign_mat((dword)-1),
      mat_draw_enabled(false),
      hwnd_lod(NULL),
      curr_lod_range(100)
   {}

   virtual bool Init(){
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      if(!e_undo || !e_modify)
         return false;

#define DS "&View\\&Debug\\"
      ed->AddShortcut(this, VIEW_HR_BOXES, DS"&Hierarchy boxes\tShift+Ctrl+H", K_H, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_BOUND_BOXES, DS"&Bounding boxes\tShift+Ctrl+B", K_B, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_WIRE_FRAME, DS"&Wire-frame\tW", K_W, 0);
      ed->AddShortcut(this, VIEW_OCCLUDERS, DS"&Occluders\tU", K_U, 0);
      ed->AddShortcut(this, VIEW_DUMMIES, DS"&Dummies\tD", K_D, 0);
      ed->AddShortcut(this, VIEW_SECTORS, DS"&Sectors\tR", K_R, 0);
      ed->AddShortcut(this, VIEW_PORTALS, DS"&Portals\tP", K_P, 0);
      ed->AddShortcut(this, VIEW_VOLUMES, DS"&Volumes\tM", K_M, 0);
      ed->AddShortcut(this, VIEW_CAMERAS, DS"&Cameras\tC", K_C, 0);
      ed->AddShortcut(this, VIEW_LIGHTS, DS"&Lights\tShift+Ctrl+L", K_L, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_SOUNDS, DS"&Sounds\tShift+Ctrl+S", K_S, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_LM_TEXTURES, DS"&LM textures\tShift+Ctrl+Alt+L", K_L, SKEY_CTRL|SKEY_ALT|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_JOINTS, DS"&Joints\tJ", K_J, 0);
      ed->AddShortcut(this, VIEW_SHD_CASTERS, DS"Dynamic shadow casters\tCtrl+Shift+Alt+D", K_D, SKEY_CTRL|SKEY_SHIFT|SKEY_ALT);
      ed->AddShortcut(this, VIEW_SHD_RECEIVERS, DS"Dynamic shadow receivers\tCtrl+Shift+Alt+R", K_R, SKEY_CTRL|SKEY_SHIFT|SKEY_ALT);
      ed->AddShortcut(this, VIEW_BSP_TREE, DS"&Bsp tree\tCtrl+Shift+V", K_V, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_DYN_COL_HR, DS"D&ynamic collision hiearchy\tCtrl+Shift+Y", K_Y, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_STATIC_COLS, DS"St&atic collision frames\tCtrl+Shift+A", K_A, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_OVERDRAW, DS"Overd&raw\tCtrl+Alt+Shift+W", K_W, SKEY_ALT|SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_COL_TESTS, DS"Collision testing\tCtrl+Alt+Shift+C", K_C, SKEY_CTRL|SKEY_ALT|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_LOD_BETTER, DS"%i LOD\\Better level\tCtrl+<", K_COMMA, SKEY_CTRL);
      ed->AddShortcut(this, VIEW_LOD_WORSE, DS"LOD\\Worse level\tCtrl+>", K_DOT, SKEY_CTRL);
      ed->AddShortcut(this, VIEW_LOD_DEFAULT, DS"LOD\\Default level\tCtrl+Alt+Shift+<", K_COMMA, SKEY_ALT|SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_LOD_SCL_LESS, DS"LOD\\%i Lower scale\tCtrl+Shift+<", K_COMMA, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_LOD_SCL_MORE, DS"LOD\\Increase scale\tCtrl+Shift+>", K_DOT, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_LOD_SCL_DEF, DS"LOD\\Default scale\tCtrl+Alt+Shift+>", K_DOT, SKEY_ALT|SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_LOD_TOOLBAR, DS"LOD\\Toolbar\tCtrl+Shift+O", K_O, SKEY_CTRL|SKEY_SHIFT);

#define MS "&View\\&Debug\\Material\\"
      ed->AddShortcut(this, VIEW_VIZ_MATS, MS"Vizualize material\tCtrl+M", K_M, SKEY_CTRL);
      //ed->AddShortcut(this, VIEW_VIZ_MATS_OFF, MS"Disable\tCtrl+Alt+Shift+M", K_M, SKEY_ALT|SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_MATS_ASSIGN, MS"Assign mode\tCtrl+Alt+M", K_M, SKEY_ALT|SKEY_CTRL);

#define QS "&View\\&Quality\\"
      ed->AddShortcut(this, VIEW_BGND_CLEAR, QS"Clear background\tCtrl+F7", K_F7, SKEY_CTRL);
      ed->AddShortcut(this, VIEW_VISUALS, QS"Draw &visuals\tV", K_V, 0);
      ed->AddShortcut(this, VIEW_USE_FILTERING, QS"&Filtering\tCtrl+F5", K_F5, SKEY_CTRL);
      ed->AddShortcut(this, VIEW_USE_MIPMAPPING, QS"&MipMapping\tCtrl+F6", K_F6, SKEY_CTRL);
      ed->AddShortcut(this, VIEW_USE_DITHERING, QS"&Dithering\tCtrl+F8", K_F8, SKEY_CTRL);
      ed->AddShortcut(this, VIEW_USE_FOG, QS"F&og\tCtrl+F9", K_F9, SKEY_CTRL);
      ed->AddShortcut(this, VIEW_USE_LMAPS, QS"&Light-maps\tShift+Ctrl+Alt+T", K_T, SKEY_CTRL|SKEY_ALT|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_TEXTURES, QS"&Textures\tCtrl+Shift+T", K_T, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_ENV_MAPPING, QS"E&nvironment mapping\tCtrl+Shift+N", K_N, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_EMBM_MAPPING, QS"&EMBM mapping\tCtrl+Shift+U", K_U, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_MIRRORS, QS"&Mirrors\tCtrl+Shift+M", K_M, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_SHADOWS, QS"&Shadows\tCtrl+Shift+D", K_D, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_USE_OCCLUSION, QS"&Use occlusion\tShift+Ctrl+Alt+O", K_O, SKEY_CTRL|SKEY_ALT|SKEY_SHIFT);
      ed->AddShortcut(this, VIEW_DETAIL_MAPPING, QS"D&etail mapping\tShift+Ctrl+E", K_E, SKEY_CTRL|SKEY_SHIFT);

      ed->AddShortcut(this, VIEW_CRASH, "%30 &Debug\\%61Simulate cr&ash", K_NOKEY, 0);
                              //init state
      memcpy(state, GetDefaultState(), sizeof(byte)*E_LAST);
      for(int i=0; i<E_LAST; i++) ed->CheckMenu(this, 1000+i, state[i]);
      ApplyState(state);

      {
                              //init toolbar
         toolbar = ed->GetToolbar("Debug");

         S_toolbar_button tbs[] = {
            {VIEW_WIRE_FRAME, 0, "Wire frame"},
            {VIEW_OCCLUDERS, 1, "Occluders"},
            {VIEW_DUMMIES, 2, "Dummies"},
            {VIEW_SECTORS, 3, "Sectors"},
            {VIEW_PORTALS, 4, "Portal"},
            {VIEW_CAMERAS, 6, "Cameras"},
            {VIEW_LIGHTS, 7, "Lights"},
            {VIEW_SOUNDS, 8, "Sounds"},
            {VIEW_JOINTS, 9, "Joints"},
            {VIEW_VOLUMES, 5, "Volumes"},
            {VIEW_DYN_COL_HR, 10, "Dynamic collisions"},
            {VIEW_STATIC_COLS, 11, "Static collisions"},
            {VIEW_VISUALS, 12, "Visuals"},
            {0, -1},
         };
         toolbar->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_DEBUG", GetHInstance(), 10);
      }
      return true;
   }

//----------------------------

   virtual void Close(){ }
   //virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}

   virtual dword Action(int id, void *context){

      switch(id){
      case VIEW_HR_BOXES: case VIEW_BOUND_BOXES: case VIEW_BGND_CLEAR: case VIEW_WIRE_FRAME:
      case VIEW_USE_FILTERING: case VIEW_USE_MIPMAPPING: case VIEW_USE_DITHERING: case VIEW_USE_FOG:
      case VIEW_USE_LMAPS: case VIEW_SECTORS: case VIEW_PORTALS: case VIEW_VOLUMES:
      case VIEW_CAMERAS: case VIEW_VISUALS: case VIEW_LIGHTS: case VIEW_SOUNDS:
      case VIEW_LM_TEXTURES: case VIEW_DUMMIES: case VIEW_OCCLUDERS: case VIEW_TEXTURES:
      case VIEW_JOINTS: case VIEW_USE_OCCLUSION: case VIEW_ENV_MAPPING: case VIEW_EMBM_MAPPING:
      case VIEW_MIRRORS: case VIEW_SHADOWS: case VIEW_SHD_CASTERS: case VIEW_SHD_RECEIVERS:
      case VIEW_BSP_TREE: case VIEW_DYN_COL_HR: case VIEW_STATIC_COLS: case VIEW_DETAIL_MAPPING:
      case VIEW_COL_TESTS: case VIEW_OVERDRAW:
         {
            int i = id - 1000;
            state[i] = !state[i];
            dword on_flags = state[i];
            switch(i3d_map[i]){
            case RS_DETAILMAPPING:
            case RS_DRAWTEXTURES:
            case RS_ENVMAPPING:
            case RS_USE_EMBM:
               on_flags |= 0x80000000;
               break;
            }
            ed->GetDriver()->SetState(i3d_map[i], on_flags);
            ed->CheckMenu(this, id, state[i]);
            static const char *state_text[E_LAST] = {
               "Hierarchy bounding boxes", "Visual bounding boxes", "Background clearing", "Wire-frame mode",
               "Texture filtering", "Mipmapping", "Dithering", "Fog",
               "Light-mapping", "Draw sectors", "Draw portals", "Draw volumes",
               "Draw cameras", "Draw visuals", "Draw lights", "Draw sounds",
               "Draw light-map cache", "Draw dummys", "Draw occluders", "Textures",
               "Draw joints", "Use occlusion", "Environment mapping", "Environment-bump mapping",
               "Draw mirrors", "Use shadows", "Debug shadows", "Draw shadow receivers",
               "Draw bsp tree", "Draw dynamic tree", "Draw static frames", "Detail mapping",
               "Draw collision tests", "Show overdraw",
            };
            ed->Message(C_fstr("%s %s", state_text[i], state[i] ? "on" : "off"));
            toolbar->SetButtonPressed(this, id, state[i]);

            switch(id){
            case VIEW_SHD_RECEIVERS:
                              //toggle "receive shadow" flag mode
               {
                  PC_editor_item_MouseEdit m_edit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
                  if(state[i]){
                     m_edit->SetUserPick(this, VIEW_MAT_TOGGLE_REC_SHD, LoadCursor(GetHInstance(), "IDC_CURSOR_TOGGLE_SHD"));
                  }else
                  if(m_edit->GetUserPick()==this){
                     m_edit->SetUserPick(NULL, 0);
                  }
               }
               break;
            }
         }
         break;

      case VIEW_VIZ_MATS:
         {
            /*
                              //visualize material (choose from list)
            C_vector<char> vc;
            int curr_sel = -1;
            int i = 0;
            for(t_mat_tab::const_iterator it=mat_table.begin(); it!=mat_table.end(); it++, i++){
               const C_str &str = (*it).second;
               vc.insert(vc.end(), (const char*)str, (const char*)str+str.Size()+1);
               if((*it).first==curr_mat_draw)
                  curr_sel = i;
            }
            vc.push_back(0);
            int indx = ChooseItemFromList(ed->GetIGraph(), NULL, "Choose material to draw",
               &vc.front(), curr_sel);
            if(indx==-1){
               //Action(VIEW_VIZ_MATS_OFF, 0);
               break;
            }
            for(it=mat_table.begin(); indx--; it++);
            assert(it!=mat_table.end());
            curr_mat_draw = (*it).first;
            ed->Message(C_fstr("Drawing material '%s'", (const char*)(*it).second));
            mat_draw_enabled = true;
            */
            mat_draw_enabled = !mat_draw_enabled;
            ed->CheckMenu(this, VIEW_VIZ_MATS, mat_draw_enabled); 
            ed->Message(C_xstr("Material visualization %") % (mat_draw_enabled ? "on" : "off"));
            //ed->GetDriver()->SetState(RS_DEBUGMATVISUALIZE, curr_mat_draw);
            ed->GetDriver()->SetState(RS_DEBUG_DRAW_MATS, mat_draw_enabled);
         }
         break;

         /*
      case VIEW_VIZ_MATS_OFF:
         {
            //ed->GetDriver()->SetState(RS_DEBUGMATVISUALIZE, curr_mat_draw = -1);
            //ed->Message("Material visualization disebled");
            mat_draw_enabled = !mat_draw_enabled;
            ed->GetDriver()->SetState(RS_DEBUGMATVISUALIZE, mat_draw_enabled ? curr_mat_draw : -1);
            ed->Message(C_fstr("Material visualization %s", mat_draw_enabled ? "enebled" : "disabled"));
         }
         break;
         */

      case VIEW_MATS_ASSIGN:
         {
                              //visualize material (choose from list)
            C_vector<char> vc;
            int curr_sel = -1;
            int i = 0;
            t_mat_tab::const_iterator it;
            for(it=mat_table.begin(); it!=mat_table.end(); it++, i++){
               const C_str &str = (*it).second;
               vc.insert(vc.end(), (const char*)str, (const char*)str+str.Size()+1);
               if((*it).first==curr_assign_mat)
                  curr_sel = i;
            }
            vc.push_back(0);
            int indx = ChooseItemFromList(ed->GetIGraph(), NULL, "Choose material to draw",
               &vc.front(), curr_sel);
            if(indx==-1){
               //Action(VIEW_VIZ_MATS_OFF, 0);
               break;
            }
            for(it=mat_table.begin(); indx--; it++);
            assert(it!=mat_table.end());
            curr_assign_mat = (*it).first;
            /*
            if(curr_mat_draw==-1){
                              //select material to vizualize
               Action(VIEW_VIZ_MATS, 0);
               if(curr_mat_draw==-1)
                  break;
            }
            const char *curr_mat_name = GetDrawMatName();
            if(!curr_mat_name){
                              //invalid material selected, disable mode
               Action(VIEW_VIZ_MATS_OFF, 0);
               break;
            }
            */
                              //enanble drawing if disabled
            if(!mat_draw_enabled){
               mat_draw_enabled = true;
               ed->CheckMenu(this, VIEW_VIZ_MATS, mat_draw_enabled); 
               //ed->GetDriver()->SetState(RS_DEBUGMATVISUALIZE, mat_draw_enabled ? curr_mat_draw : -1);
               ed->GetDriver()->SetState(RS_DEBUG_DRAW_MATS, true);
            }
                              //switch to user pick mode
            PC_editor_item_MouseEdit m_edit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
            m_edit->SetUserPick(this, VIEW_MAT_ASSIGN_PICK, LoadCursor(GetHInstance(), "IDC_SEED_PUTCURSOR"));

            //ed->Message(C_fstr("Assigning material '%s'", curr_mat_name));
         }
         break;

      case VIEW_MAT_ASSIGN_PICK:
         {
            if(!mat_draw_enabled)
               break;
            const char *curr_mat_name = GetDrawMatName();
            if(!curr_mat_name)
               break;
            if(!ed->CanModify())
               break;
            assert(curr_assign_mat != -1);
            const S_MouseEdit_picked *mp = (S_MouseEdit_picked*)context;
                              //do own (normal-collision) picking
            I3D_collision_data cd;
            cd.from = mp->pick_from;
            cd.dir = mp->pick_dir * 1000.0f;
            cd.flags = I3DCOL_LINE | I3DCOL_COLORKEY;// | I3DCOL_RAY;
            ed->GetScene()->TestCollision(cd);
            PI3D_frame frm = cd.GetHitFrm();
            float dist = cd.GetHitDistance();
            {
               cd.flags = I3DCOL_VOLUMES | I3DCOL_RAY;
               if(ed->GetScene()->TestCollision(cd)){
                  PI3D_frame frm_vol = cd.GetHitFrm();
                  if(!frm){
                     frm = frm_vol;
                  }else
                  if(dist>cd.GetHitDistance())
                     frm = frm_vol;
               }
            }
            if(frm){
               int save_mat = -1;
               switch(frm->GetType()){
               case FRAME_VISUAL:
                  {
                     PI3D_visual vis = I3DCAST_VISUAL(frm);
                              //check if volume marked 'static'
                     if(!(vis->GetFlags()&I3D_FRMF_STATIC_COLLISION)){
                        ed->Message(C_fstr("Visual '%s' has not static collision!", (const char*)frm->GetName()));
                        break;
                     }
                     if(vis->GetCollisionMaterial()!=curr_assign_mat){
                        e_modify->AddFrameFlags(frm, E_MODIFY_FLG_COL_MAT);

                        save_mat = vis->GetCollisionMaterial();
                        vis->SetCollisionMaterial(curr_assign_mat);
                        ed->Message(C_fstr("Visual '%s': material '%s' assigned", (const char*)frm->GetName(), curr_mat_name));
                        ed->SetModified();
                     }
                  }
                  break;
               case FRAME_VOLUME:
                  {
                     PI3D_volume vol = I3DCAST_VOLUME(frm);
                     if(vol->GetCollisionMaterial()!=curr_assign_mat){
                        e_modify->AddFrameFlags(frm, E_MODIFY_FLG_COL_MAT);

                        save_mat = vol->GetCollisionMaterial();
                        vol->SetCollisionMaterial(curr_assign_mat);
                        ed->Message(C_fstr("Volume '%s': material '%s' assigned", (const char*)frm->GetName(), curr_mat_name));
                        ed->SetModified();
                     }
                  }
                  break;
               }
               if(save_mat!=-1){
                  C_chunk &ck_undo = e_undo->Begin(this, UNDO_SET_COL_MAT, frm);
                  ck_undo.Write((dword)save_mat);
                  e_undo->End();
               }
            }
            return true;
         }
         break;

      case VIEW_MAT_TOGGLE_REC_SHD:
         {
            if(!ed->CanModify())
               break;

            const S_MouseEdit_picked *mp = (S_MouseEdit_picked*)context;
            PI3D_frame frm = mp->frm_picked;
            if(frm){
               switch(frm->GetType()){
               case FRAME_VISUAL:
                  {
                                 //toggle flag
                     dword flags = frm->GetFlags();

                     e_modify->AddFrameFlags(frm, E_MODIFY_FLG_FRM_FLAGS);
                                 //save undo
                     C_chunk &ck = e_undo->Begin(this, UNDO_SET_FLAG, frm);
                     ck.Write(bool(flags&I3D_FRMF_SHADOW_RECEIVE));
                     e_undo->End();

                     flags ^= I3D_FRMF_SHADOW_RECEIVE;
                     frm->SetFlags(flags, I3D_FRMF_SHADOW_RECEIVE);
                     ed->Message(C_fstr("Frame '%s' - rec. shd. flag %s", (const char*)frm->GetName(),
                        (flags&I3D_FRMF_SHADOW_RECEIVE) ? "set" : "cleared"));
                  }
                  break;
               }
               ed->SetModified();
            }
            return true;
         }
         break;

      case VIEW_LOD_TOOLBAR:
         {
                              //toggle LOD toolbar
            ToggleLODDialog();
            ed->Message(C_fstr("LOD dialog %s", hwnd_lod ? "on" : "off"));
         }
         break;

      case VIEW_LOD_BETTER:
      case VIEW_LOD_WORSE:
      case VIEW_LOD_DEFAULT:
         {
            switch(id){
            case VIEW_LOD_BETTER:
               lod_quality = Max(-1, lod_quality - 1);
               break;
            case VIEW_LOD_WORSE:
               ++lod_quality;
               break;
            case VIEW_LOD_DEFAULT:
               lod_quality = -1;
               break;
            default: assert(0);
            }
            ed->GetDriver()->SetState(RS_LOD_INDEX, lod_quality);
            ed->Message(C_fstr("LOD quality: %i", lod_quality));
            if(hwnd_lod){
               SetDlgItemInt(hwnd_lod, IDC_EDIT_LOD, lod_quality, true);
               SendDlgItemMessage(hwnd_lod, IDC_SLIDER_LOD, TBM_SETPOS, false, lod_quality);
            }
         }
         break;

      case VIEW_LOD_SCL_LESS:
      case VIEW_LOD_SCL_MORE:
      case VIEW_LOD_SCL_DEF:
         {
            //float curr_scale = I3DIntAsFloat(ed->GetDriver()->GetState(RS_LOD_SCALE));
            const float sd = .8f, su = 1.25f;
            switch(id){
            case VIEW_LOD_SCL_LESS:
               lod_scale = Max(sd*sd*sd*sd*sd*sd*sd*sd*sd*sd, lod_scale * sd);
               break;
            case VIEW_LOD_SCL_MORE:
               lod_scale = Min(su*su*su*su*su*su*su*su*su*su*su, lod_scale * su);
               break;
            case VIEW_LOD_SCL_DEF:
               lod_scale = 1.0f;
               break;
            default: assert(0);
            }
            ed->GetDriver()->SetState(RS_LOD_SCALE, I3DFloatAsInt(lod_scale));
            ed->Message(C_fstr("LOD scale: %.2f", lod_scale));
            if(hwnd_lod){
               SetDlgItemText(hwnd_lod, IDC_LOD_SCALE, C_fstr("%.2f", lod_scale));
               SendDlgItemMessage(hwnd_lod, IDC_SLIDER_LOD_SCALE, TBM_SETPOS, true, FloatToInt(lod_scale*10.0f));
            }
         }
         break;

      case VIEW_CRASH:
         //sqrt(-2);
         //assert(0);
         *(char*)NULL = 0;
         //UserException("a", "b");
         break;

      case EDIT_ACTION_FEED_MATERIALS:
         {
            pair<int, const char*> *pairs = (pair<int, const char*>*)context;

            mat_table.clear();

            for(int i=0; pairs[i].first!=-1; i++)
               mat_table[pairs[i].first] = pairs[i].second;

            //FeedMaterials();
         }
         break;
      }
      return 0;
   }

//----------------------------

   //virtual void Render(){}

//----------------------------

#define STATE_VERSION 0x00d1

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

                              //write version
      byte version = STATE_VERSION;
      ck.Write(&version, sizeof(byte));
                              //write state
      ck.Write(&state, sizeof(state));
      ck.Write(&lod_quality, sizeof(lod_quality));
      ck.Write(&lod_scale, sizeof(lod_scale));
      ck.Write(&curr_assign_mat, sizeof(curr_assign_mat));
      ck.Write(&curr_lod_range, sizeof(curr_lod_range));
      //ck.Write(&mat_draw_enabled, sizeof(mat_draw_enabled));

      bool lod_sheet_on = (hwnd_lod!=NULL);
      ck.Write(&lod_sheet_on, sizeof(bool));

      return true;
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){

                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version != STATE_VERSION)
         return false;
                              //read state
      ck.Read(&state, sizeof(state));
      ck.Read(&lod_quality, sizeof(lod_quality));
      ck.Read(&lod_scale, sizeof(lod_scale));
      ck.Read(&curr_assign_mat, sizeof(curr_assign_mat));
      ck.Read(&curr_lod_range, sizeof(curr_lod_range));
      //ck.Read(&mat_draw_enabled, sizeof(mat_draw_enabled));
      bool lod_sheet_on = false;
      ck.Read(&lod_sheet_on, sizeof(bool));
      if(lod_sheet_on != (hwnd_lod!=NULL))
         ToggleLODDialog();

      ApplyState(state);
      ed->GetDriver()->SetState(RS_LOD_SCALE, I3DFloatAsInt(lod_scale));

      if(state[VIEW_SHD_RECEIVERS-1000]){
         PC_editor_item_MouseEdit m_edit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
         m_edit->SetUserPick(this, VIEW_MAT_TOGGLE_REC_SHD, LoadCursor(GetHInstance(), "IDC_CURSOR_TOGGLE_SHD"));
      }
      {
                              //press buttons for all 'on' states
         for(dword i=E_LAST; i--; ){
            if(state[i])
               toolbar->SetButtonPressed(this, i+1000, true);
         }
      }
      return true;
   }

//----------------------------
};

//----------------------------

I3D_RENDERSTATE C_edit_View::i3d_map[E_LAST] = {
   RS_DRAWHRBOUNDBOX, RS_DRAWBOUNDBOX, RS_CLEAR, RS_WIREFRAME,
   RS_LINEARFILTER, RS_MIPMAP, RS_DITHER, RS_FOG,
   RS_USELMAPPING, RS_DRAWSECTORS, RS_DRAWPORTALS, RS_DRAWVOLUMES, 
   RS_DRAWCAMERAS, RS_DRAWVISUALS, RS_DRAWLIGHTS, RS_DRAWSOUNDS,
   RS_DRAWLMTEXTURES, RS_DRAWDUMMYS, RS_DRAWOCCLUDERS, RS_DRAWTEXTURES,
   RS_DRAWJOINTS, RS_USE_OCCLUSION, RS_ENVMAPPING, RS_USE_EMBM,
   RS_DRAWMIRRORS, RS_USESHADOWS, RS_DEBUGDRAWSHADOWS, RS_DEBUGDRAWSHDRECS,
   RS_DEBUGDRAWBSP, RS_DEBUGDRAWDYNAMIC, RS_DEBUGDRAWSTATIC, RS_DETAILMAPPING,
   RS_DRAW_COL_TESTS, RS_DEBUG_SHOW_OVERDRAW,
};

//----------------------------

void CreateView(PC_editor ed){
   PC_editor_item ei = new C_edit_View;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------