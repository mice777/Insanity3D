#include "..\common\pch.h"
#include "resource.h"

#include "..\common\common.h"

#define CID_JOINTCREATE  CID_USER + 1

#define JOINT_FP_INTERFACE_ID Interface_ID(0x438aff72, 0xef9675ac)

extern ClassDesc* GetNewBonesDesc();
HINSTANCE hInstance;

//--- Joint Object ------------------------------------------------------------------

#define A_HIDE_JOINT_OBJECT (A_PLUGIN1)

//----------------------------

#define PBLOCK_REF_NO 0

// block IDs
enum{
   jointobj_params
};

extern ParamBlockDesc2 jointobj_param_blk;

//----------------------------

class JointObj: public SimpleObject2{

//----------------------------

   static void MakeQuadFace(Face &f0, Face &f1, DWORD a, DWORD b, DWORD c, DWORD d, DWORD smooth, MtlID matID){

      f0.setVerts (a, d, b);
      f1.setVerts (c, b, d);
      f0.setSmGroup(smooth);
      f1.setSmGroup(smooth);
      f0.setMatID(matID);
      f1.setMatID(matID);
      f0.setEdgeVisFlags(1, 0, 1);
      f1.setEdgeVisFlags(1, 0, 1);
   }

//----------------------------

   void BuildFin(float size, float startTaper, float endTaper, dword i0, dword i1, dword i2, dword i3, dword &curVert,
      dword &curFace){
   
                              //grab the verts of the face we're building the fin off of
      const Point3 &v0 = mesh.verts[i0];
      const Point3 &v1 = mesh.verts[i1];
      const Point3 &v2 = mesh.verts[i2];
      const Point3 &v3 = mesh.verts[i3];
   
                              //compute two perpindicular vectors along the face
      Point3 horizDir = ((v3+v2)*0.5f) - ((v1+v0)*0.5f);
      Point3 vertDir  = ((v1+v2)*0.5f) - ((v3+v0)*0.5f);
   
                              //normal
      Point3 normal = Normalize(CrossProd(horizDir, vertDir));
   
                              //we'll make the border size be 1/3 the height of the face (on the left side)
      float border = Length(v1-v0)/3.0f;
      if(Length(horizDir) < border * 2.0f) {
         border = Length(horizDir) * 0.5f;
      }
      horizDir = Normalize(horizDir);
      vertDir  = Normalize(vertDir);
   
      // Interpolate along the top and bottom edge to get 4 points
      Point3 p0 = v0 + Normalize(v3-v0) * border;
      Point3 p1 = v1 + Normalize(v2-v1) * border;
      Point3 p2 = v2 + Normalize(v1-v2) * border;
      Point3 p3 = v3 + Normalize(v0-v3) * border;
   
      // Now drop down vertically to get the final base points
      Point3 bv0 = p0*2.0f/3.0f + p1/3.0f;
      Point3 bv1 = p1*2.0f/3.0f + p0/3.0f;
      Point3 bv2 = p2*2.0f/3.0f + p3/3.0f;
      Point3 bv3 = p3*2.0f/3.0f + p2/3.0f;
   
                              //save start vert index
      int sv = curVert;
   
      // We'll need edge vectors to taper the end of the fin
      Point3 topEdge = bv2-bv1;
      Point3 botEdge = bv3-bv0;
   
      // Add base verts to array
      mesh.setVert(curVert++, bv0);
      mesh.setVert(curVert++, bv1);
      mesh.setVert(curVert++, bv2);
      mesh.setVert(curVert++, bv3);
   
      // Extrude out in the direction of the normal (and taper)
      mesh.setVert(curVert++, bv0 + normal*size + botEdge*startTaper);
      mesh.setVert(curVert++, bv1 + normal*size + topEdge*startTaper);
      mesh.setVert(curVert++, bv2 + normal*size - topEdge*endTaper);
      mesh.setVert(curVert++, bv3 + normal*size - botEdge*endTaper);
   
      // End
      MakeQuadFace(mesh.faces[curFace], mesh.faces[curFace+1], sv+4, sv+5, sv+6, sv+7, (1<<0), 0);
      curFace += 2;
   
      // Top
      MakeQuadFace(mesh.faces[curFace], mesh.faces[curFace+1], sv+1, sv+2, sv+6, sv+5, (1<<1), 1);
      curFace += 2;
   
      // Right side
      MakeQuadFace(mesh.faces[curFace], mesh.faces[curFace+1], sv+0, sv+1, sv+5, sv+4, (1<<2), 2);
      curFace += 2;
   
      // Left side
      MakeQuadFace(mesh.faces[curFace], mesh.faces[curFace+1], sv+7, sv+6, sv+2, sv+3, (1<<3), 3);
      curFace += 2;
   
      // Bottom
      MakeQuadFace(mesh.faces[curFace], mesh.faces[curFace+1], sv+4, sv+7, sv+3, sv+0, (1<<4), 4);
      curFace += 2;
   }

//----------------------------

   static IObjParam *ip;
   
public:
   JointObj(BOOL loading=false){
      GetNewBonesDesc()->MakeAutoParamBlocks(this);
   }
   
//----------------------------

   RefTargetHandle Clone(RemapDir& remap = NoRemap()){
      JointObj* newob = new JointObj(false);
      newob->ReplaceReference(0, pblock2->Clone(remap));
      newob->ivalid.SetEmpty();  
      return newob;
   }

private:
                              //Animatable methods      
   void DeleteThis(){ delete this; }
   Class_ID ClassID(){ return JOINT_CLASSID; }      
   void BeginEditParams(IObjParam  *ip, ULONG flags, Animatable *prev);
   void EndEditParams(IObjParam *ip, ULONG flags, Animatable *next);    

//----------------------------
//Added by AF (12/01/2000) to support the JointObject filter in schematic view
   SvGraphNodeReference SvTraverseAnimGraph(IGraphObjectManager *gom, Animatable *owner, int id, DWORD flags){

      if(!gom->TestFilter(SV_FILTER_BONES))
         return SvGraphNodeReference();
      return Object::SvTraverseAnimGraph(gom, owner, id, flags);
   }
   
//ReferenceMaker methods:
//----------------------------

   void RescaleWorldUnits(float f){

      if(TestAFlag(A_WORK1))
         return;
   
      SimpleObject2::RescaleWorldUnits(f);
      DbgAssert(TestAFlag(A_WORK1));
   
      DependentIterator iter(this);
      for (ReferenceMaker* mk = iter.Next(); mk != NULL; mk = iter.Next()) {
         if (mk->SuperClassID() == BASENODE_CLASS_ID) break;
      }
      if(mk != NULL){
         INode* node = (INode*)mk;
         TimeValue t = GetCOREInterface()->GetTime();
         node->ResetBoneStretch(t);
      }
   }

//----------------------------
   
   CreateMouseCallBack* GetCreateMouseCallBack(){ return NULL; }

//----------------------------

   TCHAR *GetObjectName(){ return "I3D Joint"; }

//----------------------------

   BOOL HasUVW(){ return false; }

//----------------------------

   //void SetGenUVW(BOOL sw){ }

//----------------------------

   int NumParamBlocks(){ return 1; }
   IParamBlock2 *GetParamBlock(int i){ return pblock2; }
   IParamBlock2 *GetParamBlockByID(BlockID id){ return (pblock2->ID() == id) ? pblock2 : NULL; }
   
//----------------------------
// From SimpleObject
   void BuildMesh(TimeValue t){
#define  ALMOST_ONE (0.999f)

      float width, height, width2, height2, taper, length;
      float endWidth, endHeight, endWidth2, endHeight2;
      float pyrmidHeight;
      //BOOL sideFins = false, frontFin = false, backFin = false, genuv = false;
      int nverts = 9, nfaces = 14;
   
      // Get params from pblock
      ivalid = FOREVER;
      pblock2->GetValue(jointobj_width, t, width, ivalid);
      //pblock2->GetValue(jointobj_height, t, height, ivalid);
      //pblock2->GetValue(jointobj_taper, t, taper, ivalid);
      height = width;
      taper = 90;
      pblock2->GetValue(jointobj_length, t, length, ivalid);
   
      // Don't actually allow numerical 100% taper (due to shading artifacts at point)
      if(taper > ALMOST_ONE)
         taper = ALMOST_ONE;

      nverts += 8;
      nfaces += 10;

      /*
      {
         float sfSize = 0, sfStartTaper = 0, sfEndTaper = 0; // side fins
         float ffSize = 0, ffStartTaper = 0, ffEndTaper = 0; // front fin
         float bfSize = 0, bfStartTaper = 0, bfEndTaper = 0; // back fin

         pblock2->GetValue(jointobj_sidefins, t, sideFins, ivalid);
         if(sideFins){
            pblock2->GetValue(jointobj_sidefins_size, t, sfSize, ivalid);
            pblock2->GetValue(jointobj_sidefins_starttaper, t, sfStartTaper, ivalid);
            pblock2->GetValue(jointobj_sidefins_endtaper, t, sfEndTaper, ivalid);
            nverts += 16;
            nfaces += 20;
            if (sfStartTaper + sfEndTaper > ALMOST_ONE) {
               float d = (ALMOST_ONE - sfStartTaper - sfEndTaper) * 0.5f;
               sfStartTaper += d;
               sfEndTaper   += d;
            }
         }
   
         pblock2->GetValue(jointobj_frontfin, t, frontFin, ivalid);
         if(frontFin) {
            pblock2->GetValue(jointobj_frontfin_size, t, ffSize, ivalid);
            pblock2->GetValue(jointobj_frontfin_starttaper, t, ffStartTaper, ivalid);
            pblock2->GetValue(jointobj_frontfin_endtaper, t, ffEndTaper, ivalid);
            nverts += 8;
            nfaces += 10;
            if (ffStartTaper + ffEndTaper > ALMOST_ONE) {
               float d = (ALMOST_ONE - ffStartTaper - ffEndTaper) * 0.5f;
               ffStartTaper += d;
               ffEndTaper   += d;
            }
         }
   
         pblock2->GetValue(jointobj_backfin, t, backFin, ivalid);
         if (backFin) {
            pblock2->GetValue(jointobj_backfin_size, t, bfSize, ivalid);
            pblock2->GetValue(jointobj_backfin_starttaper, t, bfStartTaper, ivalid);
            pblock2->GetValue(jointobj_backfin_endtaper, t, bfEndTaper, ivalid);
            nverts += 8;
            nfaces += 10;
            if (bfStartTaper + bfEndTaper > ALMOST_ONE) {
               float d = (ALMOST_ONE - bfStartTaper - bfEndTaper) * 0.5f;
               bfStartTaper += d;
               bfEndTaper   += d;
            }
         }

         pblock2->GetValue(jointobj_genmap, t, genuv, ivalid);
      }
      /**/
   
      // Compute tapers 'n stuff
      endWidth     = width * (1.0f-taper);
      endHeight    = height * (1.0f-taper);
      width2       = width*0.5f;
      height2      = height*0.5f;
      endWidth2    = endWidth*0.5f;
      endHeight2   = endHeight*0.5f;
      pyrmidHeight = (width2+height2)*0.5f;
   
   
      mesh.setNumVerts(nverts);
      mesh.setNumFaces(nfaces);
      mesh.setSmoothFlags(0);
      mesh.setNumTVerts (0);
      mesh.setNumTVFaces (0);
   
      mesh.setVert(0, -height2, pyrmidHeight, -width2);
      mesh.setVert(1,  height2, pyrmidHeight, -width2);
      mesh.setVert(2,  height2, pyrmidHeight, width2);
      mesh.setVert(3, -height2, pyrmidHeight, width2);
   
      mesh.setVert(4, -endHeight2, length, -endWidth2);
      mesh.setVert(5,  endHeight2, length, -endWidth2);
      mesh.setVert(6,  endHeight2, length, endWidth2);
      mesh.setVert(7, -endHeight2, length, endWidth2);
   
      mesh.setVert(8, 0.0f,  0.0f, 0.0f);
   
      // End
      MakeQuadFace(mesh.faces[0], mesh.faces[1], 4, 5, 6, 7, (1<<0), 0);
   
      // Top
      MakeQuadFace(mesh.faces[2], mesh.faces[3], 1, 2, 6, 5, (1<<1), 1);
   
      // Right side
      MakeQuadFace(mesh.faces[4], mesh.faces[5], 0, 1, 5, 4, (1<<2), 2);
   
      // Left side
      MakeQuadFace(mesh.faces[6], mesh.faces[7], 7, 6, 2, 3, (1<<3), 3);
   
      // Bottom
      MakeQuadFace(mesh.faces[8], mesh.faces[9], 4, 7, 3, 0, (1<<4), 4);
   
      // Start pyrmid
      mesh.faces[10].setVerts (8, 0, 1);
      mesh.faces[10].setSmGroup(1<<5);
      mesh.faces[10].setMatID(5);
      mesh.faces[10].setEdgeVisFlags(1, 1, 1);
   
      mesh.faces[11].setVerts (8, 1, 2);
      mesh.faces[11].setSmGroup(1<<6);
      mesh.faces[11].setMatID(5);
      mesh.faces[11].setEdgeVisFlags(1, 1, 1);
   
      mesh.faces[12].setVerts (8, 2, 3);
      mesh.faces[12].setSmGroup(1<<7);
      mesh.faces[12].setMatID(5);
      mesh.faces[12].setEdgeVisFlags(1, 1, 1);
   
      mesh.faces[13].setVerts (8, 3, 0);
      mesh.faces[13].setSmGroup(1<<8);
      mesh.faces[13].setMatID(5);
      mesh.faces[13].setEdgeVisFlags(1, 1, 1);
   
      // This is how many verts and faces we've made so far
      dword curVert = 9;
      dword curFace = 14;
      /*

      // Optionally build fins
      if(sideFins) {
         BuildFin(sfSize, sfStartTaper, sfEndTaper, 0, 1, 5, 4, curVert, curFace);
         BuildFin(sfSize, sfStartTaper, sfEndTaper, 2, 3, 7, 6, curVert, curFace);
      }
      if (frontFin) {
         BuildFin(ffSize, ffStartTaper, ffEndTaper, 1, 2, 6, 5, curVert, curFace);
      }
      if (backFin) {
         BuildFin(bfSize, bfStartTaper, bfEndTaper, 3, 0, 4, 7, curVert, curFace);
      }
   
      if(genuv){
         mesh.ApplyUVWMap(MAP_FACE,
            1.0f, 1.0f, 1.0f, // tile
            false, false, false, // flip
            false, // cap
            Matrix3(1));
      }
      */
      {
         const float ff_size = width * .3f;
         const float ff_start = .5f;
         const float ff_end = .1f;
         //nverts += 8;
         //nfaces += 10;
         /*
         if(ff_start + ff_end > ALMOST_ONE){
            float d = (ALMOST_ONE - ff_start - ff_end) * 0.5f;
            ff_start += d;
            ff_end += d;
         }
         */
         BuildFin(ff_size, ff_start, ff_end, 7, 6, 2, 3, curVert, curFace);
      }
   
      // Invalidate caches
      mesh.InvalidateTopologyCache();
   }

//----------------------------

   BOOL OKtoDisplay(TimeValue t){ return !TestAFlag(A_HIDE_JOINT_OBJECT); }

//----------------------------

   void InvalidateUI(){
      // if this was caused by a NotifyDependents from pblock2, LastNotifyParamID()
      // will contain ID to update, else it will be -1 => inval whole rollout
      jointobj_param_blk.InvalidateUI(pblock2->LastNotifyParamID());
   }
};

IObjParam *JointObj::ip = NULL;

//----------------------------

class C_JointsCreateMode: public CommandMode{

   class JointsCreationManager: public MouseCallBack, ReferenceMaker{
//----------------------------

      static Matrix3 ComputeExtraBranchLinkPosition(TimeValue t,INode *parNode, Matrix3 constTM){

         Point3 averageChildBonePos(0, 0, 0);
         int ct=0;

         for (int i=0; i<parNode->NumberOfChildren(); i++) {
            INode *childNode = parNode->GetChildNode(i);
            if (childNode->GetBoneNodeOnOff()) {
               averageChildBonePos += childNode->GetNodeTM(t).GetTrans();
               ct++;
            }
         }
         if (ct>0) {
            // Position the construction plane at the average position of children
            constTM.SetTrans(averageChildBonePos/float(ct));
         } else {    
            // No children. Position the construction plane at the end of the last bone.
            Matrix3 potm = parNode->GetObjectTM(t);      
            Object *obj = parNode->GetObjectRef();
            if(obj->ClassID()==JOINT_CLASSID){
               JointObj *bobj = (JointObj*)obj;
               float length;
               bobj->pblock2->GetValue(jointobj_length, t, length, FOREVER);
               constTM.SetTrans(potm.GetTrans() + length * potm.GetRow(0));
            }     
         }
         return constTM;
      }

//----------------------------

      static void SetParentJointAlignment(TimeValue t, INode *parNode, INode *childNode, Matrix3 &constTM){
         //
         // Setup the alignment on the parent node's TM.
         // The X axis points to the child
         // The Z axis is parallel to the construction plane normal
         // The Y axis is perpindicular to X and Z
         //
         Matrix3 ptm = parNode->GetNodeTM(t);
         Matrix3 ntm = childNode->GetNodeTM(t);
         Point3 xAxis = Normalize(ntm.GetTrans()-ptm.GetTrans());
         Point3 zAxis = constTM.GetRow(2);
         Point3 yAxis = Normalize(CrossProd(zAxis, xAxis));
   
         // RB 12/14/2000: Will fix 273660. If the jointss are being created off the
         // construction plane (3D snap), the Z axis may not be perpindicular to X
         // so we need to orthogonalize.
         zAxis = Normalize(CrossProd(xAxis, yAxis));
         yAxis = Normalize(CrossProd(zAxis, xAxis));
   
         ptm.SetRow(0, -yAxis);
         ptm.SetRow(1, xAxis);
         ptm.SetRow(2, zAxis);
   
         SuspendSetKeyMode();
   
         // Plug in the TMs.
         parNode->SetNodeTM(t, ptm);
         childNode->SetNodeTM(t, ntm);
   
         ResumeSetKeyMode();
      }

   //----------------------------

      static void SetHideJointObject(INode *node, BOOL onOff){

         if(!node)
            return;
         Object *obj = node->GetObjectRef();
         if(obj && obj->ClassID()==JOINT_CLASSID){
            if(onOff)
               obj->SetAFlag(A_HIDE_JOINT_OBJECT);
            else
               obj->ClearAFlag(A_HIDE_JOINT_OBJECT);
         }
      }

   //----------------------------

      static int DSQ(IPoint2 p, IPoint2 q){
         return (p.x-q.x)*(p.x-q.x)+(p.y-q.y)*(p.y-q.y);
      }

   //----------------------------

      friend class PickAutoJoint;
      friend class C_JointClassDesc;
      friend class C_NewJointClassDesc;

      class JointsPicker: public PickNodeCallback{

         BOOL Filter(INode *node){
            return node->GetBoneNodeOnOff();
#if 0
            Object* obj = node->GetObjectRef();
            if (obj && obj->SuperClassID()==HELPER_CLASS_ID && obj->ClassID()==Class_ID(BONE_CLASS_ID,0))
               return 1;
            return 0;
#endif
         }
      };

   private:
      CreateMouseCallBack *createCB;      
      INode *curNode, *lastNode, *firstChainNode;
      IObjCreate *createInterface;
      ClassDesc *cDesc;
      Matrix3 mat;  // the nodes TM relative to the CP
      IPoint2 lastpt;
      int ignoreSelectionChange;
      WNDPROC suspendProc;
      Object *editJointObj;
      Tab<INode*> newBoneChain;
   
      int NumRefs(){ return 1; }

   //----------------------------

      RefTargetHandle JointsCreationManager::GetReference(int i){

         switch(i){
         case 0: return (RefTargetHandle)curNode;
         default: assert(0); 
         }
         return NULL;
      }

   //----------------------------

      void SetReference(int i, RefTargetHandle rtarg){
         switch(i) {
         case 0: curNode = (INode*)rtarg; break;
         default: assert(0); 
         }
      }
   
   //----------------------------
   // StdNotifyRefChanged calls this, which can change the partID to new value 
   // If it doesnt depend on the particular message& partID, it should return REF_DONTCARE
      RefResult NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message){
         switch (message) {
         case REFMSG_TARGET_SELECTIONCHANGE:
            if(ignoreSelectionChange){
               break;
            }
            if(curNode==hTarget){
               // this will set curNode== NULL;
               DeleteReference(0);
               goto endEdit;
            }
            // fall through
         
         case REFMSG_TARGET_DELETED:      
            if(curNode==hTarget){           
   endEdit:
               curNode = NULL;
            }
            break;      
         }
         return REF_SUCCEED;
      }
   
   //----------------------------

   public:

      void Begin(IObjCreate *ioc, ClassDesc *desc){
         createInterface = ioc;
         cDesc           = desc;
         createCB        = NULL;
         curNode        = NULL;
         lastNode        = NULL;
         firstChainNode  = NULL;
      
         theHold.Suspend();
         editJointObj = CreateNewJointObject();
         if(editJointObj)
            editJointObj->BeginEditParams(ioc, BEGIN_EDIT_CREATE);
         theHold.Resume();
      }
   
   //----------------------------

      void End(){
         if (editJointObj) {
            // Stop editing and dump object
            theHold.Suspend();
            editJointObj->EndEditParams(createInterface, END_EDIT_REMOVEUI);
            editJointObj->DeleteThis();
            editJointObj = NULL;
            theHold.Resume();
         }
      
         createInterface->ClearPickMode();
         if(curNode) 
            DeleteReference(0);  // sets curNode = NULL  
      }

   //----------------------------

      Object *CreateNewJointObject(){

         macroRec->BeginCreate(GetNewBonesDesc());
         SuspendSetKeyMode();
         JointObj *obj = (JointObj*)createInterface->CreateInstance(JOINT_SUPERCLASSID, JOINT_CLASSID);
         ResumeSetKeyMode();
         return obj;
      }

   //----------------------------

      void SetupObjectTM(INode *node){
         //node->SetObjOffsetRot(QFromAngAxis(HALFPI, Point3(0,-1,0)));
      }

   //----------------------------

      void UpdateBoneLength(INode *parNode){

         Point3 pos(0, 0, 0);
         int ct = 0;
   
                                    //compute the average bone child pos
         for(int i=0; i<parNode->NumberOfChildren(); i++){
            INode *cnode = parNode->GetChildNode(i);
            if(cnode->GetBoneNodeOnOff()){
               pos += cnode->GetNodeTM(createInterface->GetTime()).GetTrans();
               ct++;
            }
         }
   
         SuspendSetKeyMode();
   
         if(ct){
            // To get the bone length, we transform the average child position back
            // into parent space. We then use the distance along the parent Y axis
            // for bone length 
            pos /= float(ct);
            Matrix3 ntm = parNode->GetNodeTM(createInterface->GetTime());     
            parNode->SetNodeTM(createInterface->GetTime(), ntm);     
            pos = pos * Inverse(ntm);
            float len = pos.y;
      
            // Plug the new length into the object
            Object *obj = parNode->GetObjectRef();
            if(obj && 
               obj->ClassID()==JOINT_CLASSID && 
               obj->SuperClassID()==JOINT_SUPERCLASSID){
               JointObj *bobj = (JointObj*)obj;
               bobj->pblock2->SetValue(jointobj_length, TimeValue(0), len);
            }
         }else{
            // No children. Bone should appear as a nub.
         }
   
         // During creation we are going to set the length directly and keep the stretch factor at 0.
         // After creation, changes in bone length will be accomplished by stretching.
         parNode->ResetBoneStretch(createInterface->GetTime());   
   
         ResumeSetKeyMode();
      }

   //----------------------------

      void UpdateBoneCreationParams(){
         if(editJointObj){
            // This causes the displayed parameters to be copied to the class parameters so the
            // next bone object created will use the updated params.
            editJointObj->EndEditParams(createInterface, 0);
            editJointObj->BeginEditParams(createInterface, BEGIN_EDIT_CREATE);
         }
      }

   //----------------------------

      JointsCreationManager()
      {
         ignoreSelectionChange = false;
         editJointObj  = NULL;
         suspendProc = NULL;
      }

   //----------------------------

      int proc(HWND hwnd, int msg, int point, int flag, IPoint2 m){
   
         int res = 0;
         INode *newNode, *parNode;   
         JointsPicker bonePick;
         ViewExp *vpx = createInterface->GetViewport(hwnd);
         assert(vpx);
         Matrix3 constTM;
         vpx->GetConstructionTM(constTM);
         Point3 boneColor = GetUIColor(COLOR_BONES);
         static float constPlaneZOffset = 0.0f;
         TimeValue t = createInterface->GetTime();
   
         switch(msg){
      
         case MOUSE_POINT:
            {
               Object *ob;
               if (point==0) {
                  newBoneChain.SetCount(0);
                  SuspendAnimate();
                  SuspendSetKeyMode();
                  AnimateOff();
                  MouseManager* mm = createInterface->GetMouseManager();
                  if (mm != NULL) {
                     // This is the additional window proc that intercepts
                     // the mouse event for the manipulator. It interferes
                     // with bone creation. We temporarily suspend it.
                     suspendProc = mm->GetMouseWindProcCallback();
                     mm->SetMouseWindProcCallback(NULL);
                  }
            
                  mat.IdentityMatrix();
                  if ( createInterface->SetActiveViewport(hwnd) ) {
                     return false;
                  }
            
                  // RB 7/28/00: Wait for first mouse up to avoid making small bones.
                  res = true;
                  goto done;
            
               }else{
                  if(DSQ(m,lastpt)<10){
                     res = true;
                     goto done;
                  }
                  theHold.SuperAccept(IDS_DS_CREATE);
               }
         
               if (createInterface->IsCPEdgeOnInView()) { 
                  res = false;
                  goto done;
               }
         
         
               // Make sure the next bone created uses the creation parameters displayed
               // by the bone object UI in the command panel.
               UpdateBoneCreationParams();
         
               theHold.SuperBegin();    // begin hold for undo
               theHold.Begin();
         
               //mat.IdentityMatrix();
               mat = constTM;
      #ifdef _3D_CREATE
               mat.SetTrans(constTM*vpx->SnapPoint(m,m,NULL,SNAP_IN_3D));
      #else
               //mat.SetTrans(vpx->SnapPoint(m,m,NULL,SNAP_IN_PLANE));
               mat.SetTrans(constTM*vpx->SnapPoint(m,m,NULL,SNAP_IN_PLANE));
      #endif
         
               BOOL newParent = false, overB=false;
               if (curNode==NULL)   {
                  constPlaneZOffset = 0.0f;
                  INode* overBone = createInterface->PickNode(hwnd,m,&bonePick);
                  if (overBone) {
               
                     // Compute the Z distance of the existing bone the user clicked on from
                     // the current construction plane. We'll add this offset to every new bone
                     // created in this chain.
                     Matrix3 extraBranchTM = ComputeExtraBranchLinkPosition(t,overBone,mat);
                     constPlaneZOffset = (Inverse(constTM) * extraBranchTM.GetTrans()).z;
               
                     // We don't want 'mat' to get offset twice so we'll save and restore it.
                     Matrix3 oldMat = mat;
                     mat.SetTrans(mat.GetTrans() + constPlaneZOffset*constTM.GetRow(2));
               
                     // Instead of just linking newly created bones to the existing parent,
                     // insert an extra link under the existing parent bone and position this
                     // extra link at the average child position of the existing parent bone.               
                     ob = CreateNewJointObject();
                     parNode = createInterface->CreateObjectNode(ob);
                     //watje
                     for (int i=0; i < GetCOREInterface()->GetNumberDisplayFilters(); i++)
                     {
                        if (GetCOREInterface()->DisplayFilterIsNodeVisible(i,ob->SuperClassID(), ob->ClassID(), parNode))
                        {
                           GetCOREInterface()->SetDisplayFilter(i, false );
                        }
                     }
               
                     newBoneChain.Append(1,&parNode);
                     parNode->SetWireColor(RGB(int(boneColor.x*255.0f), int(boneColor.y*255.0f), int(boneColor.z*255.0f) ));
                     parNode->SetNodeTM(0,extraBranchTM);               
                     macroRec->SetNodeTM(parNode, extraBranchTM);
               
                     SetupObjectTM(parNode);
                     parNode->SetBoneNodeOnOff(true, t);
                     parNode->SetRenderable(false);
                     //parNode->ShowBone(1); // Old bones on by default
                     overBone->AttachChild(parNode);                             
                     // RB 12/13/2000: Don't want to align the pre-existing bone
                     //SetParentBoneAlignment(t, overBone, parNode, constTM);
                     UpdateBoneLength(overBone);
               
                     newParent = true;
               
                     firstChainNode = parNode;
               
                     mat = oldMat;
               
                     //parNode = overBone;               
                     overB = true;
                  } else {
                     // Make first node 
                     //ob = (Object *)createInterface->
                     // CreateInstance(HELPER_CLASS_ID,Class_ID(BONE_CLASS_ID,0));              
                     ob = CreateNewJointObject();
                     parNode = createInterface->CreateObjectNode(ob);
                     //watje
                     for (int i=0; i < GetCOREInterface()->GetNumberDisplayFilters(); i++)
                     {
                        if (GetCOREInterface()->DisplayFilterIsNodeVisible(i,ob->SuperClassID(), ob->ClassID(), parNode))
                        {
                           GetCOREInterface()->SetDisplayFilter(i, false );
                        }
                     }
               
                     newBoneChain.Append(1,&parNode);
               
                     //createInterface->SetNodeTMRelConstPlane(parNode, mat);                            
                     parNode->SetWireColor(RGB(int(boneColor.x*255.0f), int(boneColor.y*255.0f), int(boneColor.z*255.0f) ));
                     parNode->SetNodeTM(0,mat);             
                     macroRec->SetNodeTM(parNode, mat);
               
                     SetupObjectTM(parNode);
                     parNode->SetBoneNodeOnOff(true, t);
                     parNode->SetRenderable(false);
                     //parNode->ShowBone(1); // Old bones on by default
                     newParent = true;
               
                     firstChainNode = parNode;
                  }
                  //parNode->ShowBone(1);
            
                  // RB 5/10/99
                  lastNode = parNode;           
            
               } else {
                  lastNode = parNode = curNode;
                  DeleteReference(0);
               }
         
               // Make new node 
               //ob = (Object *)createInterface->
               // CreateInstance(HELPER_CLASS_ID,Class_ID(BONE_CLASS_ID,0));        
               ob = CreateNewJointObject();
               newNode = createInterface->CreateObjectNode(ob);         
               newBoneChain.Append(1,&newNode);
               SetupObjectTM(newNode);
               newNode->SetBoneNodeOnOff(true, t);
               newNode->SetWireColor(RGB(int(boneColor.x*255.0f), int(boneColor.y*255.0f), int(boneColor.z*255.0f) ));
               newNode->SetRenderable(false);
               //newNode->ShowBone(1); // Old bones on by default
         
               //createInterface->SetNodeTMRelConstPlane(newNode, mat);
               mat.SetTrans(mat.GetTrans() + constPlaneZOffset*constTM.GetRow(2));        
               newNode->SetNodeTM(0,mat);                   
               macroRec->SetNodeTM(newNode, mat);
         
               parNode->AttachChild(newNode); // make node a child of prev 
         
               // Align the parent bone to point at the child bone
               if(overB && parNode){
                  SetParentJointAlignment(t, parNode, newNode, constTM);
                  UpdateBoneLength(parNode);
               }
         
               curNode = newNode;
               SetHideJointObject(curNode, true);
         
               // Reference the new node so we'll get notifications.
               MakeRefByID( FOREVER, 0, curNode);

               ignoreSelectionChange = true;
               createInterface->SelectNode( curNode);
               ignoreSelectionChange = false;
         
               createInterface->RedrawViews(t);  
               theHold.Accept(IDS_DS_CREATE);    
               lastpt = m;
               res = true; 
            }
            break;
      
         case MOUSE_MOVE:
            // The user can loose capture by switching to another app.
            if(!GetCapture()){
               theHold.SuperAccept(IDS_DS_CREATE);
               macroRec->EmitScript();
               return false;
            }
      
            if(curNode){
         
               // Hide the bone floating under the cursor during creation.
               SetHideJointObject(curNode, true);
               SetHideJointObject(lastNode, false);          
         
      #ifdef _3D_CREATE
               mat.SetTrans(constTM * vpx->SnapPoint(m, m, NULL, SNAP_IN_3D));
      #else
               //mat.SetTrans(vpx->SnapPoint(m,m,NULL,SNAP_IN_PLANE));
               mat.SetTrans(constTM*vpx->SnapPoint(m, m, NULL, SNAP_IN_PLANE));
      #endif
               mat.SetTrans(mat.GetTrans() + constPlaneZOffset * constTM.GetRow(2));
         
               //createInterface->SetNodeTMRelConstPlane(curNode, mat);
               curNode->SetNodeTM(t, mat);
               macroRec->SetNodeTM(curNode, mat);
         
               if(lastNode){
                  SetParentJointAlignment(t, lastNode, curNode, constTM);
                  UpdateBoneLength(lastNode);
               }
         
               createInterface->RedrawViews(t);
            }
            res = true;
            break;
      
         case MOUSE_FREEMOVE:
            {
               INode* overNode = createInterface->PickNode(hwnd,m,&bonePick);
               if(overNode){
                  SetCursor(LoadCursor(hInstance, MAKEINTRESOURCE(IDC_CROSS_HAIR)));
               }else{
                  SetCursor(LoadCursor(NULL, IDC_ARROW));
               }
      #ifdef _OSNAP
      #ifdef _3D_CREATE
               vpx->SnapPreview(m,m,NULL, SNAP_IN_3D);
      #else
               vpx->SnapPreview(m,m,NULL, SNAP_IN_PLANE);
      #endif
      #endif
            }
            break;
      
            // mjm - 3.1.99
         case MOUSE_PROPCLICK:
            // right click while between creations
            createInterface->RemoveMode(NULL);
            break;
            // mjm - end
      
         case MOUSE_ABORT:
            if(curNode){
               theHold.SuperCancel(); // this deletes curNode and everything (and sets curNode to NULL)
               curNode = NULL;            
         
               INode *oldCurNode = curNode;
                                    //delete the last two bones
               INode *deletedBone = NULL;
               if (newBoneChain.Count()>2) {                            
                  deletedBone = newBoneChain[newBoneChain.Count()-2];
                  if (deletedBone!=oldCurNode && deletedBone!=firstChainNode) {                             
                     theHold.Begin();
                     if (lastNode==deletedBone) {
                        if (lastNode!=firstChainNode) {
                           lastNode = lastNode->GetParentNode();
                        } else {
                           lastNode       = NULL;
                           firstChainNode = NULL;
                        }
                     }
                     createInterface->DeleteNode(deletedBone, false);
               
                     // Also select last node
                     if (lastNode) {
                        ignoreSelectionChange = true;
                        createInterface->SelectNode(lastNode);
                        ignoreSelectionChange = false;
                     }
               
                     theHold.Accept("Delete Joint");
                  }
               } else {
                  // If only two or less bones are created, then the whole chain creation is
                  // being canceled.
                  firstChainNode = NULL;
               }
         
               // firstChainNode may have just been deleted if we aborted before
               // the first two nodes have been placed. In this case the creation
               // of the whole bone chain is being canceled.
               //if (firstChainNode==oldCurNode || firstChainNode==deletedBone) {
               // firstChainNode = NULL;
               // }
               //curNode = NULL;  
         
               macroRec->EmitScript();
               createInterface->RedrawViews(t); 
            }
      
            if (suspendProc != NULL) {
               createInterface->GetMouseManager()->SetMouseWindProcCallback(suspendProc);
               suspendProc = NULL;
            }
            ResumeSetKeyMode();
            ResumeAnimate();
            res = false;
            break;
         }
   
      done:
         createInterface->ReleaseViewport(vpx); 
         return res;
      }
   };
public:
   JointsCreationManager proc;
   void Begin(IObjCreate *ioc, ClassDesc *desc){ proc.Begin( ioc, desc ); }
   void End(){ proc.End(); }
private:   
   int Class(){ return CREATE_COMMAND; }
   int ID(){ return CID_JOINTCREATE; }
   MouseCallBack *MouseProc(int *numPoints){ *numPoints = 1000000; return &proc; }
   ChangeForegroundCallback *ChangeFGProc(){ return CHANGE_FG_SELECTED; }
   BOOL ChangeFG(CommandMode *oldMode){ return (oldMode->ChangeFGProc() != CHANGE_FG_SELECTED); }
   void EnterMode(){}
   void ExitMode(){}
   BOOL IsSticky(){ return false; }
};
   
//----------------------------
//----------------------------

class C_NewJointClassDesc: public ClassDesc2{
   C_JointsCreateMode JointsCreateMode;

   int IsPublic(){ return true; }
   void *Create(BOOL loading = false) { return new JointObj(loading); }
   const TCHAR *ClassName(){ return "Joint"; }
//----------------------------

   int BeginCreate(Interface *i){
      IObjCreate *iob = i->GetIObjCreate();
   
      JointsCreateMode.Begin(iob, this);
      iob->PushCommandMode(&JointsCreateMode);
      return true;
   }

//----------------------------

   int EndCreate(Interface *i){
   
      JointsCreateMode.End();
      i->RemoveMode(&JointsCreateMode);
      return true;
   }

//----------------------------
   SClass_ID SuperClassID(){ return JOINT_SUPERCLASSID; }
   Class_ID ClassID(){ return JOINT_CLASSID; }
   const TCHAR *Category() { return _T("");}
   const TCHAR *InternalName() {return _T("JointObj");} // returns fixed parsable name (scripter-visible name)
   HINSTANCE HInstance() {return hInstance;}        // returns owning module handle

};

//----------------------------
   
static C_NewJointClassDesc NewJointClassDesc;
ClassDesc *GetNewBonesDesc(){ return &NewJointClassDesc; }
   
//----------------------------
//--- Publish bone creation -------------------------------------------------------------------
   
class C_BoneFunctionPublish: public FPStaticInterface{
   DECLARE_DESCRIPTOR(C_BoneFunctionPublish);
   
   enum OpID {
      kBoneSegmentCreate
   };
   
   BEGIN_FUNCTION_MAP
   FN_3(kBoneSegmentCreate, TYPE_INODE, CreateBoneSegment, TYPE_POINT3, TYPE_POINT3, TYPE_POINT3)
   END_FUNCTION_MAP
      
//----------------------------

   INode *CreateBoneSegment(Point3 startPos, Point3 endPos, Point3 zAxis){

      TimeValue t = GetCOREInterface()->GetTime();
   
      // Figure out length and build node TM 
      Point3 xAxis  = endPos-startPos;
      float length = Length(xAxis);
      if (fabs(length) > 0.0f) {
         xAxis = xAxis/length;
      } else {
         xAxis = Point3(1,0,0);
      }
      Point3 yAxis = Normalize(CrossProd(zAxis, xAxis));
      zAxis = Normalize(CrossProd(xAxis,yAxis));
      Matrix3 tm(xAxis, yAxis, zAxis, startPos);
   
      SuspendAnimate();
      SuspendSetKeyMode();
      AnimateOff();
   
      // Make the new object and set its length
      JointObj *newObj = new JointObj(false);  
      newObj->pblock2->SetValue(jointobj_length, t, length);
   
      // Make the node for the object
      INode *newNode = GetCOREInterface()->CreateObjectNode(newObj);
      Point3 jointColor = GetUIColor(COLOR_BONES);
   
      // Set node params
      newNode->SetWireColor(RGB(int(jointColor.x*255.0f), int(jointColor.y*255.0f), int(jointColor.z*255.0f)));
      newNode->SetNodeTM(t, tm);
      newNode->SetBoneNodeOnOff(true, t);
      newNode->SetRenderable(false);
      //newNode->ShowBone(1);
   
      ResumeSetKeyMode();
      ResumeAnimate();
   
      return newNode;
   }
};

//----------------------------

static C_BoneFunctionPublish theBoneFunctionPublish(
   JOINT_FP_INTERFACE_ID, _T("JointSys"), -1, 0, FP_CORE,
                              //the first operation, boneCreate:
   C_BoneFunctionPublish::kBoneSegmentCreate, _T("createJoint"), -1, TYPE_INODE, 0, 3,
                              //argument 1, 2, 3:
   _T("startPos"), -1, TYPE_POINT3,
   _T("endPos"), -1, TYPE_POINT3,
   _T("zAxis"), -1, TYPE_POINT3,
   end);
   
//----------------------------
//----------------------------

// per instance block
ParamBlockDesc2 jointobj_param_blk(
  jointobj_params, _T("JointObjectParameters"),  0, &NewJointClassDesc, P_AUTO_CONSTRUCT+P_AUTO_UI, PBLOCK_REF_NO,
  
                              //rollout
  IDD_NEWJOINT, IDS_RB_BONEPARAMS, 0, 0, NULL,
  
                              //params
  jointobj_width, _T("width"), TYPE_WORLD, P_ANIMATABLE, IDS_BONE_WIDTH,
  p_default,     .1f, 
  p_ms_default,  .1f,
  p_range,       0.0f, float(1.0E30), 
  p_ui,          TYPE_SPINNER, EDITTYPE_UNIVERSE, IDC_BONE_WIDTH, IDC_BONE_WIDTHSPIN, SPIN_AUTOSCALE, 
  end, 
  
  jointobj_length, _T("length"), TYPE_FLOAT, P_RESET_DEFAULT, IDS_BONE_LENGTH,
  p_default,     .5f,
  p_ms_default,  .5f,
  p_range,       0.0, float(1.0E30),
  p_ui,          TYPE_SPINNER, EDITTYPE_FLOAT, IDC_BONE_LENGTH, IDC_BONE_LENGTHSPIN, 0.01,
  end, 
  
  end
  );
                                         
//----------------------------
                                         
class JointObjUserDlgProc: public ParamMap2UserDlgProc{

                              //refinement class - used for splitting existing joint to halh
   class PickBoneRefineMode: public PickModeCallback, public PickNodeCallback{
   public:     
      JointObjUserDlgProc *dlg;
      INode *pickedNode;
      float refinePercentage;
     
      PickBoneRefineMode() {dlg=NULL;pickedNode=NULL;}

   //----------------------------

      BOOL HitTest(IObjParam *ip,HWND hWnd,ViewExp *vpt,IPoint2 m,int flags){
         pickedNode = ip->PickNode(hWnd,m);
         if (pickedNode) {
            Object *nobj = pickedNode->GetObjectRef();      
            if(nobj) nobj = nobj->FindBaseObject();
            if(nobj && nobj->ClassID()==JOINT_CLASSID){
            
               // Get the bone object's TM
               Matrix3 otm = pickedNode->GetObjectTM(ip->GetTime());
               Point3 mousePoint(m.x, m.y, 0);
            
               // Find the two ends of the bone in world space
               JointObj *bobj = (JointObj*)nobj;
               float length;
               bobj->pblock2->GetValue(jointobj_length, ip->GetTime(), length, FOREVER);
               Point3 pt0 = otm.GetTrans();
               Point3 pt1 = Point3(length, 0.0f, 0.0f) * otm;
            
               // Transform into screen space
               GraphicsWindow *gw = vpt->getGW();
               gw->setTransform(Matrix3(1));
               gw->transPoint(&pt0, &pt0);
               gw->transPoint(&pt1, &pt1);
            
               // Don't care about Z
               pt0.z = 0.0f;
               pt1.z = 0.0f;
            
               // Project mouse point onto bone axis
               Point3 baxis = Normalize(pt1-pt0);
               Point3 mvect = mousePoint-pt0;
               refinePercentage = DotProd(mvect, baxis)/Length(pt1-pt0);
            
               // We may have missed the bone
               if (refinePercentage < 0.0f || refinePercentage > 1.0f) {
                  return false;
               }
            }
         }
         return pickedNode? true: false;
      }

   //----------------------------

      BOOL Pick(IObjParam *ip,ViewExp *vpt){
         if(pickedNode)
            dlg->RefineSegment(pickedNode,refinePercentage);
         pickedNode = NULL;
         return false;
      }

   //----------------------------

      void EnterMode(IObjParam *ip){
         if(dlg->iRefine)
            dlg->iRefine->SetCheck(true);
      }

   //----------------------------

      void ExitMode(IObjParam *ip){
         if(dlg->iRefine)
            dlg->iRefine->SetCheck(false);
      }

   //----------------------------

      BOOL RightClick(IObjParam *ip,ViewExp *vpt){ return true; }

   //----------------------------

      BOOL PickBoneRefineMode::Filter(INode *node){
         Object *nobj = node->GetObjectRef();
         if (nobj) {
            nobj = nobj->FindBaseObject();
            if(nobj && nobj->ClassID()==JOINT_CLASSID){
               return true;
            }
         }
         return false;
      }

   //----------------------------

      PickNodeCallback *GetFilter() {return this; }
   };
   PickBoneRefineMode refineMode;

   JointObj *bo;
   BOOL inCreate;
public:
   ICustButton *iRefine;

//----------------------------
     
   BOOL DlgProc(TimeValue t, IParamMap2 *map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
      switch (msg) {
      case WM_INITDIALOG:        
         iRefine = GetICustButton(GetDlgItem(hWnd, IDC_BONE_REFINE));
         if(!inCreate) iRefine->Enable();
         else iRefine->Disable();
         iRefine->SetType(CBT_CHECK);
         iRefine->SetHighlightColor(GREEN_WASH);
         break;
         
      case WM_COMMAND:
         switch (LOWORD(wParam)) {
         case IDC_BONE_REFINE:
            GetCOREInterface()->SetPickMode(&refineMode);
            return true;
         }
         break;
      }
      return false;
   }

//----------------------------

   void DeleteThis(){ delete this; }

//----------------------------

   JointObjUserDlgProc(JointObj *b, BOOL ic):
      bo(b),
      inCreate(ic),
      iRefine(NULL)

   {
      refineMode.dlg = this;
   }
   ~JointObjUserDlgProc(){
      ReleaseICustButton(iRefine);
      //iRefine=NULL;
   }

//----------------------------

   INode *GetCurBoneNode() const{
      if (GetCOREInterface()->GetSelNodeCount()!=1) return NULL;
      INode *selNode = GetCOREInterface()->GetSelNode(0);
      Object *obj = selNode->GetObjectRef();
      if(obj->ClassID()==JOINT_CLASSID)
         return selNode;
      return NULL;
   }

//----------------------------

   void RefineSegment(INode *parNode, float refinePercentage){

      TimeValue t = GetCOREInterface()->GetTime();
      float oneMinusRefinePercentage = 1.0f - refinePercentage;
     
      // Find the bone node we're refining and get at its object
      //INode *parNode = GetCurBoneNode();
      Object *obj = parNode->GetObjectRef();
      if(obj->ClassID()!=JOINT_CLASSID)
         return;
      JointObj *bobj = (JointObj*)obj;
     
     // Determine the position for the new node.
     float length;
     bobj->pblock2->GetValue(jointobj_length, t, length, FOREVER);
     Matrix3 otm   = parNode->GetObjectTM(t);
     Point3 pos    = otm * Point3(length * refinePercentage, 0.0f, 0.0f);
     Matrix3 newTM = parNode->GetNodeTM(t);
     newTM.SetTrans(pos);
     
     // Begin holding and turn animating off
     theHold.Begin();
     SuspendAnimate();
     AnimateOff();
     
     // Shrink the existing bone object
     bobj->pblock2->SetValue(jointobj_length, t, length * refinePercentage);
     
     // Make the new object (as a clone of the bone we're refining)
     RemapDir *rmap = NewRemapDir();
     JointObj *newObj = (JointObj*)bobj->Clone(*rmap);
     rmap->DeleteThis();
     
     // Setup the bone node
     newObj->pblock2->SetValue(jointobj_length, t, length * oneMinusRefinePercentage);
     INode *newNode = GetCOREInterface()->CreateObjectNode(newObj);
     Point3 boneColor = GetUIColor(COLOR_BONES);
     newNode->SetWireColor(RGB(int(boneColor.x*255.0f), int(boneColor.y*255.0f), int(boneColor.z*255.0f) ));
     newNode->SetNodeTM(t, newTM);
     newNode->SetBoneNodeOnOff(true, t);
     newNode->SetRenderable(false);
     //newNode->ShowBone(1);
     
     // Copy bone params
     newNode->SetBoneAutoAlign(parNode->GetBoneAutoAlign());
     newNode->SetBoneFreezeLen(parNode->GetBoneFreezeLen());
     newNode->SetBoneScaleType(parNode->GetBoneScaleType());
     newNode->SetBoneAxis(parNode->GetBoneAxis());
     newNode->SetBoneAxisFlip(parNode->GetBoneAxisFlip());
     
     // Copy material and wire frame colors
     newNode->SetMtl(parNode->GetMtl());
     newNode->SetWireColor(parNode->GetWireColor());
     
     // Attach children of parent to new node
     for(int i=0; i<parNode->NumberOfChildren(); i++) {
        INode *cnode = parNode->GetChildNode(i);
        newNode->AttachChild(cnode);
     }
     
     // Attach new node to parent
     parNode->AttachChild(newNode);
     
     // re-align and reset stretch
     parNode->RealignBoneToChild(t);
     newNode->RealignBoneToChild(t);
     parNode->ResetBoneStretch(t);
     newNode->ResetBoneStretch(t);
     
     parNode->InvalidateTreeTM();
     
     ResumeAnimate();
     theHold.Accept("Refine Joint");
     GetCOREInterface()->RedrawViews(t);
   }
};

//----------------------------
   
void JointObj::BeginEditParams(IObjParam  *ip, ULONG flags, Animatable *prev){

   SimpleObject::BeginEditParams(ip, flags, prev);
   this->ip = ip;
   NewJointClassDesc.BeginEditParams(ip, this, flags, prev);   
   jointobj_param_blk.SetUserDlgProc(new JointObjUserDlgProc(this,(flags&BEGIN_EDIT_CREATE)?true:false));
}

//----------------------------
   
void JointObj::EndEditParams(IObjParam *ip, ULONG flags, Animatable *next){

   ip->ClearPickMode();
   SimpleObject::EndEditParams(ip, flags, next);
   this->ip = NULL;
   NewJointClassDesc.EndEditParams(ip, this, flags, next);
}
   
//----------------------------
//----------------------------

//----------------------------
// public functions
BOOL WINAPI DllMain(HINSTANCE hinstDLL,ULONG fdwReason,LPVOID lpvReserved) {

   hInstance = hinstDLL;
   static bool controlsInit = false;
   
   if(!controlsInit){
      controlsInit = true;
      
      InitCustomControls(hInstance);
      InitCommonControls();
   }
   return true;
}

//----------------------------

//------------------------------------------------------
// This is the interface to Jaguar:
//------------------------------------------------------

__declspec(dllexport) const TCHAR *LibDescription(){
   return "Joints interface";
}

//----------------------------

__declspec (dllexport) int LibNumberClasses(){
   return 1;
}

//----------------------------

__declspec (dllexport) ClassDesc *LibClassDesc(int i){
   switch(i) {
   case 0: return GetNewBonesDesc();
   default: return NULL;
   }
}

//----------------------------
// Return version so can detect obsolete DLLs
__declspec (dllexport) ULONG LibVersion(){ return VERSION_3DSMAX; }

//----------------------------

TCHAR *GetString(int id){

   static TCHAR buf[256];
   if(hInstance)
      return LoadString(hInstance, id, buf, sizeof(buf)) ? buf : NULL;
   return NULL;
}

//----------------------------
