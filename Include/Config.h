#include <rules.h>

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Configuration system.
// This header file provides basic game configutation structure,
// holding information about global settings.
//
// Provided is function running configuration dialog on the structure.
//----------------------------

#define CONFIG_API

#ifdef _DEBUG
#pragma comment(lib, "config_d")
#else
#pragma comment(lib, "config")
#endif

//----------------------------

struct S_game_configuration{
                              //video:
   bool tripple_buffering;
   bool fullscreen;
   bool compress_textures;
   dword mode_x, mode_y;
   dword bits_per_pixel;
   int adapter_identifier;
   bool use_reference_device;
   bool disable_vshader;
   bool disable_pshader;
   enum E_FILTER{
      FILTER_LINEAR,
      FILTER_ANISO,
   } filter;

                              //sounds:
   bool use_sounds;
                              //volumes in range 0 ... 1
   bool sounds_use_3D_hw;
   bool sounds_use_EAX;

                              //other:
   bool use_dbase;

   enum E_ANTIALIAS_MODE{
      AA_NONE,
      AA_NORMAL,
      AA_BEST,
   } antialias_mode;

//----------------------------
// Read/write config from/to system registry.
   bool CONFIG_API ReadSysConfig(const char *reg_base);
   bool CONFIG_API WriteSysConfig(const char *reg_base);

   S_game_configuration();
};

//----------------------------

enum E_CONFIG_STATUS{
   CONFIG_OK,                 //config run successfully
   CONFIG_ERR_NO_DLL,         //cannot fine config DLLs
   CONFIG_INIT_FAIL,          //initialization failed (no DirectX)
   CONFIG_CANCELED,           //configuration canceled
};

//----------------------------
// Run game configuration dialog.
E_CONFIG_STATUS CONFIG_API RunGameConfiguration(const char *reg_base, bool all_features = false, const char *language = NULL);

//----------------------------