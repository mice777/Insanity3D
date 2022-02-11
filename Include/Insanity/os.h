#ifndef __INSANITY_OS_H_
#define __INSANITY_OS_H_
//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// OS-dependent functions.
//----------------------------

#include <C_str.hpp>
#include <C_buffer.h>

//----------------------------

struct WIN_GUID{
   byte data[16];
};

//----------------------------

struct S_date_time{
   word year;
   byte month;                //1 ... 12
   byte day;                  //1 ... 31
   byte hour;                 //0 ... 23
   byte minute;               //0 ... 59
   byte second;               //0 ... 59
   bool IsOlder(const S_date_time &dt) const{
      if(year<dt.year) return true;
      if(year>dt.year) return false;
      if(month<dt.month) return true;
      if(month>dt.month) return false;
      if(day<dt.day) return true;
      if(day>dt.day) return false;
      if(hour<dt.hour) return true;
      if(hour>dt.hour) return false;
      if(minute<dt.minute) return true;
      if(minute>dt.minute) return false;
      return (second<dt.second);
   }
   bool IsEqual(const S_date_time &dt) const{
      return (year==dt.year &&
         month==dt.month &&
         day==dt.day &&
         hour==dt.hour &&
         minute==dt.minute &&
         second==dt.second);
   }
};

//----------------------------
                              //messagebox button styles
enum MSGBOX_STYLE{
   MBOX_OK,                   //OK button
   MBOX_YESNO,                //Yes/No buttons
   MBOX_YESNOCANCEL,          //Yes/No/Cancel buttons
   MBOX_RETRYCANCEL,          //Retry/Cancel buttons
   MBOX_OKCANCEL,             //Ok/Cancel buttons
};

                              //message box return value
enum MSGBOX_RETURN{
   MBOX_RETURN_NO,            //No button pressed
   MBOX_RETURN_YES,           //Yes/Ok/Retry button pressed
   MBOX_RETURN_CANCEL,        //Cancel button pressed
};

//----------------------------
// Display message box, and wait until user clicks one of buttons.
// hwnd ... handle of parent window, may be NULL
// msg ... message to display
// title ... window title
// style ... button style
MSGBOX_RETURN OsMessageBox(void *hwnd, const char *msg, const char *title, MSGBOX_STYLE style);

//----------------------------
// Create specified directory.
bool OsCreateDirectory(const char *dir);

//----------------------------
// Delete specified directory.
bool OsDeleteDirectory(const char *dir);

//----------------------------
// Remove directory, return true if succed. Only empty directory is removed.
bool OsRemoveDirectory(const char *dir);

//----------------------------
// Create directory tree (ignoring file on top of path).
bool OsCreateDirectoryTree(const char *path);

//----------------------------
// Delete specified file
bool OsDeleteFile(const char *filename);

//----------------------------
// Get name of computer.
bool OsGetComputerName(C_str &str);

//----------------------------
// Move file from source to destination.
bool OsMoveFile(const char *src, const char *dst);

//----------------------------
// Destroy specified window.
void OsDestroyWindow(void *hwnd);

//----------------------------
// Show/hide specified window.
void OsShowWindow(void *hwnd, bool on);

//----------------------------
// Center specified window 'whnd' relative to another window 'hwnd_other'.
void OsCenterWindow(void *hwnd, void *hwnd_other);

//----------------------------
// Check if specified file exists and has read-only attribute set.
bool OsIsFileReadOnly(const char *filename);

//----------------------------
// Set or clear read-only attribute on specified file.
// The returned value is false if file doesn't exists, or attributes cannot be modified.
bool OsMakeFileReadOnly(const char *filename, bool rdonly);

//----------------------------
// Set or clear 'hidden' attribute on specified file.
// The returned value is false if file doesn't exists, or attributes cannot be modified.
bool OsMakeFileHidden(const char *filename, bool hidden);

//----------------------------
// Sleep for specified time.
void OsSleep(int time);

//----------------------------
// Run system shell - perform specified cmd (usually open the file with associated program).
bool OsShell(const char *cmd);

//----------------------------
// Check if provided path is on a CD-ROM drive.
bool OsIsCDROMDrive(const char *path);

//----------------------------
// Collect all matching names of files in specified directory.
void OsCollectFiles(const char *root_path, const char *wildmask, C_buffer<C_str> &ret, bool dirs = true, bool files = true);

//----------------------------
// Create unique filename from provided string. This means the name contains only characters allowable in filenames,
// and doesn't collide with existing file in provided directory.
// In this context, provided string is just a hint, and resulting name may be quite different from original.
// Upon return, 'str' contains new unique name.
void OsCreateUniqueFileName(const char *path, const char *ext, C_str &str);

//----------------------------
// Same as above but for directories.
void OsCreteUniqueDirName(const char *path, C_str &str);

//----------------------------
// Return true if directoty exist.
bool OsIsDirExist(const char *path);

//----------------------------
// Get date and time.
void OsGetCurrentTimeDate(struct S_date_time&);
bool OsGetFileTimeDate(const char *fname, struct S_date_time&);

//----------------------------
// Get version info about specified executable.
bool OsGetExecutableVersion(const char *fname, int &major, int &minor);

//----------------------------
// Create sytem font (HFONT). Returns handle to created font.
void *OsCreateFont(int size);

//----------------------------
// Destroy system font (HFONT).
void OsDestroyFont(void *handle);

//----------------------------
// Display context help window.
// Parameters:
//    text ... text to display
//    hwnd_parent ... handle to window which will be parent of help window
//    x, y ... position of upper left corner, relative to parent window
//    width ... width of help window, in pixels (pass zero to use default width)
bool OsDisplayHelpWindow(const char *text, void *hwnd_parent, int x, int y, int width = 0);

//----------------------------
//Display context of textual filename (using notepad or such editor).
bool OsDisplayFile(const char *filename);


//----------------------------

#endif

