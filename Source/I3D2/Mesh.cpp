/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Mesh.cpp
   Content: Mesh classes and mesh utility functions.
--------------------------------------------------------*/

#include "all.h"
#include "mesh.h"
#include "visual.h"
#ifndef GL
#include "stripifier.h"
#endif
#include <list>
#include <algorithm>

//----------------------------

#ifndef GL
#define USE_SHARED_DEST_VB
#endif

#ifdef USE_STRIPS
#define DBASE_VERSION 0x003c  //version of I3D_mesh::SaveCachedInfo
#else
#define DBASE_VERSION 0x0001  //version of I3D_mesh::SaveCachedInfo
#endif
                              
const float SAFE_BOUND_THRESH = .001f; //make bounding volume slightly greater, to be safe
const float BOUND_EXPAND = .01f;       //during expansion, add this value to minimize further expands

                              //debugging:
//#define PAINT_VERTEX_NORMALS

//----------------------------
// Determine which vertices are identical (by examining position) with others
//    in the pool, and create mapping array                                    
void MakeVertexMapping(const S_vector *vp, dword pitch, dword numv, word *v_map, float thresh){

   if(!numv)
      return;
#if defined _MSC_VER && 0

   __asm{
      push ecx

      mov ecx, 0
      mov esi, vp
      mov ebx, v_map
      push eax
      mov eax, thresh
   l1:
      mov edx, 0
      cmp edx, ecx
      jz sk1
      mov edi, vp
   l2:
      fld dword ptr[esi+0]
      fsub dword ptr[edi+0]
      fabs
      
      fld dword ptr[esi+4]
      fsub dword ptr[edi+4]
      fabs
      faddp st(1), st
      
      fld dword ptr[esi+8]
      fsub dword ptr[edi+8]
      fabs
      faddp st(1), st
      fstp dword ptr[esp]

      cmp eax, [esp]
      ja sk1

      add edi, pitch
      inc edx
      cmp edx, ecx
      jnz l2
   sk1:
      mov [ebx+ecx*2], dx
      add esi, pitch
      inc ecx
      cmp ecx, numv
      jne l1
      pop eax

      pop ecx
   }

#else
                              //sort vertices by one axis
   C_sort_list<int> sorted_vertes(numv);
   for(dword i=numv; i--; ){
      const S_vector &v = *(S_vector*)(((byte*)vp)+pitch*i);
      sorted_vertes.Add(i, FloatToInt(v.x * 1000.0f));
   }
   sorted_vertes.Sort();
                              //go through all vertices in the array, search neighbours until thresh
   for(i=numv; i--; ){
                              //get vertex index
      int vi = sorted_vertes[i];
                              //already processed?
      if(vi==-1)
         continue;
                              //originally map it to itself
      v_map[vi] = (word)vi;
      const S_vector &v = *(S_vector*)(((byte*)vp)+pitch*vi);
                              //check neighbour vertices down
      for(dword i1=i; i1--; ){
         int vi1 = sorted_vertes[i1];
                              //already processed?
         if(vi1==-1)
            continue;
         const S_vector &v1 = *(S_vector*)(((byte*)vp)+pitch*vi1);
                              //first check distance on the sortex axis, break as soon as it's greater than thresh
         float delta = I3DFabs(v1.x - v.x);
         if(delta >= thresh)
            break;
         delta += I3DFabs(v1.y - v.y) + I3DFabs(v1.z - v.z);
         if(delta < thresh){
            if(vi < vi1){
               sorted_vertes[(int)i1] = -1;
               v_map[vi1] = word(vi);
            }else{
               v_map[vi] = word(vi1);
            }
         }
      }
   }
                              //fix-up - go from bottom up, make always higher index point down
   for(i=0; i<numv; i++){
      word vi = v_map[i];
      if(vi > i){
                              //point higher index to lower index
         v_map[vi] = word(i);
         v_map[i] = word(i);
      }else
      if(vi!=i){
                              //point to original vertex only
         while(v_map[vi] != vi)
            vi = v_map[vi];
         v_map[i] = vi;
      }
   }

#ifdef _DEBUG
   for(i=numv; i--; ){
      word vi = v_map[i];
      assert(vi <= i);
      assert(v_map[vi] == vi);
   }
#endif
   /*
   for(dword i=0; i<numv; i++){
      const S_vector &v1 = *(S_vector*)(((byte*)vp) + pitch * i);
      for(dword j=0; j<i; j++){
         const S_vector &v2 = *(S_vector*)(((byte*)vp) + pitch * j);
         if((I3DFabs(v1.x-v2.x) + I3DFabs(v1.y-v2.y) + I3DFabs(v1.z-v2.z)) < thresh)
            break;
      }
      v_map[i] = (word)j;
   }
   */

#endif
}

//----------------------------

static bool operator <(const I3D_edge &e1, const I3D_edge &e2){
   return ( (*(dword*)&e1) < (*(dword*)&e2) );
}

//----------------------------
// Check if set of vertices are planar, use specified thresh to determine
bool CheckIfVerticesPlanar(const S_vector *verts, dword num_verts, float thresh){
                              
   assert(num_verts >= 3);
   float planear_err = 0.0f;
   S_vector normal;
   normal.GetNormal(verts[0], verts[1], verts[2]);
   for(dword j=1; j<num_verts; j++){
      S_vector n1;
      n1.GetNormal(verts[j], verts[(j+1)%num_verts], verts[(j+2)%num_verts]);
      planear_err = Max(planear_err, I3DFabs(normal.AngleTo(n1)));
      normal = n1;
   }
   return (planear_err <= thresh);
}

//----------------------------
// Build normals of list of faces
void BuildFaceNormals(const S_vector *verts, const I3D_triface *faces, dword num_faces, S_vector *normals){

   for(int i=num_faces; i--; ){
      const I3D_triface &fc = faces[i];
      S_vector &n = normals[i];
      n.GetNormal(verts[fc[0]], verts[fc[1]], verts[fc[2]]);
      n.Normalize();
   }
}

//----------------------------
// Find specified face (given by 2 indicies) in list, return index,
// or -1 if not found, if found, fill third index.
int FindFace(const I3D_triface *faces, dword num_faces, word e1, word e2, word &e3){

   for(int i=num_faces; i--; ){
      const I3D_triface &fc = faces[i];
      if(fc[0]==e1 && fc[1]==e2){ e3=fc[2]; break; }
      if(fc[1]==e1 && fc[2]==e2){ e3=fc[0]; break; }
      if(fc[2]==e1 && fc[0]==e2){ e3=fc[1]; break; }
   }
   return i;
}
//----------------------------
// Find specified face (given by 3 indicies) in list, return index,
// or -1 if not found.
int FindFace(const I3D_triface *faces, dword num_faces, const I3D_triface &fc1){

   for(int i=num_faces; i--; ){
      const I3D_triface &fc = faces[i];
      if(fc[0]==fc1[0] && fc[1]==fc1[1] && fc[2]==fc1[2]) break;
      if(fc[0]==fc1[1] && fc[1]==fc1[2] && fc[2]==fc1[0]) break;
      if(fc[0]==fc1[2] && fc[1]==fc1[0] && fc[2]==fc1[1]) break;
   }
   return i;
}

//----------------------------

void CleanMesh(C_vector<S_vector> &verts, C_vector<I3D_triface> &faces){

   int i;
                              //mark used vertices
   C_vector<word> v_map; v_map.assign(verts.size());
   C_vector<bool> v_use; v_use.assign(verts.size(), false);

   for(i=faces.size(); i--; ){
      for(int j=3; j--; )
         v_use[faces[i][j]] = true;
   }
   int new_count = verts.size();
                              //process all verts, remove unused
   for(i=0; i<new_count; i++){
      v_map[i] = word(i);     //by default vertex stays where it is
      if(!v_use[i]){
                              //vertex from back is moved forward
         --new_count;
         v_map[new_count] = word(i);
         verts[i] = verts[new_count];
         v_use[i] = v_use[new_count];
         --i;
      }
   }
                              //strip verts
   verts.erase(verts.begin()+new_count, verts.end());
                              //remap faces
   for(i=faces.size(); i--; ){
      for(int j=3; j--; )
         faces[i][j] = v_map[faces[i][j]];
   }
}

//----------------------------

void CleanMesh(C_vector<S_vector> &verts, C_vector<I3D_face> &faces){

   int i;
                              //mark used vertices
   C_vector<word> v_map; v_map.assign(verts.size());
   C_vector<bool> v_use; v_use.assign(verts.size(), false);

   for(i=faces.size(); i--; ){
      const I3D_face &fc = faces[i];
      for(int j=fc.num_points; j--; )
         v_use[fc[j]] = true;
   }
   int new_count = verts.size();
                              //process all verts, remove unused
   for(i=0; i<new_count; i++){
      v_map[i] = word(i);     //by default vertex stays where it is
      if(!v_use[i]){
                              //vertex from back is moved forward
         --new_count;
         v_map[new_count] = word(i);
         verts[i] = verts[new_count];
         v_use[i] = v_use[new_count];
         --i;
      }
   }
                              //strip verts
   verts.erase(verts.begin()+new_count, verts.end());
                              //remap faces
   for(i=faces.size(); i--; ){
      I3D_face &fc = faces[i];
      for(int j=fc.num_points; j--; )
         fc[j] = v_map[fc[j]];
   }
}

//----------------------------
// Make clone of src VB - copy common elements into dst, leave others.
static void CloneVertexBuffer(const void *src1, dword src_fvf, const void *dst1, dword dst_fvf, dword num_vertices){

   const byte *src = (const byte*)src1;
   byte *dst = (byte*)dst1;

                              //for now, support only 2D texture coordinates
   assert(!(src_fvf&0xffff0000));
   assert(!(dst_fvf&0xffff0000));
   dword src_stride = GetSizeOfVertex(src_fvf);
   dword dst_stride = GetSizeOfVertex(dst_fvf);

   dword src_pos_bytes = GetSizeOfVertex(src_fvf&D3DFVF_POSITION_MASK);
   dword dst_pos_bytes = GetSizeOfVertex(dst_fvf&D3DFVF_POSITION_MASK);
   dword pos_bytes = Min(src_pos_bytes, dst_pos_bytes);
   dword src_uv_bytes = ((src_fvf&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT) * sizeof(I3D_text_coor);
   dword dst_uv_bytes = ((dst_fvf&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT) * sizeof(I3D_text_coor);
   dword uv_coord_bytes = Min(src_uv_bytes, dst_uv_bytes);
   for(int i=num_vertices; i--; ){
      const byte *p_src = src;
      byte *p_dst = dst;
                           //copy position
      memcpy(p_dst, p_src, pos_bytes);
      p_src += src_pos_bytes;
      p_dst += dst_pos_bytes;
                           //now copy others elements of the vertex
      if(dst_fvf&D3DFVF_NORMAL){
         if(src_fvf&D3DFVF_NORMAL)
            *(S_vector*)p_dst = *(S_vector*)p_src;
         p_dst += sizeof(S_vector);
      }
      if(src_fvf&D3DFVF_NORMAL)
         p_src += sizeof(S_vector);

      if(dst_fvf&D3DFVF_DIFFUSE){
         if(src_fvf&D3DFVF_DIFFUSE)
            *(dword*)p_dst = *(dword*)p_src;
         p_dst += sizeof(dword);
      }
      if(src_fvf&D3DFVF_DIFFUSE)
         p_src += sizeof(dword);

      if(dst_fvf&D3DFVF_SPECULAR){
         if(src_fvf&D3DFVF_SPECULAR)
            *(dword*)p_dst = *(dword*)p_src;
         p_dst += sizeof(dword);
      }
      if(src_fvf&D3DFVF_SPECULAR)
         p_src += sizeof(dword);

      memcpy(p_dst, p_src, uv_coord_bytes);
      src += src_stride;
      dst += dst_stride;
   }
}

//----------------------------

static void CreateUniqueVMap(const void *out_verts, dword vstride, dword num_verts, C_vector<word> v_map){

   v_map.assign(num_verts, 0);
                              //re-build vertex-map info
   MakeVertexMapping((const S_vector*)out_verts, vstride, num_verts, &v_map.front());
   if(v_map.size()){
                           //determine number of vertices mapped to themselves
      for(dword i=0; i<v_map.size(); i++){
         if(v_map[i] != i)
            break;
      }
                              //strip those from the pool save space
      v_map.erase(v_map.begin(), v_map.begin()+i);
   }
}

//----------------------------
//Reorganize vertices, so that first removed are at back.
// Parameters:
//    num_verts ... number of vertices
//    vstride ... vertex stride
//    vertex_remove_order ... list of vertices in order in which they were removed (optimized by LOD)
//    v_remap ... array which will contain mapping to shuffled indices when the call returns
//    out_verts ... output vertex pool, which will contain ordered vertices
static void ReorderVertices(const void *in_verts, dword num_verts, dword vstride,
   const C_vector<word> &vertex_remove_order,
   C_vector<word> &v_remap, void *out_verts){

   v_remap.assign(num_verts, 0xffff);

                           //put vertices to back, in order in which they were removed
   for(dword i=0, j=num_verts-1; i<vertex_remove_order.size(); i++, --j){
      int vi = vertex_remove_order[i];
      memcpy(j*vstride + (byte*)out_verts, vi*vstride + (const byte*)in_verts, vstride);
      assert(v_remap[vi]==0xffff);  //make sure it's not assigned yet
      v_remap[vi] = word(j);
   }
   for(j=j, i=num_verts; i--; ){
      if(v_remap[i]==0xffff){
         memcpy(j*vstride + (byte*)out_verts, i*vstride + (const byte*)in_verts, vstride);
         v_remap[i] = word(j);
         --j;
      }
   }
   assert(j==-1);          //make sure we've put all vertices back into list
}

//----------------------------

bool SaveCachedInfoFGroups(C_cache *ck, const C_buffer<I3D_face_group> &fgroups, const C_vector<C_smart_ptr<I3D_material> > &mats){

   dword num = fgroups.size();
   ck->write(&num, sizeof(word));
   for(dword i=0; i<num; i++){
      const I3D_face_group &fg = fgroups[i];
      dword base = fg.BaseIndex1();
      dword num_f = fg.NumFaces1();
      ck->write(&base, sizeof(word));
      ck->write(&num_f, sizeof(word));
                           //write material index
      for(int j=mats.size(); j--; ){
         if(mats[j]==fg.mat)
            break;
      }
      if(j==-1){
         assert(0);
         return false;
      }
      ck->WriteWord(word(j));
   }
   return true;
}

//----------------------------

bool LoadCachedInfoFGroups(C_cache *ck, C_buffer<I3D_face_group> &fgroups, C_vector<C_smart_ptr<I3D_material> > &mats){

                              //read fgroups
   word num = 0;
   dword rl = ck->read(&num, sizeof(num));
   if(rl!=sizeof(num))
      return false;
   fgroups.assign(num);
   for(int i=0; i<num; i++){
                              //read number of faces
      word base, num_f;
      rl = ck->read(&base, sizeof(base));
      if(rl!=sizeof(base)) return false;
      rl = ck->read(&num_f, sizeof(num_f));
      if(rl!=sizeof(num_f)) return false;
                              //create and init face-group
      I3D_face_group &fg = fgroups[i];
      fg.base_index = base;
      fg.num_faces = num_f;

      word mat_indx;
      if(ck->read(&mat_indx, sizeof(word)) != sizeof(word))
         return false;
      assert(mat_indx < mats.size());
      if(mat_indx >= mats.size())
         return false;
      fg.SetMaterial(mats[mat_indx]);
   }
   return true;
}

//----------------------------

bool SaveCachedInfoLods(C_cache *ck, const C_buffer<C_auto_lod> &auto_lods, const C_vector<C_smart_ptr<I3D_material> > &mats){

   dword num = auto_lods.size();
   ck->WriteWord(word(num));
   for(dword i=0; i<num; i++){
      const C_auto_lod &al = auto_lods[i];

      ck->write(&al.ratio, sizeof(al.ratio));
                              //write vertex count
      ck->WriteWord(word(al.vertex_count));
                              //write faces of this LOD
      dword num1 = al.GetIndexBuffer().NumFaces1();
      ck->write(&num1, sizeof(word));
      const I3D_triface *faces = al.GetIndexBuffer().LockForReadOnly();
      ck->write(faces, num1 * sizeof(I3D_triface));
      al.GetIndexBuffer().Unlock();

      SaveCachedInfoFGroups(ck, al.fgroups, mats);
#ifdef USE_STRIPS
                              //write strip info
      {
         dword num = al.strip_info.size();
         ck->WriteWord(word(num));
         ck->write(al.strip_info.begin(), num*sizeof(S_fgroup_strip_info));
      }
#endif
   }
   return true;
}

//----------------------------

bool LoadCachedInfoLods(C_cache *ck, C_buffer<C_auto_lod> &auto_lods, C_vector<C_smart_ptr<I3D_material> > &mats, PI3D_driver drv){

   word num;
   dword rl = ck->read(&num, sizeof(num));
   if(rl!=sizeof(num))
      return false;
   auto_lods.assign(num);
   for(int i=0; i<num; i++){
      C_auto_lod &al = auto_lods[i];
      al.Init(drv);
      rl = ck->read(&al.ratio, sizeof(al.ratio));
      if(rl!=sizeof(al.ratio))
         return false;
      word num1;
                           //read vertex counts
      al.vertex_count = 0;
      rl = ck->read(&al.vertex_count, sizeof(word));
      if(rl!=sizeof(word))
         return false;

                           //read faces of this LOD
      rl = ck->read(&num1, sizeof(num1));
      if(rl!=sizeof(num1))
         return false;
      I3D_source_index_buffer &ib = const_cast<I3D_source_index_buffer&>(al.GetIndexBuffer());
      ib.AllocFaces(num1);
      PI3D_triface faces = ib.Lock(0);
      rl = ck->read(faces, num1 * sizeof(I3D_triface));
#ifdef GL
      if(num1){
         assert(ib.ibo);
         glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.ibo);
         glBufferData(GL_ELEMENT_ARRAY_BUFFER, num1*sizeof(I3D_triface), faces, GL_STATIC_DRAW);
      }
#endif
      ib.Unlock();
      if(rl!=num1 * sizeof(I3D_triface))
         return false;

      if(!LoadCachedInfoFGroups(ck, al.fgroups, mats))
         return false;
#ifdef USE_STRIPS
                              //write strip info
      {
         dword num = ck->ReadWord();
         al.strip_info.assign(num);
         ck->read(al.strip_info.begin(), num*sizeof(S_fgroup_strip_info));
      }
#endif
   }
   return true;
}

//----------------------------
//----------------------------

void C_auto_lod::SetFaces(const I3D_triface *faces, bool allow_strips){

#ifndef USE_STRIPS
   index_buffer.SetFaces(faces, NumFaces1());
#else
   if(allow_strips){
      strip_info.assign(fgroups.size());
      C_vector<word> all_strips;
      all_strips.reserve(NumFaces1()*2);

      for(dword i=0; i<fgroups.size(); i++){
         const I3D_face_group &fg = fgroups[i];
         C_vector<word> strips;
         MakeStrips(&faces[fg.base_index], fg.num_faces, strips, NULL);

         S_fgroup_strip_info &si = strip_info[i];
         si.base_index = all_strips.size();
         si.num_indicies = strips.size();

         all_strips.insert(all_strips.end(), strips.begin(), strips.end());
      }
                              //align indicies number to number dividible by 3
      while(all_strips.size()%3)
         all_strips.push_back(0);
      index_buffer.SetFaces((I3D_triface*)&all_strips.front(), all_strips.size() / 3);
   }else{
      strip_info.clear();
      index_buffer.SetFaces(faces, NumFaces1());
   }
#endif
}

//----------------------------

I3D_RESULT C_auto_lod::GetFaces(I3D_triface *buf) const{

#ifdef USE_STRIPS
   if(HasStrips()){

      dword dest_i = 0;

      const word *indicies_base = (word*)index_buffer.LockForReadOnly();
      C_destripifier destrip;

      for(dword ii=0; ii<fgroups.size(); ii++){
         const S_fgroup_strip_info &si = strip_info[ii];
         destrip.Begin(indicies_base + si.base_index);
         for(dword jj = 0; jj < si.num_indicies-2; jj++){
            destrip.Next();
                              //ignore degenerate strip faces
            if(destrip.IsDegenerate())
               continue;
            assert(dest_i < NumFaces1());
            buf[dest_i++] = destrip.GetFace();
         }
      }
      assert(dest_i == NumFaces1());
   }else{
      const I3D_triface *faces = index_buffer.LockForReadOnly();
      memcpy(buf, faces, NumFaces1()*sizeof(I3D_triface));
   }
#else
   const I3D_triface *faces = index_buffer.LockForReadOnly();
   memcpy(buf, faces, NumFaces1()*sizeof(I3D_triface));
#endif
   index_buffer.Unlock();
   return I3D_OK;
}

//----------------------------
                              //cost multiplier for various quality aspects
#if 1
#define COST_EDGE_SIZE .03f   //cost for split edge size (relative to squared area size)
#define COST_EDGE_SIZE_DELTA .02f //cost for edge size change (relative to squared area size)
//#define COST_EDGE_OPEN .2f //cost for open edge (relative to squared area size)
#define COST_SURFACE_AREA 3.0f  //cost for changing surface area (scaled by relative changed surface area)
#define COST_NORMAL_DELTA 3.0f   //cost for normal change (scaled by angle and relative area of changed faces)
#define COST_MATERIAL_MISMATCH 1.0f //cost of mismatched material (scaled by relative area of face)
#define COST_UV_DELTA .05f    //cost of uv mismatch (scaled by UV delta and relative area of face)
#define COST_INFACE_DEPTH 4.f //cost of changing depth of new vertex withing the face
#else

#define COST_EDGE_SIZE .0f   //cost for split edge size (relative to squared area size)
#define COST_EDGE_SIZE_DELTA .0f //cost for edge size change (relative to squared area size)
//#define COST_EDGE_OPEN .2f //cost for open edge (relative to squared area size)
#define COST_SURFACE_AREA 8.0f  //cost for changing surface area (scaled by relative changed surface area)
#define COST_NORMAL_DELTA 2.0f   //cost for normal change (scaled by angle and relative area of changed faces)
#define COST_MATERIAL_MISMATCH .0f //cost of mismatched material (scaled by relative area of face)
#define COST_UV_DELTA .0f    //cost of uv mismatch (scaled by UV delta and relative area of face)
#define COST_INFACE_DEPTH 2.f //cost of changing depth of new vertex withing the face

#endif

//----------------------------

static void StoreLOD(C_auto_lod &al, const C_vector<I3D_triface> &faces, const C_buffer<I3D_face_group> &fgroups,
   dword num_valid_faces, const word *vertex_remap, bool allow_strips){

   C_buffer<I3D_triface> buf(faces.size());
   //al.index_buffer.AllocFaces(num_valid_faces);
   //I3D_triface *fc_dst = al.index_buffer.Lock();
   I3D_triface *fc_dst = buf.begin();
   C_vector<I3D_face_group> tmp_fgs;
   tmp_fgs.reserve(fgroups.size());

   dword curr_fgi = 0;
   dword curr_fg_end = fgroups[curr_fgi].base_index + fgroups[curr_fgi].num_faces;
   assert(curr_fg_end <= faces.size());
   dword last_fg_base = 0;
                              //store only valid faces
   for(dword si=0, di=0; si<faces.size()+1; si++){
      if(si==curr_fg_end){
                              //store fgroup now
         if(last_fg_base!=di){
            tmp_fgs.push_back(I3D_face_group());
            I3D_face_group &fg = tmp_fgs.back();
            fg.mat = fgroups[curr_fgi].mat;
            fg.base_index = last_fg_base;
            fg.num_faces = di - last_fg_base;
            last_fg_base = di;
         }
         ++curr_fgi;
         if(curr_fgi==fgroups.size())
            break;
         curr_fg_end = fgroups[curr_fgi].base_index + fgroups[curr_fgi].num_faces;
      }
      //if(si>=faces.size())
         //continue;
      const I3D_triface &fc = faces[si];
      if(!fc.IsValid())
         continue;
      fc_dst[di++].CopyAndRemap(fc, vertex_remap);
   }
   assert(di==num_valid_faces);
   //al.index_buffer.Unlock();
   al.fgroups.assign(&tmp_fgs.front(), &tmp_fgs.front() + tmp_fgs.size());
   al.SetFaces(fc_dst, allow_strips);
}

//----------------------------

struct S_LOD_info{
   C_vector<I3D_triface> faces;
   dword num_valid_faces;
   dword vertex_count;
   float quality;
};

//----------------------------

struct S_edge_info{
   float cost;
   C_vector<I3D_edge> split_pairs;
   S_edge_info():
      cost(0)
   {}

   inline bool IsValid() const{ return (split_pairs.size()!=0); }
   inline void Invalidate(){ split_pairs.clear(); }
};

//----------------------------
#ifdef _DEBUG
//#define DEBUG_NO_VERTEX_REMAP
//#define AL_DEBUG_LOG(t) drv->PRINT(t)
#define AL_DEBUG_LOG(t)
//#define DEBUG_PROFILE         //profile computation time
//#define DEBUG_STORE_ALL_LODS  //store all lods after each edge killing
//#define DEBUG_STORE_SINGLE_LOD//store only 1st LOD
//#define DEBUG_SHOW_CRC
//#define DEBUG_EDGE_COMP_COUNT //count edge recomputations
#else
#define AL_DEBUG_LOG(t)
#endif

//----------------------------

void I3D_mesh_base::CreateLODs(dword min_num_faces, dword num_lods_requested, float min_dist, float max_dist,
   bool preserve_edges, const bool *vertex_nobreak_info){

   if(auto_lods.size())
      return;

   struct S_vertex_info{
   private:
      S_vertex_info(const S_vertex_info&){ assert(0); }
      void operator =(const S_vertex_info&){ assert(0); }
   public:
      S_vector pos;
      S_vector normal;
      S_vector2 tex;             //texture coordinates kept as 2D vector (due to computations)

      bool used;                 //true while used, set to flase when optimized out

                                 //list of all vertices originating from this pos (this and duplicated ones)
                                 // (valid in data of unique verts only)
      C_vector<word> all_verts;
      C_vector<word> edge_verts;   //vertices making up edges with this vertex
      dword fgroups;             //bits of fgroup indices where this vertex is used (indices>32 are wrapped)
      C_vector<word> faces;        //all faces, in which this vertex participates (face index)

      S_vertex_info():
         fgroups(0),
         used(false)
      {}

   //----------------------------

      void AddEdgeVertex(word w){
         for(int i=edge_verts.size(); i--; ){
            if(edge_verts[i]==w)
               return;
         }
         edge_verts.push_back(w);
      }

   //----------------------------
   // Kill vertex - unregister from edges of peers.
   // Returns # of active verts on this position.
      int Kill(S_vertex_info *vip, word this_index, const word *v_map){
         assert(used);
         int i;
         for(i=edge_verts.size(); i--; ){
            word w = edge_verts[i];
            for(int j=vip[w].edge_verts.size(); j--; ){
               if(vip[w].edge_verts[j]==this_index){
                  vip[w].edge_verts[j] = vip[w].edge_verts.back();
                  vip[w].edge_verts.pop_back();
                  break;
               }
            }
            assert(j!=-1);
         }
         edge_verts.clear();
         used = false;

         word v_uni = v_map[this_index];
         S_vertex_info &vi_uni = vip[v_uni];
         for(i=vi_uni.all_verts.size(); i--; ){
            if(vi_uni.all_verts[i]==this_index){
               vi_uni.all_verts[i] = vi_uni.all_verts.back();
               vi_uni.all_verts.pop_back();
               break;
            }
         }
         assert(i!=-1);
         return vi_uni.all_verts.size();
      }
   };


#ifdef DEBUG_PROFILE
   BegProf();
#endif

                              //must keep at leas one face
   min_num_faces = Max(1ul, min_num_faces);
   num_lods_requested = Max(1ul, num_lods_requested);

   int i;

   dword num_verts = vertex_buffer.NumVertices();
   if(!num_verts)
      return;
   void *verts = vertex_buffer.Lock();
   dword vstride = vertex_buffer.GetSizeOfVertex();
                              //find out unique and duplicated vertices
   word *v_map = new word[num_verts];
   MakeVertexMapping((const S_vector*)verts, vstride, num_verts, v_map);

   dword num_faces = NumFaces1();
                              //working set of faces (being invalidated here)
   I3D_triface *faces = new I3D_triface[num_faces];
   GetFaces(faces);
   C_buffer<I3D_triface> orig_faces(faces, faces+num_faces);
   //I3D_triface *orig_faces = index_buffer.Lock();

   min_num_faces = Min(min_num_faces, num_faces);

   //memcpy(faces, orig_faces, num_faces*sizeof(I3D_triface));
   dword curr_valid_faces = num_faces;
   C_vector<word> vertex_remove_order; vertex_remove_order.reserve(num_verts);
   C_vector<word> face_remove_order; face_remove_order.reserve(num_faces);
   //C_vector<I3D_triface> dest_faces(num_faces);

   C_buffer<bool> tmp_nobreak_info;
   if(preserve_edges && !vertex_nobreak_info){
      tmp_nobreak_info.assign(num_verts, false);
                              //detect open edges
      C_vector<I3D_edge> edges;
      dword i;
      for(i=num_faces; i--; ){
         I3D_triface fc = faces[i];
         fc.Remap(v_map);
         AddOrSplitEdge(edges, I3D_edge(fc[0], fc[1]));
         AddOrSplitEdge(edges, I3D_edge(fc[1], fc[2]));
         AddOrSplitEdge(edges, I3D_edge(fc[2], fc[0]));
      }
      for(i=edges.size(); i--; )
         tmp_nobreak_info[edges[i][0]] = true;

      vertex_nobreak_info = &tmp_nobreak_info.front();
   }
                              //indices of face groups being filled
   //C_vector<word> fgroups_num_faces(fgroups.size(), 0);

                              //faces stored for each LOD
   list<S_LOD_info> work_lods;

   S_vertex_info *vertex_info = new S_vertex_info[num_verts];
   struct S_face_info{
      float surface_area;
      S_plane plane;
      word face_group_index;
   };
   S_face_info *face_info = new S_face_info[num_faces];
   float r_orig_surface_area = 0.0f;

                              //mesh edges made of unique vertices (1st is dest, 2nd is source)
   typedef map<I3D_edge, S_edge_info> t_mesh_edges;
   t_mesh_edges mesh_edges;

   {
      const I3D_face_group *fg = fgroups.end()-1;
      for(i=num_faces; i--; ){
         I3D_triface &fc = faces[i];
         S_face_info &fi = face_info[i];
         while(fg->base_index > (dword)i)
            --fg;

         int fgi = fg-fgroups.begin();
         fi.face_group_index = word(fgi);

                              //detect too small faces - mapping to themselves - optimize out now
         {
            I3D_triface fcr = fc;
            fcr.Remap(v_map);
            if(fcr[0]==fcr[1] || fcr[1]==fcr[2] || fcr[2]==fcr[0]){
               fc.Invalidate();
               --curr_valid_faces;
               continue;
            }
         }
                              //put info to all face's verts
         for(int j=3; j--; ){
            word v = fc[j];
            assert(v<num_verts);
            S_vertex_info &vi = vertex_info[v];
                              //mark the vertex as used
            vi.used = true;
                              //store face group index
            vi.fgroups |= 1 << fgi;
                              //store rest of face
            vi.AddEdgeVertex(fc[(j+1)%3]);
            vi.AddEdgeVertex(fc[(j+2)%3]);
            vi.faces.push_back(word(i));
         }
                              //compute face area
         fi.surface_area = fc.ComputeSurfaceArea((const S_vector*)verts, vstride);
         r_orig_surface_area += fi.surface_area;

                              //compute face plane
         fi.plane.ComputePlane(
            *(S_vector*)(((byte*)verts) + fc[0]*vstride),
            *(S_vector*)(((byte*)verts) + fc[1]*vstride),
            *(S_vector*)(((byte*)verts) + fc[2]*vstride));

         {
            for(int i=3; i--; ){
                              //make mesh edges
               word w0 = v_map[fc[i]], w1 = v_map[fc[(i+1)%3]];
               mesh_edges[I3D_edge(w0, w1)];
               mesh_edges[I3D_edge(w1, w0)];
            }
         }
      }
   }

   dword max_vertex_faces = 0;
                              //build info about shared vertices
   {
      dword fvf = vertex_buffer.GetFVFlags();
      int norm_offs = GetVertexComponentOffset(fvf, D3DFVF_NORMAL);
      int uv_offs = GetVertexComponentOffset(fvf, 0<<D3DFVF_TEXCOUNT_SHIFT);
                                 //algo assumes normal and UV coords are present
      assert(norm_offs!=-1 && uv_offs!=-1);

      for(i=num_verts; i--; ){
         S_vertex_info &vi = vertex_info[i];
                                 //store vertex data
         byte *vertex = ((byte*)verts) + i * vstride;
         vi.pos = *(S_vector*)vertex;
         vi.normal = *(S_vector*)(vertex + norm_offs);
         vi.tex = *(S_vector2*)(vertex + uv_offs);
                              //if vertex is not used by any face, just ignore it
         if(!vi.used)
            continue;
         assert(vi.faces.size());
         max_vertex_faces = Max(max_vertex_faces, (dword)vi.faces.size());

         word v_uni = v_map[i];
         if(v_uni!=i){
            S_vertex_info &vi_uni = vertex_info[v_uni];
            vi_uni.all_verts.push_back(word(i));
         }else{
            vi.all_verts.push_back(word(i));
         }
      }
   }

                              //info about vertex' faces being kept after split
   struct S_kept_face{
      word indx;
                              //relative area of the face
      float area;
                              //ideal UV for dest vertex position
      S_vector2 ideal_uv;
   } *kept_faces = new S_kept_face[max_vertex_faces];

   float r_orig_surface_area_sqrt = 1.0f / I3DSqrt(r_orig_surface_area);
   r_orig_surface_area = 1.0f / r_orig_surface_area;


   float approx_faces_per_lod = (float)(num_faces - min_num_faces) / (float)num_lods_requested;
   float next_lod_faces = float(num_faces - FloatToInt(approx_faces_per_lod * .5f));

#ifdef DEBUG_EDGE_COMP_COUNT
   int edge_comp = 0;
#endif
                              //dirty vertices - those affected by move in recent edge killing
                              // used for determining which edge costs must be recomputed
                              // (unique verts stored here!)
   set<word> dirty_dst_verts;

   while(mesh_edges.size()){
      assert(curr_valid_faces);

                              //determine which edge we're going to optimize out now
      t_mesh_edges::const_iterator best_edge_it = mesh_edges.end();

                              //traverse the edges of mesh
      for(t_mesh_edges::iterator e_it = mesh_edges.begin(); e_it!=mesh_edges.end(); e_it++){
         const I3D_edge edge = (*e_it).first;
         S_edge_info &ei = (*e_it).second;
         if(vertex_nobreak_info && vertex_nobreak_info[edge[1]] && !vertex_nobreak_info[edge[0]])
            continue;
                              //check if this edge is dirty
         if(dirty_dst_verts.find(edge[0])!=dirty_dst_verts.end())
            ei.Invalidate();
                              //compute edge cost, if not valid
         if(!ei.IsValid()){
            word v_edge_src = edge[1];
            word v_edge_dst = edge[0];
            const S_vertex_info &vi_edge_src = vertex_info[v_edge_src];
            const S_vertex_info &vi_edge_dst = vertex_info[v_edge_dst];
            assert(vi_edge_src.all_verts.size());
            assert(vi_edge_dst.all_verts.size());

#ifdef DEBUG_EDGE_COMP_COUNT
            ++edge_comp;
#endif
                                 //compute cost for this edge
            ei.cost = 0.0f;

            float face_surface_delta = 0.0f;
            float edge_size_delta = 0.0f;
                                 //check all vertices originating at this position
            for(int src_i=vi_edge_src.all_verts.size(); src_i--; ){
               word v_src = vi_edge_src.all_verts[src_i];
               //assert(!vertex_nobreak_info || !vertex_nobreak_info[v_map[v_src]]);

               const S_vertex_info &vi_src = vertex_info[v_src];
               assert(vi_src.used);

                                 //add score for killing vertex
               //cost += COST_VERTEX_KILL;

               dword num_kept_faces = 0;
               float kept_face_new_area = 0.0f;

               for(int fi=vi_src.faces.size(); fi--; ){
                  word face_index = vi_src.faces[fi];
                  I3D_triface fcr = faces[face_index];
                  fcr.Remap(v_map);

                  float new_surface_area = 0.0f;
                  if(!fcr.ContainsIndex(v_edge_dst)){
                              //if face won't be killed, compute it's new area
                     int ii = fcr.FindIndex(v_edge_src);
                     assert(ii!=-1);
                     fcr[ii] = v_edge_dst;
                     new_surface_area = fcr.ComputeSurfaceArea((const S_vector*)verts, vstride);
                     kept_faces[num_kept_faces].indx = face_index;
                     kept_faces[num_kept_faces].area = new_surface_area * r_orig_surface_area;
                     kept_face_new_area += new_surface_area;

                              //compute ideal UV for the face
                     {
                        S_vector2 &ideal_uv = kept_faces[num_kept_faces].ideal_uv;
                        int ii0 = (ii+1)%3;
                        int ii1 = (ii+2)%3;

                        const I3D_triface &fc = faces[face_index];
                        const S_vertex_info &vi0 = vertex_info[fc[ii0]];
                        const S_vertex_info &vi1 = vertex_info[fc[ii1]];
                        const S_vector2 &uv_src = vi_src.tex;
                        const S_vector2 &uv1 = vi1.tex;

                        float u1, u2;
                        if(LineIntersection(vi0.pos, vi1.pos-vi0.pos,
                           vi_edge_src.pos, vi_edge_dst.pos-vi_edge_src.pos, u1, u2) &&
                           !IsAbsMrgZero(u2)){

                           const S_vector2 &tex0 = vi0.tex;
                           S_vector2 uv_dir = uv1 - tex0;
                           S_vector2 pol = tex0 + uv_dir * u1;
                           uv_dir = pol - uv_src;
                           ideal_uv = uv_src + uv_dir * (1.0f / u2);
                        }else{
                           ideal_uv.Zero();
                        }
                     }

                              //add cost of height change
                     float depth = I3DFabs(vi_edge_dst.pos.DistanceToPlane(face_info[face_index].plane));
                     depth *= (face_info[face_index].surface_area * r_orig_surface_area);
                     ei.cost += depth * COST_INFACE_DEPTH;

                     ++num_kept_faces;
                  }
                              //subtract space of the space (it's killed or adjusted)
                  float delta = new_surface_area - face_info[face_index].surface_area;
                  face_surface_delta += delta;
               }
               assert(num_kept_faces <= max_vertex_faces);

               kept_face_new_area *= r_orig_surface_area;
                                 //find best destination for this actual vertex
               word v_dst = v_src;
               float max_dest_cost = 1e+16f;
               for(int dst_i = vi_edge_dst.all_verts.size(); dst_i--; ){
                  word v_dst_test = vi_edge_dst.all_verts[dst_i];
                  const S_vertex_info &vi_test = vertex_info[v_dst_test];
                  float test_cost = 0.0f;

                              //count all faces which will survive
                  for(int fi=num_kept_faces; fi--; ){
                     word face_index = kept_faces[fi].indx;

                     float face_cost = 0.0f;
                              //check if material matches
                     if(!((1<<face_info[face_index].face_group_index) & vi_test.fgroups)){
                        face_cost += COST_MATERIAL_MISMATCH;
                     }else{
                              //material matches, check how good uv match we may have
                              // compute ideal uv coordinates of dest vertex, mapping to our face
                        const S_vector2 &ideal_uv = kept_faces[fi].ideal_uv;
                        float uv_delta = (ideal_uv - vi_test.tex).Magnitude();
                        face_cost += Min(uv_delta, 1.0f) * COST_UV_DELTA;
                     }
                              //add to test cost
                     test_cost += face_cost * kept_faces[fi].area;
                  }

                              //apply costs of normal
                  //float normal_delta = (float)acos(::Min(::Max(vi_src.normal.Dot(vi_test.normal), -1.0f), 1.0f)) / PI;
                  float normal_delta = (1.0f - vi_src.normal.Dot(vi_test.normal)) * .5f;
                  normal_delta *= kept_face_new_area;
                  test_cost += normal_delta * COST_NORMAL_DELTA;

                              //keep better dest
                  if(max_dest_cost > test_cost){
                     max_dest_cost = test_cost;
                     v_dst = v_dst_test;
                  }
               }

                              //compute edge size delta
               {
                  const S_vector &pos_src = vi_edge_src.pos;
                  const S_vector &pos_dst = vi_edge_dst.pos;
                  for(int ei=vi_src.edge_verts.size(); ei--; ){
                     word v = vi_src.edge_verts[ei];
                     const S_vector &v_pos = vertex_info[v].pos;
                     float orig_len = (v_pos - pos_src).Magnitude();
                     float new_len = (v_pos - pos_dst).Magnitude();
                     edge_size_delta += new_len - orig_len;
                  }
               }

                              //add cost of splitting with the best dest vertex
               ei.cost += max_dest_cost;


                              //store best result for this source vertex
               assert(v_src!=v_dst);
               ei.split_pairs.push_back(I3D_edge(v_src, v_dst));
            }

                              //add edge size cost
            {
               float edge_size = (vi_edge_src.pos - vi_edge_dst.pos).Magnitude();
               float cost = edge_size * r_orig_surface_area_sqrt;
               ei.cost += cost * COST_EDGE_SIZE;
                              //apply special cost for open edges
               /*
               if(ei.num_faces==1){
                  //ei.cost += cost * COST_EDGE_OPEN;
               }
               */
            }

                              //add edge size change delta
            {
               float cost = I3DFabs(edge_size_delta) * r_orig_surface_area_sqrt;
               ei.cost += cost * COST_EDGE_SIZE_DELTA;
            }

                              //compute how this split will change mesh surface area
            {
               float delta_ratio = I3DFabs(face_surface_delta) * r_orig_surface_area;
               ei.cost += delta_ratio * COST_SURFACE_AREA;
            }

            assert(ei.IsValid());
         }else{
#if defined _DEBUG && 0
            for(int i=ei.split_pairs.size(); i--; ){
               const I3D_edge e = ei.split_pairs[i];
               assert(vertex_info[e[0]].used);
               assert(vertex_info[e[1]].used);
            }
#endif
         }

                              //keep edge with lowest cost
         if(((best_edge_it==mesh_edges.end()) || ((*best_edge_it).second.cost > ei.cost)) && ei.IsValid())
            best_edge_it = e_it;
      }

      if(best_edge_it==mesh_edges.end())
         break;

      AL_DEBUG_LOG(C_fstr("Cost: %.4f", (*best_edge_it).second.cost));

                              //make copy of split pairs, because the iterator may be killed
      C_vector<I3D_edge> split_pairs = (*best_edge_it).second.split_pairs;

                              //prepare collection of info about dirty verts, they'll be filled below
      dirty_dst_verts.clear();
      C_vector<word> dirty_src_verts;
      dirty_src_verts.reserve(split_pairs.size()*2+1);

                              //kill edge
      word v_edge_src = (*best_edge_it).first[1];
      word v_edge_dst = (*best_edge_it).first[0];
      dirty_dst_verts.insert(v_edge_dst);
      dword curr_killed_faces = face_remove_order.size();

      for(i=split_pairs.size(); i--; ){
         const I3D_edge split_edge = split_pairs[i];
         word v_src = split_edge[0];
         word v_dst = split_edge[1];
         assert(v_src!=v_dst);
         word v_src_uni = v_map[v_src];
         word v_dst_uni = v_map[v_dst];
         assert(vertex_info[v_dst].used);
         AL_DEBUG_LOG(C_fstr("%i -> %i", v_src, v_dst));
         //assert(!vertex_nobreak_info || !vertex_nobreak_info[v_map[v_src]]);

                              //remap or kill all vertex' faces
         S_vertex_info &vi_src = vertex_info[v_src];
         for(int fi=vi_src.faces.size(); fi--; ){
            word face_index = vi_src.faces[fi];
            I3D_triface &fc = faces[face_index];
            assert(fc.IsValid());
            if(!fc.IsValid())
               continue;

            int ii = fc.FindIndex(v_src);
            assert(ii!=-1);
            if(ii==-1)
               continue;

            {
                              //kill edges of this vertex
               for(int i=0; i<2; i++){
                  word v = v_map[fc[(ii+1+i)%3]];
                              //store the affected (dirty) vertex
                  dirty_src_verts.push_back(v);

                  t_mesh_edges::iterator it = mesh_edges.find(I3D_edge(v_src_uni, v));
                  if(it!=mesh_edges.end()){
                     mesh_edges.erase(it);
                              //find and kill opposite edge
                     it = mesh_edges.find(I3D_edge(v, v_src_uni));
                     assert(it!=mesh_edges.end());
                     mesh_edges.erase(it);
                  }else{
                     assert(mesh_edges.find(I3D_edge(v, v_src_uni))==mesh_edges.end());
                  }
               }
            }

                              //if the face contains any vertex at dest position, it'll be killed
            {
               S_vertex_info &vi_dst = vertex_info[v_dst_uni];
               for(int j=vi_dst.all_verts.size(); j--; ){
                  word v = vi_dst.all_verts[j];
                  if(fc[(ii+1)%3]==v || fc[(ii+2)%3]==v)
                     break;
               }
               if(j!=-1){
                  face_remove_order.push_back(face_index);
                  continue;
               }
            }

                              //adjust face - move source vertex to dest vertex
            fc[ii] = v_dst;
            S_vertex_info &vi_dst = vertex_info[v_dst];
            face_info[face_index].surface_area = fc.ComputeSurfaceArea((const S_vector*)verts, vstride);
            face_info[face_index].plane.ComputePlane(vertex_info[fc[0]].pos, vertex_info[fc[1]].pos, vertex_info[fc[2]].pos);

                              //make new edge connections
                              //add this face to participants
                              // (old connections will be removed when vertex is killed)
                              // (old faces will be removed when face is killed)
            {
               for(int i=0; i<2; i++){
                  word v = fc[(ii+1+i)%3];
                              //adjsut vertex info
                  {
                     vi_dst.AddEdgeVertex(v);
                     S_vertex_info &vi = vertex_info[v];
                     vi.AddEdgeVertex(v_dst);
                     if(find(vi.faces.begin(), vi.faces.end(), face_index) == vi.faces.end()){
                        vi.faces.push_back(face_index);
                        if(max_vertex_faces < vi.faces.size()){
                           delete[] kept_faces; kept_faces = new S_kept_face[max_vertex_faces=vi.faces.size()];
                        }
                     }
                  }
                              //adjust mesh edge info
                  {
                     v = v_map[v];
                     mesh_edges[I3D_edge(v_dst_uni, v)];
                     mesh_edges[I3D_edge(v, v_dst_uni)];
                  }
               }
            }

                              //add this face to dest's vertices
            assert(find(vi_dst.faces.begin(), vi_dst.faces.end(), face_index) == vi_dst.faces.end());
            vi_dst.faces.push_back(face_index);
            if(max_vertex_faces < vi_dst.faces.size()){
               delete[] kept_faces; kept_faces = new S_kept_face[max_vertex_faces=vi_dst.faces.size()];
            }
         }

                              //this vertex is killed
         vi_src.Kill(vertex_info, v_src, v_map);
         AL_DEBUG_LOG(C_fstr("killing vertex %i", v_src));
         vertex_remove_order.push_back(v_src);
      }
      {
                              //add v_edge_dst into list, if not yet
         int i;
         for(i=dirty_src_verts.size(); i--; ){
            if(dirty_src_verts[i]==v_edge_dst)
               break;
         }
         if(i==-1)
            dirty_src_verts.push_back(v_edge_dst);
                              //invalidate all egdes containing any of these as source
         for(i=dirty_src_verts.size(); i--; ){
            word v = dirty_src_verts[i];
            for(t_mesh_edges::iterator it = mesh_edges.lower_bound(I3D_edge(0, v)); (it!=mesh_edges.end()) && ((*it).first[1]==v); it++)
               (*it).second.Invalidate();
         }
      }

                              //clean - detect unused verts in recently killed faces
      while(curr_killed_faces<face_remove_order.size()){
         word face_index = face_remove_order[curr_killed_faces++];
         I3D_triface &fc = faces[face_index];
         assert(fc.IsValid());

                              //unregister from all its verts
         int v_src_ii = -1;
         for(int ii=3; ii--; ){
            word v = fc[ii];
            S_vertex_info &vi = vertex_info[v];
            if(vi.used){
               dirty_dst_verts.insert(v_map[v]);
               for(int j=vi.faces.size(); j--; ){
                  if(vi.faces[j]==face_index)
                     break;
               }
               assert(j!=-1);
               vi.faces[j] = vi.faces.back(); vi.faces.pop_back();
               if(!vi.faces.size()){
                              //verted dead (no longer used by any face)   
                  vi.Kill(vertex_info, v, v_map);
                  AL_DEBUG_LOG(C_fstr("killing vertex %i", v));
                  vertex_remove_order.push_back(v);
                  {
                              //kill edges of this vertex
                     word v_src_uni = v_map[v];
                     //AL_DEBUG_LOG(C_fstr("killing edge of %i", v_src_uni));
                     for(int i=0; i<2; i++){
                        word v = v_map[fc[(ii+1+i)%3]];
                        t_mesh_edges::iterator it = mesh_edges.find(I3D_edge(v_src_uni, v));
                        if(it!=mesh_edges.end()){
                           mesh_edges.erase(it);
                           it = mesh_edges.find(I3D_edge(v, v_src_uni));
                           assert(mesh_edges.find(I3D_edge(v, v_src_uni))!=mesh_edges.end());
                           mesh_edges.erase(it);
                        }else{
                           assert(mesh_edges.find(I3D_edge(v, v_src_uni))==mesh_edges.end());
                        }
                     }
                  }
               }
            }
            if(v_map[v]==v_edge_src)
               v_src_ii = ii;
         }
         assert(v_src_ii!=-1);
         {
                           //check if remaining edge will survive
            word v0 = fc[(v_src_ii+1)%3];
            word v1 = fc[(v_src_ii+2)%3];
            const S_vertex_info &vi0 = vertex_info[v0];
            const S_vertex_info &vi1 = vertex_info[v1];
                           //check if those two have common faces
            int i;
            for(i=vi0.faces.size(); i--; ){
               if(find(vi1.faces.begin(), vi1.faces.end(), vi0.faces[i])!=vi1.faces.end())
                  break;
            }
            if(i==-1){
               v0 = v_map[v0];
               v1 = v_map[v1];
               t_mesh_edges::iterator it = mesh_edges.find(I3D_edge(v0, v1));
               if(it!=mesh_edges.end()){
                  mesh_edges.erase(it);
                  it = mesh_edges.find(I3D_edge(v1, v0));
                  assert(mesh_edges.find(I3D_edge(v1, v0))!=mesh_edges.end());
                  mesh_edges.erase(it);
               }else{
                  assert(mesh_edges.find(I3D_edge(v1, v0))==mesh_edges.end());
               }
            }
         }
         AL_DEBUG_LOG(C_fstr("killing face %i", face_index));
         --curr_valid_faces;
         fc.Invalidate();

         /*
                              //prepare face reorder info
         {
            int fgi = face_info[face_index].face_group_index;
            const I3D_face_group &fg = fgroups[fgi];
            int dest_index = fg.base_index + fg.num_faces - (++fgroups_num_faces[fgi]);
            dest_faces[dest_index] = orig_faces[face_index];
         }
         */
      }

#if defined _DEBUG && 0
                              //make sure all 'used' verts are really used
      {
         C_vector<bool> v_use(num_verts, false);
         for(int i=num_faces; i--; ){
            const I3D_triface &fc = faces[i];
            if(fc.IsValid()){
               v_use[fc[0]] = true;
               v_use[fc[1]] = true;
               v_use[fc[2]] = true;
            }
         }
         for(i=num_verts; i--; )
            assert(v_use[i] == vertex_info[i].used);
      }
      {
                              //validate edge connections
         for(int i=num_verts; i--; ){
            const S_vertex_info &vi = vertex_info[i];
            for(int j=vi.edge_verts.size(); j--; ){
               word w = vi.edge_verts[j];
               const S_vertex_info &vi1 = vertex_info[w];
               for(int k=vi1.edge_verts.size(); k--; ){
                  if(vi1.edge_verts[k]==i)
                     break;
               }
               assert(k!=-1);
            }
         }
      }
      {
                              //make sure mesh edge list is valid
         for(t_mesh_edges::iterator e_it = mesh_edges.begin(); e_it!=mesh_edges.end(); e_it++){
            const I3D_edge edge = (*e_it).first;
            //assert(vertex_info[edge[0]].all_verts.size());
            //assert(vertex_info[edge[1]].all_verts.size());
            for(int i=num_faces; i--; ){
               I3D_triface fc = faces[i];
               if(!fc.IsValid())
                  continue;
               fc.Remap(v_map);
               if(fc.ContainsIndices(edge[0], edge[1]))
                  break;
            }
            assert(i!=-1);
         }
      }
#endif
                              //store LOD now
      //if(curr_valid_faces < min_num_faces)
         //break;
                              //check if it's time to store LOD now
#ifndef DEBUG_STORE_ALL_LODS
      if((float)curr_valid_faces <= next_lod_faces)
#endif
      {

         AL_DEBUG_LOG(C_fstr("store LOD %i", work_lods.size()));

         {
            S_LOD_info li;
            li.quality = 0;
            work_lods.push_back(li);
         }
         S_LOD_info &lod_i = work_lods.back();
         //lod_i.faces = faces;
         lod_i.faces.assign(faces, faces+num_faces);
         lod_i.num_valid_faces = curr_valid_faces;
         lod_i.vertex_count = num_verts - vertex_remove_order.size();
         lod_i.quality = (float)curr_valid_faces / (float)num_faces;
#if defined _DEBUG && 0
         if(work_lods.size()==3)
            break;
#endif
         if(curr_valid_faces <= min_num_faces)
            break;
#ifdef DEBUG_STORE_SINGLE_LOD
         break;
#endif
         //next_lod_faces = num_faces - FloatToInt(approx_faces_per_lod * ((float)(work_lods.size()*1) + .5f) );
         next_lod_faces -= approx_faces_per_lod;
         next_lod_faces = Max(next_lod_faces, (float)min_num_faces);
      }
   }


                              //reorganize vertices, so that first removed are at back
   C_vector<word> v_remap;
   {
#ifndef DEBUG_NO_VERTEX_REMAP
      byte *new_verts = new byte[num_verts*vstride];
      ReorderVertices(verts, num_verts, vstride, vertex_remove_order, v_remap, new_verts);
      memcpy(verts, new_verts, num_verts*vstride);
      delete[] new_verts;

      C_vector<word> new_v_map;
      CreateUniqueVMap(verts, vstride, num_verts, new_v_map);
      vertex_buffer.SetVertexSpreadMap(new_v_map.size() ? &new_v_map.front() : NULL, new_v_map.size());
#else
      v_remap.assign(num_verts);
      for(i=num_verts; i--; )
         v_remap[i] = i;
#endif
   }

   /*
   {
                              //store rest faces not stored yet
      for(int i=num_faces; i--; ){
         if(faces[i].IsValid()){
            int fgi = face_info[i].face_group_index;
            const I3D_face_group &fg = fgroups[fgi];
            int dest_index = fg.base_index + fg.num_faces - (++fgroups_num_faces[fgi]);
            dest_faces[dest_index] = orig_faces[i];
         }
      }
                              //copy new ordered faces
      memcpy(orig_faces, dest_faces.begin(), num_faces*sizeof(I3D_triface));
   }
   */

                              //remap main set of faces
   for(i=num_faces; i--; ){
      I3D_triface &fc = orig_faces[i];
      assert(fc[0]<num_verts && fc[1]<num_verts && fc[2]<num_verts);
      fc.Remap(&v_remap.front());
   }


                              //store all LODS
   //num_auto_lods = work_lods.size();
   //delete[] auto_lods;
   //auto_lods = NULL;
   auto_lods.assign(work_lods.size());
   if(work_lods.size()){
      //auto_lods = new C_auto_lod[work_lods.size()];
      i = 0;
      for(list<S_LOD_info>::const_iterator it = work_lods.begin(); it!=work_lods.end(); it++, i++){
         const S_LOD_info &lod_i = (*it);
         C_auto_lod &al = auto_lods[i];
         al.Init(drv);
         al.ratio = lod_i.quality;
         al.vertex_count = lod_i.vertex_count;
#ifdef DEBUG_NO_VERTEX_REMAP
         al.vertex_count = num_verts;
#endif
         StoreLOD(al, lod_i.faces, fgroups, lod_i.num_valid_faces, &v_remap.front(), HasStrips());
      }
   }
#ifdef DEBUG_SHOW_CRC
   {
      dword crc = 0;
      for(int i=num_verts; i--; )
         crc ^= _rotl(v_remap[i], i);
      drv->PRINT(C_fstr("CRC: %x", crc));
   }
#endif

   SetFaces(orig_faces.begin(), HasStrips());
   //index_buffer.Unlock();
   vertex_buffer.Unlock();
   delete[] vertex_info;
   delete[] face_info;
   delete[] faces;
   delete[] v_map;
   delete[] kept_faces;

#ifdef DEBUG_PROFILE
   drv->PRINT(C_fstr("Compute time: %.2f", EndProf()));
#endif
#ifdef DEBUG_EDGE_COMP_COUNT
   drv->PRINT(C_fstr("Edge computed: : %i", edge_comp));
#endif

   auto_lod_dists[0] = min_dist;
   auto_lod_dists[1] = max_dist;
}

//----------------------------
//----------------------------

I3D_source_vertex_buffer::~I3D_source_vertex_buffer(){
   if(D3D_vertex_buffer){
#ifdef USE_SHARED_DEST_VB
      drv->FreeVertexBuffer(D3D_vertex_buffer, D3D_vertex_buffer_index, NULL);
#else
      D3D_vertex_buffer->Release();
#endif
   }
#ifdef GL
   vs_decl = NULL;
   if(vbo)
      glDeleteBuffers(1, &vbo);
#endif
}

//----------------------------

void I3D_source_vertex_buffer::SetFVFlags(dword fvf){

   if(num_vertices)
      SetVertices(NULL, 0);
   fvflags = fvf;
   vstride = ::GetSizeOfVertex(fvf);
}

//----------------------------

void I3D_source_vertex_buffer::AllocVertices(dword num_verts1){

   dword d3d_usage_flags = 0;
   d3d_usage_flags |= D3DUSAGE_DONOTCLIP;
      //D3DUSAGE_WRITEONLY |
   D3DPOOL d3d_pool_flags = D3DPOOL_SYSTEMMEM;

#ifndef GL
   if(drv->IsDirectTransform()){
      //d3d_pool_flags = D3DPOOL_DEFAULT;
   }else{
      d3d_usage_flags |= D3DUSAGE_SOFTWAREPROCESSING;
   }
#endif

                              //alloc vertex buffer
   if(!D3D_vertex_buffer || num_vertices != num_verts1){
      if(D3D_vertex_buffer){
#ifdef USE_SHARED_DEST_VB
         drv->FreeVertexBuffer(D3D_vertex_buffer, D3D_vertex_buffer_index, NULL);
#else
         D3D_vertex_buffer->Release();
#endif
         D3D_vertex_buffer = NULL;
         D3D_vertex_buffer_index = 0;
      }
#ifdef GL
      vs_decl = NULL;
      if(vbo){
         glDeleteBuffers(1, &vbo);
         vbo = 0;
      }
#endif
                           //alloc raw VB
#ifdef USE_SHARED_DEST_VB
                           //// specify size in dwords (???)
      bool b;
      b = drv->AllocVertexBuffer(fvflags, d3d_pool_flags, d3d_usage_flags,
         num_verts1,
         (IDirect3DVertexBuffer9**)&D3D_vertex_buffer, &D3D_vertex_buffer_index, NULL);
      assert(b);
#else
      HRESULT hr;
      hr = drv->GetDevice1()->CreateVertexBuffer(num_verts1 * ::GetSizeOfVertex(fvflags), d3d_usage_flags, fvflags, d3d_pool_flags, (IDirect3DVertexBuffer9**)&D3D_vertex_buffer, NULL);
      CHECK_D3D_RESULT("CreateVertexBuffer", hr);
      D3D_vertex_buffer_index = 0;
#endif
      num_vertices = num_verts1;
#ifdef GL
      vs_decl = drv->GetVSDeclaration(fvflags);
      glGenBuffers(1, &vbo);
#endif
   }
   assert((!num_vertices && !D3D_vertex_buffer) || (D3D_vertex_buffer && num_vertices));
}

//----------------------------

void I3D_source_vertex_buffer::SetVertices(const void *src_vp, dword nv){

   AllocVertices(nv);
                           //feed vertices
   void *vp = Lock();
   assert(vp);
   if(!vp)
      return;
   memcpy(vp, src_vp, num_vertices * vstride);
   Unlock();
#ifdef GL
   assert(vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, num_vertices*vstride, src_vp, GL_STATIC_DRAW);
#endif
}

//----------------------------

void I3D_source_vertex_buffer::DuplicateVertices(word *dvi, dword num_dup_verts){

   dword d3d_usage_flags = 0;
   d3d_usage_flags |= D3DUSAGE_DONOTCLIP;     //bug in DX ? must include clipping flags in source VB
      //D3DUSAGE_WRITEONLY |
   D3DPOOL d3d_pool_flags = D3DPOOL_SYSTEMMEM;

#ifndef GL
   if(drv->IsDirectTransform()){
      //d3d_pool_flags = D3DPOOL_DEFAULT;
   }else{
      d3d_usage_flags |= D3DUSAGE_SOFTWAREPROCESSING;
   }
#endif
   int num_orig_verts = num_vertices - vertex_map.size();
   vertex_map.reserve(num_vertices + num_dup_verts);

   for(dword i=0; i<num_dup_verts; i++){
      int index = dvi[i];
      if(index < num_orig_verts)
         vertex_map.push_back(word(index));
      else
         vertex_map.push_back(vertex_map[index - num_orig_verts]);
   }
                              //alloc (new) vertex buffer
   IDirect3DVertexBuffer9 *D3D_vertex_buffer1;
   dword D3D_vertex_buffer_index1;

#ifdef USE_SHARED_DEST_VB
   bool b;
   b = drv->AllocVertexBuffer(fvflags, d3d_pool_flags, d3d_usage_flags, num_vertices + num_dup_verts,
      (IDirect3DVertexBuffer9**)&D3D_vertex_buffer1, &D3D_vertex_buffer_index1, NULL);
   assert(b);
#else
   HRESULT hr;
   hr = drv->GetDevice1()->CreateVertexBuffer((num_vertices+num_dup_verts) * ::GetSizeOfVertex(fvflags), d3d_usage_flags, fvflags, d3d_pool_flags, (IDirect3DVertexBuffer9**)&D3D_vertex_buffer1, NULL);
   CHECK_D3D_RESULT("CreateVertexBuffer", hr);
   D3D_vertex_buffer_index1 = 0;
#endif

   if(D3D_vertex_buffer){
                              //feed vertices
      const void *vp = Lock();
      assert(vp);
      if(vp){
         void *vp1;
         dword vstride = ::GetSizeOfVertex(fvflags);
         HRESULT hr = D3D_vertex_buffer1->Lock(D3D_vertex_buffer_index1 * vstride,
            (num_vertices + num_dup_verts) * vstride,
            (void**)&vp1,
            0);
         if(SUCCEEDED(hr)){
            memcpy(vp1, vp, num_vertices * vstride);
            for(i=0; i<num_dup_verts; i++){
               int index = dvi[i];
               memcpy((byte*)vp1 + vstride*(num_vertices+i),
                  (byte*)vp + vstride*index, vstride);
            }

            D3D_vertex_buffer1->Unlock();
         }
         Unlock();
      }
#ifdef USE_SHARED_DEST_VB
      drv->FreeVertexBuffer(D3D_vertex_buffer, D3D_vertex_buffer_index, NULL);
#else
      D3D_vertex_buffer->Release();
#endif
      D3D_vertex_buffer = D3D_vertex_buffer1;
      D3D_vertex_buffer_index = D3D_vertex_buffer_index1;
   }
   num_vertices += num_dup_verts;
}

//----------------------------
//----------------------------

I3D_source_index_buffer::I3D_source_index_buffer(PI3D_driver drv1):
   drv(drv1),
   D3D_index_buffer(NULL),
#ifdef GL
   ibo(0),
#endif
   num_faces(0)
{
}

//----------------------------

I3D_source_index_buffer::~I3D_source_index_buffer(){

   if(D3D_index_buffer){
#ifdef USE_SHARED_DEST_VB
      drv->FreeIndexBuffer(D3D_index_buffer, D3D_index_buffer_index);
#else
      D3D_index_buffer->Release();
#endif
   }
#ifdef GL
   if(ibo)
      glDeleteBuffers(1, &ibo);
#endif
}

//----------------------------

void I3D_source_index_buffer::AllocFaces(dword num){

                              //alloc vertex buffer
   if(!D3D_index_buffer || num_faces != num){
      if(D3D_index_buffer){
#ifdef USE_SHARED_DEST_VB
         drv->FreeIndexBuffer(D3D_index_buffer, D3D_index_buffer_index);
#else
         D3D_index_buffer->Release();
#endif
         D3D_index_buffer = NULL;
         D3D_index_buffer_index = 0;
      }
#ifdef GL
      if(ibo){
         glDeleteBuffers(1, &ibo);
         ibo = 0;
      }
#endif
      if(num){
         dword d3d_usage_flags = 0;
         D3DPOOL d3d_pool_flags = D3DPOOL_SYSTEMMEM;

#ifndef GL
         if(drv->IsDirectTransform()){
                              //don't use default now, we need a static copy!
            //d3d_pool_flags = D3DPOOL_DEFAULT;
         }else{
            d3d_usage_flags |= D3DUSAGE_SOFTWAREPROCESSING;
         }
#endif
#ifdef USE_SHARED_DEST_VB
         bool b;
         b = drv->AllocIndexBuffer(d3d_pool_flags, d3d_usage_flags, num, &D3D_index_buffer, &D3D_index_buffer_index);
         assert(b);
#else
         HRESULT hr;
         hr = drv->GetDevice1()->CreateIndexBuffer(num*6, d3d_usage_flags, D3DFMT_INDEX16, d3d_pool_flags, (IDirect3DIndexBuffer9**)&D3D_index_buffer, NULL);
         CHECK_D3D_RESULT("CreateIndexBuffer", hr);
         D3D_index_buffer_index = 0;
#endif
#ifdef GL
         glGenBuffers(1, &ibo);
#endif
      }
      num_faces = num;
   }
}

//----------------------------

void I3D_source_index_buffer::SetFaces(const I3D_triface *faces, dword num_faces1){

   AllocFaces(num_faces1);
   if(num_faces1){
                              //feed vertices
      PI3D_triface ip = Lock();
      assert(ip);
      if(!ip)
         return;
      memcpy(ip, faces, num_faces * sizeof(I3D_triface));
      Unlock();
#ifdef GL
      assert(ibo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_faces*sizeof(I3D_triface), faces, GL_STATIC_DRAW);
#endif
   }
}

//----------------------------

PI3D_triface I3D_source_index_buffer::Lock(dword d3d_lock_flags){

   if(!num_faces)
      return NULL;
   assert(D3D_index_buffer);

   PI3D_triface ip;
   HRESULT hr = D3D_index_buffer->Lock(D3D_index_buffer_index * sizeof(I3D_triface),
      num_faces * sizeof(I3D_triface),
      (void**)&ip,
      d3d_lock_flags);
   CHECK_D3D_RESULT("Lock", hr);
   if(FAILED(hr))
      return NULL;
   return ip;
}

//----------------------------

const I3D_triface *I3D_source_index_buffer::LockForReadOnly() const{

   return ((I3D_source_index_buffer*)this)->Lock(D3DLOCK_READONLY);
}

//----------------------------

bool I3D_source_index_buffer::Unlock() const{

   if(!num_faces)
      return false;
   HRESULT hr = D3D_index_buffer->Unlock();
   CHECK_D3D_RESULT("Unlock", hr);
   return (SUCCEEDED(hr));
}

//----------------------------
//----------------------------
                              //meshes

I3D_mesh_base::I3D_mesh_base(PI3D_driver d1, dword fvf1):
   ref(1),
   drv(d1),
   vertex_buffer(drv, fvf1),
   index_buffer(drv)
{
   auto_lod_dists[0] = 0.0f;
   auto_lod_dists[1] = 100.0f;
   drv->AddCount(I3D_CLID_MESH_BASE);
}

//----------------------------

I3D_mesh_base::~I3D_mesh_base(){
   drv->DecCount(I3D_CLID_MESH_BASE);
}

//----------------------------   

I3D_RESULT I3D_mesh_base::GetFaces(I3D_triface *buf) const{

#ifdef USE_STRIPS
   if(HasStrips()){

      dword dest_i = 0;

      const word *indicies_base = (word*)index_buffer.LockForReadOnly();
      C_destripifier destrip;

      for(dword ii=0; ii<NumFGroups1(); ii++){
         const S_fgroup_strip_info &si = strip_info[ii];
         destrip.Begin(indicies_base + si.base_index);

         for(dword jj = si.num_indicies-2; jj--; ){
            destrip.Next();
                              //ignore degenerate strip faces
            if(destrip.IsDegenerate())
               continue;
            assert(dest_i < NumFaces1());
            buf[dest_i++] = destrip.GetFace();
         }
      }
      assert(dest_i == NumFaces1());
   }else{
      const I3D_triface *faces = index_buffer.LockForReadOnly();
      memcpy(buf, faces, NumFaces1()*sizeof(I3D_triface));
   }
#else
   const I3D_triface *faces = index_buffer.LockForReadOnly();
   memcpy(buf, faces, NumFaces1()*sizeof(I3D_triface));
#endif
   index_buffer.Unlock();
   return I3D_OK;
}

//----------------------------

bool I3D_mesh_base::SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const{

                              //write version
   word version = DBASE_VERSION;
   ck->write(&version, sizeof(word));
                              //write fvf
   dword fvf = vertex_buffer.GetFVFlags();
   ck->write(&fvf, sizeof(dword));

   {                          //write vertices
      dword num = vertex_buffer.NumVertices();
      ck->write(&num, sizeof(word));
      const void *verts = const_cast<PI3D_mesh_base>(this)->vertex_buffer.Lock();
      ck->write(verts, num * vertex_buffer.GetSizeOfVertex());
      const_cast<PI3D_mesh_base>(this)->vertex_buffer.Unlock();
   }
   {                       //write spread map
      const C_vector<word> &vs_map = vertex_buffer.GetVertexSpreadMap();
      dword num = vs_map.size();
      ck->write(&num, sizeof(word));
      if(num)
         ck->write(&vs_map.front(), num*sizeof(word));
   }
   {                       //write faces
      dword num = index_buffer.NumFaces1();
      ck->WriteWord(word(num));
      const I3D_triface *faces = index_buffer.LockForReadOnly();
      ck->write(faces, num * sizeof(I3D_triface));
      index_buffer.Unlock();
   }
   SaveCachedInfoFGroups(ck, fgroups, mats);
#ifdef USE_STRIPS
   {                          //write strips
      ck->WriteDword(strip_info.size());
      ck->write(strip_info.begin(), strip_info.size()*sizeof(S_fgroup_strip_info));
   }
#endif
   SaveCachedInfoLods(ck, auto_lods, mats);
   ck->write(&auto_lod_dists, sizeof(auto_lod_dists));

   {                          //write smooth groups
      byte has_sg_list = (smooth_groups.Size() != 0);
      ck->write(&has_sg_list, sizeof(byte));
      if(has_sg_list)
         ck->write((const char*)smooth_groups, NumFaces1() * sizeof(byte));
   }
   {                          //write additional map channel
      bool mc2_present = (map_channel_2 != NULL);
      ck->WriteByte(mc2_present);
      if(mc2_present){
                                 //write vertices
         dword num = map_channel_2->uv_verts.size();
         ck->WriteWord(word(num));
         ck->write(map_channel_2->uv_verts.begin(), num * sizeof(S_vector2));
                                 //write faces
         num = map_channel_2->uv_faces.size();
         ck->WriteWord(word(num));
         ck->write(map_channel_2->uv_faces.begin(), num * sizeof(I3D_triface));
                                 //write texture name
         num = map_channel_2->bitmap_name.Size();
         ck->WriteWord(word(num));
         ck->write((const char*)map_channel_2->bitmap_name, num);
      }
   }
   return true;
}

//----------------------------

bool I3D_mesh_base::LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){

   {                          //read and check version
      word v = ck->ReadWord();
      if(v!=DBASE_VERSION)
         return false;
      dword fvf = ck->ReadDword();
      if(vertex_buffer.GetFVFlags()!=fvf){
         vertex_buffer.SetFVFlags(fvf);
         //return false;
      }
      /**/
      /*
      word v = 0;
      ck->read(&v, sizeof(word));
      if(v!=DBASE_VERSION)
         return false;
      dword fvf = 0;
      ck->read(&fvf, sizeof(dword));
      if(vertex_buffer.fvflags!=fvf)
         return false;
      /**/
   }
   word num;
   dword rl;
   {                       //read vertices
      rl = ck->read(&num, sizeof(num));
      if(rl!=sizeof(num))
         return false;
      vertex_buffer.AllocVertices(num);
      void *verts = vertex_buffer.Lock();
      rl = ck->read(verts, num * vertex_buffer.GetSizeOfVertex());
#ifdef GL
      assert(vertex_buffer.vbo);
      glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer.vbo);
      glBufferData(GL_ARRAY_BUFFER, num*vertex_buffer.GetSizeOfVertex(), verts, GL_STATIC_DRAW);
#endif
      vertex_buffer.Unlock();
      if(rl!=num * vertex_buffer.GetSizeOfVertex())
         return false;
   }
   {                       //read spread map
      rl = ck->read(&num, sizeof(num));
      if(rl!=sizeof(num))
         return false;
      vertex_buffer.vertex_map.resize(num, 0);
      if(num){
         rl = ck->read(&vertex_buffer.vertex_map.front(), num*sizeof(word));
         if(rl!=num*sizeof(word))
            return false;
      }
   }
   {                       //read faces
      rl = ck->read(&num, sizeof(num));
      if(rl!=sizeof(num))
         return false;
      index_buffer.AllocFaces(num);
      I3D_triface *faces = index_buffer.Lock();
      rl = ck->read(faces, num * sizeof(I3D_triface));

#ifdef GL
      assert(index_buffer.ibo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer.ibo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, num*sizeof(I3D_triface), faces, GL_STATIC_DRAW);
#endif

      index_buffer.Unlock();
      if(rl!=num * sizeof(I3D_triface))
         return false;
   }
   if(!LoadCachedInfoFGroups(ck, fgroups, mats))
      return false;
#ifdef USE_STRIPS
   {                          //read strips
      dword num = ck->ReadDword();
      strip_info.assign(num);
      ck->read(strip_info.begin(), num*sizeof(S_fgroup_strip_info));
   }
#endif
   if(!LoadCachedInfoLods(ck, auto_lods, mats, drv))
      return false;

   rl = ck->read(&auto_lod_dists, sizeof(auto_lod_dists));
   if(rl!=sizeof(auto_lod_dists))
      return false;


   {                          //read smooth groups
      //byte has_sg_list;
      //ck->read(&has_sg_list, sizeof(byte));
      byte has_sg_list = ck->ReadByte();;
      if(has_sg_list){
         smooth_groups.Assign(NULL, NumFaces1());
         ck->read(&smooth_groups[0], NumFaces1() * sizeof(byte));
      }else{
         smooth_groups = NULL;
      }
   }
                              //read additional map channel
   bool mc2_present = ck->ReadByte();
   if(mc2_present){
      map_channel_2 = new C_map_channel; map_channel_2->Release();
      dword num;
                              //read vertices
      num = ck->ReadWord();
      map_channel_2->uv_verts.assign(num);
      ck->read(map_channel_2->uv_verts.begin(), num * sizeof(S_vector2));
                              //read faces
      num = ck->ReadWord();
      map_channel_2->uv_faces.assign(num);
      ck->read(map_channel_2->uv_faces.begin(), num * sizeof(I3D_triface));
                              //read texture name
      num = ck->ReadWord();
      if(num){
         char *cp = (char*)alloca(num+1);
         ck->read(cp, num);
         cp[num] = 0;
         map_channel_2->bitmap_name = cp;
      }
   }
   return true;
}

//----------------------------

void I3D_mesh_base::Clone(CPI3D_mesh_base mb, bool allow_strips){

                           //copy faces
   SetFGroups(mb->GetFGroups1(), mb->NumFGroups1());
   smooth_groups = mb->smooth_groups;
   map_channel_2 = NULL;
   if(mb->map_channel_2){
      if(allow_strips==mb->HasStrips())
         map_channel_2 = mb->map_channel_2;
      else{
         map_channel_2 = new C_map_channel;
         map_channel_2->Release();
         *map_channel_2 = *mb->map_channel_2;
      }
   }

#ifdef USE_STRIPS
   if(allow_strips==mb->HasStrips()){
                              //plain copy, no conversion
      const I3D_triface *faces = mb->index_buffer.LockConst();
      index_buffer.SetFaces(faces, mb->index_buffer.NumFaces1());
      mb->index_buffer.Unlock();
      strip_info = mb->strip_info;
   }else{
                              //copy faces through intermediate face buffer
      C_buffer<I3D_triface> faces(mb->NumFaces1());
      mb->GetFaces(faces.begin());
      SetFaces(faces.begin(), allow_strips);
   }
#else
   const I3D_triface *faces = mb->index_buffer.LockConst();
   index_buffer.SetFaces(faces, mb->NumFaces1());
   mb->index_buffer.Unlock();
#endif
                           //copy auto-lod info
   const C_auto_lod *als = mb->GetAutoLODs();
   int num_lods = mb->NumAutoLODs();
   auto_lods.assign(num_lods);
   for(int i=num_lods; i--; ){
      const C_auto_lod &s = als[i];
      C_auto_lod &d = auto_lods[i];
      d.Init(drv);
      d.ratio = s.ratio;
      d.vertex_count = s.vertex_count;
      d.fgroups = s.fgroups;
#ifdef USE_STRIPS
      if(allow_strips==mb->HasStrips()){
                              //plain copy, no conversion
         I3D_source_index_buffer &ib = const_cast<I3D_source_index_buffer&>(s.GetIndexBuffer());
         const I3D_triface *faces = s.GetIndexBuffer().LockConst();
         ib.SetFaces(faces, s.GetIndexBuffer().NumFaces1());
         ib.Unlock();
         d.strip_info = s.strip_info;
      }else{
                                 //copy faces through intermediate face buffer
         C_buffer<I3D_triface> faces(s.NumFaces1());
         s.GetFaces(faces.begin());
         d.SetFaces(faces.begin(), allow_strips);
      }
#else
      const I3D_triface *fcs = s.GetIndexBuffer().LockConst();
      I3D_source_index_buffer &ib = const_cast<I3D_source_index_buffer&>(d.GetIndexBuffer());
      ib.SetFaces(fcs, s.GetIndexBuffer().NumFaces1());
      s.GetIndexBuffer().Unlock();
#endif
   }
   memcpy(auto_lod_dists, mb->GetAutoLODDists(), sizeof(auto_lod_dists));
                           //copy vertices - only matching elements, ignore others
   C_vector<byte> new_vertices; new_vertices.resize(mb->vertex_buffer.NumVertices() * vertex_buffer.GetSizeOfVertex());
   const void *src = mb->vertex_buffer.LockConst();
   CloneVertexBuffer(src, mb->vertex_buffer.GetFVFlags(), &new_vertices.front(), vertex_buffer.GetFVFlags(),
      mb->vertex_buffer.NumVertices());
   mb->vertex_buffer.UnlockConst();
   vertex_buffer.SetVertexSpreadMap(mb->vertex_buffer.GetVertexSpreadMap());
   SetClonedVertices(new_vertices);
}

//----------------------------

void I3D_mesh_base::ComputeBoundingVolume(){

   bvolume.bbox.Invalidate();
   if(!NumVertices1()){
      bvolume.bsphere.pos.Zero();
      bvolume.bsphere.radius = 0.0f;
      return;
   }

   const S_vector *src = (const S_vector*)vertex_buffer.Lock(D3DLOCK_READONLY);
   assert(src);
   if(!src)
      return;

   const S_vector *src1 = src;
   for(int i=vertex_buffer.NumVertices(); i--; src1 = (const S_vector*)(((byte*)src1) + vertex_buffer.GetSizeOfVertex())){
      bvolume.bbox.min.Minimal(*src1);
      bvolume.bbox.max.Maximal(*src1);
   }
                              //expand by safe thresh
   for(i=0; i<3; i++){
      bvolume.bbox.min[i] -= SAFE_BOUND_THRESH;
      bvolume.bbox.max[i] += SAFE_BOUND_THRESH;
   }
                              //bounding sphere
   S_vector bbox_diagonal = bvolume.bbox.max - bvolume.bbox.min;
   bvolume.bsphere.pos = (bvolume.bbox.min + bbox_diagonal*.5f);

                              //try to make bsphere as small as possible
   src1 = src;
   bvolume.bsphere.radius = 0.0f;
   for(i = vertex_buffer.NumVertices(); i--; src1 = (const S_vector*)(((byte*)src1) + vertex_buffer.GetSizeOfVertex())){
      S_vector v = (*src1) - bvolume.bsphere.pos;
      float dist2 = v.Square();
      bvolume.bsphere.radius = Max(bvolume.bsphere.radius, dist2);
   }
   if(bvolume.bsphere.radius>MRG_ZERO){
      bvolume.bsphere.radius = I3DSqrt(bvolume.bsphere.radius);
                              //safe expansion
      bvolume.bsphere.radius += SAFE_BOUND_THRESH;
   }else
      bvolume.bsphere.radius = 0.0f;
   //bvolume.bsphere.radius = bbox_diagonal.Magnitude() * .5f;

   vertex_buffer.Unlock();
}

//----------------------------
#ifdef USE_STRIPS
#ifdef _DEBUG
static void ShowStrips(PI3D_driver drv, const word *beg, dword num_i, const I3D_source_vertex_buffer &vb){

   const void *verts = vb.LockConst();
   dword vstride = vb.GetSizeOfVertex();

   C_destripifier destrip;
   destrip.Begin(beg);
   bool was_degen = true;
   S_vector last_v(0, 0, 0);
   dword num_f = 0;
   dword num_d = 0;
   for(dword j=num_i-2; j--; ){
      destrip.Next();
      const I3D_triface &fc = destrip.GetFace();
      const S_vector &v0 = *(S_vector*)(((byte*)verts) + vstride*fc[0]),
         &v1 = *(S_vector*)(((byte*)verts) + vstride*fc[1]),
         &v2 = *(S_vector*)(((byte*)verts) + vstride*fc[2]);
      if(!destrip.IsDegenerate()){
         ++num_f;
                              //show point
         S_vector v = (v0+v1+v2)/3.0f;
         float sz = Min((v0-v1).Square(), Min((v1-v2).Square(), (v2-v0).Square()));
         drv->DebugPoint(v, I3DSqrt(sz)*.1f, 0, 0xff00ff00);
         if(!was_degen){
            drv->DebugLine(v, last_v, 0, 0xff00ff00);
         }
         was_degen = false;
         last_v = v;
      }else{
         ++num_d;
         was_degen = true;
         const S_vector *vs = &v0, *ve = &v1;
         if(fc[0]==fc[1])
            ve = &v2;
         drv->DebugLine(*vs, *ve, 0, 0xffff0000);
      }
   }
   vb.UnlockConst();
   drv->PRINT(C_xstr("Good: %, Degen: %, ind: % (#%%%)") %num_f %num_d %num_i %int(100.0f*(float)num_i/(float)(num_f*3)));
}
#endif

//----------------------------

static void RemapMapChannel(I3D_triface *buf, dword num, const word *remap, const I3D_triface *orig_faces, const word *strip){

   C_buffer<I3D_triface> src(buf, buf+num);
   C_destripifier destrip;
   destrip.Begin(strip);

   for(dword i=0; i<num; i++){
      const I3D_triface &fc_orig = orig_faces[remap[i]];
      do{
         destrip.Next();
      }while(destrip.IsDegenerate());
      const I3D_triface &fc_curr = destrip.GetFace();
      int beg_i = fc_orig.FindIndex(fc_curr[0]);
      assert(beg_i!=-1);

      const I3D_triface &uv_o = src[remap[i]];
      I3D_triface &uv_c = buf[i];
      uv_c[0] = uv_o[beg_i];
      uv_c[1] = uv_o[next_tri_indx[beg_i]];
      uv_c[2] = uv_o[prev_tri_indx[beg_i]];
   }
}
#endif
//----------------------------

void I3D_mesh_base::SetFaces(const I3D_triface *faces, bool allow_strips){

#ifndef USE_STRIPS
   index_buffer.SetFaces(faces, NumFaces1());
#else
   if(allow_strips){
      strip_info.assign(fgroups.size());
      C_vector<word> all_strips;
      all_strips.reserve(NumFaces1()*2);
      C_buffer<word> face_remap(NumFaces1());

      for(dword i=0; i<fgroups.size(); i++){
         const I3D_face_group &fg = fgroups[i];
         C_vector<word> strips;
         MakeStrips(&faces[fg.base_index], fg.num_faces, strips, face_remap.begin());
#if defined _DEBUG && 0
         ShowStrips(drv, strips.begin(), strips.size(), vertex_buffer);
#endif

         S_fgroup_strip_info &si = strip_info[i];
         si.base_index = all_strips.size();
         si.num_indicies = strips.size();

         all_strips.insert(all_strips.end(), strips.begin(), strips.end());

                              //remap smooth groups
         if(smooth_groups.Size()){
            char *buf = &smooth_groups[fg.base_index];
            C_buffer<char> src(buf, buf+fg.num_faces);
            for(dword i=fg.num_faces; i--; )
               buf[i] = src[face_remap[i]];
         }
                              //remap uv channel #2
         if(map_channel_2){
            I3D_triface *buf = &map_channel_2->uv_faces[fg.base_index];
            RemapMapChannel(buf, fg.num_faces, &face_remap.front(), faces, &strips.front());
         }
      }
                              //align indicies number to number dividible by 3
      while(all_strips.size()%3)
         all_strips.push_back(0);
      index_buffer.SetFaces((I3D_triface*)&all_strips.front(), all_strips.size() / 3);
   }else{
      strip_info.clear();
      index_buffer.SetFaces(faces, NumFaces1());
   }
#endif
}

//----------------------------

void I3D_mesh_base::ApplyPivotAndNUScale(const S_vector &p, const S_vector &s){

   if(!p.IsNull() || s != S_vector(1.0f, 1.0f, 1.0f)){
      S_vector *verts = (S_vector*)vertex_buffer.Lock(), *vp = verts;
      dword vstride = vertex_buffer.GetSizeOfVertex();
      for(int i=vertex_buffer.NumVertices(); i--; vp = (S_vector*)(((byte*)vp) + vstride)){
         *vp -= p;
         *vp *= s;
      }
                              //adjust normals, if scale is negative
      if(s.x<0.0f || s.y<0.0f || s.z<0.0f){
         int ni = GetVertexComponentOffset(vertex_buffer.GetFVFlags(), D3DFVF_NORMAL);
         if(ni!=-1){
            S_vector n_scale(1, 1, 1);
            if(s.x<0.0f) n_scale.x = -1;
            if(s.y<0.0f) n_scale.y = -1;
            if(s.z<0.0f) n_scale.z = -1;

            S_vector *np = (S_vector*)(((byte*)verts) + ni);
            for(int i=vertex_buffer.NumVertices(); i--; np = (S_vector*)(((byte*)np) + vstride)){
               *np *= n_scale;
            }
         }
      }
      vertex_buffer.Unlock();

      /*
                              //flip faces, if scale negates them
      if(s.x*s.y*s.z < 0.0f){
         PI3D_triface fcs = index_buffer.Lock();
         if(HasStrips()){
            for(int i=fgroups.size(); i--; ){
               I3D_triface &fc = *(I3D_triface*)(((word*)fcs) + strip_info[i].base_index);
               swap(fc[1], fc[2]);
            }
         }else{
            for(int i=NumFaces1(); i--; ){
               I3D_triface &fc = fcs[i];
               swap(fc[0], fc[1]);
            }
         }
         index_buffer.Unlock();
      }
      */
   }
}

//----------------------------
//----------------------------
