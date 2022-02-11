/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.
   All rights reserved.

   File: Lmap.cpp
   Content: Lit-object frame.
--------------------------------------------------------*/

#include "all.h"
#include "light.h"
#include "visual.h"
#include "scene.h"
#include "camera.h"
#include "Compress.h"
#include "mesh.h"
#include <integer.h>

//----------------------------

//#define USE_ASM               //use inline assembly parts of code

#define LM_REQUEST_TEXTURE_SIZE 256
#define LM_MAX_COMPUTE_SIZE 256  //maximal size of lightmap we may compute

#define MAX_LM_PIXELS (256*256*16)  //max # of pixels we may allocate for lightmapping

//#define LMAP_RANDOM_INIT      //init by random contents
//#define FADE_SHADOWS

#define EDGE_OUT_DISTANCE 3.1f   //lumel's distance from edge, behind which it is considered not to affect particular rectangle
#define INSIDE_PUSH_RATIO .0075f  //ratio at which we push reference point inside of closest rectangle during light computation - this causes proper collision detection
#define LM_CONSTR_PLANAR_THRESH (PI*.6f) //angle at which faces are considered planar (between same smoothgroup)
#define EXPAND_LUMEL_SIZE .5f //number of lumels to expand edges on each side of rectangle

#define MIN_RESOLUTION .001f  //minimal resolution user may set

#define LM_ALLOC_ERR(driver,msg) driver->DEBUG(msg, 2)

#define MIN_PROGRESS .01f        //how often progress indicator is called (ratio 0 ... 1)
#define MIN_PROGRESS_DELAY 300   //how often we show progress

#ifdef _DEBUG

#define LM_ALLOC_LOG(driver,n)
//#define LM_ALLOC_LOG(driver,msg) driver->PRINT(msg)

                              //debugging:
//#define DEBUG_DRAW_LUMEL_NORMAL
//#define DEBUG_DRAW_RAYS
//#define DEBUG_REAL_TIME       //recompute lighting in real-time
//#define DEBUG_NO_COLLISIONS
//#undef LM_REQUEST_TEXTURE_SIZE
//#define LM_REQUEST_TEXTURE_SIZE 16
//#define DEBUG_NO_BLUR
//#define DEBUG_MIX_BLEND_IMAGE_COLOR //during mixing blend image's alpha, mix its color into lightmap

#else
#define LM_ALLOC_LOG(driver,n)
#endif

static const float unit_light = 128.0f;

//----------------------------
// Fill rectangle in a texture by white (debugging purposes).
static void FillTextureByWhite(PI3D_texture tp, const I3D_rectangle *rc = NULL){

   D3DLOCKED_RECT lrc;
   CPIImage img = tp->GetSysmemImage();
   dword bpp = img->GetPixelFormat()->bytes_per_pixel;
   HRESULT hr = img->GetDirectXTexture()->LockRect(0, &lrc, (LPRECT)rc, 0);
   if(FAILED(hr))
      return;

   int sx = !rc ? img->SizeX() : (rc->r - rc->l);
   int sy = !rc ? img->SizeY() : (rc->b - rc->t);
   for(int i=sy; i--; ){
      memset(lrc.pBits, -1, sx*bpp);
      (byte*&)lrc.pBits += lrc.Pitch;
   }
   img->GetDirectXTexture()->UnlockRect(0);
   tp->SetDirty();
}

/*--------------------------------------------------------
   C_LM_alloc_block
   Abstract:
      Allocation handle of multiple LM rectangles - used
      by single LM object to assure that all LMaps are
      on same texture.
--------------------------------------------------------*/

struct I3D_LM_rect{
   I3D_rectangle rect;
   bool dirty;
};

class C_LM_alloc_block{
public:
   class C_LM_surface *lms;   //for use by LM manager only
   C_vector<I3D_LM_rect> rect_list;
   dword last_render_time;

   C_LM_alloc_block(){}
   C_LM_alloc_block(int num_rects):
      last_render_time(0),
      lms(NULL)
   {
      rect_list.resize(num_rects);
   }
};

/*--------------------------------------------------------
   C_rect_allocator
   Abstract:
      Memory allocation manager of rectanguar areas on a surface.
--------------------------------------------------------*/

class C_rect_allocator{
public:
   struct S_block{
      C_LM_alloc_block *owner_id; //identificator of owner
      I3D_rectangle rect;     //position+size
   };
private:
   C_vector<S_block> block_list;//currently allocated blocks
   int sx, sy;                //size of rectangle

                              //allocation help:
   int size_free;
   short *alloc_map_x_y;      //X * Y per-block allocation info, highest bit marks if block is used
   short *line_info;          //info per each line - # of blocks free
public:
   C_rect_allocator(int sx1, int sy1):
      sx(sx1),
      sy(sy1),
      size_free(sx1*sy1)
   {
      alloc_map_x_y = new short[sx * sy + sy];
      line_info = alloc_map_x_y + (sx * sy);
      for(int y=sy; y--; ){
         line_info[y] = (short)sx;
         short *alloc_map_x = alloc_map_x_y + (y * sx);
         for(int x=sx+1; --x; ++alloc_map_x)
            *alloc_map_x = (short)x;
      }
   }
   ~C_rect_allocator(){
#ifdef _DEBUG
      assert(size_free == sx*sy);
                              //assert that alloc maps are valid upon exit
      for(int y=0; y<sy; y++){
         assert(line_info[y] == sx);
         for(int x=0; x<sx; x++)
            assert(alloc_map_x_y[y*sx + x] == sx-x);
      }
#endif
      delete[] alloc_map_x_y;
   }
   bool Allocate(int req_size_x, int req_size_y, C_LM_alloc_block *owner, I3D_rectangle &out_rc);
   void Free(C_LM_alloc_block *owner);
   inline int GetFreeSize() const{ return size_free; }
   inline int GetTotalSize() const{ return sx * sy; }

   inline int NumBlocks() const{ return block_list.size(); }
   inline const S_block &GetBlock(int i) const{ return block_list[i]; }
};

//----------------------------

bool C_rect_allocator::Allocate(int req_size_x, int req_size_y, C_LM_alloc_block *owner, I3D_rectangle &out_rc){

                              //check invalid requirements
   if(req_size_x>sx || req_size_y>sy) return false;

   if(!block_list.size()){
                              //take 1st position
      out_rc.l = 0;
      out_rc.t = 0;
   }else{
                              //first scan vertical list to find appropriate area
      int y;
      int i;

      for(y=0; y <= sy-req_size_y; y++){
         if(line_info[y] >= req_size_x){
                              //scan next n lines to see if their size matches
            for(i=1; i<req_size_y; i++){
               if(line_info[y+i] < req_size_x)
                  break;
            }
            if(i==req_size_y){
               while(true){
                              //lines found, try columns
                  short *alloc_map_x = alloc_map_x_y + (y * sx);
                  for(int x=0; x!=sx; ){
                     assert(x<sx);
                     int bl_free = alloc_map_x[x];
                              //block allocated or not satisfying, skip
                     if(bl_free<req_size_x){
                              //move past the failed area
                        x += bl_free&0x7fff;
                        continue;
                     }
                              //block ok, try to fit on the area,
                              // but consult fail tries first
                     {
                              //try down
                        for(i=1; i<req_size_y; i++){
                           if(alloc_map_x_y[(y+i)*sx + x] < req_size_x)
                              break;
                        }
                        if(i==req_size_y){
                              //alloc ok, take this block
                           out_rc.l = x;
                           out_rc.t = y;
                           goto ok;
                        }
                              //this column failed, move past the failed area
                        x += bl_free;
                     }
                  }
                              //alloc failed, move on to next line
                  ++y;
                  if(y > (sy-req_size_y))
                     break;
                  if(line_info[y+req_size_y-1] < req_size_x)
                     break;
                              //ok, continue with this segment
               }
            }else{
                              //move past the failed area
               y += i;
            }
         }
      }
                              //no satisfying block found, alloc failed
      return false;
   }
ok:
                              //save block
   out_rc.r = out_rc.l + req_size_x;
   out_rc.b = out_rc.t + req_size_y;
   block_list.push_back(S_block());
   block_list.back().owner_id = owner;
   block_list.back().rect = out_rc;
                              //count free size
   size_free -= req_size_x * req_size_y;

                              //adjust alloc maps
   {
                              //mark all lines
      for(int y=out_rc.t; y<out_rc.b; y++){
                              //lower free space on line
         line_info[y] = short(line_info[y] - req_size_x);
                              //create block
         short *alloc_map_x = alloc_map_x_y + (y * sx);
         for(int x=out_rc.r; --x >= out_rc.l; )
            alloc_map_x[x] = word((out_rc.r - x) | 0x8000);

         if(x>=0){
            if(alloc_map_x[x]&0x8000){
               ++x;
                              //adjust used block
               while(x-- && (alloc_map_x[x]&0x8000))
                  alloc_map_x[x] = word((out_rc.l - x) | 0x8000);
            }else{
               ++x;
                              //adjust unused block
               while(x-- && !(alloc_map_x[x]&0x8000))
                  alloc_map_x[x] = word(out_rc.l - x);
            }
         }
      }
   }
#ifdef _DEBUG
                              //assert that blocks never overlap
   {
      for(int i=block_list.size()-1; i--; ){
         const I3D_rectangle &rc = block_list[i].rect;
         bool overlap = (rc.r > out_rc.l && rc.l < out_rc.r &&
            rc.b > out_rc.t && rc.t < out_rc.b);
         assert(!overlap);
      }
   }
#endif

   return true;
}

//----------------------------

void C_rect_allocator::Free(C_LM_alloc_block *owner){

                              //free all blocks of the 'owner'
   for(int i=block_list.size(); i--; ){
      S_block &bl = block_list[i];
      if(bl.owner_id==owner){
                           //count free size
         const I3D_rectangle &rc = bl.rect;
         size_free += (rc.r - rc.l) * (rc.b - rc.t);
                           //adjust alloc maps
         int req_size_x = rc.r - rc.l;
         for(int y=rc.t; y<rc.b; y++){
                              //adjust free space on line
            line_info[y] = word(line_info[y] + req_size_x);
            short *alloc_map_x = alloc_map_x_y + (y * sx);

            int free_base = 0;
            if(rc.r != sx && alloc_map_x[rc.r] > 0)
               free_base = alloc_map_x[rc.r];

                              //kill block
            for(int x=rc.r; --x >= rc.l; )
               alloc_map_x[x] = word(free_base + (rc.r - x));

            if(x>=0){
               if(alloc_map_x[x]&0x8000){
                  ++x;
                              //adjust used block
                  while(x-- && (alloc_map_x[x]&0x8000))
                     alloc_map_x[x] = word((rc.l - x) | 0x8000);
               }else{
                  ++x;
                              //adjust unused block
                  while(x-- && !(alloc_map_x[x]&0x8000))
                     alloc_map_x[x] = word(free_base + (rc.r - x));
               }
            }
         }
                           //erase block
         bl = block_list.back(); block_list.pop_back();
      }
   }
}

/*--------------------------------------------------------
   C_LM_surface
   Abstract:
      Logical surface which allows allocation of small rectangular areas used as light-maps.
      - contains a texture which provides physical surface
      - manages allocation
--------------------------------------------------------*/

class C_LM_surface: public C_unknown{
public:
   C_rect_allocator alloc;
   C_smart_ptr<I3D_texture> txt;

   C_LM_surface(PI3D_texture txt1):
      alloc(txt1->SizeX1(), txt1->SizeY1()),
      txt(txt1)
   { }
};

//----------------------------

struct S_lm_face{
   I3D_triface fc;
   int face_index;         //index into original faces
};

//----------------------------
//----------------------------

class I3D_lit_object: public I3D_visual{

#define LMAPF_CONSTRUCTED     1     //set when LM rectanges are created and lit
#define LMAPF_DYNLIGHT_VALID  2     //dynamic light prepared properly (between AddPrims/DrawPrim calls)
#define LMAPF_HAS_ALPHA       4     //alpha channel in pixels is valid
#define LMAPF_DRAW_LIGHT_MAP  0x10  //light-map should be drawn, in addiction to vertex lighting

   dword lmap_flags;
   C_smart_ptr<I3D_mesh_base> mesh;

#pragma pack(push,1)
   struct S_rgb{
      byte b, g, r;
      inline bool operator ==(const S_rgb &rgb) const{
         return (rgb.r==r && rgb.g==g && rgb.b==b);
      }
   };

   struct S_lmap_pixel{
      union{
         struct{
            byte b, g, r;
            byte a;
         };
         dword argb;
      };

      void Zero(){
         argb = 0;
      };
      void Add(byte r1, byte g1, byte b1, byte a1){
         r = (byte)Min(255, r+r1);
         g = (byte)Min(255, g+g1);
         b = (byte)Min(255, b+b1);
         a = (byte)Min(255, a+a1);
      }
      inline bool operator ==(const S_lmap_pixel &lmp) const{
         return (argb==lmp.argb);
      }
      inline S_rgb ToRGB(){
         S_rgb ret;
         ret.r = r;
         ret.g = g;
         ret.b = b;
         return ret;
      }
      inline void operator =(const S_rgb &rgb){
         r = rgb.r;
         g = rgb.g;
         b = rgb.b;
         a = 0;
      }
   };
#pragma pack(pop)

//----------------------------
                              //light-mapping:
   struct S_lmap_rectangle: public C_unknown{
                              //matrix for rectangle representation,
                              // in lmap object's local coords
      S_matrix m;
      S_matrix inv_m;
      float z_delta;

      bool dirty;             //need re-loading into cached lmap

      dword size_x, size_y;
      S_lmap_pixel *dst_map;  //RGB light-map representation

      S_lmap_rectangle():
         dst_map(NULL),
         z_delta(0.0f),
         dirty(true)
      {}
      S_lmap_rectangle(const S_lmap_rectangle &l1);
      S_lmap_rectangle &operator =(const S_lmap_rectangle&);
      ~S_lmap_rectangle(){
         delete[] dst_map;
      }
   };
   C_buffer<C_smart_ptr<S_lmap_rectangle> > lm_rects;
   C_buffer<C_buffer<word> > face_lmap_use;  //[num_fgroups][num_faces_in_group] - index into 'lm_rects'

   bool any_rect_dirty;       //set if any of lm_rects is dirty

   class C_LM_alloc_block *lm_alloc_block; //light-map, run-time allocated
   float curr_resolution;

//----------------------------
// Allocate lightmap textures and upload info there.
   void UploadLMaps();

   void MapVertices();
   void CheckUV();

   typedef void t_DP(const S_preprocess_context &pc, const S_render_primitive &rp, CPI3D_face_group fgrps,
      dword num_fg,
      dword vertex_count, dword base_index);
   t_DP I3D_lit_object::*DrawPrim;

                              //various techniques
#ifndef GL
                              // notation:
                              // Direct, Indirect = mode how verticess are processed
                              // B=base texture, L=lightmap, S=secondary, D=detailmap (lower case = optional)
                              // + = singlepass, | = additional pass
   t_DP DPUnconstructed;                  //LM unconstructed
   t_DP DP_B;                             //B (base only)
   t_DP DP_L;                             //L (LM only)
   t_DP DPDirect_BLd, DPIndirect_BLd;     //B + L + d
   t_DP DPDirect_BLSd, DPIndirect_BLSd;   //B + L + S + d
   t_DP DPDirect_B_L, DPIndirect_B_L;     //B | L
   t_DP DPDirect_BL_D, DPIndirect_BL_D;   //(B + L) | D
   t_DP DPDirect_B_L_D, DPIndirect_B_L_D; //B | L | D
   t_DP DPDirect_D, DPIndirect_D;         //? | D

#endif
                              //and pixel-shader counterparts
   t_DP DPUnconstructed_PS;               //LM unconstructed
   t_DP DP_B_PS;                          //B (base only)
   t_DP DP_BLd_PS;                        //B + L + d
   t_DP DP_BLSd_PS;                        //B + L + S + d
//----------------------------

   struct S_lumel{
      S_vector color;
      float alpha;
      dword num_samples;
      bool antialiased;
#ifdef _DEBUG
      bool debug_mark;
#endif

      void SolveBrightness(){
         assert(num_samples);
         if(num_samples<=16){
            static const float mul_map[] = {
               0.0f, 1/1.0f, 1/2.0f, 1/3.0f,
               1/4.0f, 1/5.0f, 1/6.0f, 1/7.0f,
               1/8.0f, 1/9.0f, 1/10.0f, 1/11.0f,
               1/12.0f, 1/13.0f, 1/14.0f, 1/15.0f,
               1/16.0f
            };
            color *=(mul_map[num_samples]);
         }else{
            color /= (float)num_samples;
         }
         num_samples = 1;
      }

      void SetInvalid(float x, float y, float z){
         color.x = x;
         color.y = y;
         color.z = z;
         alpha = 0;
         num_samples = 0;
      }

      inline bool IsValid() const{ return (num_samples!=0); }
   };

//----------------------------

   static S_vectorw GetTexturePixel(CPI3D_driver drv, const void *area, dword pitch, const S_pixelformat *pf, dword x, dword y){

      S_vectorw ret;

      byte r, g, b, a;
      if(pf->flags&PIXELFORMAT_COMPRESS){
         dword block_x = x / 4;
         dword block_y = y / 4;
         dword block_offset_x = x&3;
         dword block_offset_y = y&3;
         dword block_size = 8;
         bool has_alpha = (pf->four_cc > '1TXD');
         if(has_alpha)
            block_size *= 2;
         const word *block_addr = (word*)(((byte*)area) + pitch*block_y + block_x*block_size);

         if(has_alpha){
            word alpha_word = *(block_addr + block_offset_y);
            a = byte(((alpha_word >> (block_offset_x*4)) & 0xf) << 4);
            block_addr += 4;
         }else{
            a = 0xff;
         }
         word color_0 = block_addr[0];
         word color_1 = block_addr[1];

         struct S_color{
            dword r, g, b;
            S_color(){}
            S_color(word c_565){
               r = (c_565>>8)&0xf8;
               g = (c_565>>3)&0xfc;
               b = (c_565<<3)&0xf8;
            }
            S_color operator *(int k){
               S_color c;
               c.r = r * k;
               c.g = g * k;
               c.b = b * k;
               return c;
            }
            S_color operator /(int k){
               S_color c;
               c.r = r / k;
               c.g = g / k;
               c.b = b / k;
               return c;
            }
            S_color operator +(const S_color &cc){
               S_color c;
               c.r = r + cc.r;
               c.g = g + cc.g;
               c.b = b + cc.b;
               return c;
            }
         };
         S_color c0(color_0);
         S_color c1(color_1);

         block_addr += 2;

         dword code = ((((byte*)block_addr)[block_offset_y]) >> (block_offset_x*2)) & 0x3;
         S_color color;
         if(color_0 > color_1){
            switch(code){
            case 0: color = c0; break;
            case 1: color = c1; break;
            case 2: color = (c0*2 + c1) / 3; break;
            case 3: color = (c0 + c1*2) / 3; break;
            default: assert(0); color = 0;
            }
         }else{
            switch(code){
            case 0: color = c0; break;
            case 1: color = c1; break;
            case 2: color = (c0 + c1) / 2; break;
            case 3: color = 0; a = 0;
            default: assert(0); color = 0;
            }
         }
         r = (byte)color.r;
         g = (byte)color.g;
         b = (byte)color.b;
      }else{
         dword texel;
         byte *bp = ((byte*)area) + pitch*y + pf->bytes_per_pixel*x;
         switch(pf->bytes_per_pixel){
         case 1: texel = *bp; break;
         case 2: texel = *(word*)bp; break;
         case 3:
            *(word*)&texel = *(word*)bp;
            *(((byte*)&texel)+2) = *(((byte*)bp)+2);
            break;
         case 4: texel = *(dword*)bp; break;
         default: assert(0); texel = 0;
         }
         LPC_rgb_conversion rgb_conv = drv->GetGraphInterface()->CreateRGBConv();
         rgb_conv->InverseRGB(texel, &r, &g, &b, &a, pf, NULL);
         rgb_conv->Release();
      }
                           //get color of texel
      ret = S_vector(b*R_255, g*R_255, r*R_255);
      ret.w = a*R_255;

      return ret;
   }

//----------------------------

   static bool GetTexturePixel(PI3D_driver drv, const void *mem, dword pitch, const S_vector2 &uv, S_vectorw &ret, bool filter = false){

   }

//----------------------------
// Setup currently selected LM cached texture into the texture stage 0.
// Also disable anisotropy filtering for the stage.
   inline void SetupLMTextureToStage0(){
      drv->SetTexture1(0, lm_alloc_block->lms->txt);
      drv->EnableAnisotropy(0, true);
   }

public:
//----------------------------
// Get pixel of texture on specified uv coordinates.
// Returned value is in vector, plus alpha (0.0 ... 1.0).
// Compressed textures are supported.
   static bool GetTexturePixel(PI3D_driver drv, CPIImage img, const S_vector2 &uv, S_vectorw &ret, bool filter = false){

      const void *area;
      dword pitch;
      if(!((PIImage)img)->Lock((void**)&area, &pitch))
         return false;
      const S_pixelformat *pf = img->GetPixelFormat();

      float u = (float)fmod(uv.x, 1.0f);
      float v = (float)fmod(uv.y, 1.0f);
      if(u<0.0f) u += 1.0f;
      if(v<0.0f) v += 1.0f;
      float real_x = u * img->SizeX();
      float real_y = v * img->SizeY();
      dword x = FloatToInt(real_x);
      dword y = FloatToInt(real_y);
      x = Min(x, img->SizeX()-1);
      y = Min(y, img->SizeY()-1);

      if(!filter){
         ret = GetTexturePixel(drv, area, pitch, pf, x, y);
      }else{
                              //filter among four samples
         float rem_x = real_x - (float)x;
         float rem_y = real_y - (float)y;
         dword x1 = x + 1;
         if(rem_x < 0.0f){
            rem_x = -rem_x;
            x1 -= 2;
         }
         dword y1 = y + 1;
         if(rem_y < 0.0f){
            rem_y = -rem_y;
            y1 -= 2;
         }
         x1 %= img->SizeX();
         y1 %= img->SizeY();

         S_vectorw c00 = GetTexturePixel(drv, area, pitch, pf, x, y);
         S_vectorw c01 = GetTexturePixel(drv, area, pitch, pf, x1, y);
         S_vectorw c0 = c00*(1.0f-rem_x) + c01*rem_x;

         S_vectorw c10 = GetTexturePixel(drv, area, pitch, pf, x, y1);
         S_vectorw c11 = GetTexturePixel(drv, area, pitch, pf, x1, y1);
         S_vectorw c1 = c10*(1.0f-rem_x) + c11*rem_x;

         ret = c0*(1.0f-rem_y) + c1*rem_y;
      }
      
                           //get alpha of texel
      /*
      if(tb->GetTxtFlags()&TXTF_SELF_ALPHA){
         ret.w = ((S_vector&)ret).Square() / 3.0f;
      }else*/
      {
         if(pf->flags&PIXELFORMAT_COMPRESS){
            ret.w *= (255.0f/240.0f);
         }else{
            int num_alpha_bits = CountBits(pf->a_mask);
            if(num_alpha_bits){
               dword max_alpha = 0xff - ((1<<(8-num_alpha_bits)) - 1);
               ret.w *= (255.0f / (float)max_alpha);
            }else
               ret.w = 1.0f;
         }
      }
      ((PIImage)img)->Unlock();
      return true;
   }
private:
//----------------------------

   class C_lm_light{
   public:
      PI3D_light lp;
      //C_vector<PI3D_visual> visuals;
   };

//----------------------------

   struct S_makelight_collision_help{

      S_vector opacity;
      S_vector self_add;
      bool collided;
      bool collision_or;
      float collision_dist;
      bool texture_alpha;
      bool zero_opacity;
      PI3D_driver drv;

//----------------------------

      static bool I3DAPI cbcol(I3D_cresp_data &rd){
         CPI3D_frame frm1 = rd.GetHitFrm();
         do{
            if(frm1->GetFrameFlags()&I3D_FRMF_NOSHADOW)
               return false;
            frm1 = frm1->GetParent1();
         }while(frm1);
         return true;
      }

   //----------------------------
   //Texel collision processor - blending of ray passing matrials.
      bool TexelCollision(I3D_cresp_data &rd){

         CPI3D_frame frm1 = rd.GetHitFrm();
         do{
            if(frm1->GetFrameFlags()&I3D_FRMF_NOSHADOW)
               return false;
            frm1 = frm1->GetParent1();
         }while(frm1);

                           //check texel of texture
         I3D_texel_collision *tc = (I3D_texel_collision*)rd.texel_data;

         S_vector self_color;
         if(tc->mat){
            self_color = tc->mat->GetEmissive();
         }

         CPI3D_texture_base tb = tc->mat->GetTexture1();
         CPIImage img = ((PI3D_texture_base)tb)->GetSysmemImage();
         if(!img)
            return false;
         if(tc->mat->IsDiffuseAlpha() || tc->mat->IsTextureAlpha() || tc->mat->IsAddMode()){
                           //get color of texel
            S_vectorw color;
            if(!GetTexturePixel(drv, img, tc->tex, color))
               return true;
            float alpha = color.w;

            if(tc->mat)
               alpha *= tc->mat->GetAlpha();

            opacity *= (1.0f - alpha);
            opacity += (opacity * color * alpha);
            if(opacity.IsNull())
               zero_opacity = true;

            collided = true;
            collision_or = true;
            texture_alpha = true;
            //collision_dist = collision_dist;
            collision_dist = rd.GetHitDistance();
         }
         return true;
      }

      static bool I3DAPI cbTexel_thunk(I3D_cresp_data &rd){
         S_makelight_collision_help *hp = (S_makelight_collision_help*)rd.cresp_context;
         if(hp->zero_opacity)
            return true;
         return hp->TexelCollision(rd);
      }
   };

//----------------------------

   void CollectLights(const C_vector<PI3D_light> &sct_lights, const C_vector<PI3D_visual> &vis_list,
      const S_lmap_rectangle &lm_rc, const S_plane &rc_plane,
      C_vector<C_lm_light> &rc_lights, S_vector &ambient) const{

      for(dword i=sct_lights.size(); i--; ){
         PI3D_light lp = sct_lights[i];
         lp->UpdateLight();

         if(!lp->IsOn1() || !(lp->I3D_light::GetMode()&I3DLIGHTMODE_LIGHTMAP) || IsAbsMrgZero(lp->power))
            continue;

         I3D_LIGHTTYPE lt = lp->GetLightType1();
         switch(lt){
         case I3DLIGHT_LAYEREDFOG: case I3DLIGHT_FOG:
         case I3DLIGHT_POINTFOG: case I3DLIGHT_NULL:
            break;
         case I3DLIGHT_AMBIENT:
            {
               S_vector color = lp->color * lp->power;
               swap(color.x, color.z);
               ambient += color;
            }
            break;

         default:
            {
               const S_matrix &m = lp->GetMatrix();

                              //based on type, compute several values
               bool is_spherical = false;
               switch(lt){
                  /*
               case I3DLIGHT_DIRECTIONAL:
                  {
                           //compute direction to light
                     float dir_cos = -(lp->world_dir.Dot(pl.normal));
                           //optimizations: remove lights shining from back
                     if(dir_cos <= 0.0f) continue;
                  }
                  break;
                  */

               case I3DLIGHT_SPOT:
                  {
                           //fast reject lights pointing away
                     float inner, outer;
                     lp->I3D_light::GetCone(inner, outer);
                     float a = (float)acos(lp->normalized_dir.Dot(rc_plane.normal)) + outer * .5f;
                     if(a < PI*.5f)
                        continue;
                  }
                           //flow...
               case I3DLIGHT_POINT:
                  {
                              //fast reject lights too far from object
                     {
                        const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
                        S_vector l_dist = bsphere.pos - m(3);
                        float dist_2 = l_dist.Dot(l_dist);
                        float r_sum = lp->range_f_scaled + bsphere.radius;
                        if((r_sum*r_sum) <= dist_2)
                           continue;
                     }

                              //fast reject lights behind plane
                     float dist_to_plane = rc_plane.normal.Dot(m(3)) + rc_plane.d;
                     if((dist_to_plane + lm_rc.z_delta) <= -lp->range_f_scaled)
                        continue;
                              //fast reject lights too far from plane
                     if(dist_to_plane >= lp->range_f_scaled)
                        continue;
                     is_spherical = true;
                  }
                  break;

               case I3DLIGHT_POINTAMBIENT:
                  {
                              //fast reject lights too far from object
                     const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
                     S_vector l_dist = bsphere.pos - m(3);
                     float dist_2 = l_dist.Dot(l_dist);
                     float r_sum = lp->range_f_scaled + bsphere.radius;
                     if((r_sum*r_sum) <= dist_2)
                        continue;
                     is_spherical = true;
                  }
                  break;
               }

               if(is_spherical){
                              //check if light collides with rectangle
                  const S_vector &lpos = lp->GetWorldPos();
                  float l_radius = lp->range_f_scaled;
                  const S_vector &l_loc_pos = lpos * GetInvMatrix() * lm_rc.m;
                  /*          //MB: not working properly, need to fix
                  float m_scale = GetInvMatrix()(0).Magnitude();
                  //drv->PRINT(l_loc_pos);
                  float scl_x = lm_rc.m(0).Magnitude() * m_scale;
                  float scl_y = lm_rc.m(2).Magnitude() * m_scale;
                  float rx = l_radius * scl_x;
                  float ry = l_radius * scl_y;
                              //if absolutely out of rectangle, skip
                  if(l_loc_pos.x < -rx || l_loc_pos.x > 1.0f+rx ||
                     l_loc_pos.y < -ry || l_loc_pos.y > 1.0f+ry){
                     continue;
                  }
                  */
                  S_matrix mrc = lm_rc.inv_m * matrix;
                  const S_vector rc_pos = mrc(3);
                  const S_vector &rc_dir_x = mrc(0);
                  const S_vector &rc_dir_y = mrc(1);
                              //if partially out, check distances to lines
                  if(l_loc_pos.x <= 0.0f){
                     float d = lpos.DistanceToLine(rc_pos, rc_dir_y);
                     if(d > l_radius)
                        continue;
                  }
                  if(l_loc_pos.x >= 1.0f){
                     float d = lpos.DistanceToLine(rc_pos+rc_dir_x, rc_dir_y);
                     if(d > l_radius)
                        continue;
                  }
                  if(l_loc_pos.y <= 0.0f){
                     float d = lpos.DistanceToLine(rc_pos, rc_dir_x);
                     if(d > l_radius)
                        continue;
                  }
                  if(l_loc_pos.y >= 1.0f){
                     float d = lpos.DistanceToLine(rc_pos+rc_dir_y, rc_dir_x);
                     if(d > l_radius)
                        continue;
                  }
               }
                              //definitelly add this light
               rc_lights.push_back(C_lm_light());
               C_lm_light &lml = rc_lights.back();
               lml.lp = lp;

               /*
               if(is_spherical){
                  S_vector v[4];
                  S_matrix m = lm_rc.inv_m * matrix;
                  v[0] = m(3);
                  v[1] = m(3) + m(0);
                  v[2] = m(3) + m(1);
                  v[3] = m(3) + m(0) + m(1);

                  const S_vector &lpos = lp->GetWorldPos();
                  float max_dist = -1.0f;
                  int max_i = -1;
                  for(dword i=4; i--; ){
                     float d = (v[i] - lpos).Square();
                     if(max_dist<d){
                        max_dist = d;
                        max_i = i;
                     }
                  }
                  I3D_bsphere bs;
                  S_vector dir = v[max_i] - lpos;
                  //max_dist = I3DSqrt(max_dist);
                  bs.pos = lpos + dir * .5f;
                  bs.radius = dir.Magnitude();

                              //add all visuals colliding with the sphere
                  for(i=vis_list.size(); i--; ){
                     PI3D_visual vis = vis_list[i];
                     if(!(vis->GetVisFlags()&VISF_BOUNDS_VALID))
                        vis->ComputeBounds();
                     vis->bound.GetBoundSphereTrans(vis, FRMFLAGS_BSPHERE_TRANS_VALID);
                     float d = (vis->bound.bsphere_world.pos - bs.pos).Magnitude();
                     if(d < vis->bound.bsphere_world.radius+bs.radius)
                        lml.visuals.push_back(vis);
                  }
#if 1
                  {
                     drv->DebugLine(v[0], v[1], 0);
                     drv->DebugLine(v[1], v[3], 0);
                     drv->DebugLine(v[3], v[2], 0);
                     drv->DebugLine(v[2], v[0], 0);
                  }
#endif
               }
               */
            }
         }
      }
      //drv->PRINT(C_fstr("%s: %i", (const char*)GetName(), rc_lights.size())); rc_lights.clear();
   }

//----------------------------

   void CollectRectangleFacesAndVertices(CPI3D_mesh_base mesh, const void *verts, dword vstride, dword lmrc_i,
      const S_lmap_rectangle &lm_rect,
      C_vector<S_lm_face> &rc_faces, C_vector<S_vector2> &verts_trans) const{

      //PI3D_triface faces = ((PI3D_mesh_base)mesh)->index_buffer.Lock(D3DLOCK_READONLY);
      C_buffer<I3D_triface> face_buf(mesh->NumFaces1());
      PI3D_triface faces = face_buf.begin();
      mesh->GetFaces(faces);

      for(dword fgi=0, face_i = 0; fgi<face_lmap_use.size(); fgi++){
         for(dword fi=0; fi<face_lmap_use[fgi].size(); fi++, face_i++, faces++){
            if(face_lmap_use[fgi][fi] == lmrc_i){
               rc_faces.push_back(S_lm_face());
               rc_faces.back().fc = *faces;
               rc_faces.back().face_index = face_i;
            }
         }
      }

      dword i = mesh->vertex_buffer.NumVertices();
      verts_trans.resize(i);
      while(i--){
         const S_vector &v_in = *(S_vector*)(((byte*)verts) + vstride * i);
         //S_vector v = verts[i].xyz * lm_rect.m;
         S_vector v = v_in * lm_rect.m;
         verts_trans[i].x = v.x * (lm_rect.size_x-1);
         verts_trans[i].y = v.y * (lm_rect.size_y-1);
      }
      //mesh->index_buffer.Unlock();
   }

//----------------------------

   struct S_lighten_data{
      PI3D_scene scene;
      const C_vector<S_lm_face> *rc_faces;
      const S_vector2 *rc_verts_trans;
      const S_vector *verts;
      dword vstride;
      C_vector<C_lm_light> rc_lights;
      S_plane rc_plane;
      PI3D_LOAD_CB_PROC cb_proc;
      void *cb_context;
   };

//----------------------------
// Get face index in mesh, onto which is a lumel is mapped.
// 'rc_src_point' is floating 2d point in the source LM rectangle.
// The returned value is the face index.
// If lumel doesn't map directly onto any face, *_out_face_i contains index of closest outer face
//    which was considered as the face, and *_out_edge_i is index of closest edge.
// (otherwise *_out_face_i and *_out_edge_i are set to -1)
   int GetLumelClosestFace(const S_lighten_data &data, const S_vector2 &rc_src_point,
      int *_out_face_i, int *_out_edge_i) const{

                              //face, which this point is definitely inside of
      int in_face_i = -1;
                              //in case lumel is not inside of any face, this is closest face
                              // lumel may be in, with closes edge
      int out_face_i = -1, out_edge_i = -1;
                              //distance of lumel to out-face
      float out_face_dist = 1e+16f;
                              //choose face we should operate on
      for(dword i=data.rc_faces->size(); i--; ){
         const I3D_triface &fc = (*data.rc_faces)[i].fc;
      
         const S_vector2 *v[3] = {
            &data.rc_verts_trans[fc[0]],
            &data.rc_verts_trans[fc[1]],
            &data.rc_verts_trans[fc[2]]
         };
                              //check if point is in triangle,
                              // eventually compute dist to closest edge
         bool in_face = true;
         int out_edge_i1 = -1;
         float out_face_dist1 = 1e+16f;
         float out_face_dist_sum = 0.0f;

                              //check all 3 edges of the face
         dword vi0 = 0;
         for(int k=3; k--; ){
            dword vi1 = vi0;
            vi0 = k;

                              //compute distance to line
            const S_vector2 &p = *v[vi0];
            S_vector2 dir = (*v[vi1]) - p;
            float d = rc_src_point.DistanceToLine(p, dir);
            bool out_edge = (d < 0.001f);
                              //since bluring and filtering may be applied, 
                              // include also lumels beyong edges
            if(out_edge){
               in_face = false;
               if(d > -EDGE_OUT_DISTANCE){
                  d = -d;
                  if(out_face_dist1 > d){
                     out_face_dist1 = d;
                     out_edge_i1 = k;
                  }
                  out_face_dist_sum += d;
               }else{
                              //too far, can't be in this face
                  out_edge_i1 = -1;
                  break;
               }
            }
         }
         if(in_face){
                              //found face which we're in, stop looking for more
            in_face_i = i;
            break;
         }else
         if(out_edge_i1!=-1 && out_face_dist>out_face_dist_sum){
                              //choose triangle which is closer to the point,
                              // use sum of 'out-of-edge' values to determine this
            out_face_dist = out_face_dist_sum;
            out_edge_i = out_edge_i1;
            out_face_i = i;
         }
      }
      if(_out_face_i){
         *_out_face_i = -1;
         *_out_edge_i = -1;
         if(in_face_i==-1){
            *_out_face_i = out_face_i;
            *_out_edge_i = out_edge_i;
         }
      }
                              //check if such face exists in mesh
      return in_face_i != -1 ? in_face_i : out_face_i;
   }

//----------------------------

   bool GetLumelOrigin(const S_lighten_data &data, const S_vector2 &rc_src_point, S_lumel &lumel,
      S_vector &pt_pos, S_vector &pt_normal, S_vector &pt_col_src, CPI3D_triface faces) const{

      pt_pos.Zero();
      pt_normal.Zero();

      int out_face_i, out_edge_i = -1;
      int face_i = GetLumelClosestFace(data, rc_src_point, &out_face_i, &out_edge_i);
      if(face_i == -1){
                              //set 'outside' lumels to special color (red)
         lumel.SetInvalid(0, 0, 2);
         return false;
      }
                              //found in one of faces, or near
      I3D_triface fc = (*data.rc_faces)[face_i].fc;
      
      const S_vector2 *v[3] = {
         &data.rc_verts_trans[fc[0]],
         &data.rc_verts_trans[fc[1]],
         &data.rc_verts_trans[fc[2]] 
      };

      S_vector2 edge_dir = *v[2] - *v[1];
      S_vector2 p_dir = rc_src_point - *v[0];
                              //get intersection of those 2 lines
      float u1, u2;

      bool b = LineIntersection(S_vector(v[0]->x, v[0]->y, 0),
         S_vector(p_dir.x, p_dir.y, 0),
         S_vector(v[1]->x, v[1]->y, 0), 
         S_vector(edge_dir.x, edge_dir.y, 0),
         u1, u2);

      if(b && I3DFabs(u1)<MRG_ZERO){
         swap(u1, u2);
         swap(edge_dir, p_dir);
      }
      if(!b || I3DFabs(u1)<MRG_ZERO){
                              //shuffle a bit and try again
         swap(v[0], v[1]); swap(v[0], v[2]);
         swap(fc[0], fc[1]); swap(fc[0], fc[2]);

         edge_dir = *v[2] - *v[1];
         p_dir = rc_src_point - *v[0];

         bool b = LineIntersection(S_vector(v[0]->x, v[0]->y, 0),
            S_vector(p_dir.x, p_dir.y, 0),
            S_vector(v[1]->x, v[1]->y, 0), 
            S_vector(edge_dir.x, edge_dir.y, 0),
            u1, u2);
         if(b && I3DFabs(u1)<MRG_ZERO){
            swap(u1, u2);
            swap(edge_dir, p_dir);
         }
         if(!b || I3DFabs(u1)<MRG_ZERO){
                              //shuffle a bit and try again
            swap(v[0], v[1]); swap(v[0], v[2]);
            swap(fc[0], fc[1]); swap(fc[0], fc[2]);

            edge_dir = *v[2] - *v[1];
            p_dir = rc_src_point - *v[0];

            b = LineIntersection(S_vector(v[0]->x, v[0]->y, 0),
               S_vector(p_dir.x, p_dir.y, 0),
               S_vector(v[1]->x, v[1]->y, 0), 
               S_vector(edge_dir.x, edge_dir.y, 0),
               u1, u2);
            if(b && I3DFabs(u1)<MRG_ZERO){
               swap(u1, u2);
               swap(edge_dir, p_dir);
            }
            if(!b || I3DFabs(u1)<MRG_ZERO){
                              //invalid face
                              //set lumel to special color (green)
               lumel.SetInvalid(0, 2, 0);
                              //report problem to user
               {
                  if(data.cb_proc){
                     (*data.cb_proc)(CBM_ERROR, (dword)"invalid face", 0, data.cb_context);
                  }
                  const S_matrix &m = GetMatrix();
                  const I3D_triface &fc = faces[(*data.rc_faces)[face_i].face_index];

                  const S_vector &v0 = *(S_vector*)(((byte*)data.verts) + data.vstride*fc[0]);
                  const S_vector &v1 = *(S_vector*)(((byte*)data.verts) + data.vstride*fc[1]);
                  const S_vector &v2 = *(S_vector*)(((byte*)data.verts) + data.vstride*fc[2]);

                  drv->DebugLine(v0*m, v1*m, 0, 0xffff0000);
                  drv->DebugLine(v1*m, v2*m, 0, 0xffff0000);
                  drv->DebugLine(v2*m, v0*m, 0, 0xffff0000);
               }
               return false;
            }
         }
      }

      const S_vector *v0 = (S_vector*)(((byte*)data.verts) + data.vstride*fc[0]);
      const S_vector *v1 = (S_vector*)(((byte*)data.verts) + data.vstride*fc[1]);
      const S_vector *v2 = (S_vector*)(((byte*)data.verts) + data.vstride*fc[2]);
      {           
                              //get lumel's point, in world coords,
                              // and its normal
         S_vector edge_xyz = *v1 + (*v2 - *v1) * u2;
         pt_pos = *v0 + (edge_xyz - *v0) / u1;
         pt_pos *= matrix;

         S_vector edge_normal = v1[1] * (1.0f-u2) + v2[1] * u2;
         float u1a = 1.0f / u1;
         pt_normal = v0[1] * (1.0f-u1a) + edge_normal * u1a;
         pt_normal = pt_normal.RotateByMatrix(matrix);
         pt_normal.Normalize();
      }

                              //get collision point, usually the same as lumel's point,
                              // but it's different for out-of-face lumels
      if(out_face_i!=-1){
                              //collisions in this case must be handled
                              // from the closest edge point, to avoid
                              // 'out of area' collision testing
      
         const I3D_triface &fc = (*data.rc_faces)[out_face_i].fc;
         word vi0 = fc[out_edge_i], vi1 = fc[next_tri_indx[out_edge_i]];
                              //get edge's dir
         S_vector2 edge_dir(data.rc_verts_trans[vi1] - data.rc_verts_trans[vi0]);

                              //get direct point on edge
         float u = edge_dir.Dot(edge_dir);
         if(u>MRG_ZERO){
            u = edge_dir.Dot(rc_src_point-data.rc_verts_trans[vi0]) / u;

                              //put slightly towards triangle
                              // - helps to collision testing to avoid missing something
                              // this computation is in 3D
                              //clamp to line
            const S_vector *v_i0 = (S_vector*)(((byte*)data.verts) + data.vstride*vi0);
            const S_vector *v_i1 = (S_vector*)(((byte*)data.verts) + data.vstride*vi1);

            u = Max(INSIDE_PUSH_RATIO*2.0f, Min(1.0f-INSIDE_PUSH_RATIO*2.0f, u));
            pt_col_src = *v_i0 + (*v_i1 - *v_i0) * u;
            pt_col_src = pt_col_src * matrix;
            {
               const S_vector *v0 = (S_vector*)(((byte*)data.verts) + data.vstride*fc[0]);
               const S_vector *v1 = (S_vector*)(((byte*)data.verts) + data.vstride*fc[1]);
               const S_vector *v2 = (S_vector*)(((byte*)data.verts) + data.vstride*fc[2]);

               S_vector face_normal;
               face_normal.GetNormal(*v0, *v1, *v2);

               S_vector edge_normal;
               edge_normal.GetNormal(*v_i0, *v_i1, *v_i0-face_normal);
               edge_normal = edge_normal.RotateByMatrix(matrix);
               float mag = edge_normal.Magnitude();
               if(mag>MRG_ZERO)
                  pt_col_src += edge_normal * INSIDE_PUSH_RATIO / mag;
               //drv->DebugLine(pt_xyz, pt_xyz+pt_normal, 0);
            }
         }else{
                              //failcase, but also rather ok (anyway it shouldn't happen)
            pt_col_src = pt_pos;
         }
      }else{
         pt_col_src = pt_pos;
      }

                           //debug - breakpoint on specified lumel
#if defined _DEBUG && 0
#define DEBUG_LX 4
#define DEBUG_LY 0
      if(lmrc_i==0 && x==DEBUG_LX && y==DEBUG_LY){
         lumel[2] = 2.0f;
         drv->DebugLine(pt_xyz, pt_xyz+pt_normal, 0, S_vector(1, 0, 0));
      }
#endif

                           //light computation goes from collision point,
                           // not recommended - just test
      //pt_xyz = col_pt_xyz;

#ifdef DEBUG_DRAW_LUMEL_NORMAL
      drv->DebugLine(pt_pos, pt_pos+pt_normal, 0, 0xff0000ff);
#endif
#if defined _DEBUG && 0
      if(in_face_i==4){
         lumel.SetInvalid(0, 0, 2);
         is_valid_lumel = false;
         break;
      }
#endif
      return true;
   }

//----------------------------

   bool LightenLumel(const S_lighten_data &data, const S_vector2 &rc_src_point, S_lumel &lumel,
      CPI3D_triface faces, bool test_collisions) const{

      S_vector pt_src, pt_normal, pt_src_collision;
      if(!GetLumelOrigin(data, rc_src_point, lumel, pt_src, pt_normal, pt_src_collision, faces))
         return false;

      ++lumel.num_samples;
      for(int i=data.rc_lights.size(); i--; ){
         PI3D_light lp = data.rc_lights[i].lp;
         const S_matrix &m_light = lp->GetMatrixDirect();
         const S_vector &light_pos = m_light(3);
         S_vector ldir = light_pos - pt_src;
         dword col_test_flags = 0;

         float A = lp->power;

         switch(lp->I3D_light::GetLightType()){
         case I3DLIGHT_DIRECTIONAL:
            {
               float dir_cos = -(lp->normalized_dir.Dot(pt_normal));
               if(CHECK_ZERO_LESS(dir_cos)) continue;
               A *= dir_cos;
               ldir = -lp->normalized_dir * ldir.Magnitude();
               col_test_flags |= I3DCOL_RAY;
            }
            break;

         case I3DLIGHT_SPOT:
            {
                              //compute angle, fast reject out-going rays
               float Rd = ldir.Dot(pt_normal);
               if(Rd<0.0f)
                  continue;
                              //get distance
               float ldist_2 = ldir.Dot(ldir);
               if(ldist_2>=lp->range_f_scaled_2)
                  continue;
               float ldist = I3DSqrt(ldist_2);
                              //compute attenuation
               if(ldist>lp->range_n_scaled) A *= (lp->range_f_scaled - ldist) / lp->range_delta_scaled;
                              //cone
               float r_dist = 1.0f/ldist;
               {
                  float a = -lp->normalized_dir.Dot(ldir) * r_dist;
                  if(a<=lp->outer_cos)
                     continue;
                  if(a<lp->inner_cos)
                     A *= (a - lp->outer_cos) / (lp->inner_cos - lp->outer_cos);
               }
               A *= Rd * r_dist;

               ldir = light_pos - pt_src_collision;
            }
            break;

         case I3DLIGHT_POINT:
            {
                     //compute angle, fast reject out-going rays
               float Rd = ldir.Dot(pt_normal);
               if(Rd<0.0f)
                  continue;
                     //get distance
               float ldist_2 = ldir.Dot(ldir);
               if(ldist_2>=lp->range_f_scaled_2)
                  continue;
               float ldist = I3DSqrt(ldist_2);
                     //compute attenuation
               if(ldist>lp->range_n_scaled) A *= (lp->range_f_scaled - ldist) / lp->range_delta_scaled;
               A *= Rd / ldist;

               ldir = light_pos - pt_src_collision;
               //drv->DebugLine(light_pos, pt_src_collision, 0);
            }
            break;

         case I3DLIGHT_POINTAMBIENT:
            {
                     //compute angle, fast reject out-going rays
               //float Rd = ldir.Dot(pt_normal);
               //if(Rd<0.0f) continue;
                     //get distance
               float ldist_2 = ldir.Dot(ldir);
               if(ldist_2>=lp->range_f_scaled_2) continue;
               float ldist = I3DSqrt(ldist_2);
                     //compute attenuation
               if(ldist>lp->range_n_scaled) A *= (lp->range_f_scaled - ldist) / lp->range_delta_scaled;
               //A *= Rd / ldist;

               ldir = light_pos - pt_src_collision;
            }
            break;
         }
                     //optimization: don't further process rays with minimal intensity
         if(IsAbsMrgZero(A))
            continue;
                     //check collisions
                     // note: PointAmbient lights do not check collisions
         S_vector color = lp->color;
         swap(color.x, color.z);
#ifndef DEBUG_NO_COLLISIONS
         if(test_collisions &&
            lp->I3D_light::GetLightType() != I3DLIGHT_POINTAMBIENT){

            float len = ldir.Magnitude();
            float dist = 0.0f;
            bool b;
                  //put a bit in front of face, 
                  // don't allow colliding with itself
            S_vector x_pos1 = pt_src_collision + data.rc_plane.normal*INSIDE_PUSH_RATIO;
#ifdef DEBUG_DRAW_RAYS
            drv->DebugVector(x_pos1, ldir, 0);
#endif
            //if(flags&I3D_LIGHT_GEO_COLLISIONS)
            {
               S_makelight_collision_help hlp;

               hlp.opacity = S_vector(1.0f, 1.0f, 1.0f);
               hlp.self_add.Zero();
               hlp.collision_or = false;
               hlp.collided = false;
               hlp.zero_opacity = false;
               hlp.drv = drv;

               b = false;
               for(;;){
                  hlp.collided = false;
                  hlp.texture_alpha = false;
                  I3D_collision_data cd;
                  cd.from = x_pos1;
                  cd.dir = ldir;
                  cd.flags = I3DCOL_EXACT_GEOMETRY |
                     col_test_flags |
                     I3DCOL_COLORKEY | I3DCOL_FORCE2SIDE;
                  cd.callback = S_makelight_collision_help::cbcol;
                  cd.texel_callback = S_makelight_collision_help::cbTexel_thunk;
                  cd.cresp_context = &hlp;
                  b = data.scene->TestCollision(cd);

                  if(!hlp.collided){
                     if(b)
                        dist = cd.GetHitDistance();
                     break;
                  }
                  if(hlp.zero_opacity)
                     break;
                  dist = hlp.collision_dist + .01f;
                  if(len<dist)
                     break;
                  S_vector flied = ldir * (dist/len);
                  x_pos1 += flied;
                  ldir -= flied;
                  if(IsAbsMrgZero(ldir.Magnitude()))
                     break;
               }
               if(hlp.collision_or){
                  color *= hlp.opacity;
                  lumel.color += hlp.self_add;
               }
            }
            /*else{
               I3D_collision_data cd(x_pos1, ldir, 0);
               b = scene->TestCollision(cd);
               if(b)
                  dist = cd.GetHitDistance();
            }
            */ 
#ifdef DEBUG_DRAW_RAYS
            drv->DebugVector(x_pos1, (!b ? ldir : (ldir*dist/ldir.Magnitude())), 0, 0xff808080);
#endif

            if(b){
               //drv->DebugLine(x_pos, x_pos1+ldir*dist/ldir.Magnitude(), 0);
#ifdef FADE_SHADOWS
               //float f = dist/(len*.6666f);
                     //collision detected, attenuate light amount by collision distance
               float f = dist/(len*.8f);
               f = Min(1.0f, f);
               A *= f;
#else
               continue;
#endif
            }
         }
#endif//!DEBUG_NO_COLLISIONS
         lumel.color += color * A;
      }
      return true;
   }

//----------------------------

   bool DisplayProgress(float progress, float &last_progress,
      dword &last_progress_time, bool allow_cancel, PI3D_LOAD_CB_PROC cb_proc, void *cb_context) const{

      assert(progress >= last_progress);
      if((progress-last_progress) >= MIN_PROGRESS){
         last_progress = progress;

         PIGraph igraph = drv->GetGraphInterface();
         int curr_time = igraph->ReadTimer();
         if((curr_time-last_progress_time) >= MIN_PROGRESS_DELAY){
            last_progress_time = curr_time;
                              //call-back
            if(cb_proc)
               (*cb_proc)(CBM_PROGRESS, I3DFloatAsInt(progress), 1, cb_context);
            if(allow_cancel){
                              //check cancellation
               IG_KEY k = igraph->ReadKey(true);
               if(k==K_ESC)
                  return true;
            }
         }
      }
      return false;
   }

//----------------------------

   static void ClearAlpha(dword sx, dword sy, S_lumel *buf){
      for(dword i=sx*sy; i--; )
         buf[i].alpha = 0.0f;
   }

//----------------------------

   void MixImageIntoAlpha(const S_lighten_data &data, dword sx, dword sy, S_lumel *buf, const C_map_channel *mc2){

      bool map_ok = (drv->last_LM_blend_img_name==mc2->bitmap_name);
      if(map_ok){
         C_str map_name;
         dword bsx, bsy;
         byte bpp;
         if(!drv->FindBitmap(mc2->bitmap_name, map_name, bsx, bsy, bpp))
            map_ok = false;
         else{
            PC_dta_stream dta = DtaCreateStream(map_name);
            if(!dta)
               map_ok = false;
            else{
                              //check dates
               __int64 tm;
               dta->GetTime((dword*)&tm);
               if(tm!=drv->last_blend_map_time)
                  map_ok = false;
               dta->Release();
            }
         }
      }
      if(!map_ok){
         C_str map_name;

         dword bsx, bsy;
         byte bpp;
         if(!drv->FindBitmap(mc2->bitmap_name, map_name, bsx, bsy, bpp)){
            if(data.cb_proc)
               (*data.cb_proc)(CBM_ERROR, (dword)(const char*)C_fstr("cannot open blending map '%s'", (const char*)mc2->bitmap_name), 0, data.cb_context);
            ClearAlpha(sx, sy, buf);
                              //error message should go here
            return;
         }
      
                              //create and init new blend image
         if(!drv->last_LM_blend_img){
            drv->last_LM_blend_img = drv->GetGraphInterface()->CreateImage();
            drv->last_LM_blend_img->Release();
         }
         S_pixelformat pf;
         memset(&pf, 0, sizeof(pf));
         pf.bytes_per_pixel = 3;
         pf.r_mask = 0xff0000;
         pf.g_mask = 0x00ff00;
         pf.b_mask = 0x0000ff;
         pf.flags = 0;
         if(!drv->last_LM_blend_img->Open(map_name, IMGOPEN_SYSMEM, 0, 0, &pf)){
            if(data.cb_proc)
               (*data.cb_proc)(CBM_ERROR, (dword)(const char*)C_fstr("cannot open blending map '%s'", (const char*)map_name), 0, data.cb_context);
            ClearAlpha(sx, sy, buf);
            return;
         }
         PC_dta_stream dta = DtaCreateStream(map_name);
         assert(dta);
         if(dta){
            dta->GetTime((dword*)&drv->last_blend_map_time);
            dta->Release();
            drv->last_LM_blend_img_name = mc2->bitmap_name;
         }
      }
      CPIImage img = drv->last_LM_blend_img;

      S_vector2 rc_src_point;
      for(dword y=0; y<sy; y++ ){
         rc_src_point.y = (float)y;
         for(dword x=0; x<sx; x++){
            rc_src_point.x = (float)x;

            S_lumel &lum = *(buf + y*sx + x);
            lum.alpha = 0;

            int face_i = GetLumelClosestFace(data, rc_src_point, NULL, NULL);
                              //just ignore outside lumels
            if(face_i == -1)
               continue;
                                    //found in one of faces, or near
            const I3D_triface &fc = (*data.rc_faces)[face_i].fc;

            const S_vector2 *v[3] = {
               &data.rc_verts_trans[fc[0]],
               &data.rc_verts_trans[fc[1]],
               &data.rc_verts_trans[fc[2]] 
            };

            S_vector2 edge_dir = *v[2] - *v[1];
            S_vector2 p_dir = rc_src_point - *v[0];
                                    //get intersection of those 2 lines
            float u1, u2;

            bool b = LineIntersection(S_vector(v[0]->x, v[0]->y, 0),
               S_vector(p_dir.x, p_dir.y, 0),
               S_vector(v[1]->x, v[1]->y, 0), 
               S_vector(edge_dir.x, edge_dir.y, 0),
               u1, u2);
            if(!b){
               //assert(0);
               continue;
            }
                              //get uv-mapping face
            dword uv_face_i = (*data.rc_faces)[face_i].face_index;
            const I3D_triface &uv_fc = mc2->uv_faces[uv_face_i];
            const S_vector2 *uv[3] = {
               &mc2->uv_verts[uv_fc[0]],
               &mc2->uv_verts[uv_fc[1]],
               &mc2->uv_verts[uv_fc[2]] 
            };

                              //get uv coord at the location
            S_vector2 edge_xyz = *uv[1] + (*uv[2] - *uv[1]) * u2;
            S_vector2 pt_pos = *uv[0] + (edge_xyz - *uv[0]) / u1;

            S_vectorw color;
            if(!GetTexturePixel(drv, img, pt_pos, color, true))
               continue;

#ifdef DEBUG_MIX_BLEND_IMAGE_COLOR
            lum.color = color;
#endif
            lum.alpha = color.GetBrightness();
         }
      }
   }

//----------------------------

   I3D_RESULT MakeLight(PI3D_scene scene, dword flags, PI3D_LOAD_CB_PROC cb_proc, void *cb_context, CPI3D_triface faces){

#define DISPLAY_PROGRESS(progress) \
   if(DisplayProgress(progress_base+progress, last_progress, last_progress_time, flags&I3D_LIGHT_ALLOW_CANCEL, cb_proc, cb_context))\
      ir = I3DERR_CANCELED;

      I3D_RESULT ir = I3D_OK;

#ifdef DEBUG_NO_COLLISIONS
      flags &= ~I3D_LIGHT_GEO_COLLISIONS;
#endif

      dword num_comp_count = 0;

      if(cb_proc){
         static const float f = 0.0f;
         (*cb_proc)(CBM_PROGRESS, I3DFloatAsInt(f), 0, cb_context);
      }

      const C_vector<PI3D_light> *sct_lights = NULL;
                                 //collect lights which apply to this sector
      PI3D_frame frm_sect = this;
      while(frm_sect && frm_sect->GetType1()!=FRAME_SECTOR) frm_sect = frm_sect->GetParent();
      if(frm_sect){
         PI3D_sector sct = I3DCAST_SECTOR(frm_sect);
         sct_lights = &sct->GetLights1();
      }
                              
                                 //get sum of lumels (due to progress indicator)
      int num_lumels = 0;
      for(dword i=lm_rects.size(); i--; ){
         const S_lmap_rectangle *lm_rect = lm_rects[i];
         num_lumels += lm_rect->size_x * lm_rect->size_y;
      }

                                 //progress indicator support
      dword last_progress_time = drv->GetGraphInterface()->ReadTimer();
      float last_progress = 0.0f;
      float progress_base = 0.0f;

      S_lighten_data ldata;
      ldata.scene = scene;
      ldata.cb_proc = cb_proc;
      ldata.cb_context = cb_context;

      C_vector<PI3D_visual> vis_list;
      /*
      struct S_hlp{
         static I3DENUMRET I3DAPI cbCollect(PI3D_frame frm, dword c){
            if(!frm->IsOn1())
               return I3DENUMRET_SKIPCHILDREN;
            PI3D_visual vis = I3DCAST_VISUAL(frm);
            if(vis->GetMesh() && !(vis->GetFrameFlags()&I3D_FRMF_NOSHADOW)){
               C_vector<PI3D_visual> &vis_list = *(C_vector<PI3D_visual>*)c;
               vis_list.push_back(vis);
            }
            return I3DENUMRET_OK;
         }
      };
      scene->EnumFrames(S_hlp::cbCollect, (dword)&vis_list, ENUMF_VISUAL);
      */
      bool any_lights = false;
      bool any_alpha = false;

                              //process all lm_rects
      for(dword lmrc_i = 0; lmrc_i != lm_rects.size(); lmrc_i++){
         S_lmap_rectangle *lm_rect = lm_rects[lmrc_i];

         S_matrix mat_lm = lm_rect->inv_m;
         mat_lm = mat_lm * matrix;

                              //get lm rect plane
         ldata.rc_plane.normal = -mat_lm(2);
         ldata.rc_plane.normal.Normalize();
         ldata.rc_plane.d = -(mat_lm(3).Dot(ldata.rc_plane.normal));

                              //collect all lights which may illuminate the surface
         S_vector ambient(0, 0, 0);
         ldata.rc_lights.clear();

         if(sct_lights)
            CollectLights(*sct_lights, vis_list, *lm_rect, ldata.rc_plane, ldata.rc_lights, ambient);
         any_lights = (any_lights || ldata.rc_lights.size());

         dword size_x = lm_rect->size_x;
         dword size_y = lm_rect->size_y;
         assert(size_x>1 && size_y>1);
         {
            CPI3D_material mat = mesh->GetFGroups1()[0].GetMaterial1();
            const S_vector &mat_diffuse = mat->GetDiffuse1();
                              //1. collect all triangles of this LM rectangle
                              //2. transform all verts into LM rect's space (2D)
            ldata.verts = (S_vector*)mesh->vertex_buffer.Lock(D3DLOCK_READONLY);
            ldata.vstride = mesh->vertex_buffer.GetSizeOfVertex();
         
            C_vector<S_lm_face> rc_faces;
            C_vector<S_vector2> rc_verts_trans;

            CollectRectangleFacesAndVertices(mesh, ldata.verts, ldata.vstride, lmrc_i, *lm_rect, rc_faces, rc_verts_trans);
            
                              //alloc temp mem
            S_lumel *lumel_map = new S_lumel[size_x * size_y];
            memset(lumel_map, 0, size_x*size_y*sizeof(S_lumel));

            ldata.rc_faces = &rc_faces;
            ldata.rc_verts_trans = &*rc_verts_trans.begin();

                                 //position on 2D LM rectangle, for which we compute lightness
                                 // it may have also non-zero fractional part (sub-pixels)
            S_vector2 rc_src_point;
#if 1                         //switch old/new version
            do{
               float this_rect_progress = float(size_x * size_y) / (float)num_lumels;

               const int AA_MODE = 3;//drv->lm_create_aa_ratio;
               //const int AA_MODE = 1;
               float aa_pixel_offset = (.5f/(float)AA_MODE - .5f);
               float aa_pixel_shift = (1.0f/(float)AA_MODE);

               const float MAX_BRIGHTNESS_DELTA = .075f;
               dword x, y;

                              //pass 1 - compute centers of each other lumel
               for(y=0; y<size_y && I3D_SUCCESS(ir); y++ ){
                  rc_src_point.y = (float)y;
                  for(x=0; x<size_x; x++){
                     if(x!=size_x-1 && y!=size_y-1 && ((x&1) || (y&1)))
                        continue;

                     rc_src_point.x = (float)x;

                     S_lumel &lumel = *(lumel_map + y*size_x + x);
                     LightenLumel(ldata, rc_src_point, lumel, faces, (flags&I3D_LIGHT_GEO_COLLISIONS));
                     ++num_comp_count;
                     //lumel.debug_mark = true;
                  }
                  DISPLAY_PROGRESS(((y+1)*.3f / (float)size_y) * this_rect_progress);
               }
               if(I3D_FAIL(ir))
                  break;

#if 1
                              //pass 2 - refine neighbours of black lumels
               for(y=0; y<size_y-1 && I3D_SUCCESS(ir); y++ ){
                  for(x=0; x<size_x-1; x++){
                     if(!((y^x)&1)) continue;
                              //delta indices of two neighbours
                     int n_x[2], n_y[2];
                     if(!(y&1)){
                        n_x[0] = -1; n_y[0] = 0;
                        n_x[1] =  1; n_y[1] = 0;
                     }else{
                        n_x[0] = 0; n_y[0] = -1;
                        n_x[1] = 0; n_y[1] =  1;
                     }
                     //S_lumel &lumel = *(lumel_map + y*size_x + x);
                     //lumel.debug_mark = true;
                     S_lumel *neighbours[2] = {
                        (lumel_map + (y+n_y[0])*size_x + x + n_x[0]),
                        (lumel_map + (y+n_y[1])*size_x + x + n_x[1])
                     };
                     bool delta_ok = false;
                     if(neighbours[0]->IsValid() && neighbours[1]->IsValid()){
                        neighbours[0]->SolveBrightness();
                        neighbours[1]->SolveBrightness();
                        float br0 = neighbours[0]->color.GetBrightness();
                        float br1 = neighbours[1]->color.GetBrightness();
                        float delta = I3DFabs(br0 - br1);
                        delta_ok = (delta < MAX_BRIGHTNESS_DELTA);
                     }
                     if(!delta_ok){
                              //refine neighbours
                        for(dword ni=2; ni--; ){
                           S_lumel &neighbour = *neighbours[ni];
                           if(neighbour.IsValid() && !neighbour.antialiased){
                              rc_src_point.y = (float)(y+n_y[ni]) + aa_pixel_offset;
                              bool ok = true;
                              for(dword aa_y=AA_MODE; ok && aa_y--; rc_src_point.y += aa_pixel_shift){
                                 rc_src_point.x = (float)(x+n_x[ni]) + aa_pixel_offset;
                                 for(dword aa_x=AA_MODE; aa_x--; rc_src_point.x += aa_pixel_shift){
                                    if(aa_y!=(AA_MODE/2) || aa_x!=(AA_MODE/2)){  //not center point again
                                       ++num_comp_count;
                                       ok = LightenLumel(ldata, rc_src_point, neighbour, faces, (flags&I3D_LIGHT_GEO_COLLISIONS));
                                       if(!ok)
                                          break;
                                    }
                                 }
                              }
                              if(neighbour.IsValid()){
                                 neighbour.SolveBrightness();
                                 neighbour.antialiased = true;
                                 //neighbour.debug_mark = true;
                              }
                           }
                        }
                     }
                  }
                  DISPLAY_PROGRESS((.3f + (y+1)*.4f / (float)size_y) * this_rect_progress);
               }
#endif//phase 2

#if 1
                              //pass 3 - process lumels in between (1/2)
               for(y=0; y<size_y-1 && I3D_SUCCESS(ir); y++ ){

                  for(x=0; x<size_x-1; x++){
                     if(!((y^x)&1)) continue;

                     S_lumel &lumel = *(lumel_map + y*size_x + x);

                              //delta indices of two neighbours
                     int n_x[2], n_y[2];

                     if(!(y&1)){
                        n_x[0] = -1; n_y[0] = 0;
                        n_x[1] =  1; n_y[1] = 0;
                     }else{
                        n_x[0] = 0; n_y[0] = -1;
                        n_x[1] = 0; n_y[1] =  1;
                     }
                     S_lumel *neighbours[2] = {
                        (lumel_map + (y+n_y[0])*size_x + x + n_x[0]),
                        (lumel_map + (y+n_y[1])*size_x + x + n_x[1])
                     };

                     bool computed_ok = false;
                     if(neighbours[0]->IsValid() && neighbours[1]->IsValid()){
                        neighbours[0]->SolveBrightness();
                        neighbours[1]->SolveBrightness();
                        float br0 = neighbours[0]->color.GetBrightness();
                        float br1 = neighbours[1]->color.GetBrightness();
                        float delta = I3DFabs(br0 - br1);
                        if(delta < MAX_BRIGHTNESS_DELTA){
                           lumel.color = (neighbours[0]->color + neighbours[1]->color) * .5f;
                           lumel.num_samples = 1;
                           computed_ok = true;
                           //lumel.debug_mark = true;
                        }
                     }
                     if(!computed_ok){
                              //refine this lumel
                        rc_src_point.y = (float)y + aa_pixel_offset;
                        bool ok = true;
                        for(dword aa_y=AA_MODE; ok && aa_y--; rc_src_point.y += aa_pixel_shift){
                           rc_src_point.x = (float)x + aa_pixel_offset;
                           for(dword aa_x=AA_MODE; aa_x--; rc_src_point.x += aa_pixel_shift){
                              ++num_comp_count;
                              ok = LightenLumel(ldata, rc_src_point, lumel, faces, (flags&I3D_LIGHT_GEO_COLLISIONS));
                              if(!ok)
                                 break;
                           }
                        }
                        lumel.antialiased = true;
                        //lumel.debug_mark = true;
                     }
                  }
                  DISPLAY_PROGRESS((.7f + (y+1)*.1f / (float)size_y) * this_rect_progress);
               }
               if(I3D_FAIL(ir))
                  break;
#endif//phase 3

#if 1
                              //pass 4 - rest of lumels (1/4)
               for(y=0; y<size_y-1 && I3D_SUCCESS(ir); y++ ){
                  if(!(y&1)) continue;
                  for(x=0; x<size_x-1; x++){
                     if(!(x&1)) continue;
                     S_lumel &lumel = *(lumel_map + y*size_x + x);

                              //debug break
                     //if(x==157 && y==175) lumel.color = S_vector(0, 0, 2);

                     S_lumel *neighbours[4] = {
                        (lumel_map + y*size_x + x - 1),
                        (lumel_map + y*size_x + x + 1),
                        (lumel_map + (y-1)*size_x + x),
                        (lumel_map + (y+1)*size_x + x)
                     };
                     bool computed_ok = false;
                     if(neighbours[0]->IsValid() && neighbours[1]->IsValid() &&
                        neighbours[2]->IsValid() && neighbours[3]->IsValid()){

                        neighbours[0]->SolveBrightness();
                        neighbours[1]->SolveBrightness();
                        neighbours[2]->SolveBrightness();
                        neighbours[3]->SolveBrightness();
                        float br0 = neighbours[0]->color.GetBrightness();
                        float br1 = neighbours[1]->color.GetBrightness();
                        float br2 = neighbours[2]->color.GetBrightness();
                        float br3 = neighbours[3]->color.GetBrightness();

                        float delta = I3DFabs(br0 - br1);
                        delta = Max(delta, I3DFabs(br2 - br3));
                        delta = Max(delta, I3DFabs(br0 - br2));
                        delta = Max(delta, I3DFabs(br0 - br3));
                        delta = Max(delta, I3DFabs(br1 - br2));
                        delta = Max(delta, I3DFabs(br1 - br3));

                        if(delta < MAX_BRIGHTNESS_DELTA){
                           lumel.color = (neighbours[0]->color + neighbours[1]->color + neighbours[2]->color + neighbours[3]->color) * .25f;
                           lumel.num_samples = 1;
                           computed_ok = true;
                           //lumel.debug_mark = true;
                        }
                     }
                     if(!computed_ok){
                              //refine this lumel
                        rc_src_point.y = (float)y + aa_pixel_offset;
                        bool ok = true;
                        for(dword aa_y=AA_MODE; ok && aa_y--; rc_src_point.y += aa_pixel_shift){
                           rc_src_point.x = (float)x + aa_pixel_offset;
                           for(dword aa_x=AA_MODE; aa_x--; rc_src_point.x += aa_pixel_shift){
                              ++num_comp_count;
                              ok = LightenLumel(ldata, rc_src_point, lumel, faces, (flags&I3D_LIGHT_GEO_COLLISIONS));
                              if(!ok)
                                 break;
                           }
                        }
                        lumel.antialiased = true;
                        //lumel.debug_mark = true;
                     }
                  }
                  DISPLAY_PROGRESS((.8f + (y+1)*.2f / (float)size_y) * this_rect_progress);
               }
               if(I3D_FAIL(ir))
                  break;
#endif//phase 4

#ifdef _DEBUG
               dword num_aa = 0;
#endif
                             //finalize all lumels
               S_lumel *lumel_p = lumel_map;
               for(y=0; y<size_y && I3D_SUCCESS(ir); y++ ){
                  for(x=0; x<size_x; x++){
                     S_lumel &lumel = *lumel_p++;
                     if(lumel.IsValid()){
                        lumel.SolveBrightness();
                        lumel.color += ambient;
                        lumel.color *= mat_diffuse;
#ifdef _DEBUG
                        if(lumel.debug_mark){
                           lumel.SetInvalid(0, 1, 0);
                        }
                        if(lumel.antialiased)
                           ++num_aa;
#endif
                     }
                  }
               }
#if defined _DEBUG && 0
               drv->PRINT(C_fstr("LM: total: %i, aa: %i, cols: %i", size_x*size_y, num_aa, num_comp_count));
#endif
               progress_base += this_rect_progress;
            }while(false);
#else
            S_lumel *lumel_ptr = lumel_map;
            float aa_mode = (float)drv->lm_create_aa_ratio;
            float aa_pixel_offset = (.5f/aa_mode - .5f);
            float aa_pixel_shift = (1.0f/aa_mode);
            //float r_num_aa_levels = (1.0f/(aa_mode*aa_mode));
                              //process all lumels
            for(dword y=0; y<size_y && I3D_SUCCESS(ir); y++){

               for(dword x=0; x<size_x; x++){
                  S_lumel &lumel = *lumel_ptr++;

                  bool is_valid_lumel = true;

                  rc_src_point.y = y + aa_pixel_offset;
                  for(int aa_i_y=0; aa_i_y<drv->lm_create_aa_ratio && is_valid_lumel; aa_i_y++,
                     rc_src_point.y += aa_pixel_shift){

                     rc_src_point.x = x + aa_pixel_offset;
                     for(int aa_i_x=0; aa_i_x<drv->lm_create_aa_ratio; aa_i_x++,
                        rc_src_point.x += aa_pixel_shift){

                        is_valid_lumel = LightenLumel(ldata, rc_src_point, lumel);
                        if(!is_valid_lumel)
                           break;
                     }
                  }
                  if(is_valid_lumel){
                     lumel.SolveBrightness();
                     //lumel *= r_num_aa_levels;
                     lumel.color += ambient;
                     lumel.color *= mat_diffuse;
                  }
               }              //row loop

               DISPLAY_PROGRESS((y+1)*size_x / (float)num_lumels);
            }                    //line loop
            progress_base += float(size_x * size_y) / (float)num_lumels;
#endif

            mesh->vertex_buffer.Unlock(); ldata.verts = NULL;

            if(I3D_SUCCESS(ir)){
#ifndef DEBUG_NO_BLUR
               if(flags&I3D_LIGHT_BLUR){
                  dword sx = lm_rect->size_x, sy = lm_rect->size_y;
                  S_lumel *v_map1 = new S_lumel[sx*sy];
                  BlurRectangle(sx, sy, lumel_map, v_map1);
                  delete[] lumel_map;
                  lumel_map = v_map1;
               }
#endif
               const C_map_channel *mc2 = mesh->GetMapChannel2();
               if(mc2){
                  MixImageIntoAlpha(ldata, lm_rect->size_x, lm_rect->size_y, lumel_map, mc2);
                  any_alpha = true;
               }else
                  ClearAlpha(lm_rect->size_x, lm_rect->size_y, lumel_map);
               //BegProf(); for(i=300; i--; )
               StoreRGBRectangle(lm_rect->size_x, lm_rect->size_y, lumel_map, lm_rect->dst_map);
               //drv->PRINT(EndProf());
            }
            delete[] lumel_map;
         }
         if(I3D_FAIL(ir))
            break;
         lm_rect->dirty = true;
      }
      any_rect_dirty = true;

      if(I3D_FAIL(ir))
         Destruct();
      else
      if(!any_lights){
         (*cb_proc)(CBM_ERROR, (dword)"no lights hitting area!", 1, cb_context);
      }
      lmap_flags &= ~LMAPF_HAS_ALPHA;
      if(any_alpha)
         lmap_flags |= LMAPF_HAS_ALPHA;

      if(cb_proc){
         static const float f = 1.0f;
         (*cb_proc)(CBM_PROGRESS, I3DFloatAsInt(f), 2, cb_context);
      }
      return ir;
   }

//----------------------------
// Blur light-map's color channel. Leave alpha channel unchanged.
   static void BlurRectangle(dword sx, dword sy, const S_lumel *src, S_lumel *dst){
                        
                              //make blur by 3x3 matrix:
                              // 1, 2, 1,
                              // 2, 4, 2,
                              // 1, 2, 1
      static const float blur_mat[3][3] = {
         1, 2, 1,
         2, 4, 2,
         1, 2, 1
      };
      const float R_BLUR_SUM = 1.0f / 16.0f;

      for(int y=0; y<(int)sy; y++){
         for(int x=0; x<(int)sx; x++, ++dst){
            dst->color.Zero();
            for(int yy = -1; yy<2; yy++){
               for(int xx = -1; xx<2; xx++){
                  const S_lumel *l = src +
                     Max(0, Min((int)sy-1, y+yy)) * sx +
                     Max(0, Min((int)sx-1, x+xx));
                  float f = blur_mat[yy+1][xx+1];
                  dst->color.x += l->color.x * f;
                  dst->color.y += l->color.y * f;
                  dst->color.z += l->color.z * f;
               }
            }
            dst->color *= R_BLUR_SUM;
         }
      }
   }

//----------------------------
// convert to RGB surface
   static void StoreRGBRectangle(dword sx, dword sy, const S_lumel *src, S_lmap_pixel *dstp){

                        //final step - 
#if defined _MSC_VER && defined USE_ASM && 1

      dword sz = sizeof(S_lumel);
      static const float C_255 = 255.0f;
      __asm{
         push ecx
                              //ebx = size S_lumel
                        //ecx = counter
                        //esi = src
                        //edi = dst
                        //ST(0) = unit_light
         mov ebx, sz
         mov esi, src
         mov edi, dstp
         fld unit_light
         mov ecx, sx
         imul ecx, sy
         push eax
      lx:
                           //Red
         fld dword ptr[esi+0]
         fmul st, st(1)
         fistp dword ptr[esp]
         mov eax, [esp]
         test eax, 0xffffff00
         jz ok1
         test eax, eax
         jns ok2
         xor eax, eax
         jmp ok1
      ok2:
         mov al, 0xff
      ok1:
         mov [edi+0],al
                           //Green
         fld dword ptr[esi+4]
         fmul st, st(1)
         fistp dword ptr[esp]
         mov eax, [esp]
         test eax, 0xffffff00
         jz ok3
         test eax, eax
         jns ok4
         xor eax, eax
         jmp ok3
      ok4:
         mov al, 0xff
      ok3:
         mov [edi+1],al
                           //Red
         fld dword ptr[esi+8]
         fmul st, st(1)
         fistp dword ptr[esp]
         mov eax, [esp]
         test eax, 0xffffff00
         jz ok5
         test eax, eax
         jns ok6
         xor eax, eax
         jmp ok5
      ok6:
         mov al, 0xff
      ok5:
         mov [edi+2],al
                           //Alpha
         fld dword ptr[esi+12]
         fmul C_255
         fistp dword ptr[esp]
         mov eax, [esp]
         test eax, 0xffffff00
         jz ok7
         test eax, eax
         jns ok8
         xor eax, eax
         jmp ok7
      ok8:
         mov al, 0xff
      ok7:
         mov [edi+3],al

         add esi, ebx
         add edi, 4  //size S_lmap_pixel

         dec ecx
         jnz lx
                           //clear FPU
         fstp dword ptr[esp]
         pop eax

         pop ecx
      }
#else //_MSC_VER
      for(dword i=sx*sy; i--; ++dstp, ++src){
         dstp->b = (byte)Max(0, Min(255, FloatToInt(src->color[0] * unit_light)));
         dstp->g = (byte)Max(0, Min(255, FloatToInt(src->color[1] * unit_light)));
         dstp->r = (byte)Max(0, Min(255, FloatToInt(src->color[2] * unit_light)));
         dstp->a = (byte)Max(0, Min(255, FloatToInt(src->alpha * 255.0f)));
      }
#endif   //!_MSC_VER
   }

//----------------------------

public:
   I3D_lit_object(PI3D_driver d):
      I3D_visual(d),
      lmap_flags(0),
      DrawPrim(NULL),
      curr_resolution(1.0f),
      any_rect_dirty(true),
      lm_alloc_block(NULL)
   {
      visual_type = I3D_VISUAL_LIT_OBJECT;
   }
   ~I3D_lit_object(){
      if(lm_alloc_block)
         drv->LMFree(lm_alloc_block);
   }

   I3DMETHOD(Duplicate)(CPI3D_frame);

   I3DMETHOD(SetProperty)(dword index, dword value){
      switch(index){
      case I3DPROP_LMAP_F_RESOLUTION:
         {
            float f = I3DIntAsFloat(value);
            if(f < MIN_RESOLUTION)
               return I3DERR_INVALIDPARAMS;
            curr_resolution = f;
         }
         break;
      default: return I3DERR_INVALIDPARAMS;
      }
      return I3D_OK;
   }

   I3DMETHOD_(dword,GetProperty)(dword index) const{
      switch(index){
      case I3DPROP_LMAP_F_RESOLUTION: return I3DFloatAsInt(curr_resolution);
      }
      return 0xffcdcdcd;
   }

   I3DMETHOD_(PI3D_mesh_base,GetMesh)(){ return mesh; }
   I3DMETHOD_(CPI3D_mesh_base,GetMesh)() const{ return mesh; }

//----------------------------

   virtual bool ComputeBounds(){
      if(!mesh){
         bound.bound_local.bbox.Invalidate();
         bound.bound_local.bsphere.pos.Zero();
         bound.bound_local.bsphere.radius = 0.0f;
         return true;
      }
      mesh->ComputeBoundingVolume();
      bound.bound_local = mesh->GetBoundingVolume();

      vis_flags |= VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;

      return true;
   }

   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
   void SetMesh(PI3D_mesh_base mb){

      if(!mb){
         mesh = NULL;
         return;
      }
                              //determine fvf of mesh
      dword lmap_mesh_fvf = D3DFVF_XYZ | D3DFVF_NORMAL;

      dword num_uvs = 1;
#ifndef GL
      if(!drv->IsDirectTransform()){
                              //keep lmap secondary uvs in source mesh
         ++num_uvs;
      }
#else
      ++num_uvs;
#endif
      lmap_mesh_fvf |= num_uvs << D3DFVF_TEXCOUNT_SHIFT;//D3DFVF_TEXCOUNT_MASK;

      if(mb->vertex_buffer.GetFVFlags() == lmap_mesh_fvf){
         mesh = mb;
      }else{
         mesh = drv->CreateMesh(I3DVC_XYZ | I3DVC_NORMAL | (num_uvs<<I3DVC_TEXCOUNT_SHIFT));
         mesh->Clone(mb, false);
         mesh->Release();
      }
   }

   virtual void PrepareDestVB(I3D_mesh_base*, dword = 2);
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(Construct)(PI3D_scene, dword flags = 0, PI3D_LOAD_CB_PROC = NULL, void *context = NULL);
   I3DMETHOD(Destruct)(){

      if(lm_alloc_block){
         drv->LMFree(lm_alloc_block);
         lm_alloc_block = NULL;
      }
      lm_rects.clear();
      face_lmap_use.clear();
      any_rect_dirty = true;

      lmap_flags &= ~LMAPF_CONSTRUCTED;
#ifndef GL
      vertex_buffer.DestroyD3DVB();
#endif
      return I3D_OK;
   }

   I3DMETHOD(Load)(class C_cache*);
   I3DMETHOD(Save)(class C_cache*) const;
   I3DMETHOD_(bool,IsConstructed)() const{
      return (lmap_flags&LMAPF_CONSTRUCTED);
   }

//----------------------------
// No alpha adjustment.
   I3DMETHOD_(void,SetVisualAlpha)(float f){ }

//----------------------------
// Get lightmap pixel of given face, in specified position.
   I3DMETHOD_(bool,GetLMPixel)(word face_index, const S_vector &point_on_face, S_vector &color) const{

      if(!(lmap_flags&LMAPF_CONSTRUCTED))
         return false;
      assert(mesh);
                              //determine fgroup of the face
      const C_buffer<I3D_face_group> &fgroups = mesh->GetFGroupVector();
      for(dword i=fgroups.size(); i--; ){
         const I3D_face_group &fg = fgroups[i];
         if(fg.base_index <= face_index){
            dword index_in_group = face_index - fg.base_index;
            assert(index_in_group < fg.num_faces);
            dword lmi = face_lmap_use[i][index_in_group];
            assert(lmi < lm_rects.size());
            const S_lmap_rectangle *lm_rect = lm_rects[lmi];

            CPI3D_triface faces = mesh->GetIndexBuffer().LockForReadOnly();

            const I3D_triface &fc = faces[face_index];

            const void *verts = ((PI3D_mesh_base)(CPI3D_mesh_base)mesh)->vertex_buffer.Lock(D3DLOCK_READONLY);
            dword vstride = mesh->GetSizeOfVertex();
            I3D_texel_collision tc;
            tc.loc_pos = true;
            tc.v[0] = (S_vector*)(((byte*)verts) + fc[0]*vstride);
            tc.v[1] = (S_vector*)(((byte*)verts) + fc[1]*vstride);
            tc.v[2] = (S_vector*)(((byte*)verts) + fc[2]*vstride);
            //tc.v[0] = &verts[fc[0]].xyz;
            //tc.v[1] = &verts[fc[1]].xyz;
            //tc.v[2] = &verts[fc[2]].xyz;
            //drv->DEBUG(fc[0]); drv->DEBUG(fc[1]); drv->DEBUG(fc[2]);
            //drv->DEBUG(face_index);

            S_vector2 uv[3];
            for(i=3; i--; ){
               const S_vector &vs = *tc.v[i];
               const S_matrix &m = lm_rect->m;
                              //transform into 2D uniform rectangle space
               uv[i].x = vs.x * m.m[0][0] + vs.y * m.m[1][0] + vs.z * m.m[2][0] + m.m[3][0];
               uv[i].y = vs.x * m.m[0][1] + vs.y * m.m[1][1] + vs.z * m.m[2][1] + m.m[3][1];
            }
            bool ok = false;
            tc.point_on_plane = point_on_face;
            tc.face_index = face_index;
            if(tc.ComputeUVFromPoint(uv[0], uv[1], uv[2])){
               //drv->DEBUG(tc.tex.x); drv->DEBUG(tc.tex.y);
               assert(tc.tex.x >= 0.0f && tc.tex.x <= 1.0f && tc.tex.y >= 0.0f && tc.tex.y <= 1.0f);
               float fx = tc.tex.x * (float)(lm_rect->size_x-1);
               float fy = tc.tex.y * (float)(lm_rect->size_y-1);
               dword x = FloatToInt(fx);
               dword y = FloatToInt(fy);
               assert(x < lm_rect->size_x);
               assert(y < lm_rect->size_y);

               S_lmap_pixel &rgb = lm_rect->dst_map[y*lm_rect->size_x + x];
               color.x = (float)rgb.r * R_128;
               color.y = (float)rgb.g * R_128;
               color.z = (float)rgb.b * R_128;

                              //apply filtering
               {
                  float frc_x = (fx - (float)x);
                  float frc_y = (fy - (float)y);
                  int delta_x = 1;
                  int delta_y = 1;
                  if(frc_x<0.0f){
                     delta_x = -1;
                     frc_x = -frc_x;
                  }
                  if(frc_y<0.0f){
                     delta_y = -1;
                     frc_y = -frc_y;
                  }
                  S_lmap_pixel &rgb_01 = lm_rect->dst_map[y*lm_rect->size_x + x + delta_x];
                  S_vector c_01(float(rgb_01.r)*R_128, float(rgb_01.g)*R_128, float(rgb_01.b)*R_128);

                  S_lmap_pixel &rgb_10 = lm_rect->dst_map[(y+delta_y)*lm_rect->size_x + x];
                  S_vector c_10(float(rgb_10.r)*R_128, float(rgb_10.g)*R_128, float(rgb_10.b)*R_128);

                  S_lmap_pixel &rgb_11 = lm_rect->dst_map[(y+delta_y)*lm_rect->size_x + x + delta_x];
                  S_vector c_11(float(rgb_11.r)*R_128, float(rgb_11.g)*R_128, float(rgb_11.b)*R_128);

                  S_vector cf_y0 = color*(1.0f-frc_x) + c_01*frc_x;
                  S_vector cf_y1 = c_10*(1.0f-frc_x) + c_11*frc_x;
                  color = cf_y0*(1.0f-frc_y) + cf_y1*frc_y;

                  //drv->DEBUG(delta_y);
                  //drv->DEBUG(frc_y);
               }

               ok = true;
            }
            ((PI3D_mesh_base)(CPI3D_mesh_base)mesh)->vertex_buffer.Unlock();
            mesh->GetIndexBuffer().Unlock();
            return ok;
         }
      }
      return false;
   }
};

//----------------------------

I3D_RESULT I3D_lit_object::Duplicate(CPI3D_frame frm){

   if(frm==this)
      return I3D_OK;
   if(frm->GetType1()!=FRAME_VISUAL)
      return I3D_frame::Duplicate(frm);

   Destruct();

   CPI3D_visual vis = I3DCAST_CVISUAL(frm);

   switch(vis->GetVisualType1()){
   case I3D_VISUAL_LIT_OBJECT:
      {
         PI3D_lit_object lmap = (PI3D_lit_object)vis;
         lm_rects = lmap->lm_rects;
         for(int i=lm_rects.size(); i--; ){
            lm_rects[i]->dirty = true;
         }
         face_lmap_use = lmap->face_lmap_use;
         any_rect_dirty = true;
         lmap_flags &= ~LMAPF_CONSTRUCTED;
         if(lmap->lmap_flags&LMAPF_CONSTRUCTED)
            lmap_flags |= LMAPF_CONSTRUCTED;
      }
      break;
   }
   SetMesh(const_cast<PI3D_visual>(vis)->GetMesh());

   return I3D_visual::Duplicate(frm);
}

//----------------------------

void I3D_lit_object::PrepareDestVB(I3D_mesh_base *mb, dword){

                              //search secondary bitmap
   for(int j=mb->NumFGroups1(); j--; ){
      PI3D_material mat = mb->GetFGroups1()[j].mat;
      if(mat->GetTexture1(MTI_SECONDARY))
         break;
   }
   bool has_second_map = (j!=-1) && (lmap_flags&LMAPF_HAS_ALPHA);
   I3D_visual::PrepareDestVB(mb, 2);
   if(drv->CanUsePixelShader()){

      if(!(lmap_flags&LMAPF_CONSTRUCTED))
         DrawPrim = &I3D_lit_object::DPUnconstructed_PS;
      else
      if(!(lmap_flags&LMAPF_DRAW_LIGHT_MAP))
         DrawPrim = &I3D_lit_object::DP_B_PS;
      else{
         DrawPrim = &I3D_lit_object::DP_BLd_PS;
         if(has_second_map)
            DrawPrim = &I3D_lit_object::DP_BLSd_PS;
      }
   }
#ifndef GL
   else{

      if(!(lmap_flags&LMAPF_CONSTRUCTED))
         DrawPrim = &I3D_lit_object::DPUnconstructed;
      else
      if(!(lmap_flags&LMAPF_DRAW_LIGHT_MAP))
         DrawPrim = &I3D_lit_object::DP_B;
      else
      if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
         DrawPrim = &I3D_lit_object::DP_L;
      else{
         if(drv->IsDirectTransform()){
            if(drv->GetFlags()&DRVF_SINGLE_PASS_MODULATE2X){
               DrawPrim = &I3D_lit_object::DPDirect_BLd;
               if(has_second_map){
                  if(drv->MaxSimultaneousTextures()>2){
                     DrawPrim = &I3D_lit_object::DPDirect_BLSd;
                  }else{
                     assert(0);
                  }
               }else{
                  if((vis_flags&VISF_USE_DETMAP) && drv->MaxSimultaneousTextures()<=2)
                     DrawPrim = &I3D_lit_object::DPDirect_BL_D;
               }
            }else{
               DrawPrim = &I3D_lit_object::DPDirect_B_L;
               if((vis_flags&VISF_USE_DETMAP) && drv->MaxSimultaneousTextures()<=1)
                  DrawPrim = &I3D_lit_object::DPDirect_B_L_D;
            }
         }else{
                                 //no envmapping in non-direct mode
            vis_flags &= ~VISF_USE_ENVMAP;
            if(drv->GetFlags()&DRVF_SINGLE_PASS_MODULATE2X){
               DrawPrim = &I3D_lit_object::DPIndirect_BLd;
               if(has_second_map){
                  if(drv->MaxSimultaneousTextures()>=2){
                     DrawPrim = &I3D_lit_object::DPIndirect_BLSd;
                  }else{
                     assert(0);
                  }
               }else{
                  if((vis_flags&VISF_USE_DETMAP) && drv->MaxSimultaneousTextures()<=2)
                     DrawPrim = &I3D_lit_object::DPIndirect_BL_D;
               }
            }else{
               DrawPrim = &I3D_lit_object::DPIndirect_B_L;
               if((vis_flags&VISF_USE_DETMAP) && drv->MaxSimultaneousTextures()<=1)
                  DrawPrim = &I3D_lit_object::DPIndirect_B_L_D;
            }
         }
      }
   }
#endif

   vis_flags &= ~VISF_DEST_UV0_VALID;

   {
      if(lm_alloc_block){
         drv->LMFree(lm_alloc_block);
         lm_alloc_block = NULL;
         for(int i=lm_rects.size(); i--; )
            lm_rects[i]->dirty = true;
         any_rect_dirty = true;
      }
   }
   vis_flags |= VISF_DEST_PREPARED;
}

//----------------------------

struct S_face{
   int group_index;           //face-group index
   int face_index;            //face index within the group
   I3D_triface face;
   bool free;
   S_vector normal;
   byte smooth_group;
};

I3D_RESULT I3D_lit_object::Construct(PI3D_scene scene, dword flags, PI3D_LOAD_CB_PROC cb_proc, void *cb_context){

   if(!mesh)
      return I3DERR_GENERIC;

   const byte *smooth_groups = mesh->GetSmoothGroups();
   if(!smooth_groups){
      if(cb_proc)
         (*cb_proc)(CBM_ERROR, (dword)"mesh has no smooth groups", 0, cb_context);
      return I3DERR_INVALIDPARAMS;
   }
   for(dword i=mesh->NumFaces1(); i--; ){
      if(!smooth_groups[i]){
         if(cb_proc)
            (*cb_proc)(CBM_ERROR, (dword)"cannot compute because not all faces have smoothgroup set", 0, cb_context);
         return I3DERR_INVALIDPARAMS;
      }
   }

   assert(curr_resolution >= MIN_RESOLUTION*.5f);

   Destruct();

   assert(!(lmap_flags&LMAPF_CONSTRUCTED));

   any_rect_dirty = true;

   int j;

   const void *verts = mesh->vertex_buffer.Lock(D3DLOCK_READONLY);
   dword vstride = mesh->GetSizeOfVertex();
   int num_verts = mesh->vertex_buffer.NumVertices();

                              //keeping remapped faces
   C_vector<S_face> face_list;

   face_lmap_use.resize(mesh->NumFGroups1());
   for(i = 0; i<mesh->NumFGroups1(); i++){
      //face_lmap_use.push_back(C_vector<word>());
      //face_lmap_use.back().assign(mesh->GetFGroups1()[i].num_faces, 0xffff);
      face_lmap_use[i].resize(mesh->GetFGroups1()[i].num_faces, 0xffff);
   }

   C_buffer<I3D_triface> faces(mesh->NumFaces1());
   mesh->GetFaces(faces.begin());

   {
                              //perform vertex mapping - this causes proper
                              // computation of meshes, for which
                              // verts have been duplicated because of
                              // texture mapping
      C_vector<word> v_map; v_map.resize(num_verts);
      {
         const C_vector<word> &v_map1 = mesh->vertex_buffer.GetVertexSpreadMap();
         int num_orig_verts = num_verts - v_map1.size();
         for(int i=0; i<num_orig_verts; i++)
            v_map[i] = (word)i;
         for(; i<num_verts; i++)
            v_map[i] = v_map1[i - num_orig_verts];
      }

      //PI3D_triface faces = mesh->index_buffer.Lock(D3DLOCK_READONLY);
      CPI3D_triface face_ptr = faces.begin();
                              //collect all faces
                              // + build face_lmap_use (uninitialized)
      for(i = 0; i<mesh->NumFGroups1(); i++){
         const I3D_face_group *fg = &mesh->GetFGroups1()[i];
         for(dword j=0; j<fg->num_faces; j++){
            face_list.push_back(S_face());
            S_face &f = face_list.back();
            f.group_index = i;
            f.face_index = j;
            f.face = *face_ptr++;
            f.face[0] = v_map[f.face[0]];
            f.face[1] = v_map[f.face[1]];
            f.face[2] = v_map[f.face[2]];
            f.free = true;

            const S_vector &v0 = *(S_vector*)(((byte*)verts) + vstride*f.face[0]);
            const S_vector &v1 = *(S_vector*)(((byte*)verts) + vstride*f.face[1]);
            const S_vector &v2 = *(S_vector*)(((byte*)verts) + vstride*f.face[2]);

            //f.normal.GetNormal(verts[f.face[0]].xyz, verts[f.face[1]].xyz, verts[f.face[2]].xyz);
            f.normal.GetNormal(v0, v1, v2);
            if(f.normal.IsNull()){
                              //discard this face (invalid)
               face_lmap_use[i][j] = 0xffff;
               f.free = false;
               continue;
            }
            assert(!f.normal.IsNull());
            f.normal.Normalize();
            f.smooth_group = smooth_groups ? (*smooth_groups++) : byte(0xff);
         }
      }
      //mesh->index_buffer.Unlock();
   }

                              //process all free faces
   for(i=face_list.size(); i--; ){
      if(face_list[i].free){
         S_face &face = face_list[i];
         face.free = false;

                              //faces (indicies) of this lmap
         C_vector<dword> face_indx;
         dword lmap_index = lm_rects.size();
         face_indx.push_back(i);
         face_lmap_use[face.group_index][face.face_index] = (word)lmap_index;

                              //resulting normal of all faces in thresh
         S_vector rc_normal;
         rc_normal = face.normal;

                              //*** collect coplanar adjacent faces ***
         //if(0)
         {
                              //edge list - contains re-mapped vert indicies
            C_vector<I3D_edge> egde_list;
                              //smooth group for each edge
            C_vector<byte> smooth_group_list;
            {
               const I3D_triface &fc = face.face;
               egde_list.push_back(I3D_edge(fc[0], fc[1]));
               egde_list.push_back(I3D_edge(fc[1], fc[2]));
               egde_list.push_back(I3D_edge(fc[2], fc[0]));
            
               smooth_group_list.push_back(face.smooth_group);
               smooth_group_list.push_back(face.smooth_group);
               smooth_group_list.push_back(face.smooth_group);
            }

            while(egde_list.size()){
               I3D_edge e = egde_list.back(); egde_list.pop_back();
               byte sg = smooth_group_list.back(); smooth_group_list.pop_back();

               for(j=i; j--; )
               if(face_list[j].free){
                  S_face &face1 = face_list[j];
                  byte sg1 = face1.smooth_group;
                              //check smoothgroups
                  if(!(sg&sg1))
                     continue;
                  const I3D_triface &fc1 = face1.face;

                              //check if these faces share same edge
                  dword w0 = fc1[0];
                  for(int k=3; k--; ){
                     dword w1 = w0;
                     w0 = fc1[k];
                     if(w0==e[1] && w1==e[0])
                        break;
                  }
                  if(k!=-1){
                              //check angle against all added faces
                     for(int l=face_indx.size(); l--; ){
                        int fci = face_indx[l];
                        float a = face1.normal.AngleTo(face_list[fci].normal);
                        if(a > LM_CONSTR_PLANAR_THRESH)
                           break;
                     }
                     if(l==-1)
                     {
                              //add this face
                        face_indx.push_back(j);
                        face1.free = false;
                        face_lmap_use[face1.group_index][face1.face_index] = (word)lmap_index;
                              //add its edges (except the shared one)

                        egde_list.push_back(I3D_edge(fc1[(k+1)%3], fc1[(k+2)%3]));
                        egde_list.push_back(I3D_edge(fc1[(k+2)%3], fc1[k]));

                        smooth_group_list.push_back(sg1);
                        smooth_group_list.push_back(sg1);

                        rc_normal += face1.normal;
                              //don't process other faces, 
                              // assuming edges are always only among 2 faces
                        break;
                     }
                  }
               }
            }
            assert(!smooth_group_list.size());
         }

         rc_normal.Normalize();

                              //*** make transformation matrix ***
                              //collect all points of faces of this lmap
         C_vector<dword> pt_list;
         for(j=face_indx.size(); j--; ){
            const I3D_triface &fc = face_list[face_indx[j]].face;
            for(int k=0; k<3; k++){
               dword vi = fc[k];
                              //don't add same vertex twice
               for(int l=pt_list.size(); l--; ) 
                  if(pt_list[l]==vi)
                     break;
               if(l==-1) 
                  pt_list.push_back(vi);
            }
         }

         const S_vector &v0 = *(S_vector*)(((byte*)verts) + vstride*face.face[0]);
         //const S_vector &v1 = *(S_vector*)(((byte*)verts) + vstride*f.face[1]);
         //const S_vector &v2 = *(S_vector*)(((byte*)verts) + vstride*f.face[2]);

                              //find smallest possible rectangle containing all points 
                              // (in it 2D transformation)
         S_plane lm_plane;
         lm_plane.normal = rc_normal;
         //lm_plane.d = -verts[face.face[0]].xyz.Dot(lm_plane.normal);
         lm_plane.d = -v0.Dot(lm_plane.normal);

         float best_size = 1e+16f;
         I3D_bbox best_bbox;
         best_bbox.min.Zero();
         best_bbox.max.Zero();
         S_matrix best_mat;
         best_mat.Identity();

                              //proposed transformation matrix
         S_matrix prop_matrix;
         prop_matrix.Identity();
         prop_matrix(2) = -lm_plane.normal;

         S_vector *v_rect = new S_vector[pt_list.size()];
         S_vector *v_trans = new S_vector[pt_list.size()];
         for(j=pt_list.size(); j--; ){
            //v_rect[j] = verts[pt_list[j]].xyz;
            v_rect[j] = *(S_vector*)(((byte*)verts) + vstride*pt_list[j]);
         }

         for(j = pt_list.size(); j--; ){
                              //get 1st vertex and let it lay on the plane
            const S_vector &v0a = v_rect[j];
            S_vector v0 = v0a - lm_plane.normal * v0a.DistanceToPlane(lm_plane);

            for(int k=j; k--; ){
                              //get 2nd vertex and let it lay on the plane
               const S_vector &v1a = v_rect[k];
               S_vector v1 = v1a - lm_plane.normal * v1a.DistanceToPlane(lm_plane);

               S_vector dir_v0_v1 = v1 - v0;
                              //if verts are too close (identical), skip this vertex
               if(IsMrgZeroLess(dir_v0_v1.Dot(dir_v0_v1)))
                  continue;
                              //compute proposed matrix
               prop_matrix(0) = S_normal(dir_v0_v1);
               prop_matrix(1) = prop_matrix(0).Cross(prop_matrix(2));

               S_matrix prop_matrix_inv = ~prop_matrix;

                              //process all points, build bounds (projected to 2D)
               I3D_bbox rc_bbox;
               rc_bbox.Invalidate();

               TransformVertexArray(v_rect, sizeof(S_vector), pt_list.size(),
                  v_trans, sizeof(S_vector), prop_matrix_inv);

               for(int l=pt_list.size(); l--; ){
                  const S_vector &vt = v_trans[l];
                  rc_bbox.min.Minimal(vt);
                  rc_bbox.max.Maximal(vt);
               }
                              //save result, if better than previous
               float curr_size = (rc_bbox.max.x-rc_bbox.min.x) * (rc_bbox.max.y-rc_bbox.min.y);
               if(best_size > curr_size){
                  best_size = curr_size;
                  best_bbox = rc_bbox;
                  best_mat = prop_matrix;
               }
            }
         }
         delete[] v_trans;
         delete[] v_rect;

                              //*** now we have computed smallest rectangle ***
         //lm_rects.push_back(new S_lmap_rectangle());
         lm_rects.resize(lm_rects.size() + 1);
         //S_lmap_rectangle *lm_rect = lm_rects.back();
         S_lmap_rectangle *lm_rect = new S_lmap_rectangle;
         lm_rects[lm_rects.size()-1] = lm_rect;
         lm_rect->Release();
                              //initialize LM rectangle
         lm_rect->m = best_mat;

                              //expand bbox by some value
                              // - helps in phong shading, also it's safer for edges
#if 1
         {
            float f = (1.0f/curr_resolution) * EXPAND_LUMEL_SIZE;
            f /= GetWorldScale();
            best_bbox.min.x -= f;
            best_bbox.min.y -= f;
            best_bbox.max.x += f;
            best_bbox.max.y += f;
         }
#endif
         lm_rect->m(3) = best_bbox.min.RotateByMatrix(lm_rect->m);
         S_vector bbox_diag = best_bbox.max - best_bbox.min;
                              //scale
         {
            for(int i=0; i<3; i++){
               lm_rect->m(0, i) *= bbox_diag.x;
               lm_rect->m(1, i) *= bbox_diag.y;
            }
         }
         lm_rect->inv_m = lm_rect->m;
         lm_rect->m.Invert();
         lm_rect->z_delta = best_bbox.max.z - best_bbox.min.z;
#if 0                         //debug draw lm rextangle
         {
            for(int i=pt_list.size(); i--; ){
               const S_vector &v1 = verts[pt_list[i]].xyz;
               S_vector vt = v1 * lm_rect->m;
               drv->PRINT(vt);
               drv->DebugLine(v1, v1 + lm_plane.normal, 0);

            }
         }
#endif

         S_vector scale = matrix.GetScale();
                              //init lm surface
         float res = curr_resolution;
         {
                              //compute destination resolution
            dword size_x = FloatToInt(bbox_diag.x * res * scale.x + 1);
            dword size_y = FloatToInt(bbox_diag.y * res * scale.z + 1);

                              //let size be always at least 2, 
                              // so that we have phong shading,
                              // and avoid problems with too small lm surfaces
                              // (width or height of 1 causes divide by zero, 
                              //    and other problems during light computations)
            size_x = Max(size_x, 2ul);
            size_y = Max(size_y, 2ul);

                              //limit to max possible lmap size
            size_x = Min(size_x, (dword)LM_MAX_COMPUTE_SIZE);
            size_y = Min(size_y, (dword)LM_MAX_COMPUTE_SIZE);

            lm_rect->size_x = size_x;
            lm_rect->size_y = size_y;
            lm_rect->dst_map = new S_lmap_pixel[size_x*size_y];
#ifdef LMAP_RANDOM_INIT       //init by random contents
            {
               memset(dlod.dst_map, 0, sizeof(S_lmap_pixel)*dlod.size_x*dlod.size_y);
               int ii = 0;
               for(j=dlod.size_y-1; j--; ){
                  for(int i=dlod.size_x-1; i--; ++ii){
                     dlod.dst_map[ii].r = Random(256);
                     dlod.dst_map[ii].g = Random(256);
                     dlod.dst_map[ii].b = Random(256);
                  }
                  ++ii;
               }
            }
#endif
         }
         lm_rect->dirty = true;
      }
   }
   mesh->vertex_buffer.Unlock();

   lmap_flags |= LMAPF_CONSTRUCTED;
   CheckUV();

   I3D_RESULT ir = MakeLight(scene, flags, cb_proc, cb_context, faces.begin());
   if(I3D_FAIL(ir))
      Destruct();

   return ir;
}

//----------------------------

#define LM_SAVE_VERSION 0x12    //format version

#define LM_SAVE_HAS_ALPHA 1

#pragma pack(push,1)
struct S_lobj_data_header{
   byte version;              //current format version, equals to LM_SAVE_VERSION
   byte mode;
   int num_lods;              //number of levels of detail, 0 for unconstructed saved data
   dword flags;               //LM_SAVE_??? flags
};

struct S_lmap_header{         //used with light-mapping
   word num_LM_rectangles, num_face_groups;
};

#pragma pack(pop)

//----------------------------

I3D_RESULT I3D_lit_object::Load(C_cache *cp){

   if(IsConstructed())
      Destruct();

   if(!mesh)
      return I3DERR_NOTINITIALIZED;

                              //read header
   S_lobj_data_header hdr;
   cp->read((byte*)&hdr, sizeof(hdr));
   if(hdr.version != LM_SAVE_VERSION)
      return I3DERR_INVALIDPARAMS;
   if(!hdr.num_lods){
                              //not constructed
      return I3D_OK;
   }

                              //older versions supported multiple LODs
                              // now it's only 1
   if(hdr.num_lods != 1)
      return I3DERR_INVALIDPARAMS;

   lmap_flags &= ~LMAPF_HAS_ALPHA;
   if(hdr.flags&LM_SAVE_HAS_ALPHA)
      lmap_flags |= LMAPF_HAS_ALPHA;

   int i;
   any_rect_dirty = true;

                              //load light-map information
   {
      S_lmap_header hdr1;
      cp->read((byte*)&hdr1, sizeof(hdr1));

      PI3D_mesh_base mb = mesh;
                              //check if data are valid for current mesh
      if(hdr1.num_face_groups != mb->NumFGroups1())
         return I3DERR_INVALIDPARAMS;
                              //read LM rectangles
      lm_rects.resize(hdr1.num_LM_rectangles);
      for(i=0; i<hdr1.num_LM_rectangles; i++){
         S_lmap_rectangle *lm_rect = new S_lmap_rectangle;
         lm_rects[i] = lm_rect;
         lm_rect->Release();

         cp->read((byte*)&lm_rect->m, sizeof(S_matrix));
         lm_rect->inv_m = ~lm_rect->m;
                              //read num of LODs
         dword num_lods = cp->ReadDword();
         if(num_lods!=1)
            goto fail;
         cp->read(&lm_rect->size_x, sizeof(dword)*2);
                              //read compressed data 
         dword num_elements;
         if(lmap_flags&LMAPF_HAS_ALPHA)
            Decompress(cp, &lm_rect->dst_map, &num_elements);
         else{
                              //convert from 3-byte to 4-byte pixel
            S_rgb *buf;
            num_elements = 0;
            Decompress(cp, &buf, &num_elements);
            lm_rect->dst_map = new S_lmap_pixel[num_elements];
            while(num_elements--){
               S_lmap_pixel &d = lm_rect->dst_map[num_elements];
               d = buf[num_elements];
               d.a = 0xff;
            }
            delete[] buf;
         }
      }
                              //read face use
      face_lmap_use.resize(hdr1.num_face_groups);
      for(i=0; i<hdr1.num_face_groups; i++){
         dword nf = cp->ReadDword();

                              //check mesh consistency
         const I3D_face_group &fg = mb->GetFGroups1()[i];
         if(nf != fg.num_faces)
            goto fail;
                              //read data
         face_lmap_use[i].resize(nf);
         cp->read((byte*)&face_lmap_use[i][0], sizeof(word)*nf);

#if defined _DEBUG || 1
                              //make sanity check
         for(dword ii=nf; ii--; ){
            if(face_lmap_use[i][ii] >= lm_rects.size())
               goto fail;
         }
#endif
      }
      lmap_flags |= LMAPF_CONSTRUCTED;
   }

   CheckUV();
   lmap_flags |= LMAPF_CONSTRUCTED;
#ifndef GL
   vertex_buffer.DestroyD3DVB();
#endif
   vis_flags &= ~VISF_DEST_PREPARED;
   return I3D_OK;

fail:
   Destruct();

   return I3DERR_INVALIDPARAMS;
}

//----------------------------

I3D_RESULT I3D_lit_object::Save(C_cache *cp) const{

   S_lobj_data_header hdr;
   hdr.version = LM_SAVE_VERSION;
   hdr.flags = 0;
   hdr.num_lods = IsConstructed() ? 1 : 0;

   if(lmap_flags&LMAPF_HAS_ALPHA)
      hdr.flags |= LM_SAVE_HAS_ALPHA;

                              //save header
   cp->write(&hdr, sizeof(hdr));

                              //unconstructed saves only header
   if(!IsConstructed())
      return I3D_OK;

                              //save light-map information
   S_lmap_header hdr1;
   hdr1.num_LM_rectangles = word(lm_rects.size());
   hdr1.num_face_groups = word(face_lmap_use.size());
   cp->write(&hdr1, sizeof(hdr1));

                              //save LM rectangles info
   for(dword i=0; i<lm_rects.size(); i++){
      const S_lmap_rectangle *lm_rect = lm_rects[i];
                              //save:
                              // - matrix
                              // - dword num_lods (always 1)
                              // - size
                              // - bitmap
      cp->write(&lm_rect->m, sizeof(S_matrix));

      dword num_lods = 1;
      cp->write(&num_lods, sizeof(dword));
      cp->write(&lm_rect->size_x, sizeof(dword)*2);
                              //write compressed data 
      C_vector<byte> v;
      dword num_lumels = lm_rect->size_x*lm_rect->size_y;
      if(lmap_flags&LMAPF_HAS_ALPHA)
         Compress(lm_rect->dst_map, num_lumels, v);
      else{
                              //convert from 4-byte to 3-byte pixel
         C_buffer<S_rgb> buf(num_lumels);
         for(dword i=num_lumels; i--; )
            buf[i] = lm_rect->dst_map[i].ToRGB();
         Compress(buf.begin(), num_lumels, v);
      }
      cp->write(&*v.begin(), v.size());
   }     
                              //save face use
   for(i=0; i<face_lmap_use.size(); i++){
                              //save:
                              // - num_faces
                              // - face use indicies
      dword nf = face_lmap_use[i].size();
      cp->write(&nf, sizeof(dword));
      cp->write(&face_lmap_use[i][0], sizeof(word)*nf);
   }
   return I3D_OK;
}

//----------------------------

void I3D_lit_object::CheckUV(){

   if(!IsConstructed())
      return;

   int num_verts = mesh->vertex_buffer.NumVertices();
   C_vector<word> vert_lm_use; vert_lm_use.resize(num_verts, 0xffff);
   C_vector<word> vertex_dup_info; vertex_dup_info.reserve(num_verts);

   //PI3D_triface faces = const_cast<I3D_source_index_buffer&>(mesh->GetIndexBuffer()).Lock(0);
   //*
   C_buffer<I3D_triface> face_buf(mesh->NumFaces1());
   PI3D_triface faces = face_buf.begin();
   mesh->GetFaces(faces);
   /**/

   for(dword i=0; i<face_lmap_use.size(); i++){
      for(dword j=0; j<face_lmap_use[i].size(); j++){
         I3D_triface &face = *faces++;
         dword lmap_index = face_lmap_use[i][j];
         for(int k=0; k<3; k++){
            word vi = face[k];
            if(vert_lm_use[vi]==0xffff || vert_lm_use[vi]==lmap_index){
               vert_lm_use[vi] = word(lmap_index);
            }else{
                              //save duplicated vertex index, make copy out of loop
               vertex_dup_info.push_back(vi);
               vert_lm_use.push_back(word(lmap_index));

               face[k] = word(num_verts);
               num_verts++;
            }
         }
      }
   }
   if(vertex_dup_info.size()){
                             //duplicate all vertices now at once
      mesh->vertex_buffer.DuplicateVertices(&vertex_dup_info.front(), vertex_dup_info.size());
      mesh->SetFaces(face_buf.begin(), false);

#ifndef GL
      vertex_buffer.DestroyD3DVB();
#endif
      vis_flags &= ~VISF_DEST_UV0_VALID;
   }
   //mesh->GetIndexBuffer().Unlock();
}

//----------------------------

void I3D_lit_object::MapVertices(){

                              //setup light-map texture
   int vertex_count = mesh->vertex_buffer.NumVertices();
                              //re-map UV coords of this face group
   const void *v_src = mesh->vertex_buffer.Lock(D3DLOCK_READONLY);
   dword vstride = mesh->vertex_buffer.GetSizeOfVertex();

   void *dst_ptr;             //pointer to destination uv vertices
   dword dst_stride;          //stride of dest vertex

#ifndef GL
   if(drv->IsDirectTransform()){
      dst_ptr = ((byte*)vertex_buffer.Lock(0)) + GetVertexComponentOffset(vertex_buffer.GetFVFlags(), 1<<D3DFVF_TEXCOUNT_SHIFT);
      dst_stride = vertex_buffer.GetSizeOfVertex();
   }else
#endif
   {
      dst_ptr = ((byte*)v_src) + GetVertexComponentOffset(mesh->vertex_buffer.GetFVFlags(), 1<<D3DFVF_TEXCOUNT_SHIFT);
      dst_stride = vstride;
   }

                              //array of 'vertex processed' flags, to avoid duplicating work
   C_vector<bool> v_ok;
   v_ok.resize(vertex_count, false);

   assert(lm_alloc_block);
   float tx = (float)lm_alloc_block->lms->txt->SizeX1();
   float ty = (float)lm_alloc_block->lms->txt->SizeY1();
   float r_tx = 1.0f / tx;
   float r_ty = 1.0f / ty;
   float half_texel_x = .5f * r_tx;
   float half_texel_y = .5f * r_ty;

                              //make all face-groups
   //const I3D_triface *faces = mesh->GetIndexBuffer().LockForReadOnly();
   C_buffer<I3D_triface> face_buf(mesh->NumFaces1());
   mesh->GetFaces(face_buf.begin());
   CPI3D_triface faces = face_buf.begin();

   for(dword j = 0; j<mesh->NumFGroups1(); j++){
      const I3D_face_group *fg = &mesh->GetFGroups1()[j];
      const C_buffer<word> &face_lmaps = face_lmap_use[j];

      for(dword i = 0; i<fg->num_faces; i++){
         int lm_index = face_lmaps[i];
         const I3D_triface &face = *faces++;
         const S_lmap_rectangle *lm_rect = lm_rects[lm_index];

                              //compute texture coordinates in light-map texture
         const I3D_LM_rect &t_rc = lm_alloc_block->rect_list[lm_index];

         for(int j=0; j<3; j++){
            int vertex_index = face[j];
            if(v_ok[vertex_index])
               continue;
            v_ok[vertex_index] = true;

            float u, v;
            {
               const S_vector &vs = *(S_vector*)(((byte*)v_src) + vertex_index*vstride);
               const S_matrix &m = lm_rect->m;
                              //transform into 2D uniform rectangle space
               S_vector2 vt;
               vt.x = vs.x * m.m[0][0] + vs.y * m.m[1][0] + vs.z * m.m[2][0] + m.m[3][0];
               vt.y = vs.x * m.m[0][1] + vs.y * m.m[1][1] + vs.z * m.m[2][1] + m.m[3][1];
               //assert(vt.x >= 0.0f && vt.x <= 1.0f);
               //assert(vt.y >= 0.0f && vt.y <= 1.0f);
                              //transform to real LM texture space
               u = (t_rc.rect.l + (t_rc.rect.r-t_rc.rect.l-1) * vt.x) * r_tx + half_texel_x;
               v = (t_rc.rect.t + (t_rc.rect.b-t_rc.rect.t-1) * vt.y) * r_ty + half_texel_y;
            }
            I3D_text_coor *txt = (I3D_text_coor*) (((byte*)dst_ptr) + vertex_index * dst_stride);
            txt->x = u;
            txt->y = v;
         }
      }
   }
   //mesh->GetIndexBuffer().Unlock();
   mesh->vertex_buffer.Unlock();

#ifndef GL
   if(drv->IsDirectTransform())
      vertex_buffer.Unlock();
   //else
#endif
      //mesh->vertex_buffer.Unlock();
}

//----------------------------

void I3D_lit_object::UploadLMaps(){

                              //if lm_alloc_block dirty (texture released), delete it
   if(lm_alloc_block && (!lm_alloc_block->lms || (lm_alloc_block->lms->txt->GetTxtFlags()&TXTF_UPLOAD_NEEDED))){
      drv->LMFree(lm_alloc_block);
      lm_alloc_block = NULL;
      for(int i=lm_rects.size(); i--; )
         lm_rects[i]->dirty = true;
      any_rect_dirty = true;
   }
                              //if lm_alloc_block not allocated, do it now
   if(!lm_alloc_block){
      while(true){
         C_vector<I3D_rectangle> v;
         v.reserve(lm_rects.size());

         for(dword i=0; i<lm_rects.size(); i++){
            const S_lmap_rectangle *lm_rect = lm_rects[i];

            v.push_back(I3D_rectangle());
            I3D_rectangle &rc = v.back();
            rc.r = lm_rect->size_x;
            rc.b = lm_rect->size_y;
         }
         lm_alloc_block = drv->LMAlloc(&v);
         if(!lm_alloc_block){
                           //serious problem - we don't have space for lightmap
            return;
         }
         MapVertices();
         break;
      }
   }

   lm_alloc_block->last_render_time = drv->GetRenderTime();

   if(any_rect_dirty){

      PI3D_texture tp_lmap = lm_alloc_block->lms->txt;

      IDirect3DTexture9 *dst = tp_lmap->GetSysmemImage()->GetDirectXTexture();
                           //update all dirty lm_rects
      for(int i=lm_rects.size(); i--; ){
         S_lmap_rectangle *lm_rect = lm_rects[i];
         if(lm_rect->dirty){
            const I3D_rectangle &rc_dst = lm_alloc_block->rect_list[i].rect;

            D3DLOCKED_RECT lrc;
            HRESULT hr = dst->LockRect(0, &lrc, (LPRECT)&rc_dst, 0);
            CHECK_D3D_RESULT("LockRect", hr);
            if(FAILED(hr))
               continue;

            void *dstp = lrc.pBits;

            {
               drv->rgbconv_lm->Convert(
                  lm_rect->dst_map,
                  dstp,
                  lm_rect->size_x, lm_rect->size_y,
                  lm_rect->size_x*sizeof(S_lmap_pixel),  //srcpitch
                  lrc.Pitch,              //dstpitch
                  sizeof(S_lmap_pixel),   //srcbpp
                  NULL,          //pal
                  ((drv->GetFlags2()&DRVF2_LMDITHER) ? MCONV_DITHER : 0),
                  0);
            }
            hr = dst->UnlockRect(0);
            CHECK_D3D_RESULT("UnlockRect", hr);
            lm_rect->dirty = false;
         }
      }
      tp_lmap->SetDirty();
      any_rect_dirty = false;
   }
}

//----------------------------

void I3D_lit_object::AddPrimitives(S_preprocess_context &pc){

#ifndef GL
   if(pc.mode==RV_SHADOW_CASTER)
      return;
#endif
   if(!mesh)
      return;

#ifdef DEBUG_REAL_TIME
   MakeLight(pc.scene,
      I3D_LIGHT_GEO_COLLISIONS |
      I3D_LIGHT_BLUR |
      0, NULL, NULL, 0);
#endif
   
   bool use_lmapping = (drv->GetFlags()&DRVF_USELMAPPING);
   bool is_lmapping = (lmap_flags&LMAPF_DRAW_LIGHT_MAP);
   if(use_lmapping != is_lmapping){
      lmap_flags &= ~LMAPF_DRAW_LIGHT_MAP;
      vis_flags &= ~VISF_DEST_LIGHT_VALID;
      if(use_lmapping){
         lmap_flags |= LMAPF_DRAW_LIGHT_MAP;
      }else{
                           //release light-map resources
         if(lm_alloc_block){
            drv->LMFree(lm_alloc_block);
            lm_alloc_block = NULL;
            for(int i=lm_rects.size(); i--; )
               lm_rects[i]->dirty = true;
            any_rect_dirty = true;
         }
      }
#ifndef GL
      vertex_buffer.DestroyD3DVB();
#endif
      vis_flags &= ~VISF_DEST_PREPARED;
   }
   I3D_visual::AddPrimitives1(mesh, pc);
                              //upload light-maps into textures, if necessary
   if((lmap_flags&LMAPF_CONSTRUCTED) && (lmap_flags&LMAPF_DRAW_LIGHT_MAP))
      UploadLMaps();
}

//----------------------------
#ifndef GL
void I3D_lit_object::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   PI3D_mesh_base mb = mesh;
   /*
   CPI3D_face_group fgrps;
   dword vertex_count;
   dword base_index;
   dword num_fg;

   if(rp.curr_auto_lod < 0){
      fgrps = mb->GetFGroupVector().begin();
      num_fg = mb->GetFGroupVector().size();
      vertex_count = mesh->vertex_buffer.NumVertices();
      base_index = mb->GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(mb->GetIndexBuffer().GetD3DIndexBuffer());
   }else{
      const C_auto_lod &al = mb->GetAutoLODs()[rp.curr_auto_lod];
      fgrps = al.fgroups.begin();
      num_fg = al.fgroups.size();
      vertex_count = al.vertex_count;
      base_index = al.GetIndexBuffer().D3D_index_buffer_index;
      drv->SetIndices(al.GetIndexBuffer().GetD3DIndexBuffer());
   }
   */
   CPI3D_face_group fgrps = mb->GetFGroupVector().begin();
   dword num_fg = mb->GetFGroupVector().size();
   dword vertex_count = mesh->vertex_buffer.NumVertices();
   dword base_index = mb->GetIndexBuffer().D3D_index_buffer_index;
   drv->SetIndices(mb->GetIndexBuffer().GetD3DIndexBuffer());

   assert(pc.mode!=RV_SHADOW_CASTER);
                              //perform typical settings
   drv->SetupBlend(rp.blend_mode);

   drv->SetVSDecl(vertex_buffer.vs_decl);

   if(drv->IsDirectTransform()){
      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
   }else{
      drv->SetStreamSource(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.GetSizeOfVertex());
      drv->DisableTextureStage(1);
   }

   (this->*DrawPrim)(pc, rp, fgrps, num_fg, vertex_count, base_index);
}
#endif
//----------------------------

void I3D_lit_object::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   PI3D_mesh_base mb = mesh;

   CPI3D_face_group fgrps = mb->GetFGroupVector().begin();
   dword num_fg = mb->GetFGroupVector().size();
   dword vertex_count = mesh->vertex_buffer.NumVertices();
   dword base_index = mb->GetIndexBuffer().D3D_index_buffer_index;
   drv->SetIndices(mb->GetIndexBuffer().GetD3DIndexBuffer());
#ifndef GL
   assert(pc.mode!=RV_SHADOW_CASTER);
#endif
                              //perform typical settings
   drv->SetupBlend(rp.blend_mode);
#ifndef GL
   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
   drv->SetVSDecl(vertex_buffer.vs_decl);
#else
   drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.GetSizeOfVertex());
   drv->SetVSDecl(mesh->vertex_buffer.vs_decl);
#endif

   (this->*DrawPrim)(pc, rp, fgrps, num_fg, vertex_count, base_index);
}

//----------------------------

#define FACE_GROUP_RENDER_LOOP(free_stg) \
   for(dword fgi=num_fg; fgi--; ){ \
      const I3D_face_group *fg = &fgrps[fgi]; \
      CPI3D_material mat = fg->GetMaterial1(); \
      drv->EnableNoCull(mat->Is2Sided1()); \
      SetupSpecialMapping(mat, &rp, free_stg); \
      HRESULT hr;

#define FACE_GROUP_RENDER_LOOP1 \
   for(dword fgi=num_fg; fgi--; ){ \
      const I3D_face_group *fg = &fgrps[fgi]; \
      CPI3D_material mat = fg->GetMaterial1(); \
      drv->EnableNoCull(mat->Is2Sided1()); \
      HRESULT hr; hr = 0;

#ifndef GL
//----------------------------
// Absolute failback - should be never executed on most cards.
void I3D_lit_object::DPDirect_B_L(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   drv->DisableTextureStage(1);
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      {
                              //render 1st texture
         SetupLMTextureToStage0();
         drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         drv->SetupBlend(rp.blend_mode);

         {
            dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV | VSPREP_NO_DETAIL_MAPS;
            I3D_driver::S_vs_shader_entry_in se;
            se.CopyUV(1, 0);
            PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);
         }
         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
            (base_index + fg->base_index) * 3, fg->num_faces);
         CHECK_D3D_RESULT("DrawIP", hr);
      }
      {
                              //render 2nd texture
         drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
         drv->SetupBlend(I3DBLEND_MODULATE2X);
         dword save_fog = drv->GetFogColor();
         if(rp.flags&RP_FLAG_FOG_VALID) drv->SetFogColor(0x80808080);
         {
            I3D_driver::S_vs_shader_entry_in se;
            PrepareVertexShader(mat, 2, se, rp, pc.mode, VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV | VSPREP_NO_DETAIL_MAPS);
         }

         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
            (base_index + fg->base_index) * 3, fg->num_faces);
         CHECK_D3D_RESULT("DrawIP", hr);

         if(rp.flags&RP_FLAG_FOG_VALID)
            drv->SetFogColor(save_fog);
      }
   }
}

//----------------------------

void I3D_lit_object::DPIndirect_B_L(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   PI3D_mesh_base mb = mesh;

                              //process vertices
   {
      /*
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }
      */
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_DETAIL_MAPS;
      //if(!(vis_flags&VISF_DEST_UV0_VALID)){
         //vis_flags |= VISF_DEST_UV0_VALID;
         prep_flags |= VSPREP_COPY_UV;
      //}
      I3D_driver::S_vs_shader_entry_in se;
      se.CopyUV(1, 1);
      if(!(vis_flags&VISF_DEST_LIGHT_VALID)){
         if(drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD)
            prep_flags |= VSPREP_MAKELIGHTING | VSPREP_LIGHT_LM_DYNAMIC;
         vis_flags |= VISF_DEST_LIGHT_VALID;
      }

      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      drv->SetVSDecl(vertex_buffer.vs_decl);

      hr = d3d_dev->ProcessVertices(mb->vertex_buffer.D3D_vertex_buffer_index,
         vertex_buffer.D3D_vertex_buffer_index, vertex_count,
         vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
      CHECK_D3D_RESULT("ProcessVertices", hr);
      drv->SetVertexShader(NULL);
   }
   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
   drv->SetFVF(vertex_buffer.GetFVFlags());
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
                              //render 1st texture
      {
         SetupLMTextureToStage0();
         drv->SetTextureCoordIndex(0, 1);
         drv->SetupTextureStage(0, (drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD) ? D3DTOP_ADD : D3DTOP_SELECTARG1);
         drv->SetupBlend(rp.blend_mode);

         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
            (base_index + fg->base_index) * 3, fg->num_faces);

         CHECK_D3D_RESULT("DrawIP", hr);
      }
                              //render 2nd texture
      {
         drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
         drv->SetTextureCoordIndex(0, 0);
         drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         drv->SetupBlend(I3DBLEND_MODULATE2X);
         dword save_fog = drv->GetFogColor();
         if(rp.flags&RP_FLAG_FOG_VALID) drv->SetFogColor(0x80808080);

         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
            (base_index + fg->base_index) * 3, fg->num_faces);
         CHECK_D3D_RESULT("DrawIP", hr);

         if(rp.flags&RP_FLAG_FOG_VALID)
            drv->SetFogColor(save_fog);
      }
   }
}

//----------------------------

void I3D_lit_object::DPDirect_BLd(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   {
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV;
      I3D_driver::S_vs_shader_entry_in se;
      if(drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD)
         prep_flags |= VSPREP_MAKELIGHTING | VSPREP_LIGHT_LM_DYNAMIC;
      se.CopyUV(1, 0);
      se.CopyUV(0, 1);
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);
   }
   drv->SetupAlphaTest(false);
   bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);

                              //add diffuse color for additional lighting
   drv->SetupTextureStage(0,
      (has_diffuse && (drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD)) ? D3DTOP_ADD : D3DTOP_SELECTARG1);
                              
   SetupLMTextureToStage0();
   drv->SetupTextureStage(1, D3DTOP_MODULATE2X);

   FACE_GROUP_RENDER_LOOP(2)
      drv->SetTexture1(1, mat->GetTexture1(MTI_DIFFUSE));

      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0,
         vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}

//----------------------------

void I3D_lit_object::DPDirect_BLSd(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   {
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV;
      I3D_driver::S_vs_shader_entry_in se;
      if(drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD)
         prep_flags |= VSPREP_MAKELIGHTING | VSPREP_LIGHT_LM_DYNAMIC;
      se.CopyUV(1, 0);
      se.CopyUV(0, 1);
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);
   }
   bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);
   drv->SetupAlphaTest(false);

                              //add diffuse color for additional lighting
   drv->SetupTextureStage(0,
      (has_diffuse && (drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD)) ? D3DTOP_ADD : D3DTOP_SELECTARG1);
                              
   SetupLMTextureToStage0();
   drv->SetupTextureStage(1, D3DTOP_MODULATE2X);

   FACE_GROUP_RENDER_LOOP(2)
      drv->SetTexture1(1, mat->GetTexture1(MTI_DIFFUSE));

      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0,
         vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}

//----------------------------

void I3D_lit_object::DPIndirect_BLd(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

                              //process vertices
   {
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV;

      I3D_driver::S_vs_shader_entry_in se;
      se.CopyUV(0, 1);
      se.CopyUV(1, 0);
      if(!(vis_flags&VISF_DEST_LIGHT_VALID)){

         if(drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD)
            prep_flags |= VSPREP_MAKELIGHTING | VSPREP_LIGHT_LM_DYNAMIC;
         vis_flags |= VISF_DEST_LIGHT_VALID;
      }
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      hr = d3d_dev->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index,
         vertex_buffer.D3D_vertex_buffer_index, vertex_count,
         vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
      CHECK_D3D_RESULT("ProcessVertices", hr);
      drv->SetVertexShader(NULL);

      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
      drv->SetFVF(vertex_buffer.GetFVFlags());
   }

                           //add diffuse color for additional lighting
   drv->SetupTextureStage(0, (drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD) ? D3DTOP_ADD : D3DTOP_SELECTARG1);

                              //setup lightmap texture into stage 0
   SetupLMTextureToStage0();
   drv->SetupTextureStage(1, D3DTOP_MODULATE2X);
   drv->SetupAlphaTest(false);
                              //use only vertex alpha
   if(rp.blend_mode!=I3DBLEND_OPAQUE)
      d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);

   FACE_GROUP_RENDER_LOOP(2)
      drv->SetTexture1(1, mat->GetTexture1(MTI_DIFFUSE));

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
   if(rp.blend_mode!=I3DBLEND_OPAQUE)
      d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
}

//----------------------------

void I3D_lit_object::DPIndirect_BLSd(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;
                              //process vertices
   {
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV;

      I3D_driver::S_vs_shader_entry_in se;
      se.CopyUV(0, 1);
      se.CopyUV(1, 0);
      if(!(vis_flags&VISF_DEST_LIGHT_VALID)){

         if(drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD)
            prep_flags |= VSPREP_MAKELIGHTING | VSPREP_LIGHT_LM_DYNAMIC;
         vis_flags |= VISF_DEST_LIGHT_VALID;
      }
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      hr = d3d_dev->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index,
         vertex_buffer.D3D_vertex_buffer_index, vertex_count,
         vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
      CHECK_D3D_RESULT("ProcessVertices", hr);
      drv->SetVertexShader(NULL);

      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
      drv->SetFVF(vertex_buffer.GetFVFlags());
   }

                              //pass 1 - draw base texture (opaque)
   drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
   drv->EnableAnisotropy(0, true);
   drv->DisableTextureStage(1);
   drv->SetTextureCoordIndex(0, 1);
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }
   drv->SetTextureCoordIndex(0, 0);
                              //pass 2 - blend in secondary texture
   drv->SetupBlend(I3DBLEND_ALPHABLEND);
   drv->SetupTextureStage(1, D3DTOP_SELECTARG1);
   SetupLMTextureToStage0();
   d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
   drv->DisableTextureStage(2);
   drv->SetAlphaRef(0x02);
   drv->SetupAlphaTest(true);
   drv->EnableZWrite(false);
   dword save_fog = drv->GetFogColor();

   {FACE_GROUP_RENDER_LOOP1
      CPI3D_texture_base ts = mat->GetTexture1(MTI_SECONDARY);
      if(!ts)
         continue;
      drv->SetTexture1(1, ts);

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }}
   d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
   drv->SetupAlphaTest(false);

   if(rp.flags&RP_FLAG_FOG_VALID) drv->SetFogColor(0x80808080);
                              //pass 3 - blend in lightmap texture (and optionally detailmap)
   drv->SetupBlend(I3DBLEND_MODULATE2X);
   drv->SetupTextureStage(0, (drv->GetFlags2()&DRVF2_CAN_TXTOP_ADD) ? D3DTOP_ADD : D3DTOP_SELECTARG1);

   {FACE_GROUP_RENDER_LOOP1
      CPI3D_texture_base tp_det = mat->GetTexture1(MTI_DETAIL);
      if(!tp_det)
         drv->DisableTextureStage(1);
      else{
         drv->SetupTextureStage(1, D3DTOP_MODULATE2X);
         drv->SetTexture1(1, tp_det);
         drv->SetTextureCoordIndex(1, 2);
      }

      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }}
   drv->EnableZWrite(true);
   if(rp.flags&RP_FLAG_FOG_VALID)
      drv->SetFogColor(save_fog);
   drv->SetTextureCoordIndex(1, 1);
}

//----------------------------

void I3D_lit_object::DPDirect_BL_D(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   DPDirect_BLd(pc, rp, fgrps, num_fg, vertex_count, base_index);
   DPDirect_D(pc, rp, fgrps, num_fg, vertex_count, base_index);
}

//----------------------------

void I3D_lit_object::DPIndirect_BL_D(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   DPIndirect_BLd(pc, rp, fgrps, num_fg, vertex_count, base_index);
   DPIndirect_D(pc, rp, fgrps, num_fg, vertex_count, base_index);
}

//----------------------------

void I3D_lit_object::DPDirect_B_L_D(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   DPDirect_B_L(pc, rp, fgrps, num_fg, vertex_count, base_index);
   DPDirect_D(pc, rp, fgrps, num_fg, vertex_count, base_index);
}

//----------------------------

void I3D_lit_object::DPIndirect_B_L_D(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   DPIndirect_B_L(pc, rp, fgrps, num_fg, vertex_count, base_index);
   DPIndirect_D(pc, rp, fgrps, num_fg, vertex_count, base_index);
}

//----------------------------

void I3D_lit_object::DPDirect_D(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   bool any = false;
   dword save_fog = drv->GetFogColor();

   FACE_GROUP_RENDER_LOOP1
      CPI3D_texture_base tp_det = mat->GetTexture1(MTI_DETAIL);
      if(!tp_det)
         continue;
      if(!any){
         drv->SetupBlend(I3DBLEND_MODULATE2X);
         dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV;
         I3D_driver::S_vs_shader_entry_in se;

         se.AddFragment(VSF_PICK_UV_0);
         se.AddFragment(VSF_MULTIPLY_UV_BY_XY);
         se.AddFragment((E_VS_FRAGMENT)(VSF_STORE_UV_0 + 2));

         PrepareVertexShader(fgrps->GetMaterial1(), 0, se, rp, pc.mode, prep_flags);
         drv->EnableAnisotropy(0, true);
         drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         drv->DisableTextureStage(1);
         if(rp.flags&RP_FLAG_FOG_VALID) drv->SetFogColor(0x80808080);
         drv->SetupAlphaTest(false);
         any = true;
      }
      drv->SetTexture1(0, tp_det);
      {
         S_vectorw uv_scale;
         (S_vector2&)uv_scale = mat->GetDetailScale();
         uv_scale.z = 0.0f;
         uv_scale.w = 0.0f;
         drv->SetVSConstant(VSC_UV_SHIFT_SCALE, &uv_scale);
      }
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }
   if(any){
      if(rp.flags&RP_FLAG_FOG_VALID)
         drv->SetFogColor(save_fog);
   }
}

//----------------------------

void I3D_lit_object::DPIndirect_D(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   bool any = false;
   dword save_fog = drv->GetFogColor();
   FACE_GROUP_RENDER_LOOP1
      CPI3D_texture_base tp_det = mat->GetTexture1(MTI_DETAIL);
      if(!tp_det)
         continue;
      if(!any){
         drv->SetupBlend(I3DBLEND_MODULATE2X);
         if(rp.flags&RP_FLAG_FOG_VALID) drv->SetFogColor(0x80808080);

         drv->SetTextureCoordIndex(0, 2);
         drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
         drv->EnableAnisotropy(0, true);
         drv->DisableTextureStage(1);
         drv->SetupAlphaTest(false);
         any = true;
      }
      drv->SetTexture1(0, tp_det);
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0,
         vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }
   if(any){
      drv->SetTextureCoordIndex(0, 0);
      if(rp.flags&RP_FLAG_FOG_VALID)
         drv->SetFogColor(save_fog);
   }
}

//----------------------------

void I3D_lit_object::DP_L(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   if(drv->IsDirectTransform()){
      //last_auto_lod = rp.curr_auto_lod;
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_DETAIL_MAPS;
      I3D_driver::S_vs_shader_entry_in se;
      se.CopyUV(1, 0);
      PrepareVertexShader(fgrps->GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

      drv->DisableTextureStage(1);
   }else{
      /*
                                 //process vertices
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }
      */
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_DETAIL_MAPS;
      I3D_driver::S_vs_shader_entry_in se;
      se.CopyUV(1, 0);
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      HRESULT hr;
      hr = d3d_dev->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index,
         vertex_buffer.D3D_vertex_buffer_index, vertex_count,
         vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
      CHECK_D3D_RESULT("ProcessVertices", hr);
      drv->SetVertexShader(NULL);

      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
      drv->SetFVF(vertex_buffer.GetFVFlags());
   }

   SetupLMTextureToStage0();
   drv->SetupTextureStage(0, D3DTOP_SELECTARG1);
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }
}

//----------------------------

void I3D_lit_object::DPUnconstructed(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   if(drv->IsDirectTransform()){
      //last_auto_lod = rp.curr_auto_lod;

      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV | VSPREP_NO_DETAIL_MAPS;
      I3D_driver::S_vs_shader_entry_in se;
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      drv->DisableTextureStage(1);
   }else{
      /*
                              //process vertices
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }
      */
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV | VSPREP_NO_DETAIL_MAPS;
      I3D_driver::S_vs_shader_entry_in se;
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      hr = d3d_dev->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index,
         vertex_buffer.D3D_vertex_buffer_index, vertex_count,
         vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
      CHECK_D3D_RESULT("ProcessVertices", hr);
      drv->SetVertexShader(NULL);

      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
      drv->SetFVF(vertex_buffer.GetFVFlags());
   }
   drv->SetupTextureStage(0, D3DTOP_MODULATE);
   d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
   drv->SetTextureFactor(0xff00ffff);
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }
   d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
}

//----------------------------

void I3D_lit_object::DP_B(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   if(drv->IsDirectTransform()){
      //last_auto_lod = rp.curr_auto_lod;
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV | VSPREP_NO_DETAIL_MAPS | VSPREP_MAKELIGHTING;
      I3D_driver::S_vs_shader_entry_in se;
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      drv->DisableTextureStage(1);
   }else{
                                 //process vertices
      /*
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }
      */
      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV | VSPREP_NO_DETAIL_MAPS;
      I3D_driver::S_vs_shader_entry_in se;
      if(!(vis_flags&VISF_DEST_LIGHT_VALID)){
         prep_flags |= VSPREP_MAKELIGHTING;
         vis_flags |= VISF_DEST_LIGHT_VALID;
      }
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode, prep_flags);

      HRESULT hr;
      hr = d3d_dev->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index,
         vertex_buffer.D3D_vertex_buffer_index, vertex_count,
         vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
      CHECK_D3D_RESULT("ProcessVertices", hr);
      drv->SetVertexShader(NULL);

      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
      drv->SetFVF(vertex_buffer.GetFVFlags());
   }
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES)){
         drv->SetupTextureStage(0, D3DTOP_MODULATE);
         drv->SetTexture1(0, NULL);
      }else{
         drv->SetupTextureStage(0, drv->vertex_light_blend_op);
         drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
      }
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr); \
   }
}
#endif
//----------------------------

void I3D_lit_object::DPUnconstructed_PS(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   {
      I3D_driver::S_vs_shader_entry_in se;
      PrepareVertexShader(fgrps->GetMaterial1(), 1, se, rp, pc.mode,
         VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV | VSPREP_NO_DETAIL_MAPS);
   }
   {
      I3D_driver::S_ps_shader_entry_in se_ps;
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_MOD_t0_CONSTCOLOR);
#ifndef GL
      if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
         se_ps.TexKill(1);
#endif
      drv->DisableTextures(1);

      drv->SetPixelShader(se_ps);
      static const S_vectorw c(0, 1, 1, 1);
      drv->SetPSConstant(PSC_COLOR, &c);
   }
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
#ifndef GL
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
#else
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mesh->vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count,
         (base_index + fg->base_index) * 3, fg->num_faces);
#endif
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}

//----------------------------

void I3D_lit_object::DP_B_PS(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   {
      I3D_driver::S_vs_shader_entry_in se;
      PrepareVertexShader(fgrps->GetMaterial1(), 1, se, rp, pc.mode,
         VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV | VSPREP_NO_DETAIL_MAPS | VSPREP_MAKELIGHTING);
   }
   drv->SetupAlphaTest(false);
   bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);

   FACE_GROUP_RENDER_LOOP1
      I3D_driver::S_ps_shader_entry_in se_ps;
      CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
      if((drv->GetFlags2()&DRVF2_DRAWTEXTURES) && tb){
         se_ps.Tex(0);
         drv->SetTexture1(0, tb);
         se_ps.AddFragment(has_diffuse ? PSF_MODX2_t0_v0 : PSF_COPY_BLACK_t0a);
         SetupSpecialMappingPS(mat, se_ps, 1);
      }else{
         se_ps.AddFragment(has_diffuse ? PSF_v0_COPY : PSF_COPY_BLACK);
         SetupSpecialMappingPS(mat, se_ps, 1);
      }
      drv->SetPixelShader(se_ps);
#ifndef GL
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
#else
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mesh->vertex_buffer.D3D_vertex_buffer_index, 0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
#endif
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}

//----------------------------

void I3D_lit_object::DP_BLd_PS(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   {
      I3D_driver::S_vs_shader_entry_in se;
      se.CopyUV(1, 0);
      se.CopyUV(0, 1);
      PrepareVertexShader(fgrps->GetMaterial1(), 2, se, rp, pc.mode,
         VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV | VSPREP_MAKELIGHTING | VSPREP_LIGHT_LM_DYNAMIC);
   }
   bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);

   SetupLMTextureToStage0();
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      I3D_driver::S_ps_shader_entry_in se_ps;
      se_ps.Tex(0);              //lightmap texture
                              //add lightmap with vertex color
      if(has_diffuse)
         se_ps.AddFragment(PSF_ADD_t0_v0);

      CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
      if((drv->GetFlags2()&DRVF2_DRAWTEXTURES) && tb){
         se_ps.Tex(1);
         drv->SetTexture1(1, tb);
                                 //modulate with base texture
         se_ps.AddFragment(has_diffuse ? PSF_MODX2_r0_t1 : PSF_MODX2_t0_t1);
      }else
      if(!has_diffuse)
         se_ps.AddFragment(PSF_t0_COPY);
      SetupSpecialMappingPS(mat, se_ps, 2);
      drv->SetPixelShader(se_ps);
#ifndef GL
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0,
         vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
#else
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, mesh->vertex_buffer.D3D_vertex_buffer_index, 0,
         vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
#endif
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}

//----------------------------

void I3D_lit_object::DP_BLSd_PS(const S_preprocess_context &pc, const S_render_primitive &rp,
   CPI3D_face_group fgrps, dword num_fg, dword vertex_count, dword base_index){

   if(!lm_alloc_block) return;
   {
      I3D_driver::S_vs_shader_entry_in se;
      se.CopyUV(1, 0);
      se.CopyUV(0, 1);
      se.CopyUV(0, 2);
      PrepareVertexShader(fgrps->GetMaterial1(), 3, se, rp, pc.mode,
         VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV | VSPREP_MAKELIGHTING | VSPREP_LIGHT_LM_DYNAMIC);
   }
   bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);

   SetupLMTextureToStage0();
   drv->SetupAlphaTest(false);

   FACE_GROUP_RENDER_LOOP1
      I3D_driver::S_ps_shader_entry_in se_ps;
      se_ps.Tex(0);           //lightmap texture
                              //add lightmap with vertex color
      if(has_diffuse)
         se_ps.AddFragment(PSF_ADD_t0_v0);

      CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
      if((drv->GetFlags2()&DRVF2_DRAWTEXTURES) && tb){
         se_ps.Tex(1);
         drv->SetTexture1(1, tb);
                              //mix secondary color (if any)
         CPI3D_texture_base ts = mat->GetTexture1(MTI_SECONDARY);
         if(!ts){
                              //without secondary texture - modulate with base texture
            se_ps.AddFragment(has_diffuse ? PSF_MODX2_r0_t1 : PSF_MODX2_t0_t1);
         }else{
            se_ps.Tex(2);
            drv->SetTexture1(2, ts);
            se_ps.AddFragment(PSF_BLEND_BY_ALPHA);
            se_ps.AddFragment(!has_diffuse ? PSF_MODX2_t0_r1 : PSF_MODX2_r0_r1);
         }
      }else
      if(!has_diffuse)
         se_ps.AddFragment(PSF_t0_COPY);
      SetupSpecialMappingPS(mat, se_ps, 3);
      drv->SetPixelShader(se_ps);
#ifndef GL
      hr = drv->GetDevice1()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0,
         vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr);
#endif
   }
}

//----------------------------
//----------------------------

struct S_rect_lm{
   int indx;
   I3D_rectangle rc;
   S_rect_lm(){}
   S_rect_lm(const I3D_rectangle &rc1, int i):
      indx(i),
      rc(rc1)
   {}
   bool operator <(const S_rect_lm &r2) const{
      return ((rc.r*rc.b) < (r2.rc.r*r2.rc.b));
   }
};

//----------------------------

struct S_surf{
   C_LM_surface *surf;
   int sort_value;
   S_surf(){}
   S_surf(C_LM_surface *s1):
      surf(s1),
      sort_value(s1->alloc.GetFreeSize())
   {}
   bool operator <(const S_surf &s2) const{
      return (sort_value < s2.sort_value);
   }
};

//----------------------------

static int LmRectCompare(const void *_r1, const void *_r2){
   S_rect_lm &r1 = *(S_rect_lm*)_r1;
   S_rect_lm &r2 = *(S_rect_lm*)_r2;
   return (r1<r2) ? -1 : (r2<r1) ? 1 : 0;
}

static int LmSurfCompare(const void *_s1, const void *_s2){
   S_surf &s1 = *(S_surf*)_s1;
   S_surf &s2 = *(S_surf*)_s2;
   return (s1<s2) ? -1 : (s2<s1) ? 1 : 0;
}

//----------------------------

C_LM_alloc_block *I3D_driver::LMAlloc(C_vector<I3D_rectangle> *rect_list){

   int i;
   int req_size = 0;
   dword max_x = 0;
   dword max_y = 0;
                              //sort input rectangles by size
   C_vector<S_rect_lm> rl;
   rl.reserve(rect_list->size());
   for(i=rect_list->size(); i--; ){
      rl.push_back(S_rect_lm((*rect_list)[i], i));
      const S_rect_lm &rc = rl.back();
      req_size += rc.rc.r * rc.rc.b;
      max_x = Max(max_x, (dword)rc.rc.r);
      max_y = Max(max_y, (dword)rc.rc.b);
   }
   qsort(rl.begin(), rl.size(), sizeof(S_rect_lm), &LmRectCompare);


   C_LM_alloc_block *ab = new C_LM_alloc_block(rect_list->size());

   C_LM_surface *lms = NULL;
   bool alloc_ok = false;

   C_vector<S_surf> sl;

   if(!lm_textures.size())
      goto alloc_new;

                              //check if requirements not out of possibilities
                              // assume all lmaps are the same size
   if(max_x>lm_textures.back()->txt->SizeX1() || max_y>lm_textures.back()->txt->SizeY1()){
      LM_ALLOC_ERR(this, "LightMap allocate: a block too big, can't allocate");
      return NULL;
   }

                              //Strategy:
                              // 1. search existing LM_surface-s for free space
                              // 2. search existing LM_surface-s, ignoring too old blocks
                              // 3. determine ratio (LM_surfaces:free_vidmem), if underlimit, alloc new
                              // 4. alloc on LM_surface with best (free_space:old_blocks) ratio

                              //0. sort existing surfaces by free space
   sl.reserve(lm_textures.size());
   for(i=lm_textures.size(); i--; ) sl.push_back(S_surf(lm_textures[i]));
   qsort(sl.begin(), sl.size(), sizeof(S_surf), &LmSurfCompare);

                              //1. search all existing
#if 0                         //try to fit first in those with smallest free space
   for(i=0; i<sl.size(); i++){
      if(sl[i].sort_value < req_size*2) continue;
#else                         //try to fit first in those with largest free space
   for(i=sl.size(); i--; ){
      if(sl[i].sort_value < req_size*2) break;
#endif
      LM_ALLOC_LOG(this, "lm allocate: trying in existing texture...");
      C_LM_surface *lms1 = sl[i].surf;
                              //try to allocate all surfaces here
      for(int j=rl.size(); j--; ){
         bool b = lms1->alloc.Allocate(rl[j].rc.r, rl[j].rc.b, ab, ab->rect_list[rl[j].indx].rect);
         if(!b) break;
      }
      if(j==-1){              //all surfaces allocated OK, quit
         LM_ALLOC_LOG(this, "alloc OK");
         lms = lms1;
         lms->AddRef();
         alloc_ok = true;
         break;
      }
      LM_ALLOC_LOG(this, "failed to alloc");
      lms1->alloc.Free(ab);   //release surfaces
   }

   if(!lms){
      dword curr_lm_pixels = lm_textures.back()->txt->SizeX1() * lm_textures.back()->txt->SizeY1() * sl.size();
      if(curr_lm_pixels < MAX_LM_PIXELS)
      {                       //3. if we're still in limit with texture memory, go on and allocate new
                              // assume all lmaps are the same size
         int single_size = lm_textures.back()->txt->SizeX1()*lm_textures.back()->txt->SizeY1() *
            lm_textures.back()->txt->GetSysmemImage()->GetPixelFormat()->bytes_per_pixel;
         int current_lm_size = lm_textures.size() * single_size;
         if((current_lm_size+single_size/2) < max_lm_memory)
            goto alloc_new;
      }

      const int optimal_expire_time = 20000;
                              //sort by (free_space:old_blocks) ratio
      if(sl.size()>=1)
      for(i=sl.size(); i--; ){
         C_LM_surface *lms1 = sl[i].surf;

         int sort_value = 0;
         for(int j=0; j<lms1->alloc.NumBlocks(); j++){
            const C_rect_allocator::S_block &bl = lms1->alloc.GetBlock(j);
            int block_size = (bl.rect.r-bl.rect.l) * (bl.rect.b-bl.rect.t);
            int age = render_time - bl.owner_id->last_render_time;
            sort_value += Min(optimal_expire_time, age) * block_size;
         }
         sl[i].sort_value = sl[i].sort_value * (optimal_expire_time/10) + sort_value;
      }
      qsort(sl.begin(), sl.size(), sizeof(S_surf), &LmSurfCompare);

      LM_ALLOC_LOG(this, "allocate: choosing lm texture with largest oldest block(s)");
      lms = sl.back().surf;
      lms->AddRef();
   }


alloc_new:
   if(!lms){
      LM_ALLOC_LOG(this, "creating new LM texture and allocating here");
                              //3. alloc new LM surface
      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      ct.flags = TEXTMAP_NOMIPMAP | TEXTMAP_USEPIXELFORMAT | TEXTMAP_HINTDYNAMIC;
      ct.pf = &pf_light_maps;
      ct.size_x = LM_REQUEST_TEXTURE_SIZE;
      ct.size_y = LM_REQUEST_TEXTURE_SIZE;

      PI3D_texture tp;
      I3D_RESULT ir;
      ir = CreateTexture(&ct, &tp);
      assert(I3D_SUCCESS(ir));
      if(GetFlags()&DRVF_DRAWLMTEXTURES)
         FillTextureByWhite(tp);
      lms = new C_LM_surface(tp);
      lm_textures.push_back(lms);

      tp->Release();
   }


   if(!alloc_ok)
   while(true){
                              //try to allocate all rectangles
      for(i=rl.size(); i--; ){
         bool b = lms->alloc.Allocate(rl[i].rc.r, rl[i].rc.b, ab, ab->rect_list[rl[i].indx].rect);
         if(!b) break;
      }
      if(i==-1){
                              //all surfaces allocated OK, quit
         LM_ALLOC_LOG(this, "alloc OK");
         break;
      }
      LM_ALLOC_LOG(this, "failed to alloc, trying next texture");
      lms->alloc.Free(ab);    //release surfaces

      if(lms->alloc.NumBlocks()){
                              //free oldest block
         int max_age = 0;     //don't accept recent blocks
         C_LM_alloc_block *oldest_block = NULL;
         for(int j=0; j<lms->alloc.NumBlocks(); j++){
            const C_rect_allocator::S_block &bl = lms->alloc.GetBlock(j);
            int age = render_time - bl.owner_id->last_render_time;
            if(max_age<age){
               max_age = age;
               oldest_block = bl.owner_id;
            }     
         }
         if(!oldest_block){
            LM_ALLOC_ERR(this, "LightMap: failed to alloc (no oldest block)");
            lms->Release();
            delete ab;
            return NULL;
         }
                              //discard oldest block
         if(GetFlags()&DRVF_DRAWLMTEXTURES){
                              //fill all discarded rects by white
            for(int j=lms->alloc.NumBlocks(); j--; ){
               const C_rect_allocator::S_block &bl = lms->alloc.GetBlock(j);
               if(bl.owner_id==oldest_block){
                  LM_ALLOC_LOG(this, "filling block by white");
                  FillTextureByWhite(lms->txt, &bl.rect);
               }
            }
         }
         LM_ALLOC_LOG(this, "freeing (old) block to gain space");
         lms->alloc.Free(oldest_block);
         oldest_block->lms->Release();
         oldest_block->lms = NULL;
      }else{
         LM_ALLOC_ERR(this, "LightMap: alloc failed, quitting");
         ab->lms = lms;
         LMFree(ab);
         return NULL;
      }
   }
   ab->lms = lms;

   return ab;
}

//----------------------------

void I3D_driver::LMFree(C_LM_alloc_block *ab){

   if(GetFlags()&DRVF_DRAWLMTEXTURES)
   if(ab->lms){               //fill all discarded rects by white
      for(int j=ab->lms->alloc.NumBlocks(); j--; ){
         const C_rect_allocator::S_block &bl = ab->lms->alloc.GetBlock(j);
         if(bl.owner_id==ab){
            LM_ALLOC_LOG(this,
               C_fstr("I3D_driver::LMFree: filling blocks by white (%i, %i, %i, %i)", 
               bl.rect.l, bl.rect.t, bl.rect.r, bl.rect.b));
            FillTextureByWhite(ab->lms->txt, &bl.rect);
         }
      }
   }

   C_LM_surface *lms = ab->lms;
                              //might be NULL, if released by surface manager (LMAlloc)
   if(lms){
                              //free all blocks on this LM texture
      lms->alloc.Free(ab);

      if(!lms->Release()){    //texture is empty, it may be released
         for(int i=lm_textures.size(); i--; ){
            if(lm_textures[i]==lms)
               break;
         }
         if(i!=-1){              //it might not be in the list because it may be lost
            lm_textures[i] = lm_textures.back();
            lm_textures.pop_back();
         }
      }
   }
   delete ab;
}

//----------------------------

bool I3D_driver::UnregLMTexture(PI3D_texture tp){

   for(int i=lm_textures.size(); i--; ){
      if(tp==lm_textures[i]->txt){
         lm_textures[i] = lm_textures.back(); lm_textures.pop_back();
         return true;
      }
   }
   return false;
}

//----------------------------

void I3D_driver::DrawLMaps(const I3D_rectangle &viewport){
                              //draw all lmaps on screen
   const int NUM_X = 8, NUM_Y = 6;

   float sx = (float)(viewport.r - viewport.l) / NUM_X;
   float sy = (float)(viewport.b - viewport.t) / NUM_Y;

   bool is_filter = (drv_flags&DRVF_LINFILTER);
   if(is_filter) SetState(RS_LINEARFILTER, false);
   SetupBlend(I3DBLEND_OPAQUE);

#ifndef GL
   if(!CanUsePixelShader()){
      SetupTextureStage(0, D3DTOP_SELECTARG1);
   }else
#endif
   {
      S_ps_shader_entry_in se_ps;
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_t0_COPY);
      SetPixelShader(se_ps);
   }

   EnableZBUsage(false);
   SetupAlphaTest(false);

   dword i = 0;
   for(int y = 0; y<NUM_Y; y++)
   for(int x = 0; x<NUM_X; x++, i++){
      if(i>=lm_textures.size())
         break;

      struct S_vertex{
         S_vector xyz;
         float rhw;
         float u, v;
      } verts[4];
      for(int j=0; j<4; j++){
         verts[j].xyz.z = 0.0f;
         verts[j].rhw = 1.0f;
         verts[j].u = float(j&1);
         verts[j].v = float((j&2)>>1);
         verts[j].xyz.x = viewport.l + x * sx;
         verts[j].xyz.y = viewport.t + y * sy;
         if(j&1) verts[j].xyz.x += sx - 1;
         if(j&2) verts[j].xyz.y += sy - 1;
      }
      SetTexture(lm_textures[i]->txt);

      SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
      HRESULT hr;
      hr = d3d_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(S_vertex));
      CHECK_D3D_RESULT("DrawPrimitiveUP", hr);
   }
   EnableZBUsage(true);
   if(is_filter) SetState(RS_LINEARFILTER, true);
   ResetStreamSource();
}

//----------------------------
//----------------------------

I3D_RESULT I3D_scene::ConstructLightmaps(PI3D_lit_object *lmaps, dword num_lmaps, dword flags,
   I3D_LOAD_CB_PROC *cb_proc, void *cb_context){

   for(dword i=0; i<num_lmaps; i++){
      PI3D_lit_object lo = lmaps[i];
      I3D_RESULT ir = lo->Construct(this, flags, cb_proc, cb_context);
      if(I3D_FAIL(ir))
         return ir;
   }
   return I3D_OK;
}

//----------------------------

extern const S_visual_property props_LitObject[] = {
                              //I3DPROP_LMAP_F_RESOLUTION
   {I3DPROP_FLOAT, "Resolution", "Resolution of light-map, in texels per unit geometry distance."},
   {I3DPROP_NULL}
};

I3D_visual *CreateLitObject(PI3D_driver drv){
   return new I3D_lit_object(drv);
}

//----------------------------
//----------------------------
