#ifndef __SYSTABLES_H
#define __SYSTABLES_H


extern PC_table tab_materials;//global material table - used with volume's materials
extern PC_table tab_sound_envs;  //sound environment sets

                              //open and read all tables
bool TablesOpenAll();

                              //save and close all tables
void TablesCloseAll();

                              //apply tables
void ApplyMaterialTable();
void ApplySoundEnvTable();

//----------------------------

#define MAX_MATERIALS 64      //expandable

                              //material style - main identifier of what can be done with material,
                              // or frame containing volume with such material

                              //!!! if you modify these types, update also enum in the material table !!!
enum E_MAT_STYLE{
   MATSTYLE_DEFAULT,
   MATSTYLE_GLASS=1,            //glass-like, breakable
   MATSTYLE_STAIRS=2,
   MATSTYLE_BORDER=3,           //border for AI movement, ignored by eyes/shots/some actors
   MATSTYLE_CALLBACK=4,         //use actor/script callback on this or parent frame to determine behavior
   MATSTYLE_SWIM_SURFACE=5,     //detection of swimming
   MATSTYLE_WATER_SURFACE=6,    //makes sounds and circles
   MATSTYLE_WOUND=7,            //wounding organisms
   MATSTYLE_LADDER=9,           //ladder (climbable by human)
};

//----------------------------
                              //indicies into tab_materials
enum E_MAT_TABLE{
   TAB_S32_MAT_NAME = 0,      //user-friendly name of material
   TAB_E_MAT_CATEGORY = 1,    //general type, one of E_MAT_STYLE
   TAB_F_MAT_FRICTION = 2,    //physics friction, in normalized range 0 - 1
   TAB_F_MAT_DENSITY = 3,     //density of material (used for physics)
   //TAB_F_MAT_GLIDENESS = 5,   //how much this material glides, normal is 1.0
   TAB_I_MAT_SOUND_LIKE = 6,  //index of material, which this mat mostly sounds like (alibism, to save time in creating sounds)

   TAB_F_MAT_DEBUG_COLOR = 9,
   TAB_I_MAT_DEBUG_ALPHA = 10,

                              //TerrainDetail model sets
   TAB_S24_MAT_MODELSET_NAME = 16,  //name of set
   TAB_F_MAT_MODELSET_RATIO,  //ratio how often this model is selected
   TAB_B_MAT_MODELSET_ALIGN,  //align model by normal


                              //non-data members (count)
   TAB_MAT_NUM_MODELS_IN_SET = 32,
};

//----------------------------
//----------------------------

#endif//__SYSTABLES_H
