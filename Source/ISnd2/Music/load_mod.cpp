#include "all.h"
#include "c_music.h"

//-------------------------------------

struct MSAMPINFO{       /* sample header as it appears in a module */
   char  samplename[22];
	word length;
	byte finetune;
	byte volume;
   word restart_position;
	word replen;
};


struct S_mod_header{
   char       songname[20];                /* the songname.. */
	MSAMPINFO  samples[31];                         /* all sampleinfo */
	byte      songlength;                          /* number of patterns used */
	byte      magic1;                                      /* should be 127 */
	byte      positions[128];                      /* which pattern to play at pos */
	byte      magic2[4];                           /* string "M.K." or "FLT4" or "FLT8" */

   S_mod_header():
      songlength(0),
      magic1(0)
   {
      memset(songname, 0, sizeof(songname));
      memset(samples, 0, sizeof(samples));
      memset(positions, 0, sizeof(positions));
      memset(magic2, 0, sizeof(magic2));
   }
};

#define MODULEHEADERSIZE 1084

struct MODNOTE{
	byte a, b, c, d;
};

//-------------------------------------

static const char protracker[]   = "Protracker";
static const char startracker[]  = "Startracker";
static const char fasttracker[]  = "Fasttracker";
static const char ins15tracker[] = "15-instrument";
static const char oktalyzer[]    = "Oktalyzer";
static const char taketracker[]  = "TakeTracker";

static const struct S_mod_type{
   const char *id;
	byte channels;
	const char *name;
} mod_types[] = {
	"M.K.", 4, protracker,    /* protracker 4 channel */
	"M!K!", 4, protracker,    /* protracker 4 channel */
	"FLT4", 4, startracker,   /* startracker 4 channel */
	"4CHN", 4, fasttracker,   /* fasttracker 4 channel */
	"6CHN", 6, fasttracker,   /* fasttracker 6 channel */
	"8CHN", 8, fasttracker,   /* fasttracker 8 channel */
	"CD81", 8, oktalyzer,     /* atari oktalyzer 8 channel */
	"OKTA", 8, oktalyzer,     /* atari oktalyzer 8 channel */
	"16CN", 16,taketracker,  /* taketracker 16 channel */
	"32CN", 32,taketracker,  /* taketracker 32 channel */
	"    ", 4, ins15tracker   /* 15-instrument 4 channel */
};

static const word finetune[16] = {
	8363,	8413,	8463,	8529,	8581,	8651,	8723,	8757,
	7895,	7941,	7985,	8046,	8107,	8169,	8232,	8280
};

static S_mod_header *mh;        /* raw as-is module header */

//-------------------------------------

static bool MOD_Test(C_cache *cc){

	int t;

   char id[4] = {0,0,0,0};

   cc->seekg(MODULEHEADERSIZE-4);
   cc->read(id, sizeof(id));
                                       //find out which ID string
   for(t=0; t<sizeof(mod_types)/sizeof(S_mod_type); t++)
      if(!memcmp(id, mod_types[t].id, 4))
         return true;
	return false;
}

//-------------------------------------

static bool MOD_Init(){

   mh = new S_mod_header;
   if(!mh)
      return false;
   memset(mh, 0, sizeof(mh));
   return true;
}

//-------------------------------------

static void MOD_Cleanup(){

   delete mh; mh = NULL;
}


/*
Old (amiga) noteinfo:

 _____byte 1_____   byte2_    _____byte 3_____   byte4_
/                \ /      \  /                \ /      \
0000          0000-00000000  0000          0000-00000000

Upper four    12 bits for    Lower four    Effect command.
bits of sam-  note period.   bits of sam-
ple number.                  ple number.
*/


static word npertab[60]={
                                       // -> Tuning 0
	1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,906,
	856,808,762,720,678,640,604,570,538,508,480,453,
	428,404,381,360,339,320,302,285,269,254,240,226,
	214,202,190,180,170,160,151,143,135,127,120,113,
	107,101,95,90,85,80,75,71,67,63,60,56
};

//-------------------------------------

static void ConvertNote(C_uni_recorder &rec, MODNOTE *n){

	byte instrument,effect,effdat,note;
	word period;

   //extract the various information from the 4 bytes that
   //make up a single note

	instrument=(n->a&0x10)|(n->c>>4);
	period=(((word)n->a&0xf)<<8)+n->b;
	effect=n->c&0xf;
	effdat=n->d;
                                       //Convert the period to a note number
	note=0;
	if(period!=0){
		for(note=0;note<60;note++){
			if(period>=npertab[note]) break;
		}
		note++;
		if(note==61) note=0;
	}

   if(instrument!=0)
      rec.WriteInstrument(instrument-1);

   if(note!=0)
      rec.WriteNote(note+23);
   rec.WritePTEffect(effect, effdat);
}

//-------------------------------------

static void ConvertTrack(const C_unimod &of, MODNOTE *n, vector<byte> &track){

	int t;

   C_uni_recorder rec;

	for(t=0;t<64;t++){
		ConvertNote(rec, n);
      rec.Newline();
		n += of.num_chnannels;
	}
   rec.GetBuffer(track);
}

//-------------------------------------

static bool ML_LoadPatterns(C_unimod &of, C_cache *cc){

   //Loads all patterns of a modfile and converts them into the 3 byte format.

   int t,s;

   if(!of.AllocPatterns())
      return 0;
   if(!(of.tracks = new vector<byte>[of.num_tracks]))
      return 0;
         //Allocate temporary buffer for loading and converting the patterns
   MODNOTE *patbuf = new MODNOTE[64U * of.num_chnannels];
   if(!patbuf)
      return false;
   memset(patbuf, 0, sizeof(MODNOTE)*64U * of.num_chnannels);

   int track_index = 0;

   for(t=0; t<of.num_patterns; t++){
                        //Load the pattern into the temp buffer and convert it
      for(s=0; s < (int)(64U*of.num_chnannels); s++){
         patbuf[s].a = mmReadByte(cc);
         patbuf[s].b = mmReadByte(cc);
         patbuf[s].c = mmReadByte(cc);
         patbuf[s].d = mmReadByte(cc);
		}
      for(s=0; s<of.num_chnannels; s++){
         assert(track_index < of.num_tracks);
         ConvertTrack(of, patbuf+s, of.tracks[track_index++]);
		}
	}
   delete[] patbuf;
	return true;
}

//----------------------------

static bool MOD_Load(C_unimod &of, C_cache *cc){

	S_instrument *d;             //new sampleinfo structure
	S_sample *q;
	MSAMPINFO *s;              //old module sampleinfo
                              //try to read module header
   //_mm_read_str((char *)mh->songname, 20, cc);
   cc->read(mh->songname, 20);
                                       //load 31 instruments
   for(int t=0; t<31; t++){
		s = &mh->samples[t];
      //_mm_read_str(s->samplename,22,cc);
      cc->read(s->samplename, 22);
      s->length   =_mm_read_M_UWORD(cc);
      s->finetune = mmReadByte(cc);
      s->volume   = mmReadByte(cc);
      s->restart_position =_mm_read_M_UWORD(cc);
      s->replen   =_mm_read_M_UWORD(cc);
	}

   mh->songlength = mmReadByte(cc);
   mh->magic1     = mmReadByte(cc);

   cc->read((char*)mh->positions, 128);
   cc->read((char*)mh->magic2, 4);
                                       //find out which ID string
   for(int modtype=0; modtype<sizeof(mod_types)/sizeof(S_mod_type); modtype++){
		if(!memcmp(mh->magic2, mod_types[modtype].id, 4))
         break;
	}
	if(modtype==sizeof(mod_types)/sizeof(S_mod_type)){
                                       //unknown modtype
		return 0;
	}
                                       //set module variables
	of.initspeed = 6;
	of.inittempo = 125;
   of.num_chnannels = mod_types[modtype].channels;
   //of.mod_type = mod_types[modtype].name;
   of.song_name = mh->songname;
   of.num_positions = mh->songlength;
   memcpy(of.positions, mh->positions, 128);
                                       //Count the number of patterns
	of.num_patterns = 0;

   for(t=0; t<128; t++){             /* <-- BUGFIX... have to check ALL positions */
		if(of.positions[t] > of.num_patterns){
			of.num_patterns = of.positions[t];
		}
	}
   ++of.num_patterns;
	of.num_tracks = of.num_patterns * of.num_chnannels;
                                    //Finally, init the sampleinfo structures
	of.num_instruments = 31;
	//if(!AllocInstruments(of))
   if(!(of.instruments = new S_instrument[of.num_instruments]))
      return false;

   s = mh->samples;    /* init source pointer */
	d = of.instruments;  /* init dest pointer */

	for(t=0;t<of.num_instruments;t++){
		d->numsmp = 1;
		//if(!AllocSamples(d))
      if(d->numsmp && !(d->samples = new S_sample[d->numsmp]))
         return false;
		q = d->samples;
                                       //convert the samplename
      //d->name = s->samplename;
//init the sampleinfo variables and convert the size pointers to longword format

      q->c2spd=finetune[s->finetune&0xf];
		q->volume=s->volume;
		q->loopstart = (ULONG)s->restart_position << 1;
		q->loopend=q->loopstart+((ULONG)s->replen<<1);
		q->length=(ULONG)s->length<<1;
		q->seekpos=0;

		q->flags=SF_SIGNED;
		if(s->replen>1)
         q->flags|=SF_LOOP;
                                       //fix replen if repend>length
		if(q->loopend>q->length)
         q->loopend=q->length;

      s++;
      d++;
	}
   return ML_LoadPatterns(of, cc);
}

//-------------------------------------

LOADER load_mod = {
	NULL,
   MOD_Init,
	MOD_Test,
	MOD_Load,
	MOD_Cleanup
};

//----------------------------
