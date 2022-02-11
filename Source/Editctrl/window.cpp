#include "all.h"
#include "ectrl_i.h"



void DrawLineColumn(int x, int y);

//----------------------------

C_window::C_window():
   scroll_x(0),
   scroll_y(0),
   overwrite(false),
   hwnd(NULL),
   redraw(true)
{
   undo[0].Add(C_undo_element::MARK);
}

//----------------------------

void C_window::Activate(){
   SetFocus(hwnd);
}

//----------------------------

bool C_window::Save(){

   if(doc.unnamed){
                                       //prompt for name
      char buf[257];
      strcpy(buf, doc.title);
      OPENFILENAME of;
      memset(&of, 0, sizeof(of));
      of.lStructSize = sizeof(of);
      //of.hwndOwner = hwnd_main;
      of.hInstance = ec->hi;
      of.lpstrFile = buf;
      of.nMaxFile = sizeof(buf)-1;
      of.lpstrFileTitle = buf;
      of.nMaxFileTitle = sizeof(buf)-1;
      of.lpstrInitialDir = NULL;
      of.Flags = OFN_OVERWRITEPROMPT;
      if(!GetSaveFileName(&of)) return false;

      doc.SetTitle(buf);
      doc.modified = true;
      doc.unnamed = false;
   }
   if(!doc.Write()) return false;

                              //parse undo buffer and change all FILEMODIFY elements to NOP
   {
      for(int i=UNDO_BUFFER_ELEMENTS; i--; ){
         if(undo[0].elements[i] && undo[0].elements[i]->id==C_undo_element::FILEMODIFY){
            undo[0].elements[i]->id = C_undo_element::NOP;
         }
      }
   }

   SetWindowText(ec->hwnd, doc.title);
   return true;
}

//----------------------------

bool C_window::SetScrollPos(int x, int y, int undo_buff){

   if(scroll_x!=x || scroll_y!=y){
                                       //add position into undo buffer
      undo[undo_buff].Add(C_undo_element::SCROLL, scroll_x, scroll_y);
      scroll_x = x;
      scroll_y = y;
      doc.redraw = true;
      cursor.redraw = true;

                              //setup scroll-bar
      SCROLLINFO si;
      si.cbSize = sizeof(si);
      si.fMask = SIF_RANGE | SIF_POS;
      si.nMin = 0;
      si.nMax = doc.linenum;
      si.nPos = scroll_y;
      SetScrollInfo(hwnd, SB_VERT, &si, true);

      return true;
   }
   return false;
}

//----------------------------

/*
C_str C_window::ExtractFilename() const{

   dword i = doc.title.Size();
   while(i && doc.title[i-1]!='\\') --i;
   if(ec->read_only)
      return C_fstr("%s [read-only]", (const char*)doc.title + i);
   return &doc.title[i];
}
*/

//----------------------------

