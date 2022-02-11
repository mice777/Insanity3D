#ifndef __DRIVER_H
#define __DRIVER_H

#include "texture.h"
#include "material.h"
#ifndef GL
#include "vb_mgr.h"
#endif
#include "database.h"
#include "common.h"

                              //define type for nvlinker compatibility with DX8
typedef struct IDirect3DDevice9 *LPDIRECT3DDEVICE8;

#include "nvidia\nvlink\nvlink.h"   //vertex shader stuff


//----------------------------
                              //I3D class identificators
enum I3D_CLID{
   I3D_CLID_DRIVER,
   I3D_CLID_SCENE,

   I3D_CLID_FRAME,
   I3D_CLID_DUMMY,
   I3D_CLID_VISUAL,
   I3D_CLID_MODEL,
   I3D_CLID_LIGHT,
   I3D_CLID_SOUND,
   I3D_CLID_SECTOR,
   I3D_CLID_CAMERA,
   I3D_CLID_,
   I3D_CLID_VOLUME,
   I3D_CLID_USER,
   I3D_CLID_JOINT,
   I3D_CLID_OCCLUDER,
   I3D_CLID_SINGLE_MESH,

   I3D_CLID_TEXTURE,
   I3D_CLID_MATERIAL,
   I3D_CLID_MESH_BASE,
   I3D_CLID_MESH_OBJECT,
   I3D_CLID_PORTAL,
   I3D_CLID_ANIMATION_BASE,
   I3D_CLID_ANIMATION_SET,
   I3D_CLID_INTERPOLATOR,
   I3D_CLID_ANIM_INTERPOLATOR,
   I3D_CLID_PROCEDURAL,

   I3D_CLID_LAST
};

//----------------------------

class C_LM_alloc_block;
class C_LM_surface;

//----------------------------

#define MAX_VS_LIGHTS 6       //maximal number of lights for vertex shader
#define MAX_VS_FRAGMENTS (MAX_VS_LIGHTS+20)  //maximal number of fragments a shader may have
#define MAX_VS_BLEND_MATRICES 17

enum E_VS_FRAGMENT{
   VSF_TRANSFORM,          //simple transformation by world-view-proj matrix
   VSF_MUL_TRANSFORM,      //simple multiplication of input pos by C_vector
   VSF_M_PALETTE_TRANSFORM,//transformation using matrix palette blending (up to 2 matrices per verex, 1 weight)
   VSF_M_PALETTE_TRANSFORM1,//transformation using indexed matrix blending (up to 4 matrices per vertex, 3 weights)
   VSF_TRANSFORM_MORPH1_OS,//transformation using weighted blending - 1 morph
   VSF_TRANSFORM_MORPH1_WS,//transformation using weighted blending - 1 morph
   VSF_TRANSFORM_MORPH2_OS,//transformation using weighted blending - 2 morph2
   VSF_TRANSFORM_MORPH2_WS,//transformation using weighted blending - 2 morph2
   VSF_TRANSFORM_DISCHARGE,//transformation for Discharge visual
   VSF_TRANSFORM_PARTICLE, //transformation for Particle visual
   VSF_TEST,               //used for debugging
   VSF_PICK_UV_0,          //pick texture coordinate
   VSF_PICK_UV_1,
   VSF_PICK_UV_2,
   VSF_PICK_UV_3,
   VSF_STORE_UV_0,         //store texture coordinate
   VSF_STORE_UV_1,
   VSF_STORE_UV_2,
   VSF_STORE_UV_3,
   /*
   VSF_STORE_UV_4,
   VSF_STORE_UV_5,
   VSF_STORE_UV_6,
   VSF_STORE_UV_7,
   */
   VSF_STORE_UVW_0,
   VSF_STORE_UVW_1,
   VSF_STORE_UVW_2,
   VSF_STORE_UVW_3,
   /*
   VSF_STORE_UVW_4,
   VSF_STORE_UVW_5,
   VSF_STORE_UVW_6,
   VSF_STORE_UVW_7,
   */
   VSF_GENERATE_ENVUV_OS,  //generate environment mapping uv coordinates
   VSF_GENERATE_ENVUV_WS,  //'', but in world space
   VSF_GENERATE_ENVUV_CUBE_OS,
   VSF_GENERATE_ENVUV_CUBE_WS,
   VSF_SHIFT_UV,              //compute UV shifting on uv by VSC_UV_SHIFT constant
   VSF_MULTIPLY_UV_BY_XY,     //multiply UV by VSC_UV_SHIFT_SCALE.xy
   VSF_MULTIPLY_UV_BY_Z,      //multiply UV by VSC_UV_SHIFT_SCALE.z
   VSF_TRANSFORM_UV0,         //transform picked uv by 3x2 matrix and store to uv 0
   VSF_TEXT_PROJECT,          //project texture for shadow rendering
   VSF_MAKE_RECT_UV,          //make uv from unit-sided rectangle
   /*
   VSF_TEXT_CLIP_OS_1,        //texture clipping coordinates
   VSF_TEXT_CLIP_OS_2,
   VSF_TEXT_CLIP_OS_3,
   VSF_TEXT_CLIP_WS_1,
   VSF_TEXT_CLIP_WS_2,
   VSF_TEXT_CLIP_WS_3,
   */
   VSF_FOG_SIMPLE_OS,      //simple height fog
   VSF_FOG_SIMPLE_WS,      //simple height fog
   VSF_FOG_BEGIN,          //initialize fog computation
   VSF_FOG_END,            //finish fog computation, write to output reg
   VSF_FOG_HEIGHT_OS,      //compute height fog, using current constant parameters (VSC_FOGPARAMS)
   VSF_FOG_HEIGHT_WS,      //'' in world space
   VSF_FOG_LAYERED_OS,     //compute layered fog in object space
   VSF_FOG_LAYERED_WS,     //'' in world space
   VSF_FOG_POINT_OS,       //compute point fog
   VSF_FOG_POINT_WS,       //compute point fog
   VSF_LIGHT_BEGIN,        //initialize lighting computation (diffuse)
   VSF_LIGHT_END,          //finish lighting computation, write to diffuse reg
   VSF_LIGHT_END_ALPHA,    //multiply current lighting temp value by vertex alpha component and write to diffuse
   VSF_SPECULAR_BEGIN,     //initialize lighting computation (specular)
   VSF_SPECULAR_END,       //finish lighting computation, write to specular reg
   VSF_LIGHT_DIR_OS,       //single directional light (object space)
   VSF_LIGHT_DIR_WS,       //''                       (world space)
   //VSF_LIGHT_DIR_OS_DS,    //''                       (object space, double-sided)
   VSF_LIGHT_POINT_OS,     //single point light (object space)
   VSF_LIGHT_POINT_WS,     //''                 (world space)
   VSF_LIGHT_SPOT_OS,      //single spot light (object space)
   VSF_LIGHT_SPOT_WS,      //''                (world space)
   //VSF_LIGHT_POINT_OS_DS,  //''                 (object space, double-sided)
   VSF_LIGHT_POINT_AMB_OS, //single point ambient light (object space)
   VSF_LIGHT_POINT_AMB_WS, //single point ambient light (world space)
   VSF_DIFFUSE_COPY,       //plain copy of vertex diffuse element
   VSF_DIFFUSE_MUL_ALPHA,  //copy vertex diffuse, modulate vertex alpha with source diffuse
   VSF_COPY_ALPHA,         //copy alpha from vertex to diffuse element
   VSF_SIMPLE_DIR_LIGHT,   //simple dp3 directional light (for debug drawing)
   VSF_TEXKILL_PROJECT_OS,    //project plane for texture kill
   VSF_TEXKILL_PROJECT_WS,    //project plane for texture kill

   VSF_BUMP_OS,               //bump-mapping computation (light C_vector transform to texture space)
   VSF_BUMP_WS,
   VSF_LAST
};

                           //VS constant registers
enum E_VS_CONSTANT{
   VSC_CAM_LOC_POS,        //camera local position (xyz), plus uv shift scale (w)
   VSC_LIGHT_DIR_NORMAL,   //directional light, normal
   VSC_LIGHT_DIR_DIFFUSE,  //directional light, diffuse color
   VSC_LIGHT_POINT_POS_FR, //point light, position + far range
   VSC_LIGHT_POINT_DIFFUSE_RRD,  //point light, diffuse color + reciprocal of range delta
   VSC_LIGHT_SPOT_DIR_COS, //spot light direction + outer range cos
   VSC_LIGHT_LAYERED_PLANE,//layered light, plane
   VSC_LIGHT_LAYERED_COL_R,//layered light, color (xyz) and power (w)
   VSC_LIGHT_LAYERED_POS,  //layered light, position (xyz)
   VSC_LIGHT_PFOG_CONE_TOP,//pointfog light, cone top (xyz) and r_cone_cos_angle (w)
   VSC_LIGHT_PFOG_CONE_DIR,//pointfog light, cone dir (xyz) and multiplier (w)
   VSC_LIGHT_PFOG_COLOR,   //pointfog light, color (xyz) and power (w)
   VSC_TXT_CLIP_PLANE,     //texture clipping plane
   VSC_UV_TRANSFORM,       //3x2 uv transformation matrix (2 slots)
   VSC_MATRIX_FRAME,          //4x3 frame's world matrix
   VSC_UV_SHIFT,            //uv shifting
   //VSC_TEXKILL_PLANE,      //plane for texkill coord generator
   VSC_LAST
};
enum E_VS_FIXED_CONSTANT{
   VSC_CONSTANTS = 0,         //constants: [0.0, 0.5, 1.0, 2.0]
   VSC_MAT_TRANSFORM_0 = 1,   //transposed transformation matrix (4 slots!)
   VSC_FOGPARAMS = 5,         //distance fog parameters: [fog_start, fog_end, 1.0/fog_range, 0.0]
   VSC_AMBIENT = 6,           //ambient light
   VSC_DEBUG = 7,             //value used for debugging
   VSC_TEXKILL = 7,           //texkill plane
   VSC_UV_SHIFT_SCALE = 8,    //uv shifting/scaling value
   VSC_PARTICLE_MATRIX_BASE = 8, //base matrix for particle rendering
   VSC_MAT_TRANSFORM_1 = 9,   //second matrix used for geometry blending (4 slots!)
   VSC_MAT_BLEND_BASE = 9,    //base for matrix palette blending, each matrix consumes 3 slots
   VSC_MAT_BLEND_END = VSC_MAT_BLEND_BASE + MAX_VS_BLEND_MATRICES*3 - 1,
                           //last value, until this, constants are reserved (NVLink can't use them)
   VSC_FIXED_LAST          
};


#define MAX_PS_FRAGMENTS 8    //max # of fragments in ps program

enum E_PS_FRAGMENT{
   PSF_TEX_0,                 //texture declarators for n textures
   PSF_TEX_1,
   PSF_TEX_2,
   PSF_TEX_3,
   PSF_TEX_1_BEM,             //EMBM (t0=embm, t1=sampled)
   PSF_TEX_2_BEM,             //EMBM (t1=embm, t2=sampled)
   PSF_TEX_3_BEM,             //EMBM (t2=embm, t3=sampled)
   PSF_TEX_1_BEML,            //EMBM+LUM (t0=embm, t1=sampled)
   PSF_TEX_2_BEML,            //EMBM+LUM (t1=embm, t2=sampled)
   PSF_TEX_3_BEML,            //EMBM+LUM (t2=embm, t3=sampled)
   PSF_TEXKILL_0,
   PSF_TEXKILL_1,
   PSF_TEXKILL_2,
   PSF_TEXKILL_3,
   PSF_FIRST_NOTEX,
   PSF_TEST = PSF_FIRST_NOTEX,
   PSF_v0_COPY,               //simple copy of diffuse color from vertices
   PSF_v0_MUL2X,              //simple copy of diffuse color from vertices * 2
   PSF_COLOR_COPY,            //simple copy of diffuse color from constant (PSC_COLOR)
   PSF_MOD_COLOR_v0,          //modulate PSC_COLOR with v0
   PSF_COPY_BLACK,            //copy black
   PSF_COPY_BLACK_t0a,        //copy black and alpha from t0
   PSF_t0_COPY,               //simple copy of texture 0
   PSF_t0_COPY_inv,           //copy of texture 0 inversed (1-t0)
   PSF_t1_COPY,               //simple copy of texture 1
   PSF_MOD_t0_v0,             //modulation t0 * v0
   PSF_MOD_r0_t1,             //modulation r0 * t1
   PSF_MODX2_t0_v0,           //modulation_x2 t0 * v0
   PSF_MODX2_r0_t0,           //modulation_x2 r0 * t1
   PSF_MODX2_r0_t1,           //modulation_x2 r0 * t1
   PSF_MODX2_r0_t2,           //modulation_x2 r0 * t2
   PSF_MODX2_r0_t3,           //modulation_x2 r0 * t2
   PSF_MODX2_t0_t1,           //modulation_x2 t0 * t1
   PSF_MODX2_r0_r1,           //modulation_x2 r0 * r1
   PSF_MODX2_t0_r1,           //modulation_x2 t0 * r1
   PSF_MOD_t0_CONSTCOLOR,     //modulate texture by constant color (PSC_COLOR)
   PSF_GRAYSCALE,             //linearly modulate to grayscale using PSC_FACTOR.a (PSC_FACTOR.rgb must be [.3, .6, .1])
   PSF_ADD_t0_v0,
   PSF_ADD_t1_v0,
   PSF_SHADOW_CAST,           //set r0.rgb to PSC_COLOR, and r0.a to t0.a
   PSF_SHADOW_RECEIVE,        //set r0.rgb to t0, and r0.a to v0.a
   PSF_r0_B_2_A,              //copy r0.b to r0.a
   PSF_BLEND_BY_ALPHA,        //blend between t1 and t2 using t0.a
   PSF_NIGHT_VIEW,            //using t0 as input, generate black-green-white gamma-raised night-vision look
   PSF_LAST
};

enum E_PS_FIXED_CONSTANT{
   PSC_ZERO = 0,              //[0, 0, 0, 0]
   PSC_ONE = 1,               //[1, 1, 1, 1]
   PSC_FACTOR = 2,            //tex/alpha factor color
   PSC_FACTOR1 = 3,
   PSC_COLOR = 4,             //fixed constant color
   PSC_COLOR1 = 5,
   PSC_DEBUG = 7,             //value used for debugging
};

//----------------------------
#ifdef GL

class C_gl_shader: public C_unknown{
public:
   enum E_TYPE{
      TYPE_VERTEX,
      TYPE_FRAGMENT,
   };
   int gl_id;
   C_gl_shader():
      gl_id(0)
   {}
   ~C_gl_shader(){
      if(gl_id)
         glDeleteShader(gl_id);
   }
   bool Build(E_TYPE type, const char *src, dword len = 0);
   inline operator int() const{ return gl_id; }
};

//----------------------------

class C_gl_program: public C_unknown{
   C_gl_shader shd_vertex, shd_fragment;
   I3D_driver &drv;
public:
   int gl_prg_id;
   C_gl_program(I3D_driver &d):
      drv(d),
      gl_prg_id(0)
   {}
   ~C_gl_program();
   bool Build(const char *v_shader, const char *f_shader, const char *attr_names, dword v_len = 0, dword f_len = 0);
   virtual void BuildStoreAttribId(dword index, dword id) = 0;

   void Use();
   inline operator int() const{ return gl_prg_id; }
};

#endif
//----------------------------

#define MAX_TEXTURE_STAGES 8  //max texture stages we may use simlultaneously

class I3D_driver{
   dword ref;
                              //drv_flags:
#define DRVF_USEZB            1
#define DRVF_CLEAR            2
#define DRVF_TEXTURECOMPRESS  4
#define DRVF_WIREFRAME        8

#define DRVF_DRAWBBOX         0x10
#define DRVF_DRAWHRBBOX       0x20
#define DRVF_DRAWHRLINK       0x40
#define DRVF_LOADMIPMAP       0x80

#define DRVF_LINFILTER        0x100
#define DRVF_MIPMAP           0x200
#define DRVF_EMBMMAPPING      0x400
#define DRVF_DRAWMIRRORS      0x800

#define DRVF_HASSTENCIL       0x1000      //stencil buffer is attached
#define DRVF_DRAWSECTORS      0x2000
#define DRVF_DRAWPORTALS      0x4000
#define DRVF_TEXTDITHER       0x8000

#define DRVF_DRAWVISUALS      0x10000
#define DRVF_USEFOG           0x20000
#define DRVF_CANRENDERSHADOWS 0x40000     //hardware is capable to render shadows
#define DRVF_USE_ANISO_FILTER 0x80000     //use anisotropic filtering (RS_ANISO_FILTERING)

#ifndef GL
#define DRVF_USESHADOWS       0x100000
#define DRVF_DEBUGDRAWSHADOWS 0x200000
#define DRVF_DEBUGDRAWSHDRECS 0x400000
#endif
#define DRVF_DEBUGDRAWBSP     0x800000

#define DRVF_DEBUGDRAWDYNAMIC 0x1000000
#define DRVF_DITHER           0x2000000
#define DRVF_USELMAPPING      0x4000000
#define DRVF_DRAWLMTEXTURES   0x8000000

#define DRVF_SINGLE_PASS_MODULATE   0x10000000  //single-pass multi-texturing for MODULATE
#define DRVF_SINGLE_PASS_MODULATE2X 0x20000000  //single-pass multi-texturing for MODULATE2X
#define DRVF_ANISOTROPY_ON    0x40000000  //anisotropy filtering is on
#define DRVF_SOUND_ENV_RESET  0x80000000  //can use D3DTA_COMPLEMENT blend stage argument


                              //drv_flags2:
#define DRVF2_DRAWCAMERAS     1
#define DRVF2_DRAWDUMMYS      2
#define DRVF2_DETAILMAPPING   4        //use detail mapping
#define DRVF2_DRAWOCCLUDERS   8

#define DRVF2_DRAWTEXTURES    0x10
#define DRVF2_DRAWVOLUMES     0x20
#ifndef GL
#define DRVF2_CAN_MODULATE2X  0x40     //can set D3DTSS_COLOROP = D3DTOP_MODULATE2X
#define DRVF2_CAN_TXTOP_ADD   0x80     //can set D3DTSS_COLOROP = D3DTOP_ADD
#endif

#define DRVF2_DRAWSOUNDS      0x100
#define DRVF2_DRAWLIGHTS      0x200
#define DRVF2_LMTRUECOLOR     0x400    //use true-color lightmaps
#define DRVF2_LMDITHER        0x800    //dither highcolor lightmaps

#define DRVF2_DRAWJOINTS      0x1000    
#ifndef GL
#define DRVF2_DIRECT_TRANSFORM 0x2000  //using direct transforming, no intermediate ProcessVertices calls
#endif
#define DRVF2_CAN_USE_UBYTE4  0x4000   //vertex declarator type D3DVSDT_UBYTE4 can be used
#define DRVF2_USE_OCCLUSION   0x8000   //use occlusion testing

#ifndef GL
#define DRVF2_USE_PS          0x10000  //use pixel shader
#endif
#define DRVF2_ENVMAPPING      0x20000  //use environment mapping
#define DRVF2_USE_EMBM        0x40000  //use embm mapping
#define DRVF2_DEBUGDRAWSTATIC 0x80000  //draw frames which are collision static

#define DRVF2_DRAWCOLTESTS    0x100000 //draw collision testing by lines
#ifndef GL
#define DRVF2_TEXCLIP_ON      0x200000 //simulate clip planes by texture
#endif
#define DRVF2_CAN_RENDER_MIRRORS 0x400000 //set if mirrors can be rendered
#ifndef GL
#define DRVF2_DEBUG_SHOW_OVERDRAW 0x800000 //show overdraw in colors
#define DRVF2_NIGHT_VISION    0x1000000   //use night-vision effect for rendering
#define DRVF2_IN_NIGHT_VISION 0x2000000   //inside of night-vision rendering
#endif

//----------------------------

   IDirect3D9 *pD3D_driver;        //if NULL, interface is uninitialized
   D3DCAPS9 d3d_caps;
   IDirect3DDevice9 *d3d_dev;
   dword drv_flags, drv_flags2;
   I3DINIT init_data;
   dword render_time;         //counter set to current time (IGraph::ReadTimer) each time a scene is rendered
   dword in_scene_count;      //counter for proper BeginScene/EndScene calls

   bool debug_draw_mats;

   dword reapply_volume_count;//override dsound bug - reapply volume after some several rendered frames after SetFocus

#ifndef GL
   bool is_hal;
#endif
   float global_sound_volume;
public:

   inline float GetGlobalSoundVolume() const{ return global_sound_volume; }

#ifndef GL
//----------------------------
// Check if hardware device is being used.
   inline bool IsHardware() const{ return is_hal; }
#endif

//----------------------------
// Check if device supports hardware vertex processing.
   inline bool IsDirectTransform() const{
#ifndef GL
      return (drv_flags2&DRVF2_DIRECT_TRANSFORM);
#else
      return true;
#endif
   }

//----------------------------
// Check if devide has support for pixel shader.
   inline bool CanUsePixelShader() const{
#ifdef GL
      return true;
#else
      return (drv_flags2&DRVF2_USE_PS);
#endif
   }

   inline const D3DCAPS9 *GetCaps() const{ return &d3d_caps; }

   HINSTANCE h_nvlinker;      //HINSTANCE of nvlinker
   INVLink *nv_linker[2];     //[vertex_shading|pixel_shading]

                              //vertex shader opcodes for dcl_usage instructions for particular input registers
   enum E_VS_DECLARATION{
      VSDECL_POSITION,        //v0
      VSDECL_NORMAL,          //v2
      VSDECL_DIFFUSE,         //v2
      VSDECL_SPECULAR,        //v5
      VSDECL_TEXCOORD01,      //v1.xyzw   (tex0 + tex1)
      VSDECL_TEXCOORD23,      //v3.xyzw   (tex0 + tex1)
      VSDECL_BLENDWEIGHT,     //v4
      VSDECL_TXSPACE_S,       //v5
      VSDECL_TXSPACE_T,       //v6
      VSDECL_TXSPACE_SxT,     //v7
      VSDECL_LAST,
   };
   typedef dword t_vsdecl[3];
   t_vsdecl vsdecls[2][VSDECL_LAST];   //[shader version - 1.1 | 3.0]

   dword vs_fragment_ids[VSF_LAST];
   dword vs_constant_ids[VSC_LAST];
   dword ps_fragment_ids[PSF_LAST];

                              //shader cache
   struct S_shader_entry_base{
      __int64 sort_value;       //value used for fast look-up
      inline bool operator <(const S_shader_entry_base &se) const{
         return (sort_value < se.sort_value);
      }
   };

                              //** vertex shader cache **
   struct S_vs_shader_entry_in: public S_shader_entry_base{
      dword num_fragments;
      E_VS_FRAGMENT fragment_code[MAX_VS_FRAGMENTS];

      S_vs_shader_entry_in():
         num_fragments(0)
      {
                              //begin sort value with bitmask of declarator
         sort_value = 0;
      }
      enum{ VSF_NUM_BITS = 6 };  //number of bits sufficiently holding all VSF_ values (except of VSF_LAST)

      inline void AddFragment(E_VS_FRAGMENT vsf){
         assert(num_fragments < MAX_VS_FRAGMENTS);
         sort_value ^= Rotl(vsf, num_fragments*VSF_NUM_BITS);
         fragment_code[num_fragments++] = vsf;
      }
      inline void PopFragment(){
         assert(num_fragments);
         --num_fragments;
         sort_value ^= Rotl(fragment_code[num_fragments], num_fragments*VSF_NUM_BITS);
      }
      inline void CopyUV(dword src_stage = 0, dword dst_stage = 0){
         assert(src_stage<4);
         assert(dst_stage<4);
         AddFragment((E_VS_FRAGMENT)(VSF_PICK_UV_0+src_stage));
         AddFragment((E_VS_FRAGMENT)(VSF_STORE_UV_0+dst_stage));
      }
   };

   struct S_vs_shader_entry: public S_shader_entry_base{
      C_smart_ptr<IDirect3DVertexShader9> vs;
      dword last_render_time; //value used for caching
                              //slots for constants:
      dword vscs_cam_loc_pos; //valid only if VSF_GENERATE_ENV_UV is present
      dword vscs_txt_clip;    //valid only if VSF_TXT_CLIP_??_? is present
      dword vscs_uv_transform;//valid only if VSF_UV_TRANSFORM is present
      dword vscs_mat_frame;   //valid only if VSC_MATRIX_FRAME is present
      dword vscs_uv_shift;    //valid only if VSF_SHIFT_UV is present

      dword vscs_light_param[MAX_VS_LIGHTS * 3];   //reserve 2 constant slots per light

      S_vs_shader_entry(const S_shader_entry_base &se)
      {
         sort_value = se.sort_value;
      }
                              //assignment, not implmented, can't be used
      S_vs_shader_entry &operator =(const S_vs_shader_entry &se);
   };
                              //vertex-shader cache
   typedef set<S_vs_shader_entry> vs_set;
   vs_set vs_cache;

//----------------------------
// Get vertex shader from provided fragment code. The input entry must have initialized
// 'shd_decl' and 'num_fragments'.
// The output entry contains all valid members.
// If the call fails, the returned value is NULL.
   S_vs_shader_entry *GetVSHandle(const S_vs_shader_entry_in &se);


                              //** pixel shader cache **

   struct S_ps_shader_entry_in: public S_shader_entry_base{
      dword num_fragments;
      E_PS_FRAGMENT fragment_code[MAX_PS_FRAGMENTS];

      S_ps_shader_entry_in():
         num_fragments(0)
      {
         sort_value = 0;
      }
      enum{ PSF_NUM_BITS = 5 };  //number of bits sufficiently holding all PSF_ values (except of PSF_LAST)

      inline void AddFragment(E_PS_FRAGMENT psf){
         assert(num_fragments < MAX_PS_FRAGMENTS);
         sort_value ^= _rotl(psf, num_fragments*PSF_NUM_BITS);
         fragment_code[num_fragments++] = psf;
      }
      inline void PopFragment(){
         assert(num_fragments);
         --num_fragments;
         sort_value ^= _rotl(fragment_code[num_fragments], num_fragments*PSF_NUM_BITS);
      }
      inline void Tex(dword stage){ assert(stage<4); AddFragment((E_PS_FRAGMENT)(PSF_TEX_0 + stage)); }
      inline void TexBem(dword stage){ assert(stage<4); AddFragment((E_PS_FRAGMENT)(PSF_TEX_1_BEM + stage - 1)); }
      inline void TexBeml(dword stage){ assert(stage<4); AddFragment((E_PS_FRAGMENT)(PSF_TEX_1_BEML + stage - 1)); }
      inline void TexKill(dword stage){ assert(stage<4); AddFragment((E_PS_FRAGMENT)(PSF_TEXKILL_0 + stage)); }
      inline void Mod2X(dword stage){ assert(stage<4); AddFragment((E_PS_FRAGMENT)(PSF_MODX2_r0_t0 + stage)); }
   };

   struct S_ps_shader_entry: public S_shader_entry_base{
      C_smart_ptr<IDirect3DPixelShader9> ps;
      dword last_render_time; //value used for caching

      S_ps_shader_entry(const S_shader_entry_base &se)
      {
         sort_value = se.sort_value;
      }
                              //assignment, not implmented, can't be used
      S_ps_shader_entry &operator =(const S_ps_shader_entry &se);
   };
                              //vertex-shader cache
   typedef set<S_ps_shader_entry> ps_set;
   ps_set ps_cache;

//----------------------------
// Get shader from provided fragment code. The input entry must have initialized
// 'shd_decl' and 'num_fragments'.
// The output entry contains all valid members.
// If the call fails, the returned value is NULL.
   S_ps_shader_entry *GetPSHandle(S_ps_shader_entry_in &se);

//----------------------------
#ifdef GL
   enum E_GL_PROGRAM{
      GL_PROGRAM_DRAW_LINES,
      GL_PROGRAM_DRAW_TRIANGLES,
      GL_PROGRAM_VISUAL,
      GL_PROGRAM_LAST
   };
   C_smart_ptr <C_gl_program> gl_shader_programs[GL_PROGRAM_LAST];
#endif
//----------------------------
                              //callback
   void *cb_context;
   PI3D_CALLBACK cb_proc;

   int force_lod_index;       //-1 = default
   float lod_scale;
   float shadow_range_n, shadow_range_f;

                              //multiple VB managers for various vertex formats
#ifndef GL
   C_vector<C_smart_ptr<C_RB_manager<IDirect3DVertexBuffer9> > > vb_manager_list;
   C_vector<C_smart_ptr<C_RB_manager<IDirect3DIndexBuffer9> > > ib_manager_list;
#endif
   C_smart_ptr<IDirect3DVertexBuffer9> vb_rectangle;  //initialized vertex buffer for rendering rectangle primitives
   C_smart_ptr<IDirect3DVertexDeclaration9> vs_decl_rectangle; //vs declaration for above
   C_smart_ptr<IDirect3DVertexBuffer9> vb_particle;   //initialized vertex buffer for rendering particles
   C_smart_ptr<IDirect3DIndexBuffer9> ib_particle;    //initialized index buffer for rendering of flares
   C_smart_ptr<IDirect3DVertexBuffer9> vb_clear;   //screen rectangle for clearing entire screen area

#ifdef _DEBUG
   C_block_profiler *block_profiler;
   C_smart_ptr<I3D_scene> block_profiler_scene; //helper scene for profiler rendering
#endif

                              //program database
   C_database dbase;

//----------------------------
// Create depth buffer of desired size, compatible with provided pixel format.
   I3D_RESULT CreateDepthBuffer(dword sx, dword sy, const S_pixelformat &fmt, IDirect3DSurface9 **zb);

//----------------------------
// Initialize render target - create all render surfaces (as textures) and associated depth buffer.
   template<int NUM, class T>
   I3D_RESULT InitRenderTarget(C_render_target<NUM, T> &rt, dword ct_flags, dword sx, dword sy){
      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      ct.flags = ct_flags;
      ct.size_x = sx;
      ct.size_y = sy;
      for(int i=NUM; i--; ){
         PI3D_texture tp;
         I3D_RESULT ir = CreateTexture(&ct, &tp);
         if(I3D_FAIL(ir)){
            rt.Close();
            return ir;
         }
         rt.rt[i] = tp;
         tp->Release();
      }
      IDirect3DSurface9 *zb;
      I3D_RESULT ir = CreateDepthBuffer(sx, sy, rt->GetPixelFormat(), &zb);
      if(SUCCEEDED(ir)){
         rt.zb = zb;
         zb->Release();
      }else{
         rt.Close();
      }
      return ir;
   }

private:
//----------------------------
                              //render targets
   C_render_target<> default_render_target;
   C_render_target<> curr_render_target;

                              //night-vision mode
   C_render_target<1, I3D_texture_base> rt_night_vision;

                              //VS declarator cache, only growing until driver is released
   C_vector<C_vector<dword> *> vs_decl_cache;

   dword texture_sort_id;     //texture is, assigned to texture at creation, used for sorting
   
public:
   map<dword, dword*> fvf_to_decl_map; //used by I3D_scene::DrawTriangles


   typedef map<dword, I3D_collision_mat> t_col_mat_info;
private:
   t_col_mat_info col_mat_info;
   dword stencil_ref;
   D3DSTENCILOP stencil_pass, stencil_fail, stencil_zfail;
   D3DCMPFUNC stencil_func;
   bool stencil_on;

public:
   inline const t_col_mat_info &GetColMatInfo() const{ return col_mat_info; }

   inline void SetStencilRef(dword r){ if(stencil_ref!=r) d3d_dev->SetRenderState(D3DRS_STENCILREF, stencil_ref = r); }
   inline void SetStencilPass(D3DSTENCILOP op){ if(stencil_pass!=op) d3d_dev->SetRenderState(D3DRS_STENCILPASS, stencil_pass = op); }
   inline void SetStencilFail(D3DSTENCILOP op){ if(stencil_fail!=op) d3d_dev->SetRenderState(D3DRS_STENCILFAIL, stencil_fail = op); }
   inline void SetStencilZFail(D3DSTENCILOP op){ if(stencil_zfail!=op) d3d_dev->SetRenderState(D3DRS_STENCILZFAIL, stencil_zfail = op); }
   inline void SetStencilFunc(D3DCMPFUNC f){ if(stencil_func!=f) d3d_dev->SetRenderState(D3DRS_STENCILFUNC, stencil_func = f); }
   inline void StencilEnable(bool on){ if(stencil_on!=on) d3d_dev->SetRenderState(D3DRS_STENCILENABLE, stencil_on = on); }

   /*
//----------------------------
// Register new vertex-shader declarator, return pointer to it.
   dword *RegisterVSDeclarator(const C_vector<dword> &decl);
   dword *RegisterVSDeclarator(dword fvf);
   void BuildVSDeclarator(dword fvf, dword decl[11]);
   */
#ifndef GL
                              //shadow rendering:
   C_smart_ptr<I3D_texture> tp_shadow; //range texture
   C_render_target<2, I3D_texture_base> rt_shadow; //[rendertarget|blurred]

                              //bluring rectangles
   C_smart_ptr<IDirect3DVertexBuffer9> vb_blur;
   C_smart_ptr<IDirect3DIndexBuffer9> ib_blur;
   struct S_blur_vertex{
      S_vectorw v;
      dword diffuse;
      I3D_text_coor uv;
   };
   enum{ NUM_SHADOW_BLUR_SAMPLES = 9 };

                              //clipping by texture:
   S_plane pl_clip;
#endif

                              //temp vertex and index buffers:
#define IB_TEMP_SIZE 0xc000   //number of bytes
   C_smart_ptr<IDirect3DIndexBuffer9> ib_temp;
   dword ib_temp_base_index;

#define VB_TEMP_SIZE 32768     //number of bytes
   C_smart_ptr<IDirect3DVertexBuffer9> vb_temp;
   dword vb_temp_base_index;


   void SetRenderTarget(C_render_target<> &rt, int indx = 0){

                              //setup new values
      curr_render_target = rt;
      HRESULT hr;
      hr = d3d_dev->SetRenderTarget(0, rt.rt[indx]);
      CHECK_D3D_RESULT("SetRenderTarget", hr);
      hr = d3d_dev->SetDepthStencilSurface(rt.zb);
      CHECK_D3D_RESULT("SetDepthStencilSurface", hr);

      drv_flags &= ~DRVF_HASSTENCIL;
      if(rt.zb){
         D3DSURFACE_DESC desc;
         rt.zb->GetDesc(&desc);
         switch(desc.Format){
         case D3DFMT_D15S1: case D3DFMT_D24S8: case D3DFMT_D24X4S4: drv_flags |= DRVF_HASSTENCIL; break;
         }
      }
   }
   template<int N>
   void SetRenderTarget(C_render_target<N, I3D_texture_base> &rt, int indx = 0){
      IDirect3DSurface9 *surf;
      HRESULT hr;
      hr = ((IDirect3DTexture9*)rt.rt[indx]->GetD3DTexture())->GetSurfaceLevel(0, &surf);
      CHECK_D3D_RESULT("GetSurfaceLevel", hr);
      C_render_target<> rt1(surf, rt.zb);
      SetRenderTarget(rt1);
      surf->Release();
   }

   const C_render_target<> &GetCurrRenderTarget() const{ return curr_render_target; }
public:

//----------------------------

   inline void SetVSConstant(dword index, const void *data, dword num = 1){
      assert(index+num <= 96);
      HRESULT hr;
      hr = d3d_dev->SetVertexShaderConstantF(index, (const float*)data, num);
      CHECK_D3D_RESULT("SetVertexShaderConstantF", hr);
   }

//----------------------------

   inline void SetPSConstant(dword index, const void *data, dword num = 1){
      assert(index+num <= 8);
      HRESULT hr;
      hr = d3d_dev->SetPixelShaderConstantF(index, (const float*)data, num);
      CHECK_D3D_RESULT("SetPixelShaderConstantF", hr);
   }
#ifndef GL
   inline const S_plane &GetTexkillPlane() const{ return pl_clip; }
#endif
//----------------------------
// Update D3D viewport. The rectangle must fit withing current render target (no check made).
   void UpdateViewport(const I3D_rectangle &rc_ltrb);

//----------------------------
// Update viewport, specified in screen coords. If render target size is different than screen size,
// the coordinates are re-scaled.
   void UpdateScreenViewport(const I3D_rectangle &rc_ltrb);

private:
   void RefreshAfterReset();

//----------------------------
                              //fog

   bool fog_enable;               //fog in 3D API (DirectX, OpenGL) enable flag
   dword d3d_fog_color;
public:
   inline void EnableFog(bool on){
      if(fog_enable != on){
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_FOGENABLE, fog_enable = on);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
   }
   inline bool IsFogEnabled() const{ return fog_enable; }

   inline void SetFogColor(dword color){
      if(d3d_fog_color!=color){
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_FOGCOLOR, d3d_fog_color = color);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
   }
   inline dword GetFogColor() const{ return d3d_fog_color; }

   inline bool IsInScene() const{ return (in_scene_count>0); }

private:
//----------------------------
   D3DBLEND blend_table[2][14];    //[src/dest][D3DBLEND]
                              //blending cache
   dword srcblend, dstblend;
#ifdef GL
   dword gl_srcblend, gl_dstblend;
#endif
   bool alpha_blend_on;
   dword num_available_txt_stages;

   C_vector<S_pixelformat> texture_formats[2]; //[texture|RTT texture]
   C_vector<C_vector<D3DFORMAT> > rtt_zbuf_fmts; //depth surfaces usable for each rendertarget

//----------------------------
// Initialize D3D and all associated resources.
   I3D_RESULT InitD3DResources();

//----------------------------
   void CloseD3DResources();

//----------------------------
   void ReleaseHWVertexBuffers();

//----------------------------
// Initialize light-map pixel format convertor.
   void InitLMConvertor();

#ifndef GL
//----------------------------
// Init all resources associated with real-time shadow rendering.
   void InitShadowResources();
#endif
                              //internal resource lists
   C_vector<PI3D_texture> managed_textures;
   C_vector<PI3D_sound> sounds;

#ifndef GL
                              //list of hardware vertex buffers and their references
   C_vector<class I3D_dest_vertex_buffer*> hw_vb_list;
#endif

//----------------------------
                              //directories
   C_vector<C_str> dirs[I3DDIR_LAST];

public:
   const C_vector<C_str> &GetDirs(I3D_DIRECTORY_TYPE dt) const{ return dirs[dt]; }
   mutable dword last_dir_index[I3DDIR_LAST];     //index of last directory

private:
//----------------------------
                              //debugging stats
   mutable dword interface_count[I3D_CLID_LAST];

   friend I3D_scene;
   friend I3D_sound;
   friend I3D_light;

   I3D_driver();
   friend I3D_RESULT I3DAPI I3DCreate(PI3D_driver *drvp, CPI3DINIT id);

   I3D_RESULT Init(CPI3DINIT);
   void Close();

   mutable HINSTANCE hi_res;  //hinstance of DLL containing resources for debug drawing
public:
   HINSTANCE GetHInstance() const;
                              //callback used when device is reset
   enum E_RESET_MSG{
      RESET_RELEASE,          //notification before device reset (all resources must be released now)
      RESET_RECREATE,         //notification after device reset (resources may be recreated)
   };
   class C_reset_callback{
   public:
      virtual void ResetCallback(E_RESET_MSG msg) = 0;
   };

   void RegResetCallback(C_reset_callback *rb){
      reset_callbacks.push_back(rb);
   }
   void UnregResetCallback(C_reset_callback *rb){
      for(int i=reset_callbacks.size(); i--; ){
         if(reset_callbacks[i]==rb){
            reset_callbacks[i] = reset_callbacks.back(); reset_callbacks.pop_back();
            break;
         }
      }
   }
private:
                              //all registered callbacks
   C_vector<C_reset_callback*> reset_callbacks;

public:
   ~I3D_driver();
//----------------------------
                              //cache states
   dword last_blend_flags;
   C_smart_ptr<IDirect3DVertexShader9> last_vs;
   C_smart_ptr<IDirect3DVertexDeclaration9> last_vs_decl;
   C_smart_ptr<IDirect3DPixelShader9> last_ps;
   dword last_fvf;

   dword last_texture_factor;
   dword last_alpha_ref;
   bool alpharef_enable;
   float last_embm_scale[MAX_TEXTURE_STAGES];
   dword last_txt_coord_index[MAX_TEXTURE_STAGES];
   D3DTEXTUREOP stage_txt_op[MAX_TEXTURE_STAGES], stage_alpha_op[MAX_TEXTURE_STAGES];

   bool stage_anisotropy[MAX_TEXTURE_STAGES];
   dword curr_anisotropy;
   D3DTEXTUREFILTERTYPE stage_minf[MAX_TEXTURE_STAGES], min_filter[2];    //[normal|aniso]

   CPI3D_texture_base last_texture[MAX_TEXTURE_STAGES];   //multi-texturing
   //bool d3d_clipping;
                              //stream0 cache:
   IDirect3DVertexBuffer9 *last_stream0_vb;
   dword last_stream0_stride;

   class C_vs_decl_key: public C_vector<D3DVERTEXELEMENT9>{
   public:
      bool operator <(const C_vs_decl_key &key) const{
         if(size() < key.size())
            return true;
         if(size() > key.size())
            return false;
         int cmp = memcmp(&front(), &key.front(), size()*sizeof(D3DVERTEXELEMENT9));
         return (cmp<0);
      }
   };
   typedef map<C_vs_decl_key, C_smart_ptr<IDirect3DVertexDeclaration9> > t_vs_declarators;
   t_vs_declarators vs_declarators;

   typedef map<dword, C_smart_ptr<IDirect3DVertexDeclaration9> > t_vs_declarators_fvf;
   t_vs_declarators_fvf vs_declarators_fvf;

   inline void SetStreamSource(IDirect3DVertexBuffer9 *vb, dword vs){
      if(last_stream0_vb!=vb || last_stream0_stride!=vs){
         HRESULT hr;
         hr = d3d_dev->SetStreamSource(0, last_stream0_vb=vb, 0, last_stream0_stride=vs);
         CHECK_D3D_RESULT("SetStreamSource", hr);
      }
   }
   inline void ResetStreamSource(){ last_stream0_vb = 0; }

   IDirect3DIndexBuffer9 *last_ib;
   inline void SetIndices(IDirect3DIndexBuffer9 *ib){
      if(last_ib!=ib){
         HRESULT hr;
         hr = d3d_dev->SetIndices(last_ib=ib);
         CHECK_D3D_RESULT("SetIndices", hr);
      }
   }


   inline void EnableAnisotropy(int stage, bool b){
      if(stage_anisotropy[stage]!=b){
         stage_anisotropy[stage] = b;
         D3DTEXTUREFILTERTYPE minf = min_filter[b];
         if(stage_minf[stage]!=minf){
            HRESULT hr;
            hr = d3d_dev->SetSamplerState(stage, D3DSAMP_MINFILTER, stage_minf[stage] = minf);
            CHECK_D3D_RESULT("SetSamplerState", hr);
         }
      }
   }

private:
   bool b_cullnone;           //true when no culling
   bool zw_enable;            //true when z-writing is enabled
   D3DCULL cull_default;      //default culling order; normally CCW, may be changed when mirrors are rendered

public:

#ifndef GL
                              //vertex lighting mode
   D3DTEXTUREOP vertex_light_blend_op;    //MODULATE or MODULATE2X, depending on caps
#endif
   float vertex_light_mult;               //multiplier for vertex lighting

//----------------------------
// Set alpha reference value.
   inline void SetAlphaRef(dword ar){

      if(last_alpha_ref!=ar){
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_ALPHAREF, last_alpha_ref = ar);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
   }

//----------------------------
// Enable/disable alpha testing.
   inline void SetupAlphaTest(bool on){

      if(alpharef_enable!=on){
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_ALPHATESTENABLE, alpharef_enable = on);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
   }

   inline void SetTexture1(dword i, const I3D_texture_base *tb){
      if(last_texture[i]!=tb){
         last_texture[i] = tb;
         IDirect3DBaseTexture9 *new_t = tb ? tb->GetD3DTexture() : NULL;
         HRESULT hr;
         hr = d3d_dev->SetTexture(i, new_t);
         CHECK_D3D_RESULT("SetTexture", hr);
#ifdef GL
         dword tid = tb ? tb->GetGlId() : 0;
         glBindTexture(GL_TEXTURE_2D, tid);
#endif
      }
   }

//----------------------------
#ifndef GL
   inline void SetDefaultCullMode(D3DCULL cm){

      cull_default = cm;
      if(!b_cullnone){
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_CULLMODE, cull_default);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
   }

#endif
//----------------------------

   inline D3DCULL GetDefaultCullMode() const{ return cull_default; }
//----------------------------

   inline void EnableNoCull(bool b){

      if(b_cullnone != b){
         b_cullnone = b;
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_CULLMODE, b_cullnone ? D3DCULL_NONE : cull_default);
         CHECK_D3D_RESULT("SetRenderState", hr);
#ifdef GL
         if(b)
            glDisable(GL_CULL_FACE);
         else
            glEnable(GL_CULL_FACE);
#endif
      }
   }

//----------------------------

   inline bool IsCullNone() const{ return b_cullnone; }

//----------------------------


   inline bool IsZWriteEnabled() const{ return zw_enable; }

//----------------------------

   inline void SetRenderMat(CPI3D_material mat, int txt_stage = 0, float alpha_mul = 1.0f){

      CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
      SetTexture1(txt_stage, tb);

                              //setup culling
      EnableNoCull(mat->Is2Sided1());
                              //setup alpha ref for translucent materials
      if(mat->IsCkeyAlpha1() && tb){
         dword ar = FloatToInt((float)mat->GetAlphaRef()*alpha_mul);
         SetAlphaRef(ar);
         SetupAlphaTest(true);
      }else{
         SetupAlphaTest(false);
      }
   }

//----------------------------
#ifndef GL
   inline void SetTextureFactor(dword tf){

      //assert(!CanUsePixelShader());
      if(last_texture_factor != tf){
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_TEXTUREFACTOR, last_texture_factor = tf);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
   }
#endif
//----------------------------

   inline void SetEMBMScale(int stage, float bs){

      if(last_embm_scale[stage] != bs){
         last_embm_scale[stage] = bs;
         HRESULT hr;
         hr = d3d_dev->SetTextureStageState(stage, D3DTSS_BUMPENVMAT00, I3DFloatAsInt(bs));
         CHECK_D3D_RESULT("SetTextureStageState", hr);
         hr = d3d_dev->SetTextureStageState(stage, D3DTSS_BUMPENVMAT11, I3DFloatAsInt(bs));
         CHECK_D3D_RESULT("SetTextureStageState", hr);
      }
   }

//----------------------------
#ifndef GL
   inline void SetTextureCoordIndex(int stage, dword index){

      if(last_txt_coord_index[stage] != index){
         assert(!IsDirectTransform());
         HRESULT hr;
         hr = d3d_dev->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX,
            D3DTSS_TCI_PASSTHRU | (last_txt_coord_index[stage]=index));
         CHECK_D3D_RESULT("SetTextureStageState", hr);
      }
   }
#endif
//----------------------------
// Return the number of texture stages maximally available.
   inline dword MaxSimultaneousTextures() const{
#ifndef GL
      if(!CanUsePixelShader())
         return Min(d3d_caps.MaxTextureBlendStages, d3d_caps.MaxSimultaneousTextures);
#endif
      return 4;
   }

//----------------------------
// Return the number of texture stages currently available.
// This may be lower than above due to clipping switched on (using texture clipper).
   inline dword NumSimultaneousTextures() const{ return num_available_txt_stages; }

//----------------------------

   inline void SetVertexShader(IDirect3DVertexShader9 *vs){

      if(vs!=last_vs){
         HRESULT hr;
         hr = d3d_dev->SetVertexShader(last_vs = vs);
         CHECK_D3D_RESULT("SetVertexShader", hr);
      }
   }

//----------------------------
// Get/create vertex shader declaration object.
// The performance of this function may be slow, not to be used frequently.
   IDirect3DVertexDeclaration9 *GetVSDeclaration(const C_vector<D3DVERTEXELEMENT9> &els);
   IDirect3DVertexDeclaration9 *GetVSDeclaration(dword fvf);

//----------------------------

   inline void SetVSDecl(IDirect3DVertexDeclaration9 *vsd){

      assert(vsd);
      if(vsd!=last_vs_decl){
         HRESULT hr;
         hr = d3d_dev->SetVertexDeclaration(last_vs_decl = vsd);
         CHECK_D3D_RESULT("SetVertexDeclaration", hr);
         last_fvf = 0;
      }
   }

//----------------------------

   inline void SetFVF(dword fvf){
      if(last_fvf != fvf){
         HRESULT hr;
         hr = d3d_dev->SetFVF(last_fvf = fvf);
         CHECK_D3D_RESULT("SetFVF", hr);
         last_vs_decl = NULL;
      }
   }

//----------------------------

   inline void SetPixelShader(IDirect3DPixelShader9 *ps){

      assert(CanUsePixelShader());
      if(ps!=last_ps){
         HRESULT hr;
         hr = d3d_dev->SetPixelShader(last_ps = ps);
         CHECK_D3D_RESULT("SetPixelShader", hr);
      }
   }

//----------------------------

   void SetPixelShader(S_ps_shader_entry_in &se);

//----------------------------

   inline dword GetFlags() const{ return drv_flags; }
   inline dword GetFlags2() const{ return drv_flags2; }

//----------------------------

   inline bool CanRenderMirrors() const{

      if(drv_flags2&DRVF2_CAN_RENDER_MIRRORS){
         if(drv_flags&DRVF_DRAWMIRRORS){
#ifndef GL
            if(drv_flags2&DRVF2_DEBUG_SHOW_OVERDRAW)
               return false;
#endif
            return true;
         }
      }
      return false;
   }

//----------------------------
#ifndef GL
   inline void SetupTextureStage(int stage, D3DTEXTUREOP op){

      //assert(!CanUsePixelShader());
      if(stage_txt_op[stage] != op){
         stage_txt_op[stage] = op;
         HRESULT hr;
         hr = d3d_dev->SetTextureStageState(stage, D3DTSS_COLOROP, op);
         CHECK_D3D_RESULT("SetTextureStageState", hr);
      }
   }

//----------------------------

   inline void DisableTextureStage(dword stage){

      //assert(!CanUsePixelShader());
      assert(stage>0);
      if(stage_txt_op[stage] != D3DTOP_DISABLE){
         HRESULT hr;
         hr = d3d_dev->SetTextureStageState(stage, D3DTSS_COLOROP, stage_txt_op[stage] = D3DTOP_DISABLE);
         CHECK_D3D_RESULT("SetTextureStageState", hr);
         SetTexture1(stage, NULL);
                              //reset also coord index
         SetTextureCoordIndex(stage, stage);
                              //disable all other enabled stages, some drivers have problems
                              // with this
         while(++stage < MAX_TEXTURE_STAGES){
            if(stage_txt_op[stage] == D3DTOP_DISABLE)
               break;
            hr = d3d_dev->SetTextureStageState(stage, D3DTSS_COLOROP, stage_txt_op[stage] = D3DTOP_DISABLE);
            CHECK_D3D_RESULT("SetTextureStageState", hr);
            SetTexture1(stage, NULL);
            SetTextureCoordIndex(stage, stage);
         }
      }
   }
#endif
//----------------------------

   inline void DisableTextures(dword stage){

      assert(CanUsePixelShader());
      //assert(stage>0);
      if(last_texture[stage]){
         last_texture[stage] = NULL;
         d3d_dev->SetTexture(stage, NULL);
                              //disable all other enabled stages, some drivers have problems with this
         while(++stage < MAX_TEXTURE_STAGES){
            if(!last_texture[stage])
               break;
            last_texture[stage] = NULL;
            d3d_dev->SetTexture(stage, NULL);
         }
      }
   }

#ifndef GL
//----------------------------
// Clipping planes - using either D3D clipping plane, or emulated clipping by alpha texture.
   void SetClippingPlane(const S_plane &pl, const S_matrix &_m_view_proj_hom);
   void DisableClippingPlane();
#endif

   inline dword GetTextureSortID(){
      return texture_sort_id++;
   }

//----------------------------
/*   
   inline void SetClipping(bool b){

      if(d3d_clipping != b){
         HRESULT hr;
         hr = d3d_dev->SetRenderState(D3DRS_CLIPPING, d3d_clipping = b);
         CHECK_D3D_RESULT("SetRenderState", hr);
      }
   }
*/
//----------------------------
   inline dword GetRenderTime() const{ return render_time; }
   
                              //debugging
#ifdef _DEBUG
   int debug_int[8];
   float debug_float[8];
#else
   int _debug_int[8];
   float _debug_float[8];
#endif

   void PRINT(const char *cp, bool still=false) const{ if(cb_proc) cb_proc(I3DCB_MESSAGE, (dword)cp, still, NULL); }
   void PRINT(float f, bool still=false) const{ PRINT(C_fstr("%.2f", f), still); }
   void PRINT(int i, bool still=false) const{ PRINT(C_fstr("%i", i), still); }
   void PRINT(const S_vector &v, bool still=false) const{ PRINT(C_fstr("[%.2f, %.2f, %.2f]", v.x, v.y, v.z), still); }

   void DEBUG(const char *cp, int type = 0) const{ if(cb_proc) cb_proc(I3DCB_DEBUG_MESSAGE, (dword)cp, type, NULL); }
   void DEBUG(float f, bool persist = false) const{ DEBUG(C_fstr("%.2f", f), persist); }
   void DEBUG(int i, bool persist = false) const{ DEBUG(C_fstr("%i", i), persist); }
   void DEBUG(const S_vector &v, bool persist = false) const{ DEBUG(C_fstr("[%.2f, %.2f, %.2f]", v.x, v.y, v.z), persist); }

   void DEBUG_CODE(dword code, dword data) const{ if(cb_proc) cb_proc(I3DCB_DEBUG_CODE, code, data, NULL); }

   inline void DebugLine(const S_vector &from, const S_vector &to, int type = 1, dword color = 0xffffffff) const{
      struct S_dl{
         S_vector from, to;
         int type;
         dword color;
      } l;
      l.from = from;
      l.to = to;
      l.type = type;
      l.color = color;
      DEBUG_CODE(0, (dword)&l);
   }
   inline void DebugVector(const S_vector &from, const S_vector &dir, int type = 1, dword color = 0xffffffff) const{
      DebugLine(from, from+dir, type, color);
   }
   inline void DebugPoint(const S_vector &pos, float radius = .1f, int type = 1, dword color = 0xffffffff) const{
      struct S_dp{
         S_vector pos;
         float radius;
         int type;
         dword color;
      } p;
      p.pos = pos;
      p.radius = radius;
      p.type = type;
      p.color = color;
      DEBUG_CODE(1, (dword)&p);
   }

//----------------------------
// Visualize plane as a point and line. The point chosen is closest point to the reference point passed to the function.
   inline void DebugPlane(const S_plane &pl, const S_vector &ref_pt = S_vector(0, 0, 0), dword color = 0xffffffff) const{
      S_vector p = ref_pt + pl.normal * -(pl.d + ref_pt.Dot(pl.normal));
      DebugPoint(p, .1f, 1, color);
      DebugLine(p, p+pl.normal, 1, color);
   }
private:
//----------------------------
                              //light-mapping

                              //keep all LMap textures here for further allocation,
                              // no AddRef kept on these
   C_vector<C_LM_surface*> lm_textures;
   int max_lm_memory;         //max bytes for LM textures
public:
   int lm_create_aa_ratio;    //LM creation - antialias sub-pixel divider
   C_LM_alloc_block *LMAlloc(C_vector<I3D_rectangle>*);
   void LMFree(C_LM_alloc_block*);
   bool UnregLMTexture(PI3D_texture);
   void DrawLMaps(const I3D_rectangle &viewport);

                              //texture formats
   S_pixelformat pf_light_maps;
   class C_rgb_conversion *rgbconv_lm;

   void PurgeLMTextures();

//----------------------------
                              //help for LM construction:
   C_str last_LM_blend_img_name;
   C_smart_ptr<IImage> last_LM_blend_img;
   __int64 last_blend_map_time;

//----------------------------

   dword GraphCallback(dword msg, dword par1, dword par2);

   static GRAPHCALLBACK GraphCallback_thunk;
   
   void RegisterManagedTexture(PI3D_texture);
   void UnregisterManagedTexture(PI3D_texture);
   const C_vector<PI3D_texture> &GetManagedTextures() const{ return managed_textures; }

//----------------------------
// Find bitmap in registered Maps directories.
// If found, obtain information about it.
   bool FindBitmap(const char *fname, C_str &out_name, dword &sx1, dword &sy1, byte &bpp1, bool is_cubemap = false) const;

                              //creation

   I3D_RESULT CreateTexture1(const I3D_CREATETEXTURE&, PI3D_texture*, PI3D_LOAD_CB_PROC, void *cb_context, C_str *err_msg);

//----------------------------
   inline IDirect3DDevice9 *GetDevice1() const{ return d3d_dev; }

#ifndef GL
                              //resource buffers
   bool AllocVertexBuffer(dword fvf_flags, D3DPOOL mem_pool, dword d3d_usage, dword num_verts,
      IDirect3DVertexBuffer9 **vb_ret, dword *beg_indx_ret, class I3D_dest_vertex_buffer *vb_d, bool allow_cache = true);
   bool FreeVertexBuffer(IDirect3DVertexBuffer9 *vb, dword vertex_indx, I3D_dest_vertex_buffer*);
   bool AllocIndexBuffer(D3DPOOL mem_pool, dword d3d_usage, dword num_faces, IDirect3DIndexBuffer9 **ib_ret, dword *beg_face_ret);
   bool FreeIndexBuffer(IDirect3DIndexBuffer9 *vb, dword face_indx);
#endif
//#endif
//----------------------------

   inline void EnableZWrite(bool b){
      if(zw_enable!=b){
         zw_enable = b;
         d3d_dev->SetRenderState(D3DRS_ZWRITEENABLE, b);
#ifdef GL
         glDepthMask(b);
#endif
      }
   }

//----------------------------

   inline void EnableZBUsage(bool on){
      EnableZWrite(on);
      d3d_dev->SetRenderState(D3DRS_ZFUNC, !on ? D3DCMP_ALWAYS : D3DCMP_LESSEQUAL);
#ifdef GL
      on ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
#endif
   }

//----------------------------
                              //debug interface counting
   void AddCount(I3D_CLID id) const{ ++interface_count[id]; }
   void DecCount(I3D_CLID id) const{ --interface_count[id]; }

   void SetupBlendInternal(dword flags);
   inline void SetupBlend(dword flags){
      if(last_blend_flags!=flags)
         SetupBlendInternal(flags);
   }
//----------------------------

   bool GetVisualPropertiesInfo(dword visual_type, const S_visual_property **props, dword *num_props) const;

//----------------------------
                              //procedurals:
private:

   mutable C_vector<class I3D_procedural_base*> procedural_cache;
   void UnregisterProcedural(I3D_procedural_base*) const;
   friend I3D_procedural_base;
public:
//----------------------------
// Create specified procedural, or return one from cache.
// Parameters:
//    'file_name' ... name of procedural's file definition (will also become the name of procedural)
//    'lc' ... loader class for reporting errors
//    'creator_name' ... name of frame, material or other creating entity (used for error reporting)
//    'init_data' ... special init data, which purpose is dependent on procedural type
// If creation fails, the returned value is NULL, and error is written to log.
   I3D_RESULT GetProcedural(const char *filename, PI3D_LOAD_CB_PROC, void *cb_context, I3D_procedural_base **pb_ret, dword init_data = 0) const;

//----------------------------
                              //editing support
private:
   C_buffer<C_smart_ptr<I3D_material> > icon_materials;
public:
   PI3D_material GetIcon(dword res_id);

//----------------------------
// Get valid format of Zbuffer/Stencil surface for current device and specified render target format.
// Returns D3DFMT_UNKNOWN is no such format exists.
   D3DFORMAT FindZStenFormat(const S_pixelformat&) const;


   typedef map<int, S_sound_env_properties> t_env_properties;
private:
   t_env_properties env_properties;

public:
   inline const t_env_properties &GetEnvProperties() const{ return env_properties; }
   const S_sound_env_properties *GetEnvProperty(int id){
      t_env_properties::const_iterator it = env_properties.find(id);
      return (it!=env_properties.end()) ? &(*it).second : NULL;
   }
   inline void MakeEnvSettingDirty(){ drv_flags |= DRVF_SOUND_ENV_RESET; }

                              //exported functions
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(GetStats)(I3DSTATSINDEX, void *data);

                              //--- directories ---
   I3DMETHOD(AddDir)(I3D_DIRECTORY_TYPE dt, const C_str &d){
      if(dt >= I3DDIR_LAST) return I3DERR_INVALIDPARAMS;
      dirs[dt].push_back(d);
      return I3D_OK;
   }

   I3DMETHOD(RemoveDir)(I3D_DIRECTORY_TYPE dt, const C_str &d){
      if(dt >= I3DDIR_LAST)
         return I3DERR_INVALIDPARAMS;
      C_vector<C_str>::iterator it;// = find(dirs[dt].begin(), dirs[dt].end(), d);
      for(it=dirs[dt].begin(); it!=dirs[dt].end(); it++){
         if(*it==d)
            break;
      }
      if(it==dirs[dt].end())
         return I3DERR_OBJECTNOTFOUND;
      dirs[dt].erase(it);
      return I3D_OK;
   }

   I3DMETHOD(ClearDirs)(I3D_DIRECTORY_TYPE dt){
      if(dt >= I3DDIR_LAST) return I3DERR_INVALIDPARAMS;
      dirs[dt].clear();
      return I3D_OK;
   }

   I3DMETHOD_(int,NumDirs)(I3D_DIRECTORY_TYPE dt) const{
      if(dt >= I3DDIR_LAST) return -1;
      return dirs[dt].size();
   }

   I3DMETHOD_(const C_str&,GetDir)(I3D_DIRECTORY_TYPE dt, int indx) const{
      if(dt >= I3DDIR_LAST)
         //return NULL;
         throw C_except("invalid index");
      if(indx<0 || indx>=(int)dirs[dt].size())
         //return NULL;
         throw C_except("invalid index");
      return dirs[dt][indx];
   }

                              //--- interfaces ---
   I3DMETHOD_(PISND_driver,GetSoundInterface)() const{ return init_data.lp_sound; }
   I3DMETHOD_(const PISND_driver,GetSoundInterface)(){ return init_data.lp_sound; }
   I3DMETHOD_(PIGraph,GetGraphInterface)(){ return init_data.lp_graph; }
   I3DMETHOD_(CPIGraph,GetGraphInterface)() const{ return init_data.lp_graph; }

                              //--- Render state ---
   I3DMETHOD(SetState)(I3D_RENDERSTATE, dword value);
   I3DMETHOD_(dword,GetState)(I3D_RENDERSTATE) const;
   I3DMETHOD(FindTextureFormat)(S_pixelformat&, dword flags) const;
   I3DMETHOD(SetTexture)(CPI3D_texture);
                              //rendering
   I3DMETHOD(SetViewport)(const I3D_rectangle &rc_ltrb);
   I3DMETHOD(BeginScene)();
   I3DMETHOD(EndScene)();
   I3DMETHOD(Clear)(dword color = 0);
                              //--- Creation
   I3DMETHOD_(PI3D_scene,CreateScene)();
   I3DMETHOD(CreateTexture)(const I3D_CREATETEXTURE *ct, PI3D_texture *tp1, C_str *err_msg = NULL){ return CreateTexture1(*ct, tp1, NULL, NULL, err_msg); }
   I3DMETHOD_(PI3D_material,CreateMaterial)();
   I3DMETHOD_(PI3D_mesh_base,CreateMesh)(dword vc_flags);
   I3DMETHOD_(PI3D_animation_base,CreateAnimation)(I3D_ANIMATION_TYPE);
   I3DMETHOD_(PI3D_animation_set,CreateAnimationSet)();
   I3DMETHOD_(PI3D_interpolator,CreateInterpolator)();
                              //visual types
   I3DMETHOD(EnumVisualTypes)(I3D_ENUMVISUALPROC*, dword user) const;

   I3DMETHOD_(void,SetCallback)(PI3D_CALLBACK cb, void *cbc){
      cb_proc = cb;
      cb_context = cbc;
   }
   I3DMETHOD(SetDataBase)(const char *db_filename, dword size, int max_keep_days);
   I3DMETHOD(FlushDataBase)();

   I3DMETHOD_(dword,NumProperties)(dword visual_type) const;
   I3DMETHOD_(const char*,GetPropertyName)(dword visual_type, dword index) const;
   I3DMETHOD_(I3D_PROPERTYTYPE,GetPropertyType)(dword visual_type, dword index) const;
   I3DMETHOD_(const char*,GetPropertyHelp)(dword visual_type, dword index) const;

                              //sound properties
   I3DMETHOD(SetSoundEnvData)(int set_id, I3D_SOUND_ENV_DATA_INDEX, dword data);
   I3DMETHOD_(dword, GetSoundEnvData)(int set_id, I3D_SOUND_ENV_DATA_INDEX) const;

   //I3DMETHOD(ReloadTexture)(PI3D_texture) const;
   I3DMETHOD_(void,EvictResources)();

   I3DMETHOD_(void,SetCollisionMaterial)(dword index, const I3D_collision_mat&);
   I3DMETHOD_(void,ClearCollisionMaterials)();

#ifndef GL
   I3DMETHOD_(I3D_RESULT,SetNightVision)(bool on);
   I3DMETHOD_(I3D_RESULT,BeginNightVisionRender)();
   I3DMETHOD_(I3D_RESULT,EndNightVisionRender)();
#endif
};

//----------------------------

#endif
