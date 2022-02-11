/*--------------------------------------------------------
   Copyright (c) 2000, 2001 Lonely Cat Games.
   All rights reserved.

   File: Light.cpp
   Content: Light frame.
--------------------------------------------------------*/

#include "all.h"
#include "light.h"
#include "scene.h"
#include "camera.h"


#define FOG_FADE_OUT_RATIO 1.0f  //ratio between fog's distance to camera and its radius, below which fog starts to fade out

//----------------------------

                              //light class
I3D_light::I3D_light(PI3D_driver d):
   I3D_frame(d),
   lflags(I3DLIGHTMODE_VERTEX | I3DLIGHTMODE_LIGHTMAP),
   last_render_time((dword)-1),
   range_n(5.0f),
   range_f(10.0f),
   power(1.0f),
   ltype(I3DLIGHT_NULL),
   normalized_dir(0, 0, 0),
   dw_color(0),
   range_n_scaled(0),
   range_f_scaled(0),
   range_f_scaled_2(0),
   range_delta_scaled(0)
{
   drv->AddCount(I3D_CLID_LIGHT);

   cone_in = 20.0f*PI/180.0f;
   cone_out = 60.0f*PI/180.0f;
   //ld.specular = S_vector(1.0f, 1.0f, 1.0f);
   //ld.specular_power = 0.5f;

   type = FRAME_LIGHT;
   enum_mask = ENUMF_LIGHT;

   color.Zero();              //avoid SetColor to be too smart
   SetColor(S_vector(1.0f, 1.0f, 1.0f));
}

//----------------------------

I3D_light::~I3D_light(){

                              //unlink all light sectors
   while(light_sectors.size())
      light_sectors[0]->RemoveLight(this);

   drv->DecCount(I3D_CLID_LIGHT);
}

//----------------------------

I3D_RESULT I3D_light::Duplicate(CPI3D_frame frm){

   if(frm->GetType1()==FRAME_LIGHT){
      CPI3D_light lp = I3DCAST_CLIGHT(frm);

      while(NumLightSectors())
         GetLightSectors()[0]->RemoveLight(this);

      SetLightType(lp->GetLightType1());

      for(int i=lp->NumLightSectors(); i--; )
         lp->GetLightSectors()[i]->AddLight(this);

      color = lp->color;
      power = lp->power;
      range_n = lp->range_n;
      range_f = lp->range_f;
      cone_in = lp->cone_in;
      cone_out = lp->cone_out;

      dw_color = lp->dw_color;
      drv = lp->drv;
      lflags = lp->lflags;
      lflags |= LFLG_DIRTY | LFLG_COLOR_DIRTY;
   }
   return I3D_frame::Duplicate(frm);
}

//----------------------------

I3D_RESULT I3D_light::SetLightType(I3D_LIGHTTYPE t){

   if(ltype==t)
      return I3D_OK;

   switch(t){
   case I3DLIGHT_NULL:
   case I3DLIGHT_POINT:
   case I3DLIGHT_SPOT:
   case I3DLIGHT_DIRECTIONAL:
   case I3DLIGHT_AMBIENT:
   case I3DLIGHT_FOG:
   case I3DLIGHT_POINTAMBIENT:
   case I3DLIGHT_POINTFOG:
   case I3DLIGHT_LAYEREDFOG:
      break;
   default:
      return I3DERR_INVALIDPARAMS;
   }

   MarkDirty();

   bool was_fog = IsFogType();
   ltype = t;
   bool is_fog = IsFogType();

   if(is_fog != was_fog){
                              //change list in sector's lists
      for(int i=light_sectors.size(); i--; ){
         PI3D_sector sct = light_sectors[i];
         sct->sct_flags |= SCTF_LIGHT_LIST_RESET;
         C_vector<PI3D_light> *l[2] = { &sct->light_list, &sct->fog_light_list };
         int pi;
         pi = FindPointerInArray((void**)&l[was_fog]->front(), l[was_fog]->size(), this);
         assert(pi!=-1);
         (*l[was_fog])[pi] = l[was_fog]->back(); l[was_fog]->pop_back();
         assert(-1 == FindPointerInArray((void**)&l[is_fog]->front(), l[is_fog]->size(), this));
         l[is_fog]->push_back(this);
         if(was_fog && sct->fog_light==this)
            sct->fog_light = NULL;
         if(is_fog)
            sct->fog_light = this;
      }
   }
   lflags |= LFLG_COLOR_DIRTY;
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_light::SetMode(dword m){

   if(m&(~LFLG_MODE_MASK))
      return I3DERR_INVALIDPARAMS;
   if(GetMode()!=m){
      MarkDirty();
      lflags &= ~LFLG_MODE_MASK;
      lflags |= m;
      MarkDirty();

      for(int i=light_sectors.size(); i--; ){
         PI3D_sector sct = light_sectors[i];
         sct->sct_flags |= SCTF_LIGHT_LIST_RESET;
      }
   }
   return I3D_OK;
}

//----------------------------

void I3D_light::SetColor(const S_vector &v){

                              //check if some change occured
   float delta = I3DFabs(color.x-v.x) + I3DFabs(color.y-v.y) + I3DFabs(color.z-v.z);
   if(delta<MRG_ZERO)
      return;

   color = v;
   MarkDirty();
   lflags |= LFLG_COLOR_DIRTY;
}

//----------------------------

void I3D_light::SetPower(float p){
                              //check if some change occured
   float delta = I3DFabs(power-p);
   if(delta<MRG_ZERO)
      return;

   power = p;
   MarkDirty();
   lflags |= LFLG_COLOR_DIRTY;
}

//----------------------------

void I3D_light::SetRange(float rn, float rf){

                              //check if some change occured
   float delta = I3DFabs(range_n-rn) + I3DFabs(range_f-rf);
   if(delta<MRG_ZERO)
      return;

   range_n = rn;
   range_f = Min(1e+10f, rf);

   MarkDirty();
}

//----------------------------

I3D_RESULT I3D_light::SetCone(float n, float f){

   if(n > f)
      n = f;
                              //check if some change occured
   float delta = I3DFabs(cone_in-n) + I3DFabs(cone_out-f);
   if(delta>MRG_ZERO){
      cone_in = n;
      cone_out = f;
      MarkDirty();
   }
   return I3D_OK;
}

//----------------------------

void I3D_light::SetOn(bool on){

   if(on){
      if(!(frm_flags&FRMFLAGS_ON)){
         I3D_frame::SetOn(on);
         MarkDirty();
      }
   }else{
      if(frm_flags&FRMFLAGS_ON){
         MarkDirty();
         I3D_frame::SetOn(on);
      }
   }
}

//----------------------------

void I3D_light::MarkDirty() const{

   if(!(frm_flags&FRMFLAGS_ON))
      return;
   lflags |= LFLG_DIRTY;

   if(lflags&(I3DLIGHTMODE_VERTEX|I3DLIGHTMODE_DYNAMIC_LM)){
      for(int i=light_sectors.size(); i--; )
         light_sectors[i]->frm_flags |= FRMFLAGS_HR_LIGHT_RESET;
   }
}

//----------------------------

void I3D_light::UpdateLight(){

   if(lflags & (LFLG_DIRTY | LFLG_COLOR_DIRTY)){
      if(lflags&LFLG_COLOR_DIRTY){
                              //update color
         switch(ltype){
         case I3DLIGHT_FOG:
         case I3DLIGHT_LAYEREDFOG:
         case I3DLIGHT_AMBIENT:
            {
               S_vector dst_color = color * power;
                              //make dword color
               int c[3] = {
                  FloatToInt(dst_color[0]*255.0f),
                  FloatToInt(dst_color[1]*255.0f),
                  FloatToInt(dst_color[2]*255.0f)
               };
               if(ltype==I3DLIGHT_FOG){
                              //match dword color to current pixel format
                  const S_pixelformat &pf = *drv->GetGraphInterface()->GetPixelFormat();
                  for(int i=0; i<3; i++){
                     dword dw;
                     const dword &mask = (&pf.r_mask)[i];
                     dw = FindLastBit(mask);
                     dw = _rotr(mask, dw-7);
                     c[i] = Min(255, c[i] + ((~(signed char)dw)+1)/2);
                     c[i] &= dw;
                  }
               }
                              //clamp
               for(int i=0; i<3; i++) c[i] = Max(0, Min(255, c[i]));
               dw_color = 0xff000000 | (c[0]<<16) | (c[1]<<8) | c[2];
            }
            break;
         }
      }

      switch(ltype){
      case I3DLIGHT_NULL:
      case I3DLIGHT_AMBIENT:
      case I3DLIGHT_FOG:
         break;

      case I3DLIGHT_SPOT:
         GetCone(inner_cos, outer_cos);
         outer_tan = (float)tan(outer_cos*.5f);
         inner_cos = (float)cos(inner_cos*.5f);
         outer_cos = (float)cos(outer_cos*.5f);
                              //flow...
      case I3DLIGHT_POINTFOG:
      case I3DLIGHT_LAYEREDFOG:
      case I3DLIGHT_POINT:
      case I3DLIGHT_DIRECTIONAL:
      case I3DLIGHT_POINTAMBIENT:
         {
                                 //update characteristics
            const S_matrix &m = GetMatrix();
            float scale = m(2).Magnitude();
            if(!IsAbsMrgZero(scale))
               normalized_dir = m(2) / scale;
            else
               normalized_dir.Zero();

                              //compute run-time cached values
            range_n_scaled = range_n * scale;
            range_f_scaled = range_f * scale;
            range_f_scaled_2 = range_f_scaled * range_f_scaled;
            range_delta_scaled = range_f_scaled - range_n_scaled;
         }
         break;
      default: assert(0);
      }
      lflags &= ~(LFLG_DIRTY | LFLG_COLOR_DIRTY);
   }
}

//----------------------------

#define SPHERE_NUM_VERTS 24
static word sphere_c_list[SPHERE_NUM_VERTS*2];

void I3D_light::Draw1(PI3D_scene scene, const S_view_frustum *vf, bool strong) const{

                              //sphere values
   static struct S_auto_init{
      S_auto_init(){
                                 //init volume sphere
         for(int i=0; i<SPHERE_NUM_VERTS; i++){
            sphere_c_list[i*2+0] = word(i + 0);
            sphere_c_list[i*2+1] = word(i + 1);
         }
         sphere_c_list[i*2-1] = 0;
      }
   } auto_init;

   static const dword color_cone_out = 0xc0c030, color_cone_in = 0x808020;

   dword alpha(strong ? 0xff : 0x50);

   dword lt = GetLightType();
   switch(lt){

   case I3DLIGHT_NULL:
      if(vf){                 //check clipping
         bool clip;
         if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
            return;
      }
      scene->DrawIcon(GetWorldPos(), 1, (alpha<<24) | 0xffffff);
      break;

   case I3DLIGHT_POINT:
   case I3DLIGHT_POINTAMBIENT:
   case I3DLIGHT_POINTFOG:
   case I3DLIGHT_LAYEREDFOG:
      {
         float nr, fr;
         GetRange(nr, fr);
         switch(lt){
         case I3DLIGHT_LAYEREDFOG:
            if(strong)
               scene->DebugDrawCylinder(GetMatrix(), fr, fr, nr, (alpha<<24) | color_cone_out);
            scene->DebugDrawArrow(GetMatrix(), nr, (alpha<<24) | color_cone_in, 1);
            break;
         case I3DLIGHT_POINTFOG:
            scene->DebugDrawSphere(GetMatrix(), fr, (alpha<<24) | color_cone_out);
            scene->DebugDrawSphere(GetMatrix(), nr, (alpha<<24) | color_cone_in);
            break;
         default:
            scene->DebugDrawSphere(GetMatrix(), fr, (alpha<<24) | color_cone_out);
            scene->DebugDrawSphere(GetMatrix(), nr, (alpha<<24) | color_cone_in);
         }

         if(vf){              //check clipping
            bool clip;
            if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
               return;
         }

         switch(lt){
         case I3DLIGHT_POINT: scene->DrawIcon(GetWorldPos(), 1, (alpha<<24) | 0xffffff); break;
         case I3DLIGHT_POINTAMBIENT: scene->DrawIcon(GetWorldPos(), 6, (alpha<<24) | 0xffffff); break;
         case I3DLIGHT_POINTFOG: scene->DrawIcon(GetWorldPos(), 12, (alpha<<24) | 0xffffff); break;
         case I3DLIGHT_LAYEREDFOG: scene->DrawIcon(GetWorldPos(), 13, (alpha<<24) | 0xffffff); break;
         }
      }
      break;

   case I3DLIGHT_DIRECTIONAL:
   //case I3DLIGHT_SPOT:
      if(vf){                 //check clipping
         bool clip;
         if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
            return;
      }
      scene->DebugDrawArrow(GetMatrix(), 1.0f, (alpha<<24) | color_cone_out, 2);
      scene->DrawIcon(GetWorldPos(), lt==I3DLIGHT_DIRECTIONAL ? 4 : 7, (alpha<<24) | 0xffffff);
      break;

   case I3DLIGHT_SPOT:
      {
         float cone[2];
         GetCone(cone[0], cone[1]);
         cone[0] *= .5f; cone[1] *= .5f;
         float rng[2];
         GetRange(rng[0], rng[1]);
         S_matrix m(I3DGetIdentityMatrix());
         const int sphere_num_verts = 24;
         S_vector sphere_circle[sphere_num_verts+1];

         sphere_circle[sphere_num_verts].Zero();
                     //outer range

         for(int ii=0; ii<2; ii++){
            float cosc = (float)cos(cone[ii]);
            float sinc = (float)sin(cone[ii]);
            for(int j=0; j<sphere_num_verts; j++){
               sphere_circle[j].z = cosc;
               float f = (float)j * PI*2.0f / (float)sphere_num_verts;
               sphere_circle[j].x = sinc * (float)cos(f);
               sphere_circle[j].y = sinc * (float)sin(f);
            }

            m(0, 0) = m(1, 1) = m(2, 2) = rng[ii];
            S_matrix wm = m * GetMatrix();
            scene->SetRenderMatrix(wm);
            scene->DrawLines(sphere_circle, sphere_num_verts, sphere_c_list, sphere_num_verts*2,
               (alpha<<24) | (!ii ? color_cone_in : color_cone_out));
                     //connecting lines
            static const word indx[] = {
               sphere_num_verts, 0,
               sphere_num_verts, sphere_num_verts/4,
               sphere_num_verts, sphere_num_verts/2,
               sphere_num_verts, sphere_num_verts*3/4,
            };
            scene->DrawLines(sphere_circle, sphere_num_verts + 1,
               indx, sizeof(indx)/sizeof(word), (alpha<<24) | color_cone_out);
         }
         scene->DrawIcon(GetWorldPos(), 5, (alpha<<24) | 0xffffff);
      }
      break;

   case I3DLIGHT_AMBIENT:
      if(vf){                 //check clipping
         bool clip;
         if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
            return;
      }
      scene->DrawIcon(GetWorldPos(), 6, (alpha<<24) | 0xffffff);
      break;

   case I3DLIGHT_FOG:
      if(vf){                 //check clipping
         bool clip;
         if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
            return;
      }
      scene->DrawIcon(GetWorldPos(), 7, (alpha<<24) | 0xffffff);
      break;
   }
}

//----------------------------
//----------------------------
