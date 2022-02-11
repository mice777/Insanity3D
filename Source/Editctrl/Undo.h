#include "document.h"

#pragma warning(disable:4291)

#define UNDO_BUFFER_ELEMENTS 512

//----------------------------
                                       //any undo element
class C_undo_element{
public:
   enum E_id{
      MARK, POSITION, SCROLL, PUTCHAR, DELCHAR, LINE, DEL_LINE,
      LINEPART, KILLPART, MARKBLOCK, UNMARKBLOCK, CUAPOS,
      FILEMODIFY, STATECHANGE, NOP
   } id;
   C_undo_element(E_id id1): id(id1)
   {
   }
   virtual void GetData(dword *d1=NULL, dword *d2=NULL, dword *d3=NULL){
   }
   virtual ~C_undo_element(){
   }
};

//----------------------------
                                       //undo elements with position
class C_undo_pos: public C_undo_element{
public:
   word x, y;
   C_undo_pos(C_undo_element::E_id id1, word x1, word y1): C_undo_element(id1)
   {
      x=x1;
      y=y1;
   }
   virtual void GetData(dword *d1, dword *d2, dword *d3){
      *d1=x;
      *d2=y;
   }
};

//----------------------------

class C_undo_data1: public C_undo_element{
public:
   dword d;
   C_undo_data1(C_undo_element::E_id id1, dword d1): C_undo_element(id1), d(d1)
   {
   }
   virtual void GetData(dword *d1, dword *d2, dword *d3){
      *d1=d;
   }
};

//----------------------------

class C_undo_data3: public C_undo_element{
public:
   dword d[3];
   C_undo_data3(C_undo_element::E_id id1, dword d1, dword d2, dword d3):
      C_undo_element(id1)
   {
      d[0]=d1;
      d[1]=d2;
      d[2]=d3;
   }
   virtual void GetData(dword *d1, dword *d2, dword *d3){
      *d1=d[0];
      *d2=d[1];
      *d3=d[2];
   }
};

//----------------------------

class C_undo_buffer{
public:
   C_undo_element *elements[UNDO_BUFFER_ELEMENTS];
   int bottom_pos;
   int pos;
   C_undo_buffer(): pos(0), bottom_pos(0)
   {
      memset(elements, 0, sizeof(elements));
   }
   ~C_undo_buffer(){
      for(int i=UNDO_BUFFER_ELEMENTS; i--; )
         delete[] elements[i];
   }
   int NextPos(int pos){
      return (++pos==UNDO_BUFFER_ELEMENTS) ? 0 : pos;
   }
   int PrevPos(int pos){
      return (--pos<0) ? UNDO_BUFFER_ELEMENTS-1 : pos;
   }
   void Clear(){
      for(int i=0; i<UNDO_BUFFER_ELEMENTS; i++){
         delete elements[i];
         elements[i] = 0;
      }
      pos = 0;
      bottom_pos = 0;
   }
   bool Add(C_undo_element::E_id id, dword d1=0, dword d2=0, dword d3=0);
   bool Undo(class C_window *wp, int redo_buf);
};

//----------------------------

class C_undo_line: public C_undo_element{
public:
   C_document_line *line;
   int linenum;
   C_undo_line(C_document_line *l1, dword y1): C_undo_element(LINE), linenum(y1)
   {
      line = new(l1->text_len) C_document_line(l1->text, l1->text_len, l1->text_len);
   }
   virtual void GetData(dword *d1, dword *d2, dword *d3){
      *d1=(dword)line;
      *d2=linenum;
   }
   virtual ~C_undo_line(){
      delete line;
   }
};

//----------------------------

class C_undo_linepart: public C_undo_pos{
public:
   C_document_line *line;
   C_undo_linepart(dword d1, dword d2, dword d3):
      C_undo_pos(LINEPART, word(d2>>16), (word)d3)
   {
      int llen=d2&0x7fff;
      line=new(llen) C_document_line((char*)d1, llen, llen);
   }
   virtual void GetData(dword *d1, dword *d2, dword *d3){
      *d1=(dword)line;
      *d2=x;
      *d3=y;
   }
   virtual ~C_undo_linepart(){
      delete line;
   }
};

//----------------------------
