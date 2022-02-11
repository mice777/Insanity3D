/*--------------------------------------------------------
   Copyright (c) 1999, 2000 Michal Bacik. 
   All rights reserved.

   File: Sound.cpp
   Content: Sound frame.
--------------------------------------------------------*/

#include "all.h"
#include "soundi3d.h"
#include "scene.h"
#include "camera.h"

#ifdef _DEBUG

//#define DEBUG_NO_3D_HW        //disable use of 3D sound hardware
//#define DEBUG_FORCE_SOFTWARE  //force sw sound buffer (also disables 3D hw usage)

//#define EMULATE_3DPOSITIONING //compute 3D positioning (by setting panning) ourselves
#endif

#define EMULATE_ATTENUATION   //compute attenuation (by setting volume) ourselves


#if defined _DEBUG && 0

#define SND_DEBUG(msg) drv->DEBUG(msg)
#define SND_PRINT(msg) drv->PRINT(msg)

#else

#define SND_DEBUG(n)
#define SND_PRINT(msg)

#endif


//----------------------------
//----------------------------

void S_sound_env_properties::SetupDefaults(){

                              //setup sound data defaults (EAX defaults)
   memset(this, 0, sizeof(S_sound_env_properties));
#ifdef USE_EAX
   room_level_lf = (float)(EAXLISTENER_DEFAULTROOM-EAXLISTENER_MINROOM) / (float)(EAXLISTENER_MAXROOM-EAXLISTENER_MINROOM);
   room_level_hf = (float)(EAXLISTENER_DEFAULTROOMHF-EAXLISTENER_MINROOMHF) / (float)(EAXLISTENER_MAXROOMHF-EAXLISTENER_MINROOMHF);
   decay_time = (int)(EAXLISTENER_DEFAULTDECAYTIME * 1000.0f);
   decay_hf_ratio = (EAXLISTENER_DEFAULTDECAYHFRATIO-EAXLISTENER_MINDECAYHFRATIO) / (EAXLISTENER_MAXDECAYHFRATIO-EAXLISTENER_MINDECAYHFRATIO);
   reflections = (float)(EAXLISTENER_DEFAULTREFLECTIONS-EAXLISTENER_MINREFLECTIONS) / (float)(EAXLISTENER_MAXREFLECTIONS-EAXLISTENER_MINREFLECTIONS);
   reflections_delay = (int)(EAXLISTENER_DEFAULTREFLECTIONSDELAY * 1000.0f);
   reverb = (float)(EAXLISTENER_DEFAULTREVERB-EAXLISTENER_MINREVERB) / (float)(EAXLISTENER_MAXREVERB-EAXLISTENER_MINREVERB);
   reverb_delay = (int)(EAXLISTENER_DEFAULTREVERBDELAY * 1000.0f);
   env_diffusion = EAXLISTENER_DEFAULTENVIRONMENTDIFFUSION;
#endif                        //USE_EAX
   name = NULL;
   emulation_volume = 1.0f;
}

//----------------------------

void S_sound_env_properties::MakeBlend(const S_sound_env_properties &ssp1, const S_sound_env_properties &ssp2,
   float b2, bool use_eax){

   float b1 = (1.0f - b2);

   if(use_eax){
      room_level_lf = b1 * ssp1.room_level_lf + b2 * ssp2.room_level_lf;
      room_level_hf = b1 * ssp1.room_level_hf + b2 * ssp2.room_level_hf;
      decay_time = FloatToInt(b1 * ssp1.decay_time + b2 * ssp2.decay_time);
      decay_hf_ratio = b1 * ssp1.decay_hf_ratio + b2 * ssp2.decay_hf_ratio;
      reflections = b1 * ssp1.reflections + b2 * ssp2.reflections;
      reflections_delay = FloatToInt(b1 * ssp1.reflections_delay + b2 * ssp2.reflections_delay);
      reverb = b1 * ssp1.reverb + b2 * ssp2.reverb;
      reverb_delay = FloatToInt(b1 * ssp1.reverb_delay + b2 * ssp2.reverb_delay);
      env_diffusion = b1 * ssp1.env_diffusion + b2 * ssp2.env_diffusion;
   }else{
      emulation_volume = b1 * ssp1.emulation_volume + b2 * ssp2.emulation_volume;
   }
}

//----------------------------
//----------------------------
                              //sounds
I3D_sound::I3D_sound(CPI3D_scene s):
   I3D_frame(s->GetDriver1()),
   scene(s),
   snd_src(NULL),
   voice(NULL),
                              //set D3D defaults
   volume_out(0.0f),
   cone_out(PI*.5f), cone_in(0.0f), cos_cone_in(1.0f), cos_cone_out(.7071f),
   snd_type(I3DSOUND_NULL),
   frequency(1.0f),
   virtual_volume(1.0f),
   state(STOP),
   snd_flags(SNDF_UPDATEPOS | SNDF_UPDATEDIR | SNDF_UPDATEDIST | SNDF_UPDATESCALE),
   fade_time(0),
   fade_count_down(0),
   pause_time(0),
   pause_pos(0),
   volume(1.0f)
{
   drv->AddCount(I3D_CLID_SOUND);
   type = FRAME_SOUND;
   enum_mask = ENUMF_SOUND;
   SetRange(1.0f, 10.0f);

   ((PI3D_scene)scene)->AddRef();
   ((PI3D_scene)scene)->AddXRef();
}

//----------------------------

I3D_sound::~I3D_sound(){

   Close();
                              //unregister from driver list
   for(int i=drv->sounds.size(); i--; ){
      if(drv->sounds[i]==this){
         drv->sounds[i]=drv->sounds.back(); drv->sounds.pop_back();
         break;
      }
   }
   assert(i!=-1);
   while(sound_sectors.size())
      sound_sectors[0]->RemoveSound(this);
   drv->DecCount(I3D_CLID_SOUND);

   i = FindPointerInArray((void**)&scene->sounds.front(), scene->sounds.size(), this);
   if(i!=-1){
      scene->sounds[i] = scene->sounds.back();
      scene->sounds.pop_back();
   }
   ((PI3D_scene)scene)->ReleaseXRef();
   ((PI3D_scene)scene)->Release();
}

//----------------------------

I3D_RESULT I3D_sound::Duplicate(CPI3D_frame frm){

   if(type==frm->GetType1()){
      CPI3D_sound s1 = I3DCAST_CSOUND(frm);
      Close();

      SetSoundType(s1->snd_type);
      SetVolume(s1->volume * drv->GetGlobalSoundVolume());
      SetOutVol(s1->volume_out);
      SetCone(s1->cone_in, s1->cone_out);
      SetRange(s1->range_n, s1->range_f);
      SetLoop(s1->IsLoop());
      SetStreaming(s1->IsStreaming());
      fade_count_down = 0;
      fade_time = 0;
      file_name = s1->file_name;
      open_flags = s1->open_flags;
      frequency = s1->frequency;
      pause_time = 0;
      pause_pos = 0;
      if(snd_src) snd_src->Release();
      snd_src = s1->snd_src;
      if(snd_src) snd_src->AddRef();
      sound_sectors = s1->sound_sectors;
      state = STOP;
      virtual_volume = 1.0f;
      FreeVoice();
      SetOn(s1->IsOn1());
      //Open(s1->GetFileName(), s1->open_flags);
   }
   return I3D_frame::Duplicate(frm);
}

//----------------------------

I3D_RESULT I3D_sound::SetSoundType(I3D_SOUNDTYPE st){

   if(st>I3DSOUND_POINTAMBIENT)
      return I3DERR_INVALIDPARAMS;

   FreeVoice();
   snd_type = st;
   snd_flags |= SNDF_UPDATEOUTVOL;     //we may simulate DSound point sounds by setting out vol=1.0

   return ISND_OK;
}

//----------------------------

I3D_RESULT I3D_sound::Open(const char *fname, dword flags, I3D_LOAD_CB_PROC *cb_proc, void *context){

   if(flags & ~(I3DLOAD_SOUND_FREQUENCY | I3DLOAD_SOUND_SOFTWARE | I3DLOAD_LOG | I3DLOAD_PROGRESS | I3DLOAD_ERRORS))
      return I3DERR_INVALIDPARAMS;
   Close();

   file_name = fname;
                              //make filename lower-case (for comparision)
   file_name.ToLower();
   open_flags = flags;
   frequency = 1.0f;

                              //try to reuse existing sample
   for(dword i=drv->sounds.size(); i--; ){
      PI3D_sound snd = drv->sounds[i];
                              //try to duplicate sound
      if(snd->file_name==fname && snd->snd_src){
         snd_src = snd->snd_src;
         if(snd_src) snd_src->AddRef();
         if(IsOn()) SetOn(true);
         return I3D_OK;
      }
   }

   PISND_driver lpis = drv->GetSoundInterface();
   if(!lpis)
      return I3DERR_NOTINITIALIZED;
                              //create sound source
   lpis->CreateSource(&snd_src);

   ISND_RESULT ir;
                              //look to all dirs
   const C_vector<C_str> &sounds_dirs = drv->GetDirs(I3DDIR_SOUNDS);
   for(i=0; i<sounds_dirs.size(); i++){
      C_str str;
      str = sounds_dirs[i].Size() ? C_fstr("%s\\%s", (const char*)sounds_dirs[i], fname) :
         fname;

      dword src_open_flags = ISND_PRELOAD;
      if(snd_flags&SNDF_STREAMING)
         src_open_flags = ISND_STREAMING;
      ir = snd_src->Open(str, src_open_flags);
      if(ISND_SUCCESS(ir)){
         SND_PRINT(C_fstr("Opened source from location '%s'.", (const char*)str));
         if(IsOn())
            SetOn(true);
         return I3D_OK;
      }
      if(ir==ISNDERR_OUTOFMEM)
         goto fail;
   }
   ir = ISNDERR_NOFILE;
fail:
   SND_PRINT(C_fstr("Failed to open source '%s'.", fname));
   snd_src->Release();
   snd_src = NULL;
                              //open error
   switch(ir){
   case ISNDERR_OUTOFMEM: return I3DERR_OUTOFMEM;
   case ISNDERR_NOFILE: return I3DERR_NOFILE;
   default: return I3DERR_GENERIC;
   }
}

//----------------------------

void I3D_sound::Close(){

   if(IsPlaying()){
      if(voice)
         voice->Stop();
      state = STOP;
   }

   if(snd_src){
      snd_src->Release();
      snd_src = NULL;
   }
   file_name = NULL;

   FreeVoice();
   state = STOP;
}

//----------------------------

void I3D_sound::SetOn(bool on){

   I3D_frame::SetOn(on);

   if(on){
      if(voice && IsPlaying())
         voice->Stop();
                              //use real time, it may be long since we last rendered
      int timer = drv->GetGraphInterface()->ReadTimer();
      if(voice){
         voice->SetCurrTime(pause_pos = 0);
         pause_time = timer;
      }else{
         pause_pos = 0;
         pause_time = timer;
      }
      state = PAUSE;

      if(scene->GetActiveCamera1()){
         dword num_sectors = sound_sectors.size();
         if(num_sectors){
                                 //check if camera is in one of sectors we're in
            int i = FindPointerInArray((void**)&sound_sectors.front(), num_sectors,
               scene->GetActiveCamera1()->GetCurrSector());
            if(i!=-1){
               fade_count_down = fade_time = 1000;
            }
         }
      }
   }else{
      if(IsPlaying()){
         if(voice)
            voice->Stop();
         state = STOP;
      }
   }
}

//----------------------------

void I3D_sound::SetLoop(bool b){

   snd_flags &= ~SNDF_LOOPED;
   if(b)
      snd_flags |= SNDF_LOOPED;
}

//----------------------------

I3D_RESULT I3D_sound::SetStreaming(bool b){

   snd_flags &= ~SNDF_STREAMING;
                              //possible crashes and other problems with streaming
                              // don't do this now
   //if(b) snd_flags |= SNDF_STREAMING;
   return I3D_OK;
}

//----------------------------

bool I3D_sound::IsPlaying() const{

   if(!snd_src) return false;
                              //playing means PLAY or PAUSE (PAUSE is internal)
   return (state!=STOP);
}

//----------------------------

dword I3D_sound::GetPlayTime() const{

   if(!snd_src)
      return 0;
   const S_wave_format &wf = *snd_src->GetFormat();
   return wf.size * 1000 / (wf.samples_per_sec*wf.bytes_per_sample);
}

//----------------------------

dword I3D_sound::GetCurrTime() const{

   if(!snd_src) return 0;

   if(state==PAUSE){
                              //use real time, it may be long since we last rendered
      int timer = drv->GetGraphInterface()->ReadTimer();
      int ret = pause_pos + timer - pause_time;
      int len = GetPlayTime();
      if(ret >= len)          //check overflow
      if(snd_flags&SNDF_LOOPED) ret %= len;  //loop
      else ret = len;         //clamp
      return ret;
   }
   if(!voice) return 0;
   return voice->GetCurrTime();
}

//----------------------------

I3D_RESULT I3D_sound::SetCurrTime(dword pos){

   if(!snd_src) return I3DERR_NOTINITIALIZED;

   switch(state){
   case PAUSE:
      {
                              //use real time, it may be long since we last rendered   
         int timer = drv->GetGraphInterface()->ReadTimer();
         int virt = pause_pos + timer - pause_time;
         int delta = pos - virt;
         pause_time -= delta;
      }
      break;
   case PLAY:
      assert(voice);
      voice->SetCurrTime(pos);
      break;
   }
   return I3D_OK;
}

//----------------------------

I3D_RESULT I3D_sound::SetFrequency(float fr){

   if(!(open_flags&I3DLOAD_SOUND_FREQUENCY)){
      return I3DERR_UNSUPPORTED;
   }
   frequency = fr;
   snd_flags |= SNDF_UPDATEFREQ;

   return I3D_OK;
}

//----------------------------

void I3D_sound::SetRange(float min, float max){

   range_n = min;
   range_f = max;
   snd_flags |= SNDF_UPDATESCALE;
   snd_flags |= SNDF_UPDATEDIST;
}

//----------------------------

void I3D_sound::SetCone(float ia1, float oa1){

   cone_in = ia1;
   cone_out = oa1;
   cos_cone_in = (float)cos(cone_in*.5f);
   cos_cone_out = (float)cos(cone_out*.5f);
   snd_flags |= SNDF_UPDATECONE;
}

//----------------------------

void I3D_sound::SetOutVol(float v){

   volume_out = v;
   snd_flags |= SNDF_UPDATEOUTVOL;
}

//----------------------------

void I3D_sound::MakeVoice(){

   CPI3D_camera cam = scene->GetActiveCamera1();

   dword flags = open_flags;
   bool use_3d_hw = true;
#ifdef DEBUG_NO_3D_HW
   use_3d_hw = false;
#endif

   dword isnd_open_flags = 0;

   PISND_driver lpis = drv->GetSoundInterface();

   dword caps[6];
   lpis->GetCaps(caps);

                              //check if we don't have any 3D hardware,
                              // go with emulation
   if(!caps[ISNDCAPS_NUM_3D_ALL_BUFFERS])
      use_3d_hw = false;

                              //use hardware buffer, if available
#ifndef DEBUG_FORCE_SOFTWARE
   bool use_hw = caps[ISNDCAPS_NUM_HW_ALL_BUFFERS];
#else
   bool use_hw = false;
   use_3d_hw = false;
   isnd_open_flags |= ISND_SYSMEM;
#endif

                              //streaming sounds always in software
   if(snd_flags&SNDF_STREAMING){
      use_3d_hw = false;
      use_hw = false;
      isnd_open_flags |= ISND_SYSMEM;
   }

   switch(snd_type){
   case I3DSOUND_AMBIENT:
   case I3DSOUND_VOLUME:
                              //not need for 3D sound
      use_3d_hw = false;
      break;
   case I3DSOUND_POINTAMBIENT:
      isnd_open_flags |= ISND_SYSMEM;
      use_hw = false;
      use_3d_hw = false;
      break;
   }

   if(flags&I3DLOAD_SOUND_SOFTWARE){
      isnd_open_flags |= ISND_SYSMEM;
      use_hw = false;
      use_3d_hw = false;
   }

   if(flags&I3DLOAD_SOUND_FREQUENCY){
                              //frequency-adjustable sounds, deal with them special way -
                              // force software mixing
      isnd_open_flags |= ISND_FREQUENCY | ISND_SYSMEM;
      use_hw = false;
      use_3d_hw = false;
   }

   const S_wave_format *wf = snd_src->GetFormat();
   if(wf->num_channels > 1){
                              //check if ambient
      if(snd_type!=I3DSOUND_AMBIENT && snd_type!=I3DSOUND_POINTAMBIENT)
         return;
                              //stereo sounds can't use 3D acceleration
      use_3d_hw = false;
   }

   if(use_hw && use_3d_hw){
      isnd_open_flags |= ISND_3DBUFFER;
                              //no panning on real 3D buffers
      isnd_open_flags |= ISND_NOPANNING;
   }

   snd_flags &= ~SNDF_EMULATED;


                              //create sound voice
   ISND_RESULT ir;
   ir = lpis->Create3DSound(&voice);
   if(ISND_FAIL(ir))
      return;

   if(use_hw){                 //locate on card
      isnd_open_flags |= ISND_CARDMEM;
   }

                              //try to open until we succeed
   while(true){
      ir = voice->Open(snd_src, isnd_open_flags);

      if(ISND_SUCCESS(ir)){
         SND_PRINT(C_fstr("Alloc voice ok."));
         if(!use_3d_hw)       //emulate if not a 3D sound buffer
            snd_flags |= SNDF_EMULATED;
         if(use_hw)
            snd_flags |= SNDF_HARDWARE;
         break;
      }
      SND_PRINT(C_fstr("Alloc voice - failed, free farthest."));
      if(use_3d_hw || use_hw){
         const S_vector &cam_pos = cam->GetMatrixDirect()(3);
                              //hardware requested, try to free most far paused hw buffer
         float max_dist = 0.0f;
         PI3D_sound fahrest_voice = NULL;
         for(int i=drv->sounds.size(); i--; ){
            PI3D_sound snd = drv->sounds[i];
            if(snd!=this && snd->voice && snd->state==PAUSE){
               bool check_this_voice = false;
               if(use_3d_hw && !(snd->snd_flags&SNDF_EMULATED))
                  check_this_voice = true;
               else
               if(use_hw && (snd->snd_flags&SNDF_HARDWARE))
                  check_this_voice = true;

               if(check_this_voice){
                              //paused hw buffer, determine distance
                  float d = (snd->GetWorldPos()-cam_pos).Magnitude();
                  d -= snd->scaled_range_f;
                  if(max_dist<d){
                     max_dist = d;
                     fahrest_voice = snd;
                  }
               }
            }
         }
         if(fahrest_voice){
            SND_PRINT(C_fstr("Alloc voice - freeing '%s'.", (const char*)fahrest_voice->GetName1()));
                              //a voice to be freed found, free it and continue trying
            fahrest_voice->FreeVoice();
            continue;
         }
                              //failed to free voice
         if(use_3d_hw){
                              //we're out of 3D buffers, use non-3d
            use_3d_hw = false;
            isnd_open_flags &= ~(ISND_3DBUFFER | ISND_NOPANNING);
            SND_PRINT(C_fstr("Out of 3D buffers, use non-3D."));
         }else{
                              //we're out of hw buffers, use software
            use_hw = false;
            isnd_open_flags &= ~ISND_CARDMEM;
            SND_PRINT(C_fstr("Out of hw buffers, use sw."));
         }
      }else{
                              //unknown error - cannot allocate sysmem voice?!?
         SND_PRINT(C_fstr("Can't allocate - unknown error."));
         voice->Release();
         voice = NULL;
         break;
      }
   }
   pause_pos = 0;
   return;
}

//----------------------------

void I3D_sound::FreeVoice(){

   VirtualPause();
   if(voice){
      SND_PRINT(C_fstr("Releasing voice."));
      voice->Release();
      voice = NULL;
      snd_flags |= (SNDF_UPDATEPOS | SNDF_UPDATEDIR | SNDF_UPDATEDIST | SNDF_UPDATECONE | SNDF_UPDATEOUTVOL | SNDF_UPDATEFREQ);
   }
}

//----------------------------

void I3D_sound::VirtualPause(){

   if(state==PLAY){
                              //pause sound and save pause time
      voice->Stop();
      state = PAUSE;
                              //keep pause position and time, so that we may restore
                              // playing from actual position at the time
      pause_pos = voice->GetCurrTime();
                              //use render time, we're called from rendering
      int timer = drv->render_time;

      pause_time = timer;
      fade_count_down = 0;

      SND_PRINT(C_fstr("Virtual pause, pos: %i, time: %i.", pause_pos, pause_time));
   }
}

//----------------------------

void I3D_sound::VirtualPlay(){

   if(state == PAUSE){
                              //advance play position
      int timer = drv->render_time;
                              //use render time, we're called from rendering
      dword pos = pause_pos + timer - pause_time;
      if(pos >= snd_src->GetPlayTime()){
                              //wrapping around - non-looped samples will be stopped
         if(!(snd_flags&SNDF_LOOPED)){
            SND_PRINT(C_fstr("Virtual resume: non-looped voice stopped."));
            if(voice)
               voice->Stop();
            state = STOP;
            return;
         }
         pos %= snd_src->GetPlayTime();
      }
      voice->SetCurrTime(pos);
                              //unpause sound
      state = PLAY;
      SND_PRINT(C_fstr("Virtual resume, pos: %i.", pos));
   }
}

//----------------------------

bool I3D_sound::UpdateFading(int time, bool in, float &new_volume){

   assert(fade_count_down > 0);

   if(in){
      if((fade_count_down += time) >= fade_time){
         //SND_PRINT(C_fstr("Fade in finished."));
         fade_count_down = 0;
         return true;
      }
   }else{
      if((fade_count_down -= time) <= 0){
         SND_PRINT(C_fstr("Fade off finished."));
         fade_count_down = 0;
         return true;
      }
   }
   assert(fade_time);
   if(!fade_time)
      return true;

   float fade_ratio = (float)fade_count_down / (float)fade_time;
   SND_DEBUG(C_fstr("fade ratio: %.3f.", fade_ratio));
   new_volume *= fade_ratio;
   return false;
}

//----------------------------

bool I3D_sound::CheckIfAudible(int time, float &new_volume,
   float &curr_dist, float &dir_cos){

   CPI3D_camera cam = scene->GetActiveCamera1();
   dir_cos = 0;

   bool sector_ok = true;
   {
      dword num_sectors = sound_sectors.size();
      if(num_sectors){
                              //check if camera is in one of sectors we're in
         int i = FindPointerInArray((void**)&sound_sectors.front(), num_sectors,
            cam->GetCurrSector());
         if(i!=-1){
                              //sector found
            //fade_time = cam->GetCurrSector()->GetSoundProps().switch_fade_time;
            fade_time = 1000;
         }else{
                              //not in any sector
            sector_ok = false;
         }
      }
   }

   if(!sector_ok){
      if(state!=PLAY)
         return false;
                              //if currently playing, setup fade-out
      if(!fade_count_down){
         SND_PRINT(C_fstr("not in any audible sector, starting fade down."));
         assert(fade_time >= 0);
         fade_count_down = fade_time;
         if(!fade_count_down)
            return false;
      }
      if(UpdateFading(time, false, new_volume)){
                              //fading finished
         return false;
      }
                              //flow... (continue in attenuation computation)
   }else{
                              //sector ok, start fading on if sound's off
      if(state!=PLAY){
         if(!fade_count_down){
            SND_PRINT(C_fstr("in audible sector, starting fade on."));
            fade_count_down = 1;
         }
      }
   }

   switch(snd_type){

   case I3DSOUND_AMBIENT:
      if(sector_ok && fade_count_down)
         UpdateFading(time, true, new_volume);
      new_volume *= volume;
      return true;

   case I3DSOUND_POINT:
   case I3DSOUND_SPOT:
   case I3DSOUND_POINTAMBIENT:
      {
                              //get listener/sound positions
         const S_matrix &m = GetMatrixDirect();
         const S_matrix &cm = cam->GetMatrixDirect();
         const S_vector &snd_pos = m(3);
         const S_vector &cam_pos = cm(3);
                              //get direction vector
         S_vector dir_to_cam = cam_pos - snd_pos;
         float curr_dist_2 = dir_to_cam.Dot(dir_to_cam);

                              //check for maximal distance
         if(curr_dist_2<scaled_range_f_2){
            curr_dist = I3DSqrt(curr_dist_2);
            bool audible = false;

            if(snd_type==I3DSOUND_SPOT){
               if(!IsMrgZeroLess(curr_dist)){
                              //check outside cone
                  dir_to_cam /= curr_dist;

                  S_vector snd_dir = m(2);
                  snd_dir.Normalize();
                  dir_cos = dir_to_cam.Dot(snd_dir);
                  if(volume_out || dir_cos>cos_cone_out){
                     audible = true;
                  }
               }else
                  audible = true;
            }else
               audible = true;

            if(audible){
               if(sector_ok && fade_count_down)
                  UpdateFading(time, true, new_volume);

                              //apply sound's volume
               new_volume *= volume;
               return true;
            }
         }
      }
      break;
   }
   return false;
}

//----------------------------

void I3D_sound::ComputeAttenuation(float curr_dist, float dir_cos, float &new_volume){

   switch(snd_type){
   case I3DSOUND_SPOT:
                              //apply cone volume
      if(dir_cos<cos_cone_in){
         if(dir_cos>cos_cone_out){
                              //apply cone volume
            new_volume *=
               volume_out + (1.0f-volume_out)*(dir_cos-cos_cone_out) / (cos_cone_in-cos_cone_out);
         }else
            new_volume *= volume_out;
      }
                              //flow...
   case I3DSOUND_POINT:
   case I3DSOUND_POINTAMBIENT:
                                 //apply distance
      if(curr_dist > scaled_range_n){
         assert(curr_dist <= scaled_range_f);
         float delta = scaled_range_f - scaled_range_n;
         if(!IsMrgZeroLess(delta)){
            float f = (scaled_range_f - curr_dist) / delta;
            new_volume *= f;
         }
      }
      break;
   }
}

//----------------------------

void I3D_sound::ComputePanning(){

   CPI3D_camera listener = scene->GetActiveCamera1();

   switch(snd_type){
   case I3DSOUND_SPOT:
   case I3DSOUND_POINT:
      {
                              //emulate position by panning
         const S_matrix &m = GetMatrixDirect();
         const S_matrix &lm = listener->GetMatrixDirect();
         const S_vector &l_dir = lm(2);
         S_vector dir_to_cam = lm(3) - m(3);
         float ca = (!IsAbsMrgZero(l_dir.z)) ? (float)atan(l_dir.x/l_dir.z) : PI*.5f;
         if(l_dir.z<0.0f) ca += PI;
         if(l_dir.x<0.0f) ca += (PI*2.0f);
         float sa = (!IsAbsMrgZero(dir_to_cam.z)) ? (float)atan(dir_to_cam.x/dir_to_cam.z) : PI*.5f;
         if(dir_to_cam.z>=0.0f) sa += PI;
         if(dir_to_cam.x>=0.0f) sa += (PI*2.0f);
         float diff = sa - ca;
         float pan = (float)sin(diff);
         voice->SetPanning(pan);
      }
      break;
   }
}

//----------------------------

void I3D_sound::UpdateSound(int time){

   assert(!(frm_flags&FRMFLAGS_CHANGE_MAT));
   /*
   if(frm_flags&FRMFLAGS_CHANGE_MAT)
      snd_flags |= (SNDF_UPDATEPOS | SNDF_UPDATEDIR | SNDF_UPDATESCALE);
   */

   if(!snd_src || (state==STOP))
      return;

   const S_matrix &m = GetMatrix();

   SND_DEBUG(C_fstr("Sound '%s':", (const char*)GetName1()));

                              //update scale first, because it affects our distance computations
   if(snd_flags&SNDF_UPDATESCALE){
      float f = m(0).Magnitude();
      scaled_range_n = f * range_n;
      scaled_range_f = f * range_f;

                           //run-time values
      scaled_range_f_2 = scaled_range_f * scaled_range_f;
      f = scaled_range_f - scaled_range_n;

      snd_flags &= ~SNDF_UPDATESCALE;
   }

   bool audible = false;

   float new_volume = 1.0f;

   float curr_dist = 0;
   float dir_cos = 0;             //valid and used only for spot sounds
   audible = CheckIfAudible(time, new_volume, curr_dist, dir_cos);
   SND_DEBUG(C_fstr("audible: %i", audible));

                              //toggle sound pause based on if audible
   if(!audible){
      switch(state){
      case PLAY:
         VirtualPause();
         break;
      case PAUSE:             
                              //check non-looped sample for finish
         if(!(snd_flags&SNDF_LOOPED)){
            int timer = drv->render_time;
                              //use render time, we're called from rendering
            dword pos = pause_pos + timer - pause_time;
            if(pos >= snd_src->GetPlayTime()){
               SND_PRINT(C_fstr("Virtual pause - loop over."));
               SetOn(false);
            }
         }
         break;
      }
   }else{
      bool start = false;

      if(!voice){
                              //create and upload sample
         MakeVoice();
         if(!voice)           //failed to create voice?
            return;
      }
                              //update frequency
      if(snd_flags&SNDF_UPDATEFREQ){
         voice->SetFrequency(frequency);
         snd_flags &= ~SNDF_UPDATEFREQ;
      }
      if(snd_flags&SNDF_EMULATED){
         ComputeAttenuation(curr_dist, dir_cos, new_volume);
         ComputePanning();
      }else{
#ifdef EMULATE_ATTENUATION
         ComputeAttenuation(curr_dist, dir_cos, new_volume);
#else
         if(snd_flags&SNDF_UPDATEDIR)
            voice->SetDir(m(2));
         if(snd_flags&SNDF_UPDATEDIST)
            voice->SetDistances(scaled_range_n, scaled_range_f);
         if(snd_flags&SNDF_UPDATEOUTVOL)
            voice->SetOutVolume((snd_type==I3DSOUND_SPOT) ? volume_out : 1.0f);
         if(snd_flags&SNDF_UPDATECONE)
            voice->SetCone(cone_in, cone_out);
#endif//!EMULATE_ATTENUATION

#ifdef EMULATE_3DPOSITIONING
         //EmulatePanning(cam);
#else
                              //update 3D hw sound
         if(snd_flags&SNDF_UPDATEPOS)
            voice->SetPos(m(3));
#endif

         snd_flags &= ~(SNDF_UPDATEPOS | SNDF_UPDATEDIR | SNDF_UPDATEDIST | SNDF_UPDATECONE | SNDF_UPDATEOUTVOL);
      }

      if(state == I3D_sound::PAUSE){
         VirtualPlay();
                              //looped voices won't get play
         if(state!=PLAY)
            return;

         start = true;
                              //force re-applying volume if buffer is restarted
         virtual_volume = -1.0f;
      }else{                  
                              //check non-looped sample for finish
         if(!(snd_flags&SNDF_LOOPED) && !voice->IsPlaying()){
            if(voice){
               SND_PRINT(C_fstr("Loop over, stopping sound."));
               voice->Stop();
            }
            state = STOP;
            return;
         }
      }
                              //update volume
      SND_DEBUG(C_fstr("volume: %.3f.", new_volume));
      if(!IsAbsMrgZero(virtual_volume-new_volume)){
         virtual_volume = new_volume;
         //SND_PRINT(C_fstr("Setting volume: %.3f.", new_volume));
         voice->SetVolume(new_volume * drv->GetGlobalSoundVolume());
         SND_PRINT(C_fstr("applying volume: %.3f.", new_volume));
      }

      if(start){
         ISND_RESULT ir;
         ir = voice->Play((snd_flags&SNDF_LOOPED) ? ISND_LOOP : 0);
         assert(ISND_SUCCESS(ir));
      }
                              //stats
      ++scene->scene_stats.frame_count[FRAME_SOUND];
   }
}

//----------------------------

void I3D_sound::ReapplyVolume(bool hard){
   if(voice && hard){
      voice->SetVolume(1.0f);
      voice->SetVolume(0.0f);
   }
   virtual_volume = -1.0f;
}

//----------------------------

void I3D_sound::GetRange(float &min, float &max) const{

   min = range_n, max = range_f;
}

//----------------------------

void I3D_sound::GetCone(float &in_angle, float &out_angle) const{

   in_angle = cone_in;
   out_angle = cone_out;
}

//----------------------------

void I3D_sound::SetVolume(float v){

   volume = v;
}

//----------------------------
#define SPHERE_NUM_VERTS 24
static word sphere_c_list[SPHERE_NUM_VERTS*2];


void I3D_sound::Draw1(PI3D_scene scene, const S_view_frustum *vf, bool strong) const{

                              //sphere values
   static struct S_auto_init{
      S_auto_init(){
                                 //init volume sphere
         for(int i=0; i<SPHERE_NUM_VERTS; i++){
            sphere_c_list[i*2+0] = word(i + 0);
            sphere_c_list[i*2+1] = word(i + 1);
         }
         sphere_c_list[i*2-1] = 0;
      }
   } auto_init;

   //static const S_vector color_cone_out(.0, .75f, .8f), color_cone_in(.0f, .5f, .5f);
   static const dword color_cone_out = 0x00c0cc, color_cone_in = 0x008080;

   dword alpha(strong ? 0xff : 0x50);

   switch(snd_type){

   case I3DSOUND_NULL:
      if(vf){                 //check clipping
         bool clip;
         if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
            return;
      }
      scene->DrawIcon(GetWorldPos(), 3, (alpha<<24) | 0xffffff);
      break;

   case I3DSOUND_POINT:
   case I3DSOUND_POINTAMBIENT:
      scene->DebugDrawSphere(GetMatrix(), range_f, (alpha<<24) | color_cone_out);
      scene->DebugDrawSphere(GetMatrix(), range_n, (alpha<<24) | color_cone_in);
      if(vf){                 //check clipping
         bool clip;
         if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
            return;
      }
      scene->DrawIcon(GetWorldPos(), 3, (alpha<<24) | 0xffffff);
      break;

   case I3DSOUND_SPOT:
      {
         float cone[2];
         GetCone(cone[0], cone[1]);
         cone[0] *= .5f; cone[1] *= .5f;
         float rng[2] = { range_n, range_f };
         S_matrix m = I3DGetIdentityMatrix();
         const int sphere_num_verts = 24;
         S_vector sphere_circle[sphere_num_verts+1];
                              //outer range
         float cosc = (float)cos(cone[1]);
         float sinc = (float)sin(cone[1]);
         for(int j=0; j<sphere_num_verts; j++){
            sphere_circle[j].z = cosc;
            float f((float)j*(PI*2.0f) / (float)sphere_num_verts);
            sphere_circle[j].x = sinc * (float)cos(f);
            sphere_circle[j].y = sinc * (float)sin(f);
         }
         sphere_circle[sphere_num_verts].Zero();
         for(int ii=0; ii<2; ii++){
            m(0, 0) = m(1, 1) = m(2, 2) = rng[ii];
            scene->SetRenderMatrix(m*GetMatrix());
            const dword color = !ii ? color_cone_in : color_cone_out;
            scene->DrawLines(sphere_circle, sphere_num_verts, sphere_c_list, sphere_num_verts*2, (alpha<<24) | color);
                                 //connecting lines
            static const word indx[]={
               sphere_num_verts, 0,
               sphere_num_verts, sphere_num_verts/4,
               sphere_num_verts, sphere_num_verts/2,
               sphere_num_verts, sphere_num_verts*3/4,
            };
            scene->DrawLines(sphere_circle, sphere_num_verts+1,
               indx, sizeof(indx)/sizeof(word), (alpha<<24) | color_cone_out);
         }
         if(vf){                 //check clipping
            bool clip;
            if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
               return;
         }
         scene->DrawIcon(GetWorldPos(), 3, (alpha<<24) | 0xffffff);
      }
      break;

   case I3DSOUND_AMBIENT:
      if(vf){                 //check clipping
         bool clip;
         if(!SphereInVF(*vf, I3D_bsphere(GetWorldPos(), .5f), clip))
            return;
      }
      scene->DrawIcon(GetWorldPos(), 8, (alpha<<24) | 0xffffff);
      break;
   }
}

//----------------------------
//----------------------------
