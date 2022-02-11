#include "all.h"
#include "visual.h"
#include "volume.h"
#include "mesh.h"
#include <integer.h>

//----------------------------

                              //tolerance between cosines of angles, cosidered as same
#define BSP_COS_ANGLE_TOLERANCE MRG_ZERO
                              //tolerance between distances, cosidered as same
#define BSP_DISTANCE_TOLERANCE MRG_ZERO
                              //random type of best partion selection 
#define RANDOM_PLANES 20

#define USE_BALLANCE_DEPTH 7
                              //building:
#define TREE_SAFE_MAX_DEPTH 2000 //cancel build if tree depth overruns this

                              //enable simplify vertices pool in bsp tree (vertices on same position or similar are unified)
#define REMOVE_REDUNDANT_VERTICES


//----------------------------
                              //BSP builder - class used only for building the tree
                              // (storing results to parent class)
class C_bsp_tree_builder: public C_bsp_tree{
   PI3D_scene scene;          //scene we operate on

   struct S_bsp_polygon{

      BSP_index origin_face;      //point into trees face list
      C_vector<S_vector> verticies;//already transformed to world coord
      bool valid;

      S_bsp_polygon():
         valid(false)
      {}
         /*
      S_bsp_polygon(const S_bsp_polygon &p){

         verticies = p.verticies;
         origin_face = p.origin_face;
         valid = p.valid;
      }
      const S_bsp_polygon &S_bsp_polygon::operator =(const C_bsp_tree_builder::S_bsp_polygon &p){

         verticies.clear();
         verticies = p.verticies;
         origin_face = p.origin_face;
         valid = p.valid;
         return *this;
      }
      */
      void SetVerticies(const S_vector *vp, int num_v, BSP_index face_idx){

         verticies.clear();
         verticies.insert(verticies.end(), vp, vp+num_v);
         origin_face = face_idx;
         valid = true;
      }
   };
   C_vector<S_bsp_polygon> scene_polygons;

   struct S_partion_rating{
      dword split;
      int front;
      int back;
      int on;
      float ballance_score;
      float split_score;
      void Reset(){
         split = 0; front = 0; back = 0; on = 0;
         ballance_score = .0f;
         split_score = .0f;
      }
   };

   struct S_build_stat{
      int polygon_rest;
      int curr_depth;
      void Reset(){

         polygon_rest = 0;
         curr_depth = 0;
      }
   } build_stats;

   enum E_PARTION_RESULT{

      PR_COINCIDE,
      PR_FRONT,
      PR_BACK,
      PR_INTERSECT,
   };

   typedef map<C_str, C_vector<dword> > t_vertice_map;

   typedef C_vector<BSP_index> t_facelist;

//----------------------------

   static bool GetFrameVerticesMap(CPI3D_frame frm, C_vector<word> &v_map){

      v_map.clear();
      assert(frm);
      switch(frm->GetType1()){
      case FRAME_VISUAL:
         {
            CPI3D_visual vis = I3DCAST_CVISUAL(frm);
            CPI3D_mesh_base mb = vis->GetMesh();
            if(!mb)
               return false;

            const S_vector *pick_verts = (const S_vector*)const_cast<PI3D_mesh_base>(mb)->vertex_buffer.Lock(D3DLOCK_READONLY);
            dword vstride = mb->vertex_buffer.GetSizeOfVertex();
            dword num_verts = mb->vertex_buffer.NumVertices();
            v_map.resize(num_verts, 0);
            MakeVertexMapping(pick_verts, vstride, num_verts, &v_map.front());
            const_cast<PI3D_mesh_base>(mb)->vertex_buffer.Unlock();
            return true;
         }
      }
      return false;
   }

//----------------------------

   static void RemapVertices(I3D_triface &t, const C_vector<word> &v_map){

      for(int i = 3; i--; )
         t[i] = v_map[t[i]];
   }

//----------------------------

   bool IsConvex(const S_bsp_triface &tf, const S_bsp_triface &tf2, byte tf1_non_shared_vertex){

      //check position of non shared vertex of face1 against normal of face2
      const S_vector &tf1_vert = vertices[tf.vertices_id[tf1_non_shared_vertex]];
      const S_plane &tf2_plane = planes[tf2.plane_id];

      float dist = tf1_vert.DistanceToPlane(tf2_plane);

      bool is_convex = CHECK_ZERO_LESS(dist);
      return is_convex; 
   }

//----------------------------
// Vytvori hrany pres ktere pri detekci kolize s kouli generuji normalu tak, aby po nich koule klouzala
// v pripade ze se jeji stred dostane pres hranu.
// Urceni slidovacich hran:
// 1) vsechny hrany ktere nejsou sdilene s zadnym jinym facem (okrajove hrany) JSOU slidovaci.
// 2)hrany ktere jsou sdilene jsou slidovaci jen tehdy, kdyz facy ktere sdileji hranu
//   tvori konvexni uhel a tento uhel je vetsi nez otrejsi nez stanoveny uhel.

//Agoritmus:
//Je treba ke kazdemu facu najit facy jehoz 2 vertexy jsou totozne s vertexy naseho facu(+tolerance).
//Pokud tyto facy sviraji pravny uhel s nasim facem, oba facy si pro hranu nastavi slide
//   (a nenusi jiz tuto hranu testovat s ostatnimi facy).

//Algoritmus:
// Vezmi seznam facu.
// Pro kazdy z nich:
//    Nalezni mnozinu facu ktere by mohli sdilet nekterou z hran.
//    (hlavni problem - jak vyradit efectivene facy, ktere nemohou sdilet spolecnou hranu)
//    (nebo jak nalezt vsechny ktere by hranu sdilet mohli)

//Algoritmus:
// 1)Vezmu vsechny vertexy a vytvorim ke kazdemu map_index tak, aby vertexy ktere jsou na stejne pozici meli stejny index.
// 2)Vezmu vsechny facy a pro kazdy z nich zjistim map_indexy (viz bod 1) prirazene jejich vertexum.
// 3)Pro kazdy z techto tri indexu zjistim seznam facu, ktere zde jiz maji ulozeny vertex.
// 4)Zjistim ktere z techto facu se vyskytuje v seznamech 2 indexu. Znemena to, ze tyto dva vertexy tvori spolecnou hranu
//   pro testovany face a tento face.
//  5)Zjistim zda facy sviraji vhodny uhel pro sliding.
//   6)pokud nejsou pro slide vhodne, naleznu index hrany pro oba facy a oznacim je jako neslidovaci.
// 7)Ulozim testovany face do seznamu pro kazdy jeho vertex.

//----------------------------
// Determine which vertices are identical (by examining position) with others in the pool, and create mapping array.
// Version for num vertices > 64k.
   void MakeVertexMappingLarge(const S_vector *vp, dword numv, dword *v_map, float thresh, I3D_BSP_BUILD_CB_PROC *cbP, void *context){

      const dword pitch = sizeof(S_vector);

      if(!numv) 
         return;

      int last_percentage = 0;
      for(dword i = 0; i < numv; i++){
         const S_vector &v1 = *(S_vector*)(((byte*)vp) + pitch * i);
         for(dword j = 0; j < i; j++){
            const S_vector &v2 = *(S_vector*)(((byte*)vp) + pitch * j);
            if((I3DFabs(v1.x-v2.x) + I3DFabs(v1.y-v2.y) + I3DFabs(v1.z-v2.z)) < thresh)
               break;
         }
         v_map[i] = j;

                              //report first half part about bulding progress (0 - 50%)
         int percentage = (50 * i) / numv;
         if(cbP!=NULL && percentage != last_percentage){
            (*cbP)(BM_BUILDING_EDGES, (dword)percentage, 0, context);
            last_percentage = percentage;
         }
      }
   }

//----------------------------
// This function mark edges specified by two or 3 vertices.
// Vertices are passed by flags (1 << by their index).
   static void UnmarkEdgeslide(S_bsp_triface &tf, byte edge_vertices){

                              //find out index of edges from vertices
      byte num_edges(0);
      for(byte k = 3; k--; ){
         //if(shared_v1[k] && shared_v1[(k+1)%3])
         byte curr_id = k;
         byte prev_id = byte((k+1)%3);
         byte vflag_curr = byte(1 << curr_id);
         byte vflag_prev = byte(1 << prev_id);
         if((edge_vertices&vflag_curr) && (edge_vertices&vflag_prev))
         {
            tf.slide_flags &= ~(1 << k);
            num_edges++;
         }
      }
      assert((num_edges == 1) || (num_edges == 3));

   }

//----------------------------
                              //typedefs for shared functions
   struct S_face_info{
      dword face_id;
      byte vertex_id;
      S_face_info(){}
      S_face_info(dword face_id1, byte vertex_id1) : 
         face_id(face_id1),
         vertex_id(vertex_id1)
      {}
   };

   struct S_face_test{
      byte tf_vertices; //shared vertices of tested face (tf)
      byte curr_vertices; //shared vertices of this face (tf2)
   };

   typedef C_vector<S_face_info> t_face_list;

//----------------------------

   bool AcceptableForSlide(const S_bsp_triface &tf1, const S_bsp_triface &tf2, byte non_shared_vertex_of_tf1){

                              //identical planes are allways not acceptable
      if(tf1.plane_id == tf2.plane_id)
         return false; 
                              //edges on concave hull obviously cannot be touched by sphere, thus should not be slided
      if(!IsConvex(tf1, tf2, non_shared_vertex_of_tf1))
         return false;
                              //check angle between faces
      const float MIN_SLIDE_ANGLE = PI/5; //angle tolerance from planar faces(range from 0 to PI)
      //const float MIN_SLIDE_ANGLE = PI/2.1f; //angle tolerance from planar faces(range from 0 to PI)
      const float slide_cos = (float)cos(MIN_SLIDE_ANGLE);
      const S_vector &norm_1 = planes[tf1.plane_id].normal;
      const S_vector &norm_2 = planes[tf2.plane_id].normal;
      const float cosh1 = norm_1.Dot(norm_2); //range (1 - for planar; -1 opposite)
                              //faces forming angle less sharp then specified are not good for slide
                              //(basicly they are close enough to be planar)
      if(cosh1 > slide_cos)
         return false;
      return true; //by default all edges are acceptable
   }


//----------------------------
// This function find faces which are referenced in 2 of vertexes, ie which share exaclty one edge.
// That for that faces make mark which depend upon angle test of faces.
   //void MarkSharedEdges(S_bsp_triface &tf, const t_face_list &list_v1, const t_face_list &list_v2, const t_face_list &list_v3)
   void TestSharedEdges(S_bsp_triface &tf, t_face_list *const*list_v3){


      typedef map<dword, S_face_test> t_face_shared_vert;
      t_face_shared_vert shared_vert;

                              //find out which vertices is used by each face
      for(byte k = 3; k--; ){
         const t_face_list &cur_list = *list_v3[k];
         assert(list_v3[k]->size());
         const dword list_size = dword(list_v3[k]->size() - 1); //we skip last face entry, becaouse it is info about our tested face (tf).
         for(dword i = list_size; i--; ){
            const S_face_info &fi = cur_list[i];
            if(shared_vert.find(fi.face_id) == shared_vert.end()){
               S_face_test &test_inf = shared_vert[fi.face_id];
               test_inf.tf_vertices = byte(1 << k);
               test_inf.curr_vertices = byte(1 << fi.vertex_id);
               //shared_vert[fi.face_id] = byte(1 << k);
            }else{
               S_face_test &test_inf = shared_vert[fi.face_id];
               assert(("each face should be referenced for each vertex onece", !(test_inf.tf_vertices&(1 << k))));
               test_inf.tf_vertices |= (1 << k);
               test_inf.curr_vertices |= byte(1 << fi.vertex_id);
            }
         }
      }
                              //skip faces which has not shared 2 vertices - we looking only for this which share exacly one edge
                              //(triangles can share 1 or 3 edges; ve are not interested in 3 edges since it implies same or 2sided triangle, on which sliding will be with no regard of angle between them)
      S_bsp_triface *p_faces = &faces.front();
      for(t_face_shared_vert::iterator it = shared_vert.begin(); it != shared_vert.end(); it++){
         const byte curr_vert_flags = (*it).second.curr_vertices;
         const byte num_shared_vert = (byte)CountBits(curr_vert_flags);
         if(num_shared_vert != 2)
            continue;

                              //current face
         const dword face2_id = (*it).first;
         S_bsp_triface &tf2 = p_faces[face2_id];
                              //now make test for angle acceptable for sliding
                              //find vertex from tf vhich is not shared
         const byte tf_vertices = (*it).second.tf_vertices;
         byte non_shared_flag = byte((~tf_vertices) & (0x7));
         assert(CountBits(non_shared_flag) == 1);
         int tf1_non_shared_vertex_id = FindHighestBit(non_shared_flag);
         assert(tf1_non_shared_vertex_id != -1 && tf1_non_shared_vertex_id >= 0 && tf1_non_shared_vertex_id < 3);
         if(AcceptableForSlide(tf, tf2, (byte)tf1_non_shared_vertex_id))
            continue;
                              //for all faces which share one edge, we must find which edge is shared 
                              //and mark it in both tested and our current face.
                              
         UnmarkEdgeslide(tf2, curr_vert_flags); //unmark vertices of current face (tf2)

                              //unmark vertices of tested face(tf)
         assert(num_shared_vert == (byte)CountBits((*it).second.tf_vertices)); // num of shared vert should be same for both faces
         UnmarkEdgeslide(tf, (*it).second.tf_vertices);
      }
   }

//----------------------------

   void BuildSlideFlags(I3D_BSP_BUILD_CB_PROC *cbP, void *context){

                              //assign same index for each vertexes on same position
      const S_vector *pick_verts = &vertices.front();
      dword num_verts = vertices.size();
      C_vector<dword> v_map;
      v_map.resize(num_verts, 0);
      MakeVertexMappingLarge(pick_verts, num_verts, &v_map.front(), .001f, cbP, context);

                              //for each (position unique) vertex prepare list of faces which share it
      typedef map<dword, t_face_list> t_faces_vertex_map; //map<vertex_id, list of faces info>
      t_faces_vertex_map faces_vertex_map;

                              //store info about every face in list asociated with his vertices map
                              //and find out faces previously stored for each vertex
      S_bsp_triface *p_faces = &faces.front();
      const dword num_faces = faces.size();
      int last_percentage = 0;
      for(dword i = faces.size(); i--; ){

         S_bsp_triface &tf = p_faces[i];
         t_face_list *list_v[3] = {NULL, NULL, NULL};
         for(byte j = 3; j--; ){
            dword v_indx = tf.vertices_id[j]; //vertex index (into vertices and v_map array)
            dword vert_map_id = v_map[v_indx]; //id obtained from v_map. It is index whis is same for all vertices with same position
                              //store listo for each vertex
            list_v[j] = &faces_vertex_map[vert_map_id];
                              //add info about tested face
            list_v[j]->push_back(S_face_info(i, j)); //i==face_id, j == vertex_id
         }
         TestSharedEdges(tf, list_v);
                              //report second half part about bulding progress (50 - 100%)
         int percentage = 50 + (50 * (num_faces-i)) / num_faces;
         if(cbP!=NULL && percentage != last_percentage){
            (*cbP)(BM_BUILDING_EDGES, (dword)percentage, 0, context);
            last_percentage = percentage;
         }
      }
   }


//----------------------------//old version
   /*void BuildSlideFlags(I3D_BSP_BUILD_CB_PROC *cbP, void *context){

      S_bsp_triface *p_faces = &faces.front();
      typedef map<CPI3D_frame, t_facelist> t_frm_face;

      t_frm_face help_map;

      for(int ii = faces.size(); ii--; ){

         //CPI3D_frame frm = p_faces[ii].origin.frame;
         CPI3D_frame frm = frames.GetFrame(p_faces[ii].origin.frm_id);
         t_frm_face::iterator map_it = help_map.find(frm);
         if(map_it == help_map.end()){
            pair<CPI3D_frame, t_facelist> new_pair(frm, t_facelist());
            pair<t_frm_face::iterator, bool> result_it;
            result_it = help_map.insert(new_pair);
            map_it = result_it.first;
         }
         t_facelist &fl = (*map_it).second;
         fl.push_back(ii);
      }

      int num_frames  = help_map.size();
      int curr_frame(0);
      for(t_frm_face::iterator it = help_map.begin(); it != help_map.end(); it++){

         int percentage = (100 * curr_frame++) / num_frames;
         if(cbP!=NULL){
            (*cbP)(BM_BUILDING_EDGES, (dword)percentage, 0, context);
            //(*cbP)(build_stats.depth, 0, context);
         }
         CPI3D_frame frm = (*it).first;

         C_buffer<I3D_triface> all_faces;
         if(frm->GetType1()==FRAME_VISUAL){
            CPI3D_mesh_base mb = I3DCAST_CVISUAL(frm)->GetMesh();
            if(mb){
               all_faces.assign(mb->NumFaces1());
               mb->GetFaces(all_faces.begin());
            }
         }

         t_facelist &list = (*it).second;
         BSP_index *p_list = &list.front();

         C_vector<word> v_map;
         bool remap = GetFrameVerticesMap(frm, v_map);

         for(int i = list.size(); i--; ){
            S_bsp_triface &tf = p_faces[p_list[i]];
            assert(frames.GetFrameInfo(tf.origin.frm_id).frm==frm);
            I3D_triface tii;

            //bool b1 = tf.GetVertexIndexes(tii, frames);
            bool b1 = frames.GetFace(tf.origin.frm_id, tf.origin.GetFaceIndex(), tii, all_faces.begin());
            assert(b1);
            if(!b1) continue;

            if(remap)
               RemapVertices(tii, v_map);

            bool tf1_shared_edges[3] = {false, false, false}; 

            for(int j = list.size(); j--; ){
               if(i == j)
                  continue;
               assert(p_list[i] != p_list[j]);
               S_bsp_triface &tf2 = p_faces[p_list[j]];
               assert(tf2.origin.frm_id == tf.origin.frm_id);

               I3D_triface tii2;
               //bool b2 = tf2.GetVertexIndexes(tii2, frames);
               bool b2 = frames.GetFace(tf2.origin.frm_id, tf2.origin.GetFaceIndex(), tii2, all_faces.begin());
               assert(b2);
               if(!b2) continue;

               if(remap)
                  RemapVertices(tii2, v_map);

               bool shared_v1[3] = {false, false, false};
               bool shared_v2[3] = {false, false, false};
               byte share_vert_cnt = 0;
               for(int i1 = 3; i1--; ){
                  for(int i2 = 3; i2--; ){
                     if(tii[i1] == tii2[i2]){
                         shared_v1[i1] = shared_v2[i2] = true;
                         share_vert_cnt++;
                     }
                  }
               }
               //assert(share_vert_cnt < 3); //comment out: we can share all verticies if we have 2 faces on same verticies(simulating 2 sided material)
               if(share_vert_cnt != 2)
                  continue;
                                 //check if this two faces share some edge, mark them for face tf from outer loop
                                 //(using redundancy we iterate through all faces in outer loop, so we don't care of tf2 since we reach it again later)
               {
                  for(int k = 3; k--; ){
                     if(shared_v1[k] && shared_v1[(k+1)%3]){
                                 //find shared edge in tf
                        tf1_shared_edges[k] = true;
                     }
                  }
               }
                              //check convexity betveen faces
               
               byte non_shared_v1(0xff);
               for(byte ii = 3; ii--; ){
                  if(!shared_v1[ii]){
                     non_shared_v1 = ii;
                     break;
                  }
               }
               assert(non_shared_v1 != 0xff); //must have non shared vertex
               if(non_shared_v1 == 0xff)
                  continue;


               if(IsConvex(tf, tf2, non_shared_v1)){
                                       //check angle between faces
                  if(tf.plane_id != tf2.plane_id){
                     const S_vector &norm_1 = planes[tf.plane_id].normal;
                     //if(!tf.plane_side) norm_1.Invert();
                     const S_vector &norm_2 = planes[tf2.plane_id].normal;
                     //if(!tf2.plane_side) norm_2.Invert();
                     float cosh1 = norm_1.Dot(norm_2);
                     float cos_zero_delta = cosh1 - 1.0f;
                     if(I3DFabs(cos_zero_delta) > .2f) //little more tolerance for terrains; (basicly, we want slide 90deg edges or sharper)
                        continue; //not near cooplanar
                  }
               }
                                    //find index of edge
               byte edges1 = 0; byte edges2 = 0;
               for(int k = 3; k--; ){
                  if(shared_v1[k] && shared_v1[(k+1)%3]){
                     tf.slide_flags &= ~(1 << k);
                     edges1++;
                  }
                  if(shared_v2[k] && shared_v2[(k+1)%3]){
                     tf2.slide_flags &= ~(1 << k);
                     edges2++;
                  }
               }
               assert((edges1 == 1) && (edges2 == 1));
            }
                                 //now check for no shared edges, we set them non slide for ground faces
                                 //to prevent slide between two separate tarrain pieces which could share this edge.
                                 //todo: this should be better handled with compute shared edges between frames
            {
               for(int i1 = 3; i1--; ){
                  if(!tf1_shared_edges[i1]){
                                 //do not slide edges which are not shared with no oter face in face frame, it will be probably shared with other visual
                     CPI3D_frame origin_frm = frames.GetFrame(tf.origin.frm_id);
                     if(origin_frm->GetType() == FRAME_VISUAL){
                                 //check face normal, we prevent from slide only ground now
                        //const S_vector &f_normal = planes[tf.plane_id].normal;
                        //float fdot = f_normal.Dot(S_vector(0, 1, 0));
                        //if(fdot > .5f)
                           tf.slide_flags &= ~(1 << i1);
                     }
                  }
               }
            }
         }
      }
   }*/

//----------------------------
                              //scene polygons

//----------------------------
// Add face to tree faces list and scene_polygons list for build.
// If face is invalid, it will not be added and false is returned.
   bool AddFace(dword frame_id, int face_id, t_vertice_map &v_map, I3D_BSP_BUILD_CB_PROC *cbP, void *context,
      const I3D_triface *all_faces){

      faces.push_back(S_bsp_triface());
      S_bsp_triface &tf = faces.back();
      //tf.origin.Setup(frame_id, origin_frm, face_id, lod_id);
      tf.origin.Setup(frame_id, face_id, frames);
                              //push transformed verticies
      //bool b = tf.Expand(frames);
      S_vector p_tmp_vert[3];
      bool b = tf.ComputeVertices(frames, p_tmp_vert, all_faces);
      if(!b){
         faces.pop_back();
         CPI3D_frame origin_frm = frames.GetFrame(frame_id);
         if(cbP){
            (*cbP)(BM_LOG, (dword)(const char *)C_fstr("Cannot get geometry info: frame '%s', face %i. Face ignored.",
               (const char*)origin_frm->GetName1(), face_id), 1, context);
         }
         return false;
      }
                             //check for degenerate face
      S_vector face_normal;
      //face_normal.GetNormal(tf.verticies[0], tf.verticies[1], tf.verticies[2]);
      face_normal.GetNormal(p_tmp_vert[0], p_tmp_vert[1], p_tmp_vert[2]);
      if(IsAbsMrgZero(face_normal.Magnitude()))
      {
         faces.pop_back();
         //if(cbP)
         //   (*cbP)(BM_LOG, (dword)(const char *)C_fstr("Invalid face normal: frame '%s', face %i. Face ignored.", (const char*)origin_frm->GetName(), face_id), 1, context);
         return false;
      }
                                 //store vertices
      {
         for(int i = 0; i < 3; i++){
            tf.vertices_id[i] = AddVertex(p_tmp_vert[i], v_map);
         }
         tf.ComputeEdges(&vertices.front(), vertices.size());
      }

                              //also push to build list 
      scene_polygons.push_back(S_bsp_polygon());
      //scene_polygons.back().SetVerticies(tf.verticies, 3, faces.size() -1);
      scene_polygons.back().SetVerticies(p_tmp_vert, 3, faces.size() -1);
      return true;
   }
   
//----------------------------

   E_BSP_BUILD_RESULT AddPolygons(C_vector<PI3D_frame> &frm_list, I3D_BSP_BUILD_CB_PROC *cbP, void *context){

      t_hmap help_map;
                     
      t_vertice_map vert_map;  //map for speed up searchinf for redundant vertices


      const PI3D_frame *p_frm = &frm_list.front();
      for(dword i = 0; i < frm_list.size(); i++){
         PI3D_frame frm = p_frm[i];

         switch(frm->GetType1()){
         case FRAME_VISUAL:
            {

               PI3D_visual vis = I3DCAST_VISUAL(frm);
               PI3D_mesh_base mb = vis->GetMesh();
               if(!mb)              //skip visuals vithout mesh
                  continue;

               int num_fgroups = mb->NumFGroups1();
               if(!num_fgroups)
                  continue;         //skip visuals without faces

               C_buffer<I3D_triface> all_faces(mb->NumFaces1());
               mb->GetFaces(all_faces.begin());
                              //get all verticies on first LOD and push them to list by tri verticies groups
               const dword frm_id = frames.AddFrame(frm);
               const C_bsp_frames::S_frm_inf &fi = frames.GetFrameInfo(frm_id);
               const dword num_faces1 = (fi.lod_id == NO_LOD) ? mb->NumFaces() : fi.num_faces;
               for(dword ii = num_faces1; ii--; ){
                  if(AddFace(frm_id, ii, vert_map, cbP, context, all_faces.begin()))
                     AddFacePlane(faces.back(), help_map);
               }
            }
            break;
         case FRAME_VOLUME:
            {
               PI3D_volume vol = I3DCAST_VOLUME(frm);
               switch(vol->GetVolumeType1()){
               case I3DVOLUME_BOX:
                  {
                                 //push 12 faces in polygon list
                     const dword frm_id = frames.AddFrame(frm);
                     for(int i = 0; i < 12; i++){
                        if(AddFace(frm_id, i, vert_map, cbP, context, NULL))
                           AddFacePlane(faces.back(), help_map);
                     }
                  }
                  break;
               case I3DVOLUME_RECTANGLE:
                  {
                     const dword frm_id = frames.AddFrame(frm);

                                  //assure that rectangle faces will have same plane
                     for(int i = 0; i < 2; i++){
                        if(!AddFace(frm_id, i, vert_map, cbP, context, NULL))
                           break;
                        if(!i){
                           AddFacePlane(faces.back(), help_map);
                        }else{
                           faces.back().plane_id = faces[faces.size()-2].plane_id;
                           //faces.back().plane_side = faces[faces.size()-2].plane_side;
                        }
                     }
                  }
                  break;
               }
            }
            break;
         }
#ifndef GL
                                 //check cancellation
         if(!(i&0x3f)){
            PIGraph igraph = scene->GetDriver1()->GetGraphInterface();
            IG_KEY k = igraph->ReadKey(true);
            if(k==K_ESC){
               return BSP_BUILD_CANCEL;
            }
         }
#endif
         if(cbP)
            (*cbP)(BM_GETTING_POLYGONS, planes.size(), 0, context);
      }

      return BSP_BUILD_OK;
   }

//----------------------------

   typedef multimap<long, int> t_hmap;

//----------------------------

   void AddFacePlane(S_bsp_triface &tf, t_hmap &hmap){

      static const float map_trunc_mask = 1e4f;

      //assert(tf.IsValid());
      assert(frames.GetFrame(tf.origin.frm_id));
      //S_plane face_plane(tf.verticies[0], tf.verticies[1], tf.verticies[2]);
      S_plane face_plane(vertices[tf.vertices_id[0]], vertices[tf.vertices_id[1]], vertices[tf.vertices_id[2]]);

      assert(!IsAbsMrgZero(face_plane.normal.Magnitude()));

                                 //check if equal plane exist in planes list

      //bool same_side = false;    //if face normal is oriented as plane normal; valid only if will be found plane.
      int find_id = -1;
      const S_plane *plane_p = &planes.front();

      long search_d = FloatToInt(face_plane.d*map_trunc_mask);

                                 //optimising: find equal d sequence in map
      /*
      for(int second_test = 0; second_test < 2; second_test++){
         if(second_test){
                                 //second test other side
            search_d = - search_d;
         }
         */
         for(t_hmap::const_iterator it_plus = hmap.find(search_d); it_plus != hmap.end(); it_plus++){
            if((*it_plus).first > search_d)
               break;               //out of tolerance

            int curr_id = (*it_plus).second;
            const S_plane &curr_plane = plane_p[curr_id];
                                    //check angle
            float cosh1 = face_plane.normal.Dot(curr_plane.normal);

            //float cos_zero_delta = I3DFabs(cosh1)-1.0f;
            //if(I3DFabs(cos_zero_delta) > BSP_COS_ANGLE_TOLERANCE)
            float cos_zero_delta = I3DFabs(1.0f - cosh1);
            if(cos_zero_delta > BSP_COS_ANGLE_TOLERANCE)
               continue;
            //same_side = CHECK_ZERO_GREATER(cosh1);
            //float d_delta = (same_side) ? (curr_plane.d - face_plane.d) : (curr_plane.d + face_plane.d);
            float d_delta = curr_plane.d - face_plane.d;

            if(I3DFabs(d_delta) < BSP_DISTANCE_TOLERANCE)
            {
                                    //this test check if face is really cooplanar with plane
               S_bsp_polygon pp;
               //pp.SetVerticies(tf.verticies, 3, (dword)-1);
               assert(tf.vertices_id[0] < vertices.size()); assert(tf.vertices_id[1] < vertices.size()); assert(tf.vertices_id[2] < vertices.size());
               S_vector tmp_vert[3] = {vertices[tf.vertices_id[0]], vertices[tf.vertices_id[1]], vertices[tf.vertices_id[2]]};
               pp.SetVerticies(tmp_vert, 3, (dword)-1);
               E_PARTION_RESULT pr = ClassifyPolygon(pp, curr_id);
               if(pr!=PR_COINCIDE)
                  continue;
               find_id = curr_id;
               break;               //plane found
            }
         }
         /*
         if(find_id != -1)
            break;
      }
      */

      if(find_id == -1){
         tf.plane_id = planes.size();
         planes.push_back(face_plane);
         //tf.plane_side = true;
         //search_d = -search_d;   //value was negated in second test, return sign back
         pair<long, int> hp(search_d, tf.plane_id);      
         hmap.insert(hp);
      }else{
                              //re-use index into found plane
         tf.plane_id = find_id;
         //tf.plane_side = same_side;
      }
   }

//----------------------------

   E_BSP_BUILD_RESULT BuildScenePolygons(I3D_BSP_BUILD_CB_PROC *cbP, void *context){

      assert(scene);
      if(!scene)
         return BSP_BUILD_FAIL;

      PI3D_frame prim_sector = scene->GetPrimarySector1();
      assert(prim_sector);
      if(!prim_sector)
         return BSP_BUILD_FAIL;
                                 //clear tree lists
      faces.clear();
      planes.clear();
                                 //collect candidate list of frames, which should be in bsp tree
      C_vector<PI3D_frame> cadidate_list;

      scene->GetStaticFrames(cadidate_list);

      scene_polygons.reserve(cadidate_list.size() * 80);
      faces.reserve(cadidate_list.size() * 80);
      planes.reserve(cadidate_list.size() * 80);
      vertices.reserve(cadidate_list.size() * 80);

      E_BSP_BUILD_RESULT ret = AddPolygons(cadidate_list, cbP, context);

      build_stats.polygon_rest = scene_polygons.size();

      return ret;
   }

//----------------------------

   /*static C_str GetVertexCode(const S_vector &v){

      int x = FloatToInt(v.x);
      int y = FloatToInt(v.y);
      int z = FloatToInt(v.z);

      return C_fstr("%i:%i:%i", x,y,z);
   }*/

//----------------------------
// border_tolerance must be equal or greater then VERTICES_TRASH in AddVertex
   static void GetVertexCodes(const S_vector &v, float border_tolerance, C_vector<C_str> &out_codes){

      C_vector<int> vi[3];

      for(int kk = 3; kk--; ){
         int d[2];
         d[0] = FloatToInt(v[kk] + border_tolerance);
         d[1] = FloatToInt(v[kk] - border_tolerance);
         vi[kk].resize(d[0] != d[1] ? 2 : 1, 0);
         for(dword jj = vi[kk].size(); jj--; )
            vi[kk][jj] = d[jj];
      }

      dword num_codes = vi[0].size()*vi[1].size()*vi[2].size();
      out_codes.resize(num_codes);

      const C_vector<int> &x = vi[0];
      const C_vector<int> &y = vi[1];
      const C_vector<int> &z = vi[2];

      dword id = 0;
      for(dword i = x.size(); i--; ){
         for(dword j = y.size(); j--; ){
            for(dword k = z.size(); k--; ){
               out_codes[id] = C_fstr("%iy%iz%i", x[i], y[j], z[k]);
               id++;
            }
         }
      }
      assert(id==num_codes);
   }

//----------------------------

   dword AddVertex(const S_vector &v1, t_vertice_map &v_map){


#ifdef REMOVE_REDUNDANT_VERTICES

      const float VERTICES_TRASH = 1e-4f;
      const float BORDER_TOL = VERTICES_TRASH*2;

#if 0
      S_vector *p_verts = vertices.begin();
      for(dword i = 0; i < vertices.size(); i++ ){
         const S_vector &v2 = p_verts[i];
         if((Fabs(v1.x-v2.x) + Fabs(v1.y-v2.y) + Fabs(v1.z-v2.z)) < VERTICES_TRASH)
            return i;
      }

#else

      //const C_str v1_code(GetVertexCode(v1));
      C_vector<C_str> codes;
      GetVertexCodes(v1, BORDER_TOL, codes);
      assert(codes.size());
      const C_str &first_code = codes[0];

      t_vertice_map::iterator it;
      //it = v_map.find(v1_code);
      it = v_map.find(first_code);
                                 //if segmnet vith same vertices found, search it
      if(it != v_map.end()){

         C_vector<dword> &serch_v = (*it).second;

         S_vector *p_verts = &vertices.front();
         dword *p_search = &serch_v.front();
         for(dword i = 0; i < serch_v.size(); i++ ){
            const dword v2_id = p_search[i];
            assert(v2_id < vertices.size());
            const S_vector &v2 = p_verts[v2_id];
            if((I3DFabs(v1.x-v2.x) + I3DFabs(v1.y-v2.y) + I3DFabs(v1.z-v2.z)) < VERTICES_TRASH)
               return v2_id;
         }
      }
//      else{
//                                 //segment not exist yet, create one
//         pair<t_vertice_map::iterator, bool> insert_result =
//            v_map.insert(t_vertice_map::value_type(v1_code, C_vector<dword>()));
//         it = insert_result.first;
//      }
//                                 //no matching vertex found, add new one
//      (*it).second.push_back(vertices.size());

                              //no matching vertex, add into all regions
      for(dword j = codes.size(); j--; ){
         const C_str &curr_code = codes[j];
         it = v_map.find(curr_code);
         if(it == v_map.end()){
                              //create new required region
            pair<t_vertice_map::iterator, bool> insert_result =
               v_map.insert(t_vertice_map::value_type(curr_code, C_vector<dword>()));
            it = insert_result.first;
         }
                              //add index of current vertex into it
         (*it).second.push_back(vertices.size());
      }
#endif

#endif //REMOVE_REDUNDANT_VERTICES

      vertices.push_back(v1);
      return (vertices.size() - 1);
   }


//----------------------------
                              //node build support


//----------------------------
// Make axis alignated box aroud all point in list
   void MakePolygonsVolume(I3D_bbox &bb, const C_vector<S_bsp_polygon> &poly_list){

      static const float EXPAND_VALUE = .001f;
      bb.Invalidate();

      for(int kk = poly_list.size(); kk--;){
         const S_bsp_polygon &poly = poly_list[kk];
         assert(poly.valid);
         for (int ii=poly.verticies.size(); ii--;){
            const S_vector &point = poly.verticies[ii];
                                 //update min and max
            for (int i=3; i--;){
               if (point[i] < bb.min[i]) bb.min[i] = point[i];
               if (point[i] > bb.max[i]) bb.max[i] = point[i];
            }
         }
      }
                                 //expand the bounding box slightly
      for(int jj = 3; jj--;){
         bb.min[jj] -= EXPAND_VALUE;
         bb.max[jj] += EXPAND_VALUE;
      }
   }

//----------------------------
   
   E_PARTION_RESULT ClassifyPolygon(const S_bsp_polygon &poly, BSP_index plane_id){

      assert(plane_id < planes.size());
      const S_plane &plane = planes[plane_id];
      E_PARTION_RESULT poly_class = PR_COINCIDE;
   
      assert(poly.valid);

      const S_vector *p_vert = &poly.verticies.front();
      for(int i = poly.verticies.size(); i--; ){
         float f = p_vert->DistanceToPlane(plane);
         p_vert++;
         if(f > BSP_SPLIT_TOLERANCE){
            poly_class = PR_FRONT;
            break;
         }else
         if(f < -BSP_SPLIT_TOLERANCE){
            poly_class = PR_BACK;
            break;
         }
      }
      if(i!=-1){
         if(poly_class == PR_BACK){
            while(i--){
               float f = p_vert->DistanceToPlane(plane);
               p_vert++;
               if(f > BSP_SPLIT_TOLERANCE)
                  return PR_INTERSECT;
            }
         }else{
            assert(poly_class == PR_FRONT);
            while(i--){
               float f = p_vert->DistanceToPlane(plane);
               p_vert++;
               if(f < -BSP_SPLIT_TOLERANCE)
                  return PR_INTERSECT;
            }
         }
      }
      return poly_class;
   }

//----------------------------

   E_PARTION_RESULT Split_Polygon(const S_bsp_polygon &poly,
      const S_plane &partion, C_vector<S_bsp_polygon> &front, C_vector<S_bsp_polygon> &back)
   {

      assert(poly.valid);
      int   count = poly.verticies.size();
      E_PARTION_RESULT ret_res = PR_COINCIDE;

      assert(count > 2);

      S_vector ptA, ptB;
      C_vector<S_vector> out_pts;
      C_vector<S_vector> in_pts;

      float	sideA, sideB;
      ptA = poly.verticies[count - 1];
      sideA = ptA.DistanceToPlane(partion);
      for (short i = -1; ++i < count;)
      {
         ptB = poly.verticies[i];
         sideB = ptB.DistanceToPlane(partion);
         if (sideB > BSP_SPLIT_TOLERANCE)
         {
            if (sideA < -BSP_SPLIT_TOLERANCE)
            {
               // compute the intersection point of the line from 
               // point A to point B with the partition
               // plane. This is a simple ray-plane intersection.
               S_vector v = ptB - ptA;
               S_vector sect_point;
               bool b;
               b = partion.Intersection(ptA, v, sect_point);
               assert(b);
               out_pts.push_back(sect_point); in_pts.push_back(sect_point);
               ret_res = PR_INTERSECT;
            }
            out_pts.push_back(ptB);

            if(ret_res == PR_COINCIDE)
               ret_res = PR_FRONT;
            if(ret_res == PR_BACK)
               ret_res = PR_INTERSECT;
         }
         else if (sideB < -BSP_SPLIT_TOLERANCE)
         {
            if (sideA > BSP_SPLIT_TOLERANCE)
            {
               // compute the intersection point of the line
               // from point A to point B with the partition
               // plane. This is a simple ray-plane intersection.
               S_vector v = ptB - ptA;
               S_vector sect_point;
               bool b;
               b = partion.Intersection(ptB, v, sect_point);
               assert(b);
               out_pts.push_back(sect_point); in_pts.push_back(sect_point);
               ret_res = PR_INTERSECT;
            }
            in_pts.push_back(ptB);
            if(ret_res == PR_COINCIDE)
               ret_res = PR_BACK;
            if(ret_res == PR_FRONT)
               ret_res = PR_INTERSECT;
         }
         else{
            out_pts.push_back(ptB); in_pts.push_back(ptB);
         }
         ptA = ptB;
         sideA = sideB;
      }

      if(ret_res == PR_INTERSECT){
         front.push_back(S_bsp_polygon());
         front.back().SetVerticies(&out_pts.front(), out_pts.size(), poly.origin_face);
         back.push_back(S_bsp_polygon());
         back.back().SetVerticies(&in_pts.front(), in_pts.size(), poly.origin_face);
      }

      return  ret_res;
   }

//----------------------------

   void ClassifyPartionPlane(BSP_index plane_id, const C_vector<S_bsp_polygon> &poly_list, S_partion_rating &rating){

      rating.Reset();
      for(int j = poly_list.size(); j--;){
         const S_bsp_polygon &pp = poly_list[j];

         assert(pp.valid);

         E_PARTION_RESULT pr = ClassifyPolygon(pp, plane_id);

         switch(pr){
         case PR_COINCIDE:
            rating.on++;
            break;
         case PR_FRONT:
            rating.front++;
            break;
         case PR_BACK:
            rating.back++;
            break;
         case PR_INTERSECT:
            rating.split++;
            break;
         }
      }
   }

//----------------------------

   BSP_index PickPartionPlane(const C_vector<S_bsp_polygon> &poly_list);

//----------------------------

   BSP_index PickPartionPlaneRandom(const C_vector<S_bsp_polygon> &poly_list){

      C_vector<dword> rest_rnd;
      dword best_split = 9999999;
      float best_ballance = -1.0f;
      BSP_index best_plane_id = (dword)-1;

      bool exclusive_ones = poly_list.size() < (RANDOM_PLANES *3);
      if(exclusive_ones){
                                 //prepare rnd array
         for(int i = poly_list.size(); i--;){
            rest_rnd.push_back(i);
         }
      }

      const S_bsp_triface *p_faces = &faces.front();
      for(int rest_num = RANDOM_PLANES; rest_num--;){
         dword rnd_polygon_id;
         if(exclusive_ones){
            if(rest_rnd.size()==0)
               break;
            dword rnd_index = S_int_random(rest_rnd.size());
            rnd_polygon_id = rest_rnd[rnd_index];
            assert(rnd_index < rest_rnd.size());
            rest_rnd[rnd_index] = rest_rnd.back(); rest_rnd.pop_back();
         }
         else{
            rnd_polygon_id = S_int_random(poly_list.size());
         }

         S_partion_rating rating;
         assert(rnd_polygon_id < poly_list.size());
         assert(poly_list[rnd_polygon_id].valid);
         BSP_index rnd_face_id = poly_list[rnd_polygon_id].origin_face;
         assert(rnd_face_id < faces.size());
         const S_bsp_triface &trf = p_faces[rnd_face_id];
         //assert(trf.IsValid());
         assert(frames.GetFrame(trf.origin.frm_id));
         BSP_index plane_id = trf.plane_id;
         ClassifyPartionPlane(plane_id, poly_list, rating);

         if(build_stats.curr_depth < USE_BALLANCE_DEPTH){
            float greater = (float)Max(rating.back, rating.front);
            float smaller = (float)Min(rating.back, rating.front);
            float curr_ballace = (float)greater ? smaller / greater : .0f;
            if(best_ballance < curr_ballace){
               best_ballance = curr_ballace;
               best_plane_id = plane_id;
            }
         }else
         if(best_split > rating.split){
            best_split = rating.split;
            best_plane_id = plane_id;
         }
      }
      rest_rnd.clear();
      return best_plane_id;
   }

//----------------------------

   E_BSP_BUILD_RESULT BuildNode(C_vector<S_bsp_polygon> &poly_list, dword &ret_offset,
      I3D_BSP_BUILD_CB_PROC *cbP, void *context){

      build_stats.curr_depth++;

      //MakePolygonsVolume(node->bbox, poly_list);
      BSP_index prt_plane = PickPartionPlaneRandom(poly_list);

                                 //save plane id
      //node->plane_id = prt_plane;
      const S_plane &partion_plane = planes[prt_plane];
                                 //divide polygon list to polygons on plane, front list and back lists
      C_vector<S_bsp_polygon> poly_front;
      C_vector<S_bsp_polygon> poly_back;
      C_vector<BSP_index> coincident_list;

      for(int i = poly_list.size(); i--; ){

         S_bsp_polygon &pp = poly_list[i];
         assert(pp.valid);

         E_PARTION_RESULT pr = Split_Polygon(pp, partion_plane, poly_front, poly_back);

         switch(pr){
         case PR_COINCIDE:
                                 //store at C_vector, alloc in node at end (save memory in final tree)
            coincident_list.push_back(pp.origin_face);
            build_stats.polygon_rest--;
            break;
         case PR_FRONT:
                                 //add to temporary front list
            poly_front.push_back(pp);
            break;
         case PR_BACK:
                                 //add to temporary back list
            poly_back.push_back(pp);
            break;
         case PR_INTERSECT:
            build_stats.polygon_rest++;
            break;
         }
         poly_list.pop_back();
      }

      assert(coincident_list.size()); //every time must be pushed at least one face - this one which plane was used as partion
      if(!coincident_list.size()){
         return BSP_BUILD_FAIL;
      }
      //node->SetFaces(coincident_list.begin(), coincident_list.size());
#ifndef GL
                                 //let check cancellation
      if(!--esc_check_count){
         esc_check_count = 200;
         PIGraph igraph = scene->GetDriver1()->GetGraphInterface();
         IG_KEY k = igraph->ReadKey(true);
         if(k==K_ESC){
            return BSP_BUILD_CANCEL;
         }
      }
#endif

      if(cbP!=NULL){
         (*cbP)(BM_BUILDING_NODES, build_stats.polygon_rest, 0, context);
         //(*cbP)(build_stats.depth, 0, context);
      }

                                 //assure that divided list was cleared by children
      assert(!poly_list.size());

      assert(build_stats.curr_depth < TREE_SAFE_MAX_DEPTH);
      if(build_stats.curr_depth > TREE_SAFE_MAX_DEPTH){
         return BSP_BUILD_FAIL;
      }

      {
                                 //we cannot store builded_node, it can change due realloc
         S_bsp_node *builded_node = nodes.CreateNode(coincident_list.size());
         if(!builded_node){
            return BSP_BUILD_FAIL;
         }
         builded_node->plane_id = prt_plane;
                                    //assign faces
         assert(builded_node->NumFaces() == coincident_list.size());
         BSP_index *p_list = &coincident_list.front();
         for(dword k = builded_node->NumFaces(); k--; ){
            builded_node->facelist[k] = p_list[k];
         }
                                 //store offset of current builded_node
         ret_offset = nodes.GetOffset(builded_node);
      }
   
                                 //we need also obtain offsets of front and back node, which we are going to build
      dword front_offset(0);
      dword back_offset(0);

      if(poly_front.size()){
         //if(node->front == NULL)
         //   node->front = new S_bsp_node;
         //E_BSP_BUILD_RESULT br1 = BuildNode(poly_front, cbP, context);
         E_BSP_BUILD_RESULT br1 = BuildNode(poly_front, front_offset, cbP, context);
         assert(poly_front.size() == 0);
         if(br1 != BSP_BUILD_OK)
            return br1;
      }

      if(poly_back.size()){
         //if(node->back == NULL)
         //   node->back = new S_bsp_node;
         //E_BSP_BUILD_RESULT br2 = BuildNode(poly_back, cbP, context);
         E_BSP_BUILD_RESULT br2 = BuildNode(poly_back, back_offset, cbP, context);
         assert(poly_back.size() == 0);
         if(br2 != BSP_BUILD_OK)
            return br2;
      }
                                 //now we need find node which we created and assign indexes to it
      {
         //S_bsp_node *builded_node = nodes[node_id];
         //builded_node->front_offset = front_offset;
         //builded_node->back_id = back_id;
         S_bsp_node *builded_node = nodes.GetNode(ret_offset);
         builded_node->front_offset = front_offset;
         builded_node->back_offset = back_offset;
      }

      build_stats.curr_depth--;

      return BSP_BUILD_OK;                                                         
   }

//----------------------------
   int esc_check_count;

public:
   C_bsp_tree_builder(PI3D_scene s):
      scene(s),
      esc_check_count(1)
   {}

   I3D_RESULT Build(I3D_BSP_BUILD_CB_PROC *cbP, void *context){

      Clear();
      build_stats.Reset();
      E_BSP_BUILD_RESULT br = BuildScenePolygons(cbP, context);
      if((br == BSP_BUILD_OK) && scene_polygons.size()){
         //root = new S_bsp_node;
         //br = BuildNode(root, scene_polygons, cbP, context);
         assert(!nodes.NumNodes());
         {
                                 //reserve approximate memory for prevent frequent reallocations.
            const dword app_num_nodes = 3*scene_polygons.size();
            const word avg_faces_per_node = 2;
            const dword aproximate_size = app_num_nodes * S_bsp_node::BspNodeSize(avg_faces_per_node);
            nodes.Reserve(aproximate_size);
         }
         dword root_offset;
         br = BuildNode(scene_polygons, root_offset, cbP, context);
         if(br == BSP_BUILD_OK){
            assert(root_offset == 0);
            nodes.Compact();
         }
      }

      I3D_RESULT ret;

      switch(br){
      case BSP_BUILD_OK:
         {
            valid = true;
            ret = I3D_OK;
            if(!nodes.OffsetsToNodes())
               return I3DERR_GENERIC;
            BuildSlideFlags(cbP, context);
            frames.BuildChecksum();
         }
         break;
      case BSP_BUILD_CANCEL:
         ret = I3DERR_CANCELED;
         break;
      default:
         ret = I3DERR_OBJECTNOTFOUND;
      }
      return ret;
   }
};

//----------------------------

I3D_RESULT I3D_scene::CreateBspTree(I3D_BSP_BUILD_CB_PROC *cbP, void *context){

                              //clear current tree
   bsp_tree.Clear();
   bsp_draw_help.Clear();
                              //use temp builder class for doing the work
   C_bsp_tree_builder bsp_builder(this);
   I3D_RESULT ir = bsp_builder.Build(cbP, context);
                              //if successful, copy its base tree class into scene's tree class
   if(I3D_SUCCESS(ir))
      bsp_tree = bsp_builder;
   return ir;
}

//----------------------------
