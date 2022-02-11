#include "all.h"
#include "common.h"

#define BSP_TOOL_VERSION 1

//----------------------------
// Editor pluggin for bsp tree builder.
//----------------------------

class C_edit_bsp_tool: public C_editor_item{
   virtual const char *GetName() const{ return "BSPTool"; }

//----------------------------

   enum{
      E_BSP_CREATE,              //ret: bool success; 
      //E_BSP_TOGGLE_SHOW,
      E_BSP_DESTROY,
      E_BSP_REQUIRED,
      E_BSP_TOGGLE_STATIC_SELECTION,
      E_BSP_NOTIFY_MODIFY,
      //E_BSP_FRAME_DELETED, //PI3D_frame
   };

   enum{
      UNDO_SET_STATIC,
   };

//----------------------------

   //bool draw_tree;
   bool bsp_required;         //warn user for bsp missing if this flag is checked
   bool loaded_bsp_req;       //used for reset after load, if no bsp_required info was saved.
   bool modified;             //flag for editor to remind we need build it when something was changed

   C_smart_ptr<C_editor_item> e_properties;
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Log> e_log;

//----------------------------

   struct S_bsp_build_context{
      PC_editor ed;
      dword last_progress_time;
   };

   static bool I3DAPI cpBspBuild(I3D_BSP_MESSAGE msg, dword prm1, dword prm2, void *context){

      S_bsp_build_context *bc = (S_bsp_build_context*)context;
      PC_editor ed = bc->ed;
                              //check if it's time to show progress now
      const dword MIN_PROGRESS_DELTA = 200;
      dword curr_time = ed->GetIGraph()->ReadTimer();
      if((curr_time - bc->last_progress_time) < MIN_PROGRESS_DELTA)
         return true;
      bc->last_progress_time = curr_time;

      switch(msg){
      case BM_BUILDING_NODES:
         ed->Message(C_fstr("Polygon rest: %i", prm1), 0, EM_MESSAGE, true);
         break;
      case BM_GETTING_POLYGONS:
         ed->Message(C_fstr("Getting polygons: %i", prm1), 0, EM_MESSAGE, true);
         break;
      case BM_BUILDING_EDGES:
         ed->Message(C_fstr("Building edges: %i%%", prm1), 0, EM_MESSAGE, true);
         break;
      case BM_LOG:
         {                    //0:error, 1 warning
            dword color(prm2==0 ? 0xff0000 : 0x008000);
            PC_editor_item_Log e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
            e_log->AddText((const char*)prm1, color);
         }
         break;
      default:
         assert(("Unhandled bsp build message.", 0));
      }
      //ed->GetScene()->Render();
      //ed->GetIGraph()->UpdateScreen(IGUPDATE_ENTIRE_AREA);
      return true;
   }

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      switch(id){
      case UNDO_SET_STATIC:
         {
            C_chunk &ck_undo = e_undo->Begin(this, UNDO_SET_STATIC, frm);
            ck_undo.Write(bool(frm->GetFlags()&I3D_FRMF_STATIC_COLLISION));
            e_undo->End();

            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_FRM_FLAGS);

            bool on = ck.ReadBool();
            frm->SetFlags(on ? I3D_FRMF_STATIC_COLLISION : 0, I3D_FRMF_STATIC_COLLISION);

            //UpdatePropertySheet(frm);
         }
         break;
      default: assert(0);
      }
   }

//----------------------------

   virtual bool Create(){

      if(!ed->CanModify())
         return false;
                        //let it run also in background
      ed->GetIGraph()->EnableSuspend(false);

      dword start_time = ed->GetIGraph()->ReadTimer();

      PI3D_scene scn = ed->GetScene();
      S_bsp_build_context bc = {ed, start_time};
      I3D_RESULT br = scn->CreateBspTree(cpBspBuild, &bc);
      dword time_delta = ed->GetIGraph()->ReadTimer() - start_time;

      ed->GetIGraph()->EnableSuspend(true);
      ed->SetModified();

      ed->CheckMenu(this, E_BSP_CREATE, I3D_SUCCESS(br));
      if(I3D_SUCCESS(br)){
         int mins = time_delta / 60000;
         int secs = (time_delta /  1000) % 60;
         int decs = (time_delta / 100) % 10;
         ed->Message(C_fstr("Tree constructed in %i min %2i.%i sec",
            mins, secs, decs));
                        //if user ones create tree, we supposses it will be required forever.(if not, user can turn it off anyway)
         SetBspRequired(true);
         modified = false;
         return true;
      }else{
         switch(br){
         case I3DERR_CANCELED:
            ed->Message("Bsp build canceled! Tree cleared.");
            break;
         default:
            ed->Message("Failed to build bsp tree.");
         }
         scn->DestroyBspTree();
         return false;
      }
   }

//----------------------------

   virtual void AfterLoad(){

      PI3D_scene scn = ed->GetScene();
      ed->CheckMenu(this, E_BSP_CREATE, scn->IsBspBuild());
      SetBspRequired(loaded_bsp_req ? bsp_required : false);
      loaded_bsp_req = false;
      if(bsp_required && !scn->IsBspBuild()){
         e_log->AddText("Bsp tree is not build.", 0xffff0000);
      }
      modified = false;
   }

//----------------------------

   virtual bool Validate(){

      if(bsp_required){
         bool ok = true;
         if(!ed->GetScene()->IsBspBuild()){
            e_log->AddText("Bsp tree is not build.", 0xffff0000);
            ok = false;
         }
         if(modified){
            e_log->AddText("Bsp tree is modified, probably need rebuild.", 0xffff0000);
            ok = false;
         }
         return ok;
      }
#if 0
      //currenly not used. validity is checked with loading, any change in geometry after load
      //is detected by modify pluggin but not by this test(tree use for test its ovn copy of geometry)
      //and after build validity IS ok.
      bool err = !ed->GetScene()->CompareBspChecksum(cbErrLog, NULL);
      return !err;
#endif
      return true;
   }

//----------------------------

   virtual void LoadFromMission(C_chunk &ck){

      while(ck)
      switch(++ck){
      case CT_INT:
         bsp_required = ck.RBoolChunk();
         loaded_bsp_req = true;
         break;
      default: --ck;
      }
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==3){
                           //save info about bsp requirement
         ck <<= CT_EDITOR_PLUGIN;
         ck.WStringChunk(CT_NAME, GetName());
         ck.WBoolChunk(CT_INT, bsp_required);
         --ck;

                           //don't save invalid tree
         if(ed->GetScene()->IsBspBuild()){
            ck <<= CT_BSP_FILE;
            ed->GetScene()->SaveBsp(ck.GetHandle());
            --ck;
         }
      }
   }

//----------------------------

   virtual void OnFrameDelete(PI3D_frame frm){

      PI3D_scene scn = ed->GetScene();
      if(scn->IsBspBuild()){
         if(IsInBsp(frm)){
            //e_log->AddText(C_xstr("Frame '%' removed, destroying bsp.") %frm->GetName(), 0xff0faf0f);
            e_log->AddText("BSP tree is destroyed.", 0xff00b000);
            scn->DestroyBspTree();
         }
      }
   }

//----------------------------

public:

//----------------------------

   virtual bool Init(){

      //draw_tree = false;
      bsp_required = false;
      loaded_bsp_req = false;
      modified = false;
      e_properties = ed->FindPlugin("Properties");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
      if(!e_modify || !e_properties || !e_undo || !e_log)
         return false;

                              //init menu
#define MENU_BASE "%25 &Create\\%i %90 &BSP-tree\\"
      ed->AddShortcut(this, E_BSP_TOGGLE_STATIC_SELECTION,  MENU_BASE"%05 Toggle selection &static \tCtrl+V", K_V, SKEY_CTRL);
      ed->AddShortcut(this, E_BSP_CREATE,                   MENU_BASE"%10 &Create \tCtrl+B", K_B, SKEY_CTRL);
      ed->AddShortcut(this, E_BSP_DESTROY,                  MENU_BASE"%i %50 Clear", K_NOKEY, 0);
      ed->AddShortcut(this, E_BSP_REQUIRED,                 MENU_BASE"%i %60 Required", K_NOKEY, 0);
                              //register for notify of modified frames
      e_modify->AddNotify(this, E_BSP_NOTIFY_MODIFY);

                              //initialize toolbar
      PC_toolbar tb = ed->GetToolbar("Create");
      {
         S_toolbar_button tbs[] = {
            {E_BSP_CREATE,  0, "Build BSP tree"},
            {0, -1},
         };
         tb->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_BSP", GetHInstance(), 60);
      }
      return true;
   }

//----------------------------

   virtual void Close(){}

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){
   }

//----------------------------

   void SaveUndo(PI3D_frame frm){

      C_chunk &ck_undo = e_undo->Begin(this, UNDO_SET_STATIC, frm);
      ck_undo.Write(bool(frm->GetFlags()&I3D_FRMF_STATIC_COLLISION));
      e_undo->End();
   }

   /*
//----------------------------
// Notify properties, so that sheets are updated.
   void UpdatePropertySheet(PI3D_frame frm){

      if(e_properties){
         dword notify_flags = E_MODIFY_FLG_FRM_FLAGS;
         pair<PI3D_frame, dword> flgs(frm, notify_flags);
         e_properties->Action(E_PROP_NOTIFY_MODIFY, &flgs);
      }
   }
   */

//----------------------------

   void ToggleChildrenStatic(PI3D_frame frm, int &toggled_on, int &toggled_off, bool volume_only, int &on){

      //bool cur_static = (frm->GetFlags()&I3D_FRMF_STATIC_COLLISION);

      bool skip_child = false;
      switch(frm->GetType()){
      case FRAME_VISUAL:
                              //for visual selection don't toggle children
         if(volume_only) break;
         else skip_child = true;
                              //..flow
      case FRAME_VOLUME:
         {
            if(on==2)
               on = !(frm->GetFlags()&I3D_FRMF_STATIC_COLLISION);
            bool is_on = (frm->GetFlags()&I3D_FRMF_STATIC_COLLISION);
            if(is_on!=(bool)on){
               SaveUndo(frm);

               e_modify->AddFrameFlags(frm, E_MODIFY_FLG_FRM_FLAGS);
               //frm->SetFlags(!cur_static ? I3D_FRMF_STATIC_COLLISION : 0, I3D_FRMF_STATIC_COLLISION);
               frm->SetFlags(on ? I3D_FRMF_STATIC_COLLISION : 0, I3D_FRMF_STATIC_COLLISION);
               if(on)
                  toggled_on++;
               else
                  toggled_off++;
                              //notify properties, so that sheets are updated
               //UpdatePropertySheet(frm);
            }
         }
         break;
      default: 
         break;
      }
      if(!skip_child)
      for(dword i = 0; i < frm->NumChildren(); i++){
         PI3D_frame child_frm = frm->GetChildren()[i];
         ToggleChildrenStatic(child_frm, toggled_on, toggled_off, volume_only, on);
      }
   }

//----------------------------

   static bool I3DAPI cbErrLog(I3D_LOADMESSAGE msg, dword prm1, dword prm2, void *context){

      if(context){
         PC_editor ed = (PC_editor)context;
         switch(msg){
         case CBM_ERROR:
            {
               dword color = 0;
               switch(prm2){
               case 0:              //error
                  color = 0xff0000;
                  break;
               case 1:              //warning
                  color = 0x008000;
                  break;
               }
               PC_editor_item_Log e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
               e_log->AddText((const char*)prm1, color);
            }
            break;
         }
      }
      return false;
   }

//----------------------------

   static bool IsAnyChildInBsp(CPI3D_frame frm){

      if(frm){
         if(frm->GetFlags()&I3D_FRMF_STATIC_COLLISION){
            switch(frm->GetType()){
            case FRAME_VISUAL:
               return true;
            case FRAME_VOLUME:
               if(I3DCAST_CVOLUME(frm)->GetVolumeType() != I3DVOLUME_SPHERE)
                  return true;
               break;
            default:
               break;
            }
         }
         CPI3D_frame const *frms = frm->GetChildren();
         for(dword i = frm->NumChildren(); i--;){         
            if(IsAnyChildInBsp(frms[i]))
               return true;
         }
      }
      return false;
   }

//----------------------------
// Return true if frame belog to bsp tree. 
// Current version simply check static flag of frame and its children.
   bool IsInBsp(CPI3D_frame frm){

      return IsAnyChildInBsp(frm);
   }

//----------------------------

   void SetBspRequired(bool on_off){

      bsp_required = on_off;
      ed->CheckMenu(this, E_BSP_REQUIRED, on_off);
   }

//----------------------------

   virtual dword Action(int id, void *context){

      dword ret = 0;          //dafault return value

      switch(id){
      case E_BSP_CREATE:
         Create();
         break;

         /*
      case E_BSP_TOGGLE_SHOW:
         draw_tree = !draw_tree;
         ed->CheckMenu(this, E_BSP_TOGGLE_SHOW, draw_tree);
         ed->Message(C_fstr("Draw space occupy %s.", draw_tree ? "on" : "off"));
         break;
         */

      case E_BSP_DESTROY:
         if(!ed->CanModify())
            return false;
         ed->GetScene()->DestroyBspTree();
         ed->SetModified();
         ed->Message("Bsp tree cleared.");
         ed->CheckMenu(this, E_BSP_CREATE, false);
         break;

      case E_BSP_REQUIRED:
         SetBspRequired(!bsp_required);
         break;

      case E_BSP_TOGGLE_STATIC_SELECTION:
         if(ed->CanModify()){
            int togg_on(0); int togg_off(0); 
            const C_vector<PI3D_frame> &sel_list = ((PC_editor_item_Selection)ed->FindPlugin("Selection"))->GetCurSel();
            
            for(int i = sel_list.size(); i--; ){

               PI3D_frame frm = sel_list[i];
               assert(frm);
                              //visual can be set only directly, when frame IS visual. 
                              //for other selection(dummy, models, sectors) change only volumes.
               bool volume_only = (frm->GetType() != FRAME_VISUAL);
               int on = 2;
               ToggleChildrenStatic(frm, togg_on, togg_off, volume_only, on);
            }
            ed->SetModified();
            ed->Message(C_fstr("%i frames set static, %i non static.",togg_on, togg_off));
         }
         break;

      case E_BSP_NOTIFY_MODIFY:
         {
            const pair<PI3D_frame, dword> &p = *(pair<PI3D_frame, dword>*)context;
            if(p.second&E_MODIFY_FLG_POSITION || p.second&E_MODIFY_FLG_ROTATION || 
               p.second&E_MODIFY_FLG_SCALE || p.second&E_MODIFY_FLG_NU_SCALE)
            {
               if(ed->GetScene()->IsBspBuild() && IsInBsp(p.first)){
                  if(!modified){
                     modified = true;
                     e_log->AddText("Bsp tree modified, rebuild.", 0xff0faf0f);
                  }
               }
            }
         }
         break;
      }
      return ret;
   }

//----------------------------

/*
   virtual bool SaveState(C_chunk &ck) const{
   
                              //write version
      byte version = BSP_TOOL_VERSION;
      ck.Write(&version, sizeof(byte));
                              //write variables
      ck.Write(&draw_tree, sizeof(bool));
      return true;
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){

                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version != BSP_TOOL_VERSION)
         return false;
                              //read variables
      ck.Read(&draw_tree, sizeof(bool));

      //ed->CheckMenu(this, E_BSP_TOGGLE_SHOW, draw_tree);
      return true; 
   }
   */

//----------------------------
};

//----------------------------
//----------------------------

void CreateBSPTool(PC_editor ed){

   C_edit_bsp_tool *ei = new C_edit_bsp_tool(); 
   ed->InstallPlugin(ei); 
   ei->Release();
}

//----------------------------
