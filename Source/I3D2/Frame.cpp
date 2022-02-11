/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.
   All rights reserved.

   File: Frame.cpp
   Content: Basic frame - base class for all frame classes.
--------------------------------------------------------*/

#include "all.h"
#include "frame.h"
#include "model.h"
#include "visual.h"
#include "volume.h"

//----------------------------

                              //frame class
I3D_frame::I3D_frame(PI3D_driver d):
   ref(1),
   drv(d),
   parent(NULL),
   type(FRAME_NULL),
   enum_mask(0),
   pos(0.0f, 0.0f, 0.0f),
   q_rot(0.0f, 0.0f, 0.0f, 1.0f),
   uniform_scale(1.0f),
   num_hr_vis(0), num_hr_occl(0), num_hr_sct(0), num_hr_jnt(0),
   user_data(0),
   frm_flags(FRMFLAGS_ON | FRMFLAGS_CHANGE_MAT | FRMFLAGS_ROT_QUAT_DIRTY |
      //FRMFLAGS_UPDATE_NEEDED |
      FRMFLAGS_HR_LIGHT_RESET)
{
   m_rot.Identity();
   f_matrix.Identity();
   matrix.Identity();
}

//----------------------------

I3D_frame::~I3D_frame(){

                              //set special mark, so that if functions of inherited classes are called,
                              // they know they're already destroyed
   frm_flags |= FRMFLAGS_DESTRUCTING;

   LinkTo(NULL);
   for(int i=children.size(); i--; )
      children[0]->LinkTo(NULL);
}

//----------------------------

I3D_RESULT I3D_frame::Duplicate(CPI3D_frame frm){

   m_rot = frm->m_rot;
   q_rot = frm->q_rot;
   pos = frm->pos;
   uniform_scale = frm->uniform_scale;

   frm_flags = frm->frm_flags&(FRMFLAGS_ON | FRMFLAGS_ROT_QUAT_DIRTY | FRMFLAGS_SUBMODEL | FRMFLAGS_USER_MASK);
   //frm_flags |= FRMFLAGS_UPDATE_NEEDED;
   //frm_flags |= FRMFLAGS_CHANGE_MAT;
   PropagateDirty();
   user_data = frm->user_data;

   SetMatrixDirty();
   SetName(frm->GetName1());

   return I3D_OK;
}

//----------------------------

static const float idn_rot[3][4] = {
   1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
};

I3D_RESULT I3D_frame::LinkTo(PI3D_frame frm, dword flags){

                              //it's our parent already
   if(frm == parent)
      return I3D_OK;
                              //check if both are constructed on the same scene
   /*
   if(scn != frm->scn)
      return I3DERR_INVALIDPARAM;
      */

                              //check if it's not our child
   if(frm){
      PI3D_frame f1 = frm;
      do{
         if(f1==this)
            return I3DERR_CYCLICLINK;
         f1 = f1->parent;
      }while(f1);

      frm->AddChild1(this);
      frm_flags |= FRMFLAGS_HR_LIGHT_RESET;
   }

   if(flags&I3DLINK_UPDMATRIX){

      bool is_null_rotation = true;     //true if linked parent's matrix has identity 3x3 base
      const S_matrix *mp;

      const S_matrix &m = GetMatrix();

                              //get world position
      S_vector p = m(3);

                              //get world rotation (in form of LOOK-AT and UP direction vectors)
      S_vector rot_z = m(2);
      S_vector rot_y = m(1);

                              //get world scale
      float s = m(0).Magnitude();

                              //note: parent points still to previous parent
      bool was_null_rotation = parent ?
         !memcmp(&parent->GetMatrix(), idn_rot, sizeof(idn_rot)) :
         true;

      if(frm){
         mp = &frm->I3D_frame::GetMatrix();
         if(frm->I3D_frame::GetType1()==FRAME_SECTOR && I3DCAST_SECTOR(frm)->IsPrimary()){
            is_null_rotation = true;
         }else{
                              //check for identity rotation
            is_null_rotation = !memcmp(mp, idn_rot, sizeof(idn_rot));
         }
         if(!is_null_rotation){
            S_matrix m_inv = ~(*mp);

                              //inverse position
            p = p * m_inv;

                              //inverse scale
            s = s * m_inv(0).Magnitude();

                              //inverse rotation
            rot_z = rot_z.RotateByMatrix(m_inv);
            rot_y = rot_y.RotateByMatrix(m_inv);
         }else{
            p -= (*mp)(3);
         }
      }

      I3D_frame::SetPos(p);
      s = Min(s, I3D_MAX_FRAME_SCALE);
      I3D_frame::SetScale(s);
      if(!was_null_rotation || !is_null_rotation){
         I3D_frame::SetDir1(rot_z, rot_y);
      }
      I3D_frame::UpdateMatrices();

      if(parent)
         parent->DeleteChild1(this);
      parent = frm;
   }else{
      if(parent)
         parent->DeleteChild1(this);
      parent = frm;

      PropagateDirty();
      UpdateMatrixInternal();
   }

   return I3D_OK;
}

//----------------------------

bool I3D_frame::AddChild1(PI3D_frame frm){

   {
                              //if we have visual children
                              // affect up-tree boundboxes of all objects
      bool do_update = (frm->num_hr_vis || frm->num_hr_jnt || frm->type==FRAME_VISUAL || frm->type==FRAME_JOINT);
      if(do_update){
         PI3D_frame prnt = this;
         do{
            prnt->SetFrameFlags(prnt->GetFrameFlags() & ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID));
            prnt = prnt->GetParent1();
         }while(prnt);
      }
   }


   if(frm->num_hr_vis || frm->type==FRAME_VISUAL){
                              //find last frame with children, put behind this
      int i;
      for(i=children.size(); i--; ){
         if(children[i]->num_hr_vis || children[i]->type==FRAME_VISUAL)
            break;
      }
      children.insert(children.begin()+(i+1), frm);
   }else{
      children.push_back(frm);
   }

   word num_hr_vis1  = frm->num_hr_vis;
   word num_hr_jnt1  = frm->num_hr_jnt;
   word num_hr_occl1 = frm->num_hr_occl;
   word num_hr_sct1 = frm->num_hr_sct;

   switch(frm->GetType1()){
   case FRAME_CAMERA: I3DCAST_CAMERA(frm)->ResetViewMatrix(); break;
   case FRAME_VISUAL: ++num_hr_vis1; break;
   case FRAME_JOINT: ++num_hr_jnt1; break;
   case FRAME_SECTOR: ++num_hr_sct1; break;
   case FRAME_OCCLUDER: ++num_hr_occl1; break;
   }
   if(num_hr_vis1 || num_hr_occl1 || num_hr_sct1 || num_hr_jnt1){
                              //affect all ancestors
      PI3D_frame prnt = this;
      do{
         prnt->num_hr_jnt = word(prnt->num_hr_jnt + num_hr_jnt1);
         prnt->num_hr_occl = word(prnt->num_hr_occl + num_hr_occl1);
         prnt->num_hr_sct = word(prnt->num_hr_sct + num_hr_sct1);
         if(!prnt->num_hr_vis && num_hr_vis1 && prnt->parent){
                              //reorganise position in parent's vector
            C_vector<PI3D_frame> &pc = prnt->parent->children;
            int fi = FindPointerInArray((void**)&pc.front(), pc.size(), prnt);
            assert(fi!=-1);
            for(int lvi=fi; lvi--; )
               if(pc[lvi]->num_hr_vis || pc[lvi]->type==FRAME_VISUAL)
                  break;
                              //swap this with 1st frame with num_hr_vis==0
            assert(lvi>=-1 && lvi<(int)pc.size()-1);
            swap(pc[lvi+1], pc[fi]);
         }
         prnt->num_hr_vis = word(prnt->num_hr_vis + num_hr_vis1);
         prnt = prnt->parent;
      }while(prnt);
   }

                              //hierarchy boundboxes from us up to root has changed
   PI3D_frame frm1a = this;
   do{
      frm1a->frm_flags &= ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID);
      frm1a = frm1a->parent;
   }while(frm1a);

   return true;
}

//----------------------------

bool I3D_frame::DeleteChild1(PI3D_frame frm){

   {
                              //if we have visual children
                              // affect up-tree boundboxes of all objects
      bool do_update = (frm->num_hr_vis || frm->num_hr_jnt || frm->type==FRAME_VISUAL || frm->type==FRAME_JOINT);
      if(do_update){
         PI3D_frame prnt = this;
         do{
            prnt->SetFrameFlags(prnt->GetFrameFlags() & ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID));
            prnt = prnt->GetParent1();
         }while(prnt);
      }
   }

   int i;
   i = FindPointerInArray((void**)&children[0], children.size(), frm);
   assert(i!=-1);
   if(frm->num_hr_vis || frm->type==FRAME_VISUAL){ 
      children.erase(children.begin() + i);
   }else{
      children[i] = children.back(); children.pop_back();
   }

   word num_hr_vis1  = frm->num_hr_vis;
   word num_hr_jnt1  = frm->num_hr_jnt;
   word num_hr_occl1 = frm->num_hr_occl;
   word num_hr_sct1 = frm->num_hr_sct;

   switch(frm->I3D_frame::GetType1()){
   case FRAME_CAMERA: I3DCAST_CAMERA(frm)->ResetViewMatrix(); break;
   case FRAME_VISUAL: ++num_hr_vis1; break;
   case FRAME_JOINT: ++num_hr_jnt1; break;
   case FRAME_SECTOR: ++num_hr_sct1; break;
   case FRAME_OCCLUDER: ++num_hr_occl1; break;
   }
   if(num_hr_vis1 || num_hr_occl || num_hr_sct1 || num_hr_jnt1){
                              //affect all ancestors
      PI3D_frame prnt = this;
      do{
         prnt->num_hr_jnt = word(prnt->num_hr_jnt - num_hr_jnt1);
         prnt->num_hr_occl = word(prnt->num_hr_occl - num_hr_occl1);
         prnt->num_hr_sct = word(prnt->num_hr_sct - num_hr_sct1);
         prnt->num_hr_vis = word(prnt->num_hr_vis - num_hr_vis1);
         if(num_hr_vis1 && !prnt->num_hr_vis && prnt->parent){
                              //reorganise position in parent's vector
            C_vector<PI3D_frame> &pc = prnt->parent->children;
            int sz = pc.size();
            int fi = FindPointerInArray((void**)&pc.front(), sz, prnt);
            assert(fi!=-1);
            for(int lvi=fi+1; lvi<sz; lvi++)
               if(!pc[lvi]->num_hr_vis && pc[lvi]->type!=FRAME_VISUAL)
                  break;
                              //swap this with 1st frame with num_hr_vis==0
            assert(lvi>0 && lvi<=(int)pc.size());
            swap(pc[lvi-1], pc[fi]);
         }
         prnt = prnt->parent;
      }while(prnt);
   }

   return true;
}

//----------------------------

void I3D_frame::SetOn(bool on){

   bool was_on = IsOn1();

   frm_flags &= ~FRMFLAGS_ON;
   if(on)
      frm_flags |= FRMFLAGS_ON;

   if(was_on != IsOn1()){
                              //visibility changed,
                              // affect hierarchy trees
      frm_flags &= ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID);
                              //if we have visual children
                              // affect up-tree boundboxes of all objects
      bool do_update = (num_hr_vis || num_hr_jnt || type==FRAME_VISUAL || type==FRAME_JOINT);
      if(GetParent1() && do_update){
         PI3D_frame frm = GetParent1();
         do{
            frm->SetFrameFlags(frm->GetFrameFlags() & ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID));
            frm = frm->GetParent1();
         }while(frm);
      }
                              //propagate to all children volumes
      struct S_hlp{
         static void Propagate(PI3D_frame root){
            if(root->GetType1()==FRAME_VOLUME)
               I3DCAST_VOLUME(root)->ResetDynamicVolume();
            const C_vector<PI3D_frame> &chlds = root->GetChildren1();
            if(chlds.size()){
               const PI3D_frame *frms = &chlds.front();
               for(dword i=chlds.size(); i--; )
                  Propagate(*frms++);
            }
         }
      };
      S_hlp::Propagate(this);
   }
}

//----------------------------

void I3D_frame::PropagateDirty(){

   if(!(frm_flags&FRMFLAGS_CHANGE_MAT)){
      frm_flags |= FRMFLAGS_CHANGE_MAT;
      frm_flags &= ~FRMFLAGS_INV_MAT_VALID;
      if(children.size()){
         const PI3D_frame *frms = &children.front();
         for(int i=children.size(); i--; ){
            PI3D_frame frm = *frms++;
            frm->PropagateDirty();
         }
      }
   }
}

//----------------------------

I3D_RESULT I3D_frame::SetPos(const S_vector &v){

   assert(!_isnan(v.x) && !_isnan(v.y) && !_isnan(v.z));
   if(pos==v)
      return I3D_OK;
   pos = v;
   PropagateDirty();
   return I3D_OK;
}

//----------------------------
                              //set rotation matrix given by quaternion
I3D_RESULT I3D_frame::SetRot(const S_quat &q){

   assert(!_isnan(q.s) && !_isnan(q.v.x) && !_isnan(q.v.y) && !_isnan(q.v.z));
                              //check delta
   if(!(frm_flags&FRMFLAGS_ROT_QUAT_DIRTY) && !memcmp(&q, &q_rot, sizeof(S_quat)))
      return I3D_OK;

#ifdef _DEBUG
                              //quaternion should be unit-lenght, check it
   float delta = 1.0f - q.Square();
   if(delta>.01f || delta<-.01f){
      return I3DERR_INVALIDPARAMS;
   }
#endif

   q_rot = q;
   m_rot = S_matrix(q);
   frm_flags &= ~FRMFLAGS_ROT_QUAT_DIRTY;
   PropagateDirty();
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_frame::SetScale(float f){

   assert(!_isnan(f));
                              //we don't support negative scales or too big scales
   if(f < 0.0f || f>I3D_MAX_FRAME_SCALE)
      return I3DERR_INVALIDPARAMS;
   uniform_scale = f;
   PropagateDirty();
   return I3D_OK;
}

//----------------------------
                              //set rotation given by direction
I3D_RESULT I3D_frame::SetDir(const S_vector &dir, float roll){

   assert(!_isnan(dir.x) && !_isnan(dir.y) && !_isnan(dir.z) && !_isnan(roll));

   if(!m_rot.SetDir(dir, roll))
      return I3DERR_INVALIDPARAMS;
   frm_flags |= FRMFLAGS_ROT_QUAT_DIRTY;
   PropagateDirty();
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_frame::SetDir1(const S_vector &dir, const S_vector &up){

   assert(!_isnan(dir.x) && !_isnan(dir.y) && !_isnan(dir.z));
   assert(!_isnan(up.x) && !_isnan(up.y) && !_isnan(up.z));

   if(!m_rot.SetDir(dir, up))
      return I3DERR_INVALIDPARAMS;
   frm_flags |= FRMFLAGS_ROT_QUAT_DIRTY;
   PropagateDirty();
   return I3D_OK;
}

//----------------------------

float I3D_frame::GetWorldScale() const{

   return I3D_frame::GetMatrix()(0).Magnitude();
}

//----------------------------

const S_quat &I3D_frame::GetRot() const{

   if(frm_flags&FRMFLAGS_ROT_QUAT_DIRTY){
      q_rot = S_quat(m_rot);

#ifdef _DEBUG
      float delta = 1.0f - (q_rot.v.Dot(q_rot.v) + q_rot.s*q_rot.s);
      assert(delta<.01f && delta>-.01f);
#endif

      frm_flags &= ~FRMFLAGS_ROT_QUAT_DIRTY;
   }
   return q_rot;
}

//----------------------------

const S_vector &I3D_frame::GetWorldPos() const{ return I3D_frame::GetMatrix()(3); }

//----------------------------

const S_vector &I3D_frame::GetWorldDir() const{ return I3D_frame::GetMatrix()(2); }

//----------------------------

S_quat I3D_frame::GetWorldRot() const{

   S_quat q(GetMatrix());

#ifdef _DEBUG
   float delta = 1.0f - (q.v.Dot(q.v) + q.s*q.s);
   assert(delta<.01f && delta>-.01f);
#endif

   return q;
}

//----------------------------

const char *I3D_frame::GetOrigName() const{

   if(!(frm_flags&FRMFLAGS_SUBMODEL))
      return name;
   //const char *dot = strchr(name, '.');
   //return dot ? &dot[1] : name;
   for(dword i=name.Size(); i--; ){
      if(name[i]=='.'){
         return &name[i+1];
      }
   }
   return name;
}

//----------------------------

void I3D_frame::ComputeHRBoundVolume(PI3D_bound_volume bvol) const{

   int max_num_bs = children.size() + 1;
   I3D_bsphere *bs = (I3D_bsphere*)alloca(sizeof(I3D_bsphere)*max_num_bs);
   dword num_bs = 0;

   if(type==FRAME_VISUAL && IsOn1()){
      PI3D_visual vis = (PI3D_visual)this;
      if(!(vis->vis_flags&VISF_BOUNDS_VALID))
         vis->ComputeBounds();
      *bvol = vis->bound.bound_local;

      bs[num_bs++] = bvol->bsphere;
   }else{
      bvol->bbox.Invalidate();
   }

                              //expand bbox by all childrens' bboxes
   const PI3D_frame *chldp = children.size() ? &children.front() : NULL;
   for(int i = children.size(); i--; ){
      PI3D_frame frm = *chldp++;
#if defined USE_PREFETCH && 0
      if(i){
         Prefetch1(*chldp);
      }
#endif
      I3D_bound_volume bvol_tmp;
      const I3D_bound_volume *bvol_children;

      switch(frm->GetType1()){
      case FRAME_VISUAL:
         if(!frm->IsOn1())
            continue;
         bvol_children = &I3DCAST_VISUAL(frm)->GetHRBoundVolume();
         break;
      case FRAME_MODEL:
         if(!frm->IsOn1())
            continue;
         bvol_children = &I3DCAST_MODEL(frm)->GetHRBoundVolume();
         break;
      case FRAME_DUMMY:
         if(!frm->IsOn1())
            continue;
                              //flow...
      default:
                              //don't bother if frame has no visuals
         if(!frm->num_hr_vis)
            continue;
         frm->ComputeHRBoundVolume(&bvol_tmp);
         bvol_children = &bvol_tmp;
      }

      if(bvol_children->bbox.IsValid()){
         const S_matrix &m = frm->GetLocalMatrix1();
                              //transform bound-box
         for(int j=8; j--; ){
            S_vector v;
            v.x = bvol_children->bbox[j&1].x;
            v.y = bvol_children->bbox[(j&2)/2].y;
            v.z = bvol_children->bbox[(j&4)/4].z;
            v *= m;
            bvol->bbox.min.Minimal(v);
            bvol->bbox.max.Maximal(v);
         }

         {
            I3D_bsphere &bs1 = bs[num_bs++];
                              //transform bound-sphere
            float scale = I3DSqrt(m(0, 0)*m(0, 0) + m(1, 0)*m(1, 0) + m(2, 0)*m(2, 0));
            bs1.radius = bvol_children->bsphere.radius * scale;
            bs1.pos = bvol_children->bsphere.pos;
            bs1.pos *= m;
         }
      }
   }
   if(bvol->bbox.IsValid()){
                              //compute bounding sphere
      S_vector bbox_half_diagonal = (bvol->bbox.max - bvol->bbox.min) * .5f;
      bvol->bsphere.pos = bvol->bbox.min + bbox_half_diagonal;
      bvol->bsphere.radius = bbox_half_diagonal.Magnitude();

                              //don't make it greater than sum of bound spheres
      float max_dist = 0.0f;
      for(i=num_bs; i--; ){
         float dist = (bvol->bsphere.pos - bs[i].pos).Magnitude() + bs[i].radius;
         max_dist = Max(max_dist, dist);
      }
      bvol->bsphere.radius = Min(bvol->bsphere.radius, max_dist);

                              //don't let bound-box be greater than bound-sphere
      for(i=0; i<3; i++){
         bvol->bbox.min[i] = Max(bvol->bbox.min[i], bvol->bsphere.pos[i] - bvol->bsphere.radius);
         bvol->bbox.max[i] = Min(bvol->bbox.max[i], bvol->bsphere.pos[i] + bvol->bsphere.radius);
      }
   }else{
      bvol->bsphere.pos.Zero();
      bvol->bsphere.radius = 0.0f;
   }
}

//----------------------------

void I3D_frame::UpdateMatrixInternal() const{

   SetMatrixDirty();

                              //our hierarchy boundbox is invalid
   frm_flags &= ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID);
                              //if we are visual, or we have visual children
                              //affect up-tree boundboxes of all objects
   bool do_update = (num_hr_vis || num_hr_jnt ||
      type==FRAME_VISUAL || type==FRAME_JOINT);
   if(parent && do_update){
      PI3D_frame frm = parent;
      do{
         frm->frm_flags &= ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID);
         frm = frm->parent;
      }while(frm);
   }
}

//----------------------------
                              //enumeration

static bool NameMatch(const char *s, const char *cp){

   if(!*s || !*cp){
      if(*s == '*')
         return true;
      return (*s==*cp);
   }

   for(const char *cp1 = s; ; ++cp1, ++cp){
      switch(*cp1){
      case 0:
         return !(*cp);

      case '*':
         return true;

      case '?':
         if(!*cp)
            return false;
         break;

      default:
         if(*cp != *cp1)
            return false;
      }
   }
}

//----------------------------

                              //version without name checking
static I3DENUMRET EnumRecursiveProc(const C_vector<PI3D_frame> &root, I3D_ENUMPROC *proc, dword user, dword flags){

   for(dword i=0; i<root.size(); ){
      PI3D_frame frm = root[i];

      if(frm->GetEnumMask()&flags){
         I3DENUMRET er = proc(frm, user);
         switch(er){
         case I3DENUMRET_SKIPCHILDREN:
            if(i>=root.size())
               return I3DENUMRET_OK;
            if(frm!=root[i]){
                              //try to find current frame in array, and continue after that
                              // otherwise continue from current position
               int ii = FindPointerInArray((void**)&root.front(), root.size(), frm);
               if(ii!=-1)
                  i = ii + 1;
               continue;
            }
            goto next;

         case I3DENUMRET_CANCEL:
            return I3DENUMRET_CANCEL;

         default:
            if(i>=root.size())
               return I3DENUMRET_OK;
            if(frm!=root[i]){
                              //try to find current frame in array, and continue after that
                              // otherwise continue from current position
               int ii = FindPointerInArray((void**)&root.front(), root.size(), frm);
               if(ii!=-1)
                  i = ii + 1;
               continue;
            }
         }
      }
                              //enum all children, if any
      if(frm->NumChildren1()){
         if(EnumRecursiveProc(frm->GetChildren1(), proc, user, flags)==I3DENUMRET_CANCEL)
            return I3DENUMRET_CANCEL;
      }
next:
      i++;
   }
   return I3DENUMRET_OK;
}

//----------------------------
                              //version with name checking
static I3DENUMRET EnumRecursiveProc(const C_vector<PI3D_frame> &root, I3D_ENUMPROC *proc, dword user, dword flags,
   const char *mask){

   for(dword i=0; i<root.size(); ){
      PI3D_frame frm = root[i];

      if(frm->GetEnumMask()&flags){
         const C_str &str_name = frm->GetName1();
         const char *fname = str_name;
         if((flags&ENUMF_MODEL_ORIG) && (frm->GetFrameFlags()&FRMFLAGS_SUBMODEL)){
                              //search from beginning (there may be more than one dot in the name)
            for(dword i=str_name.Size(); i--; ){
            //for(dword i=0; i<str_name.Size(); i++){
               if(str_name[i]=='.'){
                  fname += i+1;
                  break;
               }
            }
         }
         bool do_this = (flags&ENUMF_WILDMASK) ?
            NameMatch(mask, fname) :
            !strcmp(fname, mask);

         if(do_this){
            I3DENUMRET er=proc(frm, user);
            switch(er){
            case I3DENUMRET_SKIPCHILDREN:
               if(i>=root.size())
                  return I3DENUMRET_OK;
               if(frm!=root[i]){
                              //try to find current frame in array, and continue after that
                              // otherwise continue from current position
                  int ii = FindPointerInArray((void**)&root.front(), root.size(), frm);
                  if(ii!=-1)
                     i = ii + 1;
                  continue;
               }
               goto next;

            case I3DENUMRET_CANCEL:
               return I3DENUMRET_CANCEL;

            default:
               if(i>=root.size()){
                  return I3DENUMRET_OK;
               }
               if(frm!=root[i]){
                              //try to find current frame in array, and continue after that
                              // otherwise continue from current position
                  int ii = FindPointerInArray((void**)&root.front(), root.size(), frm);
                  if(ii!=-1)
                     i = ii + 1;
                  continue;
               }
            }
         }
      }
                              //enum all children, if any
      if(frm->NumChildren1()){
         if(EnumRecursiveProc(frm->GetChildren1(), proc, user, flags, mask)==I3DENUMRET_CANCEL){
            return I3DENUMRET_CANCEL;
         }
      }
next:
      i++;
   }
   return I3DENUMRET_OK;
}

//----------------------------

I3D_RESULT I3D_frame::EnumFrames(I3D_ENUMPROC *proc, dword user, dword flags, const char *mask) const{

   if(frm_flags&FRMFLAGS_SUBMODEL)
      flags |= ENUMF_MODEL_ORIG;

   I3DENUMRET er;
   if(!mask){
      er = EnumRecursiveProc(GetChildren1(), proc, user, flags);
   }else{
      er = EnumRecursiveProc(GetChildren1(), proc, user, flags, mask);
   }
   if(er==I3DENUMRET_CANCEL) return I3DERR_CANCELED;
   return I3D_OK;
}

//----------------------------

static I3DENUMRET I3DAPI enumFindPrnt(PI3D_frame frm, dword f){

   PI3D_frame *frm1a = (PI3D_frame*)f;
   *frm1a = frm;
   return I3DENUMRET_CANCEL;
}

//----------------------------

PI3D_frame I3D_frame::FindChildFrame(const char *name, dword flags) const{

   PI3D_frame frm = NULL;
   if((flags&ENUMF_WILDMASK) && (!name || *name=='*'))
      name = NULL;
   I3D_frame::EnumFrames(enumFindPrnt, (dword)&frm, flags, name);
   return frm;
}

//----------------------------
//----------------------------

void C_bound_volume::MakeHierarchyBounds(PI3D_frame frm){

   if(!(frm->frm_flags&FRMFLAGS_HR_BOUND_VALID)){
      frm->ComputeHRBoundVolume(&bound_local);

      frm->frm_flags |= FRMFLAGS_HR_BOUND_VALID;
      frm->frm_flags &= ~(FRMFLAGS_HR_BSPHERE_VALID);
   }
}

//----------------------------

void C_bound_volume::DebugDrawBounds(PI3D_scene scene, PI3D_frame frm, dword valid_flags){

   scene->SetRenderMatrix(frm->matrix);

   S_vector bbox_full[8];
   bound_local.bbox.Expand(bbox_full);
   const dword color = 0x40ffffff;
   scene->DebugDrawBBox(bbox_full, color);

   S_matrix m = I3DGetIdentityMatrix();
   const I3D_bsphere &bsphere = GetBoundSphereTrans(frm, valid_flags);
   m(3) = bsphere.pos;
   scene->DebugDrawSphere(m, bsphere.radius, color);
}

//----------------------------

void I3D_frame::GetChecksum(float &matrix_sum, float &vertc_sum, dword &num_v) const{

   matrix_sum = .0f;
   vertc_sum = .0f;
   num_v = 0;
                              //matrix
   const S_matrix &m = GetMatrix();
   for(int i=4; i--; ){
      for(int j=4; j--; ){
         matrix_sum += m(i, j);
      }
   }
}

//----------------------------
//----------------------------
