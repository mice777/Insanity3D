#include "all.h"
#include "main.h"
#include <windows.h>
#include "winstuff.h"
#include "..\..\Source\IEditor\resource.h"
#ifdef NDEBUG
                              //comment-out to disable query of adapter name
//#include <d3d9.h>
#endif

//----------------------------

                              //where crash reports are being sent
                              // (network shared folder)
#define CRASH_REPORT_PATH "\\\\net_machine_name\\!In\\Crashes\\"

//----------------------------

C_log_mt_safe::C_log_mt_safe(){

   critical_section = new CRITICAL_SECTION;
   InitializeCriticalSection((LPCRITICAL_SECTION)critical_section);
}

//----------------------------

C_log_mt_safe::~C_log_mt_safe(){

   DeleteCriticalSection((LPCRITICAL_SECTION)critical_section);
   delete (LPCRITICAL_SECTION)critical_section;
}

//----------------------------

void C_log_mt_safe::WriteString(const char *cp){

   EnterCriticalSection((LPCRITICAL_SECTION)critical_section);
                              //write to log
   fwrite(cp, sizeof(char), strlen(cp), f_log);
                              //flush log
   fclose(f_log);
   f_log = fopen(log_name, "at");

   LeaveCriticalSection((LPCRITICAL_SECTION)critical_section);
}

//----------------------------

int WinSelectItem(const char *title, const char *item_list, int curr_sel){

   struct S_hlp{
      const char *title;
      const char *item_list;
      int curr_sel;
      static BOOL CALLBACK dlgListSelect(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

         switch(uMsg){
         case WM_INITDIALOG:
            {
               S_hlp *hp = (S_hlp*)lParam;
               OsCenterWindow(hwnd, (HWND)igraph->GetHWND());
               SetWindowText(hwnd, hp->title);
               const char *cp = hp->item_list;
               while(*cp){
                  SendDlgItemMessage(hwnd, IDC_ITEM_SELECT, LB_ADDSTRING, 0, (LPARAM)cp);
                  cp += strlen(cp) + 1;
               }
               SendDlgItemMessage(hwnd, IDC_ITEM_SELECT, LB_SETCURSEL, hp->curr_sel, 0);
               ShowWindow(hwnd, SW_SHOW);
            }
            return 1;
         case WM_COMMAND:
            switch(LOWORD(wParam)){
            case IDOK:
               {
                  EndDialog(hwnd, SendDlgItemMessage(hwnd, IDC_ITEM_SELECT, LB_GETCURSEL, 0, 0));
               }
               break;
            case IDCANCEL: EndDialog(hwnd, -1); break;
            }
            break;
         }
         return 0;
      }
   } hlp;
   hlp.title = title;
   hlp.item_list = item_list;
   hlp.curr_sel = curr_sel;

   return DialogBoxParam(GetModuleHandle(NULL), "IDD_LISTSELECT", (HWND)igraph->GetHWND(),
      (DLGPROC)S_hlp::dlgListSelect, (LPARAM)&hlp);
}

//----------------------------

bool WinGetName(const char *title, C_str &str, void *hwnd){

   struct S_hlp{
      const char *title;
      C_str *str;
      static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

         switch(uMsg){
         case WM_INITDIALOG:
            {
               S_hlp *hp = (S_hlp*)lParam;
               SetWindowLong(hwnd, GWL_USERDATA, lParam);
               OsCenterWindow(hwnd, GetParent(hwnd));
               SetWindowText(hwnd, hp->title);
               SetDlgItemText(hwnd, IDC_EDIT, *hp->str);
               ShowWindow(hwnd, SW_SHOW);
            }
            return 1;
         case WM_COMMAND:
            switch(LOWORD(wParam)){
            case IDOK:
               {
                  S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                  char buf[512];
                  GetDlgItemText(hwnd, IDC_EDIT, buf, sizeof(buf));
                  *hp->str = buf;
                  EndDialog(hwnd, 1);
               }
               break;
            case IDCANCEL: EndDialog(hwnd, 0); break;
            }
            break;
         }
         return 0;
      }
   } hlp;
   hlp.title = title;
   hlp.str = &str;

   if(!hwnd)
      hwnd = igraph->GetHWND();
   return DialogBoxParam(GetModuleHandle(NULL), "IDD_GETNAME", (HWND)hwnd, (DLGPROC)S_hlp::dlgProc, (LPARAM)&hlp);
}

//----------------------------

bool __stdcall WinSendCrashReport(const char *log_dump){

   C_str dx_info;
   C_str game_info;

#ifdef DIRECT3D_VERSION
   if(igraph){
      IDirect3D9 *d3dMain = igraph->GetD3DInterface();
      D3DADAPTER_IDENTIFIER9 ai;
      d3dMain->GetAdapterIdentifier(0, 0, &ai);
      dx_info = C_fstr(
         "Adapter name: %s  Driver: %s   ver: %i.%i.%i.%i"
         ,
         ai.Description,
         ai.Driver, HIWORD(ai.DriverVersion.HighPart), LOWORD(ai.DriverVersion.HighPart),
         HIWORD(ai.DriverVersion.LowPart), LOWORD(ai.DriverVersion.LowPart)
         );
   }
#endif

   if(GetTickClass()){
      dword id = GetTickClass()->GetID();
      char ids[5];
      *(dword*)ids = id;
      reverse(ids, ids+4);
      ids[4] = 0;
      game_info = C_fstr("Tick class: %s", ids);
      switch(id){
      case 'MISS':
      case 'GMIS':
         game_info += C_fstr("  Mission name: '%s'", (const char*)((C_mission*)GetTickClass())->GetName());
         break;
      }
   }

   struct S_hlp{
      static void WriteString(PC_dta_stream h, const char *str, bool eol = true){
         h->Write(str, strlen(str));
         if(eol)
            h->Write("\r\n", 2);
      }
   };
#if defined _DEBUG && 1
                              //get name to be saved
   C_str comp_name;
   OsGetComputerName(comp_name);
   SYSTEMTIME st;
   GetLocalTime(&st);

   C_fstr name_base("%s%i-%.2i-%.2i %s",
      CRASH_REPORT_PATH,
      st.wYear, st.wMonth, st.wDay, (const char*)comp_name);
   C_str name;
   for(int i=0; i<1000; i++){
      name = C_fstr("%s - %.3i.err", (const char*)name_base, i);
      PC_dta_stream h = DtaCreateStream(name);
      if(h){
         h->Release();
         continue;
      }
      h = DtaCreateStream(name, true);
      if(h){
         S_hlp::WriteString(h, log_dump);
         if(dx_info.Size()) S_hlp::WriteString(h, dx_info);
         if(game_info.Size()) S_hlp::WriteString(h, game_info);
         h->Release();
         MessageBox(NULL, C_fstr("Report sent to: \"%s\"", (const char*)name), "Crash report", MB_OK);
         return true;
      }
   }
   return false;
#else
#pragma comment(lib, "shell32")
   {
      char tmp_path[MAX_PATH], filename[MAX_PATH];
      if(GetTempPath(sizeof(tmp_path), tmp_path)){
         if(GetTempFileName(tmp_path, "bug", 0, filename)){
            PC_dta_stream h = DtaCreateStream(filename, true);
            if(h){
               S_hlp::WriteString(h, "To: <info@lonelycatgames.com>");
               S_hlp::WriteString(h, "Subject: Bug Report");
               S_hlp::WriteString(h, "MIME-Version: 1.0");
               S_hlp::WriteString(h, "Content-Type: text/plain;");
               S_hlp::WriteString(h, "Content-Transfer-Encoding: 7bit");
               S_hlp::WriteString(h, "X-Priority: 3");
               S_hlp::WriteString(h, "X-MSMail-Priority: Normal");
               S_hlp::WriteString(h, "X-Unsent: 1");
               S_hlp::WriteString(h, "X-MimeOLE: Produced By Microsoft MimeOLE V6.00.2600.0000");
               S_hlp::WriteString(h, "");
               S_hlp::WriteString(h, log_dump);
               if(dx_info.Size()) S_hlp::WriteString(h, dx_info);
               if(game_info.Size()) S_hlp::WriteString(h, game_info);
               h->Release();
               C_str new_name = filename;
               for(dword i=new_name.Size(); i--; ){
                  if(new_name[i]=='.'){
                     ++i;
                     new_name[i] = 0;
                     new_name = (const char*)new_name;
                     new_name += "eml";
                     break;
                  }
               }
               if(i==-1)
                  new_name += ".eml";
               MoveFile(filename, new_name);

               HINSTANCE hi = ShellExecute(NULL, NULL, new_name, NULL, NULL, SW_SHOWDEFAULT);
               if(((dword)hi) >= 32)
                  return true;
            }
         }
      }
                              //some error occured
      MessageBox(NULL,
         "Cannot execute mail client.\nPlease save report to file and send manually.\nThanks for making this product better.",
         "Sending e-mail", MB_OK);
   }
   return false;
#endif
}

//----------------------------

void DisplayControlHelp(void *hwnd, word ctrl_id, const S_ctrl_help_info *hi){
   RECT rc;
   GetWindowRect(GetDlgItem((HWND)hwnd, ctrl_id), &rc);
   POINT pt;
   pt.x = rc.left;
   pt.y = rc.bottom;

   ScreenToClient((HWND)hwnd, &pt);
   pt.x -= 40;
   pt.y += 2;
   const char *txt = "<no help>";
   for(dword i=0; hi[i].ctrl_id; i++){
      if(hi[i].ctrl_id==ctrl_id){
         txt = hi[i].text;
         break;
      }
   }
   OsDisplayHelpWindow(txt, (HWND)hwnd, pt.x, pt.y, 150);
}

//----------------------------
//----------------------------
