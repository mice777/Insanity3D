/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.

   File: Driver.cpp
   Content: Insanity 3D driver.
--------------------------------------------------------*/

#include "all.h"
#include "mesh.h"
#include <math.h>
#include <win_res.h>
#include "material.h"
#include "light.h"
#include "soundi3d.h"
#include "camera.h"
#include "sector.h"
#include "volume.h"
#include "scene.h"
#include "driver.h"
#include "model.h"
#include "visual.h"
#include "procedural.h"
#include "texture.h"
#include "Cpu.h"
#include "anim.h"
#include <integer.h>

#ifdef _DEBUG
#include <insanity\3DTexts.h>
#endif


static const dword BREAK_ALLOC_VB_ID = 0; //id of vertex buffer manager. 0 for disable
//static const dword BREAK_ALLOC_VB_ID = 5; //id of vertex buffer manager
static const dword BREAK_ALLOC_VERTEX_ID = 0; //id of vertex in vb

//----------------------------


//#define USE_PALETIZED_TEXTURES//if source texture is paletized, try to find paletized texture format

                              //how many recent shaders we keep in cache
#define VSHADER_CACHE_SIZE 96
#define PSHADER_CACHE_SIZE 32

//#define USE_OWN_FRAME_ALLOCATION 128   //specify heap size (kb), undef to disable own allocation


#ifdef _DEBUG
                              //debugging:
//#define OVERRIDE_F_TO_L       //override calls to _ftol with our (faster) version
//#define DEBUG_NO_LEAK_WARNING //no warning on unreleased interfaces

//#define DEBUG_NO_SINGLEPASS_MODULATE //disable setting single-pass drv flags
//#define DEBUG_MAX_TEXTURE_STAGES 2  //clamp max txt stages to this

                              //simulate low of video memory, the number specifies bytes
                              // to grab when initializing
                              // (undef to disable)
//#define DEBUG_SIM_LOWVIDMEM (20*1024*1024)
//#define DEBUG_DISABLE_BUMPMAPPING  //disable using of bump texture formats

//#define DEBUG_FORCE_DIRECT_TRANSFORM   //direct transfrormations, force inclusion of DRVF2_DIRECT_TRANSFORM

//#define DEBUG_NO_NVLINK_OPTIM //disable shader optimizations of VN Linker

//#define DEBUG_NO_MODULATE2X   //disable support for MODULATE2X
//#define DEBUG_NO_REAL_CLIP_PLANES   //do not use clip planes, only try to use clip textures
#define DEBUG_DUMP_SHADERS
//#define DEBUG_DISABLE_ANISOTROPY //no anisotropic filtering

#endif

//----------------------------

#ifdef DEBUG_SIM_LOWVIDMEM
static C_vector<C_smart_ptr<IImage> > low_mem_textures;
#endif

//----------------------------
//----------------------------
static dword num_alloc_bytes;

/*
void *__cdecl operator new(size_t sz){
   void *vp = malloc(sz);
   if(vp) num_alloc_bytes += _msize(vp);
   return vp;
}
void *operator new[](size_t sz){ return operator new(sz);}
void *operator new[](size_t sz, void*vp){ return vp;}

void __cdecl operator delete(void *vp){
   if(vp) num_alloc_bytes -= _msize(vp);
   free(vp);
}
void operator delete[](void *vp){ operator delete(vp); }
*/

//----------------------------
                              //plugin system - visual creation
I3D_visual *CreateAtmospheric(PI3D_driver drv);
I3D_visual *CreateBillboard(PI3D_driver drv);
I3D_visual *CreateCameraView(PI3D_driver drv);
I3D_visual *CreateDischarge(PI3D_driver drv);
I3D_visual *CreateFlare(PI3D_driver drv);
I3D_visual *CreateLitObject(PI3D_driver drv);
I3D_visual *CreateMorph(PI3D_driver drv);
I3D_visual *CreateObject(PI3D_driver drv);
I3D_visual *CreateObjectUVshift(PI3D_driver drv);
I3D_visual *CreateParticle(PI3D_driver drv);
I3D_visual *CreateSingleMesh(PI3D_driver drv);
I3D_visual *CreateDynamic(PI3D_driver drv);

extern const S_visual_property props_Atmospheric[],
   props_Billboard[],
   props_Discharge[],
   props_Flare[],
   props_LitObject[],
   props_Morph[],
   props_ObjectUVshift[],
   props_Particle[],
   props_CameraView[];

const S_visual_plugin_entry visual_plugins[] = {
   { &CreateAtmospheric, "Atmospheric", I3D_VISUAL_ATMOS, props_Atmospheric},
   { &CreateBillboard, "Billboard", I3D_VISUAL_BILLBOARD, props_Billboard},
   { &CreateCameraView, "Camera View", I3D_VISUAL_CAMVIEW, props_CameraView},
   { &CreateDischarge, "Discharge", I3D_VISUAL_DISCHARGE, props_Discharge},
   { &CreateFlare, "Flare", I3D_VISUAL_FLARE, props_Flare},
   { &CreateLitObject, "Lit-object", I3D_VISUAL_LIT_OBJECT, props_LitObject},
   { &CreateMorph, "Morph", I3D_VISUAL_MORPH, props_Morph},
   { &CreateObject, "Object", I3D_VISUAL_OBJECT },
   { &CreateSingleMesh, "SingleMesh", I3D_VISUAL_SINGLEMESH },
   { &CreateDynamic, "Dynamic", I3D_VISUAL_DYNAMIC },
   { &CreateObjectUVshift, "Object (UV shift)", I3D_VISUAL_UV_SHIFT, props_ObjectUVshift},
   { &CreateParticle, "Particle", I3D_VISUAL_PARTICLE, props_Particle},
};
const int num_visual_plugins = sizeof(visual_plugins)/sizeof(*visual_plugins);

//----------------------------
//----------------------------

#ifdef USE_OWN_FRAME_ALLOCATION

static class C_mem_alloc_manager{
   class C_heap{
      byte *base;
      dword heap_size;

      dword max_free_block;

      struct S_alloc_block{
         enum E_TYPE{
            FREE,
            USED
         } type;
         dword size;
         S_alloc_block(E_TYPE t, dword sz):
            type(t), size(sz)
         {}
      };
                              //list of blocks
      list<S_alloc_block> blocks;
      typedef list<S_alloc_block>::iterator bl_iter;
   public:
      C_heap(dword hs = USE_OWN_FRAME_ALLOCATION * 1024):
         base(new byte[heap_size = hs]),
         max_free_block(hs)
      {
                              //create base allocation frame
         blocks.push_back(S_alloc_block(S_alloc_block::FREE, heap_size));
      }
      ~C_heap(){
         delete[] base;
      }
                              //copying - not implemented
      C_heap(const C_heap&);
      C_heap &operator =(const C_heap&);

      dword GetMaxFreeBlock() const{
         return max_free_block;
      }

      void *Allocate(dword size){
                              //search blocks, find free
         byte *p = base;
         dword max_free_bl = 0;
         for(bl_iter it = blocks.begin(); it!=blocks.end(); it++){
            dword block_size = (*it).size;
            if((*it).type == S_alloc_block::FREE){
               if(block_size >= size){
                              //use this block
                  (*it).type = S_alloc_block::USED;
                  dword rest_size = block_size - size;
                  if(rest_size >= 32){
                              //split block - create new after this
                     (*it).size = size;
                     ++it;
                     it = blocks.insert(it, S_alloc_block(S_alloc_block::FREE, rest_size));
                  }
                  if(max_free_block==block_size){
                              //find different max-sized block
                     for(; it!=blocks.end(); it++){
                        if((*it).type == S_alloc_block::FREE){
                           max_free_bl = Max(max_free_bl, (*it).size);
                        }
                     }
                     max_free_block = max_free_bl;
                  }
                  break;
               }
               max_free_bl = Max(max_free_bl, block_size);
            }
            p += block_size;
         }
         return p;
      }

      bool Free(void *vp){
                              //fast test if it's in our area
         byte *bp = (byte*)vp;
         if(bp < base || bp >= (base + heap_size))
            return false;

                              //probably ours pointer, try to delete it
         byte *p = base;
         for(bl_iter it = blocks.begin(); it!=blocks.end(); it++){
            dword block_size = (*it).size;
            if(p==bp){
               assert((*it).type == S_alloc_block::USED);
                              //this is ours block, free it and concanetate with neighbours
               (*it).type = S_alloc_block::FREE;
               bl_iter bl_next = it;
               ++bl_next;
               if(bl_next!=blocks.end() && (*bl_next).type==S_alloc_block::FREE){
                              //join with next block
                  (*it).size += (*bl_next).size;
                  blocks.erase(bl_next);
               }
               if(it!=blocks.begin()){
                  bl_iter bl_prev = it;
                  --bl_prev;
                  if((*bl_prev).type==S_alloc_block::FREE){
                              //join with next block
                     (*bl_prev).size += (*it).size;
                     blocks.erase(it);
                     it = bl_prev;
                  }
               }
               max_free_block = Max(max_free_block, (*it).size);
               break;
            }
            p += block_size;
         }
         assert(it!=blocks.end());
         return true;
      }

      bool IsEmpty() const{
         return (blocks.size()==1);
      }
   };

   list<C_heap*> all_heaps;
   typedef list<C_heap*>::iterator heap_iter;
public:
   ~C_mem_alloc_manager(){
                              //delete all currently allocated heaps,
                              // although we should finish with empty list
      assert(!all_heaps.size());
      for(heap_iter it=all_heaps.begin(); it!=all_heaps.end(); it++){
         delete (*it);
      }
   }

   void *Allocate(dword size){
                              //search free space in all heaps
      for(heap_iter it=all_heaps.begin(); it!=all_heaps.end(); it++){
         if((*it)->GetMaxFreeBlock() >= size)
            break;
      }
      if(it==all_heaps.end()){
                              //create new heap
                              // push to front, so that we search fresh heaps first
         all_heaps.push_front(new C_heap);
         it = all_heaps.begin();
      }
      void *p = (*it)->Allocate(size);
      assert(p);
      return p;
   }

   void Free(void *p){

      for(heap_iter it=all_heaps.begin(); it!=all_heaps.end(); it++){
         if((*it)->Free(p)){
                              //erase empty heaps
            if((*it)->IsEmpty()){
               delete (*it);
               all_heaps.erase(it);
            }
            return;
         }
      }
                              //pointer not found in our heaps???
      assert(0);
   }

} mem_alloc_manager;
#endif

//----------------------------

/*
void *I3D_visual::operator new(size_t sz){
#ifdef USE_OWN_FRAME_ALLOCATION
   return mem_alloc_manager.Allocate(sz);
#else
   return ::operator new(sz);
#endif
}
void I3D_visual::operator delete(void *p){
#ifdef USE_OWN_FRAME_ALLOCATION
   mem_alloc_manager.Free(p);
#else
   ::operator delete(p);
#endif
}

//----------------------------

void *I3D_model::operator new(size_t sz){
#ifdef USE_OWN_FRAME_ALLOCATION
   return mem_alloc_manager.Allocate(sz);
#else
   return ::operator new(sz);
#endif
}
void I3D_model::operator delete(void *p){
#ifdef USE_OWN_FRAME_ALLOCATION
   mem_alloc_manager.Free(p);
#else
   ::operator delete(p);
#endif
}
*/

//----------------------------
//----------------------------
#ifdef _DEBUG
static PI3D_driver debug_drv;
#endif
//----------------------------

I3D_RESULT I3DAPI I3DCreate(PI3D_driver *drvp, CPI3DINIT id){

                              //create interface
   PI3D_driver drv = new I3D_driver();

   if(!drv)
      return I3DERR_OUTOFMEM;
                              //initialize now
   I3D_RESULT ir = drv->Init(id);
   if(I3D_FAIL(ir)){
      drv->Release();
   }else{
      *drvp = drv;
   }
   return ir;
}

//----------------------------
//----------------------------

inline dword Rotate(dword dw, int num_bits){
   if(num_bits>=0) return dw<<num_bits;
   else return dw>>(-num_bits);
}

//----------------------------

static const char *inteface_name[I3D_CLID_LAST] = {
   "I3D_driver",
   "I3D_scene",
   "I3D_frame",
   "I3D_dummy",
   "I3D_visual",
   "I3D_model",
   "I3D_light",
   "I3D_sound",
   "I3D_sector",
   "I3D_camera",
   "",
   "I3D_volume",
   "I3D_user",
   "I3D_bone",
   "I3D_occluder",
   "I3D_singlemesh",

   "I3D_texture",
   "I3D_material",
   "I3D_mesh_base",
   "I3D_mesh",
   "I3D_portal",
   "I3D_animation_base",
   "I3D_animation_set",
   "I3D_interpolator",
   "I3D_anim_interpolator",
   "I3D_procedural",
};

//----------------------------
//----------------------------
/*
static char copyright_info[] =
   " ***   Copyright (c) 2000 Lonely Cat Games, s.r.o.. All rights reserved. "
   "This library is part of Insanity graphics engine. ***"
;
*/
//----------------------------
//----------------------------

static const D3DFORMAT supported_texture_formats[] = {
   D3DFMT_R8G8B8,
   D3DFMT_A8R8G8B8,
   D3DFMT_X8R8G8B8,
   D3DFMT_R5G6B5,
   D3DFMT_X1R5G5B5,
   D3DFMT_A1R5G5B5,
   D3DFMT_A4R4G4B4,
   D3DFMT_R3G3B2,
   D3DFMT_A8R3G3B2,
   D3DFMT_X4R4G4B4,
   //D3DFMT_P8,
                              //compressed
   D3DFMT_DXT1,
   D3DFMT_DXT2,
   D3DFMT_DXT3,
   D3DFMT_DXT4,
   D3DFMT_DXT5,
                              //bump
                              // note: we don't support bump formats with more
                              // than 8 bits per element
   D3DFMT_V8U8,
   D3DFMT_L6V5U5,
   D3DFMT_X8L8V8U8,
   //D3DFMT_Q8W8V8U8,
}, supported_zsten_formats[] = {
   D3DFMT_D16,
   D3DFMT_D32,
   D3DFMT_D24S8,
   D3DFMT_D24X4S4,
   D3DFMT_D15S1,
   D3DFMT_D24X8,
   //D3DFMT_D16_LOCKABLE,
};

//----------------------------

enum{
   AF_ALPHA = 1,
   AF_PREMULT = 2,
   AF_INTERPOLATED = 4,
};

I3D_RESULT I3D_driver::FindTextureFormat(S_pixelformat &pf, dword flags) const{

   if(flags&FINDTF_ALPHA1)
      flags &= ~FINDTF_ALPHA;
#ifndef GL
                              //can't support paletized alpha-textures
   if(flags&FINDTF_ALPHA)
      flags &= ~FINDTF_PALETIZED;
#endif
   const S_pixelformat *best_fmt = NULL;

   const C_vector<S_pixelformat> &fmts = texture_formats[!(flags&FINDTF_RENDERTARGET) ? 0 : 1];
   for(dword i=0; i<fmts.size(); i++){
      const S_pixelformat &new_fmt = fmts[i];

      if(flags&FINDTF_EMBMMAP){
                              //
         if(new_fmt.flags&PIXELFORMAT_BUMPMAP){
            //if(i==15) break;  //debug - force using specified texture format
            if(!best_fmt){
               best_fmt = &new_fmt;
            }else{
               if(flags&FINDTF_ALPHA){
                  if(!new_fmt.l_mask)
                     continue;
                  if(!best_fmt->l_mask){
                     best_fmt = &new_fmt;
                     continue;
                  }
               }
               {
                  int orig_bpp = best_fmt->bytes_per_pixel;
                  int new_bpp = new_fmt.bytes_per_pixel;
                  int orig_uv_depth = CountBits(best_fmt->u_mask) + CountBits(best_fmt->v_mask);
                  int new_uv_depth = CountBits(new_fmt.u_mask) + CountBits(new_fmt.v_mask);
                  if(flags&FINDTF_ALPHA){
                     orig_uv_depth += CountBits(best_fmt->l_mask);
                     new_uv_depth += CountBits(new_fmt.l_mask);
                  }
                  if(orig_bpp == new_bpp){
                     if(orig_uv_depth < new_uv_depth)
                        best_fmt = &new_fmt;
                  }else
                  switch(orig_bpp){
                  case 2:
                     switch(new_bpp){
                     case 3: case 4:
                        if((flags&FINDTF_TRUECOLOR) && (orig_uv_depth < new_uv_depth))
                           best_fmt = &new_fmt;
                        break;
                     }
                     break;
                  case 4:
                     if(!(flags&FINDTF_TRUECOLOR)){
                              //trade for smaller bitdepth
                        if(new_bpp<4 && new_bpp>=2)
                           best_fmt = &new_fmt;
                     }else
                     if(orig_uv_depth==new_uv_depth && new_bpp<orig_bpp){
                        best_fmt = &new_fmt;
                     }
                     break;
                  }
               }
            }
         }
         continue;
      }
#ifndef USE_PALETIZED_TEXTURES
      if(new_fmt.flags&PIXELFORMAT_PALETTE)
         continue;
#endif
                              //skip non-RGB formats
      if(new_fmt.flags&PIXELFORMAT_BUMPMAP)
         continue;

      if(new_fmt.flags&PIXELFORMAT_COMPRESS){
#ifndef GL
                              //that's compressed format
         if(flags&FINDTF_COMPRESSED){
            //if(i==6) break;  //debug - force using specified texture format
            struct S_hlp{
               static bool GetAlphaFlags(dword four_cc, dword &ret){
                  ret = AF_ALPHA;
                  switch(four_cc){
                  case MAKEFOURCC('D', 'X', 'T', '3'): break;
                  case MAKEFOURCC('D', 'X', 'T', '1'): ret = 0; break;
                  case MAKEFOURCC('D', 'X', 'T', '2'): ret |= AF_PREMULT; break;
                  case MAKEFOURCC('D', 'X', 'T', '4'): ret |= AF_PREMULT;
                              //flow...
                  case MAKEFOURCC('D', 'X', 'T', '5'): ret |= AF_INTERPOLATED; break;
                  default:    //unknown
                     return false;
                  }
                  return true;
               }
            };

            dword new_flags;
            if(!S_hlp::GetAlphaFlags(new_fmt.four_cc, new_flags))
               continue;
            if(new_flags&(AF_PREMULT|AF_INTERPOLATED))
               continue;

            if(!best_fmt || !(best_fmt->flags&PIXELFORMAT_COMPRESS))
               best_fmt = &new_fmt;
            else{
               dword curr_flags;
               S_hlp::GetAlphaFlags(best_fmt->four_cc, curr_flags);
               bool take = false;
                           //strategy:
                           // prefer non-premultiplied explicit alpha
                           // for alpha-textures choose formats fith alpha bits
                           // for non-alpha or alpha1 textures choose color-key format
               if(flags&FINDTF_ALPHA){
                  if(new_flags&AF_ALPHA){
                     if(!(curr_flags&AF_ALPHA)) take = true;
                     else
                     if(!(new_flags&AF_PREMULT) && (curr_flags&AF_PREMULT)) take = true;
                     else
                     if(!(new_flags&AF_INTERPOLATED) && (curr_flags&AF_INTERPOLATED)) take = true;
                  }
               }else{
                  if(!(new_flags&AF_ALPHA)) take = true;
                  else
                  if(!(new_flags&AF_PREMULT) && (curr_flags&AF_PREMULT)) take = true;
                  else
                  if(!(new_flags&AF_INTERPOLATED) && (curr_flags&AF_INTERPOLATED)) take = true;
               }
               if(take)
                  best_fmt = &new_fmt;
            }
         }
#endif
      }else{
#if 0
         if(i==19){
            best_fmt = &new_fmt;
            break;  //debug - force using specified texture format
         }
#endif
#ifndef GL
         if(flags&FINDTF_COMPRESSED){
                              //if we have already compressed format, skip
            if(best_fmt && (best_fmt->flags&PIXELFORMAT_COMPRESS))
               continue;
         }
#endif
         if(flags&FINDTF_ALPHA){
            if(new_fmt.flags&PIXELFORMAT_ALPHA){
               if(!best_fmt || !(best_fmt->flags&PIXELFORMAT_ALPHA)){
                  best_fmt = &new_fmt;
               }else{
                  int orig_depth = CountBits(*best_fmt);
                  int new_depth = CountBits(new_fmt);
                  if(orig_depth==new_depth){
                                 //keep all masks sizes ballanced
                     if(abs(CountBits(new_fmt.r_mask)-CountBits(new_fmt.a_mask)) <
                        abs(CountBits(best_fmt->r_mask)-CountBits(best_fmt->a_mask))){
                        best_fmt = &new_fmt;
                     }
                  }else
                  switch(orig_depth){
                  case 8:        //don't know such format, use the other
                  default:
                     best_fmt = &new_fmt;
                     break;
                  case 16:
                     switch(new_depth){
                     case 24: case 32:
                                 //support higher depths only if alpha insufficient
                        if((flags&FINDTF_TRUECOLOR) || (CountBits(best_fmt->a_mask)<3)) 
                           best_fmt = &new_fmt;
                        break;
                     }
                     break;
                  case 24:
                     if(!(flags&FINDTF_TRUECOLOR))
                     switch(new_depth){
                     case 16:    //check aplha sufficiency
                        if(CountBits(new_fmt.a_mask)>=3)
                           best_fmt = &new_fmt;
                        break;
                     }
                     break;
                  case 32:
                     if(!(flags&FINDTF_TRUECOLOR))
                     switch(new_depth){
                     case 16:
                        if(CountBits(new_fmt.a_mask) >=3 )
                           best_fmt = &new_fmt;
                        break;
                     case 24:
                        best_fmt = &new_fmt;
                        break;
                     }
                     break;
                  }
               }
            }
            if(best_fmt && (best_fmt->flags&PIXELFORMAT_ALPHA))
               continue;
                                 //alpha not found yet, try non-alpha
         }
#ifndef GL
         if(flags&FINDTF_PALETIZED){
            if(!best_fmt){
                              //1st found
               best_fmt = &new_fmt;
            }else
            if(new_fmt.flags&PIXELFORMAT_PALETTE){
               if((flags&FINDTF_ALPHA1) && CountBits(best_fmt->a_mask)==1){
                                 //we've got alpha1 format already
               }else{
                                 //paletized found
                  best_fmt = &new_fmt;
               }
            }else{
                                 //paletized requested, non-pal found
                                 //check RGB bit-count, support
                                 //16-bit, 8-bit, 24-bit, 32-bit
               int orig_depth = CountBits(*best_fmt);
               int new_depth = CountBits(new_fmt);
               if(orig_depth==new_depth){
                  if((flags&FINDTF_ALPHA1) && CountBits(new_fmt.a_mask) ==1 ){
                                 //get alpha1 format
                     best_fmt = &new_fmt;
                  }else          //don't drop alpha1 format
                  if(!((flags&FINDTF_ALPHA1) && CountBits(best_fmt->a_mask)==1)){
                     if(orig_depth==8){
                                 //prefer paletized (new is not)

                     }else       //check for wider resolution
                     if((CountBits(new_fmt.r_mask) + CountBits(new_fmt.g_mask) + CountBits(new_fmt.b_mask)) >
                        (CountBits(best_fmt->r_mask) + CountBits(best_fmt->r_mask) + CountBits(best_fmt->r_mask))){

                        best_fmt = &new_fmt;
                     }
                  }
               }else
               switch(orig_depth){
               case 8:           //RGB format
               default:
                                 //don't drop alpha1 format
                  if(!((flags&FINDTF_ALPHA1) && CountBits(best_fmt->a_mask)==1)){
                                 //don't drop paletized format
                     if(!(best_fmt->flags&PIXELFORMAT_PALETTE))
                     switch(new_depth){
                     case 16:
                        best_fmt = &new_fmt;
                        break;
                     }
                  }
                  break;
               case 24:
                  switch(new_depth){
                  case 8:
                  case 16:
                     best_fmt = &new_fmt;
                     break;
                  }
                  break;
               case 32:
                  switch(new_depth){
                  case 8:
                  case 16:
                  case 24:
                     best_fmt = &new_fmt;
                     break;
                  }
                  break;
               }
            }
         }else
#endif
         {
            if(!best_fmt){
                                 //any is good
               best_fmt = &new_fmt;
            }else
                                 //non-paletized requested
            if(!(new_fmt.flags&PIXELFORMAT_PALETTE)){
                                 //check RGB bit-count, support
                                 //16-bit, 8-bit, 24-bit, 32-bit
               int orig_depth = CountBits(*best_fmt);
               int new_depth = CountBits(new_fmt);
               if(orig_depth==new_depth){
                  if((flags&FINDTF_ALPHA1) && CountBits(new_fmt.a_mask)==1){
                     best_fmt = &new_fmt;
                  }else          //don't drop alpha1 format
                  if(!((flags&FINDTF_ALPHA1) && CountBits(best_fmt->a_mask)==1)){
                                 //check for wider resolution
                     if((CountBits(new_fmt.r_mask) + CountBits(new_fmt.g_mask) + CountBits(new_fmt.b_mask)) >
                        (CountBits(best_fmt->r_mask) + CountBits(best_fmt->r_mask) + CountBits(best_fmt->r_mask))){
                        best_fmt = &new_fmt;
                     }else    //drop alpha format, if not requested
                     if((best_fmt->flags&PIXELFORMAT_ALPHA) && !(new_fmt.flags&PIXELFORMAT_ALPHA)){
                        best_fmt = &new_fmt;
                     }
                  }
               }else
                                 //don't drop alpha1 format
               if(!((flags&FINDTF_ALPHA1) && CountBits(new_fmt.a_mask)==1))
               switch(orig_depth){
               case 8:           //RGB format
               default:
                  switch(new_depth){
                  case 24: case 16: 
                     best_fmt = &new_fmt;
                     break;
                  case 8:        //take this, as it yelds same results (3-3-2)
                  case 32:
                     if(best_fmt->flags&PIXELFORMAT_PALETTE)
                        best_fmt = &new_fmt;
                     break;
                  }
                  break;
               case 16:
                  switch(new_depth){
                  case 24: case 32:
                     if(flags&FINDTF_TRUECOLOR)
                        best_fmt = &new_fmt;
                     break;
                  }
                  break;
               case 24:
                  switch(new_depth){
                  case 16: 
                     if(!(flags&FINDTF_TRUECOLOR))
                        best_fmt = &new_fmt;
                     break;
                  }
                  break;
               case 32:
                  switch(new_depth){
                  case 16:
                     if(!(flags&FINDTF_TRUECOLOR))
                        best_fmt = &new_fmt;
                     break;
                  case 24: 
                     best_fmt = &new_fmt;
                     break;
                  }
                  break;
               }
            }
         }
      }
   }
   if(!best_fmt)
      return I3DERR_TEXTURESNOTSUPPORTED;
   pf = *best_fmt;
   return I3D_OK;
}

//----------------------------

D3DFORMAT I3D_driver::FindZStenFormat(const S_pixelformat &pf) const{

                              //find which format it is
   const C_vector<S_pixelformat> &fmts = texture_formats[1];
   for(int i=fmts.size(); i--; ){
      if(!memcmp(&fmts[i], &pf, sizeof(S_pixelformat)))
         break;
   }
   assert(i!=-1);
   return rtt_zbuf_fmts[i].front();
}

//----------------------------
//----------------------------

#ifdef USE_PREFETCH
static __declspec(naked) void Prefetch3DNow(const void *ptr){
                              //opcode: 0x0f, 0xd, W(&0x8) | reg(&0x7)
   __asm{
      __emit 0x0f
      __emit 0x0d
      __emit 0x01 
      ret
   }
}
static __declspec(naked) void Prefetch3DNow2(const void*, const void*){
   __asm{
      __emit 0x0f
      __emit 0x0d
      __emit 0x01 
      __emit 0x0f
      __emit 0x0d
      __emit 0x02
      ret
   }
}
static __declspec(naked) void Prefetch3DNow64(const void*){
   __asm{
      __emit 0x0f
      __emit 0x0d
      __emit 0x01
      __emit 0x0f
      __emit 0x0d
      __emit 0x41 
      __emit 0x20
      ret
   }
}

//----------------------------

static __declspec(naked) void PrefetchSIMD(const void *ptr){
                              //opcode: 0x0f, 0x18, type(&0x18) | reg(&0x7)
                              // type: x00 = prefetchnta
                              //       x08 = prefetcht0
                              //       x10 = prefetcht1
                              //       x18 = prefetcht2
   __asm{
      __emit 0x0f
      __emit 0x18
      __emit 0x09
      ret
   }
}
static __declspec(naked) void PrefetchSIMD2(const void*, const void*){
   __asm{
      __emit 0x0f
      __emit 0x18
      __emit 0x09
      __emit 0x0f
      __emit 0x18
      __emit 0x0a
      ret
   }
}
static __declspec(naked) void PrefetchSIMD64(const void*){
   __asm{
      __emit 0x0f
      __emit 0x18
      __emit 0x09
      __emit 0x0f
      __emit 0x18
      __emit 0x49
      __emit 0x20
      ret
   }
}

//----------------------------

static __declspec(naked) void PrefetchNop(const void *ptr){
   __asm ret
}

t_Prefetch1 *Prefetch1 = PrefetchNop;
t_Prefetch2 *Prefetch2ptr = (t_Prefetch2*)PrefetchNop;
t_Prefetch64 *Prefetch64Bytes = (t_Prefetch64*)PrefetchNop;

//t_SphereInFrustum SphereInFrustum_x86, SphereInFrustum_3DNow, SphereInFrustum_SSE, *SphereInFrustum = SphereInFrustum_x86;

struct S_CPU_init{
   S_CPU_init(){
                              //detect CPU type
      if(GetCPUCaps(HAS_3DNOW)){
         //MessageBox(NULL, "3DNow! detected!", "", MB_OK);
         Prefetch1 = Prefetch3DNow;
         Prefetch2ptr = Prefetch3DNow2;
         Prefetch64Bytes = Prefetch3DNow64;
         if(GetCPUCaps(HAS_MMX)){
            //SphereInFrustum = SphereInFrustum_3DNow;
         }
      }else
      if(GetCPUCaps(HAS_SIMD)){
         //MessageBox(NULL, "SIMD detected!", "", MB_OK);
         Prefetch1 = PrefetchSIMD;
         Prefetch2ptr = PrefetchSIMD2;
         Prefetch64Bytes = PrefetchSIMD64;
         //SphereInFrustum = SphereInFrustum_SSE;
      }
   }
} CPU_init;

#endif//USE_PREFETCH

//----------------------------
//----------------------------

I3D_driver::I3D_driver():
   ref(1),
   lod_scale(1.0f),
   global_sound_volume(1.0f),
   debug_draw_mats(false),
   pD3D_driver(NULL),
#ifndef GL
   is_hal(false),
#endif
   in_scene_count(0),
   texture_sort_id(0),
   force_lod_index(-1),
   drv_flags(0),
   drv_flags2(0),
   cb_context(NULL),
   cb_proc(NULL),
   rgbconv_lm(NULL),
   lm_create_aa_ratio(1),
   hi_res(NULL),

                              //cache
   srcblend(D3DBLEND_ONE),
   dstblend(D3DBLEND_ZERO),
#ifdef GL
   gl_srcblend(GL_ONE), gl_dstblend(GL_ZERO),
#endif
   alpha_blend_on(false),
   render_time(0),
                              //cache
   //last_render_mat(NULL),
   b_cullnone(false),
   cull_default(D3DCULL_CCW),
   last_blend_flags(I3DBLEND_OPAQUE),
   last_stream0_vb(NULL),
   last_stream0_stride(0),
   last_ib(NULL),
   shadow_range_n(20.0f),
   shadow_range_f(30.0f),
   curr_anisotropy(0xffffffff)
{
   memset(&d3d_caps, 0, sizeof(d3d_caps));
   memset(&init_data, 0, sizeof(init_data));

   memset(last_dir_index, 0, sizeof(last_dir_index));
   memset(stage_anisotropy, 0, sizeof(stage_anisotropy));
   stage_txt_op[0] = D3DTOP_MODULATE;
   for(int i=1; i<MAX_TEXTURE_STAGES; i++){
      stage_txt_op[i] = D3DTOP_DISABLE;
   }

#ifdef _DEBUG
   memset(debug_int, 0, sizeof(debug_int));
   memset(debug_float, 0, sizeof(debug_float));
#else
   memset(_debug_int, 0, sizeof(_debug_int));
   memset(_debug_float, 0, sizeof(_debug_float));
#endif
   memset(interface_count, 0, sizeof(interface_count));
   memset(last_texture, 0, sizeof(last_texture));
}

//----------------------------

I3D_driver::~I3D_driver(){

   Close();
   if(hi_res)
      FreeLibrary(hi_res);
}

//----------------------------

HINSTANCE I3D_driver::GetHInstance() const{
   if(!hi_res)
      hi_res = LoadLibrary("IEditor.dll");
   return hi_res;
}

//----------------------------

#ifdef OVERRIDE_F_TO_L
extern "C" long __cdecl _ftol (float);

#pragma warning(disable:4035)
__declspec(naked) long __cdecl _ftol (float f){
   __asm{
      push eax
      fistp dword ptr [esp]
      pop eax
      ret
   }
}
#pragma warning(disable:4035)
#endif   //OVERRIDE_F_TO_L

//----------------------------

#if defined _DEBUG && 0
#pragma comment(lib, "cgd3d.lib")
#pragma warning(push,1)
#include "cg\cgd3d.h"
#pragma warning(pop)
#endif

void SetBreakAlloc(dword);

//----------------------------
#ifdef GL

//----------------------------

bool C_gl_shader::Build(E_TYPE type, const char *src, dword len){
   gl_id = glCreateShader(type==TYPE_VERTEX ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER); CHECK_GL_RESULT("glCreateShader");
   if(gl_id){
      glShaderSource(gl_id, 1, &src, len ? (int*)&len : NULL); CHECK_GL_RESULT("glShaderSource");
      glCompileShader(gl_id); CHECK_GL_RESULT("glCompileShader");
      GLint compiled = 0;
      glGetShaderiv(gl_id, GL_COMPILE_STATUS, &compiled);
      if(!compiled){
         GLint infoLen = 0;
         glGetShaderiv(gl_id, GL_INFO_LOG_LENGTH, &infoLen); CHECK_GL_RESULT("glGetShaderiv");
         if(infoLen){
            char *buf = new char[infoLen+1];
            if(buf){
               glGetShaderInfoLog(gl_id, infoLen, NULL, buf);
               buf[infoLen] = 0;
               OutputDebugString(buf);
               delete[] buf;
               assert(0);
            }
         }
         glDeleteShader(gl_id); CHECK_GL_RESULT("glDeleteShader");
         gl_id = 0;
      }
   }
   return (gl_id!=0);
}

//----------------------------

bool C_gl_program::Build(const char *v_shader, const char *f_shader, const char *attr_names, dword v_len, dword f_len){
   if(!shd_vertex.Build(C_gl_shader::TYPE_VERTEX, v_shader, v_len))
      return false;
   if(!shd_fragment.Build(C_gl_shader::TYPE_FRAGMENT, f_shader, f_len))
      return false;
   gl_prg_id = glCreateProgram(); CHECK_GL_RESULT("glCreateProgram");
   if(gl_prg_id){
      glAttachShader(gl_prg_id, shd_fragment); CHECK_GL_RESULT("glAttachShader");
      glAttachShader(gl_prg_id, shd_vertex); CHECK_GL_RESULT("glAttachShader");
      glLinkProgram(gl_prg_id); CHECK_GL_RESULT("glLinkProgram");
      GLint linkStatus = GL_FALSE;
      glGetProgramiv(gl_prg_id, GL_LINK_STATUS, &linkStatus);
      if(linkStatus != GL_TRUE){
         GLint bufLength = 0;
         glGetProgramiv(gl_prg_id, GL_INFO_LOG_LENGTH, &bufLength);
         if(bufLength) {
            char *buf = new char[bufLength+1];
            if(buf){
               glGetProgramInfoLog(gl_prg_id, bufLength, NULL, buf);
               buf[bufLength] = 0;
               OutputDebugString(buf);
               delete[] buf;
               assert(0);
            }
         }
         glDeleteProgram(gl_prg_id);
         gl_prg_id = 0;
      }else{
                              //collect attributes id's
         dword indx = 0;
         while(*attr_names){
            int id = -1;
            switch(*attr_names){
            case 'u': id = glGetUniformLocation(gl_prg_id, attr_names); break;
            case 'a': id = glGetAttribLocation(gl_prg_id, attr_names); break;
            }
            CHECK_GL_RESULT("glGet*Location");
            //assert(id!=-1);
            BuildStoreAttribId(indx, id);
            attr_names += strlen(attr_names)+1;
            ++indx;
         }
      }
   }
   return (gl_prg_id!=0);
}

//----------------------------

C_gl_program::~C_gl_program(){
   if(gl_prg_id)
      glDeleteProgram(gl_prg_id);
}

//----------------------------

void C_gl_program::Use(){
   glUseProgram(gl_prg_id); CHECK_GL_RESULT("glUseProgram");
}

//----------------------------

#endif
//----------------------------

I3D_RESULT I3D_driver::Init(CPI3DINIT isp){

   //SetBreakAlloc(4129);
   I3D_RESULT ir;

   if(!isp){
      assert(0);
      ir = I3DERR_INVALIDPARAMS;
      goto out;
   }
#ifdef GL
   glCullFace(GL_FRONT);
   glEnable(GL_CULL_FACE);
#endif
   {
      h_nvlinker = LoadLibrary("nvlinker.dll");
      if(!h_nvlinker){
         DEBUG_LOG("Cannot load nvlinker.dll");
         assert(0);
         ir = I3DERR_GENERIC;
         goto out;
      }
      LPCREATELINKER pCreate = (LPCREATELINKER)GetProcAddress(h_nvlinker, "CreateLinker");
      if(!pCreate){
         DEBUG_LOG("CreateLinker failed");
         assert(0);
         ir = I3DERR_GENERIC;
         goto out;
      }
      eOPTIMIZERLEVEL nv_create_flags = OPTIMIZERLEVEL_BASIC;
#ifdef DEBUG_NO_NVLINK_OPTIM
      nv_create_flags = OPTIMIZERLEVEL_NONE;
#endif
                              //create 2 linkers, for both vertex and pixel shaders
      for(int i=0; i<2; i++){
         nv_linker[i] = (*pCreate)(0, i==0 ? nv_create_flags : OPTIMIZERLEVEL_NONE);
         nv_linker[i]->ReserveConstantRange(0, VSC_FIXED_LAST - 1);
      }
                              //find and open shader file
      HRESULT hr;
      bool ok = false;
      {
         {
            HMODULE hi = GetModuleHandle(NULL);
            char fn[MAX_PATH];
            GetModuleFileName(hi, fn, sizeof(fn));
            for(int i=strlen(fn); i--; ){
               if(fn[i]=='\\'){
                  ++i;
                  break;
               }
            }
            for(int j=2; j--; ){
               strcpy(fn+i, !j ? "vertex.shd" : "pixel.shd");
               PC_dta_stream dta = DtaCreateStream(fn);
               if(!dta)
                  break;
               int sz = dta->GetSize();
               char *buf = new char[sz];
               dta->Read(buf, sz);
               dta->Release();

               hr = nv_linker[j]->AddFragments(buf, sz);
               delete[] buf;
               CHECK_D3D_RESULT("AddFragments", hr);
               if(FAILED(hr))
                  break;
            }
            ok = (j==-1);
         }
//#endif
      }
      if(!ok){
         assert(0);
         ir = I3DERR_GENERIC;
         goto out;
      }
      static const char *vs_fragment_names[VSF_LAST] = {
         "transform",
         "mul_transform",
         "transform_matrix_palette",
         "transform_matrix_palette_pos_only",
         "transform_weighted_morph1_os",
         "transform_weighted_morph1_ws",
         "transform_weighted_morph2_os",
         "transform_weighted_morph2_ws",
         "transform_discharge",
         "transform_particle",
         "test",
         "pick_uv_0",
         "pick_uv_1",
         "pick_uv_2",
         "pick_uv_3",
         "store_uv_0",
         "store_uv_1",
         "store_uv_2",
         "store_uv_3",
         "store_uvw_0",
         "store_uvw_1",
         "store_uvw_2",
         "store_uvw_3",
         "generate_envmap_uv_os",
         "generate_envmap_uv_ws",
         "generate_envmap_cube_uv_os",
         "generate_envmap_cube_uv_ws",
         "shift_uv",
         "multiply_uv_by_xy",
         "multiply_uv_by_z",
         "transform_uv0",
         "texture_project",
         "make_rect_uv",
         "fog_simple_os",
         "fog_simple_ws",
         "fog_begin",
         "fog_end",
         "fog_height",
         "fog_height_ws",
         "fog_layered_os",
         "fog_layered_ws",
         "fog_point_os",
         "fog_point_ws",
         "light_begin",
         "light_end",
         "light_end_alpha",
         "specular_begin",
         "specular_end",
         "light_directional_os",
         "light_directional_ws",
         //NULL,
         "light_point_os",
         "light_point_ws",
         "light_spot_os",
         "light_spot_ws",
         //NULL,
         "light_point_ambient_os",
         "light_point_ambient_ws",
         "diffuse_copy",
         "diffuse_mul_alpha",
         "copy_alpha",
         "simple_dir_light",
         "texkill_project_os",
         "texkill_project_ws",
         "bump_os",
         "bump_ws",
      }, *constant_names[VSC_LAST] = {
         "c_cam_loc_pos",
         "c_light_dir_normal",
         "c_light_dir_diffuse",
         "c_light_point_pos_fr",
         "c_light_point_diffuse_rrd",
         "c_light_spot_dir_cos",
         "c_light_layered_plane",
         "c_light_layered_color_r",
         "c_light_layered_pos",
         "c_light_pfog_conetop_rcca",
         "c_light_pfog_conedir_mult",
         "c_light_pfog_color_power",
         "c_txt_clip",
         "c_uv_transform",
         "c_mat_frame",
         "c_uv_shift",
      };
      for(i=0; i<VSF_LAST; i++){
         if(!vs_fragment_names[i]) continue;   //allow NULL names (unused)
         hr = nv_linker[0]->GetFragmentID(vs_fragment_names[i], &vs_fragment_ids[i]);
         CHECK_D3D_RESULT(C_fstr("NVLink: can't find fragment: %s", vs_fragment_names[i]), hr);
         if(FAILED(hr))
            return false;
      }
      for(i=0; i<VSC_LAST; i++){
         hr = nv_linker[0]->GetConstantID(constant_names[i], &vs_constant_ids[i]);
         if(FAILED(hr))
            vs_constant_ids[i] = (dword)-1;
      }
//#ifdef _DEBUG

      static const char *ps_fragment_names[PSF_LAST] = {
         "tex_0",
         "tex_1",
         "tex_2",
         "tex_3",
         "tex_1_bem",
         "tex_2_bem",
         "tex_3_bem",
         "tex_1_beml",
         "tex_2_beml",
         "tex_3_beml",
         "texkill_0",
         "texkill_1",
         "texkill_2",
         "texkill_3",
         "ps_test",
         "v0_copy",
         "v0_mul2x",
         "color_copy",
         "mod_color_v0",
         "copy_black",
         "copy_black_t0a",
         "t0_copy",
         "t0_copy_inv",
         "t1_copy",
         "mod_t0_v0",
         "mod_r0_t1",
         "modx2_t0_v0",
         "modx2_r0_t0",
         "modx2_r0_t1",
         "modx2_r0_t2",
         "modx2_r0_t3",
         "modx2_t0_t1",
         "modx2_r0_r1",
         "modx2_t0_r1",
         "mod_t0_constcolor",
         "grayscale",
         "add_t0_v0",
         "add_t1_v0",
         "shadow_cast",
         "shadow_receive",
         "r0_b_2_a",
         "blend_by_alpha",
         "night_view",
      };

      for(i=0; i<PSF_LAST; i++){
         if(!ps_fragment_names[i]) continue;   //allow NULL names (unused)
         hr = nv_linker[1]->GetFragmentID(ps_fragment_names[i], &ps_fragment_ids[i]);
         CHECK_D3D_RESULT(C_fstr("NVLink: can't find fragment: %s", ps_fragment_names[i]), hr);
         if(FAILED(hr))
            return false;
      }
//#endif
   }

   drv_flags = 0;
   drv_flags2 = 0;

   pD3D_driver = isp->lp_graph->GetD3DInterface();
   pD3D_driver->AddRef();

   d3d_dev = isp->lp_graph->GetD3DDevice();
   d3d_dev->AddRef();

   isp->lp_graph->AddCallback(GraphCallback_thunk, this);

   init_data = (*isp);
                              //keep references to interfaces
   if(init_data.lp_sound) init_data.lp_sound->AddRef();
   if(init_data.lp_graph) init_data.lp_graph->AddRef();

   drv_flags2 |= DRVF2_LMTRUECOLOR;

   {
      static const char *decl_srcs[VSDECL_LAST] = {
         "dcl_position v0",   //VSDECL_POSITION
         "dcl_normal v2",     //VSDECL_NORMAL
         "dcl_color0 v2",     //VSDECL_DIFFUSE
         "dcl_color1 v5",     //VSDECL_SPECULAR
         "dcl_texcoord0 v1.xyzw",   //VSDECL_TEXCOORD01
         "dcl_texcoord2 v3.xyzw",   //VSDECL_TEXCOORD23
         "dcl_blendweight v4",//VSDECL_BLENDWEIGHT
         "dcl_normal1 v5",
         "dcl_normal2 v6",
         "dcl_normal3 v7",
      };
      for(int sv=0; sv<1; sv++){
         LPD3DXBUFFER buf_shd;
         for(dword i=0; i<VSDECL_LAST; i++){
            C_fstr src("%s\n%s\nmov oPos.xyzw,c0.xxxx\n", !sv ? "vs_1_1" : "vs_3_0", decl_srcs[i]);
            buf_shd = NULL;
            HRESULT hr = D3DXAssembleShader(src, src.Size(), NULL, NULL,
               0,//D3DXSHADER_SKIPVALIDATION,
               &buf_shd, NULL);
            CHECK_D3D_RESULT("D3DXCompileShader", hr);
            if(SUCCEEDED(hr)){
               dword sz;
               sz = buf_shd->GetBufferSize();
               assert(sz == (2 + 3 + 3) * 4);
               const dword *dwp = (dword*)buf_shd->GetBufferPointer();
               memcpy(&vsdecls[sv][i], dwp+1, sizeof(t_vsdecl));
            }
            if(buf_shd)
               buf_shd->Release();
         }
      }
   }

   ir = InitD3DResources();
   if(I3D_FAIL(ir))
      return ir;

                              //setup default state
   SetState(RS_LINEARFILTER, true);
   SetState(RS_USEZB, true);
   SetState(RS_CLEAR, true);
   SetState(RS_TEXTUREDITHER, true);
   SetState(RS_DRAWVISUALS, true);
   SetState(RS_DRAWMIRRORS, true);
   SetState(RS_DRAWTEXTURES, true);
   SetState(RS_MIPMAP, true);
   SetState(RS_FOG, true);
   SetState(RS_POINTFOG, true);
   SetState(RS_DITHER, true);
   SetState(RS_USELMAPPING, true);
   SetState(RS_ENVMAPPING, true);
   SetState(RS_USE_EMBM, true);
   SetState(RS_USE_OCCLUSION, true);
   SetState(RS_USESHADOWS, true);
   SetState(RS_LOADMIPMAP, true);
   SetState(RS_DETAILMAPPING, true);

                              //init default sound environment
   env_properties.insert(pair<int, S_sound_env_properties>(0, S_sound_env_properties()));

   ir = I3D_OK;
out:
   return ir;
}

//----------------------------
#ifdef DEBUG_VB_ALLOC


template<class T>
void C_RB_huge<T>::DebugOutput(){

   multiset<S_RB_block*, S_RB_block_less<S_RB_block*> >::iterator it1;
   for(it1 = alloc_map.begin(); it1 != alloc_map.end(); ++it1){

      S_RB_block::E_status st = (*it1)->status;
      dword idx = (*it1)->index;            //block begin, in items
      dword sz = (*it1)->size;             //block size

      C_fstr lmsg("id: %i, size: %i, state: %s", idx, sz, st==S_RB_block::S_FREE ? "free" : st==S_RB_block::S_ALLOC ? "alloc" : "uknown");
         MessageBox(NULL, (const char*)lmsg, "vb leak element", MB_OK);

   }
}
#endif

//----------------------------

void I3D_driver::Close(){

   if(!pD3D_driver)
      return;

   CloseD3DResources();
   managed_textures.clear();

   dbase.Close();

   int i;

   for(i=vs_decl_cache.size(); i--; ){
      delete vs_decl_cache[i];
   }
   vs_decl_cache.clear();

   last_stream0_vb = NULL;
   last_ib = NULL;
   last_LM_blend_img = NULL;
   last_LM_blend_img_name = NULL;

   if(d3d_dev){
      d3d_dev->Release();
      d3d_dev = NULL;
   }

   if(init_data.lp_graph)
      init_data.lp_graph->RemoveCallback(GraphCallback_thunk, this);

   pD3D_driver->Release();
   pD3D_driver = NULL;

   for(i=0; i<I3DDIR_LAST; i++)
      ClearDirs((I3D_DIRECTORY_TYPE)i);

   if(h_nvlinker){
      for(i=0; i<2; i++){
         if(nv_linker[i]){
            nv_linker[i]->Release();
            nv_linker[i] = NULL;
         }
      }
      FreeLibrary(h_nvlinker);
      h_nvlinker = NULL;
   }

   if(init_data.lp_sound) init_data.lp_sound->Release();
   if(init_data.lp_graph) init_data.lp_graph->Release();

#ifndef DEBUG_NO_LEAK_WARNING

   bool rel_warned = false;
   if(init_data.flags&I3DINIT_WARN_OUT){
                              //check interface counting
      C_str str;
      for(i=0; i<I3D_CLID_LAST; i++)
      if(interface_count[i]){
         if(!str.Size()) str="I3D_driver::Release() - unreleased interfaces:\n";
         str += C_fstr("%s:\t%i\n", inteface_name[i], interface_count[i]);
      }
      if(str.Size()){
         GetGraphInterface()->FlipToGDI();
         MessageBox((HWND)GetGraphInterface()->GetHWND(), str, "I3D Driver Destruction", MB_OK);
         rel_warned = true;
      }
   }
   if(!rel_warned){
#ifndef GL
      assert(!vb_manager_list.size());
      assert(!ib_manager_list.size());
      assert(!hw_vb_list.size());
#endif
#ifdef DEBUG_VB_ALLOC
      if(vb_manager_list.size()){
         dword id = vb_manager_list[0]->GetAllocID();
         C_fstr msg("vb unallocated id: %i.", id);
         MessageBox(NULL, (const char*)msg, "vb leak", MB_OK);
         list<C_RB_huge<struct IDirect3DVertexBuffer9> > &leeked_list = vb_manager_list[0]->GetRBList();

         list<C_RB_huge<struct IDirect3DVertexBuffer9> >::iterator it;
         for(it=leeked_list.begin(); it!=leeked_list.end(); it++){
            (*it).DebugOutput();
         }
      }
#endif
   }
#endif//DEBUG_NO_LEAK_WARNING

   memset(&init_data, 0, sizeof(init_data));
}

//----------------------------

/*
dword *I3D_driver::RegisterVSDeclarator(const C_vector<dword> &decl){

                              //check if such declarator exists
   for(int i=vs_decl_cache.size(); i--; ){
      if(*vs_decl_cache[i]==decl){
         return &vs_decl_cache[i]->front();
      }
   }
   vs_decl_cache.push_back(new C_vector<dword>(decl));
   return &vs_decl_cache.back()->front();
}

//----------------------------

dword *I3D_driver::RegisterVSDeclarator(dword fvf){

   assert(0);  //DX9 port
                              //build shader declarator
   C_vector<dword> decl;
                              //always use stream 0
   decl.push_back(D3DVSD_STREAM(0));

                              //always must contain XYZ
   switch(fvf&D3DFVF_POSITION_MASK){
   case D3DFVF_XYZ: decl.push_back(D3DVSD_REG(0, D3DVSDT_FLOAT3)); break;
   case D3DFVF_XYZB1:
      decl.push_back(D3DVSD_REG(0, D3DVSDT_FLOAT3));
      decl.push_back(D3DVSD_REG(3, D3DVSDT_FLOAT1));
      break;
   case D3DFVF_XYZRHW: decl.push_back(D3DVSD_REG(0, D3DVSDT_FLOAT4)); break;
   default: assert(0);
   }
   if(fvf&D3DFVF_NORMAL)
      decl.push_back(D3DVSD_REG(2, D3DVSDT_FLOAT3));
   if(fvf&D3DFVF_DIFFUSE)
      decl.push_back(D3DVSD_REG(2, D3DVSDT_D3DCOLOR));
   if(fvf&D3DFVF_SPECULAR)
      decl.push_back(D3DVSD_REG(5, D3DVSDT_D3DCOLOR));
   switch((fvf&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT){
   case 0: break;
   case 1: decl.push_back(D3DVSD_REG(1, D3DVSDT_FLOAT2)); break;
   default: decl.push_back(D3DVSD_REG(1, D3DVSDT_FLOAT4)); break;
   }
                              //terminator
   decl.push_back(D3DVSD_END());

   return RegisterVSDeclarator(decl);
   return NULL;
}
*/

//----------------------------

static dword Bin2Text(char *buf, dword len){
   
   char *src = buf, *dst = buf;
   if(len)
   do{
      if(*src!='\r') *dst++=*src;
   }while(++src, --len);
   return dst - buf;
}

//----------------------------

I3D_driver::S_vs_shader_entry *I3D_driver::GetVSHandle(const S_vs_shader_entry_in &se){

                              //find shader in cache
                              //a little trick here - pass in S_vs_shader_entry_in&
                              // cast to a S_vs_shader_entry&
                              // both inherit from same base class, and find() only
                              // uses base's operator< for comparing
   vs_set::iterator it = vs_cache.find(*(const S_vs_shader_entry*)&se);
   if(it != vs_cache.end()){
      (*it).last_render_time = render_time;
      return &(*it);
   }

   assert(se.num_fragments <= MAX_VS_FRAGMENTS);

   HRESULT hr;
                              //manage cache, don't allow to grow too big
   if(vs_cache.size() >= VSHADER_CACHE_SIZE){
                              //find oldest shader, remove this
      vs_set::iterator it_oldest = vs_cache.end();
      dword oldest_time = 4000;  //don't erase too young shaders
      for(it=vs_cache.begin(); it!=vs_cache.end(); it++){
         dword shd_time = render_time - (*it).last_render_time;
         if(oldest_time < shd_time){
            oldest_time = shd_time;
            it_oldest = it;
         }
      }
      if(it_oldest != vs_cache.end())
         vs_cache.erase(it_oldest);
   }

   C_vector<dword> shd_code;
   shd_code.reserve(se.num_fragments + 18);
   bool use_3_0_version = false;
                              //inser code for "vs_1_1"
   shd_code.push_back(!use_3_0_version ? 0xfffe0101 : 0xfffe0300);

                              //convert internal fragment IDs into NVLinker IDs
                              // also include "dcl_usage" opcodes into compiled shader code
   dword *nv_fragment_ids = (dword*)alloca((se.num_fragments + 1) * sizeof(dword));
   bool dcl_used[VSDECL_LAST];
   memset(dcl_used, false, sizeof(dcl_used));

                              //position is always present
   dcl_used[VSDECL_POSITION] = true;
   shd_code.insert(shd_code.end(), vsdecls[use_3_0_version][VSDECL_POSITION], vsdecls[use_3_0_version][VSDECL_POSITION+1]);

   for(dword i=0; i<se.num_fragments; i++){
      E_VS_FRAGMENT vsf = se.fragment_code[i];
      nv_fragment_ids[i] = vs_fragment_ids[vsf];
      E_VS_DECLARATION vsd = VSDECL_LAST,
         vsd1 = VSDECL_LAST,
         vsd2 = VSDECL_LAST;
      switch(vsf){
      case VSF_LIGHT_DIR_OS:
      case VSF_LIGHT_DIR_WS:
      case VSF_LIGHT_POINT_OS:
      case VSF_LIGHT_POINT_WS:
      case VSF_LIGHT_SPOT_OS:
      case VSF_LIGHT_SPOT_WS:
      case VSF_LIGHT_POINT_AMB_OS:
      case VSF_LIGHT_POINT_AMB_WS:
      case VSF_SIMPLE_DIR_LIGHT:
      case VSF_GENERATE_ENVUV_OS:
      case VSF_GENERATE_ENVUV_WS:
      case VSF_GENERATE_ENVUV_CUBE_OS:
      case VSF_GENERATE_ENVUV_CUBE_WS:
         vsd = VSDECL_NORMAL;
         break;
      case VSF_PICK_UV_0:
      case VSF_PICK_UV_1:
      case VSF_TEXT_PROJECT:
      case VSF_SHIFT_UV:
      case VSF_TRANSFORM_DISCHARGE:
         vsd = VSDECL_TEXCOORD01;
         break;
      case VSF_PICK_UV_2:
      case VSF_PICK_UV_3:
         vsd = VSDECL_TEXCOORD23;
         break;
      case VSF_M_PALETTE_TRANSFORM:
         vsd1 = VSDECL_NORMAL;
                              //flow...
      //case VSF_M_PALETTE_TRANSFORM1:
      case VSF_TRANSFORM_MORPH1_OS:
      case VSF_TRANSFORM_MORPH1_WS:
      case VSF_TRANSFORM_MORPH2_OS:
      case VSF_TRANSFORM_MORPH2_WS:
         vsd = VSDECL_BLENDWEIGHT;
         break;
      case VSF_DIFFUSE_COPY:
      case VSF_DIFFUSE_MUL_ALPHA:
      case VSF_COPY_ALPHA:
      case VSF_LIGHT_END_ALPHA:
         vsd = VSDECL_BLENDWEIGHT;
         break;
         /*
      case VSF_TEST:
         vsd = VSDECL_BLENDWEIGHT;
         vsd1 = VSDECL_TEXCOORD01;
         break;
         */
      case VSF_BUMP_OS:
         vsd = VSDECL_TXSPACE_S;
         vsd1 = VSDECL_TXSPACE_T;
         vsd2 = VSDECL_TXSPACE_SxT;
         break;
      //default: assert(0);
      }
      if(vsd != VSDECL_LAST){
         if(!dcl_used[vsd]){
            dcl_used[vsd] = true;
            shd_code.insert(shd_code.end(), vsdecls[use_3_0_version][vsd], vsdecls[use_3_0_version][vsd+1]);
         }
      }
      if(vsd1 != VSDECL_LAST){
         if(!dcl_used[vsd1]){
            dcl_used[vsd1] = true;
            shd_code.insert(shd_code.end(), vsdecls[use_3_0_version][vsd1], vsdecls[use_3_0_version][vsd1+1]);
         }
      }
      if(vsd2 != VSDECL_LAST){
         if(!dcl_used[vsd2]){
            dcl_used[vsd2] = true;
            shd_code.insert(shd_code.end(), vsdecls[use_3_0_version][vsd2], vsdecls[use_3_0_version][vsd2+1]);
         }
      }
   }
   nv_fragment_ids[se.num_fragments] = 0; //terminator

   C_smart_ptr<INVLinkBuffer> nv_shader;
   hr = nv_linker[0]->CreateBinaryShader(nv_fragment_ids, (INVLinkBuffer**)&nv_shader, 0);
   assert(SUCCEEDED(hr));
   if(FAILED(hr))
      return NULL;

#ifdef DEBUG_DUMP_SHADERS
   if(1){
      C_smart_ptr<INVLinkBuffer> src;
      nv_linker[0]->GetShaderSource((INVLinkBuffer**)&src);
      char *cp = (char*)src->GetPointer();
      int len = src->GetBufferSize();
      len = Bin2Text(cp, len);
      cp[len] = 0;
      if(init_data.log_func)
         init_data.log_func(cp);
   }
#endif
   IDirect3DVertexShader9 *vs;

   const dword *shd_ptr = (dword*)nv_shader->GetPointer();
   dword shd_size = nv_shader->GetBufferSize();
   shd_code.insert(shd_code.end(), shd_ptr+1, shd_ptr + shd_size/4);

   hr = d3d_dev->CreateVertexShader(&shd_code.front(), &vs);
   if(FAILED(hr)){
      C_smart_ptr<INVLinkBuffer> src;
      nv_linker[0]->GetShaderSource((INVLinkBuffer**)&src);
      char *cp = (char*)src->GetPointer();
      int len = src->GetBufferSize();
      len = Bin2Text(cp, len);
      cp[len] = 0;
      if(init_data.log_func)
         init_data.log_func(cp);

      CHECK_D3D_RESULT("CreateVertexShader", hr);
      return NULL;
   }

   pair<vs_set::iterator, bool> pit = vs_cache.insert(se);
   assert(pit.second);
                              //save values
   S_vs_shader_entry &se_out = (*pit.first);
   se_out.vs = vs;
   se_out.last_render_time = render_time;

   vs->Release();
   {
                              //parse fragments, get slots of constants which are used
      dword dir_light_count = 0;
      dword pnt_light_count = 0;
      dword spt_light_count = 0;
      dword lfog_light_count = 0;
      dword pfog_light_count = 0;
      dword constant_count = 0;
      for(dword i=0; i<se.num_fragments; i++){
         E_VS_FRAGMENT vsf = se.fragment_code[i];

         switch(vsf){
         case VSF_GENERATE_ENVUV_OS: case VSF_GENERATE_ENVUV_WS:
         case VSF_GENERATE_ENVUV_CUBE_OS: case VSF_GENERATE_ENVUV_CUBE_WS:
            hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_CAM_LOC_POS], 0, &se_out.vscs_cam_loc_pos);
            assert(SUCCEEDED(hr) && se_out.vscs_cam_loc_pos<96);
            break;

         case VSF_LIGHT_DIR_OS:
         case VSF_LIGHT_DIR_WS:
            {
               assert(dir_light_count < MAX_VS_LIGHTS);
               dword &c0 = se_out.vscs_light_param[constant_count++],
                  &c1 = se_out.vscs_light_param[constant_count++];
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_DIR_NORMAL], dir_light_count, &c0);
               assert(SUCCEEDED(hr) && c0<96);
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_DIR_DIFFUSE], dir_light_count, &c1);
               assert(SUCCEEDED(hr) && c1<96);
               ++dir_light_count;
            }
            break;

         case VSF_LIGHT_POINT_OS:
         case VSF_LIGHT_POINT_WS:
         case VSF_LIGHT_POINT_AMB_OS:
         case VSF_LIGHT_POINT_AMB_WS:
         case VSF_LIGHT_SPOT_OS:
         case VSF_LIGHT_SPOT_WS:
            {
               assert(pnt_light_count < MAX_VS_LIGHTS);
               dword &c0 = se_out.vscs_light_param[constant_count++],
                  &c1 = se_out.vscs_light_param[constant_count++];
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_POINT_POS_FR], pnt_light_count, &c0);
               assert(SUCCEEDED(hr) && c0<96);
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_POINT_DIFFUSE_RRD], pnt_light_count, &c1);
               assert(SUCCEEDED(hr) && c1<96);
               ++pnt_light_count;

               if(vsf==VSF_LIGHT_SPOT_OS || vsf==VSF_LIGHT_SPOT_WS){
                  dword &c2 = se_out.vscs_light_param[constant_count++];
                  hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_SPOT_DIR_COS], spt_light_count, &c2);
                  assert(SUCCEEDED(hr) && c2<96);
                  ++spt_light_count;
               }
            }
            break;

         case VSF_FOG_LAYERED_OS:
         case VSF_FOG_LAYERED_WS:
            {
               assert(lfog_light_count < MAX_VS_LIGHTS);
               dword &c0 = se_out.vscs_light_param[constant_count++],
                  &c1 = se_out.vscs_light_param[constant_count++],
                  &c2 = se_out.vscs_light_param[constant_count++];
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_LAYERED_PLANE], lfog_light_count, &c0);
               assert(SUCCEEDED(hr) && c0<96);
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_LAYERED_COL_R], lfog_light_count, &c1);
               assert(SUCCEEDED(hr) && c1<96);
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_LAYERED_POS], lfog_light_count, &c2);
               assert(SUCCEEDED(hr) && c2<96);
               ++lfog_light_count;
            }
            break;

         case VSF_FOG_POINT_OS:
         case VSF_FOG_POINT_WS:
            {
               assert(pfog_light_count < MAX_VS_LIGHTS);
               dword &c0 = se_out.vscs_light_param[constant_count++],
                  &c1 = se_out.vscs_light_param[constant_count++],
                  &c2 = se_out.vscs_light_param[constant_count++];
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_PFOG_CONE_TOP], pfog_light_count, &c0);
               assert(SUCCEEDED(hr) && c0<96);
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_PFOG_CONE_DIR], pfog_light_count, &c1);
               assert(SUCCEEDED(hr) && c1<96);
               hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_LIGHT_PFOG_COLOR], pfog_light_count, &c2);
               assert(SUCCEEDED(hr) && c2<96);
               ++pfog_light_count;
            }
            break;

         case VSF_TRANSFORM_DISCHARGE:
            hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_MATRIX_FRAME], 0, &se_out.vscs_mat_frame);
            assert(SUCCEEDED(hr) && se_out.vscs_mat_frame<96);
            hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_CAM_LOC_POS], 0, &se_out.vscs_cam_loc_pos);
            assert(SUCCEEDED(hr) && se_out.vscs_cam_loc_pos<96);
            break;

         case VSF_TRANSFORM_UV0:
            hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_UV_TRANSFORM], 0, &se_out.vscs_uv_transform);
            assert(SUCCEEDED(hr) && se_out.vscs_uv_transform<96);
            break;

         case VSF_SHIFT_UV:
            hr = nv_linker[0]->GetConstantSlot(vs_constant_ids[VSC_UV_SHIFT], 0, &se_out.vscs_uv_shift);
            assert(SUCCEEDED(hr) && se_out.vscs_uv_shift<96);
            break;
         }
      }
   }
   return &se_out;
}

//----------------------------

I3D_driver::S_ps_shader_entry *I3D_driver::GetPSHandle(S_ps_shader_entry_in &se){

#if defined _DEBUG && 0
                              //make sure all higher textures are set to NULL
   {
      bool stage_used[4] = {false, false, false, false};
      for(dword i=0; i<se.num_fragments; i++){
         E_PS_FRAGMENT frg = se.fragment_code[i];
         switch(frg){
         case PSF_TEX_0:
            stage_used[0] = true; break;
         case PSF_TEX_1:
         case PSF_TEX_1_BEM:
         case PSF_TEX_1_BEML:
            stage_used[1] = true; break;
         case PSF_TEX_2:
         case PSF_TEX_2_BEM:
         case PSF_TEX_2_BEML:
            stage_used[2] = true; break;
         case PSF_TEX_3:
         case PSF_TEX_3_BEM:
         case PSF_TEX_3_BEML:
            stage_used[3] = true; break;
         }
      }
      for(i=4; i--; ){
         if(!stage_used[i]) assert(!last_texture[i]);
      }
   }
#endif

                              //find shader in cache
                              //a little trick here - pass in S_ps_shader_entry_in&
                              // cast to a S_ps_shader_entry&
                              // both inherit from same base class, and find() only
                              // uses base's operator< for comparing
   ps_set::iterator it = ps_cache.find(*(const S_ps_shader_entry*)&se);
   if(it != ps_cache.end()){
      (*it).last_render_time = render_time;
      return &(*it);
   }

   assert(se.num_fragments <= 8);

   HRESULT hr;
                              //manage cache, don't allow to grow too big
   if(ps_cache.size() >= PSHADER_CACHE_SIZE){
                              //find oldest shader, remove this
      ps_set::iterator it_oldest = ps_cache.end();
      dword oldest_time = 4000;  //don't erase too young shaders
      for(it=ps_cache.begin(); it!=ps_cache.end(); it++){
         dword shd_time = render_time - (*it).last_render_time;
         if(oldest_time < shd_time){
            oldest_time = shd_time;
            it_oldest = it;
         }
      }
      if(it_oldest != ps_cache.end()){
         //hr = d3d_dev->DeletePixelShader((*it_oldest).shd_handle);
         //CHECK_D3D_RESULT("DeletePixelShader", hr);
         ps_cache.erase(it_oldest);
      }
   }

                              //convert internal fragment IDs into NVLinker IDs
   dword *nv_fragment_ids = (dword*)alloca((se.num_fragments + 1) * sizeof(dword));
   dword i, di = 0;
                              //add tex declarations first
   for(i=0; i<se.num_fragments; i++){
      E_PS_FRAGMENT frg = se.fragment_code[i];
      if(frg < PSF_FIRST_NOTEX)
         nv_fragment_ids[di++] = ps_fragment_ids[frg];
   }
   for(i=0; i<se.num_fragments; i++){
      E_PS_FRAGMENT frg = se.fragment_code[i];
      if(frg >= PSF_FIRST_NOTEX)
         nv_fragment_ids[di++] = ps_fragment_ids[frg];
   }
   nv_fragment_ids[se.num_fragments] = 0; //terminator

   C_smart_ptr<INVLinkBuffer> nv_shader;
   hr = nv_linker[1]->CreateBinaryShader(nv_fragment_ids, (INVLinkBuffer**)&nv_shader, 0);
   assert(SUCCEEDED(hr));
   if(FAILED(hr))
      return NULL;

#ifdef DEBUG_DUMP_SHADERS
   if(1){
      C_smart_ptr<INVLinkBuffer> src;
      nv_linker[1]->GetShaderSource((INVLinkBuffer**)&src);
      char *cp = (char*)src->GetPointer();
      int len = src->GetBufferSize();
      len = Bin2Text(cp, len);
      cp[len] = 0;
      if(init_data.log_func)
         init_data.log_func(cp);
   }
#endif
   IDirect3DPixelShader9 *ps;

   hr = d3d_dev->CreatePixelShader((const dword*)nv_shader->GetPointer(), &ps);
   CHECK_D3D_RESULT("CreatePixelShader", hr);
   if(FAILED(hr))
      return NULL;

   pair<ps_set::iterator, bool> pit = ps_cache.insert(se);
   assert(pit.second);
                              //save values
   S_ps_shader_entry &se_out = (*pit.first);
   se_out.ps = ps;
   se_out.last_render_time = render_time;
   ps->Release();

   return &se_out;
}

//----------------------------

void I3D_driver::SetPixelShader(S_ps_shader_entry_in &se){

   SetPixelShader(GetPSHandle(se)->ps);
}

//----------------------------
//----------------------------

static void ConvertD3DpfToI3Dpf(const D3DFORMAT d3d_fmt, S_pixelformat &pf){

   memset(&pf, 0, sizeof(pf));
                              //setup bpp
   switch(d3d_fmt){

   case D3DFMT_P8:
   case D3DFMT_R3G3B2:
      pf.bytes_per_pixel = 1;
      break;

   case D3DFMT_R5G6B5:
   case D3DFMT_X1R5G5B5:
   case D3DFMT_A1R5G5B5:
   case D3DFMT_A4R4G4B4:
   case D3DFMT_A8R3G3B2:
   case D3DFMT_X4R4G4B4:
   case D3DFMT_V8U8:
   case D3DFMT_L6V5U5:
      pf.bytes_per_pixel = 2;
      break;

   case D3DFMT_R8G8B8:
      pf.bytes_per_pixel = 3;
      break;

   case D3DFMT_A8R8G8B8:
   case D3DFMT_X8R8G8B8:
   case D3DFMT_X8L8V8U8:
   case D3DFMT_Q8W8V8U8:
   case D3DFMT_V16U16:
   //case D3DFMT_W11V11U10:
      pf.bytes_per_pixel = 4;
      break;
   }
                              //setup alpha/palette/compression/bump
   switch(d3d_fmt){
#ifndef GL
   case D3DFMT_DXT1:
   case D3DFMT_DXT2:
   case D3DFMT_DXT3:
   case D3DFMT_DXT4:
   case D3DFMT_DXT5:
      {
         pf.flags = PIXELFORMAT_COMPRESS;
         pf.flags |= PIXELFORMAT_ALPHA;
         byte b;
         switch(d3d_fmt){
         case D3DFMT_DXT1: pf.flags &= ~PIXELFORMAT_ALPHA; b = 1; break;
         case D3DFMT_DXT2: b = 2; break;
         case D3DFMT_DXT3: b = 3; break;
         case D3DFMT_DXT4: b = 4; break;
         case D3DFMT_DXT5: b = 5; break;
         default: assert(0); b = 0;
         }
         pf.four_cc = MAKEFOURCC('D', 'X', 'T', '0' + b);
      }
      break;
   case D3DFMT_P8:
      pf.flags = PIXELFORMAT_PALETTE;
      break;
#endif
   case D3DFMT_A8R8G8B8:
   case D3DFMT_A1R5G5B5:
   case D3DFMT_A4R4G4B4:
   case D3DFMT_A8R3G3B2:
      pf.flags = PIXELFORMAT_ALPHA;
      break;

   case D3DFMT_L6V5U5:
      pf.flags = PIXELFORMAT_BUMPMAP | PIXELFORMAT_ALPHA;
      break;                              
   case D3DFMT_V8U8:
   case D3DFMT_X8L8V8U8:
   case D3DFMT_Q8W8V8U8:
   case D3DFMT_V16U16:
   //case D3DFMT_W11V11U10:
      pf.flags = PIXELFORMAT_BUMPMAP;
      break;
   }

                              //setup RGB/UV components
   switch(d3d_fmt){
   case D3DFMT_R8G8B8:
   case D3DFMT_X8R8G8B8:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x00ff0000;
      pf.g_mask = 0x0000ff00;
      pf.b_mask = 0x000000ff;
      break;
   case D3DFMT_A8R8G8B8:
      pf.a_mask = 0xff000000;
      pf.r_mask = 0x00ff0000;
      pf.g_mask = 0x0000ff00;
      pf.b_mask = 0x000000ff;
      break;
   case D3DFMT_R5G6B5:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x0000f800;
      pf.g_mask = 0x000007e0;
      pf.b_mask = 0x0000001f;
      break;
   case D3DFMT_X1R5G5B5:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x00007c00;
      pf.g_mask = 0x000003e0;
      pf.b_mask = 0x0000001f;
      break;
   case D3DFMT_A1R5G5B5:
      pf.a_mask = 0x00008000;
      pf.r_mask = 0x00007c00;
      pf.g_mask = 0x000003e0;
      pf.b_mask = 0x0000001f;
      break;
   case D3DFMT_A4R4G4B4:
      pf.a_mask = 0x0000f000;
      pf.r_mask = 0x00000f00;
      pf.g_mask = 0x000000f0;
      pf.b_mask = 0x0000000f;
      break;
   case D3DFMT_X4R4G4B4:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x00000f00;
      pf.g_mask = 0x000000f0;
      pf.b_mask = 0x0000000f;
      break;
   case D3DFMT_R3G3B2:
      pf.a_mask = 0x00000000;
      pf.r_mask = 0x000000e0;
      pf.g_mask = 0x0000001c;
      pf.b_mask = 0x00000003;
      break;
   case D3DFMT_A8R3G3B2:
      pf.a_mask = 0x0000ff00;
      pf.r_mask = 0x000000e0;
      pf.g_mask = 0x0000001c;
      pf.b_mask = 0x00000003;
      break;

   case D3DFMT_V8U8:
      pf.u_mask = 0x00ff;
      pf.v_mask = 0xff00;
      pf.l_mask = 0x0000;
      break;

   case D3DFMT_L6V5U5:
      pf.u_mask = 0x001f;
      pf.v_mask = 0x03e0;
      pf.l_mask = 0xfc00;
      break;      

   case D3DFMT_X8L8V8U8:
      pf.u_mask = 0x000000ff;
      pf.v_mask = 0x0000ff00;
      pf.l_mask = 0x00ff0000;
      break;      

   case D3DFMT_Q8W8V8U8:
      pf.u_mask = 0x000000ff;
      pf.v_mask = 0x0000ff00;
      pf.l_mask = 0x00000000;
      break;      

   case D3DFMT_V16U16:
      pf.u_mask = 0x0000ffff;
      pf.v_mask = 0xffff0000;
      pf.l_mask = 0x00000000;
      break;      
   /*
   case D3DFMT_W11V11U10:
      pf.u_mask = 0x000003ff;
      pf.v_mask = 0x001ffc00;
      pf.l_mask = 0x00000000;
      break;      
      */
   }
}

//----------------------------

static D3DFORMAT ConvertI3DpfToD3Dpf(const S_pixelformat &pf){

#ifndef GL
   if(pf.flags&PIXELFORMAT_COMPRESS){
      switch(pf.four_cc){
      case MAKEFOURCC('D', 'X', 'T', '1'):
         return D3DFMT_DXT1;
      case MAKEFOURCC('D', 'X', 'T', '2'):
         return D3DFMT_DXT2;
      case MAKEFOURCC('D', 'X', 'T', '3'):
         return D3DFMT_DXT3;
      case MAKEFOURCC('D', 'X', 'T', '4'):
         return D3DFMT_DXT4;
      case MAKEFOURCC('D', 'X', 'T', '5'):
         return D3DFMT_DXT5;
      }
   }else
#endif
   if(pf.flags&PIXELFORMAT_BUMPMAP){
      switch(pf.bytes_per_pixel){
      case 2:
         switch(pf.u_mask){
         case 0x00ff:
            assert(pf.v_mask==0xff00);
            assert(pf.l_mask==0x0000);
            return D3DFMT_V8U8;
         case 0x001f:
            assert(pf.v_mask==0x03e0);
            assert(pf.l_mask==0xfc00);
            return D3DFMT_L6V5U5;
         default:
            assert(0);
         }
         break;
      case 4:
         switch(pf.u_mask){
         case 0x000000ff:
            assert(pf.v_mask==0x0000ff00);
            switch(pf.l_mask){
            case 0x00ff0000:
               return D3DFMT_X8L8V8U8;
            case 0x00000000:
               return D3DFMT_Q8W8V8U8;
            default:
               assert(0);
            }
            break;
         case 0x0000ffff:
            assert(pf.v_mask==0xffff0000);
            assert(pf.l_mask==0x00000000);
            return D3DFMT_V16U16;
            /*
         case 0x000003ff:
            assert(pf.v_mask==0x001ffc00);
            assert(pf.l_mask==0xffe00000);
            return D3DFMT_W11V11U10;
            */
         default:
            assert(0);
         }
         break;
      default:
         assert(0);
      }
   }else
   switch(pf.bytes_per_pixel){

   case 1:
#ifndef GL
      if(pf.flags&PIXELFORMAT_PALETTE)
         return D3DFMT_P8;
      else
#endif
         return D3DFMT_R3G3B2;

   case 2:

      switch(pf.r_mask){
      case 0xf800:
         assert(pf.g_mask==0x07e0);
         assert(pf.b_mask==0x001f);
         return D3DFMT_R5G6B5;

      case 0x7c00:
         switch(pf.a_mask){
         case 0x0000:
            return D3DFMT_X1R5G5B5;
         case 0x8000:
            return D3DFMT_A1R5G5B5;
         }
         break;

      case 0x0f00:
         switch(pf.a_mask){
         case 0x0000:
            return D3DFMT_X4R4G4B4;
         case 0xf000:
            assert(pf.r_mask==0x0f00);
            assert(pf.g_mask==0x00f0);
            assert(pf.b_mask==0x000f);
            return D3DFMT_A4R4G4B4;
         }
         break;

      case 0x00e0:
         return D3DFMT_A8R3G3B2;

      default: assert(0);
      }
      break;

   case 3:
      assert(pf.r_mask == 0x00ff0000);
      assert(pf.g_mask == 0x0000ff00);
      assert(pf.b_mask == 0x000000ff);
      assert(pf.a_mask == 0x00000000);
      return D3DFMT_R8G8B8;

   case 4:
      assert(pf.r_mask == 0x00ff0000);
      assert(pf.g_mask == 0x0000ff00);
      assert(pf.b_mask == 0x000000ff);
      switch(pf.a_mask){
      case 0:
         return D3DFMT_X8R8G8B8;
      case 0xff000000:
         return D3DFMT_A8R8G8B8;
      }
      break;
   default: assert(0);
   }
   return D3DFMT_UNKNOWN;
}

//----------------------------

static const S_vertex_rectangle rect_coor[4] = {
   -.5f,  .5f,//       0, 0,
    .5f,  .5f,//       1, 0,
   -.5f, -.5f,//       0, 1,
    .5f, -.5f,//       1, 1
};

//----------------------------

I3D_RESULT I3D_driver::InitD3DResources(){

   EnableZBUsage(true);
   HRESULT hr;
   hr = d3d_dev->GetDeviceCaps(&d3d_caps);
   CHECK_D3D_RESULT("GetDeviceCaps", hr);

   {                          //setup blend table
      dword caps = d3d_caps.SrcBlendCaps;
                              //setup source
      blend_table[0][0] = D3DBLEND_ZERO;
      blend_table[0][D3DBLEND_ZERO] = caps&D3DPBLENDCAPS_ZERO ? D3DBLEND_ZERO : D3DBLEND_ONE;
      blend_table[0][D3DBLEND_ONE] = D3DBLEND_ONE;
      blend_table[0][D3DBLEND_SRCCOLOR] = caps&D3DPBLENDCAPS_SRCCOLOR ? D3DBLEND_SRCCOLOR :
         caps&D3DPBLENDCAPS_SRCALPHA ? D3DBLEND_SRCALPHA : D3DBLEND_ONE;
      blend_table[0][D3DBLEND_INVSRCCOLOR] = caps&D3DPBLENDCAPS_INVSRCCOLOR ? D3DBLEND_INVSRCCOLOR :
         caps&D3DPBLENDCAPS_INVSRCCOLOR ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE;
      blend_table[0][D3DBLEND_SRCALPHA] = caps&D3DPBLENDCAPS_SRCALPHA ? D3DBLEND_SRCALPHA :
         blend_table[0][D3DBLEND_SRCCOLOR];
      blend_table[0][D3DBLEND_INVSRCALPHA] = caps&D3DPBLENDCAPS_INVSRCALPHA ? D3DBLEND_INVSRCALPHA :
         blend_table[0][D3DBLEND_INVSRCCOLOR];
      blend_table[0][D3DBLEND_DESTCOLOR] = caps&D3DPBLENDCAPS_DESTCOLOR ? D3DBLEND_DESTCOLOR :
         caps&D3DPBLENDCAPS_DESTALPHA ? D3DBLEND_DESTALPHA : D3DBLEND_ONE;
      blend_table[0][D3DBLEND_INVDESTCOLOR] = caps&D3DPBLENDCAPS_INVDESTCOLOR ? D3DBLEND_INVDESTCOLOR :
         caps&D3DPBLENDCAPS_INVDESTCOLOR ? D3DBLEND_INVDESTALPHA : D3DBLEND_ONE;
      blend_table[0][D3DBLEND_DESTALPHA] = caps&D3DPBLENDCAPS_DESTALPHA ? D3DBLEND_DESTALPHA :
         blend_table[0][D3DBLEND_DESTCOLOR];
      blend_table[0][D3DBLEND_INVDESTALPHA] = caps&D3DPBLENDCAPS_INVDESTALPHA ? D3DBLEND_INVDESTALPHA :
         blend_table[0][D3DBLEND_INVDESTCOLOR];
      blend_table[0][D3DBLEND_SRCALPHASAT] = D3DBLEND_SRCALPHASAT;
      blend_table[0][D3DBLEND_SRCALPHASAT] = D3DBLEND_BOTHSRCALPHA;
      blend_table[0][D3DBLEND_BOTHINVSRCALPHA] = D3DBLEND_BOTHINVSRCALPHA;
                              //setup dest
      memcpy(&blend_table[1], &blend_table[0], sizeof(blend_table[0]));
      blend_table[1][0] = D3DBLEND_ZERO;
      blend_table[1][D3DBLEND_ZERO] = D3DBLEND_ZERO;
      blend_table[1][D3DBLEND_ONE] = caps&D3DPBLENDCAPS_ONE ? D3DBLEND_ONE : blend_table[1][D3DBLEND_INVSRCALPHA];
   }
#ifndef GL
   drv_flags2 &= ~DRVF2_USE_PS;
#endif

   d3d_dev->SetFVF(D3DFVF_XYZ);

   D3DDEVICE_CREATION_PARAMETERS cp;
   hr = d3d_dev->GetCreationParameters(&cp);
   CHECK_D3D_RESULT("GetCreationParameters", hr);
                              //get list of supported texture formats
   {
      texture_formats[0].clear();
      texture_formats[1].clear();
      rtt_zbuf_fmts.clear();
      D3DDISPLAYMODE disp_mode;
      hr = d3d_dev->GetDisplayMode(0, &disp_mode);
      CHECK_D3D_RESULT("GetDisplayMode", hr);

      C_vector<D3DFORMAT> rtt_formats;
      for(int i = 0 ; i < sizeof(supported_texture_formats)/sizeof(D3DFORMAT); i++){
         for(int j=0; j<2; j++){
            D3DFORMAT fmt = supported_texture_formats[i];
            hr = pD3D_driver->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, disp_mode.Format,
               !j ? 0 : D3DUSAGE_RENDERTARGET,
               D3DRTYPE_TEXTURE, fmt);
            if(SUCCEEDED(hr)){
               texture_formats[j].push_back(S_pixelformat());
               S_pixelformat &pf = texture_formats[j].back();
               ConvertD3DpfToI3Dpf(fmt, pf);
               if(j==1){
                              //store also D3D fmt, for z-buf checking below
                  rtt_formats.push_back(fmt);
                  rtt_zbuf_fmts.push_back(C_vector<D3DFORMAT>());
               }
            }
         }
      }
      for(i=0; i<sizeof(supported_zsten_formats)/sizeof(D3DFORMAT); i++){
         D3DFORMAT fmt = supported_zsten_formats[i];
         hr = pD3D_driver->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, disp_mode.Format,
            D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, fmt);
         if(SUCCEEDED(hr)){
                              //check if this format matches some render targets
            for(int j=rtt_formats.size(); j--; ){
               hr = pD3D_driver->CheckDepthStencilMatch(cp.AdapterOrdinal, cp.DeviceType, disp_mode.Format,
                  rtt_formats[j], fmt);
               if(SUCCEEDED(hr)){
                  rtt_zbuf_fmts[j].push_back(fmt);
               }
            }
         }
      }
                              //now remove all RTT formats which has no compatible depth formats
      for(i=texture_formats[1].size(); i--; ){
         if(!rtt_zbuf_fmts[i].size()){
            texture_formats[1][i] = texture_formats[1].back(); texture_formats[1].pop_back();
            rtt_zbuf_fmts[i] = rtt_zbuf_fmts.back(); rtt_zbuf_fmts.pop_back();
         }
      }
#ifndef GL
      is_hal = (cp.DeviceType == D3DDEVTYPE_HAL);
#endif
   }

                              //init D3D defaults
   hr = d3d_dev->SetTextureStageState(0, D3DTSS_COLOROP, stage_txt_op[0] = D3DTOP_MODULATE);
   CHECK_D3D_RESULT("SetTextureStageState", hr);
   for(dword i=1; i<d3d_caps.MaxTextureBlendStages; i++){
      hr = d3d_dev->SetTextureStageState(i, D3DTSS_COLOROP, stage_txt_op[i] = D3DTOP_DISABLE);
      CHECK_D3D_RESULT("SetTextureStageState", hr);
      hr = d3d_dev->SetTextureStageState(i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
      CHECK_D3D_RESULT("SetTextureStageState", hr);
      hr = d3d_dev->SetTextureStageState(i, D3DTSS_COLORARG2, D3DTA_CURRENT);
      CHECK_D3D_RESULT("SetTextureStageState", hr);
      hr = d3d_dev->SetTextureStageState(i, D3DTSS_ALPHAOP, stage_alpha_op[i] = D3DTOP_DISABLE);
      CHECK_D3D_RESULT("SetTextureStageState", hr);
   }
   for(i=0; i<d3d_caps.MaxTextureBlendStages; i++){
                              //setup bump-mapping formulas
      d3d_dev->SetTextureStageState(i, D3DTSS_BUMPENVLSCALE, I3DFloatAsInt(1.0f));
      d3d_dev->SetTextureStageState(i, D3DTSS_BUMPENVLOFFSET, I3DFloatAsInt(1.0f));
      d3d_dev->SetTextureStageState(i, D3DTSS_BUMPENVMAT00, I3DFloatAsInt(0.0f));
      d3d_dev->SetTextureStageState(i, D3DTSS_BUMPENVMAT01, I3DFloatAsInt(0.0f));
      d3d_dev->SetTextureStageState(i, D3DTSS_BUMPENVMAT10, I3DFloatAsInt(0.0f));
      d3d_dev->SetTextureStageState(i, D3DTSS_BUMPENVMAT11, I3DFloatAsInt(0.0f));
   }
   memset(last_embm_scale, 0, sizeof(last_embm_scale));
   last_fvf = 0;

   hr = d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, stage_alpha_op[0] = D3DTOP_MODULATE);
   CHECK_D3D_RESULT("SetTextureStageState", hr);
   hr = d3d_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
   CHECK_D3D_RESULT("SetTextureStageState", hr);

   hr = d3d_dev->SetRenderState(D3DRS_FOGCOLOR, d3d_fog_color = 0x808080);
   CHECK_D3D_RESULT("SetRenderState", hr);

                              //FFP-only states: (not used, reset just for complete code)
   d3d_dev->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
   d3d_dev->SetRenderState(D3DRS_NORMALIZENORMALS, false);
   d3d_dev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
   d3d_dev->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, false);
   d3d_dev->SetRenderState(D3DRS_TWEENFACTOR, 0);


   d3d_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
   d3d_dev->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, false);
   d3d_dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, false);
   d3d_dev->SetRenderState(D3DRS_STENCILREF, stencil_ref = 1);
   d3d_dev->SetRenderState(D3DRS_STENCILPASS, stencil_pass = D3DSTENCILOP_KEEP);
   d3d_dev->SetRenderState(D3DRS_STENCILFAIL, stencil_fail = D3DSTENCILOP_KEEP);
   d3d_dev->SetRenderState(D3DRS_STENCILZFAIL, stencil_zfail = D3DSTENCILOP_KEEP);
   d3d_dev->SetRenderState(D3DRS_STENCILFUNC, stencil_func = D3DCMP_ALWAYS);
   d3d_dev->SetRenderState(D3DRS_STENCILENABLE, stencil_on = false);

   d3d_dev->SetRenderState(D3DRS_TEXTUREFACTOR, last_texture_factor = 0);
   last_alpha_ref = (dword)-1;

   drv_flags2 &= ~DRVF2_CAN_USE_UBYTE4;
   //if(!(d3d_caps.VertexProcessingCaps&D3DVTXPCAPS_NO_VSDT_UBYTE4))
   if(d3d_caps.DeclTypes&D3DDTCAPS_UBYTE4)
      drv_flags2 |= DRVF2_CAN_USE_UBYTE4;

                              //re-apply filtering
   bool is_filter = (drv_flags&DRVF_LINFILTER);
   drv_flags ^= DRVF_LINFILTER;
   SetState(RS_LINEARFILTER, is_filter);

   curr_anisotropy = 0xffffffff;

   for(i=0; i<d3d_caps.MaxTextureBlendStages; i++){
      last_txt_coord_index[i] = i;
      SetTexture1(i, NULL);
      stage_minf[i] = D3DTEXF_POINT;
      EnableAnisotropy(i, true);
   }

   d3d_dev->SetRenderState(D3DRS_SPECULARENABLE, false);
   d3d_dev->SetRenderState(D3DRS_ALPHATESTENABLE, alpharef_enable = false);

                              //init fog
   d3d_dev->SetRenderState(D3DRS_FOGENABLE, fog_enable = false);

   d3d_dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
   d3d_dev->SetRenderState(D3DRS_LIGHTING, false);

   d3d_dev->SetRenderState(D3DRS_SRCBLEND, srcblend = D3DBLEND_ONE);
   d3d_dev->SetRenderState(D3DRS_DESTBLEND, dstblend = D3DBLEND_ZERO);
                              //setup clipping
   //d3d_clipping = true;
   d3d_dev->SetRenderState(D3DRS_CLIPPING, true);

   alpha_blend_on = false;
   last_blend_flags = I3DBLEND_OPAQUE;
   b_cullnone = false;
   zw_enable = true;

#ifdef DEBUG_MAX_TEXTURE_STAGES
   d3d_caps.MaxTextureBlendStages = Min(d3d_caps.MaxTextureBlendStages, (dword)DEBUG_MAX_TEXTURE_STAGES);
   d3d_caps.MaxSimultaneousTextures = Min(d3d_caps.MaxSimultaneousTextures, (dword)DEBUG_MAX_TEXTURE_STAGES);
#endif

#ifndef GL
                              //check basic blending caps
   drv_flags2 &= ~(DRVF2_CAN_MODULATE2X | DRVF2_CAN_TXTOP_ADD);
#ifndef DEBUG_NO_MODULATE2X
   if(d3d_caps.TextureOpCaps&D3DTEXOPCAPS_MODULATE2X){
                              //make sure we really can use this (some drivers report it, but can't use it)

      I3D_RESULT ir;
      S_pixelformat pf;
      ir = FindTextureFormat(pf, 0);
      assert(I3D_SUCCESS(ir));

      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      ct.flags = 0;
      ct.flags |= TEXTMAP_NOMIPMAP;
      ct.size_x = 16;
      ct.size_y = 16;
      ct.flags |= TEXTMAP_USEPIXELFORMAT;
      ct.pf = &pf;

      PI3D_texture tp;
      if(I3D_SUCCESS(CreateTexture(&ct, &tp))){
         SetupTextureStage(0, D3DTOP_MODULATE2X);
         SetTexture1(0, tp);

         dword num_passes = 0;
         HRESULT hr = d3d_dev->ValidateDevice(&num_passes);
         if(SUCCEEDED(hr) && num_passes==1){
            drv_flags2 |= DRVF2_CAN_MODULATE2X;
         }
         SetTexture1(0, NULL);
         tp->Release();
      }
   }
#endif
   if(d3d_caps.TextureOpCaps&D3DTEXOPCAPS_ADD){
      drv_flags2 |= DRVF2_CAN_TXTOP_ADD;
   }
#endif
                              //determine vertex blending mode
#ifndef GL
   vertex_light_mult = 1.0f;
   vertex_light_blend_op = D3DTOP_MODULATE;
   if(drv_flags2&DRVF2_CAN_MODULATE2X){
      vertex_light_blend_op = D3DTOP_MODULATE2X;
      vertex_light_mult = .5f;
   }
#else
   vertex_light_mult = .5f;
#endif
#ifndef GL
                              //determine if device is capable of
                              // hardware transforming
   drv_flags2 &= ~DRVF2_DIRECT_TRANSFORM;
   if(d3d_caps.DevCaps&D3DDEVCAPS_HWTRANSFORMANDLIGHT){
      D3DDEVICE_CREATION_PARAMETERS cp;
      hr = d3d_dev->GetCreationParameters(&cp);
      CHECK_D3D_RESULT("GetCreationParameters", hr);
      if(cp.BehaviorFlags&(D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MIXED_VERTEXPROCESSING)){
         drv_flags2 |= DRVF2_DIRECT_TRANSFORM;
      }
   }
#ifdef DEBUG_FORCE_DIRECT_TRANSFORM
   drv_flags2 |= DRVF2_DIRECT_TRANSFORM;
#endif
   if(init_data.flags&I3DINIT_DIRECT_TRANS)
      drv_flags2 |= DRVF2_DIRECT_TRANSFORM;
#endif

   num_available_txt_stages = MaxSimultaneousTextures();
   InitLMConvertor();
#ifndef GL
                              //determine if device is capable to use pixel shaders
   if(!(init_data.flags&I3DINIT_NO_PSHADER)){
      if(IsDirectTransform() && (d3d_caps.PixelShaderVersion&0xffff) >= 0x0101){
         drv_flags2 |= DRVF2_USE_PS;
      }
   }
#endif
   num_available_txt_stages = MaxSimultaneousTextures();


   if(!vb_rectangle){
                              //init vertex buffer for rendering of rectangles
      HRESULT hr;
      dword d3d_usage = D3DUSAGE_WRITEONLY;
#ifndef GL
      if(!IsDirectTransform())
         d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
#endif
      IDirect3DVertexBuffer9 *vb;
      dword buf_size = sizeof(S_vertex_rectangle) * 4;
      hr = d3d_dev->CreateVertexBuffer(buf_size, d3d_usage, 0, D3DPOOL_DEFAULT, &vb, NULL);
      CHECK_D3D_RESULT("CreateVertexBuffer", hr);
      if(SUCCEEDED(hr)){
         void *p_dst;
         hr = vb->Lock(0, buf_size, &p_dst, 0);
         if(SUCCEEDED(hr)){
            memcpy(p_dst, rect_coor, buf_size);
            vb->Unlock();
            vb_rectangle = vb;
         }
         vb->Release();
      }

      C_vector<D3DVERTEXELEMENT9> els;
      els.push_back(S_vertex_element(0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_POSITION));
      els.push_back(D3DVERTEXELEMENT9_END);
      vs_decl_rectangle = GetVSDeclaration(els);
   }

   if(!vb_clear){
                              //init vertex buffer for rendering of rectangles
      HRESULT hr;
      dword d3d_usage = D3DUSAGE_WRITEONLY;
#ifndef GL
      if(!IsDirectTransform())
         d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
#endif
      IDirect3DVertexBuffer9 *vb;
      dword buf_size = sizeof(S_vectorw)*4;
      hr = d3d_dev->CreateVertexBuffer(buf_size, d3d_usage, 0, D3DPOOL_DEFAULT, &vb, NULL);
      CHECK_D3D_RESULT("CreateVertexBuffer", hr);
      if(SUCCEEDED(hr)){
         vb_clear = vb;
         vb->Release();
      }
   }
   if(!vb_particle){
                              //init vertex buffer for particles
      HRESULT hr;
      dword d3d_usage = D3DUSAGE_WRITEONLY;
#ifndef GL
      if(!IsDirectTransform())
         d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
#endif
      IDirect3DVertexBuffer9 *vb;
      dword buf_size = sizeof(S_vertex_particle) * 4 * MAX_PARTICLE_RECTANGLES;
      hr = d3d_dev->CreateVertexBuffer(buf_size, d3d_usage, 0, D3DPOOL_DEFAULT, &vb, NULL);
      CHECK_D3D_RESULT("CreateVertexBuffer", hr);
      if(SUCCEEDED(hr)){
         S_vertex_particle *p_dst;
         hr = vb->Lock(0, buf_size, (void**)&p_dst, 0);
         if(SUCCEEDED(hr)){
            for(dword ri=0; ri<MAX_PARTICLE_RECTANGLES; ri++){
               for(dword vi=0; vi<4; vi++, ++p_dst){
                  const S_vertex_rectangle &v_src = rect_coor[vi];
                  p_dst->x = v_src.x;
                  p_dst->y = v_src.y;
                  p_dst->index = (float)(ri*4);
               }
            }
            vb->Unlock();
            vb_particle = vb;
         }
         vb->Release();
      }
   }
   if(!ib_particle){
                              //init vertex buffer for particles
      HRESULT hr;
      dword d3d_usage = D3DUSAGE_WRITEONLY;
#ifndef GL
      if(!IsDirectTransform())
         d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
#endif
      IDirect3DIndexBuffer9 *ib;
      dword buf_size = sizeof(I3D_triface)*MAX_PARTICLE_RECTANGLES*2;
      hr = d3d_dev->CreateIndexBuffer(buf_size, d3d_usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, NULL);
      CHECK_D3D_RESULT("CreateIndexBuffer", hr);
      if(SUCCEEDED(hr)){
         I3D_triface *p_dst;
         hr = ib->Lock(0, buf_size, (void**)&p_dst, 0);
         if(SUCCEEDED(hr)){
            dword base = 0;
            for(dword ri=0; ri<MAX_PARTICLE_RECTANGLES; ri++, p_dst += 2, base += 4){
               p_dst[0][0] = word(base+0);
               p_dst[0][1] = word(base+1);
               p_dst[0][2] = word(base+2);
               p_dst[1][0] = word(base+2);
               p_dst[1][1] = word(base+1);
               p_dst[1][2] = word(base+3);
            }
            ib->Unlock();
            ib_particle = ib;
         }
         ib->Release();
      }
   }

   SetState(RS_USELMAPPING, GetState(RS_USELMAPPING));

                              //detect support for bump-mapping
   drv_flags &= ~DRVF_EMBMMAPPING;

#ifndef DEBUG_DISABLE_BUMPMAPPING
                              //does this device support the two bump mapping blend operations?
                              //does this device support up to 2 blending stages?
   if((d3d_caps.TextureOpCaps&(D3DTEXOPCAPS_BUMPENVMAP|D3DTEXOPCAPS_BUMPENVMAPLUMINANCE)) &&
      d3d_caps.MaxTextureBlendStages >= 2){

      drv_flags |= DRVF_EMBMMAPPING;
   }
#endif//!DEBUG_DISABLE_BUMPMAPPING

                              //determine if stencil-buffer is attached
   drv_flags &= ~DRVF_HASSTENCIL;
   {
      IDirect3DSurface9 *lpZS;
      HRESULT hr = d3d_dev->GetDepthStencilSurface(&lpZS);
      if(SUCCEEDED(hr)){
         D3DSURFACE_DESC desc;
         hr = lpZS->GetDesc(&desc);
         if(SUCCEEDED(hr)){
            switch(desc.Format){
            case D3DFMT_D15S1:
            case D3DFMT_D24S8:
            case D3DFMT_D24X4S4:
               drv_flags |= DRVF_HASSTENCIL;
               break;
            }
         }
         lpZS->Release();
      }
   }

#ifdef DEBUG_SIM_LOWVIDMEM
   {
      S_pixelformat pf;
      memset(&pf, 0, sizeof(pf));
      pf.bytes_per_pixel = 2;
      pf.r_mask = 0xf800;
      pf.g_mask = 0x07e0;
      pf.b_mask = 0x001f;
      int num_txts = DEBUG_SIM_LOWVIDMEM / (256*256*2);
      for(int i=0; i<num_txts; i++){
         PIImage img = GetGraphInterface()->CreateImage();
         if(img){
            bool b = img->Open(NULL,
               IMGOPEN_TEXTURE|IMGOPEN_VIDMEM|IMGOPEN_EMPTY|IMGOPEN_MIPMAP|IMGOPEN_HINTSTATIC,
               256, 256, &pf, 0);
            if(b)
               low_mem_textures.push_back(img);
            img->Release();
         }
      }
   }
#endif
                              //setup shader constants
   {
      static const S_vectorw consts(0.0f, 0.5f, 1.0f, 1.0f / 256.0f);
      SetVSConstant(VSC_CONSTANTS, &consts);
   }

   if(CanUsePixelShader()){
      static const S_vectorw c_zero(0, 0, 0, 0), c_one(1, 1, 1, 1);
      SetPSConstant(0, &c_zero);
      SetPSConstant(1, &c_one);
   }

   {
      IDirect3DSurface9 *surf;
      hr = d3d_dev->GetRenderTarget(0, &surf);
      CHECK_D3D_RESULT("GetRenderTarget", hr);
      curr_render_target.rt[0] = surf;
      surf->Release();

      hr = d3d_dev->GetDepthStencilSurface(&surf);
      CHECK_D3D_RESULT("GetDepthStencilSurface", hr);
      curr_render_target.zb = surf;
      surf->Release();
   }
   default_render_target = curr_render_target;

   {
                              //create dynamic vertex and index buffer
      dword d3d_usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
      D3DPOOL pool = D3DPOOL_DEFAULT;
#ifndef GL
      if(!IsDirectTransform())
         d3d_usage |= D3DUSAGE_SOFTWAREPROCESSING;
#endif
      {
         IDirect3DIndexBuffer9 *ib;
         hr = d3d_dev->CreateIndexBuffer(IB_TEMP_SIZE, d3d_usage, D3DFMT_INDEX16, pool, &ib, NULL);
         CHECK_D3D_RESULT("CreateIndexBuffer", hr);
         ib_temp = ib;
         ib->Release();
         ib_temp_base_index = 0;
      }
      {
         IDirect3DVertexBuffer9 *vb;
         hr = d3d_dev->CreateVertexBuffer(VB_TEMP_SIZE, d3d_usage, 0, pool, &vb, NULL);
         CHECK_D3D_RESULT("CreateVertexBuffer", hr);
         vb_temp = vb;
         vb->Release();
         vb_temp_base_index = 0;
      }
   }

#ifndef GL
   InitShadowResources();
#endif
   drv_flags2 &= ~DRVF2_CAN_RENDER_MIRRORS;
   if(NumSimultaneousTextures() >= 2 && (drv_flags&DRVF_SINGLE_PASS_MODULATE2X)){
      if(d3d_caps.MaxUserClipPlanes || CanUsePixelShader()){
         if(drv_flags&DRVF_HASSTENCIL){
            drv_flags2 |= DRVF2_CAN_RENDER_MIRRORS;
         }
      }
   }

#ifdef _DEBUG
                              //create profiler
   {
      C_cache ck_bitmap, ck_opacity, ck_project;
      if(OpenResource(GetHInstance(), "BINARY", "Font1", ck_bitmap) &&
         OpenResource(GetHInstance(), "BINARY", "Font1_a", ck_opacity) &&
         OpenResource(GetHInstance(), "BINARY", "font.txt", ck_project)){

         PC_poly_text pt;
         E_SPRITE_IMAGE_INIT_RESULT ir = CreatePolyText(this, &pt, ck_project, &ck_bitmap, &ck_opacity, TEXTMAP_NOMIPMAP | TEXTMAP_USE_CACHE);
         //TEXTMAP_TRANSP |
         if(ir==SPRINIT_OK){
            block_profiler = CreateBlockProfiler(GetGraphInterface(), pt, "I3D engine profiler", block_profile_names);
            pt->Release();

            block_profiler_scene = CreateScene();
            block_profiler_scene->Release();

            //block_profiler->SetMode(C_block_profiler::MODE_SELF);
         }
      }
   }
#endif

   d3d_dev->SetFVF(NULL);

#ifdef GL
   glBlendFunc(gl_srcblend = GL_ONE, gl_dstblend = GL_ZERO); CHECK_GL_RESULT("glBlendFunc");
   //glBlendEquation(GL_FUNC_ADD); CHECK_GL_RESULT("glBlendEquation");
   glDisable(GL_BLEND);

                              //we use different coordinate system, need to reverse depth buffer usage
   glDepthFunc(GL_GREATER); CHECK_GL_RESULT("glDepthFunc");
   glClearDepthf(0.0f); CHECK_GL_RESULT("glClearDepthf");
   glEnable(GL_DEPTH_TEST);
   //CHECK_GL_RESULT("!");

#endif

   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_driver::CreateDepthBuffer(dword sx, dword sy, const S_pixelformat &fmt, IDirect3DSurface9 **zb){

   HRESULT hr;
   D3DDISPLAYMODE disp_mode;
   hr = d3d_dev->GetDisplayMode(0, &disp_mode);
   CHECK_D3D_RESULT("GetDisplayMode", hr);

   D3DDEVICE_CREATION_PARAMETERS cp;
   hr = d3d_dev->GetCreationParameters(&cp);
   CHECK_D3D_RESULT("GetCreationParameters", hr);

   D3DFORMAT rt_fmt = ConvertI3DpfToD3Dpf(fmt);

   static const D3DFORMAT zb_fmts[] = {
      D3DFMT_D16,
      D3DFMT_D15S1,
      D3DFMT_D32,
      D3DFMT_D24S8,
      D3DFMT_D24X4S4,
      D3DFMT_D24X8,
      D3DFMT_UNKNOWN
   };
   *zb = NULL;
   for(int i=0; zb_fmts[i]!=D3DFMT_UNKNOWN; i++){
      hr = pD3D_driver->CheckDepthStencilMatch(cp.AdapterOrdinal, cp.DeviceType, disp_mode.Format, rt_fmt, zb_fmts[i]);
      if(SUCCEEDED(hr)){
         hr = d3d_dev->CreateDepthStencilSurface(sx, sy, zb_fmts[i], D3DMULTISAMPLE_NONE, 0, true, zb, NULL);
         if(SUCCEEDED(hr)){
            break;
         }
      }
   }
   return *zb ? I3D_OK : I3DERR_GENERIC;
}

//----------------------------
#ifndef GL
void I3D_driver::InitShadowResources(){

   HRESULT hr;
                              //init shadow rendering
   drv_flags &= ~DRVF_CANRENDERSHADOWS;
                              //multitexturing must be supported
   if(MaxSimultaneousTextures() >= 2)
   do{
                              //D3DPBLENDCAPS_INVSRCCOLOR must be reported
      if(!(d3d_caps.DestBlendCaps&D3DPBLENDCAPS_INVSRCCOLOR))
         break;

      const int SHD_RECT_SIZE = 128;

      I3D_RESULT ir = InitRenderTarget(rt_shadow,
         TEXTMAP_NOMIPMAP | //TEXTMAP_HINTDYNAMIC |
         TEXTMAP_RENDERTARGET | TEXTMAP_TRUECOLOR | TEXTMAP_NO_SYSMEM_COPY,
         SHD_RECT_SIZE, SHD_RECT_SIZE);
      if(I3D_FAIL(ir))
         break;

      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      {
                              //create clamp texture
         ct.flags = TEXTMAP_TRUECOLOR | TEXTMAP_NOMIPMAP;
         ct.size_x = 256;
         ct.size_y = 1;
         PI3D_texture tp;
         I3D_RESULT ir = CreateTexture(&ct, &tp);
         if(I3D_FAIL(ir))
            break;
         tp_shadow = tp;
         tp->Release();
         byte range[768];
         for(int i=0; i<256; i++){
            //byte b = 256 - i;
            byte b = byte(i);
            range[i*3+0] = range[i*3+1] = range[i*3+2] = b;
         }
         range[765] = range[766] = range[767] = 0;

         IDirect3DTexture9 *dst = tp->GetSysmemImage()->GetDirectXTexture();
         D3DLOCKED_RECT lrc;
         hr = dst->LockRect(0, &lrc, NULL, 0);
         CHECK_D3D_RESULT("LockRect", hr);
         if(FAILED(hr))
            break;
         C_rgb_conversion *rgbconv = GetGraphInterface()->CreateRGBConv();
         rgbconv->Init(*tp->GetSysmemImage()->GetPixelFormat());

         void *dstp = lrc.pBits;

         rgbconv->Convert(range, dstp,
            tp->SizeX1(), tp->SizeY1(),
            0,                //srcpitch
            lrc.Pitch,        //dstpitch
            3,                //srcbpp
            NULL,             //pal
            0,                //flags
            0);               //colorkey

         rgbconv->Release();

         hr = dst->UnlockRect(0);
         CHECK_D3D_RESULT("UnlockRect", hr);
         if(FAILED(hr))
            break;
      }

      HRESULT hr;
      hr = d3d_dev->CreateVertexBuffer(sizeof(S_blur_vertex) * 4 * NUM_SHADOW_BLUR_SAMPLES,
         //D3DUSAGE_DONOTCLIP |
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_DEFAULT, (IDirect3DVertexBuffer9**)&vb_blur, NULL);
      CHECK_D3D_RESULT("CreateVertexBuffer", hr);
      if(FAILED(hr))
         break;

      hr = d3d_dev->CreateIndexBuffer(sizeof(word) * 6 * NUM_SHADOW_BLUR_SAMPLES,
         //D3DUSAGE_DONOTCLIP |
         D3DUSAGE_WRITEONLY,
         D3DFMT_INDEX16, D3DPOOL_DEFAULT, (IDirect3DIndexBuffer9**)&ib_blur, NULL);
      CHECK_D3D_RESULT("CreateIndexBuffer", hr);
      if(FAILED(hr))
         break;
                              //fill-in blur buffers
      {
         S_blur_vertex *bv;
         word *indx;
         hr = vb_blur->Lock(0, 0, (void**)&bv, 0);
         CHECK_D3D_RESULT("Lock", hr);
         if(FAILED(hr))
            break;

         hr = ib_blur->Lock(0, 0, (void**)&indx, 0);
         CHECK_D3D_RESULT("Lock", hr);
         if(FAILED(hr)){
            vb_blur->Unlock();
            break;
         }

         struct S_blur_matrix{
            float du, dv;
            float amount;
         };
         static const S_blur_matrix bm_4[] = {
            {-.5f, -.5f, 4.0f / 16.0f},
            { .5f, -.5f, 4.0f / 16.0f},
            {-.5f,  .5f, 4.0f / 16.0f},
            { .5f,  .5f, 4.0f / 16.0f},
         };

         const float F5 = .7f;
         static const S_blur_matrix bm_5[] = {
            {-F5, -F5, .2f},
            { F5, -F5, .2f},
            {-F5,  F5, .2f},
            { F5,  F5, .2f},
            {  0,   0, .2f},
         };

         //const float F9a = 1.0f, F9b = .707f;
         const float F9a = 1.5f, F9b = 1.0f;
         //const float F9a = 2.0f, F9b = 1.41f;
         /*
         static const S_blur_matrix bm_9[] = {
            {-F9b, -F9b, 1.0f / 16.0f},
            {   0, -F9a, 2.0f / 16.0f},
            { F9b, -F9b, 1.0f / 16.0f},
            {-F9a,    0, 2.0f / 16.0f},
            {   0,    0, 4.0f / 16.0f},
            { F9a,    0, 2.0f / 16.0f},
            {-F9b,  F9b, 1.0f / 16.0f},
            {   0,  F9a, 2.0f / 16.0f},
            { F9b,  F9b, 1.0f / 16.0f},
         };
         */
         static const S_blur_matrix bm_9[] = {
            {-F9b, -F9b, 1.5f / 16.0f},
            {   0, -F9a, 2.0f / 16.0f},
            { F9b, -F9b, 1.5f / 16.0f},
            {-F9a,    0, 2.0f / 16.0f},
            {   0,    0, 2.0f / 16.0f},
            { F9a,    0, 2.0f / 16.0f},
            {-F9b,  F9b, 1.5f / 16.0f},
            {   0,  F9a, 2.0f / 16.0f},
            { F9b,  F9b, 1.5f / 16.0f},
         };

         const S_blur_matrix *bm = (NUM_SHADOW_BLUR_SAMPLES==9) ? bm_9 : bm_5;

         float textel_size = 1.0f / (float)rt_shadow->SizeX1();
                                       //add half-texel to correct filtering
         float half_texel = textel_size * .5f;

         for(int i=NUM_SHADOW_BLUR_SAMPLES; i--; bv += 4, indx += 6){
                              //make indicies
            indx[0] = word(i*4);
            indx[1] = word(indx[0] + 1);
            indx[2] = word(indx[0] + 2);
            indx[3] = word(indx[2]);
            indx[4] = word(indx[1]);
            indx[5] = word(indx[0] + 3);
                              //make vertices
            for(int j=4; j--; ){
               S_blur_vertex &v = bv[j];
               v.v.Zero();
               v.v.w = 1.0f;
               v.uv.x = half_texel;
               v.uv.y = half_texel;
               if(j&1){
                  v.v.x += rt_shadow.rt[1]->SizeX1();
                  v.uv.x += 1.0f;
               }
               if(j&2){
                  v.v.y += rt_shadow.rt[1]->SizeY1();
                  v.uv.y += 1.0f;
               }
               v.uv.x += textel_size * bm[i].du;
               v.uv.y += textel_size * bm[i].dv;
               v.diffuse = ((FloatToInt(255.0f * bm[i].amount)) << 24);
               //v.diffuse |= 0xffffffff;
            }
         }
         hr = vb_blur->Unlock();
         CHECK_D3D_RESULT("Unlock", hr);
         hr = ib_blur->Unlock();
         CHECK_D3D_RESULT("Unlock", hr);
      }
      drv_flags |= DRVF_CANRENDERSHADOWS;
   }while(false);

   if(!(drv_flags&DRVF_CANRENDERSHADOWS)){
      rt_shadow.Close();
      tp_shadow = NULL;
      vb_blur = NULL;
      ib_blur = NULL;
   }
}
#endif
//----------------------------

void I3D_driver::InitLMConvertor(){

                              //determine how much memory we may spend on light-maps
   {
      I3D_stats_video sv;
      GetStats(I3DSTATS_VIDEO, &sv);
                              //
      max_lm_memory = sv.txtmem_total/6;
      //max_lm_memory = sv.txtmem_free / 6;
   }

                              //find texture format for light-mapping (standard format)
   dword findtf_flags = 0;

   if(drv_flags2&DRVF2_LMTRUECOLOR){
                              //allow only if we have enough LM memory, use default depth otherwise
      if(max_lm_memory > 3*1024*1024)
         findtf_flags |= FINDTF_TRUECOLOR;
   }
                              //add apha channel due to special effects
   findtf_flags |= FINDTF_ALPHA;

   I3D_RESULT ir;
   ir = FindTextureFormat(pf_light_maps, findtf_flags);
   assert(I3D_SUCCESS(ir));

   {
                              //check if single-pass multitexturing possible
      drv_flags &= ~DRVF_SINGLE_PASS_MODULATE | DRVF_SINGLE_PASS_MODULATE2X;
#ifndef DEBUG_NO_SINGLEPASS_MODULATE
      if(MaxSimultaneousTextures() >= 2){

         PI3D_texture tp[2] = {NULL, NULL};
         I3D_CREATETEXTURE ct;
         ct.flags = 0;
         ct.flags |= TEXTMAP_NOMIPMAP;
         ct.size_x = 16;
         ct.size_y = 16;
         ct.flags |= TEXTMAP_USEPIXELFORMAT | TEXTMAP_HINTDYNAMIC;
         ct.file_name = NULL;

                              //base texture
         {
            S_pixelformat pf;
            FindTextureFormat(pf, 0);
            ct.pf = &pf;
            CreateTexture(&ct, &tp[0]);
         }
                              //light-map texture
         ct.pf = &pf_light_maps;
         CreateTexture(&ct, &tp[1]);
         if(tp[0] && tp[1]){
            //SetState(RS_MIPMAP, false);
            if(stage_txt_op[0]!=D3DTOP_SELECTARG1)
               d3d_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

            SetTexture1(0, tp[0]);
            SetTexture1(1, tp[1]);

            dword num_passes;
                              //check MODULATE
            //SetupTextureStage(1, D3DTOP_MODULATE);
            d3d_dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
            drv_flags &= ~DRVF_SINGLE_PASS_MODULATE;
            HRESULT hr = d3d_dev->ValidateDevice(&num_passes);
            if(SUCCEEDED(hr) && num_passes==1)
               drv_flags |= DRVF_SINGLE_PASS_MODULATE;

                              //check MODULATE2X
            //SetupTextureStage(1, D3DTOP_MODULATE2X);
            d3d_dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
            drv_flags &= ~DRVF_SINGLE_PASS_MODULATE2X;
            hr = d3d_dev->ValidateDevice(&num_passes);
            if(SUCCEEDED(hr) && num_passes==1)
               drv_flags |= DRVF_SINGLE_PASS_MODULATE2X;

            d3d_dev->SetTextureStageState(1, D3DTSS_COLOROP, stage_txt_op[1]);

            SetTexture1(0, NULL);
            SetTexture1(1, NULL);
#ifndef GL
            DisableTextureStage(1);
#endif
            if(stage_txt_op[0]!=D3DTOP_SELECTARG1)
               d3d_dev->SetTextureStageState(0, D3DTSS_COLOROP, stage_txt_op[0]);
         }
         if(tp[0]) tp[0]->Release();
         if(tp[1]) tp[1]->Release();
                              //todo: if this is really possible (try also with 3-linear filtering)
      }
#endif   //!DEBUG_NO_SINGLEPASS_MODULATE
   }

                              //init rgb convertor
   if(!rgbconv_lm)
      rgbconv_lm = GetGraphInterface()->CreateRGBConv();
   S_pixelformat pf = pf_light_maps;
   if(pf.flags&PIXELFORMAT_PALETTE){
      pf.r_mask = 0xe0;
      pf.g_mask = 0x1c;
      pf.b_mask = 0x03;
      pf.a_mask = 0x00;
      pf.flags &= ~PIXELFORMAT_PALETTE;

      pf_light_maps = pf;
   }else{
      static const dword bit_mask[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
                              //make all components same width, so that we don't have
                              // colorized gray scale
      int num = Min(CountBits(pf.r_mask), Min(CountBits(pf.g_mask), CountBits(pf.b_mask)));
      if(num<=8){
         dword mask = 0xff&bit_mask[num];
         pf.r_mask = Rotate(mask, FindLastBit(pf.r_mask)-7);
         pf.g_mask = Rotate(mask, FindLastBit(pf.g_mask)-7);
         pf.b_mask = Rotate(mask, FindLastBit(pf.b_mask)-7);
      }
   }
   rgbconv_lm->Init(pf);
}

//----------------------------

void I3D_driver::CloseD3DResources(){

#ifdef DEBUG_SIM_LOWVIDMEM
   low_mem_textures.clear();
#endif
   int i;

                              //call all registered callbacks
   for(i=reset_callbacks.size(); i--; ){
      reset_callbacks[i]->ResetCallback(RESET_RELEASE);
   }

   for(i=managed_textures.size(); i--; ){
      PI3D_texture tp = managed_textures[i];
      tp->Manage(I3D_texture::MANAGE_FREE);
   }
   texture_formats[0].clear();
   texture_formats[1].clear();

   for(i=d3d_caps.MaxTextureBlendStages; i--; )
      SetTexture1(i, NULL);

   /*
   {
      for(vs_set::const_iterator it = vs_cache.begin(); it!=vs_cache.end(); it++){
         HRESULT hr;
         hr = d3d_dev->DeleteVertexShader((*it).vs_handle);
         CHECK_D3D_RESULT("DeleteVertexShader", hr);
      }
   }
   */
   vs_cache.clear();
   vs_declarators.clear();
   vs_declarators_fvf.clear();
   /*
   {
      for(ps_set::const_iterator it = ps_cache.begin(); it!=ps_cache.end(); it++){
         HRESULT hr;
         hr = d3d_dev->DeletePixelShader((*it).shd_handle);
         CHECK_D3D_RESULT("DeletePixelShader", hr);
      }
   }
   */
   ps_cache.clear();

   SetIndices(NULL);

   ib_temp = NULL;
   vb_temp = NULL;
#ifndef GL
   vb_blur = NULL;
   ib_blur = NULL;
   rt_shadow.Close();
   tp_shadow = NULL;
#endif
   SetVertexShader(NULL);
   last_vs_decl = NULL;
   if(last_ps)
      SetPixelShader(NULL);
#ifndef GL
   SetNightVision(false);
#endif
   curr_render_target.Close();
   default_render_target.Close();

   vb_rectangle = NULL;
   vs_decl_rectangle = NULL;
   vb_clear = NULL;
   vb_particle = NULL;
   ib_particle = NULL;

#ifdef _DEBUG
   delete block_profiler;
   block_profiler = NULL;
   block_profiler_scene = NULL;
#endif

   ReleaseHWVertexBuffers();

   if(rgbconv_lm){
      rgbconv_lm->Release();
      rgbconv_lm = NULL;
   }

                              //release editing icons
   icon_materials.clear();
}

//----------------------------

void I3D_driver::EvictResources(){

   for(dword i=managed_textures.size(); i--; ){
      PI3D_texture tp = managed_textures[i];
      tp->Manage(I3D_texture::MANAGE_FREE);
   }
   ReleaseHWVertexBuffers();
}

//----------------------------

void I3D_driver::ReleaseHWVertexBuffers(){

#ifndef GL
                              //modify all references to HW vertex buffers
   for(dword i = hw_vb_list.size(); i--; ){
      I3D_dest_vertex_buffer *vb_d = hw_vb_list[i];
      vb_d->DestroyD3DVB();
   }
                              //now no allocated VBs and IBs may be in hw
#ifdef _DEBUG
   for(i=vb_manager_list.size(); i--; )
      assert(vb_manager_list[i]->GetPool() != D3DPOOL_DEFAULT);
   for(i=ib_manager_list.size(); i--; )
      assert(ib_manager_list[i]->GetPool() != D3DPOOL_DEFAULT);
#endif
                              //forget all references, they'll be recreated
   hw_vb_list.clear();
#endif
#ifdef GL
   for(int i=GL_PROGRAM_LAST; i--; ){
      gl_shader_programs[i] = NULL;
   }
#endif
   SetStreamSource(NULL, 0);
}

//----------------------------

dword I3D_driver::GraphCallback(dword msg, dword prm1, dword prm2){

   switch(msg){
   case CM_ACTIVATE:
      if(prm1){
         reapply_volume_count = 20;
      }
      break;

   case CM_RECREATE:
      switch(prm2){
      case 0:
         {
                              //release all hardware resources
            if(!pD3D_driver)
               return 0;
            CloseD3DResources();
         }
         break;
      case 1:                 //construct
         {
            InitD3DResources();
            RefreshAfterReset();
         }
         break;
      }
      break;
   }
   return 0;
}

//----------------------------

dword I3D_driver::GraphCallback_thunk(dword msg, dword par1, dword par2, void *context){

   PI3D_driver drv = (PI3D_driver)context;
   assert(drv);
   return drv->GraphCallback(msg, par1, par2);
}

//----------------------------

void I3D_driver::RefreshAfterReset(){


   SetViewport(I3D_rectangle(0, 0, init_data.lp_graph->Scrn_sx(), init_data.lp_graph->Scrn_sy()));

   int i;
                              //textures
   
   for(i=managed_textures.size(); i--; ){
      PI3D_texture tp = managed_textures[i];
                           //don't refresh LMap textures
      if(UnregLMTexture(tp)){
         tp->SetDirty();
         managed_textures[i] = managed_textures.back(); managed_textures.pop_back();
         continue;
      }
      tp->Reload();
   }
   lm_textures.clear();    //don't keep dirty LM textures

                              //call all registered callbacks
   for(i=reset_callbacks.size(); i--; ){
      reset_callbacks[i]->ResetCallback(RESET_RECREATE);
   }

                              //refresh lost state
   static const I3D_RENDERSTATE states[] = {
      RS_LINEARFILTER, RS_MIPMAP, RS_USEZB, RS_DITHER,
      RS_FOG, RS_WIREFRAME, 
   };
   for(i=0; i<sizeof(states)/sizeof(I3D_RENDERSTATE); i++){
      bool b = GetState(states[i]);
      SetState(states[i], !b);
      SetState(states[i], b);
   }
}

//----------------------------

IDirect3DVertexDeclaration9 *I3D_driver::GetVSDeclaration(const C_vector<D3DVERTEXELEMENT9> &els){

   t_vs_declarators::iterator it = vs_declarators.find((const C_vs_decl_key&)els);
   if(it!=vs_declarators.end())
      return it->second;

   IDirect3DVertexDeclaration9 *vd;
   HRESULT hr;
   hr = d3d_dev->CreateVertexDeclaration(&els.front(), &vd);
   CHECK_D3D_RESULT("CreateVertexDeclaration", hr);
   vs_declarators[(const C_vs_decl_key&)els] = vd;
   vd->Release();
   return vd;
}

//----------------------------

IDirect3DVertexDeclaration9 *I3D_driver::GetVSDeclaration(dword fvf){

   t_vs_declarators_fvf::iterator it = vs_declarators_fvf.find(fvf);
   if(it!=vs_declarators_fvf.end())
      return it->second;

   C_vector<D3DVERTEXELEMENT9> els;
   byte offs = 0;
                              //position
   switch(fvf&D3DFVF_POSITION_MASK){
   case D3DFVF_XYZ:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
      offs += sizeof(S_vector);
      break;
   case D3DFVF_XYZRHW:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION));
      offs += sizeof(S_vectorw);
      break;
   case D3DFVF_XYZB1:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
      offs += sizeof(S_vector);
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_BLENDWEIGHT));
      offs += sizeof(float) * 1;
      break;
   case D3DFVF_XYZB2:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
      offs += sizeof(S_vector);
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_BLENDWEIGHT));
      offs += sizeof(float) * 2;
      break;
   case D3DFVF_XYZB3:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
      offs += sizeof(S_vector);
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_BLENDWEIGHT));
      offs += sizeof(float) * 3;
      break;
   case D3DFVF_XYZB4:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
      offs += sizeof(S_vector);
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_BLENDWEIGHT));
      offs += sizeof(float) * 4;
      break;
   default:
      assert(0);
   }
   
   if(fvf&D3DFVF_PSIZE){
      assert(0);              //todo
   }
   if(fvf&D3DFVF_NORMAL){
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
      offs += sizeof(S_vector);
   }
   if(fvf&D3DFVF_DIFFUSE){
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR));
      offs += sizeof(dword);
   }
   if(fvf&D3DFVF_SPECULAR){
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1));
      offs += sizeof(dword);
   }
   dword num_txt = (fvf&D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
   switch(num_txt){
   case 0:
      break;
   case 1:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
      offs += sizeof(S_vector2);
      break;
   case 2:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 0));
      offs += sizeof(S_vector2) * 2;
      break;
   case 3:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 0));
      offs += sizeof(S_vector2) * 2;
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
      offs += sizeof(S_vector2);
      break;
   case 4:
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 0));
      offs += sizeof(S_vector2) * 2;
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 2));
      offs += sizeof(S_vector2) * 2;
      break;
   default:
      assert(0);
   }
   if(fvf&D3DFVF_TEXTURE_SPACE){
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 1)); offs += sizeof(S_vector);
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 2)); offs += sizeof(S_vector);
      els.push_back(S_vertex_element(offs, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 3)); offs += sizeof(S_vector);
   }
   els.push_back(D3DVERTEXELEMENT9_END);

   IDirect3DVertexDeclaration9 *vd;
   HRESULT hr;
   hr = d3d_dev->CreateVertexDeclaration(&els.front(), &vd);
   CHECK_D3D_RESULT("CreateVertexDeclaration", hr);
   vs_declarators_fvf[fvf] = vd;
   vd->Release();
   return vd;
}

//----------------------------

PI3D_material I3D_driver::CreateMaterial(){

   PI3D_material mp = new I3D_material(this);
   return mp;
}

//----------------------------

PI3D_scene CreateScene(PI3D_driver);

PI3D_scene I3D_driver::CreateScene(){

   return ::CreateScene(this);
}

//----------------------------

I3D_RESULT I3D_driver::EnumVisualTypes(I3D_ENUMVISUALPROC *ep, dword user) const{

   for(int i=0; i<num_visual_plugins; i++){
      I3DENUMRET er = (*ep)(visual_plugins[i].visual_type, visual_plugins[i].friendly_name, user);
      if(er==I3DENUMRET_CANCEL) return I3DERR_CANCELED;
   }
   return I3D_OK;
}

//----------------------------

PI3D_mesh_base I3D_driver::CreateMesh(dword vcf){

   dword fvf = ConvertFlags(vcf);
   if(fvf==-1)
      return NULL;
   return new I3D_mesh_base(this, fvf);
}
//----------------------------

PI3D_animation_base I3D_driver::CreateAnimation(I3D_ANIMATION_TYPE type){

   switch(type){
   case I3DANIM_KEYFRAME:
      return new I3D_keyframe_anim(this);
   case I3DANIM_POSE:
      return new I3D_anim_pose(this);
   }
   return NULL;
}

//----------------------------

PI3D_animation_set I3D_driver::CreateAnimationSet(){
   
   return new I3D_animation_set(this);
}

//----------------------------

bool I3D_driver::FindBitmap(const char *fname, C_str &out_name, dword &sx1, dword &sy1, byte &bpp1, bool is_cubemap) const{

   dword &last_maps_dir = last_dir_index[I3DDIR_MAPS];
   const C_vector<C_str> &mdir = dirs[I3DDIR_MAPS];
   dword id1;
   if(last_maps_dir<mdir.size()){
      out_name = C_fstr("%s\\%s", (const char*)mdir[last_maps_dir], fname);
      if(is_cubemap)
         out_name = MakeCubicTextureName(out_name, 0);
      if(GetGraphInterface()->GetImageInfo(out_name, sx1, sy1, bpp1, id1)){
         if(is_cubemap)
            out_name = C_fstr("%s\\%s", (const char*)mdir[last_maps_dir], fname);
         return true;
      }
   }
   for(dword i=0; i<mdir.size(); i++){
      if(i==last_maps_dir)
         continue;
      out_name = C_fstr("%s\\%s", (const char*)mdir[i], fname);
      if(is_cubemap)
         out_name = MakeCubicTextureName(out_name, 0);
      if(GetGraphInterface()->GetImageInfo(out_name, sx1, sy1, bpp1, id1)){
         last_maps_dir = i;
         if(is_cubemap)
            out_name = C_fstr("%s\\%s", (const char*)mdir[i], fname);
         return true;
      }
   }
   return false;
}

//----------------------------
                              //internal

I3D_RESULT I3D_driver::CreateTexture1(const I3D_CREATETEXTURE &ct, PI3D_texture *tp1,
   PI3D_LOAD_CB_PROC cb_load, void *cb_context, C_str *err_msg){

   if(ct.flags&TEXTMAP_PROCEDURAL){
      PI3D_procedural_base pb;
      I3D_RESULT ir = GetProcedural(ct.file_name, cb_load, cb_context, &pb, ct.proc_data);
      if(I3D_SUCCESS(ir)){
         if(pb->GetID()!=PROCID_TEXTURE){
            pb->Release();
            pb = NULL;
         }
         *tp1 = (I3D_procedural_texture*)pb;
      }
      return ir;
   }

   if(ct.flags&(TEXTMAP_DIFFUSE|TEXTMAP_OPACITY|TEXTMAP_EMBMMAP|TEXTMAP_NORMALMAP)){
                              //check if such texture already exists, re-use
      for(dword i=managed_textures.size(); i--; ){
         if(managed_textures[i]->Match(ct)){
            *tp1 = managed_textures[i];
            (*tp1)->AddRef();
            return I3D_OK;
         }
      }
   }

   PI3D_texture tp = new I3D_texture(const_cast<PI3D_driver>(this));
   if(!tp)
      return I3DERR_OUTOFMEM;

   I3D_RESULT ir;
#if 1
   ir = tp->Open(ct, cb_load, cb_context, err_msg);
#else
   ir = I3D_OK;
   BegProf();
   for(dword i=10; i--; )
      ir = tp->Open(ct, cb_load, cb_context, err_msg);
   PRINT(EndProf());
#endif
   if(I3D_FAIL(ir)){
      tp->Release();
      *tp1 = NULL;
      return ir;
   }
   *tp1 = tp;
   return ir;
}

//----------------------------

void I3D_driver::RegisterManagedTexture(PI3D_texture tp){

   //assert(find(managed_textures.begin(), managed_textures.end(), tp)==managed_textures.end());
   managed_textures.push_back(tp);
}

//----------------------------

void I3D_driver::UnregisterManagedTexture(PI3D_texture tp){

   //int i = VectorIndex(managed_textures, tp);
   for(int i=managed_textures.size(); i--; ){
      if(managed_textures[i]==tp)
         break;
   }
   //assert(i!=-1);           //lm textures hit this... why?
   if(i!=-1){
      managed_textures[i] = managed_textures.back(); managed_textures.pop_back();
   }
   for(i=d3d_caps.MaxTextureBlendStages; i--; ){
      if(last_texture[i]==tp)
         SetTexture1(i, NULL);
   }
}

//----------------------------
#ifndef GL
bool I3D_driver::AllocVertexBuffer(dword fvf_flags, D3DPOOL mem_pool, dword d3d_usage, dword num_verts,
   IDirect3DVertexBuffer9 **vb_ret, dword *beg_indx_ret, I3D_dest_vertex_buffer *vb_d, bool allow_cache){

#ifdef DEBUG_VB_ALLOC
   static dword vb_alloc_id(0);
#endif
                              //if, for some reason, FVF is XYZRHW (transformed),
                              // force include clipping info
   if((fvf_flags&D3DFVF_POSITION_MASK)==D3DFVF_XYZRHW)
      d3d_usage &= ~D3DUSAGE_DONOTCLIP;

                              //find VB manager which would be suitable
   for(int i=vb_manager_list.size(); i--; ){
      if(vb_manager_list[i]->GetFVFlags()==fvf_flags && vb_manager_list[i]->GetUsage()==d3d_usage &&
         vb_manager_list[i]->GetPool()==mem_pool){
         break;
      }
   }

   if(i==-1){
                              //alloc new
      dword size_of_element = GetSizeOfVertex(fvf_flags);
      C_RB_manager<IDirect3DVertexBuffer9> *mgr = new C_RB_manager<IDirect3DVertexBuffer9>(mem_pool, d3d_usage, size_of_element, fvf_flags);
      vb_manager_list.push_back(mgr);
#ifdef DEBUG_VB_ALLOC
      mgr->SetAllocID(++vb_alloc_id);
#endif
      mgr->Release();         //smart pointer's keeping ref
      i = vb_manager_list.size() - 1;
   }
   C_RB_manager<IDirect3DVertexBuffer9> *mgr = vb_manager_list[i];
   bool b = mgr->Allocate(num_verts, this, vb_ret, beg_indx_ret, allow_cache);
   if(!b){
      if(mem_pool!=D3DPOOL_DEFAULT){
         assert(0);
         return false;
      }
                              //try to release oldest VBs
                              //1. sort VBs by time when they wer last used
      C_sort_list<I3D_dest_vertex_buffer*> slist;
      slist.Reserve(hw_vb_list.size());
      for(int i=hw_vb_list.size(); i--; ){
         dword t = render_time - hw_vb_list[i]->GetLastRenderTime();
         if(t)
            slist.Add(hw_vb_list[i], t);
      }
      slist.Sort();
                              //2. step through oldest, try to release and alloc again
      for(i=slist.Count(); i--; ){
         I3D_dest_vertex_buffer *vb = slist[i];
         vb->DestroyD3DVB();
                              //try to alloc now
         b = mgr->Allocate(num_verts, this, vb_ret, beg_indx_ret, allow_cache);
         if(b)
            break;
      }


                              //try again in system mem
      if(i==-1){
         DEBUG("Failed to alloc vidmemory VB, using sysmem.", 2);
         return AllocVertexBuffer(fvf_flags, D3DPOOL_SYSTEMMEM, d3d_usage, num_verts, vb_ret, beg_indx_ret, vb_d, allow_cache);
      }
   }

#ifdef DEBUG_VB_ALLOC
   if(BREAK_ALLOC_VB_ID && (mgr]->GetAllocID() == BREAK_ALLOC_VB_ID)){
      if(*beg_indx_ret == BREAK_ALLOC_VERTEX_ID){
         assert(("used vb_alloc on 460", 0));
      }
   }
#endif
                              //keep reference of pointer
   if(//mem_pool == D3DPOOL_DEFAULT &&
      vb_d){
      hw_vb_list.push_back(vb_d);
      /*
      hw_vb_list.push_back(S_VB_use_ref());
      S_VB_use_ref &vbr = hw_vb_list.back();
      vbr.vb = *vb_ret;
      vbr.vertex_index = *beg_indx_ret;
      vbr.ref = vb_ret;
      */
   }
   return b;
}

//----------------------------

bool I3D_driver::FreeVertexBuffer(IDirect3DVertexBuffer9 *vb, dword vertex_indx, I3D_dest_vertex_buffer *vb_d){

                              
   for(int i=vb_manager_list.size(); i--; ){
      C_RB_manager<IDirect3DVertexBuffer9> *vb_mgr = vb_manager_list[i];

      if(vb_mgr->Free(vb, vertex_indx)){
                              //check pool, remove hw vb from internal list
         if(//vb_mgr->GetPool() == D3DPOOL_DEFAULT &&
            vb_d){
            I3D_dest_vertex_buffer **vb_ref = (&hw_vb_list.back())+1;
            int i;
            for(i=hw_vb_list.size(); --vb_ref, i--; ){
               if(*vb_ref == vb_d){
                  hw_vb_list[i] = hw_vb_list.back(); hw_vb_list.pop_back();
                  break;
               }
            }
            assert(i!=-1);
         }

                              //destroy empty managers
         if(vb_mgr->IsEmpty()){

            vb_manager_list[i] = vb_manager_list.back(); vb_manager_list.pop_back();
                              //forget cache
            if(last_stream0_vb==vb)
               last_stream0_vb = NULL;
         }
         return true;
      }
   }
   assert(0);
   return false;
}

//----------------------------

bool I3D_driver::AllocIndexBuffer(D3DPOOL mem_pool, dword d3d_usage, dword num_faces,
   IDirect3DIndexBuffer9 **ib_ret, dword *beg_indx_ret){

   //return ib_manager.Allocate(num_faces, this, ib_ret, beg_indx_ret);

                              //find VB manager which would be suitable
   for(int i=ib_manager_list.size(); i--; ){
      C_RB_manager<IDirect3DIndexBuffer9> *ib = ib_manager_list[i];
      if(ib->GetUsage()==d3d_usage && ib->GetPool()==mem_pool)
         break;
   }

   if(i==-1){
                              //alloc new
      C_RB_manager<IDirect3DIndexBuffer9> *mgr = new C_RB_manager<IDirect3DIndexBuffer9>(mem_pool, d3d_usage, sizeof(I3D_triface));
      ib_manager_list.push_back(mgr);
      mgr->Release();         //smart pointer's keeping ref
      i = ib_manager_list.size() - 1;
   }
   bool b = ib_manager_list[i]->Allocate(num_faces, this, ib_ret, beg_indx_ret);
   assert(b);

                              //keep reference of pointer
   if(mem_pool == D3DPOOL_DEFAULT){
      assert(0);
      /*
      hw_vb_list.push_back(S_VB_use_ref());
      S_VB_use_ref &vbr = hw_vb_list.back();
      vbr.vb = *vb_ret;
      vbr.vertex_index = *beg_indx_ret;
      vbr.ref = vb_ret;
      */
   }
   return b;
}

//----------------------------

bool I3D_driver::FreeIndexBuffer(IDirect3DIndexBuffer9 *ib, dword base_index){

   //return ib_manager.Free(ib, base_index);
   for(int i=ib_manager_list.size(); i--; ){
      C_RB_manager<IDirect3DIndexBuffer9> *ib_mgr = ib_manager_list[i];
      if(ib_mgr->Free(ib, base_index)){
                              //check pool, remove hw vb from internal list
         if(ib_mgr->GetPool() == D3DPOOL_DEFAULT){
            assert(0);
            /*
            S_VB_use_ref *vb_ref = hw_vb_list.end();
            for(int i=hw_vb_list.size(); --vb_ref, i--; ){
               //if(hw_vb_list[i].vb == vb && hw_vb_list[i].vertex_index == vertex_indx)
               if(vb_ref->vb == vb && vb_ref->vertex_index == vertex_indx)
                  break;
            }
            assert(i!=-1);
            if(i!=-1){
               hw_vb_list[i] = hw_vb_list.back(); hw_vb_list.pop_back();
            }
            */
         }
                              //destroy empty managers
         if(ib_mgr->IsEmpty()){
            ib_manager_list[i] = ib_manager_list.back(); ib_manager_list.pop_back();
                              //forget cache
            if(last_ib==ib)
               SetIndices(NULL);
         }
         return true;
      }
   }
   assert(0);
   return false;
}
#endif
//----------------------------

void I3D_driver::UpdateScreenViewport(const I3D_rectangle &rc1){

   I3D_rectangle rc = rc1;
   int rtsx, rtsy;
#ifndef GL
   if(drv_flags2&DRVF2_IN_NIGHT_VISION){
      int sx1 = GetGraphInterface()->Scrn_sx();
      int sy1 = GetGraphInterface()->Scrn_sy();

      rtsx = rt_night_vision->SizeX();
      rtsy = rt_night_vision->SizeY();

      rc.l = rc.l*rtsx/sx1;
      rc.r = rc.r*rtsx/sx1;
      rc.t = rc.t*rtsy/sy1;
      rc.b = rc.b*rtsy/sy1;
   }else
#endif
   {
      rtsx = GetGraphInterface()->Scrn_sx();
      rtsy = GetGraphInterface()->Scrn_sy();
   }
   rc.r = Min(rc.r, rtsx);
   rc.b = Min(rc.b, rtsy);

   UpdateViewport(rc);
}

//----------------------------

void I3D_driver::UpdateViewport(const I3D_rectangle &rc){

   D3DVIEWPORT9 vport;
   vport.X = rc.l;
   vport.Y = rc.t;
   vport.Width = rc.r - rc.l;
   vport.Height = rc.b - rc.t;
   vport.MinZ = 0.0f;
   vport.MaxZ = 1.0f;

   HRESULT hr;
   hr = d3d_dev->SetViewport(&vport);
   CHECK_D3D_RESULT("SetViewport", hr);
#ifdef GL
   HWND hwnd = (HWND)GetGraphInterface()->GetHWND();
   RECT wrc;
   GetClientRect(hwnd, &wrc);
   glViewport(rc.l, wrc.bottom-rc.b, rc.r - rc.l, rc.b - rc.t);
#endif
}

//----------------------------

I3D_RESULT I3D_driver::SetViewport(const I3D_rectangle &rc){

   if(rc.r<rc.l || rc.b<rc.t || dword(rc.r)>GetGraphInterface()->Scrn_sx() || dword(rc.b)>GetGraphInterface()->Scrn_sy())
      return I3DERR_INVALIDPARAMS;
   UpdateScreenViewport(rc);
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_driver::BeginScene(){

   if(!in_scene_count){
                              //clear last frame's resources
      HRESULT hr = d3d_dev->BeginScene();
      if(FAILED(hr)){
         CHECK_D3D_RESULT("BeginScene", hr);
         return I3DERR_GENERIC;
      }
      render_time = GetGraphInterface()->ReadTimer();
   }
   ++in_scene_count;
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_driver::EndScene(){

   if(!in_scene_count)
      return I3DERR_INVALIDPARAMS;
   if(!--in_scene_count){
#if defined _DEBUG
      if(block_profiler){
         block_profiler->PrepareForRender();
         block_profiler->Render(block_profiler_scene);
      }
#endif

      SetStreamSource(NULL, 0);
#ifndef GL
      if(!CanUsePixelShader()){
         DisableTextureStage(1);
      }else
#endif
      {
         for(dword stage=0; stage < MAX_TEXTURE_STAGES; stage++){
            if(!last_texture[stage])
               break;
            last_texture[stage] = NULL;
            d3d_dev->SetTexture(stage, NULL);
         }
         SetPixelShader(NULL);
      }

      HRESULT hr;
      hr = d3d_dev->EndScene();
      CHECK_D3D_RESULT("EndScene", hr);

      if(reapply_volume_count){
         if(!--reapply_volume_count){
                           //force to re-apply sound's volume (DirectSound bug)
            for(dword i=sounds.size(); i--; ){
               PI3D_sound snd = sounds[i];
               snd->ReapplyVolume(true);
            }
         }
      }
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_driver::Clear(dword color){

   dword clear_flags = D3DCLEAR_TARGET;
   if((drv_flags&DRVF_USEZB))
      clear_flags |= D3DCLEAR_ZBUFFER;
   if(GetFlags()&DRVF_HASSTENCIL)
      clear_flags |= D3DCLEAR_STENCIL;

   HRESULT hr;
   hr = d3d_dev->Clear(0, NULL, clear_flags, color, 1.0f, 0);
   CHECK_D3D_RESULT("Clear", hr);

   return SUCCEEDED(hr) ? I3D_OK : I3DERR_GENERIC;
}

//----------------------------

void I3D_driver::SetupBlendInternal(dword flags){

   last_blend_flags = flags;

   bool alpha = (flags!=I3DBLEND_OPAQUE);

   HRESULT hr;
   dword src, dst;

   switch(flags){
   case I3DBLEND_OPAQUE:
      src = blend_table[0][D3DBLEND_ONE];
      dst = blend_table[1][D3DBLEND_ZERO];
      break;
   case I3DBLEND_ALPHABLEND:
      src = blend_table[0][D3DBLEND_SRCALPHA];
      dst = blend_table[1][D3DBLEND_INVSRCALPHA];
      break;
   case I3DBLEND_ADD:
      //src = blend_table[0][D3DBLEND_ONE];
      src = blend_table[0][D3DBLEND_SRCALPHA];
      dst = blend_table[1][D3DBLEND_ONE];
      break;
   case I3DBLEND_ADDALPHA:
      src = blend_table[0][D3DBLEND_SRCALPHA];
      dst = blend_table[1][D3DBLEND_ONE];
      break;
   case I3DBLEND_INVMODULATE:
      src = D3DBLEND_ZERO;
      dst = D3DBLEND_INVSRCCOLOR;
      break;
   case I3DBLEND_MODULATE2X:
      src = D3DBLEND_DESTCOLOR;
      dst = D3DBLEND_SRCCOLOR;
      break;
      /*
   case I3DBLEND_MODULATE:
      src = D3DBLEND_ZERO;
      dst = D3DBLEND_SRCCOLOR;
      break;
   case I3DBLEND_INVMODULATE2X:
      src = D3DBLEND_DESTCOLOR;
      dst = D3DBLEND_INVSRCCOLOR;
      break;
   case I3DBLEND_TEST:
      src = D3DBLEND_SRCALPHA;
      dst = D3DBLEND_INVSRCALPHA;
      break;
      */
   default:
      assert(0);
      src = D3DBLEND_ZERO;
      dst = D3DBLEND_ZERO;
   }

   if(!alpha){
      if(alpha_blend_on){
         alpha_blend_on = false;
         hr = d3d_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
         CHECK_D3D_RESULT("SetRenderState", hr);
#ifdef GL
         glDisable(GL_BLEND);
#endif
      }
   }else{
      if(!alpha_blend_on){
         alpha_blend_on = true;
         hr = d3d_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
         CHECK_D3D_RESULT("SetRenderState", hr);
#ifdef GL
         glEnable(GL_BLEND);
#endif
      }
   }
   if(srcblend != src){
      hr = d3d_dev->SetRenderState(D3DRS_SRCBLEND,  srcblend=(D3DBLEND)src);
      CHECK_D3D_RESULT("SetRenderState", hr);
   }
   if(dstblend != dst){
      hr = d3d_dev->SetRenderState(D3DRS_DESTBLEND, dstblend=(D3DBLEND)dst);
      CHECK_D3D_RESULT("SetRenderState", hr);
   }
#ifdef GL
   switch(flags){
   default:
      //assert(0);
   case I3DBLEND_OPAQUE:
      src = GL_ONE;
      dst = GL_ZERO;
      break;
   case I3DBLEND_ALPHABLEND:
      src = GL_SRC_ALPHA;
      dst = GL_ONE_MINUS_SRC_ALPHA;
      break;
   case I3DBLEND_ADD:
      //src = blend_table[0][D3DBLEND_ONE];
      src = GL_SRC_ALPHA;
      dst = GL_ONE;
      break;
   case I3DBLEND_ADDALPHA:
      src = GL_SRC_ALPHA;
      dst = GL_ONE;
      break;
      /*
   case I3DBLEND_INVMODULATE:
      src = GL_ZERO;
      dst = GL_ONE_MINUS_SRC_COLOR;
      break;
   case I3DBLEND_MODULATE2X:
      src = GL_DST_COLOR;
      dst = GL_SRC_COLOR;
      break;
   case I3DBLEND_MODULATE:
      src = GL_ZERO;
      dst = GL_SRC_COLOR;
      break;
   case I3DBLEND_INVMODULATE2X:
      src = GL_DST_COLOR;
      dst = GL_ONE_MINUS_SRC_COLOR;
      break;
   case I3DBLEND_TEST:
      src = GL_SRC_ALPHA;
      dst = GL_ONE_MINUS_SRC_ALPHA;
      break;
      */
   }

   if(gl_srcblend != src || gl_dstblend != dst){
      glBlendFunc(gl_srcblend=src, gl_dstblend=dst); CHECK_GL_RESULT("glBlendFunc");
   }
#endif
}

//----------------------------
#ifndef GL
void I3D_driver::SetClippingPlane(const S_plane &pl, const S_matrix &_m_view_proj_hom){

                              //choose technique
#ifndef DEBUG_NO_REAL_CLIP_PLANES
   if(d3d_caps.MaxUserClipPlanes){
      S_matrix m = _m_view_proj_hom;
      m.Invert();
      m.Transpose();

      S_plane c_clip_plane;
      *(S_vectorw*)&c_clip_plane = (*(S_vectorw*)&pl) * m;
      c_clip_plane.Invert();

      HRESULT hr;
      hr = d3d_dev->SetClipPlane(0, (const float*)&c_clip_plane);
      CHECK_D3D_RESULT("SetClipPlane", hr);
      hr = d3d_dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 1);
      CHECK_D3D_RESULT("SetRenderState", hr);
   }else
#endif//!DEBUG_NO_REAL_CLIP_PLANES
   if(CanUsePixelShader()){
      drv_flags2 |= DRVF2_TEXCLIP_ON;
      pl_clip = -pl;
      num_available_txt_stages = MaxSimultaneousTextures() - 1;
   }else{
      assert(0);
   }
}

//----------------------------

void I3D_driver::DisableClippingPlane(){

#ifndef DEBUG_NO_REAL_CLIP_PLANES
   if(d3d_caps.MaxUserClipPlanes){
      HRESULT hr;
      hr = d3d_dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
      CHECK_D3D_RESULT("SetRenderState", hr);
   }else
#endif//!DEBUG_NO_REAL_CLIP_PLANES
   if(CanUsePixelShader()){
      num_available_txt_stages = MaxSimultaneousTextures();
      drv_flags2 &= ~DRVF2_TEXCLIP_ON;
   }else{
      assert(0);
   }
}
#endif
//----------------------------

I3D_RESULT I3D_driver::SetTexture(CPI3D_texture tp){

   SetTexture1(0, tp);
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_driver::SetState(I3D_RENDERSTATE st, dword value){

   HRESULT hr = D3D_OK;

   switch(st){
   case RS_LINEARFILTER:
      {
         bool is_on = (drv_flags&DRVF_LINFILTER);
         if((bool)value==is_on)
            break;
         drv_flags &= ~DRVF_LINFILTER;
         if(value)
            drv_flags |= DRVF_LINFILTER;

         D3DTEXTUREFILTERTYPE ftmin, ftmag;
         dword anisotropy = 1;
         switch(value){
         case 0: 
            ftmin = ftmag = D3DTEXF_POINT;
            min_filter[0] = min_filter[1] = ftmin;
            break;
         case 1: 
            {
               ftmin = D3DTEXF_LINEAR;
               ftmag = D3DTEXF_LINEAR;
#ifndef DEBUG_DISABLE_ANISOTROPY
                              //choose anisotropic filtering, if available
               if((drv_flags&DRVF_USE_ANISO_FILTER) &&
                  (d3d_caps.RasterCaps&D3DPRASTERCAPS_ANISOTROPY) &&
                  (d3d_caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC) &&
                  d3d_caps.MaxAnisotropy){

                  ftmin = D3DTEXF_ANISOTROPIC;
                  anisotropy = d3d_caps.MaxAnisotropy;
               }
#endif//DEBUG_DISABLE_ANISOTROPY
               min_filter[0] = D3DTEXF_LINEAR;
               min_filter[1] = ftmin;
            }
            break;
         default:
            return I3DERR_UNSUPPORTED;
         }
         HRESULT hr;
         for(dword i=0; i<d3d_caps.MaxTextureBlendStages; i++){
            D3DTEXTUREFILTERTYPE minf = min_filter[stage_anisotropy[i]];
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_MINFILTER, stage_minf[i] = minf);
            CHECK_D3D_RESULT("SetSamplerState", hr);
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_MAGFILTER, ftmag);
            CHECK_D3D_RESULT("SetSamplerState", hr);
            if(curr_anisotropy!=anisotropy){
               hr = d3d_dev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, anisotropy);
               CHECK_D3D_RESULT("SetSamplerState", hr);
            }
         }
         curr_anisotropy = anisotropy;
#ifdef GL
         for(int i=managed_textures.size(); i--; ){
            I3D_texture *tp = managed_textures[i];
            dword tid = tp->GetGlId();
            if(tid){
               glBindTexture(GL_TEXTURE_2D, tid);
               glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, value ? GL_LINEAR : GL_NEAREST);
            }
         }
#endif
      }
      break;

   case RS_ANISO_FILTERING:
      {
         drv_flags &= ~DRVF_USE_ANISO_FILTER;
         if(value)
            drv_flags |= DRVF_USE_ANISO_FILTER;
                              //apply change
         bool filter_on = (drv_flags&DRVF_LINFILTER);
         if(filter_on){
            drv_flags &= ~DRVF_LINFILTER;
            SetState(RS_LINEARFILTER, true);
         }
      }
      break;

   case RS_MIPMAP:
      {
         bool is_on = (drv_flags&DRVF_MIPMAP);
         if((bool)value==is_on)
            break;

         if(value)
            drv_flags |= DRVF_MIPMAP;
         else
            drv_flags &= ~DRVF_MIPMAP;

         D3DTEXTUREFILTERTYPE ft;
         if(!value)
            ft = D3DTEXF_NONE;
         else
            ft = D3DTEXF_LINEAR;
         for(dword i=0; i<d3d_caps.MaxTextureBlendStages; i++){
            hr = d3d_dev->SetSamplerState(i, D3DSAMP_MIPFILTER, ft);
            CHECK_D3D_RESULT("SetSamplerState", hr);
         }
#ifdef GL
         for(int i=managed_textures.size(); i--; ){
            I3D_texture *tp = managed_textures[i];
            dword tid = tp->GetGlId();
            if(tid && (tp->GetTxtFlags()&TXTF_MIPMAP)){
               glBindTexture(GL_TEXTURE_2D, tid);
               glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, value ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
            }
         }
#endif
      }
      break;

   case RS_LOADMIPMAP:
      drv_flags &= ~DRVF_LOADMIPMAP;
      if(value) drv_flags |= DRVF_LOADMIPMAP;
      break;

      /*
   case RS_POINTFOG:
      drv_flags &= ~DRVF_POINTFOG;
      if(value) drv_flags |= DRVF_POINTFOG;
      break;
      */

   case RS_ENVMAPPING:
      {
         bool on = (value&1);
         if(bool(drv_flags2&DRVF2_ENVMAPPING) != on){
            drv_flags2 &= ~DRVF2_ENVMAPPING;
            if(on) drv_flags2 |= DRVF2_ENVMAPPING;
            if(value&0x80000000)
               ReleaseHWVertexBuffers();
         }
      }
      break;

   case RS_DETAILMAPPING:
      {
         bool on = (value&1);
         if(bool(drv_flags2&DRVF2_DETAILMAPPING) != on){
            drv_flags2 &= ~DRVF2_DETAILMAPPING;
            if(on) drv_flags2 |= DRVF2_DETAILMAPPING;
            if(value&0x80000000)
               ReleaseHWVertexBuffers();
         }
      }
      break;

   case RS_USE_EMBM:
      {
         bool on = (value&1);
         if(bool(drv_flags2&DRVF2_USE_EMBM) != on){
            drv_flags2 &= ~DRVF2_USE_EMBM;
            if(on) drv_flags2 |= DRVF2_USE_EMBM;
            if(value&0x80000000)
               ReleaseHWVertexBuffers();
         }
      }
      break;

   case RS_TEXTURECOMPRESS:
      drv_flags &= ~DRVF_TEXTURECOMPRESS;
      if(value) drv_flags |= DRVF_TEXTURECOMPRESS;
      break;

   case RS_DRAW_COL_TESTS:
      drv_flags2 &= ~DRVF2_DRAWCOLTESTS;
      if(value) drv_flags2 |= DRVF2_DRAWCOLTESTS;
      break;

   case RS_USEZB:
      {
         bool is_on = (drv_flags&DRVF_USEZB);
         bool on = value;
         if(is_on!=on){
            if(on) drv_flags |= DRVF_USEZB;
            else drv_flags &= ~DRVF_USEZB;
            D3DZBUFFERTYPE zb_type;
            if(!on){
               zb_type = D3DZB_FALSE;
            }else{
               if((init_data.flags&I3DINIT_WBUFFER) &&
                  (d3d_caps.RasterCaps&D3DPRASTERCAPS_WBUFFER)){

                  zb_type = D3DZB_USEW;
               }else{
                  zb_type = D3DZB_TRUE;
               }
            }
            hr = d3d_dev->SetRenderState(D3DRS_ZENABLE, zb_type);
            CHECK_D3D_RESULT("SetRenderState", hr);
         }
      }
      break;

   case RS_CLEAR:
      drv_flags &= ~DRVF_CLEAR;
      if(value) drv_flags |= DRVF_CLEAR;
      break;

   case RS_DITHER:
      {
         bool is_on = (drv_flags&DRVF_DITHER);
         if((bool)value==is_on)
            break;

         drv_flags &= ~DRVF_DITHER;
         if(value)
            drv_flags |= DRVF_DITHER;

         hr = d3d_dev->SetRenderState(D3DRS_DITHERENABLE, value);
         CHECK_D3D_RESULT("SetRenderState", hr);
#ifdef GL
         value ? glEnable(GL_DITHER) : glDisable(GL_DITHER);
#endif
      }
      break;

   case RS_WIREFRAME:
      {
         bool is_on = (drv_flags&DRVF_WIREFRAME);
         if((bool)value==is_on)
            break;

         drv_flags &= ~DRVF_WIREFRAME;
         if(value)
            drv_flags |= DRVF_WIREFRAME;

         hr = d3d_dev->SetRenderState(D3DRS_FILLMODE, value ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
      break;

   case RS_DRAWBOUNDBOX:
      drv_flags &= ~DRVF_DRAWBBOX;
      if(value) drv_flags |= DRVF_DRAWBBOX;
      break;

   case RS_DRAWHRBOUNDBOX:
      drv_flags &= ~DRVF_DRAWHRBBOX;
      if(value) drv_flags |= DRVF_DRAWHRBBOX;
      break;

   case RS_DRAWLINKS:
      drv_flags &= ~DRVF_DRAWHRLINK;
      if(value) drv_flags |= DRVF_DRAWHRLINK;
      break;

   case RS_DRAWPORTALS:
      drv_flags &= ~DRVF_DRAWPORTALS;
      if(value) drv_flags |= DRVF_DRAWPORTALS;
      break;

   case RS_DRAWSECTORS:
      drv_flags &= ~DRVF_DRAWSECTORS;
      if(value) drv_flags |= DRVF_DRAWSECTORS;
      break;

   case RS_TEXTUREDITHER:
      drv_flags &= ~DRVF_TEXTDITHER;
      if(value) drv_flags |= DRVF_TEXTDITHER;
      break;

   case RS_DRAWVISUALS:
      drv_flags &= ~DRVF_DRAWVISUALS;
      if(value) drv_flags |= DRVF_DRAWVISUALS;
      break;

   case RS_FOG:
      drv_flags &= ~DRVF_USEFOG;
      if(value) drv_flags |= DRVF_USEFOG;
      break;
#ifndef GL
   case RS_USESHADOWS:
      drv_flags &= ~DRVF_USESHADOWS;
      if(value) drv_flags |= DRVF_USESHADOWS;
      break;

   case RS_DEBUGDRAWSHADOWS:
      drv_flags &= ~DRVF_DEBUGDRAWSHADOWS;
      if(value) drv_flags |= DRVF_DEBUGDRAWSHADOWS;
      break;

   case RS_DEBUGDRAWSHDRECS:
      drv_flags &= ~DRVF_DEBUGDRAWSHDRECS;
      if(value) drv_flags |= DRVF_DEBUGDRAWSHDRECS;
      break;
#endif
   case RS_DEBUGDRAWSTATIC:
      drv_flags2 &= ~DRVF2_DEBUGDRAWSTATIC;
      if(value) drv_flags2 |= DRVF2_DEBUGDRAWSTATIC;
      break;

   case RS_DRAWVOLUMES:
      drv_flags2 &= ~DRVF2_DRAWVOLUMES;
      if(value) drv_flags2 |= DRVF2_DRAWVOLUMES;
      break;

   case RS_DRAWLIGHTS:
      drv_flags2 &= ~DRVF2_DRAWLIGHTS;
      if(value) drv_flags2 |= DRVF2_DRAWLIGHTS;
      break;

   case RS_DRAWSOUNDS:
      drv_flags2 &= ~DRVF2_DRAWSOUNDS;
      if(value) drv_flags2 |= DRVF2_DRAWSOUNDS;
      break;

   case RS_DRAWMIRRORS:
      drv_flags &= ~DRVF_DRAWMIRRORS;
      if(value) drv_flags |= DRVF_DRAWMIRRORS;
      break;

   case RS_USELMAPPING:
      {
         drv_flags &= ~DRVF_USELMAPPING;
         if(value){
            if(!(drv_flags&DRVF_SINGLE_PASS_MODULATE2X)){
                              //check if required blending mode supported
               if(!((d3d_caps.SrcBlendCaps&D3DPBLENDCAPS_DESTCOLOR) &&
                  (d3d_caps.DestBlendCaps&D3DPBLENDCAPS_SRCCOLOR))){
                              //can't use desired blend mode, light-mapping not supported
                  break;
               }
            }
            drv_flags |= DRVF_USELMAPPING;
         }
      }
      break;

   case RS_DRAWLMTEXTURES:
      drv_flags &= ~DRVF_DRAWLMTEXTURES;
      if(value){
         drv_flags |= DRVF_DRAWLMTEXTURES;
         PurgeLMTextures();
      }
      break;

   case RS_DEBUGDRAWBSP:
      drv_flags &= ~DRVF_DEBUGDRAWBSP;
      if(value) drv_flags |= DRVF_DEBUGDRAWBSP;
      break;

   case RS_DEBUGDRAWDYNAMIC:
      drv_flags &= ~DRVF_DEBUGDRAWDYNAMIC;
      if(value) drv_flags |= DRVF_DEBUGDRAWDYNAMIC;
      break;

   case RS_DRAWCAMERAS:
      drv_flags2 &= ~DRVF2_DRAWCAMERAS;
      if(value) drv_flags2 |= DRVF2_DRAWCAMERAS;
      break;

   case RS_DRAWDUMMYS:
      drv_flags2 &= ~DRVF2_DRAWDUMMYS;
      if(value) drv_flags2 |= DRVF2_DRAWDUMMYS;
      break;

   case RS_DRAWOCCLUDERS:
      drv_flags2 &= ~DRVF2_DRAWOCCLUDERS;
      if(value) drv_flags2 |= DRVF2_DRAWOCCLUDERS;
      break;

   case RS_DRAWTEXTURES:
      {
         bool on = (value&1);
         if(bool(drv_flags2&DRVF2_DRAWTEXTURES) != on){
            drv_flags2 &= ~DRVF2_DRAWTEXTURES;
            if(on) drv_flags2 |= DRVF2_DRAWTEXTURES;
            if(value&0x80000000)
               ReleaseHWVertexBuffers();
         }
      }
      break;

   case RS_LMTRUECOLOR:
      {
         bool was_on = (drv_flags2&DRVF2_LMTRUECOLOR);
         if((bool)value!=was_on){
            drv_flags2 &= ~DRVF2_LMTRUECOLOR;
            if(value) drv_flags2 |= DRVF2_LMTRUECOLOR;
            InitLMConvertor();
            PurgeLMTextures();
         }
      }
      break;

   case RS_LMDITHER:
      {
         bool was_on = (drv_flags2&DRVF2_LMDITHER);
         if((bool)value!=was_on){
            drv_flags2 &= ~DRVF2_LMDITHER;
            if(value) drv_flags2 |= DRVF2_LMDITHER;
            if(pf_light_maps.bytes_per_pixel <= 2){
               InitLMConvertor();
               PurgeLMTextures();
            }
         }
      }
      break;
#ifndef GL
   case RS_DEBUG_SHOW_OVERDRAW:
      drv_flags2 &= ~DRVF2_DEBUG_SHOW_OVERDRAW;
      if(value)
         drv_flags2 |= DRVF2_DEBUG_SHOW_OVERDRAW;
      break;
#endif
   case RS_LM_AA_RATIO:
      if((int)value <= 0 || value > 100)
         return I3DERR_INVALIDPARAMS;
      lm_create_aa_ratio = value;
      break;

   case RS_DRAWJOINTS:
      drv_flags2 &= ~DRVF2_DRAWJOINTS;
      if(value) drv_flags2 |= DRVF2_DRAWJOINTS;
      break;

   case RS_DEBUG_INT0: case RS_DEBUG_INT1: case RS_DEBUG_INT2: case RS_DEBUG_INT3:
   case RS_DEBUG_INT4: case RS_DEBUG_INT5: case RS_DEBUG_INT6: case RS_DEBUG_INT7:
#ifdef _DEBUG
      debug_int[st-RS_DEBUG_INT0] = value;
#else
      _debug_int[st-RS_DEBUG_INT0] = value;
#endif
      break;

   case RS_DEBUG_FLOAT0: case RS_DEBUG_FLOAT1: case RS_DEBUG_FLOAT2: case RS_DEBUG_FLOAT3:
   case RS_DEBUG_FLOAT4: case RS_DEBUG_FLOAT5: case RS_DEBUG_FLOAT6: case RS_DEBUG_FLOAT7:
#ifdef _DEBUG
      debug_float[st-RS_DEBUG_FLOAT0] = *(float*)&value;
#else
      _debug_float[st-RS_DEBUG_FLOAT0] = *(float*)&value;
#endif
      break;

   case RS_LOD_INDEX:
      force_lod_index = Max(-1, (int)value);
      break;

   case RS_LOD_SCALE:
      lod_scale = I3DIntAsFloat(value);
      break;

   case RS_USE_OCCLUSION:
      drv_flags2 &= ~DRVF2_USE_OCCLUSION;
      if(value) drv_flags2 |= DRVF2_USE_OCCLUSION;
      break;

   case RS_DEBUG_DRAW_MATS:
      debug_draw_mats = value;
      break;

   case RS_SOUND_VOLUME:
      {
         float vol = I3DIntAsFloat(value);
         if(global_sound_volume!=vol){
            global_sound_volume = vol;
            for(dword i=sounds.size(); i--; ){
               PI3D_sound snd = sounds[i];
               snd->ReapplyVolume(false);
            }
         }
      }
      break;

   case RS_PROFILER_MODE:
      if(value > 2)
         return I3DERR_INVALIDPARAMS;
#ifdef _DEBUG
      if(block_profiler){
         block_profiler->SetMode((C_block_profiler::E_MODE)value);
         break;
      }
#endif
      return I3DERR_UNSUPPORTED;
   }

   if(FAILED(hr))
      return I3DERR_GENERIC;
   return I3D_OK;
}

//----------------------------

dword I3D_driver::GetState(I3D_RENDERSTATE st) const{

   switch(st){
   case RS_LINEARFILTER: return (bool)(drv_flags&DRVF_LINFILTER);
   case RS_ANISO_FILTERING: return (bool)(drv_flags&DRVF_USE_ANISO_FILTER); break;
   case RS_MIPMAP: return (bool)(drv_flags&DRVF_MIPMAP);
   case RS_TEXTURECOMPRESS: return (bool)(drv_flags&DRVF_TEXTURECOMPRESS);
   case RS_DRAW_COL_TESTS: return (bool)(drv_flags2&DRVF2_DRAWCOLTESTS);
   case RS_USEZB: return (bool)(drv_flags&DRVF_USEZB);
   case RS_CLEAR: return (bool)(drv_flags&DRVF_CLEAR);
   case RS_DITHER: return (bool)(drv_flags&DRVF_DITHER);
   case RS_WIREFRAME: return (bool)(drv_flags&DRVF_WIREFRAME);
   case RS_DRAWBOUNDBOX: return (bool)(drv_flags&DRVF_DRAWBBOX);
   case RS_DRAWHRBOUNDBOX: return (bool)(drv_flags&DRVF_DRAWHRBBOX);
   case RS_DRAWLINKS: return (bool)(drv_flags&DRVF_DRAWHRLINK);
   case RS_LOADMIPMAP: return (bool)(drv_flags&DRVF_LOADMIPMAP);
   //case RS_POINTFOG: return (bool)(drv_flags&DRVF_POINTFOG);
   case RS_ENVMAPPING: return (bool)(drv_flags2&DRVF2_ENVMAPPING);
   case RS_DETAILMAPPING: return (bool)(drv_flags2&DRVF2_DETAILMAPPING);
   case RS_USE_EMBM: return (bool)(drv_flags2&DRVF2_USE_EMBM);
   case RS_DRAWSECTORS: return (bool)(drv_flags&DRVF_DRAWSECTORS);
   case RS_DRAWPORTALS: return (bool)(drv_flags&DRVF_DRAWPORTALS);
   case RS_TEXTUREDITHER: return (bool)(drv_flags&DRVF_TEXTDITHER);
   case RS_DRAWVISUALS: return (bool)(drv_flags&DRVF_DRAWVISUALS);
   case RS_FOG: return (bool)(drv_flags&DRVF_USEFOG);
   //case RS_TEXTURELOWDETAIL: return (bool)(drv_flags&DRVF_TEXTURELOWDET);
   case RS_DRAWVOLUMES: return (bool)(drv_flags2&DRVF2_DRAWVOLUMES);
   case RS_DRAWLIGHTS: return (bool)(drv_flags2&DRVF2_DRAWLIGHTS);
   case RS_DRAWSOUNDS: return (bool)(drv_flags2&DRVF2_DRAWSOUNDS);
   case RS_USELMAPPING: return (bool)(drv_flags&DRVF_USELMAPPING);
   case RS_DRAWLMTEXTURES: return (bool)(drv_flags&DRVF_DRAWLMTEXTURES);
   case RS_DRAWCAMERAS: return (bool)(drv_flags2&DRVF2_DRAWCAMERAS);
   case RS_DRAWDUMMYS: return (bool)(drv_flags2&DRVF2_DRAWDUMMYS);
   case RS_DRAWOCCLUDERS: return (bool)(drv_flags2&DRVF2_DRAWOCCLUDERS);
   case RS_DRAWTEXTURES: return (bool)(drv_flags2&DRVF2_DRAWTEXTURES);
   case RS_LMTRUECOLOR: return (bool)(drv_flags2&DRVF2_LMTRUECOLOR);
   case RS_LMDITHER: return (bool)(drv_flags2&DRVF2_LMDITHER);
#ifndef GL
   case RS_DEBUG_SHOW_OVERDRAW: return (bool)(drv_flags2&DRVF2_DEBUG_SHOW_OVERDRAW);
#endif
   case RS_LM_AA_RATIO: return lm_create_aa_ratio;
   case RS_DRAWJOINTS: return (bool)(drv_flags2&DRVF2_DRAWJOINTS);
   case RS_DEBUG_INT0: case RS_DEBUG_INT1: case RS_DEBUG_INT2: case RS_DEBUG_INT3:
   case RS_DEBUG_INT4: case RS_DEBUG_INT5: case RS_DEBUG_INT6: case RS_DEBUG_INT7:
#ifdef _DEBUG
      return debug_int[st-RS_DEBUG_INT0];
#else
      return _debug_int[st-RS_DEBUG_INT0];
#endif
   case RS_DEBUG_FLOAT0: case RS_DEBUG_FLOAT1: case RS_DEBUG_FLOAT2: case RS_DEBUG_FLOAT3:
   case RS_DEBUG_FLOAT4: case RS_DEBUG_FLOAT5: case RS_DEBUG_FLOAT6: case RS_DEBUG_FLOAT7:
#ifdef _DEBUG
      return I3DFloatAsInt(debug_float[st-RS_DEBUG_FLOAT0]);
#else
      return I3DFloatAsInt(_debug_float[st-RS_DEBUG_FLOAT0]);
#endif
   case RS_LOD_INDEX: return force_lod_index;
   case RS_LOD_SCALE: return I3DFloatAsInt(lod_scale); break;
   case RS_USE_OCCLUSION: return (bool)(drv_flags2&DRVF2_USE_OCCLUSION);
   case RS_DEBUG_DRAW_MATS: return debug_draw_mats;
   case RS_SOUND_VOLUME: return I3DFloatAsInt(global_sound_volume);
   case RS_PROFILER_MODE:
#ifdef _DEBUG
      if(block_profiler)
         return block_profiler->GetMode();
#endif
      return 0;
   }
   return 0;
}

//----------------------------

I3D_RESULT I3D_driver::GetStats(I3DSTATSINDEX indx, void *data){

   switch(indx){

   case I3DSTATS_VIDEO:
      {
         PI3D_stats_video st = (PI3D_stats_video)data;
         st->txt_total = managed_textures.size();
         st->txt_vidmem = 0;
         st->txt_sys_mem = 0;
         st->txt_video_mem = 0;
         for(int i=managed_textures.size(); i--; ){
            CPI3D_texture tp = managed_textures[i];
            dword txtf = tp->GetTxtFlags();
            //if(!(txtf&TXTF_UPLOAD_NEEDED))
            if(!(txtf&TXTF_UPLOAD_NEEDED))
               ++st->txt_vidmem;
            CPIImage img = tp->GetSysmemImage();

            if(!img)          //04.06.2002 JV: don't count invalid img
               continue;
                              //compute memory of image
            const S_pixelformat *pf = img->GetPixelFormat();
            dword img_bytes;
            if(pf->flags&PIXELFORMAT_COMPRESS){
               img_bytes = img->SizeX() * img->SizeY() * 1;
               if(pf->flags&PIXELFORMAT_ALPHA)
                  img_bytes *= 2;
            }else{
               img_bytes = img->SizeX() * img->SizeY() * pf->bytes_per_pixel;
            }
                              //add mipmap levels
            //img_bytes += img_bytes*.3333f;
            st->txt_sys_mem += img_bytes;

            if(!(txtf&TXTF_UPLOAD_NEEDED))
               st->txt_video_mem += img_bytes;
         }
                              //get vidmem stats from D3D
         st->vidmem_free = d3d_dev->GetAvailableTextureMem();
         st->vidmem_total = st->txt_video_mem + st->vidmem_free;

         st->txtmem_total = st->vidmem_total;
         st->txtmem_free = st->vidmem_free;
      }
      break;

   case I3DSTATS_MEMORY:
      {
         PI3D_stats_memory st = (PI3D_stats_memory)data;
         st->used_other = 0;
         st->available = 0;
         dbase.GetStats(st->dbase_size, st->dbase_used);

#ifdef _MSC_VER
         st->num_blocks = 0;
         st->used = num_alloc_bytes;
         /*
         st->used = 0;
         _heapinfo hi;
         hi._pentry = NULL;
         while(true){
            int status = _heapwalk(&hi);
            if(status!=_HEAPOK) break;
            if(hi._useflag == _USEDENTRY){
               ++st->num_blocks;
               st->used += hi._size;
            }
         }
         */
#else
         memset(st, 0, sizeof(I3D_stats_memory));
         //return I3DERR_UNSUPPORTED;
#endif
      }
      break;

   default:
      return I3DERR_INVALIDPARAMS;
   }
   return I3D_OK;
}

//----------------------------

PI3D_material I3D_driver::GetIcon(dword res_id){

   if(res_id >= icon_materials.size())
      icon_materials.resize(res_id+1);

   if(!icon_materials[res_id]){
      PI3D_texture txt = NULL;

      HINSTANCE hinst = GetHInstance();
      C_cache ck;
      if(OpenResource(hinst, "BINARY", (const char*)res_id, ck)){
         I3D_CREATETEXTURE ct;
         memset(&ct, 0, sizeof(ct));
         ct.flags = TEXTMAP_DIFFUSE | TEXTMAP_TRANSP |
            TEXTMAP_MIPMAP |
            TEXTMAP_USE_CACHE;
         ct.ck_diffuse = &ck;
         CreateTexture(&ct, &txt);
      }
      PI3D_material mat = CreateMaterial();
      mat->SetTexture(MTI_DIFFUSE, txt);
      if(txt)
         txt->Release();

      icon_materials[res_id] = mat;
      mat->Release();
   }
   return icon_materials[res_id];
}

//----------------------------

void I3D_driver::PurgeLMTextures(){

   for(int i=managed_textures.size(); i--; ){
      PI3D_texture tp = managed_textures[i];
                           //don't refresh LMap textures
      if(UnregLMTexture(tp))
         tp->SetDirty();
   }
}

//----------------------------

I3D_RESULT I3D_driver::SetDataBase(const char *db_filename, dword size, int max_keep_days){

   if(db_filename){
      bool b = dbase.Open(db_filename, size, max_keep_days);
      if(!b){
         DEBUG_LOG(C_xstr("Failed to open database file '%'.") %db_filename);
         return I3DERR_GENERIC;
      }
      return I3D_OK;
   }else{
      dbase.Close();
      return I3D_OK;
   }
}

//----------------------------

I3D_RESULT I3D_driver::FlushDataBase(){

   if(!dbase.IsOpen()) return I3DERR_NOTINITIALIZED;
   dbase.Flush();
   return I3D_OK;
}

//----------------------------

dword I3D_driver::NumProperties(dword visual_type) const{

   for(int i=num_visual_plugins; i--; ){
      const S_visual_plugin_entry &pe = visual_plugins[i];
      if(pe.visual_type == visual_type){
         int i = 0;
         if(pe.props){
            while(pe.props[i].prop_type)
               ++i;
         }
         return i;
      }
   }
   return (dword)-1;
}

//----------------------------

const char *I3D_driver::GetPropertyName(dword visual_type, dword index) const{

   for(int i=num_visual_plugins; i--; ){
      const S_visual_plugin_entry &pe = visual_plugins[i];
      if(pe.visual_type == visual_type){
         /*
         dword num = pe.num_props;
         if(index >= num)
            return NULL;
            */
         return pe.props[index].prop_name;
      }
   }
   return NULL;
}

//----------------------------

I3D_PROPERTYTYPE I3D_driver::GetPropertyType(dword visual_type, dword index) const{

   for(int i=num_visual_plugins; i--; ){
      const S_visual_plugin_entry &pe = visual_plugins[i];
      if(pe.visual_type == visual_type){
         /*
         dword num = pe.num_props;
         if(index >= num)
            return I3DPROP_NULL;
            */
         return pe.props[index].prop_type;
      }
   }
   return I3DPROP_NULL;
}

//----------------------------

const char *I3D_driver::GetPropertyHelp(dword visual_type, dword index) const{

   for(int i=num_visual_plugins; i--; ){
      const S_visual_plugin_entry &pe = visual_plugins[i];
      if(pe.visual_type == visual_type){
         /*
         dword num = pe.num_props;
         if(index >= num)
            return NULL;
            */
         return pe.props[index].help;
      }
   }
   return NULL;
}

//----------------------------

bool I3D_driver::GetVisualPropertiesInfo(dword vt, const S_visual_property **props, dword *num_props) const{

   for(int i=num_visual_plugins; i--; ){
      const S_visual_plugin_entry &pe = visual_plugins[i];
      if(pe.visual_type == vt){
         *props = pe.props;
         //*num_props = pe.num_props;
         for(*num_props=0; pe.props[*num_props].prop_type; *num_props++);
         return true;
      }
   }
   return false;
}

//----------------------------

void I3D_driver::UnregisterProcedural(PI3D_procedural_base pb) const{

   for(int i=procedural_cache.size(); i--; ){
      if(procedural_cache[i]==pb){
         procedural_cache[i] = procedural_cache.back(); procedural_cache.pop_back();
         break;
      }
   }
}

//----------------------------

                              //sound properties
I3D_RESULT I3D_driver::SetSoundEnvData(int set_id, I3D_SOUND_ENV_DATA_INDEX indx, dword data){

   if(indx>=I3D_SENV_LAST)
      return I3DERR_INVALIDPARAMS;

   t_env_properties::iterator it = env_properties.find(set_id);
                              //create new set, if not exists
   if(it==env_properties.end())
      it = env_properties.insert(pair<int, S_sound_env_properties>(set_id, S_sound_env_properties())).first;

   S_sound_env_properties &ep = (*it).second;

   bool raw_store = true;
   switch(indx){
   case I3D_SENV_F_ROOM_LEVEL_LF:
   case I3D_SENV_F_ROOM_LEVEL_HF:
   case I3D_SENV_F_DECAY_HF_RATIO:
   case I3D_SENV_F_REFLECTIONS_RATIO:
   case I3D_SENV_F_REVERB_RATIO:
   case I3D_SENV_F_ENV_DIFFUSION:
   case I3D_SENV_F_EMUL_VOLUME:
      {
         float f = I3DIntAsFloat(data);
         f = Min(1.0f, Max(0.0f, f));
         data = I3DFloatAsInt(f);
      }
      break;

#ifdef USE_EAX
   case I3D_SENV_I_REVERB_DECAY_TIME:
      data = (dword)Max((int)(EAXLISTENER_MINDECAYTIME*1000), Min((int)(EAXLISTENER_MAXDECAYTIME*1000), (int)data));
      break;

   case I3D_SENV_I_REFLECTIONS_DELAY:
      data = (dword)Max((int)(EAXLISTENER_MINREFLECTIONSDELAY*1000), Min((int)(EAXLISTENER_MAXREFLECTIONSDELAY*1000), (int)data));
      break;

   case I3D_SENV_I_REVERB_DELAY:
      data = (dword)Max((int)(EAXLISTENER_MINREVERBDELAY*1000), Min((int)(EAXLISTENER_MAXREVERBDELAY*1000), (int)data));
      break;
#endif

   case I3D_SENV_S_NAME:
      raw_store = false;
      ep.name = (const char*)data;
      break;

   default:
      //assert(0);
      return I3DERR_INVALIDPARAMS;
   }
                              //save value
   if(raw_store)
      ((dword*)&ep)[indx] = data;
   drv_flags |= DRVF_SOUND_ENV_RESET;

   return I3D_OK;
}

//----------------------------

dword I3D_driver::GetSoundEnvData(int set_id, I3D_SOUND_ENV_DATA_INDEX indx) const{

   if(indx>=I3D_SENV_LAST)
      return (dword)-1;

   t_env_properties::const_iterator it = env_properties.find(set_id);
   if(it==env_properties.end())
      return (dword)-1;
   const S_sound_env_properties &ep = (*it).second;

   switch(indx){
   case I3D_SENV_S_NAME:
      return (dword)(const char*)ep.name;
   }
   return ((dword*)&ep)[indx];
}

//----------------------------

void I3D_driver::SetCollisionMaterial(dword index, const I3D_collision_mat &mi){

   col_mat_info[index] = mi;
}

//----------------------------

void I3D_driver::ClearCollisionMaterials(){

   col_mat_info.clear();
}

//----------------------------

/*
I3D_RESULT I3D_driver::ReloadTexture(PI3D_texture txt) const{

   for(dword i=managed_textures.size(); i--; ){
      if(managed_textures[i]==txt){
                              //reload the texture
         txt->Reload();
         return I3D_OK;
      }
   }
   return I3DERR_OBJECTNOTFOUND;
}
*/

//----------------------------
#ifndef GL
I3D_RESULT I3D_driver::SetNightVision(bool on){

   if(drv_flags2&DRVF2_IN_NIGHT_VISION)
      return I3DERR_INVALIDPARAMS;

   if(on==bool(drv_flags2&DRVF2_NIGHT_VISION))
      return I3D_OK;

   if(on){
      dword sx = 512, sy = 512;
      CPIGraph igraph = GetGraphInterface();
      dword scr_x = igraph->Scrn_sx();
      dword scr_y = igraph->Scrn_sy();
      while(sx>scr_x)
         sx >>= 2;
      while(sy>scr_y)
         sy >>= 2;
      dword flags = TEXTMAP_NOMIPMAP | TEXTMAP_RENDERTARGET | TEXTMAP_TRUECOLOR | TEXTMAP_NO_SYSMEM_COPY;
      I3D_RESULT ir = InitRenderTarget(rt_night_vision, flags, sx, sy);
      if(I3D_FAIL(ir)){
                           //not enough memory? free resources and try again
         EvictResources();
         ir = InitRenderTarget(rt_night_vision, flags, sx, sy);
      }
      if(I3D_SUCCESS(ir))
         drv_flags2 |= DRVF2_NIGHT_VISION;
   }else{
      rt_night_vision.Close();
      drv_flags2 &= ~DRVF2_NIGHT_VISION;
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_driver::BeginNightVisionRender(){

   if(!(drv_flags2&DRVF2_NIGHT_VISION))
      return I3DERR_NOTINITIALIZED;
   if(drv_flags2&DRVF2_IN_NIGHT_VISION)
      return I3DERR_INVALIDPARAMS;

   BeginScene();
   drv_flags2 |= DRVF2_IN_NIGHT_VISION;

   SetRenderTarget(rt_night_vision);
   SetViewport(I3D_rectangle(0, 0, GetGraphInterface()->Scrn_sx(), GetGraphInterface()->Scrn_sy()));
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_driver::EndNightVisionRender(){

   if(!(drv_flags2&DRVF2_IN_NIGHT_VISION))
      return I3DERR_NOTINITIALIZED;

   SetRenderTarget(default_render_target);

   if(drv_flags&DRVF_USEZB)
      Clear(0);
      //d3d_dev->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

   EnableZBUsage(false);
   //EnableZWrite(false);
   d3d_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
   SetTexture1(0, rt_night_vision);

   I3D_driver::S_vs_shader_entry_in se;
   se.AddFragment(VSF_MUL_TRANSFORM);
   se.AddFragment(VSF_MAKE_RECT_UV);

   if(CanUsePixelShader()){
      DisableTextures(1);
      I3D_driver::S_ps_shader_entry_in se_ps;
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_NIGHT_VIEW);
      //se_ps.AddFragment(PSF_t0_COPY);
      SetPixelShader(se_ps);

      static S_vectorw psc[] = {
         S_vectorw(0, .9f, 0, 1),     //fully green color
         S_vectorw(1, 0, 1, 1),     //complement to white (partionally added to the 1st color)
         S_vectorw(0, .1f, 0, 1),
         S_vectorw(.3f, .6f, .1f, 1),
      };
      SetPSConstant(PSC_FACTOR, psc, 2);
      SetPSConstant(PSC_COLOR, &psc[2], 2);
   }else{
      bool can_dot3 = (GetCaps()->TextureOpCaps&D3DTEXOPCAPS_DOTPRODUCT3);
      if(can_dot3)
         SetupTextureStage(0, D3DTOP_DOTPRODUCT3);
      else
         SetupTextureStage(0, D3DTOP_MODULATE2X);

      SetTexture1(1, NULL);
      SetupTextureStage(1, D3DTOP_MODULATE2X);
      d3d_dev->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TFACTOR);
      if(can_dot3)
         d3d_dev->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT | D3DTA_COMPLEMENT);
      SetTextureFactor(0x40ff40);

      DisableTextureStage(2);

                           //pre-multiply by RGB for making grayscale (r=.3, g=.6, b=.1),
      se.AddFragment(VSF_LIGHT_BEGIN);
      se.AddFragment(VSF_LIGHT_END);
      static const S_vectorw color_grayscale(.5f-(.5f*.24f), .5f-(.5f*.48f), .5f-(.5f*.08f), 1),
         color_raise(1, 1, 1, 1);
      if(can_dot3)
         SetVSConstant(VSC_AMBIENT, &color_grayscale);
      else
         SetVSConstant(VSC_AMBIENT, &color_raise);
   }
   {
      SetVertexShader(GetVSHandle(se)->vs);

      SetStreamSource(vb_rectangle, sizeof(S_vertex_rectangle));
      SetVSDecl(vs_decl_rectangle);
                              //setup position multiplication constant
      S_vectorw vc(-2, 2, 0, 1);
      SetVSConstant(VSC_MAT_TRANSFORM_0, &vc);
   }
   SetupBlend(I3DBLEND_OPAQUE);
   SetupAlphaTest(false);
   EnableNoCull(true);

   if(drv_flags&DRVF_WIREFRAME)
      d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

   HRESULT hr;
   hr = d3d_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   CHECK_D3D_RESULT("DrawPrimitive", hr);

   if(!CanUsePixelShader()){
      d3d_dev->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
      d3d_dev->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
   }
   if(drv_flags&DRVF_WIREFRAME)
      d3d_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
   SetTexture1(0, NULL);
   //d3d_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
   //EnableZWrite(true);
   EnableZBUsage(true);

   drv_flags2 &= ~DRVF2_IN_NIGHT_VISION;
   EndScene();

   return I3D_OK;
}
#endif
//----------------------------
//----------------------------
dword GetTickTime(){
   return GetTickCount();
}

