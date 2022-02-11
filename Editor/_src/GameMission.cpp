#include "all.h"
#include "loader.h"
#include "gamemission.h"
#include "systables.h"
#include "game_cam.h"
#include "Script.h"
#include <IPhysics.h>

#include "TerrainDetail.h"
#include "winstuff.h"

//----------------------------

#define ACTION_SOUND_DIR "Action\\"

                              //debugging:

//#define DEBUG_LISTEN_SHOW  //draw listening

//----------------------------

#define GUN_SOUND_MIN 6.0f
#define GUN_SOUND_MAX 60.0f

//----------------------------

#define WRITEMESSAGEF_MAX_FMT_BUFFER 1024  //size of formatting buffer, in characters

//----------------------------

extern const char *APP_NAME;

//----------------------------
//----------------------------

extern const C_table_template templ_mission_cfg;


//----------------------------
//----------------------------

class C_game_mission_imp: public C_game_mission{
   //----------------------------
   // Class for keeping count of variations of played sounds. First time the method CountOf is called
   // for snd_name, it scans ACTION_SOUND_DIR in sound directory and counts number of sounds
   // with mask snd_name%d.wav; snd_name and count is then stored in map for future use;
   class C_action_snd_count{
      typedef map<C_str, dword> t_snd_count_map;
      t_snd_count_map snd_count_map;

      dword GetFileCount(const char *snd_name){
         for(int i = 1; i < 10; i++){
            PC_dta_stream dta = NULL;
            {
               C_str s1;
               s1 = C_fstr("%s"ACTION_SOUND_DIR"%s_%d.ogg", (const char*)sound_cache.GetDir(), snd_name, i);
               dta = DtaCreateStream(s1);
               if(!dta){
                  s1 = C_fstr("%s"ACTION_SOUND_DIR"%s_%d.wav", (const char*)sound_cache.GetDir(), snd_name, i);
                  dta = DtaCreateStream(s1);
               }
               if(dta)
                  dta->Release();
            }
            if(!dta)
               break;
         }
         return (i-1);
      }
   public:
   //----------------------------
   // Return number of variations of given sound.
      dword CountOf(const char *snd_name){

         C_str sn = snd_name;
 
         t_snd_count_map::iterator it = snd_count_map.find(sn);
         if(it!=snd_count_map.end())
            return (*it).second;
         int c = GetFileCount(snd_name);
#ifdef EDITOR
         if(c)                //in edit mode, if zero count, so store into map (allow designers to possibly add sound)
#endif
            snd_count_map[sn] = c;
         return c;
      }
   };

//----------------------------
                              //init data:

   C_vector<C_smart_ptr<C_checkpoint> > checkpoints;

   C_smart_ptr<I3D_model> weather_effect; //model of weather (rain or snow)


//----------------------------
                              //helpers (temp variables, counters, etc)
   C_action_snd_count action_snd_count;
#ifdef EDITOR
   bool cinematics_enabled;   //helper variable, used in editor
   dword debug_log_mask;
#endif
   int im_last_line_flash_count; //counter for flashing last line of text


//----------------------------
                              //composites (interfaces, vectors, etc):
   C_smart_ptr<C_game_camera> game_cam;
   C_smart_ptr<C_terrain_detail> terrain_detail;
   C_smart_ptr<IPH_world> phys_world;


//----------------------------
                              //run-time:
   E_MISSION_RESULT mission_over_result; //result of game, set by MissionOver call
   C_str mission_over_msg_id;


//----------------------------
   typedef map<C_str, C_smart_ptr<I3D_frame> > t_frm_map;

// Function creating mapping of frame name to its pointer, enumerating scene.
   void CreateFrameMap(t_frm_map &frm_map) const{

      struct S_hlp{
         t_frm_map *frm_map;
         static I3DENUMRET I3DAPI cbE(PI3D_frame frm, dword c){
            typedef map<C_str, C_smart_ptr<I3D_frame> > t_frm_map;
            t_frm_map &frm_map = *(t_frm_map*)c;
            frm_map[frm->GetName()] = frm;
            return I3DENUMRET_OK;
         }
      };
      scene->EnumFrames(S_hlp::cbE, (dword)&frm_map);
      frm_map[scene->GetPrimarySector()->GetName()] = scene->GetPrimarySector();
      frm_map[scene->GetBackdropSector()->GetName()] = scene->GetBackdropSector();
   }

//----------------------------

   static bool PhysContactQuery(CPI3D_volume src_vol, PIPH_body src_body, CPI3D_frame dst_frm, PIPH_body dst_body, void *context){
      return true;
   }

   static bool PhysContactReport(CPI3D_volume src_vol, PIPH_body src_body, CPI3D_frame dst_frm, PIPH_body dst_body,
      const S_vector &pos, const S_vector &normal, float depth, void *context){
      return true;
   }

//----------------------------
// Tick actors depending on current mission state.
   void TickActors(const S_tick_context &tc){

      assert(IsInGame());
                              //tick actors
      for(dword i=0; i<actors.size(); i++){
         PC_actor ap = actors[i];

         ap->Tick(tc);
                              //check if actor's not deleted 
         if(ap!=actors[i]) i--;
      }     
   }

//----------------------------

   virtual bool LoadChunk(CK_TYPE, S_load_context&);

//----------------------------

   void InitWeather(){

      if(!tab_config->ItemE(TAB_E_WEATHER_EFFECT))
         return;

      PI3D_model mod = I3DCAST_MODEL(scene->CreateFrame(FRAME_MODEL));

      const char *fname;
      switch(tab_config->ItemE(TAB_E_WEATHER_EFFECT)){
      case 1: fname = "system\\atmo rain"; break;
      case 2: fname = "system\\atmo snow"; break;
      default: assert(0); fname = NULL;
      }
      model_cache.Open(mod, fname, scene, 0, ErrReport, editor);
      PI3D_visual vis = I3DCAST_VISUAL(mod->FindChildFrame(NULL, ENUMF_VISUAL));
      if(vis && vis->GetVisualType()==I3D_VISUAL_ATMOS){
         weather_effect = mod;
         mod->LinkTo(scene->GetPrimarySector());

         S_vector dir;
         S_vector2 scl;
         float depth;
         switch(tab_config->ItemE(TAB_E_WEATHER_EFFECT)){
         case 1: dir = S_vector(0, -20, 4); scl = S_vector2(.02f, .8f); depth = 10; break;
         case 2: dir = S_vector(0, -1.4f, .5f); scl = S_vector2(.035f, .035f); depth = 10; break;
         default: dir.Zero(); scl.Zero(); assert(0);
         }

         vis->SetProperty(I3DPROP_ATMO_F_DIR_X, I3DFloatAsInt(dir.x));
         vis->SetProperty(I3DPROP_ATMO_F_DIR_Y, I3DFloatAsInt(dir.y));
         vis->SetProperty(I3DPROP_ATMO_F_DIR_Z, I3DFloatAsInt(dir.z));
         vis->SetProperty(I3DPROP_ATMO_I_NUM_ELEMS, tab_config->ItemI(TAB_I_WEATHER_DENSITY));
         vis->SetProperty(I3DPROP_ATMO_F_DEPTH, I3DFloatAsInt(15.0f));
         vis->SetProperty(I3DPROP_ATMO_F_SCALE_X, I3DFloatAsInt(scl.x));
         vis->SetProperty(I3DPROP_ATMO_F_SCALE_Y, I3DFloatAsInt(scl.y));
      }
      mod->Release();
   }

//----------------------------

   void InitTerrainDetail(){

      if(!tab_config->ItemB(TAB_B_TERRDET_USE))
         return;

      const char *mask = tab_config->ItemS(TAB_S20_TERRDET_MASK);
      if(*mask){
         S_terrdetail_init tdi; 

         tdi.visible_distance = tab_config->ItemF(TAB_F_TERRDET_VISIBILITY);
         C_vector<dword> set_map;
         for(int i=tab_config->ArrayLen(TAB_S20_TERRDET_MATMASK); i--; ){
            const char *mat_name = tab_config->ItemS(TAB_S20_TERRDET_MATMASK, i);
            if(*mat_name){
               dword set_i = tab_config->ItemE(TAB_E_TERRDET_MODELSET, i);
               assert(set_i < (tab_materials->ArrayLen(TAB_S24_MAT_MODELSET_NAME)/TAB_MAT_NUM_MODELS_IN_SET));
                              //check if we have already this one created
               for(int ii=tdi.model_sets.size(); ii--; ){
                  if(set_map[ii]==set_i)
                     break;
               }
               if(ii==-1){
                  set_map.push_back(set_i);
                              //store the model set
                  ii = tdi.model_sets.size();
                  tdi.model_sets.push_back(S_terrdetail_init::S_model_set());
                  S_terrdetail_init::S_model_set &ms = tdi.model_sets.back();

                  bool set_align = tab_materials->ItemB(TAB_B_MAT_MODELSET_ALIGN, set_i*TAB_MAT_NUM_MODELS_IN_SET);
                              //insert all models
                  for(dword i=1; i<TAB_MAT_NUM_MODELS_IN_SET; i++){
                     int indx = set_i*TAB_MAT_NUM_MODELS_IN_SET + i;
                     const char *mname = tab_materials->ItemS(TAB_S24_MAT_MODELSET_NAME, indx);
                     if(*mname){
                        float ratio = tab_materials->ItemF(TAB_F_MAT_MODELSET_RATIO, indx);
                        bool align = (set_align || tab_materials->ItemB(TAB_B_MAT_MODELSET_ALIGN, indx));
                        if(mname[0]!='#'){
                           ms.model_sources.push_back(S_terrdetail_init::S_model_set::S_model_source(mname, ratio, align));
                        }else{
                           dword set_i = atoi(mname+1);
                           if(set_i>=(tab_materials->ArrayLen(TAB_S24_MAT_MODELSET_NAME)/TAB_MAT_NUM_MODELS_IN_SET)){
                              ErrReport("Invalid terraindetail set number!", editor);
                              continue;
                           }
                           bool set_align = tab_materials->ItemB(TAB_B_MAT_MODELSET_ALIGN, set_i*TAB_MAT_NUM_MODELS_IN_SET);
                           for(dword i=1; i<TAB_MAT_NUM_MODELS_IN_SET; i++){
                              int indx = set_i*TAB_MAT_NUM_MODELS_IN_SET + i;
                              const char *mname = tab_materials->ItemS(TAB_S24_MAT_MODELSET_NAME, indx);
                              if(*mname){
                                 float r = tab_materials->ItemF(TAB_F_MAT_MODELSET_RATIO, indx) * ratio;
                                 bool al = (set_align || tab_materials->ItemB(TAB_B_MAT_MODELSET_ALIGN, indx));
                                 ms.model_sources.push_back(S_terrdetail_init::S_model_set::S_model_source(
                                    mname, r, (al || align)));
                              }
                           }
                        }
                     }
                  }
                  ms.density = tab_materials->ItemF(TAB_F_MAT_MODELSET_RATIO, set_i*TAB_MAT_NUM_MODELS_IN_SET);
               }
               if(tdi.model_sets[ii].model_sources.size())
                  tdi.model_sets[ii].material_dests.push_back(mat_name);
               else
                  ErrReport("Used empty terraindetail set!", editor);
            }
         }

         tdi.terrain_name_base = mask;

         terrain_detail = CreateTerrainDetail(scene, tdi);
         terrain_detail->Release();
      }
   }

//----------------------------
// Enumeration function called by GameBegin on all frames - used to initialize frames, actors, scripts, etc.
   static I3DENUMRET I3DAPI cbGameBegin(PI3D_frame frm, dword c){

      C_game_mission_imp &mission = *(C_game_mission_imp*)c;

      S_frame_info *fi = GetFrameInfo(frm);
      if(fi && fi->vm){
                        //call script's GameBegin function
         script_man->RunFunction(frm, "GameBegin", &mission);
      }
      return I3DENUMRET_OK;
   }

//----------------------------

   PC_actor CreatePlayer(){

      PC_actor hero = NULL;
                              //initialize hero
      PI3D_frame hero_loc = scene->FindFrame("hero", ENUMF_DUMMY);
      if(hero_loc){
         hero = CreateActor(hero_loc, ACTOR_PLAYER);
      }
      return hero;
   }

//----------------------------

public:
   C_game_mission_imp()
   {
   }

   virtual ~C_game_mission_imp(){
      Close();
      tab_config->Release();
   }

//----------------------------

   virtual dword GetID() const{ return TICK_CLASS_ID_GAME_MISSION; }

//----------------------------

   virtual C_game_camera *GetGameCamera(){ return game_cam; }
   virtual const C_game_camera *GetGameCamera() const{ return game_cam; }

//----------------------------

   virtual E_MISSION_IO Open(const char *dir, dword open_flags,
      I3D_LOAD_CB_PROC *cb_proc = NULL, void *load_cb_context = 0){

      if(open_flags&OPEN_MERGE_MODE){
      }else{
         tab_config->Load(&templ_mission_cfg, TABOPEN_TEMPLATE);
      }
      E_MISSION_IO mio = C_mission::Open(dir, open_flags, cb_proc, load_cb_context);

      return mio;
   }

//----------------------------

   virtual void Close(dword close_flags = 0);

   virtual void GameBegin();
   virtual void GameEnd();
   virtual void Tick(const S_tick_context &tc);
   virtual void Render();

//----------------------------

   virtual PC_actor CreateActor(PI3D_frame frm, E_ACTOR_TYPE, const void *data = NULL);

//----------------------------
// Destroy specified actor.
   virtual void DestroyActor(PC_actor);

//----------------------------

   virtual bool MissionOver(E_MISSION_RESULT gr, const char *msg_id){

      switch(mission_state){
      case MS_IN_GAME:
         {
            mission_state = MS_QUIT_REQUEST;
            mission_over_result = gr;
            mission_over_msg_id = msg_id;
            return true;
         }
         break;
      default:
         {
                              //valid state, it is possible to re-enter; just ignore
            //assert(0);
         }
      }
      return false;
   }

//----------------------------

   virtual CPC_actor FindActor(const char *name) const{

      for(dword i=0; i<actors.size(); i++){
         if(!strcmp(actors[i]->GetName(), name))
            return actors[i];
      }
      return NULL;
   }

   virtual PC_actor FindActor(const char *name){
      return (PC_actor)((const C_game_mission_imp*)this)->FindActor(name);
   }

//----------------------------

   virtual PI3D_sound PlaySound(const char *snd_name, float min_dist, float max_dist,
      PI3D_frame link_to = NULL, const S_vector &pos = S_vector(0.0f, 0.0f, 0.0f), float volume = 1.0f){

      if(!isound)
         return NULL;

      if(volume<0.0f || volume> 1.001f){
         ErrReport(C_fstr("C_game_mission::PlaySound: invalid volume: %.3f", volume), editor);
      }

      PI3D_sound snd = I3DCAST_SOUND(scene->CreateFrame(FRAME_SOUND));
      I3D_RESULT ir = sound_cache.Open(snd, snd_name, scene, 0, ErrReport, editor);
      if(I3D_SUCCESS(ir)){
         snd->SetSoundType(I3DSOUND_POINT);
         snd->SetVolume(volume);
         snd->SetLoop(false);

         S_effect_init ei;
         ei.pos = pos;

         if(!link_to){
            link_to = scene->GetPrimarySector();
         }else{
            float r_scl = 1.0f / link_to->GetMatrix()(0).Magnitude();
            min_dist *= r_scl;
            max_dist *= r_scl;
         }
         ei.link_to = link_to;
         snd->SetRange(min_dist, max_dist);
         snd->SetOn(true);
         //snd->LinkTo(scene->GetPrimarySector());

         ei.mode_flags = EFFECT_AUTO_DESTROY;
         CreateActor(snd, ACTOR_EFFECT, &ei);

         snd->Release();
         return snd;
      }else{
         snd->Release();
         return NULL;
      }
   }

//----------------------------

   virtual I3D_RESULT PlayAmbientSound(const char *snd_name, float volume = 1.0f){
   
      if(!isound)
         return I3D_OK;

      PI3D_sound snd = I3DCAST_SOUND(scene->CreateFrame(FRAME_SOUND));
      I3D_RESULT ir = sound_cache.Open(snd, snd_name, scene, I3DLOAD_SOUND_SOFTWARE, ErrReport, editor);
      if(I3D_SUCCESS(ir)){
         snd->SetSoundType(I3DSOUND_AMBIENT);
         snd->SetVolume(volume);
         snd->SetLoop(false);
         snd->SetOn(true);

         S_effect_init ei;
         ei.mode_flags = EFFECT_AUTO_DESTROY;
         CreateActor(snd, ACTOR_EFFECT, &ei);
      }else{
         ErrReport(C_fstr("Can't load sound: '%s'", snd_name), editor);
      }
      snd->Release();
      return ir;
   }

//----------------------------
                              //actions sounds mapping
                              // key: first is 1st material, second is 2nd material
                              // value: base sound name, plus number of variations
                              //key.second: value E_ACTION_ID_HIT means hit group,
                              // value E_ACTION_ID_STEP means step group
   struct S_action_snd_entry{
      C_str name_base;
      mutable dword num_vars;
      float volume;

      S_action_snd_entry():
         num_vars(0),
         volume(1.0f)
      {}

      void MakeCount() const{

         for(dword i = 1; i < 10; i++){
            PC_dta_stream dta = NULL;
            {
               C_str s1;
               s1 = C_str(C_xstr("%"ACTION_SOUND_DIR"%_%.ogg") %sound_cache.GetDir() %name_base %i);
               dta = DtaCreateStream(s1);
               if(!dta){
                  s1 = C_str(C_xstr("%"ACTION_SOUND_DIR"%_%.wav") %sound_cache.GetDir() %name_base %i);
                  dta = DtaCreateStream(s1);
               }
               if(dta)
                  dta->Release();
            }
            if(!dta)
               break;
         }
         num_vars = i-1;
      }
   public:
   //----------------------------
   // Return number of variations of given sound.
      dword NumVariations() const{

         if(!num_vars)
            MakeCount();
         return num_vars;
      }
   };

   typedef map<pair<dword, dword>, S_action_snd_entry> t_action_sounds;
   t_action_sounds action_sounds;

//----------------------------
// Load and init action sound table from text file.
   void InitActionSoundTable(){

      action_sounds.clear();
      
      C_cache ck;
      dword line_num = 0;

      try{
         if(ck.open("Tables\\ActionSounds.txt", CACHE_READ)){
            char line[512], *cp;

            while(!ck.eof()){
               ck.getline(line, sizeof(line));
               ++line_num;
               for(cp=line; *cp && isspace(*cp); ++cp);
               if(!*cp || *cp==';') continue;
                              //format: first(mat) second(mat) sound_name [volume]
               pair<dword, dword> mat_ids;
               C_str snd_name;
               float volume = 1.0f;
                              //read all params
               for(dword i=0; i<4 && *cp; i++){
                  char buf[256];
                  char *n = buf;
                  bool quoted = (*cp=='\"');
                  if(quoted)
                     ++cp;
                  while(true){
                     char c = *cp++;
                     if(quoted){
                        if(c=='\"')
                           break;
                     }else{
                        if(c==' ')
                           break;
                        if(c==0){
                           --cp;
                           break;
                        }
                     }
                     *n++ = c;
                  }
                  *n = 0;
                  while(isspace(*cp)) ++cp;

                  switch(i){
                  case 0:
                  case 1:
                              //detect material id
                     if(i==1 && *buf=='*'){
                        if(!strcmp(buf+1, "hit"))
                           mat_ids.second = E_ACTION_ID_HIT;
                        else
                        if(!strcmp(buf+1, "step"))
                           mat_ids.second = E_ACTION_ID_STEP;
                        else{
                           throw C_except(C_xstr("invalid material name: '%'") %buf);
                        }
                     }else{
                        int id = -1;
                        for(dword j=0; j<tab_materials->ArrayLen(TAB_S32_MAT_NAME); j++){
                           const char *mn = tab_materials->ItemS(TAB_S32_MAT_NAME, j);
                           if(!strcmp(mn, buf)){
                              id = j;
                              break;
                           }
                        }
                        if(id == -1){
                           throw C_except(C_xstr("invalid material name: '%'") %buf);
                        }
                        /*
                        int id1 = tab_materials->ItemI(TAB_I_MAT_SOUND_LIKE, id);
                        if(id1!=-1)
                           id = id1;
                           */
                        (i==0 ? mat_ids.first : mat_ids.second) = id;
                     }
                     break;

                  case 2:
                              //this is sound name
                     snd_name = buf;
                     break;

                  case 3:
                              //volume (optional)
                     if(*buf){
                        if(sscanf(buf, "%f", &volume)!=1 || volume<0.0f || volume>1.0f)
                           throw C_except(C_xstr("invalid volume: '%'") %buf);
                     }
                     break;
                  }
               }
                              //always make 1st to be smaller
               if(mat_ids.first > mat_ids.second)
                  swap(mat_ids.first, mat_ids.second);

                              //add to map (check for duplication)
               pair<t_action_sounds::iterator, bool> p = action_sounds.insert(pair<pair<dword, dword>, S_action_snd_entry>(mat_ids, S_action_snd_entry()));
               if(!p.second)
                  throw C_except(C_xstr("entry duplication: '%'") %line);
               S_action_snd_entry &se = p.first->second;
               se.name_base = snd_name;
               se.volume = volume;
            }
         }
      } catch(const C_except &e){
         ErrReport(C_xstr("Parsing file 'ActionSounds.txt' (line %) error: %") %line_num %e.what(), editor);
         action_sounds.clear();
      }
   }

//----------------------------

   virtual PI3D_sound PlayActionSound(const S_vector &pos, dword mat_id1, dword mat_id2, CPC_actor emmitor, float volume){

      if(!action_sounds.size())
         InitActionSoundTable();

                              //make 'sounds like' material mapping
      {
         int i = tab_materials->ItemI(TAB_I_MAT_SOUND_LIKE, mat_id1);
         if(i!=-1)
            mat_id1 = i;
         if(mat_id2 < E_ACTION_ID_HIT){
            i = tab_materials->ItemI(TAB_I_MAT_SOUND_LIKE, mat_id2);
            if(i!=-1)
               mat_id2 = i;
         }
      }

      pair<dword, dword> key(mat_id1, mat_id2);
      if(key.first > key.second)
         swap(key.first, key.second);
      t_action_sounds::const_iterator it = action_sounds.find(key);
      if(it==action_sounds.end()){
                              //if not found, map 1st ID to default and retry
         if(mat_id1!=0){
            key.first = 0;
            key.second = mat_id2;
            it = action_sounds.find(key);
         }
      }
      if(it==action_sounds.end()){
         const char *mn1 = mat_id1<tab_materials->ArrayLen(TAB_S32_MAT_NAME) ? tab_materials->ItemS(TAB_S32_MAT_NAME, mat_id1) : "unknown";
         const char *mn2 = mat_id2<tab_materials->ArrayLen(TAB_S32_MAT_NAME) ? tab_materials->ItemS(TAB_S32_MAT_NAME, mat_id2) :
            (mat_id2==E_ACTION_ID_HIT) ? "*hit" :
            (mat_id2==E_ACTION_ID_STEP) ? "*step" :
            "unknown";
         ErrReport(C_xstr("PlayActionSound: no sound defined for materials '%' and '%'") %mn1 %mn2,
            editor);
         return NULL;
      }
      const S_action_snd_entry &se = it->second;

      const float MIN_DIST = 8.0f, MAX_DIST = 28.0f;

      PI3D_sound ret = NULL;
      if(isound && se.name_base.Size()){
                              //play physical sound

                              //optimization - check distance to camera, don't play sounds which are too far
         CPI3D_camera cam = scene->GetActiveCamera();
         if(cam){
            S_vector dir_to_cam = cam->GetWorldPos() - pos;
            if(dir_to_cam.Dot(dir_to_cam) < (MAX_DIST*MAX_DIST)){
         
               dword num_variations = se.NumVariations();
               if(!num_variations){
                  ErrReport(C_xstr("PlayActionSound: sound '%' has no files to play") %se.name_base, editor);
               }else{
                  volume *= se.volume;
                  C_str full_name(C_xstr(ACTION_SOUND_DIR"%_%") %se.name_base %(S_int_random(num_variations)+1));
                  ret = PlaySound(full_name, MIN_DIST, MAX_DIST, NULL, pos, Min(1.0f, volume));
               }
            }
         }
      }
      return ret;
   }

//----------------------------

   virtual const C_vector<C_smart_ptr<C_checkpoint> > &GetCheckpoints() const{ return checkpoints; }

//----------------------------

   virtual CPC_checkpoint FindCheckpoint(const char *name) const{

      if(!name || !*name)
         return NULL;
      for(int i=checkpoints.size(); i--; ){
         const C_str &str = checkpoints[i]->GetName();
         if(str.Match(name))
            return checkpoints[i];
      }
      return NULL;
   }

//----------------------------

   virtual PC_checkpoint CreateCheckpoint(){

      assert(checkpoints.size() <= 0x10000);
      PC_checkpoint cp = new C_checkpoint;
      cp->index = checkpoints.size();
      checkpoints.push_back(cp);
      cp->Release();
      return cp;
   }

//----------------------------

   virtual void DestroyCheckpoint(CPC_checkpoint cp){

      for(int i=checkpoints.size(); i--; )
         if(checkpoints[i]==cp) break;
      assert(i!=-1);
      checkpoints[i] = checkpoints.back();
      checkpoints[i]->index = i;
      checkpoints.pop_back();
   }

//----------------------------

   virtual void PrepareCheckpoints(){

                              //reset indicies, compute distances
      for(dword i=0; i<checkpoints.size(); i++){
         PC_checkpoint cp = checkpoints[i];
         cp->index = i;
         int numc = cp->NumConnections();
         for(int j=numc; j--; ){
            C_checkpoint::S_connection &con = cp->connections[j];
            con.distance = (cp->GetPos() - con.con_cp->GetPos()).Magnitude();
         }
      }
   }

//----------------------------

   virtual class IPH_world *GetPhysicsWorld(){

      if(!phys_world){
         phys_world = IPHCreateWorld();
         phys_world->SetGravity(S_vector(0, -.5f, 0));
         phys_world->SetERP(.2f); 
         phys_world->SetCFM(1e-5f);
         /*
#ifdef EDITOR
         phys_world->SetErrorHandler(IPHMessage);
         phys_world->SetDebugLineFunc(IPHDebugLine);
         phys_world->SetDebugPointFunc(IPHDebugPoint);
         phys_world->SetDebugTextFunc(IPHDebugText);
#endif
                              //feed material params
         IPH_surface_param mats[MAX_MATERIALS];
         dword num_mats = 0;
         for(dword i=0; i<MAX_MATERIALS; i++){
            if(*tab_materials->ItemS(TAB_S32_MAT_NAME, i)){
               IPH_surface_param &m = mats[i];
               float f = tab_materials->ItemF(TAB_F_MAT_FRICTION, i);
               m.mode |= IPH_CONTACT_SOFT_ERP;
               m.coulomb_friction = ((float)cos(PI+f*PI)+1.0f) * 1.0f;
               m.soft_erp = .9f;

               num_mats = i+1;
            }
         }
         phys_world->SetMaterials(mats, num_mats);
         */
         phys_world->Release();
      }
      return phys_world;
   }

//----------------------------

   virtual bool LoadPhysicsTemplate(const char *fname, S_phys_template &t) const{

      C_chunk ck;
      if(ck.ROpen(C_xstr("Missions\\%") % fname) && ++ck==CT_BASECHUNK){
         bool done = false;
         while(!done && ck)
         switch(++ck){
         case CT_PHYSICS_TEMPLATE:
            t.Load(ck);
            done = true;
            break;
         default: --ck;
         }
      }else
         return false;

      return true;
   }

//----------------------------
};

//----------------------------

C_game_mission::C_game_mission():
   mission_state(MS_EDIT),
   tab_config(CreateTable())
{
   tab_config->Load(&templ_mission_cfg, TABOPEN_TEMPLATE);
   //game_cam->Init(this);
}

//----------------------------

bool C_game_mission_imp::LoadChunk(CK_TYPE in_ck_t, S_load_context &lc){

   switch(in_ck_t){

   case CT_MISSION_TABLE:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_SETTINGS)) break;
      if(lc.open_flags&OPEN_MODEL) break;
      tab_config->Load(lc.ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
      break;

   case CT_ACTOR:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_ACTORS)) break;
      if(lc.open_flags&OPEN_MODEL) break;
      {
         C_str str;
         map<C_str, PI3D_frame>::const_iterator it_fmap;
                           //read name and type
         if(lc.ck.ReadString(CT_NAME, str)){
            word atw = 0;
            lc.ck.Read((char*)&atw, sizeof(word));

                              //find frame on which we're going to create the actor
            it_fmap = lc.frame_map.find(str);
            PI3D_frame frm = (it_fmap!=lc.frame_map.end()) ? (*it_fmap).second : NULL;

            bool ok = false;
            if(frm){
               PS_frame_info fi = GetFrameInfo(frm);
               if(fi && fi->actor){
                  lc.ErrLog(C_fstr("Internal error - frame '%s' already had actor set.", (const char*)str), NULL);
               }else{
                  PC_actor actor = CreateActor(frm, (E_ACTOR_TYPE)atw);
                  fi = GetFrameInfo(frm);
                  if(actor){
                     ok = true;
                     while(lc.ck){
                        switch(++lc.ck){
                        case CT_ACTOR_TABLE:
                           {
                                                //allow editing actor's table
                              PC_table tab = const_cast<PC_table>(actor->GetTable());
                              if(tab){
                                 tab->Load(lc.ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
                              }else{
                                 lc.WarnLog(C_fstr("Ignoring redundant table on actor '%s'", (const char*)str), NULL);
                              }
                              --lc.ck;
                           }
                           break;
                        default:
                           --lc.ck;
                        }
                     }
                     //actor->MissionLoad(&lc.ck);
                  }
               }
            }
            if(!ok){
               lc.ErrLog(C_fstr("Cannot create actor on frame '%s'", (const char*)str), NULL);
            }
         }
      }
      break;

   case CT_SCRIPT:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_SCRIPTS)) break;
      if(lc.open_flags&OPEN_MODEL) break;
      {
         if(!(lc.open_flags&OPEN_NO_SHOW_PROGRESS)){
            if(lc.ShowProgress()){
               lc.cancelled = true;
               return false;
            }
         }
         PI3D_frame frm = NULL;
         C_str str, script_name;
         map<C_str, PI3D_frame>::const_iterator it_fmap;

         while(lc.ck){
            switch(++lc.ck){
            case CT_NAME:
               {
                  str = lc.ck.RStringChunk();
                  it_fmap = lc.frame_map.find(str);
                  frm = (it_fmap!=lc.frame_map.end()) ? (*it_fmap).second : NULL;
                  //frm = lc.frame_map[str];
                  if(!frm){
                     lc.ErrLog(C_fstr("Cannot set script, frame not found: '%s'", (const char*)str), NULL);
                  }
               }
               break;

            case CT_SCRIPT_NAME:
               {
                  script_name = lc.ck.RStringChunk();
                  if(!frm)
                     break;
                  if(script_name[0ul]=='*'){
                              //mission-local, add mission directory
                     script_name = C_fstr("%s\\%s", (const char*)lc.open_dir, &script_name[1ul]);
                  }
                  /*
#ifdef EDITOR
                  if(!(lc.open_flags&OPEN_NO_EDITOR)){
                              //check if script is up-to-date
                     bool utd = lc.e_script->Action(E_SCRIPT_IS_UP_TO_DATE, (void*)(const char*)script_name);
                     if(!utd){
                        lc.e_script->Action(E_SCRIPT_COMPILE, (void*)(const char*)script_name);
                     }
                  }
#endif
                  */
                  C_script_manager::E_SCRIPT_RESULT sr = script_man->LoadScript(frm, script_name, this, false);
                              //report possible problem to user
                  const char *err = NULL;
                  if(sr<0)
                  switch(sr){
                  case C_script_manager::SR_LOAD_ERROR: err = "file not found"; break;
                  case C_script_manager::SR_LINK_ERROR: err = "link error"; break;
                  default: err = "unknown error";
                  }
                  if(err){
                     lc.ErrLog(C_fstr("Script '%s' open failed: %s.", (const char*)script_name, err), NULL);
                  }
               }
               break;

            case CT_SCRIPT_TABLE:
               {           
                  if(frm){
                              //load table regardless if script is set
                     S_frame_info *fi = GetFrameInfo(frm);
                     if(fi && fi->vm && fi->vm->GetTable()){
                        /*
                              //load table regardless if script is set
                        if(!fi->tab){
                           fi->tab = CreateTable();
                           fi->tab->Release();
                        }
                        */
                        fi->vm->GetTable()->Load(lc.ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
                     }else{
                        lc.WarnLog(C_fstr("Frame '%s': ignoring redundant table.", (const char*)frm->GetName()), NULL);
                     }
                  }
               }
               --lc.ck;
               break;

            default:
               --lc.ck;
            }
         }
         if(frm && script_name.Size()){
            script_man->RunFunction(frm, "Main", this);
         }
      }
      break;

   case CT_NUM_CHECKPOINTS:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_CHECKPOINTS)) break;
      if(lc.open_flags&OPEN_MODEL) break;
      {
         int num_cpts;
         lc.ck.Read(&num_cpts, sizeof(int));
         assert(!checkpoints.size());
         checkpoints.reserve(num_cpts);
      }
      break;

   case CT_CHECKPOINT:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_CHECKPOINTS)) break;
      if(lc.open_flags&OPEN_MODEL) break;
      {
         PC_checkpoint cp = CreateCheckpoint();
         while(lc.ck){
            switch(++lc.ck){
            case CT_CHKPT_POS:
               cp->SetPos(lc.ck.RVectorChunk());
               break;

            case CT_CHKPT_TYPE:
               cp->SetType((E_CHECKPOINT_TYPE)lc.ck.RByteChunk());
               break;

            case CT_CHKPT_NAME:
               cp->SetName(lc.ck.RStringChunk());
               break;

            case CT_CHKPT_CONNECTION:
               {
                  PC_checkpoint con_cp = NULL;
                  dword ct = CPT_DEFAULT;
                              //read all info about connection
                  while(lc.ck){
                     switch(++lc.ck){
                     case CT_CP_CON_ID:
                        {
                           dword con_id = 0;
                           lc.ck.Read((char*)&con_id, sizeof(word));
                           assert(con_id < checkpoints.size());
                           con_cp = checkpoints[con_id];
                           --lc.ck;
                        }
                        break;
                     case CT_CP_CON_TYPE:
                        ct = lc.ck.RByteChunk();
                        break;
                     default:
                        --lc.ck;
                     }
                  }
                  if(con_cp){
                     cp->Connect(con_cp, ct);
                  }
                  --lc.ck;
               }
               break;
            default:
               --lc.ck;
            }
         }
      }
      break;

   default:
      return C_mission::LoadChunk(in_ck_t, lc);
   }
   return true;
}

//----------------------------

void C_game_mission_imp::Close(dword close_flags){

#ifdef EDITOR
   if(!(close_flags&OPEN_NO_EDITOR))
   if(editor)
      editor->Broadcast(&C_editor_item::BeforeFree);
#endif
                              //quit game mode
   if(IsInGame())
      GameEnd();
#ifdef USE_GAMESPY
   assert(!gs_init_ok);       //must be done with GameSpy now
#endif

                              //remove scripts of all ours frames
   struct S_hlp{
      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
         S_frame_info *fi = GetFrameInfo(frm);
         if(fi && fi->vm)
            script_man->FreeScript(frm, (PC_game_mission)c, true);
         return I3DENUMRET_OK;
      }
   };
   //S_hlp::cbEnum(scene->GetPrimarySector(), (dword)NULL);
   //scene->EnumFrames(S_hlp::cbEnum, (dword)NULL);
   scene->EnumFrames(S_hlp::cbEnum, (dword)this);

   actors.clear();
   checkpoints.clear();

   C_mission::Close();
}

//----------------------------

void C_game_mission_imp::GameBegin(){

   assert(!IsInGame());
   mission_state = MS_IN_GAME;

   game_cam = CreateGameCamera(scene);
   game_cam->Release();
   game_cam->SetDistance(2.0f);

   model_cache.PushWorkMission(this);
   anim_cache.PushWorkMission(this);

   PrepareCheckpoints();

   level_time = 0;

   {
      PI3D_camera cam = I3DCAST_CAMERA(scene->CreateFrame(FRAME_CAMERA));
      cam->SetName("<game camera>");
      cam->SetRange(CAM_NEAR_RANGE, GetCameraRange());
      cam->SetFOV(GetCameraFOV());
      game_cam->SetCamera(cam);
      //game_cam->ResetPosition(); //04.04.2002
#ifdef EDITOR
      if(editor){
         PI3D_camera scn_cam = scene->GetActiveCamera();
         if(scn_cam){
            cam->SetPos(scn_cam->GetWorldPos());
            cam->SetRot(scn_cam->GetWorldRot());
         }
      }
#endif
      scene->SetActiveCamera(cam);
      cam->Release();
   }

   CreatePlayer();

                              //init actors
   for(int i=actors.size(); i--; )
      actors[i]->GameBegin();

                              //GameBegin() in scripts
   GetScene()->EnumFrames(cbGameBegin, (dword)this);

                              //init terrain detail. 
                              //23.9.2002 : Leave it before InitWeather for now, because
                              //atmospheric invalidate Scene bounding box, which InitTerrainDetail assume to be valid.
   InitTerrainDetail();
                              //initialize weather effect (if any)
   InitWeather();

   game_cam->ResetPosition();

   {
      S_tick_context tc; tc.Zero();
      Tick(tc);
   }
   igraph->NullTimer();
}

//----------------------------

void C_game_mission_imp::GameEnd(){

   assert(IsInGame());
   scene->AttenuateSounds();

   phys_world = NULL;

   model_cache.PopWorkMission(this);
   anim_cache.PopWorkMission(this);

   typedef map<C_str, C_smart_ptr<I3D_frame> > t_frm_map;
   t_frm_map frm_map;
                              //call GameEnd in scripts
                              // (also create frame map for faster later enumerations)
   struct S_hlp{
      PC_game_mission _this;

      t_frm_map *frm_map;
      static I3DENUMRET I3DAPI cbE(PI3D_frame frm, dword c){

         S_hlp *hp = (S_hlp*)c;
         S_frame_info *fi = GetFrameInfo(frm);
         if(fi && fi->vm){
            script_man->RunFunction(frm, "GameEnd", hp->_this);
                              //remove any running threads of this frame
            script_man->ClearAllThreads(frm);
         }
         /*
         if(fi && (fi->flags&FRM_INFO_ACTIVE_OBJECT)){
            fi->flags &= ~FRM_INFO_ACTIVE_OBJECT;
            fi = SetFrameInfo(frm, fi);
         }
         */
         return I3DENUMRET_OK;
      }
   } hlp = { this, &frm_map };
   scene->EnumFrames(S_hlp::cbE, (dword)&hlp);
                              //call back-to-front, actors may be destroying themselved during the call
   for(int i=actors.size(); i--; )
      actors[i]->GameEnd();

   weather_effect = NULL;
   terrain_detail = NULL;

   {
      game_cam->SetCamera(NULL);
      game_cam->SetFocus(NULL, NULL);
#ifdef EDITOR
      if(editor){
         PC_editor_item_MouseEdit me = (PC_editor_item_MouseEdit)editor->FindPlugin("MouseEdit");
         PI3D_camera cam = me->GetEditCam();
         scene->SetActiveCamera(cam);
      }
#endif
   }
   
   game_cam = NULL;

   mission_state = MS_EDIT;
}

//----------------------------

void C_game_mission_imp::Tick(const S_tick_context &tc){

   scene->Tick(tc.time);

   script_man->Tick(tc.time, this);
                                             
   switch(mission_state){
   case MS_IN_GAME:
      {
         level_time += tc.time;
         if(tc.p_ctrl){
#ifdef EDITOR
                                 //update camera mode
            if(tc.p_ctrl->Get(CS_CAM_ZOOM_IN, true)){
               {
                  float d = game_cam->GetDistance();
                  d -= 2.0f;
                  if(d <= 1.0f){
                     d = 0.0f;
                  }
                  game_cam->SetDistance(d);
               }
            }else
            if(tc.p_ctrl->Get(CS_CAM_ZOOM_OUT, true)){
               {
                  float d = game_cam->GetDistance();
                  d = Min(8.0f, d + 2.0f);
                  game_cam->SetDistance(d);
               }
            }
            /*
            if(tc.p_ctrl->Get(CS_CAM_ZOOM_UP, true)){
               if(game_cam->GetAngleMode()!=2)
                  game_cam->SetAngleMode((game_cam->GetAngleMode() + 1) % 3);
            }else
            if(tc.p_ctrl->Get(CS_CAM_ZOOM_DOWN, true)){
               if(game_cam->GetAngleMode()!=0)
                  game_cam->SetAngleMode((game_cam->GetAngleMode() + 2) % 3);
            }
            */
#endif//EDITOR
         }
         TickActors(tc);

         game_cam->Tick(tc.time);
      }
      break;

   case MS_GAME_OVER:
      {
         S_tick_context tc1 = tc;
         tc1.ClearInput();
         TickActors(tc1);
      }
      break;

   case MS_EDIT:
      break;

   default:
      {
         TickActors(tc);
      }
   }

   if(weather_effect){
                              //enable/disable depending on where game camera is located
      PI3D_camera cam = game_cam->GetCamera();
      PI3D_sector sct = cam->GetSector();
      if(sct){
         weather_effect->SetOn(sct==scene->GetPrimarySector());
      }
   }

#if defined _DEBUG && 1
   if(tc.p_ctrl && tc.p_ctrl->Get(CS_DEBUG, true)){
      driver->EvictResources();
   }
#endif
                              //update physics
   if(phys_world)
      phys_world->Tick(tc.time, scene, PhysContactQuery, PhysContactReport, NULL);

                              //terrain detail
   if(terrain_detail){
      terrain_detail->Update(tc.time,
         scene->GetActiveCamera());
   }
}

//----------------------------

void C_game_mission_imp::Render(){

   driver->BeginScene();

   C_mission::Render();

#ifdef EDITOR
   if(editor) editor->Render();
#endif

   driver->EndScene();
}

//----------------------------

PC_actor CreateEffectActor(C_game_mission&, PI3D_frame, const S_effect_init*, C_chunk *ck_savegame);
PC_actor CreatePlayerActor(C_game_mission&, PI3D_frame, const void*);
PC_actor CreateDierActor(C_game_mission &gm, PI3D_frame frm);
PC_actor CreateActorPhysics(C_game_mission &gm, PI3D_frame frm);
PC_actor CreateVehicleActor(C_game_mission &gm1, PI3D_frame in_frm);

//----------------------------

PC_actor C_game_mission_imp::CreateActor(PI3D_frame in_frm, E_ACTOR_TYPE in_type, const void *data){

   PC_actor ap = NULL;
   switch(in_type){
   case ACTOR_EFFECT: ap = CreateEffectActor(*this, in_frm, (S_effect_init*)data, NULL); break;
   case ACTOR_PLAYER: ap = CreatePlayerActor(*this, in_frm, data); break;
   case ACTOR_DIER: ap = CreateDierActor(*this, in_frm); break;
   case ACTOR_PHYSICS: ap = CreateActorPhysics(*this, in_frm); break;
   case ACTOR_VEHICLE: ap = CreateVehicleActor(*this, in_frm); break;
   default:
      ErrReport("C_game_mission::CreateActor: Cannot create actor!", editor);
   }
   if(ap){
      actors.push_back(ap);
      ap->Release();          //'actors' C_vector is keeping ref of this
   }
   return ap;
}

//----------------------------

void C_game_mission_imp::DestroyActor(PC_actor act){

   for(int i=actors.size(); i--; )
   if(act==actors[i]){
      actors[i] = actors.back(); actors.pop_back();
      break;
   }
                              //we're not accepting deleting unknown actor
   assert(i!=-1);
}

//----------------------------

PC_game_mission CreateGameMission(){ return new C_game_mission_imp; }

//----------------------------
//----------------------------

