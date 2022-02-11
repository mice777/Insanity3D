//#define WIN32_LEAN_AND_MEAN
#define DTA_READ_STATIC
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <d3d9.h>
#include "resource.h"
#include <c_cache.h>
#include <dsound.h>
#include <algorithm>
#pragma warning(push,3)
#include <vector>
#pragma warning(pop)
#include <config.h>
#include <insanity\texts.h>
#include <insanity\os.h>

using namespace std;

//#define REG_BASE "Software\\Lonely Cat Games\\Hidden and Dangerous Deluxe\\"
//static const char REGDIR[] = REG_BASE"1.50";
//static const char REGKEY_CONFIG[] = REG_BASE"Config";

static const char APP_NAME[] = "Insanity 3D setup";

//#define ALLOW_FREQUENCY

static C_all_texts all_txt;
static HINSTANCE hInstance;

//----------------------------

#ifndef NDEBUG
static void D3D_Fatal(const char *text, dword hr);
#define CHECK_D3D_RESULT(text, hr) if(FAILED(hr)) D3D_Fatal(text, hr);
#else
#define D3D_Fatal(a, b)
#define CHECK_D3D_RESULT(text, hr)
#endif

//----------------------------

#ifndef NDEBUG
static void D3D_Fatal(const char *text, dword hr){

   char *msgp;
   switch(hr){

   case D3D_OK: msgp = "No error occurred."; break;
   case D3DERR_CONFLICTINGRENDERSTATE: msgp = "The currently set render states cannot be used together."; break;
   case D3DERR_CONFLICTINGTEXTUREFILTER: msgp = "The current texture filters cannot be used together."; break;
   case D3DERR_CONFLICTINGTEXTUREPALETTE: msgp = "The current textures cannot be used simultaneously. This generally occurs when a multitexture device requires that all palletized textures simultaneously enabled also share the same palette."; break;
   case D3DERR_DEVICELOST: msgp = "The device is lost and cannot be restored at the current time, so rendering is not possible."; break;
   case D3DERR_DEVICENOTRESET: msgp = "The device cannot be reset."; break;
   case D3DERR_DRIVERINTERNALERROR: msgp = "Internal driver error."; break;
   case D3DERR_INVALIDCALL: msgp = "The method call is invalid. For example, a method's parameter may have an invalid value."; break;
   case D3DERR_INVALIDDEVICE: msgp = "The requested device type is not valid."; break;
   case D3DERR_MOREDATA: msgp = "There is more data available than the specified buffer size can hold."; break;
   case D3DERR_NOTAVAILABLE: msgp = "This device does not support the queried technique."; break;
   case D3DERR_NOTFOUND: msgp = "The requested item was not found."; break;
   case D3DERR_OUTOFVIDEOMEMORY: msgp = "Direct3D does not have enough display memory to perform the operation."; break;
   case D3DERR_TOOMANYOPERATIONS: msgp = "The application is requesting more texture-filtering operations than the device supports."; break;
   case D3DERR_UNSUPPORTEDALPHAARG: msgp = "The device does not support a specified texture-blending argument for the alpha channel."; break;
   case D3DERR_UNSUPPORTEDALPHAOPERATION: msgp = "The device does not support a specified texture-blending operation for the alpha channel."; break;
   case D3DERR_UNSUPPORTEDCOLORARG: msgp = "The device does not support a specified texture-blending argument for color values."; break;
   case D3DERR_UNSUPPORTEDCOLOROPERATION: msgp = "The device does not support a specified texture-blending operation for color values."; break;
   case D3DERR_UNSUPPORTEDFACTORVALUE: msgp = "The device does not support the specified texture factor value."; break;
   case D3DERR_UNSUPPORTEDTEXTUREFILTER: msgp = "The device does not support the specified texture filter."; break;
   case D3DERR_WRONGTEXTUREFORMAT: msgp = "The pixel format of the texture surface is not valid."; break;
   case E_FAIL: msgp = "An undetermined error occurred inside the Direct3D subsystem."; break;
   case E_INVALIDARG: msgp = "An invalid parameter was passed to the returning function."; break;
   //case E_INVALIDCALL: msgp = "The method call is invalid. For example, a method's parameter may have an invalid value."; break;
   case E_OUTOFMEMORY: msgp = "Direct3D could not allocate sufficient memory to complete the call."; break;
   //default: DD_Fatal(text, hr); return;
   }

   char buf[16];
   itoa(hr, buf, 16);

   MessageBox(NULL,
      (C_str)text + (char const*)": " + (char const*)msgp +
      (char const*)" 0x" + (char const*)buf,
      "Fatal Error", MB_OK);
}
#endif

//----------------------------

static void InitDlg(HWND hwnd){
                              //center dlg and flip to GDI
   RECT rc;
   GetWindowRect(hwnd, &rc);
   int sx = GetSystemMetrics(SM_CXSCREEN);
   int sy = GetSystemMetrics(SM_CYSCREEN);
   SetWindowPos(hwnd, NULL,
      (sx-(rc.right-rc.left))/2,
      (sy-(rc.bottom-rc.top))/2,
      0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

//----------------------------

static S_game_configuration config;
static C_str reg_base;

static bool first_adapter_init = true, first_dev_init = true;

#ifdef NDEBUG
static bool all_features = false;
static dword limit_mode_x = 640, limit_mode_y = 480;
#else
static bool all_features = true;
static dword limit_mode_x = 0, limit_mode_y = 0;
#endif

//----------------------------

class C_DisplayMode{
public:
   dword width, height;
#ifdef ALLOW_FREQUENCY
   dword freq;
#endif

   vector<dword> bit_depths;

   C_DisplayMode(){}
   C_DisplayMode(dword w, dword h
#ifdef ALLOW_FREQUENCY
      , int f
#endif
      ):
      width(w), height(h)
#ifdef ALLOW_FREQUENCY
         , freq(f)
#endif
   {}

                              //func for sorting display modes
   bool operator <(const C_DisplayMode &dm) const{

      if((width*height) < (dm.width*dm.height))
         return true;
      if((width*height) > (dm.width*dm.height))
         return false;

#ifdef ALLOW_FREQUENCY
      if(freq < dm.freq)
         return true;
#endif
      return false;
   }

   void AddBitDepth(dword d){
      bit_depths.push_back(d);
      sort(bit_depths.begin(), bit_depths.end());
   }

   bool HasBitDepth(dword d) const{

      for(int i=bit_depths.size(); i--; ){
         if(bit_depths[i]==d)
            return true;
      }
      return false;
   }
};

//----------------------------
//----------------------------

class C_Adapter{
   C_str name;

   vector<C_DisplayMode> display_modes;
public:
   int adapter_identifier;

   C_Adapter(){}

   C_Adapter(const char *name, IDirect3D9 *d3d9, int adapter_index);

   const C_str &Name() const{ return name; }

   inline C_DisplayMode* GetModes(){ return &display_modes.front(); }

   inline int NumModes() const{ return display_modes.size(); }
};

//----------------------------

C_Adapter::C_Adapter(const char *n,
   IDirect3D9 *d3d9, int adapter_index):
   adapter_identifier(adapter_index),
   name(n)
{
   int i;

   static const D3DFORMAT check_formats[] = {
      D3DFMT_R5G6B5,
      D3DFMT_X1R5G5B5,
      D3DFMT_R8G8B8,
      D3DFMT_X8R8G8B8,
      D3DFMT_A8R8G8B8,
   };

   for(dword fi=sizeof(check_formats)/sizeof(check_formats[0]); fi--; ){

      D3DFORMAT fmt = check_formats[fi];
      int num_modes = d3d9->GetAdapterModeCount(adapter_index, fmt);
      dword bpp = 0;
                              //allow only rgb modes
      switch(fmt){
      case D3DFMT_X1R5G5B5:
         bpp = 16;
         break;
      case D3DFMT_R5G6B5:
         bpp = 16;
         break;
      case D3DFMT_R8G8B8:
         bpp = 24;
         break;
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A8R8G8B8:
         bpp = 32;
         break;
      default: assert(0);
      }
                              //enum display modes of this device
      for(i=0; i<num_modes; i++){
         D3DDISPLAYMODE dd;
         d3d9->EnumAdapterModes(adapter_index, fmt, i, &dd);


                                 //check if resolution acceptable
         if(dd.Width>=limit_mode_x && dd.Height>=limit_mode_y){
#ifndef ALLOW_FREQUENCY
                                 //don't include same mode more times
            int j;
            for(j=display_modes.size(); j--; ){
               if(display_modes[j].width==dd.Width && display_modes[j].height==dd.Height){
                  if(!display_modes[j].HasBitDepth(bpp))
                     display_modes[j].AddBitDepth(bpp);
                  break;
               }
            }
            if(j!=-1)
               continue;
            display_modes.push_back(C_DisplayMode(dd.Width, dd.Height));
            display_modes.back().AddBitDepth(bpp);
#else
            display_modes.push_back(C_DisplayMode(dd.Width, dd.Height, bpp, dd.RefreshRate));
#endif
         }
      }
   }
                              //add current mode, if not yet
   int j;
   for(j=display_modes.size(); j--; ){
      if(display_modes[j].width==config.mode_x && display_modes[j].height==config.mode_y)
         break;
   }
   if(j==-1)
      display_modes.push_back(C_DisplayMode(config.mode_x, config.mode_y));

   sort(display_modes.begin(), display_modes.end());
}

//----------------------------

class C_Enum{
   vector<C_Adapter*> adapters;
public:
   HINSTANCE hi_dll;          //hinstance of this DLL

   HRESULT EnumAll(C_str&);

   void Clear(){
      for(int i=0; i<(int)adapters.size(); i++)
         delete adapters[i];
      adapters.clear();
   }

   inline int Size() const{
      return adapters.size();
   }

   inline C_Adapter& operator [](int i) const{
      return *adapters[i];
   }
};

//----------------------------

HRESULT C_Enum::EnumAll(C_str &err){

   {
                              //Load d3d9 library
      HINSTANCE hi_d3d9 = LoadLibrary("d3d9.dll");
      if(!hi_d3d9){
         err = "unable to load d3d9.dll";
         goto fail;
      }

      FARPROC fp = GetProcAddress(hi_d3d9, "Direct3DCreate9");
      if(!fp){
         FreeLibrary(hi_d3d9);
         err = "unable to find entry point 'Direct3DCreate9' in d3d9.dll";
         goto fail;
      }
      
      typedef IDirect3D9 *WINAPI t_Direct3DCreate9(UINT SDKVersion);
      t_Direct3DCreate9 *create_proc = (t_Direct3DCreate9*)fp;

      IDirect3D9 *d3d9 = (*create_proc)(D3D_SDK_VERSION);
      if(!d3d9){
         FreeLibrary(hi_d3d9);
         err = "unable to create Direct3D object";
         goto fail;
      }

      int num_adapters = d3d9->GetAdapterCount();
      if(!num_adapters){
         d3d9->Release();
         FreeLibrary(hi_d3d9);
         err = "Direct3D returned zero adapter count";
         goto fail;
      }

                                 //enum all adapters
      for(int i=0; i<num_adapters; i++){

         D3DADAPTER_IDENTIFIER9 ai;
         d3d9->GetAdapterIdentifier(i, 0, &ai);

         adapters.push_back(new C_Adapter(ai.Description, d3d9, i));
      }

      d3d9->Release();

      FreeLibrary(hi_d3d9);
   }

   return D3D_OK;

fail:
   return -1;
}

//----------------------------

static int SelectAdapter(HWND hwnd, C_Enum* dde){

   config.adapter_identifier = 0;
   first_adapter_init = false;
   //if(i==dde->Size()) i = 0;
   SendDlgItemMessage(hwnd, IDC_COMBO_DEVICES, CB_SETCURSEL, 0, 0);
   return 0;
}

//----------------------------

static void SetModes(HWND hwnd, C_Adapter *dp){
   
   C_DisplayMode *modes = dp->GetModes();
   SendDlgItemMessage(hwnd, IDC_COMBO_MODES, CB_RESETCONTENT, 0, 0);

   for(int i=0; i<dp->NumModes(); i++){
      C_str str;
      {
#ifdef ALLOW_FREQUENCY
         str = C_fstr("%ix%i %i bit (%i Hz)", modes[i].width,
            modes[i].height, modes[i].bits_per_pixel, modes[i].freq);
#else
         str = C_fstr("%i x %i", modes[i].width, modes[i].height);
#endif
         SendDlgItemMessage(hwnd, IDC_COMBO_MODES, CB_ADDSTRING,
            0, (LPARAM)(const char*)str);
      }
   }
}

//----------------------------

static void SelectMode(C_Adapter *dp, int res_i){

   const C_DisplayMode &mode = dp->GetModes()[res_i];
   config.mode_x = mode.width;
   config.mode_y = mode.height;
}

//----------------------------

static void SetDepths(HWND hwnd, const C_DisplayMode &mode, dword flags){

   SendDlgItemMessage(hwnd, IDC_COMBO_DEPTHS, CB_RESETCONTENT, 0, 0);

   for(int i=0, j=0; i<(int)mode.bit_depths.size(); i++){
      dword bpp = mode.bit_depths[i];
      if(flags&(1<<((bpp/8)-1))){
         C_fstr str("%i bit", bpp);
         SendDlgItemMessage(hwnd, IDC_COMBO_DEPTHS, CB_ADDSTRING,
            0, (LPARAM)(const char*)str);
         if(config.bits_per_pixel==bpp)
            SendDlgItemMessage(hwnd, IDC_COMBO_DEPTHS, CB_SETCURSEL, j, 0);
         ++j;
      }
   }
}

//----------------------------

static void SelectDepth(int res_i, int dep_i, C_Adapter *dp, dword flags = 0xf){

   C_DisplayMode &mode = dp->GetModes()[res_i];
   for(int j=0; j<(int)mode.bit_depths.size(); j++){
      int bpp = mode.bit_depths[j];
      if(flags&(1<<((bpp/8)-1))){
         if(!dep_i--){
            config.bits_per_pixel = bpp;
            return;
         }
      }
   }
}

//----------------------------

static void SelectMode(HWND hwnd, C_Adapter *dp, C_Enum* dde, dword flags = 0xf){

   int best_mode = -1;
   const C_DisplayMode *modes = dp->GetModes();
   int i, j;
   for(i=0, j=0; i<dp->NumModes(); i++){
      const C_DisplayMode *mode = &dp->GetModes()[i];
      if(config.mode_x==mode->width &&
         config.mode_y==mode->height){
         if(best_mode==-1)
            best_mode = j;
      }
      ++j;
   }
   if(i==dp->NumModes())
      j=best_mode==-1 ? 0 : best_mode;
   SelectMode(dp, j);

   SendDlgItemMessage(hwnd, IDC_COMBO_MODES, CB_SETCURSEL, j, 0);
   SetDepths(hwnd, modes[best_mode], flags);
}

//----------------------------

static const char font_name[] = "MS Gothic";

static HFONT japCreateFontJ(int charx, int chary){

   static HFONT hfont = CreateFont(
      chary,                  //logical height of font
      charx/2,
      0,                      //angle of escapement
      0,                      //base-line orientation angle
      FW_NORMAL,              //font weight
      //FW_THIN,              //font weight
      FALSE,                  //italic attribute flag
      FALSE,                  //underline attribute flag
      FALSE,                  //strikeout attribute flag
      SHIFTJIS_CHARSET,       //character set identifier
      OUT_DEFAULT_PRECIS,     //output precision
      CLIP_DEFAULT_PRECIS,    //clipping precision
      DEFAULT_QUALITY,        //output quality
      FIXED_PITCH | FF_DONTCARE, //pitch and family
      (const char*)font_name);//pointer to typeface name string);

   return hfont;
}

//----------------------------
/*
                              //window controls' help data
struct S_ctrl_help_info{
   word ctrl_id;              //control ID (0 is terminator)
   const char *text_id;       //help text
};

static const S_ctrl_help_info help_data[] = {
   IDC_DISPLAY,         "help_disp",
   IDC_COMBO_DEVICES,   "help_disp",
   IDC_TXT_RES,         "help_res",
   IDC_COMBO_MODES,     "help_res",
   IDC_TXT_DEPTH,       "help_depth",
   IDC_COMBO_DEPTHS,    "help_depth",
   IDOK,                "help_ok",
   IDCANCEL,            "help_cancel",
   ID_APPLY,            "help_apply",
   IDC_TXT_SETTINGS,    "help_settings",
   IDC_CHECK_FULLSCREEN,"help_fullscreen",
   IDC_CHECK_TRIPPLEBUF,"help_tripple",
   IDC_CHECK_TXT_COMPR, "help_compress",
   IDC_TXT_TXFILTER,    "help_filtering",
   //IDC_RADIO_LINEAR,    "help_filter_linear",
   //IDC_RADIO_ANISO,     "help_filter_aniso",
   IDC_TXT_ANTIALIAS,   "help_anitialias",
   IDC_ANTIALIAS,       "help_anitialias",

   IDC_SOUND_DEV_TXT,   "help_snd_dev",
   IDC_SND_DEVICE_NAME, "help_snd_dev",
   IDC_CHECK_SOUNDS,    "help_sounds",
   IDC_CHECK_USE_EAX,   "help_use_eax",
   IDC_CHECK_USE_SOUND_HW, "help_snd_hw",

   IDC_CHECK_DBASE,     "help_database",
   IDC_CHECK_REF_DEVICE,"help_ref",
   IDC_CHECK_NO_VSHADER,"help_no_vshader",
   IDC_CHECK_NO_PSHADER,"help_no_pshader",
   0
};

static void DisplayHelp(HWND hwnd, word ctrl_id, const S_ctrl_help_info *hi){

   RECT rc;
   GetWindowRect(hwnd, &rc);
   POINT pt;
   pt.x = rc.left;
   pt.y = rc.bottom;

   ScreenToClient(hwnd, &pt);
   pt.x -= 40;
   pt.y += 2;
   const char *text_id = "help_no";
   for(dword i=0; hi[i].ctrl_id; i++){
      if(hi[i].ctrl_id==ctrl_id){
         text_id = hi[i].text_id;
         break;
      }
   }
   OsDisplayHelpWindow(all_txt[text_id], hwnd, pt.x, pt.y, 250);
}
*/

//----------------------------

enum E_SHEET{
   SHEET_VIDEO,
   SHEET_SOUND,
   SHEET_ADVANCED,
   SHEET_LAST
};

static const struct{
   const char *sheet_name;
   const char *dlg_name;
} sheet_data[SHEET_LAST] = {
   {"Video", "IDD_PAGE_VIDEO"},
   {"Sound", "IDD_PAGE_SOUND"},
   {"Advanced", "IDD_PAGE_ADVANCED"},
};

static HWND hwnd_sheet[SHEET_LAST], hwnd_dlg, hwnd_tc;

//----------------------------

static void SaveChanges(C_Enum *dde){

   {
      HWND hwnd = hwnd_sheet[SHEET_VIDEO];
      config.compress_textures = IsDlgButtonChecked(hwnd, IDC_CHECK_TXT_COMPR);

      config.tripple_buffering = IsDlgButtonChecked(hwnd, IDC_CHECK_TRIPPLEBUF);
      config.fullscreen = IsDlgButtonChecked(hwnd, IDC_CHECK_FULLSCREEN);
      int i = SendDlgItemMessage(hwnd, IDC_ANTIALIAS, CB_GETCURSEL, 0, 0);
      config.antialias_mode = (S_game_configuration::E_ANTIALIAS_MODE)i;

      i = SendDlgItemMessage(hwnd, IDC_COMBO_DEVICES, CB_GETCURSEL, 0, 0);
   }

   {
      HWND hwnd = hwnd_sheet[SHEET_SOUND];
      config.use_sounds = IsDlgButtonChecked(hwnd, IDC_CHECK_SOUNDS);
      config.sounds_use_EAX = IsDlgButtonChecked(hwnd, IDC_CHECK_USE_EAX);
      config.sounds_use_3D_hw = IsDlgButtonChecked(hwnd, IDC_CHECK_USE_SOUND_HW);
   }

   {
      HWND hwnd = hwnd_sheet[SHEET_ADVANCED];
      if(hwnd){
         config.use_dbase  = IsDlgButtonChecked(hwnd, IDC_CHECK_DBASE);
         config.use_reference_device = IsDlgButtonChecked(hwnd, IDC_CHECK_REF_DEVICE);
         config.disable_vshader  = IsDlgButtonChecked(hwnd, IDC_CHECK_NO_VSHADER);
         config.disable_pshader  = IsDlgButtonChecked(hwnd, IDC_CHECK_NO_PSHADER);
      }
   }
   config.WriteSysConfig(reg_base);
}

//----------------------------

static BOOL CALLBACK DlgSheet(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      }
      break;

   case WM_COMMAND:
      switch(LOWORD(wParam)){

      case IDC_COMBO_MODES:
         switch(HIWORD(wParam)){
         case CBN_SELCHANGE:
            {
               C_Enum *dde = (C_Enum*)GetWindowLong(hwnd, GWL_USERDATA);
               int i = SendDlgItemMessage(hwnd, IDC_COMBO_DEVICES, CB_GETCURSEL, 0, 0);
               C_Adapter *dp = &(*dde)[i];
               i = SendDlgItemMessage(hwnd, LOWORD(wParam), CB_GETCURSEL, 0, 0);
               SelectMode(dp, i);
               SelectMode(hwnd, dp, dde);
               EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
            }
            break;
         }
         break;

      case IDC_COMBO_DEPTHS:
         switch(HIWORD(wParam)){
         case CBN_SELCHANGE:
            {
               C_Enum *dde = (C_Enum*)GetWindowLong(hwnd, GWL_USERDATA);
               int i = SendDlgItemMessage(hwnd, IDC_COMBO_DEVICES, CB_GETCURSEL, 0, 0);
               C_Adapter *dp = &(*dde)[i];
               int res_i = SendDlgItemMessage(hwnd, IDC_COMBO_MODES, CB_GETCURSEL, 0, 0);
               int dep_i = SendDlgItemMessage(hwnd, LOWORD(wParam), CB_GETCURSEL, 0, 0);
               SelectDepth(res_i, dep_i, dp);
               EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
            }
            break;
         }
         break;

      case IDC_CHECK_FULLSCREEN:
         {
            bool b = IsDlgButtonChecked(hwnd, IDC_CHECK_FULLSCREEN);
            EnableWindow(GetDlgItem(hwnd, IDC_CHECK_TRIPPLEBUF), b);
            EnableWindow(GetDlgItem(hwnd, IDC_COMBO_DEPTHS), b);

            EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
         }
         break;

      case IDC_CHECK_SOUNDS:
         {
            bool b = IsDlgButtonChecked(hwnd, IDC_CHECK_SOUNDS);
            EnableWindow(GetDlgItem(hwnd, IDC_CHECK_USE_SOUND_HW), b);
            if(b)
               b = IsDlgButtonChecked(hwnd, IDC_CHECK_USE_SOUND_HW);
            EnableWindow(GetDlgItem(hwnd, IDC_CHECK_USE_EAX), b);

            EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
         }
         break;

      case IDC_CHECK_USE_SOUND_HW:
         {
            bool b = IsDlgButtonChecked(hwnd, IDC_CHECK_USE_SOUND_HW);
            EnableWindow(GetDlgItem(hwnd, IDC_CHECK_USE_EAX), b);

            EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
         }
         break;

      case IDC_CHECK_NO_VSHADER:
         {
            bool b = IsDlgButtonChecked(hwnd, IDC_CHECK_NO_VSHADER);
            EnableWindow(GetDlgItem(hwnd, IDC_CHECK_NO_PSHADER), !b);
            EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
         }
         break;

      case IDC_ANTIALIAS:
         switch(HIWORD(wParam)){
         case CBN_SELCHANGE:
            EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
            break;
         }
         break;

      case IDC_COMBO_DEVICES:
         switch(HIWORD(wParam)){
         case CBN_SELCHANGE:
            {
               C_Enum *dde = (C_Enum*)GetWindowLong(hwnd, GWL_USERDATA);
               int i = SendDlgItemMessage(hwnd, IDC_COMBO_DEVICES, CB_GETCURSEL, 0, 0);
               C_Adapter *dp = &(*dde)[i];
                              //save dd device
               config.adapter_identifier = dp->adapter_identifier;

               //SetDevices(hwnd, dp);
               //i = SelectDevice(hwnd, dp, dde);
               //if(i != -1)
               {
                  SetModes(hwnd, dp);
                  SelectMode(hwnd, dp, dde);
               }
               EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
            }
            break;
         }
         break;

      default:
         EnableWindow(GetDlgItem(hwnd_dlg, ID_APPLY), true);
         break;
      }
      break;
   }
   return 0;
}

//----------------------------

static BOOL CALLBACK DlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         InitDlg(hwnd);
         hwnd_dlg = hwnd;
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         C_Enum *dde = (C_Enum*)lParam;

         {
            hwnd_tc = GetDlgItem(hwnd, IDC_TAB_MAIN);
            assert(hwnd_tc);

            POINT tc_pos, tc_size;
            dword tab_height;
            {
               RECT rc;
               GetWindowRect(hwnd_tc, &rc);
               tc_size.x = rc.right - rc.left;
               tc_size.y = rc.bottom - rc.top;

               tc_pos.x = rc.left;
               tc_pos.y = rc.top;
               ScreenToClient(hwnd, &tc_pos);

               SendMessage(hwnd_tc, TCM_GETITEMRECT, 0, (LPARAM)&rc);
               tab_height = (rc.bottom - rc.top);
            }

            for(dword i=0; i<SHEET_LAST; i++){
               if(i==SHEET_ADVANCED && !all_features)
                  break;
               HWND hsht = CreateDialogParam(hInstance, sheet_data[i].dlg_name, hwnd, DlgSheet, lParam);
               assert(hsht);
               hwnd_sheet[i] = hsht;
               SetParent(hsht, hwnd);
               TCITEM tci;
               memset(&tci, 0, sizeof(tci));
               tci.mask = TCIF_TEXT;
               //tci.pszText = (char*)sheet_data[i].sheet_name;
               tci.pszText = (char*)(const char*)all_txt[27+i];
               tci.lParam = (LPARAM)hsht;

               SendMessage(hwnd_tc, TCM_INSERTITEM, i, (LPARAM)&tci);

               SetWindowPos(hsht, NULL, tc_pos.x + 10, tc_pos.y + tab_height + 6, 0, 0, SWP_NOSIZE);

               switch(i){
               case SHEET_VIDEO:
                  {
                     CheckDlgButton(hsht, IDC_CHECK_TXT_COMPR, config.compress_textures);
                     //CheckDlgButton(hwnd, IDC_RADIO_LINEAR, (config.filter==S_game_configuration::FILTER_LINEAR));
                     //CheckDlgButton(hwnd, IDC_RADIO_ANISO, (config.filter==S_game_configuration::FILTER_ANISO));

                     CheckDlgButton(hsht, IDC_CHECK_TRIPPLEBUF, config.tripple_buffering);
                     CheckDlgButton(hsht, IDC_CHECK_FULLSCREEN, config.fullscreen);

                     if(!config.fullscreen){
                        EnableWindow(GetDlgItem(hsht, IDC_CHECK_TRIPPLEBUF), false);
                        EnableWindow(GetDlgItem(hsht, IDC_COMBO_DEPTHS), false);
                     }

                                          //feed all devices
                     for(int ii=0; ii<dde->Size(); ii++){
                        C_Adapter *ddp=&(*dde)[ii];
                        SendDlgItemMessage(hsht, IDC_COMBO_DEVICES, CB_ADDSTRING,
                           0, (LPARAM)(const char*)ddp->Name());
                     }
                     ii = SelectAdapter(hsht, dde);
                     C_Adapter *dp = &(*dde)[ii];

                     //SetDevices(hwnd, dp);
                     //ii = SelectDevice(hsht, dp, dde);
                     //if(ii != -1)
                     {
                        SetModes(hsht, dp);
                        SelectMode(hsht, dp, dde);
                     }

                                          //feed antialias mode
                     {
                        for(int i=0; i<3; i++){
                           SendDlgItemMessage(hsht, IDC_ANTIALIAS, CB_ADDSTRING, 0, (LPARAM)(const char*)all_txt[21+i]);
                        }
                        SendDlgItemMessage(hsht, IDC_ANTIALIAS, CB_SETCURSEL, config.antialias_mode, 0);
                     }
                  }
                  break;

               case SHEET_SOUND:
                  {
                     CheckDlgButton(hsht, IDC_CHECK_SOUNDS, config.use_sounds);
                     CheckDlgButton(hsht, IDC_CHECK_USE_SOUND_HW, config.sounds_use_3D_hw);
                     CheckDlgButton(hsht, IDC_CHECK_USE_EAX, config.sounds_use_EAX);

                     if(!config.use_sounds){
                        EnableWindow(GetDlgItem(hsht, IDC_CHECK_USE_SOUND_HW), false);
                        EnableWindow(GetDlgItem(hsht, IDC_CHECK_USE_EAX), false);
                     }else
                     if(!config.sounds_use_3D_hw)
                        EnableWindow(GetDlgItem(hsht, IDC_CHECK_USE_EAX), false);

                     struct S_hlp{
                        static BOOL CALLBACK cbEnum(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext){
                           if(!lpGuid){
                              C_str &str = *(C_str*)lpContext;
                              str = lpcstrDescription;
                              return false;
                           }
                           return true;
                        }
                     };
                     C_str str;
                     DirectSoundEnumerate(S_hlp::cbEnum, &str);
                     if(str.Size()){
                        SetDlgItemText(hsht, IDC_SND_DEVICE_NAME, str);
                     }else{
                        ShowWindow(GetDlgItem(hsht, IDC_SOUND_DEV_TXT), SW_HIDE);
                        ShowWindow(GetDlgItem(hsht, IDC_SND_DEVICE_NAME), SW_HIDE);
                     }
                  }
                  break;

               case SHEET_ADVANCED:
                  {
                     CheckDlgButton(hsht, IDC_CHECK_DBASE, config.use_dbase);
                     CheckDlgButton(hsht, IDC_CHECK_REF_DEVICE, config.use_reference_device);
                     CheckDlgButton(hsht, IDC_CHECK_NO_VSHADER, config.disable_vshader);
                     CheckDlgButton(hsht, IDC_CHECK_NO_PSHADER, config.disable_pshader);

                     EnableWindow(GetDlgItem(hsht, IDC_CHECK_NO_PSHADER), !config.disable_vshader);
                  }
                  break;
               }
            }
            ShowWindow(hwnd_sheet[0], SW_SHOW);
            SendMessage(hwnd_tc, TCM_SETCURSEL, 0, 0);

         //*
                              //init controls' texts
         struct S_ctrl_text{
            int ctrl_id;
            int text_id;
            int sheet_i;
         } ctrl_texts[] = {
            { IDC_DISPLAY, 1, 0},
            { IDC_3DDEV, 2, 0},
            { IDC_TXT_RES, 3, 0},
            { IDC_TXT_DEPTH, 4, 0},
            { IDOK, 5, -1},
            { IDCANCEL, 6, -1},
            { ID_APPLY, 7, -1},
            { IDC_TXT_SETTINGS, 8, 0},
            { IDC_TXT_SETTINGS, 8, 1},
            { IDC_CHECK_FULLSCREEN, 9, 0},
            { IDC_CHECK_TRIPPLEBUF, 10, 0},
            { IDC_CHECK_SOUNDS, 11, 1},
            { IDC_CHECK_TXT_COMPR, 12, 0},
            { IDC_CHECK_DBASE, 13, 2},
            { IDC_TXT_TXFILTER, 14, 0},
            { IDC_TXT_ANTIALIAS, 20, 0},
            { IDC_SOUND_DEV_TXT, 24, 1},
            { IDC_CHECK_USE_SOUND_HW, 25, 1},
            { IDC_CHECK_USE_EAX, 26, 1},
            0
         };
         for(i=0; ctrl_texts[i].ctrl_id; i++){
            int tid = ctrl_texts[i].text_id;
            bool wide = all_txt.IsWide(tid);
            int id = ctrl_texts[i].ctrl_id;
            int si = ctrl_texts[i].sheet_i;
            HWND hw = si==-1 ? hwnd : hwnd_sheet[si];
            if(!wide){
               SetDlgItemText(hw, id, all_txt[tid]);
            }else{
               HFONT hfnt = japCreateFontJ(12, 12);
               SendDlgItemMessage(hw, id, WM_SETFONT, (WPARAM)hfnt, true);
               SetDlgItemTextW(hw, id, (wchar_t*)(const char*)all_txt[tid]);
            }
         }
         /**/
         }
         {
            HICON hc = LoadIcon(dde->hi_dll, MAKEINTRESOURCE(1));
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hc);
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hc);
         }

         EnableWindow(GetDlgItem(hwnd, ID_APPLY), first_adapter_init);

         ShowWindow(hwnd, SW_SHOW);
         UpdateWindow(hwnd);
      }
      return 1;

      /*
   case WM_HELP:
      {
         LPHELPINFO hi = (LPHELPINFO)lParam;
         DisplayHelp((HWND)hi->hItemHandle, (word)hi->iCtrlId, help_data);
      }
      break;
      */

   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case ID_APPLY:
         {
            C_Enum *dde = (C_Enum*)GetWindowLong(hwnd, GWL_USERDATA);
            SaveChanges(dde);
            EnableWindow(GetDlgItem(hwnd, LOWORD(wParam)), 0);
            SetFocus(GetDlgItem(hwnd, IDOK));
         }
         break;

      case IDOK:
         if(HIWORD(wParam)==BN_CLICKED){
            C_Enum *dde = (C_Enum*)GetWindowLong(hwnd, GWL_USERDATA);
            SaveChanges(dde);
            EndDialog(hwnd, 1);
         }
         break;

      case IDCANCEL:
         EndDialog(hwnd, 0);
         break;
      }
      break;

   case WM_NOTIFY:
      switch(wParam){
      case IDC_TAB_MAIN:
         {
            LPNMHDR hdr = (LPNMHDR)lParam;
            switch(hdr->code){
            case TCN_SELCHANGING:
               {
                  int curr = SendMessage(hwnd_tc, TCM_GETCURSEL, 0, 0);
                  if(curr!=-1)
                     ShowWindow(hwnd_sheet[curr], SW_HIDE);
                  SetFocus(hwnd_tc);
               }
               break;
            case TCN_SELCHANGE:
               {
                  int curr = SendMessage(hwnd_tc, TCM_GETCURSEL, 0, 0);
                  if(curr!=-1){
                     ShowWindow(hwnd_sheet[curr], SW_SHOW);
                     //SetFocus(hwnd_sheet[curr]);
                  }
               }
               break;

            case TCN_KEYDOWN:
               {
                  NMTCKEYDOWN *kd = (NMTCKEYDOWN*)lParam;
                  switch(kd->wVKey){
                  case VK_PRIOR:
                  case VK_NEXT:
                     {
                        int num_tabs = SendMessage(hwnd_tc, TCM_GETITEMCOUNT, 0, 0);
                        if(num_tabs>1){
                           int curr_sel = SendMessage(hwnd_tc, TCM_GETCURSEL, 0, 0);
                           (kd->wVKey==VK_PRIOR) ? --curr_sel : ++curr_sel;
                           if(curr_sel<0) curr_sel += num_tabs;
                           curr_sel %= num_tabs;
                           SendMessage(hwnd_tc, TCM_SETCURSEL, curr_sel, 0);
                        }
                     }
                     break;
                  }
               }
               break;
            }
         }
      }
      break;

   case WM_DESTROY:
      {
         for(dword i=0; i<SHEET_LAST; i++){
            if(hwnd_sheet[i]){
               bool b;
               b = DestroyWindow(hwnd_sheet[i]);
               assert(b);
               hwnd_sheet[i] = NULL;
            }
         }
         hwnd_dlg = NULL;
      }
      break;
   }

   return 0;
}

//----------------------------

extern"C"{
E_CONFIG_STATUS __declspec(dllexport) RunConfig(HINSTANCE hInstance, const char *reg_base, bool all, const char *lang);
}

E_CONFIG_STATUS __declspec(dllexport) RunConfig(HINSTANCE hInstance, const char *reg_base, bool all, const char *lang){

   C_Enum dd_enum;
   dd_enum.hi_dll = hInstance;
   ::hInstance = hInstance;

                              //command-line processing
   if(all){
                              //allow all modes
      limit_mode_x = 0;
      limit_mode_y = 0;
      all_features = true;
   }

                              //read localized texts
   if(!lang)
      lang ="english";

   HRSRC hsrc;
   hsrc = FindResource(hInstance, lang, "TEXTS");
   if(!hsrc)
      hsrc = FindResource(hInstance, "english", "TEXTS");
   bool txt_ok = false;
   if(hsrc){
      HGLOBAL hgl;
      hgl = LoadResource(hInstance, hsrc);
      if(hgl){
         void *vp = LockResource(hgl);
         if(vp){
            dword sz = SizeofResource(hInstance, hsrc);
            C_cache is;
            if(is.open((byte**)&vp, &sz, CACHE_READ_MEM)){
               if(all_txt.AddFile(is))
                  txt_ok = true;
               is.close();
            }
         }
      }
   }
   if(!txt_ok){
      MessageBox(NULL, "Failed to open text file.", APP_NAME, MB_OK);
      return CONFIG_ERR_NO_DLL;
   }

   ::reg_base = reg_base;
   config.ReadSysConfig(reg_base);

   E_CONFIG_STATUS ret;
   C_str err;
   if(FAILED(dd_enum.EnumAll(err))){
      MessageBox(NULL,
         C_fstr("Unable to query DirectX 9.\nPlease install newer version of DirectX.\n\nError details:\n%s.", (const char*)err),
         APP_NAME, MB_OK);
      ret = CONFIG_INIT_FAIL;
   }else{
      bool ok = DialogBoxParam(hInstance, "IDD_DIALOG_MAIN", NULL, (DLGPROC)DlgProc,
         (LPARAM)&dd_enum);
      ret = ok ? CONFIG_OK : CONFIG_CANCELED;
   }

   dd_enum.Clear();
   return ret;
}

//----------------------------
