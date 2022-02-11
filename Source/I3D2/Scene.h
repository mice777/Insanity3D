#ifndef __SCENE_H
#define __SCENE_H

#include "sector.h"
#include "bsp.h"
#include "container.h"
#include "camera.h"
#include "volume.h"


//----------------------------

template<class T> class C_sort_list;

//----------------------------

class I3D_scene{
protected:
   dword ref;
   dword xref;                //number of cross-references made on scene

   I3D_container cont;

#define SCNF_IN_DYN_TREE_UPDATE 1   //set when inside of BuildDynamicVolTree (optimization hint)
#define SCNF_VIEWPORT_ASPECT  2     //use real viewport aspect ratio

   PI3D_driver drv;
   PI3D_sector primary_sector;
   mutable dword scn_flags;

                              //level map containers:
   mutable C_vector<C_smart_ptr<I3D_sector> > sectors;
   mutable C_vector<PI3D_sound> sounds;
   C_smart_ptr<I3D_camera> shd_camera; //camera for rendering shadows (so that we don't need to create one in runtime)

   S_vector color_bgnd;       //background color
   dword dw_color_bgnd;       //background color - precomputed
   PI3D_sector backdrop_sector;
   PI3D_camera backdrop_camera;
                              
   C_smart_ptr<I3D_camera> current_camera;
                              //transformations
   S_matrix m_view_proj_hom;  //result of view and projection matrices, 4x4, in homogeneous space (x and y in range -1 ... 1, z in range 0 .. 1)
   S_matrix mt_view_proj_hom;
#ifdef GL
   S_matrix gl_m_view_proj_hom;
#endif

   dword last_render_time;    //counter set to current time (IGraph::ReadTimer) each time a scene is rendered

   int proposed_num_shd_receivers;  //number of shd recs from last rendering

   S_preprocess_context pc_main, pc_backdrop, pc_shadows;

#if defined _DEBUG && 0
                              //render target utilizing cube texture
   class C_cube_render_target: public C_render_target<6, I3D_texture>{
   public:
      C_smart_ptr<I3D_camera> cam;
      enum E_SIDE{
         SIDE_LEFT,
         SIDE_RIGHT,
         SIDE_DOWN,
         SIDE_UP,
         SIDE_BACK,
         SIDE_FRONT,
      };

      void Close(){
         C_render_target<6, I3D_texture>::Close();
         cam = NULL;
      }

//----------------------------
// Initalize camera, rtt, etc.
   I3D_RESULT Init(PI3D_scene, dword size, dword ct_flags);

//----------------------------
// Setup camera to look to particular side.
      void SetupCamera(const S_vector &pos, E_SIDE, const S_matrix *m_rot = NULL);

//----------------------------
// etup render target to be set to appropriate side texture.
      void SetupRenderTarget(PI3D_driver, E_SIDE);
   } rt_env;
#endif
   C_smart_ptr<I3D_model> debug_mesh_model;  //model containing meshes for debug rendering

public:
   S_matrix curr_render_matrix;

   mutable I3D_stats_render render_stats;
   mutable I3D_stats_scene scene_stats;

   inline const S_matrix &GetViewProjHomMatTransposed() const{ return mt_view_proj_hom; }

   //PI3D_visual GetDebugDrawVisual(const char *name);
   //void RenderDebugDrawVisual(PI3D_visual, const S_matrix &m, const S_matrix &m_inv);
private:
                              //sector sound properties fading
   S_sound_env_properties ssp_fade;
   int ssp_count_down;        //0 = not fading

                              //recursive rendering funcs
   void Preprocess(S_preprocess_context&, dword prep_flags);
   void PreprocessOccluders(const S_preprocess_context&, PI3D_frame root, C_sort_list<class I3D_occluder*> &collect_list, PI3D_sector curr_sector);
   void PreprocessSectors(PI3D_frame root, S_preprocess_context&);
   void PreprocessChildren(PI3D_frame root, S_preprocess_context&, dword prep_flags);
   void PreprocessSounds(int time);
   void UpdateSoundProperties(int time, PI3D_sector prev_sector);

#ifndef GL
   void RenderShadows(const S_preprocess_context&);
   void RenderMirrors(const S_preprocess_context&);
#endif
//----------------------------
// Select which light will be used for making shadow on given visual.
   PI3D_light SelectShadowLight(PI3D_sector sct, const S_vector &visual_pos,
      PI3D_frame last_light, float dist_to_cam, float &opacity) const;

   void SetupCameraSector();

   friend I3D_sector;
   friend I3D_scene *CreateScene(PI3D_driver);
   friend I3D_sound;

protected:
                              //BSP tree manager
   C_bsp_tree bsp_tree;
   mutable C_bsp_drawer bsp_draw_help;

//----------------------------

public:
   void DrawList(const S_preprocess_context&);
   void DrawListPS(const S_preprocess_context&);
   inline dword GetBgndClearColor() const{ return dw_color_bgnd; }

   void SetMViewProj(CPI3D_camera, bool z_bias = false);
//----------------------------
// Render single view from currently active camera.
   void RenderView(dword render_flags, E_RENDERVIEW_MODE,
#ifndef GL
      const S_plane *add_clip_planes = NULL, dword num_add_clip_planes = 0,
#endif
      S_preprocess_context *use_pc = NULL);

   static I3DENUMRET I3DAPI cbDebugDraw(PI3D_frame, dword);

   void DebugDrawFrames(I3D_FRAME_TYPE ft, dword enum_mask, const S_view_frustum&, const C_vector<PI3D_occluder> &occ_list);

   void GetStaticFrames(C_vector<PI3D_frame> &frm_list) const;

public:
   I3D_scene(PI3D_driver);
   bool Init();
   ~I3D_scene();
   I3D_rectangle viewport;

                              //viewport pre-process
   float viewport_center[2];
   float viewport_size_half[2];
   float aspect_ratio;

   inline int NumSectors() const{ return sectors.size(); }
   inline CPI3D_sector *GetSectors(){ return (CPI3D_sector*)&sectors.front(); }
   //inline PI3D_camera GetCurrentCamera() const{ return current_camera; }
   inline PI3D_sector GetPrimarySector1() const{ return primary_sector; }
   inline const I3D_rectangle &GetViewport1(bool *aspect = NULL) const{
      if(aspect) *aspect = (scn_flags&SCNF_VIEWPORT_ASPECT);
      return viewport;
   }
   inline PI3D_camera GetActiveCamera1(){ return current_camera; }
   inline CPI3D_camera GetActiveCamera1() const{ return current_camera; }
   inline PI3D_container GetContainer1(){ return &cont; }

   void DelAnimation(CPI3D_frame);

                              //debugging
   void DrawIcon(const S_vector &pos, int res_id, dword color = 0xffffffff, float size = 1.0f);
   void DebugDrawSphere(const S_matrix &m, float radius, dword color = 0xffffffff);
   void DebugDrawCylinder(const S_matrix &m, float radius1, float radius2, float height, dword color = 0xffffffff);
   void DebugDrawCylinder(const S_matrix &m, float radius, float height, dword color = 0xffffffff, bool capped = false);
   void DebugDrawArrow(const S_matrix &m, float lenght, dword color = 0xffffffff, int axis = 2);
   void DebugDrawBBox(const S_vector *bbox_full, dword color = 0xffffffff);
   void DebugDrawContour(const S_vector *contour_points, dword num_cpts, dword color = 0xffffffff);

   inline PI3D_driver GetDriver1() const{ return drv; }

   void AddAnimation(const class I3D_keyframe_anim*, PI3D_frame, dword options);

//----------------------------
   inline float GetInvAspectRatio() const{
      return (scn_flags&SCNF_VIEWPORT_ASPECT) ? (-viewport_size_half[1] / viewport_size_half[0]) : .75f;
   }

   inline void AddXRef(){ ++xref; }
   inline void ReleaseXRef(){ --xref; }
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

                              //general
   I3DMETHOD_(PI3D_driver,GetDriver)(){ return (PI3D_driver)drv; }
   I3DMETHOD_(CPI3D_driver,GetDriver)() const{ return (PI3D_driver)drv; }
                              //rendering
   I3DMETHOD(Render)(dword flags);
   I3DMETHOD(SetViewport)(const I3D_rectangle &rc_ltrb, bool aspect=false);
   I3DMETHOD_(const I3D_rectangle&,GetViewport)() const{ return viewport; }
                              //frames
   I3DMETHOD(EnumFrames)(I3D_ENUMPROC*, dword user, dword flags=ENUMF_ALL, const char *mask=NULL) const;
   I3DMETHOD_(PI3D_frame,FindFrame)(const char *name, dword data=ENUMF_ALL) const;
                              //lights
   I3DMETHOD_(S_vector,GetLightness)(const S_vector &pos, const S_vector *norm, dword light_mode_flags) const;
                              //cameras
   I3DMETHOD(SetActiveCamera)(PI3D_camera);
   I3DMETHOD_(PI3D_camera,GetActiveCamera)(){ return current_camera; }
   I3DMETHOD_(CPI3D_camera,GetActiveCamera)() const{ return current_camera; }
   I3DMETHOD(UnmapScreenPoint)(int x, int y, S_vector &pos, S_vector &dir) const;
   I3DMETHOD(TransformPointsToScreen)(const S_vector *in, S_vectorw *out, dword num) const;

   I3DMETHOD_(PI3D_frame,CreateFrame)(dword frm_type, dword sub_type = 0) const;
   I3DMETHOD_(PI3D_sector,GetSector)(const S_vector&, PI3D_sector suggest_sector = NULL);
   I3DMETHOD_(CPI3D_sector,GetSector)(const S_vector&, CPI3D_sector suggest_sector = NULL) const;
   I3DMETHOD_(PI3D_sector,GetPrimarySector)(){ return primary_sector; }
   I3DMETHOD_(CPI3D_sector,GetPrimarySector)() const{ return primary_sector; }
   I3DMETHOD_(PI3D_sector,GetBackdropSector)(){ return backdrop_sector; }
   I3DMETHOD_(CPI3D_sector,GetBackdropSector)() const{ return backdrop_sector; }
   I3DMETHOD_(void,SetBackdropRange)(float n1, float f1);
   I3DMETHOD_(void,GetBackdropRange)(float &near1, float &far1) const;
   I3DMETHOD(SetFrameSector)(PI3D_frame);
   I3DMETHOD(SetFrameSectorPos)(PI3D_frame, const S_vector &pos);
                              //background
   I3DMETHOD(SetBgndColor)(const S_vector&);
   I3DMETHOD_(const S_vector&,GetBgndColor)() const{ return color_bgnd; }

                              //collisions
   I3DMETHOD_(bool,TestCollision)(I3D_collision_data&) const = 0;
   I3DMETHOD_(void,GenerateContacts)(const I3D_contact_data &cd) const = 0;

   I3DMETHOD_(void,AttenuateSounds)();

   I3DMETHOD_(dword,GetLastRenderTime)() const{ return last_render_time; }

   I3DMETHOD_(I3D_RESULT,SetRenderMatrix)(const S_matrix&);
   I3DMETHOD(DrawLine)(const S_vector &v1, const S_vector &v2, dword color = 0xffffffff) const;
   I3DMETHOD(DrawLines)(const S_vector *v, dword numv, const word *indx,
      dword numi, dword color = 0xffffffff) const;
   I3DMETHOD(DrawTriangles)(const void *v, dword numv, dword vc_flags, const word *indx, dword numi,
      dword color = 0xffffffff) const;

   I3DMETHOD(DrawSprite)(const S_vector &pos, PI3D_material, dword color = 0xffffffff, float size = 1.0f,
      const I3D_text_coor [2] = NULL) const;

   I3DMETHOD(CreateBspTree)(I3D_BSP_BUILD_CB_PROC *cbP = NULL, void *context = NULL) ;
   I3DMETHOD_(void,DestroyBspTree)();

   I3DMETHOD_(bool,SaveBsp)(C_cache *cp) const;
   I3DMETHOD_(bool,LoadBsp)(C_cache *cp, I3D_LOAD_CB_PROC* = NULL, void *context = NULL, dword check_flags = 0);
   I3DMETHOD_(bool,IsBspBuild)() const;

   I3DMETHOD(ConstructLightmaps)(PI3D_lit_object *lmaps, dword num_lmaps, dword flags, I3D_LOAD_CB_PROC*, void *context);
   I3DMETHOD(GetStats)(I3DSTATSINDEX, void *data) = 0;

   I3D_CONTAINER(;)
   I3DMETHOD_(PI3D_container,GetContainer)(){ return &cont; }
   I3DMETHOD_(CPI3D_container,GetContainer)() const{ return &cont; }

   virtual void UpdateDynamicVolume(PI3D_volume) const = 0;
   virtual void RemoveFromDynamicVolume(PI3D_volume, bool smart) const = 0;
   virtual void DebugDrawDynCols() = 0;
   virtual void BuildDynamicVolTree() const = 0;
};

//----------------------------

#endif
