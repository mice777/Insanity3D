#ifndef __SOUND_I_H_
#define __SOUND_I_H_

#include "loader.h"
#include <DSound.h>

using namespace std;

//#define USE_EAX               //enable to use EAX environmental audio

#ifdef USE_EAX
#include <eax.h>
#endif

#ifndef NDEBUG
void DS_Fatal(const char *text, dword hr);
#define CHECK_DS_RESULT(text, hr) if(FAILED(hr)) DS_Fatal(text, hr);
#else
#define DS_Fatal(a, b)
#define CHECK_DS_RESULT(text, hr)
#endif

//----------------------------


#define SO_3DENABLE  8        //internal
//----------------------------

extern"C"{
PISND_driver __declspec(dllexport) ISoundCreate(PISND_INIT);
}

//----------------------------
//----------------------------

class ISND_3Dlistener{
   dword ref;

#define LSNF_DIRTY      1     //need to call SetAllParameters

                              //DSound stuff
   LPDIRECTSOUND3DLISTENER8 ds_listener8;
   DS3DLISTENER ds_data;
   dword lsnr_flags;

   PISND_driver drv;
   S_vector last_pos;

//----------------------------
// Check if DirectSound interface is initialized.
   inline bool IsHardware() const{
      return (ds_listener8 != NULL);
   }
public:
   ISND_3Dlistener(PISND_driver);
   ~ISND_3Dlistener();
   ISND_RESULT Initialize();
public:
   ISNDMETHOD_(dword,AddRef)(){ return ++ref; }
   ISNDMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
                              //position
   ISNDMETHOD(SetPos)(const struct S_vector&);
   ISNDMETHOD(SetDir)(const struct S_vector &dir, const struct S_vector &up);
   ISNDMETHOD(SetVelocity)(const struct S_vector&);
                              //settings
   ISNDMETHOD(SetDistanceFactor)(float);
   ISNDMETHOD(SetDopplerFactor)(float);
   ISNDMETHOD(SetRolloffFactor)(float);

   ISNDMETHOD(SetEAXProperty)(int index, dword data);

   ISNDMETHOD(Update)();
};

typedef ISND_3Dlistener *PISND_3Dlistener;

//----------------------------
//----------------------------

class ISND_driver{
   dword ref;

   HINSTANCE h_dsound;        //dsound.dll

   DSCAPS dssc;

   IDirectSound8 *lpDS8;
   IDirectSoundBuffer *lpDSP; //primary sound buffer
   C_smart_ptr<ISND_3Dlistener> ds_listener;

   dword create_flags;        //creation flags
   dword sound_caps;
   float volume;              //global volume
   friend PISND_driver __declspec(dllexport) ISoundCreate(PISND_INIT);

   vector<PISND_sound> created_sounds;

   ISND_driver();

   ISND_RESULT Initialize(void *hwnd, int mix_freq, int num_chan, int bps, dword flags, ISND_INIT::t_log_func* = NULL);
   ISND_RESULT InitBuffer1(int mix_freq, int num_chan, int bps, dword flags);
public:
#ifdef USE_EAX
                              //EAX extension:
   C_smart_ptr<IDirectSound3DBuffer8> ds_property;
   C_smart_ptr<IKsPropertySet> eax_property_set;
#endif
   CRITICAL_SECTION lock;

   void RemoveSound(PISND_sound);

   ~ISND_driver();
   //short *vol_tab;
   short vol_tab[65];

   bool IsInit() const;
   inline IDirectSound8 *GetDSInterface() const{ return lpDS8; }
   inline IDirectSoundBuffer *GetDSPrimaryBuffer() const{ return lpDSP; }
   inline dword GetSoundCaps() const{ return sound_caps; }
public:
   ISNDMETHOD_(dword,AddRef)(){ return ++ref; }
   ISNDMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   ISNDMETHOD(InitBuffer)(int mix_freq, int num_chan, int bps);
   ISNDMETHOD(SetVolume)(float);

   ISNDMETHOD(GetCaps)(dword[6]) const;

   ISNDMETHOD(CreateSource)(PISND_source*);
   ISNDMETHOD(CreateSound)(PISND_sound*);
   ISNDMETHOD(Create3DSound)(PISND_3Dsound*);
   ISNDMETHOD(CreateMusic)(PISND_music*);

   ISNDMETHOD_(struct IDirectSound8*,GetDS)() const{ return lpDS8; }
   ISNDMETHOD_(struct IDirectSoundBuffer*,GetDSPrimayrBuffer)() const{ return lpDSP; }

                              //listener properties:
                              //position
   ISNDMETHOD(SetListenerPos)(const S_vector &pos){
      return ds_listener ? ds_listener->SetPos(pos) : ISNDERR_NOTINITIALIZED;
   }
   ISNDMETHOD(SetListenerDir)(const S_vector &dir, const S_vector &up){
      return ds_listener ? ds_listener->SetDir(dir, up) : ISNDERR_NOTINITIALIZED;
   }
   ISNDMETHOD(SetListenerVelocity)(const S_vector &v){
      return ds_listener ? ds_listener->SetVelocity(v) : ISNDERR_NOTINITIALIZED;
   }
                              //settings
   ISNDMETHOD(SetListenerDistanceFactor)(float f){
      return ds_listener ? ds_listener->SetDistanceFactor(f) : ISNDERR_NOTINITIALIZED;
   }
   ISNDMETHOD(SetListenerDopplerFactor)(float f){
      return ds_listener ? ds_listener->SetDopplerFactor(f) : ISNDERR_NOTINITIALIZED;
   }
   ISNDMETHOD(SetListenerRolloffFactor)(float f){
      return ds_listener ? ds_listener->SetRolloffFactor(f) : ISNDERR_NOTINITIALIZED;
   }

   ISNDMETHOD(SetEAXProperty)(int index, dword data){
      return ds_listener ? ds_listener->SetEAXProperty(index, data) : ISNDERR_NOTINITIALIZED;
   }

   ISNDMETHOD(UpdateListener)(){
      return ds_listener ? ds_listener->Update() : ISNDERR_NOTINITIALIZED;
   }

   ISNDMETHOD_(float,GetVolume)() const{ return volume; }
};

typedef ISND_driver *PISND_driveri;

//----------------------------
//----------------------------

class ISND_source{
   dword ref;

#define SRCF_OPEN       1     //source contains data
#define SRCF_LOCKED     2     //interface is currently locked

   C_str filename;
   S_wave_format wf;
   PISND_driver drv;

                              //open data in various formats:
   LPDIRECTSOUNDBUFFER8 lpDSB8; //DS buffer with open data
   void *save_lock_ptr1, *save_lock_ptr2;
   dword save_lick_size1, save_lick_size2;

   LPC_sound_loader strm_loader; //loader capable to load streaming sound
   byte *data; dword data_size;  //sys-mem data

   friend class ISND_sound;

   dword open_flags;
   dword src_flags;

   ISND_RESULT OpenWave(const char *fname, LPS_wave_format wf, LPC_sound_loader &loader, dword &loader_handle);
public:
   ISND_source(PISND_driver);
   ~ISND_source();
public:
   ISNDMETHOD_(dword,AddRef)(){ return ++ref; }
   ISNDMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   ISNDMETHOD(Open)(const char *filename, dword flags = 0);
   ISNDMETHOD_(void,Close)();
   ISNDMETHOD_(bool,IsOpen)() const{ return (src_flags&SRCF_OPEN); }

   ISNDMETHOD_(dword,GetPlayTime)() const;

   ISNDMETHOD_(const S_wave_format*,GetFormat)() const{ return IsOpen() ? &wf : NULL; }
   ISNDMETHOD_(const char*,GetFileName)() const{ return IsOpen() ? filename : NULL; }

   ISNDMETHOD(Lock)(void **area);
   ISNDMETHOD(Unlock)();
};

//----------------------------

class ISND_sound{
#define SNDF_STREAM_LOOP      1
#define SNDF_ADJ_FREQUENCY    2     //frequency adjustable

protected:
   dword ref;

   PISND_driver drv;
   LPDIRECTSOUNDBUFFER8 lpDSB8; //DS buffer which is used for playback

   float volume;
private:
   float panning, freq_mult;
   S_wave_format wf;
   dword play_time;

                              //streaming playback - keep isloated from other sound stuff
   struct S_stream{
      LPC_sound_loader loader;
      int load_handle;
      C_smart_ptr<IDirectSoundNotify8> DSnotify8;//DS notify object, for which we're waiting
      HANDLE h_notify;        //event handle (DS-notify)
      HANDLE h_thread;        //thread handle
      dword id_thread;        //thread ID
      dword buffer_half;      //next half of buffer to be read to, toggles between 0 and 1
      dword file_pos;         //current file position
      bool thread_done;       //true when thread has exited
      dword buf_size;

      S_stream():
         loader(NULL), load_handle(0),
         buffer_half(0),
         thread_done(false),
         file_pos(0),
         buf_size(0),
         h_notify(NULL),
         h_thread(NULL)
      {
      }
      S_stream(const S_stream&);
      S_stream &operator =(const S_stream&);
      ~S_stream(){
         bool b;
         if(!thread_done){
            b = TerminateThread(h_thread, 0);
            assert(b);
         }
         b = CloseHandle(h_notify);
         assert(b);
         b = CloseHandle(h_thread);
         assert(b);
         if(loader)
            loader->Close(load_handle);
      }
   } *stream;

//----------------------------

   void NotifyThreadStream();

   static dword CALLBACK NotifyThreadStreamThunk(void *c){
      ((PISND_sound)c)->NotifyThreadStream();
      return 0;
   }

//----------------------------
   ISND_RESULT ReadStream(dword total_size);

   dword snd_flags;
public:
   ISND_sound(PISND_driver);
   ~ISND_sound();

   inline float GetVolume() const{ return volume; }
public:
   ISNDMETHOD_(dword,AddRef)(){ return ++ref; }
   ISNDMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   ISNDMETHOD(Open)(PISND_source, dword flags = 0);
   ISNDMETHOD(Close)();
   ISNDMETHOD(PlaySource)(PISND_source, dword flags = 0);
   ISNDMETHOD(Play)(dword flags = 0);
   ISNDMETHOD(Stop)();

   ISNDMETHOD_(dword,GetCurrTime)() const;
   ISNDMETHOD(SetCurrTime)(dword);

   ISNDMETHOD(SetVolume)(float);
   ISNDMETHOD(SetFrequency)(float);
   ISNDMETHOD(SetPanning)(float);
   ISNDMETHOD_(bool,IsPlaying)() const;

   ISNDMETHOD_(struct IDirectSoundBuffer*,GetDSBuffer)() const{
      return lpDSB8;
   }
};

//----------------------------

class ISND_3Dsound: public ISND_sound{

   IDirectSound3DBuffer8 *lpDSB3D8; //3D sound interface
   DS3DBUFFER ds_data;
public:
   ISND_3Dsound(PISND_driver);
   ~ISND_3Dsound();
public:

   ISNDMETHOD(Open)(PISND_source, dword flags = 0);
   ISNDMETHOD(Close)();
   ISNDMETHOD(SetVolume)(float);
public:
   ISNDMETHOD_(dword,AddRef)(){ return ++ref; }
   ISNDMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   ISNDMETHOD_(bool,Is3DInit)() const{ return (lpDSB3D8 != NULL); }
                              //position
   ISNDMETHOD(SetPos)(const struct S_vector&);
   ISNDMETHOD(SetDir)(const struct S_vector&);
   ISNDMETHOD(SetVelocity)(const struct S_vector&);

                              //settings
   ISNDMETHOD(SetDistances)(float min, float max);
   ISNDMETHOD(SetCone)(float in_angle, float out_angle);
   ISNDMETHOD(SetOutVolume)(float out_vol);

   ISNDMETHOD_(struct IDirectSound3DBuffer*,GetDS3DBuffer)() const{
      return lpDSB3D8;
   }
};

//----------------------------

#endif   //__SOUND_I_H_
