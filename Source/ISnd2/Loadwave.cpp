#include "all.h"
#include "loader.h"
#include <rules.h>

//----------------------------

static class C_wave_loader: public C_sound_loader{
   struct S_load_process{
      dword mmio_data_offs;
      HMMIO mmio_handle;
   };

                              //custom mmio loader for wave files through dta? functions
   static LRESULT CALLBACK mmioProc(LPSTR lpmmioinfo, UINT uMsg, LONG lParam1, LONG lParam2){

      LPMMIOINFO mmi = (LPMMIOINFO)lpmmioinfo;

      switch(uMsg){
      case MMIOM_OPEN:
         {
            PC_dta_stream dta = DtaCreateStream((char*)lParam1);
            if(!dta)
               return MMIOERR_CANNOTOPEN;
            mmi->adwInfo[0] = (dword)dta;
         }
         break;
      case MMIOM_CLOSE:
         {
            PC_dta_stream dta = (PC_dta_stream)mmi->adwInfo[0];
            if(!dta)
               return MMIOERR_CANNOTCLOSE;
            dta->Release();
            mmi->adwInfo[0] = 0;
         }
         break;
      case MMIOM_READ:
         {
            PC_dta_stream dta = (PC_dta_stream)mmi->adwInfo[0];
            int i = dta->Read((void*)lParam1, lParam2);
            if(i!=-1)
               mmi->lDiskOffset += i;
            return i;
         }

      case MMIOM_WRITE:
      case MMIOM_WRITEFLUSH:
         return -1;

      case MMIOM_SEEK:
         {
            PC_dta_stream dta = (PC_dta_stream)mmi->adwInfo[0];
            int i = dta->Seek(lParam1, lParam2);
            if(i!=-1)
               mmi->lDiskOffset = i;
            return i;
         }
      }
      return 0;
   }
public:
   C_wave_loader(){
      RegisterSoundLoader(this);
   }

//----------------------------

   virtual dword Open(PC_dta_stream dta, const char *file_name, void *header, dword hdr_size, S_wave_format *wf){

                              //if header is not RIFF, don't bother
      if(hdr_size<4)
         return 0;
      if((*(dword*)header) != 0x46464952)
         return 0;

                              //open wave
      MMIOINFO mmi;
      memset(&mmi, 0, sizeof(mmi));
      mmi.pIOProc = mmioProc;
      HMMIO h = mmioOpen((char*)file_name, &mmi, MMIO_READ);
                              //open error
      if(!h)
         return 0;

      MMCKINFO mmckInfoParent;
      MMCKINFO mmckInfoSubchunk;
                              //find wave chunk
      mmckInfoParent.fccType = mmioFOURCC('W','A','V','E');
      if(mmioDescend(h, &mmckInfoParent, NULL, MMIO_FINDRIFF)){
         mmioClose(h, 0);        //no wave file, error
         return 0;
      }
                              //find fmt chunk
      mmckInfoSubchunk.ckid = mmioFOURCC('f','m','t',' ');
      if(mmioDescend(h, &mmckInfoSubchunk, &mmckInfoParent, MMIO_FINDCHUNK)){
         mmioClose(h, 0);        //wave error?
         return 0;
      }
                              //read wave format
      PCMWAVEFORMAT pcmwf;
      memset(&pcmwf, 0, sizeof(pcmwf));
      int i = mmioRead(h, (LPSTR)&pcmwf.wf, Min(mmckInfoSubchunk.cksize, (dword)sizeof(pcmwf.wf)));
                              //check header validity
      if(i==sizeof(pcmwf.wf));
      else
      if(pcmwf.wf.nChannels && pcmwf.wf.nBlockAlign && pcmwf.wBitsPerSample);
      else{
                              //wave error?
         mmioClose(h, 0);
         return 0;
      }
      mmioAscend(h, &mmckInfoSubchunk, 0);

                              //find data chunk
      mmckInfoSubchunk.ckid = mmioFOURCC('d','a','t','a');
      if(mmioDescend(h, &mmckInfoSubchunk, &mmckInfoParent, MMIO_FINDCHUNK)){
                              //wave error?
         mmioClose(h, 0);
         return 0;
      }
      if(!mmckInfoSubchunk.cksize){
                              //no sample data, error
         mmioClose(h, 0);
         return 0;
      }
      wf->num_channels = pcmwf.wf.nChannels;
      wf->size = mmckInfoSubchunk.cksize;
      wf->bytes_per_sample = pcmwf.wf.nBlockAlign;
      wf->samples_per_sec = pcmwf.wf.nSamplesPerSec;

                              //close provided handle
      dta->Release();

                              //return load process handle
      S_load_process *lp = new S_load_process;
      lp->mmio_data_offs = mmioSeek(h, 0, SEEK_CUR);;
      lp->mmio_handle = h;
      return (dword)lp;
   }

//----------------------------

   virtual void Close(dword handle){

      S_load_process *lp = (S_load_process*)handle;
      mmioClose(lp->mmio_handle, 0);
      delete lp;
   }

//----------------------------

   virtual int Read(dword handle, void *mem, dword size){

      S_load_process *lp = (S_load_process*)handle;
      return mmioRead(lp->mmio_handle, (char*)mem, size);
   }

//----------------------------

   virtual int Seek(dword handle, dword pos){

      S_load_process *lp = (S_load_process*)handle;
      return mmioSeek(lp->mmio_handle, lp->mmio_data_offs + pos, SEEK_SET) - lp->mmio_data_offs;
   }

} wave_loader;

//----------------------------
