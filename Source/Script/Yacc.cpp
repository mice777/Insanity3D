//----------------------------
// Main "C" grammar processing class,
// code generator
//----------------------------

#include "all.h"
#include "yacc.h"
#include <math.h>

//----------------------------
//----------------------------

void C_c_type_imp::Reset(){

   type = TS_NULL;
   type_flags = 0;
   frame_size = 0;
   address = 0;
   //struct_offset = 0;
   value.dw = 0;

                           //other class members
   name = NULL;
   list.clear();
   code.clear();
   reloc.clear();
   type_ref = NULL;
   parent_type = NULL;
   table_desc = NULL;
   debug_info_line = 0;
}

//----------------------------

void C_c_type_imp::CopyType(const C_c_type &ct){

   type = ct->type;
   type_flags = ct->type_flags;
   parent_type = ct->parent_type;
   struct_members = ct->struct_members;
   code = ct->code;
   address = ct->address;
   reloc = ct->reloc;
}

//----------------------------

dword C_c_type_imp::SizeOf() const{

   switch(type){
   case TS_NULL: return 0;
   case TS_VOID: return 0;
   case TS_CHAR:
   case TS_BOOL:
   case TS_BYTE: return 1;
   case TS_SHORT:
   case TS_WORD: return 2;
   case TS_INT:
   case TS_DWORD:
   case TS_FLOAT: return 4;
   case TS_STRING: return 4;
   case TS_ENUM_CONSTANT: return 2;
   case TS_ENUM_TYPE: return 2;
   case TS_FUNC: return 0;
   default: assert(("SizeOf - unknown type", 0));
   }
   return 0;
}

//----------------------------

void C_c_type_imp::codePickFPUValue(){

   code.push_back(MOV_FPU_EAX);
}

//----------------------------

void C_c_type_imp::codeAddCode(const C_c_type &ct){

                              //add reloc table
   dword reloc_base = code.size();
   const S_symbol_info *rt = ct->GetRelocs();
   for(dword i=0; i<ct->GetRelocNum(); i++){
      reloc.push_back(S_symbol_info(rt[i]));
      reloc.back().address += reloc_base;
   }
   const byte *code_ptr = ct->GetCode();
   for(i=0; i<ct->GetCodeSize(); i++)
      code.push_back(code_ptr[i]);
}

//----------------------------

void C_c_type_imp::codeSkip(int skip_count){

                              //if skipping back, skip also this opcode
   code.push_back(JMP);
   StoreWord(code, word(skip_count<0 ? (skip_count-3) : skip_count));
}

//----------------------------

void C_c_type_imp::codeReturn(){
   code.push_back(JMP);
                              //create reloc info
   reloc.push_back(S_symbol_info());
   reloc.back().Setup(SI_RETURN, NULL, code.size());

   StoreWord(code, 0);        //will be relocated later
}

//----------------------------

void C_c_type_imp::codeSkipIter(E_SYMBOL_TYPE reloc_type){

   code.push_back(JMP);
                              //create reloc info
   reloc.push_back(S_symbol_info());
   reloc.back().Setup(reloc_type, NULL, code.size());

   StoreWord(code, 0);        //will be relocated later
}

//----------------------------

void C_c_type_imp::codeRelocIterSkip(E_SYMBOL_TYPE stype, int skip_pos){

   for(dword i=0; i<reloc.size(); ){
      S_symbol_info &ri = reloc[i];
      if(ri.type == stype){
         *((word*)&code[ri.GetAddress()]) = (word)(skip_pos-ri.GetAddress() - 2);
         ri = reloc.back();
         reloc.pop_back();
      }else i++;
   }
}

//----------------------------

int C_c_type_imp::FindDeclSymbol(const S_symbol_info &si) const{

   if(si.type == SI_AUTOVAR){
      const char *sname=si.GetName();
      if(sname)
      for(dword i=0; i<ListSize(); i++){
         const C_c_type &ct=ListMember(i);
         if((ct->type_flags&TYPE_LOCAL) && ct->GetName().Size() && !strcmp(ct->GetName(), sname))
            return i;
      }
   }
   return -1;
}

//----------------------------

void C_c_type_imp::SolveCompoundRelocations(const C_c_type &loc_sym, dword add_frm_size){

   const S_symbol_info *rt = GetRelocs();
   const byte *code_ptr = GetCode();
   for(dword i=0; i<GetRelocNum(); ){
      int di = loc_sym->FindDeclSymbol(rt[i]);
      if(di!=-1){          //resolved
         *((short*)(code_ptr+rt[i].address)) = (short)(-int((di+add_frm_size+1) * 4));
         RemoveReloc(i);
      }else ++i;
   }
}

//----------------------------

dword C_c_type_imp::GetPromoteVal(E_TYPE_SPECIFIER type) const{

   E_TYPE_SPECIFIER from_type = GetType();
   union{
      int l;
      dword u;
      float f;
   };
   u = GetValue();

   switch(type){
   case TS_INT:
      if(from_type==TS_FLOAT)
         l = (int)f;
      break;
   case TS_FLOAT:
      switch(from_type){
      case TS_INT: f = (float)l; break;
      case TS_DWORD: f = (float)u; break;
      }
      break;
   }
   return u;
}

//----------------------------
                              //functions of C_c_type - class for storing
                              // all results of compilation
bool C_c_type_imp::codePickParam(const C_c_type &ct_r, E_TYPE_SPECIFIER to_type, C_str &err,
   C_vector<byte> *tmp_code, const C_c_type &ct_l){

                           //can't convert to void
   if(to_type==TS_VOID)
      return false;
                           //can't convert from void
   E_TYPE_SPECIFIER from_type = ct_r->GetType();

   switch(from_type){
   case TS_FUNC:
   case TS_STRUCT:
      err = "invalid type";
      return false;
   case TS_VOID:
      return false;
   }
                           //already picked
   union{
      dword value;
      int svalue;
      float flt;
   };
   if(ct_r->IsConst()){
      if(to_type==TS_STRING){
         switch(from_type){
         case TS_STRING:
            if(ct_r->GetTypeFlags()&TYPE_EVAL){
                              //expression evaluated, just copy code
               if(ct_r != this)
                  codeAddCode(ct_r);
            }else
            if(ct_r->GetTypeFlags()&TYPE_PARAM_DEF){
               assert(tmp_code);
                              //instantiate default parameter now
               C_vector<byte> &seg = *tmp_code;
               seg.push_back(OP_STRING);
               dword offs = seg.size();
                                       //instantiate string in const segment
               assert(ct_r->GetParentType());
               const C_str &str = ct_r->GetParentType()->GetName();
               if(str.Size()){
                  seg.insert(seg.end(), (byte*)(const char*)str, ((byte*)(const char*)str)+str.Size()+1);
               }else
                  seg.push_back(0);

               code.push_back(MOV_EAX);
               reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, code.size()));
               StoreDWord(code, offs);    //local address - will be relocated
            }else{
               //err = "cannot convert string";
               //return false;
                              //generate pointer into const segment
               code.push_back(MOV_EAX);
               reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, code.size()));
               StoreDWord(code, ct_r->GetValue());   //local address - will be relocated
            }
            break;
         case TS_LITERAL:
                              //generate pointer into const segment
            code.push_back(MOV_EAX);
            reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, code.size()));
            StoreDWord(code, ct_r->GetValue());   //local address - will be relocated
            break;
         default:
            err = "cannot convert string";
            return false;
         }
         return true;
      }
      if(from_type==TS_STRING || from_type==TS_LITERAL){
         err = "cannot convert string";
         return false;
      }
      value = ct_r->GetValue();
                           //constant into eax
      switch(to_type){
      case TS_CHAR:
      case TS_BYTE:
      case TS_BOOL:
         code.push_back(MOV_AL);
         if(from_type==TS_FLOAT)
            value = (dword)*((float*)&value);
         StoreByte(code, (byte)value);
         return true;

      case TS_SHORT:
      case TS_WORD:
      case TS_ENUM_CONSTANT:
         code.push_back(MOV_AX);
         if(from_type==TS_FLOAT)
            value = (dword)*((float*)&value);
         StoreWord(code, (word)value);
         return true;

      case TS_INT: case TS_DWORD:
         code.push_back(MOV_EAX);
         if(from_type==TS_FLOAT)
            value = (dword)*((float*)&value);
         StoreDWord(code, value);
         return true;

      case TS_FLOAT:
         code.push_back(MOV_EAX);
         if(from_type!=TS_FLOAT){
            switch(from_type){
            case TS_INT: flt = (float)svalue; break;
            case TS_DWORD: flt = (float)value; break;
            default: assert(0);
            }
         }
         StoreDWord(code, value);
         return true;

      case TS_ENUM_TYPE:
         switch(from_type){
         case TS_ENUM_CONSTANT:
         case TS_ENUM_TYPE:
                              //check if types matches
            if(ct_l && ct_r->GetParentType()==ct_l->GetParentType()){
                              //must pick as 32-byte param, because we may call external C/C++ functions
                              // which treat enum params as dwords
               code.push_back(MOV_EAX);
               StoreDWord(code, value);
               return true;
            }
            break;
         }
         break;
      }
      return false;
   }else{
      if(ct_r->GetTypeFlags()&TYPE_TABLE){
         if(to_type==TS_STRING && ct_r->GetType()!=TS_STRING){
            err = "not a string";
            return false;
         }
                              //special accessing of table members
         if(ct_r->GetTypeFlags()&TYPE_ARRAY){
            //assert(ct->GetTypeFlags()&TYPE_EVAL);
            if(!(ct_r->GetTypeFlags()&TYPE_EVAL)){
               err = "cannot use array this way";
               return false;
            }
                                 //expression evaluated, just copy
            if(ct_r != this)
               codeAddCode(ct_r);
                              
            switch(ct_r->SizeOf()){
            case 1: code.push_back(MOV_AL_TAB_POP); break;
            case 4:
               if(ct_r->GetType()==TS_STRING){
                  /*
                  int max_str_size = ct_r->ListSize() ? ct_r->ListMember(0)->GetValue() : 0;
                  code.push_back(MOV_EAX);
                  StoreDWord(code, max_str_size);
                  code.push_back(PUSH);
                  */
                  code.push_back(MOV_EAX_TABS_POP);
               }else{
                  code.push_back(MOV_EAX_TAB_POP);
               }
               break;
            default:
               err = "unsupported table type for this operation";
               return false;
            }
         }else{
            switch(ct_r->SizeOf()){
            case 1: code.push_back(MOV_AL_TAB); break;
            case 4: code.push_back(byte((ct_r->GetType()==TS_STRING) ? MOV_EAX_TABS : MOV_EAX_TAB)); break;
            default:
               err = "unsupported table type for this operation";
               return false;
            }
         }
         code.push_back(byte(ct_r->GetAddress()));
      }else{
         if(to_type==TS_STRING){
            err = "not a string";
            return false;
         }
         if(ct_r->GetTypeFlags()&TYPE_EVAL){
                                 //expression evaluated, just copy
            if(ct_r != this)
               codeAddCode(ct_r);
         }else{
            bool is_local = (ct_r->type_flags&TYPE_LOCAL);
            bool is_param = (ct_r->type_flags&TYPE_PARAM);
            if(from_type == TS_STRING){
               code.push_back(MOV_EAX);
               reloc.push_back(S_symbol_info());
               reloc.back().Setup(SI_DATAOFFSET, NULL, code.size());
               StoreDWord(code, ct_r->GetAddress());  //address - will be relocated
            }else{
               if((ct_r->GetTypeFlags()&TYPE_ARRAY) && !(is_local|is_param)){
                                    //pick pointer (relocated)
                  code.push_back(MOV_EAX);
               }else{
                  switch(ct_r->SizeOf()){
                  case 1:
                     code.push_back(byte((is_local|is_param) ? MOV_AL_L : MOV_AL_G));
                     break;
                  case 2:
                     code.push_back(byte((is_local|is_param) ? MOV_AX_L : MOV_AX_G));
                     break;
                  case 4:
                     code.push_back(byte((is_local|is_param) ? MOV_EAX_L : MOV_EAX_G));
                     break;
                  default:
                     return false;
                  }
               }
               if(is_local){  //local relocation
                  reloc.push_back(S_symbol_info());
                  reloc.back().Setup(SI_AUTOVAR, ct_r->GetName(), code.size());
                  //StoreWord(code, 0);  //address - will be relocated
                  StoreWord(code, ct_r->GetAddress());
               }else
               if(is_param){     //parameter - address known
                  StoreWord(code, ct_r->GetAddress());
               }else{            //store relocation info

                  E_SYMBOL_TYPE st = SI_LOCAL;
                  if(ct_r->GetTypeFlags()&TYPE_EXTERN)
                     st = SI_EXTERNAL;
                  reloc.push_back(S_symbol_info(st, ct_r->GetName(), code.size()));
                              //address - will be relocated
                  StoreDWord(code, ct_r->GetAddress());
               }
            }
         }
      }
                           //perform conversion of return value
      switch(to_type){
      case TS_CHAR: case TS_BYTE:
      case TS_BOOL:
         if(from_type==TS_FLOAT)
            code.push_back(MOV_EAX_FLT);
         return true;
      case TS_SHORT: case TS_ENUM_CONSTANT: case TS_ENUM_TYPE:
         switch(from_type){
         case TS_CHAR:
            code.push_back(MOVSX_EAX_AL);
            break;
         case TS_BYTE:
         case TS_BOOL:
            code.push_back(MOVZX_EAX_AL);
            break;
         case TS_FLOAT:
            code.push_back(MOV_EAX_FLT);
            break;
         }
         return true;
      case TS_INT:
      case TS_DWORD:
         switch(from_type){
         case TS_CHAR:
            code.push_back(MOVSX_EAX_AL);
            break;
         case TS_BYTE:
         case TS_BOOL:
            code.push_back(MOVZX_EAX_AL);
            break;
         case TS_SHORT: case TS_ENUM_CONSTANT: case TS_ENUM_TYPE:
            code.push_back(MOVSX_EAX_AX);
            break;
         case TS_WORD:
            code.push_back(MOVZX_EAX_AX);
            break;
         case TS_FLOAT:
            {
               OPCODE oc;
               switch(to_type){
               case TS_INT:
               case TS_DWORD:
                  oc = MOV_EAX_FLT; break;
               default: assert(0); oc = BRK;
               }
               code.push_back((byte)oc);
            }
            break;
         }
         return true;
      case TS_FLOAT:
         switch(from_type){
         case TS_CHAR:
            code.push_back(MOVSX_EAX_AL);
            code.push_back(MOV_FLT_EAX);
            break;
         case TS_BYTE:
         case TS_BOOL:
            code.push_back(MOVZX_EAX_AL);
            code.push_back(MOV_FLT_EAX);
            break;
         case TS_SHORT: case TS_ENUM_CONSTANT:
            code.push_back(MOVSX_EAX_AX);
            code.push_back(MOVS_FLT_EAX);
            break;
         case TS_INT:
            code.push_back(MOVS_FLT_EAX);
            break;
         case TS_DWORD:
            code.push_back(MOV_FLT_EAX);
            break;
         case TS_WORD:
            code.push_back(MOVZX_EAX_AX);
            code.push_back(MOV_FLT_EAX);
            break;
         }
         return true;

      case TS_STRING:
         break;

      default: assert(0);
      }
      return true;
   }
}

//----------------------------

bool C_c_type_imp::codeStore(const C_c_type &ct, C_str &str){

   if(ct->GetType() == TS_FUNC){
      str = "left operand cannot be a function";
      return false;
   }
   /*
   if(type_flags&TYPE_REF){
                           //save value
      //code.push_back(MOV_EBX_EAX);
      code.push_back(PUSH);
                           //reference store - eax contains ptr to referenced value
      codeAddCode(ct);
      switch(ct->SizeOf()){
      case 1: code.push_back(MOV_PTR_STPB); break;
      case 2: code.push_back(MOV_PTR_STPW); break;
      case 4: code.push_back(MOV_PTR_STPD); break;
      default: return false;
      }
      return true;
   }
   */
   bool a = (ct->type_flags&TYPE_LOCAL);
   bool p = (ct->type_flags&TYPE_PARAM);
   switch(ct->SizeOf()){
   case 1: code.push_back(byte((a||p) ? MOV_L_AL : MOV_G_AL)); break;
   case 2: code.push_back(byte((a||p) ? MOV_L_AX : MOV_G_AX)); break;
   case 4: code.push_back(byte((a||p) ? MOV_L_EAX : MOV_G_EAX)); break;
   default: return false;
   }
   if(a){                     //local relocation
      reloc.push_back(S_symbol_info());
      reloc.back().Setup(SI_AUTOVAR, ct->GetName(), code.size());
                              //address - will be relocated
      StoreWord(code, ct->GetAddress());
   }else
   if(p){                     //parameter - address known
      StoreWord(code, ct->GetAddress());
   }else{                     //store relocation info
      E_SYMBOL_TYPE st = SI_LOCAL;
      if(ct->GetTypeFlags()&TYPE_EXTERN)
         st = SI_EXTERNAL;
      reloc.push_back(S_symbol_info(st, ct->GetName(), code.size()));
                              //address - will be relocated
      StoreDWord(code, ct->GetAddress());
   }
   return true;
}

//----------------------------

bool C_c_type_imp::codeAssign(const C_c_type &l, dword op, const C_c_type &r, C_str &err, bool const_check){

   if(const_check && (l->GetTypeFlags()&(TYPE_CONST|TYPE_EVAL))){
      err = ERR_NOLVALUE;
      return false;
   }
   if(l->GetTypeFlags()&TYPE_TABLE){
      err = "left value is member of table";
      return false;
   }

   E_TYPE_SPECIFIER ptype = l->GetPromoteType(r, err, true);
   if(ptype==TS_NULL)
      return false;

   if(!codePickParam(r, ptype, err))
      return false;

                              //we'll have type and value of promoted value
   type = l->GetType();
   type_flags = l->GetTypeFlags();

   if(op != '='){
      if(ptype==TS_STRING){
                              //can't do anything on strings, except of assignments
         err = "cannot perform assignment to string";
         return false;
      }

      code.push_back(PUSH);
      if(!codePickParam(l, ptype, err))
         return false;
      switch(op){
      case MUL_ASSIGN:
         switch(ptype){
         case TS_INT: code.push_back(IMUL_EAX_STP); break;
         case TS_DWORD: code.push_back(MUL_EAX_STP); break;
         case TS_FLOAT: code.push_back(FMUL_EAX_STP); break;
         default: assert(0);
         }
         break;
      case DIV_ASSIGN:
         switch(ptype){
         case TS_INT: code.push_back(IDIV_EAX_STP); break;
         case TS_DWORD: code.push_back(DIV_EAX_STP); break;
         case TS_FLOAT: code.push_back(FDIV_EAX_STP); break;
         default: assert(0);
         }
         break;
      case MOD_ASSIGN:
         switch(ptype){
         case TS_INT: code.push_back(IMOD_EAX_STP); break;
         case TS_DWORD: code.push_back(MOD_EAX_STP); break;
         case TS_FLOAT: return false;
         default: assert(0);
         }
         break;
      case ADD_ASSIGN:
         switch(ptype){
         case TS_INT:
         case TS_DWORD: code.push_back(ADD_POP); break;
         case TS_FLOAT: code.push_back(ADDF_EAX_STP); break;
         default: assert(0);
         }
         break;
      case SUB_ASSIGN:
         switch(ptype){
         case TS_INT:
         case TS_DWORD: code.push_back(SUB_POP); break;
         case TS_FLOAT: code.push_back(SUBF_EAX_STP); break;
         default: assert(0);
         }
         break;
      case LEFT_ASSIGN:
         if(ptype==TS_FLOAT)
            return false;
         code.push_back(SHL_EAX_STP);
         break;
      case RIGHT_ASSIGN:
         switch(ptype){
         case TS_INT: code.push_back(SAR_EAX_STP); break;
         case TS_DWORD: code.push_back(SHR_EAX_STP); break;
         case TS_FLOAT: return false;
         default: assert(0);
         }
         break;
      case AND_ASSIGN:
         if(ptype==TS_FLOAT)
            return false;
         code.push_back(AND_EAX_STP);
         break;
      case XOR_ASSIGN:
         if(ptype==TS_FLOAT)
            return false;
         code.push_back(XOR_EAX_STP);
         break;
      case OR_ASSIGN:
         if(ptype==TS_FLOAT) return false;
         code.push_back(OR_EAX_STP); break;
      default: return false;
      }
                              //constness is lost if value is changed
      type_flags &= ~TYPE_CONST;
   }else{
      if(type_flags&TYPE_CONST){
                              //during const assignment, we get value of right operand
         SetValue(r->GetValue());
      }
   }
   if(!codeStore(l, err))
      return false;
   
   type_flags |= TYPE_EVAL;
   return true;
}

//----------------------------

static const char *type_names[] = {
   "NULL", "void", "char", "byte",
   "short", "word", "int", "dword",
   "float", "bool", "literal", "string",
   "enum", "enum", "struct", "func"
};

//----------------------------

C_str C_c_type_imp::GetFunctionDeclaration(const C_c_type &params) const{

   C_fstr ret("%s %s(", type_names[type], (const char*)name);
   for(dword i=0; i<params->ListSize(); i++){
      const C_c_type &param = params->ListMember(i);
      ret += C_fstr("%s%s %s", i ? ", " : "", type_names[param->type], (const char*)param->name);
   }
   ret += ")";
   return ret;
}

//----------------------------

bool C_c_type_imp::MakeFunctionCall(const C_c_type &ct, const C_vector<C_c_type> *args, const C_vector<C_c_type> *enum_args,
   C_str &err, C_vector<byte> &tmp_code){

                           //no function
   if(ct->GetType() != TS_FUNC){
      err = ERR_NOFUNC;
      return false;
   }
   const C_c_type &ret_type = ct->GetReturnType();
   CopyType(ret_type);
   SetName(ct->GetName());
   type_flags &= ~TYPE_CONST;

   if(enum_args){
      C_vector<bool> param_used(enum_args->size(), false);
      C_c_type def_value = new C_c_type_imp; def_value->Release();

      for(int i=ct->ListSize(); i--; ){
         const C_c_type &param = ct->ListMember(i);
         C_c_type init;
                              //check if this parm is found in initializer list
         for(int j=enum_args->size(); j--; ){
            if((*enum_args)[j]->GetName() == param->GetName()){
               init = (*enum_args)[j]->ListMember(0);
               param_used[j] = true;
               break;
            }
         }
         if(!init){
                              //try use default parameter
            if(param->GetTypeFlags()&TYPE_PARAM_DEF){
               init = def_value;
               init->CopyType(param);
               init->AddTypeFlags(TYPE_CONST);
               init->SetValue(param->GetValue());
               init->SetParentType(param->GetParentType());
            }else{
               err = C_fstr("function call missing parameter '%s %s'",
                  type_names[param->GetType()], (const char*)param->GetName());
               return false;
            }
         }
         if(!codePickParam(init, param->GetType(), err, &tmp_code, param)){
            err = C_fstr("cannot convert parameter '%s %s' %s",
               type_names[param->GetType()], (const char*)param->GetName(),
               (err.Size() ? (const char*)C_fstr("(%s)", (const char*)err) : ""));
            return false;
         }
         code.push_back(PUSH);
      }
      for(i=param_used.size(); i--; ){
         if(!param_used[i]){
            err = C_fstr("parameter '%s' is not declared for function '%s'",
               (const char*)(*enum_args)[i]->GetName(), (const char*)GetName());
            return false;
         }
      }
   }else
   //if(args)
   {
                              //parameters specified as contiguous argument list
      int num_pars = args ? args->size() : 0;
      int req_pars = ct->ListSize();
      if(num_pars > req_pars){
         err = C_fstr("too many parameters into function call (expecting %i)", req_pars);
         return false;
      }
      C_c_type def_value = new C_c_type_imp; def_value->Release();
                              //put params onto stack right to left
      while(req_pars--){
         bool pick_ok;
         const C_c_type &param = ct->ListMember(req_pars);
         bool use_default_param = (req_pars>=num_pars);
         if(!use_default_param)
            use_default_param = ((*args)[req_pars]->GetType() == TS_DEFAULT_VALUE);

         if(use_default_param){
            if(param->GetTypeFlags()&TYPE_PARAM_DEF){
               def_value->CopyType(param);
               def_value->AddTypeFlags(TYPE_CONST);
               def_value->SetValue(param->GetValue());
               pick_ok = codePickParam(def_value, param->GetType(), err, &tmp_code, param);
            }else{
               err = C_fstr("missing parameter %i (%s %s)", req_pars+1,
                  type_names[param->GetType()], (const char*)param->GetName());
               return false;
            }
         }else{
            pick_ok = codePickParam((*args)[req_pars], param->GetType(), err, NULL, param);
         }
         if(!pick_ok){
            err = C_fstr("cannot convert parameter %i %s (%s %s)", req_pars+1,
               (err.Size() ? (const char*)C_fstr("(%s)", (const char*)err) : ""),
               type_names[param->GetType()], (const char*)param->GetName());
            return false;
         }
         code.push_back(PUSH);
      }
      /*
   }else{
      int req_pars = ct->ListSize();
      if(req_pars){
         err = C_fstr("not enough parameters into function call (expecting %i)", req_pars);
         return false;
      }
      */
   }

   code.push_back(CALL);

   E_SYMBOL_TYPE st = SI_LOCAL;
   if(ct->GetTypeFlags()&TYPE_EXTERN)
      st = SI_EXTERNAL;

   reloc.push_back(S_symbol_info(st, ct->GetName(), code.size()));
   StoreDWord(code, 0);       //address - will be relocated
                              //move floats from FPU to eax
   if(type == TS_FLOAT)
      code.push_back(MOV_EAX_FPU);

                              //type is evaluated now
   type_flags |= TYPE_EVAL;
   return true;
}

//----------------------------

bool C_c_type_imp::codeIncDecOp(C_str &err, bool inc, bool pick, bool save_acc){

   if(IsConst()){
      err = ERR_CONSTMODIFY;
      return false;
   }
   if(pick)
      codePickParam(this, type, err);

   if(save_acc)
      code.push_back(PUSH);

   switch(type){
   case TS_FLOAT:
      {
         code.push_back(PUSH);
         code.push_back(MOV_EAX);
         float f = 1.0f;
         StoreDWord(code, *((dword*)&f));
         code.push_back(byte(inc ? ADDF_EAX_STP : SUBF_EAX_STP));
      }
      break;
   default:
      code.push_back(byte(inc ? INC_EAX : DEC_EAX));
   }
   if(!codeStore(this, err)){
      if(!err.Size())
         err = ERR_NOLVALUE;
      return false;
   }
   if(save_acc)
      code.push_back(POP);

   type_flags |= TYPE_EVAL;
   return true;
}

//----------------------------

bool C_c_type_imp::codeSubscript(const C_c_type &ct_i, C_str &err){

   if(!(type_flags&TYPE_ARRAY)){
      err = ERR_BADSUBSCR;
      return false;
   }
                           //evaluate index expression
   switch(ct_i->GetType()){
   case TS_INT: case TS_DWORD:
   case TS_CHAR: case TS_BYTE:
   case TS_SHORT: case TS_WORD:
   case TS_ENUM_CONSTANT: case TS_ENUM_TYPE:
      break;
   default:
      err = ERR_MUSTBEINT;
      return false;
   }
                              //get index as int
   if(!codePickParam(ct_i, TS_INT, err)){
      err = ERR_BADSUBSCR;
      return false;
   }
   code.push_back(PUSH);
   type_flags |= TYPE_EVAL;
   return true;
}

  /*
//----------------------------

bool C_c_type_imp::codeStructMember(C_str &str, const C_str &mname){

   if(type != TS_STRUCT){
      str = "expression for \'.\' must be a struct or union";
      return false;
   }
                              //unroll nested structs
   C_c_type *str_base = this;
   while(str_base->NumStructMembers()==1 && str_base->StructMember(0).IsStruct())
      str_base = &str_base->StructMember(0);
                           //find if member exists
   int i = str_base->NumStructMembers();
   const C_c_type *member;

   while(i--) if((member = &str_base->StructMember(i))->GetName()==mname) break;
   if(i==-1){
      char buf[128];
      sprintf(buf, ERR_NOSTRMEMBER, (const char *)mname, (const char*)str_base->GetName());
      str = buf;
      return false;
   }
                           //get pointer to struct
   if(!codeUnaryOp('&', str)) return false;
                           //add with index value
   code.push_back(ADD_EAX);
   StoreDWord(code, member->GetAddress());
                           //set reference into member type
   SetTypeSpec(member->GetTypeSpec() | TS_REF | TS_EVAL);
   SetQualifier(member->GetQualifier());
   return true;
}

//----------------------------

bool C_c_type_imp::codeStructMemberPtr(C_str &str, const C_str &mname){

   if(!IsPointer() || !PointedType()->IsStruct()){
      str="expression for \'->\' must be pointer to struct or union";
      return false;
   }
   const C_c_type *pointed=PointedType();
                           //find if member exists
   int i=pointed->NumStructMembers();
   const C_c_type *member;

   while(i--) if((member=&pointed->StructMember(i))->GetName()==mname) break;
   if(i==-1){
      char buf[128];
      sprintf(buf, ERR_NOSTRMEMBER, (const char *)mname, (const char*)pointed->GetName());
      str=buf;
      return false;
   }
                           //get pointer to struct
   if(!codePickParam(*this, GetTypeSpec(), str)) return false;
                           //add with index value
   code.push_back(ADD_EAX);
   StoreDWord(code, member->GetAddress());
                           //set reference into member type
   SetTypeSpec(member->GetTypeSpec() | TS_REF | TS_EVAL);
   SetQualifier(member->GetQualifier());
   return true;
}
*/

//----------------------------

bool C_c_type_imp::codeUnaryOp(dword op_type, C_str &err){

   if(!IsConst()){
      if(!codePickParam(this, GetType(), err))
         return false;
   }

   switch(op_type){
   case '-':
      switch(type){
      case TS_FLOAT:
         if(IsConst())
            SetFValue(-GetFValue());
         else
            code.push_back(NEGF);
         break;
      case TS_CHAR: case TS_SHORT: case TS_INT: case TS_ENUM_CONSTANT:
      case TS_BYTE: case TS_WORD: case TS_DWORD:
         if(IsConst())
            SetValue(-int(GetValue()));
         else
            code.push_back(NEG);
         break;
      default:
         return false;
      }
      break;

   case '~':
      switch(type){
      case TS_CHAR: case TS_SHORT: case TS_INT: case TS_ENUM_CONSTANT:
      case TS_BYTE: case TS_WORD: case TS_DWORD:
         if(IsConst())
            SetValue(~GetValue());
         else
            code.push_back(NOT);
         break;
      default:
         return false;
      }
      break;

   case '!':
      switch(type){
      case TS_FLOAT:
         if(IsConst())
            SetValue(!GetFValue());
         else
            code.push_back(SET_B_EAX);
         break;

      case TS_CHAR: case TS_SHORT: case TS_INT: case TS_ENUM_CONSTANT:
      case TS_BYTE: case TS_WORD: case TS_DWORD: case TS_BOOL:
         switch(SizeOf()){
         case 1:
            if(IsConst())
               SetValue(!(GetValue()&0xff));
            else
               code.push_back(SET_B_AL);
            break;
         case 2:
            if(IsConst())
               SetValue(!(GetValue()&0xffff));
            else
               code.push_back(SET_B_AX);
            break;
         case 4:
            if(IsConst())
               SetValue(!GetValue());
            else
               code.push_back(SET_B_EAX);
            break;
         default:
            return false;
         }
         break;

      case TS_STRING:
         code.push_back(MOV_AL_PTR);
         code.push_back(SET_B_AL);
         break;

      default:
         return false;
      }
      if(!IsConst()){
         code.push_back(NOTB);
         code.push_back(SET_EAX_B);
      }
      SetType(TS_INT);
      SetTypeFlags(0);
      break;
   default:
      err = C_fstr("invalid unary operator: '%c'", op_type);
      return false;
   }
   type_flags |= TYPE_EVAL;
   return true;
}

//----------------------------

E_TYPE_SPECIFIER C_c_type_imp::GetPromoteType(const C_c_type &t1, C_str &err, bool op_assign) const{

   E_TYPE_SPECIFIER tt1 = t1->GetType();

   if(type==TS_STRING || tt1==TS_STRING){
      if(type==tt1)
         return TS_STRING;
      err = C_fstr("mismatched promotion %s string", ((type==TS_STRING) ? "to" : "from"));
      return TS_NULL;
   }

   if(GetParentType() && GetParentType() != t1->GetParentType()){
      err = "type mismatch";
      return TS_NULL;
   }
   if(type==TS_ENUM_TYPE){
                              //since our parent types match, this must be enum constant
      if(tt1!=TS_ENUM_CONSTANT && tt1!=TS_ENUM_TYPE){
         err = ERR_ENUMASSIGN;
         return TS_NULL;
      }
      return TS_INT;
   }
                              //from now, allow only numeric types
   switch(type){
   case TS_CHAR: case TS_BYTE: case TS_SHORT: case TS_WORD: case TS_INT: case TS_DWORD: case TS_FLOAT:
   case TS_ENUM_CONSTANT: case TS_BOOL:
      break;
   default:
      err = ERR_MISMATCH;
      return TS_NULL;
   }
   switch(tt1){
   case TS_CHAR: case TS_BYTE: case TS_SHORT: case TS_WORD: case TS_INT: case TS_DWORD: case TS_FLOAT:
   case TS_ENUM_CONSTANT: case TS_BOOL:
      break;
   default:
      err = ERR_MISMATCH;
      return TS_NULL;
   }
                              //if assignment, we must promote to type of left operand (this)
   if(op_assign)
      return type;

   if(type==TS_FLOAT || tt1==TS_FLOAT) return TS_FLOAT;
   if(type==TS_DWORD || tt1==TS_DWORD) return TS_DWORD;
   return TS_INT;
};

//----------------------------

bool C_c_type_imp::codeAritmOp(const C_c_type &l, dword op, const C_c_type &r, C_str &err){

   E_TYPE_SPECIFIER ptype = l->GetPromoteType(r, err);
   if(ptype==TS_NULL)
      return false;
   SetType(ptype);

   if(l->IsConst() && r->IsConst()){
      type_flags |= TYPE_CONST;
      union{
         long l;
         unsigned long u;
         float f;
      }v1, v2;
      v1.u = l->GetPromoteVal(ptype);
      v2.u = r->GetPromoteVal(ptype);

      switch(ptype){
      case TS_FLOAT:
         switch(op){
         case '+': v1.f += v2.f; break;
         case '-': v1.f -= v2.f; break;
         case '*': v1.f *= v2.f; break;
         case '/':
            if((float)fabs(v2.f) < MRG_ZERO){
               err = ERR_ZERODIVIDE;
            }else{
               v1.f /= v2.f;
            }
            break;
         case '%': case '&': case '^': case '|':
         case LEFT_OP: case RIGHT_OP:
         default: return false;
         }
         break;
      case TS_INT:
         switch(op){
         case '+': v1.u += v2.u; break;
         case '-': v1.u -= v2.u; break;
         case '*': v1.u *= v2.u; break;
         case '/': v1.u /= v2.u; break;
         case '%': v1.u %= v2.u; break;
         case '&': v1.u &= v2.u; break;
         case '^': v1.u ^= v2.u; break;
         case '|': v1.u |= v2.u; break;
         case LEFT_OP: v1.u <<= v2.u; break;
         case RIGHT_OP: v1.u >>= v2.u; break;
         default: return false;
         }
         break;
      case TS_DWORD:
         switch(op){
         case '+': v1.l += v2.l; break;
         case '-': v1.l -= v2.l; break;
         case '*': v1.l *= v2.l; break;
         case '/': v1.l /= v2.l; break;
         case '%': v1.l %= v2.l; break;
         case '&': v1.l &= v2.l; break;
         case '^': v1.l ^= v2.l; break;
         case '|': v1.l |= v2.l; break;
         case LEFT_OP: v1.l <<= v2.l; break;
         case RIGHT_OP: v1.l >>= v2.l; break;
         default: return false;
         }
         break;
      }
      SetValue(v1.u);
      return (err.Size()==0);
   }
   if(!codePickParam(r, ptype, err))
      return false;
   code.push_back(PUSH);
   if(!codePickParam(l, ptype, err))
      return false;

   switch(op){
   case '+': code.push_back(byte((ptype==TS_FLOAT) ? ADDF_EAX_STP : ADD_POP)); break;
   case '-': code.push_back(byte((ptype==TS_FLOAT) ? SUBF_EAX_STP : SUB_POP)); break;
   case '*': code.push_back(byte((ptype==TS_FLOAT) ? FMUL_EAX_STP : (ptype==TS_INT) ? IMUL_EAX_STP : MUL_EAX_STP)); break;
   case '/':
      code.push_back(byte((ptype==TS_FLOAT) ? FDIV_EAX_STP : (ptype==TS_INT) ? IDIV_EAX_STP : DIV_EAX_STP));
      if(r->IsConst()){
         dword v = r->GetPromoteVal(TS_FLOAT);
         float f = *(float*)&v;
         if((float)fabs(f) < MRG_ZERO){
            err = ERR_ZERODIVIDE;
         }
      }
      break;
   case '%': if(ptype==TS_FLOAT) return false;
      code.push_back(byte((ptype==TS_INT) ? IMOD_EAX_STP : MOD_EAX_STP)); break;
   case '&': if(ptype==TS_FLOAT) return false;
      code.push_back(AND_EAX_STP); break;
   case '^': if(ptype==TS_FLOAT) return false;
      code.push_back(XOR_EAX_STP); break;
   case '|': if(ptype==TS_FLOAT) return false;
      code.push_back(OR_EAX_STP); break;
   case LEFT_OP: if(ptype==TS_FLOAT) return false;
      code.push_back(SHL_EAX_STP); break;
   case RIGHT_OP: if(ptype==TS_FLOAT) return false;
      code.push_back(byte((ptype==TS_INT) ? SAR_EAX_STP : SHR_EAX_STP)); break;
   default: return false;
   }
   type_flags = TYPE_EVAL;
   return (err.Size()==0);
}

//----------------------------

bool C_c_type_imp::codeBooleanOp(const C_c_type &l, dword op, const C_c_type &r, C_str &err){

   E_TYPE_SPECIFIER ptype;

   switch(op){
   case '<': case '>': case LE_OP: case GE_OP: case EQ_OP: case NE_OP:
      ptype = l->GetPromoteType(r, err);
      if(ptype==TS_NULL)
         return false;
      break;
   case AND_OP: case OR_OP:
      ptype = TS_NULL;
      break;
   default: assert(0); ptype = TS_NULL;
   }

   if(l->IsConst() && r->IsConst()){
      type_flags |= TYPE_CONST;
      union{
         long l;
         unsigned long u;
         float f;
      }v1, v2;
      v1.u = l->GetPromoteVal(ptype ? ptype : l->GetType());
      v2.u = r->GetPromoteVal(ptype ? ptype : r->GetType());
      if(!ptype)
         ptype = l->GetType();
      switch(ptype){
      case TS_FLOAT:
         switch(op){
         case '<':  v1.u=(v1.f <  v2.f); break;
         case '>':  v1.u=(v1.f >  v2.f); break;
         case LE_OP: v1.u=(v1.f <= v2.f); break;
         case GE_OP: v1.u=(v1.f >= v2.f); break;
         case EQ_OP: v1.u=(v1.f == v2.f); break;
         case NE_OP: v1.u=(v1.f != v2.f); break;
         case AND_OP: v1.u=(v1.f && v2.f); break;
         case OR_OP: v1.u=(v1.f || v2.f); break;
         default: return false;
         }
         break;
      case TS_INT:
         if(ptype == TS_DWORD){
            switch(op){
            case '<':  v1.u=(v1.u <  v2.u); break;
            case '>':  v1.u=(v1.u >  v2.u); break;
            case LE_OP: v1.u=(v1.u <= v2.u); break;
            case GE_OP: v1.u=(v1.u >= v2.u); break;
            case EQ_OP: v1.u=(v1.u == v2.u); break;
            case NE_OP: v1.u=(v1.u != v2.u); break;
            case AND_OP: v1.u=(v1.u && v2.u); break;
            case OR_OP: v1.u=(v1.u || v2.u); break;
            default: return false;
            }
         }else{
            switch(op){
            case '<':  v1.u=(v1.l <  v2.l); break;
            case '>':  v1.u=(v1.l >  v2.l); break;
            case LE_OP: v1.u=(v1.l <= v2.l); break;
            case GE_OP: v1.u=(v1.l >= v2.l); break;
            case EQ_OP: v1.u=(v1.l == v2.l); break;
            case NE_OP: v1.u=(v1.l != v2.l); break;
            case AND_OP: v1.u=(v1.l && v2.l); break;
            case OR_OP: v1.u=(v1.l || v2.l); break;
            default: return false;
            }
         }
         break;
      }
      SetValue(v1.u);
      return true;
   }
   if(!codePickParam(r, ptype ? ptype : l->GetType(), err))
      return false;
   code.push_back(PUSH);
   if(!codePickParam(l, ptype ? ptype : r->GetType(), err))
      return false;
   //return true;

   switch(op){
   case '<': code.push_back(byte((ptype==TS_FLOAT) ? LTF : (ptype==TS_INT) ? LTS : LT)); break;
   case '>': code.push_back(byte((ptype==TS_FLOAT) ? GTF : (ptype==TS_INT) ? GTS : GT)); break;
   case LE_OP: code.push_back(byte((ptype==TS_FLOAT) ? LTFE : (ptype==TS_INT) ? LTSE : LTE)); break;
   case GE_OP: code.push_back(byte((ptype==TS_FLOAT) ? GTFE : (ptype==TS_INT) ? GTSE : GTE)); break;
   case EQ_OP: code.push_back(byte(EQ_EAX_STP)); break;
   case NE_OP:
      code.push_back(EQ_EAX_STP);
      code.push_back(NOTB);
      break;
   case AND_OP: code.push_back(BAND_EAX_STP); break;
   case OR_OP: code.push_back(BOR_EAX_STP); break;
   default: return false;
   }
   code.push_back(SET_EAX_B);

   type = TS_INT;
   type_flags = TYPE_EVAL;
   return true;
}

//----------------------------

bool C_c_type_imp::codeSkipCond(bool skip_on_true, const C_c_type &ct, int skip_count, C_str &err){

   dword size = ct->SizeOf();
   if(!size){
      err = ERR_CANTBEVOID;
      return false;
   }
   E_TYPE_SPECIFIER type = ct->GetType();
   int orig_size = code.size();
                              //instantiate expression
   if(!codePickParam(ct, type, err)){
      if(!err.Size())
         err = "cannot expand expression";
      return false;
   }
                              //if skipping back (or here), assume
                              //expression size and (SETX JX rel16) opcodes
   if(skip_count<0)
      skip_count -= (code.size()-orig_size+4);

   if(type==TS_STRING){
      code.push_back(MOV_AL_PTR);
      size = 1;
   }

   switch(size){
   case 1: code.push_back(SET_B_AL); break;
   case 2: code.push_back(SET_B_AX); break;
   case 4: code.push_back(SET_B_EAX); break;
   }
   code.push_back(byte(skip_on_true ? JTRUE : JFALSE));
   StoreWord(code, (word)skip_count);

   return true;
}

//----------------------------

bool C_c_type_imp::codeBuildSwitchBranch(C_c_type &ct, dword expr_size, C_str &err){

                              //code strategy for switch:
                              //    <pick expression>
                              //    EQ_???, <case_1_value>
                              //    JE <case_1_label>
                              // ...
                              //    EQ_???, <case_n_value>
                              //    JE <case_n_label>
                              //    JMP <default_label | out>
                              // case_1_label:
                              //    <case_1_code>
                              //    JMP <out>
                              // ...
                              // case_n_label:
                              //    <case_n_code>
                              //    JMP <out>
                              // default_label:
                              //    <default_code>
                              // out:

                           //look for all 'case's
   const S_symbol_info *rt = ct->GetRelocs();
                           //count # of case relocations
   dword num_cases = 0;
   for(dword i=0; i<ct->GetRelocNum(); i++){
      if(rt[i].type == SI_CASE)
         ++num_cases;
   }

   for(i=0; i<ct->GetRelocNum(); ){
      if(rt[i].type == SI_CASE){
         --num_cases;
                              //compare with constant value
         switch(expr_size){
         case 1:
            code.push_back(EQ_AL);
            StoreByte(code, byte(rt[i].GetData(0)));
            break;
         case 2:
            code.push_back(EQ_AX);
            StoreWord(code, word(rt[i].GetData(0)));
            break;
         case 4:
            code.push_back(EQ_EAX);
            StoreDWord(code, rt[i].GetData(0));
            break;
         default:
            err = "invalid expression type for switch statement";
            return false;
         }
         code.push_back(JTRUE);
                     //store case_n_label offset
         StoreWord(code,
            word(rt[i].GetAddress() +
                     //reserve others EQ_???, <case_n_value>, jmp <case_n_label>
                     //                 1      +(1 | 2 | 4)   +1     +2
            (num_cases*(4+expr_size))+
                     //reserve JMP <default | out>
                     //         1      +2
            3));

         ct->RemoveReloc(i);
      }else{
         i++;
      }
   }
                           //find defalut statement info (if any)
   const S_symbol_info *def_ri=NULL;
   for(i=0; i<ct->GetRelocNum(); i++)
   if(rt[i].type == SI_DEFAULT){
                           //duplicate default reloc info found
      if(def_ri){
         err = "only one \'default\' per switch statement is allowed";
         return false;
      }
      def_ri=&rt[i];
   }
                           //skip to default or out of switch
   codeSkip(def_ri ? def_ri->GetAddress() : ct->GetCodeSize());
                           //remove default info
   if(def_ri)
      ct->RemoveReloc(def_ri-rt);

   return true;
}

//----------------------------

bool C_c_type_imp::dataInstantiate(C_vector<byte> &tmp_data,
   C_vector<S_symbol_info> &tmp_symbols, C_vector<S_symbol_info> &tmp_relocs, C_str &err){

   E_TYPE_SPECIFIER type_spec = GetType();
   if(type_spec == TS_FUNC){
                              //external function declaration
                              // make their type automatically extern
      type_flags |= TYPE_EXTERN;
   }else
   if(!IsConst() && !(type_flags&TYPE_EXTERN)){
                        //put to file symbol table
      tmp_symbols.push_back(S_symbol_info());
      if(type == TS_STRUCT){
         tmp_symbols.back().Setup(SI_LOCAL, GetName(), tmp_data.size(), SI_DATA);
         dword struct_size = NumStructMembers();

         const C_c_type &init = GetInitializer();
         dword init_members = !init ? 0 : init->ListSize();

         if(struct_size)   //if no size, dont bother
         if(init_members>struct_size){
            err = ERR_MANYINIT;
            return false;
         }else
         for(dword ii=0; ii<struct_size; ii++){
            const C_c_type &member = StructMember(ii);
            dword member_size = member->SizeOf();

            dword val = 0;
            if(ii<init_members){
               const C_c_type &cct = init->ListMember(ii);
               E_TYPE_SPECIFIER member_type = member->GetPromoteType(cct, err, true);
               if(member_type==TS_NULL){
                  err = C_fstr("cannot initialize member '%s' (%s)", (const char*)member->GetName(), (const char*)err);
                  return false;
               }
                        //init must be const
               if(cct->IsConst())
                  val = cct->GetPromoteVal(member_type);
               else{
                  err = ERR_MUSTBECONST;
                  return false;
               }
            }
                              //store data
            switch(member_size){
            case 1: StoreByte(tmp_data, (byte)val); break;
            case 2: StoreWord(tmp_data, (word)val); break;
            case 4: StoreDWord(tmp_data, val); break;
            }
         }
      }else{
                              //single variable
         tmp_symbols.back().Setup(SI_LOCAL, GetName(), tmp_data.size(), SI_DATA);
         dword val = 0;
         const C_c_type &init = GetInitializer();
         if(init){
                              //initializer found (must be const)
            if(!init->IsConst()){
               err = ERR_MUSTBECONST;
               return false;
            }
            val = init->GetPromoteVal(type);
         }
         switch(SizeOf()){
         case 0:
            err = C_str(GetName()+" "+ERR_VOIDVAR);
            return false;
         case 1: StoreByte(tmp_data, (byte)val); break;
         case 2: StoreWord(tmp_data, (word)val); break;
         case 4: StoreDWord(tmp_data, val); break;
         }
      }
   }else
   if(IsConst() && type==TS_STRUCT){
      err = "structure cannot be constant";
      return false;
   }
   return true;
}

//----------------------------

bool C_c_type_imp::InstantiateTableTemplate(const C_c_type &name, C_vector<byte> &templ,
   C_vector<S_symbol_info> &reloc, C_str &err){

                              //check if table already defined
   if(templ.size()){
      err = "only one table definition per file is allowed";
      return false;
   }
   C_table_template tt;
   memset(&tt, 0, sizeof(tt));
   tt.caption = (const char*)name->GetValue();
   templ.assign((byte*)&tt, ((byte*)&tt) + sizeof(tt));
   reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, offsetof(C_table_template, caption), SI_TEMPLATE));

   struct S_hlp{
      static bool Instantiate(C_vector<C_c_type> &members, C_vector<byte> &templ, C_vector<S_symbol_info> &reloc,
         int &tab_index, int array_len, C_str &err){

         for(dword i=0; i<members.size(); i++){
            C_c_type &member = members[i];
            E_TYPE_SPECIFIER ts = member->GetType();
            C_table_element te;
            memset(&te, 0, sizeof(te));
            const C_c_type &name = member->GetTableDesc();
            te.tab_index = tab_index;

            if(member->GetTypeFlags()&TYPE_ARRAY){
               if(array_len){
                  err = "table contains array of arrays";
                  return false;
               }
               C_table_element tea;
               memset(&tea, 0, sizeof(tea));
               tea.type = TE_ARRAY;
               tea.tab_index = tab_index;
               tea.array_size = member->GetValue();
               tea.caption = (const char*)name->GetValue();
               reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, templ.size() + offsetof(C_table_element, caption), SI_TEMPLATE));
               templ.insert(templ.end(), (byte*)&tea, ((byte*)&tea)+sizeof(tea));
            }else{
               if(array_len){
                  member->AddTypeFlags(TYPE_ARRAY);
                  member->SetValue(array_len);
               }

               te.caption = (const char*)name->GetValue();
               reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, templ.size() + offsetof(C_table_element, caption), SI_TEMPLATE));
            }

            if(ts==TS_NULL){
                              //this is a branch
               te.type = TE_BRANCH;
               te.branch_depth = member->ListSize();

               const C_c_type &init = member->GetInitializer();
               if(init){
                  te.branch_text = (const char*)init->GetValue();
                  reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, templ.size() + offsetof(C_table_element, branch_text), SI_TEMPLATE));
               }
                              //store help
               if(member->GetParentType()){
                  te.help_string = (const char*)member->GetParentType()->GetValue();
                  reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, templ.size() + offsetof(C_table_element, help_string), SI_TEMPLATE));
               }

               templ.insert(templ.end(), (byte*)&te, ((byte*)&te)+sizeof(te));
               if(!Instantiate(member->GetList(), templ, reloc, tab_index,
                  (member->GetTypeFlags()&TYPE_ARRAY) ? member->GetValue() : 0,
                  err))
                  return false;
            }else{
                              //we'll keep member table index in its 'address' variable.
               member->SetAddress((short)tab_index);
               switch(ts){
               case TS_INT: te.type = TE_INT; break;
               case TS_FLOAT: te.type = TE_FLOAT; break;
               case TS_BOOL: te.type = TE_BOOL; break;
               case TS_STRING: te.type = TE_STRING; break;
               default:
                  assert(0);
               }
               ++tab_index;
               const C_vector<C_c_type> &init = member->GetList();
               if(init.size()){
                  if(ts==TS_STRING){
                     int sz = init[0]->GetPromoteVal(TS_INT);
                     if(sz<=0){
                        err = C_fstr("invalid string size of '%s' (%i) - must be greater than 0", (const char*)member->GetName(), sz);
                        return false;
                     }
                     te.string_size = sz;
                  }else{
                     te.int_default = init[0]->GetPromoteVal(TS_FLOAT);
                              //initial values
                     if(init.size() >= 3){
                        te.int_min = init[1]->GetPromoteVal(TS_INT);
                        te.int_max = init[2]->GetPromoteVal(TS_INT);
                     }
                  }
                              //store help
                  if(init.size() >= 4){
                     te.help_string = (const char*)init[3]->GetValue();
                     reloc.push_back(S_symbol_info(SI_CODEOFFSET, NULL, templ.size() + offsetof(C_table_element, help_string), SI_TEMPLATE));
                  }
               }
               templ.insert(templ.end(), (byte*)&te, ((byte*)&te)+sizeof(te));
            }
         }
         return true;
      }
   };
   int tab_index = 0;
   bool ret = S_hlp::Instantiate(list, templ, reloc, tab_index, 0, err);

                              //finalize template by TE_NULL member
   C_table_element te;
   memset(&te, 0, sizeof(te));
   templ.insert(templ.end(), (byte*)&te, ((byte*)&te)+sizeof(te));
   return ret;
}

//----------------------------
//----------------------------
                              //error messages
const char *err_msgs[]={
   "syntax error",
   "illegal combination of type specifiers",
   " is a variable of type \'void\'",
   "left expression must be a function or a function pointer",
   "left operand must be an \'lvalue\'",
   "cannot assign value of right operand to left operand",
   "expression is not meaningful",
   "attempt to modify a constant value",
   "expression must be constant",
   "expression must be integral",
   "expression cannot have \'void\' type",
   "type mismatch",
   "symbol \'%s\' already defined",
   "symbol \'%s\' has not been declared",
   "dimension cannot be negative or zero",
   "too many initializers",
   "expression must be \'pointer to ...\'",
   "subscript on non-array",
   "expression must be arithmetic",
   "member \'%s\' has not been declared in \'%s\'",
   "invalid storage class in file scope",
   "enumeration variable is not assigned a constant from its enumeration",
   "missing return value",
   "not expecting return value",
   "potential divide by zero",
};

//----------------------------

bool C_scope::AddIdentifier(const C_c_type &ct){

   const C_str &idn_name = ct->GetName();
   S_name_space &ns = name_space[GetLevel()];
                              //check if already exists
   t_type_map::const_iterator it = ns.identifiers.find(idn_name);
   if(it != ns.identifiers.end()){
                              //allow replacement of extern by local
      if(!((*it).second->GetTypeFlags()&TYPE_EXTERN))
         return false;
   }
   ns.identifiers[idn_name] = ct;
   return true;
}
//----------------------------

const C_c_type C_scope::FindIdentifier(const C_str &idn_name) const{

   dword l = GetLevel();
   do{
      const S_name_space &ns = name_space[l];
      t_type_map::const_iterator it = ns.identifiers.find(idn_name);
      if(it != ns.identifiers.end())
         return (*it).second;
   }while(l--);
   return NULL;
}

//----------------------------

bool C_scope::IsIdentifierInScope(const C_str &name){

#if 1
   return (FindIdentifier(name)!=NULL);
#else
   const S_name_space &ns = name_space[GetLevel()];
   t_type_map::const_iterator it = ns.identifiers.find(name);
   return (it!=ns.identifiers.end());
#endif
}

//----------------------------

bool C_scope::AddType(const C_c_type &ct){

   const C_str &type_name = ct->GetName();
   S_name_space &ns = name_space[GetLevel()];
                              //check if already exists
   t_type_map::const_iterator it = ns.typenames.find(type_name);
   if(it != ns.typenames.end())
      return false;
   ns.typenames[type_name] = ct;
   return true;
}

//----------------------------

/*
bool C_scope::AddFunctionPrototype(const C_c_type &ct){

   const C_str &type_name = ct->GetName();
   S_name_space &ns = name_space[GetLevel()];
                              //check if already exists
   t_type_map::const_iterator it = ns.func_prototypes.find(type_name);
   if(it != ns.func_prototypes.end())
      return false;
   ns.func_prototypes[type_name] = ct;
   return true;
}
*/

//----------------------------

const C_c_type C_scope::FindType(const C_str &type_name) const{

   dword l = GetLevel();
   do{
      const S_name_space &ns = name_space[l];
      t_type_map::const_iterator it = ns.typenames.find(type_name);
      if(it != ns.typenames.end())
         return (*it).second;
   }while(l--);
   return NULL;
}

//----------------------------

/*
const C_c_type C_scope::FindFunctionPrototype(const C_str &type_name) const{

   dword l = GetLevel();
   do{
      const S_name_space &ns = name_space[l];
      t_type_map::const_iterator it = ns.func_prototypes.find(type_name);
      if(it != ns.func_prototypes.end())
         return (*it).second;
   }while(l--);
   return NULL;
}
*/

//----------------------------

bool C_scope::IsTypeInScope(const C_str &name){

   return (FindType(name)!=NULL);
   /*
   const S_name_space &ns = name_space[GetLevel()];
   t_type_map::const_iterator it = ns.identifiers.find(name);
   return (it!=ns.identifiers.end());
   */
}

//----------------------------

/*
bool C_scope::TypeNameCollision(const C_c_type &ct){
   const S_name_space &ns=name_space[GetLevel()];
   for(int i=0; i<ns.typenames.size(); i++)
      if(ns.typenames[i]->GetName()==ct->GetName())
         return true;
   return false;
}
*/

//----------------------------

/*
void C_scope::AddGotoLabel(const C_str &label, const S_location &loc){
   S_goto_label gl;
   gl.Setup(label, loc);
   goto_labels.push_back(gl);
}

//----------------------------

const S_location *C_scope::FindGotoLabel(const C_str &label){
   for(int i=0; i<goto_labels.size(); i++)
      if(goto_labels[i].name==label) return &goto_labels[i].loc;
   return NULL;            //not found
}
*/

//----------------------------
//----------------------------

