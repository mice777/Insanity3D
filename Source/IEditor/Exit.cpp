#include "all.h"
#include "common.h"

//----------------------------

class C_editor_item_Exit_imp: public C_editor_item_Exit{
   virtual const char *GetName() const{ return "Exit"; }
   bool b_exit;

   enum{
      ACTION_EXIT,
   };

//----------------------------

   virtual bool GetState() const{ return b_exit; }

//----------------------------

   virtual void SetState(bool b){ b_exit = b; }

//----------------------------
public:
   virtual bool Init(){
      ed->AddShortcut(this, ACTION_EXIT, "%0 &File\\%100 %i E&xit\tShit+Esc", K_ESC, SKEY_SHIFT);

      b_exit = false;
      return true;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}
   virtual dword Action(int id, void *context){

      switch(id){
      case ACTION_EXIT:
         SetState(true);
         break;
      }
      return 0;
   }
};

//----------------------------

void CreateExit(PC_editor ed){
   PC_editor_item ei = new C_editor_item_Exit_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
