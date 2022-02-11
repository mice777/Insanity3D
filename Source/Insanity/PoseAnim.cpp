#include "pch.h"
#include <i3d\PoseAnim.h>
#include <C_chunk.h>
#include <i3d\bin_format.h>

//----------------------------
//----------------------------

typedef map<C_str, float> t_weight_map;

//----------------------------

S_pose_anim::S_pose_anim():
   weight_map(NULL)
{
   edit_pos.Zero();
   edit_rot.Identity();
}

//----------------------------

S_pose_anim::~S_pose_anim(){
   if(weight_map)
      delete (t_weight_map*)weight_map;
}

//----------------------------

S_pose_anim::S_pose_anim(const S_pose_anim &pa):
   weight_map(NULL)
{ 
   operator =(pa);
}

//----------------------------

S_pose_anim &S_pose_anim::operator =(const S_pose_anim &pa){
   key_list = pa.key_list;
   edit_pos = pa.edit_pos;
   edit_rot = pa.edit_rot;
   if(weight_map){
      delete (t_weight_map*)weight_map;
      weight_map = NULL;
   }
   if(pa.weight_map){
      t_weight_map *wm = new t_weight_map;
      weight_map = wm;
      *wm = *(t_weight_map*)pa.weight_map;
   }
   return *this;
}

//----------------------------

void S_pose_anim::SetWeight(const C_str &link, float f){

   if(!weight_map)
      weight_map = new t_weight_map;
   t_weight_map *wm = (t_weight_map*)weight_map;
   (*wm)[link] = f;
}

//----------------------------

float S_pose_anim::GetWeight(const C_str &link) const{

   if(!weight_map)
      return 1.0f;
   t_weight_map *wm = (t_weight_map*)weight_map;
   t_weight_map::const_iterator it = wm->find(link);
   if(it != wm->end())
      return (*it).second;
   return 1.0f;
}

//----------------------------

void S_pose_anim::EnumWeights(void(*EnumProc)(const C_str &link, float val, dword context), dword context) const{

   if(!weight_map)
      return;
   t_weight_map *wm = (t_weight_map*)weight_map;
   for(t_weight_map::const_iterator it=wm->begin(); it!=wm->end(); it++)
      EnumProc(it->first, it->second, context);
}

//----------------------------

void S_pose_anim::GetAnimFrames(C_buffer<C_str> &names_list) const{

   names_list.clear();

   if(!key_list.size())
      return;

   C_vector<C_str> _names_list;
                           //we assume that all keys contain the same set of frame links, so we read from 1st key
   const S_pose_key &pk = key_list[0];
   for(int i = pk.anim_set->NumAnimations(); i--; ){
      const C_str &link = pk.anim_set->GetAnimLink(i);
      _names_list.push_back(link);
   }
   names_list.assign(&_names_list.front(), (&_names_list.back())+1);
}

//----------------------------

bool S_pose_anim::ConvertToAnimSet(PI3D_animation_set as, PI3D_driver drv) const{

   assert(as);
   assert(!as->NumAnimations());
                           //no frames to anim
                           //no keys
   if(!key_list.size())
      return false;
                           //one key, creat pose anim
   if(key_list.size() == 1){
      const S_pose_key &pk = key_list.front();
      for (int k = pk.anim_set->NumAnimations(); k--;){
         const C_str &link = pk.anim_set->GetAnimLink(k);
         if(!IsInFramesList(link))
            continue;

         CPI3D_animation_base anim = ((PI3D_animation_set)(CPI3D_animation_set)pk.anim_set)->GetAnimation(k);
         assert(anim->GetType() == I3DANIM_POSE);
         PI3D_anim_pose source_anim = (PI3D_anim_pose)anim;

         PI3D_anim_pose new_anim = (PI3D_anim_pose)drv->CreateAnimation(I3DANIM_POSE);
         new_anim->SetPos(source_anim->GetPos());
         new_anim->SetRot(source_anim->GetRot());
         new_anim->SetPower(source_anim->GetPower());
         float weight = GetWeight(link);
         as->AddAnimation(new_anim, link, weight);
         new_anim->Release();
      }
      return true;
   }
                           //keyframe
   typedef C_vector<I3D_anim_pos_tcb> t_pos_keys;
   typedef C_vector<I3D_anim_quat> t_rot_keys;
   typedef C_vector<I3D_anim_power> t_pow_keys;
   typedef C_vector<I3D_anim_note> t_note_keys;

   typedef C_vector<S_key_inf> t_pose_vect;
   map<C_str, t_pose_vect> pose_key_map;
   typedef map<C_str, t_note_keys> t_note_map;
   t_note_map note_map;
   typedef map<C_str, PI3D_keyframe_anim> t_anim_map;
   t_anim_map anim_map;

                           //create keyframe
   for(dword j = 0; j < key_list.size(); j++){
      const S_pose_key &pk = key_list[j];
      for(int k = pk.anim_set->NumAnimations(); k--;){
         const C_str &a_link = pk.anim_set->GetAnimLink(k);
         if(!IsInFramesList(a_link))
            continue;
         CPI3D_animation_base anim = ((PI3D_animation_set)(CPI3D_animation_set)pk.anim_set)->GetAnimation(k);
         assert(anim->GetType() == I3DANIM_POSE);
         PI3D_anim_pose ap = (PI3D_anim_pose)anim;

         t_pose_vect &pv = pose_key_map[a_link];
         pv.push_back(S_key_inf());
         S_key_inf &ki = pv.back();
         ki.key_id = j;
         ki.ap = ap;
         //pose_key_map[link].push_back(ki);
      }
                           //push note to map
      if(pk.notify.Size()){

         C_str frm_link = pk.notify_link;
         if(!pk.notify_link.Size()){
                           //find any first frame which have anim
            if(pose_key_map.size())
               frm_link = pose_key_map.begin()->first;
         }
                           //ignore if there is no anim
         if(frm_link.Size()){
            t_note_keys &nk = note_map[frm_link];

            nk.push_back(I3D_anim_note());
            I3D_anim_note &new_note = nk.back();
            new_note.time = pk.time;
            new_note.note = pk.notify;
         }
      }
   }

                           //convert to keyframes:
   for(map<C_str, t_pose_vect>::const_iterator it = pose_key_map.begin(); it != pose_key_map.end(); it++){

      const t_pose_vect &pose_keys = (t_pose_vect) (*it).second;
      dword num_keys = pose_keys.size();

      t_pos_keys pos_keys;
      pos_keys.reserve(num_keys);
      t_rot_keys rot_keys;
      rot_keys.reserve(num_keys);
      t_pow_keys pow_keys;
      pow_keys.reserve(num_keys);

      for(dword i = 0; i < num_keys; i++){
         const S_key_inf &inf = pose_keys[i];
         PI3D_anim_pose ap = inf.ap;
         assert(inf.key_id < key_list.size());
         assert(inf.key_id == i); //should be
         const S_pose_key &key = key_list[inf.key_id];

                           //pos
         if(ap->GetPos()){
            pos_keys.push_back(I3D_anim_pos_tcb());
            I3D_anim_pos_tcb &pos_tbc = pos_keys.back();
            pos_tbc.tension = .0f;
            //pos_tbc.continuity = Max(.0f, Min(1.0f, (1.0f - key.smooth)));
                              //set continuity <-1, 0> (-1 for sharp, 0 for smooth)
            pos_tbc.continuity = (-1.0f + key.smooth);
            pos_tbc.continuity = Min(Max(-1.0f, pos_tbc.continuity), 0.0f);
            pos_tbc.bias = .0f;
            pos_tbc.easy_from = key.easy_from;
            pos_tbc.easy_to = key.easy_to;
            pos_tbc.time = key.time;
            pos_tbc.v = *ap->GetPos();
         }

                           //rot:
         if(ap->GetRot()){
            rot_keys.push_back(I3D_anim_quat());
            I3D_anim_quat &rot_a = rot_keys.back();
            rot_a.time = key.time;
            rot_a.easy_from = key.easy_from;
            rot_a.easy_to = key.easy_to;
            rot_a.smoothness = Max(.0f, Min(1.0f, key.smooth));
            rot_a.q = *ap->GetRot();
         }

                           //power:
         if(ap->GetPower()){
            pow_keys.push_back(I3D_anim_power());
            I3D_anim_power &pow_a = pow_keys.back();
            pow_a.time = key.time;
            pow_a.easy_from = key.easy_from;
            pow_a.easy_to = key.easy_to;
            pow_a.power = *ap->GetPower();
         }
      }

                           //create anim only when some track is going to be created
      if(pos_keys.size() || rot_keys.size() || pow_keys.size()){
         PI3D_keyframe_anim kf_anim = (PI3D_keyframe_anim)drv->CreateAnimation(I3DANIM_KEYFRAME);
         kf_anim->SetPositionKeys(&(*pos_keys.begin()), pos_keys.size());
         kf_anim->SetRotationKeys1(&(*rot_keys.begin()), rot_keys.size());
         kf_anim->SetPowerKeys(&(*pow_keys.begin()), pow_keys.size());
         kf_anim->SetEndTime(GetEndTime());

         const C_str &link = (*it).first;
         float weight = GetWeight(link);

         as->AddAnimation(kf_anim, link, weight);
         kf_anim->Release();
                              //store for fast lookup when adding note keys
         anim_map[link] = kf_anim;
      }
   }
                           //add note tracks
   for (t_note_map::const_iterator it2 = note_map.begin(); it2 != note_map.end(); it2++){

      const C_str &link = (*it2).first;
                           //find/create anim
      t_anim_map::iterator it1 = anim_map.find(link);
      PI3D_keyframe_anim kf_anim;
      if(it1==anim_map.end()){
         kf_anim = (PI3D_keyframe_anim)drv->CreateAnimation(I3DANIM_KEYFRAME);
         as->AddAnimation(kf_anim, link);
         kf_anim->Release();
         anim_map[link] = kf_anim;
      }else
         kf_anim = (*it1).second;

      const t_note_keys &note_keys = (*it2).second;
                           //set note key nees non constant, cast due qualifier
      I3D_anim_note *p_note = const_cast<I3D_anim_note *>(&(*note_keys.begin()));
                           //add notify keys for this link
      kf_anim->SetNoteKeys(p_note, note_keys.size());
      kf_anim->SetEndTime(GetEndTime());
   }
   return true;
}

//----------------------------

bool S_pose_anim::Load(class C_chunk &ck, I3D_driver *drv){

   bool b = false;
   try{
      b = LoadInternal(ck, drv);
   }
   catch(...){
      return false;
   }
   return b;
}

//----------------------------

bool S_pose_anim::LoadInternal(C_chunk &ck, PI3D_driver drv){

   C_vector<C_str> names_list;
   C_vector<S_pose_key> _key_list;
                           //read all chunks
   while(ck){
      switch(++ck){
      case CT_POSE_ANIM_FRAMES:
         {
            names_list.clear();
            while(ck)
            switch(++ck){
            case CT_POSE_ANIM_FRAME_LINK:
               names_list.push_back(ck.RStringChunk());
               break;
            default:
               assert(0);
               --ck;
            }
         }
         break;

      case CT_POSE_ANIM_KEYS:
         while(ck){
            switch(++ck){
            case CT_POSE_ANIM_KEY:
               {
                  _key_list.push_back(S_pose_key());
                  S_pose_key &pk = _key_list.back();

                  while(ck)
                  switch(++ck){
                  case CT_POSE_KEY_TIME:
                     pk.time = ck.RIntChunk();
                     break;
                  case CT_POSE_KEY_EASY_FROM:
                     pk.easy_from = ck.RFloatChunk();
                     break;
                  case CT_POSE_KEY_EASY_TO:
                     pk.easy_to = ck.RFloatChunk();
                     break;
                  case CT_POSE_KEY_SMOOTH:
                     pk.smooth = ck.RFloatChunk();
                     break;
                  case CT_POSE_KEY_NOTIFY:
                     pk.notify = ck.RStringChunk();
                     break;
                  case CT_POSE_KEY_NOTIFY_LINK:
                     pk.notify_link = ck.RStringChunk();
                     break;
                  case CT_POSE_KEY_ANIM_SET:
                     {
                                          //create animset in our key
                        //S_pose_key &pk = key_list.back();
                        pk.anim_set = drv->CreateAnimationSet();
                        pk.anim_set->Release();
                                          //read anim set raw data:
                        dword num_anims = ck.ReadWord();
                        for(dword i = 0; i < num_anims; i++){
                           C_smart_ptr<I3D_anim_pose> pose = (PI3D_anim_pose)drv->CreateAnimation(I3DANIM_POSE);
                           pose->Release();
                           pose->Clear();
                           dword link_id = ck.ReadWord();
                           assert(link_id < names_list.size());
                           if(link_id >= names_list.size()){
                              throw C_except("S_pose_anim: file corrupted");
                           }
                           const C_str &link = names_list[link_id];
                           assert(FindAnimId(pk.anim_set, link) == -1);
                           pk.anim_set->AddAnimation(pose, link);
                           bool has_pos = ck.ReadBool();
                           if(has_pos){
                              S_vector pos = ck.ReadVector();
                              pose->SetPos(&pos);
                           }
                           bool has_rot = ck.ReadBool();
                           if(has_rot){
                              S_quat rot = ck.ReadQuaternion();
                              pose->SetRot(&rot);
                           }
                           bool has_pow = ck.ReadBool();
                           if(has_pow){
                              float f = ck.ReadFloat();
                              pose->SetPower(&f);
                           }
                        }
                        --ck;
                     }
                     break;
                  //case CT_POSE_KEY_ANIM_SET_OLD:
                     //break;
                  default:
                     assert(0);
                     --ck;
                  }
               }
               break;
            default: assert(0);
            }
            --ck;
         }
         break;

      case CT_POSE_ANIM_WEIGHTS:
         while(ck){
            switch(++ck){
            case CT_POSE_ANIM_WEIGHT:
               {
                  C_str link;
                  float val = 1.0f;
                  while(ck)
                  switch(++ck){
                  case CT_POSE_ANIM_WEIGHT_LINK:
                     link = ck.RStringChunk();
                     break;
                  case CT_POSE_ANIM_WEIGHT_VAL:
                     val = ck.RFloatChunk();
                     break;
                  default:
                     assert(0);
                     --ck;
                  }
                  if(link.Size())
                     SetWeight(link, val);
               }
               break;
            default: assert(0);
            }
            --ck;
         }
         break;

      case CT_POSE_ANIM_POS:
         edit_pos = ck.ReadVector();
         break;

      case CT_POSE_ANIM_ROT:
         edit_rot = ck.ReadQuaternion();
         break;

      default:
         assert(0);
      }
      --ck;
   }

   key_list.assign(&_key_list.front(), (&_key_list.back())+1);

   return true;
}

//----------------------------
