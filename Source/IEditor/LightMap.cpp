#include "all.h"
#include "common.h"

//----------------------------

class C_edit_LightMap: public C_editor_item{
   virtual const char *GetName() const{ return "LightMap"; }

//----------------------------

   enum E_ACTION_LMAP{
      E_LMAP_SELECT_NOTC = 8000, // - construct all unconstructed
      E_LMAP_CONSTRUCT_SEL,      // - (re)construct selected
      E_LMAP_CONSTRUCT_SEL_PREV, // - (re)construct selected in preview mode
      E_LMAP_CONSTRUCT_ALL,      // - (re)construct all
      E_LMAP_DESTRUCT_SEL,       // - destruct selected
      E_LMAP_DESTRUCT_ALL,       // - destruct all
      E_LMAP_TOGGLE_BLUR,
      E_LMAP_TOGGLE_MIPMAP,
      E_LMAP_AA_1,               // - setup 1x1 antialiasing
      E_LMAP_AA_2,
      E_LMAP_AA_3,
      E_LMAP_AA_4,
      E_LMAP_AA_5,
      E_LMAP_AA_6,
      E_LMAP_RECONSTRUCT,        // - reconstruct all constructed LMaps using current settings
   };

   enum{
      UNDO_CONSTRUCT,
      UNDO_DESTRUCT,
   };

//----------------------------
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;
                              //creation params
   bool lm_blur;
   int lm_aa_ratio;

//----------------------------

   void SaveLMUndo(PI3D_lit_object lm){

      C_chunk &ck_undo = e_undo->Begin(this, UNDO_CONSTRUCT, lm);
      lm->Save(ck_undo.GetHandle());
      e_undo->End();
   }

//----------------------------

   virtual void Undo(dword undo_id, PI3D_frame frm, C_chunk &ck){

      PI3D_lit_object lm = (PI3D_lit_object)frm;

      switch(undo_id){
      case UNDO_CONSTRUCT:
         if(lm->IsConstructed())
            SaveLMUndo(lm);
         else{
            e_undo->Begin(this, UNDO_DESTRUCT, lm);
            e_undo->End();
         }
         lm->Load(ck.GetHandle());
         break;

      case UNDO_DESTRUCT:
         SaveLMUndo(lm);
         lm->Destruct();
         break;
      }
   }

//----------------------------

   struct S_light_map_help{   //info used during light-map construction
      dword num_all_lmaps;
      dword curr_lmap;
      C_edit_LightMap *e_lmap;
      //PI3D_frame frm;        //frame being constructed
      PI3D_lit_object *frms;
   };

   static bool I3DAPI cbLightMap(I3D_LOADMESSAGE msg, dword prm1, dword prm2, void *context){

      S_light_map_help &lmh = *(S_light_map_help*)context;
      switch(msg){
      case CBM_PROGRESS:
         {
            const float &f = *(float*)&prm1;
            lmh.e_lmap->ed->Message(C_fstr(
               "LM: %i/%i - %s (%i%%)",
               lmh.curr_lmap + 1,
               lmh.num_all_lmaps,
               (const char*)lmh.frms[lmh.curr_lmap]->GetName(),
               (int)(f*100.0f)),
               0, EM_MESSAGE, true);

            if(prm2==2){
                              //show user what he's done
               lmh.e_lmap->ed->GetScene()->Render();
               lmh.e_lmap->ed->GetIGraph()->UpdateScreen();
               ++lmh.curr_lmap;
            }
         }
         break;

      case CBM_ERROR:
         {
            const char *cp = (const char*)prm1;
            PC_editor_item_Log e_log = (PC_editor_item_Log)lmh.e_lmap->ed->FindPlugin("Log");
            if(e_log){
               C_fstr str("Constructing lightmap on '%s': %s", (const char*)lmh.frms[lmh.curr_lmap]->GetName(), cp);
                              //error in red, warning in green
               dword color = 0xffff0000;
               if(prm2)
                  color = 0xff008000;
               e_log->AddText(str, color);
            }
         }
         break;
      }
      return false;
   }

//----------------------------

public:
   C_edit_LightMap():
      lm_blur(true),
      lm_aa_ratio(1)
   {}

//----------------------------

   virtual bool Init(){
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      if(!e_undo || !e_slct || !e_modify){
         e_undo = NULL;
         e_slct = NULL;
         e_modify = NULL;
      }

#define MB "%70 Light-&map\\"
      ed->AddShortcut(this, E_LMAP_SELECT_NOTC, MB"Select &unconstructed", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_CONSTRUCT_SEL, MB"Construct &selection\tCtrl+F1", K_F1, SKEY_CTRL);
      ed->AddShortcut(this, E_LMAP_CONSTRUCT_SEL_PREV, MB"Construct &selection\tAlt+F1", K_F1, SKEY_ALT);
      ed->AddShortcut(this, E_LMAP_CONSTRUCT_ALL, MB"%a Construct &all", K_NOKEY, 0);
      /*
      ed->AddShortcut(this, E_LMAP_TOGGLE_BLUR, MB"&Blur", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_AA_1, MB"Ant&ialiasing\\&1x1 (draft)", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_AA_2, MB"Ant&ialiasing\\&2x2", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_AA_3, MB"Ant&ialiasing\\&3x3", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_AA_4, MB"Ant&ialiasing\\&4x4 (production)", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_AA_5, MB"Ant&ialiasing\\&5x5", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_AA_6, MB"Ant&ialiasing\\&6x6 (waste of time)", K_NOKEY, 0);
      */
      ed->AddShortcut(this, E_LMAP_DESTRUCT_SEL, MB"&Destruct selection", K_NOKEY, 0);
      ed->AddShortcut(this, E_LMAP_DESTRUCT_ALL, MB"D&estruct all", K_NOKEY, 0);

      /*
      ed->CheckMenu(this, E_LMAP_TOGGLE_BLUR, lm_blur);
      ed->GetDriver()->SetState(RS_LM_AA_RATIO, lm_aa_ratio);
      ed->CheckMenu(this, E_LMAP_AA_1 + lm_aa_ratio - 1, true);
      */
                              //initialize toolbar
      PC_toolbar tb = ed->GetToolbar("Create");
      {
         S_toolbar_button tbs[] = {
            {E_LMAP_SELECT_NOTC,   0, "Select unconstructed lightmaps"},
            {E_LMAP_CONSTRUCT_SEL, 1, "Construct selected lightmaps"},
            {0, -1},
         };
         tb->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_LMAP", GetHInstance(), 90);
      }

      return true;
   }

   virtual void Close(){ 
      e_undo = NULL;
      e_slct = NULL;
      e_modify = NULL;
   }

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}

   virtual dword C_edit_LightMap::Action(int id, void *context){

      int i;
      switch(id){
      case E_LMAP_SELECT_NOTC:   //select all not constructed
         {
            e_slct->Clear();

            struct S_hlp{
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                  PI3D_visual vis = I3DCAST_VISUAL(frm);
                  if(vis->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
                     PI3D_lit_object lm = I3DCAST_LIT_OBJECT(vis);
                     if(!lm->IsConstructed()){
                        ((PC_editor_item_Selection)c)->AddFrame(lm);
                     }
                  }
                  return I3DENUMRET_OK;
               }
            };
            ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)(PC_editor_item_Selection)e_slct, ENUMF_VISUAL);
         }
         break;

      case E_LMAP_CONSTRUCT_SEL:
      case E_LMAP_CONSTRUCT_SEL_PREV:
      case E_LMAP_CONSTRUCT_ALL:
         {
            if(!ed->CanModify())
               break;
            C_vector<PI3D_lit_object> lm_list;
            bool test_cols = true;
                              //collect all
            switch(id){
            case E_LMAP_CONSTRUCT_SEL_PREV:
               test_cols = false;
                              //flow...
            case E_LMAP_CONSTRUCT_SEL:
               {
                  const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                  for(i=sel_list.size(); i--; ){
                     PI3D_frame frm = sel_list[i];
                     if(frm->GetType()==FRAME_VISUAL && I3DCAST_VISUAL(frm)->GetVisualType()==I3D_VISUAL_LIT_OBJECT)
                        lm_list.push_back((PI3D_lit_object)frm);
                  }
               }
               break;
            case E_LMAP_CONSTRUCT_ALL:
               {
                  struct S_hlp{
                     static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                        PI3D_visual vis = I3DCAST_VISUAL(frm);
                        if(vis->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
                           C_vector<PI3D_lit_object> &lm_list = *(C_vector<PI3D_lit_object>*)c;
                           lm_list.push_back(I3DCAST_LIT_OBJECT(vis));
                        }
                        return I3DENUMRET_OK;
                     }
                  };
                  ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&lm_list, ENUMF_VISUAL);
               }
               break;
            }
                              //sort objects by name (at least by something)
            {
               struct S_hlp{
                  static bool cbSort(PI3D_frame f1, PI3D_frame f2){
                     //return (strcmp(f1->GetName(), f2->GetName()) < 0);
                     return (f1->GetName() < f2->GetName());
                  }
               };
               sort(lm_list.begin(), lm_list.end(), S_hlp::cbSort);
            }

            S_light_map_help lmh = { lm_list.size(), 0, this, &lm_list.front()};
            I3D_RESULT ir = I3D_OK;
            C_str cancel_name;

                              //let it run also in background
            ed->GetIGraph()->EnableSuspend(false);
            HANDLE h_thread = GetCurrentThread();
            bool b;
            int curr_priority = GetThreadPriority(h_thread);
            //b = SetThreadPriority(h_thread, THREAD_PRIORITY_LOWEST);
            b = SetThreadPriority(h_thread, THREAD_PRIORITY_IDLE);
            assert(b);
                              //show hourglass
            long save_cursor = GetClassLong((HWND)ed->GetIGraph()->GetHWND(), GCL_HCURSOR);
            SetCursor(LoadCursor(NULL, IDC_WAIT));
            SetClassLong((HWND)ed->GetIGraph()->GetHWND(), GCL_HCURSOR, (long)LoadCursor(NULL, IDC_WAIT));
                              //count time
            dword start_time = ed->GetIGraph()->ReadTimer();

            for(i=lm_list.size(); i--; ){
               PI3D_lit_object lm = lm_list[i];
               if(!lm->IsConstructed()){
                  e_undo->Begin(this, UNDO_DESTRUCT, lm);
                  e_undo->End();
               }else{
                  SaveLMUndo(lm);
               }
               /*
               ir = lm->Construct(ed->GetScene(), 
                  (lm_blur ? I3D_LIGHT_BLUR : 0) |
                  I3D_LIGHT_GEO_COLLISIONS |
                  //I3D_LIGHT_VOL_COLLISIONS |
                  I3D_LIGHT_ALLOW_CANCEL,
                  cbLightMap, &lmh);
               */
               e_modify->AddFrameFlags(lm, E_MODIFY_FLG_VISUAL);
               //if(I3D_FAIL(ir)) break;
            }

            ir = ed->GetScene()->ConstructLightmaps(&lm_list.front(), lm_list.size(),
               (lm_blur ? I3D_LIGHT_BLUR : 0) |
               (test_cols ? I3D_LIGHT_GEO_COLLISIONS : 0) |
               I3D_LIGHT_ALLOW_CANCEL,
               cbLightMap, &lmh);

                              //restore cursor
            SetCursor((HCURSOR)save_cursor);
            SetClassLong((HWND)ed->GetIGraph()->GetHWND(), GCL_HCURSOR, save_cursor);

            ed->GetIGraph()->EnableSuspend(true);
            b = SetThreadPriority(h_thread, curr_priority);
            assert(b);

            ed->SetModified();
            if(I3D_SUCCESS(ir)){
               dword time_delta = ed->GetIGraph()->ReadTimer() - start_time;
               int mins = time_delta / 60000;
               int secs = (time_delta /  1000) % 60;
               int decs = (time_delta / 100) % 10;
               ed->Message(C_fstr("Lightmaps constructed: %i in %i:%.2i.%i sec",
                  lm_list.size(),
                  mins, secs, decs));
            }else
               ed->Message(C_fstr("Canceled on lightmap %i (%s)", lmh.curr_lmap,
                  (const char*)lm_list[Min(lmh.curr_lmap-1, 0ul)]->GetName()));
         }
         break;

      case E_LMAP_DESTRUCT_SEL:
         {
            if(!ed->CanModify()) break;
            int num_done = 0;
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            for(i=sel_list.size(); i--; ){
               PI3D_frame frm = sel_list[i];
               if(frm->GetType()==FRAME_VISUAL){
                  PI3D_visual vis = I3DCAST_VISUAL(frm);
                  if(vis->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
                     PI3D_lit_object lm = I3DCAST_LIT_OBJECT(vis);
                     if(lm->IsConstructed()){
                        SaveLMUndo(lm);

                        lm->Destruct();
                        ++num_done;
                     }
                  }
               }
            }
            if(num_done) ed->SetModified();
            ed->Message(C_fstr("Lightmaps destructed: %i", num_done));
         }
         break;

      case E_LMAP_DESTRUCT_ALL:
         {
            if(!ed->CanModify()) break;
            C_vector<PI3D_lit_object> lm_list;
            struct S_hlp{
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                  PI3D_visual vis = I3DCAST_VISUAL(frm);
                  if(vis->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
                     PI3D_lit_object lm = I3DCAST_LIT_OBJECT(vis);
                     if(lm->IsConstructed()){
                        C_vector<PI3D_lit_object> &lm_list = *(C_vector<PI3D_lit_object>*)c;
                        lm_list.push_back(lm);
                     }
                  }
                  return I3DENUMRET_OK;
               }
            };
                              //collect contructed
            ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&lm_list, ENUMF_VISUAL);
            for(int i=lm_list.size(); i--; ){
               PI3D_lit_object lm = lm_list[i];
               SaveLMUndo(lm);
               lm->Destruct();
            }
            if(lm_list.size()) ed->SetModified();
            ed->Message(C_fstr("Lightmaps destructed: %i", lm_list.size()));
         }
         break;

         /*
      case E_LMAP_TOGGLE_BLUR:
         lm_blur = !lm_blur;
         ed->CheckMenu(this, E_LMAP_TOGGLE_BLUR, lm_blur);
         break;

      case E_LMAP_AA_1:
      case E_LMAP_AA_2:
      case E_LMAP_AA_3:
      case E_LMAP_AA_4:
      case E_LMAP_AA_5:
      case E_LMAP_AA_6:
         ed->CheckMenu(this, E_LMAP_AA_1 + lm_aa_ratio - 1, false);
         lm_aa_ratio = id - E_LMAP_AA_1 + 1;
         ed->GetDriver()->SetState(RS_LM_AA_RATIO, lm_aa_ratio);
         ed->CheckMenu(this, E_LMAP_AA_1 + lm_aa_ratio - 1, true);
         break;
         */
      }
      return 0;
   }

#define VERSION 5

   virtual bool LoadState(C_chunk &ck){
                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version!=VERSION)
         return false;

      //ed->CheckMenu(this, E_LMAP_AA_1 + lm_aa_ratio - 1, false);
                              //read variables
      ck.Read(&lm_blur, sizeof(byte));
      ck.Read(&lm_aa_ratio, sizeof(int));

      /*
      ed->CheckMenu(this, E_LMAP_TOGGLE_BLUR, lm_blur);
      ed->GetDriver()->SetState(RS_LM_AA_RATIO, lm_aa_ratio);
      ed->CheckMenu(this, E_LMAP_AA_1 + lm_aa_ratio - 1, true);
      */
      return true;
   }

   virtual bool SaveState(C_chunk &ck) const{
                              //write version
      byte version = VERSION;
      ck.Write(&version, sizeof(byte));
                              //write variables
      ck.Write(&lm_blur, sizeof(byte));
      ck.Write(&lm_aa_ratio, sizeof(int));
      return true;
   }
};

//----------------------------

void CreateLightMap(PC_editor ed){
   PC_editor_item ei = new C_edit_LightMap;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
