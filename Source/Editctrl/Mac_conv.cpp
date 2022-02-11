#include "all.h"
#include "resource.h"
#include "ectrl_i.h"

//----------------------------

#define MAX_SCOPES 32
#define MAX_WORD_SIZE 128
#define MACRO_TABLE_SIZE 8192

//-------------------------------------

static byte CheckShifts(char **cp){

   byte sh = 0;
   while(true){
      switch(*(*cp)++){
      case '@':
         sh |= KEY_ALT;
         break;
      case '#':
         sh |= KEY_SHIFT;
         break;
      case '^':
         sh |= KEY_CTRL;
         break;
      case '!':
         sh |= KEY_IGNORE_SHIFT;
         break;
      case '&':
         sh |= KEY_DISABLE_SHIFT;
         break;
      default:
         --(*cp);
         return sh;
      }
   }
}

//-------------------------------------

static char CheckChar(char *cp){
   return (strlen(cp)==1) ? tolower(*cp) : 0;
}

//-------------------------------------

static BOOL CALLBACK DlgCompErr(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){

   case WM_INITDIALOG:
      SendDlgItemMessage(hwnd, IDC_STATIC_MAC_FILE, WM_SETTEXT, 0, lParam);
      ShowWindow(hwnd, SW_SHOW);
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

//-------------------------------------

static void Message(const char *fname, HWND *hwnd_dlg){

   if(!*hwnd_dlg){
      HINSTANCE hi = GetModuleHandle(DLL_NAME);
      *hwnd_dlg = CreateDialogParam(hi, "IDD_DIALOG_COMP_ERR", NULL, (DLGPROC)DlgCompErr, (LPARAM)fname);
   }
}

//-------------------------------------

static void Warning(int ln, char *cp, const char *fname, HWND *hwnd_dlg){

   Message(fname, hwnd_dlg);
   SendDlgItemMessage(*hwnd_dlg, IDC_LIST1, LB_ADDSTRING, 0, 
      (LPARAM)(const char*)C_fstr("Line %i, warning: %s", ln, cp));
}

//-------------------------------------

static void Error(int ln, char *cp, const char *fname, HWND *hwnd_dlg){

   Message(fname, hwnd_dlg);
   SendDlgItemMessage(*hwnd_dlg, IDC_LIST1, LB_ADDSTRING, 0, 
      (LPARAM)(const char*)C_fstr("Line %i, error: %s", ln, cp));
}

//-------------------------------------

static byte CheckKeyName(char *kn){

   static const char key_names[]={
      "ESC\0"
      "BACKSPACE\0"
      "TAB\0"
      "ENTER\0"
      "SPACE\0"
      "F1\0"
      "F2\0"
      "F3\0"
      "F4\0"
      "F5\0"
      "F6\0"
      "F7\0"
      "F8\0"
      "F9\0"
      "F10\0"
      "F11\0"
      "F12\0"
      "HOME\0"
      "CURSORUP\0"
      "PGUP\0"
      "CURSORLEFT\0"
      "CURSORRIGHT\0"
      "END\0"
      "CURSORDOWN\0"
      "PGDN\0"
      "INS\0"
      "DEL\0"
      "GREY/\0"
      "GREYENTER\0"
      "GREY+\0"
      "GREY-\0"
      "GREY*\0"
   };

   static const byte codes[] = {
      K_ESC, K_BACKSPACE, K_TAB, K_ENTER, K_SPACE,
      K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10,
      K_F11, K_F12,
      K_HOME, K_CURSORUP, K_PAGEUP, K_CURSORLEFT, K_CURSORRIGHT, K_END,
      K_CURSORDOWN, K_PAGEDOWN, K_INS, K_DEL, K_GREYSLASH, K_GREYENTER,
      K_GREYPLUS, K_GREYMINUS, K_GREYMULT
   };
   const char *cp = key_names;
   int i = 0;
   while(*cp){
      if(!stricmp(kn, cp)) return codes[i];
      cp += strlen(cp)+1;
      ++i;
   }
   return 0;
}

//-------------------------------------

bool GetWord(ifstream *isp, char *line, int *pos, char *Word, int *linenum, bool *newl){

   dword num = 0;
   while(*newl || sscanf(&line[*pos], "%128s%n", Word, &num)!=1){
      if(isp->eof())
         return false;
      *newl = 0;
      ++*linenum;
      isp->getline(line, 256);
      *pos = 0;
   }
   *pos += num;
   return true;
}

//-------------------------------------

bool FinishString(char *Word, char *line, int *pos){

   int i = strlen(Word);
   do{
      if(!line[*pos] || i>=MAX_WORD_SIZE) return false;
      Word[i] = line[(*pos)++];
   }while(Word[i++]!=*Word);
   Word[i] = 0;
   return true;
}

//-------------------------------------

static const char commands[] = {
   "BegLine\0"
   "EndLine\0"
   "BegFile\0"
   "EndFile\0"
   "BegScreen\0"
   "EndScreen\0"
   "CursorDown\0"
   "CursorLeft\0"
   "CursorRight\0"
   "CursorUp\0"
   "BackSpace\0"
   "DelCh\0"
   "DelLine\0"
   "WordLeft\0"
   "WordRight\0"
   "SplitLine\0"
   "DelToEol\0"
   "PageUp\0"
   "PageDown\0"
   "HalfPageUp\0"
   "HalfPageDown\0"
   "ScrollUp\0"
   "ScrollDown\0"
   "JoinLine\0"
   "TabRt\0"
   "TabLt\0"
   "TabBlockRt\0"
   "TabBlockLt\0"
   "AddLine\0"
   "InsertLine\0"
   "DupLine\0"
   "FirstNonWhite\0"
   "ToggleInsert\0"
   "MarkLine\0"
   "MarkColumn\0"
   "MarkCUA\0"
   "MarkWord\0"
   "UnMarkBlock\0"
   "GotoBlockBeg\0"
   "GotoBlockEnd\0"
   "DeleteBlock\0"
   "Cut\0"
   "Copy\0"
   "Paste\0"
   "Undo\0"
   "Redo\0"
   "MainMenu\0"
   "Enter\0"
   "Beep\0"
   "Message\0"
   "Match\0"
   "MakeCtrLine\0"

   "MaximizeWindow\0"
   "RestoreWindow\0"
   "CascadeWindows\0"
   "TileWindows\0"
   "NextWindow\0"
   "PrevWindow\0"

   "IsBegLine\0"
   "IsEndLine\0"
   "IsAfterEndLine\0"
   "IsFirstLine\0"
   "IsLastLine\0"
   "IsBegFile\0"
   "IsEndFile\0"
   "IsEmptyLine\0"
   "IsWord\0"
   "IsAreaPartOf\0"

   "NewFile\0"
   "OpenFile\0"
   "SaveFile\0"
   "SaveAs\0"
   "SaveAll\0"
   "CloseFile\0"
   "ReloadFile\0"
   "ReadBlock\0"
   "WriteBlock\0"
   "AddToProject\0"
   "RemoveFromProject\0"
   "OpenProject\0"
   "SaveProject\0"
   "SaveProjectAs\0"
   "CloseProject\0"
   "Exit\0"
   "CallBack\0"
   "WaitProcessEnd\0"
   "DestroyLastProcess\0"
   "Call\0"
   "CurrFilename\0"
   "CurrPath\0"
   "CurrName\0"
   "CurrRow\0"
   "CurrLine\0"

   "GotoLine\0"
   "GotoColumn\0"
   "GotoFirstLine\0"
   "GotoLastLine\0"
   "IsCursorInBlock\0"
   "IsCUABlock\0"
   "IsBlock\0"
   "IsCurrChar\0"
   "IsSpace\0"
   "IsDigit\0"
   "IsUpper\0"
   "IsLower\0"
   "IsBrace\0"
   "IsFileReadOnly\0"
   "Upper\0"
   "Lower\0"
   "Flip\0"
   "SavePosition\0"
   "GotoPrevPos\0"

   "Find\0"
   "FindReplace\0"
   "Repeat\0"
   "IncSearch\0"
                                       //system
   "Return\0"
   "And\0"
   "Or\0"
   "Xor\0"
   "Not\0"
   "True\0"
   "Escape\0"
};

enum{
   KEYWORD_DO = 0,
   KEYWORD_WHILE,
   KEYWORD_IF,
   KEYWORD_ELSE,
   KEYWORD_CONTINUE,
};

static const char keywords[] = {
   "do\0"
   "while\0"
   "if\0"
   "else\0"
   "continue\0"
};

//----------------------------

static int CheckCommand(char *cmd, const char *cmds){

   const char *cp=cmds;
   int i = 0;
   while(*cp){
      if(!stricmp(cmd, cp)) return i;
      cp += strlen(cp)+1;
      ++i;
   }
   return -1;
}

//-------------------------------------

bool ConvertMacros(const char *filename, short **cbuf){

   free(*cbuf);
   *cbuf = NULL;
   ifstream is;
   is.open(filename, ios::in);
   if(!is.good())
      return false;
   char line[257];
   char Word[MAX_WORD_SIZE+1];
   char err_msg[32+MAX_WORD_SIZE];
   int i;
   enum E_scope{
      FILE, IN_BLOCK, DO, AFTER_DO, WHILE, DOWHILE, DOCOND, IF, ELSE,
      SCOPE_LAST
   } scope = FILE, scope_type = FILE;
   struct{
      int cond_start;
      int cond_end;
      E_scope type;
   } scopes[MAX_SCOPES];
   int scope_depth = 0;
   scopes[scope_depth].type = FILE;
   memset(scopes, 0, sizeof(scopes));

   byte shifts;
   char *cp;
   int linenum = 0;
   byte a_code;
   *line = 0;
   int pos = 0;
                                       //alloc buffer
   short *code_buf = (short*)malloc(MACRO_TABLE_SIZE);
   int buf_ptr = 0;
   bool newl = false;
   bool ok = true;
   HWND hwnd_dlg = NULL;

   while(GetWord(&is, line, &pos, Word, &linenum, &newl)){
                                       //comment?
      if(*Word==';' || (Word[0]=='/' && Word[1]=='/')){
         newl=1;
         continue;
      }
      cp = Word;
      switch(scope){
      case IN_BLOCK:
                                       //open block
         if(!strcmp(cp, "{" /*}*/ )){
            if(scope_type==DOCOND)
               scope_type = IN_BLOCK;
            if(scope_depth==MAX_SCOPES){
               Error(linenum, "Too many scopes", filename, &hwnd_dlg);
               goto end;
            }
            switch(scope_type){
            case IF:
            case WHILE:                //reserve 2 for Jcond n
               code_buf[buf_ptr++] = JFALSE;
               ++buf_ptr;
               scopes[scope_depth].cond_end = buf_ptr;
               break;
            case ELSE:
               break;
            }
            scope_depth++;
            scope_type = IN_BLOCK;
            //scopes[scope_depth].type = scope_type;
            break;
         }
                                       //close block
         if(!strcmp(cp, /*{*/ "}")){
            if(scope_type==DOCOND)
               scope_type = IN_BLOCK;
            --scope_depth;
            switch(scopes[scope_depth].type){
            case WHILE:
                                    //jump to block beg
               code_buf[buf_ptr++]=JUMP;
               ++buf_ptr;
               code_buf[buf_ptr-1]=scopes[scope_depth].cond_start-buf_ptr;
               //break;
                              //flow...
            case IF:
            case ELSE:
               code_buf[scopes[scope_depth].cond_end-1] =
                  buf_ptr-scopes[scope_depth].cond_end;
               break;
            case DO:
               scope_type=AFTER_DO;
               break;
            case FILE:
            case IN_BLOCK:
               break;
            default:
               //{
               Error(linenum, "unexpected '}'", filename, &hwnd_dlg);
            }
            if(!scope_depth){
               code_buf[buf_ptr++] = BR;
               scope = FILE;
            }
            break;
         }
                                       //DO must have block
         if(scope_type==DO){
            Error(linenum, "Expecting '{'", filename, &hwnd_dlg); //}
            goto end;
         }
                                       //check literal
         if(i=strlen(Word), Word[0]=='\'' || Word[i-1]=='\"'){
            if(scope_type==DOCOND) scope_type=IN_BLOCK;
                                       //string
            if(i<=1 || Word[0]!=Word[i-1]){
               if(!FinishString(Word, line, &pos)){
                  Error(linenum, "Missing end for string", filename, &hwnd_dlg);
                  goto end;
               }
               i = strlen(Word);
            }
                                       //put string into table
            i -= 2;
            cp = Word + 1;
            while(i--){
               code_buf[buf_ptr++] = *cp++;
            }
            if(!scope_depth){
               code_buf[buf_ptr++] = BR;
               scope = FILE;
            }
            break;
         }
                                       //macro compiler keywords
         if(i=CheckCommand(Word, keywords), i!=-1){
            if(scope_type==DOCOND)
               scope_type = IN_BLOCK;
                                       //keyword
            switch(i){
            case KEYWORD_DO:
               scope_type=DO;
               scopes[scope_depth].cond_start = buf_ptr;
               break;
            case KEYWORD_WHILE:
               switch(scope_type){
               case IN_BLOCK:
                  scope_type=WHILE;
                  scopes[scope_depth].cond_start = buf_ptr;
                  break;
               case DO:
               case AFTER_DO:
                  scope_type=DOWHILE;
                  break;
               default:
                  sprintf(err_msg, "Not expecting '%s'", Word);
                  Error(linenum, err_msg, filename, &hwnd_dlg);
                  goto end;
               }
               break;
            case KEYWORD_IF:
               scope_type=IF;
               scopes[scope_depth].cond_start = buf_ptr;
               break;
            case KEYWORD_ELSE:
               if(scopes[scope_depth].type!=IF){
                  Error(linenum, "'Else' may appear only after 'If' block", filename, &hwnd_dlg);
                  goto end;
               }
                                       //jump if cond 2 further
               code_buf[scopes[scope_depth].cond_end-1] += 2;
                                       //put jump nn
               code_buf[buf_ptr++]=JUMP;
               code_buf[buf_ptr++]=0;

               scope_type=ELSE;
               scopes[scope_depth].cond_end = buf_ptr;
               break;
            }
            assert(scope_type < SCOPE_LAST);
            scopes[scope_depth].type = scope_type;
            break;
         }
                                       //editor commands
         if(i=CheckCommand(Word, commands), i!=-1){
                                       //store command
            code_buf[buf_ptr++] = i + 256;
            if(scope_type==DOWHILE){
               if((i+256)!=NOT){
                  scope_type=DOCOND;
                                       //this command makes condition for
                                       //previos scope
                  code_buf[buf_ptr++]=JTRUE;
                  ++buf_ptr;
                  code_buf[buf_ptr-1]=scopes[scope_depth].cond_start-buf_ptr;
               }
            }else
            if(scope_type==DOCOND){
               switch(i+256){
               case AND: case OR: case XOR:
                                       //oops, erase condition and store this
                  buf_ptr-=3;
                  code_buf[buf_ptr++]=i+256;
                  scope_type=DOWHILE;
                  break;
               default:
                  scope_type=IN_BLOCK;
               }
            }
            break;
         }
                                       //no keyword
         sprintf(err_msg, "Unknow command: '%s'", Word);
         Warning(linenum, err_msg, filename, &hwnd_dlg);
         ok=0;
         break;
                                       //it may be key
      case FILE:
         if(i=strlen(Word), Word[0]=='\'' || Word[i-1]=='\"'){
                                       //quoted char
            if(i<=1 || Word[0]!=Word[i-1]){
               if(!FinishString(Word, line, &pos)){
                  Error(linenum, "Missing end for string", filename, &hwnd_dlg);
                  goto end;
               }
               i=strlen(Word);
            }
            strcpy(Word, &Word[1]);
            Word[i-2]=0;
         }
                                       //determine key
         shifts = CheckShifts(&cp);
         a_code = CheckChar(cp);
         if(a_code){
            a_code = toupper(a_code);
         }else{
            a_code = CheckKeyName(cp);
            if(!a_code){
               sprintf(err_msg, "Unknow key: '%s'", Word);
               Warning(linenum, err_msg, filename, &hwnd_dlg);
               ok=0;
            }
         }
                                       //store ascii & code
         code_buf[buf_ptr++] = a_code;
         code_buf[buf_ptr++] = shifts;
         scope_type = IN_BLOCK;
         scope = IN_BLOCK;
         break;
      }
   }
//ok:
   code_buf[buf_ptr++] = BR;
                                       //table ok
   if(ok){
      *cbuf = (short*)realloc(code_buf, buf_ptr * sizeof(short));
      code_buf = NULL;
   }
end:
   free(code_buf);
   is.close();
   if(hwnd_dlg){
      EnableWindow(GetDlgItem(hwnd_dlg, IDOK), 1);
      SetFocus(GetDlgItem(hwnd_dlg, IDOK));
      MSG msg;
      while(hwnd_dlg && GetMessage(&msg, NULL, 0, 0)){
         DispatchMessage(&msg);
      }
   }
   return (*cbuf != NULL);
}

//----------------------------