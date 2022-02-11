#define WIN32_LEAN_AND_MEAN

#define NOATOM            //Atom Manager routines
#define NOCLIPBOARD       //Clipboard routines
#define NOCOLOR           //Screen colors
#define NOCOMM            //COMM driver routines
#define NODEFERWINDOWPOS  //DeferWindowPos routines
#define NODRAWTEXT        //DrawText() and DT_*
#define NOGDICAPMASKS     //CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOHELP            //Help engine interface.
#define NOKANJI           //Kanji support stuff.
#define NOKERNEL          //All KERNEL defines and routines
#define NOKEYSTATES       //MK_*
#define NOMCX             //Modem Configuration Extensions
#define NOMEMMGR          //GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        //typedef METAFILEPICT
#define NONLS             //All NLS defines and routines
#define NOOPENFILE        //OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOPROFILER        //Profiler interface.
#define NORASTEROPS       //Binary and Tertiary raster ops
#define NOSCROLL          //SB_* and scrolling routines
#define NOSERVICE         //All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           //Sound driver routines
#define NOSYSCOMMANDS     //SC_*
#define NOTEXTMETRIC      //typedef TEXTMETRIC and associated routines
#define OEMRESOURCE       //OEM Resource values

#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <zmouse.h>

#define IG_INTERNAL
#include <igraph2.h>
#include <conv_rgb.hpp>
#include <C_str.hpp>
#include <smartptr.h>
#include <c_cache.h>
#include <insanity\assert.h>
#include <C_vector.h>

#ifdef GL
#include <Egl\Egl.h>
#include <Gles2\gl2.h>
#endif

#ifdef _MSC_VER               //MB: need stack probes for debugging crashes
#pragma optimize("y", off)
#endif
