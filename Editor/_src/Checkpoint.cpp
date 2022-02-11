#include "all.h"
#include "checkpoint.h"
#include "loader.h"
#include "..\..\Source\IEditor\resource.h"
#include "winstuff.h"
#include "gamemission.h"
#include <insanity\sprite.h>
                                             
//----------------------------
// 1. Checkpoint methods.
// 2. Checkpoint plugin - editor for checkpoints.
//----------------------------

                              //for groung collision testing
#define MAX_TEST_GROUND_DISTANCE 800.0f
#define NAME_FONT_SIZE 12

#define TEST_COL_RADIUS .2f

//----------------------------

C_checkpoint::~C_checkpoint(){
                              //close all connections
   while(connections.size())
      Disconnect(connections[0].con_cp);
}

//----------------------------

void C_checkpoint::Connect(PC_checkpoint cp, dword ct){

   assert(!IsConnected(cp));
   {
      connections.push_back(S_connection());
      S_connection &con = connections.back();
      con.con_cp = cp;
      con.type = ct;
   }
                              //make cross-reference
   {
      cp->connections.push_back(S_connection());
      S_connection &con = cp->connections.back();
      con.con_cp = this;
      con.type = ct;
   }
}

//----------------------------

void C_checkpoint::Disconnect(PC_checkpoint cp){

   int i;
   i = GetConnectionIndex(cp);
   assert(i!=-1);
   connections[i] = connections.back(); connections.pop_back();

   i = cp->GetConnectionIndex(this);
   assert(i!=-1);
   cp->connections[i] = cp->connections.back(); cp->connections.pop_back();
}

//----------------------------
//----------------------------
                              // *** Plugin editor ***
#ifdef EDITOR

#include <windows.h>
#include <win_res.h>
#include "systables.h"


//----------------------------
                              //editor plugin: Mission
class C_edit_checkpoint: public C_editor_item{
   virtual const char *GetName() const{ return "CheckPoint"; }

   enum E_ACTION_CHKPT{
      E_CHKPT_CREATE,
      E_CHKPT_TOGGLE_SHOW,
      E_CHKPT_MODIFY_NOTIFY,
      E_CHKPT_CONFIG,
      E_CHKPT_MOUSE_MODE,     //user click on
      E_CHKPT_CREATE_USER_PICKED,//at user pick coll point
      E_CHKPT_RECONNECT_SELECTION,//do connection on selected cp again
      E_CHKPT_FALL_SELECTION,    //fall selected cp to ground distance from config table
      E_CHKPT_CLEAR_ALL,         //clear all checkpoints
      E_CHKPT_SET_TYPE,          //set type of selected checkpoint (and all connected)
      E_CHKPT_RENAME,            //rename selected checkpoint
   };

   C_smart_ptr<C_editor_item_MouseEdit> e_mouseedit;
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;

   C_smart_ptr<I3D_material> mat_icon;

                              //string textures for connection info
   C_smart_ptr<I3D_texture> txt_con[2];
   int txt_con_size[2][4];
   
   bool show_on;              //flag if cp's SHOULD BE visible
   bool visible;              //flag if cp's ARE visible

                              //user frames - frame representations for shown checkpoints
                              // kept due to editing purposes (editor plugins work better 
                              // with frames than with checkpoints)
   struct S_cp_info{
      C_smart_ptr<I3D_user> frm;
      C_smart_ptr<I3D_texture> name_texture;
      int txt_size[2];
      dword id;
      bool visible;
      //inline operator dword() const{ return id; }
   };
   typedef map<PC_checkpoint, S_cp_info> t_cp_map;
   t_cp_map cp_info;

   C_game_mission &mission;

                              //checkpoints, which position needs to be updated by user frame
   set<PC_checkpoint> dirty_cpts;

                              //currently dragged CP - making connection (NULL = no)
   C_smart_ptr<C_checkpoint> drag_cp;

   void *name_font;
//----------------------------

   void *GetNameFont(){

      if(!name_font){
         name_font = OsCreateFont(NAME_FONT_SIZE);
      }
      return name_font;
   }

//----------------------------

   void *hwnd_edit;
   C_smart_ptr<C_table> config_tab;
   enum{
      TAB_F_CFG_SCALE,        //in meters
      TAB_I_CFG_ALPHA,        //in percent
      TAB_F_OVERLAND_DISTANCE,//in meters

      TAB_F_MAX_CONNECT_DIST, //in meters
      TAB_F_MIN_SHORTCUT_DISTANCE, //in meters
      TAB_F_MAX_DETPH,        //in meters
      TAB_I_MAX_VISIBILITY,   //in meters
      TAB_I_SHORTCUT_RATIO,   //in percent
      TAB_F_MIN_NEW_CP_DIST,  

   };

   static void TABAPI cbTabCfg(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){
      switch(msg){
      case TCM_CLOSE:
         {
            C_edit_checkpoint *ec = (C_edit_checkpoint*)cb_user;
            ec->ed->GetIGraph()->RemoveDlgHWND(ec->hwnd_edit);
            ec->hwnd_edit = NULL;
         }
         break;
      case TCM_MODIFY:
         switch(prm2){
         case TAB_F_CFG_SCALE:
            {
               C_edit_checkpoint *ec = (C_edit_checkpoint*)cb_user;
               ec->ed->GetScene()->Render(0);
               ec->ed->GetIGraph()->UpdateScreen();
            }
            break;
         }
         break;
      }
   }
   static CPC_table_template GetTemplate(){

      static const C_table_element te[] =
      {
         {TE_FLOAT, TAB_F_CFG_SCALE, "Scale", 0, 5, 1, "Scale (size) of scheckpoint, in scene world coordinates."},
         {TE_INT, TAB_I_CFG_ALPHA, "Opacity", 0, 100, 50, "Opacity in which checkpoints are rendered."},
         {TE_INT, TAB_I_MAX_VISIBILITY, "Visibility", 0, 500, 50, "Visibility distance, until which checkpoints are displayed. If your editing is slow, set smaller value here."},
         {TE_FLOAT, TAB_F_OVERLAND_DISTANCE, "Distance over land [m]", 0, 10, 1.0f, "Distance, at which checkpoints are placed above land, when created, or re-aligned."},
         {TE_FLOAT, TAB_F_MAX_CONNECT_DIST, "Max connection length", 1, 100, 10.0f, "Maximal distance, at which automatical checkpoint connection is computed (when checkpoint is created or re-connected."},
         //{TE_FLOAT, TAB_F_MIN_SHORTCUT_DISTANCE, "Min shortcut distance", 0.f, 20.0f, 2.0f},
         {TE_INT, TAB_I_SHORTCUT_RATIO, "Shortcut tolerance", 0, 100, 20, "Tolerance of angle, when connection is optimized out, when parallel connection to the checkpoint exists.\nThis value is in percent."},
         {TE_FLOAT, TAB_F_MAX_DETPH, "Max tolerable depth", 0, 50, 4.0f, "Depth to which collision are checked under connection to see if the connection is valid path."},
         {TE_FLOAT, TAB_F_MIN_NEW_CP_DIST, "Min cp dist", 0, 0, .5f, "Minimal distance at which new CP may be created from existing CPs, when creating by click."},
         {TE_NULL}
      };
      static C_table_template tt = { "Checkpoint configuration", te };
      return &tt;
   }

//----------------------------
   
   void DestroyEdit(){
      if(hwnd_edit){
         ed->GetIGraph()->RemoveDlgHWND(hwnd_edit);
         OsDestroyWindow(hwnd_edit);
         hwnd_edit = NULL;
      }
   }

//----------------------------

   PI3D_material GetIcon(){

      if(!mat_icon){
                              //load checkpoint texture
         I3D_CREATETEXTURE ct;
         memset(&ct, 0, sizeof(ct));
         ct.flags = 0;

         C_cache ck;
         if(OpenResource(NULL, "BINARY", "Checkpoint.png", ck)){
            ct.flags |= TEXTMAP_DIFFUSE | TEXTMAP_TRANSP | TEXTMAP_MIPMAP | TEXTMAP_USE_CACHE;
            ct.ck_diffuse = &ck;
         }
         PI3D_texture tp;
         driver->CreateTexture(&ct, &tp);

         PI3D_material mat = driver->CreateMaterial();
         mat->SetTexture(MTI_DIFFUSE, tp);
         tp->Release();

         mat_icon = mat;
         mat->Release();
      }
      return mat_icon;
   }

//----------------------------

   void ShowCheckpoints(bool b){

      if(visible==b)
         return;

      visible = b;
      if(visible){
                              //create user frames for checkpoints and put to scene
         for(t_cp_map::iterator it = cp_info.begin(); it!=cp_info.end(); it++){

            CPC_checkpoint cp = it->first;
            PI3D_user usr = it->second.frm;

            usr->SetName(C_xstr("<chkpt> #.4%") % (*it).second.id);
            usr->SetData2((dword)cp);
            usr->SetPos(cp->GetPos());
            usr->LinkTo(ed->GetScene()->GetPrimarySector());
            e_modify->AddFrameFlags(usr, E_MODIFY_FLG_TEMPORARY, false);

                              //set debug visibility flag (for AI tests)
            cp->help_visited = false;
         }
         e_modify->AddNotify(this, E_CHKPT_MODIFY_NOTIFY);
      }else{
         for(t_cp_map::iterator it=cp_info.begin(); it!=cp_info.end(); it++){
            e_modify->RemoveFrame(it->second.frm);
                              //remove frame from selection
            e_slct->RemoveFrame((*it).second.frm);
            //(*it).second.frm = NULL;
         }
         e_modify->RemoveNotify(this);
      }
   }

//----------------------------

   void ShowCheckpoint(CPC_checkpoint cp, bool b){

      if(b){
         t_cp_map::iterator it = cp_info.find(const_cast<PC_checkpoint>(cp));
         assert(it!=cp_info.end());
         PI3D_user usr = it->second.frm;

         usr->SetName(C_xstr("<chkpt> #.4%") % (*it).second.id);
         usr->SetData2((dword)cp);
         usr->SetPos(cp->GetPos());
         usr->LinkTo(ed->GetScene()->GetPrimarySector());
         e_modify->AddFrameFlags(usr, E_MODIFY_FLG_TEMPORARY, false);
      }else{
         t_cp_map::iterator it = cp_info.find(const_cast<PC_checkpoint>(cp));
         assert(it!=cp_info.end());
         e_modify->RemoveFrame(it->second.frm);
                              //remove frame from selection
         e_slct->RemoveFrame((*it).second.frm);
      }
   }

//----------------------------

   PC_checkpoint CreateCP(int suggested_id = -1, bool setup_pos = true, PI3D_user use_frm = NULL){

      PC_checkpoint cp = mission.CreateCheckpoint();

      if(setup_pos){
                     //put somewhere in front of camera
         PI3D_camera cam = ed->GetScene()->GetActiveCamera();
         cp->SetPos(cam->GetWorldPos() + cam->GetWorldDir() * 1.0f);
      }

      t_cp_map::const_iterator it;
                        //create unique ID
      int new_id = suggested_id;

      if(new_id!=-1){
                              //check if we may use suggested id
         for(it = cp_info.begin(); it!=cp_info.end(); it++)
            if((*it).second.id==(dword)new_id){
               new_id = -1;
               break;
            }
      }
      if(new_id==-1){
                              //generate unique id
         C_vector<int> sorted_id; sorted_id.reserve(cp_info.size());
         for(it = cp_info.begin(); it!=cp_info.end(); it++)
            sorted_id.push_back((*it).second.id);

         sort(sorted_id.begin(), sorted_id.end());
         if(sorted_id.size()){
            for(int i=sorted_id.size()-1; i--; ){
               if(sorted_id[i] != sorted_id[i+1]-1)
                  new_id = sorted_id[i] + 1;
            }
            if(new_id==-1)
               new_id = sorted_id.back() + 1;
         }else
            new_id = 0;
      }
      S_cp_info &cpi = cp_info[cp];
      cpi.id = new_id;
      if(use_frm)
         cpi.frm = use_frm;
      else{
         cpi.frm = I3DCAST_USER(ed->GetScene()->CreateFrame(FRAME_USER));
         cpi.frm->Release();
      }

                              //add undo info
      e_undo->Begin(this, UNDO_DESTROY, cpi.frm);
      e_undo->End();

      return cp;
   }

//----------------------------

   void DestroyCP(CPC_checkpoint cp){

      t_cp_map::iterator it;
      for(it=cp_info.begin(); it!=cp_info.end(); it++)
         if(it->first==cp) break;
      assert(it!=cp_info.end());

      PI3D_user frm = it->second.frm;

      if(visible) ShowCheckpoint(cp, false);
                              //save undo info
      {
         dword num_cons = cp->NumConnections();
         C_chunk &ck = e_undo->Begin(this, UNDO_CREATE, frm);
         ck.Write((dword)it->second.id);
         ck.Write(cp->GetPos());
         ck.Write((dword)cp->GetType());
         ck.Write(num_cons);
         while(num_cons--){
            const C_checkpoint::S_connection &con = cp->GetConnection(num_cons);
            PC_checkpoint cp1 = con.con_cp;
            ck.Write((dword)cp_info[cp1].id);
            ck.Write(con.type);
         }
         ck.Write(cp->GetName());
         e_undo->End();
      }

      cp_info.erase(it);
      mission.DestroyCheckpoint(cp);
   }

//----------------------------

   bool FallCheckpoint(PC_checkpoint cp){

      assert(cp);

      S_vector pos = cp->GetPos();
      S_vector g_dir = S_vector(.0f, -1.0f*MAX_TEST_GROUND_DISTANCE, .0f);
      pos -= S_normal(g_dir) * 2.0f;

      //DebugLine(pos, pos+g_dir , 0, 0xffff0000);
      I3D_collision_data cd(pos, g_dir, I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_FORCE2SIDE);
      if(TestCollision(ed->GetScene(), cd)){
         g_dir.Normalize();
         pos += g_dir*(cd.GetHitDistance() - config_tab->ItemF(TAB_F_OVERLAND_DISTANCE));
         //DebugLine(pos, pos + g_dir * config_tab->ItemF(TAB_F_OVERLAND_DISTANCE), E_DEBUGLINE_ADD_TIMED, 0xffff00ff);

                              //save undo
         {
            t_cp_map::iterator it;
            for(it=cp_info.begin(); it!=cp_info.end(); it++)
               if((*it).first==cp) break;
            assert(it!=cp_info.end());

            C_chunk &ck_undo = e_undo->Begin(this, UNDO_SET_POS, it->second.frm);
            ck_undo.Write(cp->GetPos());
            e_undo->End();
         }
                              //update position
         cp->SetPos(pos);
         t_cp_map::iterator it = cp_info.find(cp);
         assert(it!=cp_info.end());
         assert((*it).second.frm);
         (*it).second.frm->SetPos(pos);
         return true;
      }
      else
         return false;
   }

//----------------------------

   PC_checkpoint CreateCPOnPosition(const S_vector &in_pos, bool auto_connect, E_CHECKPOINT_TYPE t){

      if(!show_on)
         Action(E_CHKPT_TOGGLE_SHOW, 0);

      S_vector pos = in_pos;

      if(config_tab->ItemF(TAB_F_OVERLAND_DISTANCE) > .0f){
         pos.y += config_tab->ItemF(TAB_F_OVERLAND_DISTANCE);
         DebugLine(in_pos, pos, DL_TIMED, 0xff00ff00);
      }

                              //detect common faults - creation very close to other CP
      const C_vector<C_smart_ptr<C_checkpoint> > &cpts = mission.GetCheckpoints();
      for(dword i=cpts.size(); i--; ){
         CPC_checkpoint cp = cpts[i];
         float d = (cp->GetPos() - pos).Magnitude();
         const float MIN_DIST = config_tab->ItemF(TAB_F_MIN_NEW_CP_DIST);
         if(d < (MIN_DIST*MIN_DIST)){
            ed->Message("Failed to create checkpoint so close to other checkpoint");
            return NULL;
         }
      }

      e_slct->Clear();

      PC_checkpoint cp = CreateCP(-1, false);
      cp->SetPos(pos);
      cp->SetType(t);

      assert(visible);
      if(visible) ShowCheckpoint(cp, true);

      if(auto_connect)
         AutoConnect(cp);
                     //add newly created CP to selection
      e_slct->AddFrame(cp_info[cp].frm);

      ed->SetModified();

      return cp;
   }

//----------------------------

   void ChangeConnectionType(PC_checkpoint cp1, PC_checkpoint cp2){

      for(int i=cp1->NumConnections(); i--; ){
         if(cp1->GetConnection(i).con_cp==cp2)
            break;
      }
      if(i==-1)
         return;
      C_checkpoint::S_connection &con = cp1->GetConnection(i);

      struct S_hlp{
         dword cp_type;
         static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

            switch(uMsg){
            case WM_INITDIALOG:
               {
                  OsCenterWindow(hwnd, GetParent(hwnd));
                  SetWindowLong(hwnd, GWL_USERDATA, lParam);
                  S_hlp *hp = (S_hlp*)lParam;
                  dword t = hp->cp_type;
                  if(t&CPT_DEFAULT) CheckDlgButton(hwnd, IDC_CT_NORMAL, true);
                  if(t&CPT_DISABLED) CheckDlgButton(hwnd, IDC_CT_DISABLED, true);
               }
               return 1;

            case WM_COMMAND:
               switch(LOWORD(wParam)){
               case IDOK:
                  {
                     S_hlp *hp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);;
                     dword &t = hp->cp_type;
                     t = 0;
                     if(IsDlgButtonChecked(hwnd, IDC_CT_NORMAL)) t |= CPT_DEFAULT;
                     if(IsDlgButtonChecked(hwnd, IDC_CT_DISABLED)) t |= CPT_DISABLED;
                     EndDialog(hwnd, 1);
                  }
                  break;
               case IDCANCEL: EndDialog(hwnd, 0); break;
               }
               break;
            }
            return 0;
         }
      } hlp;
      hlp.cp_type = con.type;

      int ii = DialogBoxParam(GetModuleHandle(NULL), "IDD_CP_TYPE", (HWND)ed->GetIGraph()->GetHWND(), S_hlp::dlgProc, (LPARAM)&hlp);
      if(!ii || con.type == hlp.cp_type)
         return;

                              //save undo
      C_chunk &ck_undo = e_undo->Begin(this, UNDO_MOD_CON_TYPE, cp_info[cp1].frm);
      ck_undo.Write(cp_info[cp2].id);
      ck_undo.Write(con.type);
      e_undo->End();

      con.type = hlp.cp_type;
      for(i=cp2->NumConnections(); i--; ){
         if(cp2->GetConnection(i).con_cp==cp1){
            cp2->GetConnection(i).type = hlp.cp_type;
            break;
         }
      }
      assert(i!=-1);
      ed->Message("Changed connection type.");
      ed->SetModified();
   }

//----------------------------
// Return distance to ground at pos.
// If no collision is detected, the returned value is MAX_TEST_GROUND_DISTANCE.
   float GetDepthAt(const S_vector &pos) const{
   
      S_vector g_dir = S_vector(.0f, -1.0f, .0f);
      g_dir *= MAX_TEST_GROUND_DISTANCE;
      I3D_collision_data cd(pos, g_dir, I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_FORCE2SIDE);
      bool col = TestCollision(ed->GetScene(), cd);
      return (col ? cd.GetHitDistance() : MAX_TEST_GROUND_DISTANCE);
   }

//----------------------------

   struct S_col_test_context{
      C_vector<PC_actor> hit_actors;

   };

//----------------------------
// Collision response for checking checkpoint connection visibility.
   static bool I3DAPI cbLineResp(I3D_cresp_data &rd){

      CPI3D_frame frm = rd.GetHitFrm();
      bool accept = true;     //default is to accept collision
      if(!(frm->GetFlags()&I3D_FRMF_STATIC_COLLISION)){
                              //check if collision frame is part of actor
         do{
            PC_actor fa = GetFrameActor(frm);
            if(fa){
               if(rd.cresp_context){
                  S_col_test_context *cc = (S_col_test_context*) rd.cresp_context;
                  cc->hit_actors.push_back(fa);
               }
            }
         }while((frm->GetType() != FRAME_MODEL) && (frm = frm->GetParent(), frm));
      }
      return accept;
   }

//----------------------------

   static bool cbSort(PC_checkpoint c1, PC_checkpoint c2){
      return (c1->curr_distance_help < c2->curr_distance_help);
   }

//----------------------------
// Auto-connect given CP by requested criteria.
   void AutoConnect(PC_checkpoint cp){

      int con_add = 0, con_del = 0;

      const float max_dist = config_tab->ItemF(TAB_F_MAX_CONNECT_DIST);
      const float max_depth = config_tab->ItemF(TAB_F_MAX_DETPH);

                              //collect all cps which can be connected with cp
      C_vector<PC_checkpoint> adept_list;
      for(t_cp_map::iterator it = cp_info.begin(); it!=cp_info.end(); it++){
         PC_checkpoint cp1 = (*it).first;
                              //skip self
         if(cp == cp1 || cp->GetType()!=cp1->GetType())
            continue;

         const S_vector cc_cp = cp->GetPos() - cp1->GetPos();
                              //compare distance between cp and connection candidate
         float dist = cc_cp.Magnitude();
         if(dist > max_dist)
            continue;
         cp1->curr_distance_help = dist;
                              //test collision
         I3D_collision_data cd;
         cd.from = cp1->GetPos();
         cd.dir = cc_cp;
         cd.callback = cbLineResp;
         cd.flags = I3DCOL_MOVING_SPHERE | I3DCOL_FORCE2SIDE;
         cd.radius = TEST_COL_RADIUS;
         if(TestCollision(ed->GetScene(), cd)){
            //DebugLine(cd.from, cd.from + cd.dir, 0);
            continue;
         }
                              //test depth (3 times)
         if(max_depth > .0f){
                              //center
            if(GetDepthAt(S_vector(cp1->GetPos() + (cc_cp * .5f))) > max_depth)
               continue;
            if(GetDepthAt(S_vector(cp1->GetPos() + (cc_cp * .25f))) > max_depth)
               continue;
            if(GetDepthAt(S_vector(cp1->GetPos() + (cc_cp * .75f))) > max_depth)
               continue;
         }
         adept_list.push_back(cp1);
      }
      sort(adept_list.begin(), adept_list.end(), cbSort);

      const float MAX_GOOD_RATIO = 1.0f + (float)config_tab->ItemI(TAB_I_SHORTCUT_RATIO) * .01f;

      C_path_movement *cpm = CreatePathMovement(mission, NULL, 0xffffffff);
      bool need_prepare = true;

      for(dword ii = 0; ii<adept_list.size(); ii++){
         PC_checkpoint cc1 = adept_list[ii];
         if(cp->IsConnected(cc1))
            continue;

         if(need_prepare){
            mission.PrepareCheckpoints();
            need_prepare = false;
         }
                              //check if appropriate connection to this already exists
         float dist;
         if(cpm->FindPath(cp, cc1, dist)){
                              //check if it's in limit
            float new_dist = (cp->GetPos() - cc1->GetPos()).Magnitude();
            if(IsMrgZeroLess(new_dist))
               continue;
            float ratio = dist / new_dist;
            if(ratio < MAX_GOOD_RATIO)
               continue;
         }
         cp->Connect(cc1);
         ++con_add;
         C_chunk &ck_undo = e_undo->Begin(this, UNDO_DESTROY_CON, cp_info[cp].frm);
         ck_undo.Write((dword)cp_info[cc1].id);
         e_undo->End();

         need_prepare = true;
      }
      cpm->Release();

      cpm = CreatePathMovement(mission, NULL, 0xffffffff);

                              //now try to remove connections among adepts, if possible
      for(ii=adept_list.size(); ii--; ){
         PC_checkpoint cc0 = adept_list[ii];
         for(int i=ii; i--; ){
            PC_checkpoint cc1 = adept_list[i];
            int ci = cc0->GetConnectionIndex(cc1);
            if(ci==-1)
               continue;
            C_checkpoint::S_connection &con = cc0->GetConnection(ci);
            if(con.type&CPT_DISABLED)
               continue;
            C_checkpoint::S_connection &con1 = cc1->GetConnection(cc1->GetConnectionIndex(cc0));
                              //temporary disable the connection
            dword save_type = con.type;
            con.type = CPT_NULL;
            con1.type = CPT_NULL;

            if(need_prepare){
               mission.PrepareCheckpoints();
               need_prepare = false;
            }
            float dist;
            bool path_ok = cpm->FindPath(cc0, cc1, dist);
            con.type = save_type;
            con1.type = save_type;
            if(path_ok){
                              //check if it's in limit
               float old_dist = (cc0->GetPos() - cc1->GetPos()).Magnitude();
               if(!IsMrgZeroLess(old_dist)){
                  float ratio = dist / old_dist;
                  if(ratio < MAX_GOOD_RATIO){
                     ++con_del;
                     C_chunk &ck_undo = e_undo->Begin(this, UNDO_MAKE_CON, cp_info[cc0].frm);
                     ck_undo.Write((dword)cp_info[cc1].id);
                     ck_undo.Write(cc0->GetConnection(cc0->GetConnectionIndex(cc1)).type);
                     e_undo->End();

                     cc0->Disconnect(cc1);
                     need_prepare = true;
                  }
               }
            }
         }
      }

      cpm->Release();
                              //write message
      C_str str = C_xstr("connected: %") % con_add;
      if(con_del) 
         str += C_str(C_xstr(", disconnected: %") % con_del);
      ed->Message(str);
   }

//----------------------------

   enum E_HIT_RESULT{
      HIT_NO,
      HIT_GEOMETRY,
      HIT_CHECKPOINT,         //cpp1 is the hit CP
      HIT_LINE,               //line between cpp1 and cpp2 is hit
   };

   E_HIT_RESULT GetHit(S_vector &pos, S_vector &dir, PC_checkpoint *cpp1, PC_checkpoint *cpp2, float &closest_hit){

      *cpp1 = NULL;
      *cpp2 = NULL;
      ed->GetScene()->SetRenderMatrix(I3DGetIdentityMatrix());
                              //we're currently editing, do some automatic support (cursor change)
      int mx = igraph->Mouse_x();
      int my = igraph->Mouse_y();
      if(I3D_FAIL(ed->GetScene()->UnmapScreenPoint(mx, my, pos, dir)))
         return HIT_NO;
      *cpp1 = NULL;
      *cpp2 = NULL;

      S_MouseEdit_picked pick_data;
      memset(&pick_data, 0, sizeof(pick_data));
      pick_data.pick_from = pos;
      pick_data.pick_dir = dir;
      pick_data.pick_dist = 1e+16f;
      pick_data.pick_action = S_MouseEdit_picked::PA_DEFAULT;

      I3D_collision_data cd(pos, dir, I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_FORCE2SIDE | I3DCOL_RAY);
      if(TestCollision(ed->GetScene(), cd)){
         pick_data.frm_picked = cd.GetHitFrm();
         pick_data.pick_dist = cd.GetHitDistance();
      }
      OnPick(&pick_data);
      E_HIT_RESULT rtn = HIT_NO;
      if(pick_data.frm_picked){
         if(pick_data.frm_picked->GetType()==FRAME_USER){
                              //find CP which is being deleted
            PI3D_user usr = I3DCAST_USER(pick_data.frm_picked);
            PC_checkpoint cp = (PC_checkpoint)usr->GetData2();
            if(cp_info[cp].visible){
               *cpp1 = cp;
               closest_hit = 0.0f;
               return HIT_CHECKPOINT;
            }
         }
         closest_hit = pick_data.pick_dist;
         rtn = HIT_GEOMETRY;
      }else
         closest_hit = 1e+16f;
                              //check if some connection is being hit (might be slow)
      map<PC_checkpoint, S_cp_info>::const_iterator it;
      for(it=cp_info.begin(); it!=cp_info.end(); it++){
         PC_checkpoint cp1 = (*it).first;
         const S_cp_info &cpi = (*it).second;
         if(!cpi.visible)
            continue;
                              //check all its connections
         for(int i=cp1->NumConnections(); i--; ){
            PC_checkpoint cp2 = cp1->GetConnection(i).con_cp;
                              //don't check the same connection twice
            if((dword)cp1 > (dword)cp2)
               continue;
                              //also the other must be visible
            if(!cp_info[cp2].visible)
               continue;
            const S_vector &pos1 = cp1->GetPos();
            const S_vector &pos2 = cp2->GetPos();
            S_vector cp_dir = pos2 - pos1; 
            float u1, u2;
            if(LineIntersection(pos, dir, pos1, pos2-pos1, u1, u2)){
               if(u1>0.0f && u1<closest_hit && u2>0.0f && u2<1.0f){
                  S_vector xpt1 = pos + dir * u1;
                  S_vector xpt2 = pos1 + cp_dir * u2;
                  float a = (xpt1-pos).AngleTo(xpt2-pos);
                  if(a<PI*.005f){
                     closest_hit = u1;
                     *cpp1 = cp1;
                     *cpp2 = cp2;
                     rtn = HIT_LINE;
                  }
               }
            }
         }
      }
      return rtn;
   }

//----------------------------
// Make visibility info - set bool flag for each CP depending on if it might be visible on screen.
   void MakeVisibilityInfo(){

      const S_vector &cam_pos = ed->GetScene()->GetActiveCamera()->GetWorldPos();
      float max_dist_2 = (float)config_tab->ItemI(TAB_I_MAX_VISIBILITY); max_dist_2 *= max_dist_2;

      map<PC_checkpoint, S_cp_info>::iterator it;
      for(it=cp_info.begin(); it!=cp_info.end(); it++){
         PC_checkpoint cp = (*it).first;
         S_cp_info &cpi = (*it).second;
         const S_vector &pos = cp->GetPos();
         S_vector dir_to_cam = cam_pos - pos;
         float dist_to_cam_2 = dir_to_cam.Dot(dir_to_cam);
         cpi.visible = (dist_to_cam_2 < max_dist_2);
      }
   }

//----------------------------

   void RecursiveSetType(PC_checkpoint cp, E_CHECKPOINT_TYPE t){


      if(cp->GetType()==t)
         return;
                              //save undo
      {
         t_cp_map::iterator it;
         for(it=cp_info.begin(); it!=cp_info.end(); it++)
            if((*it).first==cp) break;
         assert(it!=cp_info.end());

         C_chunk &ck_undo = e_undo->Begin(this, UNDO_SET_TYPE, it->second.frm);
         ck_undo.Write((dword)cp->GetType());
         e_undo->End();
      }

                     //recursively change all linked
      set<PC_checkpoint> done;
      struct S_hlp{
         static void SetThis(PC_checkpoint cp, const E_CHECKPOINT_TYPE t, set<PC_checkpoint> &done){
            cp->SetType(t);
            done.insert(cp);
            for(int i=cp->NumConnections(); i--; ){
               PC_checkpoint cp1 = cp->GetConnection(i).con_cp;
               if(done.find(cp1)==done.end())
                  SetThis(cp1, t, done);
            }
         }
      };
      S_hlp::SetThis(cp, t, done);
      ed->SetModified();
   }

//----------------------------

   void RenameCheckpoint(PC_checkpoint cp, const C_str &str){

      t_cp_map::iterator it = cp_info.find(cp);

                              //save undo
      C_chunk &ck_undo = e_undo->Begin(this, UNDO_RENAME, it->second.frm);
      ck_undo.Write(cp->GetName());
      e_undo->End();

                              //rename
      cp->SetName(str);
      it->second.name_texture = NULL;
      ed->SetModified();
   }
   
//----------------------------

   enum{
      UNDO_CREATE,
      UNDO_DESTROY,
      UNDO_SET_POS,
      UNDO_RENAME,
      UNDO_SET_TYPE,
      UNDO_MAKE_CON,
      UNDO_DESTROY_CON,
      UNDO_MOD_CON_TYPE,
   };

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      if(!ed->CanModify())
         return;

      switch(id){
      case UNDO_CREATE:
         {
            PC_checkpoint cp = CreateCP(ck.ReadInt(), false, I3DCAST_USER(frm));
            cp->SetPos(ck.ReadVector());
            cp->SetType((E_CHECKPOINT_TYPE)ck.ReadDword());

                              //re-connect
            dword num_c = ck.ReadDword();
            while(num_c--){
               dword id = ck.ReadDword();
               dword type = ck.ReadDword();
               for(map<PC_checkpoint, S_cp_info>::const_iterator it=cp_info.begin(); it!=cp_info.end(); it++){
                  if(it->second.id==id){
                     cp->Connect(it->first, type);
                     break;
                  }
               }
            }
            cp->SetName(ck.ReadString());

            if(visible)
               ShowCheckpoint(cp, true);
            ed->SetModified();
         }
         break;

      case UNDO_DESTROY:
      case UNDO_SET_POS:
      case UNDO_RENAME:
      case UNDO_SET_TYPE:
      case UNDO_MAKE_CON:
      case UNDO_DESTROY_CON:
      case UNDO_MOD_CON_TYPE:
         {
            for(map<PC_checkpoint, S_cp_info>::const_iterator it=cp_info.begin(); it!=cp_info.end(); it++){
               CPI3D_frame cfrm = it->second.frm;
               if(cfrm==frm){
                  PC_checkpoint cp = it->first;

                  switch(id){
                  case UNDO_DESTROY: DestroyCP(cp); break;
                  case UNDO_SET_POS:
                     {
                        C_chunk &ck_undo = e_undo->Begin(this, id, frm);
                        ck_undo.Write(cp->GetPos());
                        e_undo->End();

                        cp->SetPos(ck.ReadVector());
                        frm->SetPos(cp->GetPos());
                     }
                     break;
                  case UNDO_RENAME:
                     RenameCheckpoint(cp, ck.ReadString());
                     break;
                  case UNDO_SET_TYPE:
                     RecursiveSetType(cp, (E_CHECKPOINT_TYPE)ck.ReadDword());
                     break;
                  case UNDO_MAKE_CON:
                  case UNDO_DESTROY_CON:
                  case UNDO_MOD_CON_TYPE:
                     {
                        dword cp_id = ck.ReadDword();
                        for(it=cp_info.begin(); it!=cp_info.end(); it++){
                           if(it->second.id==cp_id){
                              PC_checkpoint cp1 = it->first;

                              switch(id){
                              case UNDO_MAKE_CON:
                                 {
                                    C_chunk &ck_undo = e_undo->Begin(this, UNDO_DESTROY_CON, frm);
                                    ck_undo.Write(cp_id);
                                    e_undo->End();
                                    cp->Connect(it->first, ck.ReadDword());
                                 }
                                 break;
                              case UNDO_DESTROY_CON:
                                 {
                                    C_chunk &ck_undo = e_undo->Begin(this, UNDO_MAKE_CON, frm);
                                    ck_undo.Write(cp_id);
                                    ck_undo.Write(cp->GetConnection(cp->GetConnectionIndex(cp1)).type);
                                    e_undo->End();

                                    cp->Disconnect(it->first);
                                 }
                                 break;
                              case UNDO_MOD_CON_TYPE:
                                 {
                                    C_checkpoint::S_connection &con = cp->GetConnection(cp->GetConnectionIndex(cp1));

                                    C_chunk &ck_undo = e_undo->Begin(this, id, frm);
                                    ck_undo.Write(cp_id);
                                    ck_undo.Write(con.type);
                                    e_undo->End();

                                    con.type = ck.ReadDword();
                                 }
                                 break;
                              default: assert(0);
                              }
                              ed->SetModified();
                              break;
                           }
                        }
                     }
                     break;
                  default: assert(0);
                  }
                  break;
               }
            }
         }
         break;

      default: assert(0);
      }
   }

//----------------------------

   virtual void AfterLoad(){

      ShowCheckpoints(false);
      cp_info.clear();

                        //assign checkpoints' IDs
      const C_vector<C_smart_ptr<C_checkpoint> > &cps = mission.GetCheckpoints();
      for(int i=cps.size(); i--; ){
         S_cp_info &cpi = cp_info[(PC_checkpoint)(CPC_checkpoint)cps[i]];
         cpi.id = i;
         cpi.frm = I3DCAST_USER(ed->GetScene()->CreateFrame(FRAME_USER));
         cpi.frm->Release();
      }

      ShowCheckpoints(show_on);
   }

//----------------------------

   virtual void BeforeFree(){

      ShowCheckpoints(false);
      cp_info.clear();
   }

//----------------------------

   virtual bool Validate(){

      PC_editor_item_Log e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
      if(!e_log)
         return true;
      ed->Message("Checking checkpoints...", 0, EM_MESSAGE, true);

      C_vector<C_smart_ptr<C_checkpoint> > cps;
                        //get PATH checkpoints only
      {
         const C_vector<C_smart_ptr<C_checkpoint> > &cps1 = mission.GetCheckpoints();
         cps.reserve(cps1.size());
         for(int i=cps1.size(); i--; ){
            PC_checkpoint cp = (PC_checkpoint)(CPC_checkpoint)cps1[i];
            if(cp->GetType()==CP_PATH)
               cps.push_back(cp);
         }
      }
      {
                                 //check checkpoints duplication
         for(int i=cps.size(); i-- ; ){
            CPC_checkpoint cp = cps[i];
            const S_vector &pos = cp->GetPos();
            for(int j=i; j-- ; ){
               CPC_checkpoint cp1 = cps[j];
               const S_vector &pos1 = cp1->GetPos();
               if(pos==pos1){
                  ShowCheckpoints(show_on = true);
                  e_slct->AddFrame(cp_info[(PC_checkpoint)cp].frm);

                  e_log->AddText(C_fstr("Checkpoint duplication on [%.3f, %.3f, %.3f]", pos.x, pos.y, pos.z));
                  return false;
               }
            }
         }
      }

                        //check checkpoints grid consistency
      if(cps.size()){
         set<CPC_checkpoint> con_cps;
         struct S_hlp{
            static void RecFunc(set<CPC_checkpoint> &con_cps, CPC_checkpoint cp){
               con_cps.insert(cp);
               for(int i=cp->NumConnections(); i--; ){
                  PC_checkpoint cp1 = cp->GetConnection(i).con_cp;
                  set<CPC_checkpoint>::const_iterator it = con_cps.find(cp1);
                  if(it==con_cps.end())
                     RecFunc(con_cps, cp1);
               }
            }
         };
         S_hlp::RecFunc(con_cps, cps.front());

         if(con_cps.size() != cps.size()){
            ShowCheckpoints(show_on = true);
            e_log->AddText("Checkpoint grid not consistent");
                        //select all unconnected checpoints
            for(int i=cps.size(); i-- ; ){
               CPC_checkpoint cp = cps[i];
               set<CPC_checkpoint>::const_iterator it = con_cps.find(cp);
               if(it==con_cps.end())
                  e_slct->AddFrame(cp_info[(PC_checkpoint)cp].frm);
            }
            return false;
         }
      }
                        //check collisions, detect obstructions
      {
         dword num_faults = 0;
         for(dword i=cps.size(); i--; ){
            CPC_checkpoint cp = cps[i];
            for(dword ci=cp->NumConnections(); ci--; ){
               const C_checkpoint::S_connection &con = cp->GetConnection(ci);
               CPC_checkpoint con_cp = con.con_cp;
                           //don't check same connection twice
               if(cp<con_cp) continue;

                           //ignore disabled connections
               if(con.type&CPT_DISABLED)
                  continue;
               /*
               struct S_hlp{
                  static bool I3DAPI cbCol(I3D_cresp_data &rd){
                     PI3D_frame hit_frm = rd.GetHitFrm();
                     bool collided = true;
                     if(!(hit_frm->GetFlags()&I3D_FRMF_STATIC_COLLISION)){
                           //ignore dynamic collisions on actors
                        for(PI3D_frame prnt = hit_frm; prnt; prnt = prnt->GetParent()){
                           PS_frame_info fi = GetFrameInfo(prnt);
                           if(fi && fi->actor){
                              collided = false;
                              break;
                           }
                        }
                     }
                     return collided;
                  }
               };
               */
               I3D_collision_data cd;
               cd.from = cp->GetPos();
               cd.dir = con_cp->GetPos() - cd.from;
               cd.callback = cbLineResp;
               S_col_test_context cc;
               cd.cresp_context = &cc;

               cd.radius = TEST_COL_RADIUS;
               cd.flags = I3DCOL_MOVING_SPHERE | I3DCOL_FORCE2SIDE;
               //DebugLine(cd.from, cd.from + cd.dir*.5f, 0);
               bool err = false;
               bool col = TestCollision(ed->GetScene(), cd);
               if(col){
                  if(!num_faults){
                           //select given checkpoints and write error
                     e_log->AddText(C_xstr("Checkpoint connection obstructed by '%'") %cd.GetHitFrm()->GetName());
                     err = true;
                  }
                  ++num_faults;
               }
               if(err){
                  ShowCheckpoints(show_on = true);
                  e_slct->AddFrame(cp_info[(PC_checkpoint)cp].frm);
                  e_slct->AddFrame(cp_info[(PC_checkpoint)con_cp].frm);
               }
            }
         }
         if(num_faults){
            e_log->AddText(C_fstr("%i checkpoint obstructions found", num_faults));
            return false;
         }
      }
      return true;
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==1){
         const C_vector<C_smart_ptr<C_checkpoint> > &cps = mission.GetCheckpoints();
         int num_cps = cps.size();
         const C_checkpoint **cp_ptr = (const C_checkpoint**)&(*cps.begin());

                           //write (optional) number of cpts
         if(num_cps)
            ck.WIntChunk(CT_NUM_CHECKPOINTS, num_cps);

                           //build temp indicies
         map<const C_checkpoint*, int> cp_map;
         for(int i=num_cps; i--; ){
            cp_map[cp_ptr[i]] = i;
         }
         
         for(i=0; i<num_cps; i++){ //count upwards, we're assuming that in CT_CP_CON_ID chunk
            const C_checkpoint *cp = cp_ptr[i];
            ck <<= CT_CHECKPOINT;
            ck.WVectorChunk(CT_CHKPT_POS, cp->GetPos());
            if(cp->GetType())
               ck.WByteChunk(CT_CHKPT_TYPE, (byte)cp->GetType());
            const C_str &n = cp->GetName();
            if(n.Size())
               ck.WStringChunk(CT_CHKPT_NAME, n);
                           //write all connections which connect to lower index than ours
            for(int j=cp->NumConnections(); j--; ){
               const C_checkpoint::S_connection &con = cp->GetConnection(j);
               PC_checkpoint cp1 = con.con_cp;
               int cp_index = cp_map[cp1];
               if(cp_index<i){
                  ck <<= CT_CHKPT_CONNECTION;
                  {
                     ck <<= CT_CP_CON_ID;
                     ck.Write((const char*)&cp_index, sizeof(word));
                     --ck;
                     if(con.type!=CPT_DEFAULT){
                        assert(con.type<256);
                        ck.WByteChunk(CT_CP_CON_TYPE, (byte)con.type);
                     }
                  }
                  --ck;
               }
            }
            --ck;
         }
      }
   }

//----------------------------

   virtual void OnPick(S_MouseEdit_picked *mp){

      if(!visible)
         return;
      
      const S_vector &from = mp->pick_from, &dir = mp->pick_dir;
                        //ckeck if some checkpoint is close to direction line
      const C_vector<C_smart_ptr<C_checkpoint> > &cps = mission.GetCheckpoints();

      float &best_dist = mp->pick_dist;
      PC_checkpoint best_cp = NULL;
      if(!mp->frm_picked)
         best_dist = 1e+16f;
      for(int i=cps.size(); i--; ){
         PC_checkpoint cp = (PC_checkpoint)(CPC_checkpoint)cps[i];
                        //get distance of the CP from direction C_vector
         float u = dir.Dot(cp->GetPos() - from);
         if(u>=0.0f && u < best_dist){
            S_vector pol = from + dir * u;
            float dist_to_line = (pol - cp->GetPos()).Magnitude();
            if(dist_to_line < (config_tab->ItemF(TAB_F_CFG_SCALE) * .5f)){
               best_dist = u;
               best_cp = cp;
            }
         }
      }
      if(best_cp){
         switch(mp->pick_action){
         case S_MouseEdit_picked::PA_DEFAULT:
         case S_MouseEdit_picked::PA_TOGGLE:
            mp->frm_picked = cp_info[best_cp].frm;
            break;
         case S_MouseEdit_picked::PA_LINK:
            {
                        //override linking
               mp->pick_action = S_MouseEdit_picked::PA_NULL;
            }
            break;
         }
      }
   }

//----------------------------

   virtual void OnFrameDelete(PI3D_frame frm){

      if(frm && frm->GetType()==FRAME_USER && visible && !strncmp(frm->GetName(), "<chkpt>", 7)){
                        //find CP which is being deleted
         PI3D_user usr = I3DCAST_USER(frm);
         PC_checkpoint cp = (PC_checkpoint)usr->GetData2();
         assert(cp);
         DestroyCP(cp);
      }
   }

//----------------------------

   virtual void OnFrameDuplicate(PI3D_frame frm_old, PI3D_frame frm_new){

      if(frm_new->GetType()==FRAME_USER){
         PC_checkpoint old_cp = (PC_checkpoint)I3DCAST_USER(frm_new)->GetData2();
         assert(old_cp);
         assert(visible);
         PC_checkpoint cp = CreateCP(-1, false);
         cp->SetPos(old_cp->GetPos());
         cp->SetType(old_cp->GetType());
         cp->Connect(old_cp);
         //ShowCheckpoint(cp, true, I3DCAST_USER(p.second));
         ShowCheckpoint(cp, true);
      }
   }

//----------------------------
public:
   C_edit_checkpoint(C_game_mission &m1):
      mission(m1),
      hwnd_edit(NULL),
      name_font(NULL),
      visible(false),
      show_on(false)
   {
      config_tab = CreateTable();
      config_tab->Load(GetTemplate(), TABOPEN_TEMPLATE);
                              //setup defaults
      config_tab->ItemF(TAB_F_CFG_SCALE) = .7f;
      config_tab->Release();
   }

   virtual bool Init(){
      //e_property = ed->FindPlugin("Properties");
      e_mouseedit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      if(!e_modify || !e_mouseedit || !e_slct || !e_undo){
         e_modify = NULL;
         e_mouseedit = NULL;
         e_slct = NULL;
         e_undo = NULL;
         return false;
      }

#define MENU_BASE "%80 &Game\\%80 Chec&kPoint\\"
      ed->AddShortcut(this, E_CHKPT_CREATE, MENU_BASE"&Create\tShift+K", K_K, SKEY_SHIFT);
      ed->AddShortcut(this, E_CHKPT_MOUSE_MODE, MENU_BASE"&Mouse mode\tK", K_K, 0);
      ed->AddShortcut(this, E_CHKPT_RECONNECT_SELECTION, MENU_BASE"&Reconnect selection\tCtrl+K", K_K, SKEY_CTRL);
      ed->AddShortcut(this, E_CHKPT_FALL_SELECTION, MENU_BASE"&Fall selection\tCtrl+Alt+K", K_K, SKEY_CTRL|SKEY_ALT);
      ed->AddShortcut(this, E_CHKPT_CLEAR_ALL, MENU_BASE"Clear all", K_NOKEY, 0);
      ed->AddShortcut(this, E_CHKPT_TOGGLE_SHOW, MENU_BASE"%i &Show\tCtrl+Shift+K", K_K, SKEY_CTRL|SKEY_SHIFT);
      ed->AddShortcut(this, E_CHKPT_SET_TYPE, MENU_BASE"Set &type", K_NOKEY, 0);
      ed->AddShortcut(this, E_CHKPT_RENAME, MENU_BASE"&Rename\tCtrl+Shift+Alt+K", K_K, SKEY_CTRL|SKEY_SHIFT|SKEY_ALT);
      ed->AddShortcut(this, E_CHKPT_CONFIG, MENU_BASE"Confi&g", K_NOKEY, 0);
      return true;
   }

//----------------------------

   virtual void Close(){
      
      if(hwnd_edit){
         OsDestroyWindow(hwnd_edit);
         hwnd_edit = NULL;
      }
      config_tab = NULL;

      e_modify = NULL;
      e_undo = NULL;
      e_mouseedit = NULL;
      e_slct = NULL;
      mat_icon = NULL;
      if(name_font){
         OsDestroyFont(name_font);
         name_font = NULL;
      }
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(dirty_cpts.size()){
         if(visible){
            for(set<PC_checkpoint>::const_iterator it=dirty_cpts.begin(); it!=dirty_cpts.end(); it++){
               PC_checkpoint cp = *it;
               PI3D_user usr = cp_info[cp].frm;
               cp->SetPos(usr->GetWorldPos());
            }
         }
         dirty_cpts.clear();
         ed->SetModified();
      }

      if(drag_cp){
         if(!(mouse_butt&1)){
                              //dragged line released, evaluate
            S_vector pos, dir;
            float closest_hit = 0;
            PC_checkpoint cp1, cp2;
            switch(GetHit(pos, dir, &cp1, &cp2, closest_hit)){
            case HIT_CHECKPOINT:
               if(cp1!=drag_cp && drag_cp->GetType()==cp1->GetType()){
                              //toggle connection among these
                  bool connected = (cp1->GetConnectionIndex(drag_cp)!=-1);
                  if(connected){
                     C_chunk &ck_undo = e_undo->Begin(this, UNDO_MAKE_CON, cp_info[cp1].frm);
                     ck_undo.Write((dword)cp_info[drag_cp].id);
                     ck_undo.Write(cp1->GetConnection(cp1->GetConnectionIndex(drag_cp)).type);
                     e_undo->End();

                     cp1->Disconnect(drag_cp);
                     ed->Message("Disconnected.");
                  }else{
                     C_chunk &ck_undo = e_undo->Begin(this, UNDO_DESTROY_CON, cp_info[cp1].frm);
                     ck_undo.Write((dword)cp_info[drag_cp].id);
                     e_undo->End();

                     cp1->Connect(drag_cp);
                     ed->Message("Connected.");
                  }
               }
               break;
            case HIT_GEOMETRY:
               {
                  PC_checkpoint cp = CreateCPOnPosition(pos + dir * closest_hit, false, drag_cp->GetType());
                  if(cp)
                     cp->Connect(drag_cp);
               }
               break;
            }
            drag_cp = NULL;
         }
      }
   }

//----------------------------

   virtual dword Action(int id, void *context){

      dword ret = 0;          //default retunr value

      switch(id){
      case E_CHKPT_CREATE:
         {
            if(!ed->CanModify()) break;
            if(!show_on)
               Action(E_CHKPT_TOGGLE_SHOW, 0);

            e_slct->Clear();

            PC_checkpoint cp = CreateCP();

            assert(visible);
            ShowCheckpoint(cp, true);

            AutoConnect(cp);
                              //add newly created CP to selection
            e_slct->AddFrame(cp_info[cp].frm);

            ed->SetModified();
         }
         break;

      case E_CHKPT_CLEAR_ALL:
         {
            if(!ed->CanModify())
               break;
            const C_vector<C_smart_ptr<C_checkpoint> > &cpts = mission.GetCheckpoints();
            while(cpts.size()){
               CPC_checkpoint cp = cpts.front();
               DestroyCP(cp);
            }
         }
         break;

      case E_CHKPT_TOGGLE_SHOW:
          {
            show_on = !show_on;
            ed->CheckMenu(this, E_CHKPT_TOGGLE_SHOW, show_on);
            ed->Message(C_xstr("Draw checkpoints %") % (show_on ? "on" : "off"));
            ShowCheckpoints(show_on);
         }
         break;

      case E_CHKPT_MODIFY_NOTIFY:
         {
            if(!ed->CanModify()) break;
            pair<PI3D_frame, dword> *p = (pair<PI3D_frame, dword>*)context;
            if((p->second&E_MODIFY_FLG_POSITION) && p->first->GetType()==FRAME_USER &&
               !strncmp(p->first->GetName(), "<chkpt>", 7)){
               PI3D_user usr = I3DCAST_USER(p->first);
                              //move checkpoint to the same position as notified frame
                              // (postpone - do it in Tick, because notify is sent before changing the position)
               PC_checkpoint cp = (PC_checkpoint)usr->GetData2();
               dirty_cpts.insert(cp);
            }
         }
         break;

      case E_CHKPT_SET_TYPE:
      case E_CHKPT_RENAME:
         {
            PI3D_frame sel_frm = e_slct->GetSingleSel();
            if(!sel_frm){
               ed->Message("single selection required");
               break;
            }
            if(sel_frm->GetType()!=FRAME_USER){
               ed->Message("selected frame is not checkpoint");
               break;
            }
            PI3D_user usr = I3DCAST_USER(sel_frm);
            PC_checkpoint cp = (PC_checkpoint)usr->GetData2();

            switch(id){
            case E_CHKPT_SET_TYPE:
               {
                  int itm = WinSelectItem("Checkpoint type", "Path\0User\0",
                     cp->GetType());
                  if(itm!=-1)
                     RecursiveSetType(cp, (E_CHECKPOINT_TYPE)itm);
               }
               break;

            case E_CHKPT_RENAME:
               {
                              //ask user for name
                  C_str str = cp->GetName();
                  while(true){
                     if(WinGetName("Checkpoint name:", str)){
                        if(!strcmp(str, cp->GetName()))
                           break;
                        if(str.Size()){
                              //check name duplication
                           const C_vector<C_smart_ptr<C_checkpoint> > &cpts = mission.GetCheckpoints();
                           for(int i=cpts.size(); i--; ){
                              if(!strcmp(str, cpts[i]->GetName()))
                                 break;
                           }
                           if(i!=-1){
                              OsMessageBox(ed->GetIGraph()->GetHWND(), "Checkpoint with this name already exists. Please select different name.",
                                 "Rename error:", MBOX_OK);
                              continue;
                           }
                        }
                        RenameCheckpoint(cp, str);
                     }
                     break;
                  }
               }
               break;
            }
         }
         break;

      case E_CHKPT_CONFIG:
         {
            if(hwnd_edit) DestroyEdit();
            hwnd_edit = config_tab->Edit(GetTemplate(), ed->GetIGraph()->GetHWND(), 
               cbTabCfg, (dword)this,
               TABEDIT_MODELESS | TABEDIT_IMPORT | TABEDIT_EXPORT,// | TABEDIT_HIDDEN,
               NULL);
            if(hwnd_edit)
               ed->GetIGraph()->AddDlgHWND(hwnd_edit);
         }
         break;

      case E_CHKPT_MOUSE_MODE:
         e_mouseedit->SetUserPick(this, E_CHKPT_CREATE_USER_PICKED, LoadCursor(GetModuleHandle(NULL), "IDC_CURSOR_DEBUG_CP"));
         ed->Message("Checkpoints mouse mode");
         break;

      case E_CHKPT_CREATE_USER_PICKED:
         {
            if(!ed->CanModify())
               break;
            const S_MouseEdit_picked &mp = *(S_MouseEdit_picked*)context;
            {
                              //check if we delete existing checkpoint
               S_vector pos, dir;
               float closest_hit;
               PC_checkpoint cp1, cp2;
               switch(GetHit(pos, dir, &cp1, &cp2, closest_hit)){
               case HIT_CHECKPOINT:
                  {
                     drag_cp = cp1;
                     PI3D_frame cp_frm = cp_info[cp1].frm;
                     switch(mp.pick_action){
                     case S_MouseEdit_picked::PA_DEFAULT:
                        {
                           e_slct->Clear();
                           e_slct->AddFrame(cp_frm);
                        }
                        break;
                     case S_MouseEdit_picked::PA_TOGGLE:
                        {
                           const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                           for(int i=sel_list.size(); i--; ){
                              if(sel_list[i]==cp_frm)
                                 break;
                           }
                           if(i==-1)
                              e_slct->AddFrame(cp_frm);
                           else
                              e_slct->RemoveFrame(cp_frm);
                        }
                        break;
                     }
                  }
                  return 1;
               case HIT_LINE:
                  {
                     if(mp.pick_action==S_MouseEdit_picked::PA_TOGGLE){
                        ChangeConnectionType(cp1, cp2);
                     }else{
                        C_chunk &ck_undo = e_undo->Begin(this, UNDO_MAKE_CON, cp_info[cp1].frm);
                        ck_undo.Write((dword)cp_info[cp2].id);
                        ck_undo.Write(cp1->GetConnection(cp1->GetConnectionIndex(cp2)).type);
                        e_undo->End();

                        cp1->Disconnect(cp2);
                        ed->Message("Connection deleted.");
                        ed->SetModified();
                     }
                  }
                  return 1;
               }
            }
            if(mp.frm_picked){
               S_vector pos = mp.pick_from + mp.pick_dir * mp.pick_dist;
               CreateCPOnPosition(pos, true, mp.pick_action!=S_MouseEdit_picked::PA_TOGGLE ? CP_PATH : CP_USER);
               return true;
            }
         }
         break;

      case E_CHKPT_RECONNECT_SELECTION:
         {
            if(!ed->CanModify()) break;
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            for(int i=sel_list.size(); i--; ){
               PI3D_frame frm = sel_list[i];
               if(frm->GetType()!=FRAME_USER) continue;
               PC_checkpoint cp = (PC_checkpoint)I3DCAST_USER(frm)->GetData2();
               assert(cp);

                              //remove connections
               for(int j = cp->NumConnections(); j--;){
                  PC_checkpoint con_cp = cp->GetConnection(j).con_cp;
                  assert(con_cp);
                  if(!cp->IsConnected(con_cp))
                     continue;

                  C_chunk &ck_undo = e_undo->Begin(this, UNDO_MAKE_CON, cp_info[cp].frm);
                  ck_undo.Write((dword)cp_info[con_cp].id);
                  ck_undo.Write(cp->GetConnection(cp->GetConnectionIndex(con_cp)).type);
                  e_undo->End();

                  cp->Disconnect(con_cp);
               }

                              //autoconnect again
               AutoConnect(cp);
               ed->SetModified();
            }
         }
         break;

      case E_CHKPT_FALL_SELECTION:
         {
            if(!ed->CanModify()) break;
            int sucess_num = 0;
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            for(int i=sel_list.size(); i--; ){
               PI3D_frame frm = sel_list[i];
               if(frm->GetType()!=FRAME_USER) continue;
               PC_checkpoint cp = (PC_checkpoint)I3DCAST_USER(frm)->GetData2();
               assert(cp);
               if(FallCheckpoint(cp))
                  sucess_num++;
            }
            C_str s1 = C_fstr("sucess fall %i cp", sucess_num);
            int fail_num(sel_list.size()-sucess_num);
            if(!fail_num)
               ed->SetModified();
            else
               s1 += C_fstr(", failed : %i.", fail_num);
            ed->Message(s1);               
         }
         break;
      }
      return ret;
   }

//----------------------------

   virtual void Render(){

      if(!visible)
         return;
      PI3D_scene scn = ed->GetScene();

      //bool use_zb = driver->GetState(RS_USEZB);
      //if(use_zb) driver->SetState(RS_USEZB, false);

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      scn->SetRenderMatrix(I3DGetIdentityMatrix());
      const int opacity = config_tab->ItemI(TAB_I_CFG_ALPHA);
      const float icon_scale = config_tab->ItemF(TAB_F_CFG_SCALE);
      dword alpha = opacity * 255 / 100;
      dword half_alpha = opacity;
      map<PC_checkpoint, S_cp_info>::iterator it;

                              //preprocess - generate visibility info
      MakeVisibilityInfo();
      static const dword render_colors[] = { 0x00ffff00, 0x0000ff00 };

      if(visible && e_mouseedit->GetUserPick()==this && e_mouseedit->GetCurrentMode()==0){
         S_vector pos, dir;
         float closest_hit;
         PC_checkpoint cp1, cp2;
         E_HIT_RESULT hr = GetHit(pos, dir, &cp1, &cp2, closest_hit);
         if(drag_cp){
            if(hr==HIT_CHECKPOINT && drag_cp->GetType()==cp1->GetType()){
               scn->DrawSprite(cp1->GetPos(), GetIcon(), 0xffff0000, icon_scale);
               scn->DrawLine(drag_cp->GetPos(), cp1->GetPos(), 0xffffff00);
            }else{
               scn->DrawLine(drag_cp->GetPos(), pos+dir, 0xffffff00);
            }
         }else{
            switch(hr){
            case HIT_LINE:
               scn->DrawLine(cp1->GetPos(), cp2->GetPos(), 0xffff0068);
               break;
            case HIT_CHECKPOINT:
               scn->DrawSprite(cp1->GetPos(), GetIcon(), 0xc0000000 | render_colors[cp1->GetType()], icon_scale);
               break;
            }
         }
      }else{
         drag_cp = NULL;
      }

      const float max_vis_dist = (float)config_tab->ItemI(TAB_I_MAX_VISIBILITY);
      for(it=cp_info.begin(); it!=cp_info.end(); it++){
         PC_checkpoint cp = it->first;
         S_cp_info &cpi = it->second;
         float scale = icon_scale;
         if(cp->help_visited)
            scale *= .33f;
                              //render sprite on position of checkpoint
         if(cpi.visible){
            PI3D_frame frm = it->second.frm;
                              //if the user frame is selected, draw opaque, otherwise draw ...
            const C_vector<PI3D_frame>::const_iterator it = find(sel_list.begin(), sel_list.end(), frm);
            bool selected = (it!=sel_list.end());

            scn->DrawSprite(cp->GetPos(), GetIcon(), ((selected ? 0xff : alpha)<<24) | 
               (cp->GetType() ? 0x00ff00 : 0xffff00), scale);
            {
                              //draw connections
               for(int j=cp->NumConnections(); j--; ){
                  const C_checkpoint::S_connection &con = cp->GetConnection(j);
                  PC_checkpoint cp1 = con.con_cp;
                              //always draw only one line per cross-connection
                  bool draw = (cp<cp1 || !cp_info[cp1].visible);
                  if(draw){
                     dword color = alpha;
                     if(con.type&CPT_DISABLED)
                        color = half_alpha;
                     color <<= 24;
                     color |= render_colors[cp->GetType()];
                     scn->DrawLine(cp->GetPos(), cp1->GetPos(), color);
                  }
               }
            }
                              //draw name
            const C_str &name = cp->GetName();
            if(name.Size()){
               if(!cpi.name_texture){
                  cpi.name_texture = CreateStringTexture(name, cpi.txt_size, driver, GetNameFont());
                  cpi.name_texture->Release();
               }
               DrawStringTexture(frm, 0.0f, scn, cpi.name_texture, cpi.txt_size, (alpha<<24) | 0xffffff, max_vis_dist*.8f, max_vis_dist, 2.0f, scale * .5f);
            }else{
               cpi.name_texture = NULL;
            }
         }
      }
      //if(use_zb) driver->SetState(RS_USEZB, true);
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){
      
      ck.Read(&show_on, sizeof(byte));
                              //read table
      config_tab->Load(ck.GetHandle(), TABOPEN_FILEHANDLE | TABOPEN_UPDATE);

      ed->CheckMenu(this, E_CHKPT_TOGGLE_SHOW, show_on);

                              //don't call ShowCheckpoints(), it's done after scene is loaded
      return true; 
   }

   virtual bool SaveState(C_chunk &ck) const{

      ck.Write(&show_on, sizeof(byte));
                              //write table
      config_tab->Save(ck.GetHandle(), TABOPEN_FILEHANDLE);

      return true; 
   }
};

//----------------------------

void InitCheckpointPlugin(PC_editor editor, C_game_mission &gm){
   C_edit_checkpoint *em = new C_edit_checkpoint(gm); editor->InstallPlugin(em); em->Release();
}

#endif                        //EDITOR

//----------------------------
