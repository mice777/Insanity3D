/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.
   All rights reserved.

   File: Dummy.cpp
   Content: Dummy frame.
--------------------------------------------------------*/

#include "all.h"
#include "dummy.h"
#include "scene.h"


//----------------------------

                              //dummy frame class
I3D_dummy::I3D_dummy(PI3D_driver d):
   I3D_frame(d)
{
   drv->AddCount(I3D_CLID_DUMMY);
   type = FRAME_DUMMY;
   enum_mask = ENUMF_DUMMY;
   //bbox.min.Zero();
   //bbox.max.Zero();
}

//----------------------------

I3D_RESULT I3D_dummy::Duplicate(CPI3D_frame frm){

   /*
   if(frm->GetType1()==type){
      PI3D_dummy dp = I3DCAST_DUMMY(frm);
      bbox = dp->bbox;
   }
   */
   return I3D_frame::Duplicate(frm);
}

//----------------------------

I3D_dummy::~I3D_dummy(){
   drv->DecCount(I3D_CLID_DUMMY);
}

//----------------------------
/*
static const I3D_bbox bbox;

const I3D_bbox *I3D_dummy::GetBBox() const{
   return &bbox;
}

//----------------------------

void I3D_dummy::SetBBox(const I3D_bbox &bb){
   //bbox = bb;
}
*/
//----------------------------

void I3D_dummy::Draw1(PI3D_scene scene, bool strong) const{

                              //draw icon as big as its bound-box is
   float size = GetMatrix().GetScale().Magnitude() * .2f;
   /*
   float d = (GetMatrix()(3) - scene->GetActiveCamera1()->GetMatrixDirect()(3)).Magnitude();
   float size = .1f * d;
   */

   scene->DrawIcon(GetWorldPos(), 10,
      strong ? 0xffffffff : 0x80ffffff,
      size);
}

//----------------------------
//----------------------------
