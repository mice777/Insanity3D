#ifndef __CAMPATH_H
#define __CAMPATH_H

#include <i3d\i3d_math.h>
#include <math\bezier.hpp>
#include <math\ease.hpp>
#include <c_unknwn.hpp>
#include <C_str.hpp>

/*----------------------------
   Copyright (c) Lonely Cat Games  All rights reserved.

Camera path bezier interpolator - class building bezier line from scene dummy system, and allows for animation
along this path.
It may be used for animation of any frame (camera, cannons, etc).
----------------------------*/

//----------------------------
//----------------------------
#define CAMPATH_MAX_NOTES_PER_KEY 4

                              //camera path key
struct S_cam_path_key{
   S_vector pos;              //position
   S_vector dir;              //main direction
   S_vector up;               //up vector
   //float easy_to, easy_from;
   //float smooth;
   float easiness;
   float speed;               //speed of flying on segment from current to next key
   dword pause;               //pause at the key
   struct S_note{
      C_str text;             //note string
      float pos;              //relative position between this and next key
      S_note(): pos(0.0f){}
      S_note(const S_note &n){ operator =(n); }
      S_note &operator =(const S_note &n){ text = n.text; pos = n.pos; return *this; }
      inline bool operator <(const S_note &n) const{
         return (pos < n.pos);
      }
   };
   S_note notes[CAMPATH_MAX_NOTES_PER_KEY];
   S_cam_path_key():
      speed(10.0f),
      /*
      easy_from(0.0f),
      easy_to(0.0f),
      smooth(1.0f),
      */
      easiness(0.0f),
      pause(0)
   {}
   /*
   S_cam_path_key(const S_cam_path_key &n){ operator =(n); }
   void operator =(const S_cam_path_key &n){
      pos = n.pos;
      dir = n.dir;
      up = n.up;
      speed = n.speed;
      easy_from = n.easy_from;
      easy_to = n.easy_to;
      smooth = n.smooth;
      pause = n.pause;
      for(dword i=0; i<CAMPATH_MAX_NOTES_PER_KEY; i++)
         notes[i] = n.notes[i];
   }
   */
};

//----------------------------
                              //data associated with single camera path
struct S_camera_path_data{
   C_str name;                //user-friendly name of the path
   C_vector<S_cam_path_key> keys;  //keys making up the path

   S_camera_path_data(){}
   S_camera_path_data(const S_camera_path_data &d){ operator =(d); }
   void operator =(const S_camera_path_data &d){
      name = d.name;
      keys = d.keys;
   }
};

//----------------------------
// Load single path from CT_CAMERA_PATH chunk. Chunk is not descended after loading.
bool LoadCameraPath(class C_chunk &ck, S_camera_path_data &path);

//----------------------------
// Load specified animations from the CT_CAMERA_PATHS chunk. Chunk is not descended after loading.
bool LoadCameraPathByName(class C_chunk &ck, const char *name, S_camera_path_data &path);

//----------------------------

class C_camera_path: public C_unknown{
public:
   virtual ~C_camera_path(){}

//----------------------------
// The callback should return true to allow further callbacks be processed and process time further in current Tick,
// or false to freeze at the position of note, and not advance position further in this Tick.
   typedef bool t_NotifyCallback(const C_str &note, void *context);

//----------------------------
// Build bezier path from scene.
   virtual bool Build(const S_camera_path_data&) = 0;

//----------------------------
// Destroy current path, forget everything.
   virtual void Close() = 0;

//----------------------------
// Render path of bezier path for debugging purposes.
   virtual void DebugRender(I3D_scene*) = 0;

//----------------------------
// Get current position (in msec).
   virtual int GetPosition() const = 0;

//----------------------------
// Set current anim position (in msec). Setting -1 means setting at the end.
   virtual bool SetPosition(int time) = 0;

//----------------------------
// Set position by key index.
   virtual bool SetKeyPosition(dword key_index) = 0;

//----------------------------
// Get current key index and (optionally) time relative to the key.
   virtual dword GetCurrentKey(dword *key_time = NULL) const = 0;

//----------------------------
// Get key's time (-1 if invalid key);
   virtual dword GetKeyTime(dword key_index) const = 0;

//----------------------------
// Evaluate current position based on elapsed time. Call notify callback func for each encountered callback.
// Store resulting position and rotation in given 'pos' and 'rot' params.
// The returned value is false if path is not build properly, or already at the end. In this case, the function
// doesn't return valid values.
   virtual bool Tick(int time, S_vector &pos, S_vector &dir, S_vector &up, t_NotifyCallback* = NULL, void *context = NULL,
      bool edit_mode = false) = 0;

//----------------------------
// Get position and direction at specified position.
   virtual bool ComputePosition(dword key_index, float ratio_to_next, S_vector *pos,
      S_vector *dir = NULL, S_vector *up = NULL) const = 0;

//----------------------------
// Check if animation is in running mode.
   virtual bool IsAtEnd() const = 0;
};

//----------------------------

C_camera_path *CreateCameraPath();

//----------------------------

#endif

