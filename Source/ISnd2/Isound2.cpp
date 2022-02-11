#include "all.h"
#include "sound_i.h"
//#include "music\c_music.h"

#ifdef _MSC_VER
using namespace std;
#endif

//#define DS_APPLY_FLAGS DS3D_IMMEDIATE 
#define DS_APPLY_FLAGS DS3D_DEFERRED

//#define DEBUG_FORCE_SYSMEM    //force all sounds to be created in systemmem

//----------------------------

#define STREAM_LENGTH 1.5f       //length of stream buffer in sec

#define STREAM_BUFFER_SIZE(freq, bytes_per_sample) (((int)(freq*bytes_per_sample*STREAM_LENGTH)) & -16)

//----------------------------
                              //sound caps (internal)
#define SCAPS_INIT   1        //initialized system
#define SCAPS_3DINIT 2        //3D controlling on primary buffer enabled

//----------------------------

#if defined _DEBUG || 0
//#define DEBUG_LOG(n) {OutputDebugString(n); OutputDebugString("\n"); }
#define DEBUG_LOG(n)
#else 
#define DEBUG_LOG(n)
#endif

//----------------------------

#ifndef NDEBUG

void DS_Fatal(const char *text, dword hr){
   char buf[256];
   char *msgp;

   switch(hr){
   case DSERR_ALLOCATED: msgp="Allready Allocated"; break;
   case DSERR_ALREADYINITIALIZED: msgp="Allready Initialized"; break;
   case DSERR_BADFORMAT: msgp="Bad Format"; break;
   case DSERR_BUFFERLOST: msgp="Buffer Lost"; break;
   case DSERR_CONTROLUNAVAIL: msgp="Control Unavailable"; break;
   case DSERR_GENERIC: msgp="Generic"; break;
   case DSERR_INVALIDCALL: msgp="Invalid Call"; break;
   case DSERR_INVALIDPARAM: msgp="Invalid Parameter"; break;
   case DSERR_NOAGGREGATION: msgp="No Aggregation"; break;
   case DSERR_NODRIVER: msgp="No Driver"; break;
   case DSERR_OUTOFMEMORY: msgp="Out Of Memory"; break;
   case DSERR_PRIOLEVELNEEDED: msgp="Priority Level Needed"; break;
   case DSERR_UNINITIALIZED: msgp="Uninitialized"; break;
   case DSERR_UNSUPPORTED: msgp="Unsupported"; break;
   default: msgp="Unknown Error";
   }
   wsprintf(buf, "DirectSound: %s\n%s", text, msgp);
   MessageBox(NULL, buf, "Fatal Error", MB_OK);
   //exit(3);
}

#else

#define DS_Fatal(a,b)

#endif

//----------------------------
                              //help functions
static short Lin2Log(byte val){

   if(!val)
      return 0;
                              // 1..64 -> 0..10000 logarithmic
   return (short)(10000.0f / (16*.69314718) * log(float(val*1024)));
}

//----------------------------

inline short VolConv(byte vol){
   return short(-10000 + Lin2Log(vol));
}

//----------------------------
                              //registered loaders
#define MAX_PLUGINS 32
static LPC_sound_loader load_plugins[MAX_PLUGINS];
static int num_load_plugins;

void __cdecl RegisterSoundLoader(LPC_sound_loader sl){
   assert(num_load_plugins < MAX_PLUGINS);
   load_plugins[num_load_plugins++] = sl;
}

//----------------------------
//----------------------------

extern"C"
PISND_driver __declspec(dllexport) ISoundCreate(PISND_INIT in){
   
   PISND_driver is = new ISND_driver;
   if(!is)
      return false;
   if(ISND_FAIL(is->Initialize(in->hwnd, in->mix_frequency, in->num_channels, in->bits_per_second, in->flags, 
      in->log_func))){

      is->Release();
      is = NULL;
   }
   return is;

}

//----------------------------
//----------------------------

static void DumpCaps(const DSCAPS &dssc, ISND_INIT::t_log_func *log_func){

   {
      (*log_func)("----------------------\n");
      (*log_func)("Sound Caps:\n");
      dword dw = dssc.dwFlags;
      if(dw&DSCAPS_CERTIFIED) (*log_func)(" DSCAPS_CERTIFIED\n");
      if(dw&DSCAPS_CONTINUOUSRATE) (*log_func)(" DSCAPS_CONTINUOUSRATE\n");
      if(dw&DSCAPS_EMULDRIVER) (*log_func)(" DSCAPS_EMULDRIVER\n");
      if(dw&DSCAPS_PRIMARY16BIT) (*log_func)(" DSCAPS_PRIMARY16BIT\n");
      if(dw&DSCAPS_PRIMARY8BIT) (*log_func)(" DSCAPS_PRIMARY8BIT\n");
      if(dw&DSCAPS_PRIMARYMONO) (*log_func)(" DSCAPS_PRIMARYMONO\n");
      if(dw&DSCAPS_PRIMARYSTEREO) (*log_func)(" DSCAPS_PRIMARYSTEREO\n");
      if(dw&DSCAPS_SECONDARY16BIT) (*log_func)(" DSCAPS_SECONDARY16BIT\n");
      if(dw&DSCAPS_SECONDARY8BIT) (*log_func)(" DSCAPS_SECONDARY8BIT\n");
      if(dw&DSCAPS_SECONDARYMONO) (*log_func)(" DSCAPS_SECONDARYMONO\n");
      if(dw&DSCAPS_SECONDARYSTEREO) (*log_func)(" DSCAPS_SECONDARYSTEREO\n");
   }
   {
      (*log_func)(C_fstr("dwMinSecondarySampleRate: %u\n", dssc.dwMinSecondarySampleRate));
      (*log_func)(C_fstr("dwMaxSecondarySampleRate: %u\n", dssc.dwMaxSecondarySampleRate));
      (*log_func)(C_fstr("dwPrimaryBuffers: %u\n", dssc.dwPrimaryBuffers));
      (*log_func)(C_fstr("dwMaxHwMixingAllBuffers: %u\n", dssc.dwMaxHwMixingAllBuffers));
      (*log_func)(C_fstr("dwMaxHwMixingStaticBuffers: %u\n", dssc.dwMaxHwMixingStaticBuffers));
      (*log_func)(C_fstr("dwMaxHwMixingStreamingBuffers: %u\n", dssc.dwMaxHwMixingStreamingBuffers));
      (*log_func)(C_fstr("dwFreeHwMixingAllBuffers: %u\n", dssc.dwFreeHwMixingAllBuffers));
      (*log_func)(C_fstr("dwFreeHwMixingStaticBuffers: %u\n", dssc.dwFreeHwMixingStaticBuffers));
      (*log_func)(C_fstr("dwFreeHwMixingStreamingBuffers: %u\n", dssc.dwFreeHwMixingStreamingBuffers));
      (*log_func)(C_fstr("dwMaxHw3DAllBuffers: %u\n", dssc.dwMaxHw3DAllBuffers));
      (*log_func)(C_fstr("dwMaxHw3DStaticBuffers: %u\n", dssc.dwMaxHw3DStaticBuffers));
      (*log_func)(C_fstr("dwMaxHw3DStreamingBuffers: %u\n", dssc.dwMaxHw3DStreamingBuffers));
      (*log_func)(C_fstr("dwFreeHw3DAllBuffers: %u\n", dssc.dwFreeHw3DAllBuffers));
      (*log_func)(C_fstr("dwFreeHw3DStaticBuffers: %u\n", dssc.dwFreeHw3DStaticBuffers));
      (*log_func)(C_fstr("dwFreeHw3DStreamingBuffers: %u\n", dssc.dwFreeHw3DStreamingBuffers));
      (*log_func)(C_fstr("dwTotalHwMemBytes: %u\n", dssc.dwTotalHwMemBytes));
      (*log_func)(C_fstr("dwFreeHwMemBytes: %u\n", dssc.dwFreeHwMemBytes));
      (*log_func)(C_fstr("dwMaxContigFreeHwMemBytes: %u\n", dssc.dwMaxContigFreeHwMemBytes));
      (*log_func)(C_fstr("dwUnlockTransferRateHwBuffers: %u\n", dssc.dwUnlockTransferRateHwBuffers));
      (*log_func)(C_fstr("dwPlayCpuOverheadSwBuffers: %u\n", dssc.dwPlayCpuOverheadSwBuffers));
      (*log_func)("----------------------\n");
   }
}

//----------------------------

ISND_driver::ISND_driver():
   ref(1),
   lpDS8(NULL),
   lpDSP(NULL),
   sound_caps(0),
   volume(1.0f),
   h_dsound(NULL)
{
   InitializeCriticalSection(&lock);
}

//----------------------------

ISND_driver::~ISND_driver(){

   if(!IsInit())
      return;

#ifdef USE_EAX
   ds_property = NULL;
   eax_property_set = NULL;
#endif
   ds_listener = NULL;
                              //release primary sound buffer
   if(lpDSP){
      lpDSP->Stop();
      lpDSP->Release();
      lpDSP = NULL;
   }
                              //release DSound interface
   if(lpDS8){
      lpDS8->Release();
      lpDS8 = NULL;
   }
   sound_caps = 0;

   FreeLibrary(h_dsound);
   h_dsound = NULL;

   DeleteCriticalSection(&lock);
}

//----------------------------

ISND_RESULT ISND_driver::Initialize(void *hwnd, int mix_freq, int num_chan,
   int bps, dword flags, ISND_INIT::t_log_func *log_func){

   HRESULT hr = DS_OK;

                              //load dsound dll
   h_dsound = LoadLibraryEx("dsound.dll", NULL, 0);
   if(h_dsound == NULL){
      DS_Fatal("Unable to find dsound.dll", hr);
      return ISNDERR_GENERIC;
   }

   FARPROC fp = GetProcAddress(h_dsound, "DirectSoundCreate8");
   if(!fp){
      DS_Fatal("Unable to find locate function \"DirectSoundCreate8\" in dsound.dll.", hr);
      FreeLibrary(h_dsound);
      h_dsound = NULL;
      goto fail;
   }

                              //create main DS object
   typedef HRESULT WINAPI fnc_type(LPGUID, LPDIRECTSOUND8*, LPUNKNOWN);
   hr = (*(fnc_type*)fp)(NULL, &lpDS8, NULL);
   if(FAILED(hr))
      goto fail;
                              //get caps
   memset(&dssc, 0, sizeof(dssc));
   dssc.dwSize = sizeof(dssc);
   hr = lpDS8->GetCaps(&dssc);
   if((flags&ISC_VERBOSE) && log_func)
      DumpCaps(dssc, log_func);
   hr = lpDS8->SetCooperativeLevel((HWND)hwnd, DSSCL_EXCLUSIVE);
   if(SUCCEEDED(hr)){
      ISND_RESULT ir = InitBuffer1(mix_freq, num_chan, bps, flags);
      if(ISND_SUCCESS(ir)){
         //vol_tab = new short[65];
         for(int i=0; i<65; i++)
            vol_tab[i] = VolConv(byte(i));
         sound_caps |= SCAPS_INIT;
         create_flags = flags;

                              //create and init listener
         ds_listener = new ISND_3Dlistener(this);
         ds_listener->Release();
         return ISND_OK;
      }
   }
   lpDS8->Release();
   lpDS8 = NULL;

fail:
   sound_caps = 0;
   CHECK_DS_RESULT("ISND_driver::Initialize", hr);
   return ISNDERR_GENERIC;
}

//----------------------------

ISND_RESULT ISND_driver::GetCaps(dword dw[ISNDCAPS_LAST]) const{

   DSCAPS c;

   assert(lpDS8);
   if(!lpDS8)
      return ISNDERR_NOTINITIALIZED;

   c.dwSize = sizeof(c);
   HRESULT hr = lpDS8->GetCaps(&c);
   if(FAILED(hr))
      return ISNDERR_UNSUPPORTED;

#ifdef DEBUG_FORCE_SYSMEM
   memset(dw, 0, sizeof(dword)*ISNDCAPS_LAST);
#else
   dw[ISNDCAPS_NUM_HW_ALL_BUFFERS] = c.dwMaxHwMixingAllBuffers;
   dw[ISNDCAPS_FREE_HW_ALL_BUFFERS] = c.dwFreeHwMixingAllBuffers;
   dw[ISNDCAPS_NUM_3D_ALL_BUFFERS] = c.dwMaxHw3DAllBuffers;
   dw[ISNDCAPS_FREE_3D_ALL_BUFFERS] = c.dwFreeHw3DAllBuffers;
   dw[ISNDCAPS_HW_MEM_ALL] = c.dwTotalHwMemBytes;
   dw[ISNDCAPS_HW_MEM_FREE] = c.dwFreeHwMemBytes;
#endif
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_driver::InitBuffer1(int mix_freq, int num_chan, int bps, dword flags){

   EnterCriticalSection(&lock);
#ifdef USE_EAX
                              //release EAX support
   ds_property = NULL;
   eax_property_set = NULL;
#endif
                              //release old primary (if any)
   if(lpDSP){
      lpDSP->Stop();
      lpDSP->Release();
      lpDSP = NULL;
   }
                              //set up DSBUFFERDESC structure.
   DSBUFFERDESC dsbdesc;
   memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
   dsbdesc.dwSize = sizeof(dsbdesc);
   dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
   if(flags&ISC_3DENABLE){
      dsbdesc.dwFlags |= DSBCAPS_CTRL3D;
      sound_caps |= SCAPS_3DINIT;
   }
   //dsbdesc.dwFlags |= DSBCAPS_STICKYFOCUS;
                              //buffer size is determined by sound hardware.
   dsbdesc.dwBufferBytes = 0;
                              //must be NULL for primary buffers.
   dsbdesc.lpwfxFormat = NULL;

   HRESULT hr;
   hr = lpDS8->CreateSoundBuffer(&dsbdesc, &lpDSP, NULL);
   if(SUCCEEDED(hr)){
                                       //set up wave format structure.
      WAVEFORMATEX pcmwf;
      memset(&pcmwf, 0, sizeof(pcmwf));
      pcmwf.cbSize = sizeof(pcmwf);

      pcmwf.wFormatTag = WAVE_FORMAT_PCM;
      pcmwf.nChannels = word(num_chan);
      pcmwf.nSamplesPerSec = mix_freq;
      pcmwf.wBitsPerSample = word(bps);
      pcmwf.nBlockAlign = word(pcmwf.wBitsPerSample/8 * pcmwf.nChannels);
      pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec*pcmwf.nBlockAlign;
      hr = lpDSP->SetFormat(&pcmwf);

      if(SUCCEEDED(hr)){
         lpDSP->Play(0, 0, DSBPLAY_LOOPING);

#ifdef USE_EAX
         if((flags&ISC_3DENABLE) && (flags&ISC_USE_EAX)){
                              //create dummy buffer for property set
            LPDIRECTSOUNDBUFFER ds_property1;
            DSBUFFERDESC dsds;
            memset(&dsds, 0, sizeof(dsds));
            dsds.dwSize = sizeof(dsds);
            dsds.dwFlags = DSBCAPS_LOCHARDWARE | DSBCAPS_STATIC | DSBCAPS_CTRL3D;
            dsds.dwBufferBytes = DSBSIZE_MIN;

            WAVEFORMATEX wf;
            memset(&wf, 0, sizeof(wf));
            wf.cbSize = sizeof(wf);

            wf.wFormatTag = WAVE_FORMAT_PCM;
            wf.nChannels = 1;
            wf.nSamplesPerSec = 11025;
            wf.nAvgBytesPerSec = 11025;
            wf.nBlockAlign = 1;
            wf.wBitsPerSample = 8;
            dsds.lpwfxFormat = &wf;
            hr = lpDS8->CreateSoundBuffer(&dsds, &ds_property1, NULL);
            if(SUCCEEDED(hr)){
                                    //query for 3D sound buffer
               IDirectSound3DBuffer8 *dsb;
               hr = ds_property1->QueryInterface(IID_IDirectSound3DBuffer, (void**)&dsb);
               if(SUCCEEDED(hr)){
                                    //query for property set object
                  IKsPropertySet *ps;
                  hr = dsb->QueryInterface(IID_IKsPropertySet, (void**)&ps);
                  if(SUCCEEDED(hr)){
                  
                     dword support = 0;
                     hr = ps->QuerySupport(DSPROPSETID_EAX30_ListenerProperties,
                        DSPROPERTY_EAXLISTENER_ALLPARAMETERS, &support);
                     //CHECK_DS_RESULT("QuerySupport", hr);
                     if(SUCCEEDED(hr) && (support&(KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET)) == (KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET)){
                        hr = ps->QuerySupport(DSPROPSETID_EAX30_BufferProperties,
                           DSPROPERTY_EAXBUFFER_ALLPARAMETERS, &support);
                     }
                     if(SUCCEEDED(hr) && (support&(KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET)) == (KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET)){
                        eax_property_set = ps;
                        ds_property = dsb;
                     }
                     ps->Release();
                  }
                  dsb->Release();
               }
               ds_property1->Release();
            }
         }
#endif//USE_EAX

         LeaveCriticalSection(&lock);

         return ISND_OK;
      }
      lpDSP->Release();
   }
   lpDSP = NULL;

   LeaveCriticalSection(&lock);

   return ISNDERR_GENERIC;
}

//----------------------------

ISND_RESULT ISND_driver::InitBuffer(int mix_freq, int num_chan, int bps){

   return InitBuffer1(mix_freq, num_chan, bps, create_flags);
}

//----------------------------

ISND_RESULT ISND_driver::SetVolume(float v){

   if(v<0.0f || v>1.0f)
      return ISNDERR_INVALIDPARAM;
   volume = v;
   for(int i=created_sounds.size(); i--; ){
      PISND_sound snd = created_sounds[i];
      snd->SetVolume(snd->GetVolume());
   }

   return ISND_OK;
}

//----------------------------

bool ISND_driver::IsInit() const{

   return (lpDS8 != NULL);
}

//----------------------------

ISND_RESULT ISND_driver::CreateMusic(PISND_music *mus){
   return ISNDERR_UNSUPPORTED;
}

//----------------------------
//----------------------------
                              //listener interface
ISND_3Dlistener::ISND_3Dlistener(PISND_driver drv1):
   ref(1),
   ds_listener8(NULL),
   lsnr_flags(0),
   drv(drv1)
{
   Initialize();
}

//----------------------------

ISND_3Dlistener::~ISND_3Dlistener(){

   if(IsHardware()){
      ds_listener8->Release();
      ds_listener8 = NULL;
      drv = NULL;
   }
}

//----------------------------

ISND_RESULT ISND_3Dlistener::Initialize(){

   LPDIRECTSOUNDBUFFER lpPSB = drv->GetDSPrimaryBuffer();
   if(!lpPSB)
      return ISNDERR_GENERIC;
                              //query for listener
   HRESULT hr;
   hr = lpPSB->QueryInterface(IID_IDirectSound3DListener8, (void**)&ds_listener8);
   if(FAILED(hr))
      return ISNDERR_GENERIC;
   ds_data.dwSize = sizeof(ds_data);
   hr = ds_listener8->GetAllParameters(&ds_data);
   CHECK_DS_RESULT("GetAllParameters", hr);

   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dlistener::SetPos(const S_vector &p){

   (S_vector&)ds_data.vPosition = p;
   lsnr_flags |= LSNF_DIRTY;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dlistener::SetDir(const S_vector &dir, const S_vector &up){

   (S_vector&)ds_data.vOrientFront = dir;
   (S_vector&)ds_data.vOrientTop = up;
   lsnr_flags |= LSNF_DIRTY;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dlistener::SetVelocity(const S_vector &v){

   (S_vector&)ds_data.vVelocity = v;
   lsnr_flags |= LSNF_DIRTY;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dlistener::SetDistanceFactor(float f){

   ds_data.flDistanceFactor = f;
   lsnr_flags |= LSNF_DIRTY;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dlistener::SetDopplerFactor(float f){

   ds_data.flDopplerFactor = Max(DS3D_MINDOPPLERFACTOR, Min(DS3D_MAXDOPPLERFACTOR, f));
   lsnr_flags |= LSNF_DIRTY;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dlistener::SetRolloffFactor(float f){

   ds_data.flRolloffFactor = Max(DS3D_MINROLLOFFFACTOR, Min(DS3D_MAXROLLOFFFACTOR, f));
   lsnr_flags |= LSNF_DIRTY;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dlistener::SetEAXProperty(int index, dword data){

#ifdef USE_EAX
   if(!drv->eax_property_set)
      return ISNDERR_NOTINITIALIZED;

   HRESULT hr;
   hr = drv->eax_property_set->Set(DSPROPSETID_EAX30_ListenerProperties,
      //index | DSPROPERTY_EAXLISTENER_DEFERRED,
      index,
      NULL, 0, &data, sizeof(dword));
   CHECK_DS_RESULT("SetEAXProperty", hr);
   return ISND_OK;
#else
   return ISNDERR_UNSUPPORTED;
#endif

}

//----------------------------

ISND_RESULT ISND_3Dlistener::Update(){

   EnterCriticalSection(&drv->lock);

   HRESULT hr;
   if(lsnr_flags&LSNF_DIRTY){
      if(IsHardware()){
         hr = ds_listener8->SetAllParameters(&ds_data, DS_APPLY_FLAGS);
         CHECK_DS_RESULT("SetAllParameters", hr);
      }
      lsnr_flags &= ~LSNF_DIRTY;
   }
   if(IsHardware()){
      hr = ds_listener8->CommitDeferredSettings();
      CHECK_DS_RESULT("CommitDeferredSettings", hr);
   }

   LeaveCriticalSection(&drv->lock);

   return ISND_OK;
}

//----------------------------
//----------------------------

ISND_RESULT ISND_driver::CreateSource(PISND_source *ret){

   *ret = new ISND_source(this);
   return (*ret) ? ISND_OK : ISNDERR_OUTOFMEM;
}

//----------------------------

ISND_RESULT ISND_driver::CreateSound(PISND_sound *ret){

   PISND_sound snd = new ISND_sound(this);
   if(snd) created_sounds.push_back(snd);
   *ret = snd;
   return (*ret) ? ISND_OK : ISNDERR_OUTOFMEM;
}

//----------------------------

void ISND_driver::RemoveSound(PISND_sound snd){

   for(int i=created_sounds.size(); i--; ){
      if(created_sounds[i]==snd){
         created_sounds[i] = created_sounds.back();
         created_sounds.pop_back();
         return;
      }
   }
   assert(0);
}

//----------------------------

ISND_RESULT ISND_driver::Create3DSound(PISND_3Dsound *ret){

   PISND_3Dsound snd = new ISND_3Dsound(this);
   if(snd) created_sounds.push_back(snd);
   *ret = snd;
   return (*ret) ? ISND_OK : ISNDERR_OUTOFMEM;
}

//----------------------------
//----------------------------

static HRESULT CreateDSBuffer(LPDIRECTSOUND8 lpDS, S_wave_format &wf, dword flags, LPDIRECTSOUNDBUFFER8 *buf,
   bool use_3d_hw){

                              //init format struct
   WAVEFORMATEX pcmwf;
   memset(&pcmwf, 0, sizeof(pcmwf));
   pcmwf.cbSize = sizeof(pcmwf);

   pcmwf.wFormatTag = WAVE_FORMAT_PCM;
   pcmwf.nChannels = word(wf.num_channels);
   pcmwf.nSamplesPerSec = wf.samples_per_sec;
   pcmwf.nBlockAlign = word(wf.bytes_per_sample);
   pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * wf.bytes_per_sample;
   pcmwf.wBitsPerSample = word(wf.bytes_per_sample * 8 / wf.num_channels);

                              //create sound buffer
   DSBUFFERDESC dsbd;
   memset(&dsbd, 0, sizeof(dsbd));
   dsbd.dwSize = sizeof(dsbd);

                              //must allow volume change (due to ours global volume)
   dsbd.dwFlags |= DSBCAPS_CTRLVOLUME;

   //dsbd.dwFlags |= DSBCAPS_STATIC;
   dsbd.dwFlags |= DSBCAPS_GETCURRENTPOSITION2;

   if((flags&ISND_3DBUFFER) && use_3d_hw)
      dsbd.dwFlags |= DSBCAPS_CTRL3D;
   if(flags&ISND_STREAMING) dsbd.dwFlags |= DSBCAPS_CTRLPOSITIONNOTIFY;
   if(!(flags&ISND_NOPANNING)) dsbd.dwFlags |= DSBCAPS_CTRLPAN;
   if(flags&ISND_FREQUENCY) dsbd.dwFlags |= DSBCAPS_CTRLFREQUENCY;
#ifdef DEBUG_FORCE_SYSMEM
   dsbd.dwFlags |= DSBCAPS_LOCSOFTWARE;
#else
   if(flags&ISND_CARDMEM) dsbd.dwFlags |= DSBCAPS_LOCHARDWARE;
   if(flags&ISND_SYSMEM) dsbd.dwFlags |= DSBCAPS_LOCSOFTWARE;
#endif
   //dsbd.dwFlags |= DSBCAPS_STICKYFOCUS;

                              //buffer length
   dsbd.dwBufferBytes = wf.size;
   dsbd.lpwfxFormat = &pcmwf;

   LPDIRECTSOUNDBUFFER lpDStmp;
   HRESULT hr = lpDS->CreateSoundBuffer(&dsbd, &lpDStmp, NULL);
   if(SUCCEEDED(hr)){
      hr = lpDStmp->QueryInterface(IID_IDirectSoundBuffer8, (void**)buf);
      lpDStmp->Release();
#if defined _DEBUG && 0
      {                       //debug! clear buffer
         void *bl1, *bl2;
         ulong bl1s, bl2s;
         (*buf)->Lock(0, 0, &bl1, &bl1s, &bl2, &bl2s, DSBLOCK_ENTIREBUFFER);
         memset(bl1, 0, bl1s);
         memset(bl2, 0, bl2s);
         (*buf)->Unlock(bl1, bl1s, bl2, bl2s);
      }
#endif
   }
   if(FAILED(hr))
      *buf = NULL;
   return hr;
}

//----------------------------
//----------------------------

ISND_source::ISND_source(PISND_driver d1):
   ref(1),
   drv(d1),
   src_flags(0),
   data(NULL), data_size(0),
   open_flags(0),
   lpDSB8(NULL)
{
   memset(&wf, 0, sizeof(wf));
}

//----------------------------

ISND_source::~ISND_source(){

   Close();
}

//----------------------------

ISND_RESULT ISND_source::OpenWave(const char *fname, LPS_wave_format wf, LPC_sound_loader &loader, dword &loader_handle){

   ISND_RESULT ret;
   EnterCriticalSection(&drv->lock);
                              //open file
   PC_dta_stream dta = DtaCreateStream(fname);
   if(!dta){
      ret = ISNDERR_NOFILE;
   }else{
                              //read header - first n bytes
      byte header[8];
      dta->Read(header, sizeof(header));
                              //scan through registered loaders to see which may load the file
      loader_handle = 0;
      for(int i=num_load_plugins; i--; ){
         loader_handle = load_plugins[i]->Open(dta, fname, header, sizeof(header), wf);
         if(loader_handle)
            break;
      }
      if(!loader_handle){        //loader not found, cannot read the file
         dta->Release();
         ret = ISNDERR_NOFILE;
      }else{
         loader = load_plugins[i];
         ret = ISND_OK;
      }
   }

   LeaveCriticalSection(&drv->lock);

   return ret;
}

//----------------------------

ISND_RESULT ISND_source::Open(const char *fname, dword flags){

   Close();
   if(!drv->IsInit())
      return ISNDERR_NOTINITIALIZED;
   if(flags&ISND_LOOP)
      return ISNDERR_INVALIDPARAM;

   EnterCriticalSection(&drv->lock);

   ISND_RESULT ir;

   LPC_sound_loader loader = NULL;
   dword loader_handle;
                              //find plugin capable to load sound
   ir = OpenWave(fname, &wf, loader, loader_handle);
   if(ISND_SUCCESS(ir)){

      if(flags&ISND_STREAMING){
         if(flags & (ISND_PRELOAD | ISND_CARDMEM | ISND_SYSMEM)){
            ir = ISNDERR_INVALIDPARAM;
            goto fail;
         }
         strm_loader = loader;   //save loader for later use
      }else
      if(flags&ISND_PRELOAD){
         if(flags&(ISND_NOVOLUME | ISND_NOPANNING | ISND_FREQUENCY | ISND_CARDMEM | ISND_SYSMEM | ISND_3DBUFFER)){
            ir = ISNDERR_INVALIDPARAM;
            goto fail;
         }
                              //load into sysmem buffer
         data = new byte[wf.size];
         if(!data){
            ir = ISNDERR_OUTOFMEM;
            goto fail;
         }
         data_size = wf.size;
         assert(data_size);
         loader->Read(loader_handle, data, data_size);
      }else{
                              //initialize DSound buffer
         HRESULT hr = CreateDSBuffer(drv->GetDSInterface(), wf, flags, &lpDSB8, (drv->GetSoundCaps()&SCAPS_3DINIT));
         if(FAILED(hr)){
            ir = ISNDERR_GENERIC;
            goto fail;
         }
                              //read data into buffer
         void *bl1, *bl2;
         ulong bl1s, bl2s;
                              //get access to buffer
         hr = lpDSB8->Lock(0, 0, &bl1, &bl1s, &bl2, &bl2s, DSBLOCK_ENTIREBUFFER);
         if(FAILED(hr)){
            lpDSB8->Release(); lpDSB8 = NULL;
            ir = ISNDERR_GENERIC;
            goto fail;
         }
                              //read sample data into buffer
         dword read_len = loader->Read(loader_handle, bl1, bl1s);
         if(bl2 && bl2s)
            read_len += loader->Read(loader_handle, bl2, bl2s);
         lpDSB8->Unlock(bl1, bl1s, bl2, bl2s);
         if(read_len != (bl1s + bl2s)){
                                 //error reading data
            lpDSB8->Release(); lpDSB8 = NULL;
            ir = ISNDERR_GENERIC;
            goto fail;
         }
      }
      if(loader)
         loader->Close(loader_handle);

      open_flags = flags;
      filename = fname;
      src_flags |= SRCF_OPEN;
   }

   LeaveCriticalSection(&drv->lock);

   return ir;
fail:
   if(loader)
      loader->Close(loader_handle);

   LeaveCriticalSection(&drv->lock);

   return ir;
}

//----------------------------

void ISND_source::Close(){

   EnterCriticalSection(&drv->lock);

   if(src_flags&SRCF_LOCKED)
      Unlock();

   delete[] data;
   data = NULL;
   if(lpDSB8){
      lpDSB8->Release();
      lpDSB8 = NULL;
   }
   filename = NULL;
   src_flags &= ~SRCF_OPEN;

   LeaveCriticalSection(&drv->lock);
}

//----------------------------

dword ISND_source::GetPlayTime() const{

   if(!IsOpen())
      return 0xffffffff;
   return wf.size * 1000 / (wf.samples_per_sec * wf.bytes_per_sample);
}

//----------------------------

ISND_RESULT ISND_source::Lock(void **bl1){

   if(!IsOpen())
      return ISNDERR_NOTINITIALIZED;
   if(src_flags&SRCF_LOCKED)
      return ISNDERR_ALREADYLOCKED;
   if(open_flags&ISND_PRELOAD){
      *bl1 = data;
   }else{
      assert(lpDSB8);
      HRESULT hr = lpDSB8->Lock(
         0,                      //write-cursor
         0,                      //size (ignored due to DSBLOCK_ENTIREBUFFER flag)
         &save_lock_ptr1,
         &save_lick_size1,
         &save_lock_ptr2,
         &save_lick_size2,
         DSBLOCK_ENTIREBUFFER);
      if(FAILED(hr))
         return ISNDERR_GENERIC;
      *bl1 = save_lock_ptr1;
   }
   src_flags |= SRCF_LOCKED;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_source::Unlock(){

   if(!IsOpen())
      return ISNDERR_NOTINITIALIZED;;

   if(!(src_flags&SRCF_LOCKED))
      return ISNDERR_NOTLOCKED;

   src_flags &= ~SRCF_LOCKED;

   if(!(open_flags&ISND_PRELOAD)){
      assert(lpDSB8);
      HRESULT hr = lpDSB8->Unlock(save_lock_ptr1, save_lick_size1, save_lock_ptr2, save_lick_size2);
      if(FAILED(hr))
         return ISNDERR_GENERIC;
   }
   return ISND_OK;
}

//----------------------------
//----------------------------

ISND_sound::ISND_sound(PISND_driver d):
   ref(1),
   drv(d),
   lpDSB8(NULL),
   volume(1.0f),
   freq_mult(1.0f),
   snd_flags(0),
   stream(NULL),
   play_time(0),
   panning(0.0f)
{
}

//----------------------------

ISND_sound::~ISND_sound(){

   Close();
   drv->RemoveSound(this);
}

//----------------------------
// Wrapped to multithread lock by calling functions.
ISND_RESULT ISND_sound::ReadStream(dword total_size){

   assert(stream);

   void *bl1, *bl2;
   ulong bl1s, bl2s;
   HRESULT hr;

   dword read_len = stream->buf_size / 2;

   ISND_RESULT res = ISND_OK;
                              //get access to buffer
   hr = lpDSB8->Lock(stream->buffer_half * read_len, read_len, &bl1, &bl1s, &bl2, &bl2s, 0);
   if(hr==DSERR_BUFFERLOST){
                              //restore and retry lock
      lpDSB8->Restore();
      hr = lpDSB8->Lock(stream->buffer_half*read_len, read_len, &bl1, &bl1s, &bl2, &bl2s, 0);
   }
   CHECK_DS_RESULT("Lock", hr);

   if(FAILED(hr)){
      res = ISNDERR_GENERIC;
   }else{
      assert(!bl2s);

      stream->buffer_half ^=1;
      int i = Min(total_size - stream->file_pos, read_len);

      //DEBUG_LOG(C_fstr("ReadStream: reading from pos: 0x%x, size: %i", stream->loader->Seek(stream->load_handle, 0), i));
                              //read sample data into buffer
      int j = i - stream->loader->Read(stream->load_handle, bl1, Min((int)bl1s, i));
      if(j)
         stream->loader->Read(stream->load_handle, bl2, j);
      stream->file_pos += i;

      dword rest = read_len - i;
      if(rest){
         if(snd_flags&SNDF_STREAM_LOOP){
            do{
                              //rewind file
               stream->loader->Seek(stream->load_handle, 0);
               int len = Min(rest, total_size);
               i = stream->loader->Read(stream->load_handle, (byte*)bl1+read_len-rest, len);
               stream->file_pos = i;
            }while(rest -= i);
         }else{
            memset((byte*)bl1+i, 0, rest);
            res = ISND_DONE;
         }
      }
      hr = lpDSB8->Unlock(bl1, bl1s, bl2, bl2s);
      CHECK_DS_RESULT("Unlock", hr);
   }
   return res;
}

//----------------------------
                              //thread for streaming sample loading
void ISND_sound::NotifyThreadStream(){

   DEBUG_LOG(C_fstr("Stream thread 0x%x: entered function.", stream->id_thread));
   for(;;){
      dword dw_wait;
      dw_wait = WaitForSingleObject(stream->h_notify, INFINITE);
      assert(dw_wait==WAIT_OBJECT_0);
#if defined _DEBUG && 0
      {                       //ignore non-playing buffers - override DS bug
         dword status;
         lpDSB8->GetStatus(&status);
         if(!(status&(DSBSTATUS_PLAYING|DSBSTATUS_LOOPING))){
            DEBUG_LOG(C_fstr("Stream thread 0x%x: signalled although not playing..?", stream->id_thread));
            //assert(0);
            continue;
         }
      }
#endif
      DEBUG_LOG(C_fstr("Stream thread 0x%x: reading data to buffer half %i.", stream->id_thread, stream->buffer_half));

      EnterCriticalSection(&drv->lock);
      ISND_RESULT ir = ReadStream(wf.size);
      LeaveCriticalSection(&drv->lock);

      if(ISND_FAIL(ir)){
         DEBUG_LOG("Stream thread: failed to read from buffer.");
         break;
      }
      if(ir==ISND_DONE){
         DEBUG_LOG(C_fstr("Stream thread 0x%x: done reading data, waiting for buffer to finish.", stream->id_thread));
         WaitForSingleObject(stream->h_notify, INFINITE);

                              //fill by zero
         DEBUG_LOG(C_fstr("Stream thread 0x%x: filling buffer half %i to zero.", stream->id_thread, stream->buffer_half));

         EnterCriticalSection(&drv->lock);
         ReadStream(wf.size);
         LeaveCriticalSection(&drv->lock);

         WaitForSingleObject(stream->h_notify, INFINITE);
                              //stop buffer
         DEBUG_LOG(C_fstr("Stream thread 0x%x: stopping the buffer.", stream->id_thread));
         lpDSB8->Stop();
         break;
      }
   }
   stream->thread_done = true;
   ExitThread(0);
}

//----------------------------

ISND_RESULT ISND_sound::Open(PISND_source src, dword flags){

   if(!src)
      return ISNDERR_INVALIDPARAM;

   Close();
   if(!src->IsOpen())
      return ISNDERR_NOTINITIALIZED;

   EnterCriticalSection(&drv->lock);

   ISND_RESULT ir = ISNDERR_GENERIC;
   HRESULT hr;
   bool ok = false;
   dword play_flags = 0;

   if(src->open_flags&ISND_STREAMING){
      PC_dta_stream dta = DtaCreateStream(src->filename);
      if(!dta){
         LeaveCriticalSection(&drv->lock);
         return ISNDERR_NOFILE;
      }
                              //read header - first n bytes
      S_wave_format wf1;
      byte header[8];
      dta->Read(header, sizeof(header));
      int load_handle = src->strm_loader->Open(dta, src->filename, header, sizeof(header), &wf1);
      if(load_handle){
                              //create playback buffer
         dword strm_len = STREAM_BUFFER_SIZE(wf1.samples_per_sec, wf1.bytes_per_sample);
         wf1.size = strm_len;
         hr = CreateDSBuffer(drv->GetDSInterface(), wf1, flags | ISND_STREAMING, &lpDSB8, (drv->GetSoundCaps()&SCAPS_3DINIT));
         if(SUCCEEDED(hr)){
                              //create notification
            LPDIRECTSOUNDNOTIFY8 lpDsNotify8;
            hr = lpDSB8->QueryInterface(IID_IDirectSoundNotify8, (void**)&lpDsNotify8);
            if(SUCCEEDED(hr)){
               DSBPOSITIONNOTIFY pn[2];
                              //win32 CreateEvent
               HANDLE ds_event = CreateEvent(NULL, false, false, NULL);
               assert(ds_event);
               DEBUG_LOG(C_fstr("Creating event 0x%x for sound '%s'.", ds_event, src->GetFileName()));
               pn[0].hEventNotify = ds_event;
               pn[1].hEventNotify = ds_event;
               pn[0].dwOffset = strm_len / 2 - 16;
               pn[1].dwOffset = strm_len - 16;
               hr = lpDsNotify8->SetNotificationPositions(2, pn);
               if(SUCCEEDED(hr)){
                              //init stream data
                  stream = new S_stream;
                  stream->buf_size = strm_len;
                  stream->loader = src->strm_loader;
                  stream->load_handle = load_handle;
                  stream->DSnotify8 = lpDsNotify8;
                  stream->h_notify = ds_event;
                  stream->h_thread = CreateThread(NULL, NULL, &NotifyThreadStreamThunk,
                     this, CREATE_SUSPENDED, &stream->id_thread);
                  DEBUG_LOG(C_fstr("Creating stream thread 0x%x (tid=0x%x) for sound '%s'.",
                     stream->h_thread, stream->id_thread, src->GetFileName()));
                  //SetThreadPriority(stream->h_thread, THREAD_PRIORITY_ABOVE_NORMAL);
                              //rewind file
                  stream->loader->Seek(stream->load_handle, 0);

                  snd_flags &= ~SNDF_STREAM_LOOP;
                  if(flags&ISND_LOOP) snd_flags |= SNDF_STREAM_LOOP;
                              //read 2 halfs of buffer
                  ReadStream(src->wf.size);
                  ReadStream(src->wf.size);
                  ir = ISND_OK;
                              //play buffer looped
                  play_flags |= DSBPLAY_LOOPING;

                  ok = true;
               }
               lpDsNotify8->Release();
            }
            if(FAILED(hr)){
               lpDSB8->Release(); lpDSB8 = NULL;
            }
         }
      }else{
         ir = ISNDERR_NOFILE;
         dta->Release();
      }
   }else
   if(src->open_flags&ISND_PRELOAD){
      hr = CreateDSBuffer(drv->GetDSInterface(), src->wf, flags, &lpDSB8, (drv->GetSoundCaps()&SCAPS_3DINIT));
      if(SUCCEEDED(hr)){
                              //upload data into buffer
         void *bl1, *bl2;
         ulong bl1s, bl2s;
                              //get access to buffer
         hr = lpDSB8->Lock(0, 0, &bl1, &bl1s, &bl2, &bl2s, DSBLOCK_ENTIREBUFFER);
         if(SUCCEEDED(hr)){
                              //read sample data into buffer
            if(bl1) memcpy(bl1, src->data, bl1s);
            if(bl2) memcpy(bl2, src->data + bl1s, bl2s);
            lpDSB8->Unlock(bl1, bl1s, bl2, bl2s);
         }
         if(FAILED(hr)){
            lpDSB8->Release();
            lpDSB8 = NULL;
            ir = ISNDERR_GENERIC;
         }
      }
      ok = SUCCEEDED(hr);
   }else{
      HRESULT hr = DS_OK;
                              //use or duplicate buffer
      dword status;
      hr = src->lpDSB8->GetStatus(&status);
      CHECK_DS_RESULT("GetStatus", hr);
      if(status&DSBSTATUS_BUFFERLOST){
         DS_Fatal("GetStatus = Buffer lost!", 0ul);
         LeaveCriticalSection(&drv->lock);
         return ISNDERR_BUFFERLOST;
      }
      if(status&(DSBSTATUS_PLAYING|DSBSTATUS_LOOPING)){
                              //duplicate sound buffer if possible
         LPDIRECTSOUNDBUFFER lpDStmp;
         hr = drv->GetDSInterface()->DuplicateSoundBuffer(src->lpDSB8, &lpDStmp);
         if(SUCCEEDED(hr)){
            hr = lpDStmp->QueryInterface(IID_IDirectSound3DBuffer8, (void**)&lpDSB8);
            lpDStmp->Release();
         }
         if(FAILED(hr)){
            lpDSB8 = NULL;
                              //create temporary source, play through this
            PISND_source tmp_src;
            ir = drv->CreateSource(&tmp_src);
            if(ISND_SUCCESS(ir)){
               ir = tmp_src->Open(src->filename, src->open_flags);
               if(ISND_SUCCESS(ir)){
                              //open from this source
                  ir = PlaySource(tmp_src, flags);
               }
               tmp_src->Release();
            }
         }else{
            ok = true;
         }
      }else{
         lpDSB8 = src->lpDSB8;
         lpDSB8->AddRef();
         ok = true;
      }
   }
   if(ok){
      play_time = 0;

      wf = src->wf;
      ir = ISND_OK;

      snd_flags &= ~SNDF_ADJ_FREQUENCY;
      if(flags&ISND_FREQUENCY)
         snd_flags |= SNDF_ADJ_FREQUENCY;

      SetVolume(volume);
   }

   LeaveCriticalSection(&drv->lock);

   return ir;
}

//----------------------------

ISND_RESULT ISND_sound::Play(dword flags){

   if(!lpDSB8)
      return ISNDERR_NOTINITIALIZED;

   if(flags&(~ISND_LOOP))
      return ISNDERR_INVALIDPARAM;

   dword play_flags = 0;
   HRESULT hr;
   if(stream){
                              //streaming sounds play always the buffer looped
      play_flags |= DSBPLAY_LOOPING;
      if(flags&ISND_LOOP)
         snd_flags |= SNDF_STREAM_LOOP;
      dword dw_res;
      dw_res = ResumeThread(stream->h_thread);
      assert(dw_res!=-1);
   }else{
      if(flags&ISND_LOOP)
         play_flags |= DSBPLAY_LOOPING;

      dword ppos = play_time * wf.samples_per_sec * wf.bytes_per_sample / 1000;
      if(ppos >= wf.size){
         assert(0);
         return ISNDERR_INVALIDPARAM;
      }
                              //align to sample boundary
      ppos &= -int(wf.bytes_per_sample);

      hr = lpDSB8->SetCurrentPosition(ppos);
      CHECK_DS_RESULT("SetCurrentPosition", hr);
   }
   hr = lpDSB8->Play(0, 0, play_flags);
   CHECK_DS_RESULT("Play", hr);
   if(FAILED(hr))
      return ISNDERR_GENERIC;
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_sound::PlaySource(PISND_source src, dword flags){

   ISND_RESULT ir = Open(src, flags&(~ISND_LOOP));
   if(ISND_SUCCESS(ir)){
      ir = Play(flags&ISND_LOOP);
      if(ISND_FAIL(ir))
         Close();
   }
   return ir;
}

//----------------------------

ISND_RESULT ISND_sound::Close(){

   if(!lpDSB8)
      return ISNDERR_NOTINITIALIZED;

   Stop();
   if(stream){
      delete stream;
      stream = NULL;
   }
                              //release buffer
   lpDSB8->Release(); lpDSB8 = NULL;

   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_sound::Stop(){

   if(!lpDSB8)
      return ISNDERR_NOTINITIALIZED;
   if(stream){
      SuspendThread(stream->h_thread);
   }
   HRESULT hr;
   hr = lpDSB8->Stop();
   CHECK_DS_RESULT("Stop", hr);
   return ISND_OK;
}

//----------------------------

dword ISND_sound::GetCurrTime() const{

   if(!lpDSB8)
      return 0xffffffff;

   dword ppos, wpos;

   HRESULT hr;
   hr = lpDSB8->GetCurrentPosition(&ppos, &wpos);
   CHECK_DS_RESULT("GetCurrentPosition", hr);
   if(stream){
                              //wrong computation
      ppos += stream->file_pos - stream->buf_size;
      if(stream->buffer_half)
         ppos -= stream->buf_size/2;
   }
   return ppos * 1000 / (wf.samples_per_sec * wf.bytes_per_sample);
}

//----------------------------

ISND_RESULT ISND_sound::SetCurrTime(dword t){

   play_time = t;
   if(lpDSB8){
      dword ppos = dword((__int64)play_time * (__int64)wf.samples_per_sec * (__int64)wf.bytes_per_sample / (__int64)1000);
      if(ppos >= wf.size){
         assert(0);
         return ISNDERR_INVALIDPARAM;
      }
                              //align to sample boundary
      ppos &= -int(wf.bytes_per_sample);
      if(stream){
                              //streaming mode - different behaviour
         if(IsPlaying())
            SuspendThread(stream->h_thread);

         stream->buffer_half ^=1;

         DEBUG_LOG(C_fstr("Stream 0x%x position: %i of %i, buffer half: %i",
            stream->id_thread, ppos, wf.size, stream->buffer_half));

                              //seek within stream
         stream->loader->Seek(stream->load_handle, ppos);
         stream->file_pos = ppos;
                              //read 1st half of the buffer
         ReadStream(wf.size);
                              //start playing from this position
         ppos = 0;
         if(!stream->buffer_half){
            ppos = stream->buf_size / 2;
         }
         HRESULT hr;
         hr = lpDSB8->SetCurrentPosition(ppos);
         CHECK_DS_RESULT("SetCurrentPosition", hr);

                              //reset notify for cases it would reach end of buffer in meantime
         ResetEvent(stream->h_notify);

                              //now since we're playing the half of buffer,
                              // read the other half
         ReadStream(wf.size);

         if(IsPlaying())
            ResumeThread(stream->h_thread);
      }else{
         HRESULT hr;
         hr = lpDSB8->SetCurrentPosition(ppos);
         CHECK_DS_RESULT("SetCurrentPosition", hr);
      }
   }
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_sound::SetVolume(float f){

   volume = f;
   if(lpDSB8){
      float v = Max(0.0f, Min(1.0f, drv->GetVolume() * volume));
      int i1 = Max(0, Min(64, (int)(v*64.0f)));
      HRESULT hr;
      hr = lpDSB8->SetVolume(drv->vol_tab[i1]);
      CHECK_DS_RESULT("SetVolume", hr);
   }
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_sound::SetFrequency(float f){

   if(!(snd_flags&SNDF_ADJ_FREQUENCY))
      return ISNDERR_UNSUPPORTED;

   freq_mult = f;
   if(lpDSB8){
      int new_freq = int(freq_mult * wf.samples_per_sec);
      new_freq = Max(DSBFREQUENCY_MIN, Min(DSBFREQUENCY_MAX, new_freq));
      HRESULT hr;
      hr = lpDSB8->SetFrequency(new_freq);
      CHECK_DS_RESULT("SetFrequency", hr);
   }
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_sound::SetPanning(float f){

   panning = Max(-1.0f, Min(1.0f, f));
   if(lpDSB8){
      HRESULT hr;
      hr = lpDSB8->SetPan(long(10000 - 10000.0f * (1.0f-f)));
      CHECK_DS_RESULT("SetPan", hr);
   }
   return ISND_OK;
}

//----------------------------

bool ISND_sound::IsPlaying() const{

   if(!lpDSB8)
      return false;
   dword status;
   HRESULT hr;
   hr = lpDSB8->GetStatus(&status);
   CHECK_DS_RESULT("GetStatus", hr);
   return (status&(DSBSTATUS_PLAYING|DSBSTATUS_LOOPING));
}

//----------------------------
//----------------------------

ISND_3Dsound::ISND_3Dsound(PISND_driver d):
   ISND_sound(d),
   lpDSB3D8(NULL)
{
   memset(&ds_data, 0, sizeof(ds_data));
   ds_data.dwSize = sizeof(ds_data);
   ds_data.dwInsideConeAngle = 360;
   ds_data.dwOutsideConeAngle = 360;
   ds_data.vConeOrientation.z = 1.0f;
   ds_data.flMinDistance = 1.0f;
   ds_data.flMaxDistance = 10.0f;
   ds_data.dwMode = DS3DMODE_NORMAL;
}

//----------------------------

ISND_3Dsound::~ISND_3Dsound(){

   Close();
}

//----------------------------

ISND_RESULT ISND_3Dsound::Close(){

   if(lpDSB3D8){
      lpDSB3D8->Release();
      lpDSB3D8 = NULL;
   }
   return ISND_sound::Close();
}

//----------------------------

ISND_RESULT ISND_3Dsound::Open(PISND_source src, dword flags){

   ISND_RESULT ir = ISND_sound::Open(src, flags);
   if(ISND_SUCCESS(ir)){
                              //obtain 3D interface & setup
      HRESULT hr = lpDSB8->QueryInterface(IID_IDirectSound3DBuffer, (void**)&lpDSB3D8);
      if(SUCCEEDED(hr)){
         HRESULT hr;
         hr = lpDSB3D8->SetAllParameters(&ds_data, DS3D_IMMEDIATE);
         CHECK_DS_RESULT("SetAllParameters", hr);
      }else{
         lpDSB3D8 = NULL;
      }
   }
   return ir;
}

//----------------------------

ISND_RESULT ISND_3Dsound::SetPos(const S_vector &v){

   *(S_vector*)&ds_data.vPosition = v;
   if(lpDSB3D8){
      HRESULT hr;
      hr = lpDSB3D8->SetPosition(v.x, v.y, v.z, DS_APPLY_FLAGS);
      CHECK_DS_RESULT("SetPosition", hr);
   }
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dsound::SetDir(const S_vector &v){

   *(S_vector*)&ds_data.vConeOrientation = v;
   if(lpDSB3D8){
      HRESULT hr;
      hr = lpDSB3D8->SetConeOrientation(v.x, v.y, v.z, DS_APPLY_FLAGS);
      CHECK_DS_RESULT("SetConeOrientation", hr);
   }
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dsound::SetVelocity(const S_vector &v){

   *(S_vector*)&ds_data.vVelocity = v;
   if(lpDSB3D8){
      HRESULT hr;
      hr = lpDSB3D8->SetVelocity(v.x, v.y, v.z, DS_APPLY_FLAGS);
      CHECK_DS_RESULT("SetVelocity", hr);
   }
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dsound::SetDistances(float min, float max){

   ds_data.flMinDistance = min;
   ds_data.flMaxDistance = max;
   if(lpDSB3D8){
      HRESULT hr = lpDSB3D8->SetMinDistance(min, DS_APPLY_FLAGS);
      CHECK_DS_RESULT("SetMinDistance", hr);
      hr = lpDSB3D8->SetMaxDistance(max, DS_APPLY_FLAGS);
      CHECK_DS_RESULT("SetMaxDistance", hr);
   }
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dsound::SetCone(float in_angle, float out_angle){

   ds_data.dwInsideConeAngle = dword(in_angle * 180.0f / PI);
   ds_data.dwOutsideConeAngle = dword(out_angle * 180.0f / PI);
   if(lpDSB3D8){
      HRESULT hr;
      hr = lpDSB3D8->SetConeAngles(ds_data.dwInsideConeAngle, ds_data.dwOutsideConeAngle, DS_APPLY_FLAGS);
      CHECK_DS_RESULT("SetConeAngles", hr);
   }

   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dsound::SetVolume(float f){

   return ISND_sound::SetVolume(f);
   if(!lpDSB3D8)
      return ISND_sound::SetVolume(f);
   volume = f;
                              //setup linear volume
   float v = Max(0.0f, Min(1.0f, drv->GetVolume() * volume));
   HRESULT hr;
   hr = lpDSB8->SetVolume(long(v * 10000.0f - 10000.0f));
   CHECK_DS_RESULT("SetVolume", hr);
   return ISND_OK;
}

//----------------------------

ISND_RESULT ISND_3Dsound::SetOutVolume(float out_vol){

   ds_data.lConeOutsideVolume = long(DSBVOLUME_MIN + (DSBVOLUME_MAX-DSBVOLUME_MIN) * out_vol);
   if(lpDSB3D8){
      HRESULT hr;
      hr = lpDSB3D8->SetConeOutsideVolume(ds_data.lConeOutsideVolume, DS_APPLY_FLAGS);
      CHECK_DS_RESULT("SetConeOutsideVolume", hr);
   }
   return ISND_OK;
}

//----------------------------
//----------------------------
