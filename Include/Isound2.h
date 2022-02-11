#ifndef __SOUND_H
#define __SOUND_H
#pragma once

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Abstract:
//    Sound class - object capable of playing static and streaming sound files.
//----------------------------

#include <rules.h>

#define ISNDAPI
#define ISNDMETHOD_(type,method) virtual type ISNDAPI method
#define ISNDMETHOD(method) virtual ISND_RESULT ISNDAPI method

#pragma comment (lib,"isound2_thunk")

//----------------------------

struct S_wave_format{
   dword num_channels;        //number of channels, 1=mono, 2=stereo, 4=quadro
   dword bytes_per_sample;    //1, 2, 4, 8
   dword samples_per_sec;     //sound playback frequency
   dword size;                //size of data, in bytes
};

typedef S_wave_format *LPS_wave_format;

//----------------------------
                              //result codes
typedef long ISND_RESULT;

enum{
   ISND_OK,
   ISND_DONE,
   ISNDERR_NOFILE=0x80000001,
   ISNDERR_OUTOFMEM,
   ISNDERR_GENERIC,
   ISNDERR_NOTINITIALIZED,
   ISNDERR_NO3DHW,
   ISNDERR_CANTREUSE,
   ISNDERR_NOTLOADED,
   ISNDERR_INVALIDPARAM,
   ISNDERR_UNSUPPORTED,
   ISNDERR_BUFFERLOST,
   ISNDERR_ALREADYLOCKED,
   ISNDERR_NOTLOCKED,
};

#ifndef ISND_SUCCEESS
 #define ISND_SUCCESS(n) ((ISND_RESULT)n>=0)
#endif
#ifndef ISND_FAIL
 #define ISND_FAIL(n) ((ISND_RESULT)n<0)
#endif
//----------------------------

enum E_ISNDCAPS{              //caps indicies for ISND_driver::GetCaps
   ISNDCAPS_NUM_HW_ALL_BUFFERS,  //number of available hardware buffers
   ISNDCAPS_FREE_HW_ALL_BUFFERS, //number of free hardware buffers

   ISNDCAPS_NUM_3D_ALL_BUFFERS,  //number of available hardware 3D buffers
   ISNDCAPS_FREE_3D_ALL_BUFFERS, //number of free hardware 3D buffers

   ISNDCAPS_HW_MEM_ALL,          //total card memory in bytes
   ISNDCAPS_HW_MEM_FREE,         //free card memory in bytes
   ISNDCAPS_LAST
};

//----------------------------

#define ISC_3DENABLE    1     //enable 3D capabilities of sound
#define ISC_VERBOSE     2     //dump sound caps
#define ISC_WARN_OUT    4     //dump warning about unreleased interfaces at driver release
#define ISC_USE_EAX     8     //use EAX extensions, if available

#ifndef ISND_FULL
class ISND_driver{
public:
   ISNDMETHOD_(dword,AddRef)() = 0;
   ISNDMETHOD_(dword,Release)() = 0;

                              //playback
   ISNDMETHOD(InitBuffer)(int mix_freq, int num_chan, int bps) = 0;
   ISNDMETHOD(SetVolume)(float) = 0;

                              //info
   ISNDMETHOD(GetCaps)(dword[ISNDCAPS_LAST]) const = 0;

                              //new interface:
   ISNDMETHOD(CreateSource)(class ISND_source**) = 0;
   ISNDMETHOD(CreateSound)(class ISND_sound**) = 0;
   ISNDMETHOD(Create3DSound)(class ISND_3Dsound**) = 0;
   ISNDMETHOD(CreateMusic)(class ISND_music**) = 0;

   ISNDMETHOD_(struct IDirectSound8*,GetDS)() const = 0;
   ISNDMETHOD_(struct IDirectSoundBuffer*,GetDSPrimayrBuffer)() const = 0;

                              //listener properties:
                              // position
   ISNDMETHOD(SetListenerPos)(const struct S_vector&) = 0;
   ISNDMETHOD(SetListenerDir)(const struct S_vector &dir, const struct S_vector &up) = 0;
   ISNDMETHOD(SetListenerVelocity)(const struct S_vector&) = 0;
                              // settings
   ISNDMETHOD(SetListenerDistanceFactor)(float) = 0;
   ISNDMETHOD(SetListenerDopplerFactor)(float) = 0;
   ISNDMETHOD(SetListenerRolloffFactor)(float) = 0;

   ISNDMETHOD(SetEAXProperty)(int index, dword data) = 0;

   ISNDMETHOD(UpdateListener)() = 0;

   ISNDMETHOD_(float,GetVolume)() const = 0;
};
#endif

typedef class ISND_driver *PISND_driver;

//----------------------------
                              //init driver struct
struct ISND_INIT{
   int mix_frequency;
   int num_channels;
   int bits_per_second;
   dword flags;               //combination of ISC_??? flags
   void *hwnd;                
   void *guid;                //NULL = default device
   typedef void t_log_func(const char*);
   t_log_func *log_func;
};
typedef ISND_INIT *PISND_INIT;

                              //located in thunk static lib:
PISND_driver ISNDAPI ISoundCreate(PISND_INIT, bool use_debug_version);

//----------------------------
//----------------------------
                              //open/play flags:
#define ISND_STREAMING  1
#define ISND_NOVOLUME   2
#define ISND_NOPANNING  4
#define ISND_PRELOAD    8     //load into sysmem buffer, use as needed
#define ISND_FREQUENCY  0x10  //frequency adjustable
#define ISND_CARDMEM    0x20  //alloc buffer in hardware
#define ISND_SYSMEM     0x40  //alloc buffer in system memory
#define ISND_LOOP       0x100 //loop playback - for all Play methods, also for Open method in case of streaming source
#define ISND_3DBUFFER   0x200 //alloc buffer with 3D hardware capabilities
#define ISND_NOREWIND   0x400 //when starting play, don't rewind buffer

//----------------------------

#ifndef ISND_FULL
class ISND_source{
public:
   ISNDMETHOD_(dword,AddRef)() = 0;
   ISNDMETHOD_(dword,Release)() = 0;

   ISNDMETHOD(Open)(const char *filename, dword flags = 0) = 0;
   ISNDMETHOD_(void,Close)() = 0;
   ISNDMETHOD_(bool,IsOpen)() const = 0;

   ISNDMETHOD_(dword,GetPlayTime)() const = 0;

   ISNDMETHOD_(const S_wave_format*,GetFormat)() const = 0;
   ISNDMETHOD_(const char*,GetFileName)() const = 0;

   ISNDMETHOD(Lock)(void **area) = 0;
   ISNDMETHOD(Unlock)() = 0;
};
#endif

typedef class ISND_source *PISND_source;

//----------------------------

#ifndef ISND_FULL
class ISND_sound{
public:
   ISNDMETHOD_(dword,AddRef)() = 0;
   ISNDMETHOD_(dword,Release)() = 0;

   ISNDMETHOD(Open)(PISND_source, dword flags = 0) = 0;
   ISNDMETHOD(Close)() = 0;
   ISNDMETHOD(PlaySource)(PISND_source, dword flags = 0) = 0;
   ISNDMETHOD(Play)(dword flags = 0) = 0;
   ISNDMETHOD(Stop)() = 0;

   ISNDMETHOD_(dword,GetCurrTime)() const = 0;
   ISNDMETHOD(SetCurrTime)(dword) = 0;

   ISNDMETHOD(SetVolume)(float) = 0;
   ISNDMETHOD(SetFrequency)(float) = 0;
   ISNDMETHOD(SetPanning)(float) = 0;
   ISNDMETHOD_(bool,IsPlaying)() const = 0;

   ISNDMETHOD_(struct IDirectSoundBuffer*,GetDSBuffer)() const = 0;
};
#endif

typedef class ISND_sound *PISND_sound;

//----------------------------

#ifndef ISND_FULL
class ISND_3Dsound: public ISND_sound{
public:
   ISNDMETHOD_(bool,Is3DInit)() const = 0;
                              //position
   ISNDMETHOD(SetPos)(const struct S_vector&) = 0;
   ISNDMETHOD(SetDir)(const struct S_vector&) = 0;
   ISNDMETHOD(SetVelocity)(const struct S_vector&) = 0;

                              //setting s
   ISNDMETHOD(SetDistances)(float min, float max) = 0;
   ISNDMETHOD(SetCone)(float in_angle, float out_angle) = 0;
   ISNDMETHOD(SetOutVolume)(float out_vol) = 0;

   ISNDMETHOD_(struct IDirectSound3DBuffer*,GetDS3DBuffer)() const = 0;
};
#endif

typedef class ISND_3Dsound *PISND_3Dsound;

//----------------------------

#ifndef TRACKER_FULL
class ISND_music{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;
   virtual bool Open(const char *name) = 0;
   virtual bool Close() = 0;
   virtual bool Play() = 0;
   virtual void Stop() = 0;
   virtual bool IsOpen() const = 0;

   virtual bool SetPlayTrack(int track) = 0;
   virtual void SetLoop(bool) = 0;
   virtual void SetVolume(float) = 0;

   virtual bool IsLoop() const = 0;
   virtual float GetVolume() const = 0;
   virtual int GetNumChannels() const = 0;
   virtual int GetNumPositions() const = 0;
   virtual int GetNumPatterns() const = 0;
   virtual int GetNumTracks() const = 0;
   virtual int GetNumInstruments() const = 0;
   virtual const char *GetSongName() const = 0;
   virtual int GetPlayTrack() const = 0;
};
#endif

typedef class ISND_music *PISND_music;

//----------------------------

#endif
