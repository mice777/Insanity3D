#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#define NOATOM            //Atom Manager routines
#define NOCLIPBOARD       //Clipboard routines
#define NOCOLOR           //Screen colors
#define NOCOMM            //COMM driver routines
#define NOCTLMGR          //Control and Dialog routines
#define NODEFERWINDOWPOS  //DeferWindowPos routines
#define NODRAWTEXT        //DrawText() and DT_*
#define NOGDICAPMASKS     //CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOHELP            //Help engine interface.
#define NOICONS           //IDI_*
#define NOKANJI           //Kanji support stuff.
#define NOKEYSTATES       //MK_*
#define NOMCX             //Modem Configuration Extensions
#define NOMEMMGR          //GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMENUS           //MF_*
#define NOMETAFILE        //typedef METAFILEPICT
#define NOMINMAX          //Macros min(a,b) and max(a,b)
#define NONLS             //All NLS defines and routines
#define NOOPENFILE        //OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOPROFILER        //Profiler interface.
#define NORASTEROPS       //Binary and Tertiary raster ops
#define NOSCROLL          //SB_* and scrolling routines
#define NOSERVICE         //All Service Controller routines, SERVICE_ equates, etc.
#define NOSHOWWINDOW      //SW_*
#define NOSOUND           //Sound driver routines
#define NOSYSCOMMANDS     //SC_*
#define NOTEXTMETRIC      //typedef TEXTMETRIC and associated routines
#define NOVIRTUALKEYCODES //VK_*
#define NOWH              //SetWindowsHook and WH_*
#define NOWINMESSAGES     //WM_*, EM_*, LB_*, CB_*
#define NOWINOFFSETS      //GWL_*, GCL_*, associated routines
#define NOWINSTYLES       //WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define OEMRESOURCE       //OEM Resource values

#include <Windows.h>
#include <d3d9.h>
#pragma warning(push,3)
#include <D3dx9math.h>
#pragma warning(pop)

#include <malloc.h>

#define I3D_FULL
#include <igraph2.h>
#include <I3D\i3d2.h>
#include <math\Spline.hpp>
#include <smartptr.h>
#include <profile.h>
#include <insanity\assert.h>
#include <sortlist.hpp>
#include <c_buffer.h>
#include <c_cache.h>
#include <c_chunk.h>
#include <conv_rgb.hpp>
#include <isound2.h>
#include <c_str.hpp>
#include <math\bezier.hpp>
#include <math\ease.hpp>
#include <c_unknwn.hpp>
#include <C_buffer.h>
#include <iexcpt.h>
#include <c_linklist.h>
#include <C_vector.h>

#ifdef GL
#include <Egl\Egl.h>
#include <Gles2\gl2.h>
#endif

#pragma warning(push,3)
#include <map>
#include <set>
#pragma warning(pop)

using namespace std;

#include "driver.h"
#include "frame.h"

