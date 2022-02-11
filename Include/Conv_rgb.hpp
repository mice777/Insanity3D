#ifndef __CONV_RGB_H
#define __CONV_RGB_H
#pragma once

#include <rules.h>
#include <igraph2.h>

/*----------------------------------------------------------------\
Copyright (c) 1998 Michal Bacik  All rights reserved
Abstract:
 Color model conversion class - provides converting color data
 from 8-bit (paletized, rgb), 16, 24 and 32-bit models.
 The class represents destination color model, to which the
 data are converted.

Functions:
   Init - initialize convertor
   Uninit - uninitialize
   Convert - convert data into different model, or copy if model same
      In:
         dst - ptr to dest ptr
         dst_pitch - if zero, it will become sx*srcbpp
         pal - ptr to dest pal (if needed, it will be allocated)
   AlphaBlend - blend alpha pixels into surface
   CKeyToAlpha - convert colorkey info into alpha channel (transp/opaque)
   AlphaToCKey - convert alpha channel to colorkey

Possible conversions:
                source   destination
   bit-depth:     8(pal)   8(pal)         only copy
                  8(pal)   8(rgb)         dither
                  8(pal)   16             compressed, dither
                  8(pal)   24             compressed
                  8(pal)   32             compressed
                  16       16             only copy
                  24/32    8(pal)         remap to pal
                  24/32    8(pal)         3-3-2 palette, dither
                  24/32    8(rgb)         dither
                  24/32    16             dither
                  24/32    24/32
|\----------------------------------------------------------------*/

#define I2DAPI
#define I2DMETHOD_(type,method) virtual type I2DAPI method

#define MCONV_RAW       0     //raw data (default)
//#define MCONV_PACKED    1     //conver packed transparency data (sprites)
#define MCONV_DITHER    2     //dither during conversion
#define MCONV_INVALPHA  4     //inverse alpha during blend
#define MCONV_FLIP_Y    8     //flip lines
#define MCONV_COLORKEY  0x10  //mix colorkey information into destination (compressed format)
#define MCONV_BGR       0x20  //Blue-Green-Red order true-color (or paletized) source mode

class C_rgb_conversion{
public:
   I2DMETHOD_(dword,AddRef)() = 0;
   I2DMETHOD_(dword,Release)() = 0;

   I2DMETHOD_(void,Init)(const S_pixelformat &pf, const dword *pal_rgba = NULL) = 0;
   I2DMETHOD_(void,UnInit)() = 0;
   I2DMETHOD_(bool,Convert)(const void *src, void *dst, dword sx, dword sy,
      dword src_pitch, dword dst_pitch, byte srcbpp, dword **pal, dword flags,
      dword color_key = 0) const = 0;

   I2DMETHOD_(bool,AlphaBlend)(void *dst, void *src, const dword *pal_argb,
      dword sx, dword sy,
      dword src_pitch, dword dst_pitch, byte srcbpp, dword flags = 0) const = 0;

   I2DMETHOD_(dword,GetPixel)(byte r, byte g, byte b, byte a=255, byte *pal=NULL) const = 0;
   I2DMETHOD_(void,InverseRGB)(dword val, byte *r, byte *g, byte *b, byte *a, 
      const S_pixelformat *pf, byte *pal = NULL) const = 0;
   I2DMETHOD_(const S_pixelformat*,GetPixelFormat)() const = 0;
#ifndef GL
   I2DMETHOD_(bool,IsPaletized)() const = 0;
#endif
   I2DMETHOD_(byte,Bpp)() const = 0;
};

typedef C_rgb_conversion *LPC_rgb_conversion;

//----------------------------

#endif
