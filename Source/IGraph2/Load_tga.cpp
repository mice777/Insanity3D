#include "all.h"
#include "loader.h"

//----------------------------
//#pragma warning(disable: 4189)

//----------------------------

class C_tga_loader: public C_image_loader{

   enum{
      UL_TGA_LR = 0x10,
      UL_TGA_BT = 0x20,
   };

   struct S_tmp_info{
      byte id_length;
      byte cm_entry_size;
      word cm_length;
      byte type;
      byte desc;
      byte bits_per_pixel;
   };

//----------------------------

   virtual E_LOADER_RESULT GetImageInfo(C_cache &ck, void *header, dword hdr_size,
      dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err, S_tmp_info &inf){

      if(hdr_size<4)
         return IMGLOAD_BAD_FORMAT;
      dword sig = *(dword*)header;
      switch(sig&0x00ffffff){
      case 0x00010100:        //PAL
      case 0x00020000:        //RGB
      case 0x00030000:        //grayscale
      case 0x00090100:        //PAL + RLE
      case 0x000a0000:        //RGB + RLE
         break;
      default:
         return IMGLOAD_BAD_FORMAT;
      }

      inf.id_length = ck.ReadByte();
      //byte m_color_map_type =
         ck.ReadByte();
      inf.type = ck.ReadByte();

      //word m_cm_index =
         ck.ReadWord();
      inf.cm_length = ck.ReadWord();

      inf.cm_entry_size = ck.ReadByte();

      //word m_x_org =
         ck.ReadWord();
      //word m_y_org =
         ck.ReadWord();
      *size_x = ck.ReadWord();
      *size_y = ck.ReadWord();

      inf.bits_per_pixel = ck.ReadByte();
      inf.desc = ck.ReadByte();

      memset(pf, 0, sizeof(S_pixelformat));
      pf->bytes_per_pixel = inf.bits_per_pixel/8;

      switch(inf.bits_per_pixel){
      case 8:
         pf->flags = PIXELFORMAT_PALETTE;
         break;

      case 16:
         pf->bytes_per_pixel = 3;
         pf->r_mask = 0x00ff0000;
         pf->g_mask = 0x0000ff00;
         pf->b_mask = 0x000000ff;
         break;

      case 32:
         pf->flags = PIXELFORMAT_ALPHA;
         pf->a_mask = 0xff000000;
      case 24:
         pf->r_mask = 0x00ff0000;
         pf->g_mask = 0x0000ff00;
         pf->b_mask = 0x000000ff;
         break;
      default:
         err = "invalid pixel depth";
         return IMGLOAD_BAD_FORMAT;
      }
      return IMGLOAD_OK;
   }

//----------------------------

   struct S_rgb{
      byte r, g, b;
   };

   static S_rgb Read16BitPixel(C_cache &ck){

      S_rgb ret;
      word p = ck.ReadWord();

      ret.r = byte((p<<3) & 0xf8);
      ret.g = byte((p>>2) & 0xf8);
      ret.b = byte((p>>7) & 0xf8);
      return ret;
   }

//----------------------------
public:

//----------------------------

   virtual E_LOADER_RESULT GetImageInfo(C_cache &ck, void *header, dword hdr_size,
      dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      S_tmp_info tmp;
      return GetImageInfo(ck, header, hdr_size, size_x, size_y, pf, err, tmp);
   }

//----------------------------

   virtual E_LOADER_RESULT Load(C_cache &ck, void *header, dword hdr_size,
      void **mem1, void **pal1, dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err){

      S_tmp_info inf;
      E_LOADER_RESULT res = GetImageInfo(ck, header, hdr_size, size_x, size_y, pf, err, inf);
      if(res)
         return res;

      //int right = inf.desc & UL_TGA_LR;
      bool flipped = !(inf.desc&UL_TGA_BT);

      byte m_id[256];
      ck.read(m_id, inf.id_length);

                              //alloc buffer
      *mem1 = MemAlloc((*size_x) * (*size_y) * pf->bytes_per_pixel);
      if(!*mem1){
         err = "failed to allocate memory";
         return IMGLOAD_READ_ERROR;
      }
      *pal1 = NULL;
      if(pf->bytes_per_pixel==1){
         *pal1 = MemAlloc(256 * sizeof(dword));
         dword *pal = (dword*)*pal1;
         if(inf.cm_entry_size){
            memset(pal, 0, 256*sizeof(dword));
            for(dword i=0; i<inf.cm_length; i++){
               union{
                  dword argb;
                  struct S_u{
                     byte b, g, r, a;
                  } u;
               } e;
               ck.read(&e, inf.cm_entry_size/8);
               Swap(e.u.r, e.u.b);
               pal[i] = e.argb;
            }
         }else{
                                       //grayscale - create fake palette
            for(int i=0, mask = 0; i<256; i++, mask += 0x010101)
               pal[i] = mask;
         }
      }

      int sx = *size_x, sy = *size_y;
      switch(inf.type){
      case 1:                 //PAL
      case 2:                 //RGB
      case 3:                 //GRAYSCALE
         switch(inf.bits_per_pixel){
         case 16:
            {
               assert(pf->bytes_per_pixel == 3);
               for(int y=0; y<sy; y++){
                  byte *line = (byte*)*mem1;
                  line += sx * 3 * (!flipped ? y : sy-y-1);
                  for(dword x=sx; x--; line += 3)
                     (S_rgb&)*line = Read16BitPixel(ck);
               }
            }
            break;

         case 8:
         case 24:
         case 32:
            {
                              //read all lines (bottom-to-top)
               for(int y=0; y<sy; y++){
                  byte *line = (byte*)*mem1;
                  line += sx * pf->bytes_per_pixel * (!flipped ? y : sy-y-1);
                  ck.read(line, sx * pf->bytes_per_pixel);
               }
            }
            break;
         default: assert(0);
         }
         break;

      case 9:                 //RLE PAL
      case 10:                //RLE RGB
         switch(inf.bits_per_pixel){
         case 8:
         case 24:
         case 32:
            {
               byte *dest = (byte*)*mem1;

               int x = 0, y = 0;
               while(y < sy){
                  byte code = ck.ReadByte();
                  if(code < 0x80){
                     ++code;
                     while(code--){
                        if(y==sy)
                           throw C_except("data corrupted");
                        ck.read(&dest[((!flipped?y:sy-y-1)*sx+x)*pf->bytes_per_pixel], pf->bytes_per_pixel);
                        if(++x==sx){
                           ++y;
                           x = 0;
                        }
                     }
                  }else{
                     code -= 127;
                     dword fill = 0;
                     ck.read(&fill, pf->bytes_per_pixel);
                     while(code--){
                        if(y==sy)
                           throw C_except("data corrupted");
                        memcpy(&dest[((!flipped?y:sy-y-1)*sx+x)*pf->bytes_per_pixel], &fill, pf->bytes_per_pixel);
                        if(++x==sx){
                           ++y;
                           x = 0;
                        }
                     }
                  }
               }
               assert(x==0);
            }
            break;

         case 16:
            {
               assert(pf->bytes_per_pixel==3);
               byte *dest = (byte*)*mem1;

               int x = 0, y = 0;
               while(y < sy){
                  byte code = ck.ReadByte();
                  if(code < 0x80){
                     ++code;
                     while(code--){
                        if(y==sy)
                           throw C_except("data corrupted");
                        (S_rgb&)dest[((!flipped?y:sy-y-1)*sx+x)*3] = Read16BitPixel(ck);
                        if(++x==sx){
                           ++y;
                           x = 0;
                        }
                     }
                  }else{
                     code -= 127;
                     S_rgb fill = Read16BitPixel(ck);
                     while(code--){
                        if(y==sy)
                           throw C_except("data corrupted");
                        (S_rgb&)dest[((!flipped?y:sy-y-1)*sx+x)*pf->bytes_per_pixel] = fill;
                        if(++x==sx){
                           ++y;
                           x = 0;
                        }
                     }
                  }
               }
               assert(x==0);
            }
            break;

         default: assert(0);
         }
         break;
      }
      return IMGLOAD_OK;
   }
};

C_image_loader *CreateLoaderTGA(){
   return new C_tga_loader;
}


//----------------------------
//----------------------------
