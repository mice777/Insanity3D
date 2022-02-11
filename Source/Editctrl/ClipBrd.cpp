#include "all.h"
#include "ectrl_i.h"

//-------------------------------------

C_clipboard::C_clipboard(): handle(NULL)
{
   cf = RegisterClipboardFormat("Highware_Text");
}

//-------------------------------------

bool C_clipboard::IsEmpty(){
   return (!IsClipboardFormatAvailable(cf) &&
      !IsClipboardFormatAvailable(CF_TEXT));
}

//-------------------------------------

void C_clipboard::Empty(){
   EmptyClipboard();
}

//-------------------------------------

bool C_clipboard::Copy(C_document *doc, C_block *bl, HWND hwnd){

   if(bl->IsEmpty()) return false;
                                       //prepare to copy
   if(!OpenClipboard(hwnd)) return false;
   EmptyClipboard();
   int l, r = 0;
   int line_len;
   int copy_len;
   int len=0;
   C_document_line *lp;
   int i;
                                       //count text len
   for(i=bl->ymin; i<=bl->ymax; i++){
      lp=doc->lines[i];
      bl->GetLineInfo(i, &l, &r);
      line_len=r-l+1;
      l=min(l, lp->text_len);
      r=min(r, lp->text_len-1)+1;
      copy_len=r-l;
      if(bl->type!=COLUMN) line_len=copy_len;
      len+=line_len+2;
   }
   for(int fmt=0; fmt<2; fmt++){
                                       //alloc mem
      HGLOBAL hg=GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, len+3);
      if(!hg){
         CloseClipboard();
         return 0;
      }
      char *cp=(char*)GlobalLock(hg);
      int copy_ptr=0;
                                       //registered format uses type
      if(!fmt)
         cp[copy_ptr++] = (char)bl->type;
                                       //copy all lines
      for(i=bl->ymin; i<=bl->ymax; i++){
         lp=doc->lines[i];
         bl->GetLineInfo(i, &l, &r);
         line_len=r-l+1;               //for case of column type
         l=min(l, lp->text_len);
         r=min(r, lp->text_len-1)+1;
         copy_len=r-l;
         if(bl->type!=COLUMN) line_len=copy_len;
                                          //copy text
         memcpy(&cp[copy_ptr], &lp->text[l], copy_len);
                                          //fill (column) area by space
         memset(&cp[copy_ptr+copy_len], ' ', line_len-copy_len);
         copy_ptr+=line_len;
         if(i!=bl->ymax){
            cp[copy_ptr++]='\r';
            cp[copy_ptr++]='\n';
         }
      }
                                       //eol
      cp[copy_ptr++]=0;
      GlobalUnlock(hg);
                                          //feed clipboard
      SetClipboardData(fmt ? CF_TEXT : cf, hg);
   }
   CloseClipboard();
   return true;
}

//-------------------------------------

bool C_clipboard::Destroy(){
   if(!handle) return false;
   GlobalFree(handle);
   handle=NULL;
   return true;
}

//-------------------------------------

