#include "pch.h"
#include <algorithm>

//----------------------------

struct S_text_vertex{
   S_vectorw pos;
   dword diffuse;
   I3D_text_coor tx;
};

//----------------------------


//----------------------------

//----------------------------
//----------------------------

                              //simple cache for last created font
static class C_font_resource{
public:
   struct S_font_info{
      int res_x, res_y;
      HFONT hfont;
   } fnt_j, fnt_e;
   
   C_font_resource(){
      memset(&fnt_j, 0, sizeof(fnt_j));
      memset(&fnt_e, 0, sizeof(fnt_e));
   }

   ~C_font_resource(){
      if(fnt_j.hfont) DeleteObject(fnt_j.hfont);
      if(fnt_e.hfont) DeleteObject(fnt_e.hfont);
   }
} font_res;

//----------------------------
                              //MS Gothic
static const char font_name[] = "MS Gothic";

static HFONT japCreateFontJ(int charx, int chary){

                              //try to reuse cache
   if(font_res.fnt_j.hfont){
      if(font_res.fnt_j.res_x==charx && font_res.fnt_j.res_y==chary)
         return font_res.fnt_j.hfont;
      bool b;
      b = DeleteObject(font_res.fnt_j.hfont);
      assert(b);
   }

   font_res.fnt_j.hfont = CreateFont(
      chary,                  //logical height of font
      charx/2,
      0,                      //angle of escapement
      0,                      //base-line orientation angle
      FW_NORMAL,              //font weight
      FALSE,                  //italic attribute flag
      FALSE,                  //underline attribute flag
      FALSE,                  //strikeout attribute flag
      SHIFTJIS_CHARSET,       //character set identifier
      OUT_DEFAULT_PRECIS,     //output precision
      CLIP_DEFAULT_PRECIS,    //clipping precision
      DEFAULT_QUALITY,        //output quality
      FIXED_PITCH | FF_DONTCARE, //pitch and family
      (const char*)font_name);//pointer to typeface name string);

   font_res.fnt_j.res_x = charx;
   font_res.fnt_j.res_y = chary;
   return font_res.fnt_j.hfont;
}

static HFONT japCreateFontE(int charx, int chary){

                              //try to reuse cache
   if(font_res.fnt_e.hfont){
      if(font_res.fnt_e.res_x==charx && font_res.fnt_e.res_y==chary)
         return font_res.fnt_e.hfont;
      bool b;
      b = DeleteObject(font_res.fnt_e.hfont);
      assert(b);
   }
   font_res.fnt_e.hfont = CreateFont(
      chary,                  //logical height of font
      charx/2,
      0,                      //angle of escapement
      0,                      //base-line orientation angle
      FW_THIN,              //font weight
      FALSE,                  //italic attribute flag
      FALSE,                  //underline attribute flag
      FALSE,                  //strikeout attribute flag
      ANSI_CHARSET,       //character set identifier
      OUT_DEFAULT_PRECIS,     //output precision
      CLIP_DEFAULT_PRECIS,    //clipping precision
      DEFAULT_QUALITY,        //output quality
      FIXED_PITCH | FF_DONTCARE, //pitch and family
      //DEFAULT_PITCH | FF_DONTCARE, //pitch and family
      (const char*)font_name);//pointer to typeface name string);

   font_res.fnt_e.res_x = charx;
   font_res.fnt_e.res_y = chary;
   return font_res.fnt_e.hfont;
}

//----------------------------

struct S_wide_line{
   wchar_t *wp;
   float x;
   int len;
   float width;
   int base_indx;
};

//----------------------------

class C_wide_text{
   dword ref;

                              //texture keeping the text
   C_smart_ptr<I3D_texture> tp;
   float size[2];             //in normalized screen coords
   float use_area[2];         //ratio of used texture area
                              //screen position (reference point)
   S_vector2 pos;
                              //screen position (upper left corner)
   float real_x, real_y;
   dword color;

   C_smart_ptr<I3D_visual_dynamic> dyn_vis;
public:
   C_wide_text():
      ref(1),
      pos(0, 0),
      real_x(0),
      real_y(0),
      color(0xffffffff)
   {
   }

   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

   virtual void Render() const{
      if(!tp)
         return;
    
      /*
      if(!dyn_vis){
         dyn_vis = (PI3D_visual_dynamic)driver->CreateFrame(FRAME_VISUAL, I3D_VISUAL_DYNAMIC);
         dyn_vis->Release();

         I3D_face_group fg;
         PI3D_material mat = driver->CreateMaterial();
         fg.base_index = 0;
         fg.num_faces = 2;
         mat->SetTexture(rep1->tp);
         fg.SetMaterial(mat);
         mat->Release();

         I3D_triface fc[2];
         fc[0][0] = 2;
         fc[0][1] = 0;
         fc[0][2] = 1;
         fc[1][0] = 2;
         fc[1][1] = 1;
         fc[1][2] = 3;

         struct S_vertex{
            S_vectorw v;
            dword diffuse;
            //dword specular;      //padding to 32 bytes
            I3D_text_coor tex;
         } v[4];

         float sx = igraph->Scrn_sx();
         float sy = igraph->Scrn_sy();

         float size_x = floor(rep1->size[0] * sx) - .1f;
         float size_y = floor(rep1->size[1] * sx) - .1f;
                                    //copy verts, also perform simple clipping
         for(int i=0; i<4; i++){
            v[i].tex.u = 0.0f;
            v[i].tex.v = 0.0f;
            v[i].v.x = rep1->real_x * sx;
            v[i].v.y = rep1->real_y * sx;
            v[i].v.z = 1.0f;
            v[i].v.w = 1.0f;
            v[i].diffuse = rep1->color;
            if(i&1){
               //v[i].v.x += rep1->used_size[0];
               v[i].v.x += size_x;
               v[i].tex.u = rep1->use_area[0];
            }else{
               if(v[i].v.x >= sx) return;
            }
            if(i&2){
               //v[i].v.y += rep1->used_size[1];
               v[i].v.y += size_y;
               v[i].tex.v = rep1->use_area[1];
            }else{
               if(v[i].v.y >= sy) return;
            }
         }

         rep1->dyn_vis->Build(v, 4, I3DVC_XYZRHW | I3DVC_DIFFUSE | (1<<I3DVC_TEXCOUNT_SHIFT),
            fc, 2, &fg, 1);
      }

      bool filter = driver->GetState(RS_LINEARFILTER);
      if(filter) driver->SetState(RS_LINEARFILTER, false);

      rep1->dyn_vis->Render();

      if(filter) driver->SetState(RS_LINEARFILTER, true);
      */
   }

   virtual void SetPos(const S_vector2 &pos){
   }

   virtual const S_vector2 &GetPos() const{ return pos; }

   virtual void SetColor(dword){
   }

   virtual void GetExtent(float &l, float &t, float &r, float &b) const{
   }
//----------------------------
// Construct wide text. Input parameters are: color, screen-relative position and size,
// justify and optional formatting rectangle.
// Output values are (optional): real position (after clipping to screen),
// number of lines of text.
   C_wide_text(const wchar_t *wcp, dword color, float x, float y, float size,
      dword justify, class I3D_driver*, float fmt_rect[4] = NULL,
      float *xret = NULL, float *yret = NULL,
      int *num_lines = NULL);

};

//----------------------------

C_wide_text::C_wide_text(const wchar_t *wcp, dword color, float x, float y, float in_size, 
   dword just, PI3D_driver drv, float fmt_rect[4],
   float *xr1, float *yr1, int *num_lines){

   PIGraph igraph = drv->GetGraphInterface();

   int scrn_sx = igraph->Scrn_sx();
   //int scrn_sy = igraph->Scrn_sy();

   in_size *= .8f;
   float scrn_width = 1.0f;

   /*
   {
      const float ratio_x = screen_ratio * .75f;
      x *= ratio_x;
      if(fmt_rect){
         fmt_rect[0] *= ratio_x;
         fmt_rect[2] *= ratio_x;
      }
      scrn_width *= ratio_x;
   }
   */
   const float ratio_x = 1.0f;

   int charx = (int)((float)scrn_sx * in_size * ratio_x);
   int chary = (int)((float)scrn_sx * in_size);
   int num_chars = wcslen(wcp);

   bool scr_clip = (just&TEXT_JUSTIFY_CLIP_TO_SCREEN);
   bool eol_char = (just&TEXT_JUSTIFY_CONVERT_EOL);
   just &= 0xffff;

   wchar_t *wcp1 = NULL;

   if(eol_char){
      int len = wcslen(wcp);
      wcp1 = new wchar_t[len + 1];

      wcscpy(wcp1, wcp);
      for(int i=0; i<len; i++)
         if(wcp1[i]=='\\') wcp1[i] = '\n';

      wcp = wcp1;
   }

   C_vector<S_wide_line> lines;

   float xr = x;
   float yr = y;

   float *char_width = NULL;

   HFONT hFontJap = NULL, hFontEng = NULL;
   
   if(num_lines) *num_lines = 0;

   HWND hwnd = (HWND)igraph->GetHWND();

   if(num_chars){

      int i;
      float x_scale = in_size;
      const wchar_t *wp = wcp;

      {
         HDC hdc = NULL;
         char_width = new float[num_chars+1];
         bool is_tt = true;
         for(i=0; i<num_chars; i++){
            wchar_t c = wp[i];
            if(c>=256) char_width[i] = x_scale;
            else{
               if(!hFontEng)
                  hFontEng = japCreateFontE(charx, chary);

               if(!hdc){
                  hdc = GetDC(hwnd);
                  SelectObject(hdc, hFontEng);
               }
               int w;
               bool st = false;
               if(is_tt){
                  ABC abc;
                  is_tt = GetCharABCWidths(hdc, wp[i], wp[i], &abc);
                  if(is_tt){
                     w = abc.abcA + abc.abcB + abc.abcC;
                     st = true;
                  }else{
                     st = GetCharWidth32(hdc, wp[i], wp[i], &w);
                  }
               }else
                  st = GetCharWidth32(hdc, wp[i], wp[i], &w);
               if(!st){
                  w = charx;
               }
               char_width[i] = (float)w / (float)scrn_sx;
            }
         }
         ReleaseDC(hwnd, hdc);
      }

      float x_clip_delta = 0.0f;

      int base_indx = 0;
      for(;;){
         if(num_lines) ++(*num_lines);
                              //get width of line
         const wchar_t *eol = wcschr(wp, '\n');
         bool end = bool(!eol);
         if(end) eol = wp + wcslen(wp);
         int line_len = eol - wp;

         float line_width = 0;
         for(i=0; i<line_len; i++)
            line_width += char_width[base_indx+i];

         float x_base = x;
                              //format text to fit in rectanlge (optional)
         if(fmt_rect){
            x_base = Max(x_base, fmt_rect[0]);
            if(line_width > (fmt_rect[2]-fmt_rect[0])){
                              //remove some words from end
               int last_beg = line_len;
               for(;;){
                  i = last_beg - 1;
                  line_width -= char_width[base_indx+i];
                  if(line_width <= (fmt_rect[2]-fmt_rect[0])){
                              //check if we may break the line here
                     if(wp[i-1]>=256 || wp[i]>=256 || wp[i]==' ')
                        break;
                  }
                  last_beg = i;
               };
               assert(i>0);
               line_len = i;
            }
         }

         switch(just){
         case TEXT_JUSTIFY_CENTER:
            x_base -= line_width/2;
            xr -= line_width/2;
            break;
         case TEXT_JUSTIFY_RIGHT:
            x_base -= line_width;
            xr -= line_width;
            break;
         }
         if(scr_clip){
            float new_x_base = x_base;
                              //clip onto screen
            if((x_base+line_width) > scrn_width-.02f) new_x_base = scrn_width - .02f - line_width;
            new_x_base = Max(0.0f, new_x_base);
            if(I3DFabs(x_clip_delta) < I3DFabs(new_x_base-x_base))
               x_clip_delta = new_x_base - x_base;
            x_base = new_x_base;
         }
                              //copy to line
         lines.push_back(S_wide_line());
         lines.back().wp = new wchar_t[line_len+1];
         wcsncpy(lines.back().wp, wp, line_len);
         lines.back().wp[line_len] = 0;
         lines.back().len = line_len;
         lines.back().x = x_base;
         lines.back().width = line_width;
         lines.back().base_indx = base_indx;

         wp += line_len;
         if(!(*wp)) break;
         base_indx += line_len;
                           //skip eol
         if(*wp=='\n'){
            ++base_indx;
            ++wp;
         }

         y += in_size;
      }
      xr += x_clip_delta;
   }
   delete[] wcp1;

   if(num_chars){
      PI3D_texture tpp = NULL;

      float min_x = 1000;
      float max_x = -1000;
                              //determine boundaries
      for(int i=0; i<(int)lines.size(); i++){
         min_x = Min(min_x, lines[i].x);
         max_x = Max(max_x, lines[i].x + lines[i].width);
      }

      float rel_size_x = (max_x - min_x);
      float rel_size_y = lines.size() * in_size;

      int sx = (int)(rel_size_x * scrn_sx);
      int sy = (int)(rel_size_y * scrn_sx);

      int used_sx = sx;
      int used_sy = sy;

      if(sx && sy){
         int size1[2];
         size1[0] = sx;
         size1[1] = sy;
         for(i=0; i<16; i++) if((1<<i) >= sx){ sx = (1<<i); break; }
         for(i=0; i<16; i++) if((1<<i) >= sy){ sy = (1<<i); break; }

         HDC hdc = CreateCompatibleDC(NULL);
         assert(hdc);

         void *bitmap_bits = NULL;

         BITMAPINFO bmi;
         memset(&bmi, 0, sizeof(bmi));
         bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
         bmi.bmiHeader.biWidth = sx;
         bmi.bmiHeader.biHeight = sy;
         bmi.bmiHeader.biPlanes = 1;
         bmi.bmiHeader.biBitCount = 24;
         bmi.bmiHeader.biCompression = BI_RGB;
         bmi.bmiHeader.biSizeImage = 0;

         HBITMAP h_bm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bitmap_bits, NULL, 0);
         assert(h_bm);

         i = SetMapMode(hdc, MM_TEXT);
         assert(i);
         SelectObject(hdc, h_bm);

         i = SetTextAlign(hdc, TA_TOP | TA_LEFT);
         assert(i!=GDI_ERROR);

         {
            dword fill_color = 0;
   #ifdef DEBUG_MODE
            fill_color = 0x00101010;
   #endif
            HBRUSH hb = CreateSolidBrush(fill_color);
            assert(hb);
            RECT rc = {0, 0, size1[0], size1[1]};
            i = FillRect(hdc, &rc, hb);
            assert(i);
            i = DeleteObject(hb);
            assert(i);
         }

   #ifndef DEBUG_MODE
         i = SetBkMode(hdc, TRANSPARENT);
         assert(i);
   #else
         i = SetBkMode(hdc, OPAQUE);
         assert(i);
         i = SetBkColor(hdc, 0x00000080);
         assert(i!=CLR_INVALID);
   #endif

         SetTextCharacterExtra(hdc, 0);
         int shadow_dist = Max(1, scrn_sx*3 / 1024);

         HFONT last_font = NULL;

         float py = 0;
         for(i=0; i<(int)lines.size(); i++){
            float px = lines[i].x-min_x;
            for(int j=0; j<lines[i].len; j++){
               wchar_t c = lines[i].wp[j];

               HFONT hf;
               if(c<256){
                  if(!hFontEng)
                     hFontEng = japCreateFontE(charx, chary);
                  hf = hFontEng;
               }else{
                  if(!hFontJap) 
                     hFontJap = japCreateFontJ(charx, chary);
                  hf = hFontJap;
               }
               if(last_font!=hf) SelectObject(hdc, last_font = hf);
                                 //draw shadow
               SetTextColor(hdc, 0x080808);
               TextOutW(hdc, (int)(px*scrn_sx) + shadow_dist, (int)(py*scrn_sx) + shadow_dist, &c, 1);

                                 //draw text
               SetTextColor(hdc, 0xffffff);
   #ifdef DEBUG_MODE
               SetBkMode(hdc, TRANSPARENT);
   #endif
               TextOutW(hdc, int(px * scrn_sx), int(py * scrn_sx), &c, 1);
   #ifdef DEBUG_MODE
               SetBkMode(hdc, OPAQUE);
   #endif
               px += char_width[lines[i].base_indx+j];
            }
            py += in_size;
         }
                                 //create texture from given bitmap
         {
            S_pixelformat pf;
            dword find_flags = FINDTF_ALPHA1;
#ifndef GL
   //#ifndef DEBUG_MODE
            find_flags |= FINDTF_COMPRESSED;
   //#endif
#endif
            I3D_RESULT ir = drv->FindTextureFormat(pf, find_flags);
            if(I3D_SUCCESS(ir)){
               assert(0);
               /*
                                 //create texture of desired resolution and pixelformat
               I3D_CREATETEXTURE ct;
               memset(&ct, 0, sizeof(ct));
               //ct.flags = TEXTMAP_TRANSP | TEXTMAP_NOMIPMAP | TEXTMAP_USEPIXELFORMAT | TEXTMAP_DIFFUSE | TEXTMAP_MEM_PTR;
               ct.pf = &pf;
               bmi.bmiHeader.biCompression = (long)bitmap_bits;
               ct.text_mem = &bmi;
               ct.size_x = sx;
               ct.size_y = sy;

               ir = drv->CreateTexture(&ct, &tpp);
               assert(I3D_SUCCESS(ir));
               */
            }
         }
         i = DeleteObject(h_bm);
         assert(i);
         i = DeleteDC(hdc);
         assert(i);
      }
      real_x = min_x;
      real_y = yr - in_size*.81f;

      use_area[0] = (float)used_sx / (float)sx;
      use_area[1] = (float)used_sy / (float)sy;
      size[0] = rel_size_x;
      size[1] = rel_size_y;

      tp = tpp;
      tpp->Release();
   }else{
      real_x = 0.0f;
      real_y = 0.0f;
      memset(size, 0, sizeof(size));
      memset(use_area, 0, sizeof(use_area));
   }
   pos.x = x;
   pos.y = y;

   delete[] char_width;
   for(int i=lines.size(); i--; )
      delete lines[i].wp;
   lines.clear();

   if(xr1) *xr1 = xr;
   if(yr1) *yr1 = yr;

   SetColor(color);
}

//----------------------------
//----------------------------

const dword SHADOW_COLOR = 0x60000000;
const dword ALPHA_MASK = 0xff000000;
const float SHADOW_ALPHA_RATIO = float((SHADOW_COLOR & ALPHA_MASK) >> 24) / 0xff;

static dword ComputeShadowClr(dword text_color){

   dword shadow_alpha = FloatToInt(((text_color & ALPHA_MASK) >> 24) * SHADOW_ALPHA_RATIO);
   dword new_shadow_clr = (shadow_alpha << 24) | (SHADOW_COLOR & ~ALPHA_MASK);
   return new_shadow_clr;
}

class C_text_imp: public C_text{

   dword ref;

   //C_poly_text *texts;
   C_smart_ptr<C_poly_text> texts;

   C_smart_ptr<C_sprite_group> poly_text;
   C_smart_ptr<C_wide_text> wide_text;

   dword color;
   bool on;
   int z;

   friend class C_poly_text_imp;
   /*
   void C_text::CreateVisual(){

      if(dyn_vis)
         dyn_vis->Release();
      PI3D_driver drv = texts->GetDriver();
      dyn_vis = (PI3D_visual_dynamic)drv->CreateFrame(FRAME_VISUAL, I3D_VISUAL_DYNAMIC);
      dyn_vis->Release();

      C_vector<S_text_vertex> v;
      C_vector<I3D_triface> f;
      I3D_face_group fg;
      PI3D_material mat = drv->CreateMaterial();

      if(poly_text){
         int nums = poly_text->NumSprites();
         const PC_sprite *sprs = poly_text->GetSprites();
         if(nums){
            v.reserve(nums*4);
            f.reserve(nums*2);
            mat->SetTexture(sprs[0]->GetSpriteImage()->GetTexture());
         }

         for(int i=nums; i--; ){
                                 //construct 2 faces of single letter
            I3D_triface fc;
            fc[0] = v.size() + 2;
            fc[1] = fc[0] - 2;
            fc[2] = fc[0] - 1;
            f.push_back(fc);
            fc[1] = fc[0] - 1;
            fc[2] = fc[0] + 1;
            f.push_back(fc);
                                 //contruct vertices
            const C_sprite *spr = sprs[i];
            for(int j=0; j<4; j++){
               const S_text_vertex &sv = spr->tlv[j];
               S_text_vertex vx;
               vx.pos = sv.v;
               vx.rhw = sv.rhw;
               vx.diffuse = color;
               vx.tx = sv.tx;
               v.push_back(vx);
            }
         }
      }else{
         assert(0);
      }

      fg.base_index = 0;
      fg.num_faces = f.size();

      fg.SetMaterial(mat);
      mat->Release();

      dyn_vis->Build(v.begin(), v.size(), I3DVC_XYZRHW | I3DVC_DIFFUSE | (1<<I3DVC_TEXCOUNT_SHIFT),
         f.begin(), f.size(), &fg, 1);
   }
   */
public:
   C_text_imp(C_poly_text *t, PC_sprite_group pg):
      ref(1),
      color(0xffffffff),
      on(true),
      z(0),
      texts(t),
      poly_text(pg)
   {}

   ~C_text_imp(){
      texts->RemoveText(this);
   }

   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

//----------------------------

   virtual void GetExtent(float &l, float &t, float &r, float &b) const{

      l = 1.0f;
      t = 1.0f;
      r = 0.0f;
      b = 0.0f;

      if(poly_text){
         const PC_sprite *sprs = poly_text->GetSprites();
         for(int i=poly_text->NumSprites(); i--; ){
            S_vector2 pos = sprs[i]->GetPos();
            const S_vector2 &size = sprs[i]->GetSize();
            const S_vector2 &hotspot = sprs[i]->GetHotSpot();
            pos -= hotspot;
            l = Min(l, pos.x);
            t = Min(t, pos.y);
            r = Max(r, pos.x + size.x);
            b = Max(b, pos.y + size.y);
         }
      }else
      if(wide_text){
         wide_text->GetExtent(l, t, r, b);
      }else{
         assert(0);
      }
   }

//----------------------------

   virtual void SetColor(dword c){

      color = c;
      dword new_shadow_clr = ComputeShadowClr(c);

      if(poly_text){
         poly_text->SetColor(c, z);

         poly_text->SetColor(new_shadow_clr, z-1);
      }else{
         wide_text->SetColor(c);
      }
   }

//----------------------------

   virtual dword GetColor() const{ return color; }

//----------------------------

   virtual void SetPos(const S_vector2 &pos){

      if(poly_text){
         S_vector2 d = GetPos();

         d = pos - d;

         const PC_sprite *sprs = poly_text->GetSprites();
         for(int i=0; i<(int)poly_text->NumSprites(); i++){
            const S_vector2 &pos = sprs[i]->GetPos();
            sprs[i]->SetPos(pos + d);
         }
         poly_text->SetPos(pos);

      }else{
         wide_text->SetPos(pos);
      }
   }

//----------------------------

   /*virtual void SetSize(float size1){

      if(poly_text)
         poly_text->SetSize(size1);
      else{
         //not implemented
         //wide_text->SetSize(size1);
      }
   }*/

//----------------------------

   virtual const S_vector2 &GetPos() const{

      if(poly_text){
         return poly_text->GetPos();
      }
      return wide_text->GetPos();
   }

//----------------------------

   virtual void Render(PI3D_scene scene) const{
      if(!on)
         return;
      if(poly_text){
         poly_text->Render(scene);
         //dyn_vis->Render();
      }
      else
         wide_text->Render();
   }

   virtual void SetOn(bool b){
      if(poly_text) poly_text->SetOn(b);
      on = b;
   }
};

//----------------------------
//----------------------------

                              //national support
/*--------------------------------------------------------
   punktation marks:
   0x80  -  a' (?)   a`'
   0x81  -  cv       c`v
   0x82  -  d'       d`|
   0x83  -  a: (?)   a`:
   0x84  -  e^ (?)   e`^
   0x85  -  ao (?)   a`o
   0x86  -  c, (?)   c`,
   0x87  -  c. (?)   c`.
   0x88  -  a` (?)   a``
   0x89  -  n~ (?)   n`~
--------------------------------------------------------*/
#define DLZEN     0x80
#define MAKCEN    0x81
#define MAKCEN1   0x82
#define DVOJBODKA 0x83
#define BOKAN     0x84
#define KRUZOK    0x85
#define PL1       0x86
#define PL2       0x87
#define DLZEN_L   0x88
#define VLNOVKA   0x89
#define GERMAN_S  's'

struct S_national_table{
   byte orig;
   char base;
   byte mark;
};
static const S_national_table ntab_1[] = {
   0x8a, 'S', MAKCEN,
   0x8c, 'S', DLZEN,
   0x8d, 'T', MAKCEN,
   0x8e, 'Z', MAKCEN,

   0x92, '\'', 0,
   0x9a, 's', MAKCEN,
   0x9c, 's', DLZEN,
   0x9d, 't', MAKCEN1,
   0x9e, 'z', MAKCEN,

   0xbc, 'L', MAKCEN,
   0xbe, 'l', MAKCEN1,

   0xc0, 'R', DLZEN,
   0xc1, 'A', DLZEN,
   0xc4, 'A', DVOJBODKA,
   0xc5, 'L', DLZEN,
   0xc6, 'C', DLZEN,
   0xc8, 'C', MAKCEN,
   0xc9, 'E', DLZEN,
   0xcb, 'E', DVOJBODKA,
   0xcc, 'E', MAKCEN,
   0xcd, 'I', DLZEN,
   0xcf, 'D', MAKCEN1,

   0xd1, 'N', DLZEN,
   0xd2, 'N', MAKCEN,
   0xd3, 'O', DLZEN,
   0xd6, 'O', DVOJBODKA,
   0xd8, 'R', MAKCEN,
   0xd9, 'U', KRUZOK,
   0xda, 'U', DLZEN,
   0xdc, 'U', DVOJBODKA,
   0xdd, 'Y', DLZEN,
   0xdf, 's', GERMAN_S,       //german s converted to ss

   //0xe0, 'r', DLZEN,
   0xe0, 'a', DLZEN_L,
   0xe1, 'a', DLZEN,
   0xe2, 'a', BOKAN,
   0xe4, 'a', DVOJBODKA,
   0xe5, 'l', DLZEN,
   0xe6, 'c', DLZEN,
   0xe7, 'c', PL1,
   //0xe8, 'c', MAKCEN,
   0xe8, 'e', DLZEN_L,
   0xe9, 'e', DLZEN,
   0xea, 'e', BOKAN,
   0xeb, 'e', DVOJBODKA,
   //0xec, 'e', MAKCEN,
   0xec, 127, DLZEN_L,
   0xed, 127, DLZEN,
   0xee, 127, BOKAN,
   0xef, 'd', MAKCEN1,

   0xf1, 'n', VLNOVKA,
   //0xf2, 'n', MAKCEN,
   0xf2, 'o', DLZEN_L,
   0xf3, 'o', DLZEN,
   0xf4, 'o', BOKAN,
   0xf6, 'o', DVOJBODKA,
   0xf8, 'r', MAKCEN,
   //0xf9, 'u', KRUZOK,
   0xf9, 'u', DLZEN_L,
   0xfa, 'u', DLZEN,
   0xfb, 'u', BOKAN,
   0xfc, 'u', DVOJBODKA,
   0xfd, 'y', DLZEN,
   0
};
static const S_national_table ntab_2[] = {               //Czech language - special conversion table
   0x8a, 'S', MAKCEN,
   0x8c, 'S', DLZEN,
   0x8d, 'T', MAKCEN,
   0x8e, 'Z', MAKCEN,

   0x92, '\'', 0,
   0x9a, 's', MAKCEN,
   0x9c, 's', DLZEN,
   0x9d, 't', MAKCEN1,
   0x9e, 'z', MAKCEN,

   0xbc, 'L', MAKCEN,
   0xbe, 'l', MAKCEN1,

   0xc0, 'R', DLZEN,
   0xc1, 'A', DLZEN,
   0xc4, 'A', DVOJBODKA,
   0xc5, 'L', DLZEN,
   0xc6, 'C', DLZEN,
   0xc8, 'C', MAKCEN,
   0xc9, 'E', DLZEN,
   0xcb, 'E', DVOJBODKA,
   0xcc, 'E', MAKCEN,
   0xcd, 'I', DLZEN,
   0xcf, 'D', MAKCEN1,

   0xd1, 'N', DLZEN,
   0xd2, 'N', MAKCEN,
   0xd3, 'O', DLZEN,
   0xd6, 'O', DVOJBODKA,
   0xd8, 'R', MAKCEN,
   0xd9, 'U', KRUZOK,
   0xda, 'U', DLZEN,
   0xdc, 'U', DVOJBODKA,
   0xdd, 'Y', DLZEN,
   0xdf, 's', GERMAN_S,       //german s converted to ss

   0xe0, 'r', DLZEN,
   //0xe0, 'a', DLZEN_L,
   0xe1, 'a', DLZEN,
   0xe2, 'a', BOKAN,
   0xe4, 'a', DVOJBODKA,
   0xe5, 'l', DLZEN,
   0xe6, 'c', DLZEN,
   0xe7, 'c', PL1,
   0xe8, 'c', MAKCEN,
   //0xe8, 'e', DLZEN_L,
   0xe9, 'e', DLZEN,
   0xea, 'e', BOKAN,
   0xeb, 'e', DVOJBODKA,
   0xec, 'e', MAKCEN,
   //0xec, 127, DLZEN_L,
   0xed, 127, DLZEN,
   0xee, 127, BOKAN,
   0xef, 'd', MAKCEN1,

   0xf1, 'n', VLNOVKA,
   0xf2, 'n', MAKCEN,
   //0xf2, 'o', DLZEN_L,
   0xf3, 'o', DLZEN,
   0xf4, 'o', BOKAN,
   0xf6, 'o', DVOJBODKA,
   0xf8, 'r', MAKCEN,
   0xf9, 'u', KRUZOK,
   //0xf9, 'u', DLZEN_L,
   0xfa, 'u', DLZEN,
   0xfb, 'u', BOKAN,
   0xfc, 'u', DVOJBODKA,
   0xfd, 'y', DLZEN,
   0
};

//----------------------------
// Converts specially formatted string into native formats. It includes
// national characters adds punktations, '\' into EOL and
// special national encoding.
static dword FormatString(const char *in_str, char *out_str, dword len, bool convert_eol,
   const S_national_table *ntab){

   int i;
   dword out_len = 0;

   for(i=0; i<(int)len; i++){
      byte c = in_str[i];
      if(c>=128){
                              //try to find in national table
         for(int ii = 0; ntab[ii].orig; ii++){
            if(ntab[ii].orig==c)
               break;
         }
         if(ntab[ii].orig){
            out_str[out_len++] = ntab[ii].base;
            if(ntab[ii].mark)
               out_str[out_len++] = ntab[ii].mark;
         }
      }else
      if(c=='\\' && convert_eol){
         out_str[out_len++] = '\n';
      }else
      if(c=='`'){
         if(out_str[out_len-1]=='i')
            out_str[out_len-1] = 127;
         char punkt_code = in_str[++i];
         switch(punkt_code){
         case '\'': out_str[out_len++] = (byte)0x80; break;
         case 'v': out_str[out_len++] = (byte)0x81; break;
         case '|': out_str[out_len++] = (byte)0x82; break;
         case ':': out_str[out_len++] = (byte)0x83; break;
         case '^': out_str[out_len++] = (byte)0x84; break;
         case 'o': out_str[out_len++] = (byte)0x85; break;
         case ',': out_str[out_len++] = (byte)0x86; break;
         case '.': out_str[out_len++] = (byte)0x87; break;
         case '`': out_str[out_len++] = (byte)0x88; break;
         case '~': out_str[out_len++] = (byte)0x89; break;
         }
      }else
         out_str[out_len++] = c;
   }
   out_str[out_len] = 0;

   return out_len;
}

//----------------------------
                              //Class to keep all visible 2D texts so that they may be
                              // rendered together.
class C_poly_text_imp: public C_poly_text{
   dword ref;

   PI3D_driver driver;
   mutable bool resort;

                              //loaded font data:
   struct S_font_size{
      byte l, r;
   };
   C_vector<S_font_size> font_sizes;
   C_vector<int> font_size_offsets;
   C_smart_ptr_const<C_sprite_image> font_img;

#ifdef _DEBUG
   C_str text;
#endif

//----------------------------
// Fit two letters together - compute smallest distance for which letters don't overlap.
   short LetterFit(byte l, byte r) const{

      l -= ' ', r -= ' ';
      if(l>102 || r>102)
         return 0;

      const S_sprite_rectangle *font_rects = font_img->GetRects();
      return font_rects[l].isx;
      /*
      int xdist = 0;

      const S_font_size *lfs = &font_sizes[font_size_offsets[l]],
         *rfs = &font_sizes[font_size_offsets[r]];
      int lcy = font_rects[l].icy, rcy = font_rects[r].icy;
      int lsy = font_rects[l].isy, rsy = font_rects[r].isy;
      int lsy1 = lsy;
      const S_font_size *lfs1 = lfs;
                                 //adjust top
      if(lcy>rcy)
         lfs += (lcy-rcy), lsy += (rcy-lcy);
      else
         rfs += (rcy-lcy), rsy += (lcy-rcy);
                                 //get compare height
      int sy = Min(lsy, rsy);
      if(sy<=0){
                                 //no overlapping, use hardspace
         while(lsy1-- > 0){
                                 //compare line
            xdist = Max(xdist, (int)lfs1->r);
            ++lfs1;
         }
      }else{
         int average = 0;
         int max = 0;
         for(int i=sy; i--; ++lfs, ++rfs){
                                 //compare line
            int distance = lfs->r - rfs->l;
            average += distance;
            max = Max(max, Max(distance, 0));
         }
         average /= sy;
         const float smooth_ratio = 0.4f;
         //xdist = (int)(max - (max - average) * smooth_ratio);
         xdist = FloatToInt(max - (max - average) * smooth_ratio);
      }
      return (short)xdist;
      */
   }

//----------------------------
// For given string, build spacing table - distances of letters.
   void BuildSpaceTable(const char *text, short *tab, int tablen){

      int i;
      short x = 0;
      char last = ' ';
      bool first = true;
      const S_sprite_rectangle *font_rects = font_img->GetRects();

      int space_width = font_rects[0].isx / 2;
      short space_height = (short)font_rects[0].isy;

      for(i=0; ; i++){
         byte c = text[i];       //get one character
         if(!c) break;           //end of text
         short *tp = &tab[i*2];

         if(c<' '){
            if(text[i]=='\n'){
               tp[0] = (short)(x + font_rects[last-' '].isx);
               x = 0;
            }else
               tp[0] = 0;
            tp[1] = 0;
            first = true;
         }else
         if(c>=128 && c<0x90){
            switch(c){
            case 0x80:
            case 0x81:
            case 0x83:
            case 0x84:
            case 0x85:
            case 0x88:
            case 0x89:
                                 //put mark onto center of letter
               tp[0] = (short)(x + 1 + font_rects[last-' '].isx / 2);
               tp[1] = (short)(font_rects[last-' '].icy + 1);
               break;
            case 0x86:
            case 0x87:
                                 //put mark below center of letter
               tp[0] = (short)(x + 1 + font_rects[last-' '].isx / 2);
               tp[1] = -1;
               break;
            case 0x82:
               //tp[0] = x + 3 + font_rects[last-' '].isx;
               tp[0] = (short)(x + font_rects[last-' '].isx - 2);
               tp[1] = (short)(font_rects[last-' '].icy);
               x += 3;
               break;
            }
         }else{
            if(c==' '){
               //x += 4 * space_width;
               x += space_height / 4;
            }else{
               if(first)
                  first = false;
               else
                  x += LetterFit(last, c) + space_width;
               last = c;
            }
            tp[0] = x;
            tp[1] = 0;
         }
      }
      tab[i*2] = (short)(x + font_rects[last-' '].isx);
      tab[i*2+1] = 0;
   }

//----------------------------
// All texts to be rendered.
   mutable C_vector<C_text_imp*> texts;
   static bool TextLess(const C_smart_ptr<C_text_imp> &t1, const C_smart_ptr<C_text_imp> &t2){
      return (t1->z < t2->z);
   }

//----------------------------
// Internal - make text italic-look.
   void SetItalic(PC_text, float angle);
public:
   C_poly_text_imp():
      ref(1),
      driver(NULL),
      resort(true)
   {
   }
   ~C_poly_text_imp(){
      font_img = NULL;
      for(int i=0; i<(int)texts.size(); i++)
         delete texts[i];
   }

//----------------------------
// Call with param diff_name relative to dir "Maps\\".
// 'font_image' is name of image to be used for building font spacing table.
   E_SPRITE_IMAGE_INIT_RESULT Init(PI3D_driver driver, const char *prjname, const char *bitmap_name, const char *opt_name, dword txt_optn_flags);

   E_SPRITE_IMAGE_INIT_RESULT Init(CPC_poly_text src1){

      if(!src1)
         return SPRINIT_CANT_OPEN_PROJECT;
      const C_poly_text_imp *src = (const C_poly_text_imp*)src1;
      driver = src->driver;
      font_img = src->font_img;
      font_sizes = src->font_sizes;
      font_size_offsets = src->font_size_offsets;
      return SPRINIT_OK;
   }

//----------------------------

   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

   virtual CPC_sprite_image GetSpriteImage() const{ return font_img; }

   virtual PC_text CreateText(S_text_create &ct);

//----------------------------

   virtual bool RemoveText(PC_text tp){

      for(int i=texts.size(); i--; ){
         if(texts[i]==tp){
            texts[i] = texts.back(); texts.pop_back();
            resort = true;
            return true;
         }
      }
      assert(0);              //text not found???
      return false;
   }

//----------------------------

   virtual void Render(PI3D_scene scene) const{
      if(resort){
         sort(texts.begin(), texts.end(), TextLess);
         resort = false;
      }
      bool use_zb = driver->GetState(RS_USEZB);
      if(use_zb) driver->SetState(RS_USEZB, false);

      for(int i=0; i<(int)texts.size(); i++){
         PC_text tp = texts[i];
         tp->Render(scene);
      }
      if(use_zb) driver->SetState(RS_USEZB, true);
   }

//----------------------------

   virtual I3D_driver *GetDriver() const{ return driver; }
};

//----------------------------

E_SPRITE_IMAGE_INIT_RESULT C_poly_text_imp::Init(PI3D_driver drv, const char *prjname, const char *bitmap_name, const char *opt_name, dword txt_create_flags){

   driver = drv;
   C_sprite_image *img = CreateSpriteImage();

                              //find extension
   bool use_new_format = false;
   if(!(txt_create_flags&TEXTMAP_USE_CACHE)){
      for(int ei = strlen(prjname); ei--; ){
         if(prjname[ei]=='.')
            break;
      }
      if(ei != -1){
                                 //one for '.'
         if(strcmp(prjname+ei+1, "txt") == 0)
            use_new_format = true;
      }
   }else{
      dword id;
      C_cache *ck = (C_cache*)prjname;
      ck->read(&id, sizeof(id));
      ck->seekg(ck->tellg()-sizeof(id));
      use_new_format = (id != 'JRPS');
   }

   E_SPRITE_IMAGE_INIT_RESULT ir = img->Init(driver, prjname, bitmap_name, opt_name, txt_create_flags, use_new_format);

   if(ir==SPRINIT_OK){
      font_img = img;
      C_cache *ck, tmp_cache;

      if(txt_create_flags&TEXTMAP_USE_CACHE)
         ck = (C_cache*)prjname;
      else{
         if(!tmp_cache.open(prjname, CACHE_READ))
            return SPRINIT_CANT_OPEN_PROJECT;
         ck = &tmp_cache;
      }
      //cache.open(prjname, CACHE_READ);

      /*struct{
         dword id;
         word img_num;
         word spr_num;
         dword res[2];
         char img_name[16];
      } header;
      ck->read(&header, sizeof(header));*/
      //cache.close();

      PIImage font_img1 = driver->GetGraphInterface()->CreateImage();
      S_pixelformat pf;
      memset(&pf, 0, sizeof(pf));
      pf.bytes_per_pixel = 1;
      pf.flags = PIXELFORMAT_PALETTE;
      if(!bitmap_name)
         bitmap_name = opt_name;

      bool ok;
      if(txt_create_flags&TEXTMAP_USE_CACHE){
         ok = font_img1->Open(*(C_cache*)bitmap_name, IMGOPEN_SYSMEM | TEXTMAP_USE_CACHE, 0, 0, &pf);
      }else{
         ok = false;
         int num_d = drv->NumDirs(I3DDIR_MAPS);
         for(int i=0; i<num_d; i++){
            const C_str &dir = drv->GetDir(I3DDIR_MAPS, i);
            ok = font_img1->Open(C_fstr("%s\\%s", (const char*)dir, bitmap_name), IMGOPEN_SYSMEM, 0, 0, &pf);
            if(ok)
               break;
         }
      }
      if(!ok){

         font_img = NULL;
         font_img1->Release();
         return SPRINIT_CANT_OPEN_FONTIMG;
      }

      dword spr_num = font_img->NumRects();
                              //build font spacing table
                              //get access to bitmap data
      byte *font_data;
      dword pitch;
      font_img1->Lock((void**)&font_data, &pitch);

      font_size_offsets.clear();
      //font_size_offsets.reserve(header.spr_num);
      font_size_offsets.reserve(spr_num);
      font_sizes.clear();
                              //build all letters
      //for(int i = 0; i<header.spr_num; i++)
      for(dword i = 0; i < spr_num; i++){
         const S_sprite_rectangle &rc = font_img->GetRects()[i];
                              //get address of letter
         const byte *mem = font_data + rc.iy * pitch + rc.ix;
                              //assign pointer
         font_size_offsets.push_back(font_sizes.size());

         dword sx = rc.isx, sy = rc.isy;
                              //process all letter's lines
         for(int j=0; j<(int)sy; j++, mem += pitch){
            font_sizes.push_back(S_font_size());
            S_font_size &fs = font_sizes.back();

                              //find left
            fs.l = 0;
            const byte *bm = mem;
            while(!*bm && fs.l<sx)
               ++bm, ++fs.l;
                              //find right
            fs.r = (byte)sx;
            bm = mem + sx;
            while(!*--bm && fs.r)
               --fs.r;
         }
      }
      font_img1->Unlock();
      font_img1->Release();
   }
   img->Release();
   return ir;
}

//----------------------------

PC_text C_poly_text_imp::CreateText(S_text_create &tc){

   if(tc.wide){
      /*
      C_wide_text *wt = new C_wide_text((const wchar_t*)tp1, color, x, y, size, justify, driver, fmt_rect,
         xr, yr, num_lines);
      C_text *ret = new C_text(wt, this);
      wt->Release();
      texts.push_back(ret);
      ret->color = color;
      ret->z = z;
      resort = true;
      return ret;
      */
      return NULL;
   }

   const char *in_str = (const char*)tc.tp;
#ifdef _DEBUG
   text = in_str;
#endif
   int i;
   int len = strlen(in_str);

   if(tc.num_lines)
      *tc.num_lines = 0;

   char *cpa = new char[len*2+1];

   //const S_national_table *ntab = (game_configuration.language==7) ? ntab_2 : ntab_1;
   const S_national_table *ntab = ntab_1;

   len = FormatString(in_str, cpa, len, (tc.justify&TEXT_JUSTIFY_CONVERT_EOL), ntab);

   PC_sprite_group pg = CreateSpriteGroup();
   pg->SetPos(S_vector2(tc.x, tc.y));

   short *spct = new short[(len+1)*2];    //x, y

   BuildSpaceTable(cpa, spct, (len+1)*2);

   const S_sprite_rectangle &rc = font_img->GetRects()[0];
   float f = rc.isy * tc.in_scale_y;
   float y_scale = tc.size / f;
   //float y_scale = tc.size / font_img->GetRects()[0].isy * tc.in_scale_y;

   if(tc.xret) *tc.xret = tc.x;
   if(tc.yret) *tc.yret = tc.y;
   float x_clip_delta = 0.0f;

   PIGraph igraph = driver->GetGraphInterface();
   float scrn_sx = (float)igraph->Scrn_sx();
   float r_pixel_size_x = 1.0f / scrn_sx;

   const char *curr_char = cpa;
   short *spct1 = spct;
   float cur_line_y = tc.y;

   while(true){
      if(tc.num_lines)
         ++(*tc.num_lines);
                              //get width of line
      const char *end_of_line = strchr(curr_char, '\n');
      bool last_line = !end_of_line;
      if(last_line){
         end_of_line = curr_char + strlen(curr_char);
      }
      int line_len = end_of_line - curr_char;

      float line_width = (spct1[line_len*2] - spct1[0]) * y_scale;
      float x_base = tc.x;
      
      switch(tc.justify&TEXT_JUSTIFY_MASK){
      case TEXT_JUSTIFY_CENTER:
         x_base -= line_width/2;
         if(tc.xret) *tc.xret -= line_width/2;
         break;
      case TEXT_JUSTIFY_RIGHT:
         x_base -= line_width;
         if(tc.xret) *tc.xret -= line_width;
         break;
      }

                              //format text to fit in rectanlge (optional)
      if(tc.fmt_rect){
         if(x_base<tc.fmt_rect[0]){
            if(tc.xret) *tc.xret -= (x_base - tc.fmt_rect[0]);
            x_base = tc.fmt_rect[0];
         }
         float fmt_width = tc.fmt_rect[2] - tc.fmt_rect[0];
         if(line_width > fmt_width){
                              //remove some words from end
            int last_beg = line_len;
            for(;;){
               for(i=last_beg; i--; ){
                  if(curr_char[i]==' ')
                     break;
               }
               if(i==-1){
                  i = last_beg;
                  break;
               }
               line_width = (spct1[i*2]-spct1[0]) * y_scale;
               if(line_width <= fmt_width)
                  break;
               last_beg = i;
            };
            last_line = (line_len==i);
            line_len = i;
         }
      }

      if(tc.justify&TEXT_JUSTIFY_CLIP_TO_SCREEN){
         float new_x_base = x_base;
                              //clip onto screen
         if((x_base+line_width) > 1.0f)
            new_x_base = 1.0f-line_width;
         new_x_base = Max(0.0f, new_x_base);
         if(I3DFabs(x_clip_delta) < I3DFabs(new_x_base-x_base))
            x_clip_delta = new_x_base-x_base;
         x_base = new_x_base;
      }
      //float scrn_y = tc.y * scrn_sx;
      float scrn_y = cur_line_y * scrn_sx;

      float spct_base = spct1[0];
      for(i=0; i<line_len; i++, ++curr_char, spct1+=2){
         int indx = ((byte)*curr_char) - ' ';
         if(indx <= 0)
            continue;
         const S_sprite_rectangle &rc = font_img->GetRects()[indx];
         float fx = x_base + (float)(spct1[0]-spct_base) * y_scale * tc.in_scale_x;
         //float fy = tc.y - ((float)spct1[1] * y_scale);
         float fy = cur_line_y - ((float)spct1[1] * y_scale);
         fx -= (float)fmod(fx, r_pixel_size_x);
         fy -= (float)fmod(fy, r_pixel_size_x);

         PC_sprite spr = CreateSprite(fx, fy, rc.isy*y_scale, font_img, indx, tc.z, tc.in_scale_x / tc.in_scale_y);

         spr->SetColor(tc.color);
         if(tc.italic!=0.0f)
            spr->Shear(tc.italic, scrn_y);

         pg->AddSprite(spr);
         spr->Release();

         if(tc.shadow)
         {
            const float SHADOW_OFFSET = .003f;
            PC_sprite spr = CreateSprite(fx+SHADOW_OFFSET, fy+SHADOW_OFFSET, rc.isy*y_scale, font_img, indx, tc.z-1, tc.in_scale_x / tc.in_scale_y);

            dword shadow_clr = ComputeShadowClr(tc.color);
            spr->SetColor(shadow_clr);
            if(tc.italic!=0.0f)
               spr->Shear(tc.italic, scrn_y);

            pg->AddSprite(spr);
            spr->Release();
         }
      }
      if(last_line)
         break;
                           //skip eol
      ++curr_char;
      spct1 += 2;

      //tc.y += tc.size;
      cur_line_y += (tc.line_dist <= .0f) ? tc.size : tc.line_dist;
   }

   if(tc.xret) *tc.xret += x_clip_delta;

   delete[] spct;
   delete[] cpa;

   C_text_imp *txt = new C_text_imp(this, pg);
   txt->color = tc.color;
   txt->z = tc.z;
   if(tc.clip_rect){
      pg->SetClipRect(tc.clip_rect);
   }
   texts.push_back(txt);

   pg->Release();
   //txt->CreateVisual();

   resort = true;

   return txt;
}

//----------------------------

E_SPRITE_IMAGE_INIT_RESULT CreatePolyText(PI3D_driver driver, PC_poly_text *out, const char *prjname,
   const char *diff_name, const char *opt_name, dword txt_create_flags){

   C_poly_text_imp *pt = new C_poly_text_imp;

   E_SPRITE_IMAGE_INIT_RESULT ir = pt->Init(driver, prjname, diff_name, opt_name, txt_create_flags);
   if(ir!=SPRINIT_OK){
      pt->Release();
      pt = NULL;
   }
   *out = pt;
   return ir;
}

//----------------------------

E_SPRITE_IMAGE_INIT_RESULT CreatePolyText(I3D_driver *driver, PC_poly_text *out, class C_cache &ck_project,
   C_cache *ck_diffuse, C_cache *ck_opacity, dword txt_create_flags){

   return CreatePolyText(driver, out, (const char*)&ck_project, (const char*)ck_diffuse,
      (const char*)ck_opacity, txt_create_flags | TEXTMAP_USE_CACHE);
}

//----------------------------

E_SPRITE_IMAGE_INIT_RESULT CreatePolyText(CPC_poly_text src, PC_poly_text *out){

   C_poly_text_imp *pt = new C_poly_text_imp;
   E_SPRITE_IMAGE_INIT_RESULT ir = pt->Init(src);
   if(ir!=SPRINIT_OK){
      pt->Release();
      pt = NULL;
   }
   *out = pt;
   return ir;
}

//----------------------------
