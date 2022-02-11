#include "all.h"
#include "common.h"
#include "toolbar.h"

//----------------------------

const dword BUTTON_SIZE = 24;
const dword BITMAP_SIZE = 15;
const dword SEPARATOR_WIDTH = 8;

#ifdef _DEBUG
#define DEBUG(n) OutputDebugString(n)
#endif

//#define ENABLE_MAGNETISM      //use magneting toolbar window to edges of screen/main window when moving

//----------------------------
                              //fast fix: C_smart_ptr without defined operator&, which causes problems with list<>
template<class T>
class C_smart_ptr_1{
protected:
   T *ptr;
public:
//----------------------------
// Default constructor - initialize to NULL.
   inline C_smart_ptr_1(): ptr(0){}

//----------------------------
// Constructor from pointer to T. If non-NULL, reference is increased.
   inline C_smart_ptr_1(T *tp): ptr(tp){ if(ptr) ptr->AddRef(); }

//----------------------------
// Constructor from reference to smartpointer. If ptr non-NULL, reference is increased.
   inline C_smart_ptr_1(const C_smart_ptr_1<T> &sp): ptr(sp.ptr){ if(ptr) ptr->AddRef(); }

//----------------------------
// Destructor - releasing reference if non-NULL pointer.
   inline ~C_smart_ptr_1(){ if(ptr) ptr->Release(); }

//----------------------------
// Assignment from pointer to T. References to non-NULL pointers adjusted.
   inline C_smart_ptr_1 &operator =(T *tp){
      if(ptr!=tp){
         if(tp) tp->AddRef();
         if(ptr) ptr->Release();
         ptr = tp;
      }
      return *this;
   }

//----------------------------
// Assignment from reference to smartpointer. References to non-NULL pointers adjusted.
   inline C_smart_ptr_1 &operator =(const C_smart_ptr_1 &sp){
      if(ptr!=sp.ptr){
         if(sp.ptr) sp.ptr->AddRef();
         if(ptr) ptr->Release();
         ptr = sp.ptr;
      }
      return *this;
   }

//----------------------------
// Three pointer-access operators. Const and non-const versions.
   inline operator T *(){ return ptr; }
   inline operator const T *() const{ return ptr; }
   inline T &operator *(){ return *ptr; }
   inline const T &operator *() const{ return *ptr; }
   inline T *operator ->(){ return ptr; }
   inline const T *operator ->() const{ return ptr; }

//----------------------------
// Boolean comparison of pointer.
   inline bool operator !() const{ return (!ptr); }
   inline bool operator ==(const T *tp) const{ return (ptr==tp); }
   inline bool operator ==(const C_smart_ptr_1<T> &s) const{ return (ptr == s.ptr); }
   inline bool operator !=(const T *tp) const{ return (ptr!=tp); }
   inline bool operator !=(const C_smart_ptr_1<T> &s) const{ return (ptr != s.ptr); }

//----------------------------
// Comparison of pointers.
   inline bool operator <(const C_smart_ptr_1<T> &s) const{ return (ptr < s.ptr); }
};

//----------------------------

class C_toolbar_imp: public C_toolbar_special{
   C_str name;
   PC_editor ed;

   HBRUSH hbr_background, hbr_background_press;
   HWND hwnd;
   HWND hwnd_tooltip;
   //C_smart_ptr<C_editor_item> e_usability;
   bool in_action;            //true while in action of some plugin (to avoid reentrance)

   class C_bitmap: public C_unknown{
   public:
      HBITMAP hbm;
      HDC hdc;

      C_bitmap(HBITMAP hbm1, HWND hwnd_dest):
         hbm(hbm1)
      {
         HDC hdc_dst = GetDC(hwnd_dest);
         assert(hdc_dst);
         hdc = CreateCompatibleDC(hdc_dst);
         assert(hdc);
         bool b;
         b = SelectObject(hdc, hbm);
         assert(b);
         b = ReleaseDC(hwnd_dest, hdc_dst);
         assert(b);
      }
      ~C_bitmap(){
         bool b;
         if(hdc){
            b = DeleteDC(hdc);
            assert(b);
         }
         if(hbm){
            b = DeleteObject(hbm);
            assert(b);
         }
      }
   };

   class C_button: public C_unknown{
   public:
      HWND hwnd;
      C_smart_ptr<C_bitmap> bitmap;
      PC_editor_item owner;   //plugin owning this button
      dword action_id;        //action ID being sent when button is pressed
      dword rel_pos;
      C_str tooltip;

      int img_index;          //-1 = separator
      bool pressed;
      bool enabled;

      C_button(PC_editor_item ei, C_bitmap *bm, int ii, HWND hwnd_dlg, dword id1, const C_str &tip):
         owner(ei),
         action_id(id1),
         tooltip(tip),
         bitmap(bm),
         img_index(ii),
         pressed(false),
         enabled(true)
      {
         bool sep = IsSeparator();
         dword wsextyle = WS_EX_NOPARENTNOTIFY;
         dword wstyle = WS_CHILD | WS_VISIBLE;
         const char *cls_name;
         dword sz_x, sz_y;
         if(!sep){
            wstyle |= BS_OWNERDRAW;
            wsextyle |= WS_EX_STATICEDGE;
            cls_name = "BUTTON";
            sz_x = BUTTON_SIZE;
            sz_y = BUTTON_SIZE;
         }else{
            wstyle |= SS_ETCHEDFRAME;
            cls_name = "STATIC";
            sz_x = SEPARATOR_WIDTH - 4;
            sz_y = BUTTON_SIZE*2/3;
         }
         hwnd = CreateWindowEx(wsextyle,
            cls_name,
            NULL,
            wstyle,
            0, 0,
            sz_x, sz_y,
            hwnd_dlg,
            NULL,
            NULL,
            NULL);
         assert(hwnd);
         SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)this);
      }
      ~C_button(){
         bool b;
         b = DestroyWindow(hwnd);
         assert(b);
      }

   //----------------------------

      inline bool IsSeparator() const{ return (img_index==-1); }

   //----------------------------

      inline dword GetWidth() const{
         return IsSeparator() ? SEPARATOR_WIDTH : BUTTON_SIZE;
      }

   //----------------------------

      void SetPos(dword x, dword y){

         if(IsSeparator()){
            x += 2;
            y += BUTTON_SIZE/6;;
         }
         SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
      }
   };
   typedef C_button *PC_button;
   typedef const C_button *CPC_button;

   typedef list<C_smart_ptr_1<C_button> > t_buttons;
   t_buttons buttons;

//----------------------------

   BOOL DlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)(const char*)name);
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL:
            SetActiveWindow((HWND)ed->GetIGraph()->GetHWND());
            break;
         default:
            switch(HIWORD(wParam)){
            case BN_CLICKED:
            case BN_DBLCLK:
               {
                  if(in_action)
                     break;
                  in_action = true;
                  HWND hwnd_but = (HWND)lParam;
                  PC_button but = (PC_button)GetWindowLong(hwnd_but, GWL_USERDATA);
                  assert(but);
                  but->owner->Action(but->action_id, (void*)but->pressed);
#if 0
                              //count statistics
                  if(!e_usability)
                     e_usability = ed->FindPlugin("Usability");
                  if(e_usability)
                     e_usability->Action(E_USABILITY_TOOLBAR, (void*)(const char*)but->tooltip);
#endif
                  in_action = false;
               }
               break;
            }
         }
         break;

      case WM_DRAWITEM:
         {
            bool b;
            DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lParam;
            HWND hwnd_but = di->hwndItem;
            PC_button but = (PC_button)GetWindowLong(hwnd_but, GWL_USERDATA);
            assert(but);
            HDC hdc = di->hDC;
            RECT &rc = di->rcItem;
            bool selected = (di->itemState&ODS_SELECTED);
            int bitmap_down = selected;
            bool pushed = but->pressed;
            if(!bitmap_down)
               bitmap_down = (int)pushed;
                              //while in-action, simulate nothing is pressed
            if(in_action){
               bitmap_down = 0;
               selected = false;
            }

            FillRect(hdc, &rc, pushed ? hbr_background_press : hbr_background);
            {
               bool draw_pressed = selected;
               if(!draw_pressed)
                  draw_pressed = pushed;
               dword color_hi = COLOR_3DHIGHLIGHT, color_low = COLOR_3DDKSHADOW;
               HPEN pen;
               {
                  LOGBRUSH lb = {BS_SOLID, GetSysColor(draw_pressed ? color_low : color_hi), 0 };
                  pen = ExtCreatePen(PS_SOLID, 1, &lb, 0, NULL);
               }
               HPEN old_pen = (HPEN)SelectObject(hdc, pen);

               {
                              //paint left-top shadow
                  POINT pt[3] = {
                     {rc.left, rc.bottom-2},
                     {rc.left, rc.top},
                     {rc.right-1, rc.top},
                  };
                  Polyline(hdc, pt, 3);
               }

               if(!draw_pressed){
                              //paint right-bottom shadow
                  DeleteObject(pen);
                  LOGBRUSH lb = {BS_SOLID, GetSysColor(COLOR_3DSHADOW), 0 };
                  pen = ExtCreatePen(PS_SOLID, 1, &lb, 0, NULL);
                  SelectObject(hdc, pen);
                  POINT pt[3] = {
                     {rc.left+1, rc.bottom-2},
                     {rc.right-2, rc.bottom-2},
                     {rc.right-2, rc.top},
                  };
                  Polyline(hdc, pt, 3);
               }

               {
                  DeleteObject(pen);
                  LOGBRUSH lb = {BS_SOLID, GetSysColor(draw_pressed ? color_hi : color_low), 0 };
                  pen = ExtCreatePen(PS_SOLID, 1, &lb, 0, NULL);
                  SelectObject(hdc, pen);
                  POINT pt[3] = {
                     {rc.right-1, rc.top},
                     {rc.right-1, rc.bottom-1},
                     {rc.left-1, rc.bottom-1},
                  };
                  Polyline(hdc, pt, 3);
               }

               SelectObject(hdc, old_pen);
               DeleteObject(pen);
            }

            const dword OFFSET = (BUTTON_SIZE - BITMAP_SIZE) / 2 - 1;
            b = TransparentBlt(hdc, OFFSET+bitmap_down, OFFSET+bitmap_down, BITMAP_SIZE, BITMAP_SIZE,
               but->bitmap->hdc, BITMAP_SIZE*but->img_index, but->enabled ? 0 : BITMAP_SIZE,
               BITMAP_SIZE, BITMAP_SIZE,
               0xff00ff);
            assert(b);
         }
         return true;

      case WM_CLOSE:
         {
            ed->FindPlugin("ToolbarMgr")->Action(0, this);
         }
         break;

      case WM_SIZING:
         {
            RECT &rc = *(RECT*)lParam;
            POINT pt_delta;
            {
                              //get delta of window relative to client area
               RECT rc_delta = {0, 0, 0, 0};
               AdjustWindowRectEx(&rc_delta, GetWindowLong(hwnd, GWL_STYLE), false, GetWindowLong(hwnd, GWL_EXSTYLE));
               pt_delta.x = (rc_delta.right - rc_delta.left);
               pt_delta.y = (rc_delta.bottom - rc_delta.top);
            }

            dword sx = rc.right - rc.left - pt_delta.x;
            dword sy = rc.bottom - rc.top - pt_delta.y;
            //DEBUG(C_fstr("%i %i\n", sx, sy));
            ComputeClosestSize(sx, sy);
            sx += pt_delta.x;
            sy += pt_delta.y;

            switch(wParam){
            case WMSZ_TOP: case WMSZ_TOPLEFT: case WMSZ_TOPRIGHT:
               rc.top = rc.bottom - sy;
               break;
            case WMSZ_LEFT: case WMSZ_RIGHT: case WMSZ_BOTTOM: case WMSZ_BOTTOMLEFT: case WMSZ_BOTTOMRIGHT:
               rc.bottom = rc.top + sy;
               break;
            }
            switch(wParam){
            case WMSZ_BOTTOMLEFT: case WMSZ_LEFT: case WMSZ_TOPLEFT:
               rc.left = rc.right - sx;
               break;
            case WMSZ_BOTTOM: case WMSZ_TOP: case WMSZ_BOTTOMRIGHT: case WMSZ_RIGHT: case WMSZ_TOPRIGHT:
               rc.right = rc.left + sx;
               break;
            }
            return true;
         }
         break;

      case WM_SIZE:
         ArrangeButtons(LOWORD(lParam), HIWORD(lParam));
         break;

#ifdef ENABLE_MAGNETISM
      case WM_MOVING:
         {
            if(GetAsyncKeyState(VK_MENU)&0x8000)
               break;
            RECT &rc = *(RECT*)lParam;
            enum{
               LEFT, TOP, RIGHT, BOTTOM
            };
            dword sx = GetSystemMetrics(SM_CXSCREEN);
            dword sy = GetSystemMetrics(SM_CYSCREEN);
                              //collect 'glue' offsets for 4 sides
            C_vector<int> glue_offsets[4];
            glue_offsets[LEFT].push_back(0);
            glue_offsets[TOP].push_back(0);
            glue_offsets[RIGHT].push_back(sx);
            glue_offsets[BOTTOM].push_back(sy);
            {
               HWND hwnd_main = (HWND)ed->GetIGraph()->GetHWND();
               RECT rc_main, rc_client;
               GetWindowRect(hwnd_main, &rc_main);
               GetClientRect(hwnd_main, &rc_client);
               ClientToScreen(hwnd_main, (LPPOINT)&rc_client);
               rc_client.right += rc_client.left;
               rc_client.bottom += rc_client.top;
               {
                              //subtract height of status bar
                  HWND hwnd_sb = (HWND)ed->GetStatusBarHWND();
                  if(hwnd_sb){
                     RECT rc;
                     GetWindowRect(hwnd_sb, &rc);
                     rc_client.bottom -= rc.bottom - rc.top;
                  }
               }
                              //glue to outer corners of the IGraph window
               if(rc.bottom>rc_main.top && rc.top<rc_main.bottom){
                  glue_offsets[LEFT].push_back(rc_main.right);
                  glue_offsets[RIGHT].push_back(rc_main.left);
               }
               if(rc.right>rc_main.left && rc.left<rc_main.right){
                  glue_offsets[TOP].push_back(rc_main.bottom);
                  glue_offsets[BOTTOM].push_back(rc_main.top);
               }
                              //glue to client area of the IGraph window
               if(rc.bottom>rc_client.top && rc.top<rc_client.bottom &&
                  rc.right>rc_client.left && rc.left<rc_client.right){

                  glue_offsets[LEFT].push_back(rc_client.left);
                  glue_offsets[RIGHT].push_back(rc_client.right);
                  glue_offsets[TOP].push_back(rc_client.top);
                  glue_offsets[BOTTOM].push_back(rc_client.bottom);
               }
            }

                              //try to 'glue' the closest edge
            const int GLUE_DIST = 6;
            int best_delta[2] = {GLUE_DIST, GLUE_DIST};
            int offset[2] = {0, 0};
            for(dword i=4; i--; ){
               int pos = (i==LEFT) ? rc.left : (i==TOP) ? rc.top : (i==RIGHT) ? rc.right : rc.bottom;
               int &best_axis_delta = best_delta[i&1];
               for(int j=glue_offsets[i].size(); j--; ){
                  int delta = glue_offsets[i][j] - pos;
                  int abs_delta = abs(delta);
                  if(best_axis_delta > abs_delta){
                     best_axis_delta = abs_delta;
                     offset[i&1] = delta;
                  }
               }
            }
            //DEBUG(C_fstr("%i %i\n", offset[0], offset[1]));
            rc.left += offset[0];
            rc.right += offset[0];
            rc.top += offset[1];
            rc.bottom += offset[1];
            return true;
         }
         break;
#endif//ENABLE_MAGNETISM
      }
      return 0;
   }

//----------------------------

   static BOOL CALLBACK DlgProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      C_toolbar_imp *tb = (C_toolbar_imp*)GetWindowLong(hwnd, GWL_USERDATA);
      if(tb)
         return tb->DlgProc(hwnd, uMsg, wParam, lParam);
      return 0;
   }

//----------------------------

   HBITMAP CreateBitmapFromResource(const char *bitmap_name, HINSTANCE hi, dword *sx = NULL, dword *sy = NULL) const{
   
      HBITMAP hbm = LoadBitmap(hi, bitmap_name);
      if(hbm){
         SIZE sz;
         GetBitmapDimensionEx(hbm, &sz);
         if(sx) *sx = sz.cx;
         if(sy) *sy = sz.cy;
      }
      return hbm;
   }

//----------------------------

   HBITMAP CreateBitmap(const char *bitmap_filename, dword *sx = NULL, dword *sy = NULL) const{

      HBITMAP hbm = NULL;
      PC_dta_stream dta = DtaCreateStream(bitmap_filename);
      if(dta){
                        //read bitmap into memory
         int sz = dta->GetSize();
         byte *buf = new byte[sz];
         dta->Read(buf, sz);
         dta->Release();

         dword data_offs = *(dword*)(buf+10);
                           //create Win32 bitmap
         BITMAPINFOHEADER *bih = (BITMAPINFOHEADER*)(buf+0xe);
         assert(bih->biBitCount==8);
         dword szx = bih->biWidth;
         dword szy = bih->biHeight;

         HDC hdc = GetDC(hwnd);
         hbm = CreateDIBitmap(hdc, bih, CBM_INIT, buf + data_offs, (BITMAPINFO*)bih, DIB_RGB_COLORS);

         if(sx)
            *sx = szx;
         if(sy)
            *sy = szy;

         ReleaseDC(hwnd, hdc);
         delete[] buf;
      }else{
         if(sx) *sx = 0;
         if(sy) *sy = 0;
      }
      return hbm;
   }

//----------------------------

   void ArrangeButtons(dword sx, dword sy){

      dword x = 0, y = 0;

      for(t_buttons::iterator it = buttons.begin(); it!=buttons.end(); it++){
         PC_button but = *it;
         dword bw = but->GetWidth();
         if(x+bw > sx){
            x = 0;
            y += BUTTON_SIZE;
         }
         but->SetPos(x, y);
         x += bw;
      }
      assert(y+BUTTON_SIZE == sy);
   }

//----------------------------

   void ComputeClosestSize(dword &sx, dword &sy) const{

      t_buttons::const_iterator it;

      dword width = 0;
                              //compute width of all buttons
      for(it = buttons.begin(); it!=buttons.end(); it++){
         width += (*it)->GetWidth();
      }
      const dword num_buttons = buttons.size();

      dword best_area_delta = 10000000;
      const dword orig_sx = sx;
      const dword orig_sy = sy;
                              //try all combinations of numbers of lines
      for(dword y=num_buttons; y-- > 1; ){
         const dword avg_width = Max((dword)BUTTON_SIZE, (width+BUTTON_SIZE) / y);
                              
         dword max_width = 0;
         it = buttons.begin();
                              //add buttons in lines
         for(dword ly=0; it!=buttons.end(); ly++){
            dword curr_width = 0;
            while(it!=buttons.end()){
               CPC_button but = *it;
               dword bw = but->GetWidth();
               if(curr_width+bw > avg_width)
                  break;
               curr_width += bw;
               it++;
            }
            max_width = Max(max_width, curr_width);
         }
         dword delta = abs((int)orig_sx - (int)max_width) + abs((int)orig_sy - int(ly * BUTTON_SIZE));
         if(best_area_delta > delta){
            best_area_delta = delta;
            sx = max_width;
            sy = ly * BUTTON_SIZE;
         }
      }
      //DEBUG(C_fstr("%i %i\n", sx, sy));
   }

//----------------------------

   void SetWindowSizeByClientSize(dword sx, dword sy){

      RECT rc = {0, 0, sx, sy};
      AdjustWindowRectEx(&rc, GetWindowLong(hwnd, GWL_STYLE), false, GetWindowLong(hwnd, GWL_EXSTYLE));
      dword wsx = rc.right - rc.left;
      dword wsy = rc.bottom - rc.top;
      SetWindowPos(hwnd, NULL, 0, 0, wsx, wsy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
      ArrangeButtons(sx, sy);
   }

//----------------------------

public:
   C_toolbar_imp(PC_editor ed1, const C_str &n):
      ed(ed1),
      name(n),
      hwnd(NULL),
      in_action(false),
      hbr_background(NULL),
      hbr_background_press(NULL),
      hwnd_tooltip(NULL)
   {}
   ~C_toolbar_imp(){

      ShowWindow(hwnd, SW_HIDE);
      buttons.clear();
      bool b;
      if(hwnd_tooltip){
         b = DestroyWindow(hwnd_tooltip);
         assert(b);
      }
      if(hwnd && ed){
         PIGraph igraph = ed->GetIGraph();
         igraph->RemoveDlgHWND(hwnd);
         b = DestroyWindow(hwnd);
         assert(b);
      }
      if(hbr_background){
         b = DeleteObject(hbr_background);
         assert(b);
      }
      if(hbr_background_press){
         b = DeleteObject(hbr_background_press);
         assert(b);
      }
   }
   bool Init(){

      hbr_background = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
      hbr_background_press = CreateSolidBrush(GetSysColor(COLOR_BTNSHADOW));

      PIGraph igraph = ed->GetIGraph();
      hwnd = CreateDialogParam(GetHInstance(), "IDD_TOOLBAR", (HWND)igraph->GetHWND(), DlgProcThunk, (LPARAM)this);
      if(!hwnd)
         return false;

      hwnd_tooltip = CreateWindowEx(0,
         TOOLTIPS_CLASS,
         NULL,
         0,
         0, 0,
         10, 10,
         0,
         NULL,
         NULL,
         NULL);
      SendMessage(hwnd_tooltip, TTM_ACTIVATE, true, 0);

      igraph->AddDlgHWND(hwnd);
      return true;
   }

   virtual const C_str &GetName() const{ return name; }

//----------------------------

   virtual void Resize(dword sx, dword sy){

      ComputeClosestSize(sx, sy);
      SetWindowSizeByClientSize(sx, sy);
   }

//----------------------------

   virtual void *GetHWND(){ return hwnd; }

//----------------------------

   virtual bool AddButtons(PC_editor_item owner, const S_toolbar_button *buts, dword num,
      const char *bitmap_name, void *hinstance, dword rel_pos){

      dword x, y;
      HBITMAP hbm = hinstance ? CreateBitmapFromResource(bitmap_name, (HINSTANCE)hinstance, &x, &y) :
         CreateBitmap(bitmap_name, &x, &y);
      if(!hbm)
         return false;

      C_bitmap *bm = new C_bitmap(hbm, hwnd);

      POINT curr_size = {0, 0};
      if(buttons.size()){
         RECT rc;
         GetClientRect(hwnd, &rc);
         curr_size.x = rc.right;
         curr_size.y = rc.bottom;
      }

      dword total_width = 0;

      t_buttons::iterator insert_pos = buttons.end();
                              //find where we're going insert at
      while(insert_pos!=buttons.begin()){
         t_buttons::iterator it_prev = insert_pos;
         --it_prev;
         CPC_button pb = *it_prev;
         if(pb->rel_pos < rel_pos)
            break;
         insert_pos = it_prev; 
      }
      for(dword i=0; i<num; i++){
         const S_toolbar_button &bi = buts[i];
         PC_button bt = new C_button(owner, bm, bi.img_index, hwnd, bi.action_id, bi.tooltip);
         insert_pos = buttons.insert(insert_pos, bt);
         insert_pos++;
         bt->rel_pos = rel_pos;

                              //install tooltip text
         if(bi.tooltip){
            TOOLINFO ti;
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_CENTERTIP | TTF_IDISHWND;
            ti.uFlags |= TTF_SUBCLASS;
            ti.hwnd = hwnd;
            ti.uId = (dword)bt->hwnd;
            ti.lpszText = (char*)bi.tooltip;
            SendMessage(hwnd_tooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
         }
         total_width += bt->GetWidth();

         bt->Release();
      }
      bm->Release();

      SetWindowSizeByClientSize(curr_size.x + total_width, BUTTON_SIZE);

      return true;
   }

//----------------------------

   PC_button FindButton(PC_editor_item owner, dword action_id){

      for(t_buttons::iterator it=buttons.begin(); it!=buttons.end(); it++){
         PC_button but = *it;
         if(but->owner==owner && but->action_id==action_id)
            return but;
      }
      return NULL;
   }

//----------------------------

   virtual void SetButtonPressed(PC_editor_item owner, dword id, bool pressed){

      PC_button but = FindButton(owner, id);
      if(!but)
         return;
      if(but->pressed!=pressed){
         but->pressed = pressed;
         InvalidateRect(but->hwnd, NULL, true);
      }
   }

//----------------------------

   virtual void SetButtonTooltip(PC_editor_item owner, dword id, const char *tooltip){

      PC_button but = FindButton(owner, id);
      if(!but)
         return;
      TOOLINFO ti;
      ti.cbSize = sizeof(ti);
      ti.uFlags = TTF_CENTERTIP | TTF_IDISHWND;
      ti.uFlags |= TTF_SUBCLASS;
      ti.hwnd = hwnd;
      ti.uId = (dword)but->hwnd;
      ti.lpszText = (char*)tooltip;
      if(tooltip)
         SendMessage(hwnd_tooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
      else
         SendMessage(hwnd_tooltip, TTM_DELTOOL, 0, (LPARAM)&ti);
   }

//----------------------------

   virtual void EnableButton(PC_editor_item owner, dword id, bool enabled){

      PC_button but = FindButton(owner, id);
      if(!but)
         return;
      if(but->enabled!=enabled){
         but->enabled = enabled;
         EnableWindow(but->hwnd, enabled);
         InvalidateRect(but->hwnd, NULL, true);
      }
   }

//----------------------------

   virtual void SetNumLines(dword num_lines){

      dword sx = 0;
      dword sy = BUTTON_SIZE * num_lines;
      for(t_buttons::const_iterator it = buttons.begin(); it!=buttons.end(); it++)
         sx += (*it)->GetWidth();

      sx /= num_lines;
      ComputeClosestSize(sx, sy);
      SetWindowSizeByClientSize(sx, sy);
   }

};

//----------------------------

C_toolbar_special *CreateToolbar(PC_editor ed, const C_str &name){

   C_toolbar_imp *tb = new C_toolbar_imp(ed, name);
   if(!tb->Init()){
      tb->Release();
      tb = NULL;
   }
   return tb;
}

//----------------------------
