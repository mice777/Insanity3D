#include "pch.h"


//----------------------------

struct S_help_init{
   int x_pos;
   int y_pos;
   int width;
   const char *txt;
   HWND hwnd_parent;
};

//----------------------------

static int PaintText(HWND hwnd, const char *txt){

   PAINTSTRUCT ps;
   HDC hdc = BeginPaint(hwnd, &ps);
   assert(hdc);
   HFONT fnt = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
   assert(fnt);
   HFONT fnt_save = (HFONT)SelectObject(hdc, fnt);
   int save_mm = SetMapMode(hdc, MM_TEXT);
   assert(save_mm);
   SetBkMode(hdc, TRANSPARENT);
   RECT rc;
   GetClientRect(hwnd, &rc);
   rc.top += 2;
   rc.left += 4;
   rc.right -= 8;
   int h = DrawText(hdc, txt, strlen(txt), &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);

   SelectObject(hdc, fnt_save);
   SetMapMode(hdc, save_mm);
   EndPaint(hwnd, &ps);
   return h + 8;
}

//----------------------------

static BOOL CALLBACK dlgHelp(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         const S_help_init *hi = (S_help_init*)lParam;

         SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_STATICEDGE);
         SetWindowLong(hwnd, GWL_STYLE, WS_BORDER | WS_POPUP);
                              //position window properly
         POINT pos, size;
         size.x = hi->width;
         if(!size.x)
            size.x = 240;

         pos.x = hi->x_pos;
         pos.y = hi->y_pos;
         if(hi->hwnd_parent)
            ClientToScreen(hi->hwnd_parent, &pos);
                              //compute height
         {
            SetWindowPos(hwnd, NULL, 0, 0, size.x, 1000, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            size.y = PaintText(hwnd, hi->txt);
         }
                              //clip onto screen
         int sx = GetSystemMetrics(SM_CXSCREEN);
         int sy = GetSystemMetrics(SM_CYSCREEN);
         if(pos.x + size.x > sx) pos.x = sx - size.x;
         if(pos.y + size.y > sy) pos.y = sy - size.y;
         if(pos.x < 0) pos.x = 0;
         if(pos.y < 0) pos.y = 0;
         SetWindowPos(hwnd, NULL, pos.x, pos.y, size.x, size.y, SWP_NOZORDER | SWP_NOOWNERZORDER);
         ShowWindow(hwnd, SW_SHOW);
      }
      return 1;

   case WM_CTLCOLORDLG:
      {
         HDC hdc = (HDC)wParam;
         dword color = GetSysColor(COLOR_INFOBK);
         HBRUSH hbr = CreateSolidBrush(color&0xffffff);
         SelectObject(hdc, hbr);
         return *(BOOL*)&hbr;
      }
      break;

   case WM_PAINT:
      {
         const S_help_init *hi = (S_help_init*)GetWindowLong(hwnd, GWL_USERDATA);
         PaintText(hwnd, hi->txt);
         return 0;
      }
      break;

   case WM_KEYDOWN:
   case WM_CHAR:
   case WM_LBUTTONDOWN:
   case WM_RBUTTONDOWN:
   case WM_MBUTTONDOWN:
      EndDialog(hwnd, 0);
      break;

   case WM_KILLFOCUS:
      EndDialog(hwnd, 0);
      break;

   case WM_GETDLGCODE:
      return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;

   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDCANCEL:
         EndDialog(hwnd, 0);
         break;
      case IDOK:
         EndDialog(hwnd, 0);
         break;
      }
      break;
   }
   return 0;
}


//----------------------------

bool OsDisplayHelpWindow(const char *text, void *hwnd_parent, int x, int y, int width){

   S_help_init hi = {
      x,
      y,
      width,
      text,
      (HWND)hwnd_parent
   };
   static const struct{
      DLGTEMPLATE hdr;
      word menu;
      word cls;
      word title;
      word font_size;
      wchar_t font_name[14];
   } dt = {
      {
         DS_3DLOOK | DS_SETFONT,     //style
         0,                   //ext style (unused)
         0,                   //# of items in dialog
         0, 0,                //position
         10, 10,              //size
      },
      0,                      //menu
      0,                      //class
      0,                      //title
      8,                      //font size
      {L"MS Sans Serif"}
   };
   return (DialogBoxIndirectParam(GetModuleHandle(NULL), &dt.hdr,
      //(HWND)hwnd_parent,
      NULL,
      dlgHelp, (LPARAM)&hi) != -1);
}

//----------------------------
