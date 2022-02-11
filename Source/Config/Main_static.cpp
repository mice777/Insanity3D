#include <windows.h>
#include <config.h>
#include <memory.h>
#include <win_reg.h>


//----------------------------

//----------------------------
// Implicit conversion if float to int, with rounding to nearest.
#if defined _MSC_VER
inline int FloatToInt(float f){
   __asm{
      fld f
      fistp f 
   }
   return *(int*)&f;
}

#else

inline int FloatToInt(float f){
   return (int)f;
}

#endif

//----------------------------

S_game_configuration::S_game_configuration(){
   memset(this, 0, sizeof(S_game_configuration));
   use_dbase = true;

   use_sounds = true;
   sounds_use_3D_hw = true;
   sounds_use_EAX = true;

   mode_x = 800;
   mode_y = 600;
   bits_per_pixel = 16;
   adapter_identifier = 0;
   filter = FILTER_ANISO;
   tripple_buffering = true;
   fullscreen = false;
   compress_textures = true;
   antialias_mode = AA_NONE;
}

//----------------------------

static const char *key_name[] = {
                              //0
   "Display width",
   "Display height",
   "Display bitdepth",
   "Antialias mode",
                              //4
   "Fullscreen",
   "TrippleBuf",
   "Adapter ID",
   "Use REF device",
                              //8
   "Use database",
   "Sounds",
   "Filtering",
   "Compressed textures",
                              //12
   "Sounds use 3D hardware",
   "Sounds use EAX",
   "Disable vertex shader",
   "Disable pixel shader",
};

//----------------------------

bool S_game_configuration::ReadSysConfig(const char *reg_base){

   int i = RegkeyOpen(reg_base, E_REGKEY_CURRENT_USER);
   if(i!=-1){
      RegkeyRdata(i, key_name[0], (byte*)&mode_x, sizeof(mode_x));
      RegkeyRdata(i, key_name[1], (byte*)&mode_y, sizeof(mode_y));
      RegkeyRdata(i, key_name[2], (byte*)&bits_per_pixel, sizeof(bits_per_pixel));
      RegkeyRdata(i, key_name[3], (byte*)&antialias_mode, sizeof(antialias_mode));

      RegkeyRdata(i, key_name[4], (byte*)&fullscreen, sizeof(fullscreen));
      RegkeyRdata(i, key_name[5], (byte*)&tripple_buffering, sizeof(tripple_buffering));
      RegkeyRdata(i, key_name[6], (byte*)&adapter_identifier, sizeof(adapter_identifier));
      RegkeyRdata(i, key_name[7], (byte*)&use_reference_device, sizeof(use_reference_device));

      RegkeyRdata(i, key_name[8], (byte*)&use_dbase, sizeof(use_dbase));
      RegkeyRdata(i, key_name[9], (byte*)&use_sounds, sizeof(use_sounds));
      RegkeyRdata(i, key_name[10], (byte*)&filter, sizeof(filter));
      RegkeyRdata(i, key_name[11], (byte*)&compress_textures, sizeof(compress_textures));

      RegkeyRdata(i, key_name[12], (byte*)&sounds_use_3D_hw, sizeof(sounds_use_3D_hw));
      RegkeyRdata(i, key_name[13], (byte*)&sounds_use_EAX, sizeof(sounds_use_EAX));
      RegkeyRdata(i, key_name[14], &disable_vshader, sizeof(disable_vshader));
      RegkeyRdata(i, key_name[15], &disable_pshader, sizeof(disable_pshader));

      RegkeyClose(i);
      return true;
   }

   return false;
}

//----------------------------

bool S_game_configuration::WriteSysConfig(const char *reg_base){

   int i = RegkeyCreate(reg_base, E_REGKEY_CURRENT_USER);
   if(i!=-1){
      RegkeyWdword(i, key_name[0], mode_x);
      RegkeyWdword(i, key_name[1], mode_y);
      RegkeyWdword(i, key_name[2], bits_per_pixel);
      RegkeyWdword(i, key_name[3], antialias_mode);

      RegkeyWdword(i, key_name[4], fullscreen);
      RegkeyWdword(i, key_name[5], tripple_buffering);
      RegkeyWdword(i, key_name[6], adapter_identifier);
      RegkeyWdword(i, key_name[7], use_reference_device);

      RegkeyWdword(i, key_name[8], use_dbase);
      RegkeyWdword(i, key_name[9], use_sounds);
      RegkeyWdword(i, key_name[10], filter);
      RegkeyWdword(i, key_name[11], compress_textures);

      RegkeyWdword(i, key_name[12], sounds_use_3D_hw);
      RegkeyWdword(i, key_name[13], sounds_use_EAX);
      RegkeyWdword(i, key_name[14], disable_vshader);
      RegkeyWdword(i, key_name[15], disable_pshader);

      RegkeyClose(i);

      return true;
   }
   return false;
}

//----------------------------

E_CONFIG_STATUS CONFIG_API RunGameConfiguration(const char *reg_base, bool all, const char *lang){

#ifdef _DEBUG
   const char *dll_filename = "Config_d.dll";
#else
   const char *dll_filename = "Config.dll";
#endif
   HINSTANCE hi = LoadLibrary(dll_filename);
   if(!hi)
      return CONFIG_ERR_NO_DLL;

   FARPROC fp = GetProcAddress(hi, "RunConfig");
   if(!fp)
      return CONFIG_ERR_NO_DLL;
   typedef E_CONFIG_STATUS t_RunConfig(HINSTANCE, const char *reg_base, bool, const char*);
   t_RunConfig *p_Cfg = (t_RunConfig*)fp;
   E_CONFIG_STATUS ret = (*p_Cfg)(hi, reg_base, all, lang);
   FreeLibrary(hi);
   return ret;
}

//----------------------------
