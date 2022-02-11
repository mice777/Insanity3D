/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Occluder.cpp
   Content: Occluder frame.
--------------------------------------------------------*/

#include "all.h"
#include "occluder.h"
#include "scene.h"
#include "mesh.h"
#include "camera.h"


#define DEBUG_LINE(drv, p1, p2) {\
   drv->DebugLine(p1, p2, 1);

                              //debugging:
//#define DEBUG_ENABLE_SPHERE_ONLY //enable only sphere occluding
//#define DEBUG_OUTPUT          //enable debugging lines/dumps
//#define DEBUG_EXTERNAL_CAM "cam" //use this camera
//#define DEBUG_DRAW_NEAR_PLANE //draw near occlusion plane
//#define DEBUG_DRAW_CONTOURS   //draw contours of occluder as seen from camera


//----------------------------

static const S_vector box_verts[] = {
   S_vector(-1, -1, -1),
   S_vector(1, -1, -1),
   S_vector(-1, 1, -1),
   S_vector(1, 1, -1),
   S_vector(-1, -1, 1),
   S_vector(1, -1, 1),
   S_vector(-1, 1, 1),
   S_vector(1, 1, 1),
};

//----------------------------
//----------------------------

I3D_occluder::I3D_occluder(PI3D_driver d):
   I3D_frame(d),
   occ_type(I3DOCCLUDER_SPHERE),
   work_sector(NULL),
   occ_flags(OCCF_RESET_BBOX)
{
   type = FRAME_OCCLUDER;
   enum_mask = ENUMF_OCCLUDER;
   drv->AddCount(I3D_CLID_OCCLUDER);
   
                              //build default bounds
   BuildBounds();
}

//----------------------------

I3D_occluder::~I3D_occluder(){

   drv->DecCount(I3D_CLID_OCCLUDER);
}

//----------------------------

void I3D_occluder::BuildBounds(){

   I3D_bbox &bb = bound.bound_local.bbox;
   I3D_bsphere &bs = bound.bound_local.bsphere;

   switch(occ_type){
   case I3DOCCLUDER_MESH:
      {
         bb.Invalidate();
         for(int i=vertices.size(); i--; ){
            bb.min.Minimal(vertices[i]);
            bb.max.Maximal(vertices[i]);
         }
         S_vector diag_half = (bb.max - bb.min) * .5f;
         bs.pos = bb.min + diag_half;
         bs.radius = diag_half.Magnitude();
      }
      break;

   case I3DOCCLUDER_SPHERE:
      bb.min = S_vector(-1.0f, -1.0f, -1.0f);
      bb.max = S_vector( 1.0f,  1.0f,  1.0f);
      bs.pos.Zero();
      bs.radius = 1.0f;
      break;

   default:
      assert(0);
   }
   occ_flags |= OCCF_RESET_BBOX;
}

//----------------------------

I3D_RESULT I3D_occluder::Duplicate(CPI3D_frame frm){

   if(frm==this) return I3D_OK;
   if(frm->GetType1()==type){
      CPI3D_occluder occ = I3DCAST_COCCLUDER(frm);

      occ_type = occ->occ_type;
      bound = occ->bound;
      vertices = occ->vertices;
      faces = occ->faces;
      v_trans = occ->v_trans;
      f_normals = occ->f_normals;
   }
   return I3D_frame::Duplicate(frm);
}

//----------------------------

I3D_RESULT I3D_occluder::Build(const S_vector *in_verts, dword num_verts){

   C_vector<S_vector> verts;
   verts.insert(verts.end(), in_verts, in_verts+num_verts);

   bool b = qhCreateConvexHull(verts, NULL, &faces);
   if(!b){
      vertices.clear();
      faces.clear();
      v_trans.clear();
      f_normals.clear();
      return I3DERR_GENERIC;
   }
   CleanMesh(verts, faces);

                              //save polyhedron
   vertices = verts;
   v_trans.assign(vertices.size(), S_vector());
   f_normals.assign(faces.size(), S_vector());

   BuildBounds();
   frm_flags &= ~(FRMFLAGS_OCCLUDER_TRANS_OK | FRMFLAGS_BSPHERE_TRANS_VALID);

   return I3D_OK;
}

//----------------------------

void I3D_occluder::TransformVertices(){

   if(frm_flags&FRMFLAGS_OCCLUDER_TRANS_OK)
      return;

   const S_matrix &m = GetMatrix();
   S_vector *v_t = &v_trans.front();
   TransformVertexArray(&vertices.front(), sizeof(S_vector), vertices.size(), 
      v_t, sizeof(S_vector), m);

                           //prepare faces' normals
   for(int i=faces.size(); i--; ){
      const I3D_face &fc = faces[i];
                           //compute area of face
      float area = 0.0f;
      for(int j=fc.num_points-2; j--; ){
         area += ((const I3D_triface*)&fc[j])->ComputeSurfaceArea(&vertices.front(), sizeof(S_vector));
      }

      S_vector &n = f_normals[i];
      n.GetNormal(v_t[fc[0]], v_t[fc[1]], v_t[fc[2]]);
      n.Normalize();
                           //pre-scale by area of face
      n *= area;
   }
   frm_flags |= FRMFLAGS_OCCLUDER_TRANS_OK;
}

//----------------------------

bool I3D_occluder::TestCollision(const S_vector &from, const S_vector &norm_dir, float &best_dist){

   if(occ_type!=I3DOCCLUDER_MESH)
      return false;

                              //check if some parent not hidden
   for(CPI3D_frame prnt = this; prnt=prnt->GetParent(), prnt; ){
      if(!prnt->IsOn1()){
         return false;
      }
   }

   TransformVertices();

   float fahrtest_front = -1e+16f;
   float closest_back = 1e+16f;
   for(int i=faces.size(); i--; ){
      const S_vector &v0 = v_trans[faces[i][0]];
      const S_vector &fnorm = f_normals[i];
      float f = fnorm.Dot(norm_dir);
      if(I3DFabs(f)<MRG_ZERO)
         continue;
      float d = -fnorm.Dot(v0);
      float dist_to_plane = -(fnorm.Dot(from)+d) / f;

      if(f >= 0.0f){
                              //hull is behind the ray?
         if(dist_to_plane < 0.0f)
            return false;
         closest_back = Min(dist_to_plane, closest_back);
      }else{
         fahrtest_front = Max(dist_to_plane, fahrtest_front);
      }
   }
                              //if we're in, there's no collision
   if(fahrtest_front < 0.0f)
      return false;
                              //check if we have hit the hull
   if(fahrtest_front > closest_back)
      return false;
                              //check if hit possition is closer than what we already have
   if(fahrtest_front > best_dist)
      return false;
                              //collision ok
   best_dist = fahrtest_front;
   return true;
}

//----------------------------

static void AddOrSplitEdge(I3D_edge *edges, dword &num_edges, const I3D_edge &e){
   int i = FindEdge(edges, num_edges, I3D_edge(e[1], e[0]));
   if(i==-1){
      edges[num_edges++] = e;
      return;
   }
   edges[i] = edges[--num_edges];
}

//----------------------------
#define SINGLE_Z_PLANE        //compute single plane for cam look dir, rather than all faced planes

bool I3D_occluder::ComputeMeshOccludingFrustum(PI3D_scene scene){

#ifdef DEBUG_ENABLE_SPHERE_ONLY
   if(occ_type!=I3DOCCLUDER_SPHERE)
      return false;
#endif//DEBUG_ENABLE_SPHERE_ONLY

   PI3D_camera cam = scene->GetActiveCamera1();
#ifdef DEBUG_EXTERNAL_CAM
   {
      PI3D_camera ext_cam = I3DCAST_CAMERA(scene->FindFrame(DEBUG_EXTERNAL_CAM, ENUMF_CAMERA));
      if(ext_cam)
         cam = ext_cam;
   }
#endif//DEBUG_EXTERNAL_CAM
   const S_vector &cam_pos = cam->GetMatrix()(3);

   int i;
   vf.num_clip_planes = 0;

   if(!(frm_flags&FRMFLAGS_OCCLUDER_TRANS_OK))
      TransformVertices();

   const S_vector *v_t = &v_trans.front();
   const S_vector *f_i = &f_normals.front();

                              //collect contour edges
   i = faces.size();
   for(int edge_count=0; i--; )
      edge_count += faces[i].num_points;
   I3D_edge *edges = (I3D_edge*)alloca(sizeof(I3D_edge)*edge_count);
   dword num_edges = 0;
   S_vector common_normal;
   common_normal.Zero();

   for(i = faces.size(); i--; ){
      const I3D_face &fc = faces[i];
                              //check if this face is visible from camera
      S_vector fdir = v_t[fc[0]] - cam_pos;
      float d = f_i[i].Dot(fdir);
      if(CHECK_ZERO_LESS(d)){
                              //visible,
                              // add all edges into edge list
         for(int k=fc.num_points; k--; )
            AddOrSplitEdge(edges, num_edges, I3D_edge(fc[k], fc[(k+1)%fc.num_points]));
                              //compute common normal
         common_normal += f_i[i];
      }
   }
                              //no countour? we must be inside
   if(!num_edges)
      return false;
   if(num_edges > (MAX_FRUSTUM_CLIP_PLANES-1)){
      drv->DEBUG(C_fstr("occl %s: too many edges", (const char*)name), 2);
      return false;
   }

                              //reserve 1st plane for Z
   ++vf.num_clip_planes;


#if defined DEBUG_DRAW_CONTOURS && 1
                              //debug paint contour edges
   {
      drv->SetWorldMatrix(m);
      drv->DrawLines(&vertices.front(), vertices.size(),
         (word*)edges, num_edges*2, S_vector(1, 1, 1));
   }
#endif

                              //sort all edges into contiguous array
   int nume = num_edges;
   int numv = nume;
   word *contour_points = (word*)alloca(nume*sizeof(word));
   word last_vertex = edges[--nume][0];
   contour_points[nume] = last_vertex;

   do{
      for(i = nume; i--; )
         if(edges[i][1] == last_vertex)
            break;
      if(i==-1)
         return false; //why???
      last_vertex = edges[i][0];
      edges[i] = edges[--nume];
      contour_points[nume] = last_vertex;
   }while(nume);

   I3D_edge last_e;
   for(i=0; i<numv; i++){
      I3D_edge e(contour_points[i], contour_points[(i+1)%numv]);
      const S_vector &v0 = v_t[e[0]];
      const S_vector &v1 = v_t[e[1]];

      int j = vf.num_clip_planes;
      S_plane &pl = vf.clip_planes[j];
      pl.normal.GetNormal(v0, v1, cam_pos);

      pl.normal.Normalize();
      pl.d = -v0.Dot(pl.normal);
      pl.d += SAFE_DIST;

      ++vf.num_clip_planes;
      last_e = e;
#if defined DEBUG_OUTPUT && 1
      {                       //draw optimized countour edge
         drv->SetWorldMatrix(GetIdentityMatrix());
         drv->DrawLine(v0, v1, S_vector(1, 1, 1), 0);
      }
#endif
   }
#if defined DEBUG_OUTPUT && 0
   drv->DEBUG(vf.num_clip_planes);
#endif

   assert(vf.num_clip_planes<MAX_FRUSTUM_CLIP_PLANES-1);

   {
      S_plane &pl = vf.clip_planes[0];
                              //get near plane
      pl.normal = common_normal;
      pl.normal.Normalize();
      pl.d = -1e+16f;
      int best_i;
                              //use fahrtest contour point
      for(int i=numv; i--; ){
         const S_vector &v = v_t[contour_points[i]];
         float d = -(v.Dot(pl.normal));
         if(pl.d < d){
            pl.d = d;
            best_i = i;
         }
      }

#if defined DEBUG_DRAW_NEAR_PLANE && 1
      {
         drv->SetWorldMatrix(GetIdentityMatrix());
         const S_vector &v = v_t[contour_points[best_i]];
         drv->DrawLine(v, v + common_normal * .1f, S_vector(1, 0, 0), 0);
      }
#endif
   }
   //exp_vf.Make(vf);

   return true;
}

//----------------------------

bool I3D_occluder::ComputeSphereOccludingFrustum(PI3D_scene scene){

   PI3D_camera cam = scene->GetActiveCamera1();
   const S_vector &cpos = cam->GetWorldPos();
   const S_matrix &m = GetMatrix();

   float fog_radius = m(0).Magnitude();
   S_vector cone_axis = m(3) - cpos;
   float fog_dist = cone_axis.Magnitude();
   if(fog_dist > fog_radius){
      S_normal n_cone_axis = cone_axis;
                     //compute cone
      //drv->DEBUG(C_fstr("occ distance: %f", occ_dist));
      //drv->DEBUG(C_fstr("occ radius: %f", occ_radius));
      float angle_sin = (fog_radius / fog_dist);
      float angle = (float)asin(angle_sin);
      //drv->DEBUG(C_fstr("angle: %i", (int)(angle*180.0f/PI)));

                              //compute ncp
      //DEBUG_LINE(drv, cpos, cpos + cone_axis);
                              //get angle from occluder center to the point on its sphere,
                              // which is on contour from cam's view
      float contour_angle = (PI*.5f) - angle;
      float d = (float)cos(contour_angle) * fog_radius;
      S_vector point_on_ncp = m(3) - n_cone_axis * d;
      //DEBUG_LINE(drv, m(3), point_on_ncp);

                              //save occlusion frustum
      cf.ncp.normal = -n_cone_axis;
      cf.ncp.d = point_on_ncp.Dot(n_cone_axis);
      cf.top = cpos;
      cf.cone_axis = n_cone_axis;
      cf.angle = angle;
      cf.tan_angle = (float)tan(cf.angle);
      return true;
   }

   return false;
}

//----------------------------

bool I3D_occluder::ComputeOccludingFrustum(PI3D_scene scene){

   switch(occ_type){
   case I3DOCCLUDER_MESH:
      return ComputeMeshOccludingFrustum(scene);
   case I3DOCCLUDER_SPHERE:
      return ComputeSphereOccludingFrustum(scene);
   }
   return false;
}

//----------------------------

bool I3D_occluder::IsOccluding(const I3D_bsphere &bsphere, bool &clip) const{

   switch(occ_type){

   case I3DOCCLUDER_MESH:
      {
         bool in = SphereInVF(vf, bsphere, clip);
         if(!in){
            clip = false;
            return false;
         }
      }
      return (!clip);

   case I3DOCCLUDER_SPHERE:
      {
         {
                              //check if clipped by ncp
            float d_ncp = bsphere.pos.DistanceToPlane(cf.ncp);
            //drv->DEBUG(d_ncp);
            if(d_ncp > bsphere.radius){
                              //in front of ncp, visible, non-clipped
               clip = false;
               return false;
            }
            if(d_ncp > -bsphere.radius){
                              //somewhere in ncp, visible, clipped
               clip = true;
               return false;
            }
         }

         S_vector sphere_dir = bsphere.pos - cf.top;
                              //get closest point from sphere to cone's axis
         float f = cf.cone_axis.Dot(sphere_dir);
         S_vector p = cf.top + cf.cone_axis * f;

         //DEBUG_LINE(drv, cf.top, p);
                              //get point on line lying on cone's body,
                              // which leads from cone's top and is closest to sphere
         S_vector dir = (bsphere.pos - p);
         float dir_size = dir.Magnitude();

         if(IsAbsMrgZero(dir_size)){
                              //bsphere is on axis, problem :(
            //assert(0);
            return false;     //failcase, if this happen, consider it is not occluded
         }

         f *= cf.tan_angle;

         S_vector point_on_cone = p + dir * (f / dir_size);

         //DEBUG_LINE(drv, cf.top, point_on_cone);

         S_vector cone_line_dir = point_on_cone - cf.top;
         float cone_line_dir_size_2 = cone_line_dir.Square();
                              //failback - should mever happen
         if(IsAbsMrgZero(cone_line_dir_size_2))
            return false;
                              //get closest point from sphere to this line
         float f1 = cone_line_dir.Dot(sphere_dir) / cone_line_dir_size_2;
         S_vector closest_point_on_cone = cf.top + cone_line_dir * f1;

         //DEBUG_LINE(drv, cf.top, closest_point_on_cone);

         S_vector dir_tmp = closest_point_on_cone - bsphere.pos;
         float d_sphere_to_cone_2 = dir_tmp.Square();
         //drv->DEBUG(I3DSqrt(d_sphere_to_cone_2));
         if(d_sphere_to_cone_2 < (bsphere.radius*bsphere.radius)){
            //drv->DEBUG("clip");
            clip = true;
            return false;
         }
         clip = false;
                           //now it depends on if sphere is in or out
         return (f >= dir_size);
      }
      break;
   }
   return false;
}

//----------------------------

bool I3D_occluder::IsOccluding(const C_bound_volume &bvol, const S_matrix &mat, bool &clip) const{

   switch(occ_type){
   case I3DOCCLUDER_MESH:
      {
         bool in = SphereInVF(vf, bvol.bsphere_world, clip);
         if(!in){
            clip = false;
            return false;
         }
         if(!clip)
            return true;
                              //detailed check on bounding box
         S_vector bbox_full[8];
         bvol.bound_local.bbox.Expand(bbox_full);
         S_vector vt[8];
         TransformVertexArray(bbox_full, sizeof(S_vector), 8, vt, sizeof(S_vector), mat);
         return AreAllVerticesInVF(vf, vt, 8);
      }
      break;

   case I3DOCCLUDER_SPHERE:
      return IsOccluding(bvol.bsphere_world, clip);

   default:
      assert(0);
   }
   return false;
}

//----------------------------

I3D_RESULT I3D_occluder::SetOccluderType(I3D_OCCLUDERTYPE ot){

   switch(ot){
   case I3DOCCLUDER_MESH:
   case I3DOCCLUDER_SPHERE:
      if(occ_type != (I3D_OCCLUDERTYPE)ot){
         occ_type = (I3D_OCCLUDERTYPE)ot;
         BuildBounds();
      }
      return I3D_OK;
   }
   return I3DERR_INVALIDPARAMS;
}

//----------------------------

I3D_OCCLUDERTYPE I3D_occluder::GetOccluderType() const{

   return occ_type;
}


//----------------------------

void I3D_occluder::Draw1(PI3D_scene scene, bool strong) const{

   //const S_vector &color = strong ? S_vector(0.0f, 1.0f, 0.0f) : S_vector(0.3f, .7f, 0.3f);
   const dword color = strong ? 0x00ff00 : 0x4cb04c;

   bool hr_on = true;
   {
                              //check if some parent not hidden
      for(CPI3D_frame prnt = this; prnt=prnt->GetParent(), prnt; ){
         if(!prnt->IsOn1()){
            hr_on = false;
            break;
         }
      }
   }
   dword alpha = strong ? 0x50 : hr_on ? 0x40 : 0x10;

   const S_matrix &m = GetMatrix();

   drv->SetTexture(NULL);

   bool zw_en = drv->IsZWriteEnabled();
   if(zw_en) drv->EnableZWrite(false);

   switch(occ_type){
   case I3DOCCLUDER_MESH:
      {
         scene->SetRenderMatrix(m);
         S_vector cpos;
                                    //get camera point in local coords
         {
            const S_matrix &m_inv = GetInvMatrix1();
            cpos = scene->GetActiveCamera1()->GetWorldPos() * m_inv;
         }
         {
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
                                    //solid fill
            scene->DrawTriangles(&vertices.front(), vertices.size(), I3DVC_XYZ, &tri_faces[0][0], tri_faces.size() * 3, (alpha<<24) | color);
         }

         C_vector<I3D_edge> edges;
         C_vector<byte> e_flags;   //0 = back, 1 = contour, 2 = front

         int i;
         for(i=faces.size(); i--; ){
            const I3D_face &fc = faces[i];
            S_vector fdir = vertices[fc[0]] - cpos;
            S_vector fnormal;
            fnormal.GetNormal(vertices[fc[0]], vertices[fc[1]], vertices[fc[2]]);
            float d = fnormal.Dot(fdir);
            bool visible = (d<0.0f);
            for(int j=fc.num_points; j--; ){
               I3D_edge e(fc[j], fc[(j+1)%fc.num_points]);
               int ei = FindEdge(&edges.front(), edges.size(), I3D_edge(e[1], e[0]));
               if(ei==-1){
                  edges.push_back(e);
                  e_flags.push_back(visible);
               }else
                  e_flags[ei] = (byte)(e_flags[ei] + visible);
            }
         }

                                    //separate edges by their visibility
         C_vector<I3D_edge> separate_edges[3];
         for(i=edges.size(); i--; )
            separate_edges[e_flags[i]].push_back(edges[i]);

                                    //back
         scene->DrawLines(&vertices.front(), vertices.size(),
            &separate_edges[0].front()[0], separate_edges[0].size()*2, 0x20000000 | color);
                                    //front
         scene->DrawLines(&vertices.front(), vertices.size(),
            &separate_edges[2].front()[0], separate_edges[2].size()*2, 0x40000000 | color);
                                    //contour - special color for occluders
         scene->DrawLines(&vertices.front(), vertices.size(),
            &separate_edges[1].front()[0], separate_edges[1].size()*2,
            (((occ_flags&OCCF_DEBUG_OCCLUDING) ? 0xff : alpha)<<24) | 0x00ff00);
      }
      break;
   case I3DOCCLUDER_SPHERE:
      {
         dword alpha = (occ_flags&OCCF_DEBUG_OCCLUDING) ? 0xff : 0x40;
         scene->DebugDrawSphere(m, 1.0f, (alpha<<24) | color);
      }
      break;
   }

   occ_flags &= ~OCCF_DEBUG_OCCLUDING;

   if(zw_en) drv->EnableZWrite(true);
}

//----------------------------
//----------------------------
