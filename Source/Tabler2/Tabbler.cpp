/*
   Tabbler v2.0 Copyright (c) 2002 Lonely Cat Games

Notes:
   System uses 2 table classes:
      1. Table template (C_table_template)
         - defines template for table editing
         - keeps GUID for identification
         - there's one template for each table type
         - template elements are items of user table or branch/array definitions
         - branch element may have formatted text showing elements from inside of its tree
      2. Table interface (C_table)
         - keeps GUID for identification
         - keeps user table data
         - keeps table descriptor (used for item access)
         - opens table by:
            - creating from template
            - duplicating existing table
            - loading from file
         - saves table to file
         - keeps table lock
         - accesses user table elements
         - there's one interface for each table used by user

   Table editor:
      - operates on Modeless dialog box
      - uses owner-drawn listbox as main element (with vertical scroll)
      - each table element takes one line
      - there're only one of editing controlls of a kind (e.g. edit box, slide bar, etc)
         defined directly in dialog template - these are positioned and used as necessary
      - during editing, the edited table is updated dynamically
      - in order to edit C_table, GUIDs of table and template must match
*/
#define TABBLER_FULL
#include <windows.h>
#include <tabler2.h>
#include <commctrl.h>
#include <rules.h>
#include <c_unknwn.hpp>
#include <insanity\os.h>
#include "ITabCore.h"
#include "resource.h"


#define TABLE_BREAK1 80
#define TABLE_VALUE_SX1 60
#define TABLE_BREAK2 40
#define TABLE_VALUE_SX2 30

const int DEFAULT_SIZE_X = 100;
const int DEFAULT_SIZE_Y = 200;

                              //the filename of this dll (for resource location)
#define DLL_FILENAME "itabler2.dll"

#define TRUE_TEXT "True"
#define FALSE_TEXT "False"

// {05E6C3A0-404B-11d2-9B9D-004F4905FF1A}
static const GUID guid_templ_table =
   { 0x5e6c3a0, 0x404b, 0x11d2, { 0x9b, 0x9d, 0x0, 0x4f, 0x49, 0x5, 0xff, 0x1a } };


#define TABEDIT_EDITING_DEFAULTS 0x10000

//----------------------------

//static PC_table_template TableToTemplate(PC_table table);

//----------------------------

enum{
   TAB_G_TEMPL_GUID,
   TAB_S32_CAPTION,
   TAB_I_DEF_SIZE_X,
   TAB_I_DEF_SIZE_Y,

   TAB_E_ELEM_TYPE,
   TAB_I_ELEM_INDEX,
   TAB_S24_CAPTION,
   TAB_U_PARAM1,
   TAB_U_PARAM2,
};

//----------------------------

static bool StringToGUID(const char *cp, LPGUID guid){

   int i=sscanf(cp, "{%x-%x-%x-%2x%2x-%2x%2x%2x%2x%2x%2x}", &guid->Data1, &guid->Data2,
      &guid->Data3, &guid->Data4[0], &guid->Data4[1],
      &guid->Data4[2],
      &guid->Data4[3],
      &guid->Data4[4],
      &guid->Data4[5],
      &guid->Data4[6],
      &guid->Data4[7]);
   return (i==11);
}

//----------------------------

struct S_rgb{
   byte b, g, r;
};

//----------------------------
//----------------------------
                              //static functions

static void RemoveFloatZeros(char *buf){

   int i=strlen(buf);
   while(i && buf[i-1]=='0' ) --i;
   if(!i || buf[i-1]=='.') buf[i++]='0';
   buf[i]=0;
}

//----------------------------

static void SetWindowInt(HWND hwnd, int i, bool select=false){
   char buf[128];
   sprintf(buf, "%i", i);
   SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)buf);
   if(select) SendMessage(hwnd, EM_SETSEL, 0, strlen(buf));
}

//----------------------------

static void SetWindowFlt(HWND hwnd, const float &f, bool select=false, bool flt=false){

   char buf[128];
   sprintf(buf, "%f", f);
   RemoveFloatZeros(buf);
   SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)buf);
   if(select) SendMessage(hwnd, EM_SETSEL, 0, strlen(buf));
}

//----------------------------

static void MapDlgUnits(HWND hwnd_dlg, int *x, int *y){

   RECT rc;
   SetRect(&rc, x ? *x : 0, y ? *y : 0, x ? *x : 0, y ? *y : 0);
   MapDialogRect(hwnd_dlg, &rc);
   if(x) *x=rc.left;
   if(y) *y=rc.top;
}

//----------------------------
                              //line breaking descriptor and value (in pixels)
#define TABLE_DRAW_VALUE_X 3  //in pixels from TABLE_BREAK


                              //dialog box init struct
struct S_db_data: public C_unknown{
                              //creation data
   HINSTANCE hinst;
   const C_table_template *templ;
   PC_table table;
   HWND hwnd_main;            //HWND of dialog's parent window
   table_callback cb_proc;
   dword cb_user;
   dword create_flags;        //TABEDIT_?
   int *pos_size;

                              //managed by dialog box
   HWND hwnd_dlg, hwnd_lbox;
   HWND hwnd_check, hwnd_edit, hwnd_slider, hwnd_choose, hwnd_browse;
   WNDPROC orig_proc_lb, orig_proc_chk, orig_proc_edn, orig_proc_sld, orig_proc_cmb;
   bool active, buttons, manual;
   int table_value_sx, table_break, table_draw_value_x, table_indent;
   S_db_data():
      create_flags(0),
      hwnd_main(NULL),
      hwnd_check(NULL),
      hwnd_dlg(NULL),
      hwnd_lbox(NULL),
      hwnd_edit(NULL),
      hwnd_slider(NULL),
      hwnd_choose(NULL),
      hwnd_browse(NULL),
      orig_proc_lb(NULL),
      orig_proc_chk(NULL),
      orig_proc_edn(NULL),
      orig_proc_sld(NULL),
      orig_proc_cmb(NULL),
      hinst(NULL),
      templ(NULL),
      table(NULL),
      cb_proc(NULL),
      cb_user(0),
      pos_size(NULL),
      buttons(false),
      manual(false),
      active(false)
   {}
   ~S_db_data(){
      table->Release();
   }
   void operator =(const S_db_data &d){
      memcpy(&this->hinst, &d.hinst, offsetof(S_db_data, active)-offsetof(S_db_data, hinst)+sizeof(bool));
      if(table) table->AddRef();
   }
};

//----------------------------

static void InsertTable(S_db_data *dd, class S_element_data *root = NULL, int num_elems = 0,
   int indent = 0, int templ_indx1 = 0, int array_index = 0, int array_index_show = 0);
static void DestroyTable(S_db_data *dd, int num_elems, int elem_base);

//----------------------------

                              //element working data (set as list-box user value)
class S_element_data: public C_unknown{
   bool active;
   bool expanded;             //for branches/arrays/enums
   bool array_member;
   dword indent_level;
   int templ_indx, tab_indx;
   int array_indx;            //for non-array members this is zero
   int array_indx_show;       //array member index (for showing only - may be different of array_indx)
   S_element_data *branch_root;
   C_str enum_text;           //cached alternative enum text (provided via callback)

   ~S_element_data(){}
public:
   const C_table_element *te;

   S_element_data(S_db_data *dd, S_element_data *br_root, int templ_indx1, int indent1,
      int array_index1, int array_index2, bool array_member1):
      branch_root(br_root),
      disable_modify(false),
      active(false),
      expanded(false),
      templ_indx(templ_indx1),
      array_member(array_member1),
      indent_level(indent1),
      array_indx(array_index1),
      array_indx_show(array_index2)
   {
      te = &dd->templ->GetElements()[templ_indx];
      tab_indx = te->tab_index;
   }

   bool IsExpanded() const{ return expanded; }

   bool HasRange(){
      switch(te->type){
      case TE_FLOAT: return (te->flt_min < te->flt_max);
      case TE_INT: return (te->flt_min < te->flt_max);
      default:  return false;
      }
   }

//----------------------------

   const char *GetEnumText(S_db_data *dd, int indx, const C_table_element *te){

      if(indx==TAB_INV_ENUM)
         return "---";
      const char *cp = te->enum_list;
      if(!cp){
         if(!enum_text.Size()){
            if(dd->cb_proc){
               dd->cb_proc(dd->table, TCM_GET_ENUM_STRING, dd->cb_user, te->tab_index, (dword)&enum_text);
            }
         }
         if(enum_text.Size())
            cp = enum_text;
      }
      if(!cp) return "<enum not defined>";
      while(indx--){
         if(!*cp) return "<out of enum>";
         cp += strlen(cp)+1;
      }
      return cp;
   }

//----------------------------

   C_str FormatString(S_db_data *dd, const char *instr, const C_table_element *tee1, int indx_mult = 1){

      char outstr[512];

      int ii = 0, oi = 0;
      char c;
      while(c=instr[ii++], c){
         if(c=='%' && instr[ii]=='['){
            ii++;
                              //get index number
            int index, numc;
            int num = sscanf(&instr[ii], "%i]%n", &index, &numc);
            if(num==1){
               ii += numc;
                              //find element which should be filled here
               const C_table_element *tee = tee1;
               do{
                  switch((++tee)->type){
                              //format error
                  case TE_NULL: tee = NULL; index = 0; break;
                  case TE_BRANCH:  //skip sub-branch
                     if(index){
                        int num_skip = tee->branch_depth;
                        while(num_skip)
                        switch((++tee)->type){
                        case TE_BRANCH: num_skip += tee->branch_depth; break;
                        default: --num_skip; //++tb_index;
                        }
                     }
                     break;
                  }
               }while(index--);
               int tb_index = tee->tab_index;
               int array_ix = array_indx * indx_mult;
               if(tee){
                  switch(tee->type){
                  case TE_ENUM:
                     {
                        index = dd->table->ItemE(tb_index, array_ix);
                        if(index==TAB_INV_ENUM)
                           oi += sprintf(&outstr[oi], "---");
                        else
                           oi += sprintf(&outstr[oi], "%s", GetEnumText(dd, index, tee));
                     }
                     break;
                  case TE_STRING:
                     {
                        const char *cp = dd->table->ItemS(tb_index, array_ix);//, dd->table->StringSize(tb_index));
                        if(cp[0] == TAB_INV_STRING[0])
                           strcpy(&outstr[oi], "---");
                        else
                           strcpy(&outstr[oi], cp);
                        oi = strlen(outstr);
                     }
                     break;
                  case TE_FLOAT:
                     {
                        float f = dd->table->ItemF(tb_index, array_ix);
                        char buf[64];
                        if(f==TAB_INV_FLOAT){
                           strcpy(buf, "---");
                        }else{
                           sprintf(buf, "%f", f);
                        }
                        RemoveFloatZeros(buf);
                        strcpy(&outstr[oi], buf);
                        oi=strlen(outstr);
                     }
                     break;
                  case TE_INT:
                     {
                        int i = dd->table->ItemI(tb_index, array_ix);
                        if(i==TAB_INV_INT)
                           oi += sprintf(&outstr[oi], "---");
                        else
                           oi += sprintf(&outstr[oi], "%i", i);
                     }
                     break;
                  case TE_BOOL:
                     {
                        byte b = (byte&)dd->table->ItemB(tb_index, array_ix);
                        if(b==TAB_INV_BOOL){
                           oi += sprintf(&outstr[oi], "---");
                        }else{
                           oi += sprintf(&outstr[oi], "%s", b ? TRUE_TEXT : FALSE_TEXT);
                        }
                     }
                     break;
                  case TE_BRANCH:
                     if(tee->branch_text){
                        oi += sprintf(&outstr[oi], "%s", (const char*)FormatString(dd, tee->branch_text, tee, indx_mult));
                     }
                     break;
                  }
               }
            }
         }else
            outstr[oi++] = c;
      }
      outstr[oi++]=0;
      return outstr;
   }

//----------------------------
                              //set color for default values
   void SetDefaultColor(HDC hdc){
      SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
   }

//----------------------------

   int ArrayLen(){
      return te->array_size;
   }

//----------------------------

   void Draw(HWND hwnd_dlg, HDC hdc, LPRECT rc, S_db_data *dd, bool active1){

      bool sel_changed = false;
      if(active!=active1){
         active = active1;
         sel_changed = true;
      }

                              //setup colors
      dword color_bk = GetSysColor(COLOR_3DFACE);

                              //clear area
      {
         HBRUSH br = CreateSolidBrush(color_bk);
         RECT rc1;
         SetRect(&rc1, rc->left+1, rc->top, rc->right, rc->bottom-1);
         FillRect(hdc, &rc1, br);
         DeleteObject(br);
      }

                              //draw lines and caption
      {
         LOGBRUSH lb = {BS_SOLID, GetSysColor(COLOR_3DSHADOW), 0 };
         HPEN pen = ExtCreatePen(PS_SOLID, 1, &lb, 0, NULL);
         HPEN old_pen = (HPEN)SelectObject(hdc, pen);

         POINT pts[3];
         pts[0].x=rc->left + indent_level*dd->table_indent;
         pts[0].y=rc->top;
         pts[1].x=pts[0].x;
         pts[1].y=rc->bottom-1;
         pts[2].x=rc->right;
         pts[2].y=rc->bottom-1;
         Polyline(hdc, pts, 3);
         if(te->type==TE_BRANCH && !te->branch_text);
         else{
            pts[0].x=pts[1].x = dd->table_break;
            Polyline(hdc, pts, 2);
         }

         SelectObject(hdc, old_pen);
         DeleteObject(pen);
      }

                              //draw caption
      TEXTMETRIC tm;
      GetTextMetrics(hdc, &tm);
      int y = rc->top+1;

      SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
      const char *caption;
      bool full_width = false;
      switch(te->type){
      case TE_BRANCH:
         caption = te->branch_text;
         if(caption){
                              //draw formatted branch caption
            int x = dd->table_break + 4;
            C_str str = FormatString(dd, caption, te);
            RECT rc1;
            SetRect(&rc1, x, rc->top, rc->right, rc->bottom);
            DrawText(hdc, str, str.Size(), &rc1, DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
         }else full_width=true;
         break;
      case TE_ARRAY:
         {
            C_fstr str("[%i]", ArrayLen());
            if(te->branch_text){
               str = C_fstr("%s  %s", (const char*)str, (const char*)FormatString(dd, te->branch_text, te,
                  ArrayLen()));
            }
            int x = dd->table_break + 4;
            TextOut(hdc, x, y, str, str.Size());
         }
         break;
      }

      if(array_member){
         char *cp="";
         char buf[512];
         switch(te->type){
         case TE_ARRAY: case TE_BRANCH: cp=expanded ? "- " : "+"; break;
         }
         caption=te->caption;
         const char *fmt = "%s[%i] %s";
         if(caption && caption[0]=='%' && caption[1]=='x'){
            fmt = "%s[%x] %s";
            caption += 2;
         }
         sprintf(buf, fmt, cp, array_indx_show, caption ? caption : "");
         int x=indent_level*dd->table_indent + 4;
         int text_width = dd->table_break - x;
         RECT rc1;
         SetRect(&rc1, x, rc->top, x+text_width, rc->bottom);
         DrawText(hdc, buf, strlen(buf), &rc1, DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
      }else{
         caption = te->caption;
         if(caption){
            char buf[256];

            switch(te->type){
            case TE_ARRAY: case TE_BRANCH:
               strcpy(buf, expanded ? "- " : "+");
               strcat(buf, caption);
               caption=buf;
               break;
            }
            int x = indent_level*dd->table_indent + 4;
            int text_width=(full_width ? rc->right : dd->table_break) - x;
            RECT rc1;
            SetRect(&rc1, x, rc->top, x+text_width, rc->bottom);
            DrawText(hdc, caption, strlen(caption), &rc1, DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
         }
      }

      int i;
                              //show/hide controlls
      for(i=0; i<2; i++){
         HWND hwnd=NULL;
         int x=0, y=0, sx=0, sy=0;
         switch(te->type){
         case TE_FLOAT: case TE_INT:
            {
               bool range = HasRange();
               if(!range || i){
                  if(!i) break;

                  hwnd = dd->hwnd_edit;
                  if(active){
                              //setup edit controll text
                     if(sel_changed){
                        disable_modify = true;
                        switch(te->type){
                        case TE_INT:
                           {
                              int value = dd->table->ItemI(tab_indx, array_indx);
                              if(value!=TAB_INV_INT)
                                 SetWindowInt(dd->hwnd_edit, value, true);
                              else
                                 SendMessage(dd->hwnd_edit, WM_SETTEXT, 0, (LPARAM)"");
                           }
                           break;
                        case TE_FLOAT:
                           {
                              float f = dd->table->ItemF(tab_indx, array_indx);
                              if(f!=TAB_INV_FLOAT)
                                 SetWindowFlt(dd->hwnd_edit, f, true);
                              else
                                 SendMessage(dd->hwnd_edit, WM_SETTEXT, 0, (LPARAM)"");
                           }
                           break;
                        }
                        disable_modify = false;
                     }
                     x = dd->table_break+2, y=1;
                     sx = rc->right-x-2;
                     if(!range) x += dd->table_value_sx;
                     //if(!range) x += sx/2;
                     //else sx -= dd->table_value_sx;
                     else sx = dd->table_value_sx;
                     sy = SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, 0, 0) - 3;
                  }
               }else{
                  hwnd = dd->hwnd_slider;
                  if(active){
                           //setup slider
                     int i, j;
                     switch(te->type){
                     case TE_INT:
                        {
                           if(sel_changed){
                              SendMessage(hwnd, TBM_SETLINESIZE, 0, 1);
                              SendMessage(hwnd, TBM_SETPAGESIZE, 0, (dword)sqrt(float(te->int_max-te->int_min)));
                              SendMessage(hwnd, TBM_SETRANGEMIN, false, te->int_min);
                              SendMessage(hwnd, TBM_SETRANGEMAX, false, te->int_max);
                              j=te->int_max-te->int_min;
                              i=1;
                              if(j > 16) i=j/8;
                              SendMessage(hwnd, TBM_SETTICFREQ, i, 0);
                           }
                           int i = dd->table->ItemI(tab_indx, array_indx);
                           if(i==TAB_INV_INT)
                              i = 0;
                           SendMessage(hwnd, TBM_SETPOS, true, i);
                        }
                        break;
                     case TE_FLOAT:
                        {
                           if(!sel_changed)
                              break;
                           RECT rc;
                           SendMessage(hwnd, TBM_GETCHANNELRECT, 0, (LPARAM)&rc);
                           j=rc.right-rc.left;
                           SendMessage(hwnd, TBM_SETRANGEMIN, false, 0);
                           SendMessage(hwnd, TBM_SETRANGEMAX, false, j);
                           SendMessage(hwnd, TBM_SETLINESIZE, 0, 1);
                           SendMessage(hwnd, TBM_SETPAGESIZE, 0, (dword)sqrt(float(Max(0, j))));
                           SendMessage(hwnd, TBM_SETTICFREQ, 0, 0);
                           float range = float(te->flt_max - te->flt_min);
                           float f = dd->table->ItemF(tab_indx, array_indx);
                           if(f==TAB_INV_FLOAT){
                              i = 0;
                           }else{
                              f -= te->flt_min;
                              if(f<range)
                                 i = int(f*j/range);
                              else i = j;
                           }
                           SendMessage(hwnd, TBM_SETPOS, true, i);
                        }
                        break;
                     }
                     x = dd->table_break + dd->table_value_sx, y=1;
                     sx = rc->right-x-2;
                     sy=SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, 0, 0) - 3;
                  }
               }
            }
            break;

         case TE_ENUM:
            hwnd = dd->hwnd_choose;
            if(active){
                        //setup choose controll items
               SendMessage(hwnd, LB_RESETCONTENT, 0, 0);
               const char *cp = te->enum_list;
               if(!cp){
                  if(!enum_text.Size()){
                     if(dd->cb_proc){
                        dd->cb_proc(dd->table, TCM_GET_ENUM_STRING, dd->cb_user, tab_indx, (dword)&enum_text);
                     }
                  }
                  if(enum_text.Size())
                     cp = enum_text;
               }

               int num_vals = 0;
               if(cp)
               while(*cp){
                  SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)cp);
                  cp += strlen(cp)+1;
                  ++num_vals;
               }
               expanded=false;
               int i = dd->table->ItemE(tab_indx, array_indx);
               if(i!=TAB_INV_ENUM){
                  SendMessage(hwnd, LB_SETCURSEL, i, 0);
                  SendMessage(hwnd, LB_SETTOPINDEX, i, 0);
               }
               x = dd->table_break + 2, y=1;
               sx = rc->right - (dd->table_break + dd->table_draw_value_x)-1;
               sy = SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, 0, 0) - 3;
               sy = Min(sy, (int)rc->bottom);
            }
            break;
         case TE_BOOL:
            if(!sel_changed)
               break;
            hwnd = dd->hwnd_check;
            if(active){
               byte b = (byte&)dd->table->ItemB(tab_indx, array_indx);
               SendMessage(hwnd, BM_SETCHECK, b==TAB_INV_BOOL ? BST_INDETERMINATE : b ? BST_CHECKED : BST_UNCHECKED, 0);
               x = dd->table_break + dd->table_value_sx, y=2;
            }
            break;
         case TE_STRING:
            hwnd = dd->hwnd_edit;
            if(active){
               if(!i){
                  disable_modify = true;
                        //setup edit controll text
                  const char *cp = GetString(dd, NULL);
                  char buf[4];
                  if(cp[0]==TAB_INV_STRING[0]){
                     strcpy(buf, "");
                     cp = buf;
                  }
                  SendMessage(dd->hwnd_edit, WM_SETTEXT, 0, (LPARAM)cp);
                  SendMessage(hwnd, EM_SETSEL, 0, strlen(cp));
                  x = dd->table_break + 2, y=1;
                  sx = rc->right - (dd->table_break + dd->table_draw_value_x)-1;
                  sy = SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, 0, 0) - 3;
                  disable_modify = false;
               }
            }
            break;
         }
         if(hwnd){
            if(active){
               SetWindowPos(hwnd, NULL, rc->left+x, rc->top+y, sx, sy,
                  (!sx ? SWP_NOSIZE : 0) |
                  SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);
               RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
               ::SetFocus(hwnd);
            }else ShowWindow(hwnd, SW_HIDE);
         }
         if(te->type!=TE_FLOAT && te->type!=TE_INT)
            break;
      }
                              //draw other controlls
      char buf[256];
                              //save text color
      COLORREF text_color=GetTextColor(hdc);

      switch(te->type){
      case TE_BOOL:
         {
            byte b = (byte&)dd->table->ItemB(tab_indx, array_indx);
                              //check if default
            if(b==TAB_INV_BOOL){
               SetDefaultColor(hdc);
               strcpy(buf, "---");
            }else{
               sprintf(buf, "%s", b ? TRUE_TEXT : FALSE_TEXT);
            }
            TextOut(hdc, dd->table_break + dd->table_draw_value_x, y, buf, strlen(buf));
         }
         break;
      case TE_INT:
         {
            if(active && HasRange())
               break;
            int i = dd->table->ItemI(tab_indx, array_indx);
                              //check if default
            if(i==TAB_INV_INT){
               SetDefaultColor(hdc);
               strcpy(buf, "---");
            }else{
               sprintf(buf, "%i", i);
            }
            TextOut(hdc, dd->table_break + dd->table_draw_value_x, y, buf, strlen(buf));
         }
         break;
      case TE_FLOAT:
         {
            if(active && HasRange())
               break;
            float f = dd->table->ItemF(tab_indx, array_indx);
                              //check if default
            if(f==TAB_INV_FLOAT){
               SetDefaultColor(hdc);
               strcpy(buf, "---");
            }else{
               sprintf(buf, "%f", f);
            }
            RemoveFloatZeros(buf);
            TextOut(hdc, dd->table_break + dd->table_draw_value_x, y, buf, strlen(buf));
         }
         break;
      case TE_COLOR_RGB:
      case TE_COLOR_VECTOR:
         {
            union{
               byte b[4];
               dword dw;
            } au;
            au.dw = GetColor(dd);
            switch(te->type){
            case TE_COLOR_RGB:
               sprintf(buf, "(r=%i, g=%i, b=%i)", au.b[0], au.b[1], au.b[2]);
               break;
            case TE_COLOR_VECTOR:
               //float *f = (float*)dd->table->Item(tab_indx);
               const S_vector &f = dd->table->ItemV(tab_indx);
               sprintf(buf, "(r=%.2f, g=%.2f, b=%.2f)", f[0], f[1], f[2]);
               break;
            }
            int x = dd->table_break + dd->table_value_sx/2 + 4;
            //if(dd->def_table && GetColor(dd, true)==au.dw) SetDefaultColor(hdc);
            RECT rc1;
            SetRect(&rc1, x, rc->top, rc->right, rc->bottom);
            DrawText(hdc, buf, strlen(buf), &rc1, DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

            HBRUSH br = CreateSolidBrush(au.dw);
            HBRUSH old_brush = (HBRUSH)SelectObject(hdc, br);
            Rectangle(hdc, dd->table_break + 3, rc->top+1, dd->table_break + dd->table_value_sx/2, rc->bottom-2);
            SelectObject(hdc, old_brush);
            DeleteObject(br);
         }
         break;

      case TE_STRING:
         if(!active){
            const char *cp = GetString(dd, NULL);
                              //check if default
            if(cp[0]==TAB_INV_STRING[0]){
               SetDefaultColor(hdc);
               strcpy(buf, "---");
               cp = buf;
            }
            int x = dd->table_break + dd->table_draw_value_x;
            int text_width = rc->right-x;
            RECT rc1;
            SetRect(&rc1, x, rc->top, x+text_width, rc->bottom);
                              //draw quoted text
            DrawText(hdc, cp, strlen(cp), &rc1, DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
         }
         break;

      case TE_ENUM:
         if(!active){
            const char *cp;
            i = dd->table->ItemE(tab_indx, array_indx);
            if(i==TAB_INV_ENUM){
               SetDefaultColor(hdc);
               strcpy(buf, "---");
               cp = buf;
            }else{
               cp = GetEnumText(dd, i, te);
            }
            int x = dd->table_break + dd->table_draw_value_x;
            int text_width = rc->right-x;
            RECT rc1;
            SetRect(&rc1, x, rc->top, x+text_width, rc->bottom);
            DrawText(hdc, cp, strlen(cp), &rc1, DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
         }
         break;
      }
                              //draw selection
      if(active){
         HBRUSH br = CreateSolidBrush(0x0000ff);
         RECT rc1;
         rc1 = *rc;
         rc1.left++;
         rc1.bottom--;
         FrameRect(hdc, &rc1, br);
         DeleteObject(br);
      }

      SetTextColor(hdc, text_color);
   }

//----------------------------

   void InvalidateValueRect(S_db_data *dd, dword part){

      RECT rc;
      GetClientRect(dd->hwnd_lbox, &rc);
      switch(part){
      case 1:                 //value
         rc.left = dd->table_break + dd->table_draw_value_x;
         rc.right = dd->table_break + dd->table_draw_value_x + dd->table_value_sx;
         break;
      case 2:                 //formatted text
         if(!te->branch_text) return;
         rc.left = dd->table_break + dd->table_draw_value_x;
         break;
      }
      int i = SendMessage(dd->hwnd_lbox, LB_FINDSTRING, (dword)-1, (LPARAM)this);
      int height=SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, i, 0);
      i -= SendMessage(dd->hwnd_lbox, LB_GETTOPINDEX, 0, 0);
      rc.top = i*height;
      rc.bottom = rc.top + height - 1;

      InvalidateRect(dd->hwnd_lbox, &rc, true);
   }

//----------------------------

   void InitchooseCtl(S_db_data *dd, LPRECT rc, bool expand, int num_vals){

      dword item_height = SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, 0, 0);
      int sx=rc->right-(dd->table_break + dd->table_draw_value_x)-1;
      int sy;
      int h = item_height;
      int i=SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0) - SendMessage(dd->hwnd_lbox, LB_GETTOPINDEX, 0, 0);
      int x=dd->table_break + 2, y=h*i;

      if(expand){
         sy = num_vals * item_height;
         sy += GetSystemMetrics(SM_CYBORDER)*2;

         if(sy+y > rc->bottom){
            y = Max(0, (int)rc->bottom-sy);
            if(!y)
               sy = Min(sy, (int)rc->bottom);
         }
      }else sy=h-3;
      SetWindowPos(dd->hwnd_choose, HWND_TOP, x, y, sx, sy,
         SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
   }

//----------------------------

   const char *GetString(S_db_data *dd, int *len1){

      static char buf[256];
      int max_str_len = dd->table->StringSize(tab_indx);
      PC_table tp = dd->table;
      const char *cp = tp->ItemS(tab_indx, array_indx);//, max_str_len);
      int i=0;
      while(cp[i]) if(++i==max_str_len) break;
      if(i==max_str_len){
         i=Min(255, i);
         strncpy(buf, cp, i);
         cp=buf;
      }
      if(len1) *len1=max_str_len;
      return cp;
   }

//----------------------------

   int SetString(S_db_data *dd, const char *str){

      int max_str_len = dd->table->StringSize(tab_indx);
      char *cp = dd->table->ItemS(tab_indx, array_indx);//, max_str_len);
      int slen=strlen(str);
      if(slen < max_str_len){
         strcpy(cp, str);
         max_str_len=slen;
      }else{
         strncpy(cp, str, --max_str_len);
         cp[max_str_len]=0;
      }
      return max_str_len;
   }

//----------------------------

   dword GetColor(S_db_data *dd){
      union{
         byte b[4];
         dword dw;
      } au;
      switch(te->type){
      case TE_COLOR_RGB:
         {
            S_rgb *rgb = (S_rgb*)dd->table->GetItem(tab_indx, TE_NULL);
            *(S_rgb*)&au = rgb[array_indx];
         }
         break;
      case TE_COLOR_VECTOR:
         {
            const S_vector &fp = dd->table->ItemV(tab_indx, array_indx);
            au.b[0] = byte(fp[0]*255.0f);
            au.b[1] = byte(fp[1]*255.0f);
            au.b[2] = byte(fp[2]*255.0f);
         }
         break;
      default: assert(0); au.dw = 0;
      }
      return au.dw & 0xffffff;
   }

//----------------------------

   void SetColor(S_db_data *dd, dword color){
      switch(te->type){
      case TE_COLOR_RGB:
         {
            S_rgb *rgb = (S_rgb*)dd->table->GetItem(tab_indx, TE_NULL);
            S_rgb &c = rgb[array_indx];
            c = *(S_rgb*)&color;
         }
         break;
      case TE_COLOR_VECTOR:
         union{
            byte b[4];
            dword dw;
         } au;
         au.dw = color;
         S_vector &fp = dd->table->ItemV(tab_indx, array_indx);
         fp[0] = (float)au.b[0]/255.0f;
         fp[1] = (float)au.b[1]/255.0f;
         fp[2] = (float)au.b[2]/255.0f;
      }
   }

//----------------------------

#define AF_HIT          1
#define AF_MODIFY       2
#define AF_SET_VALUE    4
#define AF_DBLCLICK     8
//#define AF_RESET        0x10  //reset default value
#define AF_BROWSE       0x20
                              //ChooseColor hook
   static UINT APIENTRY ccHook(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam){
      switch(uiMsg){
      case WM_INITDIALOG:
         {
            LPCHOOSECOLOR cc=(LPCHOOSECOLOR)lParam;
            SetWindowLong(hwnd, GWL_USERDATA, cc->lCustData);
         }
         break;
      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case 706: case 707: case 708:
                              //hook RGB edit controlls
            switch(HIWORD(wParam)){
            case EN_CHANGE:
               S_db_data *dd=(S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
               if(dd){
                  int i=SendDlgItemMessage(dd->hwnd_dlg, IDC_STUFF, LB_GETCURSEL, 0, 0);
                  if(i==-1) break;
                              //update
                  dword b[4] = {
                     GetDlgItemInt(hwnd, 706, NULL, false),
                     GetDlgItemInt(hwnd, 707, NULL, false),
                     GetDlgItemInt(hwnd, 708, NULL, false),
                     0
                  };
                  dword color = (b[2]<<16) | (b[1]<<8) | b[0];
                  S_element_data *ed = (S_element_data*)SendDlgItemMessage(dd->hwnd_dlg, IDC_STUFF, LB_GETITEMDATA, i, 0);
                  ed->Action(dd, AF_SET_VALUE, color);
               }
               break;
            }
            break;
         }
         break;
      }
      return 0;
   }

   void SetModify(S_db_data *dd){
      dd->table->SetModify(/*dd->create_flags&TABEDIT_INFO*/);
   }

//----------------------------

   bool disable_modify;

//----------------------------

   void Action(S_db_data *dd, dword flags, dword data=0){

      if((flags&AF_MODIFY) && disable_modify) return;
                              //if callback, better inc ref
      if(dd->cb_proc){
         dd->AddRef();
         AddRef();
      }

      switch(te->type){
      case TE_BOOL:           //switch boolean state
         if(flags&(AF_HIT|AF_DBLCLICK)){
            bool b = false;
            if(flags&(AF_HIT|AF_DBLCLICK)){
               b = dd->table->ItemB(tab_indx, array_indx);
               b = !b;
            }
            bool &ref = dd->table->ItemB(tab_indx, array_indx);
            if(ref!=b){
               ref=b;
               SendMessage(dd->hwnd_check, BM_SETCHECK, b, 0);
               InvalidateValueRect(dd, 0);
               if(dd->cb_proc)
                  dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
               SetModify(dd);
            }
         }
         break;
      case TE_COLOR_RGB:      //select color
      case TE_COLOR_VECTOR:
         if(flags&(AF_HIT|AF_DBLCLICK)){
            dword save_color=GetColor(dd);
            static COLORREF custom[16];
            CHOOSECOLOR cc={
               sizeof(CHOOSECOLOR),
               dd->hwnd_dlg,
               NULL,
               save_color,
               custom,
               CC_ANYCOLOR | CC_FULLOPEN | CC_SOLIDCOLOR | CC_RGBINIT | CC_ENABLEHOOK,
               (dword)dd,
               ccHook,
               NULL
            };
            int i=ChooseColor(&cc);
            SetColor(dd, i ? cc.rgbResult : save_color);
         }else
         if(flags&AF_SET_VALUE){
            SetColor(dd, data);
            InvalidateValueRect(dd, 0);
         }
         if(dd->cb_proc)
            dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
         SetModify(dd);
         break;

      case TE_INT:            //get and apply value
         if(flags&AF_MODIFY){
            if(HasRange() && GetFocus()!=dd->hwnd_edit)
               break;
            int i, num = 0;
            if(flags&AF_MODIFY){
               char buf[256];
               SendMessage(dd->hwnd_edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
               i = sscanf(buf, "%i", &num);
               //if(i==EOF || i!=1) break;
               if(i==EOF || i!=1)
                  num=0;
            }
            int &ref = dd->table->ItemI(tab_indx, array_indx);
            if(ref!=num){
               ref = num;
               if(dd->cb_proc)
                  dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
               SetModify(dd);
               InvalidateValueRect(dd, 0);
            }
         }else
         if(flags&AF_SET_VALUE){
            int &ref = dd->table->ItemI(tab_indx, array_indx);
            if(ref != (int)data){
               SetWindowInt(dd->hwnd_edit, data, true);
               ref=data;
               if(dd->cb_proc) dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
               SetModify(dd);
            }
         }
         break;

      case TE_FLOAT:          //get and apply value
         if(flags&AF_MODIFY){
            if(HasRange() && GetFocus()!=dd->hwnd_edit) break;
            float num = 0;
            if(flags&AF_MODIFY){
               char buf[256];
               SendMessage(dd->hwnd_edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
               int i=sscanf(buf, "%f", &num);
               //if(i==EOF || i!=1) break;
               if(i==EOF || i!=1) num = 0.0f;
            }
            float &ref = dd->table->ItemF(tab_indx, array_indx);
                              //binary compare to avoid -nan problems
            if(*(dword*)&ref != *(dword*)&num){
               if(HasRange()){
                              //position slide-bar
                  int i, j;
                  RECT rc;
                  SendMessage(dd->hwnd_slider, TBM_GETCHANNELRECT, 0, (LPARAM)&rc);
                  j=rc.right-rc.left;
                  float range = float(te->flt_max - te->flt_min);
                  float f=num - te->flt_min;
                  i = int(f*j/range);
                  SendMessage(dd->hwnd_slider, TBM_SETPOS, true, i);
               }

               ref = num;
               if(dd->cb_proc)
                  dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
               SetModify(dd);
               InvalidateValueRect(dd, 0);
            }
         }else
         if(flags&AF_SET_VALUE){
            float f=(float)data/(float)SendMessage(dd->hwnd_slider, TBM_GETRANGEMAX, 0, 0);
            f *= (te->flt_max-te->flt_min);
            f = Min((float)te->flt_max,
               (float)(Max(0.0f, f)+te->flt_min));
            float &ref = dd->table->ItemF(tab_indx, array_indx);
            if(ref!=f){
               SetWindowFlt(dd->hwnd_edit, f, true);

               ref=f;
               if(dd->cb_proc) dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
               SetModify(dd);
            }
         }
         break;

      case TE_STRING:
         if(flags&(/*AF_BROWSE|*/AF_MODIFY)){
            int i, num = 0;
            char buf[256];
            if(flags&AF_MODIFY){
               num = SendMessage(dd->hwnd_edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
            }
                              //check if modified
            char *cp = dd->table->ItemS(tab_indx, array_indx);//, dd->table->StringSize(tab_indx));
            if(strlen(cp)==(dword)num && !strcmp(cp, buf)) break;

            int nlen=SetString(dd, buf);
            if(active && nlen<num){
                              //crop
               buf[nlen]=0;
               SendMessage(dd->hwnd_edit, EM_GETSEL, NULL, (LPARAM)&i);
               SendMessage(dd->hwnd_edit, WM_SETTEXT, 0, (LPARAM)buf);
               SendMessage(dd->hwnd_edit, EM_SETSEL, i, i);
            }
            if(dd->cb_proc)
               dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
            SetModify(dd);
         }
         break;

      case TE_ENUM:
         if(flags&(AF_HIT|AF_DBLCLICK)){
            RECT rc;
            GetClientRect(dd->hwnd_lbox, &rc);
                              //expand/collapse
            if(!expanded){
               int i=SendMessage(dd->hwnd_choose, LB_GETCOUNT, 0, 0);
               InitchooseCtl(dd, &rc, true, i);
            }else{
               InitchooseCtl(dd, &rc, false, 0);
               SendMessage(dd->hwnd_choose, LB_SETTOPINDEX, SendMessage(dd->hwnd_choose, LB_GETCURSEL, 0, 0), 0);
            }
            expanded=!expanded;
         }else
         if(flags&AF_MODIFY){
            int i=SendMessage(dd->hwnd_choose, LB_GETCURSEL, 0, 0);
            if(!expanded) SendMessage(dd->hwnd_choose, LB_SETTOPINDEX, i, 0);
            dd->table->ItemE(tab_indx, array_indx) = (byte)i;
            if(dd->cb_proc)
               dd->cb_proc(dd->table, TCM_MODIFY, dd->cb_user, tab_indx, array_indx);
            SetModify(dd);
         }
         break;
      case TE_BRANCH:
         if(flags&(AF_HIT|AF_DBLCLICK)){
            if(!expanded){
               int indx = array_indx;
               //if(array_member) indx *= ArrayLen();
               InsertTable(dd, this, te->branch_depth, indent_level+1, templ_indx+1, indx, 0);
            }else{
               DestroyTable(dd, te->branch_depth, SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0)+1);
            }
            expanded=!expanded;
         }
         break;
      case TE_ARRAY:
         if(flags&(AF_HIT|AF_DBLCLICK)){
            int array_len = ArrayLen();
            if(!expanded){
               int indx = array_indx;
               if(array_member) indx *= array_len;
               InsertTable(dd, this, array_len, indent_level+1, templ_indx+1, indx, 0);
            }else{
               DestroyTable(dd, array_len, SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0)+1);
            }
            expanded=!expanded;
         }
         break;
      }
      if(branch_root)
         branch_root->InvalidateValueRect(dd, 2);
      if(dd->cb_proc){
         dd->Release();
         Release();
      }
   }

//----------------------------

   void SetFocus1(S_db_data *dd){
      //if(dd->cb_proc) dd->cb_proc(dd->table, TCM_FOCUS, dd->cb_user, tab_indx, 0);
   }
};

//----------------------------

static void InsertTable(S_db_data *dd, S_element_data *root, int num_elems,
   int indent, int templ_indx1, int array_index, int array_index_show){

   const C_table_element *te = dd->templ->GetElements();

   int templ_indx = templ_indx1;
   int skip_count = 0;
   int insert_pos = SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0);
   ++insert_pos;
   bool is_array = (root && root->te->type==TE_ARRAY);

   while(te[templ_indx].type!=TE_NULL){
      if(!skip_count){
         S_element_data *ed = new S_element_data(dd, root, templ_indx, indent,
            array_index, array_index_show, is_array);
         SendMessage(dd->hwnd_lbox, LB_INSERTSTRING, insert_pos++, (LPARAM)ed);
      }else
         --skip_count;

      if(is_array){
         ++array_index;
         ++array_index_show;
      }else{
         switch(te[templ_indx].type){
         case TE_BRANCH:
            skip_count += te[templ_indx].branch_depth;
            break;
         case TE_ARRAY:
            skip_count++;
            break;
         }
         ++templ_indx;
      }
      if(!skip_count && num_elems && !--num_elems)
         break;
   }
}

//----------------------------

static void DestroyTable(S_db_data *dd, int num_elems, int elem_base){

   while(num_elems--){
      S_element_data *ed=(S_element_data*)
         SendMessage(dd->hwnd_lbox, LB_GETITEMDATA, elem_base, 0);
      switch(ed->te->type){
      case TE_BRANCH:
      case TE_ARRAY:
         if(ed->IsExpanded()) //collapse this branch
            DestroyTable(dd, ed->te->branch_depth, elem_base+1);
         break;
      }
      ed->Release();
      SendMessage(dd->hwnd_lbox, LB_DELETESTRING, elem_base, 0);
   }
}

//----------------------------

static void DestroyAllTable(S_db_data *dd){
                              //destroy all items
   dword i = SendMessage(dd->hwnd_lbox, LB_GETCOUNT, 0, 0);
   while(i--){
      S_element_data *ed = (S_element_data*)
         SendMessage(dd->hwnd_lbox, LB_GETITEMDATA, i, 0);
      ed->Release();
   }
   SendMessage(dd->hwnd_lbox, LB_RESETCONTENT, 0, 0);
}

//----------------------------

static int ImportTable(S_db_data *dd){

   char buf[256];
   buf[0] = 0;
   char dir[256];
   GetCurrentDirectory(sizeof(dir), dir);

   OPENFILENAME of;
   memset(&of, 0, sizeof(of));
   of.lStructSize=sizeof(of);
   of.hwndOwner=dd->hwnd_dlg;
   of.lpstrFile=buf;
   of.nMaxFile=sizeof(buf);
   of.lpstrFilter="(*.tab)\0*.tab\0";
   of.lpstrTitle="Import table";
   of.lpstrDefExt="tab";
   of.lpstrInitialDir=dir;
   of.Flags=OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_NOTESTFILECREATE;

   if(!GetOpenFileName(&of)) return -1;

   int ok=-2;
                              //open temp table
   PC_table tmp_tab = CreateTable();
   if(tmp_tab->Load(buf, TABOPEN_FILENAME)){
                              //check guids
      //if(!memcmp(tmp_tab->GetGUID(), dd->templ->GetGUID(), sizeof(GUID)))
      {
                              //duplicate to current table
         dd->table->Load(tmp_tab, TABOPEN_DUPLICATE | TABOPEN_UPDATE);
         ok = 0;
      }
   }

   tmp_tab->Release();
   return ok;
}

//----------------------------

static bool ExportTable(S_db_data *dd){

   char buf[256];
   buf[0]=0;
   char dir[256];
   GetCurrentDirectory(sizeof(dir), dir);

   OPENFILENAME of;
   memset(&of, 0, sizeof(of));
   of.lStructSize=sizeof(of);
   of.hwndOwner=dd->hwnd_dlg;
   of.lpstrFile=buf;
   of.nMaxFile=sizeof(buf);
   of.lpstrFilter="(*.tab)\0*.tab\0";
   of.lpstrTitle="Export table";
   of.lpstrDefExt="tab";
   of.lpstrInitialDir=dir;
   of.Flags=OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_NOTESTFILECREATE | OFN_OVERWRITEPROMPT;

   if(!GetSaveFileName(&of))
      return true;
   return dd->table->Save(buf, TABOPEN_FILENAME);
}

//----------------------------

#define NUM_DLG_BUTTONS 2

static void SizeWindow(S_db_data *dd){

   RECT rc, rc1;
   int i;
   GetClientRect(dd->hwnd_dlg, &rc);
   if(dd->buttons){
      int x = 0;
      memset(&rc1, 0, sizeof(rc1));
      for(i=0; i<NUM_DLG_BUTTONS; i++){
         HWND hwnd_but = GetDlgItem(dd->hwnd_dlg, IDC_BUTTON_DEF + 3 + i);
         GetWindowRect(hwnd_but, &rc1);
         SetWindowPos(hwnd_but, NULL,
            x, rc.bottom-(rc1.bottom-rc1.top), 0, 0,
            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE);
         ShowWindow(hwnd_but, SW_SHOW);
         x += rc1.right-rc1.left+3;
      }
      rc.bottom -= (rc1.bottom-rc1.top) + 2;
   }

   SetWindowPos(dd->hwnd_lbox, NULL, rc.left, rc.top, rc.right, rc.bottom,
      SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
   InvalidateRect(dd->hwnd_lbox, &rc, true);
}

//----------------------------

static void DrawButton(LPDRAWITEMSTRUCT lpdis){

   int i;
   HDC &hdc=lpdis->hDC;
   RECT &rc=lpdis->rcItem;

   {              //erase
      HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
      FillRect(hdc, &rc, br);
      DeleteObject(br);
   }

   bool slct=lpdis->itemState&ODS_SELECTED;
   bool focus=lpdis->itemState&ODS_FOCUS;
   bool dis=lpdis->itemState&ODS_DISABLED;

   {              //text
      char buf[256];
      SendMessage(lpdis->hwndItem, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
      SetBkMode(hdc, TRANSPARENT);
      for(i=0; i<1+dis; i++){
         if(dis)
            SetTextColor(hdc, GetSysColor(!i ? COLOR_BTNHILIGHT : COLOR_BTNSHADOW));
         RECT rc1 = rc;
         int j=(1+dis-i);
         InflateRect(&rc1, -j, -j-slct);
         if(!i) rc1.left++, rc1.right++;;
         DrawText(hdc, buf, strlen(buf), &rc1, DT_END_ELLIPSIS | DT_CENTER | DT_SINGLELINE | DT_TOP);
      }
   }

   if(focus){              //dark frame (focus)
      HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_3DDKSHADOW));
      FrameRect(hdc, &rc, br);
      DeleteObject(br);
      InflateRect(&rc, -1, -1);
   }
   {              //left/top
      LOGBRUSH lb={BS_SOLID, GetSysColor(!slct ? COLOR_BTNHILIGHT : COLOR_BTNSHADOW), 0 };
      HPEN pen=ExtCreatePen(PS_SOLID, 1, &lb, 0, NULL);
      HPEN old_pen = (HPEN)SelectObject(hdc, pen);

      MoveToEx(hdc, rc.left, rc.bottom-1, NULL);
      LineTo(hdc, rc.left, rc.top); LineTo(hdc, rc.right-1, rc.top);

      SelectObject(hdc, old_pen);
      DeleteObject(pen);
   }
   {              //right/bottom
      LOGBRUSH lb={BS_SOLID, GetSysColor(!slct ? COLOR_BTNSHADOW : COLOR_BTNHILIGHT), 0 };
      HPEN pen = ExtCreatePen(PS_SOLID, 1, &lb, 0, NULL);
      HPEN old_pen = (HPEN)SelectObject(hdc, pen);

      LineTo(hdc, rc.right-1, rc.bottom-1); LineTo(hdc, rc.left, rc.bottom-1);

      SelectObject(hdc, old_pen);
      DeleteObject(pen);
   }
}

//----------------------------
typedef long __stdcall std_proc(HWND, unsigned int, unsigned int, long);
                              //sublassing functions for various controlls

static BOOL CALLBACK WndProcLBox(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   int i;
   S_db_data *dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
   assert(dd);

   switch(uMsg){
   case WM_CHAR: return 0;
   case WM_ERASEBKGND:
      {
         HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_3DFACE));
         RECT rc;
         GetClientRect(hwnd, &rc);
         FillRect((HDC)wParam, &rc, br);
         DeleteObject(br);
      }
      return true;
   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDC_BROWSE:
         switch(HIWORD(wParam)){
         case BN_CLICKED: case BN_DBLCLK:
            i=SendMessage(hwnd, LB_GETCURSEL, 0, 0);
            if(i==-1) break;
            S_element_data *ed=(S_element_data*)SendMessage(hwnd, LB_GETITEMDATA, i, 0);
            ed->Action(dd, AF_BROWSE);
         }
         break;
      case IDC_CHECK:
         switch(HIWORD(wParam)){
         case BN_CLICKED: case BN_DBLCLK:
            i=SendMessage(hwnd, LB_GETCURSEL, 0, 0);
            if(i==-1) break;
            S_element_data *ed=(S_element_data*)SendMessage(hwnd, LB_GETITEMDATA, i, 0);
            ed->Action(dd, AF_HIT);
            break;
         }
         break;
      }
      break;

   case WM_KEYDOWN:
      switch(wParam){
      case VK_RIGHT:
      case VK_LEFT:
         i = SendMessage(hwnd, LB_GETCURSEL, 0, 0);
         if(i==-1) break;
         S_element_data *ed=(S_element_data*)SendMessage(hwnd, LB_GETITEMDATA, i, 0);
         switch(ed->te->type){
         case TE_BRANCH:
         case TE_ARRAY:
            bool b=ed->IsExpanded();
            if(wParam==VK_RIGHT) b=!b;
            if(b){
               ed->Action(dd, AF_HIT);
               return 0;
            }
            break;
         }
         break;
      }
      break;

   case WM_DRAWITEM:
      {
         LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT) lParam;
         switch(lpdis->CtlType){
         case ODT_BUTTON:
            DrawButton(lpdis);
            break;
         }
      }
      break;
   }
   return CallWindowProc((std_proc*)dd->orig_proc_lb, hwnd, uMsg, wParam, lParam);
}

//----------------------------

static BOOL CALLBACK WndProcEdit(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   HWND hwnd_dlg = GetParent(hwnd);
   S_db_data *dd = (S_db_data*)GetWindowLong(hwnd_dlg, GWL_USERDATA);

   switch(uMsg){
   case WM_GETDLGCODE: return DLGC_WANTCHARS | DLGC_WANTARROWS | DLGC_WANTTAB;

   case WM_KEYDOWN:
      switch(wParam){
      case VK_DOWN:
         if(SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0) == SendMessage(dd->hwnd_lbox, LB_GETCOUNT, 0, 0)-1)
            return 1;
         SetFocus(dd->hwnd_lbox);
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         return 0;
      case VK_UP:
         if(!SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0))
            return 1;
                              //flow...
      case VK_PRIOR: case VK_NEXT:
         SetFocus(dd->hwnd_lbox);
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         return 0;
      case VK_F1:
      case VK_F12:
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         break;
      case VK_TAB:
                              //for range-based edit, switch to slidebar
         SetFocus(dd->hwnd_slider);
         return 0;
      }
      break;
   }
   return CallWindowProc((std_proc*)dd->orig_proc_edn, hwnd, uMsg, wParam, lParam);
}

//----------------------------

static BOOL CALLBACK WndProcCheck(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   HWND hwnd_dlg = GetParent(hwnd);
   S_db_data *dd=(S_db_data*)GetWindowLong(hwnd_dlg, GWL_USERDATA);

   switch(uMsg){
   case WM_GETDLGCODE:
      return DLGC_WANTARROWS;

   case WM_KEYDOWN:
      switch(wParam){
      case VK_DOWN:
         if(SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0) == SendMessage(dd->hwnd_lbox, LB_GETCOUNT, 0, 0)-1)
            return 1;
         SetFocus(dd->hwnd_lbox);
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         return 0;
      case VK_UP:
         if(!SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0))
            return 1;
                              //flow...
      case VK_LEFT: case VK_RIGHT:
      case VK_PRIOR: case VK_NEXT:
      case VK_HOME: case VK_END:
         SetFocus(dd->hwnd_lbox);
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         return 0;
      case VK_F1:
      case VK_F12:
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         break;
      }
      break;
   }
   return CallWindowProc((std_proc*)dd->orig_proc_chk, hwnd, uMsg, wParam, lParam);
}

//----------------------------

static BOOL CALLBACK WndProcSld(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   int i;
   HWND hwnd_dlg = GetParent(hwnd);
   S_db_data *dd=(S_db_data*)GetWindowLong(hwnd_dlg, GWL_USERDATA);

   switch(uMsg){
   case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTTAB;

   case WM_KEYDOWN:
      bool modify = false;
      switch(wParam){
      case VK_LEFT: case VK_RIGHT:
         SendMessage(hwnd, TBM_SETPOS, true,
            SendMessage(hwnd, TBM_GETPOS, 0, 0) + (wParam==VK_LEFT ? -1 : 1));
         modify=true;
         break;
      case VK_HOME: case VK_END:
         SendMessage(hwnd, TBM_SETPOS, true,
            SendMessage(hwnd, (wParam==VK_HOME ? TBM_GETRANGEMIN : TBM_GETRANGEMAX), 0, 0));
         modify=true;
         break;
      case VK_PRIOR: case VK_NEXT:
         SendMessage(hwnd, TBM_SETPOS, true,
            SendMessage(hwnd, TBM_GETPOS, 0, 0) +
            SendMessage(hwnd, TBM_GETPAGESIZE, 0, 0) *
            (wParam==VK_PRIOR ? -1 : 1));
         modify=true;
         break;
      case VK_DOWN:
         if(SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0) == SendMessage(dd->hwnd_lbox, LB_GETCOUNT, 0, 0)-1)
            return 1;
         SetFocus(dd->hwnd_lbox);
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         return 0;
      case VK_UP:
         if(!SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0))
            return 1;
         SetFocus(dd->hwnd_lbox);
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         return 0;
      case VK_F1:
      case VK_F12:
         //if(dd->hwnd_main) SetFocus(dd->hwnd_main);
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         break;
      case VK_TAB:
         SetFocus(dd->hwnd_edit);
         return 0;
      }
      if(modify){
         i=SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0);
         if(i==-1) break;
         S_element_data *ed=(S_element_data*)SendMessage(dd->hwnd_lbox, LB_GETITEMDATA, i, 0);
         ed->Action(dd, AF_SET_VALUE, SendMessage(hwnd, TBM_GETPOS, 0, 0));
         return 0;
      }
      break;
   }
   return CallWindowProc((std_proc*)dd->orig_proc_sld, hwnd, uMsg, wParam, lParam);
}

//----------------------------

static BOOL CALLBACK WndProcChoose(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   int i;
   HWND hwnd_dlg = GetParent(hwnd);
   S_db_data *dd=(S_db_data*)GetWindowLong(hwnd_dlg, GWL_USERDATA);

   switch(uMsg){
   case WM_KEYDOWN:
      switch(wParam){
      case VK_DOWN:
      case VK_UP:
      case VK_PRIOR: case VK_NEXT:
         {
            i=SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0);
            if(i==-1) break;
            S_element_data *ed=(S_element_data*)SendMessage(dd->hwnd_lbox, LB_GETITEMDATA, i, 0);
            if(ed->IsExpanded()) break;
            SetFocus(GetParent(hwnd));
            SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         }
         return 0;
      case VK_F1:
      case VK_F12:
         SendMessage(dd->hwnd_lbox, uMsg, wParam, lParam);
         break;
      }
      break;
   }
   return CallWindowProc((std_proc*)dd->orig_proc_cmb, hwnd, uMsg, wParam, lParam);
}

//----------------------------

static BOOL CALLBACK dlgProperties(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   S_db_data *dd;
   RECT rc;
   int i;

   switch(uMsg){
   case WM_INITDIALOG:
      {
         dd = new S_db_data;
         *dd = *(S_db_data*)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, (long)dd);

         dword ws = GetWindowLong(hwnd, GWL_STYLE);
         if(!(dd->create_flags&TABEDIT_CONTROL)){
            ws |= WS_CAPTION | WS_SYSMENU;
            ws &= ~DS_CONTROL;
         }else{
            ws |= DS_CONTROL;
            
         }
         SetWindowLong(hwnd, GWL_STYLE, ws);

         dd->table_break = (dd->create_flags&TABEDIT_EXACTHEIGHT) ? TABLE_BREAK2 : TABLE_BREAK1;
         MapDlgUnits(hwnd, &dd->table_break, NULL);
         dd->table_draw_value_x=2; MapDlgUnits(hwnd, &dd->table_draw_value_x, NULL);
         dd->table_indent=4; MapDlgUnits(hwnd, &dd->table_indent, NULL);

                              //set window's caption
         {
            const char *cp=dd->templ->GetCaption();
            char buf[512];
            if(dd->create_flags&TABEDIT_EDITING_DEFAULTS){
               sprintf(buf, "%s [default table]", cp);
               cp=buf;
            }
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)cp);
         }

         dd->hwnd_dlg = hwnd;
         dd->hwnd_lbox = GetDlgItem(hwnd, IDC_STUFF);
         SetWindowLong(dd->hwnd_lbox, GWL_USERDATA, (long)dd);
         SetWindowLong(dd->hwnd_lbox, GWL_STYLE, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | GetWindowLong(dd->hwnd_lbox, GWL_STYLE));
                              //subclass list-box
         dd->orig_proc_lb = (WNDPROC)SetWindowLong(dd->hwnd_lbox, GWL_WNDPROC, (dword)WndProcLBox);

                              //init edit controlls
                              //CHECK
         dd->hwnd_check = GetDlgItem(hwnd, IDC_CHECK);
         SetParent(dd->hwnd_check, dd->hwnd_lbox);
         dd->orig_proc_chk = (WNDPROC)SetWindowLong(dd->hwnd_check, GWL_WNDPROC, (dword)WndProcCheck);
         int sx = 20;
         int sy = SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, 0, 0) - 5;
         MapDlgUnits(hwnd, &sx, 0);
         SetWindowPos(dd->hwnd_check, NULL, 0, 0, sx, sy, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);
                              //EDIT
         dd->hwnd_edit = GetDlgItem(hwnd, IDC_EDIT);
         SetParent(dd->hwnd_edit, dd->hwnd_lbox);
         dd->orig_proc_edn = (WNDPROC)SetWindowLong(dd->hwnd_edit, GWL_WNDPROC, (dword)WndProcEdit);
         dd->table_value_sx = (dd->create_flags&TABEDIT_EXACTHEIGHT) ? TABLE_VALUE_SX2 : TABLE_VALUE_SX1;
         MapDlgUnits(hwnd, &dd->table_value_sx, NULL);
                              //SLIDER
         dd->hwnd_slider=GetDlgItem(hwnd, IDC_SLIDER);
         SetParent(dd->hwnd_slider, dd->hwnd_lbox);
         dd->orig_proc_sld = (WNDPROC)SetWindowLong(dd->hwnd_slider, GWL_WNDPROC, (dword)WndProcSld);
                              //CHOOSE
         dd->hwnd_choose=GetDlgItem(hwnd, IDC_CHOOSE);
         SetParent(dd->hwnd_choose, dd->hwnd_lbox);
         dd->orig_proc_cmb = (WNDPROC)SetWindowLong(dd->hwnd_choose, GWL_WNDPROC, (dword)WndProcChoose);
                              //BROWSE
         dd->hwnd_browse=GetDlgItem(hwnd, IDC_BROWSE);
         SetParent(dd->hwnd_browse, dd->hwnd_lbox);
         sx = 10;
         sy = SendMessage(dd->hwnd_lbox, LB_GETITEMHEIGHT, 0, 0) - 3;
         MapDlgUnits(hwnd, &sx, 0);
         SetWindowPos(dd->hwnd_browse, NULL, 0, 0, sx, sy, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);

         InsertTable(dd);
         SendMessage(dd->hwnd_lbox, LB_SETCURSEL, 0, 0);

                              //setup dialog pos & size
         int x = 0;
         int y = 0;
         sx = DEFAULT_SIZE_X;
         sy = DEFAULT_SIZE_Y;
         MapDlgUnits(hwnd, &sx, &sy);

         if(dd->pos_size){
            x = dd->pos_size[0], y = dd->pos_size[1];
            if(dd->pos_size[2] && dd->pos_size[3]){
               sx=dd->pos_size[2], sy=dd->pos_size[3];
            }
         }else
         if(dd->create_flags&TABEDIT_CENTER){
            x = (GetSystemMetrics(SM_CXSCREEN) - sx) / 2;
            y = (GetSystemMetrics(SM_CYSCREEN) - sy) / 2;
         }
         if(!(dd->create_flags&TABEDIT_EXACTHEIGHT))
            sx = Max(dd->table_break + dd->table_value_sx*2, sx);

                              //Setup buttons
         /*
         if(dd->def_table){
            dd->buttons = true;
            EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_DEF), true);
            if(dd->create_flags&TABEDIT_EDITDEFAULTS)
               EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_EDIT), true);
         }
         */
         //if(dd->create_flags&TABEDIT_MANUALUPDATE)
            //EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_UPDATE), dd->buttons=true);
         if(dd->create_flags&TABEDIT_IMPORT)
            EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_IMPORT), dd->buttons=true);
         if(dd->create_flags&TABEDIT_EXPORT)
            EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_EXPORT), dd->buttons=true);

         SetWindowPos(hwnd, NULL, x, y, sx, sy, SWP_NOACTIVATE | 
            (!(dd->create_flags&TABEDIT_HIDDEN) ? SWP_SHOWWINDOW : 0));
         /*
         if(dd->cb_proc)
            dd->cb_proc(dd->table, TCM_OPEN, dd->cb_user, 0, 0);
            */

                              //set focus to 1st item
         S_element_data *ed = (S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, 0, 0);
         ed->SetFocus1(dd);
      }
      return true;

   case WM_DRAWITEM:
      {
         dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
         LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
         switch(lpdis->CtlType){
         case ODT_BUTTON:
            DrawButton(lpdis);
            break;
         case ODT_LISTBOX:    //list box
            {
                              //if there are no list box items, skip this message.
               if(lpdis->itemID==-1)
                  break;

               S_element_data *ed = (S_element_data*)lpdis->itemData;
                              //Draw the bitmap and text for the list box item.
                              //Draw a rectangle around the bitmap if it is selected.
               switch(lpdis->itemAction){

               case ODA_SELECT:
               case ODA_DRAWENTIRE:
                  {
                              //draw element
                     ed->Draw(hwnd, lpdis->hDC, &lpdis->rcItem, dd, 
                        dd->active && (lpdis->itemState&ODS_SELECTED));
                  }
                  break;
               }
            }
            break;
         }
      }
      return true;

   case WM_VKEYTOITEM:
      switch(LOWORD(wParam)){
      case VK_F1:
         {
            int cs = SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETCURSEL, 0, 0);
            if(cs!=-1){
               const S_element_data *ed = (S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, cs, 0);
               if(ed->te->help_string){
                  int height = SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMHEIGHT, cs, 0);
                  dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
                  cs -= SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETTOPINDEX, 0, 0);
                  OsDisplayHelpWindow(ed->te->help_string, hwnd, height/2, height * cs + height);
               }
            }
         }
         break;
      case VK_F12:
         {
            dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
            if(dd->hwnd_main)
               SetFocus(dd->hwnd_main);
         }
         break;
      }
      return -1;

   case WM_ACTIVATE:
      {
         dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
         if(!dd)
            break;
         dd->active = (LOWORD(wParam)!=WA_INACTIVE);
         i = SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETCURSEL, 0, 0);
         if(i == -1) break;
         S_element_data *ed=(S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
         ed->InvalidateValueRect(dd, 0);
      }
      break;

   case WM_HSCROLL:
      dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
      if((HWND)lParam==dd->hwnd_slider){
         i=SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETCURSEL, 0, 0);
         if(i==-1) break;
         S_element_data *ed=(S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
         i=SendMessage(dd->hwnd_slider, TBM_GETPOS, 0, 0);
         ed->Action(dd, AF_SET_VALUE, i);
      }
      break;

   case WM_COMMAND:

      dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);

      switch(LOWORD(wParam)){
         /*
      case IDC_BUTTON_CLOSE:
         EndDialog(hwnd, true);
         DestroyWindow(dd->hwnd_dlg);
         break;
         
      case IDC_BUTTON_DEF:    //reset defaults
         switch(HIWORD(wParam)){
         case BN_CLICKED:
            i=SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0);
            if(i==-1) break;
            S_element_data *ed=(S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
            ed->Action(dd, AF_RESET);
            SetFocus(dd->hwnd_lbox);
            break;
         }
         break;

      case IDC_BUTTON_EDIT:   //edit defaults
         {
            GetWindowRect(hwnd, &rc);
            rc.right -= rc.left;
            rc.bottom -= rc.top;
            int x=10, y=10;
            MapDlgUnits(hwnd, &x, &y);
            OffsetRect(&rc, x, y);
            dd->def_table->Edit(dd->templ, hwnd, NULL, 0, TABEDIT_EDITING_DEFAULTS, NULL, (int*)&rc);
            GetClientRect(dd->hwnd_lbox, &rc);
            InvalidateRect(dd->hwnd_lbox, &rc, true);
         }
         break;

      case IDC_BUTTON_UPDATE:       //update
         switch(HIWORD(wParam)){
         case BN_CLICKED:
            if(dd->cb_proc) dd->cb_proc(dd->table, TCM_UPDATE, dd->cb_user, 0, 0);
            SetFocus(dd->hwnd_lbox);
            break;
         }
         break;
         */

      case IDC_BUTTON_IMPORT:
         switch(HIWORD(wParam)){
         case BN_CLICKED:
            i=ImportTable(dd);
            if(!i){
               DestroyAllTable(dd);
               InsertTable(dd);
               if(dd->cb_proc) dd->cb_proc(dd->table, TCM_IMPORT, dd->cb_user, 0, 0);
            }else
            if(i==-2)
               MessageBox(hwnd, "Import failed!", "Tabbler", MB_OK | MB_ICONASTERISK);
            SetFocus(dd->hwnd_lbox);
            break;
         }
         break;

      case IDC_BUTTON_EXPORT:
         switch(HIWORD(wParam)){
         case BN_CLICKED:
            if(!ExportTable(dd))
               MessageBox(hwnd, "Export failed!", "Tabbler", MB_OK | MB_ICONASTERISK);
            SetFocus(dd->hwnd_lbox);
            break;
         }
         break;

      case IDC_CHOOSE:
         switch(HIWORD(wParam)){
         case LBN_DBLCLK:
         case LBN_SELCHANGE:
            {
               i=SendMessage(dd->hwnd_lbox, LB_GETCURSEL, 0, 0);
               if(i==-1) break;
               S_element_data *ed=(S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
               if(ed)
               switch(HIWORD(wParam)){
               case LBN_DBLCLK:
                  ed->Action(dd, AF_HIT);
                  break;
               case LBN_SELCHANGE:
                  ed->Action(dd, AF_MODIFY);
                  break;
               }
            }
            break;
         }
         break;

      case IDC_EDIT:
         switch(HIWORD(wParam)){
         case EN_UPDATE:
            i = SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETCURSEL, 0, 0);
            if(i==-1)
               break;
            S_element_data *ed = (S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
            if(ed){
               ed->Action(dd, AF_MODIFY);
            }
            break;
         }
         break;

      case IDC_STUFF:
         switch(HIWORD(wParam)){
         case LBN_DBLCLK:
            {
               i=SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETCURSEL, 0, 0);
               if(i==-1) break;
               S_element_data *ed=(S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
               if(ed)
                  ed->Action(dd, AF_DBLCLICK);
            }
            break;
         case LBN_SELCHANGE:
            {
               i=SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETCURSEL, 0, 0);
               if(i==-1) break;
               S_element_data *ed = (S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
               if(ed)
                  ed->SetFocus1(dd);
            }
            break;
         }
         break;

      case IDOK:
         {
            i=SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETCURSEL, 0, 0);
            if(i==-1) break;
            S_element_data *ed=(S_element_data*)SendDlgItemMessage(hwnd, IDC_STUFF, LB_GETITEMDATA, i, 0);
            ed->Action(dd, AF_HIT);
         }
         break;

      case IDCANCEL:
         EndDialog(hwnd, true);
         DestroyWindow(hwnd);
         break;
      }
      break;

   case WM_SIZE:
      SizeWindow((S_db_data*)GetWindowLong(hwnd, GWL_USERDATA));
      break;

   case WM_GETMINMAXINFO:
      {
         dd=(S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
         if(!(dd->create_flags&TABEDIT_EXACTHEIGHT)){
            LPMINMAXINFO mmi=(LPMINMAXINFO)lParam;
            mmi->ptMinTrackSize.x = dd->table_break + dd->table_value_sx*2;
         }
      }
      break;

   case WM_CLOSE:
      EndDialog(hwnd, true);
      return 0;

   case WM_DESTROY:
      dd = (S_db_data*)GetWindowLong(hwnd, GWL_USERDATA);
      if(dd){
         DestroyAllTable(dd);
                              //remove subclassing
         SetWindowLong(dd->hwnd_lbox, GWL_WNDPROC, (dword)dd->orig_proc_lb);
         SetWindowLong(dd->hwnd_check, GWL_WNDPROC, (dword)dd->orig_proc_chk);
         SetWindowLong(dd->hwnd_edit, GWL_WNDPROC, (dword)dd->orig_proc_edn);
         SetWindowLong(dd->hwnd_slider, GWL_WNDPROC, (dword)dd->orig_proc_sld);
         SetWindowLong(dd->hwnd_choose, GWL_WNDPROC, (dword)dd->orig_proc_cmb);

         GetWindowRect(hwnd, &rc);
         rc.right -= rc.left;
         rc.bottom -= rc.top;
         if(dd->cb_proc)
            dd->cb_proc(dd->table, TCM_CLOSE, dd->cb_user, (dword)&rc, 0);
         dd->Release();
      }
      return true;
   }
   return 0;
}

//----------------------------

void *C_table::Edit(const C_table_template *templ, void *hwnd_parent,
   table_callback cb_proc, dword cb_user, dword create_flags, int *pos_size){

   HINSTANCE hinst = GetModuleHandle(DLL_FILENAME);
   if(!hinst) return NULL;

   /*
                              //compare GUIDs of table and template
   LPGUID gtmp = (LPGUID)templ->GetGUID();
   LPGUID gtab = (LPGUID)GetGUID();
   if(memcmp(gtab, gtmp, sizeof(GUID))) return NULL;
   */

   S_db_data dd;
   /*
                              //default table (if present) must be of same type
   if(def_table_name){
      PC_table def_table = CreateTable();
                              //open table by updating template - to prevent
                              //differences of older versions
      def_table->Load(templ, TABOPEN_TEMPLATE);
      def_table->Load(def_table_name, TABOPEN_FILENAME | TABOPEN_UPDATE);
      LPGUID gtab = (LPGUID)def_table->GetGUID();
      if(!memcmp(gtab, gtmp, sizeof(GUID)))
         dd.def_table = def_table;
      else
         def_table->Release();
      dd.def_table = def_table;
   }
   */

   dd.hinst = hinst;
   dd.templ = templ;
   dd.table = this;
   dd.hwnd_main = (HWND)hwnd_parent;
   dd.cb_proc = cb_proc;
   dd.cb_user = cb_user;
   dd.create_flags = create_flags;
   dd.pos_size = pos_size;

   AddRef();

   InitCommonControls();
   HWND hwnd;
   if(create_flags&TABEDIT_MODELESS){
      if(create_flags&TABEDIT_CONTROL){
         hwnd = CreateDialogParam(hinst, "DLG_PROPERTIES", NULL,
            dlgProperties, (LPARAM)&dd);

         dword ws = GetWindowLong(hwnd, GWL_STYLE);
         dword wes = GetWindowLong(hwnd, GWL_EXSTYLE);

         ws &= ~(WS_POPUP | WS_BORDER | WS_CAPTION | WS_DLGFRAME |
            WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_OVERLAPPED | WS_SYSMENU |
            WS_THICKFRAME | DS_3DLOOK);
         ws |= WS_TABSTOP | WS_CHILD;

         wes &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE |
            WS_EX_WINDOWEDGE);
         wes |= WS_EX_NOPARENTNOTIFY;

         SetWindowLong(hwnd, GWL_STYLE, ws);
         SetWindowLong(hwnd, GWL_EXSTYLE, wes);
      }else{
         hwnd = CreateDialogParam(hinst, "DLG_PROPERTIES", (HWND)hwnd_parent,
            dlgProperties, (LPARAM)&dd);
      }
   }else{
      DialogBoxParam(hinst, "DLG_PROPERTIES", (HWND)hwnd_parent, (DLGPROC)dlgProperties, (LPARAM)&dd);
      hwnd = NULL;
   }
   return hwnd;
}

//----------------------------

#if 1

void FatalError(const char *msg){

   OsMessageBox(NULL, msg, "Tabler.dll exception", MBOX_OK);
   const char *__f__ = __FILE__; int __l__ = __LINE__; __asm push msg __asm push __l__ __asm push __f__ __asm push 0x12345678
   __asm int 3
   __asm add esp, 16
}

#else
#include <iexcpt.h>

void FatalError(const char *msg){

   HINSTANCE hi = LoadLibrary("iexcpt.dll");
   if(hi){
      typedef EXCPT_RETURN_CODE __stdcall t_UserException(const char *title, const char *msg);
      t_UserException *exc = (t_UserException*)GetProcAddress(hi, "_UserException@8");
      if(exc){
         EXCPT_RETURN_CODE ir = exc("Tabler.dll exception", msg);
         if(ir==EXCPT_CLOSE)
            _exit(0);
      }
      FreeLibrary(hi);
   }
}

#endif

//----------------------------
//----------------------------

