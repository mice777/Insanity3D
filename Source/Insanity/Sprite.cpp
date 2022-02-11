#include "pch.h"
#include <algorithm>
//#define ONE_VISUAL


//----------------------------

struct S_rect_f{
   //union{                   //MB: commented out, non-standard extension (unnamed structure)
      //struct S_rc{
         float l, t, r, b;
      //};
      //float f[4];
   //};
   S_rect_f(float l1, float t1, float r1, float b1):
      l(l1), t(t1), r(r1), b(b1)
   {}
   //inline float &operator [](int i){ return f[i]; }
   //inline const float &operator [](int i) const{ return f[i]; }
};

//----------------------------

class C_sprite_image_imp: public C_sprite_image{
   dword ref;
public:

private:
                              //texture containing sprites
   C_smart_ptr<I3D_texture> tp;

   float image_aspect_ratio;
                              //sprite rectangles
   C_vector<S_sprite_rectangle> spr_rects;
public:
   C_sprite_image_imp():
      ref(1),
      image_aspect_ratio(1.0f)
   {}
   ~C_sprite_image_imp(){
      Close();
   }
//----------------------------
   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

//----------------------------
   virtual E_SPRITE_IMAGE_INIT_RESULT Init(PI3D_driver, const char *prjname, const char *diff_name,
      const char *opt_name, dword txt_create_flags, bool use_new_prj_format, float border_offset);

   virtual E_SPRITE_IMAGE_INIT_RESULT Init(PI3D_driver drv, C_cache &ck_prj, C_cache *ck_diffuse,
      C_cache *ck_opacity, dword txt_create_flags, bool use_new_prj_format, float border_offset){

      return Init(drv, (const char*)&ck_prj, (const char*)ck_diffuse, (const char*)ck_opacity,
         txt_create_flags | TEXTMAP_USE_CACHE, use_new_prj_format, border_offset);
   }

//----------------------------
   virtual void Close(){
      tp = NULL;
      spr_rects.clear();
   }

//----------------------------
   virtual float GetAspectRatio() const{ return image_aspect_ratio; }

//----------------------------
   virtual PI3D_texture GetTexture() { return tp; }
   virtual CPI3D_texture GetTexture() const{ return tp; }

   
   virtual dword NumRects() const{ return spr_rects.size(); }
   virtual const S_sprite_rectangle *GetRects() const{ return &spr_rects.front(); }
};

//----------------------------

E_SPRITE_IMAGE_INIT_RESULT C_sprite_image_imp::Init(PI3D_driver driver, const char *prjname, const char *diff_name,
   const char *opt_name, dword txt_create_flags,
   bool use_new_prj_format, float border_offset){

   int i;
   C_cache *ck, tmp_cache;
   int ck_prj_offs = 0;
   if(txt_create_flags&TEXTMAP_USE_CACHE){
      ck = (C_cache*)prjname;
      ck_prj_offs = ck->tellg();
   }else{
      if(!tmp_cache.open(prjname, CACHE_READ))
         return SPRINIT_CANT_OPEN_PROJECT;
      ck = &tmp_cache;
   }

                              //load texture
   I3D_RESULT ir;
   I3D_CREATETEXTURE ct;
   memset(&ct, 0, sizeof(ct));

   int ck_offs = 0;

   if(diff_name){
      ct.flags = txt_create_flags | TEXTMAP_DIFFUSE;
      ct.file_name = diff_name;
      if(txt_create_flags&TEXTMAP_USE_CACHE)
         ck_offs = ((C_cache*)diff_name)->tellg();
   }
   if(opt_name){
      ct.flags |= TEXTMAP_OPACITY;
      ct.opt_name = opt_name;
      if(!diff_name && (txt_create_flags&TEXTMAP_USE_CACHE))
         ck_offs = ((C_cache*)diff_name)->tellg();
   }
                              //if mipmapping not requested, disable it
   if(!(txt_create_flags&TEXTMAP_MIPMAP))
      ct.flags |= TEXTMAP_NOMIPMAP;

   PI3D_texture tp1;
   ir = driver->CreateTexture(&ct, &tp1);
   if(I3D_FAIL(ir)){
      //cache.close();
      Close();
      switch(ir){
      case I3DERR_NOFILE: return SPRINIT_CANT_OPEN_DIFFUSE;
      case I3DERR_NOFILE1: return SPRINIT_CANT_OPEN_OPACITY;
      }
      return SPRINIT_CANT_CREATE_TEXTURE;
   }
   tp = tp1;
   tp1->Release();

   dword img_sx, img_sy; 
   byte bpp; 
   dword id;

   bool b;
   if(txt_create_flags&TEXTMAP_USE_CACHE){
      C_cache *ck = (C_cache*)(ct.file_name ? ct.file_name : ct.opt_name);
      ck->seekp(ck_offs);
      b = driver->GetGraphInterface()->GetImageInfo(*ck, img_sx, img_sy, bpp, id);
   }else{
      b = false;
      img_sx = img_sy = 0;
      int num_d = driver->NumDirs(I3DDIR_MAPS);
      for(int i=0; i<num_d; i++){
         const C_str &dir = driver->GetDir(I3DDIR_MAPS, i);
         b = driver->GetGraphInterface()->GetImageInfo(C_fstr("%s\\%s", (const char*)dir, ct.file_name ? ct.file_name : ct.opt_name), img_sx, img_sy, bpp, id);
         if(b)
            break;
      }
   }
   if(!b){
      //cache.close();
      Close();
      return SPRINIT_CANT_OPEN_DIFFUSE;
   }
   image_aspect_ratio = (float)img_sx / (float)img_sy;

                              //read rectangle definitions
   if(!use_new_prj_format){
      struct{
         dword id;
         word img_num;
         word spr_num;
         dword res[2];
         char img_name[16];
      }header;
      ck->read((char*)&header, sizeof(header));
      spr_rects.reserve(header.spr_num);
      ck->seekp(header.img_num * 16 + 16);

      struct{
         word x, y, sx, sy;
         short cx, cy;
         dword res;
      } rc;

      for(i=0; i<header.spr_num; i++){
         ck->read((char*)&rc, sizeof(rc));
         spr_rects.push_back(S_sprite_rectangle());
         S_sprite_rectangle &src = spr_rects.back();
         src.l = (float(rc.x) - border_offset) / float(img_sx);
         src.t = (float(rc.y) - border_offset)/float(img_sy);
         src.sx = (float(rc.sx) + border_offset * 2.0f)/float(img_sx);
         src.sy = (float(rc.sy) + border_offset * 2.0f)/float(img_sy);
         src.cx = (float)rc.cx/(float)rc.sx;
         src.cy = (float)rc.cy/(float)rc.sy;
         src.ix = rc.x;
         src.iy = rc.y;
         src.isx = rc.sx;
         src.isy = rc.sy;
         src.icx = rc.cx;
         src.icy = rc.cy;
      }
   }else{
      int line_num = 0;
      while(!ck->eof()){
         ++line_num;
                              //get entire line
         char line[512];
         ck->getline(line, sizeof(line));
         int line_size = strlen(line);
         int i;
                              //skip white-space
         for(i=0; i<line_size; i++){
            if(!isspace(line[i]))
               break;
         }
                              //skip empty or commented lines
         if(!line[i] || line[i]==';')
            continue;
                              //decode line
         int sn, x, y, sx, sy, cx, cy;
         int num_scanned = sscanf(line, "%i: %i, %i, %i, %i, %i, %i", &sn, &x, &y, &sx, &sy, &cx, &cy);
         if(num_scanned!=7){
            //ErrReport(C_fstr("Sprite def file '%s' (%i): syntax error.", prjname, line_num), editor);
            continue;
         }
         if(sn < (int)spr_rects.size()){
            //ErrReport(C_fstr("Sprite def file '%s' (%i): sprite %i already defined.", prjname, line_num, sn), editor);
         }
                              //scan ok, add to sprites
         while((int)spr_rects.size() < sn){
            spr_rects.push_back(S_sprite_rectangle());
            memset(&spr_rects.back(), 0, sizeof(S_sprite_rectangle));
         }
         spr_rects.push_back(S_sprite_rectangle());
         S_sprite_rectangle &src = spr_rects.back();
         src.l = (float(x) - border_offset)/float(img_sx);
         src.t = (float(y) - border_offset)/float(img_sy),
         src.sx = (float(sx) + border_offset * 2.0f)/float(img_sx),
         src.sy = (float(sy) + border_offset * 2.0f)/float(img_sy),
         src.cx = (float)cx/(float)sx,
         src.cy = (float)cy/(float)sy;
         src.ix = x;
         src.iy = y;
         src.isx = sx;
         src.isy = sy;
         src.icx = cx;
         src.icy = cy;
      }
   }
   //ck->close();
   if(txt_create_flags&TEXTMAP_USE_CACHE){
      ck->seekp(ck_prj_offs);
   }

   return SPRINIT_OK;
}

//----------------------------

C_sprite_image *CreateSpriteImage(){
   return new C_sprite_image_imp;
}

//----------------------------

class C_sprite_imp: public C_sprite{

   dword ref;

   bool on;
   S_vector2 pos;             //position in normalized coords
   S_vector2 size;            //size in normalized coords
   S_vector2 hotspot;         //hotspot offset in normalized coords
   S_vector2 screen_pos;      //position in screen coords
   S_vector2 screen_rect[4];  //rectangle in screen coords
   int z;
   dword color;

   mutable C_smart_ptr_const<C_sprite_image> img;

                              //dynamic visual for rendering the sprite, may be NULL if sprite is part of text
   mutable C_smart_ptr<I3D_visual_dynamic> dyn_vis;
   mutable bool vis_contents_dirty;

   struct S_vertex{           //param to DrawPrimitive
      S_vector v;
      float rhw;
      dword diffuse;
      I3D_text_coor tx;
   } tlv[4];

//----------------------------

   void PrepareDraw(){

      CPIGraph igraph = img->GetTexture()->GetDriver()->GetGraphInterface();
                              //deform by resolution
      float screen_aspect_ratio = 1.3333333333f*((float)igraph->Scrn_sy()/(float)igraph->Scrn_sx());
      for(int j=0; j<4; j++){
         S_vertex &v = tlv[j];
         v.v.x = (screen_pos.x + screen_rect[j].x);
         v.v.y = (screen_pos.y + screen_rect[j].y) * screen_aspect_ratio;
      }
      vis_contents_dirty = true;
   }

public:
   C_sprite_imp(float x, float y, float size1, CPC_sprite_image img1, dword indx, int z1, float aspect):
      ref(1),
      on(true),
      z(z1),
      img(img1),
      pos(1e+16f, 1e+16f),    //some crazy value, so that SetPos passes
      screen_pos(1e+16f, 1e+16f),
      vis_contents_dirty(true),
      color(0)
   {
      CPIGraph igraph = img->GetTexture()->GetDriver()->GetGraphInterface();

      assert(img);
      const S_sprite_rectangle &rc = img->GetRects()[indx<img->NumRects() ? indx : 0];
      float sx = rc.sx * size1 / rc.sy * aspect;

      float scrn_sx = (float)igraph->Scrn_sx();

      size.x = sx;
      size.y = size1;

                                 //total hack to correct scale problem
      sx *= img->GetAspectRatio();
                              //03.10.2002: I really hate these stuff with aspect ratio!
      size.x *= img->GetAspectRatio();

      //hotspot.x = rc.cx * sx;
      //hotspot.y = rc.cy * size1;
      hotspot.x = size.x * ((float)rc.icx / (float)(rc.isx-1));
      hotspot.y = size.y * ((float)rc.icy / (float)(rc.isy-1));

      for(int i=0; i<4; i++){
         S_vertex &tv = tlv[i];

         tv.v.z = 1.0f;
         tv.rhw = 1.0f;
         S_vector2 &v = screen_rect[i];
         v.x = -rc.cx * sx * scrn_sx;
         v.y = -rc.cy * size1 * scrn_sx;
         if(i&1) v.x += sx * scrn_sx;
         if(i&2) v.y += size1 * scrn_sx;
                                 //setup texture coordinates
         tv.tx.x = rc.l;
         tv.tx.y = rc.t;
         if(i&1) tv.tx.x += rc.sx;
         if(i&2) tv.tx.y += rc.sy;
      }
      SetPos(S_vector2(x, y));
      SetColor(0xffffffff);
   }

   const S_vector2 *GetScreenRect() const { return screen_rect; }

   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

   virtual void SetOn(bool b){ on = b; }
   virtual bool IsOn() const{ return on; }

//----------------------------
   virtual void SetScreenPos(const S_vector2 &p){

      if(screen_pos!=p){
         screen_pos = p;
         PrepareDraw();
      }
   }
   virtual const S_vector2 &GetScreenPos() const{ return screen_pos; }

//----------------------------

   virtual void SetPos(const S_vector2 &p){

      if(pos!=p){
         pos = p;
         CPIGraph igraph = img->GetTexture()->GetDriver()->GetGraphInterface();
         float scrn_sx = (float)igraph->Scrn_sx();
         SetScreenPos(S_vector2(FloatToInt(scrn_sx*pos.x), FloatToInt(scrn_sx*pos.y)));
      }
   }
   virtual const S_vector2 &GetPos() const{ return pos; }

//----------------------------
   virtual const S_vector2 &GetSize() const{ return size; }
   virtual const S_vector2 &GetHotSpot() const{ return hotspot; }
   virtual int GetZ() const{ return z; }

//----------------------------
// Set new size to sprite. Currently not implemented.
// This version required to remember index of created sprite and calculation of
// new rectangle size is little confusing. Also spaces between letters was not corrected.
// I suggest step back in next implementation and revisit sprite class first.
   /*virtual void SetSize(float new_size){
                              
      //PIGraph igraph = img->GetTexture()->GetDriver()->GetGraphInterface();
      //assert(img);
      //const S_sprite_rectangle &rc = img->GetRects()[indx];
      //float sx = rc.sx * size1 / rc.sy * aspect;
      //size.x = sx;
      //size.y = size1;

      float size1 = size.y;
      size.x = size.x * new_size / size1;
      size.y = new_size;

                              //recompute screen rect
      PIGraph igraph = img->GetTexture()->GetDriver()->GetGraphInterface();
      assert(img);
      const S_sprite_rectangle &rc = img->GetRects()[create_indx];

      float scrn_sx = (float)igraph->Scrn_sx();
      float sx = size.x * img->GetAspectRatio();

      for(int i=0; i<4; i++){

         S_vector2 &v = screen_rect[i];
         v.x = -rc.cx * sx * scrn_sx;
         v.y = -rc.cy * size1 * scrn_sx;
         if(i&1) v.x += sx * scrn_sx;
         if(i&2) v.y += size1 * scrn_sx;
      }
   }*/

//----------------------------
   virtual void SetColor(dword c){
      color = c;
      for(int i=4; i--; )
         tlv[i].diffuse = c;
      vis_contents_dirty = true;
   }
   virtual dword GetColor() const{ return color; }

//----------------------------
   virtual void Render(PI3D_scene scene) const{

      if(!on)
         return;
                              //prepare dynamic visual, if not yet
      if(vis_contents_dirty){
         PI3D_driver drv = scene->GetDriver();
         if(!dyn_vis){
            dyn_vis = (PI3D_visual_dynamic)scene->CreateFrame(FRAME_VISUAL, I3D_VISUAL_DYNAMIC);
            dyn_vis->Release();
         }
         I3D_triface fc[2];
         fc[0][0] = 2;
         fc[0][1] = 0;
         fc[0][2] = 1;
         fc[1][0] = 2;
         fc[1][1] = 1;
         fc[1][2] = 3;

         I3D_face_group fg;
         PI3D_material mat;
                              //re-use previous material, if possible
         if(dyn_vis->NumFGroups()){
            mat = const_cast<PI3D_material>(dyn_vis->GetFGroups()->GetMaterial());
            mat->AddRef();
         }else
            mat = drv->CreateMaterial();

         fg.base_index = 0;
         fg.num_faces = 2;

         fg.SetMaterial(mat);
         mat->SetTexture(MTI_DIFFUSE, const_cast<PI3D_texture>(img->GetTexture()));
         mat->Release();

         dyn_vis->Build(tlv, 4, I3DVC_XYZRHW | I3DVC_DIFFUSE | /*I3DVC_SPECULAR | */(1<<I3DVC_TEXCOUNT_SHIFT),
            fc, 2, &fg, 1);

         vis_contents_dirty = false;
      }
      dyn_vis->Render();
   }

//----------------------------
   virtual void GetExtent(float &l, float &t, float &r, float &b) const{
      l = pos.x - hotspot.x;
      t = pos.y - hotspot.y;
      r = l + size.x;
      b = t + size.y;
   }

//----------------------------
   virtual void GetTextureExtent(float &l, float &t, float &r, float &b) const{
      l = tlv[0].tx.x;
      t = tlv[0].tx.y;
      r = tlv[3].tx.x;
      b = tlv[3].tx.y;
   }

//----------------------------
// Shear - italic-look.
   virtual void Shear(float angle, float base_y){

      float dy = base_y - screen_pos.y;
      //float tan_a = (float)tan(angle);
      float tan_a = angle;
      {
         float sy1 = -screen_rect[0].y;
         sy1 += dy;
         float dx = sy1 * tan_a;
         screen_rect[0].x += dx;
         screen_rect[1].x += dx;
      }
      {
         float sy1 = -screen_rect[2].y;
         sy1 += dy;
         float dx = sy1 * tan_a;
         screen_rect[2].x += dx;
         screen_rect[3].x += dx;
      }
      PrepareDraw();
   }

//----------------------------

   virtual void Rotate(float angle, const S_vector2 &pt){

      float sa = (float)sin(angle);
      float ca = (float)cos(angle);

      for(dword i=4; i--; ){
         S_vector2 &p = screen_rect[i];
         S_vector2 dir = p - pt;

         dir = S_vector2(dir.x*ca+dir.y*sa, -dir.x*sa+dir.y*ca);
         p = pt + dir;
      }
      PrepareDraw();
   }

//----------------------------
// Get associated sprite image.
   virtual CPC_sprite_image GetSpriteImage() const{ return img; }

//----------------------------

   virtual void SetScale(const S_vector2 &scl){

      CPIGraph igraph = img->GetTexture()->GetDriver()->GetGraphInterface();
      float scrn_sx = (float)igraph->Scrn_sx();
      size = scl;
      screen_rect[3].x = screen_rect[1].x = screen_rect[0].x + size.x * scrn_sx;
      screen_rect[3].y = screen_rect[2].y = screen_rect[0].y + size.y * scrn_sx;

      PrepareDraw();
   }

//----------------------------

   virtual void SetUV(const S_vector2 &uv_lt, const S_vector2 &uv_rb){

      tlv[0].tx = uv_lt;
      tlv[3].tx = uv_rb;

      tlv[1].tx.x = uv_rb.x; tlv[1].tx.y = uv_lt.y;
      tlv[2].tx.x = uv_lt.x; tlv[2].tx.y = uv_rb.y;
   }
};

//----------------------------

C_sprite *CreateSprite(float x, float y, float size, CPC_sprite_image img, dword indx, int z, float aspect){

   if(!img)
      return NULL;
   C_sprite_imp *spr = new C_sprite_imp(x, y, size, img, indx, z, aspect);
   return spr;
}

//----------------------------

                              //Group of sprites - usualy ones using the same sprite image.
                              //All these sprites are rendered together.
                              //Usable for text consisting from more letters - sprites.
class C_sprite_group_imp: public C_sprite_group{
   dword ref;

   mutable C_vector<C_smart_ptr<C_sprite> > sprs;
   S_vector2 pos;             //position in normalized coords
   mutable bool resort;

                              //clipping rect in normalised screen coords
   S_rect_f clip_rect;

   struct S_vertex{
      S_vectorw v;
      dword diffuse;
      I3D_text_coor tex;
   };

   mutable C_smart_ptr<I3D_visual_dynamic> dyn_vis;
   mutable bool vis_contents_dirty;

   
#ifdef ONE_VISUAL
   void PrepareDraw(PI3D_scene scene) const{

      if(!vis_contents_dirty) 
         return;

      vis_contents_dirty = false;
      if(!sprs.size())
         return;

      PI3D_driver drv = scene->GetDriver();
      if(!dyn_vis){
         dyn_vis = (PI3D_visual_dynamic)scene->CreateFrame(FRAME_VISUAL, I3D_VISUAL_DYNAMIC);
         dyn_vis->Release();
      }
                              //rectangle index offset
      static const word rect_indx[] = {0, 1, 2, 2, 1, 3};

                              //create verticies and faces
                              // (currently only one material, one face_group)
      C_vector<S_vertex> verts;
      C_vector<I3D_triface> faces;
      //C_vector<I3D_face_group> fgroups;

      verts.reserve(sprs.size()*4);
      faces.reserve(sprs.size()*2);

                              //deform by resolution
      PIGraph igraph = drv->GetGraphInterface();
      float screen_aspect_ratio = 1.3333333333f*((float)igraph->Scrn_sy()/(float)igraph->Scrn_sx());

                              //todo:aspect ratio
      float clip_scr_b = clip_rect.b /.75f * igraph->Scrn_sy();
      float clip_scr_t = clip_rect.t /.75f * igraph->Scrn_sy();
#if 1
      float clip_scr_l = clip_rect.l * igraph->Scrn_sx();
      float clip_scr_r = clip_rect.r * igraph->Scrn_sx();
#endif

      for(dword i = 0; i < sprs.size(); i++){

         if(!sprs[i]->IsOn())
            continue;
                              //check for full clipped sprites
         S_vector2 pos = sprs[i]->GetPos();
         const S_vector2 &size = sprs[i]->GetSize();
         const S_vector2 &hotspot = sprs[i]->GetHotSpot();
         pos -= hotspot;
                              //top and bottom
         bool out_t = (pos.y > clip_rect.b);
         bool out_b = (pos.y + size.y < clip_rect.t);
         if(out_t || out_b)
            continue;
                              //left and right
         bool out_l = (pos.x + size.x < clip_rect.l);
         bool out_r = (pos.x > clip_rect.r);
         if(out_l || out_r)
            continue;

                              //make 2 faces and 4 verticies for sprite
         int last_vertex_index = verts.size();
         for(int k1 = 0; k1 < 2; k1++){
            faces.push_back(I3D_triface());
            I3D_triface &tf = faces.back();
            for(int k2 = 0; k2 < 3; k2++)
               tf[k2] = last_vertex_index+rect_indx[k1*3+k2];
         }

         const S_vector2 &screen_pos = sprs[i]->GetScreenPos();
         const S_vector2 *screen_rect = ((C_sprite_imp *)(C_sprite *)sprs[i])->GetScreenRect();
                              //image rectangle for texture coord
         float l, t, r, b;
         sprs[i]->GetTextureExtent(l, t, r, b);

         for(int j = 0; j < 4; j++){

            verts.push_back(S_vertex());
            S_vertex &vm = verts.back();
            vm.v.z = 1.0f;
            vm.v.w = 1.0f;
            vm.diffuse = sprs[i]->GetColor();

                                 //setup rectangle coordinates
            vm.v.x = (screen_pos.x + screen_rect[j].x);
            vm.v.y = (screen_pos.y + screen_rect[j].y) * screen_aspect_ratio;

                              //check for clipping
                              //top and bottom
            float size_y = (screen_rect[2].y - screen_rect[0].y) * screen_aspect_ratio;
            float top_clip(0), bottom_clip(0);
            if(j < 2){

               float new_y = Max(clip_scr_t, vm.v.y);
               top_clip = (new_y - vm.v.y) / size_y;
               vm.v.y = new_y;

            }else{
               float new_y = Min(clip_scr_b, vm.v.y);
               bottom_clip = (vm.v.y - new_y) / size_y;
               vm.v.y = new_y;
            }
                              //left and right
#if 1
            float size_x = (screen_rect[1].x - screen_rect[0].x) * screen_aspect_ratio;
            float left_clip(0), right_clip(0);
            if(j&1){
                              //right
               float new_x = Min(clip_scr_r, vm.v.x);
               right_clip = (vm.v.x - new_x) / size_x;
               vm.v.x = new_x;
            }else{
                              //left
               float new_x = Max(clip_scr_l, vm.v.x);
               left_clip = (new_x - vm.v.x) / size_x;
               vm.v.x = new_x;
            }
#endif
                              //clip texture aproximate to rectangle
            float t_sx = r-l;
            float t_sy = b-t;
            vm.tex.x = l;
            vm.tex.y = t;
            if(top_clip)
               vm.tex.y += (t_sy)*top_clip;
            if(j&2){
               vm.tex.y += t_sy;
               if(top_clip)
                  vm.tex.y -= (t_sy)*top_clip;
               if(bottom_clip)
                  vm.tex.y -= (t_sy)*bottom_clip;
            }
#if 1
            if(left_clip)
               vm.tex.x += (t_sx)*left_clip;
#endif
            if(j&1){
               vm.tex.x += t_sx;
#if 1
               if(left_clip)
                  vm.tex.x -= (t_sx)*left_clip;
               if(right_clip)
                  vm.tex.x -= (t_sx)*right_clip;
#endif
            }
         }
      }
                              //take texture from first image,
                              //more face groups not supported now
                              //todo: more FGroups, by sprite texture
      CPC_sprite_image sp = sprs[0]->GetSpriteImage();
      I3D_texture *text = const_cast<PC_sprite_image>(sp)->GetTexture();

                                 //make only one group
      //fgroups.push_back(I3D_face_group());
      //I3D_face_group &fg = fgroups.back();
      I3D_face_group fg;
      fg.base_index = 0;
      fg.num_faces = faces.size();
      PI3D_material mat = drv->CreateMaterial();
      fg.SetMaterial(mat);
      mat->SetTexture(MTI_DIFFUSE, text);
      mat->Release();

      S_vertex *p_verts = &verts.front();
      I3D_triface *p_faces = &faces.front();
      //I3D_face_group *p_fgroups = fgroups.begin();

      dword vc_flags = I3DVC_XYZRHW | I3DVC_DIFFUSE | (1<<I3DVC_TEXCOUNT_SHIFT);

                              //allow no render when all sprites are hidden
      if(!verts.size() || !faces.size()){
         dyn_vis = NULL;
         return;
      }

      dyn_vis->Build(p_verts, verts.size(), vc_flags,
         p_faces, faces.size(),
         //p_fgroups, fgroups.size());
         &fg, 1);
   }
#endif

//----------------------------
   
   friend class C_poly_text;  //direct access to our sprites
public:
   C_sprite_group_imp():
      ref(1),
      resort(false),
      vis_contents_dirty(false),
      clip_rect(.0f, .0f, 1.0f, .75f),
      pos(0, 0)
   {}

//----------------------------
   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

   virtual void AddSprite(PC_sprite spr){
      sprs.push_back(spr);
      resort = true;
      vis_contents_dirty = true;
   }

   virtual void RemoveSprite(PC_sprite spr){

      for(int i=sprs.size(); i--; )
      if(spr==sprs[i]){
         sprs[i] = sprs.back(); sprs.pop_back();
         resort = true;
         vis_contents_dirty = true;
         break;
      }
   }

   static bool spr_less(C_sprite *s1, C_sprite *s2){ return s1->GetZ()<s2->GetZ(); }

   void Render(PI3D_scene scene) const{

      if(resort){
         sort(sprs.begin(), sprs.end(), spr_less);
         resort = false;
         vis_contents_dirty = true;
      }

                              //there's some bug in color copying, we need always refresh VB
      vis_contents_dirty = true;
#ifdef ONE_VISUAL
      if(vis_contents_dirty)
         PrepareDraw(scene);
      if(dyn_vis)
         dyn_vis->Render();
#else
      
      for(int i=0; i<(int)sprs.size(); i++)
         sprs[i]->Render(scene);
#endif //ONE_VISUAL

   }

//----------------------------
// Set color of all sprites.
   void SetColor(dword c){

      for(int i=sprs.size(); i--; )
         sprs[i]->SetColor(c);
#ifdef ONE_VISUAL
      vis_contents_dirty = true;
#endif
   }

//----------------------------

   void SetColor(dword c, int z){

      for(int i=sprs.size(); i--; ){
         if(sprs[i]->GetZ() == z)
            sprs[i]->SetColor(c);
      }
#ifdef ONE_VISUAL
      vis_contents_dirty = true;
#endif
   }

//----------------------------
// Set group's position.
   virtual void SetPos(const S_vector2 &p){

      //if(IsMrgZeroLess((pos-p).Square())) return;
      if(pos==p)
         return;
      pos = p;
#ifdef ONE_VISUAL
      vis_contents_dirty = true;
#endif
   }

//----------------------------
// Get group's position.
   virtual const S_vector2 & GetPos() const{ return pos; }

//----------------------------

   /*virtual void SetSize(float size1){

      for(int i=sprs.size(); i--; )
         sprs[i]->SetSize(size1);
   #ifdef ONE_VISUAL
      vis_contents_dirty = true;
   #endif
   }*/

//----------------------------
// Enable/disable all sprites.
   void SetOn(bool b){

      for(int i=sprs.size(); i--; )
         sprs[i]->SetOn(b);
   }

//----------------------------
// Get sprites.
   virtual const PC_sprite *GetSprites() const{ return (PC_sprite*)&sprs.front(); }
   virtual dword NumSprites() const{ return sprs.size(); }

//----------------------------
// Clipping in relative screen coord. Rect - pointer to 4 float array.
   virtual void SetClipRect(float *rect1){

      if(!rect1){
         clip_rect.l = .0f;
         clip_rect.t = .0f;
         clip_rect.r = 1.0f;
         clip_rect.b = .75f;
         return;
      }
      assert(rect1[0] >= .0f && rect1[0] <= 1.0f);
      assert(rect1[2] >= .0f && rect1[2] <= 1.0f);
      assert(rect1[1] >= .0f && rect1[1] <= .75f);
      assert(rect1[3] >= .0f && rect1[3] <= .75f);

      //for(int i = 4; i--; )
         //clip_rect[i] = rect1[i];
      clip_rect.l = rect1[0];
      clip_rect.t = rect1[1];
      clip_rect.r = rect1[2];
      clip_rect.b = rect1[3];
   }
};

//----------------------------

PC_sprite_group CreateSpriteGroup(){

   return new C_sprite_group_imp;
}

//----------------------------
