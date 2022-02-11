#include "pch.h"

//----------------------------

void OsDestroyWindow(void *hwnd){
   DestroyWindow((HWND)hwnd);
}

//----------------------------

void OsShowWindow(void *hwnd, bool on){

   if(on){
      ShowWindow((HWND)hwnd, SW_SHOW);
   }else{
      ShowWindow((HWND)hwnd, SW_HIDE);
   }
   UpdateWindow((HWND)hwnd);
}

//----------------------------

void OsCenterWindow(void *hwnd, void *hwnd_other){

   RECT rc;
   GetWindowRect((HWND)hwnd, &rc);
   int x, y, sx, sy;
   if(hwnd_other){
      RECT rc;
      GetWindowRect((HWND)hwnd_other, &rc);
      x = rc.left;
      y = rc.top;
      sx = rc.right - x;
      sy = rc.bottom - y;
   }else{
      RECT rc;
      SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
      x = rc.left;
      y = rc.top;
      sx = rc.right - x;
      sy = rc.bottom - y;
      /*
      x = y = 0;
      sx = GetSystemMetrics(SM_CXSCREEN);
      sy = GetSystemMetrics(SM_CYSCREEN);
      */
   }
   SetWindowPos((HWND)hwnd, NULL,
      x + (sx-(rc.right-rc.left))/2,
      y + (sy-(rc.bottom-rc.top))/2,
      0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

//----------------------------

void *OsCreateFont(int size){

   HFONT h_font =
      CreateFont(
      -size,                  //height
      0,                      //width
      0,                      //escapement
      0,                      //orientation
      0,                      //weight
      false,                  //italic
      false,                  //underline
      false,                  //stike-out
      OEM_CHARSET,
      OUT_RASTER_PRECIS,
      CLIP_DEFAULT_PRECIS,
      DEFAULT_QUALITY,
      VARIABLE_PITCH, NULL
      );
   return h_font;
}

//----------------------------

void OsDestroyFont(void *h_font){

   DeleteObject((HFONT)h_font);
}

//----------------------------
