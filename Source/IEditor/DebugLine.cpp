#include "all.h"
#include "common.h"


//#define TUNE_PERFORMANCE 100

//----------------------------

class C_edit_DebugLine: public C_editor_item{
   virtual const char *GetName() const{ return "DebugLine"; }

//----------------------------
   enum E_ACTION{
                              //toggle debug collision testing (between frames "from" and "to")
      E_DEBUGLINE_COLTEST_STANDARD, // - standard test
      E_DEBUGLINE_COLTEST_EXACT,    // - exact test
      E_DEBUGLINE_COLTEST_SPHERE,   // - sphere test
      E_DEBUGLINE_COLTEST_SPHERE_SLIDE,
      E_DEBUGLINE_COLTEST_STATIC_SPHERE, // - static sphere test
      E_DEBUGLINE_COLTEST_STATIC_BOX,    // - static box test
      E_DEBUGLINE_COLTEST_NO,

      E_DEBUGLINE_SET_PERF_COUNT,

      E_DEBUGLINE_CLEAR,
   };

//----------------------------

   struct S_db_line{
      S_vector from, to;
      int time;
      int init_time;          //if it's -1, the line is flashing
      dword color;
      byte alpha;
      bool is_point;          //true if it's point, false if line
   };
   C_vector<S_db_line> db_lines, db_lines_s, db_lines_t;
   dword perf_count;          //0 = disable

   void Update(int time){
                              //timed lines are fading
      for(int i=db_lines_t.size(); i--; ){
         if((db_lines_t[i].time -= time) <= 0){
            db_lines_t[i] = db_lines_t.back();
            db_lines_t.pop_back();
         }
      }
   }
   C_smart_ptr<I3D_frame> col_from, col_to;
   C_smart_ptr<I3D_material> mat_icon;
   bool scene_loaded;

   enum E_COL_TYPE{
      CT_NO,
      CT_STANDARD,
      CT_EXACT,
      CT_SPHERE,
      CT_SPHERE_SLIDE,
      CT_STATIC_SPHERE,
      CT_STATIC_BOX,
   } line_col_test_type;

//----------------------------

   void CheckColTypeMenu(){
      E_COL_TYPE ct = line_col_test_type;
      ed->CheckMenu(this, E_DEBUGLINE_COLTEST_EXACT, (ct==CT_EXACT));
      ed->CheckMenu(this, E_DEBUGLINE_COLTEST_STANDARD, (ct==CT_STANDARD));
      ed->CheckMenu(this, E_DEBUGLINE_COLTEST_SPHERE, (ct==CT_SPHERE));
      ed->CheckMenu(this, E_DEBUGLINE_COLTEST_SPHERE_SLIDE, (ct==CT_SPHERE_SLIDE));
      ed->CheckMenu(this, E_DEBUGLINE_COLTEST_STATIC_SPHERE, (ct==CT_STATIC_SPHERE));
      ed->CheckMenu(this, E_DEBUGLINE_COLTEST_STATIC_BOX, (ct==CT_STATIC_BOX));
   }

//----------------------------

   PI3D_material GetIcon(){

      if(!mat_icon){
                              //load checkpoint texture
         I3D_CREATETEXTURE ct;
         memset(&ct, 0, sizeof(ct));
         ct.flags = 0;

         C_cache ck;
         if(OpenResource(GetHInstance(), "BINARY", "Sphere", ck)){
            ct.flags |= TEXTMAP_DIFFUSE | TEXTMAP_TRANSP | TEXTMAP_MIPMAP | TEXTMAP_USE_CACHE;
            ct.ck_diffuse = &ck;
         }else{
            assert(0);
         }
         PI3D_texture tp;
         ed->GetDriver()->CreateTexture(&ct, &tp);

         PI3D_material mat = ed->GetDriver()->CreateMaterial();
         mat->SetTexture(MTI_DIFFUSE, tp);
         tp->Release();

         mat_icon = mat;
         mat->Release();
      }
      return mat_icon;
   }

//----------------------------

   void AddLine(const S_vector &v0, const S_vector &v1, E_DEBUGLINE_TYPE type, dword color, dword time, bool is_pt){

      byte a = byte((color>>24)&255);
                              //don't add fully translucent line
      if(!a)
         return;

      S_db_line *l;
      switch(type){
      case DL_PERMANENT:
         db_lines.push_back(S_db_line());
         l = &db_lines.back();
         l->init_time = 0;
         break;
      case DL_FLASHING:
         db_lines.push_back(S_db_line());
         l = &db_lines.back();
         l->init_time = -1;
         break;
      case DL_ONE_TIME:
         db_lines_s.push_back(S_db_line());
         l = &db_lines_s.back();
         break;
      case DL_TIMED:
         if(!time)
            return;
         db_lines_t.push_back(S_db_line());
         l = &db_lines_t.back();
         l->init_time = l->time = time;
         break;
      default:
         return;
      }
      l->from = v0;
      l->to = v1;
      l->color = color & 0x00ffffff;
      l->alpha = a;
      l->is_point = is_pt;
   }

//----------------------------

   virtual void AddLine(const S_vector &v0, const S_vector &v1, E_DEBUGLINE_TYPE type, dword color, dword time = 500){
      AddLine(v0, v1, type, color, time, false);
   }

//----------------------------

   virtual void AddPoint(const S_vector &v, float radius, E_DEBUGLINE_TYPE type, dword color, dword time){
      AddLine(v, S_vector(radius, 0, 0), type, color, time, true);
   }

//----------------------------

   virtual void ClearAll(){

      db_lines.clear();
      db_lines_s.clear();
      db_lines_t.clear();
      ed->Message("Debug lines cleared");
   }

//----------------------------
   virtual void AfterLoad(){ scene_loaded = true; }

   virtual void BeforeFree(){

      ClearAll();
      col_from = NULL;
      col_to = NULL;
      scene_loaded = false;
   }

//----------------------------
public:
   C_edit_DebugLine():
      line_col_test_type(CT_NO),
      perf_count(0),
      scene_loaded(false)
   {}
   virtual bool Init(){
#define MB "%30 &Debug\\%50 &Collision\\"
      ed->AddShortcut(this, E_DEBUGLINE_CLEAR, "%30 &Debug\\%50 Clear lines\tCtrl+C", K_C, SKEY_CTRL);
      ed->AddShortcut(this, E_DEBUGLINE_COLTEST_STANDARD, MB"&Standard line", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUGLINE_COLTEST_EXACT, MB"&Exact line", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUGLINE_COLTEST_SPHERE, MB"Sphere &motion", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUGLINE_COLTEST_SPHERE_SLIDE, MB"Sphere motion s&lide", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUGLINE_COLTEST_STATIC_SPHERE, MB"S&tatic sphere", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUGLINE_COLTEST_STATIC_BOX, MB"Static &Box", K_NOKEY, 0);
      ed->AddShortcut(this, E_DEBUGLINE_SET_PERF_COUNT, MB"%i Set &performance count", K_NOKEY, 0);

      CheckColTypeMenu();
      return true;
   }
   virtual void Close(){ }

//----------------------------

   struct S_cb_context{
      C_edit_DebugLine *_this;

   };

   static bool I3DAPI cbTest(I3D_cresp_data &rd){

      S_cb_context &cbc = *(S_cb_context*)rd.cresp_context;
      cbc._this->ed->DEBUG(rd.GetHitFrm()->GetName());
      return true;
   }

   struct S_texel_context{
      word face_index;
      S_vector point_on_face;
      PI3D_visual vis;
   };

   static bool I3DAPI cbTexel(I3D_cresp_data &rd){
      PI3D_visual vis = I3DCAST_VISUAL(rd.GetHitFrm());
      if(vis->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
         S_texel_context *cc = (S_texel_context*)rd.cresp_context;
         cc->vis = vis;
         cc->face_index = (word)rd.GetFaceIndex();
         cc->point_on_face = rd.texel_data->point_on_plane;
      }
      return true;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      Update(time);

                              //debug collision testing
      if(scene_loaded && line_col_test_type!=CT_NO){

         PI3D_scene scn = ed->GetScene();
         PI3D_frame sphere(NULL);

         do{                  //allow break-out

            bool static_test = (line_col_test_type==CT_STATIC_SPHERE || line_col_test_type==CT_STATIC_BOX);
                              //prepare testing
            if(!col_from)
               col_from = scn->FindFrame("from");
            if(!col_to)
               col_to = scn->FindFrame("to");
            if(!col_from){
                              //disable testing
               ed->Message("ColTest: 'from' not found, disabling test");
               Action(E_DEBUGLINE_COLTEST_NO, 0);
               break;
            }
            if(!col_to){
                                 //only static tests do not need 'to' frame
               if(!static_test){
                              //disable testing
                  ed->Message("ColTest: 'to' not found, disabling test");
                  Action(E_DEBUGLINE_COLTEST_NO, 0);
                  break;
               }
            }

            I3D_collision_data cd;
            cd.from = col_from->GetWorldPos();
            S_cb_context cbc = {this};
            S_texel_context c_texel;

            if(col_to)
               cd.dir = col_to->GetWorldPos() - cd.from;

            bool ok = true;
            switch(line_col_test_type){
            case CT_SPHERE:
            case CT_SPHERE_SLIDE:
               sphere = scn->FindFrame("debugsphere", ENUMF_VOLUME);
               if(!sphere){
                  ed->DEBUG("volume 'debugsphere' not found");
                  ok = false;
                  break;
               }
                  cd.flags |= I3DCOL_MOVING_SPHERE | I3DCOL_FORCE2SIDE;
               if(line_col_test_type==CT_SPHERE_SLIDE)
                  cd.flags |= I3DCOL_SOLVE_SLIDE;
               cd.frm_ignore = sphere;
               cd.radius = sphere->GetWorldScale();
               break;

            case CT_STATIC_SPHERE:
               if(col_from->GetType()!=FRAME_VOLUME){
                  ed->DEBUG("'from' frame must be a volume for static sphere test");
                  ok = false;
                  break;
               }
               cd.flags |= I3DCOL_STATIC_SPHERE;
               cd.radius = col_from->GetWorldScale();
               cd.frm_ignore = col_from;
               if(!perf_count){
                  cd.callback = cbTest;
                  cd.cresp_context = &cbc;
                  ed->DEBUG("Hit frames:");
               }
               break;

            case CT_STATIC_BOX:
               if(col_from->GetType()!=FRAME_VOLUME){
                  ed->DEBUG("'from' frame must be a volume for static box test");
                  ok = false;
                  break;
               }
               cd.flags |= I3DCOL_STATIC_BOX;
               cd.frm_root = col_from;
               cd.frm_ignore = col_from;
               break;

            case CT_STANDARD:
            case CT_EXACT:
               if(line_col_test_type==CT_EXACT)
                  cd.flags |= I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY;
               else
                  cd.flags = I3DCOL_LINE | I3DCOL_JOINT_VOLUMES | I3DCOL_COLORKEY;
               if(!perf_count){
                              //test texel callback
                  cd.texel_callback = cbTexel;
                  cd.cresp_context = &c_texel;
                  memset(&c_texel, 0, sizeof(c_texel));
               }
               break;
            }
            if(!ok)
               break;

            if(!perf_count){
               bool col = scn->TestCollision(cd);
                              //anounce results
               switch(line_col_test_type){
               case CT_STATIC_BOX:
                              //todo: announce static-box results other way
                  ed->DEBUG(col ? (const char*)(C_xstr("volume box collide with '%'") %cd.GetHitFrm()->GetName()) : "<no collision>");
                  break;
               }
            }else{
               BegProf();
               for(dword i=perf_count; i--; )
                  scn->TestCollision(cd);
               ed->DEBUG(C_xstr("DebugLine test time: #.2%") %EndProf());
            }
                              //visualize result
            if(!static_test){
               if(!cd.GetHitFrm()){
                              //single line
                  ed->DEBUG("<no collision>");
                  //Action(E_DEBUGLINE_ADD_SINGLE, &S_add_line(cd.from, cd.from+cd.dir, 0x80ff8080));
                  AddLine(cd.from, cd.from+cd.dir, DL_ONE_TIME, 0x80ff8080);
               }else{
                  S_vector col_pt = cd.ComputeHitPos();
                              //paint line in 2 parts
                  AddLine(cd.from, col_pt, DL_ONE_TIME, 0xffff0000);
                  AddLine(col_pt, cd.from+cd.dir, DL_ONE_TIME, 0x80ff8080);
                              //paint also normal
                  AddLine(col_pt, col_pt+cd.GetHitNormal()*.25f, DL_ONE_TIME, 0xffff00ff);
                  ed->DEBUG(C_fstr("Hit: %s, face %i, dist: %.2f", (const char*)cd.GetHitFrm()->GetName(),
                     cd.GetFaceIndex(), cd.GetHitDistance()));

                  //Action(E_DEBUGLINE_ADD_SINGLE, &S_add_line(cd.GetDestination(), cd.ComputeHitPos(), 0xffffff00));
               }
               if(line_col_test_type==CT_SPHERE || line_col_test_type==CT_SPHERE_SLIDE){
                           //set debugsphere to collided pos
                  S_vector pos = cd.GetDestination();                        
                  PI3D_frame frm_parent = sphere->GetParent();
                  if(frm_parent)
                     pos *= frm_parent->GetInvMatrix();
                  sphere->SetPos(pos);
               }
            }
         }while(false);
      }
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){
         /*
      case E_DEBUGLINE_ADD:
      case E_DEBUGLINE_ADD_SINGLE:
      case E_DEBUGLINE_ADD_TIMED:
      case E_DEBUGLINE_ADD_FLASHING:

      case E_DEBUGLINE_ADD_PT:
      case E_DEBUGLINE_ADD_PT_SINGLE:
      case E_DEBUGLINE_ADD_PT_TIMED:
      case E_DEBUGLINE_ADD_PT_FLASHING:
         {
            S_add_line &al = *(S_add_line*)context;
            byte a = byte((al.color>>24)&255);
            if(!a) break;     //don't add fully translucent line

            S_db_line *l;
            switch(id){
            case E_DEBUGLINE_ADD:
            case E_DEBUGLINE_ADD_PT:
               db_lines.push_back(S_db_line());
               l = &db_lines.back();
               l->init_time = 0;
               break;
            case E_DEBUGLINE_ADD_FLASHING:
            case E_DEBUGLINE_ADD_PT_FLASHING:
               db_lines.push_back(S_db_line());
               l = &db_lines.back();
               l->init_time = -1;
               break;
            case E_DEBUGLINE_ADD_SINGLE:
            case E_DEBUGLINE_ADD_PT_SINGLE:
               db_lines_s.push_back(S_db_line());
               l = &db_lines_s.back();
               break;
            case E_DEBUGLINE_ADD_TIMED:
            case E_DEBUGLINE_ADD_PT_TIMED:
               db_lines_t.push_back(S_db_line());
               l = &db_lines_t.back();
               if(!al.time) return 0;
               l->init_time = l->time = al.time;
               break;
            default: return 0;
            }
            l->from = al.v0;
            l->to = al.v1;
            l->color = al.color & 0x00ffffff;
            l->alpha = a;
            l->is_point = (id>=E_DEBUGLINE_ADD_PT && id<E_DEBUGLINE_ADD_PT_FLASHING);
         }
         break;
         */

      case E_DEBUGLINE_COLTEST_NO:
         line_col_test_type = CT_NO;
         CheckColTypeMenu();
         break;

      case E_DEBUGLINE_COLTEST_EXACT:
         line_col_test_type = (line_col_test_type==CT_EXACT) ? CT_NO : CT_EXACT;
         CheckColTypeMenu();
         break;

      case E_DEBUGLINE_COLTEST_STANDARD:
         line_col_test_type = (line_col_test_type==CT_STANDARD) ? CT_NO : CT_STANDARD;
         CheckColTypeMenu();
         break;

      case E_DEBUGLINE_COLTEST_SPHERE:
         line_col_test_type = (line_col_test_type==CT_SPHERE) ? CT_NO : CT_SPHERE;
         CheckColTypeMenu();
         break;

      case E_DEBUGLINE_COLTEST_SPHERE_SLIDE:
         line_col_test_type = (line_col_test_type==CT_SPHERE_SLIDE) ? CT_NO : CT_SPHERE_SLIDE;
         CheckColTypeMenu();
         break;

      case E_DEBUGLINE_COLTEST_STATIC_SPHERE:
         line_col_test_type = (line_col_test_type==CT_STATIC_SPHERE) ? CT_NO : CT_STATIC_SPHERE;
         CheckColTypeMenu();
         break;
      case E_DEBUGLINE_COLTEST_STATIC_BOX:
         line_col_test_type = (line_col_test_type==CT_STATIC_BOX) ? CT_NO : CT_STATIC_BOX;
         CheckColTypeMenu();
         break;

      case E_DEBUGLINE_SET_PERF_COUNT:
         {
            C_str name(C_xstr("%") %(int)perf_count);
            while(true){
               if(!SelectName(ed->GetIGraph(), NULL, "Debugline performance count", name,
                  "Set number of times how many debug tests will be done."))
                  break;
               if(!name.Size()){
                  perf_count = 0;
                  break;
               }
               if(sscanf(name, "%i", &perf_count) == 1)
                  break;
            }
         }
         break;

      case E_DEBUGLINE_CLEAR:   
         ClearAll();
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Render(){

      dword i;
      PI3D_scene scene = ed->GetScene();
      scene->SetRenderMatrix(I3DGetIdentityMatrix());

                              //draw all flashing ones
      bool flash_on = (ed->GetIGraph()->ReadTimer() & 0x100);
      for(i = db_lines.size(); i--; ){
         const S_db_line &l = db_lines[i];
         if(l.init_time==-1){
            if(!flash_on)
               continue;
         }
         if(!l.is_point)
            scene->DrawLine(l.from, l.to, (l.alpha<<24) | l.color);
         else
            scene->DrawSprite(l.from, GetIcon(), (l.alpha<<24) | l.color, l.to.x);
      }

                              //draw all single
      for(i = db_lines_s.size(); i--; ){
         const S_db_line &l = db_lines_s[i];
         if(!l.is_point)
            scene->DrawLine(l.from, l.to, (l.alpha<<24) | l.color);
         else
            scene->DrawSprite(l.from, GetIcon(), (l.alpha<<24) | l.color, l.to.x);
      }
      db_lines_s.clear();

                              //draw all timed
      for(i = db_lines_t.size(); i--; ){
         const S_db_line &lt = db_lines_t[i];
         dword a = lt.alpha;
         a = FloatToInt(a * ((float)lt.time/(float)lt.init_time)) << 24;
         a |= lt.color;
         if(!lt.is_point){
            scene->DrawLine(lt.from, lt.to, a);
         }else{
            scene->DrawSprite(lt.from, GetIcon(), a, lt.to.x);
         }
      }
   }

//----------------------------

   enum{
      CT_COL_TEST,
      CT_PERF_CNT,
   };

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      ck
         (CT_COL_TEST, line_col_test_type)
         (CT_PERF_CNT, perf_count)
         ;

      return true; 
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){
      
      if(ck.Size()==4) return false;   //old version?
      while(ck)
      switch(++ck){
      case CT_COL_TEST: ck >> line_col_test_type; break;
      case CT_PERF_CNT: ck >> perf_count; break;
      default: --ck;
      }
      CheckColTypeMenu();

      return true; 
   }

//----------------------------
};

//----------------------------

void CreateDebugLine(PC_editor ed){
   PC_editor_item ei = new C_edit_DebugLine;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
