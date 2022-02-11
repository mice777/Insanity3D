#include "all.h"
#include "c_music.h"

#define LOOPSAMPLES (8*1024)

/***************************************************************************
>>>>>>>>>>>>>>>>>>>>>> "lolevel" DIRECTSOUND stuff <<<<<<<<<<<<<<<<<<<<<<<<<
***************************************************************************/

//LPDIRECTSOUND lpDS;
//LPDIRECTSOUNDBUFFER lpDSSBPrimary;

#if defined NDEBUG && 0

#include <iostream>
#define LOG(n) cout<<(const char*)n

#else

#define LOG(n)

#endif

//-------------------------------------

static bool WriteDataToBuffer(LPDIRECTSOUNDBUFFER lpDsb, schar *buf, DWORD len){

   LPVOID lpvPtr1, lpvPtr2;
   DWORD dwBytes1, dwBytes2;
                                       //obtain write pointer.
   HRESULT hr = lpDsb->Lock(0, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
                        //if we got DSERR_BUFFERLOST, restore and retry lock.
   if(hr==DSERR_BUFFERLOST){
      lpDsb->Restore();
      hr = lpDsb->Lock(0, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
   }
   if(SUCCEEDED(hr)){
                                       //write to pointers.
      memcpy(lpvPtr1, buf, dwBytes1);
      if(lpvPtr2)
         memcpy(lpvPtr2, buf+dwBytes1, dwBytes2);
                                       //release the data back to DirectSound.
      hr = lpDsb->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);
      if(SUCCEEDED(hr))
         return true;
   }
   return false;
}

//-------------------------------------

bool ISND_music::LoadSamp(LPDIRECTSOUNDBUFFER *lplpDsb, schar *samp, UINT length){

                              //set up wave format structure.
   WAVEFORMATEX pcmwf;
   memset(&pcmwf, 0, sizeof(pcmwf));
   pcmwf.cbSize = sizeof(pcmwf);

   pcmwf.wFormatTag = WAVE_FORMAT_PCM;
   pcmwf.nChannels = 1;
   //pcmwf.nSamplesPerSec = md_mixfreq;
   pcmwf.nSamplesPerSec = 22050;
   pcmwf.wBitsPerSample = 8<<(int)b_16bit;
   pcmwf.nBlockAlign = pcmwf.wBitsPerSample / 8 * pcmwf.nChannels;
   pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;

   DSBUFFERDESC dsbd;
   memset(&dsbd, 0, sizeof(dsbd));
   dsbd.dwSize = sizeof(dsbd);
   dsbd.dwFlags = DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY |
      DSBCAPS_STATIC | DSBCAPS_GETCURRENTPOSITION2;
   //dsbd.dwFlags |= DSBCAPS_LOCSOFTWARE;

   //dsbd.dwFlags |= DSBCAPS_GLOBALFOCUS;
   //dsbd.dwFlags |= DSBCAPS_STICKYFOCUS;

   dsbd.dwBufferBytes = length;
   dsbd.lpwfxFormat = &pcmwf;

   HRESULT hr = drv->GetDS()->CreateSoundBuffer(&dsbd, lplpDsb, NULL);
   if(SUCCEEDED(hr)){
      return WriteDataToBuffer(*lplpDsb, samp, length);
   }
   *lplpDsb = NULL;
   return false;
}

//-------------------------------------

/***************************************************************************
>>>>>>>>>>>>>>>>>>>>>> The actual DIRECTSOUND driver <<<<<<<<<<<<<<<<<<<<<<<
***************************************************************************/

//-------------------------------------

void ISND_music::DSSetBPM(word bpm){

   DS_BPM = (byte)bpm;
   float rate = 125.0f * 1000.0f / (bpm*50);
   myTimerSet(rate);
}

//-------------------------------------

void ISND_music::DS_Update(){

   EnterCriticalSection(&timer_lock);

   Tick();

   if(DS_BPM != beats_per_minute)
      DSSetBPM(beats_per_minute);
   ulong status;
   ulong pos, posp;
   HRESULT hr;
                                       //loop switching temps
   LPDIRECTSOUNDBUFFER killed_base = NULL;
   byte killed_base_dupl = 0;
   byte killed_base_handle = 0;
                                       //process all channels
   dword t = unimod.num_chnannels;
   if(t) do{
      GHOLD *aud = &ghld[--t];
      if(!aud->kick){
                                       //care of non-kicked non-looped voices
         if(aud->samp && aud->flags&SF_LOOP && !aud->real_looping){
                                       //do looping
            hr = aud->samp->GetStatus(&status);
            if(SUCCEEDED(hr))
            if(!(status&DSBSTATUS_PLAYING)){
                                       //base stopped (shouldn't!), play looped
               LOG("base stopped?, real_loop switch\n");
                                       //release base
               if(aud->is_dupl)
                  aud->samp->Release();
               else
                  is_sam_playing[aud->playing_handle][0] = 0;
                                       //if primary stopped, free it
               if(is_sam_playing[aud->handle][1]){
                  hr = lpSamp[aud->handle][1]->GetStatus(&status);
                  if(!(status&DSBSTATUS_PLAYING) && SUCCEEDED(hr)){
                     LOG("stopped primary found\n");
                     is_sam_playing[aud->handle][1] = 0;
                  }
               }
               if(is_sam_playing[aud->handle][1]){
                  LOG("play duplicate looped\n");
                                       //primary playing, duplicate
                  drv->GetDS()->DuplicateSoundBuffer(lpSamp[aud->handle][1], &aud->samp);
                  aud->is_dupl=1;
               }else{
                  LOG("play primary looped\n");
                                    //play primary
                  aud->samp=lpSamp[aud->handle][1];
                  aud->is_dupl=0;
                  ++is_sam_playing[aud->playing_handle=aud->handle][1];
               }
               aud->samp->SetCurrentPosition(0);
                                       //setup later
               aud->frq_trig=aud->vol_trig=aud->pan_trig=aud->kick=1;
               aud->real_looping=1;
            }else{                     //playing
                                       //check if behind loop_start
               hr = aud->samp->GetCurrentPosition(&pos, &posp);
               pos >>= (int)b_16bit;
               if(pos>=aud->loop_start && SUCCEEDED(hr)){
                                       //switch to loop mode
                  LOG(C_fstring("loop switch:  pos: %i   loop_start: %i  loop_end: %i  new: %i\n", pos, aud->loop_start, aud->loop_end, (pos-aud->loop_start)%(aud->loop_end-aud->loop_start)));
                  pos -= aud->loop_start;
                  pos %= (aud->loop_end-aud->loop_start);
                                       //save base for later processing
                  killed_base = aud->samp;
                  killed_base_dupl = aud->is_dupl;
                  killed_base_handle = (byte)aud->playing_handle;
                                       //if primary stopped, free it
                  if(is_sam_playing[aud->handle][1]){
                     hr = lpSamp[aud->handle][1]->GetStatus(&status);
                     if(!(status&DSBSTATUS_PLAYING) && SUCCEEDED(hr)){
                        LOG("stopped primary found\n");
                        is_sam_playing[aud->handle][1] = 0;
                     }
                  }
                                       //start looped
                  if(is_sam_playing[aud->handle][1]){
                     LOG("play duplicate looped\n");
                                       //primary playing, duplicate
                     drv->GetDS()->DuplicateSoundBuffer(lpSamp[aud->handle][1], &aud->samp);
                     aud->is_dupl = 1;
                  }else{
                     LOG("play primary looped\n");
                                       //play primary
                     aud->samp=lpSamp[aud->handle][1];
                     aud->is_dupl=0;
                     ++is_sam_playing[aud->playing_handle=aud->handle][1];
                  }
                  aud->samp->SetCurrentPosition(pos);
                                       //setup later
                  aud->frq_trig = aud->vol_trig = aud->pan_trig = aud->kick = 1;
                  aud->real_looping = 1;
               }
            }
         }
      }else{
                                       //kicked - play voice
         if(aud->samp){
            LOG("kill\n");
                                       //kill old voice
            hr = aud->samp->Stop();
            if(SUCCEEDED(hr)){
               if(aud->is_dupl)
                  aud->samp->Release();
               else
                  is_sam_playing[aud->playing_handle][aud->real_looping]=0;
            }
            aud->samp = NULL;
         }
                                       //determine if withing loop range
         aud->real_looping = (aud->flags&SF_LOOP && aud->start>=aud->loop_start);
                                       //does the sample exist?
         if(lpSamp[aud->handle][aud->real_looping]){
                                       //if primary stopped, free it
            if(is_sam_playing[aud->handle][aud->real_looping]){
               hr = lpSamp[aud->handle][aud->real_looping]->GetStatus(&status);
               if(!(status&DSBSTATUS_PLAYING) && SUCCEEDED(hr)){
                  LOG("stopped primary found\n");
                  is_sam_playing[aud->handle][aud->real_looping]=0;
               }
            }
                                       //is primary currently playing?
            if(is_sam_playing[aud->handle][aud->real_looping]){
               LOG(C_fstring("play duplicate%s\n", aud->real_looping ? "looped!":""));
                                    //primary playing, duplicate
               drv->GetDS()->DuplicateSoundBuffer(lpSamp[aud->handle][aud->real_looping], &aud->samp);
               aud->is_dupl=1;
            }else{
               LOG(C_fstring("play primary%s\n", aud->real_looping ? "looped!":""));
                                    //play primary
               aud->samp=lpSamp[aud->handle][aud->real_looping];
               aud->is_dupl = 0;
               ++is_sam_playing[aud->playing_handle=aud->handle][aud->real_looping];
            }
         }
         if(aud->samp){
                                       //set startup position
            LOG(C_fstring("setpos: %i\n", aud->start));
            aud->samp->SetCurrentPosition(
               (aud->start- aud->real_looping ? aud->loop_start : 0)<<(int)b_16bit);
                                       //trigger all
            aud->frq_trig = aud->vol_trig = aud->pan_trig = 1;
         }
      }
      if(aud->samp){
                                       //update vol, pan, freq & kick
         if(aud->frq_trig){
            LOG(C_fstring("set freq: %i\n", aud->frq));
            aud->samp->SetFrequency(aud->frq);
            aud->frq_trig = 0;
         }
         if(aud->pan_trig){
            LOG(C_fstring("set pan: %i\n", aud->pan));
            aud->samp->SetPan(pantab[aud->pan]);
            aud->pan_trig = 0;
         }
         if(aud->vol_trig){
//printf("set vol: %i\n", aud->vol);
            if(!aud->vol){
                                       //zero volume, release
               hr = aud->samp->Stop();
               if(SUCCEEDED(hr)){
                  if(aud->is_dupl)
                     aud->samp->Release();
                  else
                     is_sam_playing[aud->playing_handle][aud->real_looping] = 0;
               }
               aud->samp = NULL;
               aud->kick = 0;
               if(killed_base){
                                          //stop & release base
                  killed_base->Stop();
                  if(killed_base_dupl) killed_base->Release();
                  else is_sam_playing[killed_base_handle][0]=0;
                  killed_base=NULL;
               }
            }else
               aud->samp->SetVolume(voltab[aud->vol]);
            aud->vol_trig = 0;
         }
         if(aud->kick){
            LOG("play\n");
            aud->samp->Play(0, 0, aud->real_looping ? DSBPLAY_LOOPING : 0);
            if(killed_base){
                                       //stop & release base
               killed_base->Stop();
               if(killed_base_dupl)
                  killed_base->Release();
               else
                  is_sam_playing[killed_base_handle][0] = 0;
               killed_base = NULL;
            }
            aud->kick = 0;
         }
      }
   }while(t);
   LeaveCriticalSection(&timer_lock);
}

//----------------------------

static void SL_Load(C_cache *cc, void *buffer, dword length, short &sl_old,
   word sl_infmt, word sl_outfmt){

	int t;
                                       //compute number of samples to load
	if(sl_outfmt&SF_16BITS)
      length >>= 1;

   if(sl_infmt&SF_16BITS){
      cc->read((char*)buffer, length*sizeof(word));
      /*
#ifdef MM_BIG_ENDIAN
      if(!(sl_infmt&SF_BIG_ENDIAN)) swab((char *)buffer,(char *)buffer, length<<1);
#else
                              //assume machine is little endian by default
      if(sl_infmt&SF_BIG_ENDIAN) swab((char *)buffer,(char *)buffer, length<<1);
#endif
      */
   }
   else{
      cc->read((char*)buffer, length);
                                    //convert to 16-bits
      if(sl_outfmt&SF_16BITS){
         schar *s = ((schar*)buffer) + length;
         short *d = ((short*)buffer) + length;
         for(int i=length; i--; ){
            --d;
            --s;
            *d = (*s) << 8;
         }
      }
   }
   if(sl_infmt&SF_DELTA){
      if(sl_outfmt&SF_16BITS){
         short *d = (short*)buffer;
         for(t=0; t<(int)length; t++){
            d[t] += sl_old;
            sl_old = d[t];
         }
      }else{
         short sl_old1 = sl_old;
         schar *d = (schar*)buffer;
         for(t=0; t<(int)length; t++){
            d[t] += sl_old1;
            sl_old = d[t];
         }
         sl_old = sl_old1;
      }
   }
   if((sl_infmt^sl_outfmt) & SF_SIGNED){
      if(sl_outfmt & SF_16BITS){
         short *d = (short*)buffer;
         for(t=0; t<(int)length; t++) d[t]^=0x8000;
      }else{
         schar *s = (schar*)buffer;
         t = length;
         do{
            (*s++)^=0x80;
         }while(--t);
      }
   }
}

//----------------------------

short ISND_music::SampleLoad(C_cache *cc, ULONG length, ULONG loopstart, ULONG loopend, word flags){

   int t;
   int i = 0;
   union{
      short *lpSampMem;
      schar *lpSampMem_b;
   };
   ULONG pos;

   short sl_old = 0;
   word outfmt = flags | SF_SIGNED | (b_16bit ? SF_16BITS : 0);
                                    //find empty slot to put sample address in
   int handle=0;
   while(lpSamp[handle][0] || lpSamp[handle][1]){
      if(++handle==MAXSAMPLEHANDLES){
         return -1;
      }
   }
                                       //get lenght to alloc
   if(!(flags&SF_LOOP)) loopstart=0;
   dword lengthAlloc=length+2;
   if(flags&SF_LOOP) lengthAlloc=max(lengthAlloc, loopstart+LOOPSAMPLES);
   lengthAlloc <<= (int)b_16bit;

   lpSampMem_b=new schar[lengthAlloc];
   if(!lpSampMem_b) return -1;
                                       //load the sample
   SL_Load(cc, lpSampMem, length<<(int)b_16bit, sl_old, flags, outfmt);

   if(!(flags&SF_LOOP)){
                                       //finish sample by zero
      if(b_16bit){
         lpSampMem[length]=0;
         lpSampMem[length+1]=0;
      }else{
         lpSampMem_b[length]=0;
         lpSampMem_b[length+1]=0;
      }
   }else{
                                       //looped sample, wrap around
      pos=loopstart;
      if(b_16bit){
         for(t=loopend; t<(int)lengthAlloc/2; t++){
            lpSampMem[t]=lpSampMem[pos];
            if(++pos==loopend) pos=loopstart;
         }
      }else{
         for(t=loopend; t<(int)lengthAlloc; t++){
            lpSampMem_b[t]=lpSampMem_b[pos];
            pos++;
            if(pos==loopend) pos=loopstart;
         }
      }
                                       //create looped part
      i = LoadSamp(&lpSamp[handle][1],
         lpSampMem_b + (loopstart<<(int)b_16bit), (loopend-loopstart) << (int)b_16bit);
      if(!i){
         delete[] lpSampMem;
         return -1;
      }
   }
   if(!(flags&SF_LOOP) || loopstart){
      i = LoadSamp(&lpSamp[handle][0], lpSampMem_b,
         (flags&SF_LOOP) ? ((loopstart+LOOPSAMPLES)<<(int)b_16bit) : lengthAlloc);
   }
   delete[] lpSampMem;
   if(!i){
      lpSamp[handle][1]->Release();
      lpSamp[handle][1]=NULL;
      return -1;
   }
   is_sam_playing[handle][0]=0;
   is_sam_playing[handle][1]=0;
   return handle;
}

//-------------------------------------

void ISND_music::SampleUnload(short handle){

   for(int i=0; i<2; i++)
   if(lpSamp[handle][i]){
      lpSamp[handle][i]->Stop();
      lpSamp[handle][i]->Release();
      lpSamp[handle][i]=NULL;
   }
}

//-------------------------------------

dword WINAPI ISND_music::TimerThreatProc(void *context){

   PISND_music mp = (PISND_music)context;
                              //update playback
   for(;;){
      WaitForSingleObject(mp->h_timer_event, INFINITE);
      mp->DS_Update();
   }
}

//-------------------------------------

void ISND_music::TimerInit(){

   TIMECAPS tc;
   if(timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR){
        /* Error; application can't continue. */
   }
   wTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
   timeBeginPeriod(wTimerRes);

   h_timer_event = CreateEvent(NULL, false, false, NULL);
                              //setup timer thread
   dword tid;
   h_timer = CreateThread(NULL, 2048, TimerThreatProc, this, 0, &tid);
   SetThreadPriority(h_timer, THREAD_PRIORITY_ABOVE_NORMAL);
}

//-------------------------------------

dword ISND_music::myTimerSet(float msInterval){

   float RateInt = msInterval;
   if(wTimerID) timeKillEvent(wTimerID);
   wTimerID = timeSetEvent(
      (int)RateInt,                         //Delay
      wTimerRes,                       //Resolution (global variable)
      (LPTIMECALLBACK)h_timer_event,
      0,                               //User data
      TIME_CALLBACK_EVENT_SET |
      TIME_PERIODIC);                  //Event type

   return wTimerID ? TIMERR_NOERROR : TIMERR_NOCANDO;
}

//-------------------------------------

void ISND_music::myTimerDone(){

   if(h_timer){
#ifndef NDEBUG
      bool b = 
#endif
         TerminateThread(h_timer, 0);
      assert(b);
      h_timer = NULL;
   }
   if(wTimerID){
#ifndef NDEBUG
      MMRESULT mr =
#endif
         timeKillEvent(wTimerID);
      assert(mr==TIMERR_NOERROR);
      wTimerID = 0;
      timeEndPeriod(wTimerRes);
   }
   if(h_timer_event){
      CloseHandle(h_timer_event);
      h_timer_event = NULL;
   }
}

//----------------------------


