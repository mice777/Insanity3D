#ifndef __SECTOR_H
#define __SECTOR_H

#include "frame.h"
#include "driver.h"


//----------------------------
// Note: portal has set of vertices - relative to sector it is owned by (prim_sector member),
// and a plane, the plane's normal points out of sector's area

//----------------------------

enum PORTAL_STATE{        
   PS_UNKNOWN,
   PS_OUT_OF_VIEW,
   PS_FACED,               //looking to camera - allowing sight into owning sector
   PS_NOT_FACED,           //looking opposite - allowing sight into connected sector
};

//----------------------------

class I3D_portal{
   dword ref;

#define PRTF_ENABLED       1     //active portal
#define PRTF_ONEWAY_IN     2     //portal allowing to look inside
#define PRTF_ONEWAY_OUT    4     //portal allowing to look outside

   PI3D_driver drv;
   PI3D_sector owning_sector; //sector which has created us and for which we work

   dword prt_flags;
   
                              //portal's geometry
   C_vector<S_vector> vertices;
   S_plane plane;             //portal's plane, the normal points OUT of sector
   C_smart_ptr<I3D_sector> connect_sector;   //keep this, we may be connected to parent sector also without matching portal
   C_smart_ptr<I3D_portal> connect_portal;   //portal we're connected to

   S_view_frustum vf;
   PORTAL_STATE state;

public:
   I3D_portal(PI3D_sector, dword init_flags = 0);
   ~I3D_portal();
//----------------------------
// Build portal, given a convex polygon (defined by set of CW vertices).
   bool Build(const C_vector<S_vector> &v);

//----------------------------
// Check if all vertices of 2 portals are identical (in 3D), and their normals are inversed.
   bool Equals(CPI3D_portal) const;

//----------------------------
// Prepare portal's frustum for current view.
// This function returns true when portal is facing the camera position.
   bool PrepareFrustum(const S_vector &cam_pos);

//----------------------------
// Connect 2 portals - this is done by simply keeping pointer and ref to connected portal.
   inline void ConnectTo(PI3D_portal prt){
      connect_portal = prt;
      state = PS_UNKNOWN;
   }

   inline PI3D_portal GetConnectedPortal(){ return connect_portal; }
   inline CPI3D_portal GetConnectedPortal() const{ return connect_portal; }
   inline PI3D_sector GetConnectedSector(){ return connect_sector; }
   inline CPI3D_sector GetConnectedSector() const{ return connect_sector; }
   inline void SetConnectedSector(PI3D_sector s){ connect_sector = s; }
   inline PI3D_sector GetOwningSector() const{ return owning_sector; }
   inline void SetOwningSector(PI3D_sector s){ owning_sector = s; }
   inline bool IsEnabled() const{ return (prt_flags&PRTF_ENABLED); }
   inline dword GetFlags() const{ return prt_flags; }
   inline const C_vector<S_vector> &GetVertexPool() const{ return vertices; }
   inline const S_view_frustum &GetFrustum() const{ return vf; }
   inline PORTAL_STATE GetState() const{ return state; }
   inline void SetState(PORTAL_STATE st){ state = st; }

   void SetOneWay(dword flags){
      prt_flags &= ~(PRTF_ONEWAY_IN | PRTF_ONEWAY_OUT);
      prt_flags |= flags & (PRTF_ONEWAY_IN | PRTF_ONEWAY_OUT);
   }

//----------------------------
// Check if portal is 'actice'. When 2 portals are connected, always only one of them
// is active, so that point transformation and collision testing is done only once.
   inline bool IsActive() const{
      return (this > (CPI3D_portal)connect_portal);
   }

   void Duplicate(CPI3D_portal);
public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(dword,NumVertices)() const{
      return vertices.size();
   }
   I3DMETHOD_(const S_vector*,GetVertices)() const{
      return &vertices.front(); 
   }
   I3DMETHOD_(const S_plane&,GetPlane)() const{
      return plane;
   }
   I3DMETHOD_(PI3D_sector,GetSector)(){ return owning_sector; }
   I3DMETHOD_(CPI3D_sector,GetSector)() const{ return owning_sector; }
   I3DMETHOD_(PI3D_sector,GetConnectSector)(){ return connect_sector; }
   I3DMETHOD_(CPI3D_sector,GetConnectSector)() const{ return connect_sector; }
   I3DMETHOD_(void,SetOn)(bool);
   I3DMETHOD_(bool,IsOn)() const{ return IsEnabled(); }
};

//----------------------------

class I3D_sector: public I3D_frame{

#define SCTF_IN_ENV_MAP_CREATE 1    //just creating envmap
#define SCTF_VISIBLE          2     //sector is visible in current view
#define SCTF_DRAW_TRIGGER     4     //was visible, draw specially
#define SCTF_PREPARED         8     //portals are prepared for current view
#define SCTF_RECURSE          0x10  //recursion help in CheckIntersectionRec
#define SCTF_PRIMARY          0x20  //primary sector - flag set during creation
#define SCTF_SIMPLE           0x40  //no portal capabilities - just logical sector
#define SCTF_LIGHT_LIST_RESET 0x80  //light_list reset, need to rebuild helper lists

   mutable dword sct_flags;

                              //sector skeleton
   C_buffer<S_vector> vertices;
   C_vector<I3D_face> faces;
                              //bound planes - kept for fast computation if point
                              // is inside of sector's area
   C_vector<S_plane> bound_planes; //normals of sector's planes point out of its area

   C_vector<PI3D_light> light_list;      //all non-fog lights set to this sector
   C_vector<PI3D_light> fog_light_list;  //all fog lights set to this sector
   PI3D_light fog_light;     //current fog light of sector, may be NULL
   C_vector<I3D_sound*> sound_list;
   dword sound_env_id;

                              //helper lists for faster light access
   C_vector<PI3D_light> lights_vertex, lights_dyn_lm, lights_shadow;

                              //portals which are owned by this sector
   C_vector<C_smart_ptr<I3D_portal> > own_portals;

                              //portals which this sector is connected to
   C_vector<C_smart_ptr<I3D_portal> > connected_portals;
   I3D_bbox bbox;             //bound-box, for faster point-in-sector detection.

   dword last_transform_time, last_visible_time;

                              //physiscs:
   float temperature;
   /*
                              //render target utilizing cube texture (for environment map)
   class C_cube_render_target: public C_render_target<1, I3D_texture>{
   public:
      enum E_SIDE{
         SIDE_LEFT,
         SIDE_RIGHT,
         SIDE_DOWN,
         SIDE_UP,
         SIDE_BACK,
         SIDE_FRONT,
      };
      dword last_build_time;
      C_cube_render_target():
         last_build_time(0)
      {}

//----------------------------
// Initalize camera, rtt, etc.
      I3D_RESULT Init(PI3D_scene, dword size, dword ct_flags);

//----------------------------
// Setup camera to look to particular side.
      void SetupCamera(PI3D_camera cam, const S_vector &pos, E_SIDE, const S_matrix *m_rot = NULL);

//----------------------------
// etup render target to be set to appropriate side texture.
      void SetupRenderTarget(PI3D_driver, E_SIDE);
   } env_map;
   */

   friend I3D_light;
   friend I3D_sound;

   void BuildPortals(const C_vector<S_vector> &verts, const C_vector<word> &prt_verts);

//----------------------------
   void BuildHelperLightLists();

//----------------------------
// Recursively check if polygon is visible to dest sector.
   bool CheckIntersectionRec(const S_vector *verts, dword num_verts, CPI3D_sector dest_sct) const;
   bool CheckIntersectionRec(const I3D_bsphere&, CPI3D_sector dest_sct) const;

//----------------------------
// Recursive func setting all neighbouring sectors visible.
   void SetVisibleRec(const S_view_frustum&, struct S_set_vis_link*);

//----------------------------
   void DisablePortalsRec(PI3D_sector);

public:
   I3D_sector(PI3D_driver, bool primary = false);
   ~I3D_sector();

   inline dword GetSctFlags() const{ return sct_flags; }
   inline void SetSctFlags(dword f){ sct_flags = f; }

   inline dword GetSoundEnvID() const{ return sound_env_id; }

   //PI3D_texture GetEnvironmentTexture(PI3D_scene);

//----------------------------
// Check if this is primary portal.
   inline bool IsPrimary1() const{ return (sct_flags&SCTF_PRIMARY); }

//----------------------------
// Build sector from a set of walls (visuals).
// The function returns I3D_OK if everything was built correctly, false if some error occured.
   I3D_RESULT Build(PI3D_frame *walls, dword num_walls, CPI3D_visual *hlp_portals, dword num_hlp_portals,
      bool simple, class C_loader &ld);

//----------------------------
// Build bounds ('bbox', 'face_normals', 'bound_planes') from current polyhedron 
// ('vertices', 'faces').
// Also I3D_occluder_base::BuildBounds is called.
   void BuildBounds();

//----------------------------
// Save current sector data (convex hull and portals) into a cache file.
   bool SaveCachedInfo(class C_cache*) const;

//----------------------------
// Load current sector data (convex hull and portals) from a cache file.
   bool LoadCachedInfo(class C_cache*);

//----------------------------
// Connect this sector (through equal portals) to any other sector(s) contained in
// given scene.
   bool Connect(PI3D_scene);

//----------------------------
// Disconnect this sector from connected sectors, and re-connect diconnected sectors.
   void Disconnect(PI3D_scene);

//----------------------------
// Add a portal into 'connected_portals' vector.
   void AddConnectedPortal(PI3D_portal);

//----------------------------
// Delete portal from 'connected_portals' vector.
   void DelConnectedPortal(PI3D_portal p);

//----------------------------
// Recursively reset visibility and transform state of sector. This is done after
// preprocessing children and put them into render list, so that another phase
// (possibly next frame, or mirror) may detect visibility again).
   void ResetRecursive();

//----------------------------
// Transform all sector's active portals into 2D and set their state flags.
   void PreparePortals(const S_view_frustum&);

//----------------------------
// Set this sector to be visible - recursive function called during determination
// of which sectors and portals are visible.
// This function is to be called once during rendered view for all visible sectors.
   void SetVisible(const S_view_frustum&);

//----------------------------
// Solve occlusion for all sectors linked to this sector - recursive function called after all occluders
// are preprocessed and after SetVisible was called.
// This function is to be called once during rendered view for all visible sectors.
   void SolveOcclusion(const C_vector<class I3D_occluder*> &occ_list);

//----------------------------
// Return current visibility state.
   inline bool IsVisible() const{
      if(!IsOn1()) return false;
      if(IsSimple()) return true;
      return (sct_flags&SCTF_VISIBLE);
   }

//----------------------------
// Check if specified portal collides (in 2D) with any of our active portals.
   bool CheckPortalCollision(CPI3D_portal) const;

//----------------------------
// Check if bounding volume (in local coords) is visible.
// This check includes:
// - test against view frustum (if 'clip_vf' is !NULL), returns clip info in '*clip_vf'
// - test against set of occluders (if 'clip_occ' is !NULL),     ''        in '*clip_occ'
// - test against sector's portals
// Note: clipping info is valid only if returned value is true (visible)
   bool IsBoundSphereVisible(const I3D_bsphere &bs_world, CPI3D_sector dest_sct,
      const S_preprocess_context &pc, bool *clip_vf, bool *clip_occ) const;

   bool IsBoundVolumeVisible(const C_bound_volume&, const S_matrix &tm, CPI3D_sector dest_sct,
      const S_preprocess_context &pc,
      bool *clip_vf, bool *clip_occ) const;
   
//----------------------------
// Get sector's fog light (if any).
   inline PI3D_light GetFogLight() const{ return fog_light; }

   void UpdateAllLights();

   inline const C_vector<PI3D_light> &GetLights1() const{ return light_list; }
   inline const C_vector<PI3D_light> &GetFogs() const{ return fog_light_list; }
   inline const C_vector<PI3D_light> &GetLights_vertex(){
      if(sct_flags&SCTF_LIGHT_LIST_RESET) BuildHelperLightLists();
      return lights_vertex;
   }
   inline const C_vector<PI3D_light> &GetLights_dyn_LM(){
      if(sct_flags&SCTF_LIGHT_LIST_RESET) BuildHelperLightLists();
      return lights_dyn_lm;
   }
   inline const C_vector<PI3D_light> &GetLights_shadow(){
      if(sct_flags&SCTF_LIGHT_LIST_RESET) BuildHelperLightLists();
      return lights_shadow;
   }

   void Draw1(PI3D_scene, bool skeleton, bool portals) const;

   inline bool IsSimple() const{ return (sct_flags&SCTF_SIMPLE); }

public:
   I3DMETHOD(DebugDraw)(PI3D_scene scene) const{
      Draw1(scene, true, true);
      return I3D_OK;
   }
   I3DMETHOD(Duplicate)(CPI3D_frame);

   I3DMETHOD(SetPos)(const S_vector&){ return I3DERR_UNSUPPORTED; }
   I3DMETHOD(SetScale)(float){ return I3DERR_UNSUPPORTED; }
   I3DMETHOD(SetRot)(const S_quat&){ return I3DERR_UNSUPPORTED; }
   I3DMETHOD(SetDir)(const S_vector&, float roll = 0.0f){ return I3DERR_UNSUPPORTED; }
   I3DMETHOD(SetDir1)(const S_vector &dir, const S_vector &up){ return I3DERR_UNSUPPORTED; }

public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(bool,CheckPoint)(const S_vector &v) const;
   I3DMETHOD_(bool,IsPrimary)() const{ return (sct_flags&SCTF_PRIMARY); }
                              //portals
   I3DMETHOD_(dword,NumPortals)() const{ return own_portals.size(); }
   I3DMETHOD_(const PI3D_portal*,GetPortals)() const{
      return own_portals.size() ? (PI3D_portal*)&own_portals.front() : NULL;
   }
   I3DMETHOD_(dword,NumConnectPortals)() const{ return connected_portals.size(); }
   I3DMETHOD_(const PI3D_portal*,GetConnectPortals)() const{
      return connected_portals.size() ? (PI3D_portal*)&connected_portals.front() : NULL;
   }
                              //lights
   I3DMETHOD(AddLight)(PI3D_light);
   I3DMETHOD(RemoveLight)(PI3D_light);
   I3DMETHOD_(dword,NumLights)() const{
      return light_list.size() + fog_light_list.size();
   }
   I3DMETHOD_(CPI3D_light,GetLight)(int index) const{
      return (PI3D_light)const_cast<PI3D_sector>(this)->GetLight(index);
   }
   I3DMETHOD_(PI3D_light,GetLight)(int index){
      int nl = light_list.size();
      if(index < nl)
         return light_list[index];
      index -= nl;
      if(index < (int)fog_light_list.size())
         return fog_light_list[index];
      return NULL;
   }
                              //sounds
   I3DMETHOD(AddSound)(I3D_sound*);
   I3DMETHOD(RemoveSound)(I3D_sound*);
   I3DMETHOD_(dword,NumSounds)() const{ return sound_list.size(); }
   I3DMETHOD_(I3D_sound* const*,GetSounds)(){ return &sound_list.front(); }
   I3DMETHOD_(CPI3D_sound const*,GetSounds)() const{ return &sound_list.front(); }

   I3DMETHOD_(void,SetEnvironmentID)(dword id);
   I3DMETHOD_(dword,GetEnvironmentID)() const{ return sound_env_id; }

   I3DMETHOD_(void,SetTemperature)(float f){ temperature = f; }
   I3DMETHOD_(float,GetTemperature)() const{ return temperature; }

   I3DMETHOD_(dword,NumBoundPlanes)() const{ return bound_planes.size(); }
   I3DMETHOD_(const S_plane*,GetBoundPlanes)() const{ return &bound_planes.front(); }
};

//----------------------------

#ifdef _DEBUG
inline PI3D_sector I3DCAST_SECTOR(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_SECTOR ? NULL : static_cast<PI3D_sector>(f); }
inline CPI3D_sector I3DCAST_CSECTOR(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_SECTOR ? NULL : static_cast<CPI3D_sector>(f); }
#else
inline PI3D_sector I3DCAST_SECTOR(PI3D_frame f){ return static_cast<PI3D_sector>(f); }
inline CPI3D_sector I3DCAST_CSECTOR(CPI3D_frame f){ return static_cast<CPI3D_sector>(f); }
#endif

//----------------------------

#endif

