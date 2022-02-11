/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Sng_mesh.cpp
   Content: Single-mesh frame.
--------------------------------------------------------*/

#include "all.h"
#include "sng_mesh.h"
#include "joint.h"
#include "camera.h"
#include "scene.h"
#include "loader.h"
#include "mesh.h"


//----------------------------

                              //use 3 weights, actually only one is used,
                              // others 2 are indicies into 2 matrices
#define FVF_VERTEX_SINGLEMESH (D3DFVF_XYZB3 | D3DFVF_NORMAL | D3DFVF_TEX1)

                              //make bounding volume slightly greater,
                              // for safe purposes
const float SAFE_BOUND_THRESH = .001f;

//#define USE_VS_QUATERNION     //use quaternion rotation in VS (not matrices) - it's slower!
//#undef USE_STRIPS             //define to disable using strips (don't work well with broken meshes)

                              //debugging:
//#define DEBUG_PAINT_VERTEX_ASSIGNMENT
//#define DEBUG_PAINT_ASSIGN_WAVE

//----------------------------
//----------------------------

struct S_SM_vertex_info{
   int joint_i;               //index of joint, to which this vertex belongs (and partially to its parent)
   float weight;              //ratio, by which vertex belongs to this joint, rather than to its parent
   bool assigned;             //set to true as soon as vertex is assigned to joint
   int segment_index;
   enum{                      //vertex 100% belongs to 'break' joint has assigned this
      BREAK_NO,               //no breaking
      BREAK_SPLIT,            //vertex to split (duplicate)
      BREAK_TEAR_OFF,         //vertex to tear off
   } break_mode;

   S_SM_vertex_info():
      assigned(false),
      segment_index(-1),
      break_mode(BREAK_NO)
   {}
};

//----------------------------
                           //helper class for building mesh
struct S_SM_joint_build_info{
   enum E_REGION_TYPE{
      REGION_FULL,            //vertex is 100% influenced by region
      REGION_LINEAR,          //weight is computed from Z axis
      REGION_ELIPSOID,        //radial type, fading influence from its center (linearly)
      REGION_ELIPSOID_SIN,    //same as above, but with sine fade, instead of linear
   } rtype;

   S_matrix m_bb;             //joint's bbox matrix in local coords (relative to single-mesh)
   S_matrix m_bb_inv;         //also inverted
   PI3D_joint joint;
   C_vector<word> vertices;     //indices to vertices - temporary during building
   int bone_segment;
};

//----------------------------

#ifdef _DEBUG
static void PaintEdge(PI3D_driver drv, const C_vector<S_vector> &verts, const I3D_edge &e){
   drv->DebugLine(verts[e[0]], verts[e[1]], 0);
}

static void PaintNormal(PI3D_driver drv, const S_vector &v, const S_vector &n){
   drv->DebugLine(v, v+n, 0);
}
#endif

//----------------------------
//----------------------------
                              //single-mesh mesh class
class I3D_singlemesh: public I3D_mesh_base{
public:
                              //joint info, lists vertices affected by single joint in I3D_singlemesh
                              //I3D_singlemesh contains one or more such 
                              //Note: joint is referenced by name
   struct S_SM_joint_info{
      C_str joint_name;
      S_matrix orig_mat_inv;  //joint's original matrix, in mesh's local coords, inverted
      I3D_bound_volume bvol;
   };

   struct S_bone_segment{
      word base_vertex;       //index to 1st vertex used by segment (from beginning of vertex buffer)
      word num_vertices;      //index to 1st vertex used by segment (from beginning of vertex buffer)
      C_buffer<I3D_face_group> fgroups;   //fgroups, indexing faces from beginning of face array
      C_buffer<S_SM_joint_info> joint_info;   //info about all joints in the segment
      C_buffer<C_auto_lod> auto_lods;  //auto-lods of the segment
      C_str segment_root;     //NULL = SM itself
      S_matrix root_orig_mat_inv;
   };

   struct I3D_vertex_singlemesh{
      S_vector xyz;
      float indicies[2];
      float weight;
      S_vector normal;
      I3D_text_coor tex;
   };
private:
   I3D_bound_volume bvol_root;//bounding volume of vertices affected by root matrix

   C_buffer<S_bone_segment> bone_segments;

//----------------------------
// Helper class for collecting joints during preprocessing phase.
   struct S_collect_joints{
      const S_matrix *m_inv;
      C_vector<S_SM_joint_build_info> *joint_info;
      int curr_break_index;

      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

         switch(frm->GetType1()){
         case FRAME_JOINT:
            {
               PI3D_joint joint = I3DCAST_JOINT(frm);
               bool break_joint = (joint->GetJointFlags()&JOINTF_BREAK);
               if(joint->IsBBoxValid()){
                  S_collect_joints *hp = (S_collect_joints*)c;
                  hp->joint_info->push_back(S_SM_joint_build_info());
                  S_SM_joint_build_info &ji = hp->joint_info->back();
                  ji.m_bb = joint->GetBBoxMatrix1() * joint->GetMatrix() * (*hp->m_inv);
                  ji.m_bb_inv = ~ji.m_bb;
                  ji.joint = joint;
                  ji.bone_segment = !break_joint ? 0 : hp->curr_break_index++;
                  ji.rtype = S_SM_joint_build_info::REGION_LINEAR;
                  switch(joint->GetRegionIndex()){
                  case 1: ji.rtype = S_SM_joint_build_info::REGION_FULL; break;
                  case 2: ji.rtype = S_SM_joint_build_info::REGION_ELIPSOID; break;
                  case 3: ji.rtype = S_SM_joint_build_info::REGION_ELIPSOID_SIN; break;
                  }
#if 0                         //debug paint direction of bounding-box
                              // (not up-to-date!)
                  {
                     joint->GetDriver1()->DebugLine(joint->GetWorldPos(), joint->GetWorldPos() + (ji.m_loc * root->GetMatrix())(2), 0);
                     drv->DebugLine(verts[e[0]], verts[e[1]], 0);
                  }
#endif
               }
                              //if joint marked as 'break', skip its children
               if(!break_joint)
                  frm->EnumFrames(cbEnum, c, ENUMF_ALL);
               return I3DENUMRET_SKIPCHILDREN;
            }
            break;
         }
         //return I3DENUMRET_SKIPCHILDREN;
         return I3DENUMRET_OK;
      }
   };

//----------------------------
// Check if vertex is in a bounding-box, also compute weight value, depending on region type.
// If test succeeded, weight is in range 0.0f ... 1.0f
   static bool IsPointInBBox(const S_vector &in_v, const I3D_bbox &bb, const S_matrix &m_inv,
      S_SM_joint_build_info::E_REGION_TYPE rtype, float &weight){

                              //get point in local coords of bbox
      S_vector v = in_v * m_inv;
      if(v.x > bb.min.x && v.x < bb.max.x &&
         v.y > bb.min.y && v.y < bb.max.y &&
         v.z > bb.min.z && v.z < bb.max.z){

                              //compute weight
         switch(rtype){
         case S_SM_joint_build_info::REGION_FULL:
            weight = 1.0f;
            break;
         case S_SM_joint_build_info::REGION_LINEAR:
            {
               float f = (bb.max.z - bb.min.z);
               assert(!IsMrgZeroLess(f));
               weight = (v.z - bb.min.z) / f;
            }
            break;
         case S_SM_joint_build_info::REGION_ELIPSOID:
         case S_SM_joint_build_info::REGION_ELIPSOID_SIN:
            {
                                 //compute normalized direction of C_vector relative to
               S_vector dir = v;
               float d = dir.Magnitude();
               if(IsMrgZeroLess(d)){
                  weight = 1.0f;
               }else{
                  dir /= d;
                  dir.x = I3DFabs(dir.x);
                  dir.y = I3DFabs(dir.y);
                  dir.z = I3DFabs(dir.z);
                  //float r = (dir * bb.max).Sum();
                  //float r = bb.max.Magnitude();
                  //float r = Max(bb.max.x, Max(bb.max.y, bb.max.z));
                  float r = Max(bb.max.x, Max(bb.max.y, bb.max.z));
                  weight = 1.0f - (d / r);
                  if(weight <= 0.0f)
                     return false;
                  assert(weight <= 1.0f);
                  if(rtype==S_SM_joint_build_info::REGION_ELIPSOID_SIN)
                     weight = (float)sin(weight*PI*.5f);
               }
            }
            break;
         default:
            assert(0);
         }
         return true;
      }
      return false;
   }

//----------------------------
// Assign unique vertices inside of joint's bounding boxes.
   static void AssignUniqueInsideVertices(C_loader &ld, C_vector<S_SM_vertex_info> &vertex_info,
      const I3D_vertex_singlemesh *verts, const word *vertex_map,
      const C_vector<S_SM_joint_build_info> &joint_info, CPI3D_frame root, const word *v_remap){

                                 //process all unique vertices
      for(int i=vertex_info.size(); i--; ){
                                 //if not unique, skip it
         if(vertex_map[i] != i)
            continue;

         const I3D_vertex_singlemesh &v = verts[i];
         S_SM_vertex_info &vi = vertex_info[i];

         vi.joint_i = -1;
         vi.weight = 0.0f;
         for(int j = joint_info.size(); j--; ){
            const S_SM_joint_build_info &ji = joint_info[j];
            PI3D_joint joint = ji.joint;


            float weight;
            if(joint->IsBBoxValid()){
               if(IsPointInBBox(v.xyz, joint->GetBBox1(), ji.m_bb_inv, ji.rtype, weight)){
                                 //check if re-assign desired
                  if(vi.assigned){
                              //re-assign only if the better joint is not this joint's parent
                     PI3D_frame curr_j = joint_info[vi.joint_i].joint;
                     if(weight<=vi.weight){
                        for(PI3D_frame p = joint; (p=p->GetParent1(), p); ){
                           if(curr_j==p)
                              break;
                        }
                        if(p)
                           vi.assigned = false;
                     }else{
                        for(PI3D_frame p = curr_j; (p=p->GetParent1(), p); ){
                           if(joint==p)
                              break;
                        }
                        if(p)
                           weight = 0.0f;
                     }
                  }
                                 //check better weight
                  if(!vi.assigned || weight>vi.weight){
                     vi.joint_i = j;
                     vi.weight = weight;
                     vi.assigned = true;
                  }
               }
            }
#if defined DEBUG_PAINT_VERTEX_ASSIGNMENT || 0
            if(vi.assigned){
               drv->DebugLine(v.xyz * root->GetMatrix(), joint_info[vi.joint_i].joint->GetWorldPos(), 0);
            }
#endif //DEBUG_PAINT_VERTEX_ASSIGNMENT
         }
      }
      /*
                                 //utilize skin data for additional assignment
      C_loader::t_skin_data::const_iterator it = ld.skin_data.find(root);
      if(it != ld.skin_data.end()){
         const C_loader::t_skin_data_list &sd = (*it).second;
         for(int ji=joint_info.size(); ji--; ){
            const char *jname = joint_info[ji].joint->GetOrigName();
            for(int j=sd.size(); j--; ){
               if(sd[j].joint_name==jname){
                  const C_buffer<C_loader::S_skin_data::S_vertex_info> &vinfo = sd[j].vertex_info;
                  for(dword vi = vinfo.size(); vi--; ){
                     word vertex_index = vinfo[vi].vertex_index;
                     vertex_index = v_remap[vertex_index];
                     if(vertex_index==0xffff)
                        continue;
                     float weight = vinfo[vi].weight;
                     assert(vertex_index<vertex_info.size());
                     S_SM_vertex_info &vi = vertex_info[vertex_index];
                     if(!vi.assigned){
                        vi.joint_i = ji;
                        vi.weight = weight;
                        vi.assigned = true;
                     }
                  }
                  break;
               }
            }
         }
      }
      */
   }

//----------------------------
// Assign all vertices, which are not directly in one of joints' bounding boxes.
// This is done by traversing the edges and determining, which to assigned vertices
// given vertex is connected to.
   static void AssignUnassignedVertices(C_vector<S_SM_vertex_info> &vertex_info,
      const I3D_vertex_singlemesh *verts, const word *vertex_map,
      const I3D_triface *faces, dword num_faces,
      const C_vector<S_SM_joint_build_info> &joint_info, CPI3D_frame root){

      dword num_verts = vertex_info.size();
      int i;

      C_vector<C_vector<word> > vertex_face_use(num_verts);
      for(i = num_faces; i--; ){
         const I3D_triface &fc = faces[i];
         vertex_face_use[vertex_map[fc[0]]].push_back(word(i));
         vertex_face_use[vertex_map[fc[1]]].push_back(word(i));
         vertex_face_use[vertex_map[fc[2]]].push_back(word(i));
      }

      struct S_hlp{
         static void SpreadVertex(dword vertex_i, const I3D_triface *all_faces,
            C_vector<S_SM_vertex_info> &vertex_info, const word *v_map,
            const C_vector<S_SM_joint_build_info> &joint_info,
            const I3D_vertex_singlemesh *verts,
#ifdef DEBUG_PAINT_ASSIGN_WAVE
            PI3D_driver drv, const S_matrix &mat,
#endif
            const C_vector<C_vector<word> > &vertex_face_use, int &num_done){

            const C_vector<word> &face_use = vertex_face_use[vertex_i];
                                 //process all faces accessing this
            for(int i=face_use.size(); i--; ){
               const I3D_triface &fc = all_faces[face_use[i]];
                                 //this is our face, spread it further
               for(int k=3; k--; ){
                  word vertex_i1 = fc[k];
                                 //process only unique verts
                  vertex_i1 = v_map[vertex_i1];
                                 //don't process this vertex
                  if(vertex_i1 == vertex_i)
                     continue;

                  S_SM_vertex_info &vi = vertex_info[vertex_i1];
                                 //don't process processed ones
                  if(vi.assigned)
                     continue;
#if defined DEBUG_PAINT_ASSIGN_WAVE || 0
                  {
                     drv->DebugLine(verts[vertex_i].xyz*mat, verts[vertex_i1].xyz*mat, 0);
                  }
#endif
                  int j_indx = vertex_info[vertex_i].joint_i;

                  const S_SM_joint_build_info &ji = joint_info[j_indx];
                  PI3D_joint joint = ji.joint;
                  if(joint->IsBBoxValid() && joint->GetRegionIndex()==-1){
                     float w;
                     if(!IsPointInBBoxXY(verts[vertex_i1].xyz, joint->GetBBox1(), ji.m_bb_inv, w))
                        continue;
                     if(w <= 0.0f)
                        continue;
                     vi.weight = 1.0f;
                  }else{
                                 //shouldn't happen!
                     continue;
                     vi.weight = 0.0f;
                  }
                                 //set vertex to be like the processed one
                  vi.joint_i = j_indx;

                  vi.assigned = true;
                  ++num_done;
                                 //spread this vertex further
                  SpreadVertex(vertex_i1, all_faces, vertex_info, v_map,
                     joint_info, verts,
#ifdef DEBUG_PAINT_ASSIGN_WAVE
                     drv, mat,
#endif
                     vertex_face_use, num_done);
               }
            }
         }
      };

      int num_done;
      do{
         num_done = 0;
         for(i = num_verts; i--; ){    
                                 //process only unique vertices
            if(vertex_map[i] != i)
               continue;
            S_SM_vertex_info &vi = vertex_info[i];
            if(!vi.assigned){
               continue;
            }
                                 //spread accross all faces
            S_hlp::SpreadVertex(i, faces, vertex_info, vertex_map,
               joint_info, verts,
#ifdef DEBUG_PAINT_ASSIGN_WAVE
               root->GetDriver1(), root->GetMatrix(),
#endif
               vertex_face_use, num_done);
         }
      }while(num_done);
   }

//----------------------------
// Store vertices data info vertex pool.
// Also assign joint and weight to yet unassigned vertices, which is actually
// wrong assignment, but for the rest of pipeline, each vertex must belong somewhere.
   static void StoreVertexInfo(const C_vector<S_SM_vertex_info> &vertex_info, C_vector<S_SM_joint_build_info> &joint_info,
      I3D_vertex_singlemesh *verts, dword num_verts, const word *vertex_map){

      C_vector<dword> parent_index(joint_info.size());
                                 //generate per-joint index into parent joint
      for(int i=joint_info.size(); i--; ){
         S_SM_joint_build_info &ji = joint_info[i];
                                 //find index of parent joint
         PI3D_frame j_prnt = ji.joint->GetParent();
         dword prnt_index = joint_info.size();
         if(j_prnt->GetType1() == FRAME_JOINT){
            for(prnt_index=0; prnt_index<joint_info.size(); prnt_index++){
               if(joint_info[prnt_index].joint == j_prnt)
                  break;
            }
         }
         parent_index[i] = prnt_index;
      }

      float ji_max = (float)joint_info.size();
#ifdef USE_VS_QUATERNION
      ji_max *= 2.0f;
#else
      ji_max *= 3.0f;
#endif
      for(i = num_verts; i--; ){
         const S_SM_vertex_info &vi = vertex_info[vertex_map[i]];
         I3D_vertex_singlemesh &v = verts[i];
         if(!vi.assigned){
            v.weight = 0.0f;
            v.indicies[0] = ji_max;
            v.indicies[1] = ji_max;
            continue;
         }
         v.weight = vi.weight;
         int ji = vi.joint_i;
                                 //generate matrix palette indicies
                                 // each matrix consumes 3 slots, so multiply by 3
         v.indicies[0] = (float)ji;
         v.indicies[1] = (float)parent_index[ji];
#ifdef USE_VS_QUATERNION
         v.indicies[0] *= 2.0f;
         v.indicies[1] *= 2.0f;
#else
         v.indicies[0] *= 3.0f;
         v.indicies[1] *= 3.0f;
#endif
         joint_info[ji].vertices.push_back(word(i));
      }
   }

//----------------------------

   static void InitBoundingVolumes(I3D_bound_volume &bvol_root, C_buffer<S_SM_joint_info> &bone_info,
      const C_vector<S_SM_joint_build_info> &joint_info, const I3D_vertex_singlemesh *verts, CPI3D_frame root,
      const C_vector<S_SM_vertex_info> &vertex_info){

      int i;
      bvol_root.bbox.Invalidate();
                              //create bounding-boxes on bones
      for(i=bone_info.size(); i--; ){
         const S_SM_joint_build_info &ji = joint_info[i];
         S_SM_joint_info &bs = bone_info[i];
         bs.bvol.bbox.Invalidate();
                              //expand by all our verts
         for(int j=ji.vertices.size(); j--; ){
            const I3D_vertex_singlemesh &v = verts[ji.vertices[j]];
                              //consider this vertex if it has at least something from us
            if(ji.joint->GetParent()==root && v.weight!=1.0f){
                              //put into root bound
               bvol_root.bbox.min.Minimal(v.xyz);
               bvol_root.bbox.max.Maximal(v.xyz);
            }
            if(v.weight!=0.0f){
               bs.bvol.bbox.min.Minimal(v.xyz);
               bs.bvol.bbox.max.Maximal(v.xyz);
            }
         }

                              //expand by vertices of all other bones, which connect to us
                              // or by parent's joint
         for(int k=bone_info.size(); k--; ){
            const S_SM_joint_build_info &ji1 = joint_info[k];
            float extreme_weight = 1.0f;
            if(ji1.joint==ji.joint->GetParent()){
               continue;
                              //expand by parent's joint
               extreme_weight = 0.0f;
            }else{
               if(ji1.joint->GetParent()!=ji.joint)
                  continue;
            }

            for(int j=ji1.vertices.size(); j--; ){
               const I3D_vertex_singlemesh &v = verts[ji1.vertices[j]];
                              //consider this vertex if it has at least something from us
               if(v.weight==extreme_weight)
                  continue;
               bs.bvol.bbox.min.Minimal(v.xyz);
               bs.bvol.bbox.max.Maximal(v.xyz);
            }
         }

         for(j=0; j<3; j++){
            bs.bvol.bbox.min[j] -= SAFE_BOUND_THRESH;
            bs.bvol.bbox.max[j] += SAFE_BOUND_THRESH;
         }

                              //make bounding sphere
         S_vector bbox_half_diagonal = (bs.bvol.bbox.max - bs.bvol.bbox.min) * .5f;
         bs.bvol.bsphere.pos = bs.bvol.bbox.min + bbox_half_diagonal;
         bs.bvol.bsphere.radius = bbox_half_diagonal.Magnitude();
      }
                              //finish computation of root's bounding volume
      for(i=vertex_info.size(); i--; ){
         const S_SM_vertex_info &vi = vertex_info[i];
         if(vi.joint_i==-1){
            const I3D_vertex_singlemesh &v = verts[i];
            bvol_root.bbox.min.Minimal(v.xyz);
            bvol_root.bbox.max.Maximal(v.xyz);
         }
      }

                              //make bounding sphere
      S_vector bbox_half_diagonal = (bvol_root.bbox.max - bvol_root.bbox.min) * .5f;
      bvol_root.bsphere.pos = bvol_root.bbox.min + bbox_half_diagonal;
      bvol_root.bsphere.radius = bbox_half_diagonal.Magnitude();
   }

//----------------------------

public:
   I3D_singlemesh(PI3D_driver d):
      I3D_mesh_base(d, FVF_VERTEX_SINGLEMESH)
   {
      bvol_root.bbox.Invalidate();
      bvol_root.bsphere.pos.Zero();
      bvol_root.bsphere.radius = 0.0f;
   }

//----------------------------
// Initialize bones from provided hierarchy, where joints are scanned.
   bool InitBones(C_loader &ld, CPI3D_visual root, const C_loader::S_auto_lod_data &al_data);

//----------------------------
   inline const C_buffer<S_bone_segment> &GetBoneSegments() const{ return bone_segments; }

   inline const I3D_bound_volume &GetRootBounds() const{ return bvol_root; }

//----------------------------
// Save/Load mesh - vertices and faces after optimization, vertex normals, bone info.
   bool SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const{

                              //write core
      if(!I3D_mesh_base::SaveCachedInfo(ck, mats))
         return false;
                              //write segment info
      dword num = bone_segments.size();
      ck->write(&num, sizeof(word));
      for(dword i=0; i<num; i++){
         const S_bone_segment &bs = bone_segments[i];
         ck->write(&bs.base_vertex, sizeof(word));
         ck->write(&bs.num_vertices, sizeof(word));
         ck->write(&bs.root_orig_mat_inv, sizeof(S_matrix));
         ck->write((const char*)bs.segment_root, bs.segment_root.Size() + 1);

         SaveCachedInfoFGroups(ck, bs.fgroups, mats);
                              //write bone info
         {
            dword num = bs.joint_info.size();
            ck->write(&num, sizeof(word));
            for(dword j=0; j<num; j++){
               const S_SM_joint_info &bi = bs.joint_info[j];
                              //write joint's name
               ck->write((const char*)bi.joint_name, bi.joint_name.Size() + 1);
                              //other info
               ck->write((const char*)&bi.orig_mat_inv, sizeof(S_matrix));
               ck->write((const char*)&bi.bvol, sizeof(bi.bvol));
            }
         }
         SaveCachedInfoLods(ck, bs.auto_lods, mats);
      }
      ck->write((const char*)&bvol_root, sizeof(bvol_root));
      return true;
   }

//----------------------------

   bool LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){

                                 //read core
      if(!I3D_mesh_base::LoadCachedInfo(ck, lc, mats))
         return false;
      {                          //read bone info
         word num;
         ck->read(&num, sizeof(num));
         bone_segments.assign(num);
         for(int i=0; i<num; i++){
            S_bone_segment &bs = bone_segments[i];
            ck->read(&bs.base_vertex, sizeof(word));
            ck->read(&bs.num_vertices, sizeof(word));
            ck->read(&bs.root_orig_mat_inv, sizeof(S_matrix));
            char name[256];
            int j = 0;
            do{
               ck->read(&name[j++], sizeof(char));
            }while(name[j-1]);
            bs.segment_root = name;
            if(!LoadCachedInfoFGroups(ck, bs.fgroups, mats))
               return false;
            {
               word num = 0xffff;
               ck->read(&num, sizeof(num));
               if(num==0xffff)
                  return false;
               bs.joint_info.assign(num);
               for(int i=0; i<num; i++){
                  S_SM_joint_info &bi = bs.joint_info[i];
                              //read joint's name
                  int j = 0;
                  do{
                     ck->read(&name[j++], sizeof(char));
                  }while(name[j-1]);
                  bi.joint_name = name;
            
                                       //other info
                  ck->read(&bi.orig_mat_inv, sizeof(S_matrix));
                  ck->read(&bi.bvol, sizeof(bi.bvol));
               }
            }
            if(!LoadCachedInfoLods(ck, bs.auto_lods, mats, drv))
               return false;
         }
         ck->read(&bvol_root, sizeof(bvol_root));
      }
      return true;
   }

//----------------------------

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   inline const C_auto_lod *GetAutoLODs(int si) const{ return
      !si ? auto_lods.begin() : bone_segments[si].auto_lods.begin();
   }
   inline const int NumAutoLODs(int si) const{
      return !si ? auto_lods.size() : bone_segments[si].auto_lods.size();
   }

   I3DMETHOD_(dword,NumFaces)() const{
      if(!bone_segments.size()){
         //return index_buffer.NumFaces1();
         const I3D_face_group &fg = fgroups.back();
         return fg.base_index + fg.num_faces;
      }
      const S_bone_segment &bs = bone_segments.front();
      return bs.fgroups.back().base_index + bs.fgroups.back().num_faces;
   }
};

//----------------------------

bool I3D_singlemesh::InitBones(C_loader &ld, CPI3D_visual root, const C_loader::S_auto_lod_data &al_data){

   assert(NumVertices1() && NumFaces1());
   if(!NumVertices1()){
      ld.REPORT_ERR(C_fstr("invalid single-mesh: '%s' (no vertices)", (const char*)root->GetName1()));
      return false;
   }
   if(!NumFaces1()){
      ld.REPORT_ERR(C_fstr("invalid single-mesh: '%s' (no faces)", (const char*)root->GetName1()));
      return false;
   }

                              //get root's inversed matrix, so that we operate in it's local coords
   const S_matrix &m_inv = root->GetInvMatrix1();

                              //info about all collected joints
   C_vector<S_SM_joint_build_info> joint_info;

                              //collect all valid joint children from provided hierarchy tree
                              //also alloc bones info and init matrices plus names
   S_collect_joints cj = {&m_inv, &joint_info, 1};
   root->EnumFrames(S_collect_joints::cbEnum, (dword)&cj, ENUMF_ALL);

   if(!joint_info.size()){
      ld.REPORT_ERR(C_fstr("invalid single-mesh: '%s' (no joints children)", (const char*)root->GetName1()));
      return false;
   }
   if(joint_info.size() > (MAX_VS_BLEND_MATRICES-1)){
      ld.REPORT_ERR(C_fstr("invalid single-mesh: '%s' (too many joints children)", (const char*)root->GetName1()));
      return false;
   }

                              //get access to vertices and faces
   dword num_faces = NumFaces1();

   C_buffer<I3D_triface> face_buf(num_faces);
   GetFaces(face_buf.begin());
   const I3D_triface *faces = face_buf.begin();
   I3D_vertex_singlemesh *verts = (I3D_vertex_singlemesh*)vertex_buffer.Lock(0);
   dword num_verts = NumVertices1();

   dword num_segments = cj.curr_break_index;
   int i;

   bone_segments.assign(num_segments);
                              //init bones
   bone_segments[0].joint_info.assign(joint_info.size());
   bone_segments[0].base_vertex = 0;
   for(i=joint_info.size(); i--; ){
      S_SM_joint_build_info &ji = joint_info[i];
      PI3D_joint joint = ji.joint;
                              //init bone
      S_SM_joint_info &bs = bone_segments[0].joint_info[i];
      bs.joint_name = joint->GetOrigName();
      bs.orig_mat_inv = ~(joint->GetMatrix() * m_inv);
   }

   bone_segments[0].num_vertices = word(num_verts);
   bone_segments[0].root_orig_mat_inv.Identity();

                              //determine vertices singularity
   C_vector<word> v_map(num_verts);
   MakeVertexMapping(&verts[0].xyz, sizeof(I3D_vertex_singlemesh), num_verts, &v_map.front());

   C_vector<S_SM_vertex_info> vertex_info(num_verts);

   AssignUniqueInsideVertices(ld, vertex_info, verts, &v_map.front(), joint_info, root, &v_map.front());
   AssignUnassignedVertices(vertex_info, verts, &v_map.front(), faces, num_faces, joint_info, root);
   StoreVertexInfo(vertex_info, joint_info, verts, num_verts, &v_map.front());

   InitBoundingVolumes(bvol_root, bone_segments[0].joint_info, joint_info, verts, root, vertex_info);

   if(num_segments > 1){
                              //mark 'break' vertices and segment index in which they appear
      for(i=num_verts; i--; ){
         word v = v_map[i];
         S_SM_vertex_info &vi = vertex_info[v];
         if(!vi.assigned)
            continue;
         dword ji = vi.joint_i;
         assert(ji<joint_info.size());
         if(joint_info[ji].bone_segment!=0 && vi.weight==1.0f){
            vi.segment_index = joint_info[ji].bone_segment;
            vertex_info[v].break_mode = S_SM_vertex_info::BREAK_TEAR_OFF;
         }
      }
      if(al_data.use && 1){
                              //create auto-LOD now

                              //mark broken verts which will be split (the ones connected with faces)
         C_buffer<bool> no_break_info(num_verts, false);
         for(dword fi=0; fi<num_faces; fi++){
            I3D_triface fc = faces[fi];
            fc.Remap(&v_map.front());
                              //check if face has broken vertex
            if(vertex_info[fc[0]].break_mode || vertex_info[fc[1]].break_mode || vertex_info[fc[2]].break_mode){
               if(!(vertex_info[fc[0]].break_mode && vertex_info[fc[1]].break_mode && vertex_info[fc[2]].break_mode)){
                              //mark all broken verts to be split
                  for(int j=3; j--; ){
                     if(vertex_info[fc[j]].break_mode){
                        //no_break_info[faces[fi][j]] = true;
                        no_break_info[fc[j]] = true;
                        //drv->DebugLine(verts[faces[fi][j]].xyz, verts[faces[fi][j]].xyz+S_vector(1, 0, 0), 0);
                     }
                  }
               }
            }
         }
                              //create auto-LOD, and re-call this function again
         vertex_buffer.Unlock();
         CreateLODs(al_data.min_num_faces, al_data.num_parts, al_data.min_dist, al_data.max_dist, false, no_break_info.begin());
         C_loader::S_auto_lod_data al1;
         al1.use = false;
         return InitBones(ld, root, al1);
      }

      C_vector<I3D_vertex_singlemesh> new_verts; new_verts.reserve(num_verts);
      C_vector<I3D_triface> new_faces; new_faces.reserve(num_faces);
      C_vector<I3D_face_group> new_fgroups; new_fgroups.reserve(fgroups.size());

      C_vector<C_vector<I3D_vertex_singlemesh> > segment_verts(num_segments);
      C_vector<C_vector<I3D_triface> > segment_faces(num_segments);
      C_vector<C_vector<I3D_face_group> > segment_fgroups(num_segments);
      C_vector<C_vector<word> > segment_remap(num_segments, C_vector<word>(num_verts, 0xffff));

                              //mark broken verts which will be split (the ones connected with faces)
                              // also reorder/move faces and fgroups
      int fgi = 0;
      for(dword fi=0; fi<num_faces; fi++){
         I3D_triface fc = faces[fi];
         fc.Remap(&v_map.front());
                              //advance fgroup index
         if(fi >= (int)(fgroups[fgi].base_index + fgroups[fgi].num_faces))
            ++fgi;
         const I3D_face_group &fg = fgroups[fgi];

                              //check if face has broken vertex
         bool face_copy = true;
         if(vertex_info[fc[0]].break_mode ||
            vertex_info[fc[1]].break_mode ||
            vertex_info[fc[2]].break_mode){

            if(vertex_info[fc[0]].break_mode &&
               vertex_info[fc[1]].break_mode &&
               vertex_info[fc[2]].break_mode){

                              //entire face is in broken area, it'll be moved to segment,
               int seg_i = vertex_info[fc[0]].segment_index;
               if(!segment_fgroups[seg_i].size() || segment_fgroups[seg_i].back().mat!=fg.mat){
                  segment_fgroups[seg_i].push_back(I3D_face_group());
                  I3D_face_group &sfg = segment_fgroups[seg_i].back();
                  sfg.base_index = segment_faces[seg_i].size();
                  sfg.mat = fg.mat;
               }
               ++segment_fgroups[seg_i].back().num_faces;
               segment_faces[seg_i].push_back(faces[fi]);
               face_copy = false;
            }else{
                              //mark all broken verts to be split
               for(int j=3; j--; ){
                  if(vertex_info[fc[j]].break_mode)
                     vertex_info[fc[j]].break_mode = S_SM_vertex_info::BREAK_SPLIT;
               }
            }
         }
         if(face_copy){
            if(!new_fgroups.size() || new_fgroups.back().mat!=fg.mat){
               new_fgroups.push_back(I3D_face_group());
               I3D_face_group &sfg = new_fgroups.back();
               sfg.base_index = new_faces.size();
               sfg.mat = fg.mat;
            }
            ++new_fgroups.back().num_faces;
            new_faces.push_back(faces[fi]);
         }
      }

      C_vector<word> v_remap(num_verts, 0xffff);

                              //copy vertices to segments
      for(dword si=0; si<num_verts; si++){
         S_SM_vertex_info &vi = vertex_info[v_map[si]];
         if(vi.break_mode != S_SM_vertex_info::BREAK_TEAR_OFF){
            v_remap[si] = word(new_verts.size());
            new_verts.push_back(verts[si]);
         }
         if(vi.break_mode != S_SM_vertex_info::BREAK_NO){
            int seg_i = vi.segment_index;
            segment_remap[seg_i][si] = word(segment_verts[seg_i].size());
            segment_verts[seg_i].push_back(verts[si]);
         }
      }
      vertex_buffer.Unlock();

                              //remap faces now
      for(fi=new_faces.size(); fi--; ){
         I3D_triface &fc = new_faces[fi];
         assert(v_remap[fc[0]]!=0xffff && v_remap[fc[1]]!=0xffff && v_remap[fc[2]]!=0xffff);
         fc.Remap(&v_remap.front());
      }
                              //store vertices of base segment
      bone_segments[0].num_vertices = word(new_verts.size());

                              //append vertices and faces of all segments, and store fgroups
      for(dword segi=1; segi<num_segments; segi++){
         S_bone_segment &seg = bone_segments[segi];
         seg.base_vertex = word(new_verts.size());
         seg.fgroups.assign(&segment_fgroups[segi].front(), &segment_fgroups[segi].front() + segment_fgroups[segi].size());
                              //remap seg's fgroups to begin with right faces
         for(int fgi=seg.fgroups.size(); fgi--; )
            seg.fgroups[fgi].base_index += new_faces.size();
                              //remap seg's faces for proper verts
         const word *v_remap_seg = &segment_remap[segi].front();
         for(fi=segment_faces[segi].size(); fi--; ){
            I3D_triface &fc = segment_faces[segi][fi];
            assert(v_remap_seg[fc[0]]!=0xffff && v_remap_seg[fc[1]]!=0xffff && v_remap_seg[fc[2]]!=0xffff);
            fc.Remap(v_remap_seg);
         }
                              //append faces
         new_faces.insert(new_faces.end(), segment_faces[segi].begin(), segment_faces[segi].end());

                              //init bone segments of the vertices
         {
            PI3D_frame joint = NULL;
            {
               for(int sgi=joint_info.size(); sgi--; ){
                  if(joint_info[sgi].bone_segment==(int)segi)
                     break;
               }
               assert(sgi!=-1);
               joint = joint_info[sgi].joint;
            }
            seg.segment_root = joint->GetOrigName();
            seg.root_orig_mat_inv = ~(joint->GetMatrix() * m_inv);
            C_vector<S_SM_joint_build_info> joint_info;
                              //collect all valid joint children from provided hierarchy tree
                              //also alloc bones info and init matrices plus names
            S_collect_joints cj = {&m_inv, &joint_info, 0};
            joint->EnumFrames(S_collect_joints::cbEnum, (dword)&cj, ENUMF_ALL);

            if(joint_info.size() > (MAX_VS_BLEND_MATRICES-1)){
               ld.REPORT_ERR(C_fstr("invalid single-mesh: '%s' (too many joints children in break segment '%s')",
                  (const char*)root->GetName1(), (const char*)joint->GetName1()));
               bone_segments.clear();
               return false;
            }

                              //init bones
            seg.joint_info.assign(joint_info.size());
            for(int i=joint_info.size(); i--; ){
               S_SM_joint_build_info &ji = joint_info[i];
               PI3D_joint joint = ji.joint;
                              //init bone
               S_SM_joint_info &bs = seg.joint_info[i];
               bs.joint_name = joint->GetOrigName();
               bs.orig_mat_inv = ~(joint->GetMatrix() * m_inv);
            }

            const I3D_triface *faces = &segment_faces[segi].front();
            dword num_faces = segment_faces[segi].size();
            I3D_vertex_singlemesh *verts = &segment_verts[segi].front();
            dword num_verts = segment_verts[segi].size();

                              //determine vertices singularity
            C_vector<word> v_map_seg(num_verts);
            MakeVertexMapping(&verts[0].xyz, sizeof(I3D_vertex_singlemesh), num_verts, &v_map_seg.front());

            C_vector<S_SM_vertex_info> vertex_info(num_verts);

            AssignUniqueInsideVertices(ld, vertex_info, verts, &v_map_seg.front(), joint_info, root, v_remap_seg);
            AssignUnassignedVertices(vertex_info, verts, &v_map_seg.front(), faces, num_faces, joint_info, root);
            StoreVertexInfo(vertex_info, joint_info, verts, num_verts, &v_map_seg.front());
         }
                              //append vertices
         new_verts.insert(new_verts.end(), segment_verts[segi].begin(), segment_verts[segi].end());
         seg.num_vertices = word(segment_verts[segi].size());
      }

      fgroups.assign(&new_fgroups.front(), &new_fgroups.front()+new_fgroups.size());
      vertex_buffer.SetVertices(new_verts);

      index_buffer.SetFaces(new_faces);
#ifdef USE_STRIPS
      strip_info.clear();
#endif

                              //make changes to all LOD levels
      for(i=1; i<(int)num_segments; i++)
         bone_segments[i].auto_lods.assign(auto_lods.size());
      for(int li=auto_lods.size(); li--; ){
         C_auto_lod &al = auto_lods[li];
         const C_buffer<I3D_face_group> &fgroups = al.fgroups;
         dword num_faces = al.NumFaces1();

         C_buffer<I3D_triface> fc_buf(num_faces);
         al.GetFaces(fc_buf.begin());
         const I3D_triface *faces = fc_buf.begin();

         C_vector<I3D_face_group> new_fgroups; new_fgroups.reserve(al.fgroups.size());
         C_vector<I3D_triface> new_faces; new_faces.reserve(num_faces);

         C_vector<C_vector<I3D_face_group> > segment_fgroups(num_segments);  //all seg's fgroups for this LOD
         C_vector<C_vector<I3D_triface> > segment_faces(num_segments);       //all seg's faces for this LOD

         int fgi = 0;
         for(dword fi=0; fi<num_faces; fi++){
            I3D_triface fc = faces[fi];
            fc.Remap(&v_map.front());
                              //advance fgroup index
            if(fi >= (int)(fgroups[fgi].base_index + fgroups[fgi].num_faces))
               ++fgi;
            const I3D_face_group &fg = fgroups[fgi];

                              //check if face has broken vertex
            bool face_copy = true;
            if(vertex_info[fc[0]].break_mode ||
               vertex_info[fc[1]].break_mode ||
               vertex_info[fc[2]].break_mode){

               if(vertex_info[fc[0]].break_mode &&
                  vertex_info[fc[1]].break_mode &&
                  vertex_info[fc[2]].break_mode){

                              //entire face is in broken area, it'll be moved to segment,
                  int seg_i = vertex_info[fc[0]].segment_index;
                  if(!segment_fgroups[seg_i].size() || segment_fgroups[seg_i].back().mat!=fg.mat){
                     segment_fgroups[seg_i].push_back(I3D_face_group());
                     I3D_face_group &sfg = segment_fgroups[seg_i].back();
                     sfg.base_index = segment_faces[seg_i].size();
                     sfg.mat = fg.mat;
                  }
                  ++segment_fgroups[seg_i].back().num_faces;
                  segment_faces[seg_i].push_back(faces[fi]);
                  face_copy = false;
               }
            }
            if(face_copy){
               if(!new_fgroups.size() || new_fgroups.back().mat!=fg.mat){
                  new_fgroups.push_back(I3D_face_group());
                  I3D_face_group &sfg = new_fgroups.back();
                  sfg.base_index = new_faces.size();
                  sfg.mat = fg.mat;
               }
               ++new_fgroups.back().num_faces;
               new_faces.push_back(faces[fi]);
            }
         }

                              //remap faces now
         for(fi=new_faces.size(); fi--; ){
            I3D_triface &fc = new_faces[fi];
            assert(v_remap[fc[0]]!=0xffff && v_remap[fc[1]]!=0xffff && v_remap[fc[2]]!=0xffff);
            fc.Remap(&v_remap.front());
         }
                              //append vertices and faces of all segments, and store fgroups
         for(dword segi=1; segi<num_segments; segi++){
            C_auto_lod &seg_al = bone_segments[segi].auto_lods[li];
            seg_al.Init(drv);
            seg_al.ratio = al.ratio;

            if(segment_fgroups[segi].size())
               seg_al.fgroups.assign(&segment_fgroups[segi].front(), &segment_fgroups[segi].front() + segment_fgroups[segi].size());
            else
               seg_al.fgroups.clear();
                              //remap seg's fgroups to begin with right faces
            for(int fgi=seg_al.fgroups.size(); fgi--; )
               seg_al.fgroups[fgi].base_index += new_faces.size();
                              //remap seg's faces for proper verts
            const word *v_remap_seg = &segment_remap[segi].front();
            for(fi=segment_faces[segi].size(); fi--; ){
               assert(v_remap_seg[segment_faces[segi][fi][0]]!=0xffff && v_remap_seg[segment_faces[segi][fi][1]]!=0xffff && v_remap_seg[segment_faces[segi][fi][2]]!=0xffff);
               segment_faces[segi][fi].Remap(v_remap_seg);
            }
                              //append faces
            new_faces.insert(new_faces.end(), segment_faces[segi].begin(), segment_faces[segi].end());

                              //count number of segment's LOD's vertices
            seg_al.vertex_count = 0;//al.vertex_count;
            for(dword fi=segment_faces[segi].size(); fi--; ){
               const I3D_triface &fc = segment_faces[segi][fi];
               for(dword vi=3; vi--; ){
                  word v = fc[vi];
                  assert(v<bone_segments[segi].num_vertices);
                  seg_al.vertex_count = Max(seg_al.vertex_count, (dword)v);
               }
            }
            ++seg_al.vertex_count;
         }
         al.fgroups.assign(&new_fgroups.front(), &new_fgroups.front()+new_fgroups.size());
         const_cast<I3D_source_index_buffer&>(al.GetIndexBuffer()).SetFaces(&new_faces.front(), new_faces.size());
#ifdef USE_STRIPS
         al.strip_info.clear();
#endif
      }
   }else{
      vertex_buffer.Unlock();

      if(al_data.use)
         CreateLODs(al_data.min_num_faces, al_data.num_parts, al_data.min_dist, al_data.max_dist, al_data.preserve_edges);
   }
   bone_segments[0].fgroups = fgroups;

   return true;
}

//----------------------------
//----------------------------

class I3D_object_singlemesh_imp: public I3D_object_singlemesh{

   class I3D_singlemesh *mesh;

                              //joint info, contains joint (the other joint is the joint's parent)
                              // also contains (temporary) matrix of the joint
   struct S_SM_object_joint_info{
      C_smart_ptr<I3D_frame> joint;

                              //joint's matrix, positioned at joint's world pos,
                              // rotated to transform joint's vertices
      S_matrix mat;              

                              //joint's matrix, expressed as quaternion and pos+scale
      S_vectorw rot;
      S_vectorw pos_scale;
   };
   struct S_bone_segment{
      C_buffer<S_SM_object_joint_info> joint_info;
      PI3D_frame root;        //root frame of the segment (NULL for main root)
   };
   C_buffer<S_bone_segment> bone_segments;

//----------------------------
// Compute blending matrices for all joints of all segments.
   void ComputeBlendMatrices(){

      const C_buffer<I3D_singlemesh::S_bone_segment> &bsgs = mesh->GetBoneSegments();
      for(dword seg_i = bsgs.size(); seg_i--; ){
         const C_buffer<I3D_singlemesh::S_SM_joint_info> &b_info = bsgs[seg_i].joint_info;
         const I3D_singlemesh::S_SM_joint_info *mesh_bs_ptr = b_info.begin();
         S_SM_object_joint_info *sm_bs_ptr = bone_segments[seg_i].joint_info.begin();

         for(int ji=b_info.size(); ji--; ){
            const I3D_singlemesh::S_SM_joint_info &bs = mesh_bs_ptr[ji];
            S_SM_object_joint_info &obs = sm_bs_ptr[ji];
            PI3D_frame joint = obs.joint;
                                    //resulting transformation matrix:
                                    // - rotate by joint's matrix (m_joint and m_joint_prnt)
                                    // - translation to joint's current world position (m_joint(3))
            const S_matrix &m_joint = joint->GetMatrix();
            S_matrix &m = obs.mat;
            m = bs.orig_mat_inv % m_joint;
            m(3) += m_joint(3);
#ifdef USE_VS_QUATERNION
            S_quat q = m;
            obs.rot = q.v;
            obs.rot.w = q.s;
            obs.pos_scale = m(3);
            obs.pos_scale.w = m(0).Magnitude();
#endif
         }
      }
      frm_flags |= FRMFLAGS_SM_BOUND_VALID;
   }

//----------------------------
// Feed joint matrices to vertex constant registers.
   void UploadBlendMatrices(dword segment_index){

      const S_SM_object_joint_info *sm_bs_ptr = bone_segments[segment_index].joint_info.begin();

      IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
      for(int i=mesh->GetBoneSegments()[segment_index].joint_info.size(); i--; ){
         const S_SM_object_joint_info &obs = sm_bs_ptr[i];
#ifdef USE_VS_QUATERNION
         d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE + i*2, (const float*)&obs.rot, 2);
#else
         S_matrix mt = obs.mat;
         mt.Transpose();
         d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE + i*3, (const float*)&mt, 3);
#endif
      }
                                 //base matrix
      S_matrix mt;
      if(!segment_index)
         mt = matrix;
      else{
         const S_matrix &m_joint = bone_segments[segment_index].root->GetMatrix();
         mt = mesh->GetBoneSegments()[segment_index].root_orig_mat_inv % m_joint;
         mt(3) += m_joint(3);
      }
#ifdef USE_VS_QUATERNION
      {
         S_vectorw vw[2];
         S_quat q = mt;
         vw[0] = q.v;
         vw[0].w = q.s;
         vw[1] = mt(3);
         vw[1].w = mt(0).Magnitude();
         d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE + bone_segments[segment_index].joint_info.size()*2, (const float*)vw, 2);
      }
#else
      mt.Transpose();
      d3d_dev->SetVertexShaderConstantF(VSC_MAT_BLEND_BASE + bone_segments[segment_index].joint_info.size()*3, (const float*)&mt, 3);
#endif
   }

//----------------------------
public:
   struct S_str_less: public less<const char*>{
      inline bool operator()(const char *x, const char *y) const{
         return (strcmp(x, y) < 0);
      }
   };
private:

//----------------------------

   void SetMesh(I3D_singlemesh *m1){

      if(m1) m1->AddRef();
      if(mesh) mesh->Release();
      mesh = m1;
                                 //init bones
      bone_segments.clear();
      if(mesh){
         const C_buffer<I3D_singlemesh::S_bone_segment> &mesh_segs = mesh->GetBoneSegments();
         bone_segments.assign(mesh_segs.size());

         typedef map<const char*, PI3D_joint, S_str_less> t_jnts;
         t_jnts jnts;
         struct S_hlp{
            static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
               typedef map<const char*, PI3D_joint, I3D_object_singlemesh_imp::S_str_less> t_jnts;
               t_jnts &jnts = *(t_jnts*)c;
               jnts[frm->GetOrigName()] = I3DCAST_JOINT(frm);
               return I3DENUMRET_OK;
            }
         };
         EnumFrames(S_hlp::cbEnum, (dword)&jnts, ENUMF_JOINT);

         for(dword si=mesh_segs.size(); si--; ){
            const I3D_singlemesh::S_bone_segment &mseg = mesh_segs[si];
            bone_segments[si].joint_info.assign(mseg.joint_info.size());
            for(dword i=0; i<mseg.joint_info.size(); i++){
               S_SM_object_joint_info &bs = bone_segments[si].joint_info[i];

               t_jnts::iterator it = jnts.find(mseg.joint_info[i].joint_name);
               assert(it!=jnts.end());
               bs.joint = it->second;

               //PI3D_joint joint = I3DCAST_JOINT(FindChildFrame(mseg.joint_info[i].joint_name, ENUMF_JOINT | ENUMF_MODEL_ORIG));
               //assert(joint);
               //bs.joint = joint;
            }
            bone_segments[si].root = NULL;
            if(mseg.segment_root.Size()){
               t_jnts::iterator it = jnts.find(mseg.segment_root);
               assert(it!=jnts.end());
               bone_segments[si].root = it->second;
               //bone_segments[si].root = FindChildFrame(mseg.segment_root, ENUMF_ALL | ENUMF_MODEL_ORIG);
            }
         }
      }
#ifndef GL
                                 //destroy old vertex buffers (if any)
      vertex_buffer.DestroyD3DVB();
#endif

      vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      frm_flags &= ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID);
   }

//----------------------------

   /*
   virtual void PrepareDestVB(I3D_mesh_base *mb, dword num_txt_stages = 1){
      I3D_visual::PrepareDestVB(mb, num_txt_stages);
   }
   */

//----------------------------

public:
   I3D_object_singlemesh_imp(PI3D_driver d):
      I3D_object_singlemesh(d),
      mesh(NULL)
   {
      visual_type = I3D_VISUAL_SINGLEMESH;
   }
   ~I3D_object_singlemesh_imp(){
      if(mesh) mesh->Release();
   }

//----------------------------

   I3DMETHOD(Duplicate)(CPI3D_frame frm){

      if(frm==this)
         return I3D_OK;
      if(frm->GetType1()!=FRAME_VISUAL)
         return I3D_frame::Duplicate(frm);

      CPI3D_visual vis = I3DCAST_CVISUAL(frm);
      I3D_RESULT ir = I3D_visual::Duplicate(vis);
      switch(vis->GetVisualType1()){
      case I3D_VISUAL_SINGLEMESH:
         {
            I3D_object_singlemesh_imp *sm = (I3D_object_singlemesh_imp*)vis;
            SetMesh((I3D_singlemesh*)sm->GetMesh());
            smesh_flags = sm->smesh_flags & SMESH_FLAGS_VBOX_VALID;
#ifdef USE_CAPCYL_JNT_VOLUMES
            vol_pos = sm->vol_pos;
            vol_dir = sm->vol_dir;
            vol_radius = sm->vol_radius;
            vol_half_len = sm->vol_half_len;
#else
            volume_box_matrix = sm->volume_box_matrix;
            volume_box = sm->volume_box;
#endif
         }
         break;
      default:
         SetMeshInternal(const_cast<PI3D_visual>(vis)->GetMesh());
      }
      return ir;
   }

//----------------------------

   I3DMETHOD(DebugDraw)(PI3D_scene) const;

//----------------------------

   I3DMETHOD_(PI3D_mesh_base,GetMesh)(){ return mesh; }
   I3DMETHOD_(CPI3D_mesh_base,GetMesh)() const{ return mesh; }

//----------------------------

   virtual bool AddRegion(const I3D_bbox &bb, const S_matrix &m, int index){
      if(index!=-1)
         return false;
      if(smesh_flags&SMESH_FLAGS_VBOX_VALID)
         return false;

#ifdef USE_CAPCYL_JNT_VOLUMES
      vol_pos = ((bb.min+bb.max) * .5f) * m;
      dword i = 0;
      if((bb.max.x-bb.min.x) < (bb.max.y-bb.min.y))
         ++i;
      if((bb.max[i]-bb.min[i]) < (bb.max.z-bb.min.z))
         i = 2;
      vol_dir.Zero();
      vol_dir[i] = (bb.max[i] - bb.min[i]) * .5f;
      vol_dir %= m;
      vol_half_len = vol_dir.Magnitude();
      vol_dir /= vol_half_len;
      ++i %= 3;
      float a = bb.max[i] - bb.min[i];
      ++i %= 3;
      float b = bb.max[i] - bb.min[i];
#if 1
      vol_radius = (a + b) * .25f;
      vol_radius *= m(0).Magnitude();
      vol_half_len -= vol_radius;
#else
      vol_radius = I3DSqrt(a*b / PI);
      vol_radius *= m(0).Magnitude();
      //vol_half_len -= vol_radius * .707f;
#endif
      assert(vol_half_len >= 0.0f);

#else

      volume_box = bb;
      volume_box_matrix = m;
#endif
      smesh_flags |= SMESH_FLAGS_VBOX_VALID;
      return true;
   }
   inline bool IsVolumeBoxValid() const{ return (smesh_flags&SMESH_FLAGS_VBOX_VALID); }
#ifdef USE_CAPCYL_JNT_VOLUMES
   inline const S_vector &GetVolumePos() const{ return vol_pos; }
   inline const S_vector &GetVolumeDir() const{ return vol_dir; }
   inline float GetVolumeRadius() const{ return vol_radius; }
   inline float GetVolumeHalfLen() const{ return vol_half_len; }
#else
   inline const I3D_bbox &GetVolumeBox1() const{ return volume_box; }
   inline const S_matrix &GetVolumeBoxMatrix1() const{ return volume_box_matrix; }
#endif

//----------------------------

   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
//----------------------------

   virtual bool ComputeBounds(){

      if(frm_flags&FRMFLAGS_SM_BOUND_VALID)
         return true;

      if(!mesh || !bone_segments.size()){
                                 //invalidate bounding volume
         bound.bound_local.bbox.Invalidate();
         bound.bound_local.bsphere.pos.Zero();
         bound.bound_local.bsphere.radius = 0.0f;
      }else{
         I3D_bound_volume &bvol = bound.bound_local;

         const S_matrix &m_inv = GetInvMatrix1();

         dword num_bones = bone_segments[0].joint_info.size();
         I3D_bsphere *bs_list = (I3D_bsphere*)alloca(sizeof(I3D_bsphere)*(num_bones+1));
         dword num_bspheres = 0;

         bvol = mesh->GetRootBounds();
      
         bs_list[num_bspheres++] = bvol.bsphere;

         if(!(frm_flags&FRMFLAGS_SM_BOUND_VALID))
            ComputeBlendMatrices();

                                 //expand bbox by all joints' bboxes
         for(int i=num_bones; i--; ){
            const I3D_singlemesh::S_SM_joint_info &bs = mesh->GetBoneSegments()[0].joint_info[i];

            if(!bs.bvol.bbox.IsValid())
               continue;

            const S_SM_object_joint_info &obs = bone_segments[0].joint_info[i];

            const S_matrix &m_joint = obs.mat;
            S_matrix m_joint_inv = m_joint * m_inv;

                                 //transform bound-box to SM's local coords
            for(int j=8; j--; ){
#ifndef USE_D3DX
               S_vector v = S_vector(bs.bvol.bbox[j&1].x, bs.bvol.bbox[(j&2)/2].y, bs.bvol.bbox[(j&4)/4].z) * m_joint_inv;
#else
               S_vectorw v = S_vector(bs.bvol.bbox[j&1].x, bs.bvol.bbox[(j&2)/2].y, bs.bvol.bbox[(j&4)/4].z);
               D3DXVec3Transform((D3DXVECTOR4*)&v, (D3DXVECTOR3*)&v, (D3DXMATRIX*)&m_joint_inv);
#endif
               bvol.bbox.min.Minimal(v);
               bvol.bbox.max.Maximal(v);
            }
         
            {
               I3D_bsphere &bs1 = bs_list[num_bspheres++];
                                 //transform bound-sphere
               float scale = I3DSqrt(m_joint_inv(0, 0)*m_joint_inv(0, 0) + m_joint_inv(1, 0)*m_joint_inv(1, 0) + m_joint_inv(2, 0)*m_joint_inv(2, 0));
               bs1.radius = bs.bvol.bsphere.radius * scale;
               bs1.pos = bs.bvol.bsphere.pos * m_joint_inv;
            }
         }
                                 //compute bounding sphere
         S_vector bbox_half_diagonal = (bvol.bbox.max - bvol.bbox.min) * .5f;
         bvol.bsphere.pos = bvol.bbox.min + bbox_half_diagonal;
         bvol.bsphere.radius = bbox_half_diagonal.Magnitude();

                                 //don't let bound-box be greater than bound-sphere
         for(i=0; i<3; i++){
            bvol.bbox.min[i] = Max(bvol.bbox.min[i], bvol.bsphere.pos[i] - bvol.bsphere.radius);
            bvol.bbox.max[i] = Min(bvol.bbox.max[i], bvol.bsphere.pos[i] + bvol.bsphere.radius);
         }
      }
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
      frm_flags |= FRMFLAGS_SM_BOUND_VALID;
      return true;
   }

//----------------------------

   virtual void SetMeshInternal(PI3D_mesh_base mb){

      if(mesh) mesh->Release();
      mesh = NULL;
      if(mb){
         mesh = new I3D_singlemesh(drv);
#ifdef USE_STRIPS
         mesh->Clone(mb, true);
#else
         mesh->Clone(mb, false);
#endif
      }
   }

//----------------------------

   virtual bool LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){

      if(mesh) mesh->Release();
      mesh = new I3D_singlemesh(drv);
      bool ok = mesh->LoadCachedInfo(ck, lc, mats);
      if(!ok){
         mesh->Release();
         mesh = NULL;
      }
      return ok;
   }

//----------------------------

   virtual bool FinalizeLoad(C_loader &ld, bool ck_load_ok, const void *al_info){

      if(!ck_load_ok){
         bool ok = mesh->InitBones(ld, this, *(C_loader::S_auto_lod_data*)al_info);
         SetMesh(mesh);
         return ok;
      }else{
         SetMesh(mesh);
      }
      return true;
   }

//----------------------------

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
};

//----------------------------

void I3D_object_singlemesh::DebugDrawVolBox(PI3D_scene scene) const{

   if(IsVolumeBoxValid()){
#ifdef USE_CAPCYL_JNT_VOLUMES
                              //draw capped cylinder volume
      S_matrix m = GetMatrix();
                              //use crazy temp matrix made of volume's pos and dir
                              // (slow, but saves redundant data stored in joint)
      S_matrix m1; m1.Identity();
      m1(2) = vol_dir;
      m1(1) = vol_dir.Cross(S_vector(vol_dir.x, vol_dir.z, vol_dir.y));
      if(I3DFabs(1.0f-m1(1).Square()) > .04f) m1(1) = vol_dir.Cross(S_vector(vol_dir.y, vol_dir.x, vol_dir.z));
      assert(I3DFabs(1.0f-m1(1).Square()) < .04f);
      m1(0).GetNormal(m1(1), m1(3), m1(2));
      m1(3) = vol_pos;

      scene->DebugDrawCylinder(m1 * m, vol_radius, vol_half_len, 0x40ff0000, true);
#else
      static const word sides_i[] = {
         1, 3, 7,  7, 5, 1,
         4, 6, 2,  2, 0, 4,
         2, 6, 7,  7, 3, 2,
         4, 0, 1,  1, 5, 4,

         0, 2, 3,  3, 1, 0,
         7, 6, 5,  6, 4, 5,
      };

      drv->SetTexture(NULL);
      S_vector bbox_full[8];
      GetVolumeBox1().Expand(bbox_full);
      scene->SetRenderMatrix(GetVolumeBoxMatrix1() * GetMatrix());
                              //draw sides
      scene->DrawTriangles(bbox_full, 8, I3DVC_XYZ, sides_i, sizeof(sides_i)/sizeof(word), 0x40ff0000);
                              //draw edges
      scene->DebugDrawBBox(bbox_full, 0x80ff0000);
#endif
   }
}

//----------------------------

I3D_RESULT I3D_object_singlemesh_imp::DebugDraw(PI3D_scene scene) const{

   if(!mesh)
      return I3DERR_INVALIDPARAMS;
   int i;

   //static const S_vector color(1.0f, .7f, 0.0f);
   const dword color = 0xffb000;
   byte alpha = 0x20;
   S_vector bbox_full[8];
                              //paint root
   scene->SetRenderMatrix(matrix);
   mesh->GetRootBounds().bbox.Expand(bbox_full);
   scene->DebugDrawBBox(bbox_full, 0x20000000 | color);
   {
      S_matrix m = matrix;
      m(3) += mesh->GetRootBounds().bsphere.pos.RotateByMatrix(m);
      scene->DebugDrawSphere(m, mesh->GetRootBounds().bsphere.radius, (alpha<<24) | color);
   }

   if(mesh->GetBoneSegments().size())
   for(i=mesh->GetBoneSegments()[0].joint_info.size(); i--; ){
      const I3D_singlemesh::S_SM_joint_info &bs = mesh->GetBoneSegments()[0].joint_info[i];
      const S_SM_object_joint_info &obs = bone_segments[0].joint_info[i];

      scene->SetRenderMatrix(obs.mat);

      bs.bvol.bbox.Expand(bbox_full);
      scene->DebugDrawBBox(bbox_full, 0x20000000 | color);

      {
         S_matrix m = obs.mat;
         m(3) += bs.bvol.bsphere.pos.RotateByMatrix(m);
         scene->DebugDrawSphere(m, bs.bvol.bsphere.radius, (alpha<<24) | color);
      }
   }

   if(drv->GetFlags2()&DRVF2_DRAWVOLUMES)
      DebugDrawVolBox(scene);
   return I3D_OK;
}

//----------------------------

void I3D_object_singlemesh_imp::AddPrimitives(S_preprocess_context &pc){

   if(!mesh)
      return;
   
   int curr_auto_lod = -1;
                              //add all segments
   for(dword segi=0; segi<bone_segments.size(); segi++){
      const C_auto_lod *auto_lods = mesh->GetAutoLODs(segi);
      if(!segi){
         if(drv->force_lod_index==-1){
                              //compute automatic LOD based on distance
            curr_auto_lod = GetAutoLOD(pc, mesh);
         }else{
            curr_auto_lod = Min(drv->force_lod_index, mesh->NumAutoLODs(segi)-1);
         }
      }
      const C_buffer<I3D_singlemesh::S_bone_segment> &bsegs = mesh->GetBoneSegments();
                              //get LOD's vertex count and face source
      const C_buffer<I3D_face_group> *fgrps;
      int vertex_count;
      if(curr_auto_lod < 0){
         fgrps = &bsegs[segi].fgroups;
         vertex_count = bsegs[segi].num_vertices;
      }else{
         fgrps = &auto_lods[curr_auto_lod].fgroups;
         vertex_count = auto_lods[curr_auto_lod].vertex_count;
      }
      if(!vertex_count || !fgrps->size())
         continue;

                              //add primitives to sort list
      const I3D_face_group &fg = fgrps->front();
      CPI3D_material mat = fg.GetMaterial1();

         //dword face_count = fg.NumFaces1();
         //assert(face_count);

      /*
                              //driver stats
      {
         const I3D_face_group &fg = fgrps->back();
         pc.scene->render_stats.triangle += fg.base_index + fg.num_faces;
      }
      */

      pc.prim_list.push_back(S_render_primitive(curr_auto_lod, alpha, this, pc));
      S_render_primitive &p = pc.prim_list.back();
   
      bool is_alpha = false;
      dword sort_value;
                              //determine blending mode
      p.blend_mode = I3DBLEND_OPAQUE;
      
      if(alpha!=255){
         p.blend_mode = mat->IsAddMode() ? I3DBLEND_ADD : I3DBLEND_ALPHABLEND;
         is_alpha = true;
         sort_value = PRIM_SORT_ALPHA_NOZWRITE;
         ++pc.alpha_nozwrite;
      }else
      if(!mat->IsTransl()){
         sort_value = PRIM_SORT_OPAQUE;
         ++pc.opaque;
      }else
      if(mat->IsCkeyAlpha1()){
         p.blend_mode = I3DBLEND_ALPHABLEND;
         sort_value = PRIM_SORT_CKEY_ALPHA;
         ++pc.ckey_alpha;
         is_alpha = true;
      }else
      if(mat->GetMirrorID1() != -1){
         if(drv->CanRenderMirrors()){
            pc.prim_list.pop_back();
            AddMirrorData(pc, mat, curr_auto_lod);
            continue;
         }else{
            sort_value = PRIM_SORT_OPAQUE;
            ++pc.opaque;
         }
      }else{
         p.blend_mode = mat->IsAddMode() ? I3DBLEND_ADD : I3DBLEND_ALPHABLEND;
         is_alpha = true;
         sort_value = PRIM_SORT_ALPHA_NOZWRITE;
         ++pc.alpha_nozwrite;
      }
      p.user1 = segi;

      if(!is_alpha){
                           //sort by material
         //sort_value |= (mp->GetSortID()&PRIM_SORT_MAT_MASK)<<PRIM_SORT_MAT_SHIFT;
         sort_value |= (mat->GetSortID()&0xffffff)<<PRIM_SORT_MAT_SHIFT;
                           //also sort primarily by segment index
         sort_value |= (segi & 0x1f) << 24;
      }else{
                           //sort by distance
         const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
         const S_matrix &m = pc.scene->GetActiveCamera1()->GetViewMatrix1();
         float depth =
            bsphere.pos[0] * m(0, 2) +
            bsphere.pos[1] * m(1, 2) +
            bsphere.pos[2] * m(2, 2) +
                             m(3, 2);
         sort_value |= ((FloatToInt(depth * 100.0f))&PRIM_SORT_DIST_MASK) << PRIM_SORT_DIST_SHIFT;
      }
      p.sort_value = sort_value;
                              //driver's stats
      pc.scene->render_stats.vert_trans += vertex_count;
   }
                              //make sure destination VB is allocated
   if(NeedPrepareDestVb())
      PrepareDestVB(mesh);
}

//----------------------------
#ifndef GL
void I3D_object_singlemesh_imp::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   const int segment_index = rp.user1;
   const I3D_singlemesh::S_bone_segment &bseg = mesh->GetBoneSegments()[segment_index];
   dword base_segment_vertex = bseg.base_vertex;

   {
      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }

      bool do_matrix_blend = true;
      if(segment_index!=0){
         PI3D_frame root = bone_segments[segment_index].root;
         assert(root);
         if(root->GetType1()==FRAME_JOINT){
            PI3D_joint jnt = I3DCAST_JOINT(root);
            float hdist = jnt->GetHideDistance();
            if(I3DFloatAsInt(hdist)!=0){
               PI3D_camera cam = pc.scene->GetActiveCamera1();
               float d = (cam->GetMatrixDirect()(3) - root->GetMatrixDirect()(3)).Magnitude();
               d *= pc.LOD_factor;
               do_matrix_blend = (d < hdist);
            }
         }
      }
                              //transform & light vertices

      dword prep_flags = VSPREP_COPY_UV | VSPREP_MAKELIGHTING;
      I3D_driver::S_vs_shader_entry_in se;

      if(!(frm_flags&FRMFLAGS_SM_BOUND_VALID))
         ComputeBlendMatrices();
      if(do_matrix_blend){
         UploadBlendMatrices(segment_index);
         prep_flags |= VSPREP_WORLD_SPACE;
                              //fill-in transposed view-projection matrix
         d3d_dev->SetVertexShaderConstantF(VSC_MAT_TRANSFORM_0, (const float*)&pc.scene->GetViewProjHomMatTransposed(), 4);

         se.AddFragment(VSF_M_PALETTE_TRANSFORM);
         PrepareVertexShader(bseg.fgroups[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);
      }else{
         prep_flags |= VSPREP_TRANSFORM | VSPREP_FEED_MATRIX;
         S_matrix m;
         const S_matrix &m_joint = bone_segments[segment_index].root->GetMatrixDirect();
         m = bseg.root_orig_mat_inv % m_joint;
         m(3) += m_joint(3);
         PrepareVertexShader(bseg.fgroups[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags, &m);
      }

      if(!drv->IsDirectTransform()){
         drv->DisableTextureStage(1);

         dword vertex_count;
         const C_auto_lod *auto_lods = mesh->GetAutoLODs(segment_index);
         if(rp.curr_auto_lod < 0){
            vertex_count = bseg.num_vertices;
         }else{
            //vertex_count = bseg.auto_lods[rp.curr_auto_lod].vertex_count;
            vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;
         }
         assert(base_segment_vertex + vertex_count <= mesh->vertex_buffer.NumVertices());

         drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.GetSizeOfVertex());
         drv->SetVSDecl(vertex_buffer.vs_decl);

         hr = d3d_dev->ProcessVertices(mesh->vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
            vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex, vertex_count,
            vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
         CHECK_D3D_RESULT("ProcessVertices", hr);
         drv->SetVertexShader(NULL);
      }
   }

                              //get LOD's vertex count and face source
   const C_buffer<I3D_face_group> *fgrps;
   int vertex_count;
   const C_buffer<I3D_singlemesh::S_bone_segment> &bsegs = mesh->GetBoneSegments();
   const C_auto_lod *auto_lods = mesh->GetAutoLODs(segment_index);
   if(rp.curr_auto_lod < 0){
      fgrps = &bsegs[segment_index].fgroups;
      vertex_count = bsegs[segment_index].num_vertices;
   }else{
      fgrps = &auto_lods[rp.curr_auto_lod].fgroups;
      vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;
   }
   if(!vertex_count)
      return;

   for(int fgi=fgrps->size(); fgi--; ){

      const I3D_face_group *fg;
      dword vertex_count;
      dword base_index;
#ifdef USE_STRIPS
      const S_fgroup_strip_info *strp_info;
#endif
      if(rp.curr_auto_lod < 0){
                                 //direct mesh
         fg = &bseg.fgroups[fgi];
         vertex_count = bseg.num_vertices;
         base_index = mesh->GetIndexBuffer().D3D_index_buffer_index;
         drv->SetIndices(mesh->GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
         strp_info = mesh->GetStripInfo();
#endif
      }else{
         //const C_auto_lod &al = mesh->GetAutoLODs(segment_index)[rp.curr_auto_lod];
         const C_auto_lod &al = mesh->GetAutoLODs(segment_index)[rp.curr_auto_lod];
         fg = &al.fgroups[fgi];
         vertex_count = al.vertex_count;
         {
            const C_auto_lod &al = mesh->GetAutoLODs(0)[rp.curr_auto_lod];
            base_index = al.GetIndexBuffer().D3D_index_buffer_index;
            drv->SetIndices(al.GetIndexBuffer().GetD3DIndexBuffer());
         }
#ifdef USE_STRIPS
         strp_info = al.GetStripInfo();
#endif
      }
      pc.scene->render_stats.triangle += fg->num_faces;

      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
      drv->SetFVF(vertex_buffer.GetFVFlags());



      CPI3D_material mat = fg->GetMaterial1();
      if(pc.mode!=RV_SHADOW_CASTER){
         drv->SetRenderMat(mat, 0, (float)rp.alpha * R_255);
         drv->SetupBlend(rp.blend_mode);
         drv->SetupTextureStage(0, drv->vertex_light_blend_op);
         SetupSpecialMapping(mat, &rp, 1);

         drv->EnableAnisotropy(0, true);
      }else{
         if(mat->IsTextureAlpha() || mat->IsCkeyAlpha1()){
            CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
            drv->SetTexture1(0, tb);
            if(tb){
               dword ar = (tb->GetTxtFlags()&TXTF_MIPMAP) ? 0x60 : 0xe0;
               ar = ar * rp.alpha / 255;
               drv->SetAlphaRef(ar);
            }
         }else{
            drv->SetTexture1(0, NULL);
         }
         drv->EnableNoCull(mat->Is2Sided1());
         if(rp.alpha!=0xff){
            dword save_tf = drv->last_texture_factor;
            dword fc = ((save_tf&0xff) * rp.alpha) / 0xff;
            drv->SetTextureFactor(0xff000000 | (fc<<16) | (fc<<8) | fc);

            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
               0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
            CHECK_D3D_RESULT("DrawIP", hr);

            drv->SetTextureFactor(save_tf);
            return;
         }
      }
                                 //debug mode - without textures
      if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES)){
                                 //use factor (50% gray) instead of texture
         drv->SetTexture1(0, NULL);
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
         drv->SetTextureFactor(0xff808080);

         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
            0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
         CHECK_D3D_RESULT("DrawIP", hr);
         d3d_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
         return;
      }

#if defined USE_STRIPS
      if(strp_info){
         const S_fgroup_strip_info &si = strp_info[fgi];
         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
            0, vertex_count, base_index*3 + si.base_index, si.num_indicies-2);
      }else
#endif
         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
            0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}
#endif
//----------------------------

void I3D_object_singlemesh_imp::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   const int segment_index = rp.user1;
   const I3D_singlemesh::S_bone_segment &bseg = mesh->GetBoneSegments()[segment_index];
   dword base_segment_vertex = bseg.base_vertex;

   {
      bool do_matrix_blend = true;
      if(segment_index!=0){
         PI3D_frame root = bone_segments[segment_index].root;
         assert(root);
         if(root->GetType1()==FRAME_JOINT){
            PI3D_joint jnt = I3DCAST_JOINT(root);
            float hdist = jnt->GetHideDistance();
            if(I3DFloatAsInt(hdist)!=0){
               PI3D_camera cam = pc.scene->GetActiveCamera1();
               float d = (cam->GetMatrixDirect()(3) - root->GetMatrixDirect()(3)).Magnitude();
               d *= pc.LOD_factor;
               do_matrix_blend = (d < hdist);
            }
         }
      }
      if(!(frm_flags&FRMFLAGS_SM_BOUND_VALID))
         ComputeBlendMatrices();
                              //transform & light vertices

      dword prep_flags = VSPREP_COPY_UV | VSPREP_MAKELIGHTING;
      I3D_driver::S_vs_shader_entry_in se;

      if(do_matrix_blend){
         UploadBlendMatrices(segment_index);
         prep_flags |= VSPREP_WORLD_SPACE;
                              //fill-in transposed view-projection matrix
         d3d_dev->SetVertexShaderConstantF(VSC_MAT_TRANSFORM_0, (const float*)&pc.scene->GetViewProjHomMatTransposed(), 4);

         se.AddFragment(VSF_M_PALETTE_TRANSFORM);
         PrepareVertexShader(bseg.fgroups[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);
      }else{
         prep_flags |= VSPREP_TRANSFORM | VSPREP_FEED_MATRIX;
         S_matrix m;
         const S_matrix &m_joint = bone_segments[segment_index].root->GetMatrixDirect();
         m = bseg.root_orig_mat_inv % m_joint;
         m(3) += m_joint(3);
         PrepareVertexShader(bseg.fgroups[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags, &m);
      }
   }

   const C_buffer<I3D_face_group> *fgrps;
   int vertex_count;
   const C_buffer<I3D_singlemesh::S_bone_segment> &bsegs = mesh->GetBoneSegments();
   const C_auto_lod *auto_lods = mesh->GetAutoLODs(segment_index);
   if(rp.curr_auto_lod < 0){
      fgrps = &bsegs[segment_index].fgroups;
      vertex_count = bsegs[segment_index].num_vertices;
   }else{
      fgrps = &auto_lods[rp.curr_auto_lod].fgroups;
      vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;
   }
   if(!vertex_count)
      return;

   for(int fgi=fgrps->size(); fgi--; ){

      const I3D_face_group *fg;
      dword vertex_count;
      dword base_index;
#ifdef USE_STRIPS
      const S_fgroup_strip_info *strp_info;
#endif
      if(rp.curr_auto_lod < 0){
                                 //direct mesh
         fg = &bseg.fgroups[fgi];
         vertex_count = bseg.num_vertices;
         base_index = mesh->GetIndexBuffer().D3D_index_buffer_index;
         drv->SetIndices(mesh->GetIndexBuffer().GetD3DIndexBuffer());
#ifdef USE_STRIPS
         strp_info = mesh->GetStripInfo();
#endif
      }else{
         const C_auto_lod &al = mesh->GetAutoLODs(segment_index)[rp.curr_auto_lod];
         fg = &al.fgroups[fgi];
         vertex_count = al.vertex_count;
         {
            const C_auto_lod &al = mesh->GetAutoLODs(0)[rp.curr_auto_lod];
            base_index = al.GetIndexBuffer().D3D_index_buffer_index;
            drv->SetIndices(al.GetIndexBuffer().GetD3DIndexBuffer());
         }
#ifdef USE_STRIPS
         strp_info = al.GetStripInfo();
#endif
      }
      pc.scene->render_stats.triangle += fg->num_faces;

#ifndef GL
      drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.GetSizeOfVertex());
      drv->SetVSDecl(vertex_buffer.vs_decl);
#else
      drv->SetStreamSource(mesh->vertex_buffer.GetD3DVertexBuffer(), mesh->vertex_buffer.GetSizeOfVertex());
      drv->SetVSDecl(mesh->vertex_buffer.vs_decl);
#endif

      CPI3D_material mat = fg->GetMaterial1();

      I3D_driver::S_ps_shader_entry_in se_ps;

      bool has_diffuse = (rp.flags&RP_FLAG_DIFFUSE_VALID);
   
#ifndef GL
      if(pc.mode==RV_SHADOW_CASTER){
         if(mat->IsTextureAlpha() || mat->IsCkeyAlpha1()){
            se_ps.Tex(0);
            se_ps.AddFragment(PSF_SHADOW_CAST);

            CPI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
            drv->SetTexture1(0, tb);
            if(tb){
               dword ar = (tb->GetTxtFlags()&TXTF_MIPMAP) ? 0x60 : 0xe0;
               ar = ar * rp.alpha / 255;
               drv->SetAlphaRef(ar);
            }
         }else{
            se_ps.AddFragment(PSF_COLOR_COPY);
         }
         drv->EnableNoCull(mat->Is2Sided1());
         if(rp.alpha!=0xff){
            drv->SetPixelShader(se_ps);

            float o = (pc.shadow_opacity * (float)rp.alpha) * R_255;
            const S_vectorw c(o, o, o, 1.0f);
            drv->SetPSConstant(PSC_COLOR, &c);

            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
               0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
            CHECK_D3D_RESULT("DrawIP", hr);
            const S_vectorw c1(pc.shadow_opacity, pc.shadow_opacity, pc.shadow_opacity, 1.0f);
            drv->SetPSConstant(PSC_COLOR, &c1);
            continue;
         }
      }else
#endif
      {
         drv->SetupBlend(rp.blend_mode);

         if((drv->GetFlags2()&DRVF2_DRAWTEXTURES) && mat->GetTexture1(MTI_DIFFUSE)){
            se_ps.Tex(0);
            se_ps.AddFragment(has_diffuse ? PSF_MODX2_t0_v0 : PSF_COPY_BLACK_t0a);
            drv->SetRenderMat(mat, 0, (float)rp.alpha * R_255);
            drv->EnableAnisotropy(0, true);
            SetupSpecialMappingPS(mat, se_ps, 1);
         }else{
            se_ps.AddFragment(has_diffuse ? PSF_v0_COPY : PSF_COPY_BLACK);
#ifndef GL
            if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
               se_ps.TexKill(1);
#endif
            drv->DisableTextures(0);
         }
      }
      drv->SetPixelShader(se_ps);

#if defined USE_STRIPS
      if(strp_info){
         const S_fgroup_strip_info &si = strp_info[fgi];
         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
            0, vertex_count, base_index*3 + si.base_index, si.num_indicies-2);
      }else
#endif
         hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
#ifndef GL
         vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
#else
         mesh->vertex_buffer.D3D_vertex_buffer_index + base_segment_vertex,
#endif
            0, vertex_count, (base_index + fg->base_index) * 3, fg->num_faces);
      CHECK_D3D_RESULT("DrawIP", hr);
   }
}

//----------------------------
//----------------------------

I3D_visual *CreateSingleMesh(PI3D_driver drv){
   return new I3D_object_singlemesh_imp(drv);
}

//----------------------------
//----------------------------


