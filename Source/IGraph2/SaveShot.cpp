#include "all.h"
#define I2D_FULL
#include "igraph_i.h"

//----------------------------

bool WritePNG(C_cache *ck, const void *buf, int sz_x, int sz_y, byte bpp, dword pitch);
bool SaveJPG(C_cache *ck, dword size_x, dword size_y, const void *area, byte bpp);

//----------------------------

#define CACHE_SIZE 0x8000         //cache size

//----------------------------

dword __cdecl ror(dword val, byte count){
   __asm{
      mov cl, count;
      ror val, cl;
   }
   return val;
}

static const byte CODE_C1 = 0xc1;

#pragma pack(push, 1)
struct S_rgb{
   byte r, g, b;
};
#pragma pack(pop)

//----------------------------

inline byte pixel_to_color(dword rm, dword gm, dword bm, dword rp, dword gp, dword bp, const void *p, byte bpp, int plane){

   switch(bpp){
   case 1:
      return *(byte*)p;
      break;
   case 2:
      switch(plane){
      case 0: return (byte)ror((*(word*)p)&rm, (byte)rp);
      case 1: return (byte)ror((*(word*)p)&gm, (byte)gp);
      default: return (byte)ror((*(word*)p)&bm, (byte)bp);
      }
   case 3:
   case 4:
      return ((byte*)p)[2-plane];
   }
   return 0;
}

//----------------------------

inline int FindLastBit(dword val){
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

//----------------------------

bool IImage::SaveShot(C_cache *ck, const char *ext) const{

   void *base;
   dword pitch;
   if(!const_cast<PIImage>(this)->Lock(&base, &pitch, true)){
      OutputDebugString("failed to lock image");
      return false;
   }

   bool ok = true;

   const S_pixelformat &pf = pixel_format;

   int num_planes = 3;
   dword r_pos = (FindLastBit(pf.r_mask)-7) & 31;
   dword g_pos = (FindLastBit(pf.g_mask)-7) & 31;
   dword b_pos = (FindLastBit(pf.b_mask)-7) & 31;

   if(!stricmp(ext, "pcx")){
                              //prepare header
      union{
         byte b[128];
         word w[64];
         dword d[32];
      } header;
      memset(&header, 0, sizeof(header));

      header.d[0] = 0x0801050a;
      header.w[4] = (word)((header.w[0x21] = header.w[6] = (word)size_x) - 1);
      header.w[5] = (word)((header.w[7] = (word)size_y) - 1);
                              //# of planes
      header.b[0x41] = (byte)num_planes;
                              //write header
      ck->write((const char*)&header, sizeof(header));

      int i;
      byte count;
      byte last;
      void *mem;
      dword line_count = size_y;

      struct S_hlp{
         static void StoreData(C_cache *ck, byte count, byte data){

            if(count==1){
               if(data >= 0xc0)
                  ck->write((const char*)&CODE_C1, sizeof(byte));
            }else{
               if(!count)
                  return;
               count += 0xc0;
               ck->write((const char*)&count, sizeof(byte));
            }
            ck->write((const char*)&data, sizeof(byte));
         }
      };

                              //line loop
      while(line_count--){
         int plane = 0;
         do{
            i = count = 0;
            mem = base;
            last = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
               mem, (byte)pf.bytes_per_pixel, plane);
            while(i < (int)size_x){
               byte new1 = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
                  mem, (byte)pf.bytes_per_pixel, plane);
               if(last==new1){
                              //same color, use counting
                  if(++count == 0x3f){
                              //overflow, write
                     count += 0xc0;
                     ck->write((const char*)&count, sizeof(byte));
                     ck->write((const char*)&last, sizeof(byte));
                     count = 0;
                  }
               }else{
                              //write previous and start counting again
                  S_hlp::StoreData(ck, count, last);
                  last = new1;
                  count = 1;
               }
                              //next pixel
               ++i;
               mem = ((byte*)mem) + pf.bytes_per_pixel;
            }
                              //store rest of line
            S_hlp::StoreData(ck, count, last);
                              //next color plane
         }while(++plane < num_planes);
                              //next line
         base = ((byte*)base)+pitch;
      }
      assert(num_planes!=1);
   }else
   if(!stricmp(ext, "bmp")){
      BITMAPINFOHEADER bh;
      memset(&bh, 0, sizeof(bh));
      bh.biSize = sizeof(bh);
      bh.biWidth = size_x;
      bh.biHeight = size_y;
      bh.biPlanes = 1;
      bh.biBitCount = 24;
      bh.biCompression = BI_RGB;
      bh.biSizeImage = 3*size_x*size_y;
      bh.biXPelsPerMeter = 0;
      bh.biYPelsPerMeter = 0;
      bh.biClrUsed = 0;
      bh.biClrImportant = 0;

      dword base_pos = ck->tellg();
      word id = 0x4d42;
      ck->write((const char*)&id, sizeof(word));
      dword res = 0;
      ck->write((const char*)&res, sizeof(dword));
      ck->write((const char*)&res, sizeof(dword));
      dword offs = 14 + sizeof(bh);
      ck->write((const char*)&offs, sizeof(dword));
      ck->write((const char*)&bh, sizeof(bh));

      S_rgb *line = new S_rgb[size_x];

      base = (byte*)base + pitch * size_y;
      for(int y=size_y; y--; ){
         base = (byte*)base - pitch;
         const byte *mem = (const byte*)base;
         for(dword x=0; x<size_x; x++, mem += pf.bytes_per_pixel){
            S_rgb &rgb = line[x];
            rgb.r = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
               mem, (byte)pf.bytes_per_pixel, 2);
            rgb.g = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
               mem, (byte)pf.bytes_per_pixel, 1);
            rgb.b = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
               mem, (byte)pf.bytes_per_pixel, 0);
         }
         ck->write((const char*)line, size_x*sizeof(S_rgb));
      }
      delete[] line;
                              //write back file size
      dword size = ck->tellg() - base_pos;
      ck->seekp(base_pos+2);
      ck->write((const char*)&size, sizeof(dword));
      ck->seekp(base_pos + size);
   }else
   if(!stricmp(ext, "png")){
      S_rgb *buf = new S_rgb[size_x*size_y];
      for(int y=size_y; y--; ){
         S_rgb *line = buf + y*size_x;
         const byte *mem = (const byte*)base;
         for(dword x=0; x<size_x; x++, mem += pf.bytes_per_pixel){
            S_rgb &rgb = line[x];
            rgb.r = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
               mem, (byte)pf.bytes_per_pixel, 2);
            rgb.g = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
               mem, (byte)pf.bytes_per_pixel, 1);
            rgb.b = pixel_to_color(pf.r_mask, pf.g_mask, pf.b_mask, r_pos, g_pos, b_pos,
               mem, (byte)pf.bytes_per_pixel, 0);
         }
         base = (byte*)base + pitch;
      }
      ok = WritePNG(ck, buf, size_x, size_y, 3, size_x*3);
      delete[] buf;
   }else
   if(!stricmp(ext, "jpg")){
      ok = false;
   }else{
      ok = false;
   }

   const_cast<PIImage>(this)->Unlock();

   return ok;
}

//----------------------------
//----------------------------


