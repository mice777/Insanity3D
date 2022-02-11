#include "all.h"
#include "loader.h"

//----------------------------

class C_pcx_loader: public C_image_loader{
public:

//----------------------------

   virtual E_LOADER_RESULT GetImageInfo(C_cache &ck, void *header, dword hdr_size,
      dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      if(hdr_size<4)
         return IMGLOAD_BAD_FORMAT;
      if((*(dword*)header) != 0x0801050a)
         return IMGLOAD_BAD_FORMAT;

      long offs = ck.tellg();

      word sx, sy;
      byte bpp;

                              //get size
      ck.seekg(offs+0xa);
      //sx = ck.ReadWord();
      //++sx;
      sy = ck.ReadWord();
      ++sy;
                              //get bpp
      ck.seekg(offs+0x41);
      ck.read((char*)&bpp, sizeof(byte));
                              //get size x
      ck.read((char*)&sx, sizeof(word));
                              //get to data beg
      ck.seekg(offs+0x80);

                              //ok, we've identified file and allocated memory
                              //setup return values
      *size_x = sx;
      *size_y = sy;
      memset(pf, 0, sizeof(S_pixelformat));
      pf->bytes_per_pixel = bpp;
      if(bpp==1){
         pf->flags = PIXELFORMAT_PALETTE;
      }else{
         pf->r_mask = 0xff0000;
         pf->g_mask = 0x00ff00;
         pf->b_mask = 0x0000ff;
      }
      return IMGLOAD_OK;
   }

//----------------------------

   virtual E_LOADER_RESULT Load(C_cache &ck, void *header, dword hdr_size,
      void **mem1, void **pal1, dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      E_LOADER_RESULT res = GetImageInfo(ck, header, hdr_size, size_x, size_y, pf, err);
      if(res)
         return res;
      dword sx = *size_x;
      dword sy = *size_y;
      dword bpp = pf->bytes_per_pixel;

                              //try to allocate memory
      byte *mem = (byte*)MemAlloc(sx*sy*bpp);
      if(!mem)
         return IMGLOAD_READ_ERROR;
                              //setup pointer to palette
      byte *pal_argb = NULL;
      if(bpp==1){
         pal_argb = (byte*)MemAlloc(256 * sizeof(dword));
         if(!pal_argb){
            MemFree(mem);
            return IMGLOAD_READ_ERROR;
         }
      }
      *mem1 = mem;
      *pal1 = pal_argb;


      byte color = 0;

                              //do all lines
      while(sy--){
                              //do all planes
         int plane = bpp - 1;
         dword last_rept = 0;
         do{
            byte *dst = &mem[plane];
                                 //do entire row
            dword row = 0;
            while(row < sx){
               if(last_rept){
                  dword rept = last_rept;
                  last_rept = 0;
                  if((row += rept) > sx)
                     rept -= (row - sx), last_rept = (row - sx);
                  if(bpp==1){
                     memset(dst, color, rept);
                     dst += rept;
                  }else
                  do{
                     *dst = color;
                     dst += bpp;
                  }while(--rept);
               }else{
                  byte code = ck.ReadByte();
                  if(code>=0xc0){
                                    //color row
                     dword rept = code - 0xc0;
                     color = ck.ReadByte();
                     if((row += rept) > sx)
                        rept -= (row - sx), last_rept = (row - sx);
                     if(bpp==1){
                        memset(dst, color, rept);
                        dst += rept;
                     }else
                     do{
                        *dst = color;
                        dst += bpp;
                     }while(--rept);
                  }else{
                                    //single color
                     ++row;
                     *dst = code;
                     dst += bpp;
                  }
               }
            }
         }while(--plane >= 0);
         mem += sx*bpp;
      }
                                 //read palette
      if(bpp==1){
         ck.seekg(ck.tellg()+1);
         byte *pal_3 = new byte[768];
         ck.read(pal_3, 768);
         for(int i=0; i<256; i++){
            pal_argb[i*4 + 0] = pal_3[i*3 + 0];
            pal_argb[i*4 + 1] = pal_3[i*3 + 1];
            pal_argb[i*4 + 2] = pal_3[i*3 + 2];
            pal_argb[i*4 + 3] = 0xff;
         }
         delete[] pal_3;
      }
      return IMGLOAD_OK;
   }
};

C_image_loader *CreateLoaderPCX(){
   return new C_pcx_loader;
}

//----------------------------
//----------------------------
