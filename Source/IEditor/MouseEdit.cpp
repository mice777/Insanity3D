#include "all.h"
#include "common.h"
#include <malloc.h>

//----------------------------

#define STARTUP_ORBIT_DIST 1.0f

//----------------------------

class C_edit_MouseEdit: public C_editor_item_MouseEdit{
   virtual const char *GetName() const{ return "MouseEdit"; }

//----------------------------

   enum{
      E_MOUSE_TOGGLE_MODE,       // - switch between editing modes
      E_MOUSE_SET_CURR_CAMERA,   //(PI3D_frame) - setup current camera used by editor
      E_MOUSE_CANCEL,            // - ESC key to cancel user pick mode
      E_MOUSE_ROT_45_LEFT,       // - rotate selection 45 degrees to the left
      E_MOUSE_ROT_45_RIGHT,      // - rotate selection 45 degrees to the right
      E_MOUSE_Y_ALIGN,       //align selection to polar coordinates
      E_MOUSE_ALIGN_TO_NORMAL,   //align selection to picked frame
      E_MOUSE_ALIGN_PICK,        // - internal - picked frame to alight with
      E_MOUSE_EDIT_SUBOBJECT,    // - toggle subobject edit mode
      E_MOUSE_ALIGN_EXACT,       //align pos/rot/scale with picked object
      E_MOUSE_ALIGN_EXACT_PICK,  //internal - picked frame to alight with

      ACTION_ROT_AXIS_X,
      ACTION_ROT_AXIS_Y,
      ACTION_ROT_AXIS_Z,

      ACTION_ADD_AXIS_X,
      ACTION_ADD_AXIS_Y,
      ACTION_ADD_AXIS_Z,

      ACTION_LOC_SYSTEM,
      ACTION_UNIFORM_SCALE,

      ACTION_SPEED_0,
      ACTION_SPEED_1,
      ACTION_SPEED_2,
      ACTION_SPEED_3,
      ACTION_SPEED_4,
      ACTION_SPEED_5,
      ACTION_SPEED_6,
      ACTION_SPEED_7,
      ACTION_SPEED_8,
      ACTION_SPEED_9,

      ACTION_USE_COLLISIONS,

      ACTION_CAM_NEXT,
      ACTION_CAM_PREV,
      ACTION_CAM_EDIT,
      ACTION_CAM_SELECTED,

      ACTION_GO_TO_SEL_OBJ,
      ACTION_GO_TO_POS,

      ACTION_LINK_SEL_TO,
   };

   enum{
      UNDO_TRANSLATE,
      UNDO_ROTATE,
      UNDO_ROT_TRANS,
      UNDO_SCALE_U,
      UNDO_SCALE_NU,
      UNDO_LINK,
      //UNDO_CAM_MOVE,
   };

//----------------------------

   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   int mx, my;
   enum E_mode{
      MODE_EDIT, MODE_VIEW, MODE_SELECT
   } mode;
   int slct_rect[2];          //used in MODE_SELECT, beginning of selection, in screen coords
   bool mouse_init;
   int rotate_cumulate;
   bool uniform_scale;
   bool loc_system;
   bool collisions;
   int undo_cache_count;

   S_vector orbit_pos;
   bool orbit_ok, orbit_init;

   static const float speeds[10];
   enum{
      AXIS_X = 1, AXIS_Y = 2, AXIS_Z = 4,
   };
   byte rot_axis;
   float speed;

   C_smart_ptr<I3D_camera> edit_cam, curr_cam;
   C_smart_ptr<I3D_interpolator> cam_interp;

   HMENU igraph_menu;         //we're auto-hiding menu of igraph in full-screen mode, thus we keep this here
   bool menu_on;

                              //undo structs:
   struct S_edit_Mouse_undo_cam{
      S_vector pos;
      S_quat rot;
      char cam_name[1];
   };

   enum E_CURSOR_INDEX{
      CURSOR_DEFAULT, CURSOR_TOGGLE, CURSOR_LINK, CURSOR_PICK_SPEC,
      CURSOR_USER, CURSOR_LASSO_IN, CURSOR_LASSO_OUT, CURSOR_INACTIVE,
      CURSOR_LAST
   };
   HCURSOR cursors[CURSOR_LAST];
   int last_set_cursor;
   /*
   static bool SubModelWarning(PI3D_frame frm, const char *op){

                              //check if model's child
      switch(frm->GetType()){
      case FRAME_USER:
      case FRAME_VOLUME:
         return false;
      }
      PI3D_frame prnt=frm;
      while(prnt=prnt->GetParent())
      if(prnt->GetType()==FRAME_MODEL){
         if(!mission.GetModifyFlags(frm)){
                              //show warning
            igraph->FlipToGDI();
            int i=MessageBox((HWND)igraph->GetHWND(), C_fstr("Object %s is part of model.\nDo you really want to %s it?",
               frm->GetName(), op), APP_NAME, MB_YESNO);
            return (i!=IDYES);
         }
         break;
      }
      return false;
   }
   */

   static float GetAngle(float x, float y){

      if(!x) return y>0 ? PI/2.0f : 3*PI/2.0f;
      float val;
      if(x > 0.0f){
         val = atanf(y/x);
         if(val < 0.0f)
            val += (PI*2.0f);
         return val;
      }else{
         val = PI + atanf(-y/-x);
         if(val < 0.0f) val += (PI*2.0f);
         return val;
      }
   }

//----------------------------

   void SetFrameSector(PI3D_frame frm){
                              //set by frame's center point
      I3D_bound_volume bvol;
      frm->ComputeHRBoundVolume(&bvol);
      const I3D_bbox &bbox = bvol.bbox;

      S_vector pos = frm->GetWorldPos();
      if(bbox.IsValid()) pos += bbox.min + (bbox.max-bbox.min)*.5f;
      ed->GetScene()->SetFrameSectorPos(frm, pos);
   }

//----------------------------
   enum E_MOVE_MODE{
      MOVE_XZ,
      MOVE_Y,
      MOVE_ACTIVE,
      MOVE_ACTIVE_WORLD
   };

//----------------------------
// Get translation C_vector.
   S_vector GetTranslateVector(int rx, int ry, E_MOVE_MODE mode, PI3D_frame frm){

      PI3D_camera cam = curr_cam;
      S_vector delta;
      switch(mode){
      case MOVE_XZ:
         {
            const S_vector &cdir = cam->GetMatrix()(2);
            float zx_angle = GetAngle(cdir.z, -cdir.x);
            delta = S_vector(((float)cos(zx_angle)*rx+(float)sin(zx_angle)*ry) * speed / 200.0f,
               0, ((float)sin(zx_angle)*rx - (float)cos(zx_angle)*ry) * speed / 200.0f);
         }
         break;

      case MOVE_Y:
         delta = S_vector(0, -ry * speed / 200.0f, 0);
         break;

      case MOVE_ACTIVE:
      case MOVE_ACTIVE_WORLD:
         {
            delta.Zero();
            float t = (rx-ry) * speed / 200.0f;
            if(rot_axis&AXIS_X) delta.x = t;
            else
            if(rot_axis&AXIS_Y) delta.y = t;
            else
            if(rot_axis&AXIS_Z) delta.z = t;
            if(mode == MOVE_ACTIVE_WORLD)
               break;
            if(frm){
               S_matrix m = frm->GetMatrix();
               float s = 1.0f / m(0).Magnitude();
               m(0) *= s;
               m(1) *= s;
               m(2) *= s;
               delta %= m;
            }
         }
         break;

      default:
         assert(0); delta.Zero();
      }
      return delta;
   }

//----------------------------

   void TranslateObjects(int rx, int ry, E_MOVE_MODE mode){

      if(user_mode.ei){
         S_vector delta = GetTranslateVector(rx, ry, mode, NULL);
         user_mode.ei->Action(user_mode.modes[S_MouseEdit_user_mode::MODE_MOVE_Y].action_id, &delta);
         return;
      }

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      if(!sel_list.size())
         return;

                              //check if we enable undo info
      bool undo_on = undo_reset || !e_undo->IsTopEntry(this, UNDO_TRANSLATE);
      undo_reset = false;

      for(dword ii=0; ii<sel_list.size(); ii++){
         PI3D_frame frm = sel_list[ii];

         bool pos_locked = (e_modify->GetLockFlags(frm)&E_MODIFY_FLG_POSITION);
         if(pos_locked){
            C_fstr msg("Frame '%s' locked, cannot change pos.", (const char*)frm->GetName());
            ed->Message(msg);
            continue;
         }

         dword save_flags = e_modify->GetFrameFlags(frm);

         e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION);

         const S_vector from = frm->GetPos();
         S_vector to = from;

         S_vector delta = GetTranslateVector(rx, ry, mode, frm);

         const S_matrix *m_prnt;
         S_matrix m_prnt_i;
         if(frm->GetParent()){
            m_prnt = &frm->GetParent()->GetMatrix();
            m_prnt_i = *m_prnt;
            m_prnt_i.Invert();
            delta = delta.RotateByMatrix(m_prnt_i);
         }else{
            m_prnt = &I3DGetIdentityMatrix();
         }
         to += delta;

         if(collisions){
            I3D_collision_data cd;
            cd.from = from * (*m_prnt);
            cd.dir = (to * (*m_prnt)) - from;
            cd.flags = I3DCOL_MOVING_GROUP | I3DCOL_SOLVE_SLIDE;
            cd.frm_ignore = frm;
            cd.frm_root = frm;
            if(ed->GetScene()->TestCollision(cd)){
               if(frm->GetParent())
                  to = cd.GetDestination() * m_prnt_i;
            }
         }

         I3D_RESULT ir = frm->SetPos(to);
         if(I3D_SUCCESS(ir)){
            switch(frm->GetType()){
            case FRAME_MODEL:
               {
                  PI3D_frame prnt=frm->GetParent();
                  if(!prnt || prnt->GetType()==FRAME_SECTOR)
                     SetFrameSector(frm);
               }
               break;
            }
                              //save undo info
            if(undo_on){
               C_chunk &ck_undo = e_undo->Begin(this, UNDO_TRANSLATE, frm);
               //ck_undo.Write(frm->GetName());
               ck_undo.Write(from);
               e_undo->End();
            }
            ed->SetModified();
         }else{
                              //failed to change value, restore flags
            if(!(save_flags&E_MODIFY_FLG_POSITION))
               e_modify->RemoveFlags(frm, E_MODIFY_FLG_POSITION);
         }
      }
   }

//----------------------------

   void ScaleObjects(int rx){

      const float scl_delta = 1.0f + (float)rx * speed * .001f;

      if(user_mode.ei){
         user_mode.ei->Action(user_mode.modes[S_MouseEdit_user_mode::MODE_SCALE].action_id, (void*)I3DFloatAsInt(scl_delta));
         return;
      }

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      if(!sel_list.size())
         return;

      bool undo_on = undo_reset || !e_undo->IsTopEntry(this, (uniform_scale ? UNDO_SCALE_U : UNDO_SCALE_NU));
      undo_reset = false;

      for(dword ii=0; ii<sel_list.size(); ii++){
         PI3D_frame frm = sel_list[ii];

         bool scale_locked = (e_modify->GetLockFlags(frm)&E_MODIFY_FLG_SCALE);
         if(scale_locked){
            C_fstr msg("Frame '%s' locked, cannot scale.", (const char*)frm->GetName());
            ed->Message(msg);
            continue;
         }

         //if(SubModelWarning(frm, "scale")) break;

         if(!uniform_scale){
            switch(frm->GetType()){
            case FRAME_VOLUME:
               S_vector scl = I3DCAST_VOLUME(frm)->GetNUScale();
               e_modify->AddFrameFlags(frm, E_MODIFY_FLG_NU_SCALE);

               if(undo_on){
                  C_chunk &ck_undo = e_undo->Begin(this, UNDO_SCALE_NU, frm);
                  //ck_undo.Write(frm->GetName());
                  ck_undo.Write(scl);
                  e_undo->End();
               }

               if(rot_axis&AXIS_X)
                  scl.x = Max(.001f, scl.x*scl_delta);
               if(rot_axis&AXIS_Y)
                  scl.y = Max(.001f, scl.y*scl_delta);
               if(rot_axis&AXIS_Z)
                  scl.z = Max(.001f, scl.z*scl_delta);
               scl.Minimal(S_vector(I3D_MAX_FRAME_SCALE, I3D_MAX_FRAME_SCALE, I3D_MAX_FRAME_SCALE));
               I3DCAST_VOLUME(frm)->SetNUScale(scl);

               ed->SetModified();
               break;
            }
         }else{
            float scl = frm->GetScale();

            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SCALE);
            if(undo_on){
               C_chunk &ck_undo = e_undo->Begin(this, UNDO_SCALE_U, frm);
               //ck_undo.Write(frm->GetName());
               ck_undo.Write(scl);
               e_undo->End();
            }
            scl = Max(.001f, scl*scl_delta);
            scl = Min(scl, I3D_MAX_FRAME_SCALE);
            frm->SetScale(scl);

            ed->SetModified();
         }
      }
   }

//----------------------------
// Align frame to polar coords.
   void AlignFrameToPolar(PI3D_frame frm){

      const S_matrix &fmat = frm->GetMatrix();
      const S_vector &up_dir = fmat(3);
      S_vector front; front.GetNormal(up_dir, S_vector(0, 0, 0), fmat(0));
      S_matrix m_inv = ~frm->GetParent()->GetMatrix();
      frm->SetDir1(front.RotateByMatrix(m_inv), up_dir.RotateByMatrix(m_inv));
   }

//----------------------------
// Align frame to specified axis.
   void AlignFrameToAxis(PI3D_frame frm, const S_vector &align_to, int which_axis){

      const S_matrix &fmat = frm->GetMatrix();
      //const S_vector &up_dir = fmat(3);
      S_vector front; front.GetNormal(align_to, S_vector(0, 0, 0), fmat(!which_axis ? 1 : 0));
      S_matrix m_inv = ~frm->GetParent()->GetMatrix();
      frm->SetDir1(front.RotateByMatrix(m_inv), align_to.RotateByMatrix(m_inv));
   }


//----------------------------

   enum E_ROTATE_MODE{
      RM_ANGLE,               //rotate by specified angle
      RM_POLAR_ALIGN,         //align to polar coordinates
      RM_AXIS_ALIGN,          //align with specified axis
   };

//----------------------------
// Rotate selected objects by specified angle.
   void RotateObjects(E_ROTATE_MODE rm, float angle, const S_vector *axis = NULL,
      int align_axis = 0){

      if(user_mode.ei){
         user_mode.ei->Action(user_mode.modes[S_MouseEdit_user_mode::MODE_ROTATE].action_id, 0);
         return;
      }

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
      if(!sel_list.size())
         return;
      bool single_sel = (sel_list.size()==1);

      dword undo_action = single_sel ? UNDO_ROTATE : UNDO_ROT_TRANS;
      bool undo_on = undo_reset || !e_undo->IsTopEntry(this, undo_action);
      undo_reset = false;

      dword mod_flags = E_MODIFY_FLG_ROTATION;

      S_vector pivot_all;
      pivot_all.Zero();

      if(!single_sel){
         mod_flags |= E_MODIFY_FLG_POSITION;
                              //compute center point
         I3D_bbox bbox_all;      
         bbox_all.Invalidate();

         for(dword i=sel_list.size(); i--; ){
            PI3D_frame frm = sel_list[i];
            pivot_all += frm->GetWorldPos();
         }
         pivot_all /= (float)sel_list.size();
      }
                              //check if rotation is possible due some frames may be locked
                              //because in multiple rotation is center point for that case unclear, we rather skip this rotation.
      for(dword j=sel_list.size(); j--; ){
         PI3D_frame frm = sel_list[j];
         dword locked_flags = e_modify->GetLockFlags(frm);
         if(locked_flags&mod_flags){
            if(locked_flags&E_MODIFY_FLG_ROTATION){
               C_fstr msg("Frame '%s' locked, cannot rotate.", (const char*)frm->GetName());
               ed->Message(msg);
               return;
            }
                              //simply ignore other flags, and treat as single-sel
            mod_flags &= ~E_MODIFY_FLG_ROTATION;
            single_sel = true;
         }
      }

      S_quat q1;              //to be computed only once
      S_matrix mat_pos;


      for(dword ii=0; ii<sel_list.size(); ii++){
         PI3D_frame frm = sel_list[ii];

         e_modify->AddFrameFlags(frm, mod_flags);
         

         S_quat q = frm->GetRot();

                              //save undo info
         if(undo_on){
            C_chunk &ck_undo = e_undo->Begin(this, undo_action, frm);
            ck_undo.Write(q);
            if(undo_action==UNDO_ROT_TRANS)
               ck_undo.Write(frm->GetPos());
            e_undo->End();
         }

         switch(rm){
         case RM_ANGLE:
            {
               if(ii==0){
                  S_vector axis;
                  axis.Zero();
                  if(rot_axis&AXIS_X) axis.x = 1.0f;
                  else
                  if(rot_axis&AXIS_Y) axis.y = 1.0f;
                  else
                  if(rot_axis&AXIS_Z) axis.z = 1.0f;

                  if(loc_system && single_sel){
                                    //rotate by frame's matrix
                     const S_matrix &m = frm->GetLocalMatrix();
                     S_vector axis1 = axis.RotateByMatrix(m);
                     if(!axis1.IsNull()) axis = axis1;
                  }else
                                    //put to local coord system
                  if(frm->GetParent()){
                     S_matrix m = frm->GetParent()->GetMatrix();
                     m.Invert();
                     axis = axis.RotateByMatrix(m);
                  }

                  axis.Normalize();
                  q1.Make(axis, angle);
                  if(!single_sel)
                     mat_pos = S_matrix(q1);
               }

               frm->SetRot(q * q1);

               if(!single_sel){
                  PI3D_frame prnt = frm->GetParent();
                  if(prnt){
                     S_vector pos = frm->GetWorldPos();
                     S_vector delta_pos = pos - pivot_all;
                     delta_pos = delta_pos * mat_pos;
                     S_matrix m = prnt->GetMatrix();
                     m.Invert();
                     frm->SetPos((pivot_all + delta_pos) * m);
                  }
               }
            }
            break;

         case RM_POLAR_ALIGN:
            AlignFrameToPolar(frm);
            break;

         case RM_AXIS_ALIGN:
            AlignFrameToAxis(frm, *axis, align_axis);
            break;

         default:
            assert(0);
         }
         ed->SetModified();
      }
   }

//----------------------------

   bool LinkTo(PI3D_frame link_to, const C_vector<PI3D_frame> &sel_list){

      if(!ed->CanModify())
         return false;

      for(dword i=sel_list.size(); i--; ){
         PI3D_frame frm = sel_list[i];
         PI3D_frame curr_prnt = frm->GetParent();
                              //check if already has this parent
         if(curr_prnt==link_to)
            continue;

                              //check if we're re-linking child of a model
         {
            int i = IDOK;
            PI3D_frame prnt = frm;
            while(prnt=prnt->GetParent(), prnt)
            if(prnt->GetType()==FRAME_MODEL){
               i = MessageBox((HWND)ed->GetIGraph()->GetHWND(), C_fstr("Object %s is part of model.\nDo you really want to re-link it?",
                  (const char*)frm->GetName()), "", MB_YESNOCANCEL);
               break;
            }
            if(i==IDNO) continue;
            if(i==IDCANCEL) break;
         }
         bool add_mod = true;
         bool was_mod = false;

         /*
         int indx = e_modify->Action(E_MODIFY_GET_ITEM_INDEX, frm);
         if(indx!=-1){
            const char *orig_ling = (const char*)e_modify->Action(E_MODIFY_GET_ITEM_LINK, (void*)indx);
         */
            const char *orig_ling = e_modify->GetFrameSavedLink(frm);
            if(orig_ling){
               was_mod = true;
               //if(!strcmp(orig_ling, link_to->GetName())){
               if(link_to->GetName() == orig_ling){
                  add_mod = false;
                              //delete modification
                  e_modify->RemoveFlags(frm, E_MODIFY_FLG_LINK);
               }
            }
         //}
         if(add_mod){
            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_LINK | E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE);
         }

         S_vector save_pos = frm->GetPos();
         S_quat save_rot = frm->GetRot();
         float save_scl = frm->GetScale();

         I3D_RESULT ir = frm->LinkTo(link_to, I3DLINK_UPDMATRIX);
         if(I3D_SUCCESS(ir)){
                              //save undo info
            C_chunk &ck_undo = e_undo->Begin(this, UNDO_LINK, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(curr_prnt->GetName());
            ck_undo.Write(save_pos);
            ck_undo.Write(save_rot);
            ck_undo.Write(save_scl);
            e_undo->End();

            e_slct->FlashFrame(frm);
         }else{
                              //link failed
            if(was_mod){
               e_modify->AddFrameFlags(frm, E_MODIFY_FLG_LINK | E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE);
            }else
            if(add_mod){
               e_modify->RemoveFlags(frm, E_MODIFY_FLG_LINK | E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE);
            }
         }
         ed->SetModified();
      }
      ed->Message(C_fstr("%i object(s) linked to %s", sel_list.size(), (const char*)link_to->GetName()));
      return true;
   }

//----------------------------

   void MoveByMouse(int rx, int ry, int rz, byte mbut, int time, bool orbit_mode){

      MouseInit();
      if(cam_interp)
         return;

      bool upd_pos = false;
      bool upd = false;
      PI3D_camera cam = curr_cam;

      const S_matrix &cmat = cam->GetMatrix();
      S_vector cdir = cmat(2);

      float tsec = time*.001f;

                              //position
      bool allow_dir = true;

      S_vector cpos = cmat(3);
      S_vector new_pos = cpos;
      switch(mbut){
      case 1:                 //forward
         new_pos += cdir * speed * tsec;
         upd_pos = true;
         break;
      case 2:                 //backward
         new_pos -= cdir * speed * tsec;
         upd_pos = true;
         break;
      case 3:                 //move in x/y camera's plane
         if(rx || ry){
            float x =  speed * rx / 200.0f;
            float y = -speed * ry / 200.0f;
            new_pos += cmat(0) * x + cmat(1) * y;
            upd_pos = true;
         }
         allow_dir = false;
         break;
      case 4:
         orbit_mode = true;
         break;
      }
      
      bool upd_dir = false;
      S_vector new_dir = cdir;

      if(allow_dir && orbit_mode){

         allow_dir = false;
         if(!orbit_ok){
            float dist;
            float nr, fr;
            cam->GetRange(nr, fr);
            S_vector cpos1 = cpos + cdir * nr;

            CPI3D_frame frm = NULL;

            {
               I3D_collision_data cd;
               cd.from = cpos1;
               cd.dir = cdir;
               cd.flags = I3DCOL_EXACT_GEOMETRY | I3DCOL_RAY;
               ed->GetScene()->TestCollision(cd);
               frm = cd.GetHitFrm();
               dist = cd.GetHitDistance();
               {
                  cd.flags = I3DCOL_EDIT_FRAMES | I3DCOL_RAY;
                  if(ed->GetScene()->TestCollision(cd)){
                     if(!frm || dist > cd.GetHitDistance()){
                        frm = cd.GetHitFrm();
                        dist = cd.GetHitDistance();
                     }
                  }
               }
            }

            if(frm){
               orbit_pos = cpos1 + cdir*dist;
               orbit_init = true;
            }else{            //deduct point
               float len = orbit_init ? (cpos-orbit_pos).Magnitude() : STARTUP_ORBIT_DIST;
               orbit_pos = cpos + cdir*len;
            }
            orbit_ok = true;
         }
         if(orbit_ok && (rx || ry)){

            S_vector dir;

            S_vector old_dir = new_pos - orbit_pos;
            float sz = old_dir.Magnitude();
            dir = old_dir / sz;
            dir = dir - cmat(0) * (float)rx*PI/500.0f +
               cmat(1) * (float)ry*PI/500.0f;
            dir.Normalize();
            dir *= sz;

            new_dir = -dir;
            upd_dir = true;

            new_pos = orbit_pos + dir;
            upd_pos = true;
         }
      }else
         orbit_ok = false;

      bool make_dir = (allow_dir && (rx || ry));
                              //direction
      if(make_dir){
         new_dir.Zero();

         float x_angle = rx * PI/500.0f;
         new_dir.x = sinf(x_angle);
         new_dir.z = cosf(x_angle);

         float y_angle = -ry * PI/500.0f;
         new_dir.y = sinf(y_angle);
         new_dir.z += cosf(y_angle);

         new_dir = new_dir.RotateByMatrix(cmat);

         {
                           //check if we would turn up-head
            static const S_vector up_dir(0, 1, 0);
            S_vector up_plane;
            up_plane.GetNormal(up_dir, S_vector(0, 0, 0), cmat(0));
            up_plane.Normalize();

            float new_dot = new_dir.Dot(up_plane);
            if(new_dot < PI*.01f){
                              //limit angle, don't go further
               new_dir = cmat(2);
            }
         }

         upd_dir = true;
         upd = true;
      }

      if(upd_dir && (curr_cam == edit_cam || ed->CanModify())){

         if(cam->GetParent()){
                              //convert from world coords to local coords
            S_matrix inv_cmat = cam->GetParent()->GetMatrix();
            inv_cmat.Invert();
            new_dir = new_dir.RotateByMatrix(inv_cmat);
         }
                              //check for lock on non-edit camera
         bool curr_cam_rot_locked = (curr_cam != edit_cam) && (e_modify->GetLockFlags(curr_cam)&E_MODIFY_FLG_ROTATION);
         if(!curr_cam_rot_locked){
            cam->SetDir(new_dir, 0.0f);
         }else{
            C_fstr msg("Camera '%s' locked, cannot rotate.", (const char*)curr_cam->GetName());
            ed->Message(msg);
         }

      }

      if(upd || upd_pos){
         if(upd_pos && (curr_cam == edit_cam || ed->CanModify())){
                              //check camera collisions
            if(collisions){
               float cam_near_range, cfr;
               cam->GetRange(cam_near_range, cfr);

               I3D_collision_data cd;
               cd.from = cpos;
               cd.dir = new_pos-cpos;
               cd.flags = I3DCOL_MOVING_SPHERE | I3DCOL_SOLVE_SLIDE;
               cd.radius = cam_near_range * 1.5f;
               if(ed->GetScene()->TestCollision(cd))
                  new_pos = cd.GetDestination();
            }

            if(cam->GetParent()){
               S_matrix inv_cmat = cam->GetParent()->GetMatrix();
               inv_cmat.Invert();
               new_pos = new_pos * inv_cmat;
            }

                              //check for lock on non-edit camera
            bool curr_cam_pos_locked = (curr_cam != edit_cam) && (e_modify->GetLockFlags(curr_cam)&E_MODIFY_FLG_POSITION);
            if(!curr_cam_pos_locked){
               cam->SetPos(new_pos);
            }else{
               C_fstr msg("Camera '%s' locked, cannot change pos.", (const char*)curr_cam->GetName());
               ed->Message(msg);
            }
         }

         if(curr_cam != edit_cam){
            if(ed->CanModify()){
                              //save modifications of non-edit camera to UNDO buffer
               if(!undo_cache_count){
                  C_chunk &ck_undo = e_undo->Begin(this, UNDO_ROT_TRANS, cam);
                  ck_undo.Write(cam->GetRot());
                  ck_undo.Write(cpos);
                  e_undo->End();
               }
               undo_cache_count = 1000; 
                                 //since this is not editing camera, scene is changed
               e_modify->AddFrameFlags(curr_cam, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION);
               ed->SetModified();
            }else{
               ed->Message("Can't modify scene!");
            }
         }
      }else
         undo_cache_count = Max(0, undo_cache_count-time);
   }

//----------------------------

   void UpdateParams(){
      ed->CheckMenu(this, ACTION_ROT_AXIS_X, rot_axis&AXIS_X);
      ed->CheckMenu(this, ACTION_ROT_AXIS_Y, rot_axis&AXIS_Y);
      ed->CheckMenu(this, ACTION_ROT_AXIS_Z, rot_axis&AXIS_Z);
      ed->CheckMenu(this, ACTION_LOC_SYSTEM, loc_system);
      ed->CheckMenu(this, ACTION_UNIFORM_SCALE, uniform_scale);
      for(int i=10; i--; ){
         ed->CheckMenu(this, ACTION_SPEED_0+i, (speeds[i]==speed));
      }
      ed->CheckMenu(this, ACTION_USE_COLLISIONS, collisions);
   }

//----------------------------

   PI3D_frame PickObject(S_MouseEdit_picked::E_PICK_ACTION pa, S_vector *ppos = NULL,
      S_vector *pdir = NULL, float *pdist = NULL, S_vector *pnorm = NULL){

      PI3D_camera cam = curr_cam;
      S_vector pos, dir;
      ed->GetScene()->UnmapScreenPoint(mx, my, pos, dir);
      float cam_near_range, cfr;
      cam->GetRange(cam_near_range, cfr);
                              //start from near clipping plane
      float near_dist = cam_near_range/cosf(dir.AngleTo(cam->GetWorldDir()));
      pos += dir*near_dist;

      if(ppos) *ppos = pos;
      if(pdir) *pdir = dir;

      if(user_mode.ei)
         pa = S_MouseEdit_picked::PA_DEFAULT;

      switch(pa){
      case S_MouseEdit_picked::PA_VOLUME:
         {
            I3D_collision_data cd;
            cd.from = pos;
            cd.dir = dir*1000.0f;
            cd.flags = I3DCOL_VOLUMES;
            ed->GetScene()->TestCollision(cd);
            return cd.GetHitFrm();
         }
         break;

      case S_MouseEdit_picked::PA_EDIT_ONLY:
         {
            I3D_collision_data cd;
            cd.from = pos;
            cd.dir = dir*1000.0f;
            cd.flags = I3DCOL_EDIT_FRAMES;
            ed->GetScene()->TestCollision(cd);
            return cd.GetHitFrm();
         }
         break;

      default:
         PI3D_frame frm = NULL;
         PI3D_scene scn = ed->GetScene();
         {
            I3D_collision_data cd;
            cd.from = pos;
            cd.dir = dir;
            cd.flags = I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_RAY;

            if(scn->TestCollision(cd)){
               *pdist = cd.GetHitDistance();
               *pnorm = cd.GetHitNormal();
               frm = cd.GetHitFrm();
            }
            {
               cd.flags = I3DCOL_EDIT_FRAMES | I3DCOL_RAY;
               if(scn->TestCollision(cd)){
                  if(!frm || *pdist > cd.GetHitDistance()){
                     frm = cd.GetHitFrm();
                     *pdist = cd.GetHitDistance();
                     *pnorm = cd.GetHitNormal();
                  }
               }
            }
            if(pa==S_MouseEdit_picked::PA_NONPICKABLE){
               cd.flags = I3DCOL_NOMESH_VISUALS | I3DCOL_RAY;
               if(scn->TestCollision(cd)){
                  if(!frm || *pdist > cd.GetHitDistance()){
                     frm = cd.GetHitFrm();
                     *pdist = cd.GetHitDistance();
                     *pnorm = cd.GetHitNormal();
                  }
               }
            }
         }

         if(ed->GetDriver()->GetState(RS_DRAWVOLUMES)){
                              //2nd pass - check volumes
            I3D_collision_data cd;
            cd.from = pos;
            cd.dir = dir;
            cd.flags = I3DCOL_VOLUMES | I3DCOL_RAY;
            if(scn->TestCollision(cd)){
               if(frm && cd.GetHitDistance() < *pdist)
                  frm = NULL;
               if(!frm){
                  *pdist = cd.GetHitDistance();
                  *pnorm = cd.GetHitNormal();
                  frm = cd.GetHitFrm();
               }
            }
         }
         return frm;
      }
   }

   byte last_mouse_but;
   //bool cursor_reset;
   bool undo_reset;           //force adding into undo buffer

                              //double-click detection:
   int last_click_time;
   byte last_click_but;

                              //user-pick mode:
   PC_editor_item ei_user_pick; //plugin to be notified (NULL if not in that mode)
   dword user_pick_action;    //action to be called
   void CancelUserPickMode(){
      if(ei_user_pick){
         ei_user_pick->Release();
         ei_user_pick = NULL;
         //cursor_reset = true;
         last_set_cursor = -1;
      }
   }

                              //subobject-edit mode:
   S_MouseEdit_user_mode user_mode;
   void CancelUserMode(){
      if(user_mode.ei){
         user_mode.ei->SubObjEditCancel();
         user_mode.ei = NULL;
         //cursor_reset = true;
         last_set_cursor = -1;
         LoadCursors();
      }
   }

//----------------------------

   void SelectObjectsInRegion(int l, int t, int r, int b, bool full_in){

      PI3D_driver drv = ed->GetDriver();
      PI3D_scene scene = ed->GetScene();
      PI3D_camera cam = scene->GetActiveCamera();
      if(cam->GetOrthogonal())
         return;
      const S_matrix &cm = cam->GetMatrix();
      const S_vector &cpos = cm(3);

      struct S_hlp: public S_MouseEdit_rectangular_pick{
         C_vector<PI3D_frame> sel;
         PC_editor ed;
         dword enum_flags;
         bool do_vis;
         bool draw_dum;

      //----------------------------

         bool CheckPointCollision(const S_vector &p){

            for(dword i=5; i--; ){
               float d = p.DistanceToPlane(vf[i]);
               if(d >= 0.0f)
                  return false;
            }
            return true;
         }

      //----------------------------

         bool CheckBSphereCollision(const S_matrix &m, const I3D_bsphere &bs1){

            I3D_bsphere bs;
            bs.pos = bs1.pos * m;
            bs.radius = bs1.radius * m(0).Magnitude();
            for(int i=5; i--; ){
               float d = bs.pos.DistanceToPlane(vf[i]);
               if(d > bs.radius)
                  return false;
            }
            return true;
         }

      //----------------------------

         bool CheckBBoxCollision(const S_matrix &m, const I3D_bbox &bb1){

            S_vector contour_points[6];
            dword num_cpts;
            ComputeContour(bb1, m, cpos, contour_points, num_cpts, false);
            if(!num_cpts)
               return true;
            return CheckFrustumIntersection(vf, 4, hull_pts, contour_points, num_cpts, cpos);
         }

      //----------------------------

         bool CheckVertexCollision(const S_matrix &m, PI3D_visual vis){

            PI3D_mesh_base mb = vis->GetMesh();
            if(!mb)
               return false;

            S_matrix m_inv = ~m;
                              //transform planes to local space
            S_plane vf_loc[5];
            for(int i=5; i--; )
               vf_loc[i] = vf[i] * m_inv;

            const void *verts = mb->LockVertices();
            dword vstride = mb->GetSizeOfVertex();
            int numv = mb->NumVertices();
            bool is_in = false;
            if(full_in){
               for(i=numv; i--; ){
                  const S_vector *v = (const S_vector*)(((byte*)verts) + i * vstride);
                  for(int pi=5; pi--; ){
                     float d = v->DistanceToPlane(vf_loc[pi]);
                     if(d >= 0.0f)
                        break;
                  }
                                 //out of frustum, skip
                  if(pi!=-1)
                     break;
               }
               if(i==-1)
                  is_in = true;
            }else{
               S_vector hull_pts_loc[4];
               for(int i=4; i--; )
                  hull_pts_loc[i] = hull_pts[i] * m_inv;

               S_vector cpos_loc = cpos * m_inv;
               dword numf = mb->NumFaces();
               
               C_vector<bool> is_2sided(numf, false);
               for(i=mb->NumFGroups(); i--; ){
                  const I3D_face_group &fg = mb->GetFGroups()[i];
                  PI3D_material mat = ((PI3D_face_group)&fg)->GetMaterial();
                  if(mat->Is2Sided()){
                     assert(fg.base_index<=numf && fg.base_index+fg.num_faces<=numf);
                     fill(is_2sided.begin()+fg.base_index, is_2sided.begin()+fg.base_index+fg.num_faces, true);
                  }
               }

               //CPI3D_triface faces = mb->LockFaces();
               C_buffer<I3D_triface> faces(numf);
               mb->GetFaces(faces.begin());

               for(i=numf; i--; ){
                  const I3D_triface &fc = faces[i];
                  S_vector fv[3] = {
                     *(S_vector*)(((byte*)verts)+fc[0]*vstride),
                     *(S_vector*)(((byte*)verts)+fc[1]*vstride),
                     *(S_vector*)(((byte*)verts)+fc[2]*vstride),
                  };
                              //check if face is looking towards camera
                  S_vector n;
                  n.GetNormal(fv[0], fv[1], fv[2]);
                  float d = n.Dot(fv[0] - cpos_loc);
                  if(d >= 0.0f){
                              //check if this is double-sided face
                     if(!is_2sided[i])
                        continue;
                  }

                  is_in = CheckFrustumIntersection(vf_loc, 4, hull_pts_loc, fv, 3, cpos_loc);
                  if(is_in){
#if defined _DEBUG && 0
                     DebugLine(ed, fv[0]*m, fv[1]*m, 0, 0xffff0000);
                     DebugLine(ed, fv[1]*m, fv[2]*m, 0, 0xff00ff00);
                     DebugLine(ed, fv[2]*m, fv[0]*m, 0, 0xff0000ff);
                     DebugLine(ed, ((fv[0]+fv[1]+fv[2]) / 3.0f)*m, S_vector(0, 0, 0), 0);
#endif
                     break;
                  }
               }
               //mb->UnlockFaces();
            }
            mb->UnlockVertices();
            return is_in;
         }

      //----------------------------

         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword context){

            S_hlp *hp = (S_hlp*)context;
            bool do_vis = hp->do_vis, save_vis = do_vis;

            bool add = false;
            switch(frm->GetType()){
            case FRAME_VISUAL:
            case FRAME_MODEL:
               if(frm->IsOn() && do_vis){
                              //hierarchy check first
                  const I3D_bound_volume *bvol;
                  switch(frm->GetType()){
                  case FRAME_VISUAL: bvol = &I3DCAST_VISUAL(frm)->GetHRBoundVolume(); break;
                  case FRAME_MODEL: bvol = &I3DCAST_MODEL(frm)->GetHRBoundVolume(); break;
                  default: assert(0); bvol = NULL;
                  }
                  const S_matrix &m = frm->GetMatrix();
                  if(hp->CheckBSphereCollision(m, bvol->bsphere) && hp->CheckBBoxCollision(m, bvol->bbox)){

                     switch(frm->GetType()){
                     case FRAME_VISUAL:
                        {
                                       //check if visual is colliding with frustum
                           PI3D_visual vis = I3DCAST_VISUAL(frm);
                           const I3D_bound_volume &bvol = vis->GetBoundVolume();

                           if(hp->CheckBSphereCollision(m, bvol.bsphere)){
                              if(hp->CheckBBoxCollision(m, bvol.bbox)){
                                 add = hp->CheckVertexCollision(m, vis);
                              }
                           }
                        }
                        break;
                     }
                  }else
                     do_vis = false;
               }else
                  do_vis = false;
               break;

               /*
            case FRAME_DUMMY:
               {
                  if(!frm->IsOn())
                     do_vis = false;
                  if(hp->draw_dum){
                     PI3D_dummy dum = I3DCAST_DUMMY(frm);
                     add = hp->CheckBBoxCollision(frm->GetMatrix(), *dum->GetBBox());
                  }
               }
               break;
               */

            case FRAME_LIGHT:
               {
                  PI3D_light l = I3DCAST_LIGHT(frm);
                  const S_matrix &m = frm->GetMatrix();
                  switch(l->GetLightType()){
                  case I3DLIGHT_POINT:
                  case I3DLIGHT_POINTAMBIENT:
                     if(!hp->full_in){
                        I3D_bsphere bs;
                        bs.pos.Zero();
                        float tmp;
                        l->GetRange(tmp, bs.radius);
                        add = hp->CheckBSphereCollision(m, bs);
                        break;
                     }
                              //flow...
                  default:
                     add = hp->CheckPointCollision(m(3));
                  }
               }
               break;

            case FRAME_SOUND:
               {
                  PI3D_sound snd = I3DCAST_SOUND(frm);
                  const S_matrix &m = frm->GetMatrix();
                  switch(snd->GetSoundType()){
                  case I3DSOUND_POINT:
                  case I3DSOUND_POINTAMBIENT:
                     if(!hp->full_in){
                        I3D_bsphere bs;
                        bs.pos.Zero();
                        float tmp;
                        snd->GetRange(tmp, bs.radius);
                        add = hp->CheckBSphereCollision(m, bs);
                        break;
                     }
                              //flow...
                  default:
                     add = hp->CheckPointCollision(m(3));
                  }
               }
               break;

            default:
               add = hp->CheckPointCollision(frm->GetMatrix()(3));
               break;
            }
            if(add)
               hp->sel.push_back(frm);

            hp->do_vis = do_vis;
            frm->EnumFrames(cbEnum, context, hp->enum_flags);
            hp->do_vis = save_vis;
            return I3DENUMRET_SKIPCHILDREN;
         }

      } hlp;
      hlp.full_in = full_in;
      hlp.ed = ed;
      hlp.cpos = cpos;
      hlp.do_vis = true;
      hlp.draw_dum = drv->GetState(RS_DRAWDUMMYS);

                              //construct viewing frustum
      hlp.vf[4].normal = S_normal(-cm(2));
      hlp.vf[4].d = -hlp.vf[4].normal.Dot(cpos);

                              //get four corner directions
      S_vector vdir[4];
      for(int i=0; i<4; i++){
         int x = l, y = t;
         if(i&1)
            x = r;
         if(i&2)
            y = b;
         S_vector pos, dir;
         if(I3D_FAIL(scene->UnmapScreenPoint(x, y, pos, vdir[i])))
            return;
      }
                              //get hull planes and points on hull's edge
      for(i=0; i<4; i++){
         S_plane &pl = hlp.vf[i];
         int i0, i1;
         switch(i){
         case 0: i0 = 2; i1 = 0; break;
         case 1: i0 = 1; i1 = 3; break;
         case 2: i0 = 3; i1 = 2; break;
         case 3: i0 = 0; i1 = 1; break;
         default: assert(0); i0 = -1; i1 = -1;
         }
         pl.normal.GetNormal(cpos, cpos+vdir[i0], cpos+vdir[i1]);
         pl.normal.Normalize();
         pl.d = -pl.normal.Dot(cpos);
         hlp.hull_pts[i] = cpos + vdir[i];
      }
      if(user_mode.ei){
         user_mode.ei->Action(user_mode.modes[S_MouseEdit_user_mode::MODE_SEL_IN_RECT].action_id, &hlp);
      }else{
         hlp.enum_flags = 0;
         if(drv->GetState(RS_DRAWVISUALS)) hlp.enum_flags |= ENUMF_VISUAL | ENUMF_MODEL | ENUMF_DUMMY;
         if(drv->GetState(RS_DRAWLIGHTS)) hlp.enum_flags |= ENUMF_LIGHT;
         if(drv->GetState(RS_DRAWSOUNDS)) hlp.enum_flags |= ENUMF_SOUND;
         if(hlp.draw_dum) hlp.enum_flags |= ENUMF_DUMMY;
         if(drv->GetState(RS_DRAWJOINTS)) hlp.enum_flags |= ENUMF_JOINT;
         if(drv->GetState(RS_DRAWCAMERAS)) hlp.enum_flags |= ENUMF_CAMERA;
         scene->EnumFrames(S_hlp::cbEnum, (dword)&hlp, hlp.enum_flags);

         e_slct->Clear();
         for(i=hlp.sel.size(); i--; ){
            PI3D_frame frm = hlp.sel[i];
            e_slct->AddFrame(frm);
         }
      }
   }

//----------------------------

   void ManageMouseButtons(int mx1, int my1, byte mbut, dword gk, dword skeys, S_MouseEdit_picked::E_PICK_ACTION action = S_MouseEdit_picked::PA_DEFAULT){

      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      if(last_mouse_but != mbut){
         last_mouse_but = mbut;

         PIGraph igraph = ed->GetIGraph();
         bool double_click = false;
         if(last_click_but==last_mouse_but){
            dword curr_time = igraph->ReadTimer();
            double_click = ((curr_time-last_click_time) < GetDoubleClickTime());
            if(action != S_MouseEdit_picked::PA_DEFAULT)
               double_click = false;
         }

         switch(mbut){
         case 0:              //no button
            switch(mode){
            case MODE_SELECT:
               {  
                  mode = MODE_EDIT;
                  SetCapture(NULL);
                              //evaluate selection
                  int mx = igraph->Mouse_x();
                  int my = igraph->Mouse_y();
                  if(mx!=slct_rect[0] && my!=slct_rect[1]){
                     int l = slct_rect[0], t = slct_rect[1], r = mx, b = my;
                     if(l>r) swap(l, r);
                     if(t>b) swap(t, b);
                     SelectObjectsInRegion(l, t, r, b, skeys&SKEY_CTRL);
                  }
               }
               break;
            }
            break;

         case 1:              //LMB
         case 2:              //RMB
         case 4:              //MMB
            {
               S_vector ppos, pdir, pnorm(0, 0, 0);
               float pdist = 0.0f;
               /*
               if((mbut == 2) && action==S_MouseEdit_picked::PA_VOLUME)
                  action = S_MouseEdit_picked::PA_EDIT_ONLY;
               else
               */
               if(double_click)
                  action = S_MouseEdit_picked::PA_NONPICKABLE;
               else
               if(mbut==4)
                  action = S_MouseEdit_picked::PA_VOLUME;

               PI3D_frame sel_frm = PickObject(action, &ppos, &pdir, &pdist, &pnorm);
               switch(action){
               case S_MouseEdit_picked::PA_VOLUME:
                  break;
               default: 
                  if(ei_user_pick)
                     break;
                  if(mbut==1 && sel_frm){
                              //go up to find if it's higher type (model, etc)
                     sel_frm = TrySelectTopModel(sel_frm, !double_click);
                  }
               }
                              //broadcast picking
               S_MouseEdit_picked pi;
               pi.frm_picked = sel_frm;
               pi.screen_x = mx, pi.screen_y = my;
               pi.pick_from = ppos;
               pi.pick_dir = pdir;
               pi.pick_dist = pdist;
               pi.pick_norm = pnorm;
               pi.pick_action = (S_MouseEdit_picked::E_PICK_ACTION)action;

               ed->Broadcast((void(C_editor_item::*)(void*))&C_editor_item::OnPick, &pi);
               action = pi.pick_action;
               sel_frm = pi.frm_picked;

               if(ei_user_pick){
                  if(mbut==2){
                     CancelUserPickMode();
                  }else{
                              //call registered plugin
                     if(!ei_user_pick->Action(user_pick_action, &pi))
                        CancelUserPickMode();
                  }
                  break;
               }
               if(user_mode.ei){
                  dword id;
                  switch(action){
                  case S_MouseEdit_picked::PA_DEFAULT: id = S_MouseEdit_user_mode::MODE_DEFAULT; break;
                  case S_MouseEdit_picked::PA_TOGGLE: id = S_MouseEdit_user_mode::MODE_SELECT; break;
                  case S_MouseEdit_picked::PA_LINK: id = S_MouseEdit_user_mode::MODE_LINK; break;
                  case S_MouseEdit_picked::PA_VOLUME: id = S_MouseEdit_user_mode::MODE_PICK_SPEC; break;
                  case S_MouseEdit_picked::PA_NONPICKABLE: id = S_MouseEdit_user_mode::MODE_DOUBLECLICK; break;
                  default: assert(0); id = (dword)-1;
                  }
                  user_mode.ei->Action(user_mode.modes[id].action_id, &pi);
                  break;
               }
               if(action==S_MouseEdit_picked::PA_NULL)
                  break;

               if(sel_frm){
                  int i;
                  switch(action){
                  case S_MouseEdit_picked::PA_DEFAULT:
                  case S_MouseEdit_picked::PA_VOLUME:
                  case S_MouseEdit_picked::PA_NONPICKABLE:
                     if(sel_frm!=e_slct->GetSingleSel()){
                        e_slct->Clear();
                        e_slct->AddFrame(sel_frm);
                        ed->Message(C_fstr("Object selected: '%s'", (const char*)sel_frm->GetName()));
                     }
                     break;
                  case S_MouseEdit_picked::PA_TOGGLE:
                              //check if object is in selection
                     for(i=sel_list.size(); i--; )
                        if(sel_list[i]==sel_frm) break;
                     if(i==-1){
                        e_slct->AddFrame(sel_frm);
                        ed->Message(C_fstr("Object selected: '%s'", (const char*)sel_frm->GetName()));
                     }else{
                        e_slct->RemoveFrame(sel_frm);
                        ed->Message(C_fstr("Object unselected: '%s'", (const char*)sel_frm->GetName()));
                     }
                     break;
                  case S_MouseEdit_picked::PA_LINK:
                     LinkTo(sel_frm, sel_list);
                     break;
                  }
               }else{
                  switch(action){
                     /*
                  case S_MouseEdit_picked::PA_DEFAULT:
                  case S_MouseEdit_picked::PA_VOLUME:
                  case S_MouseEdit_picked::PA_NONPICKABLE:
                     */
                  default:
                     if(sel_list.size()){
                        e_slct->Clear();
                        ed->Message("Selection cleared");
                     }
                     break;
                  case S_MouseEdit_picked::PA_LINK:
                     LinkTo(ed->GetScene()->GetPrimarySector(), sel_list);
                     break;
                  }
               }
            }
            break;
         }
         if(last_mouse_but){
            if(double_click) last_click_but = 0;
            else{
               last_click_but = last_mouse_but;
               last_click_time = igraph->ReadTimer();
            }
         }
      }
   }

//----------------------------

   int sb_mode_index;         //status-bar index for mode
   int curr_message;

   void SetMessage(int i){

      static const char *messages[] = {
         "Edit mode", "View mode", "Sel. region", "Move Y",
         "Scale", "Rotate", "Link", "Add/Remove",
         "Move active", "Move XZ", "User pick",
      };
      if(curr_message!=i){
         curr_message = i;
         ed->Message(messages[i], sb_mode_index);
      }
   }

   void SetIGMenu(bool b){
      if(menu_on!=b){
         menu_on = b;
         ed->GetIGraph()->SetMenu(b ? igraph_menu : NULL);
      }
   }

   void MouseClose(){
      if(mouse_init){
         ed->GetIGraph()->MouseClose();
         ed->GetIGraph()->SetMousePos(mx, my);
         mouse_init = false;
      }
   }

   void MouseInit(){
      if(!mouse_init){
         ed->GetIGraph()->MouseInit();
         mouse_init = true;
      }
   }

//----------------------------
// Load MouseEdit cursors.
   void LoadCursors(){
                              //pre-load cursors, so that they're ready
      cursors[CURSOR_TOGGLE] = LoadCursor(GetHInstance(), "IDC_CURSOR_CROSS_PLUS");
      cursors[CURSOR_LINK] = LoadCursor(GetHInstance(), "IDC_CURSOR_CROSS_LINK");
      cursors[CURSOR_PICK_SPEC] = LoadCursor(GetHInstance(), "IDC_CURSOR_CROSS_VOLUME");
      cursors[CURSOR_DEFAULT] = LoadCursor(GetHInstance(), "IDC_CURSOR_CROSS");
      cursors[CURSOR_LASSO_IN] = LoadCursor(GetHInstance(), "IDC_CURSOR_LASSO_IN");
      cursors[CURSOR_LASSO_OUT] = LoadCursor(GetHInstance(), "IDC_CURSOR_LASSO_OUT");
      cursors[CURSOR_INACTIVE] = LoadCursor(GetHInstance(), "IDC_CURSOR_HAND");

                              //change default cursor
      //SetClassLong((HWND)ed->GetIGraph()->GetHWND(), GCL_HCURSOR, (long)cursor[CURSOR_DEFAULT]);
      SetCursor(CURSOR_DEFAULT);
   }

   void SetCursor(int id){
      if(last_set_cursor!=id){
         ::SetCursor(cursors[id]);
         SetClassLong((HWND)ed->GetIGraph()->GetHWND(), GCL_HCURSOR, (long)cursors[id]);
         last_set_cursor = id;
      }
   }

//----------------------------

   bool cb_registered;
   bool ignore_shift_keys;
   bool ignore_mouse_click;

   dword I2DAPI cbGraph(dword msg, dword par1, dword par2){
      switch(msg){
      case CM_ACTIVATE:
         if(par1){
                              //when re-activated, forget modifying keys
            ignore_shift_keys = true;
            ignore_mouse_click = true;
         }else{
            MouseClose();
            SetCursor(CURSOR_INACTIVE);
         }
         break;
      }
      return 0;
   }

   static dword I2DAPI cbGraph_Thunk(dword msg, dword par1, dword par2, void *context){
      return ((C_edit_MouseEdit*)context)->cbGraph(msg, par1, par2);
   }

//----------------------------

   virtual void Undo(dword id, PI3D_frame frm, C_chunk &ck){

      if(!ed->CanModify()) return;
      /*
      C_str frm_name = ck.ReadString();
      PI3D_frame frm = ed->GetScene()->FindFrame(frm_name);
      if(!frm){
         ed->Message("Properties(E_PROP_RENAME_FRAME): frame not found", 0, EM_ERROR);
         return;
      }
      */

      switch(id){
      case UNDO_TRANSLATE:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(frm->GetPos());
            e_undo->End();
            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION);

            frm->SetPos(ck.ReadVector());
            switch(frm->GetType()){
            case FRAME_MODEL:
               {
                  PI3D_frame prnt=frm->GetParent();
                  if(!prnt || prnt->GetType()==FRAME_SECTOR)
                     SetFrameSector(frm);
               }
               break;
            }
         }
         break;

      case UNDO_ROTATE:
      case UNDO_ROT_TRANS:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(frm->GetRot());
            if(id==UNDO_ROT_TRANS)
               ck_undo.Write(frm->GetPos());
            e_undo->End();

            dword mf = E_MODIFY_FLG_ROTATION;
            if(id==UNDO_ROT_TRANS)
               mf |= E_MODIFY_FLG_POSITION;
            e_modify->AddFrameFlags(frm, mf);

            frm->SetRot(ck.ReadQuaternion());
            if(id==UNDO_ROT_TRANS)
               frm->SetPos(ck.ReadVector());
         }
         break;

      case UNDO_SCALE_U:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            //ck_undo.Write(frm->GetName());
            ck_undo.Write(frm->GetScale());
            e_undo->End();

            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_SCALE);
            frm->SetScale(ck.ReadFloat());
         }
         break;

      case UNDO_SCALE_NU:
         {
            switch(frm->GetType()){
            case FRAME_VOLUME:
               {
                  PI3D_volume vol = I3DCAST_VOLUME(frm);
                  C_chunk &ck_undo = e_undo->Begin(this, id, frm);
                  //ck_undo.Write(frm->GetName());
                  ck_undo.Write(vol->GetNUScale());
                  e_undo->End();

                  e_modify->AddFrameFlags(frm, E_MODIFY_FLG_NU_SCALE);
                  vol->SetNUScale(ck.ReadVector());
               }
               break;
            default: assert(0);
            }
         }
         break;

      case UNDO_LINK:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            ck_undo.Write(frm->GetParent()->GetName());
            ck_undo.Write(frm->GetPos());
            ck_undo.Write(frm->GetRot());
            ck_undo.Write(frm->GetScale());
            e_undo->End();

            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE | E_MODIFY_FLG_LINK);

            PI3D_frame prnt = ed->GetScene()->FindFrame(ck.ReadString());
                              //failcase
            if(!prnt) prnt = ed->GetScene()->GetPrimarySector();
            frm->LinkTo(prnt);
            frm->SetPos(ck.ReadVector());
            frm->SetRot(ck.ReadQuaternion());
            frm->SetScale(ck.ReadFloat());
         }
         break;

         /*
      case UNDO_CAM_MOVE:
         {
            C_chunk &ck_undo = e_undo->Begin(this, id, frm);
            ck_undo.Write(frm->GetPos());
            ck_undo.Write(frm->GetRot());

            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION);

            frm->SetPos(ck.ReadVector());
            frm->SetRot(ck.ReadQuaternion());

            e_undo->End();
         }
         break;
         */

      default: assert(0);
      }

      e_slct->FlashFrame(frm);
      ed->SetModified();
   }

//----------------------------

   virtual dword GetRotationAxis() const{ return rot_axis; }
   virtual float GetMoveSpeed() const{ return speed; }
   virtual bool IsLocSystem() const{ return loc_system; }
   virtual bool IsUniformScale() const{ return uniform_scale; }

   virtual void SetUserMode(const S_MouseEdit_user_mode *um){

      CancelUserPickMode();
      CancelUserMode();
      if(!um)
         return;
      memcpy(&user_mode, um, sizeof(user_mode));
      if(user_mode.modes[S_MouseEdit_user_mode::MODE_DEFAULT].hcursor){
         cursors[CURSOR_DEFAULT] = (HCURSOR)user_mode.modes[S_MouseEdit_user_mode::MODE_DEFAULT].hcursor;
         //SetClassLong((HWND)ed->GetIGraph()->GetHWND(), GCL_HCURSOR, (long)cursor[CURSOR_DEFAULT]);
         SetCursor(CURSOR_DEFAULT);
      }
      if(user_mode.modes[S_MouseEdit_user_mode::MODE_SELECT].hcursor)
         cursors[CURSOR_TOGGLE] = (HCURSOR)user_mode.modes[S_MouseEdit_user_mode::MODE_SELECT].hcursor;
      if(user_mode.modes[S_MouseEdit_user_mode::MODE_LINK].hcursor)
         cursors[CURSOR_LINK] = (HCURSOR)user_mode.modes[S_MouseEdit_user_mode::MODE_LINK].hcursor;
      if(user_mode.modes[S_MouseEdit_user_mode::MODE_PICK_SPEC].hcursor)
         cursors[CURSOR_PICK_SPEC] = (HCURSOR)user_mode.modes[S_MouseEdit_user_mode::MODE_PICK_SPEC].hcursor;
      last_set_cursor = -1;
   }
   virtual const S_MouseEdit_user_mode *GetUserMode(){
      return user_mode.ei ? &user_mode : NULL;
   }
   virtual void SetUserPick(PC_editor_item ei, int action_id, void *hcursor){

      CancelUserPickMode();
      CancelUserMode();
      if(ei){
         ei_user_pick = ei;
         ei_user_pick->AddRef();
         user_pick_action = action_id;
         cursors[CURSOR_USER] = (hcursor) ? (HCURSOR)hcursor : LoadCursor(GetHInstance(), "IDC_CURSOR_CROSS_USER");
         //cursor_reset = true;
         last_set_cursor = -1;
      }
   }
   virtual PC_editor_item GetUserPick(){ return ei_user_pick; }

   virtual void SetEditMode(){
      mode = MODE_EDIT;
      MouseClose();
   }
   virtual void SetViewMode(){
      mode = MODE_VIEW;
      MouseInit();
   }
   virtual dword GetCurrentMode() const{ return mode; }

   virtual PI3D_camera GetEditCam(){ return edit_cam; }

//----------------------------

   virtual void AfterLoad(){
      curr_cam = ed->GetScene()->GetActiveCamera();
      Action(ACTION_CAM_EDIT, 0);
   }

//----------------------------

   virtual void BeforeFree(){
      cam_interp = NULL;
      curr_cam = edit_cam;
      MouseClose();
      CancelUserMode();
   }

//----------------------------

   virtual void LoadFromMission(C_chunk &ck){

      while(ck)
      switch(++ck){
      case 0x3000:
         while(ck){ 
            switch(++ck){
            case CT_POSITION: edit_cam->SetPos(ck.RVectorChunk()); break;
            case CT_DIRECTION: edit_cam->SetDir(ck.RVectorChunk(), 0.0f); break;
            default: --ck;
            }
         }
         --ck;
         break;
      default:
         --ck;
      }
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==1){
         ck <<= CT_EDITOR_PLUGIN;
         ck.WStringChunk(CT_NAME, GetName());
                           //save camera
         {
            ck <<= 0x3000;
            {
               ck.WVectorChunk(CT_POSITION, edit_cam->GetPos());
               ck.WVectorChunk(CT_DIRECTION, edit_cam->GetDir());
            }
            --ck;
         }
         --ck;
      }
   }

//----------------------------
public:
   C_edit_MouseEdit():
      rotate_cumulate(0),
      undo_cache_count(0),
      uniform_scale(true),
      loc_system(true),
      cb_registered(false),
      ignore_shift_keys(false),
      ignore_mouse_click(false),
      last_mouse_but(0),
      last_set_cursor(-1),
      rot_axis(AXIS_Y),
      last_click_but(0),
      mode(MODE_EDIT),
      orbit_ok(false),
      orbit_init(false),
      mouse_init(false),
      //cursor_reset(false),
      curr_message(-1),
      undo_reset(false),
      collisions(true),
      igraph_menu(NULL),
      menu_on(true),
      ei_user_pick(NULL),
      speed(5.0f)
   {
      memset(&user_mode, 0, sizeof(user_mode));
   }

   ~C_edit_MouseEdit(){
      Close();
   }

//----------------------------

   virtual bool Init(){

      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      if(!e_undo || !e_slct || !e_modify)
         return false;

      mx = ed->GetIGraph()->Mouse_x();
      my = ed->GetIGraph()->Mouse_y();
      ed->AddShortcut(this, E_MOUSE_TOGGLE_MODE, NULL, K_SPACE, 0);

#define MENU_BASE "%10 &Edit\\%20 A&xis\\"
      ed->AddShortcut(this, ACTION_ROT_AXIS_X, MENU_BASE"%0 &X\t(Shift+) X", K_X, 0);
      ed->AddShortcut(this, ACTION_ROT_AXIS_Y, MENU_BASE"%0 &Y\t(Shift+) Y", K_Y, 0);
      ed->AddShortcut(this, ACTION_ROT_AXIS_Z, MENU_BASE"%0 &Z\t(Shift+) Z", K_Z, 0);
      ed->AddShortcut(this, E_MOUSE_Y_ALIGN, MENU_BASE"%11 Ali&gn by Y\tCtrl+P", K_P, SKEY_CTRL);
      ed->AddShortcut(this, E_MOUSE_ALIGN_TO_NORMAL, MENU_BASE"%11 &Align to normal\tShift+N", K_N, SKEY_SHIFT);
      //ed->AddShortcut(this, E_MOUSE_ALIGN_EXACT, MENU_BASE"%11 Align exact\tShift+A", K_A, SKEY_SHIFT);
      
      ed->AddShortcut(this, ACTION_ADD_AXIS_X, NULL, K_X, SKEY_SHIFT);
      ed->AddShortcut(this, ACTION_ADD_AXIS_Y, NULL, K_Y, SKEY_SHIFT);
      ed->AddShortcut(this, ACTION_ADD_AXIS_Z, NULL, K_Z, SKEY_SHIFT);

      ed->AddShortcut(this, ACTION_LOC_SYSTEM, MENU_BASE"%20 %i &Local\t`", K_BACKAPOSTROPH, 0);
      ed->AddShortcut(this, ACTION_UNIFORM_SCALE, MENU_BASE"%20 &Uniform scale\tCtrl+U", K_U, SKEY_CTRL);

      ed->AddShortcut(this, ACTION_LINK_SEL_TO, "%10 &Edit\\%88 %i %a Link &to...", K_NOKEY, 0);
      ed->AddShortcut(this, E_MOUSE_ROT_45_LEFT,  "%10 &Edit\\Rotate 45 degrees\\Left\t,", K_COMMA, 0);
      ed->AddShortcut(this, E_MOUSE_ROT_45_RIGHT, "%10 &Edit\\Rotate 45 degrees\\Right\t.", K_DOT, 0);

      ed->AddShortcut(this, E_MOUSE_EDIT_SUBOBJECT, "%10 &Edit\\Subobject edit\tB", K_B, 0);

                              //edit/fly speed
#define SB "&Edit\\&Speed\\"
      ed->AddShortcut(this, ACTION_SPEED_0, "%10 &Edit\\%90 &Speed\\1. 20 cm\t1", K_1, 0);
      ed->AddShortcut(this, ACTION_SPEED_1, SB"2. 50 cm\t2", K_2, 0);
      ed->AddShortcut(this, ACTION_SPEED_2, SB"3. 1 m\t3", K_3, 0);
      ed->AddShortcut(this, ACTION_SPEED_3, SB"4. 2 m\t4", K_4, 0);
      ed->AddShortcut(this, ACTION_SPEED_4, SB"5. 5 m\t5", K_5, 0);
      ed->AddShortcut(this, ACTION_SPEED_5, SB"6. 10 m\t6", K_6, 0);
      ed->AddShortcut(this, ACTION_SPEED_6, SB"7. 20 m\t7", K_7, 0);
      ed->AddShortcut(this, ACTION_SPEED_7, SB"8. 50 m\t8", K_8, 0);
      ed->AddShortcut(this, ACTION_SPEED_8, SB"9. 100 m\t9", K_9, 0);
      ed->AddShortcut(this, ACTION_SPEED_9, SB"0. 200 m\t0", K_0, 0);

                              //camera control
#define CB "&View\\Camera\\"
      ed->AddShortcut(this, ACTION_USE_COLLISIONS, CB"%80 Collisions\tShift+F7", K_F7, SKEY_SHIFT);
      ed->AddShortcut(this, ACTION_CAM_NEXT, CB"Go to &next\t]", K_RBRACKET, 0);
      ed->AddShortcut(this, ACTION_CAM_PREV, CB"Go to &previous\t[", K_LBRACKET, 0);
      ed->AddShortcut(this, ACTION_CAM_EDIT, CB"Go to &editing\tCtrl+[", K_LBRACKET, SKEY_CTRL);
      ed->AddShortcut(this, ACTION_CAM_SELECTED, CB"Go to &selected\tCtrl+]", K_RBRACKET, SKEY_CTRL);
      ed->AddShortcut(this, ACTION_GO_TO_SEL_OBJ, CB"%i &Find selected object(s)\tCtrl+F", K_F, SKEY_CTRL);
      ed->AddShortcut(this, ACTION_GO_TO_POS, CB"Go to p&osition\tCtrl+A", K_A, SKEY_CTRL);

      ed->AddShortcut(this, E_MOUSE_CANCEL, NULL, K_ESC, 0);
      UpdateParams();

      ed->GetIGraph()->MouseUpdate();
      mx = ed->GetIGraph()->Mouse_x();
      my = ed->GetIGraph()->Mouse_y();

      sb_mode_index = ed->CreateStatusIndex(70);

      edit_cam = I3DCAST_CAMERA(ed->GetScene()->CreateFrame(FRAME_CAMERA));
      edit_cam->SetName("<edit camera>");
      curr_cam = edit_cam;
      edit_cam->Release();
      ed->GetScene()->SetActiveCamera(edit_cam);

      LoadCursors();
      /*
      igraph_menu = (HMENU)igraph->GetMenu();
      if(igraph->GetFlags()&IG_FULLSCREEN)
         SetIGMenu(false);
         */
      ed->GetIGraph()->AddCallback(cbGraph_Thunk, this);
      cb_registered = true;

      return true;
   }

//----------------------------

   virtual void Close(){

      if(ed && cb_registered){
         ed->GetIGraph()->RemoveCallback(cbGraph_Thunk, this);
         cb_registered = false;
      }

      e_undo = NULL;
      e_slct = NULL;
      e_modify = NULL;

      edit_cam = NULL;
      curr_cam = NULL;
      CancelUserPickMode();
      MouseClose();
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(cam_interp){
         if(cam_interp->Tick(time)==I3D_DONE)
            cam_interp = NULL;
      }

      PIGraph igraph = ed->GetIGraph();

      if(GetFocus()!=(HWND)igraph->GetHWND()){
         return;
      }
      if(ignore_mouse_click){
         if(mouse_butt)
            return;
         ignore_mouse_click = false;
      }

      if(!mouse_init){
         mx = igraph->Mouse_x();
         my = igraph->Mouse_y();
         /*
                              //show/hide menu in full-screen mode
         if(igraph->GetFlags()&IG_FULLSCREEN){
            if(my==0) SetIGMenu(true);
            else
            if(menu_on && my>GetSystemMetrics(SM_CYMENU)*2) SetIGMenu(false);
         }
         ed->Message(C_fstr("%i", my));
         */
      }
      if(ignore_shift_keys){
         if(skeys) skeys = 0;
         else 
            ignore_shift_keys = false;
      }

                              //adjust speed by mouse wheel
      if(mouse_rz){
         float k = (float)mouse_rz * .5f;
         if(mouse_rz<0) k *= .666666f;
         speed *= (1.0f + k);
         speed = Max(speeds[0], Min(speeds[9], speed));
         /*
         if(mouse_rz<0){
            speed = Max(speeds[0], speed*.666f);
         }else
         if(mouse_rz>0){
            speed = Min(speeds[9], speed*1.5f);
         }
         */
         ed->Message(C_fstr("Edit speed: %.1f m/sec", speed), false);
      }

      if(ei_user_pick){
         if(mode!=MODE_VIEW){
            MouseClose();
            SetMessage(10);
            SetCursor(CURSOR_USER);
            //cursor_reset = true;

            S_MouseEdit_picked::E_PICK_ACTION pa = S_MouseEdit_picked::PA_DEFAULT;
            switch(skeys){
            case SKEY_CTRL: pa = S_MouseEdit_picked::PA_TOGGLE; break;
            case SKEY_CTRL + SKEY_SHIFT: pa = S_MouseEdit_picked::PA_LINK; break;
            }
            ManageMouseButtons(mouse_rx, mouse_ry, mouse_butt, igraph->ReadKeys(), skeys, pa);
         }else
            MoveByMouse(mouse_rx, mouse_ry, mouse_rz, mouse_butt, time, (skeys==SKEY_CTRL));
      }else
      switch(skeys){

      case SKEY_CTRL | SKEY_SHIFT:  //link
         if(mode!=MODE_VIEW){
            SetMessage(6);
            MouseClose();

            SetCursor(CURSOR_LINK);
            //cursor_reset = true;
            ManageMouseButtons(mouse_rx, mouse_ry, mouse_butt, igraph->ReadKeys(), skeys, S_MouseEdit_picked::PA_LINK);
         }
         break;

      case SKEY_CTRL | SKEY_ALT | SKEY_SHIFT:   //move by active axis
         /*
         if(mode!=MODE_VIEW){
            SetMessage(8);
            MouseClose();
            SetCursor(CURSOR_PICK_SPEC);
            cursor_reset = true;
            ManageMouseButtons(mouse_rx, mouse_ry, mouse_butt, igraph->ReadKeys(), skeys, S_MouseEdit_picked::PA_VOLUME);
         }
         */
         SetMessage(8);
         MouseInit();
         if(mouse_rx || mouse_ry){
            if(ed->CanModify()){
               TranslateObjects(mouse_rx, mouse_ry, loc_system ? MOVE_ACTIVE : MOVE_ACTIVE_WORLD);
            }
         }
         break;

      case SKEY_ALT:          //move in Y
         SetMessage(3);
         MouseInit();
         if(mouse_ry){
            if(ed->CanModify())
               TranslateObjects(mouse_rx, mouse_ry, MOVE_Y);
         }
         break;

      case SKEY_SHIFT:        //move in XZ
         SetMessage(9);
         MouseInit();
         if(mouse_rx || mouse_ry){
            if(ed->CanModify())
               TranslateObjects(mouse_rx, mouse_ry, MOVE_XZ);
         }
         break;

      case SKEY_CTRL | SKEY_ALT:    //scale
         SetMessage(4);
         MouseInit();
         if(mouse_rx){
            if(ed->CanModify())
               ScaleObjects(mouse_rx);
         }
         break;

      case SKEY_SHIFT | SKEY_ALT:   //rotate
         SetMessage(5);
         MouseInit();
         if(mouse_rx){
            if(ed->CanModify())
               RotateObjects(RM_ANGLE, PI/1000.0f * (float)mouse_rx);
         }
         break;

      default:                //select
         undo_reset = true;
                              //manage buttons
         if(mode!=MODE_VIEW)
            MouseClose();
         bool is_ctrl = (skeys==SKEY_CTRL);
         //if(cursor_reset)
         {
            if(!mouse_init){
               int ci = CURSOR_DEFAULT;
               if(mode==MODE_SELECT)
                  ci = is_ctrl ? CURSOR_LASSO_IN : CURSOR_LASSO_OUT;
               else
               if(is_ctrl)
                  ci = CURSOR_TOGGLE;
               SetCursor(ci);
            }
            //cursor_reset = false;
         }

         SetMessage(((mode==MODE_SELECT) || !is_ctrl) ? mode : 7);
         switch(mode){
         case MODE_VIEW:
            MoveByMouse(mouse_rx, mouse_ry, mouse_rz, mouse_butt, time, is_ctrl);
            break;
         case MODE_EDIT:
            if(!(last_mouse_but&1)){
               slct_rect[0] = mx;
               slct_rect[1] = my;
            }
         case MODE_SELECT:
            ManageMouseButtons(mouse_rx, mouse_ry, mouse_butt, igraph->ReadKeys(),
               skeys,
               is_ctrl ? S_MouseEdit_picked::PA_TOGGLE : S_MouseEdit_picked::PA_DEFAULT);
            if(mode!=MODE_SELECT && (mouse_rx || mouse_ry) && (last_mouse_but&1)){
               if(abs(slct_rect[0] - (mx - mouse_rx)) > 10 ||
                  abs(slct_rect[1] - (my - mouse_ry)) > 10){
                  mode = MODE_SELECT;
                  SetCapture((HWND)ed->GetIGraph()->GetHWND());
               }
            }
            break;
         }
      }
   }

//----------------------------

   virtual dword C_edit_MouseEdit::Action(int id, void *context){

      switch(id){

      case E_MOUSE_TOGGLE_MODE:
         switch(mode){
         case MODE_EDIT: SetViewMode(); break;
         case MODE_VIEW: SetEditMode(); break;
         }
         break;

      case E_MOUSE_CANCEL:
         if(ei_user_pick){
            CancelUserPickMode();
            break;
         }
         if(user_mode.ei){
            CancelUserMode();
            break;
         }
         break;

      case E_MOUSE_ROT_45_LEFT:
      case E_MOUSE_ROT_45_RIGHT:
         if(ed->CanModify()){
            RotateObjects(RM_ANGLE, (id==E_MOUSE_ROT_45_LEFT) ? PI*.25f : -PI*.25f);
            ed->Message(C_fstr("Selection rotated 45 deg %s",
               (id==E_MOUSE_ROT_45_LEFT) ? "left" : "right"));
         }
         break;

      case ACTION_ROT_AXIS_X:
      case ACTION_ROT_AXIS_Y:             
      case ACTION_ROT_AXIS_Z:             
      case ACTION_ADD_AXIS_X:
      case ACTION_ADD_AXIS_Y:
      case ACTION_ADD_AXIS_Z:
         {
            switch(id){
            case ACTION_ROT_AXIS_X: rot_axis = AXIS_X; break;
            case ACTION_ROT_AXIS_Y: rot_axis = AXIS_Y; break;
            case ACTION_ROT_AXIS_Z: rot_axis = AXIS_Z; break;
            case ACTION_ADD_AXIS_X: rot_axis ^= AXIS_X; break;
            case ACTION_ADD_AXIS_Y: rot_axis ^= AXIS_Y; break;
            case ACTION_ADD_AXIS_Z: rot_axis ^= AXIS_Z; break;
            default: assert(0);
            }
            ed->Message(C_fstr("%s%s%s axis",
               (rot_axis&AXIS_X) ? "X" : "",
               (rot_axis&AXIS_Y) ? "Y" : "",
               (rot_axis&AXIS_Z) ? "Z" : ""), false);
            ed->CheckMenu(this, ACTION_ROT_AXIS_X, rot_axis&AXIS_X);
            ed->CheckMenu(this, ACTION_ROT_AXIS_Y, rot_axis&AXIS_Y);
            ed->CheckMenu(this, ACTION_ROT_AXIS_Z, rot_axis&AXIS_Z);
         }
         break;

      case ACTION_GO_TO_SEL_OBJ:
         {
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            if(!sel_list.size()){
               ed->Message("Selection is empty!");
               break;
            }
            S_vector center(0, 0, 0), dir(0, 0, 0);
            for(int i=sel_list.size(); i--; ){
               CPI3D_frame frm = sel_list[i];
               switch(frm->GetType()){
               case FRAME_VISUAL:
                  {
                     const I3D_bound_volume &bvol = I3DCAST_CVISUAL(frm)->GetBoundVolume();
                     S_vector p = ((bvol.bbox.min + bvol.bbox.max) * .5f) * frm->GetMatrix();
                     center += p;
                  }
                  break;
               case FRAME_MODEL:
                  {
                     const I3D_bound_volume &bvol = I3DCAST_CMODEL(frm)->GetHRBoundVolume();
                     S_vector p = ((bvol.bbox.min + bvol.bbox.max) * .5f) * frm->GetMatrix();
                     center += p;
                  }
                  break;
               default:
                  center += frm->GetWorldPos();
               }
               const S_vector &fdir = frm->GetWorldDir();
               dir += S_normal(S_vector(fdir.x, 0.0f, fdir.z));
            }
            center /= (float)sel_list.size();
            dir.y = 0.0f;
            dir.Normalize();

            S_vector dest = center;
            dir *= 5.0f;
            dir.y -= 5.0f;
            dir.Invert();
            {
               I3D_collision_data cd(dest, dir, I3DCOL_EXACT_GEOMETRY);
               if(ed->GetScene()->TestCollision(cd)){
                  float d = cd.GetHitDistance();
                  d = Max(.1f, d-.3f);
                  dest += S_normal(dir)*d;
               }else{
                  dest += dir;
               }
            }
            S_quat rot;
            rot.SetDir(center - dest, 0.0f);

            float dist = (curr_cam->GetPos() - dest).Magnitude();
            int speed = 200 + (int)(dist * 4.0f);

            PI3D_keyframe_anim anim = (PI3D_keyframe_anim)ed->GetDriver()->CreateAnimation(I3DANIM_KEYFRAME);
            {
               I3D_anim_pos_tcb pkeys[2];
               memset(pkeys, 0, sizeof(pkeys));
               pkeys[0].v = curr_cam->GetPos();
               pkeys[0].easy_from = 1.0f;
               pkeys[1].v = dest;
               pkeys[1].time = speed;
               anim->SetPositionKeys(pkeys, 2);
            }
            {
               I3D_anim_rot rkeys[2];
               memset(rkeys, 0, sizeof(rkeys));
               const S_quat &curr_rot = curr_cam->GetRot();
               curr_rot.Inverse(rkeys[0].axis, rkeys[0].angle);
               rkeys[0].easy_from = 1.0f;
               rot = ~curr_rot * rot;
               rot.Inverse(rkeys[1].axis, rkeys[1].angle);
               rkeys[1].time = speed;
               anim->SetRotationKeys(rkeys, 2);
            }
            anim->SetEndTime(speed);
            cam_interp = ed->GetDriver()->CreateInterpolator();
            cam_interp->Release();
            cam_interp->SetFrame(curr_cam);
            cam_interp->SetAnimation(anim);
            anim->Release();
         }
         break;

      case ACTION_GO_TO_POS:
         {
                              //ask user where to move camera to
            C_str str;
            if(SelectName(ed->GetIGraph(), NULL, "Change camera position", str,
               "Enter 3 float numbers separated by space or colon:")){
                              //scan numbers
               S_vector pos;
               bool ok = (sscanf(str, "%f %f %f", &pos.x, &pos.y, &pos.z)==3);
               if(!ok)
                  ok = (sscanf(str, "%f , %f , %f", &pos.x, &pos.y, &pos.z)==3);
               if(ok){
                  curr_cam->SetPos(pos);
               }
            }
         }
         break;

      case E_MOUSE_Y_ALIGN:
         if(ed->CanModify())
            RotateObjects(RM_AXIS_ALIGN, 0.0f, &S_vector(0, 1, 0), 1);
         break;

      case E_MOUSE_ALIGN_TO_NORMAL:
         SetUserPick(this, E_MOUSE_ALIGN_PICK, LoadCursor(GetHInstance(), "IDC_CURSOR_ALIGN"));
         break;

      case E_MOUSE_ALIGN_EXACT:
         SetUserPick(this, E_MOUSE_ALIGN_EXACT_PICK, LoadCursor(GetHInstance(), "IDC_CURSOR_ALIGN"));
         break;

      case E_MOUSE_ALIGN_PICK:
         if(ed->CanModify()){
            /*
                              //ask for axis
            struct S_hlp{
               static BOOL CALLBACK cbDlg(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

                  static const dword axis_ctrls[3] = {IDC_ALIGN_X, IDC_ALIGN_Y, IDC_ALIGN_Z};
                  switch(uMsg){
                  case WM_INITDIALOG:
                     CheckDlgButton(hwnd, axis_ctrls[lParam], BST_CHECKED);
                     return 1;
                  case WM_COMMAND:
                     switch(LOWORD(wParam)){
                     case IDOK:
                        {
                           for(int i=3; i--; ){
                              if(IsDlgButtonChecked(hwnd, axis_ctrls[i]))
                                 break;
                           }
                           assert(i!=-1);
                           EndDialog(hwnd, i);
                        }
                        break;
                     case IDCANCEL:
                        EndDialog(hwnd, -1);
                        break;
                     }
                     break;
                  }
                  return 0;
               }
            };
            int i = DialogBoxParam(GetHInstance(), "IDD_ALIGNMODE", (HWND)ed->GetIGraph()->GetHWND(), S_hlp::cbDlg, 0);
            if(i==-1)
               break;
               */
            const S_MouseEdit_picked *pick = (const S_MouseEdit_picked*)context;
            if(!pick->frm_picked)
               return true;
            RotateObjects(RM_AXIS_ALIGN, 0, &pick->pick_norm, 1);
            return true;
         }
         break;

      case E_MOUSE_ALIGN_EXACT_PICK:
         {
            const S_MouseEdit_picked *pick = (const S_MouseEdit_picked*)context;
            if(pick->frm_picked){
               const S_vector &pos = pick->frm_picked->GetWorldPos();
               const S_quat &rot = pick->frm_picked->GetWorldRot();
               float scl = pick->frm_picked->GetWorldScale();
               const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
               for(dword i=sel_list.size(); i--; ){
                  PI3D_frame frm = sel_list[i];
                  S_matrix m_inv = frm->GetParent()->GetInvMatrix();
                  assert(0);
                  /*
                              //save undo
                  {
                     dword slen = frm->GetName().Size();
                     {
                        dword len = sizeof(S_edit_Modify_undo_rot)+slen;
                        S_edit_Modify_undo_rot &ip = *(S_edit_Modify_undo_rot*)alloca(len);
                        ip.num_frames = 1;
                        ip.frm_info[0].pos = frm->GetPos();
                        ip.frm_info[0].rot = frm->GetRot();
                        ip.frm_info[0].use_pos = true;
                        strcpy(ip.frm_info[0].frm_name, frm->GetName());
                        S_edit_Undo_2 eu(e_modify, E_MODIFY_ROT_TRANS_NAMED, &ip, len);
                        e_undo->Action(E_UNDO_SAVE, &eu);
                     }
                     {
                        dword len = sizeof(S_edit_Modify_undo_pos)+slen;
                        S_edit_Modify_undo_scl &ip = *(S_edit_Modify_undo_scl*)alloca(len);
                        ip.num_frames = 1;
                        ip.frm_info[0].scl.x = frm->GetScale();
                        ip.frm_info[0].use_non_uniform = false;
                        strcpy(ip.frm_info[0].frm_name, frm->GetName());
                        S_edit_Undo_2 eu(e_modify, E_MODIFY_SCALE_NAMED, &ip, len);
                        e_undo->Action(E_UNDO_SAVE, &eu);
                     }
                  }
                  */

                  frm->SetPos(pos * m_inv);
                  frm->SetRot(rot * m_inv);
                  frm->SetScale(scl * m_inv(0).Magnitude());
               }
            }
         }
         return true;

      case ACTION_LOC_SYSTEM:
         loc_system = !loc_system;
         ed->CheckMenu(this, id, loc_system);
         ed->Message(C_str(loc_system ? "Local" : "World")+" coordinate system", false);
         break;

      case ACTION_UNIFORM_SCALE:
         uniform_scale = !uniform_scale;
         ed->CheckMenu(this, id, uniform_scale);
         ed->Message(C_str("Uniform scale ") + (uniform_scale ? "on" : "off"), false);
         break;

      case ACTION_SPEED_0:
      case ACTION_SPEED_1:
      case ACTION_SPEED_2:
      case ACTION_SPEED_3:
      case ACTION_SPEED_4:
      case ACTION_SPEED_5:
      case ACTION_SPEED_6:
      case ACTION_SPEED_7:
      case ACTION_SPEED_8:
      case ACTION_SPEED_9:
         {
            for(int i=10; i--; ) if(speeds[i]==speed) break;
            if(i!=-1) ed->CheckMenu(this, ACTION_SPEED_0+i, false);
            speed = speeds[id-ACTION_SPEED_0];
            ed->CheckMenu(this, id, true);
            ed->Message(C_fstr("Edit speed: %.1f m/sec", speed), false);
         }
         break;

      case ACTION_USE_COLLISIONS:
         collisions = !collisions;
         ed->CheckMenu(this, id, collisions);
         ed->Message(C_fstr("Collisions %s", collisions ? "on" : "off"));
         break;

      case ACTION_CAM_NEXT:
      case ACTION_CAM_PREV:
      case ACTION_CAM_EDIT:
      case ACTION_CAM_SELECTED:
      case E_MOUSE_SET_CURR_CAMERA:
         {
            PI3D_camera want_cam = NULL;
            switch(id){
            case ACTION_CAM_NEXT:
            case ACTION_CAM_PREV:
               {
                                    //collect all cameras in scene
                  struct S_hlp{
                     static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                        if(!frm->IsOn())
                           return I3DENUMRET_SKIPCHILDREN;
                        if(frm->GetType()==FRAME_CAMERA){
                           C_vector<PI3D_camera> &cl = *(C_vector<PI3D_camera>*)c;
                           cl.push_back(I3DCAST_CAMERA(frm));
                        }
                        return I3DENUMRET_OK;
                     }
                     static bool pr(PI3D_camera c1, PI3D_camera c2){
                        //return (strcmp(c1->GetName(), c2->GetName()) < 0);
                        return (c1->GetName() < c2->GetName());
                     }
                  };
                  C_vector<PI3D_camera> cl;
                  ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&cl, ENUMF_ALL);
                  cl.push_back(edit_cam);
                  if(cl.size()<=1) break;
                                    //sort by name
                  sort(cl.begin(), cl.end(), S_hlp::pr);
                                    //find our camera
                  for(int i=cl.size(); i--; )
                     if(cl[i]==curr_cam) break;
                                    //go to next
                  i += (id==ACTION_CAM_NEXT ? 1 : -1);
                  i += cl.size();   //don't go to negative!
                  i %= cl.size();
                  want_cam = cl[i];
               }
               break;
            case ACTION_CAM_EDIT: want_cam = edit_cam; break;
            case ACTION_CAM_SELECTED:
               {
                  PI3D_frame frm = e_slct->GetSingleSel();
                  if(!frm || frm->GetType()!=FRAME_CAMERA && curr_cam!=frm) break;
                  want_cam = I3DCAST_CAMERA(frm);
               }
               break;
            case E_MOUSE_SET_CURR_CAMERA:
               {
                  PI3D_frame frm = (PI3D_frame)context;
                  if(frm && frm->GetType()==FRAME_CAMERA){
                     want_cam = I3DCAST_CAMERA(frm);
                  }else
                     ed->Message("MouseEdit(E_MOUSE_SET_CURR_CAMERA): need a camera parameter");
               }
               break;
            }
            if(!want_cam || want_cam==curr_cam)
               break;
                              //copy params
            want_cam->SetFOV(curr_cam->GetFOV());
            float n, f;
            curr_cam->GetRange(n, f);
            want_cam->SetRange(n, f);
            want_cam->SetOrthoScale(curr_cam->GetOrthoScale());
            want_cam->SetOrthogonal(curr_cam->GetOrthogonal());

            ed->GetScene()->SetActiveCamera(curr_cam = want_cam);
            ed->Message(C_fstr("Current camera: '%s'", (const char*)curr_cam->GetName()));
         }
         break;

      case ACTION_LINK_SEL_TO:
         {
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            if(!sel_list.size()){
               ed->Message("Can't link - empty selection.");
               break;
            }
                              //ask user for dest frame
            C_vector<PI3D_frame> all_frames;
            struct S_hlp{
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                  C_vector<PI3D_frame> &all_frames = *(C_vector<PI3D_frame>*)c;
                  all_frames.push_back(frm);
                  return I3DENUMRET_OK;
               }
               static bool cbSortFrames(PI3D_frame f1, PI3D_frame f2){
                  //return (strcmp(f1->GetName(), f2->GetName()) < 0);
                  return (f1->GetName() < f2->GetName());
               }
            };
            ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&all_frames);
            all_frames.push_back(ed->GetScene()->GetPrimarySector());
            all_frames.push_back(ed->GetScene()->GetBackdropSector());
            sort(all_frames.begin(), all_frames.end(), S_hlp::cbSortFrames);
            int i = e_slct->Prompt(all_frames);
            if(!i || !all_frames.size())
               break;
            if(all_frames.size() != 1){
               ed->Message("Can't link to multiple frames.");
               break;
            }
            LinkTo(all_frames.front(), sel_list);
         }
         break;

      case E_MOUSE_EDIT_SUBOBJECT:
         {
            /*
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            if(sel_list.size()!=1){
               ed->Message("Single selection required");
               break;
            }
            */
                              //broadcast validation message
            struct S_hlp{
               bool ok;
               static bool cbEnum(PC_editor_item ei, void *c){
                  S_hlp *hp = (S_hlp*)c;
                  if(ei->SubObjectEdit()){
                     hp->ok = true;
                     return false;
                  }
                  return true;
               }
            } hlp;
            hlp.ok = false;
            ed->EnumPlugins(S_hlp::cbEnum, &hlp);
            if(!hlp.ok){
               ed->Message("Can't enter sub-edit mode");
               break;
            }
         }
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Render(){

      if(mode != MODE_SELECT)
         return;
      PIGraph igraph = ed->GetIGraph();
      assert(mode==MODE_SELECT);
      int mx = igraph->Mouse_x();
      int my = igraph->Mouse_y();
      S_vector rect[4];
      PI3D_camera cam = edit_cam;
      float nr, fr;
      cam->GetRange(nr, fr);
      S_plane cam_ncp;
      cam_ncp.normal = cam->GetWorldDir();
      cam_ncp.d = -cam_ncp.normal.Dot(cam->GetWorldPos() + cam_ncp.normal*nr);

      int i;
      for(i=0; i<4; i++){
         S_vector pos, dir;
         int x = !(i&1) ? slct_rect[0] : mx;
         int y = !(i&2) ? slct_rect[1] : my;
         ed->GetScene()->UnmapScreenPoint(x, y, pos, dir);
                              //get intersection with ncp
         cam_ncp.Intersection(pos, dir, rect[i]);
         rect[i] += dir*(nr*.1f);
      }
      static const word indx[] = {0, 1, 1, 3, 3, 2, 2, 0};
      ed->GetScene()->SetRenderMatrix(I3DGetIdentityMatrix());
      ed->GetScene()->DrawLines(rect, 4, indx, sizeof(indx)/2);
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){
                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version!=2) return false;
                              //read other variables
      ck.Read(&mx, sizeof(mx));
      ck.Read(&my, sizeof(my));
      byte mode1 = 0;
      ck.Read(&mode1, sizeof(mode1));
      if(mode1)
         SetViewMode();
      else
         SetEditMode();
      ck.Read(&collisions, sizeof(byte));
      ck.Read(&speed, sizeof(speed));

      UpdateParams();

      return true;
   }

   virtual bool SaveState(C_chunk &ck) const{
                              //write version
      byte version = 2;
      ck.Write(&version, sizeof(byte));
                              //write other variables
      ck.Write(&mx, sizeof(mx));
      ck.Write(&my, sizeof(my));
      ck.Write(&mode, sizeof(byte));
      ck.Write(&collisions, sizeof(byte));
      ck.Write(&speed, sizeof(speed));
      return true;
   }
};

//----------------------------

const float C_edit_MouseEdit::speeds[10] = { 
   .2f, .5f, 1.0f, 2.0f, 5.f, 10, 20, 50, 100, 200.0f
};

//----------------------------

void CreateMouseEdit(PC_editor ed){
   PC_editor_item ei = new C_edit_MouseEdit;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
