#include "all.h"
#include "visual.h"
#include "mesh.h"
#include "procedural.h"

//----------------------------
// Author: Michal Bacik
//----------------------------

#define DBASE_VERSION 0x0001
#define MAX_TICK_COUNT 500

#ifdef _DEBUG

//#define USE_DEBUG_CAM "cam"
//#define DEBUG_NO_FALLING
//#define DEBUG_NO_PROCEDURAL

#endif

//----------------------------
//----------------------------

class I3D_atmos: public I3D_visual{
#define ATMOS_RESET     1  //content need to be reset in next update
#define ATMOS_VB_RESIZE 2  //resize vertex buffer

   dword atmos_flags;

   struct S_vertex{
      S_vector xyz;
      short u, v;
   };

                              //parameters:
   C_smart_ptr<I3D_material> mat;

   struct S_element{
      S_vector pos;
      S_vector base_pos;
      dword proc_channel;     //channel of procedural (index)

      S_element():
         base_pos(0, 0, 0),
         pos(0, 0, 0),
         proc_channel(0)
      {}
   };
   C_buffer<S_element> elems;
   S_vector2 elem_scale;
   float element_bs_radius;   //radius of element's bounding sphere
   float depth;
   S_vector fall_dir;         //falling direction (and speed)

                              //source of morphing
   C_smart_ptr<I3D_procedural_morph> procedural;

   /*
                              //anti-work regions
   struct S_region{
      I3D_bbox bb;
      S_matrix m, m_inv;
   };
   C_vector<S_region> regions;
   */

   I3D_source_index_buffer index_buffer;
   S_plane current_clip_planes[5];  //current frustum for which elements are valid
   S_matrix current_cam_mat;
   dword draw_count;
   dword num_elements;        //at fov = 65 degrees

#ifdef GL
   I3D_dest_vertex_buffer vertex_buffer;
#endif

//----------------------------

   void SetNumElements(dword num){

      if(elems.size()==num)
         return;
      vertex_buffer.DestroyD3DVB();
      index_buffer.AllocFaces(0);
      elems.assign(num);

      atmos_flags |= ATMOS_RESET;
   }

//----------------------------

   void SetElementScale(const S_vector2 &s){
      elem_scale = s;
      element_bs_radius = s.Magnitude();
      atmos_flags |= ATMOS_RESET;
   }

//----------------------------

   void SetDepth(float f){
      depth = f;
      atmos_flags |= ATMOS_RESET;
   }

//----------------------------

   void ComputeCornerPoints(const S_view_frustum &vf, const S_matrix &m_inv, S_vector pt[4]) const{

      for(dword i=4; i--; ){
         S_vector dir = (vf.frustum_pts[i] - vf.view_pos) % m_inv;
         pt[i] = dir * (depth / dir.z);
      }
   }

//----------------------------

   static float GetRandomF(){
      const float R_RAND_MAX = 1.0f / (float)RAND_MAX;
      return rand() * R_RAND_MAX;
   }

//----------------------------
// Get random position inside of AA bounding box (defined by single direction vector in positive axes),
// in which a cut pyramid is written.
   void GetRandomPos(const S_plane loc_clip_planes[5], const S_vector &vf_dir, S_vector &v) const{

      S_vector half_vf_dir = vf_dir * .5f;

      while(true){
         //v = S_vector(S_float_random(), S_float_random(), .1f + S_float_random(.9f)) * vf_dir;
         v = S_vector(GetRandomF(), GetRandomF(), .1f + GetRandomF()*.9f) * vf_dir;
         if(v.DistanceToPlane(loc_clip_planes[VF_TOP]) < 0.0f &&
            v.DistanceToPlane(loc_clip_planes[VF_RIGHT]) < 0.0f)
            break;
      }
      if(I3DFloatAsInt(v.x)&1) v.x = -v.x;
      if(I3DFloatAsInt(v.y)&1) v.y = -v.y;
   }

//----------------------------

   void InitializeRandomElements(const S_plane clip_planes[5], const S_vector &vf_dir,
      const S_matrix &m, const S_matrix &m_inv, int from_index = 0){

      S_plane loc_clip_planes[5];
      for(dword i=4; i--; )
         loc_clip_planes[i] = clip_planes[i] * m_inv;

      for(int ei=elems.size()-1; ei >= from_index; ei--){
         S_element &el = elems[ei];
         GetRandomPos(loc_clip_planes, vf_dir, el.base_pos);
         el.base_pos *= m;
         el.pos = el.base_pos;
      }
                              //save current vf
      memcpy(current_clip_planes, clip_planes, sizeof(current_clip_planes));
      current_cam_mat = m;
   }

//----------------------------
// Get time delta since last render. The returned value is not grater than some optimal value,
// so that we don't waste time on updating unnecessary elements.
   int GetTickTime() const{
      int count = drv->GetRenderTime();
      int delta = count - last_render_time;
      return Max(0, Min(MAX_TICK_COUNT, delta));
   }

//----------------------------

   bool __cdecl CheckPointInVF(const S_vector &p, const S_plane clip_planes[5], float thresh) const{

#if 1
      bool rtn = false;

      __asm{
         mov eax, p
         fld dword ptr[eax+8]
         fld dword ptr[eax+4]
         fld dword ptr[eax+0]
         fld thresh
         mov esi, clip_planes
         mov ecx, 5
                              //registers:
                              // esi = clip_planes
                              // st0 = thresh
                              // st1 = p.x
                              // st2 = p.y
                              // st3 = p.z
      loop1:
         fld dword ptr[esi+0]
         fmul st, st(2)
         fld dword ptr[esi+4]
         fmul st, st(4)
         faddp st(1), st
         fld dword ptr[esi+8]
         fmul st, st(5)
         faddp st(1), st
         fadd dword ptr[esi+0xc]
         fcomp st(1)
         fnstsw ax
         test ah, 0x41
         je out_all
         add esi, SIZE S_plane
         dec ecx
         jnz loop1

         mov rtn, 1
      out_all:
         push eax
         fstp dword ptr[esp]
         fstp dword ptr[esp]
         fstp dword ptr[esp]
         fstp dword ptr[esp]
         pop eax
      }
      return rtn;
#else
      if(p.DistanceToPlane(clip_planes[3]) > thresh) return false;
      if(p.DistanceToPlane(clip_planes[0]) > thresh) return false;
      if(p.DistanceToPlane(clip_planes[1]) > thresh) return false;
      if(p.DistanceToPlane(clip_planes[2]) > thresh) return false;
      if(p.DistanceToPlane(clip_planes[4]) > thresh) return false;
      return true;
#endif
   }

   /*
//----------------------------
// Collect all ours regions, which collide with view frustum.
   void CollectTouchedRegions(const S_plane clip_planes[5], C_vector<dword> &touch_regions){

      for(dword i=regions.size(); i--; ){

      }
   }
   */

//----------------------------
// Update elements and prepare billboards in destination VB.
// The returned value is number of elements currently visible.
   dword UpdateElementsAndPrepareVB(const S_plane clip_planes[5], const S_vector &vf_dir, const S_matrix &m,
      const S_matrix &m_inv, S_vertex *verts, int time, dword want_num_elems){

      float tsec = (float)time * .001f;
      S_vector curr_fall_dir = fall_dir * tsec;
#ifdef DEBUG_NO_FALLING
      curr_fall_dir.Zero();
#endif

      /*
                              //collect affected regions
      C_vector<dword> touch_regions;
      touch_regions.reserve(regions.size());
      CollectTouchedRegions(clip_planes, touch_regions);
      */

      S_plane loc_clip_planes[5];
      for(dword i=4; i--; )
         loc_clip_planes[i] = clip_planes[i] * m_inv;

                              //vector by which we return out-of-frustum elements back to opposite side of frustum
      float fall_dir_mag = fall_dir.Magnitude();
      S_vector back_dir = fall_dir;
      {
         float rnd_size = fall_dir_mag * .5f;
         back_dir.x += (GetRandomF()-.5f) * rnd_size;
         back_dir.y += (GetRandomF()-.5f) * rnd_size;
         back_dir.z += (GetRandomF()-.5f) * rnd_size;
      }
      S_normal back_dir_normal = back_dir;

                              //billboard rectangle
                              // initially in camera space
      S_vector dir_x(elem_scale.x, 0, 0);
      S_vector dir_y(0, -elem_scale.y, 0);
      if(elem_scale.x != elem_scale.y){
         S_matrix m_roll;

         m_roll.Identity();
         m_roll(1) = -S_normal(fall_dir);
         m_roll(2) = m(2);
         m_roll(0) = m_roll(1).Cross(m_roll(2));
         m_roll(0).Normalize();
                              //put billboard rectangle into world coords
         dir_x %= m_roll;
         dir_y %= m_roll;

         /*
         drv->DebugLine(m(3) + m(2), m(3) + m(2) + fall_dir, 1);
         drv->DebugLine(m(3) + m(2), m(3) + m(2) + dir_x, 1);
         drv->DebugLine(m(3) + m(2), m(3) + m(2) + dir_y, 1);
         /**/
      }else{
         dir_x %= m;
         dir_y %= m;
      }
      S_vector bill_pos[4];
      bill_pos[0] = (dir_x + dir_y) * -.5f;
      bill_pos[1] = bill_pos[0] + dir_x;
      bill_pos[2] = bill_pos[0] + dir_y;
      bill_pos[3] = bill_pos[0] + dir_x + dir_y;

      dword num_visible_elems = 0;

      S_vectorw tmp(0, 0, 0, 0);
      const S_vectorw *morphs = &tmp;
      if(procedural){
         procedural->Evaluate();
         morphs = procedural->GetMorphingValues();
      }
      bool do_test_sides = (memcmp(current_clip_planes, clip_planes, sizeof(current_clip_planes)));

      dword new_elem_count = elems.size();
      S_element *el_ptr = elems.begin();
      for(dword ei=elems.size(); ei--; ){
         S_element &el = *el_ptr++;

         //bool chech_this = ((ei&7)==draw_count);
         bool chech_this = true;
                              //check if it is currently in new frustum
         //if(vf_changed && !CheckPointInVF(el.pos, clip_planes, element_bs_radius))
         if(do_test_sides && chech_this && !CheckPointInVF(el.pos, clip_planes, element_bs_radius)){
                              //if we want shrinking, then just forget this
            if(new_elem_count > want_num_elems){
               --new_elem_count;
               el = elems[new_elem_count];
               continue;
            }

                              //move to newly initialized frustum
            GetRandomPos(loc_clip_planes, vf_dir, el.base_pos);
            el.base_pos *= m;
            //++debug_count[0];
                              //if in old frustum, move it out of that
            if(CheckPointInVF(el.base_pos, current_clip_planes, 0.0f)){
               //++debug_count[1];
               int fail_check = 0;
               while(true){
                              //make random transfer direction
                  //S_vector dir(S_float_random(), S_float_random(), S_float_random());
                  S_vector dir(GetRandomF(), GetRandomF(), GetRandomF());
                  dir -= S_vector(.5f, .5f, .5f);
                  if(dir.IsNull())
                     continue;
                              //check if old box ends earlier than new box
                  float old_closest_dist = 1e+16f;
                  float new_closest_dist = 1e+16f;
                  for(dword i=5; i--; ){
                     float d;
                     if(dir.Dot(clip_planes[i].normal) >= 0.0f){
                        if(clip_planes[i].IntersectionPosition(el.base_pos, dir, d))
                           new_closest_dist = Min(new_closest_dist, d);
                     }
                     if(dir.Dot(current_clip_planes[i].normal) >= 0.0f){
                        if(current_clip_planes[i].IntersectionPosition(el.base_pos, dir, d))
                           old_closest_dist = Min(old_closest_dist, d);
                     }
                  }
                  if(old_closest_dist <= new_closest_dist){
                     S_vector move_dir = dir * (old_closest_dist + GetRandomF()*(new_closest_dist - old_closest_dist));
                     move_dir += S_normal(dir) * element_bs_radius;
                     //drv->DebugLine(el.base_pos, el.base_pos + move_dir, 0);
                     el.base_pos += move_dir;
                     assert(!CheckPointInVF(el.base_pos, current_clip_planes, -element_bs_radius));
                     break;
                  }
                  //++debug_count[2];
                  if(++fail_check == 5)
                     break;
               }
            }
         }
                              //animate
         el.base_pos += curr_fall_dir;
         el.pos = el.base_pos;
         el.pos += morphs[el.proc_channel];
         if(chech_this && !CheckPointInVF(el.pos, clip_planes, element_bs_radius)){
                              //if we want shrinking, then just forget this
            if(new_elem_count > want_num_elems){
               --new_elem_count;
               el = elems[new_elem_count];
               continue;
            }
                              //put it back on opposite side of fall diection,
                              // onto 1st collision with plane
            float closest_col = -10.0f;
            do{
               bool found = false;
               for(dword i=5; i--; ){
                  if(back_dir.Dot(clip_planes[i].normal) >= 0.0f)
                     continue;
                  float d;
                  if(clip_planes[i].IntersectionPosition(el.pos, back_dir, d)){
                     if(closest_col < d && d<10.0f){
                        closest_col = d;
                        found = true;
                     }
                  }
               }
               if(found)
                  break;
                              //compute better back dir
               back_dir = fall_dir;
               float rnd_size = fall_dir_mag * .5f;
               back_dir.x += (GetRandomF()-.5f) * rnd_size;
               back_dir.y += (GetRandomF()-.5f) * rnd_size;
               back_dir.z += (GetRandomF()-.5f) * rnd_size;
            }while(true);
            //assert(closest_col < 0.0f);
            //assert(closest_col > -9.99f);
            assert(closest_col > -10.0f);
                              //move onto closest non-facing plane
            el.base_pos = el.pos + back_dir * closest_col;
                              //put is very slightly outside the frustum
            el.base_pos -= back_dir_normal * element_bs_radius;
            //drv->DebugLine(el.base_pos, el.pos, 2);
                              //animate current fall direction
            el.base_pos += curr_fall_dir;
                              //counter-accomodate for morphing
            el.base_pos -= morphs[el.proc_channel];

            //++debug_count[3];

            el.pos = el.base_pos + morphs[el.proc_channel];
         }

                              //update vertices in VB
#if 0
         __asm{
            mov esi, el
            mov edi, verts
            lea edx, bill_pos
                              //v0
            fld dword ptr[esi+0]
            fadd dword ptr[edx+0x00]
            fstp dword ptr [edi+0]

            fld dword ptr[esi+4]
            fadd dword ptr[edx+0x04]
            fstp dword ptr [edi+4]

            fld dword ptr[esi+8]
            fadd dword ptr[edx+0x08]
            fstp dword ptr [edi+8]
                              //v1
            fld dword ptr[esi+0]
            fadd dword ptr[edx+0x0c]
            fstp dword ptr [edi+16+0]

            fld dword ptr[esi+4]
            fadd dword ptr[edx+0x10]
            fstp dword ptr [edi+16+4]

            fld dword ptr[esi+8]
            fadd dword ptr[edx+0x14]
            fstp dword ptr [edi+16+8]
                              //v2
            fld dword ptr[esi+0]
            fadd dword ptr[edx+0x18]
            fstp dword ptr [edi+32+0]

            fld dword ptr[esi+4]
            fadd dword ptr[edx+0x1c]
            fstp dword ptr [edi+32+4]

            fld dword ptr[esi+8]
            fadd dword ptr[edx+0x20]
            fstp dword ptr [edi+32+8]
                              //v3
            fld dword ptr[esi+0]
            fadd dword ptr[edx+0x24]
            fstp dword ptr [edi+48+0]

            fld dword ptr[esi+4]
            fadd dword ptr[edx+0x28]
            fstp dword ptr [edi+48+4]

            fld dword ptr[esi+8]
            fadd dword ptr[edx+0x2c]
            fstp dword ptr [edi+48+8]
         }
         /*
         assert(verts[0].xyz == el.pos + bill_pos[0]);
         assert(verts[1].xyz == el.pos + bill_pos[1]);
         assert(verts[2].xyz == el.pos + bill_pos[2]);
         assert(verts[3].xyz == el.pos + bill_pos[3]);
         */
#else
                              //note: verts is read-only!
         verts[0].xyz = el.pos + bill_pos[0];
         verts[1].xyz = el.pos + bill_pos[1];
         verts[2].xyz = el.pos + bill_pos[2];
         verts[3].xyz = el.pos + bill_pos[3];
#endif
         verts += 4;
         ++num_visible_elems;
      }
      if(new_elem_count != elems.size())
         elems.resize(new_elem_count);
                              //save current vf
      memcpy(current_clip_planes, clip_planes, sizeof(current_clip_planes));
      current_cam_mat = m;

      return num_visible_elems;
   }

//----------------------------

public:
   I3D_atmos(PI3D_driver d):
      I3D_visual(d),
      depth(0.0f),
      fall_dir(0, 0, 0),
      draw_count(0),
      elem_scale(0, 0),
#ifdef GL
      vertex_buffer(d),
#endif
      num_elements(100)
   {
      visual_type = I3D_VISUAL_ATMOS;
      index_buffer.Init(drv);
      //SetNumElements(100);
      SetDepth(5.0f);
      SetElementScale(S_vector2(.05f, .5f));

      //fall_dir = S_vector(.4f, -.8f, -.3f);
      fall_dir = S_vector(0, -.8f, -.8f);

      {
         C_vector<D3DVERTEXELEMENT9> els;
         els.push_back(S_vertex_element(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
         els.push_back(S_vertex_element(12, D3DDECLTYPE_SHORT2, D3DDECLUSAGE_TEXCOORD, 0));
         els.push_back(D3DVERTEXELEMENT9_END);
         vertex_buffer.vs_decl = drv->GetVSDeclaration(els);
      }
   }

public:
                              //specialized methods of I3D_visual:

//#define FLARE_BSPHERE_RADIUS .70711f

   virtual bool ComputeBounds(){
      bound.bound_local.bbox.min = S_vector(-1e+16f, -1e+16f, -1e+16f);
      bound.bound_local.bbox.max = S_vector( 1e+16f,  1e+16f,  1e+16f);
      bound.bound_local.bsphere.pos.Zero();
      bound.bound_local.bsphere.radius = 1e+16f;

      vis_flags |= VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;
      return true;
   }

//----------------------------

   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
//----------------------------

   virtual bool AttachProcedural(PI3D_procedural_base pb){

                              //check if procedural is of expected type
      if(pb->GetID() == PROCID_MORPH_3D){
#ifndef DEBUG_NO_PROCEDURAL
         procedural = (I3D_procedural_morph*)pb;
#endif
         return true;
      }
      return false;
   }

//----------------------------
   /*
   virtual bool AddRegion(const I3D_bbox &bb, const S_matrix &m, int index){

      regions.push_back(S_region());
      S_region &r = regions.back();
      r.bb = bb;
      r.m = m;
      r.m_inv = ~m;
      return true;
   }
   */
//----------------------------

   I3DMETHOD(Duplicate)(CPI3D_frame frm){

      if(frm==this)
         return I3D_OK;
      if(frm->GetType1()!=FRAME_VISUAL)
         return I3D_frame::Duplicate(frm);
      CPI3D_visual vis = I3DCAST_CVISUAL(frm);
      switch(vis->GetVisualType1()){
      case I3D_VISUAL_ATMOS:
         {
            I3D_atmos *atm = (I3D_atmos*)vis;
            SetMaterial(atm->GetMaterial());
         }
         break;
      default:
         SetMeshInternal(const_cast<PI3D_visual>(vis)->GetMesh());
      }
      return I3D_visual::Duplicate(vis);
   }

//----------------------------

   void SetMeshInternal(PI3D_mesh_base mesh){

      if(mesh){
         SetMaterial(mesh->GetFGroups1()->mat);
      }else{
         PI3D_material mat = drv->CreateMaterial();
         SetMaterial(mat);
         mat->Release();
      }
   }

//----------------------------

   bool SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const{

      word version = DBASE_VERSION;
      ck->write(&version, sizeof(word));
      for(int i=mats.size(); i--; ){
         if(mats[i]==mat)
            break;
      }
      assert(i!=-1);
      if(i==-1)
         return false;
      ck->write(&i, sizeof(word));
      return true;
   }

//----------------------------

   virtual bool LoadCachedInfo(C_cache *ck, C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){
      {
         //word v = 0;
         //ck->read((char*)&v, sizeof(word));
         word v = ck->ReadWord();
         if(v!=DBASE_VERSION)
            return false;
      }
      //word mat_id = 0;
      //ck->read((char*)&mat_id, sizeof(word));
      word mat_id = ck->ReadWord();
      mat = mats[mat_id];
      return true;
   }

//----------------------------
   /*
   I3DMETHOD(DebugDraw)(PI3D_scene scn) const{

      S_vector bb_full[8];
      for(dword i=regions.size(); i--; ){
         const S_region &rg = regions[i];
         scn->SetTransformVertsMatrix(&rg.m);
         rg.bb.Expand(bb_full);
         scn->DebugDrawBBox(bb_full, 0xff0000ff);
      }
      return I3D_OK;
   }
   */
//----------------------------

   void InitVertexBuffer(dword num_elems){

                              //use XYZ + DIFFUSE, diffuse dword is 'fake' fvf for keeping 2 short uv coords
      vertex_buffer.CreateD3DVB(D3DFVF_XYZ | D3DFVF_DIFFUSE, num_elems*4);

      D3D_lock<byte> v_dst(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.D3D_vertex_buffer_index, num_elems*4,
         vertex_buffer.GetSizeOfVertex(), 0);
      v_dst.AssureLock();
      S_vertex *verts = (S_vertex*)(byte*)v_dst;

      for(dword ei=num_elems; ei--; verts += 4){
         for(dword i=4; i--; ){
            S_vertex &v = verts[i];
            v.u = short((i&1) ? 1 : 0);
            v.v = short((i&2) ? 1 : 0);
         }
      }
                           //create index buffer
      index_buffer.AllocFaces(num_elems * 2);
      PI3D_triface faces = index_buffer.Lock();
      for(dword fi=num_elems; fi--; ){
         I3D_triface &fc0 = faces[fi*2+0];
         I3D_triface &fc1 = faces[fi*2+1];
         //dword base = fi*4; //MSVC optimization bug
         fc0[0] = word(fi*4 + 0);
         fc0[1] = word(fi*4 + 1);
         fc0[2] = word(fi*4 + 2);
         fc1[0] = word(fi*4 + 2);
         fc1[1] = word(fi*4 + 1);
         fc1[2] = word(fi*4 + 3);
      }
      index_buffer.Unlock();
   }

//----------------------------

   dword PrepareEffect(const S_preprocess_context &pc, int time){

      dword want_num_elems;
      {
         float fov = pc.scene->GetActiveCamera()->GetFOV();
         const float ref_fov = (65.0f * PI / 180.0f);
         want_num_elems = FloatToInt(num_elements * (fov*fov) / (ref_fov*ref_fov));
      }
      

      srand(S_int_random(0));
      //srand(0);
      PI3D_camera cam = pc.scene->GetActiveCamera1();
#ifdef USE_DEBUG_CAM
      cam = I3DCAST_CAMERA(pc.scene->FindFrame(USE_DEBUG_CAM, ENUMF_CAMERA));
      if(!cam)
         return;
      S_view_frustum vf;
      I3D_bsphere vf_bs;
      cam->PrepareViewFrustum(vf, vf_bs, pc.scene->GetInvAspectRatio());
#else
      const S_view_frustum &vf = pc.view_frustum;
#endif
      const S_matrix &m = cam->GetMatrix();
      const S_matrix &m_inv = cam->GetInvMatrix();

      S_plane clip_planes[5];
      memcpy(clip_planes, vf.clip_planes, sizeof(clip_planes));
                              //adjust far clipping plane
      clip_planes[4].d += (cam->GetFCP() - depth);

                              //compute top-right edge direction vector
      S_vector vf_dir = (vf.frustum_pts[VF_RIGHT] - vf.view_pos) % m_inv;
      vf_dir *= (depth / vf_dir.z);

                              //create vertex buffer, if not yet
      if(elems.size() < want_num_elems){
         dword num_channels = !procedural ? 0 : procedural->GetNumChannels();
         if(elems.size() < want_num_elems){
            dword curr_count = elems.size();
            elems.resize(want_num_elems);
            for(dword i=curr_count; i<want_num_elems; i++)
               elems[i].proc_channel = (!num_channels) ? 0 : (dword)S_int_random(num_channels);
            if(!(atmos_flags&ATMOS_RESET))
               InitializeRandomElements(clip_planes, vf_dir, m, m_inv, curr_count);
         }
         vertex_buffer.DestroyD3DVB();
      }
      if(atmos_flags & ATMOS_VB_RESIZE){
         atmos_flags &= ~ATMOS_VB_RESIZE;
         vertex_buffer.DestroyD3DVB();
      }

      if(!vertex_buffer.GetD3DVertexBuffer()){
         InitVertexBuffer(want_num_elems);
      }

      dword num_elems;
                              //prepare transformed vertices
      {

         if(atmos_flags&ATMOS_RESET){
            atmos_flags &= ~ATMOS_RESET;
            InitializeRandomElements(clip_planes, vf_dir, m, m_inv);
         }

#ifdef USE_DEBUG_CAM
         {
            pc.scene->SetRenderMatrix(I3DGetIdentityMatrix());
            S_vector cv[4];
            ComputeCornerPoints(vf, cam->GetInvMatrix(), cv);
            for(dword i=4; i--; ){
               pc.scene->DrawLine(m(3), cv[i] * m, 0x8000ff00);
               pc.scene->DrawLine(cv[i] * m, cv[(i+1)&3] * m, 0x800000ff);
            }
         }
#endif
         D3D_lock<byte> v_dst(vertex_buffer.GetD3DVertexBuffer(), vertex_buffer.D3D_vertex_buffer_index, elems.size()*4,
            vertex_buffer.GetSizeOfVertex(), 0);
         v_dst.AssureLock();
         S_vertex *verts = (S_vertex*)(byte*)v_dst;

                                 //animate and prepare billboards in dest VB
         dword old_elem_count = elems.size();
         num_elems = UpdateElementsAndPrepareVB(clip_planes, vf_dir, m, m_inv, verts, time, want_num_elems);

         pc.scene->render_stats.vert_trans += 4 * num_elems;
         pc.scene->render_stats.triangle += 2 * num_elems;

                              //compact num elems, if desired number has lowered
         if(old_elem_count > want_num_elems && num_elems<=want_num_elems){
            elems.resize(want_num_elems);
            atmos_flags |= ATMOS_VB_RESIZE;
         }
      }
      return num_elems;
   }

//----------------------------

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(SetProperty)(dword index, dword value){
      switch(index){
      case I3DPROP_ATMO_F_DIR_X: fall_dir.x = I3DIntAsFloat(value); break;
      case I3DPROP_ATMO_F_DIR_Y: fall_dir.y = I3DIntAsFloat(value); break;
      case I3DPROP_ATMO_F_DIR_Z: fall_dir.z = I3DIntAsFloat(value); break;
      case I3DPROP_ATMO_F_DEPTH: SetDepth(I3DIntAsFloat(value)); break;
      case I3DPROP_ATMO_F_SCALE_X: SetElementScale(S_vector2(I3DIntAsFloat(value), elem_scale.y)); break;
      case I3DPROP_ATMO_F_SCALE_Y: SetElementScale(S_vector2(elem_scale.x, I3DIntAsFloat(value))); break;
      //case I3DPROP_ATMO_I_NUM_ELEMS: SetNumElements(value); break;
      case I3DPROP_ATMO_I_NUM_ELEMS: num_elements = value; break;
      default:
         assert(0);
         return I3DERR_INVALIDPARAMS;
      }
      return I3D_OK;
   }

//----------------------------

   I3DMETHOD_(dword,GetProperty)(dword index) const{
      switch(index){
      case I3DPROP_ATMO_F_DIR_X: return I3DFloatAsInt(fall_dir.x); break;
      case I3DPROP_ATMO_F_DIR_Y: return I3DFloatAsInt(fall_dir.y); break;
      case I3DPROP_ATMO_F_DIR_Z: return I3DFloatAsInt(fall_dir.z); break;
      case I3DPROP_ATMO_F_DEPTH: return I3DFloatAsInt(depth); break;
      case I3DPROP_ATMO_F_SCALE_X: return I3DFloatAsInt(elem_scale.x); break;
      case I3DPROP_ATMO_F_SCALE_Y: return I3DFloatAsInt(elem_scale.y); break;
      //case I3DPROP_ATMO_I_NUM_ELEMS: return elems.size(); break;
      case I3DPROP_ATMO_I_NUM_ELEMS: return num_elements; break;
      }
      assert(0);
      return 0;
   }

//----------------------------

   I3DMETHOD_(I3D_RESULT,SetMaterial)(PI3D_material mat1){
      mat = mat1;
      if(!mat)
         SetOn(false);
      return I3D_OK;
   }
   I3DMETHOD_(PI3D_material,GetMaterial()){ return mat; }
   I3DMETHOD_(CPI3D_material,GetMaterial()) const{ return mat; }
};

//----------------------------

void I3D_atmos::AddPrimitives(S_preprocess_context &pc){

   if(pc.mode!=RV_NORMAL) return;
   //if(pc.mode==RV_SHADOW_CASTER) return;

   if(!mat)
      return;
   if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
      return;

                              //add to sort list
   pc.prim_list.push_back(S_render_primitive(0, alpha, this, pc));
   S_render_primitive &p = pc.prim_list.back();
                              //determine blending mode
   //p.blend_mode = I3DBLEND_VERTEXALPHA;
   p.blend_mode = I3DBLEND_ALPHABLEND;
   p.sort_value = PRIM_SORT_ALPHA_NOZWRITE;
   ++pc.alpha_nozwrite;

   if(mat->IsAddMode())
      p.blend_mode = I3DBLEND_ADD;
   p.user1 = GetTickTime();
}

//----------------------------
#ifndef GL
void I3D_atmos::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   dword num_elems = PrepareEffect(pc, rp.user1);

   if(!num_elems)
      return;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   dword new_blend = rp.blend_mode;
   drv->SetupBlend(new_blend);
   drv->SetupTextureStage(0, D3DTOP_MODULATE);

   I3D_driver::S_vs_shader_entry_in se;
   se.AddFragment(VSF_LIGHT_BEGIN);
   se.AddFragment(VSF_LIGHT_END);
   PrepareVertexShader(NULL, 1, se, rp, pc.mode, VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV, &I3DGetIdentityMatrix());

   drv->DisableTextureStage(1);
   drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
   drv->EnableNoCull(true);   //use no culling due to mirrors

                              //setup flare's color
   S_vectorw color;
   color = mat->GetDiffuse();
   color.w = mat->GetAlpha();
   drv->SetVSConstant(VSC_AMBIENT, &color);

   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), sizeof(S_vertex));
   drv->SetVSDecl(vertex_buffer.vs_decl);
   drv->SetIndices(index_buffer.GetD3DIndexBuffer());

   HRESULT hr;
   hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, num_elems*4,
      index_buffer.D3D_index_buffer_index * 3, num_elems*2);
   CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
}
#endif
//----------------------------

void I3D_atmos::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   dword num_elems = PrepareEffect(pc, rp.user1);
   if(!num_elems)
      return;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   dword new_blend = rp.blend_mode;
   drv->SetupBlend(new_blend);

   I3D_driver::S_ps_shader_entry_in se_ps;
   se_ps.Tex(0);
   se_ps.AddFragment(PSF_MOD_t0_CONSTCOLOR);

   I3D_driver::S_vs_shader_entry_in se;
   PrepareVertexShader(NULL, 1, se, rp, pc.mode, VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_COPY_UV, &I3DGetIdentityMatrix());

   drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));
   drv->EnableNoCull(true);   //use no culling due to mirrors

   drv->SetPixelShader(se_ps);

                              //setup flare's color
   S_vectorw color;
   color = mat->GetDiffuse();
   color.w = mat->GetAlpha();
   drv->SetPSConstant(PSC_COLOR, &color);

   drv->SetStreamSource(vertex_buffer.GetD3DVertexBuffer(), sizeof(S_vertex));
   drv->SetVSDecl(vertex_buffer.vs_decl);
   drv->SetIndices(index_buffer.GetD3DIndexBuffer());

   HRESULT hr;
   hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_buffer.D3D_vertex_buffer_index, 0, num_elems*4,
      index_buffer.D3D_index_buffer_index * 3, num_elems*2);
   CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
}

//----------------------------

extern const S_visual_property props_Atmospheric[] = {
                              //I3DPROP_ATMO_F_DIR_?
   {I3DPROP_FLOAT, "Dir X", "Direction of movement of elements."},
   {I3DPROP_FLOAT, "Dir Y", NULL},
   {I3DPROP_FLOAT, "Dir Z", NULL},
                              //I3DPROP_ATMO_I_NUM_ELEMS
   {I3DPROP_INT, "Num elements", "Number of elements being generated."},
                              //I3DPROP_ATMO_F_DEPTH
   {I3DPROP_FLOAT, "Depth", "Depth of effect - defined as distance from camera in Z axis."},
                              //I3DPROP_ATMO_F_SCALE_X
   {I3DPROP_FLOAT, "Scale X", "Scale of element in X axis."},
   {I3DPROP_FLOAT, "Scale Y", NULL},
   {I3DPROP_NULL}
};

I3D_visual *CreateAtmospheric(PI3D_driver drv){
   return new I3D_atmos(drv);
}

//----------------------------
