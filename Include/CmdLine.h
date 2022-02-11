#ifndef __CMDLINE_H
#define __CMDLINE_H

#include <rules.h>
#include <C_str.hpp>

//----------------------------

#ifdef _DEBUG
#pragma comment(lib, "insanity_d")
#else
#pragma comment(lib, "insanity")
#endif

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Command-line base class, used to scan and fill-in command parameters.
// It is capable to scan parameters by name, and when particular parameter is found,
// it performs an action - read and fill in a value, and/or call user function.
//
// The command scan is made either on all significant character, or up to n first characters in each command
//    (selected by constructor parameter).
//
// Inherited class must provide pointer to scanned commands with appropriate actions
//----------------------------

class C_command_line_base{
   int num_match_chars;
public:

// Type of operation performed on data when particular command is found in command-line.
   enum E_COMMAND_OPERATION{
      CMD_NOP,                //no data operation
      CMD_BOOL,               //set boolean
      CMD_BOOL_CLEAR,         //clear boolean
      CMD_INT,                //read int
      CMD_INT_X2,             //read 2x int
      CMD_STRING,             //read string (C_str)
                  //additional flags:
      CMD_GLOBAL_PTR = 0x10000,  //the offset is absolute pointer into memory, rather than relative offset
   };

//----------------------------
// Callback called when particular command is found in command-line. Input parameter is current pointer to
// command-line string, which may be modified by function (if function want to read or skip parameters).
// If returned value is true, scanning continues, otherwise further scanning is stopped
// and returned value from Scan is false.
   typedef bool (C_command_line_base::*t_Callback)(const char **cmd_line);

// Structure for declaration of single command, and required action (from which number and types of parameters
// are determined).
   struct S_scan_data{
      const char *command_name;  //name of command (scanned in command-line), max size is 255 chars
      dword operation;        //data operation (E_COMMAND_OPERATION) performed when command is found
      int data_offset;        //data offset to maniputale (relative to this pointer), ignored if CMD_NOP
      t_Callback Callback;    //callback function, may be NULL
   };

protected:
//----------------------------
// Read particular parameter from the command-line and advance the pointer.
   bool ReadInt(const char **cl, int &ret);
   bool ReadString(const char **cl, C_str &ret);

public:
   C_command_line_base(int _num_match_chars = 0):
      num_match_chars(_num_match_chars)
   {}

//----------------------------
// Get pointer to command definition table, terminated by item with <null> command string.
// Implemented by inherited class.
   virtual const S_scan_data *GetScanData() const = 0;

//----------------------------
// Function called when an error occurs. This may be overriden by inherited class to override default behavior.
   virtual void DisplayError(const char *err_msg, const char *app_name);

//----------------------------
// Scan command-line, fill in parameters. If successfully scanned, the returned value is true.
// If an error occurs, a warning message is written and returned value is false.
   bool Scan(const char *cmdline, const char *app_name);
};

//----------------------------

#endif
