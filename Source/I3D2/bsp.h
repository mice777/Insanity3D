#ifndef __BSP_H
#define __BSP_H

#include "frame.h"

//----------------------------
// Binary sorted partion tree. 
// Features: 
//    -Can contain polygons in nodes (with same or near cooplanar normals to node's plane).
//    -can contain leaves (end nodes with list of polygons with different normals)
//    -polygons are represented as array of original scene triangles outside of tree structure (world coord)
//    -models faces can be represented as array of models faces, each model represented only one (local coord)
//    -nodes can be bounded by axis alignated boxes to posssibility skip branchech if ray not collide with box
//       (unfortunally, in trees which are not divided by axis plane bound boxes are often owerlaping, so
//        there is need of some box hit cache or selective contruction of bboxes. 
//        in fact, node bboxes can slow down entire traaverse)
//    -models could be represented by bound volume (sphere, box), positioned in space occupied by node.
//       this can speed up collision test but also expand traverse algoritm, due need of distance compare.
//    -mooving or destructable geometry and entities can be solved in several ways:
//       a)Post inserted to tree as pointer to model or volume
//       b)separate hiearchy list with supprt for sectors
//       c)Mooving or inserted in octtree as pointer to model or volume. 
//          Other opportunity to it is divide space into octtree with resonable size of nodes(boxes),
//          and into each node compile own bsp tree, and also move there entities and non static geometry.
//          adventage is only one traverse, disadventage bsp faces redundancy (1 frame can reside in more then 8 nodes)
//    -each face/volume can be cached for hit testing by ray: whenewer ray is casted throw tree,
//       result of test with current ray is stored and this face should not be tested again if reside in more nodes
//       main problemm is how reset cache for next ray. Maybe we could keep it as bit array of hit
//       faces, set and read as fast shift opearation, reset by zero mem.
//    -dynamic plane shifting & beveling:(optional)
//    -collision statistic (optional)

/*
   //cached info about ray/plane test.
   // data: current ray
   //       bit_array indicates which planes was tested
   //       info about tested planes
   // input:   plane index
   // output:  ray_origin near/far side(FRONT/BACK)
   //          signed distance along ray to plane
*/

typedef dword BSP_frame_index;

const byte NO_LOD = 255;

//----------------------------

enum E_BSP_BUILD_RESULT{
   BSP_BUILD_FAIL,
   BSP_BUILD_OK,
   BSP_BUILD_CANCEL,
};

                              //points closer then this are considered as on partion plane
#define BSP_SPLIT_TOLERANCE .001f


//----------------------------

enum E_CLIP_LINE{
   CL_INSIDE_NOCLIP = 0,
   CL_INSIDE_CLIP_MIN = 1,
   CL_INSIDE_CLIP_MAX = 2,
   CL_OUTSIDE = 3,
};

enum E_PLANE_SIDE{
   PS_ON,
   PS_FRONT,
   PS_BACK,
};

//----------------------------
// some inlains used un bsp an collisions
//----------------------------

// Check if frm is child of root.
static inline bool IsChildOf(CPI3D_frame frm, CPI3D_frame root){

   do{
      if(frm == root)
         return true;
      frm = frm->GetParent1();
   }while(frm);
   return false;
}

//----------------------------
// Check if frame is hidden by own or by some parent.
inline bool IsHidden(CPI3D_frame frm){

   do{
      if(!frm->IsOn1())
         return true;
      frm = frm->GetParent1();
   }while(frm);
   return false;
}

//----------------------------
//----------------------------

typedef dword BSP_index;      //type used for indexing bsp arrays

//----------------------------
//C_bsp_frames
//----------------------------

//----------------------------
// Class keeping all frames and frames related info used in bsp tree.
class C_bsp_frames{

public:
   struct S_frm_inf{
      C_smart_ptr_const<I3D_frame> frm;
      struct S_checksum{
         float matrix_sum;
         float vertc_sum;
         dword num_v;
      }cksum;
      byte lod_id;
      dword first_face;    //index of first face in lod_faces buffer
      dword num_faces;
   };
private:
   C_vector<S_frm_inf> frm_info;
   C_buffer<I3D_triface> lod_faces; //indexes in vertex buffers of frames with lod

//----------------------------
// Add faces to lod_faces buffer and return nuber of faces added.
// If no lod aviable, return 0.
   dword CreateLod(CPI3D_frame frm, byte &ret_lod);
public:
   dword Size() const { return frm_info.size(); }
   void Clear(){ 
      frm_info.clear();
      lod_faces.clear();
   }

//----------------------------
// Return id of added frame.
   BSP_frame_index AddFrame(CPI3D_frame frm);
   void BuildChecksum();
   bool Save(C_cache *cp) const;
   bool Load(C_cache *cp, CPI3D_scene scn, bool cmp_checksum, I3D_LOAD_CB_PROC *cbP, void *context);

//----------------------------
   bool CompareChecksum(I3D_LOAD_CB_PROC *cbP, void *context) const;

//----------------------------
   inline CPI3D_frame GetFrame(BSP_frame_index indx) const{ 
      assert(indx < frm_info.size());
      return frm_info[indx].frm;
   }
   const S_frm_inf &GetFrameInfo(BSP_frame_index frm_id) const {
      assert(frm_id < frm_info.size());
      return frm_info[frm_id];
   }
   bool GetFace(BSP_frame_index frm_id, dword face_id, I3D_triface &ret, const I3D_triface *all_faces) const;
};

//----------------------------

#define SLIDE_EDGE_ALL 7

//----------------------------
                              //bsp face
struct S_bsp_triface{

   struct S_bsp_origin{

      BSP_frame_index frm_id;
   private:
      dword face_index;          //direct index into mesh index_buffer or lod index buffer
      PI3D_material mat;         //original material of face from face_group
      inline bool SetGroupMaterial(CPI3D_frame frame); //return false if something is wrong
   public:
      S_bsp_origin():
         mat(NULL),
         frm_id(0),
         face_index(0)
      {}   
      bool Setup(BSP_frame_index frame_id, dword face, const C_bsp_frames&); // return false if something is wrong
      inline PI3D_material GetMaterial() const{ return mat; }
      inline dword GetFaceIndex() const{ return face_index; }
      inline BSP_frame_index GetFrameIndex() const{ return frm_id; }
   };
   S_bsp_origin origin;       //info into face's frame

   BSP_index plane_id;        //index of the plane the face is parallel to (into the tree's 'planes' array)
   BSP_index vertices_id[3];  //indicies into 3 verts making the triange (into the tree's 'vertices' array)

   S_plane edge_planes[3];    //edge planes (pointing out of triangle)

   mutable const S_bsp_triface *tag;//help used in collision testing, to mark that this face was already tested

   //bool plane_side;          //set if the normal is parallel to the plane normal
   mutable bool highlight;
   byte slide_flags;          //which edges may modify collision normal in sphere tests
   S_bsp_triface() :
      highlight(false),
      slide_flags(SLIDE_EDGE_ALL),
      tag(NULL)
   {}

   bool IsPointInTriangle(const S_vector &pt)const;

   bool ComputeVertices(const C_bsp_frames&, S_vector *p3vert, const I3D_triface *faces);
   bool ComputeEdges(const S_vector *verts, dword num_v);

   void DebugDraw(PI3D_scene sc, dword color, bool fill = false) const;

   bool GetVertexIndexes(I3D_triface&, const C_bsp_frames&) const;
private:
   bool GetVisualTriface(CPI3D_visual vis, byte lod_id, I3D_triface &ti) const;
};

//----------------------------

struct S_bsp_node{

private: 
   dword num_faces;
   friend class C_bsp_node_list;

//----------------------------
// Return pointer to next bsp node. Should be used only in C_bsp_node_list where is valid.
   inline S_bsp_node *NextNode();
   inline const S_bsp_node *NextNode() const;

   //static S_bsp_node *CreateBspNode(word num_faces1);
public:
   BSP_index plane_id;        //partioning plane (into the tree's 'planes' array)
   union{
      S_bsp_node *front;
      dword front_offset;
   };
   union{
      S_bsp_node *back;
      dword back_offset;
   };

//----------------------------
// Get number of children in child hierarchy.
   dword GetNumChildren() const;

//----------------------------
   void CollectNodesAndFaces(dword &ret_nodes, dword &ret_faces)const;
   void GetStatsRec(I3D_stats_bsp &st) const;

   inline const BSP_index *GetFaces() const { return facelist; }
   inline dword NumFaces() const { return num_faces; }

//----------------------------
// Get child node, determined by given side of plane.
   inline S_bsp_node *GetChildBySide(E_PLANE_SIDE ps) const{
      return (ps == PS_FRONT) ? front : (ps == PS_BACK) ? back : NULL;
   }

//----------------------------
// Return size of node in bytes.
   dword Size() const{ return BspNodeSize(num_faces); }

//----------------------------
// Return size in bytes needed for node with num_faces1;
   static dword BspNodeSize(dword num_faces1);
                              //polygons coincident with partion plane (indexes into tree's 'faces' list)
   BSP_index facelist[1];     //!leave it last, struct has variable length        
};

//----------------------------

struct S_collision_info{

   S_plane plane;             //OPTIM: can we solve it without copy?
   CPI3D_frame hit_frm;
   dword plane_id;
   PI3D_volume vol_source;
   bool modified_face_normal; //true if 'face_normal' contains valid info
   S_vector face_normal;
   byte slided;                //bitflag of edges which was slidet for rurrent face(modified normal)
   int face_index;

   float hit_distance;
   dword triface_id;             //for solving edge collision

   inline bool operator <(const S_collision_info &ci) const{
      return (hit_distance < ci.hit_distance);
   }

   S_collision_info() : 
      plane_id(0),
      hit_frm(NULL),
      vol_source(NULL),
      triface_id((dword)-1),
      face_index(-1),
      slided(0),
      modified_face_normal(false)
   {}
};

//----------------------------// structure keeping info for object collision trace

struct S_trace_help{
   S_vector from;             //trace origin
   S_vector to;               //trace end
   S_vector dir;              //(to - from)
   S_vector dir_norm;         //direction normalized
   float from_squared;        //from.Square()
   float dir_magn;            //trace magnitude
   float r_dir_squared;       //1 / dir.dot(dir) - used i dynamic tests
   float radius;              //sphere object
   dword flags;
   CPI3D_frame frm_ignore;   //skip this frame and his children in tests
   PI3D_volume vol_source;    //pass to collision_list
   dword collide_bits;
   //float space_min, space_max;
   //int critical_dist_i;       //precomputed value for dynamic box tests
   //bool space_computed;

   inline const S_vector &GetNormalizedDir() const{ return dir_norm; }
   inline bool Prepare(){
      dir = to - from;
      r_dir_squared = dir.Square();
      if(IsMrgZeroLess(r_dir_squared)) 
         return false;
      dir_magn = I3DSqrt(r_dir_squared);
      r_dir_squared = 1.0f / r_dir_squared;
      dir_norm = dir / dir_magn;
      from_squared = from.Square();
      return true;
   }
   /*
   inline void PrepareMoveSpace(){
      space_min = from.Magnitude();
      space_max = to.Magnitude();
      if(space_min > space_max)
         swap(space_min, space_max);
      space_min = Max(.0f, space_min - radius);
      space_max = Max(.0f, space_max + radius);
      float critical_d = dir_magn + radius; //we put this computin here 'cos radius is in Prepare undefied yet.
      critical_dist_i = FLOAT_BITMASK(critical_d);
      space_computed = true;
   }
   */
   mutable C_vector<S_collision_info> &collision_list;
   S_trace_help(C_vector<S_collision_info> &col_list_buf):
      collision_list(col_list_buf),
      frm_ignore(NULL),
      vol_source(NULL),
      radius(.0f),
      collide_bits(0),
      //space_min(.0f), space_max(.0f),
      //critical_dist_i(0),
      //space_computed(false),
      flags(0)
   {
      collision_list.clear();
   }
};

//----------------------------

class C_bsp_node_list{

   dword alloc_size;
   dword used_size;
   dword num_nodes;
   byte *buf;
   bool offset_pointers;   //version of node pointers - due build and load they are stored as offsets, after that are converted to real pointers.

//----------------------------
// Reallocate buffer to specified size. Used memory is copied as possible.
   bool Realloc(dword size);

//----------------------------
// Get pointer to unused memory block of specified size and mark this block as used.
// If there is not enough space in buffer, it will try reallocate to double buffer size or at least to required size.
// If allocation fail, function return NULL.
   byte *AllocMem(dword size);

//----------------------------
// Return unused memory in buffer in bytes.
   dword GetFreeSize() const;

//----------------------------
// Calculate size of buffer in bytes which chan hold num_nodes1 with total sum of num_faces1.
// Note: there is assumptation that every node has at least one face.
   static dword CalculBufferSize(dword num_nodes1, dword num_faces1);

//----------------------------
   I3D_RESULT SaveNode(C_cache *cp, const S_bsp_node *node)const;

//----------------------------
   I3D_RESULT LoadNode(C_cache *cp, dword &ret_offset);

//----------------------------
// Return pointer to begin of used memory in buffer, ie firts node.
// If no node allocated, return NULL.
   inline S_bsp_node *GetBegin() const {

      return num_nodes ? (S_bsp_node*)buf : NULL; //theoreticly buffer could be reserved for particular size and no node yet allocated
   }

//----------------------------
// Return end of used memory in buffer, ie behind last node. Is less or equal to end of allocated memory.
// If no node allocated, return NULL.
   inline S_bsp_node *GetEnd() const{

      return num_nodes ? (S_bsp_node*)(buf+used_size) : NULL;
   }

//----------------------------
// Return allocated size of buffer in bytes.
   inline dword GetAllocSize() const{ return alloc_size; }

//----------------------------
// Add node to end of used buffer. Node must be valid (size is computed from node->num_faces).
// If free size in buffer is too small, buffer is realocated.
// Return pointer to memory where node was copied or NULL if allocation fail.
   //inline S_bsp_node *AddNode(const S_bsp_node *node);
public:
   C_bsp_node_list();
   ~C_bsp_node_list(){
      Realloc(0);
   }
   C_bsp_node_list(const C_bsp_node_list &nl):
      buf(NULL),
      offset_pointers(true)
   { operator =(nl); }
   void operator =(const C_bsp_node_list &nl);

//----------------------------
// Return pointer to root node (currently it is begin of buffer).
   inline S_bsp_node *GetRoot() const { return GetBegin(); }

//----------------------------
// Return num nodes currenlty allocated.
   inline dword NumNodes() const{ return num_nodes; }

//----------------------------
// Reserve buffer to required_size in bytes. Used memory is realocated and copied.
   bool Reserve(dword required_size);

//----------------------------
// Return pointer to new allocated node with reserved space for num_faces1;
   S_bsp_node *CreateNode(dword num_faces1);

//----------------------------
   void Clear();

//----------------------------
// Realocate to actualy used size.
   void Compact();

//----------------------------

   inline dword GetOffset(const S_bsp_node *node) const{

      assert(buf);
      assert((byte *)node >= buf);
      dword ret_offset = (byte *)node - buf;
      assert(ret_offset < used_size);
      return ret_offset;
   }

//----------------------------

   inline S_bsp_node *GetNode(dword offset) const{

      return (S_bsp_node*)(buf + offset);
   }

//----------------------------

   bool NodesToOffset();
   bool OffsetsToNodes();

//----------------------------
   I3D_RESULT Save(C_cache *cp) const;

   I3D_RESULT Load(C_cache *cp);
};

//----------------------------

class C_bsp_tree{

   struct S_bsp_header{
      byte version;
      int node_count;
      dword num_planes;
      dword num_faces;
      dword num_vertices;

      S_bsp_header() :
         version(0),
         num_planes(0),
         num_faces(0),
         num_vertices(0),
         node_count(-1) //invalid tree
      {
      }
   };
                              //report frames which are static but are not in tree
   void ReportConsistency(PI3D_scene scene, I3D_LOAD_CB_PROC *cbP, void *context)const;

   friend struct S_frame_base;
protected:
   bool valid;
                              //list of all planes, used for partion or face
   C_vector<S_plane> planes;

                              //list of all planes, used for partion or face
   C_vector<S_bsp_triface> faces;

                              //list of all verticies used by bsp tree
   C_vector<S_vector> vertices;
   C_bsp_frames frames;

   C_bsp_node_list nodes;

                              //traverse:
                              //ray
   float ClassifyRayInterval(BSP_index plane_indx, float ray_min, float ray_max, E_PLANE_SIDE &near_side, E_PLANE_SIDE &far_side,
      const I3D_collision_data &cd) const;
   bool FaceListItersection(const S_bsp_node &node, const float dist, I3D_collision_data &cd) const;
   bool TextureIntersection(const S_bsp_triface &tf, const float hit_dist, I3D_collision_data &cd) const;
   bool NodeIntersection(const S_bsp_node &node, float ray_min, float ray_max, I3D_collision_data &cd) const;
                              //moving sphere
   bool CheckCol_MS_F(const S_bsp_triface &tf, const S_trace_help &th, BSP_index face_id) const;
   bool Classify_MS(BSP_index plane_indx, const S_trace_help &th, float &in_dist, float &out_dist, E_PLANE_SIDE &near_side)const;
   bool FaceListItersection_MS(const S_bsp_node &node, float near_dist, float far_dist, const S_trace_help &th)const;
   bool NodeIntersection_MS(const S_bsp_node &node, float ray_min, float ray_max, const S_trace_help &th)const;

//----------------------------
                              //*** static sphere ***

//----------------------------
// Determine on which side of plane given by 'plane_indx' lies given point.
// Return distance to the plane.
   float GetPointPlaneDistance(BSP_index plane_indx, const S_vector &p, E_PLANE_SIDE &near_side, E_PLANE_SIDE &far_side) const;

   bool CheckCol_S_F(const S_bsp_triface &tf, I3D_collision_data &cd, float plane_dist)const;
   bool NodeIntersection_S(const S_bsp_node &node, I3D_collision_data &cd,
      C_bitfield &face_cache, C_bitfield &frame_cache) const;
   bool FaceListItersection_S(const S_bsp_node &node, I3D_collision_data &cd, float plane_dist,
      C_bitfield &face_cache, C_bitfield &frame_cache) const;
   byte GetSlideNormal(const S_bsp_triface &tf, const S_plane &pl, const S_vector &ip, S_vector &rn) const;

   void DebugHighlight(const S_preprocess_context &pc) const;

//----------------------------
// Generate contacts on one BSP node's trangles.
   void GenContactsOnNode(const S_bsp_node &node, const I3D_contact_data &cd, float plane_dist, void *context) const;

//----------------------------
// Recursively traverse the tree for contact generation.
   void GenContactsRecursive(const S_bsp_node &node, const I3D_contact_data &cd, void *context) const;

//----------------------------
// Get total number of nodes in the tree.
// This function recursively traverses entire tree, thus is potentionally very slow.
   dword GetNumNodes() const{

      S_bsp_node *root = nodes.GetRoot();
      if(root)
         return root->GetNumChildren() + 1;
      return 0;
   }

//----------------------------

public:
   C_bsp_tree():
      valid(false)
   {
   }
   bool TraceSphere(const S_trace_help &th) const;
   inline dword GetNumFaces() const{ return faces.size(); }

//----------------------------

   inline const S_plane &GetPlane(BSP_index id) const{
      assert(id < planes.size()); return planes[id]; 
   }

//----------------------------

   void Close(){
      Clear();
   }

//----------------------------

   const S_bsp_triface *GetFace(dword idx) const{
      if(idx < faces.size())
         return &faces[idx];
      else
         return NULL;
   }

//----------------------------

   bool GetFaceVertices(const S_bsp_triface &tf, S_vector p3vert[3]) const;

//----------------------------
// Collect statistics about tree.
   void GetStats(I3D_stats_bsp &st) const;
   
//----------------------------
   void Clear();
   inline bool IsValid()const { return valid; }
   bool RayIntersection(I3D_collision_data &cd) const;
   bool SphereStaticIntersection(I3D_collision_data &cd) const;                              
   //bool CompareBspChecksum(I3D_LOAD_CB_PROC*, void *context) const;

//----------------------------
// Generate collision contacts with provided volume (see I3D_scene::GenerateContacts for details).
   void GenerateContacts(const I3D_contact_data &cd, PI3D_driver) const;

//----------------------------
   I3D_RESULT Save(C_cache *cp) const;
   I3D_RESULT Load(PI3D_scene scene, C_cache *cp, I3D_LOAD_CB_PROC *cbP, void *context, dword check_flags);

//----------------------------
// Draw the BSP tree - using provided 'draw_help' class for caching the data.
   void DebugDraw(const S_preprocess_context &pc, class C_bsp_drawer &draw_help) const;
};

//----------------------------
// Class for helping visualizing the BSP tree.
//----------------------------
class C_bsp_drawer{
   bool prepared_to_draw;
public:
   struct S_render_group{
      dword vertex_offset;
      dword index_offset;
      dword slide_offset;
      dword thin_offset;
      word num_vertices;
      word num_indices;
      word num_slides;
      word num_thins;
   };
private:
                              //vertex buffer for rendering
   C_vector<S_vector> vertex_buffer;
                              //index buffer for render, composed from groups, like buffer above
   C_vector<word> index_buffer;
                              //index buffer for slide edges
   C_vector<word> slide_edges;
                              //thin edges - used for render non slided edges
   C_vector<word> thin_edges;


   C_vector<S_render_group> render_groups;

public:
   C_bsp_drawer(): prepared_to_draw(false)
   {}

   void Clear(){
      prepared_to_draw = false;
      vertex_buffer.clear();
      index_buffer.clear();
      render_groups.clear();
      slide_edges.clear();
   }

   bool IsPrepared() const{ return prepared_to_draw; }

   void Init(const C_vector<S_vector> &vertices, const C_vector<S_bsp_triface> &faces);

   void Render(const S_preprocess_context &pc);
};

//----------------------------
//----------------------------

#endif                        //__BSP_H