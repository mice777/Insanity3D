#ifndef __EDITCTRL_H
#define __EDITCTRL_H

#include <rules.h>

/**************************************************************************
* Copyright (c) Lonely Cat Games  All rights reserved.
*  File: editctrl.h
*  Content: HighWare text editor - include file
**************************************************************************/

#pragma comment (lib,"i_ectrl")


enum ECTRL_RESULT{            //return value:
   ECTRL_OK,                  
   ECTRL_MODIFIED,            //file modified
   ECTRLERR_MACROOPENFAIL = 0x80000000,   //cannot open specified macro file
   ECTRLERR_OPENFAILED,       //cannot open file
   ECTRLERR_CONFIGOPENFAIL,   //cannot open specified configuration file
   ECTRLERR_GENERIC,          //internal error
   ECTRLERR_SAVEFAILED,       //cannot save file
};

#define ECTRL_SUCCESS(n) ((ECTRL_RESULT)n>=0)
#define ECTRL_FAIL(n) ((ECTRL_RESULT)n<0)

#define ECTRL_OPEN_HIDDEN     1     //create invisible window
#define ECTRL_OPEN_READONLY   2     //can't modify contents of file

                              //editor interface:
class C_edit_control_pure{
public:
   typedef bool (t_callback)(const char *msg, void *context);

   virtual dword AddRef() = 0;
   virtual dword Release() = 0;

//----------------------------
// Init editor - load macros and configuration.
   virtual ECTRL_RESULT Init(const char *macro_file, const char *cfg_file) = 0;

//----------------------------
// Add word into list of highlit words. 'indx' is group index from 0 to 1 where 'res_word' is added.
   virtual ECTRL_RESULT AddHighlightWord(dword indx, const char *res_word) = 0;

   virtual void SetCallback(t_callback *cb_proc, void *context) = 0;

//----------------------------
// Open file, start editing - create window, return handle to this.
   virtual ECTRL_RESULT Open(const char *filename, void *hwnd_parent, dword flags, void **hwnd = NULL,
      const int pos_size[4] = NULL) = 0;
   virtual ECTRL_RESULT Open(class C_cache*, const char *title, void *hwnd_parent, dword flags, void **hwnd = NULL,
      const int pos_size[4] = NULL) = 0;

                              //Save file, if it has been modified.
                              // when prompt is true, message box appears.
   virtual ECTRL_RESULT Save(bool prompt = true) = 0;

                              //Close editor, save file unconditionally.
                              // When save_cfg is this, configuration is saved, 
                              // in case it has been changed.
   virtual ECTRL_RESULT Close(bool save_cfg = true) = 0;

                              //User 32-bit value
   virtual void SetUserData(dword) = 0;
   virtual dword GetUserData() const = 0;

                              //cursor position
   virtual void SetCurrPos(int line, int row) = 0;
   virtual void GetCurrPos(int &line, int &row) const = 0;

                              //direct access to text
   virtual int GetNumLines() const = 0;
   virtual bool GetTextLine(int line, char **buf, int *buf_len) const = 0;

   virtual bool GetPosSize(int pos_size[4]) const = 0;
};


#ifndef EDIT_CONTROL_INTERNAL
class C_edit_control: public C_edit_control_pure{};
#endif

typedef class C_edit_control *PC_edit_control;

                              //interface creation
extern"C"{
PC_edit_control __declspec(dllexport) CreateEditControl();
void __declspec(dllexport) EditControlDumpMemoryLeaks(bool);
}

#endif