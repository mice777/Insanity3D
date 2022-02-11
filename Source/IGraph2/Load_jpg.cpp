#include "all.h"
#include "loader.h"

//----------------------------

bool ReadJPGHeader(C_cache &ck, dword &size_x, dword &size_y, byte &bpp);

bool LoadJPG(C_cache &ck, dword size_x, dword size_y, void *area, byte *pal, byte bpp);

//----------------------------

class C_jpg_loader: public C_image_loader{
public:

//----------------------------

   virtual E_LOADER_RESULT GetImageInfo(C_cache &ck, void *header, dword hdr_size,
      dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      if(hdr_size<4)
         return IMGLOAD_BAD_FORMAT;
      dword sig = *(dword*)header;
      if((sig&0xfeffffff) != 0xe0ffd8ff)
         return IMGLOAD_BAD_FORMAT;

      byte bpp;
      bool b = ReadJPGHeader(ck, *size_x, *size_y, bpp);
      if(!b || bpp!=3)
         return IMGLOAD_READ_ERROR;
      memset(pf, 0, sizeof(S_pixelformat));
      pf->r_mask = 0x00ff0000;
      pf->g_mask = 0x0000ff00;
      pf->b_mask = 0x000000ff;
      pf->bytes_per_pixel = bpp;

      return IMGLOAD_OK;
   }

//----------------------------

   virtual E_LOADER_RESULT Load(C_cache &ck, void *header, dword hdr_size,
      void **mem1, void **pal1, dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      size_t offs = ck.tellg();
      E_LOADER_RESULT res = GetImageInfo(ck, header, hdr_size, size_x, size_y, pf, err);
      if(res)
         return res;
      ck.seekg(offs);

                              //alloc buffer
      *mem1 = MemAlloc((*size_x) * (*size_y) * pf->bytes_per_pixel);
      if(!*mem1){
         err = "failed to allocate memory";
         return IMGLOAD_READ_ERROR;
      }
      *pal1 = NULL;
      bool b = LoadJPG(ck, *size_x, *size_y, *mem1, NULL, (byte)pf->bytes_per_pixel);
      if(b)
         return IMGLOAD_OK;
      MemFree(*mem1);
      *mem1 = NULL;

      return IMGLOAD_READ_ERROR;
   }
};

C_image_loader *CreateLoaderJPG(){
   return new C_jpg_loader;
}


//----------------------------
//----------------------------
