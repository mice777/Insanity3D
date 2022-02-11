#include "all.h"
#include "common.h"
#include <sortlist.hpp>

//----------------------------
 
class C_edit_Modify: public C_editor_item_Modify{
   virtual const char *GetName() const{ return "Modify"; }

   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Selection> e_slct;

//----------------------------

   enum E_ACTION{
      E_MODIFY_RESET,         //reset modifications on selection
   };

   enum E_UNDO{
      UNDO_ADD_FRAME,
      UNDO_REM_FRAME,
   };

//----------------------------

   struct S_edit_Modify_undo{
      int num_frames;
#pragma pack(push, 1)
      struct S_frm{
         dword flags;            //flags to modify/reset
         S_vector pos;
         S_quat rot;
         float scale;
         S_vector nu_scale;
         char frm_names[1];      //frm_name [link_name]
      };
      S_frm frm_info[1];
#pragma pack(pop)
   };

//----------------------------

   struct S_modify_item{
      dword flags;
      S_vector pos;
      S_quat rot;
      float scale;
      S_vector nu_scale;
      dword col_mat;
      C_str link_name;
      I3D_VOLUMETYPE vol_type;
      dword frm_flags;
      float brightness;

      S_modify_item():
         flags(0),
         scale(0),
         brightness(0)
      {}
   };
   typedef map<C_smart_ptr<I3D_frame>, S_modify_item> t_item_map;
   t_item_map items;
   dword default_reset_flags;

                              //list of plugins to which we send notifications
   C_vector<pair<C_smart_ptr<C_editor_item>, dword> > notify_list;

//----------------------------
                              //frame locking support
                              // (each frame here have associated 'lock' flags - params that cannot be modified
   typedef map<C_smart_ptr<I3D_frame>, dword> t_lock_map;
   t_lock_map lock_items;

//----------------------------

   void SendNotify(const pair<PI3D_frame, dword> &p){

      for(dword i=0; i<notify_list.size(); i++)
         notify_list[i].first->Action(notify_list[i].second, (void*)&p);
   }

//----------------------------

   void SendNotify(PI3D_frame frm, dword flags){
      if(notify_list.size())
         SendNotify(pair<PI3D_frame, dword>(frm, flags));
   }

//----------------------------

   void SetFrameSector(PI3D_frame frm){
                              //set by frame's center point
      I3D_bound_volume bvol;
      frm->ComputeHRBoundVolume(&bvol);
      const I3D_bbox &bbox = bvol.bbox;

      S_vector pos = frm->GetWorldPos();
      if(bbox.IsValid()) pos += bbox.min + (bbox.max-bbox.min)*.5f;
      ed->GetScene()->SetFrameSectorPos(frm, pos);
   }

//----------------------------
// Save undo information for particular flags of a frame, from params stored in the frame.
   void SaveItemUndo(PI3D_frame frm, const S_modify_item &itm, dword flags, dword undo_id){

      C_chunk &ck_undo = e_undo->Begin(this, undo_id, frm);
      //ck_undo.Write(frm->GetName());
      ck_undo.Write(flags);

      if(flags&E_MODIFY_FLG_POSITION){
         ck_undo.Write(frm->GetPos());
         flags &= ~E_MODIFY_FLG_POSITION;
      }
      if(flags&E_MODIFY_FLG_ROTATION){
         ck_undo.Write(frm->GetRot());
         flags &= ~E_MODIFY_FLG_ROTATION;
      }
      if(flags&E_MODIFY_FLG_SCALE){
         ck_undo.Write(frm->GetScale());
         flags &= ~E_MODIFY_FLG_SCALE;
      }
      if(flags&E_MODIFY_FLG_NU_SCALE){
         switch(frm->GetType()){
         case FRAME_VOLUME:
            ck_undo.Write(I3DCAST_CVOLUME(frm)->GetNUScale());
            break;
         default: assert(0);
         }
         flags &= ~E_MODIFY_FLG_NU_SCALE;
      }
      if(flags&E_MODIFY_FLG_LINK){
         ck_undo.Write(frm->GetParent()->GetName());
         flags &= ~E_MODIFY_FLG_LINK;
      }
      if(flags&E_MODIFY_FLG_COL_MAT){
         switch(frm->GetType()){
         case FRAME_VOLUME: ck_undo.Write(I3DCAST_CVOLUME(frm)->GetCollisionMaterial()); break;
         case FRAME_VISUAL: ck_undo.Write(I3DCAST_CVISUAL(frm)->GetCollisionMaterial()); break;
         default: assert(0);
         }
         flags &= ~E_MODIFY_FLG_COL_MAT;
      }
      if(flags&E_MODIFY_FLG_BRIGHTNESS){
         switch(frm->GetType()){
         case FRAME_MODEL: ck_undo.Write(GetModelBrightness(I3DCAST_CMODEL(frm))); break;
         default: assert(0);
         }
         flags &= ~E_MODIFY_FLG_BRIGHTNESS;
      }
      if(flags&E_MODIFY_FLG_VOLUME_TYPE){
         ck_undo.Write((dword)I3DCAST_CVOLUME(frm)->GetVolumeType());
         flags &= ~E_MODIFY_FLG_VOLUME_TYPE;
      }
      if(flags&E_MODIFY_FLG_FRM_FLAGS){
         ck_undo.Write(frm->GetFlags());
         flags &= ~E_MODIFY_FLG_FRM_FLAGS;
      }
      flags &= ~(E_MODIFY_FLG_SECTOR | E_MODIFY_FLG_HIDE | E_MODIFY_FLG_LIGHT | E_MODIFY_FLG_SOUND);
      assert(!flags);

      e_undo->End();
   }

//----------------------------
// Save modify item data into provided structure.
   void SaveItemData(CPI3D_frame frm, S_modify_item &itm, dword flags){

      if(flags&E_MODIFY_FLG_POSITION){
         itm.pos = frm->GetPos();
         flags &= ~E_MODIFY_FLG_POSITION;
      }
      if(flags&E_MODIFY_FLG_ROTATION){
         itm.rot = frm->GetRot();
         flags &= ~E_MODIFY_FLG_ROTATION;
      }
      if(flags&E_MODIFY_FLG_SCALE){
         itm.scale = frm->GetScale();
         flags &= ~E_MODIFY_FLG_SCALE;
      }
      if(flags&E_MODIFY_FLG_NU_SCALE){
         switch(frm->GetType()){
         case FRAME_VOLUME: itm.nu_scale = I3DCAST_CVOLUME(frm)->GetNUScale(); break;
         default: assert(0);
         }
         flags &= ~E_MODIFY_FLG_NU_SCALE;
      }
      if(flags&E_MODIFY_FLG_LINK){
         CPI3D_frame prnt = frm->GetParent();
         if(!prnt) prnt = ed->GetScene()->GetPrimarySector();
         itm.link_name = prnt->GetName();
         flags &= ~E_MODIFY_FLG_LINK;
      }
      if(flags&E_MODIFY_FLG_COL_MAT){
         switch(frm->GetType()){
         case FRAME_VOLUME: itm.col_mat = I3DCAST_CVOLUME(frm)->GetCollisionMaterial(); break;
         case FRAME_VISUAL: itm.col_mat = I3DCAST_CVISUAL(frm)->GetCollisionMaterial(); break;
         default: assert(0);
         }
         flags &= ~E_MODIFY_FLG_COL_MAT;
      }
      if(flags&E_MODIFY_FLG_BRIGHTNESS){
         switch(frm->GetType()){
         case FRAME_MODEL: itm.brightness = GetModelBrightness(I3DCAST_CMODEL(frm)); break;
         default: assert(0);
         }
         flags &= ~E_MODIFY_FLG_BRIGHTNESS;
      }
      if(flags&E_MODIFY_FLG_VOLUME_TYPE){
         itm.vol_type = I3DCAST_CVOLUME(frm)->GetVolumeType();
         flags &= ~E_MODIFY_FLG_VOLUME_TYPE;
      }
      if(flags&E_MODIFY_FLG_FRM_FLAGS){
         itm.frm_flags = frm->GetFlags();
         flags &= ~E_MODIFY_FLG_FRM_FLAGS;
      }
      flags &= ~(E_MODIFY_FLG_SECTOR | E_MODIFY_FLG_CREATE | E_MODIFY_FLG_TEMPORARY | E_MODIFY_FLG_VISUAL | E_MODIFY_FLG_LIGHT | E_MODIFY_FLG_SOUND | E_MODIFY_FLG_HIDE);
      assert(!flags);
   }

//----------------------------

   virtual void AddFrameFlags(PI3D_frame frm, dword flags, bool save_undo = true){

                              //check if frame already in list
      if(!flags)
         return;
      
      t_item_map::iterator it = items.find(frm);
      if(it==items.end()){
         S_modify_item mi;
         memset(&mi, 0, sizeof(mi));
         it = items.insert(t_item_map::value_type(frm, mi)).first;
      }
      S_modify_item &itm = it->second;

      dword new_flags = (itm.flags^flags)&flags;
      if(new_flags){
                        //new flags added
         if(save_undo && (new_flags&~(E_MODIFY_FLG_TEMPORARY))){
            C_chunk &ck_undo = e_undo->Begin(this, UNDO_REM_FRAME, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(new_flags);
            e_undo->End();
         }

         SaveItemData(frm, itm, new_flags);

         itm.flags |= new_flags;
      }
                        //send modify (not if only TEMP flag is being set)
      if(flags&(~E_MODIFY_FLG_TEMPORARY))
         SendNotify(frm, flags);
   }

//----------------------------

   virtual void RemoveFlags(PI3D_frame frm, dword flags){

      t_item_map::iterator it = items.find(frm);
      if(it!=items.end()){
         it->second.flags &= ~flags;
         if(!it->second.flags)
            items.erase(it);
      }
   }

//----------------------------

   virtual dword RemoveFlagsAndReset(PI3D_frame frm, dword flags){

                              //find entry
      t_item_map::iterator it = items.find(frm);
      if(it==items.end())
         return 0;
      S_modify_item &itm = it->second;
      flags &= itm.flags;

      flags &= ~(E_MODIFY_FLG_TEMPORARY | E_MODIFY_FLG_CREATE);
                              //if frame was created in editor, some flags cannot be reset
      if(itm.flags&E_MODIFY_FLG_CREATE)
         flags &= ~(E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_VOLUME_TYPE);
      if(!flags)
         return 0;
                              //save undo info
      SaveItemUndo(frm, itm, flags, UNDO_ADD_FRAME);

                        //perform reset
      dword f = flags;
      if(f&E_MODIFY_FLG_POSITION){
         frm->SetPos(itm.pos);
         f &= ~E_MODIFY_FLG_POSITION;
      }
      if(f&E_MODIFY_FLG_ROTATION){
         frm->SetRot(itm.rot);
         f &= ~E_MODIFY_FLG_ROTATION;
      }
      if(f&E_MODIFY_FLG_SCALE){
         frm->SetScale(itm.scale);
         f &= ~E_MODIFY_FLG_SCALE;
      }
      if(f&E_MODIFY_FLG_NU_SCALE){
         switch(frm->GetType()){
         case FRAME_VOLUME: I3DCAST_VOLUME(frm)->SetNUScale(itm.nu_scale); break;
         default: assert(0);
         }
         f &= ~E_MODIFY_FLG_NU_SCALE;
      }
      if(f&E_MODIFY_FLG_LINK){
         PI3D_frame prev_link = ed->GetScene()->FindFrame(itm.link_name);
         if(!prev_link)
            ed->Message(C_xstr("Reset: can't find link: '%'") %itm.link_name);
         else
            frm->LinkTo(prev_link);
         f &= ~E_MODIFY_FLG_LINK;
      }
      if(f&E_MODIFY_FLG_COL_MAT){
         switch(frm->GetType()){
         case FRAME_VOLUME: I3DCAST_VOLUME(frm)->SetCollisionMaterial(itm.col_mat); break;
         case FRAME_VISUAL: I3DCAST_VISUAL(frm)->SetCollisionMaterial(itm.col_mat); break;
         default: assert(0);
         }
         f &= ~E_MODIFY_FLG_COL_MAT;
      }
      if(f&E_MODIFY_FLG_BRIGHTNESS){
         switch(frm->GetType()){
         case FRAME_MODEL: SetModelBrightness(I3DCAST_MODEL(frm), itm.brightness); break;
         default: assert(0);
         }
         f &= ~E_MODIFY_FLG_BRIGHTNESS;
      }
      if(f&E_MODIFY_FLG_VOLUME_TYPE){
         I3DCAST_VOLUME(frm)->SetVolumeType(itm.vol_type);
         f &= ~E_MODIFY_FLG_VOLUME_TYPE;
      }
      if(f&E_MODIFY_FLG_FRM_FLAGS){
         frm->SetFlags(itm.frm_flags);
         f &= ~E_MODIFY_FLG_FRM_FLAGS;
      }
      f &= ~(E_MODIFY_FLG_SECTOR | E_MODIFY_FLG_HIDE | E_MODIFY_FLG_LIGHT | E_MODIFY_FLG_SOUND);
      assert(!f);

                              //update flags
      itm.flags &= ~flags;
      if(!itm.flags)
         items.erase(it);

      SendNotify(frm, flags);
      e_slct->FlashFrame(frm);
      return flags;
   }

//----------------------------

   virtual bool RemoveFrame(PI3D_frame frm){

      {
         t_lock_map::iterator it = lock_items.find(frm);
         if(it!=lock_items.end())
            lock_items.erase(it);
      }
      t_item_map::iterator it = items.find(frm);
      if(it==items.end())
         return false;
      items.erase(it);
      return true;
   }

//----------------------------

   virtual dword GetFrameFlags(CPI3D_frame frm) const{

      t_item_map::const_iterator it = items.find(const_cast<PI3D_frame>(frm));
      if(it==items.end())
         return 0;
      return (*it).second.flags;
   }

//----------------------------

   virtual bool ReplaceFrame(PI3D_frame frm_orig, PI3D_frame frm_new){

      t_item_map::iterator it = items.find(frm_orig);
      if(it==items.end())
         return false;
      //assert((*it).second.frm==frm_orig);

      S_modify_item &itm = (items[frm_new] = S_modify_item());
      itm = (*it).second;
      //itm.frm = frm_new;
      items.erase(frm_orig);
      return true;
   }

//----------------------------

   virtual void ResetFlagsOnSelection(){

      if(!ed->CanModify())
         return;
      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      for(int j=sel_list.size(); j--; ){
         PI3D_frame frm = sel_list[j];
         t_item_map::iterator it = items.find(frm);
         if(it==items.end()) continue;

         S_modify_item &itm = it->second;
         dword reset_flags = E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE | E_MODIFY_FLG_NU_SCALE | E_MODIFY_FLG_LINK;
         reset_flags &= itm.flags;

                        //can't reset position/rotation on created
         if(itm.flags&E_MODIFY_FLG_CREATE) reset_flags &= ~(E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION);

         if(!reset_flags)
            continue;

         {
                        //prompt user for properties to reset
            struct S_hlp{
               dword reset_flags, default_reset_flags;
               PIGraph igraph;
               const char *frm_name;

               static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

                  switch(uMsg){
                  case WM_INITDIALOG:
                     {
                        S_hlp *hp = (S_hlp*)lParam;
                        InitDlg(hp->igraph, hwnd);
                                                      //enable selected items
                        if(!(hp->reset_flags&E_MODIFY_FLG_POSITION)) EnableWindow(GetDlgItem(hwnd, IDC_RESET_POS), false);
                        if(!(hp->reset_flags&E_MODIFY_FLG_ROTATION)) EnableWindow(GetDlgItem(hwnd, IDC_RESET_ROT), false);
                        if(!(hp->reset_flags&(E_MODIFY_FLG_SCALE|E_MODIFY_FLG_NU_SCALE))) EnableWindow(GetDlgItem(hwnd, IDC_RESET_SCL), false);
                        if(!(hp->reset_flags&E_MODIFY_FLG_LINK)) EnableWindow(GetDlgItem(hwnd, IDC_RESET_LINK), false);

                        if(hp->default_reset_flags&E_MODIFY_FLG_POSITION) CheckDlgButton(hwnd, IDC_RESET_POS, BST_CHECKED);
                        if(hp->default_reset_flags&E_MODIFY_FLG_ROTATION) CheckDlgButton(hwnd, IDC_RESET_ROT, BST_CHECKED);
                        if(hp->default_reset_flags&(E_MODIFY_FLG_SCALE|E_MODIFY_FLG_NU_SCALE)) CheckDlgButton(hwnd, IDC_RESET_SCL, BST_CHECKED);
                        if(hp->default_reset_flags&E_MODIFY_FLG_LINK) CheckDlgButton(hwnd, IDC_RESET_LINK, BST_CHECKED);
                        char win_name[256];
                        GetWindowText(hwnd, win_name, sizeof(win_name));
                        SetWindowText(hwnd, C_fstr("%s - %s", win_name, hp->frm_name));

                        SetWindowLong(hwnd, GWL_USERDATA, lParam);
                     }
                     return 1;
                  case WM_COMMAND:
                     switch(wParam){
                     case IDCANCEL:
                        EndDialog(hwnd, 0);
                        break;
                     case IDOK:
                        {
                           S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                                                      //collect results
                           hp->default_reset_flags &= 
                              ~(E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE|E_MODIFY_FLG_NU_SCALE|E_MODIFY_FLG_LINK);

                           if(IsDlgButtonChecked(hwnd, IDC_RESET_POS)) hp->default_reset_flags |= E_MODIFY_FLG_POSITION;
                           if(IsDlgButtonChecked(hwnd, IDC_RESET_ROT)) hp->default_reset_flags |= E_MODIFY_FLG_ROTATION;
                           if(IsDlgButtonChecked(hwnd, IDC_RESET_SCL)) hp->default_reset_flags |= E_MODIFY_FLG_SCALE|E_MODIFY_FLG_NU_SCALE;
                           if(IsDlgButtonChecked(hwnd, IDC_RESET_LINK)) hp->default_reset_flags |= E_MODIFY_FLG_LINK;
                           hp->reset_flags &= hp->default_reset_flags;
                        }
                        EndDialog(hwnd, 1);
                        break;
                     }
                     break;
                  }
                  return 0;
               }
            } hlp = {reset_flags, default_reset_flags, ed->GetIGraph(), frm->GetName()};
            int i = DialogBoxParam(GetHInstance(), "IDD_MODIFY_RESET", (HWND)ed->GetIGraph()->GetHWND(),
               S_hlp::dlgProc, (LPARAM)&hlp);
            if(!i) break;
            reset_flags = hlp.reset_flags;
            default_reset_flags = hlp.default_reset_flags;
            if(!reset_flags)
               continue;
         }

         RemoveFlagsAndReset(frm, reset_flags);

         e_slct->FlashFrame(frm);
         ed->SetModified();
      }
   }

//----------------------------

   virtual const char *GetFrameSavedLink(CPI3D_frame frm) const{

      t_item_map::const_iterator it = items.find((PI3D_frame)frm);
      if(it==items.end())
         return NULL;
      if(!((*it).second.flags&E_MODIFY_FLG_LINK)) return NULL;
      return (*it).second.link_name;
   }

//----------------------------

   virtual void EnumModifies(t_modify_enum *fnc, dword context){

      t_item_map::iterator it;
      for(it=items.begin(); it!=items.end(); it++){
         C_smart_ptr<I3D_frame> frm = it->first;
         (*fnc)(frm, it->second.flags, context);
      }
   }

//----------------------------

   virtual void AddNotify(PC_editor_item ei, dword message){

      for(dword i=notify_list.size(); i--; )
         if(ei==notify_list[i].first)
            return;
      notify_list.push_back(pair<C_smart_ptr<C_editor_item>, dword>(ei, message));
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

   virtual void AddLockFlags(PI3D_frame frm, dword flags){

      t_lock_map::iterator it = lock_items.find(frm);
      if(it==lock_items.end()){
         lock_items[frm] = flags;
      }else 
         it->second |= flags;
   }

//----------------------------

   virtual void RemoveLockFlags(PI3D_frame frm, dword flags){

      t_lock_map::iterator it = lock_items.find(frm);
      if(it!=lock_items.end()){
         it->second &= ~flags;
                              //remove from list if no more locked
         if(!it->second)
            lock_items.erase(it);
      }
   }

//----------------------------

   virtual dword GetLockFlags(CPI3D_frame frm) const{

      t_lock_map::const_iterator it = lock_items.find(const_cast<PI3D_frame>(frm));
      if(it!=lock_items.end())
         return it->second;
      return 0;
   }

//----------------------------

   virtual void ClearAllLocks(){

      lock_items.clear();
   }

//----------------------------

   virtual void Undo(dword undo_id, PI3D_frame frm, C_chunk &ck){

      switch(undo_id){
      case UNDO_ADD_FRAME:
      case UNDO_REM_FRAME:
         {
            PI3D_scene scn = ed->GetScene();
            /*
            C_str n = ck.ReadString();
            PI3D_frame frm = scn->FindFrame(n);
            if(!frm){
               ed->Message(C_xstr("Modify(UNDO_ADD_FRAME): can't find frame '%'") %n);
               break;
            }
            */
                              //check which flags we modify/reset
            dword flags = ck.ReadDword();

            if(undo_id==UNDO_REM_FRAME){
               RemoveFlagsAndReset(frm, flags);
            }else{
                                 //find in list (create if not yet)
               t_item_map::iterator it = items.find(frm);
               if(it==items.end())
                  it = items.insert(t_item_map::value_type(frm, S_modify_item())).first;
               S_modify_item &itm = it->second;

               C_chunk &ck_undo = e_undo->Begin(this, UNDO_REM_FRAME, frm);
               //ck_undo.Write(n);
               ck_undo.Write(flags);
               e_undo->End();

               SaveItemData(frm, itm, flags);
               
               dword f = flags;
               if(f&E_MODIFY_FLG_POSITION){
                  frm->SetPos(ck.ReadVector());
                  f &= ~E_MODIFY_FLG_POSITION;
               }
               if(f&E_MODIFY_FLG_ROTATION){
                  frm->SetRot(ck.ReadQuaternion());
                  f &= ~E_MODIFY_FLG_ROTATION;
               }
               if(f&E_MODIFY_FLG_SCALE){
                  frm->SetScale(ck.ReadFloat());
                  f &= ~E_MODIFY_FLG_SCALE;
               }
               if(f&E_MODIFY_FLG_NU_SCALE){
                  switch(frm->GetType()){
                  case FRAME_VOLUME: I3DCAST_VOLUME(frm)->SetNUScale(ck.ReadVector()); break;
                  default: assert(0);
                  }
                  f &= ~E_MODIFY_FLG_NU_SCALE;
               }
               if(f&E_MODIFY_FLG_LINK){
                  C_str n = ck.ReadString();
                  PI3D_frame prev_link = scn->FindFrame(n);
                  if(!prev_link) ed->Message(C_xstr("Reset: can't find link: '%'") %n);
                  else frm->LinkTo(prev_link);
                  f &= ~E_MODIFY_FLG_LINK;
               }
               if(f&E_MODIFY_FLG_COL_MAT){
                  switch(frm->GetType()){
                  case FRAME_VOLUME: I3DCAST_VOLUME(frm)->SetCollisionMaterial(ck.ReadDword()); break;
                  case FRAME_VISUAL: I3DCAST_VISUAL(frm)->SetCollisionMaterial(ck.ReadDword()); break;
                  default: assert(0);
                  }
                  f &= ~E_MODIFY_FLG_COL_MAT;
               }
               if(f&E_MODIFY_FLG_BRIGHTNESS){
                  switch(frm->GetType()){
                  case FRAME_MODEL: SetModelBrightness(I3DCAST_MODEL(frm), ck.ReadFloat()); break;
                  default: assert(0);
                  }
                  f &= ~E_MODIFY_FLG_BRIGHTNESS;
               }
               if(f&E_MODIFY_FLG_VOLUME_TYPE){
                  I3DCAST_VOLUME(frm)->SetVolumeType((I3D_VOLUMETYPE)ck.ReadDword());
                  f &= ~E_MODIFY_FLG_VOLUME_TYPE;
               }
               if(f&E_MODIFY_FLG_FRM_FLAGS){
                  frm->SetFlags(ck.ReadDword());
                  f &= ~E_MODIFY_FLG_FRM_FLAGS;
               }
               f &= ~(E_MODIFY_FLG_SECTOR | E_MODIFY_FLG_HIDE | E_MODIFY_FLG_LIGHT | E_MODIFY_FLG_SOUND);
               assert(!f);     //make sure we didn't forget anything

               itm.flags |= flags;
               SendNotify(frm, flags);
               e_slct->FlashFrame(frm);
               ed->SetModified();
            }
         }
         break;
      default: assert(0);
      }
   }

//----------------------------

   virtual void BeforeFree(){

      items.clear();
      lock_items.clear();
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==1){
         if(items.size()){

            ck <<= CT_MODIFICATIONS;
                              //sort them by number of parents due to link dependency
            C_sort_list<CPI3D_frame> mod_list(items.size());
            t_item_map::const_iterator it;
            for(it=items.begin(); it!=items.end(); ++it){
               CPI3D_frame frm = it->first;
               int sort_value = 0;

               dword nump = 0;
               for(CPI3D_frame prnt = frm; prnt=prnt->GetParent(), prnt; nump++);
                              //on the same level, save models earlier than other,
                              // because they may contain sectors, on which other
                              // things may be dependent
               if(frm->GetType()!=FRAME_MODEL){
                  ++sort_value;

                  if(it->second.flags&E_MODIFY_FLG_LINK){
                              //model's children sort by it's model, because they may be re-linked
                     C_str fname = frm->GetName();
                     const char *dot_pos = strchr(fname, '.');
                     if(dot_pos){
                              //find real frame's model
                        fname[(dword)(dot_pos - fname)] = 0;
                        PI3D_frame prnt = ed->GetScene()->FindFrame(fname, ENUMF_MODEL);
                        if(prnt){
                           for(CPI3D_frame p1 = frm; p1=p1->GetParent(), p1; ){
                              if(p1==prnt)
                                 break;
                           }
                           if(!p1){
                              nump = 0;
                              for(; prnt=prnt->GetParent(), prnt; nump++);
                              ++nump;
                           }
                        }
                     }
                  }
               }
               
               sort_value += nump*2;
               //mod_list.Add(&(*it).second, sort_value);
               mod_list.Add(frm, sort_value);
            }
            mod_list.Sort();

            for(dword i=0; i<mod_list.Count(); i++){
               CPI3D_frame frm = mod_list[i];
               const S_modify_item &itm = items[const_cast<PI3D_frame>(frm)];

                              //don't save temporary frames
               if(itm.flags&E_MODIFY_FLG_TEMPORARY)
                  continue;

               dword flags = itm.flags;
                              //check if it's scene-frame or explicitly created frame
               bool created = (itm.flags&E_MODIFY_FLG_CREATE);

               ck <<= CT_MODIFICATION;

               if(created){
                  ck.WIntChunk(CT_FRAME_TYPE, frm->GetType());
                  if(frm->GetType()==FRAME_VISUAL)
                     ck.WIntChunk(CT_FRAME_SUBTYPE, I3DCAST_CVISUAL(frm)->GetVisualType());
               }
                              //write frame's name
               ck(CT_NAME, frm->GetName());

               switch(frm->GetType()){

               case FRAME_MODEL:
                  {
                     CPI3D_model model = I3DCAST_CMODEL(frm);
                              //write model's file name
                     const C_str &str = model->GetFileName();
                     ck(CT_MODIFY_MODELFILENAME, str);
                              //write brightness
                     float brg = GetModelBrightness(model);
                     if(brg!=1.0f)
                        ck(CT_MODIFY_BRIGHTNESS, brg);
                  }
                  break;

               case FRAME_LIGHT:
                  {
                     CPI3D_light lp = I3DCAST_CLIGHT(frm);
                              //write all light's params
                     ck <<= CT_MODIFY_LIGHT;
                     {
                        ck.WIntChunk(CT_LIGHT_TYPE, lp->GetLightType());
                        ck.WVectorChunk(CT_COLOR, lp->GetColor());
                        ck.WFloatChunk(CT_LIGHT_POWER, lp->GetPower());
                        {     //cone
                           pair<float, float> c;
                           lp->GetCone(c.first, c.second);
                           ck(CT_LIGHT_CONE, c);
                        }
                        {     //range
                           pair<float, float> r;
                           lp->GetRange(r.first, r.second);
                           ck(CT_LIGHT_RANGE, r);
                        }
                        ck.WIntChunk(CT_LIGHT_MODE, lp->GetMode());

                              //sector names
                        for(int i=lp->NumLightSectors(); i--; ){
                           PI3D_sector sct = lp->GetLightSectors()[i];
                           ck.WStringChunk(CT_LIGHT_SECTOR, sct->GetName());
                        }
                     }
                     --ck;
                  }
                  break;

               case FRAME_SOUND:
                  {
                     CPI3D_sound sp = I3DCAST_CSOUND(frm);
                                 //write all sound's params
                     ck <<= CT_MODIFY_SOUND;
                     {
                        if(sp->IsStreaming()){  //must be before filename!
                           ck <<= CT_SOUND_STREAMING;
                           --ck;
                        }

                        C_str name = sp->GetFileName();
                        ed->GetSoundCache().GetRelativeDirectory(&name[0]);
                        ck.WStringChunk(CT_NAME, name);

                        ck.WIntChunk(CT_SOUND_TYPE, sp->GetSoundType());
                        ck.WFloatChunk(CT_SOUND_VOLUME, sp->GetVolume());
                        ck.WFloatChunk(CT_SOUND_OUTVOL, sp->GetOutVol());
                        {     //cone
                           pair<float, float> c;
                           sp->GetCone(c.first, c.second);
                           ck(CT_SOUND_CONE, c);
                        }
                        {     //range
                           pair<float, float> r;
                           sp->GetRange(r.first, r.second);
                           ck(CT_SOUND_RANGE, r);
                        }
                        if(sp->IsLoop()){
                           ck <<= CT_SOUND_LOOP;
                           --ck;
                        }
                        if(sp->IsOn()){
                           ck <<= CT_SOUND_ENABLE;
                           --ck;
                        }

                              //sector names
                        for(int i=sp->NumSoundSectors(); i--; ){
                           PI3D_sector sct = sp->GetSoundSectors()[i];
                           ck.WStringChunk(CT_SOUND_SECTOR, sct->GetName());
                        }
                     }
                     --ck;
                  }
                  break;

               case FRAME_VOLUME:
                  {
                     CPI3D_volume vol = I3DCAST_CVOLUME(frm);
                     dword mf = itm.flags;
                     if(mf&E_MODIFY_FLG_COL_MAT){
                        if(vol->GetCollisionMaterial()==itm.col_mat)
                           mf &= ~E_MODIFY_FLG_COL_MAT;
                     }
                     if(mf&(E_MODIFY_FLG_VOLUME_TYPE|E_MODIFY_FLG_COL_MAT)){
                                 //write all volume's params
                        ck <<= CT_MODIFY_VOLUME;

                        if(mf&E_MODIFY_FLG_VOLUME_TYPE)
                           ck(CT_VOLUME_TYPE, (int)vol->GetVolumeType());
                        if(mf&E_MODIFY_FLG_COL_MAT)
                           ck(CT_VOLUME_MATERIAL, (int)vol->GetCollisionMaterial());
                        --ck;
                     }
                  }
                  break;

               case FRAME_OCCLUDER:
                  {
                     CPI3D_occluder occ = I3DCAST_COCCLUDER(frm);
                     ck <<= CT_MODIFY_OCCLUDER;
                     switch(occ->GetOccluderType()){
                     case I3DOCCLUDER_MESH:
                        {
                           ck <<= CT_OCCLUDER_VERTICES;
                           {
                              dword numv = occ->NumVertices();
                              ck.Write(numv);
                              const S_vector *vp = const_cast<PI3D_occluder>(occ)->LockVertices();
                              ck.Write(vp, numv*sizeof(S_vector));
                              const_cast<PI3D_occluder>(occ)->UnlockVertices();
                           }
                           --ck;
                        }
                        break;
                     }
                     --ck;
                  }
                  break;

               case FRAME_SECTOR:
                  {
                     CPI3D_sector sct = I3DCAST_CSECTOR(frm);
                     dword env_id = sct->GetEnvironmentID();
                     float temp = sct->GetTemperature();
                     if(env_id || temp){
                        ck <<= CT_MODIFY_SECTOR;
                        if(env_id)
                           ck(CT_SECTOR_ENVIRONMENT, (word)env_id);
                        if(temp)
                           ck(CT_SECTOR_TEMPERATURE, temp);
                        --ck;
                     }
                  }
                  break;

               }
                              //write position, rotation & scale
               if(flags&E_MODIFY_FLG_POSITION){
                  const S_vector *pos;
                  if((flags&E_MODIFY_FLG_CREATE) && !(flags&E_MODIFY_FLG_LINK)){
                     pos = &frm->GetWorldPos();
                  }else{
                     pos = &frm->GetPos();
                  }
                              //check redundancy
                  if(*pos!=itm.pos){
                     ck.WVectorChunk(CT_POSITION, *pos);
                              //save world pos, for case link would fail
                     ck.WVectorChunk(CT_WORLD_POSITION, frm->GetWorldPos());
                  }
               }
               if(flags&E_MODIFY_FLG_ROTATION){
                  /*
                  const S_quat *rot;
                  if((flags&E_MODIFY_FLG_CREATE) && !(flags&E_MODIFY_FLG_LINK))
                     rot = &frm->GetWorldRot();
                  else
                     rot = &frm->GetRot();
                  if(!(*rot == itm.rot))
                     ck.WRotChunk(CT_QUATERNION, *rot);
                  */
                  const S_quat &rot = frm->GetRot();
                  if(!(rot == itm.rot))
                     ck.WRotChunk(CT_QUATERNION, rot);
               }
               if(flags&E_MODIFY_FLG_SCALE){
                  const float scl = frm->GetScale();
                  if(scl != itm.scale)
                     ck.WFloatChunk(CT_USCALE, scl);
               }
               if(flags&E_MODIFY_FLG_NU_SCALE){
                  switch(frm->GetType()){
                  case FRAME_VOLUME:
                     {
                        const S_vector &scl = I3DCAST_CVOLUME(frm)->GetNUScale();
                        if(scl != itm.nu_scale)
                           ck.WVectorChunk(CT_NUSCALE, scl);
                     }
                     break;
                  }
               }
               if(flags&E_MODIFY_FLG_LINK){
                  PI3D_frame prnt = frm->GetParent();
                  if(prnt && prnt->GetName()!=itm.link_name){
                     ck <<= CT_LINK;
                     ck.WStringChunk(CT_NAME, prnt->GetName());
                     --ck;
                  }
               }
               if(flags&E_MODIFY_FLG_HIDE){
                  ck <<= CT_HIDDEN;
                  --ck;
               }
               if(flags&E_MODIFY_FLG_VISUAL){
                  CPI3D_visual vis = I3DCAST_CVISUAL(frm);
                              //write visual type
                  ck.WIntChunk(CT_MODIFY_VISUAL, vis->GetVisualType());

                  switch(vis->GetVisualType()){
                  case I3D_VISUAL_LIT_OBJECT:
                     {
                        PI3D_lit_object lmap = (PI3D_lit_object)vis;
                        //if(lmap->IsConstructed())
                        {
                           ck <<= CT_MODIFY_LIT_OBJECT;
                           lmap->Save(ck.GetHandle());
                           --ck;
                        }
                     }
                     break;
                  }
               }
               if(frm->GetType() == FRAME_VISUAL){
                  CPI3D_visual vis = I3DCAST_CVISUAL(frm);

                              //visual's material
                  if(itm.flags&E_MODIFY_FLG_COL_MAT){
                     if(vis->GetCollisionMaterial()!=itm.col_mat)
                        ck(CT_VISUAL_MATERIAL, (dword)vis->GetCollisionMaterial());
                  }

                              //write visual's properties
                  dword vtype = vis->GetVisualType();
                  dword num_props = ed->GetDriver()->NumProperties(vtype);
                  for(dword pi=0; pi<num_props; pi++){
                     I3D_PROPERTYTYPE ptype = ed->GetDriver()->GetPropertyType(vtype, pi);
                     if(ptype==I3DPROP_NULL)
                        continue;
                     ck <<= CT_MODIFY_VISUAL_PROPERTY;
                     ck.Write(&ptype, sizeof(byte));
                     assert(pi < 65536);
                     ck.Write(&pi, sizeof(word));
                              //write value
                     dword value = vis->GetProperty(pi);
                     int GetPropertySize(I3D_PROPERTYTYPE, dword);   //definition
                     dword len = GetPropertySize(ptype, value);
                     switch(ptype){
                     case I3DPROP_STRING:
                     case I3DPROP_VECTOR3:
                     case I3DPROP_VECTOR4:
                     case I3DPROP_COLOR:
                        ck.Write((const void*)value, len);
                        break;
                     default:
                        ck.Write(&value, len);
                     }
                     --ck;
                  }
               }
                              //write frame flags
               if(flags&E_MODIFY_FLG_FRM_FLAGS)
                  ck.WIntChunk(CT_FRAME_FLAGS, frm->GetFlags());

               --ck;     //CT_FRAME
            }
            --ck;
         }
      }
   }

//----------------------------

public:
   C_edit_Modify():
      default_reset_flags(E_MODIFY_FLG_ALL)
   {}

   virtual bool Init(){
                              //find required plugins
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      if(!e_undo || !e_slct){
         e_undo = NULL;
         e_slct = NULL;
         return false;
      }
      e_undo->AddRef();
      e_slct->AddRef();

      ed->AddShortcut(this, E_MODIFY_RESET, "%10 &Edit\\%80 Reset modify\tHome", K_HOME, 0);
      return true;
   }

   virtual void Close(){
      if(e_undo) e_undo->Release();
      if(e_slct) e_slct->Release();
      e_undo = NULL;
      e_slct = NULL;
      items.clear();

      notify_list.clear();
   }

   //virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}

   virtual dword Action(int id, void *context){

      switch(id){

      case E_MODIFY_RESET:
         ResetFlagsOnSelection();
         break;
      }
      return 0;
   }
   //virtual void Render(){}
   //virtual bool LoadState(C_chunk&){ return false; }
   //virtual bool SaveState(C_chunk&) const{ return false; }
};

//----------------------------

void CreateModify(PC_editor ed){
   PC_editor_item ei = new C_edit_Modify;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
