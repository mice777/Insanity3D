#include "all.h"
#include "common.h"
#include <fps_counter.h>
#include <win_res.h>
#include <insanity\3DTexts.h>
//#include "inet2.h"

//----------------------------

class C_editor_item_Stats_imp: public C_editor_item_Stats{
   virtual const char *GetName() const{ return "Stats"; }

   //C_smart_ptr<I_net> net; //for stats

//----------------------------

   enum E_ACTION_STATS{
      E_STATS_FPS = 9000,        // - toggles FPS counter
      E_STATS_DISABLE,           // - hides all stats
      E_STATS_VIDEO,             // - toggle video stats
      E_STATS_SYSMEM,            // - toggle sysmem stats
      E_STATS_COLLISIONS,        // - toggle collision stats
      E_STATS_SCENE,             // - toggle scene stats
      E_STATS_SELECTION,         // - toggle stats about current scene
      E_STATS_SOUND,             // - toggle sound stats 
      E_STATS_NET,               // - toggle network stats
      E_STATS_BSP,               // - toggle bsp stats
      E_STATS_VOLTREE,           // - toggle volume tree stats
      E_STATS_GEOMETRY,          // - toggle geometry stats
      //E_STATS_SET_NET_TEXT,      //(const char*) - save network stats string, for displaying if net stats are selected
      //E_STATS_RENDER_STATS,      //(const S_user_stats*) - render user stats
      E_STATS_SUMMARY,           // - display scene summary information
      /*
      E_STATS_STATUS_BAR,        // - toggle status bar
      E_STATS_ADD_SB_TEXT,       //(const char*) - internal - set SB text
      E_STATS_SET_MODIFIED,      //(bool) - internal - set modified flag
      */
   };

//----------------------------

   int stats_index;
   bool show_fps;

   int curr_count;            //used for state update

   //C_str net_stats;        //saved network text

   C_fps_counter fps_cnt;

//----------------------------

   float curr_fps_count;

//----------------------------

   void DrawRect(float x, float y, float sx1, float sy1) const{

      PI3D_driver drv = ed->GetDriver();
      bool is_zb = drv->GetState(RS_USEZB);
      if(is_zb)
         drv->SetState(RS_USEZB, false);
      drv->SetTexture(NULL);
      const I3D_rectangle &vp = ed->GetScene()->GetViewport();
      int sy = (vp.r - vp.l);

      //if(bottom)
         //sy *= (float)ed->GetIGraph()->Scrn_sy() / ((float)ed->GetIGraph()->Scrn_sx());

      S_vectorw v[4];
      for(int i=0; i<4; i++){
         v[i].x = vp[0] + sy * x;
         v[i].y = vp[1] + sy * y;
         v[i].z = 0.0f;
         v[i].w = 1.0f;
         if(i&1) v[i].x += sy * sx1;
         if(i&2) v[i].y += sy * sy1;
      }
      static const word indx[] = {0, 1, 2, 2, 1, 3};
      ed->GetScene()->DrawTriangles(v, 4, I3DVC_XYZRHW, indx, sizeof(indx)/sizeof(word), 0x80000000);
      if(is_zb)
         drv->SetState(RS_USEZB, true);
   }

//----------------------------

   //C_smart_ptr<C_poly_text> texts;         //polygon font system
   C_smart_ptr<C_text> text, text_fps;
   float rect_sy;

//----------------------------

   C_text *CreateText(const char *cp, float x, float y, float size, float &rect_sy) const{


      int numl;
      S_text_create ct;
      ct.tp = cp;
      ct.x = x;
      ct.y = y;
      ct.color = 0xc0ffffff;
      ct.size = size;
      ct.num_lines = &numl;
      PC_poly_text texts = ed->GetTexts();
      PC_text text = texts->CreateText(ct);

      rect_sy = size * (numl + .5f);

      return text;
   }

//----------------------------

   void DestroyText(){
      text = NULL;
      rect_sy = 0.0f;
   }

//----------------------------

   void DestroyFPS(){
      text_fps = NULL;
   }

//----------------------------

   static bool GetVisualInfo(PI3D_frame frm, dword &num_verts, dword &num_faces){

      switch(frm->GetType()){
      case FRAME_VISUAL:
         {
            PI3D_visual vis = I3DCAST_VISUAL(frm);
            PI3D_mesh_base mb = vis->GetMesh();
            if(mb){
               num_verts += mb->NumVertices();
               num_faces += mb->NumFaces();
               return true;
            }
            switch(vis->GetVisualType()){
            case I3D_VISUAL_PARTICLE:
               {
                  dword num_el = vis->GetProperty(I3DPROP_PRTC_I_NUMELEMENTS);
                  num_verts += num_el*4;
                  num_faces += num_el*2;
               }
               break;
            }
         }
      }
      return false;
   }

//----------------------------

   static void AddModelInfo(PI3D_model mod, dword &num_verts, dword &num_faces, dword &num_meshs){

      struct S_hlp{
         dword num_verts;
         dword num_faces;
         dword num_meshes;
         bool (*GetVisualInfo)(PI3D_frame, dword&, dword&);
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

            S_hlp &hlp = *(S_hlp*)c;
            if(hlp.GetVisualInfo(frm, hlp.num_verts, hlp.num_faces))
               ++hlp.num_meshes;
            return I3DENUMRET_OK;
         }
      } hlp;
      hlp.num_verts = 0;
      hlp.num_faces = 0;
      hlp.num_meshes = 0;
      hlp.GetVisualInfo = GetVisualInfo;

      mod->EnumFrames(S_hlp::cbEnum, (dword)&hlp);
      num_verts += hlp.num_verts;
      num_faces += hlp.num_faces;
      num_meshs += hlp.num_meshes;
   }

//----------------------------

   void DisplaySummaryStats(){

      PC_editor_item_Log e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
      PC_editor_item_Modify e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");

      e_log->Clear();
      e_log->AddText("Scene summary:\n");

      struct S_hlp{
         C_vector<PI3D_model> uni_mod_list;
         dword num_models;

         static I3DENUMRET I3DAPI cbEnumModels(PI3D_frame frm, dword c){

            S_hlp *hp = (S_hlp*)c;
            ++hp->num_models;
            PI3D_model mod = I3DCAST_MODEL(frm);

            for(int i=hp->uni_mod_list.size(); i--; ){
               if(hp->uni_mod_list[i]->GetFileName() == mod->GetFileName())
                  break;
            }
            if(i==-1)
               hp->uni_mod_list.push_back(mod);
            return I3DENUMRET_OK;
         }

         static I3DENUMRET I3DAPI cbCollectFrames(PI3D_frame frm, dword c){
            C_vector<PI3D_frame> &frm_list = *(C_vector<PI3D_frame>*)c;
            frm_list.push_back(frm);
            return I3DENUMRET_OK;
         }

         static bool cbLess(PI3D_model m1, PI3D_model m2){
            return (m1->GetFileName() < m2->GetFileName());
         }
         static I3DENUMRET I3DAPI cbEnum1(PI3D_frame frm, dword c){
            return I3DENUMRET_OK;
         }
         static bool cbLessTxt(PI3D_texture t1, PI3D_texture t2){
            return (t1->GetFileName() < t2->GetFileName());
         }
         
         static bool cbLessString(const C_str &s1, const C_str &s2){
            return false;
         }
         static bool cbLessSnd(PI3D_sound s1, PI3D_sound s2){
            return (s1->GetFileName() < s2->GetFileName());
         }

         static I3DENUMRET I3DAPI cbEnumSnd(PI3D_frame frm, dword c){

            PI3D_sound snd = I3DCAST_SOUND(frm);

            C_vector<PI3D_sound> &uni_snd_list = *(C_vector<PI3D_sound>*)c;
            for(int i=uni_snd_list.size(); i--; ){
               if(uni_snd_list[i]->GetFileName() == snd->GetFileName())
                  break;
            }
            if(i==-1)
               uni_snd_list.push_back(snd);
            return I3DENUMRET_OK;
         }
      } hlp;
      hlp.num_models = 0;
      ed->GetScene()->EnumFrames(S_hlp::cbEnumModels, (dword)&hlp, ENUMF_MODEL);
                              //remove non-created models
      for(dword i=hlp.uni_mod_list.size(); i--; ){
         PI3D_model mod = hlp.uni_mod_list[i];
         dword flgs = e_modify->GetFrameFlags(mod);
         if(!(flgs&E_MODIFY_FLG_CREATE)){
            hlp.uni_mod_list[i] = hlp.uni_mod_list.back();
            hlp.uni_mod_list.pop_back();
         }
      }

                              //compute total size of models on disk
      dword total_size = 0;
      for(i=0; i<hlp.uni_mod_list.size(); i++){
         C_str fname = hlp.uni_mod_list[i]->GetFileName();
         fname += ".i3d";
         dword size = GetFileSize(fname);
         if(size!=-1)
            total_size += size;
      }
      e_log->AddText(C_fstr("Models:\n  %i total\n  %i unique\n  size on disk: %.1f KB\n",
         hlp.num_models, hlp.uni_mod_list.size(), 
         (float)total_size/1024.0f));

                              //collect all frames
      C_vector<PI3D_frame> frm_list;
      ed->GetScene()->EnumFrames(S_hlp::cbCollectFrames, (dword)&frm_list);

      e_log->AddText(C_fstr("Frames:\n  %i total\n", frm_list.size()));

                              //collect unique meshes
      set<PI3D_mesh_base> mb_list;
      set<CPI3D_material> mat_list;
      dword num_faces = 0;
      dword num_verts = 0;
      dword num_meshes = 0;
      dword num_fgroups = 0;

      for(i=frm_list.size(); i--; ){
         PI3D_frame frm = frm_list[i];
         switch(frm->GetType()){
         case FRAME_VISUAL:
            {
               PI3D_visual vis = I3DCAST_VISUAL(frm);
               PI3D_mesh_base mb = vis->GetMesh();
               if(mb){
                  ++num_meshes;
                  num_fgroups += mb->NumFGroups();
                  mb_list.insert(mb);
                  num_faces += mb->NumFaces();
                  num_verts += mb->NumVertices();
               }else{
                        //specialized visuals
                  PI3D_material mat = vis->GetMaterial();
                  if(mat)
                     mat_list.insert(mat);
               }
            }
            break;
         }
      }

      e_log->AddText(C_fstr("Geometry:\n  %i unique meshes\n  %i faces\n  %i vertices\n  ~%.2f mats per mesh",
         mb_list.size(),
         num_faces, num_verts,
         num_meshes ? (float)num_fgroups / (float)num_meshes : 0.0f));


                        //collect unique materials
      for(set<PI3D_mesh_base>::const_iterator it=mb_list.begin(); it!=mb_list.end(); it++){
         PI3D_mesh_base mb = *it;
         for(int j=mb->NumFGroups(); j--; ){
            mat_list.insert(((PI3D_face_group)&mb->GetFGroups()[j])->GetMaterial());
         }
      }

      e_log->AddText(C_fstr("Materials:\n  %i unique\n",
         mat_list.size()));

                        //collect unique textures
      set<C_str> txt_name_list;
      dword total_tex_pixels = 0;
      for(set<CPI3D_material>::const_iterator mat_it=mat_list.begin(); mat_it!=mat_list.end(); mat_it++){
         CPI3D_material mat = *mat_it;
         for(int mti=0; mti<MTI_LAST; mti++){
            CPI3D_texture tp = ((PI3D_material)mat)->GetTexture((I3D_MATERIAL_TEXTURE_INDEX)mti);
            if(tp){
               for(i=0; i<2; i++){
                  const C_str &fname = tp->GetFileName(i);
                  if(fname.Size())
                     txt_name_list.insert(fname);
               }
               total_tex_pixels += tp->SizeX() * tp->SizeY();
            }
         }
      }
      e_log->AddText(C_fstr("Textures:\n  %i unique\n  total pixels: %.2f mil\n",
         txt_name_list.size(),
         (float)total_tex_pixels/1000000.0f));

                              //collect all sounds
      C_vector<PI3D_sound> uni_snd_list;
      ed->GetScene()->EnumFrames(S_hlp::cbEnumSnd, (dword)&uni_snd_list, ENUMF_SOUND);
      total_size = 0;
      dword play_time = 0;
      for(i=uni_snd_list.size(); i--; ){
         PI3D_sound snd = uni_snd_list[i];
         play_time += snd->GetPlayTime();
         if(snd->GetSoundSource())
            total_size += snd->GetSoundSource()->GetFormat()->size;
      }

      play_time /= 1000;
      e_log->AddText(C_fstr("Sounds:\n  %i unique\n  play time: %i:%.2i\n  size: %.1f kb\n",
         uni_snd_list.size(),
         play_time/60, play_time%60,
         (float)total_size/1024.0f));

      e_log->AddText("---------------\n");
   }

//----------------------------

   mutable C_vector<C_smart_ptr<C_text> > last_render_txt;

//----------------------------

   virtual void RenderStats(float l, float t, float sx, float sy, const char *text, float text_size) const{

      DrawRect(l, t, sx, sy);
      float rect_sy = 0;
      C_text *txt = CreateText(text, l+text_size*.1f, t + text_size, text_size, rect_sy);
      txt->Render(ed->GetScene());
      last_render_txt.push_back(txt);
      txt->Release();
   }

//----------------------------

   virtual void AfterLoad(){
      curr_count = 0;
   }

//----------------------------

   virtual void OnDeviceReset(){
      DestroyText();
      DestroyFPS();
      curr_count = 0;
   }

//----------------------------

void ComputeSectorAmbient(CPI3D_frame frm, float &ret_amb_lm, float &ret_amb_vx){

   ret_amb_lm = .0f;
   ret_amb_vx = .0f;
   for(CPI3D_frame sct1 = frm; sct1=sct1->GetParent(), sct1; ){
      if(sct1->GetType()==FRAME_SECTOR){
         CPI3D_sector sct = I3DCAST_CSECTOR(sct1);
         for(dword i=sct->NumLights(); i--; ){
            CPI3D_light lp = sct->GetLight(i);
            if(lp->GetLightType()==I3DLIGHT_AMBIENT){
               if(lp->GetMode()&I3DLIGHTMODE_LIGHTMAP)
                  ret_amb_lm += lp->GetColor().GetBrightness() * lp->GetPower();
               if(lp->GetMode()&I3DLIGHTMODE_VERTEX)
                  ret_amb_vx += lp->GetColor().GetBrightness() * lp->GetPower();
            }
         }
         break;
      }
   }
}

//----------------------------

   void GetGeometryStats(C_str &ret_str){

      PC_editor_item_MouseEdit e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      int mode = e_medit->GetCurrentMode();
      if(mode!=0){
         ret_str = "---";
         return;
      }

      PIGraph ig = ed->GetIGraph();
      int mx = ig->Mouse_x(), my = ig->Mouse_y();
      S_vector pos, dir;
      PI3D_scene scn = ed->GetScene();
      if(I3D_SUCCESS(scn->UnmapScreenPoint(mx, my, pos, dir))){
         /*
         struct S_hlp{
            float best_dist;
            int face_index;
            static bool I3DAPI cbHit(I3D_cresp_data &rd){
               return true;
            }
         } hlp;
         */
         CPI3D_camera cam = scn->GetActiveCamera();
         float n, f;
         cam->GetRange(n, f);
         float near_dist = n/cosf(dir.AngleTo(cam->GetWorldDir()));
         pos += dir * near_dist;

         I3D_collision_data cd(pos, dir, I3DCOL_EXACT_GEOMETRY |
            I3DCOL_COLORKEY | I3DCOL_RAY,
            NULL);
         //cd.cresp_face = S_hlp::cbHit;
         //cd.cresp_context = &hlp;
         if(scn->TestCollision(cd)){
            CPI3D_frame frm = cd.GetHitFrm();
            C_str mat_info;
            int fi = cd.GetFaceIndex();
            if(fi!=-1){
               if(frm->GetType()==FRAME_VISUAL){
                  PI3D_mesh_base mb = I3DCAST_VISUAL(const_cast<PI3D_frame>(frm))->GetMesh();
                  if(mb){
                     CPI3D_face_group fgs = mb->GetFGroups();
                     for(dword i=mb->NumFGroups(); i--; ){
                        if(fgs[i].base_index <= (dword)fi){
                           CPI3D_material mat = ((PI3D_face_group)&fgs[i])->GetMaterial();
                           mat_info = C_fstr("Material: %s\n", (const char*)mat->GetName());
                           static const char *map_name[MTI_LAST] = {
                              "Diff", "Env", "EMBM", "Det", "Bump", "Sec",
                           };
                           static const I3D_MATERIAL_TEXTURE_INDEX indices[MTI_LAST] = {
                              MTI_DIFFUSE, MTI_ENVIRONMENT, MTI_EMBM, MTI_DETAIL, MTI_NORMAL, MTI_SECONDARY
                           };
                           for(dword i=0; i<MTI_LAST; i++){
                              PI3D_texture tp = ((PI3D_material)mat)->GetTexture(indices[i]);
                              if(tp)
                                 mat_info += C_fstr("%s: %s\n", map_name[i], (const char*)tp->GetFileName());
                           }
                           mat_info[mat_info.Size()-1] = 0;
                           break;
                        }
                     }
                  }
               }
            }
                              //light info
            C_str str_light;
            {
               const S_vector &hit_pos = cd.ComputeHitPos();
               float l_lm_pixel(-1.0f);
                               //light on lm pixel 
               if(frm->GetType()==FRAME_VISUAL && I3DCAST_CVISUAL(frm)->GetVisualType() == I3D_VISUAL_LIT_OBJECT){
                  if(fi != -1){
                     CPI3D_lit_object lm = I3DCAST_CLIT_OBJECT(I3DCAST_CVISUAL(frm));
                     S_vector v_color;
                     if(lm->GetLMPixel((word)fi, hit_pos * lm->GetInvMatrix(), v_color))
                        l_lm_pixel = v_color.GetBrightness();
                  }
               }
                                 //vertext light
               const S_vector up_dir(.0f, 1.0f, .0f);
               float l_vert_sum = scn->GetLightness(hit_pos, &up_dir, I3DLIGHTMODE_VERTEX).GetBrightness();
                                 //lm light
               float l_lm_sum = scn->GetLightness(hit_pos, &up_dir, I3DLIGHTMODE_LIGHTMAP).GetBrightness();
                              //dynamic lm
               float l_dynamic_lm = scn->GetLightness(hit_pos, &up_dir, I3DLIGHTMODE_DYNAMIC_LM).GetBrightness();
                                 //lm ambient and vertex ambient (in real scene should be same)
               float l_ambient_lm, l_ambient_vx;
               ComputeSectorAmbient(frm, l_ambient_lm, l_ambient_vx);
                              //create light info string
               str_light = C_fstr("\n Light: \n"
                  " Lm pixel: %.2f\n"
                  " lm sum: %.2f\n"
                  " dyn lm: %.2f\n"
                  " vertx sum: %.2f\n"
                  " lm amb: %.2f\n" 
                  " vertx amb: %.2f"
                  , l_lm_pixel, l_lm_sum, l_dynamic_lm, l_vert_sum, l_ambient_lm, l_ambient_vx);
            }

            ret_str = C_fstr(
               "Frame: %s\n"
               "Distance: %.2f\n"
               "%s"
               ,
               (const char*)frm->GetName(),
               cd.GetHitDistance(),
               (const char*)mat_info
            );
            if(str_light.Size())
               ret_str += str_light;
            frm->DebugDraw(scn);
         }else{
            ret_str = "no collision";
         }
      }else{
         ret_str = "error unmapping screen pos";
      }
   }

//----------------------------
public:
   C_editor_item_Stats_imp():
      stats_index(E_STATS_SCENE),
      show_fps(true),
      text(NULL), text_fps(NULL),
      rect_sy(0.0f),
      curr_fps_count(0.0f),
      curr_count(0)
   {
   }
   ~C_editor_item_Stats_imp(){
      Close();
   }

   virtual bool Init(){

//#define MENU_BASE "&View\\%90 %i &Stats\\"
#define MENU_BASE "&Stats\\"
      ed->AddShortcut(this, E_STATS_FPS, MENU_BASE"&FPS\tF", K_F, 0);
      ed->AddShortcut(this, E_STATS_VIDEO, MENU_BASE"&Video\tCtrl+1", K_1, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_SYSMEM, MENU_BASE"System &memory\tCtrl+2", K_2, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_COLLISIONS, MENU_BASE"&Collisions\tCtrl+3", K_3, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_SCENE, MENU_BASE"&Scene\tCtrl+4", K_4, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_SELECTION, MENU_BASE"&Selection\tCtrl+5", K_5, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_SOUND, MENU_BASE"Sound\tCtrl+6", K_6, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_NET, MENU_BASE"Network\tCtrl+7", K_7, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_BSP, MENU_BASE"&Bsp\tCtrl+8", K_8, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_VOLTREE, MENU_BASE"Volume &tree\tCtrl+9", K_9, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_GEOMETRY, MENU_BASE"&Geometry\tCtrl+0", K_0, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_SUMMARY, MENU_BASE"S&ummary\tCtrl+-", K_MINUS, SKEY_CTRL);
      ed->AddShortcut(this, E_STATS_DISABLE, MENU_BASE"&Disable", K_NOKEY, 0);

      return true;
   }

//----------------------------

   virtual void Close(){

      DestroyText();
      DestroyFPS();
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){
      case E_STATS_VIDEO:
      case E_STATS_SYSMEM:
      case E_STATS_COLLISIONS:
      case E_STATS_SCENE:
      case E_STATS_SELECTION:
      case E_STATS_SOUND:
      case E_STATS_NET:
      case E_STATS_BSP:
      case E_STATS_VOLTREE:
      case E_STATS_GEOMETRY:
         {
            if(stats_index!=-1) ed->CheckMenu(this, stats_index, false);
            if(stats_index==id){
               stats_index = -1;
               DestroyText();
               break;
            }
            stats_index = id;
            ed->CheckMenu(this, id, true);
            curr_count = 0;
         }
         break;

      case E_STATS_FPS:
         if(show_fps){
            DestroyFPS();
         }
         show_fps = !show_fps;
         curr_fps_count = 0.0f;
         ed->CheckMenu(this, id, show_fps);
         break;

      case E_STATS_SUMMARY:
         DisplaySummaryStats();
         break;

      /*case E_STATS_SET_NET_TEXT:
         if(stats_index==E_STATS_NET)
            net_stats = (const char*)context;
         break;*/
      }
      return 0;
   }

//----------------------------

   virtual void RegisterNet(class I_net *pnet){

      //net = pnet;
   }


//----------------------------

   virtual void Render(){

      if(!show_fps && stats_index==-1)
         return;

      PI3D_driver drv = ed->GetDriver();
      PI3D_scene scn = ed->GetScene();
      bool was_wire = drv->GetState(RS_WIREFRAME);
      if(was_wire) drv->SetState(RS_WIREFRAME, false);

      bool was_txt = drv->GetState(RS_DRAWTEXTURES);
      if(!was_txt) drv->SetState(RS_DRAWTEXTURES, true);

      float y = .005f;
      const float SIZE = .0185f;
      if(show_fps){
         DrawRect(.005f, y, .12f, .0262f);
         float rect_sy;
         text_fps = CreateText(C_fstr("fps: %.2f", curr_fps_count), .01f, y+SIZE, SIZE, rect_sy);
         text_fps->Release();
         y += .03f;
      }

      C_str str;

      switch(stats_index){
      case E_STATS_VIDEO: 
         if(curr_count<0){
            curr_count = 200;
            I3D_stats_video st;
            drv->GetStats(I3DSTATS_VIDEO, &st);
            str = C_fstr(
               "Video memory:\n"
               " total: %.1f MB\n"
               " free: %.1f MB\n"
               "Texture memory:\n"
               " total: %.1f MB\n"
               " free: %.1f MB\n"
               "Textures:\n"
               " total: %i\n"
               " in video mem: %i\n"
               " sysmem used: %.1f MB\n"
               " vidmem used: %.1f MB",
               (float)st.vidmem_total/1048576,
               (float)st.vidmem_free/1048576,
               (float)st.txtmem_total/1048576,
               (float)st.txtmem_free/1048576,
               st.txt_total,
               st.txt_vidmem,
               (float)st.txt_sys_mem/1048576,
               (float)st.txt_video_mem/1048576
               );
         }
         break;

      case E_STATS_SYSMEM:
         if(curr_count<0){
            curr_count = 500;

            I3D_stats_memory st;
            drv->GetStats(I3DSTATS_MEMORY, &st);
            dword mem_used = 0, num_blocks = 0;
            /*
#if defined _MSC_VER
#ifdef _DEBUG
            _CrtMemState state;
            _CrtMemCheckpoint(&state);
            num_blocks = state.lCounts[_NORMAL_BLOCK];
            mem_used = state.lSizes[_NORMAL_BLOCK];
#endif
#endif
            */
            str = C_fstr(
               "System memory stats:\n"
               " used (total): %.1f kb\n"
               " used (engine): %.1f kb\n"
               "  in %i blocks\n"
               " used (game): %.1f kb\n"
               "  in %i blocks\n"
               "Database:\n"
               " total: %i kb\n"
               " used: %i kb",
               //(float)st.available/1024.0f,
               (float)(st.used + st.used_other + mem_used)/1024.0f,
               (float)(st.used + st.used_other)/1024.0f,
               st.num_blocks,
               (float)mem_used/1024.0f,
               num_blocks,
               st.dbase_size / 1024,
               st.dbase_used / 1024
               );
         }
         break;

      case E_STATS_COLLISIONS:
         {
            I3D_stats_volume st;
            scn->GetStats(I3DSTATS_VOLUME, &st);
            str = C_fstr(
               "Collision stats:\n"
               " ls: %i"
               " lr: %i\n"
               " lb: %i"
               " lf: %i\n"
               " ss: %i"
               " sr: %i\n"
               " sb: %i"
               " sf: %i"
               ,
               st.line_sphere,
               st.line_rectangle,
               st.line_box,
               st.line_face,
               st.sphere_sphere,
               st.sphere_rectangle,
               st.sphere_box,
               st.sphere_face
               );
         }
         break;

      case E_STATS_SCENE:
         {
            I3D_stats_render st;
            scn->GetStats(I3DSTATS_RENDER, &st);
            I3D_stats_scene st1;
            scn->GetStats(I3DSTATS_SCENE, &st1);
            str = C_fstr(
               "Scene stats:\n"
               " tri: %i\n"
               " vert: %i\n"
               " vis: %i\n"
               " snd: %i\n"
               " sct: %i\n"
               " occ: %i\n"
               " comp.: %i\n"
               " shd: %i",
               st.triangle,
               st.vert_trans,
               st1.frame_count[FRAME_VISUAL],
               st1.frame_count[FRAME_SOUND],
               st1.frame_count[FRAME_SECTOR],
               st1.frame_count[FRAME_OCCLUDER],
               st1.num_vis_computed,
               st1.dyn_shd_casters
               );
         }
         break;

      case E_STATS_SELECTION:
         {
            const C_vector<PI3D_frame> &sel_list = ((PC_editor_item_Selection)ed->FindPlugin("Selection"))->GetCurSel();
            switch(sel_list.size()){
            case 0:
               str += "<no selection>";
               break;
            case 1:
               {
                  PI3D_frame frm = sel_list.front();
                  switch(frm->GetType()){
                  case FRAME_VISUAL:
                     {
                        PI3D_visual vis = I3DCAST_VISUAL(frm);
                        dword num_verts = 0, num_faces = 0;
                        GetVisualInfo(vis, num_verts, num_faces);
                        str = C_fstr("%s v: %i, f: %i\n",
                           (const char*)str,
                           num_verts,
                           num_faces);
                     }
                     break;

                  case FRAME_MODEL:
                     {
                        PI3D_model mod = I3DCAST_MODEL(frm);
                        /*struct S_hlp{
                           dword num_verts;
                           dword num_faces;
                           dword num_meshes;
                           bool (*GetVisualInfo)(PI3D_frame, dword&, dword&);
                           static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

                              S_hlp &hlp = *(S_hlp*)c;
                              if(hlp.GetVisualInfo(frm, hlp.num_verts, hlp.num_faces))
                                 ++hlp.num_meshes;
                              return I3DENUMRET_OK;
                           }
                        } hlp;
                        hlp.num_verts = 0;
                        hlp.num_faces = 0;
                        hlp.num_meshes = 0;
                        hlp.GetVisualInfo = GetVisualInfo;

                        mod->EnumFrames(S_hlp::cbEnum, (dword)&hlp);*/

                        dword num_verts(0); dword num_faces(0); dword num_meshes(0); 
                        AddModelInfo(mod, num_verts, num_faces, num_meshes);

                        str = C_fstr("Meshes: %i\nv: %i, f: %i",
                           num_meshes, num_verts, num_faces);
                     }
                     break;

                  case FRAME_SECTOR:
                     {
                        PI3D_sector sct = I3DCAST_SECTOR(frm);
                        str += C_fstr(
                           "\n"
                           "lights: %i"
                           ,
                           sct->NumLights()
                           );
                     }
                     break;

                  default:
                     str = "";
                  }
               }
               break;
            default:
               {
                  dword num_verts = 0;
                  dword num_faces = 0;
                  dword num_meshes = 0;
                  for(int i=sel_list.size(); i--; ){
                     if(sel_list[i]->GetType() == FRAME_MODEL){
                              //add info about meshes in model
                        AddModelInfo(I3DCAST_MODEL(sel_list[i]), num_verts, num_faces, num_meshes);
                     }else{
                              //add visual info
                        if(GetVisualInfo(sel_list[i], num_verts, num_faces))
                           ++num_meshes;
                     }
                  }
                  str = C_fstr("Meshes: %i\nv: %i, f: %i",
                     num_meshes, num_verts, num_faces);
               }
            }
         }
         break;

      case E_STATS_SOUND:
         {
            I3D_stats_sound st;
            scn->GetStats(I3DSTATS_SOUND, &st);
            str = C_fstr(
               "Sound stats:\n"
               " all: %i\n"
               " audible: %i\n"
               " in card: %i\n"
               " mem used: %.1f Kb\n"
               "Hardware:\n"
               " hw all: %i free: %i\n"
               " 3D all: %i free: %i\n"
               " mem all: %i\n"
               " mem free: %i",
               st.snds_all,
               st.snds_audible,
               st.snds_in_card,
               (float)st.mem_used / 1024.0f,
               st.buf_hw_all,
               st.buf_hw_free,
               st.buf_3d_all,
               st.buf_3d_free,
               st.hw_mem_all,
               st.hw_mem_free
               );
         }
         break;

      case E_STATS_BSP:
         {
            I3D_stats_bsp st;
            scn->GetStats(I3DSTATS_BSP, &st);
            str = C_fstr(
               "Bsp stats:\n"
               " num nodes: %i\n"
               " depth: %i\n"
               " num planes: %i\n"
               " num faces: %i\n"
               " faces ref: %i\n"
               " vertices: %i\n"
               " mem used: %.1f Kb",
               st.num_nodes,
               st.depth,
               st.num_planes,
               st.num_faces,
               st.face_references,
               st.num_vertices,
               (float)st.mem_used / 1024.0f
               );
         }
         break;
      case E_STATS_VOLTREE:
         {
            I3D_stats_voltree st;
            scn->GetStats(I3DSTATS_VOLTREE, &st);
            str = C_fstr(
               "Volume tree stats:\n"
               " total volumes: %i\n"
               " root volumes: %i\n"
               " moving static: %i"
               //" reserved2: %i\n"
               "",
               st.total_num,
               st.root_num,
               st.moving_static
               //st.reserved2
               );
         }
         break;

      case E_STATS_NET:
         {
            /*
            if(net){
               //dword in, out, out_ng;
               I3D_stats_net st;
               net->GetStats(st);
               C_fstr dp_stats(
                  "dwRoundTripLatencyMS: %i\n"
                  "dwThroughputBPS: %i\n"
                  "dwPeakThroughputBPS: %i\n"
                  "dwBytesSentGuaranteed: %i\n"
                  "dwPacketsSentGuaranteed: %i\n"
                  "dwBytesSentNonGuaranteed: %i\n"
                  "dwPacketsSentNonGuaranteed: %i\n"
                  "dwBytesRetried: %i\n"
                  "dwPacketsRetried: %i\n"
                  "dwBytesDropped: %i\n"
                  "dwPacketsDropped: %i\n"
                  "dwMessagesTransmittedHighPriority: %i\n"
                  "dwMessagesTimedOutHighPriority: %i\n"
                  "dwMessagesTransmittedNormalPriority: %i\n"
                  "dwMessagesTimedOutNormalPriority: %i\n"
                  "dwMessagesTransmittedLowPriority: %i\n"
                  "dwMessagesTimedOutLowPriority: %i\n"
                  "dwBytesReceivedGuaranteed: %i\n"
                  "dwPacketsReceivedGuaranteed: %i\n"
                  "dwBytesReceivedNonGuaranteed: %i\n"
                  "dwPacketsReceivedNonGuaranteed: %i\n"
                  "dwMessagesReceived: %i\n",
                  st.dp.dwRoundTripLatencyMS,
                  st.dp.dwThroughputBPS,
                  st.dp.dwPeakThroughputBPS,
                  st.dp.dwBytesSentGuaranteed,
                  st.dp.dwPacketsSentGuaranteed,
                  st.dp.dwBytesSentNonGuaranteed,
                  st.dp.dwPacketsSentNonGuaranteed,
                  st.dp.dwBytesRetried,
                  st.dp.dwPacketsRetried,
                  st.dp.dwBytesDropped,
                  st.dp.dwPacketsDropped,
                  st.dp.dwMessagesTransmittedHighPriority,
                  st.dp.dwMessagesTimedOutHighPriority,
                  st.dp.dwMessagesTransmittedNormalPriority,
                  st.dp.dwMessagesTimedOutNormalPriority,
                  st.dp.dwMessagesTransmittedLowPriority,
                  st.dp.dwMessagesTimedOutLowPriority,
                  st.dp.dwBytesReceivedGuaranteed,
                  st.dp.dwPacketsReceivedGuaranteed,
                  st.dp.dwBytesReceivedNonGuaranteed,
                  st.dp.dwPacketsReceivedNonGuaranteed,
                  st.dp.dwMessagesReceived
                  );
               str = C_fstr(
                  "Net stats:\n"
                  " in: %i\n"
                  " out: %i\n"
                  " out_ng: %i\n"
                  //" reserved2: %i\n"
                  "%s",
                  st.in,
                  st.out_g,
                  st.out_ng,
                  (const char*)dp_stats
                  //st.reserved2
                  );
            }
            */
         }
         break;

      case E_STATS_GEOMETRY:
         GetGeometryStats(str);
         break;

      case -1:
         break;

      default:
         assert(0);
      }

      if(stats_index!=-1){
         if(str.Size()){
            text = CreateText(str, .01f, y+SIZE, SIZE, rect_sy);
            text->Release();
         }
         if(rect_sy)
            DrawRect(.005f, y, .185f, rect_sy);
      }
      if(was_wire) drv->SetState(RS_WIREFRAME, true);
      if(!was_txt) drv->SetState(RS_DRAWTEXTURES, false);
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){
      curr_count -= time;
      if(show_fps){
         fps_cnt.Tick(time);
         curr_fps_count = fps_cnt.GetCount();
      }
      last_render_txt.clear();
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){
      
      int version = 0;
      ck.Read(&version, sizeof(int));
      if(version!=101) return false;

                              //write stats type
      ck.Read(&stats_index, sizeof(int));
      ck.Read(&show_fps, sizeof(byte));
      if(stats_index!=-1){
         ed->CheckMenu(this, stats_index, true);
      }
      ed->CheckMenu(this, E_STATS_FPS, show_fps);
      return true;
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      int version = 101;
      ck.Write(&version, sizeof(int));
                              //write stats type
      ck.Write(&stats_index, sizeof(int));
      ck.Write(&show_fps, sizeof(byte));
      return true;
   }
};

//----------------------------

void CreateStats(PC_editor ed){
   PC_editor_item ei = new C_editor_item_Stats_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
