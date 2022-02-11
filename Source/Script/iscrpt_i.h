
//----------------------------
#define PCP_VERSION  0x110    //pre-compiled scrips version (1.xx)

                              //internal compiler flags
#define ISLCOMP_SEARCH_INCL_PATHS   0x10000   //during opening the file

#define MRG_ZERO 1e-8f
#define MRG_ZERO_BITMASK 0x322BCC77

//----------------------------

                              //pre-compiled script header
#pragma pack(push, 1)
struct S_pcp_header{
   dword version;
   dword code_size, data_size;
   dword num_reloc_data, num_reloc_symbols;
   dword table_templ_size;
   dword num_dependencies;
};

                              //following the script is an array of this structure, which has variable length,
                              // depending of filename
struct S_pcp_dependency{
   //S_date_time file_time;
   __int64 file_time;
   char name[1];
};

#pragma pack(pop)

//----------------------------
                              //symbol (and relocation) info
enum E_SYMBOL_TYPE{
   SI_NULL,
   SI_LOCAL,                  //local type
   SI_EXTERNAL,               //external type
   SI_DATAOFFSET,             //offset to data (exported type)
   SI_CODEOFFSET,             //offset to code (exported type)
   SI_AUTOVAR,                //automatic variable
   SI_BREAK,
   SI_CASE,
   SI_DEFAULT,
   SI_RETURN,
   SI_CONTINUE,
};

                              //location of relocation (default is code)
#define SI_DATA      0x1      //data segment
#define SI_TEMPLATE  0x2      //template segment

//----------------------------

                              //relocation symbol info
#pragma pack(push,1)
struct S_symbol_info{
   union{
      char sym_name[27];
      dword data[2];
   };
   E_SYMBOL_TYPE type;
   dword flags;
   dword address;             //address - relative for file, real for externals

   S_symbol_info(){}
   S_symbol_info(E_SYMBOL_TYPE t, const char *n, dword a, dword f = 0):
      type(t),
      flags(f)
   {
      PutName(n);
      SetAddress(a);
      Validate();
   }

   void Validate(){
#ifdef _DEBUG
      switch(type){
      case SI_EXTERNAL:
      case SI_LOCAL:
         assert(sym_name[0]!=0);
         break;
      }
#endif
   }

   //inline void SetType(E_SYMBOL_TYPE t){ type = t; }
   //inline E_SYMBOL_TYPE GetType() const{ return type; }
   void PutName(const char *name){
      if(!name) sym_name[0] = 0;
      else{
         int i = Min((int)sizeof(sym_name)-1, (int)strlen(name));
         memcpy(sym_name, name, i);
         sym_name[i] = 0;
      }
   }
   inline const char *GetName() const{ return sym_name; }

   inline void SetData(dword d, int i){ data[i] = d; }
   inline dword GetData(int i) const{ return data[i]; }

   inline void SetAddress(dword a){ address = a; }
   inline dword GetAddress() const{ return address; }

   void Setup(E_SYMBOL_TYPE t, const char *n, dword a, dword f = 0){
      type = t;
      flags = f;
      PutName(n);
      SetAddress(a);
      Validate();
   }
   inline bool IsLast() const{ return (type==SI_NULL); }
};

//----------------------------
#pragma pack(pop)

//----------------------------

                              //location type
struct S_location{
   int l, r;
   void Home(){
      l = 1;
      r = 1;
   }
   void NewLine(){
      ++l;
      r = 1;
   }
};

//----------------------------

typedef map<dword, dword> t_file_debug_info;
typedef map<C_str, t_file_debug_info> t_debug_info;

//----------------------------

class C_script{
   dword ref;

   C_buffer<byte> code, data, table_templ;

                              //dependency info - filenames on which the script is dependent
   C_buffer<C_str> dependency;

   struct S_symbol_info *reloc_symbols;  //symbols contained in this program
   struct S_symbol_info *reloc_data;     //references to external symbols
   C_str script_name;         //name of script
   bool comp_ok;
   //S_date_time comp_time;     //compile time

                              //mapping of source-line into address
   t_debug_info debug_info;

   C_script();
   ~C_script();

public:
                              //error reporting function
   typedef void (ISLAPI T_report_err)(const char *msg, void *context, int l, int r, bool warn);
private:

//----------------------------
// Preprocess commandline options.
   ISL_RESULT ProcessCommandLine(const char *cmd_line, dword flags,
      class C_compile_process &compile_process, class C_scope &yyscope,
      C_vector<byte> &tmp_code, C_vector<byte> &tmp_data, C_vector<S_symbol_info> &tmp_reloc,
      C_vector<S_symbol_info> &tmp_symbols, C_vector<byte> &table_templ);

//----------------------------
// Compile single file.
   ISL_RESULT ProcessFile(const char *fname, dword flags,
      C_compile_process &compile_process, C_scope &yyscope,
      C_vector<byte> &tmp_code, C_vector<byte> &tmp_data, C_vector<S_symbol_info> &tmp_reloc,
      C_vector<S_symbol_info> &tmp_symbols, C_vector<byte> &table_templ, t_debug_info *debug_info);

//----------------------------
// Handle all process of compilation.
   ISL_RESULT CompileInternal(const char *fname, dword flags,
      const char *cmd_line,
      T_report_err *err_fnc1, void *cb_context);

   void RelocateTableTemplate();

   friend class C_v_machine;
   friend PC_script ISLAPI CreateScript();
public:
   ISLMETHOD_(dword,AddRef)(){ return ++ref; }
   ISLMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }

   ISLMETHOD_(ISL_RESULT,Compile)(const char *fname, dword flags = 0,
      const char *cmd_line = NULL,
      T_report_err *cb_err = NULL, void *cb_context = NULL);
   ISLMETHOD_(void,Free)();
   ISLMETHOD_(ISL_RESULT,GetStatus)() const{
      return comp_ok ? ISL_OK : ISLERR_NOPRG;
   }

   ISLMETHOD_(void,SetName)(const char *n){ script_name = n; }
   ISLMETHOD_(const char*,GetName()) const{ return script_name; }
   ISLMETHOD_(const void*,GetAddress)(const char *var_name) const;
   ISLMETHOD_(const class C_table_template*,GetTableTemplate)() const{
      if(!table_templ.size())
         return NULL;
      return (C_table_template*)table_templ.begin();
   }
   ISLMETHOD_(bool,Dump)(void(ISLAPI *func)(const char*, void *context), void *context) const;
};

//----------------------------

class C_v_machine{
   int ref;

   void *code, *data;         //code and data of loaded program
   dword code_size;           //size of code - used to resolve call addresses
   C_smart_ptr<C_script> script; //script of which program is (will be) loaded
   bool load_ok;
   dword user_data;           //user dword value
   C_smart_ptr<C_table> tab;  //table created from script's template

   C_v_machine();
   ~C_v_machine();
   dword do_save_context;     //set asynchronously by SaveContext during program run

   struct S_saved_context: public C_unknown{
      dword id;               //unique identifier of saved context (for given VM)

                              //saved registers and flags:
      dword _eax, _ebx;
      dword stack_frame_size; //size of frame (delta between esp and base_pointer)
      bool flag_bool;
      dword next_instr;       //offset of next instruction (relative to 'code' pointer)
      dword num_fnc_params;   //number of params of top function
                              //saved local stack
      C_vector<dword> stack;
      C_vector<dword> params;
   };
   C_vector<C_smart_ptr<S_saved_context> > saved_context_list;

   friend PC_v_machine ISLAPI CreateVM();

   dword FindUniqueSaveContextID() const;
public:
   ISLMETHOD_(dword,AddRef)(){ return ++ref; }
   ISLMETHOD_(dword,Release)(){ if(--ref) return ref; delete this; return 0; }
                              //load program from script
   ISLMETHOD_(ISL_RESULT,Load)(CPC_script, const VM_LOAD_SYMBOL*);
   ISLMETHOD_(ISL_RESULT,ReloadData)();
   ISLMETHOD_(void,Unload)();
   ISLMETHOD_(ISL_RESULT,GetStatus)() const{ return load_ok ? ISL_OK : ISLERR_NOPRG; }
                              //run loaded program
   ISLMETHOD_(ISL_RESULT,Run)(dword *retval, void *start, dword flags, dword num_pars=0, ...);
   ISLMETHOD_(void*,GetAddress)(const char *var_name) const;
   ISLMETHOD_(ISL_RESULT,EnumSymbols)(void (ISLAPI*cb_proc)(const char *name, void *address, bool is_data, void *context), void *cb_context) const;
   ISLMETHOD_(const char*,GetName)() const{ return script ? script->GetName() : NULL; }
                              //script
   ISLMETHOD_(CPC_script,GetScript)(){ return script; }
                              //user data
   ISLMETHOD_(void,SetData)(dword dw){ user_data = dw; }
   ISLMETHOD_(dword,GetData)() const{ return user_data; }

   ISLMETHOD_(ISL_RESULT,SaveContext)(dword *ret_id);
   ISLMETHOD_(ISL_RESULT,ClearSavedContext)(dword id);

   ISLMETHOD_(ISL_RESULT,GetSavedContext)(dword id, C_buffer<byte> &buf) const;
   ISLMETHOD_(ISL_RESULT,SetSavedContext)(dword *ret_id, const byte *buf, dword buf_size);

   ISLMETHOD_(ISL_RESULT,GetGlobalVariables)(C_buffer<byte> &buf) const;
   ISLMETHOD_(ISL_RESULT,SetGlobalVariables)(const byte *buf, dword buf_size);

   ISLMETHOD_(class C_table*,GetTable)(){ return tab; }
   ISLMETHOD_(ISL_RESULT,SetBreakpoint)(const char *file, dword line, bool on);
};

//--------------------------
