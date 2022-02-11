#include "pch.h"
#include <i3d\camerapath.h>
#include <c_chunk.h>
#include <i3d\bin_format.h>
#include <algorithm>

//----------------------------
//----------------------------
                              //loaders:
static bool LoadCameraPath(class C_chunk &ck, S_camera_path_data &pd, const char *check_name){

   while(ck.Size()){
      switch(ck.RAscend()){
      case CT_CAMPATH_NAME:
         pd.name = ck.RStringChunk();
         if(check_name && !pd.name.Matchi(check_name))
            return false;
         break;
      case CT_CAMPATH_KEYS:
         {
            dword num_keys = 0;
            ck.Read(&num_keys, sizeof(dword));
            pd.keys.assign(num_keys, S_cam_path_key());
            dword key_i = 0;
            while(ck.Size()){
               switch(ck.RAscend()){
               case CT_CAMPATH_KEY:
                  {
                     S_cam_path_key &key = pd.keys[key_i];
                     dword note_index = 0;
                     while(ck.Size()){
                        switch(ck.RAscend()){
                        case CT_CAMPATH_POS:
                           key.pos = ck.RVectorChunk();
                           break;
                        case CT_CAMPATH_DIR:
                           key.dir = ck.RVectorChunk();
                           break;
                        case CT_CAMPATH_UPVECTOR:
                           key.up = ck.RVectorChunk();
                           break;
                        case CT_CAMPATH_NOTIFY: //old (compatibility)
                           key.notes[0].text = ck.RStringChunk();
                           break;
                        case CT_CAMPATH_NOTE:
                           if(note_index < CAMPATH_MAX_NOTES_PER_KEY){
                              S_cam_path_key::S_note &nt = key.notes[note_index++];
                              ck.Read(&nt.pos, sizeof(float));
                              nt.text = ck.ReadString();
                           }
                           ck.Descend();
                           break;
                        case CT_CAMPATH_SPEED:
                           key.speed = ck.RFloatChunk();
                           break;
                        case CT_CAMPATH_EASINESS:
                           key.easiness = ck.RFloatChunk();
                           break;
                              //obsolete:
                        case CT_CAMPATH_EASY_FROM:
                           //key.easy_from =
                              ck.RFloatChunk();
                           break;
                        case CT_CAMPATH_EASY_TO:
                           //key.easy_to =
                              ck.RFloatChunk();
                           break;
                           /*
                        case CT_CAMPATH_SMOOTH:
                           key.smooth = ck.RFloatChunk();
                           break;
                           */
                        case CT_CAMPATH_PAUSE:
                           key.pause = ck.RIntChunk();
                           break;
                        default: assert(0); ck.Descend();
                        }
                     }
                     ++key_i;
                     ck.Descend();
                  }
                  break;
               }
            }
            assert(key_i==num_keys);
            ck.Descend();
         }
         break;
      default: assert(0); ck.Descend();
      }
   }
   return true;
}

//----------------------------

bool LoadCameraPath(class C_chunk &ck, S_camera_path_data &pd){
   return LoadCameraPath(ck, pd, NULL);
}

//----------------------------

bool LoadCameraPathByName(class C_chunk &ck, const char *name, S_camera_path_data &path){

   while(ck.Size()){
      switch(ck.RAscend()){
      case CT_CAMPATH_LAST_EDITED:
      case CT_CAMPATH_CURR_EDIT_POS:
         ck.Descend();
         break;
      case CT_CAMERA_PATH:
         {
            if(LoadCameraPath(ck, path, name))
               return true;
            ck.Descend();
         }
         break;
      default: assert(0); ck.Descend();
      }
   }
   return false;
}

//----------------------------
//----------------------------

class C_easiness_interpolator{
                              //segment of cosine we operate on
   float curve_beg, curve_end, curve_delta;
                              //normalization values
   float norm_base, norm_mult;
   bool is_linear;
public:

//----------------------------
// Setup beginning and end easiness values. The values are in range 0.0 ... 1.0.
// The greater the value is, the more the keys are grouped at the beg/end.
   void Setup(float v0, float v1){
      curve_beg = PI*1.5f - PI*v0*.5f;
      curve_end = PI*1.5f + PI*v1*.5f;
      curve_delta = curve_end - curve_beg;
      is_linear = IsAbsMrgZero(curve_delta);
      if(!is_linear){
         float cos_beg = (float)cos(curve_beg);
         float cos_end = (float)cos(curve_end);
         norm_base = -cos_beg;
         float d = cos_end - cos_beg;
         norm_mult = !IsAbsMrgZero(d) ? (1.0f / d) : 1.0f;
      }
   }

   float operator[](float t) const{
                              //special case - linear interpolation
      if(is_linear)
         return t;
                              //get value inside of cosine segment
      float segment_pos = curve_beg + t * curve_delta;
                              //get cosine at this point
      float cos_val = (float)cos(segment_pos);
                              //normalize inside of the segment
      float val = (norm_base + cos_val) * norm_mult;
      return val;
   }
};

//----------------------------
//----------------------------

class C_camera_path_imp: public C_camera_path{

   dword curr_key;            //index of current key
   int curr_key_time;         //time from this key to next

   struct S_cpath_key{
      C_bezier_curve<S_vector> pos_to_next;
      C_bezier_curve<S_vector> dir_to_next;
      C_bezier_curve<S_vector> up_to_next;

      C_easiness_interpolator time_curve;
      int pause_time;         //how long we rest here
      int time_to_next;       //how long we travel to next key

      S_cam_path_key::S_note notes[CAMPATH_MAX_NOTES_PER_KEY];

      S_cpath_key(){}
      S_cpath_key(const S_cpath_key &n){ operator =(n); }
      S_cpath_key &operator =(const S_cpath_key &n){
         memcpy(this, &n, offsetof(S_cpath_key, notes));
         for(dword i=0; i<CAMPATH_MAX_NOTES_PER_KEY; i++)
            notes[i] = n.notes[i];
         return *this;
      }
   };

   C_vector<S_cpath_key> keys;
                              //frame to be animated on the path
   C_smart_ptr<I3D_frame> anim_obj;  

public:
   C_camera_path_imp():
      curr_key(0),
      curr_key_time(0)
   {
   }

   inline PI3D_frame GetAnimObject(){ return anim_obj; }
   inline CPI3D_frame GetAnimObject() const{ return anim_obj; }

//----------------------------
// Build bezier path from scene.
   virtual bool Build(const S_camera_path_data&);

//----------------------------

   virtual void Close(){

      keys.clear();
      SetPosition(0);
   }

//----------------------------

   void DebugRender(PI3D_scene);

//----------------------------

   virtual int GetPosition() const{

      dword ret = curr_key_time;
      for(dword i=curr_key; i--; )
         ret += keys[i].time_to_next;
      return ret;
   }

//----------------------------

   virtual bool SetPosition(int time){

      if(keys.size() < 2){
         curr_key = 0;
         curr_key_time = 0;
         return false;
      }

      if(time==-1){
         curr_key = keys.size() - 2;
         curr_key_time = keys[curr_key].time_to_next + keys[curr_key].pause_time;
      }else{
                                 //todo: compute proper positions
         curr_key_time = time;
         curr_key = 0;
         while(curr_key_time > keys[curr_key].time_to_next){
            curr_key_time -= keys[curr_key].time_to_next;
            ++curr_key;
         }
         assert(curr_key < keys.size()-1);
      }
      return true;
   }

//----------------------------

   virtual bool SetKeyPosition(dword key_index){

      if(keys.size() < 2){
         curr_key = 0;
         curr_key_time = 0;
         return false;
      }
      if(key_index > keys.size()-2)
         return false;
      curr_key = key_index;
      curr_key_time = 0;
      return true;
   }

//----------------------------

   virtual dword GetCurrentKey(dword *key_time = NULL) const{

      if(key_time)
         *key_time = curr_key_time;
      return curr_key;
   }

//----------------------------

   virtual dword GetKeyTime(dword key_index) const{

      if(key_index >= keys.size())
         return 0xffffffff;
      return keys[key_index].time_to_next;
   }

//----------------------------

   virtual bool ComputePosition(dword key_index, float ratio_to_next, S_vector *pos,
      S_vector *dir = NULL, S_vector *upv = NULL) const{

                              //check if valid index provided
      if(key_index >= keys.size())
         return false;
                              //check if range is valid
      if(ratio_to_next<0.0f || ratio_to_next>1.0f)
         return false;

                              //on last key, functionality is limited
      if(key_index==keys.size()-1){
                              //fail if beyond last key requested
         if(ratio_to_next)
            return false;
         if(!key_index)
            return false;
                              //return values directly from last key
         const S_cpath_key &key = keys[key_index-1];
         if(pos)
            *pos = key.pos_to_next[3];
         if(dir)
            *dir = key.dir_to_next[3];
         if(upv)
            *upv = key.up_to_next[3];
         return true;
      }
      const S_cpath_key &key = keys[key_index];
      
                              //fail if in-between zero-time (teleport) key position requested
      if(ratio_to_next && ratio_to_next!=1.0f && !key.time_to_next)
         return false;
      float t = key.time_curve[ratio_to_next];
      if(pos)
         key.pos_to_next.Evaluate(t, *pos);
      if(dir)
         key.dir_to_next.Evaluate(t, *dir);
      if(upv)
         key.up_to_next.Evaluate(t, *upv);
      return true;
   }

//----------------------------
// Evaluate current position based on elapsed time. Call notify callback func for each encountered callback.
// Store resulting position and rotation in given 'pos' and 'rot' params.
   bool Tick(int time, S_vector &pos, S_vector &dir, S_vector &up, t_NotifyCallback* = NULL, void *context = NULL, 
      bool edit_mode = false);

//----------------------------
// Check if animation is in running mode.
   inline bool IsAtEnd() const{

      /*
      if(curr_key<keys.size()-1)
         return false;
      int key_end = keys[curr_key].time_to_next + keys[curr_key].pause_time;
      assert(curr_key_time <= key_end);
      return (curr_key_time==key_end);
      */
      return (curr_key>=keys.size()-2 && curr_key_time==(keys[curr_key].time_to_next + keys[curr_key].pause_time));
   }
};

//----------------------------

bool C_camera_path_imp::Build(const S_camera_path_data &path){

   keys.clear();

   if(path.keys.size()<2)
      return false;

   dword num = path.keys.size();
   keys.assign(num, S_cpath_key());

   struct S_temp_key{
      S_vector pos, up, dir;
      S_vector tan_pos_in, tan_pos_out;
      S_vector tan_dir_in, tan_dir_out;
      S_vector tan_up_in, tan_up_out;

      dword pause_time;       //how long we rest here
   };
   S_temp_key *tmp_keys = new S_temp_key[num];

                              //compute tangents among keys
   for(dword ni=0; ni<num; ni++){
      const S_cam_path_key *prev = &path.keys[Max(0, (int)ni-1)];
      const S_cam_path_key *next = &path.keys[Min(num-1, ni+1)];
      const S_cam_path_key *curr = &path.keys[ni];
      if(!curr->speed)
         next = curr;
      if(!prev->speed)
         prev = curr;

      S_cpath_key &key = keys[ni];
      S_temp_key &tmp = tmp_keys[ni];

                              //copy common values
      key.pause_time = curr->pause;
      for(dword i=0; i<CAMPATH_MAX_NOTES_PER_KEY; i++)
         key.notes[i] = curr->notes[i];
                              //sort note keys by time
      sort(key.notes, key.notes+CAMPATH_MAX_NOTES_PER_KEY);

      //const float smooth = curr->smooth;
      const float smooth = 1.0f - curr->easiness;

      const S_vector dir_to_next = next->pos - curr->pos;
      const S_vector dir_to_prev = prev->pos - curr->pos;
      const float dist_to_next = dir_to_next.Magnitude();
      const float dist_to_prev = dir_to_prev.Magnitude();

                              //compute position
      {
         const S_normal dir_prev_to_next = next->pos - prev->pos;

         tmp.tan_pos_in = curr->pos;
         tmp.tan_pos_in -= dir_prev_to_next * (dist_to_prev * smooth / 3.0f);

         tmp.tan_pos_out = curr->pos;
         tmp.tan_pos_out += dir_prev_to_next * (dist_to_next * smooth / 3.0f);
      }

                              //compute direction
      {
         const S_vector dir_to_next = next->dir - curr->dir;
         const S_vector dir_to_prev = prev->dir - curr->dir;
         const float dist_to_next = dir_to_next.Magnitude();
         const float dist_to_prev = dir_to_prev.Magnitude();
         const S_normal dir_prev_to_next = next->dir - prev->dir;

         tmp.tan_dir_in = curr->dir;
         tmp.tan_dir_in -= dir_prev_to_next * (dist_to_prev * smooth / 3.0f);

         tmp.tan_dir_out = curr->dir;
         tmp.tan_dir_out += dir_prev_to_next * (dist_to_next * smooth / 3.0f);
      }

                              //compute up C_vector
      {
         const S_vector dir_to_next = next->up - curr->up;
         const S_vector dir_to_prev = prev->up - curr->up;
         const float dist_to_next = dir_to_next.Magnitude();
         const float dist_to_prev = dir_to_prev.Magnitude();
         const S_normal dir_prev_to_next = next->up - prev->up;

         tmp.tan_up_in = curr->up;
         tmp.tan_up_in -= dir_prev_to_next * (dist_to_prev * smooth / 3.0f);

         tmp.tan_up_out = curr->up;
         tmp.tan_up_out += dir_prev_to_next * (dist_to_next * smooth / 3.0f);
      }

      {
                              //compute travel speed and easiness
         key.time_to_next = 0;
         if(!IsMrgZeroLess(curr->speed)){
            key.time_to_next = FloatToInt(1000.0f * dist_to_next / curr->speed);
            for(dword i=0; i<CAMPATH_MAX_NOTES_PER_KEY; i++)
               key.notes[i].pos *= key.time_to_next;
         }
         for(dword i=0; i<CAMPATH_MAX_NOTES_PER_KEY; i++)
            key.notes[i].pos += key.pause_time;

         float ei = 1.0f;
         float eo = 1.0f;

         if(prev->speed && curr->speed > prev->speed){
            eo *= 1.0f + cos((prev->speed / curr->speed) * PI*.5f + PI);
         }
         if(next->speed && curr->speed > next->speed){
            ei *= 1.0f + cos((next->speed / curr->speed) * PI*.5f + PI);
         }
         key.time_curve.Setup(1.0f - eo, 1.0f - ei);
      }
   }
                              //compute bezier curves of keys
   for(ni=0; ni<num-1; ni++){
      S_cpath_key &key = keys[ni];
      const S_temp_key &tmp_key = tmp_keys[ni];
      const S_temp_key &tmp_next = tmp_keys[ni+1];
      const S_cam_path_key &in_next = path.keys[ni+1];
      const S_cam_path_key &in_key = path.keys[ni];

      key.pos_to_next.Init(in_key.pos, tmp_key.tan_pos_out, tmp_next.tan_pos_in, in_next.pos);
      key.dir_to_next.Init(in_key.dir, tmp_key.tan_dir_out, tmp_next.tan_dir_in, in_next.dir);
      key.up_to_next.Init(in_key.up, tmp_key.tan_up_out, tmp_next.tan_up_in, in_next.up);
   }

   delete[] tmp_keys;
   return true;
}

//----------------------------

void C_camera_path_imp::DebugRender(PI3D_scene scene){

                              //debug draw entire bezier curve
   scene->SetRenderMatrix(I3DGetIdentityMatrix());
   static const dword color_tan = 0x404cff4c;
   static const dword color_pos = 0x9098ffff;
   static const dword color_dir = 0x50ffff4c;
   static const dword color_up  = 0x508080ff;
   int num = keys.size();
   for(int ni=0; ni<num-1; ni++){
      const S_cpath_key &key = keys[ni];

      if(!key.time_to_next){
                           //zero speed, direct link
         //scene->DrawLine(key.pos, keys[ni+1].pos, 0x20000000 | (color_pos&0xffffff));
         scene->DrawLine(key.pos_to_next[0], keys[ni].pos_to_next[3], 0x20000000 | (color_pos&0xffffff));
      }else{
                           //draw key's tangents
         /*
         scene->DrawLine(key.pos_to_next[0], key.pos_to_next[1], color_tan);
         scene->DrawLine(key.pos_to_next[1], key.pos_to_next[2], color_tan);
         scene->DrawLine(key.pos_to_next[2], key.pos_to_next[3], color_tan);
         */
                           //draw connection between key and look-at
      //scene->DrawLine(key.pos, key.look_at, color_con);

         S_vector curr_pos = key.pos_to_next[0];
         const float step = .1f;
         for(float f=0; f<=1.0f; f += step){
                              //draw position
            S_vector next_pos;
            key.pos_to_next.Evaluate(f+step, next_pos);
                              //draw connection among those two
            scene->DrawLine(curr_pos, next_pos, color_pos);
            curr_pos = next_pos;
         }
      }
      scene->DrawLine(key.pos_to_next[0], key.pos_to_next[0] + key.dir_to_next[0] * 5.0f, color_dir);
      scene->DrawLine(key.pos_to_next[0], key.pos_to_next[0] + key.up_to_next[0]  * 2.0f, color_up);
      if(ni==num-2){
         scene->DrawLine(key.pos_to_next[3], key.pos_to_next[3] + key.dir_to_next[3] * 5.0f, color_dir);
         scene->DrawLine(key.pos_to_next[3], key.pos_to_next[3] + key.up_to_next[3]  * 2.0f, color_up);
      }
   }
}

//----------------------------

bool C_camera_path_imp::Tick(int time, S_vector &pos, S_vector &dir, S_vector &up, t_NotifyCallback *cb_proc, void *context,
   bool edit_mode){

   if(keys.size() < 2)
      return false;
   curr_key = Min(curr_key, (dword)keys.size()-1);

//----------------------------
   float beg_time = curr_key_time;
                              //adjust time and key index
   curr_key_time += time;
   dword compute_time = curr_key_time;
   dword compute_key_index = curr_key;

   if(time > 0){
      int key_time;
      bool done = false;
      bool pass_next = true;
      while((key_time = keys[curr_key].time_to_next + keys[curr_key].pause_time, curr_key_time > key_time) && pass_next){

                              //process all rest notifications of this key
         if(cb_proc){
            for(dword ni=0; ni<CAMPATH_MAX_NOTES_PER_KEY; ni++){
               const S_cam_path_key::S_note &nt = keys[curr_key].notes[ni];
               if(nt.text.Size() && nt.pos>=beg_time){
                  bool pass = (*cb_proc)(nt.text, context);
                  if(!pass){
                              //stop at this key
                     pass_next = false;
                     curr_key_time = key_time;

                     compute_key_index = curr_key;
                     compute_time = FloatToInt(nt.pos);
                  }
               }
            }
         }
         beg_time = 0.0f;

         curr_key_time -= key_time;
         if(++curr_key == keys.size()-1){
            SetPosition(-1);
            done = true;
            break;
         }
      }
      if(pass_next){
         compute_key_index = curr_key;
         compute_time = curr_key_time;
      }

                              //process all rest notifications of this key
      if(cb_proc && !done && pass_next){
         for(dword ni=0; ni<CAMPATH_MAX_NOTES_PER_KEY; ni++){
            const S_cam_path_key::S_note &nt = keys[curr_key].notes[ni];
            if(nt.text.Size() && nt.pos>=beg_time && nt.pos<curr_key_time){
               bool pass = (*cb_proc)(nt.text, context);
               if(!pass){
                              //stop at this key
                  compute_key_index = curr_key;
                  compute_time = FloatToInt(nt.pos);
                  curr_key_time = compute_time + 1;
               }
            }
         }
      }
   }else
   if(time < 0){
      while(curr_key_time < 0){
         if(--curr_key == -1){
            SetPosition(0);
            break;
         }
         curr_key_time += keys[curr_key].time_to_next;
         curr_key_time += keys[curr_key].pause_time;
      }
      compute_key_index = curr_key;
      compute_time = curr_key_time;
   }
   const S_cpath_key &key = keys[compute_key_index];

   if(curr_key_time < key.pause_time){
      pos = key.pos_to_next[0];
      dir = key.dir_to_next[0];
      up = key.up_to_next[0];
   }else{
                              //bezier-interpolate position and look-at
      float f;
      if(key.time_to_next){
         f = (float)(compute_time - key.pause_time) / (float)key.time_to_next;
      }else
         f = 0.0f;
      f = key.time_curve[f];
      assert(f >= 0.0f && f <= 1.0f);
      key.pos_to_next.Evaluate(f, pos);
      key.dir_to_next.Evaluate(f, dir);
      key.up_to_next.Evaluate(f, up);
   }
   return true;
}

//----------------------------

C_camera_path *CreateCameraPath(){ return new C_camera_path_imp; }

//----------------------------
