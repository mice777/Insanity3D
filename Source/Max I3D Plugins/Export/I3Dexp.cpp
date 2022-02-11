#include "..\common\pch.h"

#include "..\common\common.h"
#include "..\common\mtldef.h"

#pragma comment(lib, "edmodel.lib")

using namespace std;

#define EXPORT_BIPED


#ifdef EXPORT_BIPED
#include "..\common\BIPEXP.H"
#endif

                              //max. number of map channels we do export
                              // this limitation is only because we don't want
                              // redundant channels we don't use
#define MAX_EXPORT_MAP_CHANNELS 2

#define W_TENS       1
#define W_CONT       2
#define W_BIAS       4
#define W_EASETO     8
#define W_EASEFROM   16

#define MAX_MESH_VERTICES 0x7fff

//#define REPORT_REDUNDANT_MATERIALS  //report if redundant materials were found

//----------------------------

inline bool NeedsKeys(dword nkeys){
   return (nkeys>0 || nkeys==NOT_KEYFRAMEABLE);
}

//----------------------------

enum E_OBJTYPE{
   OBTYPE_MESH = 0,
   OBTYPE_CAMERA = 1,
   OBTYPE_OMNILIGHT = 2,
   OBTYPE_SPOTLIGHT = 3,
   OBTYPE_DUMMY = 5,
   OBTYPE_CTARGET = 6,
   OBTYPE_LTARGET = 7,

   OBTYPE_FREEDIRECTLIGHT = 9,
   OBTYPE_FREESPOTLIGHT = 10,
   OBTYPE_TARGETDIRECTLIGHT = 11,

   OBTYPE_NURBS = 13,
   OBTYPE_JOINT = 20,
};

//----------------------------
//----------------------------

//- Material Export -------------------------------------------------------------

struct S_material_entry{
   Mtl *m;                    //MAX material class
   S_material mat_data;       //our converted material data
   int identical_index;       //index into identical material, or -1 if unique

   S_material_entry():
      identical_index(-1)
   {}

   inline bool operator <(const S_material_entry &me){
                              //note: inverse sorting, we want redundant mats with negative indices ent at back of v
      return (identical_index > me.identical_index);
   }
};

//----------------------------

inline bool IsMultiMaterial(Mtl *m){
   return (m->ClassID()==Class_ID(MULTI_CLASS_ID, 0));
}

//----------------------------

class C_material_list: public vector<S_material_entry>{

   static inline S_rgb C24(Color c){

      S_rgb a;
      a.r = (int)(255.0f*c.r);
      a.g = (int)(255.0f*c.g);
      a.b = (int)(255.0f*c.b);
      return a;
   }

//----------------------------

   static inline int Pcnt(float f){ return (int)(f*100.0f+.5f); }

//----------------------------

   static void ConvertMaxMtlToSMtl(S_material *s, Mtl *m){

      s->name = m->GetName();
      Interval v;
      m->Update(0, v);
      if(strchr(s->name, '@')){
         s->amb = C24(m->GetAmbient());
         s->diff = C24(m->GetDiffuse());
      }else{
         s->amb.White();
         s->diff.White();
      }
      {
         const char *mid = strchr(s->name, '/');
         if(mid)
            s->mirror_id = atoi(mid+1);
      }
      if(strchr(s->name, '^'))
         s->intended_duplicate = true;
      {
         const char *mid = strchr(s->name, '!');
         if(mid){
            s->transp_mode = S_material::TRANSP_COLORKEY;
            if(mid[1]=='!')
               s->transp_mode = S_material::TRANSP_COLORKEY_ALPHA;
         }
      }

      s->spec = C24(m->GetSpecular());
      //s->shininess = Pcnt(m->GetShininess());
      s->transparency = m->GetXParency();
      if(m->ClassID()==Class_ID(DMTL_CLASS_ID,0)){
         StdMat *std = (StdMat*)m;

         s->selfipct = Pcnt(std->GetSelfIllum(0));    
         if(std->GetTwoSided()) s->flags |= MF_TWOSIDE;
         if(std->GetWire()) s->flags |= MF_WIRE;
         if(!std->GetFalloffOut()) s->flags |= MF_XPFALLIN;
         if(std->GetFaceMap()) s->flags |= MF_FACEMAP;
         if(std->GetSoften()) s->flags |= MF_PHONGSOFT;
         if(std->GetWireUnits()) s->flags |= MF_WIREABS;

         switch(std->GetTransparencyType()){
         case TRANSP_FILTER:
         case TRANSP_SUBTRACTIVE: break;
         case TRANSP_ADDITIVE: s->flags |= MF_ADDITIVE;
         }

         for(int i=0; i<MAP_LAST; i++){
            int n = MAXMapIndex(i);
            if(!std->MapEnabled(n))
               continue;
            Texmap *tx = std->GetSubTexmap(n);
            if(!tx)
               continue;
                              //just do bitmap textures for now
            if(tx->ClassID()!=Class_ID(BMTEX_CLASS_ID, 0))
               continue;

            BitmapTex *bmt = (BitmapTex*)tx;
            S_map_data &md = s->maps[i];
            md.use = true;
            md.percentage = Pcnt(std->GetTexmapAmt(n, 0));

            TSTR filename;
            SplitPathFile(TSTR(bmt->GetMapName()), NULL, &filename);
            md.bitmap_name = filename;
            md.bitmap_name.ToLower();

            {
               StdUVGen *uv = bmt->GetUVGen();
               md.uv_offset.x = uv->GetUOffs(0); 
               md.uv_offset.y = -uv->GetVOffs(0); 
               md.uv_scale.x = uv->GetUScl(0); 
               md.uv_scale.y = uv->GetVScl(0); 
               //float ang = uv->GetAng(0);
               //par.ang_sin = -(float)sin(ang);
               //par.ang_cos = (float)cos(ang);
               //md.texture_blur  = uv->GetBlur(0);
               int tile = uv->GetTextureTiling();
               if(tile&(U_MIRROR|V_MIRROR))
                  md.flags |= MAP_UV_MIRROR;
               if(!(tile&(U_WRAP|V_WRAP)))
                  md.flags |= MAP_NOWRAP;
               TextureOutput *texout = bmt->GetTexout();
               if(texout->GetInvert()) 
                  md.flags |= MAP_INVERT;
               if(bmt->GetFilterType()==FILTER_SAT)
                  md.flags |= MAP_SAT;
            }

            {
                              //decipher more info from name
               C_str name = tx->GetName();
               for(dword i=0; i<name.Size(); i++){
                  char c = name[i];
                  switch(c){
                  case '+':
                     md.flags |= MAP_TRUE_COLOR;
                     break;

                  case '|':
                     {
                        ++i;
                        dword n = 0;
                        sscanf(&name[i], "%i%n", &md.anim_speed, &n);
                        i += n;
                        md.flags |= MAP_ANIMATED;
                     }                  
                     break;

                  case '$':
                     if(name[i+1]=='-'){
                        ++i;
                        md.flags |= MAP_NO_MIPMAP;
                     }
                     break;
                  case 'c':
                     if(name[i+1]=='-'){
                        ++i;
                        md.flags |= MAP_NO_COMPRESS;
                     }
                     break;
                  case '&':
                     {
                        ++i;
                        char pname[128];
                        pname[0] = 0;
                        int num = 0;
                        sscanf(&name[i], "%128s%n", pname, &num);
                        i += num;
                        if(pname[0]){
                           md.bitmap_name = pname;
                           md.flags |= MAP_PROCEDURAL;
                        }
                     }
                     break;
                  }
               }
            }
         }
      }
   }

//----------------------------

   void AddMtlInternal(Mtl *m){

      if(FindMaterial(m) != -1)
         return;

      S_material_entry me;
      me.m = m;
      ConvertMaxMtlToSMtl(&me.mat_data, m);
      push_back(me);
   }                       

//----------------------------
public:
   void AddMaterial(Mtl *m){

      if(!m)
         return;
      Interval v;
      m->Update(0, v);
      if(IsMultiMaterial(m)){
         for(int i=0; i<m->NumSubMtls(); i++){
            Mtl *sub = m->GetSubMtl(i);
            if(sub)
               AddMtlInternal(sub);
         }
      }else
         AddMtlInternal(m);
   }

//----------------------------
// Find index of specified MAX material.
// Return -1 if material couldn't be found.
   int FindMaterial(Mtl *m){

      for(int i=0; i<size(); i++){
         const S_material_entry &me = (*this)[i];
         if(me.m == m){
                              //check if it is reference to other material
            if(me.identical_index != -1){
               i = me.identical_index;
               assert((*this)[i].identical_index == -1);
            }
            return i;
         }
      }
      return -1;
   }

};


//----------------------------
//----------------------------

class S_scene_entry{
public:
	INode *node;
	Object *obj;
	E_OBJTYPE type;
	int id;                      //node identification number, used for saving parent-child relationship

   S_scene_entry(INode *n, Object *o, E_OBJTYPE t):
      node(n),
      obj(o),
      type(t),
      id(-1)
   {
   }

//----------------------------

   inline void SetID(int id1){ id = id1; }
};

//----------------------------
//----------------------------

class C_SceneEnumProc: public ITreeEnumProc{
   C_material_list materials;
   dword num_unique_materials;

                              //entries automatically sorted by number of parents!
   list<S_scene_entry*> entries;
   bool selection_only;

//----------------------------

   void Append(INode *node, Object *obj, E_OBJTYPE type){

      entries.push_back(new S_scene_entry(node, obj, type));
   }

//----------------------------

   virtual int callback(INode *node){

      assert(node);
      if(selection_only){
         if(!node->Selected())
            return TREE_CONTINUE;
      }

#ifdef EXPORT_BIPED
                              //ignore biped nodes
      Class_ID cid = node->GetTMController()->ClassID();
      if(cid==BIPSLAVE_CONTROL_CLASS_ID ||
         cid==BIPBODY_CONTROL_CLASS_ID ||
         cid==FOOTPRINT_CLASS_ID){
         return TREE_IGNORECHILDREN;
      }
#endif

//#ifdef _DEBUG
      {
         Object *obj_ref = node->GetObjectRef();
         Class_ID idr = obj_ref->ClassID();
         //dword sidr = obj_ref->SuperClassID();
         if(idr==Class_ID(XREFOBJ_CLASS_ID, 0)){
            /*
            IXRefObject *xref = (IXRefObject*)obj_ref;
            TSTR fn = xref->GetFileName();
            TSTR on = xref->GetObjName();
            TSTR cfn = xref->GetCurFileName();
            */
            Object *obj = node->EvalWorldState(time).obj;
                              //check if name contains two '~'
            const char *cp = strchr(node->GetName(), '~');
            if(cp && strchr(cp+1, '~')){
               Append(node, obj, OBTYPE_DUMMY);
               return TREE_IGNORECHILDREN;
            }
         }
      }
//#endif

      Object *obj = node->EvalWorldState(time).obj;
      Class_ID id = obj->ClassID();

                              //NURBS
      if(id==EDITABLE_SURF_CLASS_ID){
         Append(node, obj, OBTYPE_NURBS);
         materials.AddMaterial(node->GetMtl());
         ++num_unique_materials;
         return TREE_CONTINUE;
      }

      if(node->IsTarget()){
         /*
         INode* ln = node->GetLookatNode();
         if(ln){
            Object *lobj = ln->EvalWorldState(time).obj;
            switch(lobj->SuperClassID()){
               case LIGHT_CLASS_ID:  Append(node, obj, OBTYPE_LTARGET); break;
               case CAMERA_CLASS_ID: Append(node, obj, OBTYPE_CTARGET); break;
            }
         }
         */
         return TREE_CONTINUE;
      }

      SClass_ID scid = obj->SuperClassID();
      if(scid==JOINT_SUPERCLASSID && id==JOINT_CLASSID){
         Append(node, obj, OBTYPE_JOINT);
      }else{
         switch(scid){
         case HELPER_CLASS_ID:
            if(id==Class_ID(DUMMY_CLASS_ID, 0)){
               Append(node, obj, OBTYPE_DUMMY);
            }else
            if(id==Class_ID(BONE_CLASS_ID, 0)){
               Append(node, obj, OBTYPE_JOINT);
            }
            break;

         case LIGHT_CLASS_ID:
            {
               if(id==Class_ID(OMNI_LIGHT_CLASS_ID,0))
                  Append(node, obj, OBTYPE_OMNILIGHT);
               if(id==Class_ID(SPOT_LIGHT_CLASS_ID,0)) 
                  Append(node, obj, OBTYPE_SPOTLIGHT);
               if(id==Class_ID(DIR_LIGHT_CLASS_ID, 0)) {
   //             Append(node, obj, OBTYPE_FREEDIRECTLIGHT);
               }
               if(id==Class_ID(FSPOT_LIGHT_CLASS_ID,0)){
   //             Append(node, obj, OBTYPE_FREESPOTLIGHT);
               }
               if(id==Class_ID(TDIR_LIGHT_CLASS_ID,0)){
                  Append(node, obj, OBTYPE_TARGETDIRECTLIGHT);
               }
            }
            break;

         case CAMERA_CLASS_ID:
            Append(node, obj, OBTYPE_CAMERA);
            break;

         default:
            if(obj->CanConvertToType(triObjectClassID)){
               Append(node, obj, OBTYPE_MESH);
               materials.AddMaterial(node->GetMtl());
               ++num_unique_materials;
               return TREE_CONTINUE;
            }
         }
      }
      return TREE_CONTINUE;
   }

//----------------------------

public:
   TimeValue time;
   
   C_SceneEnumProc(IScene *scene, TimeValue t, bool sel):
      time(t),
      selection_only(sel)
   {
      materials.reserve(1000);
      scene->EnumTree(this);
   }

//----------------------------

   ~C_SceneEnumProc(){

      list<S_scene_entry*>::iterator it;
      for(it=entries.begin(); it!=entries.end(); it++)
         delete (*it);
   }

   inline int Count(){ return entries.size(); }
   inline const list<S_scene_entry*> &GetEntries() const{ return entries; }
   inline C_material_list &GetMaterials(){ return materials; }
   inline dword GetNumUniqueMaterials() const{ return num_unique_materials; }

//----------------------------

   S_scene_entry *Find(INode *node){

      list<S_scene_entry*>::iterator it;
      for(it=entries.begin(); it!=entries.end(); it++){
         if((*it)->node==node)
            return *it;
      }
      return NULL;
   }

//----------------------------

   void DetectIdenticalMaterials(Interface *max_i){

      num_unique_materials = materials.size();
      dword num_redundant = 0;

      for(int i=0; i<materials.size(); i++){
         S_material_entry &me = materials[i];
                              //unique have positive index
         me.identical_index = i;
         //if(me.mat_data.name=="brana") assert(0);
         for(int j=0; j<i; j++){
            S_material_entry &me1 = materials[j];
            //if(me.mat_data.name=="brana" && me1.mat_data.name=="brana") assert(0);
            if(me.mat_data.HaveIdenticalData(me1.mat_data)){
               if(!me.mat_data.intended_duplicate && !me1.mat_data.intended_duplicate){
                                 //redundant have negative index of the copy (minus one)
                  assert(me1.identical_index >= 0);
                  me.identical_index = -me1.identical_index - 1;
                  ++num_redundant;
                  break;
               }
            }
         }
      }
                                 //sort by indicies
      if(num_redundant){
         num_unique_materials -= num_redundant;
         sort(materials.begin(), materials.end());
      }

                                 //fix-up indices of redundant materials
      for(i=materials.size(); i--; ){
         S_material_entry &me = materials[i];
         if(me.identical_index >= 0){
            ++i;
            break;
         }
         int find_index = -me.identical_index - 1;
         for(int j=0; j<i; j++){
            if(materials[j].identical_index==find_index){
               me.identical_index = j;
               break;
            }
         }
         assert(j<i);
      }
                                 //reset indices of the rest
      if(i!=-1)
      while(i--){
         S_material_entry &me = materials[i];
         assert(me.identical_index >= 0);
         me.identical_index = -1;
      }
#ifdef REPORT_REDUNDANT_MATERIALS
      if(num_redundant){
                                 //warn user
         MessageBox(max_i->GetMAXHWnd(),
            C_fstr("Exporter found %i redundant (duplicated) materials from total %i materials. The duplicated materials will not be saved.", num_redundant, materials.size()),
            "Insanity export plugin", MB_OK);
      }
#endif
   }

//----------------------------
};

//----------------------------
//----------------------------

struct S_dump_context{
   class C_SceneEnumProc *scene_enum;
   C_chunk &ck;

   Interface *max_i;

   S_dump_context(C_chunk &ck_in):
      ck(ck_in)
   {}

//----------------------------

   bool WriteMaterials() const{

                                 //save materials
      const C_material_list &ml = scene_enum->GetMaterials();
      dword num_unique_materials = scene_enum->GetNumUniqueMaterials();

      ck(CT_NUM_MATERIALS, num_unique_materials);

      for(int ix=0; ix<num_unique_materials; ix++){
         const S_material *savemtl = &ml[ix].mat_data;

         ck <<= CT_MATERIAL;

         ck.WStringChunk(CT_MAT_NAME, savemtl->name);

         ck.WArrayChunk(CT_MAT_AMBIENT, &savemtl->amb, 3);
         ck.WArrayChunk(CT_MAT_DIFFUSE, &savemtl->diff, 3);
         ck.WArrayChunk(CT_MAT_SPECULAR, &savemtl->spec, 3);

         if(savemtl->transparency)
            ck.WByteChunk(CT_MAT_TRANSPARENCY, (int)(savemtl->transparency*100.0f+.5f));

         if(savemtl->selfipct)
            ck.WByteChunk(CT_MAT_SELF_ILPCT, savemtl->selfipct);

         if(savemtl->flags&MF_TWOSIDE){
            ck <<= CT_MAT_TWO_SIDE;
            --ck;
         }

                                 //save out any texture maps
         for(int k=0; k<MAP_LAST; k++){
            const S_map_data &md = savemtl->maps[k];
            if(md.use){
               static const ULONG map_chunks[MAP_LAST] = {
                  CT_MAP_DIFFUSE, CT_MAP_OPACITY, CT_MAP_BUMP,
                  CT_MAP_SPECULAR, CT_MAP_SHINNINESS, CT_MAP_SELF, CT_MAP_REFLECTION,
                  CT_MAP_DETAIL, CT_MAP_REFRACT, CT_MAP_DISPLACE, CT_MAP_AMBIENT,
                  CT_MAP_SPEC_LEVEL,
               };
               ck <<= map_chunks[k];
               {
                  if(md.percentage)
                     ck.WWordChunk(CT_PERCENTAGE, md.percentage);
                  ck.WStringChunk(CT_MAP_NAME, md.bitmap_name);

                  if(md.uv_offset.x!=0.0f || md.uv_offset.y!=0.0f){
                     ck <<= CT_MAP_UVOFFSET;
                     ck.Write(&md.uv_offset, sizeof(float)*2);
                     --ck;
                  }
                  if(md.uv_scale.x!=1.0f || md.uv_scale.y!=1.0f){
                     ck <<= CT_MAP_UVSCALE;
                     ck.Write(&md.uv_scale, sizeof(float)*2);
                     --ck;
                  }
                  if(md.flags&MAP_TRUE_COLOR){
                     ck <<= CT_MAP_TRUECOLOR;
                     --ck;
                  }
                  if(md.flags&MAP_ANIMATED){
                     ck(CT_MAP_ANIMATED, md.anim_speed);
                  }
                  if(md.flags&MAP_NO_MIPMAP){
                     ck <<= CT_MAP_NOMIPMAP;
                     --ck;
                  }
                  if(md.flags&MAP_NO_COMPRESS){
                     ck <<= CT_MAP_NOCOMPRESS;
                     --ck;
                  }
                  if(md.flags&MAP_PROCEDURAL){
                     ck <<= CT_MAP_PROCEDURAL;
                     --ck;
                  }
               }
               --ck;
            }
         }
         --ck;
      }
      return true;
   }

//----------------------------

   void WriteTCBTrackHeader(dword num_keys) const{

      S_I3D_track_header header;
      memset(&header, 0, sizeof(header));
      header.num_keys = num_keys;
      ck.Write(&header, sizeof(header));
   }

//----------------------------

   void WriteKeyBasicParams(const ITCBKey &key) const{

                              //write flags
      short wflags = 0;
      if(key.tens   != 0.0f) wflags |= W_TENS;
      if(key.cont   != 0.0f) wflags |= W_CONT;
      if(key.bias   != 0.0f) wflags |= W_BIAS;
      if(key.easeIn != 0.0f) wflags |= W_EASETO;
      if(key.easeOut!= 0.0f) wflags |= W_EASEFROM;       
      ck.Write(&wflags, sizeof(short));

                              //write TCB and ease
      if(key.tens   != 0.0f) ck.Write(&key.tens, sizeof(float));
      if(key.cont   != 0.0f) ck.Write(&key.cont, sizeof(float));
      if(key.bias   != 0.0f) ck.Write(&key.bias, sizeof(float));
      if(key.easeIn != 0.0f) ck.Write(&key.easeIn, sizeof(float));
      if(key.easeOut!= 0.0f) ck.Write(&key.easeOut, sizeof(float));
   }

//----------------------------

   bool WriteTCBPosKeys(INode *node, Control *cont) const{

                              //get the keyframe interface to the TCB Controller
      IKeyControl *ikeys = GetKeyControlInterface(cont);

      int num = cont->NumKeys();
      if(num == NOT_KEYFRAMEABLE || num == 0 || !ikeys)
         return false;

      WriteTCBTrackHeader(num);

      for(int i=0; i<num; i++){
         ITCBPoint3Key key;
         ikeys->GetKey(i, &key);

                              //write key time
         long keytime = key.time / GetTicksPerFrame();
         ck.Write(&keytime, sizeof(dword));

         WriteKeyBasicParams(key);
                              //write values
         S_vector v = (S_vector&)key.val;
         swap(v.y, v.z);
         ck.Write(&v, sizeof(S_vector));
      }
      return true;
   }

//----------------------------

   bool WriteTCBRotKeys(INode *node, Control *cont) const{

                              //get the keyframe interface to the TCB Controller
      IKeyControl *ikeys = GetKeyControlInterface(cont);

      int num = cont->NumKeys();
      if(num == NOT_KEYFRAMEABLE || num == 0 || !ikeys)
         return false;

      WriteTCBTrackHeader(num);

      for(int i=0; i<num; i++){
         ITCBRotKey key;
         ikeys->GetKey(i, &key);

                                 //write key time
         long keytime = key.time / GetTicksPerFrame();
         ck.Write(&keytime, sizeof(dword));

         WriteKeyBasicParams(key);
                                 //write values
         ck.Write(&key.val.angle, sizeof(float));
         ck.Write(&key.val.axis, sizeof(S_vector));
      }
      return true;
   }

//----------------------------

   bool WriteTCBSclKeys(INode *node, Control *cont) const{

                              //get the keyframe interface to the TCB Controller
      IKeyControl *ikeys = GetKeyControlInterface(cont);

      int num = cont->NumKeys();
      if(num == NOT_KEYFRAMEABLE || num == 0 || !ikeys)
         return false;

      WriteTCBTrackHeader(num);

      for(int i=0; i<num; i++){
         ITCBScaleKey key;
         ikeys->GetKey(i, &key);

                              //write key time
         long keytime = key.time / GetTicksPerFrame();
         ck.Write(&keytime, sizeof(dword));

         WriteKeyBasicParams(key);
                              //write values
         S_vector v = (S_vector&)key.val.s;
         swap(v.y, v.z);
                              //make all values positive
         v.Maximal(-v);
         ck.Write(v);
      }
      return true;
   }

//----------------------------

   static Matrix3 GetLocalNodeTM(INode *node, TimeValue t){

      if(!node)
         return Matrix3(true);
      Matrix3 tm = node->GetNodeTM(t);
      if(!node->GetParentNode()->IsRootNode()){
         Matrix3 ip = Inverse(node->GetParentNode()->GetNodeTM(t));
         tm = tm * ip;
      }
      return tm;
   }

//----------------------------

   void WriteSinglePosKey(INode *node) const{

      const Matrix3 &tm = GetLocalNodeTM(node, 0);
      /*
      AffineParts parts;
      decomp_affine(tm, &parts);

                              //write values
      Point3 pkey = parts.t;
      S_vector p(pkey.x, pkey.z, pkey.y);
      */
      S_vector p(tm[3][0], tm[3][2], tm[3][1]);
      ck.WVectorChunk(CT_NODE_POSITION, p);
   }

//----------------------------

   void WriteSingleRotKey(INode *node) const{

      AngAxis aa;
                              //get directly from controller, if possible
      Control *crot = node->GetTMController()->GetRotationController();
      if(crot && crot->ClassID()==Class_ID(TCBINTERP_ROTATION_CLASS_ID, 0)){
         //IKeyControl *ikeys = GetKeyControlInterface(crot);
         //ITCBRotKey key;
         //ikeys->GetKey(0, &key);
         Interval valid;
         Quat q;
         crot->GetValue(0, &q, valid);
         //aa = key.val;
         AngAxisFromQ(q, &aa.angle, aa.axis);
      }else{
         const Matrix3 tm = GetLocalNodeTM(node, 0);

         AffineParts parts;
         decomp_affine(tm, &parts);
         const Quat &q = parts.q;
         AngAxisFromQ(q, &aa.angle, aa.axis);
      }
      if(aa.angle!=0.0f){
         swap(aa.axis.y, aa.axis.z);
      
         aa.angle = -aa.angle;
         ck <<= CT_NODE_ROTATION;
         ck.Write(&aa.axis, sizeof(S_vector));
         ck.Write(&aa.angle, sizeof(float));
         --ck;
      }
   }

//---------------------------
// Get scale of node, in Insanity reference system.
   static S_vector GetNodeScale(INode *node){

      Point3 s;
                              //get directly from controller, if possible
      Control *cscl = node->GetTMController()->GetScaleController();
      if(cscl && cscl->ClassID()==Class_ID(TCBINTERP_SCALE_CLASS_ID, 0)){
         Interval valid;
         ScaleValue scl;
         cscl->GetValue(0, &scl, valid);
         s = scl.s;
      }else{
         const Matrix3 tm = GetLocalNodeTM(node, 0);

         AffineParts parts;
         decomp_affine(tm, &parts);
         s = ScaleValue(parts.k, parts.u).s;
         if(parts.f < 0.0f)
            s = - s;
      }
      return S_vector(s.x, s.z, s.y);
   }

//----------------------------

   void WriteSingleSclKey(INode *node) const{

      S_vector scl = GetNodeScale(node);
                              //make all values positive
      scl.Maximal(-scl);
      if(scl.x!=1.0f || scl.y!=1.0f || scl.z!=1.0f)
         ck.WVectorChunk(CT_NODE_SCALE, scl);
   }

//----------------------------
// Write the tracks.
   bool WriteController(INode* node) const{

      Interval valid;
      //TimeValue start = dc.max_i->GetAnimRange().Start();

      bool force_rot = false;

      Control *cpos = node->GetTMController()->GetPositionController();
      Control *crot = node->GetTMController()->GetRotationController();
      Control *cscl = node->GetTMController()->GetScaleController();

      Class_ID cid = node->GetTMController()->ClassID();

      if(cid == Class_ID(PRS_CONTROL_CLASS_ID, 0)){
         assert(cpos && crot && cscl);
                                 //this is a special case (path controller) modifies both pos and rot
         Class_ID cID = cpos->ClassID();
         if(cID == Class_ID(HYBRIDINTERP_COLOR_CLASS_ID, 0)) 
            force_rot = true;
      }

                                 //write POSITION Track keys
      if(!cpos){
                                 //this is a transform controller that doesn't support GetPositionController()
         WriteSinglePosKey(node);
      }else{
         if(NeedsKeys(cpos->NumKeys())){
            Class_ID cid = cpos->ClassID();
            bool ok = false;
            if(cid == Class_ID(TCBINTERP_POSITION_CLASS_ID, 0)){
               ck <<= CT_TRACK_POS;
               WriteTCBPosKeys(node, cpos);
               --ck;
               ok = true;
            }
#ifdef EXPORT_BIPED
            if(cid == Class_ID(POSITION_CONSTRAINT_CLASS_ID, 0)){
               IPosConstPosition *cc = (IPosConstPosition*)cpos;
               if(cc->GetNumTargets()){
                  INode *cc_node = cc->GetNode(0);
                  Control *cc_pos = cc_node->GetTMController()->GetPositionController();
                  if(cc_pos){
                     int num = cc_pos->NumKeys();
                     if(num>0){
                        ck <<= CT_TRACK_POS;
                        WriteTCBTrackHeader(num);

                        for(int i=0; i<num; i++){
                           dword key_time = cc_pos->GetKeyTime(i);
                                    //write key time
                           {
                              dword t = key_time / GetTicksPerFrame();
                              ck.Write(&t, sizeof(dword));
                           }

                           word flags = 0;
                           ck.Write(&flags, sizeof(word));

                           Interval valid;
                           Point3 pos;
                           cpos->GetValue(key_time, &pos, valid);

                           INode *prnt = node->GetParentNode();
                           if(!prnt->IsRootNode()){
                              Matrix3 ip = Inverse(prnt->GetNodeTM(key_time));
                              pos = pos * ip;
                           }
                           swap(pos.y, pos.z);
                           ck.Write(&pos, sizeof(Point3));
                        }
                        --ck;
                     }
                     ok = true;
                  }
               }
            }
#endif
            if(!ok){
               WriteSinglePosKey(node);
            }
         }else{
            WriteSinglePosKey(node);
         }
      }

                                 //write ROTATION Track keys
      if(!crot || force_rot){
                                 //this is a transform controller that doesn't support GetRotationController()
         WriteSingleRotKey(node);
      }else{
         if(NeedsKeys(crot->NumKeys())){
            Class_ID cid = crot->ClassID();
            bool ok = false;
            if(cid == Class_ID(TCBINTERP_ROTATION_CLASS_ID, 0)){
               ck <<= CT_TRACK_ROT;
               WriteTCBRotKeys(node, crot);
               --ck;
               ok = true;
            }
#ifdef EXPORT_BIPED
            if(cid == Class_ID(ORIENTATION_CONSTRAINT_CLASS_ID, 0)){
               IOrientConstRotation *cc = (IOrientConstRotation*)crot;
               if(cc->GetNumTargets()){
                  INode *cc_node = cc->GetNode(0);
                  Control *cc_rot = cc_node->GetTMController()->GetRotationController();
                  if(crot){
                     int num = cc_rot->NumKeys();
                     //IKeyControl *cc_ikeys = GetKeyControlInterface(cc_rot);
                     if(num>0){
                        ck <<= CT_TRACK_ROT;
                        WriteTCBTrackHeader(num);

                        Quat q_last1;
                        for(int i=0; i<num; i++){
                           dword key_time = cc_rot->GetKeyTime(i);
                                    //write key time
                           {
                              dword t = key_time / GetTicksPerFrame();
                              ck.Write(&t, sizeof(dword));
                           }

                           word flags = 0;
                           ck.Write(&flags, sizeof(word));

                           INode *prnt = node->GetParentNode();

                           Matrix3 m = node->GetNodeTM(key_time);
                           if(!prnt->IsRootNode()){
                              Matrix3 ip = Inverse(prnt->GetNodeTM(key_time));
                              Matrix3 m1 = m * ip;
                              m = m1;
                           }
                           Quat q1(m);

                           AngAxis aa;
                           if(!i)
                              aa = AngAxis(q1);
                           else{
                              aa = AngAxis(q1 / q_last1);
                           }
                                             //don't allow angles greater than PI
                           if(aa.angle > PI)
                              aa.angle -= PI*2.0f;
                           ck.Write(&aa.angle, sizeof(float));
                           ck.Write(&aa.axis, sizeof(S_vector));
                           q_last1 = q1;
                        }
                        --ck;
                     }
                     ok = true;
                  }
               }
            }
#endif
            if(!ok)
               WriteSingleRotKey(node);
         }else{
            WriteSingleRotKey(node);
         }
      }

                                 //write SCALE Track keys    
      if(!cscl){
         WriteSingleSclKey(node);
      }else{
         if(NeedsKeys(cscl->NumKeys())){
            Class_ID cid = cscl->ClassID();
            if(cid == Class_ID(TCBINTERP_SCALE_CLASS_ID, 0)){
               ck <<= CT_TRACK_SCL;
               WriteTCBSclKeys(node, cscl);
               --ck;
            }else{
               WriteSingleSclKey(node);
            }
         }else{
            WriteSingleSclKey(node);
         }
      }

                                 //write note track
      bool isnote = node->HasNoteTracks();
      if(isnote){
         ck <<= CT_TRACK_NOTE;

         dword ntracks = node->NumNoteTracks();
         ck.Write(&ntracks, sizeof(int));

         for(int i = 0; i<ntracks; i++){
            DefNoteTrack *track = (DefNoteTrack*)node->GetNoteTrack(i);            
            int numkeys = track->NumKeys();
            ck.Write(&numkeys, sizeof(int));

            for(int j = 0; j<numkeys;j++){
               TimeValue t = track->GetKeyTime(j) / GetTicksPerFrame();

               ck.Write(&t, sizeof(TimeValue));

               TSTR note = track->keys[j]->note;
               ck.Write(note.data(), strlen(note.data())+1);
            }
         }
         --ck;
      }

                                 //write visibility track
      Control *ctrl = node->GetVisController();

      if(ctrl){
         ck <<= CT_TRACK_VISIBILITY;

         SuspendAnimate();
         AnimateOn();
      
         S_I3D_track_header hdr;
         memset(&hdr, 0, sizeof(hdr));
         hdr.num_keys = ctrl->NumKeys();

         if(hdr.num_keys == NOT_KEYFRAMEABLE)
            hdr.num_keys = 0;
            
         ck.Write(&hdr, sizeof(hdr));

         TimeValue t;

         Interval valid;

         for(int i =0; i<hdr.num_keys;i++){
            t = ctrl->GetKeyTime(i);
            S_I3D_visibility_key key;
            ctrl->GetValue(t, &key.visibility, valid);
            key.time = t / GetTicksPerFrame();

            ck.Write(&key, sizeof(key));
         }
         ResumeAnimate();

         --ck;
      }
      return true;
   }

//----------------------------

   bool WriteProperties(INode *node){

      TSTR str;
      node->GetUserPropBuffer(str);
      if(str.Length() > 0)
         ck.WStringChunk(CT_PROPERTIES, str.data());
      return true;
   }

//----------------------------

   bool WriteFaceArray(Mesh *m, INode *node, bool flip_faces) const{

      ck <<= CT_FACE_ARRAY;

      word num_faces = m->getNumFaces();
      Mtl *mat = node->GetMtl();

      bool is_multi = (mat && IsMultiMaterial(mat));
      vector<int> mat_num_map;
      int num_sub_mats = 1; 
      if(is_multi){
                                 //make a table that maps sub-mtl number to the proper index
                                 // into dc.materials
         num_sub_mats = mat->NumSubMtls();
         mat_num_map.assign(num_sub_mats);
         for(int i=0; i<num_sub_mats; i++){
            int mat_i = -1;
            Mtl *sub  = mat->GetSubMtl(i);
            if(sub){
               mat_i = scene_enum->GetMaterials().FindMaterial(sub);
               assert(mat_i==-1 || mat_i < scene_enum->GetNumUniqueMaterials());
               /*
                              //check if material already present
               for(int j=i; j--; ){
                  if(mat_num_map[j]==mat_i){
                              //material already there? duplication, forget second instance
                     mat_i = -1;
                     break;
                  }
               }
               */
            }
            mat_num_map[i] = mat_i;
         }
      }

      vector<Face> face_list(m->faces, m->faces+num_faces);
      vector<dword> order(num_faces);

      for(int i=num_faces; i--; ){
         dword sort_val = i;
         if(is_multi){
            int face_mat_index = m->getFaceMtlIndex(i);
            face_mat_index %= num_sub_mats;
            //sort_val |= (face_mat_index<<16);
            sort_val |= (mat_num_map[face_mat_index]<<16);
         }
         order[i] = sort_val;
      }
      if(is_multi)
         sort(order.begin(), order.end());

      ck.Write(&num_faces, 2);
      bool anySmooth = false;
      for(i=0; i<num_faces; ++i){
         const Face &f = m->faces[order[i]&0xffff];
         if(f.smGroup)
            anySmooth = true;
         I3D_triface fc;
                              //copy face
         fc[0] = f.v[0];
         if(!flip_faces){
                              //we're flipping face for Insanity
            fc[1] = f.v[2];
            fc[2] = f.v[1];
         }else{
                              //correction of negative scale - let face unfilled (for Insanity it means flipped)
            fc[1] = f.v[1];
            fc[2] = f.v[2];
         }
         ck.Write(&fc, sizeof(I3D_triface));
      }
      --ck;

      ck <<= CT_EDGE_VISIBILITY;
      for(i=0; i<num_faces; ++i){
         const Face &f = m->faces[order[i]&0xffff];
         byte flags = f.flags & (EDGE_A|EDGE_B|EDGE_C);
         ck.Write(&flags, sizeof(byte));
      }
      --ck;

#pragma pack(push,1)
      struct S_face_group{
         word mat_id;
         word first_face;
         word num_faces;
      };
#pragma pack(pop)
      if(is_multi){
         word last_id = order[0]>>16;
         int count = 0;
         for(i=0; i<num_faces; i++){
            word mat_id = order[i]>>16;
            if(mat_id==last_id)
               ++count;
            else{
               ck <<= CT_MESH_FACE_GROUP;
               //S_face_group fg = { mat_num_map[last_id], i-count, count};
               S_face_group fg = { last_id, i-count, count};
               ck.Write(&fg, sizeof(fg));
               --ck;
               last_id = mat_id;
               count = 1;
            }
         }
         ck <<= CT_MESH_FACE_GROUP;
         //S_face_group fg = { mat_num_map[last_id], i-count, count};
         S_face_group fg = { last_id, i-count, count};
         ck.Write(&fg, sizeof(fg));
         --ck;
      }else{
         ck <<= CT_MESH_FACE_GROUP;
         int mat_i = -1;
         if(mat){
            mat_i = scene_enum->GetMaterials().FindMaterial(mat);
            assert(mat_i < scene_enum->GetNumUniqueMaterials());
         }
         S_face_group fg = { mat_i, 0, num_faces};
         ck.Write(&fg, sizeof(fg));
         --ck;
      }

                           //save smoothing groups if any
      if(anySmooth){
         ck <<= CT_SMOOTH_GROUP;
                           //now dump all object faces' smoothing group flags
         for(i=0; i<num_faces; ++i) {
            //Face f = m->faces[i];
            Face f = m->faces[order[i]&0xffff];
            ck.Write(&f.smGroup, 4);
         }
         --ck;
      }

                                 //export mapping channels
      for(i=1; i<m->getNumMaps(); i++){
         if(!m->mapSupport(i))
            continue;
         ck <<= CT_FACE_MAP_CHANNEL;

                                 //save map channel index
         ck.Write(&i, sizeof(int));

         const UVVert *uvverts = m->mapVerts(i);
         int num_uv_verts = m->getNumMapVerts(i);
         const TVFace *uvfaces = m->mapFaces(i);
         int num_faces = m->numFaces;

                                 //make temp buffers
         vector<S_vector2> tmp_uv_verts(num_uv_verts);
         vector<I3D_triface> tmp_uv_faces(num_faces);
         vector<bool> vertex_used(num_uv_verts, false);

         for(dword fi=num_faces; fi--; ){
            const TVFace &f = uvfaces[order[fi]&0xffff];
            I3D_triface &fc = tmp_uv_faces[fi];
            fc[0] = f.t[0];
            if(!flip_faces){
               fc[1] = f.t[2];
               fc[2] = f.t[1];
            }else{
               fc[1] = f.t[1];
               fc[2] = f.t[2];
            }
            vertex_used[fc[0]] = true;
            vertex_used[fc[1]] = true;
            vertex_used[fc[2]] = true;
         }

         vector<word> vertex_remap(num_uv_verts, 0xffff);
                                 //copy used vertices
         dword v_dst_i = 0;
         for(dword vi=num_uv_verts; vi--; ){
            if(vertex_used[vi]){
               S_vector2 &v = tmp_uv_verts[v_dst_i];
               v.x = uvverts[vi].x;
               v.y = uvverts[vi].y;
                                 //fix invalid (NaN) UV vertices
               if(v.x < -1e+16f)
                  v.x = -1e+16f;
               else
               if(v.x > 1e+16f)
                  v.x = 1e+16f;

               if(v.y < -1e+16f)
                  v.y = -1e+16f;
               else
               if(v.y > 1e+16f)
                  v.y = 1e+16f;

               vertex_remap[vi] = v_dst_i;
               ++v_dst_i;
            }else{
               --num_uv_verts;
            }
         }
                                 //remap uv faces
         for(fi=num_faces; fi--; ){
            I3D_triface &fc = tmp_uv_faces[fi];
            fc.Remap(vertex_remap.begin());
         }

                              //save map vertices
         ck.Write(&num_uv_verts, sizeof(word));
         ck.Write(tmp_uv_verts.begin(), sizeof(S_vector2)*num_uv_verts);
         /*
         for(int j=0; j<num_uv_verts; j++){
            ck.Write(&uvverts[j].x, sizeof(float));
            ck.Write(&uvverts[j].y, sizeof(float));
         }
         */
                              //save map faces
         ck.Write(&num_faces, sizeof(word));
         ck.Write(tmp_uv_faces.begin(), sizeof(I3D_triface)*num_faces);
         /*
         for(j=0; j<num_faces; j++){
            const TVFace &f = uvfaces[order[j]&0xffff];
            I3D_triface fc;
            fc[0] = f.t[0];
            fc[1] = f.t[2];
            fc[2] = f.t[1];
            ck.Write(&fc, sizeof(I3D_triface));
         }
         */
         --ck;
      }
      return true;
   }

//----------------------------
};

//----------------------------

HINSTANCE hInstance;
//----------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved){

   hInstance = hinstDLL;
   static int controlsInit = false;

   if(!controlsInit){
      controlsInit = true;
      
      InitCustomControls(hinstDLL);
      InitCommonControls();
   }
   return true;
}

//----------------------------
//----------------------------

class FindDepNodeEnum: public DependentEnumProc{
public:
   ReferenceTarget *targ;
   INode *depNode;

   FindDepNodeEnum(){
      targ = NULL;
      depNode = NULL;
   }

//----------------------------
// proc should return 1 when it wants enumeration to halt.
   virtual int proc(ReferenceMaker *rmaker){

      if(rmaker->SuperClassID()!=BASENODE_CLASS_ID)
         return 0;
      INode* node = (INode*)rmaker;
      if(node->GetTarget()==targ){
         depNode = node;
         return 1;
      }
      return 0;
   }
};

//----------------------------

static void RemoveUnusedMaterials(S_dump_context &dc){

   C_material_list &ml = dc.scene_enum->GetMaterials();
   vector<bool> mat_used(ml.size(), false);

   list<S_scene_entry*>::const_iterator it;
   const list<S_scene_entry*> &entries = dc.scene_enum->GetEntries();
   for(it=entries.begin(); it!=entries.end(); it++){
      S_scene_entry *se = (*it);

      switch(se->type){
      case OBTYPE_MESH:
         {
            INode *node = se->node;
            Object *obj = se->obj;
            TriObject *tri = (TriObject*)obj->ConvertToType(dc.max_i->GetTime(), triObjectClassID);
            Mesh &mesh = tri->mesh;
            dword num_faces = mesh.getNumFaces();
            if(num_faces){
               Mtl *mat = node->GetMtl();
               if(mat){
                  if(IsMultiMaterial(mat)){
                     set<dword> mat_indxs;
                     dword num_sub_mats = mat->NumSubMtls();
                     for(dword fi=num_faces; fi--; ){
                        dword mat_i = mesh.getFaceMtlIndex(fi);
                        mat_i %= num_sub_mats;
                        mat_indxs.insert(mat_i);
                     }
                     for(set<dword>::const_iterator it=mat_indxs.begin(); it!=mat_indxs.end(); it++){
                        Mtl *sub  = mat->GetSubMtl(*it);
                        assert(sub);
                        int indx = dc.scene_enum->GetMaterials().FindMaterial(sub);
                        assert(indx>=0 && indx<ml.size());
                        mat_used[indx] = true;
                     }
                  }else{
                     int indx = dc.scene_enum->GetMaterials().FindMaterial(mat);
                     assert(indx>=0 && indx<ml.size());
                     mat_used[indx] = true;
                  }
               }
            }
         }
         break;
      }
   }

   for(dword i=ml.size(); i--; ){
      if(!mat_used[i]){
         ml[i] = ml.back(); ml.pop_back();
      }
   }
}

//----------------------------

static bool KFWriteMain(S_dump_context &dc){

   {
      dc.ck <<= CT_KF_SEGMENT;
      Interval i = dc.max_i->GetAnimRange();
      long s = i.Start() / GetTicksPerFrame();
      long e = i.End() / GetTicksPerFrame();
      //long animLength = dc.max_i->GetAnimRange().End() / GetTicksPerFrame();
      dc.ck.Write(&s, sizeof(dword));
      dc.ck.Write(&e, sizeof(dword));
      //dc.ck->Write(&animLength, sizeof(dword));
      --dc.ck;
   }
   //dc.ck->WIntChunk(KFCURTIME, dc.max_i->GetTime());

   int node_id = 0;
   list<S_scene_entry*>::const_iterator it;
   const list<S_scene_entry*> &entries = dc.scene_enum->GetEntries();
   for(it=entries.begin(); it!=entries.end(); it++){
      S_scene_entry *se = (*it);

      switch(se->type){
      
      case OBTYPE_DUMMY:
      case OBTYPE_MESH:
      case OBTYPE_JOINT:
         {
            se->SetID(node_id++);
                              //begin chunk
            word ct;

            switch(se->type){
            case OBTYPE_DUMMY: ct = CT_NODE_DUMMY; break;
            case OBTYPE_MESH:
               ct = CT_NODE_MESH;
               break;
            case OBTYPE_JOINT: ct = CT_NODE_BONE; break;
            default: assert(0); ct = 0;
            }
            dc.ck <<= ct;

                              //write common data
            //dc.ck->WWordChunk(CT_NODE_ID, se->id);
            dc.ck.WStringChunk(CT_NODE_NAME, se->node->GetName());
            {
               INode *parentNode = se->node->GetParentNode();
               S_scene_entry *pse = dc.scene_enum->Find(parentNode);
               if(pse){
                  assert(pse->id!=-1);
                  dc.ck.WWordChunk(CT_NODE_PARENT, pse->id);
               }
            }
            if(se->node->IsBoneShowing()){
               dc.ck <<= CT_DISPLAY_LINK;
               --dc.ck;
            }

            if(!dc.WriteProperties(se->node))
               return false;

            /*
            {
                              //write pivot only if it's non-zero
               S_vector v = (S_vector&)GetPivotOffset(se->node);
               if(v.x!=0.0f || v.y!=0.0f || v.z!=0.0f){
                  swap(v.y, v.z);
                  dc.ck->WVectorChunk(CT_PIVOT_OFFSET, v);
               }
            }
            */
                              //write object-specific data
            switch(se->type){
            case OBTYPE_JOINT:
               {
                  if(se->obj->ClassID()==Class_ID(BONE_CLASS_ID, 0)){
                              //old joints (bones)
                  }else{
                     Animatable *psim = (Animatable*)se->obj;
                     IParamBlock2 *pb = psim->GetParamBlock(0);
                     float w, l;
                     pb->GetValue(jointobj_width, 0, w, FOREVER);
                     pb->GetValue(jointobj_length, 0, l, FOREVER);
                     dc.ck.WFloatChunk(CT_JOINT_WIDTH, w);
                     dc.ck.WFloatChunk(CT_JOINT_LENGTH, l);

                     Quat qo = se->node->GetObjOffsetRot();
                     S_quat q;
                     q.v.x = qo.x;
                     q.v.y = qo.z;
                     q.v.z = qo.y;
                     q.s = -qo.w;
                     dc.ck.WRotChunk(CT_JOINT_ROT_OFFSET, q);
                  }
               }
               break;

            case OBTYPE_DUMMY:
               {
                  dc.ck <<= CT_BOUNDING_BOX;
                  ViewExp *vpt = dc.max_i->GetViewport(NULL);
                  I3D_bbox bb;
                  se->obj->GetLocalBoundBox(dc.max_i->GetTime(), se->node, vpt, (Box3&)bb);
                  dc.ck.Write(&bb, sizeof(I3D_bbox));
                  --dc.ck;
               }
               break;

            case OBTYPE_MESH:
               {
                  INode *node = se->node;
                  Object *obj = se->obj;
                  TriObject *tri = (TriObject*)obj->ConvertToType(dc.max_i->GetTime(), triObjectClassID);
                  Mesh &mesh = tri->mesh;

                  if(mesh.getNumVerts() > MAX_MESH_VERTICES){
                     MessageBox(dc.max_i->GetMAXHWnd(),
                        C_xstr("Invalid mesh '%': too many vertices (%), max. allowed is %.") %se->node->GetName()
                        %mesh.getNumVerts() %MAX_MESH_VERTICES,
                        "Insanity export plugin", MB_OK);
                     break;
                  }

                  S_vector scl = dc.GetNodeScale(node);
                  bool flip_faces = (scl.x * scl.y * scl.z < 0.0f);

                  dc.ck <<= CT_MESH;

                  if(mesh.getNumVerts()){
                     const Matrix3 &otm = node->GetObjTMAfterWSM(0);
                     const Matrix3 &ntm = node->GetNodeTM(0);
                     Matrix3 delta_tm = otm * Inverse(ntm);

                     dc.ck <<= CT_POINT_ARRAY;

                     word count = mesh.getNumVerts();
                     dc.ck.Write(&count, 2);

                              //correct negative scales
                     S_vector scale_correct(1, 1, 1);
                     if(scl.x < 0.0f)
                        scale_correct.x = -1.0f;
                     if(scl.y < 0.0f)
                        scale_correct.y = -1.0f;
                     if(scl.z < 0.0f)
                        scale_correct.z = -1.0f;

                     for(int i=0; i<count; ++i){
                        Point3 v = mesh.verts[i];
                        v = v * delta_tm;
                        S_vector vv(v.x, v.z, v.y);
                        vv *= scale_correct;
                        dc.ck.Write(vv);
                     }
                     --dc.ck;
                  }
                  if(mesh.getNumFaces()){
                     if(!dc.WriteFaceArray(&mesh, node, flip_faces))
                        return false;
                  }
                  /*
                              //write vertex colors
                  if(mesh.getNumVerts() && mesh.mapSupport(0)){
                     dc.ck <<= CT_MESH_COLOR;

                     const UVVert *uvverts = mesh.mapVerts(0);
                     const TVFace *uvfaces = mesh.mapFaces(0);

                                          //re-map UV-style color mapping to per-vertex color
                     int num_verts = mesh.getNumVerts();
                     int num_faces = mesh.getNumFaces();
                     vector<S_vector> v_color(num_verts, S_vector(0, 0, 0));
                     vector<bool> v_used(num_verts, false);
                     for(int i=num_faces; i--; ){
                        const Face &f = mesh.faces[i];
                        const TVFace &cf = uvfaces[i];
                        for(int j=3; j--; ){
                           int vi = f.v[j];
                           int ci = cf.t[j];
                           const S_vector &color = *(S_vector*)&uvverts[ci];
                           if(!v_used[vi]){
                              v_color[vi] = color;
                              v_used[vi] = true;
                           }else{
                              v_color[vi] = (v_color[vi] + color) * .5f;
                           }
                        }
                     }
                                          //convert S_vector to RGB triplet
                     vector<S_rgb> rgb_colors(num_verts);
                     for(i=num_verts; i--; ){
                        const S_vector &src = v_color[i];
                        S_rgb &dst = rgb_colors[i];
                        dst.r = Max(0, Min((int)(src.x*255.0f), 255));
                        dst.g = Max(0, Min((int)(src.y*255.0f), 255));
                        dst.b = Max(0, Min((int)(src.z*255.0f), 255));
                     }
                     dc.ck.Write(rgb_colors.begin(), num_verts*sizeof(S_rgb));
                     --dc.ck;
                  }
                  */

                  if(tri!=obj)
                     tri->DeleteThis();

                  /*
                              //try to save additional data (skin, etc)
#if 0
                  {
                     Modifier *mod = FindSkinModifier(node);
                     if(mod){
                        ISkin *skin = (ISkin*)mod->GetInterface(I_SKIN);
                        ISkinContextData *sc = skin->GetContextInterface(node);

                        int num_bones = skin->GetNumBones();
                        vector<vector<word> > bone_verts(num_bones);
                        vector<vector<float> > bone_weights(num_bones);

                        int numv = sc->GetNumPoints();
                        for(int vi=0; vi<numv; vi++){
                           int numb = sc->GetNumAssignedBones(vi);
                           for(int bi=numb; bi--; ){
                              int bone_i = sc->GetAssignedBone(vi, bi);
                              bone_verts[bone_i].push_back(vi);
                              float w = sc->GetBoneWeight(vi, bi);
                              bone_weights[bone_i].push_back(w);
                           }
                        }
                        dc.ck <<= CT_SKIN;

                              //write data
                        for(int bi=0; bi<num_bones; bi++){
                           INode *bone = skin->GetBone(bi);
                           dc.ck <<= CT_SKIN_JOINT;

                           //TCHAR *bname = skin->GetBoneName(bi);
                           TCHAR *bname = bone->GetName();
                           dc.ck->WStringChunk(CT_NAME, bname);
                           {
                              S_vector m_init[4];
                              skin->GetBoneInitTM(bone, (Matrix3&)m_init, false);
                              SwapMatrixYZ(m_init);

                              dc.ck <<= CT_SKIN_JOINT_INIT_TM;
                              dc.ck->Write(m_init, sizeof(m_init));
                              dc.ck->Descend();
                           }
                           {
                              dword bone_prop = skin->GetBoneProperty(bi);
                              dc.ck->WIntChunk(CT_SKIN_JOINT_FLAGS, bone_prop);
                           }

                                                //write bone's vertices
                           {
                              dc.ck->WAscend(CT_SKIN_JOINT_VERTS);
                              word num = bone_verts[bi].size();
                              dc.ck->Write(&num, sizeof(word));
                              dc.ck->Write(bone_verts[bi].begin(), num*sizeof(word));
                              dc.ck->Write(bone_weights[bi].begin(), num*sizeof(float));

                              dc.ck->Descend();
                           }
                           dc.ck->Descend();
                        }
                        dc.ck->Descend();
                     }
                  }
#endif
                  */
                  --dc.ck;
               }
               break;
            }
                              //write controllers
            dc.WriteController(se->node);
            --dc.ck;
         }
         break;

         /*
      //case OBTYPE_MESH:
      case OBTYPE_NURBS:
         {
            se->SetID(nodeid++);
            dc.ck->WAscend(OBJECT_NODE_TAG);

            //Object *ob = se->obj;
            //dc.ck->WWordChunk(CT_NODE_ID, se->id);
            if(!WriteNodeHeader(se, dc))
               return false;
            {
                                 //write pivot only if it's non-zero
               Point3 pivot = GetPivotOffset(se->node);
               if(pivot.x!=0.0f || pivot.y!=0.0f || pivot.z!=0.0f){
                  dc.ck->WVectorChunk(PIVOT, *(S_vector*)&pivot);
               }
            }
            TSTR name = se->node->GetName();
            ObjectEntry *oe = dc.objects->Contains(ob);
            assert(oe);
            if(se->type!=OBTYPE_CTARGET&&se->type!=OBTYPE_LTARGET){
               TSTR mname(oe->entry->name);     // Master name
               if(se->type==OBTYPE_DUMMY || !(mname == name)){ 
                  CStr wname(name);
                  dc.ck->WStringChunk(INSTANCE_NAME, wname);
               }
            }
            WriteController(se->node, dc);

            if(!dc.WriteProperties(se->node))
               return false;

            dc.ck->Descend();
         }
         break;
         */
      }
   }
   return true;
}

//----------------------------

static int SaveMain(const TCHAR *filename, ExpInterface *ei, Interface *gi, bool selection_only){

   C_chunk ck;
   if(!ck.WOpen(filename))
      return 0;

   gi->SetIncludeXRefsInHierarchy(true);
                              //Make sure there are nodes we're interested in.
                              //Ask the scene to enumerate all its nodes
                              // so we can determine if there are any we can use
   C_SceneEnumProc scene_enum(ei->theScene, gi->GetTime(), selection_only);

   S_dump_context dc(ck);
   dc.scene_enum = &scene_enum;
   dc.max_i = gi;

   int error = 0;

   RemoveUnusedMaterials(dc);
   scene_enum.DetectIdenticalMaterials(gi);
   
   ck <<= CT_BASE;

   {
                              //write user comments
      PROPSPEC ps;
      ps.ulKind = PRSPEC_PROPID;//PRSPEC_LPWSTR;
      ps.propid = PIDSI_COMMENTS;
      int pi = gi->FindProperty(PROPSET_SUMMARYINFO, &ps);
      if(pi!=-1){
   		const PROPVARIANT *prop = gi->GetPropertyVariant(PROPSET_SUMMARYINFO, pi);
	   	if(prop && prop->vt == VT_LPSTR){
            int len = strlen(prop->pszVal);
            if(len){
               ck(CT_USER_COMMENTS, (const char*)prop->pszVal);
            }
         }
      }
   }
   dc.WriteMaterials();
   KFWriteMain(dc);
   --dc.ck;
   
   if(error){
      MessageBox(GetActiveWindow(), C_fstr("Failed to write to file: %s.", filename),
         "I3D exporter", MB_OK);
      remove(filename);
      return 0;
   }
   return 1;   
}

//----------------------------

class C_I3DExport: public SceneExport{

public:
   C_I3DExport(){
   }

   virtual ~C_I3DExport(){
   }

//----------------------------

   virtual int ExtCount(){ return 1; }

//----------------------------

   virtual const TCHAR *Ext(int n){
      
      switch(n){
      case 0: return "i3d";
      }
      return "";
   }

   virtual const TCHAR *LongDesc(){ return "Insanity 3D Resource File"; }
   virtual const TCHAR *ShortDesc(){ return "Insanity3D format"; }
   virtual const TCHAR *AuthorName(){ return "Lonely Cat Games"; }
   virtual const TCHAR *CopyrightMessage(){ return "Copyright (c) 2001 Lonely Cat Games"; }
   virtual const TCHAR *OtherMessage1(){ return ""; }
   virtual const TCHAR *OtherMessage2(){ return ""; }
   virtual unsigned int Version(){ return 200; }
   virtual void ShowAbout(HWND hWnd){ }

//----------------------------

   virtual int DoExport(const TCHAR *filename, ExpInterface *ei, Interface *gi, BOOL suppressPrompts, DWORD options){

      int status = SaveMain(filename, ei, gi, (options&SCENE_EXPORT_SELECTED));

      if(status == 0)
         return 1;      //Dialog cancelled
      if(status < 0)
         return 0;      //real error
      return status;
   }

//----------------------------

   virtual BOOL SupportsOptions(int ext, DWORD options){
      return (options==SCENE_EXPORT_SELECTED);
   }
};

//----------------------------

class C_I3DClassDesc: public ClassDesc{

public:
   virtual int IsPublic(){ return true; }
#if 0
   virtual void *Create(BOOL loading = false){ return new C_utility; }
   virtual SClass_ID SuperClassID(){ return UTILITY_CLASS_ID; }
   virtual int NumShortcutTables(){
      return 1;
   }
   virtual ShortcutTable *GetShortcutTable(int i){
      return C_utility::GetShortcuts();
   }
#else
   virtual void *Create(BOOL loading = false){ return new C_I3DExport; }
   virtual SClass_ID SuperClassID(){ return SCENE_EXPORT_CLASS_ID; }
#endif
   virtual Class_ID ClassID(){ return Class_ID(0x51b52649, 0x1662350c); }
   virtual const TCHAR *ClassName(){ return "Insanity3D Import/Export"; }

   virtual const TCHAR *Category(){ return "Scene Export"; }

};

static C_I3DClassDesc I3DDesc;

//----------------------------

__declspec(dllexport) const TCHAR *LibDescription(){ return "I3D file exporter"; }

//----------------------------

__declspec(dllexport) int LibNumberClasses(){ return 1; }

//----------------------------
// Return version so can detect obsolete DLLs
__declspec(dllexport) ULONG LibVersion(){ return VERSION_3DSMAX; }

//----------------------------

__declspec(dllexport) ClassDesc *LibClassDesc(int i){
   switch(i){
      case 0: return &I3DDesc; break;
      default: return 0; break;
   }
}

//----------------------------
//----------------------------
