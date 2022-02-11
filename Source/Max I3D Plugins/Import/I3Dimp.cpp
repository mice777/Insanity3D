#include "..\common\pch.h"
#include "..\common\common.h"
#include "resource.h"
#include "..\common\mtldef.h"

#pragma comment(lib, "edmodel.lib")

//----------------------------

#define BOOL_CONTROL_CLASS_ID Class_ID(0x984b8d27,0x938f3e43)

HINSTANCE hInstance;

//----------------------------

static S_vector ColorFrom24(S_rgb c){
   S_vector a;
   a.x = (float)c.r/255.0f;
   a.y = (float)c.g/255.0f;
   a.z = (float)c.b/255.0f;
   return a;
}

//----------------------------
//----------------------------

class C_import: public C_chunk{

   int node_load_id;

   bool ReadString(TSTR &tstr){

      int maxsize = 4096;
      char string;
      char buf[2];

      while(maxsize--){
         C_chunk::Read(&string, 1);
         sprintf(buf, "%c", string);
         tstr += TSTR(buf);
      
         if(string == 0)
            return true;
      }
      return false;
   }

//----------------------------

   static Texmap *MakeTextureMap(const S_map_data &md, const S_material &smtl){

      BitmapTex *bmt = NewDefaultBitmapTex();
      bmt->ActivateTexDisplay(true);
      if(!(md.flags&MAP_PROCEDURAL))
         bmt->SetMapName((char*)(const char*)md.bitmap_name);

      bmt->SetAlphaAsMono(md.flags&MAP_ALPHA_SOURCE);
      bmt->SetAlphaSource((md.flags&MAP_DONT_USE_ALPHA) ? ALPHA_NONE : ALPHA_FILE);
      bmt->SetFilterType((md.flags&MAP_SAT) ? FILTER_SAT : FILTER_PYR);

      StdUVGen *uv = bmt->GetUVGen();
      uv->SetUOffs(md.uv_offset.x, 0);
      uv->SetVOffs(-md.uv_offset.y, 0);
      uv->SetUScl(md.uv_scale.x, 0);
      uv->SetVScl(md.uv_scale.y, 0);
      //uv->SetAng(-((float)atan2(par.ang_sin, par.ang_cos)),0);
      //uv->SetBlur(md.texture_blur, 0);

      int tile = 0;
      if(md.flags&MAP_UV_MIRROR)
         tile |= U_MIRROR | V_MIRROR;
      else
      if(!(md.flags&MAP_NOWRAP))
         tile |= U_WRAP | V_WRAP;
      uv->SetTextureTiling(tile);
      bmt->GetTexout()->SetInvert(md.flags&MAP_INVERT);

                              //construct map's name from parameters
      C_str name;
      if(md.flags&MAP_TRUE_COLOR) name += "+ ";
      if(md.flags&MAP_ANIMATED) name += C_fstr("|%i ", md.anim_speed);
      if(md.flags&MAP_NO_MIPMAP) name += "$- ";
      if(md.flags&MAP_NO_COMPRESS) name += "c- ";
      if(md.flags&MAP_PROCEDURAL) name += C_fstr("&%s ", (const char*)md.bitmap_name);
      if(!name.Size())
         name = " ";
      bmt->SetName((const char*)name);
      return bmt;
   }

//----------------------------

   ImpInterface *imp_interface;
   Interface *_interface;
   typedef map<int, ImpNode*> t_node_list;
   t_node_list node_list;

   MtlList loaded_materials;
public:
   bool merge_tracks_mode;    //true if merging anim tracks only
   bool merge_mode;           //true if merging with current scene (no NewScene)
   dword num_imported;
   C_str status;

   C_import(ImpInterface *iptr, Interface *ip):
      node_load_id(-1),
      num_imported(0),
      merge_tracks_mode(false),
      merge_mode(false),
      imp_interface(iptr),
      _interface(ip)
   {
   }

//----------------------------

/*
   bool _CreateNurbs(int type){
      
      Matrix3 mat(1);
      _nurbs = CreateNURBSObject(NULL, &_nset, mat);
      if(!_nurbs){
         DebugPrint("Nurbs type %d create failure\n",type);
         return false;
      }
      return true;
   }
   */

//----------------------------

   void SetupEnvironment(){

      if(!merge_mode){
         imp_interface->SetAmbient(0, Color(.5f, .5f, .5f));
         imp_interface->SetBackGround(0, Color(.5f, .5f, .5f));
      }
      imp_interface->SetUseMap(false);
   }

//----------------------------

   ImpNode *FindNodeFromId(short id){

      t_node_list::const_iterator it = node_list.find(id);
      if(it==node_list.end())
         return NULL;
      return (*it).second;
   }

//----------------------------

   INode *FindNodeByName(const char *name){

      t_node_list::const_iterator it;
      for(it=node_list.begin(); it!=node_list.end(); it++){
         INode *node = (*it).second->GetINode();
         if(!strcmp(node->GetName(), name))
            return node;
      }
      return NULL;
   }

//----------------------------

   void SetPositionKeys(Control *cont, const vector<S_I3D_position_key> &keys){

      dword count = keys.size();

                              //special case where there is only one key at frame 0.
                              // just set the controller's value
      if(count==1 && keys.front().header.time==0){
         const S_I3D_position_key &key = keys.front();
         cont->SetValue(0, (void*)&key.data);
         return;
      }

                              //Make sure we can get the interface and have some keys
      IKeyControl *ikeys = (IKeyControl*)cont->GetInterface(I_KEYCONTROL);

      if(!count || !ikeys)
         return;
   
                              //allocate the keys in the table
      ikeys->SetNumKeys(count);

      for(int i=0; i<count; i++){

         const S_I3D_position_key &key = keys[i];

                              //set the common values
         ITCBPoint3Key k;
         k.time    = key.header.time * GetTicksPerFrame();
         k.tens    = key.add_data.tension;
         k.cont    = key.add_data.continuity;
         k.bias    = key.add_data.bias;
         k.easeIn  = key.add_data.easy_to;
         k.easeOut = key.add_data.easy_from;
      
         k.val[0] = key.data[0];
         k.val[1] = key.data[1];
         k.val[2] = key.data[2];
      
                              //set the key in the table
         ikeys->SetKey(i, &k);     
      }
      ikeys->SortKeys();
   }

//----------------------------

   void SetScaleKeys(Control *cont, const vector<S_I3D_scale_key> &keys){

      ITCBScaleKey k;

      int count = keys.size();

                              //special case where there is only one key at frame 0.
                              // just set the controller's value
      if(count==1 && keys.front().header.time==0){
         const S_I3D_scale_key &key = keys.front();
         ScaleValue s((Point3&)key.data);
         cont->SetValue(0, (void*)&s);
         return;
      }

                              //Make sure we can get the interface and have some keys
      IKeyControl *ikeys = GetKeyControlInterface(cont);
      if(!count || !ikeys)
         return;
   
                              //allocate the keys in the table
      ikeys->SetNumKeys(count);

      for(int i=0; i<count; i++){

         const S_I3D_scale_key &key = keys[i];
                              //set the common values
         k.time    = key.header.time * GetTicksPerFrame();
         k.tens    = key.add_data.tension;
         k.cont    = key.add_data.continuity;
         k.bias    = key.add_data.bias;
         k.easeIn  = key.add_data.easy_to;
         k.easeOut = key.add_data.easy_from;
      
         k.val[0] = key.data[0];
         k.val[1] = key.data[1];
         k.val[2] = key.data[2];
      
                              //set the key in the table
         ikeys->SetKey(i, &k);     
      }
      ikeys->SortKeys();
   }

//----------------------------

   void SetRotationKeys(Control *cont, const vector<S_I3D_rotation_key> &keys){

      ITCBRotKey k;

      int count = keys.size();

                              //special case where there is only one key at frame 0.
                              // just set the controller's value
      if(count==1 && keys.front().header.time==0){
         const S_I3D_rotation_key &key = keys.front();
         Point3 axis = (Point3&)key.data.axis;
         Quat q = QFromAngAxis(key.data.angle, axis);
         cont->SetValue(0, &q);
         return;
      }

                              //Make sure we can get the interface and have some keys
      IKeyControl *ikeys = GetKeyControlInterface(cont);
      if(!count || !ikeys)
         return;
   
                              //allocate the keys in the table
      ikeys->SetNumKeys(count);

      for(int i=0; i<count; i++){

         const S_I3D_rotation_key &key = keys[i];
                              //set the common values
         k.time    = key.header.time * GetTicksPerFrame();
         k.tens    = key.add_data.tension;
         k.cont    = key.add_data.continuity;
         k.bias    = key.add_data.bias;
         k.easeIn  = key.add_data.easy_to;
         k.easeOut = key.add_data.easy_from;
      
         k.val.angle  = key.data.angle; 
         k.val.axis.x = key.data.axis.x;
         k.val.axis.y = key.data.axis.y;
         k.val.axis.z = key.data.axis.z;
      
                              //set the key in the table
         ikeys->SetKey(i, &k);
      }
      ikeys->SortKeys();
   }

//----------------------------

   void SetColorKeys(Control *cont, const vector<S_I3D_color_key> &keys){

      ITCBPoint3Key k;

      int count = keys.size();

                              //special case where there is only one key at frame 0.
                              // just set the controller's value
      if(count==1 && keys.front().header.time==0){
         const S_I3D_color_key &key = keys.front();
         cont->SetValue(0, (void*)&key.data);
         return;
      }

                              //Make sure we can get the interface and have some keys
      IKeyControl *ikeys = GetKeyControlInterface(cont);
      if(!count || !ikeys)
         return;
   
                              //allocate the keys in the table
      ikeys->SetNumKeys(count);

      for(int i=0; i<count; i++){

         const S_I3D_color_key &key = keys[i];
                              //set the common values
         k.time    = key.header.time * GetTicksPerFrame();
         k.tens    = key.add_data.tension;
         k.cont    = key.add_data.continuity;
         k.bias    = key.add_data.bias;
         k.easeIn  = key.add_data.easy_to;
         k.easeOut = key.add_data.easy_from;
      
         k.val[0] = key.data[0];
         k.val[1] = key.data[1];
         k.val[2] = key.data[2];
      
                              //set the key in the table
         ikeys->SetKey(i, &k);     
      }
      ikeys->SortKeys();
   }

//----------------------------

   void MakeControlsTCB(Control *control){

      const dword inherit = INHERIT_ALL;
      control->SetInheritanceFlags(inherit, false);

      Control *c;
      c = control->GetPositionController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_POSITION_CLASS_ID,0)){
         Control *tcb = (Control*)_interface->CreateInstance(CTRL_POSITION_CLASS_ID, Class_ID(TCBINTERP_POSITION_CLASS_ID,0));
         if(!control->SetPositionController(tcb))
            tcb->DeleteThis();
      }

      c = control->GetRotationController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_ROTATION_CLASS_ID,0)){
         Control *tcb = (Control*)_interface->CreateInstance(CTRL_ROTATION_CLASS_ID, Class_ID(TCBINTERP_ROTATION_CLASS_ID,0));
         if(!control->SetRotationController(tcb))
            tcb->DeleteThis();
      }

      c = control->GetRollController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_FLOAT_CLASS_ID,0)){
         Control *tcb = (Control*)_interface->CreateInstance(CTRL_FLOAT_CLASS_ID, Class_ID(TCBINTERP_FLOAT_CLASS_ID,0));
         if(!control->SetRollController(tcb))
            tcb->DeleteThis();
      }

      c = control->GetScaleController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_SCALE_CLASS_ID,0)){
         Control *tcb = (Control*)_interface->CreateInstance(CTRL_SCALE_CLASS_ID, Class_ID(TCBINTERP_SCALE_CLASS_ID,0));
         if(!control->SetScaleController(tcb))
            tcb->DeleteThis();
      }
   }

//----------------------------

   void AssignMaterials(INode *inode, Mesh *mesh){

      typedef map<int, int> t_map;
      t_map mat_id_to_loc_id;
                              //count materials and make conversion map
      for(int i =0; i<mesh->numFaces; i++){
         int mat_id = mesh->faces[i].getMatID();
         if(mat_id_to_loc_id.find(mat_id) == mat_id_to_loc_id.end())
            mat_id_to_loc_id[mat_id] = mat_id_to_loc_id.size();
      }

      int num_face_mats = mat_id_to_loc_id.size();
                              //check if multi-mat is required
      if(num_face_mats > 1){
                              //need a multi-mat
                              // scrunch the numbers down to be local to this multi-mtl
         for(i=0; i<mesh->numFaces; i++){
            Face &f = mesh->faces[i];
            int id = f.getMatID();
            int loc_id = mat_id_to_loc_id[id];
            f.setMatID(loc_id);
         }
                              //create a new multi with numMtls, and set them
         MultiMtl *mat = NewDefaultMultiMtl();
         mat->SetNumSubMtls(num_face_mats);
         int i = 0;
         for(t_map::const_iterator it=mat_id_to_loc_id.begin(); it!=mat_id_to_loc_id.end(); it++, i++){
            mat->SetSubMtl(i, loaded_materials[(*it).first]);
         }
         inode->SetMtl(mat);
         loaded_materials.AddMtl(mat);         
      }else{
         if(mesh->getNumFaces()){
            word mat_id = mesh->faces[0].getMatID();
            if(mat_id!=0xffff){
               assert(mat_id < loaded_materials.Count());
               for(i=0; i<mesh->numFaces; i++) 
                  mesh->faces[i].setMatID(0);
               inode->SetMtl(loaded_materials[mat_id]);
            }
         }
      }
   }

//----------------------------
// Convert mesh mtl to max standard matl.
   void AddMaterial(const S_material &smtl){

      StdMat *m = NewDefaultStdMat();
      m->SetName((const char*)smtl.name);
      m->SetShading(SHADE_PHONG);
      m->SetAmbient((Color&)ColorFrom24(smtl.amb), 0);
      m->SetDiffuse((Color&)ColorFrom24(smtl.diff), 0);
      m->SetFilter((Color&)ColorFrom24(smtl.diff), 0);
      m->SetSpecular((Color&)ColorFrom24(smtl.spec), 0);
      //m->SetShininess((float)smtl.shininess * .01f, 0);
      m->SetOpacity(1.0f - smtl.transparency, 0);
      m->SetFalloffOut(smtl.flags&MF_XPFALLIN ? 0 : 1);  
      m->SetSelfIllum((float)smtl.selfipct * .01f, 0);
      m->SetFaceMap(smtl.flags&MF_FACEMAP);
      m->SetSoften(smtl.flags&MF_PHONGSOFT);
      m->SetWire(smtl.flags&MF_WIRE);
      m->SetTwoSided(smtl.flags&MF_TWOSIDE);
      m->SetTransparencyType((smtl.flags&MF_ADDITIVE) ? TRANSP_ADDITIVE : TRANSP_FILTER);
      m->SetWireUnits(smtl.flags&MF_WIREABS);


      for(int i=0; i<MAP_LAST; i++){
         if(!smtl.maps[i].use)
            continue;

         const S_map_data &md = smtl.maps[i];
         int n = MAXMapIndex(i);
      
         {
            float amt = (float)md.percentage * .01f;
            if(n==ID_BU) amt *= 10.0f;
            m->SetTexmapAmt(n, amt, 0);
         }
                              //create map
         Texmap *txm = MakeTextureMap(md, smtl);
         m->SetSubTexmap(n, txm);

                              //make diffuse bitmap visible
         if(i==MAP_DIFFUSE){
            m->EnableMap(n, true);
            if(!i){
               m->SetActiveTexmap(txm);
               _interface->ActivateTexture(txm, m);
            }
         }
      }
      loaded_materials.AddMtl(m);
   }

//----------------------------

   bool GetChunk(CK_TYPE ck_t, void *data);

//----------------------------
// Read map info and descend chunk.
   bool GetMap(S_material &loadmtl, int n){

      S_map_data &md = loadmtl.maps[n];
      md.use = true;

      while(C_chunk::Size()){
         switch(C_chunk::RAscend()){
         case CT_PERCENTAGE:
            {
               md.percentage = C_chunk::RWordChunk();
               if(n==MAP_BUMP)
                  md.percentage = (float)md.percentage * .1f;
            }
            break;
            /*
         case FLOAT_PERCENTAGE:
            assert(0);
            C_chunk::Descend();
            break;
            */
         case CT_MAP_NAME:
            md.bitmap_name = C_chunk::RStringChunk();
            break;

         case CT_MAP_UVOFFSET:
            Read(&md.uv_offset, sizeof(float)*2);
            C_chunk::Descend();
            break;

         case CT_MAP_UVSCALE:
            Read(&md.uv_scale, sizeof(float)*2);
            C_chunk::Descend();
            break;

         case CT_MAP_TRUECOLOR:
            md.flags |= MAP_TRUE_COLOR;
            Descend();
            break;

         case CT_MAP_ANIMATED:
            md.flags |= MAP_ANIMATED;
            md.anim_speed = RIntChunk();
            break;

         case CT_MAP_NOMIPMAP:
            md.flags |= MAP_NO_MIPMAP;
            Descend();
            break;

         case CT_MAP_NOCOMPRESS:
            md.flags |= MAP_NO_COMPRESS;
            Descend();
            break;

         case CT_MAP_PROCEDURAL:
            md.flags |= MAP_PROCEDURAL;
            Descend();
            break;

         default:
            C_chunk::Descend();
         }
      }
      C_chunk::Descend();
      return true;
   }

//----------------------------
// Get material chunk and Descend.
   bool GetMaterialChunk(S_material &loadmtl){

      while(C_chunk::Size()){
         CK_TYPE ck_t = C_chunk::RAscend();
         switch(ck_t){
         case CT_MAT_NAME:
            loadmtl.name = C_chunk::ReadString();
            C_chunk::Descend();
            {
                              //remove any invalid marks
               for(dword i=0; i<loadmtl.name.Size(); i++){
                  switch(loadmtl.name[i]){
                  case '%':
                     if(loadmtl.name[i+1]=='%'){
                        ++i;
                        break;
                     }
                              //flow...
                  case '$':
                  case '|':
                     loadmtl.name[i] = '_';
                     break;
                  }
               }
            }
            break;     

         case CT_MAT_AMBIENT:
         case CT_MAT_DIFFUSE:
         case CT_MAT_SPECULAR:
            {
               void *data;
               switch(ck_t){
               case CT_MAT_AMBIENT: data = &loadmtl.amb; break;
               case CT_MAT_DIFFUSE: data = &loadmtl.diff; break;
               case CT_MAT_SPECULAR: data = &loadmtl.spec; break;
               default: assert(0); data = NULL;
               }
               C_chunk::Read(data, 3);
               C_chunk::Descend();
            }
            break;
      
         case CT_MAP_DIFFUSE: if(!GetMap(loadmtl, MAP_DIFFUSE)) return false; break;
         case CT_MAP_OPACITY: if(!GetMap(loadmtl, MAP_OPACITY)) return false; break;
         case CT_MAP_BUMP: if(!GetMap(loadmtl, MAP_BUMP)) return false; break;
         case CT_MAP_SPECULAR: if(!GetMap(loadmtl, MAP_SPECULAR)) return false; break;
         case CT_MAP_SHINNINESS: if(!GetMap(loadmtl, MAP_SHININESS)) return false; break;
         case CT_MAP_SELF: if(!GetMap(loadmtl, MAP_SELF_ILLUM)) return false; break;
         case CT_MAP_REFLECTION: if(!GetMap(loadmtl, MAP_REFLECTION)) return false; break;
         case CT_MAP_DETAIL: if(!GetMap(loadmtl, MAP_FILTER)) return false; break;
         case CT_MAP_REFRACT: if(!GetMap(loadmtl, MAP_REFRACTION)) return false; break;
         case CT_MAP_DISPLACE: if(!GetMap(loadmtl, MAP_DISPLACEMENT)) return false; break;
         case CT_MAP_AMBIENT: if(!GetMap(loadmtl, MAP_AMBIENT)) return false; break;

         case CT_MAT_TWO_SIDE:
            loadmtl.flags |= MF_TWOSIDE;
            C_chunk::Descend();
            break;

         case CT_MAT_TRANSPARENCY:
            loadmtl.transparency = (float)C_chunk::RByteChunk() * .01f;
            break;

         case CT_MAT_SELF_ILPCT:
            loadmtl.selfipct = C_chunk::RByteChunk();
            break;

         case CT_MAT_SELF_ILLUM:
            loadmtl.flags |= MF_SELF;   
            loadmtl.selfipct = 100;            
            C_chunk::Descend();
            break;

         default:
            C_chunk::Descend();
         }
      }
      C_chunk::Descend();
      return true;
   }
};

//----------------------------
//----------------------------

bool C_import::GetChunk(CK_TYPE ck_t, void *data){

   switch(ck_t){

   case CT_USER_COMMENTS:
      {
         C_str str = C_chunk::ReadString();
         PROPSPEC ps;
         ps.ulKind = PRSPEC_PROPID;//PRSPEC_LPWSTR;
         ps.propid = PIDSI_COMMENTS;

         PROPVARIANT prop;
         prop.vt = VT_LPSTR;
         prop.pszVal = (char*)(const char*)str;
         _interface->AddProperty(PROPSET_SUMMARYINFO, &ps, &prop);
      }
      break;

   case CT_KF_SEGMENT:
      if(!merge_mode){
         long segStart, segEnd;
         C_chunk::Read(&segStart, sizeof(dword));
         C_chunk::Read(&segEnd, sizeof(dword));

         Interval cur = imp_interface->GetAnimRange();
         Interval nrange = Interval(0, segEnd * GetTicksPerFrame());
         nrange.SetEnd(Max(nrange.End(), 1));
         if(!(cur==nrange))
            imp_interface->SetAnimRange(nrange);
      }
      break;

   case CT_NODE_MESH:
   case CT_NODE_DUMMY:
   case CT_NODE_BONE:
      {
         ImpNode *imp_node = NULL;
         INode *inode = NULL;
         Object *obj = NULL;
         Mesh *mesh = NULL;

         if(!merge_tracks_mode){
            ++node_load_id;
            imp_node = imp_interface->CreateNode();
            inode = imp_node->GetINode();
            inode->AllEdges(true);
            obj = NULL;

            switch(ck_t){
            case CT_NODE_MESH:
               {
                  TriObject *tobj = CreateNewTriObject();
                  mesh = &tobj->GetMesh();
                  //inode->SetCastShadows(true);
                  //inode->SetRcvShadows(true);
                  obj = tobj;
                  //mesh = &object->GetMesh();
               }
               break;
            case CT_NODE_DUMMY:
               {
                  DummyObject *dum = (DummyObject*)imp_interface->Create(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0));
                  obj = dum;
                  dum->SetBox(Box3(-Point3(0.5f,0.5f,0.5f), Point3(0.5f,0.5f,0.5f)));
               }
               break;
            case CT_NODE_BONE:
               {
                  //obj = (Object*)imp_interface->Create(HELPER_CLASS_ID, Class_ID(BONE_CLASS_ID, 0));
                  obj = (Object*)imp_interface->Create(JOINT_SUPERCLASSID, JOINT_CLASSID);
                  //inode->ShowBone(true);
                  const Point3 &jointColor = GetUIColor(COLOR_BONES);
                  inode->SetWireColor(RGB(int(jointColor.x*255.0f), int(jointColor.y*255.0f), int(jointColor.z*255.0f)));
               }
               break;
            default: assert(0);
            }
            imp_node->Reference(obj);
            imp_interface->AddNodeToScene(imp_node);
            node_list[node_load_id] = imp_node;

            imp_node->SetTransform(0, Matrix3(true));
         }
         bool parent_set = false;

         bool do_loop = true;
         while(C_chunk::Size() && do_loop){
            switch(ck_t = C_chunk::RAscend()){
            case CT_NODE_NAME:
               {
                  C_str name = C_chunk::RStringChunk();
                  if(merge_tracks_mode){
                     inode = _interface->GetINodeByName(name);
                     if(!inode){
                        status += C_fstr("Node '%s':  NOT FOUND\n", (const char*)name);
                        do_loop = false;
                        break;
                     }
                     ++num_imported;
                     //status += C_fstr("Node '%s':  TRACKS IMPORTED\n", (const char*)name);

                              //remove all node's tracks
                     while(inode->NumNoteTracks()){
                        inode->DeleteNoteTrack(inode->GetNoteTrack(0));
                     }
                     inode->SetVisController(NULL);
                     Control *control = inode->GetTMController();
                     MakeControlsTCB(control);
                     ((IKeyControl*)control->GetPositionController()->GetInterface(I_KEYCONTROL))->SetNumKeys(0);
                     ((IKeyControl*)control->GetRotationController()->GetInterface(I_KEYCONTROL))->SetNumKeys(0);
                     ((IKeyControl*)control->GetScaleController()->GetInterface(I_KEYCONTROL))->SetNumKeys(0);
                  }else{
                     imp_node->SetName(name);
                  }
               }
               break;

            case CT_NODE_PARENT:
               if(merge_tracks_mode){
                  Descend();
                  break;
               }
               {
                  word prnt_id = C_chunk::RWordChunk();
                  ImpNode *pnode = FindNodeFromId(prnt_id);
                  assert(pnode);
                  if(pnode){
                     pnode->GetINode()->AttachChild(inode);
                     parent_set = true;
                     /*
                              //if we're attaching bone to bone, setup parent's length
                     if(obj->ClassID()==JOINT_CLASSID){
                        Object *pobj = pnode->GetINode()->EvalWorldState(0).obj;
                        if(pobj->ClassID()==JOINT_CLASSID){
                           //const Matrix3 &tm1 = inode->GetNodeTM(0);

                           Animatable *psim = (Animatable*)pobj;
                           IParamBlock2 *pb = psim->GetParamBlock(0);
                           pb->SetValue(jointobj_length, 0, 2.0f);
                        }
                     }
                     */
                  }
               }
               break;

            case CT_DISPLAY_LINK:
               {
                  if(!merge_tracks_mode)
                     inode->ShowBone(1);
                  C_chunk::Descend();
               }
               break;

            case CT_BOUNDING_BOX:
               {
                  I3D_bbox bb;
                  C_chunk::Read(&bb, sizeof(I3D_bbox));
                  if(!merge_tracks_mode)
                     ((DummyObject*)obj)->SetBox((Box3&)bb);
                  C_chunk::Descend();
               }
               break;

            case CT_JOINT_WIDTH:
               {
                  float w = C_chunk::RFloatChunk();
                  if(!merge_tracks_mode){
                     assert(obj->ClassID()==JOINT_CLASSID);
                     IParamBlock2 *pb = ((Animatable*)obj)->GetParamBlock(0);
                     pb->SetValue(jointobj_width, 0, w);
                  }
               }
               break;

            case CT_JOINT_LENGTH:
               {
                  float l = C_chunk::RFloatChunk();
                  if(!merge_tracks_mode){
                     assert(obj->ClassID()==JOINT_CLASSID);
                     IParamBlock2 *pb = ((Animatable*)obj)->GetParamBlock(0);
                     pb->SetValue(jointobj_length, 0, l);
                  }
               }
               break;

            case CT_JOINT_ROT_OFFSET:
               {
                  S_quat q = C_chunk::RQuaternionChunk();
                  if(!merge_tracks_mode){
                     assert(obj->ClassID()==JOINT_CLASSID);
                     Quat qo;
                     qo.x = q.v.x;
                     qo.y = q.v.z;
                     qo.z = q.v.y;
                     qo.w = -q.s;
                     inode->SetObjOffsetRot(qo);
                  }
               }
               break;

            case CT_PROPERTIES:
               {
                  C_str str = C_chunk::RStringChunk();
                  if(!merge_tracks_mode)
                     inode->SetUserPropBuffer(CStr((const char*)str));
               }
               break;

            case CT_MESH:
               if(!merge_tracks_mode){
                  bool has_map_channel = false;

                  int num_faces = 0;
                  while(C_chunk::Size()){
                     switch(ck_t = C_chunk::RAscend()){
                     case CT_POINT_ARRAY:
                        {
                           word count;
                           C_chunk::Read(&count, sizeof(word));

                           if(!mesh->setNumVerts(count))
                              return false;

                           S_vector v;
                           for(int i=0; i<count; ++i){
                              C_chunk::Read(&v, sizeof(S_vector));
                              mesh->setVert(i, v.x, v.z, v.y);
                           }
                           C_chunk::Descend();
                        }
                        break;

                     case CT_FACE_ARRAY:
                        {
                           word count;
                           C_chunk::Read(&count, sizeof(word));
                           num_faces = count;

                           if(!mesh->setNumFaces(count))
                              return false;
                           if(!mesh->setNumTVFaces(count))
                              return false;

                           I3D_triface *faces = new I3D_triface[count];
                           C_chunk::Read(faces, count*sizeof(I3D_triface));

                           for(int i=0; i<count; ++i){
                              const I3D_triface &fc = faces[i];
                              Face &f = mesh->faces[i];

                              f.setVerts(fc[0], fc[2], fc[1]);
                           }
                           delete[] faces;
                           C_chunk::Descend();
                        }
                        break;

                     case CT_EDGE_VISIBILITY:
                        {
                           byte *flags = new byte[num_faces];
                           C_chunk::Read(flags, num_faces*sizeof(byte));
                           for(int i=num_faces; i--; ){
                              Face &f = mesh->faces[i];
                              f.flags &= ~(EDGE_A|EDGE_B|EDGE_C);
                              f.flags |= flags[i] & (EDGE_A|EDGE_B|EDGE_C);
                           }
                           delete[] flags;
                           C_chunk::Descend();
                        }
                        break;

                     case CT_MESH_FACE_GROUP:
                        {
                           word mat_id;
                           word first_face;
                           word num_faces;
                           C_chunk::Read(&mat_id, sizeof(word));
                           C_chunk::Read(&first_face, sizeof(word));
                           C_chunk::Read(&num_faces, sizeof(word));
                           for(int i=0; i<num_faces; i++)
                              mesh->faces[first_face+i].setMatID(mat_id);
                           C_chunk::Descend();
                        }
                        break;

                     case CT_SMOOTH_GROUP:
                        {
                           dword *sgl = new dword[num_faces];
                           C_chunk::Read(sgl, sizeof(dword)*num_faces);
                           for(int i=num_faces; i--; ){
                              Face &f = mesh->faces[i];
                              f.setSmGroup(sgl[i]);
                           }
                           delete[] sgl;

                           C_chunk::Descend();
                        }
                        break;

                     case CT_FACE_MAP_CHANNEL:
                        {
                           int channel_i;
                                       //read map channel index
                           C_chunk::Read(&channel_i, sizeof(int));
                           dword num_uv_verts = 0;
                                       //read map vertices
                           C_chunk::Read(&num_uv_verts, sizeof(word));
                           UVVert *uvverts = new UVVert[num_uv_verts];
                           for(int i=0; i<num_uv_verts; i++){
                              C_chunk::Read(&uvverts[i].x, sizeof(float));
                              C_chunk::Read(&uvverts[i].y, sizeof(float));
                              uvverts[i].z = 0.0f;
                           }
                           dword num_faces = 0;
                                       //read map faces
                           C_chunk::Read(&num_faces, sizeof(word));
                           TVFace *uvfaces = new TVFace[num_faces];

                           for(i=0; i<num_faces; i++){
                              I3D_triface fc;
                              C_chunk::Read(&fc, sizeof(I3D_triface));
                              uvfaces[i].t[0] = fc[0];
                              uvfaces[i].t[1] = fc[2];
                              uvfaces[i].t[2] = fc[1];
                           }
                           if(channel_i>=2 && mesh->getNumMaps() < (channel_i+1))
                              mesh->setNumMaps(channel_i+1, true);
                           mesh->setMapSupport(channel_i, true);
                           if(num_uv_verts && num_faces){
                              mesh->setNumMapVerts(channel_i, num_uv_verts, false);
                              for(i=0; i<num_uv_verts; i++)
                                 mesh->setMapVert(channel_i, i, uvverts[i]);
                              mesh->setNumMapFaces(channel_i, num_faces, false);
                              for(i=0; i<num_faces; i++)
                                 mesh->mapFaces(channel_i)[i] = uvfaces[i];
                           }else{
                              mesh->MakeMapPlanar(channel_i);
                           }

                           delete[] uvverts;
                           delete[] uvfaces;

                           C_chunk::Descend();

                           has_map_channel = true;
                        }
                        break;

                     case CT_MESH_COLOR:
                        C_chunk::Descend();
                        break;

                     default:
                        assert(0);
                        C_chunk::Descend();
                     }
                  }
                  if(!has_map_channel){
                              //always need at least one mapping channel
                              // create one now, if it's not in the file
                     mesh->setNumMaps(1, false);
                     mesh->MakeMapPlanar(1);
                  }
                  AssignMaterials(inode, mesh);
               }
               C_chunk::Descend();
               break;

            case CT_NODE_POSITION:
               {
                  S_vector v = C_chunk::RVectorChunk();
                  swap(v.y, v.z);

                  Control *control = inode->GetTMController();
                  MakeControlsTCB(control);
                  control->GetPositionController()->SetValue(0, (void*)&v);
               }
               break;

            case CT_NODE_ROTATION:
               {
                  Point3 axis;
                  float angle;
                  C_chunk::Read(&axis, sizeof(S_vector));
                  C_chunk::Read(&angle, sizeof(float));
                  swap(axis.y, axis.z);

                  Control *control = inode->GetTMController();
                  MakeControlsTCB(control);

                  Quat q = QFromAngAxis(-angle, axis);
                  control->GetRotationController()->SetValue(0, &q);

                  C_chunk::Descend();
               }
               break;

            case CT_NODE_SCALE:
               {
                  S_vector s = C_chunk::RVectorChunk();
                  ScaleValue ss(Point3(s.x, s.z, s.y));

                  Control *control = inode->GetTMController();
                  MakeControlsTCB(control);
                  control->GetScaleController()->SetValue(0, (void*)&ss);
               }
               break;

            case CT_TRACK_POS:
            case CT_TRACK_ROT:
            case CT_TRACK_SCL:
               {
                  S_I3D_track_header header;
                  C_chunk::Read((char*)&header, sizeof(header));

                  int read_keys = Max(1, (int)header.num_keys);
                  Control *control = inode->GetTMController();
                  MakeControlsTCB(control);

                  switch(ck_t){
                  case CT_TRACK_POS:
                     {
                        vector<S_I3D_position_key> keys(read_keys);
                        for(int i=0; i<read_keys; i++){
                           keys[i].Read(C_chunk::GetHandle());
                           S_vector &v = keys[i].data;
                           swap(v.y, v.z);
                        }
                        SetPositionKeys(control->GetPositionController(), keys);
                     }
                     break;
                  case CT_TRACK_ROT:
                     {
                        vector<S_I3D_rotation_key> keys(read_keys);
                        for(int i=0; i<read_keys; i++)
                           keys[i].Read(C_chunk::GetHandle());
                        SetRotationKeys(control->GetRotationController(), keys);
                     }
                     break;
                  case CT_TRACK_SCL:
                     {
                        vector<S_I3D_scale_key> keys(read_keys);
                        for(int i=0; i<read_keys; i++){
                           keys[i].Read(C_chunk::GetHandle());
                           S_vector &v = keys[i].data;
                           swap(v.y, v.z);
                        }
                        SetScaleKeys(control->GetScaleController(), keys);
                     }
                     break;
                     /*
                  case CT_TRACK_COLOR:
                     {
                        Control *cont = (Control*)_interface->CreateInstance(CTRL_POINT3_CLASS_ID, Class_ID(TCBINTERP_POINT3_CLASS_ID, 0));
                        GenLight *lt = (GenLight*)obj;
                        lt->SetColorControl(cont);

                        vector<S_I3D_color_key> keys(read_keys);
                        for(int i=0; i<read_keys; i++)
                           keys[i].Read(C_chunk::GetHandle());
                        SetColorKeys(cont, keys);
                     }
                     break;
                     */
                  default: assert(0);
                  }
                  C_chunk::Descend();
               }
               break;

            case CT_TRACK_NOTE:
               {
                  dword num_tracks = 0;
                  C_chunk::Read(&num_tracks, sizeof(dword));
                  for(dword i = 0; i<num_tracks; i++){
                     DefNoteTrack *trk = (DefNoteTrack*)NewDefaultNoteTrack();
                     dword nun_keys = 0;
                     C_chunk::Read(&nun_keys, sizeof(dword));
                     for(dword j = 0; j<nun_keys; j++){
                        TimeValue t;
                        C_chunk::Read(&t, sizeof(TimeValue));
                        TSTR buffer;
                        if(!ReadString(buffer))
                           return false;
                        trk->AddNewKey(t*GetTicksPerFrame(), 0);
                        trk->keys[j]->note = buffer;
                     }
                     inode->AddNoteTrack(trk);
                  }
                  C_chunk::Descend();
               }
               break;

            case CT_TRACK_VISIBILITY:
               {
                  S_I3D_track_header header;
                  C_chunk::Read((char*)&header, sizeof(header));

                  Control *ctrl = (Control*)CreateInstance(CTRL_FLOAT_CLASS_ID, Class_ID(HYBRIDINTERP_FLOAT_CLASS_ID, 0));

                  SuspendAnimate();
                  AnimateOn();

                  inode->AssignController(ctrl, 0);
                  inode->SetVisController(ctrl);

                  for(dword i = 0; i<header.num_keys; i++){
                     S_I3D_visibility_key key;
                     C_chunk::Read(&key, sizeof(key));
                     TimeValue t = key.time;
                     t *= GetTicksPerFrame();
                     ctrl->AddNewKey(t, key.visibility);
                     inode->SetVisibility(t, key.visibility);
                  }
                  ResumeAnimate();

                  C_chunk::Descend();
               }
               break;

            case CT_PIVOT_OFFSET:
               {
                  S_vector v = C_chunk::RVectorChunk();
                  swap(v.y, v.z);
                  if(!merge_tracks_mode)
                     imp_node->SetPivot((Point3&)-v);
               }
               break;

            default:
               assert(0);
               C_chunk::Descend();
            }
         }
         if(!merge_tracks_mode && !parent_set){
            _interface->GetRootNode()->AttachChild(inode);
         }
      }
      break;

   case CT_BASE:
      {
         while(C_chunk::Size()){
            switch(ck_t = C_chunk::RAscend()){
            case CT_NUM_MATERIALS:
               C_chunk::Descend();
               break;
            case CT_MATERIAL:
               {
                  S_material mat;
                  if(!GetMaterialChunk(mat))
                     return false;
                  AddMaterial(mat);
               }
               break;     
            default:
               if(!GetChunk(ck_t, NULL))
                  return false;
            }
         }
      }
      break;
      /*
   case N_NURBS:
      {
         NURBSCVSurface *cc = new NURBSCVSurface();
         while(C_chunk::Size()){
            if(!RAscend(tag))
               return false;
            switch(tag){
            case NURBS_UV_ORDER:
            case NURBS_CVS:
            case NURBS_KNOTS:   
            case NURBS_TRANSFORM_MATRIX:
            case NURBS_MAT:
            case NURBS_FLIP_NORMALS:
            case NURBS_UV_MAP:
               if(GetChunk(tag, cc)==0)
                  return false;
               break;
            default :
               Descend();
            }
         }
         if(_nset.GetNumObjects() == 1)
            _nset.RemoveObject(0);
         _nset.AppendObject(cc);
      }
      break;

   case NURBS_UV_MAP:
      {
         NURBSCVSurface *cc = (NURBSCVSurface *)data;
         float uoffset, voffset, utile, vtile;
         C_chunk::Read(&uoffset, sizeof(float));
         C_chunk::Read(&voffset, sizeof(float));
         C_chunk::Read(&utile, sizeof(float));
         C_chunk::Read(&vtile, sizeof(float));

         Point2 pt;
         for(int j = 0;j<4;j++){
            C_chunk::Read(&pt.x, sizeof(float));
            C_chunk::Read(&pt.y, sizeof(float));
            cc->SetTextureUVs(0, j, pt);
         }
      }
      break;

   case NURBS_MAT:
      {
         NURBSCVSurface *cc = (NURBSCVSurface *)data;

         TSTR buffer;

         if(!_tchar_read_string(this, buffer)) 
            return false;

         int mtlnum = Max(0, loaded_materials.FindMtlByName(CStr(buffer.data())));

         cc->MatID(mtlnum);
         mtlnum = mtlnum;
      }
      break;

   case NURBS_FLIP_NORMALS:
      {
         NURBSCVSurface *cc = (NURBSCVSurface *)data;
         bool FlipNormals;
         C_chunk::Read(&FlipNormals, sizeof(bool));
         cc->FlipNormals(FlipNormals);
      }
      break;

   case NURBS_TRANSFORM_MATRIX:
      {
         NURBSCVSurface *cc = (NURBSCVSurface *)data;
         Matrix3 TransformMatrix;
         C_chunk::Read(&TransformMatrix, sizeof(Matrix3));
         SetXFormPacket mat(TransformMatrix);
         cc->SetTransformMatrix(0, mat);
      }
      break;

   case NURBS_KNOTS:
      {
         NURBSCVSurface *cc = (NURBSCVSurface *)data;
         int numuknots, numvknots;
         C_chunk::Read(&numuknots, sizeof(int));
         C_chunk::Read(&numvknots, sizeof(int));
         cc->SetNumUKnots(numuknots);
         cc->SetNumVKnots(numvknots);
         double knot;
         for(int i=0;i<numuknots;i++){
            C_chunk::Read(&knot,sizeof(double));
            cc->SetUKnot(i, knot);            
         }
         for(i=0;i<numvknots;i++){
            C_chunk::Read(&knot,sizeof(double));
            cc->SetVKnot(i, knot);            
         }
      }
      break;

   case NURBS_UV_ORDER:
      {
         int uorder, vorder;
         NURBSCVSurface *cc = (NURBSCVSurface *)data;
         C_chunk::Read(&uorder, sizeof(int));
         C_chunk::Read(&vorder, sizeof(int));
         cc->SetUOrder(uorder);
         cc->SetVOrder(vorder);
      }
      break;

   case NURBS_CVS:
      {
         float x, y, z, w;
         int ix, iy;
         NURBSCVSurface *cc = (NURBSCVSurface *)data;
         C_chunk::Read(&ix, sizeof(int));
         C_chunk::Read(&iy, sizeof(int));
         cc->SetNumCVs(ix, iy);
         cc->SetName(_T("CV Surface"));
         NURBSControlVertex cv;

         for(int i=0;i<ix;i++){
            for(int j=0;j<iy;j++){
               C_chunk::Read(&x,sizeof(float));
               C_chunk::Read(&y,sizeof(float));
               C_chunk::Read(&z,sizeof(float));
               C_chunk::Read(&w,sizeof(float));
               cv.SetPosition(0, Point3(x, y, z));
               cv.SetWeight(0, w);
               cc->SetCV(i, j, cv);               
            }
         }
      }
      break;
      */
   default:
      assert(0);
      return false;
   }
   Descend();
   return true;
}

//----------------------------
void __fastcall SetBreakAlloc(dword);
int ImportOld(const TCHAR *filename, ImpInterface *i, Interface *gi, BOOL suppressPrompts);

//----------------------------
//----------------------------

static BOOL CALLBACK dlgStatus(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

   switch(uMsg){
   case WM_INITDIALOG:
      {
         CenterDialog(hwnd);
         const char *cp = (const char*)lParam;
         int len = strlen(cp);
         vector<char> buf;
         buf.reserve(len*2);
         for(int i=0; i<len; i++){
            char c = cp[i];
            if(c=='\n')
               buf.push_back('\r');
            buf.push_back(c);
         }
         buf.push_back(0);
         SetDlgItemText(hwnd, IDC_EDIT1, buf.begin());
      }
      return 1;
   case WM_COMMAND:
      switch(LOWORD(wParam)){
      case IDOK:
         EndDialog(hwnd, 0);
         break;
      }
      break;
   }
   return 0;
}

//----------------------------
enum E_MODE{
   IMPORT_REPLACE,
   IMPORT_MERGE,
   IMPORT_MERGE_ANIM,
};

class StudioImport: public SceneImport{
   E_MODE mode;
public:
   StudioImport(E_MODE m):
      mode(m)
   {
   }

   virtual int ExtCount(){ return 1; }

//----------------------------

   virtual const TCHAR *Ext(int n){
      switch(n) {
      case 0: return "i3d";
      }
      return _T("");
   }

   virtual const TCHAR *LongDesc(){ return "Insanity3D import"; }
   virtual const TCHAR *ShortDesc(){
      switch(mode){
      case IMPORT_REPLACE: return "Insanity3D (replace)";
      case IMPORT_MERGE: return "Insanity3D (merge)";
      case IMPORT_MERGE_ANIM: return "Insanity3D (merge tracks)";
      default: return "";
      }
   }
   virtual const TCHAR *AuthorName(){ return "Lonely Cat Games"; }
   virtual const TCHAR *CopyrightMessage(){ return "Copyright (c) 2002 Lonely Cat Games"; }
   virtual const TCHAR *OtherMessage1(){ return _T(""); }
   virtual const TCHAR *OtherMessage2(){ return _T(""); }
   virtual unsigned int Version(){ return 200; }
   virtual void ShowAbout(HWND hWnd){ }
   virtual int ZoomExtents(){ return ZOOMEXT_YES; }

   virtual int DoImport(const TCHAR *filename, ImpInterface *i, Interface *gi, BOOL suppressPrompts){

      PC_dta_stream dta = DtaCreateStream(filename);
      if(!dta)
         return IMPEXP_FAIL;
      word header = 0;
      dta->Read(&header, sizeof(word));
      dta->Release();
      if(header==M3DMAGIC){
         //return ImportOld(filename, i, gi, suppressPrompts);
         return IMPEXP_FAIL;
      }

      C_import imp(i, gi);
      if(!imp.ROpen(filename))
         return IMPEXP_FAIL;

      imp.merge_tracks_mode = (mode == IMPORT_MERGE_ANIM);
      imp.merge_mode = (mode==IMPORT_MERGE);

      if(mode==IMPORT_REPLACE){
         if(!i->NewScene())
            return IMPEXP_CANCEL;
      }
      i->RedrawViews();

      SetCursor(LoadCursor(NULL, IDC_WAIT));

      CK_TYPE tag = imp.RAscend();
      if(tag != CT_BASE)
         return IMPEXP_FAIL;

      SetFrameRate(100);
      bool status = imp.GetChunk(tag, NULL);

      SetCursor(LoadCursor(NULL, IDC_ARROW));

      if(status){
         imp.SetupEnvironment();
         i->RedrawViews();
      }
      //gi->ClearNodeSelection(true);

      if(imp.num_imported){
         imp.status = C_fstr("Animations imported: %i\n", imp.num_imported) + imp.status;
      }

      if(imp.status.Size()){
         DialogBoxParam(hInstance, "IDD_STATUS", gi->GetMAXHWnd(), dlgStatus, (LPARAM)(const char*)imp.status);
      }

      return status ? IMPEXP_SUCCESS : IMPEXP_FAIL;
   }
};

//----------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL,ULONG fdwReason,LPVOID lpvReserved){

   static bool controlsInit = false;
   if(!controlsInit){
      controlsInit = true;
      
      InitCustomControls(hinstDLL);
      InitCommonControls();
      hInstance = hinstDLL;
   }
   return true;
}

//----------------------------
//----------------------------

static class StudioClassDesc: public ClassDesc{
   E_MODE mode;
public:
   StudioClassDesc(E_MODE m):
      mode(m)
   {}
   virtual int IsPublic(){ return 1; }
   void *Create(BOOL loading = FALSE){ return new StudioImport(mode); }
   const TCHAR *ClassName(){
      switch(mode){
      case IMPORT_REPLACE: return "I3D Import (replace)";
      case IMPORT_MERGE: return "I3D Import (merge)";
      case IMPORT_MERGE_ANIM: return "I3D Import (merge tracks)";
      default: return "";
      }
   }
   SClass_ID SuperClassID(){ return SCENE_IMPORT_CLASS_ID; }
   Class_ID ClassID(){ return Class_ID(0x694e4941, 0x70491ec0 + mode); }
   const TCHAR *Category(){ return "Import"; }
} StudioDesc(IMPORT_REPLACE), StudioDesc_merge(IMPORT_MERGE), StudioDesc_merge_anim(IMPORT_MERGE_ANIM);

//----------------------------

__declspec(dllexport) int LibNumberClasses(){ return 3; }

//----------------------------

__declspec(dllexport) const TCHAR *LibDescription(){ return "I3D file import"; }

//----------------------------

__declspec(dllexport) ClassDesc *LibClassDesc(int i){

   switch(i){
   case 0: return &StudioDesc; break;
   case 1: return &StudioDesc_merge; break;
   case 2: return &StudioDesc_merge_anim; break;
   }
   return NULL;
}

//----------------------------

__declspec(dllexport) ULONG LibVersion(){ return VERSION_3DSMAX; }

//----------------------------
//----------------------------
