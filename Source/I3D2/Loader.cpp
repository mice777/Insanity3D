/*--------------------------------------------------------
   Copyright (c) Lonely Cat Games
   All rights reserved.

   Loader of Insanity3D format.
--------------------------------------------------------*/

#include "all.h"
#include <i3d\i3d_format.h>
#include "loader.h"

#include "dummy.h"
#include "visual.h"
#include "joint.h"
#include "volume.h"
#include "mesh.h"
#include "procedural.h"
#include "anim.h"
#include <integer.h>

#define ANIM_FPS 100
//#define AUTO_SMOOTH_ANGLE (50.0f*PI/180.0f)  //angle of autosmoothing - used when smoothgroups are not present
#define FORCE_MIPMAPPING      //force using mipmapping, unless it is explicitly disabled
#define REPORT_EMPTY_MESH_ERROR  //report meshes with no vertices or faces
#define REPORT_EMPTY_SMOOTHGROUPS //report warning on meshes with faces with no smoothgroup set
#define REPORT_INVALID_FACES  //report meshes with invalid (on line) faces
//#define FORCE_DETAILMAP_COMPRESSION
//#define REPORT_UNUSED_VERTS  //report meshes with unused vertices
//#define DETECT_DUPLICATED_MATERIALS //detect duplicated materials in one loaded file
#define USE_DATABASE          //try to cache computation-expensive info in database
#define MAX_VERTICES 0x7fff   //max vertices we may duplicate due to UV mapping and normal generation

//----------------------------

#if defined _DEBUG || 0
//#define DEBUG_PROFILE_SMOOTH  //profile SmoothSceneNormals call
//#define DEBUG_PROFILE_NORMALS //profile MakeVertexNormals
#endif

//----------------------------
//----------------------------
// Check if UV coordinates are almost equal.
inline bool AlmostEqual(const I3D_text_coor &txt1, const I3D_text_coor &txt2){

   return ((I3DFabs(txt1.x - txt2.x) < 1e-4f) && (I3DFabs(txt1.y - txt2.y) < 1e-4f));
}

//----------------------------
//----------------------------

enum E_PROP_KEYWORD{          //frame properties keyword IDs
   PROP_UNKNOWN,
                              //casting:
   PROP_CAST_SNGM,
   PROP_CAST_PRTC,
   PROP_CAST_LMAP,
   PROP_CAST_BBRD,
   PROP_CAST_OCCL,
   PROP_CAST_SECT,
   PROP_CAST_MRPH,
   PROP_CAST_FLRE,
   PROP_CAST_OUVS,
   PROP_CAST_FIRE,

   PROP_CAST_JTVB,
   PROP_CAST_JTBB,
   PROP_REGION,

   PROP_CAST_VOLS,
   PROP_CAST_VOLR,
   PROP_CAST_VOLB,
   PROP_CAST_VOLC,
   PROP_CAST_VOLP,

                              //modifiers:
   PROP_HELP,
   PROP_SIMPLE_SECTOR,
   PROP_SECTOR_ONEWAY_PORTAL,
   PROP_SMOOTH,
   PROP_AUTO_LOD,
   PROP_HIDE,
   PROP_MATERIAL,
   PROP_PROCEDURAL,
   PROP_LOCK,
   PROP_BREAK,
   PROP_HEAD,
   PROP_PARAM,
};

static const struct S_PROP_keyword{ //properties keywords
   E_PROP_KEYWORD id;
   const char *keyword;
} prop_keywords[] = {
                              //casts:
   {PROP_CAST_SNGM, "SNGM"},
   {PROP_CAST_PRTC, "PRTC"},
   {PROP_CAST_LMAP, "LMAP"},
   {PROP_CAST_BBRD, "BBRD"},
   {PROP_CAST_SECT, "SECT"},
   {PROP_CAST_MRPH, "MRPH"},
   {PROP_CAST_FLRE, "FLRE"},
   {PROP_CAST_OUVS, "OUVS"},

   {PROP_CAST_VOLS, "VOLS"},
   {PROP_CAST_VOLR, "VOLR"},
   {PROP_CAST_VOLB, "VOLB"},
   {PROP_CAST_VOLC, "VOLC"},
   {PROP_CAST_VOLP, "VOLP"},

   {PROP_CAST_JTBB, "JTBB"},
   {PROP_CAST_JTVB, "JTVB"},
   {PROP_REGION, "region"},
                              //modifiers:
   {PROP_HELP, "help"},
   {PROP_SIMPLE_SECTOR, "simple"},
   {PROP_SECTOR_ONEWAY_PORTAL, "one_way_portal"},
   {PROP_SMOOTH, "smooth"},
   {PROP_AUTO_LOD, "auto_lod"},
   {PROP_HIDE, "hide"},
   {PROP_MATERIAL, "material"},
   {PROP_PROCEDURAL, "procedural"},
   {PROP_LOCK, "lock"},
   {PROP_BREAK, "break"},
   {PROP_HEAD, "head"},
   {PROP_PARAM, "param"},

   {PROP_UNKNOWN}             //must be last!
};

//----------------------------
// Get next keyword from property stream, return it's type and return also
// new pointer into property stream (optional),                              
//----------------------------
static E_PROP_KEYWORD GetPropertyKeyword(const char *cp, const char **ptr_out, C_str *kw_name){

   if(ptr_out) *ptr_out = NULL;
   if(!cp){                   //NULL properties
      return PROP_UNKNOWN;
   }
   int slen;
   char keyword[256];
                              //take care of 1st comma
   for(slen=0; slen<sizeof(keyword) && cp[slen]; slen++){
      keyword[slen] = cp[slen];
      if(keyword[slen]==',' || isspace(keyword[slen])){
         keyword[slen] = 0;
         break;
      }
   }
   keyword[slen] = 0;
   if(kw_name)
      *kw_name = keyword;
   if(ptr_out){
      *ptr_out = cp + slen;
      while(isspace(**ptr_out) || (**ptr_out)==',') ++(*ptr_out);
      if(!(**ptr_out)) *ptr_out = NULL;    //end of input
   }
   if(slen<3) return PROP_UNKNOWN;
   for(const S_PROP_keyword *pk = prop_keywords; pk->id!=PROP_UNKNOWN; ++pk)
      if(!strnicmp(pk->keyword, cp, slen)) break;
   return pk->id;
}

//----------------------------
// Convert keyword back to string.
//----------------------------
static const char *GetKeywordString(E_PROP_KEYWORD id){

   for(int i=0; prop_keywords[i].id!=PROP_UNKNOWN; i++)
   if(prop_keywords[i].id==id){
      return prop_keywords[i].keyword;
   }
   return NULL;
}

//----------------------------
// Get first keyword in frame's properties, or PROP_UNKNOWN if not found.
// 'rtn' contains info for getting next data.
// To get further keywords, use GetPropertiesNext.
//----------------------------
static bool GetPropetriesBegin(const C_str &str, C_str &rtn, E_PROP_KEYWORD &kw, C_str *kw_name = NULL){

                              //check properties
   const void *mem;
   dword size;
   mem = (const char*)str;
   size = str.Size();
   if(size < 3){
      kw = PROP_UNKNOWN;
      return false;
   }

   const char *cp = (const char*)mem;
   if(cp && cp[0] == '{')//} <- leave it there!
   {
      ++cp;
      while(isspace(*cp)) ++cp;
      if(*cp==',') ++cp;
      while(isspace(*cp)) ++cp;
      rtn = cp;
                              //find and erase terminator
      for(dword j=0; j<rtn.Size(); j++)  //{ <- leave it there!
      if(rtn[j]=='}'){
         rtn[j] = 0;
         break;
      }
   }else
      return false;
   kw = GetPropertyKeyword(rtn, &cp, kw_name);
   rtn = cp;

   return true;
}

//----------------------------
// Get next keyword in property buffer (str).
//----------------------------
static bool GetPropertiesNext(C_str &str, E_PROP_KEYWORD &kw, C_str *kw_name = NULL){

                              //skip white-space
   for(dword i=0; isspace(str[i]); i++);

   if(str.Size() <= i)
      return false;

   const char *cp;
                              //decode
   kw = GetPropertyKeyword(&str[i], &cp, kw_name);
   str = cp;

   return true;
}

//----------------------------
// Get value from properties buffer specified by format string.
// Note: format string must have second parameter '%n'.
//----------------------------
static bool GetPropertiesValue(C_str &str, const char *fmt_string, void *retp){

   if(!str.Size())
      return false;
                              //skip white-space
   for(dword i=0; isspace(str[i]) || str[i]==','; i++);
   int numc = 0;
   if(sscanf(&str[i], fmt_string, retp, &numc) == 1){
      str = &str[i+numc];
      return true;
   }
   return false;
}

//----------------------------
// Read float value from property buffer, if available.
static bool GetPropertiesFloat(C_str &str, float &ret){
   return GetPropertiesValue(str, "%f%n", &ret);
}

//----------------------------
// Read int value from property buffer, if available.
static bool GetPropertiesInt(C_str &str, int &ret){
   return GetPropertiesValue(str, "%i%n", &ret);
}

//----------------------------
// Read string value from property buffer, if available.
static bool GetPropertiesString(C_str &str, C_str &ret){

   char buf[256];
   if(str[0]=='\"'){
                              //read quoted string
      str = &str[1];
      if(!GetPropertiesValue(str, "%256[^\"]\"%n", buf))
         return false;
   }else
   if(!GetPropertiesValue(str, "%256s%n", buf))
      return false;
   ret = buf;
   return true;
}

//----------------------------
//----------------------------

I3D_RESULT C_loader::ShowProgress(){

   if((load_flags&I3DLOAD_PROGRESS) && load_cb_proc){
      //driver->GetGraphInterface()->ProcessWinMessages(true);
      float f = (float)ck.GetHandle()->tellg() / (float)file_length;
      if(num_mats){
         f *= .5f;
         f += ((float)materials.size() / (float)num_mats) * .5f;
      }
      bool cancelled = load_cb_proc(CBM_PROGRESS, I3DFloatAsInt(f), 1, load_cb_context);
      if(cancelled)
         return I3DERR_CANCELED;
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT C_loader::ReadMaterial(){

   byte self_illum = 0;
   C_str txt_proc_name;
   S_mat_src diff, opt, env, embm, det, normalmap, bump_level, specular, secondmap;

   dword txt_flags = 0;
   bool keep_diffuse = false;
   C_smart_ptr<I3D_material> mat = driver->CreateMaterial(); mat->Release();
   dword ct_flags = 0;
#ifdef FORCE_MIPMAPPING
   ct_flags |= TEXTMAP_MIPMAP;
#endif

   C_str mat_name;
   while(ck){
      CK_TYPE ct = ++ck;
      switch(ct){
      case CT_MAT_NAME:
         {
            mat_name = ck.RStringChunk();
            mat->SetName1(mat_name);
                              //detect material behavior from name
            char c;
            for(dword i = 0; c=mat_name[i++], c; ){
               switch(c){
               case '!':
                  txt_flags |= LOADF_TXT_TRANSP;
                  if(mat_name[i]=='!'){
                     mat->SetMatGlags(mat->GetMatGlags() | MATF_CKEY_ZERO_REF);
                     ++i;
                  }
                  break;

               case '/':
                  {
                     dword id = 0;
                     sscanf(&mat_name[i], "%i", &id);
                     mat->SetMirrorID(id);
                  }
                  break;

               case '#':      //unusable - MAX uses this
               case '-':
               case '^':      //ignored duplication
                  break;

               case '%':
                                       //read environment effect type
                  {
                     switch(mat_name[i]){
                     case '%':
                        ++i;
                        txt_flags |= LOADF_TXT_CUBEMAP;
                        break;
                     }
                  }
                  break;

               case '=':
                  mat->SetAddMode(true);
                  break;

               case '@':               //keep diffuse color
                  keep_diffuse = true;
                  break;

               default:
                  if(!(isalnum(c) || c=='_' || c==' '))
                     REPORT_ERR(C_fstr("Invalid material name: '%s'", (const char*)mat_name));
               }
            }
         }
         break;

      case CT_MAT_AMBIENT:
      case CT_MAT_DIFFUSE:
      case CT_MAT_SPECULAR:
         {
            C_rgb rgb;
            ck.Read(&rgb, sizeof(C_rgb));
            --ck;
            const S_vector c = rgb.ToVector();
            switch(ct){
            case CT_MAT_AMBIENT: mat->SetAmbient(c); break;
            case CT_MAT_DIFFUSE: mat->SetDiffuse(c); break;
            //case CT_MAT_SPECULAR: mat->SetSpecularColor(c); break;
            }
         }
         break;

      case CT_MAT_TRANSPARENCY:
         {
            dword alpha_percentage = 100 - ck.RByteChunk();
            if(!alpha_percentage){
                              //zero opacity, issue warning
               REPORT_ERR(C_fstr("Material '%s': zero opacity", (const char*)mat_name));
            }
            mat->SetAlpha((float)alpha_percentage / 100.0f);
         }
         break;

      case CT_MAT_SELF_ILPCT:
         self_illum = ck.RByteChunk();
         break;

      case CT_MAT_TWO_SIDE:
         mat->Set2Sided(true);
         --ck;
         break;

      case CT_MAP_SPEC_LEVEL:
      case CT_MAT_SHADING:
      case CT_MAP_REFRACT:
         --ck;
         break;

      case CT_MAP_DIFFUSE:
      case CT_MAP_OPACITY:
      case CT_MAP_BUMP:
      case CT_MAP_REFLECTION:
      case CT_MAP_DETAIL:
      case CT_MAP_DISPLACE:
      case CT_MAP_SPECULAR:
      case CT_MAP_AMBIENT:
      case CT_MAP_SHINNINESS:
         {
            CK_TYPE ct_map = ct;
            S_mat_src tmp;
            S_mat_src *map_src = NULL;
            switch(ct_map){
            case CT_MAP_DIFFUSE: map_src = &diff; break;
            case CT_MAP_OPACITY: map_src = &opt; break;
            case CT_MAP_BUMP: map_src = &embm; break;
            case CT_MAP_REFLECTION: map_src = &env; break;
            case CT_MAP_DETAIL: map_src = &det; break;
            case CT_MAP_AMBIENT: map_src = &secondmap; break;
            //case CT_MAP_SPECULAR: map_src = &specular; break;
            case CT_MAP_SPECULAR: map_src = &normalmap; break;
            case CT_MAP_DISPLACE: map_src = &tmp; break;
            case CT_MAP_SHINNINESS: map_src = &bump_level; break;
            default: assert(0);
            }
            while(ck){
               ct = ++ck;
               switch(ct){
               case CT_PERCENTAGE:
                  {
                     word percentage = ck.RWordChunk();
                     map_src->power = (percentage) * .01f;
                  }
                  break;
               case CT_MAP_NAME:
                  *(C_str*)map_src = ck.RStringChunk();
                  break;
               case CT_MAP_UVOFFSET:
                  --ck;
                  break;
               case CT_MAP_UVSCALE:
                  {
                     ck.Read(&map_src->uv_scale, sizeof(float)*2);
                     --ck;
                  }
                  break;

               case CT_MAP_TRUECOLOR: map_src->flags |= MAT_SRC_TRUECOLOR; --ck; break;
               case CT_MAP_ANIMATED:
                  map_src->flags |= MAT_SRC_ANIMATED;
                  map_src->anim_speed = ck.RIntChunk();
                  break;
               case CT_MAP_NOMIPMAP: map_src->flags |= MAT_SRC_NOMIPMAP; --ck; break;
               case CT_MAP_NOCOMPRESS: map_src->flags |= MAT_SRC_NOCOMPRESS; --ck; break;
               case CT_MAP_PROCEDURAL: map_src->flags |= MAT_SRC_PROCEDURAL; --ck; break;

               default:
                  REPORT_ERR(C_fstr("Unknown chunk: 0x%.4x", ct));
                  --ck;
               }
            }
            switch(ct_map){
            case CT_MAP_DISPLACE:
               {
                              //store map for later use
                  blend_maps[mat_name] = tmp;
               }
               break;
            }
            --ck;
         }
         break;

      default:
         REPORT_ERR(C_fstr("Unknown chunk: 0x%.4x", ct));
         --ck;
      }
   }
#ifndef GL
                              //use compression
   if(driver->GetState(RS_TEXTURECOMPRESS))
      ct_flags |= TEXTMAP_COMPRESS;
#endif

   if(diff.Size()){
      if(!keep_diffuse){
                              //set diffuse color to white
         S_vector col = mat->GetDiffuse();
         S_vector up = S_vector(1, 1, 1) - col;
         mat->SetDiffuse(col+up * diff.power);
         mat->SetAmbient(S_vector(1.0f, 1.0f, 1.0f));
      }
   }
   if(self_illum!=0.0f)
      mat->SetEmissive(mat->GetDiffuse() * ((float)self_illum * .01f));

   materials.push_back(mat);
   I3D_RESULT ir = SetupMaterial(mat, ct_flags, txt_flags, diff, opt, env, txt_proc_name, embm, det,
      normalmap, bump_level, specular, secondmap, mat_name);
   if(I3D_SUCCESS(ir))
      ir = ShowProgress();

   return ir;
}

//----------------------------

I3D_RESULT C_loader::CreateTexture(dword ct_flags, const S_mat_src &mat_src, const char *mat_name,
   PI3D_material mat, I3D_MATERIAL_TEXTURE_INDEX mti, const C_str *opt){

   I3D_RESULT ir;
   PI3D_texture_base tp = NULL;

   I3D_CREATETEXTURE ct;
   memset(&ct, 0, sizeof(ct));
   ct.flags = ct_flags;
   if(mat_src.flags&MAT_SRC_PROCEDURAL){
      ct.file_name = mat_src;
      ct.proc_data = mti;
      ct.flags |= TEXTMAP_PROCEDURAL;
   }else{
      if(mat_src.Size()){
         ct.file_name = mat_src;
         ct.flags |= TEXTMAP_DIFFUSE;
      }
      if(opt && opt->Size()){
         ct.opt_name = *opt;
         ct.flags |= TEXTMAP_OPACITY;
      }
   }
   if(mat_src.flags&MAT_SRC_TRUECOLOR)
      ct.flags |= TEXTMAP_TRUECOLOR;
#ifndef GL
   if(mat_src.flags&MAT_SRC_NOCOMPRESS)
      ct.flags &= ~TEXTMAP_COMPRESS;
#endif
   if(mat_src.flags&MAT_SRC_NOMIPMAP){
      ct.flags &= ~TEXTMAP_MIPMAP;
      ct.flags |= TEXTMAP_NOMIPMAP;
   }
   C_str err_msg;

   C_str tmp_name;
   if(mat_src.flags&MAT_SRC_ANIMATED){
      const C_str &tn = mat_src;
                                 //find extension index
      for(dword ei=tn.Size(); ei--; ){
         if(tn[ei] == '.')
            break;
      }
                                 //no extension? ok, we'll use entire filename
      if(ei==-1)
         ei = tn.Size();
                                 //find digit index
      for(dword di=ei; di-- && isdigit(tn[di]); );
      ++di;
      ir = I3DERR_GENERIC;

      if(di==ei){
                                 //error - no digit found in filename
         err_msg = C_fstr("animated texture '%s' - no digits found at end of filename", mat_src);
      }else{
         const C_str &o_tn = opt ? *opt : NULL;
                                    //find extension index
         for(dword o_ei=o_tn.Size(); o_ei--; ){
            if(o_tn[o_ei] == '.')
               break;
         }
                                    //no extension? ok, we'll use entire filename
         if(o_ei==-1)
            o_ei = o_tn.Size();
                                    //find digit index
         for(dword o_di=o_ei; o_di-- && isdigit(o_tn[o_di]); );
         ++o_di;
         int max_num;
         int i = sscanf(&tn[di], (const char*)C_fstr("%%%iu", ei - di), &max_num);
         if(i==1 && max_num!=0){
                                 //digit scanned successfully

            C_vector<PI3D_texture> txts;
            txts.reserve(max_num);
            I3D_animated_texture *atp = atp = new I3D_animated_texture(driver);
            atp->SetAnimSpeed(mat_src.anim_speed);

                                 //load texture sequence
            tmp_name = tn;
            C_str o_new_name = o_tn;
            for(i=0; i<max_num; i++){
               sprintf(&tmp_name[di], C_fstr("%%.%ii", ei - di), i);
               tmp_name[ei] = tn[ei];

               ct.file_name = tmp_name;

                                 //make (optionally) opacity name
               if((ct.flags&TEXTMAP_OPACITY)){
                  if(o_ei != o_di){
                     sprintf(&o_new_name[o_di], C_fstr("%%.%ii", o_ei - o_di), i);
                     o_new_name[o_ei] = o_tn[o_ei];
                     ct.opt_name = o_new_name;
                  }else
                     ct.opt_name = o_tn;
               }
               PI3D_texture tp;
               ir = driver->CreateTexture1(ct, &tp, (load_flags&I3DLOAD_ERRORS) ? load_cb_proc : NULL, load_cb_context, &err_msg);
               if(I3D_FAIL(ir)){
                  //ct.file_name = mat_src;
                  break;
               }
               txts.push_back(tp);
            }
            atp->SetTextures(&txts.front(), txts.size());
            for(i=txts.size(); i--; ){
               txts[i]->Release();
            }
            tp = atp;
         }else{
            tp = NULL;
         }
      }
   }else{
      ir = driver->CreateTexture1(ct, (PI3D_texture*)&tp, (load_flags&I3DLOAD_ERRORS) ? load_cb_proc : NULL, load_cb_context, &err_msg);
   }
   if(I3D_FAIL(ir)){
      const char *txt_name = ct.file_name;
      if(ir==I3DERR_NOFILE1)
         txt_name = ct.opt_name;
      REPORT_ERR(C_fstr("Cannot load texture '%s' (%s) (mat '%s')",
         txt_name,
         (const char*)err_msg,
         mat_name));
   }
   if(tp){
      mat->SetTexture(mti, tp);
      tp->Release();
   }
   return ir;
}

//----------------------------

I3D_RESULT C_loader::SetupMaterial(PI3D_material mat, dword ct_flags, dword txt_flags,
   const S_mat_src &diff, const S_mat_src &opt, const S_mat_src &env, const C_str &proc, const S_mat_src &embm,
   const S_mat_src &detail, const S_mat_src &normalmap, const S_mat_src &bump_level, const S_mat_src &specular,
   const S_mat_src &secondmap, const char *mat_name){

   const S_mat_src *diff_opt = &opt;
   const S_mat_src *embm_opt = NULL;
   if(embm.Size()){
      diff_opt = NULL;
      if((driver->GetCaps()->TextureOpCaps&D3DTOP_BUMPENVMAPLUMINANCE) && opt.Size())
         embm_opt = &opt;
   }

   if(diff.Size() || (diff_opt && diff_opt->Size())){
      dword ctf = ct_flags;
      if(txt_flags&LOADF_TXT_TRANSP)
         ctf |= TEXTMAP_TRANSP;
      //if(diff_opt && diff_opt->Size())
         //ctf &= ~TEXTMAP_COMPRESS;
      if(diff_opt && (const C_str&)diff==*diff_opt){
         REPORT_ERR(C_fstr("Diffuse and opacity use same texture, use '=' for ADD mode! (mat '%s')", mat_name));
      }
      CreateTexture(ctf, diff, mat_name, mat, MTI_DIFFUSE, diff_opt);
   }
   if(env.Size()){
      dword ctf = ct_flags;
      if((txt_flags&LOADF_TXT_CUBEMAP) && (driver->GetCaps()->TextureCaps&D3DPTEXTURECAPS_CUBEMAP))
         ctf |= TEXTMAP_CUBEMAP;
      CreateTexture(ctf, env, mat_name, mat, MTI_ENVIRONMENT);
   }
   if(embm.Size()){
                              //check conflict with alpha channel
      if(mat->IsCkeyAlpha1() || mat->IsTextureAlpha()){
         REPORT_ERR(C_fstr("Bumpmap conflict with texture alpha (mat '%s')", mat_name));
      }else
         /*
      if(!env.Size()){
         REPORT_ERR(C_fstr("bumpmap without environment map is meaningless (mat '%s')", mat_name));
      }else
      */
      if(driver->GetFlags()&DRVF_EMBMMAPPING){
         dword ctf = ct_flags;
         if(embm_opt)
            ctf &= ~TEXTMAP_MIPMAP;
                              //load embm map
         CreateTexture(ctf | TEXTMAP_EMBMMAP, embm, mat_name, mat, MTI_EMBM, embm_opt);
         mat->SetEMBMOpacity(embm.power);
         if(embm.uv_scale[0]!=embm.uv_scale[1])
            REPORT_ERR(C_fstr("UV scale mismatch for bumpmap (u=%.2f, v=%.2f) (mat '%s')", embm.uv_scale[0], embm.uv_scale[1], mat_name));
         mat->SetEMBMScale(embm.uv_scale[0]);
      }
   }
   if(normalmap.Size()){
                              //load normal map
      //if(driver->GetFlags()&DRVF_EMBMMAPPING)
      {
         dword ctf = ct_flags | TEXTMAP_NORMALMAP;
#ifndef GL
         ctf &= ~TEXTMAP_COMPRESS;
#endif
         CreateTexture(ctf, normalmap, mat_name, mat, MTI_NORMAL, &bump_level);
      }
   }
   if(secondmap.Size()){
                              //load secondary map
      CreateTexture(ct_flags, secondmap, mat_name, mat, MTI_SECONDARY, &specular);
   }
   if(detail.Size()){
      bool ok = true;
      /*
      if(detail.uv_scale.x < 5.0f || detail.uv_scale.x > 50.0f || detail.uv_scale.y < 5.0f || detail.uv_scale.y > 50.0f){
         ok = false;
         REPORT_ERR(C_fstr("Detailmap invalid scale [%.2f, %.2f] (allowed range is 5 ... 50) (mat '%s')",
            detail.uv_scale.x, detail.uv_scale.y, mat_name));
      }
      */
      if(ok){
                              //setup material's detail map
         dword ctf = ct_flags;
         //ctf &= ~TEXTMAP_COMPRESS;
#ifndef GL
         ctf |= TEXTMAP_COMPRESS;
#endif
         //ctf |= TEXTMAP_TRUECOLOR;
         CreateTexture(ctf, detail, mat_name, mat, MTI_DETAIL);
         mat->SetDetailScale(detail.uv_scale);
         /*
                              //finally, check aspect ratio
         PI3D_texture_base tdiff = mat->GetTexture1(MTI_DIFFUSE);
         PI3D_texture_base tdet = mat->GetTexture1(MTI_DETAIL);
         if(tdiff && tdet){
            float sx = detail.uv_scale.x * tdet->SizeX() / tdiff->SizeX();
            float sy = detail.uv_scale.y * tdet->SizeY() / tdiff->SizeY();

            float ratio = Max(sx, sy) / Min(sx, sy);

            if(sx < 5.0f || sx > 200.0f || sy < 5.0f || sy > 200.0f){
               REPORT_ERR(C_fstr("Detailmap invalid pixel scale [%.2f, %.2f] (allowed range is 5 ... 200) (mat '%s')", sx, sy, mat_name));
               mat->SetTexture(MTI_DETAIL, NULL);
            }
            if(ratio > 2.001f){
               REPORT_ERR(C_fstr("Detailmap invalid pixel ratio %.1f : 1 (allowed distortion is 2 : 1) (mat '%s')", ratio, mat_name));
               mat->SetTexture(MTI_DETAIL, NULL);
            }
         }
         */
      }
   }
   return I3D_OK;
}

//----------------------------

bool C_loader::ReadTrackPos(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp){

   S_I3D_track_header header;
   header.num_keys = 0;
   ck.Read(&header, sizeof(header));

   I3D_anim_pos_tcb *akeys = new I3D_anim_pos_tcb[header.num_keys];
   S_I3D_position_key key;
   key.header.time = 0;

   for(dword i=0; i<header.num_keys; i++){
      key.Read(ck.GetHandle());

      I3D_anim_pos_tcb &ak = akeys[i];
      ak.time = key.header.time*1000/ANIM_FPS;
      ak.v = key.data;
      ak.tension = key.add_data.tension;
      ak.continuity = key.add_data.continuity;
      ak.bias = key.add_data.bias;
      ak.easy_from = key.add_data.easy_from;
      ak.easy_to = key.add_data.easy_to;
      if(!i && frm)
         frm->SetPos(key.data);
   }
   if(header.num_keys>1){
      if(!*animp){
         *animp = (PI3D_keyframe_anim)driver->CreateAnimation(I3DANIM_KEYFRAME);
         (*animp)->SetEndTime(anim_length);
      }
      dword track_time = key.header.time * 1000 / ANIM_FPS;
      if(track_time > anim_length)
         REPORT_ERR(C_fstr("Frame '%s': position track keys beyond time segment", frm_name));
      (*animp)->SetPositionKeys(akeys, header.num_keys);
#if defined _DEBUG && 0
      {
         const I3D_anim_pos_bezier *keys;
         dword nk;
         (*animp)->GetPositionKeys(&keys, &nk);
         for(dword i=0; i<nk; i++){
            dword ni = (i+1)%nk;
            driver->DebugLine(keys[i].v, keys[ni].v, 0, S_vector(1, 0, 0));
            driver->DebugLine(keys[i].v, keys[i].out_tan, 0, S_vector(0, 0, 1));
            driver->DebugLine(keys[i].out_tan, keys[ni].in_tan, 0, S_vector(0, 0, 1));
            driver->DebugLine(keys[ni].in_tan, keys[ni].v, 0, S_vector(0, 0, 1));
         }
      }
#endif
   }
   delete[] akeys;
   return true;
}

//----------------------------

bool C_loader::ReadTrackRot(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp){

   S_I3D_track_header header;
   header.num_keys = 0;
   ck.Read(&header, sizeof(header));

   I3D_anim_rot *akeys = new I3D_anim_rot[header.num_keys];

   S_vector axis;
   S_I3D_rotation_key key;
   key.header.time = 0;

   for(dword i=0; i<header.num_keys; i++){
      key.Read(ck.GetHandle());
      axis = S_vector(-key.data.axis.x, -key.data.axis.z, -key.data.axis.y);

      I3D_anim_rot &ak = akeys[i];
      ak.time = (key.header.time)*1000/ANIM_FPS;
      ak.axis = axis;
      ak.angle = key.data.angle;
      ak.easy_from = key.add_data.easy_from;
      ak.easy_to = key.add_data.easy_to;
      ak.smoothness = Min(1.0f, key.add_data.continuity + 1.0f);
      if(!i && frm){
         S_quat q;
         q.Make(axis, key.data.angle);
         frm->SetRot(q);
      }
   }
   if(header.num_keys>1){
      if(!*animp){
         *animp = (PI3D_keyframe_anim)driver->CreateAnimation(I3DANIM_KEYFRAME);
         (*animp)->SetEndTime(anim_length);
      }
      dword track_time = key.header.time * 1000 / ANIM_FPS;
      if(track_time > anim_length)
         REPORT_ERR(C_fstr("Frame '%s': rotation track keys beyond time segment", frm_name));
      (*animp)->SetRotationKeys(akeys, header.num_keys);
#if defined _DEBUG && 0
      {
         const I3D_anim_quat_bezier *keys;
         dword nk;
         (*animp)->GetRotationKeys(&keys, &nk);
         const S_vector &wp = frm->GetWorldPos();
         for(dword i=0; i<nk; i++){
            driver->DebugLine(wp, wp + S_matrix(keys[i].q)(2) * 5, 0, S_vector(1, 0, 0));
            driver->DebugLine(wp, wp + S_matrix(keys[i].out_tan)(2) * 5, 0, S_vector(0, 0, 1));
            driver->DebugLine(wp, wp + S_matrix(keys[i].in_tan)(2) * 5, 0, S_vector(0, 0, 1));
         }
      }
#endif
   }
   delete[] akeys;
   return true;
}

//----------------------------

inline bool IsUniformScale(const S_vector &scale){
   return ((I3DFabs(scale.x-scale.y) + I3DFabs(scale.y-scale.z)) < 1e-4f);
}

//----------------------------

bool C_loader::ReadTrackScl(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp, S_vector &nu_scale){

   S_I3D_track_header header;
   header.num_keys = 0;
   ck.Read(&header, sizeof(header));

   typedef I3D_anim_vector_tcb I3D_anim_scale_tcb;
   I3D_anim_scale_tcb *skeys = new I3D_anim_scale_tcb[header.num_keys];

   S_I3D_scale_key key;
   key.header.time = 0;
   bool scale_err_reported = false;

   bool is_uni_scale = true;
   S_vector base_scale(0, 0, 0);

   for(dword i=0; i<header.num_keys; i++){
      key.Read(ck.GetHandle());

      I3D_anim_scale_tcb &ak = skeys[i];
      ak.time = (key.header.time)*1000/ANIM_FPS;
      ak.v = key.data;
      ak.tension = key.add_data.tension;
      ak.continuity = key.add_data.continuity;
      ak.bias = key.add_data.bias;
      ak.easy_from = key.add_data.easy_from;
      ak.easy_to = key.add_data.easy_to;
                              //negative scale check
      if(key.data.x<0.0f || key.data.y<0.0f || key.data.z<0.0f){
         if(!scale_err_reported){
            scale_err_reported = true;
            REPORT_ERR(C_fstr("Frame '%s': negative scale", frm_name));
         }
      }
      is_uni_scale = (is_uni_scale && IsUniformScale(key.data));
      if(!i)
         base_scale = key.data;
   }
   if(frm){
      if(is_uni_scale){
         frm->SetScale(base_scale.x);
      }else{
         float avg_scale = (I3DFabs(base_scale.x) + I3DFabs(base_scale.y) + I3DFabs(base_scale.z)) / 3.0f;
         frm->SetScale(avg_scale);
         nu_scale = base_scale / avg_scale;
      }
   }
   if(header.num_keys>1){
      if(!*animp){
         *animp = (PI3D_keyframe_anim)driver->CreateAnimation(I3DANIM_KEYFRAME);
         (*animp)->SetEndTime(anim_length);
      }
      dword track_time = key.header.time * 1000 / ANIM_FPS;
      if(track_time > anim_length)
         REPORT_ERR(C_fstr("Frame '%s': scale track keys beyond time segment", frm_name));

      if(!is_uni_scale){
                                 //set scale keys
         //lc.recent_anim->SetScaleKeys(skeys, num_keys);
      }else{
                                 //set power keys
         I3D_anim_power *pkeys = (I3D_anim_power*)alloca(sizeof(I3D_anim_power)*header.num_keys);
         for(i=0; i<header.num_keys; i++){
            pkeys[i].time = skeys[i].time;
            pkeys[i].power = skeys[i].v.x;
         }
         (*animp)->SetPowerKeys(pkeys, header.num_keys);
      }
   }
   delete[] skeys;
   return true;
}

//----------------------------

bool C_loader::ReadTrackVis(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp){

   S_I3D_track_header header;
   header.num_keys = 0;
   ck.Read(&header, sizeof(header));

   if(header.num_keys){
      S_I3D_visibility_key *keys = new S_I3D_visibility_key[header.num_keys];
      ck.Read(keys, sizeof(S_I3D_visibility_key)*header.num_keys);

      I3D_anim_visibility *vkeys = new I3D_anim_visibility[header.num_keys + 1];
      memset(vkeys, 0, sizeof(I3D_anim_visibility)*(header.num_keys+1));

      int j = 0;
      //bool on = true;
      if(keys[0].time!=0){
                              //put 'on' key on position 0
         vkeys[0].time = 0;
         vkeys[0].power = keys[0].visibility;
         ++j;
      }else
      if(frm && (keys[0].visibility < .5f)){
         frm->SetOn(false);
      }
      if(j || header.num_keys>1){
         for(dword i=0; i<header.num_keys; i++, j++){
            vkeys[j].time = keys[i].time * 1000/ANIM_FPS;
            vkeys[j].power = keys[i].visibility;
         }
         if(!*animp){
            *animp = (PI3D_keyframe_anim)driver->CreateAnimation(I3DANIM_KEYFRAME);
            (*animp)->SetEndTime(anim_length);
         }
         dword track_time = keys[header.num_keys-1].time * 1000 / ANIM_FPS;
         if(track_time > anim_length)
            REPORT_ERR(C_fstr("Frame '%s': visibility track keys beyond time segment", frm_name));
         (*animp)->SetVisibilityKeys(vkeys, j);
      }
      delete[] vkeys;
      delete[] keys;
   }
   return true;
}

//----------------------------
// Convert binary buffer to text format - remove '\r' characters from buffer.
// Return size of new buffer.
static dword dtaBin2Text(void *mem, dword len){

   char *src=(char*)mem, *dst=(char*)mem;
   if(len)
   do{
      if(*src!='\r') *dst++=*src;
   }while(++src, --len);
   return dst - (char*)mem;
}

//----------------------------

bool C_loader::ReadTrackNote(PI3D_frame frm, const char *frm_name, PI3D_keyframe_anim *animp){

                              //read # of note tracks
   /*
   dword num_tracks = 0;
   ck.GetHandle()->read(&num_tracks, sizeof(num_tracks));
   if(!num_tracks)
      return true;
   dword num_keys = 0;
   ck.GetHandle()->read(&num_keys, sizeof(num_keys));
   if(!num_keys)
      return true;
      */
   //dword num_tracks =
      ck.ReadDword();
   dword num_keys = ck.ReadDword();

   C_vector<C_str> str_list(num_keys);
   C_vector<I3D_anim_note> note_list(num_keys);

   for(dword i=0; i<num_keys; i++){
                              //read time
      //int time;
      //ck.GetHandle()->read((char*)&time, sizeof(time));
      int time = ck.ReadDword();
      time *= 1000/ANIM_FPS;
                              //read note
      str_list[i] = ck.ReadString();
      dtaBin2Text(&str_list[i][0], str_list[i].Size());

      I3D_anim_note &note = note_list[i];
      note.time = time;
      note.note = str_list[i];
   }
   if(!*animp){
      *animp = (PI3D_keyframe_anim)driver->CreateAnimation(I3DANIM_KEYFRAME);
      (*animp)->SetEndTime(anim_length);
   }
   dword track_time = note_list[num_keys-1].time;
   if(track_time > anim_length)
      REPORT_ERR(C_fstr("Frame '%s': note track keys beyond time segment", frm_name));
   (*animp)->SetNoteKeys(&note_list.front(), note_list.size());

   return true;
}

//----------------------------

void C_loader::CleanMesh(PI3D_frame frm, C_vector<S_vector> &verts, C_vector<I3D_triface> &faces,
   C_vector<I3D_triface> uv_faces[MAX_UV_CHANNELS], C_vector<byte> &smooth_groups,
   C_vector<I3D_face_group> &face_groups, const char *name, bool &allow_cache){

   dword i, j;

   if(uv_faces[0].size() && uv_faces[0].size()!=faces.size()){
      REPORT_ERR(C_fstr("Frame '%s': invalid UV mapping", name));
      allow_cache = false;
      return;
   }
   assert(!uv_faces[1].size() || (uv_faces[1].size() == uv_faces[0].size()));

                              //mark used vertices
   C_vector<bool> v_use(verts.size(), false);

   int num_faces_out = 0;
                              //clean invalid faces
   for(i=faces.size(); i--; ){
      const I3D_triface &fc = faces[i];
                              //thresh = minimal distance each vertex in triangle must have from adjacent edge
      const float thresh = 5e-4f;

      float d = verts[fc[0]].DistanceToLine(verts[fc[1]], verts[fc[2]] - verts[fc[1]]);
      if(d >= thresh){
         float d1 = verts[fc[1]].DistanceToLine(verts[fc[2]], verts[fc[0]] - verts[fc[2]]);
         d = Min(d, d1);
      }
      if(d >= thresh){
         float d1 = verts[fc[2]].DistanceToLine(verts[fc[0]], verts[fc[1]] - verts[fc[0]]);
         d = Min(d, d1);
      }
      float valid = (d >= thresh);
      if(valid){
                              //check near-degenerate triangles
         float f0 = (verts[fc[0]] - verts[fc[1]]).Square();
         float f1 = (verts[fc[1]] - verts[fc[2]]).Square();
         float f2 = (verts[fc[2]] - verts[fc[0]]).Square();
         float max_side_2 = Max(f0, Max(f1, f2));
         float max_side = I3DSqrt(max_side_2);
         float ratio = d / max_side;
         valid = (ratio > .001f);
      }

      if(!valid){
#ifdef REPORT_INVALID_FACES
         {
            const I3D_triface &fc = faces[i];

            t_visual_err_report::iterator it = visual_err_report.find(frm);
            if(it==visual_err_report.end())
               it = visual_err_report.insert(pair<C_smart_ptr<I3D_frame>, C_vector<S_triangle> >(frm, C_vector<S_triangle>())).first;

            (*it).second.push_back(S_triangle());
            S_triangle &t = (*it).second.back();
            t.v[0] = verts[fc[0]];
            t.v[1] = verts[fc[1]];
            t.v[2] = verts[fc[2]];
         }
#endif
         faces.erase(faces.begin()+i);

         for(j=MAX_UV_CHANNELS; j--; ){
            if(uv_faces[j].size())
               uv_faces[j].erase(uv_faces[j].begin() + i);
         }
         if(smooth_groups.size())
            smooth_groups.erase(smooth_groups.begin() + i);

         for(int j=face_groups.size(); j--; ){
            I3D_face_group &fg = face_groups[j];
            if(fg.base_index>i){
                              //move group down
               --fg.base_index;
            }else
            if(fg.base_index+fg.num_faces > i){
                              //remove vertex from group
               if(!--fg.num_faces){
                              //fgroup empty, erase (note: can't change order of fgroups!)
                  face_groups.erase(face_groups.begin()+j);
               }
            }
         }
         ++num_faces_out;
      }else{
         v_use[fc[0]] = true;
         v_use[fc[1]] = true;
         v_use[fc[2]] = true;
      }
   }
#ifdef REPORT_INVALID_FACES
   if(num_faces_out){
      REPORT_ERR(C_fstr("Mesh '%s': %i invalid face(s)", name, num_faces_out));
      allow_cache = false;
   }
#endif

   C_vector<word> v_map(verts.size());
   dword new_vertex_count = verts.size();
                              //process all verts, remove unused
   for(i=0; i<new_vertex_count; ){
      v_map[i] = word(i);           //by default vertex stays where it is
      if(!v_use[i]){
                              //vertex from back is moved forward
         --new_vertex_count;
         v_map[new_vertex_count] = word(i);
         verts[i] = verts[new_vertex_count];
         v_use[i] = v_use[new_vertex_count];
      }else
         ++i;
   }

   if(new_vertex_count != verts.size()){
                              //some vertex was removed
#ifdef REPORT_UNUSED_VERTS
      lc.allow_cache = false;
      REPORT_ERR(C_fstr("Mesh '%s': %i unused vertices",
         (const char*)recent_string, verts.size() - new_vertex_count));
#endif
                              //strip verts
      //verts.erase(verts.begin() + new_vertex_count, verts.end());
      verts.resize(new_vertex_count);
                              //remap faces
      for(i=faces.size(); i--; ){
         for(int j=3; j--; )
            faces[i][j] = v_map[faces[i][j]];
      }
   }
}

//----------------------------

bool C_loader::AssignUVCoordinates(C_vector<word> &in_vertex_map, C_vector<S_vertex> &in_verts, C_vector<I3D_triface> &faces,
   const C_vector<I3D_text_coor> uv_verts[], const C_vector<I3D_triface> uv_faces[], const char *err_name) const{

   S_vertex *verts = &in_verts.front();
   dword num_verts = in_verts.size();
   word *vertex_map = &in_vertex_map.front();

   if(!uv_verts[0].size() || !uv_faces[0].size()){
                              //just zero-out uvs
      for(dword i=num_verts; i--; )
         verts[i].tex.Zero();
      return true;
   }

   if(faces.size() != uv_faces[0].size()){
      REPORT_ERR(C_fstr("Mesh '%s' uv mapping corrupted", err_name));
      for(dword i=num_verts; i--; )
         verts[i].tex.Zero();
      return false;
   }

   bool ok = true;

   dword num_orig_verts = num_verts;
   C_buffer<bool> buf_vertex_used(num_orig_verts, false);

   bool *vertex_used = buf_vertex_used.begin();

   int i, j;
   bool err_reported = false;

   for(i=faces.size(); i--; ){
      I3D_triface &fc = faces[i];
      const I3D_triface &uv_fc = uv_faces[0][i];
      for(j=3; j--; ){
         word vi = fc[j];
         word uv_vi = uv_fc[j];
         if(uv_vi >= uv_verts[0].size()){
            if(ok){
               REPORT_ERR(C_fstr("Mesh '%s' uv mapping corrupted", err_name));
               ok = false;
            }
            uv_vi = 0;
         }
         const I3D_text_coor &txt = uv_verts[0][uv_vi];
         if(!vertex_used[vi]){
            verts[vi].tex = txt;
            vertex_used[vi] = true;
         }else
         if(!AlmostEqual(verts[vi].tex, txt)){
                           //check if we may map it onto any other duplicated vertex
            for(dword ii=num_orig_verts; ii<num_verts; ii++){
               if(vertex_map[ii] == vi && AlmostEqual(verts[ii].tex, txt)){
                              //vertex found, use this
                  fc[j] = word(ii);
                  break;
               }
            }
            if(ii == num_verts){
               if(num_verts == MAX_VERTICES){
                  if(!err_reported){
                     REPORT_ERR(C_fstr("Mesh '%s': UV mapping failed, too many vertices in mesh", err_name));
                     err_reported = true;
                     ok = false;
                  }
               }else{
                              //must duplicate vertex
                  word wcopy = vertex_map[vi];
                  in_vertex_map.push_back(wcopy);
                  vertex_map = &in_vertex_map.front();
                  fc[j] = word(num_verts);
                           //duplicate vertex
                  S_vertex copy = verts[vi];
                  in_verts.push_back(copy);
                  verts = &in_verts.front();
                  verts[num_verts++].tex = txt;
               }
            }
         }
      }
   }
   return ok;
}

//----------------------------

bool C_loader::GenerateTextureSpace(const C_vector<S_vertex> &verts, const C_vector<I3D_triface> &faces,
   C_buffer<S_texture_space> &texture_space, const char *err_name, bool &allow_cache) const{

   texture_space.assign(verts.size(), S_texture_space());
   memset(&texture_space.front(), 0, texture_space.size()*sizeof(S_texture_space));

   for(dword i=faces.size(); i--; ){
      const I3D_triface &fc = faces[i];

      const S_vertex &v0 = verts[fc[0]];
      const S_vertex &v1 = verts[fc[1]];
      const S_vertex &v2 = verts[fc[2]];

      S_texture_space &st0 = texture_space[fc[0]];
      S_texture_space &st1 = texture_space[fc[1]];
      S_texture_space &st2 = texture_space[fc[2]];

      S_vector edge1, edge2;
      S_vector cp;

                              //x, s, t
      edge1 = S_vector(v1.xyz.x - v0.xyz.x, v1.tex.x - v0.tex.x, v1.tex.y - v0.tex.y );
      edge2 = S_vector(v2.xyz.x - v0.xyz.x, v2.tex.x - v0.tex.x, v2.tex.y - v0.tex.y );

      cp = edge1.Cross(edge2);
      if(!IsAbsMrgZero(cp.x)){
         float r_x = cp.x;
         float y = -cp.y * r_x;
         float z = -cp.z * r_x;
         st0.s.x += y;
         st0.t.x += z;
         
         st1.s.x += y;
         st1.t.x += z;
         
         st2.s.x += y;
         st2.t.x += z;
      }

                              //y, s, t
      edge1 = S_vector(v1.xyz.y - v0.xyz.y, v1.tex.x - v0.tex.x, v1.tex.y - v0.tex.y);
      edge2 = S_vector(v2.xyz.y - v0.xyz.y, v2.tex.x - v0.tex.x, v2.tex.y - v0.tex.y);
      cp = edge1.Cross(edge2);
      if(!IsAbsMrgZero(cp.x)){
         float r_x = cp.x;
         float y = -cp.y * r_x;
         float z = -cp.z * r_x;
         st0.s.y += y;
         st0.t.y += z;
         
         st1.s.y += y;
         st1.t.y += z;
         
         st2.s.y += y;
         st2.t.y += z;
      }

                              //z, s, t
      edge1 = S_vector(v1.xyz.z - v0.xyz.z, v1.tex.x - v0.tex.x, v1.tex.y - v0.tex.y);
      edge2 = S_vector(v2.xyz.z - v0.xyz.z, v2.tex.x - v0.tex.x, v2.tex.y - v0.tex.y);
      cp = edge1.Cross(edge2);
      if(!IsAbsMrgZero(cp.x)){
         float r_x = cp.x;
         float y = -cp.y * r_x;
         float z = -cp.z * r_x;
         st0.s.z += y;
         st0.t.z += z;
         
         st1.s.z += y;
         st1.t.z += z;
         
         st2.s.z += y;
         st2.t.z += z;
      }
      //driver->DebugLine(v1.xyz, v1.xyz + st1.s, 0);
      //driver->DebugLine(v1.xyz, v1.xyz + st1.t, 0);
   }
                              //normalize those normals, and create SxT
   for(dword vi=verts.size(); vi--; ){
      S_texture_space &ts = texture_space[vi];
      ts.s.Normalize();
      ts.t.Normalize();
      ts.sxt = ts.s.Cross(ts.t);
      ts.t.Invert();
      //assert(I3DFabs(1.0f - ts.sxt.Magnitude()) < 1e-2f);
#if 1
      {
         const S_vertex &v = verts[vi];
         driver->DebugVector(v.xyz, ts.s, 0, 0xffff0000);
         driver->DebugVector(v.xyz, ts.t, 0, 0xff00ff00);
         dword cn = 0xff0000ff;
         if(I3DFabs(1.0f - ts.sxt.Magnitude()) > 1e-2f)
            cn = 0xffffffff;
         driver->DebugVector(v.xyz, ts.sxt, 0, cn);
      }
#endif
   }

   return true;
}


//----------------------------

bool C_loader::UV_Check(C_vector<I3D_text_coor> &uv_verts, const char *err_name){

   float minu = 1e+6f, minv = 1e+6f, maxu = -1e+6f, maxv = -1e+6f;
   for(int i = uv_verts.size(); i--; ){
      const I3D_text_coor &tex = uv_verts[i];
      minu = Min(minu, tex.x);
      minv = Min(minv, tex.y);
      maxu = Max(maxu, tex.x);
      maxv = Max(maxv, tex.y);
   }
                              //get middle
   minu = (float)floor(minu);
   minv = (float)floor(minv);
   maxu = (float)ceil(maxu);
   maxv = (float)ceil(maxv);
                              //center around 0
   float midu = minu + (float)floor((maxu - minu) * .5f);
   float midv = minv + (float)floor((maxv - minv) * .5f);

   for(i = uv_verts.size(); i--; ){
      I3D_text_coor &tex = uv_verts[i];
      tex.x -= midu;
      tex.y -= midv;
   }

   maxu -= minu;
   maxv -= minv;

   if(maxu > 128.0f || maxv > 128.0f){
      REPORT_ERR(C_fstr("Mesh '%s': UV range overflow (U = %.1f, V = %.1f)", err_name, maxu, maxv));
      return false;
   }
   return true;
}

//----------------------------

void C_loader::MakeVertexNormals(int num_orig_verts, C_vector<word> &vertex_map,
   C_vector<S_vertex> &v_verts, C_vector<I3D_triface> &v_faces, const C_vector<byte> &v_smooth_groups) const{

   dword num_faces = v_faces.size();
   I3D_triface *faces = &v_faces.front();

   dword num_verts = v_verts.size();
   S_vertex *verts = &v_verts.front();

   assert(!v_smooth_groups.size() || v_smooth_groups.size()==num_faces);
   const byte *smooth_groups = v_smooth_groups.size() ? &v_smooth_groups.front() : NULL;

   S_vector *face_normals = new S_vector[num_faces];
   S_normals_vertex_info *vertex_info = new S_normals_vertex_info[num_verts];
   float *face_area = new float[num_faces];

   int max_faces_per_vertex = 0;
                              //generate face info
   for(int i=num_faces; i--; ){
      const I3D_triface &fc = faces[i];
                              //compute face normal
      S_vector &fn = face_normals[i];
      fn.GetNormal(verts[fc[0]].xyz, verts[fc[1]].xyz, verts[fc[2]].xyz);
      if(!fn.Normalize()){
         //assert(0);
                              //should not happen (only if CleanMesh failed)
         fn = S_vector(1, 0, 0);
      }

                              //compute face area
      face_area[i] = fc.ComputeSurfaceArea(&verts->xyz, sizeof(S_vertex));
      /*
      if(fn.Square() < .1f){
         REPORT_ERR(C_fstr("Mesh '%s': degenerate face #%i", "", i));
      }
      */
                              //assign face to vertex info of all 3 vertices
      for(dword j=3; j--; ){
         word v0 = fc[j];
         word v1 = fc[(j+1)%3];
         word v2 = fc[(j+2)%3];
         S_normals_vertex_info &vi = vertex_info[v0];

         const S_vector &v_pos = verts[v0].xyz;
         float angle = (verts[v1].xyz - v_pos).AngleTo(verts[v2].xyz - v_pos);

         vi.faces.push_back(S_normals_vertex_info::S_face(word(i), angle));
         max_faces_per_vertex = Max(max_faces_per_vertex, (int)vi.faces.size());
      }
   }
                              //alloc temp buffers used in the loop
   S_vector *vertex_normals = new S_vector[max_faces_per_vertex];
   word *face_vertex = new word[max_faces_per_vertex];

                              //process all vertices, generate normals, duplicate as necessary
   const float NORMAL_COLLAPSE_ANGLE_COS = (float)cos(PI*.05f);
   //const float NORMAL_AUTO_SMOOTH_ANGLE_COS = cos(AUTO_SMOOTH_ANGLE);

   for(i=num_verts; i--; ){
      const S_normals_vertex_info &vi = vertex_info[i];
      dword num_vertex_faces = vi.faces.size();
      if(!num_vertex_faces)
         continue;
      const S_normals_vertex_info::S_face *vertex_faces = &vi.faces.front();

                              //process all faces of vertex, 
      for(dword fii=num_vertex_faces; fii--; ){
         word face_index = vertex_faces[fii].index;
         const S_vector &fn = face_normals[face_index];
                              //by default, add normal of current face
         S_vector &n = vertex_normals[fii];
         n = fn * (vertex_faces[fii].angle * face_area[face_index]);
                              //add all normals of matching faces
         dword sg = smooth_groups ? smooth_groups[face_index] : 0;
         for(dword fii1=num_vertex_faces; fii1--; ){
            if(fii1==fii)
               continue;
            word face_index1 = vertex_faces[fii1].index;
            const S_vector &fn1 = face_normals[face_index1];
            bool match;
            if(smooth_groups){
                              //use matching by smooth-groups
               match = (smooth_groups[face_index1]&sg);
            }else{
                              //use matching by comparing faces' normals
               //match = (fn1.Dot(fn) > NORMAL_AUTO_SMOOTH_ANGLE_COS);
               match = true;
            }
            if(match)
               n += fn1 * (vertex_faces[fii1].angle * face_area[face_index1]);
         }
                              //normalize resulting normal
         n.Normalize();
      }
                              //assign 1st normal to vertex itself
      verts[i].normal = vertex_normals[0];
      face_vertex[0] = word(i);
                              //now duplicate vertices for each mismatching face's normal
      for(fii=1; fii<num_vertex_faces; fii++){
         const S_vector &n = vertex_normals[fii];
         word face_index = vertex_faces[fii].index;
         I3D_triface &fc = faces[face_index];

         word v = 0xffff;
                           //check if we can map it to other normal already assigned
         for(int j=fii; j--; ){
            float cos_angle = vertex_normals[j].Dot(n);
            if(cos_angle > NORMAL_COLLAPSE_ANGLE_COS){
               v = face_vertex[j];
               break;
            }
         }
         if(j==-1){
                              //can't map to existing, must duplicate vertex
            assert(v_verts.size() < MAX_VERTICES);

            v = word(v_verts.size());
            vertex_map.push_back(word(i));
            S_vertex copy = verts[i];
            v_verts.push_back(copy); verts = &v_verts.front();
            verts[v].normal = n;
         }
         fc.ReplaceIndex(word(i), v);
         face_vertex[fii] = v;
      }
   }

   delete[] vertex_normals;
   delete[] face_vertex;
   delete[] face_normals;
   delete[] vertex_info;
   delete[] face_area;

#if defined _DEBUG && 0
                              //validity check
   for(i=v_verts.size(); i--; ){
      const S_normal &n = v_verts[i].normal;
      float len = n.Magnitude();
      assert(IsAbsMrgZero(1.0f - len));
   }
#endif
}

//----------------------------

I3D_RESULT C_loader::ReadMesh(PI3D_frame frm, PI3D_mesh_base mesh, const char *name, bool &allow_cache){

   C_vector<S_vector> verts;
   C_vector<I3D_triface> faces;
   C_vector<I3D_face_group> fgroups;
   C_vector<byte> smooth_groups;
   C_vector<I3D_text_coor> uv_verts[MAX_UV_CHANNELS];
   C_vector<I3D_triface> uv_faces[MAX_UV_CHANNELS];

                              //multi-mat stats
   dword mat_num_opaque = 0;
   dword mat_num_transl = 0;
   dword mat_num_ckey = 0;
   dword mat_num_mirror = 0;
   bool diffuse_match = true;
   bool emissive_match = true;
   bool alpha_match = true;
   bool any_bump = false;
   S_vector2 last_detmap_uv_scale(-1e+16f, 0);

   while(ck){
      CK_TYPE ct = ++ck;
      switch(ct){
      case CT_POINT_ARRAY:
         {
                              //get # of vertices
            //word n = 0;
            //ck.Read((char*)&n, sizeof(word));
            word n = ck.ReadWord();
                              //reserve more in the array, due to UV duplication
            verts.reserve(n * 2);
            verts.assign(n);
            ck.Read(&verts.front(), sizeof(S_vector)*n);
            //if(load_flags&I3DLOAD_LOG) LOG(C_fstr("Num vertices: %i", n));
            --ck;
         }
         break;

      case CT_FACE_ARRAY:
         {
                              //read # of faces
            //word n = 0;
            //ck.Read(&n, sizeof(word));
            word n = ck.ReadWord();
            //if(load_flags&I3DLOAD_LOG) LOG(C_fstr("Num faces: %i", n));

            faces.assign(n);
            ck.Read(&faces.front(), sizeof(I3D_triface)*n);
#if defined _DEBUG && 0
            int nv = verts.size();
            for(int i=0; i<n; i++){
               I3D_triface &fc = faces[i];
               assert(fc[0] < nv);
               assert(fc[1] < nv);
               assert(fc[2] < nv);
            }
#endif
         }
         --ck;
         break;

      case CT_EDGE_VISIBILITY:
         --ck;
         break;

      case CT_MESH_FACE_GROUP:
         {
#pragma pack(push, 1)
            struct S_fg_header{
               word mat_id;
               word base_index;
               word num_faces;
            } hdr;
#pragma pack(pop)
            I3D_face_group fg;
            //word mat_id = 0xffff;
            //ck.Read(&mat_id, sizeof(word));
            //ck.Read(&fg.base_index, sizeof(word));
            //ck.Read(&fg.num_faces, sizeof(word));
            hdr.mat_id = 0xffff;
            ck.Read(&hdr, sizeof(hdr));
            fg.base_index = hdr.base_index;
            fg.num_faces = hdr.num_faces;
            if(hdr.mat_id==0xffff){
               REPORT_ERR(C_fstr("Mesh '%s' has default material", name));
               hdr.mat_id = word(materials.size());
               PI3D_material mat = driver->CreateMaterial();
               materials.push_back(mat);
               mat->Release();
               allow_cache = false;
            }
            if(hdr.mat_id<materials.size())
               fg.mat = materials[hdr.mat_id];
            fgroups.push_back(fg);

            if(fgroups.size() >= 2){
               PI3D_material mat0 = fgroups.front().mat;
               diffuse_match = (diffuse_match && (mat0->GetDiffuse()==fg.mat->GetDiffuse()));
               emissive_match = (emissive_match && (mat0->GetEmissive()==fg.mat->GetEmissive()));
               alpha_match = (alpha_match && (mat0->GetAlpha()==fg.mat->GetAlpha()));
            }

            if(!fg.mat->IsTransl())
               ++mat_num_opaque;
            else
            if(fg.mat->IsCkeyAlpha1())
               ++mat_num_ckey;
            else
            if(fg.mat->GetMirrorID1() != -1)
               ++mat_num_mirror;
            else
               ++mat_num_transl;

            any_bump = (any_bump || fg.mat->GetTexture1(MTI_NORMAL));

            if(fg.mat->GetTexture1(MTI_DETAIL)){
                              //check for detmap uv mishmatch (on non-PS cards it uses only uv scale of 1st detmap)
               const S_vector2 &scl = fg.mat->GetDetailScale();
               if(last_detmap_uv_scale.x<0.0f)
                  last_detmap_uv_scale = scl;
               else
               if(last_detmap_uv_scale != scl){
                              //mishmatch - on non-PS card just disable the second detmap
                  if(!driver->CanUsePixelShader())
                     fg.mat->SetTexture(MTI_DETAIL, NULL);
                              //don't report now (we've many geometry done this way)
                  //REPORT_ERR(C_fstr("Mesh '%s': detail map uv scale in multi-material must be the same on all maps: [%.1f, %.1f] vs. [%.1f, %.1f]", name, last_detmap_uv_scale.x, last_detmap_uv_scale.y, scl.x, scl.y));
                  //allow_cache = false;
               }
            }

            --ck;
         }
         break;

      case CT_SMOOTH_GROUP:
         {
            int num_faces = faces.size();
            dword *sgl = new dword[num_faces];
            ck.Read(sgl, sizeof(dword)*num_faces);
                                       //convert dwords to bytes
            smooth_groups.assign(num_faces, 0);
            dword sg_intersection = 0;
            for(int i=num_faces; i--; ){
               smooth_groups[i] = byte(sgl[i]);
               sg_intersection |= sgl[i];
            }

            if(sg_intersection&0xffffff00){
                                       //convert smoothgroups
               byte conv_tab[32];
               dword dst_bit = 1;
               for(i=0; i<32; i++){
                  conv_tab[i] = 0;
                  if(sg_intersection&(1<<i)){
                     conv_tab[i] = byte(dst_bit);
                     dst_bit <<= 1;
                                       //override bug with more than 8 smoothgroups
                     if(dst_bit==0x100)
                        dst_bit = 1;
                  }
               }
               for(i=smooth_groups.size(); i--; ){
                  byte &dst = smooth_groups[i];
                  dword src = sgl[i];
                  dst = 0;

                  dword test_mask = 1;
                  for(int j=0; j<32; j++, test_mask <<= 1){
                     if(src&test_mask){
                        dst |= conv_tab[j];
                     }
                  }
#ifdef _DEBUG
                  if(CountBits(sg_intersection) < 8){
                     assert(CountBits(src) == CountBits(dst));
                  }
#endif
               }
            }
            delete[] sgl;
            --ck;
         }
         break;

      case CT_FACE_MAP_CHANNEL:
         {
                              //read channel index
            //int channel = -1;
            //ck.Read(&channel, sizeof(channel));
            int channel = ck.ReadDword();
            /*
            if(load_flags&I3DLOAD_LOG)
               LOG(C_fstr("channel: %i", channel));
               */
            channel -= 1;
            if(channel >= MAX_UV_CHANNELS){
               REPORT_ERR(C_fstr("Mesh '%s': invalid map channel used: %i", name, channel+1));
               return false;
            }
                                       //read uv vertices
            word num_uv_verts = ck.ReadWord();
            /*
            if(load_flags&I3DLOAD_LOG)
               LOG(C_fstr("number of uv verts: %i", num_uv_verts));
            */
            C_vector<I3D_text_coor> &uv_vs = uv_verts[channel];
            uv_vs.assign(num_uv_verts, S_vector2());
            if(num_uv_verts)
               ck.Read(&uv_vs.front(), num_uv_verts * sizeof(I3D_text_coor));

                                       //negate 'v'
            for(dword i=num_uv_verts; i--; )
               uv_vs[i].y = -uv_vs[i].y;

                                       //read channel faces
            //word num_faces = 0;
            //ck.Read(&num_faces, sizeof(word));
            word num_faces = ck.ReadWord();
            /*
            if(load_flags&I3DLOAD_LOG)
               LOG(C_fstr("number of uv faces: %i", num_faces));
            */
            C_vector<I3D_triface> &uv_fc = uv_faces[channel];
            uv_fc.assign(num_faces, I3D_triface());
            ck.Read(&uv_fc.front(), num_faces * sizeof(I3D_triface));

#if 0
                              //check dead uv vertices
            C_vector<bool> uv_used(num_uv_verts, false);
            bool *uv_used_p = uv_used.begin();
            for(i=num_faces; i--; ){
               const I3D_triface &fc = uv_fc[i];
               uv_used_p[fc[0]] = true;
               uv_used_p[fc[1]] = true;
               uv_used_p[fc[2]] = true;
            }
            dword num_unused = 0;
            for(i=num_uv_verts; i--; ){
               if(!uv_used_p[i]){
                              //clear the vertex (may be bogus)
                  uv_vs[i].Zero();
                  ++num_unused;
               }
            }
            if(num_unused && channel==0){
               REPORT_ERR(C_fstr("Mesh '%s': %i dead UV vertices", name, num_unused));
               allow_cache = false;
            }
#endif

                              //make sure they're in range
            if(!UV_Check(uv_vs, name))
               allow_cache = false;

            --ck;

                              //save map channel 2
            if(channel==1){
               assert(fgroups.size());
               PI3D_material mat = fgroups.begin()->mat;
                              //find appropriate blend map
               t_blend_maps::const_iterator it = blend_maps.find(mat->GetName());
               if(it!=blend_maps.end()){
                  C_map_channel *mc2 = new C_map_channel;
                  mc2->uv_verts.assign(&uv_verts[1].front(), &uv_verts[1].back()+1);
                  mc2->uv_faces.assign(&uv_faces[1].front(), &uv_faces[1].back()+1);
                                       //save (possible) name of environment map
                  mc2->bitmap_name = it->second;
                  mesh->SetMapChannel2(mc2);
                  mc2->Release();
               }else{
                  REPORT_ERR(C_fstr("Mesh '%s': mapping channel #1 present without blend map", name));
                  allow_cache = false;
               }
            }
         }
         break;

      default:
         REPORT_ERR(C_fstr("Unknown chunk: 0x%.4x", ct));
         --ck;
         allow_cache = false;
      }
   }
                              //check for redundant vertices
   if(verts.size()){
      C_buffer<word> v_map(verts.size());
      MakeVertexMapping(&verts.front(), sizeof(S_vector), verts.size(), &v_map.front(), .0001f);
      int num_redundant = 0;
      const word *vp = v_map.begin();
      for(int i=verts.size(); i--; ){
         if(vp[i]!=i)
            ++num_redundant;
      }
      if(num_redundant){
         REPORT_ERR(C_fstr("Mesh '%s': %i redundant vertices", name, num_redundant));
         allow_cache = false;
      }
   }

                              //check multimats
   {
      dword num_fg = fgroups.size();
      if(num_fg!=mat_num_opaque &&
         num_fg!=mat_num_transl &&
         num_fg!=mat_num_ckey &&
         num_fg!=mat_num_mirror){

         REPORT_ERR(C_fstr("Incompatible materials in multimat (opaque: %i, colorkey: %i, transl: %i, mirror: %i) (mesh '%s')",
            mat_num_opaque, mat_num_ckey, mat_num_transl, mat_num_mirror, name));
         allow_cache = false;
      }else
      if(!diffuse_match || !emissive_match || ! alpha_match){
         REPORT_ERR(C_fstr("Incompatible materials in multimat: %s (mesh '%s')",
            !diffuse_match ? "diffuse color mismatch" : !emissive_match ? "emissive color mismatch" : "alpha mismatch",
            name));
         allow_cache = false;
      }
   }

   CleanMesh(frm, verts, faces, uv_faces, smooth_groups, fgroups, name, allow_cache);

                              //alloc work vertices, reserve more space (for possible duplications)
   C_vector<S_vertex> mesh_verts;
   mesh_verts.reserve(verts.size()*2);
   mesh_verts.assign(verts.size());
                              //create one-to-one vertex map
   dword num_orig_verts = verts.size();
   C_vector<word> vertex_map;
   vertex_map.reserve(num_orig_verts * 2);
   vertex_map.assign(num_orig_verts, 0);
   for(int i=num_orig_verts; i--; ){
      vertex_map[i] = word(i);
      mesh_verts[i].xyz = verts[i];
   }

#ifdef REPORT_EMPTY_SMOOTHGROUPS
   if(!smooth_groups.size()){
      REPORT_ERR(C_fstr("Mesh '%s': no smoothgroups", name));
      allow_cache = false;
   }else{
      dword num_clear = 0;
      for(dword i=smooth_groups.size(); i--; ){
         if(!smooth_groups[i])
            ++num_clear;
      }
      if(num_clear){
         REPORT_ERR(C_fstr("Mesh '%s': no smoothgroups on %i faces:", name, num_clear));
         allow_cache = false;
      }
   }
#endif

                              //prepare uv coordinates and normals, duplicate vertices as necessary
#ifdef DEBUG_PROFILE_NORMALS
   BegProf();
#endif
   MakeVertexNormals(num_orig_verts, vertex_map, mesh_verts, faces, smooth_groups);
#ifdef DEBUG_PROFILE_NORMALS
   driver->PRINT(C_fstr("MakeVertexNormals: %.2f", EndProf()));
#endif
   if(!AssignUVCoordinates(vertex_map, mesh_verts, faces, uv_verts, uv_faces, name)){
      allow_cache = false;
   }

                              //don't keep empty meshes
   if(!faces.size() || !mesh_verts.size()){
#ifdef REPORT_EMPTY_MESH_ERROR
      REPORT_ERR(C_fstr("Mesh '%s': zero vertices or faces", name));
#endif
      allow_cache = false;
   }else{
#if defined _DEBUG && 1
      if(any_bump){
         C_buffer<S_texture_space> texture_space;
         GenerateTextureSpace(mesh_verts, faces, texture_space, name, allow_cache);

         mesh->vertex_buffer.SetFVFlags(mesh->vertex_buffer.GetFVFlags() | D3DFVF_TEXTURE_SPACE);
         C_vector<S_vertex_st> st_verts(mesh_verts.size());
         for(dword i=mesh_verts.size(); i--; ){
            (S_vertex&)st_verts[i] = mesh_verts[i];
            st_verts[i].ts = texture_space[i];
         }
         mesh->vertex_buffer.SetVertices(&st_verts.front(), st_verts.size());
      }else
#endif
      mesh->vertex_buffer.SetVertices(&mesh_verts.front(), mesh_verts.size());

      mesh->SetFGroups(&fgroups.front(), fgroups.size());
      mesh->SetSmoothGroups(smooth_groups);
      mesh->SetFaces(&faces.front());
#if defined _DEBUG && 0
      C_buffer<I3D_triface> fb(faces.size());
      mesh->GetFaces(fb.begin());
      mesh->SetFaces(fb.begin());
#endif
   }

   if(vertex_map.size() > num_orig_verts)
      mesh->vertex_buffer.SetVertexSpreadMap(&vertex_map[num_orig_verts], vertex_map.size()-num_orig_verts);

   return true;
}

//----------------------------

void C_loader::AddSectorWalls(PI3D_frame root, C_vector<C_smart_ptr<I3D_frame> > &walls,
   C_vector<C_smart_ptr<I3D_visual> > &portal_defs, bool root_help, PI3D_sector sct){

   walls.push_back(root);

   for(int i=root->NumChildren1(); i--; ){
      PI3D_frame f1 = root->GetChildren1()[i];

      C_str prop_data;
      E_PROP_KEYWORD kw_cast_to;

      t_frm_properties::const_iterator it = frm_properties.find(f1);
      if(it==frm_properties.end())
         continue;
      if(!GetPropetriesBegin((*it).second, prop_data, kw_cast_to))
         continue;
      while(kw_cast_to != PROP_CAST_SECT && GetPropertiesNext(prop_data, kw_cast_to));
      if(kw_cast_to!=PROP_CAST_SECT)
         continue;

      if(root_help){
                              //re-link to sector
         f1->LinkTo(sct, I3DLINK_UPDMATRIX);
      }
      f1->AddRef();

      E_PROP_KEYWORD kw;

      bool can_add_wall = true;

      if(GetPropetriesBegin((*it).second, prop_data, kw))
      do{
         switch(kw){
         case PROP_HELP:
            {
                              //link all childrens of f1 to frm
               while(f1->NumChildren())
                  f1->GetChildren()[0]->LinkTo(root, I3DLINK_UPDMATRIX);
               I3D_RESULT ir;
               ir = container->RemoveFrame(f1);
               assert(I3D_SUCCESS(ir));
            }
            break;
         case PROP_SIMPLE_SECTOR:
            REPORT_ERR(C_fstr("Keyword 'simple' ignored on linked wall: '%s'", (const char*)f1->GetName()));
            break;

         case PROP_SECTOR_ONEWAY_PORTAL:
            {
               can_add_wall = false;
               if(f1->GetType()==FRAME_VISUAL)
                  portal_defs.push_back(I3DCAST_VISUAL(f1));
               else
                  REPORT_ERR(C_fstr("Keyword 'portal' allowed only on visual: '%s'", (const char*)f1->GetName()));
            }
            break;
         }
      } while(GetPropertiesNext(prop_data, kw));

      if(can_add_wall)
         AddSectorWalls(f1, walls, portal_defs, root_help, sct);

      f1->Release();
   }
}

//----------------------------

I3DENUMRET C_loader::BuildSectors(PI3D_frame frm, PI3D_scene scene, PI3D_model model){

   assert(frm->GetType1()==FRAME_VISUAL);
   do{
                              //check if its father or grand-father is already sector
      if(frm->GetParent1()){
         if(frm->GetParent1()->GetType1()==FRAME_SECTOR){
            if(!I3DCAST_SECTOR(frm->GetParent1())->IsPrimary())
               break;
         }
         if(frm->GetParent1()->GetParent1() && frm->GetParent1()->GetParent1()->GetType1()==FRAME_SECTOR)
            break;
      }

      C_str prop_data;
      E_PROP_KEYWORD kw_cast_to;

      t_frm_properties::const_iterator it = frm_properties.find(frm);
      if(it==frm_properties.end())
         break;
      if(!GetPropetriesBegin((*it).second, prop_data, kw_cast_to))
         break;

      while(kw_cast_to != PROP_CAST_SECT && GetPropertiesNext(prop_data, kw_cast_to));
      if(kw_cast_to!=PROP_CAST_SECT)
         break;

      PI3D_sector sct = I3DCAST_SECTOR(scene->CreateFrame(FRAME_SECTOR));
      sct->SetName(C_fstr("sector %s", (const char*)frm->GetName()));

      bool root_help = false;
      bool simple_sector = false;

                     //check additional properties
      E_PROP_KEYWORD kw;
      if(GetPropetriesBegin((*it).second, prop_data, kw))
      do{
         switch(kw){
         case PROP_HELP:
            root_help = true;
            break;

         case PROP_SIMPLE_SECTOR:
            simple_sector = true;
            break;
         }
      } while(GetPropertiesNext(prop_data, kw));

      bool dbase_ok = false;
#ifdef USE_DATABASE
      C_fstr cache_name("%s+%s:sector", (const char*)file_name, (const char*)frm->GetName1());
      C_cache *ck = driver->dbase.OpenRecord(cache_name, file_time_);
      if(ck){
         dbase_ok = sct->LoadCachedInfo(ck);
         driver->dbase.CloseRecord(ck);
      }
#endif
      {
                              //collect all walls forming sector
         C_vector<C_smart_ptr<I3D_frame> > walls;
         C_vector<C_smart_ptr<I3D_visual> > portal_defs;
         AddSectorWalls(frm, walls, portal_defs, root_help, sct);

         if(!dbase_ok){
            I3D_RESULT ir = sct->Build((PI3D_frame*)&walls.front(), walls.size(), (CPI3D_visual*)(portal_defs.size() ? &portal_defs.front() : NULL), portal_defs.size(), simple_sector, *this);
#ifdef USE_DATABASE
            if(I3D_SUCCESS(ir)){
               C_cache *ck = driver->dbase.CreateRecord(cache_name, file_time_);
               if(ck){
                  sct->SaveCachedInfo(ck);
                  driver->dbase.CloseRecord(ck);
               }
            }
         }
#endif
      }

      sct->Connect(scene);
      sct->LinkTo(frm->GetParent());
      frm->LinkTo(sct);
      //if(hp->model) hp->model->AddFrame(sct);
      //else hp->scene->AddFrame(sct);
      assert(!model);
      container->AddFrame(sct);
      sct->Release();

      if(root_help){    //delete root
                        //link all childrens of f1 to frm
         while(frm->NumChildren()){
            frm->GetChildren()[0]->LinkTo(frm->GetParent(), I3DLINK_UPDMATRIX);
         }
         I3D_RESULT ir;
         ir = container->RemoveFrame(frm);
         assert(I3D_SUCCESS(ir));
      }
                        //nested sectors unsupported now
      //return I3DENUMRET_SKIPCHILDREN;
   }while(false);             //allow break-out from block

   return I3DENUMRET_SKIPCHILDREN;
}

//----------------------------

void C_loader::SmoothSceneNormals(PI3D_scene scene, C_vector<S_smooth_info> &smooth_list){

   if(smooth_list.size()<2)
      return;
                              //count how many (if any) really needs to be shaded
   for(dword i=smooth_list.size(); i--; ){
      if(smooth_list[i].need_this)
         break;
   }
   if(i==-1)
      return;

                              //keep per-visual smooth data
   class C_smooth_data{
      C_smooth_data(const C_smooth_data&){}
      void operator =(const C_smooth_data&){}
   public:
      C_smooth_data():
         vertex_info(NULL)
      {}
      ~C_smooth_data(){
         delete[] vertex_info;
      }
      PI3D_visual vis;
      PI3D_mesh_base mb;      //visual's mesh
      void *verts;            //pointer to locked vertices
      dword vstride;          //stride of vertices
      S_matrix m_inv;         //inverse visual's matrix

                              //per-vertex data
      struct S_vertex_info{
         word indx;
         S_vector v_world;
         S_vector n_world;
         S_vector n_sum;
         dword count;
         S_vertex_info(): count(0){}
      } *vertex_info;
      dword num_verts;
      I3D_bsphere bs_world;
      I3D_bbox bb_world;
      C_vector<pair<word, word> > dup_verts; //non-orignal vertices. whose normals will be duplicated
   };
   C_smooth_data *smooth_data = new C_smooth_data[smooth_list.size()];
   dword num_sd = 0;

                              //preprocess - create info
   for(i=smooth_list.size(); i--; ){
      S_smooth_info &si = smooth_list[i];
      C_smooth_data &sd = smooth_data[num_sd];
      PI3D_visual vis = I3DCAST_VISUAL((PI3D_frame)si.vis);
      sd.vis = vis;
      sd.mb = vis->GetMesh();
      if(!sd.mb)
         continue;
                              //get access to vertices
      sd.verts = (void*)sd.mb->LockVertices();
      sd.vstride = sd.mb->GetSizeOfVertex();

      //PI3D_driver drv = vis->GetDriver1();
      //drv->PRINT(C_fstr("visual: '%s'", (const char*)vis->GetName1()));
                              //create list of edge vertices (only they will be processed, to speed up)
      {
         dword numv = sd.mb->NumVertices();
         C_vector<word> v_map(numv);
         MakeVertexMapping((const S_vector*)sd.verts, sd.vstride, numv, &v_map.front());
         dword num_faces = sd.mb->NumFaces();
         C_buffer<I3D_triface> faces(num_faces);
         //const I3D_triface *faces = sd.mb->LockFaces();
         sd.mb->GetFaces(faces.begin());
         C_vector<I3D_edge> edges; edges.reserve(num_faces);

         for(int fi=num_faces; fi--; ){
            const I3D_triface &fc = faces[fi];
            word v0 = fc.vi[0];
            for(int j=3; j--; ){
               word v1 = v0;
               v0 = fc.vi[j];
               I3D_edge e_mapped(v_map[v0], v_map[v1]);

                              //add or split edge
               int ei = FindEdge(edges.size() ? &edges.front() : NULL, edges.size(), I3D_edge(e_mapped.v[1], e_mapped.v[0]));
               if(ei!=-1){
                  //edges_mapped[ei] = edges_mapped.back(); edges_mapped.pop_back();
                  edges[ei] = edges.back(); edges.pop_back();
               }else{
                  //edges_mapped.push_back(e_mapped);
                  edges.push_back(e_mapped);
                  //edges.push_back(I3D_edge(v0, v1));
               }
            }
         }

         sd.m_inv = vis->GetInvMatrix();
         S_matrix m = vis->GetMatrix();
         float r_mag = 1.0f / m(0).Magnitude();
         m(0) *= r_mag;
         m(1) *= r_mag;
         m(2) *= r_mag;

         const I3D_bsphere &bs = vis->GetBoundVolume().bsphere;
         sd.bs_world.pos = bs.pos * m;
         sd.bs_world.radius = bs.radius * m(0).Magnitude();

         I3D_bbox &bb = sd.bb_world;
         bb.Invalidate();

                              //now consider all vertices of edges (one vertex of each edge)
         sd.num_verts = edges.size();
         sd.vertex_info = new C_smooth_data::S_vertex_info[sd.num_verts];
         for(int ei=sd.num_verts; ei--; ){
            const I3D_edge &e = edges[ei];
            word vi = e[0];
            //drv->PRINT(C_fstr("%i", vi));
            C_smooth_data::S_vertex_info &vinf = sd.vertex_info[ei];
            vinf.indx = vi;

            const S_vector *v = (S_vector*)(((byte*)sd.verts) + sd.vstride*vi);
            vinf.v_world = v[0] * m;
            vinf.n_world = v[1] % m;
            vinf.n_sum = vinf.n_world;

            bb.min.Minimal(vinf.v_world);
            bb.max.Maximal(vinf.v_world);
         }
         //sd.mb->UnlockFaces();

                              //also add all vertices which were duplicated (due to uv assignment)
                              // (using C_vector<I3D_edge> to store vertices now)
         for(dword vi=numv; vi--; ){
            word orig = v_map[vi];
            if(orig!=vi){
                              //check if original is included
               for(dword i=edges.size(); i--; ){
                  if(edges[i][0]==orig){
                     sd.dup_verts.push_back(pair<word, word>(word(vi), orig));
                     break;
                  }
               }
            }
         }
      }
      ++num_sd;
   }

                              //now we have enough info pre-computed, make smoothing
   for(i=smooth_list.size(); i--; ){
      const C_smooth_data &sd0 = smooth_data[i];
                              //try to smooth this with all neighbours
      for(int j=i; j--; ){
         const C_smooth_data &sd1 = smooth_data[j];
                              //check if those 2 connect
         float d = (sd0.bs_world.pos - sd1.bs_world.pos).Magnitude();
         if(d > (sd0.bs_world.radius + sd1.bs_world.radius))
            continue;
         if(sd0.bb_world.max.x<sd1.bb_world.min.x ||
            sd0.bb_world.max.y<sd1.bb_world.min.y ||
            sd0.bb_world.max.z<sd1.bb_world.min.z ||
            sd1.bb_world.max.x<sd0.bb_world.min.x ||
            sd1.bb_world.max.y<sd0.bb_world.min.y ||
            sd1.bb_world.max.z<sd0.bb_world.min.z)
            continue;
                              //those 2 potentionally connect, continue
         for(int vi0=sd0.num_verts; vi0--; ){
            C_smooth_data::S_vertex_info &vinf0 = sd0.vertex_info[vi0];

            for(int vi1=sd1.num_verts; vi1--; ){
               C_smooth_data::S_vertex_info &vinf1 = sd1.vertex_info[vi1];
               float d = (vinf0.v_world - vinf1.v_world).Square();
               if(d > .001f)
                  continue;
               //PI3D_driver drv = sd0.vis->GetDriver1();
                              //add those normals
               vinf0.n_sum += vinf1.n_world;
               vinf1.n_sum += vinf0.n_world;
               ++vinf0.count;
               ++vinf1.count;
            }
         }
      }

                              //finalize - store normals sums and unlock mesh
      const S_matrix &m_inv = sd0.m_inv;
      for(int vi0=sd0.num_verts; vi0--; ){
         C_smooth_data::S_vertex_info &vinf = sd0.vertex_info[vi0];
         if(!vinf.count)
            continue;
         word vi = vinf.indx;
         //driver->DebugLine(vinf.v_world, vinf.v_world + vinf.n_sum, 0);
         S_vector *v = (S_vector*)(((byte*)sd0.verts) + sd0.vstride*vi);
         v[1] = vinf.n_sum % m_inv;
         v[1].Normalize();
      }
                              //duplicate non-orig verts from originals
      for(vi0=sd0.dup_verts.size(); vi0--; ){
         word vi = sd0.dup_verts[vi0].first;
         word vi_src = sd0.dup_verts[vi0].second;
         S_vector *v = (S_vector*)(((byte*)sd0.verts) + sd0.vstride*vi);
         const S_vector *v_src = (S_vector*)(((byte*)sd0.verts) + sd0.vstride*vi_src);
         v[1] = v_src[1];
      }
#if defined _DEBUG && 0
      {
         PI3D_driver drv = sd0.vis->GetDriver1();
         for(dword i=sd0.num_verts; i--; ){
            const S_vector *v = (S_vector*)(((byte*)sd0.verts) + sd0.vstride*i);
            float len = v[1].Magnitude();
            //assert(IsAbsMrgZero(1.0f - len));
            drv->PRINT(C_fstr("%i: %.8f", i, len));
         }
      }
#endif
      sd0.mb->UnlockVertices();
   }

   delete[] smooth_data;
}

//----------------------------

I3D_RESULT C_loader::Open(const char *fname, dword flags, PI3D_LOAD_CB_PROC cb_proc,
   void *cb_context, PI3D_frame root, PI3D_scene s, PI3D_animation_set as, PI3D_container c){

   _control87(_PC_24, _MCW_PC);

   load_cb_proc = cb_proc;
   load_cb_context = cb_context;
   load_flags = flags;
   file_length = ck.GetHandle()->filesize();
   file_name = fname;
   file_name.ToLower();
   ck.GetHandle()->GetStream()->GetTime((dword*)&file_time_);
   root_frame = root;
   scene = s;
   container = c;
   anim_set = as;

   word node_id = 0xffff;
   typedef set<C_str> t_name_check;
   t_name_check name_check;

   typedef map<class I3D_visual*, S_SM_data> t_vis_data;
   t_vis_data vis_finish;
   C_vector<S_smooth_info> smooth_list;
   bool any_sector = false;
                              //keep visuals which will be written to cache
   C_vector<C_smart_ptr<I3D_visual> > vis_to_cache;

   dword rename_index = 0;

   while(ck){
      CK_TYPE ct = ++ck;
      switch(ct){
      case CT_USER_COMMENTS:
         ck >> user_comments;
         break;

      case CT_NUM_MATERIALS:
         num_mats = ck.RIntChunk();
         break;

      case CT_MATERIAL:
         if(scene){
            I3D_RESULT ir = ShowProgress();
            if(I3D_SUCCESS(ir))
               ir = ReadMaterial();
            if(I3D_FAIL(ir))
               return ir;
         }
         --ck;
         break;

      case CT_NODE_MESH:
      case CT_NODE_DUMMY:
      case CT_NODE_BONE:
         {
            I3D_RESULT ir = ShowProgress();
            if(I3D_FAIL(ir))
               return ir;
            ++node_id;
            C_smart_ptr<I3D_frame> frm;

            if(scene){
               switch(ct){
               case CT_NODE_DUMMY: frm = scene->CreateFrame(FRAME_DUMMY); break;
               case CT_NODE_MESH: frm = scene->CreateFrame(FRAME_VISUAL, I3D_VISUAL_OBJECT); break;
               case CT_NODE_BONE: frm = scene->CreateFrame(FRAME_JOINT); break;
               default: assert(0);
               }
               frm->Release();
            }
            PI3D_keyframe_anim anim = NULL;
            PI3D_frame prnt = NULL;
            S_vector nu_scale(1, 1, 1);
            S_vector pivot(0, 0, 0);
            C_str props;
            C_str frm_name;
            bool lock_joint = false;
            bool break_joint = false;
            bool mesh_read = false;
            int region_index = -1;
            E_PROP_KEYWORD bbox_update = PROP_UNKNOWN;
            S_SM_data *sm_data = NULL;
            I3D_bbox bbox;
            bbox.Invalidate();
            S_auto_lod_data al_data;
            C_smart_ptr<I3D_procedural_base> procedural;
            bool allow_cache = true;
            C_str name_dup_err;  //name of duplicated frame's name, NULL if no

            while(ck){
               ct = ++ck;
               switch(ct){
               case CT_NODE_NAME:
                  {
                     frm_name = ck.RStringChunk();
                              //check name duplication
                     if(name_check.find(frm_name) != name_check.end()){
                        name_dup_err = frm_name;
                        frm_name = C_fstr("%s_renamed_%.8x", (const char*)frm_name, rename_index);
                        ++rename_index;
                     }
                     name_check.insert(frm_name);

                     if(frm)
                        frm->SetName(frm_name);
                  }
                  break;
               case CT_NODE_PARENT:
                  if(frm){
                     word parent_id = ck.RWordChunk();
                     prnt = GetFrame(parent_id);
                     if(!prnt)
                        REPORT_ERR(C_xstr("Frame '%': cannot find parent object") %frm_name);
                  }else{
                     --ck;
                  }
                  break;

               case CT_BOUNDING_BOX:
                  {
                     ck.Read(&bbox, sizeof(bbox));
                     swap(bbox.min.y, bbox.min.z);
                     swap(bbox.max.y, bbox.max.z);
                     --ck;
                  }
                  break;

               case CT_PROPERTIES:
                  if(frm){
                     props = ck.RStringChunk();
                     //props = "{\nOUVS\nParam 0 .1\n Param Mode Delta Param \"Anim V\" 1.0 }"; //debug test
                     dword size = props.Size();
                     if(size){
                        size = dtaBin2Text(&props[0], size);
                        props[size] = 0;
                     }

                     E_PROP_KEYWORD kw;
                     C_str str_prop, kw_name;
                     if(GetPropetriesBegin(props, str_prop, kw, &kw_name))
                     do{
                        C_str err;
                        switch(kw){
                           /*
                        case PROP_CAST_SNGM:
                        case PROP_CAST_PRTC:
                        case PROP_CAST_BBRD:
                        case PROP_CAST_MRPH:
                        case PROP_CAST_FLRE:
                        case PROP_CAST_OUVS:
                        case PROP_CAST_FIRE:
                        */
                        default:
                           {
                              bool ok = false;
                              if(frm && strlen(kw_name)==4){
                                 const char *cp_cast_to = kw_name;
                                 dword dw_cast_type = *(dword*)cp_cast_to;

                                 PI3D_frame new_vis = scene->CreateFrame(FRAME_VISUAL, dw_cast_type);
                                 if(new_vis){
                                    ok = true;
                                    new_vis->Duplicate(frm);
                                    frm = new_vis;
                                    new_vis->Release();

                                                //initialize visual (if possible)
                                    switch(kw){
                                    case PROP_CAST_SNGM:
                                    //case PROP_CAST_FACE:
                                       sm_data = &(*vis_finish.insert(pair<PI3D_visual, S_SM_data>
                                          ((PI3D_visual)(PI3D_frame)frm, S_SM_data())).first).second;
                                       allow_cache = false;
                                       break;

                                    case PROP_CAST_FLRE:
                                       {
                                          //PI3D_flare flr = (PI3D_flare)(PI3D_frame)frm;
                                          PI3D_visual vis = I3DCAST_VISUAL(frm);
                                                      //read additional properties
                                          float f;
                                          int i;
                                          if(GetPropertiesFloat(str_prop, f)){
                                             vis->SetProperty(I3DPROP_FLRE_F_SCALE_DIST, I3DFloatAsInt(f));
                                             if(GetPropertiesFloat(str_prop, f)){
                                                vis->SetProperty(I3DPROP_FLRE_F_NORMAL_ANGLE, I3DFloatAsInt(f * PI / 180.0f));
                                                if(GetPropertiesFloat(str_prop, f)){
                                                   vis->SetProperty(I3DPROP_FLRE_F_ROTATE_RATIO, I3DFloatAsInt(f));
                                                   if(GetPropertiesInt(str_prop, i)){
                                                      vis->SetProperty(I3DPROP_FLRE_I_FADE_TIME, i);
                                                      if(GetPropertiesFloat(str_prop, f)){
                                                         vis->SetProperty(I3DPROP_FLRE_F_PIVOT, I3DFloatAsInt(f));
                                                      }
                                                   }
                                                }
                                             }
                                          }
                                       }
                                       break;
                                    }
                                 }else{
                                    err = C_fstr("cannot cast visual object: '%s'", cp_cast_to);
                                 }
                              }
                              if(!ok)
                                 err = C_fstr("unknown keyword: '%s'", (const char*)kw_name);
                           }
                           break;

                        case PROP_PARAM:
                           if(frm->GetType()==FRAME_VISUAL){
                                       //set visual parameter
                              PI3D_visual vis = I3DCAST_VISUAL(frm);
                              dword vt = vis->GetVisualType();
                              const S_visual_property *props;
                              dword num_props;
                              if(!driver->GetVisualPropertiesInfo(vt, &props, &num_props)){
                                 err = "param: unknown visual type";
                                 break;
                              }

                              int indx;
                              if(!GetPropertiesInt(str_prop, indx)){
                                 C_str p_name;
                                 if(!GetPropertiesString(str_prop, p_name)){
                                    err = "param: expecting index";
                                    break;
                                 }
                                       //search the param by name
                                 for(int i=num_props; i--; ){
                                    if(p_name == props[i].prop_name)
                                       break;
                                 }
                                 if(i==-1){
                                    err = C_fstr("param: invalid property name: '%s'", (const char*)p_name);
                                    break;
                                 }
                                 indx = i;
                              }
                              if(indx<0 || indx>=(int)num_props){
                                 err = C_fstr("param: invalid property index: %i", indx);
                                 break;
                              }
                              const S_visual_property &prop = props[indx];

                              C_str tmp;
                              dword val = 0;
                                       //read by type
                              switch(prop.prop_type){
                              case I3DPROP_BOOL:
                              case I3DPROP_INT:
                                 {
                                    int i;
                                    if(!GetPropertiesInt(str_prop, i)){
                                       C_str s;
                                       if(!GetPropertiesString(str_prop, s))
                                          s = str_prop;
                                       err = C_fstr("param: invalid property #%i: '%s'", indx, (const char*)s);
                                       break;
                                    }
                                    val = i;
                                 }
                                 break;

                              case I3DPROP_FLOAT:
                                 {
                                    float f;
                                    if(!GetPropertiesFloat(str_prop, f)){
                                       C_str s;
                                       if(!GetPropertiesString(str_prop, s))
                                          s = str_prop;
                                       err = C_fstr("param: invalid property #%i: '%s'", indx, (const char*)s);
                                       break;
                                    }
                                    val = I3DFloatAsInt(f);
                                 }
                                 break;

                              case I3DPROP_STRING:
                                 {
                                    if(!GetPropertiesString(str_prop, tmp)){
                                       err = C_fstr("param: invalid property #%i: expecting value", indx);
                                       break;
                                    }
                                    val = (dword)(const char*)tmp;
                                 }
                                 break;

                              case I3DPROP_ENUM:
                                 {
                                          //enum represented either by index, or name
                                    const char *cp = prop.prop_name;
                                    cp += strlen(cp) + 1;

                                    int i;
                                    if(GetPropertiesInt(str_prop, i)){
                                          //check if index valid
                                       for(int num_enums=0; *cp; ++num_enums, cp += strlen(cp) + 1);
                                       if(i<=0 || i>=num_enums){
                                          err = C_fstr("param: invalid property #%i: bad index: %i", indx, i);
                                          break;
                                       }
                                       val = i;
                                    }else{
                                       C_str str;
                                       if(GetPropertiesString(str_prop, str)){
                                          for(val=0; *cp; ++val, cp += strlen(cp) + 1){
                                             if(str==cp)
                                                break;
                                          }
                                          if(!*cp){
                                             err = C_fstr("param: invalid property #%i: bad value: '%s'", indx, (const char*)str);
                                             break;
                                          }
                                       }else{
                                          err = C_fstr("param: invalid property #%i: expecting value", indx);
                                          break;
                                       }
                                    }
                                 }
                                 break;
                              default:
                                 err = C_fstr("param: invalid property #%i: unsupported type", indx);
                              }
                              if(err.Size())
                                 break;
                                                //finally set the property
                              I3D_RESULT ir = vis->SetProperty(indx, val);
                              if(I3D_FAIL(ir)){
                                 err = C_fstr("'param': failed to set property '#", indx);
                              }
                           }else{
                              err = "'param' may be used only on visuals";
                           }
                           break;

                        case PROP_LOCK:
                           if(frm && frm->GetType()==FRAME_JOINT){
                              lock_joint = true;
                           }else{
                              err = "'lock' may be used only on joints";
                           }
                           break;

                        case PROP_BREAK:
                           if(frm && frm->GetType()==FRAME_JOINT){
                              break_joint = true;
                           }else{
                              err = "'break' may be used only on joints";
                           }
                           break;

                        case PROP_CAST_JTVB:
                        case PROP_CAST_JTBB:
                        case PROP_REGION:
                           if(frm && frm->GetType()==FRAME_DUMMY){
                              switch(kw){
                              case PROP_REGION:
                                 GetPropertiesInt(str_prop, region_index);
                                 break;
                              }
                              //PI3D_dummy dum = I3DCAST_DUMMY(frm);
                              //bbox = *dum->GetBBox();
                              bbox_update = kw;
                           }else{
                              err = C_fstr("keyword '%s' allowed only on dummies", (const char*)kw_name);
                           }
                           break;

                        case PROP_AUTO_LOD:
                           if(frm){
                                          //read additional properties
                              float min_dist = 5.0f;
                              float max_dist = 50.0f;
                              int num_parts = 10;
                              int min_num_faces = 0;

                              if(GetPropertiesFloat(str_prop, min_dist)){
                                 if(GetPropertiesFloat(str_prop, max_dist)){
                                    if(GetPropertiesInt(str_prop, num_parts)){
                                       if(!GetPropertiesInt(str_prop, min_num_faces))
                                          err = "auto_lod - lowest face count expected";
                                    }else
                                       err = "auto_lod - number of parts expected";
                                 }else
                                    err = "auto_lod - maximal distance expected";
                              }else
                                 err = "auto_lod - minimal distance expected";
                              //if(!err.Size())
                              {
                                 if(frm && frm->GetType()==FRAME_VISUAL){
                                    al_data.use = true;
                                    al_data.min_dist = min_dist;
                                    al_data.max_dist = max_dist;
                                    al_data.num_parts = num_parts;
                                    al_data.min_num_faces = min_num_faces;
                                 }else
                                    err = "auto_lod is only applicable on visuals";
                              }
                           }
                           break;

                        case PROP_CAST_VOLS:
                        case PROP_CAST_VOLR:
                        case PROP_CAST_VOLB:
                        case PROP_CAST_VOLC:
                        case PROP_CAST_VOLP:
                           if(frm && frm->GetType()==FRAME_DUMMY){
                              PI3D_volume vol = I3DCAST_VOLUME(scene->CreateFrame(FRAME_VOLUME));
                              vol->Duplicate(frm);

                              frm = vol;
                              vol->Release();

                              switch(kw){
                              case PROP_CAST_VOLS: vol->SetVolumeType(I3DVOLUME_SPHERE); break;
                              case PROP_CAST_VOLB: vol->SetVolumeType(I3DVOLUME_BOX); break;
                              case PROP_CAST_VOLR: vol->SetVolumeType(I3DVOLUME_RECTANGLE); break;
                              case PROP_CAST_VOLC: vol->SetVolumeType(I3DVOLUME_CYLINDER); break;
                              case PROP_CAST_VOLP: vol->SetVolumeType(I3DVOLUME_CAPCYL); break;
                              default: assert(0);
                              }
                                       //explicitly set to 'invalid' material id now
                              vol->SetCollisionMaterial(0x80000000);
                           }else{
                              err = C_fstr("keyword '%s' allowed only on dummies", (const char*)kw_name);
                           }
                           break;

                        case PROP_SMOOTH:
                           if(frm && frm->GetType()==FRAME_VISUAL && I3DCAST_VISUAL(frm)->GetVisualType1()==I3D_VISUAL_OBJECT){
                              smooth_list.push_back(S_smooth_info());
                              S_smooth_info &si = smooth_list.back();
                              si.vis = frm;
                              al_data.preserve_edges = true;
                           }else
                              err = C_fstr("keyword '%s' allowed only on visuals (objects)", (const char*)kw_name);
                           break;

                        case PROP_PROCEDURAL:
                           {
                              C_str proc_name;
                              if(!GetPropertiesString(str_prop, proc_name)){
                                 err  = "expected procedure name";
                                 break;
                              }
                              if(frm && frm->GetType()==FRAME_VISUAL){
                                             //get and attach procedural to frame
                                 PI3D_procedural_base pb;
                                 I3D_RESULT ir = driver->GetProcedural(proc_name, (load_flags&I3DLOAD_ERRORS) ? load_cb_proc : NULL, load_cb_context, &pb);
                                 if(I3D_SUCCESS(ir)){
                                    procedural = pb;
                                    pb->Release();
                                 }
                              }else{
                                 err = C_fstr("keyword '%s' allowed only on visuals", (const char*)kw_name);
                              }
                           }
                           break;

                        case PROP_MATERIAL:
                           {
                                             //scan material ID - either integer, or name
                              int mat_id = 1;
                              if(!GetPropertiesInt(str_prop, mat_id)){
                                 C_str mat_name;
                                 if(!GetPropertiesString(str_prop, mat_name)){
                                    err = "expecting material identifier (number or name)";
                                 }else{
                                             //detect which material it is
                                    const I3D_driver::t_col_mat_info &mat_info = driver->GetColMatInfo();
                                    //if(mat_info.size()){
                                       I3D_driver::t_col_mat_info::const_iterator it;
                                       for(it=mat_info.begin(); it!=mat_info.end(); it++){
                                          if(it->second.name==mat_name){
                                             mat_id = it->first;
                                             break;
                                          }
                                       }
                                       if(it==mat_info.end()){
                                          err = C_str(C_xstr("unknown material name: '%'") % mat_name);
                                       }
                                    //}
                                 }
                              }

                              if(frm)
                              switch(frm->GetType()){
                              case FRAME_VISUAL:
                                 I3DCAST_VISUAL(frm)->SetCollisionMaterial(mat_id);
                                 frm->SetFlags(I3D_FRMF_STATIC_COLLISION, I3D_FRMF_STATIC_COLLISION);
                                 break;
                              case FRAME_VOLUME:
                                 I3DCAST_VOLUME(frm)->SetCollisionMaterial(mat_id);
                                 break;
                              default:
                                 err = C_fstr("keyword '%s' allowed only on visuals or volumes", (const char*)kw_name);
                              }
                           }
                           break;

                        case PROP_CAST_SECT:
                        case PROP_SIMPLE_SECTOR:
                        case PROP_HELP:
                        case PROP_SECTOR_ONEWAY_PORTAL:
                           if(root_frame->GetType1()==FRAME_SECTOR){
                              any_sector = true;
                           }else{
                              err = C_fstr("keyword '%s' allowed only for scene loading", (const char*)kw_name);
                           }
                           break;

                        case PROP_HIDE:
                           {
                              float hide;
                              if(GetPropertiesFloat(str_prop, hide)){
                                 bool ok = false;
                                 if(frm){
                                    ok = true;
                                    switch(frm->GetType1()){
                                    case FRAME_VISUAL: I3DCAST_VISUAL(frm)->SetHideDistance(hide); break;
                                    case FRAME_JOINT: I3DCAST_JOINT(frm)->SetHideDistance(hide); break;
                                    default: ok = false;
                                    }
                                 }
                                 if(!ok){
                                    err = C_fstr("keyword '%s' not allowed on this frame type", (const char*)kw_name);
                                 }
                              }else{
                                 err = "keyword 'hide': expecting distance";
                              }
                           }
                           break;
                        }

                        if(err.Size())
                           REPORT_ERR(C_xstr("Frame '%': %") %frm_name %err);
                     }while(GetPropertiesNext(str_prop, kw, &kw_name));

                     if(frm->GetType1()==FRAME_VOLUME){
                              //if it was cast to volume, it must have set explicitly material
                        if(I3DCAST_CVOLUME(frm)->GetCollisionMaterial()==0x80000000){
                           I3DCAST_VOLUME(frm)->SetCollisionMaterial(0);
                           if(driver->GetColMatInfo().size())
                              REPORT_ERR(C_xstr("Volume '%' has not set material") %frm_name);
                        }
                     }
                  }else{
                     --ck;
                  }
                  break;

               case CT_MESH:
                  {
                     if(frm && frm->GetType()==FRAME_VISUAL){
                                       //read and assign mesh now
                        PI3D_visual vis = I3DCAST_VISUAL(frm);
                                       //try to read cached version
                        bool read_ok = false;
                        C_fstr cache_name("%s+%s", (const char*)file_name, (const char*)frm->GetName1());
                        C_cache *cck = driver->dbase.OpenRecord(cache_name, file_time_);
                        if(cck){
                           read_ok = vis->LoadCachedInfo(cck, *this, materials);
                           driver->dbase.CloseRecord(cck);
                           if(read_ok){
                                       //unmark creation of cached items
                              al_data.use = false;
                              if(smooth_list.size() && smooth_list.back().vis==frm)
                                 smooth_list.back().need_this = false;
                           }
                        }
                        bool allow_cache_now = true;
                        if(!read_ok){
                           C_smart_ptr<I3D_mesh_base> mb = driver->CreateMesh(I3DVC_XYZ | I3DVC_NORMAL | (1<<I3DVC_TEXCOUNT_SHIFT));
                           mb->Release();
                           I3D_RESULT ir = ReadMesh(frm, mb, frm_name, allow_cache_now);
                           if(I3D_FAIL(ir))
                              return ir;
                           vis->SetMeshInternal(mb);
                           //mb->ComputeBoundingVolume();
                           mesh_read = true;
                           if(allow_cache_now && allow_cache)
                              vis_to_cache.push_back(vis);
                        }
                        switch(vis->GetVisualType1()){
                        case I3D_VISUAL_SINGLEMESH:
                           sm_data->mesh_ok = read_ok;
                           sm_data->allow_cache = allow_cache_now;
                           break;
                        }
                     }
                     --ck;
                  }
                  break;

               case CT_NODE_POSITION:
                  {
                     S_vector pos = ck.RVectorChunk();
                     if(frm) frm->SetPos(pos);
                  }
                  break;

               case CT_NODE_ROTATION:
                  {
                     S_vectorw aa(0, 0, 0, 0);
                     ck.Read(&aa, sizeof(aa));
                     --ck;
                     if(frm)
                        frm->SetRot(S_quat(aa, aa.w));
                  }
                  break;

               case CT_NODE_SCALE:
                  {
                     S_vector s = ck.RVectorChunk();
                     if(s.x<0.0f || s.y<0.0f || s.z<0.0f){
                        REPORT_ERR(C_xstr("Frame '%': negative scale") %frm_name);
                        /*
                              //flip faces, if necessary
                        if(frm && frm->GetType()==FRAME_VISUAL && s.x*s.y*s.z < 0.0f){
                           PI3D_mesh_base mesh = I3DCAST_VISUAL(frm)->GetMesh();
                           if(mesh)
                              mesh->FlipFaces();
                        }
                        */
                     }
                     if(frm){
                        if(IsUniformScale(s) && s.x>=0.0f){
                           frm->SetScale(s.x);
                        }else{
                           float avg_scale = (I3DFabs(s.x) + I3DFabs(s.y) + I3DFabs(s.z)) / 3.0f;
                           frm->SetScale(avg_scale);
                           nu_scale = s / avg_scale;
                        }
                     }
                  }
                  break;

               case CT_TRACK_POS:
                  if(!ReadTrackPos(frm, frm_name, &anim))
                     return I3DERR_FILECORRUPTED;
                  --ck;
                  break;

               case CT_TRACK_ROT:
                  if(!ReadTrackRot(frm, frm_name, &anim))
                     return I3DERR_FILECORRUPTED;
                  --ck;
                  break;

               case CT_TRACK_SCL:
                  if(!ReadTrackScl(frm, frm_name, &anim, nu_scale))
                     return I3DERR_FILECORRUPTED;
                  --ck;
                  break;

               case CT_PIVOT_OFFSET:
                  pivot = ck.RVectorChunk();
                  break;

               case CT_TRACK_VISIBILITY:
                  if(!ReadTrackVis(frm, frm_name, &anim))
                     return I3DERR_FILECORRUPTED;
                  --ck;
                  break;

               case CT_TRACK_NOTE:
                  if(!ReadTrackNote(frm, frm_name, &anim))
                     return I3DERR_FILECORRUPTED;
                  --ck;
                  break;

               case CT_JOINT_WIDTH:
                  {
                     float sz = ck.RFloatChunk();
                     if(frm && frm->GetType()==FRAME_JOINT)
                        I3DCAST_JOINT(frm)->SetDrawSize(sz);
                  }
                  break;

               case CT_JOINT_LENGTH:
                  {
                     float sz = ck.RFloatChunk();
                     if(frm && frm->GetType()==FRAME_JOINT)
                        I3DCAST_JOINT(frm)->SetDrawLength(sz);
                  }
                  break;

               case CT_JOINT_ROT_OFFSET:
                  {
                     S_quat r = ck.RQuaternionChunk();
                     if(frm && frm->GetType()==FRAME_JOINT)
                        I3DCAST_JOINT(frm)->SetDrawRot(r);
                  }
                  break;

               case CT_DISPLAY_LINK:
                  --ck;
                  break;

               default:
                  REPORT_ERR(C_fstr("Unknown chunk: 0x%.4x", ct));
                  --ck;
               }
            }
            if(root_frame && frm){
                              //finalize frame
               switch(frm->GetType()){
               case FRAME_DUMMY:
                  {
                     //PI3D_dummy dum = I3DCAST_DUMMY(frm);
                     bbox.min *= nu_scale;
                     bbox.max *= nu_scale;
                     if(bbox_update){
                                          //setup bounding-box of joint
                        if(prnt){
                           const S_matrix &m = frm->GetLocalMatrix();
                           switch(bbox_update){
                           case PROP_CAST_JTBB:
                              if(prnt->GetType1()==FRAME_JOINT){
                                 if(!I3DCAST_JOINT(prnt)->AddRegion(bbox, m)){
                                    REPORT_ERR(C_fstr("Dummy '%s': failed to set bbox", (const char*)prnt->GetName1()));
                                 }
                              }
                              AddRegion(prnt, bbox, m, -1);
                              break;
                           case PROP_CAST_JTVB:
                              {
                                 bool ok = false;
                                 switch(prnt->GetType1()){
                                 case FRAME_JOINT:
                                    ok = I3DCAST_JOINT(prnt)->SetVolumeBox(bbox, m);
                                    break;
                                 case FRAME_VISUAL:
                                    ok = ((PI3D_visual)prnt)->AddRegion(bbox, m);
                                    AddRegion(prnt, bbox, m, -1);
                                    break;
                                 }
                                 if(!ok)
                                    REPORT_ERR(C_fstr("Frame '%s': failed to set vol box", (const char*)prnt->GetName1()));
                              }
                              break;
                           case PROP_REGION:
                              {
                                 bool ok = false;
                                 if(prnt){
                                    AddRegion(prnt, bbox, m, -1);
                                    ok = true;
                                    switch(prnt->GetType1()){
                                    case FRAME_VISUAL:
                                       ok = I3DCAST_VISUAL(prnt)->AddRegion(bbox, frm->GetLocalMatrix(), region_index);
                                       break;
                                    case FRAME_JOINT:
                                       I3DCAST_JOINT(prnt)->AddRegion(bbox, m, region_index);
                                       ok = true;
                                       break;
                                    }
                                 }
                                 if(!ok)
                                    REPORT_ERR(C_fstr("Frame '%s': failed to set region", (const char*)frm->GetName1()));
                              }
                              break;
                           }
                           frm = NULL;
                        }else{
                           REPORT_ERR(C_fstr("Dummy '%s': needs parent", (const char*)frm->GetName1()));
                        }
                     }else{
                        //dum->SetBBox(bbox);
                     }
                  }
                  break;

               case FRAME_VOLUME:
                  {
                     PI3D_volume vol = I3DCAST_VOLUME(frm);
                     bbox.max *= nu_scale;
                     const S_vector &base_scale = bbox.max;
                     float avg_scale = base_scale.Sum() / 3.0f;
                     vol->SetScale(avg_scale * vol->GetScale());
                     S_vector nu_scale = base_scale / avg_scale;

                     switch(vol->GetVolumeType()){
                     case I3DVOLUME_CAPCYL:
                        {
                                          //make x and y (radius) be compromise
                           nu_scale.x = nu_scale.y = (nu_scale.x+nu_scale.y) * .5f;
                                          //make z (half len) be lowered by radius
                           nu_scale.z = Max(0.0f, nu_scale.z-nu_scale.x);
                                          //make sure height is non-zero (which produces sphere volume)
                           if(IsMrgZeroLess(nu_scale.z)){
                              REPORT_ERR(C_xstr("Volume '%s': capped cylinder with zero height - use sphere instead") %vol->GetName1());
                           }
                        }
                        break;
                     }
                     vol->SetNUScale(nu_scale);
                  }
                  break;

               case FRAME_VISUAL:
                  {
                     PI3D_visual vis = I3DCAST_VISUAL(frm);
                     if(mesh_read){
                        PI3D_mesh_base mb = vis->GetMesh();
                        if(mb){
                           mb->ApplyPivotAndNUScale(pivot, nu_scale);
                                    //adjust err-report info
                           if(!pivot.IsNull() || nu_scale != S_vector(1.0f, 1.0f, 1.0f)){
                              t_visual_err_report::iterator it = visual_err_report.find(frm);
                              if(it!=visual_err_report.end()){
                                 C_vector<S_triangle> &tris = (*it).second;
                                 for(dword i=tris.size(); i--; ){
                                    for(dword j=3; j--; ){
                                       tris[i].v[j] -= pivot;
                                       tris[i].v[j] *= nu_scale;
                                    }
                                 }
                              }
                           }
                        }
                     }
                     if(al_data.use){
                        switch(vis->GetVisualType1()){
                        case I3D_VISUAL_SINGLEMESH:
                           assert(sm_data);
                           memcpy(&sm_data->al, &al_data, sizeof(al_data));
                           break;
                        default:
                           PI3D_mesh_base m = vis->GetMesh();
                           if(m){
                              m->CreateLODs(al_data.min_num_faces, al_data.num_parts, al_data.min_dist, al_data.max_dist, al_data.preserve_edges);
                           }else
                              REPORT_ERR(C_fstr("Frame '%s': unapplicable auto_lod", (const char*)frm->GetName1()));
                        }
                     }
                  }
                  break;
               case FRAME_JOINT:
                  {
                     PI3D_joint jnt = I3DCAST_JOINT(frm);
                     if(lock_joint) jnt->LockPosition(true);
                     if(break_joint) jnt->SetBreak(true);
                     if(jnt->GetHideDistance() && !break_joint){
                        REPORT_ERR(C_fstr("Frame '%s': setting hide distance without 'break' is meaningless", (const char*)frm->GetName1()));
                     }
                  }
                  break;
               }
               if(frm){
                  if(props.Size())
                     frm_properties[frm] = props;
               
                  if(procedural){
                                          //try to attach this procedural to visual object
                     bool st = I3DCAST_VISUAL(frm)->AttachProcedural(procedural);
                     if(!st)
                        REPORT_ERR(C_fstr("Frame '%s': failed to attach procedural", (const char*)frm->GetName1()));
                  }
                  if(name_dup_err.Size())
                     REPORT_ERR(C_xstr("Name duplication: '%'") %name_dup_err);
                  container->AddFrame(frm);
                  AddFrameID(node_id, frm);

                                 //add to hierarchy
                  if(!prnt){
                     frm->LinkTo(root_frame);
                  }else{
                     frm->LinkTo(prnt);
                  }
               }
            }
            if(anim){
               if(anim_set)
                  anim_set->AddAnimation(anim, frm_name);
               else
               if(scene)
                  scene->AddAnimation(anim, frm, I3DANIMOP_LOOP);
               anim->Release();
            }
            --ck;
         }
         break;

      case CT_KF_SEGMENT:
         {
            int seg[2];
            ck.Read(seg, sizeof(int)*2);
            anim_length = seg[1] * 1000 / ANIM_FPS;
            --ck;
         }
         break;

      default:
         REPORT_ERR(C_fstr("Unknown chunk: 0x%.4x", ct));
         --ck;
      }
   }

                              //finalize meshes
   for(t_vis_data::const_iterator it = vis_finish.begin(); it!=vis_finish.end(); it++){
      const S_SM_data &smd = (*it).second;
      PI3D_visual vis = (*it).first;

      if(vis->FinalizeLoad(*this, smd.mesh_ok, &smd.al)){
         if(!smd.mesh_ok){
            if(smd.allow_cache)
               vis_to_cache.push_back(vis);
         }
      }
   }

   if(smooth_list.size()){
#ifdef DEBUG_PROFILE_SMOOTH
      BegProf();
#endif
      SmoothSceneNormals(scene, smooth_list);
#ifdef DEBUG_PROFILE_SMOOTH
      driver->PRINT(C_fstr("SmoothSceneNormals: %.2f", EndProf()));
#endif
   }
                              //save all non-cached visuals to cache
   for(int i=vis_to_cache.size(); i--; ){
      PI3D_visual vis = vis_to_cache[i];
      C_fstr cache_name("%s+%s", (const char*)file_name, vis->GetOrigName());
      C_cache *cck = driver->dbase.CreateRecord(cache_name, file_time_);
      if(cck){
         vis->SaveCachedInfo(cck, materials);
         driver->dbase.CloseRecord(cck);
      }
   }

   if(scene && any_sector){
      S_buildsectors bs = {this, scene, NULL};
      root->EnumFrames(cbBuildSectors, (dword)&bs, ENUMF_VISUAL);
   }

                              //finalize reporting of geometry errors
   {
      t_visual_err_report::const_iterator it;
      for(it=visual_err_report.begin(); it!=visual_err_report.end(); it++){
         const S_matrix &m = (*it).first->GetMatrix();
         const C_vector<S_triangle> &tris = (*it).second;
         for(dword i=tris.size(); i--; ){
            const S_triangle &tri = tris[i];
            S_triangle t1;
            t1.v[0] = tri.v[0] * m;
            t1.v[1] = tri.v[1] * m;
            t1.v[2] = tri.v[2] * m;

            driver->DebugLine(t1.v[0], t1.v[1], 3, 0xffff0000);
            driver->DebugLine(t1.v[1], t1.v[2], 3, 0xffff0000);
            driver->DebugLine(t1.v[2], t1.v[0], 3, 0xffff0000);
         }
      }
   }

                              //validate materials - check if all are used
   for(i=materials.size(); i--; ){
      PI3D_material mat = materials[i];
      dword ref = (mat->AddRef(), mat->Release());
      if(ref <= 1){
         REPORT_ERR(C_fstr("Unused material: '%s'", (const char*)mat->GetName()));
      }
#ifdef DETECT_DUPLICATED_MATERIALS
      if(!strchr(mat->GetName(), '^'))
      for(int j=i; j--; ){
         CPI3D_material mat1 = materials[j];
         if(mat->GetDiffuse1()==mat1->GetDiffuse1() &&
            mat->GetAlpha1()==mat1->GetAlpha1() &&
            mat->GetAmbient1()==mat1->GetAmbient1() &&
            mat->GetEmissive1()==mat1->GetEmissive1() &&
            mat->GetPower1()==mat1->GetPower1() &&
            mat->Is2Sided1()==mat1->Is2Sided1() &&
            mat->GetDetailScale()==mat1->GetDetailScale() &&
            mat->GetEMBMScale()==mat1->GetEMBMScale() &&
            mat->GetMirrorID1()==mat1->GetMirrorID1() &&
            mat->GetEMBMOpacity1()==mat1->GetEMBMOpacity1() &&
            !strchr(mat1->GetName(), '^')){
            for(int i=MTI_LAST; i--; ){
               if(mat->GetTexture1((I3D_MATERIAL_TEXTURE_INDEX)i)!=mat1->GetTexture1((I3D_MATERIAL_TEXTURE_INDEX)i))
                  break;
            }
            if(i==-1)
               REPORT_ERR(C_fstr("Identical materials: '%s' and '%s'", (const char*)mat->GetName(), (const char*)mat1->GetName()));
         }
      }
#endif
   }
   return I3D_OK;
}

//----------------------------

                              //animation set loader

I3D_RESULT I3D_animation_set::Open(const char *fname, dword flags, PI3D_LOAD_CB_PROC cb_proc, void *cb_context){

   anims.clear();

   C_chunk ck;
   C_loader loader(drv, ck);
   I3D_RESULT ir = I3DERR_FILECORRUPTED;
   try{
      if(!ck.ROpen(fname))
         return I3DERR_NOFILE;
      if(!cb_proc)
         flags &= ~I3DLOAD_LOG;
      switch(++ck){
      case CT_BASE:
         ir = loader.Open(fname, flags, cb_proc, cb_context, NULL, NULL, this, NULL);
         break;
      }
   }catch(const C_except&){
   }
   /*
   ck.Close();

   if(I3D_SUCCESS(ir)){
                              //sync all times
      dword max_time = 0;
      dword i;
      for(i=0; i<anims.size(); i++)
         max_time = Max(max_time, ((PI3D_keyframe_anim)(PI3D_animation_base)anims[i].anim)->GetEndTime());
      for(i=0; i<anims.size(); i++)
         ((PI3D_keyframe_anim)(PI3D_animation_base)anims[i].anim)->SetEndTime(max_time);
   }
   */
   return ir;
}

//----------------------------
//----------------------------

I3D_RESULT I3D_container::OpenFromChunk(C_chunk &ck, dword load_flags, PI3D_LOAD_CB_PROC load_cb_proc,
   void *load_cb_context, PI3D_scene scene, PI3D_frame root, PI3D_animation_set anim_set, PI3D_driver drv){

   Close();

   if(load_flags&I3DLOAD_PROGRESS)
      load_cb_proc(CBM_PROGRESS, I3DFloatAsInt(0.0f), 0, load_cb_context);

   I3D_RESULT ir = I3DERR_FILECORRUPTED;
   try{
      if(!load_cb_proc){
         load_flags &= ~(I3DLOAD_LOG | I3DLOAD_PROGRESS);
      }
      switch(++ck){
      case CT_BASE:
         {
            C_loader loader(drv, ck);
            ir = loader.Open(filename, load_flags, load_cb_proc, load_cb_context, root, scene, anim_set, this);
            if(I3D_SUCCESS(ir))
               SetUserData(loader.user_comments);
         }
         break;
      }
      --ck;
   }catch(const C_except &){
   }
   if(load_flags&I3DLOAD_PROGRESS)
      load_cb_proc(CBM_PROGRESS, I3DFloatAsInt(1.0f), 2, load_cb_context);

   if(I3D_FAIL(ir))
      Close();

   return ir;
}

//----------------------------

I3D_RESULT I3D_container::Open(const char *filename1, dword load_flags, PI3D_LOAD_CB_PROC load_cb_proc,
   void *load_cb_context, PI3D_scene scene, PI3D_frame root, PI3D_animation_set anim_set, PI3D_driver drv){

   filename = filename1;
   C_chunk ck;
                              //open file
   if(!ck.ROpen(filename))
      return I3DERR_NOFILE;

   return OpenFromChunk(ck, load_flags, load_cb_proc, load_cb_context, scene, root, anim_set, drv);
}
//----------------------------
                              //scene loader
I3D_RESULT I3D_scene::Open(const char* fname, dword flags, PI3D_LOAD_CB_PROC cb_proc, void *cb_context){

   return cont.Open(fname, flags, cb_proc, cb_context, this, GetPrimarySector1(), NULL, drv);
}

//----------------------------
