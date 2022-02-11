#include "all.h"
#include "common.h"
#include <insanity\os.h>

//----------------------------

//#define DETECT_LIGHTS_IN_MODELS

//----------------------------
// Load disk file into a buffer.
// The return value is number of bytes read, or -1 if some error
// occured.
static int Load(const char *name, void *addy, dword size){

   PC_dta_stream dta = DtaCreateStream(name);
   if(!dta)
      return -1;
   size_t file_len = dta->GetSize();
   size = Min(file_len, (size_t)size);
   size_t read_len = dta->Read(addy, size);
   dta->Release();
   return read_len;
}

//----------------------------
// Save data from a buffer to disk.
// The return value is number of bytes written, or -1 if some error
// occured.
static int Save(const char *name, const void *buf, dword size){

   HANDLE h;
   h = CreateFile(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, NULL);
   if(h==INVALID_HANDLE_VALUE)
      return -1;
   ulong wb;
   int st = WriteFile(h, buf, size, &wb, NULL);
   CloseHandle(h);
   return st ? wb : -1;
}

//----------------------------

class C_edit_Validity: public C_editor_item{
   virtual const char *GetName() const{ return "Validity"; }

//----------------------------

   enum{
      ACTION_VALIDATE,
      ACTION_RES_DUMP,
   };

//----------------------------

   bool ValidateGeometry(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      //BegProf();
      bool ok = true;

                              //collect all unique meshes
      C_vector<PI3D_visual> viss;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

            PI3D_visual vis = I3DCAST_VISUAL(frm);
            if(vis->GetMesh()){
               C_vector<PI3D_visual> &v = *(C_vector<PI3D_visual>*)c;
               v.push_back(vis);
            }
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&viss, ENUMF_VISUAL);

      PC_editor_item_DebugLine e_debugline = (PC_editor_item_DebugLine)ed->FindPlugin("DebugLine");

      typedef map<int, S_plane> t_mirror_planes;
      t_mirror_planes mirror_planes;

      for(dword vi=viss.size(); vi--; ){

         int i;
         int num_dup_faces = 0;
         PI3D_visual vis = viss[vi];
         PI3D_mesh_base mb = vis->GetMesh();

         const S_vector *verts = mb->LockVertices();
         dword numv = mb->NumVertices();
         C_vector<word> v_map(numv);
         int vstride = mb->GetSizeOfVertex();
         MakeVertexMapping(verts, vstride, numv, &v_map.front(), .001f);

         dword num_faces = mb->NumFaces();
                              //remapped faces
         //C_vector<I3D_triface> faces;
         I3D_triface *faces = new I3D_triface[num_faces];

         //const PI3D_triface vfcs = mb->LockFaces();
         //memcpy(faces, vfcs, num_faces*sizeof(I3D_triface));
         //mb->UnlockFaces();
         mb->GetFaces(faces);

                              //remap faces
         for(i=num_faces; i--; ){
            I3D_triface &fc = faces[i];
            fc.vi[0] = v_map[fc.vi[0]];
            fc.vi[1] = v_map[fc.vi[1]];
            fc.vi[2] = v_map[fc.vi[2]];
         }

         for(i=num_faces; i--; ){
                              //copy face, it's accessed in loop
            const I3D_triface f0 = faces[i];
            for(int j=i; j--; ){
                              //access by reference, it's faster
               const I3D_triface &f1 = faces[j];

               bool dup = false;
               if(f0.vi[0]==f1.vi[0] && f0.vi[1]==f1.vi[1] && f0.vi[2]==f1.vi[2]){
                  dup = true;
               }else
               if(f0.vi[0]==f1.vi[1] && f0.vi[1]==f1.vi[2] && f0.vi[2]==f1.vi[0]){
                  dup = true;
               }else
               if(f0.vi[0]==f1.vi[2] && f0.vi[1]==f1.vi[0] && f0.vi[2]==f1.vi[1]){
                  dup = true;
               }
               if(dup){
                  ++num_dup_faces;
                  if(e_debugline){
                     const S_matrix &m = vis->GetMatrix();
                     S_vector vw[3];
                     int i;
                     for(i=3; i--; )
                        vw[i] = *(S_vector*)(((byte*)verts) + f0[i]*vstride) * m;
                     for(i=3; i--; )
                        //e_debugline->Action(E_DEBUGLINE_ADD, &S_add_line(vw[i], vw[(i+1)%3], 0xffff0000));
                        e_debugline->AddLine(vw[i], vw[(i+1)%3], DL_PERMANENT, 0xffff0000);
                  }
               }
            }
         }

         if(num_dup_faces){
            ok = false;
            e_log->AddText(C_fstr("Visual '%s' has %i duplicated face(s).", (const char*)vis->GetName(), num_dup_faces));
            e_slct->AddFrame(vis);
         }

         {
                              //check mirrors
            CPI3D_face_group fgrps = mb->GetFGroups();
            for(dword fgi=mb->NumFGroups(); fgi--; ){
               const I3D_face_group &fg = fgrps[fgi];
               CPI3D_material mat = fg.GetMaterial();
               int mid = mat->GetMirrorID();
               if(mid==-1)
                  continue;

               const float MAX_NORMAL_DELTA = .02f;
               const float MAX_D_DELTA = .02f;

               S_plane plane(0, 0, 0, 0);
               for(dword fi=0; fi<fg.num_faces; fi++){
                  const I3D_triface &fc = faces[fg.base_index + fi];
                  const S_matrix &m = vis->GetMatrix();
                  const S_vector v[3] = {
                     *(S_vector*)(((byte*)verts) + vstride*fc[0]) * m,
                     *(S_vector*)(((byte*)verts) + vstride*fc[1]) * m,
                     *(S_vector*)(((byte*)verts) + vstride*fc[2]) * m,
                  };
                  S_plane pl;
                  pl.ComputePlane(v[0], v[1], v[2]);
                  if(!fi){
                     plane = pl;
                  }else{
                     float dot_delta = 1.0f - pl.normal.Dot(plane.normal);
                     float d_delta = I3DFabs(pl.d - plane.d);
                     if(dot_delta > MAX_NORMAL_DELTA || d_delta > MAX_D_DELTA){
                        ok = false;
                        e_log->AddText(
                           C_fstr("Visual '%s': non-planar mirror faces (id = %i).", (const char*)vis->GetName(), mid));
                        e_slct->AddFrame(vis);
                        break;
                     }
                  }
               }
                              //check if planar with other visuals
               t_mirror_planes::iterator it = mirror_planes.find(mid);
               if(it==mirror_planes.end())
                  mirror_planes[mid] = plane;
               else{
                  const S_plane &pl = it->second;
                  float dot_delta = 1.0f - pl.normal.Dot(plane.normal);
                  float d_delta = I3DFabs(pl.d - plane.d);
                  if(dot_delta > MAX_NORMAL_DELTA || d_delta > MAX_D_DELTA){
                     ok = false;
                     e_log->AddText(
                        C_fstr("Visual '%s': non-planar mirror with other visuals (id = %i).", (const char*)vis->GetName(), mid));
                     e_slct->AddFrame(vis);
                     break;
                  }
               }
            }
         }
         delete[] faces;
         mb->UnlockVertices();
      }
      //e_log->Action(E_LOG_ADD_TEXT, (void*)(const char*)C_fstr("%.3f", EndProf())); return 1;
      return ok;
   }

//----------------------------

   bool ValidateModels(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      bool ok = true;
                              //check model duplication
      C_vector<PI3D_model> v;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

            C_vector<PI3D_model> &v = *(C_vector<PI3D_model>*)c;
            v.push_back(I3DCAST_MODEL(frm));
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&v, ENUMF_MODEL);

      for(int i=v.size(); i-- ; ){
         PI3D_model model = v[i];
         const S_vector &pos = model->GetWorldPos();
         const S_quat &rot = model->GetRot();
         for(int j=i; j-- ; ){
            PI3D_model model1 = v[j];
            const S_vector &pos1 = model1->GetWorldPos();
            const S_quat &rot1 = model1->GetRot();
            S_vector dir_to = pos - pos1;
            if(dir_to.Dot(dir_to) < .001f && rot==rot1 &&
               //!stricmp(model->GetFileName(), model1->GetFileName()))
               model->GetFileName()==model1->GetFileName()){
               e_log->AddText(
                  C_fstr("Model duplication on [%.3f, %.3f, %.3f] - '%s', duplicated with '%s'",
                  pos.x, pos.y, pos.z,
                  (const char*)model1->GetName(), (const char*)model->GetName()));
               e_slct->AddFrame(model1);
               ok = false;
            }
         }
#ifdef DETECT_LIGHTS_IN_MODELS
                              //check for lights in models
         if(model->FindChildFrame("*", ENUMF_LIGHT | ENUMF_WILDMASK)){
            e_slct->AddFrame(model);
            e_log->Action(E_LOG_ADD_TEXT, (void*)(const char*)
               C_fstr("Model '%s' contains light(s)", model->GetName()));
            ok = false;
         }
#endif
                              //check if model is not empty
         if(!model->NumChildren()){
            e_log->AddText(C_fstr("Model '%s' has no frames", (const char*)model->GetName()));
            e_slct->AddFrame(model);
            ok = false;
         }
      }
      return ok;
   }

//----------------------------

   bool ValidateVisuals(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      bool ok = true;
                              //check model duplication
      C_vector<PI3D_visual> v;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

            C_vector<PI3D_visual> &v = *(C_vector<PI3D_visual>*)c;
            v.push_back(I3DCAST_VISUAL(frm));
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&v, ENUMF_VISUAL);

      for(int i=v.size(); i-- ; ){
         PI3D_visual vis = v[i];
         if(vis->GetVisualType()==I3D_VISUAL_LIT_OBJECT){
            CPI3D_mesh_base mb = vis->GetMesh();
            if(!mb)
               continue;
            /*
            C_str str;
            for(int i=mb->NumFGroups(); i--; ){
               PI3D_face_group fg = (PI3D_face_group)&mb->GetFGroups()[i];
               CPI3D_material mat = fg->GetMaterial();
               if(mat){
                  if(mat->GetAlpha()!=1.0f){
                     str = C_fstr("'%s' (alpha=%.2f)", "?", mat->GetAlpha());
                     break;
                  }
                  if(mat->IsCkey()){
                     str = C_fstr("'%s' (color-keyed)", "?");
                     break;
                  }
                  PI3D_texture tp = ((PI3D_material)mat)->GetTexture(MTI_DIFFUSE);
                  if(tp){
                     PIImage img = tp->GetSysmemImage();
                     if(img){
                        if(img->GetPixelFormat()->flags&PIXELFORMAT_ALPHA){
                           const char *fn = tp->GetFileName(0);
                           if(!fn)
                              fn = "<unknown>";
                           str = C_fstr("'%s': (texture '%s' has alpha channel)", "?", fn);
                           break;
                        }
                     }
                  }
               }
            }
            if(i!=-1){
               e_log->Action(E_LOG_ADD_TEXT, (void*)(const char*)
                  C_fstr("Lightmapped visual '%s' has invalid material '%s'", vis->GetName(), (const char*)str));
               e_slct->AddFrame(vis);
               ok = false;
            }
            */
            PI3D_lit_object lobj = I3DCAST_LIT_OBJECT(vis);
            if(!lobj->IsConstructed()){
               e_log->AddText(C_fstr("Lightmap '%s' not computed", (const char*)vis->GetName()));
               e_slct->AddFrame(vis);
               ok = false;
            }
         }
      }
      return ok;
   }

//----------------------------

   bool ValidateLights(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      bool ok = true;

      C_vector<PI3D_light> list;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword lst){
            C_vector<PI3D_light> *frm_list = (C_vector<PI3D_light>*)lst;
            frm_list->push_back(I3DCAST_LIGHT(frm));
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbAdd, (dword)&list, ENUMF_LIGHT);

      for(int i=list.size(); i--; ){
         PI3D_light lp = list[i];
         if(!lp->NumLightSectors()){
            e_log->AddText(C_fstr("Light in no sector: '%s'", (const char*)lp->GetName()));
            e_slct->AddFrame(lp);
            ok = false;
         }
         if(IsAbsMrgZero(lp->GetPower())){
            e_log->AddText(C_fstr("Light has zero power: '%s'", (const char*)lp->GetName()));
            e_slct->AddFrame(lp);
            ok = false;
         }
      }
      return ok;
   }

//----------------------------

   bool ValidateSounds(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      bool ok = true;

      C_vector<PI3D_sound> list;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword lst){
            C_vector<PI3D_sound> *frm_list = (C_vector<PI3D_sound>*)lst;
            frm_list->push_back(I3DCAST_SOUND(frm));
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbAdd, (dword)&list, ENUMF_SOUND);

      for(int i=list.size(); i--; ){
         PI3D_sound sp = list[i];
         PISND_source src = sp->GetSoundSource();
         if(src){
                              //check format
            const S_wave_format *fmt = src->GetFormat();
            if(fmt && fmt->num_channels>1){
                              //only ambient types may be stereo
               switch(sp->GetSoundType()){
               case I3DSOUND_NULL:
               case I3DSOUND_AMBIENT:
               case I3DSOUND_POINTAMBIENT:
                  break;
               default:
                  {
                     e_log->AddText(C_fstr("Non-ambient stereo sound: '%s'", (const char*)sp->GetName()));
                     e_slct->AddFrame(sp);
                     ok = false;
                  }
               }
            }
         }
      }
      return ok;
   }

//----------------------------

   bool ValidateSectors(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      bool ok = true;

                              //collect all sectors
      C_vector<PI3D_sector> sectors;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword lst){
            C_vector<PI3D_sector> *frm_list = (C_vector<PI3D_sector>*)lst;
            frm_list->push_back(I3DCAST_SECTOR(frm));
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbAdd, (dword)&sectors, ENUMF_SECTOR);
      S_hlp::cbAdd(ed->GetScene()->GetPrimarySector(), (dword)&sectors);
      S_hlp::cbAdd(ed->GetScene()->GetBackdropSector(), (dword)&sectors);

                              //check lights
      for(int i=sectors.size(); i--; ){
         PI3D_sector sct = sectors[i];
         C_vector<PI3D_light> fog_lights;
         for(int j=sct->NumLights(); j--; ){
            PI3D_light lp = sct->GetLight(j);
            switch(lp->GetLightType()){
            case I3DLIGHT_FOG:
               fog_lights.push_back(lp);
               break;
            }
         }
         if(fog_lights.size()>1){
            C_str light_names;
            const C_vector<PI3D_light> &list = fog_lights;
            for(int i=list.size(); i--; ){
               if(light_names.Size())
                  light_names += "; ";
               light_names = C_fstr("%s\"%s\"", (const char*)light_names, (const char*)list[i]->GetName());
            }
            C_fstr str("Sector has multiple fog lights: '%s' (%s)",
               (const char*)sct->GetName(), (const char*)light_names);

            e_log->AddText(str);
            e_slct->AddFrame(sct);
            ok = false;
         }
      }
      /*
      PC_editor_item e_debugline = ed->FindPlugin("DebugLine");
                              //check portals
      const float THRESH = .002f;

      for(i=sectors.size(); i--; ){
         PI3D_sector sct = sectors[i];
         if(sct->IsPrimary())
            continue;

         dword num_pl = sct->NumBoundPlanes();
         const S_plane *pls = sct->GetBoundPlanes();

         const PI3D_portal *prts = sct->GetPortals();
         for(int pi=sct->NumPortals(); pi--; ){
            PI3D_portal prt = *prts++;

                              //check if all points of portal are on edge of sector
            C_vector<bool> plane_ok(num_pl, true);
            int num_pl_ok = num_pl;

            dword numv = prt->NumVertices();
            const S_vector *verts = prt->GetVertices();
            for(int vi=numv; vi--; ){
               const S_vector &v = verts[vi];
                              //check if there's such plane on which we lie
               for(dword pli=num_pl; pli--; ){
                  if(!plane_ok[pli])
                     continue;
                  float d = v.DistanceToPlane(pls[pli]);
                  if(Fabs(d) > THRESH){
                     plane_ok[pli] = false;
                     --num_pl_ok;
                  }
               }
            }
            if(!num_pl_ok){
               C_fstr str("Sector '%s': sink portal",
                  (const char*)sct->GetName());

               e_log->Action(E_LOG_ADD_TEXT, (void*)(const char*)str);
               e_slct->AddFrame(sct);

               if(e_debugline){
                  for(vi=numv; vi--; )
                     e_debugline->Action(E_DEBUGLINE_ADD, &S_add_line(verts[vi], verts[(vi+1)%numv], 0xffff0000));
               }

               ok = false;

            }
         }
      }
      */
      return ok;
   }

//----------------------------

   bool ValidateLinks(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

                              //collect models and visuals
      C_vector<PI3D_frame> frms;
      C_vector<PI3D_sector> sectors;

      struct S_hlp{
         static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword c){
            C_vector<PI3D_frame> &v = *(C_vector<PI3D_frame>*)c;
            v.push_back(frm);
            return I3DENUMRET_OK;
         }
         static I3DENUMRET I3DAPI cbAddS(PI3D_frame frm, dword c){
            C_vector<PI3D_sector> &v = *(C_vector<PI3D_sector>*)c;
            v.push_back(I3DCAST_SECTOR(frm));
            return I3DENUMRET_OK;
         }

         static bool CheckSectorLink(PI3D_sector sct, const S_matrix &m, const I3D_bbox &bbox){

            S_vector bb_center = (bbox.min + bbox.max) * .5f;
            S_vector bb_center_world = bb_center * m;
                              //check using thresh
            const float thresh = .05f;
            for(int i=6; i--; ){
               S_vector pt_check = bb_center_world;
               pt_check[i/2] += (i&1) ? thresh : -thresh;
               bool is_inside = sct->CheckPoint(pt_check);
               if(is_inside)
                  break;
            }
            if(i==-1){
               S_vector bbox_full[8];
               bbox.Expand(bbox_full);
               for(i=8; i--; ){
                  S_vector check_point = bbox_full[i] * m;
                  bool is_iniside = sct->CheckPoint(check_point);
                  if(is_iniside)
                     break;
               }
               if(i==-1)
                  return false;
            }
            return true;
         }

         static bool IsOutOfSectors(C_vector<PI3D_sector> &sectors, const S_matrix &m, const I3D_bbox &bbox){

            S_vector bb_center = (bbox.min + bbox.max) * .5f;
            S_vector bb_center_world = bb_center * m;
                              //check using thresh
            const float thresh = .05f;
            for(int i=6; i--; ){
               S_vector pt_check = bb_center_world;
               pt_check[i/2] += (i&1) ? thresh : -thresh;
               for(int j=sectors.size(); j--; ){
                  if(sectors[j]->CheckPoint(pt_check))
                     break;
               }
               if(j==-1)
                  break;
            }
            if(i!=-1)
               return true;

            S_vector bbox_full[8];
            bbox.Expand(bbox_full);
            for(i=8; i--; ){
               S_vector check_point = bbox_full[i] * m;
               for(int j=sectors.size(); j--; ){
                  if(sectors[j]->CheckPoint(check_point))
                     break;
               }
               if(j==-1)
                  break;
            }
            if(i!=-1)
               return true;
            return false;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbAddS, (dword)&sectors, ENUMF_SECTOR);
      ed->GetScene()->EnumFrames(S_hlp::cbAdd, (dword)&frms, ENUMF_MODEL | ENUMF_VISUAL);

      bool ok = true;

      set<PI3D_frame> err_list;

                              //go from front, because parents are stored earlier than children
      for(dword i=0; i<frms.size(); i++){

         PI3D_frame frm = frms[i];
         switch(frm->GetType()){
         case FRAME_VISUAL:
         case FRAME_MODEL:
            {
                              //if any if our parent is already in err list, skip
               {
                  for(PI3D_frame prnt = frm; prnt=prnt->GetParent(), prnt; ){
                     if(err_list.find(prnt)!=err_list.end())
                        break;
                  }
                  if(prnt)
                     break;
               }

               const I3D_bound_volume &bvol =
                  (frm->GetType()==FRAME_VISUAL) ? I3DCAST_VISUAL(frm)->GetBoundVolume() :
                  I3DCAST_MODEL(frm)->GetHRBoundVolume();
               if(bvol.bbox.IsValid()){
                           //find its sectpr
                  PI3D_frame fsct = frm;
                  while((fsct=fsct->GetParent(), fsct) && fsct->GetType()!=FRAME_SECTOR);
                  assert(fsct);
                  if(!fsct)
                     break;
                  PI3D_sector sct = I3DCAST_SECTOR(fsct);

                  const S_matrix &m = frm->GetMatrix();
                  bool link_ok;
                  if(!sct->IsPrimary()){
                     link_ok = S_hlp::CheckSectorLink(sct, m, bvol.bbox);
                  }else{
                     link_ok = S_hlp::IsOutOfSectors(sectors, m, bvol.bbox);
                  }
                  if(!link_ok){
                           //possibly bad link, report
                     C_fstr str("Frame linked to wrong sector: '%s'", (const char*)frm->GetName());
                     e_log->AddText(str);
                     e_slct->AddFrame(frm);
                     ok = false;
                     err_list.insert(frm);
                  }
               }
            }
            break;
         }
      }


      /*
      C_vector<PI3D_sector> sectors;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword lst){
            C_vector<PI3D_sector> *frm_list = (C_vector<PI3D_sector>*)lst;
            frm_list->push_back(I3DCAST_SECTOR(frm));
            return I3DENUMRET_OK;
         }
         static I3DENUMRET I3DAPI cbAdd1(PI3D_frame frm, dword lst){
            C_vector<PI3D_frame> *frm_list = (C_vector<PI3D_frame>*)lst;
            frm_list->push_back(frm);
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbAdd, (dword)&sectors, ENUMF_SECTOR);

      for(int i=sectors.size(); i--; ){
         PI3D_sector sct = sectors[i];
                              //check all frames (visuals and models)
         C_vector<PI3D_frame> frm_list;
         sct->EnumFrames(S_hlp::cbAdd1, (dword)&frm_list, ENUMF_MODEL);
         for(int j=frm_list.size(); j--; ){
            PI3D_frame frm = frm_list[j];
            I3D_bound_volume bvol;
            frm->GetHRBoundVolume(&bvol);
            if(bvol.bbox.IsValid()){
               S_vector bb_center = bvol.bbox.min + (bvol.bbox.max - bvol.bbox.min) * .5f;
               S_vector bb_center_world = bb_center * frm->GetMatrix();
                              //check if center of geometry is inside of sector
               bool is_iniside = sct->CheckPoint(bb_center_world);
               if(!is_iniside){
                              //possibli bad link, report
                  C_fstr str("Model linked to wrong sector: '%s'", frm->GetName());
                  e_log->Action(E_LOG_ADD_TEXT, (void*)(const char*)str);
                  e_slct->AddFrame(frm);
                  ok = false;
               }
            }
         }
      }
      */
      return ok;
   }

//----------------------------

   bool ValidateVolumes(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      bool ok = true;

      C_vector<PI3D_volume> list;
      struct S_hlp{
         static I3DENUMRET I3DAPI cbAdd(PI3D_frame frm, dword lst){
            C_vector<PI3D_volume> *frm_list = (C_vector<PI3D_volume>*)lst;
            frm_list->push_back(I3DCAST_VOLUME(frm));
            return I3DENUMRET_OK;
         }
      };
      ed->GetScene()->EnumFrames(S_hlp::cbAdd, (dword)&list, ENUMF_VOLUME);

      for(int i=list.size(); i--; ){
         PI3D_volume vol = list[i];
         switch(vol->GetVolumeType()){
         case I3DVOLUME_SPHERE:
         case I3DVOLUME_RECTANGLE:
         case I3DVOLUME_BOX:
         case I3DVOLUME_CAPCYL:
         //case I3DVOLUME_CYLINDER:
            break;
         default:
            e_log->AddText(C_fstr("Unknown volume type: '%s'", (const char*)vol->GetName()));
            e_slct->AddFrame(vol);
            ok = false;
            continue;
         }

         const S_vector &pos = vol->GetPos();
         float scale = vol->GetScale();
         const S_vector &nu_scale = vol->GetNUScale();
         const S_quat &rot = vol->GetRot();
         for(int j=i; j-- ; ){
            PI3D_volume vol1 = list[j];
            if(vol->GetVolumeType()!=vol1->GetVolumeType())
               continue;
            if(vol->GetParent()!=vol1->GetParent())
               continue;
            const S_vector &pos1 = vol1->GetPos();
            if(pos==pos1 && scale==vol1->GetScale() && rot==vol1->GetRot() && nu_scale==vol1->GetNUScale()){

               e_log->AddText(C_fstr("Volume duplication on [%.3f, %.3f, %.3f] - '%s'",
                  pos.x, pos.y, pos.z, (const char*)vol1->GetName()));

               e_slct->AddFrame(vol1);
               ok = false;
            }
         }
      }
      return ok;
   }

//----------------------------
// Validate single maps directory. Return true if error found.
   bool ValidateMapsDir(const char *dir, map<C_str, C_str> &map_map, PC_editor_item_Log e_log){

      bool ok = true;

      C_buffer<C_str> files;
      OsCollectFiles(dir, "*.*", files, false, true);
      for(int i=files.size(); i--; ){
         C_str f = files[i];
         f.ToLower();
         for(dword ei=f.Size(); ei--; ){
            if(f[ei]=='.')
               break;
         }
         if(ei==-1)
            continue;
         C_str ext = &f[ei+1];
         if(ext.Match("png") || ext.Match("pcx") || ext.Match("bmp") || ext.Match("jpg")){
            C_str fdir;
            while(ei--){
               if(f[ei]=='\\'){
                  f[ei] = 0;
                  fdir = (const char*)f;
                  f = &f[ei+1];
                  break;
               }
            }
            map<C_str, C_str>::iterator it = map_map.find(f);
            if(it!=map_map.end()){
               e_log->AddText(C_fstr("Texture '%s' duplicated in '%s' and '%s'",
                  (const char*)f, (const char*)fdir, (const char*)(*it).second));
               ok = false;
            }else{
               map_map[f] = fdir;
            }
         }
      }
      return ok;
   }

//----------------------------
// Check files duplication in maps directories.
   bool ValidateMaps(PC_editor_item_Log e_log, PC_editor_item_Selection e_slct){

      bool ok = true;

      PI3D_driver drv = ed->GetDriver();

      map<C_str, C_str> map_map;
      for(int i=drv->NumDirs(I3DDIR_MAPS); i--; ){
         const C_str &dir = drv->GetDir(I3DDIR_MAPS, i);
         ok = ValidateMapsDir(dir, map_map, e_log) && ok;
      }
      return ok;
   }

//----------------------------

   static dword CountBits(dword val){
      dword mask = 0x80000000;
      dword num = 0;
      do{
         if(val&mask) ++num;
      }while(mask>>=1);
      return num;
   }

//----------------------------

   char res_copy_path[256];
public:
   C_edit_Validity(){
      res_copy_path[0] = 0;
   }

//----------------------------

   virtual bool Init(){

      ed->AddShortcut(this, ACTION_VALIDATE, "%30 &Debug\\%40 Scene Validity chec&k", K_NOKEY, 0);
      ed->AddShortcut(this, ACTION_RES_DUMP, "%30 &Debug\\%40 Resource dump", K_NOKEY, 0);
      {
         PC_toolbar tb = ed->GetToolbar("Debug");
         S_toolbar_button tbs[] = {
            {ACTION_VALIDATE, 0, "Valitity check"},
         };
         tb->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_VALIDITY", GetHInstance(), 30);
      }
      return true;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){}

//----------------------------

   virtual dword C_edit_Validity::Action(int id, void *context){

      switch(id){
      case ACTION_VALIDATE:              //scene validity
         {
            PC_editor_item_Log e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
            PC_editor_item_Selection e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
            if(!e_log || !e_slct){
               break;
            }
                              //clear log
            e_log->Clear();
                              //clear selection
            e_slct->Clear();

            PC_editor_item_DebugLine e_debugline = (PC_editor_item_DebugLine)ed->FindPlugin("DebugLine");
            if(e_debugline)
               e_debugline->ClearAll();

            bool ok = true;
            do{

               ed->Message("Checking models...", 0, EM_MESSAGE, true);
               ok = ValidateModels(e_log, e_slct);
               if(!ok) break;

               ed->Message("Checking visuals...", 0, EM_MESSAGE, true);
               ok = ValidateVisuals(e_log, e_slct);
               if(!ok) break;

               ed->Message("Checking lights...", 0, EM_MESSAGE, true);
               ok = ValidateLights(e_log, e_slct);
               if(!ok) break;

               ed->Message("Checking sounds...", 0, EM_MESSAGE, true);
               ok = ValidateSounds(e_log, e_slct);
               if(!ok) break;
                              //todo: check sector light ranges to be within sector

               ed->Message("Checking sectors...", 0, EM_MESSAGE, true);
               ok = ValidateSectors(e_log, e_slct);
               if(!ok) break;

               ed->Message("Checking volumes...", 0, EM_MESSAGE, true);
               ok = ValidateVolumes(e_log, e_slct);
               if(!ok) break;

               ed->Message("Checking links...", 0, EM_MESSAGE, true);
               ok = ValidateLinks(e_log, e_slct);
               if(!ok) break;

               ed->Message("Checking maps...", 0, EM_MESSAGE, true);
               ok = ValidateMaps(e_log, e_slct);
               if(!ok) break;

                              //broadcast validation message
               struct S_hlp{
                  static bool cbEnum(PC_editor_item ei, void *c){
                     if(!ei->Validate()){
                        *(bool*)c = false;
                        return false;
                     }
                     return true;
                  }
               };
               ed->EnumPlugins(S_hlp::cbEnum, &ok);
               if(!ok) break;

               ed->Message("Checking geometry...", 0, EM_MESSAGE, true);
               ok = ValidateGeometry(e_log, e_slct) && ok;
               if(!ok) break;
            }while(false);

            ed->Message(
               ok ? "Scene is OK!" :
               "Validity check failed!",
               0, EM_MESSAGE, true);
         }
         break;

      case ACTION_RES_DUMP:
         {
            PC_editor_item_Log e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
            PC_editor_item_Modify e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
            if(!e_log) break;

            struct S_hlp2{
               static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

                  switch(uMsg){
                  case WM_INITDIALOG:
                     {
                        const char *cp = (const char*)lParam;
                        SetDlgItemText(hwnd, IDC_EDIT, cp);
                        ShowWindow(hwnd, SW_SHOW);
                        SetWindowLong(hwnd, GWL_USERDATA, lParam);
                     }
                     return 1;
                  case WM_COMMAND:
                     switch(LOWORD(wParam)){
                     case IDCANCEL: EndDialog(hwnd, 0); break;
                     case IDOK:
                        SendDlgItemMessage(hwnd, IDC_EDIT, WM_GETTEXT,
                           256, GetWindowLong(hwnd, GWL_USERDATA));
                        EndDialog(hwnd, 1);
                        break;
                     }
                     break;
                  }
                  return 0;
               }
            };

            dword i = DialogBoxParam(GetHInstance(), "IDD_RESOURCE_DUMP", (HWND)ed->GetIGraph()->GetHWND(),
               S_hlp2::dlgProc, (LPARAM)res_copy_path);
            if(!i)
               break;

            int num_errs = 0;
            static const char para_break[] = "--------------";

                              //collect all frames
            C_vector<PI3D_model> uni_mod_list;
            struct S_hlp{
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

                  PI3D_model mod = I3DCAST_MODEL(frm);

                  C_vector<PI3D_model> &uni_mod_list = *(C_vector<PI3D_model>*)c;
                  int i;
                  for(i=uni_mod_list.size(); i--; ){
                     //if(!strcmp(uni_mod_list[i]->GetFileName(), mod->GetFileName()))
                     if(uni_mod_list[i]->GetFileName() == mod->GetFileName())
                        break;
                  }
                  if(i==-1)
                     uni_mod_list.push_back(mod);
                  return I3DENUMRET_OK;
               }
               static bool cbLess(PI3D_model m1, PI3D_model m2){
                  //return (strcmp(m1->GetFileName(), m2->GetFileName()) < 0);
                  return (m1->GetFileName() < m2->GetFileName());
               }
               static I3DENUMRET I3DAPI cbEnum1(PI3D_frame frm, dword c){
                  return I3DENUMRET_OK;
               }
               static bool cbLessTxt(PI3D_texture t1, PI3D_texture t2){
                  //return (strcmp(t1->GetFileName(), t2->GetFileName()) < 0);
                  return (t1->GetFileName() < t2->GetFileName());
               }
               
               static bool cbLessString(const C_str &s1, const C_str &s2){
                  return false;
               }
               static bool cbLessSnd(PI3D_sound s1, PI3D_sound s2){
                  //return (strcmp(s1->GetFileName(), s2->GetFileName()) < 0);
                  return (s1->GetFileName() < s2->GetFileName());
               }

               static I3DENUMRET I3DAPI cbEnumSnd(PI3D_frame frm, dword c){

                  PI3D_sound snd = I3DCAST_SOUND(frm);

                  C_vector<PI3D_sound> &uni_snd_list = *(C_vector<PI3D_sound>*)c;
                  int i;
                  for(i=uni_snd_list.size(); i--; ){
                     //if(!strcmp(uni_snd_list[i]->GetFileName(), snd->GetFileName()))
                     if(uni_snd_list[i]->GetFileName() == snd->GetFileName())
                        break;
                  }
                  if(i==-1)
                     uni_snd_list.push_back(snd);
                  return I3DENUMRET_OK;
               }
            };
            ed->GetScene()->EnumFrames(S_hlp::cbEnum, (dword)&uni_mod_list, ENUMF_MODEL);
                              //remove non-created models
            for(i=uni_mod_list.size(); i--; ){
               PI3D_model mod = uni_mod_list[i];
               dword flgs = e_modify->GetFrameFlags(mod);
               if(!(flgs&E_MODIFY_FLG_CREATE)){
                  uni_mod_list[i] = uni_mod_list.back();
                  uni_mod_list.pop_back();
               }
            }

                              //sort models by name
            sort(uni_mod_list.begin(), uni_mod_list.end(), S_hlp::cbLess);
            e_log->AddText("Loaded model files:");

                              //compute total size / copy models
            dword total_size = 0;
            for(i=0; i<uni_mod_list.size(); i++){
               C_str fname = "models\\";
               fname += uni_mod_list[i]->GetFileName();
               fname += ".i3d";
               dword size = GetFileSize(fname);
               e_log->AddText(C_fstr("\"%s\"\tsize: %i", (const char*)fname, size));
               if(size!=-1)
                  total_size += size;

               if(*res_copy_path && fname[0]!='+'){
                              //copy models to required directory
                  bool ok = false;

                  C_fstr dst_name("%s\\%s", res_copy_path, (const char*)fname);
                  if(OsCreateDirectoryTree(dst_name)){
                     int size = GetFileSize(fname);
                     if(size!=-1){
                        C_vector<byte> buf(size);
                        if(Load(fname, &buf.front(), size) == size){
                           if(Save(dst_name, &buf.front(), size) == size){
                              ok = true;
                           }
                        }
                     }
                  }
                  if(!ok){
                     e_log->AddText(C_fstr("Error: failed to copy model '%s' to '%s'", (const char*)fname, (const char*)dst_name),
                        0xffff0000);
                     ++num_errs;
                  }
               }
            }
            e_log->AddText(C_fstr("total size: %i", total_size));
            e_log->AddText(para_break);

                              //collect all objects
            C_vector<PI3D_frame> frm_list;
            struct S_hlp1{
               static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
                  C_vector<PI3D_frame> &frm_list = *(C_vector<PI3D_frame>*)c;
                  frm_list.push_back(frm);
                  return I3DENUMRET_OK;
               }
            };
            ed->GetScene()->EnumFrames(S_hlp1::cbEnum, (dword)&frm_list);

                              //collect unique meshes
            set<PI3D_mesh_base> mb_list;
            set<CPI3D_material> mat_list;

            for(i=frm_list.size(); i--; ){
               PI3D_frame frm = frm_list[i];
               switch(frm->GetType()){
               case FRAME_VISUAL:
                  {
                     PI3D_visual vis = I3DCAST_VISUAL(frm);
                     PI3D_mesh_base mb = vis->GetMesh();
                     if(mb){
                        mb_list.insert(mb);
                     }else{
                              //specialized visuals
                        switch(vis->GetVisualType()){
                        case I3D_VISUAL_PARTICLE:
                           mat_list.insert((PI3D_material)vis->GetProperty(I3DPROP_PRTC_MATERIAL));
                           break;
                        case I3D_VISUAL_FLARE:
                           break;
                        }
                     }
                  }
                  break;
               }
            }

                              //collect unique materials
            for(set<PI3D_mesh_base>::const_iterator it=mb_list.begin(); it!=mb_list.end(); it++){
               PI3D_mesh_base mb = *it;
               for(int j=mb->NumFGroups(); j--; ){
                  mat_list.insert(((PI3D_face_group)&mb->GetFGroups()[j])->GetMaterial());
               }
            }

                              //collect unique textures
            set<C_str> txt_name_list;
            set<CPI3D_material>::const_iterator mat_it;
            for(mat_it=mat_list.begin(); mat_it!=mat_list.end(); mat_it++){
               CPI3D_material mat = *mat_it;
               for(int mti=0; mti<MTI_LAST; mti++){
                  CPI3D_texture tp = ((PI3D_material)mat)->GetTexture((I3D_MATERIAL_TEXTURE_INDEX)mti);
                  if(tp){
                              //23-04-2003 jv: changed to 2, because we have only 2 filenames in tp
                     for(i=0; i < 2; i++){
                        const C_str &fname = tp->GetFileName(i);
                        if(fname.Size())
                           txt_name_list.insert(fname);
                     }
                  }
               }
            }

                              //dump textures
            {
               e_log->AddText("Loaded textures:");
               dword num_map_dirs = ed->GetDriver()->NumDirs(I3DDIR_MAPS);

               for(set<C_str>::const_iterator it=txt_name_list.begin(); it!=txt_name_list.end(); ++it){
                  const C_str &fname = *it;
                  e_log->AddText(fname);

                  if(*res_copy_path){
                              //copy map to required directory
                     bool ok = false;
      
                     for(i=0; i<num_map_dirs; i++){

                        const char *dir = ed->GetDriver()->GetDir(I3DDIR_MAPS, i);
                        C_fstr src_name("%s\\%s", dir, (const char*)fname);
                        C_fstr dst_name("%s\\%s\\%s", res_copy_path, dir, (const char*)fname);
                        if(OsCreateDirectoryTree(dst_name)){
                           int size = GetFileSize(src_name);
                           if(size!=-1){
                              C_vector<byte> buf(size);
                              if(Load(src_name, &buf.front(), size) == size){
                                 if(Save(dst_name, &buf.front(), size) == size){
                                    ok = true;
                                    break;
                                 }
                              }
                           }     
                        }
                     }
                     if(!ok){
                        const char *mat_name = "<unknown>";
                        for(mat_it=mat_list.begin(); mat_it!=mat_list.end(); mat_it++){
                           CPI3D_material mat = *mat_it;
                           for(int mti=MTI_LAST; mti--; ){
                              CPI3D_texture tp = ((PI3D_material)mat)->GetTexture((I3D_MATERIAL_TEXTURE_INDEX)mti);
                              if(tp){
                                 for(i=0; i<3; i++){
                                    const C_str &fn = tp->GetFileName(i);
                                    if(fn.Size() && fname==fn){
                                       //mat_name = mat->GetName();
                                       mti = 0;
                                       break;
                                    }
                                 }
                              }
                           }
                        }
                        e_log->AddText(C_fstr("Error: failed to copy texture '%s' (material '%s')", fname, mat_name),
                           0xffff0000);
                        ++num_errs;
                     }
                  }
               }
               e_log->AddText(para_break);
            }

                              //collect all sounds
            C_vector<PI3D_sound> uni_snd_list;
            ed->GetScene()->EnumFrames(S_hlp::cbEnumSnd, (dword)&uni_snd_list, ENUMF_SOUND);

                              //sort sounds by name
            sort(uni_snd_list.begin(), uni_snd_list.end(), S_hlp::cbLessSnd);
            e_log->AddText("Loaded model files:");

                              //compute total size / copy sounds
            total_size = 0;
            for(i=0; i<uni_snd_list.size(); i++){
               C_str fname = uni_snd_list[i]->GetFileName();
               dword size = GetFileSize(fname);
               e_log->AddText(C_fstr("\"%s\"\tsize: %i", (const char*)fname, size));
               if(size!=-1)
                  total_size += size;

               if(*res_copy_path){
                              //copy sounds to required directory
                  bool ok = false;

                  C_fstr dst_name("%s\\%s", res_copy_path, (const char*)fname);
                  if(OsCreateDirectoryTree(dst_name)){
                     int size = GetFileSize(fname);
                     if(size!=-1){
                        C_vector<byte> buf(size);
                        if(Load(fname, &buf.front(), size) == size){
                           if(Save(dst_name, &buf.front(), size) == size){
                              ok = true;
                           }
                        }
                     }
                  }
                  if(!ok){
                     e_log->AddText(C_fstr("Error: failed to copy sound '%s' to '%s'", (const char*)fname, (const char*)dst_name),
                        0xffff0000);
                     ++num_errs;
                  }
               }
            }
            e_log->AddText(C_fstr("total size: %i", total_size));
            e_log->AddText(para_break);

            if(*res_copy_path)
               e_log->AddText(C_fstr("errors: %i", num_errs));
         }
         break;
      }
      return 0;
   }

//----------------------------

   //virtual void Render(){}

//----------------------------

   virtual bool LoadState(C_chunk &ck){
                              //check version
      byte version = 0xff;
      ck.Read(&version, sizeof(byte));
      if(version!=1) return false;
                              //read other variables
      ck.Read(res_copy_path, sizeof(res_copy_path));
      return true; 
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{
                              //write version
      byte version = 1;
      ck.Write(&version, sizeof(byte));
                              //write other variables
      ck.Write(res_copy_path, sizeof(res_copy_path));
      return true; 
   }
};

//----------------------------

void CreateValidity(PC_editor ed){
   PC_editor_item ei = new C_edit_Validity;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
