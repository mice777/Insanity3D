#include "all.h"
#include "common.h"

//----------------------------

class C_edit_Hide: public C_editor_item{
   virtual const char *GetName() const{ return "Hide"; }

   enum E_ACTION_HIDE{           
      E_HIDE_SELECTION = 5000,   // - hide selected frames
      E_HIDE_UNHIDE_ALL,         // - unhide all hidden frames
      E_HIDE_UNHIDE_PROMPT,      // - ask user which frames to unhide

      E_HIDE_NOTIFY_SELECTION,   //(C_vector<PI3D_frame> *sel_list) - notification received from selection
   };

   enum{
      UNDO_HIDE,
      UNDO_UNHIDE,
   };

//----------------------------

   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;


//----------------------------

   virtual void Undo(dword undo_id, PI3D_frame frm, C_chunk &ck){

      switch(undo_id){
      case UNDO_HIDE:
         e_undo->Begin(this, UNDO_UNHIDE, frm);
         e_undo->End();
         frm->SetOn(false);                
         e_modify->AddFrameFlags(frm, E_MODIFY_FLG_HIDE);
         break;

      case UNDO_UNHIDE:
         e_undo->Begin(this, UNDO_HIDE, frm);
         e_undo->End();
         frm->SetOn(true);                
         e_modify->RemoveFlags(frm, E_MODIFY_FLG_HIDE);
         break;
      }
   }

//----------------------------
public:
   C_edit_Hide()
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
      e_undo->AddRef();
      e_slct->AddRef();

      ed->AddShortcut(this, E_HIDE_SELECTION, "%10 &Edit\\%60 %i &Hide\tH", K_H, 0);
      ed->AddShortcut(this, E_HIDE_UNHIDE_PROMPT, "&Edit\\%60 &Unhide by name\tCtrl+H", K_H, SKEY_CTRL);
      ed->AddShortcut(this, E_HIDE_UNHIDE_ALL, "&Edit\\%60 Unh&ide all\tCtrl+Alt+H", K_H, SKEY_CTRL | SKEY_ALT);

                             //receive notifications from selection
      e_slct->AddNotify(this, E_HIDE_NOTIFY_SELECTION);
      return true;
   }

//----------------------------

   virtual void Close(){
      if(e_slct)
         e_slct->RemoveNotify(this);

      if(e_undo) e_undo->Release();
      if(e_slct) e_slct->Release();
      e_undo = NULL;
      e_slct = NULL;
      e_modify = NULL;
   }

   virtual dword C_edit_Hide::Action(int id, void *context){

      switch(id){
      case E_HIDE_SELECTION:
         {
            if(!ed->CanModify()) break;
            bool on = false;//(id==0);

            const C_vector<PI3D_frame> sel_list = e_slct->GetCurSel();

            if(!sel_list.size()) break;

            dword num = 0;
            for(dword i=0; i<sel_list.size(); i++){
               PI3D_frame frm = sel_list[i];
               switch(frm->GetType()){
               case FRAME_VISUAL:
               case FRAME_DUMMY:
               case FRAME_MODEL:
               case FRAME_SECTOR:
                  if(on != (bool)frm->IsOn()){
                     frm->SetOn(on);
                     if(!on){
                        e_modify->AddFrameFlags(frm, E_MODIFY_FLG_HIDE);
                        e_slct->RemoveFrame(frm);
                              //save undo info
                        e_undo->Begin(this, UNDO_UNHIDE, frm);
                        e_undo->End();
                     }
                     ++num;
                     ed->SetModified();
                  }
                  break;
               }
            }
            ed->Message(C_fstr("%s: %i object(s)", (on ? "Unhidden" : "Hidden"), num));
         }
         break;

      case E_HIDE_UNHIDE_PROMPT:
         {
            if(!ed->CanModify()) break;
                              //collect list of hidden frames
            C_vector<PI3D_frame> h_list;
            int i;
            
            struct S_hlp{
               static void cbEnum(PI3D_frame frm, dword flags, dword c){
                  if(flags&E_MODIFY_FLG_HIDE){
                     ((C_vector<PI3D_frame>*)c)->push_back(frm);
                  }
               }
            };
            e_modify->EnumModifies(S_hlp::cbEnum, (dword)&h_list);
            if(!h_list.size())
               break;
                              //perform for selection
            i = e_slct->Prompt(h_list);
            if(!i || !h_list.size())
               break;

            e_slct->Clear();

            for(i=h_list.size(); i--; ){
               PI3D_frame frm = h_list[i];
                              //unhide this
               frm->SetOn(true);                
               e_modify->RemoveFlags(frm, E_MODIFY_FLG_HIDE);

               e_slct->AddFrame(frm);

                              //save undo info
               e_undo->Begin(this, UNDO_HIDE, frm);
               e_undo->End();

               ed->SetModified();
            }
            ed->Message(C_fstr("UnHidden %i object(s)", h_list.size()));
         }
         break;

      case E_HIDE_UNHIDE_ALL:
         {
            if(!ed->CanModify()) break;
            e_slct->Clear();

            dword num_unhidden = 0;
            C_vector<PI3D_frame> unhide_list;
            int i;

            struct S_hlp{
               static void cbEnum(PI3D_frame frm, dword flags, dword c){
                  if(flags&E_MODIFY_FLG_HIDE){
                     ((C_vector<PI3D_frame>*)c)->push_back(frm);
                  }
               }
            };
            e_modify->EnumModifies(S_hlp::cbEnum, (dword)&unhide_list);

            for(i=unhide_list.size(); i--; ){
               PI3D_frame frm = unhide_list[i];
               frm->SetOn(true);                
               e_modify->RemoveFlags(frm, E_MODIFY_FLG_HIDE);

               e_slct->AddFrame(frm);

                              //save undo info
               e_undo->Begin(this, UNDO_HIDE, frm);
               e_undo->End();

               ++num_unhidden;
               ed->SetModified();
            }
            if(num_unhidden)
               ed->Message(C_fstr("Unhidden %i object(s)", num_unhidden));
         }
         break;

      case E_HIDE_NOTIFY_SELECTION:
         {
            C_vector<PI3D_frame> *sel_list = (C_vector<PI3D_frame>*)context;
            ed->EnableMenu(this, E_HIDE_SELECTION, (sel_list->size()!=0));
         }
         break;
      }
      return 0;
   }
};

//----------------------------

void CreateHide(PC_editor ed){
   PC_editor_item ei = new C_edit_Hide;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
