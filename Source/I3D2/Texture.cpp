/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Texture.cpp
   Content: Texture class, loader.
--------------------------------------------------------*/

#include "all.h"
#include "texture.h"
#include "driver.h"
#include <integer.h>

#define NONPOW2_WARNING_ON    //issue warning when textures size isn't pow2

                              //problem: VooDoo cards
#define UPLOAD_IMMEDIATELY    //load textures to vidmem as they are created

#define USE_DATABASE          //try to cache computation-expensive info in database
#define DBASE_VERSION 0x0004


//#define DEBUG_LIMIT_TEXTURE_SIZE 512

//----------------------------

I3D_texture_base::I3D_texture_base(PI3D_driver d1):
   drv(d1),
   txt_flags(0),
   sort_id(d1->GetTextureSortID() & 0xffff)
{
}

//----------------------------

                              //texture class
I3D_texture::I3D_texture(PI3D_driver d):
   ref(1),
   I3D_texture_base(d),
   img_open_flags(0),
   last_render_time(0),
#ifdef GL
   txt_id(0),
#endif
   create_flags(0)
{
   drv->RegisterManagedTexture(this);
   drv->AddCount(I3D_CLID_TEXTURE);
}

//----------------------------

I3D_texture::~I3D_texture(){

   drv->UnregisterManagedTexture(this);
   drv->DecCount(I3D_CLID_TEXTURE);
#ifdef GL
   if(txt_id){
      glDeleteTextures(1, &txt_id);
   }
#endif
}

//----------------------------

IDirect3DBaseTexture9 *I3D_texture::GetD3DTexture() const{

   if(txt_flags&TXTF_UPLOAD_NEEDED)
      Manage(MANAGE_UPLOAD);
   last_render_time = drv->GetRenderTime();
   return d3d_tex;
}

//----------------------------
#ifdef GL
dword I3D_texture::GetGlId() const{
   return txt_id;
}
#endif
//----------------------------

bool I3D_texture::Match(const I3D_CREATETEXTURE &ct){

                              //flags must match
   const dword match_flags = TEXTMAP_DIFFUSE | TEXTMAP_OPACITY | TEXTMAP_EMBMMAP | TEXTMAP_NORMALMAP |
      TEXTMAP_TRANSP | /*TEXTMAP_INVALPHA | */TEXTMAP_CUBEMAP |
      TEXTMAP_NOMIPMAP |
#ifndef GL
      TEXTMAP_COMPRESS |
#endif
      TEXTMAP_HINTDYNAMIC | TEXTMAP_TRUECOLOR;
   if((create_flags&match_flags) != (ct.flags&match_flags))
      return false;

                              //check if diffuse name matches
   if(create_flags&(TEXTMAP_DIFFUSE | TEXTMAP_EMBMMAP | TEXTMAP_NORMALMAP)){
      if(create_flags&TEXTMAP_USE_CACHE){
         return false;
      }else{
         if(stricmp(file_names[0], ct.file_name))
            return false;
      }
   }
                              //check if opacity name matches
   if(create_flags&TEXTMAP_OPACITY){
      if(stricmp(file_names[1], ct.opt_name))
         return false;
   }
                              //all params match
   return true;
}

//----------------------------

bool I3D_texture::Reload(){

   if(!(create_flags&(TEXTMAP_DIFFUSE|TEXTMAP_OPACITY|TEXTMAP_EMBMMAP|TEXTMAP_NORMALMAP)))
      return false;
   if(create_flags&TEXTMAP_USEPIXELFORMAT)
      return false;
   I3D_CREATETEXTURE ct;
   memset(&ct, 0, sizeof(ct));
   ct.flags = create_flags;
   if(create_flags&(TEXTMAP_DIFFUSE|TEXTMAP_EMBMMAP|TEXTMAP_NORMALMAP)){
      if(create_flags&TEXTMAP_USE_CACHE)
         return false;
      else
         ct.file_name = file_names[0];
   }
   if(create_flags&TEXTMAP_OPACITY)
      ct.opt_name = file_names[1];
   I3D_RESULT ir = Open(ct, NULL, NULL, NULL);
   return I3D_SUCCESS(ir);
}

//----------------------------

I3D_RESULT I3D_texture::Open(const I3D_CREATETEXTURE &ct, I3D_LOAD_CB_PROC *cb_load, void *cb_context, C_str *err_msg){

   const char *fname = ct.file_name, *fname1 = ct.opt_name;
   dword flags = ct.flags;
   IGraph *ig = drv->GetGraphInterface();
   
   const D3DCAPS9 *caps = drv->GetCaps();

                              //check if textures supported by hardware
   if(!(caps->DevCaps&D3DDEVCAPS_TEXTURESYSTEMMEMORY) && !(caps->DevCaps&D3DDEVCAPS_TEXTUREVIDEOMEMORY) &&
      !(caps->DevCaps&D3DDEVCAPS_TEXTURENONLOCALVIDMEM))
      return I3DERR_TEXTURESNOTSUPPORTED;

                              //check mipmapping
   if(flags&(TEXTMAP_MIPMAP|TEXTMAP_NOMIPMAP)){
      dword mipmap_cap_bit = (flags&TEXTMAP_CUBEMAP) ? D3DPTEXTURECAPS_MIPCUBEMAP : D3DPTEXTURECAPS_MIPMAP;
      if(!(drv->GetFlags()&DRVF_LOADMIPMAP) || !(caps->TextureCaps&mipmap_cap_bit)){
         flags &= ~TEXTMAP_MIPMAP;
      }
   }
   bool empty = !(flags&(TEXTMAP_DIFFUSE|TEXTMAP_OPACITY|TEXTMAP_EMBMMAP|TEXTMAP_NORMALMAP));
   if(ct.flags&TEXTMAP_NO_SYSMEM_COPY){
      if(!empty && (ct.flags&TEXTMAP_MIPMAP))
         return I3DERR_INVALIDPARAMS;
   }

                              //close previous
   d3d_tex = NULL;
   vidmem_image = NULL;
   sysmem_image = NULL;
   txt_flags = 0;

   int i;

                              //get info about image
   dword img_sx, img_sy;
   byte src_bpp;

   C_cache *ck_d = NULL;
   C_cache *ck_o = NULL;
   C_str sfname_a;
   const char *sfname = NULL;
   __int64 file_time = 0, file_time_opacity = 0;

   if(!empty){
      bool use_name0 = (flags&(TEXTMAP_DIFFUSE|TEXTMAP_EMBMMAP|TEXTMAP_NORMALMAP));
      if(flags&TEXTMAP_USE_CACHE){    
         dword id;
         ck_d = use_name0 ? ct.ck_diffuse : ct.ck_opacity;
         if(!ig->GetImageInfo(*ck_d, img_sx, img_sy, src_bpp, id)){
            if(err_msg)
               *err_msg = C_fstr("unknown format");
            return false;
         }
      }else{
         const C_str &name = (use_name0 ? fname : fname1);
         i = drv->FindBitmap(name, sfname_a, img_sx, img_sy, src_bpp, (flags&TEXTMAP_CUBEMAP));
         if(!i){
            if(err_msg)
               *err_msg = C_fstr("file not found");
            return use_name0 ? I3DERR_NOFILE : I3DERR_NOFILE1;
         }
         sfname = sfname_a;
      }
   }else{
      img_sx = ct.size_x;
      img_sy = ct.size_y;
      src_bpp = 2;
      assert(img_sx && img_sy);
   }

#ifdef NONPOW2_WARNING_ON
   {
                              //check if dimensions power of 2
      if(cb_load && (CountBits(img_sx)!=1 || CountBits(img_sy)!=1)){
         cb_load(CBM_ERROR, (dword)(const char*)
            C_fstr(
            "Warning: texture dimensions not power of 2: '%s' (%ix%i)",
            (flags&TEXTMAP_DIFFUSE) ? fname : fname1, img_sx, img_sy),
            1, cb_context);
      }
   }
#endif

   C_str sfname1;

   if(flags&TEXTMAP_OPACITY){
      dword sx, sy;
      byte bpp;
                              //check if opacity map present
      if(flags&TEXTMAP_USE_CACHE){ //create from memory
         dword id;
         ck_o = ct.ck_opacity;
         if(!ig->GetImageInfo(*ck_o, sx, sy, bpp, id))
            return false;
      }else{
         i = drv->FindBitmap(fname1, sfname1, sx, sy, bpp, (flags&TEXTMAP_CUBEMAP));
         if(!i)
            return I3DERR_NOFILE1;
      }
   }

   memset(&pf, 0, sizeof(pf));

                              //choose texture format
   if(!(flags&TEXTMAP_USEPIXELFORMAT)){
      dword find_texture_flags = 0;

      if(flags&TEXTMAP_RENDERTARGET)
         find_texture_flags |= FINDTF_RENDERTARGET;

      if(flags&TEXTMAP_EMBMMAP){
         find_texture_flags |= FINDTF_EMBMMAP;
         if(flags&TEXTMAP_TRUECOLOR)
            find_texture_flags |= FINDTF_TRUECOLOR;
         if(flags&TEXTMAP_OPACITY)
            find_texture_flags |= FINDTF_ALPHA;

         I3D_RESULT ir = drv->FindTextureFormat(pf, find_texture_flags);
         if(I3D_FAIL(ir))
            return ir;
      }else{
         if((flags&TEXTMAP_OPACITY) || src_bpp==4){
                              //check if texture alpha and alpha-blending supported
            if((caps->TextureCaps&D3DPTEXTURECAPS_ALPHA) &&
               (caps->SrcBlendCaps&D3DPBLENDCAPS_SRCALPHA) &&
               (caps->DestBlendCaps&D3DPBLENDCAPS_INVSRCALPHA)){
               /*
               bool self_alpha = false;
               if((flags&TEXTMAP_DIFFUSE) && (flags&TEXTMAP_OPACITY))
                  self_alpha = ((flags&TEXTMAP_USE_CACHE) ? (ck_d == ck_o) : !strcmp(fname, fname1));
                              //check if same texture is used as opacity map
               if(self_alpha){
                  if(src_bpp==1)
                     find_texture_flags = FINDTF_PALETIZED;
                  txt_flags |= TXTF_SELF_ALPHA;
               }else{
               */
                  find_texture_flags = FINDTF_ALPHA;
                  if(src_bpp==4 && (flags&TEXTMAP_TRANSP) && !(flags&TEXTMAP_MIPMAP)){
                     find_texture_flags = FINDTF_ALPHA1;
                  }
               //}
            }else{ 
#ifndef GL
               if(src_bpp==1)
                  find_texture_flags = FINDTF_PALETIZED;
#endif
            }
         }else{
#ifndef GL
            if(src_bpp==1)
               find_texture_flags = FINDTF_PALETIZED;
#endif
         }
         if(flags&TEXTMAP_TRUECOLOR){
            find_texture_flags |= FINDTF_TRUECOLOR;
#ifndef GL
                              //forget compressed format
            //flags &= ~TEXTMAP_COMPRESS;
            find_texture_flags &= ~FINDTF_PALETIZED;
#endif
         }
#ifndef GL
         else
         if(flags&TEXTMAP_COMPRESS){
                              //may be compressed if both sides greater than 4
            if(img_sx>=4 && img_sy>=4 && !(flags&TEXTMAP_CUBEMAP))
               find_texture_flags |= FINDTF_COMPRESSED;
            //else
               //flags &= ~TEXTMAP_COMPRESS;
         }
#endif

         I3D_RESULT ir = drv->FindTextureFormat(pf, find_texture_flags);
         if(I3D_FAIL(ir))
            return ir;

                              //try find alpha format
                              //for colorkey substitution
         if(!(pf.flags&PIXELFORMAT_COMPRESS))   //can't be compressed format
         if(pf.bytes_per_pixel==2)              //can't trade paletized for this
         if((flags&TEXTMAP_TRANSP) && !(pf.flags&PIXELFORMAT_ALPHA) &&
            (caps->TextureCaps&D3DPTEXTURECAPS_ALPHA)){
                              //try to find alpha-format,
                              // 1-bit for non-mipmaps, wider for mipmaps
            S_pixelformat pf1;
            ir = drv->FindTextureFormat(pf1, (flags&TEXTMAP_MIPMAP) ? FINDTF_ALPHA : FINDTF_ALPHA1);
            if(SUCCEEDED(ir) && pf1.bytes_per_pixel<=pf.bytes_per_pixel &&
               (pf1.flags&PIXELFORMAT_ALPHA) && !(pf1.flags&PIXELFORMAT_PALETTE)){

                              //use alpha-format for colorkeying
               pf = pf1;
            }
         }
#ifndef GL
                              //compressed mipmaps don't go below side size of 4 texels
         if((flags&TEXTMAP_MIPMAP) && (flags&TEXTMAP_COMPRESS)){
            if(img_sx<=4 || img_sy<=4)
               flags &= ~TEXTMAP_MIPMAP;
         }
#endif
                              //no color-keyed mipmaps
         if((flags&TEXTMAP_TRANSP) &&
            //!(flags&TEXTMAP_COMPRESS) &&
            !(pf.flags&PIXELFORMAT_ALPHA) &&
            (flags&TEXTMAP_MIPMAP)){
                              //try find alpha format
                              //for colorkey substitution
            dword ftf = 0;//find_texture_flags & FINDTF_TRUECOLOR;
            S_pixelformat pf1;
            ir = drv->FindTextureFormat(pf1,
               //((flags&TEXTMAP_MIPMAP) ? FINDTF_ALPHA : FINDTF_ALPHA1) |
               FINDTF_ALPHA |
#ifndef GL
               ((flags&TEXTMAP_COMPRESS) ? FINDTF_COMPRESSED : 0) |
#endif
               ftf
               );
            if(SUCCEEDED(ir) && //pf1.bytes_per_pixel==2 &&
               (pf1.flags&PIXELFORMAT_ALPHA) &&
               !(pf1.flags&PIXELFORMAT_PALETTE))
               pf = pf1;
            else
               flags &= ~TEXTMAP_MIPMAP;
         }
      }
   }else{
      if(!ct.pf)
         return I3DERR_INVALIDPARAMS;
      pf = *ct.pf;
   }

   bool compressed = (pf.flags&PIXELFORMAT_COMPRESS);
                              //compressed mipmaps are created in non-remapped images, put to mipmap at the end
   bool compress_postpone = (compressed && (flags&TEXTMAP_MIPMAP));

                              //get texture min/max info
   dword mintx = 1;
   dword minty = 1;
   dword maxtx = caps->MaxTextureWidth  ? caps->MaxTextureWidth  : 256;
   dword maxty = caps->MaxTextureHeight ? caps->MaxTextureHeight : 256;
#ifdef DEBUG_LIMIT_TEXTURE_SIZE
   maxtx = maxty = DEBUG_LIMIT_TEXTURE_SIZE;
#endif

   /*
                              //check if wide surfaces supported
   if(!(drv->GetDDCaps()->dwCaps2&DDCAPS2_WIDESURFACES)){
                              //limit max texture width/height to width of current screen mode
      PIGraph igraph = ig;
      dword sx = igraph->Scrn_sx();
      dword max_width = 1;
      while(max_width<sx) max_width <<= 1;
      max_width >>= 1;
      maxtx = Min(maxtx, max_width);
      maxty = Min(maxty, max_width);
   }
   */

   img_open_flags = 0;
   dword req_sx = img_sx, req_sy = img_sy;
                              //compute destination resolution
   if(caps->TextureCaps&D3DPTEXTURECAPS_POW2){
      size_x = mintx;
      while(size_x < req_sx){
         if(size_x >= req_sx - size_x/2) break;
         if(size_x >= maxtx) break;
         size_x *= 2;
      }
      size_y = minty;
      while(size_y < req_sy){
         if(size_y >= req_sy - size_y/2) break;
         if(size_y >= maxty) break;
         size_y *= 2;
      }
   }else{
      size_x = Max(mintx, Min(maxtx, req_sx));
      size_y = Max(minty, Min(maxty, req_sy));
   }

   t_type = TEXTURE_2D;
   if(flags&TEXTMAP_CUBEMAP){
      img_open_flags |= IMGOPEN_CUBE_TEXTURE;
      t_type = TEXTURE_CUBIC;
   }

   if(caps->TextureCaps&D3DPTEXTURECAPS_SQUAREONLY){

      dword more = Max(size_x, size_y);
      dword less = Min(size_x, size_y);
      while(less < more){
         less *= 2;
         more = Max(more/2, 1ul);
      }
      size_y = size_x = more;
   }

   dword num_mip_levels = 0;

   if(flags&TEXTMAP_MIPMAP){
      img_open_flags |= IMGOPEN_MIPMAP;
      dword min_size = Max(mintx, minty);
      if(pf.flags&PIXELFORMAT_COMPRESS)
         min_size = Max(4, (int)min_size);
      for(dword sx=size_x>>1, sy=size_y>>1; sx >= min_size && sy >= min_size; ){
         ++num_mip_levels;
         sx >>= 1;
         sy >>= 1;
      }
   }else
   if(flags&TEXTMAP_NOMIPMAP){
      img_open_flags |= IMGOPEN_MIPMAP;
   }

                              //if stretching necessary, postpone compression
   if(compressed){
      if(size_x!=img_sx || size_y!=img_sy)
         compress_postpone = true;
   }

   S_pixelformat pf_compress;
   if(compressed && compress_postpone){
                              //setup 8-8-8-8 format
      pf_compress.bytes_per_pixel = 4;
      pf_compress.r_mask = 0x00ff0000;
      pf_compress.g_mask = 0x0000ff00;
      pf_compress.b_mask = 0x000000ff;

      pf_compress.a_mask = 0xff000000;
      pf_compress.flags = PIXELFORMAT_ALPHA;
   }
   if(flags&TEXTMAP_HINTDYNAMIC){
      if(drv->GetCaps()->Caps2&D3DCAPS2_DYNAMICTEXTURES)
         img_open_flags |= IMGOPEN_HINTDYNAMIC;
   }

   if(flags&TEXTMAP_TRANSP)
      img_open_flags |= IMGOPEN_INIT_CKEY;

   struct S_db_cache_header{
      dword version;
      S_pixelformat pixel_format;
      __int64 file_time_opacity;
      dword size_x, size_y;
      dword num_levels;                                 
      dword txt_flags;        //I3D_texture_base::txt_flags
      dword ckey_info_size;
      dword open_flags;
   };
   C_str cache_name;

   if(!(ct.flags&TEXTMAP_NO_SYSMEM_COPY)){
      sysmem_image = ig->CreateImage();
      if(!sysmem_image)
         return I3DERR_OUTOFMEM;
      sysmem_image->Release();

#ifdef USE_DATABASE
                              //try to open cached version now
      if(!empty && !(flags&TEXTMAP_USE_CACHE)){
         file_time = 0;
         file_time_opacity = 0;
         if(sfname){
            PC_dta_stream dta = DtaCreateStream(sfname);
            if(dta){
               dta->GetTime((dword*)&file_time);
               dta->Release();
            }
         }
         if(sfname1.Size()){
            PC_dta_stream dta = DtaCreateStream(sfname1);
            if(dta){
               dta->GetTime((dword*)&file_time_opacity);
               dta->Release();
               if(!sfname)
                  file_time = file_time_opacity;
            }
         }
         cache_name = C_fstr("%s (%s) [%i %i%i%i%i]", sfname ? sfname : "", (const char*)sfname1,
            pf.bytes_per_pixel, CountBits(pf.a_mask), CountBits(pf.r_mask), CountBits(pf.g_mask), CountBits(pf.b_mask));
         C_cache *ck = drv->dbase.OpenRecord(cache_name, file_time);
         if(ck){
            bool read_ok = false;
                              //read header
            S_db_cache_header hdr;
            if(ck->read(&hdr, sizeof(hdr))==sizeof(hdr) && hdr.version == DBASE_VERSION){
               const dword MATCH_FLAGS = TEXTMAP_DIFFUSE | TEXTMAP_OPACITY | TEXTMAP_EMBMMAP | TEXTMAP_CUBEMAP | TEXTMAP_TRANSP | TEXTMAP_TRUECOLOR | TEXTMAP_NORMALMAP;
               if(!memcmp(&hdr.pixel_format, &pf, sizeof(S_pixelformat)) && hdr.size_x==size_x && hdr.size_y==size_y &&
                  hdr.num_levels==num_mip_levels+1 &&
                  ((flags&MATCH_FLAGS) == (hdr.open_flags&MATCH_FLAGS))){

                  if(!sfname1.Size() || file_time_opacity==hdr.file_time_opacity){

                              //create empty image
                     i = sysmem_image->Open(NULL, IMGOPEN_EMPTY | IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | img_open_flags,
                        size_x, size_y, &pf, num_mip_levels);
                     if(i){
                        IDirect3DTexture9 *dxtex = sysmem_image->GetDirectXTexture();
                        if(dxtex && hdr.num_levels == dxtex->GetLevelCount()){
                           read_ok = true;
                              //write all levels
                           dword sx = hdr.size_x;
                           dword sy = hdr.size_y;
                           for(dword li=0; li<hdr.num_levels; li++){
                              D3DLOCKED_RECT lrc;
                              if(SUCCEEDED(dxtex->LockRect(li, &lrc, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY))){
                                          //write raw data
                                 if(pf.flags&PIXELFORMAT_COMPRESS){
                                    dword sz = sx*2;
                                    if(pf.four_cc>'1TXD')
                                       sz *= 2;
                                    if((dword)lrc.Pitch==sz){
                                       sz *= sy/4;
                                       if(ck->read(lrc.pBits, sz)!=sz){
                                          read_ok = false;
                                          break;
                                       }
                                    }else
                                    for(dword y=0; y<sy/4; y++){
                                       if(ck->read(lrc.pBits, sz)!=sz){
                                          read_ok = false;
                                          break;
                                       }
                                       lrc.pBits = ((byte*)lrc.pBits) + lrc.Pitch;
                                    }
                                 }else{
                                    dword sz = sx*hdr.pixel_format.bytes_per_pixel;
                                    if((dword)lrc.Pitch==sz){
                                       sz *= sy;
                                       if(ck->read(lrc.pBits, sz)!=sz){
                                          read_ok = false;
                                          break;
                                       }
                                    }else
                                    for(dword y=0; y<sy; y++){
                                       if(ck->read(lrc.pBits, sz)!=sz){
                                          read_ok = false;
                                          break;
                                       }
                                       lrc.pBits = ((byte*)lrc.pBits) + lrc.Pitch;
                                    }
                                 }
                                 dxtex->UnlockRect(li);
                              }else{
                                 read_ok = false;
                              }
                              if(!read_ok)
                                 break;
                              sx >>= 1;
                              sy >>= 1;
                           }
                           if(read_ok && hdr.ckey_info_size){
                              C_vector<byte> ckey; ckey.resize(hdr.ckey_info_size);
                              ck->read(&ckey.front(), hdr.ckey_info_size);
                              sysmem_image->SetColorkeyInfo(&ckey.front(), hdr.ckey_info_size);
                           }
                        }
                     }
                  }
               }
            }
            drv->dbase.CloseRecord(ck);
            if(read_ok){
               txt_flags = hdr.txt_flags;
               goto read_finish;
            }
         }
      }
#endif
      if(flags&TEXTMAP_EMBMMAP){
         assert(!compress_postpone);
                                 //open empty bumpmap texture
         i = sysmem_image->Open(NULL, IMGOPEN_EMPTY | IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | img_open_flags,
            size_x, size_y, &pf, num_mip_levels, err_msg);
         if(!i){
            sysmem_image = NULL;
            return I3DERR_OUTOFMEM;
         }
                                 //open temp image
         PIImage tmp_pic = ig->CreateImage();
         if(!tmp_pic){
            sysmem_image = NULL;
            return I3DERR_OUTOFMEM;
         }
         i = tmp_pic->Open(sfname,
            (img_open_flags&IMGOPEN_NOREMAP) |
            IMGOPEN_NOREMAP |
            IMGOPEN_SYSMEM,
            0, 0, NULL, 0, err_msg);
         if(!i){
            tmp_pic->Release();
            sysmem_image = NULL;
            return I3DERR_NOFILE;
         }
         sysmem_image->ConvertBumpMap(tmp_pic);

         tmp_pic->Release();
         tmp_pic = NULL;
      }else
      if(flags&TEXTMAP_NORMALMAP){
         if(size_x!=img_sx || size_y!=img_sy){
            return I3DERR_GENERIC;
         }
                              //open empty normalmap texture
         i = sysmem_image->Open(NULL, IMGOPEN_EMPTY | IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | img_open_flags,
            size_x, size_y, compress_postpone ? &pf_compress : &pf,
            num_mip_levels, err_msg);
         if(!i){
            sysmem_image = NULL;
            return I3DERR_OUTOFMEM;
         }
                              //open temp image
         PIImage tmp_pic = ig->CreateImage();
         if(!tmp_pic){
            sysmem_image = NULL;
            return I3DERR_OUTOFMEM;
         }
         i = tmp_pic->Open(sfname,
            (img_open_flags&IMGOPEN_NOREMAP) |
            IMGOPEN_NOREMAP |
            IMGOPEN_SYSMEM,
            0, 0, NULL, 0, err_msg);
         if(!i){
            tmp_pic->Release();
            sysmem_image = NULL;
            return I3DERR_NOFILE;
         }
         sysmem_image->ConvertToNormalMap(tmp_pic, 4.0f);

         tmp_pic->Release();
         tmp_pic = NULL;
      }else
      if(!(flags&TEXTMAP_DIFFUSE)){
                              //empty image
         i = sysmem_image->Open(sfname,
            IMGOPEN_EMPTY | IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | img_open_flags,
            size_x, size_y,
            compress_postpone ? &pf_compress : &pf,
            num_mip_levels, err_msg);
         if(!i){
            sysmem_image = NULL;
            return I3DERR_NOFILE;
         }
         if(flags&TEXTMAP_OPACITY){
                              //fill texture by white
            byte *area;
            dword pitch;
            dword bpp = sysmem_image->GetPixelFormat()->bytes_per_pixel;
            i = sysmem_image->Lock((void**)&area, &pitch);
            if(!i){
               sysmem_image = NULL;
               return I3DERR_GENERIC;
            }
            if((compress_postpone ? pf_compress : pf).flags&PIXELFORMAT_COMPRESS){
                                 //fill linear compressed surface
               memset(area, -1, pitch);
            }else{
                                 //fill line-by-line
               for(int y=size_y; y--; area += pitch)
                  memset(area, -1, bpp*size_x);
            }
            sysmem_image->Unlock();
         }
      }else
      if(size_x!=img_sx || size_y!=img_sy){
                                 //stretch source image into proper size
         PIImage tmp_pic = ig->CreateImage();
         if(!tmp_pic){
            sysmem_image = NULL;
            return I3DERR_OUTOFMEM;
         }
                                 //open temp image
         i = tmp_pic->Open(sfname,
            (img_open_flags&IMGOPEN_NOREMAP) |
            IMGOPEN_SYSMEM,
            0, 0,
            compress_postpone ? &pf_compress : &pf, 0, err_msg);
         if(!i){
            tmp_pic->Release();
            sysmem_image = NULL;
            return I3DERR_NOFILE;
         }
         if(compress_postpone)
            pf_compress = *tmp_pic->GetPixelFormat();
                                 //open empty image
         i = sysmem_image->Open(NULL,
            IMGOPEN_EMPTY | IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | img_open_flags, size_x, size_y,
            compress_postpone ? &pf_compress : &pf, num_mip_levels, err_msg);
         if(!i){
            tmp_pic->Release();
            sysmem_image = NULL;
            return I3DERR_OUTOFMEM;
         }
         sysmem_image->CopyStretched(tmp_pic);

         sysmem_image->SetPal(tmp_pic->GetPal());
         tmp_pic->Release();
         tmp_pic = NULL;
      }else{
                                 //open texture picture directly
         dword imgopen_flags = img_open_flags;
         if(!(flags&TEXTMAP_TRANSP) && (drv->GetFlags()&DRVF_TEXTDITHER))
            imgopen_flags |= IMGOPEN_DITHER;
         if(flags&TEXTMAP_TRANSP)
            imgopen_flags |= IMGOPEN_COLORKEY;
         const S_pixelformat *imgopen_pf = compress_postpone ? &pf_compress : &pf;
         if(!(flags&TEXTMAP_USE_CACHE)){
            i = sysmem_image->Open(sfname, IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | imgopen_flags,
               size_x, size_y, imgopen_pf, num_mip_levels, err_msg);
         }else{
            i = sysmem_image->Open(*ck_d, IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | imgopen_flags,
               0, 0, imgopen_pf, num_mip_levels, err_msg);
         }
         if(!i){
            sysmem_image = NULL;
            return I3DERR_NOFILE;
         }
      }

      if(flags&TEXTMAP_OPACITY){
         /*
         if(txt_flags&TXTF_SELF_ALPHA){
            txt_flags |= TXTF_ALPHA;
         }else*/
         {
                                 //mix opacity map into texture
            PIImage pic_opt = ig->CreateImage();
            if(pic_opt){
               if(!(flags&TEXTMAP_USE_CACHE)){
                  i = pic_opt->Open(sfname1, IMGOPEN_NOREMAP | IMGOPEN_SYSMEM, 0, 0, NULL, 0, err_msg);
               }else{
                  i = pic_opt->Open(*ck_o, IMGOPEN_NOREMAP | IMGOPEN_SYSMEM | TEXTMAP_USE_CACHE, 0, 0, NULL, 0, err_msg);
               }
               if(i){
                  if(pic_opt->SizeX()!=size_x || pic_opt->SizeY()!=size_y){
                                 //stretch to required size
                     PIImage tmp_pic = ig->CreateImage();
                     if(!tmp_pic){
                        sysmem_image = NULL;
                        pic_opt->Release();
                        return I3DERR_OUTOFMEM;
                     }
                     i = tmp_pic->Open(NULL,
                        IMGOPEN_SYSMEM | IMGOPEN_EMPTY | IMGOPEN_NOREMAP,
                        size_x, size_y, pic_opt->GetPixelFormat(), 0, err_msg);
                     if(!i){
                        tmp_pic->Release();
                        sysmem_image = NULL;
                        pic_opt->Release();
                        return I3DERR_OUTOFMEM;
                     }
                     tmp_pic->CopyStretched(pic_opt);

                     tmp_pic->SetPal(pic_opt->GetPal());
                     pic_opt->Release();
                     pic_opt = tmp_pic;
                     tmp_pic = NULL;
                  }
                  if((pf.flags&PIXELFORMAT_ALPHA) || ((pf.flags&PIXELFORMAT_BUMPMAP) && pf.l_mask)){
                     dword blend_flags = 0;
                     if(flags&TEXTMAP_EMBMMAP)
                        blend_flags |= IMGBLEND_INVALPHA;
                     if(!(flags&TEXTMAP_TRANSP) && (drv->GetFlags()&DRVF_TEXTDITHER))
                        blend_flags |= IMGBLEND_DITHER;
                     i = sysmem_image->AlphaBlend(pic_opt, blend_flags);
                                 //success, can be alpha
                     if(i)
                        txt_flags |= TXTF_ALPHA;
                  }
               }else{
                                 //some problem, report to user
                  if(cb_load){
                     cb_load(CBM_ERROR, (dword)(const char*)
                        C_fstr("Failed to load texture: '%s'", (const char*)sfname1),
                        1, cb_context);
                  }
               }
               pic_opt->Release();
               pic_opt = NULL;
            }
         }
      }else
      if(src_bpp==4){
         txt_flags |= TXTF_ALPHA;
      }

      if(flags&TEXTMAP_MIPMAP){
         txt_flags |= TXTF_MIPMAP;
         sysmem_image->MixMipmap(0);
      }

      if(compress_postpone){
         PIImage tmp_img = ig->CreateImage();
         if(!tmp_img)
            return I3DERR_OUTOFMEM;

         i = tmp_img->Open(sfname, IMGOPEN_EMPTY | IMGOPEN_TEXTURE | IMGOPEN_SYSMEM | img_open_flags,
            size_x, size_y, &pf, num_mip_levels, err_msg);
         assert(i);

         IDirect3DTexture9 *src = sysmem_image->GetDirectXTexture();
         assert(src);
         IDirect3DTexture9 *dst = tmp_img->GetDirectXTexture();
         assert(dst);
                                 //use convertor
         LPC_rgb_conversion rgb_conv = ig->CreateRGBConv();
         rgb_conv->Init(pf);
         int sx = size_x, sy = size_y;

         dword conv_flags = MCONV_DITHER;

         dword num_levels = src->GetLevelCount();
         for(dword i=0; i<num_levels; i++){
            D3DLOCKED_RECT rc_src;
            D3DLOCKED_RECT rc_dst;

            HRESULT hr = src->LockRect(i, &rc_src, NULL, D3DLOCK_READONLY);
            CHECK_D3D_RESULT("LockRect", hr);
            if(FAILED(hr))
               break;
            hr = dst->LockRect(i, &rc_dst, NULL, 0);
            CHECK_D3D_RESULT("LockRect", hr);
            if(FAILED(hr)){
               src->UnlockRect(i);
               break;
            }

            conv_flags &= ~MCONV_COLORKEY;
            if(flags&TEXTMAP_TRANSP)
               conv_flags |= MCONV_COLORKEY;
            rgb_conv->Convert(rc_src.pBits, rc_dst.pBits, sx, sy,
               rc_src.Pitch, 0, (byte)sysmem_image->GetPixelFormat()->bytes_per_pixel, NULL, conv_flags,
               0);

            src->UnlockRect(i);
            dst->UnlockRect(i);

            sx >>= 1;
            sy >>= 1;
         }
         rgb_conv->Release();
         tmp_img->CopyColorkeyInfo(sysmem_image);
         sysmem_image = tmp_img;
         tmp_img->Release();
      }
   }

   if(flags&TEXTMAP_TRANSP){
      if((pf.flags&PIXELFORMAT_COMPRESS) || (pf.flags&PIXELFORMAT_ALPHA))
         txt_flags |= TXTF_CKEY_ALPHA;
   }

   if(flags&TEXTMAP_RENDERTARGET)
      img_open_flags |= IMGOPEN_RENDERTARGET;

   if(!(flags&TEXTMAP_HINTDYNAMIC))
      img_open_flags |= IMGOPEN_HINTSTATIC;

#ifdef USE_DATABASE
   if(cache_name.Size()){
      assert(sysmem_image);

      IDirect3DTexture9 *dxtex = sysmem_image->GetDirectXTexture();
      if(dxtex){
         S_db_cache_header hdr;
         hdr.size_x = sysmem_image->SizeX();
         hdr.size_y = sysmem_image->SizeY();
         hdr.num_levels = dxtex->GetLevelCount();
                              //try to save data into cache
         dword bpp = pf.bytes_per_pixel;
         if(pf.flags&PIXELFORMAT_COMPRESS)
            bpp = 1;
         C_cache *ck = drv->dbase.CreateRecord(cache_name, file_time, hdr.size_x*hdr.size_y*bpp*2);
         if(ck){
            bool save_ok = false;

            hdr.version = DBASE_VERSION;
            hdr.pixel_format = *sysmem_image->GetPixelFormat();
            hdr.file_time_opacity = file_time_opacity;
            hdr.txt_flags = txt_flags;
            hdr.open_flags = flags;
            hdr.ckey_info_size = 0;
            const void *ckey = sysmem_image->GetColorkeyInfo(&hdr.ckey_info_size);

            if(ck->write(&hdr, sizeof(hdr))){
               save_ok = true;
                              //write all levels
               dword sx = hdr.size_x;
               dword sy = hdr.size_y;
               for(dword li=0; li<hdr.num_levels; li++){
                  D3DLOCKED_RECT lrc;
                  if(SUCCEEDED(dxtex->LockRect(li, &lrc, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY))){
                              //write raw data
                     if(pf.flags&PIXELFORMAT_COMPRESS){
                        dword sz = sx*2;
                        if(pf.four_cc>'1TXD')
                           sz *= 2;
                        if((dword)lrc.Pitch==sz){
                           sz *= sy/4;
                           ck->write(lrc.pBits, sz);
                        }else
                        for(dword y=0; y<sy/4; y++){
                           ck->write(lrc.pBits, sz);
                           lrc.pBits = ((byte*)lrc.pBits) + lrc.Pitch;
                        }
                     }else{
                        dword sz = sx*hdr.pixel_format.bytes_per_pixel;
                        if((dword)lrc.Pitch==sz){
                           sz *= sy;
                           ck->write(lrc.pBits, sz);
                        }else
                        for(dword y=0; y<sy; y++){
                           ck->write(lrc.pBits, sz);
                           lrc.pBits = ((byte*)lrc.pBits) + lrc.Pitch;
                        }
                     }
                     dxtex->UnlockRect(li);
                  }else{
                     save_ok = false;
                     break;
                  }
                  sx >>= 1;
                  sy >>= 1;
               }
               if(save_ok && hdr.ckey_info_size)
                  ck->write(ckey, hdr.ckey_info_size);
            }
            drv->dbase.CloseRecord(ck);
            if(!save_ok)
               drv->dbase.EraseRecord(cache_name);
         }
      }
   }
read_finish:
#endif

   SetDirty();
#ifdef UPLOAD_IMMEDIATELY
   if(!empty){
      Manage(MANAGE_UPLOAD_IF_FREE);   //try to get onto video card, if there's some space
   }
#endif

   if(flags&(TEXTMAP_DIFFUSE|TEXTMAP_EMBMMAP|TEXTMAP_NORMALMAP)){
      if(flags&TEXTMAP_USE_CACHE){
         //assert(0);
         //filenames[0].Assign((const char*)&fname, sizeof(fname));
      }else
         file_names[0] = fname;
   }
   file_names[1] = (flags&TEXTMAP_OPACITY) ? fname1 : NULL;
   create_flags = flags;
   file_names[0].ToLower();
   file_names[1].ToLower();

   return I3D_OK;
}

//----------------------------

void I3D_texture::Manage(E_manage_cmd cmd) const{

   HRESULT hr;

   switch(cmd){
   case MANAGE_FREE:
      if(vidmem_image){
         vidmem_image = NULL;
         d3d_tex = NULL;
                              //mark source dirty, so that it is updated next time
         if(sysmem_image)
            sysmem_image->GetDirectXTexture()->AddDirtyRect(NULL);
         SetDirty();
      }
#ifdef GL
      if(txt_id){
         glDeleteTextures(1, &txt_id);
         txt_id = 0;
      }
#endif
      break;
   case MANAGE_UPLOAD:
   case MANAGE_UPLOAD_IF_FREE:
   case MANAGE_CREATE_ONLY:
      if(txt_flags&TXTF_UPLOAD_NEEDED){

         int level_count = 1;
         if(sysmem_image){
            level_count = (img_open_flags&IMGOPEN_CUBE_TEXTURE) ? sysmem_image->GetDirectXCubeTexture()->GetLevelCount() : sysmem_image->GetDirectXTexture()->GetLevelCount();
         }
         if(!vidmem_image){
            vidmem_image = drv->GetGraphInterface()->CreateImage();
            vidmem_image->Release();

                              //open the image
            int i = vidmem_image->Open(NULL, IMGOPEN_TEXTURE | IMGOPEN_EMPTY | IMGOPEN_VIDMEM |
               img_open_flags, size_x, size_y, &GetPixelFormat(), level_count-1);
            if(!i){
                              //let it be, if there's not enough space
               if(cmd==MANAGE_UPLOAD_IF_FREE){
                  vidmem_image = NULL;
                  d3d_tex = NULL;
                  break;
               }
               const C_vector<PI3D_texture> &mts = drv->GetManagedTextures();
                              //failed to create empty image, unload some texture(s)
                              //1. build sorted list of current cached textures
               C_sort_list<PI3D_texture> slist;
               slist.Reserve(mts.size()-1); //for all except ours
               dword render_time = drv->GetRenderTime();
               for(i=mts.size(); i--; ){
                  PI3D_texture tp = mts[i];
                  if(!(tp->txt_flags&TXTF_UPLOAD_NEEDED) && tp!=this && tp->vidmem_image)
                     slist.Add(tp, render_time - tp->last_render_time);
               }
               slist.Sort();
                              //2. step through oldiest, try to reuse, or release as necessary
               for(i=slist.Count(); i--; ){
                  PI3D_texture tp = slist[i];
                  int tp_level_count = 1;
                  if(tp->sysmem_image){
                     tp_level_count = (img_open_flags&IMGOPEN_CUBE_TEXTURE) ? tp->sysmem_image->GetDirectXCubeTexture()->GetLevelCount() : tp->sysmem_image->GetDirectXTexture()->GetLevelCount();
                  }
                              //check if we may use its surface directly
                  if(tp->vidmem_image &&
                     tp->size_x==size_x && tp->size_y==size_y &&
                     (tp->img_open_flags&~IMGOPEN_INIT_CKEY)==(img_open_flags&~IMGOPEN_INIT_CKEY) &&
                     tp_level_count == level_count &&
                     !memcmp(&tp->GetPixelFormat(), &GetPixelFormat(), sizeof(S_pixelformat))){
                              //great, that's our texture, reuse it
                     vidmem_image = tp->vidmem_image;
                     tp->vidmem_image = NULL;
                     d3d_tex = tp->d3d_tex;
                     tp->d3d_tex = NULL;

                     tp->SetDirty();
                     break;
                  }
                              //unload the texture
                  tp->Manage(MANAGE_FREE);
                  /*
                  if(!vidmem_image){
                     __asm mov eax, 0xdddddddd
                     __asm int 3;
                  }
                  */
                              //try to open image now
                  bool b = vidmem_image->Open(NULL, IMGOPEN_TEXTURE | IMGOPEN_EMPTY | IMGOPEN_VIDMEM |
                     img_open_flags, size_x, size_y, &GetPixelFormat(), level_count-1);
                  if(b)
                     break;
               }
               if(i==-1){
                              //error: cannot find free space, we're without texture
                  vidmem_image = NULL;
                  d3d_tex = NULL;
                  txt_flags &= ~TXTF_UPLOAD_NEEDED;
                  break;
               }
            }
            if(!d3d_tex){
               d3d_tex = (img_open_flags&IMGOPEN_CUBE_TEXTURE) ?
                  (IDirect3DBaseTexture9*)vidmem_image->GetDirectXCubeTexture() :
                  (IDirect3DBaseTexture9*)vidmem_image->GetDirectXTexture();
               assert(d3d_tex);
            }
            if(sysmem_image){
                              //mark dirty rectangle (entire area)
               if(img_open_flags&IMGOPEN_CUBE_TEXTURE){
                  IDirect3DCubeTexture9 *ct = sysmem_image->GetDirectXCubeTexture();
                  for(int i=0; i<6; i++)
                     ct->AddDirtyRect((D3DCUBEMAP_FACES)i, NULL);
               }else
                  sysmem_image->GetDirectXTexture()->AddDirtyRect(NULL);
            }
         }
         if(sysmem_image && cmd!=MANAGE_CREATE_ONLY){
                              //Load surface(s)
            assert(sysmem_image && vidmem_image);
            IDirect3DBaseTexture9 *src = (img_open_flags&IMGOPEN_CUBE_TEXTURE) ? (IDirect3DBaseTexture9*)sysmem_image->GetDirectXCubeTexture() : sysmem_image->GetDirectXTexture();
            hr = drv->GetDevice1()->UpdateTexture(src, d3d_tex);
            CHECK_D3D_RESULT("UpdateTexture", hr);
         }
         txt_flags &= ~TXTF_UPLOAD_NEEDED;
#ifdef GL
         if(!txt_id){
            glGenTextures(1, &txt_id);
            glBindTexture(GL_TEXTURE_2D, txt_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (txt_flags&TXTF_MIPMAP) && drv->GetState(RS_MIPMAP) ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, drv->GetState(RS_LINEARFILTER) ? GL_LINEAR : GL_NEAREST);

            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
         }
         if(txt_id && sysmem_image){
            void *mem;
            dword pitch;
            sysmem_image->Lock(&mem, &pitch, true);
            const S_pixelformat *pf = sysmem_image->GetPixelFormat();

            int gl_type = 0;
            int gl_fmt = GL_RGBA;
            switch(pf->bytes_per_pixel){
            case 4:
               assert(pf->r_mask == 0xff0000);
               gl_type = GL_UNSIGNED_BYTE;
               break;
            case 2:
               switch(pf->r_mask){
               case 0xf00:
               case 0xff:   //!! bumbmap
               case 0x1f:   //!! bumbmap
                  gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
                  break;
               case 0x7c00:
                  gl_type = GL_UNSIGNED_SHORT_5_5_5_1;
                  break;
               case 0xf800:
                  gl_type = GL_UNSIGNED_SHORT_5_6_5;
                  gl_fmt = GL_RGB;
                  break;
               default:
                  assert(0);
               }
               break;
            default:
               assert(0);
            }
            glBindTexture(GL_TEXTURE_2D, txt_id); CHECK_GL_RESULT("glBindTexture");
            byte *tmp = NULL;
            switch(pf->bytes_per_pixel){
            case 4:
               tmp = new byte[size_x*size_y*4];
               memcpy(tmp, mem, size_x*size_y*4);
               {
                  struct S_rgb{
                     byte r, g, b, a;
                  };
                  S_rgb *rgb = (S_rgb*)tmp;
                  for(int i=size_x*size_y; i--; ){
                     Swap(rgb[i].r, rgb[i].b);
                  }
                  mem = tmp;
               }
               break;
            case 2:
               switch(pf->r_mask){
               case 0xf00:
                  tmp = new byte[size_x*size_y*2];
                  memcpy(tmp, mem, size_x*size_y*2);
                  {
                     word *rgb = (word*)tmp;
                     for(int i=size_x*size_y; i--; ){
                        word &p = rgb[i];
                        p = (p<<4) | (p>>12);
                     }
                     mem = tmp;
                  }
                  break;
               }
               break;
            }
            glTexImage2D(GL_TEXTURE_2D, 0, gl_fmt, size_x, size_y, 0, gl_fmt, gl_type, mem); CHECK_GL_RESULT("glTexImage2D");
            delete[] tmp;
            sysmem_image->Unlock();

            if(txt_flags&TXTF_MIPMAP)
               glGenerateMipmap(GL_TEXTURE_2D);
         }
#endif
      }
      break;
   }
}

//----------------------------
//----------------------------

I3D_animated_texture::I3D_animated_texture(PI3D_driver d1):
   ref(1),
   I3D_texture_base(d1),
   last_render_time(0),
   anim_delay(250),
   anim_count(0),
   curr_phase(0)
{}

//----------------------------

IDirect3DBaseTexture9 *I3D_animated_texture::GetD3DTexture() const{

   if(!texture_list.size())
      return NULL;
   dword curr_rt = drv->GetRenderTime();
   if(last_render_time!=curr_rt){
      int time_delta = curr_rt - last_render_time;
      anim_count += time_delta;
      while(anim_count >= anim_delay){
         anim_count -= anim_delay;
         ++curr_phase %= texture_list.size();
      }
      last_render_time = curr_rt;
   }
   return texture_list[curr_phase]->GetD3DTexture();
}

//----------------------------
//----------------------------

I3D_camera_texture::I3D_camera_texture(PI3D_driver d1):
   ref(1),
   I3D_texture_base(d1)
{
   S_pixelformat pf;
   if(I3D_SUCCESS(drv->FindTextureFormat(pf, FINDTF_RENDERTARGET))){
      PIImage img = drv->GetGraphInterface()->CreateImage();
      I3D_RESULT ir = img->Open(NULL, IMGOPEN_TEXTURE | IMGOPEN_VIDMEM | IMGOPEN_HINTDYNAMIC | IMGOPEN_RENDERTARGET |
         IMGOPEN_MIPMAP, 256, 256, 0);
      if(I3D_SUCCESS(ir)){
         vidmem_image = img;
      }
      img->Release();
   }
}

//----------------------------

IDirect3DBaseTexture9 *I3D_camera_texture::GetD3DTexture() const{

   if(!vidmem_image)
      return NULL;
   return vidmem_image->GetDirectXTexture();
}

//----------------------------
//----------------------------
