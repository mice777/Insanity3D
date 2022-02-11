#include "h_keys.h"
#include "document.h"
#include "constant.h"
#include "undo.h"


#define MAX_LINES 65536
#define MAX_ROWS 1000
#define LINE_CHUNK 1000
                              //chars to expand line if too small
#define GROW_LINE 100

#define IsWordChar(c) (isalnum(c) || c=='_')
//----------------------------

enum E_block_type{
   LINE, COLUMN, CUA
};

//----------------------------

extern const char MSG_MEM[];
extern const char MSG_LINES[];


typedef int t_trans_proc(int);

class C_window;

bool SetCursorPos1(C_window *wp, int x, int y, int undo_buff = 0);
bool AddArea(C_window *wp, char *text, int pos, int len, int line, int undo_buffer = 0);
bool DeleteLine(C_window *wp, int line, int undo_buffer = 0);
bool DelChar(C_window *wp, bool split = true, int undo_buffer = 0);
bool PutChar(C_window *wp, char c, int undo_buffer=0);
bool KillArea(C_window *wp, int len, int pos, int line, int undo_buffer = 0);
bool InsertLine(C_window *wp, char *text, int len, int line, int undo_buffer = 0);
bool TransfromArea(C_window *wp, int len, int pos, int line, t_trans_proc *proc, int undo_buffer = 0);
bool MarkAny(C_window *wp, E_block_type t, int x, int y);
bool UnMark(C_window *wp, int undo_buffer = 0);
void SetCursorVisible(C_window *wp, HWND);
void SetModify(C_window *wp, int undo_buff);

//----------------------------

enum E_state{
   STATE_OK, STATE_MESSAGE, STATE_WARNING, STATE_ERROR
};

//----------------------------

enum{
   KEY_SHIFT=1, KEY_CTRL=2, KEY_ALT=4,
   KEY_IGNORE_SHIFT=8, KEY_DISABLE_SHIFT=16
};

//----------------------------

#ifdef CLR_DEFAULT
#undef CLR_DEFAULT
#endif
enum{
   CLR_DEFAULT=0, CLR_NUMBER, CLR_STRING, CLR_SYMBOLS, CLR_COMMENT,
   CLR_RESERVED1, CLR_RESERVED2, CLR_RESERVED3, CLR_RESERVED4, CLR_BLOCK, CLR_FIND,
   CLR_LAST
};


enum COLOR{
   BLACK=0,
   BLUE,
   GREEN,
   CYAN,
   RED,
   PURPUR,
   BROWN,
   GREY,
   DARK_GREY,
   LIGHT_BLUE,
   LIGHT_GREEN,
   LIGHT_CYAN,
   LIGHT_RED,
   LIGHT_PURPUR,
   YELLOW,
   WHITE
};

//----------------------------

enum{
   WM_USER_MOVECURSOR = WM_USER,
   WM_USER_PAINTLINE,                  //wParam=line, lParam=(BOOL)highlight
};

#define WM_USER_PAINTWHOLE WM_PAINT

//----------------------------

#define DLL_NAME "i_ectrl.dll"
//#define DLL_NAME "editctrl.exe"

//----------------------------

class C_cursor{
public:
   int x, y;
   int prv_x, prv_y;
   bool redraw;

   C_cursor();
   void SavePos(){
      prv_x = x;
      prv_y = y;
   }
};

//----------------------------

#pragma pack(push,1)

struct S_config{
   byte tab_width;
   byte overwrite;
   byte curs_shape[2][2];
   char symbols[128];
   byte symbols_len;
   char open_comm[8];
   byte open_comm_len;
   char close_comm[8];
   byte close_comm_len;
   char eol_comm[8];
   byte eol_comm_len;
   char string[8];
   byte string_len;
   char literal;
   int curr_line_highlight;
   byte underline_cursor;

   byte reserved;                      //until here it's saved in registry
   byte colors[CLR_LAST];
   int reserved1[4];
};

//----------------------------

class C_block{

   void SetMinMax(){
      xmin = Min(x1, x2);
      ymin = Min(y1, y2);
      xmax = Max(x1, x2);
      ymax = Max(y1, y2);
   }
public:
   int x1, y1;
   int x2, y2;
   int xmin, ymin;
   int xmax, ymax;

   E_block_type type;
   enum E_mode{
      OFF, EXPAND, ON
   }mode;
   C_block(): 
      mode(OFF)
   {
   }
   bool IsCUA(){
      return mode!=OFF && type==CUA;
   }
   int Mark(C_window *wp, E_block_type t, int x, int y){
      int i=0;
      switch(mode){
      case ON:
         i=2;
      case OFF:
         type=t;
         mode=EXPAND;
         x1=x, y1=y;
         x2=x, y2=y;
         i|=1;
         break;
      case EXPAND:
         mode=ON;
         x2=x, y2=y;
         i=2;
         break;
      }
      SetMinMax();
      return i;
   }
   void SetMark(E_block_type t, E_mode m, int x11, int y11, int x21, int y21){
      type=t, mode=m, x1=x11, y1=y11, x2=x21, y2=y21;
      SetMinMax();
   }
   bool UnMark(){
      if(mode==OFF) return 0;
      mode=OFF;
      return 1;
   }
   int Adjust(C_window *wp, bool shift, int x, int y, int prev_x, int prev_y, int undo_buff=0);
   void GetLineInfo(int line, int *l, int *r){
      *l = INT_MAX;
      if(mode!=OFF && line>=ymin && line<=ymax){
         switch(type){
         case LINE:                    //whole line marked
            *l=0;
            *r=INT_MAX;
            break;
         case COLUMN:
            *l=xmin;
            *r=xmax;
            break;
         case CUA:
            if(ymin==ymax){            //single line
               if(xmin!=xmax){
                  *l=xmin;
                  *r=xmax-1;
               }
            }else
            if(line==ymin){            //first line
               *l= ymin==y1 ? x1 : x2;
               *r=INT_MAX;
            }else
            if(line==ymax){            //last line
               *l=0;
               *r= (ymax==y1 ? x1 : x2)-1;
            }else{                     //middle lines
               *l=0;
               *r=INT_MAX;
            }
            break;
         }
      }
   }
   bool IsEmpty(){
      return (mode==OFF || (ymin==ymax && xmin==xmax));
   }
   void InsertArea(class C_window *wp, int undo_buf, int line, int l, int sx);
   void InsertLine(class C_window *wp, int undo_buf, int line);
   void KillArea(class C_window *wp, int undo_buf, int line, int l, int sx);
   void KillLine(class C_window *wp, int undo_buf, int line);
};

//----------------------------

class C_clipboard{
   void *handle;
public:
   unsigned int cf;
   C_clipboard();
   void GetText(int line, char **cp, int *tlen, HWND);
   bool IsEmpty();
   void Empty();
   bool Copy(C_document *doc, C_block *bl, HWND);
   bool Destroy();
};
extern C_clipboard clipboard;

//----------------------------
typedef bool T_edit_fnc(C_window*);

#define MACRO_STACK_SIZE 16

class C_key_code{
   short *curr_ptr;
   bool code_shift_down;
   void *stack[MACRO_STACK_SIZE];
   byte stk_ptr;
public:
   C_key_code(): curr_ptr(NULL), stk_ptr(0)
   {
   }
   bool Push(void *vp){
      if(stk_ptr==MACRO_STACK_SIZE) return false;
      stack[stk_ptr++]=vp;
      return true;
   }
   bool Pop(void **vpp){
      if(!stk_ptr) return false;
      *vpp=stack[--stk_ptr];
      return true;
   }
   bool Call(short *mac){
      if(curr_ptr && !Push(curr_ptr)) return false;
      curr_ptr = mac;
      return true;
   }

   static short default_key_table[];
   static T_edit_fnc *edit_functions[];
   int Get(short *user_macs, dword vkey, dword key_data, bool *shift = NULL,
      bool also_esc = true);
   short Peek(){
      if(IsEmpty()) return BR;
      return *curr_ptr;
   }
   int GetNumber(){
      return *curr_ptr++;
   }
   void Jump(int rel){
      curr_ptr+=rel;
   }
   void Stop(){
      curr_ptr=NULL;
      stk_ptr=0;
   }
   bool IsEmpty(){
      return !curr_ptr || *curr_ptr==BR;
   }
   bool SkipCondCode(short code){
      if(code>=SYS_COMMANDS){
         if(code==JUMP || code==JTRUE || code==JFALSE){
            GetNumber();
            return true;
         }
      }else{
         if(code==ISCURRCHAR){
            GetNumber();
            return true;
         }else
         if(code==MESSAGE){
            while(true){
               short s=Peek();
               if(s==BR || s>=FIRST_COMMAND) return true;
               Get(NULL, 0, 0);
            }
         }
      }
      return false;
   }
};

extern C_key_code code_manager;

//----------------------------

class C_window{
public:
   class C_edit_control *ec;
   int scroll_x, scroll_y;
   bool redraw;
   bool overwrite;
   HWND hwnd;

   C_cursor cursor;
   C_document doc;
   C_block block;
                                       // buffer 0 - undo
                                       // buffer 1 - redo
   C_undo_buffer undo[2];

   C_window();

   C_document_line *CurrLine(){
      return doc.lines[cursor.y];
   }
   bool Save();
   void Activate();
   //C_str ExtractFilename() const;

   bool SetScrollPos(int x, int y, int undo_buff = 0);
};

//----------------------------

class C_edit_control: public C_edit_control_pure{
   dword ref;

   HWND hwnd_sb;              //status-bar
   E_state state;
   C_str state_message;

   static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   void ProcessInput(UINT uMsg, dword wParam, dword lParam);

   C_str cfg_file;

   void SizeWindow();
public:
                              //font:
   LOGFONT curr_font_log;     //current font params
   HFONT fnt;                 //current font
   int font_sx, font_sy;      //cached font size

                              //colors:
   byte rgb_values[16][3];
   dword custom_colors[16];
   bool curr_highl;
   int curr_HDC_color;

   bool config_changed;
   bool read_only;

                              //reserved (highlighted) words:
   std::vector<C_str> reserved_word[4];

   HWND hwnd;
   C_window win;
   short *user_macros;
   HINSTANCE hi;

                              //user call-back procedure
   t_callback *cb_proc;
   void *cb_context;

                              //user data
   dword user_data;

   static long CALLBACK dlgECProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   E_state CheckState();
   void SetState(E_state s, const char *msg = NULL);
   void MakeCaret();
   void DrawLineColumn();
   void InitScrollBar();

   void SetFont(HDC hdc, int *sx, int *sy);
   void GetFontSize(int *sx, int *sy);

   void SetHDCColors(HDC hdc, byte c, bool line_highl = false);
   void ResetHDCColors();
   dword GetRGB(byte c, bool highl);

   int CheckReservedWord(char *text, byte len);
   void Redraw();
   void ContextMenu(int x = 0, int y = 0);
public:
   S_config config;

   C_edit_control();
   ~C_edit_control();

public:
   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }


   virtual ECTRL_RESULT Init(const char *macro_file, const char *cfg_file);
   virtual ECTRL_RESULT AddHighlightWord(dword indx, const char *res_word){
      if(indx >= 4)
         return ECTRLERR_GENERIC;
      reserved_word[indx].push_back(res_word);
      return ECTRL_OK;
   }

   virtual void SetCallback(t_callback *cb1, void *cb_c1){
      cb_proc = cb1;
      cb_context = cb_c1;
   }

   virtual ECTRL_RESULT Open(const char *filename, void *hwnd_parent, dword flags, void **hwnd, const int pos_size[4]);
   virtual ECTRL_RESULT Open(class C_cache*, const char *title, void *hwnd_parent, dword flags,
      void **hwnd, const int pos_size[4]);
   virtual ECTRL_RESULT Save(bool prompt = true);
   virtual ECTRL_RESULT Close(bool save_cfg);

   virtual void SetUserData(dword dw){ user_data = dw; }
   virtual dword GetUserData() const{ return user_data; }

   virtual void SetCurrPos(int line, int row);
   virtual void GetCurrPos(int &line, int &row) const{
      line = win.cursor.y;
      row = win.cursor.x;
   }
   virtual int GetNumLines() const{
      return win.doc.linenum;
   }
   virtual bool GetTextLine(int line, char **buf, int *buf_len) const{
      if(line >= win.doc.linenum || line<0){
         *buf_len = 0;
         return false;
      }
      const C_document_line *lp = win.doc.lines[line];
      if(*buf_len < lp->text_len+1){
         *buf_len = lp->text_len+1;
         return false;
      }
      if(!*buf)
         return false;
      memcpy(*buf, lp->text, lp->text_len);
      (*buf)[lp->text_len] = 0;
      *buf_len = lp->text_len+1;
      return true;
   }

   virtual bool GetPosSize(int pos_size[4]) const{
      if(!hwnd)
         return false;
      WINDOWPLACEMENT wpl;
      wpl.length = sizeof(wpl);
      GetWindowPlacement(hwnd, &wpl);
      memcpy(pos_size, &wpl.rcNormalPosition, sizeof(RECT));

      pos_size[2] -= pos_size[0];
      pos_size[3] -= pos_size[1];
      return true;
   }
};

//----------------------------
