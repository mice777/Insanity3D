#include "all.h"
#include "common.h"

//----------------------------

class C_edit_HelpLight_imp: public C_edit_HelpLight{
   virtual const char *GetName() const{ return "HelpLight"; }

//----------------------------

   enum E_ACTION_LIGHT{
      E_LIGHT_TOGGLE_HELP = 16000,  // - toggle helping light
      E_LIGHT_MAKE_HELP,         // - create helping light
      E_LIGHT_DESTROY_HELP,      // - destroy helping light
   };

//----------------------------

   PI3D_light help_light[2]; //directional, ambient
   bool status;               //status on/off
   bool light_on;             //real lights
   C_smart_ptr<C_toolbar> toolbar;

//----------------------------

   void AddHelpLight(){
      DelHelpLight();
      for(int i=0; i<2; i++){
         PI3D_light lp = I3DCAST_LIGHT(ed->GetScene()->CreateFrame(FRAME_LIGHT));
         help_light[i] = lp;
         lp->SetName("<helping light>");
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
         lp->SetOn(true);
                              //add to all sectors
         struct S_hlp{
            static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword l){
               I3DCAST_SECTOR(frm)->AddLight((PI3D_light)l);
               return I3DENUMRET_OK;
            }
         };
         ed->GetScene()->GetPrimarySector()->AddLight(lp);
         ed->GetScene()->EnumFrames(S_hlp::cbAdd, (dword)lp, ENUMF_SECTOR);
         toolbar->SetButtonPressed(this, E_LIGHT_TOGGLE_HELP, true);
      }
      light_on = true;
   }

//----------------------------

   void DelHelpLight(){
      if(light_on){
         for(int i=0; i<2; i++){
                              //delete of all sectors
            PI3D_sector const *lscts = help_light[i]->GetLightSectors();
            for(int j=help_light[i]->NumLightSectors(); j--; )
            lscts[j]->RemoveLight(help_light[i]);

            help_light[i]->Release();
         }
         light_on = false;
         toolbar->SetButtonPressed(this, E_LIGHT_TOGGLE_HELP, false);
      }
   }

//----------------------------

   virtual void AfterLoad(){

      if(status)
         AddHelpLight();
   }

//----------------------------

   virtual void BeforeFree(){

      DelHelpLight();
   }

//----------------------------

   virtual void AddHelpLight(bool on){

      if(on)
         AddHelpLight();
      else
         DelHelpLight();
      status = on;
   }

//----------------------------

   virtual bool IsHelpLightOn() const{
      return status;
   }

//----------------------------
public:
   C_edit_HelpLight_imp():
      status(false),
      light_on(false)
   {}
   ~C_edit_HelpLight_imp(){ Close(); }

//----------------------------

   virtual bool Init(){

      ed->AddShortcut(this, E_LIGHT_TOGGLE_HELP, "%30 &Debug\\%0 Helping light\tF10", K_F10, 0);
      {
         toolbar = ed->GetToolbar("Debug");
         S_toolbar_button tbs[] = {
            {E_LIGHT_TOGGLE_HELP, 0, "Help light"},
         };
         toolbar->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_LIGHT", GetHInstance(), 20);
      }
      return true;
   }

//----------------------------
   virtual void Close(){
      DelHelpLight();
   }

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){
      //Action(E_LIGHT_TOGGLE_HELP, 0);
   }

   virtual dword Action(int id, void *context){

      switch(id){
      case E_LIGHT_MAKE_HELP:  
         AddHelpLight(false);
         break;

      case E_LIGHT_DESTROY_HELP:
         AddHelpLight(false);
         status = false;
         break;

      case E_LIGHT_TOGGLE_HELP:
         {
            if(status){
               DelHelpLight();
               status = false;
            }else{
               AddHelpLight();
               status = true;
            }
            ed->CheckMenu(this, id, status);
            ed->Message(C_str("Help light ")+(status ? "created" : "destroyed"));
         }
         break;
      }
      return 0;
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){
      byte b = false;
      ck.Read(&b, sizeof(byte));
      status = b;
      ed->CheckMenu(this, 1002, status);
      return true;
   }

   virtual bool SaveState(C_chunk &ck) const{
      ck.Write(&status, sizeof(byte));
      return true;
   }
};

//----------------------------

void CreateLight(PC_editor ed){
   PC_editor_item ei = new C_edit_HelpLight_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
