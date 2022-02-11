
//----------------------------
// Common functions shared between game files.
//----------------------------

#ifndef __COMMON_H_
#define __COMMON_H_

//----------------------------
// Get angle of 2D C_vector against C_vector[1, 0]. The arc is left-turning.
// Returned value is  in range <0, 2*PI).
float GetAngle2D(float x, float y);

//----------------------------
// Get angle of 2D C_vector against C_vector[1, 0]. The arc is left-turning.
// Returned value is in range <-PI, PI).
float GetAngle2Da(float x, float y);

//----------------------------
// Check collision material of specified frame.
// The returned value is index into material table.
// If material couldn't be determined, the return value is 0 (default).
int GetMaterialID(CPI3D_frame);

//----------------------------
// Compute local center of frame.
S_vector GetFrameLocalCenter(CPI3D_frame frm);

//----------------------------
// global ease interpolators
extern const C_ease_interpolator ease_in_out, ease_in, ease_out;

//----------------------------
// Implicit conversion if float to int, with rounding to nearest.
#if defined _MSC_VER
inline int FloatToInt(float f){
   __asm{
      fld f
      fistp f 
   }
   return *(int*)&f;
}

#else

inline int FloatToInt(float f){
   return (int)f;
}

#endif

//----------------------------
// From relative mouse movement, adjust current aim C_vector. The default points to
// specified direction and is limited by specified angle.
bool ComputeMouseAiming(int rx, int ry, const S_vector &default_dir, float limit_angle,
   float look_speed, S_vector &curr_aim_dir);

//----------------------------
// Return value between 'from' and 'to' in ratio.
// Template class must provide operato - and *, ratio should be <0.0f -1.0f>.
template<class T>
static T Interpolate(T from, T to, float ratio){
   return from + (to - from) * ratio;
}

//----------------------------
// Set/Get brightness of all model's frames.
void SetModelBrightness(PI3D_model mod, float brightness, I3D_VISUAL_BRIGHTNESS reg = I3D_VIS_BRIGHTNESS_NOAMBIENT);
float GetModelBrightness(CPI3D_model mod, I3D_VISUAL_BRIGHTNESS reg = I3D_VIS_BRIGHTNESS_NOAMBIENT);


//----------------------------
// Determine frame (of sepecified type), to which this frame is linked.
PI3D_frame GetFrameParent(PI3D_frame frm, I3D_FRAME_TYPE type);

//----------------------------
// Search frame hiearchy up to model, return associated actor if find it.
class C_actor *FindFrameActor(PI3D_frame frm1);

//----------------------------
//Check if frm is child of frame root. 
bool IsChildOf(CPI3D_frame frm, CPI3D_frame root);

//----------------------------
// Determine sector, in which this frame is linked.
PI3D_sector GetFrameSector(PI3D_frame frm);

//----------------------------
// Delete all volumes from given model.
void DeleteVolumes(PI3D_model);

//----------------------------
// Enable or disable all model's volumes.
void EnableVolumes(PI3D_model, bool on);

//----------------------------

class C_class_to_be_ticked *GetTickClass();
void SetTickClass(C_class_to_be_ticked *tc);

//----------------------------
// Global collision-testing function.
// (Collects profiler data in debug mode).
inline bool TestCollision(CPI3D_scene scene, I3D_collision_data &cd){
   return scene->TestCollision(cd);
}

//----------------------------
// Save screenshot of current contents of backbuffer.
void SaveScreenShot();

//----------------------------
// Class which fluently interpoolaates from current position to selected destination.
class C_linear_interpolator{
   float current_pos;
   float destination;
   float speed;
public:
//----------------------------
   C_linear_interpolator(float spd = 1.0f):
      current_pos(0.0f),
      destination(0.0f),
      speed(spd)
   {}
//----------------------------
// pos1 ... current position
// destination ... destination
// speed how far 'pointer' goes per second
   C_linear_interpolator(float pos1, float destination1, float spd1):
      current_pos(pos1), destination(destination1),
      speed(spd1)
   {
      assert(speed > 0.0f);
   }

//----------------------------
// Set/Get destination.
   inline void SetDest(float d){ destination = d; }
   inline float GetDest() const{ return destination; }

//----------------------------
// Return current position.
   inline float GetCurrPos() const{ return current_pos; }

//----------------------------
// Set directly pointer's position and destination.
   inline void SetCurrPosDest(float new_pos){ current_pos = destination = new_pos; }

//----------------------------
// Updates new position of 'pointer' and returns it.
   float Tick(int time){

      if(destination == current_pos)
         return current_pos;
      float k = speed * (float)time * .001f;
      current_pos += k * ( (destination - current_pos < 0)?(-1.0f):(1.0f) );
      if(I3DFabs(destination - current_pos) <= k)
         current_pos = destination;
      return current_pos;
   }

//----------------------------
// Check if current position equals to destination.
   inline bool IsStable() const{ return (destination == current_pos); }
};

//----------------------------
                              //exception thrown when unknown chunk type has been ascended
class C_except_ascend: public C_except{
public:
   C_except_ascend(C_chunk &ck):
      C_except(C_fstr("unknown chunk type: 0x%x", ck.GetLastAscendChunk()))
   {
   }
};

//----------------------------

#endif //__COMMON_H_