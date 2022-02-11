/*--------------------------------------------------------
   Copyright (c) Lonely Cat Games  All rights reserved.

   File: I3D_eng.h
   Content: Insanity 3D II interfaces include file
--------------------------------------------------------*/

#ifndef __ENG_3DI_
#define __ENG_3DI_
#pragma once

#include <I3D\I3D_math.h>
#include <memory.h>
#include <smartptr.h>
#include <C_except.h>
#include <C_str.hpp>

# define I3DAPI
# define I3DMETHOD_(type,method) virtual type I3DAPI method
# define I3DMETHOD(method) virtual I3D_RESULT I3DAPI method

//----------------------------

typedef class I3D_scene *PI3D_scene;
typedef const I3D_scene *CPI3D_scene;

typedef class I3D_driver *PI3D_driver;
typedef const I3D_driver *CPI3D_driver;

class C_cache;

//----------------------------
                              //result codes
typedef long I3D_RESULT;
enum{
   I3D_OK,
   I3D_DONE,

   I3DERR_NOFILE = 0x80000001,//unable to open file
   I3DERR_OUTOFMEM = 0x80000002, //out of memory
   I3DERR_TEXTURESNOTSUPPORTED = 0x80000003,
   I3DERR_GENERIC = 0x80000006,  //internal error
   I3DERR_OBJECTNOTFOUND = 0x80000007, //find/delete - no such object in list
   I3DERR_INVALIDPARAMS = 0x80000008,
   I3DERR_CANTSETSECTOR = 0x8000000b,
   I3DERR_NOTINITIALIZED,
   I3DERR_FILECORRUPTED,
   I3DERR_NOFILE1,            //additional file (opacity map)
   I3DERR_CYCLICLINK,         //the linking would cause cyclic hierarchy
   I3DERR_CANCELED,           //operation has been cancelled by user (enumeration, loading, lightmap computation, etc)
   I3DERR_UNSUPPORTED,
};

#ifndef I3D_SUCCESS
# define I3D_SUCCESS(n) ((I3D_RESULT)n>=0)
#endif
#ifndef I3D_FAIL
# define I3D_FAIL(n) ((I3D_RESULT)n<0)
#endif

//----------------------------
                              //enumeration

                              //enum frames flags
#define ENUMF_VISUAL    1
#define ENUMF_LIGHT     2
#define ENUMF_CAMERA    4
#define ENUMF_SOUND     8
#define ENUMF_SECTOR    0x10
#define ENUMF_DUMMY     0x20

#define ENUMF_MODEL     0x80
#define ENUMF_USER      0x100
#define ENUMF_VOLUME    0x200
#define ENUMF_JOINT     0x400
#define ENUMF_OCCLUDER  0x800
#define ENUMF_ALL       0x0ffff
#define ENUMF_WILDMASK  0x10000
#define ENUMF_CASESENS  0x20000
#define ENUMF_NO_BACKDROP  0x40000  

                              //enum frames return value
enum I3DENUMRET{
   I3DENUMRET_OK,
   I3DENUMRET_SKIPCHILDREN,
   I3DENUMRET_CANCEL,
};

//----------------------------
                              //bounding box defined by 2 boundary points
struct I3D_bbox{
   S_vector min, max;
   I3D_bbox(){}
   I3D_bbox(const S_vector &n, const S_vector &x){
      min = n;
      max = x;
   }
   inline const S_vector &operator[](int i) const{ return (&min)[i]; }

//----------------------------
// Make bounding-box 'invalid' - expand min to positive infinity and max to negative infinity.
// In this state the bounding-box is prepared for extens expansion.
   inline void Invalidate(){
      min = S_vector(1e+16f, 1e+16f, 1e+16f);
      max = S_vector(-1e+16f, -1e+16f, -1e+16f);
   }

//----------------------------
// Check if bounding-box is valid.
   inline bool IsValid() const{ return(min.x <= max.x); }

//----------------------------
// Expand AA bounding-box, defined by 2 extreme points, into a bounding-box defined by
// 8 corner points.   
   void Expand(S_vector bbox_full[8]) const{
                              //expand bound-box (2 corner points) to full bbox (8 points)
      bbox_full[0].x = min.x; bbox_full[0].y = min.y; bbox_full[0].z = min.z;
      bbox_full[1].x = max.x; bbox_full[1].y = min.y; bbox_full[1].z = min.z;
      bbox_full[2].x = min.x; bbox_full[2].y = max.y; bbox_full[2].z = min.z;
      bbox_full[3].x = max.x; bbox_full[3].y = max.y; bbox_full[3].z = min.z;
      bbox_full[4].x = min.x; bbox_full[4].y = min.y; bbox_full[4].z = max.z;
      bbox_full[5].x = max.x; bbox_full[5].y = min.y; bbox_full[5].z = max.z;
      bbox_full[6].x = min.x; bbox_full[6].y = max.y; bbox_full[6].z = max.z;
      bbox_full[7].x = max.x; bbox_full[7].y = max.y; bbox_full[7].z = max.z;
   }
};

typedef I3D_bbox *PI3D_bbox;
typedef const I3D_bbox *CPI3D_bbox;

//----------------------------
                              //boundnig sphere
struct I3D_bsphere{
   S_vector pos;
   float radius;
   I3D_bsphere(){}
   I3D_bsphere(const S_vector &p, float r):
      pos(p),
      radius(r)
   {}
};

typedef I3D_bsphere *PI3D_bsphere;
typedef const I3D_bsphere *CPI3D_bsphere;

//----------------------------
                              //bounding volume - consisting of minimal box and minimal sphere
struct I3D_bound_volume{
   I3D_bbox bbox;
   I3D_bsphere bsphere;
};

typedef I3D_bound_volume *PI3D_bound_volume;
typedef const I3D_bound_volume *CPI3D_bound_volume;

//----------------------------
                              //rectangle
struct I3D_rectangle{
#pragma warning(push)
#pragma warning(disable:4201)
   union{
      struct{
         int l, t, r, b;
      };
      struct{
         int m[4];
      };
   };
#pragma warning(pop)
   I3D_rectangle(){}
   I3D_rectangle(int l1, int t1, int r1, int b1):
      l(l1), t(t1), r(r1), b(b1)
   {}
   inline int &operator[](int i){ return m[i]; }
   inline const int &operator[](int i) const{ return m[i]; }
};

//----------------------------
                              //frame types
enum I3D_FRAME_TYPE{
   FRAME_NULL, FRAME_VISUAL, FRAME_LIGHT, FRAME_CAMERA,
   FRAME_SOUND, FRAME_SECTOR, FRAME_DUMMY, FRAME_reserved,
   FRAME_USER, FRAME_MODEL, FRAME_JOINT, FRAME_VOLUME,
   FRAME_OCCLUDER,
   FRAME_LAST,
};

                              //visual types - four-cc codes
#define I3D_VISUAL_OBJECT     0x5f4a424f  //OBJ_
#define I3D_VISUAL_PARTICLE   0x43545250  //PRTC
#define I3D_VISUAL_NURBS      0x4252554e  //NURB
#define I3D_VISUAL_LIT_OBJECT 0x50414d4c  //LMAP
#define I3D_VISUAL_SINGLEMESH 0x4d474e53  //SNGM
#define I3D_VISUAL_BILLBOARD  0x44524242  //BBRD
#define I3D_VISUAL_FIRE       'ERIF'      //FIRE
#define I3D_VISUAL_MORPH      0x4850524d  //MRPH
#define I3D_VISUAL_DYNAMIC    0x434d4e44  //DNMC
#define I3D_VISUAL_UV_SHIFT   0x5356554f  //OUVS
#define I3D_VISUAL_FLARE      0x45524c46  //FLRE
#define I3D_VISUAL_CAMVIEW    0x564d4143  //CAMV
#define I3D_VISUAL_ATMOS 'OMTA'           //ATMO
#define I3D_VISUAL_DISCHARGE  'HCSD'      //DSCH


struct I3D_note_callback{     //struct used to pass params when note callback encountered
   int curr_time;             //current time
   int note_time;             //time on which the note key is set
   int stage;                 //stage on which the animation is processed
   const char *note;          //note string
};

enum I3D_CALLBACK_MESSAGE{
   I3DCB_MESSAGE       = 2,   //const char*, bool still
   I3DCB_DEBUG_MESSAGE = 3,   //(const char*, int type) - debug message, types: 0=one-time, 1=console, 2=run-time
   I3DCB_DEBUG_CODE    = 4,   //dword code, void *data
   I3DCB_ANIM_FINISH   = 5,   //(0, dword stage)
   I3DCB_ANIM_NOTE     = 6,   //(PI3D_frame, const I3D_note_callback*)
};

                              //driver callback message is the same
#define DRV_CALLBACK_MESSAGE SCN_CALLBACK_MESSAGE

                              //scene callback function
typedef void I3DAPI I3D_CALLBACK(I3D_CALLBACK_MESSAGE msg, dword prm1, dword prm2, void *context);
typedef I3D_CALLBACK *PI3D_CALLBACK;

                              //loader callback messages
enum I3D_LOADMESSAGE{
   CBM_PROGRESS      = 0,     //(float progress, dword status); status: 0=beg, 1=in, 2=end
   CBM_ERROR         = 1,     //(const char *msg, int importance (0=max, 1=warning, ...))
   CBM_LOG           = 2,     //(const char *msg)
};

//----------------------------
// Loader - callback function.
// If the returned value for CBM_PROGRESS message is true, the loading is cancelled,
// and the loader method returns I3DERR_CANCELED.
typedef bool I3DAPI I3D_LOAD_CB_PROC(I3D_LOADMESSAGE msg, dword prm1, dword prm2, void *context);
typedef I3D_LOAD_CB_PROC *PI3D_LOAD_CB_PROC;

                              //loading flags - for all Open methods
#define I3DLOAD_LOG              1
#define I3DLOAD_PROGRESS         2  
#define I3DLOAD_ERRORS           4
#define I3DLOAD_SOUND_FREQUENCY  8  //frequency of sound will adjustable
#define I3DLOAD_BUILD_SECTORS    0x10  //model only - build sectors after loading
#define I3DLOAD_SOUND_SOFTWARE   0x20  //play sound in software buffer

                              //link flags
#define I3DLINK_UPDMATRIX  1

                              //maximal frame scale, setting greater than this will fail
#define I3D_MAX_FRAME_SCALE 1e+4f
//----------------------------
                              //statistics:

typedef struct{               //collisions
   dword sphere_sphere, sphere_rectangle, sphere_box, sphere_face;
   dword line_sphere, line_rectangle, line_box, line_face;
} I3D_stats_volume, *PI3D_stats_volume;

                              //bsp
typedef struct{
   dword num_nodes;           //number of tree nodes
   dword depth;               //depth of tree
   dword num_planes;          //number of partion planes( <= num_nodes)
   dword num_faces;           //number of faces in tree
   dword face_references;     //number of references to faces ( >= num_faces)
   dword num_vertices;
   dword mem_used;            //memory used by tree in bytes.
} I3D_stats_bsp, *PI3D_stats_bsp;

typedef struct{
   dword total_num;
   dword root_num;
   dword moving_static;
   dword reserved2;
} I3D_stats_voltree, *PI3D_stats_voltree;

typedef struct{               //rendering
   dword triangle;            //triangles drawn
   dword line;                //lines drawn
   dword vert_trans;          //vertices transformed
} I3D_stats_render, *PI3D_stats_render;

typedef struct{               //frames
   dword frame_count[FRAME_LAST];   //frames seen/heard
   dword num_vis_computed;    //visuals checked for on-screen
   dword dyn_shd_casters;     //dynamic shadow casters rendered
} I3D_stats_scene, *PI3D_stats_scene;

typedef struct{               //textures
   dword txt_total;           //number of textures
   dword txt_vidmem;          //number of textures loaded into vidmem
   dword txt_sys_mem;         //system memory used by textures
   dword txt_video_mem;       //video memory used by textures
   dword vidmem_total;        //total video memory
   dword vidmem_free;         //free video memory
   dword txtmem_total;        //total texture video memory
   dword txtmem_free;         //free texture video memory
} I3D_stats_video, *PI3D_stats_video;

typedef struct{               //memory
   dword available;           //memory available (by system)
   dword used;                //memory used by I3D
   dword used_other;          //memory used by other resources
   dword num_blocks;          //allocated memory blocks
   dword dbase_size;          //total bytes available in database
   dword dbase_used;          //bytes used in database
} I3D_stats_memory, *PI3D_stats_memory;

typedef struct{               //sounds
   dword buf_hw_all, buf_hw_free;   //hardware sound buffers
   dword buf_3d_all, buf_3d_free;   //hardware 3D sound buffers
   dword hw_mem_all, hw_mem_free;   //sound card memory
   dword snds_all, snds_audible;    //sound frames total, audible
   dword snds_in_card;        //number of sounds uploaded to sound card
   dword mem_used;            //memory used for all sounds
} I3D_stats_sound, *PI3D_stats_sound;


enum I3DSTATSINDEX{           //input index for I3D_driver::GetStats or I3D_scene::GetStats
   I3DSTATS_VOLUME,           //I3D_stats_volume   (scene)
   I3DSTATS_RENDER,           //I3D_stats_render   (scene)
   I3DSTATS_SCENE,            //I3D_stats_scene    (scene)
   I3DSTATS_VIDEO,            //I3D_stats_video    (driver)
   I3DSTATS_MEMORY,           //I3D_stats_memory   (driver)
   I3DSTATS_SOUND,            //I3D_stats_sound    (scene)
   I3DSTATS_BSP,              //I3D_stats_bsp      (scene)
   I3DSTATS_VOLTREE,          //I3D_stats_voltree  (scene)
};

//----------------------------

enum{
   I3D_FRMF_NOSHADOW = 1,     //don't cast shadows on lightmaps (visuals)
   I3D_FRMF_NOCOLLISION = 2,  //don't detect collisions on this frame (visuals and volumes)
   I3D_FRMF_NO_FLARE_COLLISION = 4, //let flares no detect collisions with this frame (volumes)
   I3D_FRMF_SHADOW_CAST = 8,  //this frame (model) casts shadows
   I3D_FRMF_SHADOW_RECEIVE = 0x10,  //this frame (visual) receives shadows
   I3D_FRMF_STATIC_COLLISION = 0x20,//this frame (visual || volume) is considered static for collisions
   I3D_FRMF_FORCE_SHADOW = 0x100,//force shadow rendering also if frame is off (visuals)
};

//----------------------------

typedef class I3D_frame *PI3D_frame;
typedef const I3D_frame *CPI3D_frame;

typedef I3DENUMRET I3DAPI I3D_ENUMPROC(PI3D_frame, dword context);

//----------------------------

#if !defined I3D_FULL
class I3D_frame{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD_(I3D_FRAME_TYPE,GetType)() const = 0;
                              //setup
   I3DMETHOD(SetPos)(const S_vector&) = 0;
   I3DMETHOD(SetScale)(float) = 0;             
   I3DMETHOD(SetRot)(const S_quat&) = 0;
   I3DMETHOD(SetDir)(const S_vector&, float roll = 0.0f) = 0;
   I3DMETHOD(SetDir1)(const S_vector &dir, const S_vector &up) = 0;

   I3DMETHOD_(const S_vector&,GetPos)() const = 0;
   I3DMETHOD_(const S_vector&,GetWorldPos)() const = 0;
   I3DMETHOD_(float,GetScale)() const = 0;        
   I3DMETHOD_(float,GetWorldScale)() const = 0;   
   I3DMETHOD_(const S_quat&,GetRot)() const = 0;
   I3DMETHOD_(S_quat,GetWorldRot)() const = 0;
   I3DMETHOD_(const S_vector&,GetDir)() const = 0;
   I3DMETHOD_(const S_vector&,GetWorldDir)() const = 0;

   I3DMETHOD_(const S_matrix&,GetLocalMatrix)() const = 0;
   I3DMETHOD_(const S_matrix&,GetMatrix)() const = 0;
   I3DMETHOD_(const S_matrix&,GetInvMatrix)() const = 0;

   //I3DMETHOD_(void,Update)() = 0;

                              //user data
   I3DMETHOD_(void,SetData)(dword) = 0;
   I3DMETHOD_(dword,GetData)() const = 0;

                              //visibility control
   I3DMETHOD_(void,SetOn)(bool) = 0;
   I3DMETHOD_(bool,IsOn)() const = 0;

   I3DMETHOD_(void,SetName)(const C_str&) = 0;
   I3DMETHOD_(const C_str&,GetName)() const = 0;
   I3DMETHOD_(const char*,GetOrigName)() const = 0;
   I3DMETHOD_(void,ComputeHRBoundVolume)(PI3D_bound_volume) const = 0;
                              //hierarchy
   I3DMETHOD_(PI3D_frame,GetParent)() const = 0;
   I3DMETHOD(LinkTo)(PI3D_frame, dword flags = 0) = 0;
   I3DMETHOD_(dword,NumChildren)() const = 0;
   I3DMETHOD_(const PI3D_frame*,GetChildren)() const = 0;

   I3DMETHOD(EnumFrames)(I3D_ENUMPROC*, dword user, dword flags = ENUMF_ALL, const char *mask = NULL) const = 0;
   I3DMETHOD_(PI3D_frame,FindChildFrame)(const char *name, dword data=ENUMF_ALL) const = 0;

   I3DMETHOD_(void,GetChecksum)(float &matrix_sum, float &vertc_sum, dword &num_v) const = 0;
   I3DMETHOD(Duplicate)(CPI3D_frame) = 0;
   I3DMETHOD(DebugDraw)(PI3D_scene) const;
                              //frame flags
   I3DMETHOD_(dword,SetFlags)(dword new_flags, dword flags_mask = 0xffffffff) = 0;
   I3DMETHOD_(dword,GetFlags)() const = 0;
private:
   I3DMETHOD_(void,FrmReserved00)() = 0;
   I3DMETHOD_(void,FrmReserved01)() = 0;
   I3DMETHOD_(void,FrmReserved02)() = 0;
   I3DMETHOD_(void,FrmReserved03)() = 0;
   I3DMETHOD_(void,FrmReserved04)() = 0;
   I3DMETHOD_(void,FrmReserved05)() = 0;
   I3DMETHOD_(void,FrmReserved06)() = 0;
   I3DMETHOD_(void,FrmReserved07)() = 0;
};

#endif

//----------------------------
typedef class I3D_sector *PI3D_sector;
typedef const I3D_sector *CPI3D_sector;
                              //light
enum I3D_LIGHTTYPE{
   I3DLIGHT_NULL, I3DLIGHT_POINT, I3DLIGHT_SPOT, I3DLIGHT_DIRECTIONAL,
   I3DLIGHT_AMBIENT, I3DLIGHT_FOG, I3DLIGHT_POINTAMBIENT, I3DLIGHT_POINTFOG,
   I3DLIGHT_LAYEREDFOG,
};

                              //flags specifying if this light...
#define I3DLIGHTMODE_VERTEX   1     //...illuminates vertices
#define I3DLIGHTMODE_LIGHTMAP 2     //...affects lightmap
#define I3DLIGHTMODE_SHADOW   4     //...casts shadow
#define I3DLIGHTMODE_DYNAMIC_LM  8  //...cast dynamic circles on lightmaps

#if !defined I3D_FULL 
class I3D_light: public I3D_frame{
public:
   I3DMETHOD(SetLightType)(I3D_LIGHTTYPE) = 0;
   I3DMETHOD_(void,SetColor)(const S_vector&) = 0;
   I3DMETHOD_(void,SetPower)(float) = 0;
   I3DMETHOD(SetMode)(dword) = 0;
   I3DMETHOD_(void,SetRange)(float near1, float far1) = 0;
   I3DMETHOD(SetCone)(float inner, float outer) = 0;
   I3DMETHOD(SetSpecularColor)(const S_vector&) = 0;
   I3DMETHOD(SetSpecularPower)(float) = 0;
                              //sectors
   I3DMETHOD_(dword,NumLightSectors)() const = 0;
   I3DMETHOD_(PI3D_sector const*,GetLightSectors)() const = 0;

   I3DMETHOD_(I3D_LIGHTTYPE,GetLightType)() const = 0;
   I3DMETHOD_(const S_vector&,GetColor)() const = 0;
   I3DMETHOD_(float,GetPower)() const = 0;
   I3DMETHOD_(dword,GetMode)() const = 0;
   I3DMETHOD_(void,GetRange)(float &near1, float &far1) const = 0;
   I3DMETHOD_(void,GetCone)(float &inner, float &outer) const = 0;
   I3DMETHOD_(const S_vector&,GetSpecularColor)() const = 0;
   I3DMETHOD_(float,GetSpecularPower)() const = 0;
};
#endif

typedef class I3D_light *PI3D_light;
typedef const I3D_light *CPI3D_light;

//----------------------------
                              //sound caps
enum I3D_SOUNDTYPE{
   I3DSOUND_NULL, I3DSOUND_POINT, I3DSOUND_SPOT, I3DSOUND_AMBIENT,
   I3DSOUND_VOLUME, I3DSOUND_POINTAMBIENT,
};


#if !defined I3D_FULL 
class I3D_sound: public I3D_frame{
public:
   I3DMETHOD(Open)(const char *fname, dword flags = 0, I3D_LOAD_CB_PROC* = NULL, void *context = NULL) = 0;
   I3DMETHOD_(bool,IsPlaying)() const = 0;
                              //setup
   I3DMETHOD(SetSoundType)(I3D_SOUNDTYPE) = 0;
   I3DMETHOD_(void,SetRange)(float min, float max) = 0;
   I3DMETHOD_(void,SetCone)(float in_angle, float out_angle) = 0;
   I3DMETHOD_(void,SetOutVol)(float) = 0;
   I3DMETHOD_(void,SetVolume)(float) = 0;
   I3DMETHOD_(void,SetLoop)(bool) = 0;
   I3DMETHOD(SetStreaming)(bool) = 0;
   I3DMETHOD(SetCurrTime)(dword) = 0;
   I3DMETHOD(SetFrequency)(float) = 0;

   I3DMETHOD_(I3D_SOUNDTYPE,GetSoundType)() const = 0;
   I3DMETHOD_(void,GetRange)(float &min, float &max) const = 0;
   I3DMETHOD_(void,GetCone)(float &in_angle, float &out_angle) const = 0;
   I3DMETHOD_(float,GetOutVol)() const = 0;
   I3DMETHOD_(float,GetVolume)() const = 0;
   I3DMETHOD_(bool,IsLoop)() const = 0;
   I3DMETHOD_(bool,IsStreaming)() const = 0;
   I3DMETHOD_(dword,GetCurrTime)() const = 0;
   I3DMETHOD_(dword,GetPlayTime)() const = 0;
   I3DMETHOD_(float,GetFrequency)() const = 0;

   I3DMETHOD_(const C_str&,GetFileName)() const = 0;
   I3DMETHOD_(void,StoreFileName)(const C_str &fn) = 0;
   I3DMETHOD_(dword,GetOpenFlags)() const = 0;
   I3DMETHOD_(class ISND_source*,GetSoundSource)() const = 0;
                              //sectors
   I3DMETHOD_(dword,NumSoundSectors)() const = 0;
   I3DMETHOD_(PI3D_sector const*,GetSoundSectors)() const = 0;
};
#endif

typedef class I3D_sound *PI3D_sound;
typedef const I3D_sound *CPI3D_sound;

//----------------------------

#if !defined I3D_FULL 
class I3D_camera: public I3D_frame{
public:
                              //setup:
   I3DMETHOD(SetFOV)(float) = 0;
   I3DMETHOD(SetRange)(float near1, float far1) = 0;
   I3DMETHOD(SetOrthoScale)(float scale) = 0;
   I3DMETHOD(SetOrthogonal)(bool) = 0;

   I3DMETHOD_(float,GetFOV)() const = 0;
   I3DMETHOD_(void,GetRange)(float &near1, float &far1) const = 0;
   I3DMETHOD_(float,GetOrthoScale)() const = 0;
   I3DMETHOD_(bool,GetOrthogonal)() const = 0;

   I3DMETHOD_(const S_matrix&,GetViewMatrix)() const = 0;
   I3DMETHOD_(const S_matrix&,GetProjectionMatrix)() const = 0;
   I3DMETHOD_(PI3D_sector,GetSector)() const = 0;
};
#endif

typedef class I3D_camera *PI3D_camera;
typedef const I3D_camera *CPI3D_camera;

//----------------------------

typedef S_vector2 I3D_text_coor;

//----------------------------

#if !defined I3D_FULL 
class I3D_texture{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD_(const C_str&,GetFileName)(int index = 0) const = 0;
   I3DMETHOD_(dword,SizeX)() const = 0;
   I3DMETHOD_(dword,SizeY)() const = 0;
   I3DMETHOD_(class IImage*,GetSysmemImage)() = 0;
   I3DMETHOD_(const IImage*,GetSysmemImage)() const = 0;

   I3DMETHOD_(PI3D_driver,GetDriver)() = 0;
   I3DMETHOD_(CPI3D_driver,GetDriver)() const = 0;

   I3DMETHOD_(bool,Reload)() = 0;
};
#endif

typedef class I3D_texture *PI3D_texture;
typedef const I3D_texture *CPI3D_texture;

//----------------------------
                              //index of texture within material
enum I3D_MATERIAL_TEXTURE_INDEX{
   MTI_DIFFUSE,
   MTI_ENVIRONMENT,
   MTI_EMBM,
   MTI_DETAIL,
   MTI_NORMAL,
   MTI_SECONDARY,
   MTI_LAST
};

//----------------------------
                              //material
#if !defined I3D_FULL 
class I3D_material{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD_(void,SetAmbient)(const S_vector&) = 0;
   I3DMETHOD_(void,SetDiffuse)(const S_vector&) = 0;
   I3DMETHOD(SetEmissive)(const S_vector&) = 0;
   I3DMETHOD_(void,SetAlpha)(float) = 0;
   I3DMETHOD_(void,SetTexture)(I3D_MATERIAL_TEXTURE_INDEX, PI3D_texture) = 0;
   I3DMETHOD_(void,Duplicate)(const I3D_material*) = 0;

   I3DMETHOD_(void,Set2Sided)(bool) = 0;

   I3DMETHOD_(const S_vector&,GetAmbient)() const = 0;
   I3DMETHOD_(const S_vector&,GetDiffuse)() const = 0;
   I3DMETHOD_(const S_vector&,GetEmissive)() const = 0;
   I3DMETHOD_(float,GetAlpha)() const = 0;

   I3DMETHOD_(PI3D_texture,GetTexture)(I3D_MATERIAL_TEXTURE_INDEX) = 0;
   I3DMETHOD_(CPI3D_texture,GetTexture)(I3D_MATERIAL_TEXTURE_INDEX) const = 0;
   I3DMETHOD_(bool,IsCkey)() const = 0;
   I3DMETHOD_(bool,Is2Sided)() const = 0;

   I3DMETHOD_(void,SetEMBMOpacity)(float f) = 0;
   I3DMETHOD_(float,GetEMBMOpacity)() const = 0;

   I3DMETHOD_(void,SetName)(const C_str&) = 0;
   I3DMETHOD_(const C_str&,GetName)() const = 0;

   I3DMETHOD_(int,GetMirrorID)() const = 0;
};
#endif

typedef class I3D_material *PI3D_material;
typedef const I3D_material *CPI3D_material;

//----------------------------

struct I3D_edge{
   word v[2];
   I3D_edge(){}
   I3D_edge(word v0, word v1){ v[0]=v0; v[1]=v1; }
   inline const word &operator[](int i) const{ return v[i]; }
   inline word &operator[](int i){ return v[i]; }
   inline void Invalidate(){ v[0] = 0xffff; }
   inline bool IsValid() const{ return (v[0]!=0xffff); }
};

//----------------------------

#pragma pack(push,1)
struct I3D_triface{
   word vi[3];
public:
   I3D_triface(){}
   I3D_triface(word i0, word i1, word i2){ vi[0]=i0, vi[1]=i1, vi[2]=i2; }
   inline word &operator [](int i){ return vi[i]; }
   inline const word &operator [](int i) const{ return vi[i]; }
   inline void Invalidate(){ vi[0] = 0xffff; }
   inline bool IsValid() const{ return (vi[0]!=0xffff); }

//----------------------------
// Check if face contains vertex index.
   inline bool ContainsIndex(word v) const{
      return (vi[0]==v || vi[1]==v || vi[2]==v);
   }

//----------------------------
// Check if face contains edge.
   inline bool ContainsIndices(word v0, word v1) const{
      return (ContainsIndex(v0) && ContainsIndex(v1));
   }

//----------------------------
// Replace vertex index in face by other vertex index.
   inline void ReplaceIndex(word v_orig, word v_new){
      if(vi[0]==v_orig) vi[0] = v_new;
      if(vi[1]==v_orig) vi[1] = v_new;
      if(vi[2]==v_orig) vi[2] = v_new;
   }

//----------------------------
// Find edge shared among two faces. If one is found, the returned value is index
// into vertex indices array of 1st vertex of the edge in this face. The other edge vertex is the next vertex index
// in the face.
// If shared edge is not found, the returned value is -1.
   inline int FindSharedEdge(const I3D_triface &fc) const{
      if(fc.ContainsIndices(vi[0], vi[1])) return 0;
      if(fc.ContainsIndices(vi[1], vi[2])) return 1;
      if(fc.ContainsIndices(vi[2], vi[0])) return 2;
      return -1;
   }

//----------------------------
// Get index into vertex indices array of 1st occurence of specified vertex index in this face.
// If face doesn't contain such vertex index, the returned value is -1.
   inline int FindIndex(word w) const{
      if(vi[0]==w) return 0;
      if(vi[1]==w) return 1;
      if(vi[2]==w) return 2;
      return -1;
   }

//----------------------------
// Compute surface area of this face, given vertex array which this face is used with.
   float ComputeSurfaceArea(const S_vector *verts, dword vertex_pitch) const{
                              //use equation:
                              // area = triangle_base * line_perp_to_base / 2
      const S_vector *v[3] = {
         (S_vector*)(((byte*)verts) + vertex_pitch * vi[0]),
         (S_vector*)(((byte*)verts) + vertex_pitch * vi[1]),
         (S_vector*)(((byte*)verts) + vertex_pitch * vi[2])
      };
                              //base is between points 0 and 1
      S_vector base = *v[1] - *v[0];
      float base_len = base.Magnitude();

      float f = base.Dot(base);
      if(IsAbsMrgZero(f)) return 0.0f;
      float u = (*v[2] - *v[0]).Dot(base) / f;
      float t = ((*v[0] + base * u) - *v[2]).Magnitude();

      float area = base_len * t * .5f;
      return area;
   }

//----------------------------
// Remap this face using mapping index array.
   inline void Remap(const word *v_map){
      vi[0] = v_map[vi[0]];
      vi[1] = v_map[vi[1]];
      vi[2] = v_map[vi[2]];
   }

//----------------------------
// Remap source face and store to this face using mapping index array.
   inline void CopyAndRemap(const I3D_triface &src, const word *v_map){
      vi[0] = v_map[src.vi[0]];
      vi[1] = v_map[src.vi[1]];
      vi[2] = v_map[src.vi[2]];
   }
};
#pragma pack(pop)

typedef I3D_triface *PI3D_triface;
typedef const I3D_triface *CPI3D_triface;

//----------------------------

#if !defined I3D_FULL
struct I3D_face{
   dword num_points;
   word *points;
private:
   I3D_face();
   I3D_face(const I3D_face&);
   I3D_face &operator =(const I3D_face&);
};
#endif

//----------------------------

#if !defined I3D_FULL
class I3D_face_group{
   C_smart_ptr<I3D_material> mat;
public:
   dword base_index;
   dword num_faces;           //number of faces in this group

   inline PI3D_material GetMaterial(){ return mat; }
   inline CPI3D_material GetMaterial() const{ return mat; }
   inline void SetMaterial(PI3D_material m){ mat = m; }
};
#endif

typedef class I3D_face_group *PI3D_face_group;
typedef const I3D_face_group *CPI3D_face_group;

//----------------------------

enum I3D_VISUAL_BRIGHTNESS{
   I3D_VIS_BRIGHTNESS_AMBIENT,
   I3D_VIS_BRIGHTNESS_NOAMBIENT,
   I3D_VIS_BRIGHTNESS_EMISSIVE,
};

//----------------------------

#if !defined I3D_FULL
class I3D_visual: public I3D_frame{
public:
   I3DMETHOD_(dword,GetVisualType)() const = 0;

   I3DMETHOD_(const I3D_bound_volume&,GetBoundVolume)() const = 0;
   I3DMETHOD_(const I3D_bound_volume&,GetHRBoundVolume)() const = 0;
   I3DMETHOD_(dword,GetLastRenderTime)() const = 0;
   I3DMETHOD_(class I3D_mesh_base*,GetMesh)() = 0;
   I3DMETHOD_(const I3D_mesh_base*,GetMesh)() const = 0;

   I3DMETHOD_(I3D_RESULT,SetMaterial)(PI3D_material) = 0;
   I3DMETHOD_(PI3D_material,GetMaterial()) = 0;
   I3DMETHOD_(CPI3D_material,GetMaterial()) const = 0;

   I3DMETHOD(SetProperty)(dword index, dword param) = 0;
   I3DMETHOD_(dword,GetProperty)(dword index) const = 0;
   I3DMETHOD_(dword,GetCollisionMaterial)() const = 0;
   I3DMETHOD_(void,SetCollisionMaterial)(dword id) = 0;

   I3DMETHOD_(void,SetVisualAlpha)(float) = 0;
   I3DMETHOD_(float,GetVisualAlpha)() const = 0;

   I3DMETHOD_(void,SetBrightness)(I3D_VISUAL_BRIGHTNESS, float) = 0;
   I3DMETHOD_(float,GetBrightness)(I3D_VISUAL_BRIGHTNESS) const = 0;

   I3DMETHOD_(void,SetOverrideLight)(const S_vector *lcolor) = 0;
   I3DMETHOD_(bool,GetOverrideLight)(S_vector &lcolor) const = 0;

private:
   I3DMETHOD_(void,VisReserved00)() = 0;
   I3DMETHOD_(void,VisReserved01)() = 0;
   I3DMETHOD_(void,VisReserved02)() = 0;
   I3DMETHOD_(void,VisReserved03)() = 0;
   I3DMETHOD_(void,VisReserved04)() = 0;
   I3DMETHOD_(void,VisReserved05)() = 0;
   I3DMETHOD_(void,VisReserved06)() = 0;
   I3DMETHOD_(void,VisReserved07)() = 0;
   I3DMETHOD_(void,VisReserved08)() = 0;
   I3DMETHOD_(void,VisReserved09)() = 0;
   I3DMETHOD_(void,VisReserved10)() = 0;
   I3DMETHOD_(void,VisReserved11)() = 0;
   I3DMETHOD_(void,VisReserved12)() = 0;
   I3DMETHOD_(void,VisReserved13)() = 0;
   I3DMETHOD_(void,VisReserved14)() = 0;
   I3DMETHOD_(void,VisReserved15)() = 0;
public:
};
#endif

typedef class I3D_visual *PI3D_visual;
typedef const I3D_visual *CPI3D_visual;

//----------------------------

#if !defined I3D_FULL
class I3D_mesh_base{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

                              //vertices
   I3DMETHOD_(dword,NumVertices)() const = 0;
   I3DMETHOD_(const S_vector*,LockVertices)() = 0;
   I3DMETHOD_(void,UnlockVertices)() = 0;
   I3DMETHOD_(dword,GetSizeOfVertex)() const = 0;
                              //faces and groups
   I3DMETHOD_(dword,NumFGroups)() const = 0;
   I3DMETHOD_(dword,NumFaces)() const = 0;
   I3DMETHOD_(CPI3D_face_group,GetFGroups)() const = 0;
   //I3DMETHOD_(const I3D_triface*,LockFaces)() = 0;
   //I3DMETHOD_(void,UnlockFaces)() = 0;
   I3DMETHOD(GetFaces)(I3D_triface *buf) const = 0;
private:
   I3DMETHOD_(void,Reserved00)() = 0;
   I3DMETHOD_(void,Reserved01)() = 0;
   I3DMETHOD_(void,Reserved02)() = 0;
   I3DMETHOD_(void,Reserved03)() = 0;
   I3DMETHOD_(void,Reserved04)() = 0;
   I3DMETHOD_(void,Reserved05)() = 0;
   I3DMETHOD_(void,Reserved06)() = 0;
   I3DMETHOD_(void,Reserved07)() = 0;
};
#endif

typedef class I3D_mesh_base *PI3D_mesh_base;
typedef const I3D_mesh_base *CPI3D_mesh_base;

//----------------------------
                              //lock flags
#define I3DLOCK_XYZ        1        //position modified during lock
#define I3DLOCK_NORMAL     2        //normal modified during lock
#define I3DLOCK_UV         4        //UV coordinates modified during lock
#define I3DLOCK_DIFFUSE    8        //diffuse modified during lock
#define I3DLOCK_SPECULAR   8        //specular modified during lock
#define I3DLOCK_ALPHA      0x10     //alpha modified during lock
#define I3DLOCK_VERTEX     0xff     //all elements of vertex
#define I3DLOCK_INDEX      0x100    //face index modified during lock
#define I3DLOCK_NO_LIGHT_RESET 0x10000 //don't reset lighting info in locked vertices

//----------------------------
                              //vertex component flags
                              // the flags, if present, must appear in vertex
                              // in the order as they're defined here
#define I3DVC_XYZ          2        //S_vector - position
#define I3DVC_XYZRHW       4        //S_vectorw - homogeneous position
#define I3DVC_ALPHA        8        //float - alpha multiplier
#define I3DVC_BETA_MASK    0xf0000  //mask of bits containing beta count
#define I3DVC_BETA_SHIFT   16       //shift of beta count integer
#define I3DVC_NORMAL       0x10     //S_vector - normal (normalized)
#define I3DVC_DIFFUSE      0x40     //dword - diffuse in ARGB format
#define I3DVC_SPECULAR     0x80     //dword - specular in ARGB format
#define I3DVC_TEXCOUNT_MASK   0xff00//mask of bits containing UV set count
#define I3DVC_TEXCOUNT_SHIFT  8     //shift of UV count integer

//----------------------------

#if !defined I3D_FULL 
class I3D_object: public I3D_visual{
public:
   I3DMETHOD_(void,SetMesh)(PI3D_mesh_base) = 0;
   I3DMETHOD_(void,SetUpdate)() = 0;
};
#endif

typedef class I3D_object *PI3D_object;
typedef const I3D_object *CPI3D_object;

//----------------------------

                              //light computation flags:
#define I3D_LIGHT_GEO_COLLISIONS  1     //use geometry collisions
#define I3D_LIGHT_BLUR            4     //blur destination after make
#define I3D_LIGHT_ALLOW_CANCEL    8     //allow ESC key to cancell light-map creation

                              //lit-object mode
#if !defined I3D_FULL
class I3D_lit_object: public I3D_visual{
public:
   I3DMETHOD(Construct)(PI3D_scene, dword flags = 0, I3D_LOAD_CB_PROC* = NULL, void *context = NULL) = 0;
   I3DMETHOD(Destruct)() = 0;
   I3DMETHOD(Load)(class C_cache*) = 0;
   I3DMETHOD(Save)(class C_cache*) const = 0;
   I3DMETHOD_(bool,IsConstructed)() const = 0;
   I3DMETHOD_(bool,GetLMPixel)(word face_index, const S_vector &point_on_face, S_vector &color) const = 0;
};
#endif

typedef class I3D_lit_object *PI3D_lit_object;
typedef const I3D_lit_object *CPI3D_lit_object;

//----------------------------

#if !defined I3D_FULL
class I3D_visual_singlemesh: public I3D_visual{
public:
   /*
   I3DMETHOD_(const I3D_bbox*,GetVolumeBox)() const = 0;
   I3DMETHOD_(const S_matrix*,GetVolumeBoxMatrix)() const = 0;
   */
//----------------------------
// Get volume box information. For details see desc on I3D_joint.
   I3DMETHOD_(I3D_RESULT,GetVolume)(S_vector &pos, S_vector &dir_normal, float &radius, float &half_len) const = 0;
};
#endif

typedef class I3D_visual_singlemesh *PI3D_visual_singlemesh;
typedef const I3D_visual_singlemesh *CPI3D_visual_singlemesh;

//----------------------------
                              //dynamic visual
                              // - containing own non-inheritable mesh
                              // - used in cases when frequent updates of geometry
                              //   are necessary
                              // - accept lock flags and updates elements by these
#if !defined I3D_FULL
class I3D_visual_dynamic: public I3D_visual{
public:
   I3DMETHOD_(I3D_RESULT,Build)(const void *verts, dword num_verts, dword vc_flags,
      const I3D_triface *faces, dword num_faces,
      const I3D_face_group *fgroups, dword num_fgroups,
      dword flags = 0) = 0;
   //I3DMETHOD_(I3D_RESULT,BuildAutoLODs)(float min_dist, float max_dist, dword num_lods, float lowest_quality) = 0;
   I3DMETHOD_(I3D_RESULT,DuplicateMesh)(CPI3D_mesh_base, dword vc_flags) = 0;
   I3DMETHOD_(I3D_RESULT,ResetLightInfo)() = 0;

   I3DMETHOD_(void*,LockVertices)(dword lock_flags) = 0;
   I3DMETHOD_(I3D_RESULT,UnlockVertices)() = 0;

   //I3DMETHOD_(PI3D_triface,LockFaces)(dword lock_flags) = 0;
   //I3DMETHOD_(I3D_RESULT,UnlockFaces)() = 0;

//----------------------------
// Render - only XYZRHW (transformed) vertex formats.
   I3DMETHOD_(I3D_RESULT,Render)() const = 0;

   I3DMETHOD_(dword,NumVertices)() const = 0;
   I3DMETHOD_(dword,NumFGroups)() const = 0;
   I3DMETHOD_(dword,NumFaces)() const = 0;
   I3DMETHOD_(CPI3D_face_group,GetFGroups)() const = 0;
   //I3DMETHOD_(PI3D_face_group,GetFGroup)(int indx) = 0;
   //I3DMETHOD_(CPI3D_face_group,GetFGroup)(int indx) const = 0;
};
#endif

typedef class I3D_visual_dynamic *PI3D_visual_dynamic;
typedef const I3D_visual_dynamic *CPI3D_visual_dynamic;

//----------------------------

#if !defined I3D_FULL 
class I3D_dummy: public I3D_frame{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   //I3DMETHOD_(const I3D_bbox*,GetBBox)() const = 0;
   //I3DMETHOD_(void,SetBBox)(const I3D_bbox&) = 0;
};
#endif

typedef class I3D_dummy *PI3D_dummy;
typedef const I3D_dummy *CPI3D_dummy;

//----------------------------

#if !defined I3D_FULL
class I3D_joint: public I3D_frame{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

//----------------------------
// Get pointer to bounding-box specifying affect region of the joint.
// The method returns NULL if bounding box is not set.
   I3DMETHOD_(const I3D_bbox*,GetBBox)() const = 0;

//----------------------------
// Get pointer to bounding-box's matrix used to transform the bounding box into joint's local coordinates.
// The method returns NULL if bounding box is not set.
   I3DMETHOD_(const S_matrix*,GetBBoxMatrix)() const = 0;

   /*
//----------------------------
// Get pointer to volume-box used for collision testing on the joint.
// The method returns NULL if volume box is not set.
   I3DMETHOD_(const I3D_bbox*,GetVolumeBox)() const = 0;

//----------------------------
// Get pointer to volume-box's matrix used to transform the volume box into joint's local coordinates.
// The method returns NULL if volume box is not set.
   I3DMETHOD_(const S_matrix*,GetVolumeBoxMatrix)() const = 0;
   */
//----------------------------
// Get volume information. The func returns I3DERR_NOTINITIALIZED if volbox not set.
// Params:
//    pos ... center of volume capped cylinder, relative to this joint
//    dir_normal ... unit-length direction vector of cylinder's main axis
//    radius ... cylinder's radius
//    half_len ... cylinder half length (without capped parts)
   I3DMETHOD_(I3D_RESULT,GetVolume)(S_vector &pos, S_vector &dir_normal, float &radius, float &half_len) const = 0;
};
#endif

typedef class I3D_joint *PI3D_joint;
typedef const I3D_joint *CPI3D_joint;

//----------------------------

#if !defined I3D_FULL 
class I3D_user: public I3D_frame{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;
                              //additional user data
   I3DMETHOD_(void,SetData2)(dword) = 0;
   I3DMETHOD_(dword,GetData2)() const = 0;
};
#endif

typedef class I3D_user *PI3D_user;
typedef const I3D_user *CPI3D_user;

//----------------------------

enum I3D_VOLUMETYPE{
   I3DVOLUME_NULL,
   I3DVOLUME_SPHERE, I3DVOLUME_RECTANGLE, I3DVOLUME_BOX,
   I3DVOLUME_CYLINDER, I3DVOLUME_CAPCYL,
};

#if !defined I3D_FULL 
class I3D_volume: public I3D_frame{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD(SetNUScale)(const S_vector&) = 0;
   I3DMETHOD_(const S_vector&,GetNUScale)() const = 0;

   I3DMETHOD(SetVolumeType)(I3D_VOLUMETYPE) = 0;
   I3DMETHOD_(void,SetOwner)(PI3D_frame) = 0;
   I3DMETHOD_(void,SetCollisionMaterial)(dword) = 0;

   I3DMETHOD_(I3D_VOLUMETYPE,GetVolumeType)() const = 0;
   I3DMETHOD_(PI3D_frame,GetOwner)() const = 0;
   I3DMETHOD_(dword,GetCollisionMaterial)() const = 0;

   I3DMETHOD_(void,SetCategoryBits)(dword d) = 0;
   I3DMETHOD_(dword,GetCategoryBits)() const = 0;
   I3DMETHOD_(void,SetCollideBits)(dword d) = 0;
   I3DMETHOD_(dword,GetCollideBits)() const = 0;
};
#endif

typedef class I3D_volume *PI3D_volume;
typedef const I3D_volume *CPI3D_volume;

//----------------------------

enum I3D_OCCLUDERTYPE{
   I3DOCCLUDER_MESH, I3DOCCLUDER_SPHERE,
};


#if !defined I3D_FULL
class I3D_occluder: public I3D_frame{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD(Build)(const S_vector *verts, dword num_verts) = 0;
                              //vertices
   I3DMETHOD_(dword,NumVertices)() const = 0;
   I3DMETHOD_(const S_vector*,LockVertices)() = 0;
   I3DMETHOD_(void,UnlockVertices)() = 0;

   I3DMETHOD(SetOccluderType)(I3D_OCCLUDERTYPE) = 0;
   I3DMETHOD_(I3D_OCCLUDERTYPE,GetOccluderType)() const = 0;

   I3DMETHOD_(dword,NumFaces)() const = 0;
   I3DMETHOD_(const I3D_face*,GetFaces)() const = 0;
};
#endif

typedef class I3D_occluder *PI3D_occluder;
typedef const I3D_occluder *CPI3D_occluder;

//----------------------------

typedef class I3D_portal *PI3D_portal;
typedef const I3D_portal *CPI3D_portal;

#if !defined I3D_FULL
class I3D_portal{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD_(dword,NumVertices)() const = 0;
   I3DMETHOD_(const S_vector*,GetVertices)() const = 0;
   I3DMETHOD_(const S_plane&,GetPlane)() const = 0;
   I3DMETHOD_(PI3D_sector,GetSector)() = 0;
   I3DMETHOD_(CPI3D_sector,GetSector)() const = 0;
   I3DMETHOD_(PI3D_sector,GetConnectSector)() = 0;
   I3DMETHOD_(CPI3D_sector,GetConnectSector)() const = 0;

   I3DMETHOD_(void,SetOn)(bool) = 0;
   I3DMETHOD_(bool,IsOn)() const = 0;
};
#endif

//----------------------------

#if !defined I3D_FULL 
class I3D_sector: public I3D_frame{
public:
   I3DMETHOD_(bool,CheckPoint)(const S_vector &v) const = 0;
   I3DMETHOD_(bool,IsPrimary)() const = 0;
                              //portals
   I3DMETHOD_(dword,NumPortals)() const = 0;
   I3DMETHOD_(const PI3D_portal*,GetPortals)() const = 0;
   I3DMETHOD_(dword,NumConnectPortals)() const = 0;
   I3DMETHOD_(const PI3D_portal*,GetConnectPortals)() const = 0;

                              //lights
   I3DMETHOD(AddLight)(PI3D_light) = 0;
   I3DMETHOD(RemoveLight)(PI3D_light) = 0;
   I3DMETHOD_(dword,NumLights)() const = 0;
   I3DMETHOD_(PI3D_light,GetLight)(int index) = 0;
   I3DMETHOD_(CPI3D_light,GetLight)(int index) const= 0;

                              //sounds
   I3DMETHOD(AddSound)(PI3D_sound) = 0;
   I3DMETHOD(RemoveSound)(PI3D_sound) = 0;
   I3DMETHOD_(dword,NumSounds)() const = 0;
   I3DMETHOD_(PI3D_sound const*,GetSounds)() = 0;
   I3DMETHOD_(CPI3D_sound const*,GetSounds)() const = 0;

                              //environment
   I3DMETHOD_(void,SetEnvironmentID)(dword) = 0;
   I3DMETHOD_(dword,GetEnvironmentID)() const = 0;

   I3DMETHOD_(void,SetTemperature)(float) = 0;
   I3DMETHOD_(float,GetTemperature)() const = 0;

   I3DMETHOD_(dword,NumBoundPlanes)() const = 0;
   I3DMETHOD_(const S_plane*,GetBoundPlanes)() const = 0;
};
#endif

//----------------------------
                              //macros for conversion of float values
inline int I3DFloatAsInt(float f){ return *(int*)&f; }
inline float I3DIntAsFloat(int i){ return (*(float*)&i); }

//----------------------------
                              //particle properties:
enum{
   I3DPROP_PRTC_I_OUT_TIME,        //int
   I3DPROP_PRTC_I_OUTT_THRESH,     //int
   I3DPROP_PRTC_I_LIFE_LEN,        //int
   I3DPROP_PRTC_I_LIFEL_THRESH,    //int
   I3DPROP_PRTC_F_SCALE_GROW_MIN,  //float
   I3DPROP_PRTC_F_SCALE_GROW_MAX,  //float
   I3DPROP_PRTC_F_INIT_POS_THRESH, //float
   I3DPROP_PRTC_F_INIT_DIR_THRESH, //float
   I3DPROP_PRTC_I_AFFECT_TIME,     //int
   I3DPROP_PRTC_I_AFFECTT_THRESH,  //int
   I3DPROP_PRTC_F_AFFECT_DIR_THRESH,//float
   I3DPROP_PRTC_F_OPACITY_IN,      //float
   I3DPROP_PRTC_F_OPACITY_OUT,     //float
   I3DPROP_PRTC_F_BASE_DIR_X, I3DPROP_PRTC_F_BASE_DIR_Y, I3DPROP_PRTC_F_BASE_DIR_Z, //float
   I3DPROP_PRTC_F_DEST_DIR_X, I3DPROP_PRTC_F_DEST_DIR_Y, I3DPROP_PRTC_F_DEST_DIR_Z, //float
   I3DPROP_PRTC_E_DRAW_MODE,       //int
   I3DPROP_PRTC_I_SETCOUNT,        //int
   I3DPROP_PRTC_B_BASE_DIR_WORLD,  //bool
   I3DPROP_PRTC_B_DEST_DIR_WORLD,  //bool
   I3DPROP_PRTC_B_EMIT_BASE,       //bool
   I3DPROP_PRTC_F_ROT_SPEED_MIN,   //float
   I3DPROP_PRTC_F_ROT_SPEED_MAX,   //float
   I3DPROP_PRTC_F_ELEMENT_SCALE,   //float
   I3DPROP_PRTC_F_OPACITY,         //float
   I3DPROP_PRTC_E_MODE,            //0=billb, 1=XY, 2=XZ, 3=YZ
   I3DPROP_PRTC_B_OPTIMIZED,       //bool
   I3DPROP_PRTC_MATERIAL,        //PI3D_material
   I3DPROP_PRTC_RESERVED2,
   I3DPROP_PRTC_I_NUMELEMENTS,     //int (read-only)
};

//----------------------------
                              //billboard properties:
enum{
   I3DPROP_BBRD_B_AXIS_MODE,    //bool
   I3DPROP_BBRD_I_ROT_AXIS,     //int (0..2)
};

//----------------------------
                              //flare properties:
enum{
   I3DPROP_FLRE_F_SCALE_DIST,   //float
   I3DPROP_FLRE_F_ROTATE_RATIO, //float
   I3DPROP_FLRE_F_NORMAL_ANGLE, //float
   I3DPROP_FLRE_I_FADE_TIME,    //int
   I3DPROP_FLRE_F_PIVOT,        //float
};

//----------------------------
                              //lightmap properties:
enum{
   I3DPROP_LMAP_F_RESOLUTION,   //float
};

//----------------------------
                              //morph properties:
enum{
   I3DPROP_MRPH_F_POWER,        //float
   I3DPROP_MRPH_E_MODE,         //int (0=channel, 1=regions, 2=vertices)
};

//----------------------------
                              //UV shift object
enum{
   I3DPROP_OUVS_F_SHIFT_U,    //float
   I3DPROP_OUVS_F_SHIFT_V,    //float
   I3DPROP_OUVS_E_MODE,       //int (0=animate, 1=delta)
};

//----------------------------
                              //atmos properties:
enum{
   I3DPROP_ATMO_F_DIR_X,      //float
   I3DPROP_ATMO_F_DIR_Y,      //float
   I3DPROP_ATMO_F_DIR_Z,      //float
   I3DPROP_ATMO_I_NUM_ELEMS,  //int
   I3DPROP_ATMO_F_DEPTH,      //float
   I3DPROP_ATMO_F_SCALE_X,    //float
   I3DPROP_ATMO_F_SCALE_Y,    //float
};

//----------------------------
                              //animation options and key definition

                              //value applying mode:
#define I3DANIMOP_SET         0  //replace previous value by new value
#define I3DANIMOP_ADD         1  //add new value (scaled by scale factor) with previous value
#define I3DANIMOP_BLEND       2  //blend new value (scaled by scale factor) with previous value using blend factor
#define I3DANIMOP_DAMP_ADD    3  //damp previous value by damp factor, and add current value
#define I3DANIMOP_APPLY_MASK  3  //mask of apply mode

                              //precedence braces count
#define I3DANIMOP_BRACE_MASK  0x03c0
#define I3DANIMOP_BRACE_SHIFT 6

                              //modifier flags:
#define I3DANIMOP_LOOP 0x10000   //loop animation (if not specified, anim is played only once and stopped)
#define I3DANIMOP_KEEPDONE 0x20000  //do not erase finished animations
#define I3DANIMOP_FINISH_NOTIFY 0x40000   //use callback (CBM_ANIM_FINISH) when animation is done

                              //animation structs

                              //time unit = 1/1000 sec
                              //tension:    -1 .. 1
                              //continuity: -1 .. 1 (-1 sharp (^), 0 smooth(n), 1 not recommended (nn))
                              //bias:       -1 .. 1
                              //easy_from:   0 .. 1
                              //easy_to:     0 .. 1
struct I3D_easiness{
   float easy_from, easy_to;
};

template<class T>
struct I3D_bezier{
   T in_tan, out_tan;
};
struct I3D_tcb{
   float tension, continuity, bias;
};
struct I3D_anim_vector_tcb: public I3D_easiness, public I3D_tcb{
   int time;
   S_vector v;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_vector_tcb)); }
};
typedef I3D_anim_vector_tcb I3D_anim_pos_tcb;

struct I3D_anim_vector_bezier: public I3D_easiness, public I3D_bezier<S_vector>{
   int time;
   S_vector v;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_vector_bezier)); }
};
typedef I3D_anim_vector_bezier I3D_anim_pos_bezier;

struct I3D_anim_rot: public I3D_easiness{
   int time;
   S_vector axis;
   float angle;
   float smoothness;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_rot)); smoothness = 1.0f; }
};

struct I3D_anim_quat: public I3D_easiness{
   int time;
   S_quat q;
   float smoothness;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_quat)); smoothness = 1.0f; }
};

struct I3D_anim_quat_bezier: public I3D_easiness, public I3D_bezier<S_quat>{
   int time;
   S_quat q;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_quat_bezier)); }
};

struct I3D_anim_note{
   int time;
   const char *note;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_note)); }
};

struct I3D_anim_power: public I3D_easiness{
   int time;
   float power;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_power)); }
};

/*
struct I3D_anim_visibility{
   int time;
   float visibility;
   inline void Clear(){ memset(this, 0, sizeof(I3D_anim_visibility)); }
};
*/
typedef I3D_anim_power I3D_anim_visibility;

//----------------------------

enum I3D_ANIMATION_TYPE{
   I3DANIM_KEYFRAME,          //animation consisting of key-frame tracks
   I3DANIM_POSE,              //static pose
};

//----------------------------

#if !defined I3D_FULL
class I3D_animation_base{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD_(I3D_ANIMATION_TYPE,GetType)() const = 0;
};
#endif

typedef class I3D_animation_base *PI3D_animation_base;
typedef const I3D_animation_base *CPI3D_animation_base;

//----------------------------

#if !defined I3D_FULL
class I3D_keyframe_anim: public I3D_animation_base{
public:
   I3DMETHOD_(void,Clear)() = 0;
   I3DMETHOD(SetPositionKeys)(I3D_anim_pos_tcb*, dword num_keys) = 0;
   I3DMETHOD(SetRotationKeys)(I3D_anim_rot*, dword num_keys) = 0;
   I3DMETHOD(SetRotationKeys1)(I3D_anim_quat*, dword num_keys) = 0;
   I3DMETHOD(SetNoteKeys)(I3D_anim_note*, dword num_keys) = 0;
   I3DMETHOD(SetVisibilityKeys)(I3D_anim_visibility*, dword num_keys) = 0;
   I3DMETHOD(SetPowerKeys)(I3D_anim_power*, dword num_keys) = 0;
   I3DMETHOD(SetEndTime)(dword) = 0;

   I3DMETHOD(GetPositionKeys)(const I3D_anim_pos_bezier**, dword *num_keys) const = 0;
   I3DMETHOD(GetRotationKeys)(const I3D_anim_quat_bezier**, dword *num_keys) const = 0;
   I3DMETHOD(GetPowerKeys)(const I3D_anim_power**, dword *num_keys) const = 0;
   I3DMETHOD(GetNoteKeys)(const I3D_anim_note**, dword *num_keys) const = 0;
   I3DMETHOD(GetVisibilityKeys)(const I3D_anim_visibility**, dword *num_keys) const = 0;

   I3DMETHOD_(dword,GetEndTime)() const = 0;
};
#endif

typedef class I3D_keyframe_anim *PI3D_keyframe_anim;
typedef const I3D_keyframe_anim *CPI3D_keyframe_anim;

//----------------------------

#ifndef I3D_FULL
class I3D_anim_pose: public I3D_animation_base{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

//----------------------------
// Clear pose - reset all elements to uninitialized.
   I3DMETHOD_(void,Clear)() = 0;

//----------------------------
// Set position of pose. When param is NULL, the position is set to uninitialized.
   I3DMETHOD_(void,SetPos)(const S_vector*) = 0;

//----------------------------
// Set power of pose. When param is NULL, the power is set to uninitialized.
// Power means scale/volume/lightness, dependent on frame type.
   I3DMETHOD_(void,SetPower)(const float*) = 0;

//----------------------------
// Set rotation of pose. When param is NULL, the rotation is set to uninitialized.
   I3DMETHOD_(void,SetRot)(const S_quat*) = 0;

   I3DMETHOD_(const S_vector*,GetPos)() const = 0;
   I3DMETHOD_(const S_quat*,GetRot)() const = 0;
   I3DMETHOD_(const float*,GetPower)() const = 0;
};
#endif

typedef class I3D_anim_pose *PI3D_anim_pose;
typedef const I3D_anim_pose *CPI3D_anim_pose;

//----------------------------

#if !defined I3D_FULL 
class I3D_animation_set{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD(Open)(const char *fname, dword flags = 0, I3D_LOAD_CB_PROC* = NULL, void *context = NULL) = 0;
   I3DMETHOD_(dword,GetTime)() const = 0;

   I3DMETHOD_(void,AddAnimation)(PI3D_animation_base, const char *link, float weight = 1.0f) = 0;
   I3DMETHOD(RemoveAnimation)(CPI3D_animation_base) = 0;

   I3DMETHOD_(dword,NumAnimations)() const = 0;
   //I3DMETHOD_(PI3D_animation_base,GetAnimation)(int index) = 0;
   I3DMETHOD_(CPI3D_animation_base,GetAnimation)(int index) const = 0;
   I3DMETHOD_(const C_str&,GetAnimLink)(int index) const = 0;
};
#endif

typedef class I3D_animation_set *PI3D_animation_set;
typedef const I3D_animation_set *CPI3D_animation_set;

//----------------------------

#if !defined I3D_FULL
class I3D_interpolator{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

//----------------------------
// Update interpolator by specified time - animate frame.
// When interpolator is finished with animation of all stages, the returned value is I3D_DONE.
   I3DMETHOD_(I3D_RESULT,Tick)(int time, PI3D_CALLBACK=NULL, void *cb_context = NULL) = 0;

//----------------------------
// Set frame on which this interpolator operates (i.e. frame to which it applies results of animation).
   I3DMETHOD_(void,SetFrame)(PI3D_frame) = 0;

//----------------------------
// Setup animation for particular stage.
   I3DMETHOD_(I3D_RESULT,SetAnimation)(CPI3D_keyframe_anim, dword options = 0) = 0;

   I3DMETHOD(SetCurrTime)(dword time) = 0;

//   I3DMETHOD_(PI3D_animation_base,GetAnimation)() = 0;
   I3DMETHOD_(CPI3D_animation_base,GetAnimation)() const = 0;
//----------------------------
// Get frame on which this interpolator operates.
   I3DMETHOD_(PI3D_frame,GetFrame)() = 0;
   I3DMETHOD_(CPI3D_frame,GetFrame)() const = 0;

//----------------------------
// Get animation options.
   I3DMETHOD_(dword,GetOptions)() const = 0;

//----------------------------
   I3DMETHOD_(dword,GetEndTime)() const = 0;

//----------------------------
   I3DMETHOD_(dword,GetCurrTime)() const = 0;
};
#endif

typedef class I3D_interpolator *PI3D_interpolator;
typedef const I3D_interpolator *CPI3D_interpolator;

//----------------------------
                              //container - base class keeping references to frames and interpolators,
                              // allows for loading scene files
                              // used as base for I3D_scene and I3D_model classes

#define I3D_CONTAINER(END_LINE) \
/* Open scene hierarchy file from disk.*/ \
   I3DMETHOD_(I3D_RESULT,Open)(const char* fname, dword flags = 0, I3D_LOAD_CB_PROC* = NULL, \
      void *context = NULL) END_LINE \
/* Close - erase contents (frames, interpolators, etc).*/ \
   I3DMETHOD_(void,Close)() END_LINE \
/* Get name which was last used to Open method, regardless of success.*/ \
   I3DMETHOD_(const C_str&,GetFileName)() const END_LINE \
/* Store filename. */ \
   I3DMETHOD_(void,StoreFileName)(const C_str&) END_LINE \
/* Add/delete frame from interal list.*/ \
   I3DMETHOD_(I3D_RESULT,AddFrame)(PI3D_frame frm) END_LINE \
   I3DMETHOD_(I3D_RESULT,RemoveFrame)(PI3D_frame frm) END_LINE \
/* Add frame into internal list - it is kept there until not Close-d or Release-d.*/ \
   I3DMETHOD_(dword,NumFrames)() const END_LINE \
/* Get access to internal frame list - number and pointer to frames.*/ \
   I3DMETHOD_(PI3D_frame const*,GetFrames)() END_LINE \
   I3DMETHOD_(CPI3D_frame const*,GetFrames)() const END_LINE \
/* Get access to internal list of interpolators - add, delete, number and pointer to them.*/ \
   I3DMETHOD_(I3D_RESULT,AddInterpolator)(PI3D_interpolator) END_LINE \
   I3DMETHOD_(I3D_RESULT,RemoveInterpolator)(PI3D_interpolator) END_LINE \
   I3DMETHOD_(dword,NumInterpolators)() const END_LINE \
   I3DMETHOD_(PI3D_interpolator const*,GetInterpolators)() END_LINE \
   I3DMETHOD_(CPI3D_interpolator const*,GetInterpolators)() const END_LINE \
/* Update all interpolators.*/ \
   I3DMETHOD_(I3D_RESULT,Tick)(int time, PI3D_CALLBACK=NULL, void *cb_context = NULL) END_LINE \
/* Set/Get user data */ \
   I3DMETHOD_(const C_str&,GetUserData)() const END_LINE \
   I3DMETHOD_(void,SetUserData)(const C_str &s) END_LINE \


#if !defined I3D_FULL
class I3D_container{
public:
   I3D_CONTAINER( = 0;)
};
#endif

typedef class I3D_container *PI3D_container;
typedef const I3D_container *CPI3D_container;

//----------------------------

#if !defined I3D_FULL 
class I3D_model: public I3D_frame{
public:
//----------------------------
// Get minimal bounding volume enclosing all visual children of model.
// This may be invalidated bounding volume if there's no geometry in the hierarchy.
   I3DMETHOD_(const I3D_bound_volume&,GetHRBoundVolume)() const = 0;

//----------------------------
// Set animation-set on children of model. This function resolves links stored in
// animset, sets up interpolators processing the animation in specified mode.
   I3DMETHOD_(I3D_RESULT,SetAnimation)(dword stage, CPI3D_animation_set, dword options = I3DANIMOP_SET, float scale_factor = 1.0f,
      float blend_factor = 1.0f, float speed_factor = 1.0f) = 0;

   I3DMETHOD_(I3D_RESULT,SetPose)(PI3D_animation_set);

//----------------------------
// Set/Get scale factor of all interpolators working with specified stage.
   I3DMETHOD_(I3D_RESULT,SetAnimScaleFactor)(dword stage, float scale_factor) = 0;
   I3DMETHOD_(float,GetAnimScaleFactor)(dword stage = 0) const = 0;

//----------------------------
// Set/Get blend factor of all interpolators working with specified stage.
   I3DMETHOD_(I3D_RESULT,SetAnimBlendFactor)(dword stage, float blend_factor) = 0;
   I3DMETHOD_(float,GetAnimBlendFactor)(dword stage = 0) const = 0;

//----------------------------
// Set/Get speed multiplier of all interpolators working with specified stage.
   I3DMETHOD_(I3D_RESULT,SetAnimSpeed)(dword stage, float speed) = 0;
   I3DMETHOD_(float,GetAnimSpeed)(dword stage = 0) const = 0;

//----------------------------
// Get animation end time on specified stage. If stage is not set up, the returned value is -1.
   I3DMETHOD_(int,GetAnimEndTime)(dword stage = 0) const = 0;

//----------------------------
// Get animation current time on specified stage. If stage is not set up, the returned value is -1.
   I3DMETHOD_(int,GetAnimCurrTime)(dword stage = 0) const = 0;

//----------------------------
// Set animation current time on specified stage.
   I3DMETHOD_(I3D_RESULT,SetAnimCurrTime)(dword stage, dword time) = 0;

//----------------------------
// Move given animation stage into new position.
   I3DMETHOD_(I3D_RESULT,MoveAnimStage)(dword from, dword to) = 0;

//----------------------------
// Stop animation playback on specified stage.
   I3DMETHOD_(I3D_RESULT,StopAnimation)(dword stage) = 0;

//----------------------------
// Get animation set loaded from file when I3D_model::Open was last called.
   I3DMETHOD_(PI3D_animation_set,GetAnimationSet)() = 0;
   I3DMETHOD_(CPI3D_animation_set,GetAnimationSet)() const = 0;

//----------------------------
// Setup delta position/scale for animation. This is useful if animated position/scale is scaled or blended
//  with other stages. This position/scale represents a logical origin, around which scaling and blending
//  is done. This must be set separatelly for each frame.
// The frame must be in model's container, otherwise the call fails.
// Passing NULL in 'pos' or 'scl' removes the association, and reference values are S_vector(0, 0, 0), resp. float(0).
   I3DMETHOD_(I3D_RESULT,SetAnimBasePos)(PI3D_frame frm, const S_vector *pos) = 0;
   I3DMETHOD_(I3D_RESULT,SetAnimBaseScale)(PI3D_frame frm, const float *scl) = 0;

//----------------------------
// The rest is container.
   I3D_CONTAINER( = 0;)
   I3DMETHOD_(PI3D_container,GetContainer)() = 0;
   I3DMETHOD_(CPI3D_container,GetContainer)() const = 0;
};
#endif

typedef class I3D_model *PI3D_model;
typedef const I3D_model *CPI3D_model;

//----------------------------
                              //create texture flags - used with I3D_CREATETEXTURE structure
#define TEXTMAP_DIFFUSE          1        //text_name valid, specifying diffuse bitmap
#define TEXTMAP_OPACITY          2        //text_name valid, specifying opacity bitmap
#define TEXTMAP_EMBMMAP          4        //text_name valid, specifying env-map bumpmap bitmap
#define TEXTMAP_CUBEMAP          8        //create cubic texture
#define TEXTMAP_NO_SYSMEM_COPY   0x10     //do not create system-memory copy of texture (empty textures only)
#define TEXTMAP_PROCEDURAL       0x20     //proc_name and proc_data valid, specifying procedural texture
#define TEXTMAP_TRANSP           0x40     //texture with transparent (color-keyed) texels
#define TEXTMAP_MIPMAP           0x100    //generate mipmap levels
#define TEXTMAP_NOMIPMAP         0x200    //disable auto-mipmap feature
#ifndef GL
#define TEXTMAP_COMPRESS         0x800    //create compressed texture (if supported by hw)
#endif
#define TEXTMAP_USEPIXELFORMAT   0x1000   //use specified pixel format
#define TEXTMAP_HINTDYNAMIC      0x2000   //dymanic texture
#define TEXTMAP_TRUECOLOR        0x4000   //choose true-color pixel format, if available
#define TEXTMAP_USE_CACHE        0x8000   //diffuse and/or opacity maps specified by C_cache, rather than by filenames
#define TEXTMAP_RENDERTARGET     0x10000  //request texture which may be used as rendertarget
#define TEXTMAP_NORMALMAP        0x20000  //text_name valid, specifying normal map

                              //find texture format flags (hint only)
#ifndef GL
#define FINDTF_PALETIZED   1        //paletized textures
#endif
#define FINDTF_ALPHA       2        
#define FINDTF_ALPHA1      4
#ifndef GL
#define FINDTF_COMPRESSED  8
#endif
#define FINDTF_TRUECOLOR   0x10     //true-color
#define FINDTF_EMBMMAP     0x20     //only embm-map formats
#define FINDTF_RENDERTARGET 0x40    //render target surfaces
                              //create texture struct
struct I3D_CREATETEXTURE{
   dword flags;               //TEXTMAP_ flags
   union{                     //diffuse bitmap - valid with TEXTMAP_DIFFUSE flag
      const char *file_name;  //source file name, used with TEXTMAP_DIFFUSE, TEXTMAP_EMBMMAP, TEXTMAP_PROCEDURAL, TEXTMAP_NORMALMAP
      class C_cache *ck_diffuse; //source cache, used in combination with TEXTMAP_USE_CACHE
   };
   union{                     //opacity bitmap - valid with TEXTMAP_OPACITY flag
      const char *opt_name;   //file name
      class C_cache *ck_opacity;
      dword proc_data;        //specializaton of procedural
   };
   dword size_x, size_y;
   struct S_pixelformat *pf;  //requested pixel format, used with TEXTMAP_USEPIXELFORMAT
};

//----------------------------
                              //collision test flags (lower 16 bits is for mode, higher 16 bits are additional flags)
enum I3D_COLLISION_TEST_FLAGS{
   I3DCOL_MOVING_SPHERE = 1,        //moving sphere against BSP + dynamic volumes
   I3DCOL_MOVING_GROUP,             //moving sphere group against BSP + dynamic volumes
   I3DCOL_STATIC_SPHERE,            //static sphere against BSP
   I3DCOL_LINE,                     //line segment against BSP + dynamic volumes
   I3DCOL_VOLUMES,                  //line against volumes (no BSP, accessing raw volumes - static+dynamic)
   I3DCOL_EDIT_FRAMES,              //line against currently visualized edit frames (lights, sounds, ...)
   I3DCOL_NOMESH_VISUALS,           //line against mesh-less visuals (no BSP or volumes)
   I3DCOL_STATIC_BOX,               //static box against dymanic volumes (currently spheres only)
   I3DCOL_EXACT_GEOMETRY,           //line against all geometry
   I3DCOL_MODE_LAST,                //last mode (not valid for passing to collision detection)

   I3DCOL_RAY           = 0x10000,  //ray instead of line segment (valid only for line tests)
   I3DCOL_COLORKEY      = 0x20000,  //check transparent texels (line against BSP only)
   I3DCOL_INVISIBLE     = 0x40000,  //test also invisible frames
   I3DCOL_FORCE2SIDE    = 0x80000,  //always consider faces to be 2-sided, regardless of material (also volumes)
   I3DCOL_SOLVE_SLIDE   = 0x100000, //compute sphere slide along collision planes (moving sphere test only)
   I3DCOL_JOINT_VOLUMES = 0x200000, //test also joint volumes on singlemesh (line test only)
};

//----------------------------
                              //callback used during generating contacts
typedef dword I3DAPI I3D_contact_query(CPI3D_frame frm, void *context);
typedef void I3DAPI I3D_contact_report(CPI3D_frame frm, const S_vector &pos, const S_vector &normal,
   float depth, void *context);

                              //structure passed to I3D_scene::GenerateContacts:
struct I3D_contact_data{
   CPI3D_volume vol_src;
   CPI3D_frame frm_ignore;
   I3D_contact_query *cb_query;
   I3D_contact_report *cb_report;
   void *context;             //context passed to callbacks

   I3D_contact_data():
      vol_src(NULL),
      frm_ignore(NULL),
      cb_query(NULL),
      cb_report(NULL),
      context(NULL)
   {
   }
};

//----------------------------
                              //bsp load flags:
                              //perform checksum, delete tree if failed. use only in editor,
                              //don't use in final game where BSP MUST be valid as is.
#define FBSPLOAD_CHECKSUM     1
                              //check consistecy of tree - report which frame are marked static but not in tree.
                              //don't use in final game, this slow down loading.
#define FBSPLOAD_CONSISTENCE  2

//----------------------------

                              //collision response function
typedef bool I3DAPI I3D_collision_response(struct I3D_cresp_data&);
typedef I3D_collision_response *PI3D_collision_response;

                              //builder callback
enum I3D_BSP_MESSAGE{
   BM_GETTING_POLYGONS,       //prm1: (dword) num polygons collected.
   BM_BUILDING_NODES,         //prm1: (dword) num polygons rest to build.
   BM_BUILDING_EDGES,
   BM_LOG,                    //prm1: (const char*) message, prm2: (int) priority (0 err, 1 warn)
};
                              //called during build of bsp tree
typedef bool I3DAPI I3D_BSP_BUILD_CB_PROC(I3D_BSP_MESSAGE msg, dword prm1, dword prm2, void *context);

                              //note: when testing SPHERE_GROUP, each face if tested for entire group only ones
                              //thus if you reject volume which collide with sphere1, you cannot expect to
                              //response it for volume_source sphere2 in same test.

//----------------------------

struct I3D_collision_base{
                              //input:
public:                       
   S_vector from;             //Position from where collision is tested.
   S_vector dir;              //Direction of test,length used uless I3DCOL_RAY is specified. 
   void *cresp_context;       //User collision context. Passed from collision function to callback functions (I3D_collision_response).
   I3D_collision_base():
      cresp_context(NULL)
   {
   }
                              //output:
protected:
   PI3D_frame hit_frm;
   float hit_distance;        //distance at which collision accured. value varies by collision type.
   S_vector hit_normal;       //normal of impact plane.
   S_vector destination;      //position where we are going to. can be ajusted in response.
   int face_index;

   friend class I3D_scene_imp;    //acess to output
                              //output(only for internal use):
public:     
                              //set common return values
   inline void SetReturnValues(float h_dist, PI3D_frame h_frm, const S_vector &h_norm){
      hit_distance = h_dist;
      hit_frm = h_frm;
      hit_normal = h_norm;
   }
   inline void CopyInputValues(const I3D_collision_base &cb){
      from = cb.from;
      dir = cb.dir;
      cresp_context = cb.cresp_context;
   }
                              //access data:
public:
                              //Distance on tested line where collision accured. Distance is normalized.
   inline float GetHitDistance() const { return hit_distance; }
                              //Calculate position where collision accure and return it.
   inline S_vector ComputeHitPos() const { return from + dir*hit_distance / dir.Magnitude(); }
                              //Closest frame which was hitted.
   inline PI3D_frame GetHitFrm() const { return hit_frm; }
                              //Normal of impact plane.
   inline const S_vector &GetHitNormal() const { return hit_normal; }
                              //Return solved position after collision response (user or default sliding). Calling this method is applicable only after I3DCOL_MOVING_SPHERE or I3DCOL_MOVING_GROUP collision tests.
   inline const S_vector &GetDestination() const { return destination; }

   inline int GetFaceIndex() const { return face_index; }
};

//----------------------------

struct I3D_collision_data: public I3D_collision_base{
                              //in:

   dword flags;               //combination of I3DCOL_? flags.

                              //callback function, called after every detected collision (may be NULL if not specified otherwise)
   PI3D_collision_response callback;
   union{
                              //callback called after every line/texel test (if I3DCOL_COLORKEY is specified)
      PI3D_collision_response texel_callback;
      PI3D_collision_response contact_callback;
   };
   CPI3D_frame frm_ignore;    //This frame and all it's children are skipped in tests. Currently used only in Line tests.
   float radius;              //Radius of tested sphere. Used only in I3DCOL_MOVING_SPHERE or I3DCOL_STATIC_SPHERE tests.
   PI3D_frame frm_root;       //Frame which will be enumerated for all sphere volumes and those will be tested as group. Used only with I3DCOL_MOVING_GROUP.
   const PI3D_volume *vol_group; //Alterantive to frm_root. Instead it allow direct pass array of volumes, which will be tested. Used only with I3DCOL_MOVING_GROUP.
   dword num_vols;            //Size of above.
   dword collide_bits;        //bits which must match in tested volume's category bits, in order to be accepted
private:
                              //internal values, prepared before actual test
   S_vector normalized_dir;   //normalized version of dir
   float dir_magnitude;       //length of dir

   inline void ResetInput(){
      from.Zero();
      dir.Zero();
      flags = 0;
      frm_ignore = NULL;
      callback = NULL;
      texel_callback = NULL;
      frm_root = NULL;
      vol_group = NULL;
      num_vols = 0;
      collide_bits = 0xffff;
      radius = 0;
   }
   friend class I3D_scene_imp;
   friend class C_bsp_tree;
public:
   I3D_collision_data(){
      ResetInput();
   }
   I3D_collision_data(const S_vector &in_from, const S_vector &in_dir, dword in_flags = I3DCOL_LINE, CPI3D_frame in_fi = NULL){
      ResetInput();
      from = in_from; 
      dir = in_dir; 
      flags = in_flags; 
      frm_ignore = in_fi;
   }
                              //temporary (valid during or after the test):
   inline const S_vector &GetNormalizedDir() const { return normalized_dir; }
   inline float GetDirMagnitude() const { return dir_magnitude; }
};

//----------------------------

#if !defined I3D_FULL
class I3D_texel_collision{
public:
   I3D_text_coor tex;         //uv coordinates being collided
   bool loc_pos;              //when true, point positions are in local coordinates
   const S_vector *v[3];      //points of triangle
   word face_index;           //face index on which collision occured
   S_vector point_on_plane;   //point on face's plane
   CPI3D_material mat;        //material of face

//----------------------------
// Compute uv coordinates on triangle, using provided uv coordinates on triangle's vertices.
// Parameters:
//    uv0, uv1, uv2 ... uv coordinates on corners of triangle
// Return value:
//    true if computation succeeded
// Function works with inputs set in 'v[3]', 'point_on_plane', and stores result into variable 'tex'.
   virtual bool ComputeUVFromPoint(const I3D_text_coor &uv0, const I3D_text_coor &uv1, const I3D_text_coor &uv2) = 0;
};
#endif

//----------------------------

struct I3D_cresp_data: public I3D_collision_base{

   PI3D_volume vol_source;    //Volume which is source of collison (valid only for I3DCOL_MOVING_GROUP, otherwise NULL).
   const class I3D_texel_collision *texel_data; //texture data of currently tested texel (valid only in cresp_texel callback)
   S_vector face_normal;      //normal of impact face. can be different from hit_normal when sliding face edge.
   bool modify_normal;        //Normal of face with which we collide. It can difference from GetHitNormal() usually when sphere collide with face edge, or when hit normal was modified in response.

   friend class C_bsp_tree;   //for face_index
private:
   void Reset(){
      vol_source = NULL;
      texel_data = NULL;
      modify_normal = false;
   }
                              //out:
public:
//----------------------------
// Change hit normal for further process. Valid only for I3DCOL_MOVING_SPHERE and I3DCOL_MOVING_GROUP.
   inline void ModifyNormal(const S_vector &new_normal){
      hit_normal = new_normal;
      modify_normal = true;
   }

//----------------------------
// Change destination for further process. Valid only for I3DCOL_MOVING_SPHERE and I3DCOL_MOVING_GROUP.
   inline void SetDestination(const S_vector &new_dest){ destination = new_dest; }

//----------------------------
   I3D_cresp_data(){ 
      Reset(); 
   }
   I3D_cresp_data(PI3D_frame frm_hit1, const S_vector &norm1, float dist1, void *context1){
      Reset(); 
      hit_distance = dist1;
      hit_frm = frm_hit1;
      hit_normal = norm1;
      cresp_context = context1;
      face_normal = norm1;
   }
};

//----------------------------
                              //rendering flags
#define I3DRENDER_UPDATELISTENER    1     //update 3D sound listener during this rendering

//----------------------------

#if !defined I3D_FULL

class I3D_scene{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;
                              //general
   I3DMETHOD_(PI3D_driver,GetDriver)() = 0;
   I3DMETHOD_(CPI3D_driver,GetDriver)() const = 0;
                              //rendering
   I3DMETHOD(Render)(dword flags = 0) = 0;
   I3DMETHOD(SetViewport)(const I3D_rectangle &rc_ltrb, bool aspect=false) = 0;
   I3DMETHOD_(const I3D_rectangle&,GetViewport)() const = 0;
                              //frames
   I3DMETHOD(EnumFrames)(I3D_ENUMPROC*, dword user, dword flags = ENUMF_ALL, const char *mask = NULL) const = 0;
   I3DMETHOD_(PI3D_frame,FindFrame)(const char *name, dword data=ENUMF_ALL) const = 0;
                              //lights
   I3DMETHOD_(S_vector,GetLightness)(const S_vector &pos, const S_vector *norm = NULL, dword light_mode_flags = I3DLIGHTMODE_VERTEX) const = 0;
                              //cameras
   I3DMETHOD(SetActiveCamera)(PI3D_camera) = 0;
   I3DMETHOD_(PI3D_camera,GetActiveCamera)() = 0;
   I3DMETHOD_(CPI3D_camera,GetActiveCamera)() const = 0;
   I3DMETHOD(UnmapScreenPoint)(int x, int y, S_vector &pos, S_vector &dir) const = 0;
   I3DMETHOD(TransformPointsToScreen)(const S_vector *in, S_vectorw *out, dword num) const = 0;

   I3DMETHOD_(PI3D_frame,CreateFrame)(dword frm_type, dword sub_type = 0) const = 0;
   I3DMETHOD_(PI3D_sector,GetSector)(const S_vector&, PI3D_sector suggest_sector = NULL) = 0;
   I3DMETHOD_(CPI3D_sector,GetSector)(const S_vector&, CPI3D_sector suggest_sector = NULL) const = 0;
   I3DMETHOD_(PI3D_sector,GetPrimarySector)() = 0;
   I3DMETHOD_(CPI3D_sector,GetPrimarySector)() const = 0;
   I3DMETHOD_(PI3D_sector,GetBackdropSector)() = 0;
   I3DMETHOD_(CPI3D_sector,GetBackdropSector)() const = 0;
   I3DMETHOD_(void,SetBackdropRange)(float near1, float far1) = 0;
   I3DMETHOD_(void,GetBackdropRange)(float &near1, float &far1) const = 0;
   I3DMETHOD(SetFrameSector)(PI3D_frame) = 0;
   I3DMETHOD(SetFrameSectorPos)(PI3D_frame, const S_vector &pos) = 0;
                              //background
   I3DMETHOD(SetBgndColor)(const S_vector&) = 0;
   I3DMETHOD_(const S_vector&,GetBgndColor)() const = 0;

                              //collisions
   I3DMETHOD_(bool,TestCollision)(I3D_collision_data&) const = 0;
   I3DMETHOD_(void,GenerateContacts)(const I3D_contact_data &cd) const = 0;


   I3DMETHOD_(void,AttenuateSounds)() = 0;
   I3DMETHOD_(dword,GetLastRenderTime)() const = 0;

   I3DMETHOD(SetRenderMatrix)(const S_matrix&) = 0;
   I3DMETHOD(DrawLine)(const S_vector &v1, const S_vector &v2, dword color = 0xffffffff) const = 0;

//----------------------------
// Draws multiple lines. The line is transformed by matrix set by SetRenderMatrix function before calling this function.
// v = Pointer to array of vectors specifying points of lines.
// numv = Number of vertices.
// indx = Pointer to array of indices into vertices. Each line is defined by 2 points - start and end.
// numi = Number of indices. This must be an even number.
   I3DMETHOD(DrawLines)(const S_vector *v, dword numv, const word *indx, dword numi, dword color = 0xffffffff) const = 0;

//----------------------------
// Draw list of triangles.
// v = Pointer to array of vectors specifying points of triangles. This must be pointer to array of vertices, where the format of single vertex must contain components specified by vc_flags parameter.
// numv = Number of vertices in the array pointed to by v.
// vc_flags = Combination of Vertex Component Flags.
// indx = Pointer to array of indices into vertices. Each triangle is defined by 3 points.
// numi = Number of indices. This must be a number dividable by 3.
// color = Color of triangles, in ARGB format
   I3DMETHOD(DrawTriangles)(const void *v, dword numv, dword vc_flags, const word *indx, dword numi, dword color = 0xffffffff) const = 0;

//----------------------------

   I3DMETHOD(DrawSprite)(const S_vector &pos, PI3D_material, dword color = 0xffffffff, float size = 1.0f,
      const I3D_text_coor [2] = NULL) const = 0;

   I3DMETHOD(CreateBspTree)(I3D_BSP_BUILD_CB_PROC *cbP = NULL, void *context = NULL) = 0;
   I3DMETHOD_(void,DestroyBspTree)() = 0;

   I3DMETHOD_(bool,SaveBsp)(C_cache *cp) const = 0;
                              //check_flags is combinatiopn of FBSPLOAD_? flags
   I3DMETHOD_(bool,LoadBsp)(C_cache *cp, I3D_LOAD_CB_PROC* = NULL, void *context = NULL, dword check_flags = 0) = 0;
   I3DMETHOD_(bool,IsBspBuild)() const = 0;

   I3DMETHOD(ConstructLightmaps)(PI3D_lit_object *lmaps, dword num_lmaps, dword flags = 0,
      I3D_LOAD_CB_PROC* = NULL, void *context = NULL) = 0;
   I3DMETHOD(GetStats)(I3DSTATSINDEX, void *data) = 0;

//----------------------------
// The rest is container.
   I3D_CONTAINER( = 0;)
   I3DMETHOD_(PI3D_container,GetContainer)() = 0;
   I3DMETHOD_(CPI3D_container,GetContainer)() const = 0;

};
#endif

//----------------------------
                              //init driver struct
struct I3DINIT{
   class IGraph *lp_graph;
   dword flags;
   class ISND_driver *lp_sound;
   typedef void t_log_func(const char*);
   t_log_func *log_func;
};
                              //init driver flags
#define I3DINIT_NO_PSHADER    1     //disable usage of pixel shader
#define I3DINIT_VERBOSE       2
#ifndef GL
#define I3DINIT_DIRECT_TRANS  4     //direct transformation, no ProcessVertices calls
#endif
#define I3DINIT_WBUFFER       8     //use w-buffer instead of z-buffer
#define I3DINIT_WARN_OUT      0x10  //display warnings about unreleased interfaces

typedef I3DINIT *PI3DINIT;
typedef const I3DINIT *CPI3DINIT;

//----------------------------
                              //driver rendering state
enum I3D_RENDERSTATE{
   RS_LINEARFILTER = 0,       //linear texture filtering
   RS_MIPMAP = 1,             //mipmap texture filtering
   RS_USE_EMBM = 2,           //bool (in bit 0) - use embm mapping (if bit 31 is set, vertex buffers are reset)
   RS_USEZB = 3,              //use z-buffer during rendering
   RS_CLEAR = 4,              //clear screen/zb before rendering
   RS_DITHER = 5,             //dither primitives during rendering
   RS_POINTFOG = 6,           //allow point-fog computation
   RS_WIREFRAME = 7,          //use wire-rendering
   RS_DRAWBOUNDBOX = 8,       //debug draw object bound boxes
   RS_DRAWHRBOUNDBOX = 9,     //debug draw hierarchy bound boxes
   RS_DRAWLINKS = 10,         //debug draw hierarchy links
   RS_LOADMIPMAP = 11,        //build mipmap levels during texture loading
   RS_DRAWMIRRORS = 12,       //render mirrors
   RS_USESHADOWS = 13,        //render shadows
   RS_DEBUGDRAWSHADOWS = 14,  //debug draw shadow textures and volumes
   RS_DRAWSECTORS = 15,       //debug draw sectors
   RS_DRAWPORTALS = 16,       //debug draw portals
   RS_TEXTUREDITHER = 17,     //texture dither during loading
   RS_DRAWVISUALS = 18,
   RS_FOG = 19,               //set fog on/off
   RS_LOD_QUALITY = 20,       //float number
   RS_DEBUGDRAWSHDRECS = 21,  //draw shadow receivers
   RS_TEXTURELOWDETAIL = 22,  //scale all loaded textures to 1/4 of size
   RS_DRAWVOLUMES = 23,       //debug draw collision volumes
   RS_DRAWLIGHTS = 24,        //debug draw lights ranges and cones
   RS_DEBUG_DRAW_MATS = 25,   //true to visualize materials
   RS_DRAWSOUNDS = 26,        //draw sound ranges and cones
   RS_TEXTURECOMPRESS = 27,   //use texture compression if available
   RS_USELMAPPING = 28,       //use light-mapping
   RS_DRAWLMTEXTURES = 29,    //debug draw light-map textures
   RS_DRAWCAMERAS = 30,       //debug draw cameras
   RS_DRAWDUMMYS = 31,        //debug draw dummy frames
   RS_DETAILMAPPING = 32,     //bool (in bit 0) - use detail mapping (if bit 31 is set, vertex buffers are reset)
   RS_DRAWOCCLUDERS = 33,     //debug draw occluders
   RS_DRAWTEXTURES = 34,      //draw textures
   RS_LMTRUECOLOR = 35,       //use true-color lightmaps
   RS_LMDITHER = 36,          //dither highcolor textures
   RS_LOD_SCALE = 37,         //float scale (1.0f = default, lower=better LOD, higher=worse LOD)
   RS_DEBUGDRAWBSP = 38,      //draw bsp tree polygons
   RS_DRAWJOINTS = 39,        //debug draw joints
   RS_DEBUGDRAWDYNAMIC = 40,  //draw dynamic volume tree hiearchy
   RS_DEBUGDRAWSTATIC = 41,   //draw static frames
   RS_ANISO_FILTERING = 42,   //enable anisotropic filtering
   RS_DRAW_COL_TESTS = 43,    //display all collision testing by lines
   RS_SOUND_VOLUME = 44,      //float global sound volume (0.0 ... 1.0)
   RS_PROFILER_MODE = 45,     //int mode (0 = off, 1 = absolute, 2 = self)
   RS_LOD_INDEX = 46,         //force automatic LOD, if -1, use default (computed)
   RS_USE_OCCLUSION = 47,     //enable occlusion testing (default)
   RS_LM_AA_RATIO = 48,       //LightMaps creation - antialias subpixel division (1 = no, 2 = 4x, 3 = 9x, ect)
   RS_ENVMAPPING = 49,        //bool (in bit 0) - use environment mapping (if bit 31 is set, vertex buffers are reset)
   RS_DEBUG_INT0 = 50,        //debugging purposes
   RS_DEBUG_INT1 = 51,
   RS_DEBUG_INT2 = 52,
   RS_DEBUG_INT3 = 53,
   RS_DEBUG_INT4 = 54,
   RS_DEBUG_INT5 = 55,
   RS_DEBUG_INT6 = 56,
   RS_DEBUG_INT7 = 57,
   RS_DEBUG_FLOAT0 = 58,      //debugging purposes
   RS_DEBUG_FLOAT1 = 59,
   RS_DEBUG_FLOAT2 = 60,
   RS_DEBUG_FLOAT3 = 61,
   RS_DEBUG_FLOAT4 = 62,
   RS_DEBUG_FLOAT5 = 63,
   RS_DEBUG_FLOAT6 = 64,
   RS_DEBUG_FLOAT7 = 65,
   RS_DEBUG_SHOW_OVERDRAW = 66,//show overdraw (how many times each pixel is drawn), using separate colors
};

//----------------------------

#define I3DDRAW_TRANPARENCY_MASK 0xff

//----------------------------

enum I3D_SOUND_ENV_DATA_INDEX{
                              //EAX:
   I3D_SENV_F_ROOM_LEVEL_LF,     //(float) - room level low freq., ratio (0.0 ... 1.0)
   I3D_SENV_F_ROOM_LEVEL_HF,     //(float) -    ''      high      ''
   I3D_SENV_I_REVERB_DECAY_TIME, //(int) - reverbation decay time, in msec
   I3D_SENV_F_DECAY_HF_RATIO,    //(float) - HF to LF decay time, ratio (0.0 ... 1.0)
   I3D_SENV_F_REFLECTIONS_RATIO, //(float) - early reflections level - relative to room effect, ratio (0.0 ... 1.0)
   I3D_SENV_I_REFLECTIONS_DELAY, //(int) - early reflections delay, in msec
   I3D_SENV_F_REVERB_RATIO,      //(float) - late reverberation level - relative to room effect, (0.0 ... 1.0)
   I3D_SENV_I_REVERB_DELAY,      //(int) - late reverberation delay time relative to initial reflection, in msec
   I3D_SENV_F_ENV_DIFFUSION,     //(float) - environment diffusion, ratio (0.0 ... 1.0)
                              //other:
   I3D_SENV_S_NAME,           //(const char*) - friendly name of environment
   I3D_SENV_F_EMUL_VOLUME,       //(float) - sector's volume in non-EAX mode (emulation)

   I3D_SENV_LAST,
};

//----------------------------

enum I3D_DIRECTORY_TYPE{
   I3DDIR_MAPS,
   I3DDIR_SOUNDS,
   I3DDIR_PROCEDURALS,
   I3DDIR_LAST
};

enum I3D_PROPERTYTYPE{        //size (in bytes)    notes
   I3DPROP_NULL,              //0
   I3DPROP_BOOL,              //1
   I3DPROP_INT,               //4
   I3DPROP_FLOAT,             //4
   I3DPROP_VECTOR3,           //12                 passed as pointer
   I3DPROP_VECTOR4,           //16                 ''
   I3DPROP_COLOR,             //12                 ''
   I3DPROP_STRING,            //strlen(val)        ''
   I3DPROP_ENUM,              //4
   I3DPROP_LAST
};

//----------------------------
typedef I3DENUMRET I3DAPI I3D_ENUMVISUALPROC(dword visual_type, const char *friendly_name, dword context);

                              //collision material info
struct I3D_collision_mat{
   C_str name;                //name of material
   dword color;               //visualization color
};

#if !defined I3D_FULL 
class I3D_driver{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD(GetStats)(I3DSTATSINDEX, void *data) = 0;
                              //directories
   I3DMETHOD(AddDir)(I3D_DIRECTORY_TYPE, const C_str&) = 0;
   I3DMETHOD(RemoveDir)(I3D_DIRECTORY_TYPE, const C_str&) = 0;
   I3DMETHOD(ClearDirs)(I3D_DIRECTORY_TYPE) = 0;
   I3DMETHOD_(int,NumDirs)(I3D_DIRECTORY_TYPE) const = 0;
   I3DMETHOD_(const C_str&,GetDir)(I3D_DIRECTORY_TYPE, int indx) const = 0;
                              //interfaces
   I3DMETHOD_(class ISND_driver*,GetSoundInterface)() = 0;
   I3DMETHOD_(const ISND_driver*,GetSoundInterface)() const = 0;
   I3DMETHOD_(class IGraph*,GetGraphInterface)() = 0;
   I3DMETHOD_(const IGraph*,GetGraphInterface)() const = 0;
                              //render state
   I3DMETHOD(SetState)(I3D_RENDERSTATE, dword value) = 0;
   I3DMETHOD_(dword,GetState)(I3D_RENDERSTATE) const = 0;
   I3DMETHOD(FindTextureFormat)(S_pixelformat&, dword flags) const = 0;
   I3DMETHOD(SetTexture)(CPI3D_texture) = 0;
                              //rendering
   I3DMETHOD(SetViewport)(const I3D_rectangle &rc_ltrb) = 0;
   I3DMETHOD(BeginScene)() = 0;
   I3DMETHOD(EndScene)() = 0;
   I3DMETHOD(Clear)(dword color = 0) = 0;
                              //creation - frames
   I3DMETHOD_(PI3D_scene,CreateScene)() = 0;
                              //creation - other types
   I3DMETHOD(CreateTexture)(const I3D_CREATETEXTURE*, PI3D_texture*, class C_str *err_msg = NULL) = 0;
   I3DMETHOD_(PI3D_material,CreateMaterial)() = 0;
   I3DMETHOD_(PI3D_mesh_base,CreateMesh)(dword vc_flags) = 0;
   I3DMETHOD_(PI3D_animation_base,CreateAnimation)(I3D_ANIMATION_TYPE) = 0;
   I3DMETHOD_(PI3D_animation_set,CreateAnimationSet)() = 0;
   I3DMETHOD_(PI3D_interpolator,CreateInterpolator)() = 0;
                              //visual types
   I3DMETHOD(EnumVisualTypes)(I3D_ENUMVISUALPROC*, dword user) const = 0;

   I3DMETHOD_(void,SetCallback)(PI3D_CALLBACK cb, void *cb_context) = 0;
   I3DMETHOD(SetDataBase)(const char *db_filename, dword size, int max_keep_days = 0) = 0;
   I3DMETHOD(FlushDataBase)() = 0;

   I3DMETHOD_(dword,NumProperties)(dword visual_type) const = 0;
   I3DMETHOD_(const char*,GetPropertyName)(dword visual_type, dword index) const = 0;
   I3DMETHOD_(I3D_PROPERTYTYPE,GetPropertyType)(dword visual_type, dword index) const = 0;
   I3DMETHOD_(const char*,GetPropertyHelp)(dword visual_type, dword index) const = 0;

                              //sound properties
   I3DMETHOD(SetSoundEnvData)(int set_id, I3D_SOUND_ENV_DATA_INDEX, dword data) = 0;
   I3DMETHOD_(dword,GetSoundEnvData)(int set_id, I3D_SOUND_ENV_DATA_INDEX) const = 0;

   I3DMETHOD_(void,EvictResources)() = 0;

   I3DMETHOD_(void,SetCollisionMaterial)(dword index, const I3D_collision_mat&) = 0;
   I3DMETHOD_(void,ClearCollisionMaterials)() = 0;

#ifndef GL
   I3DMETHOD_(I3D_RESULT,SetNightVision)(bool on) = 0;
   I3DMETHOD_(I3D_RESULT,BeginNightVisionRender)() = 0;
   I3DMETHOD_(I3D_RESULT,EndNightVisionRender)() = 0;
#endif
};

typedef class I3D_driver *PI3D_driver;
typedef const I3D_driver *CPI3D_driver;
#endif

//----------------------------

#if !defined I3D_FULL
                              //casting
#ifdef _DEBUG

inline PI3D_light I3DCAST_LIGHT(PI3D_frame f){ if(f && f->GetType()!=FRAME_LIGHT) throw C_except("I3DCAST_LIGHT: bad cast"); return static_cast<PI3D_light>(f); }
inline PI3D_sound I3DCAST_SOUND(PI3D_frame f){ if(f && f->GetType()!=FRAME_SOUND) throw C_except("I3DCAST_SOUND: bad cast"); return static_cast<PI3D_sound>(f); }
inline PI3D_camera I3DCAST_CAMERA(PI3D_frame f){ if(f && f->GetType()!=FRAME_CAMERA) throw C_except("I3DCAST_CAMERA: bad cast"); return static_cast<PI3D_camera>(f); }
inline PI3D_object I3DCAST_OBJECT(PI3D_frame f){ if(f && f->GetType()!=FRAME_VISUAL) throw C_except("I3DCAST_OBJECT: bad cast"); return static_cast<PI3D_object>(f); }
inline PI3D_lit_object I3DCAST_LIT_OBJECT(PI3D_frame f){ if(f && f->GetType()!=FRAME_VISUAL) throw C_except("I3DCAST_LIT_OBJECT: bad cast"); return static_cast<PI3D_lit_object>(f); }
inline PI3D_dummy I3DCAST_DUMMY(PI3D_frame f){ if(f && f->GetType()!=FRAME_DUMMY) throw C_except("I3DCAST_DUMMY: bad cast"); return static_cast<PI3D_dummy>(f); }
inline PI3D_joint I3DCAST_JOINT(PI3D_frame f){ if(f && f->GetType()!=FRAME_JOINT) throw C_except("I3DCAST_JOINT: bad cast"); return static_cast<PI3D_joint>(f); }
inline PI3D_user I3DCAST_USER(PI3D_frame f){ if(f && f->GetType()!=FRAME_USER) throw C_except("I3DCAST_USER: bad cast"); return static_cast<PI3D_user>(f); }
inline PI3D_volume I3DCAST_VOLUME(PI3D_frame f){ if(f && f->GetType()!=FRAME_VOLUME) throw C_except("I3DCAST_VOLUME: bad cast"); return static_cast<PI3D_volume>(f); }
inline PI3D_sector I3DCAST_SECTOR(PI3D_frame f){ if(f && f->GetType()!=FRAME_SECTOR) throw C_except("I3DCAST_SECTOR: bad cast"); return static_cast<PI3D_sector>(f); }
inline PI3D_model I3DCAST_MODEL(PI3D_frame f){ if(f && f->GetType()!=FRAME_MODEL) throw C_except("I3DCAST_MODEL: bad cast"); return static_cast<PI3D_model>(f); }
inline PI3D_visual I3DCAST_VISUAL(PI3D_frame f){ if(f && f->GetType()!=FRAME_VISUAL) throw C_except("I3DCAST_VISUAL: bad cast"); return static_cast<PI3D_visual>(f); }
inline PI3D_occluder I3DCAST_OCCLUDER(PI3D_frame f){ if(f && f->GetType()!=FRAME_OCCLUDER) throw C_except("I3DCAST_OCCLUDER: bad cast"); return static_cast<PI3D_occluder>(f); }

inline CPI3D_light I3DCAST_CLIGHT(CPI3D_frame f){ if(f && f->GetType()!=FRAME_LIGHT) throw C_except("I3DCAST_LIGHT: bad cast"); return static_cast<CPI3D_light>(f); }
inline CPI3D_sound I3DCAST_CSOUND(CPI3D_frame f){ if(f && f->GetType()!=FRAME_SOUND) throw C_except("I3DCAST_SOUND: bad cast"); return static_cast<CPI3D_sound>(f); }
inline CPI3D_camera I3DCAST_CCAMERA(CPI3D_frame f){ if(f && f->GetType()!=FRAME_CAMERA) throw C_except("I3DCAST_CAMERA: bad cast"); return static_cast<CPI3D_camera>(f); }
inline CPI3D_object I3DCAST_COBJECT(CPI3D_frame f){ if(f && f->GetType()!=FRAME_VISUAL) throw C_except("I3DCAST_OBJECT: bad cast"); return static_cast<CPI3D_object>(f); }
inline CPI3D_lit_object I3DCAST_CLIT_OBJECT(CPI3D_frame f){ if(f && f->GetType()!=FRAME_VISUAL) throw C_except("I3DCAST_LIT_OBJECT: bad cast"); return static_cast<CPI3D_lit_object>(f); }
inline CPI3D_dummy I3DCAST_CDUMMY(CPI3D_frame f){ if(f && f->GetType()!=FRAME_DUMMY) throw C_except("I3DCAST_DUMMY: bad cast"); return static_cast<CPI3D_dummy>(f); }
inline CPI3D_joint I3DCAST_CJOINT(CPI3D_frame f){ if(f && f->GetType()!=FRAME_JOINT) throw C_except("I3DCAST_JOINT: bad cast"); return static_cast<CPI3D_joint>(f); }
inline CPI3D_user I3DCAST_CUSER(CPI3D_frame f){ if(f && f->GetType()!=FRAME_USER) throw C_except("I3DCAST_USER: bad cast"); return static_cast<CPI3D_user>(f); }
inline CPI3D_volume I3DCAST_CVOLUME(CPI3D_frame f){ if(f && f->GetType()!=FRAME_VOLUME) throw C_except("I3DCAST_VOLUME: bad cast"); return static_cast<CPI3D_volume>(f); }
inline CPI3D_sector I3DCAST_CSECTOR(CPI3D_frame f){ if(f && f->GetType()!=FRAME_SECTOR) throw C_except("I3DCAST_SECTOR: bad cast"); return static_cast<CPI3D_sector>(f); }
inline CPI3D_model I3DCAST_CMODEL(CPI3D_frame f){ if(f && f->GetType()!=FRAME_MODEL) throw C_except("I3DCAST_MODEL: bad cast"); return static_cast<CPI3D_model>(f); }
inline CPI3D_visual I3DCAST_CVISUAL(CPI3D_frame f){ if(f && f->GetType()!=FRAME_VISUAL) throw C_except("I3DCAST_VISUAL: bad cast"); return static_cast<CPI3D_visual>(f); }
inline CPI3D_occluder I3DCAST_COCCLUDER(CPI3D_frame f){ if(f && f->GetType()!=FRAME_OCCLUDER) throw C_except("I3DCAST_OCCLUDER: bad cast"); return static_cast<CPI3D_occluder>(f); }

#else

inline PI3D_light I3DCAST_LIGHT(PI3D_frame f){ return static_cast<PI3D_light>(f); }
inline PI3D_sound I3DCAST_SOUND(PI3D_frame f){ return static_cast<PI3D_sound>(f); }
inline PI3D_camera I3DCAST_CAMERA(PI3D_frame f){ return static_cast<PI3D_camera>(f); }
inline PI3D_object I3DCAST_OBJECT(PI3D_frame f){ return static_cast<PI3D_object>(f); }
inline PI3D_lit_object I3DCAST_LIT_OBJECT(PI3D_frame f){ return static_cast<PI3D_lit_object>(f); }
inline PI3D_dummy I3DCAST_DUMMY(PI3D_frame f){ return static_cast<PI3D_dummy>(f); }
inline PI3D_joint I3DCAST_JOINT(PI3D_frame f){ return static_cast<PI3D_joint>(f); }
inline PI3D_user I3DCAST_USER(PI3D_frame f){ return static_cast<PI3D_user>(f); }
inline PI3D_volume I3DCAST_VOLUME(PI3D_frame f){ return static_cast<PI3D_volume>(f); }
inline PI3D_sector I3DCAST_SECTOR(PI3D_frame f){ return static_cast<PI3D_sector>(f); }
inline PI3D_model I3DCAST_MODEL(PI3D_frame f){ return static_cast<PI3D_model>(f); }
inline PI3D_visual I3DCAST_VISUAL(PI3D_frame f){ return static_cast<PI3D_visual>(f); }
inline PI3D_occluder I3DCAST_OCCLUDER(PI3D_frame f){ return static_cast<PI3D_occluder>(f); }

inline CPI3D_light I3DCAST_CLIGHT(CPI3D_frame f){ return static_cast<CPI3D_light>(f); }
inline CPI3D_sound I3DCAST_CSOUND(CPI3D_frame f){ return static_cast<CPI3D_sound>(f); }
inline CPI3D_camera I3DCAST_CCAMERA(CPI3D_frame f){ return static_cast<CPI3D_camera>(f); }
inline CPI3D_object I3DCAST_COBJECT(CPI3D_frame f){ return static_cast<CPI3D_object>(f); }
inline CPI3D_lit_object I3DCAST_CLIT_OBJECT(CPI3D_frame f){ return static_cast<CPI3D_lit_object>(f); }
inline CPI3D_dummy I3DCAST_CDUMMY(CPI3D_frame f){ return static_cast<CPI3D_dummy>(f); }
inline CPI3D_joint I3DCAST_CJOINT(CPI3D_frame f){ return static_cast<CPI3D_joint>(f); }
inline CPI3D_user I3DCAST_CUSER(CPI3D_frame f){ return static_cast<CPI3D_user>(f); }
inline CPI3D_volume I3DCAST_CVOLUME(CPI3D_frame f){ return static_cast<CPI3D_volume>(f); }
inline CPI3D_sector I3DCAST_CSECTOR(CPI3D_frame f){ return static_cast<CPI3D_sector>(f); }
inline CPI3D_model I3DCAST_CMODEL(CPI3D_frame f){ return static_cast<CPI3D_model>(f); }
inline CPI3D_visual I3DCAST_CVISUAL(CPI3D_frame f){ return static_cast<CPI3D_visual>(f); }
inline CPI3D_occluder I3DCAST_COCCLUDER(CPI3D_frame f){ return static_cast<CPI3D_occluder>(f); }

#endif

#endif

//----------------------------

I3D_RESULT I3DAPI I3DCreate(PI3D_driver *drvp, CPI3DINIT);

//----------------------------


#endif
