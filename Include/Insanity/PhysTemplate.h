#ifndef __PHYS_TEMPLATE_H
#define __PHYS_TEMPLATE_H
#include <C_vector.h>
#include <C_chunk.h>
#include <IPhysics.h>
#include <I3D\Bin_format.h>

//----------------------------

                              //physics template entry (info about physical object assignments)
struct S_phys_template{
   struct S_body{
      C_str name;             //name of body's frame ('*' means root frame)
      bool is_static;
      float density;
      bool density_as_weight;
      S_body():
         is_static(false),
         density(I3DIntAsFloat(0x80000000)),
         density_as_weight(false)
      {}
   };
   C_vector<S_body> bodies;

   enum E_JOINT_VALUE_TYPE{
      VAL_STOP_LO0,
      VAL_STOP_LO1,
      VAL_STOP_LO2,
      VAL_STOP_HI0,
      VAL_STOP_HI1,
      VAL_STOP_HI2,
      VAL_MAX_FORCE,
      VAL_FUDGE,
      VAL_BOUNCE,
      VAL_CFM,
      VAL_STOP_ERP,
      VAL_STOP_CFM,
      VAL_SUSP_ERP,
      VAL_SUSP_CFM,
      VAL_LAST
   };

   struct S_joint{
      C_str name;
      enum IPH_JOINT_TYPE type;
      C_str body[2];          //names of connected bodies ('*' means root frame)
      S_vector pos;
      float val[VAL_LAST];
      S_joint():
         pos(0, 0, 0),
         type((IPH_JOINT_TYPE)0)
      {
                              //init to default value (negative zero, marking 'use defaults')
         for(int i=VAL_LAST; i--; )
            val[i] = I3DIntAsFloat(0x80000000);
      }
   };
   C_vector<S_joint> joints;

   enum{
      CK_NUM_BODIES = 10,
      CK_NUM_JOINTS,
      CK_BODY = 1000,
         //CT_NAME name
         CK_BODY_STATIC,
         CK_BODY_DENSITY,
         CK_BODY_DENSITY_AS_WEIGHT,
      CK_JOINT = 2000,
         //CT_NAME name
         CT_J_BODY1,
            //string
         CT_J_BODY2,
            //string
         CT_J_POS,
         CT_J_TYPE,
         CT_J_VALS,
   };

   void Save(C_chunk &ck) const;

   bool IsEmpty() const{ return (!bodies.size() && !joints.size()); }

   void Clear(){
      bodies.clear();
      joints.clear();
   }

   void Load(C_chunk &ck);

//----------------------------
// Init simulation from this template. The body/joint frames are searched either from 'scn' using absolute names,
//    or from 'frm_root' using relative names. Only one of these must be non-NULL.
   bool InitSimulation(C_vector<C_smart_ptr<IPH_body> > &bodies, C_vector<C_smart_ptr<IPH_joint> > &joints,
      PIPH_world world, PI3D_scene scn, PI3D_frame frm_root,
      void (*cb_err)(const char *cp, void *context) = NULL, void *cb_context = NULL) const;
};

//----------------------------

#endif