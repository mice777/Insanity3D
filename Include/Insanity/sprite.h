#ifndef __SPRITE_H
#define __SPRITE_H

#include <rules.h>

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Sprite and sprite-image classes.
//----------------------------

struct S_sprite_rectangle{
                              //original sprite's rectangle in the image
   int ix, iy, isx, isy, icx, icy;
                              //texture coordinate - in range 0.0f ... 1.0f
   float l, t, sx, sy, cx, cy;
};

//----------------------------

enum E_SPRITE_IMAGE_INIT_RESULT{
   SPRINIT_OK,
   SPRINIT_CANT_OPEN_PROJECT,
   SPRINIT_CANT_OPEN_DIFFUSE,
   SPRINIT_CANT_OPEN_OPACITY,
   SPRINIT_CANT_CREATE_TEXTURE,
   SPRINIT_CANT_OPEN_FONTIMG,
};

//----------------------------

class C_sprite_image{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

//----------------------------
// Initialize class - create texture and init sprite rectangles.
   virtual E_SPRITE_IMAGE_INIT_RESULT Init(class I3D_driver*, const char *prjname, const char *diff_name = NULL,
      const char *opt_name = NULL, dword txt_create_flags = 0,
      bool use_new_prj_format = false,
      float border_offset = .5f) = 0;

   virtual E_SPRITE_IMAGE_INIT_RESULT Init(I3D_driver *drv, class C_cache &ck_prj, C_cache *ck_diffuse = NULL,
      C_cache *ck_opacity = NULL, dword txt_create_flags = 0,
      bool use_new_prj_format = false, float border_offset = .5f) = 0;

//----------------------------
// Close all resources.
   virtual void Close() = 0;

//----------------------------
   virtual float GetAspectRatio() const = 0;

   virtual class I3D_texture *GetTexture() = 0;
   virtual const class I3D_texture *GetTexture() const = 0;

   virtual dword NumRects() const = 0;
   virtual const S_sprite_rectangle *GetRects() const = 0;
};

typedef C_sprite_image *PC_sprite_image;
typedef const C_sprite_image *CPC_sprite_image;

PC_sprite_image CreateSpriteImage();

//----------------------------

class C_sprite{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

//----------------------------
// Set/Get visibility.
   virtual void SetOn(bool b) = 0;
   virtual bool IsOn() const = 0;

//----------------------------
// Set/Get position in screen coordinates.
   virtual void SetScreenPos(const struct S_vector2&) = 0;
   virtual const S_vector2 &GetScreenPos() const = 0;

//----------------------------
// Set/Get position in relative coordinates.
   virtual void SetPos(const S_vector2&) = 0;
   virtual const S_vector2 &GetPos() const = 0;

//----------------------------
// Get additional info.
   virtual const S_vector2 &GetSize() const = 0;
   virtual const S_vector2 &GetHotSpot() const = 0;
   virtual int GetZ() const = 0;

//----------------------------
// Set/Get color.
   virtual void SetColor(dword) = 0;
   virtual dword GetColor() const = 0;

//----------------------------
// Render on screen.
   virtual void Render(class I3D_scene*) const = 0;

//----------------------------
// Get extent, in normalized screen coordinates.
   virtual void GetExtent(float &l, float &t, float &r, float &b) const = 0;

//----------------------------
// Get texture extent, in texture coordinates.
   virtual void GetTextureExtent(float &l, float &t, float &r, float &b) const = 0;

//----------------------------
// Shear - italic-look.
   virtual void Shear(float angle, float base_y) = 0;

//----------------------------
// Rotate vertices depending on given reference screen point.
   virtual void Rotate(float angle, const S_vector2 &pt) = 0;

//----------------------------
// Get associated sprite image.
   virtual CPC_sprite_image GetSpriteImage() const = 0;

//----------------------------
// Set scale of sprite (scale specified in normalized screen coordinates).
   virtual void SetScale(const S_vector2&) = 0;

//----------------------------
// Set sprite's UV coordinates directly (values specified for left-top and right-bottom corner).
   virtual void SetUV(const S_vector2 &uv_left_top, const S_vector2 &uv_right_bottom) = 0;
};

typedef C_sprite *PC_sprite;

PC_sprite CreateSprite(float x, float y, float size, CPC_sprite_image img, dword indx, int z = 0, float aspect = 1.0f);

//----------------------------
                              //Group of sprites - usualy ones using the same sprite image.
                              //All these sprites are rendered together.
                              //Usable for text consisting from more letters - sprites. 
class C_sprite_group{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

   virtual void AddSprite(PC_sprite) = 0;
   virtual void RemoveSprite(PC_sprite) = 0;

   virtual void Render(I3D_scene*) const = 0;

//----------------------------
// Set color of all sprites.
   virtual void SetColor(dword) = 0;
// Set color of all sprites wich z is equal to input.
   virtual void SetColor(dword c, int z) = 0;

//----------------------------
// Get group's position.
   virtual void SetPos(const S_vector2 &) = 0;
   virtual const S_vector2 &GetPos() const = 0;

//----------------------------
// Enable/disable all sprites.
   virtual void SetOn(bool) = 0;

//----------------------------
// Get sprites.
   virtual const PC_sprite *GetSprites() const = 0;
   virtual dword NumSprites() const = 0;
                              
//----------------------------
// Clipping in relative screen coord. Rect - pointer to 4 float array.
   virtual void SetClipRect(float *) = 0;

};

typedef C_sprite_group *PC_sprite_group;

PC_sprite_group CreateSpriteGroup();

//----------------------------
// Create small texture containing string. Since real texture size is dependent on
// hardware (power of 2), initialized part of texture is returned in 'size' param.
// winHFONT is a valid Windows' HFONT handle.
class I3D_texture *CreateStringTexture(const char *cp, int size[2], I3D_driver *drv, void *winHFONT);

//----------------------------
// Draw string texture above specified frame. The size is size of valid rectangle
// within the texture. Distances specifies visibility, range between them fades
// opacity.
void DrawStringTexture(const class I3D_frame *frm, float y_screen_displace, class I3D_scene *scene, CPI3D_texture, const int size[2], 
   dword color, float dist_near, float dist_far, float scale = 1.0f, float y_world_displace = .2f);

//----------------------------
#endif