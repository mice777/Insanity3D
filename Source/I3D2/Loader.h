#ifndef __LOAD_3DS_H
#define __LOAD_3DS_H


//----------------------------

#define MAX_UV_CHANNELS    2  //number of uv channels we may load

//----------------------------

#pragma pack(push,1)
class C_rgb{
   byte color[3];
public:
   C_rgb(dword val = 0){
      color[0] = (byte)(val>>16);
      color[1] = (byte)(val>>8);
      color[2] = (byte)val;
   }
   C_rgb(byte r, byte g, byte b){
      color[0]=r;
      color[1]=g;
      color[2]=b;
   }
   byte operator[](int i){
      return color[i];
   }

   S_vector ToVector() const{
      return S_vector((float)color[2] * R_255, (float)color[1] * R_255, (float)color[0] * R_255);
   }
};
#pragma pack(pop)

//----------------------------

#define LOADF_TXT_TRANSP         1     //colorkeyed
#define LOADF_TXT_TRUECOLOR      2
#define LOADF_TXT_DIFF_ANIM      4
#define LOADF_TXT_REFL_ANIM      8
#define LOADF_TXT_EMBM_ANIM      0x10
#define LOADF_TXT_DIFFUSE_KEEP   0x20
#define LOADF_TXT_TRANSP_ALPHA   0x80  //colorkeyed with alpharef at zero
#define LOADF_TXT_CUBEMAP        0x100 //cube map

//----------------------------
                              //internal stock lists for object keeping
                              // between creation and hierarchy build
struct S_mat_info{
   C_smart_ptr<I3D_material> mat;
   enum{
      MAP_DIFFUSE,
      MAP_OPACITY,
      MAP_ENVIRONMENT,
      MAP_EMBM,
      MAP_PROCEDURAL,
      MAP_NORMAL,
      MAP_LAST
   };
   C_str map_filename[MAP_LAST];
   dword ct_flags;
   int diffuse_anim_speed;
   float environment_power, embm_power;
   bool txt_created;
   dword load_flags;
   int embm_anim_speed;
   S_mat_info():
      load_flags(0),
      txt_created(false)
   {}
   S_mat_info(PI3D_material m1, dword ct_flags1, const C_str &sd, const C_str &so,
      const C_str &ss, const C_str &sb):
      diffuse_anim_speed(0),
      mat(m1), ct_flags(ct_flags1)
   {
      map_filename[0] = sd;
      map_filename[1] = so;
      map_filename[2] = ss;
      map_filename[3] = sb;
   }
};

//----------------------------

class C_loader{
                              //id-based array of frames
   C_vector<PI3D_frame> frame_id_list;

public:
   PI3D_driver driver;

   PI3D_scene scene;          //valid during scene loading
   PI3D_frame root_frame;     //valid during scene or model loading
   PI3D_container container;  //valid during scene or model loading
   PI3D_animation_set anim_set; //valid during animation loading

   C_chunk &ck;
   dword load_flags;
   dword file_length;
                              //loader callback function
   I3D_LOAD_CB_PROC *load_cb_proc;
   void *load_cb_context;

   C_str file_name;
   __int64 file_time_;

   C_str user_comments;       //read from the loaded file

   int tab;                   //tab for log

   typedef map<PI3D_frame, C_str> t_frm_properties;
   t_frm_properties frm_properties;

public:
                              //regions of joints
   struct S_region_data{
      S_matrix bbox_matrix;   //bounding box' matrix relative to this joint (i.e. local matrix)
      I3D_bbox bbox;          //bounding-box used for SM vertex assignment
      int index;
   };
   typedef map<PI3D_frame, C_vector<S_region_data> > t_regions_info;
   t_regions_info regions_info;

   /*
   struct S_skin_data{
      C_str joint_name;
      struct S_vertex_info{
         word vertex_index;
         float weight;
      };
      C_buffer<S_vertex_info> vertex_info;
   };
   typedef C_vector<S_skin_data> t_skin_data_list;
   typedef map<CPI3D_frame, t_skin_data_list> t_skin_data;
   t_skin_data skin_data;
   */

protected:

//----------------------------
// Add single region to list of regions associated with particular frame.
   void AddRegion(PI3D_frame frm, const I3D_bbox &bb, const S_matrix &tm, int index){

      t_regions_info::iterator it = regions_info.find(frm);
      if(it==regions_info.end())
         it = regions_info.insert(pair<PI3D_frame, C_vector<S_region_data> >(frm, C_vector<S_region_data>())).first;
      C_vector<S_region_data> &rdv = (*it).second;
      rdv.push_back(S_region_data());
      S_region_data &rd = rdv.back();
      rd.bbox_matrix = tm;
      rd.bbox = bb;
      rd.index = index;
   }

public:
   C_loader(PI3D_driver d, C_chunk &ck_in):
      driver(d),
      ck(ck_in),
      load_flags(0),
      tab(0),
      scene(NULL),
      root_frame(NULL),
      container(NULL),
      anim_set(NULL),
      load_cb_proc(NULL), load_cb_context(NULL),
      anim_length(0),
      num_mats(0)
   {}

//----------------------------
// If this function returns true, it is treated as a request to interrupt loading.
   I3D_RESULT ShowProgress();

   void REPORT_ERR(const char *cp) const{
      if((load_flags&I3DLOAD_ERRORS) && load_cb_proc){
         load_cb_proc(CBM_ERROR,
            (dword)(const char*)C_fstr("%s (file '%s')", cp, (const char*)file_name),
            0, load_cb_context);
      }
   }

   void LOG(const char *cp){
      char buf[512];
      memset(buf, ' ', Min(256, tab*2));
      strcpy(&buf[tab*2], cp);
      load_cb_proc(CBM_LOG, (dword)buf, 0, load_cb_context);
   }

   inline PI3D_frame GetFrame(dword id) const{
      return id<frame_id_list.size() ? frame_id_list[id] : NULL;
   }

   bool AddFrameID(dword id, PI3D_frame frm){
      if(frame_id_list.size() < (id+1)) frame_id_list.resize(id+256, NULL);
      frame_id_list[id] = frm;
      return true;
   }

   struct S_auto_lod_data{
      bool use;
      float min_dist;
      float max_dist;
      int num_parts;
      int min_num_faces;
      bool preserve_edges;
      S_auto_lod_data():
         use(false),
         preserve_edges(false),
         min_dist(-1),
         max_dist(-1),
         num_parts(-1)
      {}
   };
private:
   struct S_vertex{
      S_vector xyz;
      S_vector normal;
      I3D_text_coor tex;
   };
   struct S_vertex_st: public S_vertex{
      S_texture_space ts;
   };

   struct S_normals_vertex_info{
                              //keep list of faces where this vertex is used
      struct S_face{
         word index;          //index of face
         float angle;         //angle of face corner in this vertex
         S_face(){}
         S_face(word i, float a): index(i), angle(a){}
      };
      C_vector<S_face> faces;
      S_normals_vertex_info(){
         faces.reserve(4);
      }
   };

   dword anim_length;         //length of loaded animation

                              //err reporting - visually displaying geometry
   struct S_triangle{
      S_vector v[3];
   };
   typedef map<C_smart_ptr<I3D_frame>, C_vector<S_triangle> > t_visual_err_report;
   t_visual_err_report visual_err_report;

//----------------------------
// Read single material from chunk.
   I3D_RESULT ReadMaterial();

   bool ReadTrackPos(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp);
   bool ReadTrackRot(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp);
   bool ReadTrackScl(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp, S_vector &nu_scale);
   bool ReadTrackVis(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp);
   bool ReadTrackNote(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp);

   dword num_mats;            //helper for displaying progress
   C_vector<C_smart_ptr<I3D_material> > materials;

   enum{
      MAT_SRC_TRUECOLOR = 1,
      MAT_SRC_NOMIPMAP = 2,
      MAT_SRC_PROCEDURAL = 4,
      MAT_SRC_NOCOMPRESS = 8,
      MAT_SRC_ANIMATED = 0x10,
   };
   struct S_mat_src: public C_str{
      dword flags;            //combination of MAT_SRC_??? flags
      S_vector2 uv_offset;
      S_vector2 uv_scale;
      float power;
      dword anim_speed;

      S_mat_src():
         flags(0),
         power(1.0f),
         anim_speed(0),
         uv_offset(0, 0),
         uv_scale(1, 1)
      {
      }
   };

                              //blend map info, kept during loading
   typedef map<C_str, S_mat_src> t_blend_maps;
   t_blend_maps blend_maps;

//----------------------------
   I3D_RESULT CreateTexture(dword ct_flags, const S_mat_src &mat_src, const char *mat_name,
      PI3D_material mat, I3D_MATERIAL_TEXTURE_INDEX, const C_str *opt_name = NULL);

//----------------------------
   /*
   class I3D_animated_texture *CreateAnimatedTexture(const char *fname0, const char *fname1,
      const S_load_context &lc, PI3D_texture tp_last,
      dword ct_flags, int anim_speed);
      */

//----------------------------
   I3D_RESULT SetupMaterial(PI3D_material, dword ct_flags, dword txt_flags,
      const S_mat_src &diff, const S_mat_src &opt, const S_mat_src &env, const C_str &proc, const S_mat_src &embm,
      const S_mat_src &detail, const S_mat_src &normalmap, const S_mat_src &bump_level, const S_mat_src &specular,
      const S_mat_src &secondmap, const char *mat_name);

   void CleanMesh(PI3D_frame frm, C_vector<S_vector> &verts, C_vector<I3D_triface> &faces,
      C_vector<I3D_triface> uv_faces[], C_vector<byte> &smooth_groups,
      C_vector<I3D_face_group> &face_groups, const char *name, bool &allow_cache);
   I3D_RESULT ReadMesh(PI3D_frame frm, class I3D_mesh_base*, const char *name, bool &allow_cache);

//----------------------------
// Assigns uv coordinates to vertices (from uv-map faces), duplicates vertices as necessary.
   bool AssignUVCoordinates(C_vector<word> &vertex_map, C_vector<S_vertex> &verts, C_vector<I3D_triface> &faces,
      const C_vector<I3D_text_coor> uv_verts[], const C_vector<I3D_triface> uv_faces[], const char *err_name) const;

//----------------------------
   bool GenerateTextureSpace(const C_vector<S_vertex> &verts, const C_vector<I3D_triface> &faces,
      C_buffer<S_texture_space> &texture_space,
      const char *err_name, bool &allow_cache) const;

//----------------------------
// Put all UV coors into near zero range, graphics hardware don't like too big float numbers
// in texture coordinates.
   bool UV_Check(C_vector<I3D_text_coor> &uv_verts, const char *err_name);

//----------------------------
// Generate normals on set of vertices, using info from smooth groups.
// May add new vertices.
   void MakeVertexNormals(int num_orig_verts, C_vector<word> &vertex_map,
      C_vector<S_vertex> &verts, C_vector<I3D_triface> &faces, const C_vector<byte> &smooth_groups) const;

   struct S_buildsectors{
      C_loader *ld;
      PI3D_scene scene;
      PI3D_model model;      //if !NULL, it's considered to be owner of frames
   };
   I3DENUMRET BuildSectors(PI3D_frame frm, PI3D_scene scene, PI3D_model model);
   void AddSectorWalls(PI3D_frame root, C_vector<C_smart_ptr<I3D_frame> > &walls,
      C_vector<C_smart_ptr<I3D_visual> > &portal_defs, bool root_help, PI3D_sector sct);

   static I3DENUMRET I3DAPI cbBuildSectors(PI3D_frame frm, dword c){
      S_buildsectors *bs = (S_buildsectors*)c;
      return bs->ld->BuildSectors(frm, bs->scene, bs->model);
   }

   struct S_smooth_info{
      C_smart_ptr<I3D_frame> vis;
      bool need_this;         //true if this visual needs to be shaded, false if read from cache
      S_smooth_info():
         need_this(true)
      {}
   };
//----------------------------
// Smooth normals of imported selected objects. 
   void SmoothSceneNormals(PI3D_scene scene, C_vector<S_smooth_info> &smooth_list);
public:

   struct S_SM_data{
      S_auto_lod_data al;
      bool mesh_ok;
      bool allow_cache;

      S_SM_data():
         mesh_ok(false),
         allow_cache(false)
      {}
   };

//----------------------------

public:
   I3D_RESULT Open(const char* fname, dword flags, PI3D_LOAD_CB_PROC cb_proc,
      void *cb_context, PI3D_frame root, PI3D_scene scene, PI3D_animation_set, PI3D_container);
   void Close();
};

//----------------------------
#endif