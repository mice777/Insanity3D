#ifndef __C_MUSIC_H_
#define __C_MUSIC_H_

#include "all.h"
#include "..\sound_i.h"

#pragma warning(disable: 4244)
#pragma warning(disable: 4706)

//----------------------------

dword getoldperiod(byte note, word c2spd);
inline dword getlinearperiod(byte note,word fine){
   return((10L*12*16*4)-((word)note*16*4)-(fine/2)+64);
}
dword getlogperiod(byte note,word fine);

//----------------------------
//----------------------------

//        all known effects:
enum{
   UNI_NULL,
   UNI_NOTE = 1,
   UNI_INSTRUMENT,
   UNI_PTEFFECT0,
   UNI_PTEFFECT1,
   UNI_PTEFFECT2,
   UNI_PTEFFECT3,
   UNI_PTEFFECT4,
   UNI_PTEFFECT5,
   UNI_PTEFFECT6,
   UNI_PTEFFECT7,
   UNI_PTEFFECT8,
   UNI_PTEFFECT9,
   UNI_PTEFFECTA,
   UNI_PTEFFECTB,
   UNI_PTEFFECTC,
   UNI_PTEFFECTD,
   UNI_PTEFFECTE,
   UNI_PTEFFECTF,
   UNI_S3MEFFECTA,
   UNI_S3MEFFECTD,
   UNI_S3MEFFECTE,
   UNI_S3MEFFECTF,
   UNI_S3MEFFECTI,
   UNI_S3MEFFECTQ,
   UNI_S3MEFFECTT,
   UNI_XMEFFECTA,
   UNI_XMEFFECTG,
   UNI_XMEFFECTH,
   UNI_XMEFFECTP,
   UNI_LAST
};


/**************************************************************************
****** Unitrack stuff: ****************************************************
**************************************************************************/

/*
	Ok.. I'll try to explain the new internal module format.. so here it goes:


	The UNITRK(tm) Format:
	======================

	A UNITRK stream is an array of bytes representing a single track
	of a pattern. It's made up of 'repeat/length' bytes, opcodes and
	operands (sort of a assembly language):

	rrrlllll
	[REP/LEN][OPCODE][OPERAND][OPCODE][OPERAND] [REP/LEN][OPCODE][OPERAND]..
	^                                         ^ ^
	|-------ROWS 0 - 0+REP of a track---------| |-------ROWS xx - xx+REP of a track...


	The rep/len byte contains the number of bytes in the current row,
	_including_ the length byte itself (So the LENGTH byte of row 0 in the
	previous example would have a value of 5). This makes it easy to search
	through a stream for a particular row. A track is concluded by a 0-value
	length byte.

	The upper 3 bits of the rep/len byte contain the number of times -1 this
	row is repeated for this track. (so a value of 7 means this row is repeated
	8 times)
*/

#define UniSetRow(r) const byte *rowpc = r, *rowend = r + (*(rowpc++)&0x1f)
#define UniGetByte() ((rowpc<rowend) ? *(rowpc++) : 0)

//----------------------------
                              //track recorder - used during conversion from other
                              // file formats
class C_uni_recorder{
   dword row_base_index;      //holds index of the rep/len byte of a row
   dword prev_row_base_index; //holds index to the previous row (needed for compressing)
   vector<byte> uni_buf;      //contents of recording
public:
   C_uni_recorder(){
      row_base_index = 0;     //reset index to rep/len byte
      prev_row_base_index = 0;//no previous row yet
      uni_buf.clear(); uni_buf.reserve(512);
      uni_buf.push_back(0);   //clear rep/len byte
   }
   inline void Write(byte b){
      uni_buf.push_back(b);
   }
   inline void WriteInstrument(byte ins){
                              //appends UNI_INSTRUMENT opcode to the unitrk stream
      Write(UNI_INSTRUMENT);
      Write(ins);
   }
   inline void WriteNote(byte note){
                              //appends UNI_NOTE opcode to the unitrk stream
      Write(UNI_NOTE);
      Write(note);
   }
   void WritePTEffect(byte eff, byte dat){
                              //appends UNI_PTEFFECTX opcode to the unitrk stream
      if(eff || dat){         //don't write empty effect
         Write(UNI_PTEFFECT0 + eff);
         Write(dat);
      }
   }
   inline void WriteEffect(byte eff, byte dat){
      Write(eff);
      Write(dat);
   }

   void Newline(){
                              //closes the current row of a unitrk stream (updates the rep/len byte)
                              // and sets pointers to start a new row.

                              //get repeat of previous row
      dword last_row_repeate = (uni_buf[prev_row_base_index] >> 5) + 1;
                              //get length of previous row
      dword last_row_len = uni_buf[prev_row_base_index] & 0x1f;

                              //get length of current row
      dword curr_row_len = uni_buf.size() - row_base_index;

                              //now, check if the previous and the current row are identical.
                              // when they are, just increase the repeat field of the previous row.
      if(last_row_repeate<8 && curr_row_len==last_row_len &&
         !memcmp(&uni_buf[prev_row_base_index+1], &uni_buf[row_base_index+1], curr_row_len-1)){

         uni_buf[prev_row_base_index] += 0x20;

         uni_buf.erase(uni_buf.begin()+row_base_index+1, uni_buf.end());
      }else{
                              //current and previous row aren't equal..
                              // so just update the pointers
         uni_buf[row_base_index] = curr_row_len;
         prev_row_base_index = row_base_index;
         row_base_index = uni_buf.size();
                              //begin next row
         uni_buf.push_back(0);
      }
   }

//----------------------------
// Get the contents of recording
   inline void GetBuffer(vector<byte> &buf) const{
      buf = uni_buf;
      buf[row_base_index] = 0;
   }
};

/**************************************************************************
****** mikmod types: ******************************************************
**************************************************************************/

//        Sample format flags:

#define SF_16BITS       1
#define SF_SIGNED       2
#define SF_DELTA        4
//#define SF_BIG_ENDIAN   8
#define SF_LOOP         16
//#define SF_BIDI         32
#define SF_OWNPAN       64
//#define SF_REVERSE      128

                              //Envelope flags:

#define EF_ON           1
#define EF_SUSTAIN      2
#define EF_LOOP         4

                              //Unimod flags

#define UF_XMPERIODS    1     //if set use XM periods/finetuning
#define UF_LINEAR       2     //if set use LINEAR periods


struct ENVPT{
   short pos;
   short val;
};

//----------------------------

struct S_sample{
   word c2spd;            /* finetune frequency */
   schar transpose;       /* transpose value */
   byte volume;           /* volume 0-64 */
   byte panning;          /* panning */
   ULONG length;           /* length of sample (in samples!) */
   ULONG loopstart;        /* repeat position (relative to start, in samples) */
   ULONG loopend;          /* repeat end */
   word flags;            /* sample format */
   ULONG seekpos;                  /* seek position in file */
   //C_str name;
   short handle;           /* sample handle */

   S_sample():
      c2spd(0),
      transpose(0),
      volume(0),
      panning(128),
      length(0),
      loopstart(0),
      loopend(0),
      flags(0),
      seekpos(0),
      handle(-1)
   {
   }
   S_sample(const S_sample &);
   S_sample &operator =(const S_sample&);
};

//----------------------------

struct S_instrument{
   byte numsmp;
   byte samplenumber[96];

   byte volflg;           /* bit 0: on 1: sustain 2: loop */
   byte volpts;
   byte volsus;
   byte volbeg;
   byte volend;
   ENVPT volenv[12];

   byte panflg;           /* bit 0: on 1: sustain 2: loop */
   byte panpts;
   byte pansus;
   byte panbeg;
   byte panend;
   ENVPT panenv[12];

   byte vibtype;
   byte vibsweep;
   byte vibdepth;
   byte vibrate;

   word volfade;
   //C_str name;
   S_sample *samples;

   S_instrument():
      numsmp(0),
      volflg(0),
      volpts(0),
      volsus(0),
      volbeg(0),
      volend(0),
      panflg(0),
      panpts(0),
      pansus(0),
      panbeg(0),
      panend(0),
      vibtype(0),
      vibsweep(0),
      vibdepth(0),
      vibrate(0),
      volfade(0),
      samples(NULL)
   {
      memset(samplenumber, 0, sizeof(samplenumber));
      memset(volenv, 0, sizeof(volenv));
      memset(panenv, 0, sizeof(panenv));
   }
   S_instrument(const S_instrument&);
   S_instrument &operator =(const S_instrument&);
   ~S_instrument(){
      delete[] samples;
   }
};

/**************************************************************************
****** Loader stuff: ******************************************************
**************************************************************************/

                              //loader structure
struct LOADER{
   LOADER *next;
   bool (*Init)();
   bool (*Test)(class C_cache*);
   bool (*Load)(class C_unimod &of, C_cache*);
   void (*Cleanup)();
};

/**************************************************************************
****** Driver stuff: ******************************************************
**************************************************************************/

//        max. number of handles a driver has to provide. (not strict)

#define MAXSAMPLEHANDLES 64

/**************************************************************************
****** Player stuff: ******************************************************
**************************************************************************/

struct ENVPR{
   byte flg;        /* envelope flag */
   byte pts;        /* number of envelope points */
   byte sus;        /* envelope sustain index */
   byte beg;        /* envelope loop begin */
   byte end;        /* envelope loop end */
   short p;          /* current envelope counter */
   word a;          /* envelope index a */
   word b;          /* envelope index b */
   ENVPT *env;       /* envelope points */
};

//----------------------------

struct S_channel{
   S_instrument *i;
   S_sample *s;

   word fadevol;

   ENVPR venv;
   ENVPR penv;

   byte keyon;                //key is pressed.
   byte kick;                 //sample has to be restarted
   byte sample;               //which sample number (0-31)
   short handle;              //which sample-handle

   ULONG start;               //start byte index in the sample

   byte panning;              //panning position
   byte pansspd;              //panslide speed

   schar volume;              //amiga volume (0 t/m 64) to play the sample at
   word period;               //period to play the sample at

                              //you should not have to use the values
                              // below in the player routine
   schar transpose;

   byte note;

   short ownper;
   short ownvol;

   byte *row;                 //row currently playing on this channel

   schar retrig;              //retrig value (0 means don't retrig)
   word c2spd;                //what finetune to use

   schar tmpvolume;

   word tmpperiod;            //tmp period
   word wantedperiod;         //period to slide to (with effect 3 or 5)

   word slidespeed;       
   word portspeed;            //noteslide speed (toneportamento)

   byte s3mtremor;            //s3m tremor (effect I) counter
   byte s3mtronof;            //s3m tremor ontime/offtime

   byte s3mvolslide;          //last used volslide

   byte s3mrtgspeed;          //last used retrig speed
   byte s3mrtgslide;          //last used retrig slide

   byte glissando;            //glissando (0 means off)
   byte wavecontrol;

   schar vibpos;              //current vibrato position
   byte vibspd;               //"" speed
   byte vibdepth;             //"" depth

   schar trmpos;              //current tremolo position
   byte trmspd;               //"" speed */
   byte trmdepth;             //"" depth

   word soffset;              //last used sample-offset (effect 9)

   void Reset(){
      memset(this, 0, sizeof(S_channel));
   }
};

//----------------------------

inline byte mmReadByte(C_cache *cc){
   byte c;
   cc->read((char*)&c, sizeof(byte));
   return c;
}

//----------------------------

inline schar mmReadSchar(C_cache *cc){
   schar c;
   cc->read((char*)&c, sizeof(schar));
   return c;
}

//----------------------------
                              //MAC style
inline word _mm_read_M_UWORD(C_cache *cc){
   byte b = mmReadByte(cc);
   return (b<<8) | mmReadByte(cc);
}

//----------------------------

inline word mmReadWord(C_cache *cc){
   byte b = mmReadByte(cc);
   return b | (mmReadByte(cc)<<8);
}

//----------------------------

inline dword mmReadDword(C_cache *cc){
   word w = mmReadWord(cc);
   return w | (mmReadWord(cc)<<16);
}

//----------------------------

struct GHOLD{
   dword start, size;                  //sample index
   dword loop_start, loop_end;         //sample index
   dword frq;                          //current frequency
   LPDIRECTSOUNDBUFFER samp;
   word flags;                     /* 16/8 bits looping/one-shot */
   short handle;              //identifies the sample
   short playing_handle;
   byte vol, vol_trig;                       /* current volume */
   byte pan, pan_trig;                       /* current panning position */
   byte frq_trig;
   byte kick;                      /* =1 -> sample has to be restarted */
   byte is_dupl;
   byte real_looping;
};

//----------------------------

class C_unimod{
public:
   byte num_chnannels;
   word num_positions;
   word restart_position;
   word num_patterns;
   word num_tracks;
   word num_instruments;
   byte initspeed;
   byte inittempo;
   byte positions[256];
   byte panning[32];
   byte flags;
   C_str song_name;
   //C_str mod_type;      //string type of module
   S_instrument *instruments; //all samples
   word *patterns;
   word *pattrows;
   vector<byte> *tracks;

   void Init(){
      memset(this, 0, sizeof(C_unimod));
                              //init panning array
      for(int i=0; i<32; i++)
         panning[i] = ((i+1)&2) ? 255 : 0;

   }
   void UnInit(){
      delete[] patterns; patterns = NULL;
      delete[] pattrows; pattrows = NULL;
      delete[] tracks; tracks = NULL;
      delete[] instruments; instruments = NULL;
   }

   dword GetPeriod(byte note,word c2spd){
      if(flags&UF_XMPERIODS){
         return (flags&UF_LINEAR) ? getlinearperiod(note, c2spd) :
            getlogperiod(note,c2spd);
      }
      return getoldperiod(note, c2spd);
   }

   bool AllocPatterns();
};

//----------------------------

class ISND_music{
   dword ref;

   CRITICAL_SECTION timer_lock; //semaphore used for updating playback
   C_smart_ptr<ISND_driver> drv;

   C_unimod unimod;

   bool is_playing;
   bool is_open;

                              //run-time:
   dword curr_restart_position;
   dword times_to_loop;
   dword tick_counter;
   dword pattern_break;       //position where to start a new pattern
   byte pattern_delay;
   byte pattern_delay2;
   dword num_rows_in_pat;
                              //flag to indicate a position jump is needed...
                              // changed since 1.00: now also indicates the
                              // direction the position has to jump to:
                              //    0: Don't do anything
                              //    1: Jump back 1 position
                              //    2: Restart on current position
                              //    3: Jump forward 1 position
   byte position_jump;

   S_channel mp_audio[32];    //max 32 channels
   dword current_row;
   int song_position;
   dword curr_speed;
   //bool extended_speed_flag;
   //bool mp_panning;           //panning flag, default enabled
   bool loop;
   //byte mp_volume;            //song volume (0-100) (or user volume)
   float volume;
   int global_volume;         //in range 0-64
   byte global_slide;
   byte beats_per_minute;
   bool b_16bit;

                              //DSound stuff:
   GHOLD ghld[32];
   byte DS_BPM;
   LPDIRECTSOUNDBUFFER lpSamp[MAXSAMPLEHANDLES][2];
   byte is_sam_playing[MAXSAMPLEHANDLES][2];

   short voltab[65];
   short pantab[256];
                                          //timer stuff
   UINT wTimerID;
   UINT wTimerRes;
   HANDLE h_timer;        //timer threat handle
   HANDLE h_timer_event;  //timer signalling object


   bool LoadHeader(C_cache *cc);
   bool LoadSamples(C_cache *cc);

                              //voices:
   inline void VoiceSetVolume(byte voice, byte vol){

      if(ghld[voice].vol != vol){
         ghld[voice].vol = vol;
         ghld[voice].vol_trig = 1;
      }
   }

   inline void VoiceSetFrequency(byte voice, dword frq){

      if(ghld[voice].frq!=frq){
         ghld[voice].frq=frq;
         ghld[voice].frq_trig=1;
      }
   }

   inline void VoiceSetPanning(byte voice, byte pan){

      if(ghld[voice].pan!=pan){
         ghld[voice].pan=pan;
         ghld[voice].pan_trig=1;
      }
   }

   void VoicePlay(byte voice, short handle, dword start, dword size,
      dword loop_start, dword loop_end, word flags){
                                          //clip
      if(start>=size) return;
      if(flags&SF_LOOP && loop_end>size) loop_end=size;

      ghld[voice].flags=flags;
      ghld[voice].handle=handle;
      ghld[voice].start=start;
      ghld[voice].size=size;
      ghld[voice].loop_start=loop_start;
      ghld[voice].loop_end=loop_end;
      ghld[voice].kick=1;
   }

   bool LoadSamp(LPDIRECTSOUNDBUFFER *lplpDsb, schar *samp, UINT length);
   short SampleLoad(C_cache *cc, dword length, dword loopstart, dword loopend, word flags);
   void SampleUnload(short handle);
   void DSSetBPM(word bpm);
   dword myTimerSet(float msInterval);
   void myTimerDone();
   static dword WINAPI TimerThreatProc(void *context);
//----------------------------
// Setup periodic async timer.
   void TimerInit();

//----------------------------
// Update DSound playback - internally calls Tick.
   void DS_Update();

//----------------------------
// Update sound track.
   void Tick();

   void DoEEffects(S_channel *a, int channel_index, byte dat);
   void PlayEffects(S_channel *a, int channel_index);
   void DoVibrato(S_channel *a);
   void DoTremolo(S_channel *a);
   void DoVolSlide(S_channel *a, byte dat);
   void DoS3MVolSlide(S_channel *a, byte inf);
   void DoXMVolSlide(S_channel *a, byte inf);
   void DoXMGlobalSlide(byte inf);
   void DoXMPanSlide(S_channel *a, byte inf);
   void DoS3MSlideDn(S_channel *a, byte inf);
   void DoS3MSlideUp(S_channel *a, byte inf);
   void DoS3MTremor(S_channel *a, byte inf);
   void DoS3MRetrig(S_channel *a, byte inf);
   void DoS3MSpeed(S_channel *a, byte speed);
   void DoS3MTempo(S_channel *a, byte tempo);
   void DoToneSlide(S_channel *a);
   void DoPTEffect0(S_channel *a, byte dat);
   void PlayNote(S_channel *a, int channel_index);

public:
   ISND_music(PISND_driver);
   ~ISND_music();

public:
   virtual dword AddRef(){ return ++ref; }
   virtual dword Release(){ if(--ref) return ref; delete this; return 0; }

   virtual bool Open(const char *name);
   virtual bool Close();
   virtual bool Play();
   virtual void Stop();
   virtual bool IsOpen() const{ return is_open; }
   virtual bool SetPlayTrack(int track);
   virtual void SetLoop(bool b){ loop = b; }
   virtual void SetVolume(float f){ volume = Max(0.0f, Min(1.0f, f)); }

   virtual bool IsLoop() const{ return loop; }
   virtual float GetVolume() const{ return volume; }
   virtual int GetNumChannels() const{ return IsOpen() ? unimod.num_chnannels : 0; }
   virtual int GetNumPositions() const{ return IsOpen() ? unimod.num_positions : 0; }
   virtual int GetNumPatterns() const{ return IsOpen() ? unimod.num_patterns : 0; }
   virtual int GetNumTracks() const{ return IsOpen() ? unimod.num_tracks : 0; }
   virtual int GetNumInstruments(){ return IsOpen() ? unimod.num_instruments : 0; }
   virtual const char *GetSongName() const{ return IsOpen() ? unimod.song_name : NULL; }
   virtual int GetPlayTrack() const;
};

//----------------------------
#endif