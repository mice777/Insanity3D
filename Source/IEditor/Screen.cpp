#include "all.h"
#include <d3d9.h>
#include "common.h"

//----------------------------

class C_edit_Screen: public C_editor_item{
   virtual const char *GetName() const{ return "Screen"; }

//----------------------------

   enum E_ACTION_SCREEN{
      E_SCREEN_FULLSCREEN = 6000,// - toggle full-screen mode
      E_SCREEN_NEXT_RES,         // - cycle forward through available resolutions
      E_SCREEN_PREV_RES,         // - cycle backward through available resolutions
   };

//----------------------------

   virtual void OnDeviceReset(){
      SetupViewport();
   }

//----------------------------

public:
   struct S_display_mode{
      dword sx, sy, bpp;
      bool operator <(const S_display_mode &dm) const{
         return ((bpp*10000000 + (sx*sy)) < (dm.bpp*10000000 + (dm.sx*dm.sy)));
      }
      bool operator ==(const S_display_mode &dm) const{
         return (sx==dm.sx && sy==dm.sy && bpp==dm.bpp);
      }
   };

   static dword I2DAPI cbGraph(dword msg, dword par1, dword par2, void *context){
      switch(msg){
      case CM_RECREATE:
         switch(par2){
         case 2:
            {
               C_edit_Screen *es = (C_edit_Screen*)context;
               es->ed->Broadcast(&C_editor_item::OnDeviceReset);

               if(es->curr_disp_mode!=-1)
                  es->ed->CheckMenu(es, 1100+es->curr_disp_mode, false);
               es->curr_disp_mode = -1;
               for(int i=es->disp_modes.size(); i--; ){
                  const S_display_mode &dm = es->disp_modes[i];
                  if(dm.sx==es->ed->GetIGraph()->Scrn_sx() && dm.sy==es->ed->GetIGraph()->Scrn_sy() &&
                     dm.bpp==(es->ed->GetIGraph()->GetPixelFormat()->bytes_per_pixel * 8)){

                     es->curr_disp_mode = i;
                     es->ed->CheckMenu(es, 1100+i, true);
                     break;
                  }
               }
            }
            break;
         }
         break;
      }
      return 0;
   }

   void SetupViewport(){
      /*
      PIGraph igraph = ed->GetIGraph();
      if(igraph->GetFlags()&IG_FULLSCREEN){
         int menu_sy = GetSystemMetrics(SM_CYMENU);
         HWND hwnd_sb = (HWND)ed->FindPlugin("StatusBar")->Action(E_STATUSBAR_GET_HWND);
         RECT rc;
         GetWindowRect(hwnd_sb, &rc);
         int sb_sy = (rc.bottom-rc.top);
         ed->GetScene()->SetViewport(0, menu_sy, 
            igraph->Scrn_sx(), igraph->Scrn_sy()-sb_sy);
      }
      */
   }

private:
   C_vector<S_display_mode> disp_modes;
   int curr_disp_mode;
   bool init_ok;

   void ShowInfo(const S_display_mode &dm){
                              //get frequency
      dword freq = 0;
      ed->Message(C_fstr("Display mode: %ix%i %i-bit %iHz", dm.sx, dm.sy, dm.bpp, freq), false);
   }
public:
   C_edit_Screen():
      init_ok(false)
   {}

   virtual bool Init(){
      //ed->AddShortcut(this, E_SCREEN_FULLSCREEN, "&View\\%90 %i S&creen\\Fullscreen\tCtrl+Grey*", K_GREYMULT, SKEY_CTRL);
      ed->AddShortcut(this, E_SCREEN_NEXT_RES, "&View\\%90 %i S&creen\\Next resolution\tCtrl+Grey+", K_GREYPLUS, SKEY_CTRL);
      ed->AddShortcut(this, E_SCREEN_PREV_RES, "&View\\S&creen\\%a Previous resolution\tCtrl+Grey-", K_GREYMINUS, SKEY_CTRL);

                              //setup callback to get notified about moe change
      ed->GetIGraph()->AddCallback(cbGraph, this);

                              //enum all display modes
      IDirect3D9 *lpD3D9 = ed->GetIGraph()->GetD3DInterface();
      IDirect3DDevice9 *lpDev9 = ed->GetIGraph()->GetD3DDevice();
      D3DDEVICE_CREATION_PARAMETERS cp;
      lpDev9->GetCreationParameters(&cp);

      D3DDISPLAYMODE curr_mode;
      lpD3D9->GetAdapterDisplayMode(0, &curr_mode);

      int adapter_index = cp.AdapterOrdinal;
      for(dword i=0; i<lpD3D9->GetAdapterModeCount(adapter_index, curr_mode.Format); i++){
         D3DDISPLAYMODE dm;
         lpD3D9->EnumAdapterModes(adapter_index, curr_mode.Format, i, &dm);
         S_display_mode dm1;
         dm1.sx = dm.Width;
         dm1.sy = dm.Height;
         dm1.bpp = 0;

         switch(dm.Format){
         case D3DFMT_R5G6B5:
         case D3DFMT_X1R5G5B5:
            dm1.bpp = 16;
            break;
         case D3DFMT_R8G8B8:
            dm1.bpp = 24;
            break;
         case D3DFMT_X8R8G8B8:
            dm1.bpp = 32;
            break;
         }
         if(!dm1.bpp)
            continue;
                              //check if already in
         for(int j=disp_modes.size(); j--; ){
            if(disp_modes[j] == dm1)
               break;
         }
         if(j==-1){
            disp_modes.push_back(dm1);
         }
      }

      sort(disp_modes.begin(), disp_modes.end());
      curr_disp_mode = -1;
      for(i=0; i<disp_modes.size(); i++){
         ed->AddShortcut(this, 1100+i, C_fstr("&View\\S&creen\\%80 %ix%i %i-bit", disp_modes[i].sx, disp_modes[i].sy, disp_modes[i].bpp), K_NOKEY, 0);
                              //find out which mode is set now
         if(disp_modes[i].sx==ed->GetIGraph()->Scrn_sx() && disp_modes[i].sy==ed->GetIGraph()->Scrn_sy() &&
            disp_modes[i].bpp==ed->GetIGraph()->GetPixelFormat()->bytes_per_pixel * 8){

            curr_disp_mode = i;
            ed->CheckMenu(this, 1100+i, true);
         }
      }
      //ed->CheckMenu(this, E_SCREEN_FULLSCREEN, ed->GetIGraph()->GetFlags()&IG_FULLSCREEN);

      SetupViewport();

      init_ok = true;

      return true;
   }

   virtual void Close(){ 
      if(init_ok){
                              //remove IGraph callback
         if(ed)
            ed->GetIGraph()->RemoveCallback(cbGraph, this);
         init_ok = false;
      }
   }

   virtual dword C_edit_Screen::Action(int id, void *context){

      int i;
      switch(id){
         /*
      case E_SCREEN_FULLSCREEN:
         {
            dword flg = ed->GetIGraph()->GetFlags();
            flg ^= IG_FULLSCREEN;
            ed->GetIGraph()->UpdateParams(0, 0, 0, flg, IG_FULLSCREEN);
            ed->CheckMenu(this, E_SCREEN_FULLSCREEN, ed->GetIGraph()->GetFlags()&IG_FULLSCREEN);
         }
         break;
         */

      case E_SCREEN_NEXT_RES:
      case E_SCREEN_PREV_RES:              
         {
            if(!disp_modes.size())
               break;
            if(curr_disp_mode!=-1)
               ed->CheckMenu(this, 1100 + curr_disp_mode, false);
            else{
               int curr_delta = 100000000;
               for(i=disp_modes.size(); i--; ){
                  const S_display_mode &dm = disp_modes[i];
                  if(dm.bpp==(ed->GetIGraph()->GetPixelFormat()->bytes_per_pixel * 8)){
                     int delta = abs(int(dm.sx) - int(ed->GetIGraph()->Scrn_sx())) +
                        abs(int(dm.sy) - int(ed->GetIGraph()->Scrn_sy()));
                     if(curr_delta > delta){
                        curr_delta = delta;
                        curr_disp_mode = i;
                     }
                  }
               }
            }

            if(id==E_SCREEN_NEXT_RES) ++curr_disp_mode %= disp_modes.size();
            else{
               if(curr_disp_mode<=0) curr_disp_mode = disp_modes.size();
               --curr_disp_mode;
            }
            const S_display_mode &dm = disp_modes[curr_disp_mode];
            ed->GetIGraph()->UpdateParams(dm.sx, dm.sy, dm.bpp, 0, 0);
            ShowInfo(dm);

                              //find out which mode is set now
            for(i=disp_modes.size(); i--; ){
               const S_display_mode &dm = disp_modes[i];
               if(dm.sx==ed->GetIGraph()->Scrn_sx() && dm.sy==ed->GetIGraph()->Scrn_sy() &&
                  dm.bpp==(ed->GetIGraph()->GetPixelFormat()->bytes_per_pixel * 8)){

                  curr_disp_mode = i;
                  ed->CheckMenu(this, 1100+i, true);
                  break;
               }
            }
         }
         break;

      default:
         {
            dword i = id-1100;
            if(i>=0 && i<disp_modes.size()){
               if(curr_disp_mode!=-1)
                  ed->CheckMenu(this, 1100+curr_disp_mode, false);
               const S_display_mode &dm = disp_modes[i];
               ed->GetIGraph()->UpdateParams(dm.sx, dm.sy, dm.bpp, 0, 0);
               ShowInfo(dm);
                                 //find out which mode is set now
               for(i=disp_modes.size(); i--; ){
                  const S_display_mode &dm = disp_modes[i];
                  if(dm.sx==ed->GetIGraph()->Scrn_sx() && dm.sy==ed->GetIGraph()->Scrn_sy() &&
                     dm.bpp==(ed->GetIGraph()->GetPixelFormat()->bytes_per_pixel * 8)){

                     curr_disp_mode = i;
                     ed->CheckMenu(this, 1100+i, true);
                     break;
                  }
               }
            }
         }
      }
      return 0;
   }
};

//----------------------------

void CreateScreen(PC_editor ed){
   PC_editor_item ei = new C_edit_Screen;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
