#include "all.h"
#include "loader.h"
#include "common.h"
//#include "script.h"
#include "gamemission.h"


#define VERSION_MAJOR 2
#define VERSION_MINOR 0

//----------------------------

C_mission::C_mission():
   scene(driver->CreateScene()),
   cam_range(100.0f),
   cam_edit_range(0.0f),
   cam_fov(PI*.5f),
   last_render_time(0)
{
                                    //catch igraph's messages
   igraph->AddCallback(GraphCallback_thunk, this);
}

//----------------------------

C_mission::~C_mission(){

   igraph->RemoveCallback(GraphCallback_thunk, this);
   Close();
   if(scene){
      scene->Release();
      scene = NULL;
   }
}

//----------------------------

void C_mission::ResetScreen(){
                              //re-init scene's viewport from window's size
   assert(scene);
   int sx = igraph->Scrn_sx();
   int sy = igraph->Scrn_sy();
   scene->SetViewport(I3D_rectangle(0, 0, sx, sy), !(igraph->GetFlags()&IG_FULLSCREEN));
}

//----------------------------

dword C_mission::GraphCallback(dword msg, dword par1, dword par2){

   switch(msg){
   case CM_RECREATE:
      {
                              //re-size viewport when window resized
         switch(par2){
         case 2:
            ResetScreen();
            break;
         }
      }
      break;
   case CM_PAINT:
      if(scene->GetActiveCamera())
         Render();
      else
         igraph->ClearViewport();
      break;
   }
   return 0;
}

//----------------------------

dword I3DAPI C_mission::GraphCallback_thunk(dword msg, dword par1, dword par2, void *context){

   C_mission *mission = (C_mission*)context;
   assert(mission);
   return mission->GraphCallback(msg, par1, par2);
}

//----------------------------

void C_mission::Close(dword close_flags){

   sound_cache.Clear();
   scene->Close();
}

//----------------------------

void C_mission::Tick(const S_tick_context &tc){

   scene->Tick(tc.time);
}

//----------------------------

void C_mission::Render(){

   scene->Render(I3DRENDER_UPDATELISTENER);
   last_render_time = scene->GetLastRenderTime();
}

//----------------------------
                           //help class assuring that frame's name will appear
                           // before first error message
struct S_err{

#ifdef EDITOR
   PC_editor_item_Log e_log;
   bool first;
   const char *msg;
   PI3D_frame frm;
   S_err(PC_editor_item_Log ei, const char *cp, PI3D_frame f1 = NULL):
      e_log(ei), msg(cp), frm(f1), first(ei!=NULL)
   {}
   static void OpenErrFnc(const char *cp, void *context){
      S_err *ep = (S_err*)context;
      if(ep->first){
         ep->first = false;
         ep->e_log->AddText(C_fstr("*** %s load errors ***", ep->msg));
         if(ep->frm){      //flash badly, so that artists notice they've got REAL problem
            PC_editor_item_Selection e_slct = (PC_editor_item_Selection)editor->FindPlugin("Selection");
            if(e_slct) e_slct->FlashFrame(ep->frm, 0, 0xffff0000);
         }
      }
      if(ep->e_log)
         ep->e_log->AddText(cp);
   }
#else //EDITOR
   static void OpenErrFncRel(const char *cp, void *context){
      ErrReport(cp, NULL);
   }
#endif   //!EDITOR
};

//----------------------------

static I3DENUMRET I3DAPI AddFramesToMap(PI3D_frame frm, dword c){
   map<C_str, PI3D_frame> &frame_map = *(map<C_str, PI3D_frame>*)c;
   frame_map[frm->GetName()] = frm;
   return I3DENUMRET_OK;
}

//----------------------------

#define MIN_PROGRESS_TIME_DELTA 50

void C_mission::InitializeLoadProgress(){

   driver->BeginScene();
   if(scene->GetActiveCamera())
      Render();
   else
      igraph->ClearViewport();

   assert(!load_progress.back_img);
   float sx = (float)igraph->Scrn_sx(), sy = (float)igraph->Scrn_sy();
   {
      bool was_zb = driver->GetState(RS_USEZB);
      if(was_zb) driver->SetState(RS_USEZB, false);

      struct S_vertex{ 
         S_vectorw v;
         I3D_text_coor tx;
      } v[4];
                        //draw semi-dransparent triangle over backbuffer
      static const word indx[] = {0, 1, 2, 2, 1, 3};
      {
         PI3D_texture txt;
         I3D_CREATETEXTURE ct;
         memset(&ct, 0, sizeof(ct));
         ct.opt_name = "System\\Loading.png";
         ct.flags = TEXTMAP_OPACITY | TEXTMAP_NOMIPMAP | TEXTMAP_TRUECOLOR;
         driver->CreateTexture(&ct, &txt);
         driver->SetTexture(txt);

         float tx = sx / 32.0f + 1.0f / 128.0f;
         float ty = sy / 32.0f + 1.0f / 128.0f;

         for(int i=4; i--; ){
            v[i].v.Zero();
            v[i].v.w = 1.0f;
            v[i].tx.x = 0;
            v[i].tx.y = 0;

            if(i&1){
               v[i].v.x = sx;
               v[i].tx.x = tx;
            }
            if(i&2){
               v[i].v.y = sy;
               v[i].tx.y = ty;
            }
         }

         bool lf = driver->GetState(RS_LINEARFILTER);
         if(lf) driver->SetState(RS_LINEARFILTER, false);
         scene->DrawTriangles(v, 4, I3DVC_XYZRHW | (1<<I3DVC_TEXCOUNT_SHIFT), indx, 6, 0x80000000);
         if(lf) driver->SetState(RS_LINEARFILTER, true);

         if(txt){
            driver->SetTexture(NULL);
            txt->Release();
         }
      }
                        //draw logo texture to corner
      PI3D_texture txt;
      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      ct.file_name = "System\\LoadingLogo.png";
      ct.flags = TEXTMAP_DIFFUSE | TEXTMAP_NOMIPMAP | TEXTMAP_TRUECOLOR;
      driver->CreateTexture(&ct, &txt);
      if(txt){
         driver->SetTexture(txt);
         for(int i=4; i--; ){
            v[i].v.Zero();
            v[i].v.w = 1.0f;
            v[i].tx.x = 0;
            v[i].tx.y = 0;

            v[i].v.x = (float)sx * .0f;
            if(i&1){
               v[i].v.x += (float)sx * .2f;
               v[i].tx.x = 1;
            }
            v[i].v.y = (float)sy * .82f;
            if(i&2){
               v[i].v.y += (float)sy*.133f;
               v[i].tx.y = 1;
            }
         }
         scene->DrawTriangles(v, 4, I3DVC_XYZRHW | (1<<I3DVC_TEXCOUNT_SHIFT), indx, 6, 0xfeffffff);

         txt->Release();
      }
      if(was_zb) driver->SetState(RS_USEZB, true);
   }

   load_progress.back_img = igraph->CreateBackbufferCopy();
   driver->EndScene();
   if(load_progress.back_img)
      load_progress.back_img->Release();
   load_progress.last_bit_i = -1;
   load_progress.last_render_time = igraph->ReadTimer() - MIN_PROGRESS_TIME_DELTA;
}

//----------------------------

void C_mission::DisplayLoadProgress(float progress){

   int percent = FloatToInt(progress * 100.0f);
   if(load_progress.last_bit_i!=percent && load_progress.back_img){
      int timer = igraph->ReadTimer();
      int delta = timer - load_progress.last_render_time;
      if(delta >= MIN_PROGRESS_TIME_DELTA){
         load_progress.last_render_time = timer;

         load_progress.back_img->Draw();
         driver->BeginScene();

         bool was_zb = driver->GetState(RS_USEZB);
         if(was_zb) driver->SetState(RS_USEZB, false);

               //draw bar
         {
            float sx = (float)igraph->Scrn_sx();
            float sy = (float)igraph->Scrn_sy();
            S_vectorw verts[4];
            for(dword i=4; i--; ){
               S_vectorw &v = verts[i];
               v.x = sx * .01f;
               if(i&1)
                  v.x += sx * .98f * (float)percent * .01f;
               v.y = sy * .99f;
               if(i&2)
                  v.y += sy * .003f;
               v.z = 0.0f;
               v.w = 1.0f;
            }
            static const word indx[] = {0, 1, 2, 2, 1, 3};
            scene->DrawTriangles(verts, 4, I3DVC_XYZRHW, indx, 6, 0xffff8000);
         }

         if(was_zb) driver->SetState(RS_USEZB, true);
         driver->EndScene();

         igraph->UpdateScreen();

         load_progress.last_bit_i = percent;
#ifdef EDITOR
         if(editor){
            editor->Message(C_xstr("Loading... #%%%") % percent, 0, EM_MESSAGE, true);
         }
#endif
      }
   }
}

//----------------------------

bool I3DAPI C_mission::cbLoadScene(I3D_LOADMESSAGE msg, dword context, dword prm2, void *context1){

   switch((dword)msg){
   case CBM_PROGRESS:
   case 10000:                //during mission load
      {
#ifdef EDITOR
                           //check for interruption
         if(igraph->ReadKey(true)==K_ESC)
            return true;
#endif
         if(prm2){
            float p = I3DIntAsFloat(context);
            if(msg==CBM_PROGRESS){
                                 //1st half of progress bar - reserved to scene loader
               if(prm2==2) prm2=1;  //don't let it close the progress bar
               p *= .5f;
            }else{
                                 //2nd half - reserved to mission loader
               p = .5f + p*.5f;
            }
            C_mission &mission = *(C_mission*)context1;
            mission.DisplayLoadProgress(p);
         }
      }
      break;

   case CBM_ERROR:
#ifdef EDITOR
      if(editor){
         dword color = 0;
         switch(prm2){
         case 0:              //error
            color = 0xff0000;
            break;
         case 1:              //warning
            color = 0x008000;
            break;
         }
         ((PC_editor_item_Log)editor->FindPlugin("Log"))->AddText((const char*)context, color);
      }
#else
      ErrReport((const char*)context, NULL);
#endif
      break;
   }
   return false;
}

//----------------------------

/*
static void SetModelBrightness(CPI3D_scene scene, PI3D_model mod){

   struct S_hlp{
      bool loc_pos;
      word face_index;
      S_vector point_on_plane;
      float best_dist;

      static bool I3DAPI cbCol(I3D_cresp_data &rd){
         S_hlp *hp = (S_hlp*)rd.cresp_context;
         if(hp->best_dist > rd.GetHitDistance()){
            hp->best_dist = rd.GetHitDistance();
            const I3D_texel_collision &tc = *rd.texel_data;
            hp->loc_pos = tc.loc_pos;
            hp->face_index = tc.face_index;
            hp->point_on_plane = tc.point_on_plane;
         }
         return true;
      }
   } hlp;
   hlp.best_dist = 1e+6f;

   const S_matrix &m = mod->GetMatrix();
   const I3D_bound_volume &bvol = mod->GetHRBoundVolume();
   S_vector pos = m(3);
   S_vector diag = bvol.bbox.max - bvol.bbox.min;
   pos += (bvol.bbox.min + diag * .5f) % m;
   float len = diag.Magnitude() * .5f;
   I3D_collision_data cd;
   cd.from = pos;
   cd.dir = S_vector(0, -len, .001f);
   cd.flags = I3DCOL_LINE | I3DCOL_COLORKEY;
   cd.cresp_texel = S_hlp::cbCol;
   cd.frm_ignore = mod;
   cd.cresp_context = &hlp;
   //DebugLine(cd.from, cd.from+cd.dir, 0);
   if(TestCollision(scene, cd)){
      CPI3D_frame hit_frm = cd.GetHitFrm();
      hit_frm->GetName();
      if(hit_frm->GetType()==FRAME_VISUAL && I3DCAST_VISUAL(hit_frm)->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
         CPI3D_lit_object lobj = I3DCAST_CLIT_OBJECT(hit_frm);
         if(hlp.loc_pos)
            hlp.point_on_plane *= m;
         S_vector v_color;
         if(lobj->GetLMPixel(hlp.face_index, hlp.point_on_plane, v_color)){
            float color = v_color.GetBrightness();
            float light = scene->GetLightness(cd.ComputeHitPos(),
               //&S_vector(0, 1, 0),
               NULL,
               I3DLIGHTMODE_LIGHTMAP).GetBrightness();
            light *= .6f;
            float ambient = 0.0f;
                     //compute ambient light in the sector
            for(CPI3D_frame sct1 = mod; sct1=sct1->GetParent(), sct1; ){
               if(sct1->GetType()==FRAME_SECTOR){
                  PI3D_sector sct = I3DCAST_SECTOR(sct1);
                  for(dword i=sct->NumLights(); i--; ){
                     PI3D_light lp = sct->GetLight(i);
                     if((lp->GetMode()&I3DLIGHTMODE_LIGHTMAP) && lp->GetLightType()==I3DLIGHT_AMBIENT){
                        //ambient += lp->GetColor() * lp->GetPower();
                        ambient += lp->GetColor().GetBrightness() * lp->GetPower();
                     }
                  }
                  break;
               }
            }
            light -= ambient;
            color -= ambient;
            float delta = light - color;
            delta /= (light - ambient);
            delta *= .5f;
            float brightness = 1.0f - delta;
            
            const PI3D_frame *frms = mod->GetFrames();
            for(dword i=mod->NumFrames(); i--; ){
               PI3D_frame frm = *frms++;
               if(frm->GetType()==FRAME_VISUAL)
                  I3DCAST_VISUAL(frm)->SetBrightness(I3D_VIS_BRIGHTNESS_NOAMBIENT, brightness);
            }
         }
      }
   }
}
*/

//----------------------------

E_MISSION_IO C_mission::Open(const char *dir, dword open_flags, I3D_LOAD_CB_PROC *lcbp, void *lcbc){

   if(!(open_flags&OPEN_NO_SHOW_PROGRESS) && !lcbp)
      InitializeLoadProgress();

   S_load_context lc;

   lc.load_cb_proc = lcbp ? lcbp : cbLoadScene;
   lc.load_cb_context = lcbp ? lcbc : this;
   lc.container = scene->GetContainer();                              
   lc.open_flags = open_flags;
                              //debuggin support - log errors
                              // (debug compilation only)
   lc.open_dir = dir;
   igraph->EnableSuspend(false);
#ifdef EDITOR

   lc.e_log = editor ? (PC_editor_item_Log)editor->FindPlugin("Log") : NULL;
   struct S_hlp{
      static bool I3DAPI SceneErrFnc(I3D_LOADMESSAGE msg, dword prm1, dword prm2, void *context){

         C_mission::S_load_context &lc = *(C_mission::S_load_context*)context;
         switch(msg){
         case CBM_ERROR:
         case CBM_LOG:
            if(context) lc.e_log->AddText((const char*)prm1);
            break;
         default:
            return (*lc.load_cb_proc)(msg, prm1, prm2, lc.load_cb_context);
         }
         return false;
      }
   };
   if(!(open_flags&OPEN_NO_EDITOR)){
      lc.e_modify = editor ? (PC_editor_item_Modify)editor->FindPlugin("Modify") : NULL;
      lc.e_undo = editor ? editor->FindPlugin("Undo") : NULL;
      lc.e_script = editor ? editor->FindPlugin("Script") : NULL;
   }

#pragma warning(disable: 4238)
# define OPEN_ERR_LOG(name, frm) S_err::OpenErrFnc, (void*)&S_err(lc.e_log, name, frm)
# define SCENE_LOAD_LOG S_hlp::SceneErrFnc, &lc

#else //EDITOR
# define OPEN_ERR_LOG(name, frm) S_err::OpenErrFncRel, NULL
# define SCENE_LOAD_LOG lc.load_cb_proc, lc.load_cb_context
#endif   //!EDITOR

   lc.merge_mode = (open_flags&OPEN_MERGE_MODE);

   bool open_ok = false;
   bool load_cancelled = false;
   if(!lc.merge_mode){
                              //re-load scene
      Close();
      open_ok = true;
                              
      dword i3d_open_flags = I3DLOAD_ERRORS;
      if(!(open_flags&OPEN_NO_SHOW_PROGRESS))
         i3d_open_flags |= I3DLOAD_PROGRESS;
      if(open_flags&OPEN_LOG) i3d_open_flags |= I3DLOAD_LOG;

      I3D_RESULT ir;
      ir = scene->Open(C_xstr("missions\\%\\scene.i3d") % dir, i3d_open_flags, SCENE_LOAD_LOG);

#ifdef EDITOR
                              //report errors (if any)
      if(I3D_FAIL(ir)){
         const char *err;
         switch(ir){
         case I3DERR_NOFILE: 
            err = "no such file";
            break;
         case I3DERR_OUTOFMEM:
            err = "out of memory";
            break;
         case I3DERR_FILECORRUPTED:
            err = "file corrupted";
            break;
         case I3DERR_CANCELED:
            err = "loading cancelled";
            load_cancelled = true;
            break;
         default:
            err = "unknown error";
         }
         ErrReport(C_xstr("Scene open error: %.") % err, editor);
         open_ok = false;
      }
#endif                        //EDITOR
   }else{
      open_ok = true;
      /*
      if(!open_ok){
#ifdef EDITOR
         if(save_flags&IG_SUSPENDINACTIVE) igraph->UpdateParams(0, 0, 0, IG_SUSPENDINACTIVE, IG_SUSPENDINACTIVE);
#endif
         return MIO_ACCESSFAIL;
      }
      */
   }

   if(open_ok){

                              //keep all in-scene frames in a map,
                              // which allows much faster name look-up
                              // than I3D_scene::FindFrame
   scene->EnumFrames(AddFramesToMap, (dword)&lc.frame_map);
   lc.frame_map[scene->GetPrimarySector()->GetName()] = scene->GetPrimarySector();
   lc.frame_map[scene->GetBackdropSector()->GetName()] = scene->GetBackdropSector();

   C_str str;
   if(!lc.merge_mode){
                              //create binary filename
      str = C_str(C_xstr("missions\\%\\scene.bin") % dir);
   }else{
                              //in merge mode, the filename is full
      str = dir;
   }

   if(!lc.ck.ROpen(str)){
      CloseLoadProgress();
      igraph->EnableSuspend(true);
      return MIO_NOFILE;
   }
   }//open_ok

   lc.cam = scene->GetActiveCamera();
   if(!lc.cam && !(open_flags&OPEN_NO_EDITOR)){
#ifdef EDITOR
      if(editor){
         PC_editor_item_MouseEdit ei = (PC_editor_item_MouseEdit)editor->FindPlugin("MouseEdit");
         if(ei){
            lc.cam = ei->GetEditCam();
            scene->SetActiveCamera(lc.cam);
         }
      }
#endif   //EDITOR
   }

   if(open_ok){

   if(!lc.merge_mode){
      if(lc.cam){
                              //setup defaults
         lc.cam->SetOrthogonal(lc.is_ortho);
         lc.cam->SetOrthoScale(lc.ortho_scale);
         lc.cam->SetRange(CAM_NEAR_RANGE, lc.cam_range);
         lc.cam->SetFOV(lc.cam_fov);
      }
   }
   model_cache.PushWorkMission(this);
   anim_cache.PushWorkMission(this);

   if(!lc.merge_mode)
      mission_name = dir;

   try{
      if(++lc.ck == CT_BASECHUNK){
         while(lc.ck){
            CK_TYPE t = ++lc.ck;
            if(!(open_flags&OPEN_NO_SHOW_PROGRESS)){
               if(lc.ShowProgress()){
                  load_cancelled = true;
                  open_ok = false;
                  break;
               }
            }
#ifdef EDITOR
            if(open_flags&OPEN_LOG)
               lc.WarnLog(C_str(C_xstr("Chunk 0x#.4%, size %") % dword(t) % lc.ck), NULL);
#endif

                                 //let virtual func to load this chunk
            if(!LoadChunk(t, lc)){
               open_ok = false;
               load_cancelled = lc.cancelled;
               break;
            }
            --lc.ck;
         }
         --lc.ck;
      }
      lc.ck.Close();
   }catch(const C_except &e){
      open_ok = false;
      lc.ErrLog(C_xstr("File corrupted (%)") % e.what(), NULL);
   }

   if(open_ok){
      if(!lc.merge_mode){
         GetMissionCRC(mission_name, mission_crc);

         cam_range = lc.cam_range;
         cam_edit_range = lc.cam_edit_range;
         cam_fov = lc.cam_fov;
      }
#ifdef EDITOR
      if(!(open_flags&OPEN_NO_EDITOR)){
         if(!lc.merge_mode && editor){
            editor->Broadcast(&C_editor_item::AfterLoad);
         }
      }
#endif

      /*
#ifdef _DEBUG
      struct S_hlp: public I3D_enum{
         CPI3D_scene scn;
         S_hlp(CPI3D_scene s): scn(s){}
         virtual bool I3DAPI Proc(PI3D_frame frm){
            SetModelBrightness(scn, I3DCAST_MODEL(frm));
            return true;
         }
      } hlp(scene);
      scene->EnumFramesEx(hlp, ENUMF_MODEL);
#endif
      */
   }else{
      if(!lc.merge_mode)
         mission_name = NULL;
   }

   model_cache.PopWorkMission(this);
   anim_cache.PopWorkMission(this);

   }//open_ok

   CloseLoadProgress();
   igraph->GetTimer(1, 1);

   igraph->EnableSuspend(true);
   return open_ok ? MIO_OK : load_cancelled ? MIO_CANCELLED : MIO_CORRUPT;
}

//----------------------------

E_MISSION_IO C_mission::LoadModel(PI3D_model mod, const char *fname, I3D_LOAD_CB_PROC *lcbp, void *lcbc) const{

   S_load_context lc;
   lc.load_cb_proc = lcbp ? lcbp : cbLoadScene;
   lc.load_cb_context = lcbp ? lcbc : (void*)this;
   //lc.container = mod->GetContainer();                              
   lc.loaded_model = mod;
   lc.open_flags = OPEN_NO_SHOW_PROGRESS | OPEN_NO_EDITOR | OPEN_MODEL;

   /*
   C_str conv_name = fname;
   for(int i=1; i<conv_name.Size(); i++){
      if(conv_name[i]=='+')
         conv_name[i] = '\\';
   }
   */
                              //debuggin support - log errors
                              // (debug compilation only)
#ifdef EDITOR
   lc.open_dir = &fname[1];
   lc.e_log = editor ? (PC_editor_item_Log)editor->FindPlugin("Log") : NULL;
   struct S_hlp{
      static bool I3DAPI SceneErrFnc(I3D_LOADMESSAGE msg, dword prm1, dword prm2, void *context){

         C_mission::S_load_context &lc = *(C_mission::S_load_context*)context;
         switch(msg){
         case CBM_ERROR:
         case CBM_LOG:
            if(context) lc.e_log->AddText((const char*)prm1);
            break;
         default:
            return (*lc.load_cb_proc)(msg, prm1, prm2, lc.load_cb_context);
         }
         return false;
      }
   };
# define OPEN_ERR_LOG(name, frm) S_err::OpenErrFnc, (void*)&S_err(lc.e_log, name, frm)
# define SCENE_LOAD_LOG S_hlp::SceneErrFnc, &lc

#else //EDITOR
# define OPEN_ERR_LOG(name, frm) S_err::OpenErrFncRel, NULL
# define SCENE_LOAD_LOG lc.load_cb_proc, lc.load_cb_context
#endif   //!EDITOR

   bool load_cancelled = false;
                              
   dword i3d_open_flags = I3DLOAD_ERRORS;
   I3D_RESULT ir;
   ir = mod->Open(C_xstr("missions\\%\\scene.i3d") % &fname[1], i3d_open_flags, SCENE_LOAD_LOG);
   bool ok = I3D_SUCCESS(ir);
                              //report errors (if any)
   if(!ok){
#ifdef EDITOR
      const char *err;
      switch(ir){
      case I3DERR_NOFILE: 
         err = "no such file";
         break;
      case I3DERR_OUTOFMEM:
         err = "out of memory";
         break;
      case I3DERR_FILECORRUPTED:
         err = "file corrupted";
         break;
      case I3DERR_CANCELED:
         err = "loading cancelled";
         load_cancelled = true;
         break;
      default:
         err = "unknown error";
      }
      ErrReport(C_xstr("Scene open error: %.") % err, editor);
      //if(ir==I3DERR_CANCELED) return MIO_CANCELLED;
#endif//EDITOR
   }else{
                              //keep all in-scene frames in a map,
                              // which allows much faster name look-up
                              // than I3D_scene::FindFrame
      //mod->EnumFrames(AddFramesToMap_OrigName, (dword)&lc.frame_map, ENUMF_ALL);
      {
         const PI3D_frame *frmp = mod->GetFrames();
         for(int i=mod->NumFrames(); i--; ){
            PI3D_frame frm = frmp[i];
            lc.frame_map[frm->GetOrigName()] = frm;

         }

      }
      lc.frame_map["Primary sector"] = mod;
      C_str str = C_xstr("missions\\%\\scene.bin") % &fname[1];

      if(!lc.ck.ROpen(str)){
         mod->Close();
         return MIO_NOFILE;
      }

      if(++lc.ck == CT_BASECHUNK){
         while(lc.ck){
                              //let virtual func to load this chunk
            if(!const_cast<C_mission*>(this)->LoadChunk(++lc.ck, lc)){
               ok = false;
               load_cancelled = lc.cancelled;
               break;
            }
            --lc.ck;
         }
         --lc.ck;
      }
      lc.ck.Close();
      if(ok){
         t_model_frames::iterator it;
         for(it=lc.model_frames.begin(); it!=lc.model_frames.end(); it++){
            PI3D_frame frm = (*it);
            mod->AddFrame(frm);
         }
      }
   }
   return ok ? MIO_OK : load_cancelled ? MIO_CANCELLED : MIO_NOFILE;
}

//----------------------------

bool C_mission::LoadChunk(CK_TYPE ck_t, S_load_context &lc){

   switch(ck_t){
   case CT_SCENE_BGND_COLOR:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_SETTINGS)) break;
      if(lc.open_flags&OPEN_MODEL) break;
      {
         S_vector v;
         lc.ck.Read(&v, sizeof(v));
         scene->SetBgndColor(v);
      }
      break;

   case CT_CAMERA_FOV:
   case CT_CAMERA_RANGE:
   case CT_CAMERA_EDIT_RANGE:
   case CT_CAMERA_ORTHO_SCALE:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_SETTINGS)) break;
      {
         float f = 0.0f;
         lc.ck.Read(&f, sizeof(float));
         switch(ck_t){
         case CT_CAMERA_FOV:
            lc.cam_fov = f;
            if(lc.cam)
               lc.cam->SetFOV(f);
            break;
         case CT_CAMERA_RANGE:
            lc.cam_range = f;
            if(lc.cam)
               lc.cam->SetRange(CAM_NEAR_RANGE, f);
            break;
         case CT_CAMERA_EDIT_RANGE:
            lc.cam_edit_range = f;
            break;
         case CT_CAMERA_ORTHO_SCALE:
            lc.ortho_scale = f;
            if(lc.cam)
               lc.cam->SetOrthoScale(f);
            break;
         }
      }
      break;

   case CT_CAMERA_ORTHOGONAL:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_SETTINGS))
         break;
      lc.is_ortho = true;
      if(lc.cam)
         lc.cam->SetOrthogonal(true);
      break;

   case CT_SCENE_BACKDROP:
      if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_SETTINGS)) break;
      if(lc.open_flags&OPEN_MODEL) break;
      {
         float r[2];
         lc.ck.Read((char*)r, sizeof(r));
         scene->SetBackdropRange(r[0], r[1]);
      }
      break;

   case CT_MODIFICATIONS:
      if(lc.merge_mode && !(lc.open_flags&(OPEN_MERGE_SOUNDS|OPEN_MERGE_LIGHTS|OPEN_MERGE_MODELS|OPEN_MERGE_VOLUMES|
         OPEN_MERGE_DUMMYS|OPEN_MERGE_CAMERAS|OPEN_MERGE_TARGETS|OPEN_MERGE_OCCLUDERS|OPEN_MERGE_LIGHTMAPS)))
         break;
      {
#ifdef EDITOR
         pair<PI3D_frame, dword> modify_pair;
#endif                        //EDITOR

         while(lc.ck){
            if(!(lc.open_flags&OPEN_NO_SHOW_PROGRESS)){
               if(lc.ShowProgress()){
                  lc.cancelled = true;
                  return false;
               }
            }

            switch(++lc.ck){
            case CT_MODIFICATION:
               I3D_FRAME_TYPE frame_type = FRAME_NULL;   //when FRAME_NULL, find in scene
               dword frame_subtype = 0;
               PI3D_frame frm = NULL;
#ifdef EDITOR
               dword flags = 0;  //modification flags
#endif
               S_vector pos, world_pos(0, 0, 0), nu_scale, color, pivot;
               bool world_pos_ok = false;
               float scale = 0;
               S_quat rot;
               PI3D_frame link_frm = NULL;
               I3D_VOLUMETYPE vol_t = I3DVOLUME_NULL;
               dword col_mat = 0;
               dword frm_flags = 0;
               float brighness = 0;
               bool pos_ok = false, rot_ok = false, scl_ok = false,
                  nu_scl_ok = false, hide = false, created = false,
                  col_mat_ok = false, f_flgs_ok = false, bright_ok = false;

               C_str str;
               map<C_str, PI3D_frame>::const_iterator it_fmap;

               while(lc.ck){
                  CK_TYPE t = ++lc.ck;
#ifdef EDITOR
                  if(lc.open_flags&OPEN_LOG)
                     lc.WarnLog(C_xstr("  Chunk 0x#.4%, size %") % dword(t) % lc.ck, NULL);
#endif
                  switch(t){
                  case CT_FRAME_TYPE:
                     frame_type = (I3D_FRAME_TYPE)lc.ck.RIntChunk();
                     break;

                  case CT_FRAME_SUBTYPE:
                     frame_subtype = lc.ck.RIntChunk();
                     break;

                  case CT_NAME:
                     str = lc.ck.RStringChunk();
                     if(frame_type==FRAME_NULL){
                                          //in merge mode, we only operate on created frames, or lightmaps
                        if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_LIGHTMAPS))
                           break;
                        it_fmap = lc.frame_map.find(str);
                        frm = (it_fmap!=lc.frame_map.end()) ? (*it_fmap).second : NULL;
                     }
                     if(frame_type!=FRAME_NULL){
                        {
                           switch(frame_type){

                           case FRAME_LIGHT:
                              if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_LIGHTS))
                                 break;
                              frm = scene->CreateFrame(FRAME_LIGHT);
                              break;

                           case FRAME_SOUND:
                              if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_SOUNDS))
                                 break;
                              frm = scene->CreateFrame(FRAME_SOUND);
                              break;

                           case FRAME_MODEL:
                              if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_MODELS)) break;
                              frm = scene->CreateFrame(FRAME_MODEL);
                              break;

                           case FRAME_VOLUME: 
                              if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_VOLUMES)) break;
                              frm = scene->CreateFrame(FRAME_VOLUME);
                              break;

                           case FRAME_DUMMY:
                              if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_DUMMYS)) break;
                              frm = scene->CreateFrame(FRAME_DUMMY);
                              break;

                           case FRAME_USER: frm = scene->CreateFrame(FRAME_USER); break;

                           case FRAME_CAMERA:
                              {
                                 if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_CAMERAS))
                                    break;
                                 PI3D_camera cam;
                                 frm = cam = I3DCAST_CAMERA(scene->CreateFrame(FRAME_CAMERA));
                                 cam->SetRange(CAM_NEAR_RANGE, lc.cam_range);
                                 cam->SetFOV(lc.cam_fov);
                                 cam->SetOrthogonal(lc.is_ortho);
                                 cam->SetOrthoScale(lc.ortho_scale);

                                 if(!(lc.open_flags&OPEN_MODEL)){
                                             //set first created camera to scene, if not set yet
                                    if(!scene->GetActiveCamera())
                                       scene->SetActiveCamera(cam);
                                 }
                              }
                              break;

                           case FRAME_VISUAL:
                              if(lc.merge_mode) break;
                              frm = scene->CreateFrame(FRAME_VISUAL, frame_subtype);
                              break;

                           case FRAME_OCCLUDER:
                              if(lc.merge_mode && !(lc.open_flags&OPEN_MERGE_OCCLUDERS)) break;
                              frm = scene->CreateFrame(FRAME_OCCLUDER);
                              break;

                           default: 
                              lc.ErrLog(C_fstr("Cannot create frame '%s': requested type = %i", (const char*)str, frame_type), NULL);
                           }
#ifdef EDITOR
                           if(frm){
                                             //check frame duplication
                              if(lc.frame_map.find(str)!=lc.frame_map.end()){
                                 lc.ErrLog(C_fstr("Cannot create frame '%s': already exist", (const char*)str), NULL);
                                 frm->Release();
                                 frm = NULL;
                              }
                           }
#endif                                          //EDITOR

                           if(frm){
#ifdef EDITOR
                              /*
                              if(lc.merge_mode && lc.e_undo){
                                             //save undo info (merge mode only)
                                 lc.e_undo->Save(editor->FindPlugin("Create"), E_CREATE_DELETE_NAMED, (void*)(const char*)str, str.Size()+1);
                              }
                              */
                              flags |= E_MODIFY_FLG_CREATE;
#endif   //EDITOR
                              created = true;
                              frm->SetName(str);
                                          //add into map
                              lc.frame_map[str] = frm;
                           }
                        }
                     }else{
                        if(!frm){
                           lc.ErrLog(C_fstr("Cannot initialize frame '%s': not found", (const char*)str), NULL);
#ifdef EDITOR
                           flags = 0;
#endif
                        }
                     }
                     break;

                  case CT_FRAME_FLAGS:
                     frm_flags = lc.ck.RIntChunk();
                     f_flgs_ok = true;
#ifdef EDITOR
                     flags |= E_MODIFY_FLG_FRM_FLAGS;
#endif
                     break;

                  case CT_POSITION:
                     pos = lc.ck.RVectorChunk();
                     pos_ok = true;
#ifdef EDITOR
                     flags |= E_MODIFY_FLG_POSITION;
#endif
                     break;

                  case CT_WORLD_POSITION:
                     world_pos = lc.ck.RVectorChunk();
                     world_pos_ok = true;
#ifdef EDITOR
                     flags |= E_MODIFY_FLG_POSITION;
#endif
                     break;

                  case CT_QUATERNION:
                     rot = lc.ck.RQuaternionChunk();
                     rot_ok = true;
#ifdef EDITOR
                     flags |= E_MODIFY_FLG_ROTATION;
#endif
                     break;

                  case CT_USCALE:
                     scale = lc.ck.RFloatChunk();
                     scl_ok = true;
#ifdef EDITOR
                     flags |= E_MODIFY_FLG_SCALE;
#endif
                     break;

                  case CT_NUSCALE:
                     nu_scale = lc.ck.RVectorChunk();
                     nu_scl_ok = true;
#ifdef EDITOR
                     flags |= E_MODIFY_FLG_NU_SCALE;
#endif
                     break;

                  case CT_MODIFY_LIGHT:
                     if(frm && frm->GetType()==FRAME_LIGHT){
                        PI3D_light lp = I3DCAST_LIGHT(frm);
                        while(lp->NumLightSectors())
                           lp->GetLightSectors()[0]->RemoveLight(lp);
                        while(lc.ck){
                           switch(++lc.ck){
                           case CT_LIGHT_TYPE:
                              {
                                 I3D_LIGHTTYPE lt = (I3D_LIGHTTYPE)lc.ck.RIntChunk();
                                 if(lt==I3DLIGHT_SPOT){
                                    lc.ErrLog(C_fstr("Light '%s': spotlight is obsolete", (const char*)lp->GetName()), lp);
                                 }
                                 lp->SetLightType(lt);
                              }
                              break;
                           case CT_COLOR: lp->SetColor(lc.ck.RVectorChunk()); break;
                           case CT_LIGHT_POWER: lp->SetPower(lc.ck.RFloatChunk()); break;
                           case CT_LIGHT_CONE: 
                              {
                                 float c[2];
                                 lc.ck.Read(c, sizeof(c));
                                 --lc.ck;
                                 lp->SetCone(c[0], c[1]);
                              }
                              break;
                           case CT_LIGHT_RANGE:
                              {
                                 float r[2];
                                 lc.ck.Read(r, sizeof(r));
                                 --lc.ck;
                                 lp->SetRange(r[0], r[1]);
                              }
                              break;

                           case CT_LIGHT_MODE:
                              lp->SetMode(lc.ck.RIntChunk());
                              break;

                           case CT_LIGHT_SPECULAR_COLOR: lp->SetSpecularColor(lc.ck.RVectorChunk()); break;
                           case CT_LIGHT_SPECULAR_POWER: lp->SetSpecularPower(lc.ck.RFloatChunk()); break;
                           //case CT_LIGHT_SPECULAR_ANGLE: lp->SetSpecularAngle(lc.ck.ReadFloat()); break;

                           case CT_LIGHT_SECTOR:
                              {
                                 str = lc.ck.RStringChunk();
                                 PI3D_sector sct = NULL;
                                 //PI3D_frame frm1 = lc.frame_map[str];
                                 it_fmap = lc.frame_map.find(str);
                                 PI3D_frame frm1 = (it_fmap!=lc.frame_map.end()) ? (*it_fmap).second : NULL;

                                 if(frm1 && frm1->GetType()==FRAME_SECTOR) sct = I3DCAST_SECTOR(frm1);
                                 if(sct) sct->AddLight(lp);
                                 else{
                                    lc.ErrLog(C_fstr("Cannot set light '%s' to sector '%s': not found",
                                       (const char*)lp->GetName(), (const char*)str), lp);
                                 }
                              }
                              break;

                           default:
                              --lc.ck;
                           }
                        }
#ifdef EDITOR
                        flags |= E_MODIFY_FLG_LIGHT;
#endif
                     } 
                     --lc.ck;
                     break;

                  case CT_MODIFY_SOUND:
                     if(frm && frm->GetType()==FRAME_SOUND){
                        PI3D_sound sp = I3DCAST_SOUND(frm);
                        sp->SetLoop(false);
                        sp->SetStreaming(false);
                        sp->SetOn(false);
                        bool on = false;
                        while(lc.ck){
                           switch(++lc.ck){
                           case CT_NAME:
                              str = lc.ck.RStringChunk();
                              if(isound){
                                 sound_cache.Open(sp, str, scene, 0, OPEN_ERR_LOG(frm->GetName(), frm));
                              }else{
                                 sound_cache.Open(sp, str, scene, 0, NULL, NULL);
                              }
                              break;

                           case CT_SOUND_TYPE: sp->SetSoundType((I3D_SOUNDTYPE)lc.ck.RIntChunk()); break;
                           case CT_SOUND_VOLUME: sp->SetVolume(lc.ck.RFloatChunk()); break;
                           case CT_SOUND_OUTVOL: sp->SetOutVol(lc.ck.RFloatChunk()); break;
                           case CT_SOUND_CONE: 
                              {
                                 float c[2];
                                 lc.ck.Read(c, sizeof(c));
                                 --lc.ck;
                                 sp->SetCone(c[0], c[1]);
                              }
                              break;

                           case CT_SOUND_RANGE:
                              {
                                 float r[2];
                                 lc.ck.Read(&r, sizeof(r));
                                 --lc.ck;
                                 sp->SetRange(r[0], r[1]);
                              }
                              break;

                           case CT_SOUND_LOOP: 
                              sp->SetLoop(true); 
                              --lc.ck;
                              break;

                           case CT_SOUND_STREAMING: 
                              sp->SetStreaming(true); 
                              --lc.ck;
                              break;

                           case CT_SOUND_ENABLE: 
                              on = true;
                              --lc.ck;
                              break;

                           case CT_SOUND_SECTOR:
                              {
                                 str = lc.ck.RStringChunk();
                                 PI3D_sector sct = NULL;
                                 //PI3D_frame frm1 = lc.frame_map[str];
                                 it_fmap = lc.frame_map.find(str);
                                 PI3D_frame frm1 = (it_fmap!=lc.frame_map.end()) ? (*it_fmap).second : NULL;

                                 if(frm1 && frm1->GetType()==FRAME_SECTOR)
                                    sct = I3DCAST_SECTOR(frm1);
                                 if(sct) sct->AddSound(sp);
                                 else{
                                    lc.ErrLog(C_fstr("Cannot set sound '%s' to sector '%s': not found",
                                       (const char*)sp->GetName(), (const char*)str), sp);
                                 }
                              }
                              break;

                           default:
                              --lc.ck;
                           }
                        }
#ifdef EDITOR
                        flags |= E_MODIFY_FLG_SOUND;
#endif
                        if(on)
                           sp->SetOn(true);
                     } 
                     --lc.ck;
                     break;

                  case CT_MODIFY_VOLUME:
                     if(frm && frm->GetType()==FRAME_VOLUME){
                        while(lc.ck){
                           switch(++lc.ck){
                           case CT_VOLUME_TYPE:
                              vol_t = (I3D_VOLUMETYPE)lc.ck.RIntChunk();
#ifdef EDITOR
                              flags |= E_MODIFY_FLG_VOLUME_TYPE;
#endif
                              break;
                           case CT_VOLUME_MATERIAL:
                              col_mat = lc.ck.RIntChunk();
                              col_mat_ok = true;
#ifdef EDITOR
                              flags |= E_MODIFY_FLG_COL_MAT;
#endif
                              break;
                           default: assert(0); --lc.ck;
                           }
                        }
                     }
                     --lc.ck;
                     break;

                  case CT_MODIFY_OCCLUDER:
                     if(frm && frm->GetType()==FRAME_OCCLUDER){
                        PI3D_occluder occ = I3DCAST_OCCLUDER(frm);
                        I3D_OCCLUDERTYPE ot = I3DOCCLUDER_SPHERE;

                        while(lc.ck){
                           switch(++lc.ck){
                           case CT_OCCLUDER_VERTICES:
                              {
                                 dword numv = 0;
                                 lc.ck.Read(&numv, sizeof(numv));
                                 S_vector *vp = new S_vector[numv];
                                 lc.ck.Read(vp, numv*sizeof(S_vector));
                                 occ->Build(vp, numv);
                                 delete[] vp;
                              }
                              ot = I3DOCCLUDER_MESH;
                              break;
                           }
                           --lc.ck;
                        }
                        occ->SetOccluderType(ot);
                     } 
                     --lc.ck;
                     break;

                  case CT_MODIFY_SECTOR:
                     if(frm && frm->GetType()==FRAME_SECTOR){
                        PI3D_sector sct = I3DCAST_SECTOR(frm);
                        while(lc.ck){
                           switch(++lc.ck){
                           case CT_SECTOR_ENVIRONMENT:
                              sct->SetEnvironmentID(lc.ck.RWordChunk());
                              break;
                           case CT_SECTOR_TEMPERATURE:
                              sct->SetTemperature(lc.ck.RFloatChunk());
                              break;
                           default:
                              --lc.ck;
                           }
                        }
#ifdef EDITOR
                        flags |= E_MODIFY_FLG_SECTOR;
#endif
                     } 
                     --lc.ck;
                     break;

                  case CT_LINK:
                     if(frm){
                        if(++lc.ck == CT_NAME){
                           str = lc.ck.RStringChunk();
                           //link_frm = lc.frame_map[str];
                           it_fmap = lc.frame_map.find(str);
                           link_frm = (it_fmap!=lc.frame_map.end()) ? (*it_fmap).second : NULL;

                           if(link_frm){
#ifdef EDITOR
                              flags |= E_MODIFY_FLG_LINK;
#endif
                           }else{
                              lc.ErrLog(C_fstr("Frame '%s': failed to link to '%s'",
                                 (const char*)frm->GetName(), (const char*)str), NULL);
                              if(world_pos_ok)
                                 pos = world_pos;
                           }
                        }
                     }
                     --lc.ck;
                     break;

                  case CT_HIDDEN:
                     hide = true;
#ifdef EDITOR
                     flags |= E_MODIFY_FLG_HIDE;
#endif
                     --lc.ck;
                     break;

                  case CT_MODIFY_MODELFILENAME:
                     if(frm && frm->GetType()==FRAME_MODEL){
                        str = lc.ck.RStringChunk();
                        PI3D_model mod = I3DCAST_MODEL(frm);
                                          //strip extension (old-style format)
                        if(str.Size()>=4 && str[str.Size()-4]=='.'){
                           lc.WarnLog(C_fstr("Name '%s': stripping extension",
                              (const char*)frm->GetName(), (const char*)str), NULL);
                           str[str.Size()-4] = 0;
                        }


                        if(I3D_SUCCESS(model_cache.Open(mod, str, scene, 0, OPEN_ERR_LOG(str, mod)))){
                           PI3D_animation_set as = mod->GetAnimationSet();
                           if(as)
                              mod->SetAnimation(0, as, I3DANIMOP_LOOP);

                                          //add frames to map
                           struct S_hlp{
                              static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword c){
                                 map<C_str, PI3D_frame> &frame_map = *(map<C_str, PI3D_frame>*)c;
                                 frame_map[frm->GetName()] = frm;
                                 return I3DENUMRET_OK;
                              }
                           };
                           mod->EnumFrames(S_hlp::cbAdd, (dword)&lc.frame_map);
                           /*
                           const PI3D_frame *frmp = mod->GetFrames();
                           for(int i=mod->NumFrames(); i--; ){
                              PI3D_frame frm = frmp[i];
                              lc.frame_map[frm->GetName()] = frm;
                           }
                           */
                        }else{
                           lc.ErrLog(C_fstr("Frame '%s': can't load model '%s'",
                              (const char*)frm->GetName(), (const char*)str), NULL);
                        }
                     }else
                        --lc.ck;
                     break;

                  case CT_MODIFY_VISUAL:
                     {
                        dword vtype = lc.ck.RIntChunk();
                        if(frm && frm->GetType()==FRAME_VISUAL && I3DCAST_VISUAL(frm)->GetVisualType()!=vtype){
                           PI3D_visual vis = I3DCAST_VISUAL(frm);
                                             //cast visual to other type
                           bool ok = false;
                           PI3D_visual new_vis = I3DCAST_VISUAL(scene->CreateFrame(FRAME_VISUAL, vtype));
                           if(new_vis){
                              I3D_RESULT ir = new_vis->Duplicate(vis);
                              if(I3D_SUCCESS(ir)){
                                             //re-link
                                 new_vis->LinkTo(vis->GetParent());
                                 while(vis->NumChildren())
                                    vis->GetChildren()[0]->LinkTo(new_vis);
                                             //close/destroy old
                                 vis->LinkTo(NULL);

                                 //map<C_str, PI3D_frame>::iterator it = lc.frame_map.find(vis->GetName());
                                 map<C_str, PI3D_frame>::iterator it;
                                 it = lc.frame_map.find(vis->GetName());
                                 if(it==lc.frame_map.end()){
                                    for(it=lc.frame_map.begin(); it!=lc.frame_map.end(); it++){
                                       if((*it).second==vis)
                                          break;
                                    }
                                 }
                                 assert(it!=lc.frame_map.end());
                                 if(it!=lc.frame_map.end())
                                    lc.frame_map.erase(it);
                                 lc.frame_map[vis->GetName()] = new_vis;

                                 for(PI3D_frame model_root=new_vis; (model_root=model_root->GetParent(), model_root) && model_root->GetType()!=FRAME_MODEL; );
                                 if(model_root){
                                    if(created && lc.loaded_model){
                                       assert(lc.loaded_model==model_root);
                                       t_model_frames::iterator it = lc.model_frames.find(vis);
                                       assert(it!=lc.model_frames.end());
                                       lc.model_frames.erase(it);
                                       lc.model_frames.insert(new_vis);
                                    }else{
                                       I3D_RESULT ir;
                                       ir = I3DCAST_MODEL(model_root)->RemoveFrame(vis);
                                       I3DCAST_MODEL(model_root)->AddFrame(new_vis);
                                    }
                                 }else{
                                    lc.container->RemoveFrame(vis);
                                    lc.container->AddFrame(new_vis);
                                 }

                                 frm = vis = new_vis;
#ifdef EDITOR
                                 flags |= E_MODIFY_FLG_VISUAL;
#endif
                                 ok = true;
                              }
                              new_vis->Release();
                           }
                           if(!ok){
                              byte str[5];
                              *(dword*)str = vtype;
                              str[4] = 0;
                              lc.ErrLog(C_fstr("Failed to cast visual '%s' to type '%s'",
                                 (const char*)frm->GetName(), str), frm);
                           }
                        }
                     }
                     break;

                  case CT_MODIFY_LIT_OBJECT:
                     if(frm && frm->GetType()==FRAME_VISUAL && I3DCAST_VISUAL(frm)->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
                        PI3D_lit_object lmap = I3DCAST_LIT_OBJECT(frm);
                        /*
#ifdef EDITOR
                        if(lc.merge_mode && lc.e_undo)
                           SaveLMUndo(lmap, lc.e_undo);
#endif                                       //EDITOR
                           */

                        I3D_RESULT ir = lmap->Load(lc.ck.GetHandle());
                        if(I3D_FAIL(ir)){
                           lc.ErrLog(C_fstr("Lightmap '%s': failed to load", (const char*)str), lmap);
                        }
#ifdef EDITOR
                        flags |= E_MODIFY_FLG_VISUAL;
#endif
                     }
                     --lc.ck;
                     break;

                  case CT_MODIFY_VISUAL_PROPERTY:
                     if(frm && frm->GetType()==FRAME_VISUAL){
                        PI3D_visual vis = I3DCAST_VISUAL(frm);

                        I3D_PROPERTYTYPE ptype = I3DPROP_NULL;
                        lc.ck.Read(&ptype, sizeof(byte));

                        word prop_index;
                        lc.ck.Read(&prop_index, sizeof(prop_index));

                        byte tmp_data[16];
                        dword value;
                        switch(ptype){
                        case I3DPROP_BOOL:
                           value = 0;
                           lc.ck.Read(&value, sizeof(byte));
                           vis->SetProperty(prop_index, value);
                           break;
                        case I3DPROP_INT:
                        case I3DPROP_FLOAT:
                        case I3DPROP_ENUM:
                           lc.ck.Read(&value, sizeof(value));
                           vis->SetProperty(prop_index, value);
                           break;
                        case I3DPROP_VECTOR3:
                        case I3DPROP_COLOR:
                           {
                              lc.ck.Read(tmp_data, 12);
                              vis->SetProperty(prop_index, (dword)tmp_data);
                           }
                           break;
                        case I3DPROP_STRING:
                           {
                              char buf[256], *cp = buf;
                              while(lc.ck.Read(cp, 1), *cp++);
                              vis->SetProperty(prop_index, (dword)buf);
                           }
                           break;
                        default:
                           assert(0);
                        }
#ifdef EDITOR
                        flags |= E_MODIFY_FLG_VISUAL;
#endif
                     }
                     --lc.ck;
                     break;

                  case CT_VISUAL_MATERIAL:
                     if(frm && frm->GetType()==FRAME_VISUAL){
                        col_mat = lc.ck.RIntChunk();
                        col_mat_ok = true;
#ifdef EDITOR
                        flags |= E_MODIFY_FLG_COL_MAT;
#endif
                     }else
                        --lc.ck;
                     break;

                  case CT_MODIFY_BRIGHTNESS:
                     if(frm && frm->GetType()==FRAME_MODEL){
                        brighness = lc.ck.RFloatChunk();
                        bright_ok = true;
#ifdef EDITOR
                        flags |= E_MODIFY_FLG_BRIGHTNESS;
#endif
                     }else
                        --lc.ck;
                     break;

                  default:
                     lc.WarnLog(C_fstr("skipping chunk: 0x%.4x, size: %i", ck_t, lc.ck), NULL);
                     --lc.ck;
                  }
               }
               if(frm){
                  if(created){
                     if(lc.loaded_model){
                        lc.model_frames.insert(frm);
                     }else{
                        lc.container->AddFrame(frm);
                     }
                     frm->Release();
                  }

#ifdef EDITOR                 //save modification (must do it before actually applying changes)
                  if(lc.e_modify)
                     lc.e_modify->AddFrameFlags(frm, flags, false);
#endif//EDITOR
                  if(link_frm){
                     frm->LinkTo(link_frm);
                              //set owner for volumes which are part of model
                     if(frm->GetType() == FRAME_VOLUME){
                        //I3DCAST_VOLUME(frm)->SetCategoryBits(VOLCAT_STANDARD);
                     }
                  }

                  if(pos_ok)
                     frm->SetPos(pos);
                  if(rot_ok)
                     frm->SetRot(rot);
                  if(scl_ok)
                     frm->SetScale(scale);
                  if(nu_scl_ok){
                     if(frm->GetType()==FRAME_VOLUME)
                        I3DCAST_VOLUME(frm)->SetNUScale(nu_scale);
                  }
                  if(vol_t)
                     I3DCAST_VOLUME(frm)->SetVolumeType(vol_t);
                  if(col_mat_ok){
                     switch(frm->GetType()){
                     case FRAME_VOLUME: I3DCAST_VOLUME(frm)->SetCollisionMaterial(col_mat); break;
                     case FRAME_VISUAL: I3DCAST_VISUAL(frm)->SetCollisionMaterial(col_mat); break;
                     default: assert(0);
                     }
                  }
                  if(f_flgs_ok)
                     frm->SetFlags(frm_flags);
                  if(bright_ok)
                     SetModelBrightness(I3DCAST_MODEL(frm), brighness);

                  if(hide)
                     frm->SetOn(false);
                           //created un-linked frames are automatically placed into sector
                  if(created && !link_frm){
                     if(lc.open_flags&OPEN_MODEL){
                        frm->LinkTo(lc.loaded_model);
                     }else
                        scene->SetFrameSector(frm);
                  }
               }
               break;
            }
            --lc.ck;
         }
      }
      break;

   case CT_BSP_FILE:
      if(lc.open_flags&OPEN_MODEL) break;
      if(lc.merge_mode) break;
      {
         bool bsp_ok = scene->LoadBsp(lc.ck.GetHandle(), lc.load_cb_proc, lc.load_cb_context
#if (defined EDITOR) || (defined _DEBUG)
            , FBSPLOAD_CONSISTENCE | FBSPLOAD_CHECKSUM
#endif
            );
         if(!bsp_ok){
            lc.WarnLog("Failed to load bsp information.", NULL);
         }
      }
      break;

   case CT_POSE_ANIMS:
#ifdef EDITOR
      {
         if(lc.open_flags&(OPEN_MODEL | OPEN_NO_EDITOR))
            break;
         PC_editor_item ei = editor->FindPlugin("AnimEdit");
         if(ei){
            ei->LoadFromMission(lc.ck);
         }else{
            lc.WarnLog("Cannot find editor plugin 'AnimEdit'", NULL);
         }
      }
#endif //EDITOR
      break;

   case CT_CAMERA_PATHS:
#ifdef EDITOR
      {
         if(lc.open_flags&(OPEN_MODEL | OPEN_NO_EDITOR))
            break;
         PC_editor_item ei = editor->FindPlugin("CameraPath");
         if(ei){
            ei->LoadFromMission(lc.ck);
         }else{
            lc.WarnLog("Cannot find editor plugin 'CameraPath'", NULL);
         }
      }
#endif //EDITOR
      break;

   case CT_EDITOR_PLUGIN:
#ifdef EDITOR
      if(lc.merge_mode || !editor) break;
      if(lc.open_flags&OPEN_MODEL) break;
      if(!(lc.open_flags&OPEN_NO_EDITOR)){
         C_str str;
         if(lc.ck.ReadString(CT_NAME, str)){
            PC_editor_item ei = editor->FindPlugin(str);
            if(ei){
               ei->LoadFromMission(lc.ck);
            }else{
               lc.WarnLog(C_fstr("Cannot find plugin: %s", (const char*)str), NULL);
            }
         }
      }
#endif   //EDITOR
      break;

   case CT_COPYRIGHT:
   case CT_VERSION:
   case CT_THUMBNAIL:
                              //silently skip these
      break;

                              //silently ignored (inherited classes may load this)
   case CT_MISSION_TABLE:
      break;

   case CT_PHYSICS_TEMPLATE:
#ifdef EDITOR
      if(!(lc.open_flags&OPEN_MODEL)){
         PC_editor_item ei = editor->FindPlugin("Physics Studio");
         if(ei)
            ei->LoadFromMission(lc.ck);
      }
#endif
      break;
   default:
      lc.WarnLog(C_fstr("skipping chunk: 0x%.4x, size: %i", ck_t, lc.ck), NULL);
   }
   return true;
}

//----------------------------

E_MISSION_IO C_mission::Save(const char *bin_name, dword save_group) const{

   int i;

   C_fstr bin_str("missions\\%s\\scene.bin", bin_name);

                              //check if we have right to save to this file
   if(OsIsFileReadOnly(bin_str))
      return MIO_ACCESSFAIL;

                              //backup old file
   OsMoveFile(bin_str, C_fstr("missions\\_backup\\%s\\scene.bin", bin_name));

   C_chunk ck(256*1024);
   if(!ck.WOpen(bin_str)){
      if(!OsCreateDirectoryTree(bin_str))
         return MIO_ACCESSFAIL;
      if(!ck.WOpen(bin_str))
         return MIO_ACCESSFAIL;
   }

   ck <<= CT_BASECHUNK;
   {                          //version + copyright
      ck <<= CT_VERSION;
      i = VERSION_MAJOR;
      ck.Write(&i, 2);
      i = VERSION_MINOR;
      ck.Write(&i, 2);
      static const char copyright[] = " - Mission file - Copyright (c) 2001  Lonely Cat Games - ";
      ck.Write(copyright, sizeof(copyright)+1);
      --ck;
   }
#ifdef EDITOR
   editor->MissionSave(ck);
#endif
   ck.Close();
                              //sync our CRC with file on disk
   GetMissionCRC(mission_name, mission_crc);
   return MIO_OK;
}

//----------------------------

bool C_mission::GetMissionCRC(const C_str &name, dword &out_crc){

   C_fstr scene_name("missions\\%s\\scene.i3d", (const char*)name);
   C_fstr bin_name("missions\\%s\\scene.bin", (const char*)name);

   out_crc = 0;
                              //try to open both, get size and date
   bool b;
   dword ftime[2];

                              //get crc of scene file
   PC_dta_stream dta = DtaCreateStream(scene_name);
   if(!dta)
      return false;

   out_crc ^= dta->GetSize();

   b = dta->GetTime(ftime);
   dta->Release();
   if(!b)
      return false;
   out_crc ^= ftime[0] ^ ftime[1];

                                 //get crc of bin file
   dta = DtaCreateStream(bin_name);
   if(!dta)
      return false;

   out_crc ^= dta->GetSize();

   b = dta->GetTime(ftime);
   dta->Release();
   if(!b)
      return false;
   out_crc ^= ftime[0] ^ ftime[1];

   return true;
}

//----------------------------
//----------------------------

