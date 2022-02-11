/*--------------------------------------------------------
   Copyright (c) 1999 - 2001 Lonely Cat Games
   All rights reserved.

   File: Particle.cpp
   Content: Particle frame.
   Author: Michal Bacik
--------------------------------------------------------*/

#include "all.h"
#include "particle.h"
#include "camera.h"
#include "scene.h"
#include "mesh.h"


#define DATABASE_VERSION 0x0001

//----------------------------


                              //when too big time comes to Tick, slice it to smaller pieces
                              //+ nicer animation at lower frame rates
                              //- slows down
//#define ALLOW_SLICE_TIME

#define MAX_TICK_COUNT 500

//----------------------------

class I3D_particle_imp: public I3D_particle{

#define PRTF_DO_EMIT_BASE     1  //schedule to emit base elements in next Tick
#define PRTF_BASE_DIR_WORLD   2  //base dir in world coordinates
#define PRTF_DEST_DIR_WORLD   4  //dest dir in world coordinates
#define PRTF_OPTIMIZED        8  //ticked only when bounding box is visible

   enum E_EMIT_MODE{
      EMIT_SPHERE,
      EMIT_CIRCLE_XZ,
      //EMIT_XZ,
      //EMIT_YZ,
      EMIT_MESH,
   };

   mutable dword prt_flags;

   struct S_elem{
                              //life:
      int life_cnt;
      float r_life_time;
                              //current:
      float scale;
      float base_opacity, opacity;
      float rotation_angle;
                              //direction:
      S_vector base_dir, dest_dir;
      float r_affect_time;
      int affect_cnt;
      float scale_dir;
      float base_scale;
      float rotation_speed;   //<0 ... left, 0 ... no, >0 ... right
      S_matrix matrix;
      bool operator <(const S_elem &e){ return (dword)this < (dword)&e; }
      bool operator ==(const S_elem &e){ return (dword)this == (dword)&e; }
      bool operator !=(const S_elem &e){ return (dword)this != (dword)&e; }
   };
                              //keep elems in C_vector (optimized storage - growing as necessary, not shrinking)
   C_vector<S_elem> elems;
   C_smart_ptr<I3D_material> mat;

                              //particle param block
   struct S_data{
      E_EMIT_MODE emit_mode;
      int out_time[2];
      int counter;            //how many elements we'll produce
      int life_len[2];
      float element_scale;
      float scale_range[2];
      float init_pos_disp;
      float init_dir_disp;
      float affect_dir_dips;
      int affect_time[2];
      float opt_range[2];
      float opacity;
      S_vector init_dir;
      S_vector dest_dir;
      I3D_BLENDMODE blend_mode;
      float rotation_speed_min;
      float rotation_speed_max;
      bool emit_base;
      //float aspect_ratio;
   } data;
                              //run-time
   int out_time_count;
   int count_down;            //how many elements we still have to produce (-1 = stopped)

//----------------------------
                              //mesh for mesh-based sources
   class C_source_mesh: public C_unknown{
      C_source_mesh(const C_source_mesh&);
      void operator =(const C_source_mesh&);
   public:
      typedef map<float, dword> t_size_map;
      t_size_map size_map;

      C_source_mesh(){}

      C_buffer<S_vector> verts;
      C_buffer<I3D_triface> faces;

//----------------------------

      void CreateSizeMap(){

         C_buffer<float> face_sizes(faces.size());
         float total_size = 0.0f;
         for(dword i=faces.size(); i--; ){
            const I3D_triface &fc = faces[i];
            float sz = fc.ComputeSurfaceArea(verts.begin(), sizeof(S_vector));
            face_sizes[i] = sz;
            total_size += sz;
         }
         if(!IsMrgZeroLess(total_size)){
                              //make sum of sizes to be 1.0
            float r_size = 1.0f / total_size;
            for(dword i=faces.size(); i--; )
               face_sizes[i] *= r_size;
            float base = 0.0f;
            for(i=faces.size(); i--; ){
               base += face_sizes[i];
               size_map[base] = i;
            }
         }
      }
   };
   C_smart_ptr<C_source_mesh> source_mesh;

//----------------------------

   virtual bool SaveCachedInfo(C_cache *ck, const C_vector<C_smart_ptr<I3D_material> > &mats) const{

      if(!mat)
         return false;
      ck->WriteWord(DATABASE_VERSION);
                              //write material index
                           //write material index
      for(int j=mats.size(); j--; ){
         if(mats[j]==mat){
            ck->WriteDword(j);
            break;
         }
      }
      if(j==-1){
         assert(0);
         return false;
      }
                              //write mesh, if any
      if(source_mesh){
         dword numv = source_mesh->verts.size();
         dword numf = source_mesh->faces.size();
         ck->WriteWord(word(numv));
         ck->write(source_mesh->verts.begin(), numv*sizeof(S_vector));
         ck->WriteWord(word(numf));
         ck->write(source_mesh->faces.begin(), numf*sizeof(I3D_triface));
      }else{
         ck->WriteWord(0);
      }
      return true;
   }

//----------------------------

   virtual bool LoadCachedInfo(C_cache *ck, class C_loader &lc, C_vector<C_smart_ptr<I3D_material> > &mats){

      if(ck->filesize()<2)
         return false;
      dword ver = ck->ReadWord();
      if(ver!=DATABASE_VERSION)
         return false;
      dword mi = ck->ReadDword();
      mat = mats[mi];

      source_mesh = NULL;
      dword numv = ck->ReadWord();
      if(numv){
         source_mesh = new C_source_mesh;
         source_mesh->Release();

         source_mesh->verts.assign(numv);
         ck->read(source_mesh->verts.begin(), numv*sizeof(S_vector));

         dword numf = ck->ReadWord();
         source_mesh->faces.assign(numf);
         ck->read(source_mesh->faces.begin(), numf*sizeof(I3D_triface));

         source_mesh->CreateSizeMap();
      }
      return true;
   }

//----------------------------

   void Reset();

   void TickInternal(int time);

//----------------------------
// Get time delta since last render. The returned value is not grater than some optimal value,
// so that we don't waste time on updating unnecessary elements.
   int GetTickTime() const{
      int count = drv->GetRenderTime();
      int delta = count - last_render_time;
      return Max(0, Min(MAX_TICK_COUNT, delta));
   }
public:
   I3D_particle_imp(PI3D_driver d):
      I3D_particle(d),
      prt_flags(0)
   {
      visual_type = I3D_VISUAL_PARTICLE;
      data.emit_mode = EMIT_SPHERE;
      Reset();
   }

   virtual void Tick(int time){
      if(!(prt_flags&PRTF_OPTIMIZED) || (prt_flags&PRTF_DO_EMIT_BASE))
         TickInternal(time);
   }
   I3D_particle_imp &operator =(const I3D_particle_imp&);

   virtual void PropagateDirty(){
      I3D_frame::PropagateDirty();
      vis_flags &= ~VISF_BOUNDS_VALID;
      frm_flags &= ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID);
   }
public:

   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(SetProperty)(dword index, dword param);
   I3DMETHOD_(dword,GetProperty)(dword index) const;
                              //visual methods
   virtual bool ComputeBounds();
   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
//----------------------------

   virtual void SetMeshInternal(PI3D_mesh_base mesh){

      if(mesh){
         SetMaterial(mesh->GetFGroups1()->mat);
         source_mesh = new C_source_mesh;
         source_mesh->Release();
         source_mesh->verts.assign(mesh->NumVertices());
         source_mesh->faces.assign(mesh->NumFaces());
         const S_vector *vp = mesh->LockVertices();
         dword vstride = mesh->GetSizeOfVertex();
         dword i;
         for(i=0; i<source_mesh->verts.size(); i++){
            source_mesh->verts[i] = *vp;
            vp = (S_vector*)(((byte*)vp) + vstride);
         }
         source_mesh->faces.assign(mesh->NumFaces());
         mesh->GetFaces(source_mesh->faces.begin());
         /*
         const I3D_triface *fp = mesh->LockFaces();
         for(i=source_mesh->faces.size(); i--; )
            source_mesh->faces[i] = fp[i];
         mesh->UnlockFaces();
            */
         source_mesh->CreateSizeMap();

         mesh->UnlockVertices();
      }else{
         source_mesh = NULL;
         PI3D_material mat = drv->CreateMaterial();
         SetMaterial(mat);
         mat->Release();
      }
   }
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD_(I3D_RESULT,SetMaterial)(PI3D_material mat1){
      mat = mat1;
      if(!mat)
         SetOn(false);
      return I3D_OK;
   }
   I3DMETHOD_(PI3D_material,GetMaterial()){ return mat; }
   I3DMETHOD_(CPI3D_material,GetMaterial()) const{ return mat; }
};

//----------------------------

void I3D_particle_imp::Reset(){

   out_time_count = 1;
   elems.clear();
                              //setup default particle
   SetProperty(I3DPROP_PRTC_I_OUT_TIME, 100);
   SetProperty(I3DPROP_PRTC_I_OUTT_THRESH, 60);
   SetProperty(I3DPROP_PRTC_I_LIFE_LEN, 1800);
   SetProperty(I3DPROP_PRTC_I_LIFEL_THRESH, 600);
   SetProperty(I3DPROP_PRTC_F_ELEMENT_SCALE, I3DFloatAsInt(1.0f));
   SetProperty(I3DPROP_PRTC_F_SCALE_GROW_MIN, I3DFloatAsInt(-.25f));
   SetProperty(I3DPROP_PRTC_F_SCALE_GROW_MAX, I3DFloatAsInt(.6f));
   SetProperty(I3DPROP_PRTC_F_INIT_POS_THRESH, I3DFloatAsInt(.2f));
   SetProperty(I3DPROP_PRTC_F_INIT_DIR_THRESH, I3DFloatAsInt(1.0f));
   SetProperty(I3DPROP_PRTC_I_AFFECT_TIME, 400);
   SetProperty(I3DPROP_PRTC_I_AFFECTT_THRESH, 100);
   SetProperty(I3DPROP_PRTC_F_AFFECT_DIR_THRESH, I3DFloatAsInt(1.0f));
   SetProperty(I3DPROP_PRTC_F_OPACITY_IN, I3DFloatAsInt(.1f));
   SetProperty(I3DPROP_PRTC_F_OPACITY_OUT, I3DFloatAsInt(.1f));
   SetProperty(I3DPROP_PRTC_F_OPACITY, I3DFloatAsInt(1.0f));
   SetProperty(I3DPROP_PRTC_F_BASE_DIR_X, I3DFloatAsInt(0.0f));
   SetProperty(I3DPROP_PRTC_F_BASE_DIR_Y, I3DFloatAsInt(1.0f));
   SetProperty(I3DPROP_PRTC_F_BASE_DIR_Z, I3DFloatAsInt(0.0f));
   SetProperty(I3DPROP_PRTC_F_DEST_DIR_X, I3DFloatAsInt(.2f));
   SetProperty(I3DPROP_PRTC_F_DEST_DIR_Y, I3DFloatAsInt(.8f));
   SetProperty(I3DPROP_PRTC_F_DEST_DIR_Z, I3DFloatAsInt(0.0f));
   SetProperty(I3DPROP_PRTC_B_BASE_DIR_WORLD, false);
   SetProperty(I3DPROP_PRTC_B_DEST_DIR_WORLD, false);
   SetProperty(I3DPROP_PRTC_F_ROT_SPEED_MIN, 0);
   SetProperty(I3DPROP_PRTC_F_ROT_SPEED_MAX, 0);
   SetProperty(I3DPROP_PRTC_E_DRAW_MODE, I3DBLEND_ADD);
   //SetProperty(I3DPROP_PRTC_DRAW_MODE, I3DBLEND_ALPHABLEND);
   //SetProperty(I3DPROP_PRTC_DRAW_MODE, I3DBLEND_OPAQUE);
   SetProperty(I3DPROP_PRTC_I_SETCOUNT, 0);
   SetProperty(I3DPROP_PRTC_E_MODE, 0);
   SetProperty(I3DPROP_PRTC_B_EMIT_BASE, true);
   SetProperty(I3DPROP_PRTC_B_OPTIMIZED, true);
   //SetProperty(I3DPROP_PRTC_ASPECT_RATIO, I3DFloatAsInt(1.0f));
}

//----------------------------

I3D_RESULT I3D_particle_imp::Duplicate(CPI3D_frame frm){

   if(frm==this)
      return I3D_OK;
   if(frm->GetType1()!=FRAME_VISUAL)
      return I3D_frame::Duplicate(frm);
   CPI3D_visual vis = I3DCAST_CVISUAL(frm);
   switch(vis->GetVisualType1()){
   case I3D_VISUAL_PARTICLE:
      {
         I3D_particle_imp *pp = (I3D_particle_imp*)vis;
         memcpy(&data, &pp->data, sizeof(data));
         count_down = pp->count_down;
         out_time_count = pp->out_time_count;
         source_mesh = pp->source_mesh;

         elems = pp->elems;
         prt_flags = pp->prt_flags;
         SetMaterial(pp->GetMaterial());
      }
      break;
   default:
      if(vis->GetMesh())
         SetMeshInternal(const_cast<PI3D_visual>(vis)->GetMesh());
      else
      if(vis->GetMaterial())
         SetMaterial(const_cast<PI3D_visual>(vis)->GetMaterial());
   }
   return I3D_visual::Duplicate(vis);
}

//----------------------------

I3D_RESULT I3D_particle_imp::SetProperty(dword index, dword value){

   switch(index){
   case I3DPROP_PRTC_I_OUT_TIME:         data.out_time[0]    = value; break;
   case I3DPROP_PRTC_I_OUTT_THRESH:      data.out_time[1]    = value; break;
   case I3DPROP_PRTC_I_LIFE_LEN:         data.life_len[0]    = value; break;
   case I3DPROP_PRTC_I_LIFEL_THRESH:     data.life_len[1]    = value; break;
   case I3DPROP_PRTC_I_AFFECT_TIME:      data.affect_time[0] = Max(1, (int)value); break;
   case I3DPROP_PRTC_I_AFFECTT_THRESH:   data.affect_time[1] = value; break;
   case I3DPROP_PRTC_E_DRAW_MODE:        data.blend_mode     = (I3D_BLENDMODE)value; break;
   case I3DPROP_PRTC_I_SETCOUNT:         count_down = data.counter = value; break;

   case I3DPROP_PRTC_F_ELEMENT_SCALE:    data.element_scale  = *(float*)&value; break;
   case I3DPROP_PRTC_F_SCALE_GROW_MIN:   data.scale_range[0] = *(float*)&value; break;
   case I3DPROP_PRTC_F_SCALE_GROW_MAX:   data.scale_range[1] = *(float*)&value; break;
   case I3DPROP_PRTC_F_INIT_POS_THRESH:  data.init_pos_disp  = *(float*)&value; break;
   case I3DPROP_PRTC_F_INIT_DIR_THRESH:  data.init_dir_disp  = *(float*)&value; break;
   case I3DPROP_PRTC_F_AFFECT_DIR_THRESH:data.affect_dir_dips= *(float*)&value; break;
   case I3DPROP_PRTC_F_OPACITY_IN:       data.opt_range[0]   = *(float*)&value; break;
   case I3DPROP_PRTC_F_OPACITY_OUT:      data.opt_range[1]   = *(float*)&value; break;
   case I3DPROP_PRTC_F_OPACITY:          data.opacity        = *(float*)&value; break;
   case I3DPROP_PRTC_F_BASE_DIR_X:       data.init_dir.x     = *(float*)&value; break;
   case I3DPROP_PRTC_F_BASE_DIR_Y:       data.init_dir.y     = *(float*)&value; break;
   case I3DPROP_PRTC_F_BASE_DIR_Z:       data.init_dir.z     = *(float*)&value; break;
   case I3DPROP_PRTC_F_DEST_DIR_X:       data.dest_dir.x     = *(float*)&value; break;
   case I3DPROP_PRTC_F_DEST_DIR_Y:       data.dest_dir.y     = *(float*)&value; break;
   case I3DPROP_PRTC_F_DEST_DIR_Z:       data.dest_dir.z     = *(float*)&value; break;
   case I3DPROP_PRTC_B_BASE_DIR_WORLD:
      prt_flags &= ~PRTF_BASE_DIR_WORLD;
      if(value) prt_flags |= PRTF_BASE_DIR_WORLD;
      break;
   case I3DPROP_PRTC_B_DEST_DIR_WORLD:
      prt_flags &= ~PRTF_DEST_DIR_WORLD;
      if(value) prt_flags |= PRTF_DEST_DIR_WORLD;
      break;
   case I3DPROP_PRTC_F_ROT_SPEED_MIN: data.rotation_speed_min = I3DIntAsFloat(value); break;
   case I3DPROP_PRTC_F_ROT_SPEED_MAX: data.rotation_speed_max = I3DIntAsFloat(value); break;

   case I3DPROP_PRTC_B_EMIT_BASE:
      {
         data.emit_base = value;
         prt_flags &= ~PRTF_DO_EMIT_BASE;
         if(data.emit_base)
            prt_flags |= PRTF_DO_EMIT_BASE;
      }
      break;
   case I3DPROP_PRTC_E_MODE:
      data.emit_mode = (E_EMIT_MODE)value;
      break;
   case I3DPROP_PRTC_B_OPTIMIZED:
      prt_flags &= ~PRTF_OPTIMIZED;
      if(value)
         prt_flags |= PRTF_OPTIMIZED;
      break;
   case I3DPROP_PRTC_MATERIAL:
      SetMaterial((PI3D_material)value);
      break;
   default:
      assert(0);
      return I3DERR_INVALIDPARAMS;
   }
   return I3D_OK;
}

//----------------------------

dword I3D_particle_imp::GetProperty(dword index) const{

   switch(index){

   case I3DPROP_PRTC_I_OUT_TIME:         return data.out_time[0];
   case I3DPROP_PRTC_I_OUTT_THRESH:      return data.out_time[1];
   case I3DPROP_PRTC_I_LIFE_LEN:         return data.life_len[0];
   case I3DPROP_PRTC_I_LIFEL_THRESH:     return data.life_len[1];
   case I3DPROP_PRTC_I_AFFECT_TIME:      return data.affect_time[0];
   case I3DPROP_PRTC_I_AFFECTT_THRESH:   return data.affect_time[1];
   case I3DPROP_PRTC_E_DRAW_MODE:        return data.blend_mode;
   case I3DPROP_PRTC_I_SETCOUNT:         return data.counter;

   case I3DPROP_PRTC_F_ELEMENT_SCALE:    return I3DFloatAsInt(data.element_scale);
   case I3DPROP_PRTC_F_SCALE_GROW_MIN:   return I3DFloatAsInt(data.scale_range[0]);
   case I3DPROP_PRTC_F_SCALE_GROW_MAX:   return I3DFloatAsInt(data.scale_range[1]);
   case I3DPROP_PRTC_F_INIT_POS_THRESH:  return I3DFloatAsInt(data.init_pos_disp);
   case I3DPROP_PRTC_F_INIT_DIR_THRESH:  return I3DFloatAsInt(data.init_dir_disp);
   case I3DPROP_PRTC_F_AFFECT_DIR_THRESH:return I3DFloatAsInt(data.affect_dir_dips);
   case I3DPROP_PRTC_F_OPACITY_IN:       return I3DFloatAsInt(data.opt_range[0]);
   case I3DPROP_PRTC_F_OPACITY_OUT:      return I3DFloatAsInt(data.opt_range[1]);
   case I3DPROP_PRTC_F_OPACITY:          return I3DFloatAsInt(data.opacity);
   case I3DPROP_PRTC_F_BASE_DIR_X:       return I3DFloatAsInt(data.init_dir.x);
   case I3DPROP_PRTC_F_BASE_DIR_Y:       return I3DFloatAsInt(data.init_dir.y);
   case I3DPROP_PRTC_F_BASE_DIR_Z:       return I3DFloatAsInt(data.init_dir.z);
   case I3DPROP_PRTC_F_DEST_DIR_X:       return I3DFloatAsInt(data.dest_dir.x);
   case I3DPROP_PRTC_F_DEST_DIR_Y:       return I3DFloatAsInt(data.dest_dir.y);
   case I3DPROP_PRTC_F_DEST_DIR_Z:       return I3DFloatAsInt(data.dest_dir.z);
   case I3DPROP_PRTC_B_BASE_DIR_WORLD:   return bool(prt_flags&PRTF_BASE_DIR_WORLD);
   case I3DPROP_PRTC_B_DEST_DIR_WORLD:   return bool(prt_flags&PRTF_DEST_DIR_WORLD);
   case I3DPROP_PRTC_F_ROT_SPEED_MIN:    return I3DFloatAsInt(data.rotation_speed_min);
   case I3DPROP_PRTC_F_ROT_SPEED_MAX:    return I3DFloatAsInt(data.rotation_speed_max);

   case I3DPROP_PRTC_I_NUMELEMENTS:      return elems.size();
   case I3DPROP_PRTC_B_EMIT_BASE:        return data.emit_base;
   case I3DPROP_PRTC_E_MODE:             return data.emit_mode;
   case I3DPROP_PRTC_B_OPTIMIZED:        return bool(prt_flags&PRTF_OPTIMIZED);
   case I3DPROP_PRTC_MATERIAL:         return (dword)GetMaterial();
   default:
      assert(0);
   }
   return 0;
}

//----------------------------

void I3D_particle_imp::TickInternal(int time){

   if(prt_flags&PRTF_DO_EMIT_BASE){
      prt_flags &= ~PRTF_DO_EMIT_BASE;
      //int time = 2000;
      int time = 0;
      if(!time) time = data.out_time[0] + data.out_time[1] + data.life_len[0] + data.life_len[1];
      int tick_time = Max(time/5, data.out_time[0]);
      for(int i=0; i<time; i+=tick_time){
         TickInternal(tick_time);
      }
   }


   if(!IsOn1())
      return;

   PROFILE(drv, PROF_PARTICLE);

#ifdef ALLOW_SLICE_TIME
                              //slice time
   while(time > Max(100, data.out_time[0]*2)){
      int t1 = Max(50, data.out_time[0]);
      TickInternal(t1);
      time -= t1;
   }
#endif//ALLOW_SLICE_TIME

   const S_matrix &mat = GetMatrix();
   if(count_down!=-1){
                              //creation of new elements enabled
                              //check if it's right time to do so
      out_time_count -= time;
      while(out_time_count <= 0){
                              //comp delay to emit new element
         int add_time = data.out_time[0];
         if(data.out_time[1])
            add_time += S_int_random(data.out_time[1]);
         if(!add_time)
            break; //problem - neverending loop detected - invalid params?!
         out_time_count += add_time;

                              //create element info & file
         elems.push_back(S_elem());
         S_elem &si = elems.back();
         si.matrix.Identity();
                              //get inherited scale
         si.base_scale = si.scale = data.element_scale * mat(0).Magnitude();
                              //setup life length
         si.life_cnt = data.life_len[0];
         if(data.life_len[1])
            si.life_cnt += S_int_random(data.life_len[1]);
                              //avoid dividing by zero
         if(!si.life_cnt)
            si.life_cnt = 1;
         si.r_life_time = 1.0f / (float)si.life_cnt;

         si.base_opacity = data.opacity;

                              //setup direction
         {
            S_vector &dd = si.dest_dir;
            dd = data.init_dir;
            float idd = data.init_dir_disp;
            dd.x += S_float_random() * idd - idd * .5f;
            dd.y += S_float_random() * idd - idd * .5f;
            dd.z += S_float_random() * idd - idd * .5f;

            //if(data.emit_mode>=EMIT_XY && data.emit_mode<=EMIT_YZ)
               //dd[mode_off_axis[data.emit_mode-EMIT_XY]] = 0.0f;
            //if(data.emit_mode==EMIT_CIRCLE_XZ) dd.y = 0.0f;
            if(!(prt_flags&PRTF_BASE_DIR_WORLD))
               dd %= mat;
            //else i_dir = init_dir * si.base_scale;
         }

         S_vector &init_pos = si.matrix(3);
         //S_vector &init_pos = si.pos;
         const float ipd = data.init_pos_disp;
         S_vector disp(
            ipd/2.0f - S_float_random() * ipd,
            ipd/2.0f - S_float_random() * ipd,
            ipd/2.0f - S_float_random() * ipd);
         //if(data.emit_mode>=EMIT_XY && data.emit_mode<=EMIT_YZ)
            //disp[mode_off_axis[data.emit_mode-EMIT_XY]] = 0.0f;
         if(data.emit_mode==EMIT_CIRCLE_XZ) disp.y = 0.0f;
         init_pos = disp;

         if(data.emit_mode==EMIT_MESH && source_mesh){
                              //get random face we'll generate position on
            float rpos = S_float_random();
            C_source_mesh::t_size_map::const_iterator it;
            it = source_mesh->size_map.lower_bound(rpos);
            if(it==source_mesh->size_map.end())
               ++it;
            dword face_index = (*it).second;
            assert(face_index < source_mesh->faces.size());
            //drv->PRINT((int)face_index);
            const I3D_triface &fc = source_mesh->faces[face_index];
                              //get random position on that face
            const S_vector *verts = source_mesh->verts.begin();
            /*
            drv->DebugLine(verts[fc[0]], verts[fc[1]], 2);
            drv->DebugLine(verts[fc[1]], verts[fc[2]], 2);
            drv->DebugLine(verts[fc[2]], verts[fc[0]], 2);
            */
            dword edge0 = S_int_random(3);
            dword edge1;
            do{
               edge1 = S_int_random(3);
            }while(edge1==edge0);
            float re0 = S_float_random();
            float re1 = S_float_random();
            float re2 = S_float_random();
            S_vector p0 = verts[fc[edge0]] + (verts[fc[(edge0+1)%3]] - verts[fc[edge0]]) * re0;
            S_vector p1 = verts[fc[edge1]] + (verts[fc[(edge1+1)%3]] - verts[fc[edge1]]) * re1;
            init_pos += p0 + (p1 - p0) * re2;
         }else{
         }
         init_pos *= mat;
         //drv->DebugLine(init_pos, init_pos+si.dest_dir, 2);
                              //setup scale
         si.scale_dir = data.scale_range[0] + S_float_random() * (data.scale_range[1] - data.scale_range[0]);
         si.affect_cnt = 0;

                              //setup rotation
         si.rotation_speed = data.rotation_speed_min + S_float_random() * (data.rotation_speed_max - data.rotation_speed_min);
         if(!IsAbsMrgZero(si.rotation_speed)){
            si.rotation_angle = S_float_random() * (PI*2.0f);
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
   S_elem *elem_ptr = &elems.front();
   for(dword i=elems.size(); i--; ){
      S_elem &si = elem_ptr[i];
                              //lifecounf
      if((si.life_cnt -= time) <= 0){
                              //dead
         elems[i] = elems.back(); elems.pop_back();
         elem_ptr = &elems.front();
         continue;
      }
#if defined USE_PREFETCH && 0
      if(it_next!=it_end){
         Prefetch64Bytes(&(*it_next));
      }
#endif
                              //compute opacity
      float ft = 1.0f - ((float)si.life_cnt * si.r_life_time);
      si.opacity = ft<data.opt_range[0] ? ft/data.opt_range[0] :
         ft<=data.opt_range[1] ? 1.0f :
         1.0f - (ft-data.opt_range[1])/(1.0f-data.opt_range[1]);
      si.opacity *= si.base_opacity;
                              //compute affect direction
      if((si.affect_cnt -= time) <= 0){
                              //don't accept less than 1, we divide by this later
         int affect_time = data.affect_time[0];
         if(data.affect_time[1])
            affect_time += S_int_random(data.affect_time[1]);
         assert(affect_time);
         si.affect_cnt = affect_time;
         si.r_affect_time = 1.0f / (float)affect_time;

                              //compute new dest dir
         {
            S_vector &dd = si.dest_dir;
            si.base_dir = dd;
            float mag;
            float dot = dd.Dot(dd);
            if(dot <= (MRG_ZERO*MRG_ZERO)) mag = 0.0f;
            else mag = I3DSqrt(dot);

            S_vector disp1 = data.dest_dir;
            //if(data.emit_mode>=EMIT_XY && data.emit_mode<=EMIT_YZ)
               //disp1[mode_off_axis[data.emit_mode-EMIT_XY]] = 0.0f;
            //if(data.emit_mode==EMIT_CIRCLE_XZ) disp1.y = 0.0f;
            if(!(prt_flags&PRTF_DEST_DIR_WORLD))
               disp1 %= mat;
            dd += disp1;

                              //keep similar size of direction
            dot = dd.Dot(dd);
            float new_mag;
            if(dot <= (MRG_ZERO*MRG_ZERO))
               new_mag = 0.0f;
            else{
               new_mag = I3DSqrt(dot);
               dd *= mag / new_mag;
            }

            float add = data.affect_dir_dips;
            S_vector disp(
               add*.5f - S_float_random() * add,
               add*.5f - S_float_random() * add,
               add*.5f - S_float_random() * add);
            //if(data.emit_mode>=EMIT_XY && data.emit_mode<=EMIT_YZ)
               //disp[mode_off_axis[data.emit_mode-EMIT_XY]] = 0.0f;
            //if(data.emit_mode==EMIT_CIRCLE_XZ) disp.y = 0.0f;
            if(!(prt_flags&PRTF_DEST_DIR_WORLD))
               disp %= mat;

            dd += disp;
         }
      }
                              //compute new position
      float fd = (float)si.affect_cnt * si.r_affect_time;
      S_vector dir = si.base_dir * fd + si.dest_dir * (1.0f - fd);
      si.matrix(3) += dir * tsec;
      //si.pos += dir * tsec;
                              //adjust scale
      si.scale *= (1.0f + si.scale_dir * tsec);

                              //compute new rotation angle
      if(I3DFloatAsInt(si.rotation_speed) != 0){
         si.rotation_angle += si.rotation_speed * tsec;
      }
      //it = it_next;
   }
   ComputeBounds();
}

//----------------------------

const float PARTICLE_BSPHERE_RADIUS = .8660255f;

const float PARTICLE_EXPAND_RATIO = .3f;  //ratio of expanding cached bbox on both min & max sides
//----------------------------

bool I3D_particle_imp::ComputeBounds(){


                              //compute real bounding box
   I3D_bbox bbox;
   bbox.Invalidate();

   if(elems.size()){

      const S_matrix &m_inv = GetInvMatrix1();
      const float m_inv_scale = m_inv(2).Magnitude();
      float radius = PARTICLE_BSPHERE_RADIUS * m_inv_scale;

      const S_elem *elem_ptr = &elems.front();
      for(dword i=elems.size(); i--; ){
         const S_elem &si = elem_ptr[i];

         const S_vector pos = si.matrix(3) * m_inv;
         float e_radius = radius * si.scale;
                              //expand bounding box
         bbox.min.x = Min(bbox.min.x, pos.x-e_radius);
         bbox.min.y = Min(bbox.min.y, pos.y-e_radius);
         bbox.min.z = Min(bbox.min.z, pos.z-e_radius);

         bbox.max.x = Max(bbox.max.x, pos.x+e_radius);
         bbox.max.y = Max(bbox.max.y, pos.y+e_radius);
         bbox.max.z = Max(bbox.max.z, pos.z+e_radius);
      }
                              //for optimized particles, make bbox always include pivot point
      if(prt_flags&PRTF_OPTIMIZED){
         bbox.min.Minimal(S_vector(0, 0, 0));
         bbox.max.Maximal(S_vector(0, 0, 0));
      }
   }
                              //compare it with cached version
   if(vis_flags&VISF_BOUNDS_VALID){
      const I3D_bbox &vbb = bound.bound_local.bbox;
      if(vbb.IsValid()){
         bool is_greater = (bbox.min.x<vbb.min.x || bbox.min.y<vbb.min.y || bbox.min.z<vbb.min.z ||
            bbox.max.x>vbb.max.x || bbox.max.y>vbb.max.y || bbox.max.z>vbb.max.z);
                              //if new bbox is greater than cached one, invalidate
         if(is_greater){
            vis_flags &= ~VISF_BOUNDS_VALID;
         }else{
                              //check how smaller is the new one,
                              // if too small, invalidate too
            S_vector diag1 = vbb.max - vbb.min;
            if(diag1.x<.001f || diag1.y<.001f || diag1.z<.001f){
               vis_flags &= ~VISF_BOUNDS_VALID;
            }else{
               S_vector diag = bbox.max - bbox.min;
               float min_ratio = Min(diag.x/diag1.x, Min(diag.y/diag1.y, diag.z/diag1.z));
               if(min_ratio < (1.0f/(1.0f+PARTICLE_EXPAND_RATIO*4.0f))){
                              //new bbox significantly smaller, use it
                  vis_flags &= ~VISF_BOUNDS_VALID;
               }
            }
         }
      }else{
                              //cached bbox was not valid,
                              // use new if it is valid
         if(bbox.IsValid())
            vis_flags &= ~VISF_BOUNDS_VALID;
      }
   }

   if(!(vis_flags&VISF_BOUNDS_VALID)){
                              //expand our bbox by certain amount
      S_vector scaled_diag = (bbox.max - bbox.min) * PARTICLE_EXPAND_RATIO;
      bbox.min -= scaled_diag;
      bbox.max += scaled_diag;
                              //store into cached one
      bound.bound_local.bbox = bbox;
                              //expand real bbox by some value and store in real bbox
                              //invalidate hierarchy bounds
      frm_flags &= ~FRMFLAGS_HR_BOUND_VALID;
      {
         PI3D_frame frm1a = this;
         do{
            frm1a->SetFrameFlags(frm1a->GetFrameFlags() & ~(FRMFLAGS_HR_BOUND_VALID | FRMFLAGS_SM_BOUND_VALID));
            frm1a = frm1a->GetParent1();
         }while(frm1a);
      }
                              //compute bounding sphere
      if(bound.bound_local.bbox.IsValid()){
         S_vector bbox_diagonal = (bound.bound_local.bbox.max - bound.bound_local.bbox.min) * .5f;
         bound.bound_local.bsphere.pos = bound.bound_local.bbox.min + bbox_diagonal;
         bound.bound_local.bsphere.radius = bbox_diagonal.Magnitude();
      }else{
         bound.bound_local.bsphere.pos.Zero();
         bound.bound_local.bsphere.radius = 0.0f;
      }

      vis_flags |= VISF_BOUNDS_VALID;
      frm_flags &= ~FRMFLAGS_BSPHERE_TRANS_VALID;

      return true;
   }
   return true;
}

//----------------------------

void I3D_particle_imp::AddPrimitives(S_preprocess_context &pc){

                              //bug in texkill plane, do not project in mirror
   //if(drv->GetFlags2()&DRVF2_TEXCLIP_ON) return;
   //if(pc.mode==RV_SHADOW_CASTER) return;

   if(prt_flags&PRTF_OPTIMIZED){
      int time = GetTickTime();
      if(time)
         TickInternal(time);
   }
#ifndef GL
                              //don't allow in shadow casting
   if(pc.mode==RV_SHADOW_CASTER)
      return;
#endif
   if(!elems.size() || !mat)
      return;

   PI3D_camera cam = pc.scene->GetActiveCamera1();

   pc.prim_list.push_back(S_render_primitive(0, alpha, this, pc));
   S_render_primitive &p = pc.prim_list.back();
   p.blend_mode = data.blend_mode;

                              //compute sorting depth
   const I3D_bsphere &bsphere = bound.GetBoundSphereTrans(this, FRMFLAGS_BSPHERE_TRANS_VALID);
   const S_vector &w_pos = bsphere.pos;
   const S_matrix &m_view = cam->GetViewMatrix1();
   float depth =
         w_pos[0] * m_view(0, 2) +
         w_pos[1] * m_view(1, 2) +
         w_pos[2] * m_view(2, 2) +
                    m_view(3, 2);
   depth = Max(0.0f, depth-1.0f);

   dword sort_base = ((FloatToInt(depth * 100.0f))&PRIM_SORT_DIST_MASK) << PRIM_SORT_DIST_SHIFT;
   p.sort_value = PRIM_SORT_ALPHA_NOZWRITE | sort_base;
   ++pc.alpha_nozwrite;

   pc.scene->render_stats.vert_trans += elems.size() * 4;
   pc.scene->render_stats.triangle += elems.size() * 2;
}

//----------------------------
// Function copying 4x3 part of source matrix into destination transposed (leaving last row uninitialized).
static void CopyMatrixTransposed4x3(S_matrix &dst, const S_matrix &src){
   dst.m[0][0] = src.m[0][0];
   dst.m[0][1] = src.m[1][0];
   dst.m[0][2] = src.m[2][0];
   dst.m[0][3] = src.m[3][0];

   dst.m[1][0] = src.m[0][1];
   dst.m[1][1] = src.m[1][1];
   dst.m[1][2] = src.m[2][1];
   dst.m[1][3] = src.m[3][1];

   dst.m[2][0] = src.m[0][2];
   dst.m[2][1] = src.m[1][2];
   dst.m[2][2] = src.m[2][2];
   dst.m[2][3] = src.m[3][2];
}

//----------------------------
#ifndef GL
void I3D_particle_imp::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   drv->SetupBlend(data.blend_mode);
   drv->SetTexture1(0, mat->GetTexture1(MTI_DIFFUSE));

                              //debug mode - without textures
   if(!(drv->GetFlags2()&DRVF2_DRAWTEXTURES))
      drv->SetTexture1(0, NULL);

   drv->EnableNoCull(true);   //use no culling due to mirrors
   drv->SetupAlphaTest(false);

   drv->SetStreamSource(drv->vb_particle, sizeof(S_vertex_particle));
   drv->SetFVF(D3DFVF_XYZ);

   const S_vector &mat_diffuse = mat->GetDiffuse();
   float mat_alpha = mat->GetAlpha1() * (float)rp.alpha * R_255;

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

   HRESULT hr;

   drv->SetupTextureStage(0, D3DTOP_MODULATE);
   drv->DisableTextureStage(1);

   I3D_driver::S_vs_shader_entry_in se;
   se.AddFragment(VSF_TRANSFORM_PARTICLE);
   se.AddFragment(VSF_MAKE_RECT_UV);
   PrepareVertexShader(NULL, 1, se, rp, pc.mode, VSPREP_WORLD_SPACE);

                              //store transposed projection matrix
   drv->SetVSConstant(VSC_MAT_TRANSFORM_0, &rp.scene->GetViewProjHomMatTransposed(), 4);
   drv->SetIndices(drv->ib_particle);

   PI3D_camera cam = rp.scene->GetActiveCamera1();
   const S_matrix &m_camera = cam->GetMatrixDirect();
   //const S_matrix &mat = GetMatrixDirect();
   S_matrix m_rot;
   m_rot.Zero();
   m_rot(2, 2) = 1.0f;
   m_rot(3, 3) = 1.0f;

   S_matrix m_cache[MAX_PARTICLE_RECTANGLES];

   {
      S_matrix m_tmp;
      dword rect_i = 0;
      const S_elem *elem_ptr = &elems.front();
      for(dword i=elems.size(); i--; ++rect_i){
                              //flush rendering
         if(rect_i==MAX_PARTICLE_RECTANGLES){
            d3d_dev->SetVertexShaderConstantF(VSC_PARTICLE_MATRIX_BASE, (const float*)m_cache, MAX_PARTICLE_RECTANGLES*4);

            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, MAX_PARTICLE_RECTANGLES*4, 0, MAX_PARTICLE_RECTANGLES*2);
            CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
            rect_i = 0;
         }
         const S_elem &si = elem_ptr[i];

         if(I3DFloatAsInt(si.rotation_speed) != 0){
            float c = (float)cos(si.rotation_angle);
            float s = (float)sin(si.rotation_angle);
            s *= si.scale;
            c *= si.scale;

            m_rot(0, 0) = c;
            m_rot(1, 1) = c;
            m_rot(0, 1) = -s;
            m_rot(1, 0) = s;
         }else{
            m_rot(0, 0) = si.scale;
            m_rot(1, 1) = si.scale;
         }
         m_tmp = m_rot % m_camera;
         m_tmp(3) = si.matrix(3);

         S_matrix &mt = m_cache[rect_i];
         CopyMatrixTransposed4x3(mt, m_tmp);
                              //setup color of element (into 4th row of matrix)
         mt(3) = mat_diffuse;
         mt(3, 3) = si.opacity * mat_alpha;
      }
      assert(rect_i);
      d3d_dev->SetVertexShaderConstantF(VSC_PARTICLE_MATRIX_BASE, (const float*)m_cache, rect_i*4);
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, rect_i*4, 0, rect_i*2);
      CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
   }
}
#endif
//----------------------------

void I3D_particle_imp::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   IDirect3DDevice9 *d3d_dev = drv->GetDevice1();
   HRESULT hr;

   drv->SetupBlend(data.blend_mode);

   I3D_driver::S_ps_shader_entry_in se_ps;

   PI3D_texture_base tb = mat->GetTexture1(MTI_DIFFUSE);
                              //debug mode - without textures
   if((drv->GetFlags2()&DRVF2_DRAWTEXTURES) && tb){
      se_ps.Tex(0);
      se_ps.AddFragment(PSF_MOD_t0_v0);
      drv->SetTexture1(0, tb);
      drv->DisableTextures(1);
   }else{
      se_ps.AddFragment(PSF_v0_COPY);
      drv->DisableTextures(0);
   }

   drv->EnableNoCull(true);   //use no culling due to mirrors
   drv->SetupAlphaTest(false);

   drv->SetStreamSource(drv->vb_particle, sizeof(S_vertex_particle));
   drv->SetFVF(D3DFVF_XYZ);

   const S_vector &mat_diffuse = mat->GetDiffuse();
   float mat_alpha = mat->GetAlpha1() * (float)rp.alpha * R_255;

   I3D_driver::S_vs_shader_entry_in se;
   se.AddFragment(VSF_TRANSFORM_PARTICLE);
   se.AddFragment(VSF_MAKE_RECT_UV);
   PrepareVertexShader(NULL, 1, se, rp, pc.mode, VSPREP_WORLD_SPACE);

                              //store transposed projection matrix
   drv->SetVSConstant(VSC_MAT_TRANSFORM_0, &rp.scene->GetViewProjHomMatTransposed(), 4);
   drv->SetIndices(drv->ib_particle);
#ifndef GL
   if(drv->GetFlags2()&DRVF2_TEXCLIP_ON)
      se_ps.TexKill(1);
#endif
   drv->SetPixelShader(se_ps);

   PI3D_camera cam = rp.scene->GetActiveCamera1();
   const S_matrix &m_camera = cam->GetMatrixDirect();
   S_matrix m_rot;
   m_rot.Zero();
   m_rot(2, 2) = 1.0f;
   m_rot(3, 3) = 1.0f;

   S_matrix m_cache[MAX_PARTICLE_RECTANGLES];

   {
      S_matrix m_tmp;
      dword rect_i = 0;
      const S_elem *elem_ptr = &elems.front();
      for(dword i=elems.size(); i--; ++rect_i){
                              //flush rendering
         if(rect_i==MAX_PARTICLE_RECTANGLES){
            d3d_dev->SetVertexShaderConstantF(VSC_PARTICLE_MATRIX_BASE, (const float*)m_cache, MAX_PARTICLE_RECTANGLES*4);

            hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, MAX_PARTICLE_RECTANGLES*4, 0, MAX_PARTICLE_RECTANGLES*2);
            CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
            rect_i = 0;
         }
         const S_elem &si = elem_ptr[i];

         if(I3DFloatAsInt(si.rotation_speed) != 0){
            float c = (float)cos(si.rotation_angle);
            float s = (float)sin(si.rotation_angle);
            s *= si.scale;
            c *= si.scale;

            m_rot(0, 0) = c;
            m_rot(1, 1) = c;
            m_rot(0, 1) = -s;
            m_rot(1, 0) = s;
         }else{
            m_rot(0, 0) = si.scale;
            m_rot(1, 1) = si.scale;
         }
         m_tmp = m_rot % m_camera;
         m_tmp(3) = si.matrix(3);

         S_matrix &mt = m_cache[rect_i];
         CopyMatrixTransposed4x3(mt, m_tmp);
                              //setup color of element (into 4th row of matrix)
         mt(3) = mat_diffuse;
         mt(3, 3) = si.opacity * mat_alpha;
      }
      assert(rect_i);
      d3d_dev->SetVertexShaderConstantF(VSC_PARTICLE_MATRIX_BASE, (const float*)m_cache, rect_i*4);
      hr = d3d_dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, rect_i*4, 0, rect_i*2);
      CHECK_D3D_RESULT("DrawIndexedPrimitive", hr);
   }
}

//----------------------------

extern const S_visual_property props_Particle[] = {
   {I3DPROP_INT, "Out time", "Base time after which new elements are created."},
   {I3DPROP_INT, "Out time thresh", "Random time after which new elements are created."},
   {I3DPROP_INT, "Life len", "Base time - length of element's life."},
   {I3DPROP_INT, "Life len thresh", "Random time - length of element's life."},
   {I3DPROP_FLOAT, "Scale min", "Min range specifying how elements may be scaled."},
   {I3DPROP_FLOAT, "Scale max", "Max range specifying how elements may be scaled."},
   {I3DPROP_FLOAT, "Init pos thresh", "Thresh of new element's initial position."},
   {I3DPROP_FLOAT, "Init dir thresh", "Thresh of new element's initial direction."},
   {I3DPROP_INT, "Affect time", "Base time of period how often direction is changed."},
   {I3DPROP_INT, "Affect thresh", "Random time of period how often direction is changed."},
   {I3DPROP_FLOAT, "Affect dir", "Random factor of direction change."},
   {I3DPROP_FLOAT, "Opacity in", "Ratio of life (0.0 ... 1.0), until which opacity fades in during life-time of element."},
   {I3DPROP_FLOAT, "Opacity out", "Ratio of life (0.0 ... 1.0), until which opacity fades out during life-time of element."},
   {I3DPROP_FLOAT, "Base dir X", "Base direction."},
   {I3DPROP_FLOAT, "Base dir Y", "Base direction."},
   {I3DPROP_FLOAT, "Base dir Z", "Base direction."},
   {I3DPROP_FLOAT, "Dest dir X", "Destination direction."},
   {I3DPROP_FLOAT, "Dest dir Y", "Destination direction."},
   {I3DPROP_FLOAT, "Dest dir Z", "Destination direction."},
   {I3DPROP_ENUM, "Draw mode\0Opaque\0Blend\0Add\0", "Render mode."},
   {I3DPROP_INT, "Count", "Number of elements to emit, set zero for unlimited."},
   {I3DPROP_BOOL, "Base dir world", "Base direction is in world coordinates, rather than local."},
   {I3DPROP_BOOL, "Dest dir world", "Destination direction is in world coordinates, rather than local."},
   {I3DPROP_BOOL, "Emit base", "If set to true, particle emits initial set of elements when it is created."},
   {I3DPROP_FLOAT, "Rot speed min", "Rotation speed range, in radians/sec. Negative values rotate to left, positive to right."},
   {I3DPROP_FLOAT, "Rot speed max", "Rotation speed range, in radians/sec. Negative values rotate to left, positive to right."},
   {I3DPROP_FLOAT, "Element scale", "Scale of element. By default, size of element's rectangle's side is 1.0, this is multiplier."},
   {I3DPROP_FLOAT, "Opacity", "Opacity multiplier, value from 0.0 to 1.0."},
   {I3DPROP_ENUM, "Emit mode\0Sphere\0Circle XZ\0Mesh\0", "Emit mode - area in which new elements are generated."},
   {I3DPROP_BOOL, "Optimized", "Optimized mode, not animating if out of view."},
   {I3DPROP_NULL, "Material", "Material used for rendering particle."},
   //{I3DPROP_FLOAT, "Aspect", NULL},
   {I3DPROP_NULL}
};

I3D_visual *CreateParticle(PI3D_driver drv){
   return new I3D_particle_imp(drv);
}

//----------------------------
//----------------------------
