#include "all.h"
#include "ectrl_i.h"
#include "resource.h"

#define todo(n)

//----------------------------

struct S_sys_dlg_param{
   void (*init_proc)(HWND);
   int (*close_proc)(HWND);
   HWND hwnd_parent;
   C_edit_control *ec;
};

static BOOL CALLBACK DlgSysCMD(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   int i;
   switch(uMsg){
   case WM_INITDIALOG:
      {
         S_sys_dlg_param *sp = (S_sys_dlg_param*)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)sp);
                              //center
         HWND hwnd1 = sp->hwnd_parent;
         RECT rc, rc1;
         GetWindowRect(hwnd1, &rc1);
         GetWindowRect(hwnd, &rc);
         SetWindowPos(hwnd, 0, ((rc1.right-rc1.left)-(rc.right-rc.left))/2+rc1.left,
            ((rc1.bottom-rc1.top)-(rc.bottom-rc.top))/2+rc1.top, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

         sp->init_proc(hwnd);

         HWND ctl_hwnd = GetNextDlgTabItem(hwnd, NULL, 0);
                              //fill dialog by key commands
         while(!code_manager.IsEmpty()){
            short code = (short)code_manager.Get(sp->ec->user_macros, 0, 0, NULL, 0);
            if(code<FIRST_COMMAND){
               SendDlgItemMessage(hwnd, GetDlgCtrlID(ctl_hwnd), WM_CHAR, code, 1);
               if(code==' '){
                  SendDlgItemMessage(hwnd, GetDlgCtrlID(ctl_hwnd), BM_CLICK, 0, 0);
               }
            }else
            switch(code){
            case PASTE:
               SetDlgItemText(hwnd, GetDlgCtrlID(ctl_hwnd), "");
               SendDlgItemMessage(hwnd, GetDlgCtrlID(ctl_hwnd), WM_PASTE, 0, 0);
               break;
            case TABRT:
               PostMessage(hwnd, WM_NEXTDLGCTL, 0, 0);
               ctl_hwnd=GetNextDlgTabItem(hwnd, ctl_hwnd, 0);
               break;
            case TABLT:
               PostMessage(hwnd, WM_NEXTDLGCTL, 0, 0);
               ctl_hwnd=GetNextDlgTabItem(hwnd, ctl_hwnd, 1);
               break;
            case BACKSPACE:
               SendDlgItemMessage(hwnd, GetDlgCtrlID(ctl_hwnd), WM_CHAR, '\b', 1);
               break;
            case CURSORDOWN:
               ctl_hwnd=GetNextDlgGroupItem(hwnd, ctl_hwnd, 0);
               PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)ctl_hwnd, 1);
               break;
            case CURSORUP:
               ctl_hwnd=GetNextDlgGroupItem(hwnd, ctl_hwnd, 1);
               PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)ctl_hwnd, 1);
               break;
            case RETURN:
               code_manager.Stop();
               break;
            case ENTER:
               PostMessage(hwnd, WM_KEYDOWN, 0xd, 1);
               break;
            case CURRFILENAME:
            case CURRNAME:
            case CURRPATH:
            case CURRLINE:
            case CURRROW:
               code_manager.edit_functions[code-FIRST_COMMAND](&sp->ec->win);
               break;
            }
         }
      }
      return 1;

   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDCANCEL:
         EndDialog(hwnd, -1);
         break;
      case IDOK:
         i = 1;
         {
            S_sys_dlg_param *sp = (S_sys_dlg_param*)GetWindowLong(hwnd, GWL_USERDATA);
            if(sp){
               i = sp->close_proc(hwnd);
            }
         }
         EndDialog(hwnd, i);
         break;
      case IDNO:
      case ID_ALL:
         EndDialog(hwnd, LOWORD(wParam));
         break;
      }
      break;
   }
   return 0;
}

//----------------------------
                                       //conditions
static bool IsBegLine(C_window *wp){
   return (wp->cursor.x==0);
}

static bool IsEndLine(C_window *wp){
   wp->doc.lines[wp->cursor.y]->RemoveTrailingSpaces();
   return (wp->cursor.x==
      wp->doc.lines[wp->cursor.y]->text_len);
}

static bool IsAfterEndLine(C_window *wp){
   wp->doc.lines[wp->cursor.y]->RemoveTrailingSpaces();
   return (wp->cursor.x>=
      wp->doc.lines[wp->cursor.y]->text_len);
}

static bool IsFirstLine(C_window *wp){
   return (wp->cursor.y==0);
}

static bool IsLastLine(C_window *wp){
   return (wp->cursor.y == (int)wp->doc.linenum-1);
}

static bool IsBegFile(C_window *wp){
   return IsFirstLine(wp) && IsBegLine(wp);
}

static bool IsEndFile(C_window *wp){
   return IsLastLine(wp) && IsAfterEndLine(wp);
}

static bool IsEmptyLine(C_window *wp){
   return wp->CurrLine()->text_len==0;
}

char CurrChar(C_window *wp){
   if(IsAfterEndLine(wp)) return ' ';
   return wp->CurrLine()->text[wp->cursor.x];
}

static bool IsWord(C_window *wp){
   return IsWordChar(CurrChar(wp));
}

static bool IsBlock(C_window *wp){
   return !wp->block.IsEmpty();
}

static bool IsAreaPartOf(C_window *wp){
                                       //get compared text
   char test_text[64];
   int pos=0;
   short code;
   while((code=code_manager.Peek())!=BR && code<FIRST_COMMAND && pos<sizeof(test_text)-1){
      test_text[pos++] = (char)code_manager.Get(wp->ec->user_macros, 0, 0);
   }
   test_text[pos++]=0;
   if(!IsBlock(wp)) return 0;
   int l, r = 0;
   int line=wp->cursor.y;
   wp->block.GetLineInfo(line, &l, &r);
                                       //get marked text
   pos=min(wp->doc.lines[line]->text_len, l);
   int len=max(0, (min(wp->doc.lines[line]->text_len-1, r)-l)+1);
   if(!len || len > (int)strlen(test_text)) return 0;
   return !strncmp(wp->doc.lines[line]->text+pos, test_text, len);
}

static bool IsUpper(C_window *wp){
   return isupper(CurrChar(wp));
}

static bool IsLower(C_window *wp){
   return islower(CurrChar(wp));
}

static bool IsDigit(C_window *wp){
   return (bool)isdigit(CurrChar(wp));
}

static bool IsSpace(C_window *wp){
   return isspace(CurrChar(wp));
}

bool IsCurrChar(char c, C_window *wp){
   return CurrChar(wp)==c;
}

static bool IsCurrChar(C_window *wp){

   return IsCurrChar((char)code_manager.Get(wp->ec->user_macros, 0, 0), wp);
}

static bool IsCursorInBlock(C_window *wp){
   int l, r = 0;
   wp->block.GetLineInfo(wp->cursor.y, &l, &r);
   int cx = wp->cursor.x;
   return cx>=l && cx<=r;
}

static bool IsCUABlock(C_window *wp){
   return wp->block.IsCUA();
}

static bool IsFileReadOnly(C_window *wp){
   return (wp->ec->read_only);
}

//----------------------------
                              //cursor
static bool CursorRight(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.x+1, wp->cursor.y);
}

static bool CursorLeft(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.x-1, wp->cursor.y);
}

static bool CursorDown(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.x, wp->cursor.y+1);
}

static bool CursorUp(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.x, wp->cursor.y-1);
}

static bool BegLine(C_window *wp){
   return SetCursorPos1(wp, 0, wp->cursor.y);
}

static bool EndLine(C_window *wp){
   if(IsEndLine(wp)) return false;
   int line=wp->cursor.y;
   return SetCursorPos1(wp, wp->doc.lines[line]->text_len, line);
}

static bool BegFile(C_window *wp){
   return SetCursorPos1(wp, 0, 0);
}

static bool EndFile(C_window *wp){
   int lastline=wp->doc.linenum-1;
   return SetCursorPos1(wp, wp->doc.lines[lastline]->text_len, lastline);
}

static bool BegScreen(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.x, wp->scroll_y);
}

static bool EndScreen(C_window *wp){
   int fsx, fsy;
   RECT rc;
   GetClientRect(wp->hwnd, &rc);
   wp->ec->GetFontSize(&fsx, &fsy);
   int sy = rc.bottom/fsy;
   return SetCursorPos1(wp, wp->cursor.x, wp->scroll_y+sy-1);
}

static bool WordLeft(C_window *wp){
   if(IsBegFile(wp)) return false;
   CursorLeft(wp);
   if(IsBegLine(wp)){
      if(IsFirstLine(wp)) BegLine(wp);
      else{
         CursorUp(wp);
         EndLine(wp);
      }
      return true;
   }
   bool was_word = IsWord(wp);
   do{
      CursorLeft(wp);
      bool is_word = IsWord(wp);
      if(was_word && !is_word){
         CursorRight(wp);
         return true;
      }
      was_word=is_word;
      if(IsBegLine(wp)){
         if(!IsWord(wp)){
            if(IsFirstLine(wp)) BegLine(wp);
            else{
               CursorUp(wp);
               EndLine(wp);
            }
         }
         return true;
      }
   }while(!IsBegFile(wp));
   return true;
}

static bool WordRight(C_window *wp){
   if(IsEndFile(wp)) return false;
   bool was_word=IsWord(wp);
   if(IsAfterEndLine(wp)){
      BegLine(wp);
      CursorDown(wp);
      if(IsEndLine(wp)) return true;
   }
   do{
      bool is_word=IsWord(wp);
      if(is_word && !was_word) return true;
      was_word=is_word;
      CursorRight(wp);
      if(IsEndLine(wp)) return true;
   }while(!IsBegFile(wp));
   return true;
}

static bool PageUp(C_window *wp, int pagepart){
   if(!wp->scroll_y) return false;
   int cdy=wp->cursor.y-wp->scroll_y;

   int fsx, fsy;
   RECT rc;
   GetClientRect(wp->hwnd, &rc);
   wp->ec->GetFontSize(&fsx, &fsy);
   int sy=rc.bottom/fsy;

   wp->SetScrollPos(wp->scroll_x, Max(0, wp->scroll_y-(sy/pagepart)+1));
   SetCursorPos1(wp, wp->cursor.x, Min((int)wp->doc.linenum-1, wp->scroll_y+cdy));
   return true;
}

static bool PageDown(C_window *wp, int pagepart){
   if(wp->scroll_y==wp->doc.linenum-1) return false;
   int cdy=wp->cursor.y-wp->scroll_y;

   int fsx, fsy;
   RECT rc;
   GetClientRect(wp->hwnd, &rc);
   wp->ec->GetFontSize(&fsx, &fsy);
   int sy=rc.bottom/fsy;

   wp->SetScrollPos(wp->scroll_x, min(wp->doc.linenum-1, wp->scroll_y+(sy/pagepart)-1));
   SetCursorPos1(wp, wp->cursor.x, min(wp->doc.linenum-1, wp->scroll_y+cdy));
   return true;
}

static bool PageUp(C_window *wp){
   return PageUp(wp, 1);
}

static bool PageDown(C_window *wp){
   return PageDown(wp, 1);
}

static bool HalfPageUp(C_window *wp){
   return PageUp(wp, 2);
}

static bool HalfPageDown(C_window *wp){
   return PageDown(wp, 2);
}

static bool ScrollUp(C_window *wp){
   if(wp->scroll_y==wp->doc.linenum-1 || IsLastLine(wp)) return false;
   wp->SetScrollPos(wp->scroll_x, wp->scroll_y+1);
   SetCursorPos1(wp, wp->cursor.x, wp->cursor.y+1);
   return true;
}

static bool ScrollDown(C_window *wp){
   if(!wp->scroll_y) return false;
   wp->SetScrollPos(wp->scroll_x, wp->scroll_y-1);
   SetCursorPos1(wp, wp->cursor.x, wp->cursor.y-1);
   return true;
}

static bool MakeCtrLine(C_window *wp){
   int fsx, fsy;
   RECT rc;
   GetClientRect(wp->hwnd, &rc);
   wp->ec->GetFontSize(&fsx, &fsy);
   int sy=rc.bottom/fsy;

   wp->SetScrollPos(wp->scroll_x, max(0, wp->cursor.y-sy/2));
   return true;
}

static bool FirstNonWhite(C_window *wp){
   int x = wp->cursor.x;
   BegLine(wp);
   while(!IsEndLine(wp) && isspace(CurrChar(wp))) CursorRight(wp);
   return x!=wp->cursor.x;
}

static bool WordBeg(C_window *wp){
   if(!IsWord(wp)) return false;
   do{
      if(!CursorLeft(wp)) return true;
   }while(IsWord(wp));
   CursorRight(wp);
   return true;
}

static bool WordEnd(C_window *wp){
   if(!IsWord(wp)) return false;
   do{
      CursorRight(wp);
   }while(IsWord(wp));
   CursorLeft(wp);
   return true;
}

static bool GotoBlockBeg(C_window *wp){
   if(wp->block.IsEmpty()) return false;
   int l, r;
   wp->block.GetLineInfo(wp->block.ymin, &l, &r);
   if(wp->cursor.x==l && wp->cursor.y==wp->block.ymin) return false;
   return SetCursorPos1(wp, l, wp->block.ymin);
}

static bool GotoBlockEnd(C_window *wp){
   if(wp->block.IsEmpty()) return false;
   int l, r = 0;
   wp->block.GetLineInfo(wp->block.ymax, &l, &r);
   if(r>=MAX_ROWS) r=wp->cursor.x;
   else ++r;
   if(wp->cursor.x==r && wp->cursor.y==wp->block.ymax) return false;
   return SetCursorPos1(wp, r, wp->block.ymax);
}

                                       //state changing
bool ToggleIns(C_window *wp, int undo_buffer){
   wp->overwrite ^= 1;
   wp->undo[undo_buffer].Add(C_undo_element::STATECHANGE, 0);
   DestroyCaret();
   wp->ec->MakeCaret();
   return true;
}

                                       //state
static bool ToggleInsert(C_window *wp){
   return ToggleIns(wp, 0);
}

static bool Undo(C_window *wp){
   wp->undo[1].Add(C_undo_element::MARK);
   return wp->undo[0].Undo(wp, 1);
}

static bool Redo(C_window *wp){
   wp->undo[0].Add(C_undo_element::MARK);
   return wp->undo[1].Undo(wp, 0);
}

static bool MainMenu(C_window *wp){
   SendMessage(wp->ec->hwnd, WM_KEYDOWN, VK_F10, 1);
   return true;
}

//----------------------------
                                       //lines
static bool JoinLine(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(IsLastLine(wp)) return false;
   int line = wp->cursor.y;
   C_document_line *lp=wp->doc.lines[line+1];
   if(!AddArea(wp, lp->text, wp->doc.lines[line]->text_len,
      lp->text_len, line)) return false;
   DeleteLine(wp, line+1);
   wp->block.KillLine(wp, 0, line+1);
   wp->doc.CheckComments(&wp->ec->config, line, wp->doc.lines[line]->commented, 0, 1);
   wp->doc.redraw = true;
   return true;
}

//----------------------------

bool PutCh(C_window *wp, char c){

   if(wp->overwrite && !IsAfterEndLine(wp))
      DelChar(wp);
   if(!PutChar(wp, c))
      return false;
   wp->block.InsertArea(wp, 0, wp->cursor.y, wp->cursor.x, 1);
   return true;
}

//----------------------------

static bool DelCh(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(!DelChar(wp))
      return false;
   wp->block.KillArea(wp, 0, wp->cursor.y, wp->cursor.x, 1);
   return true;
}

//----------------------------

static bool BackSpace(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(CursorLeft(wp)){
      if(!IsAfterEndLine(wp))
         return DelCh(wp);
      return false;
   }else{
                              //join lines
      return (CursorUp(wp) && (EndLine(wp), DelCh(wp)));
   }
}

//----------------------------

static bool DelToEol(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(IsAfterEndLine(wp)) return false;
   int row=wp->cursor.x;
   int line=wp->cursor.y;
   KillArea(wp, wp->doc.lines[line]->text_len-row, row, line);
   wp->doc.redraw_line = true;
   return true;
}

//----------------------------

static bool DelLine(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(IsLastLine(wp)){
      BegLine(wp);
      return DelToEol(wp);
   }
   if(!DeleteLine(wp, wp->cursor.y)) return false;
   wp->block.KillLine(wp, 0, wp->cursor.y);
   return true;
}

//----------------------------

static bool SplitLine(C_window *wp){

   if(wp->ec->read_only)
      return false;
   int line = wp->cursor.y;
   int row = wp->cursor.x;
   int len = wp->doc.lines[line]->text_len;
   int copy_len = Max(0, len-row);
   if(!InsertLine(wp, &wp->doc.lines[line]->text[len-copy_len], copy_len, line+1))
      return false;
   wp->block.InsertLine(wp, 0, line);
   DelToEol(wp);
   wp->doc.CheckComments(&wp->ec->config, line, wp->doc.lines[line]->commented, 0, 1);
   wp->doc.redraw = true;
   return true;
}

//----------------------------

static bool TabRt(C_window *wp){

   if(wp->ec->read_only)
      return false;
   int pos = wp->cursor.x;
   int to;
   to = Min(MAX_ROWS, (pos+wp->ec->config.tab_width)-(pos+wp->ec->config.tab_width)%wp->ec->config.tab_width);
   if(pos==to) return false;
   do{
      if(!wp->overwrite) PutChar(wp, ' ');
      CursorRight(wp);
   }while(++pos<to);
   return true;
}

//----------------------------

static bool TabLt(C_window *wp){

   if(wp->ec->read_only)
      return false;
   int pos = wp->cursor.x;
   if(!pos) return false;
   int to;
   to = max(0, pos-1-(pos-1)%wp->ec->config.tab_width);
   if(pos==to) return false;
   do{
      CursorLeft(wp);
      if(!wp->overwrite){
         if(!IsCurrChar(' ', wp)){
            CursorRight(wp);
            return false;
         }
         if(!IsAfterEndLine(wp)) DelCh(wp);
      }
   }while(--pos>to);
   return true;
}

//----------------------------

static bool TabBlockRt(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(wp->block.IsEmpty() || wp->block.type!=LINE)
      return false;
   int i;
   char *cp;
   cp = new char[wp->ec->config.tab_width];
   memset(cp, ' ', wp->ec->config.tab_width);
   for(i=wp->block.ymin; i<=wp->block.ymax; i++)
      AddArea(wp, cp, 0, wp->ec->config.tab_width, i);
   delete[] cp;
   if(wp->block.ymin==wp->block.ymax==wp->cursor.y) wp->doc.redraw_line = true;
   else wp->doc.redraw = true;
   return true;
}

//----------------------------

static bool TabBlockLt(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(wp->block.IsEmpty() || wp->block.type!=LINE)
      return false;
   int i;
   for(i=wp->block.ymin; i<=wp->block.ymax; i++)
      KillArea(wp, wp->ec->config.tab_width, 0, i);
   if(wp->block.ymin==wp->block.ymax==wp->cursor.y) wp->doc.redraw_line = true;
   else wp->doc.redraw = true;
   return true;
}

//----------------------------

static bool DupLine(C_window *wp){

   int line = wp->cursor.y;
   C_document_line *lp=wp->doc.lines[line];
   if(!InsertLine(wp, lp->text, lp->text_len, line))
      return false;
   wp->block.InsertLine(wp, 0, line);
   CursorDown(wp);
   wp->doc.redraw = true;
   return true;
}

//----------------------------

static bool InsertLine(C_window *wp, int line){
   if(!InsertLine(wp, NULL, 0, line))
      return false;
   wp->block.InsertLine(wp, 0, line);
   return true;
}

//----------------------------

static bool InsertLine(C_window *wp){
   return InsertLine(wp, wp->cursor.y);
}

//----------------------------

static bool AddLine(C_window *wp){
   return InsertLine(wp, wp->cursor.y+1);
}

//----------------------------

static bool Enter(C_window *wp){
   if(SplitLine(wp)){
      CursorDown(wp);
      BegLine(wp);
      return true;
   }
   return false;
}

//----------------------------

static bool Beep1(C_window *wp){
   MessageBeep(0xffffffff);
   return true;
}

//----------------------------

static bool Message(C_window *wp){

   char buf[256];
   int pos = 0;
   short code;
   while((code=code_manager.Peek())!=BR && code<FIRST_COMMAND && pos<sizeof(buf)-1){
      buf[pos++] = (char)code_manager.Get(NULL, 0, 0);
   }
   buf[pos++] = 0;
   wp->ec->SetState(STATE_MESSAGE, buf);
   return true;
}

//----------------------------

static bool TransformBlock(C_window *wp, t_trans_proc *trans_proc){

   if(wp->ec->read_only)
      return false;
   if(wp->block.IsEmpty())
      return false;
   int y1=wp->block.ymin;
   int y2=wp->block.ymax;
   while(y1<=y2){
      int l, r = 0;
      wp->block.GetLineInfo(y1, &l, &r);
      r=min(r, wp->doc.lines[y1]->text_len);
      TransfromArea(wp, r-l+1, l, y1, trans_proc);
      ++y1;
   }
   return true;
}

//----------------------------

static int ToUpper(int c){
   return (c>='a' && c<='z') ? c + ('A'-'a') : c;
}

//----------------------------

static int ToLower(int c){
   return (c>='A' && c<='Z') ? c + ('a'-'A') : c;
}

//----------------------------

static bool Upper(C_window *wp){
   if(wp->ec->read_only)
      return false;
   if(IsCursorInBlock(wp))
      return TransformBlock(wp, ToUpper);
   TransfromArea(wp, 1, wp->cursor.x, wp->cursor.y, ToUpper);
   return true;
}

//----------------------------

static bool Lower(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(IsCursorInBlock(wp))
      return TransformBlock(wp, ToLower);
   TransfromArea(wp, 1, wp->cursor.x, wp->cursor.y, ToLower);
   return true;
}

//----------------------------

static int flip_proc(int c){
   return isupper(c) ? tolower(c) : toupper(c);
}

//----------------------------

static bool Flip(C_window *wp){

   if(wp->ec->read_only)
      return false;

   if(IsCursorInBlock(wp))
      return TransformBlock(wp, flip_proc);
   TransfromArea(wp, 1, wp->cursor.x, wp->cursor.y, flip_proc);
   return true;
}

//----------------------------

static bool GotoPrevPos(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.prv_x, wp->cursor.prv_y);
}

//----------------------------

#define BRACE_NUM 6
static char all_braces[]="{([])}";

static bool IsBrace(C_window *wp){
   return !!strchr(all_braces, CurrChar(wp));
}

//----------------------------

static bool Match(C_window *wp){

   if(!IsBrace(wp))
      return false;
   char this_c=CurrChar(wp);
   char *cp;
   cp=strchr(all_braces, this_c);
   int this_i=(cp-all_braces);
   char match_c=all_braces[BRACE_NUM-1-this_i];
   int count=1;
   char c;
   if(this_i<BRACE_NUM/2){
                                       //down
      CursorRight(wp);
      while(!IsEndFile(wp)){
         if(IsEndLine(wp)){
            CursorDown(wp);
            BegLine(wp);
         }else{
            c=CurrChar(wp);
            if(c==match_c && !--count) return true;
            else if(c==this_c) ++count;
            CursorRight(wp);
         }
      }
   }else{                              //up
      while(!IsBegFile(wp)){
         if(!CursorLeft(wp)){
            CursorUp(wp);
            EndLine(wp);
         }else{
            c=CurrChar(wp);
            if(c==match_c && !--count) return true;
            else if(c==this_c) ++count;
         }
      }
   }
   GotoPrevPos(wp);
   return false;
}

//----------------------------
                                       //marking
static bool MarkLine(C_window *wp){
   return MarkAny(wp, LINE, wp->cursor.x, wp->cursor.y);
}

//----------------------------

static bool MarkColumn(C_window *wp){
   return MarkAny(wp, COLUMN, wp->cursor.x, wp->cursor.y);
}

//----------------------------

static bool MarkCUA(C_window *wp){
   return MarkAny(wp, CUA, wp->cursor.x, wp->cursor.y);
}

//----------------------------

static bool UnMarkBlock(C_window *wp){
   return UnMark(wp);
}

//----------------------------

static bool MarkWord(C_window *wp){
   if(!IsWord(wp)) return 0;
   UnMarkBlock(wp);
   int x=wp->cursor.x;
   int y=wp->cursor.y;
   WordBeg(wp);
   MarkColumn(wp);
   WordEnd(wp);
   MarkColumn(wp);
   wp->cursor.x=x;
   wp->cursor.y=y;
   return 1;
}

//----------------------------
                                       //block commands
static bool DeleteBlock(C_window *wp){

   if(wp->ec->read_only)
      return false;
   if(wp->block.IsEmpty())
      return false;

   GotoBlockBeg(wp);
   int y;
   int l = 0, r = 0;
   for(y=wp->block.ymax; y>=wp->block.ymin; y--){
      wp->block.GetLineInfo(y, &l, &r);
      if(r>=MAX_ROWS){
         if(!l) DeleteLine(wp, y);
         else KillArea(wp, Max(0, wp->doc.lines[y]->text_len-l), l, y);
      }
      else KillArea(wp, r-l+1, l, y);
   }
   if(r>=MAX_ROWS && l) DelCh(wp);
   UnMark(wp);
                                       //make document to have at least 1 line
   if(!wp->doc.linenum) InsertLine(wp);
                                       //rebuild comments
   int line=wp->block.ymin;
   if(line) wp->doc.CheckComments(&wp->ec->config, line-1, wp->doc.lines[line-1]->commented, 0, 1);
   else wp->doc.CheckComments(&wp->ec->config, line, 0, 0, 1);
                                       //redraw
   if(wp->block.ymin==wp->block.ymax) wp->doc.redraw_line=1;
   else wp->doc.redraw=1;
   return true;
}

//----------------------------

static bool Copy(C_window *wp){
   return clipboard.Copy(&wp->doc, &wp->block, wp->hwnd);
}

//----------------------------

static bool Cut(C_window *wp){

   if(wp->ec->read_only)
      return false;

   if(!Copy(wp))
      return false;
   return DeleteBlock(wp);
}
//-------------------------------------

static bool Paste(C_window *wp){

   if(clipboard.IsEmpty())
      return false;
   if(!OpenClipboard(wp->hwnd))
      return false;
   if(wp->ec->read_only)
      return false;

   int type = -1;
   HGLOBAL hg=GetClipboardData(clipboard.cf);
   if(!hg){
      hg=GetClipboardData(CF_TEXT);
      type=CUA;
   }
   char *cp=(char*)GlobalLock(hg);
   if(!hg || !cp){
      CloseClipboard();
      return false;
   }
   if(type==-1) type=*cp++;
   int line=wp->cursor.y;
   int row=wp->cursor.x;
   UnMarkBlock(wp);
   static char eol = 0;

   char *cp1;
   int linelen;
   int sl=line;

   wp->undo[0].Add(C_undo_element::UNMARKBLOCK);

   switch(type){
   case COLUMN:
      cp1=strchr(cp, '\r');
      if(cp1) linelen=cp1-cp;
      else linelen=strlen(cp);

      while(*cp && line<wp->doc.linenum){
         if(!AddArea(wp, cp, row, linelen, line)) break;
         ++line;
         cp+=linelen;
         if(*cp=='\r' && *++cp=='\n') ++cp;
      }
      wp->block.Mark(wp, COLUMN, row, sl);
      wp->block.Mark(wp, COLUMN, row+linelen-1, line-1);
      break;
   case LINE:
      while(*cp){
         cp1=strchr(cp, '\r');
         if(cp1) linelen=cp1-cp;
         else linelen=strlen(cp);

         if(!InsertLine(wp, cp, linelen, line+1)) break;
         ++line;
         cp+=linelen;
         if(*cp=='\r' && *++cp=='\n') ++cp;
      }
      wp->block.Mark(wp, LINE, 0, sl+1);
      wp->block.Mark(wp, LINE, 0, line);
      break;
   case CUA:
                                       //first line: insert after
      cp1=strchr(cp, '\r');
      if(cp1) linelen=cp1-cp;
      else linelen=strlen(cp);

      if(!AddArea(wp, cp, row, linelen, line)) break;
      cp+=linelen;

      if(*cp){
         ++line;

         if(*cp=='\r' && *++cp=='\n') ++cp;
                                       //split this line
         wp->cursor.x=row+linelen;
         SplitLine(wp);
         wp->cursor.x=row;
                                       //middle lines: insert lines
         do{
            cp1=strchr(cp, '\r');
            if(cp1) linelen=cp1-cp;
            else linelen=strlen(cp);

            cp1=cp+linelen;
            if(!*cp1) break;
            if(*cp1=='\r' && *++cp1=='\n') ++cp1;

            if(!InsertLine(wp, cp, linelen, line)){
               cp=&eol;
               break;
            }
            cp=cp1;
            ++line;
         }while(1);
         linelen=0;
                                       //last line: insert before
         if(*cp && line<wp->doc.linenum){
            cp1=strchr(cp, '\r');
            if(cp1) linelen=cp1-cp;
            else linelen=strlen(cp);

            AddArea(wp, cp, 0, linelen, line);
         }
         wp->block.Mark(wp, CUA, row, sl);
         wp->block.Mark(wp, CUA, linelen, line);
      }else{
         wp->block.Mark(wp, CUA, row, line);
         wp->block.Mark(wp, CUA, row+linelen, line);
      }
      break;
   }
   GlobalUnlock(hg);
   CloseClipboard();
   wp->doc.redraw = true;
   return true;
}

#define FINDS_WIDTH 52

static char find_buf[FINDS_WIDTH+1];
static char replace_buf[FINDS_WIDTH+1];
static struct{
   bool whole, global, back, resp_case, noprompt;
} find_opts;

static enum{
   E_FIND_FIND, E_FIND_REPLACE
} repeat_find_mode = E_FIND_FIND;
static bool find_dir_back = 0;

static bool FindText(C_window *wp, char *text, int *row, int *line, bool whole, bool case_sens, bool back){

   int find_len=strlen(text);
   int x = *row, y = *line;
                                       //check all lines
   while(1){
      if(!back){                       //direction?
         if(y>=wp->doc.linenum) break;
      }else if(y<0) break;
      char *cp=wp->doc.lines[y]->text;
      int len = 0;
      if(!back){
         len=wp->doc.lines[y]->text_len+1-x-find_len;
         if(len>0) do{                    //until it's worth of
            if((!case_sens && !strnicmp(text, cp+x, find_len)) ||
               (case_sens && !strncmp(text, cp+x, find_len))){
               if(!whole ||
                  ((!x || !IsWordChar(cp[x-1])) && (len==1 || !IsWordChar(cp[x+find_len])))){
                  *row=x, *line=y;
                  return 1;
               }
            }
         }while(++x, --len);
         x=0;
         ++y;
      }else{                           //backward
         x=min(x, wp->doc.lines[y]->text_len+find_len);
         if(x>=0) do{
            if((!case_sens && !strnicmp(text, cp+x, find_len)) ||
               (case_sens && !strncmp(text, cp+x, find_len))){
               if(!whole ||
                  ((!x || !IsWordChar(cp[x-1])) && (len==1 || !IsWordChar(cp[x+find_len])))){
                  *row=x, *line=y;
                  return 1;
               }
            }
         }while(x--);
         x=MAX_ROWS;
         --y;
      }
   }
   *row=x, *line=y;
   return 0;
}

//----------------------------

static void DlgInitFind(HWND hwnd){
   SendDlgItemMessage(hwnd, IDC_EDIT1, WM_SETTEXT, 0, (LPARAM)find_buf);
   SendDlgItemMessage(hwnd, IDC_EDIT2, WM_SETTEXT, 0, (LPARAM)replace_buf);
   SendDlgItemMessage(hwnd, IDC_CHECK_WHOLE, BM_SETCHECK, find_opts.whole, 0);
   SendDlgItemMessage(hwnd, IDC_CHECK_BACK, BM_SETCHECK, find_opts.back, 0);
   SendDlgItemMessage(hwnd, IDC_CHECK_RESPECTCASE, BM_SETCHECK, find_opts.resp_case, 0);
   SendDlgItemMessage(hwnd, IDC_CHECK_NOPROMPT, BM_SETCHECK, find_opts.noprompt, 0);
}

//----------------------------

static int DlgCloseFind(HWND hwnd){
   SendDlgItemMessage(hwnd, IDC_EDIT1, WM_GETTEXT, sizeof(find_buf)-1, (LPARAM)find_buf);
   SendDlgItemMessage(hwnd, IDC_EDIT2, WM_GETTEXT, sizeof(replace_buf)-1, (LPARAM)replace_buf);
   find_opts.whole=SendDlgItemMessage(hwnd, IDC_CHECK_WHOLE, BM_GETCHECK, 0, 0);
   find_opts.global=SendDlgItemMessage(hwnd, IDC_CHECK_GLOBAL, BM_GETCHECK, 0, 0);
   find_opts.back=SendDlgItemMessage(hwnd, IDC_CHECK_BACK, BM_GETCHECK, 0, 0);
   find_opts.resp_case=SendDlgItemMessage(hwnd, IDC_CHECK_RESPECTCASE, BM_GETCHECK, 0, 0);
   find_opts.noprompt=SendDlgItemMessage(hwnd, IDC_CHECK_NOPROMPT, BM_GETCHECK, 0, 0);
   return 1;
}

//----------------------------

bool Find(C_window *wp, bool prompt){

   int i;
   if(prompt){
      S_sys_dlg_param sp = { DlgInitFind, DlgCloseFind, wp->hwnd, wp->ec};
      i = DialogBoxParam(wp->ec->hi, "IDD_DIALOG_FIND",
         wp->hwnd,
         (DLGPROC)DlgSysCMD, (LPARAM)&sp);
      if(i==-1) return false;
      if(!*find_buf) return false;
   }
   repeat_find_mode = E_FIND_FIND;
   int x=wp->cursor.x, y=wp->cursor.y;
   if(!find_opts.back) ++x; else --x;
   if(find_opts.global){
      x=y=0;
      find_opts.global=0;
   }

   if(!FindText(wp, find_buf, &x, &y, find_opts.whole,
      find_opts.resp_case, find_opts.back)){
      wp->ec->SetState(STATE_MESSAGE, C_fstr("'%.32s' not found", find_buf));
      return false;
   }

   int find_len=strlen(find_buf);
   UnMarkBlock(wp);
   wp->block.Adjust(wp, 1, x, y, x+find_len, y);

   SetCursorPos1(wp, x, y);

   int fsx, fsy;
   wp->ec->GetFontSize(&fsx, &fsy);
   RECT rc;
   GetClientRect(wp->hwnd, &rc);
   int sx=rc.right/fsx;
   int sy=rc.bottom/fsy;
   wp->SetScrollPos(max(wp->scroll_x, x+find_len-sx+1), max(0, wp->cursor.y-sy/2));
   return true;
}

//----------------------------

static bool Find(C_window *wp){
   return Find(wp, 1);
}

//----------------------------

bool FindReplace(C_window *wp, bool prompt){

   int i;
   if(prompt){
      S_sys_dlg_param sp = { DlgInitFind, DlgCloseFind, wp->hwnd, wp->ec};
      i = DialogBoxParam(wp->ec->hi, "IDD_DIALOG_FINDREPLACE",
         wp->hwnd,
         (DLGPROC)DlgSysCMD, (LPARAM)&sp);
      if(i==-1) return false;
      if(!*find_buf) return false;
   }
   repeat_find_mode = E_FIND_REPLACE;
   int x=wp->cursor.x, y=wp->cursor.y;
   if(find_opts.global){
      x=y=0;
      find_opts.global=0;
   }
   bool yes_to_all=find_opts.noprompt;
   bool replace_this;
   int count=0;
   bool found=0;

   int find_len=strlen(find_buf);
   int repl_len=strlen(replace_buf);
   int fsx, fsy;
   wp->ec->GetFontSize(&fsx, &fsy);
   RECT rc;
   GetClientRect(wp->hwnd, &rc);
   int sx=rc.right/fsx;
   int sy=rc.bottom/fsy;
   do{
      if(!FindText(wp, find_buf, &x, &y, find_opts.whole,
         find_opts.resp_case, find_opts.back)) break;
      found=1;
      replace_this=1;

      UnMarkBlock(wp);
      MarkAny(wp, COLUMN, x, y);
      MarkAny(wp, COLUMN, x+find_len-1, y);

      if(!yes_to_all){
         SetCursorPos1(wp, x, y);
         wp->SetScrollPos(Max(wp->scroll_x, x+find_len-sx+1), Max(0, wp->cursor.y-sy/2));
         SetCursorVisible(wp, wp->hwnd);
         SendMessage(wp->hwnd, WM_USER_PAINTWHOLE, 0, 0);
         wp->cursor.SavePos();
                                             //break undo level
         wp->undo[0].Add(C_undo_element::MARK);

         S_sys_dlg_param sp = { DlgInitFind, DlgCloseFind, wp->hwnd, wp->ec};
         i = DialogBoxParam(wp->ec->hi, "IDD_DIALOG_RPROMPT",
            wp->hwnd, (DLGPROC)DlgSysCMD, (LPARAM)&sp);
         switch(i){
         case IDNO:
            replace_this=0;
            break;
         case ID_ALL:
            yes_to_all=1;
         case IDOK:
            break;
         default:
            wp->cursor.redraw = true;
            goto out;
         }
      }
      if(replace_this){
         ++count;
                                       //replace this
         DeleteBlock(wp);
         if(repl_len) AddArea(wp, replace_buf, x, repl_len, y);
      }
                                       //next
      if(!find_opts.back) x+=repl_len;
      else --x;
   }while(true);
out:
   wp->ec->SetState(STATE_MESSAGE,
      found ? C_fstr("%.32s: %i change(s)", find_buf, count) : 
      C_fstr("'%.28s' not found", find_buf));
   return found;
}

//----------------------------

static bool FindReplace(C_window *wp){
   return FindReplace(wp, 1);
}

//----------------------------

static bool Repeat(C_window *wp){
   switch(repeat_find_mode){
   case E_FIND_FIND:
      return Find(wp, !(*find_buf));
   case E_FIND_REPLACE:
      return FindReplace(wp, !(*find_buf));
   }
   return true;
}

//----------------------------

static bool IncSearch(C_window *wp){
   todo("IncSearch");
   return false;
}

//-------------------------------------
                                       //document operating functions
bool PutChar(C_window *wp, char c, int undo_buffer){

   if(wp->ec->read_only)
      return false;

   int line=wp->cursor.y;
   C_document_line *cl=wp->CurrLine();
   cl=cl->InsertChar(c, wp->cursor.x);
   if(!cl) return 0;
   wp->doc.ReplaceLine(line, cl);
   wp->doc.redraw_line = true;
                                       //add to undo buffer
   wp->undo[undo_buffer].Add(C_undo_element::PUTCHAR);
   SetModify(wp, undo_buffer);
   return true;
}

//----------------------------

bool DelChar(C_window *wp, bool split, int undo_buffer){

   if(wp->ec->read_only)
      return false;

   C_document_line *cl=wp->CurrLine();
   bool st;
   if(IsAfterEndLine(wp)){
      if(split){
                                       //join lines
         if(!IsBegLine(wp) && IsAfterEndLine(wp) && !IsEndLine(wp)){
            CursorLeft(wp);
            PutChar(wp, ' ');
            CursorRight(wp);
         }
         st=JoinLine(wp);
      }else st=1;
   }else{
      char c=cl->DeleteChar(wp->cursor.x);
      wp->undo[undo_buffer].Add(C_undo_element::DELCHAR, c);
      wp->doc.redraw_line=1;
      st=1;
   }
   if(st)
      SetModify(wp, undo_buffer);
   return st;
}

//----------------------------

bool DeleteLine(C_window *wp, int line, int undo_buffer){

   if(wp->ec->read_only)
      return false;

   wp->undo[undo_buffer].Add(C_undo_element::LINE, (dword)wp->doc.lines[line], line);
   wp->doc.DelLine(line);
   wp->doc.redraw=1;
   if(wp->cursor.y==wp->doc.linenum) CursorUp(wp);
   SetModify(wp, undo_buffer);
   return true;
}

//----------------------------

bool InsertLine(C_window *wp, char *text, int len, int line, int undo_buffer){

   if(wp->ec->read_only)
      return false;

   if(!wp->doc.AddLine(text, len, line)) return 0;
   wp->doc.redraw=1;
   wp->undo[undo_buffer].Add(C_undo_element::DEL_LINE, line);
   SetModify(wp, undo_buffer);
   return true;
}

//----------------------------

bool KillArea(C_window *wp, int len, int pos, int line, int undo_buffer){

   int llen=wp->doc.lines[line]->text_len;
   wp->undo[undo_buffer].Add(C_undo_element::LINEPART,
      (dword)&wp->doc.lines[line]->text[pos],
      (pos<<16)|(max(0, min(llen, pos+len)-min(llen, pos))),
      line);
   wp->doc.lines[line]->Kill(pos, len);
   ((line==wp->cursor.y) ? wp->doc.redraw_line : wp->doc.redraw) =1;
   SetModify(wp, undo_buffer);
   return true;
}

//----------------------------

bool TransfromArea(C_window *wp, int len, int pos, int line, t_trans_proc *proc, int undo_buffer){
   int llen=wp->doc.lines[line]->text_len;
   llen=max(0, min(llen, pos+len)-min(llen, pos));
   char *cp=&wp->doc.lines[line]->text[pos];
   wp->undo[undo_buffer].Add(C_undo_element::LINEPART, (dword)cp,
      (pos<<16)|llen, line);
   wp->undo[undo_buffer].Add(C_undo_element::KILLPART, pos, len, line);
   while(llen--){
      *cp = (char)proc(*cp);
      ++cp;
   }
   ((line==wp->cursor.y) ? wp->doc.redraw_line : wp->doc.redraw) =1;
   SetModify(wp, undo_buffer);
   return true;
}

//----------------------------

bool AddArea(C_window *wp, char *text, int pos, int len, int line, int undo_buffer){

   //int llen = wp->doc.lines[line]->text_len;
   C_document_line *cl=wp->doc.lines[line];
   cl=cl->InsertText(text, len, pos);
   if(!cl) return 0;
   wp->doc.ReplaceLine(line, cl);
   (line==wp->cursor.y ? wp->doc.redraw_line : wp->doc.redraw) =1;
   wp->undo[undo_buffer].Add(C_undo_element::KILLPART, pos, len, line);
   SetModify(wp, undo_buffer);
   return true;
}

//----------------------------
                                       //block operating
bool UnMark(C_window *wp, int undo_buffer){
   if(wp->block.IsEmpty()) return 0;
   wp->undo[undo_buffer].Add(C_undo_element::MARKBLOCK,
      (wp->block.type<<16)|wp->block.mode,
      (wp->block.x1<<16)|wp->block.y1,
      (wp->block.x2<<16)|wp->block.y2);
   wp->block.UnMark();
   wp->doc.redraw=1;
   return true;
}

//----------------------------

bool MarkAny(C_window *wp, E_block_type t, int x, int y){
   wp->undo[0].Add(C_undo_element::MARKBLOCK,
      (wp->block.type<<16)|wp->block.mode,
      (wp->block.x1<<16)|wp->block.y1,
      (wp->block.x2<<16)|wp->block.y2);
   int i=wp->block.Mark(wp, t, x, y);
   if(i&2) wp->doc.redraw=1;
   else wp->doc.redraw_line=1;
   return (bool)i;
}

//----------------------------

bool NewFile(C_window *wp){
   todo("NewFile");
   return false;
}

//----------------------------

bool OpenProject(C_window *wp, char *fname);

//----------------------------

static bool CloseFile(C_window *wp){

   if(wp->doc.modified){
                              //prompt for saving changes
      switch(MessageBox(wp->ec->hwnd, 
         C_fstr("Save changes to %s?", (const char*)wp->doc.title), "HighWare editor", MB_YESNOCANCEL)){
      case IDYES:
         if(!wp->Save()) return false;
      case IDNO:
         break;
      default:
         return false;
      }
   }
   wp->doc.Close();
   return true;
}

//----------------------------

bool OpenFile(C_window *wp){
                              //prompt for name
   char buf[256];
   buf[0] = 0;
   OPENFILENAME of;
   memset(&of, 0, sizeof(of));
   of.lStructSize=sizeof(of);
   of.hwndOwner = wp->ec->hwnd;
   of.hInstance = wp->ec->hi;
   of.lpstrFilter="Text files (*.txt)\0*.txt\0" "All files\0*.*\0";
   of.lpstrFile = buf;
   of.nMaxFile = sizeof(buf)-1;
   of.lpstrInitialDir = NULL;
   of.Flags = OFN_CREATEPROMPT;
   of.lpstrTitle = "HighWare Editor - Open";
   if(!GetOpenFileName(&of)) return false;
   if(!CloseFile(wp)) return false;

   C_cache ck;
   ck.open(buf, CACHE_READ);
   wp->doc.Open(&ck, buf, &wp->ec->config);
   wp->scroll_x = 0;
   wp->scroll_y = 0;
   wp->redraw = 0;
   wp->cursor.x = 0;
   wp->cursor.y = 0;

   SetWindowText(wp->ec->hwnd, wp->doc.title);

   return false;
}

//----------------------------

static bool SaveFile(C_window *wp){

   if(!wp->doc.modified)
      return true;
   if(wp->ec->read_only)
      return false;
   return wp->Save();
}

//----------------------------

static bool SaveAs(C_window *wp){

   bool b = wp->doc.unnamed;
   wp->doc.unnamed = true;
   bool st = wp->Save();
   if(!b) wp->doc.unnamed = false;
   return st;
}

//----------------------------

static bool SaveAll(C_window *wp){

   return SaveFile(wp);
}

//----------------------------

/*
static bool ReloadFile(C_window *wp){

   if(wp->ec->read_only){
      //Beep();
      return false;
   }

   if(wp->doc.unnamed) return false;
   if(!wp->doc.modified) return true;
                              //warning message
   if(MessageBox(wp->hwnd, 
      C_fstr("Are you sure to reload file '%s'?", (const char*)wp->doc.filename),
      "HighWare Text Editor", MB_YESNO | MB_ICONQUESTION)!=IDYES) return false;

   wp->doc.Close();
   wp->doc.Open(wp->doc.filename, &wp->ec->config);
   wp->scroll_y = Min(wp->scroll_y, wp->doc.linenum-1);
   wp->redraw = false;
   wp->cursor.y = Min(wp->cursor.y, wp->doc.linenum-1);
   return true;
}
*/

//----------------------------

/*
static bool ReadBlock(C_window *wp){

   if(wp->ec->read_only){
      //Beep();
      return false;
   }
                                       //prompt for name
   char buf[257];
   *buf = 0;
   OPENFILENAME of;
   memset(&of, 0, sizeof(of));
   of.lStructSize = sizeof(of);
   of.hwndOwner = wp->ec->hwnd;
   of.hInstance = (HINSTANCE)wp->ec->hi;
   of.lpstrFilter = "Text files\0*.txt\0All files\0*.*\0";
   of.lpstrFile = buf;
   of.nMaxFile = sizeof(buf)-1;
   of.lpstrFileTitle = buf;
   of.nMaxFileTitle = sizeof(buf)-1;
   of.lpstrInitialDir = NULL;
   of.lpstrTitle = "HighWare Text Editor - Read Block";
   of.Flags = OFN_FILEMUSTEXIST;
   if(!GetOpenFileName(&of)) return false;
                              //open
   ifstream is;
   is.open(buf, ios::in);
   if(is.fail()) return false;
   UnMarkBlock(wp);
   int cx = wp->cursor.x;
   int cy = wp->cursor.y;
   char *cp = new char[MAX_ROWS];

   int x = cx;
   int line = cy;
   int len;

   is.getline(cp, MAX_ROWS);
   if(!is.eof()){
                              //finish line
      len = is.gcount();
      AddArea(wp, cp, x, len, line);
      wp->cursor.x = Min(MAX_ROWS, x+len);
      SplitLine(wp);
                              //full lines
      while(true){
         ++line;
         is.getline(cp, MAX_ROWS);
         if(is.eof()) break;
         len = is.gcount();
         InsertLine(wp, cp, len, line);
      }
   }
   wp->undo[0].Add(C_undo_element::UNMARKBLOCK);
   wp->block.Adjust(wp, 1, cx, cy, 0, line);

   wp->cursor.x = cx;

   delete[] cp;

   is.close();
   return true;
}

//----------------------------

static bool WriteBlock(C_window *wp){

   if(!IsBlock(wp)) return false;
                                       //prompt for name
   char buf[257];
   *buf = 0;
   OPENFILENAME of;
   memset(&of, 0, sizeof(of));
   of.lStructSize = sizeof(of);
   of.hwndOwner = wp->hwnd;
   of.hInstance = wp->ec->hi;
   of.lpstrFilter = "Text files\0*.txt\0All files\0*.*\0";
   of.lpstrFile = buf;
   of.nMaxFile = sizeof(buf)-1;
   of.lpstrFileTitle = buf;
   of.nMaxFileTitle = sizeof(buf)-1;
   of.lpstrInitialDir = NULL;
   of.lpstrTitle = "HighWare Text Editor - Write Block";
   of.Flags = OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN;
   if(!GetSaveFileName(&of)) return false;
                              //open
   ofstream os;
   os.open(buf, ios::trunc | ios::out);
   if(os.fail()) return false;
                              //write
   for(int line=wp->block.ymin; line<=wp->block.ymax; line++){
      int l, r = 0;
      wp->block.GetLineInfo(line, &l, &r);
      int linelen=wp->doc.lines[line]->text_len;
      l = min(l, linelen);
      if(r>MAX_ROWS) r=MAX_ROWS;
      else ++r;
      r=min(r, linelen);
      if(r>l) os.write(&wp->doc.lines[line]->text[l], r-l);
      os<<endl;
   }
   os.close();
   return true;
}
*/

//----------------------------

static int DlgCloseShell(HWND hwnd){
   todo("DlgCloseShell");
   return false;
}

//----------------------------

static bool CallBack(C_window *wp){

   char buf[256];
   int pos = 0;
   short code;
   while((code=code_manager.Peek())!=BR && code<FIRST_COMMAND && pos<256){
      buf[pos++] = (char)code_manager.Get(NULL, 0, 0);
   }
   buf[pos++] = 0;
   if(!wp->ec->cb_proc) return false;
   return (*wp->ec->cb_proc)(buf, wp->ec->cb_context);
}

//----------------------------

static bool WaitProcessEnd(C_window *wp){
   todo("WaitProcessEnd");
   return false;
}

//----------------------------

static bool DestroyLastProcess(C_window *wp){
   todo("DestroyLastProcess");
   return false;
}

//----------------------------

static bool Call(C_window *wp){
   todo("Call");
   return false;
}

//----------------------------

static short curr_code[80];

static bool CurrFilename(C_window *wp){

   const char *cp = wp->doc.title;
   int i = strlen(cp);
   curr_code[i] = BR;
   while(i--) curr_code[i] = cp[i];
   return code_manager.Call(curr_code);
}

//----------------------------

static bool CurrPath(C_window *wp){

   const C_str &str = wp->doc.title;
   dword i = str.Size();
   while(i && str[i]!='\\') --i;
   //str[i] = 0;
   curr_code[i] = BR;
   while(i--) curr_code[i] = str[i];
   return code_manager.Call(curr_code);
}

//----------------------------

static bool CurrName(C_window *wp){

   const char *cp = wp->doc.title;
   int s = strlen(cp);
   int i = s;
   while(i && cp[i-1]!='\\') --i;
   s -= i;
   cp += i;
   curr_code[s] = BR;
   while(s--)
      curr_code[s] = (short)tolower(cp[s]);
   return code_manager.Call(curr_code);
}

//----------------------------

static bool CurrRow_(C_window *wp){
   char cp[80];
   sprintf(cp, "%i", wp->cursor.x+1);
   int i=strlen(cp);
   curr_code[i]=BR;
   while(i--) curr_code[i]=cp[i];
   return code_manager.Call(curr_code);
}

//----------------------------

static bool CurrLine_(C_window *wp){
   char cp[80];
   sprintf(cp, "%i", wp->cursor.y+1);
   int i=strlen(cp);
   curr_code[i]=BR;
   while(i--) curr_code[i]=cp[i];
   return code_manager.Call(curr_code);
}

//----------------------------

bool SavePosition(C_window *wp){
   wp->cursor.prv_x = wp->cursor.x;
   wp->cursor.prv_y = wp->cursor.y;
   return true;
}

//----------------------------

static char goto_buf[19];

static void DlgInitNumber(HWND hwnd){
   SendDlgItemMessage(hwnd, IDC_EDIT1, WM_SETTEXT, 0, (LPARAM)goto_buf);
}

//----------------------------

static int DlgCloseNumber(HWND hwnd){
   SendDlgItemMessage(hwnd, IDC_EDIT1, WM_GETTEXT, sizeof(goto_buf)-1, (LPARAM)goto_buf);
   int i;
   if(sscanf(goto_buf, "%i", &i)==1) return i;
   return -1;
}

//----------------------------

static bool GotoLine(C_window *wp){

   S_sys_dlg_param sp = { DlgInitNumber, DlgCloseNumber, wp->hwnd, wp->ec};
   int i = DialogBoxParam(wp->ec->hi, "IDD_DIALOG_GOTOLINE",
      wp->hwnd,
      (DLGPROC)DlgSysCMD, (LPARAM)&sp);
   if(i==-1) return false;
   int dy;
   i = Max(0, Min(i-1, (int)wp->doc.linenum-1));
   dy = wp->cursor.y - wp->scroll_y;
   SetCursorPos1(wp, wp->cursor.x, i);
   wp->SetScrollPos(wp->scroll_x, Max(0, Min(i-dy, (int)wp->doc.linenum-1)));
   return true;
}

//----------------------------

static bool GotoColumn(C_window *wp){

   char goto_buf[19];
   goto_buf[0] = 0;
   S_sys_dlg_param sp = { DlgInitNumber, DlgCloseNumber, wp->hwnd, wp->ec};
   int i=DialogBoxParam(wp->ec->hi, "IDD_DIALOG_GOTOCOLUMN", wp->hwnd,
      (DLGPROC)DlgSysCMD, (LPARAM)&sp);
   if(i==-1) return false;
   int dy;
   i = Max(0, Min(i-1, MAX_ROWS-1));
   dy=wp->cursor.x-wp->scroll_x;
   SetCursorPos1(wp, i, wp->cursor.y);
   wp->SetScrollPos(Max(0, Min(i-dy, MAX_ROWS-1)), wp->scroll_y);
   return true;
}

//----------------------------

static bool GotoFirstLine(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.x, 0);
}

//----------------------------

static bool GotoLastLine(C_window *wp){
   return SetCursorPos1(wp, wp->cursor.x, wp->doc.linenum-1);
}

//----------------------------
                                       //windows
static bool MaximizeWindow(C_window *wp){
   todo("MaximizeWindow");
   return false;
}

//----------------------------

static bool RestoreWindow(C_window *wp){
   todo("RestoreWindow");
   return false;
}

//----------------------------

static bool CascadeWindows(C_window *wp){
   todo("CascadeWindows");
   return false;
}

//----------------------------

static bool TileWindows(C_window *wp){
   todo("TileWindows");
   return false;
}

//----------------------------

static bool NextWindow(C_window *wp){
   todo("NextWindow");
   return false;
}

//----------------------------

static bool PrevWindow(C_window *wp){
   todo("PrevWindow");
   return false;
}

//----------------------------

bool Exit(C_window *wp){

   SendMessage(wp->ec->hwnd, WM_CLOSE, 0, 0);
   return true;
}

//----------------------------

static bool AddToProject(C_window*){ return false; }
static bool RemoveFromProject(C_window*){ return false; }
static bool OpenProject(C_window*){ return false; }
static bool SaveProject(C_window*){ return false; }
static bool SaveProjectAs(C_window*){ return false; }
static bool CloseProject(C_window*){ return false; }

static bool NoCommand(C_window*){ return false; }

//-------------------------------------
//-------------------------------------


T_edit_fnc *C_key_code::edit_functions[]={
   BegLine,
   EndLine,
   BegFile,
   EndFile,
   BegScreen,
   EndScreen,
   CursorDown,
   CursorLeft,
   CursorRight,
   CursorUp,
   BackSpace,
   DelCh,
   DelLine,
   WordLeft,
   WordRight,
   SplitLine,
   DelToEol,
   PageUp,
   PageDown,
   HalfPageUp,
   HalfPageDown,
   ScrollUp,
   ScrollDown,
   JoinLine,
   TabRt,
   TabLt,
   TabBlockRt,
   TabBlockLt,
   AddLine,
   InsertLine,
   DupLine,
   FirstNonWhite,
   ToggleInsert,
   MarkLine,
   MarkColumn,
   MarkCUA,
   MarkWord,
   UnMarkBlock,
   GotoBlockBeg,
   GotoBlockEnd,
   DeleteBlock,
   Cut,
   Copy,
   Paste,
   Undo,
   Redo,
   MainMenu,
   Enter,
   Beep1,
   Message,
   Match,
   MakeCtrLine,
                                       //windows
   MaximizeWindow,
   RestoreWindow,
   CascadeWindows,
   TileWindows,
   NextWindow,
   PrevWindow,
                                       //conditional
   IsBegLine,
   IsEndLine,
   IsAfterEndLine,
   IsFirstLine,
   IsLastLine,
   IsBegFile,
   IsEndFile,
   IsEmptyLine,
   IsWord,
   IsAreaPartOf,
                                       //file operations
   NewFile,
   OpenFile,
   SaveFile,
   SaveAs,
   SaveAll,
   NoCommand,//CloseFile,
   NoCommand,//ReloadFile,
   NoCommand,//ReadBlock,
   NoCommand,//WriteBlock,
   AddToProject,
   RemoveFromProject,
   OpenProject,
   SaveProject,
   SaveProjectAs,
   CloseProject,
   Exit,
   CallBack,
   WaitProcessEnd,
   DestroyLastProcess,
   ::Call,
   CurrFilename,
   CurrPath,
   CurrName,
   CurrRow_,
   CurrLine_,

   GotoLine,
   GotoColumn,
   GotoFirstLine,
   GotoLastLine,
   IsCursorInBlock,
   IsCUABlock,
   IsBlock,
   IsCurrChar,
   IsSpace,
   IsDigit,
   IsUpper,
   IsLower,
   IsBrace,
   IsFileReadOnly,
   Upper,
   Lower,
   Flip,
   SavePosition,
   GotoPrevPos,

   Find,
   FindReplace,
   Repeat,
   IncSearch,
};
