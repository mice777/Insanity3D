#include "pch.h"
#include <insanity\TickClass.h>
#include <Integer.h>

//----------------------------

#define MAX_MOUSE_BUTTONS 8   //maximal number of mouse button which we may read

//----------------------------

class C_controller_imp: public C_controller{
   
   C_smart_ptr<IGraph> igraph;

                              //mouse button positions
   byte mouse_button_pos[MAX_MOUSE_BUTTONS];

#ifdef JOYSTICK
                              //joystick, its button positions
   C_smart_ptr<C_Joy> joy;
   vector<byte> joy_button_pos;
                              //current joy state
   S_joy_state joy_state;
#endif//JOYSTICK

                              //mouse:
   int mouse_relative_pos[3]; //relative position change for 3 mouse axes
   dword mouse_buttons;       //bits of mouse buttons

   struct S_ctrl_slot{
      E_VALUE_MODE val_mode;  //value representation of this slot
      dword button_mask;      //for masking-of ignored buttons
      int ignore_time;        //countdown
      bool was_off;           //set when last time this slot returned zero value
      dword value;            //slot's current value (updated in Update, read by Get)

      struct S_ctrl_input{
         E_INPUT_DEVICE inp_dev;
         union{
                              //VM_BOOL:
            struct{
               union{
                  char key_code;
                  dword mouse_mask;
#ifdef JOYSTICK
                  dword joy_mask;
#endif
               };
                              //VM_FLOAT:
               float button_value;  //what float value will button have when pressed
#ifdef JOYSTICK
               float joy_rel_scale;
#endif
            } s1;
            struct{
               union{
                  int mouse_axis;   //index, 0x80000000 = negate
#ifdef JOYSTICK
                  int joy_axis;     //index, 0x80000000 = negate
                  int joy_pov_index;//index, (data<<16) = data = angle (0 - 7)
#endif
               };
#ifdef JOYSTICK
               float joy_thresh; //when joy value is set to bool true
#endif
            } s2;
         };
      } inputs[CTRL_INPUTS_PER_SLOT];

      S_ctrl_slot(){
         memset(this, 0, sizeof(*this));
      }
   };
   S_ctrl_slot *ctrl_slots;
   int max_slots;

                              //configuration
   dword config[CFG_LAST];

public:
   C_controller_imp():
      ctrl_slots(NULL),
      mouse_buttons(0),
      max_slots(0)
   {
      for(int i=0; i<MAX_MOUSE_BUTTONS; i++)
         mouse_button_pos[i] = 32;
#ifdef JOYSTICK
      memset(&joy_state, 0, sizeof(joy_state));
#endif
      memset(mouse_relative_pos, 0, sizeof(mouse_relative_pos));
      memset(config, 0, sizeof(config));
   }

   ~C_controller_imp(){
      Close();
   }

//----------------------------

   virtual bool Init(PIGraph ig, class C_Joy *jp, int max_slots1){

      max_slots = max_slots1;
      igraph = ig;
#ifdef JOYSTICK
      joy = jp;
#endif
      ctrl_slots = new S_ctrl_slot[max_slots];

      SetupDefaultConfig();
      return true;
   }

//----------------------------

   virtual void Close(){

      igraph = NULL;
      delete[] ctrl_slots;
      ctrl_slots = NULL;
#ifdef JOYSTICK
      joy = NULL;
#endif
   }

//----------------------------

   virtual bool SetupSlotInput(int slot_index, int input_index, E_VALUE_MODE vm, E_INPUT_DEVICE id, dword data){

      assert(input_index>=0 && input_index<2);
      if(input_index >= 2)
         return false;
      assert(slot_index<max_slots);
      if(slot_index>=max_slots)
         return false;

      if(vm==VM_NULL){
                              //read current value mode
         vm = ctrl_slots[slot_index].val_mode;
      }else{
                              //modify value mode
         S_ctrl_slot &cs = ctrl_slots[slot_index];
         if(vm!=cs.val_mode){
                              //clear all
            for(int i=0; i<CTRL_INPUTS_PER_SLOT; i++)
               cs.inputs[i].inp_dev = ID_NULL;
         }
      }

      S_ctrl_slot &cs = ctrl_slots[slot_index];
      S_ctrl_slot::S_ctrl_input &ci = cs.inputs[input_index];

      switch(id){

      case ID_KEYBOARD:
         switch(vm){

         case VM_BUTTONS:
            igraph->DefineKeys((const IG_KEY*)data, 32);
            break;

         case VM_BOOL:
            ci.s1.key_code = (char)data;
            cs.button_mask = 1;
            break;

         case VM_FLOAT:
            ci.s1.key_code = (char)data;
            ci.s1.button_value = 1.0f;
            break;

         default:
            ci.s1.key_code = (char)data;
         }
         break;

      case ID_MOUSE_BUTTON:
         switch(vm){

         case VM_BUTTONS:
            memcpy(mouse_button_pos, (void*)data, sizeof(mouse_button_pos));
            break;

         case VM_BOOL:
            ci.s1.mouse_mask = (1<<data);
            cs.button_mask = 1;
            break;

         case VM_FLOAT:
            ci.s1.mouse_mask = (1<<data);
            ci.s1.button_value = 1.0f;
            break;

         default:
            ci.s1.mouse_mask = (1<<data);
         }
         break;

      case ID_MOUSE_AXIS:
         switch(vm){

         case VM_INT:
         case VM_BOOL:
            if((data&0x7fffffff) >= 3)
               return false;
            ci.s2.mouse_axis = data;
            break;

         default:
            return false;
         }
         break;

#ifdef JOYSTICK
      case ID_JOY_BUTTON:
         if(!joy) return false;
         switch(vm){
         case VM_BUTTONS:
            {
               byte *bp = (byte*)data;
               S_joy_info ji;
               joy->GetInfoStruct(&ji);
               dword num_j_buttons = Min(32, ji.buttons_count);
               for(int i=0; i<num_j_buttons; i++)
                  joy_button_pos.push_back(bp[i]);
            }
            break;
         case VM_BOOL:
            ci.joy_mask = (1<<data);
            cs.button_mask = 1;
            break;
         case VM_FLOAT:
            ci.joy_mask = (1<<data);
            ci.button_value = 1.0f;
            break;
         default:
            ci.joy_mask = (1<<data);
         }
         break;

      case ID_JOY_AXIS:
         if(!joy)
            return false;
         switch(vm){
         case VM_FLOAT:
         case VM_BOOL:
         case VM_INT:
            {
               S_joy_info ji;
               joy->GetInfoStruct(&ji);
               //dword num_j_axes = ji.axis_count;
               dword num_j_axes = 6;
               if((data&0x7fffffff) >= num_j_axes) return false;
               ci.joy_axis = data;
               switch(vm){
               case VM_BOOL:
                  ci.joy_thresh = .7f;
                  break;
               case VM_INT:
                  ci.joy_rel_scale = 20.0f;
                  break;
               }
            }
            break;
         default:
            return false;
         }
         break;

      case ID_JOY_POV:
         if(!joy) return false;
         switch(vm){
         case VM_FLOAT:
         case VM_INT:
         case VM_BOOL:
            {
               S_joy_info ji;
               joy->GetInfoStruct(&ji);
               dword num_j_povs = ji.POVs_count;
               if((data&0xffff) >= num_j_povs) return false;
               ci.joy_pov_index = data;
            }
            break;
         }
         break;
#else
      case ID_JOY_BUTTON:
      case ID_JOY_AXIS:
      case ID_JOY_POV:
         break;
#endif//!JOYSTICK

      case ID_NULL:           //erasing - ok
         break;

      default:                //error, unsupported combination
         assert(0);
         return false;
      }

      cs.val_mode = vm;
      ci.inp_dev = id;
      return true;
   }

//----------------------------

   virtual bool SetConversionValue(int slot_index, int input_index, dword data){

                              //slot must be already initialized
      assert(slot_index<max_slots);
      if(slot_index>=max_slots)
         return false;

      S_ctrl_slot &cs = ctrl_slots[slot_index];
      S_ctrl_slot::S_ctrl_input &ci = cs.inputs[input_index];
      switch(ci.inp_dev){
      case ID_KEYBOARD:
      case ID_MOUSE_BUTTON:
      case ID_JOY_BUTTON:
         switch(cs.val_mode){
         case VM_FLOAT:
            ci.s1.button_value = *(float*)&data;
            break;
         }
         break;
#ifdef JOYSTICK
      case ID_JOY_AXIS:
         switch(cs.val_mode){
         case VM_BOOL:
            ci.joy_thresh = *(float*)&data;
            break;
         case VM_INT:
            ci.joy_rel_scale = *(float*)&data;
            break;
         }
         break;
#endif
      }

      return true;
   }

//----------------------------

   virtual bool GetSlotSetting(int slot_index, int input_index, E_VALUE_MODE &vm, E_INPUT_DEVICE &id, dword &data){

      assert(slot_index<max_slots);
      if(slot_index>=max_slots)
         return false;

      S_ctrl_slot &cs = ctrl_slots[slot_index];
      S_ctrl_slot::S_ctrl_input &ci = cs.inputs[input_index];
      vm = cs.val_mode;
      id = ci.inp_dev;

      switch(id){
      case ID_KEYBOARD:
         data = (byte)ci.s1.key_code;
         break;

      case ID_MOUSE_BUTTON:
         data = FindHighestBit(ci.s1.mouse_mask);
         break;

      case ID_MOUSE_AXIS:
         data = ci.s2.mouse_axis;
         break;

#ifdef JOYSTICK
      case ID_JOY_BUTTON:
         data = FindHighestBit(ci.joy_mask);
         break;

      case ID_JOY_AXIS:
         data = ci.joy_axis;
         break;

      case ID_JOY_POV:
         data = ci.joy_pov_index;
         break;
#endif//JOYSTICK
      }
      return true;
   }

//----------------------------

   virtual dword Get(int slot_index, bool ignore_if_get){

      assert(slot_index<max_slots);
      if(slot_index>=max_slots)
         return false;

      const S_ctrl_slot &cs = ctrl_slots[slot_index];
      dword ret = cs.value;
      if(cs.value && ignore_if_get)
         IgnoreValue(slot_index, true);
      return ret;
   }

//----------------------------

   virtual void Update(const S_tick_context &tc){

#ifdef JOYSTICK
      if(joy){
         joy->Update();
         joy->GetState(&joy_state);
      }
#endif   //JOYSTICK

      mouse_relative_pos[0] = tc.mouse_rel[0];
      mouse_relative_pos[1] = tc.mouse_rel[1];
      mouse_relative_pos[2] = tc.mouse_rel[2];
      mouse_buttons = tc.mouse_buttons;

      const bool *key_map = igraph->GetKeyboardMap();

                              //check which keys were down since last Update, but are not down now
      const int MAX_BUF_KEYS = 32;
      IG_KEY buf_keys[MAX_BUF_KEYS];
      int num_buf_keys = 0;
      while(true){
         IG_KEY bk = igraph->GetBufferedKey();
         if(!bk)
            break;
         if(!key_map[(byte)bk] && num_buf_keys<MAX_BUF_KEYS){
                              //add to buffered keys
            for(int i=num_buf_keys; i--; ){
               if(buf_keys[i]==bk)
                  break;
            }
            if(i==-1){
               buf_keys[num_buf_keys++] = bk;
            }
         }
      }

      for(int i=max_slots; i--; ){
         S_ctrl_slot &cs = ctrl_slots[i];
         cs.ignore_time = Max(0, cs.ignore_time-tc.time);

         cs.value = 0;
                              //mix in all inputs
         for(int i=0; i<CTRL_INPUTS_PER_SLOT; i++){
            S_ctrl_slot::S_ctrl_input &ci = cs.inputs[i];
            switch(cs.val_mode){
            case VM_BUTTONS:
               {
                                    //mix in keyboard buttons
                  cs.value = igraph->ReadKeys();
                                    //mix in mouse buttons
                  {
                     dword mbut = mouse_buttons;
                     for(int i=0, mask = 1; i<MAX_MOUSE_BUTTONS; i++, mask <<= 1){
                        if(mbut&mask)
                           cs.value |= (1<<mouse_button_pos[i]);
                     }
                  }
#ifdef JOYSTICK
                                    //mix in joystick buttons
                  if(joy){
                     dword jbut = joy->GetButtonsState();
                     for(int i=joy_button_pos.size(), mask = (1<<(joy_button_pos.size()-1)); i--; mask >>= 1){
                        if(jbut&mask)
                           cs.value |= (1<<joy_button_pos[i]);
                     }
                  }
#endif   //JOYSTICK
                  cs.button_mask |= ~cs.value;
                  cs.value &= cs.button_mask;
               }
               break;

            case VM_BOOL:
               switch(ci.inp_dev){

               case ID_KEYBOARD:
                  if(key_map[(byte)ci.s1.key_code])
                     cs.value |= 1;
                  else{
                     for(int i=num_buf_keys; i--; ){
                        if(buf_keys[i]==(byte)ci.s1.key_code){
                           cs.value |= 1;
                           break;
                        }
                     }
                  }
                  break;

               case ID_MOUSE_BUTTON:
                  if(mouse_buttons&ci.s1.mouse_mask)
                     cs.value |= 1;
                  break;

#ifdef JOYSTICK
               case ID_JOY_BUTTON:
                  if(joy)
                  if(joy_state.buttons&ci.joy_mask)
                     cs.value |= 1;
                  break;

               case ID_JOY_AXIS:
                  {
                     float f = (&joy_state.x)[ci.joy_axis&0x7fffffff];
                     if(ci.joy_axis&0x80000000) f = -f;
                     if(f >= ci.joy_thresh)
                        cs.value |= 1;
                  }
                  break;

               case ID_JOY_POV:
                  {
                     int pov_i = ci.joy_pov_index&0xffff;
                     if(!joy_state.pov_centered[pov_i]){
                        int pov_dir = ((int)((joy_state.pov[pov_i] * 4 + PI/16) / PI));
                        if((ci.joy_pov_index>>16) == pov_dir)
                           cs.value |= 1;
                     }
                  }
                  break;
#endif   //JOYSTICK

               case ID_MOUSE_AXIS:
                  {
                     int i = mouse_relative_pos[ci.s2.mouse_axis&0x7fffffff];
                     if(ci.s2.mouse_axis&0x80000000) i = -i;
                     if(i > 0)
                        cs.value |= 1;
                  }
                  break;
               }
               if(i==CTRL_INPUTS_PER_SLOT-1){
                  cs.button_mask |= ~cs.value;
                  cs.value &= cs.button_mask;
               }
               break;

            case VM_FLOAT:
               switch(ci.inp_dev){

               case ID_KEYBOARD:
                  if(key_map[(byte)ci.s1.key_code])
                     cs.value = *(dword*)&ci.s1.button_value;
                  else{
                     for(int i=num_buf_keys; i--; ){
                        if(buf_keys[i]==(byte)ci.s1.key_code){
                           cs.value = *(dword*)&ci.s1.button_value;
                           break;
                        }
                     }
                  }
                  break;

               case ID_MOUSE_BUTTON:
                  if(mouse_buttons&ci.s1.mouse_mask)
                     cs.value = *(dword*)&ci.s1.button_value;
                  break;

#ifdef JOYSTICK
               case ID_JOY_BUTTON:
                  if(joy && (joy_state.buttons&ci.joy_mask))
                     cs.value = *(dword*)&ci.button_value;
                  break;

               case ID_JOY_AXIS:
                  {
                     float f = (&joy_state.x)[ci.joy_axis&0x7fffffff];
                     if(ci.joy_axis&0x80000000) f = -f;
                     if(!cs.value)
                        *(float*)&cs.value = Max(0.0f, f);
                  }
                  break;

               case ID_JOY_POV:
                  {
                     if(!i)
                        *(float*)&cs.value = -1;
                     int pov_i = ci.joy_pov_index&0xffff;
                     if(!joy_state.pov_centered[pov_i])
                        *(float*)&cs.value = joy_state.pov[pov_i];
                  }
                  break;
#endif   //JOYSTICK
               }
               break;

            case VM_INT:
               switch(ci.inp_dev){

               case ID_MOUSE_AXIS:
                  if(!cs.value)
                     cs.value = mouse_relative_pos[ci.s2.mouse_axis&0x7fffffff];
                  break;

#ifdef JOYSTICK
               case ID_JOY_AXIS:
                  if(!cs.value){
                     int axis = ci.joy_axis&0x7fffffff;
                     const float &f = (&joy_state.x)[axis];
                     cs.value = f * ci.joy_rel_scale;
                     if(axis==1)
                        cs.value = -cs.value;
                  }
                  break;

               case ID_JOY_POV:
                  {
                     if(!i)
                        cs.value = -1;
                     int pov_i = ci.joy_pov_index&0xffff;
                     if(!joy_state.pov_centered[pov_i])
                        //cs.value = joy_state.pov[pov_i] * 180.0f / PI;
                        cs.value = (int)((joy_state.pov[pov_i] * 4 + PI/16) / PI);
                  }
                  break;

               case ID_JOY_BUTTON:
                  if(joy && (joy_state.buttons&ci.joy_mask))
                     cs.value |= 1; 
                  break;
#endif   //JOYSTICK

               case ID_KEYBOARD:
                  if(key_map[(byte)ci.s1.key_code])
                     cs.value |= 1;
                  else{
                     for(int i=num_buf_keys; i--; ){
                        if(buf_keys[i]==(byte)ci.s1.key_code){
                           cs.value |= 1;
                           break;
                        }
                     }
                  }
                  break;

               case ID_MOUSE_BUTTON:
                  if(mouse_buttons&ci.s1.mouse_mask)
                     cs.value |= 1;
                  break;
               }
               break;
            }
         }
         if(!cs.value){
            cs.was_off = true;
            cs.ignore_time = 0;
         }else
         if(cs.ignore_time){
            cs.value = 0;
         }
      }
   }

//----------------------------

   virtual void IgnoreValue(int slot_index, dword data){

      assert(slot_index<max_slots);
      if(slot_index>=max_slots)
         return;

      S_ctrl_slot &cs = ctrl_slots[slot_index];
      cs.value = 0;
      switch(cs.val_mode){
      case VM_BUTTONS:
         cs.button_mask &= ~data;
         break;
      case VM_BOOL:
         if(data<=2){
            cs.button_mask = 0;
         }else{
            cs.ignore_time = cs.was_off ? data : data/4;
         }
         cs.was_off = false;
         for(int i=0; i<2; i++){
            switch(cs.inputs[i].inp_dev){
            case ID_KEYBOARD:
               if(data==2)
                  ((byte*)igraph->GetKeyboardMap())[(byte)cs.inputs[i].s1.key_code] = false;
               break;
            }
         }
         break;
      }
   }

//----------------------------

   virtual void IgnoreAllPressed(){

      const bool *kb_map = igraph->GetKeyboardMap();
      dword buttons = igraph->GetMouseButtons();

      for(int i=max_slots; i--; ){
         S_ctrl_slot &cs = ctrl_slots[i];
         if(cs.val_mode==VM_BOOL){
            for(int j=0; j<2; j++){
               const S_ctrl_slot::S_ctrl_input &in = cs.inputs[j];
               switch(in.inp_dev){
               case ID_KEYBOARD:
                  {
                     int scancode = (byte)in.s1.key_code;
                     if(kb_map[scancode]){
                        cs.button_mask = 0;
                     }
                  }
                  break;
               case ID_MOUSE_BUTTON:
                  {
                     dword mb = in.s1.mouse_mask;
                     if(buttons&mb){
                        cs.button_mask = 0;
                     }
                  }
                  break;
               }
            }
         }
      }
   }

//----------------------------

   virtual bool FindSlot(E_INPUT_DEVICE id, dword data, int &slot, int &input_index) const{

      for(int i=max_slots; i--; ){
         const S_ctrl_slot &cs = ctrl_slots[i];
         for(int j=0; j<2; j++){
            if(id==ID_NULL || cs.inputs[j].inp_dev == id){
               dword ci_data;
               switch(cs.inputs[j].inp_dev){
               case ID_KEYBOARD: ci_data = (byte)cs.inputs[j].s1.key_code; break;
               case ID_MOUSE_BUTTON: ci_data = FindHighestBit(cs.inputs[j].s1.mouse_mask); break;
               case ID_MOUSE_AXIS: ci_data = cs.inputs[j].s2.mouse_axis; break;
#ifdef JOYSTICK
               case ID_JOY_BUTTON: ci_data = FindHighestBit(cs.inputs[j].joy_mask); break;
               case ID_JOY_AXIS: ci_data = cs.inputs[j].joy_axis; break;
               case ID_JOY_POV: ci_data = cs.inputs[j].joy_pov_index;
#endif//JOYSTICK
               default: assert(0); ci_data = 0;
               }
               if(ci_data == data){
                  slot = i;
                  input_index = j;
                  return true;
               }
            }
         }
      }
      return false;
   }

//----------------------------

   virtual bool ScanInput(E_INPUT_DEVICE &id, dword &data) const{

                              //check keyboard first
      const bool *km = igraph->GetKeyboardMap();
      for(int i=0; i<256; i++)
      if(km[i]){
         id = ID_KEYBOARD;
         data = i;
         return true;
      }
                              //check mouse buttons
      if(mouse_buttons){
         id = ID_MOUSE_BUTTON;
         data = FindHighestBit(mouse_buttons);
         return true;
      }
                              //check mouse movement
      for(i=0; i<3; i++){
         int min = (i==2) ? 1 : 10;
         if(abs(mouse_relative_pos[i]) >= min){
            id = ID_MOUSE_AXIS;
            data = i;
            if(mouse_relative_pos[i] < 0) data |= 0x80000000;
            return true;
         }
      }
#ifdef JOYSTICK
      if(joy){
                              //check joystick buttons
         if(joy_state.buttons){
            id = ID_JOY_BUTTON;
            data = FindHighestBit(joy_state.buttons);
            return true;
         }
                              //check joystick movement
         S_joy_info ji;
         joy->GetInfoStruct(&ji);
         dword num_j_axes = 6;
         for(i=0; i<num_j_axes; i++){
            float f = joy->GetAxisState(i);
            if(fabs(f) >= .7f){
               id = ID_JOY_AXIS;
               data = i;
               if(f < 0.0f) data |= 0x80000000;
               return true;
            }
         }
         for(i=0; i<ji.POVs_count; i++){
            float dir;
            if(joy->GetPOVState(&dir, i)){
               id = ID_JOY_POV;
               data = i | (((int)((dir * 4 + PI/16) / PI))<<16);
               return true;
            }
         }
      }
#endif   //JOYSTICK
      return false;
   }

//----------------------------

   virtual void SetConfigValue(E_CONFIG i, dword val){
      assert(dword(i)<CFG_LAST);
      config[i] = val;
   }

//----------------------------

   virtual dword GetConfigValue(E_CONFIG i) const{
      assert(dword(i)<CFG_LAST);
      return config[i];
   }

//----------------------------

   virtual void SetupDefaultConfig(){

      memset(config, 0, sizeof(config));
      config[CFG_INVERT_MOUSE_Y] = false;
      config[CFG_MOUSE_SENSITIVITY] = 100;
      config[CFG_ALWAYS_RUN] = false;
   }
};

//----------------------------

LPC_controller CreateController(){
   return new C_controller_imp;
}

//----------------------------
// Instance of controller implementation.
//----------------------------
//static C_controller_imp controller_instance;
//LPC_controller controller = &controller_instance;

//----------------------------

