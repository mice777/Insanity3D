#define ISL_INTERFACE
#include <iscript.h>
#include <c_unknwn.hpp>
#include <smartptr.h>
#include <insanity\assert.h>
#include <C_str.hpp>
#include <insanity\os.h>
#include <c_cache.h>
#include <tabler2.h>
#include <dta_read.h>
#include <c_buffer.h>
#include <C_vector.h>

//#include <stdio.h>
#include <ctype.h>

#pragma warning(push,3)
#include <stack>
#include <map>
#include <algorithm>
#pragma warning(pop)

#include "c_stack.hpp"
#include "opcode.h"

using namespace std;

#pragma warning(disable: 4245)