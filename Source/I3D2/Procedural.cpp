#include "all.h"
#include "procedural.h"
#include "loader.h"
#include "driver.h"
#include "frame.h"
#include <list>

#pragma warning(disable: 4244)

//----------------------------

                              //how many msec must pass between 2 computed frames
                              // this is good for time accumulation
                              // so that we don't compute contents each frame
//#define MIN_TICK_TIME 66

//----------------------------

I3D_procedural_base::I3D_procedural_base(CPI3D_driver d1, const C_str &n, I3D_PROCEDURAL_ID pid1, dword id):
   drv(d1),
   pid(pid1),
   init_data(id),
   name(n),
   last_render_time(d1->GetRenderTime())
{
   drv->AddCount(I3D_CLID_PROCEDURAL);
}

//----------------------------

I3D_procedural_base::~I3D_procedural_base(){

   drv->UnregisterProcedural(this);
   drv->DecCount(I3D_CLID_PROCEDURAL);
}

//----------------------------
//----------------------------
                              //procedural morph - implementation

class I3D_procedural_morph_imp: public I3D_procedural_morph{
   dword num_channels;

                              //animation values:
   int anim_time_base, anim_time_thresh;
   S_vector scale;            //3D scale of possible animation

   float anim_dist_min, anim_dist_thresh;

   struct S_channel{
      C_bezier_curve<S_vector> bezier_pos;
      S_vector pos[4];

      int anim_time;          //time in which we want to get to animate this phase
      int time_countdown;     //time counter

      S_channel():
         anim_time(0),
         time_countdown(0)
      {
         memset(pos, 0, sizeof(pos));
      }

      void SetupRandomKey(const S_vector &scale){
                              //shift all previous vectors
         pos[0] = pos[1];
         pos[1] = pos[2];
         pos[2] = pos[3];
                              //create new unique key
         S_vector &new_key = pos[3];
         for(int i=0; i<3; i++){
            new_key[i] = S_float_random(scale[i] * 2.0f) - scale[i];
         }
                              //prepare bezier position
         S_vector t0, t1;     //bezier tangents
         t0 = pos[2] - pos[0];
         t1 = pos[1] - pos[3];
         float scl = (pos[2] - pos[1]).Magnitude() * .3333f;
         if(!IsAbsMrgZero(scl)){
            float f;
            f = t0.Magnitude();
            if(!IsAbsMrgZero(f))
               t0 *= scl / f;
            f = t1.Magnitude();
            if(!IsAbsMrgZero(f))
               t1 *= scl / f;
         }
         t0 += pos[1];
         t1 += pos[2];
         S_vector max_scale = scale;
         t0.Minimal(max_scale);
         t0.Maximal(-max_scale);
         t1.Minimal(max_scale);
         t1.Maximal(-max_scale);
         bezier_pos.Init(pos[1], t0, t1, pos[2]);
      }
   };
   C_vector<S_channel> channels;
   C_vector<S_vectorw> values;

   C_ease_interpolator ease;

   virtual I3D_RESULT Initialize(C_cache &ck, PI3D_LOAD_CB_PROC cb_proc, void *cb_context){

                              //try to init base
      I3D_RESULT ir = I3D_procedural_base::Initialize(ck, cb_proc, cb_context);
      if(I3D_FAIL(ir))
         return ir;

                              //parse the file
#define SKIP_WS while(*cp && isspace(*cp)) ++cp;
      C_str err;
      while(!ck.eof()){
         char line[256];
         ck.getline(line, sizeof(line));
         const char *cp = line;

         SKIP_WS;
         if(*cp == ';' || *cp==0) continue;

         char kw[256];
         sscanf(cp, "%256s", kw);
         cp += strlen(kw);
         SKIP_WS;

         if(!strcmp(kw, "scale")){
            if(sscanf(cp, "%f %f %f", &scale.x, &scale.y, &scale.z) != 3){
               err = "scale: expecting 3 parameters (x, y, z)";
               goto fail;
            }
         }else
         if(!strcmp(kw, "time")){
            if(sscanf(cp, "%i %i", &anim_time_base, &anim_time_thresh) != 2){
               err = "time: expecting 5 parameters (min and max)";
               goto fail;
            }
            if(anim_time_thresh < anim_time_base){
               err = "time: max can't be less than min";
               goto fail;
            }
            anim_time_thresh -= anim_time_base;
         }else
         if(!strcmp(kw, "num_channels")){
            int num = sscanf(cp, "%i", &num_channels);
            if(num!=1){
               err = "expecting 1 parameter";
               goto fail;
            }
            if(num_channels > MAX_VS_BLEND_MATRICES){
               err = C_fstr("too many morphing channels, max number is %i", MAX_VS_BLEND_MATRICES);
               goto fail;
            }
         }else{
            err = C_fstr("unknown command: '%s'", kw);
            goto fail;
         }
      }
      values.assign(num_channels, S_vectorw());
      channels.assign(num_channels, S_channel());
      memset(&values.front(), 0, num_channels*sizeof(S_vectorw));

      {
         for(int i=channels.size(); i--; ){
            channels[i].SetupRandomKey(scale);
         }
      }

      SyncRenderTime();
      return I3D_OK;
   fail:
      if(cb_proc)
         cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Procedural '%s': error: %s.", (const char*)GetName(),
            (const char*)err),
         1, cb_context);
      return I3DERR_INVALIDPARAMS;
   }

   virtual S_vector GetMaxMorphScale() const{
      //return scale * 1.3334f;
      return scale;
      /*
      float f = Max(scale.x, Max(scale.y, scale.z));
      f *= 1.4f;
      return S_vector(f, f, f);
      */
   }

   virtual void Evaluate(){
                              //check if we need to update
      if(last_render_time != drv->GetRenderTime()){
         int time = SyncRenderTime();
                              //update all channels
         for(dword i=num_channels; i--; ){
            S_channel &ch = channels[i];
            assert(ch.time_countdown <= ch.anim_time);
            if((ch.time_countdown -= time) <= 0){
                              //make random time
               ch.anim_time = anim_time_base;
               if(anim_time_thresh)
                  ch.anim_time += S_int_random(anim_time_thresh);
               ch.anim_time = Max(1, ch.anim_time);
               ch.time_countdown = ch.anim_time;

               ch.SetupRandomKey(scale);
            }
                              //compute current position
            S_vector &curr_pos = values[i];
            float f = 1.0f - (float)ch.time_countdown / (float)ch.anim_time;
            //S_vector v[2] = { curr_pos };
            ch.bezier_pos.Evaluate(f, curr_pos);
#if 0
            v[1] = curr_pos;
            drv->DEBUG_CODE(0, (dword)v);
#endif
            assert(I3DFabs(curr_pos.x) <= GetMaxMorphScale().x &&
               I3DFabs(curr_pos.y) <= GetMaxMorphScale().y &&
               I3DFabs(curr_pos.z) <= GetMaxMorphScale().z);
         }
      }
   }
public:
   I3D_procedural_morph_imp(CPI3D_driver d1, const C_str &name, dword init_data):
      I3D_procedural_morph(d1, name, init_data),
      anim_time_base(0), anim_time_thresh(0),
      anim_dist_min(0.0f), anim_dist_thresh(0.0f),
      scale(1, 1, 1),
      num_channels(0)
   {
   }

   //I3DMETHOD_(dword,AddRef)(){ return ++ref; }
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   virtual int GetNumChannels() const{ return num_channels; }

   virtual const S_vectorw *GetMorphingValues() const{
      return &values.front();
   }
};

//----------------------------
//----------------------------

class I3D_procedural_particle_imp: public I3D_procedural_particle{

#define PRTF_BASE_DIR_WORLD   2  //base dir in world coordinates
#define PRTF_DEST_DIR_WORLD   4  //dest dir in world coordinates

   mutable dword prt_flags;

   struct S_elem{
                              //current:
      S_vector pos;
      float scale;
      float base_opacity, opacity;
      float rotation_angle;
                              //life:
      int life_time, life_cnt;
                              //direction:
      S_vector base_dir, dest_dir;
      int affect_time, affect_cnt;
      float scale_dir;
      float base_scale;
      float rotation_speed;   //<0 ... left, 0 ... no, >0 ... right
      S_matrix matrix;
      bool operator < (const S_elem &e){ return (dword)this < (dword)&e; }
      bool operator == (const S_elem &e){ return (dword)this == (dword)&e; }
      bool operator != (const S_elem &e){ return (dword)this != (dword)&e; }
   };
   list<S_elem> elems;
                              //particle param block
   int out_time_count;
   int out_time[2];
   int counter;               //how many elements we'll produce
   int count_down;            //how many elements we still have to produce (-1 = stopped)
   int life_len[2];
   //float element_scale;
   float scale_range[2];
   float init_pos_disp;
   float init_dir_disp;
   float affect_dir_dips;
   int affect_time[2];
   float opt_range[2];
   //float opacity;
   S_vector init_dir;
   S_vector dest_dir;
   byte blend_mode;
   float rotation_speed_min;
   float rotation_speed_max;
   dword last_emit_base;

public:
   I3D_procedural_particle_imp(CPI3D_driver d1, const C_str &name, dword init_data):
      I3D_procedural_particle(d1, name, init_data)
   {
      Reset();
   }

   void Reset(){

      out_time_count = 1;
      elems.clear();
                                 //setup default particle
      out_time[0] = 100;
      out_time[1] = 60;
      life_len[0] = 1800;
      life_len[1] = 600;
      scale_range[0] = -.25f;
      scale_range[1] = .6f;
      init_pos_disp = .2f;
      init_dir_disp = 1.0f;
      affect_time[0] = 400;
      affect_time[1] = 100;
      affect_dir_dips = 1.0f;
      opt_range[0] = .1f;
      opt_range[1] = .1f;
      init_dir = S_vector(0, 1, 0);
      dest_dir = S_vector(.2f, .8f, 0);
      prt_flags = 0;
      rotation_speed_min = -.4f * PI;
      rotation_speed_max =  .4f * PI;
      //SetParam(PRT_ELEMENT_SCALE, FloatAsInt(1.0f));
      //SetParam(PRT_OPACITY, FloatAsInt(1.0f));
      //SetParam(PRT_DRAW_MODE, I3DBLEND_ADD);
      //SetParam(PRT_SETCOUNT, 0);
   }

   void Tick(int time){

      /*
#ifdef ALLOW_SLICE_TIME
                                 //slice time
      while(time > Max(10, out_time[0]*2)){
         int t1 = Max(5, out_time[0]);
         Tick(t1);
         time -= t1;
      }
#endif//ALLOW_SLICE_TIME

      //const S_matrix &mat = GetMatrix();
      if(count_down!=-1)
      {
                                 //creation of new elements enabled
                                 //check if it's right time to do so
         out_time_count -= time;
         while(out_time_count <= 0){
                                 //comp delay to emit new element
            int add_time = out_time[0] + S_int_random(out_time[1]);
            if(!add_time) break; //problem - neverending loop detected - invalid params?!
            out_time_count += add_time;

                                 //create element info & file
            elems.push_back(S_elem());
            S_elem &si = elems.back();
            si.matrix.Identity();
                                 //get inherited scale
            si.base_scale = si.scale;
                                 //setup life length
            si.life_time = life_len[0] + S_int_random(life_len[1]);
            si.life_cnt = si.life_time;

            //si.base_opacity = opacity;
            si.base_opacity = 1.0f;

                                 //setup direction
            S_vector i_dir;
            //if(!(prt_flags&PRTF_BASE_DIR_WORLD)) i_dir = init_dir.RotateByMatrix(mat);
            //else
               i_dir = init_dir * si.base_scale;

            float idd = init_dir_disp * si.base_scale;
            si.dest_dir = S_vector(
               i_dir.x - idd * .5f + S_float_random() * idd,
               i_dir.y - idd * .5f + S_float_random() * idd,
               i_dir.z - idd * .5f + S_float_random() * idd);
            //const S_vector &pos = mat(3);
            float ipd = init_pos_disp * si.base_scale;
            si.matrix(3) = pos + S_vector(
               ipd/2.0f - S_float_random() * ipd,
               ipd/2.0f - S_float_random() * ipd,
               ipd/2.0f - S_float_random() * ipd);

                                 //setup scale
            si.scale_dir = scale_range[0] + S_float_random() * (scale_range[1] - scale_range[0]);
            si.affect_cnt = 0;

                                 //setup rotation
            si.rotation_speed = rotation_speed_min + S_float_random() * (rotation_speed_max - rotation_speed_min);
            if(!IsAbsMrgZero(si.rotation_speed)){
               si.rotation_angle = S_float_random() * GetPI2();
            }else
               *(dword*)&si.rotation_speed = 0;

            if(count_down && !--count_down){
               count_down = -1;
               break;
            }
         }
      }

      float tsec = (float)time * .001f;
                                 //move all elements
      for(list<S_elem>::iterator it=elems.begin(); it!=elems.end(); ){

         S_elem &si = (*it);
                                 //lifecounf
         if((si.life_cnt -= time) <= 0){
                                 //dead
            elems.erase(it++);
            continue;
         }
                                 //compute opacity
         float ft = 1.0f - (float)si.life_cnt / (float)si.life_time;
         si.opacity = ft<opt_range[0] ? ft/opt_range[0] :
            ft<=opt_range[1] ? 1.0f :
            1.0f - (ft-opt_range[1])/(1.0f-opt_range[1]);
         si.opacity *= si.base_opacity;
                                 //compute affect direction
         if((si.affect_cnt -= time) <= 0){
                                 //don't accept less than 1, we divide by this later
            si.affect_time = affect_time[0] + S_int_random(affect_time[1]);
            si.affect_cnt = si.affect_time;
            si.base_dir = si.dest_dir;
            float mag;
            float dot = si.dest_dir.Dot(si.dest_dir);
            if(dot <= (MRG_ZERO*MRG_ZERO)) mag = 0.0f;
            else mag = I3DSqrt(dot);

            if(!(prt_flags&PRTF_DEST_DIR_WORLD))
               si.dest_dir = (si.dest_dir + dest_dir.RotateByMatrix(mat));
            else
               si.dest_dir = (si.dest_dir + dest_dir * si.base_scale);

            dot = si.dest_dir.Dot(si.dest_dir);
            float new_mag;
            if(dot <= (MRG_ZERO*MRG_ZERO)) new_mag = 0.0f;
            else{
               new_mag = I3DSqrt(dot);
               si.dest_dir *= mag / new_mag;
            }

            float add = affect_dir_dips * si.base_scale;
            si.dest_dir += S_vector(
               add*.5f - S_float_random() * add,
               add*.5f - S_float_random() * add,
               add*.5f - S_float_random() * add);
         }
                                 //compute new position
         float fd = (float)si.affect_cnt / (float)si.affect_time;
         S_vector dir = si.base_dir * fd + si.dest_dir * (1.0f - fd);
         si.matrix(3) += dir * tsec;
                                 //adjust scale
         si.scale *= (1.0f + si.scale_dir * tsec);

                                 //compute new rotation angle
         if(FloatAsInt(si.rotation_speed) != 0){
            si.rotation_angle += si.rotation_speed * tsec;
         }

         it++;
      }
      vis_flags &= ~VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_HR_BOUND_VALID;

      {
         PI3D_frame frm1a = this;
         do{
            frm1a->SetFrameFlags(frm1a->GetFrameFlags() & ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID));
         }while(frm1a=frm1a->GetParent1());
      }
      */
   }

   virtual void Evaluate(){
                              //check if we need to update
      if(last_render_time != drv->GetRenderTime()){
         int time = SyncRenderTime();
         Tick(time);
      }
   }

};

//----------------------------
//----------------------------

I3D_procedural_texture::I3D_procedural_texture(CPI3D_driver d1, const C_str &name, dword init_data):
   I3D_procedural_base(d1, name, PROCID_TEXTURE, init_data),
   I3D_texture((PI3D_driver)d1),
   texel_look_up(NULL),
   life_len(1000),
   life_len_thresh(0),
   create_time(100),
   create_time_thresh(0),
   create_countdown(0),
   cumulate_time(0),
   frame_counter(0),
   init_radius_ratio(.1f),
   radius_grow_ratio(.2f),
   shift_speed(1.0f),
   wave_repeat(5.0f),
   unit_scale(.75f),
   height_curve(0.0f),
   mode(MODE_ALPHA),
   true_color(false)
{
}

//----------------------------

I3D_procedural_texture::~I3D_procedural_texture(){

   delete[] (byte*)texel_look_up;
}

//----------------------------

   /*
void I3D_procedural_texture::BuildSqrtTable(){

   float f;
   dword &fi = *(dword*)&f;

   for(dword i=0; i <= 0x7f; i++){
      fi = 0;
                              //Build a float with the bit pattern i as mantissa
                              //and an exponent of 0, stored as 127
      fi = (i<<16) | (127<<23);
      f = I3DSqrt(f);
                              //Take the square root then strip the first 7 bits of
                              //the mantissa into the table
      sqrt_lut[i] = fi&0x7fffff;
                              //Repeat the process, this time with an exponent of
                              //1, stored as 128
      fi = 0;
      fi = (i<<16) | (128<<23);
      f = I3DSqrt(f);
      sqrt_lut[i+0x80] = fi&0x7fffff;
   }
}
   */

//----------------------------

bool I3D_procedural_texture::InitTexture(dword size, const I3D_CREATETEXTURE *ct, PI3D_LOAD_CB_PROC cb_proc, void *cb_context){

   vidmem_image = NULL;
   sysmem_image = NULL;
   d3d_tex = NULL;
   txt_flags &= ~TXTF_ALPHA;

   dword findtf_flags = 0;
   switch(mode){
   case MODE_RGBA:
   case MODE_ALPHA:
      findtf_flags |= FINDTF_ALPHA;
      break;
   case MODE_EMBM:
      if(!(I3D_texture::drv->GetFlags()&DRVF_EMBMMAPPING))
         return false;
      findtf_flags |= FINDTF_EMBMMAP;
      break;
   }
   if(true_color)
      findtf_flags |= FINDTF_TRUECOLOR;

   I3D_RESULT ir = I3D_texture::drv->FindTextureFormat(pf, findtf_flags);
   if(I3D_FAIL(ir))
      return false;

   if(ct && (ct->flags&TEXTMAP_DIFFUSE)){
      S_pixelformat pf1 = pf;
      I3D_CREATETEXTURE ct1 = *ct;
      ct1.flags |= TEXTMAP_NOMIPMAP | TEXTMAP_USEPIXELFORMAT | TEXTMAP_HINTDYNAMIC;
      ct1.pf = &pf1;
      Open(ct1, cb_proc, cb_context, NULL);
   }else{

                              //limit size to device's limits
      const D3DCAPS9 &caps = *I3D_texture::drv->GetCaps();
      dword maxtx = caps.MaxTextureWidth  ? caps.MaxTextureWidth  : 256;
      dword maxty = caps.MaxTextureHeight ? caps.MaxTextureHeight : 256;
      size_x = Min(Max(4ul, size), maxtx);
      size_y = Min(Max(4ul, size), maxty);

                              //if no dynamic texture support, we must go through sysmem image
      if(!(I3D_texture::drv->GetCaps()->Caps2&D3DCAPS2_DYNAMICTEXTURES)){
                                 //create image containing the texture
         PIGraph igraph = I3D_texture::drv->GetGraphInterface();
         sysmem_image = igraph->CreateImage(); sysmem_image->Release();
         SetDirty();
         bool b = sysmem_image->Open(NULL, IMGOPEN_TEXTURE | IMGOPEN_EMPTY | IMGOPEN_SYSMEM, size_x, size_y, &pf, 0);
         if(!b)
            return false;
      }
   }

   if(pf.flags&PIXELFORMAT_ALPHA)
      txt_flags |= TXTF_ALPHA;
                              //create look-up table for conversion
   LPC_rgb_conversion rgb_conv = I3D_texture::drv->GetGraphInterface()->CreateRGBConv();
   rgb_conv->Init(pf);
   delete[] (byte*)texel_look_up;

   struct S_byte3{ byte b[3]; };
   union S_pixel{
      void *vp;
      byte *bp;
      word *wp;
      dword *dwp;
      S_byte3 *b3;
   } tab, tab1;
   tab1.vp = NULL;

   if(mode==MODE_EMBM){
                              //separate maps for U and V
      texel_look_up = new byte[pf.bytes_per_pixel * 256 * 2];
                              //setup pointer to Dv table
      tab1.vp = texel_look_up; tab1.bp += pf.bytes_per_pixel * 256;
   }else{
      texel_look_up = new byte[pf.bytes_per_pixel * 256];
   }
   tab.vp = texel_look_up;

   for(dword i=0; i<256; i++){
      dword value;
      switch(mode){
      case MODE_ALPHA:
         value = rgb_conv->GetPixel(128, 128, 128, byte(i));
         //value = rgb_conv->GetPixel(0, 0, 0, byte(i));
         break;
      case MODE_EMBM:
         {
                              //store dV first
            value = rgb_conv->GetPixel(0, byte(i), 0, 0);
            switch(pf.bytes_per_pixel){
            case 1: tab1.bp[i] = byte(value); break;
            case 2: tab1.wp[i] = word(value); break;
            case 3: tab1.b3[i] = *(S_byte3*)&value; break;
            case 4: tab1.dwp[i] = value; break;
            }
            value = rgb_conv->GetPixel(byte(i), 0, 0, 0);
         }
         break;
      default:
         value = rgb_conv->GetPixel(byte(i), byte(i), byte(i), byte(i));
      }
      switch(pf.bytes_per_pixel){
      case 1: tab.bp[i] = byte(value); break;
      case 2: tab.wp[i] = word(value); break;
      case 3: tab.b3[i] = *(S_byte3*)&value; break;
      case 4: tab.dwp[i] = value; break;
      }
   }
   rgb_conv->Release();

   img_open_flags = 0;
   if(I3D_texture::drv->GetCaps()->Caps2&D3DCAPS2_DYNAMICTEXTURES)
      img_open_flags = IMGOPEN_HINTDYNAMIC;

   return true;
}

//----------------------------

I3D_RESULT I3D_procedural_texture::Initialize(C_cache &ck,// const I3D_CREATETEXTURE &ct,
   PI3D_LOAD_CB_PROC cb_proc, void *cb_context){

   I3D_RESULT ir = I3D_procedural_base::Initialize(ck, cb_proc, cb_context);
   if(I3D_FAIL(ir))
      return ir;

   life_len = 1200;
   life_len_thresh = 1000;
   create_time = 80;
   create_time_thresh = 80;
   init_radius_ratio = .1f;
   radius_grow_ratio = .2f;
   shift_speed = 1.0f;
   wave_repeat = 5.0f;
   dword texture_size = 256;

   //E_MODE optional_mode = MODE_ALPHA;
   //E_MODE optional_mode = MODE_NULL;
   mode = MODE_RGB;
   switch(GetInitData()){
   case MTI_EMBM: mode = MODE_EMBM; break;
   case MTI_DETAIL:
      //mode = MODE_ALPHA;
      mode = MODE_RGB;
      break;
   }
                              //parse the file
#define SKIP_WS while(*cp && isspace(*cp)) ++cp;
   C_str err;
   while(!ck.eof()){
      char line[256];
      ck.getline(line, sizeof(line));
      const char *cp = line;

      SKIP_WS;
      if(*cp == ';' || *cp==0) continue;

      char kw[256];
      sscanf(cp, "%256s", kw);
      cp += strlen(kw);
      SKIP_WS;

      /*
      if(!stricmp(kw, "mode")){
         for(int i=0; i<2; i++){
            E_MODE m;
            if(sscanf(cp, "%256s", kw) != 1)
               break;
            cp += strlen(kw);
            SKIP_WS;
            if(!stricmp(kw, "RGB")) m = MODE_RGB;
            else
            if(!stricmp(kw, "ALPHA")) m = MODE_ALPHA;
            else
            if(!stricmp(kw, "RGBA")) m = MODE_RGBA;
            else
            if(!stricmp(kw, "BUMP")) m = MODE_BUMP;
            else{
               err = C_fstr("mode: invalid mode name: %s", kw);
               goto fail;
            }
            (!i ? mode : optional_mode) = m;
         }
      }else
         */
      if(!stricmp(kw, "true_color")){
         true_color = true;
      }else
      if(!stricmp(kw, "texture_size_bits")){
         if(sscanf(cp, "%i", &texture_size) != 1){
            err = "texture_size_bits: expecting 1 parameter";
            goto fail;
         }
         texture_size = 1<<texture_size;
      }else
      if(!stricmp(kw, "life_len")){
         if(sscanf(cp, "%i %i", &life_len, &life_len_thresh) != 2){
            err = "life_len: expecting 2 parameters (min and max)"; goto fail;
         }
         if(life_len_thresh < life_len){
            err = "life_len: max can't be less than min"; goto fail;
         }
         life_len_thresh -= life_len;
      }else
      if(!stricmp(kw, "create_time")){
         if(sscanf(cp, "%i %i", &create_time, &create_time_thresh) != 2){
            err = "create_time: expecting 2 parameters (min and max)"; goto fail;
         }
         if(create_time_thresh < create_time){
            err = "create_time: max can't be less than min"; goto fail;
         }
         create_time_thresh -= create_time;
      }else
      if(!stricmp(kw, "init_radius")){
         if(sscanf(cp, "%f", &init_radius_ratio) != 1){
            err = "init_radius: expecting 1 parameter"; goto fail;
         }
      }else
      if(!stricmp(kw, "radius_grow")){
         if(sscanf(cp, "%f", &radius_grow_ratio) != 1){
            err = "radius_grow: expecting 1 parameter"; goto fail;
         }
      }else
      if(!stricmp(kw, "shift_speed")){
         if(sscanf(cp, "%f", &shift_speed) != 1){
            err = "shift_speed: expecting 1 parameter"; goto fail;
         }
      }else
      if(!stricmp(kw, "wave_repeat")){
         if(sscanf(cp, "%f", &wave_repeat) != 1){
            err = "wave_repeat: expecting 1 parameter"; goto fail;
         }
      }else
      if(!stricmp(kw, "unit_scale")){
         if(sscanf(cp, "%f", &unit_scale) != 1){
            err = "unit_scale: expecting 1 parameter"; goto fail;
         }
      }else
      if(!stricmp(kw, "height_curve")){
         if(sscanf(cp, "%f", &height_curve) != 1){
            err = "height_curve: expecting 1 parameter"; goto fail;
         }
      }else{
         err = C_fstr("unknown command: '%s'", kw);
         goto fail;
      }
   }

   if(!InitTexture(texture_size, NULL, cb_proc, cb_context))
      return I3DERR_TEXTURESNOTSUPPORTED;

   CreateLUTs();
   //BuildSqrtTable();
                              //prepare 1st noise
   Tick(life_len + life_len_thresh);
   return I3D_OK;

fail:
   if(cb_proc)
      cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Procedural file '%s': error: %s.", (const char*)GetName(), (const char*)err),
      1, cb_context);
   return I3DERR_INVALIDPARAMS;
}

//----------------------------

void I3D_procedural_texture::CreateLUTs(){

   for(dword i=TXTPROC_LUT_SIZE; i--; ){
      float f = (float)i / (float)TXTPROC_LUT_SIZE;
      cos_lut[i] = (float)cos(2.0f * PI * f) * unit_scale * 128.0f;
      fp_cos_asin_lut[i] = FloatToInt((float)cos((float)asin(f)) * 65536.0f);
   }
   for(dword y=32; y--; ){
      float fy2 = (float)y / 31.0f;
      fy2 *= fy2;
      for(dword x=32; x--; ){
         float fx = (float)x / 31.0f;
         dist_lut[y][x] = I3DSqrt(fy2 + fx * fx);
      }
   }
}

//----------------------------

IDirect3DBaseTexture9 *I3D_procedural_texture::GetD3DTexture() const{

   int time_delta = SyncRenderTime();
   if(time_delta){
      cumulate_time += time_delta;
      //if(!(++frame_counter %= 3))
      {
         ((I3D_procedural_texture*)this)->Tick(cumulate_time);
         cumulate_time = 0;
      }
   }
   return I3D_texture::GetD3DTexture();
}

//----------------------------

void I3D_procedural_texture::Tick(int time){

   PROFILE(I3D_texture::drv, PROF_PROC_TEXTURE);

   if((create_countdown -= time) <= 0){
      do{                     //keep creating until we're ahead of time again
         dword new_countdown = create_time;
         if(create_time_thresh)
            new_countdown += S_int_random(create_time_thresh);
                              //make new element's time
         dword el_time = life_len;
         if(life_len_thresh)
            el_time += S_int_random(life_len_thresh);
                              //check if it would survive
         if((int)el_time > -create_countdown){
                              //ok, create element
            elements.push_back(S_element());
            S_element &el = elements.back();
            el.x = S_int_random(size_x);
            el.y = S_int_random(size_y);
            el.life_len = el_time;
                              //adjust it's time, since it should be create a while ago
                              // also add 'time', cos it will be subtracted few lines below
            el.countdown = el_time + create_countdown + time;
         }
         create_countdown += new_countdown;
      } while(create_countdown <= 0);
   }

                              //create empty surface, fill with zero
   const dword MAX_STACK_ALLOC = 256*256;
   short *contents;
   if(size_x*size_y<=MAX_STACK_ALLOC)
      contents = (short*)alloca(size_x*size_y*sizeof(short));
   else
      contents = new short[size_x * size_y];
   memset(contents, 0, size_x * size_y * sizeof(word));
   float *f_c_lut = cos_lut;
   int *fp_ca_lut = fp_cos_asin_lut;

   int mask_x = (size_x-1), mask_y = (size_y-1);
   for(dword i=elements.size(); i--; ){
      S_element &el = elements[i];

                              //animate element
      if((el.countdown -= time) <= 0){
                              //this trop is gone
         el = elements.back(); elements.pop_back();
         continue;
      }
      assert(el.countdown <= el.life_len);
      
      float el_tsec = (float)(el.life_len - el.countdown) * .001f;
      float radius = (init_radius_ratio + radius_grow_ratio * el_tsec) * (float)size_x;
      float wave_shift = shift_speed * el_tsec;
                              //process all lines
      int y_pixels = FloatToInt(radius);
      if(!y_pixels)
         continue;

                              //compute power as shaped accross the time
      float sin_part = (float)el.countdown / (float)el.life_len;
      float inv_sin_part = sin_part + .5f; if(inv_sin_part < 1.0f) inv_sin_part += 1.0f;
                              //blend those 2 curves
      float power = (1.0f-height_curve)*(float)sin(sin_part*PI) + height_curve*(1.0f+(float)sin(inv_sin_part*PI));
      float r_radius = 1.0f / radius;

#if defined _MSC_VER && 1
                              //temp variables
      int i_tmp;
      const int fp_radius = FloatToInt(65536.0f * radius);
      const int x = el.x, y = el.y, sx = size_x;
      const float wrep = wave_repeat;//, wshf = el.wave_shift;
      const float f_LUT_SIZEx4 = TXTPROC_LUT_SIZE * 4.0f;
      const float f_LUT_SIZEx2 = TXTPROC_LUT_SIZE * 2.0f;
      const float *ptr_dist_lut = dist_lut[0];
      int dist_y;
      int fp_d;
      //float fy_2;
      const float CONST_32 = 31.0f;
      int count_x;
      __asm{
         push ecx
         dec y_pixels
                              //eax = free
                              //ebx = xl
                              //ecx = xr
                              //esi = linet
                              //edi = lineb
                              //st(0) = 1.0
         fld1
      loop_y:
                              //float fy = (float)dy * r_radius;
         fild y_pixels
         fmul r_radius
                              //float fy_2 = fy * fy
         fld st
         //fmul st, st
         //fstp fy_2
         fmul CONST_32
         fistp dist_y
         shl dist_y, 5
                              //int lut_index = (fy * TXTPROC_LUT_SIZE) & (TXTPROC_LUT_SIZE-1);
         fmul f_LUT_SIZEx4
         fistp i_tmp
         mov eax, i_tmp
         and eax, (TXTPROC_LUT_SIZE-1) << 2
                              //int x_pixels = f_cos_asin_lut[lut_index] * radius;
         add eax, fp_ca_lut
         mov eax, [eax]
         mul fp_radius
                              //if(!x_pixels) continue;
         test edx, edx
         jz next_y
         mov count_x, edx
                              //int yt = (el.y - (dy+1)) & mask_y;
         mov eax, y
         mov ecx, y_pixels
         mov edi, contents
         sub eax, ecx
         dec eax
         and eax, mask_y
                              //short *linet = contents + yt * size_x
         imul eax, sx
         lea esi, [edi+eax*2]
                              //int yb = (el.y + dy) & mask_y;
         add ecx, y
         and ecx, mask_y
                              //short *lineb = contents + yb*size_x;
         imul ecx, sx
         lea edi, [edi+ecx*2]
                              //int xl = (el.x - (dx+1)) & mask_x;
                              //int xr = (el.x + dx) & mask_x;
         mov ebx, x
         mov ecx, count_x
         sub ebx, ecx
         add ecx, x
         dec ecx
         dec count_x
         mov edx, ptr_dist_lut
      loop_x:
                              //float fx = (float)dx * r_radius;
         fild count_x
         fmul r_radius
                              //float d = sqrt(fx*fx + fy_2);
#if 0
         fmul st, st
         fadd fy_2
                              //use fast sqrt
         fstp i_tmp
         mov eax, i_tmp
         mov edx, eax
         shr eax, 16-2
         sub edx, 127<<23
         and eax, 0x1fc
         test edx, 1<<23
         jz ok1
         or eax, 0x200
      ok1:
         add eax, ptr_sqrt_lut
         mov eax, [eax]
         sar edx, 24
         add edx, 127
         shl edx, 23
         or eax, edx
         mov i_tmp, eax
         fld i_tmp
#elif 1
         fmul CONST_32
         fistp i_tmp
         mov eax, i_tmp
         add eax, dist_y
         //shl eax, 2
         //add eax, ptr_dist_lut
         //fld dword ptr[eax]
         fld dword ptr[edx + eax*4]
#else
         fmul st, st
         fadd fy_2

         fsqrt
#endif
         and ebx, mask_x
         and ecx, mask_x
         inc ebx
         dec ecx
                              //float ds = d * wave_repeat;
         fld st
         fmul wrep
                              //int lut_index = ((ds - wave_shift) * TXTPROC_LUT_SIZE * .5f) & (TXTPROC_LUT_SIZE-1);
         fsub wave_shift
         fmul f_LUT_SIZEx2
         fistp i_tmp
         mov eax, i_tmp
         and eax, (TXTPROC_LUT_SIZE-1) << 2
                              //float val = (1.0f - d) * power * f_cos_lut[lut_index];
         add eax, f_c_lut
         fsubr st, st(1)
         fmul power
         fmul dword ptr[eax]
                              //short sval = FloatToInt(val);
         fistp i_tmp
         mov eax, i_tmp
                              //linet[xl] += sval; linet[xr] += sval;
         add [esi+ebx*2-2], ax
         add [esi+ecx*2+2], ax
                              //lineb[xl] += sval; lineb[xr] += sval;
         add [edi+ebx*2-2], ax
         add [edi+ecx*2+2], ax

         dec count_x
         jns loop_x
      next_y:
         
         dec y_pixels
         jns loop_y
         fstp fp_d

         pop ecx
      }
#else //asm
      for(int dy=y_pixels; dy--; ){
                              //compute x range on this line
         float fy = (float)dy * r_radius;
         int lut_index = FloatToInt(fy * TXTPROC_LUT_SIZE) & (TXTPROC_LUT_SIZE-1);
         int x_pixels = FloatToInt(fp_ca_lut[lut_index] * radius) >> 16;
         if(!x_pixels)
            continue;

         int yt = (el.y - (dy+1)) & mask_y;
         int yb = (el.y + dy) & mask_y;
         short *linet = contents + yt*size_x, *lineb = contents + yb*size_x;
         float fy_2 = fy * fy;

         int xl = el.x - x_pixels;
         int xr = el.x + x_pixels-1;
         for(int dx=x_pixels; dx--; ++xl, --xr){
            xl &= mask_x;
            xr &= mask_x;
            float fx = (float)dx * r_radius;

            float d = I3DSqrt(fx*fx + fy_2);
            float ds = d * wave_repeat;
            int lut_index = FloatToInt((ds - wave_shift) * TXTPROC_LUT_SIZE * .5f) & (TXTPROC_LUT_SIZE-1);
            short sval = (short)FloatToInt((1.0f - d) * power * f_c_lut[lut_index]);
            linet[xl] += sval; linet[xr] += sval;
            lineb[xl] += sval; lineb[xr] += sval;
         }
      }
#endif//!asm
   }

                              //convert to surface's format
   //if(I3D_texture::drv->GetCaps()->Caps2&D3DCAPS2_DYNAMICTEXTURES){
   if(!sysmem_image){
      if(!vidmem_image)
         SetDirty();
      Manage(MANAGE_CREATE_ONLY);
   }
   IDirect3DTexture9 *d3d_txt;
   dword lock_flags = 0;
   //if(I3D_texture::drv->GetCaps()->Caps2&D3DCAPS2_DYNAMICTEXTURES){
   if(!sysmem_image){
      d3d_txt = vidmem_image->GetDirectXTexture();
      lock_flags |= D3DLOCK_DISCARD;
   }else
      d3d_txt = sysmem_image->GetDirectXTexture();

   D3DLOCKED_RECT lrc;
   HRESULT hr = d3d_txt->LockRect(0, &lrc, NULL, lock_flags);
   if(SUCCEEDED(hr)){

      struct S_byte3{ byte b[3]; };
      union S_generic_pixel{
         void *vp;
         byte *bp;
         word *wp;
         dword *dwp;
         S_byte3 *bp3;
      } ptr;
      ptr.vp = lrc.pBits;

      switch(mode){
      case MODE_EMBM:
         {
            S_generic_pixel tab_u, tab_v;
            tab_v.vp = tab_u.vp = texel_look_up;
            tab_v.bp += pf.bytes_per_pixel * 256;

            for(int y=size_y; y--; ptr.bp += lrc.Pitch){
               S_generic_pixel line = ptr;

               int it = (y-1)&mask_y;
               int ib = (y+1)&mask_y;
               const short *surf_line = contents, *surf_t = contents, *surf_b = contents;
               surf_line += size_x * y;
               surf_t += size_x * it;
               surf_b += size_x * ib;

               int x;

               switch(pf.bytes_per_pixel){
               case 2:
                  for(x = size_x ; x--; line.bp += 2){
                     int il = (x-1)&mask_x;
                     int ir = (x+1)&mask_x;

                     int du = Max(-128, Min(127, surf_line[ir] - surf_line[il])) & 0xff;
                     int dv = Max(-128, Min(127, surf_b[x] - surf_t[x])) & 0xff;
                     *line.wp = word(tab_u.wp[du] | tab_v.wp[dv]);
                  }
                  break;
               case 4:
                  for(x = size_x ; x--; line.bp += 4){
                     int il = (x-1)&mask_x;
                     int ir = (x+1)&mask_x;

                     int du = Max(-128, Min(127, surf_line[ir] - surf_line[il])) & 0xff;
                     int dv = Max(-128, Min(127, surf_b[x] - surf_t[x])) & 0xff;
                     *line.dwp = tab_u.dwp[du] | tab_v.dwp[dv];
                  }
                  break;
               default:
                  assert(0);
               }         
            }
         }
         break;

      case MODE_ALPHA:
         {
            const short *surf = contents;
            S_generic_pixel tab;
            tab.vp = texel_look_up;
            dword not_alpha = ~pf.a_mask;

            for(dword y=size_y; y--; ptr.bp += lrc.Pitch){
               S_generic_pixel line = ptr;

               dword x = size_x;
               switch(pf.bytes_per_pixel){
               case 1:
                  break;
               case 2:
#if defined _MSC_VER && 1
                                       //optimize - most common case
                  __asm{
                     push ecx
                                       //ecx = x
                                       //edx = surf
                                       //esi = tab
                                       //edi = line
                     mov edx, surf
                     mov esi, tab
                     mov edi, line
                     mov ecx, x
                     mov ebx, not_alpha
                  loop_x3:
                     movsx eax, word ptr[edx]
                     xor ebx, ebx
                     add eax, 128
                     sets bl
                     dec ebx
                     and eax, ebx
                     cmp eax, 256
                     jb ok3
                     mov eax, 255
                  ok3:
                     mov bx, [edi]
                     and ebx, not_alpha
                     or bx, [esi+eax*2]
                     mov [edi], bx
                     add edx, 2
                     add edi, 2
                     dec ecx
                     jnz loop_x3
                     mov surf, edx
                     mov line, edi

                     pop ecx
                  }
#else
                  for( ; x--; line.bp += 2){
                     word &w = *line.wp;
                     w &= not_alpha;
                     w |= tab.wp[Max(0, Min(255, 128 + *surf++))];
                  }
#endif
                  break;

               case 3:        //how can 24-bit format contain alpha?
                  break;

               case 4:
#if defined _MSC_VER && 1
                                       //optimize - most common case
                  __asm{
                     push ecx
                                       //edx = surf
                                       //esi = tab
                                       //edi = line
                     mov edx, surf
                     mov esi, tab
                     mov edi, line
                     mov ecx, x
                  loop_x5:
                     movsx eax, word ptr[edx]
                     xor ebx, ebx
                     add eax, 128
                     sets bl
                     dec ebx
                     and eax, ebx
                     cmp eax, 256
                     jb ok5
                     mov eax, 255
                  ok5:
                     mov ebx, [edi]
                     and ebx, not_alpha
                     or ebx, [esi+eax*4]
                     mov [edi], ebx
                     add edx, 2
                     add edi, 4
                     dec ecx
                     jnz loop_x5
                     mov surf, edx
                     mov line, edi

                     pop ecx
                  }
#else
                  for( ; x--; line.bp += 4){
                     dword &w = *line.dwp;
                     w &= not_alpha;
                     w |= tab.dwp[Max(0, Min(255, 128 + *surf++))];
                  }
#endif
                  break;
               }
            }
         }
         break;

      default:
         {
            const short *surf = contents;
            S_generic_pixel tab;
            tab.vp = texel_look_up;

            for(dword y=size_y; y--; ptr.bp += lrc.Pitch){
               S_generic_pixel line = ptr;

               dword x = size_x;
               switch(pf.bytes_per_pixel){
               case 1:
                  break;
               case 2:
#if defined _MSC_VER && 1
                                       //optimize - most common case
                  __asm{
                     push ecx
                                       //edx = surf
                                       //esi = tab
                                       //edi = line
                     mov edx, surf
                     mov esi, tab
                     mov edi, line
                     mov ecx, x
                  loop_x2:
                     movsx eax, word ptr[edx]
                     xor ebx, ebx
                     add eax, 128
                     sets bl
                     dec ebx
                     and eax, ebx
                     cmp eax, 256
                     jb ok2
                     mov eax, 255
                  ok2:
                     mov ax, [esi+eax*2]
                     mov [edi], ax
                     add edx, 2
                     add edi, 2
                     dec ecx
                     jnz loop_x2
                     mov surf, edx
                     mov line, edi

                     pop ecx
                  }
#else
                  for( ; x--; line.bp += 2)
                     *line.wp = tab.wp[Max(0, Min(255, 128 + *surf++))];
#endif
                  break;

               case 3:
                  for( ; x--; line.bp += 3)
                     *line.bp3 = tab.bp3[Max(0, Min(255, 128 + *surf++))];
                  break;

               case 4:
#if defined _MSC_VER && 1
                                       //optimize - most common case
                  __asm{
                     push ecx
                                       //edx = surf
                                       //esi = tab
                                       //edi = line
                     mov edx, surf
                     mov esi, tab
                     mov edi, line
                     mov ecx, x
                  loop_x4:
                     movsx eax, word ptr[edx]
                     xor ebx, ebx
                     add eax, 128
                     sets bl
                     dec ebx
                     and eax, ebx
                     cmp eax, 256
                     jb ok4
                     mov eax, 255
                  ok4:
                     mov eax, [esi+eax*4]
                     mov [edi], eax
                     add edx, 2
                     add edi, 4
                     dec ecx
                     jnz loop_x4
                     mov surf, edx
                     mov line, edi

                     pop ecx
                  }
#else
                  for( ; x--; line.bp += 4)
                     *line.dwp = tab.dwp[Max(0, Min(255, 128 + *surf++))];
#endif
                  break;
               }
            }
         }
      }
      d3d_txt->UnlockRect(0);
   }
   if(size_x*size_y>MAX_STACK_ALLOC)
      delete[] contents;

   //if(!(I3D_texture::drv->GetCaps()->Caps2&D3DCAPS2_DYNAMICTEXTURES))
   if(sysmem_image)
      SetDirty();
}

//----------------------------
//----------------------------
                              //table of all procedural implementations
static struct S_proc_class_table{
   I3D_PROCEDURAL_ID pid;
   const char *class_name;
} proc_class_table[] = {
   PROCID_MORPH_3D,     "MORPH_3D",
   PROCID_PARTICLE,     "PARTICLE",
   PROCID_TEXTURE,      "TEXTURE",
};

//----------------------------
//----------------------------

I3D_RESULT I3D_driver::GetProcedural(const char *filename, PI3D_LOAD_CB_PROC cb_proc, void *cb_context,
   PI3D_procedural_base *pb_ret, dword init_data) const{

                              //search in cache
   for(int i=procedural_cache.size(); i--; ){
      PI3D_procedural_base pb = procedural_cache[i];
      if(pb->GetName().Match(filename) && pb->GetInitData()==init_data){
         pb->AddRef();
         *pb_ret = pb;
         return I3D_OK;
      }
   }
   *pb_ret = NULL;
                              //not found in cache, create new
   
                              //open file and create procedural from there
   C_cache ck;
   for(i=dirs[I3DDIR_PROCEDURALS].size(); i--; ){
      if(ck.open(C_fstr("%s\\%s.txt", (const char*)dirs[I3DDIR_PROCEDURALS][i], filename), CACHE_READ))
         break;
   }
   if(i==-1){
      if(cb_proc)
         cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Cannot create procedural '%s': file not found.", filename),
            1, cb_context);
      return I3DERR_NOFILE;
   }

   PI3D_procedural_base pb = NULL;

#define SKIP_WS while(*cp && isspace(*cp)) ++cp;

   I3D_RESULT ir = I3DERR_INVALIDPARAMS;
                              //read procedural declaration
   while(!ck.eof()){
      char line[256];
      ck.getline(line, sizeof(line));
      const char *cp = line;
                              //skip leading space
      SKIP_WS;
                              //ignore comments
      if(*cp == ';' || *cp==0) continue;
                              //read keyword
      if(strncmp(cp, "Class", 5))
         continue;
      cp += 5;
      SKIP_WS;
      char class_name[128];
      if(sscanf(cp, "%128s", class_name) == 1){
                           //create specified procedural
         static struct S_proc_class_table{
            I3D_PROCEDURAL_ID pid;
            const char *class_name;
         } proc_class_table[] = {
            PROCID_MORPH_3D,     "MORPH_3D",
            PROCID_PARTICLE,     "PARTICLE",
            PROCID_TEXTURE,      "TEXTURE",
         };
         for(int i=sizeof(proc_class_table)/sizeof(S_proc_class_table); i--; ){
            if(!strcmp(class_name, proc_class_table[i].class_name))
               break;
         }
         if(i==-1){
            cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Procedural '%s': unknown class '%s'.", filename, class_name),
               1, cb_context);
            ir = I3DERR_INVALIDPARAMS;
            break;
         }
         I3D_PROCEDURAL_ID pid = proc_class_table[i].pid;
         switch(pid){
         case PROCID_MORPH_3D: pb = new I3D_procedural_morph_imp(this, filename, init_data); break;
         case PROCID_PARTICLE: pb = new I3D_procedural_particle_imp(this, filename, init_data); break;
         case PROCID_TEXTURE: pb = new I3D_procedural_texture(this, filename, init_data); break;
         default: assert(0);
         }
         /*
         if(pid==PROCID_TEXTURE)
            ir = pb->Initialize(ck, ct, cb_proc, cb_context);
         else
         */
            ir = pb->Initialize(ck, cb_proc, cb_context);
         if(I3D_FAIL(ir)){
            pb->Release();
            pb = NULL;
         }
         break;
      }
   }
   ck.close();
   if(I3D_FAIL(ir)){
      const char *err = NULL;
      switch(ir){
      case I3DERR_NOFILE: err = "file not found"; break;
      case I3DERR_INVALIDPARAMS: err = "invalid file"; break;
      }
      if(err){
         if(cb_proc)
            cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Procedural '%s': creation failed - %s.", filename, err),
            1, cb_context);
      }
   }
   if(pb){
      procedural_cache.push_back(pb);
      *pb_ret = pb;
   }
   return ir;
}

//----------------------------
//----------------------------

