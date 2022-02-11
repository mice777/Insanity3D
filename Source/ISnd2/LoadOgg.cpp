#include "all.h"
#include "loader.h"

extern"C"{
#include "vorbis\vorbisfile.h"
}

#ifdef _DEBUG
#pragma comment(lib, "ogg_d.lib")
#pragma comment(lib, "vorbis_d.lib")
#else
#pragma comment(lib, "ogg.lib")
#pragma comment(lib, "vorbis.lib")
#endif

//----------------------------

const int BITDEPTH = 16;      //bitdepth of data

//----------------------------

static class C_ogg_loader: public C_sound_loader{

   static size_t read_func(void *ptr, size_t size, size_t nmemb, void *datasource){
      PC_dta_stream dta = (PC_dta_stream)datasource;
      return dta->Read(ptr, size*nmemb);
   }

   static int seek_func(void *datasource, __int64 offset, int whence){
      PC_dta_stream dta = (PC_dta_stream)datasource;
      return dta->Seek((int)offset, whence);
   }

   static int close_func(void *datasource){
      PC_dta_stream dta = (PC_dta_stream)datasource;
      dta->Release();
      return 0;
      //return dtaClose((int)datasource) ? 0 : -1;
   }

   static long tell_func(void *datasource){
      PC_dta_stream dta = (PC_dta_stream)datasource;
      return dta->Seek(0, DTA_SEEK_CUR);
   }
public:
   C_ogg_loader()
   {
      RegisterSoundLoader(this);
   }

//----------------------------

   virtual dword Open(PC_dta_stream dta, const char *file_name, void *header, dword hdr_size, S_wave_format *wf){

                              //if header is not RIFF, don't bother
      if(hdr_size<4)
         return 0;
      if((*(dword*)header) != 0x5367674f)
         return 0;

      OggVorbis_File *vf = new OggVorbis_File;
                              //try to open the file
      ov_callbacks ovc = { read_func, seek_func, close_func, tell_func };
      int vret = ov_open_callbacks(dta, vf, (char*)header, hdr_size, ovc);
      if(vret != 0){
         delete vf;
         return 0;
      }
                              //init format
      vorbis_info *vi = ov_info(vf, -1);
      assert(vi);
      wf->num_channels = vi->channels;
      wf->bytes_per_sample = BITDEPTH/8 * wf->num_channels;
      wf->samples_per_sec = vi->rate;
      wf->size = (int)ov_pcm_total(vf, -1) * wf->bytes_per_sample;

      return (dword)vf;
   }

//----------------------------

   virtual void Close(dword handle){

      OggVorbis_File *vf = (OggVorbis_File*)handle;
      assert(vf);
      ov_clear(vf);
      delete vf;
   }

//----------------------------

   virtual int Read(dword handle, void *mem, dword size){

      OggVorbis_File *vf = (OggVorbis_File*)handle;
      dword read = 0;
                              //must read in multiple chunks (design of VorbisFile?)
      while(read < size){
         int current_section;
         dword to_read = size - read;
         int rd1 = ov_read(vf,
            ((char*)mem) + read,
            to_read,
            0,                //little/big endian
            BITDEPTH/8,       //byte-depth
            BITDEPTH==8 ? 0 : 1, //unsigned=0, signed=1
            &current_section);
         assert(rd1);
         if(rd1<0)
            return -1;
         read += rd1;
      }
      assert(read==size);
      return read;
   }

//----------------------------

   virtual int Seek(dword handle, dword pos){

      OggVorbis_File *vf = (OggVorbis_File*)handle;

                              //convert position back to samples
      dword sample = pos / (vf->vi->channels * 2);
      ov_pcm_seek(vf, sample);
      /*
      assert(pos==0);
      if(pos!=0)
         return -1;
      if(ov_pcm_tell(vf)!=0){
                              //re-open
         int h = (int)vf->datasource;
         vf->datasource = NULL;
         int ok;
         ok = ov_clear(vf);
         assert(!ok);

         dtaSeek(h, 0, DTA_SEEK_SET);
         ov_callbacks ovc = { read_func, seek_func, close_func, tell_func };
         ok = ov_open_callbacks((void*)h, vf, NULL, 0, ovc);
         assert(!ok);
      }
      */
      return pos;
   }

} ogg_loader;

//----------------------------
