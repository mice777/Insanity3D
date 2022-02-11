#include "all.h"
#include "ectrl_i.h"
#include "resource.h"


//----------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved){

   switch(fdwReason){
   case DLL_PROCESS_ATTACH:
      {
         WNDCLASS wc;
         memset(&wc, 0, sizeof(wc));

                              //register document window
         wc.style = CS_PARENTDC | CS_DBLCLKS | CS_GLOBALCLASS;
         wc.lpfnWndProc = C_edit_control::dlgECProc;
         wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
         wc.lpszClassName = "EC_WINDOW";

         if(!RegisterClass(&wc)) return ECTRLERR_GENERIC;
      }
      break;
   case DLL_PROCESS_DETACH:
      UnregisterClass("EC_WINDOW", hinstDLL);
      break;
   }
   return true;
}

//----------------------------

bool ConvertMacros(const char *filename, short **cbuf);

//----------------------------

C_key_code code_manager;
C_clipboard clipboard;

//----------------------------

static const struct{
   word cmd;
   short code;
} cmd_2_code[] = {
   //ID_FILE_NEW,      NEWFILE,
   ID_FILE_OPEN,     OPENFILE,
//   ID_FILE_CLOSE,    CLOSEFILE,
   ID_FILE_SAVE,     SAVEFILE,
   ID_FILE_SAVEAS,   SAVEAS,
//   ID_FILE_SAVEALL,  SAVEALL,
   ID_FILE_RELOAD,   RELOADFILE,
   ID_FILE_READBLOCK,READBLOCK,
   ID_FILE_WRITEBLOCK,  WRITEBLOCK,
//   ID_FILE_SAVEPROJECT, SAVEPROJECT,
//   ID_FILE_OPENPROJECT, OPENPROJECT,
//   ID_FILE_CLOSEPROJECT,CLOSEPROJECT,
//   ID_FILE_ADDTOPROJECT,ADDTOPROJECT,
//   ID_FILE_REMOVEFROMPROJECT, REMOVEFROMPROJECT,
//   ID_FILE_OSCOMMAND,OSSHELL,
   ID_FILE_EXIT,     EXIT,
   ID_EDIT_CUT,      CUT,
   ID_EDIT_COPY,     COPY,
   ID_EDIT_PASTE,    PASTE,
   ID_EDIT_DELETELINE,     DELLINE,
   ID_EDIT_DUPLICATELINE,  DUPLINE,
//   ID_EDIT_LOWER,    LOWER,
//   ID_EDIT_UPPER,    UPPER,
//   ID_EDIT_FLIP,     FLIP,
   ID_EDIT_UNDO,     UNDO,
   ID_EDIT_REDO,     REDO,
   ID_SEARCH_FIND,   FIND,
   ID_SEARCH_FINDANDREPLACE,     FIND_REPLACE,
   ID_SEARCH_REPEATFINDREPLACE,  REPEAT,
   ID_SEARCH_GOTOLINE,     GOTOLINE,
   ID_SEARCH_GOTOCOLUMN,   GOTOCOLUMN,
   ID_BLOCK_MARKLINE,      MARKLINE,
   ID_BLOCK_MARKCOLUMN,    MARKCOLUMN,
   ID_BLOCK_UNMARK,        UNMARKBLOCK,
   ID_BLOCK_DELETE,        DELETEBLOCK,

   ID_EDIT_DELTOEOL,       DELTOEOL,

   0
};

//----------------------------

static byte rgb_values_defaults[][3]={
   0,0,0,
   0,0,170,
   0,170,0,
   0,170,170,
   170,0,0,
   170,0,170,
   170,85,0,
   170,170,170,
   85,85,85,
   85,85,255,
   85,255,85,
   85,255,255,
   255,85,85,
   255,85,255,
   255,255,85,
   255,255,255
};

//----------------------------

void SetModify(C_window *wp, int undo_buffer){

   if(wp->doc.SetModify(true)){
      SetWindowText(wp->ec->hwnd, C_fstr("* %s", (const char*)wp->doc.title));
                              //only undo 0 can set it
      if(!undo_buffer)
         wp->undo[0].Add(C_undo_element::FILEMODIFY);
   }
}

//----------------------------

dword C_edit_control::GetRGB(byte c, bool highl){

   byte r=rgb_values[c][0];
   byte g=rgb_values[c][1];
   byte b=rgb_values[c][2];
   int i;
   if(highl && (i=config.curr_line_highlight)!=100){
                                       //adjust brightness
      if(i<100){
         r=r*i/100;
         g=g*i/100;
         b=b*i/100;
      }else{
         i-=100;
         r+=(255-r)*i/100;
         g+=(255-g)*i/100;
         b+=(255-b)*i/100;
      }
   }
   return RGB(r, g, b);
}

//----------------------------

static bool SavePrompt(C_window *wp, HWND hwnd_main){

   if(wp->doc.modified){
                              //prompt for saving changes
      switch(MessageBox(hwnd_main, 
         C_fstr("Save changes to %s?", (const char*)wp->doc.title), "HighWare editor", MB_YESNOCANCEL)){
      case IDYES:
         if(!wp->Save()) return false;
      case IDNO:
         break;
      default:
         return false;
      }
   }
   return true;
}

//----------------------------

static void AdjustBlock(C_window *wp, bool shift_down, bool replay = 0){
   
   if(replay && wp->block.IsCUA())
      return;
   int i = wp->block.Adjust(wp, shift_down, wp->cursor.x, wp->cursor.y,
      wp->cursor.prv_x, wp->cursor.prv_y);
   if(i&2)
      wp->doc.redraw = true;
   if(i&1)
      wp->doc.redraw_line = true;
}

//----------------------------

bool PutCh(C_window*, char c);

//----------------------------

static void MinimizeLine(C_window *wp){

   if(wp->ec->read_only)
      return;
   int line = wp->cursor.y;
   if(line>=wp->doc.linenum) return;
   C_document_line *lp=wp->doc.lines[line];
   lp->RemoveTrailingSpaces();
   wp->doc.ReplaceLine(wp->cursor.y, lp->Minimize());
}

//----------------------------

void C_edit_control::InitScrollBar(){

                              //setup scroll-bar
   int fsx, fsy;
   GetFontSize(&fsx, &fsy);
   RECT rc;
   GetClientRect(win.hwnd, &rc);
   int num_l = (rc.bottom-rc.top) / fsy;

   SCROLLINFO si;
   si.cbSize = sizeof(si);
   si.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
   si.nPage = num_l;
   si.nMin = 0;
   si.nMax = win.doc.linenum;
   si.nPos = win.scroll_y;
   SetScrollInfo(win.hwnd, SB_VERT, &si, true);
}

//----------------------------

bool SetCursorPos1(C_window *wp, int x, int y, int undo_buff){

   x = Max(0, Min(x, MAX_ROWS));
   y = Max(0, Min(y, (int)wp->doc.linenum-1));
   if(wp->cursor.x!=x || wp->cursor.y!=y){
                                       //add position into undo buffer
      wp->undo[undo_buff].Add(C_undo_element::POSITION, wp->cursor.x, wp->cursor.y);
      wp->cursor.x = x;
      if(wp->cursor.y!=y){
         MinimizeLine(wp);
                                       //un-highlight previous line
         SendMessage(wp->hwnd, WM_USER_PAINTLINE, wp->cursor.y, 0);
         wp->cursor.y = y;
                                       //highlight new current line
         SendMessage(wp->hwnd, WM_USER_PAINTLINE, wp->cursor.y, 1);
      }
      wp->cursor.redraw = true;
      return true;
   }
   return false;
}

//----------------------------

void SetCursorVisible(C_window *wp, HWND hwnd){

   RECT rc;
   GetClientRect(hwnd, &rc);
   int fsx, fsy;
   wp->ec->GetFontSize(&fsx, &fsy);
   int sx=rc.right/fsx;
   int sy=rc.bottom/fsy;

   if(wp->cursor.x >= sx+wp->scroll_x){
      wp->SetScrollPos(wp->cursor.x-sx+1, wp->scroll_y);
   }
   if(wp->cursor.x < wp->scroll_x){
      wp->SetScrollPos(wp->cursor.x, wp->scroll_y);
   }
   if(wp->cursor.y >= sy+wp->scroll_y){
      wp->SetScrollPos(wp->scroll_x, wp->cursor.y-sy+1);
   }
   if(wp->cursor.y < wp->scroll_y){
      wp->SetScrollPos(wp->scroll_x, wp->cursor.y);
   }
}

//----------------------------

void C_edit_control::SetState(E_state s, const char *msg){

   state = s;
   state_message = msg;
}

//----------------------------

E_state C_edit_control::CheckState(){
   
   C_str str;
   switch(state){
   case STATE_MESSAGE:
      str = state_message;
      break;
   case STATE_WARNING:
      str = C_fstr("Warning: %s", (const char*)state_message);
      break;
   case STATE_ERROR:
      str = C_fstr("Error: %s", (const char*)state_message);
      break;
   default:
      SendMessage(hwnd_sb, SB_SETTEXT, 1, (LPARAM)NULL);
      return STATE_OK;
   }
   SendMessage(hwnd_sb, SB_SETTEXT, 1, (LPARAM)(const char*)str);

   E_state r = state;
   state = STATE_OK;
   return r;
}

//----------------------------

void C_edit_control::DrawLineColumn(){

   SendMessage(hwnd_sb, SB_SETTEXT, 0,
      (LPARAM)(const char*)C_fstr("L: %i  C: %i", 
      win.cursor.y + 1, win.cursor.x + 1));
}

//----------------------------
                              
static const char CODE_CONVERT[]={
   VK_PRIOR, VK_NEXT, VK_END, VK_HOME,
   VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
   VK_SNAPSHOT, VK_INSERT, VK_DELETE,
   /*VK_MULTIPLY, VK_ADD, VK_SEPARATOR, VK_SUBTRACT,
   VK_DIVIDE, */VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6,
   VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
   (byte)VK_NUMLOCK, (byte)VK_SCROLL,

   (byte)219, (byte)221, (byte)186, (byte)222, 
   (byte)189, (byte)187, (byte)220, (byte)188,
   (byte)190,
   (byte)191, 111, 106, 109, 107, (byte)192,

   0
};

static const byte CODE_CONVERT1[]={
   K_PAGEUP, K_PAGEDOWN, K_END, K_HOME,
   K_CURSORLEFT, K_CURSORUP, K_CURSORRIGHT, K_CURSORDOWN,
   K_PRTSCR, K_INS, K_DEL,
   /*K_GREYMULT, K_GREYPLUS, K_CENTER, K_GREYMINUS,
   K_GREYSLASH, */K_F1, K_F2, K_F3, K_F4, K_F5, K_F6,
   K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
   K_NUMLOCK, K_SCROLLLOCK,

   '[', ']', ';', '\'', '-', '=', '\\', ',',
   '.',
   '/', 111, 106, 109, 107, '`',
};

//----------------------------

static LOGFONT font_log_defaults = {
   12, 8, 0, 0, 0, 0, 0, 0, OEM_CHARSET, OUT_DEFAULT_PRECIS,
   CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, "Terminal"
};

//----------------------------
                              //default config
static S_config config_defaults = {
   3,                         //tab
   false,                     //overwrite mode
   {6, 2, 0, 7},              //cursor shape
   "{}[]().=+-*/:;<>|&,~!^?#", 24,  //symbols
   "/*", 2,                   //comment beg
   "*/", 2,                   //comment end
   "//", 2,                   //comment eol
   "\"'", 2,                  //string
   '\\',                               //literal
   125,                                //curr line highlight [%]
   false,                     //underline cursor

   0,                         //reserved
   {                          //colors
      (BLUE<<4) | LIGHT_CYAN,       //CLR_DEFAULT
      (BLUE<<4) | YELLOW,                 //CLR_NUMBER
      (BLUE<<4) | LIGHT_RED,              //CLR_STRING
      (BLUE<<4) | GREY,                   //CLR_SYMBOLS
      (BLUE<<4) | GREEN,                  //CLR_COMMENT
      (BLUE<<4) | LIGHT_PURPUR,           //CLR_RESERVED1
      (BLUE<<4) | LIGHT_GREEN,            //CLR_RESERVED2
      (BLUE<<4) | LIGHT_BLUE,             //CLR_RESERVED3
      (BLACK<<4) | YELLOW,                 //CLR_BLOCK
      (BLACK<<4) | WHITE,                  //CLR_FIND
   },
};

//----------------------------

void C_edit_control::MakeCaret(){

   int fsx, fsy;
   GetFontSize(&fsx, &fsy);
   int x = (win.cursor.x-win.scroll_x)*fsx;
   int y = (win.cursor.y-win.scroll_y)*fsy;

                              //offset by frame size
   int fs = GetSystemMetrics(SM_CXEDGE);
   x += fs;
   y += fs;

   if(HideCaret(hwnd)) 
      DestroyCaret();
   int sx=fsx, sy=fsy;
   if(!win.overwrite)
   if(config.underline_cursor){
      sy = Max(2, GetSystemMetrics(SM_CXBORDER));
      y += fsy-sy;
   }else
      sx = Max(2, GetSystemMetrics(SM_CXBORDER));
   CreateCaret(hwnd, NULL, sx, sy);
   SetCaretPos(x, y);
   ShowCaret(hwnd);
}

//----------------------------

void C_edit_control::SizeWindow(){

   RECT rc;
   GetClientRect(hwnd, &rc);
                              //subtract status-bar
   if(hwnd_sb){
      RECT rc1;
      GetClientRect(hwnd_sb, &rc1);
      rc.bottom -= (rc1.bottom-rc1.top);
   }

   MoveWindow(win.hwnd, 0, 0, (rc.right-rc.left), (rc.bottom-rc.top), true);

   InitScrollBar();
}

//----------------------------

static unsigned int CALLBACK FontChooseHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         CHOOSEFONT *cf = (CHOOSEFONT*)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, cf->lCustData);
      }
      break;
   case WM_COMMAND:

      switch(LOWORD(wParam)){
      case 0x402:
         {
            C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
            ec->font_sy = -1;
            if(ec->fnt){
               DeleteObject(ec->fnt);
               ec->fnt = NULL;
            }
            SendMessage(hwnd, WM_CHOOSEFONT_GETLOGFONT, 0, (LPARAM)&ec->curr_font_log);
            PostMessage(ec->win.hwnd, WM_USER_PAINTWHOLE, 0, 0);
            ec->InitScrollBar();
         }
         break;
      }
      break;
   }

   return 0;
}

//----------------------------

static BOOL CALLBACK DlgColors(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   static char COLOR_TEXT[] = {
      "Default\0"
      "Number\0"
      "String\0"
      "Symbol\0"
      "Comment\0"
      "Keyword 1\0"
      "Keyword 2\0"
      "Keyword 3\0"
      "Keyword 4\0"
      "Block\0"
      "Find\0"
   };

   int i;
   static byte (*s_rgb_values)[3];
   static byte *s_colors;

   switch(uMsg){
   case WM_INITDIALOG:
      {
         C_edit_control *ec = (C_edit_control*)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)ec);
                                       //center
         RECT rc, rc1;
         GetWindowRect(ec->hwnd, &rc1);
         GetWindowRect(hwnd, &rc);
         SetWindowPos(hwnd, 0, ((rc1.right-rc1.left)-(rc.right-rc.left))/2+rc1.left,
            ((rc1.bottom-rc1.top)-(rc.bottom-rc.top))/2+rc1.top, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
         s_rgb_values = new byte[16][3];
         memcpy(s_rgb_values, ec->rgb_values, sizeof(ec->rgb_values));
         s_colors = new byte[CLR_LAST];
         memcpy(s_colors, ec->config.colors, CLR_LAST);

         char *cp = COLOR_TEXT;
         for(i=0; i<CLR_LAST; i++){
            SendDlgItemMessage(hwnd, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)cp);
            cp += strlen(cp) + 1;
         }
         for(i=0; i<16; i++){
            SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_FORE, CB_ADDSTRING, 0, (LPARAM)"");
            SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_BACK, CB_ADDSTRING, 0, (LPARAM)"");
         }
         SendDlgItemMessage(hwnd, IDC_LIST1, LB_SETCURSEL, 0, 0);
         SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_FORE, CB_SETCURSEL,
            s_colors[0]&15, 0);
         SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_BACK, CB_SETCURSEL,
            s_colors[0]>>4, 0);
      }
      return 1;
   case WM_DRAWITEM:
      DRAWITEMSTRUCT *dis;
      dis=(DRAWITEMSTRUCT*)lParam;
      if(dis->CtlType==ODT_COMBOBOX){
         HDC hdc=dis->hDC;
         i=dis->itemID;
         COLORREF cr=RGB(s_rgb_values[i][0], s_rgb_values[i][1], s_rgb_values[i][2]);
         HBRUSH hb, hb1;
         hb1 = (HBRUSH)SelectObject(hdc, hb = CreateSolidBrush(cr));
         HPEN hp, hp1;
         if(dis->itemState&ODS_SELECTED) hp1 = (HPEN)SelectObject(hdc, hp=CreatePen(PS_DOT, 1, cr^0xffffff));
         else hp1 = (HPEN)SelectObject(hdc, hp=CreatePen(PS_SOLID, 1, cr));
         Rectangle(hdc, dis->rcItem.left, dis->rcItem.top,
            dis->rcItem.right, dis->rcItem.bottom);
         SelectObject(hdc, hb1);
         SelectObject(hdc, hp1);
         DeleteObject(hb);
         DeleteObject(hp);
      }
      break;
   case WM_CTLCOLORSTATIC:
      if(((HWND)lParam)==GetDlgItem(hwnd, IDC_STATIC_SAMPLE)){
         HDC hdc=(HDC)wParam;
         i=SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_FORE, CB_GETCURSEL, 0, 0);
         SetTextColor(hdc, RGB(s_rgb_values[i][0], s_rgb_values[i][1], s_rgb_values[i][2]));
         i=SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_BACK, CB_GETCURSEL, 0, 0);
         SetBkColor(hdc, RGB(s_rgb_values[i][0], s_rgb_values[i][1],
            s_rgb_values[i][2]));
         return (int)CreateSolidBrush(RGB(s_rgb_values[i][0], s_rgb_values[i][1],
            s_rgb_values[i][2]));
      }
      break;
   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDC_LIST1:
         switch(HIWORD(wParam)){
         case LBN_SELCHANGE:
            i = SendDlgItemMessage(hwnd, IDC_LIST1, LB_GETCURSEL, 0, 0);
            SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_FORE, CB_SETCURSEL,
               s_colors[i]&15, 0);
            SendDlgItemMessage(hwnd, IDC_COMBO_COLOR_BACK, CB_SETCURSEL,
               s_colors[i]>>4, 0);

            char *cp=new char[64];
            SendDlgItemMessage(hwnd, IDC_STATIC_SAMPLE, WM_GETTEXT, 64, (LPARAM)cp);
            SendDlgItemMessage(hwnd, IDC_STATIC_SAMPLE, WM_SETTEXT, 0, (LPARAM)cp);
            delete[] cp;
            break;
         }
         break;
      case IDC_COMBO_COLOR_FORE:
      case IDC_COMBO_COLOR_BACK:
         switch(HIWORD(wParam)){
         case CBN_SELENDOK:
                                       //redraw sample
            char *cp=new char[64];
            SendDlgItemMessage(hwnd, IDC_STATIC_SAMPLE, WM_GETTEXT, 64, (LPARAM)cp);
            SendDlgItemMessage(hwnd, IDC_STATIC_SAMPLE, WM_SETTEXT, 0, (LPARAM)cp);
            delete[] cp;
                                       //get current color item
            i=SendDlgItemMessage(hwnd, IDC_LIST1, LB_GETCURSEL, 0, 0);
            int clr;                   //get selected color
            clr=SendDlgItemMessage(hwnd, LOWORD(wParam), CB_GETCURSEL, 0, 0);
                                       //set fore or back
            if(LOWORD(wParam)==IDC_COMBO_COLOR_FORE)
               s_colors[i]=(s_colors[i]&0xf0)|clr;
            else
               s_colors[i]=(s_colors[i]&0xf)|(clr<<4);
            break;
         }
         break;
      case ID_COLOR_EDIT_FORE:
      case ID_COLOR_EDIT_BACK:
         {
            C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
            bool fore;
            fore = LOWORD(wParam)==ID_COLOR_EDIT_FORE;
            i = SendDlgItemMessage(hwnd, fore ? IDC_COMBO_COLOR_FORE : IDC_COMBO_COLOR_BACK,
               CB_GETCURSEL, 0, 0);
            CHOOSECOLOR cc;
            memset(&cc, 0, sizeof(cc));
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.hInstance = (HWND)ec->hi;
            cc.rgbResult = s_rgb_values[i][0] | (s_rgb_values[i][1]<<8) | (s_rgb_values[i][2]<<16);
            cc.lpCustColors = ec->custom_colors;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if(ChooseColor(&cc)){
               s_rgb_values[i][0] = (cc.rgbResult    )&255;
               s_rgb_values[i][1] = (cc.rgbResult>>8 )&255;
               s_rgb_values[i][2] = (cc.rgbResult>>16)&255;
               SetFocus(GetDlgItem(hwnd, fore ? IDC_COMBO_COLOR_BACK : IDC_COMBO_COLOR_FORE));
               SetFocus(GetDlgItem(hwnd, fore ? IDC_COMBO_COLOR_FORE : IDC_COMBO_COLOR_BACK));
               char *cp = new char[64];
               SendDlgItemMessage(hwnd, IDC_STATIC_SAMPLE, WM_GETTEXT, 64, (LPARAM)cp);
               SendDlgItemMessage(hwnd, IDC_STATIC_SAMPLE, WM_SETTEXT, 0, (LPARAM)cp);
               delete[] cp;
               ec->config_changed = true;
            }
         }
         break;
      case IDAPPLY:
         {
            C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
            memcpy(ec->rgb_values, s_rgb_values, sizeof(ec->rgb_values));
            memcpy(ec->config.colors, s_colors, CLR_LAST);
            PostMessage(ec->win.hwnd, WM_USER_PAINTWHOLE, 0, 0);
         }
         break;
      case IDOK:
         {
            C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
            memcpy(ec->rgb_values, s_rgb_values, sizeof(ec->rgb_values));
            memcpy(ec->config.colors, s_colors, CLR_LAST);
            ec->config_changed = true;
            delete[] s_rgb_values;
            delete[] s_colors;
            EndDialog(hwnd, 1);
         }
         break;
      case IDCANCEL:
         delete[] s_rgb_values;
         delete[] s_colors;
         EndDialog(hwnd, 0);
         break;
      }
      break;
   }
   return 0;
}

//----------------------------

static BOOL CALLBACK DlgConfig(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   char line[128];
   int i, j;
   static const int IDC_KEYWORD[] = {
      IDC_LIST_KEYWORDS1, IDC_LIST_KEYWORDS2, IDC_LIST_KEYWORDS3, IDC_LIST_KEYWORDS4
   };
   static int curr_lb;

   switch(uMsg){
   case WM_INITDIALOG:
      {
         C_edit_control *ec = (C_edit_control*)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)ec);
                              //center
         RECT rc, rc1;
         GetWindowRect(ec->hwnd, &rc1);
         GetWindowRect(hwnd, &rc);
         SetWindowPos(hwnd, 0, ((rc1.right-rc1.left)-(rc.right-rc.left))/2+rc1.left,
            ((rc1.bottom-rc1.top)-(rc.bottom-rc.top))/2+rc1.top, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

         SetDlgItemInt(hwnd, IDC_EDIT_TABWIDTH, ec->config.tab_width, 0);
         SetDlgItemText(hwnd, IDC_EDIT_STRING, ec->config.string);
         SetDlgItemText(hwnd, IDC_EDIT_EOLCOMMENT, ec->config.eol_comm);
         SetDlgItemText(hwnd, IDC_EDIT_COMMENTSTART, ec->config.open_comm);
         SetDlgItemText(hwnd, IDC_EDIT_COMMENTEND, ec->config.close_comm);
         SetDlgItemText(hwnd, IDC_EDIT_SYMBOLS, ec->config.symbols);
         line[0] = ec->config.literal;
         line[1] = 0;
         SetDlgItemText(hwnd, IDC_EDIT_LITERAL, line);
         SetDlgItemInt(hwnd, IDC_EDIT_LINEHIGHL, ec->config.curr_line_highlight, 0);
         int i;
         for(i=0; i<4; i++){
            SendDlgItemMessage(hwnd, IDC_RESERVED_TYPES, CB_ADDSTRING, 0, (LPARAM)(const char*)C_fstr("Reserved %i", i+1));
         }
         SendDlgItemMessage(hwnd, IDC_RESERVED_TYPES, CB_SETCURSEL, 0, 0);
         ShowWindow(GetDlgItem(hwnd, IDC_LIST_KEYWORDS1), SW_SHOW);
         SendDlgItemMessage(hwnd, IDC_LIST_KEYWORDS1, LB_SETCURSEL, 0, 0);
         CheckDlgButton(hwnd, IDC_CHECK_CURSORUNDER, ec->config.underline_cursor);

         for(j=0; j<4; j++)
         for(i=0; i<(int)ec->reserved_word[j].size(); i++)
            SendDlgItemMessage(hwnd, IDC_KEYWORD[j], LB_ADDSTRING, 0, (LPARAM)(const char*)ec->reserved_word[j][i]);
         curr_lb = 0;
      }
      return 1;

   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDC_EDIT_KEYWORD:
         switch(HIWORD(wParam)){
         case EN_CHANGE:
            i=SendDlgItemMessage(hwnd, IDC_EDIT_KEYWORD, EM_LINELENGTH, 0, 0);
            EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_ADD), !!i);
            break;
         }
         break;
      case IDC_BUTTON_ADD:
         {
            GetDlgItemText(hwnd, IDC_EDIT_KEYWORD, line, 127);
            SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_ADDSTRING, 0, (LPARAM)line);
         }
         break;
      case IDC_RESERVED_TYPES:
         switch(HIWORD(wParam)){
         case CBN_SELENDOK:
            i = curr_lb;
            curr_lb=SendDlgItemMessage(hwnd, IDC_RESERVED_TYPES, CB_GETCURSEL, 0, 0);
            ShowWindow(GetDlgItem(hwnd, IDC_KEYWORD[i]), SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_KEYWORD[curr_lb]), SW_SHOW);
            break;
         }
         break;
      case IDC_LIST_KEYWORDS1: case IDC_LIST_KEYWORDS2: case IDC_LIST_KEYWORDS3:
         switch(HIWORD(wParam)){
         case LBN_SELCHANGE:
            {
               i = SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_GETCURSEL, 0, 0);
               SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_GETTEXT, i, (LPARAM)line);
               SendDlgItemMessage(hwnd, IDC_EDIT_KEYWORD, WM_SETTEXT, 0, (LPARAM)line);
            }
            break;
         case LBN_SETFOCUS:
            EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_DELETE), 1);
            break;
         }
         break;
      case IDC_BUTTON_DELETE:
         i=SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_GETCURSEL, 0, 0);
         SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_DELETESTRING, i, 0);
         j=SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_GETCOUNT, 0, 0);
         if(!j) EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_DELETE), 0);
         else{
            i=min(i, j-1);
            SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_SETCURSEL, i, 0);
         }
         break;

      case IDCANCEL:
         EndDialog(hwnd, -1);
         break;

      case IDOK:
         {
            C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);

            ec->config.tab_width= GetDlgItemInt (hwnd, IDC_EDIT_TABWIDTH, NULL, 0);
            ec->config.string_len=    GetDlgItemText(hwnd, IDC_EDIT_STRING, ec->config.string, 7);
            ec->config.eol_comm_len=  GetDlgItemText(hwnd, IDC_EDIT_EOLCOMMENT, ec->config.eol_comm, 7);
            ec->config.open_comm_len=   GetDlgItemText(hwnd, IDC_EDIT_COMMENTSTART, ec->config.open_comm, 7);
            ec->config.close_comm_len=  GetDlgItemText(hwnd, IDC_EDIT_COMMENTEND, ec->config.close_comm, 7);
            ec->config.symbols_len=     GetDlgItemText(hwnd, IDC_EDIT_SYMBOLS, ec->config.symbols, 127);
            i=GetDlgItemInt(hwnd, IDC_EDIT_LINEHIGHL, NULL, 0);
            ec->config.curr_line_highlight=max(1, min(200, i));
            i=GetDlgItemText(hwnd, IDC_EDIT_LITERAL, line, 2);
            ec->config.literal= i ? line[0] : 0;
            ec->config.underline_cursor=IsDlgButtonChecked(hwnd, IDC_CHECK_CURSORUNDER);
                              //collect reserved words
            for(curr_lb = 0; curr_lb<4; curr_lb++){
               int count = SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_GETCOUNT, 0, 0);
               char buf[256];
               ec->reserved_word[curr_lb].clear();
               for(j=0; j<count; j++){
                  SendDlgItemMessage(hwnd, IDC_KEYWORD[curr_lb], LB_GETTEXT, j, (LPARAM)buf);
                  ec->reserved_word[curr_lb].push_back(buf);
               }
            }
            EndDialog(hwnd, 1);
         }
         break;
      }
      break;
   }
   return 0;
}

//----------------------------

static BOOL CALLBACK DlgAbout(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         RECT rc, rc1;
         C_edit_control *ec = (C_edit_control*)lParam;
                              //center
         GetWindowRect(ec->hwnd, &rc1);
         GetWindowRect(hwnd, &rc);
         SetWindowPos(hwnd, 0, ((rc1.right-rc1.left)-(rc.right-rc.left))/2+rc1.left,
            ((rc1.bottom-rc1.top)-(rc.bottom-rc.top))/2+rc1.top, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
      }
      return 1;
   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDOK:
         EndDialog(hwnd, 1);
         break;
      }
      break;
   }
   return 0;
}

//----------------------------

BOOL CALLBACK C_edit_control::dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){

   case WM_INITDIALOG:
      {
         C_edit_control *ec = (C_edit_control*)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
                           //init status bar
         ec->hwnd_sb = CreateStatusWindow(
            SBARS_SIZEGRIP | WS_VISIBLE | WS_CHILD,
            NULL,
            hwnd,
            0
            );
         static int pw[2] = {70, -1};
         SendMessage(ec->hwnd_sb, SB_SETPARTS, 2, (LPARAM)pw);
         {
            HICON hc = LoadIcon(GetModuleHandle(DLL_NAME), MAKEINTRESOURCE(IDI_ICON3));
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hc);
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hc);
         }
      }
      return 1;

   case WM_ACTIVATE:
      {
         C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
         C_window *wp = &ec->win;
         switch(LOWORD(wParam)){
         case WA_INACTIVE:
            HideCaret(hwnd);
            break;
         default: ec->MakeCaret();
         }
         if(wp){
            ec->DrawLineColumn();
            wp->cursor.redraw = true;
         }
      }
      break;

   case WM_SIZE:
      {
         C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
         //C_window *wp = &ec->win;
         ec->SizeWindow();
         if(ec->hwnd_sb) SendMessage(ec->hwnd_sb, uMsg, wParam, lParam);
      }
      break;

   case WM_COMMAND:
      {
         C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
         C_window *wp = &ec->win;
         short wID = LOWORD(wParam);
         switch(wID){

         default:
         case ID_FILE_OPEN:
         case ID_FILE_EXIT:
            {
               ec->AddRef();
               int i = 0;
               do{
                  if(cmd_2_code[i].cmd==wID){
                     code_manager.edit_functions[cmd_2_code[i].code-FIRST_COMMAND](wp);
                     if(wp) wp->undo[0].Add(C_undo_element::MARK);
                     break;
                  }
               }while(cmd_2_code[++i].cmd);
               ec->Redraw();
               ec->Release();
            }
            break;
                              //OPTIONS
         case ID_OPTIONS_FONT:
            {
               LOGFONT lf;
               memcpy(&lf, &ec->curr_font_log, sizeof(lf));
               CHOOSEFONT cf;
               memset(&cf, 0, sizeof(cf));
               cf.lStructSize = sizeof(cf);
               cf.hwndOwner = hwnd;
               cf.lpLogFont = &lf;
               cf.Flags = CF_FIXEDPITCHONLY | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT |
                  CF_APPLY | CF_SCREENFONTS | CF_ENABLEHOOK;
               cf.nFontType = SCREEN_FONTTYPE;
               cf.lpfnHook = FontChooseHook;
               cf.lCustData = (LPARAM)ec;

               if(ChooseFont(&cf)){
                  DestroyCaret();
                  memcpy(&ec->curr_font_log, &lf, sizeof(LOGFONT));
                  ec->font_sy = -1;
                  if(ec->fnt){
                     DeleteObject(ec->fnt);
                     ec->fnt = NULL;
                  }
                  PostMessage(ec->win.hwnd, WM_USER_PAINTWHOLE, 0, 0);
                  ec->MakeCaret();
                  ec->InitScrollBar();

                  ec->config_changed = true;
               }
            }
            break;

            case ID_OPTIONS_COLORS:
               if(DialogBoxParam(ec->hi, "IDD_DIALOG_COLORS", hwnd, (DLGPROC)DlgColors, (LPARAM)ec)){
                  DestroyCaret();
                  PostMessage(ec->win.hwnd, WM_USER_PAINTWHOLE, 0, 0);
                  ec->MakeCaret();
               }
               break;

         case ID_OPTIONS_CONFIG:
            {
               int i = DialogBoxParam(ec->hi, "IDD_DIALOG_CONFIG", hwnd,
                  (DLGPROC)DlgConfig, (LPARAM)ec);

               if(i!=-1){
                  ec->win.doc.CheckComments(&ec->config, 0, 0, 1, 1);
                  PostMessage(ec->win.hwnd, WM_USER_PAINTWHOLE, 0, 0);
               }
               ec->config_changed = true;
            }
            break;
         case ID_HELP_ABOUT:
            DialogBoxParam(ec->hi, "IDD_DIALOG_ABOUT",
               ec->hwnd, (DLGPROC)DlgAbout, (LPARAM)ec);
            break;
         }
      }
      break;
   case WM_CLOSE:
      {
         C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
         if(ec){
            C_window *wp = &ec->win;
            if(!SavePrompt(wp, hwnd)) return 0;

            DestroyWindow(wp->hwnd);
            wp->hwnd = NULL;
            DestroyWindow(hwnd);
            EndDialog(hwnd, 0);
            return 0;
         }
      }
      break;
   case WM_DESTROY:
      {
         C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
         if(ec){
            ec->AddRef();
            if(ec->cb_proc) (*ec->cb_proc)("close", ec->cb_context);
            ec->hwnd = NULL;
            SetWindowLong(hwnd, GWL_USERDATA, 0);
            ec->Release();
         }
      }
      break;
   }
   return 0;
}

//----------------------------

long CALLBACK C_edit_control::dlgECProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   C_edit_control *ec = (C_edit_control*)GetWindowLong(hwnd, GWL_USERDATA);
   static bool hwnd_lbut_down;

   switch(uMsg){

   case WM_CREATE:
      {
         CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
         SetWindowLong(hwnd, GWL_ID, 1);
         SetWindowLong(hwnd, GWL_USERDATA, (long)cs->lpCreateParams);
      }
      break;

   case WM_DESTROYCLIPBOARD:
      return !clipboard.Destroy();

   case WM_ERASEBKGND:
      return 1;

   case WM_PAINT:
      HideCaret(ec->hwnd);
      if(!ec) return 0;
      ec->win.doc.redraw = false;
      ec->win.doc.Paint(ec, ec->win.scroll_x, ec->win.scroll_y, &ec->win.block,
         ec->win.cursor.y, hwnd, ec->win.cursor.y);
      if(ec->win.hwnd==GetFocus())
         ec->MakeCaret();
      break;

   case WM_USER_PAINTLINE:
      HideCaret(ec->hwnd);
      ec->win.doc.redraw_line=0;
      if(wParam < (dword)ec->win.doc.linenum){
         ec->win.doc.PaintLine(ec, ec->win.scroll_x, ec->win.scroll_y, &ec->win.block,
            wParam, hwnd, lParam);
      }
      if(ec->win.hwnd==GetFocus())
         ec->MakeCaret();
      break;

   case WM_USER_MOVECURSOR:
      if(ec->win.hwnd==GetFocus())
         ec->MakeCaret();
      ec->win.cursor.redraw = false;
      break;


   case WM_LBUTTONDOWN:
      {
         int fsx, fsy;
         ec->GetFontSize(&fsx, &fsy);
         SetCursorPos1(&ec->win, ec->win.scroll_x+LOWORD(lParam)/fsx, ec->win.scroll_y+HIWORD(lParam)/fsy);
         ec->win.cursor.redraw = true;
         if(ec->win.block.IsCUA())
            code_manager.edit_functions[UNMARKBLOCK-FIRST_COMMAND](&ec->win);
         SetCapture(hwnd);
         ec->win.cursor.SavePos();

         hwnd_lbut_down = true;
         ec->Redraw();
      }
      break;

   case WM_VSCROLL:
      {
         static bool inside;
         if(!inside){
            inside = true;
            C_window *wp = &ec->win;
            int scy = wp->scroll_y;
            int cy = wp->cursor.y;
            int delta_y = cy-scy;
            switch(LOWORD(wParam)){
            case SB_LINEDOWN:
               ++scy;
               break;
            case SB_LINEUP:
               --scy;
               break;
            case SB_THUMBTRACK:
               SCROLLINFO si;
               si.cbSize = sizeof(si);
               si.fMask = SIF_TRACKPOS;
               GetScrollInfo(hwnd, SB_VERT, &si);
               scy = si.nTrackPos;
               break;
            case SB_PAGEDOWN:
               scy += 10;
               break;
            case SB_PAGEUP:
               scy -= 10;
               break;
            }
            scy = Max(0, Min(wp->doc.linenum-1, scy));
            cy = scy + delta_y;
            cy = Max(0, Min(wp->doc.linenum-1, cy));

            wp->SetScrollPos(wp->scroll_x, scy);
            SetCursorPos1(wp, wp->cursor.x, cy);
            SendMessage(hwnd, WM_PAINT, 0, 0);
            inside = false;
         }
      }
      break;

   case WM_LBUTTONUP:
      ReleaseCapture();
      hwnd_lbut_down = false;
      break;

   case WM_LBUTTONDBLCLK:
      {
         code_manager.edit_functions[MARKWORD-FIRST_COMMAND](&ec->win);
         ec->Redraw();
      }
      break;

      /*
   case WM_RBUTTONDOWN:
      if(ec){
         TV_HITTESTINFO thti;
         int i = GetMessagePos();
         thti.pt.x = LOWORD(i);
         thti.pt.y = HIWORD(i);
         ScreenToClient(GetDlgItem(hwnd, IDC_TREE_PROJECT), &thti.pt);
         ec->ContextMenu(LOWORD(i), HIWORD(i));
      }
      break;
      */

   case WM_MOUSEMOVE:
      if(hwnd_lbut_down && (wParam&MK_LBUTTON)){
         int fsx, fsy;
         ec->GetFontSize(&fsx, &fsy);
         int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
         x = x / fsx + ec->win.scroll_x;
         y = y / fsy + ec->win.scroll_y;
         x = Max(0, x);
         y = Max(0, y);

         SetCursorPos1(&ec->win, x, y);
         ec->win.cursor.redraw = true;
         AdjustBlock(&ec->win, 1, 0);
         ec->Redraw();
      }
      break;

   case WM_GETDLGCODE :
      return DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTMESSAGE | DLGC_WANTTAB;

   case WM_CHAR:
      if(wParam==127) break;
      if(wParam<' ') break;
      if(ec) ec->ProcessInput(uMsg, wParam, lParam);
      break;
   case WM_KEYDOWN:
      if(wParam==VK_APPS){
         if(ec){
            RECT rc;
            GetWindowRect(hwnd, &rc);
            int fsx, fsy;
            ec->GetFontSize(&fsx, &fsy);
            ec->ContextMenu(rc.left + fsx*(ec->win.cursor.x-ec->win.scroll_x+1),
               rc.top + fsy*(ec->win.cursor.y-ec->win.scroll_y+1));
         }
         break;
      }
                              //flow...
   case WM_SYSKEYDOWN:
      if(ec){
                              //keep rerefence, because class may be released during the call
         ec->AddRef();
         ec->ProcessInput(uMsg, wParam, lParam);
         ec->Release();
      }
      break;

   case WM_SYSCHAR:
      return 0;
   }
   return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
 
//----------------------------

void C_edit_control::ContextMenu(int x, int y){
   int i;
   HMENU hmenu;
   hmenu = LoadMenu(hi, "MENU_CONTEXT");
   i = TrackPopupMenu(GetSubMenu(hmenu,0), TPM_LEFTBUTTON |
      TPM_RIGHTBUTTON | TPM_RETURNCMD,
      x, y, 0,
      hwnd, NULL);
   DestroyMenu(hmenu);
   switch(i){
   case -1: break;
   default:
      SendMessage(hwnd, WM_COMMAND, i, (LPARAM)hwnd);
      Redraw();
   }
}

//----------------------------

C_edit_control::C_edit_control():
   ref(1),
   user_macros(NULL),
   read_only(false),
   hi(NULL),
   state(STATE_OK),
   fnt(NULL),
   config_changed(false),
   cb_proc(NULL), cb_context(NULL),
   user_data(0),
   curr_HDC_color(-1),
   font_sx(-1), font_sy(-1),
   hwnd(NULL)
{
   win.ec = this;
   memset(&config, 0, sizeof(config));
}

//----------------------------

C_edit_control::~C_edit_control(){

   Close(true);
   if(cb_proc) (*cb_proc)("exit", cb_context);
}

//----------------------------

ECTRL_RESULT C_edit_control::Init(const char *macro_file, const char *cfg_file1){

   hi = GetModuleHandle(DLL_NAME);
   if(macro_file)
   {
                           //init macro file
      if(!ConvertMacros(macro_file, &user_macros)){
         return ECTRLERR_MACROOPENFAIL;
      }
   }
   cfg_file = NULL;
   {
                           //reset config
      memcpy(&config, &config_defaults, sizeof(S_config));
      memcpy(&curr_font_log, &font_log_defaults, sizeof(font_log_defaults));
      memcpy(&rgb_values, &rgb_values_defaults, sizeof(rgb_values_defaults));
      memset(&custom_colors, 0, sizeof(custom_colors));
      for(int i=0; i<4; i++)
         reserved_word[i].clear();

   }
   if(cfg_file1){
      cfg_file = cfg_file1;

      C_cache ck;
      if(!ck.open(cfg_file, CACHE_READ))
         return ECTRLERR_CONFIGOPENFAIL;
                              //config
      ck.read((char*)&config, sizeof(config));
                              //font log
      ck.read((char*)&curr_font_log, sizeof(curr_font_log));
                              //colors
      ck.read((char*)&rgb_values, sizeof(rgb_values));
                              //keywords
      for(int i=0; i<4; i++){
         char buf[256];
         while(true){
            ck.getline(buf, sizeof(buf), 0);
            if(!buf[0]) break;
            reserved_word[i].push_back(buf);
         }
      }
      ck.close();
   }
   win.overwrite = config.overwrite;

   return ECTRL_OK;
}

//----------------------------

ECTRL_RESULT C_edit_control::Close(bool save_cfg){

   ECTRL_RESULT er = ECTRL_OK;
   /*
   if(win.doc.modified){
      if(!win.Save()) return ECTRLERR_SAVEFAILED;
      er = ECTRL_MODIFIED;
      win.doc.Close();
   }
   */
   win.doc.Close();

   if(save_cfg && cfg_file.Size() && config_changed){
      C_cache ck;
      ck.open(cfg_file, CACHE_WRITE);

                           //write config
      ck.write((char*)&config, sizeof(config));
                           //write font log
      ck.write((char*)&curr_font_log, sizeof(curr_font_log));
                           //write colors
      ck.write((char*)&rgb_values, sizeof(rgb_values));
                           //write keywords
      for(int i=0; i<4; i++){
         for(dword j=0; j<reserved_word[i].size(); j++){
            ck.write((const char*)reserved_word[i][j], reserved_word[i][j].Size()+1);
         }
         char b = 0;
         ck.write(&b, sizeof(char));
      }
      ck.close();
   }
   if(hwnd){
      SetWindowLong(hwnd, GWL_USERDATA, 0);
      DestroyWindow(hwnd);
      hwnd = NULL;
   }

   return er;
}

//----------------------------

ECTRL_RESULT C_edit_control::Save(bool prompt){
   ECTRL_RESULT er = ECTRL_OK;
   if(win.doc.modified){
      if(prompt){
         switch(MessageBox(hwnd, C_fstr("Save changes to %s?", (const char*)win.doc.title), NULL, MB_YESNOCANCEL)){
         case IDYES:
            if(!win.Save()) return ECTRLERR_SAVEFAILED;
            er = ECTRL_MODIFIED;
            break;
         case IDNO:
            break;
         case IDCANCEL: return ECTRLERR_SAVEFAILED;
         }
      }
   }
   return er;
}

//----------------------------

ECTRL_RESULT C_edit_control::Open(const char *filename, void *hwnd_parent, dword flags, void **ret_hwnd,
   const int pos_size[4]){

   C_cache ck;
   if(!ck.open(filename, CACHE_READ))
      return ECTRLERR_OPENFAILED;
   return Open(&ck, filename, hwnd_parent, flags, ret_hwnd, pos_size);
}

//----------------------------

ECTRL_RESULT C_edit_control::Open(class C_cache *ck, const char *title, void *hwnd_parent, dword flags,
   void **ret_hwnd, const int pos_size[4]){

   read_only = true;
   if(ret_hwnd) *ret_hwnd = NULL;
   bool b = win.doc.Open(ck, title, &config);
   if(!b) return ECTRLERR_OPENFAILED;
   read_only = (flags&ECTRL_OPEN_READONLY);

   hwnd = CreateDialogParam(hi, "IDD_EDIT_CONTROL", (HWND)hwnd_parent, dlgProc, (LPARAM)this);
   if(hwnd){
      SetWindowText(hwnd, win.doc.title);

      win.hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,
         "EC_WINDOW", 
         NULL,
         WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL,
         CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
         hwnd,
         NULL, 
         hi, 
         this);

      if(pos_size){
         MoveWindow(hwnd, pos_size[0], pos_size[1], pos_size[2], pos_size[3], true);
      }
      ShowWindow(win.hwnd, SW_SHOW);
      SetFocus(win.hwnd);
   }

   //SizeWindow();            
   if(!(flags&ECTRL_OPEN_HIDDEN)){
      ShowWindow(hwnd, SW_SHOW);
      UpdateWindow(hwnd);
   }

   if(ret_hwnd) *ret_hwnd = hwnd;
   return ECTRL_OK;
}

//----------------------------

void C_edit_control::ProcessInput(UINT uMsg, dword wParam, dword lParam){


                              //try get code
   dword code = 0;
   bool shift_down = false;
   int i;
   
   if(uMsg==WM_KEYDOWN || uMsg==WM_SYSKEYDOWN){
      i = wParam;
      char *cp = (char*)strchr(CODE_CONVERT, i);
      if(cp) i = CODE_CONVERT1[cp-CODE_CONVERT];
      code = code_manager.Get(user_macros, i, lParam, &shift_down);
      //if(!code) code = wParam;
   }else
   if(uMsg==WM_CHAR){
      code = code_manager.Get(user_macros, wParam, lParam, &shift_down);
      if(!code) code = wParam;
   }

   if(code){
      bool check_state = false;
      bool command_result = false;
      short cmd_mode = 0;
      bool neg = false;
      do{
         if(code>=FIRST_COMMAND){
            check_state = true;
            if(code>=SYS_COMMANDS){
               switch(code){
               case JUMP:
                  code_manager.Jump(code_manager.GetNumber());
                  break;
               case JTRUE:
                  i = code_manager.GetNumber();
                  if(command_result) code_manager.Jump(i);
                  break;
               case JFALSE:
                  i=code_manager.GetNumber();
                  if(!command_result) code_manager.Jump(i);
                  break;
               case RETURN:
                  code_manager.Stop();
                  break;
               case AND:                  //if already 0, skip next test
                  cmd_mode = code;
                  if(!command_result){
                     while((code=code_manager.Get(user_macros, 0, 0, &shift_down))==NOT);
                     code_manager.SkipCondCode(code);
                     cmd_mode=0;
                  }
                  break;
               case OR:                   //if already 1, skip next test
                  cmd_mode=code;
                  if(command_result){
                     while((code=code_manager.Get(user_macros, 0, 0, &shift_down))==NOT);
                     code_manager.SkipCondCode(code);
                     cmd_mode=0;
                  }
                  break;
               case XOR:
                  cmd_mode=code;
                  break;
               case NOT:
                  neg ^= 1;
                  break;
               case TRUE:
                  command_result=!neg;
                  break;
               }
            }else{
               i = code_manager.edit_functions[code-FIRST_COMMAND](&win) ^ neg;
               switch(cmd_mode){
               case AND:
                  (byte&)command_result &= i;
                  break;
               case OR:
                  (byte&)command_result |= i;
                  break;
               case XOR:
                  (byte&)command_result ^= i;
                  break;
               default:
                  command_result = i;
               }
               neg = false; cmd_mode = 0;
               //if(CurrWindow())
               {
                  if(code!=UNDO && code!=REDO && code!=MAINMENU && code!=MESSAGE){
                     win.undo[1].Clear();
                     if(code!=FIND && code!=REPEAT)
                        AdjustBlock(&win, shift_down, false);
                  }else
                     AdjustBlock(&win, shift_down, true);
               }
            }
         }else
         if(code>=32){
            check_state = true;
                                          //char
            if(win.block.IsCUA())
               code_manager.edit_functions[DELETEBLOCK-FIRST_COMMAND](&win);
            if(PutCh(&win, code))
               code_manager.edit_functions[CURSORRIGHT-FIRST_COMMAND](&win);
         }
                                    //continue getting commands
         code = code_manager.Get(user_macros, 0, 0, &shift_down);
      }while(code);
      if(check_state)
         CheckState();
   }

   {
      SetCursorVisible(&win, win.hwnd);
      win.cursor.SavePos();
      Redraw();
                              //break undo level
      win.undo[0].Add(C_undo_element::MARK);
   }
}

//----------------------------

void C_edit_control::Redraw(){
   if(win.doc.redraw){
      PostMessage(win.hwnd, WM_USER_PAINTWHOLE, 0, 0);
      win.doc.redraw = false;
   }else
   if(win.doc.redraw_line){
      SendMessage(win.hwnd, WM_USER_PAINTLINE, win.cursor.y, TRUE);
      win.doc.redraw_line = false;
   }
   if(win.cursor.redraw){
      SendMessage(win.hwnd, WM_USER_MOVECURSOR, 0, 0);
      DrawLineColumn();
      win.cursor.redraw = false;
   }
}

//----------------------------

void C_edit_control::SetFont(HDC hdc, int *sx, int *sy){

   if(!fnt) fnt = CreateFontIndirect(&curr_font_log);
   SelectObject(hdc, fnt);
   if(font_sy==-1){
      TEXTMETRIC tm;
      GetTextMetrics(hdc, &tm);
      font_sy = tm.tmHeight;
      font_sx = tm.tmAveCharWidth;
   }
   *sx = font_sx;
   *sy = font_sy;
}

//----------------------------

void C_edit_control::GetFontSize(int *sx, int *sy){
   
   if(font_sy!=-1){
      *sx = font_sx;
      *sy = font_sy;
   }else{
      HDC hdc = GetDC(win.hwnd);
      SetFont(hdc, sx, sy);
      ReleaseDC(win.hwnd, hdc);
   }
}

//----------------------------

void C_edit_control::SetHDCColors(HDC hdc, byte c, bool line_highl){

   if(line_highl!=curr_highl){
      ResetHDCColors();
      curr_highl=line_highl;
   }

   byte cb;
   byte cf;
   if(curr_HDC_color==-1){
      cb=c+16;
      cf=c+1;
   }else{
      cb=curr_HDC_color>>4;
      cf=curr_HDC_color&15;
   }
   if(cb!=(c>>4)){
      cb=c>>4;
      SetBkColor(hdc, GetRGB(cb, 0));
   }
   if(cf!=(c&15)){
      cf=c&15;
      SetTextColor(hdc, GetRGB(cf, line_highl));
   }
   curr_HDC_color=(cb<<4)|cf;
}

//----------------------------

void C_edit_control::ResetHDCColors(){
   curr_HDC_color = -1;
}

//----------------------------

int C_edit_control::CheckReservedWord(char *text, byte len){

   for(int i=0; i<4; i++)
   for(int j=reserved_word[i].size(); j--; )
   if(reserved_word[i][j].Size()==len && !memcmp(text, (const char*)reserved_word[i][j], len))
      return i;
   return -1;
}

//----------------------------

void C_edit_control::SetCurrPos(int line, int row){

   SetCursorPos1(&win, row, line);
   {
      SetCursorVisible(&win, win.hwnd);
      win.cursor.SavePos();
      Redraw();
                              //break undo level
      win.undo[0].Add(C_undo_element::MARK);
   }
}

//----------------------------
//----------------------------

PC_edit_control __declspec(dllexport) CreateEditControl(){
   return new C_edit_control;
}

//----------------------------
