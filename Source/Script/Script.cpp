#include "all.h"
#include "iscrpt_i.h"

//----------------------------
//----------------------------
                              //functions
PC_script ISLAPI CreateScript(){
   return new C_script;
}

//----------------------------
//----------------------------

C_script::C_script():
   ref(1),
   reloc_symbols(NULL),
   reloc_data(NULL),
   comp_ok(false)
{
}

//----------------------------

C_script::~C_script(){
   Free();
}

//----------------------------

void C_script::Free(){

   if(comp_ok){
      code.clear();
      data.clear();
      delete[] reloc_symbols; reloc_symbols = NULL;
      delete[] reloc_data; reloc_data = NULL;
      table_templ.clear();
      dependency.clear();
      debug_info.clear();
      comp_ok = false;
   }
}

//----------------------------

ISL_RESULT C_script::Compile(const char *fname, dword flags,
   const char *cmd_line,
   T_report_err *err_fnc1, void *cb_context){

   Free();
                              //load pre-compiled script
#ifdef SCRIPT_COMPILE
   ISL_RESULT ret = CompileInternal(fname, flags, cmd_line, err_fnc1, cb_context);
   if(ISL_SUCCESS(ret))
      RelocateTableTemplate();
   return ret;
#else
   return ISLERR_NOFILE;
#endif 
}

//----------------------------

const void *C_script::GetAddress(const char *var_name) const{

   if(reloc_symbols){
      for(S_symbol_info *si = reloc_symbols; !si->IsLast(); ++si)
         if(!strcmp(si->GetName(), var_name)) break;
      if(!si->IsLast()){
                              //can only return pointer to data
         if(si->flags&SI_DATA){
            return data.begin() + si->GetAddress();
         }
      }
   }
   return NULL;
}

//----------------------------

#define OC(name) {name, #name}
#define OP(name, param) {name, #name, param}

enum PARAM_TYPE{
   PAR_NO,                    //no parameters
   PAR_C8,                    //constant 8-bit
   PAR_C16,                   //constant 16-bit
   PAR_C16_X2,                //constant 16-bit (2 times)
   PAR_C32,                   //constant 32-bit
   PAR_A32,                   //absolute 32-bit address
   PAR_D16,                   //data-relative 16-bit address
   PAR_R16,                   //code-relative 16-bit address
   PAR_T8,                    //8-bit index into table
};

struct S_opcode_map{
   OPCODE op;
   const char *name;
   PARAM_TYPE param;
} opcode_map[] = {
   OC(BRK),
            //compiler
   OP(ENTER, PAR_C16),
   OC(LEAVE),
            //memory
   OP(MOV_AL, PAR_C8),
   OP(MOV_AX, PAR_C16),
   OP(MOV_EAX, PAR_C32),
   OP(MOV_AL_G, PAR_A32),
   OP(MOV_AX_G, PAR_A32),
   OP(MOV_EAX_G, PAR_A32),
   OP(MOV_AL_L, PAR_D16),
   OP(MOV_AX_L, PAR_D16),
   OP(MOV_EAX_L, PAR_D16),
   OP(MOV_AL_L, PAR_D16),
   OP(MOV_AX_L, PAR_D16),
   OP(MOV_EAX_L, PAR_D16),
   OP(MOV_G_AL, PAR_A32),
   OP(MOV_G_AX, PAR_A32),
   OP(MOV_G_EAX, PAR_A32),
   OP(MOV_L_AL, PAR_D16),
   OP(MOV_L_AX, PAR_D16),
   OP(MOV_L_EAX, PAR_D16),
   OC(MOV_EAX_FPU),
   OC(MOV_FPU_EAX),
   OC(MOV_AL_PTR),
   OC(NOP),
            //type conversion
   OC(MOVZX_EAX_AL),
   OC(MOVZX_EAX_AX),
   OC(MOVSX_EAX_AL),
   OC(MOVSX_EAX_AX),
   OC(MOV_EAX_FLT),
   OC(MOV_FLT_EAX),
   OC(MOVS_FLT_EAX),
            //arithmetic
   OC(INC_EAX),
   OC(DEC_EAX),
   OP(ADD_EAX, PAR_C32),
   OP(SUB_EAX, PAR_C32),
   OC(ADD_POP),
   OC(SUB_POP),
   OC(ADDF_EAX_STP),
   OC(SUBF_EAX_STP),
   OC(MUL_EAX_STP),
   OC(IMUL_EAX_STP),
   OC(FMUL_EAX_STP),
   OP(IMUL_EAX, PAR_C32),
   OC(DIV_EAX_STP),
   OC(IDIV_EAX_STP),
   OC(FDIV_EAX_STP),
   OC(MOD_EAX_STP),
   OC(IMOD_EAX_STP),
            //bitwise
   OC(AND_EAX_STP),
   OC(XOR_EAX_STP),
   OC(OR_EAX_STP),
   OP(SHL_EAX, PAR_C8),
   OC(SHL_EAX),
   OC(SHL_EAX_STP),
   OC(SHR_EAX_STP),
   OC(SAR_EAX_STP),
   OC(NEG),
   OC(NEGF),
   OC(NOT),
            //boolean operations
   OP(EQ_AL, PAR_C8),
   OP(EQ_AX, PAR_C16),
   OP(EQ_EAX, PAR_C32),
   OC(EQ_EAX_STP),
   OC(SET_B_AL),
   OC(SET_B_AX),
   OC(SET_B_EAX),
   OC(SET_EAX_B),
   OC(NOTB),
   OC(BAND_EAX_STP),
   OC(BOR_EAX_STP),
   OC(LT),
   OC(LTS),
   OC(LTF),
   OC(LTE),
   OC(LTSE),
   OC(LTFE),
   OC(GT),
   OC(GTS),
   OC(GTF),
   OC(GTE),
   OC(GTSE),
   OC(GTFE),
            //jump
   OP(JMP, PAR_R16),
   OP(JTRUE, PAR_R16),
   OP(JFALSE, PAR_R16),
            //stack
   OC(PUSH),
   OC(POP),
   OP(CALL, PAR_A32),
   OC(RET),
   OP(RETN, PAR_C16),

   OP(MOV_AL_TAB, PAR_T8),
   OP(MOV_EAX_TAB, PAR_T8),
   OP(MOV_EAX_TABS, PAR_T8),
   OP(MOV_AL_TAB_POP, PAR_T8),
   OP(MOV_EAX_TAB_POP, PAR_T8),
   OP(MOV_EAX_TABS_POP, PAR_T8),

   OC(OP_STRING),
   {OP_SCRIPT_FUNC_CODE, ">", PAR_C16},

   (OPCODE)-1
};

static const S_opcode_map *GetOpcodeType(OPCODE op){

   const S_opcode_map *mp;
   for(mp = opcode_map; mp->op!=-1; ++mp){
      if(mp->op==op)
         return mp;
   }
   return NULL;
}

//----------------------------

static C_str ConvertString(const char *cp, int *scanned = NULL){

   C_vector<char> s;

   int i = 0;
   while(true){
      ++i;
      char c = *cp++;
      switch(c){
      case '\n':
         s.push_back('~');
         s.push_back('n');
         break;
      default:
         s.push_back(c);
      }
      if(!c)
         break;
   }
   if(scanned)
      *scanned = i;
   return C_fstr(" \"%s\"", s.begin());
}

//----------------------------

bool C_script::Dump(void(ISLAPI *func)(const char*, void *context), void *context) const{

   func(C_fstr("Listing script: %s", (script_name.Size() ? (const char*)script_name : "<unnamed>")), context);
   if(code.size()){
      func("", context);
      func("code:", context);

      bool search_func_name = true;
      const byte *bp = (const byte*)code.begin();
      for(dword i=0; i<code.size(); ){
         if(search_func_name){
                                 //try to find function in exported table
            if(reloc_symbols){
               for(S_symbol_info *si=reloc_symbols; !si->IsLast(); si++){
                  if(!(si->flags&SI_DATA) && si->address == i){
                     func(C_fstr(" Function %s:", si->sym_name), context);
                     search_func_name = false;
                     break;
                  }
               }
            }

         }
         OPCODE op = (OPCODE)bp[i];
         const S_opcode_map *om = GetOpcodeType(op);
         if(!om){
            func(C_fstr("*** invalid opcode: 0x%.2x", op), context);
            return false;
         }
         C_fstr str("0x%.4x    %s", i, om->name);
         ++i;

         PARAM_TYPE pt = om->param;
         switch(pt){
         case PAR_NO:
            break;
         case PAR_C8:
            str += C_fstr(", %i", *(byte*)(bp+i));
            i += 1;
            break;
         case PAR_C16:
            str += C_fstr(", %i", *(word*)(bp+i));
            i += 2;
            break;
         case PAR_C16_X2:
            str += C_fstr(", %i, %i", ((word*)(bp+i))[0], ((word*)(bp+i))[1]);
            i += 4;
            break;
         case PAR_D16:
            str += C_fstr(", [0x%.4x]", *(word*)(bp+i));
            i += 2;
            break;
         case PAR_R16:
            {
               int rel = *(short*)(bp+i) + 2;
               str += C_fstr(" [0x%x]", i+rel);
               i += 2;
            }
            break;
         case PAR_C32:
         case PAR_A32:
            {
               dword addr = *(dword*)(bp+i);
               C_str sym_name;
               if(reloc_data){
                  for(S_symbol_info *si=reloc_data; !si->IsLast(); si++){
                     if(si->address == i){
                        switch(si->type){
                        case SI_EXTERNAL:
                        case SI_LOCAL:

                           sym_name = C_fstr(", [%s%s]",
                              si->sym_name, !addr ? "" : (const char*)C_fstr(" + 0x%x", addr));
                           break;
                        case SI_CODEOFFSET:
                           if(!(si->flags&SI_TEMPLATE)){
                              dword addy = *(dword*)(bp+i);
                              sym_name = ", ";
                              sym_name += ConvertString((const char*)bp+addy);
                           }
                           break;
                        }
                        if(sym_name.Size())
                           break;
                     }
                  }
               }
               if(!sym_name.Size()){
                  sym_name = C_fstr(pt==PAR_C32 ? ", %i" : ", [0x%.4x]", addr);
               }
                                 //find this address among relocations
               str += sym_name;
            }
            i += 4;
            break;
         case PAR_T8:
            {
               int indx = *(byte*)(bp+i);
               /*
               const C_table_template *tt = (const C_table_template*)table_templ;
               const char *name = tt->te[indx].caption;
               str += C_fstr(" [%i] (\"%s\")", indx, name);
               */
               str += C_fstr(" [%i]", indx);
               i += 1;
            }
            break;
         default:
            assert(0);
         }
         switch(op){
         case OP_STRING:
            {
               int num;
               str += " ";
               str += ConvertString((const char*)bp+i, &num);
               i += num;
            }
            break;
         case RET:
         case RETN:
            str += "";
            search_func_name = true;
            break;
         }
         func(str, context);
         //func("", context);
      }
   }
   if(data.size()){
      func("data:", context);
      const byte *bp = (const byte*)data.begin();
      for(dword i=0; i<data.size(); ){
         C_fstr str("0x%.4x  ", i);
         int j = Min(data.size()-i, 8ul);
         char graph[9];
         for(int ii=0; ii<j; ii++){
            byte c = bp[i+ii];
            str += C_fstr("%.2x ", c);
            if(!isalnum(c))
               c = '.';
            graph[ii] = c;
         }
         for(; ii<8; ii++)
            str += "   ";
         graph[j] = 0;
         str += graph;
         func(str, context);
         func("", context);
         i += j;
      }
   }
   if(table_templ.size()){
      func("", context);
      func(C_fstr("table template size: %i", table_templ.size()), context);
   }
   if(debug_info.size()){
      func("", context);
      func("Debug info:", context);
   }
   func(" --- end ---\n", context);
   return true;
}

//----------------------------
//----------------------------
