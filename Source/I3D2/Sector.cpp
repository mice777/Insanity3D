/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Sector.cpp
   Content: Sector frame.
--------------------------------------------------------*/

#include "all.h"
#include "sector.h"
#include "light.h"
#include "camera.h"
#include "scene.h"
#include "visual.h"
#include "occluder.h"
#include "soundi3d.h"
#include "mesh.h"
#include "loader.h"

//----------------------------

#define DBASE_VERSION 0x0017

#define USE_VF_BSPHERE        //use bounding-sphere tests on viewing frustum

                              //distance under which verts are considered equal
                              // in sector and portal builtindg and portal connecting
#define VERTEXT_COLLAPSE .04f
#define ENABLE_CONTOUR_TEST   //enable testing object's contour agains vf/occluders/portals

#ifdef _DEBUG
                              //debugging:
//#define DEBUG_DRAW_FACE_NORMALS
//#define DEBUG_PAINT_BBOX_CONTOUR    //paint contour of bounding box
//#define DEBUG_NO_CACHE
#endif

//----------------------------
//----------------------------
                              //portal class
I3D_portal::I3D_portal(PI3D_sector s, dword init_flags):
   ref(1),
   owning_sector(s),
   state(PS_UNKNOWN),
   connect_sector(NULL),
   drv(s->GetDriver1()),
   connect_portal(NULL),
   prt_flags(PRTF_ENABLED | (init_flags&(PRTF_ONEWAY_OUT|PRTF_ONEWAY_IN)))
{
   drv->AddCount(I3D_CLID_PORTAL);
}

//----------------------------

I3D_portal::~I3D_portal(){
   drv->DecCount(I3D_CLID_PORTAL);
}

//----------------------------
                              //test if portals identical (but inversed)
bool I3D_portal::Equals(CPI3D_portal prt) const{

   dword vnum = vertices.size();
                              //vertiex count must match
   if(vnum!=prt->vertices.size())
      return false;

   const S_matrix &m = owning_sector->GetMatrix();
   const S_matrix &m1 = prt->owning_sector->GetMatrix();
                              //normals must be opposite
                              //rotate normals
   S_vector n = plane.normal.RotateByMatrix(m);
   S_vector n1 = prt->plane.normal.RotateByMatrix(m1);
                              //don't bee too strict, it's just brief test
   if(I3DFabs(n.AngleTo(n1)-PI) > 1e-1f)
      return false;

   const S_vector *vp = &vertices.front();
   const S_vector *vp1 = &prt->vertices.front();
   S_vector *v = (S_vector*)alloca(vnum*sizeof(S_vector));
   S_vector *v1 = (S_vector*)alloca(vnum*sizeof(S_vector));
   dword i;
                              //transform all his vertices
   for(i=0; i<vnum; i++)
      v1[i] = vp1[i] * m1;
                              //transform our 1st vertex & find one match
   v[0] = vp[0] * m;
   int j = vnum;
   while(j--)
      if((v[0]-v1[j]).Magnitude() <= VERTEXT_COLLAPSE)
         break;
                              
   if(j==-1)                  //no match found
      return false;
                              //transform rest of our vertices
   for(i=1; i<vnum; i++)
      v[i] = vp[i] * m;
                              //now traverse and compare rest of vertices
   for(i=1; i<vnum; i++){
      if(!j--)
         j = vnum-1;
      if((v[i]-v1[j]).Magnitude() > VERTEXT_COLLAPSE)
         break;
   }
   if(i!=vnum)
      return false;
                              //portals identical
   return true;
}

//----------------------------
                              
bool I3D_portal::Build(const C_vector<S_vector> &v){

   vertices = v;
   S_vector &n = plane.normal;
                              //get portal's normal
#if 1

                              //find 2 points with maximal distance
   int p0 = -1, p1 = -1, p2 = -1;
   float best_dist = 0.0f;
   for(int i=v.size(); i--; ){
      for(int j=i; j--; ){
         S_vector dir = v[i] - v[j];
         float d = dir.Dot(dir);
         if(best_dist<d){
            best_dist = d;
            p0 = i;
            p1 = j;
         }
      }
   }
                              //now find 3rd point with maximal distance to their line
   bool invert = false;
   assert(p0!=-1);
   S_vector dir = v[p1] - v[p0];
   best_dist = -1.0f;
   bool curr_invert = true;
   for(i = (p0+1)%v.size(); i!=p0; ++i %= v.size()){
      if(i==p1)
         curr_invert = false;
      else{
         float dtl = v[i].DistanceToLine(v[p0], dir);
         if(best_dist<dtl){
            best_dist = dtl;
            invert = curr_invert;
            p2 = i;
         }
      }
   }
   n.GetNormal(v[p0], v[p1], v[p2]);
   if(invert)
      n.Invert();
#else

   n.GetNormal(v[0], v[1], v[2]);
#endif
   if(IsMrgZeroLess(n.Magnitude()))
      return false;
   n.Normalize();
   plane.d = -n.Dot(v[0]);
   return true;
}

//----------------------------

void I3D_portal::Duplicate(CPI3D_portal prt){

   prt_flags = prt->prt_flags;
   vertices = prt->vertices;
   plane = prt->plane;
}

//----------------------------

void I3D_portal::SetOn(bool b){

   prt_flags &= ~PRTF_ENABLED;
   if(b) prt_flags |= PRTF_ENABLED;
   state = PS_UNKNOWN;
                              //make the same for connected portal
   if(connect_portal){
      connect_portal->prt_flags &= ~PRTF_ENABLED;
      if(b) connect_portal->prt_flags |= PRTF_ENABLED;
      connect_portal->state = PS_UNKNOWN;
   }
}

//----------------------------

bool I3D_portal::PrepareFrustum(const S_vector &cam_pos){

   if(vertices.size() > MAX_FRUSTUM_CLIP_PLANES)
      return false;
                              //determine on which side of portal we are
   float d = cam_pos.DistanceToPlane(plane);
   bool in_front = (d >= 0.0f);
                              //build frustum
   vf.view_pos = cam_pos;
   vf.num_clip_planes = vertices.size();   
   assert(vf.num_clip_planes <= MAX_FRUSTUM_CLIP_PLANES);
   for(int i=vf.num_clip_planes, next_i=0; i--; next_i=i){
      const S_vector &v0 = vertices[i];
      const S_vector &v1 = vertices[next_i];
                              //compute plane
      S_plane &pl = vf.clip_planes[i];
      pl.normal.GetNormal(cam_pos, v0, v1);
      pl.normal.Normalize();
      pl.d = -cam_pos.Dot(pl.normal);
      if(!in_front)
         pl.Invert();
                              //store vertex on side
      vf.frustum_pts[i] = v0;
   }
   return in_front;
}

//----------------------------
//----------------------------
                              //sector class
I3D_sector::I3D_sector(PI3D_driver d, bool primary):
   I3D_frame(d),
   fog_light(NULL),
   sound_env_id(0),
   temperature(0.0f),
   sct_flags(0)
{
   drv->AddCount(I3D_CLID_SECTOR);
   type = FRAME_SECTOR;
   enum_mask = ENUMF_SECTOR;

   if(primary)
      sct_flags |= SCTF_PRIMARY;
}

//----------------------------

I3D_sector::~I3D_sector(){

                              //disconnect lights from this
   for(int li=0; li<2; li++){
      C_vector<PI3D_light> &l = !li ? light_list : fog_light_list;

      for(dword i=0; i<l.size(); i++){
         PI3D_light lp = l[i];
         for(int j=lp->light_sectors.size(); j--; ){
            if(lp->light_sectors[j]==this){
               lp->light_sectors[j] = lp->light_sectors.back(); lp->light_sectors.pop_back();
               break;
            }
         }
         assert(j!=-1);
      }
      l.clear();
   }
                              //disconnect sounds from this
   for(dword i=0; i<sound_list.size(); i++){
      PI3D_sound sp = sound_list[i];
      for(int j=sp->sound_sectors.size(); j--; ){
         if(sp->sound_sectors[j]==this){
            sp->sound_sectors[j] = sp->sound_sectors.back(); sp->sound_sectors.pop_back();
            break;
         }
      }
      assert(j!=-1);
   }
   sound_list.clear();

   Disconnect(NULL);
   for(i=own_portals.size(); i--; ){
      own_portals[i]->SetOwningSector(NULL);
   }
   own_portals.clear();

   drv->DecCount(I3D_CLID_SECTOR);
}

//----------------------------

I3D_RESULT I3D_sector::Duplicate(CPI3D_frame frm){

   if(frm->GetType1()==FRAME_SECTOR){
      CPI3D_sector sct = I3DCAST_CSECTOR(frm);
      sct_flags = sct->sct_flags;
      vertices = sct->vertices;
      faces = sct->faces;
      bound_planes = sct->bound_planes;
      bbox = sct->bbox;
      sct_flags |= SCTF_LIGHT_LIST_RESET;

      I3D_RESULT ir = I3D_frame::Duplicate(frm);
      if(!IsSimple()){
         for(int i=sct->own_portals.size(); i--; ){
            PI3D_portal prt = new I3D_portal(this);
            own_portals.push_back(prt);
            prt->Duplicate(sct->own_portals[i]);
            prt->Release();
         }
      }
      return ir;
   }
   return I3DERR_INVALIDPARAMS;
}

//----------------------------

/*
PI3D_texture I3D_sector::GetEnvironmentTexture(PI3D_scene scene){

   if(sct_flags&SCTF_IN_ENV_MAP_CREATE)
      return NULL;
   PI3D_texture tp = env_map;
   if(!tp){
      env_map.Init(scene, 128, 0);
   }
   if(env_map.last_build_time!=scene->GetLastRenderTime())
   {
      env_map.last_build_time = scene->GetLastRenderTime();
      if(env_map){
         tp = env_map;
         sct_flags |= SCTF_IN_ENV_MAP_CREATE;
                              //render into the texture
         PI3D_camera cam = I3DCAST_CAMERA(scene->CreateFrame(FRAME_CAMERA));
         cam->SetFOV(PI*.5f);

                              //save
         C_render_target<> save_rt = drv->GetCurrRenderTarget();
         C_smart_ptr<I3D_camera> scam = scene->GetActiveCamera1();
         scene->SetActiveCamera(cam);
         bool is_aspect;
         const I3D_rectangle save_vp = scene->GetViewport1(&is_aspect);
         scene->SetViewport(I3D_rectangle(0, 0, tp->SizeX(), tp->SizeY()), true);
                                 //render
         for(int i=0; i<6; i++){
            C_cube_render_target::E_SIDE side = (C_cube_render_target::E_SIDE)i;
            env_map.SetupRenderTarget(drv, side);
            env_map.SetupCamera(cam, S_vector(0, 4, 0), side);
            scene->RenderView(0, RV_NORMAL);
         }

                                 //restore
         scene->SetViewport(save_vp, is_aspect);
         drv->SetRenderTarget(save_rt);
         scene->SetActiveCamera(scam);
         scene->SetMViewProj(scam);

         cam->Release();
         sct_flags &= ~SCTF_IN_ENV_MAP_CREATE;
      }
   }
   return tp;
}

//----------------------------

void I3D_sector::C_cube_render_target::SetupCamera(PI3D_camera cam, const S_vector &pos, E_SIDE side, const S_matrix *m_rot){

   S_vector z, y;
   switch(side){
   case SIDE_LEFT: z = S_vector(-1, 0, 0); y = S_vector(0, 1, 0); break;
   case SIDE_RIGHT: z = S_vector(1, 0, 0); y = S_vector(0, 1, 0); break;
   case SIDE_DOWN: z = S_vector(0, -1, 0); y = S_vector(0, 0, 1); break;
   case SIDE_UP: z = S_vector(0, 1, 0); y = S_vector(0, 0, -1); break;
   case SIDE_FRONT: z = S_vector(0, 0, 1); y = S_vector(0, 1, 0); break;
   case SIDE_BACK: z = S_vector(0, 0, -1); y = S_vector(0, 1, 0); break;
   default: assert(0);
   }
   cam->SetPos(pos);
   if(m_rot){
      z %= *m_rot;
      y %= *m_rot;
   }
   cam->SetDir1(z, y);
}

//----------------------------

void I3D_sector::C_cube_render_target::SetupRenderTarget(PI3D_driver drv, E_SIDE side){

   PI3D_texture tp = rt[0];
   tp->Manage(I3D_texture::MANAGE_CREATE_ONLY);
   D3DCUBEMAP_FACES cm;
   switch(side){
   case SIDE_LEFT: cm = D3DCUBEMAP_FACE_NEGATIVE_X; break;
   case SIDE_RIGHT: cm = D3DCUBEMAP_FACE_POSITIVE_X; break;
   case SIDE_DOWN: cm = D3DCUBEMAP_FACE_NEGATIVE_Y; break;
   case SIDE_UP: cm = D3DCUBEMAP_FACE_POSITIVE_Y; break;
   case SIDE_BACK: cm = D3DCUBEMAP_FACE_NEGATIVE_Z; break;
   case SIDE_FRONT: cm = D3DCUBEMAP_FACE_POSITIVE_Z; break;
   default: assert(0); cm = D3DCUBEMAP_FACE_POSITIVE_X;
   }
   IDirect3DSurface9 *surf;
   ((IDirect3DCubeTexture8*)tp->GetD3DTexture())->GetCubeMapSurface(cm, 0, &surf);
   //((IDirect3DTexture9*)tp->GetD3DTexture())->GetSurfaceLevel(0, &surf);
   assert(surf);
   drv->SetRenderTarget(C_render_target<>(surf, zb));
   surf->Release();
}

//----------------------------

I3D_RESULT I3D_sector::C_cube_render_target::Init(PI3D_scene scn, dword size, dword ct_flags){

   PI3D_driver drv = scn->GetDriver();
   I3D_CREATETEXTURE ct;
   memset(&ct, 0, sizeof(ct));
   ct.flags = ct_flags | TEXTMAP_CUBEMAP | TEXTMAP_NOMIPMAP | TEXTMAP_HINTDYNAMIC | TEXTMAP_RENDERTARGET | TEXTMAP_NO_SYSMEM_COPY;
   ct.size_x = size;
   ct.size_y = size;

   PI3D_texture tp;
   I3D_RESULT ir = drv->CreateTexture(&ct, &tp);
   if(I3D_FAIL(ir)){
      Close();
      return ir;
   }
   rt[0] = tp;
   tp->Release();
   IDirect3DSurface9 *_zb;
   ir = drv->CreateDepthBuffer(size, size, rt[0]->GetPixelFormat(), &_zb);
   if(SUCCEEDED(ir)){
      zb = _zb;
      _zb->Release();
   }else{
      Close();
   }
   return ir;
}
*/

//----------------------------

void I3D_sector::BuildHelperLightLists(){

   lights_vertex.clear();
   lights_dyn_lm.clear();
   lights_shadow.clear();

   int num_lights = light_list.size();
   const PI3D_light *lptr = light_list.size() ? &light_list.front() : NULL;
                              //make counts
   int num_v = 0, num_lm = 0, num_s = 0;
   for(int i=num_lights; i--; ){
      PI3D_light lp = *lptr++;
      dword lflg = lp->GetLightFlags();
      if(lflg&I3DLIGHTMODE_VERTEX) ++num_v;
      if(lflg&I3DLIGHTMODE_DYNAMIC_LM) ++num_lm;
      if(lflg&I3DLIGHTMODE_SHADOW) ++num_s;
   }
                              //reserve storage
   lights_vertex.reserve(num_v);
   lights_dyn_lm.reserve(num_lm);
   lights_shadow.reserve(num_s);

                              //build lists
   lptr = light_list.size() ? &light_list.front() : NULL;
   for(i=num_lights; i--; ){
      PI3D_light lp = *lptr++;
      dword lflg = lp->GetLightFlags();
      if(lflg&I3DLIGHTMODE_VERTEX) lights_vertex.push_back(lp);
      if(lflg&I3DLIGHTMODE_DYNAMIC_LM) lights_dyn_lm.push_back(lp);
      if(lflg&I3DLIGHTMODE_SHADOW) lights_shadow.push_back(lp);
   }

   sct_flags &= ~SCTF_LIGHT_LIST_RESET;
}

//----------------------------
// Collect meshes of multiple visuals, transform into world coordinates
// and put into contiguous list.
// Split vertices which are too close to each other.
//----------------------------
static void CollectMeshes(PI3D_frame *walls, dword num_walls, C_vector<S_vector> &verts, C_vector<I3D_triface> &tri_faces){

   while(num_walls--){
      PI3D_frame frm = walls[num_walls];
      if(frm->GetType1()!=FRAME_VISUAL)
         continue;
      PI3D_visual vis = I3DCAST_VISUAL(frm);
      I3D_mesh_base *mb = vis->GetMesh();
      if(!mb)
         continue;
                              //store vertices
      const S_matrix &mat = vis->GetMatrix();
      
      const S_vector *src_verts = (const S_vector*)mb->vertex_buffer.Lock(D3DLOCK_READONLY);
      dword vstride = mb->vertex_buffer.GetSizeOfVertex();
      dword vertex_base = verts.size();
      for(dword j=0; j<mb->vertex_buffer.NumVertices(); j++, src_verts = (const S_vector*)(((const byte*)src_verts) + vstride))
         verts.push_back((*src_verts) * mat);
      mb->vertex_buffer.Unlock();

      dword num_faces = mb->NumFaces1();
      dword base_index = tri_faces.size();
      tri_faces.resize(tri_faces.size()+num_faces);
      mb->GetFaces(&tri_faces[base_index]);
      //PI3D_triface faces = mb->index_buffer.Lock(D3DLOCK_READONLY);
                              //store faces - flip them now
      for(j=mb->NumFaces1(); j--; ){
         //tri_faces.push_back(*faces++);
         //I3D_triface &fc = tri_faces.back();
         I3D_triface &fc = tri_faces[base_index+j];
         swap(fc[0], fc[1]);
         for(dword l=0; l<3; l++)
            fc[l] = (word)(fc[l] + vertex_base);
      }
      //mb->index_buffer.Unlock();
   }
                              //build vertex map and remap tri_faces
   C_vector<word> v_map; v_map.resize(verts.size());
   MakeVertexMapping(&verts.front(), sizeof(S_vector),
      verts.size(), &v_map.front(), VERTEXT_COLLAPSE);

   for(int i=tri_faces.size(); i--; ){
      I3D_triface &fc = tri_faces[i];
      fc.Remap(&v_map.front());
      //for(dword j=3; j--; )
         //fc[j] = v_map[fc[j]];
                              //check if face is valid after remapping
      if(fc[0]==fc[1] || fc[1]==fc[2] || fc[2]==fc[0]){
         fc = tri_faces.back(); tri_faces.pop_back();
      }
   }
}

//----------------------------

#ifdef _DEBUG
static void PaintEdge(PI3D_driver drv, const C_vector<S_vector> &verts, const I3D_edge &e){
   drv->DebugLine(verts[e[0]], verts[e[1]], 0);
}

static void PaintNormal(PI3D_driver drv, const S_vector &v, const S_vector &n){
   drv->DebugLine(v, v+n, 0);
}
#else
#define PaintEdge(a,b,c)
#endif

//----------------------------
// Build portal from set of vertices indicies, erase used from list.
void I3D_sector::BuildPortals(const C_vector<S_vector> &verts, const C_vector<word> &prt_verts){

#define BP_PLANAR_THRESH .02f


   int i, j;

   int best_num_behind = 0;
   int best_tri = -1;         //1st point of best triangle
   S_plane best_tri_pl;       //saved plane of best triangle, so that we don't compute it twice
   best_tri_pl.normal.Zero();
   best_tri_pl.d = 0.0f;

                              //find 1st triangle, of which 1st edge has all verts
                              // behind, and which has most other verts behind
   int numv = prt_verts.size();
   for(i=numv; i--; ){
      const S_vector &v0 = verts[prt_verts[i]];
      const S_vector &v1 = verts[prt_verts[(i+1)%numv]];
      const S_vector &v2 = verts[prt_verts[(i+2)%numv]];

                              //get normal of triangle
      S_vector n_tri; n_tri.GetNormal(v0, v1, v2); n_tri.Normalize();

                              //get plane of triangle
      S_plane pl_tri(n_tri, -v0.Dot(n_tri));

                              //get normal of v01 edge
      S_vector n_edge01; n_edge01.GetNormal(v0, v1, v1+n_tri); n_edge01.Normalize();

                              //get plane of v01 edge
      S_plane pl01(n_edge01, -v0.Dot(n_edge01));

                              //check if all verts are behind edge's plane
      for(j=prt_verts.size(); j--; ){
         const S_vector &v = verts[prt_verts[j]];
         float d = v.DistanceToPlane(pl01);
         if(d > .001f)
            break;
      }
      if(j==-1){
                              //1st edge is ok
                              //check number of verts behind plane
         int num_behind = 0;
         for(j=prt_verts.size(); j--; ){
            const S_vector &v = verts[prt_verts[j]];
            float d = v.DistanceToPlane(pl_tri);
            if(d < BP_PLANAR_THRESH)
               ++num_behind;
         }
         if(best_num_behind < num_behind){
            best_num_behind = num_behind;
            best_tri = i;
            best_tri_pl = pl_tri;
         }
                              //optim (usual case for planar portals)
         if(num_behind == numv)
            break;
      }
   }
   if(best_tri == -1){
                              //error - too crazy shape for portal?
      //assert(0);
      best_tri = 0;
                              //leave it go (using 1st triangle), hoping that validity checks report the portal bug
      //return;
   }

   int beg_i, end_i;
                           //collect all planar verts in both directions
   for(end_i = (best_tri+3)%numv; end_i != best_tri; (++end_i) %= numv){
      const S_vector &v = verts[prt_verts[end_i]];
      float d = v.DistanceToPlane(best_tri_pl);
      if(I3DFabs(d) > BP_PLANAR_THRESH)
         break;
   }
   beg_i = best_tri;
   if(end_i != best_tri){
      beg_i = (best_tri + numv-1) % numv;
      do{
         const S_vector &v = verts[prt_verts[beg_i]];
         float d = v.DistanceToPlane(best_tri_pl);
         if(I3DFabs(d) > BP_PLANAR_THRESH){
                           //this one is already wrong, go back
            ++beg_i %= numv;
            break;
         }
      }while(((beg_i += numv-1) %= numv) != end_i);
   }

                           //put existing vertices into contiguous list
   C_vector<word> new_prt_verts;
   int new_best_tri = -1;
   i = beg_i;
   do{
      if(i == best_tri)
         new_best_tri = new_prt_verts.size();
      new_prt_verts.push_back(prt_verts[i]);
   }while((++i %= numv) != end_i);
   int new_numv = new_prt_verts.size();

                           //process existing vertices, remove concave ones
   C_vector<S_vector> portal_points;

   int last_good_i = new_best_tri;
   portal_points.push_back(verts[new_prt_verts[last_good_i]]);

   for(i = new_best_tri; (++i %= new_numv) != new_best_tri; ){
      const S_vector &v0 = verts[new_prt_verts[last_good_i]];
      const S_vector &v1 = verts[new_prt_verts[i]];

                           //get normal of v01 edge
      S_vector n_edge01;
      n_edge01.GetNormal(v0, v1, v1+best_tri_pl.normal);
      n_edge01.Normalize();

                           //get plane of v01 edge
      S_plane pl01(n_edge01, -v0.Dot(n_edge01));

                           //check if all other verts are behind this plane
      for(j = new_numv; j--; ){
         const S_vector &v = verts[new_prt_verts[j]];
         float d = v.DistanceToPlane(pl01);
         if(d >= .001f)
            break;
      }
      if(j==-1){
         last_good_i = i;
         portal_points.push_back(verts[new_prt_verts[i]]);
      }
   }
   assert(portal_points.size() >= 3);

                           //build portal now
   PI3D_portal prt = new I3D_portal(this);
   if(prt->Build(portal_points))
      own_portals.push_back(prt);
   prt->Release();

   if(beg_i != end_i){
                           //put remaining points into new list and repeat process
      C_vector<word> new_prt_verts;

      int new_beg_i = (beg_i+1) % numv;
      for(i=(end_i+numv-1)%numv; i != new_beg_i; ++i %= numv){
         new_prt_verts.push_back(prt_verts[i]);
      }
      BuildPortals(verts, new_prt_verts);
   }

   return;
}

//----------------------------
// Collect edges, split any edges which are on line.
static void CollectEdges(const C_vector<S_vector> &verts, C_vector<I3D_triface> &tri_faces,
   C_vector<I3D_edge> &edges, PI3D_driver drv){

   assert(!edges.size());

   C_vector<S_vector> edge_dirs;

   const float angle_thresh = PI*.0005f;
   const float line_dist_thresh = .0075f;

#if 0                         //debugging support - create and paint one more face each run
   static int debug_count1 = 18;
   int debug_count = ++debug_count1;
#endif

   for(int i=tri_faces.size(); i--; ){
      const I3D_triface &fc = tri_faces[i];
      for(int j=0; j<3; j++){
         I3D_edge e(fc[(j+1)%3], fc[j]);

         int ei = FindEdge(edges.size() ? &edges.front() : NULL, edges.size(), I3D_edge(e[1], e[0]));
         if(ei!=-1){
                              //remove existing edge
            edges[ei] = edges.back(); edges.pop_back();
            edge_dirs[ei] = edge_dirs.back(); edge_dirs.pop_back();
         }else{
            S_vector e_dir(verts[e[1]] - verts[e[0]]);
                              //compare with all other edges already in list
            for(int k=edges.size(); k--; ){
               S_vector &ed = edge_dirs[k];
               float angle = e_dir.AngleTo(ed);
               bool angle0 = (angle < angle_thresh);
               bool anglepi = (I3DFabs(angle-PI) < angle_thresh);
               if(angle0 || anglepi){
                  I3D_edge &ee = edges[k];
                              //get line segment of e on ee
                  float e_len = ed.Dot(ed);
                  assert(!IsMrgZeroLess(e_len));
                  float u0 = (verts[e[0]] - verts[ee[0]]).Dot(ed) / e_len;
                  float u1 = (verts[e[1]] - verts[ee[0]]).Dot(ed) / e_len;
                              //first check if segments overlap
                  bool overlap;
                  if(angle0){
                     overlap = (IsAbsMrgZero(u0) || IsAbsMrgZero(1.0f-u0));
                  }else{
                     overlap = (u0 >= 0.0f && u1 <= 1.0f);
                  }
                  if(overlap){
                              //check if they're really on the same line
                     float dist = (S_vector(verts[ee[0]] + edge_dirs[k] * u0) - verts[e[0]]).Magnitude();
                     if(dist < line_dist_thresh){
                        if(angle0){
                           if(e[1]==ee[0]){
                              ee[0] = e[0];
                              ed = verts[ee[1]] - verts[ee[0]];
                              e.Invalidate();
                           }else
                           if(e[0]==ee[1]){
                              ee[1] = e[1];
                              ed = verts[ee[1]] - verts[ee[0]];
                              e.Invalidate();
                           }else
                              continue;
                           dword eei = FindEdge(&edges.front(), edges.size(), I3D_edge(ee[1], ee[0]));
                           if(eei!=-1){
                              ee = edges.back(); edges.pop_back();
                              ed = edge_dirs.back(); edge_dirs.pop_back();
                              if(eei==edges.size()) eei = k;
                                                //remove existing edge
                              edges[eei] = edges.back(); edges.pop_back();
                              edge_dirs[eei] = edge_dirs.back(); edge_dirs.pop_back();
                           }
                           break;
                        }else{
#if 0                                              //debug draw new edges
                           PaintEdge(drv, verts, e);
                           PaintEdge(drv, verts, ee);
#endif
                                                   //split edges
                                                   // perform this by simply swapping indicies
                                                   // this works for all 4 possibilities of
                                                   // how these 2 edges could overlap
                                                   // (not assuming they share identical point, which shouldn't be possible due vertex-mapping made earlier)
                           if(u1 >= 1.0f)
                              swap(ee[1], e[1]);
                           else
                              swap(ee[0], e[0]);

                                                   //adjust edge dirs
                           ed = verts[ee[1]] - verts[ee[0]];
                           e_dir = verts[e[1]] - verts[e[0]];

                           if(e[0]==e[1]){
                              swap(e, ee);
                              swap(e_dir, ed);
                           }
                           if(ee[0]==ee[1]){
                              ee = edges.back(); edges.pop_back();
                              ed = edge_dirs.back(); edge_dirs.pop_back();
                           }
                           if(e[0]==e[1]){
                              e.Invalidate();
                              break;
                           }
                        }
                     }
                  }
               }
            }
            if(e.IsValid()){
               int ei = FindEdge(edges.size() ? &edges.front() : NULL, edges.size(), I3D_edge(e[1], e[0]));
               if(ei!=-1){
                              //remove existing edge
                  edges[ei] = edges.back(); edges.pop_back();
                  edge_dirs[ei] = edge_dirs.back(); edge_dirs.pop_back();
               }else{


                  edges.push_back(e);
                  edge_dirs.push_back(e_dir);
#if 0                         //debug paint the edges
                  if(!--debug_count){
                     PaintEdge(drv, verts, e);
                  }
#endif
               }
            }
         }
      }
   }
}

//----------------------------
// Build sector and its portals from set of objects (walls).
// Phases:
//    1. collect mesh - vertices and tri_faces
//       includes also welding common vertices
//    2. collect all edges
//    3. build portals
I3D_RESULT I3D_sector::Build(PI3D_frame *walls, dword num_walls, CPI3D_visual *hlp_portals, dword num_hlp_portals,
   bool simple, C_loader &ld){

   int i;
   
   C_vector<S_vector> verts;
   C_vector<I3D_triface> tri_faces;

                              //collect all edges and vertices
   CollectMeshes(walls, num_walls, verts, tri_faces);

   assert(!own_portals.size());
   if(!simple){
      sct_flags &= ~SCTF_SIMPLE;

      C_vector<I3D_edge> edges;

                              //build list of unique edges (those forming holes)
      CollectEdges(verts, tri_faces, edges, drv);

#if 0                         //debug paint edges
      {
         for(i=edges.size(); i--; )
            PaintEdge(drv, verts, edges[i]);
      }
#endif

//----------------------------
                              //build portals
                              // inputs:
                              //    - edges of hole(s)
      while(edges.size()){
         C_vector<word> prt_verts;
                              //choose starting edge
         I3D_edge e0 = edges.back(); edges.pop_back();
         word v_start = e0[0], v_curr = e0[1];
         prt_verts.push_back(v_start);
#if 0                         //debug paint edges
         {
            PaintEdge(drv, verts, e0);
         }
#endif
         S_vector start_dir(verts[v_curr] - verts[v_start]);
         S_vector last_dir = start_dir;

         const float PORTAL_ANGLE_THRESH = PI*.05f;
                              //collect chain
         for(i=edges.size(); i--; ){
            I3D_edge &e1 = edges[i];
            if(e1[0]==v_curr){
#if 0                         //debug paint edges
               {
                  PaintEdge(drv, verts, e1);
               }
#endif
                              //check angle to previous edge
               S_vector curr_dir(verts[e1[1]] - verts[v_curr]);
               float a = curr_dir.AngleTo(last_dir);
               if(a < PORTAL_ANGLE_THRESH || I3DFabs(a-PI) < PORTAL_ANGLE_THRESH){
                              //just do nothing (forget v_curr)
               }else{
                  last_dir = curr_dir;

                  prt_verts.push_back(v_curr);
               }
               v_curr = e1[1];
               e1 = edges.back(); edges.pop_back();
               if(v_curr==v_start)
                  i = 0;
               else
                  i = edges.size(); //start again
            }
         }

         assert(v_curr==v_start);   //error - incomplete hole found?
                              //check last angle
         {
            float a = start_dir.AngleTo(last_dir);
            if(a < PORTAL_ANGLE_THRESH || I3DFabs(a-PI) < PORTAL_ANGLE_THRESH){
                              //remove 1st vertex
               prt_verts[0] = prt_verts.back(); prt_verts.pop_back();
               if((prt_verts.size() >= 2) && (prt_verts[0] == prt_verts[1])){
                  prt_verts[1] = prt_verts.back(); prt_verts.pop_back();
                  prt_verts[0] = prt_verts.back(); prt_verts.pop_back();
               }
            }
         }
         if(!prt_verts.size())
            continue;
#if 0                         //debug paint edges
         {
            for(i=prt_verts.size(); i--; )
               PaintEdge(drv, verts, I3D_edge(prt_verts[i], prt_verts[(i+1)%prt_verts.size()]));
         }
#endif
         if(prt_verts.size() < 3)
            continue;

         BuildPortals(verts, prt_verts);
      }
   }else{
      sct_flags |= SCTF_SIMPLE;
   }

   CleanMesh(verts, tri_faces);

   while(num_hlp_portals--){
      CPI3D_visual vis = *hlp_portals++;
      const I3D_bound_volume &bvol = vis->GetBoundVolume();
      if(bvol.bbox.IsValid()){
         S_vector center = ((bvol.bbox.min+bvol.bbox.max) * .5f) * vis->GetMatrix();
         float closest_dist = 1e+16f;
         PI3D_portal closest_prt = NULL;
                              //find closest portal
         for(dword i=own_portals.size(); i--; ){
            PI3D_portal prt = own_portals[i];
            const C_vector<S_vector> &vpool = prt->GetVertexPool();
            if(vpool.size()){
               S_vector p_center(0, 0, 0);
               for(dword vi=vpool.size(); vi--; )
                  p_center += vpool[vi];
               p_center /= (float)vpool.size();
               float dist_2 = (p_center-center).Square();
               if(closest_dist>dist_2){
                  closest_dist = dist_2;
                  closest_prt = prt;
               }
            }
         }
         if(closest_prt){
            closest_prt->SetOneWay(PRTF_ONEWAY_OUT);
         }
      }
   }

   bool b = qhCreateConvexHull(verts, NULL, &faces);
   if(!b){
      vertices.clear();
      tri_faces.clear();
      bbox.Invalidate();
      bound_planes.clear();
      ld.REPORT_ERR(C_fstr("Sector '%s': failed to construct convex hull", (const char*)GetName()));
      return I3DERR_INVALIDPARAMS;
   }
   /*
                              //detect degenerate faces
   for(i=faces.size(); i--; ){
      const I3D_face &fc = faces[i];
      const S_vector &p0 = verts[fc[0]], &p1 = verts[fc[1]], &p2 = verts[fc[2]];
      if(p0.DistanceToLine(p1, p2-p1) < .001f){
         faces[i] = faces.back(); faces.pop_back();
      }
   }
   */

   CleanMesh(verts, faces);

                              //save sector's mesh
   vertices.assign(&verts.front(), (&verts.back())+1);
                              //build skeleton - boundbox and planes
   BuildBounds();

//#if defined _DEBUG && 1
#if 1
   {
      bool ok = true;
                              //sanity check
      for(dword pi=bound_planes.size(); pi--; ){
         const S_plane &pl = bound_planes[pi];
                              //check if all vertices of portal are on opposite side of the bounding plane
         for(dword vi=vertices.size(); vi--; ){
            const S_vector &v = vertices[vi];

            float d = v.DistanceToPlane(pl);
            if(d > 0.01f){
                              //failed - some vertex lies on wrong side
               const I3D_face &fc = faces[pi];
               S_vector sum(0, 0, 0);
                              //paint the bad face (white)
               for(dword fi=fc.num_points; fi--; ){
                  drv->DebugLine(verts[fc[fi]], verts[fc[(fi+1)%fc.num_points]], 0);
                  sum += verts[fc[fi]];
               }
                              //paint its normal (green)
               sum /= (float)fc.num_points;
               drv->DebugLine(sum, sum+pl.normal, 0, 0xff00ff00);
                              //paint the wrong point (blue)
               drv->DebugLine(sum, v, 0, 0xff0000ff);
               ok = false;
               break;
            }
         }
      }
      if(!ok){
         ld.REPORT_ERR(C_fstr("Sector '%s': computed hull has invalid face(s)", (const char*)GetName()));
         return I3DERR_INVALIDPARAMS;
      }

      {
         const float THRESH = .001f;
         const PI3D_portal *prts = GetPortals();
         for(int pi=NumPortals(); pi--; ){
            CPI3D_portal prt = *prts++;

                              //check if all points of portal are on edge of sector
            C_vector<bool> plane_ok; plane_ok.resize(bound_planes.size(), true);
            int num_pl_ok = bound_planes.size();

            dword numv = prt->NumVertices();
            const S_vector *verts = prt->GetVertices();
            for(int vi=numv; vi--; ){
               const S_vector &v = verts[vi];
                              //check if there's such plane on which we lie
               for(dword pli=bound_planes.size(); pli--; ){
                  if(!plane_ok[pli])
                     continue;
                  float d = v.DistanceToPlane(bound_planes[pli]);
                  if(I3DFabs(d) > THRESH){
                     plane_ok[pli] = false;
                     --num_pl_ok;
                  }
               }
            }
            if(!num_pl_ok){
               ld.REPORT_ERR(C_fstr("Sector '%s': sink portal", (const char*)GetName()));
               for(vi=numv; vi--; )
                  drv->DebugLine(verts[vi], verts[(vi+1)%numv], 0, 0xffff0000);
               ok = false;
            }
         }
      }
      if(!ok)
         return I3DERR_INVALIDPARAMS;
      /*
                              //draw all faces
      for(i=faces.size(); i--; ){
         const I3D_face &fc = faces[i];
         for(dword fi=fc.num_points; fi--; ){
            drv->DebugLine(verts[fc[fi]], verts[fc[(fi+1)%fc.num_points]], 0);
         }
      }
      */
   }
#endif

                              //connect to scene
   return I3D_OK;
}

//----------------------------

void I3D_sector::BuildBounds(){

                              //create bounding box
   dword i;
                              //make bound box
   bbox.Invalidate();
   for(i=vertices.size(); i--; ){
      bbox.min.Minimal(vertices[i]);
      bbox.max.Maximal(vertices[i]);
   }

   const S_vector *verts = vertices.begin();

                              //build bound planes - keep only unique ones
   bound_planes.reserve(faces.size());
   for(i=0; i < faces.size(); i++){
      const I3D_face &fc = faces[i];
      S_plane pl;

                              //get normal of plane
      if(fc.num_points == 3){

                              //detect invalid faces
                                 //thresh = minimal distance each vertex in triangle must have from adjacent edge
         const float thresh = 1e-3f;

         float d = verts[fc[0]].DistanceToLine(verts[fc[1]], verts[fc[2]] - verts[fc[1]]);
         if(d >= thresh){
            float d1 = verts[fc[1]].DistanceToLine(verts[fc[2]], verts[fc[0]] - verts[fc[2]]);
            d = Min(d, d1);
         }
         if(d >= thresh){
            float d1 = verts[fc[2]].DistanceToLine(verts[fc[0]], verts[fc[1]] - verts[fc[0]]);
            d = Min(d, d1);
         }
         float valid = (d >= thresh);
         if(valid){
                                 //check near-degenerate triangles
            float f0 = (verts[fc[0]] - verts[fc[1]]).Square();
            float f1 = (verts[fc[1]] - verts[fc[2]]).Square();
            float f2 = (verts[fc[2]] - verts[fc[0]]).Square();
            float max_side_2 = Max(f0, Max(f1, f2));
            float max_side = I3DSqrt(max_side_2);
            float ratio = d / max_side;
            valid = (ratio > .005f);
         }
         if(!valid){
            faces[i] = faces.back(); faces.pop_back();
            --i;
            continue;
         }
         pl = S_plane(verts[fc[0]], verts[fc[1]], verts[fc[2]]);
      }else{
                              //face with more than 3 points, we must be careful
                              // to choose best points for plane computation

                              //find 2 points with maximal distance
         int p0 = -1, p1 = -1, p2 = -1;
         float best_dist = 0.0f;
         for(int ii=fc.num_points; ii--; ){
            for(int jj=ii; jj--; ){
               S_vector dir = verts[fc[ii]] - verts[fc[jj]];
               float d = dir.Dot(dir);
               if(best_dist<d){
                  best_dist = d;
                  p0 = ii;
                  p1 = jj;
               }
            }
         }
                              //now find 3rd point with maximal distance to their line
         bool invert = false;
         assert(p1!=-1);
         S_vector dir = verts[fc[p1]] - verts[fc[p0]];
         best_dist = -1.0f;
         bool curr_invert = true;
         for(ii = (p0+1)%fc.num_points; ii!=p0; ++ii %= fc.num_points){
            if(ii==p1)
               curr_invert = false;
            else{
               float dtl = verts[fc[ii]].DistanceToLine(verts[fc[p0]], dir);
               if(best_dist<dtl){
                  best_dist = dtl;
                  invert = curr_invert;
                  p2 = ii;
               }
            }
         }
         assert(p2!=-1);
         pl.normal.GetNormal(verts[fc[p0]], verts[fc[p1]], verts[fc[p2]]);
         pl.normal.Normalize();
         if(invert)
            pl.normal.Invert();
         pl.d = -verts[fc[0]].Dot(pl.normal);
      }
      if(I3DFabs(1.0f - pl.normal.Magnitude()) > 1e-4f){
                              //invalid
         faces[i] = faces.back(); faces.pop_back();
         --i;
         continue;
      }
      bound_planes.push_back(pl);
   }
}

//----------------------------

bool I3D_sector::SaveCachedInfo(C_cache *ck) const{

                              //write version
   word version = DBASE_VERSION;
   ck->write(&version, sizeof(version));

   {                          //write simple flag
      bool simple = (sct_flags&SCTF_SIMPLE);
      ck->write(&simple, sizeof(byte));
   }
                              //write vertices
   {
      dword num = vertices.size();
      ck->write(&num, sizeof(word));
      ck->write(vertices.begin(), sizeof(S_vector)*num);
   }
                              //write faces
   {
      dword num = faces.size();
      ck->write(&num, sizeof(word));
      for(dword i=0; i<faces.size(); i++){
         const I3D_face &fc = faces[i];
         ck->write(&fc.num_points, sizeof(byte));
         ck->write(fc.points, fc.num_points*sizeof(word));
      }
   }
                              //write portals
   {
      dword num = own_portals.size();
      ck->write(&num, sizeof(word));
      while(num--){
         CPI3D_portal prt = own_portals[num];
         dword numv = prt->NumVertices();
         ck->write(&numv, sizeof(word));
         ck->write(prt->GetVertices(), sizeof(S_vector)*numv);
         dword flags = prt->GetFlags()&(PRTF_ONEWAY_IN|PRTF_ONEWAY_OUT);
         ck->write(&flags, sizeof(word));
      }
   }
   return true;
}

//----------------------------

bool I3D_sector::LoadCachedInfo(C_cache *ck){

#ifdef DEBUG_NO_CACHE
   return false;
#endif
                              //read and check version
   word version = 0;
   ck->read(&version, sizeof(version));
   if(version!=DBASE_VERSION) return false;
   
   {                          //read simple flag
      //bool simple = false;
      //ck->read(&simple, sizeof(byte));
      bool simple = ck->ReadByte();
      sct_flags &= ~SCTF_SIMPLE;
      if(simple) sct_flags |= SCTF_SIMPLE;
   }

   {                          //read vertices
      word num = 0;
      ck->read(&num, sizeof(num));
      vertices.assign(num);
      ck->read(vertices.begin(), sizeof(S_vector)*num);
   }
                              //read faces
   {
      word num = 0;
      ck->read(&num, sizeof(num));
      faces.resize(num);
      for(int i=0; i<num; i++){
         I3D_face &fc = faces[i];
         //byte num_points;
         //ck->read(&num_points, sizeof(byte));
         byte num_points = ck->ReadByte();
         fc.Reserve(num_points);
         ck->read(fc.points, fc.num_points*sizeof(word));
      }
   }
   {
      word num = 0;
      ck->read(&num, sizeof(num));
      while(num--){
         word numv = 0;
         ck->read(&numv, sizeof(numv));
         C_vector<S_vector> v; v.resize(numv);
         ck->read(&v.front(), sizeof(S_vector)*numv);
         word flags = 0;
         ck->read(&flags, sizeof(word));

         PI3D_portal prt = new I3D_portal(this, flags);
         if(prt->Build(v))
            own_portals.push_back(prt);
         prt->Release();
      }
   }
                              //build skeleton - boundbox and planes
   BuildBounds();
   return true;
}

//----------------------------

bool I3D_sector::Connect(PI3D_scene scene){

   if(IsSimple())
      return false;

                              //find parent sector
   PI3D_frame prnt = this;
   while((prnt=prnt->GetParent1(), prnt) && prnt->GetType1()!=FRAME_SECTOR);
   PI3D_sector prnt_sect = I3DCAST_SECTOR(prnt);
                              //if no parent found, use scene's primary
   if(!prnt_sect) prnt_sect = scene->GetPrimarySector1();
                              //process all portals
   for(int i=own_portals.size(); i--; ){
      PI3D_portal prt = own_portals[i];
                              //already connected?
      if(prt->GetConnectedSector())   
         continue;
                              //look for inverse portal
      CPI3D_sector *sp = scene->GetSectors();
      for(int j=scene->NumSectors(); j--; ){
         PI3D_sector sct = (PI3D_sector)sp[j];
                              //don't test with itself
         if(sct==this) continue;
         const PI3D_portal *pp1 = sct->GetPortals();
         for(int k=sct->NumPortals(); k--; ){
            PI3D_portal prt1 = pp1[k];
            if(prt->Equals(prt1)){

               if(prt->GetConnectedSector())
                  prt->GetConnectedSector()->DelConnectedPortal(prt);
               if(prt1->GetConnectedSector())
                  prt1->GetConnectedSector()->DelConnectedPortal(prt1);

                              //connect portals together
               prt->ConnectTo(prt1);
               prt1->ConnectTo(prt);

               AddConnectedPortal(prt1);
               sct->AddConnectedPortal(prt);
               break;
            }
         }
         if(k!=-1)
            break;
      }
      if(j==-1){
                              //no sector found, connect with parent
         prnt_sect->AddConnectedPortal(prt);
      }
   }

   return true;
}

//----------------------------

void I3D_sector::Disconnect(PI3D_scene scene){

                              //clear connected portals
   for(int i=connected_portals.size(); i--; ){
      PI3D_portal prt = connected_portals[i];
      prt->SetConnectedSector(NULL);
      prt->ConnectTo(NULL);
   }
   connected_portals.clear();
                              //disconnect our portals
   for(i=own_portals.size(); i--; ){
      PI3D_portal prt = own_portals[i];
      prt->ConnectTo(NULL);
      if(prt->GetConnectedSector()){
         PI3D_sector sct = prt->GetConnectedSector();
         sct->DelConnectedPortal(prt);
         if(scene) sct->Connect(scene);
      }
   }
}

//----------------------------

void I3D_sector::AddConnectedPortal(PI3D_portal prt){

#ifdef _DEBUG
   int i = FindPointerInArray((void**)(connected_portals.size() ? &connected_portals.front() : NULL), connected_portals.size(), prt);
   assert(i == -1);
#endif
   connected_portals.push_back(prt);
   prt->SetConnectedSector(this);
}

//----------------------------

void I3D_sector::DelConnectedPortal(PI3D_portal prt){

   int i = FindPointerInArray((void**)&connected_portals.front(), connected_portals.size(), prt);
   assert(i!=-1);
   prt->SetConnectedSector(NULL);
   connected_portals[i] = connected_portals.back(); connected_portals.pop_back();
}

//----------------------------

void I3D_sector::PreparePortals(const S_view_frustum &vf){

   assert(!(sct_flags&SCTF_PREPARED));
                              //prepare primary enabled portals
   for(int i=own_portals.size(); i--; ){
      PI3D_portal prt = own_portals[i];
      prt->SetState(PS_OUT_OF_VIEW);
      if(!prt->IsEnabled() || !prt->IsActive())
         continue;
                              //prepare its frustum
      bool in_front = prt->PrepareFrustum(vf.view_pos);
                              //check if portal is inside of view frustum
      const C_vector<S_vector> &prt_verts = prt->GetVertexPool();
      //bool in = CheckFrustumIntersection(vf.clip_planes, 4, prt_verts.front(), prt_verts.size());
      bool in = CheckFrustumIntersection(vf.clip_planes, 4, vf.frustum_pts,
         &prt_verts.front(), prt_verts.size(), vf.view_pos, false, !in_front);
      if(!in)
         continue;
      if(in_front){
         if(!(prt->GetFlags()&PRTF_ONEWAY_OUT))
            prt->SetState(PS_FACED);
      }else{
         if(!(prt->GetFlags()&PRTF_ONEWAY_IN))
            prt->SetState(PS_NOT_FACED);
      }
   }
   sct_flags |= SCTF_PREPARED;
}

//----------------------------

bool I3D_sector::CheckPortalCollision(CPI3D_portal prt1) const{

   const C_vector<S_vector> &prt_verts = prt1->GetVertexPool();
   bool inv_cull = (prt1->GetState()==PS_NOT_FACED);

   for(int phase=0; phase<2; phase++){
      const C_vector<C_smart_ptr<I3D_portal> > &v_portals = (!phase ? own_portals : connected_portals);
      PORTAL_STATE ps = (!phase ? PS_FACED : PS_NOT_FACED);
      if(v_portals.size()){
         CPI3D_portal *prts = (CPI3D_portal*)&v_portals.front();
         for(int pi=v_portals.size(); pi--; ){
            CPI3D_portal prt = *prts++;
            if(prt->GetState()==ps && prt->IsEnabled() && prt!=prt1 && prt->IsActive()){
               {

                  const S_view_frustum &prt_vf = prt->GetFrustum();
                  //if(CheckFrustumIntersection(prt_vf.clip_planes, prt_vf.num_clip_planes, prt_verts.begin(), prt_verts.size())){
                  if(CheckFrustumIntersection(prt_vf.clip_planes, prt_vf.num_clip_planes, prt_vf.frustum_pts,
                     &prt_verts.front(), prt_verts.size(), prt_vf.view_pos, false, inv_cull)){
               
                     return true;
                  }
               }
            }
         }
      }
   }
   return false;
}

//----------------------------

struct S_set_vis_link{
   S_set_vis_link *prev;      //previous link, NULL for dest sector
   PI3D_sector sct;           //sector of the link
};

//----------------------------

void I3D_sector::SetVisibleRec(const S_view_frustum &vf, S_set_vis_link *prev_link){

   assert(!IsSimple());

   sct_flags |= SCTF_VISIBLE | SCTF_DRAW_TRIGGER;

                              //transform our portals
   if(!(sct_flags&SCTF_PREPARED))
      PreparePortals(vf);
                              //transform all sectors of our connected portals
   for(int pi=connected_portals.size(); pi--; ){
      CPI3D_portal prt = connected_portals[pi];
      if(!prt->IsActive() || !prt->IsEnabled())
         continue;
      PI3D_sector prt_sct = prt->GetOwningSector();
      if(!(prt_sct->sct_flags&SCTF_PREPARED))
         prt_sct->PreparePortals(vf);
   }

   S_set_vis_link link;
   link.prev = prev_link;
   link.sct = this;
                              //set all seen sectors visible
   for(int phase=2; phase--; ){
      const PORTAL_STATE ps = !phase ? PS_NOT_FACED : PS_FACED;
      const C_vector<C_smart_ptr<I3D_portal> > &p_list = !phase ? own_portals : connected_portals;
      const PI3D_portal *prts = !p_list.size() ? NULL : (PI3D_portal*)&p_list.front();
      for(int pi=p_list.size(); pi--; ){
         PI3D_portal prt = *prts++;
                              //only one portal of two connected is actively processed
         if(!prt->IsActive()) continue;
                              //check if it's seen into
         if(prt->GetState() != ps) continue;

         PI3D_sector next_sect = !phase ? prt->GetConnectedSector() : prt->GetOwningSector();
                              //check if not visible yet
         //if(!next_sect->IsVisible()) //!!!we may be checking other portal paths
         {
                              //check if portal seen through all linked sectors
                              // back to camera sector
            const S_set_vis_link *link_up = &link;
            while(link_up->prev){
               if(!link_up->sct->CheckPortalCollision(prt))
                  break;
               link_up = link_up->prev;
            }
                              //if we came back to camera sector, the portal is visible,
                              // and also the connected sector is
            if(!link_up->prev){
                              //recursively do the same on this
               //if(!next_sect->IsVisible())   //!!!we may be checking other portal paths
                  next_sect->SetVisibleRec(vf, &link);
            }else{
                              //this portal is not visible from camera,
                              // don't consider it further
               //prt->SetState(PS_OUT_OF_VIEW); //!!! may be visible from other path
            }
         }
      }
   }
}

//----------------------------
#define SCTF_RECURSE1 0x80000000

void I3D_sector::DisablePortalsRec(PI3D_sector dest_sect){

   sct_flags |= SCTF_RECURSE1;

   for(int phase=2; phase--; ){
      C_vector<C_smart_ptr<I3D_portal> > &p_list = !phase ? own_portals : connected_portals;
      for(int pi=p_list.size(); pi--; ){
         PI3D_portal prt = p_list[pi];
                              //only one portal of two connected is actively processed
         if(!prt->IsActive())
            continue;

         PI3D_sector cons = !phase ? prt->GetConnectedSector() : prt->GetOwningSector();
                              //check if it's seen into
         if(prt->GetState() != PS_OUT_OF_VIEW){
            if(cons!=dest_sect && this!=dest_sect){
               const C_vector<S_vector> &vpool = prt->GetVertexPool();
               if(!cons->CheckIntersectionRec(&vpool.front(), vpool.size(), dest_sect) &&
                  !CheckIntersectionRec(&vpool.front(), vpool.size(), dest_sect)){
                  prt->SetState(PS_OUT_OF_VIEW);
               }
            }
         }
         if((cons->sct_flags&SCTF_PREPARED) && !(cons->sct_flags&SCTF_RECURSE1))
            cons->DisablePortalsRec(dest_sect);
      }
   }
   sct_flags &= ~SCTF_RECURSE1;
}

//----------------------------

void I3D_sector::SetVisible(const S_view_frustum &vf){

                              //recursively enable all sectors
   SetVisibleRec(vf, NULL);
}

//----------------------------

void I3D_sector::SolveOcclusion(const C_vector<class I3D_occluder*> &occ_list){

   sct_flags |= SCTF_RECURSE;
                              //for all connected visible sectors, solve visibility
   for(int phase=2; phase--; ){
      const PORTAL_STATE ps = !phase ? PS_NOT_FACED : PS_FACED;
      const C_vector<C_smart_ptr<I3D_portal> > &p_list = !phase ? own_portals : connected_portals;
      const PI3D_portal *prts = (PI3D_portal*)&p_list.front();
      for(int pi=p_list.size(); pi--; ){
         PI3D_portal prt = *prts++;
                              //only one portal of two connected is actively processed
         if(!prt->IsActive())
            continue;
                              //check if it's seen into
         if(prt->GetState() != ps)
            continue;
         PI3D_sector next_sect = !phase ? prt->GetConnectedSector() : prt->GetOwningSector();
         if(next_sect->IsVisible() && !(next_sect->sct_flags&SCTF_RECURSE)){
            next_sect->SolveOcclusion(occ_list);

            for(int oi=occ_list.size(); oi--; ){
               CPI3D_occluder occ = occ_list[oi];
                              //occluder works only in its sector
               if(occ->GetWorkSector() != this)
                  continue;
               if(occ->GetOccluderType1()!=I3DOCCLUDER_MESH)
                  continue;
               const S_view_frustum_base &of = occ->GetViewFrustum();
               const C_vector<S_vector> &pverts = prt->GetVertexPool();

               bool b = AreAllVerticesInVF(of, &pverts.front(), pverts.size());
               if(b){
                              //portal successfully hidden
                  prt->SetState(PS_OUT_OF_VIEW);
               }
            }
         }
      }
   }
   sct_flags &= ~SCTF_RECURSE;
}

//----------------------------

void I3D_sector::ResetRecursive(){

   assert(sct_flags&(SCTF_VISIBLE|SCTF_PREPARED));
   sct_flags &= ~(SCTF_VISIBLE | SCTF_PREPARED);
                              //reset all our linked visible sectors
   int i;
   for(i=own_portals.size(); i--; ){
      PI3D_portal prt = own_portals[i];
      PI3D_sector con_sct = prt->GetConnectedSector();
      assert(con_sct && con_sct!=this);
      con_sct->sct_flags &= ~SCTF_PREPARED;
      //if(con_sct->IsVisible())
      if(con_sct->sct_flags&(SCTF_VISIBLE|SCTF_PREPARED))
         con_sct->ResetRecursive();
   }
   for(i=connected_portals.size(); i--; ){
      CPI3D_portal prt = connected_portals[i];
      PI3D_sector con_sct = prt->GetOwningSector();
      assert(con_sct && con_sct!=this);
      con_sct->sct_flags &= ~SCTF_PREPARED;
      //if(con_sct->IsVisible())
      if(con_sct->sct_flags&(SCTF_VISIBLE|SCTF_PREPARED))
         con_sct->ResetRecursive();
   }
}

//----------------------------

bool I3D_sector::IsBoundSphereVisible(const I3D_bsphere &bs_world, CPI3D_sector dest_sct, const S_preprocess_context &pc, bool *clip_vf, bool *clip_occ) const{

                              //bsphere check against view frustum
   if(clip_vf){
                              //perform bounding-sphere test
      bool in;
#ifdef USE_VF_BSPHERE
      in = SphereCollide(pc.vf_sphere, bs_world);
      if(!in)
         return false;
#endif
#if defined _DEBUG && 0
      in = SphereInFrustum(pc.exp_frustum, bs_world, *clip_vf);
      //assert(in==SphereInVF(pc.view_frustum, bs_world, *clip_vf));
#else
      in = SphereInVF(pc.view_frustum, bs_world, *clip_vf);
#endif
      if(!in)
         return false;
   }

                              //bsphere check against occluders
   if(clip_occ){
      *clip_occ = false;
      CPI3D_occluder *occs = (CPI3D_occluder*)&pc.occ_list.front();
      for(int oi=pc.occ_list.size(); oi--; ){
         CPI3D_occluder occ = *occs++;
                              //occluder works only in its sector
         if(occ->GetWorkSector() != this) continue;

         bool clip;
         bool occluded = occ->IsOccluding(bs_world, clip);
         if(occluded)
            return false;
         (*(byte*)clip_occ) |= (byte)clip;
      }
   }

                              //bsphere check against portals
   if(dest_sct!=this && !IsSimple()){
      bool in = CheckIntersectionRec(bs_world, dest_sct);
      if(!in)
         return false;
   }
   return true;
}

//----------------------------

static void Reverse(S_vector *pts, dword num){
   for(dword i=num/2; i--; ){
      Swap(pts[i], pts[num-1-i]);
   }
}

//----------------------------

bool I3D_sector::IsBoundVolumeVisible(const C_bound_volume &bvol, const S_matrix &tm, CPI3D_sector dest_sct, const S_preprocess_context &pc, bool *clip_vf, bool *clip_occ) const{

   if(!IsBoundSphereVisible(bvol.bsphere_world, dest_sct, pc, clip_vf, clip_occ))
      return false;

#ifdef ENABLE_CONTOUR_TEST
                              //bounding sphere tests failed to give clear results
                              // check if it's worth to expand bounding box
   if(!(clip_vf && *clip_vf) &&
      !(clip_occ && *clip_occ) &&
      (dest_sct==this)){
                              //no way to get out of this, we're visible
      return true;
   }

                              //expand contour of bbox as seen from camera position
   S_vector contour_points[6];
   dword num_cpts;

   ComputeContour(bvol.bound_local.bbox, tm, pc.view_frustum.view_pos, contour_points, num_cpts);
   if(!num_cpts)
      return true;

#ifdef DEBUG_PAINT_BBOX_CONTOUR
   {
      pc->render_list.scene->SetRenderMatrix(I3D_identity_matrix);
      pc->render_list.scene->DebugDrawContour(contour_points, num_cpts, S_vector(1, 0, 0));
   }
#endif

   if(drv->GetDefaultCullMode()==D3DCULL_CW){
      Reverse(contour_points, num_cpts);
   }
                              //contour check against view frustum
                              // only if requested, and bsphere was clipped
   if(clip_vf && *clip_vf){
      //bool in = CheckFrustumIntersection(pc.view_frustum.clip_planes, 4, contour_points, num_cpts);
      bool in = CheckFrustumIntersection(pc.view_frustum.clip_planes, 4, pc.view_frustum.frustum_pts,
         contour_points, num_cpts, pc.view_frustum.view_pos);
      if(!in)
         return false;

                              //experimental code:
#if 0
      {
         S_vector bbox_full[8], bbox_full_t[8];
         bvol.bound_local.bbox.Expand(bbox_full);
         TransformVertexArray(bbox_full, sizeof(S_vector), 8, bbox_full_t, sizeof(S_vector), tm);
         for(int pi=2; pi--; ){
            const S_plane &pl = pc.view_frustum.clip_planes[4+pi];
            for(int vi = 8; vi--; ){
               float d = bbox_full_t[vi].DistanceToPlane(pl);
               if(d < 0.0f)
                  break;
            }
            if(vi==-1)
               return false;
         }
      }
#endif
#if 0
      if(pc.mode==RV_MIRROR){
                              //special check - expanded bbox against mirror plane
         S_vector bbox_full[8], bbox_full_t[8];
         bvol.bound_local.bbox.Expand(bbox_full);
         TransformVertexArray(bbox_full, sizeof(S_vector), 8, bbox_full_t, sizeof(S_vector), tm);
         const S_plane &m_pl = pc.view_frustum.clip_planes[6];
         for(int vi = 8; vi--; ){
            float d = bbox_full_t[vi].DistanceToPlane(m_pl);
            if(d < -0.01f)
               break;
         }
         if(vi==-1)
            return false;
      }
#endif
   }

                              //contour check against occluders
   if(clip_occ && *clip_occ){
      CPI3D_occluder *occs = (CPI3D_occluder*)&pc.occ_list.front();
      for(int oi=pc.occ_list.size(); oi--; ){
         CPI3D_occluder occ = *occs++;

                              //occluder works only in its sector
         if(occ->GetWorkSector() != this) continue;

         if(occ->GetOccluderType1() != I3DOCCLUDER_MESH)
            continue;

         const S_view_frustum_base &vf = occ->GetViewFrustum();
         bool in = AreAllVerticesInVF(vf, contour_points, num_cpts);
         if(in)
            return false;
      }
   }

                              //contour check against portals
   if(dest_sct!=this && !IsSimple()){
      bool in = CheckIntersectionRec(contour_points, num_cpts, dest_sct);
      if(!in)
         return false;
   }
#endif//ENABLE_CONTOUR_TEST

   return true;
}

//----------------------------

#ifdef ENABLE_CONTOUR_TEST

bool I3D_sector::CheckIntersectionRec(const S_vector *verts, dword num_verts, CPI3D_sector dest_sct) const{

   sct_flags |= SCTF_RECURSE;

   for(int phase=2; phase--; ){
      const C_vector<C_smart_ptr<I3D_portal> > &v_portals =
         (!phase ? own_portals : connected_portals);
      PORTAL_STATE ps = (!phase ? PS_FACED : PS_NOT_FACED);
      if(v_portals.size()){
         CPI3D_portal *prts = (CPI3D_portal*)&v_portals.front();
         for(int pi=v_portals.size(); pi--; ){
            CPI3D_portal prt = *prts++;
            if(!prt->IsActive())
               continue;
            if(prt->GetState() != ps)
               continue;
            /*
            CPI3D_sector connect_sct;
            if(!(prt->GetOwningSector()->sct_flags&SCTF_RECURSE))
               connect_sct = prt->GetOwningSector();
            else
               connect_sct = prt->GetConnectedSector();
            assert(connect_sct && connect_sct!=this);
            */
            CPI3D_sector connect_sct = phase ? prt->GetOwningSector() : prt->GetConnectedSector();
            assert(connect_sct && connect_sct!=this);
            if(!(connect_sct->sct_flags&SCTF_VISIBLE) || (connect_sct->sct_flags&SCTF_RECURSE))
               continue;

                                 //check intersection on portal
            const S_view_frustum &prt_vf = prt->GetFrustum();
            if(CheckFrustumIntersection(prt_vf.clip_planes, prt_vf.num_clip_planes, &prt->GetVertexPool().front(),
               verts, num_verts, prt_vf.view_pos)){
                                 //check if we came to dest sector
               if(connect_sct==dest_sct){
                  sct_flags &= ~SCTF_RECURSE;
                  return true;
               }
                                 //recursively check on this sector
               if(connect_sct->CheckIntersectionRec(verts, num_verts, dest_sct)){
                  sct_flags &= ~SCTF_RECURSE;
                  return true;
               }
            }
         }
      }
   }
   sct_flags &= ~SCTF_RECURSE;
   return false;
}
#endif//ENABLE_CONTOUR_TEST

//----------------------------

bool I3D_sector::CheckIntersectionRec(const I3D_bsphere &bsphere, CPI3D_sector dest_sct) const{

   sct_flags |= SCTF_RECURSE;

   for(int phase=2; phase--; ){
      const C_vector<C_smart_ptr<I3D_portal> > &v_portals =
         (!phase ? own_portals : connected_portals);
      PORTAL_STATE ps = (!phase ? PS_FACED : PS_NOT_FACED);
      if(v_portals.size()){
         CPI3D_portal *prts = (CPI3D_portal*)&v_portals.front();
         for(int pi=v_portals.size(); pi--; ){
            CPI3D_portal prt = *prts++;
            if(!prt->IsActive())
               continue;
            if(prt->GetState() != ps)
               continue;
            /*
            CPI3D_sector connect_sct;
            if(!(prt->GetOwningSector()->sct_flags&SCTF_RECURSE))
               connect_sct = prt->GetOwningSector();
            else
               connect_sct = prt->GetConnectedSector();
            assert(connect_sct && connect_sct!=this);
            */
            CPI3D_sector connect_sct = phase ? prt->GetOwningSector() : prt->GetConnectedSector();
            assert(connect_sct && connect_sct!=this);
            if(!(connect_sct->sct_flags&SCTF_VISIBLE) || (connect_sct->sct_flags&SCTF_RECURSE))
               continue;

                                 //check intersection on portal
            const S_view_frustum &prt_vf = prt->GetFrustum();
            bool clip;
            bool in = SphereInVF(prt_vf, bsphere, clip);
            if(in){
                                 //check if we came to dest sector
               if(connect_sct==dest_sct){
                  sct_flags &= ~SCTF_RECURSE;
                  return true;
               }
                                 //recursively check on this sector
               if(connect_sct->CheckIntersectionRec(bsphere, dest_sct)){
                  sct_flags &= ~SCTF_RECURSE;
                  return true;
               }
            }
         }
      }
   }
   sct_flags &= ~SCTF_RECURSE;
   return false;
}

//----------------------------

bool I3D_sector::CheckPoint(const S_vector &v) const{
                              //check if point is in this sector
                              //always true for primary sector
   if(IsPrimary1())
      return true;
   if(IsSimple())
      return false;
   if(!IsOn1())
      return false;

                              //put point to local coords
   S_vector v_loc = v * GetInvMatrix1();

                              //bounding-box test first
   int i;
   const S_vector &min = bbox.min;
   const S_vector &max = bbox.max;
   if(v_loc.x<min.x || v_loc.x>=max.x || v_loc.y<min.y || v_loc.y>=max.y || v_loc.z<min.z || v_loc.z>=max.z)
      return false;
                              //detailed face check
                              //sectors are convex, so we only check,
                              //if our vertex lies in front of all faces
   for(i=bound_planes.size(); i--; ){
      float d = v_loc.DistanceToPlane(bound_planes[i]);
      if(CHECK_ZERO_GREATER(d))
         return false;
   }
   return true;
}

//----------------------------

I3D_RESULT I3D_sector::AddLight(PI3D_light lp){

   C_vector<PI3D_light> &l = !lp->IsFogType() ? light_list : fog_light_list;
   int i = FindPointerInArray((void**)(l.size() ? &l.front() : NULL), l.size(), lp);
   if(i!=-1)
      return I3DERR_INVALIDPARAMS;

   l.push_back(lp);
   lp->light_sectors.push_back(this);
   if(!lp->IsFogType())
      sct_flags |= SCTF_LIGHT_LIST_RESET;

   switch(lp->GetLightType()){
   case I3DLIGHT_FOG:
      fog_light = lp;
      break;
   case I3DLIGHT_LAYEREDFOG:
      if(!fog_light)
         fog_light = lp;
      break;
   }
   lp->MarkDirty();
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_sector::RemoveLight(PI3D_light lp){

   C_vector<PI3D_light> &l = !lp->IsFogType() ? light_list : fog_light_list;
   int i = FindPointerInArray((void**)&l.front(), l.size(), lp);
   if(i!=-1){
      for(dword j=0; j<lp->light_sectors.size(); j++)
      if(lp->light_sectors[j] == this){
         lp->MarkDirty();
         frm_flags |= FRMFLAGS_HR_LIGHT_RESET;
         if(!lp->IsFogType())
            sct_flags |= SCTF_LIGHT_LIST_RESET;

         l[i] = l.back(); l.pop_back();
         lp->light_sectors[j] = lp->light_sectors.back(); lp->light_sectors.pop_back();
         if(lp->IsFogType()){
            if(fog_light==lp){
               fog_light = NULL;
                              //find new fog light
               for(int i=l.size(); i--; ){
                  PI3D_light lp = l[i];
                  switch(lp->GetLightType1()){
                  case I3DLIGHT_FOG:
                     fog_light = lp;
                     i = 0;
                     break;
                  case I3DLIGHT_LAYEREDFOG:
                     fog_light = lp;
                     break;
                  }
               }
            }
         }
         return I3D_OK;
      }
   }
   return I3DERR_OBJECTNOTFOUND;
}

//----------------------------

void I3D_sector::UpdateAllLights(){

   for(int j=light_list.size(); j--; )
      light_list[j]->UpdateLight();
   for(j=fog_light_list.size(); j--; )
      fog_light_list[j]->UpdateLight();

   frm_flags &= ~FRMFLAGS_HR_LIGHT_RESET;
}

//----------------------------

I3D_RESULT I3D_sector::AddSound(PI3D_sound sp){

   int i;

   i = FindPointerInArray((void**)(sound_list.size() ? &sound_list.front() : NULL), sound_list.size(), sp);
   if(i!=-1)
      return I3DERR_INVALIDPARAMS;

   sound_list.push_back(sp);
   sp->GetSectorsVector().push_back(this);

   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_sector::RemoveSound(PI3D_sound sp){

   int i = FindPointerInArray((void**)&sound_list.front(), sound_list.size(), sp);
   if(i!=-1){
      int j = FindPointerInArray((void**)&sp->GetSectorsVector().front(), sp->GetSectorsVector().size(), this);
      assert(j!=-1);

      sound_list[i] = sound_list.back(); sound_list.pop_back();
      sp->GetSectorsVector()[j] = sp->GetSectorsVector().back(); sp->GetSectorsVector().pop_back();
      return I3D_OK;
   }
   return I3DERR_OBJECTNOTFOUND;
}

//----------------------------

void I3D_sector::SetEnvironmentID(dword id){
   sound_env_id = id;
   drv->MakeEnvSettingDirty();
}

//----------------------------

void I3D_sector::Draw1(PI3D_scene scene, bool draw_skeleton, bool draw_portals) const{

   scene->SetRenderMatrix(I3DGetIdentityMatrix());
   drv->SetTexture(NULL);

   const S_vector &cpos = scene->GetActiveCamera1()->GetWorldPos() * GetInvMatrix1();

   bool zw_en = drv->IsZWriteEnabled();
   if(zw_en) drv->EnableZWrite(false);
   drv->EnableNoCull(true);

   if(draw_skeleton && vertices.size()){
                              //visible/invisible
      /*
      static const S_vector colors[] = {
         S_vector(.5f, .5f, 1.0f), S_vector(.5f, .5f, .5f)
      };
      */
      static const dword colors[] = {
         0x208080ff, 0x10808080
      };
      static const byte transps[] = { 0x20, 0x10 };
      bool cam_s = (scene->GetActiveCamera1()->GetCurrSector()==this);
      bool col_i = !(sct_flags&SCTF_DRAW_TRIGGER);
      sct_flags &= ~SCTF_DRAW_TRIGGER;

      {
                              //solid fill
         C_vector<I3D_triface> tri_faces;
         for(int i=faces.size(); i--; ){
            const I3D_face &fc = faces[i];
            I3D_triface tfc;
            tfc[0] = fc[0];
            for(dword j=1; j<fc.num_points-1; j++){
               tfc[1] = fc[j];
               tfc[2] = fc[j+1];
               tri_faces.push_back(tfc);
            }
         }

         assert(tri_faces.size());
         scene->DrawTriangles(vertices.begin(), vertices.size(), I3DVC_XYZ,
            &tri_faces[0][0], tri_faces.size() * 3, (transps[col_i]<<24) | colors[col_i]);
      }

      C_vector<I3D_edge> edges;
      C_vector<byte> e_flags;   //0 = back, 1 = contour, 2 = front
      int j;

      for(j=faces.size(); j--; ){
         const I3D_face &fc = faces[j];
         S_vector fdir = vertices[fc[0]] - cpos;
         S_vector fnormal;
         fnormal.GetNormal(vertices[fc[0]], vertices[fc[1]], vertices[fc[2]]);

         float d = fnormal.Dot(fdir);
         bool visible = (d<0.0f);
         if(cam_s) visible = !visible;

         for(int k=fc.num_points; k--; ){
            I3D_edge e(fc[k], fc[(k+1)%fc.num_points]);
            int ei = FindEdge(&edges.front(), edges.size(), I3D_edge(e[1], e[0]));
            if(ei==-1){
               edges.push_back(e);
               e_flags.push_back(visible);
            }else
               e_flags[ei] = (byte)(e_flags[ei] + visible);
         }

#ifdef DEBUG_DRAW_FACE_NORMALS                              
         {                    //draw normal of face
            S_vector sum(0, 0, 0);
            for(int k=fc.num_points; k--; ){
               sum += vertices[fc[k]];
            }
            sum /= fc.num_points;
            scene->DrawLine(sum, sum + bound_planes[j].normal * .5f,
               //(j==14 ? 0xff000000 : 0x80000000) |
               0xff000000 |
               colors[col_i]);
         }
#endif   //DEBUG_DRAW_FACE_NORMALS
      }
                              //separate edges by their visibility
      C_vector<I3D_edge> separate_edges[3];
      for(j=edges.size(); j--; )
         separate_edges[e_flags[j]].push_back(edges[j]);

      byte a = transps[col_i];
                              //back
      scene->DrawLines(vertices.begin(), vertices.size(),
         &separate_edges[0].front()[0], separate_edges[0].size()*2, (a<<24) | colors[col_i]);
                              //front
      scene->DrawLines(vertices.begin(), vertices.size(),
         &separate_edges[2].front()[0], separate_edges[2].size()*2, ((a*5/6)<<24) | colors[col_i]);
                              //contour
      scene->DrawLines(vertices.begin(), vertices.size(),
         &separate_edges[1].front()[0], separate_edges[1].size()*2, ((a*4/6)<<24) | colors[col_i]);
   }

   if(draw_portals){
      for(int i=own_portals.size(); i--; ){ 
         CPI3D_portal prt = own_portals[i];
         if(!prt->IsActive())
            continue;

         static const dword colors[] = {
            0x0000ff, 0xff00ff, 0x00ffff,
         };
         const S_vector *vp = prt->GetVertices();
                                 //fill inside
         dword numv = prt->GetVertexPool().size();
         word *poly_indx = (word*)alloca(sizeof(word)*(numv-2)*3);

         for(dword k=0; k<numv-2; k++){
            poly_indx[k*3+0] = 0;
            poly_indx[k*3+1] = word(k + 1);
            poly_indx[k*3+2] = word(k + 2);
         }
         bool connected = (prt->GetConnectedPortal()!=NULL);
         dword color = colors[!connected];
         if(prt->GetFlags()&(PRTF_ONEWAY_IN | PRTF_ONEWAY_OUT))
            color = colors[2];

         scene->DrawTriangles(vp, numv, I3DVC_XYZ, poly_indx, (numv-2)*3, 0x20000000 | color);
                              //outline
         word *line_indx = (word*)alloca(sizeof(word)*numv*2);
         for(k=0; k<numv; k++){
            line_indx[k*2] = word(k);
            line_indx[k*2+1] = word(k + 1);
         }
         line_indx[k*2-1] = 0;
         scene->DrawLines(vp, numv, line_indx, k*2, 0x80000000 | color);

         //if(!connected/* && seen*/)
         {
                              //draw normal C_vector
            S_vector mid(0, 0, 0);
            for(int i=numv; i--; ) mid += vp[i];
            mid /= (float)numv;

            scene->DrawLine(mid, mid + prt->GetPlane().normal*.25f, 0xff000000 | color);
         }
      }
   }
   if(zw_en) drv->EnableZWrite(true);
}

//----------------------------
//----------------------------
//----------------------------
