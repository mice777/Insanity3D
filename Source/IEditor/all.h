#include <windows.h>
#include <richedit.h>

#include <i3d\editor.h>
#include <i3d\Bin_format.h>
#include <tabler2.h>
#include <c_chunk.h>
#include <smartptr.h>
#include <commctrl.h>
#include <isound2.h>
#include <win_reg.h>
#include <C_str.hpp>
#include <c_buffer.h>
#include <win_res.h>
#include <insanity\os.h>
#include <profile.h>

#include <stdio.h>
#pragma warning(push, 2)
#include <map>
#include <set>
//#include <C_vector>
#include <list>
#include <algorithm>
#pragma warning(pop)

#ifdef _MSC_VER               //MB: need stack probes for debugging crashes
#pragma optimize("y", off)
#endif

#ifdef _DEBUG                 //allow unused variables in design mode
#pragma warning(disable: 4189)
#endif