#include "all.h"
#include <win_res.h>
#include <Insanity\PhysTemplate.h>
#include <windows.h>
#include "winstuff.h"
#include "..\..\Source\IEditor\resource.h"
#include "common.h"

//----------------------------

                              //we use -0 to mark 'default float value for some params
//const float DEFAULT_VALUE = -0.0f;
#define DEFAULT_VALUE I3DIntAsFloat(0x80000000)

inline bool IsDefaultValue(float f){
   return (I3DFloatAsInt(f)==0x80000000);
}

//----------------------------
//----------------------------
                              //conversion of joint value id into sheet control ID
static const dword val_ids[] = {
   IDC_LO,
   IDC_LO1,
   IDC_LO2,
   IDC_HI,
   IDC_HI1,
   IDC_HI2,
   IDC_MAXF,
   IDC_FUDGE,
   IDC_BOUNCE,
   IDC_CFM,
   IDC_S_ERP,
   IDC_S_CFM,
   IDC_SU_ERP,
   IDC_SU_CFM,
};

//----------------------------

class C_physics_studio: public C_editor_item{
   const char *GetName()const{ return "Physics Studio"; }

   bool col_test;

//----------------------------
public:
   void ErrReport(const char *cp) const{

                                 //context is pointer to C_editor class
      PC_editor_item_Log ei_log = (PC_editor_item_Log)ed->FindPlugin("Log");
      if(ei_log)
         ei_log->AddText(cp, 0xffff0000);
   }
private:
//----------------------------

   void DebugLine(const S_vector &from, const S_vector &to, int type, dword color = 0xffffffff, int time = 500){
      PC_editor_item_DebugLine ei = (PC_editor_item_DebugLine)ed->FindPlugin("DebugLine");
      if(ei)
         ei->AddLine(from, to, (E_DEBUGLINE_TYPE)type, color, time);
   }

//----------------------------

   void DebugPoint(const S_vector &from, float radius, int type, dword color = 0xffffffff, int time = 500){
      PC_editor_item_DebugLine ei = (PC_editor_item_DebugLine)ed->FindPlugin("DebugLine");
      if(ei)
         ei->AddPoint(from, radius, (E_DEBUGLINE_TYPE)type, color, time);
   }

//----------------------------

   void InitModel(PIPH_body body, PI3D_scene scn, const char *fn, float scale) const{

      PI3D_model model = I3DCAST_MODEL(scn->CreateFrame(FRAME_MODEL));
      model->LinkTo(scn->GetPrimarySector());
      model->SetScale(scale);
      model->SetFlags(I3D_FRMF_SHADOW_CAST, I3D_FRMF_SHADOW_CAST);
      ed->GetModelCache().Open(model, fn, scn, 0, NULL, NULL);

      ((PC_editor_item_Modify)ed->FindPlugin("Modify"))->AddFrameFlags(model, E_MODIFY_FLG_TEMPORARY, false);

      body->SetFrame(model, IPH_STEFRAME_HR_VOLUMES);
      model->Release();
   }

//----------------------------

   void CloseModel(PIPH_body body) const{

      if(body){
         PI3D_frame frm = body->GetFrame();
         if(ed)
            ((PC_editor_item_Modify)ed->FindPlugin("Modify"))->RemoveFrame(frm);
      }
   }

//----------------------------

   C_vector<C_smart_ptr<IPH_body> > bodies;
   C_vector<C_smart_ptr<IPH_joint> > joints;

   C_smart_ptr<IPH_world> world;
   //C_game_mission &mission;

   bool active;
   bool draw_contacts;
   bool statistics;
   bool phys_studio_active;
   HWND hwnd_sheet;

   C_smart_ptr<IPH_body> pick_body;
   S_vector pick_pos;         //in picked model's local coords
   float pick_dist;           //distance in which we picked
   C_str auto_command;

   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_MouseEdit> e_mouseedit;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Properties> e_props;

   struct S_contact{
      S_vector pos, normal;
      float depth;
      dword color;
   };

//----------------------------

   enum E_ACTION{
      E_PHYS_NULL,
      /*
      E_PHYS_JOINT_BALL,
      E_PHYS_JOINT_HINGE,
      E_PHYS_JOINT_HINGE2,
      E_PHYS_JOINT_SLIDER,
      E_PHYS_JOINT_UNIVERSAL,
      E_PHYS_JOINT_FIXED,
      E_PHYS_JOINT_AMOTOR,
      */

      E_PHYS_ACTIVE,
      E_PHYS_DRAW_CONTACTS,
      E_PHYS_STATS,
      E_ACTION_PICK_MODE,
      E_ACTION_DO_PICK,
      E_ACTION_EDIT_AUTO_CMD,
      E_ACTION_PHYS_STUDIO_TOGGLE,
      E_ACTION_PHYS_STUDIO_TEST,

      E_SEL_NOTIFY,
      //E_MODIFY_NOTIFY,

      E_ACTION_GET_CONTACT_REPORT = 1000,
   };

//----------------------------

   static C_str FloatStrip(const C_str &str){

      for(dword i=str.Size(); i--; )
         if(str[i]=='.')
            break;
      if(i==-1)
         return str;
      C_str ret = str;
      for(i=ret.Size(); i--; ) 
         if(ret[i]!='0') break;
      if(ret[i]=='.') ++i;
      ret[i+1] = 0;
      return ret;
}

//----------------------------

   virtual bool Init(){

      e_mouseedit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      if(!e_modify || !e_mouseedit || !e_slct || !e_undo || !e_props)
         return false;

#define MENU_BASE "%90 &Physics Studio\\"
      /*
      ed->AddShortcut(this, E_PHYS_JOINT_BALL, MENU_BASE"Joint B&all", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_JOINT_HINGE, MENU_BASE"Joint &Hinge", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_JOINT_HINGE2, MENU_BASE"Joint Hinge &2", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_JOINT_SLIDER, MENU_BASE"Joint S&lider", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_JOINT_UNIVERSAL, MENU_BASE"Joint &Universal", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_JOINT_FIXED, MENU_BASE"%aJoint &Fixed", K_NOKEY, 0);
      //ed->AddShortcut(this, E_PHYS_JOINT_AMOTOR, MENU_BASE"Joint &Motor", K_NOKEY, 0);
      */
      ed->AddShortcut(this, E_ACTION_PHYS_STUDIO_TOGGLE, MENU_BASE"&Physics Studio", K_NOKEY, 0);
      ed->AddShortcut(this, E_ACTION_PHYS_STUDIO_TEST, MENU_BASE"%a &Test physics", K_NOKEY, 0);
      ed->AddShortcut(this, E_ACTION_PICK_MODE, MENU_BASE"P&ick mode", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_ACTIVE, MENU_BASE"&Debug", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_DRAW_CONTACTS, MENU_BASE"D&raw contacts", K_NOKEY, 0);
      ed->AddShortcut(this, E_PHYS_STATS, MENU_BASE"&Stats", K_NOKEY, 0);
      ed->AddShortcut(this, E_ACTION_EDIT_AUTO_CMD, MENU_BASE"&Edit start-up command", K_NOKEY, 0);

      return true;
   }

//----------------------------

   /*
   void InstallShortcuts(){

      static const S_tmp_shortcut shortcuts[] = {
         {E_PHYS_ADD_BOX, K_B, 0, "Drop Ball"},
         {E_PHYS_ADD_BLOCK, K_K, 0, "Drop Block"},
         {E_PHYS_ADD_SPHERE, K_P, 0, "Drop Sphere"},
         {E_PHYS_ADD_HYBRID, K_H, 0, "Drop Hybrid"},
         //{E_PHYS_ADD_CYLINDER, K_C, 0, "Drop Cylinder"},
         {E_PHYS_ADD_CCYLINDER, K_A, 0, "Drop Capped cylinder"},

         {E_PHYS_JOINT_BALL, K_J, SKEY_SHIFT, "Drop Ball joint"},
         {E_PHYS_JOINT_HINGE, K_H, SKEY_SHIFT, "Drop Hinge joint"},
         {E_PHYS_JOINT_HINGE2, K_2, SKEY_SHIFT, "Drop Hinge2 joint"},
         {E_PHYS_JOINT_SLIDER, K_S, SKEY_SHIFT, "Drop Slider joint"},
         {E_PHYS_JOINT_UNIVERSAL, K_U, SKEY_SHIFT, "Drop Universal joint"},
         {E_PHYS_JOINT_FIXED, K_F, SKEY_SHIFT, "Drop Fixed joint"},
         {NULL}
      };
      ed->InstallTempShortcuts(this, shortcuts);
   }
   */

//----------------------------

   void InitPhysicsScene(){

      ClosePhysicsScene();

      //world = mission.GetPhysicsWorld();
      if(!world){
         world = IPHCreateWorld(); world->Release();
      }
      {
         if(auto_command.Size()){
            if(auto_command=="c")
               col_test = true;
            else
               Command(auto_command);
         }
         if(!col_test)
            Action(E_ACTION_PICK_MODE, 0);
      }
      //InstallShortcuts();
   }

//----------------------------

   void ClosePhysicsScene(){

      for(dword i=bodies.size(); i--; )
         CloseModel(bodies[i]);
      bodies.clear();
      joints.clear();
      //world = NULL;
      //ed->RemoveTempShortcuts(this);
      col_test = false;
   }

//----------------------------
//----------------------------
                              //physics studio database (run-time version):
   struct S_phys_info{
      struct S_body{
         C_smart_ptr<I3D_frame> frm;
         bool is_static;      //used for debugging - this body doesn't move
         float density;
         bool density_as_weight;

         S_body():
            is_static(false),
            density(I3DIntAsFloat(0x80000000)),
            density_as_weight(false)
         {}
      };
      C_vector<S_body> bodies;

      struct S_joint{
         C_smart_ptr<I3D_frame> frm;
         IPH_JOINT_TYPE type;
         float val[S_phys_template::VAL_LAST];

         S_joint():
            type(IPHJOINTTYPE_BALL)
         {
            for(int i=S_phys_template::VAL_LAST; i--; )
               val[i] = DEFAULT_VALUE;
         }
      };
      C_vector<S_joint> joints;

      int BodyIndex(CPI3D_frame frm) const{
         for(int i=bodies.size(); i--; ){
            if(bodies[i].frm==frm)
               break;
         }
         return i;
      }
      void RemoveBody(dword i){
         assert(i<bodies.size());
         bodies[i] = bodies.back();
         bodies.pop_back();
      }

   //----------------------------

      int JointIndex(CPI3D_frame frm) const{
         for(int i=joints.size(); i--; ){
            if(joints[i].frm==frm)
               break;
         }
         return i;
      }
      void RemoveJoint(dword i){
         assert(i<joints.size());
         joints[i] = joints.back();
         joints.pop_back();
      }

      bool IsEmpty() const{ return (!bodies.size() && !joints.size()); }

      void Clear(){
         bodies.clear();
         joints.clear();
      }

   //----------------------------

      void Export(S_phys_template &pt, PI3D_scene scn) const{

         CPI3D_frame prim_sect = scn->GetPrimarySector();
         int i;
         for(i=bodies.size(); i--; ){
            const S_body &b = bodies[i];
            pt.bodies.push_back(S_phys_template::S_body());
            S_phys_template::S_body &tb = pt.bodies.back();
            tb.name = (b.frm==prim_sect) ? "*" : b.frm->GetName();
            tb.is_static = b.is_static;
            tb.density = b.density;
            tb.density_as_weight = b.density_as_weight;
         }
         for(i=joints.size(); i--; ){
            const S_joint &j = joints[i];
            pt.joints.push_back(S_phys_template::S_joint());
            S_phys_template::S_joint &tj = pt.joints.back();
            tj.name = j.frm->GetName();
            tj.type = j.type;
            tj.pos.Zero();
            memcpy(tj.val, j.val, sizeof(tj.val));
                              //joint's parent is 'body1'
            PI3D_frame prnt = j.frm->GetParent();
            if(prnt){
               if(BodyIndex(prnt)!=-1){
                  tj.body[0] = (prnt==prim_sect) ? "*" : prnt->GetName();
                              //joint's position is relative to body1
                  tj.pos = j.frm->GetPos();
               }
            }
                              //joint's body2 is either joint's frame itself, or its 1st child
            if(BodyIndex(j.frm)!=-1)
               tj.body[1] = tj.name;
            else{
               PI3D_frame chld = j.frm->NumChildren() ? j.frm->GetChildren()[0] : NULL;
               if(chld){
                  if(BodyIndex(chld)!=-1)
                     tj.body[1] = chld->GetName();
               }
            }
         }
      }

   //----------------------------

      void Save(C_chunk &ck, PI3D_scene scn) const{

         S_phys_template pt;
         Export(pt, scn);
         pt.Save(ck);
      }

   //----------------------------

      void Import(S_phys_template &pt, PI3D_scene scn, C_physics_studio *ps){

         Clear();
         bodies.reserve(pt.bodies.size());
         joints.reserve(pt.joints.size());

         PI3D_frame prim_sect = scn->GetPrimarySector();

         int i;
         for(i=pt.bodies.size(); i--; ){
            const S_phys_template::S_body &tb = pt.bodies[i];
            bodies.push_back(S_body());
            S_body &b = bodies.back();
            b.is_static = tb.is_static;
            b.density = tb.density;
            b.density_as_weight = tb.density_as_weight;

            b.frm = tb.name=="*" ? prim_sect : scn->FindFrame(tb.name);
            if(!b.frm){
               ps->ErrReport(C_xstr("PhysStudio: frame '%' not in scene, removing body") %tb.name);
               bodies.pop_back();
            }
         }
         for(i=pt.joints.size(); i--; ){
            const S_phys_template::S_joint &tj = pt.joints[i];
            joints.push_back(S_joint());
            S_joint &j = joints.back();

            j.frm = scn->FindFrame(tj.name);
            j.type = tj.type;
            memcpy(j.val, tj.val, sizeof(j.val));
            if(!j.frm){
               ps->ErrReport(C_xstr("PhysStudio: frame '%' not in scene, removing joint") %tj.name);
               joints.pop_back();
            }
         }
      }
   };
   S_phys_info phys_info;


//----------------------------

   static BOOL CALLBACK dlgSheet_thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      C_physics_studio *ei = (C_physics_studio*)GetWindowLong(hwnd, GWL_USERDATA);
      if(!ei){
         if(uMsg!=WM_INITDIALOG)
            return 0;
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         ei = (C_physics_studio*)lParam;
         assert(ei);
      }
      return ei->dlgSheet(hwnd, uMsg, wParam, lParam);
   }

//----------------------------

   bool dlgSheet(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         {
            struct{
               IPH_JOINT_TYPE type;
               const char *name;
            } jt[] = {
               IPHJOINTTYPE_BALL, "Ball",
               IPHJOINTTYPE_HINGE, "Hinge",
               IPHJOINTTYPE_SLIDER, "Slider",
               IPHJOINTTYPE_UNIVERSAL, "Universal",
               IPHJOINTTYPE_HINGE2, "Hinge2",
               IPHJOINTTYPE_FIXED, "Fixed",
               IPHJOINTTYPE_NULL
            };
            for(dword i=0; jt[i].type; i++){
               dword j = SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_ADDSTRING, 0, (LPARAM)jt[i].name);
               SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_SETITEMDATA, j, jt[i].type);
            }
         }
         break;
      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCLOSE:
            Action(E_ACTION_PHYS_STUDIO_TOGGLE);
            break;

         case IDC_PH_RESET:
            if(!phys_info.IsEmpty()){
               if(!ed->CanModify()) break;
               int i = MessageBox((HWND)ed->GetIGraph()->GetHWND(), "Are you sure to reset all physic info?", "Physics studio", MB_YESNO);
               if(i==IDYES){
                  phys_info.Clear();
                  UpdateSheet(e_slct->GetCurSel());
                  ed->SetModified();
               }
            }
            break;

         case IDC_PH_BODY:
         case IDC_PH_JOINT:
            {
               if(!ed->CanModify()) break;
               dword id = LOWORD(wParam);
               bool jnt = (id==IDC_PH_JOINT);
               bool on = IsDlgButtonChecked(hwnd, id);
                              //toggle state
               on = !on;
                              //add selection among bodies (of not yet)
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               for(int i=sel_list.size(); i--; ){
                  PI3D_frame frm = sel_list[i];
                  int l_i = (jnt ? phys_info.JointIndex(frm) : phys_info.BodyIndex(frm));
                  if(on){
                     if(l_i==-1){
                              //remove from opposite list first, then add to current
                        if(jnt){
                           /*
                           int ii = phys_info.BodyIndex(frm);
                           if(ii!=-1)
                              phys_info.RemoveBody(ii);
                              */
                           phys_info.joints.push_back(S_phys_info::S_joint());
                           phys_info.joints.back().frm = frm;
                        }else{
                           /*
                           int ii = phys_info.JointIndex(frm);
                           if(ii!=-1)
                              phys_info.RemoveJoint(ii);
                              */
                           phys_info.bodies.push_back(S_phys_info::S_body());
                           phys_info.bodies.back().frm = frm;
                        }
                     }
                  }else
                  if(l_i!=-1){
                     if(jnt){
                        phys_info.RemoveJoint(l_i);
                     }else{
                        phys_info.RemoveBody(l_i);
                     }
                  }
               }
               UpdateSheet(sel_list);
               ed->SetModified();
            }
            break;

         case IDC_PH_BODY_STATIC:
            {
               if(!ed->CanModify()) break;
               dword id = LOWORD(wParam);
               bool on = IsDlgButtonChecked(hwnd, id);
                              //toggle state
               on = !on;
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               for(int i=sel_list.size(); i--; ){
                  PI3D_frame frm = sel_list[i];
                  int ii = phys_info.BodyIndex(frm);
                  assert(ii!=-1);
                  phys_info.bodies[ii].is_static = on;
               }
               UpdateSheet(sel_list);
               ed->SetModified();
            }
            break;

         case IDC_PH_UNIT_DENSITY:
         case IDC_PH_UNIT_WEIGHT:
            {
               if(!ed->CanModify()) break;
               dword id = LOWORD(wParam);
               bool d_a_w = (id==IDC_PH_UNIT_WEIGHT);

               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               for(int i=sel_list.size(); i--; ){
                  PI3D_frame frm = sel_list[i];
                  int ii = phys_info.BodyIndex(frm);
                  assert(ii!=-1);
                  phys_info.bodies[ii].density_as_weight = d_a_w;
               }
               UpdateSheet(sel_list);
               ed->SetModified();
            }
            break;

         case IDC_PH_JTYPE:
            switch(HIWORD(wParam)){
            case CBN_SELCHANGE:
               {
                  if(!ed->CanModify()) break;
                  int sel = SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_GETCURSEL, 0, 0);
                  IPH_JOINT_TYPE type = (IPH_JOINT_TYPE)SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_GETITEMDATA, sel, 0);
                              //set to all selected joints
                  const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                  for(int i=sel_list.size(); i--; ){
                     PI3D_frame frm = sel_list[i];
                     int ii = phys_info.JointIndex(frm);
                     assert(ii!=-1);
                     phys_info.joints[ii].type = type;
                  }
                  ed->SetModified();
               }
               break;
            }
            break;
         default:
            switch(HIWORD(wParam)){
            case EN_CHANGE:
               {
                  if(!hwnd_sheet)
                     break;
                  if(!ed->CanModify()) break;
                  dword id = LOWORD(wParam);
                              //check which value is being changes
                  for(int vi=S_phys_template::VAL_LAST; vi--; ){
                     if(val_ids[vi]==id)
                        break;
                  }
                              //scan value
                  char buf[256];
                  GetDlgItemText(hwnd, id, buf, sizeof(buf));
                  float value;
                              //check for clearing the field (set default val if so)
                  if(!*buf)
                     value = DEFAULT_VALUE;
                  else{
                     int n;
                     if(sscanf(buf, "%f%n", &value, &n)==-1 || !n)
                        break;
                  }

                              //clamp some values
                  bool clamp = false;
                  switch(vi){
                  case S_phys_template::VAL_BOUNCE:
                  case S_phys_template::VAL_STOP_ERP:
                  case S_phys_template::VAL_SUSP_ERP:
                     if(value<0.0f){ value = 0.0f; clamp = true; }
                     else
                     if(value>1.0f){ value = 1.0f; clamp = true; }
                     break;
                  case S_phys_template::VAL_CFM:
                  case S_phys_template::VAL_STOP_CFM:
                  case S_phys_template::VAL_SUSP_CFM:
                     if(value<0.0f){ value = 0.0f; clamp = true; }
                     break;
                  }
                  if(clamp){
                     hwnd_sheet = NULL;
                     SetDlgItemText(hwnd, id, FloatStrip(C_xstr("%")%value));
                     hwnd_sheet = hwnd;
                  }

                              //modify all selected frames
                  const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
                  for(int i=sel_list.size(); i--; ){
                     PI3D_frame frm = sel_list[i];
                     if(vi!=-1){
                              //it's joint
                        int ii = phys_info.JointIndex(frm);
                        assert(ii!=-1);
                        phys_info.joints[ii].val[vi] = value;
                     }else
                     switch(id){
                     case IDC_PH_DENSITY:
                        {
                           int ii = phys_info.BodyIndex(frm);
                           assert(ii!=-1);
                           phys_info.bodies[ii].density = value;
                        }
                        break;
                     }
                  }
                  ed->SetModified();
               }
               break;
            }
         }
         break;

      case WM_HELP:
         {
            HELPINFO *hi = (HELPINFO*)lParam;
            static S_ctrl_help_info help_texts[] = {
               IDC_PH_BODY, "Make selected objects to act as rigid-body parts of physics.",
               IDC_PH_BODY_STATIC, "Make body fixed (not moving) - suitable for debugging.",
               //IDC_PH_BODY_ROOT, "Instead of this frame, use model's root frame for this body.",
               IDC_PH_JOINT,  "Make selected objects to act as constraint joints. Note that joint is connected to one or two bodies - to make the connection, link the joint to body1, and (optionally) link body2 to the joint.",
               IDC_PH_JTYPE,  "Type of joint.",

               IDC_LO,  "Set low rotational/sliding limit. Positional or angle values are applied, depending on joint type (angle values are in degrees).",
               IDC_HI,  "Set high rotational/sliding limit. Positional or angle values are applied, depending on joint type (angle values are in degrees).",
               IDC_MAXF,  "Maximal force applied by joint onto the constraint. By default, this is a 'suspension' parameter, making the joint move hard.",
               IDC_FUDGE,  "...",
               IDC_BOUNCE,  "Bounce value used when joint reaches limits.",
               IDC_CFM,  "Constraint-force-mixing value. This value specifies how hard joint tries to correct the constraint situation.\nIf CFM is set to zero, the constraint will be hard. If CFM is set to a positive value, it will be possible to violate the constraint by ``pushing on it'' (for example, for contact constraints by forcing the two contacting objects together). In other words the constraint will be soft, and the softness will increase as CFM increases. What is actually happeneng here is that the constraint is allowed to be violated by an amount proportional to CFM times the restoring force that is needed to enforce the constraint.",

               IDC_S_ERP,  "Error-reduction-parameter used at the limit stop.\nThere is a mechanism to reduce joint error: during each simulation step each joint applies a special force to bring its bodies back into correct alignment. This force is controlled by the error reduction parameter (ERP), which has a value between 0 and 1.\nThe ERP specifies what proportion of the joint error will be fixed during the next simulation step. If ERP=0 then no correcting force is applied and the bodies will eventually drift apart as the simulation proceeds. If ERP=1 then the simulation will attempt to fix all joint error during the next time step. However, setting ERP=1 is not recommended, as the joint error will not be completely fixed due to various internal approximations. A value of ERP=0.1 to 0.8 is recommended (0.2 is the default).\nA global ERP value can be set that affects most joints in the simulation. However some joints have local ERP values that control various aspects of the joint.",
               IDC_S_CFM,  "Constraint-force-mixing used at the limit stop. See 'CFM' for details on this parameter.",

               IDC_SU_ERP,  "Suspension error-reduction-parameter (currently applcable only to Hinge2 joint). See 'Stop ERP' for more info on ERP parameter.",
               IDC_SU_CFM,  "Suspension constraint-force-mixing (currently applicable only to Hinge2 joint). See 'CFM' for details on CFM parameter.",
               IDC_PH_DENSITY,"Density of the body. The density is applied onto all body volumes.",
               0              //terminator
            };
            DisplayControlHelp(hwnd, (word)hi->iCtrlId, help_texts);
         }
         break;
      }
      return 0;
   }

//----------------------------

   void InitPhysStudio(){

      //world = mission.GetPhysicsWorld();
      if(!world){
         world = IPHCreateWorld(); world->Release();
      }

                              //install editor notifications
      e_slct->AddNotify(this, E_SEL_NOTIFY);
                              //create and init property sheet
      hwnd_sheet = CreateDialogParam(GetModuleHandle(NULL), "IDD_PHYS_STUDIO",
         (HWND)ed->GetIGraph()->GetHWND(),
         dlgSheet_thunk, (LPARAM)this);
      e_props->AddSheet(hwnd_sheet);

      UpdateSheet(e_slct->GetCurSel());
      //EnableSheetControls(false);
   }

//----------------------------

   void ClosePhysStudio(){

      if(!hwnd_sheet)
         return;
                              //remove notifies
      e_slct->RemoveNotify(this);

      e_props->RemoveSheet(hwnd_sheet);
      DestroyWindow(hwnd_sheet);
      hwnd_sheet = NULL;
   }

//----------------------------
// Enable/disable all controls in the property sheet window.
   void EnableSheetControls(bool b){

      HWND hwnd_close = GetDlgItem(hwnd_sheet, IDCLOSE);
      for(HWND hwnd = GetWindow(hwnd_sheet, GW_CHILD); hwnd; hwnd = GetWindow(hwnd, GW_HWNDNEXT)){
         if(hwnd!=hwnd_close)
            EnableWindow(hwnd, b);
      }
   }

//----------------------------

   void UpdateSheet(const C_vector<PI3D_frame> &sel_list){

      HWND hwnd = hwnd_sheet;
      if(!sel_list.size()){
         EnableSheetControls(false);
         /*
         SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_SETCURSEL, -1, 0);
         CheckDlgButton(hwnd, IDC_PH_BODY_STATIC, false);
         return;
         */
      }else
      EnableSheetControls(true);

                              //temporary clear to mark that we're modifying
      hwnd_sheet = NULL;

                              //count how many joint and bodies are there in selection
      dword num_b = 0, num_j = 0;
      for(int i=sel_list.size(); i--; ){
         CPI3D_frame frm = sel_list[i];
         if(phys_info.BodyIndex(frm)!=-1)
            ++num_b;
         if(phys_info.JointIndex(frm)!=-1)
            ++num_j;
      }
                              //setup body info
      if(!num_b || num_b!=sel_list.size()){
                              //clear
         CheckDlgButton(hwnd, IDC_PH_BODY, false);
         static const struct S_off_data{
            dword id;
            bool also_txt;
         } off_ids[] = {
            IDC_PH_BODY_STATIC, false,
            IDC_PH_DENSITY, true,
            IDC_PH_UNIT_DENSITY, false,
            IDC_PH_UNIT_WEIGHT, false,
            0
         };
         for(i=0; off_ids[i].id; i++){
            HWND h = GetDlgItem(hwnd, off_ids[i].id);
            EnableWindow(h, false);
            if(off_ids[i].also_txt)
               SetWindowText(h, "");
         }
      }
                              //setup joint info
      if(!num_j || num_j!=sel_list.size()){
                              //clear/disable irrelevant parts
         SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_SETCURSEL, (WPARAM)-1, 0);
         static const dword off_ids[] = {
            IDC_PH_JTYPE,
            IDC_LO, IDC_HI,
            IDC_LO1, IDC_HI1,
            IDC_LO2, IDC_HI2,
            IDC_MAXF,
            IDC_FUDGE,
            IDC_BOUNCE,
            IDC_CFM,
            IDC_S_CFM,
            IDC_S_ERP,
            IDC_SU_CFM,
            IDC_SU_ERP,
            0
         };
         for(i=0; off_ids[i]; i++){
            HWND h = GetDlgItem(hwnd, off_ids[i]);
            EnableWindow(h, false);
            SetWindowText(h, "");
         }
         CheckDlgButton(hwnd, IDC_PH_JOINT, false);
      }else{
         
      }

      if(num_j!=sel_list.size() && num_b!=sel_list.size()){
                              //mixed mode, set indeterminate state of checkboxes
         CheckDlgButton(hwnd, IDC_PH_BODY, num_b ? BST_INDETERMINATE : false);
         CheckDlgButton(hwnd, IDC_PH_JOINT, num_j ? BST_INDETERMINATE : false);
      }else{
         if(num_b && num_b==sel_list.size()){
                                 //init pure body part
            CheckDlgButton(hwnd, IDC_PH_BODY, true);
            bool valid[3] = {true, true, true };
            bool stat = false;
            float density = 0;
            bool d_a_w = false;

            for(i=0; i<(int)sel_list.size(); i++){
               int ii = phys_info.BodyIndex(sel_list[i]);
               assert(ii!=-1);
               const S_phys_info::S_body &b = phys_info.bodies[ii];
               if(!i){
                  stat = b.is_static;
                  density = b.density;
                  d_a_w = b.density_as_weight;
               }else{
                  if(stat != b.is_static)
                     valid[0] = false;
                  if(density!=b.density)
                     valid[1] = false;
                  if(d_a_w != b.density_as_weight)
                     valid[2] = false;
               }
            }
            CheckDlgButton(hwnd, IDC_PH_BODY_STATIC, !valid[0] ? BST_INDETERMINATE : stat);
            SetDlgItemText(hwnd, IDC_PH_DENSITY, !valid[1] ? "?" :
               IsDefaultValue(density) ? "" : (const char*)FloatStrip(C_xstr("%")%density));
            CheckDlgButton(hwnd, IDC_PH_UNIT_DENSITY, !valid[2] ? false : !d_a_w);
            CheckDlgButton(hwnd, IDC_PH_UNIT_WEIGHT, !valid[2] ? false : d_a_w);
         }
         if(num_j && num_j==sel_list.size()){
                                 //init pure joint part
            CheckDlgButton(hwnd, IDC_PH_JOINT, true);
            IPH_JOINT_TYPE t = IPHJOINTTYPE_NULL;
            const int NUM_VALS = S_phys_template::VAL_LAST;
            float val[NUM_VALS];
            bool valid[NUM_VALS];
            for(i=NUM_VALS; i--; )
               valid[i] = true;

            for(i=0; i<(int)sel_list.size(); i++){
               int ii = phys_info.JointIndex(sel_list[i]);
               S_phys_info::S_joint &ji = phys_info.joints[ii];
               assert(ii!=-1);
               if(!i)
                  t = ji.type;
               else
               if(t != ji.type)
                  t = IPHJOINTTYPE_NULL;
                                 //check all vals
               for(int j=NUM_VALS; j--; ){
                  if(!i)
                     val[j] = ji.val[j];
                  else
                  if(valid[j])
                     valid[j] = (I3DFloatAsInt(val[j]) == I3DFloatAsInt(ji.val[j]));
               }
            }
            if(t){
               for(i=0; ; i++){
                  int id = SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_GETITEMDATA, i, 0);
                  if(id==CB_ERR)
                     break;
                  if(id==t){
                     SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_SETCURSEL, i, 0);
                     break;
                  }
               }
            }else
               SendDlgItemMessage(hwnd, IDC_PH_JTYPE, CB_SETCURSEL, (WPARAM)-1, 0);
            for(i=NUM_VALS; i--; ){
               SetDlgItemText(hwnd, val_ids[i], !valid[i] ? "?" :
                  IsDefaultValue(val[i]) ? "" : (const char*)FloatStrip(C_xstr("%")%val[i]));
            }
         }
      }
                              //restore
      hwnd_sheet = hwnd;
   }

//----------------------------

   void PhysicsStudioTest(){

      if(phys_info.IsEmpty()){
         ed->Message("Can't test physics studio - no information set!");
         return;
      }
      if(!world){
         world = IPHCreateWorld(); world->Release();
      }
      PI3D_scene scn = ed->GetScene();
                              //lock further editing
      ((PC_editor_item_Mission)ed->FindPlugin("Mission"))->EditLock();

      for(dword i=bodies.size(); i--; )
         CloseModel(bodies[i]);

                              //export into intermediate template
      S_phys_template pt;
      phys_info.Export(pt, ed->GetScene());

      if(pt.InitSimulation(bodies, joints, world, scn, NULL)){
         assert(bodies.size()==pt.bodies.size());
                              //for all 'static' bodies, make additional fixed joint
         for(i=0; i<pt.bodies.size(); i++){
            if(pt.bodies[i].is_static){
                              //attach to one fixed joint
               PIPH_joint_fixed jnt = (PIPH_joint_fixed)world->CreateJoint(IPHJOINTTYPE_FIXED);
               joints.push_back(jnt);
               jnt->Attach(bodies[i], NULL);
               jnt->SetFixed();
               jnt->Release();
            }

         }
      }
   }

//----------------------------

   void Command(const char *cmd){

      switch(*cmd){
         /*
      case 'b':
      case 'k':
      case 's':
      case 'c':
      //case 'x':
      case 'l':
      case 'h':
         {
            S_object &obj = objs[nextobj];
            ++nextobj %= MAX_NUM;

            obj.Clear();
            obj.body = world->CreateBody();
            obj.body->Release();

            float scale = .2f + S_float_random() * 1.0f;

            S_vector pos;
            S_quat rot;
            if(random_pos){
               pos.x = S_float_random() * 2.0f - 1.0f;
               pos.y = S_float_random() + 3.0f;
               pos.z = S_float_random() * 2.0f - 1.0f;

               S_vector ax;
               ax.x = S_float_random()*2.0f-1.0f;
               ax.y = S_float_random()*2.0f-1.0f;
               ax.z = S_float_random()*2.0f-1.0f;
               const float a = S_float_random(PI*2.0f);
               rot.Make(ax, a);
            }else{
               pos.Zero();
               for(int k=0; k<MAX_NUM; k++){
                  if(objs[k].body){
                     S_vector _bpos;
                     S_quat r;
                     objs[k].body->GetPosRot(_bpos, r);
	                  if(pos.y < _bpos[1])
                        pos.y = _bpos[1];
                  }
               }
               pos.y += 1.0f;

               const float a = S_float_random(PI*2.0f);
               rot.Make(S_vector(0, 1, 0), a);
            }
            obj.body->SetPosRot(pos, rot);

            const char *mod_name = NULL;

            switch(*cmd){
            case 'b': mod_name = "test\\_box"; break;
            case 'c': scale *= .5f; mod_name = "test\\capcylinder"; break;
            case 'l': scale *= .5f; mod_name = "test\\_cylinder"; break;
            case 's': scale *= .5f; mod_name = "test\\sphere"; break;
            case 'h':
               scale *= .5f;
               mod_name = "+test";
               break;
            case 'k':
               mod_name = "test\\block";
               break;
            }
            obj.InitModel(ed->GetScene(), mod_name, scale);
         }
         break;
         */

      case 'j':               //joint
         {
            /*
            S_object &obj0 = objs[nextobj];
            ++nextobj %= MAX_NUM;

            S_object &obj1 = objs[nextobj];
            ++nextobj %= MAX_NUM;

            obj0.Clear();
            obj0.body = world->CreateBody();
            obj0.body->Release();

            obj1.Clear();
            obj1.body = world->CreateBody();
            obj1.body->Release();
            */
            PIPH_body b0 = world->CreateBody();
            PIPH_body b1 = world->CreateBody();
            bodies.push_back(b0);
            bodies.push_back(b1);
            b0->Release();
            b1->Release();

            S_quat rot;
            rot.Make(S_vector(1, 0, 0), PI*.0f);
            S_vector pos(.5f, .5f, 0);

            {
               InitModel(b0, ed->GetScene(), "test\\_box", 1.0f);
               b0->SetPosRot(S_vector(0, pos.y, 0), rot);
            }
            {
               InitModel(b1, ed->GetScene(), "test\\_box", .5f);
               b1->SetPosRot(S_vector(.6f, pos.y, 0), S_quat(1, 0, 0, 0));
            }
            {
               static const struct{
                  char cmd;
                  IPH_JOINT_TYPE jt;
               } j_map[] = {
                  'b', IPHJOINTTYPE_BALL,
                  'h', IPHJOINTTYPE_HINGE,
                  '2', IPHJOINTTYPE_HINGE2,
                  's', IPHJOINTTYPE_SLIDER,
                  'u', IPHJOINTTYPE_UNIVERSAL,
                  'f', IPHJOINTTYPE_FIXED,
                  0, IPHJOINTTYPE_NULL
               };
               for(int i=0; j_map[i].cmd && j_map[i].cmd!=cmd[1]; i++);
               IPH_JOINT_TYPE jt = j_map[i].jt;
               if(!jt)
                  jt = IPHJOINTTYPE_BALL;

               PIPH_joint j = world->CreateJoint(jt);
               assert(j);
               j->Attach(b0, b1);
               j->SetAnchor(pos);

               joints.push_back(j);

               switch(jt){
               case IPHJOINTTYPE_BALL:
                  {
                     /*
                     PIPH_joint_ball jb = (PIPH_joint_ball)j;
                     jb->SetUserMode(true);
                     jb->SetNumAxes(3);

                     jb->SetAxis(0, S_vector(1, 0, 0), IPH_JOINT_ball::AXIS_BODY1);
                     jb->SetAxis(1, S_vector(0, 1, 0), IPH_JOINT_ball::AXIS_BODY1);
                     jb->SetAxis(2, S_vector(0, 0, 1), IPH_JOINT_ball::AXIS_BODY1);

                     for(dword i=3; i--; ){
                        j->SetStopCFM(5.0f, i);

                        j->SetDesiredVelocity(0.0f, i);
                        j->SetMaxForce(.01f, i);

                        j->SetLoStop(-PI*.1f, i);
                        j->SetHiStop(PI*.1f, i);
                     }
                     */
                  }
                  break;
               case IPHJOINTTYPE_FIXED:
                  ((PIPH_joint_fixed)j)->SetFixed();
                  break;
               case IPHJOINTTYPE_HINGE:
                  {
                     PIPH_joint_hinge jh = (PIPH_joint_hinge)j;
                     //jh->SetAxis(S_vector(1, 0, 0));
                     jh->SetAxis(S_vector(0, 1, 0));
                  }
                  break;
               case IPHJOINTTYPE_SLIDER:
                  {
                     PIPH_joint_slider jh = (PIPH_joint_slider)j;
                     jh->SetAxis(S_vector(1, 0, 0));
                  }
                  break;
               case IPHJOINTTYPE_UNIVERSAL:
                  {
                     PIPH_joint_universal ju = (PIPH_joint_universal)j;
                     ju->SetAxis(0, S_vector(0, 1, 0));
                     ju->SetAxis(1, S_vector(0, 0, 1));
                  }
                  break;
               case IPHJOINTTYPE_HINGE2:
                  {
                     PIPH_joint_hinge2 j2 = (PIPH_joint_hinge2)j;
                     j2->SetAxis(0, S_vector(0, 1, 0));
                     j2->SetAxis(1, S_vector(1, 0, 0));
                  }
                  break;
               }
                              //setup some parameters
               //j->SetLoStop(-PI*.2f);
               //j->SetHiStop(PI*.5f);
               //j->SetBounce(.1f);
               //j->SetStopERP(.1f);
               //j->SetStopCFM(5.2f);

               //j->SetSuspensionCFM(2.5f);
               //j->SetSuspensionERP(2.5f);
               //j->SetDesiredVelocity(0.0f);
               //j->SetMaxForce(.01f);

               j->Release();
            }
         }
         break;

      case 'i':
         {
            PIPH_body body = world->CreateBody();
            bodies.push_back(body);
            body->Release();

            /*
            S_object &obj0 = objs[nextobj];
            ++nextobj %= MAX_NUM;

            obj0.Clear();
            obj0.body = world->CreateBody();
            obj0.body->Release();
            */

            const char *mod_name = NULL;
            {
               S_vector pos(0, .5f, 0);
               float scale = 1.0f;
               S_quat rot;
               rot.Identity();

               switch(cmd[1]){
               case 'm':
                  {
                     PI3D_frame frm = ed->GetScene()->FindFrame("phys");
                     if(frm){
                        if(body->SetFrame(frm, IPH_STEFRAME_USE_TRANSFORM | IPH_STEFRAME_HR_VOLUMES))
                           break;
                        ed->Message("can't use model 'phys'");
                     }else{
                        ed->Message("can't find model 'phys'");
                     }
                  }
                              //... flow
               case 'b':
               default:
                  {
                     mod_name = "test\\_box";
                     pos.y = .6f;
                     const float a = 21.0f*PI/180.0f;
                     rot.Make(S_vector(1, 0, 0), a);
                     rot *= S_quat(S_vector(0, 0, 1), PI*.2f);
                  }
                  break;
               case 'k':
                  mod_name = "test\\block";
                  break;
               case 's':
                  scale *= .5f;

                  //scale = .05f; pos.y += 2.0f; pos.x -= 2.0f;
                  mod_name = "test\\sphere";
                  break;
               case 'c':
                  {
                     scale *= .25f;

                     scale = .03f; pos.y += 1.0f;
                     mod_name = "test\\capcylinder";
                     rot.Make(S_vector(1, 0, 0), PI*.45f);
                     //obj0.body->AddForceAtPos(S_vector(2, 0, 0), S_vector(0, 0, 300));
                     //obj0.body->AddForceAtPos(S_vector(-2, 0, 0), S_vector(0, 0, -300));
                  }
                  break;
               case 'l':
                  scale *= .5f;
                  mod_name = "test\\_cylinder";
                  pos.y = 1.0f;
                  break;
               case 'h':
                  {
                     scale *= .5f;
                     mod_name = "+test";
                  }
                  break;
               }
               if(mod_name){
                  body->SetPosRot(pos, rot);
                  InitModel(body, ed->GetScene(), mod_name, scale);
               }
            }
         }
         break;
      }
   }

//----------------------------

   //static void ContactReport(CPI3D_volume src_vol, PIPH_body src_body, CPI3D_frame dst_frm, PIPH_body dst_body,
      //const S_vector &pos, const S_vector &normal, float depth, void *context){
   static void I3DAPI ContactReport(CPI3D_frame frm, const S_vector &pos, const S_vector &normal,
      float depth, void *context){

      C_physics_studio *ps = (C_physics_studio*)context;
      ps->DebugLine(pos, pos + normal * .2f, 1, 0x8000ff00);
      ps->DebugPoint(pos, .1f, 1, 0x80ffff00);
   }

//----------------------------

   virtual void AfterLoad(){

      if(active)
         InitPhysicsScene();
   }

//----------------------------

   virtual void BeforeFree(){
      if(active)
         ClosePhysicsScene();
      phys_info.Clear();
   }

//----------------------------

   virtual void LoadFromMission(C_chunk &ck){

      S_phys_template pt;
      pt.Load(ck);
      phys_info.Import(pt, ed->GetScene(), this);
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==2){
         if(!phys_info.IsEmpty()){
            //ck <<= CT_EDITOR_PLUGIN;
            ck <<= CT_PHYSICS_TEMPLATE;
            {
               //ck(CT_NAME, GetName());
               phys_info.Save(ck, ed->GetScene());
            }
            --ck;
         }
      }
   }

//----------------------------

   virtual void SetActive(bool a){

      if(!a)
         if(active)
            InitPhysicsScene();
   }

//----------------------------

   virtual void OnFrameDelete(PI3D_frame frm){

      int i = phys_info.BodyIndex(frm);
      if(i!=-1)
         phys_info.RemoveBody(i);
      i = phys_info.JointIndex(frm);
      if(i!=-1)
         phys_info.RemoveJoint(i);
   }

//----------------------------

public:
   C_physics_studio():
      col_test(false),
      active(false),
      phys_studio_active(false),
      hwnd_sheet(NULL),
      draw_contacts(false),
      statistics(false)
   {
   }

//----------------------------

   virtual void Close(){

      ClosePhysicsScene();
      ClosePhysStudio();
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      do{
         if(!pick_body)
            break;
         if(!(mouse_butt&1)){
            pick_body = NULL;
            break;
         }

                              //draw line of picking
         S_vector from = pick_pos * pick_body->GetFrame()->GetMatrix();
         S_vector to, ldir;
         ed->GetScene()->UnmapScreenPoint(ed->GetIGraph()->Mouse_x(), ed->GetIGraph()->Mouse_y(), to, ldir);
         to += ldir * pick_dist;
         DebugLine(from, to, 1, 0xff00ff00);

         S_vector dir =
            to - from;
            //ldir;
         float m = pick_body->GetWeight();
         dword skeys = ed->GetIGraph()->GetShiftKeys();
         switch(skeys){
         case SKEY_SHIFT: m *= 4.0f; break;
         case SKEY_CTRL: m *= 2.0f; break;
         case SKEY_ALT: m *= .5f; break;
         }
         //dir *= 3.0f;
         dir *= m;
         //DEBUG(dir);

         DebugLine(from, from+dir, 1, 0xffff00ff);
         pick_body->AddForceAtPos(from, dir);
      }while(false);

      if(active && world){
         /*
         if(bodies.size()){
            PIPH_body b = bodies[0];
            S_vector pos;
            S_quat rot;
            b->GetPosRot(pos, rot);
            DebugLine(pos, pos+b->GetLinearVelocity(), 1);
            DebugLine(pos, pos+b->GetAngularVelocity(), 1, 0xff00ff00);
            pos += b->GetFrame()->GetMatrix()(0);
            DebugLine(pos, pos+b->GetVelAtPos(pos), 1, 0xffff0000);
         }
         */

         /*
                              //examine joint #0
         if(joints.size()){
            PIPH_joint jnt = joints.front();
            switch(jnt->GetType()){
            case IPHJOINTTYPE_HINGE:
               {
                  PIPH_joint_hinge j = PIPH_joint_hinge(jnt);
                  DEBUG(C_fstr("Hinge: angle: %.2f, rate: %.2f",
                     j->GetAngle(),
                     j->GetAngleRate()));
               }
               break;
            case IPHJOINTTYPE_SLIDER:
               DEBUG(PIPH_joint_slider(jnt)->GetSlidePos());
               DEBUG(PIPH_joint_slider(jnt)->GetSlidePosRate());
               break;
            case IPHJOINTTYPE_HINGE2:
               DEBUG(C_fstr("Hinge2: angle: %.2f, rate1: %.2f, rate2: %.2f",
                  PIPH_joint_hinge2(jnt)->GetAngle(),
                  PIPH_joint_hinge2(jnt)->GetAngleRate(0),
                  PIPH_joint_hinge2(jnt)->GetAngleRate(1)));
               break;
            }
         }
         */
      }
      /*
      if(statistics && world){
         DEBUG(C_xstr("Physics statistics:"));
         DEBUG(C_xstr(" num bodies: %") % (int)world->NumBodies());
         DEBUG(C_xstr(" num joints: %") % (int)world->NumJoints());
         DEBUG(C_xstr(" num contacts: %") % (int)world->NumContacts());
      }
      */
   }

//----------------------------

   virtual dword Action(int id, void *context = NULL){

      switch(id){
         /*
      case E_PHYS_JOINT_BALL: Command("jb"); break;
      case E_PHYS_JOINT_HINGE: Command("jh"); break;
      case E_PHYS_JOINT_HINGE2: Command("j2"); break;
      case E_PHYS_JOINT_SLIDER: Command("js"); break;
      case E_PHYS_JOINT_UNIVERSAL: Command("ju"); break;
      case E_PHYS_JOINT_FIXED: Command("jf"); break;
      case E_PHYS_JOINT_AMOTOR: Command("jm"); break;
      */

      case E_PHYS_ACTIVE:
         active = !active;
         ed->CheckMenu(this, id, active);
         if(active)
            InitPhysicsScene();
         else
            ClosePhysicsScene();
         break;

      case E_ACTION_PHYS_STUDIO_TOGGLE:
         phys_studio_active = !phys_studio_active;
         ed->CheckMenu(this, id, phys_studio_active);
         if(phys_studio_active){
            InitPhysStudio();
         }else{
            ClosePhysStudio();
         }
         break;

      case E_ACTION_PHYS_STUDIO_TEST:
         PhysicsStudioTest();
         break;

      case E_PHYS_DRAW_CONTACTS:
         draw_contacts = !draw_contacts;
         ed->CheckMenu(this, id, draw_contacts);
         break;

      case E_PHYS_STATS:
         statistics = !statistics;
         ed->CheckMenu(this, id, statistics);
         break;

      case E_ACTION_EDIT_AUTO_CMD:
         WinGetName("Auto-start command", auto_command, ed->GetIGraph()->GetHWND());
         break;

      case E_ACTION_PICK_MODE:
         e_mouseedit->SetUserPick(this, E_ACTION_DO_PICK, LoadCursor(GetModuleHandle(NULL), "IDC_CURSOR_DEBUG_PHYS"));
         break;

      case E_ACTION_DO_PICK:
         {
            if(!world)
               break;
            const S_MouseEdit_picked *mp = (S_MouseEdit_picked*)context;
            dword skeys = ed->GetIGraph()->GetShiftKeys();
            switch(skeys){
            case SKEY_CTRL:
            case SKEY_ALT:
            case SKEY_SHIFT:
               {
                              //generate sphere at this position
                  S_vector pos = mp->pick_from + mp->pick_dir * mp->pick_dist;
                  pos.y += 2.0f;

                  /*
                  S_object &obj = objs[nextobj];
                  ++nextobj %= MAX_NUM;
                  obj.Clear();
                  obj.body = world->CreateBody();
                  obj.body->Release();
                  */
                  PIPH_body body = world->CreateBody();
                  bodies.push_back(body);
                  body->Release();

                  const char *mod_name = NULL;
                  float scale = 1.0f;

                  switch(skeys){
                  case SKEY_CTRL: mod_name = "test\\sphere"; break;
                  case SKEY_ALT: mod_name = "test\\block"; break;
                  case SKEY_SHIFT: mod_name = "test\\capcylinder"; scale = .4f; break;
                  default: assert(0);
                  }
                  {
                     S_quat rot;
                     rot.Make(S_vector(1, 0, 0), S_float_random(PI*2.0f));
                     rot *= S_quat(S_vector(0, 0, 1), S_float_random(PI*2.0f));
                     body->SetPosRot(pos, rot);
                  }
                  InitModel(body, ed->GetScene(), mod_name, scale);
               }
               break;
            default:
                              //pick volume
               I3D_collision_data cd(mp->pick_from, mp->pick_dir, I3DCOL_VOLUMES | I3DCOL_RAY);
               if(!ed->GetScene()->TestCollision(cd))
                  break;
               PI3D_frame frm = cd.GetHitFrm();
               assert(frm->GetType()==FRAME_VOLUME);
               pick_body = world->GetVolumeBody(I3DCAST_VOLUME(frm));
               if(pick_body){
                  pick_dist = cd.GetHitDistance();
                  pick_pos = cd.ComputeHitPos();
                  pick_pos *= pick_body->GetFrame()->GetInvMatrix();

                  float n, f;
                  ed->GetScene()->GetActiveCamera()->GetRange(n, f);
                  pick_dist += n;
                  ed->Message(C_xstr("Picked body: '%'") % pick_body->GetFrame()->GetName());
               }
            }
         }
         return true;

      //case E_ACTION_GET_CONTACT_REPORT:
         //return dword(draw_contacts ? ContactReport : NULL);

      case E_SEL_NOTIFY:
         UpdateSheet(*(C_vector<PI3D_frame>*)context);
         break;
      }
      return 0;
   }

//----------------------------

   void RenderCircleSector(const S_vector &pos, const S_vector &axis1, const S_vector &axis2, float len,
      float lo_angle, float hi_angle, dword color1, dword color2){

      const int MAX_P = 24;
      float delta = hi_angle - lo_angle;
      int num_p = Max(2, Min(MAX_P, int(delta * 6.0f)));

                              //generate points and indices
      S_vector pt[MAX_P+1];
      I3D_triface fc[2][MAX_P-1];
      word line_indx[MAX_P+1][2];
      pt[num_p] = pos;
      S_quat rot_dir;
      rot_dir.SetDir(axis2, axis1);
      float step = delta / float(num_p-1);

      for(int i=num_p; i--; ){
         float angle = lo_angle + float(i)*step;
         S_quat rot_y(S_vector(0, 1, 0), angle);
         S_quat r = rot_y * rot_dir;
         pt[i] = pos + r.GetDir()*len;
         if(i!=num_p-1){
            I3D_triface &fc1 = fc[0][i];
            fc1[0] = word(num_p);
            fc1[1] = word(i);
            fc1[2] = word(i+1);

            I3D_triface &fc2 = fc[1][i];
            fc2[0] = word(num_p);
            fc2[1] = word(i+1);
            fc2[2] = word(i);
         }
         line_indx[i][0] = word(i);
         line_indx[i][1] = word(i+1);
      }
      line_indx[num_p][0] = word(num_p);
      line_indx[num_p][1] = 0;

      ed->GetDriver()->SetTexture(NULL);
      PI3D_scene scn = ed->GetScene();
      scn->DrawTriangles(pt, num_p+1, I3DVC_XYZ, &fc[0][0][0], 3*(num_p-1), color1);
      scn->DrawTriangles(pt, num_p+1, I3DVC_XYZ, &fc[1][0][0], 3*(num_p-1), color1);
      scn->DrawLines(pt, num_p+1, line_indx[0], (num_p+1)*sizeof(word), color2);
   }

//----------------------------

   void Render(){

                              //visualize phys studio stuff
      if(phys_studio_active){
         //PI3D_scene scn = ed->GetScene();

         int i;
         for(i=phys_info.joints.size(); i--; ){
            const S_phys_info::S_joint &jnt = phys_info.joints[i];
            CPI3D_frame frm = jnt.frm;
            const S_matrix &jm = frm->GetMatrix();
            const S_vector &j_pos = jm(3);

            const dword color = 0x80ff00ff;
            DebugPoint(j_pos, .2f, 1, color);
                              //draw connections towards bodies
            for(int j=2; j--; ){
               PI3D_frame fb = !j ? frm->GetParent() : frm->NumChildren() ? frm->GetChildren()[0] : NULL;
               if(fb){
                  if(phys_info.BodyIndex(fb)!=-1){
                     S_vector pos = fb->GetWorldPos();
                     DebugLine(pos, j_pos, 1, color);
                  }
               }
            }
                              //render joint-specific data
            float lo0 = jnt.val[S_phys_template::VAL_STOP_LO0], hi0 = jnt.val[S_phys_template::VAL_STOP_HI0];
            //float lo1 = jnt.val[S_phys_template::VAL_STOP_LO1], hi1 = jnt.val[S_phys_template::VAL_STOP_HI1];
            //float lo2 = jnt.val[S_phys_template::VAL_STOP_LO2], hi2 = jnt.val[S_phys_template::VAL_STOP_HI2];
            switch(jnt.type){
            case IPHJOINTTYPE_HINGE2:
               DebugLine(j_pos, j_pos+jm(1), 1, 0xffff00ff);
               DebugLine(j_pos, j_pos+jm(2), 1, 0xff00ffff);
               if(!IsDefaultValue(lo0) || !IsDefaultValue(hi0)){
                  RenderCircleSector(jm(3), jm(1), jm(2), .5f, lo0*PI/180.0f, hi0*PI/180.0f,
                     0x40ffff00, 0xe0ffff00);
               }
               break;
            case IPHJOINTTYPE_HINGE:
               DebugLine(j_pos, j_pos+jm(2), 1, 0xff00ffff);
               if(!IsDefaultValue(lo0) || !IsDefaultValue(hi0)){
                  RenderCircleSector(jm(3), jm(2), jm(1), .5f, lo0*PI/180.0f, hi0*PI/180.0f,
                     0x40ffff00, 0xe0ffff00);
               }
               break;
            case IPHJOINTTYPE_UNIVERSAL:
               DebugLine(j_pos-jm(0)*.5f, j_pos+jm(0)*.5f, 1, 0xff00ffff);
               DebugLine(j_pos-jm(1)*.5f, j_pos+jm(1)*.5f, 1, 0xff00ffff);
               break;
            case IPHJOINTTYPE_SLIDER:
               if(!IsDefaultValue(lo0) || !IsDefaultValue(hi0)){
                  S_normal n = jm(2);
                  DebugLine(j_pos-n*lo0, j_pos-n*hi0, 1, 0xff00ffff);
               }
               break;
            }
         }
         /*
         DebugLine(pos, pos + normal * .5f, 1, 0x8000ff00);
         */
      }

#if 1
      if(col_test){
         PI3D_scene scn = ed->GetScene();
         PI3D_volume frm = I3DCAST_VOLUME(scn->FindFrame("from", ENUMF_VOLUME));
         if(frm){
            /*
            struct S_hlp{
               //PI3D_material icon;
               //PI3D_scene scn;
               static dword I3DAPI cb_q(CPI3D_frame frm, void *context){
                  return 4;
               }
               static void I3DAPI cb_r(CPI3D_frame frm, const S_vector &pos, const S_vector &normal,
                  float depth, void *context){

                  //S_hlp *hp = (S_hlp*)context;
                  //hp->scn->DrawSprite(pos, hp->icon, 0xffffff00, .1f);
                  //hp->scn->DrawLine(pos, pos + normal * .5f, 0x8000ff00);
                  DebugPoint(pos, .1f, 1, 0xffffff00);
                  DebugLine(pos, pos + normal * .5f, 1, 0x8000ff00);
                  DEBUG(depth);
               }
            };// hlp;
            */

            I3D_contact_data cd;
            cd.vol_src = frm;
            //cd.cb_query = S_hlp::cb_q;
            cd.cb_report = ContactReport;
            cd.context = this;

            scn->GenerateContacts(cd);
         }
      }
#endif
   }

//----------------------------

   enum{
      SS_reserved,
      SS_ACTIVE,
      SS_AUTO_CMD,
      SS_DRAW_CONTACTS,
      SS_STUDIO_ACTIVE,
      SS_STATS,
   };

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      ck
         (SS_ACTIVE, active)
         (SS_AUTO_CMD, auto_command)
         (SS_DRAW_CONTACTS, draw_contacts)
         (SS_STUDIO_ACTIVE, phys_studio_active)
         (SS_STATS, statistics)
         ;
      return true;
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){

      while(ck)
      switch(++ck){
      case SS_ACTIVE:
         ck >> active;
         ed->CheckMenu(this, E_PHYS_ACTIVE, active);
         break;
      case SS_AUTO_CMD: ck >> auto_command; break;
      case SS_STUDIO_ACTIVE: ck >> phys_studio_active;
         ed->CheckMenu(this, E_ACTION_PHYS_STUDIO_TOGGLE, phys_studio_active);
         break;
      case SS_DRAW_CONTACTS: ck >> draw_contacts; ed->CheckMenu(this, E_PHYS_DRAW_CONTACTS, draw_contacts); break;
      case SS_STATS: ck >> statistics; ed->CheckMenu(this, E_PHYS_STATS, statistics); break;
      default: --ck;
      }
      if(active)
         InitPhysicsScene();
      else
         ClosePhysicsScene();
      if(phys_studio_active)
         InitPhysStudio();
      return true;
   }

//----------------------------
};

//----------------------------
//----------------------------

void InitPhysicsStudio(PC_editor editor){

   C_physics_studio *em = new C_physics_studio;
   editor->InstallPlugin(em);
   em->Release();
}

//----------------------------
