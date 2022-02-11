#ifndef __INSANITY_SC_H_
#define __INSANITY_SC_H_
//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Simple source-control functions.
// Utilizing installed 3rd party SC provider
//----------------------------

#include <C_str.hpp>

//----------------------------
// Get path to source control program, and current database.
bool SCGetPath(C_str &vss_exe, C_str &vss_database);

//----------------------------
// Get file.
bool SCGetFile(const char *project, const char *filename);

//----------------------------
// Check out file.
bool SCCheckOutFile(const char *project, const char *filename);

//----------------------------
// Check in file.
bool SCCheckInFile(const char *project, const char *filename, bool keep_checked_out);

//----------------------------

#endif

