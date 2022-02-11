/*--------------------------------------------------------
   Copyright (c) 1999, 2000 Michal Bacik. 
   All rights reserved.

   File: User.cpp
   Content: User frame.
--------------------------------------------------------*/

#include "all.h"
#include "user.h"
#include "driver.h"

//----------------------------
                              //user frame class
I3D_user::I3D_user(PI3D_driver d):
   I3D_frame(d),
   add_data(0)
{
   type = FRAME_USER;
   enum_mask = ENUMF_USER;
   drv->AddCount(I3D_CLID_USER);
}

//----------------------------

I3D_RESULT I3D_user::Duplicate(CPI3D_frame frm){

   if(frm==this) return I3D_OK;
   if(frm->GetType1()==FRAME_USER){
      CPI3D_user up = I3DCAST_CUSER(frm);
      add_data = up->add_data;
   }
   return I3D_frame::Duplicate(frm);
}

//----------------------------

I3D_user::~I3D_user(){
   drv->DecCount(I3D_CLID_USER);
}

//----------------------------
//----------------------------
