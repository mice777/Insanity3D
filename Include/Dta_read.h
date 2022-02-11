#ifndef __DTA_READ_H
#define __DTA_READ_H

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Data reader - read-only access to data packages.
// Contents:
//    - data initalizer class
//    - I/O functions simulating standard open, close, read and seek
//----------------------------

#include <rules.h>

#ifndef DTA_FULL

class C_dta_read{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;
};
#endif

typedef class C_dta_read *PC_dta_read;

//----------------------------

#define DTA_SEEK_SET 0
#define DTA_SEEK_CUR 1
#define DTA_SEEK_END 2

PC_dta_read dtaCreate(const char *fname, dword flags = 0);

//----------------------------
                              //class-based interface
class C_dta_stream{
public:
   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

//----------------------------
// Read from stream.
   virtual int Read(void *buffer, dword len) = 0;

//----------------------------
// Seek within the stream.
   virtual int Seek(int pos, int whence) = 0;

//----------------------------
// Get size of stream.
   virtual int GetSize() const = 0;

//----------------------------
// Get time of file.
   virtual bool GetTime(dword time[2]) const = 0;

//----------------------------
// Write to the file (valid for writable stream).
   virtual int Write(const void *buffer, dword len) = 0;

//----------------------------
// Get DTA pack where opened file resides.
   virtual PC_dta_read GetDtaPack() const = 0;
};

typedef C_dta_stream *PC_dta_stream;

//----------------------------

PC_dta_stream DtaCreateStream(const char *filename, bool for_write = false);

//----------------------------
#endif
