#include "all.h"
#include "common.h"

//----------------------------

class C_edit_Help: public C_editor_item{
   virtual const char *GetName() const{ return "Help"; }

//----------------------------

   enum E_ACTION_HELP{
      E_HELP_HELP,               //show default help
      E_HELP_ABOUT,              //show about dialog
   };

//----------------------------

   static BOOL CALLBACK dlgAbout(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            C_edit_Help *eh = (C_edit_Help*)lParam;
            InitDlg(eh->ed->GetIGraph(), hwnd);
            ShowWindow(hwnd, SW_SHOW);
            SetWindowLong(hwnd, GWL_USERDATA, lParam);
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL:
         case IDCLOSE:
            {
               EndDialog(hwnd, 0);
            }
            break;
         }
         break;
      }
      return 0;
   }

//----------------------------

   struct S_help_data: public C_unknown{
      PC_editor_item ei;
      dword help_id;
      dword action_id;        //internal action ID
      C_str help;

      S_help_data(){}
      S_help_data(const S_help_data&);
      void operator =(const S_help_data&);
   };
   typedef C_vector<C_smart_ptr<S_help_data> > t_plugin_help;
   t_plugin_help plugin_help;

//----------------------------

   virtual void AddHelp(PC_editor_item ei, dword help_id, const char *menu_title, const char *help_string){

      static dword action_id = 0x80000000;

      RemoveHelp(ei, help_id);
      S_help_data *hd = new S_help_data;
      plugin_help.push_back(hd);
      hd->ei = ei;
      hd->help_id = help_id;
      hd->action_id = action_id++;
      if(action_id++ == 0xc0000000) action_id = 0x80000000;
      hd->help = help_string;
      hd->Release();

      ed->AddShortcut(this, hd->action_id, C_fstr("%%0 %%i &Help\\&%s", menu_title), K_NOKEY, 0);
   }

//----------------------------

   virtual void RemoveHelp(PC_editor_item ei, dword help_id){

      for(dword i=plugin_help.size(); i--; ){
         if(plugin_help[i]->ei==ei && plugin_help[i]->help_id==help_id){
            ed->DelShortcut(this, plugin_help[i]->action_id);
            plugin_help[i] = plugin_help.back(); plugin_help.pop_back();
            break;
         }
      }
   }

//----------------------------

public:

   C_edit_Help()
   {}

   virtual bool Init(){
      PIGraph igraph = ed->GetIGraph();
      if(!igraph->GetMenu()){
         igraph->UpdateParams(0, 0, 0, IG_LOADMENU, IG_LOADMENU);
         igraph->ProcessWinMessages();
      }
                              //add menu items (and keyboard short-cuts)
#define MB "%100 %i &Help\\"
      //ed->AddShortcut(this, E_HELP_HELP, MB"%0 %a &Help\tF1", K_F1, 0);
      ed->AddShortcut(this, E_HELP_ABOUT, MB"%100 %i A&bout", K_NOKEY, 0);
      return true;
   }
   //virtual void Close(){ }
   //virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}

//----------------------------

   virtual dword Action(int id, void *context){
      switch(id){
      case E_HELP_ABOUT:
         DialogBoxParam(GetHInstance(), "IDD_ABOUT", (HWND)ed->GetIGraph()->GetHWND(), dlgAbout, (LPARAM)this);
         break;

      case E_HELP_HELP:
         break;

      default:
                              //check if it's one of registered helps
         if(id>=0x80000000 && id<0xc0000000){
                              //find the menu
            for(dword i=plugin_help.size(); i--; ){
               if(plugin_help[i]->action_id==(dword)id){
                              //display the help
                  PIGraph ig = ed->GetIGraph();
                  int x = 0;
                  int y = 40;
                  const int WIDTH = 400;
                  x = (ig->Scrn_sx() - WIDTH) / 2;
                  OsDisplayHelpWindow(plugin_help[i]->help, ig->GetHWND(), x, y, WIDTH);
                  break;
               }
            }

         }
      }
      return 0;
   }

//----------------------------

   //virtual void Render(){}
   //virtual bool LoadState(class C_chunk&){ return false; }
   //virtual bool SaveState(class C_chunk&) const{ return false; }
};

//----------------------------

void CreateHelp(PC_editor ed){
   PC_editor_item ei = new C_edit_Help;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
