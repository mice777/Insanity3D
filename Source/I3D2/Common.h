#ifndef __COMMON_H
#define __COMMON_H

//----------------------------

//#define USE_EAX
//#define USE_D3DX

//#define USE_PREFETCH

//----------------------------

#define FLOAT_ONE_BITMASK 0x3f800000

const float R_128 = 1.0f / 128.0f;  //reciprocal of 128.0f
const float R_255 = 1.0f / 255.0f;  //reciprocal of 255.0f

//----------------------------

#ifdef USE_EAX
                              //override some EAX includes
#define _LPCWAVEFORMATEX_DEFINED
#define __DSOUND_INCLUDED__
//#define LPWAVEFORMATEX void*
#define DIRECTSOUND_VERSION 0x800
#define LPDIRECTSOUND void*
#define LPDIRECTSOUND8 void*
//#define _WAVEFORMATEX_
#include <eax.h>
#endif   //USE_EAX


#ifdef _MSC_VER
namespace std{}
using namespace std;
#endif


#ifdef _DEBUG
void D3D_Fatal(const char *text, dword hr, const char *file, dword line);
#define CHECK_D3D_RESULT(text, hr) if(FAILED(hr)) D3D_Fatal(text, hr, __FILE__, __LINE__);
#else
#define CHECK_D3D_RESULT(text, hr)
#endif

#ifdef GL
#ifdef _DEBUG
void GL_Fatal(const char *text, dword code, const char *file, dword line);
#define CHECK_GL_RESULT(text) { dword err = glGetError(); if(err!=GL_NO_ERROR) GL_Fatal(text, err, __FILE__, __LINE__); }
#else
#define CHECK_GL_RESULT(text)
#endif
#endif
//----------------------------

enum I3D_BLENDMODE{
   I3DBLEND_OPAQUE      = 0,  //ONE:ZERO
   I3DBLEND_ALPHABLEND  = 1,  //SRCALPHA:INVSRCALPHA
   I3DBLEND_ADD         = 2,  //ONE:ONE
   I3DBLEND_ADDALPHA    = 3,  //SRCALPHA:ONE
   //I3DBLEND_MODULATE    = 4,  //ZERO:SRCCOLOR
   I3DBLEND_INVMODULATE = 5,  //ZERO:INVSRCCOLOR
   I3DBLEND_MODULATE2X  = 6,  //DESTCOLOR:SRCCOLOR
   //I3DBLEND_INVMODULATE2X = 7,//DESTCOLOR:INVSRCCOLOR
   //I3DBLEND_TEST,             //experiments
};

//----------------------------

#ifdef _DEBUG
#define DEBUG_LOG(n) { OutputDebugString(n); OutputDebugString("\n"); }
#else
#define DEBUG_LOG(n)
#endif

//----------------------------
                              //sorting bits for render primitives
                              //bits 29 - 31 - alpha blending type (signed)
#define PRIM_SORT_OPAQUE         0x80000000
#define PRIM_SORT_CKEY_ALPHA     0xa0000000
#define PRIM_SORT_ALPHA_ZWRITE   0xc0000000
#define PRIM_SORT_ALPHA_NOZWRITE 0xe0000000
#define PRIM_SORT_ALPHA_NOZ      0x20000000
                              //bits 15-28 for - material sorting ID
#define PRIM_SORT_MAT_MASK    0x1fffffff
#define PRIM_SORT_MAT_SHIFT   0
                              //bits 0-14 - distance (not combined with material - alpha only)
#define PRIM_SORT_DIST_MASK   0x1fffffff
#define PRIM_SORT_DIST_SHIFT  0

//----------------------------

enum{                         
   I3DCLIP_LEFT = 1,
   I3DCLIP_RIGHT = 2,
   I3DCLIP_BOTTOM = 4,
   I3DCLIP_TOP = 8,
   I3DCLIP_SIDES = I3DCLIP_LEFT | I3DCLIP_TOP | I3DCLIP_RIGHT | I3DCLIP_BOTTOM,
   I3DCLIP_BACK = 0x10,
   I3DCLIP_FRONT = 0x20,
   I3DCLIP_ALL = I3DCLIP_SIDES | I3DCLIP_FRONT | I3DCLIP_BACK
};

//----------------------------

#ifndef NDEBUG
extern int debug_count[4];
#endif

//----------------------------

#if defined _MSC_VER && 1

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


                              //fast float cheats
#define CHECK_ZERO_GREATER(f) (!((*(dword*)&(f)) & 0x80000000))
#define CHECK_ZERO_LESS(f) ((*(dword*)&(f)) & 0x80000000)
#define CHECK_ZERO(f) (!*(dword*)&(f))
#define CHECK_SAME_SIGNS(f1, f2) (!(((*(dword*)&(f1)) ^ (*(dword*)&(f2))) & 0x80000000))
#define FLOAT_BITMASK(f) (*(int*)&(f))

                              //conversion of triangle index into next/previous triangle index
extern const dword next_tri_indx[3];   //this array is: 1, 2, 0
extern const dword prev_tri_indx[3];   //this array is: 2, 0, 1

//----------------------------
                              //integer-based 3D C_vector
struct S_vector_i{
#pragma warning(push)
#pragma warning(disable:4201)
   union{
      struct{
         int x, y, z;
      };
      int _v[2];
   };
#pragma warning(pop)
   S_vector_i(){}
   S_vector_i(int x1, int y1, int z1): x(x1), y(y1), z(z1){}

   inline int &operator [](int i){ return _v[i]; }
   inline int operator [](int i) const{ return _v[i]; }
   inline bool operator ==(const S_vector_i &v) const{ return(x==v.x && y==v.y && z==v.z); }
   inline bool operator !=(const S_vector_i &v) const{ return(x!=v.x || y!=v.y || z!=v.z); }

   inline void Minimal(const S_vector_i &v){
      x = (x<v.x) ? x : v.x;
      y = (y<v.y) ? y : v.y;
      z = (z<v.z) ? z : v.z;
   }
   inline void Maximal(const S_vector_i &v){
      x = (x>v.x) ? x : v.x;
      y = (y>v.y) ? y : v.y;
      z = (z>v.z) ? z : v.z;
   }

//----------------------------
// Convert input float C_vector to integers by using 'floor' value
   void Floor(const S_vector &v){
      x = FloatToInt(v.x - .5f);
      y = FloatToInt(v.y - .5f);
      z = FloatToInt(v.z - .5f);
   }
};

//----------------------------
#ifdef USE_PREFETCH
                              //prefetching function
typedef void t_Prefetch1(const void *ptr);
typedef void t_Prefetch2(const void*, const void*);
typedef void t_Prefetch3(const void*, const void*, const void*);
typedef void t_Prefetch64(const void*);
extern t_Prefetch1 *Prefetch1;
extern t_Prefetch2 *Prefetch2ptr;
extern t_Prefetch3 *Prefetch3ptr;
extern t_Prefetch64 *Prefetch64Bytes;

#endif//USE_PREFETCH
//----------------------------

enum E_VIEW_FRUSTUM_SIDE{     //side indicies
   VF_LEFT,
   VF_TOP,
   VF_RIGHT,
   VF_BOTTOM,
   VF_BACK,
   VF_FRONT
};

//----------------------------

#define MAX_FRUSTUM_CLIP_PLANES 32

struct S_view_frustum_base{
                              //planes making up the frustum
   S_plane clip_planes[MAX_FRUSTUM_CLIP_PLANES];
                              //number of clipping planes
   int num_clip_planes;
};

struct S_view_frustum: public S_view_frustum_base{
                              //following values are used only with perspective-projected frustums:

                              //points lying on the lines as seen from camera (at distance 1 from camera position)
                              // order doesn't matter
   S_vector frustum_pts[MAX_FRUSTUM_CLIP_PLANES];
                              //viewing position, from which frustum is constructed
   S_vector view_pos;
};

//----------------------------

/*
struct S_expanded_view_frustum{
   float nx[MAX_FRUSTUM_CLIP_PLANES];
   float ny[MAX_FRUSTUM_CLIP_PLANES];
   float nz[MAX_FRUSTUM_CLIP_PLANES];
   float d[MAX_FRUSTUM_CLIP_PLANES];
   int num_clip_planes;

//----------------------------
// Make expanded frustum from normal one.
   void Make(const S_view_frustum&);
};
*/

//----------------------------

enum{
   E_PREP_NO_VISUALS = 1,     //no visuals in this branch are being added to render list
   E_PREP_HR_NO_CLIP = 2,     //hierarchy is 100% on screen - no clipping, no bbox testing
   E_PREP_TEST_OCCLUDERS = 4, //test against occluders
   E_PREP_ADD_SHD_CASTERS = 8,//add shadow casters list
   E_PREP_SET_HR_LIGHT_DIRTY = 0x10,//hierarchically set FRMFLAGS_HR_LIGHT_RESET flag on processed children
};

                              //mode how we preprocess, add primitives and draw primitives
enum E_RENDERVIEW_MODE{
   RV_NORMAL,                 //normal mode
   RV_MIRROR,                 //from behind mirror
#ifndef GL
   RV_SHADOW_CASTER,          //shadow caster from light's position
#endif
   RV_CAM_TO_TEXTURE,         //CamView visual rendering into texture
};

//#define RP_FLAG_CLIP          1
#define RP_FLAG_FOG_VALID     2
#define RP_FLAG_DIFFUSE_VALID 4
#define RP_UV_DETAIL    8
#define RP_UV_EMBM      0x10
#define RP_UV_ENV       0x20

struct S_render_primitive{
   PI3D_visual vis;
   dword sort_value;
   mutable dword flags;
   I3D_BLENDMODE blend_mode;
   PI3D_sector sector;       //current sector
   PI3D_scene scene;         //current scene
   dword user1;
   int curr_auto_lod;
   byte alpha;                //visual alpha, by which this visual is to be rendered

   S_render_primitive():
      curr_auto_lod(0),
      alpha(0xff),
      flags(0)
   {}
   inline S_render_primitive(int auto_lod, byte al, PI3D_visual v, const struct S_preprocess_context &pc,
      I3D_BLENDMODE bm = I3DBLEND_OPAQUE);
};

//----------------------------
                              //single mirror structure
struct S_mirror_data: public C_unknown{
   S_render_primitive rp;
   S_mirror_data(int al, byte alpha, PI3D_visual v, const struct S_preprocess_context &pc, I3D_BLENDMODE bm = I3DBLEND_OPAQUE):
      rp(al, alpha, v, pc, bm){}
};

typedef C_vector<C_smart_ptr<S_mirror_data> > t_mirror_visuals;
typedef map<dword, t_mirror_visuals> t_mirror_data;

//----------------------------

struct S_preprocess_context{

                              //counts:
   dword opaque;              //opaque/colorkey
   dword ckey_alpha;          //1-bit alpha, z-writes, alpharef
   dword alpha_zwrite;        //alpha with z-writes
   dword alpha_nozwrite;      //alpha, no z-writes
   dword alpha_noz;           //alpha, no z-buffering

                              //read-only values
   //bool clip;
   PI3D_scene scene;
   C_vector<S_render_primitive> prim_list;
   C_sort_list<dword> prim_sort_list;
   PI3D_sector curr_sector;
   S_vector viewer_pos;
#ifndef GL
                              //frames which may cast shadows
   C_vector<PI3D_model> shadow_casters;
                              //frames which may potentionally receive shadow
   C_vector<PI3D_visual> shadow_receivers;
   float shadow_opacity;      //opacity of currently renderes shadow caster
#endif

                              //data of mirrors rendered in current frame
   t_mirror_data mirror_data;

   dword render_flags;

   float LOD_factor;          //1.0f is default under FOV=65 deg and ideal conditions

   float force_alpha;         //alha value to explicitly set on all visuals (used with hiding); ignored if 1.0

                              //list of occluders, built by PreProcess,
                              // used in PreprocessChildren
   C_vector<class I3D_occluder*> occ_list;

                              //view frustum volume
   S_view_frustum view_frustum;
   //S_expanded_view_frustum exp_frustum;
   I3D_bsphere vf_sphere;
   E_RENDERVIEW_MODE mode;

//----------------------------
// Sort primitives by distance.
   void SortByDistance();

//----------------------------
// Sort primitives by sort value.
   void Sort();

   void Reset();

   inline void Reserve(int num_items){
      prim_list.reserve(num_items);
#ifndef GL
      shadow_casters.reserve(32);
      shadow_receivers.reserve(num_items/2);
#endif
   }

   S_preprocess_context(PI3D_scene s):
      scene(s),
      mode(RV_NORMAL)
   {
   }
   void Prepare(const S_vector &vp){
      viewer_pos = vp;
      Reset();
   }
   S_preprocess_context(const S_preprocess_context &cs){ operator =(cs); }
   void operator =(const S_preprocess_context&);
};

//----------------------------

inline S_render_primitive::S_render_primitive(int auto_lod, byte al, PI3D_visual v, const struct S_preprocess_context &pc, I3D_BLENDMODE bm):
   curr_auto_lod(auto_lod),
   alpha(al),
   flags(0),
   vis(v),
   sector(pc.curr_sector),
   scene(pc.scene),
   blend_mode(bm)
{
   //if(pc.clip)
      //flags |= RP_FLAG_CLIP;
}

//----------------------------

#if defined _MSC_VER && 0
bool SphereCollide(const I3D_bsphere &vf_bs, const I3D_bsphere &bs2);
#else
inline bool SphereCollide(const I3D_bsphere &vf_bs, const I3D_bsphere &bs2){
   S_vector dir = vf_bs.pos - bs2.pos;
   float dist_2 = dir.Dot(dir);
   float radius_sum_2 = vf_bs.radius + bs2.radius; radius_sum_2 *= radius_sum_2;
   return (dist_2 < radius_sum_2);
}
#endif

//----------------------------
// Get index of the most significant bit in the mask,
// or -1 if such bit doesn't exist.
inline int FindLastBit(dword val){

#ifdef _MSC_VER
   dword rtn;
   __asm{
      mov eax, val
      bsr eax, eax
      jnz ok
      mov eax, -1
ok:
      mov rtn, eax
   }
   return rtn;
#else
   int base = 0;
   if(val&0xffff0000){
      base = 16;
      val >>= 16;
   }
   if(val&0x0000ff00){
      base += 8;
      val >>= 8;
   }
   if(val&0x000000f0){
      base += 4;
      val >>= 4;
   }
   static const int lut[] = {-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};
   return base + lut[val];
#endif
}

//----------------------------
// Rotate 64-bit value to left, wrapping bits around.
inline __int64 __cdecl Rotl(const __int64 &i, int count){

   __asm{
      mov ecx, count
      and ecx, 0x3f
      mov esi, i
      mov edx, [esi+4]
      mov eax, [esi+0]
      mov ebx, eax
      shld eax, edx, cl
      shld edx, ebx, cl
      cmp ecx, 0x20
      jb l1
      xchg eax, edx
   l1:
      mov [esi+0], eax
      mov [esi+4], edx
   }
   return i;
}

//----------------------------

dword CountBits(const S_pixelformat &fmt);

//----------------------------

typedef PI3D_visual t_CreateVisual(PI3D_driver drv);

                              //plugin registration
struct S_visual_property{
   I3D_PROPERTYTYPE prop_type;
   const char *prop_name;
   const char *help;
};

struct S_visual_plugin_entry{
   t_CreateVisual *create_proc;
   const char *friendly_name;
   dword visual_type;
   const S_visual_property *props;
   //dword num_props;
};

extern const S_visual_plugin_entry visual_plugins[];
extern const int num_visual_plugins;

//----------------------------
                              //render target class - variable number of RTT surfaces with single depth buffer
template<int NUM = 1, class T = IDirect3DSurface9>
class C_render_target{
public:
                              //render target and associated z-buffer
   C_smart_ptr<T> rt[NUM];
   C_smart_ptr<IDirect3DSurface9> zb;

   C_render_target(){}
   C_render_target(T *_rt, IDirect3DSurface9 *zb1): zb(zb1){
      rt[0] = _rt;
   }
   C_render_target(const C_render_target &rt){ operator =(rt); }
   C_render_target &operator =(const C_render_target &crt){
      for(int i=NUM; i--; )
         rt[i] = crt.rt[i];
      zb = crt.zb;
      return *this;
   }

//----------------------------
// Close - release all resources.
   void Close(){
      for(int i=NUM; i--; )
         rt[i] = NULL;
      zb = NULL;
   }

//----------------------------
// Overloaded operators for directly accessing first rtt (at index 0).
   inline operator T *(){ return rt[0]; }
   inline operator const T *() const{ return rt[0]; }
   inline T &operator *() const{ return *rt[0]; }
   inline T *operator ->(){ return rt[0]; }
   inline const T *operator ->() const{ return rt[0]; }
};

//----------------------------
// Check if sphere (in world coords) is inside of viewing frustum. If not, return also
// clip flag.
#ifdef _MSC_VER_

bool SphereInVF(const S_view_frustum_base&, const I3D_bsphere&, bool &clip);

#else //_MSC_VER

bool SphereInVF(const S_view_frustum_base&, const I3D_bsphere&, bool &clip);

#endif//!_MSC_VER

//----------------------------
// Check if all of input vertices are inside of viewing frustum.
bool AreAllVerticesInVF(const S_view_frustum_base&, const S_vector *verts, dword num_verts);

//----------------------------
// Check if any of input vertices is inside of viewing frustum.
bool IsAnyVertexInVF(const S_view_frustum&, const S_vector *verts, dword num_verts);

//----------------------------
// Compute contour of axis-aligned bounding-box, tranformed by 'tm',
// as seen from 'look'.
// If 'orthogonal' is false, the 'look' is a position point, otherwise it is direction C_vector.
// On return, 'contour_points' contain contour points in world coordinates,
// in clockwise order, and 'num_cpts' contains number of points.
// If number of points is 0, look_pos is inside of bound box, otherwise number of points
// is eigher 4 (one side visible) or 6 (two or three sides visible).
void ComputeContour(const I3D_bbox &bb_in, const S_matrix &tm, const S_vector &look,
   S_vector contour_points[6], dword &num_cpts, bool orthogonal = false);

//----------------------------
// Check if contour intersects with frustum. The contour doesn't need to be planar, but it must be seen as convex
// from provided view_pos.
// Parameters:
//    planes, num_planes ... view frustum, as seen from the view_pos
//    hull_pts ... points on sides of the frustum (but not view_pos)
//    verts, num_verts ... contour vertices in CW order
//    view_pos ... position of viewer (or look direction if orthogonal projection is used)
//    ortho_proj ... true if orthogonal projection is used, instead of perspective
//    inv_cull ... true if contour vertices are specified in CCW order
bool CheckFrustumIntersection(const S_plane *planes, dword num_planes, const S_vector *hull_pts,
   const S_vector *verts, dword num_verts, const S_vector &view_pos, bool ortho_proj = false,
   bool inv_cull = false);

//----------------------------
// Transfrom array of vertices.
// Note: source and destination pointers cannot be the same!
void TransformVertexArray(const S_vector *in_verts, dword in_stride, dword num_verts,
   S_vector *verts_out, dword out_stride, const S_matrix &m);

//----------------------------
                              //re-use reserved D3D fvf bit for texture space coordinates
#define D3DFVF_TEXTURE_SPACE 0x2000

//----------------------------
// Compute size of vertex from given FVF code.
dword GetSizeOfVertex(dword fvf_flags);

//----------------------------
int GetVertexNormalOffset(dword fvf_flags);
int GetVertexUvOffset(dword fvf_flags);

//----------------------------

//----------------------------
// Find specified 32-bit pointer in array of pointers,
// return index of pointer, or -1 if pointer not found
//----------------------------
#ifdef _MSC_VER

#pragma warning(disable:4035)
inline int FindPointerInArray(const void * const*vp, int array_len, const void *what){
   __asm{
      push ecx
      mov edi, vp
      mov ecx, array_len
      mov eax, what
      mov edx, ecx
      repne scasd
      jz ok
      mov edx, ecx
   ok:
      sub edx, ecx
      lea eax, [edx-1]
      pop ecx
   }
}
#pragma warning(default:4035)

#else

inline int FindPointerInArray(const void * const*vp, int array_len, const void *what){
   for(int i(array_len); i--; ) if(vp[i]==what) break;
   return i;
}

#endif

//----------------------------
                              //enumeration - internal
#define ENUMF_MODEL_ORIG   0x80000000  //use model's original name

//----------------------------
// Template class for locking and unlocking D3D vertex buffer.
template<class T>
class D3D_lock{
   T *ptr;                    //NULL if buffer not locked
   IDirect3DVertexBuffer9 *vb;
   dword lock_index, num_items;
   dword size_of_item;
   dword flags;
public:
   inline operator T *(){ return ptr; }
   inline operator T &(){ return *ptr; }
   inline T &operator [](int i){ return ptr[i]; }

//----------------------------
// Initialize lock - save values for later lock.
   inline D3D_lock(IDirect3DVertexBuffer9 *vbi, dword li, dword nv, dword sz, dword fl):
      ptr(NULL), vb(vbi), lock_index(li), num_items(nv), size_of_item(sz), flags(fl)
   {
   }

   inline ~D3D_lock(){ Unlock(); }

//----------------------------
// Lock, if not yet, using saved values.
   inline bool AssureLock(){
      if(!ptr){
         HRESULT hr;
         hr = vb->Lock(
               //0,       //offset (in bytes)
               lock_index * size_of_item,
               //0,             //size (in bytes)
               num_items * size_of_item,
               (void**)&ptr,
               flags);
         CHECK_D3D_RESULT("Lock", hr);
         /*
         if(SUCCEEDED(hr)){
            (byte*&)ptr += lock_index * size_of_data;
         }
         */
      }
      return (ptr != NULL);
   }

//----------------------------
// Unlock (possibly) locked buffer.
   inline void Unlock(){
      if(ptr){
         HRESULT hr;
         hr = vb->Unlock();
         CHECK_D3D_RESULT("Unlock", hr);
         ptr = NULL;
      }
   }
};

//----------------------------

struct I3D_face{
   dword num_points;
   word *points;
   I3D_face(): num_points(0), points(NULL){}
   I3D_face(dword num): num_points(num), points(num ? new word[num] : NULL){}
   I3D_face(const I3D_face &f): points(NULL){ operator =(f); }
   ~I3D_face(){ delete[] points; }
   I3D_face &operator =(const I3D_face &f){
      delete[] points;
      points = new word[num_points = f.num_points];
      memcpy(points, f.points, num_points*sizeof(word));
      return *this;
   }
   void Reserve(dword num){
      if(num_points!=num){
         delete[] points;
         points = new word[num_points = num];
      }
   }
   inline word &operator [](int i){ return points[i]; }
   inline const word &operator [](int i) const{ return points[i]; }
};

//----------------------------

struct I3D_cylinder{
   S_vector pos;              //position of the base
   S_vector dir;              //direction and length of main axis
   float radius;              //radius
};

//----------------------------// collisions //----------------------------//

inline bool pickBSphereTest(const I3D_bsphere &bs1, const S_vector &from,
   const S_vector &normalized_dir, float closest_hit){

                              //check closest point on line
   float u = (bs1.pos-from).Dot(normalized_dir);
   if(u < - bs1.radius || u > (closest_hit+bs1.radius))
      return false;

                              //determine closest point to line
   S_vector point_on_line = from + normalized_dir * u;
   S_vector dir = point_on_line - bs1.pos;
   float dist_2 = dir.Dot(dir);
   if(dist_2 >= (bs1.radius*bs1.radius))
      return false;

   return true;
}

//----------------------------

inline bool FastTest_L_S(I3D_collision_data &cd, const S_vector &sphere_pos, float radius, float radius_2){

   const S_vector &norm_dir = cd.GetNormalizedDir();
                              //get closest point of sphere to line
   float u = norm_dir.Dot(sphere_pos - cd.from);
                              //check out-of-range
   if(u < -radius)
      return false;
   if(u > cd.GetHitDistance()+radius)
      return false;

                              //get point on line
   S_vector point_on_line = cd.from + norm_dir * u;
   S_vector dir_to_point = point_on_line - sphere_pos;
   float dist_2 = dir_to_point.Dot(dir_to_point);
   return (FLOAT_BITMASK(dist_2) < FLOAT_BITMASK(radius_2));
}

//----------------------------
// Check if point is in triangle. Triangle is specified by 3 points and its normal (non-normalized works).
// The point in question doesn't need to lie on the triangle, algorithm just checks if the point is
// out of edge planes (edge plane specified as perpendicular to edge and triangle's normal).
inline bool IsPointInTriangle(const S_vector &pt, const S_vector &t0, const S_vector &t1, const S_vector &t2,
   const S_vector &face_normal){

   S_vector n;
   float d;

   float x, y, z;
   {
      x = t0.x - t1.x;
      y = t0.y - t1.y;
      z = t0.z - t1.z;
      n.x = face_normal.y * z - face_normal.z * y;
      n.y = face_normal.z * x - face_normal.x * z;
      n.z = face_normal.x * y - face_normal.y * x;
      d = t0.Dot(n) - pt.Dot(n);
      if(CHECK_ZERO_LESS(d))
         return false;
   }

   {
      x = t1.x - t2.x;
      y = t1.y - t2.y;
      z = t1.z - t2.z;
      n.x = face_normal.y * z - face_normal.z * y;
      n.y = face_normal.z * x - face_normal.x * z;
      n.z = face_normal.x * y - face_normal.y * x;
      d = t1.Dot(n) - pt.Dot(n);
      if(CHECK_ZERO_LESS(d))
         return false;
   }
   {
      x = t2.x - t0.x;
      y = t2.y - t0.y;
      z = t2.z - t0.z;
      n.x = face_normal.y * z - face_normal.z * y;
      n.y = face_normal.z * x - face_normal.x * z;
      n.z = face_normal.x * y - face_normal.y * x;
      d = t2.Dot(n) - pt.Dot(n);
      if(CHECK_ZERO_LESS(d))
         return false;
   }
   return true;
}

//----------------------------

inline bool IsPointInTriangle(const S_vector &pt, const S_vector &t0, const S_vector &t1, const S_vector &t2,
   const S_vector &face_normal, float radius){

   S_vector n;
   float d;

   {
      S_vector v(t0-t1);
      n.x = face_normal.y * v.z - face_normal.z * v.y;
      n.y = face_normal.z * v.x - face_normal.x * v.z;
      n.z = face_normal.x * v.y - face_normal.y * v.x;
      d = t0.Dot(n) - pt.Dot(n);
      if(d < -radius)
         return false;
   }

   {
      S_vector v(t1-t2);
      n.x = face_normal.y * v.z - face_normal.z * v.y;
      n.y = face_normal.z * v.x - face_normal.x * v.z;
      n.z = face_normal.x * v.y - face_normal.y * v.x;
      d = t1.Dot(n) - pt.Dot(n);
      if(d < -radius)
         return false;
   }
   {
      S_vector v(t2-t0);
      n.x = face_normal.y * v.z - face_normal.z * v.y;
      n.y = face_normal.z * v.x - face_normal.x * v.z;
      n.z = face_normal.x * v.y - face_normal.y * v.x;
      d = t2.Dot(n) - pt.Dot(n);
      if(d < -radius)
         return false;
   }
   return true;
}

//----------------------------

struct S_sound_env_properties{
                              //EAX properties:
   float room_level_lf;       //room effect level - low  freqencies, ratio (0.0 ... 1.0)
   float room_level_hf;       //room effect level - high freqencies, ratio (0.0 ... 1.0)
   int decay_time;            //reverberation decay time, in msec
   float decay_hf_ratio;      //HF to LF decay time, ratio (0.0 ... 1.0)
   float reflections;         //early reflections level - relative to room effect, ratio (0.0 ... 1.0)
   int reflections_delay;     //initial reflection delay time, in msec
   float reverb;              //late reverberation level - relative to room effect, (0.0 ... 1.0)
   int reverb_delay;          //late reverberation delay time relative to initial reflection, in msec
   float env_diffusion;       //environment diffusion, ratio (0.0 ... 1.0)

                              //other properties:
   //int switch_fade_time;      //time of volume fade between swithing
   C_str name;
   float emulation_volume;    //volume of sector if EAX is not in use

//----------------------------
// Setup default values
//----------------------------
   void SetupDefaults();

//----------------------------
// Compute blend between 2 input data. Parameter 'blend' is in range 0.0f (full ssp1)
// to 1.0f (full ssp2).
//----------------------------
   void MakeBlend(const S_sound_env_properties &ssp1, const S_sound_env_properties &ssp2,
      float blend, bool use_eax = true);

   S_sound_env_properties(){
      SetupDefaults();
   }
};

//----------------------------
// Convert I3D vertex-component flags into D3D flexible vertex format flags.
// The returned value is -1 of the combination is invalid.
inline dword ConvertFlags(dword vc_flags){
                              //deterine fvf
   dword fvf = 0;
   if(vc_flags&I3DVC_XYZ){
      if(vc_flags&I3DVC_ALPHA){
         if(vc_flags&I3DVC_BETA_MASK)
            return (dword)-1;
         fvf |= D3DFVF_XYZB1;
      }else{
         switch((vc_flags&I3DVC_BETA_MASK)>>I3DVC_BETA_SHIFT){
         case 1: fvf |= D3DFVF_XYZB1; break;
         case 2: fvf |= D3DFVF_XYZB2; break;
         case 3: fvf |= D3DFVF_XYZB3; break;
         case 4: fvf |= D3DFVF_XYZB4; break;
         case 5: fvf |= D3DFVF_XYZB5; break;
         default: fvf |= D3DFVF_XYZ;
         }
      }
   }
   if(vc_flags&I3DVC_XYZRHW){
      if(vc_flags&(I3DVC_ALPHA|I3DVC_BETA_MASK))
         return (dword)-1;
      fvf |= D3DFVF_XYZRHW;
   }
   if(vc_flags&D3DFVF_NORMAL) fvf |= D3DFVF_NORMAL;
   if(vc_flags&D3DFVF_DIFFUSE) fvf |= D3DFVF_DIFFUSE;
   if(vc_flags&D3DFVF_SPECULAR) fvf |= D3DFVF_SPECULAR;
   fvf |= (((vc_flags&I3DVC_TEXCOUNT_MASK) >> I3DVC_TEXCOUNT_SHIFT) << D3DFVF_TEXCOUNT_SHIFT) & D3DFVF_TEXCOUNT_MASK;
   return fvf;
}

//----------------------------
// Get offset of particular vertex component in vertex, given specified FVF code.
// Parameters:
//    fvflags ... FVF flags of vertex
//    fvf_component ... FVF component flag which is queried (specify only one flag)
// Return code:
//    offset of specified component from beginning of index, or -1 if component not found
int GetVertexComponentOffset(dword fvflags, dword fvf_component);

//----------------------------
// Check if vertex is in a bounding-box,
// also compute weight value, based on distance
// in bbox's Z coord
// if test succeeded, weight is in range 0.0f ... 1.0f
bool IsPointInBBox(const S_vector &v, const I3D_bbox &bbox, const S_matrix &m_bb_inv, float &weight);

//----------------------------
// Simplified version of above, where only XZ axis is tested.
bool IsPointInBBoxXY(const S_vector &v, const I3D_bbox &bbox, const S_matrix &m_bb_inv, float &weight);

//----------------------------

void LightUpdateColor(PI3D_driver drv, const S_vector &color, float power, S_vector *col, dword *dw_color, bool surf_match);

//----------------------------

class I3D_texel_collision{
public:
   I3D_text_coor tex;         //uv coordinates being collided
   bool loc_pos;              //when true, point positions are in local coordinates
   const S_vector *v[3];      //points of triangle
   dword face_index;           //face index on which collision occured
   S_vector point_on_plane;   //point on face's plane
   CPI3D_material mat;        //material of face

//----------------------------
// Compute uv coordinates on triangle, using provided uv coordinates on triangle's vertices.
// Parameters:
//    uv0, uv1, uv2 ... uv coordinates on corners of triangle
// Return value:
//    true if computation succeeded
// Function works with inputs set in 'v[3]', 'point_on_plane', and stores result into variable 'tex'.
   virtual bool ComputeUVFromPoint(const I3D_text_coor &uv0, const I3D_text_coor &uv1, const I3D_text_coor &uv2);
};

//----------------------------
// Vertex for rendering rectangle.
struct S_vertex_rectangle{
   float x, y;
};

//----------------------------
                              //S, T, SxT transformation matrix (texture-space coordinates)
struct S_texture_space{
   S_vector s;
   S_vector t;
   S_vector sxt;
};

//----------------------------
// Vertex for rendering flare
struct S_vertex_particle{
   float x, y;
   float index;               //special index value (holdiong rectainge index * 4)
};

                              //defined to be floor((96 - 8) / 4)
                              // where 96 is max # of vertex shader registers, and 8 is base register used for
                              // particle rendering matrix
#define MAX_PARTICLE_RECTANGLES 22

//----------------------------

struct S_vertex_element: public D3DVERTEXELEMENT9{
   S_vertex_element(){}
   S_vertex_element(byte o, D3DDECLTYPE t, D3DDECLUSAGE u, byte ui = 0, byte s = 0, D3DDECLMETHOD m = D3DDECLMETHOD_DEFAULT){
      Stream = s;
      Offset = o;
      Type = (byte)t;
      Method = (byte)m;
      Usage = (byte)u;
      UsageIndex = ui;
   }
};

extern D3DVERTEXELEMENT9 D3DVERTEXELEMENT9_END;

//----------------------------

enum E_PROFILE_BLOCK{         //block identifiers
   PROF_RENDER,
   PROF_CLEAR,
   PROF_MIRROR,
   PROF_SHADOWS,
   PROF_PREPROCESS,
   PROF_DRAW,
   PROF_COLLISIONS,
   PROF_ANIMS,
   PROF_PARTICLE,
   PROF_PROC_TEXTURE,
   PROF_TEST,
};

#ifdef _DEBUG
extern const char block_profile_names[];

#define PROFILE(drv, n) C_block_profiler_entry prof(n, drv->block_profiler)

#else

#define PROFILE(drv, n)

#endif

//----------------------------
// Helper class, using array of bits as boolean values.
class C_bitfield{
   byte *mem;
   dword num_items;
   dword alloc_size;
public:
   C_bitfield() :
      mem(NULL), 
      num_items(0),
      alloc_size(0)
   {}

   C_bitfield(dword size1):
      mem(NULL),
      num_items(0),
      alloc_size(0)
   {
      Reserve(size1);
   }

   ~C_bitfield(){
      delete[] mem;
   }

   inline void Clear(){
      delete[] mem;
      mem = NULL;
      num_items = 0;
   }

   inline dword Size() const{ return num_items; }

   inline void Reserve(dword num){

      if(num > num_items){
         dword new_size = (num+7) / 8;
         byte *new_mem = new byte[new_size];
         memcpy(new_mem, mem, alloc_size);
         memset(new_mem+alloc_size, 0, new_size-alloc_size);
         delete[] mem;
         mem = new_mem;
         num_items = num;
         alloc_size = new_size;
      }
   }

   inline void Reset(){
      if(mem){
         memset(mem, 0, alloc_size);
      }
   }

   inline void SetOn(dword index, bool on_off = true){

      assert(mem && (index < Size()));
      dword bit = 1 << (index&7);
      byte &b = mem[index / 8];
      if(on_off)
         b |= bit;
      else
         b &= ~bit;
   }

   inline bool IsOn(dword index) const{

      assert(mem && (index < Size()));
      dword bit = 1 << (index&7);
      return (mem[index / 8] & bit);
   }
};

//----------------------------
// Make name of cube texture from given name and index of side (0 ... 5). Extension is preserved.
C_str MakeCubicTextureName(const C_str &name, dword side);

//----------------------------

                              //thunk into QHull library
bool __cdecl qhCreateConvexHull(const C_vector<S_vector> &verts, C_vector<I3D_triface> *tri_faces,
   C_vector<I3D_face> *faces = NULL, C_vector<S_plane> *planes = NULL);

#ifdef _DEBUG
#pragma comment(lib,"qhull_d.lib")
#else
#pragma comment(lib,"qhull.lib")
#endif

//----------------------------
//----------------------------

#endif
