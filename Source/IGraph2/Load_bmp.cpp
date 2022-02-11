#include "all.h"
#include "loader.h"


#define COMPRESSED            //include de-compression code

//----------------------------

class C_bmp_loader: public C_image_loader{
#pragma pack(push,1)
   struct S_bmp_header{
      dword size;
      word res[2];
      dword data_offs;
      struct{
         dword size;
         long sx, sy;
         word planes;
         word bitcount;
         dword compression;
         dword img_size;
         long x_ppm;
         long y_ppm;
         dword num_clr;
         dword clr_important;
      }bi_hdr;
   } bmp_header;
#pragma pack(pop)

public:

//----------------------------

   virtual E_LOADER_RESULT GetImageInfo(C_cache &ck, void *header, dword hdr_size,
      dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      if(hdr_size<4)
         return IMGLOAD_BAD_FORMAT;
      if((*(word*)header) != 0x4d42)
         return IMGLOAD_BAD_FORMAT;

      size_t offs = ck.tellg();
      int i;
      ck.seekg(offs+2);
                              //read header
      i = ck.read((char*)&bmp_header, sizeof(bmp_header));
                              //error reading file header
      if(i != sizeof(bmp_header))
         return IMGLOAD_BAD_FORMAT;
                              //get to data beg
      ck.seekg(offs);

      dword sx = bmp_header.bi_hdr.sx;
      dword sy = bmp_header.bi_hdr.sy;
      byte bpp = (byte)(bmp_header.bi_hdr.bitcount / 8);

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

      if(hdr_size<4)
         throw C_except_loader("bad format");
      if((*(word*)header) != 0x4d42)
         throw C_except_loader("bad format");

      size_t offs = ck.tellg();
      int i;
      ck.seekg(offs+2);
                              //read header
      i = ck.read((char*)&bmp_header, sizeof(bmp_header));
                              //error reading file header
      if(i != sizeof(bmp_header))
         throw C_except_loader("bad format");
                              //get to data beg
      ck.seekg(bmp_header.data_offs + offs);

                              //check if it's supported bit-count
      switch(bmp_header.bi_hdr.bitcount){
      case 8:
      case 24:
         break;
      default:
         throw C_except_loader("invalid bitdepth");
      }
      dword sx = bmp_header.bi_hdr.sx;
      dword sy = bmp_header.bi_hdr.sy;
      byte bpp = (byte)(bmp_header.bi_hdr.bitcount / 8);

      byte *mem = NULL;
      byte *pal_argb = NULL;
      byte *pal = NULL;
      try{
                              //alloc screen memory
         mem = (byte*)MemAlloc(sx*sy*bpp);
                              //setup pointer to palette
         if(bpp == 1)
            pal_argb = (byte*)MemAlloc(256 * sizeof(dword));

                              //ok, we've identified file and allocated memory
                              //setup return values
         *mem1 = mem;
         *pal1 = pal_argb;
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
                                 //scanline is dword aligned,
                                 //compute padding length
         dword padding;
         padding = ((sx*bpp+3) & -4) - sx*bpp;

         switch(bmp_header.bi_hdr.compression){
         case BI_RGB:            //uncompressed data
                                 //read bottom-to-top
            mem += sx*sy*bpp;
            while(sy--){
               mem -= sx*bpp;
               ck.read((char*)mem, sx*bpp);
               if(padding)
                  ck.seekg(ck.tellg()+padding);
            };
            break;
         case BI_RLE8:           //compressed data
   #ifndef COMPRESSED
            throw C_except_loader("bad format");
   #else
            while(sy--){
               mem = ((byte*)*mem1) + sy*sx*bpp;
               while(true){
                  union{
                     byte code[2];
                     word wcode;
                  } au;
                  ck.read((char*)&au.wcode, 2);
                  if(!au.code[0]){
                     switch(au.code[1]){
                     case 0:     //eol
                        goto eol;
                     case 1:     //eof
                        goto eof;
                     case 2:     //delta
                        ck.read((char*)&au.wcode, 2);
                        break;
                     default:    //data length
                        ck.read((char*)&mem, au.code[1]);
                        mem += au.code[1];
                                 //align to word
                        if(au.code[1]&1)
                           ck.seekg(ck.tellg()+1);
                     }
                  }else{         //color fill
                     memset(mem, au.code[1], au.code[0]);
                     mem += au.code[0];
                  }
               }
   eol:{}
           }
   eof:
            break;
   #endif
         default:
            throw C_except_loader("bad format");
         }
                                 //read palette
         if(bpp==1){
            i = bmp_header.bi_hdr.num_clr;
            if(!i)
               i = 256;
            pal = new byte[sizeof(dword) * i];

            ck.seekg(offs + sizeof(bmp_header)+2);
            ck.read((char*)pal, i * sizeof(dword));
                                 //fill the rest of palette by zero
            memset(pal_argb + i*sizeof(dword), 0, (256-i)*sizeof(dword));
            while(i--){
               pal_argb[i*4 + 0] = pal[i*4+2];
               pal_argb[i*4 + 1] = pal[i*4+1];
               pal_argb[i*4 + 2] = pal[i*4+0];
               pal_argb[i*4 + 3] = 0xff;
            }
            delete[] pal;
         }
      } catch(...){
         MemFree(mem);
         MemFree(pal_argb);
         delete[] pal;
         throw;
      }
      return IMGLOAD_OK;
   }
};

//----------------------------
C_image_loader *CreateLoaderBMP(){
   return new C_bmp_loader;
}
