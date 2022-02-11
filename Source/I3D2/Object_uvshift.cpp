#include "all.h"
#include "object.h"
#include "mesh.h"

//----------------------------
//----------------------------

class I3D_object_uv_shift: public I3D_object{
                              //texture shifting
   struct{
      float add[2];
      float reserved[2];
   } curr_vals;
   S_vector2 uv_add;
   enum E_MODE{
      MODE_ANIMATE,
      MODE_DELTA,
   } mode;

   void ComputeShift();
public:
   I3D_object_uv_shift(PI3D_driver d1):
      I3D_object(d1),
      mode(MODE_ANIMATE)
   {
      visual_type = I3D_VISUAL_UV_SHIFT;
      uv_add.Zero();
      memset(&curr_vals, 0, sizeof(curr_vals));
   }

   virtual void AddPrimitives(S_preprocess_context&);
#ifndef GL
   virtual void DrawPrimitive(const S_preprocess_context&, const S_render_primitive&);
#endif
   virtual void DrawPrimitivePS(const S_preprocess_context&, const S_render_primitive&);
   virtual I3D_RESULT I3DAPI SetProperty(dword index, dword value){
      switch(index){
      case I3DPROP_OUVS_F_SHIFT_U: uv_add.x = I3DIntAsFloat(value); break;
      case I3DPROP_OUVS_F_SHIFT_V: uv_add.y = I3DIntAsFloat(value); break;
      case I3DPROP_OUVS_E_MODE:
         if(value>MODE_DELTA)
            return I3DERR_INVALIDPARAMS;
         mode = (E_MODE)value;
         break;
      default:
         assert(0);
         return I3DERR_INVALIDPARAMS;
      }
      vis_flags &= ~VISF_DEST_UV0_VALID;
      return I3D_OK;
   }

   virtual dword I3DAPI GetProperty(dword index) const{
      switch(index){
      case I3DPROP_OUVS_F_SHIFT_U: return I3DFloatAsInt(uv_add.x);
      case I3DPROP_OUVS_F_SHIFT_V: return I3DFloatAsInt(uv_add.y);
      case I3DPROP_OUVS_E_MODE: return mode;
      default: assert(0);
      }
      return 0xffcecece;
   }
};

//----------------------------

void I3D_object_uv_shift::ComputeShift(){

   int time_delta = drv->GetRenderTime() - last_render_time;
   float tsec = (float)time_delta * .001f;

   curr_vals.add[0] += uv_add.x * tsec;
   curr_vals.add[1] += uv_add.y * tsec;
                              //clamp to range -1.0 ... 1.0
   curr_vals.add[0] = (float)fmod(curr_vals.add[0], 1.0f);
   curr_vals.add[1] = (float)fmod(curr_vals.add[1], 1.0f);
}

//----------------------------

void I3D_object_uv_shift::AddPrimitives(S_preprocess_context &pc){

   if(!mesh)
      return;

   AddPrimitives1(mesh, pc);

   if(
#ifndef GL
      pc.mode!=RV_SHADOW_CASTER && 
#endif
      mode==MODE_ANIMATE)
      ComputeShift();
}

//----------------------------
#ifndef GL
void I3D_object_uv_shift::DrawPrimitive(const S_preprocess_context &pc, const S_render_primitive &rp){

   PI3D_mesh_base mb = mesh;

   HRESULT hr;
   {
      IDirect3DDevice9 *d3d_dev = drv->GetDevice1();

      if(last_auto_lod != rp.curr_auto_lod){
         last_auto_lod = rp.curr_auto_lod;
         vis_flags &= ~(VISF_DEST_LIGHT_VALID | VISF_DEST_UV0_VALID);
      }

      dword vertex_count;
      const C_auto_lod *auto_lods = mb->GetAutoLODs();
      if(rp.curr_auto_lod < 0){
         vertex_count = mb->vertex_buffer.NumVertices();
      }else{
         vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;
      }

      I3D_driver::S_vs_shader_entry_in se;

      dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV;
      if(pc.mode!=RV_SHADOW_CASTER){
         if(drv->IsDirectTransform()){
            prep_flags |= VSPREP_MAKELIGHTING;
         }else{
            if(!(vis_flags&VISF_DEST_LIGHT_VALID)){
               prep_flags |= VSPREP_MAKELIGHTING;
               vis_flags |= VISF_DEST_LIGHT_VALID;
            }
         }
         se.AddFragment(VSF_SHIFT_UV);
         se.AddFragment(VSF_STORE_UV_0);
         {
            switch(mode){
            case MODE_ANIMATE:
               break;
            case MODE_DELTA:
               vis_flags |= VISF_DEST_UV0_VALID;
               curr_vals.add[0] = uv_add[0];
               curr_vals.add[1] = uv_add[1];
               break;
            default:
               assert(0);
            }
         }
      }
   
      const I3D_driver::S_vs_shader_entry *se_out = PrepareVertexShader(mb->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

      if(pc.mode!=RV_SHADOW_CASTER){
                              //feed UV constants
         drv->SetVSConstant(se_out->vscs_uv_shift, &curr_vals);
      }

      if(!drv->IsDirectTransform()){
         drv->DisableTextureStage(1);

         drv->SetStreamSource(mb->vertex_buffer.GetD3DVertexBuffer(), mb->vertex_buffer.GetSizeOfVertex());
         drv->SetVSDecl(vertex_buffer.vs_decl);

         hr = d3d_dev->ProcessVertices(mb->vertex_buffer.D3D_vertex_buffer_index,
            vertex_buffer.D3D_vertex_buffer_index, vertex_count,
            vertex_buffer.GetD3DVertexBuffer(), NULL, D3DPV_DONOTCOPYDATA);
         CHECK_D3D_RESULT("ProcessVertices", hr);
         drv->SetVertexShader(NULL);
      }
   }
   DrawPrimitiveVisual(mb, pc, rp);
}
#endif
//----------------------------

void I3D_object_uv_shift::DrawPrimitivePS(const S_preprocess_context &pc, const S_render_primitive &rp){

   PI3D_mesh_base mb = mesh;

   /*
   dword vertex_count;
   const C_auto_lod *auto_lods = mb->GetAutoLODs();
   if(rp.curr_auto_lod < 0){
      vertex_count = mb->vertex_buffer.NumVertices();
   }else{
      vertex_count = auto_lods[rp.curr_auto_lod].vertex_count;
   }
   */
   I3D_driver::S_vs_shader_entry_in se;

   dword prep_flags = VSPREP_TRANSFORM | VSPREP_FEED_MATRIX | VSPREP_NO_COPY_UV;
#ifndef GL
   if(pc.mode!=RV_SHADOW_CASTER)
#endif
   {
      prep_flags |= VSPREP_MAKELIGHTING;
      se.AddFragment(VSF_SHIFT_UV);
      se.AddFragment(VSF_STORE_UV_0);

      switch(mode){
      case MODE_ANIMATE:
         break;
      case MODE_DELTA:
         vis_flags |= VISF_DEST_UV0_VALID;
         curr_vals.add[0] = uv_add[0];
         curr_vals.add[1] = uv_add[1];
         break;
      default:
         assert(0);
      }
   }

   const I3D_driver::S_vs_shader_entry *se_out = PrepareVertexShader(mb->GetFGroups1()[0].GetMaterial1(), 1, se, rp, pc.mode, prep_flags);

                              //feed UV constants
#ifndef GL
   if(pc.mode!=RV_SHADOW_CASTER)
#endif
      drv->SetVSConstant(se_out->vscs_uv_shift, &curr_vals);

   DrawPrimitiveVisualPS(mb, pc, rp);
}

//----------------------------
//----------------------------

extern const S_visual_property props_ObjectUVshift[] = {
                              //I3DPROP_OUVS_F_SHIFT_U
   {I3DPROP_FLOAT, "Anim U", "Shift values for U coordinate. The meaning of these values is dependent on mode."},
                              //I3DPROP_OUVS_F_SHIFT_V
   {I3DPROP_FLOAT, "Anim V", "Shift values for V coordinate. The meaning of these values is dependent on mode."},
                              //I3DPROP_OUVS_E_MODE
   {I3DPROP_ENUM, "Mode\0Animate\0Delta\0", "Shifting mode."},
   {I3DPROP_NULL}
};

I3D_visual *CreateObjectUVshift(PI3D_driver drv){
   return new I3D_object_uv_shift(drv);
}

//----------------------------
//----------------------------

