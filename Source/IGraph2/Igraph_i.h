#ifndef __IGRAPHI_H
#define __IGRAPHI_H

#include <d3d9.h>
#include <I3D\I3D_math.h>

//----------------------------

#ifdef _DEBUG

void D3D_Fatal(const char *text, dword hr, const char *file, dword line);
#define THROW(what) throw(exception(what))
#define CHECK_D3D_RESULT(text, hr) if(FAILED(hr)) D3D_Fatal(text, hr, __FILE__, __LINE__);

#else

#define D3D_Fatal(t, hr, file, line)
#define THROW(what)
#define CHECK_D3D_RESULT(text, hr)

#endif

#ifdef GL
#ifdef _DEBUG
void GL_Fatal(const char *text, dword code, const char *file, dword line);
#define CHECK_GL_RESULT(text) { dword err = glGetError(); if(err!=GL_NO_ERROR) GL_Fatal(text, err, __FILE__, __LINE__); }
#else
#define CHECK_GL_RESULT(text)
#endif
#endif

//----------------------------

#define FLOAT_BITMASK(f) (*(int*)&f)

#if defined _MSC_VER && 1

inline int FloatToInt(float f){
   __asm{
      fld f
      fistp f 
   }
   return *(int*)&f;
}
#else
inline int FloatToInt(float f){
   return (int)f;
}
#endif

inline int FloatAsInt(float f){
   return *(dword*)&f;
}

inline float IntAsFloat(int i){
   return (*(float*)&i);
}

//----------------------------

#ifndef NDEBUG

//#define LOG_MODE              //log all hardware calls
#ifdef LOG_MODE

#include <fstream>

class C_log{
   std::ostream *os_log;
   int indent;
   void MakeIndent(){
      for(int i=0; i<indent*2; i++) (*os_log)<<" ";
   }
   friend class IGraph;
public:
   C_log(): os_log(NULL) {}
   void Enter(const char *cp){
      MakeIndent();
      (*os_log)<<cp <<std::endl;
      ++indent;
   };
   void Leave(const char *cp){
      MakeIndent();
      (*os_log)<<cp <<std::endl;
      --indent;
   }
   inline void Ok(){ Leave("OK"); }
   inline void Fail(){ Leave("Fail"); }
   inline void Done(){ Leave("Done"); }
   C_log &operator <<(const char *cp){
      MakeIndent();
      (*os_log)<<cp <<std::endl;
      return *this;
   }
};
extern C_log IG_log;

#endif                        //LOG_MODE

#endif                        //NDEBUG

//----------------------------

class C_unk_base{
public:
   I2DMETHOD_(dword,AddRef)() PURE;
   I2DMETHOD_(dword,Release)() PURE;
};

template<class T>
class C_unk: public C_unk_base{
   unsigned long ref;
protected:
   C_unk(): ref(1) {}

   I2DMETHOD_(dword,AddRef)(){ return ++ref; }
   I2DMETHOD_(dword,Release)(){
      unsigned long new_ref=--ref;
      if(!new_ref) delete (T*)this;
      return new_ref;
   }
};

//----------------------------
PIGraph I2DAPI IGraphCreate(PIG_INIT);

//----------------------------
                              //graphics interface class
class IGraph: public C_unk<IGraph>{

#define IGGLAGS_IN_RESET   1  //currently reseting device

   enum{
      KEY_BUF_SIZE = 16,
   };

//----------------------------

   dword ig_flags;

   bool want_close_app;

   HINSTANCE h_dinput;        //dinput.dll

                              //keyboard
   byte shiftkeys;
   IG_KEY key;                //currently pressed key
   dword game_keys;           //game keyboard
   IG_KEY key_scodes[32];
   dword num_key_scodes;
   void *kb_hook;
   bool keyboard_map[256];    //true when key is down
   IG_KEY key_buffer[KEY_BUF_SIZE];
   byte key_buf_beg, key_buf_num;

   friend LRESULT CALLBACK GKeyboardProc(int code, WPARAM wParam, LPARAM lParam);
   LRESULT KeyboardProc(int code, WPARAM wParam, LPARAM lParam);

                              //mouse
   IDirectInput8 *lpDI8;
   IDirectInputDevice8 *lpDIMouse8;
   IDirectInputDevice8 *lpDIKeyboard;
   HANDLE h_mouse_event, h_mouse_thread;
   bool event_signalled;
   bool mouse_init;
   //int mouse_left, mouse_top, mouse_right, mouse_bottom;
   bool swap_mouse_buttons;
   bool init_ok;
   bool mouse_debug_mode;
   int acq_count;
   int ms_speed[3];
   int mouse_x1, mouse_y1;    //help
   int mouse_z1;
   byte mouse_butt1;
   int mouse_pos[3];
   int mouse_rel_pos[3];
   byte mouse_butt;
                              //wheel mouse:
   dword wm_mousewheel;       //0 = no wheel present
   dword mouse_wheel_lines;   //# of lines to scroll on wheel

                              //timer
   double qpc_resolution;
   dword start_timer, last_timer;
   bool use_qpc;

   static void DoDrawsClip(PIGraph, dword flags);
   static void DoDrawsNoClip(PIGraph, dword flags);

   HINSTANCE h_d3d9;          //d3d9.dll
   IDirect3D9 *lpD3D9;
   IDirect3DDevice9 *lpDev9;
#ifdef GL
   EGLDisplay egl_display;
   EGLSurface egl_surface;
   EGLContext egl_context;
#endif

                              //keep internal list of convertors,
                              // to avoid duplication of their initialization
   mutable C_vector<C_smart_ptr<C_rgb_conversion> > rgb_conv_list;
public:
   LPC_rgb_conversion GetRGBConv1(const S_pixelformat&, const dword *pal_rgba = NULL) const;
private:
                              //counter where is GDI window
                              // used for FlipToGDI
                              // this is 100% reliable, but works on most adapters
   int backbuffer_count;      
   int num_swap_buffers;

   HMENU hmenu_main;
                              //window's style - different in windowed
                              //and fullscreen mode
   dword create_flags;
   byte create_bits_per_pixel;
   IG_INIT::E_ANTIALIAS_MODE create_aa_mode;
   C_str appName;          //name of window
   IG_INIT::t_log_func *log_func;
   dword suspend_disable_count;  //if zero, suspension is enabled

                              //registered dialog windows
   C_vector<HWND> hwnd_dlgs;
   struct S_callback{
      PGRAPHCALLBACK cb_proc;
      void *context;
   };
   C_vector<S_callback> cb_proc_list;

   bool GrabKeys(bool on);
   LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);
   friend LRESULT CALLBACK WndProc_thunk(HWND, UINT, WPARAM, LPARAM);
   friend static dword CALLBACK NotifyThreadMouse(void *igraph);

   friend PIGraph I2DAPI IGraphCreate(PIG_INIT);
   bool Initialize(dword pos_x, dword pos_y, dword size_x, dword size_y,
      dword flags, int bits_per_pixel, IG_INIT::E_ANTIALIAS_MODE aa_mode,
      int adapter_id,
      IG_INIT::t_log_func* = NULL);
   HRESULT CreateFrontBack();

   void InitPresentStruct(int adapter_id, D3DDEVTYPE, D3DPRESENT_PARAMETERS&) const;
public:
   IGraph();
   ~IGraph();

   bool InitWindow(int posx, int posy);
   void DestroyWindow();
   HRESULT InitRGBConv();

                              //windows
   HWND hwnd;
   bool active;               //application active
   bool sleep_loop;           //inside of sleeping loop
   dword scrn_sx, scrn_sy;    //size of screen
   dword desktop_sx, desktop_sy; //desktop resolution

                              //viewport
   int vport[4];              //[left, top, right, bottom]

   C_smart_ptr<C_rgb_conversion> rgb_conv;

   void MouseAcquire(bool);

public:
   I2DMETHOD_(dword,AddRef)(){ return C_unk<IGraph>::AddRef(); }
   I2DMETHOD_(dword,Release)(){ return C_unk<IGraph>::Release(); }
                              //interfaces:
   I2DMETHOD_(bool,AddCallback)(PGRAPHCALLBACK, void *context);
   I2DMETHOD_(bool,RemoveCallback)(PGRAPHCALLBACK, void *context);

   I2DMETHOD_(const class C_rgb_conversion*,GetRGBConv)() const{ return rgb_conv; }
   I2DMETHOD_(void*,GetHWND)() const{ return hwnd; }
   I2DMETHOD_(void*,GetMenu)() const{ return hmenu_main; }
   I2DMETHOD_(void,SetMenu)(void*);
   I2DMETHOD_(dword,GetFlags)() const{ return create_flags; }
   I2DMETHOD_(bool,UpdateParams)(dword sx, dword sy, dword bpp, dword flg, dword flg_mask, bool force_recreate);

   I2DMETHOD_(bool,SetViewport)(dword left, dword top, dword sizex, dword sizey);
   I2DMETHOD_(bool,ClearViewport)();
   I2DMETHOD_(void,FlipToGDI)();

   I2DMETHOD_(bool,SetBrightness)(float r, float g, float b);

   I2DMETHOD_(const char*,GetAppName)() const{ return appName; }
   I2DMETHOD_(void,SetAppName)(const char*);

                              //keyboard
   /*
   I2DMETHOD_(void,InitKeyboard)();
   I2DMETHOD_(void,CloseKeyboard)();
   I2DMETHOD_(bool,IsKbInit)() const{ return kb_init; }
   */
                              //remove!!!
   I2DMETHOD_(void,DefineKeys)(const IG_KEY*, dword num = 32);
   I2DMETHOD_(IG_KEY,ReadKey)(bool process_messages = false);
   I2DMETHOD_(dword,ReadKeys)(){ return game_keys; }

   I2DMETHOD_(dword,GetShiftKeys)() const{ return shiftkeys; }
   I2DMETHOD_(int,GetAscii)(IG_KEY k, bool shift = false) const;
   I2DMETHOD_(const bool*,GetKeyboardMap)() const{ return keyboard_map; }
   I2DMETHOD_(dword,GetKeyName)(IG_KEY key, char *buf, dword buf_size) const;
   I2DMETHOD_(IG_KEY,GetBufferedKey)();
                              //mouse
   I2DMETHOD_(bool,MouseInit)(dword mode = 0); //normal/exclusive
   I2DMETHOD_(void,MouseClose)();
   //I2DMETHOD_(void,MouseSetup)(int mouse_x, int mouse_y, int clip_left, int clip_top, int clip_right, int clip_bottom);
   I2DMETHOD_(void,MouseUpdate)();
   I2DMETHOD_(int,Mouse_x)() const{ return mouse_pos[0]; }
   I2DMETHOD_(int,Mouse_y)() const{ return mouse_pos[1]; }
   I2DMETHOD_(int,Mouse_rx)() const{ return mouse_rel_pos[0]; }
   I2DMETHOD_(int,Mouse_ry)() const{ return mouse_rel_pos[1]; }
   I2DMETHOD_(int,Mouse_rz)() const{ return mouse_rel_pos[2]; }
   I2DMETHOD_(void,SetMousePos)(int x, int y);
   I2DMETHOD_(dword,GetMouseButtons)() const{ return mouse_butt; }
   //I2DMETHOD_(int,GetMousePos)(int axis, bool relative) const;
                              //windows
   I2DMETHOD_(void,ProcessWinMessages)(dword flags=0);
                              //timer
   I2DMETHOD_(void,NullTimer)();
   I2DMETHOD_(dword,GetTimer)(dword min, dword max);
   I2DMETHOD_(dword,ReadTimer)() const;

                              //screen
   I2DMETHOD_(dword,Scrn_sx)() const{ return scrn_sx; }
   I2DMETHOD_(dword,Scrn_sy)() const{ return scrn_sy; }
   I2DMETHOD_(const S_pixelformat*,GetPixelFormat)() const{ return rgb_conv->GetPixelFormat(); }
   I2DMETHOD_(PIImage,CreateImage)() const;
   I2DMETHOD_(class C_rgb_conversion*,CreateRGBConv)() const;

   I2DMETHOD_(bool,GetImageInfo)(const char *name, dword &sx, dword &sy, byte &bpp, dword &id) const;
   I2DMETHOD_(bool,GetImageInfo)(class C_cache &ck, dword &sx, dword &sy, byte &bpp, dword &id) const;
                              //Dialog windows
   I2DMETHOD_(bool,AddDlgHWND)(void*);
   I2DMETHOD_(bool,RemoveDlgHWND)(void*);

   I2DMETHOD_(struct IDirect3D9*,GetD3DInterface)() const{ return lpD3D9; }
   I2DMETHOD_(struct IDirect3DDevice9*,GetD3DDevice)() const{ return lpDev9; }
   I2DMETHOD_(bool,UpdateScreen)(dword flags = 0, void *dest_hwnd = NULL);

   I2DMETHOD_(void,GetDesktopResolution)(dword &x, dword &y) const;
   I2DMETHOD_(dword,NumDisplayModes)() const;
   I2DMETHOD_(bool,GetDisplayMode)(int id, dword *sx, dword *sy, dword *bpp) const;

   I2DMETHOD_(class IImage*,CreateBackbufferCopy)(dword sx, dword sy, bool lockable) const;
   I2DMETHOD_(dword,GetBackbufferCount)() const{ return num_swap_buffers-1; }

   I2DMETHOD_(bool,GetWindowCloseStatus)(){
      bool ret = want_close_app;
      want_close_app = false;
      return ret;
   }
   I2DMETHOD_(dword,EnableSuspend)(bool);

   I2DMETHOD_(bool,LockBackbuffer)(void **area, dword *pitch, bool read_only = true);
   I2DMETHOD_(bool,UnlockBackbuffer)();

//----------------------------
   I2DMETHOD_(bool,SetCursorProperties)(const char *bitmap_name, dword hot_x = 0, dword hot_y = 0);
   I2DMETHOD_(void,ShowCursor)(bool on){
      lpDev9->ShowCursor(on);
   }
   I2DMETHOD_(void,SetCursorPosition)(int x, int y){
      lpDev9->SetCursorPosition(x, y, 0);
   }
};

//----------------------------
                              //image interface
class IImage: public C_unk<IImage>{

   C_smart_ptr<IDirect3DTexture9> lpTxt9;
   C_smart_ptr<IDirect3DCubeTexture9> lpCTxt9;
   C_smart_ptr<IDirect3DSurface9> lpImg9;

   dword size_x, size_y;
   dword *pal_argb;           //pointer to 256 ARGB values, or NULL
   C_vector<byte> ckey_rle_info;//rle-compressed colorkey info
   CPIGraph igraph;
   friend IGraph;

   dword open_flags;
   bool MixMipmapOnSurface(const D3DLOCKED_RECT *lrc, int num_levels) const;
   bool OpenInternal(C_cache &ck, const char *name, dword flags, dword sx1, dword sy1, const S_pixelformat *pf, dword num_mip_levels, C_str *err_msg);
protected:
   S_pixelformat pixel_format;
public:
   IImage(CPIGraph);
   ~IImage();

   I2DMETHOD_(dword,AddRef)(){ return C_unk<IImage>::AddRef(); }
   I2DMETHOD_(dword,Release)(){ return C_unk<IImage>::Release(); }
                              //initialization
   I2DMETHOD_(bool,Open)(const char *name, dword flags = 0, dword sx = 0, dword sy = 0, const S_pixelformat* = NULL, dword mipmapminsize = 0, C_str *err_msg = NULL);
   I2DMETHOD_(bool,Open)(class C_cache &ck, dword flags = 0, dword sx = 0, dword sy = 0, const S_pixelformat* = NULL, dword mipmapminsize = 0, C_str *err_msg = NULL);
   I2DMETHOD_(void,Close)();
   I2DMETHOD_(bool,IsInit)() const;
                              //palette stuff
   I2DMETHOD_(bool,SetPal)(dword*);
   I2DMETHOD_(dword*,GetPal)() const{ return (dword*)pal_argb; }
                              //rendering
   I2DMETHOD_(bool,Draw)(int x=0, int y=0, int sx = 0, int sy = 0);
                              //access surface
   I2DMETHOD_(bool,Lock)(void **area, dword *pitch, bool read_only);
   I2DMETHOD_(bool,Unlock)();
                              //special fncs
   I2DMETHOD_(bool,AlphaBlend)(CPIImage, dword flags = 0);

   I2DMETHOD_(bool,MixMipmap)(dword flags);
   I2DMETHOD_(bool,ConvertBumpMap)(CPIImage, dword flags = 0);
                              //info
   I2DMETHOD_(const S_pixelformat*,GetPixelFormat)() const;
   I2DMETHOD_(const void*,GetColorkeyInfo)(dword *size) const{
      const void *ptr;
      dword sz;
      if(ckey_rle_info.size()){
         ptr = &ckey_rle_info.front();
         sz = ckey_rle_info.size();
      }else{
         ptr = NULL;
         sz = 0;
      }
      if(size)
         *size = sz;
      return ptr;
   }
   I2DMETHOD_(void,SetColorkeyInfo)(void *data, dword size){
      ckey_rle_info.assign((byte*)data, (byte*)data+size);
   }
   I2DMETHOD_(bool,CopyColorkeyInfo)(CPIImage img){
      if(!img)
         return false;
      ckey_rle_info = img->ckey_rle_info;
      return true;
   }
   I2DMETHOD_(dword,SizeX)() const{ return size_x; }
   I2DMETHOD_(dword,SizeY)() const{ return size_y; }

   I2DMETHOD_(CPIGraph,GetIGraph)() const{ return igraph; }

   I2DMETHOD_(IDirect3DTexture9*,GetDirectXTexture)() const{ return (IDirect3DTexture9*)(const IDirect3DTexture9*)lpTxt9; }
   I2DMETHOD_(IDirect3DSurface9*,GetDirectXSurface)() const{ return (IDirect3DSurface9*)(const IDirect3DSurface9*)lpImg9; }

   I2DMETHOD_(bool,SaveShot)(C_cache*, const char *ext) const;
   I2DMETHOD_(bool,CopyStretched)(CPIImage src_img);

   I2DMETHOD_(IDirect3DCubeTexture9*,GetDirectXCubeTexture)() const{ return (IDirect3DCubeTexture9*)(const IDirect3DCubeTexture9*)lpCTxt9; }

   I2DMETHOD_(bool,ConvertToNormalMap)(CPIImage src, float scale = 1.0f);
   typedef dword I2DAPI t_ApplyFunc(dword argb, dword x, dword y, dword pass_index);
   I2DMETHOD_(bool,ApplyFunction)(t_ApplyFunc *func, dword num_passes);
};

//----------------------------
//----------------------------
// For all fully transparent pixels, make their color to be blend of non-fully-transparent neighbours.
// This is a fix for filtering of pixels to neighbour pixels, which, although having zero alpha, affect colors of their neighbours.
// This function works for ARGB pixels, and pitch = sx*4.
void FixBordersOfAlphaPixels(dword *mem, dword sx, dword sy);

//----------------------------

#endif
