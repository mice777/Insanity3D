#include "all.h"
#include "iscrpt_i.h"

#pragma warning(disable: 4725)//instruction may be inaccurate on some Pentiums


//----------------------------
                              //find reloc symbol among internal symbols
inline const S_symbol_info *FindRelocInfo(const S_symbol_info *rs, const S_symbol_info *rd){

   for(;;){
      if(rs->IsLast())
         return NULL;
      if(!strcmp(rs->sym_name, rd->sym_name))
         return rs;
      ++rs;
   }
}

//----------------------------
                              //find reloc symbol among external symbols
inline const VM_LOAD_SYMBOL *FindExtSymbol(const VM_LOAD_SYMBOL *syms, const S_symbol_info *rd){

   for(;;){
      if(!syms->address) return NULL;
                              //check if linked sybol info
      if(!syms->name){
         const VM_LOAD_SYMBOL *sym = FindExtSymbol((const VM_LOAD_SYMBOL*)syms->address, rd);
         if(sym) return sym;
      }else
      if(!strcmp(syms->name, rd->sym_name)){
         return syms;
      }
      ++syms;
   }
};

//----------------------------
//----------------------------

PC_v_machine CreateVM(){
   return new C_v_machine;
}

//----------------------------
//----------------------------

C_v_machine::C_v_machine():
   ref(1),
   user_data(0),
   do_save_context(0),
   code(NULL),
   data(NULL),
   load_ok(false)
{
}

//----------------------------

C_v_machine::~C_v_machine(){
   Unload();
}

//----------------------------

ISL_RESULT C_v_machine::Load(CPC_script scr1, const VM_LOAD_SYMBOL *ext_sym){

   Unload();
   if(!scr1)
      return ISLERR_NOPRG;
   script = const_cast<PC_script>(scr1);

   code_size = script->code.size();
   if(!script->comp_ok)
      return ISLERR_NOPRG;
                              //setup code
   code = new byte[code_size];
                              //must be alidned to dword!
   //assert(!((dword)code&3));
   memcpy(code, script->code.begin(), code_size);
                              //setup data
   dword size = script->data.size();
                              //must be alidned to dword!
   data = new byte[size];
   assert(!((dword)data&3));
   ISL_RESULT ir = ISLERR_GENERIC;

   bool fail = false;
   const S_symbol_info *rd = script->reloc_data;
   if(rd)
   for(; !rd->IsLast(); rd++){
      if(rd->flags&SI_TEMPLATE)
         continue;
                              //compute destination of relocation
      dword *dest = (dword*)((rd->flags&SI_DATA) ? data : code);
      (byte*&)dest += rd->address;

      switch(rd->type){
      case SI_DATAOFFSET: *dest += (dword)data; break;
      case SI_CODEOFFSET: *dest += (dword)code; break;

      case SI_EXTERNAL:
         {
            if(ext_sym){
               const VM_LOAD_SYMBOL *eri = FindExtSymbol(ext_sym, rd);
               if(eri){
                  *dest += (dword)eri->address;
                  break;
               }
            }
            ir = ISLERR_NOEXTSYM;
            goto link_fail;
         }
         break;

      case SI_LOCAL:
         {
            if(script->reloc_symbols){
               const S_symbol_info *ri = FindRelocInfo(script->reloc_symbols, rd);
               if(ri){
                                    //local relocation
                  dword src = (dword)(((byte*)((ri->flags&SI_DATA) ? data : code))+(dword)ri->address);
                  *dest += src;
                  break;
               }
            }
            ir = ISLERR_NOEXTSYM;
            goto link_fail;
         }
         break;

      default:
         assert(0);
         ir = ISLERR_LINKERR;
         goto link_fail;
      }
   }
   if(!fail){
      ReloadData();
                              //initialize table, if script contains template
      if(script->table_templ.size()){
         tab = CreateTable();
         tab->Release();
         tab->Load(script->table_templ.begin(), TABOPEN_TEMPLATE);
      }

      load_ok = true;
      return ISL_OK;
   }
link_fail:
   delete[] data; data = NULL;
   delete[] code; code = NULL;
   return ir;
}

//----------------------------

ISL_RESULT C_v_machine::ReloadData(){

   if(!script || !data)
      return ISLERR_NOPRG;
   memcpy(data, script->data.begin(), script->data.size());
   return ISL_OK;
}

//----------------------------

void *C_v_machine::GetAddress(const char *var_name) const{

   if(!load_ok)
      return NULL;
   if(script && script->reloc_symbols){
      for(S_symbol_info *si = script->reloc_symbols; !si->IsLast(); ++si)
         if(!strcmp(si->GetName(), var_name)) break;
      if(!si->IsLast()){
         bool is_data(si->flags&SI_DATA);
         void *addr = (byte*)(is_data ? data : code) + si->GetAddress();
         return addr;
      }
   }
   return NULL;
}

//----------------------------

ISL_RESULT C_v_machine::EnumSymbols(void (ISLAPI*cb_proc)(const char*, void*, bool, void*), void *cb_context) const{

   if(!load_ok)
      return ISLERR_NOPRG;
   if(script && script->reloc_symbols){
      for(S_symbol_info *si = script->reloc_symbols; !si->IsLast(); ++si){
         bool is_data(si->flags&SI_DATA);
         void *addr = (byte*)(is_data ? data : code) + si->GetAddress();
         (*cb_proc)(si->GetName(), addr, is_data, cb_context);
      }
   }
   return ISL_OK;
}

//----------------------------

void C_v_machine::Unload(){

   if(load_ok){
      delete[] data; data = NULL;
      delete[] code; code = NULL;
      tab = NULL;
      load_ok = false;
   }
}

//----------------------------

dword C_v_machine::FindUniqueSaveContextID() const{

                              //find unique id
   C_vector<dword> id_list;
   id_list.reserve(saved_context_list.size());
   for(dword i=saved_context_list.size(); i--; ){
      id_list.push_back(saved_context_list[i]->id);
   }
   sort(id_list.begin(), id_list.end());
   dword ret = 1;
   for(i=0; i<id_list.size(); i++){
      if(id_list[i] > ret)
         break;
      ret = id_list[i] + 1;
   }
   return ret;
}

//----------------------------

ISL_RESULT C_v_machine::SaveContext(dword *ret_id){

   if(do_save_context)
      return ISLERR_GENERIC;
   if(!ret_id)
      return ISLERR_INVPARAM;

   do_save_context = FindUniqueSaveContextID();
   *ret_id = do_save_context;

   return ISL_OK;
}

//----------------------------

ISL_RESULT C_v_machine::ClearSavedContext(dword id){

   if(!id){
      saved_context_list.clear();
      return ISL_OK;
   }
   for(int i=saved_context_list.size(); i--; ){
      if(saved_context_list[i]->id==id)
         break;
   }
   if(i==-1)
      return ISLERR_NOT_FOUND;
   saved_context_list[i] = saved_context_list.back(); saved_context_list.pop_back();
   return ISL_OK;
}

//----------------------------
static const int CONTEXT_SAVE_MAGIC_CODE = 0xcacb0001;

ISL_RESULT C_v_machine::GetSavedContext(dword id, C_buffer<byte> &buf) const{

   for(int i=saved_context_list.size(); i--; ){
      const S_saved_context *sc = saved_context_list[i];
      if(sc->id==id){
                              //create memory cache for the data
         byte *mem;
         dword sz;
         C_cache ck;
         ck.open(&mem, &sz, CACHE_WRITE_MEM, sc->stack.size()*4 + sc->params.size()*4 + sizeof(S_saved_context));
                              //write data
         ck.write(&CONTEXT_SAVE_MAGIC_CODE, sizeof(dword));
         ck.write(&sc->_eax, sizeof(dword));
         ck.write(&sc->_ebx, sizeof(dword));
         ck.write(&sc->stack_frame_size, sizeof(dword));
         ck.write(&sc->flag_bool, sizeof(byte));
         ck.write(&sc->next_instr, sizeof(dword));
         ck.write(&sc->num_fnc_params, sizeof(dword));
         {
            dword sz;
            sz = sc->stack.size();
            ck.write(&sz, sizeof(dword));
            ck.write(&sc->stack.front(), sz*sizeof(dword));

            sz = sc->params.size();
            ck.write(&sz, sizeof(dword));
            ck.write(&sc->params.front(), sz*sizeof(dword));
         }
         ck.close();
                              //copy info provided buffer
         buf.assign(mem, mem+sz);
         ck.FreeMem(mem);
         return ISL_OK;
      }
   }
   return ISLERR_NOT_FOUND;
}

//----------------------------

ISL_RESULT C_v_machine::SetSavedContext(dword *ret_id, const byte *buf, dword buf_size){

                              //open memory cache to access the data
   C_cache ck;
   if(!ck.open((byte**)&buf, &buf_size, CACHE_READ_MEM))
      return ISLERR_INVADDRESS;
                              //check magic code
   dword code = ck.ReadDword();
   if(code!=CONTEXT_SAVE_MAGIC_CODE)
      return ISLERR_INVPARAM;

                              //create new saved context
   S_saved_context *sc = new S_saved_context;

   sc->_eax = ck.ReadDword();
   sc->_ebx = ck.ReadDword();
   sc->stack_frame_size = ck.ReadDword();
   sc->flag_bool = ck.ReadByte();
   sc->next_instr = ck.ReadDword();
   sc->num_fnc_params = ck.ReadDword();
   {
      dword sz;
      sz = ck.ReadDword();
      sc->stack.resize(sz, 0);
      ck.read(&sc->stack.front(), sz*sizeof(dword));

      sz = ck.ReadDword();
      sc->params.resize(sz, 0);
      ck.read(&sc->params.front(), sz*sizeof(dword));
   }

   sc->id = FindUniqueSaveContextID();
   *ret_id = sc->id;

   saved_context_list.push_back(sc);
   sc->Release();

   ck.close();
   return ISL_OK;
}

//----------------------------

ISL_RESULT C_v_machine::GetGlobalVariables(C_buffer<byte> &buf) const{

   if(!script || !data)
      return ISLERR_NOPRG;
   buf.assign((const byte*)data, (const byte*)data + script->data.size());
   return ISL_OK;
}

//----------------------------

ISL_RESULT C_v_machine::SetGlobalVariables(const byte *buf, dword buf_size){

   if(!script || !data)
      return ISLERR_NOPRG;
   if(buf_size != script->data.size())
      return ISLERR_INVPARAM;
   memcpy(data, buf, buf_size);
   return ISL_OK;
}

//----------------------------

ISL_RESULT C_v_machine::SetBreakpoint(const char *file, dword line, bool on){

   if(!load_ok)
      return ISLERR_NOPRG;

   t_debug_info::const_iterator it_f = script->debug_info.find(file);
   if(it_f==script->debug_info.end())
      return ISLERR_NOT_FOUND;
   t_file_debug_info::const_iterator it_l = (*it_f).second.find(line);
   if(it_l==(*it_f).second.end())
      return ISLERR_NOT_FOUND;
   return ISLERR_NOT_FOUND;
}

//----------------------------

ISL_RESULT C_v_machine::Run(dword *retval, void *start, dword flags, dword num_args, ...){

   if(retval)
      *retval = 0;
   if(!load_ok)
      return ISLERR_NOPRG;
   ISL_RESULT ir = script->GetStatus();
   if(ISL_FAIL(ir))
      return ir;

                              //registers
   union{
      dword _32;
      word _16;
      byte _8;
   } _eax, _ebx;

   union U_tmp_ptr{
      dword *_32;
      word *_16;
      byte *_8;
   };
   const byte *instr_ptr;     //instruction pointer (absolute address)
   void *base_pointer, *save_esp;
   bool flag_bool;
   dword num_fnc_params = 0;

   _eax._32 = 0;

   dword stack_frame_size = 0;
   switch(flags&VMR_ADDRESS_MODE_MASK){
   case VMR_FNCNAME:          //find symbol
      {
         S_symbol_info *si = script->reloc_symbols;
         while(!si->IsLast()) if(!strcmp(si->GetName(), (const char*)start)) break; else ++si;
         if(si->IsLast()){
            return ISLERR_INVADDRESS;
         }
         instr_ptr = (byte*)code + si->GetAddress();
         assert(*instr_ptr == OP_SCRIPT_FUNC_CODE);
         ++instr_ptr;
         num_fnc_params = *((word*&)instr_ptr)++;
      }
      break;
   case VMR_ADDRESS:
      {
         instr_ptr = (byte*)start;
         if(*instr_ptr!=OP_SCRIPT_FUNC_CODE){
            assert(0);
            return ISLERR_INVADDRESS;
         }
         ++instr_ptr;
         num_fnc_params = *((word*&)instr_ptr)++;
      }
      break;
   case VMR_SAVEDCONTEXT:
      {
         if(!saved_context_list.size()){
            return ISLERR_INVADDRESS;
         }
         if(num_args){
            return ISLERR_INVPARAM;
         }
                              //find saved context
         for(int i=saved_context_list.size(); i--; )
            if(saved_context_list[i]->id==(dword)start)
               break;

         if(i==-1){
            return ISLERR_INVADDRESS;
         }

         const S_saved_context &sc = *(saved_context_list[i]);
         _eax._32 = sc._eax;
         _ebx._32 = sc._ebx;
         flag_bool = sc.flag_bool;
         instr_ptr = sc.next_instr + (byte*)code;
         num_fnc_params = sc.num_fnc_params;
         stack_frame_size = sc.stack_frame_size;

         __asm{
            mov save_esp, esp
            sub save_esp, 4
         }
         {
                              //push saved function params
            dword num = sc.params.size();
            if(num){
               const dword *pptr = &sc.params.front();
               __asm{
                  mov ebx, num
                  mov esi, pptr
               l1:
                  push [esi+ebx*4-4]
                  dec ebx
                  jnz l1
               }
            }
         }

         __asm push 0xffffffff   //push pseudo return address
         //__asm mov save_esp, esp
         {
                              //push saved stack
            dword num = sc.stack.size();
            if(num){
               const dword *sstk = &sc.stack.front();
               __asm{
                  mov ebx, num
                  mov esi, sstk
               l2:
                  push [esi+ebx*4-4]
                  dec ebx
                  jnz l2
               }
            }
         }
                              //remove the saved context from list
         saved_context_list[i] = saved_context_list.back(); saved_context_list.pop_back();
      }
      break;
   default:
      return ISLERR_INVADDRESS;
   }
   if(instr_ptr<(byte*)code || instr_ptr>=((byte*)code+code_size)){
      return ISLERR_INVADDRESS;
   }
                              
   dword vmr_ret_code = ISL_OK;
                              //run code

   if((flags&VMR_ADDRESS_MODE_MASK)!=VMR_SAVEDCONTEXT){
                              //push arguments on stack
      if(num_args){
         void *arg0 = &num_args + 1;
         __asm{
            mov ecx, num_args
            mov eax, arg0
            lea eax, [eax+ecx*4-4]
         ssl: 
            push dword ptr[eax]
            sub eax, 4
            dec ecx
            jnz ssl
         }
      }
      __asm push 0xffffffff   //push pseudo return address
      __asm mov save_esp, esp
   }
   __asm mov base_pointer, esp
   (byte* &)base_pointer += stack_frame_size;

   do_save_context = 0;

   dword instr_count = 0;
   const dword max_instr_count = 500;

   while(true){
      OPCODE op_code = (OPCODE)*instr_ptr++;
      switch(op_code){

      case BRK:               //invalid opcode
         vmr_ret_code = ISLERR_INVOPCODE;
         goto fini1;

      case ENTER:
         {
            dword frame_size = *((word* &)instr_ptr)++;
            __asm{
               push base_pointer
               sub [esp], esp
               mov base_pointer, esp
               sub esp, frame_size
            }
         }
         break;

      case LEAVE:
         __asm{
            mov esp, base_pointer
            add [esp], esp
            pop base_pointer
         }
         break;

      case MOV_AL:            //const8
         _eax._8 = *instr_ptr++;
         break;

      case MOV_AX:            //const16
         _eax._16 = *((word*&)instr_ptr)++;
         break;

      case MOV_EAX:           //const32
         _eax._32 = *((dword* &)instr_ptr)++;
         break;

      case MOV_AL_G:          //[address32]
         {
            U_tmp_ptr ptr;
            ptr._8 = *((byte** &)instr_ptr)++;
            _eax._8 = *ptr._8;
         }
         break;

      case MOV_AX_G:          //[address32]
         {
            U_tmp_ptr ptr;
            ptr._16 = *((word** &)instr_ptr)++;
            _eax._16 = *ptr._16;
         }
         break;

      case MOV_EAX_G:         //[address32]
         {
            U_tmp_ptr ptr;
            ptr._32 = *((dword**&)instr_ptr)++;
            _eax._32 = *ptr._32;
         }
         break;

      case MOV_AL_L:          //[rel16]
         {
            U_tmp_ptr ptr;
            ptr._8 = (byte*)base_pointer + *((short* &)instr_ptr)++;
            _eax._8 = *ptr._8;
         }
         break;

      case MOV_AX_L:          //[rel16]
         {
            U_tmp_ptr ptr;
            ptr._8 = (byte*)base_pointer + *((short* &)instr_ptr)++;
            _eax._16 = *ptr._16;
         }
         break;

      case MOV_EAX_L:         //[rel16]
         {
            U_tmp_ptr ptr;
            ptr._8 = (byte*)base_pointer + *((short* &)instr_ptr)++;
            _eax._32 = *ptr._32;
         }
         break;

      case MOV_G_AL:          //[address32]
         {
            U_tmp_ptr ptr;
            ptr._8 = *((byte** &)instr_ptr)++;
            *ptr._8 = _eax._8;
         }
         break;

      case MOV_G_AX:          //[address32]
         {
            U_tmp_ptr ptr;
            ptr._16 = *((word** &)instr_ptr)++;
            *ptr._16 = _eax._16;
         }
         break;

      case MOV_G_EAX:         //[address32]
         {
            U_tmp_ptr ptr;
            ptr._32 = *((dword** &)instr_ptr)++;
            *ptr._32 = _eax._32;
         }
         break;

      case MOV_L_AL:          //[rel16]
         {
            U_tmp_ptr ptr;
            ptr._8 = (byte*)base_pointer + *((short* &)instr_ptr)++;
            *ptr._8 = _eax._8;
         }
         break;

      case MOV_L_AX:          //[rel16]
         {
            U_tmp_ptr ptr;
            ptr._8 = (byte*)base_pointer + *((short* &)instr_ptr)++;
            *ptr._16 = _eax._16;
         }
         break;

      case MOV_L_EAX:         //[rel16]
         {
            U_tmp_ptr ptr;
            ptr._8 = (byte*)base_pointer + *((short* &)instr_ptr)++;
            *ptr._32 = _eax._32;
         }
         break;

      case MOV_EAX_FPU:       //st(0)
         __asm fstp _eax._32
         break;

      case MOV_FPU_EAX:
         __asm fld _eax._32
         break;

      case MOV_AL_PTR:
         _eax._8 = *(byte*)_eax._32;
         break;

      case NOP:
         break;

         /*
      case LEA:               //[rel16]
         _eax._32 = (dword)((byte*)base_pointer + *((short* &)ip)++);
         break;

      case MOV_AL_PTR:        //[eax]
         _eax._8 = *(byte*)_eax._32;
         break;

      case MOV_AX_PTR:        //[eax]
         _eax._16 = *(word*)_eax._32;
         break;

      case MOV_EAX_PTR:        //[eax]
         _eax._32 = *(dword*)_eax._32;
         break;

      case MOV_PTR_STPB:      //[eax]
         *(byte*)_eax._32 = _ebx._8;
         break;

      case MOV_PTR_STPW:      //[eax]
         *(word*)_eax._32 = _ebx._16;
         break;
         
      case MOV_PTR_STPD:      //[eax]
         *(dword*)_eax._32 = _ebx._32;
         break;
         */

      case MOVZX_EAX_AL:
         _eax._32 &= 0xff;
         break;

      case MOVZX_EAX_AX:
         _eax._32 &= 0xffff;
         break;

      case MOVSX_EAX_AL:
         (int&)_eax._32 = (schar&)_eax._8;
         break;

      case MOVSX_EAX_AX:
         (int&)_eax._32 = (short&)_eax._8;
         break;

      case MOV_EAX_FLT:
         __asm{
            fld _eax._32
            fistp _eax._32
         }
         break;

      case MOV_FLT_EAX:
      case MOVS_FLT_EAX:
         __asm{
            fild _eax._32
            fstp _eax._32
         }
         break;

      case INC_EAX:
         ++_eax._32;
         break;

      case DEC_EAX:
         --_eax._32;
         break;

      case ADD_EAX:           //imm32
         _eax._32 += *((dword* &)instr_ptr)++;
         break;

      case SUB_EAX:           //imm32
         _eax._32 -= *((dword* &)instr_ptr)++;
         break;

      case ADD_POP:
         __asm{
            pop eax
            add _eax._32, eax
         }
         break;

      case SUB_POP:
         __asm{
            pop eax
            sub _eax._32, eax
         }
         break;

      case ADDF_EAX_STP:
         __asm{
            fld _eax._32
            fadd dword ptr[esp]
            fstp _eax._32
            pop eax
         }
         break;

      case SUBF_EAX_STP:
         __asm{
            fld _eax._32
            fsub dword ptr[esp]
            fstp _eax._32
            pop eax
         }
         break;

      case MUL_EAX_STP:
         __asm{
            pop edx;
            mov eax, _eax._32
            mul edx
            mov _eax._32, eax
         }
         break;

      case IMUL_EAX_STP:
         __asm{
            pop edx;
            mov eax, _eax._32
            imul edx
            mov _eax._32, eax
         }
         break;

      case FMUL_EAX_STP:
         __asm{
            fld _eax._32
            fmul dword ptr[esp]
            fstp _eax._32
            pop eax
         }
         break;

      case IMUL_EAX:
         _eax._32 *= *((dword* &)instr_ptr)++;
         break;

      case DIV_EAX_STP:
         __asm{
            cmp dword ptr[esp], 0
            jne ok_div_2
            mov vmr_ret_code, ISLERR_DIVIDE_BY_ZERO
            jmp fini1
         ok_div_2:
            xor edx, edx
            mov eax, _eax._32
            div dword ptr[esp]
            pop edx
            mov _eax._32, eax
         }
         break;

      case IDIV_EAX_STP:
         __asm{
            cmp dword ptr[esp], 0
            jne ok_div_1
            mov vmr_ret_code, ISLERR_DIVIDE_BY_ZERO
            jmp fini1
         ok_div_1:
            mov eax, _eax._32
            mov edx, eax
            sar edx, 31
            idiv dword ptr[esp]
            pop edx
            mov _eax._32, eax
         }
         break;

      case FDIV_EAX_STP:
         __asm{
            mov eax, dword ptr[esp]
            and eax, 0x7fffffff
            cmp eax, MRG_ZERO_BITMASK
            jg ok_div_3
            mov vmr_ret_code, ISLERR_DIVIDE_BY_ZERO
            jmp fini1
         ok_div_3:
            fld _eax._32
            fdiv dword ptr[esp]
            fstp _eax._32
            pop eax
         }
         break;

      case MOD_EAX_STP:
         __asm{
            cmp dword ptr[esp], 0
            jne ok_div_4
            mov vmr_ret_code, ISLERR_DIVIDE_BY_ZERO
            jmp fini1
         ok_div_4:
            xor edx, edx
            mov eax, _eax._32
            div dword ptr[esp]
            mov _eax._32, edx
            pop edx
         }
         break;

      case IMOD_EAX_STP:
         __asm{
            cmp dword ptr[esp], 0
            jne ok_div_5
            mov vmr_ret_code, ISLERR_DIVIDE_BY_ZERO
            jmp fini1
         ok_div_5:
            mov eax, _eax._32
            mov edx, eax
            sar edx, 31
            idiv dword ptr[esp]
            mov _eax._32, edx
            pop edx
         }
         break;

      case AND_EAX_STP:
         __asm{
            pop edx
            and _eax._32, edx
         }
         break;

      case XOR_EAX_STP:
         __asm{
            pop edx
            xor _eax._32, edx
         }
         break;

      case OR_EAX_STP:
         __asm{
            pop edx
            or _eax._32, edx
         }
         break;

      case SHL_EAX:
         _eax._32 <<= *instr_ptr++;
         break;

      case SHL_EAX_STP:
         __asm{
            pop ecx
            shl _eax._32, cl
         }
         break;

      case SHR_EAX_STP:
         __asm{
            pop ecx
            shr _eax._32, cl
         }
         break;

      case SAR_EAX_STP:
         __asm{
            pop ecx
            sar _eax._32, cl
         }
         break;

      case NEG:
         _eax._32 = -int(_eax._32);
         break;

      case NEGF:
         _eax._32 ^= 0x80000000;
         break;

      case NOT:
         __asm not _eax._32
         break;

      case EQ_AL:
         flag_bool = (_eax._8 == *((byte* &)instr_ptr)++);
         break;

      case EQ_AX:
         flag_bool = (_eax._16 == *((word* &)instr_ptr)++);
         break;

      case EQ_EAX:
         flag_bool = (_eax._32 == *((dword* &)instr_ptr)++);
         break;

      case EQ_EAX_STP:
         __asm{
            pop edx
            cmp _eax._32, edx
            sete flag_bool
         }
         break;

      case SET_B_AL:
         flag_bool = _eax._8;
         break;

      case SET_B_AX:
         flag_bool = _eax._16;
         break;
         
      case SET_B_EAX:
         flag_bool = _eax._32;
         break;

      case SET_EAX_B:
         _eax._32 = flag_bool;
         break;

      case NOTB:
         flag_bool = !flag_bool;
         break;

      case BAND_EAX_STP:
         __asm{
            pop edx
            mov eax, _eax._32
            test eax, eax
            setnz ch
            test edx, edx
            setnz cl
            and ch, cl
            mov flag_bool, ch
         }
         break;

      case BOR_EAX_STP:
         __asm{
            pop edx
            mov eax, _eax._32
            test eax, eax
            setnz ch
            test edx, edx
            setnz cl
            or ch, cl
            mov flag_bool, ch
         }
         break;

      case LT:                //LESS THAN - unsigned
         __asm{
            pop edx
            cmp _eax._32, edx
            setb flag_bool
         }
         break;

      case LTS:               //LESS THAN - signed
      case LTF:               //LESS THAN - float
         __asm{
            pop edx
            cmp _eax._32, edx
            setl flag_bool
         }
         break;

      case LTE:               //LESS OR EQUAL - unsigned
         __asm{
            pop edx
            cmp _eax._32, edx
            setbe flag_bool
         }
         break;

      case LTSE:              //LESS OR EQUAL - signed
      case LTFE:              //LESS OR EQUAL - float
         __asm{
            pop edx
            cmp _eax._32, edx
            setle flag_bool
         }
         break;

      case GT:                //GREATER THAN - unsigned
         __asm{
            pop edx
            cmp _eax._32, edx
            seta flag_bool
         }
         break;

      case GTS:               //GREATER THAN - signed
      case GTF:               //GREATER THAN - float
         __asm{
            pop edx
            cmp _eax._32, edx
            setg flag_bool
         }
         break;

      case GTE:               //GREATER THAN - unsigned
         __asm{
            pop edx
            cmp _eax._32, edx
            setae flag_bool
         }
         break;

      case GTSE:              //GREATER OR EQUAL - signed
      case GTFE:              //GREATER OR EQUAL - float
         __asm{
            pop edx
            cmp _eax._32, edx
            setge flag_bool
         }
         break;

      case JMP:
         instr_ptr += *((short* &)instr_ptr)++;
         break;

      case JTRUE:
         {
            short rel = *((short* &)instr_ptr)++;
            if(flag_bool)
               instr_ptr += rel;
         }
         break;

      case JFALSE:
         {
            short rel = *((short* &)instr_ptr)++;
            if(!flag_bool)
               instr_ptr += rel;
         }
         break;

      case PUSH:
         __asm push _eax._32
         break;

      case POP:
         __asm pop _eax._32
         break;

      case CALL:
         {
            dword addr = *((dword* &)instr_ptr)++;
            if(*((byte*)addr) == OP_SCRIPT_FUNC_CODE){
                              //inside of script code
                              // push address relative to script base
               dword base_ptr = (dword)code;
               __asm{
                  push instr_ptr
                  mov eax, base_ptr
                  sub [esp], eax
               }
               instr_ptr = (byte*)addr;
               instr_ptr += 3;
            }else{
                              //real code
               __asm{
                  call addr
                  mov _eax._32, eax
               }
               if(do_save_context){
                  void *curr_esp;
                  dword stack_frame_size;
                  __asm{
                     mov curr_esp, esp
                     mov eax, base_pointer
                     mov stack_frame_size, eax
                     sub stack_frame_size, esp
                  }
                  saved_context_list.push_back(new S_saved_context());
                  S_saved_context &sc = *saved_context_list.back();
                  sc.Release();
                  sc._eax = _eax._32;
                  sc._ebx = _ebx._32;
                  sc.stack_frame_size = stack_frame_size;
                  sc.flag_bool = flag_bool;
                  sc.next_instr = instr_ptr - (byte*)code;
                  sc.num_fnc_params = num_fnc_params;
                              //save stack
                  dword stack_len = (byte*)save_esp - (byte*)curr_esp;
                  assert(!(stack_len%4));
                  sc.stack.resize(stack_len/4, 0);
                  memcpy(&sc.stack.front(), curr_esp, stack_len);
                              //save params
                  sc.params.resize(num_fnc_params, 0);
                  memcpy(&sc.params.front(), ((byte*)save_esp)+4, num_fnc_params*4);

                  sc.id = do_save_context;
                  do_save_context = 0;
                  vmr_ret_code = ISL_SUSPENDED;
                  goto fini1;
               }
            }
         }
         break;

      case RET:
         {
            dword ret_addr;
            __asm{
               mov eax, [esp]
               mov ret_addr, eax
            }
            if(ret_addr != 0xffffffff){
                              //back to script code
               __asm pop instr_ptr
               assert((dword)instr_ptr < code_size);
                              //relocate relative address back to absolute address
               instr_ptr += (dword)code;
            }else{
               goto fini;
            }
         }
         break;

      case RETN:
         {
            dword num = *(word*)instr_ptr;
            dword ret_addr;
            __asm{
               mov eax, [esp]
               mov ret_addr, eax
            }
            if(ret_addr != 0xffffffff){
                              //back to script code
               __asm{
                  pop instr_ptr
                  add esp, num
               }
               assert((dword)instr_ptr < code_size);
                              //relocate relative address back to absolute address
               instr_ptr += (dword)code;
            }else{
               goto fini;
            }
         }
         break;

      case MOV_AL_TAB:
         {
            int id = *((byte* &)instr_ptr)++;
            _eax._8 = tab->GetItemB(id);
         }
         break;

      case MOV_EAX_TAB:
         {
            int id = *((byte* &)instr_ptr)++;
            _eax._32 = *(dword*)tab->Item(id);
         }
         break;

      case MOV_EAX_TABS:
         {
            int id = *((byte* &)instr_ptr)++;
            _eax._32 = (dword)tab->ItemS(id);
         }
         break;

      case MOV_AL_TAB_POP:
         {
            dword indx;
            __asm pop indx
            int id = *((byte* &)instr_ptr)++;
            if(indx >= tab->ArrayLen(id)){
               vmr_ret_code = ISLERR_BAD_TABLE_INDEX;
               goto fini1;
            }
            _eax._8 = tab->ItemB(id, indx);
         }
         break;

      case MOV_EAX_TAB_POP:
         {
            dword indx;
            __asm pop indx
            int id = *((byte* &)instr_ptr)++;
            if(indx>=tab->ArrayLen(id)){
               vmr_ret_code = ISLERR_BAD_TABLE_INDEX;
               goto fini1;
            }
            _eax._32 = tab->ItemI(id, indx);
         }
         break;

      case MOV_EAX_TABS_POP:
         {
            dword indx;
            __asm pop indx
            int id = *((byte* &)instr_ptr)++;
            if(indx >= tab->ArrayLen(id)){
               vmr_ret_code = ISLERR_BAD_TABLE_INDEX;
               goto fini1;
            }
            _eax._32 = (dword)tab->ItemS(id, indx);
         }
         break;

      default: 
         vmr_ret_code = ISLERR_INVOPCODE;
         goto fini1;
      }
      if(++instr_count == max_instr_count){
         vmr_ret_code = ISLERR_NEVERENDING_LOOP;
         goto fini1;
      }
   }
fini:
   {
      void *_esp;
      __asm mov _esp, esp
      if(base_pointer!=_esp) vmr_ret_code = ISLERR_STACKCORRUPT;
   }
fini1:
   __asm{
      mov esp, save_esp
      add esp, 4              //pop pseudo return address
      mov eax, num_args
      lea esp, [esp+eax*4]
   }
   if(retval) *retval = _eax._32;
   return vmr_ret_code;
}

//----------------------------

