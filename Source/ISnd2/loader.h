
//----------------------------
                              //pure virtual base class for loader plugin
class C_sound_loader{
public:
//----------------------------
// Open file, initialize loader.
// Parameters:
//    dta - data stream object
//       If the method succeeds, the loader may perform any operations on the handle,
//       and it must call dtaClose when it no longer needs this, but at latest when
//       Close method is called.
//       If this method fails, the loader may perform read or seek operation on file,
//       but after return, the position of file must be the same as before the call.
//    file_name - Name of file to be opened. The loader may use either file_handle or
//       file_name to read the file. In most cases this parameter is not used because
//       file_handle is an opened handle to the file.
//    header - Pointer to memory where the beginning of file is already read
//       If the loader has enough information from this to determine if it is
//       known file format, it doesn't need to use file_handle. THe header is
//       provided so that loader may fast determine if it will process file or
//       not, without accessing file by file_handle.
//    hdr_size - Number of data in buffer pointed by file_handle
//    wf - Pointer to S_wave_format structure which will contain format of data
//       in file if the call succeeds.
// Return value:
//    If loader recognizes file, the return value is a handle identifying
//    load process for specified file.
//    If loader can't racognize file, the return value is 0. In this case
//    it cannot modify position of file identified by file_handle.
   virtual dword Open(PC_dta_stream dta, const char *file_name, void *header, dword hdr_size, LPS_wave_format wf) = 0;

//----------------------------
// Uninitialize load process.
// Parameters:
//    handle - Handle to load process returned by Open method
   virtual void Close(dword handle) = 0;

   
//----------------------------
// Read data from file.
// Parameters:
//    handle - Handle to load process returned by Open method
//    mem - Pointer to memory where the data are to be read
//    size - Number of bytes to read
// Return value:
//    The return value is number of bytes successfully read into the buffer.
   virtual int Read(dword handle, void *mem, dword size) = 0;

//----------------------------
// Set position to data offset within file.
// Parameters:
//    handle - Handle to load process returned by Open method.
//    pos - New read position of file, in bytes.
// Return value:
//    The return value is new position of read buffer.
   virtual int Seek(dword handle, dword pos) = 0;
};

typedef C_sound_loader *LPC_sound_loader;

//----------------------------

void __cdecl RegisterSoundLoader(LPC_sound_loader);
