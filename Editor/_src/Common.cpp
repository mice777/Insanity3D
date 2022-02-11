#include "all.h"
#include "Common.h"
#include "main.h"
#include <i3d\PoseAnim.h>

//----------------------------

#define OVERRIDE_F_TO_L       //override calls to _ftol with our (faster) version, or detect usage

//----------------------------
                              //linear interpolator for conversion of linear time
                              // to accelerated / deaccelerated time
const C_ease_interpolator ease_in_out(1.0f, 1.0f),
   ease_in(0.0f, 1.0f),
   ease_out(1.0f, 0.0f);

//----------------------------

#ifdef OVERRIDE_F_TO_L
extern "C" long __cdecl _ftol (float);

//#pragma warning(disable:4035)
__declspec(naked) long __cdecl _ftol (float f){
   __asm{
      push ecx
      push edx
   }
   ErrReport("_ftol called!", editor);
   //return FloatToInt(f - .5f);
   __asm{
      pop edx
      pop ecx
      push eax
      fistp dword ptr [esp]
      pop eax
      ret
   }
}
//#pragma warning(default:4035)
#endif   //OVERRIDE_F_TO_L

//----------------------------
// Get angle among 2 axes, return value is  in range <0, 2*PI>.
float GetAngle2D(float x, float y){

   if(!x){
      return (y>0.0f) ?
         PI *  .5f :
         PI * 1.5f;
   }
   if(x > 0.0f){
      float rtn = (float)atan(y / x);
      if(rtn < 0.0f) rtn += (PI*2.0f);
      return rtn;
   }else{
      float rtn = PI + (float)atan(-y / -x);
      if(rtn < 0.0f) rtn += (PI*2.0f);
      return rtn;
   }
}

//----------------------------

float GetAngle2Da(float x, float y){
   float a = GetAngle2D(x, y);
   if(a > PI) a -= (PI*2.0f);
   return a;
}

//----------------------------

int GetMaterialID(CPI3D_frame frm){

   assert(frm);
   switch(frm->GetType()){
   case FRAME_VOLUME:
      return I3DCAST_CVOLUME(frm)->GetCollisionMaterial();
   case FRAME_VISUAL:
      return I3DCAST_CVISUAL(frm)->GetCollisionMaterial();
   case FRAME_JOINT:
      while((frm = frm->GetParent(), frm) && (frm->GetType() != FRAME_VISUAL)){}
      return frm ? I3DCAST_CVISUAL(frm)->GetCollisionMaterial() : 0;
   default:
      assert(0);
      return 0;
   }
}

//----------------------------

bool ComputeMouseAiming(int rx, int ry, const S_vector &default_dir, float limit_angle,
   float look_speed, S_vector &curr_aim_dir){

   if(!(rx || ry)) return false;
   if(curr_aim_dir.IsNull()) return false;

   float zx_len = I3DSqrt(curr_aim_dir.x * curr_aim_dir.x + curr_aim_dir.z * curr_aim_dir.z);
   float zx_angle = GetAngle2D(curr_aim_dir.z, -curr_aim_dir.x);
   if(rx){
      zx_angle -= (float)rx*look_speed;
      if(zx_angle <= -PI) zx_angle += PI*2.0f;
      else
      if(zx_angle > PI) zx_angle -= PI*2.0f;
   }
   if(ry){
      float angle = GetAngle2D(zx_len, curr_aim_dir.y);
      angle -= (float)ry*look_speed;
      if(angle <= -PI) angle += PI*2.0f;
      else
      if(angle > PI) angle -= PI*2.0f;

      curr_aim_dir.y = (float)sin(angle);
      zx_len = (float)cos(angle);
   }
   curr_aim_dir.x = -(float)sin(zx_angle)*zx_len;
   curr_aim_dir.z = (float)cos(zx_angle)*zx_len;
                              //limit
   while(true){
      float a = curr_aim_dir.AngleTo(default_dir);
      if(a<=limit_angle) break;
      curr_aim_dir.Normalize();
      float f = limit_angle / a -.01f;
      curr_aim_dir = default_dir + (curr_aim_dir - default_dir) * f;
   }
   return true;
}

//----------------------------

void SetModelBrightness(PI3D_model mod, float brightness, I3D_VISUAL_BRIGHTNESS reg){

   const PI3D_frame *frms = mod->GetFrames();
   for(dword i=mod->NumFrames(); i--; ){
      PI3D_frame frm = *frms++;
      if(frm->GetType()==FRAME_VISUAL)
         I3DCAST_VISUAL(frm)->SetBrightness(reg, brightness);
   }
}

//----------------------------

float GetModelBrightness(CPI3D_model mod, I3D_VISUAL_BRIGHTNESS reg){

   const CPI3D_frame *frms = mod->GetFrames();
   for(dword i=mod->NumFrames(); i--; ){
      CPI3D_frame frm = *frms++;
      if(frm->GetType()==FRAME_VISUAL)
         return I3DCAST_CVISUAL(frm)->GetBrightness(reg);
   }
   return -1.0f;
}

//----------------------------

S_vector GetFrameLocalCenter(CPI3D_frame frm){

   assert(frm);
   
   S_vector loc_center_pos;
   I3D_bound_volume bvol_tmp;
   const I3D_bound_volume *bvol;
   switch(frm->GetType()){
   case FRAME_VISUAL:
      bvol = &I3DCAST_CVISUAL(frm)->GetHRBoundVolume();
      break;
   case FRAME_MODEL:
      bvol = &I3DCAST_CMODEL(frm)->GetHRBoundVolume();
      break;
   default:
      frm->ComputeHRBoundVolume(&bvol_tmp);
      bvol = &bvol_tmp;
   }
   if(!bvol->bbox.IsValid()){
      loc_center_pos.Zero();
   }else{
      loc_center_pos = bvol->bbox.min + (bvol->bbox.max - bvol->bbox.min) * .5f;
   }
   return loc_center_pos;
}

//----------------------------

PI3D_frame GetFrameParent(PI3D_frame frm, I3D_FRAME_TYPE type){

   while(frm->GetType() != type){
      frm = frm->GetParent();
      if(!frm)
         return NULL;
   }
   return frm;
}

//----------------------------

class C_actor *FindFrameActor(PI3D_frame frm1){

   if(frm1){
      do{
         C_actor *col_act = GetFrameActor(frm1);
         if(col_act)
            return col_act;
      }while((frm1->GetType() != FRAME_MODEL) && (frm1 = frm1->GetParent(), frm1));
   }
   return NULL;
}

//----------------------------

bool IsChildOf(CPI3D_frame frm, CPI3D_frame root){

   if(frm){
      do{
         if(frm == root)
            return true;
         frm = frm->GetParent();
      }while(frm);
   }
   return false;
}


//----------------------------

PI3D_sector GetFrameSector(PI3D_frame frm){
   return I3DCAST_SECTOR(GetFrameParent(frm, FRAME_SECTOR));
}

//----------------------------

void DeleteVolumes(PI3D_model mod){

   struct S_hlp{
      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
         ((C_vector<PI3D_frame>*)c)->push_back(frm);
         return I3DENUMRET_OK;
      }
   };
                              //collect all volumes first (owned and also linked)
   C_vector<PI3D_frame> frms;
   mod->EnumFrames(S_hlp::cbEnum, (dword)&frms, ENUMF_VOLUME);
   for(dword i=frms.size(); i--; ){
                              //try to remove frame first, if it fails, just set it off
      if(I3D_FAIL(mod->RemoveFrame(frms[i])))
         frms[i]->SetOn(false);
   }
}

//----------------------------

void EnableVolumes(PI3D_model mod, bool on){

   struct S_hlp{
      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
         frm->SetOn(c);
         return I3DENUMRET_OK;
      }
   };
   mod->EnumFrames(S_hlp::cbEnum, on, ENUMF_VOLUME);
}

//----------------------------

extern C_smart_ptr<C_class_to_be_ticked> tick_class;

class C_class_to_be_ticked *GetTickClass(){ return tick_class; }

void SetTickClass(C_class_to_be_ticked *tc){
   tick_class = tc;
}

//----------------------------

#ifdef EDITOR
void ErrReport(const char *cp, void *context){

                              //context is pointer to C_editor class
   PC_editor ed = (PC_editor)context;
   if(ed){
      PC_editor_item_Log ei_log = (PC_editor_item_Log)ed->FindPlugin("Log");
      if(ei_log)
         ei_log->AddText(cp, 0xffff0000);
   }
}

void LogReport(const char *cp, void *context, dword color){

                              //context is pointer to C_editor class
   PC_editor ed = (PC_editor)context;
   if(ed){
      PC_editor_item_Log ei_log = (PC_editor_item_Log)ed->FindPlugin("Log");
      if(ei_log)
         ei_log->AddText(cp, color);
   }
}
#endif

//----------------------------

void SaveScreenShot(){

   C_str name;
   OsGetComputerName(name);
                              //make unique number
   for(int i=0; i<1000; i++){
      C_fstr s("Shots\\%s %.3i.png", (const char*)name, i);
      PC_dta_stream dta = DtaCreateStream(s);
      if(!dta){
         name = s;
         break;
      }else{
         dta->Release();
      }
   }
   OsCreateDirectoryTree(name);
   PIImage bb_img = igraph->CreateBackbufferCopy(0, 0, true);
   if(bb_img){
      C_cache ck;
      if(ck.open(name, CACHE_WRITE)){
         bb_img->SaveShot(&ck, "png");
         ck.close();
      }
      bb_img->Release();
   }

#ifdef EDITOR
   if(editor) editor->Message(C_fstr(all_txt["msg_SaveShot"], (const char*)name));
#endif
}

//----------------------------
//----------------------------
// Cut name into two parts: scene_name & pose_name.
// Format of path is "scene_name\\pose_name".
static void ExtractSceneAndPose(const char *name, C_str &scene_name, C_str &pose_name){

   scene_name = name;
   for(dword i = scene_name.Size(); i--; ){
      if(scene_name[i] == '\\'){
         pose_name = &scene_name[i+1];
         scene_name[i] = 0;
         break;
      }
   }
   C_fstr bin_name("missions\\%s\\scene.bin", (const char*)scene_name);
   scene_name = bin_name;
}

//----------------------------

static bool LoadAnimFromScene(PI3D_animation_set as, const char *name, bool warn, S_vector *edit_pos = NULL, S_quat *edit_rot = NULL){

   assert(!as->NumAnimations());
   C_str scene_name, pose_name;
   ExtractSceneAndPose(name, scene_name, pose_name);
   pose_name.ToLower();
                              //open binary file
   C_chunk ck;
   if(!ck.ROpen(scene_name)){
      if(warn)
         ErrReport(C_fstr("Can't open animation file: '%s'.", name), editor);
      return false;
   }
                              //ascend to base chunk, skip everything except anims
   bool open_ok = true;
   bool anim_loaded = false;
   if(++ck == CT_BASECHUNK){
      while(ck){
         switch(++ck){
         case CT_POSE_ANIMS:
            while(open_ok && !anim_loaded && ck){
               switch(++ck){
               case CT_POSE_LAST_EDITED:
                  break;
               case CT_POSE_ANIM_HEADER:
                  {
                              //read anim name and skip if not match
                     bool read_this_anim(true);
                     while(open_ok && read_this_anim && ck){
                        switch(++ck){
                        case CT_POSE_ANIM_NAME:
                           {
                              C_str anim_name = ck.RStringChunk();
                              anim_name.ToLower();
                              if(anim_name != pose_name)
                                 read_this_anim = false;
                           }
                           break;
                        case CT_POSE_ANIM:
                           anim_loaded = S_pose_anim::LoadAnimSet(ck, as, driver, edit_pos, edit_rot);
                           --ck;
                           break;
                        default:
                              //skip edit info since we are not interested in it.
                           --ck;
                           break;
                        }
                     }
                  }
                  break;
               default:
                  assert(("Unknown chunk", 0));
               }
               --ck;
            }
            break;
         }
         if(!open_ok)
            break;
         if(anim_loaded)
            break;
         --ck;
      }
   }
   ck.Close();

   return anim_loaded;
}

//----------------------------

I3D_RESULT C_I3D_special_anim_cache::OpenFile(PI3D_animation_set as, const char *dir, const char *filename, dword flags,
   I3D_LOAD_CB_PROC *cb_proc, void *cb_context){

   assert(!*dir);
                              //check if sub-directory specified in filename
   for(int i=strlen(filename); i--; ){
      if(filename[i]=='\\')
         break;
   }
   const char *pose_dir;
   if(i==-1){
                              //load pose from current mission's directory
      if(!work_missions.size())
         return I3DERR_INVALIDPARAMS;
      pose_dir = work_missions.back()->GetName();
   }else{
      pose_dir = "Anims";
   }
                              //load anim from .bin file
   S_vector pos;
   S_quat rot;
   bool b = LoadAnimFromScene(as, C_fstr("%s\\%s", pose_dir, filename), (flags&I3DLOAD_ERRORS) ? true : false, &pos, &rot);
   if(b){
                              //create special pose, containing init pos & rot
      PI3D_anim_pose pose = (PI3D_anim_pose)driver->CreateAnimation(I3DANIM_POSE);
      pose->SetPos(&pos);
      pose->SetRot(&rot);
      as->AddAnimation(pose, "<edit pos+rot>");
      pose->Release();
      return I3D_OK;
   }
   return as->Open(C_fstr("Anims\\%s%s.i3d", dir, filename), flags, cb_proc, cb_context);
}

//----------------------------

I3D_RESULT C_I3D_model_cache_special::OpenFile(PI3D_model mod, const char *dir, const char *filename, dword flags,
   I3D_LOAD_CB_PROC *cb_proc, void *cb_context){

   if(strchr(filename, '/')){
      if(cb_proc && (flags&I3DLOAD_ERRORS))
         cb_proc(CBM_ERROR, (dword)(const char*)C_fstr("Invalid character in filename '%s'", filename), 0, cb_context);
      return I3DERR_NOFILE;
   }

   I3D_RESULT ret;

   if(filename[0]=='+'){
                              //load mission as model
                              // work on top mission
      if(!work_missions.size())
         return I3DERR_GENERIC;
      /*
      C_str tmp;
      if(!strnicmp(filename, "+effects\\", 9)){
         ErrReport(C_fstr("Model '%s' moved, save scene!", filename), editor);
         tmp = C_fstr("+%s", filename+9);
         filename = tmp;
      }
      */
      E_MISSION_IO mio = work_missions.back()->LoadModel(mod, C_fstr("+models\\%s", filename+1), cb_proc, cb_context);
      switch(mio){
      case MIO_CORRUPT: return I3DERR_FILECORRUPTED;
      case MIO_ACCESSFAIL: return I3DERR_GENERIC;
      case MIO_NOFILE: return I3DERR_NOFILE;
      case MIO_CANCELLED: return I3DERR_CANCELED; 
      }
      C_str fn = filename; fn.ToLower();
      mod->StoreFileName(fn);
      ret = I3D_OK;
   }else{

      ret = C_I3D_model_cache::OpenFile(mod, dir, filename, flags, cb_proc, cb_context);
#ifdef _DEBUG
      if(I3D_FAIL(ret)){
         PC_dta_stream s = DtaCreateStream(C_fstr("%s%s.mov", dir, filename));
         if(s){
            ErrReport(C_fstr("Model '%s' moved, save scene!", filename), editor);

            int sz = s->GetSize();
            C_vector<char> buf(sz+1); 
            s->Read(&(*buf.begin()), sz);
            buf[sz] = 0;
            s->Release();

            for(dword i=sz; i--; ){
               if(!isspace(buf[i]))
                  break;
               buf[i] = 0;
            }
            filename = &*buf.begin();
            ret = C_I3D_model_cache::OpenFile(mod, dir, filename, flags, cb_proc, cb_context);
            C_str fn = filename; fn.ToLower();
            mod->StoreFileName(fn);
            return ret;
         }
      }
#endif
      C_str fn = filename; fn.ToLower();
      mod->StoreFileName(fn);
   }
   /*
   if(I3D_SUCCESS(ret)){
                              //setup all volumes to be in standard category
      struct S_hlp{
         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
            PI3D_volume vol = I3DCAST_VOLUME(frm);
            //vol->SetCategoryBits(VOLCAT_STANDARD);
            return I3DENUMRET_OK;
         }
      };
      mod->EnumFrames(S_hlp::cbEnum, (dword)mod, ENUMF_VOLUME);
   }
   */
   return ret;
}

//----------------------------
//----------------------------

