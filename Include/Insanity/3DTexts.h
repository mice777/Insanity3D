#include <stddef.h>
#include <insanity\sprite.h>
#include <rules.h>

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// 3D text output system.
//----------------------------

enum E_TEXT_JUSTIFY{
                              //mode:
   TEXT_JUSTIFY_LEFT = 0,
   TEXT_JUSTIFY_CENTER = 1,
   TEXT_JUSTIFY_RIGHT = 2,
   TEXT_JUSTIFY_MASK = 3,
                              //modifiers:
   TEXT_JUSTIFY_CLIP_TO_SCREEN= 0x10000,  //clip to screen borders
   TEXT_JUSTIFY_CONVERT_EOL   = 0x20000,  //convert '\\' char to '\n' char
};


//----------------------------

class C_text{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

   virtual void GetExtent(float &l, float &t, float &r, float &b) const = 0;

   virtual void Render(class I3D_scene*) const = 0;

   virtual void SetOn(bool) = 0;

//----------------------------
// Get/Set color of text.
   virtual void SetColor(dword c) = 0;
   virtual dword GetColor() const = 0;

//----------------------------
// Get/Set position of text;
   virtual void SetPos(const S_vector2 &) = 0;
   virtual const S_vector2 &GetPos() const = 0;

//----------------------------
//   virtual void SetSize(float) = 0;

};

typedef C_text *PC_text;

//----------------------------
// Polytext init structure

struct S_text_create{
   const void *tp;
   dword color;
   float x, y;
   float size;
   float line_dist;           //distance between lines. used when text is splitted to more lines. if <= 0, size is used insted it.
   dword justify;
   bool wide;
   float *fmt_rect;           //format rectangle - pointer to 4 float array
   //float *fill_rect;          //format rectangle to make lines - pointer to 4 float array
   float *clip_rect;          //format rectangle into which is text clipped - pointer to 4 float array
   float *xret;
   float *yret;
   int *num_lines;
   int z;
   float italic;
   float in_scale_x;
   float in_scale_y;
   bool text_3d;              //non transformed 3d text in scene
   bool shadow;

   S_text_create():
      justify(0),
      color(0xffffffff),
      wide(false), 
      fmt_rect(NULL),
      //fill_rect(NULL),
      clip_rect(NULL),
      xret(NULL),
      yret(NULL),
      num_lines(NULL),
      z(0),
      line_dist(-1.0f),
      italic(0.0f),
      in_scale_x(1.0f),
      in_scale_y(1.0f),
      text_3d(false),
      shadow(false)
   {}
};

//----------------------------

class C_poly_text{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

   virtual CPC_sprite_image GetSpriteImage() const = 0;

   virtual PC_text CreateText(S_text_create &ct) = 0;
   virtual bool RemoveText(PC_text) = 0;

   virtual void Render(I3D_scene*) const = 0;

   virtual I3D_driver *GetDriver() const = 0;
};

typedef C_poly_text *PC_poly_text;
typedef const C_poly_text *CPC_poly_text;

E_SPRITE_IMAGE_INIT_RESULT CreatePolyText(I3D_driver *driver, PC_poly_text *out, const char *prjname,
   const char *diff_name = NULL, const char *opt_name = NULL, dword txt_create_flags = 0);

E_SPRITE_IMAGE_INIT_RESULT CreatePolyText(I3D_driver *driver, PC_poly_text *out, class C_cache &ck_project,
   C_cache *ck_diffuse = NULL, C_cache *ck_opacity = NULL, dword txt_create_flags = 0);

E_SPRITE_IMAGE_INIT_RESULT CreatePolyText(CPC_poly_text src, PC_poly_text *out);

//----------------------------

