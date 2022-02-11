#include "all.h"
#include "loader.h"
extern"C"{
#include "png\png.h"
}

void FixBordersOfAlphaPixels(dword *mem, dword sx, dword sy);

//----------------------------

static void PNGAPI ReadFromCache(png_structp png_ptr, png_bytep buf, png_size_t size){
   ((C_cache*)png_get_io_ptr(png_ptr))->read(buf, size);
}

//----------------------------

static png_voidp lpng_malloc(png_structp, size_t sz){
   return new byte[sz];
}

static void lpng_free(png_structp, png_voidp p){
   delete[] (byte*)p;
}

static void PNGAPI PNG_Error(png_structp png_ptr, png_const_charp err_msg){
   C_str s(err_msg);
   throw(s);
}

//----------------------------

class C_png_loader: public C_image_loader{

//----------------------------

   E_LOADER_RESULT Load(C_cache &ck, void *header, dword hdr_size, void **mem1, void **pal1,
      dword *size_x, dword *size_y, S_pixelformat *pf, bool only_header, C_str &err){

      if(hdr_size<4)
         return IMGLOAD_BAD_FORMAT;

      if(!png_check_sig((byte*)header, hdr_size))
         return IMGLOAD_BAD_FORMAT;

      //png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, this, PNG_Error, PNG_Error);
      png_structp png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, this, PNG_Error, PNG_Error,
         NULL, lpng_malloc, lpng_free);
      if(!png_ptr){
         err = "failed to allocate memory";
         return IMGLOAD_READ_ERROR;
      }
      E_LOADER_RESULT ret = IMGLOAD_READ_ERROR;
      png_infop info_ptr = png_create_info_struct(png_ptr);
      if(info_ptr){
         try{

         png_set_bgr(png_ptr);

         png_set_read_fn(png_ptr, (void*)&ck, &ReadFromCache);

         png_read_info(png_ptr, info_ptr);

         memset(pf, 0, sizeof(S_pixelformat));
         pf->bytes_per_pixel = info_ptr->pixel_depth / 8;
         if(info_ptr->num_palette && info_ptr->bit_depth==8)
            pf->bytes_per_pixel = 1;
                              //if 8-bit grayscale, consider it as RGB
         *size_x = info_ptr->width;
         *size_y = info_ptr->height;

         bool ok = true;
         switch(pf->bytes_per_pixel){
         case 1:
            pf->flags = PIXELFORMAT_PALETTE;
            break;
         case 2:
            if(info_ptr->color_type==PNG_COLOR_TYPE_GRAY_ALPHA){
               pf->bytes_per_pixel = 1;
               pf->flags = PIXELFORMAT_PALETTE;
               png_set_strip_alpha(png_ptr);
            }else
               ok = false;
            break;
         case 3:
            pf->r_mask = 0x00ff0000;
            pf->g_mask = 0x0000ff00;
            pf->b_mask = 0x000000ff;
            pf->a_mask = 0x00000000;
            break;
         case 4:
            pf->r_mask = 0x00ff0000;
            pf->g_mask = 0x0000ff00;
            pf->b_mask = 0x000000ff;
            pf->a_mask = 0xff000000;
            pf->flags |= PIXELFORMAT_ALPHA;
            break;
         default:
            ok = false;
         }
         if(!ok){
            err = "unknown pixel format";
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            return IMGLOAD_READ_ERROR;
         }

         if(!only_header){
            switch(pf->bytes_per_pixel){
            case 1:
            case 3:
            case 4:
               {
                              //supported depth, continue reading
                              //alloc screen memory
                  byte *mem = (byte*)MemAlloc((*size_x) * (*size_y) * pf->bytes_per_pixel);
                  if(!mem)
                     break;
                                          //setup pointer to palette
                  byte *pal_argb = NULL;
                  if(pf->bytes_per_pixel == 1){
                     pal_argb = (byte*)MemAlloc(256 * sizeof(dword));
                     if(!pal_argb){
                        MemFree(mem);
                        break;
                     }
                     switch(info_ptr->color_type){
                     case PNG_COLOR_TYPE_GRAY:
                     case PNG_COLOR_TYPE_GRAY_ALPHA:
                        {
                              //grayscale - create fake palette
                           for(int i=0, mask = 0; i<256; i++, mask += 0x010101)
                              ((dword*)pal_argb)[i] = mask;
                        }
                        break;
                     default:
                        {
                              //real palette, fill-in indicies
                           const byte *pp = (const byte*)png_ptr->palette;
                           for(int pi=png_ptr->num_palette; pi--; ){
                              pal_argb[pi*4+0] = pp[pi*3+0];
                              pal_argb[pi*4+1] = pp[pi*3+1];
                              pal_argb[pi*4+2] = pp[pi*3+2];
                              pal_argb[pi*4+3] = 0;
                           }
                        }
                     }
                  }
                  *mem1 = mem;
                  *pal1 = pal_argb;

                  byte **ptr_buf = new byte*[*size_y];

                  for(int y=*size_y; y--; ){
                     ptr_buf[y] = mem + y * (*size_x) * pf->bytes_per_pixel;
                  }
                  png_read_image(png_ptr, ptr_buf);

                  delete[] ptr_buf;

                  ret = IMGLOAD_OK;

                              //prepare images containing alpha for proper blending
                  if(pf->bytes_per_pixel==4 && (pf->flags&PIXELFORMAT_ALPHA)){
                     FixBordersOfAlphaPixels((dword*)mem, *size_x, *size_y);
                  }
               }
               break;
            }
         }else{
            ret = IMGLOAD_OK;
         }
         }
         catch(C_str err1){
            err = err1;            
         }
      }
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return ret;
   }

public:
//----------------------------

   virtual E_LOADER_RESULT GetImageInfo(C_cache &ck, void *header, dword hdr_size,
      dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      return Load(ck, header, hdr_size, NULL, NULL, size_x, size_y, pf, true, err);
   }

//----------------------------

   virtual E_LOADER_RESULT Load(C_cache &ck, void *header, dword hdr_size,
      void **mem1, void **pal1, dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      return Load(ck, header, hdr_size, mem1, pal1, size_x, size_y, pf, false, err);
   }
};

C_image_loader *CreateLoaderPNG(){
   return new C_png_loader;
}

//----------------------------
//----------------------------

