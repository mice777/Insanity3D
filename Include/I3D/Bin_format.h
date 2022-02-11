#ifndef __BIN_FORMAT_H
#define __BIN_FORMAT_H

//----------------------------
                              //Insanity editor binary file chunks
                              // used by Insanity system for saving into
                              // binary file, using C_chunk class (c_chunk.h)

enum E_INSANITY_EDITOR_CHUNK_TYPE{
                              //general sub-chunk types
   CT_VERSION  =0x0001,
      //word maj
      //word min
      //copyright info text
   CT_COPYRIGHT=0x0002,
      //text
   CT_NAME     =0x0010,
      //null-terminated string
   CT_POSITION =0x0020,
      //S_vector
   CT_DIRECTION=0x0021,
      //S_vector
   CT_QUATERNION  =0x0022,
      //S_quat
   CT_SCALE    =0x0023,
      //S_vector
   CT_NUSCALE  =CT_SCALE,
      //S_vector
   CT_FLOAT    =0x0024,
   CT_INT      =0x0025,
   CT_COLOR    =0x0026,
      //S_vector
   CT_USCALE   =0x002a,
      //float
   CT_PIVOT    =0x002b,
      //S_vector
   CT_WORLD_POSITION =0x002c,
      //S_vector

   CT_ROTATION =0x0030,
      //S_vector axis / float angle
   CT_THUMBNAIL =0x0040,
      //image data


                              //mission data
   CT_BASECHUNK   =0x4c53,

      CT_CAMERA_FOV     =0x3010,
         //float
      CT_CAMERA_RANGE   =0x3011,
         //float far_range
      CT_CAMERA_ORTHOGONAL    =0x3012,
      CT_CAMERA_ORTHO_SCALE   =0x3013,
      CT_CAMERA_EDIT_RANGE   =0x3014,
         //float far_range

      CT_SCENE_BGND_COLOR  =0x3200,
         //S_vector scene_background_color
      CT_SCENE_BACKDROP    =0x3211,
         //float near_range, far_range

      CT_MODIFICATIONS=0x4000,
         CT_MODIFICATION =0x4010,
            //CT_NAME
            //CT_POSITION
            //CT_QUATERNION
         CT_FRAME_TYPE   =0x4011,
         CT_FRAME_SUBTYPE   =0x4012,
            //dword I3D_FRAME_TYPE
         CT_FRAME_FLAGS = 0x4014,
            //dword frame_flags
         CT_LINK  =0x4020,
            //CT_NAME   (parent name)
         CT_HIDDEN      =0x4033,

         CT_MODIFY_MODELFILENAME  =0x2012,
            //string

         CT_MODIFY_LIGHT   =0x4040,       //light params, followed by params:
            CT_LIGHT_TYPE  =0x4041,
               //dword type
            //CT_COLOR color
            CT_LIGHT_POWER =0x4042,
               //float power
            CT_LIGHT_CONE  =0x4043,
               //float cone_in, cone_out
            CT_LIGHT_RANGE =0x4044,
               //float range_n, range_f
            CT_LIGHT_MODE  =0x4045,
               //dword mode
            CT_LIGHT_SECTOR=0x4046,
               //char[] sector_name
            CT_LIGHT_SPECULAR_COLOR =0x4047, //obsolete
            CT_LIGHT_SPECULAR_POWER =0x4048, //obsolete
         CT_MODIFY_SOUND   =0x4060,
            //CT_NAME file_name
            CT_SOUND_TYPE  =0x4061,
               //dword type
            CT_SOUND_VOLUME   =0x4062,
               //float volume
            CT_SOUND_OUTVOL   =0x4063,
               //float outside_volume
            CT_SOUND_CONE  =0x4064,
               //float cone_in, cone_out
            CT_SOUND_RANGE =0x4065,
               //float range_n, range_f, range_fade
            CT_SOUND_LOOP  =0x4066,
            CT_SOUND_ENABLE   =0x4067,
            CT_SOUND_SECTOR   =0x4068,
            CT_SOUND_STREAMING  =0x4069,
               //char[] sector_name
         CT_MODIFY_VOLUME  =0x4070,
            CT_VOLUME_TYPE =0x4071,
               //dword type
            CT_VOLUME_MATERIAL =0x4072,
               //dword material_id
         CT_MODIFY_OCCLUDER   =0x4080,
            CT_OCCLUDER_VERTICES =0x4082,
               //dword num_vertices
               //S_vector[num_vertices] vertex_pool
         CT_MODIFY_VISUAL     =0x4090,
            //dword visual_type
            CT_VISUAL_MATERIAL =0x4092,
               //dword material_id
         CT_MODIFY_LIT_OBJECT=0x40a0,
            //lit-object data
         CT_MODIFY_SECTOR  =0x40b0,
            CT_SECTOR_ENVIRONMENT =0x40b2,
               //word env_id
            CT_SECTOR_TEMPERATURE = 0x40b3,
               //float
         CT_MODIFY_VISUAL_PROPERTY  =0x40c0,
            //byte I3D_PROPERTYTYPE
            //word property_index
            //property data (variable length)
         CT_MODIFY_BRIGHTNESS = 0x40e0,
            //float brightness

      CT_POSE_ANIMS = 0x5000,
         CT_POSE_LAST_EDITED = 0x5001,
            //int anim_index
         CT_POSE_ANIM_HEADER = 0x5005,
                              //editor info
            CT_POSE_ANIM_NAME = 0x5006, 
               //string            
            CT_POSE_ANIM_ROOT_FRM = 0x5007,
               //string
            CT_POSE_ANIM_MASK_NAME = 0x5008,
               //string
            CT_POSE_ANIM_CUSTOM_MASK = 0x5009,
               //S_mask_mode::Save() raw data 
            CT_POSE_ANIM = 0x5010,
               CT_POSE_ANIM_FRAMES = 0x503a,
                  CT_POSE_ANIM_FRAME_LINK = 0x503b,
                  //string name
               CT_POSE_ANIM_KEYS = 0x5014,
                  CT_POSE_ANIM_KEY = 0x5016,
                     CT_POSE_KEY_TIME = 0x5018,
                        //dword time
                     CT_POSE_KEY_EASY_FROM = 0x501a,
                        //float
                     CT_POSE_KEY_EASY_TO = 0x501b,
                        //float
                     CT_POSE_KEY_SMOOTH = 0x501f,
                        //float
                     CT_POSE_KEY_NOTIFY = 0x501d,
                        //string note
                     CT_POSE_KEY_NOTIFY_LINK = 0x501e,
                        //string frame name
                     CT_POSE_KEY_ANIM_SET = 0x5021,
                        //raw anim set data:
                           //word num_anims
                              //word anim_id
                              //bool has_pos
                                 //S_vector pos
                              //bool has_rot
                                 //S_quat rot
                              //bool has_scale
                                 //scale
               CT_POSE_ANIM_WEIGHTS = 0x5040,
                  CT_POSE_ANIM_WEIGHT = 0x5041,
                     CT_POSE_ANIM_WEIGHT_LINK = 0x5042,
                        //string frame name
                     CT_POSE_ANIM_WEIGHT_VAL = 0x5043,
                        //float
            CT_POSE_ANIM_POS = 0x5050,
               //S_vector edit_pos (for last loaded anim)
            CT_POSE_ANIM_ROT = 0x5051,
               //S_quat edit_rot (for last loaded anim)

      CT_CAMERA_PATHS = 0x5100,
         CT_CAMPATH_LAST_EDITED = 0x5101,
            //int path_index
         CT_CAMPATH_CURR_EDIT_POS = 0x5102,
            //int time
         CT_CAMERA_PATH = 0x5110,
            CT_CAMPATH_NAME = 0x5120,
               //string
            CT_CAMPATH_KEYS = 0x5121,
               //dword num_keys
               CT_CAMPATH_KEY = 0x5122,
                  CT_CAMPATH_POS = 0x5123,
                     //S_vector
                  CT_CAMPATH_DIR = 0x5124,
                     //S_vector
                  CT_CAMPATH_UPVECTOR = 0x5125,
                     //S_vector
                  CT_CAMPATH_NOTIFY = 0x5126, //(old)
                     //string
                  CT_CAMPATH_SPEED = 0x5127,
                     //float
                  CT_CAMPATH_EASINESS = 0x5128,
                     //float
                              //obsolete 3:
                  CT_CAMPATH_SMOOTH = 0x5128,
                     //float
                  CT_CAMPATH_EASY_TO = 0x5129,
                     //float
                  CT_CAMPATH_EASY_FROM = 0x512a,
                     //float

                  CT_CAMPATH_PAUSE = 0x512b,
                     //dword
                  CT_CAMPATH_NOTE = 0x512c,
                     //float relative_pos
                     //string

      CT_PHYSICS_TEMPLATE = 5200,
         //S_phys_template data, managed by "PhysicsStudio" plugin

      CT_BSP_FILE = 0x6800,

      CT_EDITOR_PLUGIN = 0x7800,
         //CT_NAME plugin_name
         //plugin-specified data

//----------------------------
                              //FREE: 8000 - BFFF
                              // (H&D reserved: 8000 - 9FFF)
                              // (gangsters reserved: A000 - BFFF)

//----------------------------
                              //EDITOR: C000 - CFFF

   CT_EDITOR_STATE = 0xc000,  //editor plugins state
      CT_EDITOR_PLUGIN_STATE = 0xc010,
         //CT_NAME
         //plugin custom data
//----------------------------
                              //FREE: E000 - FFFF

};

//----------------------------

#endif
