#ifndef __GAME_CAM_H_
#define __GAME_CAM_H_

//----------------------------

//----------------------------
                              //game camera
class C_game_camera: public C_unknown{
public:
                              //update camera each frame
   virtual void Tick(int time) = 0;

//----------------------------
// Save camera's state into provided chunk.
   virtual void SaveGame(C_chunk &ck) const = 0;

//----------------------------
// Apply camera's state from given chunk.
   virtual void ApplySavedGame(PI3D_scene scn, C_chunk &ck) throw(C_except) = 0;

//----------------------------
// Setup camera which is being used.
   virtual void SetCamera(PI3D_camera) = 0;
   virtual PI3D_camera GetCamera() = 0;
   virtual CPI3D_camera GetCamera() const = 0;

//----------------------------
// Set additional mix C_vector.
   virtual void SetDeltaDir(const S_vector &dir, const S_vector &up = S_vector(0, 1, 0)) = 0;
   virtual void SetDeltaRot(const S_quat &rot) = 0;
   virtual const S_quat &GetDeltaRot() const = 0;

//----------------------------
// Set current distance. The distance is changed slowly, unless 'immediate' is set to true.
   virtual void SetDistance(float, bool immediate = false) = 0;
   virtual float GetDistance() const = 0;
   virtual float GetRealDistance() const = 0;

   virtual void SetAngleMode(byte m) = 0;
   virtual byte GetAngleMode() const = 0;

   //virtual int GetFlyTime() const = 0;

   virtual void SetFocus(PI3D_frame fpos, PI3D_frame fdir) = 0;
   virtual CPI3D_frame GetFocus() const = 0;
   virtual CPI3D_frame GetFocusDir() const = 0;

//----------------------------
// Set additional shaking animation.
   virtual void SetAnim(const char *name, bool loop) = 0;

//----------------------------
// Reset camera so that it starts animate from its current ideal position (called when game begins).
   virtual void ResetPosition() = 0;
};

//----------------------------

C_game_camera *CreateGameCamera(PI3D_scene);

//----------------------------

#endif
