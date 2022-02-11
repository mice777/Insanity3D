#include "iscrpt_i.h"

//----------------------------

enum E_LEXCODE{
   IDENTIFIER = 258, CONSTANT,
   STRING_LITERAL, PTR_OP, INC_OP, DEC_OP,
   LEFT_OP, RIGHT_OP, LE_OP, GE_OP,
   EQ_OP, NE_OP, AND_OP, OR_OP,

   MUL_ASSIGN, DIV_ASSIGN, MOD_ASSIGN, ADD_ASSIGN,
   SUB_ASSIGN, LEFT_ASSIGN, RIGHT_ASSIGN, AND_ASSIGN,
   XOR_ASSIGN, OR_ASSIGN, TYPE_NAME, ELIPSIS,
   //RANGE,                     //not used at all
                              //first keyword
   TYPEDEF,
   //SIZEOF,
   EXTERN, STATIC, AUTO, REGISTER,
   CHAR, SHORT, INT,
   //LONG,
   //SIGNED, UNSIGNED,
   FLOAT,
   //DOUBLE,
   CONST,
   //VOLATILE,
   VOID, STRUCT,
   //UNION,

   ENUM, CASE, DEFAULT, IF,
   ELSE, SWITCH, WHILE, DO,
   FOR,
   //GOTO,
   CONTINUE, BREAK,
   RETURN,
   BYTE, WORD, DWORD, STRING, BOOL, TRUE, FALSE,

   TABLE, BRANCH,

   TOKEN_LAST
};

//----------------------------
                              //error/warning compile messages
extern const char *err_msgs[];

#define ERR_SYNTAX      (err_msgs[0])
#define ERR_ILLTS       (err_msgs[1])
#define ERR_VOIDVAR     (err_msgs[2])
#define ERR_NOFUNC      (err_msgs[3])
#define ERR_NOLVALUE    (err_msgs[4])
#define ERR_CANTASSIGN  (err_msgs[5])
#define ERR_NOMEAN      (err_msgs[6])
#define ERR_CONSTMODIFY (err_msgs[7])
#define ERR_MUSTBECONST (err_msgs[8])
#define ERR_MUSTBEINT   (err_msgs[9])
#define ERR_CANTBEVOID  (err_msgs[10])
#define ERR_MISMATCH    (err_msgs[11])
#define ERR_TYPEDEFINED (err_msgs[12])
#define ERR_NOTDECLARED (err_msgs[13])
#define ERR_DIMSIZE     (err_msgs[14])
#define ERR_MANYINIT    (err_msgs[15])
#define ERR_MUSTBEPTR   (err_msgs[16])
#define ERR_BADSUBSCR   (err_msgs[17])
#define ERR_MUSTBEARITH (err_msgs[18])
#define ERR_NOSTRMEMBER (err_msgs[19])
#define ERR_INVSTORAGE  (err_msgs[20])
#define ERR_ENUMASSIGN  (err_msgs[21])
#define ERR_MISSRETVAL  (err_msgs[22])
#define ERR_CANTRETVAL  (err_msgs[23])
#define ERR_ZERODIVIDE  (err_msgs[24])

//----------------------------
                              //store values into segment
inline void StoreByte(C_vector<byte> &seg, byte b){
   seg.push_back(b);
}

inline void StoreWord(C_vector<byte> &seg, word w){
   seg.push_back(byte(w&255));
   seg.push_back(byte(w>>8));
}

inline void StoreDWord(C_vector<byte> &seg, dword dw){
   StoreWord(seg, word(dw&0xffff));
   StoreWord(seg, word((dw>>16)&0xffff));
}

//----------------------------

enum E_TYPE_SPECIFIER{
   TS_NULL,
   TS_VOID,
   TS_CHAR,
   TS_BYTE,
   TS_SHORT,
   TS_WORD,
   TS_INT,
   TS_DWORD,
   TS_FLOAT,
   TS_BOOL,
   TS_LITERAL,                //local pointer ('value') to instantiated string in const segment
   TS_STRING,                 //real pointer
   TS_ENUM_CONSTANT,
   TS_ENUM_TYPE,
   TS_STRUCT,
   TS_FUNC,
   TS_DEFAULT_VALUE,          //use default value specified at function declaration
};

#define TYPE_CONST      0x1   //contant
#define TYPE_EVAL       0x2   //expression is evaluated in accumulator
#define TYPE_LOCAL      0x4   //local variable
#define TYPE_PARAM      0x8   //parameter into function
#define TYPE_ARRAY      0x10  //array of variables
#define TYPE_TYPEDEF    0x20  //user-defined type
#define TYPE_EXTERN     0x40  //external linkage
#define TYPE_TABLE      0x80  //variable is member of table
#define TYPE_PARAM_DEF  0x100 //parameter with default value specified


class C_c_type_imp;
typedef C_smart_ptr<C_c_type_imp> C_c_type;
                              //general class holding information
                              //during compilation
class C_c_type_imp{
   dword ref;

   E_TYPE_SPECIFIER type;
   dword type_flags;

   union{
      short address;          //local variable or param offset on stack (in dwords)
      word frame_size;        //# of dwords of local variables (TS_FUNC)
   };
   //word struct_offset;

                              //value of constant, or address
   union{
      dword dw;
      float f;
   } value;

   C_str name;             //name of variable, function, type, etc

   C_vector<C_c_type> list;
   C_vector<C_c_type> struct_members;

                              //code of expression/block
   C_vector<byte> code;
   C_vector<S_symbol_info> reloc;

   dword debug_info_line;     //line index of current source (or 0 if not applicable)

                              //reference to type, used for:
                              // function return type, enum type, initialization value
   C_c_type type_ref;

                              //parent type, used for identification of enums and typedefs
   C_c_type parent_type;

   C_c_type table_desc;       //description of table member (name)

public:
   C_c_type_imp():
      ref(1)
   {
      Reset();
   }

   dword AddRef(){ return ++ref; }
   dword Release(){ if(--ref) return ref; delete this; return 0; }


   void Reset();

//----------------------------
// Determine if type is contant. 
   inline bool IsConst() const{ return (type_flags&TYPE_CONST); }

//----------------------------
// Frame count.
   inline void SetFrameSize(dword dw){ frame_size = (word)dw; }
   inline void AddFrameSize(dword dw){ frame_size = word(frame_size + dw); }
   inline dword GetFrameSize() const{ return frame_size; }

//----------------------------
// Set/Get name.
   inline void SetName(const C_str &n){ name = n; }
   inline const C_str &GetName() const{ return name; }

//----------------------------
// Set/Get value, using dword and float types.
   inline void SetValue(dword val){ value.dw = val; }
   inline dword GetValue() const{ return value.dw; }
   inline void SetFValue(float f){ value.f = f; }
   inline float GetFValue() const{ return value.f; }

//----------------------------
   inline void SetDebugLine(dword l){ debug_info_line = l; }
   inline dword GetDebugLine() const{ return debug_info_line; }

//----------------------------
// Set/Get type and its flags.
   inline void SetType(E_TYPE_SPECIFIER t){ type = t; }
   inline E_TYPE_SPECIFIER GetType() const{ return type; }
   inline void SetTypeFlags(dword tf){ type_flags = tf; }
   inline void AddTypeFlags(dword tf){ type_flags |= tf; }
   inline void ClearTypeFlags(dword tf){ type_flags &= ~tf; }
   inline dword GetTypeFlags() const{ return type_flags; }
   void CopyType(const C_c_type &ct);
   inline void CopyName(const C_c_type &ct){ name = ct->name; }
   inline void CopyTypeName(const C_c_type &ct){ CopyType(ct); CopyName(ct); }

//----------------------------
// Set/Get return type of function.
   inline const C_c_type &GetReturnType() const{
      assert(type==TS_FUNC);
      return type_ref;
   }
   inline void SetReturnType(const C_c_type &rt){
      //assert(type==TS_FUNC);
      type_ref = rt;
   }
   inline const C_c_type &GetFunctionBody() const{
      assert(type==TS_FUNC);
      return parent_type;
   }
   inline void SetFunctionBody(const C_c_type &rt){
      assert(type==TS_FUNC);
      parent_type = rt;
   }

//----------------------------
// Set/Get description of table member.
   inline const C_c_type &GetTableDesc() const{ return table_desc; }
   inline void SetTableDesc(const C_c_type &t){ table_desc = t; }

//----------------------------
// Set/Get enum's constant's enumerator type.
   inline const C_c_type &GetParentType() const{ return parent_type; }
   inline void SetParentType(const C_c_type &t){ parent_type = t; }

//----------------------------
// Set/Get initializer of init_declarator.
   inline const C_c_type &GetInitializer() const{
      return type_ref;
   }
   inline void SetInitializer(const C_c_type &rt){
      type_ref = rt;
   }

//----------------------------
// Structs.
   inline dword NumStructMembers() const{
      assert(type==TS_STRUCT);
      return struct_members.size();
   }
   inline const C_c_type &StructMember(dword indx) const{
      assert(type==TS_STRUCT);
      assert(indx<struct_members.size());
      return struct_members[indx];
   }
   inline void AddStructMember(const C_c_type &ct){
      assert(type==TS_STRUCT);
      struct_members.push_back(ct);
   }
   /*
   inline void SetStructOffset(word w){ struct_offset = w; }
   inline word GetStructOffset() const{ return struct_offset; }
   */

//----------------------------
// List of other types.
   inline void AddToList(const C_c_type &ct){
      list.push_back(ct);
   }
   inline dword ListSize() const{ return list.size(); }
   inline C_c_type &ListMember(dword i){ assert(i<list.size()); return list[i]; }
   inline const C_c_type &ListMember(dword i) const{ assert(i<list.size()); return list[i]; }
   inline C_vector<C_c_type> &GetList(){ return list; }
   inline const C_vector<C_c_type> &GetList() const{ return list; }
   inline void ClearList(){ list.clear(); }
   inline void CopyList(const C_c_type &ct){ list = ct->list; }

//----------------------------
// Get size of current type.
   dword SizeOf() const;

//----------------------------
// Set/Get address.
   inline void SetAddress(short s){ address = s; }
   inline short GetAddress() const{ return address; }

//----------------------------
// Code.
   inline const byte *GetCode() const{ return code.size() ? &code[0] : NULL; }
   inline dword GetCodeSize() const{ return code.size(); }

//----------------------------
// Relocations.
   inline const S_symbol_info *GetRelocs() const{ return &reloc[0]; }
   inline dword GetRelocNum() const{ return reloc.size(); }
   inline void AddReloc(const S_symbol_info &si){ reloc.push_back(si); }
   void RemoveReloc(dword i){
      if(GetRelocNum() > i){ 
         reloc[i] = reloc.back();
         reloc.pop_back();
      }
   }

//----------------------------
// Create code fragment - load accumulator with parameter and preform conversion to type.
   bool codePickParam(const C_c_type &ct_r, E_TYPE_SPECIFIER to_type, C_str &err,
      C_vector<byte> *tmp_code = NULL, const C_c_type &ct_l = NULL);

//----------------------------
// Create code fragment - store accumulator.
// No conversion performed, assuming types of operand and location match
   bool codeStore(const C_c_type &l, C_str&);

//----------------------------
// Create code fragment - assignment of right operand to left operand through accumulator,
// with necessary type conversion.
   bool codeAssign(const C_c_type &l, dword op, const C_c_type &r,
      C_str &err, bool const_check = true);

//----------------------------
// Create code fragment - function call,
// with converting and pushing all parameters onto stack right-to-left.
//    ct ... function type to be called
//    params ... pointer to parameters into function, or NULL if no params
   bool MakeFunctionCall(const C_c_type &ct, const C_vector<C_c_type> *args, const C_vector<C_c_type> *enum_args,
      C_str &err, C_vector<byte> &tmp_code);

//----------------------------
// Assembles declaration of function (for error output).
   C_str GetFunctionDeclaration(const C_c_type &params) const;

//----------------------------
// Join code with relocations.
   void codeAddCode(const C_c_type &ct);

//----------------------------
// Load accumulator to FPU.
   void codePickFPUValue();

//----------------------------
// Code fragment - inc or dec operator.
//    inc ... true for ++, false for --
//    pick ... true for picking the expression from this
//    save_acc ... true for push/pop original value on stack
   bool codeIncDecOp(C_str &err, bool inc, bool pick, bool save_acc);

//----------------------------
// Code fragment - subscript on array - create reference to array member.
   bool codeSubscript(const C_c_type &ct_i, C_str &err);

//----------------------------
// Code fragment - struct member access create reference to member of struct.
   //bool codeStructMember(C_str &str, const C_str &mname);

//----------------------------
// Code fragment - struct member access dereference struct pointer and create reference to its member.
   //bool codeStructMemberPtr(C_str &str, const C_str &mname);

//----------------------------
// Code fragment - unary operation.
   bool codeUnaryOp(dword op_type, C_str &err);

//----------------------------
// Get type into which two expression may evaluate; this may be float, int or dword.
// Return TS_NULL if error encountered (cannot promote).
   E_TYPE_SPECIFIER GetPromoteType(const C_c_type &t1, C_str &err, bool op_assign = false) const;

//----------------------------
// Get constant value of expresion, converted into given type,
// which may be float, int or dword.
   dword GetPromoteVal(E_TYPE_SPECIFIER type) const;

//----------------------------
// Code fragment - all arithmetic operations.
   bool codeAritmOp(const C_c_type &l, dword op, const C_c_type &r, C_str &err);

//----------------------------
// Code fragment - comparement operations; yelding boolean result.
   bool codeBooleanOp(const C_c_type &l, dword op, const C_c_type &r, C_str &err);

//----------------------------
// Code fragment - skip on condition.
   bool codeSkipCond(bool skip_on_true, const C_c_type &expr, int skip_count, C_str &str);

//----------------------------
// Code fragment - skip unconditionaly.
   void codeSkip(int skip_count);

//----------------------------
// Code fragment - return statement.
   void codeReturn();

//----------------------------
// Code fragment - skip for continue and break statements.
   void codeSkipIter(E_SYMBOL_TYPE reloc_type);

//----------------------------
// Solve break or continue relocations.
   void codeRelocIterSkip(E_SYMBOL_TYPE stype, int skip_pos);

//----------------------------
// Code fragment - build switch comparision branch.
   bool codeBuildSwitchBranch(C_c_type &stmt, dword expr_size, C_str &err);

   int FindDeclSymbol(const S_symbol_info &si) const;

   void SolveCompoundRelocations(const C_c_type &loc_sym, dword add_frm_size);

//----------------------------
// Instantiate data in temp data segment.
   bool dataInstantiate(C_vector<byte> &tmp_data, C_vector<S_symbol_info> &tmp_symbols,
      C_vector<S_symbol_info> &relocs, C_str &err);

//----------------------------
// Instantiate table template from provided initializers.
   bool InstantiateTableTemplate(const C_c_type &name, C_vector<byte> &out_template, C_vector<S_symbol_info> &reloc,
      C_str &err);
};

//----------------------------
                              //scope class - keeping current scope level
                              //and namespaces for all levels, also
                              //used to look-up for identifier by
                              //scope resolution rules
class C_scope{
   typedef map<C_str, C_c_type> t_type_map;
   struct S_name_space{
      t_type_map identifiers; //variables, structs, functions, etc
      t_type_map typenames;   //typedef'd types
      //t_type_map func_prototypes;   //typedef'd function prototypes

      S_name_space(){}
      S_name_space(const S_name_space &ns){ operator =(ns); }
      void operator =(const S_name_space &ns){
         identifiers = ns.identifiers;
         typenames = ns.typenames;
         //func_prototypes = ns.func_prototypes;
      }
   };
   C_vector<S_name_space> name_space;
                              //just list of label names to check duplication
   struct S_goto_label{
      C_str name;
      S_location loc;

      void Setup(const C_str &n, const S_location &l){
         name = n;
         loc = l;
      }
   };
public:
   C_scope(){
                              //create file scope and global namespace
      name_space.push_back(S_name_space());
   }

//----------------------------
// Add identifier into current namespace.
   bool AddIdentifier(const C_c_type &ct);
//----------------------------

// Find identifier by name.
   const C_c_type FindIdentifier(const C_str &name) const;

//----------------------------
// Check if identifier exists in current namespace.
   bool IsIdentifierInScope(const C_str &name);

//----------------------------
// Add identifier into current namespace.
   bool AddType(const C_c_type &ct);

//----------------------------
// Find type by name.
   const C_c_type FindType(const C_str &name) const;

//----------------------------
// Check if type name exists in current namespace.
   bool IsTypeInScope(const C_str &name);

//----------------------------
// Add typedef'd function prototype.
   //bool AddFunctionPrototype(const C_c_type &ct);

//----------------------------
// Find function prototype by name.   
   //const C_c_type FindFunctionPrototype(const C_str &name) const;

//----------------------------
// Get current scope level.
   inline dword GetLevel() const{ return name_space.size()-1; }

//----------------------------
// Increment scope level.
   inline void IncLevel(){
      name_space.push_back(S_name_space());
   }
//----------------------------
// Decrement scope level.
   inline void DecLevel(){
      name_space.pop_back();
   }
};

//----------------------------
                              //include file
struct S_incl_file{
   C_cache *ck;
   C_str name;
   S_location loc;

   S_incl_file(){}
   S_incl_file(C_cache *ck1, const C_str &n):
      ck(ck1), name(n)
   {}
};

//----------------------------

class C_compile_process{
public:
                              //dependency info - filenames on which the script is dependent
   C_vector<C_str> dependency;

   C_vector<C_str> include_paths;
                              //include files - behaves as stack
   C_vector<S_incl_file> include_files;
   S_location yyloc;

   t_debug_info debug_info;
   t_file_debug_info *curr_dbg_info;

   C_script::T_report_err *err_fnc;
   void *cb_err_context;
   int num_errors, max_errors;

                              //string pool - mapping of string to string address (relative to code segment)
                              // (used for string concanetation)
   typedef map<C_str, dword> t_string_pool;
   t_string_pool string_pool;

//----------------------------

   C_compile_process():
      num_errors(0),
      max_errors(10),
      curr_dbg_info(NULL)
   {}

   int yyLex(C_c_type&, const C_scope&, C_str &err);

//----------------------------
// Funtion called to report compile error or warning.
   void Error(const char *s, bool warn = false, bool display_loc = true);

//----------------------------
// Add single dependency file.
   void AddDependency(const C_str &str){

      C_str add = str;
      add.ToLower();
      for(int i=dependency.size(); i--; ){
         if(dependency[i]==add)
            break;
      }
      if(i==-1)
         dependency.push_back(add);
   }
};

//----------------------------
//----------------------------

