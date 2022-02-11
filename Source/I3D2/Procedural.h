#ifndef __PROCEDURAL_H
#define __PROCEDURAL_H

#include "texture.h"
#include "driver.h"

//----------------------------

enum I3D_PROCEDURAL_ID{
   PROCID_NULL,
   PROCID_MORPH_3D,           //morphing of one or more 3D positions
   PROCID_PARTICLE,           //particle animation
   PROCID_TEXTURE,            //texture

   PROCID_LAST
};

//----------------------------

class I3D_procedural_base{
   C_str name;
   dword init_data;
   I3D_PROCEDURAL_ID pid;
protected:
   CPI3D_driver drv;
   mutable dword last_render_time;

   I3D_procedural_base(CPI3D_driver, const C_str &name, I3D_PROCEDURAL_ID pid1, dword init_data);
   ~I3D_procedural_base();
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   inline const C_str &GetName() const{ return name; }
   inline I3D_PROCEDURAL_ID GetID() const{ return pid; }
   inline dword GetInitData() const{ return init_data; }

//----------------------------
// Sync our render time with driver's render time, return delta.
   inline int SyncRenderTime() const{
      int delta = drv->GetRenderTime() - last_render_time;
      assert((delta >= 0) && (delta < 1000*60*60*72));
      last_render_time = drv->GetRenderTime();
      return delta;
   }

//----------------------------
// Initialize procedural from given definition file. Returns status of initialization.
   virtual I3D_RESULT Initialize(C_cache &ck, PI3D_LOAD_CB_PROC cb_proc, void *cb_context){
      return true;
   }

//----------------------------
// Evaluate - re-compute newest values. Use I3D_driver::GetRenderTime to update
// only once per frame.
   virtual void Evaluate(){}
};

typedef I3D_procedural_base *PI3D_procedural_base;
typedef const I3D_procedural_base *CPI3D_procedural_base;

//----------------------------
//----------------------------
                              //procedural: MORPH_3D
                              //used for:
                              // morphing one or more regions in 3D space
class I3D_procedural_morph: public I3D_procedural_base{
protected:
   dword ref;
   I3D_procedural_morph(CPI3D_driver d1, const C_str &name, dword init_data):
      ref(1),
      I3D_procedural_base(d1, name, PROCID_MORPH_3D, init_data)
   {}
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

//----------------------------
// Get max extent to which any morphing channel may get animated.
   virtual S_vector GetMaxMorphScale() const = 0;

   virtual int GetNumChannels() const = 0;
   virtual const S_vectorw *GetMorphingValues() const = 0;
};

//----------------------------
//----------------------------
                              //procedural: PARTICLE
                              //used for:
                              // animation of particle elements
class I3D_procedural_particle: public I3D_procedural_base{
   dword ref;
public:
   I3D_procedural_particle(CPI3D_driver d1, const C_str &name, dword init_data):
      ref(1),
      I3D_procedural_base(d1, name, PROCID_PARTICLE, init_data)
   {}
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
};

//----------------------------
//----------------------------

class I3D_procedural_texture: public I3D_texture, public I3D_procedural_base{
#define TXTPROC_LUT_SIZE 512  //must be 2^n

public:
   enum E_MODE{
      MODE_NULL,
      MODE_RGB,
      MODE_ALPHA,
      MODE_RGBA,
      MODE_EMBM
   };
private:
   void *texel_look_up;       //conversion table from byte to surface's pixel format
   float cos_lut[TXTPROC_LUT_SIZE];       //cos(0.0 ... 2*PI)
   int fp_cos_asin_lut[TXTPROC_LUT_SIZE]; //cos(asin(0.0 ... 1.0)) in fixed-point 16:16
   //int sqrt_lut[256];         //sqrt look-up table
   float dist_lut[32][32];


   int life_len, life_len_thresh;
   int create_time, create_time_thresh;
   float init_radius_ratio;   //ratio of initial radius, relative to texture width
   float radius_grow_ratio;   //how much radius grows per second, relative to ''
   float shift_speed;         //in waves per second
   float wave_repeat;         //how many times wave is repeat accross radius;
   float unit_scale;          //scaling of resulting value, before clamping
   float height_curve;        //height change accross life-time
   bool true_color;           //use true-color texture, if available
   bool debug_mode;           //when set, values are interactively read from driver's debugs
   E_MODE mode;

   struct S_element{
      int x, y;               //pixels
      float wave_shift_base;  //wave counts
      int life_len;           //miliseconds
      int countdown;          //miliseconds
   };
   C_vector<S_element> elements;
   int create_countdown;
   mutable int cumulate_time;
   int frame_counter;

//----------------------------
// Build look-up table for fast sqrt.
   //void BuildSqrtTable();

//----------------------------
// Initialize sysmem dynamic texture for procedural effect.
   bool InitTexture(dword size, const I3D_CREATETEXTURE *ct, PI3D_LOAD_CB_PROC cb_proc, void *cb_context);

//----------------------------
// Pre-compute look-up tables.
   void CreateLUTs();

//----------------------------
// Update contents of sysmem texture.
   void Tick(int time);
public:
   I3D_procedural_texture(CPI3D_driver, const C_str &name, dword init_data);
   ~I3D_procedural_texture();

   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
   virtual IDirect3DBaseTexture9 *GetD3DTexture() const;
   virtual E_TEXTURE_TYPE GetTextureType() const{ return TEXTURE_2D; }

   virtual I3D_RESULT Initialize(C_cache &ck, PI3D_LOAD_CB_PROC cb_proc, void *cb_context);
   //virtual I3D_RESULT Initialize(C_cache &ck, const I3D_CREATETEXTURE &ct, PI3D_LOAD_CB_PROC cb_proc, void *cb_context);
#ifdef GL
   virtual dword GetGlId() const{ return 0; }
#endif
   inline E_MODE GetMode() const{ return mode; }
};

//----------------------------
//----------------------------

#endif
