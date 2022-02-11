#include "all.h"
#include "common.h"

//----------------------------

//#define SCREENSHOT_TYPE "png"
#define SCREENSHOT_TYPE "png"

//----------------------------

class C_edit_ScreenShot: public C_editor_item{
   virtual const char *GetName() const{ return "ScreenShot"; }

//----------------------------

   enum{
      ACTION_SAVE_SHOT,
   };

//----------------------------
public:
   virtual bool Init(){
      ed->AddShortcut(this, ACTION_SAVE_SHOT, "%0 &File\\%75 &Save shot\tCtrl+F12", K_F12, SKEY_CTRL);
      return true;
   }
   //virtual void Close(){ }
   //virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}
   virtual dword Action(int id, void *context){
      switch(id){
      case ACTION_SAVE_SHOT:
         {
            PI3D_driver drv = ed->GetDriver();
            drv->BeginScene();
            ed->GetScene()->Render();
            ed->Render();
            drv->EndScene();

            C_str name;
            {
               char comp_name[128];
               dword dw = 128;
               GetComputerName(comp_name, &dw);
               name = comp_name;
            }
                              //make unique number
            {
               for(int i=0; i<1000; i++){
                  C_fstr s("%s [%.3i]."SCREENSHOT_TYPE, (const char*)name, i);
                  PC_dta_stream dta = DtaCreateStream(s);
                  if(!dta){
                     name = s;
                     break;
                  }else{
                     //dtaClose(h);
                     dta->Release();
                  }
               }
            }
            PIImage bb_img = ed->GetIGraph()->CreateBackbufferCopy(0, 0, true);
            if(bb_img){
               C_cache ck;
               if(ck.open(name, CACHE_WRITE)){
                  bb_img->SaveShot(&ck, SCREENSHOT_TYPE);
                  ck.close();
               }
               bb_img->Release();
            }

            ed->Message(C_fstr("screen saved \"%s\"", (const char*)name));
         }
         break;
      }
      return 0;
   }
   //virtual void Render(){}
};

//----------------------------

void CreateScreenShot(PC_editor ed){
   PC_editor_item ei = new C_edit_ScreenShot;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
