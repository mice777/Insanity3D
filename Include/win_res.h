/*--------------------------------------------------------
   Copyright (c) Lonely Cat Games  All rights reserved.
   Windows resource access functions.

   These functions obtain access to Windows resources
   through usage of C_cache interface.
--------------------------------------------------------*/

#ifndef __WIN_RES_H_
#define __WIN_RES_H_

#include <rules.h>

#ifdef _DEBUG
# pragma comment (lib,"base_lib_d")
#else
# pragma comment (lib,"base_lib")
#endif


//----------------------------
// Open resource.
// Parameters:
//    hinstance ... Windows HINSTANCE of module where resources reside (pass NULL to use default = executable)
//    res_group ... name of resource group, e.g. "BINARY"
//    res_name ... name of resource to open
//    ck ... reference to C_cache interface, which will be successfully opened if call succeeds
// Return value:
//    true if opened successfully
bool OpenResource(void *hinstance, const char *res_group, const char *res_name, class C_cache &ck);

//----------------------------
#endif
