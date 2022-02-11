#include "all.h"
#include "visual.h"
#include "volume.h"
#include "bsp.h"
#include "mesh.h"

                              //lod version of bsp, using smallest defined lod on object.
#define USE_LOD

const byte BSP_VERSION = 0x0010;

                              //file support:
                              //which types of frame can be written in file
#define FRAME_BASE_ALLOWED_TYPES (ENUMF_ALL)
//#define FRAME_BASE_ALLOWED_TYPES (ENUMF_VISUAL | ENUMF_VOLUME)


                              //enable count collision tests on primitives
#define VOLUME_STATISTICS

//const dword DEBUG_TRIFACE_ID = 9268;

//----------------------------//build log

#define BSP_ERR_LOG(cp) \
   FILE *f_log = fopen("BspBuild.log", "wt"); \
   if(f_log){ \
      fwrite((const char *)cp, 1, strlen((const char *)cp), f_log); fwrite("\n", 1, 1, f_log); \
      fclose(f_log); \
   }

//----------------------------//debug:

//----------------------------

#if defined _DEBUG

//#define MS_F_DEBUG(n) scene->GetDriver1()->DEBUG(n)
#define MS_F_DEBUG(n)

#else

#define MS_F_DEBUG(n)


#endif //_DEBUG

//----------------------------
// box indicies
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

static const word box_fill_indicies[] = {
   1, 3, 7, 1, 7, 5,
   0, 4, 6, 0, 6, 2,
   2, 6, 7, 2, 7, 3,
   0, 1, 5, 0, 5, 4,
   0, 2, 3, 0, 3, 1,
   5, 7, 6, 5, 6, 4
};

static const S_vector rect_edges[4] = {
   S_vector(-1.0f, -1.0f, 0.0f),
   S_vector( 1.0f, -1.0f, 0.0f),
   S_vector( 1.0f,  1.0f, 0.0f),
   S_vector(-1.0f,  1.0f, 0.0f),
};

static const word rect_fill_indicies[] = {
   0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2
};

//----------------------------
#pragma warning(disable: 4035)
                              //class representing type occupying 3 bytes
class C_triword{
   byte code[3];
public:
   inline void Store(dword dw){
      assert(dw < 16777216);
      *(word*)code = *(word*)&dw;
      code[2] = ((byte*)&dw)[2];
   }
   inline operator dword() const{
#if 1
      __asm{
         mov ecx, this
         mov al, [ecx+2]
         shl eax, 16
         mov ax, [ecx]
         and eax, 0xffffff
      }
#else
      union{
         dword dw;
         byte b[4];
         word w[2];
      } u;
      u.w[0] = *(word*)code;
      u.b[2] = code[2];
      u.b[3] = 0;
      return u.dw;
#endif
   }
};
#pragma warning(default: 4035)

//----------------------------

typedef map<C_str, CPI3D_frame> t_frms_names_map;

static void CreateFramesNameMap(CPI3D_scene scn, t_frms_names_map &frm_map){

                                 //create name - frames map for speed up loading
   static struct S_hlp{
      static I3DENUMRET I3DAPI cbAddFramesToMap(PI3D_frame frm, dword c){
         t_frms_names_map &frame_map = *(t_frms_names_map*)c;
         frame_map[frm->GetName1()] = frm;
         return I3DENUMRET_OK;
      }
   }hlp;
   scn->EnumFrames(S_hlp::cbAddFramesToMap, (dword)&frm_map, FRAME_BASE_ALLOWED_TYPES);
}

//----------------------------
// C_bsp_frames
//----------------------------

const word MAX_FRM_NAME_LEN = 512;

#pragma pack(push, 1)

struct S_frames_hdr{
   BSP_frame_index num_frames;
   dword num_faces;
};

struct S_frm_hdr{
   word name_len;
   byte lod_id;
   C_triword first_face;    //index of first face in lod_faces buffer
   word num_faces;
};

//----------------------------
struct S_cksum_sav{
   float matrix_sum;
   float vertc_sum;
   C_triword num_v;
};

#pragma pack(pop)

//----------------------------

dword C_bsp_frames::CreateLod(CPI3D_frame frm, byte &ret_lod){

   assert(frm);
   switch(frm->GetType1()){
   case FRAME_VISUAL:
      {
         CPI3D_mesh_base mb = I3DCAST_CVISUAL(frm)->GetMesh();
         if(!mb)
            break;
         const dword num_lods = mb->NumAutoLODs();
         if(!num_lods)
            break;
         ret_lod = byte(num_lods < 255 ? num_lods-1 : 254);
         const C_auto_lod *lods = mb->GetAutoLODs();
         C_auto_lod &lod = *const_cast<C_auto_lod*>(lods+ret_lod);
         const dword num_faces = lod.NumFaces1();
         lod_faces.resize(lod_faces.size()+num_faces);
         lod.GetFaces(lod_faces.end()-num_faces);
         return num_faces;
      }
      break;
   }
   return 0;
}

//----------------------------

BSP_frame_index C_bsp_frames::AddFrame(CPI3D_frame frm){

   assert(frm);
   frm_info.push_back(S_frm_inf());
   S_frm_inf &fi = frm_info.back();
   fi.frm = frm;

#ifdef USE_LOD
   fi.num_faces = CreateLod(frm, fi.lod_id);
#else
   fi.num_faces = 0;
#endif

   if(fi.num_faces){
      assert(fi.num_faces <= lod_faces.size());
      fi.first_face = lod_faces.size() - fi.num_faces;
   }else
   {
      fi.lod_id =  NO_LOD;
      fi.first_face = 0;
   }
   return BSP_frame_index(frm_info.size() - 1);
}

//----------------------------

void C_bsp_frames::BuildChecksum(){

   S_frm_inf *p_frm_inf = &frm_info.front();
   for(dword i = frm_info.size(); i--; ){
      S_frm_inf &fi = p_frm_inf[i];
      assert(fi.frm);
      fi.frm->GetChecksum(fi.cksum.matrix_sum, fi.cksum.vertc_sum, fi.cksum.num_v);
   }
}

//----------------------------

bool C_bsp_frames::Save(C_cache *cp) const{
                              
                              //header
   S_frames_hdr frames_hdr;
   frames_hdr.num_frames = BSP_frame_index(frm_info.size());
   frames_hdr.num_faces = lod_faces.size();
   cp->write(&frames_hdr, sizeof(S_frames_hdr));

                              //write lod faces
   cp->write(lod_faces.begin(), sizeof(I3D_triface)*frames_hdr.num_faces);

   const dword num_frames = frames_hdr.num_frames;
                           //write num of frame names
   const S_frm_inf *p_frm_inf = &frm_info.front();
   for(dword i = 0; i < num_frames; i++){
      const S_frm_inf &fi = p_frm_inf[i];
                           //write frame's basic info (name, lod, etc)
      S_frm_hdr hdr;
      hdr.lod_id = fi.lod_id;
      hdr.first_face.Store(fi.first_face);
      assert(fi.num_faces < 0xffff);
      hdr.num_faces = (word)fi.num_faces;
      const C_str &frm_name = fi.frm->GetName1();
      assert(frm_name.Size() < MAX_FRM_NAME_LEN);
      hdr.name_len = word(frm_name.Size());
      cp->write(&hdr, sizeof(S_frm_hdr));
      cp->write(&frm_name[0], hdr.name_len);
   }
                              //now checksum(can be loaded as block and also ingnored)
   for(dword i2 = 0; i2 < num_frames; i2++){
      const S_frm_inf &fi = p_frm_inf[i2];
      S_cksum_sav ck_sav;
      ck_sav.matrix_sum = fi.cksum.matrix_sum;
      ck_sav.vertc_sum = fi.cksum.vertc_sum;
      ck_sav.num_v.Store(fi.cksum.num_v);
      cp->write(&ck_sav, sizeof(S_cksum_sav));
   }
   return true;
}

//----------------------------

bool C_bsp_frames::Load(C_cache *cp, CPI3D_scene scn, bool cmp_checksum, I3D_LOAD_CB_PROC *cbP, void *context){

   Clear();
                              //header
   S_frames_hdr frames_hdr;
   cp->read(&frames_hdr, sizeof(S_frames_hdr));

   lod_faces.assign(frames_hdr.num_faces, I3D_triface());
                              //lod faces
   cp->read(lod_faces.begin(), sizeof(I3D_triface)*frames_hdr.num_faces);

   const dword num_frames = frames_hdr.num_frames;

   t_frms_names_map frms_map;
   CreateFramesNameMap(scn, frms_map);

                           //reserve space in base vector
   frm_info.assign(num_frames, S_frm_inf());
   S_frm_inf * const p_frm_inf = &frm_info.front(); 
   for(dword i = 0; i < num_frames; i++){

      S_frm_inf &fi = p_frm_inf[i];
                           //frame basic info
      S_frm_hdr hdr;
      cp->read(&hdr, sizeof(S_frm_hdr));
                           //read string specifying name of frame
      char buf[MAX_FRM_NAME_LEN];
      cp->read(buf, hdr.name_len);
      buf[hdr.name_len] = 0;

      t_frms_names_map::const_iterator map_it;
      map_it = frms_map.find(C_str(buf));
      if(map_it == frms_map.end()){
         if(cbP)
            (*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: frame '%s' not found" , buf), 0, context);
         return false;
      }
      CPI3D_frame frm = (*map_it).second;
      if(!(frm->GetFlags()&I3D_FRMF_STATIC_COLLISION)){
         if(cbP)
            (*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: frame '%s' is not static: ", buf), 1, context);
      }
      fi.frm = frm;
      fi.lod_id = hdr.lod_id;
      fi.first_face = hdr.first_face;
      fi.num_faces = hdr.num_faces;
   }
                              //now checksum(can be loaded as block and also ingnored)
   for(dword i2 = 0; i2 < num_frames; i2++){
      S_frm_inf &fi = p_frm_inf[i2];
      S_cksum_sav ck_sav;
      cp->read(&ck_sav, sizeof(S_cksum_sav));

      fi.cksum.matrix_sum = ck_sav.matrix_sum;
      fi.cksum.vertc_sum = ck_sav.vertc_sum;
      fi.cksum.num_v = ck_sav.num_v;
   }

#if 1
   if(cmp_checksum){
                              //just inform about checksum, we saving entire tree
      CompareChecksum(cbP, context);
   }
#endif

   return true;
}

//----------------------------

bool C_bsp_frames::CompareChecksum(I3D_LOAD_CB_PROC *cbP, void *context) const{

   const float FLOAT_TOLERANCE = 1e-4f;
   const float MATRIX_TOLERANCE = 1e-3f;

   dword max_err_msgs = 6;
   dword num_frm_failed = 0;

   bool ok(true);
   const S_frm_inf *p_frm_inf = &frm_info.front();
   for(dword i = frm_info.size(); i--; ){
                           //current frames checksum
      float m_sum;
      float v_sum;
      dword num_v;
      bool frame_ok(true);

      const S_frm_inf &fi = p_frm_inf[i];
      assert(fi.frm);
      fi.frm->GetChecksum(m_sum, v_sum, num_v);
                              //num vertices delta
      dword num_v_delta = abs(int(num_v - fi.cksum.num_v));
      if(num_v_delta){
         frame_ok = false;
      }                       //frame matrix delta
      float matrix_delta = I3DFabs(fi.cksum.matrix_sum - m_sum);
      if(matrix_delta > MATRIX_TOLERANCE){
         frame_ok = false;
      }
                              //vertices sum delta
      float vertices_delta = I3DFabs(fi.cksum.vertc_sum - v_sum);
                              //3 floats for each vertex
      const float v_tolerance = num_v*3*FLOAT_TOLERANCE;
      if(vertices_delta > v_tolerance){
         frame_ok = false;
      }
                              //report failed frames
      if(!frame_ok){
         if(cbP && max_err_msgs){
            C_fstr err("Bsp: checksum failed (frm: '%s', numv_d: %i, matrix_d: %.4f, vert_d: %.4f, num_v: %i)",
               (const char*)fi.frm->GetName(), num_v_delta, matrix_delta, vertices_delta, num_v);
            (*cbP)(CBM_ERROR, (dword)(const char*)err, 0, context);
            --max_err_msgs;
         }
         ++num_frm_failed;
         ok = false;
      }
   }
   if(!ok && cbP){
      C_fstr err("Bsp: Checksum failed on %i frames(total %i).", num_frm_failed, frm_info.size());
      (*cbP)(CBM_ERROR, (dword)(const char*)err, 0, context);
   }
   return ok;
}

//----------------------------

bool C_bsp_frames::GetFace(BSP_frame_index frm_id, dword face_id, I3D_triface &tf, const I3D_triface *all_faces) const{

   assert(frm_id < frm_info.size());
   const S_frm_inf &fi = frm_info[frm_id];
   assert(fi.frm);
   switch(fi.frm->GetType1()){
   case FRAME_VISUAL:
      {
         if(fi.lod_id != NO_LOD){
            assert(face_id < fi.num_faces);
            assert((fi.first_face + face_id) < lod_faces.size());
            tf = lod_faces[fi.first_face + face_id];
         }else{
            CPI3D_mesh_base mb = I3DCAST_CVISUAL(fi.frm)->GetMesh();
            if(!mb)
               return false;
            if(face_id >= mb->NumFaces1())
               return false;
            tf = all_faces[face_id];
         }
      }
      break;
   case FRAME_VOLUME:
      {
         switch(I3DCAST_CVOLUME(fi.frm)->GetVolumeType1()){
         case I3DVOLUME_BOX:
            {
               if(face_id >= 12)
                  return false;
               for(int j = 0; j < 3; j++)
                  tf[j] = box_fill_indicies[face_id*3 + j];
            }
            break;
         case I3DVOLUME_RECTANGLE:
            {
               if(face_id >= 2)
                  return false;
               for(int j = 0; j < 3; j++)
                  tf[j] = rect_fill_indicies[face_id*3 + j];
            }
            break;
         default:
            assert(0);
            return false;
         }
      }
      break;
   default:
      assert(0);
      return false;
   }
   return true;
}

//----------------------------
// S_bsp_triface
//----------------------------

bool S_bsp_triface::ComputeVertices(const C_bsp_frames &frames, S_vector *pvertices, const I3D_triface *faces){

   CPI3D_frame origin_frm = frames.GetFrame(origin.frm_id);
   assert(origin_frm);
   if(!origin_frm)
      return false;

   I3D_triface tf;
   bool b = frames.GetFace(origin.frm_id, origin.GetFaceIndex(), tf, faces);
   assert(b);
   if(!b)
      return false;

   switch(origin_frm->GetType1()){

   case FRAME_VISUAL:
      {
         CPI3D_mesh_base mb = I3DCAST_CVISUAL(origin_frm)->GetMesh();
         assert(mb);  //should not pass through GetFace if there is no mesh
                        //get all verticies on LOD and push them to list by tri verticies groups
         const S_matrix &m = origin_frm->GetMatrix();

         const S_vector *pick_verts = (const S_vector*)const_cast<PI3D_mesh_base>(mb)->vertex_buffer.Lock(D3DLOCK_READONLY);
         dword vstride = mb->vertex_buffer.GetSizeOfVertex();

         for(int kk = 0; kk < 3; kk++){
            const S_vector &vx = *(const S_vector*)(((byte*)pick_verts) + tf[kk] * vstride);
            pvertices[kk] = vx * m;
         }
         const_cast<PI3D_mesh_base>(mb)->vertex_buffer.Unlock();
      }
      break;
   case FRAME_VOLUME:
      {
         CPI3D_volume vol = I3DCAST_CVOLUME(origin_frm);
         const S_matrix &vol_matrix = vol->GetGeoMatrix();
         I3D_VOLUMETYPE vol_type = vol->GetVolumeType1();
         switch(vol_type){
         case I3DVOLUME_BOX:
         case I3DVOLUME_RECTANGLE:
            {
               for(int j = 0; j < 3; j++){
                  const S_vector &vt = (vol_type == I3DVOLUME_BOX) ? box_verts[tf[j]] : rect_edges[tf[j]];
                  pvertices[j] = vt*vol_matrix;
               }
            }
            break;
         default: 
            return false; 
         } //sw(vol_type)
      }
      break;
   default:
      assert(0);
      return false;
   }
   return true;
}

//----------------------------

bool S_bsp_triface::ComputeEdges(const S_vector *verts, dword num_v){

   //assert(expanded);
   S_vector face_normal;
   if(vertices_id[0] >= num_v || vertices_id[1] >= num_v || vertices_id[2] >= num_v )
      return false;
   const S_vector *p_tmp_vert[3] = {&verts[vertices_id[0]], &verts[vertices_id[1]], &verts[vertices_id[2]]};

   //face_normal.GetNormal(verticies[0], verticies[1], verticies[2]);
   face_normal.GetNormal(*p_tmp_vert[0], *p_tmp_vert[1], *p_tmp_vert[2]);
   face_normal.Normalize();
   int j0(0);
   for(int i = 3; i--;){
      int j1 = j0;
      j0 = i;
      edge_planes[i] = S_plane(*p_tmp_vert[i], *p_tmp_vert[j1], *p_tmp_vert[j1] + face_normal);
   }
   return true;
}

//----------------------------

bool S_bsp_triface::IsPointInTriangle(const S_vector &pt)const{

                        //check if the point is inside of triangle
   float u = pt.DistanceToPlane(edge_planes[0]);
                              //allow zero or zero-near values to be considered as hit on edge 
   if(!IsMrgZeroLess(u)){
      //L_F_DEBUG("Fast quit 4 - point on plane out of edge 0");
      return false;
   }
   u = pt.DistanceToPlane(edge_planes[1]);
   if(!IsMrgZeroLess(u)){
      //L_F_DEBUG("Fast quit 4 - point on plane out of edge 1");
      return false;
   }
   u = pt.DistanceToPlane(edge_planes[2]);
   if(!IsMrgZeroLess(u)){
      //L_F_DEBUG("Fast quit 4 - point on plane out of edge 2");
      return false;
   }
   return true;
}

//----------------------------
// S_bsp_origin
//----------------------------


bool S_bsp_triface::S_bsp_origin::Setup(BSP_frame_index frame_id1, dword face, const C_bsp_frames &frames){

   const C_bsp_frames::S_frm_inf &fi = frames.GetFrameInfo(frame_id1);

   if(!fi.frm)
      return false;

   face_index = face;
   frm_id = frame_id1;

   if(fi.lod_id == NO_LOD)
      return SetGroupMaterial(fi.frm);
   return true;
}

//----------------------------

bool S_bsp_triface::S_bsp_origin::SetGroupMaterial(CPI3D_frame frame){

   mat = NULL;
   if(frame->GetType1() != FRAME_VISUAL)
      return true; //ok volumes has not material

   CPI3D_mesh_base mb = I3DCAST_CVISUAL(frame)->GetMesh();
   assert(mb);
   if(!mb) 
      return false; //something wrong, we always need geometry info for visuals

   I3D_face_group *fgrps = const_cast<PI3D_mesh_base>(mb)->GetFGroups1();
                              //find triface in meshbase
   dword curr_base_id(0);
   for(dword ii = 0; ii < mb->NumFGroups1(); ii++){
      I3D_face_group &fg2 = fgrps[ii];
      curr_base_id += fg2.NumFaces1();
      if(curr_base_id > face_index){
         mat = fg2.GetMaterial1();
         break;
      }
   }
   return true;
}

//----------------------------
// S_bsp_node
//----------------------------

#pragma pack(push, 1)
struct S_node_header{
   //BSP_index plane_id;
   C_triword plane_id;
   /*
   word face_cnt;
   bool has_front;
   bool has_back;
   */
   word face_cnt_flgs;
   enum{
      FACE_COUNT_MASK = 0x3fff,
      FLAGS_HAS_FRONT = 0x4000,
      FLAGS_HAS_BACK  = 0x8000
   };
};
#pragma pack(pop)

//----------------------------

dword S_bsp_node::BspNodeSize(dword num_faces1){

   return sizeof(S_bsp_node) - sizeof(BSP_index) + sizeof(BSP_index) * (num_faces1);
}

//----------------------------

inline S_bsp_node *S_bsp_node::NextNode(){

   return (S_bsp_node *)(((byte*)this)+Size());
}

//----------------------------

inline const S_bsp_node *S_bsp_node::NextNode() const{

   return (S_bsp_node *)(((byte*)this)+Size());
}

//----------------------------

dword S_bsp_node::GetNumChildren()const{

   dword ret = 0;
   if(front)
      ret += front->GetNumChildren() + 1;
   if(back)
      ret += back->GetNumChildren() + 1;
   return ret;
}

//----------------------------

void S_bsp_node::CollectNodesAndFaces(dword &ret_nodes, dword &ret_faces)const{

   ret_nodes++;
   ret_faces += num_faces;
   if(front)
      front->CollectNodesAndFaces(ret_nodes, ret_faces);
   if(back)
      back->CollectNodesAndFaces(ret_nodes, ret_faces);
}


//----------------------------

void S_bsp_node::GetStatsRec(I3D_stats_bsp &st) const{

   static dword curr_depth = 0;
   curr_depth++;
                              //nodes
   st.num_nodes++;
                              //depth
   st.depth = Max(st.depth, curr_depth);
                              //faces
   st.face_references += NumFaces();
                              //memory
   st.mem_used += Size();
   //st.mem_used += NumFaces() * sizeof(BSP_index);
                              //children
   if(front)
      front->GetStatsRec(st);
   if(back)
      back->GetStatsRec(st);

   curr_depth--;
}


//----------------------------
// C_bsp_node_list
//----------------------------

C_bsp_node_list::C_bsp_node_list() : 
   alloc_size(0),
   used_size(0),
   num_nodes(0),
   offset_pointers(true),
   buf(NULL)
{}

//----------------------------

void C_bsp_node_list::operator =(const C_bsp_node_list &nl){

                              //temporary convert pointers to offsets, and after copying put it back
   const_cast<C_bsp_node_list&>(nl).NodesToOffset();

   alloc_size = used_size = nl.used_size;
   num_nodes = nl.num_nodes;
   delete[] buf;
   buf = alloc_size ? new byte[alloc_size] : NULL;
   memcpy(buf, nl.buf, alloc_size);

   OffsetsToNodes();
   const_cast<C_bsp_node_list&>(nl).OffsetsToNodes();
}

//----------------------------

void C_bsp_node_list::Compact(){

   if(used_size != alloc_size){
      Realloc(used_size);
   }
}

//----------------------------

dword C_bsp_node_list::CalculBufferSize(dword num_nodes1, dword num_faces1){

   return num_nodes1 * S_bsp_node::BspNodeSize(0) + num_faces1 * sizeof(BSP_index);
}

//----------------------------

bool C_bsp_node_list::Realloc(dword size){

   if(alloc_size != size){

      byte *new_buf = (size) ? new byte[size] : NULL;

      if(size && !new_buf)
         return false;
      dword copy_size = sizeof(byte) * Min(used_size, size);
      if(copy_size)
         memcpy(new_buf, buf, copy_size);
      delete buf;
      buf = new_buf;
      alloc_size = size;
      used_size = Min(used_size, size);
   }
   return true;
}

//----------------------------

void C_bsp_node_list::Clear(){

   Realloc(0);
   num_nodes = 0;
   buf = NULL;
                              //whenever we start load nodes or build them, we are in offset mode
   offset_pointers = true;
}


//----------------------------

dword C_bsp_node_list::GetFreeSize() const{

   assert(alloc_size >= used_size);
   return (alloc_size - used_size);
}

//----------------------------

bool C_bsp_node_list::Reserve(dword required_size){

   if(required_size <= alloc_size)
      return true;
   return Realloc(required_size);
}

//----------------------------

byte *C_bsp_node_list::AllocMem(dword size){

   dword required_size = used_size + size;
   if(required_size > alloc_size){
      bool b = Realloc(Max(required_size, alloc_size*2));
      if(!b)
         b = Realloc(required_size); //try exact
      if(!b){
         return NULL;
      }
   }    
   assert(GetFreeSize() >= size);
   byte *free_mem = buf + used_size;
   used_size += size;
   return free_mem;
}

//----------------------------

S_bsp_node *C_bsp_node_list::CreateNode(dword num_faces1){

   dword size = S_bsp_node::BspNodeSize(num_faces1);
   byte *p_unused = AllocMem(size);
   S_bsp_node *new_node = (S_bsp_node *)p_unused;
   if(!new_node)
      return NULL;
   memset(new_node, 0, size);
   new_node->num_faces = num_faces1;
   ++num_nodes;
   return new_node;
}

//----------------------------

bool C_bsp_node_list::NodesToOffset(){

   assert(!offset_pointers);
   if(offset_pointers)
      return false;
   offset_pointers = true;

   S_bsp_node *begin = GetBegin();
   if(!begin)
      return true;
   const S_bsp_node *end = GetEnd();
   for( S_bsp_node *it = begin; it != end; it = it->NextNode()){
      if(it->front)
         it->front_offset = GetOffset(it->front);
      else
         it->front_offset = 0;
      
      if(it->back)
         it->back_offset = GetOffset(it->back);
      else
         it->back_offset = 0;      
   }
   return true;
}

//----------------------------

bool C_bsp_node_list::OffsetsToNodes(){

   assert(offset_pointers);
   if(!offset_pointers)
      return false;
   offset_pointers = false;

   S_bsp_node *begin = GetBegin();
   if(!begin)
      return true;

   const S_bsp_node *end = GetEnd();
   for(S_bsp_node *it = begin; it != end; it = it->NextNode()){

      if(it->front_offset)
         it->front = GetNode(it->front_offset);
      else
         it->front = NULL;
      
      if(it->back_offset)
         it->back = GetNode(it->back_offset);
      else
         it->back = NULL;      
   }
   return true;
}

//----------------------------

struct S_node_list_header{
   dword num_nodes;
   dword num_faces;
};

//----------------------------

I3D_RESULT C_bsp_node_list::SaveNode(C_cache *cp, const S_bsp_node *node)const{

   assert(node);
   S_node_header hdr;

   if(node->plane_id >= 16777216)
      return I3DERR_GENERIC;
   hdr.plane_id.Store(node->plane_id);
   assert(node->num_faces <= S_node_header::FACE_COUNT_MASK);
   assert(node->num_faces < 0xffff);
   hdr.face_cnt_flgs = (word)node->num_faces;
   if(node->front)
      hdr.face_cnt_flgs |= S_node_header::FLAGS_HAS_FRONT;
   if(node->back)
      hdr.face_cnt_flgs |= S_node_header::FLAGS_HAS_BACK;
                              //header
   cp->write(&hdr, sizeof(S_node_header));
                              //array of faces id
   cp->write(node->GetFaces(), node->num_faces*sizeof(BSP_index));
                              //sub tree
   if(node->front){
      I3D_RESULT ir = SaveNode(cp, node->front);
      if(I3D_FAIL(ir))
         return ir;
   }
   if(node->back){
      I3D_RESULT ir = SaveNode(cp, node->back);
      if(I3D_FAIL(ir))
         return ir;
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT C_bsp_node_list::LoadNode(C_cache *cp, dword &this_offset){

   S_node_header hdr;
                              //header
   cp->read(&hdr, sizeof(S_node_header));
   //word numf = (hdr.face_cnt_flgs & S_node_header::FACE_COUNT_MASK);
   word numf = hdr.face_cnt_flgs;
   numf &= S_node_header::FACE_COUNT_MASK;
   {
                              //tmp node, can be reallocated during loading subtree
      S_bsp_node *node = CreateNode(numf);
      if(!node)
         return I3DERR_OUTOFMEM;
      this_offset = GetOffset(node);
                                 //assign values
      node->plane_id = hdr.plane_id;
                                 //array of faces in node
      cp->read(node->facelist, numf*sizeof(BSP_index));
   }

                              //sub tree
   dword front_offset(0);
   dword back_offset(0);
   if(hdr.face_cnt_flgs & S_node_header::FLAGS_HAS_FRONT){
      I3D_RESULT ir = LoadNode(cp, front_offset);
      if(I3D_FAIL(ir))
         return ir;
   }
   if(hdr.face_cnt_flgs & S_node_header::FLAGS_HAS_BACK){
      I3D_RESULT ir = LoadNode(cp, back_offset);
      if(I3D_FAIL(ir))
         return ir;
   }
                              //add reference to chidren
   {
      S_bsp_node *node = GetNode(this_offset);
      if(!node)
         return I3DERR_GENERIC;
      node->front_offset = front_offset;
      node->back_offset = back_offset;
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT C_bsp_node_list::Save(C_cache *cp) const{

   S_node_list_header hdr_nodes;
   hdr_nodes.num_nodes = 0;
   hdr_nodes.num_faces = 0;

   const S_bsp_node *root = GetRoot();
   if(root){
      assert(!offset_pointers);
      if(offset_pointers)
         return I3DERR_GENERIC;
      root->CollectNodesAndFaces(hdr_nodes.num_nodes, hdr_nodes.num_faces);
      assert(hdr_nodes.num_nodes == num_nodes);
   }
                              //header for nodes list
   cp->write(&hdr_nodes, sizeof(S_node_list_header));

   I3D_RESULT ir = I3D_OK;
   if(root)
      ir = SaveNode(cp, root);
   return ir;
}

//----------------------------

I3D_RESULT C_bsp_node_list::Load(C_cache *cp){

   S_node_list_header hdr_nodes;
   cp->read(&hdr_nodes, sizeof(S_node_list_header));
   dword buf_size = CalculBufferSize(hdr_nodes.num_nodes, hdr_nodes.num_faces);
   Clear();
   Reserve(buf_size);
   if(hdr_nodes.num_nodes){
      dword root_offset;
      assert(offset_pointers);
      I3D_RESULT ir = LoadNode(cp, root_offset);
      if(I3D_FAIL(ir))
         return ir;
      assert(root_offset == 0);
   }
   if(!OffsetsToNodes())
      return I3DERR_GENERIC;

   return I3D_OK;
}


//----------------------------
// C_bsp_tree
//----------------------------

#pragma pack(push, 1)
struct S_bsp_triface_saver{
                              //plane ID must fit into 24 bits (16.8 mil possibilities)
   C_triword plane_id;
                              //the same limitation for frame id
   C_triword frame_id;
                              //the same for face index
   C_triword face_index;
                              //and the same for vertices
   C_triword vertices_index[3];

   byte slide_side_flags;     //lowest 3 bits used for edge flags, highest one used for specifying side

   inline void CopyFromTriface(const S_bsp_triface &tf){

      plane_id.Store(tf.plane_id);

      frame_id.Store(tf.origin.frm_id);

      for(int i = 3; i--; )
         vertices_index[i].Store(tf.vertices_id[i]);

      dword findx = tf.origin.GetFaceIndex();
      face_index.Store(findx);

      slide_side_flags = tf.slide_flags;
      /*
                              //encode plane side into highest bit of slide flags
      assert(!(slide_side_flags&0x80));
      if(tf.plane_side)
         slide_side_flags |= 0x80;
         */
      slide_side_flags |= 0x80;  //remove when BSP version != 15
   }

   inline bool CopyToTriface(S_bsp_triface &tf, const C_bsp_frames &frames, I3D_LOAD_CB_PROC *cbP, void *context) const{

                              //face plane
      tf.plane_id = plane_id;
                              //plane_side (for now, only report if not true) - remove when BSP version != 15
      //tf.plane_side = (slide_side_flags&0x80);
      if(!(slide_side_flags&0x80) && (BSP_VERSION == 15)){
         (*cbP)(CBM_ERROR, (dword)"bsp: old version, rebuild", 0, context);
         return false;
      }

                              //frames info
      if(frame_id >= frames.Size()){
         if(cbP)
            (*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: missing frame info, id %i.", frame_id), 0, context);
         //return I3DERR_OBJECTNOTFOUND;
         return false;
      }
                              //origin face
      if(!tf.origin.Setup(frame_id, face_index, frames)){
         if(cbP){
            CPI3D_frame frm1 = frames.GetFrame(frame_id);
            (*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: origin.Setup failed, frame %i ('%s').",
               frame_id, (const char*)frm1->GetName1()), 0, context);
         }
         //return I3DERR_FILECORRUPTED;
         return false;
      }

      for(int i = 3; i--; )
         tf.vertices_id[i] = vertices_index[i];

                              //edges slide flags
      //tf.slide_flags = slide_side_flags&SLIDE_EDGE_ALL;
      tf.slide_flags = slide_side_flags;
      tf.slide_flags &= SLIDE_EDGE_ALL;

      return true;
   }

};
#pragma pack(pop)

//----------------------------

I3D_RESULT C_bsp_tree::Save(C_cache *cp) const{

                              //header
   S_bsp_header hdr;
   hdr.version = BSP_VERSION;
   hdr.node_count = IsValid() ? GetNumNodes() : -1;
   hdr.num_planes = planes.size();
   hdr.num_faces = faces.size();
   hdr.num_vertices = vertices.size();

   cp->write(&hdr, sizeof(S_bsp_header));

   if(!IsValid())             //invalid tree, save only header
      return I3DERR_GENERIC;
                              //save names of all used frames as table, first name has id 0
   frames.Save(cp);
                              //planes array
   cp->write(&planes.front(), hdr.num_planes * sizeof(S_plane));

                              //vertices array for faces
   cp->write(&vertices.front(), hdr.num_vertices * sizeof(S_vector));

                              //face array
   const S_bsp_triface *p_faces = &faces.front();
   C_vector<S_bsp_triface_saver> tmp_faces;
   tmp_faces.resize(hdr.num_faces);
                              //prepare faces for saving
   for(dword j = 0; j < hdr.num_faces; j++){
      const S_bsp_triface &tf = p_faces[j];
      S_bsp_triface_saver &tfs = tmp_faces[j];
      tfs.CopyFromTriface(tf);
   }
                              //write all trifaces
   cp->write(&tmp_faces.front(), hdr.num_faces*sizeof(S_bsp_triface_saver));

                              //tree structure
   I3D_RESULT ir;
   if(!nodes.GetRoot()){
      ir = I3D_OK;
   }else{
      ir = nodes.Save(cp);
   }
   return ir;
}

//----------------------------

I3D_RESULT C_bsp_tree::Load(PI3D_scene scene, C_cache *cp, I3D_LOAD_CB_PROC *cbP, void *context, dword check_flags){

                              //header
   S_bsp_header hdr;

   cp->read(&hdr, sizeof(S_bsp_header));

                              //check version
   if(hdr.version != BSP_VERSION){
      if(cbP)
         (*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: different version detected (%i, current %i), please rebuild tree.", hdr.version, BSP_VERSION), 0, context);
      return I3DERR_FILECORRUPTED;
   }
                              //invalid tree saved(should not happen)
   if(hdr.node_count == -1){
      return I3DERR_FILECORRUPTED;
   }

                              //bsp frames
   bool frames_ok = frames.Load(cp, scene, check_flags&FBSPLOAD_CHECKSUM, cbP, context);
   if(!frames_ok){
                              //don't bother with this message, reason of fail is alredy said in frames.Load().
      //if(cbP)
      //   (*cbP)(CBM_ERROR, (dword)"Bsp: failed to load bsp frames.", 0, context);
      return I3DERR_OBJECTNOTFOUND;
   }

                              //planes array
   assert(!planes.size());
   planes.resize(hdr.num_planes);
   cp->read(&planes.front(), hdr.num_planes*sizeof(S_plane));

                              //vertices array for faces
   assert(!vertices.size());
   vertices.resize(hdr.num_vertices);
   cp->read(&vertices.front(), hdr.num_vertices * sizeof(S_vector));


                              //triface save struct
   C_vector<S_bsp_triface_saver> trif_saves;
   trif_saves.resize(hdr.num_faces);
   //trif_saves.resize
   cp->read(&trif_saves.front(), hdr.num_faces*sizeof(S_bsp_triface_saver));

                              //face array
   faces.resize(hdr.num_faces);
   for(dword j = 0; j < hdr.num_faces; j++){
                              //copy from saving structure to triface
      S_bsp_triface &tf = faces[j];
      const S_bsp_triface_saver &tfs = trif_saves[j];
      tfs.CopyToTriface(tf, frames, cbP, context);
                              //compute necessery run time info about face
      bool b1 = tf.ComputeEdges(&vertices.front(), vertices.size());
      if(!b1){
         if(cbP){
            CPI3D_frame frm1 = frames.GetFrame(tfs.frame_id);
            //(*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: origin.Expand failed, frame %s.",
            (*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: ComputeEdges failed, frame %s.",
               (const char*)frm1->GetName1()), 0, context);
         }
         return I3DERR_OBJECTNOTFOUND;
      }
   }

                              //now all nodes
   I3D_RESULT ir;
   if(!hdr.node_count){
      ir = I3D_OK;
   }else{
      ir = nodes.Load(cp);
   }

   if(I3D_FAIL(ir)){
      return ir;
   }

                        //check consistency(slowdown loading, don't set flag in final game)
   if(cbP && check_flags&FBSPLOAD_CONSISTENCE){
      ReportConsistency(scene, cbP, context);
   }

   valid = true;

   return I3D_OK;
}


//----------------------------

void C_bsp_tree::Clear(){

   valid = false;
   planes.clear();
   faces.clear();
   vertices.clear();
   nodes.Clear();
   frames.Clear();
}

//----------------------------
// input:   plane index, ray interval
// output:  ray_origin near/far side(FRONT/BACK)
// Return signed distance along ray to plane.

float C_bsp_tree::ClassifyRayInterval(BSP_index plane_indx, float ray_min, float ray_max, E_PLANE_SIDE &near_side, E_PLANE_SIDE &far_side, const I3D_collision_data &cd) const{

   float signed_dist;         //ray-plane intersection distance.

   assert(plane_indx < planes.size());
   const S_plane &partion_plane = planes[plane_indx];
                              //coplanar test
   float costh = partion_plane.normal.Dot(cd.GetNormalizedDir());
   bool ray_is_parallel = IsAbsMrgZero(costh);
   if(ray_is_parallel){
      signed_dist = -1e+16f;     //we need zero less value for future process
   }else{
      signed_dist =  -(partion_plane.normal.Dot(cd.from) + partion_plane.d) / costh;
   }

                              //determinate start point space against partion plane
   float point_side = cd.from.DistanceToPlane(partion_plane);
                              //store tree of half-space which contain ray_start
   if(point_side < -MRG_ZERO){
      near_side = PS_BACK;
      far_side = PS_FRONT;
   }else
   if(point_side > MRG_ZERO){
      near_side = PS_FRONT;
      far_side = PS_BACK;
   }else{
      near_side = PS_ON;
      if(!ray_is_parallel){
                              //angle between ray direction and partion_plane normal determinate 
                              //side where is any ohter point then ray_origin
          far_side = (- costh > MRG_ZERO) ? PS_FRONT : PS_BACK;
      }else{
                              //ray lay on plane and is parallel, select any one side
         far_side = PS_FRONT;
      }
   }       
   return signed_dist;
}

//----------------------------

bool C_bsp_tree::FaceListItersection(const S_bsp_node &node, const float part_dist, I3D_collision_data &cd) const{

                              //we use one point for test, because faces are coincident with partion plane
                              //but, there is some error bounded by BSP_SPLIT_TOLERANCE
   const S_vector partion_intersection_point = cd.from + cd.GetNormalizedDir() * part_dist;

                              //proces all polygons
   const BSP_index *p_flist = node.GetFaces();
   const S_bsp_triface *p_faces = &faces.front();
   const S_plane *p_planes = &planes.front();
   for(int i = node.NumFaces(); i--;)
   {

#ifdef VOLUME_STATISTICS
      //++scene->col_stats[I3D_scene::COL_STATS_L_F];
#endif
      BSP_index curr_face_id = p_flist[i];
      const S_bsp_triface &tf = p_faces[curr_face_id];

      CPI3D_frame origin_frm = frames.GetFrame(tf.origin.frm_id);
      assert(origin_frm);

      if(origin_frm->GetFrameFlags()&I3D_FRMF_NOCOLLISION)
         continue;

                              //check if collide bits match
      if(origin_frm->GetType1() == FRAME_VOLUME){
         if(!(I3DCAST_CVOLUME(origin_frm)->GetCollideCategoryBits()&cd.collide_bits))
            continue;
      }

//      if(!(cd.flags&I3DCOL_INVISIBLE) && !tf.origin.frame->IsOn1())
//         continue;

      //bool invert_side = !tf.plane_side;
      bool invert_side = false;

                              //back face culling
      {
                              //lazy, can be improved (we alredy computed required information before)
         bool two_side_mat = false;
         assert(tf.plane_id < planes.size());
         const S_plane &f_plane = p_planes[tf.plane_id];
         if(origin_frm->GetType1() == FRAME_VOLUME){
            if(I3DCAST_CVOLUME(origin_frm)->GetVolumeType1() == I3DVOLUME_RECTANGLE)
               two_side_mat = true;
         }else
         {
            PI3D_material mat = tf.origin.GetMaterial();
            if(mat)
               two_side_mat = (mat->Is2Sided1() || (cd.flags&I3DCOL_FORCE2SIDE));
         }

         float f_side = cd.from.DistanceToPlane(f_plane);
         bool back_side = CHECK_ZERO_LESS(f_side);
         //if((back_side && tf.plane_side) || (!back_side && !tf.plane_side))
         if(back_side){
            if(!two_side_mat){
                                 //back face culling
               continue;
            }
            invert_side = true;//!invert_side;
         }
      }

      const S_vector &intersection_point = partion_intersection_point;
      float dist = part_dist;

      bool b = tf.IsPointInTriangle(intersection_point);

      if(b){
                              //dont' test invisible frames
         if(!(cd.flags&I3DCOL_INVISIBLE) && IsHidden(origin_frm))
            continue;

         if(cd.frm_ignore && IsChildOf(origin_frm, cd.frm_ignore))
            continue;
                              //check texture transparent pixels, if requested
         if(cd.flags&I3DCOL_COLORKEY){
            if(!TextureIntersection(tf, dist, cd)){
               continue;
            }
         }
                              //return normal of hit face, not partion plane, altrhough they should be near same
         assert(tf.plane_id < planes.size());
         const S_plane face_plane = p_planes[tf.plane_id];

                              //check if we want this
         if(cd.callback){
                              //setup values for callback
            S_vector hit_norm = face_plane.normal;
            if(invert_side)
               hit_norm.Invert();
            I3D_cresp_data rd(const_cast<PI3D_frame>(origin_frm), hit_norm, dist, cd.cresp_context);
            rd.CopyInputValues(cd);
            rd.face_index = tf.origin.GetFaceIndex();
            if(!cd.callback(rd))
               return false;
         }
                              //setup return values
         cd.SetReturnValues(dist, const_cast<PI3D_frame>(origin_frm), invert_side ? -face_plane.normal : face_plane.normal);
         cd.face_index = tf.origin.GetFaceIndex();
         tf.highlight = true;
         return true;
      }
   }
   return false;
}

//----------------------------

bool C_bsp_tree::TextureIntersection(const S_bsp_triface &tf, const float hit_dist, I3D_collision_data &cd) const{

   bool rtn = true;
#if 0                         //MB: fix for new system of I3D_mesh_base::GetFaces
   bool invert_side = false;

   CPI3D_frame origin_frm = frames.GetFrame(tf.origin.frm_id);
   assert(origin_frm);
   switch(origin_frm->GetType1()){
   case FRAME_VISUAL:
      break;
   case FRAME_VOLUME:
      return true;            //no texture check on volumes
   default: 
      assert(0);
   }
   assert(origin_frm->GetType1() == FRAME_VISUAL);
   CPI3D_visual vis = I3DCAST_CVISUAL(origin_frm);
   assert(vis);
   CPI3D_mesh_base mb = vis->GetMesh();
   if(!mb)
      return true;            //no mesh, skip test a considere it as sucess

   if(!tf.origin.GetMaterial())
      return true;

   CPI3D_material mat = tf.origin.GetMaterial();

                              //detailed texture check
   CPI3D_texture_base tb = mat->GetTexture1();
   if(tb && (mat->I3D_material::IsCkey() || mat->IsCkeyAlpha1() || cd.texel_callback)){
      I3D_texel_collision tc;
      tc.loc_pos = false;

      tc.mat = mat;
      //assert(tc.mat);

                                 //verticies, face groups, faces, texture coord
      const S_vector *pick_verts = (const S_vector*)const_cast<PI3D_mesh_base>(mb)->vertex_buffer.Lock(D3DLOCK_READONLY);
      dword vstride = mb->vertex_buffer.GetSizeOfVertex();
                                 //assumption: texture coordinates are at end of vertex
      const I3D_text_coor *txt_coords = (const I3D_text_coor*)(((byte*)pick_verts) + vstride - sizeof(I3D_text_coor));

      PI3D_triface trifaces = const_cast<PI3D_mesh_base>(mb)->index_buffer.Lock(D3DLOCK_READONLY);

      bool two_side_mat;
      two_side_mat = (tc.mat->Is2Sided1() || (cd.flags&I3DCOL_FORCE2SIDE));
                                 //find triface in meshbase
      const I3D_triface &fc = trifaces[tf.origin.GetFaceIndex()];

      tc.v[0] = (const S_vector*)(((byte*)pick_verts) + fc[0] * vstride);
      tc.v[1] = (const S_vector*)(((byte*)pick_verts) + fc[1] * vstride);
      tc.v[2] = (const S_vector*)(((byte*)pick_verts) + fc[2] * vstride);

//----------------------------
      const S_matrix &matrix = vis->GetMatrix();
      const S_matrix &m_inv = vis->GetInvMatrix1();

                                 //inverse-map vectors to local coords
      S_vector loc_from = cd.from * m_inv;
      S_vector loc_to = (cd.from + cd.GetNormalizedDir() * hit_dist) * m_inv;
      S_vector loc_dir_normalized = loc_to - loc_from;
      float loc_hit_dist = loc_dir_normalized.Magnitude();
      if(IsMrgZeroLess(loc_hit_dist))
         return false;
      loc_dir_normalized /= loc_hit_dist;

                              //intersection point in local coord
      assert(tf.plane_id < planes.size());
//----------------------------
                              //compute plane in local coord
      S_plane face_plane;
      face_plane.normal.GetNormal(*tc.v[0], *tc.v[1], *tc.v[2]);
      face_plane.normal.Normalize();
      face_plane.d = -(*tc.v[0]).Dot(face_plane.normal);

//----------------------------//computation from world plane
//      S_plane face_plane2 = planes[tf.plane_id];
//      if(!tf.plane_side)
//         face_plane2.Invert();
//      face_plane2 = face_plane2 * m_inv;
//----------------------------

      S_vector f_normal = face_plane.normal;
      
                              //intersection point in local coord
      float f_dist = loc_from.DistanceToPlane(face_plane);
      if(CHECK_ZERO_LESS(f_dist)){
         assert(two_side_mat); //backface culling performed in ray-face test, can' be single side material test
         invert_side = true; 
      }
      float u = f_normal.Dot(loc_dir_normalized);
      assert(!IsAbsMrgZero(u)); //parrallel tast performed in ray-face test, can't be parallel
      float curr_dist = - f_dist / u;
      tc.point_on_plane = loc_from + loc_dir_normalized * curr_dist;
                              //computation from world point
      //S_vector point_on_plane2 = (*ipt) * m_inv; 

//----------------------------
      const I3D_text_coor &uv0 = *(const I3D_text_coor*)(((byte*)txt_coords) + fc[0] * vstride);
      const I3D_text_coor &uv1 = *(const I3D_text_coor*)(((byte*)txt_coords) + fc[1] * vstride);
      const I3D_text_coor &uv2 = *(const I3D_text_coor*)(((byte*)txt_coords) + fc[2] * vstride);
      if(tc.ComputeUVFromPoint(uv0, uv1, uv2)){

         if(tc.mat->I3D_material::IsCkey() || tc.mat->IsCkeyAlpha1()){
                        //look on texel
            const byte *rle = tb->GetRLEMask();
            assert(rle);
            dword sx = tb->SizeX1(), sy = tb->SizeY1();
            int x = Min((int)(sx * tc.tex.x), (int)sx-1);
            int y = Min((int)(sy * tc.tex.y), (int)sy-1);
            const byte *mem = &rle[*(dword*)(&rle[y*4])];
            bool full = false;
            do{
               byte b = *mem++;
               if(!b){     //eol
                  full = false;
                  break;
               }
               full = (b&0x80);
               x -= (b&0x7f);
            }while(x>=0);
            if(!full)
               rtn = false;
               goto before_return;
         }
                                    //ask user if we furher process this
         if(cd.texel_callback){
            tc.face_index = tf.origin.GetFaceIndex();
                                 //due to this, we need to put hit data
                                 // into world coords,
                                 // performance is not critical, because this
                                 // feature is expected to be called by light-mapping only
            S_vector hit_normal = f_normal.RotateByMatrix(matrix);
            if(invert_side)
               hit_normal.Invert();
            S_vector col_pt = (loc_from + loc_dir_normalized * curr_dist) * matrix;
            float hit_dist = (col_pt - cd.from).Magnitude();

            I3D_cresp_data rd(const_cast<PI3D_frame>(origin_frm), hit_normal, hit_dist, cd.cresp_context);
            rd.CopyInputValues(cd);
            rd.texel_data = &tc;
            bool texel_accept = cd.texel_callback(rd);
            if(!texel_accept){
               rtn = false;
               goto before_return;
            }
         }
      }
before_return:
      const_cast<PI3D_mesh_base>(mb)->vertex_buffer.Unlock();
      mb->index_buffer.Unlock();
   }
#endif
   return rtn;
}

//----------------------------

bool C_bsp_tree::NodeIntersection(const S_bsp_node &node, float ray_min, float ray_max, I3D_collision_data &cd) const{

   const S_bsp_node *tree_near = NULL; //child of Node for half-space containing the origin of Ray
   const S_bsp_node *tree_far = NULL;  //the "other" child of Node -- i.e. not equal to near

                              //signed distance along Ray to plane defined by Node
   E_PLANE_SIDE near_side, far_side;
   float dist = ClassifyRayInterval(node.plane_id, ray_min, ray_max, near_side, far_side, cd);
   assert(near_side != far_side); //debug
   //tree_near = (near_side == PS_FRONT) ? node.front : (near_side == PS_BACK) ? node.back : NULL;
   tree_near = node.GetChildBySide(near_side);
   //tree_far = (far_side == PS_FRONT) ? node.front : (far_side == PS_BACK) ? node.back : NULL;
   tree_far = node.GetChildBySide(far_side);

   if(CHECK_ZERO_LESS(dist) || (dist > ray_max+BSP_SPLIT_TOLERANCE)){
                              //whole interval is on near side
      if(tree_near){
         bool hit = NodeIntersection(*tree_near, ray_min, ray_max, cd);
         return hit;
      }
   }else
   if(dist < ray_min){
                              //whole interval is on far side
      if(tree_far){
         bool hit = NodeIntersection(*tree_far, ray_min, ray_max, cd);
         return hit;
      }
   }else{
                              //the interval intersects the plane
      if(tree_near){          //Test near side
         bool hit = NodeIntersection(*tree_near, ray_min, dist, cd);
         if(hit)                   // if there was a hit return pick_data
            return hit;
      }
                           //test faces in node (on partion plane)
      if(FaceListItersection(node, dist, cd))
         return true;
                              //Test far side
      if(tree_far){
         bool hit =  NodeIntersection(*tree_far, dist, ray_max, cd);
         return hit;
      }
   }
   return false;
}

//----------------------------

bool C_bsp_tree::RayIntersection(I3D_collision_data &cd) const{

   if(!nodes.GetRoot())
      return false;

   return NodeIntersection(*nodes.GetRoot(), .0f, cd.GetHitDistance(), cd);
}

//----------------------------
// SPHERE_SHIFTING
//----------------------------
// Classify sphere move against partion plane. th contain move information,
// in_dist, out_dist return distances on move line where sphere touch the plane, 
// near_side inform where origin of move lie. (far side is the other)
// return TRUE if move direction facing to plane
bool C_bsp_tree::Classify_MS(BSP_index plane_indx, const S_trace_help &th, float &hit_distance, float &out_dist, E_PLANE_SIDE &near_side) const{

   assert(plane_indx < planes.size());
   const S_plane &plane = planes[plane_indx];

                              //distance of move origin to partion plane
   float orig_dist = th.from.DistanceToPlane(plane);
                              //facing of move dir and plane normal
   float cosh1 = plane.normal.Dot(th.dir_norm);

   if(orig_dist < -MRG_ZERO){
      near_side = PS_BACK;
   }
   else
   if(orig_dist > MRG_ZERO){
      near_side = PS_FRONT;
   }
   else{
      if(CHECK_ZERO_LESS(cosh1))
         near_side = PS_FRONT;
      else
         near_side = PS_BACK;
   }

                              //classify move
   if(IsAbsMrgZero(cosh1)){
                           //move is parallel to plane, check distance
      if(I3DFabs(orig_dist) >= th.radius){
                           //never touch the plane
         hit_distance = .0f;
         out_dist = .0f;
      }
      else{
         out_dist = -1.e+8f;
      }
   }
   else{
                           //shift plane by radius
      //float f = plane.normal.Dot(from1) + plane.d;
      const float &f = orig_dist;
      float min_d = -(f + th.radius) / cosh1;
      float max_d =  -(f - th.radius) / cosh1;
      if(min_d > max_d){
         swap(min_d, max_d);
      }
      hit_distance = min_d;
      out_dist = max_d;
   }

   return CHECK_ZERO_LESS(cosh1);
}

//----------------------------

bool C_bsp_tree::FaceListItersection_MS(const S_bsp_node &node, float near_dist, float far_dist, const S_trace_help &th)const{

   bool ret = false;
   const BSP_index *pflist = node.GetFaces();
   const S_bsp_triface *pfaces = &faces.front();
                              //FIXME: from and to are tested as entire original
                              //segmet due risc that zero length dir could accure.
                              //this require recomputing sphere/plane move line clip
                              //which was actually computed in tree traversal,so
                              //can we handle it better?
                              //NOTE:while we re-testing original segment in CheckCol_MS_F
                              //we get as output valid in/out distaces, 
                              //do not add them to near_dist!
   for(int i = node.NumFaces(); i--; ){

#ifdef VOLUME_STATISTICS
      //++scene->col_stats[I3D_scene::COL_STATS_S_F];
#endif

      BSP_index curr_face_id = pflist[i];
      const S_bsp_triface &tf = pfaces[curr_face_id];
                              //todo: pass info from header to CheckCol_MS_F, avoid recomputation.
//PR_BEG;
      bool b = CheckCol_MS_F(tf, th, curr_face_id);
//PR_END;
      if(b){
         tf.highlight = true;
      }
      ret |= b;
   }
   return ret;
}

//----------------------------
// Same as ClipLineByPlane but used precomputed values plane_dist and cosh1. Used in tree traverse.
// cosh1 = pl_norm.Dot(dir);
// plane_dist = pos_dot + pl_d;
inline E_CLIP_LINE ClipLineByPlaneCompact(float plane_dist, float cosh1, float &dmin, float &dmax){

#define EDGE_TOLERANCE .001f

   if(IsAbsMrgZero(cosh1)){
                              //line is parallel to plane
      if(CHECK_ZERO_LESS(plane_dist)){
                              //origin inside, entire line inside, no clipping 
         return CL_INSIDE_NOCLIP;
      }else{
                              //origin outside, entire line invalid
         return CL_OUTSIDE;
      }
   }else{
      float dist = -(plane_dist) / cosh1;

      if(CHECK_ZERO_LESS(dist)){
                              //clip plane before origin, entire line behind plane
         if(CHECK_ZERO_LESS(plane_dist)){
                              //origin inside, segment valid, no clipping
            return CL_INSIDE_NOCLIP;
         }else{
                              //origin outside, entire line invalid 
            return CL_OUTSIDE;
         }
      }else
      if(dist < dmin){
                              //clip plane between origin and min
         if(CHECK_ZERO_LESS(plane_dist)){
                              //origin inside, entire segment outside, invalid
            return CL_OUTSIDE;
         }else{
                              //origin outside, entire segment valid
            return CL_INSIDE_NOCLIP;
         }
      }else
      if(dist < dmax){
                              //clip plane between min and max
         if(CHECK_ZERO_LESS(plane_dist)){
                              //origin and min is inside, clip max
            dmax = dist;
            return CL_INSIDE_CLIP_MAX;
         }else{
                              //origin and min is outside, clip min
            dmin = dist;
            return CL_INSIDE_CLIP_MIN;
         }
      }else{
                              //clip plane after max
         if(CHECK_ZERO_LESS(plane_dist)){
                              //origin inside, entire line valid, no clipping
            return CL_INSIDE_NOCLIP;
         }else{
                              //origin outside, entire line invalid
            return CL_OUTSIDE;
         }
      }
   }
}

//----------------------------
// Return segment of line clipped by plane, ie part which is before plane.
// Line is defined by pos, dir and tested segment in which are we interested in(dmin, dmax).
// Plane is defined by pl_normal and pl_d.
// If line is clipped, dmin and dmax are adjusted by clipping.
// (ie dmin and dmax remain untouched when CL_OUTSIDE is returned)
inline E_CLIP_LINE ClipLineByPlane(const S_vector &pos, const S_vector &dir, const S_vector &pl_norm, float pl_d, float &dmin, float &dmax){

   float cosh1 = pl_norm.Dot(dir);
   float plane_dist = pl_norm.Dot(pos) + pl_d;

   return ClipLineByPlaneCompact(plane_dist, cosh1, dmin, dmax);
}


//----------------------------
//----------------------------
// cosh1 = pl_norm.Dot(dir);
// plane_dist = pos_dot + pl_d;
bool C_bsp_tree::NodeIntersection_MS(const S_bsp_node &node, float ray_min, float ray_max, const S_trace_help &th)const{

   bool any_hit = false;
   const S_plane &plane = planes[node.plane_id];

   const float plane2dir_dot = plane.normal.Dot(th.dir_norm);
   const float pos_dot = plane.normal.Dot(th.from);

   if(plane2dir_dot > MRG_ZERO){
      float shift_up_d = plane.d - th.radius;
      float dmin1 = ray_min;
      float dmax1 = ray_max;
      E_CLIP_LINE cl1 = ClipLineByPlaneCompact(pos_dot + shift_up_d, plane2dir_dot, dmin1, dmax1);

      bool part_under = (cl1 != CL_OUTSIDE);
      if(part_under){
         if(node.back){
            bool b = NodeIntersection_MS(*node.back, dmin1, dmax1, th);
            any_hit |= b;
         }
      }
      float dmin2 = ray_min;
      float dmax2 = ray_max;
      float shift_down_d = -(plane.d + th.radius);
      E_CLIP_LINE cl2 = ClipLineByPlaneCompact(-pos_dot + shift_down_d, -plane2dir_dot, dmin2, dmax2);
      bool part_over = (cl2 != CL_OUTSIDE);
      if(part_over){
         if(node.front){
            bool b = NodeIntersection_MS(*node.front, dmin2, dmax2, th);
            any_hit |= b;
         }
      }

      if(part_under && part_over){
         bool b = FaceListItersection_MS(node, ray_min, ray_max, th);
         any_hit |= b;
      }
   }else{
      float shift_up_d = -plane.d - th.radius;
      float dmin1 = ray_min;
      float dmax1 = ray_max;
      E_CLIP_LINE cl1 = ClipLineByPlaneCompact(-pos_dot + shift_up_d, -plane2dir_dot, dmin1, dmax1);
      bool part_under = (cl1 != CL_OUTSIDE);
      if(part_under){
         if(node.front){
            bool b = NodeIntersection_MS(*node.front, dmin1, dmax1, th);
            any_hit |= b;
         }
      }
      float dmin2 = ray_min;
      float dmax2 = ray_max;
      float shift_down_d = plane.d - th.radius;
      E_CLIP_LINE cl2 = ClipLineByPlaneCompact(pos_dot + shift_down_d, plane2dir_dot, dmin2, dmax2);
      bool part_over = (cl2 != CL_OUTSIDE);
      if(part_over){
         if(node.back){
            bool b = NodeIntersection_MS(*node.back, dmin2, dmax2, th);
            any_hit |= b;
         }
      }
      if(part_under && part_over){
         bool b = FaceListItersection_MS(node, ray_min, ray_max, th);
         any_hit |= b;
      }
   }
   return any_hit;
}

//----------------------------

inline bool MakeBevelPlane(const S_vector &norm1, const S_vector &norm2, const S_vector &ip, float radius, S_plane &bevel){

                              //bevel is neaded when planes intersects in angel greater than 90 deg
   float f = norm1.Dot(norm2);
   //if(CHECK_ZERO_LESS(f))
                              //put some tolerance here because we use this function also for edge beveling, which are above 90deg.
   if(f < .001f){
                              //bevel normal is half-way between intersections planes normals
      bevel.normal = norm1+norm2;
      bevel.normal.Normalize();
                              //distance from ip to bevel plane must be equal to radius, so bevel's 'd' is determinated by DistanceToPlane formula
                              //radius == ip.Dot(pl.normal) + pl.d; planes facing outside of triangel, so distance to plane is negative.
      bevel.d = -radius - ip.Dot(bevel.normal);
      return true;
   }
   else{
      return false;
   }
}

//----------------------------

byte C_bsp_tree::GetSlideNormal(const S_bsp_triface &tf, const S_plane &pl, const S_vector &ip, S_vector &rn) const{

   byte clip_flags = 0;
                              //calculate closest point on face
   float dist = ip.DistanceToPlane(pl);
   S_vector point_on_plane = ip - pl.normal*dist;
                              //check point against edges
   bool out_edge[3] = {false, false, false};
   int num_clips = 0;
   int clip_index[2];
   for(int i = 3; i--; ){
      if(tf.slide_flags&(1<<i)){
         float d = point_on_plane.DistanceToPlane(tf.edge_planes[i]);
         if(d > 0.0f){
            out_edge[i] = true;
            clip_flags |= (1<<i);
            assert(num_clips < 2);
            clip_index[num_clips] = i;
            num_clips++;
         }
      }
   }
   S_vector closest_point;
   if(clip_flags){
      assert(num_clips);
      if(num_clips == 1){
                              //recompute closest point - on the edge
         int cp_indx = clip_index[0];
         //S_vector e_dir = -tf._edges[cp_indx];

         const S_vector &e_point = vertices[tf.vertices_id[cp_indx]];
         const S_vector &e_point1 = vertices[tf.vertices_id[next_tri_indx[cp_indx]]];

         //assert((e_dir - (e_point1 - e_point)).IsNull());
         S_vector e_dir = e_point1 - e_point;

         float d = ip.PositionOnLine(e_point, e_dir);
         d = Max(.0f, Min(d, 1.0f));
         closest_point = e_point + e_dir * d;
      }else{
         int cp_indx;
         if(abs(clip_index[0]-clip_index[1])==1)   //they're (0 and 1) or (1 and 2) - use point of greater
            cp_indx = Max(clip_index[0], clip_index[1]);
         else        //they're 0 and 2
            cp_indx = 0;
         closest_point = vertices[tf.vertices_id[cp_indx]];
      }
      rn = S_normal(ip-closest_point);
   }else
      rn = pl.normal;
   return clip_flags;
}

//----------------------------
// Face is id of currently tested triface(S_bsp_triface doesn't contain this information yet)
bool C_bsp_tree::CheckCol_MS_F(const S_bsp_triface &tf, const S_trace_help &th, BSP_index face_id) const{

#if 0     //debug
   /*int debug_face = DEBUG_TRIFACE_ID;
   if(face_id != debug_face)
      return false;
                              //check if face points lie on face plane
   {
      const S_plane &face_plane = planes[tf.plane_id];
      for(int i0 = 3; i0--; ){
         float f = tf.verticies[i0].DistanceToPlane(face_plane);
         if(f > .000001){
            //assert(0);
         }
      }
   }*/
   //if(tf.origin.GetFaceIndex() != 0)
   //   return false;
#endif

   CPI3D_frame origin_frm = frames.GetFrame(tf.origin.frm_id);
   assert(origin_frm);
   if(!origin_frm)
      return false;

   if(origin_frm->GetFrameFlags()&I3D_FRMF_NOCOLLISION)
      return false;
                              //dont' test invisible frames
   if(!(th.flags&I3DCOL_INVISIBLE) && IsHidden(origin_frm))
      return false;

                              //check if collide bits match
   if(origin_frm->GetType1() == FRAME_VOLUME){
      if(!(I3DCAST_CVOLUME(origin_frm)->GetCollideCategoryBits()&th.collide_bits))
         return false;
   }

//   if(!(th.flags&I3DCOL_INVISIBLE) && !tf.origin.frame->IsOn())
//      return false;

   bool two_sided = false;

   float hit_distance = .0f;

   if(IsAbsMrgZero(th.dir_magn))
      return false;
   const S_vector &from1 = th.from;
   const S_vector &dir1 = th.dir_norm;
   float out_dist = th.dir_magn;
   
                              //clip segment by face plane, shifted by radius
   S_plane face_plane = planes[tf.plane_id];
   //if(!tf.plane_side) face_plane.Invert();

                              //check side, make it facing to from point
   {
      float ff = from1.Dot(face_plane.normal) + face_plane.d;
      bool other_side = CHECK_ZERO_LESS(ff);
      if(other_side){
         if(th.flags&I3DCOL_FORCE2SIDE){
            two_sided = true;
         }else{
                              //make it two sided for rectangle volume
            if(origin_frm->GetType1() == FRAME_VOLUME){
               if(I3DCAST_CVOLUME(origin_frm)->GetVolumeType1()==I3DVOLUME_RECTANGLE)
                  two_sided = true;
            }else{
                              //make it two sided for two sided materials
               PI3D_material mat = tf.origin.GetMaterial();
               if(mat)
                  two_sided = mat->Is2Sided1();
            }
         }
         if(!two_sided){
            MS_F_DEBUG("sphere move 0.1: backface quit");
            return false;
         }
         face_plane.Invert();
      }
   }
                              //backface cull - don't stick in plane, which you are leaving
   {
      float f11 = dir1.Dot(face_plane.normal);
      if(CHECK_ZERO_GREATER(f11)){
         MS_F_DEBUG("sphere move 0.2: backface quit");
         return false;
      }
   }
                              //chop move segment to touch plane part only
   {
      float cosh1 = face_plane.normal.Dot(dir1);
      if(I3DFabs(cosh1)<MRG_ZERO){
                              //move is parallel to plane, check distance
         float dist1 = from1.DistanceToPlane(face_plane);
         if(I3DFabs(dist1) >= th.radius)
         {
            MS_F_DEBUG("sphere move 1: parallel to face plane, distance > radius");
            return false;
         }
      }
      else{
         float f = face_plane.normal.Dot(from1) + face_plane.d;
         float min_d = -(f + th.radius) / cosh1;
         float max_d =  -(f - th.radius) / cosh1;
         if(min_d > max_d)
            swap(min_d, max_d);

         if((min_d) >= out_dist){
            MS_F_DEBUG("sphere move 1.1: move don't touch face plane");
            return false;
         }
         if(max_d <= hit_distance){
            MS_F_DEBUG("sphere move 1.2: move don't touch face plane");
            return false;
         }

         hit_distance = Max(min_d, hit_distance);
         out_dist = Min(max_d, out_dist);
      }
   }

                              //clip line by edges planes
   for(int i=3; i--; ){
                              //edges facing outside of triangle
      const S_plane &ed_plane = tf.edge_planes[i];
      float ed_plane_d = ed_plane.d - th.radius;
      E_CLIP_LINE cr = ClipLineByPlane(from1, dir1, ed_plane.normal, ed_plane_d, hit_distance, out_dist);
      if(cr == CL_OUTSIDE){
                              //entire segment out of edge plane
         MS_F_DEBUG(C_fstr("sphere move 2: line segment out of edge plane %i", i));
         return false;
      }
   }/**/

   S_plane bevel;
                              //apply edges beveling
   for(i=3; i--; ){
      const S_vector &shared_point = vertices[tf.vertices_id[i]];
      const S_vector &norm1 = tf.edge_planes[i].normal;
      bool b = MakeBevelPlane(face_plane.normal, norm1, shared_point, th.radius, bevel);
      if(b){
         E_CLIP_LINE cr = ClipLineByPlane(from1, dir1, bevel.normal, bevel.d, hit_distance, out_dist);
         if(cr == CL_OUTSIDE){
            MS_F_DEBUG(C_fstr("sphere move 3: line segment clipped by edge bevel %i", i));
            return false;
         }
      }

   }/**/
                              //counterside edges beveling
   for(i=3; i--; ){
      const S_vector &shared_point = vertices[tf.vertices_id[i]];
      const S_vector &norm1 = tf.edge_planes[i].normal;
      S_vector invert_face = -face_plane.normal;
      bool b = MakeBevelPlane(invert_face, norm1, shared_point, th.radius, bevel);
      if(b){
         E_CLIP_LINE cr = ClipLineByPlane(from1, dir1, bevel.normal, bevel.d, hit_distance, out_dist);
         if(cr == CL_OUTSIDE){
            MS_F_DEBUG(C_fstr("sphere move 3.5: line segment clipped by counterside edge bevel %i", i));
            return false;
         }
      }

   }/**/

                              //apply corners points beveling
   for(i=3; i--; ){
      const S_vector &shared_point = vertices[tf.vertices_id[i]];
      const S_vector &norm1 = tf.edge_planes[i].normal;
      const S_vector &norm2 = tf.edge_planes[(i+2)%3].normal;
      bool b = MakeBevelPlane(norm1, norm2, shared_point, th.radius, bevel);
      if(b){
         int cr = ClipLineByPlane(from1, dir1, bevel.normal, bevel.d, hit_distance, out_dist);
         if(cr == CL_OUTSIDE){
            MS_F_DEBUG(C_fstr("sphere move 4: line segment clipped by point bevel %i", i));
            return false;
         }
      }
   }/**/

//----------------------------
                              //compute slide normal
   S_vector ipact_point = th.from + th.dir_norm*hit_distance;
   S_vector slide_normal;
   byte slide = GetSlideNormal(tf, face_plane, ipact_point, slide_normal);
//----------------------------
                              //allow slide rectangle corner:
                              //if we collide with rectangle face which IS clipped,
                              //search for 2nd face and remove it if not clipped.
                              //if we collide with rectangle face which NOT clipped,
                              //push it only when 2nd face is not clipped or presented
   int replace_id = -1;
   if((origin_frm->GetType1() == FRAME_VOLUME) && (I3DCAST_CVOLUME(origin_frm)->GetVolumeType1() == I3DVOLUME_RECTANGLE)){
      S_collision_info *p_col = &th.collision_list.front();
      for(int i1 = th.collision_list.size(); i1--;){       
                              //we need also check volume_source 'cos different spheres can collide with same face in different distance
         if((p_col[i1].hit_frm == origin_frm) && (p_col[i1].vol_source == th.vol_source))
         {
            if(p_col[i1].triface_id == face_id)
               return true; //we alredy have this face in list(should not happen)
            else{
               if(slide){
                              //if previous face was NOT clipped, replace it
                  if(!p_col[i1].slided)
                     replace_id = i1;
               }else{
                              //do not store non clipped face if clipped is already stored.
                  if(p_col[i1].slided)
                     return true;
               }
            }
            break; //only 2 different faces expected for rectangle, end search
         }
      }
   }
                              //collect all collisions in the way
   if(replace_id == -1){
      th.collision_list.push_back(S_collision_info());
   }
   S_collision_info &ci = (replace_id == -1) ? th.collision_list.back() : th.collision_list[replace_id];

   ci.plane.d = face_plane.d;
   ci.plane.normal = slide ? slide_normal : face_plane.normal;
   ci.face_normal = face_plane.normal;
   ci.modified_face_normal = true;

   ci.slided = slide;
                              //shift by radius
   ci.plane.d -= th.radius;
   ci.hit_frm = origin_frm;
   ci.vol_source = th.vol_source;
   ci.plane_id = tf.plane_id;
   ci.triface_id = face_id;
   ci.face_index = tf.origin.GetFaceIndex();
   ci.hit_distance = hit_distance;

   return true;
}

//----------------------------

void C_bsp_tree::ReportConsistency(PI3D_scene scene, I3D_LOAD_CB_PROC *cbP, void *context) const{

   assert(cbP);
   if(!cbP) return;

   assert(scene);
   if(!scene) return;

   C_vector<PI3D_frame> static_list;
   scene->GetStaticFrames(static_list);

                              //create temp frames map for speed up cheking
   map<CPI3D_frame, int> bsp_frms_map;

   for(dword i1 = frames.Size(); i1--; ){
      bsp_frms_map[frames.GetFrame(i1)] = i1;
   }

   const PI3D_frame *p_static_list = &static_list.front();
   for(int i2 = static_list.size(); i2--; ){

      CPI3D_frame frm = p_static_list[i2];
      if(bsp_frms_map.find(frm) == bsp_frms_map.end()){
         if(cbP){
            (*cbP)(CBM_ERROR, (dword)(const char*)C_fstr("bsp: static frame '%s' missing in tree",
               (const char*)frm->GetName1()), 1, context);
         }
      }
   }
}

//----------------------------

void C_bsp_tree::GetStats(I3D_stats_bsp &st) const{

                              //memory:
   memset(&st, 0, sizeof(st));
                              //class size
   st.mem_used += sizeof(C_bsp_tree);
                              //face_cache
   //st.mem_used += (face_cache.Size()/CELL_SIZE)*sizeof(dword);
   //st.mem_used += face_cache.Size() * sizeof(byte);
                              //planes
   st.mem_used += planes.size() * sizeof(S_plane);
                              //faces
   st.mem_used += faces.size() * sizeof(S_bsp_triface);
                              //nodes
   if(nodes.GetRoot())
      nodes.GetRoot()->GetStatsRec(st);
                              //other:
   st.num_planes = planes.size();          
   st.num_faces = faces.size();
   st.num_vertices = vertices.size();
}

//----------------------------

bool C_bsp_tree::GetFaceVertices(const S_bsp_triface &tf, S_vector p3vert[3]) const{

   for(int i = 3; i--; ){
      assert(tf.vertices_id[0] < vertices.size());
      if(tf.vertices_id[i] >= vertices.size())
         return false;
      p3vert[i]  = vertices[tf.vertices_id[i]];
   }
   return true;
}

//----------------------------

float C_bsp_tree::GetPointPlaneDistance(BSP_index plane_indx, const S_vector &p, E_PLANE_SIDE &near_side, E_PLANE_SIDE &far_side) const{

   assert(plane_indx < planes.size());
   const S_plane &partion_plane = planes[plane_indx];

   float dist = p.DistanceToPlane(partion_plane);

   //if(dist < -MRG_ZERO)
   if(CHECK_ZERO_LESS(dist)){
      near_side = PS_BACK;
      far_side = PS_FRONT;
   }else{
      near_side = PS_FRONT;
      far_side = PS_BACK;
   }
   return dist;
}

//----------------------------

void C_bsp_tree::DebugDraw(const S_preprocess_context &pc, C_bsp_drawer &draw_help) const{

   if(!draw_help.IsPrepared())
      draw_help.Init(vertices, faces);

   PI3D_driver drv = pc.scene->GetDriver();
   bool zw_en = drv->IsZWriteEnabled();
   if(zw_en) drv->EnableZWrite(false);

   draw_help.Render(pc);
   DebugHighlight(pc);

   if(zw_en) drv->EnableZWrite(true);
}

//----------------------------

void C_bsp_tree::DebugHighlight(const S_preprocess_context &pc) const{

   const dword HIGHLIGHT_CLR = 0x4b0000ff;

   C_vector<S_vector> trg_verticies;
   C_vector<word> trg_indexes;

   const S_bsp_triface *p_faces = faces.size() ? &faces.front() : NULL;

   for(int i = faces.size(); i--;){
      const S_bsp_triface &tf = p_faces[i];

      if(!tf.highlight)
         continue;

      tf.highlight = false;

      S_vector tmp_vert[3];
      GetFaceVertices(tf, tmp_vert);
      if(!CheckFrustumIntersection(pc.view_frustum.clip_planes, 4,
         pc.view_frustum.frustum_pts, tmp_vert, 3, pc.view_frustum.view_pos))
         continue;
      for(int kk = 0; kk < 3; kk++){
         trg_indexes.push_back((word)trg_verticies.size());
         trg_verticies.push_back(tmp_vert[kk]);
      }
   }
   if(trg_verticies.size()){

      pc.scene->GetDriver1()->SetTexture(NULL);
      pc.scene->SetRenderMatrix(I3DGetIdentityMatrix());

      if(trg_verticies.size()){
         const S_vector *p_vert = &trg_verticies.front();
         const word *p_tri_idx = &trg_indexes.front();

         pc.scene->DrawTriangles(p_vert, trg_verticies.size(), I3DVC_XYZ, p_tri_idx, trg_indexes.size(), HIGHLIGHT_CLR);
      }
   }
}

//----------------------------
//C_bsp_drawer
//----------------------------

void C_bsp_drawer::Init(const C_vector<S_vector> &vertices, const C_vector<S_bsp_triface> &faces){

   //now let's go through all faces, fill vertex buffer with new vertices, but do not 
   //add same vertices more then ones per group.
   //store indices which are relative to begin of vertex group.
#if 1
   const word VERTICES_LIMIT = 2000;
   const word INDICES_LIMIT = 6000;
#else
//   const word VERTICES_LIMIT = 8000;
//   const word INDICES_LIMIT = 12000;
   const word VERTICES_LIMIT = 1;
   const word INDICES_LIMIT = 1;
//   const word VERTICES_LIMIT = 500;
//   const word INDICES_LIMIT = 1500;
#endif


   //temporary map of indices. remap vertex index into part of buffer reserved to current group.

   typedef map<dword, word> t_group_map;
   t_group_map group_id_map;

                              //help map for search edges duplicities
   typedef map<dword, bool > t_edges_map;
   t_edges_map slide_edges_map;
   t_edges_map thin_edges_map;


   const S_vector *p_vertices = vertices.size() ? &vertices.front() : NULL;
   const S_bsp_triface *p_faces = faces.size()?&faces.front():NULL;

   bool create_new_group = true;
   S_render_group *curr_group = NULL;
   for(int i = faces.size(); i--; ){

      if(create_new_group){
         render_groups.push_back(S_render_group());
         curr_group = &render_groups.back();
         curr_group->vertex_offset = vertex_buffer.size();
         curr_group->index_offset = index_buffer.size();
         curr_group->slide_offset = slide_edges.size();
         curr_group->thin_offset = thin_edges.size();

         group_id_map.clear();
         slide_edges_map.clear();
         thin_edges_map.clear();
         create_new_group = false;
      }

      const S_bsp_triface &tf = p_faces[i];
      for(int j = 0; j < 3; j++){
         const dword v_id = tf.vertices_id[j];
         word group_vertex_id; //id of vertex in current group
         t_group_map::iterator it = group_id_map.find(v_id);
         if(it == group_id_map.end()){
            group_vertex_id = word(vertex_buffer.size() - curr_group->vertex_offset);
            group_id_map.insert(t_group_map::value_type(v_id, group_vertex_id));
            vertex_buffer.push_back(p_vertices[v_id]);
         }else{
            group_vertex_id = (*it).second;
         }
         index_buffer.push_back(group_vertex_id);
      }
                              //push edges
      const dword first_index = index_buffer.size() - 3;
      for(int kk = 0; kk < 3; kk++){
         pair<word, word> new_edge(index_buffer[first_index + kk], index_buffer[first_index + ((kk+1)%3)]);
         dword edge_key = Min(new_edge.first, new_edge.second) | (Max(new_edge.first, new_edge.second) << 16);
         if(tf.slide_flags&(1<<kk)){
            if(slide_edges_map.find(edge_key) != slide_edges_map.end())
               continue;
            slide_edges.push_back(new_edge.first); 
            slide_edges.push_back(new_edge.second);
            slide_edges_map.insert(pair<dword,bool>(edge_key, true));
         }else{
            if(thin_edges_map.find(edge_key) != thin_edges_map.end())
               continue;
            thin_edges.push_back(new_edge.first);
            thin_edges.push_back(new_edge.second);
            thin_edges_map.insert(pair<dword,bool>(edge_key, true));
         }
      }
      if(vertex_buffer.size()+3-curr_group->vertex_offset > VERTICES_LIMIT
         || index_buffer.size()+3-curr_group->index_offset > INDICES_LIMIT)
      {
         curr_group->num_vertices = word(vertex_buffer.size() - curr_group->vertex_offset);
         curr_group->num_indices = word(index_buffer.size() - curr_group->index_offset);
         curr_group->num_slides = word(slide_edges.size() - curr_group->slide_offset);
         curr_group->num_thins = word(thin_edges.size() - curr_group->thin_offset);

         create_new_group = true;
      }
   }
   if(curr_group){
      curr_group->num_vertices = word(vertex_buffer.size() - curr_group->vertex_offset);
      curr_group->num_indices = word(index_buffer.size() - curr_group->index_offset);
      curr_group->num_slides = word(slide_edges.size() - curr_group->slide_offset);
      curr_group->num_thins = word(thin_edges.size() - curr_group->thin_offset);
   }
   prepared_to_draw = true;
}

//----------------------------

void C_bsp_drawer::Render(const S_preprocess_context &pc){

   PI3D_scene scn = pc.scene;

   const dword FACE_CLR =  0x70000070;
   const dword SLIDE_CLR = 0xffff00af;
//   const dword THIN_CLR =  0xa08080e0;
   const dword THIN_CLR =  0xa00080e0;

#ifndef GL
                              //render all groups
   if(!scn->GetDriver1()->CanUsePixelShader())
      scn->GetDriver1()->DisableTextureStage(1);
#endif
   scn->GetDriver1()->SetTexture(NULL);
   scn->SetRenderMatrix(I3DGetIdentityMatrix());

   for(int ii = render_groups.size(); ii--; ){
      const S_render_group &rg = render_groups[ii];
      const S_vector *p_vert = &vertex_buffer[rg.vertex_offset];
      const word *p_indx = &index_buffer[rg.index_offset];
      scn->DrawTriangles(p_vert, rg.num_vertices, I3DVC_XYZ, p_indx, rg.num_indices, FACE_CLR);

      const word *p_slide_idx = &slide_edges[rg.slide_offset];
      scn->DrawLines(p_vert, rg.num_vertices, p_slide_idx, rg.num_slides, SLIDE_CLR);

      const word *p_thin_idx = &thin_edges[rg.thin_offset];
      scn->DrawLines(p_vert, rg.num_vertices, p_thin_idx, rg.num_thins, THIN_CLR);
   }

}

//----------------------------
//----------------------------

bool I3D_volume::ClipLineByBox(const S_trace_help &th, float &d_in) const{

   //C_vector<S_plane>clip_planes;
   S_plane clip_planes[6];
                              //collect planes
                              //note: this algoritmus not work when some side is zero (rectangle) AND radius is zero.
                              //This is because when clipping line by two same planes but inverse, entire line is clipped (there is no tolerance).
                              //For this case when zero radius passed we should choose plane looking to dir,
                              //compute point and test all against it instead interval.
                              //Or improve ClipLineByPlane if possible.
   for(int i = 0; i < 3; i++){
      clip_planes[i*2] = S_plane(-normal[i], d_min[i]);
      clip_planes[i*2+1] = S_plane(normal[i], d_max[i]);
   }

   float d_out(th.dir_magn);
   d_in = .0f; 
                              //clip it all
   for(i = 6; i--; ){
      const S_plane &pl = clip_planes[i];
      E_CLIP_LINE cr = ClipLineByPlane(th.from, th.dir_norm, pl.normal, pl.d - th.radius, d_in, d_out);
      if(cr == CL_OUTSIDE){
         d_in = th.dir_magn;
         return false;        //entire segment out of edge plane
      }
   }
   d_in = Min(Max(.0f, d_in), th.dir_magn);
   return true;
}

//----------------------------
//----------------------------