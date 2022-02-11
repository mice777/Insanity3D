#include <rules.h>

typedef byte t_security_code[16];

//----------------------------

void GetMachineCode(t_security_code out_code);

void EncriptDate(t_security_code &out_code, int year, int month, int day, t_security_code machine_code);

                              //in:
                              //    encrypted code
                              //    date to test against
                              //out:
                              //    code validity
                              //    # of days after given y/m/d (0=no or unlocker)
                              //    encripted y/m/d
bool DecriptDate(const t_security_code in_code, int &year, int &month,
   int &day, int *days_left=NULL, const t_security_code *machine_code = NULL);

                              //work on current date
bool GetDaysLeft(const t_security_code in_code, int *days_over1, const t_security_code *machine_code = NULL);

                              //convert date into # of days from 1.1.0000
int DateToDays(int year, int month, int day);

                              //write code to registry
void WriteCodeToRegistry(const t_security_code code, const char *root, const char *val);
bool ReadCodeFromRegistry(t_security_code code, const char *root, const char *val);


//----------------------------
// Complete support for checking the trial code.
// - read code from registry
// - get machine code
// - if not present, ask user to generate code
bool CheckTrialCode(void *hinstance, void *hwnd, const char *reg_root, const char *reg_val);

//----------------------------
