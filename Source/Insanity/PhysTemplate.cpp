#include "pch.h"
#include <Insanity\PhysTemplate.h>


//----------------------------

void S_phys_template::Save(C_chunk &ck) const{

   ck
      (CK_NUM_BODIES, (int)bodies.size())
      (CK_NUM_JOINTS, (int)joints.size())
      ;
   int i;
   for(i=bodies.size(); i--; ){
      const S_body &b = bodies[i];
      ck <<= CK_BODY;
      {
         ck
            (CT_NAME, b.name)
            (CK_BODY_STATIC, b.is_static)
            (CK_BODY_DENSITY, b.density)
            (CK_BODY_DENSITY_AS_WEIGHT, b.density_as_weight)
            ;
      }
      --ck;
   }
   for(i=joints.size(); i--; ){
      const S_joint &j = joints[i];
      ck <<= CK_JOINT;
      {
         ck
            (CT_NAME, j.name)
            (CT_J_POS, j.pos)
            (CT_J_TYPE, j.type)
            ;
         for(int k=0; k<2; k++){
            if(j.body[k].Size())
               ck(word(CT_J_BODY1+k), j.body[k]);
         }
         ck(CT_J_VALS, j.val);
      }
      --ck;
   }
}

//----------------------------

void S_phys_template::Load(C_chunk &ck){

   Clear();

   while(ck)
   switch(++ck){
   case CK_NUM_BODIES: bodies.reserve(ck.RIntChunk()); break;
   case CK_NUM_JOINTS: joints.reserve(ck.RIntChunk()); break;

   case CK_BODY:
      {
         bodies.push_back(S_body());
         S_body &b = bodies.back();
         while(ck)
         switch(++ck){
         case CT_NAME: ck >> b.name; break;
         case CK_BODY_STATIC: ck >> b.is_static; break;
         case CK_BODY_DENSITY: ck >> b.density; break;
         case CK_BODY_DENSITY_AS_WEIGHT: ck >> b.density_as_weight; break;
         default: assert(0); --ck;
         }
         --ck;
      }
      break;
   case CK_JOINT:
      {
         joints.push_back(S_joint());
         S_joint &j = joints.back();
         while(ck)
         switch(++ck){
         case CT_NAME: ck >> j.name; break;
         case CT_J_BODY1: ck >> j.body[0]; break;
         case CT_J_BODY2: ck >> j.body[1]; break;
         case CT_J_POS: ck >> j.pos; break;
         case CT_J_TYPE: ck >> j.type; break;
            break;
         case CT_J_VALS: ck.RArrayChunk(j.val, Min(sizeof(j.val), (size_t)ck.Size())); break;
         default: assert(0); --ck;
         }
         --ck;
      }
      break;
   default: assert(0); --ck;
   }
}

//----------------------------

static const char *GetRelativeName(const C_str &s){

   for(dword i=s.Size(); i--; ){
      if(s[i]=='.')
         return &s[i+1];
   }
   return s;
}

//----------------------------

bool S_phys_template::InitSimulation(C_vector<C_smart_ptr<IPH_body> > &v_bodies, C_vector<C_smart_ptr<IPH_joint> > &v_joints,
   PIPH_world world, PI3D_scene scn, PI3D_frame frm_root,
   void (*ErrReport)(const char *cp, void *context), void *cb_context) const{

   assert(bool(scn) != bool(frm_root));
   if(scn)
      frm_root = scn->GetPrimarySector();

   v_bodies.clear();
   v_joints.clear();
                              //make bodies
   for(dword i=0; i<bodies.size(); i++){
      const S_body &b = bodies[i];
      PI3D_frame frm;
      if(scn)
         frm = (b.name=="*") ? frm_root : scn->FindFrame(b.name);
      else{
         frm = (b.name=="*") ? frm_root : frm_root->FindChildFrame(GetRelativeName(b.name));
      }
      if(!frm){
         if(ErrReport)
            ErrReport(C_xstr("Phys: can't find body '%'") % b.name, cb_context);
         return false;
      }
                           //debug - ignore static bodies
      PIPH_body body = world->CreateBody();
      float density = b.density;
      dword setf_flags = IPH_STEFRAME_USE_TRANSFORM;

      if(I3DFloatAsInt(density)==0x80000000){
         density = 1000.0f;
         /*
                              //read density from material table
         PI3D_volume vol;
         if(frm->GetType()==FRAME_VOLUME)
            vol = I3DCAST_VOLUME(frm);
         else
            vol = I3DCAST_VOLUME(frm->FindChildFrame(NULL, ENUMF_VOLUME));
         if(vol){
            dword mat_id = vol->GetCollisionMaterial();
            density = tab_materials->ItemF(TAB_F_MAT_DENSITY, mat_id);
         }
         */
      }else
      if(b.density_as_weight)
         setf_flags |= IPH_STEFRAME_DENSITY_AS_WEIGHT;

      if(body->SetFrame(frm, setf_flags, density)){
         v_bodies.push_back(body);
      }else{
         if(ErrReport)
            ErrReport(C_xstr("Phys: can't init body '%' (no mass?)") % frm->GetName(), cb_context);
      }
      body->Release();
   }
   if(v_bodies.size()!=bodies.size())
      return false;

                              //make joints
   for(i=0; i<joints.size(); i++){
      const S_joint &j = joints[i];
      const C_str name = scn ? j.name : GetRelativeName(j.name);
      PI3D_frame frm = scn ? scn->FindFrame(name) : frm_root->FindChildFrame(name);
      if(!frm){
         assert(0);
         return false;
      }
                              //attack to bodies
      PIPH_body body[2];
      for(int bi=2; bi--; ){
         body[bi] = NULL;

         const C_str &n = j.body[bi];
         if(n.Size()){
            int k;
            if(n=="*"){
               for(k=bodies.size(); k--; ){
                  if(v_bodies[k]->GetFrame()==frm_root){
                     body[bi] = v_bodies[k];
                     break;
                  }
               }
            }else{
               const char *bn = GetRelativeName(n);
               for(k=bodies.size(); k--; ){
                  if(!strcmp(v_bodies[k]->GetFrame()->GetOrigName(), bn)){
                     body[bi] = v_bodies[k];
                     break;
                  }
               }
            }
            if(k==-1){
               if(ErrReport)
                  ErrReport(C_xstr("Phys: can't attach joint to body '%'") %n, cb_context);
            }
         }
      }
      dword num_bodies = int(body[0]!=NULL) + int(body[1]!=NULL);
      if(!num_bodies)
         continue;
      if(num_bodies==1 && !world->JointCanAttachToOneBody(j.type))
         continue;
      if(!body[0])
         swap(body[0], body[1]);

      PIPH_joint jnt = world->CreateJoint(j.type);
      if(jnt){

         jnt->SetName(name);
         jnt->Attach(body[0], body[1]);
         if(body[0]){
            const S_matrix &m = frm->GetMatrix();
            jnt->SetAnchor(m(3));
            /*
            S_vector p0 = (m(3)-body[0]->GetFrame()->GetWorldPos()) % S_quat(body[0]->GetFrame()->GetInvMatrix());
            S_vector p1 = m(3);
            if(body[1])
               p1 = (p1-body[1]->GetFrame()->GetWorldPos()) % S_quat(body[1]->GetFrame()->GetInvMatrix());
            jnt->SetLocalAnchor(0, p0);
            jnt->SetLocalAnchor(1, p1);
            /**/

            switch(j.type){
            case IPHJOINTTYPE_BALL:
               {
                  PIPH_joint_ball j = (PIPH_joint_ball)jnt;
                  j->SetNumAxes(3);
                  j->SetAxis(0, m(0));
                  //j->SetAxis(1, m(1));
                  j->SetAxis(2, m(2));
               }
               break;
            case IPHJOINTTYPE_HINGE:
               {
                  PIPH_joint_hinge j = (PIPH_joint_hinge)jnt;
                  j->SetAxis(m(2));
               }
               break;
            case IPHJOINTTYPE_HINGE2:
               {
                  PIPH_joint_hinge2 j = (PIPH_joint_hinge2)jnt;
                  j->SetAxis(0, m(1));
                  j->SetAxis(1, m(2));
               }
               break;
            case IPHJOINTTYPE_SLIDER:
               {
                  PIPH_joint_slider j = (PIPH_joint_slider)jnt;
                  j->SetAxis(m(2));
               }
               break;
            case IPHJOINTTYPE_UNIVERSAL:
               {
                  PIPH_joint_universal j = (PIPH_joint_universal)jnt;
                  j->SetAxis(0, m(0));
                  j->SetAxis(1, m(1));
               }
               break;
            case IPHJOINTTYPE_FIXED:
               ((PIPH_joint_fixed)jnt)->SetFixed();
               break;
            }
         }
                           //apply values
         for(int ii=S_phys_template::VAL_LAST; ii--; ){
            float val = j.val[ii];
                           //do not apply 'default' value
            if(I3DFloatAsInt(val)==0x80000000)
               continue;
            switch(ii){
            case S_phys_template::VAL_STOP_LO0:
            case S_phys_template::VAL_STOP_LO1:
            case S_phys_template::VAL_STOP_LO2:
               if(j.type!=IPHJOINTTYPE_SLIDER)
                  val *= PI/180.0f;
               jnt->SetLoStop(val, ii-S_phys_template::VAL_STOP_LO0);
               break;
            case S_phys_template::VAL_STOP_HI0:
            case S_phys_template::VAL_STOP_HI1:
            case S_phys_template::VAL_STOP_HI2:
               if(j.type!=IPHJOINTTYPE_SLIDER)
                  val *= PI/180.0f;
               jnt->SetHiStop(val, ii-S_phys_template::VAL_STOP_HI0);
               break;
            case S_phys_template::VAL_MAX_FORCE:
               switch(j.type){
               case IPHJOINTTYPE_BALL:
                  jnt->SetMaxForce(val, 0);
                  jnt->SetMaxForce(val, 1);
                  jnt->SetMaxForce(val, 2);
                  break;
               case IPHJOINTTYPE_HINGE2:
                  jnt->SetMaxForce(val, 1);
                  break;
               default:
                  jnt->SetMaxForce(val);
               }
               break;
            case S_phys_template::VAL_FUDGE: jnt->SetFudgeFactor(val); break;
            case S_phys_template::VAL_BOUNCE: jnt->SetBounce(Max(0.0f, Min(1.0f, val))); break;
            case S_phys_template::VAL_CFM: jnt->SetCFM(val); break;
            case S_phys_template::VAL_STOP_ERP: jnt->SetStopERP(Max(0.0f, Min(1.0f, val))); break;
            case S_phys_template::VAL_STOP_CFM: jnt->SetStopCFM(val); break;
            case S_phys_template::VAL_SUSP_ERP: jnt->SetSuspensionERP(val); break;
            case S_phys_template::VAL_SUSP_CFM: jnt->SetSuspensionCFM(val); break;
            }
         }
         v_joints.push_back(jnt);
         jnt->Release();
      }
   }
   return true;
}

//----------------------------
