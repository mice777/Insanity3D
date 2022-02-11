#ifndef __APP_INIT_H
#define __APP_INIT_H

//----------------------------
                              //application initialization structure
                              // this struct is inside app's DLL, and tells the main exe all about application
struct S_application_data{
   typedef bool (t_Init)(PI3D_driver);
   typedef class C_editor *(t_EditorCreate)();
   typedef void (t_Run)();
   typedef void (t_Close)();
   typedef class C_str (t_GetCrashInfo)();

   const char *app_name;      //application name
   const char *reg_base;      //registry directory base
   const char *crash_send_addr;//dir or e-mail address (if it contains '@') where crash reports are sent, may be NULL
   const char *dbase_name;    //name of I3D database file
   dword dbase_size;          //size of    ''
   class C_command_line *cmd_line;   //pointer to command-line scaner class

   t_Init *Init;              //initialization function, called after initializing system (may be NULL)
   //t_Run *Run;                //function running the application
   t_Close *Close;            //function called before application is shut down and app DLL is unloaded, may be NULL
   t_EditorCreate *EdCreate;  //function for creating editor, may be NULL, or return NULL
   t_GetCrashInfo *GetCrashInfo; //func called to obtain detailed info for crash report (mission, camera, state, ...)
};

//----------------------------
// Manage game run, given information from the structure.
// The returned value is application return code.
int __declspec(dllexport) GameRun(const S_application_data &app_data, const char *cmd_line);

//----------------------------

#endif
