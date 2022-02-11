#include "all.h"
#include "igraph_i.h"
#include <math.h>
#include <malloc.h>
#include <mmsystem.h>
#include <win_reg.h>

//----------------------------

                              //disabled - problems after short times in _get_timer
#define USE_QUERYPERFORMANCECOUNTER

//#ifdef NDEBUG
// #define MOUSE_USE_EVENT      //it makes problems!
//#endif

//----------------------------

C_rgb_conversion *Create_rgb_conv();

//----------------------------

#ifdef LOG_MODE
C_log IG_log;
#endif                        //LOG_MODE

//----------------------------

static void DI_Fatal(const char *text, HRESULT hr){

   char *msgp;

   switch(hr){
   case DI_DOWNLOADSKIPPED: msgp = "DI_DOWNLOADSKIPPED"; break;
   case DI_EFFECTRESTARTED: msgp = "DI_EFFECTRESTARTED"; break;
   case DI_OK: msgp = "DI_OK"; break;
   case DI_POLLEDDEVICE: msgp = "DI_POLLEDDEVICE"; break;
   case DI_TRUNCATED: msgp = "DI_TRUNCATED"; break;
   case DI_TRUNCATEDANDRESTARTED: msgp = "DI_TRUNCATEDANDRESTARTED"; break;
   case DIERR_ACQUIRED: msgp = "DIERR_ACQUIRED"; break;
   case DIERR_ALREADYINITIALIZED: msgp = "DIERR_ALREADYINITIALIZED"; break;
   case DIERR_BADDRIVERVER: msgp = "DIERR_BADDRIVERVER"; break;
   case DIERR_BETADIRECTINPUTVERSION: msgp = "DIERR_BETADIRECTINPUTVERSION"; break;
   case DIERR_DEVICEFULL: msgp = "DIERR_DEVICEFULL"; break;
   case DIERR_DEVICENOTREG: msgp = "DIERR_DEVICENOTREG"; break;
   case DIERR_EFFECTPLAYING: msgp = "DIERR_EFFECTPLAYING"; break;
   case DIERR_HASEFFECTS: msgp = "DIERR_HASEFFECTS"; break;
   case DIERR_GENERIC: msgp = "DIERR_GENERIC"; break;
   case DIERR_HANDLEEXISTS: msgp = "DIERR_HANDLEEXISTS"; break;
   case DIERR_INCOMPLETEEFFECT: msgp = "DIERR_INCOMPLETEEFFECT"; break;
   case DIERR_INPUTLOST: msgp = "DIERR_INPUTLOST"; break;
   case DIERR_INVALIDPARAM: msgp = "DIERR_INVALIDPARAM"; break;
   case DIERR_MOREDATA: msgp = "DIERR_MOREDATA"; break;
   case DIERR_NOAGGREGATION: msgp = "DIERR_NOAGGREGATION"; break;
   case DIERR_NOINTERFACE: msgp = "DIERR_NOINTERFACE"; break;
   case DIERR_NOTACQUIRED: msgp = "DIERR_NOTACQUIRED"; break;
   case DIERR_NOTBUFFERED: msgp = "DIERR_NOTBUFFERED"; break;
   case DIERR_NOTDOWNLOADED: msgp = "DIERR_NOTDOWNLOADED"; break;
   case DIERR_NOTEXCLUSIVEACQUIRED: msgp = "DIERR_NOTEXCLUSIVEACQUIRED"; break;
   case DIERR_NOTFOUND: msgp = "DIERR_NOTFOUND"; break;
   case DIERR_NOTINITIALIZED: msgp = "DIERR_NOTINITIALIZED"; break;
   //case DIERR_OBJECTNOTFOUND: msgp = "DIERR_OBJECTNOTFOUND"; break;
   case DIERR_OLDDIRECTINPUTVERSION: msgp = "DIERR_OLDDIRECTINPUTVERSION"; break;
   //case DIERR_OTHERAPPHASPRIO: msgp = "DIERR_OTHERAPPHASPRIO"; break;
   case DIERR_OUTOFMEMORY: msgp = "DIERR_OUTOFMEMORY"; break;
   //case DIERR_READONLY: msgp = "DIERR_READONLY"; break;
   case DIERR_REPORTFULL: msgp = "DIERR_UNPLUGGED"; break;
   case DIERR_UNPLUGGED: msgp = "DIERR_UNPLUGGED"; break;
   case DIERR_UNSUPPORTED: msgp = "DIERR_UNSUPPORTED"; break;
   case E_HANDLE: msgp = "E_HANDLE"; break;
   case E_PENDING: msgp = "E_PENDING"; break;
   default: msgp="Unknown Error";
   }
   MessageBox(NULL, C_fstr("%s: %s", text, msgp), "DirectInput", MB_OK);
}

#ifndef NDEBUG
#define CHECK_DI_RESULT(text, hr) if(FAILED(hr)) DI_Fatal(text, hr);
#else
#define CHECK_DI_RESULT(text, hr)
#endif

#ifndef NDEBUG
#define CHECK_DD_RESULT(text, hr) if(FAILED(hr)) DD_Fatal(text, hr);
#else
#define CHECK_DD_RESULT(text, hr)
#endif

//----------------------------
#ifndef GL
static void DumpCaps(IDirect3D9 *lpD3D9, IDirect3DDevice9 *lpDev9,
   IG_INIT::t_log_func *log_func){

   HRESULT hr;

   D3DCAPS9 cp;
   hr = lpDev9->GetDeviceCaps(&cp);
   CHECK_D3D_RESULT("GetDeviceCaps", hr);
   if(FAILED(hr)) return;

   D3DADAPTER_IDENTIFIER9 ai;
   hr = lpD3D9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, D3DENUM_WHQL_LEVEL, &ai);
   CHECK_D3D_RESULT("GetAdapterIdentifier", hr);
   if(FAILED(hr)) return;

   (*log_func)("----------------------");
   (*log_func)("DirectX 9 Device Caps:");

   {
      (*log_func)(C_fstr("Driver      : %s", ai.Driver));
      (*log_func)(C_fstr("Description : %s", ai.Description));
      (*log_func)(C_fstr("Version     : %u.%u.%u.%u", HIWORD(ai.DriverVersion.HighPart),
         LOWORD(ai.DriverVersion.HighPart), HIWORD(ai.DriverVersion.LowPart),
         LOWORD(ai.DriverVersion.LowPart)));
      (*log_func)(C_fstr("Vendor ID   : %u", ai.VendorId));
      (*log_func)(C_fstr("Device ID   : %u", ai.DeviceId));
      (*log_func)(C_fstr("Subsystem ID: %u", ai.SubSysId));
      (*log_func)(C_fstr("Revision    : %u", ai.Revision));
      (*log_func)(C_fstr("dwWHQLLevel : %u", ai.WHQLLevel));
   }
   {
      const char *cpp;
      switch(cp.DeviceType){
      case D3DDEVTYPE_HAL: cpp = "HAL"; break;
      case D3DDEVTYPE_REF: cpp = "REF"; break;
      case D3DDEVTYPE_SW: cpp = "SW"; break;
      default: assert(0); cpp = "unknown";
      }
      (*log_func)(C_fstr("DeviceType: %s", cpp));
   }
   {
      (*log_func)(C_fstr("AdapterOrdinal: %i", cp.AdapterOrdinal));
   }
   {
      (*log_func)("Caps:");
      dword dw = cp.Caps;
      if(dw&D3DCAPS_READ_SCANLINE) (*log_func)(" D3DCAPS_READ_SCANLINE");
   }
   {
      (*log_func)("Caps2:");
      dword dw = cp.Caps2;
      if(dw&D3DCAPS2_CANAUTOGENMIPMAP) (*log_func)(" D3DCAPS2_CANAUTOGENMIPMAP");
      if(dw&D3DCAPS2_CANCALIBRATEGAMMA) (*log_func)(" D3DCAPS2_CANCALIBRATEGAMMA");
      if(dw&D3DCAPS2_CANMANAGERESOURCE) (*log_func)(" D3DCAPS2_CANMANAGERESOURCE");
      if(dw&D3DCAPS2_DYNAMICTEXTURES) (*log_func)(" D3DCAPS2_DYNAMICTEXTURES");
      if(dw&D3DCAPS2_FULLSCREENGAMMA) (*log_func)(" D3DCAPS2_FULLSCREENGAMMA");
   }
   {
      (*log_func)("Caps3:");
      dword dw = cp.Caps3;
      if(dw&D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD) (*log_func)(" D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD");
      if(dw&D3DCAPS3_COPY_TO_VIDMEM) (*log_func)(" D3DCAPS3_COPY_TO_VIDMEM");
      if(dw&D3DCAPS3_COPY_TO_SYSTEMMEM) (*log_func)(" D3DCAPS3_COPY_TO_SYSTEMMEM");
      if(dw&D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION) (*log_func)(" D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION");
   }
   {
      (*log_func)("PresentationIntervals:");
      dword dw = cp.PresentationIntervals;
      if(dw&D3DPRESENT_INTERVAL_IMMEDIATE) (*log_func)(" D3DPRESENT_INTERVAL_IMMEDIATE");
      if(dw&D3DPRESENT_INTERVAL_ONE) (*log_func)(" D3DPRESENT_INTERVAL_ONE");
      if(dw&D3DPRESENT_INTERVAL_TWO) (*log_func)(" D3DPRESENT_INTERVAL_TWO");
      if(dw&D3DPRESENT_INTERVAL_THREE) (*log_func)(" D3DPRESENT_INTERVAL_THREE");
      if(dw&D3DPRESENT_INTERVAL_FOUR) (*log_func)(" D3DPRESENT_INTERVAL_FOUR");
   }
   {
      (*log_func)("CursorCaps:");
      dword dw = cp.CursorCaps;
      if(dw&D3DCURSORCAPS_COLOR) (*log_func)(" D3DCURSORCAPS_COLOR");
      if(dw&D3DCURSORCAPS_LOWRES) (*log_func)(" D3DCURSORCAPS_LOWRES");
   }
   {
      (*log_func)("DevCaps:");
      dword dw = cp.DevCaps;
      if(dw&D3DDEVCAPS_CANBLTSYSTONONLOCAL) (*log_func)(" D3DDEVCAPS_CANBLTSYSTONONLOCAL");
      if(dw&D3DDEVCAPS_CANRENDERAFTERFLIP) (*log_func)(" D3DDEVCAPS_CANRENDERAFTERFLIP");
      if(dw&D3DDEVCAPS_DRAWPRIMITIVES2) (*log_func)(" D3DDEVCAPS_DRAWPRIMITIVES2");
      if(dw&D3DDEVCAPS_DRAWPRIMITIVES2EX) (*log_func)(" D3DDEVCAPS_DRAWPRIMITIVES2EX");

      if(dw&D3DDEVCAPS_DRAWPRIMTLVERTEX) (*log_func)(" D3DDEVCAPS_DRAWPRIMTLVERTEX");
      if(dw&D3DDEVCAPS_EXECUTESYSTEMMEMORY) (*log_func)(" D3DDEVCAPS_EXECUTESYSTEMMEMORY");
      if(dw&D3DDEVCAPS_EXECUTEVIDEOMEMORY) (*log_func)(" D3DDEVCAPS_EXECUTEVIDEOMEMORY");
      if(dw&D3DDEVCAPS_HWRASTERIZATION) (*log_func)(" D3DDEVCAPS_HWRASTERIZATION");

      if(dw&D3DDEVCAPS_HWTRANSFORMANDLIGHT) (*log_func)(" D3DDEVCAPS_HWTRANSFORMANDLIGHT");
      if(dw&D3DDEVCAPS_NPATCHES) (*log_func)(" D3DDEVCAPS_NPATCHES");
      if(dw&D3DDEVCAPS_PUREDEVICE) (*log_func)(" D3DDEVCAPS_PUREDEVICE");
      if(dw&D3DDEVCAPS_QUINTICRTPATCHES) (*log_func)(" D3DDEVCAPS_QUINTICRTPATCHES");

      if(dw&D3DDEVCAPS_RTPATCHES) (*log_func)(" D3DDEVCAPS_RTPATCHES");
      if(dw&D3DDEVCAPS_RTPATCHHANDLEZERO) (*log_func)(" D3DDEVCAPS_RTPATCHHANDLEZERO");
      if(dw&D3DDEVCAPS_SEPARATETEXTUREMEMORIES) (*log_func)(" D3DDEVCAPS_SEPARATETEXTUREMEMORIES");
      if(dw&D3DDEVCAPS_TEXTURENONLOCALVIDMEM) (*log_func)(" D3DDEVCAPS_TEXTURENONLOCALVIDMEM");

      if(dw&D3DDEVCAPS_TEXTURESYSTEMMEMORY) (*log_func)(" D3DDEVCAPS_TEXTURESYSTEMMEMORY");
      if(dw&D3DDEVCAPS_TEXTUREVIDEOMEMORY) (*log_func)(" D3DDEVCAPS_TEXTUREVIDEOMEMORY");
      if(dw&D3DDEVCAPS_TLVERTEXSYSTEMMEMORY) (*log_func)(" D3DDEVCAPS_TLVERTEXSYSTEMMEMORY");
      if(dw&D3DDEVCAPS_TLVERTEXVIDEOMEMORY) (*log_func)(" D3DDEVCAPS_TLVERTEXVIDEOMEMORY");
   }
   {
      (*log_func)("PrimitiveMiscCaps:");
      dword dw = cp.PrimitiveMiscCaps;
      if(dw&D3DPMISCCAPS_MASKZ) (*log_func)(" D3DPMISCCAPS_MASKZ");
      if(dw&D3DPMISCCAPS_CULLCCW) (*log_func)(" D3DPMISCCAPS_CULLCCW");
      if(dw&D3DPMISCCAPS_CULLCW) (*log_func)(" D3DPMISCCAPS_CULLCW");
      if(dw&D3DPMISCCAPS_CULLNONE) (*log_func)(" D3DPMISCCAPS_CULLNONE");

      if(dw&D3DPMISCCAPS_COLORWRITEENABLE) (*log_func)(" D3DPMISCCAPS_COLORWRITEENABLE");
      if(dw&D3DPMISCCAPS_CLIPPLANESCALEDPOINTS) (*log_func)(" D3DPMISCCAPS_CLIPPLANESCALEDPOINTS");
      if(dw&D3DPMISCCAPS_CLIPTLVERTS) (*log_func)(" D3DPMISCCAPS_CLIPTLVERTS");
      if(dw&D3DPMISCCAPS_TSSARGTEMP) (*log_func)(" D3DPMISCCAPS_TSSARGTEMP");

      if(dw&D3DPMISCCAPS_BLENDOP) (*log_func)(" D3DPMISCCAPS_BLENDOP");
      if(dw&D3DPMISCCAPS_NULLREFERENCE) (*log_func)(" D3DPMISCCAPS_NULLREFERENCE");
      if(dw&D3DPMISCCAPS_INDEPENDENTWRITEMASKS) (*log_func)(" D3DPMISCCAPS_INDEPENDENTWRITEMASKS");
      if(dw&D3DPMISCCAPS_PERSTAGECONSTANT) (*log_func)(" D3DPMISCCAPS_PERSTAGECONSTANT");

      if(dw&D3DPMISCCAPS_FOGANDSPECULARALPHA) (*log_func)(" D3DPMISCCAPS_FOGANDSPECULARALPHA");
      if(dw&D3DPMISCCAPS_SEPARATEALPHABLEND) (*log_func)(" D3DPMISCCAPS_SEPARATEALPHABLEND");
      if(dw&D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS) (*log_func)(" D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS");
      if(dw&D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING) (*log_func)(" D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING");

      if(dw&D3DPMISCCAPS_FOGVERTEXCLAMPED) (*log_func)(" D3DPMISCCAPS_FOGVERTEXCLAMPED");
   }
   {
      (*log_func)("RasterCaps:");
      dword dw = cp.RasterCaps;
      if(dw&D3DPRASTERCAPS_ANISOTROPY) (*log_func)(" D3DPRASTERCAPS_ANISOTROPY");
      if(dw&D3DPRASTERCAPS_COLORPERSPECTIVE) (*log_func)(" D3DPRASTERCAPS_COLORPERSPECTIVE");
      if(dw&D3DPRASTERCAPS_DITHER) (*log_func)(" D3DPRASTERCAPS_DITHER");
      if(dw&D3DPRASTERCAPS_DEPTHBIAS) (*log_func)(" D3DPRASTERCAPS_DEPTHBIAS");

      if(dw&D3DPRASTERCAPS_FOGRANGE) (*log_func)(" D3DPRASTERCAPS_FOGRANGE");
      if(dw&D3DPRASTERCAPS_FOGTABLE) (*log_func)(" D3DPRASTERCAPS_FOGTABLE");
      if(dw&D3DPRASTERCAPS_FOGVERTEX) (*log_func)(" D3DPRASTERCAPS_FOGVERTEX");
      if(dw&D3DPRASTERCAPS_MIPMAPLODBIAS) (*log_func)(" D3DPRASTERCAPS_MIPMAPLODBIAS");

      if(dw&D3DPRASTERCAPS_MULTISAMPLE_TOGGLE) (*log_func)(" D3DPRASTERCAPS_MULTISAMPLE_TOGGLE");
      if(dw&D3DPRASTERCAPS_SCISSORTEST) (*log_func)(" D3DPRASTERCAPS_SCISSORTEST");
      if(dw&D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS) (*log_func)(" D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS");
      if(dw&D3DPRASTERCAPS_WBUFFER) (*log_func)(" D3DPRASTERCAPS_WBUFFER");

      if(dw&D3DPRASTERCAPS_WFOG) (*log_func)(" D3DPRASTERCAPS_WFOG");
      if(dw&D3DPRASTERCAPS_ZBUFFERLESSHSR) (*log_func)(" D3DPRASTERCAPS_ZBUFFERLESSHSR");
      if(dw&D3DPRASTERCAPS_ZFOG) (*log_func)(" D3DPRASTERCAPS_ZFOG");
      if(dw&D3DPRASTERCAPS_ZTEST) (*log_func)(" D3DPRASTERCAPS_ZTEST");
   }
   {
      (*log_func)("ZCmpCaps:");
      dword dw = cp.ZCmpCaps;
      if(dw&D3DPCMPCAPS_ALWAYS) (*log_func)(" D3DPCMPCAPS_ALWAYS");
      if(dw&D3DPCMPCAPS_EQUAL) (*log_func)(" D3DPCMPCAPS_EQUAL");
      if(dw&D3DPCMPCAPS_GREATER) (*log_func)(" D3DPCMPCAPS_GREATER");
      if(dw&D3DPCMPCAPS_GREATEREQUAL) (*log_func)(" D3DPCMPCAPS_GREATEREQUAL");
      if(dw&D3DPCMPCAPS_LESS) (*log_func)(" D3DPCMPCAPS_LESS");
      if(dw&D3DPCMPCAPS_LESSEQUAL) (*log_func)(" D3DPCMPCAPS_LESSEQUAL");
      if(dw&D3DPCMPCAPS_NEVER) (*log_func)(" D3DPCMPCAPS_NEVER");
      if(dw&D3DPCMPCAPS_NOTEQUAL) (*log_func)(" D3DPCMPCAPS_NOTEQUAL");
   }
   {
      (*log_func)("SrcBlendCaps:");
      dword dw = cp.SrcBlendCaps;
      if(dw&D3DPBLENDCAPS_BLENDFACTOR) (*log_func)(" D3DPBLENDCAPS_BLENDFACTOR");
      if(dw&D3DPBLENDCAPS_BOTHINVSRCALPHA) (*log_func)(" D3DPBLENDCAPS_BOTHINVSRCALPHA");
      if(dw&D3DPBLENDCAPS_BOTHSRCALPHA) (*log_func)(" D3DPBLENDCAPS_BOTHSRCALPHA");
      if(dw&D3DPBLENDCAPS_DESTALPHA) (*log_func)(" D3DPBLENDCAPS_DESTALPHA");

      if(dw&D3DPBLENDCAPS_DESTCOLOR) (*log_func)(" D3DPBLENDCAPS_DESTCOLOR");
      if(dw&D3DPBLENDCAPS_INVDESTALPHA) (*log_func)(" D3DPBLENDCAPS_INVDESTALPHA");
      if(dw&D3DPBLENDCAPS_INVDESTCOLOR) (*log_func)(" D3DPBLENDCAPS_INVDESTCOLOR");
      if(dw&D3DPBLENDCAPS_INVSRCALPHA) (*log_func)(" D3DPBLENDCAPS_INVSRCALPHA");

      if(dw&D3DPBLENDCAPS_INVSRCCOLOR) (*log_func)(" D3DPBLENDCAPS_INVSRCCOLOR");
      if(dw&D3DPBLENDCAPS_ONE) (*log_func)(" D3DPBLENDCAPS_ONE");
      if(dw&D3DPBLENDCAPS_SRCALPHA) (*log_func)(" D3DPBLENDCAPS_SRCALPHA");
      if(dw&D3DPBLENDCAPS_SRCALPHASAT) (*log_func)(" D3DPBLENDCAPS_SRCALPHASAT");

      if(dw&D3DPBLENDCAPS_SRCCOLOR) (*log_func)(" D3DPBLENDCAPS_SRCCOLOR");
      if(dw&D3DPBLENDCAPS_ZERO) (*log_func)(" D3DPBLENDCAPS_ZERO");
   }
   {
      (*log_func)("DestBlendCaps:");
      dword dw = cp.DestBlendCaps;
      if(dw&D3DPBLENDCAPS_BLENDFACTOR) (*log_func)(" D3DPBLENDCAPS_BLENDFACTOR");
      if(dw&D3DPBLENDCAPS_BOTHINVSRCALPHA) (*log_func)(" D3DPBLENDCAPS_BOTHINVSRCALPHA");
      if(dw&D3DPBLENDCAPS_BOTHSRCALPHA) (*log_func)(" D3DPBLENDCAPS_BOTHSRCALPHA");
      if(dw&D3DPBLENDCAPS_DESTALPHA) (*log_func)(" D3DPBLENDCAPS_DESTALPHA");

      if(dw&D3DPBLENDCAPS_DESTCOLOR) (*log_func)(" D3DPBLENDCAPS_DESTCOLOR");
      if(dw&D3DPBLENDCAPS_INVDESTALPHA) (*log_func)(" D3DPBLENDCAPS_INVDESTALPHA");
      if(dw&D3DPBLENDCAPS_INVDESTCOLOR) (*log_func)(" D3DPBLENDCAPS_INVDESTCOLOR");
      if(dw&D3DPBLENDCAPS_INVSRCALPHA) (*log_func)(" D3DPBLENDCAPS_INVSRCALPHA");

      if(dw&D3DPBLENDCAPS_INVSRCCOLOR) (*log_func)(" D3DPBLENDCAPS_INVSRCCOLOR");
      if(dw&D3DPBLENDCAPS_ONE) (*log_func)(" D3DPBLENDCAPS_ONE");
      if(dw&D3DPBLENDCAPS_SRCALPHA) (*log_func)(" D3DPBLENDCAPS_SRCALPHA");
      if(dw&D3DPBLENDCAPS_SRCALPHASAT) (*log_func)(" D3DPBLENDCAPS_SRCALPHASAT");

      if(dw&D3DPBLENDCAPS_SRCCOLOR) (*log_func)(" D3DPBLENDCAPS_SRCCOLOR");
      if(dw&D3DPBLENDCAPS_ZERO) (*log_func)(" D3DPBLENDCAPS_ZERO");
   }
   {
      (*log_func)("AlphaCmpCaps:");
      dword dw = cp.AlphaCmpCaps;
      if(dw&D3DPCMPCAPS_ALWAYS) (*log_func)(" D3DPCMPCAPS_ALWAYS");
      if(dw&D3DPCMPCAPS_EQUAL) (*log_func)(" D3DPCMPCAPS_EQUAL");
      if(dw&D3DPCMPCAPS_GREATER) (*log_func)(" D3DPCMPCAPS_GREATER");
      if(dw&D3DPCMPCAPS_GREATEREQUAL) (*log_func)(" D3DPCMPCAPS_GREATEREQUAL");
      if(dw&D3DPCMPCAPS_LESS) (*log_func)(" D3DPCMPCAPS_LESS");
      if(dw&D3DPCMPCAPS_LESSEQUAL) (*log_func)(" D3DPCMPCAPS_LESSEQUAL");
      if(dw&D3DPCMPCAPS_NEVER) (*log_func)(" D3DPCMPCAPS_NEVER");
      if(dw&D3DPCMPCAPS_NOTEQUAL) (*log_func)(" D3DPCMPCAPS_NOTEQUAL");
   }
   {
      (*log_func)("ShadeCaps:");
      dword dw = cp.ShadeCaps;
      if(dw&D3DPSHADECAPS_ALPHAGOURAUDBLEND) (*log_func)(" D3DPSHADECAPS_ALPHAGOURAUDBLEND");
      if(dw&D3DPSHADECAPS_COLORGOURAUDRGB) (*log_func)(" D3DPSHADECAPS_COLORGOURAUDRGB");
      if(dw&D3DPSHADECAPS_FOGGOURAUD) (*log_func)(" D3DPSHADECAPS_FOGGOURAUD");
      if(dw&D3DPSHADECAPS_SPECULARGOURAUDRGB) (*log_func)(" D3DPSHADECAPS_SPECULARGOURAUDRGB");
   }
   {
      (*log_func)("TextureCaps:");
      dword dw = cp.TextureCaps;
      if(dw&D3DPTEXTURECAPS_ALPHA) (*log_func)(" D3DPTEXTURECAPS_ALPHA");
      if(dw&D3DPTEXTURECAPS_ALPHAPALETTE) (*log_func)(" D3DPTEXTURECAPS_ALPHAPALETTE");
      if(dw&D3DPTEXTURECAPS_CUBEMAP) (*log_func)(" D3DPTEXTURECAPS_CUBEMAP");
      if(dw&D3DPTEXTURECAPS_CUBEMAP_POW2) (*log_func)(" D3DPTEXTURECAPS_CUBEMAP_POW2");

      if(dw&D3DPTEXTURECAPS_MIPCUBEMAP) (*log_func)(" D3DPTEXTURECAPS_MIPCUBEMAP");
      if(dw&D3DPTEXTURECAPS_MIPMAP) (*log_func)(" D3DPTEXTURECAPS_MIPMAP");
      if(dw&D3DPTEXTURECAPS_MIPVOLUMEMAP) (*log_func)(" D3DPTEXTURECAPS_MIPVOLUMEMAP");
      if(dw&D3DPTEXTURECAPS_NONPOW2CONDITIONAL) (*log_func)(" D3DPTEXTURECAPS_NONPOW2CONDITIONAL");

      if(dw&D3DPTEXTURECAPS_NOPROJECTEDBUMPENV) (*log_func)(" D3DPTEXTURECAPS_NOPROJECTEDBUMPENV");
      if(dw&D3DPTEXTURECAPS_PERSPECTIVE) (*log_func)(" D3DPTEXTURECAPS_PERSPECTIVE");
      if(dw&D3DPTEXTURECAPS_POW2) (*log_func)(" D3DPTEXTURECAPS_POW2");
      if(dw&D3DPTEXTURECAPS_PROJECTED) (*log_func)(" D3DPTEXTURECAPS_PROJECTED");

      if(dw&D3DPTEXTURECAPS_SQUAREONLY) (*log_func)(" D3DPTEXTURECAPS_SQUAREONLY");
      if(dw&D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE) (*log_func)(" D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE");
      if(dw&D3DPTEXTURECAPS_VOLUMEMAP) (*log_func)(" D3DPTEXTURECAPS_VOLUMEMAP");
      if(dw&D3DPTEXTURECAPS_VOLUMEMAP_POW2) (*log_func)(" D3DPTEXTURECAPS_VOLUMEMAP_POW2");
   }
   {
      (*log_func)("TextureFilterCaps:");
      dword dw = cp.TextureFilterCaps;
      if(dw&D3DPTFILTERCAPS_MAGFPOINT) (*log_func)(" D3DPTFILTERCAPS_MAGFPOINT");
      if(dw&D3DPTFILTERCAPS_MAGFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MAGFLINEAR");
      if(dw&D3DPTFILTERCAPS_MAGFANISOTROPIC) (*log_func)(" D3DPTFILTERCAPS_MAGFANISOTROPIC");
      if(dw&D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD) (*log_func)(" D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD");
      
      if(dw&D3DPTFILTERCAPS_MAGFGAUSSIANQUAD) (*log_func)(" D3DPTFILTERCAPS_MAGFGAUSSIANQUAD");
      if(dw&D3DPTFILTERCAPS_MINFPOINT) (*log_func)(" D3DPTFILTERCAPS_MINFPOINT");
      if(dw&D3DPTFILTERCAPS_MINFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MINFLINEAR");
      if(dw&D3DPTFILTERCAPS_MINFANISOTROPIC) (*log_func)(" D3DPTFILTERCAPS_MINFANISOTROPIC");

      if(dw&D3DPTFILTERCAPS_MINFPYRAMIDALQUAD) (*log_func)(" D3DPTFILTERCAPS_MINFPYRAMIDALQUAD");
      if(dw&D3DPTFILTERCAPS_MINFGAUSSIANQUAD) (*log_func)(" D3DPTFILTERCAPS_MINFGAUSSIANQUAD");
      if(dw&D3DPTFILTERCAPS_MIPFPOINT) (*log_func)(" D3DPTFILTERCAPS_MIPFPOINT");
      if(dw&D3DPTFILTERCAPS_MIPFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MIPFLINEAR");
   }
   {
      (*log_func)("CubeTextureFilterCaps:");
      dword dw = cp.CubeTextureFilterCaps;
      if(dw&D3DPTFILTERCAPS_MAGFPOINT) (*log_func)(" D3DPTFILTERCAPS_MAGFPOINT");
      if(dw&D3DPTFILTERCAPS_MAGFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MAGFLINEAR");
      if(dw&D3DPTFILTERCAPS_MAGFANISOTROPIC) (*log_func)(" D3DPTFILTERCAPS_MAGFANISOTROPIC");
      if(dw&D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD) (*log_func)(" D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD");
      
      if(dw&D3DPTFILTERCAPS_MAGFGAUSSIANQUAD) (*log_func)(" D3DPTFILTERCAPS_MAGFGAUSSIANQUAD");
      if(dw&D3DPTFILTERCAPS_MINFPOINT) (*log_func)(" D3DPTFILTERCAPS_MINFPOINT");
      if(dw&D3DPTFILTERCAPS_MINFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MINFLINEAR");
      if(dw&D3DPTFILTERCAPS_MINFANISOTROPIC) (*log_func)(" D3DPTFILTERCAPS_MINFANISOTROPIC");

      if(dw&D3DPTFILTERCAPS_MINFPYRAMIDALQUAD) (*log_func)(" D3DPTFILTERCAPS_MINFPYRAMIDALQUAD");
      if(dw&D3DPTFILTERCAPS_MINFGAUSSIANQUAD) (*log_func)(" D3DPTFILTERCAPS_MINFGAUSSIANQUAD");
      if(dw&D3DPTFILTERCAPS_MIPFPOINT) (*log_func)(" D3DPTFILTERCAPS_MIPFPOINT");
      if(dw&D3DPTFILTERCAPS_MIPFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MIPFLINEAR");
   }
   {
      (*log_func)("VolumeTextureFilterCaps:");
      dword dw = cp.VolumeTextureFilterCaps;
      if(dw&D3DPTFILTERCAPS_MAGFPOINT) (*log_func)(" D3DPTFILTERCAPS_MAGFPOINT");
      if(dw&D3DPTFILTERCAPS_MAGFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MAGFLINEAR");
      if(dw&D3DPTFILTERCAPS_MAGFANISOTROPIC) (*log_func)(" D3DPTFILTERCAPS_MAGFANISOTROPIC");
      if(dw&D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD) (*log_func)(" D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD");
      
      if(dw&D3DPTFILTERCAPS_MAGFGAUSSIANQUAD) (*log_func)(" D3DPTFILTERCAPS_MAGFGAUSSIANQUAD");
      if(dw&D3DPTFILTERCAPS_MINFPOINT) (*log_func)(" D3DPTFILTERCAPS_MINFPOINT");
      if(dw&D3DPTFILTERCAPS_MINFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MINFLINEAR");
      if(dw&D3DPTFILTERCAPS_MINFANISOTROPIC) (*log_func)(" D3DPTFILTERCAPS_MINFANISOTROPIC");

      if(dw&D3DPTFILTERCAPS_MINFPYRAMIDALQUAD) (*log_func)(" D3DPTFILTERCAPS_MINFPYRAMIDALQUAD");
      if(dw&D3DPTFILTERCAPS_MINFGAUSSIANQUAD) (*log_func)(" D3DPTFILTERCAPS_MINFGAUSSIANQUAD");
      if(dw&D3DPTFILTERCAPS_MIPFPOINT) (*log_func)(" D3DPTFILTERCAPS_MIPFPOINT");
      if(dw&D3DPTFILTERCAPS_MIPFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MIPFLINEAR");
   }
   {
      (*log_func)("TextureAddressCaps:");
      dword dw = cp.TextureAddressCaps;
      if(dw&D3DPTADDRESSCAPS_BORDER) (*log_func)(" D3DPTADDRESSCAPS_BORDER");
      if(dw&D3DPTADDRESSCAPS_CLAMP) (*log_func)(" D3DPTADDRESSCAPS_CLAMP");
      if(dw&D3DPTADDRESSCAPS_INDEPENDENTUV) (*log_func)(" D3DPTADDRESSCAPS_INDEPENDENTUV");
      if(dw&D3DPTADDRESSCAPS_MIRROR) (*log_func)(" D3DPTADDRESSCAPS_MIRROR");
      if(dw&D3DPTADDRESSCAPS_MIRRORONCE) (*log_func)(" D3DPTADDRESSCAPS_MIRRORONCE");
      if(dw&D3DPTADDRESSCAPS_WRAP) (*log_func)(" D3DPTADDRESSCAPS_WRAP");
   }
   {
      (*log_func)("VolumeTextureAddressCaps:");
      dword dw = cp.VolumeTextureAddressCaps;
      if(dw&D3DPTADDRESSCAPS_BORDER) (*log_func)(" D3DPTADDRESSCAPS_BORDER");
      if(dw&D3DPTADDRESSCAPS_CLAMP) (*log_func)(" D3DPTADDRESSCAPS_CLAMP");
      if(dw&D3DPTADDRESSCAPS_INDEPENDENTUV) (*log_func)(" D3DPTADDRESSCAPS_INDEPENDENTUV");
      if(dw&D3DPTADDRESSCAPS_MIRROR) (*log_func)(" D3DPTADDRESSCAPS_MIRROR");
      if(dw&D3DPTADDRESSCAPS_MIRRORONCE) (*log_func)(" D3DPTADDRESSCAPS_MIRRORONCE");
      if(dw&D3DPTADDRESSCAPS_WRAP) (*log_func)(" D3DPTADDRESSCAPS_WRAP");
   }
   {
      (*log_func)("LineCaps:");
      dword dw = cp.LineCaps;
      if(dw&D3DLINECAPS_ALPHACMP) (*log_func)(" D3DLINECAPS_ALPHACMP");
      if(dw&D3DLINECAPS_ANTIALIAS) (*log_func)(" D3DLINECAPS_ANTIALIAS");
      if(dw&D3DLINECAPS_BLEND) (*log_func)(" D3DLINECAPS_BLEND");
      if(dw&D3DLINECAPS_FOG) (*log_func)(" D3DLINECAPS_FOG");
      if(dw&D3DLINECAPS_TEXTURE) (*log_func)(" D3DLINECAPS_TEXTURE");
      if(dw&D3DLINECAPS_ZTEST) (*log_func)(" D3DLINECAPS_ZTEST");
   }
   (*log_func)(C_fstr("MaxTextureWidth: %u", cp.MaxTextureWidth));
   (*log_func)(C_fstr("MaxTextureHeight: %u", cp.MaxTextureHeight));
   (*log_func)(C_fstr("MaxVolumeExtent: %u", cp.MaxVolumeExtent));
   (*log_func)(C_fstr("MaxTextureRepeat: %u", cp.MaxTextureRepeat));
   (*log_func)(C_fstr("MaxTextureAspectRatio: %u", cp.MaxTextureAspectRatio));
   (*log_func)(C_fstr("MaxAnisotropy: %u", cp.MaxAnisotropy));
   (*log_func)(C_fstr("MaxVertexW: %f", cp.MaxVertexW));
   (*log_func)(C_fstr("GuardBandLeft: %f", cp.GuardBandLeft));
   (*log_func)(C_fstr("GuardBandTop: %f", cp.GuardBandTop));
   (*log_func)(C_fstr("GuardBandRight: %f", cp.GuardBandRight));
   (*log_func)(C_fstr("GuardBandBottom: %f", cp.GuardBandBottom));
   (*log_func)(C_fstr("ExtentsAdjust: %f", cp.ExtentsAdjust));
   {
      (*log_func)("StencilCaps:");
      dword dw = cp.StencilCaps;
      if(dw&D3DSTENCILCAPS_DECR) (*log_func)(" D3DSTENCILCAPS_DECR");
      if(dw&D3DSTENCILCAPS_DECRSAT) (*log_func)(" D3DSTENCILCAPS_DECRSAT");
      if(dw&D3DSTENCILCAPS_INCR) (*log_func)(" D3DSTENCILCAPS_INCR");
      if(dw&D3DSTENCILCAPS_INCRSAT) (*log_func)(" D3DSTENCILCAPS_INCRSAT");
      if(dw&D3DSTENCILCAPS_INVERT) (*log_func)(" D3DSTENCILCAPS_INVERT");
      if(dw&D3DSTENCILCAPS_KEEP) (*log_func)(" D3DSTENCILCAPS_KEEP");
      if(dw&D3DSTENCILCAPS_REPLACE) (*log_func)(" D3DSTENCILCAPS_REPLACE");
      if(dw&D3DSTENCILCAPS_ZERO) (*log_func)(" D3DSTENCILCAPS_ZERO");
      if(dw&D3DSTENCILCAPS_TWOSIDED) (*log_func)(" D3DSTENCILCAPS_TWOSIDED");
   }
   {
      (*log_func)("FVFCaps:");
      dword dw = cp.FVFCaps;
      if(dw&D3DFVFCAPS_DONOTSTRIPELEMENTS) (*log_func)(" D3DFVFCAPS_DONOTSTRIPELEMENTS");
      if(dw&D3DFVFCAPS_PSIZE) (*log_func)(" D3DFVFCAPS_PSIZE");
      (*log_func)(C_fstr("Texture coord count: %u", dw&D3DFVFCAPS_TEXCOORDCOUNTMASK));
   }
   {
      (*log_func)("TextureOpCaps:");
      dword dw = cp.TextureOpCaps;
      if(dw&D3DTEXOPCAPS_ADD) (*log_func)(" D3DTEXOPCAPS_ADD");
      if(dw&D3DTEXOPCAPS_ADDSIGNED) (*log_func)(" D3DTEXOPCAPS_ADDSIGNED");
      if(dw&D3DTEXOPCAPS_ADDSIGNED2X) (*log_func)(" D3DTEXOPCAPS_ADDSIGNED2X");
      if(dw&D3DTEXOPCAPS_ADDSMOOTH) (*log_func)(" D3DTEXOPCAPS_ADDSMOOTH");

      if(dw&D3DTEXOPCAPS_BLENDCURRENTALPHA) (*log_func)(" D3DTEXOPCAPS_BLENDCURRENTALPHA");
      if(dw&D3DTEXOPCAPS_BLENDDIFFUSEALPHA) (*log_func)(" D3DTEXOPCAPS_BLENDDIFFUSEALPHA");
      if(dw&D3DTEXOPCAPS_BLENDFACTORALPHA) (*log_func)(" D3DTEXOPCAPS_BLENDFACTORALPHA");
      if(dw&D3DTEXOPCAPS_BLENDTEXTUREALPHA) (*log_func)(" D3DTEXOPCAPS_BLENDTEXTUREALPHA");

      if(dw&D3DTEXOPCAPS_BLENDTEXTUREALPHAPM) (*log_func)(" D3DTEXOPCAPS_BLENDTEXTUREALPHAPM");
      if(dw&D3DTEXOPCAPS_BUMPENVMAP) (*log_func)(" D3DTEXOPCAPS_BUMPENVMAP");
      if(dw&D3DTEXOPCAPS_BUMPENVMAPLUMINANCE) (*log_func)(" D3DTEXOPCAPS_BUMPENVMAPLUMINANCE");
      if(dw&D3DTEXOPCAPS_DISABLE) (*log_func)(" D3DTEXOPCAPS_DISABLE");

      if(dw&D3DTEXOPCAPS_DOTPRODUCT3) (*log_func)(" D3DTEXOPCAPS_DOTPRODUCT3");
      if(dw&D3DTEXOPCAPS_LERP) (*log_func)(" D3DTEXOPCAPS_LERP");
      if(dw&D3DTEXOPCAPS_MODULATE) (*log_func)(" D3DTEXOPCAPS_MODULATE");
      if(dw&D3DTEXOPCAPS_MODULATE2X) (*log_func)(" D3DTEXOPCAPS_MODULATE2X");

      if(dw&D3DTEXOPCAPS_MODULATE4X) (*log_func)(" D3DTEXOPCAPS_MODULATE4X");
      if(dw&D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR) (*log_func)(" D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR");
      if(dw&D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA) (*log_func)(" D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA");
      if(dw&D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR) (*log_func)(" D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR");

      if(dw&D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA) (*log_func)(" D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA");
      if(dw&D3DTEXOPCAPS_MULTIPLYADD) (*log_func)(" D3DTEXOPCAPS_MULTIPLYADD");
      if(dw&D3DTEXOPCAPS_PREMODULATE) (*log_func)(" D3DTEXOPCAPS_PREMODULATE");
      if(dw&D3DTEXOPCAPS_SELECTARG1) (*log_func)(" D3DTEXOPCAPS_SELECTARG1");

      if(dw&D3DTEXOPCAPS_SELECTARG2) (*log_func)(" D3DTEXOPCAPS_SELECTARG2");
      if(dw&D3DTEXOPCAPS_SUBTRACT) (*log_func)(" D3DTEXOPCAPS_SUBTRACT");
   }
   (*log_func)(C_fstr("MaxTextureBlendStages: %u", cp.MaxTextureBlendStages));
   (*log_func)(C_fstr("MaxSimultaneousTextures: %u", cp.MaxSimultaneousTextures));
   {
      (*log_func)("VertexProcessingCaps:");
      dword dw = cp.VertexProcessingCaps;
      if(dw&D3DVTXPCAPS_DIRECTIONALLIGHTS) (*log_func)(" D3DVTXPCAPS_DIRECTIONALLIGHTS");
      if(dw&D3DVTXPCAPS_LOCALVIEWER) (*log_func)(" D3DVTXPCAPS_LOCALVIEWER");
      if(dw&D3DVTXPCAPS_MATERIALSOURCE7) (*log_func)(" D3DVTXPCAPS_MATERIALSOURCE7");
      if(dw&D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER) (*log_func)(" D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER");
      if(dw&D3DVTXPCAPS_POSITIONALLIGHTS) (*log_func)(" D3DVTXPCAPS_POSITIONALLIGHTS");
      if(dw&D3DVTXPCAPS_TEXGEN) (*log_func)(" D3DVTXPCAPS_TEXGEN");
      if(dw&D3DVTXPCAPS_TEXGEN_SPHEREMAP) (*log_func)(" D3DVTXPCAPS_TEXGEN_SPHEREMAP");
      if(dw&D3DVTXPCAPS_TWEENING) (*log_func)(" D3DVTXPCAPS_TWEENING");
   }
   (*log_func)(C_fstr("MaxActiveLights: %u", cp.MaxActiveLights));
   (*log_func)(C_fstr("MaxUserClipPlanes: %u", cp.MaxUserClipPlanes));
   (*log_func)(C_fstr("MaxVertexBlendMatrices: %u", cp.MaxVertexBlendMatrices));
   (*log_func)(C_fstr("MaxVertexBlendMatrixIndex: %u", cp.MaxVertexBlendMatrixIndex));
   (*log_func)(C_fstr("MaxPointSize: %f", cp.MaxPointSize));
   (*log_func)(C_fstr("MaxPrimitiveCount: %u", cp.MaxPrimitiveCount));
   (*log_func)(C_fstr("MaxVertexIndex: %u", cp.MaxVertexIndex));
   (*log_func)(C_fstr("MaxStreams: %u", cp.MaxStreams));
   (*log_func)(C_fstr("MaxStreamStride: %u", cp.MaxStreamStride));
   (*log_func)(C_fstr("VertexShaderVersion: 0x%x", cp.VertexShaderVersion));
   (*log_func)(C_fstr("MaxVertexShaderConst: %u", cp.MaxVertexShaderConst));
   (*log_func)(C_fstr("PixelShaderVersion: 0x%x", cp.PixelShaderVersion));
   (*log_func)(C_fstr("PixelShader1xMaxValue: %f", cp.PixelShader1xMaxValue));
   {
      (*log_func)("DevCaps2:");
      dword dw = cp.DevCaps2;
      if(dw&D3DDEVCAPS2_ADAPTIVETESSRTPATCH) (*log_func)(" D3DDEVCAPS2_ADAPTIVETESSRTPATCH");
      if(dw&D3DDEVCAPS2_ADAPTIVETESSNPATCH) (*log_func)(" D3DDEVCAPS2_ADAPTIVETESSNPATCH");
      if(dw&D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) (*log_func)(" D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES");
      if(dw&D3DDEVCAPS2_DMAPNPATCH) (*log_func)(" D3DDEVCAPS2_DMAPNPATCH");
      if(dw&D3DDEVCAPS2_PRESAMPLEDDMAPNPATCH) (*log_func)(" D3DDEVCAPS2_PRESAMPLEDDMAPNPATCH");
      if(dw&D3DDEVCAPS2_STREAMOFFSET) (*log_func)(" D3DDEVCAPS2_STREAMOFFSET");
      if(dw&D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET) (*log_func)(" D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET");
   }
   (*log_func)(C_fstr("MaxNpatchTessellationLevel: %f", cp.MaxNpatchTessellationLevel));
   //(*log_func)(C_fstr("MinAntialiasedLineWidth: %f", cp.MinAntialiasedLineWidth));
   //(*log_func)(C_fstr("MaxAntialiasedLineWidth: %f", cp.MaxAntialiasedLineWidth));
   (*log_func)(C_fstr("MasterAdapterOrdinal: %u", cp.MasterAdapterOrdinal));
   (*log_func)(C_fstr("AdapterOrdinalInGroup: %u", cp.AdapterOrdinalInGroup));
   (*log_func)(C_fstr("NumberOfAdaptersInGroup: %u", cp.NumberOfAdaptersInGroup));
   {
      (*log_func)("DeclTypes:");
      dword dw = cp.DeclTypes;
      if(dw&D3DDTCAPS_UBYTE4) (*log_func)(" D3DDTCAPS_UBYTE4");
      if(dw&D3DDTCAPS_UBYTE4N) (*log_func)(" D3DDTCAPS_UBYTE4N");
      if(dw&D3DDTCAPS_SHORT2N) (*log_func)(" D3DDTCAPS_SHORT2N");
      if(dw&D3DDTCAPS_SHORT4N) (*log_func)(" D3DDTCAPS_SHORT4N");
      if(dw&D3DDTCAPS_USHORT2N) (*log_func)(" D3DDTCAPS_USHORT2N");
      if(dw&D3DDTCAPS_USHORT4N) (*log_func)(" D3DDTCAPS_USHORT4N");
      if(dw&D3DDTCAPS_UDEC3) (*log_func)(" D3DDTCAPS_UDEC3");
      if(dw&D3DDTCAPS_DEC3N) (*log_func)(" D3DDTCAPS_DEC3N");
      if(dw&D3DDTCAPS_FLOAT16_2) (*log_func)(" D3DDTCAPS_FLOAT16_2");
      if(dw&D3DDTCAPS_FLOAT16_4) (*log_func)(" D3DDTCAPS_FLOAT16_4");
   }
   (*log_func)(C_fstr("NumSimultaneousRTs: %u", cp.NumSimultaneousRTs));
   {
      (*log_func)("StretchRectFilterCaps:");
      dword dw = cp.StretchRectFilterCaps;
      if(dw&D3DPTFILTERCAPS_MINFPOINT) (*log_func)(" D3DPTFILTERCAPS_MINFPOINT");
      if(dw&D3DPTFILTERCAPS_MAGFPOINT) (*log_func)(" D3DPTFILTERCAPS_MAGFPOINT");
      if(dw&D3DPTFILTERCAPS_MINFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MINFLINEAR");
      if(dw&D3DPTFILTERCAPS_MAGFLINEAR) (*log_func)(" D3DPTFILTERCAPS_MAGFLINEAR");
   }
   {
      (*log_func)("VS20Caps:");
      dword dw = cp.VS20Caps.Caps;
      if(dw&D3DVS20CAPS_PREDICATION) (*log_func)(" D3DVS20CAPS_PREDICATION");
      if(dw&D3DVS20_MAX_DYNAMICFLOWCONTROLDEPTH) (*log_func)(" D3DVS20_MAX_DYNAMICFLOWCONTROLDEPTH");
      if(dw&D3DVS20_MIN_DYNAMICFLOWCONTROLDEPTH) (*log_func)(" D3DVS20_MIN_DYNAMICFLOWCONTROLDEPTH");
      if(dw&D3DVS20_MAX_NUMTEMPS) (*log_func)(" D3DVS20_MAX_NUMTEMPS");
      if(dw&D3DVS20_MIN_NUMTEMPS) (*log_func)(" D3DVS20_MIN_NUMTEMPS");
      if(dw&D3DVS20_MAX_STATICFLOWCONTROLDEPTH) (*log_func)(" D3DVS20_MAX_STATICFLOWCONTROLDEPTH");
      if(dw&D3DVS20_MIN_STATICFLOWCONTROLDEPTH) (*log_func)(" D3DVS20_MIN_STATICFLOWCONTROLDEPTH");
      (*log_func)(C_fstr(" DynamicFlowControlDepth: %u", cp.VS20Caps.DynamicFlowControlDepth));
      (*log_func)(C_fstr(" NumTemps: %u", cp.VS20Caps.NumTemps));
      (*log_func)(C_fstr(" StaticFlowControlDepth: %u", cp.VS20Caps.StaticFlowControlDepth));
   }
   {
      (*log_func)("PS20Caps:");
      dword dw = cp.PS20Caps.Caps;
      if(dw&D3DPS20CAPS_ARBITRARYSWIZZLE) (*log_func)(" D3DPS20CAPS_ARBITRARYSWIZZLE");
      if(dw&D3DPS20CAPS_GRADIENTINSTRUCTIONS) (*log_func)(" D3DPS20CAPS_GRADIENTINSTRUCTIONS");
      if(dw&D3DPS20CAPS_PREDICATION) (*log_func)(" D3DPS20CAPS_PREDICATION");
      if(dw&D3DPS20CAPS_NODEPENDENTREADLIMIT) (*log_func)(" D3DPS20CAPS_NODEPENDENTREADLIMIT");
      if(dw&D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT) (*log_func)(" D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT");
      if(dw&D3DPS20_MAX_DYNAMICFLOWCONTROLDEPTH) (*log_func)(" D3DPS20_MAX_DYNAMICFLOWCONTROLDEPTH");
      if(dw&D3DPS20_MIN_DYNAMICFLOWCONTROLDEPTH) (*log_func)(" D3DPS20_MIN_DYNAMICFLOWCONTROLDEPTH");
      if(dw&D3DPS20_MAX_NUMTEMPS) (*log_func)(" D3DPS20_MAX_NUMTEMPS");
      if(dw&D3DPS20_MIN_NUMTEMPS) (*log_func)(" D3DPS20_MIN_NUMTEMPS");
      if(dw&D3DPS20_MAX_STATICFLOWCONTROLDEPTH) (*log_func)(" D3DPS20_MAX_STATICFLOWCONTROLDEPTH");
      if(dw&D3DPS20_MIN_STATICFLOWCONTROLDEPTH) (*log_func)(" D3DPS20_MIN_STATICFLOWCONTROLDEPTH");
      if(dw&D3DPS20_MAX_NUMINSTRUCTIONSLOTS) (*log_func)(" D3DPS20_MAX_NUMINSTRUCTIONSLOTS");
      if(dw&D3DPS20_MIN_NUMINSTRUCTIONSLOTS) (*log_func)(" D3DPS20_MIN_NUMINSTRUCTIONSLOTS");
      (*log_func)(C_fstr(" DynamicFlowControlDepth: %u", cp.PS20Caps.DynamicFlowControlDepth));
      (*log_func)(C_fstr(" NumTemps: %u", cp.PS20Caps.NumTemps));
      (*log_func)(C_fstr(" StaticFlowControlDepth: %u", cp.PS20Caps.StaticFlowControlDepth));
      (*log_func)(C_fstr(" NumInstructionSlots: %u", cp.PS20Caps.NumInstructionSlots));
   }
   {
      dword dw = cp.VertexTextureFilterCaps;
      (*log_func)(C_fstr("VertexTextureFilterCaps: 0x%x", dw));
   }
   (*log_func)(C_fstr("MaxVShaderInstructionsExecuted: %u", cp.MaxVShaderInstructionsExecuted));
   (*log_func)(C_fstr("MaxPShaderInstructionsExecuted: %u", cp.MaxPShaderInstructionsExecuted));
   (*log_func)(C_fstr("MaxVertexShader30InstructionSlots: %u", cp.MaxVertexShader30InstructionSlots));
   (*log_func)(C_fstr("MaxPixelShader30InstructionSlots: %u", cp.MaxPixelShader30InstructionSlots));

   {
      static const struct S_format{
         D3DFORMAT fmt;
         const char *name;
      } formats[] = {
                              //unsigned:
         D3DFMT_R8G8B8, "D3DFMT_R8G8B8",
         D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8",
         D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8",
         D3DFMT_R5G6B5, "D3DFMT_R5G6B5",

         D3DFMT_X1R5G5B5, "D3DFMT_X1R5G5B5",
         D3DFMT_A1R5G5B5, "D3DFMT_A1R5G5B5",
         D3DFMT_A4R4G4B4, "D3DFMT_A4R4G4B4",
         D3DFMT_R3G3B2, "D3DFMT_R3G3B2",

         D3DFMT_A8, "D3DFMT_A8",
         D3DFMT_A8R3G3B2, "D3DFMT_A8R3G3B2",
         D3DFMT_X4R4G4B4, "D3DFMT_X4R4G4B4",
         D3DFMT_A2B10G10R10, "D3DFMT_A2B10G10R10",

         D3DFMT_A8B8G8R8, "D3DFMT_A8B8G8R8",
         D3DFMT_X8B8G8R8, "D3DFMT_X8B8G8R8",
         D3DFMT_G16R16, "D3DFMT_G16R16",
         D3DFMT_A2R10G10B10, "D3DFMT_A2R10G10B10",

         D3DFMT_A16B16G16R16, "D3DFMT_A16B16G16R16",
         D3DFMT_A8P8, "D3DFMT_A8P8",
         D3DFMT_P8, "D3DFMT_P8",
         D3DFMT_L8, "D3DFMT_L8",

         D3DFMT_L16, "D3DFMT_L16",
         D3DFMT_A8L8, "D3DFMT_A8L8",
         D3DFMT_A4L4, "D3DFMT_A4L4",
                              //signed
         D3DFMT_V8U8, "D3DFMT_V8U8",
         D3DFMT_Q8W8V8U8, "D3DFMT_Q8W8V8U8",
         D3DFMT_V16U16, "D3DFMT_V16U16",
         D3DFMT_Q16W16V16U16, "D3DFMT_Q16W16V16U16",
         D3DFMT_CxV8U8, "D3DFMT_Q16W16V16U16",
                              //mixed
         D3DFMT_L6V5U5, "D3DFMT_L6V5U5",
         D3DFMT_X8L8V8U8, "D3DFMT_X8L8V8U8",
         D3DFMT_A2W10V10U10, "D3DFMT_A2W10V10U10",
                              //four-cc
         D3DFMT_MULTI2_ARGB8, "D3DFMT_MULTI2_ARGB8",
         D3DFMT_G8R8_G8B8, "D3DFMT_G8R8_G8B8",
         D3DFMT_R8G8_B8G8, "D3DFMT_R8G8_B8G8",
         D3DFMT_UYVY, "D3DFMT_UYVY",
         D3DFMT_YUY2, "D3DFMT_YUY2",
         D3DFMT_DXT1, "D3DFMT_DXT1",
         D3DFMT_DXT2, "D3DFMT_DXT2",
         D3DFMT_DXT3, "D3DFMT_DXT3",
         D3DFMT_DXT4, "D3DFMT_DXT4",
         D3DFMT_DXT5, "D3DFMT_DXT5",
                              //depth
         D3DFMT_D16_LOCKABLE, "D3DFMT_D16_LOCKABLE",
         D3DFMT_D32, "D3DFMT_D32",
         D3DFMT_D15S1, "D3DFMT_D15S1",
         D3DFMT_D24S8, "D3DFMT_D24S8",

         D3DFMT_D24X8, "D3DFMT_D24X8",
         D3DFMT_D24X4S4, "D3DFMT_D24X4S4",
         D3DFMT_D32F_LOCKABLE, "D3DFMT_D32F_LOCKABLE",
         D3DFMT_D24FS8, "D3DFMT_D24FS8",

         D3DFMT_D16, "D3DFMT_D16",
                              //vertex
         D3DFMT_VERTEXDATA, "D3DFMT_VERTEXDATA",
         D3DFMT_INDEX16, "D3DFMT_INDEX16",
         D3DFMT_INDEX32, "D3DFMT_INDEX32",
         D3DFMT_UNKNOWN, NULL
      };
      D3DDISPLAYMODE dm;
      lpDev9->GetDisplayMode(0, &dm);
      for(int use=0; use<2; use++){
         (*log_func)(C_fstr("%s formats:", !use ? "Texture" : "Depth buffer"));

         for(int i=0, j=0; formats[i].name; i++){
            hr = lpD3D9->CheckDeviceFormat(D3DADAPTER_DEFAULT, cp.DeviceType,
               dm.Format,
               !use ? 0 : D3DUSAGE_DEPTHSTENCIL,
               !use ? D3DRTYPE_TEXTURE : D3DRTYPE_SURFACE,
               formats[i].fmt);
            if(SUCCEEDED(hr)){
               (*log_func)(C_fstr(" %2i. %s:", j, formats[i].name));
               ++j;
            }
         }
      }
   }
   (*log_func)("----------------------");
}
#endif
//----------------------------
                              //keyboard hook
static PIGraph lpg_key_hooked;

                              //bug - we receive messages after process is done - this flag helps us (but it's not clean)
static bool process_done;

//----------------------------
                              //keyboard hook proc
LRESULT IGraph::KeyboardProc(int code, WPARAM wParam, LPARAM lParam){

   if(code<0){
      return CallNextHookEx((HHOOK)kb_hook, code, wParam, lParam);
   }

   word wInfo = (word)(lParam>>16);
   dword key_code = wParam;

                              //check for ctrl-alt-del
   if(key_code==VK_DELETE && (wInfo&KF_ALTDOWN) && (shiftkeys&SKEY_CTRL))
      return CallNextHookEx((HHOOK)kb_hook, code, wParam, lParam);
   /*
                              //Alt+Space
   if(key_code==VK_SPACE && (wInfo&KF_ALTDOWN))
      return CallNextHookEx((HHOOK)kb_hook, code, wParam, lParam);
      */
                              //shift keys
   if(key_code==VK_SHIFT){
      if(!(lParam&0x80000000)) shiftkeys |= SKEY_SHIFT;
      else shiftkeys &= ~SKEY_SHIFT;
   }else
   if(wParam==VK_CONTROL){
      if(!(lParam&0x80000000))
         shiftkeys |= SKEY_CTRL;
      else
         shiftkeys &= ~SKEY_CTRL;
   }else
   if((wInfo&KF_ALTDOWN)){
      shiftkeys |= SKEY_ALT;
   }else{
      shiftkeys &= ~SKEY_ALT;
   }

   if(!(lParam&0x80000000)){  //pressed
      key = (IG_KEY)key_code;
      keyboard_map[(byte)key_code] = true;
                              //store the key into the keyboard buffer
      key_buffer[key_buf_beg] = (IG_KEY)key_code;
      if(++key_buf_beg==KEY_BUF_SIZE)
         key_buf_beg = 0;
      if(key_buf_num<KEY_BUF_SIZE)
         ++key_buf_num;
   }else{
      keyboard_map[(byte)key_code] = false;
   }
                              //update game-keys table
   for(dword i=0; i<num_key_scodes; ++i){
      if(key_code==(dword)key_scodes[i]){
         if(!(lParam&0x80000000))
            game_keys |= (1<<i);
         else
            game_keys &= (~(1<<i));
      }
   }
   if(shiftkeys&SKEY_ALT){
                              //allow menu access
      if(create_flags&IG_LOADMENU){
         if(isalnum(key_code))
            return 0;
      }else{
         if(key_code==VK_F4)
            return 0;
      }
                              //allow task-switching
      switch(key_code){
      case VK_TAB:
      case VK_ESCAPE:
         return 0;
      }
   }
   if(shiftkeys&SKEY_CTRL){
      switch(key_code){
      case VK_ESCAPE:
         return 0;
      }
   }

   switch(key_code){
   case K_NUMLOCK:
   case K_CAPSLOCK:
   case K_SCROLLLOCK:
      return 0;
   }

   return 1;
}

//----------------------------

LRESULT CALLBACK GKeyboardProc(int code, WPARAM wParam, LPARAM lParam){

   if(process_done || !lpg_key_hooked)
      return 0;
   return lpg_key_hooked->KeyboardProc(code, wParam, lParam);
}

//----------------------------
                              //un-install grab proc
bool IGraph::GrabKeys(bool on){

   if(on){                    //install
      if(!kb_hook){
         shiftkeys = 0;
         game_keys = 0;
         if(lpg_key_hooked)
            return false;
         lpg_key_hooked = this;
         kb_hook = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)GKeyboardProc,
            NULL, GetCurrentThreadId());
                              //read currently pressed keys
         {
            byte kb[256];
            GetKeyboardState(kb);
            if(kb[VK_CONTROL]&0x80) shiftkeys |= SKEY_CTRL;
            if(kb[VK_MENU]&0x80) shiftkeys |= SKEY_ALT;
            if(kb[VK_SHIFT]&0x80) shiftkeys |= SKEY_SHIFT;
         }
      }
   }else{                     //uninstall
      if(kb_hook){
         if(lpg_key_hooked!=this)
            return false;
         UnhookWindowsHookEx((HHOOK)kb_hook);
         kb_hook = NULL;
         lpg_key_hooked = NULL;
      }
   }
   return true;
}

//----------------------------
//----------------------------
                              //win32 macros
inline void SETRECT(RECT *rc, int l,int t,int r,int b){
   rc->left=l, rc->top=t, rc->right=r, rc->bottom=b;
}

//----------------------------

#pragma pack(push,1)
struct S_config{              //saved settings
   dword posx, posy;
};
#pragma pack(pop)

//----------------------------

LRESULT IGraph::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){

   switch(message){
                              //button down
   case WM_LBUTTONDOWN:
      mouse_butt1 |= 1;
      SetCapture(hwnd);
      break;

   case WM_RBUTTONDOWN:
      mouse_butt1 |= 2;
      SetCapture(hwnd);
      break;
   
   case WM_MBUTTONDOWN:
      mouse_butt1 |= 4;
      SetCapture(hwnd);
      break;
                              //button up
   case WM_LBUTTONUP:
      mouse_butt1 &= -2;
      if(!mouse_butt1) ReleaseCapture();
      break;
   
   case WM_RBUTTONUP:
      mouse_butt1 &= -3;
      if(!mouse_butt1) ReleaseCapture();
      break;
   
   case WM_MBUTTONUP:
      mouse_butt1 &= -5;
      if(!mouse_butt1) ReleaseCapture();
      break;

   case WM_LBUTTONDBLCLK:
   case WM_MBUTTONDBLCLK:
   case WM_RBUTTONDBLCLK:
   case WM_NCLBUTTONDBLCLK:
   case WM_NCMBUTTONDBLCLK:
   case WM_NCRBUTTONDBLCLK:
   case WM_MOUSEACTIVATE:
   case WM_NCHITTEST:
      if((create_flags&IG_FULLSCREEN) && !(create_flags&IG_DEBUGMOUSE))
         return 0;
      break;

   case WM_MOUSEMOVE:
      if(mouse_y1!=-2){
         mouse_x1 = (short)LOWORD(lParam);
         mouse_y1 = (short)HIWORD(lParam);
      }
      if(create_flags&IG_FULLSCREEN)
         return 0;
      break;

   case WM_MOUSEWHEEL:
      {
         int i = wParam>>16;
         if(i&0x8000)
            i |= 0xffff0000;
         mouse_z1 = i / 120;
         mouse_x1 = LOWORD(lParam);
         mouse_y1 = HIWORD(lParam);
      }
      break;

      /*
   case WM_GETMINMAXINFO:
      if(create_flags&IG_FULLSCREEN){
         LPMINMAXINFO lpmmi;
         lpmmi=(LPMINMAXINFO)lParam;
         lpmmi->ptMaxSize.x = scrn_sx;
         lpmmi->ptMaxSize.y = scrn_sy;
         lpmmi->ptMaxTrackSize.x = scrn_sx;
         lpmmi->ptMaxTrackSize.y = scrn_sy;
         return 0;
      }
      break;
      */

   case WM_SETCURSOR:
                              //no cursor in fullscreen mode
                              //or when inactive
      if(active && (create_flags&IG_FULLSCREEN) && !(create_flags&IG_NOHIDEMOUSE)){

         SetCursor(NULL);
         return TRUE;
      }
                              //hide cursor in debug mouse mode
      if(active && mouse_init && ((create_flags&IG_DEBUGMOUSE))){
         SetCursor(NULL);
         return TRUE;
      }
      break;

   case WM_ACTIVATE:
      {
         //int i = LOWORD(wParam);
         bool set_active = (LOWORD(wParam)!=WA_INACTIVE);
         if(set_active)
            set_active = !HIWORD(wParam);
         if(active == set_active)
            break;
         active = set_active;
         if(h_mouse_event)
            event_signalled = true;
         else
            mouse_butt1 = 0;
         mouse_butt = 0;
                              //grab keys only if active
         //if(IsKbInit())
         {
            GrabKeys(active);
            if(active)
               memset(keyboard_map, 0, sizeof(keyboard_map));
         }
         MouseAcquire(active);
         if(active){
                              //acquiring focus
            if(!suspend_disable_count){
                              //wake up sleeper (send any message)
               PostMessage(hwnd, WM_USER, 0, 0);
            }
                              //read mouse, so that we forget any relative mouse movements during inactive time
            mouse_y1 = -2;
         }
         if(init_ok){
            for(dword i=cb_proc_list.size(); i--; )
               (*cb_proc_list[i].cb_proc)(CM_ACTIVATE, active, 0, cb_proc_list[i].context);
         }
         if(active)
            NullTimer();
      }
      break;

   case WM_COMMAND:
      if(init_ok){
         for(int i=cb_proc_list.size(); i--; )
            (*cb_proc_list[i].cb_proc)(CM_COMMAND, LOWORD(wParam), HIWORD(wParam), cb_proc_list[i].context);
      }
      break;

   case WM_SETFOCUS:
      //if(IsKbInit())
         GrabKeys(true);
      if(mouse_init){
         if(h_mouse_event)
            event_signalled = true;
         mouse_butt = 0;
      }else
         mouse_butt1 = 0;
      break;

   case WM_KILLFOCUS:
      //if(IsKbInit())
         GrabKeys(false);
      key = K_NOKEY;
      game_keys = 0;
      shiftkeys = 0;
      break;

   case WM_ENTERMENULOOP:
   case WM_ENTERSIZEMOVE:
                              //allow move through menu
      //if(IsKbInit())
      GrabKeys(false);
      MouseAcquire(false);
      active = false;
      break;

   case WM_EXITMENULOOP:
   case WM_EXITSIZEMOVE:
                              //menu finished
      if(GetFocus())
         active = true;
      //if(IsKbInit())
      GrabKeys(active);
      memset(keyboard_map, 0, sizeof(keyboard_map));
      MouseAcquire(true);
      if(h_mouse_event)
         event_signalled = true;
      mouse_butt = 0;
      NullTimer();
      key = K_NOKEY;
      if(message==WM_EXITSIZEMOVE){
         RECT rc;
         GetClientRect(hwnd, &rc);
                              //consider status window and other helping windows
         struct S_hlp{
            static BOOL CALLBACK cbEnum(HWND hwnd, LPARAM lParam){
               char buf[256];
               if(GetClassName(hwnd, buf, sizeof(buf))){
                  if(!strcmp(buf, "msctls_statusbar32")){
                     RECT &rc = *(RECT*)lParam;
                     RECT rc1;
                     GetWindowRect(hwnd, &rc1);
                     rc.bottom -= (rc1.bottom - rc1.top);
                  }
               }
               return true;
            }
         };
         EnumChildWindows(hwnd, S_hlp::cbEnum, (LPARAM)&rc);

         rc.right = Max(4L, rc.right);
         rc.bottom = Max(4L, rc.bottom);
         UpdateParams(rc.right, rc.bottom, 0, 0, 0, false);
      }
      break;

   case WM_MOVE:
      {
         mouse_butt1 = 0;
      }
      break;

   case WM_PAINT:             //repaint our screen
      if(init_ok){
         /*
                              //let registered apps to paint the contents
         for(int i=cb_proc_list.size(); i--; )
            (*cb_proc_list[i].cb_proc)(CM_PAINT, 0, 0, cb_proc_list[i].context);
         */
         UpdateScreen();
      }
      break;

   case WM_ERASEBKGND:
      return true;

   case WM_CLOSE:
      want_close_app = true;
      return 0;

   case WM_DESTROY:           //immediate exit
      if(init_ok)
         exit(0);
      break;
   default:
      if(message==wm_mousewheel)
         mouse_z1 = (int)wParam / WHEEL_DELTA * mouse_wheel_lines;
   }
   return DefWindowProc(hwnd, message, wParam, lParam);
}

//----------------------------
                              //graphics window procedure
LRESULT CALLBACK WndProc_thunk(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){

                              //get interface
   PIGraph igraph = (PIGraph)GetWindowLong(hwnd, GWL_USERDATA);

   if(!igraph || process_done)
      return DefWindowProc(hwnd, message, wParam, lParam);

   return igraph->WndProc(hwnd, message, wParam, lParam);
}

//----------------------------
//----------------------------
                              //creation / destroying
static bool IRegisterClass(){

   WNDCLASSEX wc;
   memset(&wc, 0, sizeof(wc));
   wc.cbSize = sizeof(WNDCLASSEX);
   wc.style = 0;
   wc.lpfnWndProc = WndProc_thunk;
   wc.cbClsExtra = 0;
   wc.cbWndExtra = 0;
   wc.hInstance = GetModuleHandle(NULL);
                              //try load icon #1 from resources
   HINSTANCE hi_app = GetModuleHandle(NULL);
   HICON hicon = LoadIcon(hi_app, MAKEINTRESOURCE(1));
                              //if it failed, set default app icon
   wc.hIcon = hicon ? hicon : LoadIcon(NULL, IDI_APPLICATION);
   wc.hCursor = LoadCursor(NULL, IDC_ARROW);
   wc.hbrBackground = NULL;
   wc.lpszMenuName = NULL;
   wc.lpszClassName = "IGraph_Window";
   wc.cbClsExtra = 4;         //for graphics interface

   RegisterClassEx(&wc);

   return true;
}

//----------------------------

bool IGraph::InitWindow(int posx, int posy){


                              //add non-client area
   dword wstyle = WS_POPUP | WS_CLIPCHILDREN;
   if(!(create_flags&IG_NOTITLEBAR) && !(create_flags&IG_FULLSCREEN))
      wstyle |= WS_CAPTION;
   if(create_flags&IG_RESIZEABLE)
      wstyle |= WS_THICKFRAME;
   if(!(create_flags&IG_NOMINIMIZE)) wstyle |= WS_MINIMIZEBOX;
   if(!(create_flags&IG_NOSYSTEMMENU)) wstyle |= WS_SYSMENU;
   wstyle |= WS_CLIPCHILDREN;
                              //get size of window
   dword mode_sx;
   dword mode_sy;
   RECT rc;
   hmenu_main = NULL;

   if(!(create_flags&IG_FULLSCREEN)){
      if(create_flags&IG_LOADMENU){
         hmenu_main = LoadMenu(GetModuleHandle(NULL), "MENU_MAIN");
         if(!hmenu_main)
            hmenu_main = CreateMenu();
      }
   }

   if(create_flags&IG_FULLSCREEN){
      mode_sx = scrn_sx;
      mode_sy = scrn_sy;
   }else{
      SETRECT(&rc, 0, 0, scrn_sx, scrn_sy);
      AdjustWindowRect(&rc, wstyle, (bool)hmenu_main);
      mode_sx = rc.right - rc.left;
      mode_sy = rc.bottom - rc.top;
   }

   if(!hwnd){
                              //create window
      hwnd = CreateWindowEx(0,
         "IGraph_Window",     //ClassName
         appName,
         wstyle,
         ((create_flags&IG_FULLSCREEN) ? 0 : posx),
         ((create_flags&IG_FULLSCREEN) ? 0 : posy),
         mode_sx, mode_sy,
         NULL,                //hwndParent
         hmenu_main,
         GetModuleHandle(NULL),
         NULL);               //lpvParam

      if(!hwnd) return false;
                              //store this into window data
      SetWindowLong(hwnd, GWL_USERDATA, (LONG)this);
   }else{
      SetWindowLong(hwnd, GWL_EXSTYLE, 0);
      SetWindowLong(hwnd, GWL_STYLE, wstyle);
      SetWindowPos(hwnd,
#ifndef _DEBUG
         (create_flags&IG_FULLSCREEN) ? HWND_TOPMOST : 
#endif
         HWND_NOTOPMOST,
         ((create_flags&IG_FULLSCREEN) ? 0 : posx),
         ((create_flags&IG_FULLSCREEN) ? 0 : posy),
         mode_sx, mode_sy,
         0);
   }

   if(!(create_flags&IG_HIDDEN)){
                              //show our window
      ShowWindow(hwnd, SW_SHOW);
      UpdateWindow(hwnd);
      SetFocus(hwnd);
   }

   return true;
}

//----------------------------

void IGraph::DestroyWindow(){
                              //don't let it catch destroy messages
   init_ok = false;
   if(hwnd){
      GrabKeys(false);
      SetWindowLong(hwnd, GWL_USERDATA, 0);
      ::DestroyWindow(hwnd);
      hwnd = NULL;
   }
}

//----------------------------

static const char key_name[] = "Software\\Insanity3D system\\Applications";

//----------------------------

static bool GetKeyName(C_str &rtn){

   HINSTANCE hi_app = GetModuleHandle(NULL);
                              //create regkey name from application name
   char module_name[257];
   if(!GetModuleFileName(hi_app, module_name, sizeof(module_name)))
      return false;
   rtn = C_fstr("%s\\%s", key_name, module_name);
   return true;
}

//----------------------------

static bool ReadConfig(S_config &cfg, dword size_x, dword size_y){

   C_str key_name;
   bool b = GetKeyName(key_name);
   assert(b);
   if(!b) return false;
   int rkey = RegkeyOpen(key_name, E_REGKEY_CURRENT_USER);
   if(rkey != -1){
                              //read position
      int rlen = RegkeyRdata(rkey, "Window position", &cfg, sizeof(S_config));
      RegkeyClose(rkey);
      if(rlen==sizeof(S_config))
         return true;
   }
                              //setup defaults
   cfg.posx = (GetSystemMetrics(SM_CXSCREEN)-size_x)/2;
   cfg.posy = (GetSystemMetrics(SM_CYSCREEN)-size_y)/2;

   return false;
}

//----------------------------

static bool WriteConfig(const RECT &rc){

   C_str key_name;
   bool b = GetKeyName(key_name);
   assert(b);
   if(!b) return false;
   int rkey = RegkeyCreate(key_name, E_REGKEY_CURRENT_USER);
   assert(rkey != -1);
   if(rkey == -1)
      return false;

   S_config cfg;
   cfg.posx = rc.left;
   cfg.posy = rc.top;
                              //write position
   b = RegkeyWdata(rkey, "Window position", (const byte*)&cfg, sizeof(S_config));
   RegkeyClose(rkey);
   assert(b);
   return b;
}

//----------------------------

IGraph::IGraph():
   ig_flags(0),
   want_close_app(false),
#ifdef GL
   egl_display(NULL),
   egl_surface(NULL),
   egl_context(NULL),
#endif
   suspend_disable_count(0)
{
}

//----------------------------

IGraph::~IGraph(){

   if(!init_ok)
      return;

   MouseClose();
   //CloseKeyboard();
   if(active)
      GrabKeys(false);

   if(!(create_flags&IG_FULLSCREEN)){
                              //write position only if window is in maximized state
      WINDOWPLACEMENT wp;
      wp.length = sizeof(wp);
      if(GetWindowPlacement(hwnd, &wp) && wp.showCmd==SW_SHOWNORMAL){
         RECT rc;
#ifndef NDEBUG
         bool b = 
#endif
            GetWindowRect(hwnd, &rc);
         assert(b);
         WriteConfig(rc);
      }
   }

#ifdef GL
   if(egl_context){
      eglDestroyContext(egl_display, egl_context);
      eglDestroySurface(egl_display, egl_surface);
   }
#endif
   if(lpDI8){
      lpDI8->Release();
      lpDI8 = NULL;
   }
                                       //DD destroy
   if(lpDev9){
      lpDev9->Release();
      lpDev9 = NULL;

      lpD3D9->Release();
      lpD3D9 = NULL;
   }

   DestroyWindow();

   UnregisterClass("IGraph_Window", GetModuleHandle(NULL));

   if(h_d3d9){
      FreeLibrary(h_d3d9);
      h_d3d9 = NULL;
   }

   if(h_dinput){
      FreeLibrary(h_dinput);
      h_dinput = NULL;
   }
   if(!use_qpc){
      timeEndPeriod(1);
   }

#ifdef LOG_MODE
   IG_log.os_log = NULL;
#endif                        //LOG_MODE
}

//----------------------------

static dword MakeBitMask(dword num_bits, dword shift){
   dword ret = 0;
   for(int i=0; i<(int)num_bits; i++)
      ret |= (1 << (shift+i));
   return ret;
}

//----------------------------

static void ConvertD3DpfToI3Dpf(const D3DFORMAT d3d_fmt, S_pixelformat &pf){

   memset(&pf, 0, sizeof(pf));
                        //setup bpp
   switch(d3d_fmt){
   case D3DFMT_P8:
   case D3DFMT_R3G3B2:
      pf.bytes_per_pixel = 1;
      break;
   case D3DFMT_R5G6B5:
   case D3DFMT_X1R5G5B5:
   case D3DFMT_A1R5G5B5:
   case D3DFMT_A4R4G4B4:
   case D3DFMT_A8R3G3B2:
   case D3DFMT_X4R4G4B4:
   case D3DFMT_A8P8:
      pf.bytes_per_pixel = 2;
      break;
   case D3DFMT_R8G8B8:
      pf.bytes_per_pixel = 3;
      break;
   case D3DFMT_A8R8G8B8:
   case D3DFMT_X8R8G8B8:
      pf.bytes_per_pixel = 4;
      break;
   default:
      assert(0);
   }
                        //setup alpha/palette/compression
   switch(d3d_fmt){
   case D3DFMT_DXT1:
   case D3DFMT_DXT2:
   case D3DFMT_DXT3:
   case D3DFMT_DXT4:
   case D3DFMT_DXT5:
      pf.flags = PIXELFORMAT_COMPRESS;
      pf.four_cc = MAKEFOURCC('D', 'X', 'T', '1' + d3d_fmt - D3DFMT_DXT1);
      break;
   case D3DFMT_P8:
   case D3DFMT_A8P8:
      pf.flags = PIXELFORMAT_PALETTE;
      break;
   case D3DFMT_A8R8G8B8:
   case D3DFMT_A1R5G5B5:
   case D3DFMT_A4R4G4B4:
   case D3DFMT_A8R3G3B2:
      pf.flags = PIXELFORMAT_ALPHA;
      break;
   }
                        //setup RGB components
   switch(d3d_fmt){
   case D3DFMT_R8G8B8:
   case D3DFMT_X8R8G8B8:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x00ff0000;
      pf.g_mask = 0x0000ff00;
      pf.b_mask = 0x000000ff;
      break;
   case D3DFMT_A8R8G8B8:
      pf.a_mask = 0xff000000;
      pf.r_mask = 0x00ff0000;
      pf.g_mask = 0x0000ff00;
      pf.b_mask = 0x000000ff;
      break;
   case D3DFMT_R5G6B5:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x0000f800;
      pf.g_mask = 0x000007e0;
      pf.b_mask = 0x0000001f;
      break;
   case D3DFMT_X1R5G5B5:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x00007c00;
      pf.g_mask = 0x000003e0;
      pf.b_mask = 0x0000001f;
      break;
   case D3DFMT_A1R5G5B5:
      pf.a_mask = 0x00008000;
      pf.r_mask = 0x00007c00;
      pf.g_mask = 0x000003e0;
      pf.b_mask = 0x0000001f;
      break;
   case D3DFMT_A4R4G4B4:
      pf.a_mask = 0x0000f000;
      pf.r_mask = 0x00000f00;
      pf.g_mask = 0x000000f0;
      pf.b_mask = 0x0000000f;
      break;
   case D3DFMT_X4R4G4B4:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x00000f00;
      pf.g_mask = 0x000000f0;
      pf.b_mask = 0x0000000f;
      break;
   case D3DFMT_R3G3B2:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x000000e0;
      pf.g_mask = 0x0000001c;
      pf.b_mask = 0x00000003;
      break;
   case D3DFMT_A8R3G3B2:
      pf.a_mask = 0x0000ff00;
      pf.r_mask = 0x000000e0;
      pf.g_mask = 0x0000001c;
      pf.b_mask = 0x00000003;
      break;
   }
}

//----------------------------
#ifdef GL

static void GlInitPixelFormat(PIXELFORMATDESCRIPTOR &pf){
   memset(&pf, 0, sizeof(pf));
   pf.nSize = sizeof(pf);
	pf.nVersion = 1;
	pf.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
	pf.dwFlags |= PFD_DOUBLEBUFFER;
	pf.iPixelType = PFD_TYPE_RGBA;
	//pf.cColorBits = 16;
	pf.cDepthBits = 16;
	pf.cStencilBits = 0;
}
#endif
//----------------------------

HRESULT IGraph::InitRGBConv(){

   HRESULT hr = D3D_OK;

                              //init color conversion tables
   rgb_conv = Create_rgb_conv();
   rgb_conv->Release();
   S_pixelformat pf;
   memset(&pf, 0, sizeof(pf));

#ifdef GL
   HDC hdc = GetDC(hwnd);
   //int pf = ChoosePixelFormat(GL_hdc, &GL_pfd);
   PIXELFORMATDESCRIPTOR pfd;
   GlInitPixelFormat(pfd);
   int pfi = ChoosePixelFormat(hdc, &pfd);
   DescribePixelFormat(hdc, pfi, sizeof(pfd), &pfd);
   pf.bytes_per_pixel = (pfd.cRedBits+pfd.cGreenBits+pfd.cBlueBits+pfd.cAlphaBits)/8;
   pf.r_mask = ((1<<pfd.cRedBits)-1)<<pfd.cRedShift;
   pf.g_mask = ((1<<pfd.cGreenBits)-1)<<pfd.cGreenShift;
   pf.b_mask = ((1<<pfd.cBlueBits)-1)<<pfd.cBlueShift;
   pf.a_mask = 0;//((1<<pfd.cAlphaBits)-1)<<pfd.cAlphaShift;
#endif
   {
                              //get current color model
      D3DDISPLAYMODE dm;
      HRESULT hr = lpDev9->GetDisplayMode(0, &dm);
      CHECK_D3D_RESULT("GetDisplayMode", hr);
      if(FAILED(hr))
         return hr;
      ConvertD3DpfToI3Dpf(dm.Format, pf);
   }

   if(pf.bytes_per_pixel == 1)
      pf.flags |= PIXELFORMAT_PALETTE;
   if(pf.a_mask) pf.flags |= PIXELFORMAT_ALPHA;
   rgb_conv->Init(pf);

   return hr;
}

//----------------------------

static bool IsDepthFormatOk(IDirect3D9 *lpD3D9, int adapter_id, D3DDEVTYPE dev_type,
   D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat){

                              //verify that the depth format exists.
   HRESULT hr = lpD3D9->CheckDeviceFormat(adapter_id,
      dev_type, AdapterFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);

   if(FAILED(hr))
      return false;

                              //verify that the depth format is compatible.
   hr = lpD3D9->CheckDepthStencilMatch(adapter_id, dev_type,
      AdapterFormat, BackBufferFormat, DepthFormat);
   return SUCCEEDED(hr);
}

//----------------------------
                              //formats are selected in order as they appear here
static const D3DFORMAT depth_formats_16bit[] = {
   D3DFMT_D16,
   D3DFMT_D15S1,
   D3DFMT_D16_LOCKABLE,
}, depth_formats_32bit[] = {
   D3DFMT_D24S8,
   D3DFMT_D24X4S4,
   D3DFMT_D32,
   D3DFMT_D24X8,
};

//----------------------------

static D3DFORMAT ChooseDepthFormat(IDirect3D9 *lpD3D9, int adapter_id, D3DDEVTYPE dev_type,
   dword create_bits_per_pixel, D3DFORMAT BackBufferFormat){

   int i;

   switch(create_bits_per_pixel){
   case 8:
   case 16:
   case 24:
      for(i=0; i<sizeof(depth_formats_16bit)/sizeof(D3DFORMAT); i++){
         if(IsDepthFormatOk(lpD3D9, adapter_id, dev_type, depth_formats_16bit[i], BackBufferFormat, BackBufferFormat))
            return depth_formats_16bit[i];
      }
      for(i=0; i<sizeof(depth_formats_32bit)/sizeof(D3DFORMAT); i++){
         if(IsDepthFormatOk(lpD3D9, adapter_id, dev_type, depth_formats_32bit[i], BackBufferFormat, BackBufferFormat))
            return depth_formats_32bit[i];
      }
      break;
   case 32:
      for(i=0; i<sizeof(depth_formats_32bit)/sizeof(D3DFORMAT); i++){
         if(IsDepthFormatOk(lpD3D9, adapter_id, dev_type, depth_formats_32bit[i], BackBufferFormat, BackBufferFormat)){
            return depth_formats_32bit[i];
         }
      }
      for(i=0; i<sizeof(depth_formats_16bit)/sizeof(D3DFORMAT); i++){
         if(IsDepthFormatOk(lpD3D9, adapter_id, dev_type, depth_formats_16bit[i], BackBufferFormat, BackBufferFormat)){
            return depth_formats_16bit[i];
         }
      }
      break;
   default: assert(0);
   }
   return D3DFMT_UNKNOWN;
}

//----------------------------

void IGraph::InitPresentStruct(int adapter_id, D3DDEVTYPE dev_type, D3DPRESENT_PARAMETERS &pp) const{

   dword bpp;

   memset(&pp, 0, sizeof(pp));
   pp.BackBufferWidth = scrn_sx;
   pp.BackBufferHeight = scrn_sy;
   pp.BackBufferFormat = D3DFMT_UNKNOWN;
   pp.BackBufferCount = 1;

   bool fullscreen = (create_flags&IG_FULLSCREEN);
   if(fullscreen){
      bpp = create_bits_per_pixel;

      switch(create_bits_per_pixel){
      case 8:
         pp.BackBufferFormat = D3DFMT_P8;
         break;
      case 16:
         pp.BackBufferFormat = D3DFMT_R5G6B5;
         break;
      case 24:
         pp.BackBufferFormat = D3DFMT_R8G8B8;
         break;
      case 32:
         pp.BackBufferFormat = D3DFMT_X8R8G8B8;
         break;
      }
      if(create_flags&IG_TRIPPLEBUF)
         ++pp.BackBufferCount;
      pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
   }else{
      D3DDISPLAYMODE dm;
      lpD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);
      pp.BackBufferFormat = dm.Format;

      S_pixelformat pf;
      ConvertD3DpfToI3Dpf(dm.Format, pf);
      bpp = pf.bytes_per_pixel * 8;

      pp.SwapEffect = D3DSWAPEFFECT_COPY;
   }
   pp.MultiSampleType = D3DMULTISAMPLE_NONE;
   if(create_aa_mode){
                              //determine which aa modes are available
      C_vector<bool> ok(17, false);
      for(int i=2; i<17; i++){
         HRESULT hr = lpD3D9->CheckDeviceMultiSampleType(adapter_id, dev_type, pp.BackBufferFormat,
            !fullscreen, (D3DMULTISAMPLE_TYPE)i, NULL);
         if(hr==D3D_OK)
            ok[i] = true;
      }
      switch(create_aa_mode){
      case IG_INIT::ANTIALIAS_NORMAL:
         {
            if(ok[2])
               pp.MultiSampleType = (D3DMULTISAMPLE_TYPE)2;
         }
         break;
      case IG_INIT::ANTIALIAS_BEST:
         {
            if(ok[4])
               pp.MultiSampleType = (D3DMULTISAMPLE_TYPE)4;
         }
         break;
      default: assert(0);
      }
                              //no screen copy if AA is used in windowed mode
      if(!fullscreen && pp.MultiSampleType!=D3DMULTISAMPLE_NONE)
         pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
   }
   pp.hDeviceWindow = hwnd;
   pp.Windowed = !fullscreen;
   pp.EnableAutoDepthStencil = true;

   D3DFORMAT depth_format = ChooseDepthFormat(lpD3D9, adapter_id, dev_type, bpp, pp.BackBufferFormat);
   pp.AutoDepthStencilFormat = depth_format;

   pp.Flags = 0;
   if(create_flags&IG_FULLSCREEN){
      pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
   }
   if(create_flags&IG_LOCKABLE_BACKBUFFER)
      pp.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
   //pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
   pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}

//----------------------------
                              //initialize graphics
bool IGraph::Initialize(dword pos_x, dword pos_y, dword sx1, dword sy1,
   dword flags, int bits_per_pixel, IG_INIT::E_ANTIALIAS_MODE aa_mode,
   int adapter_id, IG_INIT::t_log_func *log_func1){

                              //clean/setup defaults
   init_ok = false;
   h_dinput = NULL;
   log_func = log_func1;

   backbuffer_count = 0;

   lpD3D9 = NULL;
   lpDev9 = NULL;

   desktop_sx = GetSystemMetrics(SM_CXSCREEN);
   desktop_sy = GetSystemMetrics(SM_CYSCREEN);

   hwnd = NULL;

   active = false;            //inactive
   sleep_loop = false;

                              //keys
   shiftkeys = 0;
   key = K_NOKEY;
   game_keys = 0;
   num_key_scodes = 0;
   //kb_init = false;
   kb_hook = NULL;
   memset(keyboard_map, 0, sizeof(keyboard_map));
   key_buf_beg = 0;
   key_buf_num = 0;
                              //mouse
   mouse_init = false;
   lpDIMouse8 = NULL;
   h_mouse_event = NULL;
   h_mouse_thread = NULL;
   event_signalled = false;
   acq_count = 0;
   mouse_butt1 = 0;
   //mouse_left = mouse_top = 0;
   //mouse_right = sx1;
   //mouse_bottom = sy1;
   memset(mouse_pos, 0, sizeof(mouse_pos));
   memset(mouse_rel_pos, 0, sizeof(mouse_rel_pos));
   mouse_x1 = 0; mouse_y1 = -1; mouse_z1 = 0;
   SwapMouseButton(swap_mouse_buttons = SwapMouseButton(false));
                              //init wheel
   wm_mousewheel = 0;

                              //check dimensions
   if(!sx1 || !sy1)
      return false;

#ifdef LOG_MODE
   IG_log.os_log = os_log1;
#endif                        //LOG_MODE

   FARPROC fp_d3d9;
   {
      h_d3d9 = LoadLibraryEx("d3d9.dll", NULL, 0);
      if(!h_d3d9)
         return false;
      fp_d3d9 = GetProcAddress(h_d3d9, "Direct3DCreate9");
      if(!fp_d3d9){
         FreeLibrary(h_d3d9);
         h_d3d9 = NULL;
         return false;
      }
   }

                              //support for wheel mouse
   dword msgMSHWheelSupportActive = RegisterWindowMessage(MSH_WHEELSUPPORT);
   HWND hdlMSHWheel = FindWindow(MSH_WHEELMODULE_CLASS, MSH_WHEELMODULE_TITLE);
   if(hdlMSHWheel && msgMSHWheelSupportActive){
      bool fWheelSupport = SendMessage(hdlMSHWheel, msgMSHWheelSupportActive, 0, 0);
      if(fWheelSupport){
         wm_mousewheel = RegisterWindowMessage(MSH_MOUSEWHEEL);
         mouse_wheel_lines = SendMessage(hdlMSHWheel, RegisterWindowMessage(MSH_SCROLL_LINES), 0, 0);
      }
   }

                              //timer
   use_qpc = false;
#ifdef USE_QUERYPERFORMANCECOUNTER
                              //get frequency
   __int64 qpc_freq;
   use_qpc = QueryPerformanceFrequency((LARGE_INTEGER*)&qpc_freq);
   if(use_qpc && qpc_freq){
      qpc_resolution = 2.0 * 1000.0 / (double)qpc_freq;
   }else
#endif
   {
      timeBeginPeriod(1);
   }
   start_timer = 0;
   start_timer = ReadTimer();
   NullTimer();

   hwnd_dlgs.clear();

   HRESULT hr;

   scrn_sx = sx1, scrn_sy = sy1;

   S_config cfg;
   cfg.posx = 0;
   cfg.posy = 0;

   if(!(flags&IG_FULLSCREEN))
      ReadConfig(cfg, scrn_sx, scrn_sy);

                              //register class
   IRegisterClass();

   create_flags = flags;

   create_bits_per_pixel = (byte)bits_per_pixel;
   create_aa_mode = aa_mode;

   if(!(flags&IG_INITPOSITION)){
      pos_x = cfg.posx;
      pos_y = cfg.posy;
   }

   if(!InitWindow(pos_x, pos_y))
      goto fail;

#ifdef GL
   HDC hdc = GetDC(hwnd);
   /*
   PIXELFORMATDESCRIPTOR pf;
   GlInitPixelFormat(pf);
   int pfi = ChoosePixelFormat(hdc, &pf);
   SetPixelFormat(hdc, pfi, &pf);
   hglrc = wglCreateContext(hdc);
   wglMakeCurrent(hdc, hglrc);
   ReleaseDC(hwnd, hdc);

   int _dgles_load_library(void *(*)(const char *));
   _dgles_load_library(proc_loader);
   */
   egl_display = eglGetDisplay(hdc);
   if(!eglInitialize(egl_display, NULL, NULL))
      assert(0);
   static const EGLint pi32ConfigAttribs[] = {
      EGL_LEVEL, 0,
      //EGL_BUFFER_SIZE, 32,
      EGL_DEPTH_SIZE, EGL_DONT_CARE,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NATIVE_RENDERABLE, false,
		EGL_NONE
	};
	int iConfigs;
   EGLConfig eglConfig = 0;
	if(!eglChooseConfig(egl_display, pi32ConfigAttribs, &eglConfig, 1, &iConfigs))
      assert(0);
   egl_surface = eglCreateWindowSurface(egl_display, eglConfig, hwnd, NULL);
   eglBindAPI(EGL_OPENGL_ES_API);

   static const EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLContext egl_context = eglCreateContext(egl_display, eglConfig, EGL_NO_CONTEXT, ai32ContextAttribs);
   eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
#endif

   assert(hwnd);

   {                          //set mouse position
      POINT pt;
      GetCursorPos(&pt);
      POINT pt1 = {0, 0};
      ClientToScreen(hwnd, &pt1);
      mouse_pos[0] = pt.x - pt1.x;
      mouse_pos[1] = pt.y - pt1.y;
   }

   h_dinput = LoadLibraryEx("dinput8.dll", NULL, 0);
   if(!h_dinput)
      goto fail;

   {
      FARPROC fp = GetProcAddress(h_dinput, "DirectInput8Create");
      if(!fp)
         goto fail;

                              //create object
      typedef HRESULT WINAPI fp_t(HINSTANCE, dword version, REFIID, LPDIRECTINPUT8*, LPUNKNOWN);
      fp_t *fp1;
      fp1 = (fp_t*)fp;
      hr = (*fp1)(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8A, &lpDI8, NULL);
      CHECK_DI_RESULT("DirectInput8Create", hr);
   }

   {
      typedef IDirect3D9 *WINAPI Direct3DCreate9_t(UINT);
      Direct3DCreate9_t *fp_d3d9_a = (Direct3DCreate9_t*)fp_d3d9;

#ifdef LOG_MODE
      IG_log.Enter("Direct3DCreate8");
#endif                        //LOG_MODE

      lpD3D9 = (*fp_d3d9_a)(D3D_SDK_VERSION);

      if(!lpD3D9){
#ifdef LOG_MODE
         IG_log.Fail();
#endif                        //LOG_MODE
         goto fail;
      }
#ifdef LOG_MODE
      IG_log.Ok();
#endif                        //LOG_MODE
      D3DDEVTYPE dev_type = 
#ifndef GL
         (flags&IG_REF_DEVICE) ? D3DDEVTYPE_REF :
#endif
         D3DDEVTYPE_HAL;

      D3DPRESENT_PARAMETERS pp;
      InitPresentStruct(adapter_id, dev_type, pp);

      D3DCAPS9 d3d_caps;
      lpD3D9->GetDeviceCaps(adapter_id, dev_type, &d3d_caps);

      dword dev_flags = 0;
      if(d3d_caps.DevCaps&D3DDEVCAPS_HWTRANSFORMANDLIGHT){
         if(!(create_flags&IG_NO_VSHADER)){
            dev_flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
            if(d3d_caps.DevCaps&D3DDEVCAPS_PUREDEVICE){
               dev_flags |= D3DCREATE_PUREDEVICE;
            }
         }else{
            dev_flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
         }
      }else
         dev_flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

#ifndef NDEBUG
      //dev_flags |= D3DCREATE_FPU_PRESERVE;
#endif
      hr = lpD3D9->CreateDevice(adapter_id,
         dev_type,
         hwnd, dev_flags, &pp, &lpDev9);

      CHECK_D3D_RESULT("CreateDevice", hr);
      if(FAILED(hr))
         goto fail;
retry_caps:
                              //check if it has appropriate shader caps
      D3DCAPS9 cp;
      hr = lpDev9->GetDeviceCaps(&cp);
      CHECK_D3D_RESULT("GetDeviceCaps", hr);
      if((cp.VertexShaderVersion&0xffff) < 0x0101){
         if(dev_flags&D3DCREATE_SOFTWARE_VERTEXPROCESSING)
            goto fail;

                              //try again in software mode
         lpDev9->Release();
         dev_flags &= ~(D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE);
         dev_flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

         hr = lpD3D9->CreateDevice(adapter_id,
            dev_type,
            hwnd, dev_flags, &pp, &lpDev9);
         CHECK_D3D_RESULT("CreateDevice", hr);
         if(FAILED(hr))
            goto fail;
         goto retry_caps;
      }
      hr = lpDev9->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, (pp.MultiSampleType!=D3DMULTISAMPLE_NONE));
      CHECK_D3D_RESULT("SetRenderState", hr);

      backbuffer_count = 0;
      num_swap_buffers = pp.BackBufferCount + 1;
#ifndef GL
      if((flags&IG_VERBOSE) && log_func1){
         DumpCaps(lpD3D9, lpDev9, log_func1);
      }
#endif
   }

   InitRGBConv();
                              //setup globals
   SetViewport(0,0, scrn_sx = sx1, scrn_sy = sy1);

                              //success
   init_ok = true;

#if defined _DEBUG && 0
   if(SetCursorProperties("test1.png")){
      ShowCursor(true);
      SetCursorPos(100, 100);
   }
#endif
   return true;

fail:
   if(lpD3D9){
      lpD3D9->Release();
      lpD3D9 = NULL;
   }
   DestroyWindow();
   UnregisterClass("IGraph_Window", GetModuleHandle(NULL));

   if(h_d3d9){
      FreeLibrary(h_d3d9);
      h_d3d9 = NULL;
   }

   return false;
}

//----------------------------

static const byte guid_hal[] = {
   0xe0, 0x3d, 0xe6, 0x84,
   0xAA, 0x46, 0xCF, 0x11,
   0x81, 0x6F, 0x00, 0x00,
   0xC0, 0x20, 0x15, 0x6E
}, guid_hal_tnl[] = {
   0x78, 0x9e, 0x04, 0xf5,
   0x61, 0x48, 0xd2, 0x11,
   0xa4, 0x07, 0x00, 0xa0,
   0xc9, 0x06, 0x29, 0xa8
};

//----------------------------

PIGraph I2DAPI IGraphCreate(PIG_INIT in){

   if(!in)
      return NULL;

   PIGraph ig = new IGraph;
   if(!ig)
      return false;

   dword fpu_save = _control87(0, 0);
   if(!ig->Initialize(in->pos_x, in->pos_y, in->size_x, in->size_y,
      in->flags, in->bits_per_pixel, in->antialias_mode,
      D3DADAPTER_DEFAULT,
      in->log_func)){
      ig->Release();
      ig = NULL;
   }else{
      ig->ProcessWinMessages(1);
   }
   _control87(fpu_save, 0xffffffff);
   return ig;
}

//----------------------------

bool IGraph::AddCallback(PGRAPHCALLBACK cb, void *context){

                              //check for duplicity of same callbacks
   for(int i=cb_proc_list.size(); i--; ){
      if(cb_proc_list[i].cb_proc==cb && cb_proc_list[i].context==context)
         break;
   }
   if(i==-1){
      cb_proc_list.push_back(S_callback());
      cb_proc_list.back().cb_proc = cb;
      cb_proc_list.back().context = context;
      return true;
   }
   //THROW("callback function already in internal list");
   return false;
}

//----------------------------

bool IGraph::RemoveCallback(PGRAPHCALLBACK cb, void *context){

   for(int i=cb_proc_list.size(); i--; ){
      if(cb_proc_list[i].cb_proc==cb && cb_proc_list[i].context==context)
         break;
   }
   if(i!=-1){
      cb_proc_list.erase(cb_proc_list.begin()+i);
      return true;
   }
   //THROW("callback function not in internal list");
   return false;
}

//----------------------------

bool IGraph::SetViewport(dword l, dword t, dword sx, dword sy){

   if(l<0 || t<0 || l+sx>scrn_sx || t+sy>scrn_sy || sx<0 || sy<0){
      //THROW("invalid viewport");
      return false;
   }
   vport[0] = l;
   vport[1] = t;
   vport[2] = l + sx;
   vport[3] = t + sy;
   return true;
}

//----------------------------

void IGraph::FlipToGDI(){

   if(create_flags&IG_FULLSCREEN){
      while(backbuffer_count){
         UpdateScreen();
      }
   }
}

//----------------------------

bool IGraph::SetBrightness(float r, float g, float b){

   D3DGAMMARAMP gr;
                              //create linear ramps
   int i;
   word *wp = gr.red;
   for(i=0; i<3; i++, wp += 256){
      float f = i==0 ? r : i==1 ? g : b;
      for(int j=0; j<256; j++)
         wp[j] = (word) Min(65535, Max(0, (int)((j<<8) * f)));
   }

   lpDev9->SetGammaRamp(0, D3DSGR_CALIBRATE, &gr);
   return true;
}

//----------------------------

bool IGraph::UpdateScreen(dword flags, void *dest_hwnd){

   assert(!flags);
   if(flags)
      return false;

   HRESULT hr = 0;
   if(!dest_hwnd){
#ifdef GL
      if(GetAsyncKeyState(VK_TAB)){
   	   //glFlush();
	      //glFinish();
         eglSwapBuffers(egl_display, egl_surface);
      }else
#endif
      if(create_flags&IG_FULLSCREEN){
         hr = lpDev9->Present(NULL, NULL, NULL, NULL);
         ++backbuffer_count %= num_swap_buffers;
      }else{
         hr = lpDev9->Present((const RECT*)vport, (const RECT*)vport, NULL, NULL);
      }
   }else{
      hr = lpDev9->Present(
                              //source rect
         NULL,
                              //dest rect
         NULL,
                              //dest window
         (HWND)dest_hwnd,
                              //dirty regions
         NULL);
   }

   switch(hr){
   case D3DERR_DEVICELOST:
      {
                              //check if we may reset
         hr = lpDev9->TestCooperativeLevel();
         if(hr == D3DERR_DEVICENOTRESET){
            UpdateParams(0, 0, 0, 0, 0, true);
         }
      }
      break;

   case D3DERR_INVALIDCALL:
                              //ignore (DirectX gives this when minimizing window)
      break;

   default:
      CHECK_D3D_RESULT("Present", hr);
   }
   return SUCCEEDED(hr);
}

//----------------------------

void IGraph::GetDesktopResolution(dword &x, dword &y) const{

   x = desktop_sx;
   y = desktop_sy;
}

//----------------------------

dword IGraph::NumDisplayModes() const{

   D3DDISPLAYMODE dm;
   lpD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);

   D3DDEVICE_CREATION_PARAMETERS cp;
   HRESULT hr;
   hr = lpDev9->GetCreationParameters(&cp);
   CHECK_D3D_RESULT("GetCreationParameters", hr);
   return lpD3D9->GetAdapterModeCount(cp.AdapterOrdinal, dm.Format);
}

//----------------------------

bool IGraph::GetDisplayMode(int id, dword *sx, dword *sy, dword *bpp) const{

   D3DDISPLAYMODE dm_curr;
   lpD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm_curr);

   D3DDEVICE_CREATION_PARAMETERS cp;
   HRESULT hr;
#ifndef NDEBUG
   hr =
#endif
      lpDev9->GetCreationParameters(&cp);
   CHECK_D3D_RESULT("GetCreationParameters", hr);

   D3DDISPLAYMODE dm;
   S_pixelformat pf;

   hr = lpD3D9->EnumAdapterModes(cp.AdapterOrdinal, dm_curr.Format, id, &dm);
   CHECK_D3D_RESULT("EnumAdapterModes", hr);
   if(FAILED(hr))
      return false;
   ConvertD3DpfToI3Dpf(dm.Format, pf);

   if(sx) *sx = dm.Width;
   if(sy) *sy = dm.Height;
   if(bpp) *bpp = pf.bytes_per_pixel;

   return true;
}

//----------------------------

bool IGraph::ClearViewport(){

   int l = 0, t = 0, r = scrn_sx, b = scrn_sy;

                                       //clip input coords
   if(l<vport[0]) l = vport[0];
   if(t<vport[1]) t = vport[1];
   if(r>vport[2]) r = vport[2];
   if(b>vport[3]) b = vport[3];
                                       //check in-window
   if(r <= 0)
      return true;
   if(b <= 0)
      return true;
#ifdef GL
   glClearColor(0, 0, 0, 0);
   glClear(GL_COLOR_BUFFER_BIT);
#else
                              //clear to color
   D3DRECT rc = {l, t, r, b};
   lpDev9->Clear(1, &rc, D3DCLEAR_TARGET, 0, 0.0f, 0);
#endif
   return true;
}

//----------------------------
                              //keyboard
/*
void IGraph::InitKeyboard(){

   if(!IsKbInit()){
      if(active) GrabKeys(this, true);
      kb_init = true;
   }
}

//----------------------------

void IGraph::CloseKeyboard(){

   if(IsKbInit()){
      if(active) GrabKeys(this, false);
      kb_init = false;
      key = K_NOKEY;
      game_keys = 0;
      shiftkeys = 0;
   }
}
*/

//----------------------------

void IGraph::DefineKeys(const IG_KEY *sc, dword num){

   IG_KEY *cp = key_scodes;
   num_key_scodes = (byte)num;
   while(num--){
      (*cp) = (*sc);
      ++cp; ++sc;
   }
   game_keys = 0;
}

//----------------------------

IG_KEY IGraph::ReadKey(bool pwm){

   if(pwm)
      ProcessWinMessages();
   IG_KEY c = key;
   key = K_NOKEY;
   return c;
}

//----------------------------

IG_KEY IGraph::GetBufferedKey(){

   if(!key_buf_num)
      return K_NOKEY;
   int indx = key_buf_beg - key_buf_num;
   if(indx < 0)
      indx += KEY_BUF_SIZE;
   --key_buf_num;
   return key_buffer[indx];
}

//----------------------------

static const byte spec_trans[][2]={
   ';', ':',
   '=', '+',
   ',', '<',
   '-', '_',
   '.', '>',
   '/', '?',
   '`', '~',
   '\'','"'
};

static const byte spec_codes[] = {
   0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xde
};

static const byte spec_codes1[] = {
   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
   '\\', '[', ']'
};
static const byte spec_trans1[]={
   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
   '|', '{', '}'
};

int IGraph::GetAscii(IG_KEY k1, bool shift) const{

   byte k = (byte)k1;
   byte *b;
   if(k>=0xba && k<=0xde){
                                       //special conversion
      b = (byte*)memchr(spec_codes, k, sizeof(spec_codes));
      if(b) return spec_trans[b-spec_codes][shift];
   }
   if(k==0x20 || k<0x10)
      return k;
   if(k<0x30 || k1==K_NOKEY)
      return 0;
   if(k>=0x5b && k<=0xA0)
      return 0;
   k &= 0x7f;
                                       //lower case
   if(k>='A' && k<='Z')
      k += -'A' + 'a';
   if(k>='a' && k<='z')
      return shift ? (k+'A'-'a') : k;
   if(!shift)
      return k;
   b = (byte*)memchr(spec_codes1, k, sizeof(spec_codes1));
   if(b)
      return spec_trans1[b-spec_codes1];
   return k;
}

//----------------------------

dword IGraph::GetKeyName(IG_KEY key, char *buf, dword buf_size) const{

   char tmp_buf[4];
   static char num_buf[] = "Num x";
   const char *name = NULL;

   switch(key){
   case K_CTRLBREAK: name = "Break"; break;
   case K_BACKSPACE: name = "Backspace"; break;
   case K_TAB: name = "Tab"; break;
   case K_CENTER: name = "Center"; break;
   case K_ENTER: name = "Enter"; break;
   case K_SHIFT: name = "Shift"; break;
   case K_CTRL: name = "Ctrl"; break;
   case K_ALT: name = "Alt"; break;
   case K_PAUSE: name = "Pause"; break;
   case K_CAPSLOCK: name = "Caps Lock"; break;
   case K_ESC: name = "Esc"; break;
   case K_SPACE: name = "Space"; break;
   case K_PAGEUP: name = "Page Up"; break;
   case K_PAGEDOWN: name = "Page Down"; break;
   case K_END: name = "End"; break;
   case K_HOME: name = "Home"; break;
   case K_CURSORLEFT: name = "Cursor Left"; break;
   case K_CURSORUP: name = "Cursor Up"; break;
   case K_CURSORRIGHT: name = "Cursor Right"; break;
   case K_CURSORDOWN: name = "Cursor Down"; break;
   case K_PRTSCR: name = "Print Screen"; break;
   case K_INS: name = "Insert"; break;
   case K_DEL: name = "Delete"; break;
   case K_LEFT_WIN: name = "Left Windows Key"; break;
   case K_RIGHT_WIN: name = "Right Windows Key"; break;
   case K_MENU: name = "Menu"; break;
   case K_GREYMULT: name = "Numpad *"; break;
   case K_GREYPLUS: name = "Numpad +"; break;
   case K_GREYMINUS: name = "Numpad -"; break;
   case K_GREYDOT: name = "Numpad ."; break;
   case K_GREYSLASH: name = "Numpad /"; break;
   case K_NUMLOCK: name = "Num Lock"; break;
   case K_SCROLLLOCK: name = "Scroll Lock"; break;
   case K_SEMICOLON: name = ";"; break;
   case K_EQUALS: name = "="; break;
   case K_COMMA: name = ","; break;
   case K_MINUS: name = "-"; break;
   case K_DOT: name = "."; break;
   case K_SLASH: name = "/"; break;
   case K_BACKAPOSTROPH: name = "`"; break;
   case K_LBRACKET: name = "["; break;
   case K_BACKSLASH: name = "\\"; break;
   case K_RBRACKET: name = "]"; break;
   case K_APOSTROPH: name = "'"; break;
   default:
      {
         if(key >= K_0 && key <= K_9){
            tmp_buf[0] = char('0' + (key - K_0));
            tmp_buf[1] = 0;
            name = tmp_buf;
         }else
         if(key >= K_A && key <= K_Z){
            tmp_buf[0] = char('A' + (key - K_A));
            tmp_buf[1] = 0;
            name = tmp_buf;
         }else
         if(key >= K_NUM0 && key <= K_NUM9){
            num_buf[sizeof(num_buf)-2] = char('0' + (key - K_NUM0));
            name = num_buf;
         }else
         if(key >= K_F1 && key <= K_F12){
            tmp_buf[0] = 'F';
            sprintf(tmp_buf+1, "%i", key - K_F1 + 1);
            name = tmp_buf;
         }else{
            name = "---";
         }
      } 
   }
   dword copy_size = Min(buf_size, (dword)strlen(name)+1ul);
   memcpy(buf, name, copy_size);
   return copy_size;
}

//----------------------------
//----------------------------

static dword CALLBACK NotifyThreadMouse(void *ig1){

   PIGraph IGraph = (PIGraph)ig1;
   HANDLE h_event = IGraph->h_mouse_event;

   while(true){
      WaitForSingleObject(h_event, INFINITE);
      IGraph->event_signalled = true;
   }
}

//----------------------------
                              //mouse
bool IGraph::MouseInit(dword mode){

   assert(!mode);

   if(mouse_init)
      return false;

   if(create_flags&IG_DEBUGMOUSE){
                              //debug mode, don't init DInput object
      mouse_init = true;
                           //acquire
      MouseAcquire(true);

      SetCursor(NULL);
      return true;
   }

   HRESULT hr;

                              //create device
   hr = lpDI8->CreateDevice(GUID_SysMouse, &lpDIMouse8, NULL);
   if(FAILED(hr)){
      CHECK_DI_RESULT("CreateDevice", hr);
      return false;
   }
                              //set data format
   hr = lpDIMouse8->SetDataFormat(&c_dfDIMouse2);
   if(FAILED(hr)){
      CHECK_DI_RESULT("SetDataFormat", hr);
      lpDIMouse8->Release();
      lpDIMouse8 = NULL;
      return false;
   }

                              //set coop level
   hr = lpDIMouse8->SetCooperativeLevel(hwnd,
      DISCL_FOREGROUND |
      DISCL_EXCLUSIVE);

   if(FAILED(hr)){
      CHECK_DI_RESULT("SetCooperativeLevel", hr);
      lpDIMouse8->Release();
      lpDIMouse8 = NULL;
      return false;
   }

   DIPROPDWORD dipdw;
   dipdw.diph.dwSize = sizeof(DIPROPDWORD);
   dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   dipdw.diph.dwObj = 0;
   dipdw.diph.dwHow = DIPH_DEVICE;
   dipdw.dwData = DIPROPAXISMODE_REL;

   hr = lpDIMouse8->SetProperty(DIPROP_AXISMODE, &dipdw.diph);
   if(FAILED(hr)){
      CHECK_DI_RESULT("SetProperty", hr);
      lpDIMouse8->Release();
      lpDIMouse8 = NULL;
      return false;
   }
                           //get mouse threshold and speed
   SystemParametersInfo(SPI_GETMOUSE, 0, ms_speed, 0);

#ifdef MOUSE_USE_EVENT
                              //setup mouse event
   h_mouse_event = CreateEvent(NULL, false, false, NULL);
   if(h_mouse_event){
      hr = lpDIMouse8->SetEventNotification(h_mouse_event);
      CHECK_DI_RESULT("SetEventNotification", hr);
      if(FAILED(hr)){
         CloseHandle(h_mouse_event); h_mouse_event = NULL;
      }else{
         dword d;
         h_mouse_thread = CreateThread(NULL, 1024, NotifyThreadMouse,
            this, 0, &d);
         if(!h_mouse_thread){
            CloseHandle(h_mouse_event); h_mouse_event = NULL;
            hr = lpDIMouse8->SetEventNotification(NULL);
            CHECK_DI_RESULT("SetEventNotification", hr);
         }else
            event_signalled = false;
      }
   }
#endif

   mouse_init = true;
                           //acquire
   MouseAcquire(true);
   return true;
}

//----------------------------

void IGraph::MouseClose(){

   HRESULT hr;
   if(mouse_init){
      {
         SetCursor(NULL);
         MouseAcquire(false);
         if(!(create_flags&IG_DEBUGMOUSE)){
            if(h_mouse_event){
               bool b;
               b = TerminateThread(h_mouse_thread, 0);
               assert(b);
               b = CloseHandle(h_mouse_thread); h_mouse_thread = NULL;
               assert(b);
               hr = lpDIMouse8->SetEventNotification(NULL);
               CHECK_DI_RESULT("SetEventNotification", hr);
               b = CloseHandle(h_mouse_event); h_mouse_event = NULL;
               assert(b);
            }
            assert(lpDIMouse8);
            lpDIMouse8->Release(); lpDIMouse8 = NULL;
         }

         POINT pt;
         pt.x = mouse_pos[0];
         pt.y = mouse_pos[1];
         ClientToScreen(hwnd, &pt);
         SetCursorPos(pt.x, pt.y);

         mouse_butt1 = 0;
         mouse_butt = 0;
      }
      mouse_init = false;
   }
}

//----------------------------

/*
void IGraph::MouseSetup(int x, int y, int l, int t, int r, int b){

   mouse_left=l, mouse_top=t, mouse_right=r, mouse_bottom=b;
   bool get=0;
   POINT pt;
   if(mouse_pos[0]==-1){
      GetCursorPos(&pt);
      mouse_pos[0] = pt.x;
      get=1;
   }else
      mouse_pos[0] = Max(mouse_left, Min(mouse_right, x));
   if(mouse_pos[1] ==-1){
      if(!get) GetCursorPos(&pt);
      mouse_pos[1] = pt.y;
   }else mouse_pos[1] = Max(mouse_top, Min(mouse_bottom, y));
   mouse_butt = 0;
}
*/

//----------------------------

void IGraph::MouseUpdate(){

   int i;
   int nm[3];

   if(!h_mouse_event)
      mouse_butt = 0;

   if(!mouse_init){
      mouse_butt = mouse_butt1;
      if(mouse_y1 < 0){
         for(i=0; i<3; i++) mouse_rel_pos[i] = 0;
         mouse_y1 = -1;
         return;
      }
                              //get standard cursor
      nm[0] = mouse_x1;
      nm[1] = mouse_y1;
      nm[2] = mouse_pos[2] + mouse_z1; mouse_z1 = 0;
      if(create_flags&IG_FULLSCREEN){
         if(hmenu_main){
            nm[1] += GetSystemMetrics(SM_CYMENU);
         }
      }
      mouse_y1 = -1;
   }else{
      if(!active)
         return;

      if(create_flags&IG_DEBUGMOUSE){
         POINT pt = { scrn_sx/2, scrn_sy/2};
         ClientToScreen(hwnd, &pt);
         int mx = pt.x;
         int my = pt.y;

         GetCursorPos(&pt);
         SetCursorPos(mx, my);

         mouse_rel_pos[0] = pt.x - mx;
         mouse_rel_pos[1] = pt.y - my;
         mouse_rel_pos[2] = mouse_z1;
         mouse_z1 = 0;

         mouse_butt = mouse_butt1;

         goto accel_ok;
      }

      if(h_mouse_event){
         if(!event_signalled){
            memset(mouse_rel_pos, 0, sizeof(mouse_rel_pos));
            return;
         }
         event_signalled = false;
         mouse_butt = 0;
      }

      DIMOUSESTATE2 ms;
      HRESULT hr;
re_read:
      assert(lpDIMouse8);
      hr = lpDIMouse8->GetDeviceState(sizeof(ms), &ms);
      switch(hr){
      case DI_OK: break;
      case DIERR_NOTACQUIRED:
      case DIERR_INPUTLOST:
         hr = lpDIMouse8->Acquire();
         if(hr!=DI_OK)
            return;
         goto re_read;
      default:
         CHECK_DI_RESULT("GetDeviceState", hr);
         return;
      }
      mouse_rel_pos[0] = ms.lX;
      mouse_rel_pos[1] = ms.lY;
      mouse_rel_pos[2] = ms.lZ/120 + mouse_z1; mouse_z1 = 0;

      if(swap_mouse_buttons)
         Swap(ms.rgbButtons[0], ms.rgbButtons[1]);

      if(ms.rgbButtons[0]&0x80) mouse_butt |= 1;
      if(ms.rgbButtons[1]&0x80) mouse_butt |= 2;
      if(ms.rgbButtons[2]&0x80) mouse_butt |= 4;
      if(ms.rgbButtons[3]&0x80) mouse_butt |= 8;

                              //acceleration/speed
      if(ms_speed[2] && abs(mouse_rel_pos[0])>ms_speed[0]){
         if(ms_speed[2]==2 && abs(mouse_rel_pos[0])>ms_speed[1])
            mouse_rel_pos[0] *= 4;
         else mouse_rel_pos[0] *= 2;
      }
      if(ms_speed[2] && abs(mouse_rel_pos[1])>ms_speed[0]){
         if(ms_speed[2]==2 && abs(mouse_rel_pos[1])>ms_speed[1])
            mouse_rel_pos[1] *= 4;
         else mouse_rel_pos[1] *= 2;
      }
   accel_ok:
      for(i=0; i<3; i++)
         nm[i] = mouse_pos[i] + mouse_rel_pos[i];
   }
   for(i=0; i<3; i++)
      mouse_rel_pos[i] = nm[i] - mouse_pos[i];

   //nm[0] = Max(mouse_left, Min(mouse_right-1, nm[0]));
   nm[0] = Max(0, Min((int)scrn_sx-1, nm[0]));
   //nm[1] = Max(mouse_top, Min(mouse_bottom-1, nm[1]));
   nm[1] = Max(0, Min((int)scrn_sy-1, nm[1]));

   for(i=0; i<3; i++)
      mouse_pos[i] = nm[i];
}

//----------------------------

void IGraph::SetMousePos(int x, int y){

   //mouse_pos[0] = Max(mouse_left, Min(mouse_right, x));
   mouse_pos[0] = Max(0, Min((int)scrn_sx, x));
   //mouse_pos[1] = Max(mouse_top, Min(mouse_bottom, y));
   mouse_pos[1] = Max(0, Min((int)scrn_sy, y));

   if(!mouse_init){
      POINT pt;
      pt.x = mouse_pos[0];
      pt.y = mouse_pos[1];
      ClientToScreen(hwnd, &pt);
      SetCursorPos(pt.x, pt.y);
   }
}

//----------------------------

void IGraph::MouseAcquire(bool b){

   if(!mouse_init){
      return;
   }
   if(create_flags&IG_DEBUGMOUSE){
                              //can't change mouse position if not active window
      if(active){
         POINT pt = { scrn_sx/2, scrn_sy/2};
         ClientToScreen(hwnd, &pt);

         SetCursorPos(pt.x, pt.y);
      }
      return;
   }

   assert(lpDIMouse8);

   HRESULT hr;
   if(b){
      if(!acq_count++){
         hr = lpDIMouse8->Acquire();
         //CHECK_DI_RESULT("Mouse::Acquire", hr);
      }
   }else
   if(!--acq_count){
      hr = lpDIMouse8->Unacquire();
      //CHECK_DI_RESULT("Mouse::Unacquire", hr);
   }
}

//----------------------------
//----------------------------
                              //timer
void IGraph::NullTimer(){

   last_timer = ReadTimer();
}

//----------------------------

dword IGraph::GetTimer(dword min, dword max){

   if(!(!min || !max || min<=max)){
      //THROW("min must be <= max");
   }

   dword time = ReadTimer() - last_timer;
   int delta = (int)min - (int)time;
   if(delta >= 4)
      Sleep(delta-3);

   while(time < min){
      Sleep(0);
      time = ReadTimer() - last_timer;
   }

   last_timer += time;

   return max ? Min(time, max) : time;
}

//----------------------------

dword IGraph::ReadTimer() const{

#ifdef USE_QUERYPERFORMANCECOUNTER
   if(use_qpc){
      unsigned __int64 t;
      QueryPerformanceCounter((LARGE_INTEGER*)&t);
                              //keep the value signed
      t >>= 1;
      double tm = qpc_resolution * (double)(__int64)t - (double)start_timer;
                              //make high time values loop around 32-bit integet value
      tm = fmod(tm, (double)0x100000000);
                              //convert to integer (round to nearest)
      unsigned __int64 cutt_t;
      __asm{
         fld tm
         fistp cutt_t
      }
      assert(cutt_t <= 0xffffffff);
      return (dword)cutt_t;
   }else
#endif
   {
      dword cutt_t = timeGetTime();
      return cutt_t - start_timer;
   }
}

//----------------------------

void IGraph::ProcessWinMessages(dword flags){

   int i;

   MSG msg;

   if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
      do{
         if(msg.message==WM_QUIT) exit(1);
         i = hwnd_dlgs.size();
         if(i){
            while(i--){
               if(IsDialogMessage(hwnd_dlgs[i], &msg))
                  break;
            }
            if(i!=-1)
               continue;
         }
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
   }
   if(!active && !suspend_disable_count &&
      !(create_flags&IG_HIDDEN) && !(flags&1)){

      i = hwnd_dlgs.size();
      if(i){
         HWND hwnd_fore = GetForegroundWindow();
         while(i--){
            if(hwnd_dlgs[i]==hwnd_fore)
               break;
         }
         if(i!=-1)
            return;
      }
      SetWindowText(hwnd, C_fstr("%s [paused]", (const char*)appName));
      sleep_loop = true;
      while(!active && sleep_loop){
         if(GetMessage(&msg, NULL, 0, 0)){
            i=hwnd_dlgs.size();
            if(i){
               while(i--){
                  if(IsDialogMessage(hwnd_dlgs[i], &msg))
                     break;
               }
               i = (i!=-1);
            }
            if(!i){
               TranslateMessage(&msg);
               DispatchMessage(&msg);
            }
         }
         i=hwnd_dlgs.size();
         if(i){
            HWND hwnd_fore=GetForegroundWindow();
            while(i--){
               if(hwnd_dlgs[i]==hwnd_fore)
                  break;
            }
            if(i!=-1)
               break;
         }
      }
      sleep_loop = false;
      SetWindowText(hwnd, appName);
   }
}

//----------------------------
                              //info

void IGraph::SetAppName(const char *cp){

   appName = cp;
   SetWindowText(hwnd, cp);
}

//----------------------------

void IGraph::SetMenu(void *menu){

   hmenu_main = (HMENU)menu;
   ::SetMenu(hwnd, hmenu_main);
}

//----------------------------

/*
int IGraph::GetMousePos(int axis, bool rel) const{

   if(axis<0 || axis>=3){
      THROW("IGraph::GetMousePos: invalid axis");
      return 0;
   }
   return !rel ? mouse_pos[axis] : mouse_rel_pos[axis];
}
*/

//----------------------------

bool IGraph::AddDlgHWND(void *hwnd){

   if(hwnd){
      hwnd_dlgs.push_back((HWND)hwnd);
      return true;
   }
   return false;
}

//----------------------------

bool IGraph::RemoveDlgHWND(void *hwnd){

   for(int i = hwnd_dlgs.size(); i--; )
   if(hwnd_dlgs[i]==hwnd){
      hwnd_dlgs[i] = hwnd_dlgs.back();
      hwnd_dlgs.pop_back();
      return true;
   }
   return false;
}

//----------------------------

class C_rgb_conversion *IGraph::CreateRGBConv() const{

   return Create_rgb_conv();
}

//----------------------------

LPC_rgb_conversion IGraph::GetRGBConv1(const S_pixelformat &pf, const dword *pal_rgba) const{

                              //look if we've this already initialized
   if(!pal_rgba)
   for(int i=rgb_conv_list.size(); i--; ){
      LPC_rgb_conversion pc = rgb_conv_list[i];
      if(!memcmp(pc->GetPixelFormat(), &pf, sizeof(S_pixelformat))){
         pc->AddRef();
         return pc;
      }
   }
                              //create and initialize new convertor
   LPC_rgb_conversion rgb_conv = CreateRGBConv();
   rgb_conv->Init(pf, pal_rgba);
                              //put into cache
   if(!pal_rgba)
      rgb_conv_list.push_back(rgb_conv);

   return rgb_conv;
}

//----------------------------
D3DFORMAT ConvertFormat(const S_pixelformat *pf);

PIImage IGraph::CreateBackbufferCopy(dword sx, dword sy, bool lockable) const{

   if(!sx)
      sx = Scrn_sx();
   if(!sy)
      sy = Scrn_sy();

   IImage *img = NULL;
#ifdef GL
   img = CreateImage();
   S_pixelformat pf;
   pf.bytes_per_pixel = 4;
   pf.r_mask = 0x00ff0000;
   pf.g_mask = 0x0000ff00;
   pf.b_mask = 0x000000ff;
   pf.a_mask = 0x00000000;
   pf.flags = 0;//PIXELFORMAT_ALPHA;
   bool b = img->Open(NULL, IMGOPEN_EMPTY | IMGOPEN_SYSMEM, Scrn_sx(), Scrn_sy(), &pf);
   if(b){
      dword pitch;
      void *mem;
      if(img->Lock(&mem, &pitch, false)){
         RECT wrc;
         GetClientRect(hwnd, &wrc);
         dword offs = wrc.bottom-Scrn_sy();
         struct S_rgb{
            byte r, g, b, a;
         };
         for(int y=Scrn_sy(); y--; ){
            glReadPixels(0, offs+y, Scrn_sx(), 1, GL_RGBA, GL_UNSIGNED_BYTE, mem);
            S_rgb *rgb = (S_rgb*)mem;
            for(int x=Scrn_sx(); x--; ){
               S_rgb &p = rgb[x];
               Swap(p.r, p.b);
            }
            (byte*&)mem += pitch;
         }
         img->Unlock();
         if(sx!=Scrn_sx() || sy!=Scrn_sy()){
            IImage *img1 = CreateImage();
            img1->Open(NULL, IMGOPEN_EMPTY, sx, sy, img->GetPixelFormat());
            img1->CopyStretched(img);
            img->Release();
            img = img1;
         }
      }
   }else{
      img->Release();
      img = NULL;
   }
#else
   IDirect3DSurface9 *bbuf;
   HRESULT hr = lpDev9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bbuf);
   //HRESULT hr = lpDev9->GetFrontBufferData(0, &bbuf);
   if(SUCCEEDED(hr)){
      img = CreateImage();
      const S_pixelformat *pf = GetPixelFormat();
      bool b = img->Open(NULL, IMGOPEN_EMPTY | IMGOPEN_SYSMEM, sx, sy, pf);
      if(b){
         if(!lockable){
                              //try to create vidmem surface, if possible
            D3DDISPLAYMODE dm;
            lpD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);

            IDirect3DSurface9 *bb;
            HRESULT hr = lpDev9->CreateRenderTarget(sx, sy, dm.Format, D3DMULTISAMPLE_NONE, 0, lockable, &bb, NULL);
            if(SUCCEEDED(hr)){
               img->lpImg9 = bb;
               bb->Release();
            }
         }else{
                              //transfer into lockable swap chain
            IDirect3DSwapChain9 *sc;
            D3DPRESENT_PARAMETERS pp;
            memset(&pp, 0, sizeof(pp));
            pp.BackBufferWidth = sx;
            pp.BackBufferHeight = sy;
            pp.BackBufferFormat = ConvertFormat(pf);
            pp.BackBufferCount = 1;
            pp.MultiSampleType = D3DMULTISAMPLE_NONE;
            pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.hDeviceWindow = NULL;
            pp.Windowed = true;
            pp.EnableAutoDepthStencil = false;
            pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
            pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
            pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
            hr = lpDev9->CreateAdditionalSwapChain(&pp, &sc);
            if(SUCCEEDED(hr)){
               IDirect3DSurface9 *bb;
               hr = sc->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bb);
               if(SUCCEEDED(hr)){
                  img->lpImg9 = bb;
                  bb->Release();
               }
               sc->Release();
            }
         }
         IDirect3DSurface9 *s1 = img->GetDirectXSurface();

         //hr = lpDev9->UpdateSurface(bbuf, NULL, s1, NULL);
         hr = lpDev9->StretchRect(bbuf, NULL, s1, NULL, D3DTEXF_NONE);
         if(FAILED(hr)){
            img->Release();
            img = NULL;
         }
      }else{
         img->Release();
         img = NULL;
      }
      bbuf->Release();
   }else{
#ifdef __DEBUG
      D3D_Fatal("Can't copy buffer", hr, __FILE__, __LINE__);
#endif
   }
#endif
   return img;
}

//----------------------------

bool IGraph::SetCursorProperties(const char *bitmap_name, dword hot_x, dword hot_y){

   bool ret = false;
   PIImage img = CreateImage();
   S_pixelformat pf;
   pf.bytes_per_pixel = 4;
   pf.r_mask = 0x00ff0000;
   pf.g_mask = 0x0000ff00;
   pf.b_mask = 0x000000ff;
   pf.a_mask = 0xff000000;
   pf.flags = PIXELFORMAT_ALPHA;
   ret = img->Open(bitmap_name, IMGOPEN_SYSMEM, 0, 0, &pf);
   if(ret){
      HRESULT hr = lpDev9->SetCursorProperties(hot_x, hot_y, img->GetDirectXSurface());
      ret = (SUCCEEDED(hr));
   }
   img->Release();
   return ret;
}

//----------------------------

dword IGraph::EnableSuspend(bool b){

   if(b){
      assert(suspend_disable_count);
      --suspend_disable_count;
   }else{
      if(!suspend_disable_count){
                              //wake up sleeper 
         sleep_loop = false;
      }
      ++suspend_disable_count;
   }
   return suspend_disable_count;
}

//----------------------------

bool IGraph::LockBackbuffer(void **area, dword *pitch, bool read_only){

   if(!(create_flags&IG_LOCKABLE_BACKBUFFER))
      return false;

   IDirect3DSurface9 *bbuf;
   HRESULT hr = lpDev9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bbuf);
   if(FAILED(hr))
      return false;
   bbuf->Release();
   D3DLOCKED_RECT lrc;
   hr = bbuf->LockRect(&lrc, NULL, D3DLOCK_NO_DIRTY_UPDATE | (read_only ? D3DLOCK_READONLY : 0));
   if(FAILED(hr))
      return false;
   *area = lrc.pBits;
   *pitch = lrc.Pitch;
   return true;
}

//----------------------------

bool IGraph::UnlockBackbuffer(){

   if(!(create_flags&IG_LOCKABLE_BACKBUFFER))
      return false;
   IDirect3DSurface9 *bbuf;
   HRESULT hr = lpDev9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bbuf);
   if(FAILED(hr))
      return false;
   hr = bbuf->UnlockRect();
   bbuf->Release();
   return SUCCEEDED(hr);
}

//----------------------------

bool IGraph::UpdateParams(dword sx, dword sy, dword bits_per_pixel, dword flg, dword flg_mask, bool force_recreate){

                              //mask only supported features
   flg_mask &= (IG_FULLSCREEN |
      //IG_SUSPENDINACTIVE |
      IG_LOADMENU | IG_HIDDEN);

   bool recreate = force_recreate;
   if(sx || sy){
      if(!sx || !sy)
         return false;
      if(sx!=scrn_sx || sy!=scrn_sy){
         scrn_sx = sx;
         scrn_sy = sy;
         recreate = true;
         SetViewport(0, 0, scrn_sx, scrn_sy);
      }
   }

   dword flags_change = (create_flags^flg)&flg_mask;
   if(flags_change){
      if(flags_change&IG_FULLSCREEN){
         bool ok = true;
                              //toggle fullscreen
         /*
                              //check if switch possible
         if(create_flags&IG_FULLSCREEN){
            D3DCAPS9 d3d_caps;
            lpDev9->GetDeviceCaps(&d3d_caps);
            if(!(d3d_caps.Caps2&D3DCAPS2_CANRENDERWINDOWED))
               ok = false;
         }
         */
         if(!ok){
            flg ^= IG_FULLSCREEN;
            flags_change &= ~IG_FULLSCREEN;
         }else
            recreate = true;
      }
      /*
      if((flags_change&IG_SUSPENDINACTIVE) && (create_flags&IG_SUSPENDINACTIVE)){
                              //wake up sleeper 
         sleep_loop = false;
      }
      */

      if(!recreate){
         dword wstyle = GetWindowLong(hwnd, GWL_STYLE);
         create_flags ^= flags_change;
                              //update window flags directly
         bool wl_change = false;

         if(flags_change&IG_NOSYSTEMMENU){
            wstyle &= ~WS_SYSMENU;
            if(!(create_flags&IG_NOSYSTEMMENU)) wstyle |= WS_SYSMENU;
            wl_change = true;
         }
         if(flags_change&IG_NOMINIMIZE){
            wstyle &= ~WS_MINIMIZEBOX;
            if(!(create_flags&IG_NOMINIMIZE)) wstyle |= WS_MINIMIZEBOX;
            wl_change = true;
         }
         if(flags_change&IG_LOADMENU){
            if(hmenu_main){
               ::SetMenu(hwnd, NULL);
               DestroyMenu(hmenu_main);
               hmenu_main = NULL;
            }
            if((create_flags&IG_LOADMENU) && !(create_flags&IG_FULLSCREEN)){
               hmenu_main = LoadMenu(GetModuleHandle(NULL), "MENU_MAIN");
               if(!hmenu_main)
                  hmenu_main = CreateMenu();
               if(hmenu_main)
                  ::SetMenu(hwnd, hmenu_main);
            }
                              //adjust window size
            RECT rc;
            SETRECT(&rc, 0, 0, scrn_sx, scrn_sy);
            AdjustWindowRect(&rc, wstyle, (bool)hmenu_main);

            dword sx = rc.right-rc.left;
            dword sy = rc.bottom-rc.top;
            SetWindowPos(hwnd, 0, 0, 0, sx, sy, SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER);

            {
               int sy = GetSystemMetrics(SM_CYMENU);
               mouse_pos[1] -= (create_flags&IG_LOADMENU) ? sy : +sy;
               mouse_y1 = -2; //ignore 1st WM_MOUSEMOVE message
            }
         }

         if(flags_change&IG_HIDDEN){
                              //show/hide window
            wstyle |= WS_VISIBLE;
            if(create_flags&IG_HIDDEN)
               wstyle &= ~WS_VISIBLE;
            wl_change = true;
         }

         if(wl_change){
            SetWindowLong(hwnd, GWL_STYLE, wstyle);

            if(flags_change&IG_HIDDEN){
               UpdateWindow(hwnd);
               SetFocus(hwnd);
            }
         }
      }
   }
   if(bits_per_pixel){
      if(create_bits_per_pixel!=bits_per_pixel){
         create_bits_per_pixel = (byte)bits_per_pixel;
         if((create_flags^flags_change)&IG_FULLSCREEN)
            recreate = true;
      }
   }

   if(recreate){
      S_config cfg;
      cfg.posx = 0;
      cfg.posy = 0;
      if(!(ig_flags&IGGLAGS_IN_RESET)){
         for(int i=cb_proc_list.size(); i--; )
            (*cb_proc_list[i].cb_proc)(CM_RECREATE, flg, 0, cb_proc_list[i].context);
         init_ok = false;
                              //release window and DirectDraw components
         if(!(create_flags&IG_FULLSCREEN)){
                              //write config
            {
               RECT rc;
#ifndef NDEBUG
               bool b = 
#endif
                  GetWindowRect(hwnd, &rc);
               assert(b);
               WriteConfig(rc);
            }
         }
         if(!(flags_change&IG_FULLSCREEN) && !(create_flags&IG_FULLSCREEN)){
            ShowWindow(hwnd, SW_HIDE);
         }
         create_flags ^= flags_change;
      }

      HRESULT hr;

      D3DDEVICE_CREATION_PARAMETERS cp;
      lpDev9->GetCreationParameters(&cp);

      D3DPRESENT_PARAMETERS pp;

      InitPresentStruct(cp.AdapterOrdinal, cp.DeviceType, pp);
      hr = lpDev9->Reset(&pp);
      if(FAILED(hr)){
         switch(hr){
         case D3DERR_INVALIDCALL:
            {
               abort();
            }
            break;
         }
         ig_flags |= IGGLAGS_IN_RESET;
         return false;
      }
      backbuffer_count = 0;
      num_swap_buffers = pp.BackBufferCount + 1;

      ig_flags &= ~IGGLAGS_IN_RESET;

      if(!(create_flags&IG_FULLSCREEN)){
                              //read config
         ReadConfig(cfg, scrn_sx, scrn_sy);
      }

      if(!InitWindow(cfg.posx, cfg.posy)){
         D3D_Fatal("InitWindow", hr, __FILE__, __LINE__);
      }

      InitRGBConv();
      ClearViewport();

      init_ok = true;
      //MouseSetup(mouse_pos[0], mouse_pos[1], 0, 0, scrn_sx, scrn_sy);
      SetMousePos(mouse_pos[0], mouse_pos[1]);
      {
         for(int i=cb_proc_list.size(); i--; )
            (*cb_proc_list[i].cb_proc)(CM_RECREATE, create_flags, 1, cb_proc_list[i].context);
         for(i=cb_proc_list.size(); i--; )
            (*cb_proc_list[i].cb_proc)(CM_RECREATE, create_flags, 2, cb_proc_list[i].context);
      }
   }
   return true;
}

//----------------------------
//----------------------------
