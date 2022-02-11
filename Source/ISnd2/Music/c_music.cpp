#include "all.h"
#include "C_music.h"
#include <math.h>


//----------------------------
                              //temp:
extern LOADER *firstloader;

//----------------------------

static short lin2log(dword val){

   if(!val) return 0;
                                       // 1..64 -> 0..10000 logarithmic */
   return (short)(10000/(16*.69314718)*log((double)(val*1024)));
}

//-------------------------------------

static short volconv(short vol){

   return -10000+lin2log(vol);
}

//-------------------------------------

static short panconv(short pan){

   pan-=128;
   if(pan<0) return -10000+lin2log(64+pan/2);
   return 10000-lin2log(64-(pan+1)/2);
}

//----------------------------
//----------------------------

LOADER *firstloader = NULL;

//----------------------------

void RegisterLoader(LOADER *ldr){

	if(firstloader==NULL){
		firstloader = ldr;
		ldr->next = NULL;
   }else{
		ldr->next = firstloader;
		firstloader = ldr;
	}
}

//----------------------------
//----------------------------

extern LOADER load_mod;
extern LOADER load_xm;

static const class C_auto_reg{
public:
   C_auto_reg(){
      RegisterLoader(&load_mod);
      RegisterLoader(&load_xm);
   }
} auto_reg;

//----------------------------
//----------------------------

bool C_unimod::AllocPatterns(){

   patterns = new word[num_patterns * num_chnannels];
   if(!patterns)
      return false;
   memset(patterns, 0, sizeof(word) * num_patterns * num_chnannels);

   pattrows = new word[num_patterns];
   if(!pattrows)
      return false;
   memset(pattrows, 0, sizeof(word) * num_patterns);

   int tracks = 0;
   for(int t=0; t<num_patterns; t++){
      pattrows[t] = 64;
      for(int s = 0; s<num_chnannels; s++){
         patterns[(t * num_chnannels) + s] = tracks++;
      }
   }
   return true;
}

//----------------------------
//----------------------------

ISND_music::ISND_music(PISND_driver drv1):
   ref(1),
   drv(drv1),
   is_open(false),
   is_playing(false),
   //mp_panning(true),
   loop(true),
   volume(1.0f),
   DS_BPM(0),
   wTimerID(0),
   wTimerRes(0),
   h_timer(NULL),
   h_timer_event(NULL)
{
   InitializeCriticalSection(&timer_lock);
   memset(ghld, 0, sizeof(ghld));
   memset(lpSamp, 0, sizeof(lpSamp));
   memset(is_sam_playing, 0, sizeof(is_sam_playing));
   b_16bit = true;

   int i;
   for(i=0; i<256; i++) pantab[i] = panconv(i);
   for(i=0; i<65; i++) voltab[i] = volconv(i);
}

//----------------------------

ISND_music::~ISND_music(){

   Close();
   DeleteCriticalSection(&timer_lock);
}

//----------------------------

bool ISND_music::Open(const char *name){

   if(!drv)
      return false;
   Close();
   C_cache *cc = new C_cache;
   if(!cc->open(name, CACHE_READ)){
      delete cc;
      return false;
   }
                              //load the track
   unimod.Init();

   if(!LoadHeader(cc)){
      unimod.UnInit();
   }else{
      if(!LoadSamples(cc)){
         unimod.UnInit();
      }else{
         is_open = true;
      }
   }

   cc->close();
   delete cc;

   if(!is_open)
      return false;

                              //init run-time
   curr_restart_position = 0;
   times_to_loop = 0;
   curr_speed = unimod.initspeed;
   tick_counter = curr_speed;
   pattern_break = 0;
   pattern_delay = 0;
   pattern_delay2 = 0;
   position_jump = 2;         //make sure the player fetches the first note
   song_position = 0;
   current_row = 0;
   global_volume = 64;
   //extended_speed_flag = true;

   for(int i=unimod.num_chnannels; i--; ){
      mp_audio[i].Reset();
   }
   beats_per_minute = unimod.inittempo;
   return true;
}

//----------------------------

bool ISND_music::LoadHeader(C_cache *cc){

                              //try to find a loader that recognizes the module
   for(LOADER *l=firstloader; l; l = l->next){
      cc->seekg(0);
      if(l->Test(cc))
         break;
   }
   if(!l){
      return false;
   }

   C_uni_recorder uni_rec;

   bool ok = false;
                              //init module loader
   if(l->Init()){
      cc->seekg(0);
      ok = l->Load(unimod, cc);
   }
   l->Cleanup();
   return ok;
}

//----------------------------

bool ISND_music::LoadSamples(C_cache *cc){

   for(int i=0; i<unimod.num_instruments; i++){
      const S_instrument *ip = &unimod.instruments[i];

		for(int j=0; j<ip->numsmp; j++){
         S_sample *s = &ip->samples[j];
                              //sample has to be loaded ? -> increase
                              //number of samples and allocate memory
                              //and load sample
			if(s->length){
            if(s->seekpos)
               cc->seekg(s->seekpos);
            s->handle =
               SampleLoad(cc, s->length, s->loopstart, s->loopend, s->flags);
				if(s->handle<0)
               return false;
			}
		}
	}
	return true;
}

//----------------------------

bool ISND_music::Close(){

   if(!is_open)
      return false;
   Stop();
   for(int i=unimod.num_instruments; i--; ){
      if(unimod.instruments[i].samples){
         for(int j=unimod.instruments[i].numsmp; j--; ){
            if(unimod.instruments[i].samples[j].handle != -1)
               SampleUnload(unimod.instruments[i].samples[j].handle);
         }
      }
   }
   unimod.UnInit();
   is_open = false;
   return true;
}

//----------------------------

bool ISND_music::Play(){

   if(IsOpen()){
      if(!is_playing){
         for(int i=0; i<unimod.num_chnannels; i++){
            ghld[i].flags = 0;
            ghld[i].handle = 0;
            ghld[i].kick = 0;
            ghld[i].frq = 10000;
            ghld[i].vol = 0;
            ghld[i].pan = (i&1) ? 0 : 255;
            ghld[i].samp = NULL;
         }

         TimerInit();
         DSSetBPM(125);

         is_playing = true;
      }
      return true;
   }
   return false;
}

//----------------------------

bool ISND_music::SetPlayTrack(int track){

   EnterCriticalSection(&timer_lock);
   if(track<0 || track>=unimod.num_positions)
      return false;
   position_jump = 2;
   pattern_break = 0;
   song_position = track;
   tick_counter = curr_speed;
   LeaveCriticalSection(&timer_lock);
   return true;
}

//----------------------------

int ISND_music::GetPlayTrack() const{

   return IsOpen() ? song_position : -1;
}

//----------------------------

void ISND_music::Stop(){

   if(is_playing){
      myTimerDone();
      for(int i=0; i<unimod.num_chnannels; i++){
         GHOLD *aud=&ghld[i];
         if(aud->samp){
            aud->samp->Stop();
            if (aud->is_dupl) aud->samp->Release();
            aud->samp = NULL;
         }
      }
      is_playing = false;
   }
}

//----------------------------

/*
ISND_RESULT ISND_driver::CreateMusic(PISND_music *mus){
   *mus = new ISND_music(this);
   return ISND_OK;
}
*/

//----------------------------
//----------------------------

