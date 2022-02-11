#ifndef __TABBLER2_H
#define __TABBLER2_H

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
//
// Tabler library header file.
//----------------------------

#include <rules.h>

#pragma comment(lib,"itabler2")

#define TABAPI
#define TABMETHOD_(type,method) virtual type TABAPI method

//----------------------------

                              //table element types
enum E_TABLE_ELEMENT_TYPE{
   TE_NULL,                   //must be last member of template
                              //basic types
   TE_BOOL,
   TE_INT,
   TE_FLOAT,
   TE_ENUM,
   TE_STRING,
   TE_COLOR_RGB,              //byte[3]   (0 - 255)
   TE_COLOR_VECTOR,           //float[3]  (0.0 - 1.0)
                              //special types (template-only)
   TE_BRANCH,
   TE_ARRAY,

   TE_LAST
};

typedef class C_table *PC_table;
typedef const C_table *CPC_table;

                              //editor callback messages
enum{
   TCM_MODIFY = 1,            //value modified (int item_index, int array_index)
   TCM_IMPORT = 2,            //table was imported
   TCM_CLOSE  = 3,            //dialog is closing (LPRECT pos_size)
//   TCM_OPEN   = 4,            //dialog is opening
//   TCM_FOCUS  = 5,            //focus changed ((short old_index)<<16 | short new_index)
   TCM_GET_ENUM_STRING = 6,   //get enum string (int item_index, C_str *ret) - called if enum string is NULL in template
};

                              //invalid values - those for which table doesn't display values
#define TAB_INV_INT 0x80000000
#define TAB_INV_FLOAT 1e+38f
#define TAB_INV_BOOL 2
#define TAB_INV_STRING "\1"
#define TAB_INV_ENUM 0xff

                              //editor callback
typedef void (TABAPI *table_callback)(PC_table, dword msg, dword cb_user, dword prm2, dword prm3);
                              //table editor flags
#define TABEDIT_CENTER        1
#define TABEDIT_EXACTHEIGHT   2
//#define TABEDIT_MANUALUPDATE  4
#define TABEDIT_MODELESS      8
//#define TABEDIT_EDITDEFAULTS  0x10
#define TABEDIT_IMPORT        0x20
#define TABEDIT_EXPORT        0x40
//#define TABEDIT_INFO          0x80
#define TABEDIT_HIDDEN        0x100
#define TABEDIT_CONTROL       0x200

//----------------------------
                              //table interface

#define TABOPEN_TEMPLATE   1  //create empty table from template
#define TABOPEN_FILENAME   2  //load from file
#define TABOPEN_FILEHANDLE 4  //load from open file
#define TABOPEN_DUPLICATE  8  //duplicate given table

#define TABOPEN_UPDATE     0x10000  //only fill-in matching mebers


#ifndef TABBLER_FULL

class C_table{
public:
   TABMETHOD_(dword,AddRef)() = 0;
   TABMETHOD_(dword,Release)() = 0;

   TABMETHOD_(bool,Load)(const void *src, dword flags) = 0;
   TABMETHOD_(bool,Save)(const void *dst, dword flags) const = 0;
   TABMETHOD_(bool,IsLoaded)() const = 0;

   TABMETHOD_(dword,NumItems)() const = 0;
   TABMETHOD_(dword,ArrayLen)(dword index) const = 0;
   TABMETHOD_(dword,StringSize)(dword index) const = 0;
   TABMETHOD_(dword,SizeOf)(dword index) const = 0;

//----------------------------
// Return address of generic item on specified index. If index is out of range,
// or no such item exists, the returned value is NULL.
   TABMETHOD_(void*,Item)(dword index) = 0;

//----------------------------
// Get various items - this is basically reference to generic item.
   TABMETHOD_(bool&,ItemB)(dword index) = 0;
   TABMETHOD_(int&,ItemI)(dword index) = 0;
   TABMETHOD_(float&,ItemF)(dword index) = 0;
   TABMETHOD_(byte&,ItemE)(dword index) = 0;
   TABMETHOD_(char*,ItemS)(dword index) = 0;
   TABMETHOD_(struct S_vector&,ItemV)(dword index) = 0;

   TABMETHOD_(bool,GetItemB)(dword index) const = 0;
   TABMETHOD_(int,GetItemI)(dword index) const = 0;
   TABMETHOD_(float,GetItemF)(dword index) const = 0;
   TABMETHOD_(byte,GetItemE)(dword index) const = 0;
   TABMETHOD_(const char*,GetItemS)(dword index) const = 0;
   TABMETHOD_(const struct S_vector&,GetItemV)(dword index) const = 0;
//----------------------------
// Get various items from arrays.
   TABMETHOD_(bool&,ItemB)(dword index, dword ai) = 0;
   TABMETHOD_(int&,ItemI)(dword index, dword ai) = 0;
   TABMETHOD_(float&,ItemF)(dword index, dword ai) = 0;
   TABMETHOD_(byte&,ItemE)(dword index, dword ai) = 0;
   TABMETHOD_(char*,ItemS)(dword index, dword ai) = 0;
   TABMETHOD_(struct S_vector&,ItemV)(dword index, dword ai) = 0;

   TABMETHOD_(bool,GetItemB)(dword index, dword ai) const = 0;
   TABMETHOD_(int,GetItemI)(dword index, dword ai) const = 0;
   TABMETHOD_(float,GetItemF)(dword index, dword ai) const = 0;
   TABMETHOD_(byte,GetItemE)(dword index, dword ai) const = 0;
   TABMETHOD_(const char*,GetItemS)(dword index, dword ai) const = 0;
   TABMETHOD_(const struct S_vector&,GetItemV)(dword index, dword ai) const = 0;

//----------------------------
// Get type of particular element (TE_NULL if invalid index specified).
   TABMETHOD_(E_TABLE_ELEMENT_TYPE,GetItemType)(dword index) const = 0;

                              //table editor - returns HWND
   TABMETHOD_(void*,Edit)(const class C_table_template*, void *hwnd_parent,
      table_callback cb_proc, dword cb_user, dword create_flags = 0,
      int *pos_size = NULL) = 0;

   TABMETHOD_(bool,IsModified)() const = 0;
};

#endif

//----------------------------
                              //table template element
class C_table_element{
public:
   E_TABLE_ELEMENT_TYPE type;
   dword tab_index;
   const char *caption;
   union{
      int flt_min;
      int int_min;

      dword branch_depth;     //TE_BRANCH
      char *enum_list;        //TE_ENUM
      dword array_size;       //TE_ARRAY
      dword string_size;      //TE_STRING
   };
   union{
      int flt_max;
      int int_max;
                              //formatted text for TE_BRANCH
                              //format is: %[x] where x is index of sub-item
                              //this is automamically included into string in its proper format
      const char *branch_text;//TE_BRANCH
   };
   union{
                              //defaults:
      float flt_default;
      int int_default;
      char *string_default;
   };
   const char *help_string;   //string specifying help associated with element
};
typedef C_table_element *PC_table_element;
typedef const C_table_element *CPC_table_element;

//----------------------------

                              //table template declaration
#ifndef TABBLER_FULL
class C_table_template{
public:
   const char *caption;
   CPC_table_element te;
};
#endif

typedef class C_table_template *PC_table_template;
typedef const C_table_template *CPC_table_template;

//----------------------------
                              //table creation
extern"C"{
PC_table __declspec(dllexport) TABAPI CreateTable();
}

//----------------------------

#endif
