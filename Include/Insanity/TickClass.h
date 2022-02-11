#ifndef __TICK_CLASS_H
#define __TICK_CLASS_H

#include <c_unknwn.hpp>

//----------------------------

                              //structure containing all necessary info
                              // for updating anything
struct S_tick_context{
   int time;                  //time from last update
   enum IG_KEY key;           //recently pressed key
   byte modify_keys;          //key modifiers (Shift, Alt, Ctrl)
   int mouse_rel[3];          //mouse relative movement since last frame
   byte mouse_buttons;        //combination of mouse buttons currently held down
   class C_controller *p_ctrl;//pointer to controller (in updated state)

//----------------------------
// Clear this struct.
   void Zero(){
      memset(this, 0, sizeof(S_tick_context));
   }

//----------------------------
// Clear input parameters, just leave time.
   void ClearInput(){
      key = (IG_KEY)0;
      modify_keys = 0;
      mouse_rel[0] = 0;
      mouse_rel[1] = 0;
      mouse_rel[2] = 0;
      mouse_buttons = 0;
      p_ctrl = NULL;
   }
};

//----------------------------
                              //base class for any class used in game loop for main processing
class C_class_to_be_ticked: public C_unknown{
public:
   virtual ~C_class_to_be_ticked(){}

//----------------------------
// Get identifier of this class. Each inherited class of C_class_to_be_ticked has specific ID,
// by which it may be identified. ID of particular class defined with the class.
   virtual dword GetID() const = 0;

//----------------------------
// Update - to be called once per frame.
   virtual void Tick(const S_tick_context&) = 0;

//----------------------------
// Render frame.
   virtual void Render() = 0;

//----------------------------
// Screen reset, update changes (when change in resolution, bitdepth, or device is reset).
   virtual void ResetScreen(){}

//----------------------------
// Process incomming network chunk.
   virtual void NetIn(enum E_NETWORK_MESSAGE msg_id,
      class C_net_in_msg *in_msg){ }

//----------------------------
   //virtual void ProcessSystemNetMessage(PI_net inet, PC_net_in_msg msg, dword context){}
   virtual void OnPlayerDisconnect(dword pid, const char *name){}
   virtual void OnSessionLost(){}
};

typedef C_class_to_be_ticked *PC_class_to_be_ticked;

//----------------------------

#endif
