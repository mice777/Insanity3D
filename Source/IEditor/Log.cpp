#include "all.h"
#include "common.h"

//----------------------------

/*
class C_mem_allocator: public allocator<void*>{
public:
   virtual pointer allocate(size_type n, const void *hint){
      return (pointer)malloc(n*sizeof(value_type));
   }
   virtual void deallocate(pointer p, size_type n){
      free(p);
   }
};

typedef C_vector<void*, C_mem_allocator> t_call_stack;
*/

//----------------------------

class C_editor_item_Log_imp: public C_editor_item_Log{
   virtual const char *GetName() const{ return "Log"; }

//----------------------------
     /*
   enum E_ACTION_LOG{
      //E_LOG_ADD_TEXT = 11000,    //(const char*) - 
      //E_LOG_ADD_COLOR_TEXT,      //(pair<const char *text, dword color>*) - add colored text line to log window
      //E_LOG_CLOSE,               // - close log window 
      //E_LOG_ADD_TEXT_QUIET,      //(const char*) - add text line to log window, don't activate log
      //E_LOG_SET_DUMP_TEXT,       //(const char*) - set contents of dump window
      //E_LOG_CLOSE_DUMP,          // - close dump window
      //E_LOG_ADD_RICH_TEXT,       //(const char *rich_text) - add text lines (in rich-edit format) to log window
   };
   */

//----------------------------

   //C_smart_ptr<C_editor_item> e_props;
   C_smart_ptr<C_editor_item_Properties> e_props;

   dword last_color;

   /*
   struct S_log_info{
      t_call_stack call_stack;
      dword first_line, num_lines;  //range of lines in the log, to which call stack applies
   };
   */

   struct S_text_queue{
      C_vector<char> text;      //including new-line and null terminator
      //int action_id;
      //bool quiet;
      bool rich_text;
      dword color;
   };
   C_vector<S_text_queue> text_queue;

//----------------------------

   struct S_hlp{
      const char *cp;
      int len;
      FILE *f_out;

      static DWORD CALLBACK StreamIn(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb){

         S_hlp *hp = (S_hlp*)dwCookie;
         int trans = Min((int)cb, hp->len);
         memcpy(pbBuff, hp->cp, trans);
         hp->cp += trans;
         hp->len -= trans;
         *pcb = trans;
         return 0;
      }
      static DWORD CALLBACK StreamOut(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb){

         S_hlp *hp = (S_hlp*)dwCookie;
         assert(hp->f_out);
         fwrite(pbBuff, 1, cb, hp->f_out);
         *pcb = cb;
         return 0;
      }
   };


//----------------------------

   struct S_add_text{
      const char *cp;
      dword color;
      bool rich_text;
   };

//----------------------------

   static BOOL CALLBACK dlgLog(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         return 1;

      case WM_APP + 100:
         {
            const S_add_text *at = (S_add_text*)lParam;
            const char *cp = at->cp;
            HWND hwnd_edit = GetDlgItem(hwnd, IDC_EDIT_LOG1);
            dword len = SendMessage(hwnd_edit, WM_GETTEXTLENGTH, 0, 0);
            CHARRANGE cr = {len, len};
            SendMessage(hwnd_edit, EM_EXSETSEL, 0, (LPARAM)&cr);

            S_hlp hlp = {cp, strlen(hlp.cp) };
            EDITSTREAM es = {(dword)&hlp, 0, S_hlp::StreamIn };

            if(at->rich_text){
               SendMessage(hwnd_edit, EM_STREAMIN, SF_RTF | SFF_SELECTION, (LPARAM)&es);
            }else{
                              //change default color
               union{
                  dword color;
                  byte b[4];
               }u;
               u.color = at->color;
               swap(u.b[0], u.b[2]);
               CHARFORMAT cf;
               cf.cbSize = sizeof(cf);
               SendMessage(hwnd_edit, EM_GETCHARFORMAT, 0, (LPARAM)&cf);
               cf.dwEffects &= ~CFE_AUTOCOLOR;
               cf.crTextColor = u.color;
               SendMessage(hwnd_edit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
                              //stream-in text
               SendMessage(hwnd_edit, EM_STREAMIN, SF_TEXT | SFF_SELECTION, (LPARAM)&es);
            }
            /*
            {                 //line-break
               S_hlp hlp = {"\n", 1 };
               EDITSTREAM es = {(dword)&hlp, 0, S_hlp::StreamIn };
               SendMessage(hwnd_edit, EM_STREAMIN, SF_TEXT | SFF_SELECTION, (LPARAM)&es);
            }
            */
            
            SendMessage(hwnd_edit, EM_SCROLLCARET, 0, 0);
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDC_BUTTON_LOG_CLOSE:
            {
               C_editor_item_Log_imp *el = (C_editor_item_Log_imp*)GetWindowLong(hwnd, GWL_USERDATA);
               el->e_props->RemoveSheet(el->hwnd_log);
               el->log_visible = false;

               el->ClearLogContents();
            }
            break;

         case IDC_BUTTON_DUMP_CLOSE:
            {
               C_editor_item_Log_imp *el = (C_editor_item_Log_imp*)GetWindowLong(hwnd, GWL_USERDATA);
               el->e_props->RemoveSheet(el->hwnd_dump);
               el->dump_visible = false;

               el->ClearDumpContents();
            }
            break;

         case IDC_BUTTON_LOG_SAVE:
            {            
               char buf[MAX_PATH];
               buf[0] = 0;
               OPENFILENAME on;
               memset(&on, 0, sizeof(on));
               on.lStructSize = sizeof(on);
               on.hwndOwner = hwnd;
               on.lpstrFilter = "Text files\0*.txt\0All files\0*.*\0";
               on.lpstrFile = buf;
               on.lpstrFileTitle = NULL;
               on.nMaxFile = MAX_PATH;
               on.nMaxFileTitle = 0;//MAX_PATH;
               on.lpstrTitle = "Enter name";
               on.lpstrDefExt = "txt";
               on.Flags = OFN_EXPLORER | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

               bool b = GetSaveFileName(&on);
               if(b){
                  {
                     FILE *f_out = fopen(buf, "wt");
                     //S_hlp hlp = {NULL, 0, new ofstream(buf, ios::out | ios::trunc) };
                     S_hlp hlp = {NULL, 0, f_out };
                     EDITSTREAM es = {(dword)&hlp, 0, S_hlp::StreamOut };
                     SendDlgItemMessage(hwnd, IDC_EDIT_LOG1, EM_STREAMOUT, SF_TEXT, (LPARAM)&es);
                     //delete hlp.os;
                     fclose(f_out);
                  }
               }
            }
            break;
         case IDCANCEL:
            SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
            break;
         }
         break;
      }
      return 0;
   }

   HWND hwnd_log, hwnd_dump;
   bool log_visible, dump_visible;

   void InitLogWindow(){
      LoadLibrary("RICHED32.DLL");
      hwnd_log = CreateDialogParam(GetHInstance(), "LOG", 
         (HWND)ed->GetIGraph()->GetHWND(),
         dlgLog, (LPARAM)this);
   }

   void CloseLogWindow(){
      if(hwnd_log){
         if(log_visible){
            e_props->RemoveSheet(hwnd_log);
            log_visible = false;
         }
         e_props->RemoveSheet(hwnd_log);
         DestroyWindow(hwnd_log);
         hwnd_log = NULL;
      }
   }

   void ClearLogContents(){
                              //clear contents
      S_hlp hlp = {NULL, 0 };
      EDITSTREAM es = {(dword)&hlp, 0, S_hlp::StreamIn };
      HWND hwnd_edit = GetDlgItem(hwnd_log, IDC_EDIT_LOG1);
      SendMessage(hwnd_edit, EM_SETSEL, 0, -1);
      SendMessage(hwnd_edit, EM_STREAMIN, SF_TEXT | SFF_SELECTION, (LPARAM)&es);
   }

   void InitDumpWindow(){
      hwnd_dump = CreateDialogParam(GetHInstance(), "DUMP",
         (HWND)ed->GetIGraph()->GetHWND(),
         dlgLog, (LPARAM)this);
   }

   void CloseDumpWindow(){
      if(hwnd_dump){
         if(dump_visible){
            e_props->RemoveSheet(hwnd_dump);
            dump_visible = false;
         }
         e_props->RemoveSheet(hwnd_dump);
         DestroyWindow(hwnd_dump);
         hwnd_dump = NULL;
      }
   }

   void ClearDumpContents(){

      SendDlgItemMessage(hwnd_dump, IDC_DUMP, WM_SETTEXT, 0, (LPARAM)"");
   }

   bool new_dump_text;

//----------------------------

   void AddText(const char *txt, dword color, bool rich){
                              //test NULL string
      if(!txt)
         return;
                              //put into queue, process in Tick
      text_queue.push_back(S_text_queue());
      S_text_queue &tq = text_queue.back();
      //tq.quiet = quiet;
      tq.color = color & 0xffffff;
      tq.rich_text = rich;
      int len = strlen(txt);
      tq.text.assign(len+2, 0);
      memcpy(&tq.text.front(), txt, len);
      tq.text[len] = '\n';
      tq.text[len+1] = 0;
   }

//----------------------------

   /*void Show(){

      if(!log_visible){
         log_visible = e_props->Action(E_PROP_ADD_SHEET, hwnd_log);
      }
      e_props->Action(E_PROP_SHOW_SHEET, hwnd_log);
   }*/

//----------------------------

   virtual void AddText(const char *txt, dword color = 0){

      AddText(txt, color, false);
   }

//----------------------------

   virtual void AddRichText(const char *txt){
      AddText(txt, 0, true);
   }

//----------------------------

   virtual void Clear(){

      if(log_visible){
         e_props->RemoveSheet(hwnd_log);
         log_visible = false;
         ClearLogContents();
      }
   }

//----------------------------
public:
   C_editor_item_Log_imp():
      new_dump_text(false),
      hwnd_log(NULL), hwnd_dump(NULL),
      log_visible(false), dump_visible(false),
      last_color(0)
   {}

//----------------------------

   virtual bool Init(){

      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      if(!e_props){
         return false;
      }
      InitLogWindow();
      InitDumpWindow();
      return true;
   }

//----------------------------

   virtual void CloseWindow(){ 

      CloseLogWindow();
      CloseDumpWindow();
      e_props = NULL;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(text_queue.size()){
         if(!log_visible){
            e_props->AddSheet(hwnd_log);
            log_visible = true;
            //e_props->Action(E_PROP_SHOW_SHEET, hwnd_log);
         }
         if(!e_props->IsActive()){
            e_props->ShowSheet(hwnd_log);
         }
         bool new_log_text = false; 

                              //don't make it more than 32K (rich-text control can't handle greater blocks)
         const int buf_size = 32000;
         C_vector<char> tmp_buf(buf_size);

         for(dword i=0; i<text_queue.size(); i++){
            const S_text_queue &tq = text_queue[i];
            S_add_text at;
                              //try to concanetate
            if(i!=text_queue.size()-1 && (tq.color==text_queue[i+1].color) &&
               ((tq.text.size()+text_queue[i+1].text.size()-2) < (buf_size-4)) &&
               //tq.action_id!=E_LOG_ADD_RICH_TEXT)
               !tq.rich_text){

               at.cp = &tmp_buf.front();
               int len = tq.text.size()-1;
               memcpy(&tmp_buf.front(), &tq.text.front(), len);
               do{
                  ++i;
                  const S_text_queue &tq1 = text_queue[i];
                  int len1 = tq1.text.size()-1;
                  memcpy(&tmp_buf.front()+len, &tq1.text.front(), len1);
                  len += len1;

                  //if(tq1.action_id!=E_LOG_ADD_TEXT_QUIET)
                  //if(!tq1.quiet)
                     new_log_text = true;

               }while(i!=text_queue.size()-1 && text_queue[i+1].color==tq.color &&
                  //text_queue[i+1].action_id!=E_LOG_ADD_RICH_TEXT &&
                  !text_queue[i+1].rich_text &&
                  (len+text_queue[i+1].text.size()-1) < (buf_size-4));
               tmp_buf[len] = 0;
            }else{
               at.cp = &tq.text.front();
            }
            at.color = tq.color;
            //at.rich_text = (tq.action_id==E_LOG_ADD_RICH_TEXT);
            at.rich_text = tq.rich_text;
            SendMessage(hwnd_log, WM_APP + 100, 0, (LPARAM)&at);

            //if(tq.action_id!=E_LOG_ADD_TEXT_QUIET)
            //if(!tq.quiet)
               new_log_text = true;
         }
         text_queue.clear();
         //if(new_log_text) e_props->Action(E_PROP_SHOW_SHEET, hwnd_log);
      }

      if(new_dump_text){
         new_dump_text = false;
         e_props->ShowSheet(hwnd_dump);
      }
   }

//----------------------------

         /*
   virtual dword Action(int id, void *context){

      switch(id){
      case E_LOG_SET_DUMP_TEXT:
         {
            if(!context)
               break;
            if(!dump_visible){
               dump_visible = e_props->Action(E_PROP_ADD_SHEET, hwnd_dump);
            }
            SendDlgItemMessage(hwnd_dump, IDC_DUMP, WM_SETREDRAW, false, 0);
            SendDlgItemMessage(hwnd_dump, IDC_DUMP, WM_SETTEXT, 0, (LPARAM)context);
            SendDlgItemMessage(hwnd_dump, IDC_DUMP, WM_SETREDRAW, true, 0);
            UpdateWindow(GetDlgItem(hwnd_dump, IDC_DUMP));
            new_dump_text = true;
         }
         break;

      case E_LOG_CLOSE_DUMP:
         if(dump_visible){
            e_props->RemoveSheet(hwnd_dump);
            dump_visible = false;
            ClearDumpContents();
         }
         break;
      }
      return 0;
   }
         */
   //virtual void Render(){}
};

//----------------------------

void CreateLog(PC_editor ed){
   PC_editor_item ei = new C_editor_item_Log_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
