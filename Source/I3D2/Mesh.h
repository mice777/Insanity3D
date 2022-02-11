#ifndef __MESH_H
#define __MESH_H

#include "driver.h"
#include "common.h"

//----------------------------
// Check if set of vertices are planar, use specified thresh to determine
bool CheckIfVerticesPlanar(const S_vector *verts, dword num_verts, float thresh = 1e-2f);

//----------------------------
// Build normals of list of faces
void BuildFaceNormals(const S_vector *verts, const I3D_triface *faces, dword num_faces, S_vector *normals);

//----------------------------
// Find specified face (given by 2 indicies) in list, return index,
// or -1 if not found, if found, fill third index.
int FindFace(const I3D_triface *faces, dword num_faces, word e1, word e2, word &e3);

//----------------------------
// Find specified face (given by 3 indicies) in list, return index,
// or -1 if not found.
int FindFace(const I3D_triface *faces, dword num_faces, const I3D_triface &fc);

//----------------------------
// Find specified edge (given by 2 indicies) in list, return index,
// or -1 if not found.
inline int FindEdge(const I3D_edge *edges, dword num_edges, const I3D_edge &e){
   return FindPointerInArray((void**)edges, num_edges, *(void**)&e);
}

//----------------------------
// Determine which vertices are identical (by examining position) with others
//    in the pool, and create mapping array                                    
void MakeVertexMapping(const S_vector *vp, dword pitch, dword numv, word *v_map, float thresh = .001f);

//----------------------------
// Remove specified edge from list, if it is found, the return value is true.
inline bool RemoveEdge(C_vector<I3D_edge> &edges, const I3D_edge &e){
   int i = FindEdge(&edges.front(), edges.size(), e);
   if(i==-1) return false;
   edges[i] = edges.back(); edges.pop_back();
   return true;
}

//----------------------------
// Add edge into list, or split with inversed edge (remove those from list),
// return value is true if edge is added, or false if it is split.
inline bool AddOrSplitEdge(C_vector<I3D_edge> &edge_list, const I3D_edge &e){
   if(RemoveEdge(edge_list, I3D_edge(e[1], e[0])))
      return false;
   edge_list.push_back(e);
   return true;
}

//----------------------------
// Clean mesh - remove unused vertices and remap faces.
void CleanMesh(C_vector<S_vector> &verts, C_vector<I3D_triface> &faces);
void CleanMesh(C_vector<S_vector> &verts, C_vector<I3D_face> &faces);

//----------------------------

class I3D_face_group{
public:
   C_smart_ptr<I3D_material> mat;
   dword base_index;
   dword num_faces;

   I3D_face_group():
      num_faces(0), base_index(0)
   {}
public:
   inline dword NumFaces1() const{ return num_faces; }
   inline dword BaseIndex1() const{ return base_index; }
                              //material
   void SetMaterial(PI3D_material mp){ mat = mp; }
   inline PI3D_material GetMaterial1(){ return mat; }
   inline CPI3D_material GetMaterial1() const{ return mat; }
};

//----------------------------
// Helper for SaveCachedInfo function - save info about face groups;
bool SaveCachedInfoFGroups(C_cache *ck, const C_buffer<I3D_face_group> &fgroups, const C_vector<C_smart_ptr<I3D_material> > &mats);
bool LoadCachedInfoFGroups(C_cache *ck, C_buffer<I3D_face_group> &fgroups, C_vector<C_smart_ptr<I3D_material> > &mats);

//----------------------------

class I3D_source_vertex_buffer{
   IDirect3DVertexBuffer9 *D3D_vertex_buffer;

   PI3D_driver drv;
   dword num_vertices;        //number of vertices in this buffer

   dword fvflags;
   dword vstride;             //stride of single vertex, in bytes
public:
#ifdef GL
   GLuint vbo;
   C_smart_ptr<IDirect3DVertexDeclaration9> vs_decl;
#endif

   dword D3D_vertex_buffer_index;
   C_vector<word> vertex_map;

   I3D_source_vertex_buffer(PI3D_driver drv1, dword fvf1):
      fvflags(fvf1),
      vstride(::GetSizeOfVertex(fvf1)),
      drv(drv1),
      num_vertices(0),
#ifdef GL
      vbo(0),
#endif
      D3D_vertex_buffer(NULL)
   {}
   ~I3D_source_vertex_buffer();

   inline dword NumVertices() const{ return num_vertices; }
   inline dword GetFVFlags() const{ return fvflags; }
   inline dword GetSizeOfVertex() const{ return vstride; }

   inline IDirect3DVertexBuffer9 *GetD3DVertexBuffer() const{ return D3D_vertex_buffer; }

   void SetFVFlags(dword fvf);

   void *Lock(dword d3d_lock_flags = 0){

      if(!num_vertices)
         return NULL;
      assert(D3D_vertex_buffer);
      void *vp;
      HRESULT hr = D3D_vertex_buffer->Lock(D3D_vertex_buffer_index * vstride,
         num_vertices * vstride,
         &vp,
         d3d_lock_flags);
      CHECK_D3D_RESULT("Lock", hr);
      if(FAILED(hr))
         return NULL;
      return vp;
   }
   inline const void *LockConst() const{ return (const void*)((I3D_source_vertex_buffer*)this)->Lock(D3DLOCK_READONLY); }

   bool Unlock(){
      if(!num_vertices)
         return false;
      assert(D3D_vertex_buffer);
      HRESULT hr = D3D_vertex_buffer->Unlock();
      CHECK_D3D_RESULT("Unlock", hr);
      return (SUCCEEDED(hr));
   }

   inline void UnlockConst() const{ ((I3D_source_vertex_buffer*)this)->Unlock(); }

   void AllocVertices(dword num_verts1);

   void SetVertices(const void *src_vp, dword nv);
   template<class T>
   void SetVertices(const C_vector<T> &vv){
      SetVertices(&vv.front(), vv.size());
   }
   inline const C_vector<word> &GetVertexSpreadMap() const{ return vertex_map; }
   void SetVertexSpreadMap(const C_vector<word> &vs_map){ vertex_map = vs_map; }
   void SetVertexSpreadMap(const word *beg, dword num){ vertex_map.clear(); vertex_map.insert(vertex_map.end(), beg, beg + num); }

   void DuplicateVertices(word *vi, dword num_verts);
};

//----------------------------

class I3D_source_index_buffer{
   IDirect3DIndexBuffer9 *D3D_index_buffer;

   PI3D_driver drv;
   dword num_faces;
public:
   dword D3D_index_buffer_index; //in sizeof(I3D_triface)
#ifdef GL
   GLuint ibo;
#endif

   I3D_source_index_buffer(PI3D_driver);
   I3D_source_index_buffer():
      D3D_index_buffer(NULL),
#ifdef GL
      ibo(0),
#endif
      num_faces(0)
   {}
   ~I3D_source_index_buffer();
   void Init(PI3D_driver d){ drv = d; }
   inline dword NumFaces1() const{ return num_faces; }
   inline IDirect3DIndexBuffer9 *GetD3DIndexBuffer() const{ return D3D_index_buffer; }

   PI3D_triface Lock(dword d3d_lock_flags = 0);
   inline CPI3D_triface LockConst() const{ return (CPI3D_triface)((I3D_source_index_buffer*)this)->Lock(D3DLOCK_READONLY); }

   const I3D_triface *LockForReadOnly() const;
   bool Unlock() const;
   void AllocFaces(dword);
   void SetFaces(const I3D_triface*, dword num_faces);
   void SetFaces(const C_vector<I3D_triface> &fv){ SetFaces(&fv.front(), fv.size()); }
};

//----------------------------

struct S_fgroup_strip_info{
   dword base_index;          //index into index buffer, where the strip for face group begins
   dword num_indicies;        //number of indicies in index buffer, which make strip for face group
};

//----------------------------

class C_auto_lod{
   I3D_source_index_buffer index_buffer;
public:
   float ratio;               //quality ratio of this LOD, range 0.0f ... 1.0f
   dword vertex_count;
   C_buffer<I3D_face_group> fgroups;
#ifdef USE_STRIPS
                              //info about strips
   C_buffer<S_fgroup_strip_info> strip_info;
   inline const S_fgroup_strip_info *GetStripInfo() const{ return strip_info.begin(); }
   inline bool HasStrips() const{ return (strip_info.size()!=0); }
#else
   inline bool HasStrips() const{ return false; }
#endif

   C_auto_lod():
      ratio(0.0f),
      vertex_count(0)
   {}
   inline dword NumFaces1() const{
      if(!fgroups.size()) return 0;
      const I3D_face_group &fg = fgroups.back();
      return fg.base_index + fg.num_faces;
   }

   void Init(PI3D_driver drv){ index_buffer.Init(drv); }
   inline const I3D_source_index_buffer &GetIndexBuffer() const{ return index_buffer; }

   void SetFaces(const I3D_triface *faces, bool allow_strips = true);
   I3DMETHOD(GetFaces)(I3D_triface *buf) const;
};

//----------------------------

bool SaveCachedInfoLods(C_cache *ck, const C_buffer<C_auto_lod> &auto_lods, const C_vector<C_smart_ptr<I3D_material> > &mats);
bool LoadCachedInfoLods(C_cache *ck, C_buffer<C_auto_lod> &auto_lods, C_vector<C_smart_ptr<I3D_material> > &mats, PI3D_driver drv);

//----------------------------
                              //additional uv mapping channel info
class C_map_channel: public C_unknown{
public:
   C_buffer<S_vector2> uv_verts;
   C_buffer<I3D_triface> uv_faces;
   C_str bitmap_name;
};

//----------------------------
                              //mesh base - class used for higher-level mesh-based sources
                              //it contains:
                              // - vertices and faces (in vertex and index buffers)
                              // - auto-LOD info
class I3D_mesh_base{
protected:
   dword ref;

   PI3D_driver drv;

                              //face groups
   C_buffer<I3D_face_group> fgroups;
public:
#ifdef USE_STRIPS
   inline const S_fgroup_strip_info *GetStripInfo() const{ return strip_info.begin(); }
   inline bool HasStrips() const{ return (strip_info.size()!=0); }
protected:
                              //info about strips; if zero size, no strips used
   C_buffer<S_fgroup_strip_info> strip_info;
#else
   inline bool HasStrips() const{ return false; }
protected:
#endif
                              //automatic LOD
   C_buffer<C_auto_lod> auto_lods;
                              //distance range at which computed LODs are distributed,
                              // valid at camera's FOV = 65 degrees, otherwise scaled
   float auto_lod_dists[2];   //{min, max}

                              //bounding volume of all vertices;
   I3D_bound_volume bvolume;

   C_str smooth_groups;
   C_smart_ptr<C_map_channel> map_channel_2;
   I3D_source_index_buffer index_buffer;

   friend class C_loader;
public:
   I3D_mesh_base(PI3D_driver d1, dword fvf1);
   ~I3D_mesh_base();

   inline const byte *GetSmoothGroups() const{
      return !smooth_groups.Size() ? NULL :
         (const byte*)(const char*)smooth_groups;
   }
   void SetSmoothGroups(const C_vector<byte> &sg){
      if(sg.size()){
         assert(sg.size() == NumFaces1());
         smooth_groups.Assign((const char*)&sg.front(), sg.size());
      }else{
         smooth_groups = NULL;
      }
   }

   inline void SetMapChannel2(C_map_channel *mc){ map_channel_2 = mc; }
   inline const C_map_channel *GetMapChannel2() const{ return map_channel_2; }

   I3D_source_vertex_buffer vertex_buffer;

   inline dword NumVertices1() const{ return vertex_buffer.NumVertices(); }
   //inline dword NumFaces1() const{ return index_buffer.NumFaces1(); }
   inline dword NumFaces1() const{
      if(!fgroups.size()) return 0;
      const I3D_face_group &fg = fgroups.back();
      return fg.base_index + fg.num_faces;
   }

   inline const I3D_source_index_buffer &GetIndexBuffer() const{ return index_buffer; }
   inline dword NumFGroups1() const{ return fgroups.size(); }
   inline I3D_face_group *GetFGroups1(){ return fgroups.begin(); }
   inline const I3D_face_group *GetFGroups1() const{ return fgroups.begin(); }
   inline const C_buffer<I3D_face_group> &GetFGroupVector() const{ return fgroups; }
   inline void SetFGroups(const I3D_face_group *fgps, dword num_fgroups){
      fgroups.assign(fgps, fgps+num_fgroups);
   }

   inline const C_auto_lod *GetAutoLODs() const{ return auto_lods.begin(); }
   inline const dword NumAutoLODs() const{ return auto_lods.size(); }
   inline const float *GetAutoLODDists() const{ return auto_lod_dists; }
   inline const I3D_bound_volume &GetBoundingVolume() const{ return bvolume; }

//----------------------------
// Create automatic levels of detail.
// If 'vertex_nobreak_info' is not NULL, it specifies an array of flags, where each flag gives value,
// if given vertex cannot be optimized out. This array is as big as vertex pool.
   void CreateLODs(dword min_num_faces, dword num_lods, float min_dist, float max_dist, bool preserve_edges,
      const bool *vertex_nobreak_info = NULL);

//----------------------------
// Create simplified mesh - generate new set of faces operating on meshes vertex pool.
   void CreateSimplifiedMesh(C_buffer<I3D_triface> &faces, float quality = 1.0f) const;

//----------------------------
// Set faces. This method stores faces into index buffer in appropriate format (list/strip).
// The number of faces is given by fgroups (NumFaces method).
   void SetFaces(const I3D_triface *faces, bool allow_strips = true);

//----------------------------
// Constructor for cloning, using artibrary source mesh.
   void Clone(CPI3D_mesh_base mb, bool allow_strips);

//----------------------------
// Setup cloned verts.
   void SetClonedVertices(C_vector<byte> &new_vertices){
      vertex_buffer.SetVertices(&new_vertices.front(), new_vertices.size()/vertex_buffer.GetSizeOfVertex());
   }

//----------------------------
// Compute bounding volume of mesh.
   void ComputeBoundingVolume();

//----------------------------
// Apply pivot offset and non-uniform scale to vertices.
   void ApplyPivotAndNUScale(const S_vector &p, const S_vector &s);
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
                              //vertices
   I3DMETHOD_(dword,NumVertices)() const{ return vertex_buffer.NumVertices(); }
   I3DMETHOD_(const S_vector*,LockVertices)(){ return (const S_vector*)vertex_buffer.Lock(); }
   I3DMETHOD_(void,UnlockVertices)(){ vertex_buffer.Unlock(); }
   I3DMETHOD_(dword,GetSizeOfVertex)() const{ return vertex_buffer.GetSizeOfVertex(); }
                              //faces and groups
   I3DMETHOD_(dword,NumFGroups)() const{ return fgroups.size(); }
   I3DMETHOD_(dword,NumFaces)() const{ return NumFaces1(); }
   I3DMETHOD_(CPI3D_face_group,GetFGroups)() const{ return &fgroups.front(); }
   //I3DMETHOD_(const I3D_triface*,LockFaces)(){ return index_buffer.Lock(); }
   //I3DMETHOD_(void,UnlockFaces)(){ index_buffer.Unlock(); }
   I3DMETHOD(GetFaces)(I3D_triface *buf) const;

   virtual bool SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const;
   virtual bool LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats);
   I3DMETHOD_(void,Reserved02)(){}
   I3DMETHOD_(void,Reserved03)(){}
   I3DMETHOD_(void,Reserved04)(){}
   I3DMETHOD_(void,Reserved05)(){}
   I3DMETHOD_(void,Reserved06)(){}
   I3DMETHOD_(void,Reserved07)(){}
};

//----------------------------
//----------------------------

#endif
