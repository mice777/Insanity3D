//----------------------------
// Opcodes for virtual machine
// Copyright (c) 1998 - 2002 Michal Bacik
//----------------------------

//----------------------------

enum OPCODE{
            //debug
   BRK,

            //compiler
   ENTER,                     //const16         make space on stack for local variables (param defines frame size)
   LEAVE,                     //                remove local space
            //memory
   MOV_AL,                    //const8          move immediate data into AL register
   MOV_AX,                    //const16         move immediate data into AX register
   MOV_EAX,                   //const32         move immediate data into EAX register
   MOV_AL_G,                  //[address32]     move data from 32-bit address into AL register
   MOV_AX_G,                  //[address32]     move data from 32-bit address into AX register
   MOV_EAX_G,                 //[address32]     move data from 32-bit address into eax register
   MOV_AL_L,                  //[rel16]         move data from relative 16-bit address into AL register
   MOV_AX_L,                  //[rel16]         move data from relative 16-bit address into AX register
   MOV_EAX_L,                 //[rel16]         move data from relative 16-bit address into EAX register
   MOV_G_AL,                  //[address32]     move data from AL register into 32-bit address
   MOV_G_AX,                  //[address32]     move data from AX register into 32-bit address
   MOV_G_EAX,                 //[address32]     move data from EAX register into 32-bit address
   MOV_L_AL,                  //[rel16]         move data from AL register into relative 16-bit address 
   MOV_L_AX,                  //[rel16]         move data from AX register into relative 16-bit address 
   MOV_L_EAX,                 //[rel16]         move data from EAX register into relative 16-bit address 
   MOV_EAX_FPU,               //ST(0)           move FPU top register into EAX register and pop FPU
   MOV_FPU_EAX,               //                load EAX register into FPU top register
   MOV_AL_PTR,                //[eax]           load al register from address pointed to by eax
   NOP,
   /*
   LEA,                       //[rel16]         load 16-bit relative address into EAX register
            //indirect memory access
   MOV_AL_PTR,                //[eax]  move data from 32-bit address pointed by EAX register into AL register
   MOV_AX_PTR,                //[eax]  move data from 32-bit address pointed by EAX register into AX register
   MOV_EAX_PTR,               //[eax]  move data from 32-bit address pointed by EAX register into EAX register
   MOV_PTR_STPB,              //[eax]  move data from BL register into 32-bit address pointed by EAX register
   MOV_PTR_STPW,              //[eax]  move data from BX register into 32-bit address pointed by EAX register
   MOV_PTR_STPD,              //[eax]  move data from EBX register into 32-bit address pointed by EAX register
   */
            //type conversion
   MOVZX_EAX_AL,              //       zero-extend top 24 bits of EAX register
   MOVZX_EAX_AX,              //       zero-extend top 16 bits of EAX register
   MOVSX_EAX_AL,              //       sign-extend top 24 bits of EAX register
   MOVSX_EAX_AX,              //       zero-extend top 16 bits of EAX register
   MOV_EAX_FLT,               //       convert value in register EAX from float into integer format
   MOV_FLT_EAX,               //       convert value in register EAX from integer into float format
   MOVS_FLT_EAX,
            //arithmetic
   INC_EAX,                   //          incremet EAX register by 1
   DEC_EAX,                   //          decremet EAX register by 1
   ADD_EAX,                   //const32   add immediate value to EAX register
   SUB_EAX,                   //const32   subtract immediate value from EAX register 

   ADD_POP,                   //          add value from top of stack to EAX register and pop stack
   SUB_POP,                   //          subtract value from top of stack from EAX register and pop stack
   ADDF_EAX_STP,              //          add float value from top of stack to EAX register and pop stack
   SUBF_EAX_STP,              //          subtract float value from top of stack from EAX register and pop stack
   MUL_EAX_STP,               //          multiply EAX register by unsigned value from top of stack and pop stack
   IMUL_EAX_STP,              //          multiply EAX register by signed value from top of stack and pop stack
   FMUL_EAX_STP,              //          multiply EAX register by float value from top of stack and pop stack
   IMUL_EAX,                  //const32   multiply EAX register by immediate unsigned value
   DIV_EAX_STP,               //          divide EAX register by unsigned value from top of stack and pop stack
   IDIV_EAX_STP,              //          divide EAX register by signed value from top of stack and pop stack
   FDIV_EAX_STP,              //          divide EAX register by float value from top of stack and pop stack
   MOD_EAX_STP,               //          get remainder after unsigned division from top of stack and pop stack
   IMOD_EAX_STP,              //          get remainder after signed division from top of stack and pop stack

            //bitwise
   AND_EAX_STP,               //          bitwise AND of register EAX and top of stack, pop stack
   XOR_EAX_STP,               //          bitwise XOR of register EAX and top of stack, pop stack
   OR_EAX_STP,                //          bitwise OR of register EAX and top of stack, pop stack
   SHL_EAX,                   //const8    shift register EAX left by immediate number of bits
   SHL_EAX_STP,               //          shift register EAX left by 32-bit value from stack and pop stack
   SHR_EAX_STP,               //          shift register EAX right by 32-bit value from stack and pop stack
   SAR_EAX_STP,               //          arithmetic shift register EAX left by 32-bit value from stack and pop stack

   NEG,                       //          negate register _eax
   NEGF,                      //          negate float value in register _eax
   NOT,                       //          
            //boolean operations
   EQ_AL,                     //const8    set flags if register AL equals to immediate data
   EQ_AX,                     //const16   set flags if register AX equals to immediate data
   EQ_EAX,                    //const32   set flags if register EAX equals to immediate data
   EQ_EAX_STP,                //          set flags if register AL equals to top of stack, pop stack

   SET_B_AL,                  //          set flags if register AL is non-zero
   SET_B_AX,                  //          set flags if register AX is non-zero
   SET_B_EAX,                 //          set flags if register EAX is non-zero
   SET_EAX_B,                 //          set register EAX to boolean state of flags
   NOTB,                      //          negate flags
   BAND_EAX_STP,              //a && b    set flags if both register EAX and top of stack evaluates to non-zero, and pop stack
   BOR_EAX_STP,               //a || b    set flags if either register EAX or top of stack evaluates to non-zero, and pop stack
                              //          compare register EAX with top of stack, pop stack
   LT,                        //          LESS THAN - unsigned
   LTS,                       //          LESS THAN - signed
   LTF,                       //          LESS THAN - float
   LTE,                       //          LESS OR EQUAL - unsigned
   LTSE,                      //          LESS OR EQUAL - signed
   LTFE,                      //          LESS OR EQUAL - float
   GT,                        //          GREATER THAN - unsigned
   GTS,                       //          GREATER THAN - signed
   GTF,                       //          GREATER THAN - float
   GTE,                       //          GREATER OR EQUAL - unsigned
   GTSE,                      //          GREATER OR EQUAL - signed
   GTFE,                      //          GREATER OR EQUAL - float
            //jump
                              //          add 16-bit signed value to current instruction pointer
   JMP,                       //[rel16]   always
   JTRUE,                     //[rel16]   if flags is set
   JFALSE,                    //[rel16]   if flags is clear
            //stack
   PUSH,                      //          push register EAX onto stack
   POP,                       //          push stack into register EAX 
   CALL,                      //[address32]  push current instruction pointer and set new 32-bit address 
   RET,                       //          pop instruction pointer from stack
   RETN,                      //const16   pop instruction pointer and remove n bytes from stack

            //tables
   MOV_AL_TAB,                //const8    get member of table, identified by its 8-bit index
   MOV_EAX_TAB,               //''
   MOV_EAX_TABS,              //''        get string from table
   MOV_AL_TAB_POP,            //''        get member of table, identified by its 8-bit index, indexed by stack top,
   MOV_EAX_TAB_POP,           //''
   MOV_EAX_TABS_POP,          //''

            //non-executable
   OP_STRING,                 //following is null-terminated string

   OP_SCRIPT_FUNC_CODE = 0xf4,//const16   special code marking that function is in script (mapping to CPU instruction "hlt"); param defines number of function parameters
};

//----------------------------