#define STRICT
#define ISND_FULL
#define TRACKER_FULL
#include <windows.h>
#include <dsound.h>
#include <insanity\assert.h>

#include <isound2.h>
#include <dta_read.h>
#include <c_unknwn.hpp>
#include <C_str.hpp>
#include <i3d\I3D_math.h>
#include <smartptr.h>

#pragma warning(push,3)
#include <vector>
#pragma warning(pop)

using namespace std;

//#pragma optimize("y", off)    //need stack probes for debugging crashes
