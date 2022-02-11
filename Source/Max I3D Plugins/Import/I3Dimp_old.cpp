#include "..\common\pch.h"
#include "..\common\common.h"
#include "..\common\mtldef.h"

//----------------------------

#define MAX_NAME_SIZE 256

//----------------------------

#define MESHVERSION 0x3D3E
#define MASTER_SCALE 0x0100
#define N_TRI_OBJECT 0x4100
#define POINT_FLAG_ARRAY 0x4111
#define MSH_MAT_GROUP 0x4130
#define TEX_VERTS 0x4140
#define MESH_MATRIX 0x4160
#define N_DIRECT_LIGHT 0x4600
#define DL_DIRLIGHT    0x4612
#define DL_SPOTLIGHT 0x4610
#define DL_OFF 0x4620
#define DL_ATTENUATE 0x4625
#define DL_INNER_RANGE 0x4659
#define DL_OUTER_RANGE 0x465A
#define DL_MULTIPLIER 0x465B
#define N_CAMERA 0x4700
#define CAM_RANGES 0x4720
#define N_DIRECT_LIGHT_TARGET    0x4602
#define N_BONE                   0x49dd
#define MAT_AMBIENT     0xA010
#define MAT_DIFFUSE     0xA020
#define MAT_SPECULAR    0xA030
#define MAT_TRANSPARENCY   0xA050
#define MAT_SELF_ILPCT  0xA084
#define M3D_VERSION 0x0002
#define KFDATA  0xB000
#define OBJECT_NODE_TAG  0xB002
#define CAMERA_NODE_TAG  0xB003
#define TARGET_NODE_TAG  0xB004	         //cameras only
#define LIGHT_NODE_TAG  0xB005
#define L_TARGET_NODE_TAG  0xB006	      //lights only
#define SPOTLIGHT_NODE_TAG  0xB007
//#define KFSEG  0xB008
#define KFCURTIME  0xB009
#define KFHDR 0xB00A
#define NODE_HDR  0xB010
#define INSTANCE_NAME 0xB011
#define PRESCALE 0xB012
#define FOV_TRACK_TAG 0xB023
#define ROLL_TRACK_TAG 0xB024
//#define COL_TRACK_TAG 0xB025
#define HOT_TRACK_TAG 0xB027
#define FALL_TRACK_TAG 0xB028
#define NODE_ID 0xB030
#define DIRECTLIGHT_NODE_TAG  0xB00C
#define BONE_NODE_TAG        0xB00E


//----------------------------
#define VWRAP (1<<11)      /* Texture coord V wraps on this face */
#define UWRAP (1<<3)    /* Texture coord U wraps on this face */

// Node list structure

#define OBJ_MESH 0
#define OBJ_OMNILIGHT 1
#define OBJ_SPOTLIGHT 2
#define OBJ_CAMERA 3
#define OBJ_DUMMY 4
#define OBJ_TARGET 5
#define OBJ_OTHER  6 // generated from app data
#define OBJ_NURBS  7
#define OBJ_BONE   8

// key types
#define KEY_FLOAT 0
#define KEY_POS      1
#define KEY_ROT      2
#define KEY_SCL      3
#define KEY_COLOR 4

//#define NUMTRACKS 8
#define _NUMTRACKS 4

#define POS_TRACK_INDEX 0
#define ROT_TRACK_INDEX 1
#define SCL_TRACK_INDEX 2
#define FOV_TRACK_INDEX 3
#define ROLL_TRACK_INDEX 4
#define COL_TRACK_INDEX 5
#define HOT_TRACK_INDEX 6
#define FALL_TRACK_INDEX 7

#define OBJ_NAME_LEN 100

/* Light flag bit meanings */

#define NO_LT_ON  0x0001
#define NO_LT_SHAD   0x0002
#define NO_LT_LOCAL  0x0004
#define NO_LT_CONE   0x0008
#define NO_LT_RECT   0x0010
#define NO_LT_PROJ   0x0020
#define NO_LT_OVER   0x0040
#define NO_LT_ATTEN  0x0080
#define NO_LT_RAYTR  0x0100
#define NO_LT_TEMP_APPDATA 0x0200 /* Free appdata after rendering complete  */   

#define NO_LT_OFF (~NO_LT_ON)
#define NO_LT_SHAD_OFF  (~NO_LT_SHAD)
#define NO_LT_LOCAL_OFF (~NO_LT_LOCAL)
#define NO_LT_CONE_OFF  (~NO_LT_CONE)
#define NO_LT_RECT_OFF  (~NO_LT_RECT)
#define NO_LT_PROJ_OFF  (~NO_LT_PROJ)
#define NO_LT_OVER_OFF  (~NO_LT_OVER)
#define NO_LT_ATTEN_OFF (~NO_LT_ATTEN)

// key types
#define KEY_FLOAT 0
#define KEY_POS   1
#define KEY_ROT   2
#define KEY_SCL   3
#define KEY_COLOR 4

#pragma pack()

// face edge vis flags
#define ABLINE (1<<2)
#define BCLINE (1<<1)
#define CALINE 1

/* these flags specify which coords are NOT inherited from parent */ 
#define LNKSHFT 7
#define NO_LNK_X (1<<LNKSHFT)
#define NO_LNK_Y (1<<(LNKSHFT+1))
#define NO_LNK_Z (1<<(LNKSHFT+2))

// A worker object for dealing with creating the objects.
// Useful in the chunk-oriented 3DS file format

#define MAX_NUM_OF_MATERIALS 4096

// Worker types

#define _WORKER_IDLE     0
#define _WORKER_MESH     1
#define _WORKER_KF    2
#define _WORKER_LIGHT 4
#define _WORKER_CAMERA   5
#define _WORKER_NURBS   6
#define _WORKER_BONE   7

struct Dirlight{
   float x;
   float y;
   float z;
   float tx;
   float ty;
   float tz;
   unsigned short flags;
   S_vector color;
   float hotsize;
   float fallsize;
   float lo_bias;
   int shadsize;
   float in_range,out_range;     //attenuation range
   float shadfilter;             //size of filter box
   char imgfile[13];
   float ray_bias;
   float bank,aspect;            //spotlight bank angle, aspect ratio
   float mult;                   //light multiplier
   void *appdata;
   NameTab excList;
};


struct Camera3DS{
   float x;
   float y;
   float z;
   float tx;
   float ty;
   float tz;
   float bank;
   float focal;
   unsigned short flags;
   float nearplane;
   float farplane;
   void *appdata;
};

struct Faces{
   unsigned short a;
   unsigned short b;
   unsigned short c;
   unsigned char material;
   unsigned char filler;
   unsigned long sm_group;
   unsigned short flags;
};

struct Chunk_hdr{
   unsigned short tag;
   long size;
};


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

class ObjWorker: public C_chunk{

// Additional data associated with node during loading.
   struct S_node_entry{

      short id;
      int type;
      int mnum;
      C_str name;
      Mesh *_mesh;
      vector<S_I3D_position_key> pos_list;
      vector<S_I3D_rotation_key> rot_list;
      vector<S_I3D_scale_key> scl_list;
      vector<S_I3D_color_key> col_list;
      /*
      vector<S_key> hotList;
      vector<S_key> fallList;
      vector<S_key> fovList;
      vector<S_key> rollList;
      */
      word _track_flags[_NUMTRACKS];

      S_node_entry(){}
      S_node_entry(const S_node_entry &ne){ operator =(ne); }
      S_node_entry &operator =(const S_node_entry &ne){
         id = ne.id;
         type = ne.type;
         mnum = ne.mnum;
         name = ne.name;
         _mesh = ne._mesh;
         pos_list = ne.pos_list;
         rot_list = ne.rot_list;
         scl_list = ne.scl_list;
         col_list = ne.col_list;
         memcpy(_track_flags, ne._track_flags, sizeof(_track_flags));
         return *this;
      }
   };

   int node_load_id;

   short _cur_node_id;
   ImpNode *_obnode;
   char _obname[101];

   /*
   struct S_background{
      int bgType;
      int envType;
      S_vector bkgd_solid;
      S_vector amb_light;
      Bkgrad bkgd_gradient;
      Fogdata fog_data;
      Distcue distance_cue;
      int fog_bg, dim_bg;
      char bkgd_map[81];
      LFogData lfog_data;
   } _background;
   */

//----------------------------

   bool _GetIntFloatChunk(short &ret){

      ret = 0;
      while(C_chunk::Size()){
         switch(C_chunk::RAscend()){
         case CT_PERCENTAGE:
            ret = C_chunk::RWordChunk();
            break;
            /*
         case FLOAT_PERCENTAGE:
            ret = C_chunk::RFloatChunk() * 100.0f;
            break;
            */
         default:
            assert(0);
            C_chunk::Descend();
         }
      }
      C_chunk::Descend();
      return true;
   }

//----------------------------

   /*
   static Object *CreateObjectFromAppData(TriObject *tobj, void *data, DWORD len){

      Chunk_hdr *sub, *hdr = (Chunk_hdr*)data;
      Object *obj;
      char idstring[100];
      int noff, nbytes = (int)len;  

      noff = 0;

      while(noff<nbytes){
         if(hdr->tag != XDATA_ENTRY)
            goto next;
         sub = SubChunk(hdr);
         if(sub->tag!= XDATA_APPNAME)
            goto next;
                              //try to convert it to an object
         GetIDStr((char*)hdr,idstring);
         obj = ObjectFromAppData(tobj, idstring, (void*)sub, hdr->size-6);
         if(obj)
            return obj;
      next:
         if(hdr->size<0)
            break; 
         noff += hdr->size;
         hdr = NextChunk(hdr);
      }
      return NULL;
   }
   */

//----------------------------
// Match 3DStudio's "Default" material
   static StdMat *_New3DSDefaultMtl(){

      StdMat *m = NewDefaultStdMat();
      m->SetName(_T("Default"));
      m->SetAmbient(Color(.7f,.7f,.7f), 0);
      m->SetDiffuse(Color(.7f,.7f,.7f), 0);
      m->SetSpecular(Color(1.0f,1.0f,1.0f), 0);
      m->SetShininess(0.5f, 0);
      m->SetShinStr(.707f, 0);
      m->EnableMap(0, true);
      return m;
   }

//----------------------------

   static bool IsSXPName(const char *name){

      char fname[30];
      char ext[5];
      _splitpath(name, NULL, NULL, fname, ext );
      return stricmp(ext, ".sxp")==0 ? 1 : 0;
   }

//----------------------------

   static Texmap *MakeTex(S_map_data &map, S_material *smtl, BOOL &wasSXP){

      Texmap *txm; 
      wasSXP = false;
      //if(map.kind==0){
         if(IsSXPName(map.bitmap_name)){
            Tex3D *t3d = GetSXPReaderClass((char*)(const char*)map.bitmap_name);
            if(t3d){
               /*
               if(map.p.tex.sxp_data){
                  ULONG *p = (ULONG *)map.p.tex.sxp_data;
                  t3d->ReadSXPData(map.map_name, (void *)(p+1));
                  wasSXP = TRUE;
               }
               */
            }
            txm = t3d;                    
         }else{
            BitmapTex *bmt = NewDefaultBitmapTex();
            bmt->ActivateTexDisplay(true);
            bmt->SetMapName((char*)(const char*)map.bitmap_name);
            //S_map_params &par = map.p.tex;
            bmt->SetAlphaAsMono(map.flags&MAP_ALPHA_SOURCE);
            bmt->SetAlphaSource((map.flags&MAP_DONT_USE_ALPHA) ? ALPHA_NONE : ALPHA_FILE);
            bmt->SetFilterType((map.flags&MAP_SAT ) ? FILTER_SAT : FILTER_PYR);
            StdUVGen *uv = bmt->GetUVGen();
            uv->SetUOffs(map.uv_offset.x, 0);
            uv->SetVOffs(-map.uv_offset.y, 0);
            uv->SetUScl(map.uv_scale.x, 0);
            uv->SetVScl(map.uv_scale.y, 0);
            //uv->SetAng(-((float)atan2(par.ang_sin, par.ang_cos)),0);
            //uv->SetBlur(map.texture_blur, 0);
            int tile=0;
            if(map.flags&MAP_UV_MIRROR)
               tile|= U_MIRROR|V_MIRROR;
            else{
            if (0==(map.flags&MAP_NOWRAP)) tile|= U_WRAP|V_WRAP;
            }
            uv->SetTextureTiling(tile);
            TextureOutput *txout = bmt->GetTexout();
            txout->SetInvert(map.flags&MAP_INVERT);
            txm = bmt;
         }
         /*
         if(map.flags&MAP_TINT){
            // map.p.tex.col1, col2 : stuff into Mix  
            MultiTex* mix = NewDefaultMixTex();
            mix->SetColor(0, (Color&)ColorFrom24(map.col1));
            mix->SetColor(1, (Color&)ColorFrom24(map.col2));
            mix->SetSubTexmap(2, txm);
            txm = mix;        
         }else
         if(map.p.tex.flags&MAP_RGB_TINT){
            // map.p.tex.rcol,gcol,bcol : stuf into tint
            MultiTex* mix = NewDefaultTintTex();
            mix->SetColor(0, (Color&)ColorFrom24(map.p.tex.rcol));
            mix->SetColor(1, (Color&)ColorFrom24(map.p.tex.gcol));
            mix->SetColor(2, (Color&)ColorFrom24(map.p.tex.bcol));
            mix->SetSubTexmap(0, txm);
            txm = mix;        
         }
         */
         /*
      }else{
         // kind == 1 :  Reflection Map
         BitmapTex *bmt = NewDefaultBitmapTex();
         bmt->SetMapName((char*)(const char*)map.map_name);
         StdUVGen *uv = bmt->GetUVGen();

         // TBD: REFLECTION BLUR SETTING:
         uv->SetBlurOffs((float)smtl->refblur/400.0f+.001f,0);
         bmt->InitSlotType(MAPSLOT_ENVIRON);
         txm = bmt;
      }
      */
      return txm;
   }

//----------------------------

   void _ParseIKData(INode *node){

      AppDataChunk *ad = 
         node->GetAppDataChunk(node->ClassID(),node->SuperClassID(),0);
      if(!ad)
         return;
                              //Get the IK data chunk
      Chunk_hdr *hdr = (Chunk_hdr*)
         GetAppDataChunk(ad->data, ad->length, "IK KXPv1 62094j39dlj3i3h42");
      if(!hdr)
         return;

      // Get the joint data sub chunk
      Chunk_hdr *nhdr = hdr+1;
      DWORD len = hdr->size - nhdr->size - 6;
      void *ikdata = (void*)(((char*)nhdr)+nhdr->size);
      hdr = (Chunk_hdr*)GetAppDataChunk(ikdata,len,"JOINTDATA");
      if(!hdr)
         return;

      // The first 4 bytes is the version number
      nhdr = hdr+1;
      int *version = (int*)(((char*)nhdr)+nhdr->size);
      if(*version!=3)
         return; // gotta be version 3

// 3DS R4 IK joint data
      struct JointData3DSR4{
         int      freeJoints[6];
         float jointMins[6];
         float jointMaxs[6];
         float precedence[6];
         float damping[6];
         int      limited[6];
         int      ease[6];
         int      lastModified;
      };

      // Then the joint data
      JointData3DSR4 *jd = (JointData3DSR4*)(version+1);

      // Copy it into our data structures
      InitJointData posData, rotData;
      for(int i=0; i<3; i++){
         posData.active[i]  = jd->freeJoints[i+3];
         posData.limit[i]   = jd->limited[i+3];
         posData.ease[i]    = jd->ease[i+3];
         posData.min[i]     = jd->jointMins[i+3];
         posData.max[i]     = jd->jointMaxs[i+3];
         posData.damping[i] = jd->damping[i+3];

         rotData.active[i]  = jd->freeJoints[i];
         rotData.limit[i]   = jd->limited[i];
         rotData.ease[i]    = jd->ease[i];
         rotData.min[i]     = jd->jointMins[i];
         rotData.max[i]     = jd->jointMaxs[i];
         rotData.damping[i] = jd->damping[i];
      }
                              //give the data to the TM controller
      node->GetTMController()->InitIKJoints(&posData,&rotData);
   }

//----------------------------

   void Reset(){
      _mode = _WORKER_IDLE;
      _tm.IdentityMatrix();
      _gotverts = false;
      _gottverts = false;
      _gotfaces = false;
      _verts = _faces = 0;
      _object = NULL;

      _bone = NULL;
      _light = NULL;
      _camera=NULL;
      _dummy = NULL;
      _mesh = NULL;
      _parentNode = _thisNode = NULL;
      // NEW INITS
      _workNode = NULL;  
      _pivot = Point3(0, 0, 0);
      //newTV.SetCount(0);
      _newTV.clear();
   }

//----------------------------

   dword _AddNewTVert(UVVert p){

                              //func shouldn't be called!
      assert(0);
      return -1;
   }

//----------------------------

   Object *_FindObjFromNode(ImpNode *node){

      t_node_list::iterator it=node_list.find(node);
      if(it!=node_list.end()){
         t_obj_map::const_iterator it1 = _obj_map.find(C_str((*it).second.name));
         if(it1!=_obj_map.end())
            return (*it1).second.object;
         assert(0);
         return NULL;
      }
      return NULL;
   }

//----------------------------

   static void _check_for_wrap(UVVert *tv, int flags){

      float d;
      if(flags&UWRAP){
         float maxu,minu;
         maxu = minu = tv[0].x;
         if (tv[1].x>maxu) maxu = tv[1].x;
         else if (tv[1].x<minu) minu = tv[1].x;
         if (tv[2].x>maxu) maxu = tv[2].x;
         else if (tv[2].x<minu) minu = tv[2].x;
         if((maxu-minu)>0.8f){
            d = (float)ceil(maxu-minu);
            if (tv[0].x<.5f)  tv[0].x += d; 
            if (tv[1].x<.5f)  tv[1].x += d; 
            if (tv[2].x<.5f)  tv[2].x += d; 
         }
      }
      if(flags&VWRAP){
         float maxv,minv;
         maxv = minv = tv[0].y;
         if (tv[1].y>maxv) maxv = tv[1].y;
         else if (tv[1].y<minv) minv = tv[1].y;
         if (tv[2].y>maxv) maxv = tv[2].y;
         else if (tv[2].y<minv) minv = tv[2].y;
         if((maxv-minv)>0.8f){
            d = (float)ceil(maxv-minv);
            if (tv[0].y<.5f)  tv[0].y += d; 
            if (tv[1].y<.5f)  tv[1].y += d; 
            if (tv[2].y<.5f)  tv[2].y += d; 
         }
      }
   }

//----------------------------

public:
   ImpInterface *imp_interface;
   Interface *_interface;
   TSTR _name;
   int _mode;
   bool _gotverts;
   int _verts;
   int _tverts;
   bool _gottverts;
   bool _gotfaces;
   int _faces;

   TriObject *_object;
   GenLight *_light;
   Object  *_bone;
   GenCamera *_camera;

   Dirlight _studioLt;
   Camera3DS _studioCam;
   Mesh *_mesh;
   ImpNode *_thisNode;
   ImpNode *_parentNode;

//----------------------------
// Work object - used by old loader at 1st phase to load meshes, cameras, etc.
   class _C_work_object{
      _C_work_object &operator =(const _C_work_object&);
   public:
      Object *object;
      S_vector srcPos;
      S_vector targPos;
      int type;
      int used;
      int cstShad;
      int rcvShad;
      int mtln;
      Matrix3 tm;

      _C_work_object():
         used(false)
      {}
   };
                              //work objects identified by name
   typedef map<C_str, _C_work_object> t_obj_map;
   t_obj_map _obj_map;

   typedef map<ImpNode*, S_node_entry> t_node_list;
   t_node_list node_list;

   S_node_entry *_workNode;
   Matrix3 _tm;
   Point3 _pivot;
   DummyObject *_dummy;
   int _lightType;
   TSTR _nodename;

   bool _length_set;
   TimeValue _anim_length;

   //bool segmentSet;
   //Interval segment; 

   MtlList loaded_materials;
   vector<vector<Mesh*> > _mat_meshes;
   //void *appdata;
   //dword appdataLen;
   vector<UVVert> _newTV;

//----------------------------

   ObjWorker(ImpInterface *iptr, Interface *ip):
      node_load_id(-1)
   {
      //memset(&_background, 0, sizeof(_background));
      imp_interface = iptr;
      _dummy = NULL;
      Reset();
      _length_set = false;
      //segmentSet = false;
      //appdataLen = 0;
      _interface = ip;
   }

//----------------------------

   ~ObjWorker() {

      _FinishUp();
      _obj_map.clear();
      node_list.clear();
      imp_interface->RedrawViews();
   }

//----------------------------

   bool _FinishUp(){

      switch(_mode){

      case _WORKER_IDLE:
         return true;

      case _WORKER_MESH:
         {
                              //must have defined verts!
            if(_gotverts){

               // Make this an inverse of the mesh matrix.  This sets up a transform which will
               // be used to transform the mesh from the 3DS Editor-space to the neutral space
               // used by the keyframer.
               Matrix3 itm = Inverse(_tm);
               int ix;  
      
               // Transform verts through the inverted mesh transform
               for(ix=0; ix<_verts; ++ix){
                  Point3 &p = _mesh->getVert(ix);
                  p = p * itm;
                  _mesh->setVert(ix,p);
               }

               Point3 cp = CrossProd(_tm.GetRow(0), _tm.GetRow(1));
               if(DotProd(cp, _tm.GetRow(2)) < 0){
                  Matrix3 tmFlipX(1);
                  Point3 row0 = tmFlipX.GetRow(0);
                  row0.x = -1.0f;
                  tmFlipX.SetRow(0,row0);
                              //transform verts through the mirror transform
                  for(ix=0; ix<_verts; ++ix){
                     Point3 &p = _mesh->getVert(ix);
                     p = p * tmFlipX;
                     _mesh->setVert(ix,p);
                  }
               }
               _mesh->buildNormals();
               _mesh->EnableEdgeList(1);            
            }

            if(_gottverts){
               int ntv;
               if((ntv = _newTV.size()) > 0){
                  int oldn = _mesh->numTVerts;
                  _mesh->setNumTVerts(oldn+ntv, TRUE); 
                  for(int i=0; i<ntv; i++)
                     _mesh->tVerts[oldn+i] = _newTV[i];
               }
            }
            /*
            if(appdata){
                              //see if we can create an object from the appdata
               Object *obj = CreateObjectFromAppData(object,appdata,appdataLen);
               if(obj){
                  obj->AddAppDataChunk(
                     triObjectClassID, 
                     GEOMOBJECT_CLASS_ID, 
                     0, appdataLen, appdata);
                  // first need to save away its mtl num
                  int mnum = -1;
                  if (_mesh && _mesh->numFaces)
                     mnum = _mesh->faces[0].getMatID();
                  // Then can toss it
                  object->DeleteThis(); // don't need tri object
                  object = NULL;
                  _AddObject(obj, OBJ_OTHER, name, NULL, mnum);
                  appdata = NULL;
                  Reset();
                  return true;
               }
                              //stick app data in the object
               object->AddAppDataChunk(triObjectClassID, GEOMOBJECT_CLASS_ID, 0, appdataLen, appdata);
               appdata = NULL;
            }
            */
            _AddObject(_object, OBJ_MESH, _name, &_tm);
            Reset();
         }
         return true;

      case _WORKER_KF:
         {
            /*
            if(appdata){
                              //stick app data in the node
               thisNode->GetINode()->AddAppDataChunk(thisNode->GetINode()->ClassID(), 
                  thisNode->GetINode()->SuperClassID(), 0, appdataLen, appdata);
               appdata = NULL;
            }
            */
            int type = _FindTypeFromNode(_thisNode);

            if(_parentNode)
               _parentNode->GetINode()->AttachChild(_thisNode->GetINode());

            switch(type){
            case OBJ_MESH:
            case OBJ_DUMMY:
            case OBJ_TARGET:
            case OBJ_OTHER:
               _thisNode->SetPivot(-_pivot);
               break;
            }
            Reset();
         }
         return true;

      case _WORKER_LIGHT:
         {
                              //must have a light
            if(_light){
               _C_work_object &obj = _AddObject(_light, _lightType, _name, &_tm);
               obj.targPos = S_vector(_studioLt.tx, _studioLt.ty, _studioLt.tz);
               obj.srcPos = S_vector(_studioLt.x, _studioLt.y, _studioLt.z);
               Reset();
               return true;
            }
            Reset();
         }
         return true;

      case _WORKER_BONE:
         {
            if(_bone){
                              //first input; tm not set
               _AddObject(_bone, OBJ_BONE, _name, &_tm);
               Reset();
               return true;
            }
            assert(false);
            Reset();
         }
         return true;

      case _WORKER_CAMERA:
                              //must have a camera
         if(_camera){
            _C_work_object &obj = _AddObject(_camera, OBJ_CAMERA, _name, &_tm);
            obj.targPos = S_vector(_studioCam.tx, _studioCam.ty, _studioCam.tz);
            obj.srcPos = S_vector(_studioCam.x, _studioCam.y, _studioCam.z);
            Reset();
            return true;
         }
         Reset();
         return true;
      }
                              //undefined state
      return false;
   }

//----------------------------

   bool _StartMesh(const char *iname){

      if(!_FinishUp())
         return false;

      _tm.IdentityMatrix();
      _name = TSTR(iname);
      _object = CreateNewTriObject();
      if(!_object)
         return false;

      _mesh = &_object->GetMesh();
      _mode = _WORKER_MESH;
      //newTV.SetCount(0);
      _newTV.clear();
      return true;
   }

//----------------------------

   bool _StartBone(const char *iname){

      if(!_FinishUp())
         return false;
      _tm.IdentityMatrix();
      _name = TSTR(iname);
      _bone = NULL;
      _mode = _WORKER_BONE;  
      return true;
   }

//----------------------------

   bool _CreateBone(){

      SuspendAnimate();
      AnimateOff();
                                
      //_bone = (Object*)imp_interface->Create(HELPER_CLASS_ID,Class_ID(BONE_CLASS_ID, 0));
      _bone = (Object*)imp_interface->Create(JOINT_SUPERCLASSID, JOINT_CLASSID);
      IParamBlock2 *pb = ((Animatable*)_bone)->GetParamBlock(0);
      pb->SetValue(jointobj_width, 0, .1f);
      pb->SetValue(jointobj_length, 0, .1f);

      ResumeAnimate();
      return true;
   }

//----------------------------

   bool _StartLight(const char *iname){

      if(!_FinishUp())
         return false;
      _tm.IdentityMatrix();
      _name = TSTR(iname);
      _light = NULL;
      _mode = _WORKER_LIGHT; 
      return true;
   }

//----------------------------

   bool _CreateLight(int type){

      _light = imp_interface->CreateLightObject(type);
      if(!_light){
         DebugPrint("Light type %d create failure\n",type);
         return false;
      }

      _lightType = (type == OMNI_LIGHT) ? OBJ_OMNILIGHT : OBJ_SPOTLIGHT;

      SuspendAnimate();
      AnimateOff();

      Point3 col;
      col[0] = _studioLt.color[0];
      col[1] = _studioLt.color[1];
      col[2] = _studioLt.color[2];
      _light->SetRGBColor(0,col);
      _light->SetUseLight(_studioLt.flags&NO_LT_ON);
      _light->SetSpotShape(_studioLt.flags&NO_LT_RECT ? RECT_LIGHT : CIRCLE_LIGHT);
      _light->SetConeDisplay(_studioLt.flags&NO_LT_CONE ? 1 : 0, FALSE);
      _light->SetUseAtten(_studioLt.flags&NO_LT_ATTEN ? 1 : 0);
      _light->SetAttenDisplay(0);
      _light->SetUseGlobal(_studioLt.flags&NO_LT_LOCAL ? 0 : 1);
      _light->SetShadow(_studioLt.flags&NO_LT_SHAD ? 1 : 0);
      _light->SetOvershoot(_studioLt.flags&NO_LT_OVER ? 1 : 0);
      _light->SetShadowType(_studioLt.flags&NO_LT_RAYTR ? 1 : 0);
      _light->SetAtten(0, ATTEN_START, _studioLt.in_range);
      _light->SetAtten(0, ATTEN_END, _studioLt.out_range);
      _light->SetIntensity(0, _studioLt.mult);
      float aspect = 1.0f/_studioLt.aspect;   
      _light->SetAspect(0, aspect);  
      _light->SetMapBias(0, _studioLt.lo_bias);
      _light->SetMapRange(0, _studioLt.shadfilter);
      _light->SetMapSize(0, _studioLt.shadsize);
      _light->SetRayBias(0, _studioLt.ray_bias);
      //_light->SetExclusionList(_studioLt.excList);
      _light->SetHotspot(0,_studioLt.hotsize);
      _light->SetFallsize(0,_studioLt.fallsize);
      _studioLt.excList.SetSize(0);

      ResumeAnimate();
      return true;
   }

//----------------------------

   bool _StartCamera(const char *iname){

      if(!_FinishUp())
         return false;
      _tm.IdentityMatrix();
      _name = TSTR(iname);
      _camera = NULL;
      _mode = _WORKER_CAMERA;
      return true;
   }

//----------------------------

   bool _CreateCamera(int type){

      _camera = imp_interface->CreateCameraObject(type);
      _camera->SetEnvRange(0, 0, _studioCam.nearplane);
      _camera->SetEnvRange(0, 1, _studioCam.farplane);
      _camera->SetFOV(0, DegToRad(2400.0/_studioCam.focal));
      return true;
   }

//----------------------------

   bool _StartKF(ImpNode *node){

      if(!_FinishUp())
         return false;
      _thisNode = node;
      _parentNode = NULL;
      _mode = _WORKER_KF;
      _pivot = Point3(0, 0, 0);
      return true;
   }

//----------------------------

   bool _SetVerts(int count){

      if(_gotverts)
         return false;
      if(!_mesh->setNumVerts(count))
         return false;
      _verts = count;
      _gotverts = true;
      return true;
   }

//----------------------------

   bool _SetTVerts(int count){

      if(_gottverts)
         return false;
      if(!_mesh->setNumTVerts(count))
         return false;
      _tverts = count;
      _gottverts = true;
      return true;
   }

//----------------------------

   bool _SetFaces(int count){

      if(_gotfaces)
         return false;
      if(!_mesh->setNumFaces(count))
         return false;

      if(_gottverts){
         if(!_mesh->setNumTVFaces(count))
            return false;
      }
      _faces = count;
      _gotfaces = true;
      return true;
   }

//----------------------------

   bool _PutTVertex(int index, UVVert *v){

      if(!_gottverts)
         return false;
      if(index<0 || index>=_tverts)
         return false;
      _mesh->setTVert(index, *v);
      return true;
   }

//----------------------------

   bool _PutFace(int index, Faces *f){

      Face *fp = &_mesh->faces[index];
      if(!_gotfaces)
         return false;
      if(index<0 || index>=_faces)
         return false;

                              //if we've got texture vertices, also put out a texture face
      if(_gottverts)
         _SetTVerts(index, f);

      fp->setVerts((int)f->a,(int)f->b,(int)f->c);
      fp->setEdgeVisFlags(f->flags & ABLINE,f->flags & BCLINE,f->flags & CALINE);
      fp->setSmGroup(f->sm_group);
      return true;
   }

//----------------------------

   bool _PutSmooth(int index,unsigned long smooth){

      Face *fp = &_mesh->faces[index];
      if(!_gotfaces)
         return false;
      if(index<0 || index>=_faces)
         return false;
      fp->setSmGroup(smooth);
      return true;
   }

//----------------------------

   void _SetTVerts(int nf, Faces *f){

      UVVert uv[3], uvnew[3]; 
      DWORD ntv[3];
      uvnew[0] = uv[0] = _mesh->tVerts[f->a];
      uvnew[1] = uv[1] = _mesh->tVerts[f->b];
      uvnew[2] = uv[2] = _mesh->tVerts[f->c];
      _check_for_wrap(uvnew, f->flags);
      ntv[0] = (uvnew[0]==uv[0]) ? f->a: _AddNewTVert(uvnew[0]);
      ntv[1] = (uvnew[1]==uv[1]) ? f->b: _AddNewTVert(uvnew[1]);
      ntv[2] = (uvnew[2]==uv[2]) ? f->c: _AddNewTVert(uvnew[2]);
      TVFace *tf = &_mesh->tvFace[nf];
      tf->setTVerts(ntv[0], ntv[1], ntv[2]);
   }

//----------------------------

   _C_work_object &_AddObject(Object *object, int type, const TCHAR *name, Matrix3 *tm, int mtlNum = -1){

      pair<t_obj_map::iterator, bool> p =
         _obj_map.insert(pair<C_str, _C_work_object>(C_str(name), _C_work_object()));
      assert(p.second);
      _C_work_object &obj = (*p.first).second;

      obj.object = object;
      obj.type = type;
      obj.cstShad = true;
      obj.rcvShad = true;
      obj.mtln = mtlNum;
      if(tm)
         obj.tm = *tm;
      else
         obj.tm.IdentityMatrix();

      return obj;
   }

//----------------------------

   bool _AddNode(ImpNode *node, const TCHAR *name, int type, Mesh *msh, const char *owner, int mtlNum = -1){

      pair<t_node_list::iterator, bool> p = node_list.insert(pair<ImpNode*, S_node_entry>(node, S_node_entry()));
      S_node_entry *ptr = &(*p.first).second;
      ptr->id = -1;
      ptr->type = type;
      ptr->name = name;
      ptr->_mesh = msh;
      ptr->mnum = mtlNum;
      //ptr->posList = NULL;
      //ptr->rotList = ptr->scList = ptr->colList = NULL;
      //ptr->hotList = ptr->fallList = ptr->fovList = ptr->rollList = NULL;
      //ptr->next_node = nodes;
      _workNode = ptr;
      _thisNode = node;
      return true;
   }

//----------------------------

   bool AddNode(ImpNode *node, int type, int node_id){

      pair<t_node_list::iterator, bool> p = node_list.insert(pair<ImpNode*, S_node_entry>(node, S_node_entry()));
      S_node_entry *ptr = &(*p.first).second;
      ptr->id = node_id;
      ptr->type = type;
      ptr->_mesh = NULL;
      ptr->mnum = -1;
      return true;
   }

//----------------------------

   bool _SetNodeId(ImpNode *node, short id){

      t_node_list::iterator it=node_list.find(node);
      if(it!=node_list.end()){
         (*it).second.id = id;
         return true;
      }
      return false;   
   }

//----------------------------

   bool _CompleteScene(){

      _FinishUp();

      if(_length_set){
         Interval cur = imp_interface->GetAnimRange();
         Interval nrange = Interval(0, _anim_length * GetTicksPerFrame());
         if(!(cur==nrange))
            imp_interface->SetAnimRange(nrange);
      }
      /*
      S_node_entry *nptr = _nodes;
      while(nptr){
         if(nptr->type==OBJ_TARGET && nptr->node){
            S_node_entry *nptr2 = nodes;
            while(nptr2){
               if((nptr2->type==OBJ_CAMERA || nptr2->type==OBJ_SPOTLIGHT) && nptr->owner==nptr2->name && nptr2->node){
                  i->BindToTarget(nptr2->node,nptr->node);
                  goto next_target;       
               }
               nptr2 = nptr2->next_node;
            }
         }
      next_target:
         nptr = nptr->next_node;
      }
      nptr = nodes;
      */

      for(t_node_list::iterator it=node_list.begin(); it!=node_list.end(); it++){
         ImpNode *node = (*it).first;
         S_node_entry *nptr = &(*it).second;

         if(node){
            INode *inode = node->GetINode();

            switch(nptr->type){
            case OBJ_MESH:
               if(nptr->type == OBJ_MESH){
                  Object *obj = inode->EvalWorldState(0).obj;
                  TriObject *tri = (TriObject*)obj->ConvertToType(0, triObjectClassID);
                  Mesh &mesh = tri->mesh;
                  _AssignMaterials(inode, &mesh);
               }else{
                  inode->SetMtl(loaded_materials[_FindMatFromNode(node)]);
               }
                                 //flow...
            case OBJ_BONE:
                                 //Create interface inode
               if(nptr->type==OBJ_BONE){
                  inode->ShowBone(true);
                  inode->SetWireColor(0x6EFFEB);
               }
                                 //flow...
            case OBJ_DUMMY:
               {
                  Control *control = inode->GetTMController();
                  _MakeControlsTCB(control, nptr->_track_flags);
                  if(control){
                     SuspendAnimate();
                     AnimateOn();
                     if(nptr->pos_list.size()){
                        Control *posControl = control->GetPositionController();
                        if(posControl)
                           SetPositionKeys(posControl, nptr->pos_list);
                     }
                     if(nptr->rot_list.size()){
                        Control *rotControl = control->GetRotationController();
                        if(rotControl)
                           SetRotationKeys(rotControl, nptr->rot_list);
                     }
                     if(nptr->scl_list.size()){
                        Control *scControl = control->GetScaleController();
                        if(scControl)
                           SetScaleKeys(scControl, nptr->scl_list);
                     }
                     ResumeAnimate();
                  }
               }
               break;

            case OBJ_OMNILIGHT:
               {
                  GenLight *lt = (GenLight*)_FindObjFromNode(node);
                  if(!lt){
                     assert(0);
                     break;
                  }
                  SuspendAnimate();
                  AnimateOn();
                  Control *control = inode->GetTMController();
                  _MakeControlsTCB(control, nptr->_track_flags);
                  if(control){
                     /*
                     if(nptr->posList){
                        Control *posControl = control->GetPositionController();
                        if(posControl)
                           SetPositionKeys(posControl, nptr->posList);
                     }
                     */
                  }
                  if(nptr->col_list.size()){
                     Control *cont = (Control*)_interface->CreateInstance(CTRL_POINT3_CLASS_ID, Class_ID(TCBINTERP_POINT3_CLASS_ID,0));
                     lt->SetColorControl(cont);
                     SetColorKeys(cont, nptr->col_list);
                  }
                  lt->Enable(TRUE);
                  ResumeAnimate();
               }
               break;

            case OBJ_SPOTLIGHT:
               {
                  GenLight *lt = (GenLight *)_FindObjFromNode(node);
                  if(!lt){
                     assert(0);
                     break;
                  }
                  SuspendAnimate();
                  AnimateOn();
                  Control *control = inode->GetTMController();
                  _MakeControlsTCB(control, nptr->_track_flags);
                  if(control){
                     if(nptr->pos_list.size()){
                        Control *posControl = control->GetPositionController();
                        if(posControl) 
                           SetPositionKeys(posControl, nptr->pos_list);
                     }
                  }
                  if(nptr->col_list.size()){
                     Control *cont = (Control*)_interface->CreateInstance(CTRL_POINT3_CLASS_ID, Class_ID(TCBINTERP_POINT3_CLASS_ID,0));
                     lt->SetColorControl(cont);
                     SetColorKeys(cont, nptr->col_list);
                  }
                  /*
                  float aspect = lt->GetAspect(0);
                  if(nptr->hotList){
                     Control *cont = (Control*)ip->CreateInstance(CTRL_FLOAT_CLASS_ID,
                        Class_ID(TCBINTERP_FLOAT_CLASS_ID, 0));
                     lt->SetHotSpotControl(cont);
                     _SetControllerKeys(cont, nptr->hotList, KEY_FLOAT, 1.0f, aspect);
                  }
                  if(nptr->fallList){
                     Control *cont = (Control*)ip->CreateInstance(CTRL_FLOAT_CLASS_ID,
                        Class_ID(TCBINTERP_FLOAT_CLASS_ID,0));
                     lt->SetFalloffControl(cont);
                     _SetControllerKeys(cont, nptr->fallList, KEY_FLOAT, 1.0f, aspect);
                  }
                  control = inode->GetTMController();
                  if(control){
                     if(nptr->rollList){
                        Control *rollControl = control->GetRollController();
                        if(rollControl)
                           _SetControllerKeys(rollControl, nptr->rollList, KEY_FLOAT, -DEG_TO_RAD);
                     }
                  }
                  */
                  lt->Enable(true);
                  ResumeAnimate();
               }
               break;

            case OBJ_CAMERA:
               {
                  GenCamera *cam = (GenCamera *)_FindObjFromNode(node);
                  if(!cam){
                     assert(0);
                     break;
                  }
                  Control *control = inode->GetTMController();
                  _MakeControlsTCB(control, nptr->_track_flags);
                  if(control){
                     SuspendAnimate();
                     AnimateOn();
                     if(nptr->pos_list.size()){
                        Control *posControl = control->GetPositionController();
                        if(posControl)
                           SetPositionKeys(posControl, nptr->pos_list);
                     }
                     if(nptr->rot_list.size()){
                        Control *rotControl = control->GetRotationController();
                        if(rotControl)
                           SetRotationKeys(rotControl, nptr->rot_list);
                     }
                     if(nptr->scl_list.size()){
                        Control *scControl = control->GetScaleController();
                        if(scControl)
                           SetScaleKeys(scControl, nptr->scl_list);
                     }
                  }
                  /*
                  if(nptr->fovList){
                     Control *cont = (Control*)ip->CreateInstance(
                        CTRL_FLOAT_CLASS_ID,
                        Class_ID(TCBINTERP_FLOAT_CLASS_ID,0));
                     cam->SetFOVControl(cont);
                     _SetControllerKeys(cont, nptr->fovList, KEY_FLOAT,DEG_TO_RAD);
                  }
                  control = inode->GetTMController();
                  if(control){
                     if(nptr->rollList) {
                        Control *rollControl = control->GetRollController();
                        if(rollControl) {
                           _SetControllerKeys(rollControl, nptr->rollList, KEY_FLOAT,-DEG_TO_RAD);
                           }
                        }
                  }
                  */
                  cam->Enable(true);
                  ResumeAnimate();
               }
               break;

            case OBJ_OTHER:
               inode->SetMtl(_GetMaxMtl(nptr->mnum));
                                 //flow...

            case OBJ_TARGET:
               {
                                 //Unload all key info into the Jaguar node
                  Control *control = inode->GetTMController();
                  _MakeControlsTCB(control, nptr->_track_flags);
                  if(control) {
                     SuspendAnimate();
                     AnimateOn();
                     if(nptr->pos_list.size()){
                        Control *posControl = control->GetPositionController();
                        if(posControl)
                           SetPositionKeys(posControl, nptr->pos_list);
                     }
                     ResumeAnimate();
                  }
               }
               break;

            default:
               assert(0);
               break;
            }
            _ParseIKData(inode);
         }
      }
      /*
      t_obj_map::const_iterator it;
      for(it=_obj_map.begin(); it!=_obj_map.end(); it++){
         const _C_work_object &wobj = (*it).second;

         if(!wobj.used){
            ImpNode *node1, *node2;
            node1 = _MakeANode((*it).first, FALSE, "");

            switch(wobj.type){
            case OBJ_MESH:
               {
                  INode *inode = node1->GetINode();
                  Mesh *mesh = &(((TriObject *)inode->GetObjectRef())->GetMesh());
                  _AssignMaterials(inode, mesh);
               }
               break;

            case OBJ_SPOTLIGHT:
            case OBJ_CAMERA:
               {
                  Matrix3 tm(1);
            
                  if(wobj.type==OBJ_SPOTLIGHT){
                     GenLight *lt = (GenLight *)_FindObjFromNode(node1);
                     lt->Enable(TRUE);
                  }else{
                     GenCamera *cam = (GenCamera *)_FindObjFromNode(node1);
                     cam->Enable(TRUE);
                  }

                                 //create a target
                  C_fstring name("%s.Target", (const char*)((*it).first));
                  node2 = _MakeANode(name, TRUE, "");                       
                  tm.SetTrans((Point3&)wobj.targPos);
                  node2->SetTransform(0, tm);
                  i->BindToTarget(node1,node2);

                                 //set the position of the light or camera.
                  tm.SetTrans((Point3&)wobj.srcPos);
                  node1->SetTransform(0, tm);
               }
               break;

            case OBJ_OMNILIGHT:
               {
                  GenLight *lt = (GenLight *)_FindObjFromNode(node1);
                  lt->Enable(TRUE);
               }
               break;
            }
         }
      }
      */
      return true;
   }

//----------------------------

   int SetupEnvironment(){

      imp_interface->SetAmbient(0, Color(.5f, .5f, .5f));
      imp_interface->SetBackGround(0, Color(.5f, .5f, .5f));
      imp_interface->SetUseMap(false);
      /*
      i->SetAmbient(0, (Color&)_background.amb_light);
      i->SetBackGround(0, (Color&)_background.bkgd_solid);
      i->SetUseMap(false);

      switch(_background.bgType){
      case BG_SOLID: 
         break;

      case BG_GRADIENT: 
         {
            GradTex *gt = NewDefaultGradTex();
            gt->SetColor(0, (Color&)_background.bkgd_gradient.topcolor);
            gt->SetColor(1, (Color&)_background.bkgd_gradient.midcolor);
            gt->SetColor(2, (Color&)_background.bkgd_gradient.botcolor);
            gt->SetMidPoint(1.0f-_background.bkgd_gradient.midpct);
            gt->GetUVGen()->SetCoordMapping(UVMAP_SCREEN_ENV);
            i->SetEnvironmentMap(gt);
            i->SetUseMap(TRUE);
         }
         break;
      case BG_BITMAP: 
         if(strlen(_background.bkgd_map)>0){
            BitmapTex *bmt = NewDefaultBitmapTex();
            bmt->SetMapName(TSTR(_background.bkgd_map));
            bmt->GetUVGen()->SetCoordMapping(UVMAP_SCREEN_ENV);
            i->SetEnvironmentMap(bmt);
            i->SetUseMap(TRUE);
         }
         break;
      }
      */

      /*
      switch(_background.envType){
      case ENV_DISTCUE:
         {
         StdFog *fog = NewDefaultStdFog();
         fog->SetType(0);  
         fog->SetColor(Color(0.0f, 0.0f, 0.0f), 0);
         fog->SetNear(_background.distance_cue.neardim/100.0f, 0);  
         fog->SetFar(_background.distance_cue.fardim/100.0f, 0);  
         fog->SetFogBackground(_background.dim_bg);
         i->AddAtmosphere(fog);        
         }           
         break;
      case ENV_FOG:{
         StdFog *fog = NewDefaultStdFog();
         fog->SetType(0);  
         fog->SetColor((Color&)_background.fog_data.color, 0);
         fog->SetNear(_background.fog_data.neardens/100.0f,0);  
         fog->SetFar(_background.fog_data.fardens/100.0f,0);  
         fog->SetFogBackground(_background.fog_bg);
         i->AddAtmosphere(fog);        
         }
         break;
      case ENV_LAYFOG:{
         StdFog *fog = NewDefaultStdFog();
         fog->SetType(1);  
         int ftype;
         switch(_background.lfog_data.type) {
            case 1:  ftype = FALLOFF_BOTTOM; break;
            case 2:  ftype = FALLOFF_TOP; break;
            default: ftype = FALLOFF_NONE; break;
         }
         fog->SetFalloffType(ftype);  
         fog->SetColor((Color&)_background.lfog_data.color, 0);
         fog->SetDensity(_background.lfog_data.density*100.0f, 0);  
         fog->SetTop(_background.lfog_data.zmax,0);  
         fog->SetBottom(_background.lfog_data.zmin,0);  
         fog->SetFogBackground(_background.lfog_data.fog_bg);
         i->AddAtmosphere(fog);        
         }
         break;
      }
      i->SetAmbient(0, (Color&)_background.amb_light);
      */
      return 1;
   }

//----------------------------

   int _FindMatFromNode(ImpNode *node){

      t_node_list::iterator it=node_list.find(node);
      if(it!=node_list.end())
         return (*it).second.mnum;
      return NULL;
   }

//----------------------------

   ImpNode *FindNodeFromId(short id){

      for(t_node_list::iterator it=node_list.begin(); it!=node_list.end(); it++){
         S_node_entry &ne = (*it).second;
         if(ne.id==id)
            return (*it).first;
      }
      return NULL;
   }

//----------------------------

   int _FindTypeFromNode(ImpNode *node){

      t_node_list::iterator it=node_list.find(node);
      if(it!=node_list.end()){
         return (*it).second.type;
      }
      return -1;
   }

//----------------------------

   ImpNode *_MakeDummy(){


      if(!_dummy){
         //_dummy = new DummyObject();
         _dummy = (DummyObject*)imp_interface->Create(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0));
         _dummy->SetBox(Box3(
            -Point3(0.5f,0.5f,0.5f),
             Point3(0.5f,0.5f,0.5f)));
      }
      ImpNode *node = imp_interface->CreateNode();
      if(!node || !_dummy){
         return NULL;
      }
      node->Reference(_dummy);
      _tm.IdentityMatrix();    // Reset initial matrix to identity
      node->SetTransform(0, _tm);
      imp_interface->AddNodeToScene(node);
      //node->SetName(name);
      _AddNode(node, _name, OBJ_DUMMY, _mesh, "");
      return node;
   }

//----------------------------

   ImpNode *_MakeANode(const C_str &name, bool target, const char *owner){

      int type, cstShad = 0, rcvShad = 0, mtlNum = 0;
      Object *obj;
                              //it MUST have an object unless it's a target!
      if(!target){
         t_obj_map::iterator it = _obj_map.find(name);
         if(it==_obj_map.end())
            return NULL;
         _C_work_object *wobj = &(*it).second;
         type = wobj->type;
         cstShad = wobj->cstShad;
         rcvShad = wobj->rcvShad;
         mtlNum = wobj->mtln;
         wobj->used = true;
         obj = wobj->object;
      }else{
         type = OBJ_TARGET;
         obj = imp_interface->CreateTargetObject();
      }
      if(type==OBJ_MESH)
         _mesh = &(((TriObject *)obj)->GetMesh());
      else
         _mesh = NULL;
      ImpNode *node = imp_interface->CreateNode();
      if(!node)
         return NULL;
      node->Reference((ObjectHandle)obj);
      _tm.IdentityMatrix();    // Reset initial matrix to identity
      node->SetTransform(0, _tm);
      node->GetINode()->SetCastShadows(cstShad);
      node->GetINode()->SetRcvShadows(rcvShad);
      imp_interface->AddNodeToScene(node);
      node->SetName(name);
      _AddNode(node, name, type, _mesh, owner, mtlNum);
      return node;
   }

//----------------------------

   void _SetInstanceName(ImpNode *node, const TCHAR *iname){

      TSTR instancename(iname);
      if(!_nodename.Length() && instancename.Length())
         node->SetName(iname);
      else
      if(!instancename.Length() && _nodename.Length())
         node->SetName(_nodename);
      else{
         node->SetName(iname);
      }
   }

//----------------------------

   inline void _SetAnimLength(TimeValue l){
      _anim_length = l;
      _length_set = true;
   }

//----------------------------

   //inline void SetSegment(Interval seg){ segment = seg; segmentSet = true; }

//----------------------------

   void SetPositionKeys(Control *cont, const vector<S_I3D_position_key> &keys){

      ITCBPoint3Key k;

      int count = keys.size();

                              //special case where there is only one key at frame 0.
                              // just set the controller's value
      if(count==1 && keys.front().header.time==0){
         const S_I3D_position_key &key = keys.front();
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

         const S_I3D_position_key &key = keys[i];
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
      
         k.val.angle   = key.data.angle; 
         k.val.axis[0] = key.data.axis[0];
         k.val.axis[1] = key.data.axis[1];
         k.val.axis[2] = key.data.axis[2];
      
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

   void MakeControlsTCB(Control *control, dword inherit = INHERIT_ALL){

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

   void _MakeControlsTCB(Control *tmCont, word *tflags){

      dword flags = INHERIT_ALL;
                              //setup inheritance flags.
      if(tflags[POS_TRACK_INDEX] & NO_LNK_X) flags &= ~INHERIT_POS_X;
      if(tflags[POS_TRACK_INDEX] & NO_LNK_Y) flags &= ~INHERIT_POS_Y;
      if(tflags[POS_TRACK_INDEX] & NO_LNK_Z) flags &= ~INHERIT_POS_Z;

      if(tflags[ROT_TRACK_INDEX] & NO_LNK_X) flags &= ~INHERIT_ROT_X;
      if(tflags[ROT_TRACK_INDEX] & NO_LNK_Y) flags &= ~INHERIT_ROT_Y;
      if(tflags[ROT_TRACK_INDEX] & NO_LNK_Z) flags &= ~INHERIT_ROT_Z;

      if(tflags[SCL_TRACK_INDEX] & NO_LNK_X) flags &= ~INHERIT_SCL_X;
      if(tflags[SCL_TRACK_INDEX] & NO_LNK_Y) flags &= ~INHERIT_SCL_Y;
      if(tflags[SCL_TRACK_INDEX] & NO_LNK_Z) flags &= ~INHERIT_SCL_Z;

      MakeControlsTCB(tmCont, flags);
   }

//----------------------------

   Mtl *_GetMaxMtl(int i){

      if(i<0)
         return NULL;
      if(i>=loaded_materials.Count())
         return NULL;
      return loaded_materials[i];
   }

//----------------------------

   void _AssignMaterials(INode *inode, Mesh *mesh){

      short used[MAX_NUM_OF_MATERIALS],
         cused[MAX_NUM_OF_MATERIALS],
         remap[MAX_NUM_OF_MATERIALS];

                              //first check if another instance of this mesh has already
                              // had a material assigned to it
      for(int i=_mat_meshes.size(); i--; ){
         if(find(_mat_meshes[i].begin(), _mat_meshes[i].end(), mesh) != _mat_meshes[i].end()){
            inode->SetMtl(loaded_materials[i]);
            return;
         }
      }
      for(i=0; i<MAX_NUM_OF_MATERIALS; i++)
         used[i] = 0;
                              //see if a multi-mtl is required
      int nmtl, numMtls = 0;
      for(i =0; i<mesh->numFaces; i++){
         nmtl = mesh->faces[i].getMatID();
         assert(nmtl<MAX_NUM_OF_MATERIALS);
         if(!used[nmtl]){
            used[nmtl] = 1;
            remap[nmtl] = numMtls;
            cused[numMtls++] = nmtl;
         }
      }
      if(numMtls>1){
                              //need a Multi-mtl
                              // scrunch the numbers down to be local to this multi-mtl
         for(i =0; i<mesh->numFaces; i++){
            Face &f = mesh->faces[i];
            int id = f.getMatID();
            f.setMatID(remap[id]);
         }
                              //create a new multi with numMtls, and set them
         MultiMtl *newmat = NewDefaultMultiMtl();
         newmat->SetNumSubMtls(numMtls);
         for(i=0; i<numMtls; i++) 
            newmat->SetSubMtl(i,_GetMaxMtl(cused[i]));
         inode->SetMtl(newmat);
         loaded_materials.AddMtl(newmat);         
         _mat_meshes.push_back(vector<Mesh*>());
         _mat_meshes.back().push_back(mesh);
      }else{
         if(mesh->getNumFaces()){
            nmtl = mesh->faces[0].getMatID();
            for(i =0; i<mesh->numFaces; i++) 
               mesh->faces[i].setMatID(0);
            inode->SetMtl(_GetMaxMtl(nmtl));
            _mat_meshes[nmtl].push_back(mesh);
         }
      }
   }

//----------------------------
// Convert mesh mtl to max standard matl.
   void AddMaterial(S_material *smtl){

      StdMat *m;
      if(smtl==NULL){
         m = _New3DSDefaultMtl();
         loaded_materials.AddMtl(m);        
         _mat_meshes.push_back(vector<Mesh*>());
         return;
      }
      m = NewDefaultStdMat();
      m->SetName((const char*)smtl->name);
      m->SetShading(SHADE_PHONG);
      m->SetAmbient((Color&)ColorFrom24(smtl->amb), 0);
      m->SetDiffuse((Color&)ColorFrom24(smtl->diff), 0);
      m->SetFilter((Color&)ColorFrom24(smtl->diff), 0);
      m->SetSpecular((Color&)ColorFrom24(smtl->spec), 0);
      //m->SetShininess((float)smtl->shininess/100.0f,0);
      m->SetOpacity(1.0f - smtl->transparency, 0);
      m->SetFalloffOut(smtl->flags&MF_XPFALLIN?0:1);  
      m->SetSelfIllum((float)smtl->selfipct/100.0f,0);
      m->SetFaceMap(smtl->flags&MF_FACEMAP?1:0);
      m->SetSoften(smtl->flags&MF_PHONGSOFT?1:0);
      m->SetWire(smtl->flags&MF_WIRE?1:0);
      m->SetTwoSided(smtl->flags&MF_TWOSIDE?1:0);
      m->SetTransparencyType(smtl->flags&MF_ADDITIVE ? TRANSP_ADDITIVE : TRANSP_FILTER);
      m->SetWireUnits(smtl->flags&MF_WIREABS?1:0);


      //if(smtl->map)
      {
         bool gotTex = false;
         for(int i=0; i<MAP_LAST; i++){
            //if(smtl->map[i]==NULL) 
            if(!smtl->maps[i].use)
               continue;
            Texmap *txm;
            float amt, amt0 = 0;

            //S_map_data &mp = *(smtl->map[i]);
            S_map_data &mp = smtl->maps[i];
            int n = MAXMapIndex(i);
            /*
            if(i==MAP_REFLECTION){
               amt = (float)mp.percentage / 100.0f;
               S_Rmap_params &par = mp.map.p.ref;
               if(par.acb.flags&AC_ON){
                              //mirror or Auto-cubic
                  if(par.acb.flags&AC_MIRROR){
                     StdMirror *mir = NewDefaultStdMirror();
                     txm = (Texmap *)mir;
                     mir->SetDoNth(par.acb.flags&AC_FIRSTONLY?0:1);
                     mir->SetNth(par.acb.nth);
                  }else{
                     StdCubic *cub = NewDefaultStdCubic();
                     txm = (Texmap *)cub;
                     cub->SetSize(par.acb.size,0);
                     cub->SetDoNth(par.acb.flags&AC_FIRSTONLY?0:1);
                     cub->SetNth(par.acb.nth);
                  }
               }else{   
                              //environment map
                  int dum;
                  txm = MakeTex(mp.map, smtl, dum);
               }
               if(strlen(mp.mask.map_name)>0){
                              //make a Mask texmap.
                  Texmap *masktex = (Texmap *)CreateInstance(TEXMAP_CLASS_ID, Class_ID(MASK_CLASS_ID,0));
                  int dum;
                  masktex->SetSubTexmap(1,MakeTex(mp.mask, smtl, dum));
                  masktex->SetSubTexmap(0,txm);
                  txm = masktex;
               }

               m->SetSubTexmap(n, txm);
                  amt = (float)mp.percentage / 100.0f;
               m->SetTexmapAmt(n, amt, 0);
            }else
            */
            {
            
                              //non-reflection maps
               amt = (float)mp.percentage / 100.0f;

               int wasSXP;
               txm = MakeTex(mp, smtl, wasSXP);
               if(n==ID_BU&&!wasSXP) amt *= 10.0f;
               m->SetTexmapAmt(n, amt, 0);
               /*
               if(strlen(mp.mask.map_name)>0){
                              //make a Mask texmap.
                  Texmap *masktex = (Texmap *)CreateInstance(TEXMAP_CLASS_ID, Class_ID(MASK_CLASS_ID,0));
                  int dum;
                  masktex->SetSubTexmap(1,MakeTex(mp.mask, smtl, dum));
                  masktex->SetSubTexmap(0,txm);
                  txm = masktex;
               }
               */
               m->SetSubTexmap(n, txm);
               if(i==MAP_DIFFUSE && txm){
                  gotTex = true;
                  amt0 = amt;
               }
            }
            m->EnableMap(n, true);
            if(!i)
               //m->SetActiveTexmap(txm);
               _interface->ActivateTexture(txm, m);
         }
      }
      loaded_materials.AddMtl(m);
      _mat_meshes.push_back(vector<Mesh*>());
   }

//----------------------------

/*
   bool LoadAppData(){

      appdata = new byte[Size()];
      if(!appdata)
         return false;
      appdataLen = Size();
      C_chunk::Read(appdata, appdataLen);
      return true;
   }
   */

//----------------------------

   bool GetChunk(word tag, void *data);

//----------------------------
// Get chunk and Descend.
   int GetS_map_dataChunk(int tag, S_map_data *md){

      switch(tag){
      case CT_MAP_NAME:
         md->bitmap_name = C_chunk::ReadString();
         C_chunk::Descend();
         break;
         /*
      case MAT_MAP_TILING: 
         return(get_mtlchunk(ck, tag, &md->p.tex.flags));

      case MAT_MAP_TILINGOLD: 
         if(!get_mtlchunk(ck, tag, &md->p.tex.flags)){
            return(0);
         }
         if(md->p.tex.flags&TEX_DECAL)
            md->p.tex.flags|=MAP_NOWRAP;           
         return(1);
      case MAT_MAP_TEXBLUR: 
         return(get_mtlchunk(ck, tag, &md->p.tex.texture_blur));
      case MAT_MAP_USCALE: 
         return(get_mtlchunk(ck, tag, &md->p.tex.uscale));
      case MAT_MAP_VSCALE: 
         return(get_mtlchunk(ck, tag, &md->p.tex.vscale));
      case MAT_MAP_UOFFSET: 
         return(get_mtlchunk(ck, tag, &md->p.tex.uoffset));
      case MAT_MAP_VOFFSET: 
         return(get_mtlchunk(ck, tag, &md->p.tex.voffset));
      case MAT_MAP_ANG: 
         return(get_mtlchunk(ck, tag, &md->p.tex));
      case MAT_MAP_COL1: 
         return(get_mtlchunk(ck, tag, &md->p.tex.col1));
      case MAT_MAP_COL2: 
         return(get_mtlchunk(ck, tag, &md->p.tex.col2));
      case MAT_MAP_RCOL: 
         return(get_mtlchunk(ck, tag, &md->p.tex.rcol));
      case MAT_MAP_GCOL: 
         return(get_mtlchunk(ck, tag, &md->p.tex.gcol));
      case MAT_MAP_BCOL: 
         return(get_mtlchunk(ck, tag, &md->p.tex.bcol));
         */
      default:
         C_chunk::Descend();
      }
      return true;
   }

//----------------------------
// Read map info and descend chunk.
   bool GetMap(S_material &loadmtl, int n){

      //S_map_data *m = loadmtl.map[n];
      S_map_data *m = &loadmtl.maps[n];
      m->use = true;
      /*
      if(m==NULL){
         m = new S_map_data;
         if(!m)
            return false;
         loadmtl.map[n] = m;
      }
      */

      word tag;
      while(C_chunk::Size()){
         tag = C_chunk::RAscend();
         switch(tag){
         case CT_PERCENTAGE:
            {
               m->percentage = C_chunk::RWordChunk();
               if(n==MAP_BUMP) 
                  m->percentage = (float)m->percentage * .1f;
            }
            break;
            /*
         case FLOAT_PERCENTAGE:
            assert(0);
            C_chunk::Descend();
            break;
            */
         default:
            if(!GetS_map_dataChunk(tag, m))
               return false;
         }
      }
      C_chunk::Descend();
      return true;
   }

//----------------------------

   /*
   bool GetMask(S_material &loadmtl, int n){

      S_map_data *m = loadmtl.map[n];
      if(m==NULL){
         m = new S_map_data(n);
         if(!m)
            return false;
         loadmtl.map[n] = m;
      }
      m->use = true;
      word tag;
      while(C_chunk::Size()){
         if(!C_chunk::RAscend(tag))
            return false;
         //switch(tag){
         //default:
            assert(0);
            C_chunk::Descend();
         //}
         if(!GetS_map_dataChunk(tag, &m->mask))
            return 0;
      }
      C_chunk::Descend();
      return true;
   }
   */

//----------------------------
// Get material chunk and Descend.
   bool GetMaterialChunk(S_material &loadmtl){

      while(C_chunk::Size()){
         CK_TYPE tag = C_chunk::RAscend();
         switch(tag){
         case CT_MAT_NAME:
            loadmtl.name = C_chunk::ReadString();
            C_chunk::Descend();
            break;     

                              //compatibility!
         case MAT_AMBIENT:
         case MAT_DIFFUSE:
         case MAT_SPECULAR:
            {
               void *data;
               switch(tag){
               case MAT_AMBIENT: data = &loadmtl.amb; break;
               case MAT_DIFFUSE: data = &loadmtl.diff; break;
               case MAT_SPECULAR: data = &loadmtl.spec; break;
               default: assert(0); data = NULL;
               }
               //bool got_lin = false;
               bool got_gam = false;
               S_rgb color;
               color.Zero();
               while(C_chunk::Size()){
                  tag = C_chunk::RAscend();
                  switch(tag){
                  case CT_COLOR_F:
                     assert(0);
                     C_chunk::Descend();
                     break;
                  case CT_COLOR_24:
                     {
                        got_gam = true;
                        //if(get_mtlchunk(ck, tag, NULL)==0) return(0);
                        C_chunk::Read(&color, 3);
                        C_chunk::Descend();
                        swap(color.r, color.b);
                     }
                     break;     
                     /*
                  case LIN_COLOR_24:
                     //got_lin = true;
                     //if(get_mtlchunk(ck, tag, NULL)==0) return(0);
                     assert(0);
                     break;     
                     */
                  default:
                     assert(0);
                     C_chunk::Descend();
                  }
               }
               /*
               if(got_lin){
                  memcpy((char*)data,(char *)&LC24,sizeof(Color_24));
               }else{
                  */
                  if(!got_gam)
                     return false;
                  if(gammaMgr.enable){
                     S_rgb gc;
                     gc.r = gammaMgr.file_in_degamtab[color.r]>>8;
                     gc.g = gammaMgr.file_in_degamtab[color.g]>>8;
                     gc.b = gammaMgr.file_in_degamtab[color.b]>>8;
                     memcpy(data, &gc, sizeof(S_rgb));
                  }else{
                     memcpy(data, &color, sizeof(S_rgb));
                  }
               //}
               C_chunk::Descend();
            }
            break;

         case CT_MAT_AMBIENT:
         case CT_MAT_DIFFUSE:
         case CT_MAT_SPECULAR:
            {
               void *data;
               switch(tag){
               case CT_MAT_AMBIENT: data = &loadmtl.amb; break;
               case CT_MAT_DIFFUSE: data = &loadmtl.diff; break;
               case CT_MAT_SPECULAR: data = &loadmtl.spec; break;
               default: assert(0); data = NULL;
               }
               C_chunk::Read(data, 3);
               C_chunk::Descend();
            }
            break;
            /*
         case MAT_ACUBIC:
            {
               S_map_data *m = loadmtl.map[MAP_REFLECTION];
               if(m==NULL){
                  ck->Descend();
                  break;
               }
               AutoCubicParams *ac = (AutoCubicParams *)data;
               ck->Read(&ac->shade, 1);
               ck->Read(&ac->aalevel, 1);
               ck->Read(&ac->flags, 2);
               ck->Read(&ac->size, 4);
               ck->Read(&ac->nth, 4);
               ck->Descend();
            }
            break;     

         case MAT_SXP_TEXT_DATA:       
         case MAT_SXP_TEXT2_DATA: 
         case MAT_SXP_OPAC_DATA:  
         case MAT_SXP_BUMP_DATA:  
         case MAT_SXP_SPEC_DATA:  
         case MAT_SXP_SELFI_DATA: 
         case MAT_SXP_SHIN_DATA:  
      
         case MAT_SXP_TEXT_MASKDATA:  
         case MAT_SXP_TEXT2_MASKDATA: 
         case MAT_SXP_OPAC_MASKDATA:  
         case MAT_SXP_BUMP_MASKDATA:  
         case MAT_SXP_SPEC_MASKDATA:  
         case MAT_SXP_SELFI_MASKDATA: 
         case MAT_SXP_SHIN_MASKDATA:  
         case MAT_SXP_REFL_MASKDATA:  
            if(!get_mtlchunk(ck, tag, NULL))
               return false;
            break;
            */
      
         case CT_MAT_TEXMAP:
            if(!GetMap(loadmtl, MAP_DIFFUSE))
               return false;
            break;

         case CT_MAT_OPACMAP:
            if(!GetMap(loadmtl, MAP_OPACITY))
               return false;
            break;

         case CT_MAT_BUMPMAP:
            if(!GetMap(loadmtl, MAP_BUMP))
               return false;
            break;

         case CT_MAT_SPECMAP:
            if(!GetMap(loadmtl, MAP_SPECULAR))
               return false;
            break;

         case CT_MAT_SHINMAP:
            if(!GetMap(loadmtl, MAP_SHININESS))
               return false;
            break;

         case CT_MAT_SELFIMAP:
            if(!GetMap(loadmtl, MAP_SELF_ILLUM))
               return false;
            break;
         case CT_MAT_REFLMAP:
            if(!GetMap(loadmtl, MAP_REFLECTION))
               return false;
            break;

            /*
         case CT_MAT_TEXMASK:
            if(!GetMask(loadmtl, MAP_DIFFUSE))
               return false;
            break;     

         case CT_MAT_OPACMASK:
            if(!GetMask(loadmtl, MAP_OPACITY))
               return false;
            break;     
         case CT_MAT_BUMPMASK:
            if(!GetMask(loadmtl, MAP_BUMP))
               return false;
            break;     
         case CT_MAT_SPECMASK:
            if(!GetMask(loadmtl, MAP_SPECULAR))
               return false;
            break;     
         case CT_MAT_SHINMASK:
            if(!GetMask(loadmtl, MAP_SHININESS))
               return false;
            break;     
         case CT_MAT_SELFIMASK:
            if(!GetMask(loadmtl, MAP_SELF_ILLUM))
               return false;
            break;     
         case CT_MAT_REFLMASK:
            if(!GetMask(loadmtl, MAP_REFLECTION))
               return false;
            break;     
            */

         case CT_MAT_TWO_SIDE:
            loadmtl.flags |= MF_TWOSIDE;
            C_chunk::Descend();
            break;

         //case CT_MAT_SHININESS:
         //case CT_MAT_SHIN2PCT:
                              //compatibility!
         case MAT_TRANSPARENCY:
            {
               short s;
               if(!_GetIntFloatChunk(s))
                  return false;
               loadmtl.transparency = (float)s * .01f;
            }
            break;
         case MAT_SELF_ILPCT:
            if(!_GetIntFloatChunk(loadmtl.selfipct))
               return false;
            break;

         case CT_MAT_TRANSPARENCY:
            loadmtl.transparency = C_chunk::RByteChunk();
            break;

         case CT_MAT_SELF_ILPCT:
            loadmtl.selfipct = C_chunk::RByteChunk();
            break;

         case CT_MAT_SELF_ILLUM:
            loadmtl.flags |= MF_SELF;   
            loadmtl.selfipct = 100;            
            C_chunk::Descend();
            break;
            /*
         case CT_MAT_XPFALL:
         case CT_MAT_REFBLUR:
         case CT_MAT_SHADING:
         
         case CT_MAT_SUPERSMP:
         case CT_MAT_WIRE:
         case CT_MAT_FACEMAP:
         case CT_MAT_XPFALLIN:
         case CT_MAT_PHONGSOFT:
         case CT_MAT_WIREABS:
         case CT_MAT_USE_XPFALL:
         case CT_MAT_USE_REFBLUR:
            if(!get_mtlchunk(ck, tag, NULL))
               return false;
            break;

         case CT_MAT_WIRESIZE:
            ck->Read(&loadmtl.wiresize, 4);
            ck->Descend();
            break;

            if(!get_mtlchunk(ck, tag, &loadmtl.appdata))
               return(0);
            break;
            */
         //case APP_DATA:

         default:
            C_chunk::Descend();
         }
      }
      /*
                           //convert old data formats to new
      if(loadmtl.shading==REND_WIRE){
         loadmtl.shading = REND_FLAT;
         loadmtl.flags |= MF_WIRE;
         loadmtl.flags |= MF_TWOSIDE;
         loadmtl.shininess=0;
         loadmtl.shin2pct=0;
         loadmtl.transparency=0;
      }

      if(loadmtl.xpfall<0.0f){
         loadmtl.flags|= MF_XPFALLIN;
         loadmtl.xpfall = -loadmtl.xpfall;
      }
      if(loadmtl.flags&MF_DECAL){
         set_mtl_decal(loadmtl);
         loadmtl.flags &= ~MF_DECAL;
      }
      if(loadmtl.shin2pct==255){
         float shin = (((float)(loadmtl.shininess))/100.0f);
         float atten = (float)sin(1.5707*shin);
         loadmtl.shin2pct = (int)((atten)*100.0f+0.5f);
      }
      */
      C_chunk::Descend();
      return true;
   }
};

//----------------------------
// Get a null-terminated string from the file.
static bool _read_string(C_chunk *ck, char *string, int maxsize){

   while(maxsize--){
      ck->Read(string, 1);
      if(*(string++)==0)
         return true;
   }
   return false;
}

//----------------------------

static bool _tchar_read_string(C_chunk *ck, TSTR &tstr){

   int maxsize = 4096;
   char string;
   char buf[2];

   while(maxsize--){
      ck->Read(&string, 1);
      sprintf(buf, "%c", string);
      tstr += TSTR(buf);
      
      if(string == 0)
         return true;
   }
   return false;
}

//----------------------------

bool ObjWorker::GetChunk(word tag, void *data){

   switch(tag){
   case KFDATA:
      while(C_chunk::Size()){
         tag = RAscend();
         if(!GetChunk(tag, NULL))
            return false;
         /*
         switch(tag){
         case OBJECT_NODE_TAG:
         case CAMERA_NODE_TAG: 
         case TARGET_NODE_TAG:   
         case L_TARGET_NODE_TAG:    
         case LIGHT_NODE_TAG:
         case SPOTLIGHT_NODE_TAG:
         case DIRECTLIGHT_NODE_TAG:
         case BONE_NODE_TAG:
         //case KFHDR:
         case KFSEG:
            if(!GetChunk(tag, NULL))
               return false;
            break;
         default:
            Descend();
         }
         */
      }
      break;

      /*
   case KFHDR:
      {
         char dumstr[10];
         LONG length;
         C_chunk::Read(&readVers, sizeof(short));
         if(!_read_string(this, dumstr, 9))
            return false;
         C_chunk::Read(&length, sizeof(dword));
         SetAnimLength((TimeValue)length);
      }
      break;
      */

   case CT_KF_SEGMENT:
      {
         long segStart, segEnd;
         C_chunk::Read(&segStart, sizeof(dword));
         C_chunk::Read(&segEnd, sizeof(dword));

         _SetAnimLength((TimeValue)segEnd);
         //SetSegment(Interval((TimeValue)segStart, (TimeValue)segEnd));

         Interval cur = imp_interface->GetAnimRange();
         Interval nrange = Interval(0, segEnd * GetTicksPerFrame());
         if(!(cur==nrange))
            imp_interface->SetAnimRange(nrange);
      }
      break;

                              //compatibility:
   case CAMERA_NODE_TAG:
   case LIGHT_NODE_TAG:
   case SPOTLIGHT_NODE_TAG:
   case DIRECTLIGHT_NODE_TAG:
      break;

   case OBJECT_NODE_TAG:
   case BONE_NODE_TAG:
      {
         ++node_load_id;
         _FinishUp();
         _cur_node_id = -32000;
         while(C_chunk::Size()){
            tag = C_chunk::RAscend();
            switch(tag){
            case NODE_HDR:
            case NODE_ID:
            //case APP_DATA:
            case CT_BOUNDING_BOX:
            case CT_PIVOT_OFFSET:
            case INSTANCE_NAME:
            case CT_TRACK_POS: 
            case CT_TRACK_ROT:
            case CT_TRACK_SCL:
            case ROLL_TRACK_TAG: 
            case FOV_TRACK_TAG: 
            case CT_TRACK_VISIBILITY:
            case CT_TRACK_NOTE:
            case CT_PROPERTIES:
            case CT_TRACK_COLOR: 
            case HOT_TRACK_TAG: 
            case FALL_TRACK_TAG: 
               if(!GetChunk(tag, NULL))
                  return false;
               break;
            default:
               Descend();
            }
         }
      }
      break;

   case TARGET_NODE_TAG: 
   case L_TARGET_NODE_TAG: 
      {
         node_load_id++;
         _FinishUp();
         _cur_node_id = -32000;   
         while(C_chunk::Size()){
            tag = C_chunk::RAscend();
            switch(tag){
            case NODE_ID:
            case NODE_HDR:
               if(!GetChunk(tag, ".target"))
                  return false;
               break;
            //case APP_DATA:
            case CT_TRACK_POS: 
            case CT_TRACK_VISIBILITY:
            case CT_TRACK_NOTE:
               if(!GetChunk(tag, NULL))
                  return false;
               break;
            default:
               Descend();
            }
         }
      }
      break;

   case NODE_ID:
      {
         C_chunk::Read(&_cur_node_id, sizeof(short));
      }
      break;

   case NODE_HDR:
      {
         word npar;
         ImpNode *pnode;
                              //if we have a suffix, this is a target object
         char *suffix = (char*)data;
                  
         if(!_read_string(this, _obname, OBJ_NAME_LEN))
            return false;
         _obname[OBJ_NAME_LEN] = 0;
         char prefix[OBJ_NAME_LEN];
         if(suffix){
            strcpy(prefix, _obname);           
            strcat(_obname, suffix);
         }else
            prefix[0] = 0;

         C_str Wname = _obname;

         // Check for dummies -- If it is a dummy, we create a node now because
         // none was created for it in the mesh section
         if(strcmp(_obname,"DUMMY")==0 || strcmp(_obname,"$$$DUMMY")==0){
            _obnode = _MakeDummy();
            if(!_obnode)
               return false;
            _nodename = "";
         }else{
            _obnode = _MakeANode(_obname, suffix ? true : false, prefix);
            if(!_obnode)
               return false;
            _nodename = (const char*)Wname;
         }

         _StartKF(_obnode);   // Start creating a KF node
                     
         if(_cur_node_id!=-32000)
            _SetNodeId(_obnode, _cur_node_id);
         else 
            _SetNodeId(_obnode, node_load_id);

         short nodeflags, nodeflags2;
         C_chunk::Read(&nodeflags, sizeof(short));

         //if(readVers<4) nodeflags &= 7;
                              //read second flag word
         C_chunk::Read(&nodeflags2, sizeof(short));
                              //get parent node if any
         C_chunk::Read(&npar, sizeof(short));
         
         pnode = NULL;
         if(npar != 0xffff)
            pnode = FindNodeFromId(npar);
         _parentNode = pnode;
      }
      break;

   case CT_PIVOT_OFFSET:
      {
         float p[3];
         C_chunk::Read(&p, sizeof(S_vector));
         _pivot = Point3(p[0], p[1], p[2]);
      }
      break;

   case CT_BOUNDING_BOX:
      {
         Point3 min,max;
         C_chunk::Read(&min, sizeof(S_vector));
         C_chunk::Read(&max, sizeof(S_vector));
         if(_dummy)
            _dummy->SetBox(Box3(min, max));
      }
      break;

   case INSTANCE_NAME:
      {
         char iname[MAX_NAME_SIZE];
         if(!_read_string(this, iname, MAX_NAME_SIZE))
            return false;

         iname[MAX_NAME_SIZE - 1] = 0;
         _SetInstanceName(_obnode, TSTR(iname));
      }
      break;

   case CT_PROPERTIES:
      {
         TSTR buffer;
         if(!_tchar_read_string(this, buffer))
            return false;
         _thisNode->GetINode()->SetUserPropBuffer(buffer);
      }
      break;

   case CT_TRACK_NOTE:
      {
         int ntracks=0;
         C_chunk::Read(&ntracks, sizeof(int));

         for(int i =0; i<ntracks;i++){
            DefNoteTrack *ntra = (DefNoteTrack*)NewDefaultNoteTrack();

            int nkeys;
            C_chunk::Read(&nkeys, sizeof(int));

            for(int j = 0; j<nkeys;j++){
               TimeValue t;
               C_chunk::Read(&t, sizeof(TimeValue));
               TSTR buffer;

               if(!_tchar_read_string(this, buffer)) 
                  return false;

               ntra->AddNewKey(t*GetTicksPerFrame(), 0);
               ntra->keys[j]->note = buffer;
            }
            _thisNode->GetINode()->AddNoteTrack(ntra);
         }
      }
      break;

#define BOOL_CONTROL_CLASS_ID Class_ID(0x984b8d27,0x938f3e43)
   case CT_TRACK_VISIBILITY:
      {
         SuspendAnimate();
         AnimateOn();

         Control *cont = (Control *)CreateInstance(CTRL_FLOAT_CLASS_ID, BOOL_CONTROL_CLASS_ID);

         int numkeys;
         C_chunk::Read(&numkeys, sizeof(int));
         TimeValue t;
         for(int i =0; i<numkeys;i++){
            C_chunk::Read(&t, sizeof(TimeValue));
            cont->AddNewKey(t*GetTicksPerFrame(), 0);
         }
         _thisNode->GetINode()->SetVisController(cont);
         ResumeAnimate();
      }
      break;
      
   case CT_TRACK_POS:
   case CT_TRACK_ROT:
   case CT_TRACK_SCL:
   case FOV_TRACK_TAG:
   case ROLL_TRACK_TAG:
   case CT_TRACK_COLOR:
   case HOT_TRACK_TAG:
   case FALL_TRACK_TAG:
      {
         S_I3D_track_header header;
         C_chunk::Read(&header, sizeof(header));

         for(int ix=0; ix<header.num_keys; ix++){
            S_I3D_key_header key_header;
            S_I3D_key_tcb key_data;
            C_chunk::Read(&key_header, sizeof(key_header));

            key_data.Read(C_chunk::GetHandle(), key_header.flags);

            switch(tag){
            case CT_TRACK_POS:
               {
                  S_I3D_position_key key;
                  key.header = key_header;
                  key.add_data = key_data;
                  C_chunk::Read(&key.data, sizeof(key.data));

                  _workNode->_track_flags[POS_TRACK_INDEX] = header.track_flags;
                  _workNode->pos_list.push_back(key);
               }
               break;

            case CT_TRACK_SCL:
               {
                  S_I3D_scale_key key;
                  key.header = key_header;
                  key.add_data = key_data;
                  C_chunk::Read(&key.data, sizeof(key.data));

                  _workNode->_track_flags[SCL_TRACK_INDEX] = header.track_flags;
                  _workNode->scl_list.push_back(key);
               }
               break;

            case CT_TRACK_COLOR:
               {
                  S_I3D_color_key key;
                  key.header = key_header;
                  key.add_data = key_data;
                  C_chunk::Read(&key.data, sizeof(key.data));

                  _workNode->_track_flags[COL_TRACK_INDEX] = header.track_flags;                
                  _workNode->col_list.push_back(key);
               }
               break;

            case CT_TRACK_ROT:
               {
                  S_I3D_rotation_key key;
                  key.header = key_header;
                  key.add_data = key_data;
                  C_chunk::Read(&key.data, sizeof(key.data));

                  _workNode->_track_flags[ROT_TRACK_INDEX] = header.track_flags;
                  _workNode->rot_list.push_back(key);
               }
               break;
               /*
            case HOT_TRACK_TAG:
               {
                  ScalarKey sckey;
                  memset(&sckey, 0, sizeof(ScalarKey));
                  memcpy(&sckey, &key_hdr, sizeof(S_key_hdr));
                  RDFLOAT(&sckey.e[0].p);
                  _workNode->_track_flags[HOT_TRACK_INDEX] = header.track_flags;
                  
                  AddHotKey(&sckey);
               }
               break;

            case FALL_TRACK_TAG:
               {
                  ScalarKey sckey;
                  memset(&sckey,0,sizeof(ScalarKey));
                  memcpy(&sckey, &key_hdr, sizeof(S_key_hdr));
                  RDFLOAT(&sckey.e[0].p);
                  _workNode->_track_flags[FALL_TRACK_INDEX] = header.track_flags;
                  
                  AddFallKey(&sckey);
               }
               break;

            case FOV_TRACK_TAG:
               {
                  ScalarKey sckey;
                  memset(&sckey, 0, sizeof(ScalarKey));
                  memcpy(&sckey, &key_hdr, sizeof(S_key_hdr));
                  RDFLOAT(&sckey.e[0].p);
                  _workNode->_track_flags[FOV_TRACK_INDEX] = header.track_flags;
                  
                  AddFOVKey(&sckey);
               }
               break;

            case ROLL_TRACK_TAG:
               {
                  ScalarKey sckey;
                  memset(&sckey, 0, sizeof(ScalarKey));
                  memcpy(&sckey, &key_hdr, sizeof(S_key_hdr));
                  RDFLOAT(&sckey.e[0].p);
                  _workNode->_track_flags[ROLL_TRACK_INDEX] = header.track_flags;
                  
                  AddRollKey(&sckey);
               }
               break;
               */
            }
         }
      }
      break;

   case CT_BASE:
   case M3DMAGIC:
      //gotM3DMAGIC = true;
                              //flow...
   case MMAGIC:
      while(C_chunk::Size()){
         tag = C_chunk::RAscend();
         switch(tag){
            /*
         case MMAGIC:
         case CT_MESH:
         case KFDATA:
         */
         default:
            if(!GetChunk(tag, NULL))
               return false;
            break;

            /*
         case MASTER_SCALE:
            if(!GetChunk(tag, NULL))
               return false;
            break;
            */
         case CT_MATERIAL:
            {
               S_material mat;
               //mat.Init();
               if(!GetMaterialChunk(mat))
                  return false;
               AddMaterial(&mat);
               //mat.FreeRefs();
            }
            break;     
            /*
         default:
            Descend();
            */
         }
      }
      break;

   //case LIN_COLOR_F:
   case CT_COLOR_F:
      {
         S_vector *cf = (S_vector*)data;
         C_chunk::Read(&cf, sizeof(S_vector));
      }
      break;

   case CT_COLOR_24:
      {
         S_rgb c24;
         S_vector *cf = (S_vector*)data;
         C_chunk::Read(&c24, 3);

         cf->x = (float)c24.r/255.0f;
         cf->y = (float)c24.g/255.0f;
         cf->z = (float)c24.b/255.0f;
      }
      break;

      /*
   case MASTER_SCALE:
      {
         C_chunk::Read(&msc_wk, 4);
         int type;
         float scale;
         GetMasterUnitInfo(&type, &scale);
         float msc_factor = (float)GetMasterScale(UNITS_INCHES);
         msc_wk = (float)((double)msc_wk / msc_factor);     //Turn into a mult factor
      }
      break;
      */

   case DL_OUTER_RANGE:
   case DL_INNER_RANGE:
      {
         float *fptr = (float *)data;
         C_chunk::Read(fptr, sizeof(float));
         //*fptr *= msc_wk;
      }
      break;

   case DL_MULTIPLIER:
      {
         Dirlight *d=(Dirlight *)data;
         C_chunk::Read(&d->mult, sizeof(float));
      }
      break;

   case CT_MESH:
      {
         if(!_read_string(this, _obname, OBJ_NAME_LEN+1))
            return false;
         while(C_chunk::Size()){
            tag = C_chunk::RAscend();
            switch(tag){
            case N_TRI_OBJECT:
               if(!_StartMesh(_obname))
                  return false;
               if(!GetChunk(tag, this))
                  return false;
               break;
               
            case N_BONE:
               {
                  if(!_StartBone(_obname))
                     return false;
                  if(GetChunk(tag, NULL)==0)
                     return false;
                  if(!_CreateBone())
                     return false;
               }
               break;

            case N_DIRECT_LIGHT_TARGET:
               {
                  Dirlight &d = _studioLt;
                  d.x=d.y=d.z=d.tx=d.ty=d.tz=0.0f;
                  d.flags=NO_LT_ON;
                  d.color.x = d.color.y = d.color.z = 1.0f;
                  d.hotsize = d.fallsize=360.0f;
                  d.lo_bias=3.0f;
                  d.shadsize=256;   
                  d.in_range=10.0f;
                  d.out_range=100.0f;
                  d.shadfilter=5.0f;   
                  strcpy(d.imgfile,"");
                  d.ray_bias = 0.2f;
                  d.bank=0.0f;
                  d.aspect=1.0f;
                  d.mult=1.0f;
                  d.appdata = NULL;
                  if(!_StartLight(_obname))
                     return false;
                  if(GetChunk(tag, &d)==0)
                      return false;
                  int type = TDIR_LIGHT ;
                  if(!_CreateLight(type))
                     return false;
               }
               break;

            case N_DIRECT_LIGHT:
               {
                  Dirlight &d = _studioLt;
                                 //init the direct light to an omni
                  d.x = d.y = d.z = d.tx = d.ty = d.tz = 0.0f;
                  d.flags = NO_LT_ON;
                  d.color.x = d.color.y = d.color.z = 1.0f;
                  d.hotsize = d.fallsize=360.0f;
                  d.lo_bias = 3.0f;   // TO DO: USE GLOBAL?
                  d.shadsize = 256;   // TO DO: USE GLOBAL?
                  d.in_range = 10.0f;
                  d.out_range = 100.0f;
                  d.shadfilter = 5.0f;   // TO DO: USE GLOBAL?
                  strcpy(d.imgfile, "");
                  d.ray_bias = 0.2f;   // TO DO: USE GLOBAL?
                  d.bank = 0.0f;
                  d.aspect = 1.0f;
                  d.mult = 1.0f;
                  d.appdata = NULL;
                  if(!_StartLight(_obname))
                     return false;
                  if(GetChunk(tag, &d)==0)
                     return false;
                  int type = d.hotsize==360.0f ? OMNI_LIGHT : TSPOT_LIGHT;
                  if(!_CreateLight(type))
                     return false;
               }
               break;

            case N_CAMERA:
               {
                  Camera3DS &c = _studioCam;
                  // Init the camera struct
                  c.x = c.y = c.z = c.tx = c.ty = c.tz = c.bank = 0.0f;
                  c.focal = 50.0f;
                  c.flags = 0;
                  c.nearplane = 10.0f;
                  c.farplane = 1000.0f;
                  c.appdata = NULL;
                  if(!_StartCamera(_obname))
                     return false;
                  if(GetChunk(tag, &c)==0)
                     return false;
                  if(!_CreateCamera(TARGETED_CAMERA))
                     return false;
               }
               break;

            default:
               Descend();
            }
         }
      }
      break;
            
      /*
   case APP_DATA:       
      LoadAppData();
      break;
      */

   case N_TRI_OBJECT:
      while(C_chunk::Size()){
         tag = C_chunk::RAscend();
         switch(tag){
         case CT_POINT_ARRAY:
         case CT_FACE_ARRAY:
         case MESH_MATRIX:
         case TEX_VERTS:
         case CT_FACE_MAP_CHANNEL:
            if(!GetChunk(tag, data))
               return false;
            break;

            /*
         case APP_DATA:
            if(GetChunk(tag, data)==0)
               return false;
            break;
            */

         case CT_MESH_COLOR:
            if(GetChunk(tag, data)==0)
               return false;
            break;

         default:
            Descend();
         }
      }
      break;

   case CT_POINT_ARRAY:
      {
         word wk_count;
         C_chunk::Read(&wk_count, sizeof(word));
         if(!_SetVerts((int)wk_count))
            return false;
         S_vector v;
         for(int ix=0; ix<wk_count; ++ix){
            C_chunk::Read(&v, sizeof(S_vector));
            _mesh->setVert(ix, v.x, v.y, v.z);
         }
      }
      break;

   case TEX_VERTS:
      {
         word wk_count;
         C_chunk::Read(&wk_count, sizeof(word));
         if(!_SetTVerts((int)wk_count))
            return false;
         UVVert tv;
         tv.z = 0.0f;
         for(int ix=0; ix<wk_count; ++ix){
            C_chunk::Read(&tv, sizeof(float)*2);

            if (tv.x>1.0e10) tv.x = 0.0f;
            if (tv.x<-1.0e10) tv.x = 0.0f;
            if (tv.y>1.0e10) tv.y = 0.0f;
            if (tv.y<-1.0e10) tv.y = 0.0f;
            if(!_PutTVertex(ix,&tv))
               return false;
         }
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
            for(int j=0; j<3; j++){
               uvfaces[i].t[j] = 0;
               C_chunk::Read(&uvfaces[i].t[j], sizeof(word));
            }
         }
         if(channel_i>=2 && _mesh->getNumMaps() < (channel_i+1))
            _mesh->setNumMaps(channel_i+1, true);
         _mesh->setMapSupport(channel_i, true);
         _mesh->setNumMapVerts(channel_i, num_uv_verts, false);
         for(i=0; i<num_uv_verts; i++)
            _mesh->setMapVert(channel_i, i, uvverts[i]);

         _mesh->setNumMapFaces(channel_i, num_faces, false);
         for(i=0; i<num_faces; i++)
            _mesh->mapFaces(channel_i)[i] = uvfaces[i];

         delete[] uvverts;
         delete[] uvfaces;
      }
      break;

   case POINT_FLAG_ARRAY:
      break;

   case CT_FACE_ARRAY:
      {
         word wk_count;
         C_chunk::Read(&wk_count, sizeof(word));
         if(!_SetFaces((int)wk_count))
            return false;

         struct fc_wrt{
            unsigned short a;
            unsigned short b;
            unsigned short c;
            unsigned short flags;
         } Fc_wrt;
         Faces f;

         for(int ix=0; ix<wk_count; ++ix){
            C_chunk::Read(&Fc_wrt, 8);

            f.a=Fc_wrt.a;
            f.b=Fc_wrt.b;
            f.c=Fc_wrt.c;
            f.material = 0;
            f.sm_group = 0;
            f.flags = Fc_wrt.flags;
            if(!_PutFace(ix,&f))
               return false;

            Face *fp = &_mesh->faces[ix];
                                    //if we've got texture vertices, also put out a texture face
            if(_gottverts)
               _SetTVerts(ix, &f);

            fp->setVerts(f.a,f.b,f.c);
            fp->setEdgeVisFlags(f.flags&ABLINE, f.flags&BCLINE, f.flags&CALINE);
            fp->setSmGroup(f.sm_group);
         }

         while(C_chunk::Size()){
            tag = C_chunk::RAscend();
            switch(tag){
            case MSH_MAT_GROUP:
            case CT_SMOOTH_GROUP:
               if(!GetChunk(tag, NULL))
                  return false;
               break;
            default:
               Descend();
            }
         }
      }
      break;

   case MESH_MATRIX:
      {
         Matrix3 tm(1);
         C_chunk::Read(tm.GetAddr(), sizeof(float) * 12);
         C_chunk::Read(tm.GetAddr(), sizeof(float) * 12);
         tm.SetNotIdent();
         _tm = tm;
      }
      break;

   case CT_MESH_COLOR:
      {
         int num_verts = _mesh->getNumVerts();
         int num_faces = _mesh->getNumFaces();

         UVVert *uvverts = new UVVert[num_verts];
         for(int i=0; i<num_verts; i++){
            S_rgb rgb;
            C_chunk::Read(&rgb, sizeof(S_rgb));
            S_vector &color = *(S_vector*)&uvverts[i];
            color.x = (float)rgb.r / 255.0f;
            color.y = (float)rgb.g / 255.0f;
            color.z = (float)rgb.b / 255.0f;
         }
         TVFace *c_faces = new TVFace[num_faces];
         for(i=num_faces; i--; ){
            for(int j=3; j--; ){
               c_faces[i].t[j] = _mesh->faces[i].v[j];
            }
         }

         _mesh->setNumMapVerts(0, num_verts, false);
         for(i=0; i<num_verts; i++)
            _mesh->setMapVert(0, i, uvverts[i]);

         _mesh->setNumMapFaces(0, num_faces, false);
         for(i=0; i<num_faces; i++)
            _mesh->mapFaces(0)[i] = c_faces[i];

         delete[] uvverts;
         delete[] c_faces;
      }
      break;

   case MSH_MAT_GROUP:
      {
         char mtlname[256];
         word wkface;
         if(!_read_string(this, mtlname, sizeof(mtlname)))
            return false;
         int mtlnum = Max(0, loaded_materials.FindMtlByName(CStr(mtlname)));
         
         word wk_count;
         C_chunk::Read(&wk_count, 2);
         for(int ix=0; ix<wk_count; ++ix){
            C_chunk::Read(&wkface,2);
            _mesh->faces[wkface].setMatID(mtlnum);
         }
      }
      break;

   case CT_SMOOTH_GROUP:
      {
         unsigned long smgroup;
         for(int ix=0; ix<_faces; ++ix){
            C_chunk::Read(&smgroup,sizeof(unsigned long));
            if(!_PutSmooth(ix, smgroup))
               return false;
         }
      }
      break;

   case N_BONE:
      {
         Matrix3 m;
         C_chunk::Read(&m, 48);
         _tm = m;
      }
      break;

   case N_DIRECT_LIGHT_TARGET:
      {
         Dirlight *d = (Dirlight *)data;
         C_chunk::Read(&d->x,sizeof(float));
         C_chunk::Read(&d->y,sizeof(float));
         C_chunk::Read(&d->z,sizeof(float));
         /*
         if(msc_wk != 1.0f){
            d->x *= msc_wk;
            d->y *= msc_wk;
            d->z *= msc_wk;
         }
         */
         while(C_chunk::Size()){
            tag = C_chunk::RAscend();
            switch(tag){
            case CT_COLOR_F:
            case CT_COLOR_24:
               if(GetChunk(tag, &d->color)==0)
                  return false;
               break;
            case DL_INNER_RANGE:
               if(GetChunk(tag, &d->in_range)==0)
                  return false;
               break;
            case DL_OUTER_RANGE:
               if(GetChunk(tag, &d->out_range)==0)
                  return false;
               break;
            case DL_SPOTLIGHT:
            case DL_OFF:
            case DL_ATTENUATE:
            case DL_MULTIPLIER:
               if(GetChunk(tag, d)==0)
                  return false;
               break;
               /*
            case APP_DATA:
               if(GetChunk(tag, &d->appdata)==0)
                  return false;
               break;
               */
            default:
               Descend();
            }
         }
      }
      break;

   case N_DIRECT_LIGHT:
      {
         Dirlight *d = (Dirlight *)data;
         C_chunk::Read(&d->x,sizeof(float));
         C_chunk::Read(&d->y,sizeof(float));
         C_chunk::Read(&d->z,sizeof(float));
         /*
         if(msc_wk != 1.0f){
            d->x *= msc_wk;
            d->y *= msc_wk;
            d->z *= msc_wk;
         }
         */
         while(C_chunk::Size()){
            tag = C_chunk::RAscend();
            switch(tag){
            case CT_COLOR_F:
            case CT_COLOR_24:
               if(GetChunk(tag, &d->color)==0)
                  return false;
               break;
            case DL_INNER_RANGE:
               if(GetChunk(tag, &d->in_range)==0)
                  return false;
               break;
            case DL_OUTER_RANGE:
               if(GetChunk(tag, &d->out_range)==0)
                  return false;
               break;
            case DL_SPOTLIGHT:
            case DL_OFF:
            case DL_ATTENUATE:
            case DL_MULTIPLIER:
               if(GetChunk(tag, d)==0)
                  return false;
               break;
               /*
            case APP_DATA:
               if(GetChunk(tag, &d->appdata)==0)
                  return false;
               break;
               */
            default:
               Descend();
            }
         }
      }
      break;

   case DL_DIRLIGHT:
   case DL_SPOTLIGHT:
      {
         Dirlight *d = (Dirlight *)data;
         C_chunk::Read(&d->tx, sizeof(float));
         C_chunk::Read(&d->ty, sizeof(float));
         C_chunk::Read(&d->tz, sizeof(float));
         /*
         if(msc_wk != 1.0f){
            d->tx *= msc_wk;
            d->ty *= msc_wk;
            d->tz *= msc_wk;
         }
         */
         C_chunk::Read(&d->hotsize, sizeof(float));
         C_chunk::Read(&d->fallsize, sizeof(float));

                              //enforce keep .5 degree gap for antialiasing
         if(d->hotsize>d->fallsize-.5&&d->fallsize>=.5f) 
            d->hotsize = d->fallsize-.5f;
      }
      break;

   case DL_OFF:
      {
         Dirlight *d = (Dirlight *)data;
         d->flags &= NO_LT_OFF;
      }
      break;

   case DL_ATTENUATE:
      {
         Dirlight *d=(Dirlight *)data;
         d->flags |= NO_LT_ATTEN;
      }
      break;

   case N_CAMERA: 
      {
         Camera3DS *c = (Camera3DS *)data;
         C_chunk::Read(c, 32);
         /*
         if(msc_wk != 1.0f){
            c->x *= msc_wk;
            c->y *= msc_wk;
            c->z *= msc_wk;
            c->tx *= msc_wk;
            c->ty *= msc_wk;
            c->tz *= msc_wk;
         }
         */
         while(C_chunk::Size()){
            tag = C_chunk::RAscend();
            switch(tag){
               /*
            case APP_DATA:
               if(GetChunk(tag, &c->appdata)==0)
                  return false;
               break;
               */
            case CAM_RANGES:
               if(GetChunk(tag, c)==0)
                  return false;
               break;
            default:
               Descend();
            }
         }
      }
      break;

   case CAM_RANGES:
      {
         Camera3DS *c = (Camera3DS *)data;
         C_chunk::Read(&c->nearplane,sizeof(float));
         C_chunk::Read(&c->farplane,sizeof(float));
         //c->nearplane *= msc_wk;
         //c->farplane *= msc_wk;
      }
      break;

   case M3D_VERSION:
   case MESHVERSION:
   case KFHDR:
   case KFCURTIME:
   case MASTER_SCALE:
      break;

   default:
      assert(0);
      return false;
   }
   Descend();
   return true;
}

//----------------------------

int ImportOld(const TCHAR *filename, ImpInterface *i, Interface *gi, BOOL suppressPrompts){


   ObjWorker ow(i, gi);
   if(!ow.ROpen(filename))
      return IMPEXP_FAIL;

   if(!i->NewScene())
      return IMPEXP_CANCEL;

   SetCursor(LoadCursor(NULL, IDC_WAIT));

   CK_TYPE tag = ow.RAscend();
   if(tag != M3DMAGIC)
      return IMPEXP_FAIL;

   ow.AddMaterial(NULL); // Create "Default" mtl

   if(!ow.GetChunk(tag, NULL))
      return IMPEXP_FAIL;

   if(!ow._CompleteScene())
      return IMPEXP_FAIL;

   SetCursor(LoadCursor(NULL, IDC_ARROW));

   if(!ow.SetupEnvironment())
      return IMPEXP_FAIL;

   return IMPEXP_SUCCESS;
}

//----------------------------
