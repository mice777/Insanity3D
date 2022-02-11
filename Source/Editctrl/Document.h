#include <rules.h>
#include <malloc.h>
#include <memory.h>
#pragma once

class C_block;

//----------------------------

class C_document_line{
public:
   word alloc_len;
   word text_len;
   bool commented;
   char text[1];

   C_document_line(char *text1, int t_len, int a_len);
   C_document_line(C_document_line *line, int alloc_l);
   void *operator new(unsigned int len, int strlen){
      return malloc(len-1+strlen);
   }
   void operator delete(void *mem){
      free(mem);
   }
   void RemoveTrailingSpaces();
   int operator ==(C_document_line &line){
      return (text_len==line.text_len && !memcmp(text, line.text, text_len));
   }
   C_document_line *InsertChar(char c, int pos);
   C_document_line *InsertText(char *text, int len, int pos);
   C_document_line *Minimize();
   char DeleteChar(int pos);
   bool Kill(int pos, int num);
};

//----------------------------

class C_document{
   bool AllocLines(int num);
   dword alloc_lines;
public:
   bool modified;
   bool unnamed;
   C_str title;
   C_document_line **lines;
   int linenum;
   bool redraw;
   bool redraw_line;

   C_document();
   ~C_document();
   void SetTitle(char *name){ title = name; }
   //bool Open(const char *file, struct S_config*);
   bool Open(class C_cache *ck, const char *title, struct S_config*);
   void Close();
   bool Write();
   void Draw(int x, int y, int sx, int sy, int scrl_x, int scrl_y,
      C_block *block, int cursor_y);
   void Paint(class C_edit_control*, int scrl_x, int scrl_y, C_block *block,
      int cursor_y, void *hwnd, int highl_line);
   void PaintLine(C_edit_control*, int scrl_x, int scrl_y, C_block *block, int cursor_y,
      void *hwnd, bool line_highl);
   void ReplaceLine(int num, C_document_line *lp){
      lines[num]=lp;
   }
   void CheckComments(struct S_config*, int linenum, bool is_comm, bool all = false, bool this_line = false);

   bool DelLine(int line);
   bool AddLine(char *text, int len, int line=-1);
   bool SetModify(bool b);
};

//----------------------------