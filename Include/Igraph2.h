//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// File: Igraph.h
// Content: Insanity 2D graphics include file
//----------------------------

#ifndef __IGRAPH2_H
#define __IGRAPH2_H

#include <rules.h>

#define I2DAPI
#define I2DMETHOD_(type,method) virtual type I2DAPI method

//----------------------------
                              //mouse buttons
#define MBUT_LEFT    1
#define MBUT_RIGHT   2
#define MBUT_MIDDLE  4

                              //shift keys
#define SKEY_SHIFT   1
#define SKEY_CTRL    4
#define SKEY_ALT     8

//----------------------------
                              //pixel-format structure
#define PIXELFORMAT_PALETTE   1  //paletized pixel format
#define PIXELFORMAT_ALPHA     2  //alpha pixels present
#define PIXELFORMAT_COMPRESS  4  //compressed pixel format (four_cc member valid)
#define PIXELFORMAT_BUMPMAP   8  //bumpmap format

struct S_pixelformat{
#pragma warning(push)
#pragma warning(disable:4201)
   union{
      dword bytes_per_pixel;  //valid if !compressed
      dword four_cc;          //valid if compressed
   };
   union{
      struct{
                              //ARGB non-compressed formats
         dword r_mask, g_mask, b_mask, a_mask;
      };
      struct{
                              //bumpmap formats
         dword u_mask, v_mask, reserved, l_mask;
      };
   };
#pragma warning(pop)
   dword flags;
};
typedef S_pixelformat *PS_pixelformat;
typedef const S_pixelformat *CPS_pixelformat;

//-------------------------------------
                              //virtual key values
enum IG_KEY{
   K_NOKEY = 0,
   K_CTRLBREAK = 3,
   K_BACKSPACE = 8, K_TAB,
   K_CENTER = 12, K_ENTER = 13, K_GREYENTER = 13,
   K_SHIFT = 16, K_LSHIFT = 16, K_RSHIFT = 16, K_CTRL, K_LCTRL = 17, K_RCTRL = 17, K_ALT, K_LALT = 18, K_RALT = 18, K_PAUSE,
   K_CAPSLOCK,
   K_ESC = 27,
   K_SPACE = 32, K_PAGEUP, K_PAGEDOWN, K_END,
   K_HOME, K_CURSORLEFT, K_CURSORUP, K_CURSORRIGHT,
   K_CURSORDOWN,
   K_PRTSCR = 44, K_INS, K_DEL,
   K_0 = 48, K_1, K_2, K_3,
   K_4, K_5, K_6, K_7,
   K_8, K_9,
   K_A = 65, K_B, K_C, K_D, K_E, K_F, K_G, K_H,
   K_I, K_J, K_K, K_L, K_M, K_N, K_O, K_P,
   K_Q, K_R, K_S, K_T, K_U, K_V, K_W, K_X,
   K_Y, K_Z,

   K_LEFT_WIN = 91, K_RIGHT_WIN = 92, K_MENU,
   K_NUM0 = 96, K_NUM1, K_NUM2, K_NUM3,
   K_NUM4, K_NUM5, K_NUM6, K_NUM7,
   K_NUM8, K_NUM9, K_GREYMULT, K_GREYPLUS,
      K_GREYMINUS = 109, K_GREYDOT, K_GREYSLASH,
   K_F1, K_F2, K_F3, K_F4,
   K_F5, K_F6, K_F7, K_F8,
   K_F9, K_F10, K_F11, K_F12,
   K_NUMLOCK = 0x90,
   K_SCROLLLOCK,
   K_SEMICOLON = 0xba,        //;   K_3B
   K_EQUALS,                  //=   K_3D
   K_COMMA,                   //,   K_2C
   K_MINUS,                   //-   K_2D
   K_DOT,                     //.   K_2E
   K_SLASH,                   ///   K_2F
   K_BACKAPOSTROPH,           //`   K_60
   K_LBRACKET = 0xdb,         //[   K_5B
   K_BACKSLASH,               //\   K_5C
   K_RBRACKET,                //]   K_5D
   K_APOSTROPH,               //'   K_27
};

//-------------------------------------
                              //graphics initialization
#define IG_FULLSCREEN      1
#define IG_TRIPPLEBUF      2
#define IG_NO_VSHADER      4        //disable hw usage of vertex shaders
#define IG_NOSYSTEMMENU    8
#define IG_NOMINIMIZE      0x10
#define IG_RESIZEABLE      0x20     //the window may be resized
#define IG_NOTITLEBAR      0x40     //no titlebar on window
#ifndef GL
#define IG_REF_DEVICE      0x80     //use reference D3D device
#endif
#define IG_LOCKABLE_BACKBUFFER 0x200   //backbuffer will be lock-able (possible performance penaulty)
#define IG_NOHIDEMOUSE     0x800    //for fullscreen only
#define IG_LOADMENU        0x1000
#define IG_HIDDEN          0x8000   //hide window
#define IG_VERBOSE         0x10000  //dump driver caps
#define IG_DEBUGMOUSE      0x20000  //debug mouse mode
#define IG_WARN_OUT        0x40000  //dump info about unreleased interfaces when driver is released
#define IG_INITPOSITION    0x80000  //initialize also position

//----------------------------

                              //graphics callback function
typedef dword I2DAPI GRAPHCALLBACK(dword msg, dword par1, dword par2, void *context);
typedef GRAPHCALLBACK *PGRAPHCALLBACK;
                              //graphics callback messages
enum{
   CM_ACTIVATE,               //(bool active)
   CM_COMMAND,                //(dword item_id, dword notify_code)
   CM_RECREATE,               //(dword new_flags, dword phase[0 = before_destroy, 1 = after_recreate, 2 = apply_changes])
   CM_PAINT,
};

typedef class IImage *PIImage;
typedef const IImage *CPIImage;

//----------------------------
                              //graphics interface class
#ifndef IG_INTERNAL
class IGraph{
public:
   I2DMETHOD_(dword,AddRef)() = 0;
   I2DMETHOD_(dword,Release)() = 0;
                              //interfaces:
   I2DMETHOD_(bool,AddCallback)(PGRAPHCALLBACK, void *context) = 0;
   I2DMETHOD_(bool,RemoveCallback)(PGRAPHCALLBACK, void *context) = 0;
   I2DMETHOD_(const class C_rgb_conversion*,GetRGBConv)() const = 0;
   I2DMETHOD_(void*,GetHWND)() const = 0;
   I2DMETHOD_(void*,GetMenu)() const = 0;
   I2DMETHOD_(void,SetMenu)(void*) = 0;
   I2DMETHOD_(dword,GetFlags)() const = 0;
   I2DMETHOD_(bool,UpdateParams)(dword sx, dword sy, dword bpp, dword flg, dword flg_mask, bool force_recreate = false) = 0;

   I2DMETHOD_(bool,SetViewport)(dword left, dword top, dword sizex, dword sizey) = 0;
   I2DMETHOD_(bool,ClearViewport)() = 0;
   I2DMETHOD_(void,FlipToGDI)() = 0;
   I2DMETHOD_(bool,SetBrightness)(float r, float g, float b) = 0;

   I2DMETHOD_(const char*,GetAppName)() const = 0;
   I2DMETHOD_(void,SetAppName)(const char*) = 0;
                              //keyboard
   //I2DMETHOD_(void,InitKeyboard)() = 0;
   //I2DMETHOD_(void,CloseKeyboard)() = 0;
   //I2DMETHOD_(bool,IsKbInit)() const = 0;
   I2DMETHOD_(void,DefineKeys)(const IG_KEY*, dword num = 32) = 0;
   I2DMETHOD_(IG_KEY,ReadKey)(bool process_messages = false) = 0;
   I2DMETHOD_(dword,ReadKeys)() = 0;
   I2DMETHOD_(dword,GetShiftKeys)() const = 0;
   I2DMETHOD_(int,GetAscii)(IG_KEY k, bool shift = false) const = 0;
   I2DMETHOD_(const bool*,GetKeyboardMap)() const = 0;
   I2DMETHOD_(dword,GetKeyName)(IG_KEY key, char *buf, dword buf_size) const = 0;
   I2DMETHOD_(IG_KEY,GetBufferedKey)() = 0;
                              //mouse
   I2DMETHOD_(bool,MouseInit)(dword mode = 0) = 0; //normal/exclusive
   I2DMETHOD_(void,MouseClose)() = 0;
   //I2DMETHOD_(void,MouseSetup)(int mouse_x, int mouse_y, int clip_left, int clip_top, int clip_right, int clip_bottom) = 0;
   I2DMETHOD_(void,MouseUpdate)() = 0;
   I2DMETHOD_(int,Mouse_x)() const = 0;
   I2DMETHOD_(int,Mouse_y)() const = 0;
   I2DMETHOD_(int,Mouse_rx)() const = 0;
   I2DMETHOD_(int,Mouse_ry)() const = 0;
   I2DMETHOD_(int,Mouse_rz)() const = 0;
   I2DMETHOD_(void,SetMousePos)(int x, int y) = 0;
   I2DMETHOD_(dword,GetMouseButtons)() const = 0;
   //I2DMETHOD_(int,GetMousePos)(int axis, bool relative = false) const = 0;
                              //windows
   I2DMETHOD_(void,ProcessWinMessages)(dword flags=0) = 0;
                              //timer
   I2DMETHOD_(void,NullTimer)() = 0;
   I2DMETHOD_(dword,GetTimer)(dword min, dword max) = 0;
   I2DMETHOD_(dword,ReadTimer)() const = 0;
                              //screen
   I2DMETHOD_(dword,Scrn_sx)() const = 0;
   I2DMETHOD_(dword,Scrn_sy)() const = 0;
   I2DMETHOD_(const S_pixelformat*,GetPixelFormat)() const = 0;
                              //creation
   I2DMETHOD_(PIImage,CreateImage)() const = 0;
   I2DMETHOD_(class C_rgb_conversion*,CreateRGBConv)() const = 0;

   I2DMETHOD_(bool,GetImageInfo)(const char *name, dword &sx, dword &sy, byte &bpp, dword &id) const = 0;
   I2DMETHOD_(bool,GetImageInfo)(class C_cache &ck, dword &sx, dword &sy, byte &bpp, dword &id) const = 0;
                              //Dialog windows
   I2DMETHOD_(bool,AddDlgHWND)(void *hwnd) = 0;
   I2DMETHOD_(bool,RemoveDlgHWND)(void *hwnd) = 0;

   I2DMETHOD_(struct IDirect3D9*,GetD3DInterface)() const = 0;
   I2DMETHOD_(struct IDirect3DDevice9*,GetD3DDevice)() const = 0;
   I2DMETHOD_(bool,UpdateScreen)(dword flags = 0, void *dest_hwnd = NULL) = 0;

   I2DMETHOD_(void,GetDesktopResolution)(dword &x, dword &y) const = 0;
   I2DMETHOD_(dword,NumDisplayModes)() const = 0;
   I2DMETHOD_(bool,GetDisplayMode)(int id, dword *sx, dword *sy, dword *bpp) const = 0;

//----------------------------
// Create sys-mem image with the same format as backbuffer
// and copy the contents of backbuffer into it.
   I2DMETHOD_(class IImage*,CreateBackbufferCopy)(dword sx = 0, dword sy = 0, bool lockable = false) const = 0;

//----------------------------
// Get number of backbuffers.
   I2DMETHOD_(dword,GetBackbufferCount)() const = 0;

   I2DMETHOD_(bool,GetWindowCloseStatus)() const = 0;

   I2DMETHOD_(dword,EnableSuspend)(bool) = 0;

                              //access backbuffer (if it's lockable)
   I2DMETHOD_(bool,LockBackbuffer)(void **area, dword *pitch, bool read_only = true) = 0;
   I2DMETHOD_(bool,UnlockBackbuffer)() = 0;
};
#endif

typedef class IGraph *PIGraph;
typedef const IGraph *CPIGraph;

//----------------------------
                              //init driver struct
struct IG_INIT{
   dword pos_x, pos_y;
   dword size_x, size_y;
   dword bits_per_pixel;
   dword flags;
   int adapter_id;
   typedef void t_log_func(const char*);
   t_log_func *log_func;
   enum E_ANTIALIAS_MODE{
      ANTIALIAS_NONE,
      ANTIALIAS_NORMAL,
      ANTIALIAS_BEST,
   } antialias_mode;
};
typedef IG_INIT *PIG_INIT;
typedef const IG_INIT *CPIG_INIT;

                              //located in thunk static lib:
PIGraph I2DAPI IGraphCreate(PIG_INIT);

//----------------------------

#define IMGOPEN_TEXTURE       1        //make texture surface
#define IMGOPEN_NOREMAP       2        //don't convert to destination pixel format
#define IMGOPEN_DITHER        4        //dither during conversion
#define IMGOPEN_SYSMEM        8        //create surface in system memory
#define IMGOPEN_VIDMEM        0x10     //create surface in video memory
#define IMGOPEN_EMPTY         0x20     //create empty surface
#define IMGOPEN_MIPMAP        0x80     //make mipmap chain
#define IMGOPEN_COLORKEY      0x200    //implicitly set color-key on surface
#define IMGOPEN_HINTDYNAMIC   0x400    //dynamic image (used with IMGOPEN_TEXTURE)
#define IMGOPEN_HINTSTATIC    0x800    //static image (used with IMGOPEN_TEXTURE)
#define IMGOPEN_CUBE_TEXTURE  0x1000   //create cube texture (used with IMGOPEN_TEXTURE)
#define IMGOPEN_RENDERTARGET  0x2000   //request image which may be used as rendertarget
#define IMGOPEN_INIT_CKEY     0x4000   //init colorkey alpha from color #0, create RLE transparency

#define IMGBLEND_INVALPHA  1
#define IMGBLEND_DITHER    IMGOPEN_DITHER

#define IMGMIPMAP_DITHER   IMGOPEN_DITHER
                              //image interface
#ifndef IG_INTERNAL
class IImage{
public:
   I2DMETHOD_(dword,AddRef)() = 0;
   I2DMETHOD_(dword,Release)() = 0;
                              //initialization
   I2DMETHOD_(bool,Open)(const char *name, dword flags = 0, dword sx = 0, dword sy = 0, const S_pixelformat* = NULL, dword num_mip_levels = 0, class C_str *err_msg = NULL) = 0;
   I2DMETHOD_(bool,Open)(class C_cache &ck, dword flags = 0, dword sx = 0, dword sy = 0, const S_pixelformat* = NULL, dword num_mip_levels = 0, C_str *err_msg = NULL) = 0;
   I2DMETHOD_(void,Close)() = 0;
   I2DMETHOD_(bool,IsInit)() const = 0;
                              //palette stuff
   I2DMETHOD_(bool,SetPal)(dword*) = 0;
   I2DMETHOD_(dword*,GetPal)() const = 0;
                              //rendering
   I2DMETHOD_(bool,Draw)(int x = 0, int y = 0, int sx = 0, int sy = 0) = 0;
                              //access surface
   I2DMETHOD_(bool,Lock)(void **area, dword *pitch, bool read_only = true) = 0;
   I2DMETHOD_(bool,Unlock)() = 0;
                              //special fncs
   I2DMETHOD_(bool,AlphaBlend)(CPIImage, dword flags = 0) = 0;
   I2DMETHOD_(bool,MixMipmap)(dword flags) = 0;
   I2DMETHOD_(bool,ConvertBumpMap)(CPIImage, dword flags = 0) = 0;
                              //info
   I2DMETHOD_(const S_pixelformat*,GetPixelFormat)() const = 0;
   I2DMETHOD_(const void*,GetColorkeyInfo)(dword *size = NULL) const = 0;
   I2DMETHOD_(void,SetColorkeyInfo)(void *data, dword size) = 0;
   I2DMETHOD_(bool,CopyColorkeyInfo)(IImage*) = 0;
   I2DMETHOD_(dword,SizeX)() const = 0;
   I2DMETHOD_(dword,SizeY)() const = 0;

   I2DMETHOD_(CPIGraph,GetIGraph)() const = 0;

   I2DMETHOD_(struct IDirect3DTexture9*,GetDirectXTexture)() const = 0;
   I2DMETHOD_(struct IDirect3DSurface9*,GetDirectXSurface)() const = 0;

   I2DMETHOD_(bool,SaveShot)(class C_cache*, const char *ext = "pcx") const = 0;
   I2DMETHOD_(bool,CopyStretched)(CPIImage src_img) = 0;

   I2DMETHOD_(struct IDirect3DCubeTexture9*,GetDirectXCubeTexture)() const = 0;

   I2DMETHOD_(bool,ConvertToNormalMap)(CPIImage src, float scale = 1.0f) = 0;
   typedef dword I2DAPI t_ApplyFunc(dword argb, dword x, dword y, dword pass_index);
   I2DMETHOD_(bool,ApplyFunction)(t_ApplyFunc *func, dword num_passes = 1) = 0;
};
#endif

//----------------------------

#endif //__IGRAPH2_H
