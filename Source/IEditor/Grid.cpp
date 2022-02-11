#include "all.h"
#include "common.h"

//----------------------------

class C_edit_Grid: public C_editor_item{
   virtual const char *GetName() const{ return "Grid"; }

//----------------------------

   enum E_ACTION_GRID{
      E_GRID_TOGGLE = 1000,      // - toggle grid on/off
      E_GRID_TOGGLE_X,           // - toggle grid on x axis
      E_GRID_TOGGLE_Y,           // - toggle grid on y axis
      E_GRID_TOGGLE_Z,           // - toggle grid on z axis
      E_GRID_CONFIG,             // - configure grid 
   };

//----------------------------

   bool enabled;
   enum E_line_type{
      LINE_MAIN,              //zero-based lines
      LINE_MAJOR,             //every n-th line
      LINE_MINOR              //rest of lines
   };
   byte draw_on_axis;         //first 3 bits used to enable each axe

   PC_table config_tab;
   enum{
      TAB_F_CFG_SPACING,
      TAB_V_CFG_COLOR_0,
      TAB_V_CFG_COLOR_1,
      TAB_V_CFG_COLOR_2,
      TAB_F_CFG_ALPHA_0,
      TAB_F_CFG_ALPHA_1,
      TAB_F_CFG_ALPHA_2,
      TAB_F_CFG_CENTER_X,
      TAB_F_CFG_CENTER_Y,
      TAB_F_CFG_CENTER_Z,
   };

   static void TABAPI cbTabCfg(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){
      switch(msg){
      case TCM_CLOSE:
         {
            C_edit_Grid *ei = (C_edit_Grid*)cb_user;
            ei->ed->GetIGraph()->RemoveDlgHWND(ei->hwnd_edit);
            ei->hwnd_edit = NULL;
         }
         break;
      case TCM_MODIFY:
         switch(prm2){
         case TAB_V_CFG_COLOR_0: case TAB_V_CFG_COLOR_1: case TAB_V_CFG_COLOR_2:
            {
               C_edit_Grid *ei = (C_edit_Grid*)cb_user;
               ei->ed->GetScene()->Render(0);
               ei->ed->GetIGraph()->UpdateScreen();
            }
            break;
         }
         break;
      }
   }
   static CPC_table_template GetTemplate(){
            
      static const C_table_element te[] =
      {
         {TE_FLOAT, TAB_F_CFG_SPACING, "Spacing (m)", 0, 0, 1},
         {TE_BRANCH, 0, "Colors", 3},
            {TE_COLOR_VECTOR, TAB_V_CFG_COLOR_0, "Zero axes"},
            {TE_COLOR_VECTOR, TAB_V_CFG_COLOR_1, "Major lines"},
            {TE_COLOR_VECTOR, TAB_V_CFG_COLOR_2, "Other lines"},
         {TE_BRANCH, 0, "Alpha", 3},
            {TE_FLOAT, TAB_F_CFG_ALPHA_0, "Zero axes", 0, 1},
            {TE_FLOAT, TAB_F_CFG_ALPHA_1, "Major lines", 0, 1},
            {TE_FLOAT, TAB_F_CFG_ALPHA_2, "Other lines", 0, 1},
         {TE_BRANCH, 0, "Center", 3},
            {TE_FLOAT, TAB_F_CFG_CENTER_X, "X"},
            {TE_FLOAT, TAB_F_CFG_CENTER_Y, "Y"},
            {TE_FLOAT, TAB_F_CFG_CENTER_Z, "Z"},
         {TE_NULL}
      };
      static C_table_template templ_config = { "Grid configuration", te};
      return &templ_config;
   }

   void UpdateParams(){
      int i;
      ed->CheckMenu(this, E_GRID_TOGGLE, enabled);
      for(i=0; i<3; i++) 
         ed->CheckMenu(this, E_GRID_TOGGLE_X+i, 
         (draw_on_axis&(1<<i))!=0);
   }

   HWND hwnd_edit;

   void DestroyEdit(){
      if(hwnd_edit){
         /*
         PC_editor_item e_prop = ed->FindPlugin("Properties");
         if(e_prop) e_prop->RemoveSheet(hwnd_edit);
         else
         */
         {
            ed->GetIGraph()->RemoveDlgHWND(hwnd_edit);
            DestroyWindow(hwnd_edit);
         }
         hwnd_edit = NULL;
      }
   }
public:
   C_edit_Grid():
      enabled(true),
      config_tab(NULL),
      hwnd_edit(NULL),
      draw_on_axis(2)
   {
      config_tab = CreateTable();
      config_tab->Load(GetTemplate(), TABOPEN_TEMPLATE);
                              //setup defaults
      config_tab->ItemV(TAB_V_CFG_COLOR_0) = S_vector(1, 1, 1);
      config_tab->ItemV(TAB_V_CFG_COLOR_1) = S_vector(1, 1, 1);
      config_tab->ItemV(TAB_V_CFG_COLOR_2) = S_vector(1, 1, 1);
      config_tab->ItemF(TAB_F_CFG_ALPHA_0) = 1.0f;
      config_tab->ItemF(TAB_F_CFG_ALPHA_1) = .5f;
      config_tab->ItemF(TAB_F_CFG_ALPHA_2) = .25f;
   }
   ~C_edit_Grid(){
      if(config_tab) config_tab->Release();
      DestroyEdit();
   }

   virtual bool Init(){
      ed->AddShortcut(this, E_GRID_TOGGLE, "%30 &View\\&Grid\\%a Enable\tG", K_G, 0);
      ed->AddShortcut(this, E_GRID_TOGGLE_X, "&View\\&Grid\\X\tCtrl+Alt+X", K_X, SKEY_CTRL|SKEY_ALT);
      ed->AddShortcut(this, E_GRID_TOGGLE_Y, "&View\\&Grid\\Y\tCtrl+Alt+Y", K_Y, SKEY_CTRL|SKEY_ALT);
      ed->AddShortcut(this, E_GRID_TOGGLE_Z, "&View\\&Grid\\Z\tCtrl+Alt+Z", K_Z, SKEY_CTRL|SKEY_ALT);
      ed->AddShortcut(this, E_GRID_CONFIG, "&View\\&Grid\\%i Configure", K_NOKEY, 0);
      UpdateParams();

      return true;
   }
   virtual void Close(){
      if(config_tab){
         config_tab->Release();
         config_tab = NULL;
      }
   }

   virtual dword Action(int id, void*){

      switch(id){
      case E_GRID_TOGGLE:
         enabled = !enabled;
         UpdateParams();
         ed->Message(C_fstr("Grid %s", enabled ? "on" : "off"));
         break;

      case E_GRID_TOGGLE_X:
      case E_GRID_TOGGLE_Y:           
      case E_GRID_TOGGLE_Z:           
         {
            dword mask = (1<<(id-E_GRID_TOGGLE_X));
            ed->CheckMenu(this, id, (draw_on_axis&mask)!=0);
            draw_on_axis ^= mask;
            ed->CheckMenu(this, id, (draw_on_axis&mask)!=0);
         }
         break;

      case E_GRID_CONFIG:
         {
            if(hwnd_edit) DestroyEdit();
            hwnd_edit = (HWND)config_tab->Edit(GetTemplate(), ed->GetIGraph()->GetHWND(), 
               cbTabCfg, (dword)this,
               TABEDIT_MODELESS | TABEDIT_IMPORT | TABEDIT_EXPORT,// | TABEDIT_HIDDEN,
               NULL);
            if(hwnd_edit){
               /*
               PC_editor_item e_prop = ed->FindPlugin("Properties");
               if(e_prop) e_prop->Action(E_PROP_ADD_SHEET, hwnd_edit);
               else 
               */
                  ed->GetIGraph()->AddDlgHWND(hwnd_edit);
            }
         }
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Render(){

      if(!enabled)
         return;
      const int MAJOR_SPACING = 10;
      PI3D_scene scene = ed->GetScene();
                              //setup rendering matrix
      scene->SetRenderMatrix(I3DGetIdentityMatrix());
      PI3D_camera cam = scene->GetActiveCamera();
      if(!cam) return;
      const S_vector &cdir = cam->GetWorldDir();
      const S_vector &cpos = cam->GetWorldPos();
      float cam_near, cam_far;
      cam->GetRange(cam_near, cam_far);

      int i, j;
                              //get 4 corner points
      S_vector corner_pos[4], corner_dir[4];
      {
         const I3D_rectangle &vp = scene->GetViewport();
         for(i=0; i<4; i++){
            scene->UnmapScreenPoint(
               !(i&1) ? vp.l : vp.r-1,
               !(i&2) ? vp.t : vp.b-1,
               corner_pos[i], corner_dir[i]);
         }
      }

                              //3 lists for all line types
      C_vector<S_vector> v_list[3];
      C_vector<word> index_list[3];
      bool ortho = cam->GetOrthogonal();

      int axis;               //loop through all 3 axes
      for(axis = 0; axis<3; axis++){
         if(!(draw_on_axis&(1<<axis)))
            continue;
         float center = config_tab->ItemF(TAB_F_CFG_CENTER_X+axis);
         float f = cdir[axis];
         if(IsAbsMrgZero(f))
            continue;
         float dist_to_plane = (-cpos[axis] + center) / f;
         const float GRID_BORDER = 1000.0f;
         if(dist_to_plane<0.0f)
            dist_to_plane = GRID_BORDER;
         else
            dist_to_plane = Min(dist_to_plane, GRID_BORDER);

         float limit_coef[2][2];//min/max coeficients for both axes [axe][min|max]
         int axe[2];
         const float max_dist = Min(1000.0f, cam_far);
         const float MAX_COEF = GRID_BORDER;

         float max_corner_dist = 0.0f;
         bool corner_on_grid = false;

         for(int rest_axis=0; rest_axis<2; rest_axis++){
            axe[rest_axis] = (axis+1+rest_axis)%3;
                              //by default make it wide
            limit_coef[rest_axis][0] = MAX_COEF;
            limit_coef[rest_axis][1] = -MAX_COEF;
                              //check all 4 corners
            for(j=0; j<4; j++){
               const S_vector &pos = corner_pos[j];
               const S_vector &dir = corner_dir[j];
               float f = dir[axis];
               if(IsAbsMrgZero(f))
                  continue;
               //float dist_to_plane = (-cpos[axis] + center) / f;
               float dist_to_plane = -pos[axis] / f;
               //int curr_coef;
               float curr_coef;
               if(ortho || dist_to_plane>=0.0f){
                              //limit dist to camera range
                  S_vector pt_on_plane = pos + dir * dist_to_plane;
                  curr_coef = Min(pt_on_plane[axe[rest_axis]], cam_far);// / curr_spacing;
                              //clip coef
                  //curr_coef = Max(-MAX_COEF, Min(MAX_COEF, curr_coef));
                  curr_coef = Max(-(float)MAX_COEF, Min((float)MAX_COEF, curr_coef));
                  //max_corner_dist = Max(max_corner_dist, dist_to_plane);
                  //max_corner_dist += dist_to_plane;
                  max_corner_dist += Min(dist_to_plane, cam_far);
                  corner_on_grid = true;
               }else{
                  curr_coef = (dir[axe[rest_axis]] < 0.0f) ? -MAX_COEF : MAX_COEF;
                  //max_corner_dist = MAX_COEF;
                  //max_corner_dist += MAX_COEF;
                  max_corner_dist += Min(GRID_BORDER, cam_far);
               }
               //DEBUG(C_fstr("dist_to_plane = %.2f", dist_to_plane));
               //DEBUG(C_fstr("curr_coef = %i", curr_coef));
               limit_coef[rest_axis][0] = Min(limit_coef[rest_axis][0], curr_coef);
               limit_coef[rest_axis][1] = Max(limit_coef[rest_axis][1], curr_coef);
            }
         }
                              //grid not visible?
         if(!corner_on_grid)
            continue;
         max_corner_dist *= .125f;

         float curr_spacing = Max(.05f, config_tab->ItemF(TAB_F_CFG_SPACING));
         if(!ortho){
                              //reduce spacing if too many lines are to be visible
            if(max_corner_dist<0.0f)
               max_corner_dist = GRID_BORDER;
            else
               max_corner_dist = Min(max_corner_dist, GRID_BORDER);

            //float dist_to_plane1 = fabs(cpos[axis]);
            while(max_corner_dist>curr_spacing*40.0f)
               curr_spacing *= MAJOR_SPACING;
         }else{
            float scl = cam->GetOrthoScale();
            float num_x = 1.0f / (curr_spacing*scl);
            while(num_x > 40){
               curr_spacing *= MAJOR_SPACING;
               num_x /= MAJOR_SPACING;
            }
         }


         for(i=0; i<2; i++)
         for(j=0; j<2; j++)
            limit_coef[i][j] /= curr_spacing;

                              //create all lines on one secondary axe
         for(int axis2 = 0; axis2<2; axis2++){
            for(i=FloatToInt(limit_coef[axis2][0]); float(i) < (limit_coef[axis2][1]+1.0f); i++)
            {
               E_line_type ltype = !i ? LINE_MAIN :
                  !(i%MAJOR_SPACING) ? LINE_MAJOR :
                  LINE_MINOR;
               for(j=0; j<2; j++){    //each line has 2 ends
                  index_list[ltype].push_back((word)v_list[ltype].size());
                  v_list[ltype].push_back(S_vector());
                  S_vector &v = v_list[ltype].back();
                  //v[axis] = 0.0f;
                  v[axis] = center;
                  v[axe[axis2]] = i * curr_spacing;
                  //v[axe[1-axis2]] = !j ? -max_dist : max_dist;
                  float f = cpos[axe[1-axis2]];
                  f += !j ? -cam_far : cam_far;
                  f = Max(-max_dist, Min(max_dist, f));
                  v[axe[1-axis2]] = f;
               }
            }
         }
      }
                             //draw all line types
      for(i=0; i<3; i++){
         //DEBUG(C_fstr("Lines type %i: %i", i, index_list[i].size()/2));
         byte alpha = (byte)FloatToInt(255.0f * config_tab->ItemF(TAB_F_CFG_ALPHA_0+i));
         scene->DrawLines(&v_list[i][0], v_list[i].size(),
            &index_list[i][0], index_list[i].size(),
            ConvertColor(config_tab->ItemV(TAB_V_CFG_COLOR_0+i), alpha));
      }
      {
         float s = cpos.Magnitude();
         s *= .1f;
         if(!IsMrgZeroLess(s))
            ed->DrawAxes(I3DGetIdentityMatrix(), false, S_vector(s, s, s), s*.3f);
      }
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){
                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version!=0) return false;
                              //read table
      config_tab->Load(ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);
                              //write other variables
      byte en1 = false;
      ck.Read(&en1, sizeof(byte));
      enabled = en1;
      ck.Read(&draw_on_axis, sizeof(byte));

      UpdateParams();
      return true;
   }

   virtual bool SaveState(C_chunk &ck) const{
                              //write version
      byte version = 0;
      ck.Write(&version, sizeof(byte));
                              //write table
      config_tab->Save(ck.GetHandle(), TABOPEN_FILEHANDLE);
                              //write other variables
      ck.Write(&enabled, sizeof(byte));
      ck.Write(&draw_on_axis, sizeof(byte));
      return true;
   }
};

//----------------------------

void CreateGrid(PC_editor ed){
   PC_editor_item ei = new C_edit_Grid;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
