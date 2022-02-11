#include "all.h"                                   
#include "main.h"
#include "TerrainDetail.h"
#include <list>

//----------------------------

#define FADE_OUT_DIST_RATIO .8f  //ratio of distance, beyond which we fade out
#define GRID_EXPAND 3         //how many grid rects are mapped per visibility distance

#ifdef _DEBUG

#define _LOG(n) LOG(n)
//#define DEBUG_DRAW_GRID       //display region grid

#else

#define _LOG(n)

#endif

//----------------------------

inline bool CheckPointRight(const S_vector2 &px, const S_vector &lp, const S_vector &ldir){
   return ((px.y*ldir.x-px.x*ldir.z) < (lp.z*ldir.x-lp.x*ldir.z));
}

//----------------------------
// Check if point 'vx' is inside of 2d triangle v1-v2-v3
inline bool IsInsideTri(const S_vector2 &vx, const S_vector &v0, const S_vector &v1, const S_vector &v2){

   return (CheckPointRight(vx, v0, v1-v0) && CheckPointRight(vx, v1, v2-v1) && CheckPointRight(vx, v2, v0-v2));
}

//----------------------------

class C_terrain_detail_imp: public C_terrain_detail{

                              //simple terrain triangle - indices are into vertex array
   struct S_face: public I3D_triface{
      word set_i;             //index into model set
      word face_index;
      C_smart_ptr<I3D_visual> vis;
      S_face(){}
      S_face(word id, word fi, PI3D_visual v):
         set_i(id),
         face_index(fi),
         vis(v)
      {}
   };

                              //set of models in one set
   struct S_model_set{
      typedef C_buffer<C_smart_ptr_1<I3D_model> > t_model_list;
      t_model_list models;
      float density;
      float scale_thresh;
      //bool normal_align;      //true to aligh model's Y by face normal

      S_model_set(){}
      /*
      S_model_set(const S_model_set &s){ operator =(s); }
      void operator =(const S_model_set &s){
         models = s.models;
         density = s.density;
         scale_thresh = s.scale_thresh;
         //normal_align = s.normal_align;
      }
      */
   };

   C_smart_ptr<I3D_scene> scene; //scene we work on

                              //input settings:
   float range;               //range of influence
   float region_scale;         //scale of region grid rectangle
   C_buffer<S_model_set> model_sets;

                              //terrain regions (2D grid)
   typedef C_vector<word> t_region;
   class C_region{
      C_region(const C_region&);
      void operator =(const C_region&);
   public:
      C_vector<word> faces;

      class C_model{
         C_model(const C_model&);
      public:
         C_smart_ptr<I3D_model> model;
         S_vector pos;
         float alpha;

         C_model(){}
         /*
         operator =(const C_model &m){
            model = m.model;
            pos = m.pos;
            alpha = m.alpha;
         }
         */
         void SetAlpha(float a){

            const PI3D_frame *frms = model->GetFrames();
            for(dword i=model->NumFrames(); i--; ){
               PI3D_frame frm = *frms++;
               if(frm->GetType()==FRAME_VISUAL)
                  I3DCAST_VISUAL(frm)->SetVisualAlpha(a);
            }
            alpha = a;
         }
      };
      typedef C_buffer<C_model> t_model_list;
      t_model_list *models;

      C_region(): models(NULL)
      {}
      ~C_region(){
         delete models;
      }
   };
   C_region *regions;
                              //resolution of region grid
   dword region_grid_size[2];
                              //base point of region grid
   S_vector2 region_base;
   C_vector<S_vector> terrain_verts;
   C_vector<S_face> terrain_faces;  //all terrain's triangles

   int last_cam_grid_pos[2];

//----------------------------

   static float GetRnd(){
      return (float)rand() / (float)RAND_MAX;
   }
   static dword GetRnd(dword max){
      return rand() % max;
   }

//----------------------------

   typedef list<C_smart_ptr_1<I3D_model> > t_rel_model_list;

//----------------------------

   void InitRegionModels(dword rx, dword ry, t_rel_model_list &rel_models){

      C_region &reg = regions[region_grid_size[0]*ry + rx];
      if(reg.models)
         return;
      //_LOG(C_fstr("Creating [%i][%i]", ry, rx));
      srand(ry * 123456 + rx*23);
      reg.models = new C_region::t_model_list;
      int num_models = FloatToInt(region_scale * region_scale);
      reg.models->assign(num_models);
      //for(int i=num_models; i--; )
      for(int i=0; i<num_models; ){
         C_region::C_model &md = (*reg.models)[i];

         S_vector2 pos = region_base;
         pos.x += (rx + GetRnd()) * region_scale;
         pos.y += (ry + GetRnd()) * region_scale;
         float pt_y, brightness;
         S_plane pl;
         int set_id = LayDownPoint(pos, reg, pt_y, brightness, pl);
         if(set_id==-1){
            //md = (*reg.models)[--num_models];
            --num_models;
            continue;
         }
#if 0
         DebugLine(S_vector(pos.x, 100, pos.y), S_vector(pos.x, pt_y, pos.y), 0);
#endif
         const S_model_set &mset = model_sets[set_id];
         assert(mset.models.size());

                              //check if density matches
         if(GetRnd() > mset.density){
            //md = (*reg.models)[--num_models];
            --num_models;
            continue;
         }

         PI3D_model mod = NULL;
         CPI3D_model mod_src = mset.models[GetRnd(mset.models.size())];
         bool reused = false;
                              //try to find already made model among recently released
         for(t_rel_model_list::iterator it = rel_models.begin(); it!=rel_models.end(); it++){
            PI3D_model mod_rel = (*it);
            assert(mod_rel);
            //if(!strcmp(mod_rel->GetFileName(), mod_src->GetFileName())){
            if(mod_rel->GetFileName() == mod_src->GetFileName()){
               mod = mod_rel;
               mod->AddRef();
               rel_models.erase(it);
               reused = true;
               //_LOG(C_fstr("--- %s", mod_src->GetFileName()));
               break;
            }
         }
         if(!mod){
                              //not found among released, create new
            mod = I3DCAST_MODEL(scene->CreateFrame(FRAME_MODEL));
            mod->Duplicate(mod_src);
            //_LOG(C_fstr("!!! %s", mod_src->GetFileName()));
         }

         S_vector model_pos(pos.x, pt_y, pos.y);
         mod->SetPos(model_pos);
         mod->LinkTo(scene->GetPrimarySector());

                              //apply random scale
         if(mset.scale_thresh){
            float s = 1.0f - mset.scale_thresh + GetRnd() * 2.0f * mset.scale_thresh;
            mod->SetScale(s);
         }
                              //apply random rotation
         bool normal_align = (mod->GetName()[0ul]=='!');
         float rot = GetRnd() * PI*2.0f;
         if(!normal_align){
            mod->SetRot(S_quat(S_vector(0, 1, 0), rot));
         }else{
            //S_vector &up = pl.normal;
            S_matrix m;
            m.SetDir(pl.normal, rot);
            mod->SetDir1(m(1), m(2));
         }

         md.model = mod;
         md.pos = model_pos;
         if(reused)
            md.SetAlpha(1.0f);
         md.alpha = 1.0f;
         SetModelBrightness(md.model, brightness);
         mod->Release();
         i++;
      }
      reg.models->resize(num_models);
   }

//----------------------------

   void ReleaseRegionModels(dword rx, dword ry, t_rel_model_list &rel_models){

      assert(rx<region_grid_size[0] && ry<region_grid_size[1]);
      C_region &reg = regions[region_grid_size[0]*ry + rx];
      if(reg.models){
         //_LOG(C_fstr("Releasing [%i][%i]", ry, rx));
                              //save models being released (for possible reuse)
         for(int i=reg.models->size(); i--; ){
            PI3D_model mod = (*reg.models)[i].model;
            //mod->LinkTo(NULL);
            assert(mod);
            rel_models.push_back(mod);
         }
         delete reg.models;
         reg.models = NULL;
      }
   }

//----------------------------
// Get intersection of specified point. The returned value is set id, or -1 if no such point exists.
   int LayDownPoint(const S_vector2 &v, const C_region &reg, float &y, float &brightness, S_plane &pl){

      for(int i=reg.faces.size(); i--; ){
         const S_face &fc = terrain_faces[reg.faces[i]];
         const S_vector &p0 = terrain_verts[fc[0]];
         const S_vector &p1 = terrain_verts[fc[1]];
         const S_vector &p2 = terrain_verts[fc[2]];
         if(IsInsideTri(v, p0, p1, p2)){

            pl.ComputePlane(p0, p1, p2);
            S_vector hit_pos;
            hit_pos.y = 0;
            if(pl.Intersection(S_vector(v.x, 1000, v.y), S_vector(0, -1, 0), hit_pos)){
               y = hit_pos.y;

                              //check brigthness on specified location
               brightness = 1.0f;
               if(fc.vis && fc.vis->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
                  CPI3D_lit_object lobj = I3DCAST_CLIT_OBJECT(fc.vis);
                  S_vector v_color;
                  S_vector point_on_face = hit_pos * lobj->GetInvMatrix();
                  if(lobj->GetLMPixel(fc.face_index, point_on_face, v_color)){
                              //check lightness on the point directly
                     float color = v_color.GetBrightness();
                     //S_vector light = scene->GetLightness(hit_pos, &pl.normal, I3DLIGHTMODE_LIGHTMAP);
                     //float light = scene->GetLightness(hit_pos, &pl.normal, I3DLIGHTMODE_LIGHTMAP).GetBrightness();
                     S_vector ldir(0, 1, 0);
                     float light = scene->GetLightness(hit_pos, &ldir, I3DLIGHTMODE_LIGHTMAP).GetBrightness();
                     //float light = scene->GetLightness(hit_pos, NULL, I3DLIGHTMODE_LIGHTMAP).GetBrightness() * .5f;

                     //S_vector ambient(0, 0, 0);
                     float ambient = 0.0f;
                              //compute ambient light in the sector
                     for(CPI3D_frame sct1 = fc.vis; sct1=sct1->GetParent(), sct1; ){
                        if(sct1->GetType()==FRAME_SECTOR){
                           CPI3D_sector sct = I3DCAST_CSECTOR(sct1);
                           for(dword i=sct->NumLights(); i--; ){
                              CPI3D_light lp = sct->GetLight(i);
                              if((lp->GetMode()&I3DLIGHTMODE_LIGHTMAP) && lp->GetLightType()==I3DLIGHT_AMBIENT){
                                 //ambient += lp->GetColor() * lp->GetPower();
                                 ambient += lp->GetColor().GetBrightness() * lp->GetPower();
                              }
                           }
                           break;
                        }
                     }
                     light -= ambient;
                     color -= ambient;
                     float delta = light - color;
                     //float pd = delta.GetBrightness();
                     //float pl = light.GetBrightness();
                     //float pa = ambient.GetBrightness();
                     //pd /= (pl-pa);
                     delta /= (light - ambient);
                     delta *= .5f;
                     brightness = 1.0f - delta;
                     //brightness = Min(1.0f, brightness*2.0f);
#if 0
                     if(S_int_random(100) < 5){
                        float d = 1.0f - delta;
                        d = Max(0.0f, Min(1.0f, d));
                        S_vectorw lc(d, d, d, 1);
                        DebugLine(hit_pos, hit_pos+S_vector(0, 1, 0), 0, (int(lc.w*255.0f)<<24) | (int(lc.x*255.0f)<<16) | (int(lc.y*255.0f)<<8) | (int(lc.z*255.0f)<<0));
                     }
#endif
                  }
               }

               return fc.set_i;
            }
         }
      }
      return -1;
   }

//----------------------------
// Add single vertex (possibly split with existing), return it's index in array.
// Optimize for speed in EDITOR mode, and for memory otherwise.
   word AddVertex(const S_vector &v){

      int vi;
#ifndef EDITOR
      for(vi=terrain_verts.size(); vi--; ){
         float sq = (terrain_verts[vi] - v).Square();
         if(sq < .0001f)
            break;
      }
      if(vi==-1)
#endif
      {
         vi = terrain_verts.size();
         terrain_verts.push_back(v);
      }
      return (word)vi;
   }

//----------------------------
// Add single triangle to list of terrain triangles.
// Also create indices into this triangle to the grid regions.
   void AddTriangle(const S_vector v[3], word tex_id, word vis_face_index, PI3D_visual vis){

      word face_index = (word)terrain_faces.size();
                              //store the face
      terrain_faces.push_back(S_face(tex_id, vis_face_index, vis));
      I3D_triface &fc = terrain_faces.back();

                              //determine all regions this face may be assigned in
      I3D_bbox bb;
      bb.Invalidate();
      for(dword i=3; i--; ){
         bb.min.Minimal(v[i]);
         bb.max.Maximal(v[i]);
                              //also store vertex and construct face now
         const S_vector &vv = v[i];
         fc[i] = AddVertex(vv);
      }
                              //this code assumes that convertsion from float to int rounds to nearest
      int min_x = Max(0, FloatToInt((bb.min.x - region_base.x) / region_scale - .5f));
      int min_y = Max(0, FloatToInt((bb.min.z - region_base.y) / region_scale - .5f));
      int max_x = Min(FloatToInt((bb.max.x - region_base.x) / region_scale - .5f), (int)region_grid_size[0]-1);
      int max_y = Min(FloatToInt((bb.max.z - region_base.y) / region_scale - .5f), (int)region_grid_size[1]-1);

      assert(min_x>=0 && max_x<(int)region_grid_size[0] && min_y>=0 && max_y<(int)region_grid_size[1]);

      for(int y=min_y; y<=max_y; ++y){
         float rx[2], ry[2];  //grid rectangle coords
         ry[0] = region_base.y + (float)y * region_scale;
         ry[1] = ry[0] + region_scale;
         rx[0] = region_base.x + (float)min_x * region_scale;
         rx[1] = rx[0] + region_scale;
         for(int x=min_x; x<=max_x; ++x, rx[0] = rx[1], rx[1] += region_scale){
                              //check it this grid rectangle really collides with the triangle
            S_vector2 vt0, vt1(v[0].x, v[0].z);
            for(i=3; i--; ){
               vt0 = vt1;
               vt1 = S_vector2(v[i].x, v[i].z);
               for(int j=4; j--; ){
                  S_vector2 vr(rx[j&1], ry[j/2]);
                  if(vr.DistanceToLine(vt0, vt1-vt0) > 0.0f)
                     break;
               }
               if(j==-1)
                  break;
            }
            if(i==-1){
                              //really add face (index) to the rectangle
               regions[region_grid_size[0] * y + x].faces.push_back(face_index);
            }
         }
      }
   }

//----------------------------

   struct S_enum_hlp{
      const S_terrdetail_init *init_data;
      C_terrain_detail_imp *_this;
   };

//----------------------------
// Enum function for building triangles from terrain meshes.
   static I3DENUMRET I3DAPI cbInit(PI3D_frame frm, dword context){

      PI3D_visual vis = I3DCAST_VISUAL(frm);
      const S_enum_hlp *hp = (S_enum_hlp*)context;

                           //store grass area to structure
      const S_matrix &m = frm->GetMatrix();
      PI3D_mesh_base mesh = vis->GetMesh();
      if(mesh){
         C_buffer<I3D_triface> faces;
         const S_vector *verts = NULL;
         dword vstride = 0;

                           //process all face groups
         for(dword j = mesh->NumFGroups(); j--; ){

            const I3D_face_group &fg = mesh->GetFGroups()[j];
            CPI3D_material mat = ((PI3D_face_group)&fg)->GetMaterial();
            //CPI3D_texture tex = mat->GetTexture(MTI_DIFFUSE);
            //const C_str &texname = tex->GetFileName();
            const C_str &matname = mat->GetName();
                           //check if texture name matches at least one set
            for(int set_i = hp->init_data->model_sets.size(); set_i--; ){
               const C_vector<C_str> &msets = hp->init_data->model_sets[set_i].material_dests;
               for(int ti=msets.size(); ti--; ){
                  if(matname.Matchi(msets[ti]))
                     break;
               }
               if(ti!=-1)
                  break;
            }
            if(set_i==-1)
               continue;
                           //lock mesh, if not yet
            if(!verts){
               faces.assign(mesh->NumFaces());
               mesh->GetFaces(faces.begin());
               verts = mesh->LockVertices();
               vstride = mesh->GetSizeOfVertex();   
            }
            for(dword i=fg.num_faces; i--; ){
               const I3D_triface &fc = faces[fg.base_index+i];
                     //write face to all intersect regions (2D)
               const S_vector *v0 = (S_vector*)(dword(verts) + fc[0]*vstride),
                  *v1 = (S_vector*)(dword(verts) + fc[1]*vstride),
                  *v2 = (S_vector*)(dword(verts) + fc[2]*vstride);
               S_vector tv[3] = {*v0 * m, *v1 * m, *v2 * m};
#if 0
               DebugLine(tv[0], tv[1], 0);
               DebugLine(tv[1], tv[2], 0);
               DebugLine(tv[2], tv[0], 0);
#endif
               hp->_this->AddTriangle(tv, (word)set_i, word(fg.base_index+i), vis);
            }
         }
         if(verts){
            mesh->UnlockVertices();
         }
      }
      return I3DENUMRET_OK;
   }

//----------------------------

   void InitTerrain(const S_terrdetail_init &init_data){

      S_enum_hlp hlp = {&init_data, this};
      scene->EnumFrames(cbInit, (dword)&hlp, ENUMF_VISUAL | ENUMF_WILDMASK, init_data.terrain_name_base);
   }

//----------------------------

   void InitSets(const S_terrdetail_init &init_data){

                              //init model sets - load files
      model_sets.assign(init_data.model_sets.size());
      for(dword i=init_data.model_sets.size(); i--; ){
         const S_terrdetail_init::S_model_set &init_mset = init_data.model_sets[i];
         S_model_set &mset = model_sets[i];

         mset.density = init_mset.density;
         mset.scale_thresh = init_mset.scale_thresh;
         const C_vector<S_terrdetail_init::S_model_set::S_model_source> &msrcs = init_mset.model_sources;
         S_model_set::t_model_list &models = mset.models;
         models.assign(msrcs.size());
         for(int si=msrcs.size(); si--; ){
            PI3D_model mod = I3DCAST_MODEL(scene->CreateFrame(FRAME_MODEL));
            model_cache.Open(mod, msrcs[si].filename, scene, 0, ErrReport, editor);
            mod->SetName(msrcs[si].normal_align ? "!" : "");
            /*
#ifdef EDITOR
            mod->SetName(C_fstr("TerrainDetail %i (set %i)", si, i));
#endif
            */
            models[si] = mod;
            mod->Release();
         }
      }
   }

//----------------------------

public:
   C_terrain_detail_imp(PI3D_scene s, const S_terrdetail_init &init_data):
      scene(s)
   {
      range = init_data.visible_distance;
      region_scale = range / (float)GRID_EXPAND;
      last_cam_grid_pos[0] = -100;
      last_cam_grid_pos[1] = -100;
                              //initialize region grid
      I3D_bound_volume bv;
      scene->GetPrimarySector()->ComputeHRBoundVolume(&bv);
      region_grid_size[0] = FloatToInt((bv.bbox.max.x - bv.bbox.min.x) / region_scale) + 1;
      region_grid_size[1] = FloatToInt((bv.bbox.max.z - bv.bbox.min.z) / region_scale) + 1;
      region_base.x = bv.bbox.min.x;
      region_base.y = bv.bbox.min.z;
      regions = new C_region[region_grid_size[0] * region_grid_size[1]];

#ifdef DEBUG_DRAW_GRID
      {
         //_LOG(C_fstr("Region size: %ix%i", region_grid_size[0], region_grid_size[1]));
         for(dword i=0; i<region_grid_size[0]+1; i++)
            DebugLine(S_vector(region_base.x + region_scale*i, 1, region_base.y), S_vector(region_base.x + region_scale*i, 1, region_base.y+region_grid_size[1]*region_scale), 0);
         for(i=0; i<region_grid_size[1]+1; i++)
            DebugLine(S_vector(region_base.x, 1, region_base.y + region_scale*i), S_vector(region_base.x + region_grid_size[0]*region_scale, 1, region_base.y+i*region_scale), 0);
      }
#endif

      InitSets(init_data);
      InitTerrain(init_data);
   }
   ~C_terrain_detail_imp(){
      delete[] regions;
   }                            

   virtual void Update(int time, PI3D_camera cam){

      assert(cam);
      const S_vector &cpos = cam->GetWorldPos();
                              //check if we've crossed to another grid sector
      int curr_x = FloatToInt((cpos.x - region_base.x) / region_scale - .5f);
      int curr_y = FloatToInt((cpos.z - region_base.y) / region_scale - .5f);

                              //determine grid rectangle
      int new_rc_min[2];
      int new_rc_max[2];
      new_rc_min[0] = Max(0, curr_x - GRID_EXPAND);
      new_rc_min[1] = Max(0, curr_y - GRID_EXPAND);
      new_rc_max[0] = Min(curr_x + GRID_EXPAND, (int)region_grid_size[0]-1);
      new_rc_max[1] = Min(curr_y + GRID_EXPAND, (int)region_grid_size[1]-1);

      if(last_cam_grid_pos[0] != curr_x || last_cam_grid_pos[1] != curr_y){
         //BegProf();
                              //camera changed current grid, re-init
                              //uninit old rectangle
         int old_rc_min[2];
         int old_rc_max[2];
         old_rc_min[0] = Max(0, last_cam_grid_pos[0] - GRID_EXPAND);
         old_rc_min[1] = Max(0, last_cam_grid_pos[1] - GRID_EXPAND);
         old_rc_max[0] = Min(last_cam_grid_pos[0] + GRID_EXPAND, (int)region_grid_size[0]-1);
         old_rc_max[1] = Min(last_cam_grid_pos[1] + GRID_EXPAND, (int)region_grid_size[1]-1);

         t_rel_model_list rel_models;
         for(int y = old_rc_min[1]; y<=old_rc_max[1]; y++){
            for(int x = old_rc_min[0]; x<=old_rc_max[0]; x++){
               if(y < new_rc_min[1] || y > new_rc_max[1] || x < new_rc_min[0] || x > new_rc_max[0])
                  ReleaseRegionModels(x, y, rel_models);
            }
         }

         for(y = new_rc_min[1]; y<=new_rc_max[1]; y++){
            for(int x = new_rc_min[0]; x<=new_rc_max[0]; x++)
               InitRegionModels(x, y, rel_models);
         }

                              //save current position
         last_cam_grid_pos[0] = curr_x;
         last_cam_grid_pos[1] = curr_y;
         //OUTTEXT(EndProf());
      }

      const float range2 = range * range;
      const float range_fade = range*FADE_OUT_DIST_RATIO;
      const float range_fade2 = range_fade * range_fade;
      const float r_out_range = 1.0f / (range-range_fade);
                              //update models in all regions
      for(int y = new_rc_min[1]; y<=new_rc_max[1]; y++){
         for(int x = new_rc_min[0]; x<=new_rc_max[0]; x++){
            C_region &reg = regions[region_grid_size[0]*y + x];
            assert(reg.models);
            C_region::t_model_list &models = *reg.models;
            for(dword mi = models.size(); mi--; ){
               C_region::C_model &md = models[mi];
               float d2 = (md.pos - cpos).Square();
               dword &dw_alpha = *(dword*)&md.alpha;
               if(d2 >= range2){
                  if(dw_alpha){
                     dw_alpha = 0;
                     md.model->SetOn(false);
                  }
               }else{
                  if(!dw_alpha)
                     md.model->SetOn(true);
                  float alpha = 1.0f;
                  if(d2 > range_fade2){
                     float d = I3DSqrt(d2);
                     alpha = ((range-d) * r_out_range);
                     assert(alpha>=0.0f && alpha<=1.0f);
                  }
                  if(md.alpha!=alpha)
                     md.SetAlpha(alpha);
               }
            }
         }
      }
   }
};

//----------------------------
//----------------------------

C_terrain_detail *CreateTerrainDetail(PI3D_scene s, const S_terrdetail_init &init_data){

   return new C_terrain_detail_imp(s, init_data);
}

//----------------------------
