/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Material.cpp
   Content: All about materials.
--------------------------------------------------------*/

#include "all.h"
#include "material.h"
#include "driver.h"

//----------------------------


                              //material class
I3D_material::I3D_material(PI3D_driver drv1):
   ref(1),

   diffuse(1.0f, 1.0f, 1.0f),
   diffuse_alpha(1.0f),
   ambient(0, 0, 0),
   emissive(0, 0, 0),
   power(1.0f),
   two_sided(false),

   detail_scale(1.0f, 1.0f),
   embm_scale(1.0f),
   embm_opacity(1.0f),
   mat_flags(0),
   sort_id(0),
   mirror_id((dword)-1),
   alpharef(0x80),
   drv(drv1)
{
   drv->AddCount(I3D_CLID_MATERIAL);
}

//----------------------------

I3D_material::~I3D_material(){

   //if(drv->last_render_mat==this) drv->last_render_mat = NULL;
   drv->DecCount(I3D_CLID_MATERIAL);
}

//----------------------------

void I3D_material::SetTexture(I3D_MATERIAL_TEXTURE_INDEX mti, PI3D_texture_base tp1){

   tp[mti] = tp1;
   {
      alpharef = 0x80;

      mat_flags &= ~(MATF_CKEY_ALPHA | MATF_TXT_ALPHA);

      if(tp[MTI_DIFFUSE]){
         sort_id = tp[MTI_DIFFUSE]->GetSortID() & PRIM_SORT_MAT_MASK;
         if(tp[MTI_DIFFUSE]->GetTxtFlags()&TXTF_CKEY_ALPHA){
            mat_flags |= MATF_CKEY_ALPHA;
            if(mat_flags&MATF_CKEY_ZERO_REF){
                                 //minimal alpha ref requested for colorkeys
               alpharef = 0x20;
            }else{
                                 //automatic alpha ref
               alpharef = 0xe0;
               if(tp1->GetTxtFlags()&TXTF_MIPMAP)
                  alpharef = 0xa0;
            }
         }
         if(tp[MTI_DIFFUSE]->GetTxtFlags()&TXTF_ALPHA){
            mat_flags |= MATF_TXT_ALPHA;
            alpharef = 0xe0;
         }
         if(GetAlpha() != 1.0f)
            mat_flags &= ~TXTF_CKEY_ALPHA;
      }else{
         sort_id = 0;
      }
      if(tp[MTI_DETAIL] && (tp[MTI_DETAIL]->GetTxtFlags()&TXTF_ALPHA)){
         mat_flags |= MATF_TXT_ALPHA;
      }
   }
}

//----------------------------

void I3D_material::Duplicate(CPI3D_material mp){

   if(mp==this)
      return;

   diffuse = mp->diffuse;
   diffuse_alpha = mp->diffuse_alpha;
   ambient = mp->ambient;
   emissive = mp->emissive;
   power = mp->power;
   two_sided = mp->two_sided;

   mat_flags = mp->mat_flags;
   detail_scale = mp->detail_scale;
   embm_scale = mp->embm_scale;
   embm_opacity = mp->embm_opacity;
   mirror_id = mp->mirror_id;
   name = mp->name;

   for(int i=0; i<MTI_LAST; i++)
      tp[i] = mp->tp[i];
}

//----------------------------
//----------------------------
