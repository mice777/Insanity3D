#include "all.h"
#include <sortlist.hpp>

#define RGB_MASTER 1          //g
#define RGB_SLAVE1 0          //r
#define RGB_SLAVE2 2          //b


//----------------------------

struct S_rgbi{
   byte rgb[3];
   byte index;
};

static const word alpha_and_mask[4] = {0, 0xf, 0xff, 0xfff};

//----------------------------
                              //help functions
inline int FindLastBit(dword val){
   dword rtn;
   __asm{
      mov eax, val
      bsr eax, eax
      jnz ok
      mov eax,-1
ok:
      mov rtn, eax
   }
   return rtn;
}

#ifdef _MSC_VER

inline dword Rol(dword dw, byte b){
   __asm{
      push ecx
      mov cl, b
      rol dw, cl
      pop ecx
   }
   return dw;
}
inline dword Ror(dword dw, byte b){
   __asm{
      push ecx
      mov cl, b
      ror dw, cl
      pop ecx
   }
   return dw;
}
#endif

//----------------------------

                              //count pixels in packed data
static void CountPackedPixels(const void *area, dword sy,
   dword *pixnum, dword *codenum){

   *pixnum=0;
   *codenum=0;
   for(dword yy=0; yy<sy; yy++){
      const byte *mem=(const byte*)area+((dword*)area)[yy];
      dword pl;
      while(++*codenum, pl=*mem++){
         if(!(pl&0x80)) *pixnum+=pl, mem+=pl;
      }
   }
}

//----------------------------
//----------------------------

class C_rgb_conversion_i: public C_rgb_conversion{

   dword ref;

   bool inited;
   S_pixelformat pf;

                              //convert with dithering (called internally)
   void Dither(const void *src, void *dst, dword pitch_src, dword pitch_dst,
      dword size_x, dword size_y, dword srcbpp, const dword *src_pal) const;

   union{
      word *wfield;
      byte *bfield;
      struct S_rgbi *rgbi;
   } r;
   union{
      word *wfield;
      byte *bfield;
      byte *r2rgbi;
   }g;
   union{
      word *wfield;
      byte *bfield;
   }b;
   union{
      word *wfield;
   }a;
   byte *init_pal_argb;

                              //true-color formats:
   int r_byte_pos, g_byte_pos, b_byte_pos, a_byte_pos;


                              //RGB element position - lowest bit of
                              //byte value, must be masked by ?BitMask
   byte r_pos, g_pos, b_pos, a_pos;
public:
   byte RPos() const{ return r_pos; }
   byte GPos() const{ return g_pos; }
   byte BPos() const{ return b_pos; }
   byte APos() const{ return a_pos; }

   dword RBitMask() const{ return pf.r_mask; }
   dword GBitMask() const{ return pf.g_mask; }
   dword BBitMask() const{ return pf.b_mask; }
   dword ABitMask() const{ return pf.a_mask; }
public:

   C_rgb_conversion_i(): inited(false), ref(1)
   {}
   ~C_rgb_conversion_i(){
      UnInit();
   }

   I2DMETHOD_(dword,AddRef)(){ return ++ref; }
   I2DMETHOD_(dword,Release)(){
      if(--ref) return ref;
      delete this;
      return 0;
   }

   I2DMETHOD_(void,Init)(const S_pixelformat &pf, const dword *pal_rgba = NULL);
   I2DMETHOD_(void,UnInit)();
   I2DMETHOD_(bool,Convert)(const void *src, void *dst, dword sx, dword sy,
      dword src_pitch, dword dst_pitch, byte srcbpp, dword **pal, dword flags,
      dword color_key) const;

   I2DMETHOD_(bool,AlphaBlend)(void *dst, void *src, const dword *pal, dword sx, dword sy,
      dword src_pitch, dword dst_pitch, byte srcbpp, dword flags=0) const;

   I2DMETHOD_(dword,GetPixel)(byte r, byte g, byte b, byte a, byte *pal=NULL) const;
   I2DMETHOD_(void,InverseRGB)(dword val, byte *r, byte *g, byte *b, byte *a,
      const S_pixelformat* = NULL, byte *pal=NULL) const;
   I2DMETHOD_(const S_pixelformat*,GetPixelFormat)() const{
      assert(inited);
      return inited ? &pf : NULL;
   }
#ifndef GL
   I2DMETHOD_(bool,IsPaletized)() const;
#endif
   I2DMETHOD_(byte,Bpp)() const;
};

//----------------------------
//----------------------------

void C_rgb_conversion_i::Init(const S_pixelformat &pf1, const dword *pal_argb1){

   UnInit();
                              //init known values
   init_pal_argb = NULL;
   pf = pf1;

   if(!(pf.flags&PIXELFORMAT_COMPRESS)){
                              //get rotation values
      r_pos = byte((FindLastBit(RBitMask())-7) & 31);
      g_pos = byte((FindLastBit(GBitMask())-7) & 31);
      b_pos = byte((FindLastBit(BBitMask())-7) & 31);
      a_pos = byte((FindLastBit(ABitMask())-7) & 31);

      int i;
      switch(pf.bytes_per_pixel){
      case 1:
#ifndef GL
         if(!(pf.flags&PIXELFORMAT_PALETTE) || !pal_argb1)
#endif
         {                       //8-bit rgb
                              //initialize byte tables
            r.bfield = new byte[256*3];
            g.bfield = r.bfield + 256*1;
            b.bfield = r.bfield + 256*2;
            byte *bp;
                              //create table of reds
            bp = r.bfield, i=0;
            do{
               *bp = byte(Rol(i, r_pos) & pf.r_mask);
            }while(++bp, ++i<256);
                              //create table of greens
            bp = g.bfield, i=0;
            do{
               *bp = byte(Rol(i, g_pos) & pf.g_mask);
            }while(++bp, ++i<256);
                              //create table of blues
            bp = b.bfield, i=0;
            do{
               *bp = byte(Rol(i, b_pos) & pf.b_mask);
            }while(++bp, ++i<256);
         }
#ifndef GL
         else{               //initialized by palette
                              //store pal
            init_pal_argb = new byte[256 * sizeof(dword)];
            memcpy(init_pal_argb, pal_argb1, 256 * sizeof(dword));
                              //build look-up sorted array
                              //based on red entries
            r.rgbi = new S_rgbi[256];
            S_sort_entry se32[256];
            byte *palp;
            for(i=0; i<256; i++){
               byte *palp = &init_pal_argb[i*sizeof(dword)];
               se32[i].sort_value=(palp[RGB_MASTER]<<16) | (palp[RGB_SLAVE1]<<8) | palp[RGB_SLAVE2];
               se32[i].data = (void*)i;
            }
            RadixSort32(se32, 256);
            for(i=0; i<256; i++){
               dword indx = (dword)se32[i].data;
               palp = &init_pal_argb[indx*4];
               *(dword*)&r.rgbi[i].rgb=*(dword*)palp;
               r.rgbi[i].index = (byte)indx;
            }
            g.r2rgbi=new byte[256];
            int j=0;
            i=0;
            do{
               while(j<255 && r.rgbi[j].rgb[RGB_MASTER]<i) ++j;
               g.r2rgbi[i] = (byte)j;
            }while(++i<256);
         }
#endif
         break;
      case 2:
                              //initialize word tables
         r.wfield = new word[256*4];
         g.wfield = r.wfield + 256*1;
         b.wfield = r.wfield + 256*2;
         a.wfield = r.wfield + 256*3;
         word *wp;
                              //create table of reds
         wp = r.wfield, i=0;
         do{
            *wp = word(Rol(i, r_pos) & pf.r_mask);
         }while(++wp, ++i<256);
                              //create table of greens
         wp = g.wfield, i=0;
         do{
            *wp = word(Rol(i, g_pos) & pf.g_mask);
         }while(++wp, ++i<256);
                              //create table of blues
         wp = b.wfield, i=0;
         do{
            *wp = word(Rol(i, b_pos) & pf.b_mask);
         }while(++wp, ++i<256);
                              //create table of alphas
         wp = a.wfield, i=0;
         do{
            *wp = word(Rol(i, a_pos) & pf.a_mask);
         }while(++wp, ++i<256);
         break;
      case 3:
      case 4:
         r_byte_pos = r_pos/8;
         g_byte_pos = g_pos/8;
         b_byte_pos = b_pos/8;
         a_byte_pos = a_pos/8;
         break;
      }
   }else{
                              //init compressed format
   }
                              //initialized
   inited = true;
}

//----------------------------

void C_rgb_conversion_i::UnInit(){
                              //initialized?
   if(!inited) return;

   if(!(pf.flags&PIXELFORMAT_COMPRESS)){
      switch(pf.bytes_per_pixel){
      case 1:
#ifndef GL
         if((pf.flags&PIXELFORMAT_PALETTE) && init_pal_argb){
            if(init_pal_argb){
               delete[] init_pal_argb;
               delete[] r.rgbi;
               delete[] g.r2rgbi;
            }
         }else
#endif
            delete[] r.bfield;
         break;
      case 2:
                                 //free word tables
         delete[] r.wfield;
         break;
      }
   }else{                     //compressed
   }
   inited = false;
}

//----------------------------

byte C_rgb_conversion_i::Bpp() const{

   if(!(pf.flags&PIXELFORMAT_COMPRESS)){
      return (byte)pf.bytes_per_pixel;
   }else{
      return 0;
   }
}

//----------------------------
#ifndef GL
bool C_rgb_conversion_i::IsPaletized() const{
   return (pf.flags&PIXELFORMAT_PALETTE);
}
#endif
//----------------------------

static const byte uni_pal[256][4] = {
                              //0
   0x10, 0x10, 0x00, 0,
   0x10, 0x10, 0x40, 0,
   0x10, 0x10, 0x80, 0,
   0x10, 0x10, 0xc0, 0,
                              //4
   0x10, 0x30, 0x20, 0,
   0x10, 0x30, 0x60, 0,
   0x10, 0x30, 0xa0, 0,
   0x10, 0x30, 0xe0, 0,
                              //8
   0x10, 0x50, 0x20, 0,
   0x10, 0x50, 0x60, 0,
   0x10, 0x50, 0xa0, 0,
   0x10, 0x50, 0xe0, 0,
                              //c
   0x10, 0x70, 0x20, 0,
   0x10, 0x70, 0x60, 0,
   0x10, 0x70, 0xa0, 0,
   0x10, 0x70, 0xe0, 0,
                              //10
   0x10, 0x90, 0x20, 0,
   0x10, 0x90, 0x60, 0,
   0x10, 0x90, 0xa0, 0,
   0x10, 0x90, 0xe0, 0,
                              //14
   0x10, 0xb0, 0x20, 0,
   0x10, 0xb0, 0x60, 0,
   0x10, 0xb0, 0xa0, 0,
   0x10, 0xb0, 0xe0, 0,
                              //18
   0x10, 0xd0, 0x20, 0,
   0x10, 0xd0, 0x60, 0,
   0x10, 0xd0, 0xa0, 0,
   0x10, 0xd0, 0xe0, 0,
                              //1c
   0x10, 0xf0, 0x20, 0,
   0x10, 0xf0, 0x60, 0,
   0x10, 0xf0, 0xa0, 0,
   0x10, 0xf0, 0xe0, 0,

                              //20
   0x30, 0x10, 0x20, 0,
   0x30, 0x10, 0x60, 0,
   0x30, 0x10, 0xa0, 0,
   0x30, 0x10, 0xe0, 0,
                              //24
   0x30, 0x30, 0x20, 0,
   0x30, 0x30, 0x60, 0,
   0x30, 0x30, 0xa0, 0,
   0x30, 0x30, 0xe0, 0,
                              //28
   0x30, 0x50, 0x20, 0,
   0x30, 0x50, 0x60, 0,
   0x30, 0x50, 0xa0, 0,
   0x30, 0x50, 0xe0, 0,
                              //2c
   0x30, 0x70, 0x20, 0,
   0x30, 0x70, 0x60, 0,
   0x30, 0x70, 0xa0, 0,
   0x30, 0x70, 0xe0, 0,
                              //30
   0x30, 0x90, 0x20, 0,
   0x30, 0x90, 0x60, 0,
   0x30, 0x90, 0xa0, 0,
   0x30, 0x90, 0xe0, 0,
                              //34
   0x30, 0xb0, 0x20, 0,
   0x30, 0xb0, 0x60, 0,
   0x30, 0xb0, 0xa0, 0,
   0x30, 0xb0, 0xe0, 0,
                              //38
   0x30, 0xd0, 0x20, 0,
   0x30, 0xd0, 0x60, 0,
   0x30, 0xd0, 0xa0, 0,
   0x30, 0xd0, 0xe0, 0,
                              //3c
   0x30, 0xf0, 0x20, 0,
   0x30, 0xf0, 0x60, 0,
   0x30, 0xf0, 0xa0, 0,
   0x30, 0xf0, 0xe0, 0,

                              //0
   0x50, 0x10, 0x20, 0,
   0x50, 0x10, 0x60, 0,
   0x50, 0x10, 0xa0, 0,
   0x50, 0x10, 0xe0, 0,
                              //4
   0x50, 0x30, 0x20, 0,
   0x50, 0x30, 0x60, 0,
   0x50, 0x30, 0xa0, 0,
   0x50, 0x30, 0xe0, 0,
                              //8
   0x50, 0x50, 0x20, 0,
   0x50, 0x50, 0x60, 0,
   0x50, 0x50, 0xa0, 0,
   0x50, 0x50, 0xe0, 0,
                              //c
   0x50, 0x70, 0x20, 0,
   0x50, 0x70, 0x60, 0,
   0x50, 0x70, 0xa0, 0,
   0x50, 0x70, 0xe0, 0,
                              //10
   0x50, 0x90, 0x20, 0,
   0x50, 0x90, 0x60, 0,
   0x50, 0x90, 0xa0, 0,
   0x50, 0x90, 0xe0, 0,
                              //14
   0x50, 0xb0, 0x20, 0,
   0x50, 0xb0, 0x60, 0,
   0x50, 0xb0, 0xa0, 0,
   0x50, 0xb0, 0xe0, 0,
                              //18
   0x50, 0xd0, 0x20, 0,
   0x50, 0xd0, 0x60, 0,
   0x50, 0xd0, 0xa0, 0,
   0x50, 0xd0, 0xe0, 0,
                              //1c
   0x50, 0xf0, 0x20, 0,
   0x50, 0xf0, 0x60, 0,
   0x50, 0xf0, 0xa0, 0,
   0x50, 0xf0, 0xe0, 0,

                              //20
   0x70, 0x10, 0x20, 0,
   0x70, 0x10, 0x60, 0,
   0x70, 0x10, 0xa0, 0,
   0x70, 0x10, 0xe0, 0,
                              //24
   0x70, 0x30, 0x20, 0,
   0x70, 0x30, 0x60, 0,
   0x70, 0x30, 0xa0, 0,
   0x70, 0x30, 0xe0, 0,
                              //28
   0x70, 0x50, 0x20, 0,
   0x70, 0x50, 0x60, 0,
   0x70, 0x50, 0xa0, 0,
   0x70, 0x50, 0xe0, 0,
                              //2c
   0x70, 0x70, 0x20, 0,
   0x70, 0x70, 0x60, 0,
   0x70, 0x70, 0xa0, 0,
   0x70, 0x70, 0xe0, 0,
                              //30
   0x70, 0x90, 0x20, 0,
   0x70, 0x90, 0x60, 0,
   0x70, 0x90, 0xa0, 0,
   0x70, 0x90, 0xe0, 0,
                              //34
   0x70, 0xb0, 0x20, 0,
   0x70, 0xb0, 0x60, 0,
   0x70, 0xb0, 0xa0, 0,
   0x70, 0xb0, 0xe0, 0,
                              //38
   0x70, 0xd0, 0x20, 0,
   0x70, 0xd0, 0x60, 0,
   0x70, 0xd0, 0xa0, 0,
   0x70, 0xd0, 0xe0, 0,
                              //3c
   0x70, 0xf0, 0x20, 0,
   0x70, 0xf0, 0x60, 0,
   0x70, 0xf0, 0xa0, 0,
   0x70, 0xf0, 0xe0, 0,

                              //0
   0x90, 0x10, 0x00, 0,
   0x90, 0x10, 0x40, 0,
   0x90, 0x10, 0x80, 0,
   0x90, 0x10, 0xc0, 0,
                              //4
   0x90, 0x30, 0x20, 0,
   0x90, 0x30, 0x60, 0,
   0x90, 0x30, 0xa0, 0,
   0x90, 0x30, 0xe0, 0,
                              //8
   0x90, 0x50, 0x20, 0,
   0x90, 0x50, 0x60, 0,
   0x90, 0x50, 0xa0, 0,
   0x90, 0x50, 0xe0, 0,
                              //c
   0x90, 0x70, 0x20, 0,
   0x90, 0x70, 0x60, 0,
   0x90, 0x70, 0xa0, 0,
   0x90, 0x70, 0xe0, 0,
                              //10
   0x90, 0x90, 0x20, 0,
   0x90, 0x90, 0x60, 0,
   0x90, 0x90, 0xa0, 0,
   0x90, 0x90, 0xe0, 0,
                              //14
   0x90, 0xb0, 0x20, 0,
   0x90, 0xb0, 0x60, 0,
   0x90, 0xb0, 0xa0, 0,
   0x90, 0xb0, 0xe0, 0,
                              //18
   0x90, 0xd0, 0x20, 0,
   0x90, 0xd0, 0x60, 0,
   0x90, 0xd0, 0xa0, 0,
   0x90, 0xd0, 0xe0, 0,
                              //1c
   0x90, 0xf0, 0x20, 0,
   0x90, 0xf0, 0x60, 0,
   0x90, 0xf0, 0xa0, 0,
   0x90, 0xf0, 0xe0, 0,

                              //20
   0xb0, 0x10, 0x20, 0,
   0xb0, 0x10, 0x60, 0,
   0xb0, 0x10, 0xa0, 0,
   0xb0, 0x10, 0xe0, 0,
                              //24
   0xb0, 0x30, 0x20, 0,
   0xb0, 0x30, 0x60, 0,
   0xb0, 0x30, 0xa0, 0,
   0xb0, 0x30, 0xe0, 0,
                              //28
   0xb0, 0x50, 0x20, 0,
   0xb0, 0x50, 0x60, 0,
   0xb0, 0x50, 0xa0, 0,
   0xb0, 0x50, 0xe0, 0,
                              //2c
   0xb0, 0x70, 0x20, 0,
   0xb0, 0x70, 0x60, 0,
   0xb0, 0x70, 0xa0, 0,
   0xb0, 0x70, 0xe0, 0,
                              //30
   0xb0, 0x90, 0x20, 0,
   0xb0, 0x90, 0x60, 0,
   0xb0, 0x90, 0xa0, 0,
   0xb0, 0x90, 0xe0, 0,
                              //34
   0xb0, 0xb0, 0x20, 0,
   0xb0, 0xb0, 0x60, 0,
   0xb0, 0xb0, 0xa0, 0,
   0xb0, 0xb0, 0xe0, 0,
                              //38
   0xb0, 0xd0, 0x20, 0,
   0xb0, 0xd0, 0x60, 0,
   0xb0, 0xd0, 0xa0, 0,
   0xb0, 0xd0, 0xe0, 0,
                              //3c
   0xb0, 0xf0, 0x20, 0,
   0xb0, 0xf0, 0x60, 0,
   0xb0, 0xf0, 0xa0, 0,
   0xb0, 0xf0, 0xe0, 0,

                              //0
   0xd0, 0x10, 0x20, 0,
   0xd0, 0x10, 0x60, 0,
   0xd0, 0x10, 0xa0, 0,
   0xd0, 0x10, 0xe0, 0,
                              //4
   0xd0, 0x30, 0x20, 0,
   0xd0, 0x30, 0x60, 0,
   0xd0, 0x30, 0xa0, 0,
   0xd0, 0x30, 0xe0, 0,
                              //8
   0xd0, 0x50, 0x20, 0,
   0xd0, 0x50, 0x60, 0,
   0xd0, 0x50, 0xa0, 0,
   0xd0, 0x50, 0xe0, 0,
                              //c
   0xd0, 0x70, 0x20, 0,
   0xd0, 0x70, 0x60, 0,
   0xd0, 0x70, 0xa0, 0,
   0xd0, 0x70, 0xe0, 0,
                              //10
   0xd0, 0x90, 0x20, 0,
   0xd0, 0x90, 0x60, 0,
   0xd0, 0x90, 0xa0, 0,
   0xd0, 0x90, 0xe0, 0,
                              //14
   0xd0, 0xb0, 0x20, 0,
   0xd0, 0xb0, 0x60, 0,
   0xd0, 0xb0, 0xa0, 0,
   0xd0, 0xb0, 0xe0, 0,
                              //18
   0xd0, 0xd0, 0x20, 0,
   0xd0, 0xd0, 0x60, 0,
   0xd0, 0xd0, 0xa0, 0,
   0xd0, 0xd0, 0xe0, 0,
                              //1c
   0xd0, 0xf0, 0x20, 0,
   0xd0, 0xf0, 0x60, 0,
   0xd0, 0xf0, 0xa0, 0,
   0xd0, 0xf0, 0xe0, 0,

                              //20
   0xf0, 0x10, 0x20, 0,
   0xf0, 0x10, 0x60, 0,
   0xf0, 0x10, 0xa0, 0,
   0xf0, 0x10, 0xe0, 0,
                              //24
   0xf0, 0x30, 0x20, 0,
   0xf0, 0x30, 0x60, 0,
   0xf0, 0x30, 0xa0, 0,
   0xf0, 0x30, 0xe0, 0,
                              //28
   0xf0, 0x50, 0x20, 0,
   0xf0, 0x50, 0x60, 0,
   0xf0, 0x50, 0xa0, 0,
   0xf0, 0x50, 0xe0, 0,
                              //2c
   0xf0, 0x70, 0x20, 0,
   0xf0, 0x70, 0x60, 0,
   0xf0, 0x70, 0xa0, 0,
   0xf0, 0x70, 0xe0, 0,
                              //30
   0xf0, 0x90, 0x20, 0,
   0xf0, 0x90, 0x60, 0,
   0xf0, 0x90, 0xa0, 0,
   0xf0, 0x90, 0xe0, 0,
                              //34
   0xf0, 0xb0, 0x20, 0,
   0xf0, 0xb0, 0x60, 0,
   0xf0, 0xb0, 0xa0, 0,
   0xf0, 0xb0, 0xe0, 0,
                              //38
   0xf0, 0xd0, 0x20, 0,
   0xf0, 0xd0, 0x60, 0,
   0xf0, 0xd0, 0xa0, 0,
   0xf0, 0xd0, 0xe0, 0,
                              //3c
   0xf0, 0xf0, 0x20, 0,
   0xf0, 0xf0, 0x60, 0,
   0xf0, 0xf0, 0xa0, 0,
   0xf0, 0xf0, 0xe0, 0,
};

//----------------------------

bool C_rgb_conversion_i::Convert(const void *src, void *dst, dword sx, dword sy,
   dword src_pitch1, dword dst_pitch, byte srcbpp, dword **pal_argb, dword flags,
   dword color_key) const{

   if(!inited) return false;
   int i;

   //dword code;                //used for compressed data
   //dword *p_offss;     //pointer to compressed data offsets
   dword pixel;
   //dword pnum, cnum;
   //bool packed_counted = false;
   int src_pitch = src_pitch1;

   assert(dst);
   if(!dst_pitch)
      dst_pitch = sx * (!(pf.flags&PIXELFORMAT_COMPRESS) ? pf.bytes_per_pixel : 2);

   if(flags&MCONV_FLIP_Y){
                              //swap source (destination might be compressed)
      src = ((byte*)src) + src_pitch*(sy-1);
      src_pitch = -src_pitch;
   }
                              //source
   const byte *bp=(const byte*)src;
   union{                     //destination
      byte *p_bp;
      word *p_wp;
      dword *p_dp;
      void *p_vp;
   };
   p_vp = dst;

   /*
   if(flags&MCONV_PACKED){
                              //skip offsets
      p_offss = p_dp;
      p_bp += sy*4, bp += sy*4;
   }
   */

   if(!(pf.flags&PIXELFORMAT_COMPRESS)){
      switch(srcbpp){
      case 1:
         switch(pf.bytes_per_pixel){
         case 1:
#ifndef GL
            if(pf.flags&PIXELFORMAT_PALETTE){       //just copy
               /*
               if(flags&MCONV_PACKED){
                                 //copy offsets
                  memcpy(p_offss, src, sy*4);
                                 //copy data
                  if(!packed_counted)
                     CountPackedPixels((byte*)src, sy, &pnum, &cnum);
                  memcpy(p_bp, bp, pnum + cnum);
               }else
               */
               {
                  do{
                     memcpy(p_bp, bp, sx);
                  }while(bp += src_pitch, p_bp += dst_pitch, --sy);
               }
            }else
#endif
            {               //map paletized into 8-bit rgb
               //if(flags&MCONV_PACKED) return false;
               if(flags&MCONV_DITHER){
                  Dither(src, dst, src_pitch, dst_pitch, sx, sy, 1, *pal_argb);
               }else{
                  do{
                     for(dword xx=0; xx<sx; xx++){
                        const byte *palp_argb = ((byte*)*pal_argb) + bp[xx] * sizeof(dword);

                        p_bp[xx] = byte(
                           r.bfield[palp_argb[0]] |
                           g.bfield[palp_argb[1]] |
                           b.bfield[palp_argb[2]] );
                     }
                  }while(bp += src_pitch, p_bp += dst_pitch, --sy);
               }
               break;
            }
            break;
         case 2:                 //... hicolor
            /*
            if(flags&MCONV_PACKED){
               do{
                                 //store new offset
                  *p_offss++ = p_bp-(byte*)dst;
                                 //make line
                                 //get counter & do until eol
                  while(code=(*p_bp++)=(*bp++), code){
                     if(!(code&0x80)){
                                 //data length
                        do{
                           const byte *palp_argb = ((byte*)*pal_argb) + (*bp) * sizeof(dword);
                           *p_wp =
                              r.wfield[palp_argb[0]] |
                              g.wfield[palp_argb[1]] |
                              b.wfield[palp_argb[2]];
                                 //map super-black pixels to non-black ones
                           if(!*p_wp) ++*p_wp;
                        }while(++p_wp, ++bp, --code);
                     }
                  }
               }while(--sy);
            }else
            */
            {
               if(!*pal_argb)
                  *pal_argb = (dword*)uni_pal[0];
               {
   #if defined _MSC_VER && 0     //slower than C++ !
                  word *tb = r.wfield;
                  word *p_dst = p_wp;

                                 //eax - free
                                 //ebx - fetch RGB from palette
                                 //ecx - row counter
                                 //edx - source color index
                                 //esi - source
                                 //edi - dest
                                 //ebp - rgb converson tables base
                                 //esp[4] - palette*
                                 //esp[8] - size_x
                                 //esp[12] - src_pitch
                                 //esp[16] - dst_pitch
                                 //esp[20] - sy
                                 //esp[24] - sx/2
                  if(sx&1){
                                 //version working with word-s
                     __asm{
                        push ecx

                        mov esi, src
                        mov edi, p_dst
                        mov edx, pal_argb
                        push [edx]
                        push ebp
                        xor edx, edx
                     ly:
                        mov ecx, sx
                        mov ebp, tb
                     lx:

                        mov dl, [esi]
                        inc esi
                        lea ebx, [edx*4]
                        add ebx, [esp+4]
                        mov ebx, [ebx]
                        mov dl, bl
                        and ebx, 0ffffffh
                        mov ax, [ebp+edx*2]
                        mov dl, bh
                        shr ebx, 16
                        or ax, [ebp+200h+edx*2]
                        or ax, [ebp+400h+ebx*2]
                        mov [edi], ax
                        add edi, 2
                        dec ecx
                        jnz lx

                        mov ebp, [esp]
                        sub esi, sx
                        add esi, src_pitch
                        sub edi, sx
                        sub edi, sx
                        add edi, dst_pitch

                        dec sy
                        jne ly

                        pop ebp
                        add esp, 4

                        pop ecx
                     }
                  }else{
                                 //version working with dword-s
                     __asm{
                        push ecx

                        mov esi, src
                        mov edi, p_dst
                        mov edx, pal_argb
                        push sx
                        shr dword ptr [esp], 1
                        push sy
                        push dst_pitch
                        push src_pitch
                        push sx
                        push [edx]
                        push ebp
                        xor edx, edx
                        mov ebp, tb
                     ly1:
                        mov ecx, [esp+24]
                     lx1:
                        mov dl, [esi+1]
                        lea ebx, [edx*4]
                        add ebx, [esp+4]
                        mov ebx, [ebx]
                        mov dl, bl
                        and ebx, 0ffffffh
                        mov ax, [ebp+edx*2]
                        mov dl, bh
                        shr ebx, 16
                        or ax, [ebp+200h+edx*2]
                        mov dl, [esi]
                        or ax, [ebp+400h+ebx*2]

                        lea ebx, [edx*4]
                        rol eax, 16
                        add ebx, [esp+4]
                        add esi, 2
                        mov ebx, [ebx]
                        mov dl, bl
                        and ebx, 0ffffffh
                        mov ax, [ebp+edx*2]
                        mov dl, bh
                        shr ebx, 16
                        or ax, [ebp+200h+edx*2]
                        add edi, 4
                        or ax, [ebp+400h+ebx*2]
                     
                        dec ecx
                        mov [edi-4], eax

                        jnz lx1

                        sub esi, [esp+8]
                        add esi, [esp+12]
                        sub edi, [esp+8]
                        sub edi, [esp+8]
                        add edi, [esp+16]

                        dec dword ptr [esp+20]
                        jne ly1

                        pop ebp
                        add esp, 24

                        pop ecx
                     }
                  }
   #else
                  for(; --sy; bp += src_pitch, p_bp += dst_pitch){
                     word *p_wp1 = p_wp;
                     const byte *bp1 = bp;
                     for(int sx1=sx; sx1--; ){
                        byte *palp = (byte*)&(*pal_argb)[*bp1++];
                        *p_wp1++ =
                           r.wfield[palp[0]] |
                           g.wfield[palp[1]] |
                           b.wfield[palp[2]];// | a_bit_mask;
                     }
                  }
   #endif
               }
            }
            break;
         case 3:                 //... 24-bit true color
            /*
            if(flags&MCONV_PACKED){
               do{
                                 //store new offset
                  *p_offss++ = p_bp-(byte*)dst;
                                 //make line
                                 //get counter & do until eol
                  while(code=(*p_bp++)=(*bp++), code){
                     if(!(code&0x80))
                                 //data length
                     do{
                        const byte *palp_argb = ((byte*)*pal_argb) + (*bp) * sizeof(dword);
                                 //get pixel
                        pixel =
                           (palp_argb[0]<<r_pos) |
                           (palp_argb[1]<<g_pos) |
                           (palp_argb[2]<<b_pos);
                                 //store pixel
                        *p_wp = pixel;
                        p_bp[2] = pixel>>16;
                     }while(p_bp+=3, ++bp, --code);
                  }
               }while(--sy);
            }else
            */
            {
               do{
                  union{
                     word *p_wp1;
                     byte *p_bp1;
                  };
                  p_bp1=p_bp;
                  for(dword xx=0; xx<sx; xx++, p_bp1+=3){
                     const byte *palp_argb = ((byte*)*pal_argb) + bp[xx] * sizeof(dword);
                                 //get pixel
                     pixel =
                        (palp_argb[0]<<r_pos) |
                        (palp_argb[1]<<g_pos) |
                        (palp_argb[2]<<b_pos);
                                 //store pixel
                     *p_wp1 = (word)pixel;
                     p_bp1[2] = (byte)(pixel>>16);
                  }
               }while(bp += src_pitch, p_bp += dst_pitch, --sy);
            }
            break;
         case 4:                 //... 32-bit true color
            /*
            if(flags&MCONV_PACKED){
               do{
                                 //store new offset
                  *p_offss++ = p_bp-(byte*)dst;
                                 //make line
                                 //get counter & do until eol
                  while(code=(*p_bp++)=(*bp++), code){
                     if(!(code&0x80))
                                 //data length
                     do{
                        const byte *palp_argb = ((byte*)*pal_argb) + (*bp) * sizeof(dword);
                        *p_dp =
                           (palp_argb[0]<<r_pos) |
                           (palp_argb[1]<<g_pos) |
                           (palp_argb[2]<<b_pos);
                     }while(++p_dp, ++bp, --code);
                  }
               }while(--sy);
            }else
            */
            {
               if(pal_argb){
                  do{
                     const byte *bp1=bp;
                     dword *p_dp1=p_dp;
                     dword xx=sx;
                     do{
                        const byte *palp_argb = ((byte*)*pal_argb) + (*bp1++) * sizeof(dword);
                        *p_dp1++ =
                           (palp_argb[0]<<r_pos) |
                           (palp_argb[1]<<g_pos) |
                           (palp_argb[2]<<b_pos) |
                           0xff000000;
                     }while(--xx);
                  }while(bp += src_pitch, p_bp += dst_pitch, --sy);
               }else{
                  assert(0);
               }
            }
            break;
         }
         break;

      case 2:                    //16-bit to...
         switch(pf.bytes_per_pixel){
         case 2:
            do{                  //just copy
               memcpy(p_bp, bp, sx*2);
            }while(bp += src_pitch, p_bp += dst_pitch, --sy);
            break;
         }
         break;

      case 3:                    //24-bit to...
      case 4:                    //32-bit (rgba) to...
         switch(pf.bytes_per_pixel){
         case 1:                 //... 8-bit
#ifndef GL
            if(IsPaletized() && init_pal_argb){
               do{               //map into specified palette
                  const byte *bp1=bp;
                  byte *p_bp1=p_bp;
                  dword xx=sx;
                  do{
                                 //get source rgb
                     int rgb[3];
                     rgb[0] = bp1[0];
                     rgb[1] = bp1[1];
                     rgb[2] = bp1[2];
                                 //get closest r value (index into rgbi table)
                     byte index=g.r2rgbi[rgb[RGB_MASTER]];
                                 //now walk up & down the list to find closest rgb val
                     S_rgbi *p_rgbi=&r.rgbi[index], *p_rgbi1=p_rgbi;
                     dword best_delta, delta;
                     best_delta=rgb[RGB_MASTER]-(int)p_rgbi->rgb[RGB_MASTER] +
                        abs(rgb[RGB_SLAVE1]-(int)p_rgbi->rgb[RGB_SLAVE1]) +
                        abs(rgb[RGB_SLAVE2]-(int)p_rgbi->rgb[RGB_SLAVE2]);
                     byte best_index=p_rgbi->index;
                                 //traverse both directions
                     byte index1=index;

                     while(index1<255 && index){
                        ++index1;
                        dword delta1=(int)(++p_rgbi1)->rgb[RGB_MASTER] - rgb[RGB_MASTER];
                                 //check if worth further traversing
                        if(delta1>=best_delta) break;
                                 //get full delta
                        delta1 +=
                           abs(rgb[RGB_SLAVE1]-(int)p_rgbi1->rgb[RGB_SLAVE1])+
                           abs(rgb[RGB_SLAVE2]-(int)p_rgbi1->rgb[RGB_SLAVE2]);
                        if(best_delta>delta1){
                           best_delta=delta1;
                           best_index=p_rgbi1->index;
                        }

                        delta=rgb[RGB_MASTER]-(int)(--p_rgbi)->rgb[RGB_MASTER];
                                 //check if worth further traversing
                        if(delta>=best_delta) break;
                        --index;
                                 //get full delta
                        delta +=
                           abs(rgb[RGB_SLAVE1]-(int)p_rgbi->rgb[RGB_SLAVE1])+
                           abs(rgb[RGB_SLAVE2]-(int)p_rgbi->rgb[RGB_SLAVE2]);
                        if(best_delta>delta){
                           best_delta=delta;
                           best_index=p_rgbi->index;
                        }
                     }
                                 //traverse up
                     while(++index1){
                        ++p_rgbi1;
                        delta=(int)p_rgbi1->rgb[RGB_MASTER] - rgb[RGB_MASTER];
                                 //check if worth further traversing
                        if(delta>=best_delta) break;
                                 //get full delta
                        delta +=
                           abs(rgb[RGB_SLAVE1]-(int)p_rgbi1->rgb[RGB_SLAVE1])+
                           abs(rgb[RGB_SLAVE2]-(int)p_rgbi1->rgb[RGB_SLAVE2]);
                        if(best_delta>delta){
                           best_delta=delta;
                           best_index=p_rgbi1->index;
                        }
                     }
                                 //traverse down
                     while(index--){
                        --p_rgbi;
                        delta=rgb[RGB_MASTER]-(int)p_rgbi->rgb[RGB_MASTER];
                                 //check if worth further traversing
                        if(delta>=best_delta) break;
                                 //get full delta
                        delta +=
                           abs(rgb[RGB_SLAVE1]-(int)p_rgbi->rgb[RGB_SLAVE1])+
                           abs(rgb[RGB_SLAVE2]-(int)p_rgbi->rgb[RGB_SLAVE2]);
                        if(best_delta>delta){
                           best_delta=delta;
                           best_index=p_rgbi->index;
                        }
                     }
                     *p_bp1++ = best_index;
                  }while(bp1+=srcbpp, --xx);
               }while(bp += src_pitch, p_bp += dst_pitch, --sy);
            }else
#endif
            {               //3-3-2 mode
#ifndef GL
               if(IsPaletized()){
                                 //paletized mode, create 3-3-2 palette
                  byte *bp1;
                  bp1 = (byte*)(*pal_argb = new dword[256]);
                  bp1[0] = bp1[1] = bp1[2] = 0;
                  for(i=1; i<256; i++){
                     bp1 += sizeof(dword);
                     bp1[0] = byte((i&0xe0) | 0x10);
                     bp1[1] = byte(((i<<3)&0xe0) | 0x10);
                     bp1[2] = byte((i<<6) | 0x20);
                     bp1[3] = 0xff;
                  }
               }
#endif
               if(flags&MCONV_DITHER){
                  Dither(src, dst, src_pitch, dst_pitch, sx, sy, srcbpp, NULL);
               }else{
                  do{
                     const byte *bp1=bp;
                     for(dword xx=0; xx<sx; xx++, bp1+=srcbpp){
                        p_bp[xx] = byte(
                           r.bfield[bp1[2]] |
                           g.bfield[bp1[1]] |
                           b.bfield[bp1[0]] );
                     }
                  }while(bp += src_pitch, p_bp += dst_pitch, --sy);
               }
            }
            break;

         case 2:                 //... 16-bit
            if(flags&MCONV_DITHER){
               Dither(src, dst, src_pitch, dst_pitch, sx, sy, srcbpp, NULL);
            }else{
               do{
                  const byte *bp1 = bp;
                  switch(srcbpp){
                  case 3:
                     {
                        word alpha = a.wfield[0xff];
                        for(dword xx=0; xx<sx; xx++, bp1+=3){
                           p_wp[xx] = word(
                              r.wfield[bp1[2]] |
                              g.wfield[bp1[1]] |
                              b.wfield[bp1[0]] |
                              alpha);
                        }
                     }
                     break;
                  case 4:
                     {
                        for(dword xx=0; xx<sx; xx++, bp1+=4){
                           p_wp[xx] = word(
                              r.wfield[bp1[2]] |
                              g.wfield[bp1[1]] |
                              b.wfield[bp1[0]] |
                              a.wfield[bp1[3]] );
                        }
                     }
                     break;
                  }
               }while(bp += src_pitch, p_bp += dst_pitch, --sy);
            }
            break;

         case 3:                 //... 24-bit
         case 4:                 //... 32-bit
            {
               bool do_memcpy;
               if(!(flags&MCONV_BGR))
                  do_memcpy = ((srcbpp==pf.bytes_per_pixel) && (r_byte_pos==2) && (g_byte_pos==1) && (b_byte_pos==0));
               else
                  do_memcpy = ((srcbpp==pf.bytes_per_pixel) && (r_byte_pos==0) && (g_byte_pos==1) && (b_byte_pos==2));

               if(do_memcpy){
                  do{               //just copy
                     memcpy(p_bp, bp, sx*srcbpp);
                  }while(bp += src_pitch, p_bp += dst_pitch, --sy);
               }else{
                  dword r_pos = (flags&MCONV_BGR) ? 0 : 2;
                  dword b_pos = 2 - r_pos;
                  if(srcbpp==4 && pf.bytes_per_pixel==4){
                     do{         //copy with alpha
                        const byte *bp1 = bp;
                        for(dword xx=0; xx<sx; xx++, bp1 += srcbpp, ++p_dp){
                           p_bp[r_byte_pos] = bp1[r_pos];
                           p_bp[g_byte_pos] = bp1[1];
                           p_bp[b_byte_pos] = bp1[b_pos];
                           p_bp[a_byte_pos] = bp1[3];
                        }
                     }while(bp += src_pitch, p_bp += dst_pitch - sx*4, --sy);
                  }else{
                     if(pf.bytes_per_pixel==3){
                        do{         //copy without alpha
                           const byte *bp1 = bp;
                           for(dword xx=0; xx<sx; xx++, bp1 += srcbpp, p_bp += 3){
                              p_bp[r_byte_pos] = bp1[r_pos];
                              p_bp[g_byte_pos] = bp1[1];
                              p_bp[b_byte_pos] = bp1[b_pos];
                           }
                        }while(bp += src_pitch, p_bp += dst_pitch - sx*3, --sy);
                     }else{
                        do{         //copy with explicit alpha set to 255
                           const byte *bp1 = bp;
                           for(dword xx=0; xx<sx; xx++, bp1 += srcbpp, p_bp += 4){
                              p_bp[r_byte_pos] = bp1[r_pos];
                              p_bp[g_byte_pos] = bp1[1];
                              p_bp[b_byte_pos] = bp1[b_pos];
                              p_bp[a_byte_pos] = 255;
                           }
                        }while(bp += src_pitch, p_bp += dst_pitch - sx*4, --sy);
                     }
                  }
               }
            }
            break;
         }
         break;
      }
      return true;
   }
//----------------------------
                              //compressed destination
   if(srcbpp==2)
      return false;

                              //destination compressed block
   struct S_surf_block{
      word color0;
      word color1;
      dword pixel_mask;
   };
   S_surf_block *sb = (S_surf_block*)dst;

                              //ARGB color value
   struct S_argb{
      union{
         struct{
            byte b, g, r, a;
         } element;
         dword argb;
      };
   };
   struct S_argb_pixel: public S_argb{
      dword brightness;

      inline dword Store(dword dw){
         argb = dw;
         brightness = (element.r*77 + element.g*153 + element.b*26);
         return brightness;
      }
   };

   dword num_blocks_x = sx/4;
   dword num_blocks_y = sy/4;

   bool has_alpha = (((byte*)&pf.four_cc)[3] != '1');
   //bool is_ckey = ((flags&MCONV_COLORKEY) && !has_alpha);

   static const dword opaque_pixel_map[4] = {
      1, 3, 2, 0
   }, ckey_pixel_map[3] = {
      1, 2, 0
   };

                              //dithering of alpha channel - alloc buffer for y distribution error
   int *dither_y_error = NULL;
   if(has_alpha){
      dither_y_error = new int[sx];
                              //clear buffers (line 0 starts with no error)
      memset(dither_y_error, 0, sx*sizeof(int));
   }


                              //iterate through all vertical blocks
   for(dword loop_y = 0; loop_y<num_blocks_y; loop_y++){

                              //dithering x distribution error (for 4 vertical pixels of a block)
      int dither_x_error[4];
      memset(dither_x_error, 0, sizeof(dither_x_error));

      int *y_error_base = dither_y_error;

                              //read-in and preprocess all horizontal blocks
      for(dword loop_x = 0; loop_x<num_blocks_x; loop_x++){
         S_argb_pixel pixels[16];

         bool transp[16];        //true if pixel is transparent
         dword num_trans = 0;

                              //use histogram
                              //get medium brightness of block
         dword mid_brightness = 0;

         switch(srcbpp){
         case 1:
            {
               i = 0;
               byte *bp = ((byte*)src) + loop_x*4;
               S_argb tmp;
               for(int y=0; y<4; y++){
                  for(int x=0; x<4; x++, i++){
                     byte input = bp[x];
                     if(flags&MCONV_COLORKEY){
                        byte is_ck = (input==color_key);
                        transp[i] = is_ck;
                        if(is_ck){
                           ++num_trans;
                           continue;
                        }
                     }
                     tmp.argb = (*pal_argb)[input];
                           //palette has swapped R and B components
                     Swap(tmp.element.r, tmp.element.b);
                     mid_brightness += pixels[i].Store(tmp.argb);
                  }
                  bp += src_pitch;
               }
            }
            break;
         case 3:
         case 4:
            {
               i = 0;
               dword *dwp = ((dword*)src) + loop_x * srcbpp;
               switch(srcbpp){
               case 3:
                  {
                     for(dword y=0; y<4; y++){
                        dword *dwp1 = dwp;
                        for(dword x=0; x<4; x++, i++, dwp1 = (dword*)(((byte*)dwp1) + 3)){
                           union U{
                              dword argb;
                              struct{
                                 word gb;
                                 byte r;
                                 byte a;
                              } element;
                           };
                           U tmp = (U&)pixels[i].argb;
                           tmp.element.gb = *(word*)dwp1;
                           tmp.element.r = *(((byte*)dwp1)+2);
                           tmp.element.a = 0;
                           dword br = pixels[i].Store(tmp.argb);

                           if(flags&MCONV_COLORKEY){
                              byte is_ck = (tmp.argb==color_key);
                              transp[i] = is_ck;
                              num_trans += is_ck;
                              if(is_ck)
                                 br = 0;
                           }
                           mid_brightness += br;
                        }
                        dwp = (dword*)(((byte*)dwp) + src_pitch);
                     }
                  }
                  break;

               case 4:
                  {
                     if(flags&MCONV_COLORKEY){
                        for(dword y=0; y<4; y++){
                           for(dword x=0; x<4; x++, i++){
                              dword br = pixels[i].Store(dwp[x]);
                              byte is_ck = (pixels[i].element.a < 0x80);
                              transp[i] = is_ck;
                              num_trans += is_ck;
                              if(!is_ck)
                                 mid_brightness += br;
                           }
                           dwp = (dword*)(((byte*)dwp) + src_pitch);
                        }
                     }else{
                        for(dword y=0; y<4; y++){
                           for(dword x=0; x<4; x++, i++){
                              dword br = pixels[i].Store(dwp[x]);
                              byte is_trans = (pixels[i].element.a == 0);
                              transp[i] = is_trans;
                              num_trans += is_trans;
                              if(!is_trans)
                                 mid_brightness += br;
                           }
                           dwp = (dword*)(((byte*)dwp) + src_pitch);
                        }
                     }
                  }
                  break;
               }
            }
            break;
         }
                              //get medium brightness
         if(num_trans!=16)
            mid_brightness = (mid_brightness+1) / (16-num_trans);

                              //two border colors of block
         S_argb color_min, color_max;

         color_min.argb = 0;
         color_max.argb = 0;

         {
            dword sum_min[3], sum_max[3];
            memset(sum_min, 0, sizeof(sum_min));
            memset(sum_max, 0, sizeof(sum_max));
            dword num_min = 0, num_max = 0;

                              //compute min and max colors
                              // - add all participants on both sides of histogram medium value
                              // - scale by distance from medium, in order to achieve better contrast
#if 0
            for(i=0; i<16; i++){
               if(num_trans && transp[i])
                  continue;
               const S_argb_pixel &px = pixels[i];
               int br_delta = mid_brightness - px.brightness;
               if(br_delta >= 0){
                  dword k = (256 + br_delta) >> 8;
                  num_min += k;
                  sum_min[0] += px.r * k;
                  sum_min[1] += px.g * k;
                  sum_min[2] += px.b * k;
               }else{
                  dword k = (256 - br_delta) >> 8;
                  num_max += k;
                  sum_max[0] += px.r * k;
                  sum_max[1] += px.g * k;
                  sum_max[2] += px.b * k;
               }
            }
#else
            __asm{
                              //eax = free
                              //ebx = free
                              //ecx = free
                              //edx = free
                              //esi = pixels
                              //edi = count
               xor edi, edi
               lea esi, pixels
            l2:
               cmp num_trans, 0
               jz no_t2
               cmp transp[edi], 0
               jne tr2
            no_t2:
               mov edx, mid_brightness
               mov ebx, [esi+edi*SIZE S_argb_pixel]
               xor eax, eax
               sub edx, [esi+edi*SIZE S_argb_pixel + 4]
               js sig2
               sar edx, 8
               inc edx
                              //b
               mov al, bl
               xor ecx, ecx
               imul eax, edx
               mov cl, bh
               add sum_min[8], eax
                              //g
               shr ebx, 16
               imul ecx, edx
               and ebx, 0xff
               add sum_min[4], ecx
                              //r
               imul ebx, edx
               add num_min, edx
               add sum_min[0], ebx
               jmp tr2
            sig2:
               neg edx
               sar edx, 8
               inc edx
                              //b
               mov al, bl
               xor ecx, ecx
               imul eax, edx
               mov cl, bh
               add sum_max[8], eax
                              //g
               shr ebx, 16
               imul ecx, edx
               and ebx, 0xff
               add sum_max[4], ecx
                              //r
               imul ebx, edx
               add num_max, edx
               add sum_max[0], ebx
            tr2:
               inc edi
               cmp edi, 16
               jne l2
            }
#endif
            if(num_min){
               int r_num_min = 65536 / num_min;
               /*
               color_min.r = sum_min[0] / num_min;
               color_min.g = sum_min[1] / num_min;
               color_min.b = sum_min[2] / num_min;
               */
               color_min.element.r = byte((sum_min[0]*r_num_min) >> 16);
               color_min.element.g = byte((sum_min[1]*r_num_min) >> 16);
               color_min.element.b = byte((sum_min[2]*r_num_min) >> 16);
            }
            if(num_max){
               int r_num_max = 65536 / num_max;
               /*
               color_max.r = sum_max[0] / num_max;
               color_max.g = sum_max[1] / num_max;
               color_max.b = sum_max[2] / num_max;
               */
               color_max.element.r = byte((sum_max[0] * r_num_max) >> 16);
               color_max.element.g = byte((sum_max[1] * r_num_max) >> 16);
               color_max.element.b = byte((sum_max[2] * r_num_max) >> 16);
            }else
               color_max = color_min;
         }

                              //two border colors in 565 format
         dword color_min_565, color_max_565;
         {
                              //discard unused bits (targtet format will be 5-6-5)
            color_min.argb &= 0x00f8fcf8;
            color_max.argb &= 0x00f8fcf8;

                              //encode 2 border colors into compressed block (format RGB565)
            dword c;
            c = color_max.argb;
            color_min_565 = ((c&0xf80000) >> 8) | ((c&0x00fc00) >> 5) | ((c&0x0000f8) >> 3);
            c = color_min.argb;
            color_max_565 = ((c&0xf80000) >> 8) | ((c&0x00fc00) >> 5) | ((c&0x0000f8) >> 3);
            if(num_trans && !has_alpha){
                              //min color must be less than max color
               if(color_min_565 > color_max_565){
                  Swap(color_min_565, color_max_565);
                  Swap(color_min.argb, color_max.argb);
               }
            }else{
                              //min color must be greater than max color
               if(color_min_565 < color_max_565){
                  Swap(color_min_565, color_max_565);
                  Swap(color_min.argb, color_max.argb);
               }
            }
         }

         if(has_alpha){
                              //fill-in alpha component (directly into destination)
            if(srcbpp!=4){
                              //no source alpha, set to max
               *(dword*)((byte*)sb+0) = 0xffffffff;
               *(dword*)((byte*)sb+4) = 0xffffffff;
            }else{
               i = 0;
               word *dst = (word*)sb;
               for(dword py=0; py<4; py++){
                  word &dst_word = dst[py];
                  dst_word = 0;
                  for(dword px=0; px<4; px++, i++){
                     int as = Min(0xff, pixels[i].element.a + (y_error_base[px] + dither_x_error[py]) / 2);
                     if(num_trans && transp[i])
                        as = 0;
                     int ad = as&0xf0;
                     y_error_base[px] = dither_x_error[py] = as - ad;
                              //store alpha
                     dst_word |= Rol(ad, byte(px*4-4));
                  }
               }
            }
            ++sb;
            num_trans = 0;
         }
                              //store encoded colors into destination
         sb->color0 = (word)color_min_565;
         sb->color1 = (word)color_max_565;

         int delta_minmax = (
            abs(color_max.element.r - color_min.element.r)*77 +
            abs(color_max.element.g - color_min.element.g)*153 +
            abs(color_max.element.b - color_min.element.b)*25);
                              //get reciprocal of delta_minmax in 16:16 fixed-point format
         int r_delta_minmax = 0;
         if(delta_minmax){
            dword x = (delta_minmax+0x8) >> 4;
            if(x)
               r_delta_minmax = 65536 / x;
         }
         int delta_minmax_half = (delta_minmax+1)/2;

                              //encode all pixels to border colors
         {
            dword pixel_mask = 0;

            if(num_trans){
               for(i=0; i<16; i++){
                  if(transp[i])
                     pixel_mask |= 3 << (i*2);
                  else{
                     const S_argb &src_pixel = pixels[i];
                     int delta_min = (
                        (src_pixel.element.r - color_min.element.r)*77 +
                        (src_pixel.element.g - color_min.element.g)*153 +
                        (src_pixel.element.b - color_min.element.b)*25);
                     delta_min = abs(delta_min);
                                 //get ratio in range 0...2
                     int ratio = ((delta_min*2 + delta_minmax_half) * r_delta_minmax) >> 20;
                     ratio = Max(0, Min(2, ratio));
                                 //store pixel
                     pixel_mask |= ckey_pixel_map[ratio] << (i*2);
                  }
               }
            }else{
#if 1
               for(i=0; i<16; i++){
                  const S_argb &src_pixel = pixels[i];
                  int delta_min = ((src_pixel.element.r - color_min.element.r)*77 + (src_pixel.element.g - color_min.element.g)*153 +
                     (src_pixel.element.b - color_min.element.b)*25);
                  delta_min = abs(delta_min);
                              //get ratio in range 0...3
                  int ratio = ((delta_min*3 + delta_minmax_half) * r_delta_minmax) >> 20;
                  ratio = Max(0, Min(3, ratio));
                              //store pixel
                  pixel_mask |= opaque_pixel_map[ratio] << (i*2);
               }
#else
               int color_min_r = color_min.element.r;
               int color_min_g = color_min.element.g;
               int color_min_b = color_min.element.b;
               delta_minmax_half = delta_minmax_half / 3;
               r_delta_minmax = r_delta_minmax * 3;
               __asm{
                              //eax = free
                              //ebx = argb
                              //ecx = counter
                              //edx = free
                              //esi = pixels
                              //edi = opaque_pixel_map
                  xor ecx, ecx
                  lea esi, pixels
                  lea edi, opaque_pixel_map
               l0:
                              //b
                  xor eax, eax
                  mov ebx, [esi+ecx*SIZE S_argb_pixel]
                  xor edx, edx
                  mov al, bl
                  sub eax, color_min_b
                  mov dl, bh
                  imul eax, 25
                              //g
                  shr ebx, 16
                  sub edx, color_min_g
                  and ebx, 0xff
                  imul edx, 153
                              //r
                  sub ebx, color_min_r
                  add eax, edx
                  imul ebx, ebx, 77
                              //int ratio = ((delta_min + delta_minmax_half/3) * r_delta_minmax*3) >> 20;
                  add eax, ebx
                  add eax, delta_minmax_half
                  imul eax, r_delta_minmax
                  sar eax, 20
                              //ratio = Max(0, Min(3, ratio));
                  jns nos0
                  xor eax, eax
               nos0:
                  cmp eax, 3
                  jle ok0
                  mov eax, 3
               ok0:
                              //pixel_mask |= opaque_pixel_map[ratio] << (i*2);
                  mov eax, [edi + eax*4]
                  shl eax, cl
                  shl eax, cl
                  inc ecx
                  or pixel_mask, eax

                  cmp ecx, 16
                  jnz l0
               }
#endif
            }
            sb->pixel_mask = pixel_mask;
         }
                              //move to next 4x4 block
         ++sb;
      }
                              //next block line
      src = ((byte*)src) + src_pitch*4;
   }
   delete[] dither_y_error;

   return true;
}

//----------------------------

bool C_rgb_conversion_i::AlphaBlend(void *dst, void *src, const dword *pal_argb,
   dword sx, dword sy, dword src_pitch, dword dst_pitch, byte srcbpp,
   dword flags) const{

   if(srcbpp==2
#ifndef GL
      || (pf.flags&PIXELFORMAT_PALETTE)
#endif
      )
      return false;

   union{
      byte *dst_bp;
      word *dst_wp;
      dword *dst_dwp;
      void *dst_vp;
   };
   dst_bp = NULL;
   union{
      byte *src_bp;
      void *src_vp;
   };
   bool inverse = (flags&MCONV_INVALPHA);
   dword mask_8;
   dword not_alpha = 0;

   if(!pal_argb)
      pal_argb = (const dword*)uni_pal[0];

   bool compressed = (pf.flags&PIXELFORMAT_COMPRESS);
   if(!compressed){
      mask_8 = (byte)Ror(pf.a_mask, byte(FindLastBit(pf.a_mask)-7));
      not_alpha = ~pf.a_mask;
      if(pf.bytes_per_pixel==3)
         not_alpha |= 0xff000000;
   }else{
      mask_8 = 0xf0;
   }

   dword *error_y = new dword[sx];
   memset(error_y, 0, sx*sizeof(dword));

   for(dword y=0; y<sy; y++){
      if(!compressed)
         dst_vp = dst;
      src_vp = src;

      dword error_x = 0;

      for(dword x=0; x<sx; x++){

         dword new_alpha;
         switch(srcbpp){
         case 1:
            {
               byte *bp = ((byte*)pal_argb) + (*src_bp) * sizeof(dword);
               new_alpha = (bp[0]*77 + bp[1]*153 + bp[2]*26)/256;
            }
            break;
         default:
            {
               byte *bp = src_bp;
               new_alpha = (bp[2]*77 + bp[1]*153 + bp[0]*26)/256;
            }
         }

         if(inverse)
            new_alpha = new_alpha^255;

         if(flags&MCONV_DITHER){
                              //dither
                              //get source alpha
            dword as = min(255ul, new_alpha + (error_y[x] + error_x)/2);
                              //get destination alpha
            new_alpha = as&mask_8;
                              //distribute error right & down
            error_y[x] = error_x = (as-new_alpha);
         }

         if(!compressed){
            new_alpha = Rol(new_alpha, a_pos) & pf.a_mask;
            switch(pf.bytes_per_pixel){
            case 1:
               *dst_bp &= not_alpha;
               *dst_bp |= new_alpha;
               break;
            case 2:
               *dst_wp &= not_alpha;
               *dst_wp |= new_alpha;
               break;
            case 3:
               *dst_wp &= not_alpha;
               dst_bp[2] &= (not_alpha>>16);
               *dst_wp |= new_alpha;
               dst_bp[2] |= (new_alpha>>16);
               break;
            case 4:
               *dst_dwp &= not_alpha;
               *dst_dwp |= new_alpha;
               break;
            }
         }else{
            word *dst_bl = (word*)(((byte*)dst) + (y/4)*(sx/4)*16 + (x/4)*16 + (y&3)*2);
            dword xpos = (x&3);
            (*dst_bl) &= alpha_and_mask[xpos];
            (*dst_bl) |= Rol(new_alpha&0xf0, byte(xpos*4-4));
         }
         if(!compressed)
            dst_bp += pf.bytes_per_pixel;
         src_bp += srcbpp;
      }
      if(!compressed)
         dst = (void*)(((byte*)dst)+dst_pitch);
      src = (void*)(((byte*)src)+src_pitch);
   }
   delete[] error_y;

   return true;
}

//----------------------------

static byte SingleIndex(const byte *pal, byte r, byte g, byte b){

   const int firstpal = 1, lastpal = 255;

   const byte *bp = pal + firstpal * sizeof(dword);
   byte col = 0;
   int bestval = 256*256, val;
   for(int cnt = firstpal; ++cnt < lastpal; ){
      int delta_r = 256 + ((int)bp[0]-(int)r);
      delta_r *= delta_r;
      int delta_g = 256+((int)bp[1]-(int)g);
      delta_g *= delta_g;
      int delta_b = 256+((int)bp[2]-(int)b);
      delta_b *= delta_b;

      val = delta_r + delta_g + delta_b;
         
      if(bestval>val){
         bestval = val;
         col = (byte)cnt;
      }
      bp += 4;
   }
   return col;
}

//----------------------------

dword C_rgb_conversion_i::GetPixel(byte r, byte g, byte b, byte a, byte *pal_argb) const{

   switch(pf.bytes_per_pixel){
   case 1:                    //remap to palette
#ifndef GL
      if(pf.flags&PIXELFORMAT_PALETTE){
         if(!pal_argb) return 0;
         return SingleIndex(pal_argb, r, g, b);
      }else
#endif
      {
         return
            (Rol(r, r_pos) & pf.r_mask) |
            (Rol(g, g_pos) & pf.g_mask) |
            (Rol(b, b_pos) & pf.b_mask) ;
      }
   case 2:                    //16-bit
      return
         (Rol(r, r_pos) & pf.r_mask) |
         (Rol(g, g_pos) & pf.g_mask) |
         (Rol(b, b_pos) & pf.b_mask) |
         (Rol(a, a_pos) & pf.a_mask);
   case 3:                    //24-bit
      return (r<<r_pos) | (g<<g_pos) | (b<<b_pos);
   case 4:                    //32-bit
      return (r<<r_pos) | (g<<g_pos) | (b<<b_pos) | (a<<a_pos);
   default:
      return 0;
   }
}

//----------------------------

void C_rgb_conversion_i::InverseRGB(dword val, byte *r, byte *g, byte *b, byte *a,
   const S_pixelformat *pf1, byte *pal_argb) const{

   if(!pf1)
      pf1 = &pf;

   switch(pf1->bytes_per_pixel){
   case 1:                             //paletized
      if(pal_argb && val<256){
                                       //val is index into palette
         pal_argb += (val&0xff) * sizeof(dword);
                                       //return palette entry
         *r = pal_argb[0];
         *g = pal_argb[1];
         *b = pal_argb[2];
         *a = pal_argb[3];
      }else{
         *r = 0, *g = 0, *b = 0;
         *a = 0xff;
      }
      break;
   case 2:                    //16-bit RGB
      {
         val &= 0xffff;
                              //get rotation values
         int r_pos = (FindLastBit(pf1->r_mask)-7) & 31;
         int g_pos = (FindLastBit(pf1->g_mask)-7) & 31;
         int b_pos = (FindLastBit(pf1->b_mask)-7) & 31;
         int a_pos = (FindLastBit(pf1->a_mask)-7) & 31;

         *r = (byte)Ror(val&pf1->r_mask, (byte)r_pos);
         *g = (byte)Ror(val&pf1->g_mask, (byte)g_pos);
         *b = (byte)Ror(val&pf1->b_mask, (byte)b_pos);
         *a = (byte)Ror(val&pf1->a_mask, (byte)a_pos);
         if(!pf1->a_mask) *a = 255;
      }
      break;
   case 3: 
      *r = byte(val>>16);
      *g = byte(val>>8);
      *b = byte(val>>0);
      *a = 0xff;
      break;
   case 4:   
      *r = byte(val>>16);
      *g = byte(val>>8);
      *b = byte(val>>0);
      *a = byte(val>>24);
      break;
   }
}

//----------------------------
                              //convert paletized or RGB (24, 32-bit) data
                              //into 8 or 16-bit RGB formats

                              //note: dither doesn't support alpha channel
void C_rgb_conversion_i::Dither(const void *src, void *dst, dword pitch_src,
   dword pitch_dst, dword size_x, dword size_y, dword srcbpp, const dword *src_pal_argb) const{

                              //shift RGB masks to 8-bit positions
   dword mask_8[4];
#ifndef GL
   if(IsPaletized()){
      mask_8[0] = 0xe0;
      mask_8[1] = 0xe0;
      mask_8[2] = 0xc0;
   }else
#endif
   {
      mask_8[3] = (byte)Ror(pf.a_mask, byte(FindLastBit(pf.a_mask)-7));
      mask_8[2] = (byte)Ror(pf.r_mask, byte(FindLastBit(pf.r_mask)-7));
      mask_8[1] = (byte)Ror(pf.g_mask, byte(FindLastBit(pf.g_mask)-7));
      mask_8[0] = (byte)Ror(pf.b_mask, byte(FindLastBit(pf.b_mask)-7));
   }
                              //alloc buffer for y distribution error
   int *error = new int[size_x*4];
                              //clear buffers (line 0 starts with no error)
   memset(error, 0, 4*size_x*sizeof(int));
                              //line loop
   dword x, y;
   for(y=0; y<size_y; y++){
                              //x distribution error,
                              //start with 0
      int error_x[4];
      error_x[0] = 0;
      error_x[1] = 0;
      error_x[2] = 0;
      error_x[3] = 0;

      int *error_p = error;
      union{
         byte *dst_bp;
         word *dst_wp;
         void *dst_vp;
      };
      const byte *b_src = (const byte*)src;
      dst_vp = dst;
                              //row loop
      switch(pf.bytes_per_pixel){
      case 1:
         for(x=0; x<size_x; x++, b_src+=srcbpp, error_p+=4){
                              //get source RGB triplet
                              //if paletized, use source index to pal
                              //else use source directly
            const byte *bp = (srcbpp==1) ? (((byte*)src_pal_argb)+(*b_src) * sizeof(dword)) : b_src;
                              //get source pixel and
                              //add distributed error
            int rs = min(255, bp[2]+(error_p[2]+error_x[2])/2);
            int gs = min(255, bp[1]+(error_p[1]+error_x[1])/2);
            int bs = min(255, bp[0]+(error_p[0]+error_x[0])/2);
                              //get destination color
            int rd = rs & mask_8[2];
            int gd = gs & mask_8[1];
            int bd = bs & mask_8[0];
                              //distribute error right & down
            error_p[2] = error_x[2] = rs - rd;
            error_p[1] = error_x[1] = gs - gd;
            error_p[0] = error_x[0] = bs - bd;
                              //store destination pixel
#ifndef GL
            if(!IsPaletized())
#endif
               Swap(rd, bd);
            *dst_bp++ = byte(
               r.bfield[rd] |
               g.bfield[gd] |
               b.bfield[bd] );
         }
         break;
      case 2:
         for(x=0; x<size_x; x++, b_src+=srcbpp, error_p+=4){
                              //get source RGB triplet
                              //if paletized, use source index to pal
                              //else use source directly
            const byte *bp = (srcbpp==1) ? (byte*)(src_pal_argb + *b_src) : b_src;
                              //get source pixel and
                              //add distributed error
            int ad = 0;
            if(srcbpp==4){
               int as = min(255, bp[3]+(error_p[3]+error_x[3])/2);
               ad = as & mask_8[3];
               error_p[3] = error_x[3] = as - ad;
            }

            int rs = min(255, bp[2]+(error_p[2]+error_x[2])/2);
            int rd = rs & mask_8[2];
            error_p[2] = error_x[2] = rs - rd;

            int gs = min(255, bp[1]+(error_p[1]+error_x[1])/2);
            int gd = gs & mask_8[1];
            error_p[1] = error_x[1] = gs-gd;

            int bs = min(255, bp[0]+(error_p[0]+error_x[0])/2);
            int bd = bs & mask_8[0];
            error_p[0] = error_x[0] = bs-bd;

                              //store destination pixel
            *dst_wp++ = word(
               r.wfield[rd] |
               g.wfield[gd] |
               b.wfield[bd] |
               a.wfield[ad] );
         }
         break;
      }
                              //add line's pitch
      src = (void*)(((byte*)src) + pitch_src);
      dst = (void*)(((byte*)dst) + pitch_dst);
   }
   delete[] error;
}

//----------------------------

C_rgb_conversion *Create_rgb_conv(){
   return new C_rgb_conversion_i;
}

//----------------------------
 