#include "all.h"
#include "common.h"
#include <insanity\os.h>
#include <profile.h>
#include <win_res.h>

//----------------------------

#define RENDER_DELAY 50       //ms

//----------------------------

                              //table limits
const int LIFE_BASE_MIN = 1;
const int LIFE_BASE_MAX = 4000;
const int LIFE_RANDOM_MIN = 1;
const int LIFE_RANDOM_MAX = 4000;
const int EMIT_BASE_MIN = 1;
const int EMIT_BASE_MAX = 4000;
const int EMIT_RANDOM_MIN = 1;
const int EMIT_RANDOM_MAX = 4000;
const int RADIUS_GROW_MIN = 0;
const int RADIUS_GROW_MAX = 1;
//shift speed has range -SHIFT_SPEED_MAX to SHIFT_SPEED_MAX;
//negative sign means inward direction of waves
const int SHIFT_SPEED_MAX = 5; 

class C_edit_ProcEdit: public C_editor_item{
   virtual const char *GetName() const{ return "ProcEdit"; }

//----------------------------

   enum{
      ACTION_EDIT,
   };

//----------------------------
   enum{
      TAB_I_SIZE_BITS,
      TAB_I_LIFE_BASE,
      TAB_I_LIFE_RANDOM,
      TAB_I_EMIT_BASE,
      TAB_I_EMIT_RANDOM,
      TAB_B_TRUECOLOR,
      TAB_F_INIT_RADIUS,
      TAB_F_RADIUS_GROW,
      TAB_F_SHIFT_SPEED,
      TAB_F_WAVE_REPEAT,
      TAB_F_SCALE,
      TAB_F_HEIGHT_CURVE,
      TAB_F_PROCEDURE_SPEED,
   };

   static CPC_table_template GetTempl(){

      static const C_table_element te[] = {
         {TE_INT, TAB_I_SIZE_BITS, "Size (bits)", 2, 9, 6, "Size of one side of procedural texture (in bits). The greater you make the texture, the slower will be update when it should be displayed. Keep size of texture reasonably small (approx. 5 bits is fine)."},
         {TE_BOOL, TAB_B_TRUECOLOR, "True color", 0, 0, 0, "When chosen, the texture will use true-color texture format, which may improve graphics appearance of texture. If difference is unnotable, do not check this button."},
         {TE_INT, TAB_I_LIFE_BASE, "Life base", LIFE_BASE_MIN, LIFE_BASE_MAX, 1000, "Length of life of single circle element."},
         {TE_INT, TAB_I_LIFE_RANDOM, "Life random", LIFE_RANDOM_MIN, LIFE_RANDOM_MAX, 100, "Random time added to time length base."},
         {TE_INT, TAB_I_EMIT_BASE, "Emit base", EMIT_BASE_MIN, EMIT_BASE_MAX, 1000, "Time after which new circle elements are created."},
         {TE_INT, TAB_I_EMIT_RANDOM, "Emit random", EMIT_RANDOM_MIN, EMIT_RANDOM_MAX, 100, "Random time added to emit time base."},
         {TE_FLOAT, TAB_F_INIT_RADIUS, "Init radius", 0, 1, 0, "Initial radius of element circle. This value is in texture-units (1.0 means radisu as big as texture side)."},
         {TE_FLOAT, TAB_F_RADIUS_GROW, "Radius grow", RADIUS_GROW_MIN, RADIUS_GROW_MAX, 1, "Radius, how fast element circle is grown, per second."},
         {TE_FLOAT, TAB_F_SHIFT_SPEED, "Shift speed", -SHIFT_SPEED_MAX, SHIFT_SPEED_MAX, 1, "Speed of wave shifting in circle, per second."},
         {TE_FLOAT, TAB_F_WAVE_REPEAT, "Wave repeat", 0, 10, 2, "Number of waves repeated on circle from its center to border."},
         {TE_FLOAT, TAB_F_SCALE, "Scale", 0, 1, 1, "Multiplier of pixels written to texture. The default value is 1. This is something like contrast setting of procedural data."},
         {TE_FLOAT, TAB_F_HEIGHT_CURVE, "Height curve", 0, 1, 0, "Curveness of power fading accross circle's life."},
         {TE_FLOAT, TAB_F_PROCEDURE_SPEED, "Adjust speed", -1, 1, 0, "Procedure speed. Less then zero slow down procedure, greater speed it up. (1/2 - 2x)"},
         {TE_NULL}
      };
      static const C_table_template tt = { "Procedural parameters", te };
      return &tt;
   }

//----------------------------
                              //copy of values needed for speed change
   friend struct S_speed_table;

   struct S_speed_table{
                              //divide by speed:
      int life_base;          //TAB_I_LIFE_BASE,
      int life_rnd;           //TAB_I_LIFE_RANDOM,
      int emit_base;          //TAB_I_EMIT_BASE,
      int emit_rnd;           //TAB_I_EMIT_RANDOM,

                              //multiply by speed
      float radius_grow;      //TAB_F_RADIUS_GROW,
      float shift_speed;      //TAB_F_SHIFT_SPEED,

      float speed_min;
      float speed_max;

   } speed_table;

   void ComputeSpeedLimits(CPC_table tab, float *min, float *max){

      float speed_min(.1f);
      float speed_max(10.f);

      float tmp_max;
      float tmp_min;
      float tmp_val;

                              //divide by speed
      tmp_val = float(tab->GetItemI(TAB_I_LIFE_BASE));
      tmp_min = (LIFE_BASE_MAX) ? tmp_val / LIFE_BASE_MAX : 1.0f;
      tmp_max = (LIFE_BASE_MIN) ? tmp_val / LIFE_BASE_MIN : 10.0f;
      speed_max = Min(speed_max, tmp_max);
      speed_min = Max(speed_min, tmp_min);

      tmp_val = float(tab->GetItemI(TAB_I_LIFE_RANDOM));
      tmp_min = (LIFE_RANDOM_MAX) ? tmp_val / LIFE_RANDOM_MAX : 1.0f;
      tmp_max = (LIFE_RANDOM_MIN) ? tmp_val / LIFE_RANDOM_MIN : 10.0f;
      speed_max = Min(speed_max, tmp_max);
      speed_min = Max(speed_min, tmp_min);

      tmp_val = float(tab->GetItemI(TAB_I_EMIT_BASE));
      tmp_min = (EMIT_BASE_MAX) ? tmp_val / EMIT_BASE_MAX : 1.0f;
      tmp_max = (EMIT_BASE_MIN) ? tmp_val / EMIT_BASE_MIN : 10.0f;
      speed_max = Min(speed_max, tmp_max);
      speed_min = Max(speed_min, tmp_min);

      tmp_val = float(tab->GetItemI(TAB_I_EMIT_RANDOM));
      tmp_min = (EMIT_RANDOM_MAX) ? tmp_val / EMIT_RANDOM_MAX : 1.0f;
      tmp_max = (EMIT_RANDOM_MIN) ? tmp_val / EMIT_RANDOM_MIN : 10.f;
      speed_max = Min(speed_max, tmp_max);
      speed_min = Max(speed_min, tmp_min);

                              //multiplied by speed
      tmp_val = tab->GetItemF(TAB_F_RADIUS_GROW);
      tmp_max = (tmp_val) ? RADIUS_GROW_MAX / tmp_val : RADIUS_GROW_MAX;
      tmp_min = (tmp_val) ? RADIUS_GROW_MIN / tmp_val : .0f;
      speed_max = Min(speed_max, tmp_max);
      speed_min = Max(speed_min, tmp_min);

                              //SHIFT_SPEED can be negative for inward speed!
      tmp_val = tab->GetItemF(TAB_F_SHIFT_SPEED);
      tmp_max = (tmp_val) ? SHIFT_SPEED_MAX / I3DFabs(tmp_val) : SHIFT_SPEED_MAX;
      const int SHIFT_SPEED_MIN = 0; //min value is .0, bacause negative sign is just for direction of waves
      tmp_min = (tmp_val) ? SHIFT_SPEED_MIN / I3DFabs(tmp_val) : .0f;
      speed_max = Min(speed_max, tmp_max);
      speed_min = Max(speed_min, tmp_min);

                              //result 
      *min = speed_min;
      *max = speed_max;
      assert(*min >= .0f && *max >= .0f);
   }

   void ResetSpeedTable(PC_table tab){

      assert(tab);

      speed_table.life_base = tab->GetItemI(TAB_I_LIFE_BASE);
      speed_table.life_rnd = tab->GetItemI(TAB_I_LIFE_RANDOM);          
      speed_table.emit_base = tab->ItemI(TAB_I_EMIT_BASE);
      speed_table.emit_rnd = tab->ItemI(TAB_I_EMIT_RANDOM);

      speed_table.radius_grow = tab->ItemF(TAB_F_RADIUS_GROW);      
      speed_table.shift_speed = tab->ItemF(TAB_F_SHIFT_SPEED);
      tab->ItemF(TAB_F_PROCEDURE_SPEED) = .0f;

      ComputeSpeedLimits(tab, &speed_table.speed_min, &speed_table.speed_max);
   }


//----------------------------

   C_smart_ptr<C_table> edit_tab;
   C_smart_ptr<I3D_material> edit_mat;
   HWND hwnd_tab;
   HWND hwnd_edit;
   bool recursive;
   float env_shift;

   struct S_proc{
      C_str name, full_name;
      C_smart_ptr<C_table> tab;
      bool read_only;
      S_proc(){}
      S_proc(const S_proc &p){ operator =(p); }
      void operator =(const S_proc &p){
         name = p.name;
         full_name = p.full_name;
         tab = p.tab;
         read_only = p.read_only;
      }
   };
   typedef C_vector<S_proc> t_proc_tabs;
   t_proc_tabs proc_tabs;

                              //rendering:
   int tiling;
   bool filter;
   enum E_MODE{
      MODE_DIFFUSE,
      MODE_BUMP,
      MODE_EMBM,
      MODE_LAST
   } mode;


//----------------------------

   void UpdateProcedureSpeed(PC_table tab){

      float new_speed = tab->GetItemF(TAB_F_PROCEDURE_SPEED);
      new_speed = (new_speed < 0) ? 1.0f/(I3DFabs(new_speed) + 1.0f) : (new_speed + 1.0f);

                              //limit speed
      new_speed = Min(Max(speed_table.speed_min, new_speed), speed_table.speed_max);

                              //store back to table
      {
         float table_val = new_speed;
         if(table_val < 1.0f){
            table_val = -(1.0f/table_val) + 1.0f;
         }else{
            table_val = table_val - 1.0f;
         }

         if(table_val != tab->GetItemF(TAB_F_PROCEDURE_SPEED)){
            tab->ItemF(TAB_F_PROCEDURE_SPEED) = table_val;
         }
      }
                              //divide by scale
      {
         float lb_new = speed_table.life_base / new_speed;
         tab->ItemI(TAB_I_LIFE_BASE) = (int)lb_new;
      }

      {
         float lb_new = speed_table.life_rnd / new_speed;
         tab->ItemI(TAB_I_LIFE_RANDOM) = (int)lb_new;
      }

      {
         float lb_new = speed_table.emit_base / new_speed;
         tab->ItemI(TAB_I_EMIT_BASE) = (int)lb_new;
      }

      {
         float lb_new = speed_table.emit_rnd / new_speed;
         tab->ItemI(TAB_I_EMIT_RANDOM) = (int)lb_new;
      }
                              //multiple
      {
         float lb_new = speed_table.radius_grow * new_speed;
         tab->ItemF(TAB_F_RADIUS_GROW) = lb_new;
      }

      {
         float lb_new = speed_table.shift_speed * new_speed;
         tab->ItemF(TAB_F_SHIFT_SPEED) = lb_new;
      }
   }

//----------------------------

   void cbEditTab(PC_table tab, dword msg, dword prm2, dword prm3){
      switch(msg){
      case TCM_CLOSE:
         hwnd_tab = NULL;
         edit_tab = NULL;
         break;
      case TCM_MODIFY:
         if(prm2 == TAB_F_PROCEDURE_SPEED){
            UpdateProcedureSpeed(tab);
         }else{
            ResetSpeedTable(tab);
         }
         WriteEditTab();
         InitTexture();
         break;
      }
   }

   static void TABAPI cbEditTab_thunk(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){
      ((C_edit_ProcEdit*)cb_user)->cbEditTab(tab, msg, prm2, prm3);
   }

//----------------------------

   static void WriteString(C_cache &ck, const C_str &s){
      ck.write((const char*)s, s.Size());
      ck.write("\r\n", 2);
   }

   void WriteEditTab(){
      int i = SendDlgItemMessage(hwnd_edit, IDC_PROC_LIST, LB_GETCURSEL, 0, 0);
      const S_proc &p = proc_tabs[i];
      PC_table tab = (PC_table)(CPC_table)p.tab;
      C_cache ck;
      ck.open(p.full_name, CACHE_WRITE);
      WriteString(ck, "Class TEXTURE");
      WriteString(ck, C_fstr("texture_size_bits %i", tab->ItemI(TAB_I_SIZE_BITS)));
      WriteString(ck, C_fstr("life_len %i %i", tab->ItemI(TAB_I_LIFE_BASE), tab->ItemI(TAB_I_LIFE_BASE)+tab->ItemI(TAB_I_LIFE_RANDOM)));
      WriteString(ck, C_fstr("create_time %i %i", tab->ItemI(TAB_I_EMIT_BASE), tab->ItemI(TAB_I_EMIT_BASE)+tab->ItemI(TAB_I_EMIT_RANDOM)));
      if(tab->ItemB(TAB_B_TRUECOLOR)) WriteString(ck, "true_color");
      WriteString(ck, C_fstr("init_radius %.3f", tab->ItemF(TAB_F_INIT_RADIUS)));
      WriteString(ck, C_fstr("radius_grow %.3f", tab->ItemF(TAB_F_RADIUS_GROW)));
      WriteString(ck, C_fstr("shift_speed %.3f", tab->ItemF(TAB_F_SHIFT_SPEED)));
      WriteString(ck, C_fstr("wave_repeat %.3f", tab->ItemF(TAB_F_WAVE_REPEAT)));
      WriteString(ck, C_fstr("unit_scale %.3f", tab->ItemF(TAB_F_SCALE)));
      WriteString(ck, C_fstr("height_curve %.3f", tab->ItemF(TAB_F_HEIGHT_CURVE)));
   }

//----------------------------

   void CreateEditTab(){

      edit_tab = NULL;
      int i = SendDlgItemMessage(hwnd_edit, IDC_PROC_LIST, LB_GETCURSEL, 0, 0);
      if(i==-1)
         return;
      const S_proc &p = proc_tabs[i];
      if(p.read_only){
         MessageBox(hwnd_edit, "Cannot edit read-only file", "Procedural editor", MB_OK);
         return;
      }
      edit_tab = p.tab;

      hwnd_tab = (HWND)edit_tab->Edit(GetTempl(), hwnd_edit,
         cbEditTab_thunk, (dword)this,
         TABEDIT_MODELESS | TABEDIT_HIDDEN | TABEDIT_IMPORT | TABEDIT_EXPORT);

      RECT rc;
      GetWindowRect(hwnd_edit, &rc);
      SetWindowPos(hwnd_tab, NULL, rc.right, rc.top, 0, 0, SWP_NOSIZE);

      ShowWindow(hwnd_tab, SW_SHOW);
      ResetSpeedTable(edit_tab);
   }

   void DestroyEditTab(){
      DestroyWindow(hwnd_tab);
      hwnd_tab = NULL;
      edit_tab = NULL;
   }

//----------------------------

   void InitTexture(){

      edit_mat->SetTexture(MTI_DIFFUSE, NULL);
      edit_mat->SetTexture(MTI_ENVIRONMENT, NULL);
      edit_mat->SetTexture(MTI_EMBM, NULL);
      int i = SendDlgItemMessage(hwnd_edit, IDC_PROC_LIST, LB_GETCURSEL, 0, 0);
      if(i==-1)
         return;
      const S_proc &p = proc_tabs[i];

      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      ct.flags = TEXTMAP_PROCEDURAL;
      ct.file_name = p.name;
      I3D_MATERIAL_TEXTURE_INDEX mti = mode==MODE_DIFFUSE ? MTI_DIFFUSE : MTI_EMBM;
      ct.proc_data = mti;
      PI3D_texture tp;
      if(I3D_SUCCESS(ed->GetDriver()->CreateTexture(&ct, &tp))){
         edit_mat->SetTexture(mti, tp);
         tp->Release();

         if(mode!=MODE_DIFFUSE){
            C_cache ck;
            edit_mat->SetEMBMOpacity(.1f);
            if(mode==MODE_EMBM){
               edit_mat->SetEMBMOpacity(1.0f);
               if(OpenResource(GetHInstance(), "BINARY", "envmap.png", ck)){
                  ct.flags = TEXTMAP_DIFFUSE | TEXTMAP_USE_CACHE;
                  ct.ck_diffuse = &ck;
                  if(I3D_SUCCESS(ed->GetDriver()->CreateTexture(&ct, &tp))){
                     edit_mat->SetTexture(MTI_ENVIRONMENT, tp);
                     tp->Release();
                  }
               }
            }
            if(OpenResource(GetHInstance(), "BINARY", "sample.png", ck)){
               ct.flags = TEXTMAP_DIFFUSE | TEXTMAP_USE_CACHE;
               ct.ck_diffuse = &ck;
               if(I3D_SUCCESS(ed->GetDriver()->CreateTexture(&ct, &tp))){
                  edit_mat->SetTexture(mode==MODE_EMBM ? MTI_DIFFUSE : MTI_ENVIRONMENT, tp);
                  tp->Release();
               }
            }
         }
      }
   }

//----------------------------

   void CollectFiles(){

      proc_tabs.clear();
                              //collect all procedurals
      C_vector<C_str> names;
      PI3D_driver drv = ed->GetDriver();
      for(dword i=drv->NumDirs(I3DDIR_PROCEDURALS); i--; ){
         //const char *dir = drv->GetDir(I3DDIR_PROCEDURALS, i);
         const C_str &dir = drv->GetDir(I3DDIR_PROCEDURALS, i);
         C_buffer<C_str> n;
         OsCollectFiles(dir, "*.txt", n);
         names.insert(names.end(), n.begin(), n.end());
      }
      sort(names.begin(), names.end());

#define SKIP_WS while(*cp && isspace(*cp)) ++cp;
      for(i=0; i<names.size(); i++){
         C_cache ck;
         const C_str &name = names[i];
         if(ck.open(name, CACHE_READ)){
            while(!ck.eof()){
               char line[256];
               ck.getline(line, sizeof(line));
               const char *cp = line;
                                       //skip leading space
               SKIP_WS;
                                       //ignore comments
               if(*cp == ';' || *cp==0) continue;
               if(strncmp(cp, "Class", 5))
                  continue;
               cp += 5;
               SKIP_WS;
               char class_name[128];
               if(sscanf(cp, "%128s", class_name) == 1){
                  if(!strcmp(class_name, "TEXTURE")){

                     PC_table tab = CreateTable();
                     tab->Load(GetTempl(), TABOPEN_TEMPLATE);

                     while(!ck.eof()){
                        char line[256];
                        ck.getline(line, sizeof(line));
                        const char *cp = line;

                        SKIP_WS;
                        if(*cp == ';' || *cp==0) continue;

                        char kw[256];
                        sscanf(cp, "%256s", kw);
                        cp += strlen(kw);
                        SKIP_WS;
                        if(!stricmp(kw, "true_color")){
                           tab->ItemB(TAB_B_TRUECOLOR) = true;
                        }else
                        if(!stricmp(kw, "texture_size_bits")){
                           sscanf(cp, "%i", &tab->ItemI(TAB_I_SIZE_BITS));
                        }else
                        if(!stricmp(kw, "life_len")){
                           if(sscanf(cp, "%i %i", &tab->ItemI(TAB_I_LIFE_BASE), &tab->ItemI(TAB_I_LIFE_RANDOM)) == 2){
                              tab->ItemI(TAB_I_LIFE_RANDOM) -= tab->ItemI(TAB_I_LIFE_BASE);
                           }
                        }else
                        if(!stricmp(kw, "create_time")){
                           if(sscanf(cp, "%i %i", &tab->ItemI(TAB_I_EMIT_BASE), &tab->ItemI(TAB_I_EMIT_RANDOM)) == 2){
                              tab->ItemI(TAB_I_EMIT_RANDOM) -= tab->ItemI(TAB_I_EMIT_BASE);
                           }
                        }else
                        if(!stricmp(kw, "init_radius")){
                           sscanf(cp, "%f", &tab->ItemF(TAB_F_INIT_RADIUS));
                        }else
                        if(!stricmp(kw, "radius_grow")){
                           sscanf(cp, "%f", &tab->ItemF(TAB_F_RADIUS_GROW));
                        }else
                        if(!stricmp(kw, "shift_speed")){
                           sscanf(cp, "%f", &tab->ItemF(TAB_F_SHIFT_SPEED));
                        }else
                        if(!stricmp(kw, "wave_repeat")){
                           sscanf(cp, "%f", &tab->ItemF(TAB_F_WAVE_REPEAT));
                        }else
                        if(!stricmp(kw, "unit_scale")){
                           sscanf(cp, "%f", &tab->ItemF(TAB_F_SCALE));
                        }else
                        if(!stricmp(kw, "height_curve")){
                           sscanf(cp, "%f", &tab->ItemF(TAB_F_HEIGHT_CURVE));
                        }
                     }
                     C_str n = name;
                     for(dword i=n.Size(); i--; ){
                        if(n[i]=='\\')
                           break;
                        if(n[i]=='.')
                           n[i] = 0;
                     }
                     n = &n[i+1];
                     n.ToLower();
                     proc_tabs.push_back(S_proc());
                     S_proc &p = proc_tabs.back();
                     p.name = n;
                     p.full_name = name;
                     p.tab = tab;
                     p.read_only = OsIsFileReadOnly(name);
                     tab->Release();
                  }else{
                     break;
                  }
               }
            }
         }
      }
   }

//----------------------------

   void InitFiles(){
      CollectFiles();

      int cs = SendDlgItemMessage(hwnd_edit, IDC_PROC_LIST, LB_GETCURSEL, 0, 0);
      SendDlgItemMessage(hwnd_edit, IDC_PROC_LIST, LB_RESETCONTENT, 0, 0);
      for(dword i = 0; i<proc_tabs.size(); i++){
         const S_proc &p = proc_tabs[i];
         C_str n = p.name;
         if(p.read_only)
            n += " *";
         SendDlgItemMessage(hwnd_edit, IDC_PROC_LIST, LB_ADDSTRING, 0, (LPARAM)(const char*)n);
      }
      SendDlgItemMessage(hwnd_edit, IDC_PROC_LIST, LB_SETCURSEL, Max(0, Min(cs, (int)i-1)), 0);
      InitTexture();
   }

//----------------------------

   void RenderInternal(){

      env_shift += (float)RENDER_DELAY * .0001f;
      env_shift = (float)fmod(env_shift, 1.0f);

      PI3D_scene scene = ed->GetScene();
      PIGraph igraph = ed->GetIGraph();
      PI3D_driver drv = ed->GetDriver();

      PI3D_visual_dynamic vis = (PI3D_visual_dynamic)scene->CreateFrame(FRAME_VISUAL, I3D_VISUAL_DYNAMIC);

      drv->BeginScene();
      bool is_filter = drv->GetState(RS_LINEARFILTER);
      drv->SetState(RS_LINEARFILTER, filter);
      igraph->ClearViewport();

      BegProf();
      drv->SetTexture(edit_mat->GetTexture(mode==MODE_DIFFUSE ? MTI_DIFFUSE : MTI_EMBM));
      float time = EndProf();
      SetDlgItemText(hwnd_edit, IDC_TIME, C_fstr("%.2f ms", time));

      struct S_vertex{
         S_vectorw pos;
         I3D_text_coor uv[3];
      };
      S_vertex v[4];
      for(dword i=0; i<4; i++){
         S_vertex &vv = v[i];
         vv.pos.z = .5f;
         vv.pos.w = 1.0f;
         vv.pos.x = 0;
         vv.pos.y = 0;

         vv.uv[0].x = 0;
         vv.uv[0].y = 0;
         vv.uv[1].x = 0;
         vv.uv[1].y = 0;
         switch(mode){
         case MODE_EMBM:
            vv.uv[2].x = -env_shift;
            vv.uv[2].y = -env_shift;
            break;
         default:
            vv.uv[2].x = 0;
            vv.uv[2].y = 0;
         }
         if(i&1){
            vv.pos.x = (float)igraph->Scrn_sx();
            switch(mode){
            case MODE_DIFFUSE:
               vv.uv[0].x = (float)(tiling+1);
               vv.uv[1].x = 0;
               break;
            case MODE_EMBM:
               vv.uv[0].x = 1;
               vv.uv[1].x = (float)(tiling+1);
               break;
            case MODE_BUMP:
               vv.uv[0].x = 1;
               vv.uv[1].x = (float)(tiling+1);
               break;
            }
            vv.uv[2].x += 1;
         }
         if(i&2){
            vv.pos.y = (float)igraph->Scrn_sy();
            switch(mode){
            case MODE_DIFFUSE:
               vv.uv[0].y = (float)(tiling+1);
               vv.uv[1].y = 0;
               break;
            case MODE_EMBM:
               vv.uv[0].y = 1;
               vv.uv[1].y = (float)(tiling+1);
               break;
            case MODE_BUMP:
               vv.uv[0].y = 1;
               vv.uv[1].y = (float)(tiling+1);
               break;
            }
            vv.uv[2].y += 1;
         }
      }
      static const I3D_triface faces[2] = {
         I3D_triface(0, 1, 2),
         I3D_triface(2, 1, 3)
      };
      I3D_face_group fg;
      fg.base_index = 0;
      fg.num_faces = 2;
      fg.SetMaterial(edit_mat);
      vis->Build(v, 4, I3DVC_XYZRHW | (3<<I3DVC_TEXCOUNT_SHIFT), faces, 2, &fg, 1);
      vis->Render();

      drv->SetState(RS_LINEARFILTER, is_filter);
      drv->EndScene();
      igraph->UpdateScreen(0, GetDlgItem(hwnd_edit, IDC_PREVIEW));
      drv->SetTexture(NULL);
      vis->Release();
   }

//----------------------------

   BOOL dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            InitDlg(ed->GetIGraph(), hwnd);
            hwnd_edit = hwnd;

            InitFiles();

            for(int i=0; i<5; i++)
               SendDlgItemMessage(hwnd, IDC_TILE, CB_ADDSTRING, 0, (LPARAM)(const char*)C_fstr("%ix%i", i+1, i+1));
            SendDlgItemMessage(hwnd, IDC_TILE, CB_SETCURSEL, tiling, 0);

            static const char *mode_names[] = {"Diffuse", "Bump", "EMBM"};
            for(i=0; i<MODE_LAST; i++)
               SendDlgItemMessage(hwnd, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)mode_names[i]);
            SendDlgItemMessage(hwnd, IDC_MODE, CB_SETCURSEL, mode, 0);

            CheckDlgButton(hwnd, IDC_FILTER, filter ? BST_CHECKED : BST_UNCHECKED);

            recursive = false;
            SetTimer(hwnd, 1, RENDER_DELAY, NULL);
         }
         return 1;

      case WM_TIMER:
         if(!recursive){
            recursive = true;
            RenderInternal();
            recursive = false;
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL:
            DestroyEditTab();
            hwnd_edit = NULL;
            EndDialog(hwnd, 1);
            break;
         case IDC_EDIT:
            if(!edit_tab)
               CreateEditTab();
            break;
         case IDC_PROC_LIST:
            switch(HIWORD(wParam)){
            case LBN_SELCHANGE:
               if(!recursive){
                  recursive = true;
                  DestroyEditTab();
                  InitTexture();
                  recursive = false;
               }
               break;
            }
            break;

         case IDC_TILE:
            switch(HIWORD(wParam)){
            case LBN_SELCHANGE:
               tiling = SendDlgItemMessage(hwnd, IDC_TILE, CB_GETCURSEL, 0, 0);
            }
            break;

         case IDC_MODE:
            switch(HIWORD(wParam)){
            case LBN_SELCHANGE:
               mode = (E_MODE)SendDlgItemMessage(hwnd, IDC_MODE, CB_GETCURSEL, 0, 0);
               InitTexture();
            }
            break;

         case IDC_FILTER:
            filter = IsDlgButtonChecked(hwnd, IDC_FILTER);
            break;

         case IDC_ADD:
            {
               DestroyEditTab();
               C_str name;
               do{
                  if(!SelectName(ed->GetIGraph(), hwnd, "Enter procedural name", name))
                     break;
                  name.ToLower();
                              //check duplication
                  for(int i=proc_tabs.size(); i--; ){
                     if(proc_tabs[i].name==name){
                        MessageBox(hwnd, "This name already exists!", "Error", MB_OK);
                        break;
                     }
                  }
                  if(i!=-1)
                     continue;
                  int num = proc_tabs.size();
                  SendDlgItemMessage(hwnd, IDC_PROC_LIST, LB_ADDSTRING, 0, (LPARAM)(const char*)name);
                  proc_tabs.push_back(S_proc());
                  S_proc &p = proc_tabs.back();
                  p.name = name;
                  p.full_name = C_fstr("%s\\%s.txt", (const char*)ed->GetDriver()->GetDir(I3DDIR_PROCEDURALS, 0), (const char*)name);
                  p.read_only = false;
                  p.tab = CreateTable();
                  p.tab->Release();
                  p.tab->Load(GetTempl(), TABOPEN_TEMPLATE);
                  SendDlgItemMessage(hwnd, IDC_PROC_LIST, LB_SETCURSEL, num, 0);
                  WriteEditTab();
                  InitTexture();
               }while(false);
            }
            break;

         case IDC_REMOVE:
            {
               DestroyEditTab();
               int i = SendDlgItemMessage(hwnd, IDC_PROC_LIST, LB_GETCURSEL, 0, 0);
               const S_proc &p = proc_tabs[i];
               i = MessageBox(hwnd, C_fstr("Are you sure to remove procedural '%s'?", (const char*)p.full_name), "Remove procedural", MB_YESNO);
               if(i==IDYES){
                  OsMakeFileReadOnly(p.full_name, false);
                  OsDeleteFile(p.full_name);
                  InitFiles();
               }
            }
            break;
         }
         break;
      }
      return 0;
   }

   static BOOL CALLBACK dlgProc_thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
      switch(uMsg){
      case WM_INITDIALOG:
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         break;
      }
      C_edit_ProcEdit *ec = (C_edit_ProcEdit*)GetWindowLong(hwnd, GWL_USERDATA);
      if(ec)
         return ec->dlgProc(hwnd, uMsg, wParam, lParam);
      return 0;
   }
public:
   C_edit_ProcEdit():
      hwnd_tab(NULL),
      tiling(0),
      hwnd_edit(NULL),
      mode(MODE_DIFFUSE),
      env_shift(0)
   {}

   virtual bool Init(){
      /*
#define DS "&View\\&Video\\"
      ed->AddShortcut(this, 1000, DS"&Grab being/end\tScrollLock", K_SCROLLLOCK, 0);
      */
      ed->AddShortcut(this, ACTION_EDIT, "&Debug\\Procedural &editor", K_NOKEY, 0);
      edit_mat = ed->GetDriver()->CreateMaterial();
      edit_mat->Release();
      return true;
   }

   virtual dword Action(int id, void *context){

      switch(id){
      case ACTION_EDIT:
         {
            //ed->Message("!!!");
            DialogBoxParam(GetHInstance(), "PROCEDIT", (HWND)ed->GetIGraph()->GetHWND(),
               dlgProc_thunk, (LPARAM)this);
         }
         break;
      }
      return 0;
   }

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){
   }

   virtual bool LoadState(C_chunk &ck){
                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version!=2) return false;
                              //read other variables
      ck.Read(&tiling, sizeof(tiling));
      ck.Read(&filter, sizeof(filter));
      ck.Read(&mode, sizeof(mode));
      return true;
   }

   virtual bool SaveState(C_chunk &ck) const{
                              //write version
      byte version = 2;
      ck.Write(&version, sizeof(byte));
                              //write other variables
      ck.Write(&tiling, sizeof(tiling));
      ck.Write(&filter, sizeof(filter));
      ck.Write(&mode, sizeof(mode));
      return true;
   }
};

//----------------------------

void CreateProcEdit(PC_editor ed){
   PC_editor_item ei = new C_edit_ProcEdit;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
