#include "all.h"
#include "common.h"
#include "toolbar.h"
#include <insanity\3DTexts.h>
#include "Usability.h"

//----------------------------

//#define FULL_ACCESS           //don't make access checking
//#define LIMITED_FUNCTIONALITY //allow only basic functions for viewing scene

//----------------------------

#define DEBUG_NO_LEAK_WARNING //no warning about unreleased memory

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

C_toolbar_special *CreateToolbar(PC_editor, const C_str &name);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, dword fdwReason, void *lpvReserved){

   switch(fdwReason){
   case DLL_THREAD_ATTACH:
      break;

   case DLL_THREAD_DETACH:
      break;

   case DLL_PROCESS_ATTACH:
      {
                              //this func switches to MSVC mem allocation, instead of system allocation
         _set_sbh_threshold(1016);
#if defined _MSC_VER && defined _DEBUe && !defined DEBUG_NO_LEAK_WARNING
                              //report memory leaks at exit
         _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);

                              //report mode
         _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_WNDW);
         _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
         _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
         struct S_hlp{
            static int __cdecl CrtDumpClient(int report_type, char *msg, int *ret_val){

               static bool called;
               if(!called){
                  _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & ~_CRTDBG_LEAK_CHECK_DF);
                  called = true;
               }
               if(report_type==_CRT_WARN){
                                          //display message that some errors occured
                  int i = MessageBox(NULL, msg, "IEditor", MB_ABORTRETRYIGNORE);
                  switch(i){
                  case IDABORT:
                     abort();
                     break;
                  case IDRETRY:
                     *ret_val = 1;
                     break;
                  default:
                     *ret_val = 0;
                  }
               }
               return true;
            }
         };
         _CrtSetReportHook(S_hlp::CrtDumpClient);
         //_CrtSetBreakAlloc(50);
#endif
      }
      break;

   case DLL_PROCESS_DETACH:
      break;
   }
   return true;
}

//----------------------------

/*
class I3D_driver_internal: public I3D_driver{
public:
   I3DMETHOD_(void,Dummy)() = 0;
   I3DMETHOD(GetVersion)(dword&,dword&) = 0;
};
*/

//----------------------------

static bool GetFileName(PIGraph igraph, const char *title, const char *filter,
   const char *def_ext, char *buf1, dword bufsize, const char *init_dir){

   //igraph->FlipToGDI();
   OPENFILENAME on;
   memset(&on, 0, sizeof(on));
   on.lStructSize = sizeof(on);
   on.hwndOwner = (HWND)igraph->GetHWND();
   on.lpstrFilter = filter;
   on.nFilterIndex = 1;
   on.lpstrFile = buf1;
   buf1[0] = 0;
   on.nMaxFile = bufsize;
   on.lpstrInitialDir = init_dir;
   on.lpstrTitle = title;
   on.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
   on.lpstrDefExt = def_ext;
   return (GetOpenFileName(&on)!=0);
}

//----------------------------
//----------------------------
                              //editor implementation
class C_editor_imp: public C_editor{
   dword ref;
   struct S_edit_render_item{
      PC_editor_item itm;
      int order;
   };
   C_smart_ptr<C_editor_item_Console> e_console;

   struct S_plugin_menu_item{
                              //keyboard shortcut used to invoke command
      IG_KEY key;
      byte key_modifier;
      int id;                 //parameter passed to Action fnc
                              //menu access
      HMENU hmenu_parent;
      int menu_id;            //unique menu item identifier, 0 if menu item not created
      bool enabled;
   };

   typedef map<C_smart_ptr<C_editor_item>, C_vector<S_plugin_menu_item> > t_plugin_list;
   t_plugin_list plugin_list;
   //C_vector<C_smart_ptr<C_editor_item> > plugin_list;   //installed plugins
   //list<S_edit_render_item> render_list;        //plugins registered for rendering

   typedef map<C_editor_item*, C_buffer<S_tmp_shortcut> > t_tmp_shortcuts;
   t_tmp_shortcuts tmp_shortcuts;

   C_smart_ptr<I3D_material> axis_icons[3];
   
   struct S_status_bar_msg{
      C_str str;
      int pos;
      bool modified;
      S_status_bar_msg():
         modified(false)
      {
      }
   };
   C_vector<S_status_bar_msg> status_bar_strings;

   PI3D_driver driver;
   C_smart_ptr<I3D_scene> scene;
   C_smart_ptr<C_poly_text> texts;         //polygon font system
public:
   void AfterLoad(){
      Message("", sb_modify);
   }
private:
                              //status bar - it's derived from S_menu_item to be notified about screen changes
   class C_edit_StatusBar: public C_editor_item{
      virtual const char *GetName() const;

      virtual void AfterLoad(){
                              //clear all fields
         ((C_editor_imp*)ed)->AfterLoad();
         //SendMessage(hwnd_sb, SB_SETTEXT, sb_modify, (LPARAM)"");
      }

      virtual void OnDeviceReset(){
         WinInit();
      }
   public:
      HWND hwnd_sb;
      void WinInit();
      virtual bool Init(){ return true; }
   } edit_StatusBar;

//----------------------------

   virtual void *GetStatusBarHWND(){ return edit_StatusBar.hwnd_sb; }

//----------------------------

   class C_edit_ToolbarMgr: public C_editor_item{
      virtual const char *GetName() const{ return "ToolbarMgr"; }
      enum E_CHUNK{
         CT_BASE = 0x1001,
            //string toolbar_name
            CT_POS = 0x1010,
               //int x, y
            CT_SIZE = 0x1020,
               //int sx, sy
            CT_ON = 0x1030,
               //bool
            CT_NO_CREATE_COUNT = 0x1040,
               //byte
      };
                              //counter, after which 'dead' toolbar info is forgotten
      enum{ MAX_NOCREATE_COUNT = 30 };

      typedef list<C_smart_ptr_1<C_toolbar_special> > t_toolbars;
      t_toolbars toolbars;

                              //toolbar info - saved info about any toolbar ever created
      struct S_toolbar_info{
         bool on;
                              //position and size of toolbar
         POINT wnd_pos;
         POINT wnd_size;
         dword action_id;     //action ID for passing into Action method (0 = unassigned)
         dword height;
         dword no_create_count;  //counter how many session this toolbar haven't been created

         enum{
            DIRTY_ON = 1,
            DIRTY_POS = 2,
            DIRTY_SIZE = 4,
            DIRTY_HEIGHT = 8,
            DIRTY_ALL = 7,
         };
         dword dirty_flags;   //conbination of DIRTY_??? flags
         S_toolbar_info():
            on(true),
            action_id(0),
            no_create_count(0),
            height(0),
            dirty_flags(DIRTY_ON | DIRTY_POS | DIRTY_SIZE)
         {}
      };
      typedef map<C_str, S_toolbar_info> t_toolbar_info;
      mutable t_toolbar_info toolbar_info;

                              //temporary unti-Tick valid information
      typedef set<C_smart_ptr_1<C_toolbar_special> > t_dirty_toolbars;
      t_dirty_toolbars dirty_toolbars;

//----------------------------

      dword GetFreeActionID() const{

         C_vector<dword> ids;
         for(t_toolbar_info::const_iterator it = toolbar_info.begin(); it!=toolbar_info.end(); it++){
            const S_toolbar_info &ti = it->second;
            if(ti.action_id)
               ids.push_back(ti.action_id);
         }
         if(!ids.size())
            return 1;
         sort(ids.begin(), ids.end());
         for(dword i=0; i<ids.size()-1; i++){
            dword proposed = ids[i] + 1;
            if(proposed != ids[i+1])
               return proposed;
         }
         return ids.back() + 1;
      }
   public:
   
   //----------------------------
      C_edit_ToolbarMgr()
      {}

      virtual bool Init(){ return true; }

      virtual void Close(){
         dirty_toolbars.clear();
         toolbars.clear();
      }

   //----------------------------

      virtual dword Action(int id, void *context = NULL){

                              //check which toolbar is going on
         switch(id){

         case 0:              //special action called by toolbar class - disabling toolbar
            {
               C_toolbar_special *tb = (C_toolbar_special*)context;
               S_toolbar_info &ti = toolbar_info[tb->GetName()];
               ti.on = !ti.on;
               ti.dirty_flags = S_toolbar_info::DIRTY_ON;
               dirty_toolbars.insert(tb);
            }
            break;

         default:
                              //search if it's one of ours ids
            for(t_toolbar_info::iterator it = toolbar_info.begin(); it!=toolbar_info.end(); it++){
               S_toolbar_info &ti = it->second;
               if(ti.action_id==(dword)id && ti.action_id==(dword)id){
                              //toggle its 'on' state
                  ti.on = !ti.on;
                  ti.dirty_flags = S_toolbar_info::DIRTY_ON;
                                 //find the toolbar and add among dirties
                  for(t_toolbars::iterator it1=toolbars.begin(); it1!=toolbars.end(); it1++){
                     C_toolbar_special *tb = *it1;
                     if(tb->GetName()==it->first){
                        dirty_toolbars.insert(tb);
                        break;
                     }
                  }
                  break;
               }
            }
         }
         return 0;
      }

   //----------------------------

      virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

         for(t_dirty_toolbars::iterator it = dirty_toolbars.begin(); it!=dirty_toolbars.end(); it++){
            C_toolbar_special *tb = *it;
            S_toolbar_info &ti = toolbar_info[tb->GetName()];
            HWND hwnd_tb = (HWND)tb->GetHWND();
            dword df = ti.dirty_flags;
            assert(df);
            ti.dirty_flags = 0;
            if(df&S_toolbar_info::DIRTY_POS){
                              //adjust pos
               SetWindowPos(hwnd_tb, NULL, ti.wnd_pos.x, ti.wnd_pos.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            }
            if(df&S_toolbar_info::DIRTY_SIZE){
                              //adjust pos
               tb->Resize(ti.wnd_size.x, ti.wnd_size.y);
            }
            if(df&S_toolbar_info::DIRTY_ON){
               ShowWindow(hwnd_tb, ti.on ? SW_SHOWNOACTIVATE : SW_HIDE);
               ed->CheckMenu(this, ti.action_id, ti.on);
            }
            if(df&S_toolbar_info::DIRTY_HEIGHT){
               tb->SetNumLines(ti.height);
            }
         }
         dirty_toolbars.clear();
      }

   //----------------------------

      virtual bool LoadState(C_chunk &ck){

         while(ck){
            switch(++ck){
            case CT_BASE:
               {
                  C_str tb_name = ck.ReadString();
                  S_toolbar_info &ti = toolbar_info[tb_name];
                  while(ck){
                     switch(++ck){
                     case CT_POS:
                        ck.Read(&ti.wnd_pos, sizeof(ti.wnd_pos));
                        --ck;
                        ti.dirty_flags |= S_toolbar_info::DIRTY_POS;
                        break;
                     case CT_SIZE:
                        ck.Read(&ti.wnd_size, sizeof(ti.wnd_size));
                        --ck;
                        ti.dirty_flags |= S_toolbar_info::DIRTY_SIZE;
                        ti.dirty_flags &= ~S_toolbar_info::DIRTY_HEIGHT;
                        break;
                     case CT_ON:
                        ti.on = ck.RBoolChunk();
                        ti.dirty_flags |= S_toolbar_info::DIRTY_ON;
                        break;
                     case CT_NO_CREATE_COUNT:
                        ti.no_create_count = ck.RByteChunk();
                              //inc now, so that we detect unused info
                        ++ti.no_create_count;
                        break;
                     default:
                        --ck;
                     }
                  }
               }
               break;
            }
            --ck;
         }
                              //forget no-create counters for all 'living' toolbars
         for(t_toolbars::iterator it = toolbars.begin(); it!=toolbars.end(); it++){
            PC_toolbar tb = *it;
            toolbar_info[tb->GetName()].no_create_count = 0;
         }
         return true;
      }

   //----------------------------

      virtual bool SaveState(C_chunk &ck) const{

                              //save info about opened toolbars
         {
            for(t_toolbars::const_iterator it = toolbars.begin(); it!=toolbars.end(); it++){
               const C_toolbar_special *tb = *it;
               HWND hwnd_tb = (HWND)const_cast<C_toolbar_special*>(tb)->GetHWND();
               RECT rc;
               GetWindowRect(hwnd_tb, &rc);

               S_toolbar_info &ti = toolbar_info[tb->GetName()];
               ti.wnd_pos = (POINT&)rc;

               GetClientRect(hwnd_tb, &rc);
               ti.wnd_size.x = rc.right;
               ti.wnd_size.y = rc.bottom;
            }
         }
                              //save info about known toolbars
         for(t_toolbar_info::const_iterator it = toolbar_info.begin(); it!=toolbar_info.end(); it++){
            const C_str &name = it->first;
            const S_toolbar_info &ti = it->second;
                              //don't save info, if the toolbar haven't been created for ages
            if(ti.no_create_count >= MAX_NOCREATE_COUNT)
               continue;
            ck <<= CT_BASE;
            {
               ck.Write((const char*)name, name.Size()+1);
               ck <<= CT_POS;
               {
                  ck.Write(&ti.wnd_pos, sizeof(ti.wnd_pos));
               }
               --ck;

               ck <<= CT_SIZE;
               {
                  ck.Write(&ti.wnd_size, sizeof(ti.wnd_size));
               }
               --ck;
               ck.WBoolChunk(CT_ON, ti.on);
               ck.WByteChunk(CT_NO_CREATE_COUNT, (byte)ti.no_create_count);
            }
            --ck;
         }
         return true;
      }

   //----------------------------

      virtual PC_toolbar GetToolbar(const C_str &name, int suggest_x, int suggest_y, dword suggested_height){

                                 //check if this already exists
         for(t_toolbars::iterator it=toolbars.begin(); it!=toolbars.end(); it++){
            C_toolbar *tb = *it;
            if(tb->GetName()==name){
               return tb;
            }
         }
                                 //create the toolbar now
         C_toolbar_special *tb = CreateToolbar(ed, name);
         toolbars.push_back(tb);
         tb->Release();

         S_toolbar_info *ti;
         {
            t_toolbar_info::iterator it = toolbar_info.find(name);
            if(it==toolbar_info.end()){
                                    //first-time created toolbar, init its basic info
               ti = &toolbar_info[name];
               ti->on = true;
               ti->wnd_pos.x = suggest_x;
               ti->wnd_pos.y = suggest_y;
               ti->dirty_flags &= ~S_toolbar_info::DIRTY_SIZE;
               if(suggested_height){
                  ti->height = suggested_height;
                  ti->dirty_flags |= S_toolbar_info::DIRTY_HEIGHT;
               }
            }else{
               ti = &it->second;
               ti->no_create_count = 0;
            }
         }
                              //create shortcut for the toolbar
         if(!ti->action_id)
            ti->action_id = GetFreeActionID();
         ed->AddShortcut(this, ti->action_id, C_fstr("&View\\%%60&Toolbars\\%%%i %s", name[0]-' ', (const char*)name));

         dirty_toolbars.insert(tb);
         return tb;
      }
   } edit_ToolbarMgr;


   void RefreshStatusBar();

   bool re_enter_block;       //flag set when inside of editor, to avoid re-entering
   bool scene_modified;
   int sb_modify;             //status bar column for modify mark
   bool active;
   bool menu_redraw;          //menu needs to be redrawn
   int last_menu_id;

                              //default caches
   C_I3D_model_cache own_model_cache;
   C_I3D_sound_cache own_sound_cache;

   C_I3D_cache_base<I3D_model> *model_cache;
   C_I3D_cache_base<I3D_sound> *sound_cache;

//----------------------------

   dword GenerateMenuID(){
      return ++last_menu_id;
   }

//----------------------------
   static dword I2DAPI cbGraph(dword msg, dword par1, dword par2, void *context){

      switch(msg){
      case CM_COMMAND:
         {
            C_editor_imp *ed = (C_editor_imp*)context;
            ed->MenuHit(par1, K_NOKEY, 0);
         }
         break;
      }
      return 0;
   }

//----------------------------

   bool Init(PI3D_driver, PI3D_scene);
   virtual void Close();
   friend PC_editor CreateEditor(PI3D_driver, PI3D_scene);
public:
   C_editor_imp():
      ref(1),
      last_menu_id(0),
      driver(NULL),
      scene_modified(false),
      re_enter_block(false),
      active(true),
      menu_redraw(false)
      //sb_modify(0)
   {
      //edit_StatusBar.hwnd_sb = NULL;
   }
   virtual ~C_editor_imp(){
      Close();
   }

   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

   virtual void MenuHit(int menu_id, IG_KEY key, byte key_modifier);
   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt);
   virtual void Render();
   virtual int CreateStatusIndex(int width);
   virtual void Message(const char *str, int sb_index = 0, E_EDIT_MESSAGE = EM_MESSAGE, bool immediate = false);
   virtual void BroadcastAction(E_EDIT_ACTION, void *context);
   virtual void EnumPlugins(bool(*)(PC_editor_item, void *context), void *context);

   virtual bool LoadState(const char *bin_name);
   virtual bool SaveState(const char *bin_name) const;

//----------------------------

   virtual void Broadcast(void (C_editor_item::*fnc)()){
      for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
         const C_editor_item *ei = it->first;
         (const_cast<C_editor_item*>(ei)->*fnc)();
      }
   }
   virtual void Broadcast(void (C_editor_item::*fnc)(void*), void *context){
      for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
         const C_editor_item *ei = it->first;
         (const_cast<C_editor_item*>(ei)->*fnc)(context);
      }
   }
   virtual void Broadcast(void (C_editor_item::*fnc)(void*, void*), void *c1, void *c2){
      for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
         const C_editor_item *ei = it->first;
         (const_cast<C_editor_item*>(ei)->*fnc)(c1, c2);
      }
   }

//----------------------------
// Check/uncheck modify flag.
   virtual void SetModified(bool);

//----------------------------
// Check if edited scene is modified.
   virtual bool IsModified() const{ return scene_modified; }

//----------------------------
// Query if modifications can be made.
   virtual bool CanModify(bool quiet);

   virtual bool InstallPlugin(PC_editor_item);
   virtual bool RemovePlugin(PC_editor_item);
   //virtual bool RegForRender(PC_editor_item, int render_order = 1000);
   //virtual bool UnregForRender(PC_editor_item);

                              //called by plugins during insitialization
   virtual bool AddShortcut(PC_editor_item ei, int id, const char *menu_item, IG_KEY key, byte key_modifier);
   virtual bool DelShortcut(PC_editor_item ei, int id);
   virtual bool InstallTempShortcuts(PC_editor_item ei, const S_tmp_shortcut*);
   virtual bool RemoveTempShortcuts(PC_editor_item ei);

   //virtual bool IsInit() const{ return (driver!=NULL); }
   virtual PIGraph GetIGraph() const{ return driver->GetGraphInterface(); }
   virtual PI3D_driver GetDriver() const{ return driver; }
   virtual PI3D_scene GetScene() const{ return (PI3D_scene)(CPI3D_scene)scene; }

   virtual PC_editor_item FindPlugin(const char *plugin_name);
                              //functions to be called bu plug-ins
   virtual void CheckMenu(PC_editor_item, int param, bool);
   virtual void EnableMenu(PC_editor_item, int param, bool);

   virtual void SetActive(bool);
   virtual bool IsActive() const{ return active; }

   virtual C_I3D_cache_base<I3D_model> &GetModelCache(){ return *model_cache; }
   virtual C_I3D_cache_base<I3D_sound> &GetSoundCache(){ return *sound_cache; }

   virtual void DrawAxes(const S_matrix &m, bool sort, S_vector line_scale = S_vector(1, 1, 1),
      float letter_scale = .5f, byte opacity[3] = NULL){

      scene->SetRenderMatrix(I3DGetIdentityMatrix());

      S_vector v[4];
      for(int i=0; i<4; i++){
         v[i] = m(3);
         if(i < 3){
            if(sort)
               v[i][i] += line_scale[i];
            else
               v[i] += m(i) * line_scale[i];
         }
      }
      const S_matrix &cm = scene->GetActiveCamera()->GetMatrix();
                              //sort axes by distance to camera
      int indx[3] = {0, 1, 2};
      if(sort){
         float d[3] = {v[0].Dot(cm(2)), v[1].Dot(cm(2)), v[2].Dot(cm(2))};
         if(d[0] > d[1]) swap(indx[0], indx[1]);
         if(d[indx[1]] > d[2]) swap(indx[1], indx[2]);
         if(d[indx[0]] > d[indx[1]]) swap(indx[0], indx[1]);
      }
      bool is_wire = driver->GetState(RS_WIREFRAME);
      if(is_wire) driver->SetState(RS_WIREFRAME, false);
                              //render back-to-front
      S_vector disp = cm(0) * letter_scale * .1f;
      letter_scale *= m(0).Magnitude();
      I3D_text_coor tx[2];
      tx[0].y = 0.0f;
      tx[1].y = 1.0f;
      for(i=3; i--; ){
         int ii = indx[i];
         dword alpha = opacity ? opacity[ii] : 0xff;
         dword color = (alpha<<24) | (0xff << (8*(2-ii)));
         const S_vector &va = v[ii];
         scene->DrawLine(v[3], va, color);

         tx[0].x = (float)ii * .25f;
         tx[1].x = tx[0].x + .25f;
         scene->DrawSprite(va + disp, axis_icons[ii], color, letter_scale, tx);
      }
      if(is_wire) driver->SetState(RS_WIREFRAME, true);
   }

   virtual class C_poly_text *GetTexts(){ return texts; }

//----------------------------

   virtual PC_toolbar GetToolbar(const C_str &name, int suggest_x, int suggest_y, dword suggested_height){

      return edit_ToolbarMgr.GetToolbar(name, suggest_x, suggest_y, suggested_height);
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck){

      const int NUM_SAVE_PHASES = 4;
      for(dword i=0; i<NUM_SAVE_PHASES; i++){
         for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
            C_smart_ptr<C_editor_item> ei = it->first;
            ei->MissionSave(ck, i);
         }
      }
   }

//----------------------------

   virtual void PRINT(const char *cp, dword color = 0xffffffff){
      if(e_console)
         e_console->Print(cp, color);
   }
   virtual void PRINT(int i){ PRINT(C_fstr("%i", i)); }
   virtual void PRINT(float f){ PRINT(C_fstr("%.2f", f)); }
   virtual void PRINT(const S_vector &v){ PRINT(C_fstr("[%.2f %.2f %.2f]", v.x, v.y, v.z)); }
   virtual void PRINT(const S_vector2 &v){ PRINT(C_fstr("[%.2f %.2f]", v.x, v.y)); }
   virtual void PRINT(const S_quat &q){ PRINT(C_fstr("[%.2f %.2f %.2f  %.2f]", q.v.x, q.v.y, q.v.z, q.s)); }

//----------------------------

   virtual void DEBUG(const char *cp, dword color = 0xc0ffffff){
      if(e_console)
         e_console->Debug(cp, color);
   }
   virtual void DEBUG(int i){ DEBUG(C_fstr("%i", i)); }
   virtual void DEBUG(dword d){ DEBUG(C_fstr("0x%.8x", d)); }
   virtual void DEBUG(float f){ DEBUG(C_fstr("%.2f", f)); }
   virtual void DEBUG(const S_vector &v){ DEBUG(C_fstr("[%.2f %.2f %.2f]", v.x, v.y, v.z)); }
   virtual void DEBUG(const S_vector2 &v){ DEBUG(C_fstr("[%.2f %.2f]", v.x, v.y)); }
   virtual void DEBUG(const S_quat &q){ DEBUG(C_fstr("[%.2f %.2f %.2f  %.2f]", q.v.x, q.v.y, q.v.z, q.s)); }

//----------------------------

   virtual void SetModelCache(C_I3D_cache_base<I3D_model> *mc){
      model_cache = mc;
   }
   virtual void SetSoundCache(C_I3D_cache_base<I3D_sound> *sc){
      sound_cache = sc;
   }
};

//----------------------------

const char *C_editor_imp::C_edit_StatusBar::GetName() const{ return "StatusBar"; }

//----------------------------

void C_editor_imp::C_edit_StatusBar::WinInit(){

   PIGraph igraph = ed->GetIGraph();

   dword sx, sy;
   RECT rc2;
   GetWindowRect((HWND)igraph->GetHWND(), &rc2);
   sx = rc2.right-rc2.left;
   sy = rc2.bottom-rc2.top;

   RECT rc_sb;
   GetWindowRect(hwnd_sb, &rc_sb);
   if(!(igraph->GetFlags()&IG_FULLSCREEN)){
                           //enlarge window for status bar
      sy += (rc_sb.bottom - rc_sb.top);
   }else{
      sy = Min(sy, igraph->Scrn_sy());
   }
   SetWindowPos((HWND)igraph->GetHWND(), NULL, 0, 0, sx, sy,
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
   RECT rc1;
   GetWindowRect((HWND)igraph->GetHWND(), &rc1);
   OffsetRect(&rc_sb, rc1.left, rc1.top);

   SetWindowPos(hwnd_sb, NULL, rc_sb.left, rc_sb.top, 0, 0,
      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);
}

//----------------------------

/*
static bool access_enabled = 
#ifndef FULL_ACCESS
   false;
#else
   true;
#endif
   */

//----------------------------

void CreateProperties(PC_editor);
void CreateLog(PC_editor);
void CreateView(PC_editor);
void CreateUndo(PC_editor);
void CreateGrid(PC_editor);
void CreateSelection(PC_editor);
void CreateCreate(PC_editor);
void CreateMouseEdit(PC_editor);
void CreateHide(PC_editor);
void CreateScreen(PC_editor);
void CreateLight(PC_editor);
void CreateLightMap(PC_editor);
void CreateHelp(PC_editor);
void CreateTimeScale(PC_editor);
void CreateModify(PC_editor);
void CreateValidity(PC_editor);
void CreateStats(PC_editor);
void CreateScreenShot(PC_editor);
void CreateDebugLine(PC_editor);
void CreateExit(PC_editor);
void CreateSeed(PC_editor);
void CreateGravitation(PC_editor);
void CreateBSPTool(PC_editor);
void CreateSubEdit(PC_editor);
PC_editor_item CreateAccess(PC_editor);
void CreateTape(PC_editor);
void CreateUsability(PC_editor editor);
void CreateVideoGrab(PC_editor);
void CreateProcEdit(PC_editor);
void CreateMeshInfo(PC_editor);
void CreateAnimEdit(PC_editor);
void InitCamPathPlugin(PC_editor);
//void CreateShaderStudio(PC_editor);
C_editor_item_Console *CreateConsole(PC_editor ed);

//----------------------------

bool C_editor_imp::Init(PI3D_driver d1, PI3D_scene s1){

   /*
   {
      I3D_driver_internal *di = (I3D_driver_internal*)d1;
      dword v[2];
      I3D_RESULT ir = di->GetVersion(v[0], v[1]);
      if(I3D_FAIL(ir)) return false;
      if(v[0]!=0x35fe9d7c || v[1]!=0x0ac576b3){
         return false;
      }
   }
   */

   driver = d1;
   driver->AddRef();
   scene = s1;

                              //create font system
   {
      C_cache ck_bitmap, ck_opacity, ck_project;
      if(OpenResource(GetHInstance(), "BINARY", "Font1", ck_bitmap) &&
         OpenResource(GetHInstance(), "BINARY", "Font1_a", ck_opacity) &&
         OpenResource(GetHInstance(), "BINARY", "font.txt", ck_project)){

         PC_poly_text pt;
         E_SPRITE_IMAGE_INIT_RESULT ir = CreatePolyText(driver, &pt, ck_project, &ck_bitmap, &ck_opacity, TEXTMAP_NOMIPMAP | TEXTMAP_USE_CACHE);
         if(ir!=SPRINIT_OK)
            return false;
         texts = pt;
         pt->Release();
      }
   }

                              //create axis textures
   {
      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      ct.flags = TEXTMAP_MIPMAP | TEXTMAP_DIFFUSE | TEXTMAP_USE_CACHE | TEXTMAP_TRANSP;

      {
         C_cache ck;
         if(OpenResource(GetHInstance(), "BINARY", "AXES", ck)){
            PI3D_texture tp;
            ct.ck_diffuse = &ck;
            I3D_RESULT ir = driver->CreateTexture(&ct, &tp);
            if(I3D_SUCCESS(ir)){
               for(int i=0; i<3; i++){
                  PI3D_material mat = driver->CreateMaterial();
                  axis_icons[i] = mat;
                  S_vector c(.5f, .5f, .5f);
                  c[i] = 1.0f;
                  mat->SetDiffuse(c);
                  mat->SetTexture(MTI_DIFFUSE, tp);
                  mat->Release();
               }
               tp->Release();
            }
         }
      }
   }

   CreateHelp(this);
//#ifndef FULL_ACCESS
   /*
   PC_editor_item e_access = CreateAccess(this);
   if(!e_access->Action(0))
      return false;
      */
   CreateAccess(this);
//#endif
                              //init status bar
   {
      InitCommonControls();
      edit_StatusBar.hwnd_sb = CreateStatusWindow(
         WS_CHILD,
         NULL,
         (HWND)GetIGraph()->GetHWND(),
         0
         );
      //GetIGraph()->ProcessWinMessages();
      InstallPlugin(&edit_StatusBar);
      edit_StatusBar.WinInit();

                              //alloc first area for standard messages
      CreateStatusIndex(350);
                              //second for modify flag
      sb_modify = CreateStatusIndex(32);
   }
                              //force copy mode in igraph
   //GetIGraph()->UpdateParams(0, 0, 0, IG_UPDATE_USE_COPY, IG_UPDATE_USE_COPY);
   GetIGraph()->AddCallback(cbGraph, this);

   own_model_cache.SetCacheSize(64);
   own_sound_cache.SetCacheSize(64);
   model_cache = &own_model_cache;
   sound_cache = &own_sound_cache;

   InstallPlugin(&edit_ToolbarMgr);

   CreateUndo(this);
   CreateSelection(this);
   CreateModify(this);

   CreateMouseEdit(this);
   CreateTimeScale(this);
#ifndef LIMITED_FUNCTIONALITY
   CreateGrid(this);
   CreateCreate(this);

   CreateValidity(this);
   CreateHide(this);
   CreateScreen(this);
   CreateLight(this);
   CreateLightMap(this);
   CreateStats(this);
   CreateView(this);
   CreateProperties(this);
   CreateScreenShot(this);
   CreateLog(this);
   CreateDebugLine(this);
   CreateSeed(this);
   CreateGravitation(this);
   CreateBSPTool(this);
   CreateSubEdit(this);
   CreateVideoGrab(this);
   CreateProcEdit(this);
   CreateMeshInfo(this);
   CreateAnimEdit(this);
   CreateTape(this);
   CreateUsability(this);
   InitCamPathPlugin(this);
   e_console = CreateConsole(this);
#endif
   CreateExit(this);

   /*
                              //initialize other plugins from directory
   {
      WIN32_FIND_DATA fd;
      memset(&fd, 0, sizeof(fd));
      //fd.dwFileAttributes =
      HANDLE h = FindFirstFile("Plugins\\*.dll", &fd);
      if(h!=INVALID_HANDLE_VALUE){
         do{
            HINSTANCE hi = LoadLibrary(C_fstr("Plugins\\%s", fd.cFileName));
            if(hi){
               typedef void tInitialize(PC_editor);
               tInitialize *ip = (tInitialize*)GetProcAddress(hi, "_Initialize@4");
               if(ip){
                  (*ip)(this);
                  //FindPlugin("Log")->Action(E_LOG_ADD_TEXT, (void*)(const char*)C_fstr("Initialized plugin: %s", fd.cFileName));
               }else
                  FreeLibrary(hi);
            }
         }while(FindNextFile(h, &fd));
         FindClose(h);
      }
   }
   */
   return true;
}

//----------------------------

void C_editor_imp::Close(){

   if(!driver)
      return;
   e_console = NULL;

   for(t_plugin_list::iterator it = plugin_list.begin(); it!=plugin_list.end(); ){
      t_plugin_list::iterator it_next = it;
      it_next++;
      C_smart_ptr<C_editor_item> ei = (*it).first;
      ei->Close();
      plugin_list.erase(it);
      it = it_next;
   }
   plugin_list.clear();
   texts = NULL;

                              //no copy mode
   if(GetIGraph()){
      //GetIGraph()->UpdateParams(0, 0, 0, 0, IG_UPDATE_USE_COPY);
      GetIGraph()->RemoveCallback(cbGraph, this);
   }

   scene = NULL;
   if(driver){
      driver->Release();
      driver = NULL;
   }
   //FreeLibrary((HINSTANCE)hInstance);
   //hInstance = NULL;

   if(edit_StatusBar.hwnd_sb){
      DestroyWindow(edit_StatusBar.hwnd_sb);
      edit_StatusBar.hwnd_sb = NULL;
   }
}

//----------------------------

bool C_editor_imp::InstallPlugin(PC_editor_item ei){

   ei->ed = this;
   /*
                              //access rights
   if(!access_enabled)
      return false;
#ifndef FULL_ACCESS
   PC_editor_item itm = FindPlugin("Access");
   if(itm){
      if(itm->Action(0) == 0)
         return false;
   }else{
                              //allow installation of Access and Help plugins without access
      if(strcmp(ei->GetName(), "Access") && strcmp(ei->GetName(), "Help")) return false;
   }
#endif                        //FULL_ACCESS
   */

   if(!GetIGraph()) return false;
   t_plugin_list::iterator it = plugin_list.insert(pair<C_smart_ptr<C_editor_item>, C_vector<S_plugin_menu_item> >(ei, C_vector<S_plugin_menu_item>())).first;
   if(!ei->Init()){
      //plugin_list.push_back(ei);
      plugin_list.erase(it);
      return false;
   }
   return true;
}

//----------------------------

bool C_editor_imp::RemovePlugin(PC_editor_item ei){

   t_plugin_list::iterator it = plugin_list.find(ei);
   if(it==plugin_list.end())
      return false;
   /*
   for(int i=plugin_list.size(); i--; ) 
      if(plugin_list[i]==ei) break;
   if(i==-1)
      return false;
      */
   RemoveTempShortcuts(ei);
   plugin_list.erase(it);
   //plugin_list.erase(plugin_list.begin()+i);
   return true;
}

//----------------------------

void C_editor_imp::MenuHit(int menu_id, IG_KEY key, byte key_modifier){

   if(re_enter_block) return;
   re_enter_block = true;

                              //process temp shortcuts first
   for(t_tmp_shortcuts::const_iterator it = tmp_shortcuts.begin(); it!=tmp_shortcuts.end(); it++){
      const C_buffer<S_tmp_shortcut> &sbuf = (*it).second;
      for(int i=sbuf.size(); i--; ){
         const S_tmp_shortcut &sc = sbuf[i];
         if(sc.key==key && sc.key_modifier==key_modifier){
            it->first->Action(sc.id);
                              //usability stats
            C_edit_Usability *e_use = (C_edit_Usability*)FindPlugin("Usability");
            if(e_use)
               e_use->AddStats(C_edit_Usability::US_KEYHIT, it->first, 0);
            re_enter_block = false;
            return;
         }
      }
   }

   {
      for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
         const C_vector<S_plugin_menu_item> &menu_items = it->second;
         for(int j=menu_items.size(); j--; ){
            const S_plugin_menu_item &itm = menu_items[j];
            if(!itm.enabled)
               continue;
            if((menu_id && itm.menu_id == menu_id) || (key!=K_NOKEY && itm.key==key && itm.key_modifier==key_modifier)){
               C_smart_ptr<C_editor_item> ei = it->first;
#if 1
                              //usability stats
               {
                  bool menu_cmd = (menu_id && itm.menu_id == menu_id);
                  C_edit_Usability *e_use = (C_edit_Usability*)FindPlugin("Usability");
                  //C_str str;
                  if(menu_cmd){
                     /*
                     char buf[512];
                     MENUITEMINFO mi;
                     mi.cbSize = sizeof(mi);
                     mi.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;
                     mi.dwTypeData = buf;
                     mi.cch = sizeof(buf);

                     GetMenuItemInfo(itm.hmenu_parent, itm.menu_id, false, &mi);
                     {
                              //remove key info - after \t character
                        char *cp = strchr(buf, '\t');
                        if(cp)
                           *cp = 0;
                     }
                     str = C_fstr("%s",
                        buf
                        );
                        */
                     e_use->AddStats(C_edit_Usability::US_MENUHIT, ei, itm.id, itm.key, itm.key_modifier);
                  }else{
                     /*
                     char key_name[32];
                     GetIGraph()->GetKeyName(key, key_name, sizeof(key_name));
                     str = C_fstr("%s%s%s%s",
                        key_modifier&SKEY_CTRL ? "Ctrl+" : "",
                        key_modifier&SKEY_SHIFT ? "Shift+" : "",
                        key_modifier&SKEY_ALT ? "Alt+" : "",
                        key_name
                        );
                        */
                     e_use->AddStats(C_edit_Usability::US_KEYHIT, ei, itm.id, itm.key, itm.key_modifier);
                  }
               }
#endif
               ei->Action(itm.id);
               re_enter_block = false;
               return;
            }
         }
      }
   }
   /*
   for(int i=plugin_list.size(); i--; ){
      PC_editor_item ei = plugin_list[i];
      for(int j=ei->menu_items.size(); j--; ){
         const C_editor_item::S_menu_item &itm = ei->menu_items[j];
         if(itm.enabled)
         if((menu_id && itm.menu_id == menu_id) || (key!=K_NOKEY && itm.key==key && itm.key_modifier==key_modifier)){
            ei->Action(itm.id);
            re_enter_block = false;
            return;
         }
      }
   }
   */
   re_enter_block = false;
}

//----------------------------

void C_editor_imp::RefreshStatusBar(){

   if(edit_StatusBar.hwnd_sb){
      for(int i=status_bar_strings.size(); i--; )
      if(status_bar_strings[i].modified){
         status_bar_strings[i].modified = false;
         SendMessage(edit_StatusBar.hwnd_sb, SB_SETTEXT, i, (LPARAM)(const char*)status_bar_strings[i].str);
      }
   }
}

//----------------------------

void C_editor_imp::Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

   if(re_enter_block) return;
   re_enter_block = true;
   
   for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
      C_smart_ptr<C_editor_item> ei = it->first;
      ei->Tick(skeys, time, mouse_rx, mouse_ry, mouse_rz, mouse_butt);
   }
   /*
   int num_plg = plugin_list.size();
   for(int i=0; i < num_plg; i++ )
   {
      PC_editor_item ei = plugin_list[i];
      ei->Tick(skeys, time, mouse_rx, mouse_ry, mouse_rz, mouse_butt);
   }
   */

   RefreshStatusBar();
   re_enter_block = false;

   if(menu_redraw){
      DrawMenuBar((HWND)GetIGraph()->GetHWND());
      menu_redraw = false;
   }
}

//----------------------------

PC_editor_item C_editor_imp::FindPlugin(const char *plugin_name){

   for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
      C_smart_ptr<C_editor_item> ei = it->first;
      if(!strcmp(ei->GetName(), plugin_name))
         return ei;
   }
   /*
   for(int i=plugin_list.size(); i--; ){
      PC_editor_item ei = plugin_list[i];
      if(!strcmp(ei->GetName(), plugin_name))
         return ei;
   }
   */
   return NULL;
}

//----------------------------

void C_editor_imp::SetModified(bool b){

   if(b && !CanModify(false)){
      Message("ERR", sb_modify);
      return;
   }
   if(scene_modified != b){
      scene_modified = b;
      Message(b ? "MOD" : "", sb_modify);
   }
}

//----------------------------

bool C_editor_imp::CanModify(bool quiet){

                              //ask all plugins
   for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
      C_smart_ptr<C_editor_item> ei = it->first;
      C_editor_item::E_QUERY_MODIFY qm = ei->QueryModify(quiet);
      if(qm!=C_editor_item::QM_OK){
         const char *msg = "!";
         switch(qm){
         case C_editor_item::QM_NO_RDONLY: msg = "RD"; break;
         case C_editor_item::QM_NO_LOCKED: msg = "LOCK"; break;
         }
         Message(msg, sb_modify);
         return false;
      }
   }
   return true;
}

//----------------------------

/*
bool C_editor_imp::RegForRender(PC_editor_item ei, int render_order){

   UnregForRender(ei);
   list<S_edit_render_item>::iterator it;
   for(it=render_list.begin(); it!=render_list.end(); it++){
      if((*it).order<render_order) break;
   }
   render_list.insert(it, S_edit_render_item());
   --it;
   (*it).itm = ei;
   (*it).order = render_order;
   return true;
}

//----------------------------

bool C_editor_imp::UnregForRender(PC_editor_item ei){

   list<S_edit_render_item>::iterator it;
   for(it=render_list.begin(); it!=render_list.end(); it++){
      if((*it).itm==ei){
         render_list.erase(it);
         break;
      }
   }
   return false;
}
*/

//----------------------------

void C_editor_imp::Render(){

   //if(re_enter_block) return;
   //re_enter_block = true;

   driver->BeginScene();

   t_plugin_list::iterator it, it1;

   for(it=plugin_list.begin(); it!=plugin_list.end(); ){
                              //plugin may be erased from list during call, so save it now
      it1 = it++;
      C_smart_ptr<C_editor_item> ei = it1->first;
      ei->Render();
   }
                              //render texts
   {
      bool was_wire = driver->GetState(RS_WIREFRAME);
      if(was_wire) driver->SetState(RS_WIREFRAME, false);
      bool was_txt = driver->GetState(RS_DRAWTEXTURES);
      if(!was_txt) driver->SetState(RS_DRAWTEXTURES, true);
      bool was_zb = driver->GetState(RS_USEZB);
      if(was_zb) driver->SetState(RS_USEZB, false);

      texts->Render(scene);

      if(was_zb) driver->SetState(RS_USEZB, true);
      if(was_wire) driver->SetState(RS_WIREFRAME, true);
      if(!was_txt) driver->SetState(RS_DRAWTEXTURES, false);
   }

   driver->EndScene();
   if(e_console)
      e_console->Action(1000);
   //re_enter_block = false;
}

//----------------------------

bool C_editor_imp::LoadState(const char *bin_name){

   C_chunk ck;
   if(!ck.ROpen(C_fstr("%s.bin", bin_name)))
      return false;
   if(++ck==CT_EDITOR_STATE)
   while(ck){
      switch(++ck){
      case CT_EDITOR_PLUGIN_STATE:
                              //read name of plugin
         {
            C_str str;
            ck.ReadString(CT_NAME, str);
            PC_editor_item ep = FindPlugin(str);
            if(ep){
               ep->LoadState(ck);
            }
         }
         break;
      };
      --ck;
   }
   --ck;
   ck.Close();
   return true;
}

//----------------------------

bool C_editor_imp::SaveState(const char *bin_name) const{

                              //backup old file
   //CopyFile(C_fstr("%s.bin", bin_name), C_fstr("%s.bak", bin_name), false);

   C_chunk ck;
   if(!ck.WOpen(C_fstr("%s.bin", bin_name))){
                              //try to build path
      bool b = OsCreateDirectoryTree(bin_name);
      if(b){
         b = ck.WOpen(C_fstr("%s.bin", bin_name));
      }
      if(!b){
         MessageBox((HWND)GetIGraph()->GetHWND(),
            C_fstr("Cannot save editor state: failed to open file \"%s.bin\" for writing.\nPlease make sure the file is not read-only.", bin_name),
            "Insanity editor", MB_OK);
         return false;
      }
   }
   
   ck <<= CT_EDITOR_STATE;
   {                          //copyright
      ck <<= CT_COPYRIGHT;
      static const char copyright[] = " - Editor configuration - Copyright (c) 2000  Lonely Cat Games - ";
      ck.Write(copyright, sizeof(copyright)+1);
      --ck;
   }
                              //write all installed plugins
   for(t_plugin_list::const_iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
      CPC_editor_item ei = it->first;
      ck <<= CT_EDITOR_PLUGIN_STATE;
      {
         ck.WStringChunk(CT_NAME, ei->GetName());
                              //let the plug-in save its data
         ei->SaveState(ck);
      }
      --ck;
   }
   --ck;
   ck.Close();
   return true;
}

//----------------------------

int C_editor_imp::CreateStatusIndex(int width){

   if(!edit_StatusBar.hwnd_sb)
      return -1;
   int count = SendMessage(edit_StatusBar.hwnd_sb, SB_GETPARTS, 0, 0);
   int *ip = new int[count+1];
   SendMessage(edit_StatusBar.hwnd_sb, SB_GETPARTS, count, (LPARAM)ip);
   if(ip[count-1]==-1)
      --count;
   ip[count] = (count ? ip[count-1] : 0) + width;
   SendMessage(edit_StatusBar.hwnd_sb, SB_SETPARTS, count+1, (LPARAM)ip);
   delete[] ip;
   status_bar_strings.push_back(S_status_bar_msg());
   return count;
}

//----------------------------

void C_editor_imp::Message(const char *str, int index, E_EDIT_MESSAGE msg_type, bool immediate){

   /*
   PC_editor_item e_stats = FindPlugin("Stats");
   if(e_stats)
      e_stats->Action(E_STATS_ADD_SB_TEXT, (void*)str);
   */
   if(index < (int)status_bar_strings.size()){
      status_bar_strings[index].str = str;
      status_bar_strings[index].modified = true;
      if(immediate){
         status_bar_strings[index].modified = false;
         SendMessage(edit_StatusBar.hwnd_sb, SB_SETTEXT, index, (LPARAM)(const char*)status_bar_strings[index].str);
      }
   }
}

//----------------------------

void C_editor_imp::BroadcastAction(E_EDIT_ACTION ea, void *context){

   for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
      C_smart_ptr<C_editor_item> ei = it->first;
      ei->Action(ea, context);
   }
}

//----------------------------

void C_editor_imp::EnumPlugins(bool(*proc)(PC_editor_item, void *context), void *context){

   for(t_plugin_list::const_iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
      C_smart_ptr<C_editor_item> ei = it->first;
      if(!(*proc)(ei, context))
         break;
   }
}

//----------------------------

bool C_editor_imp::AddShortcut(PC_editor_item ei, int id, const char *menu_item, IG_KEY key, byte key_modifier){

   int i;
                              //check key duplication
   if(key!=K_NOKEY){
                              //scan all plugins
      for(t_plugin_list::const_iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
      //for(int j=-1; j<(int)plugin_list.size(); j++)
         //PC_editor_item ei1 = (j==-1) ? ei : plugin_list[j];
         const C_vector<S_plugin_menu_item> &menu_items = it->second;
                              //scan all its commands
         for(i=menu_items.size(); i--; ){
            if(menu_items[i].key==key && menu_items[i].key_modifier==key_modifier){
               CPC_editor_item ei1 = it->first;
               MessageBox((HWND)GetIGraph()->GetHWND(),
                  C_fstr("Plugin \"%s\": shortcut key already used in \"%s\", not installing.\n"
                     "\"%s\""
                  ,
                  (const char*)ei->GetName(),
                  (const char*)ei1->GetName(), menu_item),
                  "Insanity editor", MB_OK);
               return false;
            }
         }
      }
   }

   t_plugin_list::iterator it = plugin_list.find(ei);
   if(it==plugin_list.end())
      return false;
   C_vector<S_plugin_menu_item> &menu_items = it->second;

   menu_items.push_back(S_plugin_menu_item());
   S_plugin_menu_item &mi = menu_items.back();
   mi.menu_id = 0;
   mi.hmenu_parent = NULL;
   mi.enabled = true;

   if(menu_item){
      HMENU hm = (HMENU)GetIGraph()->GetMenu();
      if(hm){
         mi.menu_id = GenerateMenuID();
         int i = 0;
         int len = strlen(menu_item);
         int last_pos = 0;
         while(true){
            int j;
                              //process formatting
            int break_mode = 0;
            dword want_pos = 50;
            while(i<len && menu_item[i]=='%'){
               ++i;
               if(isalpha(menu_item[i])){
                  switch(menu_item[i++]){
                  case 'i': break_mode |= 1; break;   //insert break flag
                  case 'a': break_mode |= 2; break;   //add break flag
                  }
               }else{
                  int num = 0;
                  sscanf(&menu_item[i], "%i%n", &want_pos, &num);
                  i += num;
               }
               while(i<len && isspace(menu_item[i])) ++i;
            }
            last_pos = i;
            for(; i<len; i++) if(menu_item[i]=='\\') break;
            bool popup = (i<len);
            if(popup){
                                 //locate or create sub-menu
               for(j=GetMenuItemCount(hm); j--; ){
                  char buf[256];
                  GetMenuString(hm, j, buf, 255, MF_BYPOSITION);
                  bool b = !strncmp(menu_item+last_pos, buf, i-last_pos);
                                 //sub-menu located
                  if(b){
                     hm = GetSubMenu(hm, j);
                     break;
                  }
               }
               if(j!=-1){
                  ++i;
                  continue;
               }
            }
                                 //add menu item itself
            char buf[256];
            strncpy(buf, menu_item+last_pos, i-last_pos);
            buf[i-last_pos] = 0;

            MENUITEMINFO m;
            memset(&m, 0, sizeof(m));
            int cnt = GetMenuItemCount(hm);
            for(j=0; j<cnt; j++){
               m.cbSize = sizeof(m);
               m.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
               GetMenuItemInfo(hm, j, true, &m);
               //if(!(m.fType&MFT_SEPARATOR))
               {
                  if(m.dwItemData > want_pos)
                     break;
               }
            }
            if(/*j && */(break_mode&1)){
                              //check if not already break
               memset(&m, 0, sizeof(m));
               m.cbSize = sizeof(m);
               m.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
               GetMenuItemInfo(hm, j-1, true, &m);
               if(!(m.fType&MFT_SEPARATOR)){
                  m.fMask = MIIM_TYPE;
                  m.fType = MFT_SEPARATOR;
                  m.dwItemData = want_pos;
                  InsertMenuItem(hm, j, true, &m);
                  ++j;
               }
            }

            memset(&m, 0, sizeof(m));
            m.cbSize = sizeof(m);
            m.fMask |= MIIM_DATA | MIIM_TYPE;
            m.fType = MFT_STRING;
            if(!popup){
               m.fMask |= MIIM_ID | MIIM_ID;
               m.wID = mi.menu_id;
            }else{
               m.fMask |= MIIM_SUBMENU;
               m.hSubMenu = CreateMenu();
            }
            m.dwTypeData = buf;
            m.cch = strlen(buf);
            m.dwItemData = want_pos;
            InsertMenuItem(hm, j, true, &m);

            if(popup) hm = m.hSubMenu;

            if((break_mode&2)){
               ++j;
               MENUITEMINFO m;
               memset(&m, 0, sizeof(m));
               m.cbSize = sizeof(m);
               m.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
               GetMenuItemInfo(hm, j-1, true, &m);
               if(!(m.fType&MFT_SEPARATOR)){
                  m.fMask = MIIM_TYPE;
                  m.fType = MFT_SEPARATOR;
                  m.dwItemData = want_pos;
                  InsertMenuItem(hm, j, true, &m);
               }
            }
            if(!popup) break;
            last_pos = ++i;
         }
         mi.hmenu_parent = hm;
//----------------------------
         menu_redraw = true;
      }
   }
   mi.key = key;
   mi.key_modifier = key_modifier;
   mi.id = id;

   return true;
}

//----------------------------

bool C_editor_imp::InstallTempShortcuts(PC_editor_item ei, const S_tmp_shortcut *ts){

   t_tmp_shortcuts::iterator it =
      tmp_shortcuts.insert(pair<PC_editor_item, C_buffer<S_tmp_shortcut> >(ei, C_buffer<S_tmp_shortcut>())).first;
   C_buffer<S_tmp_shortcut> &sbuf = (*it).second;
   sbuf.clear();
                              //count shortcuts and store
   for(dword i=0; ts[i].id; i++);
   sbuf.assign(ts, ts+i);

                              //create help about temp shortcuts
   PC_editor_item_Help e_help = (PC_editor_item_Help)FindPlugin("Help");
   if(e_help){
      C_fstr title("%s special shortcuts", ei->GetName());
      C_str help;
      help = C_fstr("Special registered shortcuts for plugin \"%s\":\n\n", ei->GetName());
      for(dword i=0; ts[i].id; i++){
         dword mods = ts[i].key_modifier;
         char key_name[256];
         IG_KEY key = ts[i].key;
         GetIGraph()->GetKeyName(key, key_name, sizeof(key_name));
         /*
         switch(key){
         case K_ESC:
            strcpy(key_name, "Esc");
            break;
         default:
            {
               key = MapVirtualKey(key, 0);
               assert(key);
               GetKeyNameText((key<<16) | (1<<24) | (1<<25), key_name, sizeof(key_name));
            }
         }
         */
         C_fstr shortcut("%s%s%s%s",
            (mods&SKEY_CTRL) ? "Ctrl+" : "",
            (mods&SKEY_ALT) ? "Alt+" : "",
            (mods&SKEY_SHIFT) ? "Shift+" : "",
            key_name
            );
         help += C_fstr("%s ... %s\n", (const char*)shortcut, ts[i].desc ? ts[i].desc : "<no description>");
      }
      e_help->AddHelp(ei, 0, title, help);
   }
   return true;
}

//----------------------------

bool C_editor_imp::RemoveTempShortcuts(PC_editor_item ei){

   t_tmp_shortcuts::iterator it = tmp_shortcuts.find(ei);
   if(it!=tmp_shortcuts.end()){
      PC_editor_item_Help e_help = (PC_editor_item_Help)FindPlugin("Help");
      if(e_help){
         e_help->RemoveHelp(ei, 0);
      }
      tmp_shortcuts.erase(it);
      return true;
   }
   return false;
}


//----------------------------

bool C_editor_imp::DelShortcut(PC_editor_item ei, int id){

   t_plugin_list::iterator it = plugin_list.find(ei);
   if(it==plugin_list.end())
      return false;
   C_vector<S_plugin_menu_item> &menu_items = it->second;

   for(int i=menu_items.size(); i--; ){
      S_plugin_menu_item &mi = menu_items[i];
      if(mi.id==id){
                              //delete menu item (if any)
         if(mi.menu_id && mi.hmenu_parent){
            HMENU hmenu_parent = mi.hmenu_parent;
            bool b;
            b = DeleteMenu(hmenu_parent, mi.menu_id, MF_BYCOMMAND);
            assert(b);
            /*
            while(hmenu_parent && !GetMenuItemCount(hmenu_parent)){
               DestroyMenu(hmenu_parent);
               hmenu_parent = NULL;
            }
            */
         }
         mi = menu_items.back(); menu_items.pop_back();
         return true;
      }
   }
   return false;
}

//----------------------------

void C_editor_imp::CheckMenu(PC_editor_item ei, int id, bool b){

                              //find menu item
   t_plugin_list::iterator it = plugin_list.find(ei);
   if(it==plugin_list.end())
      return;
   C_vector<S_plugin_menu_item> &menu_items = it->second;

   for(int i = menu_items.size(); i--; ){
      const S_plugin_menu_item &itm = menu_items[i];
      if(itm.id==id){
         CheckMenuItem(itm.hmenu_parent, itm.menu_id, MF_BYCOMMAND |
            (b ? MF_CHECKED : MF_UNCHECKED));
         break;
      }
   }
}

//----------------------------

void C_editor_imp::EnableMenu(PC_editor_item ei, int id, bool b){

                              //find menu item
   t_plugin_list::iterator it = plugin_list.find(ei);
   if(it==plugin_list.end())
      return;
   C_vector<S_plugin_menu_item> &menu_items = it->second;

   for(int i = menu_items.size(); i--; ){
      S_plugin_menu_item &itm = menu_items[i];
      if(itm.id==id){
         EnableMenuItem(itm.hmenu_parent, itm.menu_id, MF_BYCOMMAND |
            (b ? MF_ENABLED : MF_GRAYED));
         itm.enabled = b;
         break;
      }
   }
}

//----------------------------

void C_editor_imp::SetActive(bool b){

   if(active!=b){
      active = b;
      Message(active ? "Edit mode" : "Game mode");
      
      for(t_plugin_list::iterator it=plugin_list.begin(); it!=plugin_list.end(); it++){
         C_smart_ptr<C_editor_item> ei = it->first;
         ei->SetActive(b);
      }

      if(!active)             //last chance to do so
         RefreshStatusBar();
   }
}

//----------------------------
//----------------------------

static BOOL CALLBACK dlgSplash(HWND hwnd,  // handle to dialog box
   UINT uMsg,     // message  
   WPARAM wParam, // first message parameter
   LPARAM lParam  // second message parameter
   ){

   switch(uMsg){
   case WM_INITDIALOG:
      {
                              //center window
         PIGraph igraph = (PIGraph)lParam;
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         igraph->EnableSuspend(false);

         RECT rc;
         SetRect(&rc, 0, 0, 298, 196);
         int scx = GetSystemMetrics(SM_CXSCREEN);
         int scy = GetSystemMetrics(SM_CYSCREEN);
         int x = rc.left+(scx-(rc.right-rc.left))/2;
         int y = rc.top+(scy-(rc.bottom-rc.top))/2;
         SetWindowPos((HWND)hwnd, NULL, x, y, 
            rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER);
         igraph->AddRef();
         igraph->AddDlgHWND(hwnd);
         SetTimer(hwnd, 1, 2000, NULL);

         ShowWindow(hwnd, SW_SHOW);
      }
      return 1;

   case WM_TIMER:
      KillTimer(hwnd, 1);
      DestroyWindow(hwnd);
      break;

   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDCLOSE:
      case IDCANCEL:
         {
            EndDialog(hwnd, 0);
         }
         break;
      }
      break;
   case WM_DESTROY:
      {
         PIGraph igraph = (PIGraph)GetWindowLong(hwnd, GWL_USERDATA);
         igraph->EnableSuspend(true);
         igraph->RemoveDlgHWND(hwnd);
         igraph->Release();
      }
      break;
   }
   return 0;
}

//----------------------------

static void RegisterViewer(){
                           //register icon and viewer
   int rkey;
   {
      rkey = RegkeyCreate(".i3d", E_REGKEY_CLASSES_ROOT);
      if(rkey!=-1){
         RegkeyWtext(rkey, NULL, "Insanity.Viewer");
         RegkeyClose(rkey);
      }
      rkey = RegkeyCreate("Insanity.Viewer", E_REGKEY_CLASSES_ROOT);
      if(rkey!=-1){
         RegkeyWtext(rkey, NULL, "Insanity 3D scene");
         RegkeyClose(rkey);
      }
      rkey = RegkeyCreate("Insanity.Viewer\\DefaultIcon", E_REGKEY_CLASSES_ROOT);
      if(rkey!=-1){
         char buf[MAX_PATH];
         GetModuleFileName(GetModuleHandle("I3D_2.DLL"), buf, sizeof(buf));
         RegkeyWtext(rkey, NULL, C_fstr("%s,0", buf));
         RegkeyClose(rkey);
      }
      rkey = RegkeyCreate("Insanity.Viewer\\shell\\open\\command", E_REGKEY_CLASSES_ROOT);
      if(rkey!=-1){
         char buf[MAX_PATH];
         GetModuleFileName(NULL, buf, sizeof(buf));
         RegkeyWtext(rkey, NULL, C_fstr("%s model \"%%1\"", buf));
         RegkeyClose(rkey);
      }
      rkey = RegkeyCreate("Software\\Classes\\.I3D", E_REGKEY_LOCAL_MACHINE);
      if(rkey!=-1){
         RegkeyWtext(rkey, NULL, "Insanity.Viewer");
      }
   }
}

//----------------------------

static HINSTANCE hInstance;

HINSTANCE GetHInstance(){
   return hInstance;
}

//----------------------------

C_editor *CreateEditor(PI3D_driver drv, PI3D_scene scn){

   //hInstance = LoadLibrary("IEditor.dll");
   hInstance = GetModuleHandle(NULL);
   RegisterViewer();

   C_editor_imp *ed = new C_editor_imp;
   if(!ed->Init(drv, scn)){
      ed->Close();
      delete ed;
      ed = NULL;
      return NULL;
   }

   if(IsProgrammersPC(ed)){
#if defined _DEBUG && !defined DEBUG_NO_LEAK_WARNING
                              //report memory leaks at exit
      _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
      _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
#endif
   }else{
                              //show splash screen
                              // (once a day)
      C_edit_Usability *e_use = (C_edit_Usability*)ed->FindPlugin("Usability");
      if(e_use->IsFirstRunToday())
      CreateDialogParam(GetHInstance(), "IDD_ABOUT1", (HWND)drv->GetGraphInterface()->GetHWND(), dlgSplash,
         (LPARAM)drv->GetGraphInterface());
   }
   return ed;
}

//----------------------------
