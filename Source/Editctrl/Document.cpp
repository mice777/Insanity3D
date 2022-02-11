#include "all.h"
#include "ectrl_i.h"


//----------------------------

static void DrawSingleLine(C_edit_control *ec, char *text, int textlen, int line,
   int startpos, byte color, int fsx, int fsy, HDC hdc){

   char cache[512];
   ec->SetHDCColors(hdc, ec->config.colors[CLR_DEFAULT]);
   memset(cache, ' ', textlen);
   TextOut(hdc, 0, line*fsy, cache, textlen);
}

//----------------------------

static bool DrawHighlightLine(C_edit_control *ec, C_document_line *dline, int linenum,
   int startpos, int block_l, int block_r, int sx, int fsx, int fsy,
   HDC hdc, bool line_highl = false){

   block_l -= startpos;
   block_r -= startpos;
   char *text=dline->text;
   int textlen=dline->text_len;

   int pos=0;
   //int text_pos=0;
   byte color;
   byte color_block = ec->config.colors[CLR_BLOCK];
   enum{
      NO, ALNUM, STRING, EOL_COMMENT, SYMBOL, COMMENT,
      OPEN_COMMENT, CLOSE_COMMENT, LITERAL
   }color_mode;
   if(dline->commented){
      color_mode=COMMENT;
      color = ec->config.colors[CLR_COMMENT];
   }else{
      color_mode=NO;
      color = ec->config.colors[CLR_DEFAULT];
   }
   int draw_len = min(textlen-startpos, sx)+startpos;
   int y = linenum*fsy;
   int l = 0;

   char cache[512];
   int cache_ptr=0;
   byte last_color=color;

   cache[cache_ptr++]=' ';
   l -= fsx;

   char string_char = 0;                   //char starting string

   if(draw_len>0){
      do{
         char c=*text;
         switch(color_mode){
         case CLOSE_COMMENT:
            color_mode=NO;
            break;
         case OPEN_COMMENT:
            color_mode=COMMENT;
            break;
         case COMMENT:
                                       //check for comment close
            if(textlen>=ec->config.close_comm_len && !memcmp(ec->config.close_comm, text, ec->config.close_comm_len))
               color_mode=CLOSE_COMMENT;
            break;
         case EOL_COMMENT:
            break;
         case STRING:
            if(c==string_char) color_mode=NO;
            else
            if(c==ec->config.literal) color_mode=LITERAL;
            break;
         case LITERAL:
            color_mode=STRING;
            break;
         case ALNUM:
            if(isalnum(c) || c=='_') break;
            color_mode=NO;
                                       //flow
         default:
                                       //check word
            if(IsWordChar(c)){
               color_mode=ALNUM;
               if(isdigit(c))
                  color = ec->config.colors[CLR_NUMBER];
               else{
                  int i=1;
                  while(i<textlen && IsWordChar(text[i])) ++i;
                  i = ec->CheckReservedWord(text, i);
                  color= (i==-1) ?
                     ec->config.colors[CLR_DEFAULT] : ec->config.colors[CLR_RESERVED1+i];
               }
            }else                      //check string
            if(memchr(ec->config.string, c, ec->config.string_len)){
               color_mode=STRING;
               string_char=c;
               color = ec->config.colors[CLR_STRING];
            }else                      //check eol comment
            if(textlen>=ec->config.eol_comm_len && !memcmp(ec->config.eol_comm, text, ec->config.eol_comm_len)){
               color_mode = EOL_COMMENT;
               color = ec->config.colors[CLR_COMMENT];
            }else                      //check comment start
            if(textlen>=ec->config.open_comm_len && !memcmp(ec->config.open_comm, text, ec->config.open_comm_len)){
               color_mode=OPEN_COMMENT;
               color = ec->config.colors[CLR_COMMENT];
            }else                      //check symbol
            if(memchr(ec->config.symbols, c, ec->config.symbols_len)){
               color = ec->config.colors[CLR_SYMBOLS];
            }else
               color = ec->config.colors[CLR_DEFAULT];
         }
         if(startpos){
            --startpos;
         }else{
            byte ccc=(pos>=block_l && pos<=block_r) ? color_block : color;
            if(last_color!=ccc){
               ec->SetHDCColors(hdc, last_color, line_highl);
               last_color=ccc;
               if(cache_ptr){
                  TextOut(hdc, l, y, cache, cache_ptr);
                  l += cache_ptr*fsx;
                  cache_ptr=0;
               }
            }
            cache[cache_ptr++]=c;
            ++pos;
         }
      }while(++text, --textlen, --draw_len);
   }
                                       //flush character cache
   if(cache_ptr){
      ec->SetHDCColors(hdc, last_color, line_highl);
      TextOut(hdc, l, y, cache, cache_ptr);
      l+=cache_ptr*fsx;
      cache_ptr=0;
   }
                                       //get rest of line (empty)
   int rest_line=sx-pos;

   if(rest_line){
      do{
         byte ccc = (pos>=block_l && pos<=block_r) ? color_block : ec->config.colors[CLR_DEFAULT];
         if(last_color!=ccc){
            ec->SetHDCColors(hdc, last_color, line_highl);
            last_color = ccc;
            if(cache_ptr){
               TextOut(hdc, l, y, cache, cache_ptr);
               l += cache_ptr*fsx;
               cache_ptr = 0;
            }
         }
         cache[cache_ptr++]=' ';
      }while(++pos, --rest_line);
      TextOut(hdc, l, y, cache, rest_line);
      if(cache_ptr){
         ec->SetHDCColors(hdc, last_color, line_highl);
         TextOut(hdc, l, y, cache, cache_ptr);
         l+=cache_ptr*fsx;
      }
   }else
   if(color_mode!=EOL_COMMENT && textlen){
      do{
         char c=*text;
         switch(color_mode){
         case CLOSE_COMMENT:
            color_mode=NO;
            break;
         case OPEN_COMMENT:
            color_mode=COMMENT;
            break;
         case COMMENT:
                                       //check for comment close
            if(textlen>=ec->config.close_comm_len && !memcmp(ec->config.close_comm, text, ec->config.close_comm_len))
               color_mode = CLOSE_COMMENT;
            break;
         case STRING:
            if(memchr(ec->config.string, c, ec->config.string_len)) color_mode=NO;
            else if(c==ec->config.literal) color_mode = LITERAL;
            break;
         case LITERAL:
            color_mode=STRING;
            break;
         default:
                                       //check string
            if(memchr(ec->config.string, c, ec->config.string_len))
               color_mode = STRING;
            else                       //check eol comment
            if(textlen>=ec->config.eol_comm_len && !memcmp(ec->config.eol_comm, text, ec->config.eol_comm_len))
               return 0;
            else                      //check comment start
            if(textlen>=ec->config.open_comm_len && !memcmp(ec->config.open_comm, text, ec->config.open_comm_len))
               color_mode = OPEN_COMMENT;
         }
      }while(++text, --textlen);
   }
   return color_mode==COMMENT;
}

//----------------------------
//----------------------------

C_document_line::C_document_line(char *text1, int t_len, int a_len):
   alloc_len(a_len), text_len(t_len), commented(false)
{
   memcpy(text, text1, text_len);
}

//-------------------------------------

C_document_line::C_document_line(C_document_line *line, int alloc_l):
   commented(false)
{
   alloc_len=alloc_l;
   text_len=line->text_len;
   memcpy(text, line->text, text_len);
}

//-------------------------------------

void C_document_line::RemoveTrailingSpaces(){
   while(text_len && text[text_len-1]==' ')
      --text_len;
}

//-------------------------------------

C_document_line *C_document_line::Minimize(){
   if(text_len<alloc_len){
      C_document_line *lp=(C_document_line*)realloc(this, sizeof(C_document_line)-1+text_len);
      lp->alloc_len=text_len;
      return lp;
   }
   return this;
}

//-------------------------------------

C_document_line *C_document_line::InsertChar(char c, int pos){
   int res_len=min(MAX_ROWS, max(text_len, pos)+1);
   if(res_len>alloc_len){
                                       //this line is too small for resulting
                                       //realloc it and make operation on new
      res_len=min(MAX_ROWS, res_len+GROW_LINE);
      C_document_line *lp=(C_document_line*)realloc(this,
         sizeof(C_document_line)-1+res_len);
      if(!lp) return NULL;
      lp->alloc_len=res_len;
      return lp->InsertChar(c, pos);
   }

   int i;
   if(pos==text_len){                  //at eol
      if(text_len==MAX_ROWS) return NULL;
      ++text_len;
   }else
   if(pos<text_len){                   //before eol
      i=Min(MAX_ROWS, text_len+1);
      text_len=i;
      while(--i>pos){
         text[i]=text[i-1];
      }
   }else{                              //after eol
      if(pos>=alloc_len) return 0;
      memset(&text[text_len], ' ', pos-text_len);
      text_len=pos+1;
   }
   text[pos]=c;
   return this;
}

//-------------------------------------

C_document_line *C_document_line::InsertText(char *text1, int len, int pos){
   int res_len=min(MAX_ROWS, max(text_len, pos)+len);

   if(res_len>alloc_len){
                                       //this line is too small for resulting
                                       //realloc it and make operation on new
      C_document_line *lp=(C_document_line*)realloc(this, sizeof(C_document_line)-1+res_len);
      if(!lp) return NULL;
      lp->alloc_len=res_len;
      return lp->InsertText(text1, len, pos);
   }
   len=min(len, alloc_len-pos);
   if(!len) return this;
                                       //fill trailing space
   if(pos>text_len){
      memset(&text[text_len], ' ', pos-text_len);
      text_len=pos;
   }
                                       //move up
   text_len=min(text_len+len, MAX_ROWS);
   int copy_len=text_len-(pos+len);
   if(copy_len) do{
      --copy_len;
      text[pos+len+copy_len]=text[pos+copy_len];
   }while(copy_len);
   memcpy(&text[pos], text1, len);
   return this;
}

//-------------------------------------

char C_document_line::DeleteChar(int pos){
   if(pos>=text_len) return 0;
   char c=text[pos];
   memcpy(&text[pos], &text[pos+1], text_len-pos-1);
   --text_len;
   return c;
}

//-------------------------------------

bool C_document_line::Kill(int pos, int num){
   if(pos>=text_len) return 0;
   int rpos=min(num+pos, text_len);
   memcpy(&text[pos], &text[rpos], text_len-rpos);
   text_len -= rpos-pos;
   return 1;
}

//-------------------------------------
//-------------------------------------

bool C_document::AllocLines(int num){
   if(alloc_lines==(dword)num) return 1;
   C_document_line **lines1=(C_document_line**)realloc(lines, sizeof(C_document_line**)*num);
   if(!lines1) return 0;
   lines=lines1;
   alloc_lines=num;
   return 1;
}

//----------------------------
//----------------------------

C_document::C_document(): 
   lines(NULL), 
   linenum(0), 
   alloc_lines(0), 
   redraw(false),
   redraw_line(false), 
   modified(false), 
   unnamed(true)
{
}

//----------------------------

C_document::~C_document(){
   Close();
}

//----------------------------

void C_document::Close(){
                                    //free lines
   while(linenum) delete lines[--linenum];
   free(lines);
   lines = NULL;
   linenum = 0;
   alloc_lines = 0;
   modified = false;
   unnamed = true;
}

//-------------------------------------

bool C_document::SetModify(bool b){

   bool ne = (b!=modified);
   modified = b;
   return ne;
}

//-------------------------------------

bool C_document::Open(C_cache *ck, const char *title1, S_config *cfg){

   char line[MAX_ROWS+1];
   char line1[MAX_ROWS+1];
   char *lp;
   unnamed = false;
   title = title1;
   dword ck_size = ck->filesize();

   if(!ck_size){
                              //empty document, add one line
      AddLine("", 0);
   }else
   while(!ck->eof()){
      for(int i=0; i<MAX_ROWS; i++){
         if(!ck_size-- || (ck->read(line+i, 1), line[i]=='\n')){
            if(i && line[i-1]=='\r')
               --i;
            break;
         }
      }
      line[i] = 0;
      int len = strlen(line);
      if(!len && linenum && ck->eof())
         break;
      lp = line;
                                       //conert tabs to spaces
      char *cp = (char*)memchr(line, '\t', len);
      if(cp){
         lp=line1;
         int dsp, srcp;
         memcpy(lp, line, dsp=srcp=cp-line);
         while(dsp<MAX_ROWS && srcp<len){
            if(line[srcp]=='\t'){
               int tablen=((dsp+cfg->tab_width)-(dsp+cfg->tab_width)%cfg->tab_width)-dsp;
               tablen = Min(MAX_ROWS-dsp, tablen);
               memset(line1+dsp, ' ', tablen);
               dsp += tablen;
            }else line1[dsp++]=line[srcp];
            ++srcp;
         }
         len=dsp;
      }
      if(!AddLine(lp, len)){
         return false;
      }
   }
   CheckComments(cfg, 0, 0, 1);
   redraw = true;
   return true;
}

//-------------------------------------

void C_document::CheckComments(S_config *cfg, int line1, bool was_comm, bool all, bool this_line){

   bool is_string = false;
   int line = line1;
   do{
      C_document_line *lp = lines[line];
      if(!all && was_comm==lp->commented && (!this_line || line!=line1)) return;
      lp->commented=was_comm;
      int i = -1;
      while(++i<lp->text_len){
         if(was_comm){
                                       //check for comment close
            if(i+cfg->close_comm_len<=lp->text_len &&
               !memcmp(&lp->text[i], cfg->close_comm, cfg->close_comm_len)){
               was_comm = false;
               i += cfg->close_comm_len - 1;
            }
         }else{
                                       //check for in-string
            if(memchr(cfg->string, lp->text[i], cfg->string_len)){
               is_string ^= 1;
            }
            if(is_string) continue;
                                       //check for eol comment (next line)
            if(i+cfg->eol_comm_len<=lp->text_len &&
               !memcmp(&lp->text[i], cfg->eol_comm, cfg->eol_comm_len)){
               i = lp->text_len;
               continue;
            }
                                       //check for comment open
            if(i+cfg->open_comm_len<=lp->text_len &&
               !memcmp(&lp->text[i], cfg->open_comm, cfg->open_comm_len)){
               was_comm = true;
               i += cfg->open_comm_len - 1;
            }
         }
      }
   }while(++line<linenum);
}

//-------------------------------------

void C_document::Paint(C_edit_control *ec, int scrl_x, int scrl_y, C_block *block, int cursor_y,
   void *hwnd1, int highl_line){

   HWND hwnd = (HWND)hwnd1;
   ec->ResetHDCColors();
   HDC hdc = GetDC(hwnd);
   RECT rc;
   GetClientRect(hwnd, &rc);
   int fsx, fsy;
   ec->SetFont(hdc, &fsx, &fsy);

   int sx = (rc.right+fsx-1)/fsx;
   int sy = (rc.bottom+fsy-1)/fsy;

   int block_l = 0, block_r = 0;

   bool was_comment = lines[scrl_y]->commented;
   for(int doc_line=scrl_y, screen_line=0; screen_line<sy; doc_line++, screen_line++){
      if(doc_line < linenum){
         block->GetLineInfo(doc_line, &block_l, &block_r);
         lines[doc_line]->commented = was_comment;
         was_comment = DrawHighlightLine(ec, lines[doc_line],
            screen_line, scrl_x, block_l, block_r, sx, fsx, fsy,
            hdc, doc_line==highl_line);
      }else{
         DrawSingleLine(ec, NULL, sx, screen_line, scrl_x,
            ec->config.colors[CLR_DEFAULT], fsx, fsy, hdc);
      }
   }
   if(doc_line<linenum && lines[doc_line]->commented!=was_comment){
      CheckComments(&ec->config, doc_line, was_comment);
                                       //draw till end of screen
      for(; screen_line<sy; doc_line++, screen_line++){
         if(doc_line < linenum){
            DrawHighlightLine(ec, lines[doc_line], screen_line, scrl_x,
               block_l, block_r, sx, fsx, fsy, hdc);
         }
      }
   }
   ReleaseDC(hwnd, hdc);
}

//-------------------------------------

void C_document::PaintLine(C_edit_control *ec, int scrl_x, int scrl_y, C_block *block, int cursor_y,
   void *hwnd1, bool line_highl){

   HWND hwnd = (HWND)hwnd1;
   bool was_comment;

   int screen_line;
   int block_l, block_r = 0;

   ec->ResetHDCColors();
   HDC hdc=GetDC(hwnd);
   RECT rc;
   GetClientRect(hwnd, &rc);
   int fsx, fsy;
   ec->SetFont(hdc, &fsx, &fsy);

   int sx=(rc.right+fsx-1)/fsx;
   int sy=(rc.bottom+fsy-1)/fsy;

   block->GetLineInfo(cursor_y, &block_l, &block_r);
   was_comment=lines[cursor_y]->commented;
   was_comment = DrawHighlightLine(ec, lines[cursor_y],
      screen_line=cursor_y-scrl_y, scrl_x, block_l, block_r, sx,
      fsx, fsy, hdc, line_highl);
   ++screen_line;
   ++cursor_y;
   redraw_line=0;

   if(cursor_y<linenum && lines[cursor_y]->commented!=was_comment){
      CheckComments(&ec->config, cursor_y, was_comment);
                                       //draw till end of screen
      for(; screen_line<sy; cursor_y++, screen_line++){
         if(cursor_y < linenum){
            block->GetLineInfo(cursor_y, &block_l, &block_r);
            DrawHighlightLine(ec, lines[cursor_y], screen_line, scrl_x,
               block_l, block_r, sx, fsx, fsy, hdc, 0);
         }
      }
   }
   ReleaseDC(hwnd, hdc);
}

//-------------------------------------

bool C_document::Write(){

   if(!modified) return true;
   ofstream os;
   os.open((const char*)title, ios::trunc | ios::out);
   if(os.fail()) return 0;
   for(int y=0; y<linenum && os.good(); y++){
      os.write(lines[y]->text, lines[y]->text_len);
      if(y<linenum-1 || lines[y]->text[0]!='\n') os.put('\n');
   }
   os.close();
   modified=0;
   return true;
}

//-------------------------------------

bool C_document::DelLine(int line){
   if(line<0 || line>=linenum) return 0;
   delete lines[line];
   memcpy(&lines[line], &lines[line+1], (linenum-line-1)*sizeof(C_document_line*));
   --linenum;
   return 1;
}

//-------------------------------------

bool C_document::AddLine(char *text, int len, int line){
   if(line==-1) line=linenum;
   if(line<0 || line>linenum) return 0;
   if(linenum==MAX_LINES){
      //SetState(STATE_ERROR, MSG_LINES);
      return 0;
   }

   if(linenum==alloc_lines){
      if(!AllocLines(alloc_lines+LINE_CHUNK)){
         //SetState(STATE_ERROR, MSG_MEM);
         return 0;
      }
   }

   int i=linenum;
   while(--i >= line) lines[i+1]=lines[i];

   lines[line]=new(len) C_document_line(text, len, len);
   ++linenum;
   return 1;
}

//-------------------------------------

