#include "all.h"
#include "igraph_i.h"
#include "loader.h"
#include "i3d\i3d_math.h"
#include <math.h>

//----------------------------

static C_str MakeCubicTextureName(const C_str &name, dword side){

   assert(side<6);
   C_str ret = name;
   const char *ext = NULL;
   for(dword i=ret.Size(); i--; ){
      if(ret[i]=='.'){
         ret[i] = 0;
         ext = &name[i];
         break;
      }
   }
   static const char side_char[] = {'r', 'l', 't', 'b', 'f', 'k'};
   ret = C_fstr("%s_%c", (const char*)ret, side_char[side]);
   if(ext)
      ret += ext;
   return ret;
}

//----------------------------
/*
                              //registered loaders
#define MAX_LOAD_PLUGINS 16
static PC_image_loader load_plugins[MAX_LOAD_PLUGINS];
static dword num_load_plugins = 0;

void RegisterImageLoader(PC_image_loader il){
   assert(num_load_plugins<MAX_LOAD_PLUGINS);
   load_plugins[num_load_plugins++] = il;
}
*/
C_image_loader *CreateLoaderBMP();
C_image_loader *CreateLoaderJPG();
C_image_loader *CreateLoaderPCX();
C_image_loader *CreateLoaderPNG();
C_image_loader *CreateLoaderTGA();

typedef C_image_loader *(*t_CreateLoader)();
t_CreateLoader loaders[] = {
   &CreateLoaderPNG,
   &CreateLoaderJPG,
   &CreateLoaderBMP,
   &CreateLoaderPCX,
   &CreateLoaderTGA,
   NULL
};

//----------------------------

void *C_image_loader::MemAlloc(dword size){
   void *mem = new byte[size];
   if(!mem)
      throw C_except_loader("failed to allocate memory");
   return mem;
}

//----------------------------

void C_image_loader::MemFree(void *mem){
   delete[] (byte*)mem;
}

//----------------------------
                              //image creation
PIImage IGraph::CreateImage() const{

   return new IImage(this);
}

//----------------------------

bool IGraph::GetImageInfo(const char *name, dword &sx, dword &sy, byte &bpp, dword &id) const{

   C_cache ck;
   if(!ck.open(name, CACHE_READ))
      return false;
   return GetImageInfo(ck, sx, sy, bpp, id);
}

//----------------------------

bool IGraph::GetImageInfo(C_cache &ck, dword &sx, dword &sy, byte &bpp, dword &id) const{

   size_t offs = ck.tellg();
   if(ck.read((char*)&id, sizeof(dword)) != sizeof(dword))
      return false;

   ck.seekg(offs);

   C_str err;
   E_LOADER_RESULT lr = IMGLOAD_BAD_FORMAT;
   for(int i=0; loaders[i]; i++){
      C_image_loader *ldr = (*loaders[i])();
   //for(int i=num_load_plugins; i--; ){
      S_pixelformat pf;
      lr = ldr->GetImageInfo(ck, &id, sizeof(id), &sx, &sy,  &pf, err);
      delete ldr;
      if(lr==IMGLOAD_OK){
         bpp = (byte)pf.bytes_per_pixel;
         break;
      }
      if(lr!=IMGLOAD_BAD_FORMAT)
         break;
   }
   ck.seekg(offs);
   return (lr==IMGLOAD_OK);
}

//----------------------------
//----------------------------
                              //image class
IImage::IImage(CPIGraph ig):
   igraph(ig),
   pal_argb(NULL),
   size_x(0),
   size_y(0)
{
}

//----------------------------

IImage::~IImage(){

   Close();
}

//----------------------------

D3DFORMAT ConvertFormat(const S_pixelformat *pf){

   if(pf->flags&PIXELFORMAT_COMPRESS){
      switch(pf->four_cc){
      case MAKEFOURCC('D', 'X', 'T', '1'):
         return D3DFMT_DXT1;
      case MAKEFOURCC('D', 'X', 'T', '2'):
         return D3DFMT_DXT2;
      case MAKEFOURCC('D', 'X', 'T', '3'):
         return D3DFMT_DXT3;
      case MAKEFOURCC('D', 'X', 'T', '4'):
         return D3DFMT_DXT4;
      case MAKEFOURCC('D', 'X', 'T', '5'):
         return D3DFMT_DXT5;
      }
   }else
   if(pf->flags&PIXELFORMAT_BUMPMAP){
      switch(pf->bytes_per_pixel){
      case 2:
         switch(pf->u_mask){
         case 0x00ff:
            assert(pf->v_mask==0xff00);
            assert(pf->l_mask==0x0000);
            return D3DFMT_V8U8;
         case 0x001f:
            assert(pf->v_mask==0x03e0);
            assert(pf->l_mask==0xfc00);
            return D3DFMT_L6V5U5;
         default:
            assert(0);
         }
         break;
      case 4:
         switch(pf->u_mask){
         case 0x000000ff:
            assert(pf->v_mask==0x0000ff00);
            switch(pf->l_mask){
            case 0x00ff0000:
               return D3DFMT_X8L8V8U8;
            case 0x00000000:
               return D3DFMT_Q8W8V8U8;
            default:
               assert(0);
            }
            break;
         case 0x0000ffff:
            assert(pf->v_mask==0xffff0000);
            assert(pf->l_mask==0x00000000);
            return D3DFMT_V16U16;
            /*
         case 0x000003ff:
            assert(pf->v_mask==0x001ffc00);
            assert(pf->l_mask==0xffe00000);
            return D3DFMT_W11V11U10;
            */
         default:
            assert(0);
         }
         break;
      default:
         assert(0);
      }
   }else
   switch(pf->bytes_per_pixel){

   case 1:
#ifndef GL
      if(pf->flags&PIXELFORMAT_PALETTE)
         return D3DFMT_P8;
      else
#endif
         return D3DFMT_R3G3B2;

   case 2:

      switch(pf->r_mask){
      case 0xf800:
         assert(pf->g_mask==0x07e0);
         assert(pf->b_mask==0x001f);
         return D3DFMT_R5G6B5;

      case 0x7c00:
         switch(pf->a_mask){
         case 0x0000:
            return D3DFMT_X1R5G5B5;
         case 0x8000:
            return D3DFMT_A1R5G5B5;
         }
         break;

      case 0x0f00:
         switch(pf->a_mask){
         case 0x0000:
            return D3DFMT_X4R4G4B4;
         case 0xf000:
            assert(pf->r_mask==0x0f00);
            assert(pf->g_mask==0x00f0);
            assert(pf->b_mask==0x000f);
            return D3DFMT_A4R4G4B4;
         }
         break;

      case 0x00e0:
         return D3DFMT_A8R3G3B2;

      default: assert(0);
      }
      break;

   case 3:
      assert(pf->r_mask == 0x00ff0000);
      assert(pf->g_mask == 0x0000ff00);
      assert(pf->b_mask == 0x000000ff);
      assert(pf->a_mask == 0x00000000);
      return D3DFMT_R8G8B8;

   case 4:
      assert(pf->r_mask == 0x00ff0000);
      assert(pf->g_mask == 0x0000ff00);
      assert(pf->b_mask == 0x000000ff);
      switch(pf->a_mask){
      case 0:
         return D3DFMT_X8R8G8B8;
      case 0xff000000:
         return D3DFMT_A8R8G8B8;
      }
      break;
   default: assert(0);
   }
   return D3DFMT_UNKNOWN;
}

//----------------------------

static void InitColorkeyInfo(const void *src, void *dst, int size_x, int size_y, int src_pitch, int dst_pitch,
   const S_pixelformat &pf_src, const S_pixelformat &pf_dst, C_vector<byte> &rle_transp, bool store_alpha){

   dword *addr = new dword[size_y];
   byte *rle_data = new byte[size_y*(size_x*2+1)], *rle_data1 = rle_data;
   union{
      byte *bp;
      word *wp;
      dword *dwp;
      void *vp;
   };
   dword not_alpha = ~pf_dst.a_mask;

   for(int y=0; y<size_y; y++){
      addr[y] = (rle_data1 - rle_data) + size_y * 4; //save address
      vp = dst;

      bool last_opaque = false;
      int count = 0;
      static const byte segment_mask[] = {0, 0x80};
      for(int x=0; x<size_x; x++){
         bool opaque = true;;

         switch(pf_src.bytes_per_pixel){
         case 1: opaque = (((byte*)src)[x] != 0); break;
         case 2: opaque = (((word*)src)[x] != 0); break;
         case 3:
            {
               dword pixel = *(word*)(((byte*)src)+x*3);
               pixel |= ((byte*)src)[x*3+2];
               opaque = (pixel != 0);
            }
            break;
         case 4:
            {
               dword pixel = ((dword*)src)[x];
               opaque = (pixel >= 0x80000000);
            }
            break;
         }

         if(store_alpha){
            switch(pf_dst.bytes_per_pixel){
            case 1:
               *bp &= not_alpha;
               if(opaque) *bp |= pf_dst.a_mask;
               break;
            case 2:
               *wp &= not_alpha;
               if(opaque) *wp |= pf_dst.a_mask;
               break;
            case 3:
               {
                  *wp &= not_alpha;
                  bp[2] &= not_alpha>>16;
                  if(opaque){
                     *wp |= pf_dst.a_mask;
                     bp[2] |= pf_dst.a_mask>>16;
                  }
               }
               break;
            case 4:
               *dwp &= not_alpha;
               if(opaque) *dwp |= pf_dst.a_mask;
               break;
            }
         }
         if(!x){
            last_opaque = opaque;
            count = 1;
         }else
         if(last_opaque != opaque){
            while(count>=127){
               *rle_data1++ = byte(segment_mask[last_opaque] | 0x7f);
               count -= 127;
            }
            if(count)
               *rle_data1++ = byte(segment_mask[last_opaque] | count);
            last_opaque = opaque;
            count = 1;
         }else
            ++count;
         bp += pf_dst.bytes_per_pixel;
      }
      if(last_opaque){
         while(count>=127) *rle_data1++ = (0x80 | 0x7f), count -= 127;
         if(count)
            *rle_data1++ = byte(0x80 | count);
      }
                              //end line by zero byte
      *rle_data1++ = 0;
      src = ((byte*)src) + src_pitch;
      dst = ((byte*)dst) + dst_pitch;
   }
   rle_transp.reserve((rle_data1 - rle_data)+size_y*4);
                              //store addresses
   for(int i=0; i<size_y; i++){
      for(dword j=0, dw=addr[i]; j<4; j++, dw >>= 8)
         rle_transp.push_back((byte)dw);
   }
                              //store rle masks
   for(byte *bp2 = rle_data; bp2<rle_data1; )
      rle_transp.push_back(*bp2++);

   delete []addr;
   delete []rle_data;
}

//----------------------------

bool IImage::Open(const char *name1, dword flags, dword sx1, dword sy1, const S_pixelformat *pf, dword num_mip_levels, C_str *err_msg){

   const char *name = name1;
   C_cache ck;
   if(!(flags&IMGOPEN_EMPTY)){
      C_str tmp;
      if(flags&IMGOPEN_CUBE_TEXTURE){
         tmp = MakeCubicTextureName(name, 0);
         name = tmp;
      }
      if(!ck.open(name, CACHE_READ))
         return false;
   }
   return OpenInternal(ck, name1, flags, sx1, sy1, pf, num_mip_levels, err_msg);
}

//----------------------------
                              //open image
bool IImage::Open(C_cache &ck, dword flags, dword sx1, dword sy1, const S_pixelformat *pf, dword nl, C_str *err_msg){

   if(flags&IMGOPEN_CUBE_TEXTURE)
      return false;
   return OpenInternal(ck, NULL, flags, sx1, sy1, pf, nl, err_msg);
}

//----------------------------

bool IImage::OpenInternal(C_cache &ck, const char *name, dword flags, dword sx1, dword sy1, const S_pixelformat *pf, dword num_mip_levels, C_str *err_msg){

   void *area = NULL;
   LPC_rgb_conversion rgb_conv = NULL;

   try{
      Close();

      int i;
                                 //textures needs specified pixel format
      if((flags&IMGOPEN_TEXTURE) && !pf)
         throw C_except("textures need specified pixel format");

      bool texture_convert = false;
      bool create_palette = false;

      S_pixelformat source_pf;
      memset(&source_pf, 0, sizeof(source_pf));
      dword conv_flags = 0;

      dword ckey0 = 0;

      if(!(flags&IMGOPEN_EMPTY)){
                                 //read part of header
         dword header;
         ck.read((char*)&header, sizeof(header));
         ck.seekg(ck.tellg()-sizeof(header));

         C_str err;
         E_LOADER_RESULT lr = IMGLOAD_BAD_FORMAT;
                                 //scan through registered image loader
         for(int i=0; loaders[i]; i++){
            C_image_loader *ldr = (*loaders[i])();
         //for(i=num_load_plugins; i--; ){
            lr = ldr->Load(ck, &header, sizeof(header),
               &area, (void**)&pal_argb, &size_x, &size_y, &source_pf, err);
            delete ldr;
            if(lr != IMGLOAD_BAD_FORMAT)
               break;
         }
         if(lr != IMGLOAD_OK)
            throw C_except("unknown format");
      }else{
         if(!sx1 || !sy1 || !pf)
            throw C_except("cannot create empty surface (no size or pixelformat specified)");
         size_x = sx1, size_y = sy1;
         pal_argb = new dword[256];
      }
      if(flags&IMGOPEN_NOREMAP){
         if(flags&IMGOPEN_EMPTY){
            if(!pf)
               throw C_except("cannot create empty surface (no pixelformat specified)");
            pixel_format = *pf;
         }else
            pixel_format = source_pf;
      }else{
         if(pf)
            pixel_format = *pf;
      }

                              //create surface
      HRESULT hr;

      if(flags&IMGOPEN_TEXTURE){
                              //vidmem texture surface
         if(!(flags&IMGOPEN_EMPTY) && !(flags&IMGOPEN_NOREMAP)){
                              //convert to specified pixel format
                              // or let copy
            rgb_conv = igraph->GetRGBConv1(*pf);
            texture_convert = true;
                              //get color key
            if(source_pf.bytes_per_pixel==1){
               if(pf->bytes_per_pixel==1 && (pf->flags&PIXELFORMAT_PALETTE)){
                              //dest is 8-bit pal
                  ckey0 = 0;
               }else{
                              //map palette color 0 to RGB
                  if(pal_argb){
                     byte *b_pal = (byte*)pal_argb;
                     ckey0 = rgb_conv->GetPixel(b_pal[0], b_pal[1], b_pal[2], 0);
                  }else
                     ckey0 = 0;
               }
            }else{            //source is RGB, use superblack
               ckey0 = 0;
            }
         }

         dword num_levels = 1;
         if(flags&IMGOPEN_MIPMAP){
            num_levels = num_mip_levels + 1;
         }

         D3DFORMAT d3d_fmt;

         if(flags&IMGOPEN_NOREMAP){
            d3d_fmt = ConvertFormat(&pixel_format);
         }else
         if(!(pf->flags&PIXELFORMAT_COMPRESS)){
            d3d_fmt = ConvertFormat(pf);
         }else{
            d3d_fmt = ConvertFormat(pf);
         }
         if(d3d_fmt==D3DFMT_UNKNOWN)
            throw C_except("unknown pixel format");
#ifndef GL
         if(d3d_fmt==D3DFMT_P8)
            create_palette = true;
#endif
         D3DPOOL pool_type = D3DPOOL_DEFAULT;
         if(flags&IMGOPEN_SYSMEM)
            pool_type = D3DPOOL_SYSTEMMEM;

         dword usage_flags = 0;

         if(flags&IMGOPEN_RENDERTARGET)
            usage_flags |= D3DUSAGE_RENDERTARGET;
         if(flags&IMGOPEN_HINTDYNAMIC)
            usage_flags |= D3DUSAGE_DYNAMIC;

         if(!(flags&IMGOPEN_CUBE_TEXTURE)){
                              //classic texture
            hr = igraph->GetD3DDevice()->CreateTexture(size_x, size_y, num_levels,
               usage_flags,
               d3d_fmt,
               pool_type,
               (IDirect3DTexture9**)&lpTxt9,
               NULL);
         }else{
                              //cube texture
            hr = igraph->GetD3DDevice()->CreateCubeTexture(size_x, num_levels,
               usage_flags,
               d3d_fmt,
               pool_type,
               (IDirect3DCubeTexture9**)&lpCTxt9,
               NULL);
         }
      }else{
         D3DFORMAT d3d_fmt;
         if((flags&IMGOPEN_NOREMAP) || pf){
            if(!pf){
               d3d_fmt = ConvertFormat(&pixel_format);
               create_palette = (pixel_format.bytes_per_pixel == 1);
            }else{
               d3d_fmt = ConvertFormat(pf);
               create_palette = (pf->bytes_per_pixel == 1);
            }
         }else{
            d3d_fmt = ConvertFormat(igraph->GetPixelFormat());
         
            create_palette = (igraph->GetPixelFormat()->bytes_per_pixel == 1);
         }
         if(d3d_fmt==D3DFMT_UNKNOWN)
            throw C_except("unknown pixel format");

         D3DPOOL pool_type = D3DPOOL_DEFAULT;
         if(flags&(IMGOPEN_NOREMAP|IMGOPEN_SYSMEM))
            pool_type = D3DPOOL_SCRATCH;

         hr = igraph->GetD3DDevice()->CreateOffscreenPlainSurface(size_x, size_y, d3d_fmt,
            pool_type,
            (IDirect3DSurface9**)&lpImg9, NULL);
         //CHECK_D3D_RESULT("CreateImageSurface", hr);   //don't check result, may happen on low memory
      }
      if(FAILED(hr))
         throw C_except("failed to allocate DirectX surface");

      if(!(flags&IMGOPEN_EMPTY)){
                              //fill-in surface contents
         if(!(flags&IMGOPEN_CUBE_TEXTURE)){
            void *mem_ptr;
            dword pitch;
            if(!Lock(&mem_ptr, &pitch, false))
               throw C_except("failed to lock surface");
                              //copy bytes into surface
            if(texture_convert){
               rgb_conv->Convert(area, mem_ptr, size_x, size_y,
                  size_x*source_pf.bytes_per_pixel, pitch, (byte)source_pf.bytes_per_pixel, &pal_argb,
                  conv_flags |
                  ((flags&IMGOPEN_DITHER) ? MCONV_DITHER : 0) |
                  ((flags&IMGOPEN_COLORKEY) ? MCONV_COLORKEY : 0),
                  ckey0);
            }else
            if(!(flags&(IMGOPEN_NOREMAP | IMGOPEN_EMPTY))){
               const C_rgb_conversion *rc;
               if(pf){
                  assert(!rgb_conv);
                  rgb_conv = igraph->GetRGBConv1(pixel_format);
                  rc = rgb_conv;
               }else{
                  rc = igraph->GetRGBConv();
               }
               rc->Convert(area, mem_ptr, size_x, size_y,
                  size_x*source_pf.bytes_per_pixel, pitch, (byte)source_pf.bytes_per_pixel, &pal_argb,
                  conv_flags |
                  ((flags&IMGOPEN_DITHER) ? MCONV_DITHER : 0));
               pixel_format = *rc->GetPixelFormat();
            }else{
               for(dword sy=0; sy<size_y; sy++){
                  memcpy(((byte*)mem_ptr) + sy*pitch,
                     ((byte*)area)+sy*size_x*pixel_format.bytes_per_pixel, size_x*pixel_format.bytes_per_pixel);
               }
            }
                              //initialize colorkey info
            if(flags&IMGOPEN_INIT_CKEY){
               const void *src = area;
               int src_pitch = size_x*source_pf.bytes_per_pixel;
               if(conv_flags&MCONV_FLIP_Y){
                  (byte*&)src += (size_y-1)*src_pitch;
                  src_pitch = -src_pitch;
               }
               bool store_alpha =  true;
               if(pixel_format.flags&PIXELFORMAT_COMPRESS)
                  store_alpha = false;
               if(source_pf.bytes_per_pixel==4)
                  store_alpha = false;
               InitColorkeyInfo(src, mem_ptr, size_x, size_y, src_pitch, pitch,
                  source_pf, pixel_format, ckey_rle_info, store_alpha);
            }
            Unlock();
         }else{
            D3DLOCKED_RECT lrc;

            for(int i=0; i<6; i++){
               void *tmp_area;
               dword *tmp_pal;
               if(!i){
                              //level zero already opened, use values directly
                  tmp_area = area;
                  tmp_pal = pal_argb;
               }else{
                  tmp_area = NULL;
                  tmp_pal = NULL;
                              //open image
                  C_str n = MakeCubicTextureName(name, i);
                  C_cache ck;
                  if(!ck.open(n, CACHE_READ))
                     throw C_except(C_fstr("failed to open cube surface '%s'", (const char*)n));

                  dword header;
                  ck.read((char*)&header, sizeof(header));
                  ck.seekg(ck.tellg()-sizeof(header));

                  C_str err;
                  E_LOADER_RESULT lr = IMGLOAD_BAD_FORMAT;
                  dword sx = 0, sy = 0;
                              //scan through registered image loader
                  for(int i=0; loaders[i]; i++){
                     C_image_loader *ldr = (*loaders[i])();
                  //for(int i=num_load_plugins; i--; ){
                     lr = ldr->Load(ck, &header, sizeof(header),
                        &tmp_area, (void**)&tmp_pal, &sx, &sy, &source_pf, err);
                     if(lr != IMGLOAD_BAD_FORMAT)
                        break;
                  }
                  if(lr != IMGLOAD_OK)
                     throw C_except("unknown format");
                  if(sx!=size_x || sy!=size_y){
                     delete[] tmp_area;
                     delete[] tmp_pal;
                     throw C_except("cubic surface size mismatch");
                  }
               }
               static const D3DCUBEMAP_FACES side_map[] = {
                  D3DCUBEMAP_FACE_POSITIVE_X, D3DCUBEMAP_FACE_NEGATIVE_X,
                  D3DCUBEMAP_FACE_POSITIVE_Y, D3DCUBEMAP_FACE_NEGATIVE_Y,
                  D3DCUBEMAP_FACE_POSITIVE_Z, D3DCUBEMAP_FACE_NEGATIVE_Z,
               };
               hr = lpCTxt9->LockRect(side_map[i], 0, &lrc, NULL, 0);
               CHECK_D3D_RESULT("LockRect", hr);
               if(FAILED(hr))
                  throw C_except("failed to lock surface");

               rgb_conv->Convert(tmp_area, lrc.pBits, size_x, size_y,
                  size_x*source_pf.bytes_per_pixel, lrc.Pitch, (byte)source_pf.bytes_per_pixel, &tmp_pal,
                  conv_flags |
                  ((flags&IMGOPEN_DITHER) ? MCONV_DITHER : 0) |
                  ((flags&IMGOPEN_COLORKEY) ? MCONV_COLORKEY : 0),
                  ckey0);
               if(i){
                  delete[] tmp_area;
                  delete[] tmp_pal;
               }
               hr = lpCTxt9->UnlockRect(side_map[i], 0);
               CHECK_D3D_RESULT("UnlockRect", hr);
            }
         }
                              //free temp memory
         delete[] area;
      }

      if(create_palette){
         if(pal_argb && !(flags&IMGOPEN_EMPTY)){
         }else{
            if(!pal_argb)
               pal_argb = new dword[256];
                              //setup uniform palette
            for(i=0; i<256; i++){
               ((byte*)pal_argb)[i*4 + 0] = byte((i) & 0xe0);
               ((byte*)pal_argb)[i*4 + 1] = byte((i>>3) & 0x1c);
               ((byte*)pal_argb)[i*4 + 2] = byte((i>>6) & 0x3);
               ((byte*)pal_argb)[i*4 + 3] = 0xff;
            }
         }
      }else{
         delete[] pal_argb;
         pal_argb = NULL;
      }

      if(rgb_conv)
         rgb_conv->Release();

      if(!(flags&IMGOPEN_TEXTURE))
         open_flags = flags;
   }catch(const C_except &exc){
      delete[] pal_argb;
      delete[] area;
      lpTxt9 = NULL;
      lpCTxt9 = NULL;
      lpImg9 = NULL;
      if(rgb_conv)
         rgb_conv->Release();
      if(err_msg)
         *err_msg = exc.what();
      return false;
   }
   return true;
}

//----------------------------
                              //close image
void IImage::Close(){

   if(IsInit()){
      lpTxt9 = NULL;
      lpCTxt9 = NULL;
      lpImg9 = NULL;

      delete[] pal_argb;
      pal_argb = NULL;
   }
}

//----------------------------

bool IImage::IsInit() const{
   
   return (lpTxt9!=NULL || lpImg9!=NULL || lpCTxt9!=NULL);
}

//----------------------------

const S_pixelformat *IImage::GetPixelFormat() const{
   
   return IsInit() ?
      &pixel_format :
      NULL;
}

//----------------------------

bool IImage::SetPal(dword *pal1){

   if(pal_argb && pal1){
      memcpy(pal_argb, pal1, sizeof(dword)*256);
      return true;
   }
   return false;
}

//----------------------------
                              //draw image to backbuffer
bool IImage::Draw(int dst_x, int dst_y, int dsx, int dsy){

   if(!IsInit() || !lpImg9){
      return false;
   }

                              //clip
   dword src_x = 0, src_y = 0;
   /*
   if(dst_x<igraph->vport[0])
      src_x = igraph->vport[0]-dst_x, dst_x = igraph->vport[0];
   if(dst_y<igraph->vport[1])
      src_y = igraph->vport[1]-dst_y, dst_y = igraph->vport[1];
   int sx = dst_x + size_x - src_x, sy = dst_y + size_y - src_y;
   if(sx>igraph->vport[2]) sx = igraph->vport[2];
   if(sy>igraph->vport[3]) sy = igraph->vport[3];
   if((sx -= dst_x)<=0 || (sy -= dst_y)<=0)
      return true;
   */
   int sx = size_x, sy = size_y;

   RECT rc;
   rc.left = src_x;
   rc.top = src_y;
   rc.right = src_x+sx;
   rc.bottom = src_y+sy;

   if(!dsx)
      dsx = igraph->scrn_sx - dst_x;
   if(!dsy)
      dsy = igraph->scrn_sy - dst_y;

   HRESULT hr;
   IDirect3DDevice9 *lpDev9 = igraph->GetD3DDevice();

   IDirect3DSurface9 *lpBack;
   hr = lpDev9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &lpBack);
   CHECK_D3D_RESULT("GetBackBuffer", hr);
   if(SUCCEEDED(hr)){
      //RECT dst_rc = {dst_x, dst_y, sx, sy};
      RECT dst_rc = {dst_x, dst_y, dst_x + dsx, dst_y + dsy};
      //hr = lpDev9->CopyRects(lpImg9, &rc, 1, lpBack, &pt);
      //hr = lpDev9->UpdateSurface(lpImg9, &rc, lpBack, &pt);
      hr = lpDev9->StretchRect(lpImg9, &rc, lpBack, &dst_rc, D3DTEXF_NONE);
      lpBack->Release();
      //CHECK_D3D_RESULT("CopyRects", hr);
      return SUCCEEDED(hr);
   }
   return false;
}

//----------------------------

bool IImage::Lock(void **area1, dword *pitch, bool read_only){

   if(!IsInit())
      return false;

   D3DLOCKED_RECT lrc;
   HRESULT hr;
   if(lpTxt9){
      hr = lpTxt9->LockRect(0, &lrc, NULL, (read_only ? (D3DLOCK_READONLY | D3DLOCK_NO_DIRTY_UPDATE) : 0));
   }else
   if(lpImg9){
      hr = lpImg9->LockRect(&lrc, NULL, (read_only ? D3DLOCK_READONLY : 0)); //29.01.2003: D3DLOCK_NO_DIRTY_UPDATE commneted out, bacause new drivers for ATI 8500 failed to lock backbuffer with this flag added.(it was not important anyway).
   }else{
      assert(0);
      return false;
   }
   CHECK_D3D_RESULT("LockRect", hr); 
   if(FAILED(hr))
      return false;
   if(area1) *area1 = lrc.pBits;
   if(pitch){
      //if(!(pixel_format.flags&PIXELFORMAT_COMPRESS))
         *pitch = lrc.Pitch;
         /*
      else{
         *pitch = size_x * size_y / 2;
         if(pixel_format.four_cc != MAKEFOURCC('D', 'X', 'T', '1'))
            *pitch *= 2;
      }
      */
   }
   return true;
}

//----------------------------

bool IImage::Unlock(){

   if(!IsInit())
      return false;

   HRESULT hr;
   if(lpTxt9)
      hr = lpTxt9->UnlockRect(0);
   else
   if(lpImg9)
      hr = lpImg9->UnlockRect();
   else{
      assert(0);
      return false;
   }
   CHECK_D3D_RESULT("UnlockRect", hr);
   return (SUCCEEDED(hr));
}

//----------------------------

bool IImage::AlphaBlend(CPIImage src_img, dword flags){

   if(!IsInit() || !src_img || !src_img->IsInit()){
      assert(0);
      return false;
   }
   if(src_img->SizeX()!=SizeX() || src_img->SizeY()!=SizeY()){
      assert(0);
      return false;
   }
                              //check if alpha-bits present
                              // (may be either a_mask or l_mask, but both occupy the same location in S_pixel_format)
   //if(!(pixel_format.flags&PIXELFORMAT_ALPHA))
   if(!(pixel_format.a_mask))
      return false;

   bool b = false;

   void *dst_a;
   dword dst_pitch;
   b = Lock(&dst_a, &dst_pitch, false);
   if(!b)
      return false;

   void *src_a;
   dword src_pitch;
   byte src_bpp = (byte)src_img->GetPixelFormat()->bytes_per_pixel;
   b = const_cast<PIImage>(src_img)->Lock(&src_a, &src_pitch, true);
   if(!b){
      Unlock();
      return false;
   }
   LPC_rgb_conversion rgb_conv = igraph->GetRGBConv1(pixel_format);

                              //blend
   b = rgb_conv->AlphaBlend(dst_a, src_a, src_img->GetPal(),
      SizeX(), SizeY(), src_pitch, dst_pitch, src_bpp,
      (flags&IMGBLEND_DITHER ? MCONV_DITHER : 0) |
      (flags&IMGBLEND_INVALPHA ? MCONV_INVALPHA : 0));

   rgb_conv->Release();
   const_cast<PIImage>(src_img)->Unlock();
   Unlock();

   return b;
}

//----------------------------

struct S_argb{
   byte b, g, r, a;
};

struct S_argb_signed{
   //signed char b, g, r, a;
   byte b;
   signed char g, r;
   byte a;
};

//----------------------------
// Shrink surface to 1/4 of original size, filtering out four pixels to one.
// Parameters:
//    area - surface area
//    sx1, sy - new resolution of surface (original surface is 2 times greater on both axes)
static void ShrinkMipmap(S_argb *area, dword sx1, dword sy){

   S_argb *src = area, *dst = area, *src1 = area + sx1 * 2;
   for(; sy--; src += sx1*2, src1 += sx1*2){
      for(dword sx=0; sx<sx1; sx++, ++dst, src += 2, src1 += 2){
         dst->r = byte((src[0].r + src[1].r + src1[0].r + src1[1].r + 2) / 4);
         dst->g = byte((src[0].g + src[1].g + src1[0].g + src1[1].g + 2) / 4);
         dst->b = byte((src[0].b + src[1].b + src1[0].b + src1[1].b + 2) / 4);
         //dst->a = byte((src[0].a + src[1].a + src1[0].a + src1[1].a + 2) / 4);
      }
   }
}

#pragma warning(disable:4244) //conversion from 'int' to 'byte'
//----------------------------
// Shrink mipmap with alpha channel (special processing is made for fully translucent pixels).
static void ShrinkMipmapWithAlpha(S_argb *area, dword sx1, dword sy){

   S_argb *src = area, *dst = area, *src1 = area + sx1 * 2;
   for(; sy--; src += sx1*2, src1 += sx1*2){
      for(dword sx=0; sx<sx1; sx++, ++dst, src += 2, src1 += 2){
         const S_argb *neighbours[4] = {
            src, src+1, src1, src1+1
         };
         dword num_nt = 0;
         dword r = 0, g = 0, b = 0, a = 0;
         for(dword i=4; i--; ){
            const S_argb &n = *neighbours[i];
            if(n.a){
               r += n.r;
               g += n.g;
               b += n.b;
               ++num_nt;
               a += n.a;
            }
         }
         switch(num_nt){
         case 0: dst->r = 0;         dst->g = 0;         dst->b = 0;         break;
         case 1: dst->r =  r;        dst->g =  g;        dst->b =  b;        break;
         case 2: dst->r = (r+1) / 2; dst->g = (g+1) / 2; dst->b = (b+1) / 2; break;
         case 3: dst->r = (r+1) / 3; dst->g = (g+1) / 3; dst->b = (b+1) / 3; break;
         case 4: dst->r = (r+2) / 4; dst->g = (g+2) / 4; dst->b = (b+2) / 4; break;
         }
                              //alpha is always getting blended by all 4 neighbours
         dst->a = (a+2) / 4;
      }
   }
}

//----------------------------
// Shrink mipmap - signed values (DuDv format for bumpmaps).
static void ShrinkMipmap(S_argb_signed *area, dword sx1, dword sy){

   S_argb_signed *src = area, *dst = area, *src1 = area + sx1 * 2;
   for(; sy--; src += sx1*2, src1 += sx1*2){
      for(dword sx=0; sx<sx1; sx++, ++dst, src += 2, src1 += 2){
         int i;
         i = (src[0].r + src[1].r + src1[0].r + src1[1].r);
         i += (i>=0) ? 2 : -2;
         dst->r = byte(i / 4);

         i = (src[0].g + src[1].g + src1[0].g + src1[1].g);
         i += (i>=0) ? 2 : -2;
         dst->g = byte(i / 4);

         i = (src[0].b + src[1].b + src1[0].b + src1[1].b);
         i += (i>=0) ? 2 : -2;
         dst->b = byte(i / 4);

         i = (src[0].a + src[1].a + src1[0].a + src1[1].a);
         i += (i>=0) ? 2 : -2;
         dst->a = byte(i / 4);
      }
   }
}

//----------------------------

inline int __cdecl FindLastBit(dword val){
   dword rtn;
   __asm{
      mov eax, val
      bsr eax, eax
      jnz ok
      mov eax, -1
ok:
      mov rtn, eax
   }
   return rtn;
}

inline dword __cdecl Rol(dword dw, byte b){
   __asm{
      push ecx
      mov cl, b
      rol dw, cl
      pop ecx
   }
   return dw;
}
inline dword __cdecl Ror(dword dw, byte b){
   __asm{
      push ecx
      mov cl, b
      ror dw, cl
      pop ecx
   }
   return dw;
}

//----------------------------

static void ConvertToARGB(S_argb *dst, const void *src, dword pitch, const S_pixelformat *pf,
   const dword *pal_argb, dword size_x, dword size_y){

   S_argb *tp = dst;
   union{
      const byte *src_bp;
      const word *src_wp;
      const void *src_vp;
   };
   src_vp = src;

   dword xx, yy;
   switch(pf->bytes_per_pixel){
   case 1:
      if(pal_argb){
         for(yy=0; yy<size_y; yy++, src_bp+=pitch)
         for(xx=0; xx<size_x; xx++, ++tp){
            byte *palp = (byte*)&pal_argb[src_bp[xx]];
            tp->r = palp[0];
            tp->g = palp[1];
            tp->b = palp[2];
         }
      }else{
         byte r_pos = byte((FindLastBit(pf->r_mask)-7) & 31);
         byte g_pos = byte((FindLastBit(pf->g_mask)-7) & 31);
         byte b_pos = byte((FindLastBit(pf->b_mask)-7) & 31);
         byte a_pos = byte((FindLastBit(pf->a_mask)-7) & 31);
         for(yy=0; yy<size_y; yy++, src_bp+=pitch)
         for(xx=0; xx<size_x; xx++, ++tp){
            byte pixel = src_bp[xx];
            tp->r = (byte)Ror(pixel&pf->r_mask, r_pos);
            tp->g = (byte)Ror(pixel&pf->g_mask, g_pos);
            tp->b = (byte)Ror(pixel&pf->b_mask, b_pos);
            tp->a = (byte)Ror(pixel&pf->a_mask, a_pos);
         }
      }
      break;
   case 2:
      {
         byte r_pos = byte((FindLastBit(pf->r_mask)-7) & 31);
         byte g_pos = byte((FindLastBit(pf->g_mask)-7) & 31);
         byte b_pos = byte((FindLastBit(pf->b_mask)-7) & 31);
         byte a_pos = byte((FindLastBit(pf->a_mask)-7) & 31);
#if defined _MSC_VER && 1
         if(!(size_x&1)){
            dword r_mask = pf->r_mask;
            dword g_mask = pf->g_mask;
            dword b_mask = pf->b_mask;
            dword a_mask = pf->a_mask;
            const byte *src_bp1 = src_bp;
            dword sx = size_x, sy = size_y;
            //dword pitch = lrc->Pitch;
            __asm{
               push ecx
                              //eax = free
                              //ebx = free
                              //ecx = free
                              //edx = src_bp
                              //esi = src_bp_1
                              //edi = tp
                              //esp[0] = counter x
                              //esp[4] = counter y
               push sy
               mov edx, src_bp1
               mov edi, tp

            loop_y:
               push sx
               mov esi, edx
               shr dword ptr[esp], 1   //process 2 pixels at a time

            loop_x:
               mov eax, [esi]

                              //RED
               mov cl, r_pos
               mov ebx, eax
               add esi, 4
               and ebx, r_mask
               ror ebx, cl
               mov [edi+2], bl

                              //GREEN
               mov cl, g_pos
               mov ebx, eax
               and ebx, g_mask
               ror ebx, cl
               mov [edi+1], bl

                              //BLUE
               mov cl, b_pos
               mov ebx, eax
               and ebx, b_mask
               ror ebx, cl
               mov [edi+0], bl

                              //ALPHA
               mov cl, a_pos
               mov ebx, eax
               and ebx, a_mask
               ror ebx, cl
               shr eax, 16    //next vertex
               mov [edi+3], bl

                              //RED
               mov cl, r_pos
               mov ebx, eax
               and ebx, r_mask
               ror ebx, cl
               mov [edi+6], bl

                              //GREEN
               mov cl, g_pos
               mov ebx, eax
               and ebx, g_mask
               ror ebx, cl
               mov [edi+5], bl

                              //BLUE
               mov cl, b_pos
               mov ebx, eax
               and ebx, b_mask
               ror ebx, cl

                              //ALPHA
               and eax, a_mask
               mov cl, a_pos
               mov [edi+4], bl
               ror eax, cl
               mov [edi+7], al


               add edi, 8
               dec dword ptr[esp]
               jnz loop_x
               pop eax
 
               add edx, pitch
               dec dword ptr[esp]
               jnz loop_y
               pop eax

               pop ecx
            }
         }else
#endif
         {
            for(yy=0; yy<size_y; yy++, src_bp+=pitch)
            for(xx=0; xx<size_x; xx++, ++tp){
               word pixel = src_wp[xx];
               tp->r = (byte)Ror(pixel&pf->r_mask, r_pos);
               tp->g = (byte)Ror(pixel&pf->g_mask, g_pos);
               tp->b = (byte)Ror(pixel&pf->b_mask, b_pos);
               tp->a = (byte)Ror(pixel&pf->a_mask, a_pos);
            }
         }
      }
      break;
   case 3:
      for(yy=0; yy<size_y; yy++, src_bp+=pitch){
         const byte *bp1 = src_bp;
         for(xx=0; xx<size_x; xx++, ++tp, bp1 += 3){
            tp->r = bp1[0];
            tp->g = bp1[1];
            tp->b = bp1[2];
         }
      }
      break;
   case 4:
      {
         for(yy=0; yy<size_y; yy++, tp+=size_x, src_bp+=pitch)
            memcpy(tp, src_bp, size_x * 4);
      }
      break;
   }
}

//----------------------------

bool IImage::MixMipmapOnSurface(const D3DLOCKED_RECT *lrc, int num_levels) const{

                              //convert source into rgba surface
   S_argb *area = new S_argb[size_x * size_y];
   if(!area)
      return false;

   ConvertToARGB(area, lrc->pBits, lrc->Pitch, &pixel_format, pal_argb, size_x, size_y);

   LPC_rgb_conversion conv = igraph->GetRGBConv1(pixel_format, pal_argb);
   bool has_alpha = (pixel_format.flags&PIXELFORMAT_ALPHA);

   dword sx = size_x, sy = size_y;
   for(int i=1; i<num_levels; i++){
      sx /= 2, sy /= 2;
                              //shring mipmap (use signed version for bump-maps, as they're in signed format)
      if(pixel_format.flags&PIXELFORMAT_BUMPMAP){
         ShrinkMipmap((S_argb_signed*)area, sx, sy);
      }else{
         if(!has_alpha)
            ShrinkMipmap(area, sx, sy);
         else{
            ShrinkMipmapWithAlpha(area, sx, sy);
            FixBordersOfAlphaPixels((dword*)area, sx, sy);
         }
      }
                              //convert back into surface
      conv->Convert(area, lrc[i].pBits, sx, sy, sx*4, lrc[i].Pitch, 4, (dword**)&pal_argb, 0);
   }

   conv->Release();
   delete[] area;
   return true;
}

//----------------------------

bool IImage::MixMipmap(dword flags){

   if(!IsInit() || (!lpTxt9 && !lpCTxt9)){
      assert(0);
      return false;
   }
   HRESULT hr;

   if(lpTxt9){
      dword num_levels = lpTxt9->GetLevelCount();
      if(num_levels<2)
         return true;

      D3DLOCKED_RECT *lrc = new D3DLOCKED_RECT[num_levels];
      for(dword i=0; i<num_levels; i++){
         dword lock_flags = (!i) ? (D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY) : 0;
         hr = lpTxt9->LockRect(i, &lrc[i], NULL, lock_flags);
         CHECK_D3D_RESULT("LockRect", hr);
      }
      MixMipmapOnSurface(lrc, i);
      while(i--){
         hr = lpTxt9->UnlockRect(i);
         CHECK_D3D_RESULT("UnlockRect", hr);
      }
      delete[] lrc;
   }else
   if(lpCTxt9){
      dword num_levels = lpCTxt9->GetLevelCount();
      if(num_levels<2)
         return true;

      D3DLOCKED_RECT *lrc = new D3DLOCKED_RECT[num_levels];
      for(int si=0; si<6; si++){

         for(dword i=0; i<num_levels; i++){
            dword lock_flags = (!i) ? (D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY) : 0;
            hr = lpCTxt9->LockRect((D3DCUBEMAP_FACES)si, i, &lrc[i], NULL, lock_flags);
            CHECK_D3D_RESULT("LockRect", hr);
         }
         MixMipmapOnSurface(lrc, i);
         while(i--){
            hr = lpCTxt9->UnlockRect((D3DCUBEMAP_FACES)si, i);
            CHECK_D3D_RESULT("UnlockRect", hr);
         }
      }
      delete[] lrc;
   }
   return true;
}

//----------------------------

bool IImage::ConvertBumpMap(CPIImage src_img, dword flags){

   if(!IsInit() || !src_img || !src_img->IsInit()){
      assert(0);
      return false;
   }
   if(src_img->SizeX()!=SizeX() || src_img->SizeY()!=SizeY()){
      assert(0);
      return false;
   }
   if(!(pixel_format.flags&PIXELFORMAT_BUMPMAP))
      return false;
   assert(!flags);
   if(flags)
      return false;

   bool b;

   void *src_a;
   dword src_pitch;
   byte src_bpp = (byte)src_img->GetPixelFormat()->bytes_per_pixel;
   b = const_cast<PIImage>(src_img)->Lock(&src_a, &src_pitch, true);
   if(!b){
      return false;
   }
                              //convert source map to RGB format
   struct S_rgb{
      byte b, g, r;
   };
   S_rgb *src_map = new S_rgb[size_x * size_y];

   S_pixelformat pf;
   memset(&pf, 0, sizeof(pf));
   pf.bytes_per_pixel = 3;
   pf.r_mask = 0x00ff0000;
   pf.g_mask = 0x0000ff00;
   pf.b_mask = 0x000000ff;
   LPC_rgb_conversion conv = igraph->GetRGBConv1(pf);

   dword *pal = src_img->GetPal();
   conv->Convert(src_a, src_map, size_x, size_y, src_pitch, 0, src_bpp, &pal, 0, NULL);
   conv->Release();
   const_cast<PIImage>(src_img)->Unlock();

   struct S_uv{
      byte l, v, u;
   };
   S_uv *dst_map = new S_uv[size_x * size_y];

                              //convert bump map
   const S_rgb *rgb_line_base = src_map;
   S_uv *uv_line_base = dst_map;
   for(dword y=0; y<size_y; y++, rgb_line_base += size_x, uv_line_base += size_x){
      const S_rgb *rgb_line = rgb_line_base;
      const S_rgb *rgb_up = !y ? (rgb_line_base + size_x * (size_y-1)) : (rgb_line_base - size_x);
      const S_rgb *rgb_down = (y==size_y-1) ? (rgb_line_base - size_x * (size_y-1)) : (rgb_line_base + size_x);
      S_uv *dst_line = uv_line_base;

      for(dword x=0; x<size_x; x++, ++rgb_up, ++rgb_down, ++rgb_line, ++dst_line){
                              //get ARGB values of four neighbours
         const S_rgb *rgb_left = !x ? (rgb_line + (size_x - 1)) : (rgb_line - 1);
         const S_rgb *rgb_right = (x==size_x-1) ? (rgb_line - (size_x - 1)) : (rgb_line + 1);

         byte bright_l = byte((rgb_left->r + rgb_left->g + rgb_left->b) / 3);
         byte bright_t = byte((rgb_up->r + rgb_up->g + rgb_up->b) / 3);
         byte bright_r = byte((rgb_right->r + rgb_right->g + rgb_right->b) / 3);
         byte bright_b = byte((rgb_down->r + rgb_down->g + rgb_down->b) / 3);

         dst_line->l = 0xff;
         dst_line->u = byte(bright_r - bright_l);
         dst_line->v = byte(bright_b - bright_t);
      }
   }
   delete[] src_map;

   void *dst_a;
   dword dst_pitch;
   b = Lock(&dst_a, &dst_pitch, false);
   if(!b){
      delete[] dst_map;
      return false;
   }
                              //use convertor to map UV data to texture
                              // (we fake the convertor to think it converts RGB data)
   LPC_rgb_conversion conv1 = igraph->GetRGBConv1(pixel_format);
   conv1->Convert(dst_map, dst_a, size_x, size_y, size_x*sizeof(S_uv), dst_pitch,
      3, NULL, 0, NULL);
   conv1->Release();

   delete[] dst_map;

   Unlock();

   return true;
}

//----------------------------

bool IImage::ConvertToNormalMap(CPIImage src_img, float scale){

   if(!IsInit() || !src_img || !src_img->IsInit()){
      assert(0);
      return false;
   }
   if(src_img->GetPixelFormat()->flags&PIXELFORMAT_COMPRESS)
      return false;
   if(pixel_format.flags&(PIXELFORMAT_PALETTE|PIXELFORMAT_BUMPMAP))
      return false;

   bool b;

   void *src_area;
   dword src_pitch;
   byte src_bpp = (byte)src_img->GetPixelFormat()->bytes_per_pixel;
   b = const_cast<PIImage>(src_img)->Lock(&src_area, &src_pitch, true);
   if(!b){
      return false;
   }
                              //convert source map to RGB format
   struct S_rgb{
      byte b, g, r;
   };
   S_rgb *src_map = new S_rgb[size_x * size_y];

   S_pixelformat pf;
   memset(&pf, 0, sizeof(pf));
   pf.bytes_per_pixel = 3;
   pf.r_mask = 0x00ff0000;
   pf.g_mask = 0x0000ff00;
   pf.b_mask = 0x000000ff;
   LPC_rgb_conversion conv = igraph->GetRGBConv1(pf);
   dword *pal = src_img->GetPal();
   conv->Convert(src_area, src_map, size_x, size_y, src_pitch, 0, src_bpp, &pal, 0, NULL);
   conv->Release();

   const_cast<PIImage>(src_img)->Unlock();

   struct S_normal_vertex{
      byte z, y, x;
   };
   S_normal_vertex *dst_map = new S_normal_vertex[size_x * size_y];

                              //convert bump map
   const S_rgb *rgb_line_base = src_map;
   S_normal_vertex *normal_line_base = dst_map;
   for(dword y=0; y<size_y; y++, rgb_line_base += size_x, normal_line_base += size_x){
      const S_rgb *rgb = rgb_line_base;
      const S_rgb *rgb_down = src_map + (((y+1)&(size_y-1)) * size_x);
      S_normal_vertex *dst_line = normal_line_base;

      for(dword x=0; x<size_x; x++, ++rgb, ++rgb_down, ++dst_line){
                              //get ARGB values of four neighbours
         const S_rgb *rgb_right = rgb_line_base + ((x+1)&(size_x-1));

         float height = (float)((rgb->r*77 + rgb->g*153 + rgb->b*26)>>8) / 255.0f;
         float height_down = (float)((rgb_down->r*77 + rgb_down->g*153 + rgb_down->b*26)>>8) / 255.0f;
         float height_right = (float)((rgb_right->r*77 + rgb_right->g*153 + rgb_right->b*26)>>8) / 255.0f;
         
         S_vector normal;
         normal.GetNormal(S_vector(0, 0, height*scale), S_vector(0, -1, height_down*scale), S_vector(1, 0, height_right*scale));
         normal.Normalize();

         dst_line->x = byte(FloatToInt(normal.x*127.0f) + 0x80);
         dst_line->y = byte(FloatToInt(normal.y*127.0f) + 0x80);
         dst_line->z = byte(FloatToInt(normal.z*127.0f) + 0x80);
      }
   }
   delete[] src_map;

   void *dst_area;
   dword dst_pitch;
   b = Lock(&dst_area, &dst_pitch, false);
   if(!b){
      delete[] dst_map;
      return false;
   }
                              //use convertor to map normal data to texture
   LPC_rgb_conversion conv1 = igraph->GetRGBConv1(pixel_format);
   conv1->Convert(dst_map, dst_area, size_x, size_y, size_x*sizeof(S_normal_vertex), dst_pitch, 3, NULL, 0, NULL);
   conv1->Release();

   delete[] dst_map;

   Unlock();
   return true;
}

//----------------------------

bool IImage::ApplyFunction(t_ApplyFunc *func, dword num_passes){

   if(!lpTxt9)
      return false;

   void *area;
   dword pitch;
   if(!Lock(&area, &pitch, false))
      return false;
                              //convert to ARGB
   S_argb *argb = new S_argb[size_x * size_y];
   if(!argb)
      return false;
   ConvertToARGB(argb, area, pitch, &pixel_format, pal_argb, size_x, size_y);

                              //apply function
   for(dword pi=0; pi<num_passes; pi++){
      for(dword y=0; y<size_y; y++){
         S_argb *line = argb + y*size_x;
         for(dword x=0; x<size_x; x++){
            S_argb &pixel = line[x];
            *(dword*)&pixel = (*func)(*(dword*)&pixel, x, y, pi);
         }
      }
   }
   //memset(argb, 0xff, size_x*4);
                              //convert back to surface format

   LPC_rgb_conversion conv = igraph->GetRGBConv1(pixel_format, pal_argb);
   conv->Convert(argb, area, size_x, size_y, size_x*4, pitch, 4, (dword**)&pal_argb, 0);
   conv->Release();

   delete[] argb;

   Unlock();
   return true;
}

//----------------------------

bool IImage::CopyStretched(CPIImage src_img){

   if(memcmp(&pixel_format, src_img->GetPixelFormat(), sizeof(pixel_format)))
      return false;

   /*
                              //try to use DX StretchRect function
   {
      IDirect3DDevice9 *lpDev = igraph->GetD3DDevice();
      IDirect3DSurface9 *src = src_img->GetDirectXSurface(), *dst = GetDirectXSurface();
      if(src && dst){
         HRESULT hr;
         hr = lpDev->StretchRect(src, NULL, dst, NULL, D3DTEXF_NONE);
         if(SUCCEEDED(hr))
            return true;
      }
   }
   */

   void *area_src, *area_dst;
   dword src_pitch, dst_pitch;
   byte bpp = (byte)src_img->GetPixelFormat()->bytes_per_pixel;
   if(!const_cast<PIImage>(src_img)->Lock(&area_src, &src_pitch, false))
      return false;
   if(!Lock(&area_dst, &dst_pitch, false)){
      const_cast<PIImage>(src_img)->Unlock();
      return false;
   }

   bool plain_copy = (src_img->SizeX()==size_x && src_img->SizeY()==size_y);

   float ratio_x = (float)src_img->SizeX() / (float)size_x, count_x;
   float ratio_y = (float)src_img->SizeY() / (float)size_y, count_y = ratio_y * .4999f;
   const byte *src = (const byte*)area_src;

   for(dword y = size_y; y--; area_dst = (byte*)area_dst + dst_pitch){
      assert(src < (((const byte*)area_src) + src_pitch * src_img->SizeY()));

      if(plain_copy){
         memcpy(area_dst, src, size_x*bpp);
         src += src_pitch;
      }else{
         const byte *src_x = src;
         byte *dst_x = (byte*)area_dst;
         count_x = ratio_x * .4999f;
         for(dword x = size_x; x--; dst_x += bpp){

            switch(bpp){
            case 1: *dst_x = *src_x; break;
            case 2: *(word*)dst_x = *(word*)src_x; break;
            case 3: *(word*)dst_x = *(word*)src_x; ((byte*)dst_x)[2] = ((byte*)src_x)[2]; break;
            case 4: *(dword*)dst_x = *(dword*)src_x; break;
            }
                                 //next row
            count_x += ratio_x;
            int i = FloatToInt((float)floor(count_x));
            src_x += bpp * i;
            count_x -= (float)i;
         }
                              //next line
         count_y += ratio_y;
         int i = FloatToInt((float)floor(count_y));
         src += src_pitch * i;
         count_y -= (float)i;
      }
   }
   Unlock();
   const_cast<PIImage>(src_img)->Unlock();

   return true;
}

//----------------------------

void FixBordersOfAlphaPixels(dword *mem, dword sx, dword sy){

   const dword ALPHA_MASK = 0xff000000;

   for(dword y=sy; y--; ){
      dword *line = mem + y*sx;
      const dword *top = mem + ((y-1)%sy)*sx;
      const dword *bot = mem + ((y+1)%sy)*sx;

                           //traverse all pixels on line
      for(dword x=sx; x--; ){
         dword &pixel = line[x];
         if(!(pixel&ALPHA_MASK)){
                           //translucent pixel, fix it

#if 0
                           //get four neighbours
            const dword *neighbours[4] = {
               line + (x-1)%sx,
               line + (x+1)%sx,
               top + x,
               bot + x
            };

                           //make blend
                           // algo: use rgb values shifted 2 bits right (throwing away lowest 2 bits), and accumulate them together
                           // this way, values in byte can't overflow, and we don't need to separate the values
            dword num_nt = 0;
            dword sum = 0;
            for(dword i=4; i--; ){
               dword n = *neighbours[i];
               if(n&ALPHA_MASK){
                  sum += (n>>2)&0x3f3f3f;
                  ++num_nt;
               }
            }
                           //fix color for number of valid neighbours
            switch(num_nt){
            case 0: pixel = 0x00ff00; break;
            case 1: pixel = sum << 2; break;
            case 2: pixel = sum << 1; break;
            case 3:
               pixel = sum;
                           //add 1/3 of all elements
               pixel += (((sum>>16)&0xff)/3) << 16;
               pixel += (((sum>>8)&0xff)/3) << 8;
               pixel += (sum&0xff)/3;
               break;
            case 4: pixel = sum; break;
            }
#else
                           //get four neighbours
            dword x_l = (x-1) % sx;
            dword x_r = (x+1) % sx;
            const dword *neighbours[8] = {
               top + x_l, top + x, top + x_r,
               line + x_l,        line + x_r,
               bot + x_l, bot + x, bot + x_r
            };

                           //make blend from all neighbours
            dword num_nt = 0;
            dword r = 0, g = 0, b = 0;
            for(dword i=8; i--; ){
               dword n = *neighbours[i];
               if(n&ALPHA_MASK){
                  r += (n>>16)&0xff;
                  g += (n>>8)&0xff;
                  b += n&0xff;
                  ++num_nt;
               }
            }
                           //fix color for number of valid neighbours
            switch(num_nt){
            //case 0: g = 255; break;
            case 0: continue;
            case 1: break;
            case 2: r >>= 1; g >>= 1; b >>= 1; break;
            case 3: r /= 3; g /= 3; b /= 3; break;
            case 4: r >>= 2; g >>= 2; b >>= 2; break;
            case 5: r /= 5; g /= 5; b /= 5; break;
            case 6: r /= 6; g /= 6; b /= 6; break;
            case 7: r /= 7; g /= 7; b /= 7; break;
            case 8: r >>= 3; g >>= 3; b >>= 3; break;
            }
            pixel = (r<<16) | (g<<8) | b;
#endif
            //pixel = 0x00ff00;  //debug
         }
      }
   }
}

//----------------------------
//----------------------------

