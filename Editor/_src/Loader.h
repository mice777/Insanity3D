//----------------------------
                              //game binary file chunks

enum E_GAME_CHUNKTYPE{

   //CT_BASECHUNK   =0x4c53,    //mission data
      //CT_CAMERA   =0x3000,
         //CT_GC_MODE_DIST   =0x3103,
         //CT_GC_MODE_ANGLE,
         //CT_GC_MODE_DISP,


      CT_SCRIPT = 0x4100,
         //CT_NAME frame_name
         CT_SCRIPT_NAME    =0x4101,
            //null-terminated string
            // (if 1st character is '*', the name is relative to mission dir)
         CT_SCRIPT_TABLE   =0x4102,
            //saved C_table

//----------------------------
                              //FREE: 8000 - BFFF

      CT_ACTOR = 0x8000,
         //CT_NAME frame_name
         //word E_ACTOR_TYPE
         CT_ACTOR_DATA  =0x8010,
            //actor-specific mission data
         CT_ACTOR_TABLE = 0x8011,
            //saved C_table

      CT_CHECKPOINT     =0x8400,
         CT_CHKPT_POS   =0x8401,
            //S_vector
         CT_CHKPT_CONNECTION  =0x8402,
            CT_CP_CON_ID   =0x8403,
               //word
            CT_CP_CON_TYPE =0x8407,
               //byte E_CP_CONNECTION_TYPE
         CT_CHKPT_TYPE = 0x8405,
            //byte E_CHECKPOINT_TYPE
         CT_CHKPT_NAME = 0x8406,
            //string
      CT_NUM_CHECKPOINTS=0x8404,
         //int num_checkpoints (optimization hint - how many checkpoints we should expect, optional, not present in older scenes)

      CT_MISSION_TABLE = 0x8801,
         //C_table::Save
};

//----------------------------
