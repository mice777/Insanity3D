#ifndef __MATERIAL_H
#define __MATERIAL_H

#include "texture.h"

//----------------------------

                              //material
class I3D_material_base{
public:
   I3DMETHOD_(dword,AddRef)() = 0;
   I3DMETHOD_(dword,Release)() = 0;

   I3DMETHOD(SetAmbient)(const S_vector&) = 0;
   I3DMETHOD(SetDiffuse)(const S_vector&) = 0;
   I3DMETHOD(SetSpecular)(const S_vector&) = 0;
   I3DMETHOD(SetEmissive)(const S_vector&) = 0;
   I3DMETHOD(SetAlpha)(float) = 0;
   I3DMETHOD_(void,SetTexture)(I3D_MATERIAL_TEXTURE_INDEX, PI3D_texture_base) = 0;
   I3DMETHOD_(void,Update)() = 0;
   I3DMETHOD(Duplicate)(CPI3D_material) = 0;

   I3DMETHOD_(void,Set2Sided)(bool) = 0;

   I3DMETHOD_(const S_vector&,GetAmbient)() const = 0;
   I3DMETHOD_(const S_vector&,GetDiffuse)() const = 0;
   I3DMETHOD_(const S_vector&,GetSpecular)() const = 0;
   I3DMETHOD_(const S_vector&,GetEmissive)() const = 0;
   I3DMETHOD_(float,GetAlpha)() const = 0;

   I3DMETHOD_(PI3D_texture,GetTexture)(I3D_MATERIAL_TEXTURE_INDEX) = 0;
   I3DMETHOD_(CPI3D_texture,GetTexture)(I3D_MATERIAL_TEXTURE_INDEX) const = 0;
   I3DMETHOD_(bool,IsCkey)() const = 0;
   I3DMETHOD_(bool,Is2Sided)() const = 0;
};

//----------------------------

class I3D_material{

#define MATF_CKEY_ZERO_REF 1  //colorkeyed with zero alpha ref
#define MATF_CKEY_ALPHA    2
#define MATF_TXT_ALPHA     4
#define MATF_ADD_MODE      8  //material drawn using ADD blending mode
#define MATF_DIFFUSE_ALPHA 0x10
//#define MATF_MIRROR        0x20

   S_vector diffuse;          //diffuse color
   float diffuse_alpha;       //diffuse alpha
   S_vector ambient;          //ambient color
   S_vector emissive;         //emissive color
   float power;               //power
   bool two_sided;            //two-sided material

   dword sort_id;             //sorting identifier
   dword mat_flags;
   dword alpharef;            //alpha reference value used for alpha-blended items
   dword mirror_id;           //-1 = no mirror
   C_str name;

   friend I3D_driver;        //creation
protected:
   dword ref;
                              //all texture maps
   C_smart_ptr<I3D_texture_base> tp[MTI_LAST];

   S_vector2 detail_scale;    //scale of detail map
   float embm_scale;          //scale of embm map
   float embm_opacity;        //opacity of embm map

   PI3D_driver drv;

   I3D_material(PI3D_driver);
   I3D_material(const I3D_material& m);

public:
   //C_str name;             //identification

   const I3D_material& operator =(const I3D_material&);
   I3D_material(){}
   ~I3D_material();
                              //fast access functions
   //inline const C_str &GetName1() const{ return name; }
   inline bool IsTransl() const{ return mat_flags&(MATF_DIFFUSE_ALPHA|MATF_TXT_ALPHA|MATF_CKEY_ALPHA|MATF_ADD_MODE); }
   inline bool IsDiffuseAlpha() const{ return (mat_flags&MATF_DIFFUSE_ALPHA); }
   inline bool IsTextureAlpha() const{ return (mat_flags&MATF_TXT_ALPHA); }
   inline bool IsAddMode() const{ return (mat_flags&MATF_ADD_MODE); }

   inline void SetMirrorID(dword id){ mirror_id = id; }
   inline dword GetMirrorID1() const{ return mirror_id; }
   inline void SetName1(const C_str &n){ name = n; }
   inline void SetAddMode(bool b){ mat_flags &= ~MATF_ADD_MODE; if(b) mat_flags |= MATF_ADD_MODE; }

   inline float GetEMBMOpacity1() const{ return embm_opacity; }

   inline void SetDetailScale(const S_vector2 &s){ detail_scale = s; }
   inline const S_vector2 &GetDetailScale() const{ return detail_scale; }

   inline void SetEMBMScale(float f){ embm_scale = f; }
   inline float GetEMBMScale() const{ return embm_scale; }

   inline bool IsCkeyAlpha1() const{ return (mat_flags&MATF_CKEY_ALPHA); }
   inline bool Is2Sided1() const{ return two_sided; }
   inline PI3D_texture_base GetTexture1(I3D_MATERIAL_TEXTURE_INDEX mti = MTI_DIFFUSE){ return tp[mti]; }
   inline CPI3D_texture_base GetTexture1(I3D_MATERIAL_TEXTURE_INDEX mti = MTI_DIFFUSE) const{ return tp[mti]; }
   inline const S_vector &GetDiffuse1() const{ return diffuse; }
   inline const S_vector &GetAmbient1() const{ return ambient; }
   inline const S_vector &GetEmissive1() const{ return emissive; }
   inline float GetAlpha1() const{ return diffuse_alpha; }
   inline float GetPower1() const{ return power; }

   inline dword GetSortID() const{ return sort_id; }
   inline dword GetAlphaRef() const{ return alpharef; }

   inline dword GetMatGlags() const{ return mat_flags; }
   inline void SetMatGlags(dword dw){ mat_flags = dw; }

public:
   I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(void,SetAmbient)(const S_vector &v){ ambient = v; }
   I3DMETHOD_(void,SetDiffuse)(const S_vector &v){ diffuse = v; }
   //I3DMETHOD(SetSpecularColor)(const S_vector&);
   //I3DMETHOD(SetSpecularPower)(float);
   //I3DMETHOD(SetSpecularAngle)(float);
   I3DMETHOD_(void,SetEmissive)(const S_vector &v){ emissive = v; }
   I3DMETHOD_(void,SetAlpha)(float a){
      a = Max(0.0f, Min(1.0f, a));

      diffuse_alpha  = a;

      mat_flags &= ~MATF_DIFFUSE_ALPHA;
      if(a<1.0f)
         mat_flags |= MATF_DIFFUSE_ALPHA;
   }
   I3DMETHOD_(void,SetTexture)(I3D_MATERIAL_TEXTURE_INDEX, PI3D_texture_base);
   I3DMETHOD_(void,Duplicate)(CPI3D_material);

   I3DMETHOD_(void,Set2Sided)(bool b){ two_sided = b; }

   I3DMETHOD_(const S_vector&,GetAmbient)() const{ return ambient; }
   I3DMETHOD_(const S_vector&,GetDiffuse)() const{ return diffuse; }
   //I3DMETHOD_(const S_vector&,GetSpecularColor)() const{ static S_vector v(0, 0, 0); return v; }
   //I3DMETHOD_(float,GetSpecularPower)() const{ return 0.0f; }
   //I3DMETHOD_(float,GetSpecularAngle)() const{ return 0.0f; }
   I3DMETHOD_(const S_vector&,GetEmissive)() const{ return emissive; }
   I3DMETHOD_(float,GetAlpha)() const{ return diffuse_alpha; }

   I3DMETHOD_(PI3D_texture_base,GetTexture)(I3D_MATERIAL_TEXTURE_INDEX mti){ return GetTexture1(mti); }
   I3DMETHOD_(CPI3D_texture_base,GetTexture)(I3D_MATERIAL_TEXTURE_INDEX mti) const{ return GetTexture1(mti); }
   I3DMETHOD_(bool,IsCkey)() const{ return (mat_flags&MATF_CKEY_ALPHA); }
   I3DMETHOD_(bool,Is2Sided)() const{ return Is2Sided1(); }

   I3DMETHOD_(void,SetEMBMOpacity)(float f){ embm_opacity = f; }
   I3DMETHOD_(float,GetEMBMOpacity)() const{ return embm_opacity; }

   I3DMETHOD_(void,SetName)(const C_str &n){ name = n; }
   I3DMETHOD_(const C_str&,GetName)() const{ return name; }

   I3DMETHOD_(int,GetMirrorID)() const{ return (int)mirror_id; }
};

//----------------------------

#endif
