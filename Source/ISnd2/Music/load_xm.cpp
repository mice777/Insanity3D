#include "all.h"
#include "c_music.h"

/**************************************************************************
**************************************************************************/

#pragma pack(push,1)

struct XMHEADER{
   char id[17];               //ID text: "Extended module: "
   char songname[21];         //padded with zeroes and 0x1a at the end
   char trackername[20];
   word version;              //hi-byte major and low-byte minor
   dword headersize;
   word songlength;           //(in patten order table)
   word restart;
   word num_chnannels;
   word num_patterns;         //(max 256)
   word num_instruments;      //(max 128)
   word flags;                //bit 0: 0 = Amiga frequency table (see below) 1 = Linear frequency table
   word tempo;                //default tempo
   word bpm;                  //default BPM
   byte orders[256];          //pattern order table
};

struct XMINSTHEADER{
   dword size;                //instrument size
   char name[22];             //instrument name
   byte type;                 //instrument type (always 0)
   word numsmp;               //number of samples in instrument
   dword ssize;
};


struct XMPATCHHEADER{
   byte what[96];             //sample number for all notes
	byte volenv[48];       /* (byte) Points for volume envelope */
	byte panenv[48];       /* (byte) Points for panning envelope */
	byte volpts;           /* (byte) Number of volume points */
	byte panpts;           /* (byte) Number of panning points */
	byte volsus;           /* (byte) Volume sustain point */
	byte volbeg;           /* (byte) Volume loop start point */
	byte volend;           /* (byte) Volume loop end point */
	byte pansus;           /* (byte) Panning sustain point */
	byte panbeg;           /* (byte) Panning loop start point */
	byte panend;           /* (byte) Panning loop end point */
	byte volflg;           /* (byte) Volume type: bit 0: On; 1: Sustain; 2: Loop */
	byte panflg;           /* (byte) Panning type: bit 0: On; 1: Sustain; 2: Loop */
	byte vibflg;           /* (byte) Vibrato type */
	byte vibsweep;         /* (byte) Vibrato sweep */
	byte vibdepth;         /* (byte) Vibrato depth */
	byte vibrate;          /* (byte) Vibrato rate */
	word volfade;          /* (word) Volume fadeout */
	word reserved[11];     /* (word) Reserved */
};


struct XMWAVHEADER{
   dword length;
   dword loopstart;
   dword looplength;
   byte volume;
   schar finetune;            //(-128..+127)
   byte type;                 //bit 0-1: 0 = No loop, 1 = Forward loop, 
                              // 2 = Ping-pong loop; 
                              // 4: 16-bit sampledata
   byte panning;              //(0-255)
   schar relnote;             //relative note number
   byte reserved;
   char samplename[22];
};

struct XMPATHEADER{
   dword size;                //pattern header length
   byte packing;              //packing type (always 0)
   word numrows;              //number of rows in pattern (1..256)
   word packsize;             //packed patterndata size
};

struct XMNOTE{
	byte note, ins, vol, eff, dat;

   void Read(C_cache *cc){

	   memset(this, 0, sizeof(XMNOTE));
      byte cmp = mmReadByte(cc);

	   if(cmp&0x80){
         if(cmp&1)
            note = mmReadByte(cc);
         if(cmp&2)
            ins = mmReadByte(cc);
         if(cmp&4)
            vol = mmReadByte(cc);
         if(cmp&8)
            eff = mmReadByte(cc);
         if(cmp&16)
            dat = mmReadByte(cc);
	   }else{
		   note = cmp;
         ins = mmReadByte(cc);
         vol = mmReadByte(cc);
         eff = mmReadByte(cc);
         dat = mmReadByte(cc);
	   }
   }
};

#pragma pack(pop)

/**************************************************************************
**************************************************************************/

static bool XM_Test(C_cache *cc){

	char id[17];
   if(cc->read(id, 17) != 17)
      return false;
	return (!memcmp(id,"Extended Module: ", 17));
}

//----------------------------

static bool XM_Init(){
	return true;
}

//----------------------------

static void XM_Cleanup(void){
}

//----------------------------

static void XM_Convert(XMNOTE *xmtrack, word rows, vector<byte> &track){

   C_uni_recorder rec;
	for(int i=0; i<rows; i++){

		byte note = xmtrack->note;
		byte ins = xmtrack->ins;
		byte vol = xmtrack->vol;
		byte eff = xmtrack->eff;
		byte dat = xmtrack->dat;

      if(note!=0)
         rec.WriteNote(note-1);

      if(ins!=0)
         rec.WriteInstrument(ins-1);

		switch(vol>>4){

			case 0x6:					/* volslide down */
				if(vol&0xf){
               rec.WriteEffect(UNI_XMEFFECTA, vol&0xf);
				}
				break;

			case 0x7:					/* volslide up */
				if(vol&0xf){
               rec.WriteEffect(UNI_XMEFFECTA, vol<<4);
				}
				break;

			/* volume-row fine volume slide is compatible with protracker
			   EBx and EAx effects i.e. a zero nibble means DO NOT SLIDE, as
			   opposed to 'take the last sliding value'.
			*/

			case 0x8:						/* finevol down */
            rec.WritePTEffect(0xe, 0xb0 | (vol&0xf));
				break;

			case 0x9:                       /* finevol up */
            rec.WritePTEffect(0xe, 0xa0 | (vol&0xf));
				break;

			case 0xa:                       /* set vibrato speed */
            rec.WritePTEffect(0x4, vol<<4);
				break;

			case 0xb:                       /* vibrato */
            rec.WritePTEffect(0x4, vol&0xf);
				break;

			case 0xc:                       /* set panning */
            rec.WritePTEffect(0x8, vol<<4);
				break;

			case 0xd:                       /* panning slide left */
				/* only slide when data nibble not zero: */

				if(vol&0xf){
               rec.WriteEffect(UNI_XMEFFECTP, vol&0xf);
				}
				break;

			case 0xe:                       /* panning slide right */
				/* only slide when data nibble not zero: */

				if(vol&0xf){
               rec.WriteEffect(UNI_XMEFFECTP, vol<<4);
				}
				break;

			case 0xf:                       /* tone porta */
            rec.WritePTEffect(0x3, vol<<4);
				break;

			default:
				if(vol>=0x10 && vol<=0x50){
               rec.WritePTEffect(0xc, vol-0x10);
				}
		}

/*              if(eff>0xf) printf("Effect %d",eff); */

		switch(eff){

			case 'G'-55:                    /* G - set global volume */
				if(dat>64) dat=64;
            rec.WriteEffect(UNI_XMEFFECTG, dat);
				break;

			case 'H'-55:                    /* H - global volume slide */
            rec.WriteEffect(UNI_XMEFFECTH, dat);
				break;

			case 'K'-55:                    /* K - keyoff */
            rec.WriteNote(96);
				break;

			case 'L'-55:                    /* L - set envelope position */
				break;

			case 'P'-55:                    /* P - panning slide */
            rec.WriteEffect(UNI_XMEFFECTP, dat);
				break;

			case 'R'-55:                    /* R - multi retrig note */
            rec.WriteEffect(UNI_S3MEFFECTQ, dat);
				break;

			case 'T'-55:             		/* T - Tremor !! (== S3M effect I) */
            rec.WriteEffect(UNI_S3MEFFECTI, dat);
				break;

			case 'X'-55:
				if((dat>>4)==1){                /* X1 extra fine porta up */


				}
				else{                                   /* X2 extra fine porta down */

				}
				break;

			default:
				if(eff==0xa){
               rec.WriteEffect(UNI_XMEFFECTA, dat);
				}else
            if(eff<=0xf){
               rec.WritePTEffect(eff, dat);
            }
				break;
		}
      rec.Newline();
		xmtrack++;
	}
   rec.GetBuffer(track);
}

//----------------------------

static bool XM_Load(C_unimod &of, C_cache *cc){

   XMHEADER mh;
                              //read module header
   cc->read((char*)&mh, sizeof(mh));

	of.initspeed = mh.tempo;
	of.inittempo = mh.bpm;
	of.num_chnannels = (byte)mh.num_chnannels;
	of.num_patterns = mh.num_patterns;
	of.num_tracks = (word)of.num_patterns * of.num_chnannels;
   mh.songname[sizeof(mh.songname)-1] = 0;
	of.song_name = mh.songname;
	of.num_positions = mh.songlength;
	of.restart_position = mh.restart;
	of.num_instruments = mh.num_instruments;
	of.flags |= UF_XMPERIODS;
	if(mh.flags&1)
      of.flags |= UF_LINEAR;

	memcpy(of.positions, mh.orders, 256);

   of.tracks = new vector<byte>[of.num_tracks];
   if(!of.tracks)
      return false;
	if(!of.AllocPatterns())
      return false;

   int track_index = 0;
	for(int i=0; i<mh.num_patterns; i++){
		XMPATHEADER ph;
      cc->read((char*)&ph, sizeof(ph));

      of.pattrows[i] = ph.numrows;

      XMNOTE *xmpat = new XMNOTE[ph.numrows * of.num_chnannels];
      memset(xmpat, 0, sizeof(XMNOTE)*ph.numrows*of.num_chnannels);

		if(ph.packsize > 0){
			for(int j=0; j<ph.numrows; j++){
				for(int k=0; k<of.num_chnannels; k++){
					xmpat[(k*ph.numrows)+j].Read(cc);
				}
			}
		}

		for(int j=0; j<of.num_chnannels; j++){
         assert(track_index < of.num_tracks);
         XM_Convert(&xmpat[j*ph.numrows], ph.numrows, of.tracks[track_index++]);
		}
      delete[] xmpat;
	}

   of.instruments = new S_instrument[of.num_instruments];
   if(!of.instruments)
      return false;

	for(i=0; i<of.num_instruments; i++){
      S_instrument &inst = of.instruments[i];

		XMINSTHEADER ih;
                              //read instrument header
      cc->read((char*)&ih, sizeof(ih));

                              //save name of instrument
      //ih.name[sizeof(ih.name)-1] = 0;
      //inst.name = ih.name;
		inst.numsmp = ih.numsmp;

      if(inst.numsmp){
         inst.samples = new S_sample[inst.numsmp];
         if(!inst.samples)
            return false;
      }
		if(ih.numsmp > 0){
			XMPATCHHEADER pth;

         cc->read((char*)&pth, sizeof(pth));
			memcpy(inst.samplenumber, pth.what, sizeof(pth.what));
			inst.volfade = pth.volfade;

			inst.volflg = pth.volflg;
			inst.volsus = pth.volsus;
			inst.volbeg = pth.volbeg;
			inst.volend = pth.volend;
			inst.volpts = pth.volpts;

                              //copy and scale volume envelope
         memcpy(inst.volenv, pth.volenv, sizeof(pth.volenv));
			for(int p=0; p<12; p++){
				inst.volenv[p].val <<= 2;
			}

			inst.panflg = pth.panflg;
			inst.pansus = pth.pansus;
			inst.panbeg = pth.panbeg;
			inst.panend = pth.panend;
			inst.panpts = pth.panpts;

                              //scale and scale panning envelope
         memcpy(inst.panenv, pth.panenv, sizeof(pth.panenv));
			for(p=0; p<12; p++){
				inst.panenv[p].val <<= 2;
			}

			long next = 0;
			for(int u=0; u<inst.numsmp; u++){
				S_sample &sam = inst.samples[u];

            XMWAVHEADER wh;
            cc->read((char*)&wh, sizeof(wh));

                              //save name of sample
            //sam.name   =wh.samplename;
				sam.length = wh.length;
				sam.loopstart = wh.loopstart;
				sam.loopend = wh.loopstart + wh.looplength;
				sam.volume = wh.volume;
				sam.c2spd = wh.finetune + 128;
				sam.transpose = wh.relnote;
				sam.panning = wh.panning;
				sam.seekpos = next;

				if(wh.type&0x10){
					sam.length >>= 1;
					sam.loopstart >>= 1;
					sam.loopend >>= 1;
				}

				next += wh.length;

				sam.flags |= SF_OWNPAN;
				if(wh.type&0x3) sam.flags |= SF_LOOP;
				//if(wh.type&0x2) sam.flags |= SF_BIDI;

				if(wh.type&0x10) sam.flags |= SF_16BITS;
				sam.flags |= SF_DELTA;
				sam.flags |= SF_SIGNED;
			}

         int curr_pos = cc->tellg();

         for(u=0; u<inst.numsmp; u++)
            inst.samples[u].seekpos += curr_pos;

         cc->seekg(curr_pos + next);
		}
	}
   return true;
}

//----------------------------

LOADER load_xm = {
	NULL,
	XM_Init,
	XM_Test,
	XM_Load,
	XM_Cleanup
};


//----------------------------
