#include <rules.h>
#include <c_unknwn.hpp>

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Class reading controller input.
//----------------------------

#define CTRL_INPUTS_PER_SLOT 2   //how many buttons/keys may be assigned to each slot


//----------------------------
// Pure class, the implementation is in controller.cpp.
//----------------------------
class C_controller: public C_unknown{
public:

//----------------------------
// Input device from which input is received.
// Data associated with input devices:
// ID_KEYBOARD       ...   data = pointed to array of 32 characters to be scanned
// ID_MOUSE_BUTTON   ...   data = index of mouse button, button is in range 0 ... 2
// ID_JOY_BUTTON     ...    ''
// ID_MOUSE_AXIS     ...   data = index of mouse half-axe, the range is 0 ... 2, for negative ranges highest bit (31) is set (OR 0x80000000)
// ID_JOY_AXIS       ...    ''
// ID_JOY_POV        ...   index of POV button (low 16 bits), angle of POV (high 16 bits)
//----------------------------
   enum E_INPUT_DEVICE{
      ID_NULL,                
      ID_KEYBOARD,            
      ID_MOUSE_BUTTON,        
      ID_MOUSE_AXIS,
      ID_JOY_BUTTON,
      ID_JOY_AXIS,
      ID_JOY_POV,
   };

//----------------------------
// Value representation.
//----------------------------
   enum E_VALUE_MODE{
      VM_NULL,
      VM_BUTTONS,             //buttons 0-31 (boolean)
      VM_FLOAT,               //value -1.0 ... +1.0
      VM_BOOL,                //false/true
      VM_INT,                 //range 0 ... n
   };

//----------------------------
// Config registers.
//----------------------------
   enum E_CONFIG{
      CFG_INVERT_MOUSE_Y,     //bool
      CFG_MOUSE_SENSITIVITY,  //int in range 0 (minimal) to unlimited, 100 is optimal
      CFG_ALWAYS_RUN,         //bool
      CFG_LAST
   };

//----------------------------
// Initialize controller interface.
// The input values are read from:
// 1. IGraph interface (keyboard, mouse)
// 2. C_Joy interface (joystick and other game controllers)
//----------------------------
   virtual bool Init(class IGraph *ig, class C_Joy*, int max_slots) = 0;

//----------------------------
// Uninitialize interface - release references to interfaces which were used at initialization.
//----------------------------
   virtual void Close() = 0;

//----------------------------
// Setup input for a controller slot.
// - value mode may be set, mofified, or left unchanged (VM_NULL)
//   (value mode of uninitialized slots must be specified for the first time, then next
//    calls may only need to update input device, or change also value mode)
// - input device and data must always be specified
//----------------------------
   virtual bool SetupSlotInput(int slot_index, int input_index, E_VALUE_MODE vm, E_INPUT_DEVICE id, dword data) = 0;

//----------------------------
// Set value for conversion between various value modes.
// The conversion value is used when:
// - input device is a button, and returned value is float, the conversion value means float value returned when button is down, otherwise returned value is 0.0
// - input device is joystick, and returned value is bool, the conversion value means threshold value, after which joystick axe returns true
//----------------------------
   virtual bool SetConversionValue(int slot_index, int input_index, dword data) = 0;

//----------------------------
// Get input settings of a slot. This function returns settings set by SetupSlotInput.
// If slot is not set up, the returned value is false.
//----------------------------
   virtual bool GetSlotSetting(int slot_index, int input_index, E_VALUE_MODE&, E_INPUT_DEVICE&, dword &data) = 0;

//----------------------------
// Get value from an input slot of the controller. The returned value is in format
// of the slot (defined by E_VALUE_MODE), represented by a bitmask.
// If 'ignore_if_get' is true and Get returns non-zero value, function internally calls IgnoreValue after getting the value.
// Note: float numbers are also represented by a bitmask.
//----------------------------
   virtual dword Get(int slot_index, bool ignore_if_get = false) = 0;

//----------------------------
// Ignore value of a slot. The values returning VM_BUTTONS or VM_BOOL may only be ignored.
// The value is ignored until player holds down particular button. When the button is released,
// the value is returned next time the button is pressed.
   virtual void IgnoreValue(int slot_index, dword data = true) = 0;

//----------------------------
// Ignore all pressed keys.
   virtual void IgnoreAllPressed() = 0;

//----------------------------
// Update - read input from devices.
//----------------------------
   virtual void Update(const struct S_tick_context&) = 0;

//----------------------------
// Find slot and its input index, which uses particular input device. For example, this way
// it's possible to find slot which uses ESC key.
//----------------------------
   virtual bool FindSlot(E_INPUT_DEVICE is, dword data, int &slot, int &input_index) const = 0;

//----------------------------
// Scan known devices for input - return first device and data which is moved/pressed.
// This function is used for re-defining game controlls.
// If some input is successfully scanned, the returned value is true.
//----------------------------
   virtual bool ScanInput(E_INPUT_DEVICE&, dword &data) const = 0;

//----------------------------
// Setup internal config register.
//----------------------------
   virtual void SetConfigValue(E_CONFIG, dword) = 0;

//----------------------------
// Write internal config register.
//----------------------------
   virtual dword GetConfigValue(E_CONFIG) const = 0;

//----------------------------
// Setup default values for all configuration register.
//----------------------------
   virtual void SetupDefaultConfig() = 0;
};

typedef C_controller *LPC_controller;

//----------------------------
//----------------------------

LPC_controller CreateController();

//----------------------------
// Global instance of one and only game input controller interface.
//----------------------------
//extern LPC_controller controller;

//----------------------------
//----------------------------
// Setup default controller map - to be called at game initialization,
// as well as when user hits 'Defaults' button in a controller config menu.
// The input parameter specifies slot which should be set to default value,
// if this is set to -1, all slots are set to defaults.
//----------------------------
void SetupControllerDefaults(int slot = -1);

//----------------------------
// Read/write current controller config to system storage area (registry).
//----------------------------
bool ReadControllerConfig();
bool WriteControllerConfig();

//----------------------------
