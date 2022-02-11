#include "all.h"
#include "common.h"
#include <IPhysics.h>

//----------------------------

#define GRAVITATION_VERSION 7

#define SPEED_SLIDER_MIN  4*100
#define SPEED_SLIDER_MAX  100*100 //in cm

#define MIN_PHYS_FALL_TIME 500
#define MAX_PHYS_FALL_TIME 10000
#define PHYS_IDLE_TIME 500

//----------------------------
                              //editor plugin: Gravitation
class C_edit_Gravitation: public C_editor_item{
   virtual const char *GetName() const{ return "Gravitation"; }

//----------------------------

   enum E_ACTION_GRAVITATION{
      E_GRAV_SELECTED_DOWN = 12000,//fall selected objects
      E_GRAV_SELECTED_DOWN_PHYS, //same as above, but using physics
      //E_GRAV_UNDO_FALL,          // - internal - undo
      E_GRAV_TOGGLE_DIALOG,      //toggle gravitation dialog on/off
      E_GRAV_LINK_TO_BACKGROUND, //link to background
      //E_GRAV_SET_PARENT,         // - internal (undo link)
      E_GRAV_MODIFY_NOTIFY,   //notification from modify plugin
   };

   enum{
      UNDO_PRS,
   };

//----------------------------
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_Properties> e_props;

   enum E_fall_mode{
      FM_GRAVITATION,         //fall down by gravitation
      FM_OBJECT_Z,            //fall by object's local z axis
   } fall_mode;
   float fall_speed;          //meters/sec
   bool link_falled;          //true to link falled object to background
   bool calling_modify;       //set while calling modify plugin, to avoid processing its callback during this call

   enum E_collision_mode{
      CM_PIVOT,
      CM_VERTEX,
   } collision_mode;

   HWND hWndGrav;             //NULL if sheet is not displayed

   C_smart_ptr<IPH_world> phys_world;

   struct S_falling_object: public C_unknown{
      C_smart_ptr<I3D_frame> frm;
      C_smart_ptr<I3D_interpolator> intp;
      C_smart_ptr<IPH_body> phys_body;
      S_vector dest_pos;

      S_vector frm_center;
      int phys_time;
      int phys_idle_time;
      S_falling_object():
         phys_time(0),
         phys_idle_time(0)
      {}
   };                                                       
   C_vector<C_smart_ptr<S_falling_object> > fall_list;

#pragma pack(push,1)
   struct S_grav_undo_struct{
      S_vector pos;
      S_quat rot;
      float scale;
      char name_buf[1];       //frame_name, parent_name
   };
#pragma pack(pop)

//----------------------------

   static BOOL CALLBACK GravCallbackProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){

      bool changed = false;
	   switch(message){
      case WM_INITDIALOG:
         {
            SetWindowLong(hDlg, GWL_USERDATA, lParam);
            //C_edit_Gravitation *eg = (C_edit_Gravitation*)GetWindowLong(hDlg, GWL_USERDATA);

                              //setup speed track-bar
            SendMessage(GetDlgItem(hDlg, IDC_SPEEDSLIDER), TBM_SETRANGEMIN, true, SPEED_SLIDER_MIN);
            SendMessage(GetDlgItem(hDlg, IDC_SPEEDSLIDER), TBM_SETRANGEMAX, true, SPEED_SLIDER_MAX);
            SendMessage(GetDlgItem(hDlg, IDC_SPEEDSLIDER), TBM_SETPOS, true, ((SPEED_SLIDER_MAX-SPEED_SLIDER_MIN)/2));
            SendMessage(GetDlgItem(hDlg, IDC_SPEEDSLIDER), TBM_SETLINESIZE, 0, (SPEED_SLIDER_MAX-SPEED_SLIDER_MIN) / 20);
            SendMessage(GetDlgItem(hDlg, IDC_SPEEDSLIDER), TBM_SETPAGESIZE, 0, (SPEED_SLIDER_MAX-SPEED_SLIDER_MIN) / 5);

            return true;
         }
         break;

      case WM_HSCROLL:
         {
            C_edit_Gravitation *eg = (C_edit_Gravitation*)GetWindowLong(hDlg, GWL_USERDATA);
            dword value = SendMessage(GetDlgItem(hDlg, IDC_SPEEDSLIDER), TBM_GETPOS, (WPARAM) 0, (LPARAM) 0);
            if(value<SPEED_SLIDER_MIN) value = SPEED_SLIDER_MIN;
            if(value>SPEED_SLIDER_MAX) value = SPEED_SLIDER_MAX;
            eg->fall_speed = value / 100.0f;
            SetDlgItemText(hDlg, IDC_SPEEDVALUE, C_fstr("%.2f", eg->fall_speed));
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDC_HIDE:
            {
               C_edit_Gravitation *eg = (C_edit_Gravitation*)GetWindowLong(hDlg, GWL_USERDATA);
               if(HIWORD(wParam) == (WORD)BN_CLICKED)
                  if(eg->hWndGrav)
                     eg->Action(E_GRAV_TOGGLE_DIALOG, NULL);
            }
            break;

         case ((WORD)IDC_LINK):
         case ((WORD)IDC_FALL_Z):
         case ((WORD)IDC_COLM_PIVOT):
         case ((WORD)IDC_COLM_VERTEX):
            if(HIWORD(wParam) == (WORD)BN_CLICKED)
               changed = true;
            break;
         }
         break;
      }
      if(changed){

         C_edit_Gravitation *eg = (C_edit_Gravitation*)GetWindowLong(hDlg, GWL_USERDATA);

         eg->link_falled = (SendDlgItemMessage(hDlg, IDC_LINK, BM_GETCHECK, 0, 0) == BST_CHECKED);
         eg->fall_mode =
            (SendDlgItemMessage(hDlg, IDC_FALL_Z, BM_GETCHECK, 0, 0) == BST_CHECKED) ?
            FM_OBJECT_Z : FM_GRAVITATION;
         eg->collision_mode = (SendDlgItemMessage(hDlg, IDC_COLM_PIVOT, BM_GETCHECK, 0, 0) == BST_CHECKED) ?
            CM_PIVOT :
            CM_VERTEX;

         return true;
      }
      return 0;
   }

//----------------------------

   void SetDialogByParams(){

      if(!hWndGrav)
         return;

      SendDlgItemMessage(hWndGrav, IDC_SPEEDSLIDER, TBM_SETPOS, true, FloatToInt(fall_speed * 100.0f));
      SetDlgItemText(hWndGrav, IDC_SPEEDVALUE, C_fstr("%.2f", fall_speed));
   
      SendDlgItemMessage(hWndGrav, IDC_LINK, BM_SETCHECK, !link_falled ? BST_UNCHECKED : BST_CHECKED, 0);
      SendDlgItemMessage(hWndGrav, IDC_FALL_Z, BM_SETCHECK, (fall_mode==FM_GRAVITATION) ? BST_UNCHECKED : BST_CHECKED, 0);
      SendDlgItemMessage(hWndGrav, IDC_COLM_PIVOT, BM_SETCHECK, (collision_mode==CM_PIVOT) ? BST_CHECKED : BST_UNCHECKED, 0);
      SendDlgItemMessage(hWndGrav, IDC_COLM_VERTEX, BM_SETCHECK, (collision_mode==CM_VERTEX) ? BST_CHECKED : BST_UNCHECKED, 0);
   }

//----------------------------

   void ToggleDialog(){

      if(hWndGrav){
         e_props->RemoveSheet(hWndGrav);
         DestroyWindow(hWndGrav);
         hWndGrav = NULL;
      }else{
         hWndGrav = CreateDialogParam(GetHInstance(), "IDD_GRAVITATION", NULL, GravCallbackProc, (LPARAM)this);
         if(hWndGrav){
            e_props->AddSheet(hWndGrav);
            SetDialogByParams();
            e_props->ShowSheet(hWndGrav);
         }
      }                                                       
   }

//----------------------------

   void StopFalling(){

                              //put all items in fall list to their dest pos
      for(int k = fall_list.size(); k--; ){
         S_falling_object &l = *fall_list[k];
         if(l.intp)
            l.frm->SetPos(l.dest_pos);
      }

      fall_list.clear();
      phys_world = NULL;
   }

//----------------------------

   void SaveUndo(PI3D_frame frm){

      {                 //save modification
         dword flags = E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_LINK | E_MODIFY_FLG_SCALE;
         calling_modify = true;
         e_modify->AddFrameFlags(frm, flags);
         calling_modify = false;
      }

      C_chunk &ck_undo = e_undo->Begin(this, UNDO_PRS, frm);
      ck_undo.Write(frm->GetPos());
      ck_undo.Write(frm->GetRot());
      ck_undo.Write(frm->GetScale());
      ck_undo.Write(frm->GetParent()->GetName());
      e_undo->End();
   }

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      switch(id){
      case UNDO_PRS:
         {
                              //if frame is currently falling, stop it
            for(int k = fall_list.size(); k--; ){
               S_falling_object *l = fall_list[k];
               if(frm==l->frm){
                  if(l->intp)
                     frm->SetPos(l->dest_pos);
                              //remove this falling entry
                  l = fall_list.back(); fall_list.pop_back();
                  break;
               }
            }
            SaveUndo(frm);

            frm->SetPos(ck.ReadVector());
            frm->SetRot(ck.ReadQuaternion());
            frm->SetScale(ck.ReadFloat());
            frm->LinkTo(ed->GetScene()->FindFrame(ck.ReadString()));
         }
         break;
      default: assert(0);
      }
   }

//----------------------------

   virtual void BeforeFree(){
      StopFalling();
   }
//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(!phase)
         StopFalling();
   }

//----------------------------

   virtual void SetActive(bool active){
      StopFalling();
   }

//----------------------------

public:

   C_edit_Gravitation():
      fall_mode(FM_GRAVITATION),      
      collision_mode(CM_VERTEX),
      fall_speed((SPEED_SLIDER_MAX-SPEED_SLIDER_MIN)/200.0f),
      link_falled(false),
      calling_modify(false),
      hWndGrav(NULL)
   {}

   ~C_edit_Gravitation(){
   }
   
//----------------------------

   virtual bool Init(){

      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      if(!e_modify || !e_props || !e_slct || !e_undo)
         return false;

      e_modify->AddNotify(this, E_GRAV_MODIFY_NOTIFY);

      ed->AddShortcut(this, E_GRAV_SELECTED_DOWN, "&Edit\\&Gravitation\\Fall down\tCtrl+G", K_G, SKEY_CTRL);
      ed->AddShortcut(this, E_GRAV_SELECTED_DOWN_PHYS, "&Edit\\&Gravitation\\Fall down\tShift+G", K_G, SKEY_SHIFT);
      ed->AddShortcut(this, E_GRAV_TOGGLE_DIALOG, "&Edit\\&Gravitation\\Properties", K_NOKEY, 0);
      ed->AddShortcut(this, E_GRAV_LINK_TO_BACKGROUND, "&Edit\\&Gravitation\\&Link to ground\tCtrl+L", K_L, SKEY_CTRL);

      return true;                                                     
   }

//----------------------------

   virtual void Close(){ 

      StopFalling();
      if(hWndGrav)
         ToggleDialog();
   }

//----------------------------
   
   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(phys_world)
         phys_world->Tick(time, ed->GetScene());

      for(int i = fall_list.size(); i--; ){
         S_falling_object *l = fall_list[i];
         bool done = false;

         PI3D_interpolator in = l->intp;
         if(in){
                              //check when interpolator is done
            I3D_RESULT ir = in->Tick(time);
            done = (ir == I3D_DONE);
         }else{
            l->phys_time += time;
            if(l->phys_time > MAX_PHYS_FALL_TIME)
               done = true;
            else{
               if(l->phys_time > MIN_PHYS_FALL_TIME){
                                 //physics fall - check when idle
                  PIPH_body body = l->phys_body;
                  const S_vector &lv = body->GetLinearVelocity();
                  const S_vector &av = body->GetAngularVelocity();
                  if(lv.Square() < 1e-2f && av.Square() < 1e-2f){
                     if((l->phys_idle_time += time) > PHYS_IDLE_TIME)
                        done = true;
                  }else
                     l->phys_idle_time = 0;
               }
               {
                              //test collision down
                  I3D_collision_data cd;
                  cd.from = l->frm_center * l->frm->GetMatrix();
                  cd.dir = S_vector(0, -1000.0f, 0);
                  cd.flags = I3DCOL_LINE;// | I3DCOL_RAY;
                  if(!ed->GetScene()->TestCollision(cd))
                     done = true;
               }
            }
         }
         if(done){
            fall_list[i] = fall_list.back(); fall_list.pop_back();
         }else{
                              //update modify
            calling_modify = true;
            e_modify->AddFrameFlags(l->frm, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION);
            calling_modify = false;
         }
      }
   }

//----------------------------

   virtual dword Action(int id, void *context);
                            
   virtual void Render(){}

//----------------------------

   virtual bool LoadState(C_chunk &ck){ 

      dword version(dword(-1));
      ck.Read(&version, sizeof(version));
      if(GRAVITATION_VERSION != version){
         SetDialogByParams();
         return false;       
      }

      ck.Read(&fall_mode, sizeof(fall_mode));
      ck.Read(&collision_mode, sizeof(collision_mode));
      ck.Read(&link_falled, sizeof(link_falled));
      ck.Read(&fall_speed, sizeof(fall_speed));
      SetDialogByParams();
      return true; 
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{
      
      dword version = GRAVITATION_VERSION;
      ck.Write(&version, sizeof(float));

      ck.Write(&fall_mode, sizeof(fall_mode));
      ck.Write(&collision_mode, sizeof(collision_mode));
      ck.Write(&link_falled, sizeof(link_falled));
      ck.Write(&fall_speed, sizeof(fall_speed));
      return true; 
   }
};

//----------------------------------

dword C_edit_Gravitation::Action(int id, void *context){

   switch(id){
         
   case E_GRAV_LINK_TO_BACKGROUND:
      {
         if(!ed->CanModify())
            break;
         const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
         int num_linked = 0;

         for(int i = sel_list.size(); i--; ){

            PI3D_frame frm = sel_list[i];

            I3D_bound_volume frame_bbox;
            frm->ComputeHRBoundVolume(&frame_bbox);

                              //get center of frame, in world coords
            S_vector frame_centre = (frame_bbox.bbox.min + frame_bbox.bbox.max) * .5f;
            frame_centre *= frm->GetMatrix();

            static const S_vector link_dir(0, -1, 0);

                              //don't collide with itself
            bool is_on = frm->IsOn();
            frm->SetOn(false);

                              //find frame we're going link to
            I3D_collision_data cd;
            cd.from = frame_centre;
            cd.dir = link_dir;
            cd.flags = I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_RAY;
            ed->GetScene()->TestCollision(cd);
            PI3D_frame target = cd.GetHitFrm();

            frm->SetOn(is_on);
            if(!target)
               continue;

                              //check if this is part of model, if so, use model instead
            PI3D_frame mod_frm = target;
            while(mod_frm = mod_frm->GetParent(), mod_frm){
               if(mod_frm->GetType()==FRAME_MODEL)
                  break;
            }
            if(mod_frm)
               target = mod_frm;

                              //if already linked to target, skip
            if(target == frm->GetParent())
               continue;
                              
            SaveUndo(frm);

                              //re-link
            frm->LinkTo(target, I3DLINK_UPDMATRIX);

            ++num_linked;
         }
         ed->Message(C_fstr("Linked: %i object(s).", num_linked));
      }
      break;

   case E_GRAV_SELECTED_DOWN:
   case E_GRAV_SELECTED_DOWN_PHYS:
      {
         if(!ed->CanModify())
            break;
         int num_falls = 0;

         const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

         for(int i = sel_list.size(); i--; ){

            PI3D_frame frm = sel_list[i];

            {
                              //check if already in falling list
               for(int k = fall_list.size(); k--; ){
                  S_falling_object *l = fall_list[k];
                  if(frm==l->frm){
                                 //put to destination position
                     if(l->intp)
                        frm->SetPos(l->dest_pos);
                                 //remove this interpolation
                     l = fall_list.back(); fall_list.pop_back();
                     break;
                  }
               }
            }
            const S_vector &w_from = frm->GetWorldPos();

            if(id==E_GRAV_SELECTED_DOWN_PHYS){
               if(!phys_world){
                  phys_world = IPHCreateWorld();
                  if(phys_world){
                     phys_world->SetGravity(S_vector(0, -2.0f, 0));
                     phys_world->Release();
                     ed->GetIGraph()->NullTimer();
                  }
               }
               if(phys_world){
                              //check if there's something below the object
                  I3D_collision_data cd;
                  cd.from = w_from;
                  cd.dir = S_vector(0, -1000.0f, 0);
                  cd.flags = I3DCOL_LINE;// | I3DCOL_RAY;
                  if(ed->GetScene()->TestCollision(cd)){
                     PIPH_body body = phys_world->CreateBody();
                     if(body->SetFrame(frm, IPH_STEFRAME_USE_TRANSFORM | IPH_STEFRAME_HR_VOLUMES)){

                        S_falling_object *obj = new S_falling_object;
                        obj->frm = frm;
                        obj->phys_body = body;

                        obj->frm_center.Zero();
                        I3D_bound_volume bvol;
                        obj->frm->ComputeHRBoundVolume(&bvol);
                        if(bvol.bbox.IsValid())
                           obj->frm_center = bvol.bsphere.pos;


                        fall_list.push_back(obj);
                        obj->Release();
                        SaveUndo(frm);

                        ++num_falls;
                     }
                     body->Release();
                  }
               }
               continue;
            }

                              //determine destination position of our fall
            S_vector w_to;
            const S_matrix &mat = frm->GetMatrix();

                              //get direction
            S_vector dir;

            switch(fall_mode){

            case FM_GRAVITATION:
               dir = S_vector(.0f, -1.0f, .0f);
               break;

            case FM_OBJECT_Z:
               dir = mat(2);
               break;

            default:
               assert(0);
            }
            if(IsAbsMrgZero(dir.Magnitude()))
               continue;
            dir.Normalize();

            PI3D_frame target;
            float col_dist = 0.0f;
            S_vector add_thresh = dir * .001f;
                              //compute destination position
            switch(collision_mode){

            case CM_VERTEX:
               {
                  struct S_hlp{
                     static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                        PI3D_visual vis = I3DCAST_VISUAL(frm);
                        PI3D_mesh_base mb = vis->GetMesh();
                        if(mb){
                           const S_matrix &m = vis->GetMatrix();
                           C_vector<S_vector> &vert_list = *(C_vector<S_vector>*)c;
                           const S_vector *vp = mb->LockVertices();
                           dword vs = mb->GetSizeOfVertex();
                           for(int i=mb->NumVertices(); i--; ){
                              const S_vector &v = *(S_vector*)(((byte*)vp) + vs * i);
                              vert_list.push_back(v * m);
                           }
                           mb->UnlockVertices();
                        }
                        return I3DENUMRET_OK;
                     }
                  };
                              //collect all vertices, in world coordinates
                  C_vector<S_vector> vert_list;
                  if(frm->GetType() == FRAME_VISUAL)
                     S_hlp::cbEnum(frm, (dword)&vert_list);
                  frm->EnumFrames(S_hlp::cbEnum, (dword)&vert_list, ENUMF_VISUAL);

                  if(vert_list.size()){
                  
                     target = NULL;
                     bool hiden = frm->IsOn();
                     frm->SetOn(false);
                              //find closest collision
                     for(int i=vert_list.size(); i--; ){
                        float dist1;

                        const S_vector &from = vert_list[i] + add_thresh;
                        //DebugLine(ed, from, from + dir, 0);

                        I3D_collision_data cd;
                        cd.from = from;
                        cd.dir = dir;
                        cd.flags = I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_RAY;
                        ed->GetScene()->TestCollision(cd);
                        PI3D_frame t1 = cd.GetHitFrm();
                        dist1 = cd.GetHitDistance();
                        if(t1){
                           dist1 += .001f;
                           if(!target){
                              target = t1;
                              col_dist = dist1;
                           }else
                           if(col_dist > dist1){
                              col_dist = dist1;
                              target = t1;
                           }
                        }
                     }
                     frm->SetOn(hiden);
                     break;
                  }
               }
                              //flow... (failcase to pivot mode if object has no vertices)
            case CM_PIVOT:
               {
                              //don't check with itself
                  bool hiden = frm->IsOn();
                  frm->SetOn(false);
                  const S_vector &from = w_from + add_thresh;

                  I3D_collision_data cd;
                  cd.from = from;
                  cd.dir = dir;
                  cd.flags = I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_RAY;
                  ed->GetScene()->TestCollision(cd);
                  target = cd.GetHitFrm();
                  col_dist = cd.GetHitDistance();

                  frm->SetOn(hiden);
                  if(target){
                     col_dist += .001f;
                  }
               }
               break;

            default:
               assert(0);
               target = NULL;
            }

            if(!target)
               continue;

            w_to = w_from + dir * col_dist;

                              //don't link to the same parent
            if(!link_falled || target == frm->GetParent())
               target = NULL;

            SaveUndo(frm);

            {                 //setup falling
               S_falling_object *obj = new S_falling_object;

               obj->frm  = frm;

               if(target){
                  frm->LinkTo(target, I3DLINK_UPDMATRIX);
               }
               const S_matrix &imat = ~frm->GetParent()->GetMatrix();

               
               I3D_anim_pos_tcb poskeys[2];

               const S_vector &from = frm->GetPos();
               S_vector to = w_to * imat;
               int fall_time = FloatToInt((from - to).Magnitude() / fall_speed * 1000);
               obj->dest_pos = to;

               poskeys[0].Clear();
               poskeys[0].time = 0;
               poskeys[0].easy_from = 1.0f;
               poskeys[0].v = from;
               poskeys[1].Clear();
               poskeys[1].time = fall_time;
               poskeys[1].v = to;
               

               PI3D_interpolator intp = ed->GetScene()->GetDriver()->CreateInterpolator();

               intp->SetFrame(frm);
               {
                  PI3D_keyframe_anim anim = (PI3D_keyframe_anim)ed->GetScene()->GetDriver()->CreateAnimation(I3DANIM_KEYFRAME);
                  //anim->SetOptions(I3DANIM_SIMPLE);
                  anim->SetPositionKeys(poskeys, 2); 
                  anim->SetEndTime(fall_time);
                  intp->SetAnimation(anim);
                  anim->Release();
               }
               intp->SetCurrTime(0);

               obj->intp = intp;
               intp->Release();

               fall_list.push_back(obj);
               obj->Release();

               ++num_falls;
            }
         }
         if(num_falls){
            ed->SetModified();
                              //computing collisions might take time,
                              // so reset counter now so that we don't lose ticks
            ed->GetIGraph()->GetTimer(1, 1);
         }

         ed->Message(C_fstr("Gravitation: falling %i frame(s).", num_falls));
      }
      break;

   case E_GRAV_MODIFY_NOTIFY:
      {
         if(calling_modify)
            break;
                              //stop modified frame
         pair<PI3D_frame, dword> *p = (pair<PI3D_frame, dword>*)context;
         for(dword i=fall_list.size(); i--; ){
            if(p->first==fall_list[i]->frm){
               fall_list[i] = fall_list.back(); fall_list.pop_back();
               break;
            }
         }
      }
      break;

   case E_GRAV_TOGGLE_DIALOG:
      ToggleDialog();   
      break;
   }
   return 0;
}

//----------------------------

void CreateGravitation(PC_editor editor){
   C_edit_Gravitation *gr = new C_edit_Gravitation; editor->InstallPlugin(gr); gr->Release();
}

//----------------------------