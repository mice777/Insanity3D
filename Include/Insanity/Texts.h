#ifndef __LOC_TXT_H
#define __LOC_TXT_H

#include <C_str.hpp>

/************************************************************
   Copyright (c) Lonely Cat Games  All rights reserved.

   Class:      C_all_texts
   Written by: Michal Bacik
   Date:       10. 12. 1999
   Abstract:   Localized text reader used to read and keep
               game texts in various languages, including
               unicode texts.
   Note:       - single-byte and double-byte text files may be combined
               - each text is on a single line
************************************************************/

//----------------------------

class C_all_texts{
   class C_all_texts_rep *rep;   //hidden implementation
   bool Open(const char*, bool); //internal 
public:
   C_all_texts();
   C_all_texts(const C_all_texts &at): rep(NULL){ operator =(at); }
   C_all_texts &operator =(const C_all_texts &at);
   ~C_all_texts(){
      Close();
   }

//----------------------------
// Add another texts (from specified file) into current database stored in this class.
// If operation failed (file not found), the returned value is false.
   bool AddFile(const char *file_name);
   bool AddFile(class C_cache&);

//----------------------------
// Close - clear contents of the database.
   void Close();

//----------------------------
// Overloaded operator - access to particular text,
//    if requested text is not defined, the returned string
//    is "error: text #id not defined",
//    where id is substituted by actual number
   const C_str &operator [](int id) const;
   const C_str &operator [](const C_str &id) const;

//----------------------------
// Check if specified text is in the database.
   bool IsDefined(int id) const;
   bool IsDefined(const C_str &id) const;

//----------------------------
// Check if specified text is in wide-char format.
   bool IsWide(int id) const;
   bool IsWide(const C_str &id) const;

//----------------------------
// Delete all entries in the range.
   //void DelTexts(int base_id, int num_ids);

//----------------------------
// Debugging support - enumerating all texts.
   void EnumTexts(void(*)(const C_str &id, const C_str &text, void *context), void *context);
};

//----------------------------

#endif