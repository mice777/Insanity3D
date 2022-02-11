#ifndef __COMMON_H
#define __COMMON_H

//----------------------------

#include <i3d\i3d_format.h>


#define MMAGIC 0x3D3D
#define M3DMAGIC 0x4D4D

//----------------------------
/*
                              //NURBs
#define N_NURBS                  0x4904
#define NURBS_UV_ORDER           0x4905 
#define NURBS_CVS                0x4906 
#define NURBS_KNOTS              0x4907
#define NURBS_TRANSFORM_MATRIX   0x4908
#define NURBS_MAT                0x4909
#define NURBS_FLIP_NORMALS       0x490a
#define NURBS_UV_MAP             0x490b
*/

//----------------------------

//#define JOINT_CLASSID BONE_OBJ_CLASSID
#define JOINT_CLASSID Class_ID(0x4d536096, 0xa7230a4)
#define JOINT_SUPERCLASSID GEOMOBJECT_CLASS_ID
//#define JOINT_SUPERCLASSID HELPER_CLASS_ID

// jointobj_params IDs
enum{
   jointobj_width,
   //jointobj_height,
   //jointobj_taper,
   jointobj_length,
   /*
   jointobj_sidefins, jointobj_sidefins_size, jointobj_sidefins_starttaper, jointobj_sidefins_endtaper,
   jointobj_frontfin, jointobj_frontfin_size, jointobj_frontfin_starttaper, jointobj_frontfin_endtaper,
   jointobj_backfin,  jointobj_backfin_size,  jointobj_backfin_starttaper,  jointobj_backfin_endtaper,
   jointobj_genmap
   */
};

//----------------------------

inline void SwapMatrixYZ(S_vector m[4]){
   for(int i=4; i--; ){
      swap(m[i].y, m[i].z);
   }
   swap(m[1], m[2]);
}

//----------------------------

//class Modifier *FindSkinModifier(class INode* node);

//----------------------------

#pragma pack(push,1)

struct S_rgb{
   byte b, g, r;
   S_rgb(){
      Zero();
   }
   void Zero(){
      r = 0;
      g = 0;
      b = 0;
   }
   void White(){
      r = 255;
      g = 255;
      b = 255;
   }
   inline bool operator ==(const S_rgb &e) const{
      return (r==e.r &&
         g==e.g &&
         b==e.b);
   }
};
#pragma pack(pop)

//----------------------------

void CenterDialog(HWND hwnd);

//----------------------------
// Determine which vertices are identical (by examining position) with others
//    in the pool, and create mapping array                                    
void MakeVertexMapping(const S_vector *vp, dword pitch, dword numv, dword *v_map, float thresh = .001f);

//----------------------------

#endif
