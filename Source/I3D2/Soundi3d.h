#ifndef __SOUNDI3D_H
#define __SOUNDI3D_H

#include "frame.h"

//----------------------------

class I3D_sound: public I3D_frame{

#define SNDF_LOOPED        1
#define SNDF_EMULATED      2        //sample 3D positioning emulated by volume and panning

#define SNDF_HARDWARE      8        //hardware buffer used
#define SNDF_STREAMING     0x10

#define SNDF_UPDATEFREQ    0x100
                              //following flags used only by non-emulated hw sounds
#define SNDF_UPDATEPOS     0x10000
#define SNDF_UPDATEDIR     0x20000
#define SNDF_UPDATEDIST    0x40000
#define SNDF_UPDATECONE    0x80000
#define SNDF_UPDATEOUTVOL  0x100000
#define SNDF_UPDATESCALE   0x400000

   I3D_SOUNDTYPE snd_type;
   float volume, volume_out;  //volume
   float cone_in, cone_out;   //cone
   C_str file_name;           //currently-open filename
   dword open_flags;          //flags by which file is opened
   class ISND_source *snd_src;//open wave data
   float range_n, range_f;    //sound range
   //float ambient_factor;
   float frequency;           //frequency multiplier

   mutable dword snd_flags;
   CPI3D_scene scene;

   C_vector<class I3D_sector*> sound_sectors;

                              //run-time:
   class ISND_3Dsound *voice; //NULL if voice not allocated
   float virtual_volume;      //virtual volume to avoid frequent volume changes
   float scaled_range_n, scaled_range_f;  //range scaled by inherited scale
   float scaled_range_f_2;    //scaled_range_f ^ 2
   float cos_cone_in, cos_cone_out; //cos(cone_???)
   //float fade_factor;         //reciprocal of delta of scaled_range_f and scaled_fade_dist

                              //virtual pausing:
   enum{ STOP, PLAY, PAUSE } state;
   int pause_time, pause_pos;

                              //fading:
   int fade_time;             //current fade time
   int fade_count_down;       //0 = not fading

   I3D_sound(CPI3D_scene);

   friend I3D_driver;         //creation
   friend I3D_scene;          //management
   friend I3D_sector;         //sound in sector
public:
   ~I3D_sound();
   void Close();

   inline C_vector<class I3D_sector*> &GetSectorsVector() { return sound_sectors; }
   inline const ISND_3Dsound *GetVoice() const{ return voice; }
   inline dword GetSndFlags() const{ return snd_flags; }

   void ReapplyVolume(bool hard);

//----------------------------
// Update sound - to be called each frame to emulate sounds, manage sound audibility,
// release finished voices, etc.
   void UpdateSound(int time);

//----------------------------
// Create physical voice and upload sample into it.
   void MakeVoice(); 

//----------------------------
// Pause sound (virtually, calling VirtualPause), and release resources.
   void FreeVoice();       

//----------------------------
// Pause sound (physically stop), keep time when it get paused, so that when it's
// audible again, it continues playing from that position.
   void VirtualPause();

//----------------------------
// Continue in playing of virtually-paused voice.
   void VirtualPlay();

//----------------------------
// Check if voice audible (check distance to camera), if true, also compute
// attenuation.
// As a side-effect, compute current distance to camera and
// direction cosine (audible point or spot sounds only).
   bool CheckIfAudible(int time, float &volume, float &curr_dist, float &dir_cos);

//----------------------------
// Compute attenuation based on distance to listener (camera).
   void ComputeAttenuation(float curr_dist, float dir_cos, float &new_volume);

//----------------------------
// Compute and apply panning effect based on position of sound relative to listener.
   void ComputePanning();

//----------------------------
// Compute fading. If fading is finished, return true, otherwise multiply volume by fade ratio.
   bool UpdateFading(int time, bool in, float &volume);

   //inline PI3D_scene GetScene(){ return scene; }
   inline CPI3D_scene GetScene() const{ return scene; }

   I3D_sound &operator =(const I3D_sound &);

//----------------------------
// Draw light - for debugging purposes. If 'vf' is not NULL, light's icon is clipped
// against view frustum and fast rejected if out.
   void Draw1(PI3D_scene, const struct S_view_frustum *vf, bool strong) const;

   virtual void PropagateDirty(){
      I3D_frame::PropagateDirty();
      snd_flags |= SNDF_UPDATEPOS | SNDF_UPDATEDIR | SNDF_UPDATECONE | SNDF_UPDATESCALE | SNDF_UPDATEDIST;
   }
public:
   I3DMETHOD(Duplicate)(CPI3D_frame);
   I3DMETHOD(DebugDraw)(PI3D_scene scene) const{
      Draw1(scene, NULL, true);
      return I3D_OK;
   }

   I3DMETHOD_(void,SetOn)(bool);
public:
   I3DMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   I3DMETHOD(Open)(const char *fname, dword flags, I3D_LOAD_CB_PROC* = NULL, void *context = NULL);
   I3DMETHOD_(bool,IsPlaying)() const;
                              //setup
   I3DMETHOD(SetSoundType)(I3D_SOUNDTYPE);
   I3DMETHOD_(void,SetRange)(float min, float max);
   I3DMETHOD_(void,SetCone)(float in_angle, float out_angle);
   I3DMETHOD_(void,SetOutVol)(float);
   I3DMETHOD_(void,SetVolume)(float);
   I3DMETHOD_(void,SetLoop)(bool);
   I3DMETHOD(SetStreaming)(bool);
   I3DMETHOD(SetCurrTime)(dword);
   I3DMETHOD(SetFrequency)(float);

   I3DMETHOD_(I3D_SOUNDTYPE,GetSoundType)() const{ return snd_type; }
   I3DMETHOD_(void,GetRange)(float &min, float &max) const;
   I3DMETHOD_(void,GetCone)(float &in_angle, float &out_angle) const;
   I3DMETHOD_(float,GetOutVol)() const{ return volume_out; }
   I3DMETHOD_(float,GetVolume)() const{ return volume; }
   I3DMETHOD_(bool,IsLoop)() const{ return (snd_flags&SNDF_LOOPED); }
   I3DMETHOD_(bool,IsStreaming)() const{ return (snd_flags&SNDF_STREAMING); }
   I3DMETHOD_(dword,GetCurrTime)() const;
   I3DMETHOD_(dword,GetPlayTime)() const;
   I3DMETHOD_(float,GetFrequency)() const{ return frequency; }

   I3DMETHOD_(const C_str&,GetFileName)() const{ return file_name; }
   I3DMETHOD_(void,StoreFileName)(const C_str &fn){ file_name = fn; }
   I3DMETHOD_(dword,GetOpenFlags)() const{ return open_flags; }
   I3DMETHOD_(class ISND_source*,GetSoundSource)() const{ return snd_src; }

   I3DMETHOD_(dword,NumSoundSectors)() const{ return sound_sectors.size(); }
   I3DMETHOD_(class I3D_sector* const*,GetSoundSectors)() const{ return &sound_sectors.front(); }
};

//----------------------------

#ifdef _DEBUG
inline PI3D_sound I3DCAST_SOUND(PI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_SOUND ? NULL : static_cast<PI3D_sound>(f); }
inline CPI3D_sound I3DCAST_CSOUND(CPI3D_frame f){ return !f ? NULL : f->GetType1()!=FRAME_SOUND ? NULL : static_cast<CPI3D_sound>(f); }
#else
inline PI3D_sound I3DCAST_SOUND(PI3D_frame f){ return static_cast<PI3D_sound>(f); }
inline CPI3D_sound I3DCAST_CSOUND(CPI3D_frame f){ return static_cast<CPI3D_sound>(f); }
#endif

//----------------------------

#endif
