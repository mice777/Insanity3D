#include "pch.h"

//----------------------------


PI3D_texture CreateStringTexture(const char *cp, int size1[2], PI3D_driver drv, void *winHFONT){

   HFONT h_font = (HFONT)winHFONT;
   if(!h_font)
      return NULL;

   HWND hwnd = (HWND)drv->GetGraphInterface()->GetHWND();
   SIZE size;
   {
      HDC hdc = GetDC(hwnd);
      SelectObject(hdc, h_font);
      GetTextExtentPoint32(hdc, cp, strlen(cp), &size);
      ReleaseDC(hwnd, hdc);
   }
   size1[0] = size.cx;
   size1[1] = size.cy;
   int i;
   for(i=0; i<12; i++){
      if((1<<i) >= size.cx){
         size.cx = (1<<i);
         break;
      }
   }
   for(i=0; i<12; i++){
      if((1<<i) >= size.cy){
         size.cy = (1<<i);
         break;
      }
   }

   PI3D_texture tp = NULL;
   {
      void *bitmap_bits = NULL;

      HDC hdc = CreateCompatibleDC(NULL);

      BITMAPINFO bmi;
      memset(&bmi, 0, sizeof(bmi));
      bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
      bmi.bmiHeader.biWidth = size.cx;
      bmi.bmiHeader.biHeight = size.cy;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 24;
      bmi.bmiHeader.biCompression = BI_RGB;
      bmi.bmiHeader.biSizeImage = 0;

      HBITMAP h_bm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bitmap_bits, NULL, 0);
      SetMapMode(hdc, MM_TEXT);
      SelectObject(hdc, h_bm);

                              //paint into the bitmap
      SetTextAlign(hdc, TA_TOP | TA_LEFT);
      SelectObject(hdc, h_font);
      {
         HBRUSH hb = CreateSolidBrush(0x00101010);
         RECT rc = {0, 0, size1[0], size1[1]};
         FillRect(hdc, &rc, hb);
         DeleteObject(hb);
      }
      SetBkMode(hdc, TRANSPARENT);
      //SetBkColor(hdc, (COLORREF)0);
      //SetTextColor(hdc, 0x00101010);
      //TextOut(hdc, 1, 1, cp, strlen(cp));
      SetTextColor(hdc, 0xffffff);
      TextOut(hdc, 0, 0, cp, strlen(cp));

                              //convert to the texture
      {
         S_pixelformat pf;
         I3D_RESULT ir = drv->FindTextureFormat(pf, FINDTF_ALPHA1);
         if(I3D_SUCCESS(ir)){
            byte *mem = NULL;
            dword sz = 0;
            C_cache ck;
            ck.open(&mem, &sz, CACHE_WRITE_MEM);

#pragma pack(push,1)
            struct S_hdr{
               word sig;
               dword sz;
               dword res;
               dword data_offs;
            } hdr = { 0x4d42 };
#pragma pack(pop)
            hdr.data_offs = sizeof(hdr) + sizeof(bmi);

            ck.write((char*)&hdr, sizeof(hdr));
            ck.write((char*)&bmi, sizeof(bmi));
            ck.write((char*)bitmap_bits, size.cx*size.cy*3);
            ck.close();

            ck.open(&mem, &sz, CACHE_READ_MEM);

                              //create texture of desired resolution and pixelformat
            I3D_CREATETEXTURE ct;
            memset(&ct, 0, sizeof(ct));
            ct.flags = TEXTMAP_TRANSP |
               //TEXTMAP_NOMIPMAP |
               TEXTMAP_MIPMAP |
               TEXTMAP_USEPIXELFORMAT | TEXTMAP_DIFFUSE | TEXTMAP_USE_CACHE;
            ct.pf = &pf;
            ct.ck_diffuse = &ck;
            ct.size_x = size.cx;
            ct.size_y = size.cy;

            ir = drv->CreateTexture(&ct, &tp);
            ck.close();
            ck.FreeMem(mem);
            /*
            if(I3D_SUCCESS(ir)){
               PIImage img = tp->GetImage(false);
               img->CKeyToAlpha(img->GetColorKey());
            }
            */
         }
      }

      DeleteObject(h_bm);
      DeleteDC(hdc);
   }
   return tp;
}

//----------------------------

void DrawStringTexture(CPI3D_frame frm, float y_screen_displace, PI3D_scene scene, CPI3D_texture tp, const int size[2],
   dword color, float min_dist, float max_dist, float scale, float y_world_displace){

   PI3D_driver driver = scene->GetDriver();
                              //check if visual was rendered
                              // but only if visuals are drawn
   if(driver->GetState(RS_DRAWVISUALS)){
      CPI3D_frame vis_parent = frm;
      while(vis_parent && vis_parent->GetType()!=FRAME_VISUAL) vis_parent = vis_parent->GetParent();
      if(vis_parent){
         dword last_frm_render_time = I3DCAST_CVISUAL(vis_parent)->GetLastRenderTime();
         dword last_scn_render_time = scene->GetLastRenderTime();
                              //don't wast time on invisible visuals
         if(last_frm_render_time != last_scn_render_time)
            return;
      }
   }
   assert(tp && size);

   const S_matrix &mat = frm->GetMatrix();
                              //check dist to camera
   float dist_2 = (mat(3) - scene->GetActiveCamera()->GetWorldPos()).Square();
   if(dist_2 >= (max_dist*max_dist))
      return;

   S_vector pos = mat(3);

   scene->SetRenderMatrix(I3DGetIdentityMatrix());
   S_vectorw scrn_pos;

                              //blend at least by this amount, so that texture alpha is used
   byte alpha = color>>24;
   if(dist_2 >= (min_dist*min_dist)){
      float dist = sqrt(dist_2);
      alpha -= FloatToInt((255-alpha) * (dist-min_dist) / (max_dist-min_dist));
   }
                              //adjust position, if necessary
   const I3D_bbox *bbox = NULL;
   switch(frm->GetType()){
   case FRAME_VISUAL:
      bbox = &I3DCAST_CVISUAL(frm)->GetBoundVolume().bbox;
      break;
   case FRAME_MODEL:
      bbox = &I3DCAST_CMODEL(frm)->GetHRBoundVolume().bbox;
      break;
   case FRAME_DUMMY:
      //bbox = I3DCAST_DUMMY(frm)->GetBBox();
      pos += mat(1) * .1f;
      break;
   }
   if(bbox && bbox->IsValid())
      pos += mat(1) * bbox->max.y;
   pos += mat(1) * y_world_displace;
   scene->TransformPointsToScreen(&pos, &scrn_pos, 1);

                              //check z
   if(scrn_pos.z <= 0.0f)
      return;

   scrn_pos.y += y_screen_displace;

   struct S_vertex{
      S_vectorw v;
      I3D_text_coor tex;
   } v[4];
   int sx = driver->GetGraphInterface()->Scrn_sx();
   int sy = driver->GetGraphInterface()->Scrn_sy();

   scale *= scrn_pos.w;
   float fsize[2] = {size[0] * scale, size[1] * scale};
                              //copy verts, also perform simple clipping
   for(int i=0; i<4; i++){
      v[i].tex.x = 0.0f;
      v[i].tex.y = 0.0f;
      v[i].v = scrn_pos;
      v[i].v.x -= fsize[0]/2;
      v[i].v.y -= fsize[1];
      if(i&1){
         v[i].v.x += fsize[0];
         if(v[i].v.x <= 0) return;
         v[i].tex.x = (float)size[0] / (float)tp->SizeX();
      }else{
         if(v[i].v.x >= sx) return;
      }
      if(i&2){
         v[i].v.y += fsize[1];
         if(v[i].v.y <= 0) return;
         v[i].tex.y = (float)size[1] / (float)tp->SizeY();
      }else{
         if(v[i].v.y >= sy) return;
      }
   }
   driver->SetTexture(tp);
   static const word indx[] = {0, 1, 2, 2, 1, 3};
   scene->DrawTriangles(&v[0].v, 4, I3DVC_XYZRHW | (1<<I3DVC_TEXCOUNT_SHIFT), indx, sizeof(indx)/sizeof(word),
      (Min(254, (int)alpha)<<24) | color);
}

//----------------------------
