#include "pch.h"

//----------------------------

#pragma comment (lib,"user32.lib")

//----------------------------

MSGBOX_RETURN OsMessageBox(void *hwnd, const char *txt, const char *title, MSGBOX_STYLE wmb){

   int wtype;
   switch(wmb){
   case MBOX_OK: wtype = MB_OK; break;
   case MBOX_YESNO: wtype = MB_YESNO; break;
   case MBOX_YESNOCANCEL: wtype = MB_YESNOCANCEL; break;
   case MBOX_RETRYCANCEL: wtype = MB_RETRYCANCEL; break;
   case MBOX_OKCANCEL: wtype = MB_OKCANCEL; break;
   default: wtype = MB_OK;
   }
   int ret = MessageBox((HWND)hwnd, txt, title, wtype);
   switch(ret){
   case IDYES: return MBOX_RETURN_YES;
   case IDOK: return MBOX_RETURN_YES;
   case IDNO: return MBOX_RETURN_NO;
   case IDCANCEL: return MBOX_RETURN_CANCEL;
   case IDRETRY: return MBOX_RETURN_YES;
   }
   assert(0);
   return MBOX_RETURN_NO;
}

//----------------------------
