#include <IPhysics.h>
#include <C_linklist.h>
#include <C_unknwn.hpp>
#include "world.h"
#include "joint.h"
#include "lcp.h"
#include "body.h"

#pragma warning(push, 3)
#include <map>
#pragma warning(pop)

#include <sortlist.hpp>

//----------------------------

#define FIX_VEL_PROBLEM       //fix angular velocity explosion problem


#ifdef _DEBUG

//#define DEBUG_DRAW_VECTORS
//#define DEBUG_NO_LINE_COL_TESTS  //disable line collision tests used for detection of sink bodies

#endif

//----------------------------



/* round something up to be a multiple of the EFFICIENT_ALIGNMENT */
#define dEFFICIENT_SIZE(x) ((((x)-1)|(EFFICIENT_ALIGNMENT-1))+1)


/* alloca aligned to the EFFICIENT_ALIGNMENT. note that this can waste
 * up to 15 bytes per allocation, depending on what alloca() returns.
 */
#define dALLOCA16(n) \
  ((char*)dEFFICIENT_SIZE(((int)(alloca((n)+(EFFICIENT_ALIGNMENT-1))))))

//----------------------------
//----------------------------
PIPH_joint CreateJointBall(PIPH_world);
PIPH_joint CreateJointHinge(PIPH_world);
PIPH_joint CreateJointSlider(PIPH_world);
PIPH_joint CreateJointUniversal(PIPH_world);
PIPH_joint CreateJointHinge2(PIPH_world);
PIPH_joint CreateJointFixed(PIPH_world);
PIPH_joint CreateJointContact(PIPH_world, dxJointGroup &jg, const dContact *c);

//----------------------------

class IPH_world_imp: public IPH_world{
   float global_erp;          //global error reduction parameter
   float global_cfm;          //global costraint force mixing parameter
   S_vector gravity;          //gravity vector (m/s/s)

   dxJointGroup contact_joint_group;

   typedef std::map<PI3D_volume, IPH_body*> t_volume_map;
   t_volume_map volume_map;

                              //user-provided material parameters
   C_buffer<IPH_surface_param> material_params;
   IPH_surface_param default_mat_param;

   IPH_MessageFunction *msg_func;
   IPH_DebugLine *line_func;
   IPH_DebugPoint *point_func;
   IPH_DebugText *text_func;

   dword last_num_contacts;

//----------------------------
   //REF_COUTNER;

//----------------------------

   static void DefaultMessageHandler(const char *msg){
   }

//----------------------------

   void DebugLine(const S_vector &from, const S_vector &to, dword type = 1, dword color = 0xffffffff){
      if(line_func)
         line_func(from, to, type, color);
   }
   inline void DebugVector(const S_vector &from, const S_vector &dir, dword type = 1, dword color = 0xffffffff){
      DebugLine(from, from+dir, type, color);
   }

//----------------------------

   void DebugPoint(const S_vector &p, float radius = .1f, dword type = 1, dword color = 0xffffffff){
      if(point_func)
         point_func(p, radius, type, color);
   }

   void DEBUG(const char *cp) const{ if(text_func) text_func(cp, 0xffffffff); }
   void DEBUG(int i) const{ DEBUG(C_xstr("%") %i); }
   void DEBUG(const S_vector &v) const{ DEBUG(C_xstr("[#.2%, #.2%, #.2%]") %v.x %v.y %v.z); }

//----------------------------
                              //contact reporting helper
   struct S_contact_report{
      IPH_world_imp *_this;
      IPH_ContactQuery *c_query;
      IPH_ContactReport *c_report;
      void *context;

      CPI3D_volume src_vol;
      IPH_body *src_body;
   };

//----------------------------

   static dword I3DAPI ContactQuery(CPI3D_frame g2, void *context){

      S_contact_report &cr = *(S_contact_report*)context;

      IPH_body *body2 = NULL;

      if(g2->GetType()==FRAME_VOLUME){
         body2 = cr._this->GetVolumeBody(I3DCAST_CVOLUME(g2));
         if(body2){
                              //skip if they're set to the same body
                              // (also make sure bodies collisions are reported ony once)
            if(cr.src_body<=body2)
               return 0;
                              //skip if the two bodies are connected by a joint
            if(cr.src_body->IsConnected(body2))
               return 0;
         }
      }
      if(cr.c_query){
         if(!cr.c_query(cr.src_vol, cr.src_body, g2, body2, cr.context))
            return 0;
      }
      return 4;
   }

//----------------------------

   void ContactReport(S_contact_report &cr, CPI3D_frame g2,
      const S_vector &pos, const S_vector &normal, float depth){

      C_smart_ptr<IPH_body> body2 = NULL;
      dword mat_id = 0;

      switch(g2->GetType()){
      case FRAME_VOLUME:
         {
            CPI3D_volume vol = I3DCAST_CVOLUME(g2);
            body2 = GetVolumeBody(vol);
            assert(cr.src_body != body2);
            mat_id = vol->GetCollisionMaterial();
         }
         break;
      case FRAME_VISUAL:
         mat_id = I3DCAST_CVISUAL(g2)->GetCollisionMaterial();
         break;
      }
                              //report contact to user, if requested
      if(cr.c_report){
         if(!cr.c_report(cr.src_vol, cr.src_body, g2, body2, pos, normal, depth, cr.context))
            return;
      }
      if(body2 && !body2->IsEnabled())
         body2 = NULL;

      IPH_surface_param &surf = mat_id<material_params.size() ? material_params[mat_id] : default_mat_param;

      AddContact(cr.src_body, body2, surf, pos, normal, depth);
   }

//----------------------------

   static void I3DAPI ContactReport_thunk(CPI3D_frame frm, const S_vector &pos, const S_vector &normal,
      float depth, void *context){

      S_contact_report &cr = *(S_contact_report*)context;
      cr._this->ContactReport(cr, frm, pos, normal, depth);
   }

//----------------------------

   struct S_col_resp{
      IPH_world_imp *_this;
      PIPH_body src_body;
   };

   static bool I3DAPI ColRespLine(I3D_cresp_data &rd){

      S_col_resp *cr = (S_col_resp*)rd.cresp_context;
      CPI3D_frame frm = rd.GetHitFrm();
      if(frm->GetType()==FRAME_VOLUME){
         PIPH_body body2 = cr->_this->GetVolumeBody(I3DCAST_CVOLUME(frm));
         if(body2){
            if(body2==cr->src_body)
               return false;
            if(cr->src_body->IsConnected(body2))
               return false;
         }
      }
      return true;
   }

//----------------------------

   void MakeContacts(PI3D_scene scn, IPH_ContactQuery *c_query, IPH_ContactReport *c_report, void *context){

      S_contact_report cr;
      cr._this = this;
      cr.c_query = c_query;
      cr.c_report = c_report;
      cr.context = context;

      I3D_contact_data cd;
      cd.context = &cr;
      cd.cb_query = ContactQuery;
      cd.cb_report = ContactReport_thunk;
                              //process all bodies
      for(PIPH_body bb=bodies; bb; bb = ++*bb){
         PIPH_body body = bb;
         if(!body->IsEnabled())
            continue;

         const C_buffer<C_smart_ptr<I3D_volume> > &vols = body->GetVolumes();
         cr.src_body = body;
         for(dword i=vols.size(); i--; ){
                              //test this volume as source
            cd.vol_src = vols[i];
            cd.frm_ignore = cd.vol_src->GetOwner();
            cr.src_vol = cd.vol_src;
            scn->GenerateContacts(cd);
         }
      }
   }

//----------------------------
   virtual void SetGravity(const S_vector &g_in){
      gravity = g_in;
      //gravity *= .1f;
   }
   virtual const S_vector &GetGravity() const{ return gravity; }

//----------------------------
   virtual void SetERP(float erp){ global_erp = erp;; }
   virtual float GetERP() const{ return global_erp; }

//----------------------------
   virtual void SetCFM(float cfm){ global_cfm = cfm; }
   virtual float GetCFM() const{ return global_cfm; }

//----------------------------

   virtual void SetMaterials(const IPH_surface_param *mats, dword num_mats){
      material_params.assign(mats, mats+num_mats);
   }

//----------------------------

   void Tick1(dReal stepsize);
   void InternalStepIsland(const PIPH_body *body, int num_bodies, const PIPH_joint *joints, int num_joints, dReal stepsize) const;

//----------------------------

   virtual void Tick(int time, PI3D_scene scn, IPH_ContactQuery *c_query, IPH_ContactReport *c_report, void *context){

      last_num_contacts = 0;
      const int MAX_STEP = 1000/33;

      int num_steps = 1;
      float one_step = (float)time;
      if(time > MAX_STEP){
         num_steps = (time+MAX_STEP-1) / MAX_STEP;
         one_step = (float)time / (float)num_steps;
      }

      //S_vector *body_pos = new(alloca(num_bodies*sizeof(S_vector))) S_vector[num_bodies];
      S_vector *body_pos = NULL;
      dword body_pos_size = 0;
      for(dword i=num_steps; i--; ){
         MakeContacts(scn, c_query, c_report, context);

         if(time){            //may be only if zero time passed (pure contact generation - probably debug mode)

         if(body_pos_size < num_bodies){
            delete[] body_pos;
            body_pos = new S_vector[body_pos_size = num_bodies];
         }

                              //save positions of (active) bodies
         dword i = 0;
         for(PIPH_body bb=bodies; bb; bb = ++*bb)
            body_pos[i++] = bb->GetPos();
         assert(i==num_bodies);

                              //tick physics world
         Tick1(one_step * .005f);


                              //process all bodies - sort by number of parents, for proper order of transformation apply
         C_sort_list<PIPH_body> slist(num_bodies);

         i = 0;
         for(bb=bodies; bb; bb = ++*bb){
            PIPH_body body = bb;
#ifdef FIX_VEL_PROBLEM
                           //limit avel, avoid explosion
            S_vector &av = bb->avel;
            const float X = 1000.0f;
            av.Minimal(S_vector(X, X, X));
            av.Maximal(S_vector(-X, -X, -X));
            av *= .99f;
#endif
            PI3D_frame frm = body->GetFrame();
            if(frm)
               slist.Add(body, body->GetNumParents());
#ifndef DEBUG_NO_LINE_COL_TESTS
            float col_check_radius = body->GetColCheckRadius();
            if(col_check_radius != 0.0f){
                           //check that bodies do not fall through geometry
               //const S_matrix &m = frm->GetMatrix();
               //S_vector prev_pos = body->GetMassCenter1() * m;
               const S_vector &prev_pos = body_pos[i];
               //S_vector prev_pos = m(3) + body->GetMassCenter1() % m;
               const S_vector &curr_pos = body->GetPos();
               //DebugPoint(prev_pos, .1f, 1, 0xffff0000);
               //DebugPoint(curr_pos, .1f, 1, 0xff00ff00);
               S_vector dir = curr_pos - prev_pos;
               float d_2 = dir.Square();
               if(d_2 > (col_check_radius*col_check_radius*.05f)){
                           //test collision during the movement
                  S_col_resp cr;
                  cr._this = this;
                  cr.src_body = body;

                  I3D_collision_data cd;
                  cd.from = prev_pos;
                  cd.dir = dir;
                  cd.flags = I3DCOL_LINE;
                  //cd.frm_ignore = frm;
                  if(body->GetVolumes().size()){
                     CPI3D_volume vol = body->GetVolumes()[0];
                     cd.frm_ignore = vol->GetOwner();
                              //set collision's collide bits the same as in body's volume
                     cd.collide_bits = vol->GetCollideBits();
                  }
                  cd.radius = col_check_radius;
                  cd.callback = ColRespLine;
                  cd.cresp_context = &cr;

                  //DebugVector(cd.from, cd.dir, 0);
                  if(scn->TestCollision(cd)){
                     //msg_func(cd.GetHitFrm()->GetName());
                     float dist = cd.GetHitDistance();
                     //if(dist > col_check_radius)
                     {
                                    //make sure no part of object passes through collision
                        //DebugLine(prev_pos, curr_pos, 0);
                        S_plane pl;
                        pl.normal = dir;
                        pl.normal.Normalize();
                        pl.d = -prev_pos.Dot(pl.normal);
                        const C_buffer<C_smart_ptr<I3D_volume> > &vols = body->GetVolumes();
                        for(dword j=vols.size(); j--; ){
                           CPI3D_volume vol = vols[j];
                           switch(vol->GetVolumeType()){
                           case I3DVOLUME_CAPCYL:
                              {
                                 float add_d = 0.0f;
                                 const S_matrix &m = vol->GetMatrix();
                                 float scl = m(0).Magnitude();
                                 const S_vector nu_scl = vol->GetNUScale();
                                 for(dword i=2; i--; ){
                                    S_vector cap = m(3);
                                    cap += m(2) * (!i ? nu_scl.z : -nu_scl.z);
                                    float d = cap.DistanceToPlane(pl);
                                    d += nu_scl.x*scl;
                                    if(d > 0.0f){
                                       add_d = Max(add_d, d);
                                             //cap's ahead of body center, test it
                                       //cd.from = cap - pl.normal*d;
                                       //DebugVector(cd.from, cd.dir, 0, 0xff0000ff);
                                    }
                                 }
                                 dist -= add_d - nu_scl.x*scl;
                              }
                              break;
                           }
                        }
                        S_vector stop_pos = cd.from + cd.GetNormalizedDir() * (dist - col_check_radius*.05f);
                        //msg_func(C_xstr("Prev: %, Curr: %, New: %") % prev_pos.y % curr_pos.y % stop_pos.y);
                                             //apply to body only, it'll be applied to frame soon below
                        body->matrix(3) = stop_pos;
                        //body->Enable(false);
                     }
                  }
               }
            }
#endif//!DEBUG_NO_LINE_COL_TESTS
            ++i;
         }
         assert(i==num_bodies);
                              //sort list by number of parents
         slist.Sort();

                              //apply transform
         for(i=0; i<slist.Count(); i++){
            PIPH_body body = slist[i];

            body->UpdateFrameTransform();
            PI3D_frame frm = body->GetFrame();
            if(frm){
               PI3D_frame prnt = frm->GetParent();
               if(prnt && prnt->GetType()==FRAME_SECTOR){
                  scn->SetFrameSector(frm);
               }
            }
         }

         }
                              //remove all created contact joints
         last_num_contacts = contact_joint_group.num;
         contact_joint_group.Empty();
      }
      delete[] body_pos;
#ifdef DEBUG_DRAW_VECTORS
      {
         for(PIPH_body bb=bodies; bb; bb = ++*bb){
            PIPH_body b = bb;

            const S_vector &pos = b->GetPos();
            DebugVector(pos, b->GetLinearVelocity(), 1);
            DebugVector(pos, b->GetAngularVelocity(), 1, 0xff00ff00);
         }
      }
#endif
   }

//----------------------------

   mutable dword seed;

   void SetRandSeed(dword s) const{ seed = s; }

   int Rand() const{
      seed = (1664525L*seed + 1013904223L) & 0xffffffff;
      return seed;
   }

//----------------------------

   S_vector GetNormalOnCell(const S_vector &n, int x, int y) const{

                              //generate 3 pseudo-random numbers in range -1 ... +1
      SetRandSeed(x*1237 + y*113);
      const int RND_MAX = 0xffff;
      const float R_MAX = 2.0f / (float)RND_MAX;
      S_vector r;
      r.x = (float)((Rand()&RND_MAX)-RND_MAX/2) * R_MAX;
      r.y = (float)((Rand()&RND_MAX)-RND_MAX/2) * R_MAX;
      r.z = (float)((Rand()&RND_MAX)-RND_MAX/2) * R_MAX;
      DEBUG(r);

                              //add to normal (scaled)
      S_vector ret = n + r * .5f;
      ret.Normalize();
      return ret;
   }

//----------------------------

   S_vector2 GetCellForce(int x, int y) const{

                              //generate 3 pseudo-random numbers in range -1 ... +1
      SetRandSeed(x*1237 + y*113);
      const int RND_MAX = 0xffff;
      const float R_MAX = 2.0f / (float)RND_MAX;
      S_vector2 r;
      r.x = (float)((Rand()&RND_MAX)-RND_MAX/2) * R_MAX;
      r.y = (float)((Rand()&RND_MAX)-RND_MAX/2) * R_MAX;
      //DEBUG(r);
      return r;
   }

//----------------------------

   void AddContact(PIPH_body b1, PIPH_body b2,
      const IPH_surface_param &surf, const S_vector &pos, const S_vector &normal, float depth){

      dContact c;
      {
         dSurfaceParameters &s = c.surface;
         s.mode = surf.mode | dContactApprox1;
         s.mu = surf.coulomb_friction;
         s.bounce = surf.bounce;
         s.bounce_vel = surf.bounce_vel;
         s.soft_erp = surf.soft_erp;
         s.soft_cfm = surf.soft_cfm;
         s.slip1 = surf.slip;
         s.slip2 = surf.slip;

         s.mode |= dContactSlip1 | dContactSlip2;
         s.slip1 = .0005f;
         s.slip2 = .00001f;
      }
      {
         dContactGeom &g = c.geom;
         g.pos = pos;
         S_vector &n = g.normal;
         n = normal;
         g.depth = depth;

#if defined _DEBUG && 0
         if(!b2){
            S_vector f = b1->GetLinearVelocity();
            f.y *= .0f;
            f *= b1->GetMass() * -1.4f;
            b1->AddForceAtPos(b1->GetPos(), f);

            /*
            if(n.y > 0.0f){
               S_vector fp = b1->GetPos();
               const S_vector &lvel = b1->GetLinearVelocity();
               S_vector v = b1->GetVelAtPos(pos);
               //b1->AddForceAtPos(fp, v * -(n.y*1.0f*b1->GetMass()));
               float f = (lvel.Magnitude() * n.y * b1->GetMass() * 10.0f);
               f = Max(.01f, f);
               b1->AddForceAtPos(fp, lvel * -f);
            }
            */


            /*
                              //normal randomization method
            void PlaneSpace(const S_vector &n, S_vector &nx, S_vector &ny);

            S_vector nx, ny;
            PlaneSpace(n, nx, ny);
            float dx = -pos.Dot(nx);
            float dy = -pos.Dot(ny);

            const float CELL_SIZE = .2f;
            const float R_CELL_SIZE = 1.0f / CELL_SIZE;
                              //convert to cell indices
            dx *= R_CELL_SIZE;
            dy *= R_CELL_SIZE;
            int cx = FloatToInt(dx-.5f);
            int cy = FloatToInt(dy-.5f);
            float fx = dx - (float)cx;
            float fy = dy - (float)cy;
            DEBUG(C_xstr(
               "c = [%, %], f = [#.2%, #.2%]")
               %cx %cy %fx %fy);
            */

            /*
            S_vector n00 = GetNormalOnCell(n, cx, cy);
            S_vector n01 = GetNormalOnCell(n, cx+1, cy);
            S_vector n10 = GetNormalOnCell(n, cx, cy+1);
            S_vector n11 = GetNormalOnCell(n, cx+1, cy+1);

            DebugPoint(pos);
            //DebugVector(pos, n00, 1);
            DebugVector(pos, n, 1, 0xff0000ff);
            DebugVector(pos, nx, 1, 0xffff0000);
            DebugVector(pos, ny, 1, 0xff00ff00);

            S_vector n0 = n00 * (1.0f-fx) + n01 * fx;
            S_vector n1 = n10 * (1.0f-fx) + n11 * fx;

            n = n0 * (1.0f-fy) + n1 * fy;
            n.Normalize();
            DebugVector(pos, n, 1);
            */

            /*
            S_vector2 f00 = GetCellForce(cx, cy);
            S_vector2 f01 = GetCellForce(cx+1, cy);
            S_vector2 f10 = GetCellForce(cx, cy+1);
            S_vector2 f11 = GetCellForce(cx+1, cy+1);

            S_vector2 f0 = f00 * (1.0f-fx) + f01 * fx;
            S_vector2 f1 = f10 * (1.0f-fx) + f11 * fx;

            S_vector2 f = f0 * (1.0f-fy) + f1 * fy;
            f *= .3f;
            S_vector ff; ff.x =f.x; ff.y = 0; ff.z = f.y;

            b1->AddForceAtPos(b1->GetPos(), ff);
            */
         }
#endif
         /*
         if(surf.roughness){
            S_vector lvel = b1->GetLinearVelocity();
            if(b2){
               lvel += b2->GetLinearVelocity();
               lvel *= .5f;
            }
            n = lvel*(-surf.roughness) + n*(1.0f-surf.roughness);
         }
         */

         /*
         const float K = .8f;
         n.x *= K;
         n.z *= K;
         n.y *= 1.0f/K;
         n.Normalize();
         /**/
      }
                              //create contact joint
      ++contact_joint_group.num;
      PIPH_joint j = CreateJointContact(this, contact_joint_group, &c);
      j->Attach(b1, b2);
   }

//----------------------------

   virtual PIPH_body CreateBody(){
      return new IPH_body(this);
   }

//----------------------------

   virtual PIPH_joint CreateJoint(IPH_JOINT_TYPE t){

      switch(t){
      case IPHJOINTTYPE_BALL: return CreateJointBall(this);
      case IPHJOINTTYPE_HINGE: return CreateJointHinge(this);
      case IPHJOINTTYPE_SLIDER: return CreateJointSlider(this);
      case IPHJOINTTYPE_UNIVERSAL: return CreateJointUniversal(this);
      case IPHJOINTTYPE_HINGE2: return CreateJointHinge2(this);
      case IPHJOINTTYPE_FIXED: return CreateJointFixed(this);
      }
      return NULL;
   }

//----------------------------

   virtual PIPH_body GetVolumeBody(CPI3D_volume vol) const{

      t_volume_map::const_iterator it = volume_map.find(const_cast<PI3D_volume>(vol));
      if(it!=volume_map.end())
         return it->second;
      return NULL;
   }

//----------------------------

   virtual void SetErrorHandler(IPH_MessageFunction *msg_func1){
      msg_func = msg_func1;
      if(!msg_func)
         msg_func = DefaultMessageHandler;
   }
   virtual void SetDebugLineFunc(IPH_DebugLine *func){ line_func = func; }
   virtual void SetDebugPointFunc(IPH_DebugPoint *func){ point_func = func; }
   virtual void SetDebugTextFunc(IPH_DebugText *func){ text_func = func; }

//----------------------------

   virtual bool JointCanAttachToOneBody(IPH_JOINT_TYPE jt) const{
      switch(jt){
      case IPHJOINTTYPE_HINGE2:
      //case IPHJOINTTYPE_AMOTOR:
      //case IPHJOINTTYPE_BALL:
         return false;
      }
      return true;
   }

//----------------------------

   virtual dword NumBodies() const{ return num_bodies; }
   virtual dword NumJoints() const{ return num_joints; }
   virtual dword NumContacts() const{ return last_num_contacts; }

//----------------------------
public:
   IPH_world_imp():
      global_erp(.2f),
      global_cfm(1e-5f),
      gravity(0, 0, 0),
      last_num_contacts(0),
      line_func(NULL),
      point_func(NULL),
      text_func(NULL)
   {
                              //set default world gravity
      SetGravity(S_vector(0.0f, -9.81f, 0.0f));

                              //setup default material surface
      {
         IPH_surface_param &s = default_mat_param;
         s.coulomb_friction = 10.3f;
         s.slip = 1.0f;
         //s.soft_cfm = 3.8f;
         s.roughness = .5f;
      }
      SetErrorHandler(NULL);
   }

   ~IPH_world_imp(){
                                 //delete all bodies and joints
      for(PIPH_body b=bodies; b; b = ++*b)
         b->world = NULL;
      for(PIPH_joint j = joints; j; j = ++*j){
                                 //the joint is part of a group, so "deactivate" it instead
         j->world = 0;
         j->node[0].body = 0;
         j->node[0].next = 0;
         j->node[1].body = 0;
         j->node[1].next = 0;
      }
   }

//----------------------------

   void AddVolume(PI3D_volume vol, PIPH_body body){
      volume_map[vol] = body;
   }

//----------------------------

   void RemoveVolume(PI3D_volume vol){
      t_volume_map::iterator it = volume_map.find(vol);
      if(it!=volume_map.end())
         volume_map.erase(it);
   }

//----------------------------
};

//****************************************************************************
// island processing

// This groups all joints and bodies in a world into islands. All objects
// in an island are reachable by going through connected bodies and joints.
// Each island can be simulated separately.
// Note that joints that are not attached to anything will not be included
// in any island, an so they do not affect the simulation.
//
// This function starts new island from unvisited bodies. However, it will
// never start a new islands from a disabled body. Thus islands of disabled
// bodies will not be included in the simulation. Disabled bodies are
// re-enabled if they are found to be part of an active island.

void IPH_world_imp::Tick1(dReal stepsize){

                              //nothing to do if no bodies
   if(!num_bodies)
      return;

                              //make arrays for body and joint lists (for a single island) to go into
   PIPH_body *bodies = (PIPH_body*)dALLOCA16(num_bodies * sizeof(PIPH_body));
   PIPH_joint *joints = (PIPH_joint*)dALLOCA16(num_joints * sizeof(PIPH_joint));
   int bcount = 0;	         //number of bodies in `body'
   int jcount = 0;	         //number of joints in `joint'

                              //set all body/joint tags to 0
   {
      for(PIPH_body b=IPH_world::bodies; b; b = ++*b)
         b->tag = 0;
      for(PIPH_joint j=IPH_world::joints; j; j = ++*j)
         j->tag = 0;
   }

                              //allocate a stack of unvisited bodies in the island. the maximum size of
                              //the stack can be the lesser of the number of bodies or joints, because
                              //new bodies are only ever added to the stack by going through untagged
                              //joints. all the bodies in the stack must be tagged!
   dword stackalloc = (num_joints < num_bodies) ? num_joints : num_bodies;
   PIPH_body *stack = (PIPH_body*)dALLOCA16(stackalloc * sizeof(PIPH_body));

   for(PIPH_body bb=IPH_world::bodies; bb; bb = ++*bb){
                              //get bb = the next enabled, untagged body, and tag it
      if(bb->tag || (bb->flags&IPH_body::BF_Disabled))
         continue;
      bb->tag = 1;
      
                              //tag all bodies and joints starting from bb
      dword stacksize = 0;
      PIPH_body b = bb;
      bodies[0] = bb;
      bcount = 1;
      jcount = 0;
      //goto quickstart;

      //while(stacksize > 0)
      while(true){
         //b = stack[--stacksize];	// pop body off stack
         //bodies[bcount++] = b;	// put body on body list
//quickstart:
         
                              //traverse and tag all body's joints, add untagged connected bodies to stack
         for(dxJointNode *n = b->firstjoint; n; n = n->next){
            if(!n->joint->tag){
               n->joint->tag = 1;
               joints[jcount++] = n->joint;
               if(n->body && !n->body->tag){
                  n->body->tag = 1;
                  stack[stacksize++] = n->body;
               }
            }
         }
         assert(stacksize <= num_bodies);
         assert(stacksize <= num_joints);

         if(!stacksize)
            break;
         b = stack[--stacksize];	// pop body off stack
         bodies[bcount++] = b;	// put body on body list
      }
                              //now do something with body and joint lists
      InternalStepIsland(bodies, bcount, joints, jcount, stepsize);
      
                              //what we've just done may have altered the body/joint tag values
                              // we must make sure that these tags are nonzero
                              // also make sure all bodies are in the enabled state
      int i;
      for(i=0; i<bcount; i++){
         bodies[i]->tag = 1;
         bodies[i]->flags &= ~IPH_body::BF_Disabled;
      }
      for(i=0; i<jcount; i++)
         joints[i]->tag = 1;
   }

                              //if debugging, check that all objects (except for disabled bodies,
                              // unconnected joints, and joints that are connected to disabled bodies)
                              // were tagged
#ifdef _DEBUG
   {
      for(PIPH_body b=IPH_world::bodies; b; b = ++*b){
         if(b->flags&IPH_body::BF_Disabled){
            assert(!b->tag);
         }else{
            assert(b->tag);
         }
      }
      for(PIPH_joint j=IPH_world::joints; j; j = ++*j){
         if((j->node[0].body && (j->node[0].body->flags & IPH_body::BF_Disabled)==0) ||
            (j->node[1].body && (j->node[1].body->flags & IPH_body::BF_Disabled)==0)){
            assert(j->tag);
         }else{
            assert(!j->tag);
         }
      }
   }
# endif
}

//----------------------------

//****************************************************************************
// an optimized version of dInternalStepIsland1()

#pragma optimize("g", off)    //temporary optims off - something goes wrong with data if enabled

//----------------------------
//----------------------------

//----------------------------
// this assumes the 4th and 8th rows of B and C are zero.
static void MultiplyAdd2_p8r(dReal *A, dReal *B, dReal *C, int p, int r, int Askip){

   dReal sum, *bb, *cc;
   assert(p>0 && r>0 && A && B && C);
   bb = B;
   for(int i=p; i; i--){
      cc = C;
      for(int j=r; j; j--){
         sum = bb[0]*cc[0];
         sum += bb[1]*cc[1];
         sum += bb[2]*cc[2];
         sum += bb[4]*cc[4];
         sum += bb[5]*cc[5];
         sum += bb[6]*cc[6];
         *(A++) += sum; 
         cc += 8;
      }
      A += Askip - r;
      bb += 8;
   }
}

//----------------------------
// this assumes the 4th and 8th rows of B and C are zero.
static void Multiply2_p8r (dReal *A, dReal *B, dReal *C, int p, int r, int Askip){

   dReal sum, *bb, *cc;
   assert(p>0 && r>0 && A && B && C);
   bb = B;
   for(int i=p; i; i--){
      cc = C;
      for(int j=r; j; j--) {
         sum = bb[0]*cc[0];
         sum += bb[1]*cc[1];
         sum += bb[2]*cc[2];
         sum += bb[4]*cc[4];
         sum += bb[5]*cc[5];
         sum += bb[6]*cc[6];
         *(A++) = sum; 
         cc += 8;
      }
      A += Askip - r;
      bb += 8;
   }
}

//----------------------------
// this assumes the 4th and 8th rows of B are zero.
static void Multiply0_p81(dReal *A, dReal *B, dReal *C, int p){

   assert(p>0 && A && B && C);
   dReal sum;
   for(int i=p; i; i--){
      sum =  B[0]*C[0];
      sum += B[1]*C[1];
      sum += B[2]*C[2];
      sum += B[4]*C[4];
      sum += B[5]*C[5];
      sum += B[6]*C[6];
      *(A++) = sum;
      B += 8;
   }
}

//----------------------------
// this assumes the 4th and 8th rows of B are zero.
static void MultiplyAdd0_p81(dReal *A, dReal *B, dReal *C, int p){

   assert(p>0 && A && B && C);
   dReal sum;
   for(int i=p; i; i--){
      sum =  B[0]*C[0];
      sum += B[1]*C[1];
      sum += B[2]*C[2];
      sum += B[4]*C[4];
      sum += B[5]*C[5];
      sum += B[6]*C[6];
      *(A++) += sum;
      B += 8;
   }
}

//----------------------------
// this assumes the 4th and 8th rows of B are zero.
static void Multiply1_8q1(dReal *A, dReal *B, dReal *C, int q){

   int k;
   dReal sum;
   assert(q>0 && A && B && C);
   sum = 0;
   for (k=0; k<q; k++) sum += B[k*8] * C[k];
   A[0] = sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[1+k*8] * C[k];
   A[1] = sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[2+k*8] * C[k];
   A[2] = sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[4+k*8] * C[k];
   A[4] = sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[5+k*8] * C[k];
   A[5] = sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[6+k*8] * C[k];
   A[6] = sum;
}

//----------------------------
// this assumes the 4th and 8th rows of B are zero.
static void MultiplyAdd1_8q1(S_vector &force, S_vector &torque, dReal *B, dReal *C, int q){

   int k;
   dReal sum;
   assert(q>0);
   sum = 0;
   for (k=0; k<q; k++) sum += B[k*8] * C[k];
   force[0] += sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[1+k*8] * C[k];
   force[1] += sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[2+k*8] * C[k];
   force[2] += sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[4+k*8] * C[k];
   torque[0] += sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[5+k*8] * C[k];
   torque[1] += sum;
   sum = 0;
   for (k=0; k<q; k++) sum += B[6+k*8] * C[k];
   torque[2] += sum;
}

//----------------------------

void IPH_world_imp::InternalStepIsland(const PIPH_body *body, int num_bodies,
   const PIPH_joint *joints, int num_joints, dReal stepsize) const{

   int i, j, k;

   float stepsize1 = 1.0f / stepsize;
   
                              //number all bodies in the body list - set their tag values
   for(i=num_bodies; i--; )
      body[i]->tag = i;
   
   struct S_joint_info{
      PIPH_joint jnt;
      IPH_joint::Info1 info;
      int ofs;
   };
   S_joint_info *ji = new S_joint_info[num_joints];


   struct S_body_info{
      //S_matrix inv_I;
      S_matrix __inv_I;
                                 //this will be set to the force due to the constraints
      S_vector force, torque;
      S_vectorw tmp[2];
   };
                              // for all bodies, compute the inertia tensor and its inverse in the global
                              // frame, and compute the rotational force and add it to the torque
                              // accumulator. I and invI are vertically stacked 3x4 matrices, one per body
   S_body_info *bi = new S_body_info[num_bodies];
   memset(bi, 0, num_bodies*sizeof(S_body_info));

   for(i=0; i<num_bodies; i++){
      PIPH_body b = body[i];
#if 0
      S_matrix tmp, I;

      //const dMatrix3 &b_mat = b->_GetRotMatrix();
      const S_matrix b_mat = b->GetInvMatrix();
                              //compute inertia tensor in global frame
      dMULTIPLY2_333(&tmp(0).x, &b->mass.GetInertiaMatrix()(0).x, &b_mat(0).x);
      dMULTIPLY0_333(&I(0).x, &b_mat(0).x, &tmp(0).x);
                              //compute inverse inertia tensor in global frame
      dMULTIPLY2_333(&tmp(0).x, b->inv_I, &b_mat(0).x);
      //dMULTIPLY0_333(bi[i].invI, b_mat, &tmp(0).x);
      dMULTIPLY0_333(&bi[i].inv_I(0).x, &b_mat(0).x, &tmp(0).x);
      bi[i].inv_I.Transpose();
                              //compute rotational force
      S_vector v; dMULTIPLY0_331(&v.x, &I(0).x, &b->avel.x);
      b->t_acc -= b->avel.Cross(v);
#else
      {
         S_matrix m_body_i = b->GetInvMatrix();
         S_matrix I1 = (m_body_i % b->mass.GetInertiaMatrix()) % b->GetMatrix();

         bi[i].__inv_I = (m_body_i % b->inv_i_tensor) % b->GetMatrix();
         //bi[i].inv_I = bi[i].__inv_I; bi[i].inv_I.Transpose();

         S_vector v = b->avel % I1;
         b->t_acc -= b->avel.Cross(v);
      }
#endif
   //}
   
                              //add the gravity force to all bodies
   //for(i=0; i<num_bodies; i++){
      if(!(b->flags&IPH_body::BF_NoGravity)){
         b->f_acc += gravity * b->mass.GetWeight();
      }
   }
   
                                    // get m = total constraint dimension, nub = number of unbounded variables.
                                    // create constraint offset array and number-of-rows array for all joints.
                                    // the constraints are re-ordered as follows: the purely unbounded
                                    // constraints, the mixed unbounded + LCP constraints, and last the purely
                                    // LCP constraints. this assists the LCP solver to put all unbounded
                                    // variables at the start for a quick factorization.
                                    //
                                    // joints with m=0 are inactive and are removed from the joints array
                                    // entirely, so that the code that follows does not consider them.
                                    // also number all active joints in the joint list (set their tag values).
                                    // inactive joints receive a tag value of -1.

   for(i=0, j=0; j<num_joints; j++){  //i=dest, j=src
      PIPH_joint jnt = joints[j];
      jnt->GetInfo1(&ji[i].info);
      assert(ji[i].info.m >= 0 && ji[i].info.m <= 6 && ji[i].info.nub >= 0 && ji[i].info.nub <= ji[i].info.m);
      if(ji[i].info.m > 0){
         ji[i].jnt = jnt;
         jnt->tag = i;
         i++;
      }else{
         jnt->tag = -1;
      }
   }
   num_joints = i;
   
   int m = 0;
                              //the purely unbounded constraints
   for(i=0; i<num_joints; i++){
      if(ji[i].info.nub == ji[i].info.m){
         ji[i].ofs = m;
         m += ji[i].info.m;
      }
   }
   int nub = m;
                              //the mixed unbounded + LCP constraints
   for(i=0; i<num_joints; i++){
      if(ji[i].info.nub > 0 && ji[i].info.nub < ji[i].info.m){
         ji[i].ofs = m;
         m += ji[i].info.m;
      }
   }
                              //the purely LCP constraints
   for(i=0; i<num_joints; i++){
      if(ji[i].info.nub == 0){
         ji[i].ofs = m;
         m += ji[i].info.m;
      }
   }

   
                              //if there are constraints, compute cforce
   if(m > 0){
                              //create a constraint equation right hand side vector `c', a constraint
                              // force mixing vector `cfm', and LCP low and high bound vectors, and an
                              // 'findex' vector.
      dReal *c = (dReal*)alloca(m*sizeof(dReal));
      dReal *cfm = (dReal*)alloca(m*sizeof(dReal));
      dReal *lo = (dReal*)alloca(m*sizeof(dReal));
      dReal *hi = (dReal*)alloca(m*sizeof(dReal));
      int *findex = (int*)alloca(m*sizeof(int));
      for(i=0; i<m; i++){
         c[i] = 0;
         cfm[i] = global_cfm;
         lo[i] = -dInfinity;
         hi[i] = dInfinity;
         findex[i] = -1;
      }

                               // get jacobian data from constraints. a (2*m)x8 matrix will be created
                               // to store the two jacobian blocks from each constraint. it has this
                               // format:
                               //
                               //   l l l 0 a a a 0  \    .
                               //   l l l 0 a a a 0   -- jacobian body 1 block for joint 0 (3 rows)
                               //   l l l 0 a a a 0  /
                               //   l l l 0 a a a 0  \    .
                               //   l l l 0 a a a 0   -- jacobian body 2 block for joint 0 (3 rows)
                               //   l l l 0 a a a 0  /
                               //   l l l 0 a a a 0  --- jacobian body 1 block for joint 1 (1 row)
                               //   l l l 0 a a a 0  --- jacobian body 2 block for joint 1 (1 row)
                               //   etc...
                               //
                               //   (lll) = linear jacobian data
                               //   (aaa) = angular jacobian data
                               //
      float *J = (float*)dALLOCA16(2*m*8*sizeof(float));
      memset(J, 0, 2*m*8*sizeof(float));
      IPH_joint::Info2 Jinfo;
      //Jinfo.rowskip = 8;
      Jinfo.fps = stepsize1;
      Jinfo.erp = global_erp;
      for(i=0; i<num_joints; i++){
         int _ofs = ji[i].ofs;
         Jinfo.J1l = J + 2*8*_ofs;

         Jinfo.J1a = Jinfo.J1l + 4;
         Jinfo.J2l = Jinfo.J1l + 8*ji[i].info.m;

         Jinfo.J2a = Jinfo.J2l + 4;
         Jinfo._c = c + _ofs;
         Jinfo.cfm = cfm + _ofs;
         Jinfo.lo = lo + _ofs;
         Jinfo.hi = hi + _ofs;
         Jinfo.findex = findex + _ofs;
         ji[i].jnt->GetInfo2(&Jinfo);
                              //adjust returned findex values for global index numbering
         for(j=0; j<ji[i].info.m; j++){
            if(findex[_ofs + j] >= 0)
               findex[_ofs + j] += _ofs;
         }
      }

                               // compute A = J*invM*J'. first compute JinvM = J*invM. this has the same
                               // format as J so we just go through the constraints in J multiplying by
                               // the appropriate scalars and matrices.
      float *JinvM = (float*)dALLOCA16(2*m*8*sizeof(float));
      memset(JinvM, 0, 2*m*8*sizeof(float));
      for(i=0; i<num_joints; i++){
         int _ofs = ji[i].ofs;

         int b = ji[i].jnt->node[0].body->tag;
         dReal body_invMass = body[b]->r_mass;

         //dReal *body_invI = &bi[b].inv_I(0).x;
         const S_matrix &__body_invI = bi[b].__inv_I;

         dReal *Jsrc = J + 2*8*_ofs;
         dReal *Jdst = JinvM + 2*8*_ofs;

         for(j=ji[i].info.m-1; j>=0; j--){
            for (k=0; k<3; k++) Jdst[k] = Jsrc[k] * body_invMass;
            //dMULTIPLY0_133 (Jdst+4,Jsrc+4,body_invI);
            *(S_vector*)(Jdst+4) = *(S_vector*)(Jsrc+4) % __body_invI;
            Jsrc += 8;
            Jdst += 8;
         }
         if(ji[i].jnt->node[1].body){
            b = ji[i].jnt->node[1].body->tag;
            body_invMass = body[b]->r_mass;
            //body_invI = &bi[b].inv_I(0).x;
            const S_matrix &__body_invI = bi[b].__inv_I;
            for(j=ji[i].info.m-1; j>=0; j--){
               for(k=0; k<3; k++)
                  Jdst[k] = Jsrc[k] * body_invMass;

               //dMULTIPLY0_133 (Jdst+4,Jsrc+4,body_invI);
               *(S_vector*)(Jdst+4) = *(S_vector*)(Jsrc+4) % __body_invI;
               Jsrc += 8;
               Jdst += 8;
            }
         }
      }
      
                               // now compute A = JinvM * J'. A's rows and columns are grouped by joint,
                               // i.e. in the same way as the rows of J. block (i,j) of A is only nonzero
                               // if joints i and j have at least one body in common. this fact suggests
                               // the algorithm used to fill A:
                               //
                               //    for b = all bodies
                               //      n = number of joints attached to body b
                               //      for i = 1..n
                               //        for j = i+1..n
                               //          ii = actual joint number for i
                               //          jj = actual joint number for j
                               //          // (ii,jj) will be set to all pairs of joints around body b
                               //          compute blockwise: A(ii,jj) += JinvM(ii) * J(jj)'
                               //
                               // this algorithm catches all pairs of joints that have at least one body
                               // in common. it does not compute the diagonal blocks of A however -
                               // another similar algorithm does that.

      int mskip = dPAD(m);
      float *A = (float*)dALLOCA16(m*mskip*sizeof(float));
      memset(A, 0, m*mskip*sizeof(float));
      for(i=0; i<num_bodies; i++){
         for(dxJointNode *n1=body[i]->firstjoint; n1; n1=n1->next){
            for(dxJointNode *n2=n1->next; n2; n2=n2->next){
                              //get joint numbers and ensure ofs[j1] >= ofs[j2]
               int j1 = n1->joint->tag;
               int j2 = n2->joint->tag;
               int ofs1 = ji[j1].ofs;
               int ofs2 = ji[j2].ofs;

               if(ofs1 < ofs2){
                  std::swap(j1, j2);
                  std::swap(ofs1, ofs2);
               }
                              //if either joint was tagged as -1 then it is an inactive (m=0)
                              // joint that should not be considered
               if (j1==-1 || j2==-1) continue;
               
                              //determine if body i is the 1st or 2nd body of joints j1 and j2
               int jb1 = (ji[j1].jnt->node[1].body == body[i]);
               int jb2 = (ji[j2].jnt->node[1].body == body[i]);
                              //jb1/jb2 must be 0 for joints with only one body
               assert(ji[j1].jnt->node[1].body || jb1==0);
               assert(ji[j2].jnt->node[1].body || jb2==0);
               
                              //set block of A
               MultiplyAdd2_p8r(A + ofs1*mskip + ofs2,
                  JinvM + 2*8*ofs1 + jb1*8*ji[j1].info.m,
                  J     + 2*8*ofs2 + jb2*8*ji[j2].info.m,
                  ji[j1].info.m, ji[j2].info.m, mskip);
            }
         }
      }
                              //compute diagonal blocks of A
      for(i=0; i<num_joints; i++){
         int _ofs = ji[i].ofs;
         Multiply2_p8r(A + _ofs*(mskip+1),
            JinvM + 2*8*_ofs,
            J + 2*8*_ofs,
            ji[i].info.m, ji[i].info.m, mskip);
         if(ji[i].jnt->node[1].body){
            MultiplyAdd2_p8r (A + _ofs*(mskip+1),
               JinvM + 2*8*_ofs + 8*ji[i].info.m,
               J + 2*8*_ofs + 8*ji[i].info.m,
               ji[i].info.m, ji[i].info.m, mskip);
         }
      }
      
                              //add cfm to the diagonal of A
      for(i=0; i<m; i++)
         A[i*mskip+i] += cfm[i] * stepsize1;

                              //compute the right hand side `rhs'
                              //put v/h + invM*fe into tmp1
      for(i=0; i<num_bodies; i++){
         dReal body_invMass = body[i]->r_mass;
         //dReal *body_invI = &bi[i].inv_I(0).x;
         const S_matrix &__body_invI = bi[i].__inv_I;

         //for(j=0; j<3; j++) bi[i].tmp1[j] = body[i]->facc[j] * body_invMass + body[i]->lvel[j] * stepsize1;
         bi[i].tmp[0] = body[i]->f_acc * body_invMass + body[i]->lvel * stepsize1;

         //dMULTIPLY0_331(&bi[i].tmp[1].x, body_invI, &body[i]->t_acc.x);
         bi[i].tmp[1] = body[i]->t_acc % __body_invI;
         //for(j=0; j<3; j++) bi[i].tmp1[4+j] += body[i]->avel[j] * stepsize1;
         bi[i].tmp[1] += body[i]->avel * stepsize1;
      }
                              //put J*tmp1 into rhs
      float *rhs = (float*)dALLOCA16(m * sizeof(float));
      memset(rhs, 0, m * sizeof(float));
      for(i=0; i<num_joints; i++){
         int _ofs = ji[i].ofs;
         dReal *JJ = J + 2*8*_ofs;
         Multiply0_p81(rhs+_ofs,JJ,
            &bi[ji[i].jnt->node[0].body->tag].tmp[0].x, ji[i].info.m);

         if(ji[i].jnt->node[1].body){
            MultiplyAdd0_p81(rhs+_ofs,JJ + 8*ji[i].info.m,
               &bi[ji[i].jnt->node[1].body->tag].tmp[0].x, ji[i].info.m);
         }
      }
                              //complete rhs
      for(i=0; i<m; i++)
         rhs[i] = c[i]*stepsize1 - rhs[i];

                            // solve the LCP problem and get lambda
                            // this will destroy A but that's okay
      dReal *lambda = (dReal*)dALLOCA16(m * sizeof(dReal));
      dReal *residual = (dReal*)dALLOCA16(m * sizeof(dReal));
      memset(residual, 0, m * sizeof(dReal));
      dSolveLCP(m, A, lambda, rhs, residual, nub, lo, hi, findex);

//  OLD WAY - direct factor and solve
//
//    // factorize A (L*L'=A)
//    dReal *L = (dReal*) dALLOCA16 (m*mskip*sizeof(dReal));
//    memcpy (L,A,m*mskip*sizeof(dReal));
//#   ifdef FAST_FACTOR
//    dFastFactorCholesky (L,m);  // does not report non positive definiteness
//#   else
//    if (dFactorCholesky (L,m)==0) dDebug (0,"A is not positive definite");
//#   endif
//
//    // compute lambda
//    dReal *lambda = (dReal*) dALLOCA16 (m * sizeof(dReal));
//    memcpy (lambda,rhs,m * sizeof(dReal));
//    dSolveCholesky (L,lambda,m);

                              //compute the constraint force `cforce'

                              //compute cforce = J'*lambda
      for(i=0; i<num_joints; i++){
         PIPH_joint jnt = ji[i].jnt;
         int _ofs = ji[i].ofs;

         dReal *JJ = J + 2*8*_ofs;
         PIPH_body b1 = jnt->node[0].body;
         PIPH_body b2 = jnt->node[1].body;
         IPH_joint::dJointFeedback *fb = jnt->feedback;
         
         if(fb){
                              // the user has requested feedback on the amount of force that this
                              // joint is applying to the bodies. we use a slightly slower
                              // computation that splits out the force components and puts them
                              // in the feedback structure.
            dReal data1[8],data2[8];
            Multiply1_8q1(data1, JJ, lambda+_ofs, ji[i].info.m);

            //dReal *cf1 = bi[b1->tag].cforce;
            S_vector &cf10 = bi[b1->tag].force;
            cf10[0] += (fb->f1[0] = data1[0]);
            cf10[1] += (fb->f1[1] = data1[1]);
            cf10[2] += (fb->f1[2] = data1[2]);
            S_vector &cf11 = bi[b1->tag].torque;
            cf11[0] += (fb->t1[0] = data1[4]);
            cf11[1] += (fb->t1[1] = data1[5]);
            cf11[2] += (fb->t1[2] = data1[6]);
            if(b2){
               Multiply1_8q1 (data2, JJ + 8*ji[i].info.m, lambda+_ofs, ji[i].info.m);

               //dReal *cf2 = bi[b2->tag].cforce;
               S_vector &cf20 = bi[b2->tag].force;
               cf20[0] += (fb->f2[0] = data2[0]);
               cf20[1] += (fb->f2[1] = data2[1]);
               cf20[2] += (fb->f2[2] = data2[2]);
               S_vector &cf21 = bi[b2->tag].torque;
               cf21[0] += (fb->t2[0] = data2[4]);
               cf21[1] += (fb->t2[1] = data2[5]);
               cf21[2] += (fb->t2[2] = data2[6]);
            }
         }else{
                              //no feedback is required, let's compute cforce the faster way
            //MultiplyAdd1_8q1(bi[b1->tag].cforce, JJ, lambda+_ofs, ji[i].info.m);
            MultiplyAdd1_8q1(bi[b1->tag].force, bi[b1->tag].torque, JJ, lambda+_ofs, ji[i].info.m);

            if(b2){
               //MultiplyAdd1_8q1(bi[b2->tag].cforce, JJ + 8*ji[i].info.m, lambda+_ofs, ji[i].info.m);
               MultiplyAdd1_8q1(bi[b2->tag].force, bi[b2->tag].torque, JJ + 8*ji[i].info.m, lambda+_ofs, ji[i].info.m);
            }
         }
      }
   }
                              //compute the velocity update

                              //add fe to cforce
   for(i=0; i<num_bodies; i++){
      PIPH_body b = body[i];
      S_body_info &bii = bi[i];

      bii.force += b->f_acc;
      bii.torque += b->t_acc;
                              //multiply cforce by stepsize
      bii.force *= stepsize;
      bii.torque *= stepsize;
   //}

   //for(i=0; i<num_bodies; i++){
      //PPIPH_body b = body[i];
                              //add invM * cforce to the body velocity
      dReal body_invMass = b->r_mass;
      //dReal *body_invI = &bi[i].inv_I(0).x;
      const S_matrix &__body_invI = bi[i].__inv_I;

      b->lvel += bi[i].force * body_invMass;
      //dMULTIPLYADD0_331(&b->avel.x, body_invI, &bi[i].torque.x);
      b->avel += bi[i].torque % __body_invI;

                              //update the position and orientation from the new linear/angular velocity
                              // (over the given timestep)
      b->MoveAndRotate(stepsize);

                              //zero force accumulator
      b->f_acc.Zero();
      b->t_acc.Zero();
   }

   delete[] bi;
   delete[] ji;
}

#pragma optimize("", on)

//----------------------------
//----------------------------

PIPH_world IPHCreateWorld(){
   return new IPH_world_imp;
}

//----------------------------
