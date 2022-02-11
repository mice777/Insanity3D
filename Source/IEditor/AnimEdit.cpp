#include "all.h"
#include "common.h"
#include <i3d\PoseAnim.h>
#include <insanity\3dtexts.h> //for notify text

                              //test version of mirror frames function.not fully implement(missing undo, mirror by local axis, etc...)
//#define ENABLE_MIRROR_FRAMES

//----------------------------
//----------------------------
static const char *ANIM_MASK_MODES_TXT_PATH = "editor\\anim_masks.txt";
static const int MAX_ANIM_TIME = 2000000000;

const byte PLG_STATE_VERSION = 6;

//----------------------------
//----------------------------

enum E_TRANSFORMS{
   TR_POS = 1,
   TR_ROT = 2,
   TR_POW = 4,
};
                              //keep synchronous with key_words
enum E_KEY_WORDS{ KW_MASK, KW_NAME, KW_ITEMS, KW_LINK, KW_PRS, KW_LAST};

static const char *key_words[] = {
   "[Mask]", "Name", "Num_Items", "Link", "PRS"
};

static void WriteString(C_cache &ck, const C_str &s){
   ck.write((const char*)s, s.Size());
   ck.write("\r\n", 2);
}

//----------------------------
// This function search recursivly for child frame(similar to FindChildFrame), but not going search into another sub model,
// which can be linked to original model.
// Function is used when we need find child frame in model where model has frame 'child_name'
// and also another model which contain frame with same 'child_name' is linked to this model.

   static PI3D_frame FindModelChild(PI3D_frame frm, const C_str &child_name){

      const PI3D_frame *frms = frm->GetChildren();
      for(dword i = frm->NumChildren(); i--; ){
         PI3D_frame curr_frm = frms[i];
         if(curr_frm->GetType() == FRAME_MODEL)
            continue;
         if(child_name.Match(curr_frm->GetOrigName()))
            return curr_frm;
         //if(curr_frm->GetOrigName().Match(child_name))
         //   return curr_frm;
         PI3D_frame frm_found = FindModelChild(curr_frm, child_name);
         if(frm_found)
            return frm_found;
      }
      return NULL;
   }

//----------------------------
//----------------------------

class S_mask_mode{
public:
   typedef map<C_str, byte> t_mode_map;
private:
   t_mode_map enabled;
   C_str name;
   mutable bool changed;
   bool empty;                //true if not initialised

public:
   S_mask_mode() :
      empty(true),
      changed(false)
   {}

//----------------------------

   bool IsEmpty() const { return empty;}

//----------------------------

   void Duplicate(const S_mask_mode *src){

      if(!src)
         return;
      name = src->name;
      changed = src->changed;
      enabled = src->enabled;
      empty = src->empty;
   }

//----------------------------

   void operator =(const S_mask_mode &src){

      Duplicate(&src);
   }

//----------------------------

   bool IsChanged() const {return changed; }

//----------------------------

   const C_str &GetName() const { return name; }

//----------------------------

   void SetName(const C_str &name1){

     name = name1;
   }

//----------------------------

   const t_mode_map &GetMap() const { return enabled; }

//----------------------------

   void AddLink(const C_str &link){

      dword flags = TR_POS | TR_ROT | TR_POW;
      enabled[link] = (byte)flags;
      changed = true;
      empty = false;
   }

//----------------------------

   void DelLink(const C_str &link){

      t_mode_map::iterator it = enabled.find(link);
      if(it != enabled.end())
         enabled.erase(it);
      changed = true;
   }

//----------------------------

   bool IsInMask(const C_str &link) const{

      return (enabled.find(link) != enabled.end());
   }

//----------------------------

   byte GetEnabled(const C_str &link) const{

      t_mode_map::const_iterator it = enabled.find(link);
      if(it != enabled.end())
         return (*it).second;
      else
         return 0; //if link missing, consider frame as locked
   }

//----------------------------

   void SetEnabled(const C_str &link, byte new_mask, byte change_mask = TR_POS|TR_ROT|TR_POW){

      t_mode_map::iterator it = enabled.find(link);
      assert(it != enabled.end());
      if(it != enabled.end()){
         byte old_mask = (*it).second;
         byte result_mask(0);
         result_mask |= (change_mask&TR_POS) ? (new_mask&TR_POS) : (old_mask&TR_POS);
         result_mask |= (change_mask&TR_ROT) ? (new_mask&TR_ROT) : (old_mask&TR_ROT);
         result_mask |= (change_mask&TR_POW) ? (new_mask&TR_POW) : (old_mask&TR_POW);

         (*it).second = result_mask;
         changed = true;
      }
   }

//----------------------------
                                  
   bool Locked(const C_str &anim_link, dword flag) const{

      t_mode_map::const_iterator it = enabled.find(anim_link);
      if(it != enabled.end()){
         byte mask = (*it).second;
         bool locked = !(mask&flag);
         return locked;
      }else                
         return true;      //if link missing, consider frame as locked
   }

//----------------------------

   void Save(C_cache &ck) const{

                              //mode name
      dword name_size = name.Size() + 1;
      assert(("string too long", name_size < 256));
      ck.write((char*)&name_size, sizeof(byte)); //string limited to only 255 chars
      const char *s_name = name;
      ck.write((char*)s_name, name_size);
                           //num links in map
      dword num_items = enabled.size();
      ck.write((char*)&num_items, sizeof(dword));
                           //items
      for(t_mode_map::const_iterator it = enabled.begin(); it != enabled.end(); it++){
                           //write link
         const C_str &link = (*it).first;
         dword link_size = link.Size() + 1;
         assert(("string too long", link_size < 256));
         ck.write(&link_size, sizeof(byte)); //string limited to only 255 chars
         ck.write(&link[0], link_size);
                           //mask
         byte mask = (*it).second;
         ck.write((char*)&mask, sizeof(byte));
      }
      changed = false;
   }

//----------------------------

   void SaveText(C_cache &cp) const{

      C_str var;
                           //mode name
      var = C_fstr("%s %s", key_words[KW_NAME], (const char*)name);
      WriteString(cp, var);
                           //num links in map
      dword num_items = enabled.size();
      var = C_fstr(" %s %i", key_words[KW_ITEMS], num_items);
      WriteString(cp, var);
                           //items
      for(t_mode_map::const_iterator it = enabled.begin(); it != enabled.end(); it++){
                           //write link
         const char *link = (*it).first;
         var = C_fstr(" %s %s", key_words[KW_LINK], (const char*)link);
         WriteString(cp, var);
                           //mask
         byte mask = (*it).second;
         var = C_fstr(" %s %i", key_words[KW_PRS], mask);
         WriteString(cp, var);
      }
      changed = false;
   }

//----------------------------

   bool LoadText(C_cache &ck){

      #define SKIPSPACES while(*cp && isspace(*cp)) ++cp;

      if(ck.eof())
         return false;

      char line[256];
      ck.getline(line, sizeof(line));
      const char *cp = line;

                        //name
      SKIPSPACES;
      int kw_size = strlen(key_words[KW_NAME]);
      if(strncmp(cp, key_words[KW_NAME], kw_size))
         return false;

      cp += kw_size + 1;
      SKIPSPACES;

                           //read entire rest of line, name can have spaces
      name = (const char*)cp;
                        //num links
      SKIPSPACES;
      ck.getline(line, sizeof(line));
      cp = line;

      SKIPSPACES;
      kw_size = strlen(key_words[KW_ITEMS]);
      if(strncmp(cp, key_words[KW_ITEMS], kw_size))
         return false;

      cp += kw_size + 1;
      SKIPSPACES;

      dword num_items;
      sscanf(cp, "%i", &num_items);

      for(int i = num_items; i--; ){
         ck.getline(line, sizeof(line));
         cp = line;
                           //link
         SKIPSPACES;
         kw_size = strlen(key_words[KW_LINK]);
         if(strncmp(cp, key_words[KW_LINK], kw_size))
            return false;
         cp += kw_size + 1;
         SKIPSPACES;
                           //read entire rest of line, name can have spaces
         C_str link = (const char*)cp;

         ck.getline(line, sizeof(line));
         cp = line;
                           //mask
         SKIPSPACES;
         kw_size = strlen(key_words[KW_PRS]);
         if(strncmp(cp, key_words[KW_PRS], kw_size))
            return false;
         cp += kw_size + 1;
         SKIPSPACES;
         dword mask;
         sscanf(cp, "%i", &mask);

         enabled[link] = (byte)mask;
      }

      changed = false;
      empty = false;
      return true;
   }

//----------------------------

   void Load(C_cache &ck){

                              //name
      byte name_size;
      ck.read((char*)&name_size, sizeof(byte));
      char buf[256];
      ck.read((char*)&buf, name_size);
      name = buf;
                              //num links
      dword num_items;
      ck.read((char*)&num_items, sizeof(dword));

      for(dword i = num_items; i--; ){

         byte link_size;
         char link_buf[256];
         ck.read(&link_size, sizeof(byte));
         ck.read(&link_buf, link_size);
         byte mask;
         ck.read(&mask, sizeof(byte));
         enabled[link_buf] = mask;
      }
      changed = false;
      empty = false;
   }

//----------------------------

   bool DebugMatchToMask(const S_mask_mode &m) const{

      if(!name.Match(m.GetName()))
         return false;
      for(t_mode_map::const_iterator it = enabled.begin(); it != enabled.end(); it++){
                           //write link
         const C_str &link = (*it).first;
         dword mask = (*it).second;

         if(m.GetEnabled(link) != mask)
            return false;
      }
      return true;
   }

//----------------------------

   void CreateCustom(CPI3D_model mod){

      enabled.clear();
      SetName("<custom>");

      if(!mod)
         return;

      dword num = mod->NumFrames();
      CPI3D_frame const*frms = mod->GetFrames();
      for(dword j = 0; j < num; j++){
         const C_str &link = frms[j]->GetOrigName();
         AddLink(link);
      }
      empty = false;
      return;
   }
};

//----------------------------
//----------------------------

struct S_pose_anim_edit: public S_pose_anim{

private:
                              //id of transform mask for frames of common skeletons.
                              //locked pos/rot/pow are not affected nor saved.
   dword mask_mode_id;
                              //custom mask is used when mask_mode_id == 0;
   S_mask_mode custom_mask;
                              //key currently edited
   dword curr_key;

public:
                              //animation name (for saving) - must be unique in scene.
   C_str name;
                              //loaded model name - used for saving when frame was not found in loading.
   C_str model_name;

//----------------------------
   S_pose_anim_edit() : 
      curr_key(0),
      mask_mode_id(0)
   {
   }

//----------------------------

   dword CurrKey() const { return curr_key; }

   void SetCurrKey(dword id) { curr_key = id; }

//----------------------------

   dword GetMaskId() const{ return  mask_mode_id; }

//----------------------------

   const S_mask_mode &GetCustomMask() const {return custom_mask; }

//----------------------------

   void SetMask(dword new_id, const S_mask_mode *pcustom){

      mask_mode_id = new_id;
      if(!new_id){
         assert(pcustom);
         custom_mask.Duplicate(pcustom);
         custom_mask.SetName("<custom>");
      }
   }

//----------------------------
// Copy transform from pose to frame. Only transform specified in copy mask is applied.
   static void CopyPose2Frame(CPI3D_anim_pose pose, PI3D_frame frm, dword copy_flgs,
      PC_editor_item_Properties e_props, PC_editor_item_Modify e_modify){

      bool cp_pos = copy_flgs&TR_POS;
      bool cp_rot = copy_flgs&TR_ROT;
      bool cp_pow = copy_flgs&TR_POW;

      dword notify_flags = 0;

      if(cp_pos && pose->GetPos()){
         frm->SetPos(*pose->GetPos());
         notify_flags |= E_MODIFY_FLG_POSITION;
      }
      if(cp_rot && pose->GetRot()){
         frm->SetRot(*pose->GetRot());
         notify_flags |= E_MODIFY_FLG_ROTATION;
      }
      if(cp_pow && pose->GetPower()){
         frm->SetScale(*pose->GetPower());
         notify_flags |= E_MODIFY_FLG_SCALE;
      }
                              //notify properties, so that sheets are updated
      {
         //pair<PI3D_frame, dword> flgs(frm, notify_flags);
         //e_props->Action(E_PROP_NOTIFY_MODIFY, flgs);
         e_modify->AddFrameFlags(frm, notify_flags);
      }
   }

//----------------------------

   void AddKeyDefault(PI3D_driver drv){

      assert(!key_list.size());
      if(key_list.size())
         return;
      key_list.resize(key_list.size()+1);
      //key_list.push_back(S_pose_key());
      S_pose_key &pk = key_list.back();
      pk.time = 0;
      pk.anim_set = drv->CreateAnimationSet();
      pk.anim_set->Release();
   }

//----------------------------

   int FindClosestKey(int time) const{

      int closest_id(0);
      dword closest_delta(dword(-1));
      for(dword i = 0; i < key_list.size(); i++){
         dword delta = abs(key_list[i].time - time);
         if(!i || (delta < closest_delta)){
            closest_id = i;
            closest_delta = delta;
         }
      }
      return closest_id;
   }

//----------------------------
// Return id of closest key with greater time; if no such exist, return key_list.size().
   int FindClosestGreaterKey(int time) const{

      int num_k = key_list.size();
                              //keep this cycle straight forward
      for(int i = 0; i < num_k; i++){           
         if(key_list[i].time > time)
            break;
      }
      return i;
   }

//----------------------------

   dword AddKey(dword id, const S_pose_key &pk){

      assert(id <= key_list.size());
                              //we need key before which one we insert it
      //S_pose_key *it_greater = (id < key_list.size()) ? &key_list[id] : key_list.end();
      //S_pose_key &new_key = *(key_list.insert(it_greater, S_pose_key()));
      //new_key = pk;
      key_list.resize(key_list.size()+1);
      for(dword i=key_list.size()-1; i>id; i--){
         key_list[i] = key_list[i-1];
      }
      key_list[id] = pk;

      assert(id < key_list.size());
      curr_key = id;
      return curr_key;
   }

//----------------------------

   void DelKey(dword id){

      if(!key_list.size())
         return;
      assert(id < key_list.size());
      //key_list.erase(&key_list[id]);
      for(dword i=id; i<key_list.size()-1; i++){
         key_list[i] = key_list[i+1];
      }
      key_list.resize(key_list.size()-1);
      curr_key = Min(Max((dword)0, curr_key), (dword)key_list.size()-1);
   }

//----------------------------

   void SetKey(PI3D_model mod, PI3D_driver drv){

      if(curr_key < key_list.size()){
         PI3D_animation_set as = key_list[curr_key].anim_set;
         StorePoseIntoSet(mod, as, drv);
      }
   }

//----------------------------

   S_pose_key &CurrPoseKey(){
      assert(curr_key < key_list.size());
      return key_list[curr_key];
   }

//----------------------------

   const S_pose_key &CurrPoseKey() const{ 
      assert(curr_key < key_list.size());
      return key_list[curr_key];
   }

//----------------------------
// Store poses from mod to anim_set;
// Only frames which are already in anim_set are stored.
   void StorePoseIntoSet(PI3D_model model, PI3D_animation_set anim_set, PI3D_driver drv) const{

      assert(("StorePoseIntoSet: invalid anim_set", anim_set));
      assert(("StorePoseIntoSet: invalid model", model));
                              //store new ones from current model pose

      for(dword i = 0; i < anim_set->NumAnimations(); i++){

         assert(anim_set->GetAnimation(i)->GetType() == I3DANIM_POSE);
         PI3D_anim_pose pose_to = (PI3D_anim_pose)anim_set->GetAnimation(i);
         const C_str &link_name = anim_set->GetAnimLink(i);
         //PI3D_frame frm_from = model->FindChildFrame(link_name);
         PI3D_frame frm_from = FindModelChild(model, link_name);
         if(!frm_from)
            continue;
         if(pose_to->GetPos())
            pose_to->SetPos(&frm_from->GetPos());
         if(pose_to->GetRot())
            pose_to->SetRot(&frm_from->GetRot());
         if(pose_to->GetPower()){
            float scale = frm_from->GetScale();
            pose_to->SetPower(&scale);
         }
      }
   }

//----------------------------
// Apply pose from set to model
// Only frames which are in frame_list are applied.
   //void ApplyPoseFromSet(PI3D_animation_set anim_set)
   void ApplyPoseFromSet(PI3D_model mod, PI3D_animation_set anim_set, const S_mask_mode &mask_mode,
      PC_editor_item_Properties e_props, PC_editor_item_Modify e_modify) const{

      if(!mod)
         return;
      assert(("ApplyPoseFromSet: invalid anim_set", anim_set));

      for(dword i = anim_set->NumAnimations(); i--;){

         PI3D_anim_pose pose_from = (PI3D_anim_pose)anim_set->GetAnimation(i);
         const C_str &link_name = anim_set->GetAnimLink(i);
         if(!IsInFramesList(link_name))
            continue;
         assert(mod);
         //PI3D_frame frm_to = mod->FindChildFrame(link_name);
         PI3D_frame frm_to = FindModelChild(mod, link_name);
         if(!frm_to){
            continue;
         }
         CopyPose2Frame(pose_from, frm_to, mask_mode.GetEnabled(link_name), e_props, e_modify);
      }
   }
};

//----------------------------
//----------------------------

static const S_ctrl_help_info help_texts[] = {
   IDC_ANIM_NAME1,   "Name of animation.",
   IDC_ANIM_NAME,    "Name of animation. You may type a new name for currently edited animation.",
   IDC_CHANGE_MODEL, "Assignment of currently selected model for editing of the animation.",
   IDC_ADD_KEY,      "Add a new key beyond currently selected key.",
   IDC_DEL_KEY,      "Delete currently selected key.",
   IDC_PLAY_TOGGLE,  "Toggle animation playback.",
   IDC_KEY_NUM,      "Index of currently selected key, and number of keys in the animation.",
   IDC_FIRST_KEY,    "Jump to first key.",
   IDC_PREV_KEY,     "Jump to previous key.",
   IDC_NEXT_KEY,     "Jump to next key.",
   IDC_LAST_KEY,     "Jump to last key.",
   IDC_ST_TIME,      "Time of currently selected key.",
   IDC_KEY_TIME,     "Type to change time of key.",
   IDC_PLAY_TIME,    "Total playback time.",
   IDC_INSERT_TIME,  "Add time value to this and all following keys.",
   IDC_NOTIFY_GROUP, "Setting of notification key.",
   IDC_NOTIFY,       "Notify text associated with notify.",
   //IDC_PICK_NOTIFY_FRM, "Pick frame, on which notify key will be stored.",
   IDC_PICK_NOTIFY_FRM, "Make selected frame to be the frame, on which notify key will be stored.",
   IDC_NOTIFY_LINK,  "Name of frame on which notification will be used.",
   IDC_NOTIFY_LINK_TITLE,  "Name of frame on which notification will be used.",
   IDC_NOTE_CLEAR,   "Clear notification.",
   IDC_EASY_FROM_TITLE, "Easy-from value associated with key. A value of 0.0 produces linear time warping, while value of 1.0 produces slow acceleration from the key.",
   IDC_EASY_FROM,    "Type easy-from value in range from 0 to 1.",
   IDC_EASY_TO_TITLE,"Easy-to value associated with key. A value of 0.0 produces linear time warping, while value of 1.0 produces slow de-acceleration to the key.",
   IDC_EASY_TO,      "Type easy-to value in range from 0 to 1.",
   IDC_SMOOTH_TEXT,  "Smoothness of the key, specifying how position/rotation/scale will be adjusted so that passing this key is smooth relatively to neighbor keys. Value of 0.0 means no smoothness, while value os 1.0 makes maximal smooth effect.",
   IDC_SMOOTH,       "Type new smooth value in range from 0 to 1.",
   IDC_SMOOTH_ANIM_SPEED,  "Adjusts time of all keys in such a way, that movement speed of frames is linear.",
   IDC_COPY_POSE,    "Copy pose from selected model (if any), or currently selected animation key.",
   IDC_PASTE_POSE,   "Paste pose onto currently selected animation key.",
   IDC_PASTE_MIRROR, "Paste pose onto currently selected animation key, and perform mirror operation.",
   IDC_TIME_SLIDER,  "Time slider representing time of animation.",
   IDC_ANIM_TIME_SCALE, "Re-scaling of animation total time. All keys' time will be adjusted.",
   IDC_MODEL_NAME,   "Name of model on which animation is edited.",
   IDC_MODEL_NAME_TITLE,   "Name of model on which animation is edited.",
   IDC_PASTE_POSE_SELECTION,        "Paste pose onto selected frames of currently selected animation key.",
   IDC_PASTE_POSE_SELECTION_KEYS,   "Paste pose onto selected frames of all anim keys.",
   0                          //terminator
}, masks_help_texts[] = {
   IDC_MODE_SELECT,  "Predefined masks. Each mask contain frames which can be animated and transforms which can be applied.",
   IDC_NEW_MODE,     "Add new mask. Frames are collected from model of current anim, or from selected model if no anim selected.",
   IDC_DELETE_MODE,  "Delete current mask.",
   IDC_TRANS_ALL,    "Set pos, rot and scale for selected frames.",
   IDC_TRANS_CLEAR,  "Clear pos, rot and scale for selected frames.",
   IDC_ADD_FRAME,    "Add frame to mask. Frames which are in mask can be animated.",
   IDC_REMOVE_FRAME, "Remove frame from mask. Frames which are not in mask cannot be animated.",
   IDC_POS_CHECK,    "Enable/disable position change for selected frames in animation.",
   IDC_ROT_CHECK,    "Enable/disable rotation change for selected frames in animation.",
   IDC_POW_CHECK,    "Enable/disable scale change for selected frames in animation.",
   IDC_RELOAD,       "Reload masks to up to date with file on disk. You can use it when file was changed outside of editor, or when file was checked out for editing.",
   IDC_FRM_LIST2,    "Frames and transforms allowed in current mask for animation. You can add/remove frames and transforms.",
   IDC_TRANSFORM_GRP,"Transformation options of selected frames in the frame list.",
   IDOK,             "Accept changes and close this window.",
   IDCANCEL,         "Discard changes and close this window.",
   0                          //terminator
};
//----------------------------

class C_edit_AnimEdit: public C_editor_item{

   virtual const char *GetName() const { return "AnimEdit"; }

//----------------------------

   enum E_ACTION_ANIMEDIT{
      E_ANIMEDIT_NOTIFY_MODIFY = 18000,
      E_ANIMEDIT_NOTIFY_SELECTION,
   };


   enum E_MODE{
      MODE_OFF,
      MODE_EDIT,
      MODE_PLAY,
   } mode;

   bool loop_mode;
   bool lock_anim_model;

                              //internal actions:
   enum E_ACTION{

      E_ANIM_NULL,
      E_ANIM_CREATE,
      E_ANIM_CLOSE_EDIT,      
      E_ANIM_EDIT_TOGGLE,
      E_ANIM_PLAY_TOGGLE,
      E_ANIM_PLAY_STOP,

      E_ANIM_SELECT_ANIM,
      E_ANIM_LOOP_MODE,

      E_ANIM_CLEAR_SELECTION,
      E_ANIM_ALL_SELECTION,
      E_ANIM_SEL_SELECTION,
      E_ANIM_COPY_SELECTION,

      E_ANIM_PICK_MASK,
      E_ANIM_SET_WEIGHT,

      E_ANIM_COPY_POSE,
      E_ANIM_PASTE_POSE,
      E_ANIM_PASTE_MIRROR,
      E_ANIM_PASTE_SELECTION,
      E_ANIM_PASTE_SELECTION_ALL_KEYS, //same as above, but apply to all anim keys

      E_ANIM_PICK_NOTIFY_FRM,

      E_ANIM_NOTE_CLEAR,      //clear note and note link

      E_ANIM_SELECT_SCALE_TIME, //Select new length of anim and rescale keys times

      E_ANIM_RESET_SEL_FRAMES, //reset selection to original transform

      E_ANIM_ADD_FRAME,       //param: (PI3D_frame) frm to add
      E_ANIM_DEL_FRAME,       //param: (PI3D_frame) frm to del

      E_ANIM_ADD_KEY,
      E_ANIM_DEL_KEY,

      E_ANIM_INSERT_TIME,     //insert time before current key
      E_ANIM_SMOOTH_ANIM_TIME, //adjust keys times to smooth average anim speed

      E_ANIM_ADD_KEY_ID,      //internal for undo
      E_ANIM_DEL_KEY_ID,      //internal for undo

      E_ANIM_PREV_KEY,
      E_ANIM_NEXT_KEY,
      E_ANIM_PREV_KEY2,
      E_ANIM_NEXT_KEY2,
      E_ANIM_FIRST_KEY,
      E_ANIM_LAST_KEY,

      E_ANIM_LOCK_MODEL,      //lock anim model

#if 0
      //E_ANIM_PLAY_TIME_INC,
      //E_ANIM_PLAY_TIME_DEC,
#endif

      E_ANIM_GOTO_KEY,        //internal undo

      E_ANIM_RENAME_ANIM,     //internal for undo
      E_ANIM_SET_KEY_TIME,    //internal for undo
      E_ANIM_SET_KEY_SMOOTH, //internal for undo
      E_ANIM_SET_KEY_EASY_FROM, //internal for undo
      E_ANIM_SET_KEY_EASY_TO, //internal for undo
      E_ANIM_SET_KEY_NOTE,    //internal for undo
      E_ANIM_SET_KEY_NOTIFY_LINK, //internal for undo
      E_ANIM_SET_KEY_ANIM_SET,  //internal for undo
      E_ANIM_SET_EDIT_ANIM,   //internal for undo
      E_ANIM_UNDO_WEIGHT,
      E_ANIM_UNDO_FRM_TRANSFORM, //internal for undo
      E_ANIM_UNDO_INSERT_TIME,  //internal for undo
      E_ANIM_KEYS_TIMES_UNDO,      //internal for undo
      E_ANIM_UNDO_FRM_TRANFORM_KEYS, //internal for undo

      E_ANIM_ADD_FRAME_KEYS,  //internal undo
      E_ANIM_DEL_FRAME_KEYS,  //internal undo

      E_ANIM_CHANGE_MODEL,

      E_ANIM_TOGGLE_MODEL_TRANSP,  //toggle display mode of animation model
      E_ANIM_TOGGLE_PATHS,    //toggle rendering of animation paths

      E_ANIM_MIRROR_POSE,
      E_ANIM_MIRROR_POSE_NAMED, //internal for undo

      E_ANIM_IMPORT_KEYFRAME_ANIM,
      E_ANIM_IMPORT_MISSION_ANIM,

      E_ANIM_FOCUS_ANIM_EDIT,
      E_ANIM_FOCUS_FRAMES,
      E_ANIM_SET_KEY_TIME_DIRECT,
                              //test version of mirror frames
      E_ANIM_MIRROR_FRAMES,
   };

//----------------------------
// undo structures
#pragma pack(push,1)

//----------------------------
   struct S_key_del{
      int id_to_delete;
      int id_after_del;
   };

//----------------------------

   struct S_anim_del{
      int id_to_delete;
      int id_after_del;
   };

//----------------------------

   struct S_pose_key_basic{

      int time;
                                 //easy in/out smooth
      float easy_to, easy_from;

      float smooth;             //0..1
                                 //notify string. no notify if empty
      char notify[1];
      char notify_link[1];         //name of frame to notify.
   };

//----------------------------

   struct S_transform{

      S_vector pos;
      S_quat rot;
      float scale;
      bool use_pos;
      bool use_rot;
      bool use_scale;
   };

//----------------------------

   struct S_pose_key_set{

      int num_frames;
      struct S_frm_info : public S_transform{
         char frm_name[1];
      };
      S_frm_info frm_info[1];
   };

//----------------------------

   struct S_frame_keys{
      dword num_keys;
      char frm_name[1];
      S_transform transform[1];
   };

//----------------------------

   struct S_key_add{
      int id_to_create;
      S_pose_key_basic basic;
      S_pose_key_set set;
   };

//----------------------------

   struct S_ins_time{
      dword key_id;
      int time;
   };

//----------------------------

   struct S_set_weight{
      dword anim_id;
      float weight;
      char link[1];
   };

//----------------------------

#pragma pack(pop)
//----------------------------
// ok maybe could be useful have some system for add/read info to/from undo buffer.

   class C_undo_buf{
      enum{
         //M_NULL,
         M_WRITE,
         M_READ,
      } mode;
      C_vector<byte> buf;
      byte const *ptr_read_buf;
      dword read_pos;
   public:
      C_undo_buf() : mode(M_WRITE),
         read_pos(0)
      {}

      bool InitR(void *ptr){
         assert(ptr);
         if(!ptr)
            return false;
         ptr_read_buf = (byte *)ptr;
         mode = M_READ;
         return true;
      }

      bool InitW(){
         buf.clear();
         mode = M_WRITE;
         return true;
      }

      byte *Begin() { return &buf.front(); }
      //byte *End() { return buf.end(); }
      dword Size()const { return buf.size(); }
      const byte *GetPosPtr() const { assert(mode == M_READ); return ptr_read_buf + read_pos; }
      void SkipRead(dword num_skip) { read_pos += num_skip; }

      void WriteMem(void const *prt_begin, int size){

         assert(prt_begin);
         if(!prt_begin)
            return;
         assert(mode == M_WRITE);
         byte const *ptr = (byte *)prt_begin;
         for(int i = size; i--; ){
            const byte &element = (*ptr);
            buf.push_back(element);
            ++ptr;
         }
      }

      bool ReadMem(void *dest_buf, int size){

         assert(dest_buf);
         if(!dest_buf)
            return false;
         assert(mode == M_READ);
         byte const *ptr_read = ptr_read_buf + read_pos;
         byte *ptr_dest = (byte *)dest_buf;
         for(int i = 0; i < size; i++){
            ptr_dest[i] = ptr_read[i];
         }
         read_pos += size;
         return true;
      }

      void WriteString(const C_str &str){

         if(!str.Size())
            return;
         assert(mode == M_WRITE);
         const char *cp = (const char*)str;
         buf.insert(buf.end(), (const byte*)cp, (const byte*)cp + str.Size()+1);
      }

      bool ReadString(C_str &dest_str){

         byte const *ptr_read = ptr_read_buf + read_pos;
         const char *str = (const char*)ptr_read;
         dest_str = str;
         read_pos += strlen(str)+1;
         return true;
      }
   };

//----------------------------
// Add element multiple times to the end of C_vector.
   static void VectorAddElement(C_vector<byte> &undo_info, int num, byte element){

      //undo_info.insert(undo_info.end(), num, element);
      while(num--){
         undo_info.push_back(element);
      }
   }

//----------------------------
// Return id of anim link in animation set if exist, otherwise -1.
   static int FindAnimId(CPI3D_animation_set as, const C_str &link){

      assert(as);
      for(int i = as->NumAnimations(); i--; ){
         if(as->GetAnimLink(i)==link)
            break;
      }
      return i;
   }

//----------------------------

   static void CopyTransform2Pose(const S_transform &tr, PI3D_anim_pose pose){

      assert(pose);
      pose->Clear();
      if(tr.use_pos)
         pose->SetPos(&tr.pos);
      if(tr.use_rot)
         pose->SetRot(&tr.rot);
      if(tr.use_scale)
         pose->SetPower(&tr.scale);
   }

//----------------------------

   static void CopyPose2Transform(CPI3D_anim_pose pose, S_transform &tr){

      assert(pose);
      tr.use_pos = pose->GetPos();
      if(tr.use_pos)
         tr.pos = *pose->GetPos();

      tr.use_rot = pose->GetRot();
      if(tr.use_rot)
         tr.rot = *pose->GetRot();

      tr.use_scale = pose->GetPower();
      if(tr.use_scale)
         tr.scale = *pose->GetPower();
   }

//----------------------------

   static void CreateFrameKeysUndo(C_vector<byte> &undo_info, const C_buffer<S_pose_key> &key_list, const C_str &link){

      undo_info.clear();
      VectorAddElement(undo_info, sizeof(int), 0); //num_keys
      VectorAddElement(undo_info, link.Size() + 1, 0); //frm_name
      S_frame_keys &fk = (S_frame_keys &)undo_info.front();
      fk.num_keys = key_list.size();
      strcpy(fk.frm_name, link);
      for(dword i = 0; i < key_list.size(); i++){

         int transf_len = sizeof(S_transform);
         VectorAddElement(undo_info, transf_len, 0);
         S_transform &tr = *(S_transform *)(((&undo_info.back())+1) - transf_len);
         const S_pose_key &pk = key_list[i];
         assert(pk.anim_set);
         int anim_id = FindAnimId(pk.anim_set, link);
         if(anim_id != -1){
            CPI3D_animation_base ab = ((PI3D_animation_set)(CPI3D_animation_set)pk.anim_set)->GetAnimation(anim_id);
            assert(ab->GetType() == I3DANIM_POSE);
            PI3D_anim_pose pose = (PI3D_anim_pose)ab;
            CopyPose2Transform(pose, tr);
         }
      }
   }

//----------------------------

   void CreateFrameKeys(C_buffer<S_pose_key> &key_list, const S_frame_keys &fk) const{

      assert(("undo for frame keys should be performed when key_list have same num_keys as before delete", fk.num_keys == key_list.size()));
      if(fk.num_keys != key_list.size())
         return;
      const C_str &link = fk.frm_name;
      int trans_offset = sizeof(int) + strlen(fk.frm_name) + 1;
      const S_transform *tr = (const S_transform *) ((byte*)&fk + trans_offset); 
      int transf_len = sizeof(S_transform);
      for(dword i = 0; i < fk.num_keys; i++){
         S_pose_key &pk = key_list[i];
         int anim_id = FindAnimId(pk.anim_set, link);
         if(anim_id != -1)
            continue;

         PI3D_anim_pose pose = (PI3D_anim_pose)ed->GetDriver()->CreateAnimation(I3DANIM_POSE);
         CopyTransform2Pose(*tr, pose);
         pk.anim_set->AddAnimation(pose, link);
         pose->Release();

         tr = (const S_transform*)(((byte*)tr) + transf_len);
      }
   }

//----------------------------

   static void CreateAnimSetUndo(C_vector<byte> &undo_info, PI3D_animation_set as){

      assert(as);
      undo_info.clear();
      VectorAddElement(undo_info, sizeof(int), 0); //num_frames
      {
         S_pose_key_set &ps = (S_pose_key_set &)undo_info.front();
         ps.num_frames = as->NumAnimations();
      }
      for(dword i = 0; i < as->NumAnimations(); i++){

         const C_str &link = as->GetAnimLink(i);
         int frm_info_size = sizeof(S_pose_key_set::S_frm_info) + link.Size();
         VectorAddElement(undo_info, frm_info_size, 0);
         S_pose_key_set::S_frm_info &fi = *(S_pose_key_set::S_frm_info*)(((&undo_info.back())+1) - frm_info_size);
         CPI3D_animation_base ab = as->GetAnimation(i);
         assert(ab->GetType() == I3DANIM_POSE);
         PI3D_anim_pose pose = (PI3D_anim_pose)ab;

         strcpy(fi.frm_name, link);

         CopyPose2Transform(pose, fi);

      }
   }

//----------------------------
// Create anim set and return pointer to it. Pointer is keeped by driver 
// creator is responsible for release it.
   PI3D_animation_set CreateAnimSet(const void *undo_info, dword *num_readed = NULL) const{

      if(num_readed)
         *num_readed = 0;

      PI3D_animation_set as = ed->GetDriver()->CreateAnimationSet();
      S_pose_key_set &set = *(S_pose_key_set *) ((byte*)undo_info);
      S_pose_key_set::S_frm_info *fi = set.frm_info;
      if(num_readed)
         *num_readed += sizeof(int);

      for(int j = 0; j < set.num_frames; j++ )
      {
         PI3D_anim_pose pose = (PI3D_anim_pose)ed->GetDriver()->CreateAnimation(I3DANIM_POSE);
         CopyTransform2Pose(*fi, pose);
         const C_str &link =fi->frm_name;
         as->AddAnimation(pose, link);
         pose->Release();
                              //next
         int frm_info_size = sizeof(S_pose_key_set::S_frm_info) + link.Size();
         if(num_readed)
            *num_readed += frm_info_size;
         fi = (S_pose_key_set::S_frm_info*)(((byte*)fi) + frm_info_size);
      }
      return as;
   }

//----------------------------

   void CreateKeyUndoInfo(C_vector<byte> &undo_info, const S_pose_key &pk, int key_id) const{

      {
         undo_info.clear();
         int key_basic_size = sizeof(S_pose_key_basic) + pk.notify.Size() + pk.notify_link.Size();
         undo_info.assign(sizeof(int) + key_basic_size, 0);
         S_key_add &ka = *(S_key_add*)&undo_info.front();
         ka.id_to_create = key_id;
         ka.basic.time = pk.time;
         ka.basic.easy_to = pk.easy_to;
         ka.basic.easy_from = pk.easy_from;
         ka.basic.smooth = pk.smooth;
         strcpy(ka.basic.notify, pk.notify);
         strcpy(ka.basic.notify_link + strlen(ka.basic.notify), pk.notify_link);
         VectorAddElement(undo_info, sizeof(int), 0); //num_frames
         S_pose_key_set &set = *(S_pose_key_set *)(&undo_info.front() + sizeof(int) + key_basic_size);
         set.num_frames = pk.anim_set->NumAnimations();
      }

      for(dword i = 0; i < pk.anim_set->NumAnimations(); i++)
      {

         const C_str &link = pk.anim_set->GetAnimLink(i);
         int frm_info_size = sizeof(S_pose_key_set::S_frm_info) + link.Size();
         VectorAddElement(undo_info, frm_info_size, 0);
         S_pose_key_set::S_frm_info &fi = *(S_pose_key_set::S_frm_info*)(((&undo_info.back())+1) - frm_info_size);
         CPI3D_animation_base ab = ((PI3D_animation_set)(CPI3D_animation_set)pk.anim_set)->GetAnimation(i);
         assert(ab->GetType() == I3DANIM_POSE);
         PI3D_anim_pose pose = (PI3D_anim_pose)ab;

         strcpy(fi.frm_name, link);

         CopyPose2Transform(pose, fi);
      }
#if defined _DEBUG && 0
      int basic_size = sizeof(S_pose_key_basic) + pk.notify.Size() + pk.notify_link.Size();

      S_pose_key_set &set = *(S_pose_key_set *)(undo_info.begin() + sizeof(int) + basic_size);
      S_pose_key_set::S_frm_info *fi = set.frm_info;
      for(int j = 0; j < set.num_frames; j++ )
      {
         const C_str &link =fi->frm_name;
         const C_str &orig_link = pk.anim_set->GetAnimLink(j);
         assert(link==orig_link);
                              //next
         int frm_info_size = sizeof(S_pose_key_set::S_frm_info) + link.Size();
         fi = (S_pose_key_set::S_frm_info*)(((byte*)fi) + frm_info_size);
      }
#endif
   }

//----------------------------

   void CreateKeyFromUndo(S_pose_key &pk, void *undo_info) const{

      S_key_add &ka = *(S_key_add *)undo_info;
      S_pose_key_set::S_frm_info *fi = ka.set.frm_info;
      pk.time = ka.basic.time;
      pk.easy_from = ka.basic.easy_from;
      pk.easy_to = ka.basic.easy_to;
      pk.smooth = ka.basic.smooth;
      pk.notify = ka.basic.notify;
      pk.notify_link = (ka.basic.notify_link + strlen(ka.basic.notify));
      pk.anim_set = ed->GetDriver()->CreateAnimationSet();
      pk.anim_set->Release();

      int basic_size = sizeof(S_pose_key_basic) + pk.notify.Size() + pk.notify_link.Size();
      S_pose_key_set &set = *(S_pose_key_set *)(((byte*)undo_info) + sizeof(int) + basic_size);

      for(int j = 0; j < set.num_frames; j++ )
      {
         PI3D_anim_pose pose = (PI3D_anim_pose)ed->GetDriver()->CreateAnimation(I3DANIM_POSE);
         CopyTransform2Pose(*fi, pose);

         const char *link =fi->frm_name;
         pk.anim_set->AddAnimation(pose, link);
         pose->Release();
                              //next
         int frm_info_size = sizeof(S_pose_key_set::S_frm_info) + strlen(link);
         fi = (S_pose_key_set::S_frm_info*)(((byte*)fi) + frm_info_size);
      }
   }

//----------------------------

                              //used plugins
   C_smart_ptr<C_editor_item_Selection> e_slct;
   C_smart_ptr<C_editor_item_Log> e_log;
   C_smart_ptr<C_editor_item_Modify> e_modify;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<C_editor_item_DebugLine> e_dbgline;
   C_smart_ptr<C_editor_item_Properties> e_props;
   C_smart_ptr<C_editor_item_TimeScale> e_timescale;

//----------------------------

   inline dword NumMasks() const { return mask_modes.modes.size(); }

//----------------------------

   const S_mask_mode &GetAnimMask(const S_pose_anim_edit &pa) const{

      if(pa.GetMaskId())
         return mask_modes.GetMode(pa.GetMaskId());
      assert(!pa.GetCustomMask().IsEmpty());
      return pa.GetCustomMask();
   }

//----------------------------

   struct S_lock_modes{

   private:
      bool can_edit;
   public:

      enum{
         FILE_VERSION = 1,
      };

      C_vector<S_mask_mode> modes;
      mutable bool modes_changed;

   //----------------------------

      S_lock_modes() : can_edit(false), modes_changed(false)
      {
                                 //create at least one record - one which not lock any frame.
         CreateEmptyRecord();
      }

   //----------------------------

      void SetEditEnable(bool on_off){

         can_edit = on_off;
      }

   //----------------------------

      bool CanEdit() const { return can_edit; }

   //----------------------------

      bool NeedSave() const{

         for(dword i = modes.size(); i--; ){
            if(modes[i].IsChanged())
               return true;
         }
         return modes_changed;
      }

   //----------------------------

      S_mask_mode &GetMode(dword id){
         assert(id < modes.size());
         return modes[id];
      }

      const S_mask_mode &GetMode(dword id) const{
         assert(id < modes.size());
         return modes[id];
      }

   //----------------------------

      void AddMode(C_str &name, C_vector<PI3D_frame> &frm_list){

   #ifdef _DEBUG
                                 //check for duplication
         for(int i = modes.size(); i--; ){
            if(modes[i].GetName() == name){
               assert(("Mode with this name already exist.", 0));
            }
         }
   #endif

         modes.push_back(S_mask_mode());
         S_mask_mode &new_mode = modes.back();
         new_mode.SetName(name);
         for(dword j = 0; j < frm_list.size(); j++){
            const char * link = frm_list[j]->GetOrigName();
            new_mode.AddLink(link);
         }
         modes_changed = true;
      }

   //----------------------------

      void DeleteMode(dword sel_id){

         assert(sel_id < modes.size());
                                 //delete selected one
         modes.erase(modes.begin()+sel_id);

         modes_changed = true;
      }

   //----------------------------

      void SaveModes(C_cache &ck) const{

         int version = FILE_VERSION;
         ck.write((char*)&version, sizeof(version));
                                 //don't save first mode - it is default creation
         const int NUM_DEFAULT_MODES = 1;
         dword num_save_modes = modes.size() - NUM_DEFAULT_MODES;
         ck.write((char*)&num_save_modes, sizeof(num_save_modes));
         for(dword i = 0; i < num_save_modes; i++){
            assert((i + NUM_DEFAULT_MODES) < modes.size());
            modes[i + NUM_DEFAULT_MODES].Save(ck);
         }
         modes_changed = false;
      }

   //----------------------------

      bool SaveModesText(const char *filename) const{

         C_cache cp;
         bool b = cp.open(filename, CACHE_WRITE);
         if(!b)
            return false;

         const int NUM_DEFAULT_MODES = 1;
         dword num_save_modes = modes.size() - NUM_DEFAULT_MODES;
         C_str var;
         for(dword i = 0; i < num_save_modes; i++){
                                 //mode name
            var = key_words[KW_MASK];
            WriteString(cp, var);

            assert((i + NUM_DEFAULT_MODES) < modes.size());
            modes[i + NUM_DEFAULT_MODES].SaveText(cp);
         }
         return true;
      }

   //----------------------------

      bool LoadModesText(const char *filename){

         assert(modes.size() == 1);
         modes_changed = false;

         C_cache ck;
         if(!ck.open(filename, CACHE_READ))
            return false;

         while(!ck.eof()){
            char line[256];
            ck.getline(line, sizeof(line));
            const char *cp = line;

            const int kw_mask_size = strlen(key_words[KW_MASK]);
            if(strncmp(cp, key_words[KW_MASK], kw_mask_size))
               continue;

            modes.push_back(S_mask_mode());
            bool load_ok = modes.back().LoadText(ck);
            if(!load_ok)
               modes.pop_back();
         }
         return true;
      }

   //----------------------------

      void CreateEmptyRecord(){

         assert(!modes.size());
         modes.push_back(S_mask_mode());
         modes.back().SetName("<custom>");
      }

   //----------------------------

      void Clear(){

         modes.clear();
         CreateEmptyRecord();
      }

   //----------------------------

      bool LoadModes(C_cache &ck){

         assert(modes.size() == 1);
         int version;
         ck.read((char*)&version, sizeof(version));
         modes_changed = false;

         if(version != FILE_VERSION)
            return false;

         dword num_modes;
         ck.read((char*)&num_modes, sizeof(num_modes));
         for(dword i = 0; i < num_modes; i++){
            modes.push_back(S_mask_mode());
            modes.back().Load(ck);
         }
         return true;
      }
   //----------------------------

      bool DebugMatch(const S_lock_modes &mmt)const{

         if(mmt.modes.size() != modes.size())
            return false;
         for(int i = mmt.modes.size(); i--; ){

            const S_mask_mode &mm0 = modes[i];
            const S_mask_mode &mm1 = mmt.modes[i];
            assert(mm0.DebugMatchToMask(mm0));
            assert(mm1.DebugMatchToMask(mm1));
            if(!mm0.DebugMatchToMask(mm1))
               return false;
         }
         return true;
      }
   } mask_modes;

//----------------------------

   HWND hWnd_anim_edit;             //NULL if sheet is not displayed
   HWND hWnd_anim_frames;
   C_vector<S_pose_anim_edit> anims; //all anims in current scene
   int edit_anim_id;          //index of currently edited animation, or -1 if no
   bool model_transparency;
   bool render_paths;

   //int curr_playtime;

                              //text displayed for short time when notify key encountered
   C_smart_ptr<C_text> notify_text;
   int notify_text_count;

   bool pose_key_dirty; //just for update key in tick after modify notification

                              //model on which animation is performed (duplication of model in scene)
   C_smart_ptr<I3D_model> anim_mod;
   C_smart_ptr<I3D_model> hidden_frm;

                              //window position
   struct S_wnd_pos{
      int x, y;
      bool valid;
      S_wnd_pos(){
         memset(this, 0, sizeof(S_wnd_pos));
      }
   };
   mutable S_wnd_pos anim_edit_wnd_pos;

                              //last import dir
   C_str last_bin_import_dir;

                              //pose clipboard
   C_smart_ptr<I3D_animation_set> pose_clipboard;

//----------------------------

   class C_anim_play{
      dword end_time;
      dword time;
   public:

      C_anim_play():
         time(0),
         end_time(0)
      {}

   //----------------------------

      void AddAnim(PI3D_model mod, PI3D_animation_set as, bool loop){

         mod->SetAnimation(0, as, loop ? I3DANIMOP_LOOP : 0);
         end_time = as->GetTime();
      }

   //----------------------------

      void SetTime(PI3D_model mod, dword t){

         time = t;
         mod->SetAnimCurrTime(0, time);
      }

   //----------------------------

      dword GetTime() const{ return time; }

   //----------------------------

      void ResetTime() { time = 0; }

   //----------------------------

      void Tick(PI3D_model mod, int t, bool loop, PI3D_CALLBACK cb_proc = NULL, void *cb_context = NULL){

         time += t;
         if(loop)
            time %= (end_time+1);
         else
            time = Min(time, end_time);

         mod->Tick(t, cb_proc, cb_context);
      }
   } anim_play;

//----------------------------
// Get reference into currently edited anim.
   S_pose_anim_edit &GetEditAnim(){
      assert(IsEditAnimValid());
      assert(edit_anim_id < (int)anims.size());
      return anims[edit_anim_id];
   }
   const S_pose_anim_edit &GetEditAnim() const{
      assert(IsEditAnimValid());
      assert(edit_anim_id < (int)anims.size());
      return anims[edit_anim_id];
   }

//----------------------------
// Get edit mask of currently edited anim.
   const S_mask_mode &GetEditMask() const{

      const S_pose_anim_edit &pa = GetEditAnim();
      return GetAnimMask(pa);
   }

//----------------------------

   bool IsActive() const { return mode != MODE_OFF; }

//----------------------------
// Check if selected frame is child of edited model.
   bool IsChildOfEditedModel(CPI3D_frame frm) const{

      if(!anim_mod)
         return false;
      while((frm = frm->GetParent(), frm) && frm->GetType()!=FRAME_MODEL);
      return ((CPI3D_frame)anim_mod == frm);
   }

//----------------------------
// Close editor and free animation list.
   void ClearAnims(){
                              
                              //must close editor before clearing the list
      SetEditAnim(-1);
      anims.clear();
   }

//----------------------------

   bool CanEditCurrKey() const{

      return (mode == MODE_EDIT);
   }

//----------------------------

   void LockFrame(const S_pose_anim_edit &pa, PI3D_frame frm){

      const char *link = frm->GetOrigName();
      dword flags = 0;
      if(!pa.IsInFramesList(link)){
                              //lock all frames which are not in mask
         flags = E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE;
      }else{
                           //get locked transforms
         const S_mask_mode &mask = GetAnimMask(pa);
         dword locked = ~mask.GetEnabled(link);
         if(locked & TR_POS)
            flags |= E_MODIFY_FLG_POSITION;
         if(locked & TR_ROT)
            flags |= E_MODIFY_FLG_ROTATION;
         if(locked & TR_POW)
            flags |= E_MODIFY_FLG_SCALE;
      }
      e_modify->AddLockFlags(frm, flags);
   }

//----------------------------

   void UnlockFrame(PI3D_frame frm){

      e_modify->RemoveLockFlags(frm, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE);
   }

//----------------------------
//1.lock all non animated frames of model
//2.lock all disabled transforms of animated frames
   void LockAnimFrames(){

      S_pose_anim_edit &pa = GetEditAnim();
      if(!anim_mod)
         return;
      PI3D_frame const *frms = anim_mod->GetFrames();
      for(int i = anim_mod->NumFrames(); i--; ){
         PI3D_frame frm = frms[i];
         LockFrame(pa, frm);
      }
   }

//----------------------------

   void UnlockAnimFrames(){

      if(!anim_mod)
         return;
      const PI3D_frame *frms = anim_mod->GetFrames();
      for(int i = anim_mod->NumFrames(); i--; ){
         PI3D_frame frm = frms[i];
         UnlockFrame(frm);
      }
   }

//----------------------------
//scan all anims in set and look if some link/transform missing or obstruct.
#if 1
   bool IsAnimSetMatchToMask(PI3D_model mod, CPI3D_animation_set as, const S_mask_mode &mm){

      if(!mod)
         return true; //silent when no model
      assert(e_log);
      PI3D_frame const *frms = mod->GetFrames();
      for(int i = mod->NumFrames(); i--; ){
                              //resolve links
         const char *link = frms[i]->GetOrigName();

         bool is_in_mask = mm.IsInMask(link);
         bool is_enebled = mm.GetEnabled(link);
         int anim_id = FindAnimId(as, link);         
         bool is_in_anim = (anim_id != -1);

         bool can_be_in_anim = (is_in_mask && is_enebled);
         if(!can_be_in_anim && is_in_anim){
            e_log->AddText(C_fstr("Obstruct link '%s'", link));
            return false;
         }
         if(can_be_in_anim && is_in_anim){
                              //check if transforms match
            CPI3D_animation_base ab = ((PI3D_animation_set)as)->GetAnimation(anim_id);
            assert(ab->GetType() == I3DANIM_POSE);
            PI3D_anim_pose pose = (PI3D_anim_pose)ab;
            bool should_have_pos = !mm.Locked(link, TR_POS);
            bool should_have_rot = !mm.Locked(link, TR_ROT);
            bool should_have_pow = !mm.Locked(link, TR_POW);
            bool have_pos = pose->GetPos();
            bool have_rot = pose->GetRot();
            bool have_pow = pose->GetPower();
            if(should_have_pos && !have_pos){ 
               e_log->AddText(C_fstr("Missing Pos in link '%s'", link));
               return false;
            }
            if(!should_have_pos && have_pos){
               e_log->AddText(C_fstr("Obstruct Pos in link '%s'", link));
               return false;
            }
            if(should_have_rot && !have_rot){ 
               e_log->AddText(C_fstr("Missing Rot in link '%s'", link));
               return false;
            }
            if(!should_have_rot && have_rot){
               e_log->AddText(C_fstr("Obstruct Rot in link '%s'", link));
               return false;
            }
            if(should_have_pow && !have_pow){
               e_log->AddText(C_fstr("Missing Scale in link '%s'", link));
               return false;
            }
            if(!should_have_pow && have_pow){
               e_log->AddText(C_fstr("Obstruct Scale in link '%s'", link));
               return false;
            }
         }
      }
      return true;
   }

//----------------------------

   bool CheckAnimMask(){

      bool ok(true);
      assert(IsEditAnimValid());
      S_pose_anim_edit &pa = GetEditAnim();
      const S_mask_mode &mm = GetAnimMask(pa);
      for(int i = pa.key_list.size(); i--; ){
         S_pose_key &pk = pa.key_list[i];

         bool b = IsAnimSetMatchToMask(anim_mod, pk.anim_set, mm);
         if(!b){
            C_str err = C_fstr("Anim '%s' not match to mask '%s'", (const char*)pa.name, (const char*)mm.GetName());
            e_log->AddText(err);
            return false;
         }
      }
      return ok;
   }
#endif

//----------------------------

   bool CheckAnimSets(const S_pose_anim_edit &pa){

      if(!pa.key_list.size())
         return true;

      CPI3D_animation_set as0 = pa.key_list[0].anim_set;
      for(int i = pa.key_list.size(); i--; ){
         const S_pose_key &pk = pa.key_list[i];

                              //check if anims from first set are in others
         for(int k = as0->NumAnimations(); k--; ){
            CPI3D_animation_base ab0 = as0->GetAnimation(k);
            assert(ab0->GetType() == I3DANIM_POSE);
            CPI3D_anim_pose pose0 = (CPI3D_anim_pose)ab0;
            const C_str &link0 = as0->GetAnimLink(k);
            CPI3D_animation_set as = pk.anim_set;
            int id = FindAnimId(as,link0);
            if(id == -1){
               C_str err = C_fstr("Link '%s' missing in key '%i', anim '%s'", (const char*)link0, i, (const char*)pa.name);
               e_log->AddText(err);
               return false;
            }
                              //check if they have same transforms
            CPI3D_animation_base ab = as->GetAnimation(id);
            PI3D_anim_pose pose = (PI3D_anim_pose) ab;

                              //pos
            bool pos = pose->GetPos(); 
            bool pos0 = pose0->GetPos();            
            if((pos && !pos0) || (!pos && pos0)){
               C_str err = C_fstr("Pos mismatch, link '%s' in key '%i', anim '%s'", (const char*)link0, i, (const char*)pa.name);
               e_log->AddText(err);
               return false;
            }
                              //rot
            bool rot = pose->GetRot();
            bool rot0 = pose0->GetRot();
            if((rot && !rot0) || (!rot && rot0)){
               C_str err = C_fstr("Rot mismatch, link '%s' in key '%i', anim '%s'", (const char*)link0, i, (const char*)pa.name);
               e_log->AddText(err);
               return false;
            }
                              //power
            bool pow = pose->GetPower();
            bool pow0 = pose0->GetPower();            
            if((pow && !pow0) || (!pow && pow0)){
               C_str err = C_fstr("Pow mismatch, link '%s' in key '%i', anim '%s'", (const char*)link0, i, (const char*)pa.name);
               e_log->AddText(err);
               return false;
            }
         }
                              //check if some anim is not obscure (is not in first anim)
         CPI3D_animation_set as = pk.anim_set;
         for (int j = as->NumAnimations(); j--; ){
            const C_str &link = as->GetAnimLink(j);
            int id = FindAnimId(as0, link);
            if(id == -1){
               C_str err = C_fstr("Link '%s' obscure in key '%i', anim '%s'", (const char*)link, i, (const char*)pa.name);
               e_log->AddText(err);
               return false;
            }
         }
      }
      return true;
   }

//----------------------------

   void ApplyMaskToSet(S_pose_anim_edit &pa, PI3D_animation_set as, const S_mask_mode &mm, PI3D_model mod_orig){

      if(!as)
         return;

      assert(mod_orig);

      PI3D_frame const *frms = mod_orig->GetFrames();
      for(int i = mod_orig->NumFrames(); i--; ){
                              //resolve links
         PI3D_frame frm_orig = frms[i];
         const C_str link = frm_orig->GetOrigName();

         bool is_in_mask = mm.IsInMask(link);
         bool is_enebled = mm.GetEnabled(link);
         int anim_id = FindAnimId(as, link);         
         bool is_in_anim = (anim_id != -1);

         bool can_be_in_anim = (is_in_mask && is_enebled);

         if(!can_be_in_anim && is_in_anim){
            C_str msg = C_fstr("Removing link '%s', anim '%s'", (const char*)link, (const char*)pa.name);
            e_log->AddText(msg);
            as->RemoveAnimation(((PI3D_animation_set)as)->GetAnimation(anim_id));
            continue;
         }
         if(can_be_in_anim && is_in_anim){
                              //check if transforms match
            CPI3D_animation_base ab = as->GetAnimation(anim_id);
            assert(ab->GetType() == I3DANIM_POSE);
            PI3D_anim_pose pose = (PI3D_anim_pose)ab;
                              //pos
            bool should_have_pos = !mm.Locked(link, TR_POS);
            bool have_pos = pose->GetPos();

            if(should_have_pos && !have_pos){
               C_str msg = C_fstr("Adding Pos in link '%s', anim '%s' ", (const char*)link, (const char*)pa.name);
               e_log->AddText(msg);
               assert(frm_orig);
               pose->SetPos(&frm_orig->GetPos());
            }
            if(!should_have_pos && have_pos){
               C_str msg = C_fstr("Removing Pos in link '%s', anim '%s'", (const char*)link, (const char*)pa.name);
               e_log->AddText(msg);
               pose->SetPos(NULL);
            }
                              //rot
            bool should_have_rot = !mm.Locked(link, TR_ROT);
            bool have_rot = pose->GetRot();

            if(should_have_rot && !have_rot){
               C_str msg = C_fstr("Adding Rot in link '%s', anim '%s'", (const char*)link, (const char*)pa.name);
               e_log->AddText(msg);
               assert(frm_orig);
               pose->SetRot(&frm_orig->GetRot());
            }
            if(!should_have_rot && have_rot){
               C_str msg = C_fstr("Removing Rot in link '%s', anim '%s'", (const char*)link, (const char*)pa.name);
               e_log->AddText(msg);
               pose->SetRot(NULL);
            }
                              //pow
            bool should_have_pow = !mm.Locked(link, TR_POW);
            bool have_pow = pose->GetPower();

            if(should_have_pow && !have_pow){
               C_str msg = C_fstr("Adding Scale in link '%s', anim '%s'", (const char*)link, (const char*)pa.name);
               e_log->AddText(msg);
               assert(frm_orig);
               float f = frm_orig->GetScale();
               pose->SetPower(&f);
            }
            if(!should_have_pow && have_pow){
               C_str msg = C_fstr("Removing Scale in link '%s', anim '%s'", (const char*)link, (const char*)pa.name);
               e_log->AddText(msg);
               pose->SetPower(NULL);
            }
         }
      }
                              //check for links which are not in model
      C_buffer<C_str> names_list;
      pa.GetAnimFrames(names_list);
      for(int i2 = names_list.size(); i2--; ){
         const C_str &link = names_list[i2];
//         if(!mod_orig->FindChildFrame(link))
         if(!FindModelChild(mod_orig, link))
         {
         

            C_str msg = C_fstr("Removing link '%s' from anim '%s' (missing in model '%s')",
               (const char*)link, (const char*)pa.name,
               (const char*)mod_orig->GetFileName());
            e_log->AddText(msg);
            RemoveAnimKeysLink(pa, link);
         }
      }
   }

//----------------------------
// Set mask id to anim and apply mask on it.
   void SetAnimMask(S_pose_anim_edit &pa, dword mask_id, const S_mask_mode *pcustom){

                              //set mask
      assert(mask_id < mask_modes.modes.size());
      assert(mask_id || pcustom);
      pa.SetMask(mask_id, pcustom);

      PI3D_model pa_model = I3DCAST_MODEL(ed->GetScene()->FindFrame(pa.model_name, ENUMF_MODEL));
      if(!pa_model){
         //C_fstr msg("Anim '%s': Can't find frame '%s', cannot aply anim mask", (const char*)pa.name, (const char*)pa.model_name);
         //e_log->Action(E_LOG_ADD_TEXT, (void*)(const char *)msg);
         return;
      }
      const C_str &mod_filename = pa_model->GetFileName();
                              //create model with original pose transforms
                              //(used for reset frames which are not in anim or mask)
      C_smart_ptr<I3D_model> mod_orig = GetOrigModel(mod_filename);
      if(!mod_orig){
         C_fstr msg("Anim '%s': Missing model '%s', cannot apply anim mask", (const char*)pa.name, (const char*)mod_filename);
         e_log->AddText(msg);
         return;
      }
                               //apply
      const S_mask_mode &mm = GetAnimMask(pa);

      for(int i = pa.key_list.size(); i--; ){
         S_pose_key &pk = pa.key_list[i];
         ApplyMaskToSet(pa, pk.anim_set, mm, mod_orig);
      }
      mod_orig->Release();
   }

//----------------------------

   bool InitAnimModel(const S_pose_anim_edit &pa){

      assert(!anim_mod && !hidden_frm);
      PI3D_scene scn = ed->GetScene();
      anim_mod = I3DCAST_MODEL(scn->CreateFrame(FRAME_MODEL));
      anim_mod->Release();


                              //find model source
      PI3D_model src = I3DCAST_MODEL(scn->FindFrame(pa.model_name, ENUMF_MODEL));
      if(!src){
         e_log->AddText(C_fstr("Anim '%s': root frame '%s' not found.", (const char*)pa.name, (const char*)pa.model_name));
         return false;
      }

      hidden_frm = src;

#if 1
      C_I3D_cache_base<I3D_model> &model_cache = ed->GetModelCache();
      const C_str &fname = src->GetFileName();
      //model_cache.GetRelativeDirectory(&fname[0]);
      model_cache.Open(anim_mod, fname, scn, 0);
#else
                              //duplicate - let currently modified frames be modified also on the model
      anim_mod->Duplicate(src);
      anim_mod->SetData(0);
#endif
      anim_mod->SetName("<anim edit>");
      //anim_mod->LinkTo(src->GetParent());
      anim_mod->SetFlags(src->GetFlags() | I3D_FRMF_NOSHADOW, I3D_FRMF_NOSHADOW | I3D_FRMF_SHADOW_CAST);
                              //hide original
      hidden_frm->SetOn(false);
      e_slct->RemoveFrame(src);

                              //mark all frames as temporary
      {
         const PI3D_frame *frms = anim_mod->GetFrames();
         for(int i = anim_mod->NumFrames(); i--; ){
            PI3D_frame frm = frms[i];
            e_modify->AddFrameFlags(frm, E_MODIFY_FLG_TEMPORARY);
         }
         e_modify->AddFrameFlags(anim_mod, E_MODIFY_FLG_TEMPORARY);
      }
                              //lock editing model to avoid accidental move
      if(lock_anim_model)
         e_modify->AddLockFlags(anim_mod, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE);

                              //add any created frames linked to the original
      struct S_hlp{
         C_vector<PI3D_frame> crt_list;
         PC_editor_item_Modify e_modify;

         static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
            S_hlp *hp = (S_hlp*)c;
            dword flgs = hp->e_modify->GetFrameFlags(frm);
            if(flgs&E_MODIFY_FLG_CREATE)
               hp->crt_list.push_back(frm);
            return I3DENUMRET_OK;
         }
         static bool cbSort(PI3D_frame m0, PI3D_frame m1){
            dword n0 = 0, n1 = 0;
            while(m0=m0->GetParent(), m0) ++n0;
            while(m1=m1->GetParent(), m1) ++n1;
            return (n0 > n1);
         }
      } hlp;
      hlp.e_modify = e_modify;
      src->EnumFrames(S_hlp::cbEnum, (dword)&hlp, ENUMF_ALL);
                              //sort by number of parents
      sort(hlp.crt_list.begin(), hlp.crt_list.end(), S_hlp::cbSort);

      for(dword i=hlp.crt_list.size(); i--; ){
         PI3D_frame crt = hlp.crt_list[i];

         const char *link_name = crt->GetParent()->GetOrigName();
         PI3D_frame link = anim_mod->FindChildFrame(link_name);
         if(!link)
            continue;
         PI3D_frame dup = scn->CreateFrame(crt->GetType());
         dup->Duplicate(crt);
         dup->SetFlags(0);
         anim_mod->AddFrame(dup);
         dup->LinkTo(link);

         e_modify->AddFrameFlags(dup, E_MODIFY_FLG_TEMPORARY);

         dup->Release();
      }

      anim_mod->SetPos(pa.edit_pos);
      anim_mod->SetRot(pa.edit_rot);
      e_slct->Clear();
      e_slct->AddFrame(anim_mod);

      {
         S_vector pos = anim_mod->GetWorldPos();
         const I3D_bbox &bb = anim_mod->GetHRBoundVolume().bbox;
         if(bb.IsValid())
            pos = ((bb.min+bb.max)*.5f) * anim_mod->GetMatrix();
   
         ed->GetScene()->SetFrameSectorPos(anim_mod, pos);
      }

      return true;
   }

//----------------------------

   void CloseAnimModel(){

      if(anim_mod){
                              //remove all from modify plugin
         const PI3D_frame *frms = anim_mod->GetFrames();
         for(int i = anim_mod->NumFrames(); i--; ){
            PI3D_frame frm = frms[i];
            e_modify->RemoveFrame(frm);
            e_slct->RemoveFrame(frm);
         }
         e_modify->RemoveFrame(anim_mod);
         e_slct->RemoveFrame(anim_mod);
         anim_mod->LinkTo(NULL);
         anim_mod = NULL;
      }
      if(hidden_frm){
         hidden_frm->SetOn(true);
         hidden_frm = NULL;
      }
   }

//----------------------------

   bool IsEditAnimValid() const{ return edit_anim_id != -1; }

//----------------------------

   void SetEditAnim(int id, bool save_undo = true){

      if(edit_anim_id==id)
         return;
                              //un-init previous anim
      if(IsEditAnimValid()){
         if(mode == MODE_PLAY)
            PlayModeToggle();

                              //store current pos/rot
         if(anim_mod){
            S_pose_anim_edit &pa = GetEditAnim();
            pa.edit_pos = anim_mod->GetPos();
            pa.edit_rot = anim_mod->GetRot();
         }

         SetAnimFramesTransparency(false);
         UnlockAnimFrames();
         CloseAnimModel();
      }
 
                              //save current anim index into UNDO
      if(save_undo)
         e_undo->Save(this, E_ANIM_SET_EDIT_ANIM, (int*)&edit_anim_id, sizeof(dword));

      edit_anim_id = id;

      if(!IsEditAnimValid()){
         ToggleDialog(false);
         return;
      }

      S_pose_anim_edit &pa = GetEditAnim();
      InitAnimModel(pa);

      UpdateAnimGui();
      anim_play.ResetTime();
      UpdateAnimation();
      LockAnimFrames();
      SetAnimFramesTransparency(model_transparency);

      if(!IsActive()){
         ToggleDialog(true);
      }
      //ApplyAnimMask(GetEditAnim());
#ifdef _DEBUG
      CheckAnimMask();
      CheckAnimSets(GetEditAnim());
      //DebugCheckAnimDelUndo();
#endif
   }

//----------------------------

   C_str NewAnimName(const char *suggest_base) const{

      C_str base_name(suggest_base);
                              //strip numbers from end of base
      for (dword i = base_name.Size(); i--;){
         const char &c = base_name[i];
         bool is_numeric = (c >= '0' && c <= '9');
         if(is_numeric)
            base_name[i] = 0;
         else
            break;            //end when we reach anything else then a number
      }
      C_str name;
                              //try find good name
      for( int j = 0; j < 100; j++){

         name = C_fstr("%s%.2i", (const char*)base_name, j);
                           //search if not exist
         if(!IsAnimNameUsed(name))
            break;
      }
      return name;
   }

//----------------------------
//
   PI3D_animation_set CreateOrigKeyAnimSet(CPI3D_model mod, PI3D_driver drv) const{
      
      assert(mod);
      if(!mod)
         return NULL;

      CPI3D_model orig_mod = mod;

      PI3D_animation_set as = drv->CreateAnimationSet();

      CPI3D_frame const *frms = orig_mod->GetFrames();
      for(int i = orig_mod->NumFrames(); i--; ){

         CPI3D_frame frm_orig = frms[i];
         assert(frm_orig);
         const char *link = frm_orig->GetOrigName();

         PI3D_anim_pose pose = (PI3D_anim_pose) drv->CreateAnimation(I3DANIM_POSE);
         pose->Clear();
         pose->SetPos(&frm_orig->GetPos());
         pose->SetRot(&frm_orig->GetRot());
         float f = frm_orig->GetScale();
         pose->SetPower(&f);
         as->AddAnimation(pose, link);
         pose->Release();
      }
      return as;
   }

//----------------------------

   int InsertKey(S_pose_anim_edit &pa, dword time) const{

      int cg_id = pa.FindClosestGreaterKey(time);

      S_pose_key pk;
      pk.time = time;
      PI3D_animation_set as = CreateOrigKeyAnimSet(anim_mod, ed->GetDriver());
      if(as){
         pk.anim_set = CreateMaskedPoseSet(as);
         as->Release();
         if(pk.anim_set)
            pk.anim_set->Release();
      }

      return pa.AddKey(cg_id, pk);
      //return pa.curr_key;
   }

//----------------------------

   bool CreateNewKey(){

      assert(hWnd_anim_edit);
      if(!hWnd_anim_edit)
         return false;
      if(!ed->CanModify())
         return false;

                              //suggest some time
      int suggest_time(0);
      S_pose_anim_edit &pa = GetEditAnim();
      if((pa.CurrKey() + 1) < pa.key_list.size()){
                              //when between 2 keys, use middle time
         int curr_time = pa.key_list[pa.CurrKey()].time;
         int next_time = pa.key_list[pa.CurrKey() + 1].time;
         assert(next_time > curr_time);
         suggest_time = curr_time + ((next_time - curr_time)/2);
      }else{
                              //when we are on last key, use last key time + some trash
                              //MB: determine key spacing from delta with previous key
         suggest_time = pa.key_list[pa.CurrKey()].time;
         if(pa.CurrKey()) 
            suggest_time += (pa.key_list[pa.CurrKey()].time - pa.key_list[pa.CurrKey()-1].time);
         else 
            suggest_time += 500;
      }
                              //ask user for new key time
      int new_time(0);
      while(true){
         C_str time_str = C_fstr("%i", suggest_time);
         if(!SelectName(ed->GetIGraph(), hWnd_anim_edit, "Enter key time", time_str))
            return false;

         int scan = sscanf(time_str, "%i", &new_time);
         if(scan != 1)
            continue;
                     //check boundary/duplication
         for(int j=pa.key_list.size(); j--; ){
            if(pa.key_list[j].time==new_time){
               //MessageBox(hwnd, "This time already exists!", "Error", MB_OK);
               break;
            }
         }
         if(j==-1)
            break;
      }
      ed->SetModified(true);

                              //during creation, apply current anim, and position it to proposed time
      PI3D_driver drv = ed->GetDriver();
      PI3D_animation_set as = drv->CreateAnimationSet();
      if(pa.ConvertToAnimSet(as, drv)){
         anim_mod->SetAnimation(0, as);
         anim_mod->Tick(new_time);
         anim_mod->StopAnimation(0);
      }
      as->Release();

                              //create the key
      int last_key = pa.CurrKey();
      int insert_id = InsertKey(pa, new_time);
      SaveDelKeyUndo(last_key, insert_id);
      UpdateAnimation();
      ed->Message("New key created.");
      return true;
   }

//----------------------------

   void SaveDelKeyUndo(int after_del_id, int del_id){

      S_key_del u_key_del;
      u_key_del.id_after_del = after_del_id;
      u_key_del.id_to_delete = del_id;
      e_undo->Save(this, E_ANIM_DEL_KEY_ID, &u_key_del, sizeof(S_key_del));
   }

//----------------------------

   void SaveWeightUndo(dword anim_id, const char *link, float w){

      C_vector<byte> undo_buf(sizeof(S_set_weight) + strlen(link), 0);
      //VectorAddElement(undo_buf, sizeof(S_set_weight) + strlen(link), 0);
      S_set_weight &u_weight = (S_set_weight&)undo_buf.front();
      u_weight.anim_id = anim_id;
      u_weight.weight = w;
      strcpy(u_weight.link, link);
      e_undo->Save(this, E_ANIM_UNDO_WEIGHT, &undo_buf.front(), undo_buf.size());
   }

//----------------------------

   void SaveAddKeyUndo(const S_pose_anim_edit &pa, int add_id){

      C_vector<byte> undo_buf;
      CreateKeyUndoInfo(undo_buf, pa.key_list[add_id], add_id);
      e_undo->Save(this, E_ANIM_ADD_KEY_ID, &undo_buf.front(), undo_buf.size());
   }

//----------------------------

   void SaveGotoKeyUndo(){

      int key_id = GetEditAnim().CurrKey();
      e_undo->Save(this, E_ANIM_GOTO_KEY, &key_id, sizeof(int));
   }

//----------------------------

   bool DeleteKey(dword id){

      if(!ed->CanModify())
         return false;

      S_pose_anim_edit &pa = GetEditAnim();

                              //cant' delete last key, unless we destroy animation
      if(pa.key_list.size() == 1)
         return false;

      if(id >= pa.key_list.size())
         return false;
                              //undo
      SaveAddKeyUndo(pa, id);
      pa.DelKey(id);
      ed->SetModified(true);
      ed->Message("Key deleted.");
      e_slct->FlashFrame(anim_mod);
      return true;
   }

//----------------------------

   bool IsAnimNameUsed(const char *name) const{

      C_str name_low(name);
      name_low.ToLower();
                  //check duplication
      for(int i=anims.size(); i--; ){
         C_str anim_low(anims[i].name);
         anim_low.ToLower();
         if(anim_low == name_low)
            break;
      }
      return (i != -1);
   }

//----------------------------

   void TerminateUndo(){

      e_undo->Clear();
   }

//----------------------------

   bool CreateNewAnim(HWND hwnd, PI3D_model sel_mod){

      if(!ed->CanModify())
         return false;
      assert(sel_mod);

      S_anim_init ai;
      ai.name = NewAnimName("new_anim");
      ai.mask_id = 0;
      ai.cust_mask.CreateCustom(sel_mod);

      while(true){
                              //create child of window from which are called
         if(!SelectAnimParams(ed->GetIGraph(), hwnd ? hwnd : (HWND)ed->GetIGraph()->GetHWND(), "Create new anim", ai))
            return false;
         if(!ai.name.Size())
            continue;
         if(!IsAnimNameUsed(ai.name))
            break;
      }
      ed->SetModified(true);

                              //close previous edit
      SetEditAnim(-1);

      dword new_anim_id = anims.size();
      //SaveDelAnimUndo(new_anim_id, edit_anim_id);
      TerminateUndo();
      anims.push_back(S_pose_anim_edit());
      S_pose_anim_edit &new_anim = anims.back();

      new_anim.name = ai.name;
      new_anim.model_name = sel_mod->GetName();

      new_anim.edit_pos = sel_mod->GetPos();
      new_anim.edit_rot = sel_mod->GetRot();
                              //every anim must have at least 1 key (pose)
      new_anim.AddKeyDefault(ed->GetDriver());
      SetAnimMask(new_anim, ai.mask_id, &ai.cust_mask);
      SetEditAnim(new_anim_id);
      UpdateAnimation();
      ed->Message("New anim created.");

      return true;
   }

//----------------------------

   PI3D_animation_set DuplicateAnimSet(CPI3D_animation_set from, PI3D_driver drv) const{

      assert(from);
      assert(drv);
      PI3D_animation_set to = drv->CreateAnimationSet();
      assert(!to->NumAnimations());
      for (dword i = 0; i < from->NumAnimations(); i++){
         CPI3D_animation_base ab = ((PI3D_animation_set)from)->GetAnimation(i);
         assert(ab->GetType() == I3DANIM_POSE);
         PI3D_anim_pose pose_from = (PI3D_anim_pose)ab;
         PI3D_anim_pose pose_to = (PI3D_anim_pose)drv->CreateAnimation(I3DANIM_POSE);
         pose_to->SetPos(pose_from->GetPos());
         pose_to->SetRot(pose_from->GetRot());
         pose_to->SetPower(pose_from->GetPower());
         to->AddAnimation(pose_to, from->GetAnimLink(i));
         pose_to->Release();
      }
      return to;
   }

//----------------------------

   bool SelectAnimName(HWND hwnd_parent, const char *suggest_name, C_str &name) const{

      name = NewAnimName(suggest_name);

      bool cancel(false);
      while(true){
         if(!SelectName(ed->GetIGraph(), hwnd_parent, "Enter anim name", name)){
            cancel = true;
            break;
         }
         if(!name.Size())
            continue;
         if(!IsAnimNameUsed(name))
            break;
      }
      if(cancel)
         return false;
      return true;
   }

//----------------------------

   bool DuplicateAnim(HWND hwnd_parent, dword id){

      assert(id < anims.size());

      C_str name;
      if(!SelectAnimName(hwnd_parent, anims[id].name, name))
         return false;

      dword new_anim_id = anims.size();
      //SaveDelAnimUndo(new_anim_id, edit_anim_id);
      TerminateUndo();

      anims.push_back(S_pose_anim_edit());
      S_pose_anim_edit &new_anim = anims.back();

      const S_pose_anim_edit &pa_src = anims[id];

      new_anim.name = name;
      new_anim.model_name = pa_src.model_name;
      new_anim.edit_pos = pa_src.edit_pos;
      new_anim.edit_rot = pa_src.edit_rot;

      for(dword i = 0; i < pa_src.key_list.size(); i++){

         const S_pose_key &key_src = pa_src.key_list[i];
         //new_anim.key_list.push_back(S_pose_key());
         new_anim.key_list.resize(new_anim.key_list.size()+1);
         S_pose_key &new_key = new_anim.key_list.back();
                              //use copy constructor..
         new_key = key_src;
                              //.. and just override creations we need
         new_key.anim_set = DuplicateAnimSet(key_src.anim_set, ed->GetDriver());
         if(new_key.anim_set)
            new_key.anim_set->Release();
      }
      SetAnimMask(new_anim, pa_src.GetMaskId(), &pa_src.GetCustomMask());

      SetEditAnim(new_anim_id);

      return true;
   }

//----------------------------

   void CorrectEditAnimId(int id, bool added){

      if(!IsEditAnimValid())
         return;
                              //adding/removing id after edit_anim_id don't invalidate it
      if(id > edit_anim_id)
         return;

      if(added){
                              //added id before edit, edit_id is greater by one now
         edit_anim_id += 1;
         assert(edit_anim_id < (int)anims.size());
      }else{
                              //deleted before edit, edit_id is smaller now
         edit_anim_id -= 1;
         assert((int)edit_anim_id >= 0);
      }
   }

//----------------------------

   bool DeleteAnim(HWND hwnd, int id){

      if(!ed->CanModify())
         return false;

      assert(id < (int)anims.size());
      S_pose_anim_edit *pa = &anims[id];

      C_fstr msg("Are you sure to delete anim '%s'?", (const char*)pa->name);
      if(MessageBox(hwnd, (const char*)msg, "Deleting anim", MB_OKCANCEL) != IDOK)
         return false;

      if(id == edit_anim_id){
                              //close anim editor, because we're deleting active anim
         Action(E_ANIM_CLOSE_EDIT);
      }

      ed->SetModified(true);

      //SaveAddAnimUndo(id);
      TerminateUndo();
      anims.erase(anims.begin()+id);

      CorrectEditAnimId(id, false);
      return true;
   }

//----------------------------

   void ClearAnimSet(PI3D_animation_set anim_set) const{

      assert(anim_set);
      for (dword i = anim_set->NumAnimations(); i--;)
         anim_set->RemoveAnimation(anim_set->GetAnimation(i));
   }

//----------------------------
// Store poses from mod to anim_set;
// Animation set with all model's frames is created and entire pose is stored. 
   void StoreFullPoseIntoSet(PI3D_model model, PI3D_animation_set anim_set) const{

      assert(("StoreFullPoseIntoSet: invalid anim_set", anim_set));
      assert(("StoreFullPoseIntoSet: invalid model", model));

                              //remove old anims
      ClearAnimSet(anim_set);
                              //store new ones from current model pose
      PI3D_frame const *frms = model->GetFrames();
      for (int i = model->NumFrames(); i--; ){
         PI3D_frame frm_from = frms[i];
         assert(frm_from);

         const char *link_name = frm_from->GetOrigName();
         PI3D_anim_pose pose_to = (PI3D_anim_pose) ed->GetDriver()->CreateAnimation(I3DANIM_POSE);
         anim_set->AddAnimation(pose_to, link_name);
         pose_to->Release();

         pose_to->SetPos(&frm_from->GetPos());
         pose_to->SetRot(&frm_from->GetRot());
         float scale = frm_from->GetScale();
         pose_to->SetPower(&scale);
      }
   }

//----------------------------

   void CopyPoseToClipbord(PI3D_model mod){

      if(!pose_clipboard){
         assert(ed && ed->GetDriver());
         pose_clipboard = ed->GetDriver()->CreateAnimationSet();
         pose_clipboard->Release();
      }
      StoreFullPoseIntoSet(mod, pose_clipboard);
      e_slct->FlashFrame(mod);
   }

//----------------------------

   void PastePoseFromClipboard(){

      if(!pose_clipboard)
         return;

      if(!ed->CanModify())
         return;
      ed->SetModified(true);

      PI3D_animation_set as_masked = CreateMaskedPoseSet(pose_clipboard);
      SetKeyAnimSet(as_masked);
      as_masked->Release();
      e_slct->FlashFrame(anim_mod);
      ed->Message("Pasted pose from clipboard.");
   }

//----------------------------

   PI3D_animation_set CreateMirroredPoseSet(PI3D_animation_set anim_set){

      PI3D_animation_set as_ret = ed->GetDriver()->CreateAnimationSet();

      for(dword i = anim_set->NumAnimations(); i--;){

         const C_str &link_name = anim_set->GetAnimLink(i);
         PI3D_anim_pose pose_from = (PI3D_anim_pose)anim_set->GetAnimation(i);
         C_smart_ptr<I3D_anim_pose> mirr_pose = CreateMirrorPose(*pose_from->GetPos(), *pose_from->GetRot(), *pose_from->GetPower());
         mirr_pose->Release();

         bool r_side = link_name.Match("r_*");
         bool l_side = link_name.Match("l_*");

         if(r_side || l_side){
                              //find opposite
            C_str str_oposite = r_side ? "l_" : "r_";
            for(dword j = 2; j < link_name.Size(); j++){
               str_oposite += C_fstr("%c", link_name[j]);
            }
            const char *link2 = (const char*)str_oposite;
            int opp_id = FindAnimId(anim_set, link2);
                              //if opposite found, write under it's link
            if(opp_id != -1){
               as_ret->AddAnimation(mirr_pose, link2);
               continue;
            }
         }
                              //link with no opposite mirror by self (central parts)
         as_ret->AddAnimation(mirr_pose, link_name);                              
      }
      return as_ret;
   }

//----------------------------

   void PastePoseFromClipboardMirrored(){

      if(!pose_clipboard)
         return;
      if(!ed->CanModify())
         return;
      ed->SetModified(true);


      C_smart_ptr<I3D_animation_set> mirr_pose = CreateMirroredPoseSet(pose_clipboard);
      mirr_pose->Release();
      C_smart_ptr<I3D_animation_set> as_masked = CreateMaskedPoseSet(mirr_pose);
      as_masked->Release();
      SetKeyAnimSet(as_masked);
      e_slct->FlashFrame(anim_mod);
      ed->Message("Pasted mirrored pose from clipboard.");

   }

//----------------------------

   static void CopyPose2Pose(PI3D_anim_pose pose_src, PI3D_anim_pose pose_dest){

      if(!pose_src || !pose_dest)
         return;
      pose_dest->SetPos(pose_src->GetPos());
      pose_dest->SetRot(pose_src->GetRot());
      pose_dest->SetPower(pose_src->GetPower());
   }

//----------------------------
// Similar to PastePoseFromClipboard, but paste only to selected frames.
// If all_keys is true, paste is used for all keys, otherwise only current key is used to apply.
   void PasteClipboardSelection(const PI3D_frame *frms, dword num, bool all_keys){

      if(!pose_clipboard)
         return;

      if(!IsEditAnimValid())
         return;

      S_pose_anim_edit &pa = GetEditAnim();
      if(!anim_mod)
         return;

      if(!ed->CanModify())
         return;
      ed->SetModified(true);

      C_smart_ptr<I3D_animation_set> as_masked = CreateMaskedPoseSet(pose_clipboard);
      as_masked->Release();

      const PI3D_frame *frms_model = anim_mod->GetFrames();
      int num_model = anim_mod->NumFrames();

      int pasted_frames = 0;

      for(int i = num; i--; ){

         PI3D_frame frm = frms[i];
                              //skip  frames which are not part of edited model

         if(FindPointerInArray((void **)frms_model, num_model, frm) == -1)
            continue;
                              //skip frames which are not in anim
         const char *link = frm->GetOrigName();
         int anim_id = FindAnimId(as_masked, link);
         if(anim_id == -1)
            continue;

         CPI3D_animation_base ab = as_masked->GetAnimation(anim_id);
         assert(ab->GetType() == I3DANIM_POSE);
         PI3D_anim_pose pose_src = (PI3D_anim_pose)ab;

         if(all_keys){
            SaveFrameTransformKeysUndo(frm);

            for(int j = pa.key_list.size(); j--; ){
               S_pose_key &pk = pa.key_list[j];

               int dest_id = FindAnimId(pk.anim_set, link);
               if(dest_id == -1){
                  assert(("anim for paste not found in key.", 0)); //all anims which are in as_masked should be also in all keys 
                  continue; 
               }
               CPI3D_animation_base ab_dest = pk.anim_set->GetAnimation(dest_id);
               assert(ab_dest->GetType() == I3DANIM_POSE);
               PI3D_anim_pose pose_dest = (PI3D_anim_pose)ab_dest;

               CopyPose2Pose(pose_src, pose_dest);
            }
         }else{
            SaveFrameTransformUndo(frm);
            //CopyPose2Frame(pose_src, frm);
            const S_mask_mode &mask_mode = GetEditMask();
            S_pose_anim_edit::CopyPose2Frame(pose_src, frm, mask_mode.GetEnabled(link), e_props, e_modify);
         }

         e_slct->FlashFrame(frm);
         pasted_frames++;
      }
      C_str msg;
      if(all_keys){
         msg = C_fstr("%i frames pasted on %i key(s).", pasted_frames, pa.key_list.size());
         UpdateModel();
      }else{
         msg = C_fstr("%i frames pasted.", pasted_frames);
      }
      SavePose2Key();
      ed->Message(msg);
   }

//----------------------------

   void SaveFrameTransformKeysUndo(PI3D_frame frm){

      if(!IsEditAnimValid())
         return;

      C_undo_buf ub;
      ub.InitW();

      const char *link = frm->GetOrigName();
      S_pose_anim_edit &pa = GetEditAnim();
      dword num_k = pa.key_list.size();
      ub.WriteString(link);
      ub.WriteMem(&num_k, sizeof(int));

      for(int i = pa.key_list.size(); i--; ){

         int id = FindAnimId(pa.key_list[i].anim_set, link);
         assert(id != -1);
         if(id == -1)
            continue;
         assert(pa.key_list[i].anim_set->GetAnimation(id)->GetType() == I3DANIM_POSE);
         PI3D_anim_pose pose = (PI3D_anim_pose) pa.key_list[i].anim_set->GetAnimation(id);
         bool has_pos = pose->GetPos();
         bool has_rot = pose->GetRot();
         bool has_power = pose->GetPower();

         ub.WriteMem(&has_pos, sizeof(bool));
         if(has_pos)
            ub.WriteMem(pose->GetPos(), sizeof(S_vector));

         ub.WriteMem(&has_rot, sizeof(bool));
         if(has_rot)
            ub.WriteMem(pose->GetRot(), sizeof(S_quat));

         ub.WriteMem(&has_power, sizeof(bool));
         if(has_power)
            ub.WriteMem(pose->GetPower(), sizeof(float));
      }
      e_undo->Save(this, E_ANIM_UNDO_FRM_TRANFORM_KEYS, ub.Begin(), ub.Size());
   }

//----------------------------

   void UndoFrameTransformKeys(void *buf){

      if(!IsEditAnimValid())
         return;

      S_pose_anim_edit &pa = GetEditAnim();

      if(!anim_mod)
         return;

      C_undo_buf ub;
      ub.InitR(buf);
      C_str link;
      ub.ReadString(link);
      dword num_keys;
      ub.ReadMem(&num_keys, sizeof(int));

//      PI3D_frame frm = anim_mod->FindChildFrame(link);
      PI3D_frame frm = FindModelChild(anim_mod, link);

      if(!frm)
         return;

      SaveFrameTransformKeysUndo(frm);

      assert(num_keys == (int)pa.key_list.size());
      if(num_keys != pa.key_list.size())
         return;
      for(int i = num_keys; i--; ){

         S_vector pos;
         S_quat rot;
         float pow;
         bool has_pos, has_rot, has_power;

         ub.ReadMem(&has_pos, sizeof(bool));
         if(has_pos)
            ub.ReadMem(&pos, sizeof(S_vector));
         ub.ReadMem(&has_rot, sizeof(bool));
         if(has_rot)
            ub.ReadMem(&rot, sizeof(S_quat));
         ub.ReadMem(&has_power, sizeof(bool));
         if(has_power)
            ub.ReadMem(&pow, sizeof(float));

         PI3D_animation_set as = pa.key_list[i].anim_set;
         int id = FindAnimId(as, link);
         if(id == -1)
            continue;
         CPI3D_animation_base ab = as->GetAnimation(id);
         assert(ab->GetType() == I3DANIM_POSE);
         PI3D_anim_pose pose = (PI3D_anim_pose)ab;
         if(has_pos)
            pose->SetPos(&pos);
         if(has_rot)
            pose->SetRot(&rot);
         if(has_power)
            pose->SetPower(&pow);
      }
      UpdateAnimation();
      e_slct->FlashFrame(frm);
                              //notify properties, so that sheets are updated
      //e_props->Action(E_PROP_NOTIFY_MODIFY, &pair<PI3D_frame, dword>(frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE));
      e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE);
   }

//----------------------------
// Create animation set from given set and current animation mask and used frames.
   PI3D_animation_set CreateMaskedPoseSet(PI3D_animation_set anim_set) const{

      const S_pose_anim_edit &pa = GetEditAnim();

      assert(("CreateMaskedPoseSet: invalid anim_set", anim_set));
      if(!anim_set)
          return NULL;

      PI3D_animation_set as_ret = ed->GetDriver()->CreateAnimationSet();

      const S_mask_mode &mask_mode = GetEditMask();

      for(dword i = anim_set->NumAnimations(); i--;){

         PI3D_anim_pose pose_from = (PI3D_anim_pose)anim_set->GetAnimation(i);
         const C_str &link_name = anim_set->GetAnimLink(i);
         if(!pa.IsInFramesList(link_name))
            continue;

         bool locked_pos = mask_mode.Locked(link_name, TR_POS);
         bool locked_rot = mask_mode.Locked(link_name, TR_ROT);
         bool locked_pow = mask_mode.Locked(link_name, TR_POW);


                              //we won't store empty anim
         if(locked_pos && locked_rot && locked_pow)
            continue;

         PI3D_anim_pose pose_to = (PI3D_anim_pose)ed->GetDriver()->CreateAnimation(I3DANIM_POSE);
         pose_to->Clear();
         pose_to->SetPos(locked_pos ? NULL : pose_from->GetPos());
         pose_to->SetRot(locked_rot ? NULL : pose_from->GetRot());
         pose_to->SetPower(locked_pow ? NULL : pose_from->GetPower());
                              //anim should have at least one key valid
         if(pose_to->GetPos() || pose_to->GetRot() || pose_to->GetPower())   
            as_ret->AddAnimation(pose_to, link_name);
         pose_to->Release();
      }
      return as_ret;
   }

//----------------------------

   void SetKeyAnimSet(PI3D_animation_set as){

      assert(as);
                              //save undo: store anim set
      C_vector<byte> undo_buf;
      S_pose_key &curr_key = GetEditAnim().CurrPoseKey();
      CreateAnimSetUndo(undo_buf, curr_key.anim_set);
      e_undo->Save(this, E_ANIM_SET_KEY_ANIM_SET, &undo_buf.front(), undo_buf.size());

      curr_key.anim_set = as;

      UpdateModel(); 
      UpdateAnimation();
   }

//----------------------------
//user interface
//----------------------------
// make_edit_active is hint which sheet should be active. (true for edit, false for frames).
   void ToggleDialog(bool on_off, bool make_edit_active = true){

      static const S_tmp_shortcut temp_shortcuts[] = {
         {E_ANIM_FOCUS_ANIM_EDIT, K_F3, 0, "Focus animator editor window"},
         {E_ANIM_FOCUS_FRAMES, K_F4, 0, "Focus animator frame setup sheet"},
         {E_ANIM_SET_KEY_TIME_DIRECT, K_T, SKEY_SHIFT, "Set key time"},
         {E_ANIM_INSERT_TIME, K_T, SKEY_SHIFT|SKEY_CTRL|SKEY_ALT, "Insert key time"},
         {E_ANIM_PLAY_TOGGLE, K_P, 0, "Toggle animation playback"},
         {E_ANIM_PLAY_STOP, K_ESC, 0, "Stop animation playback"},
         {E_ANIM_SET_WEIGHT, K_W, SKEY_SHIFT, "Set weight of selected frame"},
         {E_ANIM_COPY_POSE, K_INS, SKEY_CTRL, "Copy pose"},
         {E_ANIM_PASTE_POSE, K_INS, SKEY_SHIFT, "Paste pose"},
         {E_ANIM_PASTE_MIRROR, K_INS, SKEY_CTRL|SKEY_SHIFT, "Paste mirror"},
         {E_ANIM_PASTE_SELECTION, K_INS, SKEY_CTRL|SKEY_SHIFT|SKEY_ALT, "Paste pose to selection"},
         {E_ANIM_PASTE_SELECTION_ALL_KEYS, K_I, SKEY_CTRL|SKEY_SHIFT|SKEY_ALT, "Paste pose to selection on all keys"},
         //{E_ANIM_PICK_NOTIFY_FRM, K_N, SKEY_SHIFT, "Pick notify frame"},
         {E_ANIM_NOTE_CLEAR, K_N, SKEY_SHIFT|SKEY_CTRL|SKEY_ALT, "Clear notify"},
         {E_ANIM_ADD_KEY, K_INS, SKEY_ALT, "Add key"},
         {E_ANIM_DEL_KEY, K_DEL, SKEY_ALT, "Delete key"},
         {E_ANIM_PREV_KEY, K_CURSORLEFT, 0, "Previous key"},
         {E_ANIM_NEXT_KEY, K_CURSORRIGHT, 0, "Next key"},
         {E_ANIM_PREV_KEY2, K_CURSORLEFT, SKEY_CTRL, "2 key back"},
         {E_ANIM_NEXT_KEY2, K_CURSORRIGHT, SKEY_CTRL, "2 keys forward"},

#if 0
//         {E_ANIM_PLAY_TIME_INC, K_CURSORRIGHT, SKEY_SHIFT, "Increase curr time"},
//         {E_ANIM_PLAY_TIME_DEC, K_CURSORLEFT, SKEY_SHIFT, "Decrease curr time"},
#endif

         {E_ANIM_FIRST_KEY, K_HOME, 0, "First key"},
         {E_ANIM_LAST_KEY, K_END, 0, "Last key"},
         {E_ANIM_TOGGLE_MODEL_TRANSP, K_T, SKEY_SHIFT|SKEY_CTRL, "Toggle model transparency"},
         {E_ANIM_RESET_SEL_FRAMES, K_HOME, SKEY_SHIFT, "Reset selection transform"},
         {E_ANIM_SMOOTH_ANIM_TIME, K_S, SKEY_SHIFT|SKEY_CTRL|SKEY_ALT, "Smooth anim velocity (constant speed)"},
         {E_ANIM_LOOP_MODE, K_I, 0, "Toggle loop mode"},
         //{E_ANIM_MIRROR_POSE, K_M, 0, "Mirror pose"},
         0
      };

      if(on_off){

         mode = MODE_EDIT;
                           //show sheets
         if(!hWnd_anim_edit){
            {
               hWnd_anim_edit = CreateDialogParam(GetHInstance(), "IDD_ANIM_EDIT", (HWND)ed->GetIGraph()->GetHWND(), AnimEditCbProc_Thunk, (LPARAM)this);
               ed->GetIGraph()->AddDlgHWND(hWnd_anim_edit);
               ShowWindow(hWnd_anim_edit, true);
            }
            if(hWnd_anim_edit){
               UpdateAssingModelButton();
               ed->InstallTempShortcuts(this, temp_shortcuts);
            }
         }
         if(!hWnd_anim_frames){
            hWnd_anim_frames = CreateDialogParam(GetHInstance(), "IDD_ANIM_FRAMES", NULL, cbAnimFrames, (LPARAM)this);
            if(hWnd_anim_frames){
               e_props->AddSheet(hWnd_anim_frames);
               if(!make_edit_active)
                  e_props->ShowSheet(hWnd_anim_frames);
            }
         }
         UpdateAnimGui();
      }else{

         mode = MODE_OFF;
                              //hide sheets
         if(hWnd_anim_edit){

            StoreAnimEditWndPos();
            ed->GetIGraph()->RemoveDlgHWND(hWnd_anim_edit);
            DestroyWindow(hWnd_anim_edit);
            hWnd_anim_edit = NULL;
            ed->RemoveTempShortcuts(this);
         }
         if(hWnd_anim_frames){
            e_props->RemoveSheet(hWnd_anim_frames);
            DestroyWindow(hWnd_anim_frames);
            hWnd_anim_frames = NULL;
         }
      }
   }

//----------------------------

   void StoreAnimEditWndPos() const{

      assert(hWnd_anim_edit);
      RECT rc;
      GetWindowRect(hWnd_anim_edit, &rc);

      anim_edit_wnd_pos.x = rc.left;
      anim_edit_wnd_pos.y = rc.top;
      anim_edit_wnd_pos.valid = true;
   }

//----------------------------

   void FillMaskListView(HWND hwnd_lv, dword sel_id, const S_mask_mode *pcustom){

      assert(hwnd_lv);
      SendMessage(hwnd_lv, LVM_DELETEALLITEMS, 0, 0);

      LVITEM li;
      memset(&li, 0, sizeof(li));

      assert(sel_id || pcustom);
      const S_mask_mode &mm = (sel_id) ? mask_modes.GetMode(sel_id) : *pcustom;

      for(S_mask_mode::t_mode_map::const_iterator it = mm.GetMap().begin(); it != mm.GetMap().end(); it++)
      {

         const char *link = (*it).first;

         li.pszText = (char*)link;
         //li.mask = LVIF_TEXT;
         li.mask = LVIF_TEXT | LVIF_PARAM;
         li.lParam = (LPARAM)link;
         int indx = SendMessage(hwnd_lv, LVM_INSERTITEM, 0, (LPARAM)&li);

                              //subitem text
         //SetItemMaskText(hwnd_lv, indx, link, sel_id);
         SetItemMaskText(hwnd_lv, indx, link, mm);

      }
   }

//----------------------------

   void SetItemMaskText(HWND hwnd_lv, int item_indx, const char*link, const S_mask_mode &mm) const{

      dword mask = mm.GetEnabled(link);
      LVITEM itm;
      memset(&itm, 0, sizeof(itm));
      itm.mask = LVIF_TEXT;
      C_str flag_str;
      for(int k = 1; k < 4; k++){
         switch(k){
         case 1: flag_str = ((mask&TR_POS) ? "P" : "_"); break;
         case 2: flag_str = ((mask&TR_ROT) ? "R" : "_"); break;
         case 3: flag_str = ((mask&TR_POW) ? "S" : "_"); break;
         }
         itm.iSubItem = k;
         itm.pszText = (char *) (const char*)flag_str;
         SendMessage(hwnd_lv, LVM_SETITEMTEXT, item_indx, (LPARAM) &itm);
      }
   }

//----------------------------
// Fill frame list frames from mask which are also in model.
   void FillFrameList2() const{

      if(!anim_mod)
         return;
      if(!hWnd_anim_frames)
         return;

      LVITEM li;
      memset(&li, 0, sizeof(li));
      li.stateMask = LVIS_STATEIMAGEMASK;
      HWND hwnd_lc = GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST);

      assert(hwnd_lc);
      SendMessage(hwnd_lc, LVM_DELETEALLITEMS, 0, 0);

      const S_mask_mode &mm = GetEditMask();
      

      CPI3D_frame const *frms = anim_mod->GetFrames();
      for(dword i = anim_mod->NumFrames(); i--;){
         assert(frms);
         CPI3D_frame frm = frms[i];

         const char *link = frm->GetOrigName();
                              //skip all frames which are not in mask
         if(!mm.IsInMask(link))
            continue;
                              //skip all frames which are fully locked
         if(!mm.GetEnabled(link))
            continue;

         C_str spaced_name;
         {
            PI3D_frame frm_parent = frm->GetParent();
            while(frm_parent){
               spaced_name += ".";
               frm_parent = frm_parent->GetParent();
            }
            spaced_name += frm->GetOrigName();
         }

         li.pszText = (char*)(const char*)spaced_name;
         li.mask = LVIF_TEXT | LVIF_PARAM;
         li.lParam = (LPARAM)frm;
         int indx = SendMessage(hwnd_lc, LVM_INSERTITEM, 0, (LPARAM)&li);

         li.mask = LVIF_STATE;
                              //use list: -1 = uninit, 0 = not in, 1 = in, 2 = indeterminate
         li.state = INDEXTOSTATEIMAGEMASK(1 + 0);
         SendMessage(hwnd_lc, LVM_SETITEMSTATE, indx, (LPARAM)&li);
      }
      UpdatePRSWColumns();
   }

//----------------------------

   void UpdatePRSWColumns() const{

      HWND hwnd_lc = GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST);
      int num_i = SendMessage(hwnd_lc, LVM_GETITEMCOUNT, 0, 0);

      const S_mask_mode &mm = GetEditMask();
      const S_pose_anim_edit &pa = GetEditAnim();
      
      for(dword i = num_i; i--;){

                              //get lParam (frm) info from item
         LVITEM li;
         memset(&li, 0, sizeof(li));
         li.iItem = i;
         li.mask = LVIF_PARAM;
         SendMessage(hwnd_lc, LVM_GETITEM, 0, (LPARAM) &li);
         CPI3D_frame frm = (PI3D_frame)li.lParam;

         assert(frm);

         const char *link = frm->GetOrigName();

                              //subitem text
         for (int k = 1; k < 5; k++){
            LVITEM itm;
            memset(&itm, 0, sizeof(itm));
            itm.mask = LVIF_TEXT;
            itm.iSubItem = k;
            C_str txt;
            dword mask = mm.GetEnabled(frm->GetOrigName());
            switch(k){
            case 1: txt = (mask&TR_POS) ? "p" : ""; break;
            case 2: txt = (mask&TR_ROT) ? "r" : ""; break;
            case 3: txt = (mask&TR_POW) ? "s" : ""; break;
            case 4: txt = C_fstr("%.1f", pa.GetWeight(link)); break;
            }

            itm.pszText = (char *) (const char *) txt;

            SendMessage(hwnd_lc, LVM_SETITEMTEXT, i, (LPARAM) &itm);
         }
      }
   }


//----------------------------

   void FillSelectionListbox(HWND hwnd) const{

      HWND hwnd_tv = GetDlgItem(hwnd, IDC_ANIMS_TREE);
                              //delete all items
      SendMessage(hwnd_tv, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);

      typedef map<C_str, HTREEITEM> t_dirs;
      t_dirs dirs;

      HTREEITEM itm_visible = NULL;

      TVINSERTSTRUCT is;
      memset(&is, 0, sizeof(is));
      is.hInsertAfter = TVI_SORT;
      for(dword ai = 0; ai < anims.size(); ai++){
         C_str name = anims[ai].name;
         bool active = (ai==(dword)edit_anim_id);

         HTREEITEM  prnt = TVI_ROOT;
                              //find dir
         dword last_beg = 0;
         for(dword ni=0; ni<name.Size(); ni++){
            if(name[ni]=='/'){
               name[ni] = 0;
               C_str dir_name((const char*)name);
               t_dirs::iterator it = dirs.find(dir_name);
               if(it==dirs.end()){
                           //insert new base into directory
                  is.item.mask = TVIF_PARAM | TVIF_TEXT | TVIF_STATE;
                  is.hParent = prnt;
                  is.item.lParam = -1;
                  is.item.pszText = (char*)&name[last_beg];
                  is.item.cchTextMax = strlen(is.item.pszText);
                  is.item.state = TVIS_BOLD;
                  is.item.stateMask = 0xff;
                              //check if this anim is active
                  //if(active) is.item.state = TVIS_EXPANDED;
                  prnt = (HTREEITEM)SendMessage(hwnd_tv, TVM_INSERTITEM, 0, (LPARAM)&is);
                  dirs[dir_name] = prnt;
               }else{
                  prnt = (*it).second;
               }
               name[ni] = '/';
               last_beg = ni + 1;
            }
         }
         is.item.mask = TVIF_PARAM | TVIF_TEXT;
         is.hParent = prnt;
         is.item.lParam = ai;
         is.item.pszText = (char*)&name[last_beg];
         is.item.cchTextMax = strlen(is.item.pszText);
         prnt = (HTREEITEM)SendMessage(hwnd_tv, TVM_INSERTITEM, 0, (LPARAM)&is);
         if(active){
            SendMessage(hwnd_tv, TVM_SELECTITEM, TVGN_CARET, (LPARAM)prnt);
            itm_visible = prnt;
         }
      }
      if(itm_visible)
         SendMessage(hwnd_tv, TVM_ENSUREVISIBLE, 0, (LPARAM)itm_visible);

                              //finally, for all dirs, write make counte
      for(t_dirs::iterator it=dirs.begin(); it!=dirs.end(); it++){
         HTREEITEM hti = (*it).second;
         dword count = 0;
         HTREEITEM chld = (HTREEITEM)SendMessage(hwnd_tv, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)hti);
         assert(chld);
         do{
            ++count;
            chld = (HTREEITEM)SendMessage(hwnd_tv, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)chld);
         }while(chld);

         char buf[256];
         TVITEM tvi;
         //memset(tvi, 0, sizeof(tvi));
         tvi.mask = TVIF_TEXT;
         tvi.hItem = hti;
         tvi.pszText = buf;
         tvi.cchTextMax = sizeof(buf);
         if(SendMessage(hwnd_tv, TVM_GETITEM, 0, (LPARAM)&tvi)){
            C_fstr str("%s (%i)", buf, count);
            tvi.pszText = (char*)(const char*)str;
            tvi.cchTextMax = strlen(tvi.pszText);
            SendMessage(hwnd_tv, TVM_SETITEM, TVGN_CHILD, (LPARAM)&tvi);
         }
      }
   }

//----------------------------

   struct S_select_anim_hlp{

      int curr_id;
      bool can_create_new;
      C_edit_AnimEdit *ae;
      bool browse_only;
   };

//----------------------------

   PI3D_model GetNewAnimModel(bool err_message = false){

      PI3D_frame sel_frm = e_slct->GetSingleSel();
      if(!sel_frm){
         if(err_message)
            ed->Message("Anim create: Single selection required.");
         return NULL;
      }
      if(sel_frm->GetType() != FRAME_MODEL){
         if(err_message)
            ed->Message("Anim create: Model selection required.");
         return NULL;
      }
      if(sel_frm==anim_mod)
         return hidden_frm;

      return I3DCAST_MODEL(sel_frm);
   }

//----------------------------
// Select edit animation.
   int SelectEditAnim(HWND hwnd_parent, bool browse_only = false){

      S_select_anim_hlp hlp;
      hlp.curr_id = edit_anim_id;
      hlp.ae = this;
      hlp.can_create_new = GetNewAnimModel();
      hlp.browse_only = browse_only;
      bool b = DialogBoxParam(GetHInstance(), "IDD_ANIM_SELECT", hwnd_parent,
         cbSelectAnim, (LPARAM)&hlp);
      if(!b)
         return -1;
      if(!anims.size()){
         ToggleDialog(false);
         return -1;
      }
      if(hlp.curr_id != -1 && !browse_only){
         SetEditAnim(hlp.curr_id);
         ToggleDialog(true);
      }
      return hlp.curr_id;
   }

//----------------------------

   PI3D_model GetModelForNewMask(){

      PI3D_frame frm = (IsEditAnimValid()) ? anim_mod : e_slct->GetSingleSel();
      if(frm && (frm->GetType() == FRAME_MODEL))
         return I3DCAST_MODEL(frm);
      else
         return NULL;
   }

//----------------------------

   void UpdaetMasksEdit(HWND hwnd){

      dword sel_id = SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);

      bool can_edit = (sel_id == CB_ERR) ? false : mask_modes.CanEdit();
      //EnableWindow(GetDlgItem(hwnd, IDC_FRM_LIST2), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_ADD_FRAME), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_REMOVE_FRAME), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_POS_CHECK), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_ROT_CHECK), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_POW_CHECK), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_TRANS_ALL), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_TRANS_CLEAR), (sel_id == 0) ? true : can_edit);
      EnableWindow(GetDlgItem(hwnd, IDC_DELETE_MODE), (sel_id == 0) ? false : can_edit);      
   }

//----------------------------

   void InitMaskDialog(HWND hwnd, dword mode_index, S_mask_mode *pcustom){

      HWND hwnd_cb = GetDlgItem(hwnd, IDC_MODE_SELECT);
      FillMaskCB(hwnd_cb);
      for(dword i=SendMessage(hwnd_cb, CB_GETCOUNT, 0, 0); i--; ){
         if((dword)SendMessage(hwnd_cb, CB_GETITEMDATA, i, 0)==mode_index){
            SendMessage(hwnd_cb, CB_SETCURSEL, i, 0);
            break;
         }
      }
      UpdateMaskFrames(hwnd, pcustom);
      //UpdateAddDelButtons(hwnd);
      UpdateMaskBitsMulti(hwnd, pcustom);
      bool enable_new_mask = GetModelForNewMask();
      EnableWindow(GetDlgItem(hwnd, IDC_NEW_MODE), enable_new_mask && mask_modes.CanEdit());
      UpdaetMasksEdit(hwnd);
   }

//----------------------------

   bool LoadModes(){

      mask_modes.Clear();
      mask_modes.SetEditEnable(!OsIsFileReadOnly(ANIM_MASK_MODES_TXT_PATH));
      return mask_modes.LoadModesText(ANIM_MASK_MODES_TXT_PATH);
   }

//----------------------------

   bool SaveModes() const{

      if(!mask_modes.NeedSave())
         return true;

      if(OsIsFileReadOnly(ANIM_MASK_MODES_TXT_PATH)){
         C_str msg = C_fstr("Failed save to file %s.",ANIM_MASK_MODES_TXT_PATH);
         MessageBox((HWND)ed->GetIGraph()->GetHWND(), msg, "Anim edit", MB_OK | MB_ICONASTERISK);
         return false;
      }
      mask_modes.SaveModesText(ANIM_MASK_MODES_TXT_PATH);

#ifdef _DEBUG
                              //check if they are same
      S_lock_modes mmtest;
      mmtest.LoadModesText(ANIM_MASK_MODES_TXT_PATH);
      assert(mmtest.DebugMatch(mask_modes));
#endif
      return true;
   }

//----------------------------

   void UpdateMaskFrames(HWND hwnd, const S_mask_mode *pcustom){

      dword sel_id = SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);
      if(sel_id == CB_ERR)
         return;
      FillMaskListView(GetDlgItem(hwnd, IDC_FRM_LIST2), sel_id, pcustom);
   }

//----------------------------

   void SetFramesMask(byte new_mask, HWND hwnd, byte change_mask, S_mask_mode *pcustom){

      dword sel_id = SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);
      if(sel_id == CB_ERR)
         return;

      assert(sel_id || pcustom);
      S_mask_mode &cur_mode = sel_id ? mask_modes.GetMode(sel_id) : *pcustom;

      HWND hwnd_lv = GetDlgItem(hwnd, IDC_FRM_LIST2);
      LVITEM li;
      dword num_items = SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0);
      for (int i = num_items; i--; ){

         memset(&li, 0, sizeof(li));
         li.iItem = i;
         li.mask = LVIF_PARAM | LVIF_STATE;
         li.stateMask = LVIS_SELECTED;
         SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM) &li);
         if(!(li.state&LVIS_SELECTED))
            continue;
         const char *link = (const char *)li.lParam;
         cur_mode.SetEnabled(link, new_mask, change_mask);
         SetItemMaskText(hwnd_lv, li.iItem, link, cur_mode);
      }

   }

//----------------------------

   void RemoveFramesFromMask(HWND hwnd, S_mask_mode *pcustom){

      dword sel_id = SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);
//      if((sel_id == CB_ERR) || (sel_id == 0))
      if(sel_id == CB_ERR)
         return;

      assert(sel_id || pcustom);
      S_mask_mode &cur_mode = (sel_id) ? mask_modes.GetMode(sel_id) : *pcustom;

      HWND hwnd_lv = GetDlgItem(hwnd, IDC_FRM_LIST2);
      LVITEM li;
      dword num_items = SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0);
      for (int i = num_items; i--; ){

         memset(&li, 0, sizeof(li));
         li.iItem = i;
         li.mask = LVIF_PARAM | LVIF_STATE;
         li.stateMask = LVIS_SELECTED;
         SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM) &li);
         if(!(li.state&LVIS_SELECTED))
            continue;
         const char *link = (const char *)li.lParam;
         cur_mode.DelLink(link);
      }
      UpdateMaskFrames(hwnd, pcustom);
   }

//----------------------------

   void AddFramesToMask(HWND hwnd, S_mask_mode *pcustom){

      dword sel_id = SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);
      //if((sel_id == CB_ERR) || (sel_id == 0))
      if(sel_id == CB_ERR)
         return;
      assert(sel_id || pcustom);
      S_mask_mode &cur_mode = (sel_id) ? mask_modes.GetMode(sel_id) : *pcustom;

      C_vector<char> vc;  //push all names into single C_vector
      if(!anim_mod)
         return;
      int curr_sel = -1;
      for(dword i = 0; i < anim_mod->NumFrames(); i++){
         PI3D_frame frm = anim_mod->GetFrames()[i];
         const char *str = frm->GetOrigName();
                              //allow select only those which are not in mask yet
         if(cur_mode.IsInMask(str))
            continue;
         vc.insert(vc.end(), str, str + strlen(str) + 1);
      }
      vc.push_back(0);  //end list
      int indx = ChooseItemFromList(ed->GetIGraph(), hwnd, "Choose notify frame", &vc.front(), curr_sel);
      if(indx != -1){

                              //find selected name
         const char *cp = &vc.front();
         while(*cp && (indx--)){
            cp += strlen(cp) + 1;
         }
         assert(indx == -1);
         assert(anim_mod->FindChildFrame(cp));
         cur_mode.AddLink(cp);
      }
      UpdateMaskFrames(hwnd, pcustom);
   }

//----------------------------

   void UpdateMaskBitsMulti(HWND hwnd, const S_mask_mode *pcustom) const{

      dword sel_id = SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hwnd, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);
      if(sel_id == CB_ERR)
         return;

      assert(sel_id || pcustom);
      const S_mask_mode &cur_mode = sel_id ? mask_modes.GetMode(sel_id) : *pcustom;

      const int UN_INIT = 9;
      int state[3] = {UN_INIT, UN_INIT, UN_INIT};

      LVITEM li;
      HWND hwnd_lv = GetDlgItem(hwnd, IDC_FRM_LIST2);
      dword num_items = SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0);
      for (dword i = 0; i < num_items; i++ ){

         memset(&li, 0, sizeof(li));
         li.iItem = i;
         li.mask = LVIF_PARAM | LVIF_STATE;
         li.stateMask = LVIS_SELECTED;
         SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM) &li);
         if(!(li.state&LVIS_SELECTED))
            continue;
         const char *link = (const char *)li.lParam;

         byte mask = cur_mode.GetEnabled(link);
         for(byte j = 3; j--; ){
            bool checked(false);
            switch(j){
            case 0: checked = mask&TR_POS; break;
            case 1: checked = mask&TR_ROT; break;
            case 2: checked = mask&TR_POW; break;               
            }
            if(state[j] == UN_INIT){
               state[j] = checked;
               continue;
            }
            switch(state[j]){
            case 0:           //unchecked, keep same value or set undetermined
               if(checked)
                  state[j] = 2;
               break;
            case 1:           //checked, keep same value or set undetermined
               if(!checked)
                  state[j] = 2;
               break;
            case 2:
               break;
            }
         }
      }
                              //now set checkboxes state
      SendDlgItemMessage(hwnd, IDC_POS_CHECK, BM_SETCHECK, state[0], 0);
      SendDlgItemMessage(hwnd, IDC_ROT_CHECK, BM_SETCHECK, state[1], 0);
      SendDlgItemMessage(hwnd, IDC_POW_CHECK, BM_SETCHECK, state[2], 0);
   }

//----------------------------

   bool IsMaskNameUsed(const char *name) const{

      for(int i=mask_modes.modes.size(); i--; ){
         if(mask_modes.modes[i].GetName().Matchi(name))
            break;
      }
      return (i != -1);
   }

//----------------------------

   bool NewMaskMode(HWND hwnd){

      PI3D_model mod = GetModelForNewMask();
      if(!mod){
         //ed->Message("No model in animation, can't create mask.");
         return false;
      }

      C_str name;
      bool cancel(false);
      while(true){
         if(!SelectName(ed->GetIGraph(), hwnd, "Enter mask name", name)){
            cancel = true;
            break;
         }
         if(!name.Size())
            continue;

         if(!IsMaskNameUsed(name))
            break;
      }
      if(cancel)
         return false;

      C_vector<PI3D_frame> frm_list;
      assert(mod);
      for(int i = mod->NumFrames(); i--;)
         frm_list.push_back(mod->GetFrames()[i]);

      mask_modes.AddMode(name, frm_list);

      return true;
   }

//----------------------------

   bool DeleteMaskMode(HWND hDlg, dword sel_id){

                              //no selection
      if(sel_id == CB_ERR)
         return false;
                              //Empty (default) mode selected, do not delete
      if(sel_id == 0)
         return false;
                              //no mode to delete
      if(mask_modes.modes.size() <= 1)
         return false;

      C_fstr msg("Are you sure to delete mode '%s' ?", (const char*)mask_modes.modes[sel_id].GetName());
      if(MessageBox(hDlg, msg, "Delete mask", MB_OKCANCEL) != IDOK)
         return false;

      mask_modes.DeleteMode(sel_id);
      return true;
   }

//----------------------------

   struct S_anim_mask_hlp{

      C_edit_AnimEdit *ae; 
      int select_id;
      S_mask_mode *cust_mask;
   };

//----------------------------

   static BOOL CALLBACK cbMaskAnim(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){

	   switch(message){
      case WM_HELP:
         {
            LPHELPINFO hi = (LPHELPINFO)lParam;
            DisplayHelp(hDlg, (word)hi->iCtrlId, masks_help_texts);
            return 1;
         }
         break;
      case WM_INITDIALOG:
         {
            SetWindowLong(hDlg, GWL_USERDATA, lParam);
            S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)lParam;
            assert(hlp.ae);
            InitDlg(hlp.ae->ed->GetIGraph(), hDlg);
            {
               HWND hwnd_lc = GetDlgItem(hDlg, IDC_FRM_LIST2);

               SendMessage(hwnd_lc, LVM_SETEXTENDEDLISTVIEWSTYLE, 
                   LVS_EX_FULLROWSELECT, 
                   LVS_EX_FULLROWSELECT);

               RECT rc;
               GetClientRect(hwnd_lc, &rc);

               const int PRS_W = 20;
               LVCOLUMN lvc;
               memset(&lvc, 0, sizeof(lvc));
               lvc.mask = LVCF_WIDTH;
               lvc.cx = (rc.right - rc.left) - (PRS_W *3) - 16;
               SendMessage(hwnd_lc, LVM_INSERTCOLUMN, 0, (LPARAM)&lvc);
                              //P|R|S column
               lvc.mask = LVCF_WIDTH|LVCF_SUBITEM;
               lvc.cx = PRS_W;
               for (int k = 1; k < 4; k++){
                  lvc.iSubItem = k;
                  SendMessage(hwnd_lc, LVM_INSERTCOLUMN, k, (LPARAM)&lvc);
               }
            }
            hlp.ae->InitMaskDialog(hDlg, hlp.select_id, hlp.cust_mask);
            return true;
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL:
            {
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               hlp.ae->LoadModes();
               EndDialog(hDlg, 0);
            }
            break;
         case IDOK:
            {
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               hlp.select_id = SendDlgItemMessage(hDlg, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hDlg, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);
                              //save before exit
               if(!hlp.ae->SaveModes())
                  break;
               EndDialog(hDlg, 1);
            }
            break;

         case IDC_MODE_SELECT:
            switch(HIWORD(wParam)){
            case CBN_SELCHANGE:
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               //hlp.ae->UpdateAddDelButtons(hDlg);
               hlp.ae->UpdateMaskFrames(hDlg, hlp.cust_mask);
               hlp.ae->UpdaetMasksEdit(hDlg);
               break;
            }
            break;
         case IDC_NEW_MODE:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){

               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               hlp.ae->NewMaskMode(hDlg);
               hlp.ae->InitMaskDialog(hDlg, hlp.ae->NumMasks()-1, hlp.cust_mask);
            }
            break;
         case IDC_DELETE_MODE:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               dword sel_id = SendDlgItemMessage(hDlg, IDC_MODE_SELECT, CB_GETITEMDATA, SendDlgItemMessage(hDlg, IDC_MODE_SELECT, CB_GETCURSEL, 0, 0), 0);
               if(hlp.ae->DeleteMaskMode(hDlg, sel_id))
                  hlp.ae->InitMaskDialog(hDlg, Min(sel_id, hlp.ae->NumMasks()-1), hlp.cust_mask);
            }
            break;
         case IDC_TRANS_ALL:
         case IDC_TRANS_CLEAR:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               byte new_mask = byte((LOWORD(wParam) == IDC_TRANS_ALL) ? (TR_POS | TR_ROT | TR_POW) : 0);
               hlp.ae->SetFramesMask(new_mask, hDlg, TR_POS|TR_ROT|TR_POW, hlp.cust_mask);
               hlp.ae->UpdateMaskBitsMulti(hDlg, hlp.cust_mask);
            }
            break;
         case IDC_ADD_FRAME:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               hlp.ae->AddFramesToMask(hDlg, hlp.cust_mask);
            }
            break;
         case IDC_REMOVE_FRAME:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               hlp.ae->RemoveFramesFromMask(hDlg, hlp.cust_mask);
            }
            break;
         case IDC_POS_CHECK:
         case IDC_ROT_CHECK:
         case IDC_POW_CHECK:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){

               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               dword pos_check = SendDlgItemMessage(hDlg, IDC_POS_CHECK, BM_GETCHECK, 0, 0);
               dword rot_check = SendDlgItemMessage(hDlg, IDC_ROT_CHECK, BM_GETCHECK, 0, 0);
               dword pow_check = SendDlgItemMessage(hDlg, IDC_POW_CHECK, BM_GETCHECK, 0, 0);

               byte change_mask = 0;
               switch(LOWORD(wParam)){
               case IDC_POS_CHECK: change_mask = TR_POS; break;
               case IDC_ROT_CHECK: change_mask = TR_ROT; break;
               case IDC_POW_CHECK: change_mask = TR_POW; break;
               }   

               byte new_mask = 0;
               if(pos_check == BST_CHECKED)
                  new_mask |= TR_POS;
               if(rot_check == BST_CHECKED)
                  new_mask |= TR_ROT;
               if(pow_check == BST_CHECKED)
                  new_mask |= TR_POW;
               hlp.ae->SetFramesMask(new_mask, hDlg, change_mask, hlp.cust_mask);
               hlp.ae->UpdateMaskBitsMulti(hDlg, hlp.cust_mask);
            }
            break;
         case IDC_RELOAD:
            {
               S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               hlp.ae->LoadModes();
               hlp.ae->InitMaskDialog(hDlg, hlp.select_id, hlp.cust_mask);
            }
            break;
         }
      case WM_NOTIFY:
         {
            switch(wParam){
            case IDC_FRM_LIST2:
               {
                  LPNMHDR nmhdr = (LPNMHDR)lParam;
                  switch(nmhdr->idFrom){
                  case IDC_FRM_LIST2:
                     switch(nmhdr->code){
                     case LVN_ITEMCHANGED:
                        {
                           //LPNMLISTVIEW pnmv = (LPNMLISTVIEW) lParam;
                           S_anim_mask_hlp &hlp = *(S_anim_mask_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
                           assert(hlp.ae);
                           hlp.ae->UpdateMaskBitsMulti(hDlg, hlp.cust_mask);
                        }
                        break;
                     }
                     break;
                  }
               }
            }
         }
         break;
      }
      return 0;
   }

//----------------------------

void ApplyChangedMaskToAnims(dword changed_mask_id){

   if(!changed_mask_id)
      return;
   for(int i = anims.size(); i--; ){
      S_pose_anim_edit &pa = anims[i];
      if(pa.GetMaskId() == changed_mask_id)
         SetAnimMask(pa, changed_mask_id, NULL);
   }
}

//----------------------------

   void SelectMaskMode(HWND hwnd_parent){

      if(!ed->CanModify())
         return;
                              //copy custm mask from current anim for editing
      S_mask_mode tmp_mask;
      assert(IsEditAnimValid());
      S_pose_anim_edit &pa = GetEditAnim();
      if(pa.GetMaskId()){
         tmp_mask.Duplicate(&GetAnimMask(pa));
      }
      else{
         assert(!pa.GetCustomMask().IsEmpty());
         tmp_mask.Duplicate(&pa.GetCustomMask());
      }

      int new_id = SelectMaskIdDlg(hwnd_parent, &tmp_mask);
      if(new_id == -1)
         return;
      ed->SetModified(true);

      SetAnimMask(GetEditAnim(), new_id, &tmp_mask);
                              //apply mask also to ohters anims for case that was changed
      ApplyChangedMaskToAnims(new_id);

#ifdef _DEBUG
      CheckAnimMask();
#endif
      UnlockAnimFrames();
      LockAnimFrames();
   }

//----------------------------

   bool IsFrameInAnim(const char *frm_name, const S_pose_anim &pa) const{

      return (FindAnimId(pa.key_list[0].anim_set, frm_name) != -1);
   }

//----------------------------

   void UpdateCheckList() const{

      assert(hWnd_anim_frames);
      HWND hwnd_lc = GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST);
      assert(hwnd_lc);

                              //get items count
      dword items_cnt = SendMessage(hwnd_lc, LVM_GETITEMCOUNT, 0, 0);

      LVITEM li;
      memset(&li, 0, sizeof(li));
      li.stateMask = LVIS_STATEIMAGEMASK;

      for(int i = items_cnt; i--; ){
         li.iItem = i;
         li.mask = LVIF_PARAM;
         bool b = SendMessage(hwnd_lc, LVM_GETITEM, 0, (LPARAM) &li);
         assert(b);
         if(!b)
            continue;
         PI3D_frame frm = (PI3D_frame)li.lParam;

         const S_pose_anim_edit &pa = GetEditAnim();
         bool used = IsFrameInAnim(frm->GetOrigName(), pa);
                              //use list: -1 = uninit, 0 = not in, 1 = in, 2 = indeterminate
         li.mask = LVIF_STATE;
         li.state = INDEXTOSTATEIMAGEMASK(1 + (used ? 1 : 0));
         HWND hwnd_save = hWnd_anim_frames;
         ((C_edit_AnimEdit*)this)->hWnd_anim_frames = NULL;
         SendMessage(hwnd_lc, LVM_SETITEMSTATE, i, (LPARAM)&li);
         ((C_edit_AnimEdit*)this)->hWnd_anim_frames = hwnd_save;
      }
   }

//----------------------------
// action: 0 ..clear all, 1: check all, 2: check selected.
// if copy_from is specified, frames are checked like in copy_from animation.
   void SelectAnimFrames(byte action, const S_pose_anim *copy_from = NULL){

      if(!ed->CanModify())
         return;
      ed->SetModified(true);

      assert(hWnd_anim_frames);
      HWND hwnd_lc = GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST);
      assert(hwnd_lc);

      C_buffer<C_str> names_list;
      if(copy_from)
         copy_from->GetAnimFrames(names_list);
                              //get items count
      dword items_cnt = SendMessage(hwnd_lc, LVM_GETITEMCOUNT, 0, 0);

      LVITEM li;
      memset(&li, 0, sizeof(li));

      for(int i = items_cnt; i--; ){
         li.iItem = i;
         li.mask = LVIF_PARAM;
         bool b = SendMessage(hwnd_lc, LVM_GETITEM, 0, (LPARAM) &li);
         assert(b);
         if(!b)
            continue;
         PI3D_frame frm = (PI3D_frame)li.lParam;
         const char *link = frm->GetOrigName();

                              //perform selection...
         if(copy_from){
                              // ... by provided name list
            int i;
            for(i=names_list.size(); i--; ){
               if(!stricmp(names_list[i], link))
                  break;
            }
            if(i!=-1){
               AddAnimFrame(link);
            }else{
               DelAnimFrame(link);
            }
                              //copy weight
            float w = copy_from->GetWeight(link);
            SetWeight(edit_anim_id, link, w);
         }else
         switch(action){
         case 0:   // ...no frames                              
            DelAnimFrame(link);
            break;
         case 1:   // ...all frames                              
            AddAnimFrame(link);
            break;
         case 2:  //selection only
            {
                              //get selection status from item
               LVITEM li;
               memset(&li, 0, sizeof(li));
               li.iItem = i;
               li.mask = LVIF_STATE;
               li.stateMask = LVIS_SELECTED;
               SendMessage(hwnd_lc, LVM_GETITEM, 0, (LPARAM) &li);
               if(li.state&LVIS_SELECTED)
                  AddAnimFrame(link);
               else
                  DelAnimFrame(link);   
            }
            break;
         }
      }
      UpdateAnimation();
   }

//----------------------------

   void UpdateMaskName() const{

      assert(hWnd_anim_frames);
      const char *mask_name = GetEditMask().GetName();
      SetDlgItemText(hWnd_anim_frames, IDC_MASK_NAME, mask_name);         
   }

//----------------------------

   void UpdateAnimGui(){

      if(!hWnd_anim_edit)
         return;
      UpdateModelNameInDialog();
      UpdateAnimName();
      UpdateKeyGui();
      UpdateFrameSelect();
      UpdatePlayTimeCaption();
   }

//----------------------------

   void UpdatePlayTimeCaption() const{

      assert(IsEditAnimValid());
      dword end_time = GetEditAnim().GetEndTime();
      C_str cpt = C_fstr("/ %i", end_time);
      SetDlgItemText(hWnd_anim_edit, IDC_PLAY_TIME, cpt);
   }

//----------------------------

   void UpdatePlayTime(bool by_playback) const{

      UpdatePlayTimeCaption();
      UpdateTimeSliderPos(by_playback);
   }

//----------------------------

   void UpdateTimeSlider() const{

      UpdateTimeSliderRange();
      UpdateTimeSliderMarks();
      UpdateTimeSliderPos(false);
   }

//----------------------------

   void UpdateTimeSliderPos(bool by_playback) const{

      int curr_time = (by_playback) ? anim_play.GetTime() : GetEditAnim().CurrPoseKey().time;

      SendMessage(GetDlgItem(hWnd_anim_edit, IDC_TIME_SLIDER), TBM_SETPOS, true, curr_time);
   }

//----------------------------

   void UpdateTimeSliderRange() const{
                              //setup speed track-bar
      const HWND &hDlg = hWnd_anim_edit;
      dword end_time = GetEditAnim().GetEndTime();
      SendMessage(GetDlgItem(hDlg, IDC_TIME_SLIDER), TBM_SETRANGEMIN, true, 0);
      SendMessage(GetDlgItem(hDlg, IDC_TIME_SLIDER), TBM_SETRANGEMAX, true, end_time);
      SendMessage(GetDlgItem(hDlg, IDC_TIME_SLIDER), TBM_SETLINESIZE, 0, end_time /100);
      SendMessage(GetDlgItem(hDlg, IDC_TIME_SLIDER), TBM_SETPAGESIZE, 0, end_time /20);
   }

//----------------------------

   void UpdateTimeSliderMarks() const{

      SendMessage(GetDlgItem(hWnd_anim_edit, IDC_TIME_SLIDER), TBM_CLEARTICS, true, 0);

      if(!IsActive())
         return;
      const C_buffer<S_pose_key> &pk_vector =  GetEditAnim().key_list;
      int num_keys = pk_vector.size();
      for(int i = 1; i < (num_keys - 1); i++ ){

         const S_pose_key &pk = pk_vector[i];
         SendMessage(GetDlgItem(hWnd_anim_edit, IDC_TIME_SLIDER), TBM_SETTIC, 0, pk.time);
      }
   }


//----------------------------

   void UpdateKeyGui(){

      UpdateKeyNum();
      UpdateKeyTime();
      UpdateSmooth();
      UpdateEasyFrom();
      UpdateEasyTo();
      UpdateNote();
      UpdateNotifyLink();
      UpdateModel();
      UpdateTimeSlider();
      UpdateKeyTimeEdit();
   }

//----------------------------

   void UpdateKeyTimeEdit() const{

      bool enable_time_edit = GetEditAnim().CurrKey();
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_KEY_TIME), enable_time_edit);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_INSERT_TIME), enable_time_edit);
   }

//----------------------------

   void UpdateFrameSelect(){

      FillFrameList2();

      UpdateCheckList();
      MarkSelectedFrame();
      UpdateMaskName();
   }

//----------------------------

   void MarkSelectedFrame(){

      if(!hWnd_anim_frames)
         return;
      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

                              //find this frame in listview
      HWND hwnd_lv = GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST);

      dword num_items = SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0);
      for(dword i = num_items; i--; ){
                              //get lParam info from item
         LVITEM li;
         memset(&li, 0, sizeof(li));
         li.iItem = i;
         li.mask = LVIF_PARAM;
         SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM) &li);
         PI3D_frame frm = (PI3D_frame)li.lParam;
         bool selected = (find(sel_list.begin(), sel_list.end(), frm)!=sel_list.end());
         {
            LVITEM itm;
            memset(&itm, 0, sizeof(itm));
            itm.iItem = i;
            itm.mask = LVIF_STATE;
            itm.state = selected ? (LVIS_SELECTED | LVIS_FOCUSED) : 0;
            itm.stateMask = LVIS_SELECTED | LVIS_FOCUSED;

            SendMessage(hwnd_lv, LVM_SETITEM, 0, (LPARAM) &itm);
         }
      }
   }

//----------------------------

   void UpdateAssingModelButton(){

      if(!hWnd_anim_edit)
         return;

      bool enable_button(false);

      PI3D_frame sel_frm = e_slct->GetSingleSel();
      if(sel_frm && sel_frm->GetType() == FRAME_MODEL)
         enable_button = true;
      
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_CHANGE_MODEL), enable_button);

   }

//----------------------------

   void UpdateModel(){

      assert(GetEditAnim().CurrKey() < GetEditAnim().key_list.size());
      ResetModelInternal();
      PI3D_animation_set as = GetEditAnim().key_list[GetEditAnim().CurrKey()].anim_set;
      const S_mask_mode &mask = GetEditMask();      
      GetEditAnim().ApplyPoseFromSet(anim_mod, as, mask, e_props, e_modify);
   }

//----------------------------

   void UpdateFrameTransform(PI3D_frame frm){

                              //reset to original
      ResetModelFrame(frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE);

      const S_pose_anim_edit &pa = GetEditAnim();

      CPI3D_animation_set anim_set = pa.key_list[GetEditAnim().CurrKey()].anim_set;
      //const S_lock_modes::S_mask_mode &mask = mask_modes.GetMode(pa.GetMaskId());
      const S_mask_mode &mask = GetAnimMask(pa);

      if(!anim_mod)
         return;
      assert(("UpdateFrameTransform: invalid anim_set", anim_set));

      const char *link = frm->GetOrigName();
      int anim_id = FindAnimId(anim_set, link);
      if(anim_id != -1){
         CPI3D_anim_pose pose_from = (PI3D_anim_pose)((PI3D_animation_set)anim_set)->GetAnimation(anim_id);

         dword enabled = mask.GetEnabled(link);
         S_pose_anim_edit::CopyPose2Frame(pose_from, frm, enabled, e_props, e_modify);
      }
   }

//----------------------------
// Create temporary model an load model into it. Used for obtain original transform of model frames.
   PI3D_model GetOrigModel(const char *filename) const{

      PI3D_scene scn = ed->GetScene();
      PI3D_model mod = I3DCAST_MODEL(scn->CreateFrame(FRAME_MODEL));
      //assert(strlen(filename) < 256);
      //char name_buf[256];
      //memset(name_buf,0, sizeof(name_buf));
      //strcpy(name_buf, filename);
      //ed->GetModelCache().GetRelativeDirectory(name_buf);
      I3D_RESULT ir = ed->GetModelCache().Open(mod, filename, scn, 0, NULL, 0);
      if(I3D_FAIL(ir)){
         mod->Release();
         return NULL;
      }
      return mod;
   }

//----------------------------
//Perform reset on non animated frames.
//Modification are not stored since it is used only for animating purpose.
//Reset also animated frames, because only some transforms could be animated.
//Slow.
   void ResetModelInternal(){
      
      if(!anim_mod)
         return;

      C_smart_ptr<I3D_frame> orig_mod = GetOrigModel(anim_mod->GetFileName());
      if(!orig_mod)
         return;
      orig_mod->Release();

      const PI3D_frame *frms = anim_mod->GetFrames();
      for(dword i = anim_mod->NumFrames(); i--; ){

         PI3D_frame frm = frms[i];
         const char *frm_name = frm->GetOrigName();
         //PI3D_frame frm_orig = orig_mod->FindChildFrame(frm_name);
         PI3D_frame frm_orig = FindModelChild(orig_mod, frm_name);
         if(!frm_orig)
            continue;
                              //reset original
         CopyFrameTransform(frm_orig, frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE);
      }
   }

//----------------------------

   void CopyFrameTransform(PI3D_frame frm_source, PI3D_frame frm_dest, dword flags){

      assert(frm_source && frm_dest);
      if(flags&E_MODIFY_FLG_POSITION){
         frm_dest->SetPos(frm_source->GetPos());
         frm_dest->GetPos();
      }
      if(flags&E_MODIFY_FLG_ROTATION){
         frm_dest->SetRot(frm_source->GetRot());
         frm_dest->GetRot();
      }
      if(flags&E_MODIFY_FLG_SCALE){
         frm_dest->SetScale(frm_source->GetScale());
         frm_dest->GetScale();
      }
                              //notify properties, so that sheets are updated
      //e_props->Action(E_PROP_NOTIFY_MODIFY, &pair<PI3D_frame, dword>(frm_dest, flags));
      e_modify->AddFrameFlags(frm_dest, flags);
   }

//----------------------------

   void ResetModelFrame(PI3D_frame frm, dword flags){

      assert(frm);

      if(!anim_mod)
         return;
      C_smart_ptr<I3D_frame> orig_mod = GetOrigModel(anim_mod->GetFileName());
      if(!orig_mod)
         return;
      orig_mod->Release();
      //PI3D_frame frm_orig = orig_mod->FindChildFrame(frm->GetOrigName());
      PI3D_frame frm_orig = FindModelChild(orig_mod, frm->GetOrigName());
      if(!frm_orig)
         return;
      CopyFrameTransform(frm_orig, frm, flags);
   }

//----------------------------

   PI3D_anim_pose CreateFrameOrigPose(const char *frm_name){

      if(!anim_mod)
         return NULL;
      C_smart_ptr<I3D_frame> orig_mod = GetOrigModel(anim_mod->GetFileName());
      if(!orig_mod)
         return NULL;
      orig_mod->Release();
      //PI3D_frame frm_orig = orig_mod->FindChildFrame(frm_name);
      PI3D_frame frm_orig = FindModelChild(orig_mod, frm_name);
      if(!frm_orig)
         return NULL;
      PI3D_anim_pose pose = (PI3D_anim_pose) ed->GetDriver()->CreateAnimation(I3DANIM_POSE);
      pose->Clear();
      pose->SetPos(&frm_orig->GetPos());
      pose->SetRot(&frm_orig->GetRot());
      float f = frm_orig->GetScale();
      pose->SetPower(&f);
      return pose;
   }

//----------------------------
// Update name of model in dialog from currently edited animation.]
   void UpdateModelNameInDialog() const{

      //C_str cpt = C_fstr("%s", anim_mod ? anim_mod->GetName() : "");
      const S_pose_anim_edit &pa = GetEditAnim();
      SetDlgItemText(hWnd_anim_edit, IDC_MODEL_NAME, pa.model_name);
   }

//----------------------------

   void UpdateAnimName() const{

                              //check if not same
      C_str curr_name = GetDlgItemText(hWnd_anim_edit, IDC_ANIM_NAME);

      if(curr_name != GetEditAnim().name){
         const C_str &cpt = GetEditAnim().name;
         SetDlgItemText(hWnd_anim_edit, IDC_ANIM_NAME, cpt);
      }
   }

//----------------------------

   void UpdateKeyNum() const{

      //C_str cpt = C_fstr("Key num: %i", GetEditAnim().curr_key);
      C_str cpt = C_fstr("Key: %i   [total %i]", GetEditAnim().CurrKey(), GetEditAnim().key_list.size());
      SetDlgItemText(hWnd_anim_edit, IDC_KEY_NUM, cpt);
   }

//----------------------------

   void UpdateKeyTime() const{

      if(!GetEditAnim().key_list.size())
         return;
      C_str cpt = C_fstr("%i", GetEditAnim().CurrPoseKey().time);
      SetDlgItemText(hWnd_anim_edit, IDC_KEY_TIME, cpt);
   }

//----------------------------

   void UpdateSmooth() const{

      if(!GetEditAnim().key_list.size())
         return;
      C_str cpt = C_fstr("%f", GetEditAnim().CurrPoseKey().smooth);
      SetDlgItemText(hWnd_anim_edit, IDC_SMOOTH, FloatStrip(cpt));
   }

//----------------------------

   void UpdateEasyFrom() const{

      if(!GetEditAnim().key_list.size())
         return;
      C_str cpt = C_fstr("%f", GetEditAnim().CurrPoseKey().easy_from);
      SetDlgItemText(hWnd_anim_edit, IDC_EASY_FROM, FloatStrip(cpt));
   }

//----------------------------

   void UpdateEasyTo() const{

      if(!GetEditAnim().key_list.size())
         return;
      C_str cpt = C_fstr("%f", GetEditAnim().CurrPoseKey().easy_to);
      SetDlgItemText(hWnd_anim_edit, IDC_EASY_TO, FloatStrip(cpt));
   }

//----------------------------

   void UpdateNote() const{

      if(!GetEditAnim().key_list.size())
         return;
      C_str cpt = GetEditAnim().CurrPoseKey().notify;
      SetDlgItemText(hWnd_anim_edit, IDC_NOTIFY, cpt);
   }

//----------------------------

   void UpdateNotifyLink() const{

      if(!GetEditAnim().key_list.size())
         return;
      C_str cpt = C_fstr("%s", (const char*)GetEditAnim().CurrPoseKey().notify_link);
      SetDlgItemText(hWnd_anim_edit, IDC_NOTIFY_LINK, cpt);
   }
//----------------------------
//----------------------------

   bool IsTopUndoActionSame(dword action_id){

      return e_undo->IsTopEntry(this, action_id);
      /*
      PC_editor_item top_plg = (PC_editor_item)e_undo->Action(E_UNDO_GET_TOP_PLUGIN1);
      if(top_plg!=this)
         return false;
      return (e_undo->Action(E_UNDO_GET_TOP_ACTION) == action_id);
      */
   }

//----------------------------

   void RenameEditAnim(const char *new_name){

      assert(new_name);
      const char *old_name = GetEditAnim().name;
      if(strcmp(old_name, new_name) == 0)
         return;

      if(!ed->CanModify())
         return;
      ed->SetModified(true);
                              //save undo
      dword undo_id = E_ANIM_RENAME_ANIM;
      if(!IsTopUndoActionSame(undo_id)){
         e_undo->Save(this, undo_id, (void*)old_name, strlen(old_name) + 1);
      }
                              //finally rename
      GetEditAnim().name = new_name;
   }

//----------------------------

   void SetKeyNote(const char *new_note, bool force_save_undo = false){

      assert(new_note);
      const char *old_note = GetEditAnim().CurrPoseKey().notify;
      if(strcmp(old_note, new_note) == 0)
         return;

      if(!ed->CanModify())
         return;
      ed->SetModified(true);

      dword undo_id = E_ANIM_SET_KEY_NOTE;
      if(!IsTopUndoActionSame(undo_id) || force_save_undo){
         e_undo->Save(this, undo_id, (void*)old_note, strlen(old_note) + 1);
      }
      GetEditAnim().CurrPoseKey().notify = new_note;
      UpdateAnimation();
      ed->Message("Changed key's note.");
   }

//----------------------------

   void SetKeyNotifyLink(const char *new_link){

      assert(new_link);
      const char *old_link = GetEditAnim().CurrPoseKey().notify_link;
      if(strcmp(old_link, new_link) == 0)
         return;

      if(!ed->CanModify())
         return;
      ed->SetModified(true);

                              //save undo
      e_undo->Save(this, E_ANIM_SET_KEY_NOTIFY_LINK, (void*)old_link, strlen(old_link) + 1);

      GetEditAnim().CurrPoseKey().notify_link = new_link;
      UpdateAnimation();
      ed->Message("Changed key's note link.");
   }

//----------------------------

   void SetKeyTime(dword t1){

      S_pose_anim_edit &pa = GetEditAnim();
                                 //make bounday so key can't be same as previous or next
      dword curr_key = pa.CurrKey();
      dword prev_time = curr_key ? pa.key_list[curr_key - 1].time + 1 : 0;
      dword next_time = (curr_key + 1) < pa.key_list.size() ? pa.key_list[curr_key + 1].time - 1: 0xffffffff;
      dword new_time = Min(next_time, Max(prev_time, t1));

      dword old_time = pa.CurrPoseKey().time;
      if(old_time != new_time){

         if(!ed->CanModify())
            return;
         ed->SetModified(true);
                              //save undo only when start editind this item
         if(!IsTopUndoActionSame(E_ANIM_SET_KEY_TIME)){
            e_undo->Save(this, E_ANIM_SET_KEY_TIME, &old_time, sizeof(dword));
         }
         pa.CurrPoseKey().time = new_time;
         UpdateAnimation();
         ed->Message("Changed key's time.");
      }
      UpdateTimeSlider();
   }

//----------------------------

   dword GetKeyTime() const{
      const S_pose_anim_edit &pa = GetEditAnim();
      return pa.CurrPoseKey().time;
   }

//----------------------------

   void SetKeySmooth(float f){

      float new_smooth  = Min(1.0f, Max(-1.0f, f));
      float old_smooth = GetEditAnim().CurrPoseKey().smooth;
      dword undo_id = E_ANIM_SET_KEY_SMOOTH;
      if(new_smooth != old_smooth){

         if(!ed->CanModify())
            return;
         ed->SetModified(true);

         if(!IsTopUndoActionSame(undo_id)){
            e_undo->Save(this, undo_id, &old_smooth, sizeof(float));
         }
         GetEditAnim().CurrPoseKey().smooth = new_smooth;
         UpdateAnimation();
         ed->Message("Changed key's smooth.");
      }
   }

//----------------------------

   void SetKeyEasyFrom(float f){

      float new_easy  = Min(1.0f, Max(.0f, f));
      float old_easy = GetEditAnim().CurrPoseKey().easy_from;
      dword undo_id = E_ANIM_SET_KEY_EASY_FROM;
      if(new_easy != old_easy){

         if(!ed->CanModify())
            return;
         ed->SetModified(true);

         if(!IsTopUndoActionSame(undo_id)){
            e_undo->Save(this, undo_id, &old_easy, sizeof(float));
         }
         GetEditAnim().CurrPoseKey().easy_from = new_easy;
         UpdateAnimation();
         ed->Message("Changed key's easy from.");
      }
   }

//----------------------------

   void SetKeyEasyTo(float f){

      float new_easy  = Min(1.0f, Max(.0f, f));
      float old_easy = GetEditAnim().CurrPoseKey().easy_to;
      dword undo_id = E_ANIM_SET_KEY_EASY_TO;
      if(new_easy != old_easy){

         if(!ed->CanModify())
            return;
         ed->SetModified(true);

         if(!IsTopUndoActionSame(undo_id))
            e_undo->Save(this, undo_id, &old_easy, sizeof(float));

         GetEditAnim().CurrPoseKey().easy_to = new_easy;
         UpdateAnimation();
         ed->Message("Changed key's easy to.");
      }
   }

//----------------------------

   void SaveDelFrameKeysUndo(const char *frm_name){

      e_undo->Save(this, E_ANIM_DEL_FRAME_KEYS, (void *)frm_name, strlen(frm_name) + 1);
   }

//----------------------------

   void CreateFrameKeysOrig(S_pose_anim_edit &pa, const char *link){

      //const S_lock_modes::S_mask_mode &mm = mask_modes.GetMode(pa.GetMaskId());
      const S_mask_mode &mm = GetAnimMask(pa);
      dword lock_flgs = ~mm.GetEnabled(link);
      for(dword i = pa.key_list.size(); i--; ){
         S_pose_key &pk = pa.key_list[i];

         int anim_id = FindAnimId(pk.anim_set, link);
         if(anim_id != -1)
            continue;

         PI3D_anim_pose pose = CreateFrameOrigPose(link);
         if(!pose)
            return;
                              //must be masked
         MaskPose(pose, lock_flgs);
         pk.anim_set->AddAnimation(pose, link);
         pose->Release();
      }
   }

//----------------------------

   void MaskPose(PI3D_anim_pose pose, dword lock_flgs) const{

      assert(pose);
      if(lock_flgs&TR_POS)
         pose->SetPos(NULL);
      if(lock_flgs&TR_ROT)
         pose->SetRot(NULL);
      if(lock_flgs&TR_POW)
         pose->SetPower(NULL);
   }

//----------------------------

   void UpdateAnimFrame(const S_pose_anim_edit &pa, const char *link){

      if(!anim_mod)
         return;
      //PI3D_frame frm = anim_mod->FindChildFrame(link);
      PI3D_frame frm = FindModelChild(anim_mod, link);
      if(frm){
         UnlockFrame(frm);
         LockFrame(pa, frm);
         UpdateFrameTransform(frm);
         SetFrameTransp(frm, model_transparency);
      }
   }

//----------------------------

   bool AddAnimFrame(const char *link, void *undo_info = NULL){

      assert(link);
      S_pose_anim_edit &pa = GetEditAnim();
      if(pa.IsInFramesList(link))
         return false;

      if(!ed->CanModify())
         return false;
      ed->SetModified(true);
                              //save undo
      SaveDelFrameKeysUndo(link);

      if(undo_info){
         const S_frame_keys &fk = *(const S_frame_keys *) undo_info;
         assert(strcmp(fk.frm_name, link)==0);
         CreateFrameKeys(pa.key_list, fk);
      }else{
         CreateFrameKeysOrig(pa, link);
      }
          
      UpdateAnimFrame(pa, link);
      return true;
   }

//----------------------------
//go through all keys, remove anims with this link
   void RemoveAnimKeysLink(S_pose_anim_edit &pa, const C_str &link) const{

      for(dword i = pa.key_list.size(); i--; ){
         S_pose_key &pk = pa.key_list[i];
         int id = FindAnimId(pk.anim_set, link);
         if(id == -1)
            continue;
         pk.anim_set->RemoveAnimation(pk.anim_set->GetAnimation(id));
      }
   }

//----------------------------

   bool DelAnimFrame(const C_str &link){

      S_pose_anim_edit &pa = GetEditAnim();

      if(!pa.IsInFramesList(link))
         return false;

      if(!ed->CanModify())
         return false;
      ed->SetModified(true);
                              //save undo
      C_vector<byte> undo_buf;
      CreateFrameKeysUndo(undo_buf, pa.key_list, link);
      e_undo->Save(this, E_ANIM_ADD_FRAME_KEYS, &undo_buf.front(), undo_buf.size());

                              //go through all keys, remove anims with this link
      RemoveAnimKeysLink(pa, link);

      UpdateAnimFrame(pa, link);
      return true;
   }

//----------------------------

// Map Control ids to pluggin actions.
   static E_ACTION GetIDCAction(WORD idc){
      switch(idc){
      case IDC_ADD_KEY:
         return E_ANIM_ADD_KEY;
      case IDC_DEL_KEY:
         return E_ANIM_DEL_KEY;
      case IDC_PREV_KEY:
         return E_ANIM_PREV_KEY;
      case IDC_NEXT_KEY:
         return E_ANIM_NEXT_KEY;
      case IDC_FIRST_KEY:
         return E_ANIM_FIRST_KEY;
      case IDC_LAST_KEY:
         return E_ANIM_LAST_KEY;
      case IDC_PLAY_TOGGLE:
         return E_ANIM_PLAY_TOGGLE;
      case IDC_COPY_POSE:
         return E_ANIM_COPY_POSE;
      case IDC_PASTE_POSE:
         return E_ANIM_PASTE_POSE;
      case IDC_PICK_NOTIFY_FRM:
         return E_ANIM_PICK_NOTIFY_FRM;
      case IDC_CHANGE_MODEL:
         return E_ANIM_CHANGE_MODEL;
      case IDC_NOTE_CLEAR:
         return E_ANIM_NOTE_CLEAR;
      case IDC_ANIM_TIME_SCALE:
         return E_ANIM_SELECT_SCALE_TIME;
      case IDC_PASTE_MIRROR:
         return E_ANIM_PASTE_MIRROR;
      case IDC_INSERT_TIME:
         return E_ANIM_INSERT_TIME;
      case IDC_PASTE_POSE_SELECTION:
         return E_ANIM_PASTE_SELECTION;
      case IDC_PASTE_POSE_SELECTION_KEYS:
         return E_ANIM_PASTE_SELECTION_ALL_KEYS;
      case IDC_SMOOTH_ANIM_SPEED:
         return E_ANIM_SMOOTH_ANIM_TIME;
      }
      return E_ANIM_NULL;
   }

//----------------------------

   static E_ACTION GetIDCAction2(WORD idc){

      switch(idc){
      case IDC_CLEAR: return E_ANIM_CLEAR_SELECTION;
      case IDC_ALL: return E_ANIM_ALL_SELECTION;
      case IDC_SEL: return E_ANIM_SEL_SELECTION;
      case IDC_COPY: return E_ANIM_COPY_SELECTION;
      case IDC_PICK_MASK: return E_ANIM_PICK_MASK;
      case IDC_WEIGHT: return E_ANIM_SET_WEIGHT;
      }
      return E_ANIM_NULL;
   }

//----------------------------
// Get animation selected in anim select dialog.
   static int GetDlgSelectedAnim(HWND hwnd, bool allow_toggle = false){

      HWND hwnd_tv = GetDlgItem(hwnd, IDC_ANIMS_TREE);
      assert(hwnd_tv);
      TVITEM it;
      it.hItem = (HTREEITEM)SendMessage(hwnd_tv, TVM_GETNEXTITEM, TVGN_CARET, 0);
      if(it.hItem){
         it.mask = TCIF_PARAM;
         if(SendMessage(hwnd_tv, TVM_GETITEM, 0, (LPARAM)&it)){
            if(it.lParam==-1 && allow_toggle)
               SendMessage(hwnd_tv, TVM_EXPAND, TVE_TOGGLE, (LPARAM)it.hItem);
            return it.lParam;
         }
      }
      return -1;
   }

//----------------------------

   bool IsFrameInEditAnim(const char*frm_name) const{

      const S_pose_anim_edit &pa = GetEditAnim();
      return pa.IsInFramesList(frm_name);
   }

//----------------------------

   static BOOL CALLBACK cbSelectAnim(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){

	   switch(message){
      case WM_INITDIALOG:
         {
            SetWindowLong(hDlg, GWL_USERDATA, lParam);
            S_select_anim_hlp &hlp = *(S_select_anim_hlp*)lParam;
            assert(hlp.ae);
            InitDlg(hlp.ae->ed->GetIGraph(),hDlg);
            hlp.ae->FillSelectionListbox(hDlg);
            EnableWindow(GetDlgItem(hDlg, IDC_NEW), hlp.can_create_new);

            if(hlp.browse_only){
                              //only browsing, disable advanced buttons
               ShowWindow(GetDlgItem(hDlg, IDC_NEW), SW_HIDE);
               ShowWindow(GetDlgItem(hDlg, IDC_DUPLICATE), SW_HIDE);
               ShowWindow(GetDlgItem(hDlg, IDC_DELETE), SW_HIDE);
               SetWindowText(hDlg, "Browse animation");
            }
            return true;
         }
         break;

      case WM_NOTIFY:
         switch(wParam){
         case IDC_ANIMS_TREE:
            {
               LPNMHDR hdr = (LPNMHDR)lParam;
               switch(hdr->code){
               case NM_DBLCLK:
                  {
                     int indx = GetDlgSelectedAnim(hDlg, true);
                     if(indx!=-1){
                        S_select_anim_hlp &hlp = *(S_select_anim_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
                        hlp.curr_id = indx;
                        EndDialog(hDlg, 1);
                     }
                  }
                  break;
               }
            }
            break;
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDCANCEL:
            EndDialog(hDlg, 0);
            break;
         case IDOK:
            {
               int indx = GetDlgSelectedAnim(hDlg, true);
               if(indx!=-1){
                  S_select_anim_hlp &hlp = *(S_select_anim_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
                  hlp.curr_id = indx;
                  EndDialog(hDlg, 1);
               }
            }
            break;

         case IDC_NEW:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){
               S_select_anim_hlp &hlp = *(S_select_anim_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               assert(hlp.ae);
               bool created = hlp.ae->ActionAnimCreate(hDlg);
               if(created)
                  EndDialog(hDlg, 0);
            }
            break;
         case IDC_DUPLICATE:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){
               int indx = GetDlgSelectedAnim(hDlg);
               if(indx==-1)
                  break;
               S_select_anim_hlp &hlp = *(S_select_anim_hlp*)GetWindowLong(hDlg, GWL_USERDATA);
               bool created = hlp.ae->ActionDuplicateAnim(hDlg, indx);
               if(created)
                  EndDialog(hDlg, 0);
            }
            break;
         case IDC_DELETE:
            if(HIWORD(wParam) == (WORD)BN_CLICKED){
               int indx = GetDlgSelectedAnim(hDlg);
               if(indx==-1)
                  break;
               S_select_anim_hlp &hlp = *(S_select_anim_hlp*)GetWindowLong(hDlg, GWL_USERDATA);

               int new_sel = -1;
               HWND hwnd_tv = GetDlgItem(hDlg, IDC_ANIMS_TREE);
               HTREEITEM hti_curr = (HTREEITEM)SendMessage(hwnd_tv, TVM_GETNEXTITEM, TVGN_CARET, 0);
               assert(hti_curr);
               HTREEITEM hti = (HTREEITEM)SendMessage(hwnd_tv, TVM_GETNEXTITEM, TVGN_NEXTVISIBLE , (LPARAM)hti_curr);
               if(!hti) hti = (HTREEITEM)SendMessage(hwnd_tv, TVM_GETNEXTITEM, TVGN_PREVIOUSVISIBLE, (LPARAM)hti_curr);
               if(hti){
                  TVITEM it;
                  it.hItem = hti;
                  it.mask = TCIF_PARAM;
                  if(SendMessage(hwnd_tv, TVM_GETITEM, 0, (LPARAM)&it)){
                     new_sel = it.lParam;
                     if(new_sel > indx)
                        --new_sel;
                  }
               }
                              //delete it
               if(hlp.ae->DeleteAnim(hDlg, indx)){
                              //refill box so we get correct indexes
                  assert(new_sel < (int)hlp.ae->anims.size());
                  int save_act = hlp.ae->edit_anim_id;
                  hlp.ae->edit_anim_id = new_sel;
                  hlp.ae->FillSelectionListbox(hDlg);
                  hlp.ae->edit_anim_id = save_act;
               }
            }
            break;
         }
      }
      return 0;
   }

//----------------------------

   static BOOL CALLBACK AnimEditCbProc_Thunk(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){

	   if(message==WM_INITDIALOG)
         SetWindowLong(hDlg, GWL_USERDATA, lParam);
      C_edit_AnimEdit *ea = (C_edit_AnimEdit*)GetWindowLong(hDlg, GWL_USERDATA);
      if(ea)
         return ea->AnimEditCbProc(hDlg, message, wParam, lParam);
      return 0;
   }

//----------------------------

   BOOL AnimEditCbProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){

      if(mode != MODE_OFF)
	   switch(message){

      case WM_INITDIALOG:
         {
            if(!anim_edit_wnd_pos.valid){
                              //init default window position
               RECT rc;
               GetWindowRect(hDlg, &rc);

               RECT rc_app;
               GetWindowRect((HWND)ed->GetIGraph()->GetHWND(), &rc_app);
                                    //by default, put it to bottom, alignet with main window
               //int x = GetSystemMetrics(SM_CXSCREEN);
               int x = rc_app.left;
               int y = GetSystemMetrics(SM_CYSCREEN);
               //x -= (rc.right - rc.left);
               y -= (rc.bottom - rc.top);
               y = Min(y, (int)rc_app.bottom);

               anim_edit_wnd_pos.x = x;
               anim_edit_wnd_pos.y = y;
               anim_edit_wnd_pos.valid = true;
            }
            SetWindowPos(hDlg, NULL, anim_edit_wnd_pos.x, anim_edit_wnd_pos.y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
         }
         return true;

      case WM_HELP:
         {
            LPHELPINFO hi = (LPHELPINFO)lParam;
            DisplayHelp(hDlg, (word)hi->iCtrlId, help_texts);
         }
         return true;

      case WM_COMMAND:
         {
            WORD idc = LOWORD(wParam);
            switch(idc){
            case IDC_ADD_KEY:
            case IDC_DEL_KEY:
            case IDC_PREV_KEY:
            case IDC_NEXT_KEY:
            case IDC_FIRST_KEY:
            case IDC_LAST_KEY:
            case IDC_PLAY_TOGGLE:
            case IDC_COPY_POSE:
            case IDC_PASTE_POSE:
            case IDC_PICK_NOTIFY_FRM:
            case IDC_CHANGE_MODEL:
            case IDC_NOTE_CLEAR:
            case IDC_ANIM_TIME_SCALE:
            case IDC_PASTE_MIRROR:
            case IDC_INSERT_TIME:
            case IDC_PASTE_POSE_SELECTION:
            case IDC_PASTE_POSE_SELECTION_KEYS:
            case IDC_SMOOTH_ANIM_SPEED:
               {
                  if(HIWORD(wParam) == (WORD)BN_CLICKED)
                     Action(GetIDCAction(idc), NULL);
               }
               break;
            case IDC_SMOOTH:
            case IDC_EASY_FROM:
            case IDC_EASY_TO:
               switch(HIWORD(wParam)){
               case EN_CHANGE:
                  {
                     C_str num = GetDlgItemText(hDlg, idc);
                     float f;
                     int i = sscanf(num, "%f", &f);
                     if(i == 1){
                        switch(idc){
                        case IDC_SMOOTH:
                           SetKeySmooth(f);
                           break;
                        case IDC_EASY_FROM:
                           SetKeyEasyFrom(f);
                           break;
                        case IDC_EASY_TO:
                           SetKeyEasyTo(f);
                           break;
                        }
                     }
                  }
                  break;
               case EN_KILLFOCUS:
                  switch(idc){
                  case IDC_SMOOTH:
                     UpdateSmooth();
                     break;
                  case IDC_EASY_FROM:
                     UpdateEasyFrom();
                     break;
                  case IDC_EASY_TO:
                     UpdateEasyTo();
                     break;
                  }
                  break;
               }
               break;
            case IDC_KEY_TIME:
               switch(HIWORD(wParam)){
               case EN_CHANGE:
                  {
                     C_str num = GetDlgItemText(hDlg, idc);
                     dword t1;
                     int i = sscanf(num, "%i", &t1);
                     if(i == 1){
                        SetKeyTime(t1);
                     }
                  }
                  break;
               case EN_KILLFOCUS:
                  UpdateKeyTime();
                  break;
               }
               break;
            case IDC_ANIM_NAME:
               {
                  WORD code = HIWORD(wParam);
                  switch(code){
                  case EN_CHANGE:
                     {
                        C_str new_name = GetDlgItemText(hDlg, LOWORD(wParam));
                        if(new_name == GetEditAnim().name)
                           break;
                        if(new_name.Size() && !IsAnimNameUsed(new_name))
                           RenameEditAnim(new_name);
                     }
                     break;
                  case EN_KILLFOCUS:
                     UpdateAnimName();
                     break;
                  }
               }
               break;
            case IDC_NOTIFY:
               {
                  switch(HIWORD(wParam)){
                  case EN_CHANGE:
                     {
                        C_str new_note = GetDlgItemText(hDlg, LOWORD(wParam));
                        SetKeyNote(new_note);
                        //GetEditAnim().CurrPoseKey().notify = new_note;
                     }
                     break;
                  case EN_KILLFOCUS:
                     UpdateNote();
                     break;
                  }
               }
               break;
            case IDCLOSE:     //
               Action(E_ANIM_CLOSE_EDIT);
               break;
            case IDCANCEL:
               SetActiveWindow((HWND)ed->GetIGraph()->GetHWND());
               break;
            }
         }
         break;

      case WM_CLOSE:
         Action(E_ANIM_CLOSE_EDIT);
         break;

      case WM_HSCROLL:
         {
            HWND hwndScrollBar = (HWND) lParam;
            if(hwndScrollBar == GetDlgItem(hDlg, IDC_TIME_SLIDER)){
               dword value = SendMessage(hwndScrollBar, TBM_GETPOS, 0, 0);
               assert(value >= 0 && value <= GetEditAnim().GetEndTime());
               switch(LOWORD(wParam)){
               //case TB_LINEDOWN:
               //case TB_LINEUP:
               //   break;
               case TB_PAGEDOWN:
               case TB_PAGEUP:
               case TB_ENDTRACK:
               case TB_BOTTOM:
               case TB_TOP:
                  {
                     SetAnimToClosestKey(value);
                     UpdatePlayTime(false);
                  }
                  break;
               default: 
                  anim_play.SetTime(anim_mod, value);
                  anim_mod->Tick(0);
                  UpdatePlayTime(true);
               }
            }
         }
         break;

      case WM_NOTIFY:
         {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            switch(nmhdr->idFrom){
            case IDC_TIME_SLIDER:
               if(nmhdr->code == NM_RELEASEDCAPTURE){
                  dword value = SendMessage(nmhdr->hwndFrom, TBM_GETPOS, 0, 0);
                  SetAnimToClosestKey(value);
                  UpdatePlayTime(false);
               }
               break;

            case IDC_SPIN_EASY_FROM:
            case IDC_SPIN_EASY_TO:
            case IDC_SPIN_SMOOTH:
               {
                  switch(nmhdr->code){
                  case UDN_DELTAPOS:
                     {
                        int id_buddy = 0;
                        switch(nmhdr->idFrom){
                        case IDC_SPIN_EASY_FROM: id_buddy = IDC_EASY_FROM; break;
                        case IDC_SPIN_EASY_TO: id_buddy = IDC_EASY_TO; break;
                        case IDC_SPIN_SMOOTH: id_buddy = IDC_SMOOTH; break;
                        default: assert(0);
                        }

                        NMUPDOWN *ud = (NMUPDOWN*)lParam;
                        int delta = -ud->iDelta;
                        char buf[64];
                        if(GetDlgItemText(hDlg, id_buddy, buf, sizeof(buf))){
                           float num;
                           if(sscanf(buf, "%f", &num)==1){
                              float new_num = Max(0.0f, Min(1.0f, num + (float)delta * .1f));
                              if(num!=new_num)
                                 SetDlgItemText(hDlg, id_buddy, FloatStrip(C_fstr("%f", new_num)));
                           }
                        }
                     }
                     return 1;//return 1 to disallow Windows to change spiner's position
                  }
               }
               break;
            }
         }
         break;
      }
      return 0;
   }

//----------------------------

   static BOOL CALLBACK cbAnimFrames(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){

      C_edit_AnimEdit *ea = (C_edit_AnimEdit*)GetWindowLong(hDlg, GWL_USERDATA);

	   switch(message){
      case WM_INITDIALOG:
         {

            const int PRS_W = 12;
            const int WEIGHT_W = 27;
            const int LAST_COL_ADD_W = 14; //tolerance vertical scrollbar
            SetWindowLong(hDlg, GWL_USERDATA, lParam);
                              //set checklistbox style
            HWND hwnd_lc = GetDlgItem(hDlg, IDC_ANIM_FRM_LIST);

            SendMessage(hwnd_lc, LVM_SETEXTENDEDLISTVIEWSTYLE, 
               LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT, 
               LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

            RECT rc;
            GetClientRect(hwnd_lc, &rc);

            LVCOLUMN lvc;
            memset(&lvc, 0, sizeof(lvc));
            lvc.mask = LVCF_WIDTH;
            lvc.cx = (rc.right - rc.left) - (PRS_W*3 + WEIGHT_W + LAST_COL_ADD_W);
            SendMessage(hwnd_lc, LVM_INSERTCOLUMN, 0, (LPARAM)&lvc);
                              //P|R|S column
            lvc.mask = LVCF_WIDTH|LVCF_SUBITEM;
            for (int k = 1; k < 5; k++){
               switch(k){
               case 1: case 3: lvc.cx = PRS_W; break;
               case 2: lvc.cx = PRS_W -2; break;
               case 4: lvc.cx = WEIGHT_W; break;
               }
               lvc.iSubItem = k;
               SendMessage(hwnd_lc, LVM_INSERTCOLUMN, k, (LPARAM)&lvc);
            }
            return true;
         }
         break;

      case WM_COMMAND:
         {
            WORD idc = LOWORD(wParam);
            switch(idc){
            case IDC_CLEAR:
            case IDC_PICK_MASK:
            case IDC_ALL:
            case IDC_SEL:
            case IDC_COPY:
            case IDC_WEIGHT:
               {
                  C_edit_AnimEdit *ea = (C_edit_AnimEdit*)GetWindowLong(hDlg, GWL_USERDATA);
                  if(HIWORD(wParam) == (WORD)BN_CLICKED){
                     assert(ea);
                     ea->Action(GetIDCAction2(idc), NULL);
                  }
               }
               break;
            }
         }
         break;
      case WM_NOTIFY:
         {
            switch(wParam){
            case IDC_ANIM_FRM_LIST:
               {
                  LPNMHDR hdr = (LPNMHDR)lParam;
                  switch(hdr->code){
                  case LVN_ITEMCHANGED:
                     {
                        NMLISTVIEW *pnmv = (LPNMLISTVIEW) lParam;
                        assert(pnmv);
                        bool state_changed = (pnmv->uChanged&LVIF_STATE);
                        if(!state_changed)
                           break;
                        PI3D_frame frm = (PI3D_frame)pnmv->lParam;
                              //select frame if item selected
                        bool was_selected = pnmv->uOldState&LVIS_SELECTED;
                        bool is_selected = pnmv->uNewState&LVIS_SELECTED;
                        if((was_selected != is_selected) && ea){
                           const C_vector<PI3D_frame> &sel_list = ea->e_slct->GetCurSel();
                           int j = FindPointerIndex((void**)&sel_list.front(), sel_list.size(), frm);

                           if(!was_selected && is_selected){
                              if(j==-1)
                                 ea->e_slct->AddFrame(frm);
                           }else
                           if(was_selected && !is_selected){
                              if(j!=-1)
                                 ea->e_slct->RemoveFrame(frm);
                           }

                        }
                              //add /remove from anim when user check/uncheck item
                        bool was_on = ((pnmv->uOldState&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2));
                        bool on = ((pnmv->uNewState&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2));
                        if(was_on == on)
                           break;
                              //add/del frame from selection
                        if(!ea)
                           break;
                        bool is_in_anim = ea->IsFrameInEditAnim(frm->GetOrigName());
                        bool changed(true);
                        if(on && !is_in_anim){
                           changed = ea->Action(E_ANIM_ADD_FRAME, frm);
                        }
                        if(!on && is_in_anim){
                           changed = ea->Action(E_ANIM_DEL_FRAME, frm);
                        }
                              //return state if not changed
                        if(!changed){
                           LVITEM li;
                           memset(&li, 0, sizeof(li));
                           li.stateMask = LVIS_STATEIMAGEMASK;
                           li.mask = LVIF_STATE;
                           li.state = INDEXTOSTATEIMAGEMASK(1 + (was_on ? 1 : 0));
                           SendMessage(GetDlgItem(hDlg, IDC_ANIM_FRM_LIST), LVM_SETITEMSTATE, pnmv->iItem, (LPARAM)&li);
                        }
                              //flash frame (if check toggled by user's input)
                        if(ea->hWnd_anim_frames)
                           ea->e_slct->FlashFrame(frm);
                              //update fram list due prs which can change
                        ea->UpdatePRSWColumns();
                     }
                     break;
                  case NM_DBLCLK:
                     {
                              //get frame from dblclicked item
                        NMLISTVIEW *pnmv = (LPNMLISTVIEW) lParam;
                        assert(pnmv);
                        if(pnmv->iItem == -1)
                           break;
                              //get lParam info from item
                        LVITEM li;
                        memset(&li, 0, sizeof(li));
                        li.iItem = pnmv->iItem;
                        li.mask = LVIF_PARAM;
                        HWND hwnd_lc = GetDlgItem(hDlg, IDC_ANIM_FRM_LIST);
                        SendMessage(hwnd_lc, LVM_GETITEM, 0, (LPARAM) &li);

                        PI3D_frame frm = (PI3D_frame)li.lParam;
                              //add to selection
                        if(!ea)
                           break;
                        {
                           ea->e_slct->Clear();
                           ea->e_slct->AddFrame(frm);
                              //switch to active window for easier editing
                           if(hdr->code == NM_DBLCLK)
                              SetActiveWindow((HWND)ea->ed->GetIGraph()->GetHWND());
                        }
                     }
                     break;
                  }
               }
               break;
            }
         }
         break;
      }

      return 0;
   }

//----------------------------

   dword FindMask(const char *name, bool warn){

      for(dword i = mask_modes.modes.size(); i--; ){
         if(mask_modes.GetMode(i).GetName() == name)
            return i;
      }
      if(warn && e_log){
         C_str msg = C_fstr("Anim mask '%s' not found, set to empty.Try upade '%s'.", 
            (const char*)name, ANIM_MASK_MODES_TXT_PATH);
         e_log->AddText(msg, 0xffff0000);
      }
      return 0;
   }

//----------------------------

   int FindNameID(C_buffer<C_str> &list, const C_str &name) const{

      for(int i = list.size(); i--; ){
         if(list[i].Match(name))
            break;
      }
      return i;
   }

//----------------------------

   void SaveAnims(C_chunk &ck) const{

      for(dword i = 0; i < anims.size(); i++){

                              //write anim info
         const S_pose_anim_edit &pa = anims[i];

         assert(const_cast<C_edit_AnimEdit*>(this)->CheckAnimSets(pa));

         ck <<= CT_POSE_ANIM_HEADER;
         {
                                 //name
            ck(CT_POSE_ANIM_NAME, pa.name);
            if(anim_mod || pa.model_name.Size())
               ck(CT_POSE_ANIM_ROOT_FRM, pa.model_name);

            if(pa.GetMaskId()){
               C_str mask_name = GetAnimMask(pa).GetName();
               ck(CT_POSE_ANIM_MASK_NAME, mask_name);
            }else{
               assert(!pa.GetCustomMask().IsEmpty());
               ck <<= CT_POSE_ANIM_CUSTOM_MASK;
               pa.GetCustomMask().Save(*ck.GetHandle());
               --ck; //CT_POSE_ANIM_CUSTOM_MASK
            }
                                 //anim data chunk
            ck <<= CT_POSE_ANIM;
            {
               C_buffer<C_str> names_list;
               pa.GetAnimFrames(names_list);
                                    //first save frames which are used in animation
               ck <<= CT_POSE_ANIM_FRAMES;
               {
                  for(dword i = 0; i < names_list.size(); i++ )
                     ck(CT_POSE_ANIM_FRAME_LINK, names_list[i]);
               }
               --ck; //CT_POSE_ANIM_FRAMES

               const S_mask_mode &mask_mode = GetAnimMask(pa);
                                    //key tracks
               ck <<= CT_POSE_ANIM_KEYS;
               {
                  for(dword j = 0; j < pa.key_list.size(); j++){
                     const S_pose_key &key = pa.key_list[j];
                     ck <<= CT_POSE_ANIM_KEY;
                     {
                        ck
                           (CT_POSE_KEY_TIME, (int)key.time)
                           (CT_POSE_KEY_EASY_FROM, (float)key.easy_from)
                           (CT_POSE_KEY_EASY_TO, (float)key.easy_to)
                           (CT_POSE_KEY_SMOOTH, (float)key.smooth);
                        if(key.notify.Size()){
                           ck
                              (CT_POSE_KEY_NOTIFY, key.notify)
                              (CT_POSE_KEY_NOTIFY_LINK, key.notify_link)
                              ;
                        }
                                          //animation set(frames links and transforms)
                        ck <<= CT_POSE_KEY_ANIM_SET;
                        {
                                       //raw anim set data:
                           assert(key.anim_set->NumAnimations() <= 0xffff);
                           word num_anims = (word)key.anim_set->NumAnimations();
                           ck.Write(&num_anims, sizeof(word));
                           for(word i = 0; i < num_anims; i++){
                              CPI3D_animation_base ab = ((PI3D_animation_set)(CPI3D_animation_set)key.anim_set)->GetAnimation(i);
                              assert(ab && ab->GetType() == I3DANIM_POSE);
                              PI3D_anim_pose ap = (PI3D_anim_pose)ab;
                                             //check if is not empty
                              const C_str &anim_link = key.anim_set->GetAnimLink(i);
                              bool has_pos = ap->GetPos() && !mask_mode.Locked(anim_link, TR_POS);
                              bool has_rot = ap->GetRot() && !mask_mode.Locked(anim_link, TR_ROT);
                              bool has_pow = ap->GetPower() && !mask_mode.Locked(anim_link, TR_POW);
                                             //set should not have any empty anims at this point
                              assert(("Empty anim in set(not prs or fully masked)", has_pos || has_rot || has_pow));
                              dword id = FindNameID(names_list, anim_link);
                              assert(("link must be in saved list", id != -1));
                              ck.Write(&id, sizeof(word));
                              ck.Write(&has_pos, sizeof(bool));
                              if(has_pos)
                                 ck.Write(ap->GetPos(), sizeof(S_vector));
                              ck.Write(&has_rot, sizeof(bool));
                              if(has_rot)
                                 ck.Write(ap->GetRot(), sizeof(S_quat));
                              ck.Write(&has_pow, sizeof(bool));
                              if(has_pow)
                                 ck.Write(ap->GetPower(), sizeof(float));
                           }
                        }
                        --ck;  //CT_POSE_KEY_ANIM_SET
                     }
                     --ck;  //CT_POSE_ANIM_KEY
                  } 
               }
               --ck;  //CT_POSE_ANIM_KEYS
                              
                                    //anim_weights
               ck <<= CT_POSE_ANIM_WEIGHTS;
               {
                  struct S_hlp{
                     static void EnumProc(const C_str &link, float val, dword c){
                        C_chunk &ck = *(C_chunk*)c;
                        ck <<= CT_POSE_ANIM_WEIGHT;
                        ck
                           (CT_POSE_ANIM_WEIGHT_LINK, link)
                           (CT_POSE_ANIM_WEIGHT_VAL, val)
                           ;
                        --ck; //CT_POSE_ANIM_WEIGHT
                     }
                  };
                  pa.EnumWeights(S_hlp::EnumProc, (dword)&ck);
               }
               --ck; //CT_POSE_ANIM_WEIGHTS

               ck
                  (CT_POSE_ANIM_POS, pa.edit_pos)
                  (CT_POSE_ANIM_ROT, pa.edit_rot)
                  ;
            }
            --ck;  //CT_POSE_ANIM
         }
         --ck; //CT_POSE_ANIM_HEADER
      }
   }

//----------------------------
// Load animations from chunk into provided C_vector.
   void LoadAnims(C_chunk &ck, C_vector<S_pose_anim_edit> &anims, int *edit_anim_id = NULL,
      PI3D_model import_model = NULL, int force_mask_id = -1){

      while(ck){
         switch(++ck){
         case CT_POSE_LAST_EDITED:
            {
               int id = ck.RIntChunk();
               if(edit_anim_id)
                  *edit_anim_id = id;
            }
            break;

         case CT_POSE_ANIM_HEADER:
            {
                                 //just read anim name and store data
               C_str anim_name;
               C_str model_name;
                              //set mask_id to 0, because zero mask is not saved in format
               dword mask_id = (force_mask_id == -1) ? 0 : force_mask_id;
               //PI3D_model mod = NULL;
               S_mask_mode *cust_mask = NULL;

               while(ck){
                  switch(++ck){
                  case CT_POSE_ANIM_NAME:
                     anim_name = ck.RStringChunk();
                     break;

                  case CT_POSE_ANIM_ROOT_FRM:
                     {
                        if(import_model){
                           model_name = import_model->GetName();
                           --ck;
                        }else{
                           model_name = ck.RStringChunk();
                        }
                     }
                     break;

                  case CT_POSE_ANIM_MASK_NAME:
                     {
                        C_str mask_name = ck.RStringChunk();
                        if(force_mask_id==-1)
                           mask_id = FindMask(mask_name, true);
                        assert(mask_id != -1);
                     }
                     break;

                  case CT_POSE_ANIM_CUSTOM_MASK:
                     {
                        assert(!cust_mask);
                        cust_mask = new S_mask_mode;
                        cust_mask->Load(*ck.GetHandle());
                        --ck;
                     }
                     break;

                  case CT_POSE_ANIM:
                     {
                                          //create new anim
                        anims.push_back(S_pose_anim_edit());
                        S_pose_anim_edit &pa = anims.back();
                        assert(anim_name.Size());
                        pa.name = anim_name;
                        pa.model_name = model_name;
                        pa.edit_pos.x = 1.1e+16f; //mark 'invalid' pos
                        if(!pa.Load(ck, ed->GetDriver())){
                           if(e_log)
                              e_log->AddText(C_fstr("Failed load anim '%s', scene corrupted.", (const char*)anim_name), 0xffff0000);
                           anims.pop_back();
                           if(*edit_anim_id==(int)anims.size())
                              --*edit_anim_id;
                           //ClearAnims();
                           return;
                        }
                        assert(mask_id != -1);
                              //create default custom mask if not loaded
                        if((!mask_id && cust_mask == NULL) || pa.edit_pos.x>=1e+16f){
                              //find model now
                           PI3D_model mod = I3DCAST_MODEL(ed->GetScene()->FindFrame(model_name, ENUMF_MODEL));
                           if(!mod){
                              if(e_log)
                                 e_log->AddText(C_fstr("Anim root frame '%s' not found.", (const char*)model_name));
                              pa.edit_pos.Zero();
                           }else{
                              if(!mask_id && cust_mask == NULL){
                              //mod = I3DCAST_MODEL(frm);
                                 cust_mask = new S_mask_mode;
                                 cust_mask->CreateCustom(mod);
                              }
                              if(pa.edit_pos.x>=1e+16f){
                                 pa.edit_pos = mod->GetPos();
                                 pa.edit_rot = mod->GetRot();
                              }
                           }
                        }
                        SetAnimMask(pa, mask_id, cust_mask);
                        --ck;
                     }
                     break;

                  default:
                     assert(("Unknown chunk type", 0));
                     --ck;
                  }
               }
               --ck;
               if(cust_mask){
                  delete cust_mask;
                  cust_mask = NULL;
               }

            }
            break;

         default: 
            assert(("Unknown chunk type", 0));
            --ck;
         }
      }
      if(edit_anim_id){
         assert(*edit_anim_id < (int)anims.size());
                              //if load of id failed, close anim editor
         if(*edit_anim_id >= (int)anims.size()){
            *edit_anim_id = -1;
         }
      }
   }

//----------------------------

   void SetAnimToClosestKey(int time){

      int closest_key = GetEditAnim().FindClosestKey(time);
      GotoAnimKey(closest_key);
      anim_play.SetTime(anim_mod, GetEditAnim().CurrPoseKey().time);
   }

//----------------------------

   void PlayModeToggle(){

      static const char * play_cpt[] = {"P&lay", "Stop p&lay" };
      switch(mode){
      case MODE_OFF:
         ed->Message("Off mode, no anim to play.");
         return;
      case MODE_EDIT:
         {
            if(!IsEditAnimValid())
               break;
            mode = MODE_PLAY;
            EnableEditControl(false);
#ifdef _DEBUG
            CheckAnimSets(GetEditAnim());
#endif
            PI3D_animation_set anim2play = ed->GetDriver()->CreateAnimationSet();

            if(GetEditAnim().ConvertToAnimSet(anim2play, ed->GetDriver())){
               int last_time = anim_play.GetTime();
               anim_play.AddAnim(anim_mod, anim2play, loop_mode);
               anim_play.SetTime(anim_mod, last_time);
            }
            anim2play->Release();
                              //avoid pops at beginning, caused by lag from preparing the anim here
            ed->GetIGraph()->NullTimer();
         }
         break;
      case MODE_PLAY:
         {
            EnableEditControl(true);
            SetAnimToClosestKey(anim_play.GetTime());
            UpdatePlayTime(false);
            UpdateKeyGui();
            mode = MODE_EDIT; //must be after anything, which could invalidate pose_key_dirty (in notification callback)
         }
         break;
      }
      SetDlgItemText(hWnd_anim_edit, IDC_PLAY_TOGGLE, play_cpt[mode == MODE_PLAY ? 1 : 0]);
   }

//----------------------------
// Enable/disable various control elements when in playback mode.
   void EnableEditControl(bool on_off) const{

                              //edit
      assert(hWnd_anim_edit);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_CHANGE_MODEL), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_ANIM_NAME), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_KEY_TIME), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_ANIM_TIME_SCALE), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_EASY_FROM), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_EASY_TO), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_SMOOTH), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_PICK_NOTIFY_FRM), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_NOTIFY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_ADD_KEY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_DEL_KEY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_PREV_KEY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_NEXT_KEY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_FIRST_KEY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_LAST_KEY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_COPY_POSE), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_PASTE_POSE), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_PASTE_MIRROR), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_NOTE_CLEAR), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_INSERT_TIME), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_SMOOTH_ANIM_SPEED), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_PASTE_POSE_SELECTION), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_edit, IDC_PASTE_POSE_SELECTION_KEYS), on_off);
                              //frames
      assert(hWnd_anim_frames);
      EnableWindow(GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_frames, IDC_PICK_MASK), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_frames, IDC_ALL), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_frames, IDC_COPY), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_frames, IDC_CLEAR), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST), on_off);
      EnableWindow(GetDlgItem(hWnd_anim_frames, IDC_WEIGHT), on_off);
   }

//----------------------------

   void ChangeAnimModel(){

      if(!IsActive())
         return;

      PI3D_model new_mod = GetNewAnimModel(true);
      if(!new_mod)
         return;

      if(!ed->CanModify())
         return;
      ed->SetModified(true);

      S_pose_anim_edit &pa = GetEditAnim();
      CloseAnimModel();
      pa.model_name = new_mod->GetName();
      InitAnimModel(pa);

      anim_play.ResetTime();
      UpdateModelNameInDialog();
      UpdateModel();
      UpdateFrameSelect();
      UpdateAnimation();
   }

//----------------------------
// Update animation for playback.
   void UpdateAnimation(){

      PI3D_animation_set anim2play = ed->GetDriver()->CreateAnimationSet();
      if(GetEditAnim().ConvertToAnimSet(anim2play, ed->GetDriver()))
         anim_play.AddAnim(anim_mod, anim2play, loop_mode);
      anim2play->Release();
   }

//----------------------------

   void cbConnectChildren(CPI3D_frame root, const S_pose_anim &pa){

      assert(root);
      assert(e_dbgline);
      const S_vector &root_pos = root->GetWorldPos();
      PI3D_frame const *frms = root->GetChildren();
      for(int i = root->NumChildren(); i--; ){

         PI3D_frame frm = frms[i];

         switch(frm->GetType()){
         case FRAME_JOINT:
            if(pa.IsInFramesList(frm->GetOrigName()))
               e_dbgline->AddLine(root_pos, frm->GetWorldPos(), DL_ONE_TIME);
            break;
         }
         cbConnectChildren(frm, pa);
      }
   }

//----------------------------

   void SetAnimFramesTransparency(bool on_off) const{

      if(!IsEditAnimValid())
         return;
      if(!anim_mod)
         return;

      CPI3D_frame const *frms = anim_mod->GetFrames();
      for(int i = anim_mod->NumFrames(); i--; ){
         CPI3D_frame frm = frms[i];
         SetFrameTransp(const_cast<PI3D_frame>(frm), on_off);
      }
   }


//----------------------------

   void SetFrameTransp(PI3D_frame frm, bool on_off) const{

      const S_pose_anim_edit &pa = GetEditAnim();
      switch(frm->GetType()){
      case FRAME_VISUAL:
         {
            PI3D_visual vis = I3DCAST_VISUAL(frm);
            switch(vis->GetVisualType()){
            case I3D_VISUAL_SINGLEMESH:
               {
                  const float SNG_MESH_ALPHA = .9f;
                  vis->SetVisualAlpha(on_off ? SNG_MESH_ALPHA : 1.0f);
               }
               break;
            default:
               {
                  const float UNUSED_FRAMES_ALPHA = .45f;
                  bool used = pa.IsInFramesList(frm->GetOrigName());
                  vis->SetVisualAlpha((!used && on_off) ? UNUSED_FRAMES_ALPHA : 1.0f);
               }
               break;
            }
         }
         break;
      }
   }

//----------------------------

   void DrawAnimSkelet(){

      if(!e_dbgline)
         return;
      if(!IsEditAnimValid())
         return;
      const S_pose_anim_edit &pa = GetEditAnim();
      if(!anim_mod)
         return;
      cbConnectChildren(anim_mod, pa);
   }

//----------------------------

   static void I3DAPI cbAnimPlay(I3D_CALLBACK_MESSAGE msg, dword prm1, dword prm2, void *context){

      switch(msg){
      case I3DCB_ANIM_NOTE:
         {
            PI3D_frame frm = (PI3D_frame) prm1;
            assert(prm2);
            const I3D_note_callback &nc = *(const I3D_note_callback *)prm2;
            C_edit_AnimEdit *ae = (C_edit_AnimEdit*) context;
            assert(ae);
            ae->OnNotify(frm, nc);
         }
         break;
      }

   }

//----------------------------

   struct S_anim_init{

      C_str name;
      dword mask_id;
      S_mask_mode cust_mask;
      S_anim_init() : mask_id(0)
      {}
   };

//----------------------------

   bool SelectAnimParams(PIGraph igraph, HWND hwnd_parent, const char *title, S_anim_init &ai){

      struct S_hlp{
         PIGraph igraph;
         S_anim_init &ai;
         C_edit_AnimEdit *ae;

         S_hlp(PIGraph ig1, S_anim_init &ai1, C_edit_AnimEdit *ae1) :
            igraph(ig1),
            ai(ai1),
            ae(ae1)
         {}

         static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

            switch(uMsg){
            case WM_INITDIALOG:
               {
                  S_hlp *hlp = (S_hlp*)lParam;
                  InitDlg(hlp->igraph, hwnd);
                  SetDlgItemText(hwnd, IDC_NAME_EDIT, (const char*)hlp->ai.name);
                  ShowWindow(hwnd, SW_SHOW);
                  SetWindowLong(hwnd, GWL_USERDATA, lParam);
                  hlp->ae->FillMaskCB(GetDlgItem(hwnd, IDC_SELECT_MASK));
                  SendDlgItemMessage(hwnd, IDC_SELECT_MASK, CB_SETCURSEL, 0, 0);
               }
               return 1;

            case WM_COMMAND:
               switch(LOWORD(wParam)){
               case IDCANCEL: 
                  EndDialog(hwnd, 0); 
                  break;
               case IDOK:
                  {
                     S_hlp *hlp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                     char buf[256];
                     SendDlgItemMessage(hwnd, IDC_NAME_EDIT, WM_GETTEXT, 256, (LPARAM)buf);
                     hlp->ai.name = buf;
                     hlp->ai.mask_id = SendDlgItemMessage(hwnd, IDC_SELECT_MASK, CB_GETCURSEL, 0, 0);
                     hlp->ai.mask_id = Max(0, (int)hlp->ai.mask_id);
                     EndDialog(hwnd, 1);            
                  }
                  break;
               case IDC_NEW_MASK:
                  {
                     S_hlp *hlp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                     int new_id = hlp->ae->SelectMaskIdDlg(hwnd, &hlp->ai.cust_mask);
                                 //update lb for case some mask was added/removed
                     hlp->ae->FillMaskCB(GetDlgItem(hwnd, IDC_SELECT_MASK));
                                 //set listbox to new item
                     SendDlgItemMessage(hwnd, IDC_SELECT_MASK, CB_SETCURSEL, (new_id != -1) ? new_id : 0, 0);
                  }
                  break;
               }
               break;

            }
            return 0;
         }
      } hlp(igraph, ai, this);
      int i = DialogBoxParam(GetHInstance(), "IDD_ANIM_CREATE", hwnd_parent, S_hlp::dlgProc, (LPARAM)&hlp);
      return i;
   }

//----------------------------

   void SavePose2Key(){

      if(!IsEditAnimValid())
         return;
      GetEditAnim().SetKey(anim_mod, ed->GetDriver());
      UpdateAnimation();
   }

//----------------------------

   virtual void AfterLoad(){
      if(edit_anim_id!=-1){
         int id = edit_anim_id;
         edit_anim_id = -1;
         SetEditAnim(id, false);
      }
   }

//----------------------------

   virtual void BeforeFree(){
                              //close opened anims
      ClearAnims();
      pose_key_dirty = false;
   }

//----------------------------

   virtual void LoadFromMission(C_chunk &ck){

      ClearAnims();
      LoadAnims(ck, anims, &edit_anim_id);
   }

//----------------------------

   virtual void MissionSave(C_chunk &ck, dword phase){

      if(phase==1){
                              //store current pos/rot
         if(IsEditAnimValid()){
            S_pose_anim_edit &pa = GetEditAnim();
            pa.edit_pos = anim_mod->GetPos();
            pa.edit_rot = anim_mod->GetRot();
         }
                              //save all anims
         ck <<= CT_POSE_ANIMS;
         {
            ck(CT_POSE_LAST_EDITED, (int)edit_anim_id);
            SaveAnims(ck);
         }
         --ck;
      }
   }

//----------------------------

   virtual void SetActive(bool active){
                              //close anim editor before editor gets inactive
      if(!active)
         SetEditAnim(-1);
   }

//----------------------------

public:
   C_edit_AnimEdit() : mode(MODE_OFF),
      edit_anim_id(-1),
      model_transparency(false),
      loop_mode(true),
      lock_anim_model(false),
      render_paths(false),
      //curr_playtime(0),
      pose_key_dirty(false),
      hWnd_anim_edit(NULL),
      hWnd_anim_frames(NULL)
   {}

//----------------------------

   virtual bool Init(){

      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      e_log = (PC_editor_item_Log)ed->FindPlugin("Log");
      e_modify = (PC_editor_item_Modify)ed->FindPlugin("Modify");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      e_dbgline = (PC_editor_item_DebugLine)ed->FindPlugin("DebugLine");
      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      e_timescale = (PC_editor_item_TimeScale)ed->FindPlugin("TimeScale");
      if(!e_modify || !e_modify || !e_undo || !e_props || !e_timescale || !e_dbgline)
         return false;

#define MENU_BASE "%85 &Anim\\Frame\\"

      //ed->AddShortcut(this, E_ANIM_CREATE,      MENU_BASE"%10 &Create Anim \tF2", K_F2, 0);
      ed->AddShortcut(this, E_ANIM_SELECT_ANIM, MENU_BASE"%10 &Select anim \tF2", K_F2, 0);
      ed->AddShortcut(this, E_ANIM_IMPORT_KEYFRAME_ANIM, MENU_BASE"%10 &Import anim from i3d", K_NOKEY, 0);
      ed->AddShortcut(this, E_ANIM_IMPORT_MISSION_ANIM, MENU_BASE"%10 Import anim from &bin", K_NOKEY, 0);
      ed->AddShortcut(this, E_ANIM_MIRROR_POSE, MENU_BASE"%20 &Mirror pose \tCtrl+Shift+R", K_R, SKEY_CTRL | SKEY_SHIFT);
      ed->AddShortcut(this, E_ANIM_LOOP_MODE, MENU_BASE"Loop mode", K_NOKEY, 0);
      ed->AddShortcut(this, E_ANIM_LOCK_MODEL, MENU_BASE"Lock anim model", K_NOKEY, 0);
#ifdef ENABLE_MIRROR_FRAMES
      ed->AddShortcut(this, E_ANIM_MIRROR_FRAMES, MENU_BASE"%20 &Mirror frames(test) \tCtrl+Alt+R", K_R, SKEY_CTRL | SKEY_ALT);
#endif
      ed->AddShortcut(this, E_ANIM_TOGGLE_MODEL_TRANSP,  MENU_BASE"%i %30 &Model tranparency", K_NOKEY, 0);
      //ed->AddShortcut(this, E_ANIM_TOGGLE_PATHS,  MENU_BASE"%30 &Render paths", 0, 0);
      ed->AddShortcut(this, E_ANIM_CLOSE_EDIT,  MENU_BASE"%i %90 &Close anim editor", K_NOKEY, 0);

      e_modify->AddNotify(this, E_ANIMEDIT_NOTIFY_MODIFY);

      e_slct->AddNotify(this, E_ANIMEDIT_NOTIFY_SELECTION);
      LoadModes();

      ed->CheckMenu(this, E_ANIM_LOOP_MODE, loop_mode);
      ed->CheckMenu(this, E_ANIM_LOCK_MODEL, lock_anim_model);
      //ed->RegForRender(this);

      return true;
   }

//----------------------------

   virtual void Close(){
      pose_clipboard = NULL;
      ToggleDialog(false, false);
      //ClearAnims();         //MB: can't do that, it uses plugins, which may be already destroyed
   }

//----------------------------

   bool ActionAnimCreate(HWND hwnd){

      if(!ed->CanModify())
         return false;

      PI3D_model mod = GetNewAnimModel(true);
      if(!mod)
         return false;
      if(CreateNewAnim(hwnd, mod)){
         ToggleDialog(true, false);
         UpdateAnimGui();
         ed->SetModified(true);
         return true;
      }
      return false;
   }

//----------------------------

   bool ActionDuplicateAnim(HWND hwnd_parent, dword id){

      if(!ed->CanModify())
         return false;
      if(DuplicateAnim(hwnd_parent, id)){
         ToggleDialog(true);
         UpdateAnimGui();
         ed->SetModified(true);
         return true;
      }
      return false;
   }

//----------------------------

   void ActionSetKeyTimeDirect(){

      if(!ed->CanModify())
         return;
      assert(IsEditAnimValid());

      const S_pose_anim_edit &pa = GetEditAnim();
                        //first key has always zero time, do not allow modify
      if(!pa.CurrKey()){
         ed->Message("Can't modify first key's time.");
         return;
      }
      int t = pa.CurrPoseKey().time;
      int min_time = 0;
      int max_time = 1000000;
      if(pa.CurrKey())
         min_time = pa.key_list[pa.CurrKey()-1].time + 1;
      if(pa.CurrKey() != pa.key_list.size()-1)
         max_time = pa.key_list[pa.CurrKey()+1].time - 1;
      if(SelectNewTime(t, "Enter key time:", min_time, max_time)){
         SetKeyTime(t);
         UpdateKeyTime();
      }
   }

//----------------------------

   void SaveFrameTransformUndo(CPI3D_frame frm){

      C_undo_buf undo_buf;
      undo_buf.InitW();
      undo_buf.WriteString(frm->GetOrigName());
      undo_buf.WriteMem(&frm->GetPos(), sizeof(S_vector));
      undo_buf.WriteMem(&frm->GetRot(), sizeof(S_quat));
      float f = frm->GetScale();
      undo_buf.WriteMem(&f, sizeof(float));
      e_undo->Save(this, E_ANIM_UNDO_FRM_TRANSFORM, undo_buf.Begin(), undo_buf.Size());
   }

//----------------------------

   void UndoFrameTransform(void *buf){

      C_undo_buf undo_buf;
      undo_buf.InitR(buf);

      C_str frm_name;
      S_vector pos;
      S_quat rot;
      float scale;
      undo_buf.ReadString(frm_name);
      undo_buf.ReadMem(&pos, sizeof(S_vector));
      undo_buf.ReadMem(&rot, sizeof(S_quat));
      undo_buf.ReadMem(&scale, sizeof(float));

      if(!IsEditAnimValid())
         return;
      if(!anim_mod)
         return;

      //PI3D_frame frm = anim_mod->FindChildFrame(frm_name);
      PI3D_frame frm = FindModelChild(anim_mod, frm_name);
      if(!frm)
         return;
                              //save undo
      SaveFrameTransformUndo(frm);
                              //apply values
      frm->SetPos(pos);
      frm->SetRot(rot);
      frm->SetScale(scale);
      SavePose2Key();
      ed->Message("Transform restored.");
      e_slct->FlashFrame(frm);
                              //notify properties, so that sheets are updated
      //e_props->Action(E_PROP_NOTIFY_MODIFY, &pair<PI3D_frame, dword>(frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE));
      e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE);
   }

//----------------------------
// Reset transform of all selected frames (which belong to animated model) to original pose. 
// If model is selected, reset is applied on all models frames.
   void ActionResetSelFrames(){

      if(!IsEditAnimValid())
         return;
      if(!anim_mod)
         return;
      if(!ed->CanModify())
         return;

      assert(e_slct);
      const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

      bool full_model(false);
      if(sel_list.size() == 1 && sel_list[0]->GetType()==FRAME_MODEL){
         if(sel_list[0] != (CPI3D_frame)anim_mod)
            return;
         full_model = true;
      }
      if(!sel_list.size())
         full_model = true;

      PI3D_frame const *frms = !full_model ? &sel_list.front() : anim_mod->GetFrames();
      dword num = !full_model ? sel_list.size() : anim_mod->NumFrames();

      ed->SetModified(true);
      for(int i = num; i--; ){
         PI3D_frame frm = frms[i];
                              //reset only frames in animated model
         if(!full_model && !IsChildOfEditedModel(frm))
            continue;

         SaveFrameTransformUndo(frm);
                              //reset to original
         ResetModelFrame(frm, E_MODIFY_FLG_POSITION|E_MODIFY_FLG_ROTATION|E_MODIFY_FLG_SCALE);
         ed->Message("Selected frames reseted.");
         e_slct->FlashFrame(frm);
      }
      SavePose2Key();
   }

//----------------------------

   dword GetEditAnimId() const{ return edit_anim_id; }

//----------------------------

   void GotoAnimKey(int id){

      S_pose_anim_edit &pa = GetEditAnim();
      int last_key = pa.CurrKey();
      int curr_key = Max(0, Min(id, (int)pa.key_list.size() - 1));
      if(curr_key != last_key){
                              //save undo
         SaveGotoKeyUndo();
         pa.SetCurrKey(curr_key);
      }
      anim_play.SetTime(anim_mod, GetEditAnim().CurrPoseKey().time);
      UpdatePlayTime(false);
      UpdateKeyGui();
      ed->Message(C_fstr("Moved to anim key %i.", id));
   }

//----------------------------

   PI3D_anim_pose CreateMirrorPose(const S_vector &pos, const S_quat &rot, float scale) const{

      PI3D_anim_pose pose_mirror = (PI3D_anim_pose)ed->GetDriver()->CreateAnimation(I3DANIM_POSE);
                              //pos
      S_vector new_pos = pos;
      new_pos.x = -new_pos.x;
      pose_mirror->SetPos(&new_pos);
                              //rot
      const S_matrix m(rot);
      S_vector up = m(1);
      S_vector dir = m(2);
      up.x = -up.x;
      dir.x = -dir.x;
      S_matrix m2;
      m2.Identity();
      m2.SetDir(dir, up);
      S_quat new_rot(m2);
      pose_mirror->SetRot(&new_rot);

                              //scale
      pose_mirror->SetPower(&scale);
      return pose_mirror;
   }

//----------------------------

   PI3D_anim_pose CreateMirrorPose(PI3D_frame frm) const{

      assert(frm);

      return CreateMirrorPose(frm->GetPos(), frm->GetRot(), frm->GetScale());

      /*PI3D_anim_pose pose_mirror = (PI3D_anim_pose)ed->GetDriver()->CreateAnimation(I3DANIM_POSE);

                              //pos
      S_vector new_pos = frm->GetPos();
      new_pos.x = -new_pos.x;
      pose_mirror->SetPos(&new_pos);

                              //rot
      const S_quat &q1 = frm->GetRot();
      const S_matrix m(q1);

      S_vector up = m(1);
      S_vector dir = m(2);
      up.x = -up.x;
      dir.x = -dir.x;
      S_matrix m2;
      m2.Identity();
      m2.SetDir(dir, up);
      S_quat new_rot(m2);
      pose_mirror->SetRot(&new_rot);

                              //scale
      float f = frm->GetScale();
      pose_mirror->SetPower(&f);

      return pose_mirror;*/
   }

//----------------------------

   void MirrorFrm(PI3D_frame frm, S_vector &pivot){

      enum{
         AXIS_X = 1,
         AXIS_Y = 2,
         AXIS_Z = 4
      };

      PC_editor_item_MouseEdit me = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      dword axis = me->GetRotationAxis();

      if(!frm || !frm->GetParent())
         return;

                              //modify frames flags
      e_modify->AddFrameFlags(frm, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION);


      const S_matrix &m_inv = frm->GetParent()->GetInvMatrix();
                              //pos
      S_vector new_pos = frm->GetWorldPos();
      if(axis&AXIS_X)
         new_pos.x = pivot.x + (pivot.x - new_pos.x);
      if(axis&AXIS_Z)
         new_pos.z = pivot.z + (pivot.z - new_pos.z);
      if(axis&AXIS_Y)
         new_pos.y = pivot.y + (pivot.y - new_pos.y);
      //...
      frm->SetPos(new_pos * m_inv);

                              //rot
#if 1
      const S_quat &q1 = frm->GetWorldRot();
      const S_matrix m(q1);

      S_vector up = m(1);
      S_vector dir = m(2);
      if(axis&AXIS_X){
         up.x = -up.x;
         dir.x = -dir.x;
      }
      if(axis&AXIS_Z){
         up.z = -up.z;
         dir.z = -dir.z;
      }
      if(axis&AXIS_Y){
         up.y = -up.y;
         dir.y = -dir.y;
      }
      S_matrix m2;
      m2.Identity();
      m2.SetDir(dir, up);
      S_quat new_rot(m2);
      frm->SetRot(new_rot * S_quat(m_inv));
#endif
   }

//----------------------------

   void MirrorFrames(const C_vector<PI3D_frame> &frms){

      if(frms.size() < 2){
         ed->Message("Multiple selection required");
         return;
      }
      S_vector pivot_all;
      pivot_all.Zero();
                           //compute center point
      for(int i=frms.size(); i--; ){
         PI3D_frame frm = frms[i];
         pivot_all += frm->GetWorldPos();
      }
      pivot_all /= (float)frms.size();

      for(int j=frms.size(); j--; ){
         PI3D_frame frm = frms[j];
         MirrorFrm(frm, pivot_all);
         e_slct->FlashFrame(frm);
      }
      ed->Message(C_fstr("Mirrored %i frames", frms.size()));
   }

//----------------------------

   void SaveMirrorUndo(PI3D_frame frm){

      const C_str &frm_name = frm->GetName();
      e_undo->Save(this, E_ANIM_MIRROR_POSE_NAMED, (void*)(const char*)frm_name, frm_name.Size()+1);
   }

//----------------------------

   void MirrorPose(PI3D_model mod){

      assert(mod);

      if(!ed->CanModify())
         return;

      ed->SetModified(true);

      SaveMirrorUndo(mod);

      PI3D_frame const *frms = mod->GetFrames();

                              //go through all all frames
      for(int i = mod->NumFrames(); i--; ){

         PI3D_frame frm = frms[i];
                              //find those which can be mirrored(starting with r_ or l_)
         C_str link = frm->GetOrigName();
         bool r_side = link.Match("r_*");
         bool l_side = link.Match("l_*");

         const S_mask_mode &ed_mask = GetEditMask();
         const S_pose_anim_edit &ed_anim = GetEditAnim();

                              //read just r side since l side is created at same time with r
         if(l_side)
            continue;
                              //if not r_side and not l_side, mirror it by self (central parts)
         if(!r_side){
                              //do not mirror frames which are not in anim
            bool is_frm_in_mask = (ed_mask.IsInMask(link) && IsFrameInAnim(link, ed_anim));
            if(is_frm_in_mask)
               continue;

            PI3D_anim_pose pose = CreateMirrorPose(frm);
            const dword mask_flgs = GetEditMask().GetEnabled(link); //mirror only non locked transforms
            S_pose_anim_edit::CopyPose2Frame(pose, frm, mask_flgs, e_props, e_modify);
            pose->Release();
            continue;
         }
                              //find opposite
         C_str str_oposite = r_side ? "l_" : "r_";
         for(dword j = 2; j < link.Size(); j++){
            str_oposite += C_fstr("%c",link[j]);
         }
         const char *link2 = (const char*)str_oposite;
         //PI3D_frame frm_oposite = mod->FindChildFrame(link2);
         PI3D_frame frm_oposite = FindModelChild(mod, link2);
         if(!frm_oposite)
            continue;

                              //both frames must be part of anim 
         bool is_frm_in_mask = (ed_mask.IsInMask(link) && IsFrameInAnim(link, ed_anim));
         bool is_oposite_in_mask = (ed_mask.IsInMask(link2) && IsFrameInAnim(link2, ed_anim));

         if(!is_frm_in_mask || !is_oposite_in_mask)
            continue;

         {
                              //current frame and opposite frame lock flags
            //S_mask_mode &ed_mask = const_cast<S_mask_mode &>(GetEditMask());
            const byte flgs_frm = ed_mask.GetEnabled(link);
            const byte flgs_oposite = ed_mask.GetEnabled(link2);

                              //put them to pose and setup their transforms each by other
            PI3D_anim_pose pose_frm = CreateMirrorPose(frm_oposite);
            PI3D_anim_pose pose_oposite = CreateMirrorPose(frm);
                                 //apply anim set to model

            S_pose_anim_edit::CopyPose2Frame(pose_frm, frm, flgs_frm, e_props, e_modify);
            S_pose_anim_edit::CopyPose2Frame(pose_oposite, frm_oposite, flgs_oposite, e_props, e_modify);
            pose_frm->Release();
            pose_oposite->Release();
         }
      }

                              //notify anim edit that want store this pose
      if(IsEditAnimValid() && mod==anim_mod){
         SavePose2Key();
      }
      ed->Message("Pose mirrored.");
   }

//----------------------------

   bool ImportAnim(S_pose_anim_edit &pa, const char *filename, PI3D_model mod_src){
                   
      C_smart_ptr<I3D_animation_set> as = ed->GetDriver()->CreateAnimationSet();
      as->Release();

      I3D_RESULT ir = as->Open(
         filename,
         0,    //flags
         NULL, //cb_proc
         NULL  //cb_context
      );
      if(I3D_FAIL(ir)){
         C_fstr msg("Can't open anim '%s'", filename);
         e_log->AddText(msg);
         return false;
      }

      C_smart_ptr<I3D_model> mod = (PI3D_model)ed->GetScene()->CreateFrame(FRAME_MODEL);
      mod->Release();

      assert(mod_src);
      mod->Duplicate(mod_src);
      mod->SetAnimation(0, as);

                              //create keys time line
      set<int> key_times;
      for(dword si = as->NumAnimations(); si--; ){
         CPI3D_animation_base ab = as->GetAnimation(si);
         if(ab->GetType()==I3DANIM_KEYFRAME){
            PI3D_keyframe_anim ka = (PI3D_keyframe_anim)ab;
            const I3D_anim_pos_bezier *pos_keys;
            const I3D_anim_quat_bezier *rot_keys;
            const I3D_anim_power *pow_keys;;
            dword num_pos, num_rot, num_pow;
            ka->GetPositionKeys(&pos_keys, &num_pos);
            ka->GetRotationKeys(&rot_keys, &num_rot);
            ka->GetPowerKeys(&pow_keys, &num_pow);
            dword ki;
            for(ki=num_pos; ki--; )
               key_times.insert(Max(0, pos_keys[ki].time));
            for(ki=num_rot; ki--; )
               key_times.insert(Max(0, rot_keys[ki].time));
            for(ki=num_pow; ki--; )
               key_times.insert(Max(0, pow_keys[ki].time));
         }
      }
      if((*key_times.begin()) != 0)
         key_times.insert(0);

      dword last_time = 0;
      for(set<int>::const_iterator it = key_times.begin(); it!=key_times.end(); it++){
         dword curr_time = (*it);
         dword tick_time = curr_time - last_time;
         mod->Tick(tick_time);
         last_time = curr_time;

         //pa.key_list.push_back(S_pose_key());
         pa.key_list.resize(pa.key_list.size()+1);
         S_pose_key &pk = pa.key_list.back();
         pk.easy_from = .0f;
         pk.easy_to = .0f;
         pk.smooth = 1.0f;
         pk.time = curr_time;
         PI3D_animation_set as2 = ed->GetDriver()->CreateAnimationSet();
         StoreFullPoseIntoSet(mod, as2);
         pk.anim_set = as2;
         as2->Release();

         e_log->AddText(C_fstr("created keys on time %i", curr_time));
      }
      return true;
   }

//----------------------------

   void ActionPickNotifyFrm(){

      if(!hWnd_anim_edit)
         return;
      if(!ed->CanModify())
         return;

                              //check if the frame is model's child
      if(!anim_mod)
         return;

      PI3D_frame frm = e_slct->GetSingleSel();
      if(!frm){
         ed->Message("Single selection required!");
         return;
      }
      for(int i=anim_mod->NumFrames(); i--; ){
         if(anim_mod->GetFrames()[i]==frm)
            break;
      }
      if(i==-1){
         ed->Message("Selected frame doesn't belong to edited model.");
         return;
      }
      SetKeyNotifyLink(frm->GetOrigName());
      UpdateNotifyLink();
      ed->SetModified(true);
      ed->Message(C_fstr("Frame '%s' assigned to note.", frm->GetOrigName()));
      e_slct->FlashFrame(frm);
   }
                                    
//----------------------------

   void FillMaskCB(HWND hwnd_lb) const{

      assert(hwnd_lb);
      SendMessage(hwnd_lb, CB_RESETCONTENT, 0, 0);
      for(dword i = 0; i < mask_modes.modes.size(); i++){
         const char *mode_name = mask_modes.GetMode(i).GetName();
         int indx = SendMessage(hwnd_lb, CB_ADDSTRING, 0, (dword)mode_name);
         SendMessage(hwnd_lb, CB_SETITEMDATA, indx, i);
      }
   }

//----------------------------
// Return selected mask id from spown dialog.
// Return index into selected mask, or -1 if cancelled.
   int SelectMaskIdDlg(HWND hwnd_parent, S_mask_mode *cust_mask){

      S_anim_mask_hlp hlp;
      hlp.ae = this;
      hlp.select_id = IsEditAnimValid() ? GetEditAnim().GetMaskId() : 0;
      hlp.cust_mask = cust_mask;
      HWND hwnd_save_focus = GetFocus();
      bool b = DialogBoxParam(GetHInstance(), "IDD_ANIM_MASK", hwnd_parent, cbMaskAnim, (LPARAM)&hlp);
      SetFocus(hwnd_save_focus);

      if(!b)
         return -1;
      return hlp.select_id;
   }

//----------------------------

   void ImportKFAnim(){

      PI3D_model mod = GetNewAnimModel(true);
      if(!mod)
         return;
      if(!ed->CanModify())
         return;

      C_str filename;               
      bool b = GetBrowsePath(ed, "Import animation", filename, "Anims",
         "Insanity 3D files (*.i3d)\0*.i3d\0" "All files\0*.*\0");
      if(!b)
         return;
                     //try to strip current working directory from fname
      {
         char cwd[260];
         _getcwd(cwd, sizeof(cwd));
         C_fstr wild_dir("%s\\*", cwd);
         C_str fname(filename);
         if(fname.Matchi(wild_dir))
            fname = &fname[wild_dir.Size()-1];
         filename = fname;
      }

      anims.push_back(S_pose_anim_edit());
      S_pose_anim_edit &pa = anims.back();
      pa.model_name = mod->GetName();

      //ImportAnim(pa, "anims\\human\\move_stay_run_f.i3d");
      bool ok = ImportAnim(pa, filename, mod);
      if(ok){

         C_str sug_name(filename);
                     //strip ext
         for(dword i = sug_name.Size(); i--; ){
            if(sug_name[i] == '.'){
               sug_name[i] = '\0';
               break;
            }
         }
                     //strip dirs
         for(dword j = sug_name.Size(); j--;){
            if(sug_name[j] == '\\'){
               sug_name = &sug_name[j+1];
               break;
            }
         }
         C_str anim_name;
         if(SelectAnimName((HWND)ed->GetIGraph()->GetHWND(), sug_name, anim_name)){
                        //take mask ID from currently edited anim (if any)
                        //otherwise make custom
            int mask_id = 0;
            const S_mask_mode *cust_mask = NULL;
            if(IsEditAnimValid()){
               mask_id = anims[edit_anim_id].GetMaskId();
               if(!mask_id)
                  cust_mask = &anims[edit_anim_id].GetCustomMask();
            }
                              //create default custom mask if not loaded
            if((!mask_id) && (cust_mask == NULL)){
               S_mask_mode *tmp_mask = new S_mask_mode;
               assert(mod);
               tmp_mask->CreateCustom(mod);
               cust_mask = tmp_mask;
            }
            SetAnimMask(pa, mask_id, cust_mask);
                        //set pos/rot from this model
            pa.edit_pos = mod->GetPos();
            pa.edit_rot = mod->GetRot();

                        //finaly add anim and make set editable
            pa.name = (const char*)anim_name;
            SetEditAnim(anims.size()-1);
            ed->SetModified(true);
            ed->Message("Animation successfully imported");
         }else{
            anims.pop_back();
         }
      }
   }

//----------------------------

   void ImportSceneAnim(){

      PI3D_model mod = GetNewAnimModel(true);
      if(!mod)
         return;
      if(!ed->CanModify())
         return;
      if(!last_bin_import_dir.Size())
         last_bin_import_dir = "Missions";

      C_str filename;
      if(!GetBrowsePath(ed, "Choose mission file", filename, last_bin_import_dir,
         "Mission binary files (*.bin)\0*.bin\0" "All files\0*.*\0"))
         return;

      last_bin_import_dir = filename;
      for(dword i=last_bin_import_dir.Size(); i--; ){
         if(last_bin_import_dir[i]=='\\'){
            last_bin_import_dir[i] = 0;
            last_bin_import_dir = (const char*)last_bin_import_dir;
            break;
         }
      }

      HWND hwnd = (HWND)ed->GetIGraph()->GetHWND();
                        //parse the file, collect anim names
      C_chunk ck;
      if(!ck.ROpen(filename) || (++ck != CT_BASECHUNK)){
         MessageBox(hwnd, C_fstr("Failed to open file: '%s'", (const char*)filename), "Import error", MB_OK);
         return;
      }

                        //take mask ID from currently edited anim (if any)
      int mask_id = -1;
      if(IsEditAnimValid()){
         mask_id = anims[edit_anim_id].GetMaskId();
      }

      C_vector<S_pose_anim_edit> import_anims;

      while(ck){
         switch(++ck){
         case CT_POSE_ANIMS:
            {
                        //import all anims into temporary list
               LoadAnims(ck, import_anims, NULL, mod, mask_id);
               --ck;
            }
            break;
         default:
            --ck;
         }
      }
      if(!import_anims.size()){
         MessageBox(hwnd, "File contains no animations!", "Import warning", MB_OK);
         return;
      }
                        //sort anims by name
      struct S_hlp{
         static bool cbLess(const S_pose_anim_edit &a1, const S_pose_anim_edit &a2){
            return (a1.name < a2.name);
         }
      };
      sort(import_anims.begin(), import_anims.end(), S_hlp::cbLess);
                        //let user choose anim to import
      C_vector<char> choose_buf;
      for(i=0; i<import_anims.size(); i++){
         const C_str &name = import_anims[i].name;
         choose_buf.insert(choose_buf.end(), (const char*)name, (const char*)name+name.Size()+1);
      }
      choose_buf.push_back(0);

      int indx = ChooseItemFromList(ed->GetIGraph(), NULL, "Choose animation to import", &choose_buf.front());
      if(indx==-1)
         return;

      S_pose_anim_edit &pa = import_anims[indx];
                        //check if anim with such name already exists
      while(true){
         for(i=anims.size(); i--; ){
            if(anims[i].name==pa.name)
               break;
         }
         if(i==-1)
            break;
         if(!SelectName(ed->GetIGraph(), NULL, "Enter animation name", pa.name))
            return;
      }
                        //set pos/rot from this model
      pa.edit_pos = mod->GetPos();
      pa.edit_rot = mod->GetRot();

                        //add the anim into ours anims
      anims.push_back(pa);
      ed->SetModified();
                        //choose to edit it
      SetEditAnim(anims.size()-1);
   }

//----------------------------

   virtual dword Action(int id, void *context = NULL){

      switch(id){
      case E_ANIM_FOCUS_FRAMES:
         if(hWnd_anim_frames){
            ed->Message("Activating anim frames");
            e_props->ShowSheet(hWnd_anim_frames);
            SetFocus(GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST));
         }
         break;

      case E_ANIM_FOCUS_ANIM_EDIT:
         if(hWnd_anim_edit){
            ed->Message("Activating anim editor");
            SetActiveWindow(hWnd_anim_edit);
         }
         break;

      case E_ANIM_SET_KEY_TIME_DIRECT:
         ActionSetKeyTimeDirect();
         break;

      case E_ANIM_CLOSE_EDIT:
         SetEditAnim(-1);
         break;

      case E_ANIM_SELECT_ANIM:
         {
                              //spawn select dialog for anims in scene
            SelectEditAnim((HWND)ed->GetIGraph()->GetHWND());
         }
         break;

      case E_ANIM_TOGGLE_MODEL_TRANSP:
         {
            model_transparency = !model_transparency;
            SetAnimFramesTransparency(model_transparency);
            ed->CheckMenu(this, id, model_transparency);
            ed->Message(C_fstr("Transparency %s", model_transparency ? "on" : "off"));
         }
         break;

      case E_ANIM_LOOP_MODE:
         {
            loop_mode = !loop_mode;
            ed->CheckMenu(this, id, loop_mode);
            PlayModeToggle();
            PlayModeToggle();
            ed->Message(C_fstr("Loop mode %s", loop_mode ? "on" : "off"));
         }
         break;

      case E_ANIM_LOCK_MODEL:
         if(anim_mod && lock_anim_model)
            e_modify->RemoveLockFlags(anim_mod, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE);
         lock_anim_model = !lock_anim_model;
         if(anim_mod && lock_anim_model)
            e_modify->AddLockFlags(anim_mod, E_MODIFY_FLG_POSITION | E_MODIFY_FLG_ROTATION | E_MODIFY_FLG_SCALE);
         ed->CheckMenu(this, id, lock_anim_model);
         ed->Message(C_fstr("Lock anim model %s", lock_anim_model ? "on" : "off"));
         break;

      case E_ANIM_TOGGLE_PATHS:
         {
            render_paths = !render_paths;
            ed->CheckMenu(this, id, render_paths);
         }
         break;

      case E_ANIMEDIT_NOTIFY_MODIFY:
                              //do not allow modify key when not in edit mode
         if(CanEditCurrKey()){
            const pair<PI3D_frame, dword> &p = *(pair<PI3D_frame, dword>*)context;
            if(!IsEditAnimValid())
               break;
            if(anims.size()){
               if(IsChildOfEditedModel(p.first)){
                              //do not save key now, notify is send in undefined order
                              //(currently BEFORE farame's transfor is actualy updated)
                              //radther set this flag and save pose in next tick.
                  pose_key_dirty = true;
               }
            }
         }
         break;

      case E_ANIM_CLEAR_SELECTION:
      case E_ANIM_ALL_SELECTION:
      case E_ANIM_SEL_SELECTION:
      case E_ANIM_COPY_SELECTION:
         {
            if(!e_slct)
               break;
            switch(id){
            case E_ANIM_CLEAR_SELECTION:
               {
                  SelectAnimFrames(0);
               }
               break;
            case E_ANIM_ALL_SELECTION:
               {
                  SelectAnimFrames(1);
               }
               break;
            case E_ANIM_SEL_SELECTION:
               {
                  SelectAnimFrames(2);
               }
               break;
            case E_ANIM_COPY_SELECTION:
               {
                              //let user select which anim we'll copy from
                  int ai = SelectEditAnim(hWnd_anim_edit, true);
                  if(ai==-1)
                     return 0;
                  SelectAnimFrames(0, &anims[ai]);
                  FillFrameList2();
               }
               break;
            default: assert(0);
            }
            UpdateCheckList();
            UpdatePRSWColumns();
            e_slct->Clear();
         }
         break;

      case E_ANIM_PICK_MASK:
         if(hWnd_anim_edit){
            SelectMaskMode(hWnd_anim_edit);
            UpdateFrameSelect();
         }
         break;
      case E_ANIM_ADD_KEY:
         if(mode!=MODE_PLAY){
            CreateNewKey();
            UpdateKeyGui();
            ed->Message("Key added.");
         }
         break;

      case E_ANIM_DEL_KEY:
         if(CanEditCurrKey()){
            int id_del = GetEditAnim().CurrKey();
            if(DeleteKey(id_del) && (id_del == 0)){
                                    //if we deleted first key, move time so current first key time will be zero
               InsertKeyTime(0, -GetEditAnim().key_list[0].time);
            }
            UpdateKeyGui();
            ed->Message("Key deleted.");
         }
         break;
      case E_ANIM_DEL_KEY_ID:
         {
            S_key_del &ukd = *(S_key_del*)context;
            DeleteKey(ukd.id_to_delete);
            GetEditAnim().SetCurrKey(ukd.id_after_del);
            UpdateAnimation();
            UpdateKeyGui();
            ed->Message("Key deleted (undo).");
         }
         break;
      case E_ANIM_ADD_KEY_ID:
         {
            S_key_add &uka = *(S_key_add *)context;
            S_pose_key pk;
            SaveDelKeyUndo(GetEditAnim().CurrKey(), uka.id_to_create);
            CreateKeyFromUndo(pk, context);
            GetEditAnim().AddKey(uka.id_to_create, pk);
            UpdateKeyGui();
            ed->Message("Key added (undo).");
         }
         break;
      case E_ANIM_PREV_KEY:
      case E_ANIM_PREV_KEY2:
      case E_ANIM_NEXT_KEY:
      case E_ANIM_NEXT_KEY2:
      case E_ANIM_FIRST_KEY:
      case E_ANIM_LAST_KEY:
         if(mode==MODE_PLAY)
            Action(E_ANIM_PLAY_STOP);
         {
            int new_id = GetEditAnim().CurrKey();
            dword num_keys = GetEditAnim().key_list.size();
            switch(id){
            case E_ANIM_PREV_KEY2:
               (new_id += num_keys-1) %= num_keys; 
                              //flow...
            case E_ANIM_PREV_KEY:
               (new_id += num_keys-1) %= num_keys;
               break;

            case E_ANIM_NEXT_KEY2:
               ++new_id %= num_keys;
                              //flow...
            case E_ANIM_NEXT_KEY:
               ++new_id %= num_keys;
               break;

            case E_ANIM_FIRST_KEY: new_id = 0; break;
            case E_ANIM_LAST_KEY: new_id = num_keys - 1; break;
            }
            GotoAnimKey(new_id);
            //curr_playtime = GetEditAnim().CurrPoseKey().time;
         }
         break;
      case E_ANIM_GOTO_KEY:
         GotoAnimKey(*(int*)context);
         break;
      case E_ANIM_RENAME_ANIM:
         RenameEditAnim((const char*)context);
         UpdateAnimName();
         break;
      case E_ANIM_SET_KEY_TIME:
         SetKeyTime(*(dword*)context);
         UpdateKeyTime();
         break;
      case E_ANIM_SET_KEY_SMOOTH:
         SetKeySmooth(*(float*)context);
         UpdateSmooth();
         break;
      case E_ANIM_SET_KEY_EASY_FROM:
         SetKeyEasyFrom(*(float*)context);
         UpdateEasyFrom();
         break;
      case E_ANIM_SET_KEY_EASY_TO:
         SetKeyEasyTo(*(float*)context);
         UpdateEasyTo();
         break;
      case E_ANIM_SET_KEY_NOTE:
         SetKeyNote((const char*)context);
         UpdateNote();
         break;
      case E_ANIM_SET_KEY_NOTIFY_LINK:
         SetKeyNotifyLink((const char*)context);
         UpdateNotifyLink();
         break;
      case E_ANIM_SET_KEY_ANIM_SET:
         {
            PI3D_animation_set as = CreateAnimSet(context);
            SetKeyAnimSet(as);
            as->Release();
            UpdateModel();
            ed->Message("Undo paste pose.");
         }
         break;
      case E_ANIM_SET_EDIT_ANIM:
         SetEditAnim(*(dword*)context);
         break;
      case E_ANIM_ADD_FRAME_KEYS:
         {
            const S_frame_keys &fk = *(const S_frame_keys *) context;
            AddAnimFrame(fk.frm_name, context);
            UpdateCheckList();
            UpdatePRSWColumns();
         }
         break;
      case E_ANIM_DEL_FRAME_KEYS:
         DelAnimFrame((const char *)context);
         UpdateCheckList();
         UpdatePRSWColumns();
         break;
      case E_ANIM_COPY_POSE:
         if(mode!=MODE_PLAY){
            PI3D_frame frm = e_slct->GetSingleSel();
            PI3D_model mod;
            if(frm && frm->GetType()==FRAME_MODEL)
               mod = I3DCAST_MODEL(frm);
            else{
               if(!IsEditAnimValid())
                  break;
               mod = anim_mod;
               if(!mod){
                  ed->Message("Anim has no model assigned.");
                  break;
               }
            }
            CopyPoseToClipbord(mod);
         }
         break;
      case E_ANIM_PASTE_POSE:
         if(CanEditCurrKey()){
            PastePoseFromClipboard();
         }
         break;
      case E_ANIM_PASTE_SELECTION:
         if(CanEditCurrKey()){
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            PasteClipboardSelection(&sel_list.front(), sel_list.size(), false);
         }
         break;
      case E_ANIM_PASTE_SELECTION_ALL_KEYS:
         if(CanEditCurrKey()){
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
            PasteClipboardSelection(&sel_list.front(), sel_list.size(), true);
         }
         break;
      case E_ANIM_PASTE_MIRROR:
         if(CanEditCurrKey()){
            PastePoseFromClipboardMirrored();
         }
         break;
      case E_ANIM_PICK_NOTIFY_FRM:
         if(CanEditCurrKey()){
            ActionPickNotifyFrm();
         }
         break;
      case E_ANIM_NOTE_CLEAR:
         if(CanEditCurrKey()){
            if(!ed->CanModify())
               break;
            SetKeyNote("", true);
            UpdateNote();
            SetKeyNotifyLink("");
            UpdateNotifyLink();
         }
         break;

      case E_ANIM_PLAY_STOP:
         if(mode!=MODE_PLAY)
            break;
                              //flow...
      case E_ANIM_PLAY_TOGGLE:
         {
            PlayModeToggle();
            ed->Message((mode == MODE_PLAY) ? "Playing" : "Stoped");
         }
         break;
      case E_ANIM_ADD_FRAME:
      case E_ANIM_DEL_FRAME:
         {
            assert(context);
            PI3D_frame frm = (PI3D_frame)context;
            switch(id){
            case E_ANIM_ADD_FRAME: 
               return AddAnimFrame(frm->GetOrigName()); 
               break;
            case E_ANIM_DEL_FRAME: 
               return DelAnimFrame(frm->GetOrigName()); 
               break;
            }
         }
         break;
                  
      case E_ANIMEDIT_NOTIFY_SELECTION:
         {
            MarkSelectedFrame();
            UpdateAssingModelButton();
         }
         break;
      case E_ANIM_CHANGE_MODEL:
         ChangeAnimModel();
         break;

      case E_ANIM_MIRROR_POSE:
         if(CanEditCurrKey()){
            PI3D_frame frm = e_slct->GetSingleSel();
            if(frm && frm->GetType()==FRAME_MODEL)
               MirrorPose(I3DCAST_MODEL(frm));
         }
         break;

      case E_ANIM_MIRROR_POSE_NAMED:
         {
            const char *name = (const char*)context;
            PI3D_frame frm = ed->GetScene()->FindFrame(name, ENUMF_MODEL);
            if(frm){
               assert(frm->GetType() == FRAME_MODEL);
               MirrorPose(I3DCAST_MODEL(frm));
            }
         }
         break;

      case E_ANIM_MIRROR_FRAMES:
         MirrorFrames(e_slct->GetCurSel());
         break;

      case E_ANIM_SET_WEIGHT:
         if(CanEditCurrKey()){
            ActionSetWeight();
         }
         break;

      case E_ANIM_UNDO_WEIGHT:
         {
            S_set_weight &u_weight = *(S_set_weight *)context;
            SetWeight(u_weight.anim_id, u_weight.link, u_weight.weight);

            HWND hwnd_lv = GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST);
            assert(hwnd_lv);
                              //update affected field
            LVITEM li;
            memset(&li, 0, sizeof(li));
            li.mask = LVIF_PARAM;
            for(dword i = SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0); i--; ){
               li.iItem = i;
               SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM) &li);
                                       //this is the one (and only) selected
               PI3D_frame frm = (PI3D_frame)li.lParam;
               assert(frm);
               const char *link = frm->GetOrigName();
               if(!stricmp(link, u_weight.link)){
                  li.mask = LVIF_TEXT;
                  li.iSubItem = 4;
                  C_fstr txt("%.1f", u_weight.weight);
                  li.pszText = (char*)(const char*)txt;
                  SendMessage(hwnd_lv, LVM_SETITEMTEXT, i, (LPARAM)&li);
                  UpdateAnimation();
                  break;
               }
            }
         }
         break;

      case E_ANIM_IMPORT_KEYFRAME_ANIM:
         ImportKFAnim();
         break;

      case E_ANIM_IMPORT_MISSION_ANIM:
         ImportSceneAnim();
         break;

      case E_ANIM_SELECT_SCALE_TIME:
         ActionScaleAnimTime();
         break;
      case E_ANIM_KEYS_TIMES_UNDO:
         UndoKeysTimes(context);
         break;
      case E_ANIM_RESET_SEL_FRAMES:
         ActionResetSelFrames();
         break;
      case E_ANIM_UNDO_FRM_TRANSFORM:
         UndoFrameTransform(context);
         break;
      case E_ANIM_INSERT_TIME:
         ActionInsertKeyTime();
         break;
      case E_ANIM_UNDO_INSERT_TIME:
         UndoInsertTime(context);
         break;
      case E_ANIM_SMOOTH_ANIM_TIME:
         SmoothAnimTime();
         break;
      case E_ANIM_UNDO_FRM_TRANFORM_KEYS:
         UndoFrameTransformKeys(context);
         UpdateModel();
         ed->Message("Undone paste selection keys.");
         break;
#if 0
//      case E_ANIM_PLAY_TIME_INC:
//      case E_ANIM_PLAY_TIME_DEC:
//         {
//            const int STEP_TIME = 10;
//            curr_playtime += (id == E_ANIM_PLAY_TIME_DEC) ? -STEP_TIME : STEP_TIME;
//            anim_play.SetTime(anim_mod, curr_playtime);
//            anim_mod->Tick(0);
//            UpdatePlayTime(true);
//         }
//         break;
#endif

      }
      return 0; 
   }

//----------------------------
// Get average sum of position changes of all frames in anim sets.
   float GetAveragePosDelta(CPI3D_animation_set as1, CPI3D_animation_set as2) const{

      float pos_delta_sum(.0f);
      float num_deltas(0);
      assert(as1 && as2);
      assert(as1->NumAnimations() == as2->NumAnimations());
      if(as1->NumAnimations() != as2->NumAnimations())
         return .0f;
      for(int i = as1->NumAnimations(); i--; ){
         CPI3D_animation_base ab1 = ((PI3D_animation_set)as1)->GetAnimation(i);
         const C_str &link1 = as1->GetAnimLink(i);
         int id2 = FindAnimId(as2, link1);
         if(id2 == -1)
            continue;
#ifdef _DEBUG
         const C_str &link2 = as2->GetAnimLink(id2);
         assert(link1==link2);
#endif
         CPI3D_animation_base ab2 = ((PI3D_animation_set)as2)->GetAnimation(id2);

         if(ab1->GetType() != I3DANIM_POSE || ab2->GetType() != I3DANIM_POSE)
            continue;

         PI3D_anim_pose pose1 = (PI3D_anim_pose)ab1;
         PI3D_anim_pose pose2 = (PI3D_anim_pose)ab2;

         const S_vector *pos1 = pose1->GetPos();
         const S_vector *pos2 = pose2->GetPos();
         if(!pos1 || !pos2)
            continue;
         float pos_dlt = (*pos1 - *pos2).Magnitude();
         pos_delta_sum += pos_dlt;
         num_deltas++;
      }
      float pos_average_delta = num_deltas ? pos_delta_sum/ num_deltas : .0f;
      return pos_average_delta;
   }

//----------------------------

   void SmoothAnimTime(){

      if(!ed->CanModify())
         return;

      if(!IsEditAnimValid())
         return;
      float pos_delta_sum = .0f;
      int time_sum = 0;
      S_pose_anim_edit &pa = GetEditAnim();
      const S_pose_key *prev_key = NULL;
      for(dword i = 0 ; i < pa.key_list.size(); i++ ){
         const S_pose_key &pk = pa.key_list[i];
         if(prev_key){
            float pos_d = GetAveragePosDelta(prev_key->anim_set, pk.anim_set);
            pos_delta_sum += pos_d;
            time_sum += (pk.time - prev_key->time);
         }
         prev_key = &pk;
      }
      if(!time_sum)
         return;
      float average_speed = pos_delta_sum / time_sum;

                              //if no visible chanege in pos, return
      if(average_speed < .0001f)
         return;

      prev_key = NULL;
      int end_time = pa.GetEndTime();
      assert(end_time <= time_sum);

      ed->SetModified(true);

      SaveKeysTimesUndo();
      for(dword j = 0; j < pa.key_list.size(); j++ ){
         S_pose_key &pk = pa.key_list[j];
         if(prev_key){

            float pos_d = GetAveragePosDelta(prev_key->anim_set, pk.anim_set);
            assert(!IsAbsMrgZero(average_speed));
            pk.time = int(pos_d / average_speed + .5f);
            pk.time += prev_key->time;
            pk.time = Min(Max(prev_key->time+1, pk.time), end_time);
         }
         prev_key = &pk;
      }
      UpdateKeyTime();
      UpdateTimeSlider();
      UpdatePlayTimeCaption();
      UpdateAnimation();
      ed->Message("Keys times adjusted to smooth anim velocity.");
      e_slct->FlashFrame(anim_mod);
   }

//----------------------------

   void SaveInsertTimeUndo(dword key_id, int time){

      C_vector<byte> undo_buf(sizeof(S_ins_time), 0);
      S_ins_time &it = (S_ins_time &)undo_buf.front();
      it.key_id = key_id;
      it.time = time;

      e_undo->Save(this, E_ANIM_UNDO_INSERT_TIME, &undo_buf.front(), undo_buf.size());
   }

//----------------------------

   void UndoInsertTime(void *buf){

      assert(buf);
      S_ins_time &it = *(S_ins_time *)buf;
      InsertKeyTime(it.key_id, it.time);
   }

//----------------------------

   void InsertKeyTime(dword id_key_from, int t){

      assert(IsEditAnimValid());

      S_pose_anim_edit &pa = GetEditAnim();

      SaveInsertTimeUndo(id_key_from, -t);

      for(dword i = id_key_from; i < pa.key_list.size(); i++){

         S_pose_key &pk = pa.key_list[i];
         pk.time = Max(0, pk.time + t);
      }
      UpdateKeyTime();
      UpdateTimeSlider();
      UpdatePlayTimeCaption();
      UpdateAnimation();
      ed->Message(C_fstr("Inserted time %i ms (key %i - end)", t, id_key_from));
   }

//----------------------------

   void ActionInsertKeyTime(){

      if(!ed->CanModify())
         return;
      assert(IsEditAnimValid());
      if(!IsEditAnimValid())
         return;
      S_pose_anim_edit &pa = GetEditAnim();
                              //first key has always zero time, do not allow modify
      if(!pa.CurrKey()){
         ed->Message("Can't insert time for first key.");
         return;
      }
      int insert_max = Max(0, (int)(MAX_ANIM_TIME - pa.GetEndTime()));
      int prew_key_time = (pa.CurrKey()) ? pa.key_list[pa.CurrKey() - 1].time : 0;
      int ins_min = Min((prew_key_time+1) - pa.key_list[pa.CurrKey()].time, 0);

      int t = 0;
      C_fstr cpt("Enter time for insert(%i , %i):", ins_min, insert_max);
      if(SelectNewTime(t, cpt, ins_min, insert_max)){
         ed->SetModified(true);

         dword id_from = GetEditAnim().CurrKey();
         InsertKeyTime(id_from, t);
      }
   }

//----------------------------

   void SetWeight(dword pose_anim_id, const char *link, float weight){

      if(pose_anim_id > anims.size())
         return;

      S_pose_anim &pa = anims[pose_anim_id];
      float curr_w = pa.GetWeight(link);
      if(curr_w != weight){
         SaveWeightUndo(pose_anim_id, link, curr_w);
         pa.SetWeight(link, weight);
      }
   }

//----------------------------

   struct S_weight_get{
      int pos_x, pos_y;
      float *weight;
   };

   static BOOL CALLBACK dlgGetWeight(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
      switch(uMsg){
      case WM_INITDIALOG:
         {
            S_weight_get *wg = (S_weight_get*)lParam;
            float w = *wg->weight;
            SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)wg->weight);
            SetDlgItemText(hwnd, IDC_EDIT_WEIGHT, FloatStrip(C_fstr("%f", w)));
            SendDlgItemMessage(hwnd, IDC_SLIDER1, TBM_SETRANGE, false, MAKELONG(0, 10));
            SendDlgItemMessage(hwnd, IDC_SLIDER1, TBM_SETPAGESIZE, 0, 1);
            SendDlgItemMessage(hwnd, IDC_SLIDER1, TBM_SETPOS, true, (LPARAM)(w*10.0f));
            SetWindowPos(hwnd, NULL, wg->pos_x, wg->pos_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
         }
         return 1;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDOK: EndDialog(hwnd, true); break;
         case IDCANCEL: EndDialog(hwnd, false); break;
         case IDC_EDIT_WEIGHT:
            {
               char buf[256];
               if(GetDlgItemText(hwnd, IDC_EDIT_WEIGHT, buf, sizeof(buf))){
                  float &w = *(float*)GetWindowLong(hwnd, GWL_USERDATA);
                  sscanf(buf, "%f", &w);
               }
            }
            break;
         }
         break;

      case WM_HSCROLL:
         {
            int i = SendDlgItemMessage(hwnd, IDC_SLIDER1, TBM_GETPOS, 0, 0);
            SetDlgItemText(hwnd, IDC_EDIT_WEIGHT, FloatStrip(C_fstr("%f", float(i)*.1f)));
         }
         break;
      }
      return 0;
   }

//----------------------------

   void ActionSetWeight(){

      if(!ed->CanModify())
         return;

      assert(hWnd_anim_frames);
      HWND hwnd_lv = GetDlgItem(hWnd_anim_frames, IDC_ANIM_FRM_LIST);

      if(SendMessage(hwnd_lv, LVM_GETSELECTEDCOUNT, 0, 0)==0){
         ed->Message("Please select item(s)");
         return;
      }

      dword spc = HIWORD(SendMessage(hwnd_lv, LVM_GETITEMSPACING, true, 0));
      dword top = SendMessage(hwnd_lv, LVM_GETTOPINDEX, 0, 0);
      S_pose_anim &pa = anims[edit_anim_id];

                              //find selected link, set value for it
      LVITEM li;
      memset(&li, 0, sizeof(li));
      li.mask = LVIF_PARAM | LVIF_STATE;
      li.stateMask = LVIS_SELECTED;
      dword num_items = SendMessage(hwnd_lv, LVM_GETITEMCOUNT, 0, 0);
      for(int i = num_items; i--; ){
         li.iItem = i;
         SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM) &li);
         if(!(li.state&LVIS_SELECTED))
            continue;

         e_props->ShowSheet(hWnd_anim_frames);
                              //this is the one (and only) selected
         PI3D_frame frm = (PI3D_frame)li.lParam;
         assert(frm);
         const char *link = frm->GetOrigName();
         float weight = pa.GetWeight(link);

                              //let user select new weight
         S_weight_get wg = {
            0, 0, &weight
         };
         {
            RECT rc;
            GetWindowRect(hwnd_lv, &rc);
            wg.pos_x = rc.left;
            wg.pos_y = rc.top;
         }
                              //terermine window's position
         wg.pos_y += spc * (i - top + 1) + 5;

         bool selected = DialogBoxParam(GetHInstance(), "IDD_GET_WEIGHT", hWnd_anim_frames, dlgGetWeight, (LPARAM)&wg);
         if(!selected)
            break;

         C_fstr txt("%.1f", weight);
                              //selection succeeded, store

         for(dword i = num_items; i--; ){
            li.iItem = i;
            li.iSubItem = 0;
            li.mask = LVIF_PARAM | LVIF_STATE;
            SendMessage(hwnd_lv, LVM_GETITEM, 0, (LPARAM) &li);
            if(!(li.state&LVIS_SELECTED))
               continue;

            PI3D_frame frm = (PI3D_frame)li.lParam;
            assert(frm);
            const char *link = frm->GetOrigName();

            SetWeight(edit_anim_id, link, weight);

                              //re-init text
            li.mask = LVIF_TEXT;
            li.iSubItem = 4;
            li.pszText = (char*) (const char*)txt;
            SendMessage(hwnd_lv, LVM_SETITEMTEXT, i, (LPARAM)&li);
         }
         ed->SetModified(true);
         assert(IsEditAnimValid());

         UpdateAnimation();
         break;
      }
   }

//----------------------------

   bool SelectNewTime(int &t, const char *caption, const int MIN_TIME, const int MAX_TIME = MAX_ANIM_TIME){

      if(!hWnd_anim_edit)
         return false;

      assert(IsEditAnimValid());
      int new_time(t);
      C_fstr time_str("%i", t);

      while(true){
         bool select = SelectName(ed->GetIGraph(), hWnd_anim_edit, caption, time_str);
         if(!select)
            return false;
         int scan = sscanf(time_str, "%i", &new_time);
         if(scan != 1)
            continue;
                     //check boundary
         if(new_time > MAX_TIME){
            time_str = C_fstr("%i", MAX_TIME);
            continue;
         }
         if(new_time < MIN_TIME){
            time_str = C_fstr("%i", MIN_TIME);
            continue;
         }
         break;
      }
      t = new_time;
      assert(t <= MAX_TIME && t >= MIN_TIME);
      return true;
   }

//----------------------------

   void SaveKeysTimesUndo(){

      assert(IsEditAnimValid());
      const S_pose_anim_edit &pa = GetEditAnim();

      C_undo_buf ub;
      ub.InitW();
      dword num_keys = pa.key_list.size();
      ub.WriteMem(&num_keys, sizeof(dword));
      for(dword i = 0; i < num_keys; i++ ){
         const S_pose_key &pk = pa.key_list[i];
         ub.WriteMem(&pk.time, sizeof(int));
      }
      e_undo->Save(this, E_ANIM_KEYS_TIMES_UNDO, ub.Begin(), ub.Size());
   }

//----------------------------

   void UndoKeysTimes(void *buf){

      assert(IsEditAnimValid());
      S_pose_anim_edit &pa = GetEditAnim();

      C_undo_buf ub;
      ub.InitR(buf);
      dword num_keys;
      ub.ReadMem(&num_keys, sizeof(dword));
      assert(num_keys == pa.key_list.size());
      SaveKeysTimesUndo();
      for(dword i = 0; i < num_keys; i++ ){
         S_pose_key &pk = pa.key_list[i];
         ub.ReadMem(&pk.time, sizeof(int));
      }
      UpdateKeyTime();
      UpdateTimeSlider();
      UpdatePlayTimeCaption();
      UpdateAnimation();
      ed->Message("Keys times restored.");
      e_slct->FlashFrame(anim_mod);
   }

//----------------------------

   void ActionScaleAnimTime(){

      if(!ed->CanModify())
         return;
      assert(IsEditAnimValid());
      int old_time = GetEditAnim().GetEndTime();
      int t = old_time;
      if(!SelectNewTime(t, "Enter new anim time:", 0))
         return;

      ed->SetModified(true);

      SaveKeysTimesUndo();

      if(!old_time)
         return;
      float f = (float)t/old_time;
      S_pose_anim_edit &pa = GetEditAnim();
      for (dword i = 0; i < pa.key_list.size(); i++){

         S_pose_key &pk = pa.key_list[i];
                                 //check if new value is greater than prev
         int prew_time = (i) ? pa.key_list[i-1].time : -1;
         pk.time = FloatToInt((float)pk.time * f);
                                 //don't allow scale it to same time
         if(prew_time >= pk.time)
            pk.time = prew_time + 1;
      }

      UpdateKeyTime();
      UpdateTimeSlider();
      UpdatePlayTimeCaption();

      UpdateAnimation();
      ed->Message("Anim time scaled.");
      e_slct->FlashFrame(anim_mod);
   }

//----------------------------

   void OnNotify(PI3D_frame frm, const I3D_note_callback &nc){

      C_str msg = C_fstr("Notify '%s' on frame '%s', time %i",
         nc.note, frm ? (const char*)frm->GetName() : "(NULL)", nc.note_time);

      S_text_create ct;
      ct.tp = (const char*)msg;
      ct.x = .02f;
      ct.y = .73f;
      ct.color = 0xffffffff;
      ct.size = .024f;

      PC_poly_text texts = ed->GetTexts();
      notify_text = texts->CreateText(ct);
      notify_text->Release();
      notify_text_count = 500;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(pose_key_dirty){
         //assert(mode != MODE_PLAY);
         if(CanEditCurrKey()){
            SavePose2Key();
         }
         pose_key_dirty = false;
      }

      if(mode == MODE_PLAY){
                              //scale time by timescale plugin
         float time_scale = e_timescale->GetScale();
         static float time_rest;
         float scaled_time = (float)time * time_scale + time_rest;
         time_rest = (float)fmod(scaled_time, 1.0f);
         time = FloatToInt((float)floor(scaled_time));
         anim_play.Tick(anim_mod, time, loop_mode, cbAnimPlay, this);
         UpdatePlayTime(true);
      }

      if(notify_text){
         if((notify_text_count -= time) <= 0)
            notify_text = NULL;
      }

      if(IsActive() && model_transparency){
         DrawAnimSkelet();
      }
   }

//----------------------------

   virtual void Render(){

      if(IsEditAnimValid() && render_paths){
         const S_pose_anim_edit &anim = anims[edit_anim_id];
         PI3D_driver drv = ed->GetDriver();
         PI3D_animation_set as = drv->CreateAnimationSet();
         if(anim.ConvertToAnimSet(as, drv)){
            const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();

            PI3D_scene scn = ed->GetScene();
            PI3D_model mod = I3DCAST_MODEL(scn->CreateFrame(FRAME_MODEL));
            mod->Duplicate(anim_mod);
            mod->SetAnimation(0, as);

            const int TIME_SLICE = 40;
            dword end_time = as->GetTime();
            dword num_samples = (end_time / TIME_SLICE) + 1;
            if(end_time%TIME_SLICE) ++num_samples;

            dword num_frames = mod->NumFrames();
            PI3D_frame const *frms = mod->GetFrames();

            C_buffer<dword> color(num_frames, 0);
            C_buffer<C_buffer<S_vector> > frm_pos;
            frm_pos.assign(num_frames);
            for(dword i=num_frames; i--; ){
               PI3D_frame frm = frms[i];
               if(anim.IsInFramesList(frm->GetOrigName())){
                  PI3D_frame prnt = frm->GetParent();
                  if(prnt && anim.IsInFramesList(prnt->GetOrigName())){
                     frm_pos[i].assign(num_samples);
                     for(int j=sel_list.size(); j--; ){
                        //if(!strcmp(sel_list[j]->GetName(), frm->GetName()))
                        if(sel_list[j]->GetName()==frm->GetName())
                           break;
                     }
                     color[i] = (j!=-1) ? 0xffffffff : 0x60ffffff;
                  }
               }
            }

            int last_tm = 0;
            for(dword tm = 0, si = 0; ; tm = Min(end_time, tm + TIME_SLICE), si++){
               assert(si<num_samples);
               int tick_time = tm - last_tm;
               last_tm = tm;
               mod->Tick(tick_time);
               for(dword i=num_frames; i--; ){
                  if(!color[i])
                     continue;
                  PI3D_frame frm = frms[i];
                  const S_vector &pos = frm->GetWorldPos();
                  frm_pos[i][si] = pos;
               }
               if(tm==end_time)
                  break;
            }
            mod->Release();

            C_vector<word> index_buffer((num_samples-1) * 2);
            for(i=num_samples-1; i--; ){
               index_buffer[i*2] = (word)i;
               index_buffer[i*2+1] = (word)(i+1);
            }
                              //render positions now
            for(i=num_frames; i--; ){
               if(!color[i])
                  continue;
               scn->DrawLines(&frm_pos[i].front(), frm_pos[i].size(), &index_buffer.front(), index_buffer.size(), color[i]);
            }
         }
         as->Release();
      }
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){

      byte version;
      ck.Read(&version, sizeof(byte));
      if(version != PLG_STATE_VERSION) 
         return false;
      ck.Read(&model_transparency, sizeof(bool));
      ck.Read(&render_paths, sizeof(bool));
      ck.Read(&loop_mode, sizeof(bool));
      ck.Read(&lock_anim_model, sizeof(bool));
                              //window pos
      ck.Read(&anim_edit_wnd_pos.valid, sizeof(anim_edit_wnd_pos.valid));
      if(anim_edit_wnd_pos.valid){
         ck.Read(&anim_edit_wnd_pos.x, sizeof(anim_edit_wnd_pos.x));
         ck.Read(&anim_edit_wnd_pos.y, sizeof(anim_edit_wnd_pos.y));
      }
      if(hWnd_anim_edit)
         SetWindowPos(hWnd_anim_edit, NULL, anim_edit_wnd_pos.x, anim_edit_wnd_pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);

      SetAnimFramesTransparency(model_transparency);
      ed->CheckMenu(this, E_ANIM_TOGGLE_MODEL_TRANSP, model_transparency);
      ed->CheckMenu(this, E_ANIM_TOGGLE_PATHS, render_paths);
      ed->CheckMenu(this, E_ANIM_LOOP_MODE, loop_mode);
      ed->CheckMenu(this, E_ANIM_LOCK_MODEL, lock_anim_model);

      return true;
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      ck.Write(&PLG_STATE_VERSION, sizeof(byte));
      ck.Write(&model_transparency, sizeof(bool));
      ck.Write(&render_paths, sizeof(bool));
      ck.Write(&loop_mode, sizeof(bool));
      ck.Write(&lock_anim_model, sizeof(bool));
                              //window pos
      if(hWnd_anim_edit){
         StoreAnimEditWndPos();
      }
      ck.Write(&anim_edit_wnd_pos.valid, sizeof(anim_edit_wnd_pos.valid));
      if(anim_edit_wnd_pos.valid){
         ck.Write(&anim_edit_wnd_pos.x, sizeof(anim_edit_wnd_pos.x));
         ck.Write(&anim_edit_wnd_pos.y, sizeof(anim_edit_wnd_pos.y));
      }
      return true;
   }
};

//----------------------------

void CreateAnimEdit(PC_editor ed){
   PC_editor_item ei = new C_edit_AnimEdit;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
//----------------------------