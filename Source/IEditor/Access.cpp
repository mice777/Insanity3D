#include "all.h"
#include "common.h"

//----------------------------

static bool WinGetComputerName(C_str &str){

   char buf[255];
   dword sz = sizeof(buf);
   if(!GetComputerName(buf, &sz)){
      str = NULL;
      return false;
   }
   str = buf;
   return true;
}

//----------------------------

static bool IsMyPC(){
   
   C_str str;
   WinGetComputerName(str);
   return !stricmp(str, "mike");
}

//----------------------------

class C_edit_Access: public C_editor_item{
   virtual const char *GetName() const{ return "Access"; }
   C_smart_ptr<C_table> tab_supervisor;
   C_smart_ptr<C_table> tab_users;
   HWND hwnd_edit;

   static void TABAPI cbTabCfg1(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){
      switch(msg){
      case TCM_CLOSE:
         {
            C_edit_Access *ei = (C_edit_Access*)cb_user;
            tab->Save("tables\\i3d_supr.tab", TABOPEN_FILENAME);
            ei->ed->GetIGraph()->RemoveDlgHWND(ei->hwnd_edit);
            ei->hwnd_edit = NULL;
         }
         break;
      }
   }

   static void TABAPI cbTabCfg2(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){
      switch(msg){
      case TCM_CLOSE:
         {
            C_edit_Access *ei = (C_edit_Access*)cb_user;
            tab->Save("tables\\i3d_user.tab", TABOPEN_FILENAME);
            ei->ed->GetIGraph()->RemoveDlgHWND(ei->hwnd_edit);
            ei->hwnd_edit = NULL;
         }
         break;
      }
   }

   static CPC_table_template GetTemplate1(){
                           
      static const C_table_element te[] = {
         {TE_ARRAY, 0, "List", 8},
            {TE_STRING, 0, 0, 32},
         {TE_NULL}
      };
      static const C_table_template templ = { "Supervisor access rights", te };
      return &templ;
   }

   static CPC_table_template GetTemplate2(){
                           
      static const C_table_element te[] = {
         {TE_ARRAY, 0, "Standard users", 64},
            {TE_STRING, 0, 0, 32},
         {TE_ARRAY, 1, "Programmers", 64},
            {TE_STRING, 1, 0, 32},
         {TE_NULL}
      };
      static const C_table_template templ = { "User access rights", te};
      return &templ;
   }

   void DestroyEdit(){
      if(hwnd_edit){
         ed->GetIGraph()->RemoveDlgHWND(hwnd_edit);
         DestroyWindow(hwnd_edit);
         hwnd_edit = NULL;
      }
   }
public:
   C_edit_Access(): hwnd_edit(NULL)
      {}
   virtual bool Init(){
                              //create & read tables
      tab_supervisor = CreateTable();
      tab_supervisor->Load(GetTemplate1(), TABOPEN_TEMPLATE);
      tab_supervisor->Load("tables\\i3d_supr.tab", TABOPEN_FILENAME | TABOPEN_UPDATE);
      tab_supervisor->Release();

      tab_users = CreateTable();
      tab_users->Load(GetTemplate2(), TABOPEN_TEMPLATE);
      tab_users->Load("tables\\i3d_user.tab", TABOPEN_FILENAME | TABOPEN_UPDATE);
      tab_users->Release();

      if(IsMyPC()){
         ed->AddShortcut(this, 1000, "%0 &File\\Access\\Supervisor");
      }
                              //for one of supervisors, create advanced menu
      C_str str;
      WinGetComputerName(str);
      for(int i=tab_supervisor->ArrayLen(0); i--; )
         if(str.Matchi(tab_supervisor->ItemS(0, i))) break;
      if(i!=-1)
         ed->AddShortcut(this, 1001, "%0 &File\\Access\\Users");
      return true;
   }
   virtual void Close(){ 
      DestroyEdit();
   }
   //virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}
   virtual dword Action(int id, void *context){

      switch(id){
                              //determine access right
                              // return value:
                              // 0 = no access
                              // 1 = standard user
                              // 2 = advanced user (possibly programmer?)
                              // 3 = supervisor
      case 0:                 
         {
            C_str str;
            WinGetComputerName(str);
            int i;
                              //check supervisors
            for(i=tab_supervisor->ArrayLen(0); i--; )
               if(str.Matchi(tab_supervisor->ItemS(0, i)))
                  return 3;
                              //check advanced users
            for(i=tab_users->ArrayLen(1); i--; )
               if(str.Matchi(tab_users->ItemS(1, i)))
                  return 2;
                              //check standard users
            for(i=tab_users->ArrayLen(0); i--; )
               if(str.Matchi(tab_users->ItemS(0, i)))
                  return 1;
                              //no access
            return 0;
         }
      case 1000:
         {
            if(hwnd_edit) DestroyEdit();
            hwnd_edit = (HWND)tab_supervisor->Edit(GetTemplate1(), ed->GetIGraph()->GetHWND(), 
               cbTabCfg1, (dword)this,
               TABEDIT_MODELESS | TABEDIT_IMPORT | TABEDIT_EXPORT,
               NULL);
            if(hwnd_edit) ed->GetIGraph()->AddDlgHWND(hwnd_edit);
         }
         break;
      case 1001:
         {
            if(hwnd_edit) DestroyEdit();
            hwnd_edit = (HWND)tab_users->Edit(GetTemplate2(), ed->GetIGraph()->GetHWND(),
               cbTabCfg2, (dword)this,
               TABEDIT_MODELESS | TABEDIT_IMPORT | TABEDIT_EXPORT,
               NULL);
            if(hwnd_edit) ed->GetIGraph()->AddDlgHWND(hwnd_edit);
         }
         break;
      }
      return 0;
   }
   //virtual void Render(){}
   virtual bool LoadState(C_chunk &ck){ 
      return false; 
   }
   virtual bool SaveState(C_chunk &ck) const{ 
      return false; 
   }
};

//----------------------------

PC_editor_item CreateAccess(PC_editor ed){
   PC_editor_item ei = new C_edit_Access;
   ed->InstallPlugin(ei);
   ei->Release();
   return ei;
}

//----------------------------
