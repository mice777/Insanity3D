#include "all.h"
#include "loader.h"
extern"C"{
#include "png\png.h"
}

//----------------------------

static void PNGAPI WriteCache(png_structp png_ptr, png_bytep buf, png_size_t size){

   C_cache *ck = (C_cache*)png_get_io_ptr(png_ptr);
   ck->write((char*)buf, size);
}

//----------------------------

static void PNGAPI FlushCache(png_structp png_ptr){

   C_cache *ck = (C_cache*)png_get_io_ptr(png_ptr);
   ck->flush();
}

//----------------------------

bool WritePNG(C_cache *ck, const void *buf, int sz_x, int sz_y, byte bpp, dword pitch){

   bool ok = false;

   png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if(png_ptr){

      png_infop info_ptr = png_create_info_struct(png_ptr);
      if(info_ptr){

         info_ptr->width = sz_x;
         info_ptr->height = sz_y;
         info_ptr->bit_depth = 8;
         info_ptr->color_type = PNG_COLOR_TYPE_RGB;
         info_ptr->interlace_type = 0;
         png_set_bgr(png_ptr);

         png_set_write_fn(png_ptr, (void*)ck, &WriteCache, &FlushCache);

         png_write_info(png_ptr, info_ptr);

         for(int y=sz_y; y--; )
            png_write_row(png_ptr, ((byte*)buf) + pitch * y);

         png_write_end(png_ptr, info_ptr);
         ok = true;

         png_destroy_info_struct(png_ptr, &info_ptr);
      }
      png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
   }

   return ok;
}

//----------------------------
//----------------------------
