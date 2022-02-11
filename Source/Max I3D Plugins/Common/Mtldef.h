#ifndef __MTLDEF__H
#define __MTLDEF__H

#include <rules.h>
#include <stdmat.h>

//----------------------------
                              //material flags
#define MF_TWOSIDE   1        //Material seen from both sides
#define MF_SELF      2        //Material self-illuminated
#define MF_DECAL     4        //Material maps act as decals (transparent color)
#define MF_ADDITIVE  8        //Material uses additive transparency
#define MF_WIRE      0x10     //Material renders as wire frame
#define MF_NEEDUV    0x20     //Material has some UV type maps
#define MF_NEED3D    0x40     //Material needs 3D coords for SXP
#define MF_XPFALLIN  0x80     //Transparency fall-off to inside
#define MF_MATTE     0x100    //Material used as a matte
#define MF_CUBICMAP  0x200    //Reflection map is cubic
#define MF_XPFALL    0x400    //Do Transparency fall-off
#define MF_SUPERSMP  0x800    //Super sample material
#define MF_FACEMAP   0x1000   //Face-texture-coords
#define MF_PHONGSOFT 0x2000   //Soften phong hilites
#define MF_WIREABS   0x4000   //Absolute wire size
#define MF_REFBLUR   0x8000   //blurred reflection map

//----------------------------
                              //map flags
#define MAP_TRUE_COLOR  1     //request for true-color texture
#define MAP_NO_MIPMAP   2     //do not generate mipmap levels
#define MAP_UV_MIRROR   4
#define MAP_INVERT      8
#define MAP_NOWRAP      0x10
#define MAP_SAT         0x20     //summed area table 
#define MAP_ALPHA_SOURCE 0x40 //use ALPHA instead of RGB of map 
#define MAP_TINT        0x80     //tint for color 
#define MAP_DONT_USE_ALPHA 0x100 //don't use map alpha 
#define MAP_RGB_TINT    0x200    //Do RGB color transform
#define MAP_PROCEDURAL  0x400    //map name specifies procedural, not bitmap
#define MAP_NO_COMPRESS 0x800    //do not compress texture
#define MAP_ANIMATED    0x1000   //animated texture

//----------------------------
                              //material type (index)
enum{
   MAP_DIFFUSE,
   MAP_OPACITY,
   MAP_BUMP,
   MAP_SPECULAR,
   MAP_SHININESS,             //also called Glosiness
   MAP_SELF_ILLUM,
   MAP_REFLECTION,
   MAP_FILTER,
   MAP_REFRACTION,
   MAP_DISPLACEMENT,
   MAP_AMBIENT,
   MAP_SPECULAR_LEVEL,
   MAP_LAST
};

//----------------------------
// Convert internal map index into MAX map index.
inline int MAXMapIndex(int i){

   switch(i){
   case MAP_AMBIENT: return ID_AM;
   case MAP_DIFFUSE: return ID_DI;
   case MAP_SPECULAR: return ID_SP;
   case MAP_SHININESS: return ID_SH;
   case MAP_SELF_ILLUM: return ID_SI;
   case MAP_OPACITY: return ID_OP;
   case MAP_FILTER: return ID_FI;
   case MAP_BUMP: return ID_BU;
   case MAP_REFLECTION: return ID_RL;
   case MAP_REFRACTION: return ID_RR;
   case MAP_DISPLACEMENT: return ID_DP;
   case MAP_SPECULAR_LEVEL: return ID_SS;
   default: assert(0); return ID_DI;
   }
}

//----------------------------

struct S_map_data{
   C_str bitmap_name;
   dword flags;

   int percentage;
   //float texture_blur;
   S_vector2 uv_scale;
   S_vector2 uv_offset;
   bool use;
   dword anim_speed;

   S_map_data():
      use(false),
      flags(0),
      //texture_blur(1.1f),
      percentage(100),
      anim_speed(0)
   {
      uv_scale.x = 1.0f;
      uv_scale.y = 1.0f;
      uv_offset.x = 0.0f;
      uv_offset.y = 0.0f;
   }
   bool operator ==(const S_map_data &md) const{
      return (percentage==md.percentage &&
         bitmap_name==md.bitmap_name &&
         flags==md.flags &&
         uv_scale==md.uv_scale &&
         anim_speed==md.anim_speed &&
         uv_offset==md.uv_offset);
   }
};

//----------------------------

struct S_material{
   C_str name;
   S_rgb amb;
   S_rgb diff;
   S_rgb spec;
   float transparency;        //0.0 ... 1.0
   dword flags;               //combination of MF_??? flags
   int mirror_id;
   enum E_TRANS_MODE{
      TRANSP_NO,
      TRANSP_COLORKEY,
      TRANSP_COLORKEY_ALPHA,
   } transp_mode;
   bool intended_duplicate;

                              //effect percent sliders 
   //short shininess;     //0-100     
   //short refblur;       //0-100     
   short selfipct;      //0-100    

   S_map_data maps[MAP_LAST];

   S_material():
      transparency(0),
      mirror_id(-1),
      //shininess(50),
      flags(0),
      //refblur(0),
      selfipct(0),
      intended_duplicate(false),
      transp_mode(TRANSP_NO)
   {}

   bool HaveIdenticalData(const S_material &m) const{
      if(amb==m.amb &&
         diff==m.diff &&
         spec==m.spec &&
         transparency==m.transparency &&
         flags==m.flags &&
         mirror_id==m.mirror_id &&
         transp_mode==m.transp_mode &&
         selfipct==m.selfipct
         ){

         for(int i=MAP_LAST; i--; ){
            if(!(maps[i] == m.maps[i]))
               break;
         }
         if(i==-1)
            return true;
      }
      return false;
   }

   inline bool operator ==(const S_material &m) const{
      return (name==m.name && HaveIdenticalData(m));
   }
};

//----------------------------

#endif

