#include "all.h"
#include "common.h"
#include "Properties.h"

//----------------------------

                              //debug:
//#define DEBUG_SELECT_LAST_SHEET  //select last active sheet by default

#define IDC_VIS_PROP_GROUP 20000

//----------------------------

static void MapDlgUnits(HWND hwnd_dlg, int *x, int *y){

   RECT rc;
   SetRect(&rc, x ? *x : 0, y ? *y : 0, x ? *x : 0, y ? *y : 0);
   MapDialogRect(hwnd_dlg, &rc);
   if(x) *x=rc.left;
   if(y) *y=rc.top;
}

//----------------------------

static const S_ctrl_help_info help_texts[] = {
   IDC_COMBO_VISUAL_TYPE, "Type of visual. This type defines the specialization class of visual object, and affects rendering.",
   IDC_CHECK_NO_SHADOW, "Frame will not cast shadow into lightmaps.",
   IDC_CHECK_SHADOW_REC, "Frame will receive dynamic shadows.",
   IDC_CHECK_VOLUME_STATIC, "Visual's geometry will be used for building BSP collision tree.",
   IDC_COMBO_COL_MAT, "Logical material of geometry. It is used for collision testing.",
   IDC_RESET_COL_MAT, "Additional properties of visual. The actual properties depend on visual type.",
   IDC_BUTTON_VISUAL_SAME_AS, "Copy properties from other visual. Click visual in scene, from which properties will be copied.",
   IDC_BUTTON_VISUAL_SAME_AS2, "Copy properties from other visual. Browse visual in dialog, from which properties will be copied.",
   IDC_BUTTON_VISUAL_COPY, "Copy visual's properties into clipboard.",
                              //light sheet:
   IDC_COMBO_LIGHT_TYPE, "Type of light. The light computations and use is based primarily on its type.",
   IDC_BUTTON_LIGHT_COLOR, "Color of light. Note that the color is additionally multiplied by power value.",
   IDC_EDIT_LIGHT_POWER, "Power of light. This is a multiplier value for light's color. The power may be also negative.",
   IDC_EDIT_LIGHT_RANGE_N, "Light's near range. This is the distance, until light's intensity is at 100%.",
   IDC_EDIT_LIGHT_RANGE_F, "Light's far range. This is the distance, at which light's intensity is at 0 %, and beyod which light no more illuminates.",
   IDC_EDIT_LIGHT_CONE_IN, "Light's near cone. The cone is used for spot-light type.",
   IDC_EDIT_LIGHT_CONE_OUT, "Light's far cone. The cone is used for spot-light type.",
   IDC_CHECK_LIGHT_MODE_V, "Flag specifying that light applies onto vertex-lit visuals.",
   IDC_CHECK_LIGHT_MODE_LM, "Flag specifying that light applies onto light-mapped visuals.",
   IDC_CHECK_LIGHT_MODE_SHD, "Flag specifying that light is used for computation of dynamic shadows.",
   IDC_CHECK_LIGHT_MODE_DYN, "Flag specifying that light is also applied on vertices of light-mapped visuals.",
   0
};

//----------------------------

static const struct{
   const char *name;
   I3D_LIGHTTYPE lt;
} light_type_list[] = {
   "Ambient", I3DLIGHT_AMBIENT,
   "Point", I3DLIGHT_POINT,
   "Directional", I3DLIGHT_DIRECTIONAL,
   "Spot", I3DLIGHT_SPOT,
   "PointAmbient", I3DLIGHT_POINTAMBIENT,
   "Fog", I3DLIGHT_FOG,
   //"PointFog", I3DLIGHT_POINTFOG,
   "LayeredFog", I3DLIGHT_LAYEREDFOG,
   NULL
};

static const struct{
   const char *name;
   I3D_SOUNDTYPE st;
} sound_type_list[] = {
   "Ambient", I3DSOUND_AMBIENT,
   "Point", I3DSOUND_POINT,
   "Spot", I3DSOUND_SPOT,
   "PointAmbient", I3DSOUND_POINTAMBIENT,
   NULL
};

static const struct{
   const char *name;
   I3D_VOLUMETYPE vt;
} volume_type_list[] = {
   "Sphere", I3DVOLUME_SPHERE,
   "Rectangle", I3DVOLUME_RECTANGLE,
   "Box", I3DVOLUME_BOX,
   "Cylinder", I3DVOLUME_CYLINDER,
   "Cap Cylinder", I3DVOLUME_CAPCYL,
   NULL
};

//----------------------------

                              //editor plugin: Properties
class C_edit_Properties_imp: public C_edit_Properties_ed{
   virtual const char *GetName() const{ return "Properties"; }

//----------------------------

   enum{
      E_PROP_SHOW = 10000,       // - 
      E_PROP_RELOAD_MODEL,       //(char *frm_name "\0" char *model_name) - internal (undo)

      E_PROP_NOTIFY_SELECTION,   //(C_vector<PI3D_frame> *sel_list) - notification received from selection
      E_PROP_NOTIFY_MODIFY,      //(pair<PI3D_frame, dword flags>*) - notification received from modify
      E_PROP_RESET_SHEET,        //reset current sheet
      E_PROP_NOTIFY_PICK_MODEL,  //(PI3D_frame) - notification received from MouseEdit
      E_PROP_NOTIFY_PICK_LIGHT,  //(PI3D_frame) - notification received from MouseEdit
      E_PROP_NOTIFY_PICK_LSECT,  //(PI3D_frame) - notification received from MouseEdit
      E_PROP_NOTIFY_PICK_SSECT,  //(PI3D_frame) - notification received from MouseEdit
      E_PROP_NOTIFY_PICK_VISUAL, //(PI3D_frame) - notification received from MouseEdit
   };

//----------------------------

   enum E_UNDO_ID{
      UNDO_RENAME_FRAME,
      UNDO_SET_VOL_TYPE,
      UNDO_SET_COL_MAT,
      UNDO_SET_FRM_FLAGS,
      UNDO_SET_BRIGHTNESS,
      UNDO_SET_TEMPERATURE,
                              //frame:
      UNDO_TRANSLATE,
      UNDO_ROTATE,
      UNDO_SCALE_U,
      UNDO_SCALE_NU,
                              //light:
      UNDO_LIGHT_TYPE,
      UNDO_LIGHT_COLOR,
      UNDO_LIGHT_POWER,
      UNDO_LIGHT_RANGE,
      UNDO_LIGHT_CONE,
      UNDO_LIGHT_MODE,
      UNDO_LIGHT_SECTOR_ADD,
      UNDO_LIGHT_SECTOR_REM,
                              //sound:
      UNDO_SOUND_TYPE,
      UNDO_SOUND_FILE,
      UNDO_SOUND_RANGE,
      UNDO_SOUND_CONE,
      UNDO_SOUND_VOLUME,
      UNDO_SOUND_VOLUME_OUT,
      UNDO_SOUND_LOOP,
      UNDO_SOUND_STRM,
      UNDO_SOUND_ON,
      UNDO_SOUND_SECTOR_ADD,
      UNDO_SOUND_SECTOR_REM,
                              //sector:
      UNDO_SECTOR_ENV,
                              //visual:
      UNDO_VISUAL_PROPERTY,
   };

//----------------------------

   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_MouseEdit> e_mouseedit;

   dword igraph_init_flags;

   HWND hwnd, hwnd_tc;        //hwnd of property dialog and tab-control
   POINT prop_size, tc_size;  //initial size of window and tab control
   enum{
      SHEET_FRAME,
      SHEET_LIGHT,
      SHEET_SOUND,
      SHEET_CAMERA,

      SHEET_MODEL,
      SHEET_TARGET,
      SHEET_LSECTORS,
      SHEET_VOLUME,

      SHEET_VISUAL,
      SHEET_VISUAL_PROPERTY, //special use - child of visual sheet
      SHEET_UNUSED0,
      SHEET_SSECTORS,

      SHEET_SECTOR,
      SHEET_USER
   };
   dword curr_sheets;
   int curr_sheet_index;

#define NUM_SHEETS 13
   C_vector<HWND> hwnd_sheet;     //sheets are loaded by their name, which is "IDD_PROP_SHEET%.2i"

   int vis_property_ground_pos[2];  //pos of visual's properties, init'ed at startup
   int vis_property_ground_size[2]; //size of visual's properties, init'ed at startup

   bool on;                   //set to true when properties window is on
   bool slct_reset;           //flag set when selection notify received - used in Tick to avoid multiple changes per frame
   bool sheet_reset[32];      //flag set particular sheet need to be re-initialize
   bool in_sheet_modify;      //set true when user is modifying data in sheet
   bool window_active;

                              //material table
   map<dword, C_str> mat_table;
   map<dword, C_str> env_table;

//----------------------------

   void RenameFrame(PI3D_frame frm, const C_str &new_name){

      C_str old_name = frm->GetName();
                              //check if we may concanetate with top undo entry
      C_chunk ck_top;
      if(e_undo->IsTopEntry(this, UNDO_RENAME_FRAME, &ck_top)){
                              //pop top undo entry
         old_name = ck_top.ReadString();
         e_undo->PopTopEntry();
      }
      C_chunk &ck = e_undo->Begin(this, UNDO_RENAME_FRAME, frm);
      ck.Write((const char*)old_name, old_name.Size()+1);
      e_undo->End();

      frm->SetName(new_name);
      ed->SetModified();
   }

//----------------------------

   void ModifyVisualProperty(PI3D_visual vis, dword prop_index, dword value){

      assert(prop_index >= 0 && prop_index < ed->GetDriver()->NumProperties(vis->GetVisualType()));

      dword curr = vis->GetProperty(prop_index);
      if(curr!=value){
         e_modify->AddFrameFlags(vis, E_MODIFY_FLG_VISUAL);
                              //save undo
         C_chunk &ck_undo = e_undo->Begin(this, UNDO_VISUAL_PROPERTY, vis);
         ck_undo.Write(prop_index);
         ck_undo.Write(curr);
         e_undo->End();

                              //modify visual's property
         vis->SetProperty(prop_index, value);

         ed->SetModified();
      }
   }

//----------------------------

   void ModifyVisualsProperty(const C_vector<PI3D_frame> &sel_list, dword prop_index, dword value){

                              //modify all visuals
      for(int i=sel_list.size(); i--; ){
         PI3D_frame frm = sel_list[i];
         assert(frm->GetType()==FRAME_VISUAL);
         if(frm->GetType()!=FRAME_VISUAL)
            continue;
         ModifyVisualProperty(I3DCAST_VISUAL(frm), prop_index, value);
      }
   }

//----------------------------

   static UINT APIENTRY ccHookColor(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam){

      switch(uiMsg){
      case WM_INITDIALOG:
         {
            LPCHOOSECOLOR cc = (LPCHOOSECOLOR)lParam;
            SetWindowLong(hwnd, GWL_USERDATA, cc->lCustData);
         }
         break;
      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case 706: case 707: case 708:
                              //hook RGB edit controlls
            switch(HIWORD(wParam)){
            case EN_CHANGE:
               C_edit_Properties_imp *ep = (C_edit_Properties_imp*)GetWindowLong(hwnd, GWL_USERDATA);
               if(ep){
                  const C_vector<PI3D_frame> &sel_list = ep->e_slct->GetCurSel();
                  S_vector v(GetDlgItemInt(hwnd, 706, NULL, false)/255.0f, 
                     GetDlgItemInt(hwnd, 707, NULL, false)/255.0f, 
                     GetDlgItemInt(hwnd, 708, NULL, false)/255.0f);
                  for(dword i=0; i<sel_list.size(); i++){
                     if(sel_list[i]->GetType()==FRAME_LIGHT){
                        PI3D_light lp = I3DCAST_LIGHT(sel_list[i]);

                        lp->SetColor(v);
   
                        ep->ed->SetModified();
                        ep->ed->GetScene()->Render();
                        ep->ed->GetIGraph()->UpdateScreen();
                     }
                  }
               }
               break;
            }
            break;
         }
         break;
      }
      return 0;
   }

//----------------------------

   struct S_sheet_init{
      int sheet_index;
      C_edit_Properties_imp *ep;
   };

   class C_property_control: public C_unknown{
   public:
      HWND hwnd;
      int base_x, base_y;

      C_property_control():
         hwnd(NULL)
      {
      }
      C_property_control(HWND h1, int bx1, int by1):
         hwnd(h1),
         base_x(bx1),
         base_y(by1)
      {
      }
      ~C_property_control(){
         if(hwnd){
            DestroyWindow(hwnd);
         }
      }
   };

   C_vector<C_smart_ptr<C_property_control> > prop_controls;
   int vis_prop_scroll_y, vis_prop_scroll_max_y;

//----------------------------
// Scroll all visual's properties to align with slide-bar.
   void ScrollVisualProperties(){
                     //update all controls
      for(int i=prop_controls.size(); i--; ){
         C_property_control *pc = prop_controls[i];
         if(pc->base_y==0x80000000)
            continue;
         SetWindowPos(pc->hwnd, NULL, pc->base_x, pc->base_y - vis_prop_scroll_y,
            0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE);
      }
   }

//----------------------------
// Replace model - load new file. Delete all frames from modify list,
// modify all common frames the same way as original.
   bool ReplaceModel(PI3D_model mod, const C_str &filename){


      const C_str &old_name = mod->GetFileName();
      if(filename.Matchi(old_name))
         return false;

      {
                              //save undo
         int len = mod->GetName().Size() + old_name.Size() + 2;
         char *cp = new char[len];
         strcpy(cp, mod->GetName());
         strcpy(cp+mod->GetName().Size()+1, old_name);
         e_undo->Save(this, E_PROP_RELOAD_MODEL, cp, len);
         delete[] cp;
      }

                              //save old params
      C_vector<C_smart_ptr<I3D_frame> > frm_save;
      PI3D_frame const *const frms = mod->GetFrames();
      for(dword jj = mod->NumFrames(); jj--; ){
         frm_save.push_back(frms[jj]);
      }

                              //find all frames linked to model's frames to restore their links after model change
      typedef pair<C_smart_ptr<I3D_frame>, C_str> t_frm_link;
      C_vector<t_frm_link> frm_linked;
      for(dword i1 = mod->NumFrames(); i1--; ){
         PI3D_frame frm_owned = frms[i1];
         for(int i2 = frm_owned->NumChildren(); i2--; ){
            PI3D_frame frm_child = frm_owned->GetChildren()[i2];
            if(FindPointerInArray((void**)mod->GetFrames(), mod->NumFrames(), frm_child) == -1){
               frm_linked.push_back(t_frm_link(frm_child, frm_owned->GetOrigName()));
               frm_child->LinkTo(NULL);
            }
         }
      }

      map<PI3D_frame, C_str> modify_link;
      for(int ii=frm_save.size(); ii--; ){
                              //commnet out: we need old frames for copy frames flags
         dword flags = e_modify->GetFrameFlags(frm_save[ii]);
         //if(!flags){
         //   frm_save[ii] = frm_save.back();
         //   frm_save.pop_back();
         //   continue;
         //}
         if(flags&E_MODIFY_FLG_LINK){
                              //store link of modified frame, after model close it can be lost
            PI3D_frame frm_parent = frm_save[ii]->GetParent();
            modify_link[frm_save[ii]] = frm_parent ? frm_parent->GetName() : "";
         }
      }

                              //store all frames linked to model (and not owned by model)
      C_vector<C_smart_ptr<I3D_frame> > model_linked;
      for(dword i3 = mod->NumChildren(); i3--; ){
         PI3D_frame frm_child = mod->GetChildren()[i3];
         if(FindPointerInArray((void**)mod->GetFrames(), mod->NumFrames(), frm_child) == -1){
            model_linked.push_back(frm_child);
            frm_child->LinkTo(NULL);
         }
      }

                              //unlink owned frames, so they do not appear in editor (model::Close not do it)
                              // //also notify bsp, that we changed frames
      {
         C_smart_ptr<C_editor_item> e_bsp = ed->FindPlugin("BSPTool");
         for(dword j2 = mod->NumFrames(); j2--; ){
            PI3D_frame frm = frms[j2];
                              //broadcast delete action
            ed->Broadcast((void(C_editor_item::*)(void*))&C_editor_item::OnFrameDelete, frm);
            //frm->LinkTo(NULL);
         }
      }

      ed->GetModelCache().Open(mod, filename, ed->GetScene(), 0, OPEN_ERR_LOG(ed, filename));

                              //link all frames which was not owned by model back to model
      for(int i4 = model_linked.size(); i4--; ){
         model_linked[i4]->LinkTo(mod);
      }
                              //now apply changes from saved frames
                              // and remove old frames from modify list
      for(ii=frm_save.size(); ii--; ){
         PI3D_frame frm = frm_save[ii];
                              //unlink self, so we do not find self as child of model
         frm->LinkTo(NULL);
         PI3D_frame frm_new = mod->FindChildFrame(frm->GetOrigName());
         dword flags = e_modify->GetFrameFlags(frm);
         bool del_frame_flags = flags;
         
         if(frm_new){
            assert(frm != frm_new);

            flags &= (E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE | E_MODIFY_FLG_LINK | E_MODIFY_FLG_NU_SCALE);
            if(flags){
               e_modify->AddFrameFlags(frm_new, flags);
               if(flags&E_MODIFY_FLG_LINK){
                  const C_str &str_parent = modify_link[frm];
                  PI3D_frame frm_parent = str_parent.Size() ? ed->GetScene()->FindFrame(modify_link[frm]) : NULL;
                  if(frm_parent || !str_parent.Size())
                     frm_new->LinkTo(frm_parent);
               }
               if(flags&E_MODIFY_FLG_POSITION)
                  frm_new->SetPos(frm->GetPos());
               if(flags&E_MODIFY_FLG_ROTATION)
                  frm_new->SetRot(frm->GetRot());
               if(flags&E_MODIFY_FLG_SCALE)
                  frm_new->SetScale(frm->GetScale());
               if(flags&E_MODIFY_FLG_NU_SCALE){
                  if(frm_new->GetType()==FRAME_VOLUME && frm->GetType() ==FRAME_VOLUME){
                     I3DCAST_VOLUME(frm_new)->SetNUScale(I3DCAST_VOLUME(frm)->GetNUScale());
                  }
               }
            }
            if(frm_new->GetName().Match(frm->GetName()))
               del_frame_flags = false;
                              //copy frame flags
            frm_new->SetFlags(frm->GetFlags(), (dword)-1);
                              //copy material
            if(frm_new->GetFlags()&I3D_FRMF_STATIC_COLLISION){
               e_modify->AddFrameFlags(frm_new, E_MODIFY_FLG_FRM_FLAGS);

               dword mat_id = 0;
               switch(frm->GetType()){
               case FRAME_VOLUME: mat_id = I3DCAST_VOLUME(frm)->GetCollisionMaterial(); break;
               case FRAME_VISUAL: mat_id = I3DCAST_VISUAL(frm)->GetCollisionMaterial(); break;
               default: 
                  break;
               }
               /*
               switch(frm_new->GetType()){
               case FRAME_VOLUME:
                  {
                     I3DCAST_VOLUME(frm_new)->SetCollisionMaterial(mat_id);
                     e_modify->AddFrameFlags(frm_new, E_MODIFY_FLG_VOLUME);
                  }
                  break;
               case FRAME_VISUAL:
                  {
                     I3DCAST_VISUAL(frm_new)->SetCollisionMaterial(mat_id); 
                     e_modify->AddFrameFlags(frm_new, E_MODIFY_FLG_VISUAL);
                  }
                  break;
               }
               */
            }
         }
         if(del_frame_flags)
            e_modify->RemoveFrame(frm);
      }
                              //now link all frames back to model's frames 
      for(int i5 = frm_linked.size(); i5--; ){
         PI3D_frame frm = frm_linked[i5].first;
         PI3D_frame frm_new_parent = mod->FindChildFrame(frm_linked[i5].second);
         if(frm_new_parent){
            frm->LinkTo(frm_new_parent);
         }
      }
                              //set its own anim
      if(mod->GetAnimationSet())
         mod->SetAnimation(0, mod->GetAnimationSet(), I3DANIMOP_LOOP);
      ed->SetModified();

      return true;
   }

//----------------------------
// Copy properties of visual frame from 'from' visual to all selected visuals which are
// of the same type. Saves UNDO for all changes, sets MODIFY property of editor.
   bool CopyProperties(PI3D_visual from){

      if(!ed->CanModify())
         return false;

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      dword vtype = from->GetVisualType();
      dword num_props = ed->GetDriver()->NumProperties(vtype);

                              //change all compatible visuals
      int count = 0;
      for(int i=sel_list.size(); i--; ){
         PI3D_frame frm = sel_list[i];
         if(frm->GetType()!=FRAME_VISUAL)
            continue;
         PI3D_visual vis = I3DCAST_VISUAL(frm);
         if(vis->GetVisualType() != vtype || vis==from)
            continue;
                              //copy all properties
         for(dword pi=0; pi<num_props; pi++)
            ModifyVisualProperty(vis, pi, from->GetProperty(pi));
         ++count;
      }
      InitSheet(SHEET_VISUAL);
      ed->Message(C_fstr("Visuals updated: %i.", count), 0, EM_WARNING);
      return true;
   }

//----------------------------

   void InitSheet_frame(HWND hwnd, bool reset_marks_only = false){

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      if(!sel_list.size())
         return;
      PI3D_frame frm0 = sel_list.front();
                              //name
      if(sel_list.size()==1){
         SetDlgItemText(hwnd, IDC_EDIT_FRM_NAME, frm0->GetName());
                  //enable for created
         bool created = (e_modify->GetFrameFlags(frm0)&E_MODIFY_FLG_CREATE);
         HWND hwnd_name = GetDlgItem(hwnd, IDC_EDIT_FRM_NAME);
         EnableWindow(hwnd_name, created);
      }else{
         SetDlgItemText(hwnd, IDC_EDIT_FRM_NAME, "");
         EnableWindow(GetDlgItem(hwnd, IDC_EDIT_FRM_NAME), false);
      }
                  //P/R/S
      const S_vector &pos = frm0->GetPos();
      const S_quat &rot = frm0->GetRot();
      const float scale = frm0->GetScale();
      const C_str &link = frm0->GetParent() ? frm0->GetParent()->GetName() : NULL;
      bool valid[6] = {1, 1, 1, 1, 1, 1};
      dword reset[4] = {0, 0, 0, 0};

      for(dword i=0; i<sel_list.size(); i++){
         PI3D_frame frm = sel_list[i];
         if(i && !reset_marks_only){
            const S_vector &p1 = frm->GetPos();
            for(dword j=0; j<3; j++) valid[j] = (valid[j] && pos[j]==p1[j]);
            const S_quat &r1 = frm->GetRot();
            valid[3] = (valid[3] && rot==r1);
            valid[4] = (valid[4] && frm->GetScale()==scale);
            valid[5] = (valid[5] && link==(frm->GetParent() ? frm->GetParent()->GetName() : NULL));
         }
                  //check what can be reset
         dword flg = e_modify->GetFrameFlags(frm);
         if(!(flg&E_MODIFY_FLG_CREATE)){
            if(flg&E_MODIFY_FLG_POSITION) ++reset[0];
            if(flg&E_MODIFY_FLG_ROTATION) ++reset[1];
         }
         if(flg&E_MODIFY_FLG_SCALE) ++reset[2];
         if(flg&E_MODIFY_FLG_LINK) ++reset[3];
      }
      if(!reset_marks_only){
         static const int id_map_pos[] = {IDC_EDIT_FRM_POS_X, IDC_EDIT_FRM_POS_Y, IDC_EDIT_FRM_POS_Z};
         static const int id_map_rot[] = {IDC_EDIT_FRM_ROT_X, IDC_EDIT_FRM_ROT_Y, IDC_EDIT_FRM_ROT_Z};
         for(dword j=0; j<3; j++)
            SetDlgItemText(hwnd, id_map_pos[j], !valid[j] ? "?" : FloatStrip(C_fstr("%f", pos[j])));

         S_vector axis;
         float angle = 0.0f;
         if(valid[3]) rot.Inverse(axis, angle);
         for(j=0; j<3; j++)
            SetDlgItemText(hwnd, id_map_rot[j], !valid[3] ? "?" : FloatStrip(C_fstr("%f", axis[j])));
         SetDlgItemText(hwnd, IDC_EDIT_FRM_ROT_A, !valid[3] ? "?" : FloatStrip(C_fstr("%f", angle*180.0f/PI)));
         SetDlgItemText(hwnd, IDC_EDIT_FRM_SCALE_U, !valid[4] ? "?" : FloatStrip(C_fstr("%f", scale)));
         SetDlgItemText(hwnd, IDC_EDIT_FRM_PARENT, !valid[5] ? "?" : link);
      }
      static const word id_reset[] = { IDC_RESET_POS, IDC_RESET_ROT, IDC_RESET_SCL, IDC_RESET_LINK};
      for(i=4; i--; ){
         dword id = id_reset[i];
         ShowWindow(GetDlgItem(hwnd, id), reset[i] ? SW_SHOW : SW_HIDE);
         if(reset[i]){
            SetDlgItemText(hwnd, id, (reset[i]==sel_list.size()) ? "R" : "r");
         }
      }
   }

//----------------------------

   void InitSheet_volume(HWND hwnd, bool reset_marks_only = false){

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      if(!sel_list.size())
         return;
      PI3D_frame frm0 = sel_list.front();
      assert(frm0->GetType()==FRAME_VOLUME);
      PI3D_volume vol0 = I3DCAST_VOLUME(frm0);
      I3D_VOLUMETYPE vtype = vol0->GetVolumeType();
      dword mat_id = vol0->GetCollisionMaterial();
      bool col_static = (frm0->GetFlags()&I3D_FRMF_STATIC_COLLISION);
      S_vector scl = vol0->GetNUScale();
      bool valid[6] = {true, true, true, true, true, true};
      dword reset[4] = {0, 0, 0, 0};

      bool scale_on = true;
      switch(vtype){
      case I3DVOLUME_SPHERE:
         valid[1] = valid[2] = valid[3] = false;
         scale_on = false;
         break;
      }

      for(dword i=0; i<sel_list.size(); i++){
         PI3D_frame frm = sel_list[i];
         if(frm->GetType()!=FRAME_VOLUME) continue;
         if(i && !reset_marks_only){
            PI3D_volume vol = I3DCAST_VOLUME(frm);
            valid[0] = (valid[0] && (vtype==vol->GetVolumeType()));
            const S_vector &s1 = vol->GetNUScale();
            for(dword j=0; j<3; j++) valid[1+j] = (valid[1+j] && scl[j]==s1[j]);
            valid[4] = (valid[4] && vol->GetCollisionMaterial()==mat_id);
            bool col_static1 = (frm->GetFlags()&I3D_FRMF_STATIC_COLLISION);
            valid[5] = (valid[5] && (col_static==col_static1));
         }
                  //check what can be reset
         dword flg = e_modify->GetFrameFlags(frm);
         if(!(flg&E_MODIFY_FLG_CREATE)){
            if(flg&E_MODIFY_FLG_VOLUME_TYPE) ++reset[0];
            if(flg&E_MODIFY_FLG_COL_MAT) ++reset[1];
         }
         if(flg&E_MODIFY_FLG_NU_SCALE) ++reset[2];
         if(flg&E_MODIFY_FLG_FRM_FLAGS) ++reset[3];
      }
      if(!reset_marks_only){
                              //type
         {
            int vti = -1;
            if(valid[0]){
               for(int i=0; volume_type_list[i].name; i++){
                  if(volume_type_list[i].vt==vtype){
                     vti = i;
                     break;
                  }
               }
            }
            SendDlgItemMessage(hwnd, IDC_COMBO_VOLUME_TYPE, CB_SETCURSEL, vti, 0);
         }
                              //scale
         static const int id_map_scl[] = {IDC_EDIT_VOL_NUSCALE_X, IDC_EDIT_VOL_NUSCALE_Y, IDC_EDIT_VOL_NUSCALE_Z};
         for(dword j=0; j<3; j++){
            EnableWindow(GetDlgItem(hwnd, id_map_scl[j]), scale_on);
            SetDlgItemText(hwnd, id_map_scl[j], !valid[1+j] ? "?" : FloatStrip(C_fstr("%f", scl[j])));
         }
         if(valid[4]){
            int i = 0;
            map<dword, C_str>::const_iterator it;
            for(it=mat_table.begin(); it!=mat_table.end(); it++, i++)
               if((*it).first==mat_id) break;
            if(it==mat_table.end()) i = -1;
            SendDlgItemMessage(hwnd, IDC_COMBO_COL_MAT, CB_SETCURSEL, i, 0);
         }else{
            SendDlgItemMessage(hwnd, IDC_COMBO_COL_MAT, CB_SETCURSEL, (dword)-1, 0);
         }
                     //set 'static collision' checkbox
         CheckDlgButton(hwnd, IDC_CHECK_VOLUME_STATIC, !valid[5] ? BST_INDETERMINATE : col_static ? BST_CHECKED : BST_UNCHECKED);
      }
      static const word id_reset[] = { IDC_RESET_VOL_TYPE, IDC_RESET_COL_MAT, IDC_RESET_NU_SCL, IDC_RESET_FRM_FLAGS};
      for(i=4; i--; ){
         dword id = id_reset[i];
         ShowWindow(GetDlgItem(hwnd, id), reset[i] ? SW_SHOW : SW_HIDE);
         if(reset[i]){
            SetDlgItemText(hwnd, id, (reset[i]==sel_list.size()) ? "R" : "r");
         }
      }
   }

//----------------------------

   void InitSheet_light(HWND hwnd, bool reset_marks_only = false){

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      if(!sel_list.size())
         return;

      PI3D_frame frm0 = sel_list.front();
      assert(frm0->GetType()==FRAME_LIGHT);
      PI3D_light lp0 = I3DCAST_LIGHT(frm0);
      I3D_LIGHTTYPE ltype = lp0->GetLightType();
      S_vector color = lp0->GetColor();
      float power = lp0->GetPower();
      bool mode_v = lp0->GetMode()&I3DLIGHTMODE_VERTEX;
      bool mode_lm = lp0->GetMode()&I3DLIGHTMODE_LIGHTMAP;
      bool mode_shd = lp0->GetMode()&I3DLIGHTMODE_SHADOW;
      bool mode_dyn = lp0->GetMode()&I3DLIGHTMODE_DYNAMIC_LM;
      float range_n, range_f;
      lp0->GetRange(range_n, range_f);
      float cone_n, cone_f;
      lp0->GetCone(cone_n, cone_f);
      bool valid[14] = {
         true,
         true,    //1 - color
         true,    //2 - power
         true, true, //3, 4 - cone
         true, true, //5, 6 - fange
         true,    //7 - vertex mode
         true,    //8 - LM mode
         true,    //9 - specular
         true,    //10 - specular power
         true,    //11 - specular angle
         true,    //12 - shadow
         true,    //13 - DynLM
      };
      //dword reset = 0;

      for(dword i=0; i<sel_list.size(); i++){
         PI3D_frame frm = sel_list[i];
         if(frm->GetType()!=FRAME_LIGHT)
            continue;
         if(i && !reset_marks_only){
            PI3D_light lp = I3DCAST_LIGHT(frm);
            valid[0] = (valid[0] && (ltype==lp->GetLightType()));
            valid[1] = (valid[1] && (color==lp->GetColor()));
            valid[2] = (valid[2] && (power==lp->GetPower()));

            float cn, cf;
            lp->GetCone(cn, cf);
            valid[3] = (valid[3] && (cone_n==cn));
            valid[4] = (valid[4] && (cone_f==cf));

            float rn, rf;
            lp->GetRange(rn, rf);
            valid[5] = (valid[5] && (range_n==rn));
            valid[6] = (valid[6] && (range_f==rf));

            bool mv = (lp->GetMode()&I3DLIGHTMODE_VERTEX);
            bool mlm = (lp->GetMode()&I3DLIGHTMODE_LIGHTMAP);
            bool mshd = (lp->GetMode()&I3DLIGHTMODE_SHADOW);
            bool mdyn = (lp->GetMode()&I3DLIGHTMODE_DYNAMIC_LM);
            valid[7] = (valid[7] && (mode_v == mv));
            valid[8] = (valid[8] && (mode_lm == mlm));
            valid[12] = (valid[12] && (mode_shd==mshd));
            valid[13] = (valid[13] && (mode_dyn==mdyn));
         }
         /*
                  //check what can be reset
         dword flg = e_modify->GetFrameFlags(frm);
         if(!(flg&E_MODIFY_FLG_CREATE)){
            if(flg&E_MODIFY_FLG_LIGHT) ++reset;
         }
         */
      }
      if(!reset_marks_only){
                              //type
         {
            int lti = -1;
            if(valid[0]){
               for(int i=0; light_type_list[i].name; i++){
                  if(light_type_list[i].lt==ltype){
                     lti = i;
                     break;
                  }
               }
            }
            SendDlgItemMessage(hwnd, IDC_COMBO_LIGHT_TYPE, CB_SETCURSEL, lti, 0);
         }
                              //color (force redraw button)
         SetWindowLong(GetDlgItem(hwnd, IDC_BUTTON_LIGHT_COLOR), GWL_USERDATA, 
            !valid[1] ? GetSysColor(COLOR_BTNFACE) :
            RGB(color[0]*255.0f, color[1]*255.0f, color[2]*255.0f));
         ShowWindow(GetDlgItem(hwnd, IDC_BUTTON_LIGHT_COLOR), SW_HIDE);
         ShowWindow(GetDlgItem(hwnd, IDC_BUTTON_LIGHT_COLOR), SW_SHOW);

                              //specular
         bool specular_on = (valid[0] && ltype!=I3DLIGHT_FOG && ltype!=I3DLIGHT_NULL && ltype!=I3DLIGHT_AMBIENT);
         if(!specular_on) valid[9] = valid[10] = valid[11] = false;
                  
                              //power
         bool power_on = (valid[0] && ltype!=I3DLIGHT_FOG && ltype!=I3DLIGHT_NULL);
         if(!power_on) valid[2] = false;
         EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIGHT_POWER), power_on);
         SetDlgItemText(hwnd, IDC_EDIT_LIGHT_POWER, !valid[2] ? "?" : FloatStrip(C_fstr("%f", power)));
                              //cone
         bool cone_on = (valid[0] && ltype==I3DLIGHT_SPOT);
         if(!cone_on) valid[3] = valid[4] = false;
         EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIGHT_CONE_IN), cone_on);
         EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIGHT_CONE_OUT), cone_on);
         SetDlgItemText(hwnd, IDC_EDIT_LIGHT_CONE_IN, !valid[3] ? "?" : FloatStrip(C_fstr("%f", cone_n*180.0f/PI)));
         SetDlgItemText(hwnd, IDC_EDIT_LIGHT_CONE_OUT, !valid[4] ? "?" : FloatStrip(C_fstr("%f", cone_f*180.0f/PI)));
                              //range
         bool range_on = (valid[0] && (ltype==I3DLIGHT_POINT || ltype==I3DLIGHT_SPOT
            || ltype==I3DLIGHT_POINTAMBIENT || ltype==I3DLIGHT_FOG ||
            //ltype==I3DLIGHT_DIRECTIONAL ||
            ltype==I3DLIGHT_POINTFOG || ltype==I3DLIGHT_LAYEREDFOG));
         bool range_on_n = (range_on || (ltype==I3DLIGHT_FOG));
         if(!range_on) valid[6] = false;
         if(!range_on && !range_on_n) valid[5] = false;
         EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIGHT_RANGE_N), range_on || range_on_n);
         EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LIGHT_RANGE_F), range_on);
         SetDlgItemText(hwnd, IDC_EDIT_LIGHT_RANGE_N, !valid[5] ? "?" : FloatStrip(C_fstr("%f", range_n)));
         SetDlgItemText(hwnd, IDC_EDIT_LIGHT_RANGE_F, !valid[6] ? "?" : FloatStrip(C_fstr("%f", range_f)));
                     //mode
         CheckDlgButton(hwnd, IDC_CHECK_LIGHT_MODE_V, !valid[7] ? BST_INDETERMINATE : mode_v ? BST_CHECKED : BST_UNCHECKED);
         CheckDlgButton(hwnd, IDC_CHECK_LIGHT_MODE_LM, !valid[8] ? BST_INDETERMINATE : mode_lm ? BST_CHECKED : BST_UNCHECKED);
         CheckDlgButton(hwnd, IDC_CHECK_LIGHT_MODE_SHD, !valid[12] ? BST_INDETERMINATE : mode_shd ? BST_CHECKED : BST_UNCHECKED);
         CheckDlgButton(hwnd, IDC_CHECK_LIGHT_MODE_DYN, !valid[13] ? BST_INDETERMINATE : mode_dyn ? BST_CHECKED : BST_UNCHECKED);
      }
      /*
      {
         ShowWindow(GetDlgItem(hwnd, IDC_RESET_LIGHT), reset ? SW_SHOW : SW_HIDE);
         if(reset){
            SetDlgItemText(hwnd, IDC_RESET_LIGHT, (reset==sel_list.size()) ? "R" : "r");
         }
      }
      */
   }

//----------------------------

   void InitSheet_sound(HWND hwnd, bool reset_flags_only){

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      if(!sel_list.size())
         return;
      if(reset_flags_only)
         return;
      if(!sel_list.size())
         return;
      PI3D_frame frm0 = sel_list.front();
      if(frm0->GetType()!=FRAME_SOUND)
         return;
      PI3D_sound snd0 = I3DCAST_SOUND(frm0);
      I3D_SOUNDTYPE stype = snd0->GetSoundType();
      float vol = snd0->GetVolume();
      float out_vol = snd0->GetOutVol();
      bool loop = snd0->IsLoop();
      bool strm = snd0->IsStreaming();
      bool on = snd0->IsOn();
      float cone_i, cone_o;
      snd0->GetCone(cone_i, cone_o);
      float range_n, range_f;
      snd0->GetRange(range_n, range_f);
      C_str fname = snd0->GetFileName();

      set<PISND_source> snd_srcs;        //normal sound sources
      dword sam_size = 0;

      if(snd0->GetSoundSource()){
         if(!snd0->IsStreaming())
            snd_srcs.insert(snd0->GetSoundSource());
         else{
            const S_wave_format &wf = *snd0->GetSoundSource()->GetFormat();
            sam_size += (((int)(wf.samples_per_sec*wf.bytes_per_sample*1.0f)) & -16);
         }
      }
      bool valid[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

      for(dword i=1; i<sel_list.size(); i++){
         PI3D_frame frm = sel_list[i];
         if(frm->GetType()!=FRAME_SOUND) continue;
         PI3D_sound snd = I3DCAST_SOUND(frm);
         valid[0] = (valid[0] && (stype==snd->GetSoundType()));
         //valid[1] = (valid[1] && fname.Match(snd->GetFileName()));
         valid[1] = (valid[1] && fname == snd->GetFileName());
         float ci, co;
         snd->GetCone(ci, co);
         float rn, rf;
         snd->GetRange(rn, rf);
         //valid[2] = (valid[2] && (fade==f));
         valid[3] = (valid[3] && (cone_i==ci));
         valid[4] = (valid[4] && (cone_o==co));
         valid[5] = (valid[5] && (range_n==rn));
         valid[6] = (valid[6] && (range_f==rf));
         valid[7] = (valid[7] && (vol==snd->GetVolume()));
         valid[8] = (valid[8] && (out_vol==snd->GetOutVol()));
         valid[9] = (valid[9] && (loop==snd->IsLoop()));
         valid[10] = (valid[10] && (on==(bool)snd->IsOn()));
         valid[11] = (valid[11] && (strm==snd->IsStreaming()));

         //sam_size += !snd->GetSoundSource() ? 0 : snd->GetSoundSource()->GetFormat()->size;
         if(snd->GetSoundSource()){
            if(!snd->IsStreaming())
               snd_srcs.insert(snd->GetSoundSource());
            else{
               const S_wave_format &wf = *snd->GetSoundSource()->GetFormat();
               sam_size += (((int)(wf.samples_per_sec*wf.bytes_per_sample*1.0f)) & -16);
            }
         }
      }
                  //type
      {
         int sti = -1;
         if(valid[0]){
            for(int i=0; sound_type_list[i].name; i++){
               if(sound_type_list[i].st==stype){
                  sti = i;
                  break;
               }
            }
         }
         SendDlgItemMessage(hwnd, IDC_COMBO_SOUND_TYPE, CB_SETCURSEL, sti, 0);
      }

      //SendDlgItemMessage(hwnd, IDC_COMBO_SOUND_TYPE, CB_SETCURSEL, valid[0] ? stype : -1, 0);
                  //filename
      ed->GetSoundCache().GetRelativeDirectory(&fname[0]);
      SetDlgItemText(hwnd, IDC_EDIT_SOUND_FILE, !valid[1] ? "?" : fname);

      {           //sample size

                  //collect size of samples
         for(set<PISND_source>::const_iterator it = snd_srcs.begin(); it!=snd_srcs.end(); ++it){
            const S_wave_format *wf = (*it)->GetFormat();
            if(wf)
               sam_size += wf->size;
         }
         SetDlgItemInt(hwnd, IDC_EDIT_SOUND_SIZE, sam_size, true);
      }
                  //range
      bool range_on = (valid[0] && stype!=I3DSOUND_NULL && stype!=I3DSOUND_AMBIENT);
      if(!range_on) valid[2] = valid[5] = valid[6] = false;
      EnableWindow(GetDlgItem(hwnd, IDC_EDIT_SOUND_RANGE_N), range_on);
      EnableWindow(GetDlgItem(hwnd, IDC_EDIT_SOUND_RANGE_F), range_on);
      SetDlgItemText(hwnd, IDC_EDIT_SOUND_RANGE_N, !valid[5] ? "?" : FloatStrip(C_fstr("%f", range_n)));
      SetDlgItemText(hwnd, IDC_EDIT_SOUND_RANGE_F, !valid[6] ? "?" : FloatStrip(C_fstr("%f", range_f)));
                  //cone
      bool cone_on = (valid[0] && stype==I3DSOUND_SPOT);
      if(!cone_on) valid[3] = valid[4] = false;
      EnableWindow(GetDlgItem(hwnd, IDC_EDIT_SOUND_CONE_IN), cone_on);
      EnableWindow(GetDlgItem(hwnd, IDC_EDIT_SOUND_CONE_OUT), cone_on);
      SetDlgItemText(hwnd, IDC_EDIT_SOUND_CONE_IN, !valid[3] ? "?" : FloatStrip(C_fstr("%f", cone_i*180.0f/PI)));
      SetDlgItemText(hwnd, IDC_EDIT_SOUND_CONE_OUT, !valid[4] ? "?" : FloatStrip(C_fstr("%f", cone_o*180.0f/PI)));
                  //volume
      bool vol_on = (valid[0] && (stype!=I3DSOUND_NULL));
      if(!vol_on) valid[7] = false;
      EnableWindow(GetDlgItem(hwnd, IDC_EDIT_SOUND_VOL), vol_on);
      SetDlgItemText(hwnd, IDC_EDIT_SOUND_VOL, !valid[7] ? "?" : FloatStrip(C_fstr("%f", vol)));
                  //out volume
      bool out_vol_on = (valid[0] && (stype==I3DSOUND_SPOT));
      if(!out_vol_on) valid[8] = false;
      EnableWindow(GetDlgItem(hwnd, IDC_EDIT_SOUND_VOL_OUT), out_vol_on);
      SetDlgItemText(hwnd, IDC_EDIT_SOUND_VOL_OUT, !valid[8] ? "?" : FloatStrip(C_fstr("%f", out_vol)));
                  //loop
      CheckDlgButton(hwnd, IDC_CHECK_SOUND_MODE_LOOP, !valid[9] ? BST_INDETERMINATE : loop ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwnd, IDC_CHECK_SOUND_MODE_STREAM, !valid[11] ? BST_INDETERMINATE : strm ? BST_CHECKED : BST_UNCHECKED);
                  //on
      CheckDlgButton(hwnd, IDC_CHECK_SOUND_MODE_ON, !valid[10] ? BST_INDETERMINATE : on ? BST_CHECKED : BST_UNCHECKED);
   }

//----------------------------

   void InitSheet_model(HWND hwnd, bool reset_marks_only = false){

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      if(!sel_list.size())
         return;
      PI3D_frame frm0 = sel_list.front();
      bool no_shadow = (frm0->GetFlags()&I3D_FRMF_NOSHADOW);
      bool shadow_cast = (frm0->GetFlags()&I3D_FRMF_SHADOW_CAST);
      assert(frm0->GetType()==FRAME_MODEL);
      PI3D_model mod = I3DCAST_MODEL(frm0);
      float brightness = GetModelBrightness(mod);

      bool valid[3] = {true, true, true};
      C_str name = mod->GetFileName();
      dword reset[2] = {0, 0};

      for(dword i=0; i<sel_list.size(); i++){
         PI3D_frame frm = sel_list[i];
         if(frm->GetType()!=FRAME_MODEL) continue;
         if(i && !reset_marks_only){
            PI3D_model mod = I3DCAST_MODEL(frm);
            if(name != mod->GetFileName()){
               name = "";
            }
            bool no_shadow1 = (frm->GetFlags()&I3D_FRMF_NOSHADOW);
            bool shadow_cast1 = (frm->GetFlags()&I3D_FRMF_SHADOW_CAST);
            float brightness1 = GetModelBrightness(mod);
            valid[0] = (valid[0] && (no_shadow==no_shadow1));
            valid[1] = (valid[1] && (shadow_cast==shadow_cast1));
            valid[2] = (valid[2] && (brightness==brightness1));
         }
                  //check what can be reset
         dword flg = e_modify->GetFrameFlags(frm);
         if(flg&E_MODIFY_FLG_FRM_FLAGS) ++reset[0];
         if(flg&E_MODIFY_FLG_BRIGHTNESS) ++reset[1];
      }
      if(!reset_marks_only){
         SetDlgItemText(hwnd, IDC_EDIT_MODEL_NAME, name);
                              //set frame flags boxes checkbox
         CheckDlgButton(hwnd, IDC_CHECK_NO_SHADOW, !valid[0] ? BST_INDETERMINATE : no_shadow ? BST_CHECKED : BST_UNCHECKED);
         CheckDlgButton(hwnd, IDC_CHECK_SHADOW_CAST, !valid[1] ? BST_INDETERMINATE : shadow_cast ? BST_CHECKED : BST_UNCHECKED);
         SetDlgItemText(hwnd, IDC_EDIT_MODEL_BRIGHTNESS, !valid[2] ? "?" : FloatStrip(C_fstr("%f", brightness)));
      }
      static const word id_reset[] = {IDC_RESET_FRM_FLAGS, IDC_RESET_BRIGHTNESS};
      for(i=2; i--; ){
         dword id = id_reset[i];
         ShowWindow(GetDlgItem(hwnd, id), reset[i] ? SW_SHOW : SW_HIDE);
         if(reset[i]){
            SetDlgItemText(hwnd, id, (reset[i]==sel_list.size()) ? "R" : "r");
         }
      }
   }

//----------------------------

   void InitSheet_visual(HWND hwnd, bool reset_marks_only = false){

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      if(!sel_list.size())
         return;
      int j;
      PI3D_frame frm0 = sel_list.front();
      assert(frm0->GetType()==FRAME_VISUAL);
      PI3D_visual vis0 = I3DCAST_VISUAL(frm0);
      dword vtype = vis0->GetVisualType();
      dword mat_id = vis0->GetCollisionMaterial();
      bool no_shadow = (frm0->GetFlags()&I3D_FRMF_NOSHADOW);
      bool no_col = (frm0->GetFlags()&I3D_FRMF_NOCOLLISION);
      bool shadow_receive = (frm0->GetFlags()&I3D_FRMF_SHADOW_RECEIVE);
      bool static_col = (frm0->GetFlags()&I3D_FRMF_STATIC_COLLISION);
      bool valid[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
      dword reset[2] = {0, 0};

      for(dword i=0; i<sel_list.size(); i++){
         PI3D_frame frm = sel_list[i];
         if(frm->GetType()!=FRAME_VISUAL)
            continue;
         if(i && !reset_marks_only){
            PI3D_visual vis = I3DCAST_VISUAL(frm);
            valid[0] = (valid[0] && (vtype==vis->GetVisualType()));
            bool no_shadow1 = (frm->GetFlags()&I3D_FRMF_NOSHADOW);
            valid[7] = (valid[7] && (no_shadow==no_shadow1));
            bool no_col1 = (frm->GetFlags()&I3D_FRMF_NOCOLLISION);
            valid[8] = (valid[8] && (no_col==no_col1));
            bool shadow_receive1 = (frm->GetFlags()&I3D_FRMF_SHADOW_RECEIVE);
            valid[9] = (valid[9] && (shadow_receive==shadow_receive1));
            valid[10] = (valid[10] && (vis->GetCollisionMaterial()==mat_id));
            bool static_col1 = (frm->GetFlags()&I3D_FRMF_STATIC_COLLISION);;
            valid[11] = (valid[11] && (static_col1 == static_col));
         }
                  //check what can be reset
         dword flg = e_modify->GetFrameFlags(frm);
         if(flg&E_MODIFY_FLG_FRM_FLAGS) ++reset[0];
         if(flg&E_MODIFY_FLG_COL_MAT) ++reset[1];
      }
      if(!reset_marks_only){
                              //hide all properties
         prop_controls.clear();
                     //type
         if(!valid[0]){
                     //different visual types
            SendDlgItemMessage(hwnd, IDC_COMBO_VISUAL_TYPE, CB_SETCURSEL, (dword)-1, 0);
         }else{
            for(j=SendDlgItemMessage(hwnd, IDC_COMBO_VISUAL_TYPE, CB_GETCOUNT, 0, 0); j--; ){
               if(vtype==(dword)SendDlgItemMessage(hwnd, IDC_COMBO_VISUAL_TYPE, CB_GETITEMDATA, j, 0))
                  break;
            }
            SendDlgItemMessage(hwnd, IDC_COMBO_VISUAL_TYPE, CB_SETCURSEL, j, 0);

            HWND hwnd_dlg = GetParent(hwnd);
                     //build property lists
            dword num_props = ed->GetDriver()->NumProperties(vtype);
            int sx = vis_property_ground_size[0];
            int sy = 10;
            MapDlgUnits(hwnd_dlg, NULL, &sy);
            int pos_x = 2, pos_y = 0;
            MapDlgUnits(hwnd_dlg, &pos_x, NULL);
            sx -= pos_x;
            HWND hwnd_ground = GetDlgItem(hwnd, IDC_VIS_PROP_GROUP);
            SetWindowLong(hwnd_ground, GWL_USERDATA, 0);

            for(dword pi=0; pi<num_props; pi++){
               I3D_PROPERTYTYPE ptype = ed->GetDriver()->GetPropertyType(vtype, pi);
                                 //ignore NULL
               if(ptype==I3DPROP_NULL)
                  continue;

               const char *pname = ed->GetDriver()->GetPropertyName(vtype, pi);
                              //determine state - check if all visuals are the same
               dword value = 0;
               bool same = true;
               for(dword vi=0; vi<sel_list.size(); vi++){
                  PI3D_visual vis = I3DCAST_VISUAL(sel_list[vi]);
                  assert(vis->GetType()==FRAME_VISUAL && vis->GetVisualType()==vtype);
                  if(!vi)
                     value = vis->GetProperty(pi);
                  else
                  if(same){
                     switch(ptype){
                     case I3DPROP_STRING:
                        same = !strcmp((const char*)vis->GetProperty(pi), (const char*)value);
                        break;
                     case I3DPROP_BOOL:
                     case I3DPROP_INT:
                     case I3DPROP_FLOAT:
                     case I3DPROP_ENUM:
                        same = (vis->GetProperty(pi) == value);
                        break;
                     default:
                        assert(0);
                     }
                  }
               }
                              //setup control state
               const char *ctrl_type = NULL;
               bool make_desc = true;
               dword wstyle = WS_CHILD | WS_TABSTOP | WS_VISIBLE | ES_AUTOHSCROLL;
               dword wsextyle = WS_EX_NOPARENTNOTIFY;
               int curr_sy = sy;
               int add_sy = sy;
               switch(ptype){
               case I3DPROP_BOOL:
                  ctrl_type = "BUTTON";
                  wstyle |= BS_3STATE;
                  break;
               case I3DPROP_INT:
               case I3DPROP_FLOAT:
               case I3DPROP_STRING:
                  ctrl_type = "EDIT";
                  wsextyle |= WS_EX_CLIENTEDGE;
                  add_sy = curr_sy = FloatToInt((float)curr_sy * 1.2f);
                  break;
               case I3DPROP_ENUM:
                  ctrl_type = "COMBOBOX";
                  wstyle |= CBS_DROPDOWNLIST;
                  add_sy = FloatToInt((float)curr_sy * 1.4f);
                  curr_sy = FloatToInt((float)curr_sy * 8.0f);
                  break;
               }
               if(ctrl_type){
                              //create desired control
                  int x = pos_x, sx1 = sx;
                  if(make_desc){
                     int desc_size = FloatToInt((float)sx * .6f);
                     x += desc_size;
                     sx1 -= desc_size;
                  }
                  HWND h1 = CreateWindowEx(wsextyle,
                     ctrl_type,
                     NULL,
                     wstyle,
                     x, pos_y, sx1, curr_sy,
                     hwnd_ground,
                     (HMENU)(20000 + pi),
                     NULL,
                     NULL);
                  SendMessage(h1, WM_SETFONT, SendMessage(hwnd_dlg, WM_GETFONT, 0, 0), 0);
                  SetWindowLong(h1, GWL_USERDATA, (LPARAM)ptype);

                  C_property_control *ec = new C_property_control(h1, x, pos_y);
                  prop_controls.push_back(ec);
                  ec->Release();

                  if(make_desc){
                     HWND h1 = CreateWindow(
                        "STATIC",
                        C_fstr("%i. %s . . . . . . . . . . . . .", pi, pname),
                        WS_CHILD |
                        //SS_RIGHT |
                        WS_VISIBLE,
                        pos_x, pos_y + 2, sx - sx1, add_sy,
                        hwnd_ground,
                        NULL,
                        NULL,
                        NULL);
                     SendMessage(h1, WM_SETFONT, SendMessage(hwnd_dlg, WM_GETFONT, 0, 0), 0);

                     C_property_control *ec = new C_property_control(h1, pos_x, pos_y);
                     prop_controls.push_back(ec);
                     ec->Release();
                  }

                  pos_y += add_sy;

                              //setup item
                  switch(ptype){
                  case I3DPROP_BOOL:
                     SendMessage(h1, BM_SETCHECK, !same ? BST_INDETERMINATE : value ? BST_CHECKED : BST_UNCHECKED, 0);
                     break;
                  case I3DPROP_INT:
                     if(same)
                        SendMessage(h1, WM_SETTEXT, 0, (LPARAM)(const char*)C_fstr("%i", value));
                     break;
                  case I3DPROP_FLOAT:
                     if(same)
                        SendMessage(h1, WM_SETTEXT, 0, (LPARAM)(const char*)FloatStrip(C_fstr("%f", *(float*)&value)));
                     break;
                  case I3DPROP_ENUM:
                     {
                                 //fill-in contents of box
                        dword num_items = 0;
                        for(const char *cp=pname; cp += strlen(cp) + 1, *cp; ++num_items){
                           SendMessage(h1, CB_ADDSTRING, 0, (LPARAM)cp);
                        }
                        if(same && value<num_items)
                           SendMessage(h1, CB_SETCURSEL, value, 0);
                     }
                     break;
                  case I3DPROP_STRING:
                     if(same && value)
                        SendMessage(h1, WM_SETTEXT, 0, (LPARAM)(const char*)value);
                     break;
                  default:
                     assert(0);
                  }
               }
            }
                     //now check if scrollbar is needed
            if(pos_y >= vis_property_ground_size[1]){
               int sb_size = 8;
               MapDlgUnits(hwnd_dlg, &sb_size, NULL);

               HWND hwnd_sb = CreateWindow(
                  "SCROLLBAR",
                  NULL,
                  WS_CHILD | WS_VISIBLE | SBS_VERT,
                  vis_property_ground_pos[0] + vis_property_ground_size[0],
                  vis_property_ground_pos[1],
                  10, vis_property_ground_size[1],
                  hwnd,
                  NULL,
                  NULL,
                  NULL);
               SendMessage(hwnd_sb, WM_SETFONT, SendMessage(hwnd_dlg, WM_GETFONT, 0, 0), 0);

               C_property_control *ec = new C_property_control(hwnd_sb, 0x80000000, 0x80000000);
               prop_controls.push_back(ec);
               ec->Release();

               EnableWindow(hwnd_sb, true);
               vis_prop_scroll_max_y = pos_y - vis_property_ground_size[1];
                     //retain previous scroll position
               vis_prop_scroll_y = Min(vis_prop_scroll_y, vis_prop_scroll_max_y);
               ScrollVisualProperties();
               SCROLLINFO si;
               si.cbSize = sizeof(si);
               si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
               si.nMin = 0;
               si.nMax = pos_y;
               si.nPage = vis_property_ground_size[1];
               si.nPos = vis_prop_scroll_y;
               SendMessage(hwnd_sb, SBM_SETSCROLLINFO, true, (LPARAM)&si);
            }else{
               vis_prop_scroll_y = 0;
            }
            SetWindowLong(hwnd_ground, GWL_USERDATA, (LPARAM)this);
         }
                     //set 'no shadow' checkbox
         CheckDlgButton(hwnd, IDC_CHECK_NO_SHADOW, !valid[7] ? BST_INDETERMINATE : no_shadow ? BST_CHECKED : BST_UNCHECKED);
                     //set 'no collision' checkbox
         CheckDlgButton(hwnd, IDC_CHECK_NO_COLL, !valid[8] ? BST_INDETERMINATE : no_col ? BST_CHECKED : BST_UNCHECKED);
                     //set 'receive shadow' checkbox
         CheckDlgButton(hwnd, IDC_CHECK_SHADOW_REC, !valid[9] ? BST_INDETERMINATE : shadow_receive ? BST_CHECKED : BST_UNCHECKED);
                     //set 'static collision' checkbox
         CheckDlgButton(hwnd, IDC_CHECK_VOLUME_STATIC, !valid[11] ? BST_INDETERMINATE : static_col ? BST_CHECKED : BST_UNCHECKED);
                     //material id
         EnableWindow(GetDlgItem(hwnd, IDC_COMBO_COL_MAT), static_col);

         if(valid[10]){
            int i = 0;
            map<dword, C_str>::const_iterator it;
            for(it=mat_table.begin(); it!=mat_table.end(); it++, i++)
               if((*it).first==mat_id) break;
            if(it==mat_table.end()) i = -1;
            SendDlgItemMessage(hwnd, IDC_COMBO_COL_MAT, CB_SETCURSEL, i, 0);
         }else{
            SendDlgItemMessage(hwnd, IDC_COMBO_COL_MAT, CB_SETCURSEL, (dword)-1, 0);
         }
      }
      static const word id_reset[] = {IDC_RESET_FRM_FLAGS, IDC_RESET_COL_MAT};
      for(i=2; i--; ){
         dword id = id_reset[i];
         ShowWindow(GetDlgItem(hwnd, id), reset[i] ? SW_SHOW : SW_HIDE);
         if(reset[i]){
            SetDlgItemText(hwnd, id, (reset[i]==sel_list.size()) ? "R" : "r");
         }
      }
   }

//----------------------------

   static const char *GetResetControlInfo(dword ctrl_id, dword &modify_flags){

      const char *name = NULL;
      switch(ctrl_id){
      case IDC_RESET_POS: name = "position"; modify_flags = E_MODIFY_FLG_POSITION; break;
      case IDC_RESET_ROT: name = "roation"; modify_flags = E_MODIFY_FLG_ROTATION; break;
      case IDC_RESET_SCL: name = "scale"; modify_flags = E_MODIFY_FLG_SCALE; break;
      case IDC_RESET_LINK: name = "link"; modify_flags = E_MODIFY_FLG_LINK; break;
      case IDC_RESET_VOL_TYPE: name = "volume type"; modify_flags = E_MODIFY_FLG_VOLUME_TYPE; break;
      case IDC_RESET_COL_MAT: name = "collision material"; modify_flags = E_MODIFY_FLG_COL_MAT; break;
      case IDC_RESET_NU_SCL: name = "non-uniform scale"; modify_flags = E_MODIFY_FLG_NU_SCALE; break;
      case IDC_RESET_FRM_FLAGS: name = "frame flags"; modify_flags = E_MODIFY_FLG_FRM_FLAGS; break;
      case IDC_RESET_LIGHT: name = "light parameters"; modify_flags = E_MODIFY_FLG_LIGHT; break;
      case IDC_RESET_BRIGHTNESS: name = "brightness"; modify_flags = E_MODIFY_FLG_BRIGHTNESS; break;
      }
      return name;
   }

//----------------------------

   void InitSheet(dword index, bool reset_flags_only = false){

                              //no initialization of user sheets
      if(index >= SHEET_USER)
         return;

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      HWND hwnd = hwnd_sheet[index];

                              //set temporaly user data to NULL, so that we're not called recursively
      SetWindowLong(hwnd, GWL_USERDATA, 0);

      dword i;

      switch(index){
      case SHEET_FRAME:
         InitSheet_frame(hwnd, reset_flags_only);
         break;

      case SHEET_LIGHT:
         InitSheet_light(hwnd, reset_flags_only);
         break;

      case SHEET_SOUND:
         InitSheet_sound(hwnd, reset_flags_only);
         break;

      case SHEET_MODEL:
         InitSheet_model(hwnd, reset_flags_only);
         break;

      case SHEET_LSECTORS:
      case SHEET_SSECTORS:
         {
            if(reset_flags_only) break;
            HWND hwnd_lc = NULL;
            I3D_FRAME_TYPE ftype;

            switch(index){
            case SHEET_LSECTORS:
               hwnd_lc = GetDlgItem(hwnd, IDC_LIST_LSECTORS);
               ftype = FRAME_LIGHT;
               break;
            case SHEET_SSECTORS:
               hwnd_lc = GetDlgItem(hwnd, IDC_LIST_SSECTORS);
               ftype = FRAME_SOUND;
               break;
            default: assert(0); ftype = FRAME_NULL;
            }
            assert(hwnd_lc);

            SendMessage(hwnd_lc, LVM_DELETEALLITEMS, 0, 0);

                        //add all sectors
            struct S_hlp{
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                  C_vector<PI3D_sector> *sect_list = (C_vector<PI3D_sector>*)c;
                  sect_list->push_back(I3DCAST_SECTOR(frm));
                  return I3DENUMRET_OK;
               }
            };
            C_vector<PI3D_sector> sect_list;
            sect_list.push_back(ed->GetScene()->GetPrimarySector());
            sect_list.push_back(ed->GetScene()->GetBackdropSector());

            ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&sect_list, ENUMF_SECTOR);
                        //use list: -1 = uninit, 0 = not in, 1 = in, 2 = indeterminate
            C_vector<int> use_list(sect_list.size(), -1);

            for(i=sel_list.size(); i--; ){
               PI3D_frame frm = sel_list[i];
               if(frm->GetType()==ftype){
                  for(int j=sect_list.size(); j--; ){
                     PI3D_sector sct = sect_list[j];
                     bool is_in = false;

                     switch(ftype){
                     case FRAME_LIGHT:
                        {
                           PI3D_light lp = I3DCAST_LIGHT(frm);
                           is_in = (1 == count(lp->GetLightSectors(), lp->GetLightSectors()+lp->NumLightSectors(), sct));
                        }
                        break;
                     case FRAME_SOUND:
                        {
                           PI3D_sound sp = I3DCAST_SOUND(frm);
                           is_in = (1 == count(sp->GetSoundSectors(), sp->GetSoundSectors()+sp->NumSoundSectors(), sct));
                        }
                        break;
                     default: assert(0);
                     }
                     if(is_in){
                        use_list[j] = (use_list[j]==-1 || use_list[j]==1) ? 1 : 2;
                     }else{
                        use_list[j] = (use_list[j]==-1 || use_list[j]==0) ? 0 : 2;
                     }
                  }
               }
            }

            LVITEM li;
            memset(&li, 0, sizeof(li));
            li.stateMask = LVIS_STATEIMAGEMASK;

            for(i=0; i<sect_list.size(); i++){
               PI3D_sector sct = sect_list[i];
               li.pszText = (char*)(const char*)sct->GetName();
               li.mask = LVIF_TEXT | LVIF_PARAM;
               li.lParam = (LPARAM)sct;
               int indx = SendMessage(hwnd_lc, LVM_INSERTITEM, 0, (LPARAM)&li);

               li.mask = LVIF_STATE;
               li.state = INDEXTOSTATEIMAGEMASK(1 + use_list[i]);
               SendMessage(hwnd_lc, LVM_SETITEMSTATE, indx, (LPARAM)&li);
            }
         }
         break;

      case SHEET_VOLUME:
         InitSheet_volume(hwnd, reset_flags_only);
         break;

      case SHEET_VISUAL:
         InitSheet_visual(hwnd, reset_flags_only);
         break;

      case SHEET_SECTOR:
         {
            if(reset_flags_only) break;
            if(!sel_list.size())
               break;
            PI3D_frame frm0 = sel_list.front();
            if(frm0->GetType()!=FRAME_SECTOR)
               break;
            PI3D_sector sct0 = I3DCAST_SECTOR(frm0);
            dword env_id = sct0->GetEnvironmentID();
            float temp = sct0->GetTemperature();
            bool valid[2] = {1, 1};

            for(dword i=1; i<sel_list.size(); i++){
               PI3D_frame frm = sel_list[i];
               if(frm->GetType()!=FRAME_SECTOR)
                  continue;
               PI3D_sector sct = I3DCAST_SECTOR(frm);
               valid[0] = (valid[0] && (env_id==sct->GetEnvironmentID()));
               valid[1] = (valid[1] && (temp==sct->GetTemperature()));
            }
            if(valid[0]){
               int i = 0;
               map<dword, C_str>::const_iterator it;
               for(it=env_table.begin(); it!=env_table.end(); it++, i++)
                  if((*it).first==env_id)
                     break;
               if(it==env_table.end()) i = -1;
               SendDlgItemMessage(hwnd, IDC_COMBO_SCT_ENV, CB_SETCURSEL, i, 0);
            }else{
               SendDlgItemMessage(hwnd, IDC_COMBO_SCT_ENV, CB_SETCURSEL, (dword)-1, 0);
            }
            if(valid[1]){
               SetDlgItemText(hwnd, IDC_EDIT_TEMPERATURE, FloatStrip(C_fstr("%f", temp)));
            }else{
               SetDlgItemText(hwnd, IDC_EDIT_TEMPERATURE, "");
            }
         }
         break;
      }
      SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)this);
   }

//----------------------------

   static BOOL CALLBACK dlgSheet_Thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
      if(uMsg==WM_INITDIALOG){
         S_sheet_init *si = (S_sheet_init*)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)si->ep);
      }
      C_edit_Properties_imp *ep = (C_edit_Properties_imp*)GetWindowLong(hwnd, GWL_USERDATA);
      if(!ep)
         return 0;
      return ep->dlgSheet(hwnd, uMsg, wParam, lParam);
   }

//----------------------------

   BOOL dlgSheet(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            S_sheet_init *si = (S_sheet_init*)lParam;
            switch(si->sheet_index){

            case SHEET_LIGHT:
               {
                  HWND hwnd_t = GetDlgItem(hwnd, IDC_COMBO_LIGHT_TYPE);
                  for(int i=0; light_type_list[i].name; i++){
                     int indx = SendMessage(hwnd_t, CB_ADDSTRING, 0, (LPARAM)light_type_list[i].name);
                     SendMessage(hwnd_t, CB_SETITEMDATA, indx, (LPARAM)light_type_list[i].lt);
                  }
               }
               break;

            case SHEET_SOUND:
               {
                  HWND hwnd_t = GetDlgItem(hwnd, IDC_COMBO_SOUND_TYPE);
                  for(int i=0; sound_type_list[i].name; i++){
                     int indx = SendMessage(hwnd_t, CB_ADDSTRING, 0, (LPARAM)sound_type_list[i].name);
                     SendMessage(hwnd_t, CB_SETITEMDATA, indx, (LPARAM)sound_type_list[i].st);
                  }
               }
               break;

            case SHEET_LSECTORS:
            case SHEET_SSECTORS:
               {
                  HWND hwnd_lc = NULL;
                  switch(si->sheet_index){
                  case 6:  hwnd_lc = GetDlgItem(hwnd, IDC_LIST_LSECTORS); break;
                  case 11: hwnd_lc = GetDlgItem(hwnd, IDC_LIST_SSECTORS); break;
                  }
                  SendMessage(hwnd_lc, LVM_SETEXTENDEDLISTVIEWSTYLE, 
                     LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT, 
                     LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

                  RECT rc;
                  GetClientRect(hwnd_lc, &rc);

                  LVCOLUMN lvc;
                  memset(&lvc, 0, sizeof(lvc));
                  lvc.mask = LVCF_WIDTH;
                  lvc.cx = (rc.right - rc.left);
                  SendMessage(hwnd_lc, LVM_INSERTCOLUMN, 0, (LPARAM)&lvc);
               }
               break;

            case SHEET_VOLUME:
               {
                  HWND hwnd_t = GetDlgItem(hwnd, IDC_COMBO_VOLUME_TYPE);
                  for(int i=0; volume_type_list[i].name; i++){
                     int indx = SendMessage(hwnd_t, CB_ADDSTRING, 0, (LPARAM)volume_type_list[i].name);
                     SendMessage(hwnd_t, CB_SETITEMDATA, indx, (LPARAM)volume_type_list[i].vt);
                  }
               }
               break;

            case SHEET_VISUAL:
               {
                  struct S_hlp{
                     static I3DENUMRET I3DAPI cbEnum(dword visual_type,
                        const char *friendly_name, dword context){

                        int i = SendDlgItemMessage((HWND)context, IDC_COMBO_VISUAL_TYPE,
                           CB_ADDSTRING, 0, (LPARAM)friendly_name);
                        SendDlgItemMessage((HWND)context, IDC_COMBO_VISUAL_TYPE,
                           CB_SETITEMDATA, i, (LPARAM)visual_type);
                        return I3DENUMRET_OK;
                     }
                  };
                  si->ep->ed->GetDriver()->EnumVisualTypes(S_hlp::cbEnum, (dword)hwnd);
               }
               break;

            }
            SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)si->ep);
         }
         return 1;

      case WM_HELP:
         {
            LPHELPINFO hi = (LPHELPINFO)lParam;
            dword ctrl_id = hi->iCtrlId;
            for(dword i=0; help_texts[i].ctrl_id; i++){
               if(help_texts[i].ctrl_id==ctrl_id){
                  DisplayHelp(hwnd, ctrl_id, help_texts);
                  return 1;
               }
            }
                              //check if it's come of reset buttons
            dword dummy;
            const char *reset_name = GetResetControlInfo(ctrl_id, dummy);
            if(reset_name){
               DisplayHelp(hwnd, ctrl_id, C_xstr("Reset % of selected frames to original values.") %reset_name);
               return 1;
            }

            if(curr_sheet_index==SHEET_VISUAL && ctrl_id){
                              //this may be one of visual property
               HWND hwnd_ctrl = (HWND)hi->hItemHandle;
                              //make sure this is one of expected controls
               for(int i=prop_controls.size(); i--; ){
                  if(prop_controls[i]->hwnd==hwnd_ctrl)
                     break;
               }
               assert(i!=-1);
               dword prop_index = ctrl_id - 20000;
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               assert(sel_list.size());
               if(sel_list.size()){
                  PI3D_frame frm = sel_list.front();
                  assert(frm->GetType()==FRAME_VISUAL);
                  if(frm->GetType()==FRAME_VISUAL){
                     PI3D_visual vis = I3DCAST_VISUAL(frm);
                     dword vis_t = vis->GetVisualType();
                     const char *help_text = ed->GetDriver()->GetPropertyHelp(vis_t, prop_index);
                     if(help_text){
                        RECT rc;
                        GetWindowRect(GetDlgItem(hwnd, ctrl_id), &rc);
                        POINT pt;
                        pt.x = rc.left;
                        pt.y = rc.bottom;

                        ScreenToClient(hwnd, &pt);
                        pt.x -= 40;
                        pt.y += 2;

                        OsDisplayHelpWindow(help_text, hwnd, pt.x, pt.y, 150);
                     }
                  }
               }
            }
            return 1;
         }
         break;

      case WM_DRAWITEM:
         switch(wParam){
         case IDC_BUTTON_LIGHT_COLOR:
         //case IDC_BUTTON_LIGHT_SPECULAR:
            {                 //draw colored button
               LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)lParam;
               HBRUSH hbr = CreateSolidBrush(GetWindowLong(GetDlgItem(hwnd, wParam), GWL_USERDATA));
               SelectObject(di->hDC, hbr);
               Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
               DeleteObject(hbr);
            }
            break;
         }
         break;

      case WM_VSCROLL:
         {
            HWND hwnd_sb = (HWND)lParam;
                              //we have only one SB - for visuals' props
            assert(prop_controls.size() && prop_controls.back()->hwnd==hwnd_sb);
            int sb_pos = vis_prop_scroll_y;
            switch(LOWORD(wParam)){
            case SB_BOTTOM: sb_pos = vis_prop_scroll_max_y; break;
            case SB_TOP: sb_pos = 0;
            case SB_LINEDOWN: sb_pos = Min(sb_pos+30, vis_prop_scroll_max_y); break;
            case SB_LINEUP: sb_pos = Max(sb_pos-30, 0); break;
            case SB_PAGEDOWN: sb_pos = Min(sb_pos+300, vis_prop_scroll_max_y); break;
            case SB_PAGEUP: sb_pos = Max(sb_pos-300, 0); break;
            case SB_THUMBTRACK: sb_pos = HIWORD(wParam); break;
            }
            if(sb_pos != vis_prop_scroll_y){
               vis_prop_scroll_y = sb_pos;
               ScrollVisualProperties();
               SendMessage(hwnd_sb, SBM_SETPOS, sb_pos, true);
               RedrawWindow(GetParent(hwnd_sb), NULL, NULL, RDW_INVALIDATE);
               UpdateWindow(GetParent(hwnd_sb));
            }
            return 0;
         }
         break;

      case WM_NOTIFY:
         switch(wParam){
         case IDC_LIST_LSECTORS:
         case IDC_LIST_SSECTORS:
            {
               I3D_FRAME_TYPE ftype;
               dword modify_flags;
               switch(wParam){
               case IDC_LIST_LSECTORS:
                  ftype = FRAME_LIGHT;
                  modify_flags = E_MODIFY_FLG_LIGHT;
                  break;
               case IDC_LIST_SSECTORS:
                  ftype = FRAME_SOUND;
                  modify_flags = E_MODIFY_FLG_SOUND;
                  break;
               default: assert(0); ftype = FRAME_NULL; modify_flags = 0;
               }

               LPNMHDR hdr = (LPNMHDR)lParam;
               switch(hdr->code){
               case LVN_ITEMCHANGED:
                  {
                     if(!ed->CanModify()) break;
                     in_sheet_modify = true;
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                     LPNMLISTVIEW lvi = (LPNMLISTVIEW)hdr;
                     if(lvi->uChanged&LVIF_STATE){
                        bool was_on = ((lvi->uOldState&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2));
                        bool on = ((lvi->uNewState&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2));
                        if(was_on!=on){
                           PI3D_sector sct = (PI3D_sector)lvi->lParam;
                           e_slct->FlashFrame(sct);

                           for(int i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()==ftype){
                                 switch(ftype){
                                 case FRAME_LIGHT:
                                    {
                                       C_chunk &ck = e_undo->Begin(this, was_on ? UNDO_LIGHT_SECTOR_ADD : UNDO_LIGHT_SECTOR_REM, frm);
                                       ck.Write(sct->GetName());
                                       e_undo->End();

                                       PI3D_light lp = I3DCAST_LIGHT(frm);
                                       if(on) sct->AddLight(lp);
                                       else sct->RemoveLight(lp);
                                    }
                                    break;
                                 case FRAME_SOUND:
                                    {
                                       C_chunk &ck = e_undo->Begin(this, was_on ? UNDO_SOUND_SECTOR_ADD : UNDO_SOUND_SECTOR_REM, frm);
                                       ck.Write(sct->GetName());
                                       e_undo->End();

                                       PI3D_sound sp = I3DCAST_SOUND(frm);
                                       if(on) sct->AddSound(sp);
                                       else sct->RemoveSound(sp);
                                    }
                                    break;
                                 }
                                 e_modify->AddFrameFlags(frm, modify_flags);

                                 ed->SetModified();
                              }
                           }
                        }
                     }
                     in_sheet_modify = false;
                  }
                  break;

               case NM_DBLCLK:
                  {
                     LPNMLISTVIEW nmlv = (LPNMLISTVIEW)lParam;
                     LVITEM lvi;
                     lvi.mask = LVIF_STATE;
                     lvi.stateMask = LVIS_STATEIMAGEMASK;
                     lvi.iItem = nmlv->iItem;
                     lvi.iSubItem = nmlv->iSubItem;
                     SendDlgItemMessage(hwnd, wParam, LVM_GETITEM, 0, (LPARAM)&lvi);
                     bool was_on = ((lvi.state&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2));
                     lvi.state &= ~LVIS_STATEIMAGEMASK; 
                     lvi.state = INDEXTOSTATEIMAGEMASK(was_on ? 1 : 2);
                     SendDlgItemMessage(hwnd, wParam, LVM_SETITEM, 0, (LPARAM)&lvi);
                  }
                  break;
               }
            }
            break;
         }
         break;
      
      case WM_COMMAND:
         switch(HIWORD(wParam)){
         case EN_CHANGE:
            {
                              //get current selection
               if(!ed->CanModify()) break;
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               if(!sel_list.size()) break;
               in_sheet_modify = true;

               dword ctl_id = LOWORD(wParam);

               switch(ctl_id){
               case IDC_EDIT_FRM_NAME:
                  {
                     PI3D_frame frm0 = sel_list.front();
                     C_str new_name = GetDlgItemText(hwnd, ctl_id);
                     if(new_name != frm0->GetName() && new_name.Size()){
                        if(!IsNameInScene(ed->GetScene(), new_name))
                           RenameFrame(frm0, new_name);
                        else{
                           /*
                              //name already exists - select last n chars
                           C_str s = new_name;
                           for(int i=s.Size(); i--; ){
                              s[(dword)i] = 0;
                              if(!IsNameInScene(ed->GetScene(), s))
                                 break;
                           }
                           SendDlgItemMessage(hwnd, IDC_EDIT_FRM_NAME, EM_SETSEL, i, new_name.Size());
                           */
                           //SendDlgItemMessage(hwnd, IDC_EDIT_FRM_NAME, EM_SETTEXTMODE, TM_RICHTEXT, 0);
                           //SendDlgItemMessage(hwnd, IDC_EDIT_FRM_NAME, EM_SETBKGNDCOLOR, false, 0xff0000);
                        }
                     }
                  }
                  break;

               case IDC_EDIT_FRM_POS_X: case IDC_EDIT_FRM_POS_Y: case IDC_EDIT_FRM_POS_Z:
               case IDC_EDIT_FRM_ROT_X: case IDC_EDIT_FRM_ROT_Y: case IDC_EDIT_FRM_ROT_Z: case IDC_EDIT_FRM_ROT_A:
               case IDC_EDIT_FRM_SCALE_U:
               case IDC_EDIT_VOL_NUSCALE_X: case IDC_EDIT_VOL_NUSCALE_Y: case IDC_EDIT_VOL_NUSCALE_Z:
                  {
                     C_str num = GetDlgItemText(hwnd, ctl_id);
                     float f;
                     int i;
                     i = sscanf(num, "%f", &f);
                     if(i==1){
                              //modify selection

                        dword undo_action = 0;
                        dword mf = 0;
                        switch(ctl_id){
                        case IDC_EDIT_FRM_POS_X: case IDC_EDIT_FRM_POS_Y: case IDC_EDIT_FRM_POS_Z:
                           undo_action = UNDO_TRANSLATE; mf = E_MODIFY_FLG_POSITION;
                           break;
                        case IDC_EDIT_FRM_ROT_X: case IDC_EDIT_FRM_ROT_Y: case IDC_EDIT_FRM_ROT_Z: case IDC_EDIT_FRM_ROT_A:
                           undo_action = UNDO_ROTATE; mf = E_MODIFY_FLG_ROTATION;
                           break;
                        case IDC_EDIT_FRM_SCALE_U:
                           undo_action = UNDO_SCALE_U; mf = E_MODIFY_FLG_SCALE;
                           break;

                        case IDC_EDIT_VOL_NUSCALE_X: case IDC_EDIT_VOL_NUSCALE_Y: case IDC_EDIT_VOL_NUSCALE_Z:
                           undo_action = UNDO_SCALE_NU; mf = E_MODIFY_FLG_NU_SCALE;
                           break;
                        default: assert(0);
                        }

                        bool undo_on = !e_undo->IsTopEntry(this, undo_action);

                        for(dword ii=0; ii<sel_list.size(); ii++){
                           PI3D_frame frm = sel_list[ii];
                                          //check if not locked
                           bool pos_locked = (e_modify->GetLockFlags(frm)&mf);
                           if(pos_locked){
                              C_fstr msg("Frame '%s' locked, cannot change pos.", (const char*)frm->GetName());
                              ed->Message(msg);
                              continue;
                           }

                           e_modify->AddFrameFlags(frm, mf);

                           C_chunk *ck_undo = NULL;
                           if(undo_on){
                              ck_undo = &e_undo->Begin(this, undo_action, frm);
                              //ck_undo->Write(frm->GetName());
                           }

                           switch(ctl_id){
                           case IDC_EDIT_FRM_POS_X: case IDC_EDIT_FRM_POS_Y: case IDC_EDIT_FRM_POS_Z:
                              {
                                 S_vector pos = frm->GetPos();
                                             //save undo
                                 if(undo_on)
                                    ck_undo->Write(pos);
                                             //modify position
                                 switch(ctl_id){
                                 case IDC_EDIT_FRM_POS_X: pos.x = f; break;
                                 case IDC_EDIT_FRM_POS_Y: pos.y = f; break;
                                 default: pos.z = f;
                                 }
                                 frm->SetPos(pos);
                              }
                              break;

                           case IDC_EDIT_FRM_ROT_X: case IDC_EDIT_FRM_ROT_Y: case IDC_EDIT_FRM_ROT_Z: case IDC_EDIT_FRM_ROT_A:
                              {
                                                //get all values
                                 C_str num;
                                 S_vector axis(0, 0, 1);
                                 float angle(0);

                                 num = GetDlgItemText(hwnd, IDC_EDIT_FRM_ROT_X);
                                 sscanf(num, "%f", &axis.x);
                                 num = GetDlgItemText(hwnd, IDC_EDIT_FRM_ROT_Y);
                                 sscanf(num, "%f", &axis.y);
                                 num = GetDlgItemText(hwnd, IDC_EDIT_FRM_ROT_Z);
                                 sscanf(num, "%f", &axis.z);
                                 num = GetDlgItemText(hwnd, IDC_EDIT_FRM_ROT_A);
                                 sscanf(num, "%f", &angle);

                                 angle *= PI/180.0f;

                                 S_quat rot;
                                 rot.Make(axis, angle);
                                             //save undo info
                                 if(undo_on)
                                    ck_undo->Write(frm->GetRot());
                                             //modify rotation
                                 frm->SetRot(rot);
                              }
                              break;

                           case IDC_EDIT_VOL_NUSCALE_X: case IDC_EDIT_VOL_NUSCALE_Y: case IDC_EDIT_VOL_NUSCALE_Z:
                              if(frm->GetType()!=FRAME_VOLUME)
                                 break;
                                          //flow...
                           case IDC_EDIT_FRM_SCALE_U:
                              e_modify->AddFrameFlags(frm, mf);

                              if(ctl_id==IDC_EDIT_FRM_SCALE_U){
                                 if(undo_on)
                                    ck_undo->Write(frm->GetScale());
                                 frm->SetScale(f);
                              }else{
                                 switch(frm->GetType()){
                                 case FRAME_VOLUME: 
                                    S_vector scl;
                                    scl = I3DCAST_VOLUME(frm)->GetNUScale();
                                    if(undo_on)
                                       ck_undo->Write(scl);
                                    switch(ctl_id){
                                    case IDC_EDIT_VOL_NUSCALE_X: scl.x = f; break;
                                    case IDC_EDIT_VOL_NUSCALE_Y: scl.y = f; break;
                                    default: scl.z = f; break;
                                    }
                                    I3DCAST_VOLUME(frm)->SetNUScale(scl); 
                                    break;
                                 }
                              }
                              break;
                           default: assert(0);
                           }
                           if(undo_on)
                              e_undo->End();
                           ed->SetModified();
                        }
                        InitSheet(curr_sheet_index, true);
                     }
                  }
                  break;

               case IDC_EDIT_LIGHT_POWER: case IDC_EDIT_LIGHT_RANGE_N: case IDC_EDIT_LIGHT_RANGE_F:
               case IDC_EDIT_LIGHT_CONE_IN: case IDC_EDIT_LIGHT_CONE_OUT:
                  {
                     C_str num = GetDlgItemText(hwnd, ctl_id);
                     float f;
                     int i;
                     i = sscanf(num, "%f", &f);
                     if(i==1){
                        for(i=sel_list.size(); i--; ){
                           PI3D_frame frm = sel_list[i];
                           if(frm->GetType()!=FRAME_LIGHT) continue;
                           PI3D_light lp = I3DCAST_LIGHT(frm);
                           e_modify->AddFrameFlags(frm, E_MODIFY_FLG_LIGHT);

                           switch(ctl_id){
                           case IDC_EDIT_LIGHT_POWER:
                              {
                                 C_chunk &ck = e_undo->Begin(this, UNDO_LIGHT_POWER, lp);
                                 ck.Write(lp->GetPower());
                                 e_undo->End();

                                 lp->SetPower(f);
                              }
                              break;
                           case IDC_EDIT_LIGHT_RANGE_N:
                           case IDC_EDIT_LIGHT_RANGE_F:
                              {
                                 pair<float, float> r;
                                 lp->GetRange(r.first, r.second);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_LIGHT_RANGE, lp);
                                 ck.Write(r);
                                 e_undo->End();

                                 if(ctl_id==IDC_EDIT_LIGHT_RANGE_N)
                                    lp->SetRange(f, r.second);
                                 else
                                    lp->SetRange(r.first, f);
                              }
                              break;

                           case IDC_EDIT_LIGHT_CONE_IN:
                           case IDC_EDIT_LIGHT_CONE_OUT:
                              {
                                 f = f*PI/180.0f;

                                 pair<float, float> r;
                                 lp->GetCone(r.first, r.second);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_LIGHT_CONE, lp);
                                 ck.Write(r);
                                 e_undo->End();

                                 if(ctl_id==IDC_EDIT_LIGHT_CONE_IN)
                                    lp->SetCone(f, r.second);
                                 else
                                    lp->SetCone(r.first, f);
                              }
                              break;
                           }

                           ed->SetModified();
                        }
                     }
                  }
                  break;

               case IDC_EDIT_SOUND_RANGE_N: case IDC_EDIT_SOUND_RANGE_F:
               case IDC_EDIT_SOUND_CONE_IN: case IDC_EDIT_SOUND_CONE_OUT:
               case IDC_EDIT_SOUND_VOL: case IDC_EDIT_SOUND_VOL_OUT:
                  {
                     C_str num = GetDlgItemText(hwnd, ctl_id);
                     float f;
                     int i;
                     i = sscanf(num, "%f", &f);
                     if(i==1){
                        for(i=sel_list.size(); i--; ){
                           PI3D_frame frm = sel_list[i];
                           if(frm->GetType()!=FRAME_SOUND) continue;
                           PI3D_sound sp = I3DCAST_SOUND(frm);
                           switch(ctl_id){
                           case IDC_EDIT_SOUND_VOL:
                              {
                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_VOLUME, sp);
                                 ck.Write(sp->GetVolume());
                                 e_undo->End();

                                 sp->SetVolume(f);
                              }
                              break;
                           case IDC_EDIT_SOUND_VOL_OUT:
                              {
                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_VOLUME_OUT, sp);
                                 ck.Write(sp->GetOutVol());
                                 e_undo->End();

                                 sp->SetOutVol(f);
                              }
                              break;
                           case IDC_EDIT_SOUND_CONE_IN:
                           case IDC_EDIT_SOUND_CONE_OUT:
                              {
                                 pair<float, float> p;
                                 sp->GetCone(p.first, p.second);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_CONE, sp);
                                 ck.Write(p);
                                 e_undo->End();

                                 if(ctl_id==IDC_EDIT_SOUND_CONE_IN)
                                    sp->SetCone(f*PI/180.0f, p.second);
                                 else
                                    sp->SetCone(p.first, f*PI/180.0f);
                              }
                              break;
                           case IDC_EDIT_SOUND_RANGE_N:
                           case IDC_EDIT_SOUND_RANGE_F:
                              {
                                 pair<float, float> p;
                                 sp->GetRange(p.first, p.second);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_RANGE, sp);
                                 ck.Write(p);
                                 e_undo->End();

                                 if(ctl_id==IDC_EDIT_SOUND_RANGE_N)
                                    sp->SetRange(f, p.second);
                                 else
                                    sp->SetRange(p.first,f);
                              }
                              break;
                           default: assert(0);
                           }

                           e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SOUND);

                           ed->SetModified();
                        }
                     }
                  }
                  break;

               case IDC_EDIT_MODEL_BRIGHTNESS:
               case IDC_EDIT_TEMPERATURE:
                  {
                     C_str num = GetDlgItemText(hwnd, ctl_id);
                     float f;
                     int i;
                     i = sscanf(num, "%f", &f);
                     if(i==1){
                        for(i=sel_list.size(); i--; ){
                           PI3D_frame frm = sel_list[i];
                           switch(frm->GetType()){
                           case FRAME_MODEL:
                              if(ctl_id==IDC_EDIT_MODEL_BRIGHTNESS){
                                 PI3D_model mod = I3DCAST_MODEL(frm);
                                 float curr = GetModelBrightness(mod);
                                 if(curr==f)
                                    break;

                                 e_modify->AddFrameFlags(frm, E_MODIFY_FLG_BRIGHTNESS);
                                 if(!e_undo->IsTopEntry(this, UNDO_SET_BRIGHTNESS)){
                                                //save undo
                                    C_chunk &ck_undo = e_undo->Begin(this, UNDO_SET_BRIGHTNESS, frm);
                                    //ck_undo.Write(frm->GetName());
                                    ck_undo.Write(curr);
                                    e_undo->End();
                                 }
                                 SetModelBrightness(mod, f);
                                 ed->SetModified();
                              }
                              break;
                           case FRAME_SECTOR:
                              if(ctl_id==IDC_EDIT_TEMPERATURE){
                                 PI3D_sector sct = I3DCAST_SECTOR(frm);
                                 float curr = sct->GetTemperature();
                                 if(curr==f)
                                    break;

                                 e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SECTOR);
                                 if(!e_undo->IsTopEntry(this, UNDO_SET_TEMPERATURE)){
                                                //save undo
                                    C_chunk &ck_undo = e_undo->Begin(this, UNDO_SET_TEMPERATURE, frm);
                                    //ck_undo.Write(frm->GetName());
                                    ck_undo.Write(sct->GetTemperature());
                                    e_undo->End();
                                 }
                                 sct->SetTemperature(f);
                                 ed->SetModified();
                              }
                              break;
                           default: assert(0);
                           }
                        }
                        InitSheet(curr_sheet_index, true);
                     }
                  }
                  break;

               default:
                  {
                     if(curr_sheet_index!=SHEET_VISUAL)
                        break;
                              //this may be one of visual property
                     int id = ctl_id;
                     HWND hwnd_ctrl = (HWND)lParam;
                              //make sure this is one of expected controls
                     for(int i=prop_controls.size(); i--; ){
                        if(prop_controls[i]->hwnd==hwnd_ctrl)
                           break;
                     }
                     assert(i!=-1);
                     int prop_index = id - 20000;

                     C_str str = GetDlgItemText(hwnd, id);
                     dword value = 0;
                              //get type of fiels
                     I3D_PROPERTYTYPE ptype = (I3D_PROPERTYTYPE)GetWindowLong(hwnd_ctrl, GWL_USERDATA);
                     switch(ptype){
                     case I3DPROP_INT:
                        sscanf(str, "%i", &value);
                        break;
                     case I3DPROP_FLOAT:
                        sscanf(str, "%f", &value);
                        break;
                     case I3DPROP_STRING:
                        value = (dword)(const char*)str;
                        break;
                     default:
                        assert(0);
                     }
                     ModifyVisualsProperty(sel_list, prop_index, value);
                  }
               }
               in_sheet_modify = false;
            }
            break;

         case CBN_SELCHANGE:
            {
               if(!ed->CanModify()) break;
               in_sheet_modify = true;
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

               dword ctl_id = LOWORD(wParam);

               switch(ctl_id){
               case IDC_COMBO_LIGHT_TYPE:
               case IDC_COMBO_SOUND_TYPE:
               case IDC_COMBO_VOLUME_TYPE:
               case IDC_COMBO_VISUAL_TYPE:
               case IDC_COMBO_COL_MAT:
               case IDC_COMBO_SCT_ENV:
                  {
                     switch(ctl_id){

                     case IDC_COMBO_LIGHT_TYPE:
                        {
                           int lti = SendDlgItemMessage(hwnd, ctl_id, CB_GETCURSEL, 0, 0);
                           assert(lti!=-1);
                           I3D_LIGHTTYPE ltype = (I3D_LIGHTTYPE)SendDlgItemMessage(hwnd, ctl_id, CB_GETITEMDATA, lti, 0);
                           for(int i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()!=FRAME_LIGHT) continue;
                              PI3D_light lp = I3DCAST_LIGHT(frm);

                              e_modify->AddFrameFlags(frm, E_MODIFY_FLG_LIGHT);

                              C_chunk &ck = e_undo->Begin(this, UNDO_LIGHT_TYPE, lp);
                              ck.Write((dword)lp->GetLightType());
                              e_undo->End();

                              lp->SetLightType(ltype);

                              ed->SetModified();
                           }
                           InitSheet(SHEET_LIGHT);
                        }
                        break;

                     case IDC_COMBO_SOUND_TYPE:
                        {
                           int sti = SendDlgItemMessage(hwnd, ctl_id, CB_GETCURSEL, 0, 0);
                           assert(sti!=-1);
                           I3D_SOUNDTYPE stype = (I3D_SOUNDTYPE)SendDlgItemMessage(hwnd, ctl_id, CB_GETITEMDATA, sti, 0);

                           for(int i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()!=FRAME_SOUND) continue;
                              PI3D_sound sp = I3DCAST_SOUND(frm);
                              I3D_SOUNDTYPE st = sp->GetSoundType();
                              if(st!=stype){
                                 e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SOUND);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_TYPE, frm);
                                 ck.Write((dword)st);
                                 e_undo->End();

                                 sp->SetSoundType(stype);
         
                                 ed->SetModified();
                              }
                           }
                           InitSheet(SHEET_SOUND);
                        }
                        break;

                     case IDC_COMBO_VOLUME_TYPE:
                        {
                           int vti = SendDlgItemMessage(hwnd, ctl_id, CB_GETCURSEL, 0, 0);
                           assert(vti!=-1);
                           I3D_VOLUMETYPE vtype = (I3D_VOLUMETYPE)SendDlgItemMessage(hwnd, ctl_id, CB_GETITEMDATA, vti, 0);

                           for(dword i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()!=FRAME_VOLUME)
                                 continue;
                              PI3D_volume vol = I3DCAST_VOLUME(frm);
                              I3D_VOLUMETYPE curr_type = vol->GetVolumeType();
                              if(curr_type==vtype)
                                 continue;
                                                //save undo
                              C_chunk &ck = e_undo->Begin(this, UNDO_SET_VOL_TYPE, frm);
                              ck.Write(curr_type);
                              e_undo->End();

                              e_modify->AddFrameFlags(frm, E_MODIFY_FLG_VOLUME_TYPE);

                              vol->SetVolumeType(vtype);

                              ed->SetModified();
                           }
                           InitSheet(SHEET_VOLUME);
                        }
                        break;

                     case IDC_COMBO_COL_MAT:
                        {
                           int i = SendDlgItemMessage(hwnd, ctl_id, CB_GETCURSEL, 0, 0);
                           dword mat_id = SendDlgItemMessage(hwnd, ctl_id, CB_GETITEMDATA, i, 0);
                           for(i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              dword curr_mat = 0;
                              switch(frm->GetType()){
                              case FRAME_VOLUME: curr_mat = I3DCAST_CVOLUME(frm)->GetCollisionMaterial(); break;
                              case FRAME_VISUAL: curr_mat = I3DCAST_CVISUAL(frm)->GetCollisionMaterial(); break;
                              default: assert(0);
                              }
                              if(curr_mat==mat_id)
                                 continue;

                                                //save undo
                              C_chunk &ck = e_undo->Begin(this, UNDO_SET_COL_MAT, frm);
                              //ck.Write(frm->GetName());
                              ck.Write(curr_mat);
                              e_undo->End();

                              e_modify->AddFrameFlags(frm, E_MODIFY_FLG_COL_MAT);

                              switch(frm->GetType()){
                              case FRAME_VOLUME: I3DCAST_VOLUME(frm)->SetCollisionMaterial(mat_id); break;
                              case FRAME_VISUAL: I3DCAST_VISUAL(frm)->SetCollisionMaterial(mat_id); break;
                              default: assert(0);
                              }

                              ed->SetModified();
                           }
                           InitSheet(curr_sheet_index, true);
                        }
                        break;

                     case IDC_COMBO_VISUAL_TYPE:
                        {
                           int i;
                           dword vtype = SendDlgItemMessage(hwnd, ctl_id,
                              CB_GETITEMDATA, SendDlgItemMessage(hwnd, ctl_id, CB_GETCURSEL, 0, 0), 0);
                                          //issue warning before casting into non-geometry types
                           static dword known_geom_vistypes[] = {
                              I3D_VISUAL_OBJECT, I3D_VISUAL_LIT_OBJECT, I3D_VISUAL_SINGLEMESH, I3D_VISUAL_BILLBOARD,
                              I3D_VISUAL_MORPH, I3D_VISUAL_DYNAMIC, I3D_VISUAL_UV_SHIFT,
                              I3D_VISUAL_CAMVIEW,
                              0
                           };
                           for(i=0; known_geom_vistypes[i]; i++){
                              if(vtype==known_geom_vistypes[i])
                                 break;
                           }
                           bool do_cast = true;
                           if(!known_geom_vistypes[i]){
                              do_cast = (MessageBox(hwnd,
                                 "You selected to cast to non-geometry visual type.\n"
                                 "Undo information won't contain geometry information.\n"
                                 "Are you sure to continue?",
                                 "Casting visual",
                                 MB_YESNOCANCEL)==IDYES);
                           }
                           if(do_cast){
                              C_vector<PI3D_frame> save_list = sel_list;
                              e_slct->Clear();

                              for(i=save_list.size(); i--; ){
                                 PI3D_frame frm = save_list[i];
                                 if(frm->GetType()!=FRAME_VISUAL) continue;
                                 PI3D_visual vis = I3DCAST_VISUAL(frm);

                                 if(vis->GetVisualType()==vtype) continue;

                                             //perform desired cast
                                 CastVisual(ed, vis, vtype, e_modify, e_slct);
                              }
                           }
                           InitSheet(SHEET_VISUAL);
                        }
                        break;

                     case IDC_COMBO_SCT_ENV:
                        {
                           int i = SendDlgItemMessage(hwnd, ctl_id, CB_GETCURSEL, 0, 0);
                           dword env_id = SendDlgItemMessage(hwnd, ctl_id, CB_GETITEMDATA, i, 0);
                           for(i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()!=FRAME_SECTOR)
                                 continue;
                              PI3D_sector sct = I3DCAST_SECTOR(frm);

                              dword old_id = sct->GetEnvironmentID();
                              if(old_id!=env_id){
                                 e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SECTOR);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SECTOR_ENV, frm);
                                 ck.Write(old_id);
                                 e_undo->End();

                                 sct->SetEnvironmentID(env_id);
                                 ed->SetModified();
                              }
                           }
                           InitSheet(SHEET_SECTOR);
                        }
                        break;
                     }
                  }
                  break;

               default:
                  {
                     if(curr_sheet_index!=SHEET_VISUAL)
                        break;
                              //this may be one of visual property
                     int id = ctl_id;
                     HWND hwnd_ctrl = (HWND)lParam;
                              //make sure this is one of expected controls
                     for(int i=prop_controls.size(); i--; ){
                        if(prop_controls[i]->hwnd==hwnd_ctrl)
                           break;
                     }
                     assert(i!=-1);
                     int prop_index = id - 20000;

                     ModifyVisualsProperty(sel_list, prop_index,
                        SendDlgItemMessage(hwnd, id, CB_GETCURSEL, 0, 0));
                  }
               }
               in_sheet_modify = false;
            }
            break;

         case BN_CLICKED:
            {
               dword ctrl_id = LOWORD(wParam);
               switch(ctrl_id){
               case IDC_RESET_POS: case IDC_RESET_ROT: case IDC_RESET_SCL: case IDC_RESET_LINK:
               case IDC_RESET_VOL_TYPE: case IDC_RESET_COL_MAT: case IDC_RESET_NU_SCL: case IDC_RESET_FRM_FLAGS:
               case IDC_RESET_LIGHT: case IDC_RESET_BRIGHTNESS:
                  {
                     SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)0);
                     if(ed->CanModify()){
                              //confirm action
                        dword reset_flags;
                        const char *name = GetResetControlInfo(ctrl_id, reset_flags);
                        assert(name);
                        if(MessageBox((HWND)ed->GetIGraph()->GetHWND(),
                           C_xstr("Are you sure to reset % on the selected frames?") %name,
                           C_xstr("Reset %") %name,
                           MB_OKCANCEL)==IDOK){

                           const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                           for(dword i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              dword mf = e_modify->RemoveFlagsAndReset(frm, reset_flags);
                              if(mf)
                                 ed->SetModified();
                           }
                        }
                     }
                     SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)this);
                  }
                  break;

               case IDC_BUTTON_MODEL_BROWSE:
                  if(!ed->CanModify()) break;
                  {
                     PC_editor_item_Create e_create = (PC_editor_item_Create)ed->FindPlugin("Create");
                     if(e_create){
                        C_str fname;
                        if(e_create->BrowseModel(fname)){
                           const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                                                   //change all models
                           for(int i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()==FRAME_MODEL){
                                 PI3D_model mod = I3DCAST_MODEL(frm);
                                 ReplaceModel(mod, fname);
                              }
                           }
                           InitSheet(SHEET_MODEL);
                        }
                     }
                  }
                  break;

               case IDC_BUTTON_LIGHT_SAME_AS:
               case IDC_BUTTON_MODEL_SAME_AS:
               case IDC_BUTTON_VISUAL_SAME_AS:
                  if(!ed->CanModify()) break;
                  {
                     dword action_id = 0;
                     switch(ctrl_id){
                     case IDC_BUTTON_LIGHT_SAME_AS: action_id = E_PROP_NOTIFY_PICK_LIGHT; break;
                     case IDC_BUTTON_MODEL_SAME_AS: action_id = E_PROP_NOTIFY_PICK_MODEL; break;
                     case IDC_BUTTON_VISUAL_SAME_AS: action_id = E_PROP_NOTIFY_PICK_VISUAL; break;
                     default:
                        assert(0);
                     }
                     e_mouseedit->SetUserPick(this, action_id);
                  }
                  break;

               case IDC_BUTTON_VISUAL_SAME_AS2:
                  if(!ed->CanModify()) break;
                  {
                     C_vector<PI3D_frame> frms;
                     struct S_hlp{
                        static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                           ((C_vector<PI3D_frame>*)c)->push_back(frm);
                           return I3DENUMRET_OK;
                        }
                     };
                     ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&frms, ENUMF_VISUAL);
                     if(e_slct->Prompt(frms)){
                        if(frms.size()==1){
                           CopyProperties(I3DCAST_VISUAL(frms.front()));
                        }else{
                           ed->Message("Failed (single visual must be selected)");
                        }
                     }
                  }
                  break;

               case IDC_BUTTON_VISUAL_COPY:
                  {
                     CPI3D_frame sel_frm = e_slct->GetSingleSel();
                     if(!sel_frm){
                        ed->Message("Single visual must be selected.");
                        break;
                     }
                     CPI3D_visual vis = I3DCAST_CVISUAL(sel_frm);
                     dword type = vis->GetVisualType();
                     CPI3D_driver drv = ed->GetDriver();
                     if(OpenClipboard(hwnd)){
                        EmptyClipboard();

                              //build string
                        C_str txt("{\r\n");
                        dword num_p = drv->NumProperties(type);
                        for(dword i=0; i<num_p; i++){
                           I3D_PROPERTYTYPE pt = drv->GetPropertyType(type, i);
                           if(!pt)
                              continue;
                           const char *pn = drv->GetPropertyName(type, i);
                           bool has_spaces = strchr(pn, ' ');
                           txt += C_str(C_xstr("  param %#%#% ")
                              %(has_spaces ? "\"" : "")
                              %pn
                              %(has_spaces ? "\"" : "")
                              );
                           dword prop = vis->GetProperty(i);
                           switch(pt){
                           case I3DPROP_BOOL:
                              prop = bool(prop);
                                       //flow...
                           case I3DPROP_INT: txt += C_str(C_xstr("%") %(int)prop); break;
                           case I3DPROP_FLOAT: txt += FloatStrip(C_xstr("%") %I3DIntAsFloat(prop)); break;
                           case I3DPROP_ENUM:
                              {
                                 const char *pr = pn;
                                 for(dword i=0; i<=prop; i++){
                                    pr += strlen(pr) + 1;
                                    assert(*pr);
                                 }
                                 bool has_spaces = strchr(pr, ' ');
                                 txt += C_str(C_xstr("%#%#%")
                                    %(has_spaces ? "\"" : "")
                                    %pr
                                    %(has_spaces ? "\"" : "")
                                    );
                              }
                              break;
                           case I3DPROP_STRING:
                              {
                                 const char *pr = (const char*)prop;
                                 bool has_spaces = strchr(pr, ' ');
                                 txt += C_str(C_xstr("%#%#%")
                                    %(has_spaces ? "\"" : "")
                                    %pr
                                    %(has_spaces ? "\"" : "")
                                    );
                              }
                              break;
                           default: assert(0);
                           }
                           txt += "\r\n";
                        }
                        txt += "}";

                        HGLOBAL hg = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, txt.Size()+1);
                        char *cp = (char*)GlobalLock(hg);
                        strcpy(cp, txt);

                        GlobalUnlock(hg);
                        SetClipboardData(CF_TEXT, hg);
                        CloseClipboard();

                        ed->Message("Properties copied to clipboard as text.");

                     }
                  }
                  break;

               case IDC_BUTTON_MODEL_RELOAD:
                  if(!ed->CanModify()) break;
                  {
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                     int num_ok = 0;
                     for(int i=sel_list.size(); i--; ){
                        PI3D_frame frm = sel_list[i];
                        if(frm->GetType()==FRAME_MODEL){
                           PI3D_model mod = I3DCAST_MODEL(frm);
                           const C_str &fname = mod->GetFileName();
                           /*
                           ed->GetModelCache().GetRelativeDirectory(&fname[0]);
                           for(dword i=strlen(fname); i--; ){
                              if(fname[i]=='.'){
                                 fname[i] = 0;
                                 break;
                              }
                           }
                           */
                           ed->GetModelCache().Open(mod, fname, ed->GetScene(), 0, NULL, 0);
                           //e_modify->Action(E_MODIFY_DEL_MODEL_DEL_CHILD, &pair<PI3D_frame, const char*>(mod, NULL));
                           ++num_ok;
                        }
                     }
                     ed->Message(C_fstr("Models reloaded: %i", num_ok));
                     ed->SetModified();
                  }
                  break;

               case IDC_BUTTON_SOUND_BROWSE:
                  if(!ed->CanModify()) break;
                  {
                     PC_editor_item_Create e_create = (PC_editor_item_Create)ed->FindPlugin("Create");
                     if(e_create){
                        C_str fname;
                        if(e_create->BrowseSound(fname)){
                           const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                           //SaveUndoSounds(sel_list);
                                       //change all 
                           for(int i=sel_list.size(); i--; ){
                              PI3D_frame frm = sel_list[i];
                              if(frm->GetType()==FRAME_SOUND){
                                 PI3D_sound snd = I3DCAST_SOUND(frm);

                                 C_str old_name = snd->GetFileName();
                                 ed->GetSoundCache().GetRelativeDirectory(&old_name[0]);
                                 if(fname.Matchi(old_name))
                                    continue;

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_FILE, frm);
                                 ck.Write(old_name);
                                 e_undo->End();

                                 ed->GetSoundCache().Open(snd, fname, ed->GetScene(), 0,
                                    OPEN_ERR_LOG(ed, fname));
                                 ed->SetModified();

                                 InitSheet(SHEET_SOUND);
                              }
                           }
                        }
                     }
                  }
                  break;

               case IDC_CHECK_LIGHT_MODE_V:
               case IDC_CHECK_LIGHT_MODE_LM:
               case IDC_CHECK_LIGHT_MODE_SHD:
               case IDC_CHECK_LIGHT_MODE_DYN:
                  if(!ed->CanModify()) break;
                  {
                     in_sheet_modify = true;
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

                     bool on = IsDlgButtonChecked(hwnd, ctrl_id);
                     on = !on;

                     for(int i=sel_list.size(); i--; ){
                        PI3D_frame frm = sel_list[i];
                        if(frm->GetType()!=FRAME_LIGHT) continue;
                        PI3D_light lp = I3DCAST_LIGHT(frm);
                        dword mode = lp->GetMode();

                        e_modify->AddFrameFlags(frm, E_MODIFY_FLG_LIGHT);

                        C_chunk &ck = e_undo->Begin(this, UNDO_LIGHT_MODE, lp);
                        ck.Write(mode);
                        e_undo->End();
                        
                        switch(ctrl_id){
                        case IDC_CHECK_LIGHT_MODE_V:
                           mode &= ~I3DLIGHTMODE_VERTEX;
                           if(on) mode |= I3DLIGHTMODE_VERTEX;
                           break;
                        case IDC_CHECK_LIGHT_MODE_LM:
                           mode &= ~I3DLIGHTMODE_LIGHTMAP;
                           if(on) mode |= I3DLIGHTMODE_LIGHTMAP;
                           break;
                        case IDC_CHECK_LIGHT_MODE_SHD:
                           mode &= ~I3DLIGHTMODE_SHADOW;
                           if(on) mode |= I3DLIGHTMODE_SHADOW;
                           break;
                        case IDC_CHECK_LIGHT_MODE_DYN:
                           mode &= ~I3DLIGHTMODE_DYNAMIC_LM;
                           if(on) mode |= I3DLIGHTMODE_DYNAMIC_LM;
                           break;
                        default:
                           assert(0);
                        }
                        lp->SetMode(mode);

                        ed->SetModified();
                     }
                     CheckDlgButton(hwnd, ctrl_id, !on ? BST_UNCHECKED : BST_CHECKED);
                     in_sheet_modify = false;
                  }
                  break;

               case IDC_CHECK_SOUND_MODE_ON:
               case IDC_CHECK_SOUND_MODE_LOOP:
               case IDC_CHECK_SOUND_MODE_STREAM:
                  if(!ed->CanModify()) break;
                  {
                     in_sheet_modify = true;
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

                     bool on = IsDlgButtonChecked(hwnd, ctrl_id);
                     on = !on;

                     for(int i=sel_list.size(); i--; ){
                        PI3D_frame frm = sel_list[i];
                        if(frm->GetType()!=FRAME_SOUND) continue;
                        PI3D_sound sp = I3DCAST_SOUND(frm);

                        switch(ctrl_id){
                        case IDC_CHECK_SOUND_MODE_ON:
                           {
                              bool play = sp->IsOn();
                              if(play!=on){
                                 e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SOUND);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_ON, frm);
                                 ck.Write(play);
                                 e_undo->End();

                                 sp->SetOn(on);
                                 ed->SetModified();
                              }
                           }
                           break;
                        case IDC_CHECK_SOUND_MODE_LOOP:
                           {
                              bool loop = sp->IsLoop();
                              if(loop!=on){
                                 e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SOUND);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_LOOP, frm);
                                 ck.Write(loop);
                                 e_undo->End();

                                 sp->SetLoop(on);
                                 ed->SetModified();
                              }
                           }
                           break;
                        case IDC_CHECK_SOUND_MODE_STREAM:
                           {
                              bool strm = sp->IsStreaming();
                              if(strm!=on){
                                 e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SOUND);

                                 C_chunk &ck = e_undo->Begin(this, UNDO_SOUND_STRM, frm);
                                 ck.Write(strm);
                                 e_undo->End();

                                 sp->SetStreaming(on);
                                 ed->SetModified();
                              }
                           }
                           break;
                        }
                     }
                     CheckDlgButton(hwnd, ctrl_id, !on ? BST_UNCHECKED : BST_CHECKED);
                     in_sheet_modify = false;
                  }
                  break;

               case IDC_CHECK_NO_SHADOW:
               case IDC_CHECK_NO_COLL:
               case IDC_CHECK_SHADOW_CAST:
               case IDC_CHECK_SHADOW_REC:
               case IDC_CHECK_VOLUME_STATIC:
                  if(!ed->CanModify()) break;
                  {
                     in_sheet_modify = true;
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

                     bool on = IsDlgButtonChecked(hwnd, ctrl_id);
                     on = !on;

                     dword mask;
                     switch(ctrl_id){
                     case IDC_CHECK_NO_SHADOW: mask = I3D_FRMF_NOSHADOW; break;
                     case IDC_CHECK_NO_COLL: mask = I3D_FRMF_NOCOLLISION; break;
                     case IDC_CHECK_SHADOW_CAST: mask = I3D_FRMF_SHADOW_CAST; break;
                     case IDC_CHECK_SHADOW_REC: mask = I3D_FRMF_SHADOW_RECEIVE; break;
                     case IDC_CHECK_VOLUME_STATIC: mask = I3D_FRMF_STATIC_COLLISION; break;
                     default: assert(0); mask = 0;
                     }
                     dword new_flags = on ? mask : 0;

                     for(int i=sel_list.size(); i--; ){
                        PI3D_frame frm = sel_list[i];
                        dword curr_flgs = frm->GetFlags()&mask;
                        if(curr_flgs!=new_flags){
                                          //save undo
                           C_chunk &ck = e_undo->Begin(this, UNDO_SET_FRM_FLAGS, frm);
                           ck.Write(curr_flgs);
                           ck.Write(mask);
                           e_undo->End();

                           e_modify->AddFrameFlags(frm, E_MODIFY_FLG_FRM_FLAGS);

                                          //change
                           frm->SetFlags(new_flags, mask);

                           switch(frm->GetType()){
                           case FRAME_VOLUME: sheet_reset[SHEET_VOLUME] = true; break;
                           case FRAME_VISUAL: sheet_reset[SHEET_VISUAL] = true; break;
                           case FRAME_MODEL: sheet_reset[SHEET_MODEL] = true; break;
                           default: assert(0);
                           }
                        }
                        ed->SetModified();
                     }
                     CheckDlgButton(hwnd, ctrl_id, !on ? BST_UNCHECKED : BST_CHECKED);
                     in_sheet_modify = false;
                     InitSheet(curr_sheet_index, true);
                  }
                  break;

               case IDC_BUTTON_LIGHT_COLOR:
                  if(!ed->CanModify()) break;
                  {
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

                     static COLORREF c_cust[16];
                     CHOOSECOLOR cc;
                     memset(&cc, 0, sizeof(cc));
                     cc.lStructSize = sizeof(CHOOSECOLOR);
                     cc.hwndOwner = hwnd;
                     cc.lpCustColors = c_cust;
                     cc.Flags = CC_ANYCOLOR | CC_FULLOPEN;
                              //check if all lights has the same color
                     bool valid = false;
                     S_vector color;
                     if(sel_list.front()->GetType()==FRAME_LIGHT){   //should always pass
                        color = 
                           //(use_specular ? I3DCAST_LIGHT(sel_list.front())->GetSpecularColor() : I3DCAST_LIGHT(sel_list.front())->GetColor());
                           I3DCAST_LIGHT(sel_list.front())->GetColor();
                        valid = true;
                     }
                     for(dword i=1; i<sel_list.size(); i++){
                        if(sel_list[i]->GetType()==FRAME_LIGHT)
                           if(valid){
                              PI3D_light lp = I3DCAST_LIGHT(sel_list[i]);
                              const S_vector &c1 =
                                 //(use_specular ? lp->GetSpecularColor() : lp->GetColor());
                                 lp->GetColor();
                              valid = (c1==color);
                           }
                     }
                     if(valid){
                        cc.rgbResult = RGB(color[0]*255.0f, color[1]*255.0f, color[2]*255.0f);
                        cc.Flags |= CC_RGBINIT;
                     }
                     cc.Flags |= CC_ENABLEHOOK;
                     cc.lpfnHook = ccHookColor;
                     cc.lCustData = (LPARAM)this;
                              //save all lights colors now
                     C_vector<S_vector> colors(sel_list.size(), S_vector());
                     for(i=0; i<sel_list.size(); i++){
                        PI3D_frame frm = sel_list[i];
                        if(frm->GetType()!=FRAME_LIGHT)
                           continue;
                        colors[i] = I3DCAST_LIGHT(frm)->GetColor();
                     }

                     if(ChooseColor(&cc)){
                        union{
                           byte b[4];
                           dword dw;
                        } au;
                        au.dw = cc.rgbResult;
                        S_vector v(au.b[0]/255.0f, au.b[1]/255.0f, au.b[2]/255.0f);
                        for(i=0; i<sel_list.size(); i++){
                           if(sel_list[i]->GetType()==FRAME_LIGHT){
                              PI3D_light lp = I3DCAST_LIGHT(sel_list[i]);
                              e_modify->AddFrameFlags(lp, E_MODIFY_FLG_LIGHT);

                              C_chunk &ck = e_undo->Begin(this, UNDO_LIGHT_COLOR, lp);
                              ck.Write(colors[i]);
                              e_undo->End();

                              lp->SetColor(v);

                              ed->SetModified();
                           }
                        }
                        InitSheet(SHEET_LIGHT);
                     }else{
                              //restore lights colors
                        for(i=0; i<sel_list.size(); i++){
                           PI3D_frame frm = sel_list[i];
                           if(frm->GetType()!=FRAME_LIGHT)
                              continue;
                           I3DCAST_LIGHT(frm)->SetColor(colors[i]);
                        }
                     }
                  }
                  break;

               case IDC_BUTTON_LSECTORS_ALL1:
               case IDC_BUTTON_LSECTORS_NO1:
               case IDC_BUTTON_SSECTORS_ALL1:
               case IDC_BUTTON_SSECTORS_NO1:
                  if(!ed->CanModify()) break;
                  {
                     in_sheet_modify = true;
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

                     I3D_FRAME_TYPE ftype;
                     dword modify_flags = 0;
                     bool add;

                     switch(ctrl_id){
                     case IDC_BUTTON_LSECTORS_ALL1:
                     case IDC_BUTTON_LSECTORS_NO1:
                        ftype = FRAME_LIGHT;
                        modify_flags = E_MODIFY_FLG_LIGHT;
                        add = (ctrl_id==IDC_BUTTON_LSECTORS_ALL1);
                        break;
                     case IDC_BUTTON_SSECTORS_ALL1:
                     case IDC_BUTTON_SSECTORS_NO1:
                        ftype = FRAME_SOUND;
                        modify_flags = E_MODIFY_FLG_SOUND;
                        add = (ctrl_id==IDC_BUTTON_SSECTORS_ALL1);
                        break;
                     default: assert(0); ftype = FRAME_NULL; add = false;
                     }
                              //get all sectors in scene
                     struct S_hlp{
                        static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                           C_vector<PI3D_sector> *sect_list = (C_vector<PI3D_sector>*)c;
                           sect_list->push_back(I3DCAST_SECTOR(frm));
                           return I3DENUMRET_OK;
                        }
                     };
                     C_vector<PI3D_sector> sect_list;
                     sect_list.push_back(ed->GetScene()->GetPrimarySector());
                     ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&sect_list, ENUMF_SECTOR);

                     for(int i=sel_list.size(); i--; ){
                        PI3D_frame frm = sel_list[i];
                        if(frm->GetType()==ftype){
                           switch(ftype){
                           case FRAME_LIGHT:
                              {
                                 PI3D_light lp = I3DCAST_LIGHT(frm);
                                 for(int j=sect_list.size(); j--; ){
                                    PI3D_sector sct = sect_list[j];
                                    I3D_RESULT ir;

                                    if(add) ir = sct->AddLight(lp);
                                    else ir = sct->RemoveLight(lp);

                                    if(I3D_SUCCESS(ir)){
                                       C_chunk &ck = e_undo->Begin(this, !add ? UNDO_LIGHT_SECTOR_ADD : UNDO_LIGHT_SECTOR_REM, frm);
                                       ck.Write(sct->GetName());
                                       e_undo->End();
                                    }
                                 }
                              }
                              break;
                           case FRAME_SOUND:
                              {
                                 PI3D_sound sp = I3DCAST_SOUND(frm);
                                 for(int j=sect_list.size(); j--; ){
                                    PI3D_sector sct = sect_list[j];
                                    I3D_RESULT ir;

                                    if(add) ir = sct->AddSound(sp);
                                    else ir = sct->RemoveSound(sp);

                                    if(I3D_SUCCESS(ir)){
                                       C_chunk &ck = e_undo->Begin(this, !add ? UNDO_SOUND_SECTOR_ADD : UNDO_SOUND_SECTOR_REM, frm);
                                       ck.Write(sct->GetName());
                                       e_undo->End();
                                    }
                                 }
                              }
                              break;
                           }
                           e_modify->AddFrameFlags(frm, modify_flags);

                           ed->SetModified();
                        }
                     }
                     switch(ftype){
                     case FRAME_LIGHT:
                        InitSheet(SHEET_LSECTORS);
                        break;
                     case FRAME_SOUND:
                        InitSheet(SHEET_SSECTORS);
                        break;
                     }
                     in_sheet_modify = false;
                  }
                  break;

               case IDC_BUTTON_LSECTORS_PICK:
               case IDC_BUTTON_SSECTORS_PICK:
                  if(!ed->CanModify()) break;
                  {
                              //pick sector
                     dword action_id = 0;
                     HCURSOR hcursor = LoadCursor(GetHInstance(),
                        ctrl_id==IDC_BUTTON_LSECTORS_PICK ?
                        "IDC_CURSOR_LSECT_PICK" :
                        "IDC_CURSOR_SSECT_PICK"
                        );
                     switch(ctrl_id){
                     case IDC_BUTTON_LSECTORS_PICK:
                        action_id = E_PROP_NOTIFY_PICK_LSECT;
                        break;
                     case IDC_BUTTON_SSECTORS_PICK:
                        action_id = E_PROP_NOTIFY_PICK_SSECT;
                        break;
                     }
                     e_mouseedit->SetUserPick(this, action_id, hcursor);
                  }
                  break;

                  /*
               case IDC_BUTTON_SCT_SND_PROPS:
                  {
                              //get all sector's properties - feed the table
                     struct S_hlp{
                        PC_editor ed;
                        C_edit_Properties_imp *ep;
                        PC_editor_item e_slct;
                        PC_editor_item e_modify;

                        static void cbTabCfg(LPC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){

                           switch(msg){
                           case TCM_UPDATE:
                              {
                                                   //just send modify notify on all items
                                 for(int i=I3D_SSDI_LAST; i--; )
                                    cbTabCfg(tab, TCM_MODIFY, cb_user, i, -1);
                              }
                              break;
                           case TCM_MODIFY:
                              {
                                 S_hlp *hp = (S_hlp*)cb_user;
                                                   //update params on selection
                                 const C_vector<PI3D_frame> &sel_list = *(C_vector<PI3D_frame>*)hp->e_slct->Action(E_SELECTION_GETVECTOR);
                                 for(int i=sel_list.size(); i--; ){
                                    PI3D_frame frm = sel_list[i];
                                    if(frm->GetType()!=FRAME_SECTOR) continue;
                                    PI3D_sector sct = I3DCAST_SECTOR(frm);
                                    sct->SetSoundData((I3D_SECTOR_SOUND_DATA_INDEX)prm2, tab->ItemI(prm2));

                                    hp->e_modify->AddFrameFlags(sct, E_MODIFY_FLG_SECTOR);
                                 }
                                 hp->ed->GetScene()->Render();
                                 hp->ed->GetIGraph()->UpdateScreen(IGUPDATE_ENTIRE_AREA);
                              }
                              break;
                           }
                        }
                     } hlp = {ed, ep, e_slct, e_modify};

                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                     if(sel_list.size() && sel_list.front()->GetType()==FRAME_SECTOR){

                              //init table by 1st sector in selection
                        PI3D_sector sct = I3DCAST_SECTOR(sel_list.front());
                        for(int i=I3D_SSDI_LAST; i--; ){
                           tab->ItemI(i) = sct->GetSoundData((I3D_SECTOR_SOUND_DATA_INDEX)i);
                        }


                        tab->Edit(&templ, hwnd, S_hlp::cbTabCfg,
                           (dword)&hlp,
                           TABEDIT_IMPORT | TABEDIT_EXPORT,
                           NULL, NULL);
                     }

                     tab->Release();
                  }
                  break;
                  */
               case IDC_SECT_SEL_LIGHTS:
               case IDC_SECT_SEL_SOUNDS:
               case IDC_BUTTON_SSECTORS_SELECT:
               case IDC_BUTTON_LSECTORS_SELECT:
                  {
                     const C_vector<PI3D_frame> sel_list = e_slct->GetCurSel();
                     e_slct->Clear();
                     for(dword i=sel_list.size(); i--; ){
                        PI3D_frame frm = sel_list[i];
                        switch(frm->GetType()){
                        case FRAME_SECTOR:
                           {
                              PI3D_sector sct = I3DCAST_SECTOR(frm);
                              if(ctrl_id==IDC_SECT_SEL_LIGHTS){
                                 for(dword li=sct->NumLights(); li--; ){
                                    PI3D_frame frm = sct->GetLight(li);
                                    e_slct->AddFrame(frm);
                                 }
                              }else{
                                 for(dword li=sct->NumSounds(); li--; ){
                                    PI3D_frame frm = sct->GetSounds()[li];
                                    e_slct->AddFrame(frm);
                                 }
                              }
                           }
                           break;
                        case FRAME_SOUND:
                           {
                              PI3D_sound snd = I3DCAST_SOUND(frm);
                              const PI3D_sector *frms = snd->GetSoundSectors();
                              for(dword i=snd->NumSoundSectors(); i--; )
                                 e_slct->AddFrame(*frms++);
                           }
                           break;
                        case FRAME_LIGHT:
                           {
                              PI3D_light lp = I3DCAST_LIGHT(frm);
                              PI3D_sector const *frms = lp->GetLightSectors();
                              for(dword i=lp->NumLightSectors(); i--; )
                                 e_slct->AddFrame(*frms++);
                           }
                           break;
                        }
                     }
                  }
                  break;

               default:
                  if(!ed->CanModify()) break;
                  {
                     if(curr_sheet_index!=SHEET_VISUAL)
                        break;
                     const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                              //this may be one of visual property
                     HWND hwnd_ctrl = (HWND)lParam;
                              //make sure this is one of expected controls
                     for(int i=prop_controls.size(); i--; ){
                        if(prop_controls[i]->hwnd==hwnd_ctrl)
                           break;
                     }
                     assert(i!=-1);
                     int prop_index = ctrl_id - 20000;

                     bool on = IsDlgButtonChecked(hwnd, ctrl_id);
                     on = !on;
                     CheckDlgButton(hwnd, ctrl_id, !on ? BST_UNCHECKED : BST_CHECKED);

                     ModifyVisualsProperty(sel_list, prop_index, on);
                  }
               }
            }
            break;
         }
         break;
      }
      return 0;
   }

//----------------------------
// Show currently selected tab and hide previous one.
   void ShowSelectedTabWindow(){

      if(curr_sheet_index!=-1)
         ShowWindow(hwnd_sheet[curr_sheet_index], SW_HIDE);
      int tab_indx = SendMessage(hwnd_tc, TCM_GETCURSEL, 0, 0);
      if(tab_indx==-1){
         curr_sheet_index = -1;
      }else{
         curr_sheet_index = TabIndexToSheetIndex(tab_indx);
         assert(curr_sheet_index!=-1);
         ShowWindow(hwnd_sheet[curr_sheet_index], SW_SHOW);
      }
   }

//----------------------------

   static BOOL CALLBACK dlgProp_Thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      C_edit_Properties_imp *ep = (C_edit_Properties_imp*)GetWindowLong(hwnd, GWL_USERDATA);
      if(!ep)
         return 0;
      return ep->dlgProp(hwnd, uMsg, wParam, lParam);
   }

//----------------------------

   BOOL dlgProp(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         return 1;

      case WM_VKEYTOITEM:
         return -1;

      case WM_ACTIVATE:
         {
            bool now_active = (LOWORD(wParam)!=WA_INACTIVE);
            if(window_active!=now_active){
               window_active = now_active;
               ed->GetIGraph()->EnableSuspend(!window_active);
            }
            if(curr_sheet_index!=-1 && (curr_sheets&(1<<curr_sheet_index))){
               SendMessage(hwnd_sheet[curr_sheet_index], uMsg, wParam, lParam);
            }
         }
         break;

      case WM_NOTIFY:
         switch(wParam){
         case IDC_PROP_TAB:
            {
               LPNMHDR hdr = (LPNMHDR)lParam;
               switch(hdr->code){
               case TCN_SELCHANGE:
                  {
                     ShowSelectedTabWindow();
                     //SetActiveWindow(hwnd);
                     SetFocus(hwnd_tc);
                  }
                  break;
               case TCN_KEYDOWN:
                  {
                     NMTCKEYDOWN *kd = (NMTCKEYDOWN*)lParam;
                     switch(kd->wVKey){
                     case VK_PRIOR:
                     case VK_NEXT:
                        {
                           int num_tabs = SendMessage(hwnd_tc, TCM_GETITEMCOUNT, 0, 0);
                           if(num_tabs>1){
                              int curr_sel = SendMessage(hwnd_tc, TCM_GETCURSEL, 0, 0);
                              (kd->wVKey==VK_PRIOR) ? --curr_sel : ++curr_sel;
                              if(curr_sel<0) curr_sel += num_tabs;
                              curr_sel %= num_tabs;
                              SendMessage(hwnd_tc, TCM_SETCURSEL, curr_sel, 0);
                              ShowSelectedTabWindow();
                           }
                        }
                        break;
                        /*
                     default:
                        {
                           dword mods = 0;
                           if(GetAsyncKeyState(VK_CONTROL)&0x8000)
                              mods |= SKEY_CTRL;
                           if(GetAsyncKeyState(VK_MENU)&0x8000)
                              mods |= SKEY_ALT;
                           if(GetAsyncKeyState(VK_SHIFT)&0x8000)
                              mods |= SKEY_SHIFT;
                           ed->MenuHit(0, kd->wVKey, mods);
                        }
                        */
                     }
                  }
                  break;
               }
            }
            break;
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL:
            SetActiveWindow((HWND)ed->GetIGraph()->GetHWND());
            break;
         }
         break;

      case WM_CLOSE:
         Show(false);
         break;

      case WM_DESTROY:
         if(window_active){
            window_active = false;
            ed->GetIGraph()->EnableSuspend(true);
         }
         break;
      }
      return 0;
   }

//----------------------------

   void UpdateWin(){
      if(on){
         ShowWindow(hwnd, SW_SHOWNOACTIVATE);
         //SetActiveWindow(hwnd);
         SetFocus(hwnd);
      }else{
         ShowWindow(hwnd, SW_HIDE);
      }
      ed->CheckMenu(this, E_PROP_SHOW, on);
   }

//----------------------------
// Adjust size of properties window depending on numner of rows.
   void AdjustPropWindowSize(){

      dword num_rows = SendMessage(hwnd_tc, TCM_GETROWCOUNT, 0, 0);
      RECT rc;
      SendMessage(hwnd_tc, TCM_GETITEMRECT, 0, (LPARAM)&rc);
      dword tab_height = (rc.bottom - rc.top) * num_rows;

      GetWindowRect(hwnd_tc, &rc);

      SetWindowPos(hwnd_tc, NULL, 0, 0, tc_size.x, tc_size.y + tab_height,
         SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);

      SetWindowPos(hwnd, NULL, 0, 0, prop_size.x, prop_size.y + tab_height,
         SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);

      GetWindowRect(hwnd_tc, &rc);
      RECT rc_adj = rc;
      SendMessage(hwnd_tc, TCM_ADJUSTRECT, false, (LPARAM)&rc_adj);
      dword x = rc_adj.left - rc.left;
      dword y = rc_adj.top - rc.top + 2;
      dword sx = rc_adj.right - rc_adj.left;
      dword sy = rc_adj.bottom - rc_adj.top - 2;

      for(dword i=hwnd_sheet.size(); i--; ){
         if(i==SHEET_VISUAL_PROPERTY) continue;
         SetWindowPos(hwnd_sheet[i], NULL, x, y, sx, sy,
            SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
      }
   }

//----------------------------

   void AddWindow(int indx, HWND hwnd_sh){

      TCITEM tci;
      memset(&tci, 0, sizeof(tci));
      tci.mask = TCIF_TEXT;

      char buf[256];
      SendMessage(hwnd_sh, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
      tci.pszText = buf;
      tci.lParam = (LPARAM)hwnd_sh;
      SendMessage(hwnd_tc, TCM_INSERTITEM, indx, (LPARAM)&tci);

      /*
      {
         RECT rc_tc;
         GetWindowRect(hwnd_tc, &rc_tc);
         //rc_tc.right -= rc_tc.left;
         rc_tc.right = tc_size.x;
         //rc_tc.bottom -= rc_tc.top;
         rc_tc.bottom = tc_size.y;
         {
            RECT rc;
            GetWindowRect(hwnd, &rc);
            rc_tc.left -= rc.left;
            rc_tc.top -= rc.top;
         }
         SendMessage(hwnd_tc, TCM_ADJUSTRECT, false, (LPARAM)&rc_tc);

         SetWindowPos(hwnd_sh, NULL, rc_tc.left, rc_tc.top,
            (rc_tc.right-rc_tc.left), (rc_tc.bottom-rc_tc.top), 
            SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      */
      AdjustPropWindowSize();
   }

//----------------------------

   void DelItem(int indx){
      ShowWindow(hwnd_sheet[TabIndexToSheetIndex(indx)], SW_HIDE);

      SendMessage(hwnd_tc, TCM_DELETEITEM, indx, 0);
      AdjustPropWindowSize();
   }

//----------------------------

   static dword GetUpperBits(int i){
      dword ret = 0;
      while(i<32){
         ret |= (1<<i);
         ++i;
      }
      return ret;
   }

//----------------------------

   void InitTabs(dword mask, const C_vector<PI3D_frame> *sel_list = NULL, 
      bool del_from_list = false){

      int itm_indx = 0;
      for(dword i=0; i<hwnd_sheet.size(); i++){
         dword bit = (1<<i);

         bool init = (mask&bit);

         if((curr_sheets^mask)&bit){
                              //some change
            if(mask&bit){
               curr_sheets |= bit;
                              //insert
               AddWindow(itm_indx, hwnd_sheet[i]);
            }else{
                              //remove
               DelItem(itm_indx);
               curr_sheets &= ~bit;
               if(del_from_list){
                              //detete sheet handle
                  hwnd_sheet.erase(hwnd_sheet.begin()+i);
                              //shift bits
                  dword save = curr_sheets & GetUpperBits(i+1);
                  curr_sheets &= ~GetUpperBits(i);
                  curr_sheets |= save>>1;

                  save = mask & GetUpperBits(i+1);
                  mask &= ~GetUpperBits(i);
                  mask |= save>>1;

                  if(curr_sheet_index==(int)i)
                     curr_sheet_index = -1;

                  --i;
               }
            }
         }

         if(init){
            InitSheet(i);
            sheet_reset[i] = false;

            ++itm_indx;
         }
      }

      if(curr_sheet_index!=-1 && !(curr_sheets&(1<<curr_sheet_index)))
         curr_sheet_index = -1;

      if(curr_sheet_index==-1){
                              //select first active sheet
#ifdef DEBUG_SELECT_LAST_SHEET
         int tab_indx = SendMessage(hwnd_tc, TCM_GETITEMCOUNT, 0, 0) - 1;
#else
         int tab_indx = 0;
#endif
         SendMessage(hwnd_tc, TCM_SETCURSEL, tab_indx, 0);
         ShowSelectedTabWindow();
      }else{
                              //keep selected active tab
         if(TabIndexToSheetIndex(SendMessage(hwnd_tc, TCM_GETCURSEL, 0, 0)) != curr_sheet_index){
            SendMessage(hwnd_tc, TCM_SETCURSEL, SheetIndexToTabIndex(curr_sheet_index), 0);
         }
      }
   }

//----------------------------

   void InitTabs(){
                              //get current selection
      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      dword mask = curr_sheets & ~((1<<SHEET_USER)-1);
      if(sel_list.size()){
         mask |= 1<<SHEET_FRAME;
         int i;
                              //check what have frames in common and display appropriate tab
         int frm_type = -1;
         dword sub_type = (dword)-1;
         for(i=sel_list.size(); i-- && frm_type!=-2; ){
            PI3D_frame frm = sel_list[i];
            dword ft = frm->GetType();
            switch(frm_type){
            case -2: break;
            case -1: 
               frm_type = ft;
               if(ft==FRAME_VISUAL) sub_type = I3DCAST_VISUAL(frm)->GetVisualType();
               break;
            default: 
               if(frm_type != (int)ft) frm_type = -2;
               else
               switch(sub_type){
               case -1: break;
               case -2: break;
               default:
                  if(ft==FRAME_VISUAL && sub_type != I3DCAST_VISUAL(frm)->GetVisualType())
                     sub_type = (dword)-2;
               }
            }
         }
                              //if same type, display frame-dependent sheet
         switch(frm_type){
         case FRAME_LIGHT:    mask |= (1<<SHEET_LIGHT) | (1<<SHEET_LSECTORS); break;
         case FRAME_SOUND:    mask |= (1<<SHEET_SOUND) | (1<<SHEET_SSECTORS); break;
         //case FRAME_CAMERA:   mask |= 1<<SHEET_CAMERA; break;
         case FRAME_MODEL:    mask |= 1<<SHEET_MODEL;  break;
         case FRAME_VOLUME:   mask |= 1<<SHEET_VOLUME; break;
         case FRAME_SECTOR:   mask |= 1<<SHEET_SECTOR; break;
		   case FRAME_VISUAL:   mask |= 1<<SHEET_VISUAL; break;
         }
      }
      InitTabs(mask, &sel_list);
   }

//----------------------------

   void FeedMaterials(){

      assert(hwnd_sheet.size() >= NUM_SHEETS);
      HWND hwnd_mat = GetDlgItem(hwnd_sheet[SHEET_VOLUME], IDC_COMBO_COL_MAT);
      HWND hwnd_vis_mat = GetDlgItem(hwnd_sheet[SHEET_VISUAL], IDC_COMBO_COL_MAT);
      assert(hwnd_mat);
      SendMessage(hwnd_mat, CB_RESETCONTENT, 0, 0);
      assert(hwnd_vis_mat);
      SendMessage(hwnd_vis_mat, CB_RESETCONTENT, 0, 0);
      int i = 0;
      for(map<dword, C_str>::const_iterator it=mat_table.begin(); it!=mat_table.end(); it++, i++){
         SendMessage(hwnd_mat, CB_ADDSTRING, 0, (LPARAM)(const char*)((*it).second));
         SendMessage(hwnd_mat, CB_SETITEMDATA, i, (*it).first);
         SendMessage(hwnd_vis_mat, CB_ADDSTRING, 0, (LPARAM)(const char*)((*it).second));
         SendMessage(hwnd_vis_mat, CB_SETITEMDATA, i, (*it).first);
      }
   }

//----------------------------

   void FeedEnvironments(){

      assert(hwnd_sheet.size() >= NUM_SHEETS);
      HWND hwnd_cb = GetDlgItem(hwnd_sheet[SHEET_SECTOR], IDC_COMBO_SCT_ENV);
      assert(hwnd_cb);
      SendMessage(hwnd_cb, CB_RESETCONTENT, 0, 0);
      int i = 0;
      for(map<dword, C_str>::const_iterator it=env_table.begin(); it!=env_table.end(); it++, i++){
         SendMessage(hwnd_cb, CB_ADDSTRING, 0, (LPARAM)(const char*)((*it).second));
         SendMessage(hwnd_cb, CB_SETITEMDATA, i, (*it).first);
      }
   }

//----------------------------
// Refresh sheets - reorganize tabs if selection changes, or selection is modified.
   void RefreshSheets(){
      if(on){
         if(slct_reset){
            slct_reset = false;
            InitTabs();
                              //cancel any previous picking
            if(e_mouseedit->GetUserPick()==this){
               e_mouseedit->SetUserPick(NULL, 0);
            }
         }else
         if(sheet_reset[curr_sheet_index]){
            sheet_reset[curr_sheet_index] = false;
            InitSheet(curr_sheet_index);
         }
      }
   }

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      if(!ed->CanModify()) return;

      switch(id){
      case UNDO_RENAME_FRAME:
         {
            C_str old_name = ck.ReadString();
            if(IsNameInScene(ed->GetScene(), old_name))
               break;
            RenameFrame(frm, old_name);
                              //update sheet
            if(on){
               sheet_reset[SHEET_FRAME] = true;
            }else
               slct_reset = true;
         }
         break;

      case UNDO_SET_COL_MAT:
         {
                              //save undo
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            switch(frm->GetType()){
            case FRAME_VOLUME:
               ck_undo.Write((dword)I3DCAST_VOLUME(frm)->GetCollisionMaterial());
               I3DCAST_VOLUME(frm)->SetCollisionMaterial(ck.ReadDword());
               if(on) sheet_reset[SHEET_VOLUME] = true;
               break;
            case FRAME_VISUAL:
               ck_undo.Write((dword)I3DCAST_VISUAL(frm)->GetCollisionMaterial());
               I3DCAST_VISUAL(frm)->SetCollisionMaterial(ck.ReadDword());
               if(on) sheet_reset[SHEET_VISUAL] = true;
               break;
            }
            e_undo->End();
            if(!on)
               slct_reset = true;
         }
         break;

      case UNDO_SET_VOL_TYPE:
         {
            if(frm->GetType()!=FRAME_VOLUME)
               return;
            PI3D_volume vol = I3DCAST_VOLUME(frm);
                              //save undo
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write((dword)vol->GetVolumeType());
            vol->SetVolumeType((I3D_VOLUMETYPE)ck.ReadDword());
            e_undo->End();
            if(on) sheet_reset[SHEET_VOLUME] = true;
            else slct_reset = true;
         }
         break;

      case UNDO_SET_FRM_FLAGS:
         {
            dword new_flags = ck.ReadDword();
            dword mask = ck.ReadDword();
            dword curr_flags = frm->GetFlags()&mask;
                              //save undo
            C_chunk &ck = e_undo->Begin(this, id, frm);
            //ck.Write(frm->GetName());
            ck.Write(curr_flags);
            ck.Write(mask);
            e_undo->End();

            frm->SetFlags(new_flags, mask);
            if(on){
               switch(frm->GetType()){
               case FRAME_VOLUME: sheet_reset[SHEET_VOLUME] = true; break;
               case FRAME_VISUAL: sheet_reset[SHEET_VISUAL] = true; break;
               case FRAME_MODEL: sheet_reset[SHEET_MODEL] = true; break;
               default: assert(0);
               }
            }else
               slct_reset = true;
         }
         break;

      case UNDO_SET_BRIGHTNESS:
         {
                              //save undo
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(GetModelBrightness(I3DCAST_MODEL(frm)));
            e_undo->End();
                              //apply value
            SetModelBrightness(I3DCAST_MODEL(frm), ck.ReadFloat());
            if(on){
               sheet_reset[SHEET_MODEL] = true;
            }else
               slct_reset = true;
         }
         break;

      case UNDO_SET_TEMPERATURE:
         {
                              //save undo
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(I3DCAST_SECTOR(frm)->GetTemperature());
            e_undo->End();
                              //apply value
            I3DCAST_SECTOR(frm)->SetTemperature(ck.ReadFloat());
            if(on){
               sheet_reset[SHEET_SECTOR] = true;
            }else
               slct_reset = true;
         }
         break;

      case UNDO_TRANSLATE:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(frm->GetPos());
            e_undo->End();
            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION);

            frm->SetPos(ck.ReadVector());
         }
         break;

      case UNDO_ROTATE:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(frm->GetRot());
            e_undo->End();

            dword mf = E_MODIFY_FLG_ROTATION;
            e_modify->AddFrameFlags(frm, mf);

            frm->SetRot(ck.ReadQuaternion());
         }
         break;

      case UNDO_SCALE_U:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(frm->GetScale());
            e_undo->End();

            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SCALE);
            frm->SetScale(ck.ReadFloat());
         }
         break;

      case UNDO_SCALE_NU:
         {
            switch(frm->GetType()){
            case FRAME_VOLUME:
               {
                  PI3D_volume vol = I3DCAST_VOLUME(frm);
                  C_chunk &ck_undo = e_undo->Begin(this, id, frm);
                  //ck_undo.Write(frm->GetName());
                  ck_undo.Write(vol->GetNUScale());
                  e_undo->End();

                  e_modify->AddFrameFlags(frm, E_MODIFY_FLG_NU_SCALE);
                  vol->SetNUScale(ck.ReadVector());
               }
               break;
            default: assert(0);
            }
         }
         break;

      case UNDO_LIGHT_TYPE:
      case UNDO_LIGHT_COLOR:
      case UNDO_LIGHT_POWER:
      case UNDO_LIGHT_RANGE:
      case UNDO_LIGHT_CONE:
      case UNDO_LIGHT_MODE:
         {
            PI3D_light lp = I3DCAST_LIGHT(frm);
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            switch(id){
            case UNDO_LIGHT_TYPE: ck_undo.Write((dword)lp->GetLightType()); lp->SetLightType((I3D_LIGHTTYPE)ck.ReadDword()); break;
            case UNDO_LIGHT_COLOR: ck_undo.Write(lp->GetColor()); lp->SetColor(ck.ReadVector()); break;
            case UNDO_LIGHT_POWER: ck_undo.Write(lp->GetPower()); lp->SetPower(ck.ReadFloat()); break;
            case UNDO_LIGHT_MODE: ck_undo.Write(lp->GetMode()); lp->SetMode(ck.ReadDword()); break;
            case UNDO_LIGHT_RANGE:
               {
                  pair<float, float> r;
                  lp->GetRange(r.first, r.second);
                  ck_undo.Write(r);
                  ck.Read(&r, sizeof(r));
                  lp->SetRange(r.first, r.second);
               }
               break;
            case UNDO_LIGHT_CONE:
               {
                  pair<float, float> r;
                  lp->GetCone(r.first, r.second);
                  ck_undo.Write(r);
                  ck.Read(&r, sizeof(r));
                  lp->SetCone(r.first, r.second);
               }
               break;
            default: assert(0);
            }
            e_undo->End();
            if(on) sheet_reset[SHEET_LIGHT] = true;
            else slct_reset = true;
         }
         break;

      case UNDO_LIGHT_SECTOR_ADD:
      case UNDO_LIGHT_SECTOR_REM:
         {
            PI3D_light lp = I3DCAST_LIGHT(frm);
            C_chunk &ck_undo = e_undo->Begin(this, (UNDO_LIGHT_SECTOR_ADD+UNDO_LIGHT_SECTOR_REM)-id, frm);
            C_str n = ck.ReadString();
            ck_undo.Write(n);
            PI3D_sector sct = I3DCAST_SECTOR(ed->GetScene()->FindFrame(n, ENUMF_SECTOR));
            if(id==UNDO_LIGHT_SECTOR_ADD)
               sct->AddLight(lp);
            else
               sct->RemoveLight(lp);
            e_undo->End();
            if(on) sheet_reset[SHEET_LSECTORS] = true;
            else slct_reset = true;
         }
         break;

      case UNDO_SOUND_TYPE:
      case UNDO_SOUND_FILE:
      case UNDO_SOUND_RANGE:
      case UNDO_SOUND_CONE:
      case UNDO_SOUND_VOLUME:
      case UNDO_SOUND_VOLUME_OUT:
      case UNDO_SOUND_LOOP:
      case UNDO_SOUND_STRM:
      case UNDO_SOUND_ON:
         {
            PI3D_sound sp = I3DCAST_SOUND(frm);
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            switch(id){
            case UNDO_SOUND_TYPE: ck_undo.Write((dword)sp->GetSoundType()); sp->SetSoundType((I3D_SOUNDTYPE)ck.ReadDword()); break;
            case UNDO_SOUND_FILE:
               {
                  C_str old_name = sp->GetFileName();
                  ed->GetSoundCache().GetRelativeDirectory(&old_name[0]);
                  ck_undo.Write(old_name);
                  C_str fname = ck.ReadString();
                  ed->GetSoundCache().Open(sp, fname, ed->GetScene(), 0, OPEN_ERR_LOG(ed, fname));
               }
               break;
            case UNDO_SOUND_VOLUME: ck_undo.Write(sp->GetVolume()); sp->SetVolume(ck.ReadFloat()); break;
            case UNDO_SOUND_VOLUME_OUT: ck_undo.Write(sp->GetOutVol()); sp->SetOutVol(ck.ReadFloat()); break;
            case UNDO_SOUND_LOOP: ck_undo.Write(sp->IsLoop()); sp->SetLoop(ck.ReadBool()); break;
            case UNDO_SOUND_STRM: ck_undo.Write(sp->IsStreaming()); sp->SetStreaming(ck.ReadBool()); break;
            case UNDO_SOUND_ON: ck_undo.Write(sp->IsOn()); sp->SetOn(ck.ReadBool()); break;
            case UNDO_SOUND_RANGE:
               {
                  pair<float, float> r;
                  sp->GetRange(r.first, r.second);
                  ck_undo.Write(r);
                  ck.Read(&r, sizeof(r));
                  sp->SetRange(r.first, r.second);
               }
               break;
            case UNDO_SOUND_CONE:
               {
                  pair<float, float> r;
                  sp->GetCone(r.first, r.second);
                  ck_undo.Write(r);
                  ck.Read(&r, sizeof(r));
                  sp->SetCone(r.first, r.second);
               }
               break;
            default: assert(0);
            }
            e_undo->End();
            if(on) sheet_reset[SHEET_SOUND] = true;
            else slct_reset = true;
         }
         break;

      case UNDO_SOUND_SECTOR_ADD:
      case UNDO_SOUND_SECTOR_REM:
         {
            PI3D_sound sp = I3DCAST_SOUND(frm);
            C_chunk &ck_undo = e_undo->Begin(this, (UNDO_SOUND_SECTOR_ADD+UNDO_SOUND_SECTOR_REM)-id, frm);
            C_str n = ck.ReadString();
            ck_undo.Write(n);
            PI3D_sector sct = I3DCAST_SECTOR(ed->GetScene()->FindFrame(n, ENUMF_SECTOR));
            if(id==UNDO_SOUND_SECTOR_ADD)
               sct->AddSound(sp);
            else
               sct->RemoveSound(sp);
            e_undo->End();
            if(on) sheet_reset[SHEET_SSECTORS] = true;
            else slct_reset = true;
         }
         break;

      case UNDO_SECTOR_ENV:
         {
            PI3D_sector sct = I3DCAST_SECTOR(frm);
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            ck_undo.Write(sct->GetEnvironmentID());
            e_undo->End();
            sct->SetEnvironmentID(ck.ReadDword());

            if(on) sheet_reset[SHEET_SECTOR] = true;
            else slct_reset = true;
         }
         break;

      case UNDO_VISUAL_PROPERTY:
         {
            dword indx = ck.ReadDword();
            dword val = ck.ReadDword();
            ModifyVisualProperty(I3DCAST_VISUAL(frm), indx, val);

            if(on) sheet_reset[SHEET_VISUAL] = true;
            else slct_reset = true;
         }
         break;

      default: assert(0);
      }
      e_slct->FlashFrame(frm);
      ed->SetModified();
   }

//----------------------------

   virtual void Show(bool b){

      if(b){
         on = true;
         UpdateWin();
         SetActiveWindow(hwnd);
      }else{
         on = false;
         UpdateWin();
      }
   }

//----------------------------

   virtual bool IsActive() const{

      HWND active_wnd = GetActiveWindow();
      return (active_wnd == hwnd || active_wnd == hwnd_tc);
   }

//----------------------------

   virtual void AddSheet(void *hwnd_in, bool show){

      RefreshSheets();

      HWND hwnd_sh = (HWND)hwnd_in;

      dword ws = GetWindowLong(hwnd_sh, GWL_STYLE);
      ws &= ~(WS_POPUP | WS_BORDER | WS_CAPTION | WS_DLGFRAME | WS_SYSMENU | WS_THICKFRAME);
      ws |= WS_CHILD | WS_CLIPSIBLINGS;
      SetWindowLong(hwnd_sh, GWL_STYLE, ws);

      dword wsex = GetWindowLong(hwnd_sh, GWL_EXSTYLE);
      wsex &= ~WS_EX_WINDOWEDGE;
      wsex |= WS_EX_CONTROLPARENT;
      SetWindowLong(hwnd_sh, GWL_EXSTYLE, wsex);

      SetParent(hwnd_sh, hwnd);
      SetActiveWindow((HWND)ed->GetIGraph()->GetHWND());

      hwnd_sheet.push_back(hwnd_sh);
      InitTabs(curr_sheets | (1<<(hwnd_sheet.size()-1)), &e_slct->GetCurSel());

      if(show)
         ShowSheet(hwnd_in);
   }

//----------------------------

   virtual bool RemoveSheet(void *hwnd_in){

      HWND hwnd_sh = (HWND)hwnd_in;
      for(dword i=NUM_SHEETS; i<hwnd_sheet.size(); i++)
      if(hwnd_sheet[i]==hwnd_sh){
         if(e_slct){
            InitTabs(curr_sheets & ~(1<<i), &e_slct->GetCurSel(), true);
         }else
            hwnd_sheet.erase(hwnd_sheet.begin()+i);
         return true;
      }
      return false;
   }

//----------------------------

   virtual void ShowSheet(void *hwnd_in){

      HWND hwnd_sh = (HWND)hwnd_in;
      for(dword i=NUM_SHEETS; i<hwnd_sheet.size(); i++)
      if(hwnd_sheet[i]==hwnd_sh){
         if(curr_sheet_index!=(int)i){
            SendMessage(hwnd_tc, TCM_SETCURSEL, SheetIndexToTabIndex(i), 0);
            ShowSelectedTabWindow();
         }
         break;
      }
   }

//----------------------------

   virtual void Activate(){

      on = true;
      UpdateWin();
   }

//----------------------------

   virtual void BeforeFree(){

      InitTabs(dword(~((1<<SHEET_USER)-1)));
   }

//----------------------------

   virtual void ShowFrameSheet(I3D_FRAME_TYPE type){

      RefreshSheets();
      int indx = SHEET_FRAME;
      switch(type){
      case FRAME_LIGHT: indx = SHEET_LIGHT; break;
      case FRAME_SOUND: indx = SHEET_SOUND; break;
      case FRAME_MODEL: indx = SHEET_MODEL; break;
      default: assert(0);
      }
      if(curr_sheet_index != indx){
         SendMessage(hwnd_tc, TCM_SETCURSEL, SheetIndexToTabIndex(indx), 0);
         ShowSelectedTabWindow();
      }
   }

//----------------------------

public:
   C_edit_Properties_imp():
      hwnd(NULL),
      curr_sheets(0),
      curr_sheet_index(-1),
      vis_prop_scroll_y(0),
      slct_reset(false),
      in_sheet_modify(false),
      on(true),
      window_active(false)
   {
      for(int i=32; i--; ) sheet_reset[i] = false;
   }

//----------------------------

   virtual bool Init(){

      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_mouseedit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      if(!e_slct || !e_modify || !e_undo || !e_mouseedit)
         return false;

      igraph_init_flags = ed->GetIGraph()->GetFlags();
                              //create properties window
      hwnd = CreateDialogParam(GetHInstance(), "IDD_PROPERTIES",
         (HWND)ed->GetIGraph()->GetHWND(), dlgProp_Thunk, (LPARAM)this);
      if(hwnd){
         RECT rc;
         GetWindowRect(hwnd, &rc);
                              //by default, put it into right down corner
         int x = GetSystemMetrics(SM_CXSCREEN);
         int y = GetSystemMetrics(SM_CYSCREEN);
         x -= (rc.right - rc.left);
         y -= (rc.bottom - rc.top);

         SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

         UpdateWin();

         ed->GetIGraph()->AddDlgHWND(hwnd);
         SetActiveWindow((HWND)ed->GetIGraph()->GetHWND());

         hwnd_tc = GetDlgItem(hwnd, 1);
         {
            RECT rc;
            GetWindowRect(hwnd_tc, &rc);
            tc_size.x = rc.right - rc.left;
            tc_size.y = rc.bottom - rc.top;

            GetWindowRect(hwnd, &rc);
            prop_size.x = rc.right - rc.left;
            prop_size.y = rc.bottom - rc.top;
         }

                              //load sheets into property window
         for(int i=0; i<NUM_SHEETS; i++){
            S_sheet_init si = {i, this};
            HWND hwnd_sh = CreateDialogParam(GetHInstance(), C_fstr("IDD_PROP_SHEET%.2i", i),
               hwnd, dlgSheet_Thunk, (LPARAM)&si);
            SetParent(hwnd_sh, hwnd);
            hwnd_sheet.push_back(hwnd_sh);
         }
         {
            HWND hwnd_pg = GetDlgItem(hwnd_sheet[SHEET_VISUAL], IDC_VIS_PROP_GROUP_BASE);
            assert(hwnd_pg);
            RECT rc;
            POINT pt = {0, 0};
            GetWindowRect(hwnd_pg, &rc);
            ClientToScreen(hwnd_sheet[SHEET_VISUAL], &pt);
            vis_property_ground_pos[0] = rc.left - pt.x;
            vis_property_ground_pos[1] = rc.top - pt.y;
            vis_property_ground_size[0] = rc.right - rc.left;
            vis_property_ground_size[1] = rc.bottom - rc.top;

            hwnd_pg = hwnd_sheet[SHEET_VISUAL_PROPERTY];
            
            SetParent(hwnd_pg, hwnd_sheet[SHEET_VISUAL]);
            SetWindowLong(hwnd_pg, GWL_ID, IDC_VIS_PROP_GROUP);
            SetWindowPos(hwnd_pg, NULL, vis_property_ground_pos[0], vis_property_ground_pos[1],
               vis_property_ground_size[0], vis_property_ground_size[1],
               SWP_SHOWWINDOW);
         }
      }

      ed->AddShortcut(this, E_PROP_SHOW, "%10 &Edit\\%100 %i &Properties\tAlt+Enter", K_ENTER, SKEY_ALT);

                             //be sensible on selection change
      e_slct->AddNotify(this, E_PROP_NOTIFY_SELECTION);
                              //be sensible on modifications
      e_modify->AddNotify(this, E_PROP_NOTIFY_MODIFY);
      return true;
   }

//----------------------------

   virtual void Close(){
      if(e_slct)
         e_slct->RemoveNotify(this);
      if(e_modify)
         e_modify->RemoveNotify(this);

      e_slct = NULL;
      e_modify = NULL;
      e_undo = NULL;
      e_mouseedit = NULL;

      if(hwnd){
         InitTabs(0);
         ed->GetIGraph()->RemoveDlgHWND(hwnd);
         DestroyWindow(hwnd);
         hwnd = NULL;
      }
      for(int i=hwnd_sheet.size(); i--; ){
         DestroyWindow(hwnd_sheet[i]);
      }
      hwnd_sheet.clear();
   }

//----------------------------

   int TabIndexToSheetIndex(int ti) const{
      if(!curr_sheets) return -1;
      for(int i = ti+1, indx = 0; i; ++indx)
         if(curr_sheets&(1<<indx)) --i;
      --indx;
      return indx;
   }

//----------------------------

   int SheetIndexToTabIndex(int si) const{

      if(!(curr_sheets&(1<<si)))
         return -1;
      for(int i = 0, indx = -1; i<si; ++i)
         if(curr_sheets&(1<<i))
            ++indx;
      ++indx;
      return indx;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      RefreshSheets();
   }

//----------------------------


   static void SetListControlCheck(HWND hwnd_lv, LPARAM item_lparam, bool on){

      if(hwnd_lv){
         LVITEM lvi;
         memset(&lvi, 0, sizeof(lvi));
         lvi.mask = LVIF_PARAM;
         for(dword i=SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0); i--; ){
            lvi.iItem = i;
            SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM)&lvi);
            if(lvi.lParam==item_lparam){
               lvi.mask = LVIF_STATE;
               lvi.state = INDEXTOSTATEIMAGEMASK(1 + on);
               lvi.stateMask = LVIS_STATEIMAGEMASK;
               SendMessage(hwnd_lv, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
               SendMessage(hwnd_lv, LVM_ENSUREVISIBLE, i, false);
               break;
            }
         }
      }
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){
      case E_PROP_SHOW:
         Show(true);
         break;

      case E_PROP_NOTIFY_SELECTION:
         slct_reset = true;
         break;

      case E_PROP_NOTIFY_MODIFY:
                              //check if immediate action is to be taken (sheet update)
         if(on && !slct_reset && !in_sheet_modify){

            const pair<PI3D_frame, dword> &p = *(pair<PI3D_frame, dword>*)context;
                              //get flags of current sheet
            dword mod_flgs = p.second;

            if(mod_flgs&(E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE|E_MODIFY_FLG_LINK))
               sheet_reset[SHEET_FRAME] = true;
            if(mod_flgs&E_MODIFY_FLG_LIGHT){
               sheet_reset[SHEET_LIGHT] = true;
               sheet_reset[SHEET_LSECTORS] = true;
            }
            if(mod_flgs&E_MODIFY_FLG_SOUND){
               sheet_reset[SHEET_SOUND] = true;
               sheet_reset[SHEET_SSECTORS] = true;
            }
            if(mod_flgs&E_MODIFY_FLG_VISUAL)
               sheet_reset[SHEET_VISUAL] = true;
            if(mod_flgs&E_MODIFY_FLG_SECTOR)
               sheet_reset[SHEET_SECTOR] = true;
            if(mod_flgs&E_MODIFY_FLG_COL_MAT){
               sheet_reset[SHEET_VOLUME] = true;
               sheet_reset[SHEET_VISUAL] = true;
            }
            if(mod_flgs&E_MODIFY_FLG_FRM_FLAGS){
               sheet_reset[SHEET_VOLUME] = true;
               sheet_reset[SHEET_MODEL] = true;
               sheet_reset[SHEET_VISUAL] = true;
            }
            if(mod_flgs&(E_MODIFY_FLG_NU_SCALE|E_MODIFY_FLG_VOLUME_TYPE))
               sheet_reset[SHEET_VOLUME] = true;
            if(mod_flgs&E_MODIFY_FLG_BRIGHTNESS)
               sheet_reset[SHEET_MODEL] = true;
         }
         break;

      case E_PROP_RESET_SHEET:
         {
            sheet_reset[curr_sheet_index] = true;
         }
         break;

      case E_PROP_NOTIFY_PICK_LIGHT:
         {
            /*
            if(!ed->CanModify()) break;
            PI3D_frame frm = ((S_MouseEdit_picked*)context)->frm_picked;
            if(!frm || frm->GetType()!=FRAME_MODEL){
               ed->Message("Please click on light", 0, EM_WARNING);
               break;
            }
            */
            ed->Message("Function not implemented", 0, EM_WARNING);
         }
         break;

      case E_PROP_NOTIFY_PICK_MODEL:
         {
            if(!ed->CanModify()) break;
            PI3D_frame frm = ((S_MouseEdit_picked*)context)->frm_picked;
            frm = TrySelectTopModel(frm, false);
            if(!frm || frm->GetType()!=FRAME_MODEL){
               ed->Message("Please click on model", 0, EM_WARNING);
               break;
            }
            const C_str &new_name = I3DCAST_MODEL(frm)->GetFileName();
            //ed->GetModelCache().GetRelativeDirectory(&new_name[0]);

            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                              //change all models
            for(int i=sel_list.size(); i--; ){
               PI3D_frame frm = sel_list[i];
               if(frm->GetType()==FRAME_MODEL)
                  ReplaceModel(I3DCAST_MODEL(frm), new_name);
            }
            InitSheet(SHEET_MODEL);
         }
         break;

      case E_PROP_NOTIFY_PICK_VISUAL:
         {
            if(!ed->CanModify()) break;
            PI3D_frame frm = ((S_MouseEdit_picked*)context)->frm_picked;
            if(!frm || frm->GetType()!=FRAME_VISUAL){
               ed->Message("Please click on visual", 0, EM_WARNING);
               break;
            }
            CopyProperties(I3DCAST_VISUAL(frm));
         }
         break;

      case E_PROP_NOTIFY_PICK_LSECT:
      case E_PROP_NOTIFY_PICK_SSECT:
         {
            if(!ed->CanModify()) break;
                              //determine picked sector
            PI3D_frame frm = ((S_MouseEdit_picked*)context)->frm_picked;
            if(!frm) frm = ed->GetScene()->GetPrimarySector();
            while(frm && frm->GetType()!=FRAME_SECTOR)
               frm = frm->GetParent();
            if(!frm)
               break;
            PI3D_sector sct = I3DCAST_SECTOR(frm);
            e_slct->FlashFrame(sct);

            in_sheet_modify = true;
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            if(!sel_list.size())
               break;

            bool set_into = false;
            switch(id){
            case E_PROP_NOTIFY_PICK_LSECT:
               {
                  if(sel_list[0]->GetType()!=FRAME_LIGHT)
                     break;
                  PI3D_light lp0 = I3DCAST_LIGHT(sel_list[0]);
                                 //determine if 1st light is currently in,
                                 // toggle based on 1st light
                  for(int i=sct->NumLights(); i--; )
                     if(sct->GetLight(i)==lp0) break;
                  set_into = (i==-1);
                  for(i=sel_list.size(); i--; ){
                     PI3D_frame frm = sel_list[i];
                     if(frm->GetType()==FRAME_LIGHT){
                        PI3D_light lp = I3DCAST_LIGHT(frm);

                        C_chunk &ck = e_undo->Begin(this, !set_into ? UNDO_LIGHT_SECTOR_ADD : UNDO_LIGHT_SECTOR_REM, frm);
                        ck.Write(sct->GetName());
                        e_undo->End();

                        if(set_into) sct->AddLight(lp);
                        else sct->RemoveLight(lp);

                        e_modify->AddFrameFlags(frm, E_MODIFY_FLG_LIGHT);

                        ed->SetModified();
                     }
                  }
                              //modify checkmark
                  SetListControlCheck(GetDlgItem(hwnd_sheet[SHEET_LSECTORS], IDC_LIST_LSECTORS), (LPARAM)sct, set_into);
                  //InitSheet(SHEET_LSECTORS);
                  in_sheet_modify = false;
               }
               break;

            case E_PROP_NOTIFY_PICK_SSECT:
               {
                  if(sel_list[0]->GetType()!=FRAME_SOUND)
                     break;
                  PI3D_sound sp0 = I3DCAST_SOUND(sel_list[0]);
                                 //determine if 1st light is currently in,
                                 // toggle based on 1st light
                  for(int i=sct->NumSounds(); i--; )
                     if(sct->GetSounds()[i]==sp0) break;
                  set_into = (i==-1);
                  for(i=sel_list.size(); i--; ){
                     PI3D_frame frm = sel_list[i];
                     if(frm->GetType()==FRAME_SOUND){
                        PI3D_sound sp = I3DCAST_SOUND(frm);

                        C_chunk &ck = e_undo->Begin(this, !set_into ? UNDO_SOUND_SECTOR_ADD : UNDO_SOUND_SECTOR_REM, frm);
                        ck.Write(sct->GetName());
                        e_undo->End();

                        if(set_into) sct->AddSound(sp);
                        else sct->RemoveSound(sp);

                        e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SOUND);

                        ed->SetModified();
                     }
                  }
                  SetListControlCheck(GetDlgItem(hwnd_sheet[SHEET_SSECTORS], IDC_LIST_SSECTORS), (LPARAM)sct, set_into);
                  in_sheet_modify = false;
               }
               break;
            default: assert(0);
            }
            ed->Message(C_fstr("Selected frames %s sector '%s'", set_into ? "added to" : "removed from",
               (const char*)sct->GetName()));
                              //stay in pick mode
            return true;
         }
         break;

      case E_PROP_RELOAD_MODEL:
         {
            if(!ed->CanModify()) break;
            const char *frm_name = (const char*)context;
            PI3D_frame frm = ed->GetScene()->FindFrame(frm_name);
            if(frm && frm->GetType()==FRAME_MODEL){
               int len = strlen(frm_name) + 1;
               const char *new_name = frm_name + len;

               PI3D_model mod = I3DCAST_MODEL(frm);
               ReplaceModel(mod, new_name);
               e_slct->FlashFrame(frm);

               InitSheet(SHEET_MODEL);
            }else
               ed->Message("Properties(E_PROP_RELOAD_MODEL): cannot find frame", 0, EM_ERROR);
         }
         break;

      case EDIT_ACTION_FEED_MATERIALS:
         {
            pair<int, const char*> *pairs = (pair<int, const char*>*)context;
            mat_table.clear();
            for(int i=0; pairs[i].first!=-1; i++)
               mat_table[pairs[i].first] = pairs[i].second;
            FeedMaterials();
         }
         break;

      case EDIT_ACTION_FEED_ENVIRONMENTS:
         {
            pair<int, const char*> *pairs = (pair<int, const char*>*)context;
            env_table.clear();
            for(int i=0; pairs[i].first!=-1; i++)
               env_table[pairs[i].first] = pairs[i].second;
            FeedEnvironments();
            sheet_reset[SHEET_SECTOR] = true;
         }
         break;
      }
      return 0;
   }

//----------------------------

   //virtual void Render(){}

   virtual bool LoadState(C_chunk &ck){
                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version!=0) return false;
                              //read window pos/size
      RECT rc;
      memset(&rc, 0, sizeof(rc));
      ck.Read(&rc, sizeof(rc));
      SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                              //read on/off status
      //ck.Read(&on, sizeof(byte));

      //ck.Read(&curr_sheet_index, sizeof(curr_sheet_index));

                              //update params
      UpdateWin();
      if(on) SetActiveWindow((HWND)ed->GetIGraph()->GetHWND());
      return true;
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{
                              //write version
      byte version = 0;
      ck.Write(&version, sizeof(byte));
                              //write window pos/size
      RECT rc;
      GetWindowRect(hwnd, &rc);
      ck.Write(&rc, sizeof(rc));
                              //write on/off status
      //ck.Write(&on, sizeof(byte));
      //ck.Write(&curr_sheet_index, sizeof(curr_sheet_index));

      return true;
   }

//----------------------------
};

//----------------------------

void CreateProperties(PC_editor ed){
   PC_editor_item ei = new C_edit_Properties_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------

