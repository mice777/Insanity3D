#include <C_cache.h>
#include <I3D\I3D_math.h>
//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// .i3d file format chunks
//----------------------------

enum E_CHUNK_TYPE{
   CT_COLOR_F = 0x0010,
      //S_vector color
   CT_COLOR_24 = 0x0011,
      //byte r, g, b
   CT_PERCENTAGE = 0x0030,
      //word percent
      

   CT_BASE = 'MD',
      CT_USER_COMMENTS = 0x1000,
         //string
      CT_NUM_MATERIALS = 0xaffe,
         //dword num_mats (informational chunk)
      CT_MATERIAL = 0xafff,
         CT_MAT_NAME = 0xa000,
         CT_MAT_AMBIENT = 0xa410,
         CT_MAT_DIFFUSE = 0xa420,
         CT_MAT_SPECULAR = 0xa430,

         CT_MAT_SHININESS = 0xA040,
         CT_MAT_SHIN2PCT= 0xA041,
         CT_MAT_SHIN3PCT= 0xA042,
         CT_MAT_TRANSPARENCY= 0xA450,
         CT_MAT_XPFALL= 0xA052,
         CT_MAT_REFBLUR= 0xA053,

         CT_MAT_SELF_ILLUM = 0xA080,
         CT_MAT_TWO_SIDE = 0xA081,
         CT_MAT_DECAL= 0xA082,
         CT_MAT_ADDITIVE = 0xA083,
         CT_MAT_SELF_ILPCT= 0xA484,
         CT_MAT_WIRE= 0xA085,
         CT_MAT_SUPERSMP= 0xA086,
         CT_MAT_WIRESIZE= 0xA087,
         CT_MAT_FACEMAP= 0xA088,
         CT_MAT_XPFALLIN= 0xA08A,
         CT_MAT_PHONGSOFT= 0xA08C,
         CT_MAT_WIREABS= 0xA08E,

         CT_MAT_SHADING = 0xA100,
         CT_MAP_DIFFUSE = 0xA200,
         CT_MAP_SPECULAR = 0xA204,
         CT_MAP_OPACITY = 0xA210,
         CT_MAP_REFLECTION= 0xA220,
         CT_MAP_DETAIL = 0xA021,
         CT_MAP_REFRACT = 0xA022,
         CT_MAP_DISPLACE = 0xA023,
         CT_MAP_AMBIENT = 0xA024,
         CT_MAP_SPEC_LEVEL = 0xA025,
         CT_MAP_BUMP = 0xA230,
         CT_MAT_USE_XPFALL = 0xA240,
         CT_MAT_USE_REFBLUR = 0xA250,
                              //map chunk
            CT_MAP_NAME = 0xA300,
            CT_MAP_UVOFFSET = 0xA301,
               //float uv_offset[2]
            CT_MAP_UVSCALE = 0xA302,
               //float uv_scale[2]
            CT_MAP_TRUECOLOR = 0xa303,
            CT_MAP_NOMIPMAP = 0xa304,
            CT_MAP_PROCEDURAL = 0xa305,
            CT_MAP_NOCOMPRESS = 0xa306,
            CT_MAP_ANIMATED = 0xa307,
               //int anim_speed (frame delay)

         CT_MAT_TEX2MAP= 0xA33A,
         CT_MAP_SHINNINESS = 0xA33C,
         CT_MAP_SELF = 0xA33D,

      CT_KF_SEGMENT = 0xB008,
         //int start, end
      CT_PROPERTIES = 0xB00B,
         //string
      CT_PIVOT_OFFSET = 0xB013,
         //S_vector

      CT_TRACK_POS = 0xb020,
         //S_I3D_track_header
         //S_I3D_position_key[]
      CT_TRACK_ROT = 0xb021,
         //S_I3D_track_header
         //S_I3D_rotation_key[]
      CT_TRACK_SCL = 0xb022,
         //S_I3D_track_header
         //S_I3D_scale_key[]
      CT_TRACK_COLOR = 0xB025,
         //S_I3D_track_header
         //S_I3D_color_track[]
      CT_TRACK_VISIBILITY = 0xb029,
         //S_I3D_track_header
         //S_I3D_visibility_key[]
      CT_TRACK_NOTE = 0xB02A,
         //S_I3D_track_header
         //S_I3D_note_track[]

      CT_NODE_POSITION = 0xB080,
         //S_vector
      CT_NODE_ROTATION = 0xB081,
         //S_vector axis
         //float angle
      CT_NODE_SCALE = 0xB082,
         //S_vector

      CT_NODE_MESH = 0xB100,
      CT_NODE_DUMMY = 0xB101,
      CT_NODE_CAMERA = 0xB102,
      CT_NODE_LIGHT = 0xB103,
      CT_NODE_BONE = 0xB104,
         CT_DISPLAY_LINK = 0xb105,
         CT_NODE_NAME = 0xb200,
            //string name
         CT_NODE_PARENT = 0xb210,
            //word parent_id
         CT_BOUNDING_BOX = 0xB014,

         CT_MESH = 0x4000,
            CT_POINT_ARRAY = 0x4110,
               //word count
               //S_vector[]
            CT_FACE_ARRAY = 0x4120,
               //word count
               //S_face{ I3D_triface, word flags} faces[]
            CT_EDGE_VISIBILITY = 0x4121,
               //byte flags[] - 3 lower bits for each face
            CT_MESH_FACE_GROUP = 0x4131,
               //word mat_index
               //word first_face
               //word num_faces
            CT_SMOOTH_GROUP = 0x4150,
               //dword fgroups[]

            CT_FACE_MAP_CHANNEL = 0x4200,
               //int channel
               //word num_uv_verts
               //I3D_text_coor[]
               //word num_uv_faces
            CT_MESH_COLOR = 0x4165,

            CT_SKIN = 0x4170,
               CT_SKIN_JOINT = 0x4171,
                  //CT_NAME joint name
                  CT_SKIN_JOINT_ENDPOINTS = 0x4172,
                     //S_vector[2] start/end
                  CT_SKIN_JOINT_INIT_TM = 0x4173,
                     //float[4][3] init matrix of joint when applied to skin
                  CT_SKIN_JOINT_FLAGS = 0x4174,
                     //dword
                  CT_SKIN_JOINT_VERTS = 0x4175,
                     //word num
                     //word[num] indices
                     //float[num] weights
                  CT_SKIN_INIT_TM = 0x4176,
                     //float[4][3] init matrix of base object when joint applied to it

         CT_JOINT_WIDTH = 0x4200,
            //float
         CT_JOINT_LENGTH = 0x4201,
            //float
         CT_JOINT_ROT_OFFSET = 0x4202,
            //S_quat
};

//----------------------------

enum I3D_KEY_FLAGS{
   I3DKF_TENSION = 1,
   I3DKF_CONTINUITY = 2,
   I3DKF_BIAS = 4,
   I3DKF_EASY_TO = 8,
   I3DKF_EASY_FROM = 0x10,
};

#pragma pack(push,1)

struct S_I3D_track_header{
   word track_flags;
   dword res[2];
   dword num_keys;
};

struct S_I3D_key_header{
   int time;                  //time of key
   word flags;                //combination of I3D_KEY_FLAGS
};

struct S_I3D_key_tcb{
   float tension;             //-1.0 to 1.0
   float continuity;          //-1.0 to 1.0
   float bias;                //-1.0 to 1.0
   float easy_from;           //0.0 to 1.0
   float easy_to;             //0.0 to 1.0

   void Clear(){
      memset(this, 0, sizeof(S_I3D_key_tcb));
   }
   void Read(C_cache *is, dword flags){
      Clear();
      if(flags&I3DKF_TENSION) is->read((char*)&tension, sizeof(float));
      if(flags&I3DKF_CONTINUITY) is->read((char*)&continuity, sizeof(float));
      if(flags&I3DKF_BIAS) is->read((char*)&bias, sizeof(float));
      if(flags&I3DKF_EASY_TO) is->read((char*)&easy_to, sizeof(float));
      if(flags&I3DKF_EASY_FROM) is->read((char*)&easy_from, sizeof(float));
   }
   void Write(C_cache *is, dword flags){
      if(flags&I3DKF_TENSION) is->write((char*)&tension, sizeof(float));
      if(flags&I3DKF_CONTINUITY) is->write((char*)&continuity, sizeof(float));
      if(flags&I3DKF_BIAS) is->write((char*)&bias, sizeof(float));
      if(flags&I3DKF_EASY_TO) is->write((char*)&easy_to, sizeof(float));
      if(flags&I3DKF_EASY_FROM) is->write((char*)&easy_from, sizeof(float));
   }
   dword GetFlags(){
      dword f = 0;
      if(tension)    f |= I3DKF_TENSION;
      if(continuity) f |= I3DKF_CONTINUITY;
      if(bias)       f |= I3DKF_BIAS;
      if(easy_to)    f |= I3DKF_EASY_TO;
      if(easy_from)  f |= I3DKF_EASY_FROM;
      return f;
   }
};

struct S_I3D_position_key{
   S_I3D_key_header header;
   S_I3D_key_tcb add_data;
   S_vector data;

   void Read(C_cache *is){
      is->read((char*)&header, sizeof(header));
      add_data.Read(is, header.flags);
      is->read((char*)&data, sizeof(data));
   }
   void Write(C_cache *is){
      header.flags = word(add_data.GetFlags());
      is->write((char*)&header, sizeof(header));
      add_data.Write(is, header.flags);
      is->write((char*)&data, sizeof(data));
   }
};

struct S_I3D_rotation_key{
   S_I3D_key_header header;
   S_I3D_key_tcb add_data;
   struct S_data{
      float angle;
      S_vector axis;
   } data;

   void Read(C_cache *is){
      is->read((char*)&header, sizeof(header));
      add_data.Read(is, header.flags);
      is->read((char*)&data, sizeof(data));
   }
   void Write(C_cache *is){
      header.flags = word(add_data.GetFlags());
      is->write((char*)&header, sizeof(header));
      add_data.Write(is, header.flags);
      is->write((char*)&data, sizeof(data));
   }
};

typedef S_I3D_position_key S_I3D_scale_key;
typedef S_I3D_position_key S_I3D_color_key;

struct S_I3D_visibility_key{
   int time;                  //time of key
   float visibility;
};

#pragma pack(pop)

//----------------------------

