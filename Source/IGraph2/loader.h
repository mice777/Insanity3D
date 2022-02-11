#ifndef __LOADER_H_
#define __LOADER_H_

//----------------------------

enum E_LOADER_RESULT{
   IMGLOAD_OK,
   IMGLOAD_BAD_FORMAT,
   IMGLOAD_READ_ERROR,
};
                              //pure virtual base class for loader plugin
class C_image_loader{
public:
   typedef C_except C_except_loader;
protected:

//----------------------------
// Description:
//    Allocation of memory for image. This method is already implemeted and loader
//    must call it to obtain memory for the image.
// Parameters:
//    size - Size of memory to allocate, in bytes.
// Return value:
//    The returned value is pointer to allocated memory, or NULL if there's not
//    enough memory.
// Note:
//    If image's format is true-color, the ordering is R-G-B.
//----------------------------
   static void *MemAlloc(dword size) throw(C_except_loader);

//----------------------------
// Description:
//    Freeing of memory allocated by MemAlloc. The loader may call this method
//    if it needs to free memory it allocated.
// Parameters:
//    mem - Pointer to memory previously allocated by MemAlloc.
// Return value:
//----------------------------
   static void MemFree(void *mem) throw();

public:
   /*
   class C_except_loader: public C_except{
   public:

   };

/*--------------------------------------------------------
   Description:
      Get information about image.
   Parameters:
      file_handle - Handle returned by dtaOpen. It may be used to access the file.
         The read position of handle is undetermined.
         The loader cannot close the handle.
      file_name - Name of file to be opened. The loader may use either file_handle or
         file_name to read the file. In most cases this parameter is not used because
         file_handle is an opened handle to the file.
      header - Pointer to memory where the beginning of file is already read.
         If the loader has enough information from this to determine if it is
         known file format, it doesn't need to use file_handle. THe header is
         provided so that loader may fast determine if it will process file or
         not, without accessing file by file_handle.
      hdr_size - Number of data in buffer pointed by file_handle.
      mem - Pointer to memory where image data are located after successful load.
         This memory must be allocated by MemAlloc method.
      pal - Pointer to memory where palette is located after successful load, in case
         the loaded image is paletized, or NULL otherwise.
         This memory must be allocated by MemAlloc method.
      size_x, size_y - Pointers to variables which will contain the resolution of
         loaded image when the function successfuly loads image.
      pf - Pointer to S_pixelformat structure which will contain valid pixel format
         of loaded image if the call succeeds.
   Return value:
      If loader recognizes file, the return value is true, and the loader must
      close the file_handle by calling dtaClose.
      If loader can't recognize file, the return value is false.
--------------------------------------------------------*/
   virtual E_LOADER_RESULT GetImageInfo(C_cache &ck, void *header, dword hdr_size,
      dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err) throw(C_except_loader) = 0;

/*--------------------------------------------------------
   Description:
      Load file.
   Parameters:
      file_handle - Handle returned by dtaOpen. It may be used to access the file.
         The read position of handle is undetermined.
      file_name - Name of file to be opened. The loader may use either file_handle or
         file_name to read the file. In most cases this parameter is not used because
         file_handle is an opened handle to the file.
      header - Pointer to memory where the beginning of file is already read.
         If the loader has enough information from this to determine if it is
         known file format, it doesn't need to use file_handle. THe header is
         provided so that loader may fast determine if it will process file or
         not, without accessing file by file_handle.
      hdr_size - Number of data in buffer pointed by file_handle.
      mem - Pointer to memory where image data are located after successful load.
         This memory must be allocated by MemAlloc method.
      pal - Pointer to memory where palette is located after successful load, in case
         the loaded image is paletized, or NULL otherwise.
         This memory must be allocated by MemAlloc method.
      size_x, size_y - Pointers to variables which will contain the resolution of
         loaded image when the function successfuly loads image.
      pf - Pointer to S_pixelformat structure which will contain valid pixel format
         of loaded image if the call succeeds.
   Return value:
      If loader recognizes file, the return value is true, and the loader must
      close the file_handle by calling dtaClose.
      If loader can't recognize file, the return value is false.
--------------------------------------------------------*/
   //virtual bool Load(int file_handle, const char *file_name, void *header, dword hdr_size,
   virtual E_LOADER_RESULT Load(C_cache &ck, void *header, dword hdr_size,
      void **mem, void **pal, dword *size_x, dword *size_y, S_pixelformat *pf, C_str &err) throw(C_except_loader) = 0;
};

typedef C_image_loader *PC_image_loader;

//----------------------------

#endif
