/*
 C language grammar for yacc (bison version).
 Actions created in 02/1998 by Michal Bacik
*/


%token IDENTIFIER CONSTANT STRING_LITERAL
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token XOR_ASSIGN OR_ASSIGN TYPE_NAME ELIPSIS
/*%token RANGE*/

%token TYPEDEF 
/*%token SIZEOF */
%token EXTERN
/*%token STATIC AUTO REGISTER*/
%token CHAR SHORT INT
/*%token LONG*/
/*%token SIGNED UNSIGNED*/
%token FLOAT
/*%token DOUBLE*/
%token CONST 
/*%token VOLATILE*/
%token VOID
%token STRUCT
/*%token UNION*/
%token ENUM

%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR
/*%token GOTO*/
%token CONTINUE BREAK RETURN

%token BYTE WORD DWORD STRING BOOL TRUE, FALSE, TABLE BRANCH
%token TOKEN_LAST

%start translation_unit

%%

%expect 1                     /* 1 conflict for if then*/

primary_expression
   : IDENTIFIER
   {
                              //find symbol in namespace
      const C_c_type &idn = yyscope.FindIdentifier($1->GetName());
      if(idn){
                              //found identifier by its name
         $$ = idn;
         if(idn->GetType()==TS_LITERAL)
            $$->SetType(TS_STRING);
      }else{
         ERROR(C_fstr(ERR_NOTDECLARED, (const char*)$1->GetName()));
         RESET_CURRENT
                              //try avoid futher error messages, create this identifier as int variable
         $$->SetType(TS_INT);
         $$->SetName($1->GetName());
         yyscope.AddIdentifier($$);
      }
   }
   | CONSTANT
   | TRUE
   {
      RESET_CURRENT;
      $$->SetType(TS_BOOL);
      $$->AddTypeFlags(TYPE_CONST);
      $$->SetValue(1);
   }
   | FALSE
   {
      RESET_CURRENT;
      $$->SetType(TS_BOOL);
      $$->AddTypeFlags(TYPE_CONST);
      $$->SetValue(0);
   }
   | STRING_LITERAL
   {
      //RESET_CURRENT
      $$ = $1;
      $$->SetType(TS_LITERAL);
   }
   | '(' expression ')'
   {
      $$ = $2;
   }
   ;

postfix_expression
   : primary_expression
   {
      $$ = $1;
      if($$->GetType()==TS_LITERAL){
                              //instantiate literal
         $$->SetTypeFlags(TYPE_CONST);

         const C_str &str = $1->GetName();
         C_compile_process::t_string_pool::const_iterator it_pool = compile_process.string_pool.find(str);
         dword offset;

         if(it_pool==compile_process.string_pool.end()){
                              //store this string to the segment
            C_vector<byte> &seg = tmp_code;
            seg.push_back(OP_STRING);
                                 //instantiate string in const segment
            offset = seg.size();
            if(str.Size()){
               seg.insert(seg.end(), (byte*)(const char*)str, ((byte*)(const char*)str)+str.Size()+1);
            }else
               seg.push_back(0);
                              //store into the pool
            compile_process.string_pool[str] = offset;
         }else{
                              //re-use existing string
            offset = it_pool->second;
         }
         $$->SetValue(offset);
                                 //clear name, no more needed
         $1->SetName(NULL);
      }
   }
   | postfix_expression '[' expression ']'
   {
                              //make copy, since this operation changes the $$
      RESET_CURRENT;
      $$->CopyType($1);
      if(!$$->codeSubscript($3, str)){
         ERROR(str);
      }
   }
   | postfix_expression '(' ')'
   {                          //function call without params
      RESET_CURRENT;
      if(!$$->MakeFunctionCall($1, NULL, NULL, str, tmp_code)){
         ERROR(str);
         MSG(C_fstr("   declaration: %s", (const char*)$$->GetFunctionDeclaration($1)));
      }
   }
   | postfix_expression '(' argument_expression_list ')'
   {                          //function call with params
      RESET_CURRENT;
      if(!$$->MakeFunctionCall($1, &$3->GetList(), NULL, str, tmp_code)){
         ERROR(str);
         MSG(C_fstr("   declaration: %s", (const char*)$$->GetFunctionDeclaration($1)));
      }
   }
   | postfix_expression '(' argument_enum_list ')'
   {                          //function call with params
      RESET_CURRENT;
      if(!$$->MakeFunctionCall($1, NULL, &$3->GetList(), str, tmp_code)){
         ERROR(str);
         MSG(C_fstr("   declaration: %s", (const char*)$$->GetFunctionDeclaration($1)));
      }
   }
   | postfix_expression '.' IDENTIFIER
   {
      RESET_CURRENT;
                              //find identifier in scructure
      const C_str &idn_name = $3->GetName();
      int i;
      for(i=$1->NumStructMembers(); i--; ){
         const C_c_type &m = $1->StructMember(i);
         if(m->GetName()==idn_name){
            $$->CopyType(m);
            $$->SetTypeFlags($1->GetTypeFlags());
            //$$->SetStructOffset(m->GetStructOffset());
            $$->SetAddress(m->GetAddress());
            break;
         }
      }
      if(i==-1){
         ERROR(C_fstr("'%s' is not member of '%s'", (const char*)idn_name, (const char*)$1->GetName()));  
         $$->SetType(TS_INT);
      }
      $$->SetName($1->GetName());
   }
   /*
   | postfix_expression PTR_OP IDENTIFIER
   */
   | postfix_expression INC_OP
   {
      RESET_CURRENT;
      $$->CopyType($1);
      $$->CopyName($1);
      if(!$$->codePickParam($$, $$->GetType(), str) ||
         !$$->codeIncDecOp(str, true, false, true)){
         ERROR(str);
      }
   }
   | postfix_expression DEC_OP
   {
      RESET_CURRENT;
      $$->CopyType($1);
      $$->CopyName($1);
      if(!$$->codePickParam($$, $$->GetType(), str) ||
         !$$->codeIncDecOp(str, false, false, true)){
         ERROR(str);
      }
   }
   ;

/* Argument(s) into function call.
*/
argument_expression_list
   : assignment_expression
   {
      RESET_CURRENT;
      $$->AddToList($1);
   }
   | argument_expression_list ',' assignment_expression 
   {
      $$ = $1;
      $$->AddToList($3);
   }
   ;

/* Argument(s) into function call, specified by enumeration.
*/
argument_enum_list
   : enum_argument
   {
      RESET_CURRENT;
      $$->AddToList($1);
   }
   | argument_enum_list ',' enum_argument
   {
      $$ = $1;
                              //check if not already in list
      for(int i=$$->ListSize(); i--; ){
         if($$->ListMember(i)->GetName() == $3->GetName()){
            ERROR(C_fstr("parameter '%s' already initialized", (const char*)$3->GetName()));
            break;
         }
      }
      $$->AddToList($3);
   }
   ;

/* Single argument for enumeration of parameters.
*/
enum_argument
   : IDENTIFIER ':' assignment_expression
   {
      $$ = $1;
      $$->AddToList($3);
   }
   ;

unary_expression
   : postfix_expression
   | INC_OP unary_expression
   {
      RESET_CURRENT;
      $$->CopyType($2);
      $$->CopyName($2);
      if(!$$->codeIncDecOp(str, true, true, false)){
         ERROR(str);
      }
   }
   | DEC_OP unary_expression
   {
      RESET_CURRENT;
      $$->CopyType($2);
      $$->CopyName($2);
      if(!$$->codeIncDecOp(str, false, true, false)){
         ERROR(str);
      }
   }
   | unary_operator cast_expression
   {
      RESET_CURRENT;
                              //must do full copy, because of instantiation of possible code
      $$->CopyType($2);
      $$->SetName($2->GetName());
      $$->SetValue($2->GetValue());
      //$$ = $2;
      if(!$$->codeUnaryOp($1->GetValue(), str))
         ERROR((str.Size() ? str : ERR_MUSTBEINT));
   }
   /*
   | SIZEOF unary_expression
   | SIZEOF '(' type_name ')'
   */
   ;

unary_operator
   :/* '&'
   | '*'
   |*/ '+'
   | '-'
   | '~'
   | '!'
   ;

cast_expression
   : unary_expression
   /*
   | '(' type_name ')' cast_expression
*/
   ;

multiplicative_expression
   : cast_expression
   | multiplicative_expression '*' cast_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '*', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | multiplicative_expression '/' cast_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '/', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | multiplicative_expression '%' cast_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '%', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

additive_expression
   : multiplicative_expression
   | additive_expression '+' multiplicative_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '+', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | additive_expression '-' multiplicative_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '-', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

shift_expression
   : additive_expression
   | shift_expression LEFT_OP additive_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, LEFT_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | shift_expression RIGHT_OP additive_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, RIGHT_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

relational_expression
   : shift_expression
   | relational_expression '<' shift_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, '<', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | relational_expression '>' shift_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, '>', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | relational_expression LE_OP shift_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, LE_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | relational_expression GE_OP shift_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, GE_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

equality_expression
   : relational_expression
   | equality_expression EQ_OP relational_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, EQ_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   | equality_expression NE_OP relational_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, NE_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

and_expression
   : equality_expression
   | and_expression '&' equality_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '&', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

exclusive_or_expression
   : and_expression
   | exclusive_or_expression '^' and_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '^', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

inclusive_or_expression
   : exclusive_or_expression
   | inclusive_or_expression '|' exclusive_or_expression
   {
      RESET_CURRENT;
      if(!$$->codeAritmOp($1, '|', $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

logical_and_expression
   : inclusive_or_expression
   | logical_and_expression AND_OP inclusive_or_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, AND_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

logical_or_expression
   : logical_and_expression
   | logical_or_expression OR_OP logical_and_expression
   {
      RESET_CURRENT;
      if(!$$->codeBooleanOp($1, OR_OP, $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_MUSTBEINT);
      }
   }
   ;

conditional_expression
   : logical_or_expression
   /*
   | logical_or_expression '?' expression ':' conditional_expression
   */
   ;

assignment_expression
   : conditional_expression
   | unary_expression assignment_operator assignment_expression
   {
      RESET_CURRENT;
      if(!$$->codeAssign($1, $2->GetValue(), $3, str)){
         ERROR(str.Size() ? (const char*)str : ERR_CANTASSIGN);
      }
   }
   ;

assignment_operator
   : '='
   | MUL_ASSIGN      /* '*=' */
   | DIV_ASSIGN      /* '/=' */
   | MOD_ASSIGN      /* '%=' */
   | ADD_ASSIGN      /* '+=' */
   | SUB_ASSIGN      /* '-=' */
   | LEFT_ASSIGN     /* '<<=' */
   | RIGHT_ASSIGN    /* '>>=' */
   | AND_ASSIGN      /* '&=' */
   | XOR_ASSIGN      /* '^=' */
   | OR_ASSIGN       /* '|=' */
   ;

expression
   : assignment_expression
   /*
   | expression ',' assignment_expression
   */
   ;

constant_expression
   : conditional_expression
   {
      $$ = $1;
      if(!$$->IsConst()){
         ERROR(ERR_MUSTBECONST);
      }
   }  
   ;

declaration
   : declaration_specifiers ';'
   {
      $$ = $1;
      if(!$$->GetName().Size()){
         if($$->GetType() != TS_ENUM_CONSTANT)
            WARNING("an anonymous type without a declarator is useless");
      }
   }
   | declaration_specifiers init_declarator_list ';'
   {
      RESET_CURRENT;

      for(dword i=0; i<$2->ListSize(); i++){
         C_c_type &ct = $2->ListMember(i);
         C_c_type init = ct->GetInitializer();
         if($1->IsConst()){
            if(init){
               if(init->IsConst()){
                              //setup direct constant value
                  ct->SetValue(init->GetPromoteVal($1->GetType()));
                  ct->SetInitializer(NULL);
               }
            }else{
               ct->SetValue(0);
            }
         }
         if(ct->GetType() == TS_FUNC){
                              //setup return type to be type of $1
            ct->SetReturnType($1);
            ct->SetTypeFlags($1->GetTypeFlags());
         }else{
            ct->CopyType($1);
            if(ct->GetType()==TS_STRING){
               ct->SetType(TS_LITERAL);
               ct->AddTypeFlags(TYPE_CONST);
               if(init){
                  ct->SetValue(init->GetValue());
               }else{
                              //instantiate empty string now
                  C_vector<byte> &seg = tmp_code;
                  ct->SetValue(seg.size());
                  seg.push_back(0);
               }
            }else
            if((ct->GetTypeFlags()&TYPE_EXTERN) && init){
               ERROR("cannot initialize external variable");
            }
         }
         if($1->GetTypeFlags()&TYPE_TYPEDEF){
            if(ct->GetTypeFlags()&TYPE_EXTERN){
               ERROR("typedef cannot be extern");
            }
                              //new type name, add to scope
            yyscope.AddType(ct);
         }else{
                              //add to declaration (through list)
            $$->AddToList(ct);
         }
      }
   }
   ;

declaration_specifiers
   : /*storage_class_specifier
   | */storage_class_specifier declaration_specifiers
   {
      $$ = $2;
      if($$->GetTypeFlags()&$1->GetTypeFlags()){
         ERROR("storage class already specified");
      }
      $$->AddTypeFlags($1->GetTypeFlags() & (TYPE_EXTERN | TYPE_TYPEDEF));
   }
   | type_specifier
   {
      $$ = $1;
      if($$->GetType() == TS_STRUCT){
                              //put named structs into current scope, if not yet
         if($$->GetName().Size()){
                              //if struct not declared in scope, do it now
            if(!yyscope.IsTypeInScope($$->GetName()))
               yyscope.AddType($$);
         }
      }
   }
   /*
   | type_specifier declaration_specifiers
   | type_qualifier
   */
   | type_qualifier declaration_specifiers
   {
      $$ = $2;
      if($$->GetTypeFlags()&$1->GetTypeFlags()){
         ERROR("qualifier already specified");
      }
      $$->AddTypeFlags($1->GetTypeFlags() & TYPE_CONST);
   }
   ;

init_declarator_list
   : init_declarator
   {
      RESET_CURRENT;
      $$->AddToList($1);
   }
   | init_declarator_list ',' init_declarator
   {
      $$ = $1;
      $$->AddToList($3);
   }
   ;

init_declarator
   : declarator
   | declarator '=' initializer
   {                          //set initializer as 1st list member
      $$ = $1;
      $$->SetInitializer($3);
   }
   ;

storage_class_specifier
   : TYPEDEF     
   { 
      $$ = $1;
      $$->AddTypeFlags(TYPE_TYPEDEF);
   }
   | EXTERN
   {
      $$ = $1;
      $$->AddTypeFlags(TYPE_EXTERN);
   }
   /*
   | STATIC
   | AUTO
   | REGISTER
   */
   ;

type_specifier
   : VOID         { $$ = $1; $$->SetType(TS_VOID); }
   | CHAR         { $$ = $1; $$->SetType(TS_CHAR); }
   | SHORT        { $$ = $1; $$->SetType(TS_SHORT); }
   | INT          { $$ = $1; $$->SetType(TS_INT); }
   | BYTE         { $$ = $1; $$->SetType(TS_BYTE); }
   | WORD         { $$ = $1; $$->SetType(TS_WORD); }
   | DWORD        { $$ = $1; $$->SetType(TS_DWORD); }
   | STRING       { $$ = $1; $$->SetType(TS_STRING); }
   | BOOL         { $$ = $1; $$->SetType(TS_BOOL); }
   /*
   | LONG
   */
   | FLOAT        { $$ = $1; $$->SetType(TS_FLOAT); }
   /*
   | DOUBLE
   | SIGNED
   | UNSIGNED
   */
   | struct_or_union_specifier { $$ = $1; $$->SetType(TS_STRUCT); }
   | enum_specifier
   | TYPE_NAME
   {
      RESET_CURRENT;
      $$->CopyType($1);
      $$->ClearTypeFlags(TYPE_TYPEDEF);
      $$->SetParentType($1);
   }
   ;

struct_or_union_specifier
   : struct_or_union IDENTIFIER '{' struct_declaration_list '}'
   {
      $$ = $1;
      $$->SetName($2->GetName());
      dword addr = 0;
      for(dword i=0; i<$4->ListSize(); i++){
         C_c_type &m = $4->ListMember(i);
         m->SetAddress((short)addr);
         $$->AddStructMember(m);
         addr += m->SizeOf();
      }
   }
   /*
   | struct_or_union '{' struct_declaration_list '}'
   | struct_or_union IDENTIFIER
   */
   ;

struct_or_union
   : STRUCT
   {
      RESET_CURRENT;
      $$->SetType(TS_STRUCT);
   }
   /*
   | UNION
   */
   ;

struct_declaration_list
   : struct_declaration
   {
      RESET_CURRENT;
      $$->CopyList($1);
   }
   | struct_declaration_list struct_declaration
   {
      $$ = $1;
      for(dword i=0; i<$2->ListSize(); i++){
         $$->AddToList($2->ListMember(i));
      }
   }
   ;

struct_declaration
   : /*specifier_qualifier_list struct_declarator_list ';'*/
     type_specifier struct_declarator_list ';'
   {
      RESET_CURRENT;
      for(dword i=0; i<$2->ListSize(); i++){
         C_c_type &ct = $2->ListMember(i);
         ct->CopyType($1);
         switch(ct->GetType()){
         case TS_STRING:
         case TS_STRUCT:
            ERROR("invalid structure member type");
            ct->SetType(TS_INT);
            break;
         }
         if($1->GetTypeFlags()&TYPE_TYPEDEF){
            assert(0);
         }else{
                              //add to declaration (through list)
            $$->AddToList(ct);
         }
      }
   }
   ;

   /*
specifier_qualifier_list
   : type_specifier specifier_qualifier_list
   | type_specifier
   | type_qualifier specifier_qualifier_list
   | type_qualifier
   ;
   */

struct_declarator_list
   : struct_declarator
   {
      RESET_CURRENT;
      $$->AddToList($1);
   }
   | struct_declarator_list ',' struct_declarator
   {
      $$ = $1;
      $$->AddToList($2);
   }
   ;

struct_declarator
   : declarator
   /*
   | ':' constant_expression
   | declarator ':' constant_expression
   */
   ;

enum_specifier
   : ENUM '{'
   {
      recent_enum_val = 0;
   }
    enumerator_list '}'
   {
      $$ = $4;
      $$->SetType(TS_ENUM_TYPE);
                              //set ourselves as parent type of all our enum constants
      for(int i=$$->ListSize(); i--; )
         $$->ListMember(i)->SetParentType($$);
                              //clear list now, we dont need that more
      $$->ClearList();
   }
   | ENUM IDENTIFIER '{'
   {
      recent_enum_val = 0;
   }
    enumerator_list '}'
   {
      $$ = $5;
      $$->SetName($2->GetName());
      $$->SetType(TS_ENUM_TYPE);
      yyscope.AddType($$);

                              //set ourselves as parent type of all our enum constants
      for(int i=$$->ListSize(); i--; )
         $$->ListMember(i)->SetParentType($$);
                              //clear list now, we dont need that more
      $$->ClearList();
   }
   /*
   | ENUM IDENTIFIER
   */
   ;

enumerator_list
   : enumerator
   {
      RESET_CURRENT;
      $$->AddToList($1);
   }
   | enumerator_list ',' enumerator
   {
      $$ = $1;
      $$->AddToList($3);
   }
   | enumerator_list ','
   {
      $$ = $1;
   }
   ;

enumerator
   : IDENTIFIER
   {
      $$ = $1;
      $$->SetValue(recent_enum_val++);
                              //add enum members to identifier namespace
      $$->SetType(TS_ENUM_CONSTANT);
      $$->SetTypeFlags(TYPE_CONST);
      yyscope.AddIdentifier($$);
   }
   | IDENTIFIER '=' constant_expression
   {
      $$ = $1;
      if(!($3->GetTypeFlags()&TYPE_CONST)){
         ERROR("right expression must be constant");
      }
      E_TYPE_SPECIFIER t = $3->GetPromoteType($3, str);
      switch(t){
      case TS_INT: case TS_DWORD:
         recent_enum_val = $3->GetPromoteVal(t);
         $$->SetValue(recent_enum_val++);
         break;
      default:
         ERROR(ERR_MUSTBEINT);
      }
                              //add enum members to identifier namespace
      $$->SetType(TS_ENUM_CONSTANT);
      $$->AddTypeFlags(TYPE_CONST);
      yyscope.AddIdentifier($$);
   }
   ;

type_qualifier
   : CONST 
   { 
      $$ = $1;
      $$->AddTypeFlags(TYPE_CONST);
   }
   /*
   | VOLATILE
   */
   ;

declarator
   : /*pointer direct_declarator
   | */direct_declarator
   ;

direct_declarator
   : IDENTIFIER
   /*
   | '(' declarator ')'
   */
/*
   | direct_declarator '[' constant_expression ']'
   | direct_declarator '[' ']'
   */
   | direct_declarator '(' parameter_type_list ')'
   {
      $$ = $1;
      $$->SetType(TS_FUNC);
      for(dword i=0; i<$3->ListSize(); i++)
         $$->AddToList($3->ListMember(i));
   }
/*
   | direct_declarator '(' identifier_list ')'
   */
   | direct_declarator '(' ')'
   {
                              //function without parameters
      $$ = $1;
      $$->SetType(TS_FUNC);
   }
   ;

   /*
pointer
   : '*'
   | '*' type_qualifier_list
   | '*' pointer
   | '*' type_qualifier_list pointer
   ;
*/


/*
type_qualifier_list
   : type_qualifier
   | type_qualifier_list type_qualifier
   ;
   */


parameter_type_list
   : parameter_list
   {
      $$=$1;
      i=$$->ListSize();        //get num_params
                              //can accept one argument of void type
      if(i==1 && $$->ListMember(0)->GetType()==TS_VOID){
         RESET_CURRENT
         i = 0;
      }
                              //check for void param
      while(i--){
         if($$->ListMember(i)->GetType()==TS_VOID)
            ERROR("function arguments cannot be of type \'void\'");
      }
   }
/*
   | parameter_list ',' ELIPSIS
   */
   ;

parameter_list
   : parameter_declaration
   {
      RESET_CURRENT
      $$->AddToList($1);
   }
   | parameter_list ',' parameter_declaration
   {
      $$ = $1; $$->AddToList($3);
   }
   ;

parameter_declaration
   : declaration_specifiers declarator
   {
      $$ = $2;
      $$->CopyType($1);
   }
   | declaration_specifiers declarator '=' unary_expression
   {
      $$ = $2;
      $$->CopyType($1);
      if($$->GetType()==TS_STRING || $4->GetType()==TS_LITERAL){
         if($$->GetType()!=TS_STRING || $1->GetType()!=TS_STRING){
            ERROR("invalid initialization type");
         }
         $$->SetParentType($4);
      }else{
         $$->SetValue($4->GetPromoteVal($$->GetType()));
      }
      $$->AddTypeFlags(TYPE_PARAM_DEF);
   }
   /*
   | declaration_specifiers declarator '=' STRING_LITERAL
   {
      $$ = $2;
      $$->CopyType($1);
      if($$->GetType()!=TS_STRING){
         ERROR("cannot initialize integer by string");
      }
      $$->AddTypeFlags(TYPE_PARAM_DEF);
      
      //$$->SetValue($4->GetPromoteVal($$->GetType()));
   }
   */
/*
   |  declaration_specifiers abstract_declarator
   */
   | declaration_specifiers
   ;

   /*
identifier_list
   : IDENTIFIER
   | identifier_list ',' IDENTIFIER
   ;

type_name
   : specifier_qualifier_list
   | specifier_qualifier_list abstract_declarator
   ;

abstract_declarator
   : pointer
   | direct_abstract_declarator
   | pointer direct_abstract_declarator
   ;

direct_abstract_declarator
   : '(' abstract_declarator ')'
   | '[' ']'
   | '[' constant_expression ']'
   | direct_abstract_declarator '[' ']'
   | direct_abstract_declarator '[' constant_expression ']'
   | '(' ')'
   | '(' parameter_type_list ')'
   | direct_abstract_declarator '(' ')'
   | direct_abstract_declarator '(' parameter_type_list ')'
   ;
   */

initializer
   : assignment_expression
   | '{' initializer_list '}'
   {
      $$ = $2;
   }
   | '{' initializer_list ',' '}'
   {
      $$ = $2;
   }
   ;

initializer_list
   : initializer
   {
      RESET_CURRENT
      $$->AddToList($1);
   }
   | initializer_list ',' initializer
   {
      $$ = $1;
      $$->AddToList($3);
   }
   ;

statement
   : labeled_statement
   {
      $$ = $1;
      $$->SetDebugLine(compile_process.yyloc.l);
   }
   | compound_statement
   {
      $$ = $1;
      $$->SetDebugLine(compile_process.yyloc.l);
   }
   | expression_statement
   {
      $$ = $1;
      $$->SetDebugLine(compile_process.yyloc.l);
   }
   | selection_statement
   {
      $$ = $1;
      $$->SetDebugLine(compile_process.yyloc.l);
   }
   | iteration_statement
   {
      $$ = $1;
      $$->SetDebugLine(compile_process.yyloc.l);
   }
   | jump_statement
   {
      $$ = $1;
      $$->SetDebugLine(compile_process.yyloc.l);
   }
   ;

labeled_statement
   : /*IDENTIFIER ':' statement
   |*/ CASE constant_expression ':' statement
   {
      RESET_CURRENT
      if(!iter_stmt && !switch_stmt){
         ERROR("\'case\' may only appear in a switch statement");
      }else
      if(!$2->IsConst()){
         ERROR("expression must be constant");
      }else{
         $$ = $4;
                              //store reloc info
         S_symbol_info si(SI_CASE, NULL, 0);
                              //convert constant into type of switch expression
                              //and save with reloc info
         E_TYPE_SPECIFIER type = switch_expr_type->GetPromoteType(switch_expr_type, str);
         if(type==TS_NULL){
            ERROR(str);
         }else{
            si.SetData($2->GetPromoteVal(type), 0);
            $$->AddReloc(si);
         }
      }
   }
   | DEFAULT ':' statement
   {
      RESET_CURRENT
      if(!iter_stmt && !switch_stmt){
         ERROR("\'default\' may only appear in a switch statement");
      }else{
         $$ = $3;
                              //store reloc info
         S_symbol_info si(SI_DEFAULT, NULL, 0);
         $$->AddReloc(si);
      }
   }
   ;

compound_statement
   : '{' '}'
   {                          //empty compound statement
      RESET_CURRENT
   }
   | '{' statement_list '}'
   {
      $$ = $2;
   }
   | '{' declaration_list '}'
   {                          //plain local variables - keep frame size
      RESET_CURRENT;
                              //instantiate locals initialization code
      for(dword i=0; i<$2->ListSize(); i++){
         const C_c_type &ct = $2->ListMember(i);
         const C_c_type &init = ct->GetInitializer();
         if(init){   //produce initialization code
                              //disable constness check
            if(!$$->codeAssign(ct, '=', init, str, false)){
               ERROR(str.Size() ? (const char*)str : ERR_CANTASSIGN);
            }
         }
      }
      $$->SolveCompoundRelocations($2, $3->GetFrameSize());
      $$->AddFrameSize($2->GetFrameSize());
      yyscope.DecLevel();     //destroy scope of decl_list
   }
   | '{' declaration_list statement_list '}'
   {                          //solve relocations for decl_list
      RESET_CURRENT;
                              //instantiate locals initialization code
      for(dword i=0; i<$2->ListSize(); i++){
         const C_c_type &ct = $2->ListMember(i);
         const C_c_type &init = ct->GetInitializer();
         if(init){   //produce initialization code
            if(!$$->codeAssign(ct, '=', init, str, false)){
               ERROR(str.Size() ? (const char*)str : ERR_CANTASSIGN);
            }
         }
      }
      $$->codeAddCode($3);
      $$->SolveCompoundRelocations($2, $3->GetFrameSize());
      $$->AddFrameSize($2->GetFrameSize()+$3->GetFrameSize());
      yyscope.DecLevel();     //destroy scope of decl_list
   }
   ;

declaration_list
   : declaration
   {                          //locals
                              //create their scope
      yyscope.IncLevel();
      RESET_CURRENT;
      for(dword i=0; i<$1->ListSize(); i++){
         C_c_type &ct = $1->ListMember(i);
         $$->AddFrameSize(1);
         ct->AddTypeFlags(TYPE_LOCAL);
         ct->SetDebugLine(compile_process.yyloc.l);

                              //check if identifier with such name doesnt already exist
         if(!yyscope.IsIdentifierInScope(ct->GetName())){
            yyscope.AddIdentifier(ct);
         }else{
            ERROR(C_fstr(ERR_TYPEDEFINED, (const char*)ct->GetName()));
         }
         $$->AddToList(ct);
      }
   }
   | declaration_list declaration
   {
      $$ = $1;
      for(dword i=0; i<$2->ListSize(); i++){
         C_c_type &ct = $2->ListMember(i);
         $$->AddFrameSize(1);
         ct->AddTypeFlags(TYPE_LOCAL);
         if(!yyscope.IsIdentifierInScope(ct->GetName())){
            yyscope.AddIdentifier(ct);
         }else{
            ERROR(C_fstr(ERR_TYPEDEFINED, (const char*)ct->GetName()));
         }
         $$->AddToList(ct);
      }
   }
   ;

statement_list
   : statement
   | statement_list statement
   {                          //join code + relocs
      $$ = $1;
      //if($$ != $1)
      {
         $$->codeAddCode($2);
         $$->SetFrameSize(Max($1->GetFrameSize(), $2->GetFrameSize()));
      }
   }
   ;

expression_statement
   : ';'
   {
      RESET_CURRENT
                              //set void due to neverending 'for' loops
      $$->SetType(TS_VOID);
   }
   | expression ';'
   ;

selection_statement
   : IF '(' expression ')' statement
   {
      RESET_CURRENT;
      if(!$$->codeSkipCond(false, $3, $5->GetCodeSize(), str)){
         ERROR(str);
      }
                              //join statement
      $$->codeAddCode($5);
      $$->SetFrameSize($5->GetFrameSize());
   }
   | IF '(' expression ')' statement ELSE statement
   {
      RESET_CURRENT;
                              //add 3 for JMP rel16 opcode
      if(!$$->codeSkipCond(false, $3, $5->GetCodeSize()+3, str)){
         ERROR(str);
      }
                              //join 1st statement
      $$->codeAddCode($5);
                              //skip out of true cond
      $$->codeSkip($7->GetCodeSize());
                              //join 2nd statement
      $$->codeAddCode($7);
      $$->SetFrameSize(Max($5->GetFrameSize(), $7->GetFrameSize()));
   }
   | SWITCH '(' expression ')'
   {
      ++switch_stmt;          //enable break
                              //push current expression type and setup new
      //$$ = switch_expr_type;
      switch_expr_type->Reset();
      switch_expr_type->CopyType($3);
   }
    statement
   {
      RESET_CURRENT;
                              //evaluate switch expression
      if(!$$->codePickParam($3, $3->GetType(), str))
         ERROR(str);
                              //build jump branch
      if(!$$->codeBuildSwitchBranch($6, $3->SizeOf(), str)){
         ERROR(str);
      }
      $$->codeAddCode($6);
                              //solve break relocations
      $$->codeRelocIterSkip(SI_BREAK, $$->GetCodeSize());
                              //pop current expression type and setup new
      //switch_expr_type = $5;
      --switch_stmt;
   }
   ;

iteration_statement
   : WHILE '(' expression ')'
   {
      ++iter_stmt;            //enable continue and break
   }
    statement
   {
      RESET_CURRENT;
                              //add 3 for JMP rel16 opcode
      if(!$$->codeSkipCond(false, $3, $6->GetCodeSize()+3, str)){
         ERROR(str);
      }
                              //join statement
      $$->codeAddCode($6);
                              //solve continue relocations
      $$->codeRelocIterSkip(SI_CONTINUE, 0);
                              //skip back (stmt size + expression)
      $$->codeSkip(-int($$->GetCodeSize()));
                              //solve break relocations
      $$->codeRelocIterSkip(SI_BREAK, $$->GetCodeSize());
      $$->SetFrameSize($6->GetFrameSize());
      --iter_stmt;
   }
   | DO
   {
      ++iter_stmt;            //enable continue and break
   }
    statement WHILE '(' expression ')' ';'
   {
      RESET_CURRENT;
                              //join statement
      $$->codeAddCode($3);
                              //solve continue relocations
      $$->codeRelocIterSkip(SI_CONTINUE, $$->GetCodeSize());

      if(!$$->codeSkipCond(true, $6, -int($$->GetCodeSize()), str)){
         ERROR(str);
      }
                              //solve break relocations
      $$->codeRelocIterSkip(SI_BREAK, $$->GetCodeSize());
      $$->SetFrameSize($3->GetFrameSize());
      --iter_stmt;
   }
   | FOR '(' expression_statement expression_statement ')'
   {
      ++iter_stmt;            //enable continue and break
   }
    statement
   {
      RESET_CURRENT;
                              //expand init statement
      $$->codeAddCode($3);

      int check_point = $$->GetCodeSize();
                              //expand check statement if not empty
      if($4->GetType() != TS_VOID){
                              //solve continue relocations
         $7->codeRelocIterSkip(SI_CONTINUE, -int($4->GetCodeSize()+3));
                              //skip-over if check fails - add 3 for jump back
         if(!$$->codeSkipCond(false, $4, $7->GetCodeSize()+3, str)){
            ERROR(str);
         }
      }else{
                              //solve continue relocations
         $7->codeRelocIterSkip(SI_CONTINUE, 0);
      }
                              //expand iteration statement
      $$->codeAddCode($7);
                              //jump back to checkpoint
      $$->codeSkip(-int($$->GetCodeSize()-check_point));
                              //solve break relocations
      $$->codeRelocIterSkip(SI_BREAK, $$->GetCodeSize());

      $$->SetFrameSize($7->GetFrameSize());
      --iter_stmt;
   }
   | FOR '(' expression_statement expression_statement expression ')'
   {
      ++iter_stmt;            //enable continue and break
   }
    statement
   {
      RESET_CURRENT
                              //expand init statement
      $$->codeAddCode($3);

      int check_point = $$->GetCodeSize();
                              //expand check statement if not empty
      if($4->GetType() != TS_VOID){
                              //solve continue relocations
         $8->codeRelocIterSkip(SI_CONTINUE, -int($4->GetCodeSize()+3));
                              //skip-over if check fails - add 3 for jump back
         if(!$$->codeSkipCond(false, $4, $8->GetCodeSize()+$5->GetCodeSize()+3, str)){
            ERROR(str);
         }
      }else{
                              //solve continue relocations
         $7->codeRelocIterSkip(SI_CONTINUE, 0);
      }
                              //expand iteration statement
      $$->codeAddCode($8);
                              //expand post-expression
      $$->codeAddCode($5);
                              //jump back to checkpoint
      $$->codeSkip(-int($$->GetCodeSize()-check_point));
                              //solve break relocations
      $$->codeRelocIterSkip(SI_BREAK, $$->GetCodeSize());

      $$->SetFrameSize($8->GetFrameSize());
      --iter_stmt;
   }
   ;

jump_statement
   : /*GOTO IDENTIFIER ';'
   | */CONTINUE ';'
   {
      RESET_CURRENT;
      if(!iter_stmt){
         ERROR("\'continue\' may only appear in a for, do, or while statement");
      }else{
         $$->codeSkipIter(SI_CONTINUE);
      }
   }
   | BREAK ';'
   {
      RESET_CURRENT;
      if(!iter_stmt && !switch_stmt){
         ERROR("\'break\' may only appear in a for, do, while, or switch statement");
      }else{
         $$->codeSkipIter(SI_BREAK);
      }
   }
   | RETURN ';'
   {
      RESET_CURRENT;
      if(fnt_ret_type->GetType() != TS_VOID){
         ERROR(ERR_MISSRETVAL);
      }
      $$->codeReturn();
   }
   | RETURN expression ';'
   {
      RESET_CURRENT;
      E_TYPE_SPECIFIER ret_type = fnt_ret_type->GetType();
      if(ret_type == TS_VOID){
         ERROR(ERR_CANTRETVAL);
      }else{
                              //expand expression
         if(!$$->codePickParam($2, fnt_ret_type->GetType(), str)){
            ERROR(str.Size() ? (const char*)str : ERR_MISMATCH);
         }else{
                              //return floats in FPU
            if(ret_type==TS_FLOAT){
               $$->codePickFPUValue();
            }
            $$->codeReturn();
         }
      }
   }
   ;

translation_unit
   : external_declaration
   | translation_unit external_declaration
   | IDENTIFIER ';'
   {
      $$ = $1;
      ERROR(C_fstr("useless identifier: '%s'", (const char*)$$->GetName()));
   }
   ;

external_declaration
   : function_definition
   {
                              //function
      RESET_CURRENT;
#if !defined _DEBUG && 0
                              //align code segment to dword
      while(tmp_code.size()&3)
         tmp_code.push_back(BRK);
#endif
                              //$$ is func type, list() are parameters, list(n) is compound statement
      C_c_type &fnc_def = $1;
      dword num_params = fnc_def->GetFrameSize();
      const C_c_type &fnc_body = fnc_def->GetFunctionBody();
                              //put fnc to external symbol table
      tmp_symbols.push_back(S_symbol_info(SI_LOCAL, fnc_def->GetName(), tmp_code.size()));

      tmp_code.push_back(OP_SCRIPT_FUNC_CODE);
      StoreWord(tmp_code, (word)num_params);

                              //create stack frame if local variables or fnc params
      dword frame_size = fnc_body->GetFrameSize();
      bool use_frame = (frame_size || num_params);
      if(use_frame){
         tmp_code.push_back(ENTER);
         StoreWord(tmp_code, word(frame_size * 4));
      }
      dword reloc_base = tmp_code.size();
                              //copy code
      const byte *code_ptr = fnc_body->GetCode();
      tmp_code.insert(tmp_code.end(), code_ptr, code_ptr+fnc_body->GetCodeSize());
                              //solve local relocations (return),
                              //copy rest to file reloc table
      const S_symbol_info *rt = fnc_body->GetRelocs();
      for(dword i=0; i<fnc_body->GetRelocNum(); i++){
         const S_symbol_info &ri = rt[i];
         switch(ri.type){
         case SI_RETURN:
            if(!use_frame){
                              //optimization - relace jmp by direct ret
               tmp_code[ri.GetAddress()+reloc_base-1] = RET;
            }else{
               *((word*)(&tmp_code[ri.GetAddress()+reloc_base])) =
                  word(tmp_code.size()-(ri.GetAddress()+reloc_base+2));
            }
            break;
         default:
            tmp_reloc.push_back(S_symbol_info(ri));
            tmp_reloc.back().address += reloc_base;
            break;
         }
      }
      if(use_frame)
         tmp_code.push_back(LEAVE);
      if(num_params){
                              //return & clean stack
         tmp_code.push_back(RETN);
         StoreWord(tmp_code, word(num_params*4));
      }else{
         tmp_code.push_back(RET);
      }
                              //function body implemented, free now as its no more needed
      fnc_def->SetFunctionBody(NULL);
      yyscope.DecLevel();
   }
   | declaration
   {
                              //instantiate all declarations,
                              // note: enums and typedefs have empy lists, so they do not instantiate anything
      for(dword i=0; i<$1->ListSize(); i++){
                              //put to global namespace
         C_c_type &ct = $1->ListMember(i);
         if(yyscope.IsIdentifierInScope(ct->GetName())){
            const C_c_type &c1 = yyscope.FindIdentifier(ct->GetName());
            if((c1->GetTypeFlags()&TYPE_EXTERN) && !(ct->GetTypeFlags()&TYPE_EXTERN)){
               if(c1->GetType() != ct->GetType()){
                  ERROR(C_fstr("type mismatch: '%s' - see previous extern declaration", (const char*)ct->GetName()));
               }
            }else{
               ERROR(C_fstr("variable already defined: '%s'", (const char*)ct->GetName()));
            }
         }
         yyscope.AddIdentifier(ct);
         if(!ct->dataInstantiate(tmp_data, tmp_symbols, tmp_reloc, str)){
            ERROR(str);
         }
      }
   }
   | table_declaration
   ;

function_definition
   : /*declaration_specifiers declarator declaration_list compound_statement
   | */declaration_specifiers declarator
   {
      $$ = $1;
      $2->SetReturnType($1);

      if(yyscope.IsIdentifierInScope($2->GetName())){
         const C_c_type &c1 = yyscope.FindIdentifier($2->GetName());
         if(c1->GetTypeFlags()&TYPE_EXTERN){
            bool match = (c1->GetReturnType()->GetType() == $2->GetReturnType()->GetType());
            if(match){
               const C_vector<C_c_type> &l1 = c1->GetList(), &l2 = $2->GetList();
               match = (l1.size() == l2.size());
               if(match){
                  int i;
                  for(i=l1.size(); i--; ){
                     if(l1[i]->GetType() != l2[i]->GetType())
                        break;
                  }
                  match = (i==-1);
               }
            }
            if(!match){
               ERROR("function declaration mismatch - see previous declaration");
            }
         }else{
            ERROR("function already declared");
         }
      }
                              //put fnc to global namespace
      yyscope.AddIdentifier($2);
                              //put func parameters into func scope
      yyscope.IncLevel();
      for(dword i=0; i<$2->ListSize(); i++){
         C_c_type &ct = $2->ListMember(i);
         ct->AddTypeFlags(TYPE_PARAM);
         ct->SetAddress(short(i*4+8));  //assume stack frame size
         yyscope.AddIdentifier(ct);
      }
                              //setup func info (for param return)
      fnt_ret_type->Reset();
      fnt_ret_type->CopyType($1);
      $$->SetName($2->GetName());

                              //store debug info
      $$->SetDebugLine(compile_process.yyloc.l);
   }
      compound_statement
   {
      $$ = $2;
      $$->SetFrameSize($$->ListSize());
      $$->SetFunctionBody($4);
      /*
      if(!$4->GetCodeSize()){
         ERROR("compund block without code");
      }
      */
   }
      /*
   | declarator declaration_list compound_statement
   | declarator
   {                          //assume fnc returns void
      RESET_CURRENT;
      $$->SetType(TS_VOID);
      $1->SetReturnType($$);
      if(yyscope.IsIdentifierInScope($1->GetName())){
         ERROR("function already declared");
      }
                              //put fnc to global namespace
      yyscope.AddIdentifier($1);
                              //put func parameters into func scope
      yyscope.IncLevel();
      for(dword i=0; i<$1->ListSize(); i++){
         C_c_type &ct = $1->ListMember(i);
         ct->AddTypeFlags(TYPE_PARAM);
         ct->SetAddress(i*4+8);  //assume stack frame size
         yyscope.AddIdentifier(ct);
      }
                              //setup func info (for param return)
      fnt_ret_type->Reset();
      fnt_ret_type->SetType(TS_VOID);
      //fnt_ret_type->SetName($1->GetName());
      $$->SetName($1->GetName());
   }
    compound_statement
   {
      $$ = $2;
      $$->SetFrameSize($$->ListSize());
      $$->SetFunctionBody($3);
   }
   */
   ;

/* String literal instantiated into code segment.
*/
table_literal
   : STRING_LITERAL
   {
      RESET_CURRENT
      $$->SetType(TS_LITERAL);
      $$->SetTypeFlags(TYPE_CONST);

      C_vector<byte> &seg = tmp_code;
      seg.push_back(OP_STRING);
                              //instantiate string in const segment
      $$->SetValue(seg.size());
      const C_str &str = $1->GetName();
      if(str.Size()){
         seg.insert(seg.end(), (byte*)(const char*)str, ((byte*)(const char*)str)+str.Size()+1);
      }else
         seg.push_back(0);
                              //clear name, no more needed
      $1->SetName(NULL);
   }


/* This type produces named table type, with stored display name stored as parent type.
*/

table_base_member
   : type_specifier IDENTIFIER
   {
      $$ = $1;
      switch($$->GetType()){
      case TS_INT:
      case TS_FLOAT:
      case TS_STRING:
      case TS_BOOL:
         break;
      default:
         $$->SetType(TS_INT);
         ERROR("unsupported type for table");
      }
      $$->AddTypeFlags(TYPE_TABLE);
      $$->SetName($2->GetName());
      if(!yyscope.AddIdentifier($$)){
         ERROR(C_fstr("symbol '%s' is already defined!", (const char*)$$->GetName()));
      }
   }
   ;

table_numeric_member_type
   : table_base_member table_literal
   {
      $$ = $1;
      $$->SetTableDesc($2);
   }
   ;

table_array_member_type
   : table_base_member '[' constant_expression ']' table_literal
   {
      $$ = $1;
      $$->AddTypeFlags(TYPE_ARRAY);
      int size = $3->GetValue();
      if(size <= 0){
         ERROR(C_fstr("invalid array size: %i", size));
         size = 1;
      }
      $$->SetValue(size);
      $$->SetTableDesc($5);
   }
   ;

table_member_type
   : table_numeric_member_type
   | table_array_member_type
   ;


table_branch_type_base
   : BRANCH table_literal
   {
      $$ = $1;
      $$->SetTableDesc($2);
   }
   | BRANCH table_literal ',' table_literal
   {
      $$ = $1;
      $$->SetTableDesc($2);
      $$->SetInitializer($4);
   }
   | BRANCH '[' constant_expression ']' table_literal
   {
      $$ = $1;
      $$->AddTypeFlags(TYPE_ARRAY);
      $$->SetTableDesc($5);
      int size = $3->GetValue();
      if(size <= 0){
         ERROR(C_fstr("invalid array size: %i", size));
         size = 1;
      }
      $$->SetValue(size);
   }
   | BRANCH '[' constant_expression ']' table_literal ',' table_literal
   {
      $$ = $1;
      $$->AddTypeFlags(TYPE_ARRAY);
      $$->SetTableDesc($5);
      $$->SetInitializer($7);
      int size = $3->GetValue();
      if(size <= 0){
         ERROR(C_fstr("invalid array size: %i", size));
         size = 1;
      }
      $$->SetValue(size);
   }
   ;

table_branch_type
   : table_branch_type_base
   | table_branch_type_base '(' table_literal ')'
   {
      $$ = $1;
      $$->SetParentType($3);
   }

/* This type adds init data to table_member_type, up to 3 init data kept in type's list.
   For branches, the branch has TS_NULL type, and its list contains additional members in branch.
*/
table_member
   : table_member_type ';'
   {
      $$ = $1;
      if($$->GetType()==TS_STRING){
         ERROR("invalid table string declaration - need size in () block after name");
      }
   }
   | table_member_type '(' constant_expression ')' ';'
   {
      $$ = $1;
      $$->AddToList($3);
   }
   | table_member_type '(' constant_expression ',' table_literal ')' ';'
   {
      $$ = $1;
      $$->AddToList($3);
      C_c_type ct = new C_c_type_imp;
      ct->SetType(TS_INT);
      ct->SetTypeFlags(TYPE_CONST);
      $$->AddToList(ct);
      $$->AddToList(ct);
      ct->Release();
      $$->AddToList($5);
   }
   | table_member_type '(' constant_expression ',' constant_expression ',' constant_expression ')' ';'
   {
      $$ = $1;
      $$->AddToList($3);
      $$->AddToList($5);
      $$->AddToList($7);
      switch($$->GetType()){
      case TS_BOOL:
      case TS_STRING:
         ERROR("variable of this type doesn't accept range values");
         break;
      }
   }
   | table_member_type '(' constant_expression ',' constant_expression ',' constant_expression ',' table_literal')' ';'
   {
      $$ = $1;
      $$->AddToList($3);
      $$->AddToList($5);
      $$->AddToList($7);
      $$->AddToList($9);
      switch($$->GetType()){
      case TS_STRING:
      case TS_BOOL:
         ERROR("variable of this type doesn't accept range values");
         break;
      }
   }
   | table_branch_type '{' table_init_list '}'
   {
      $$ = $1;
      $$->CopyList($3);
   }
   ;

/* This type makes list of table members.
*/
table_init_list
   : table_member
   {
      RESET_CURRENT;
      $$->AddToList($1);
   }
   | table_init_list table_member
   {
      $$ = $1;
      $$->AddToList($2);
   }
   ;

/* This type instantiates table, using provided initialize list.
*/
table_declaration
   : TABLE table_literal '{' table_init_list '}'
   {
      if(!$4->InstantiateTableTemplate($2, tmp_templ, tmp_reloc, str)){
         ERROR(str);
      }
   }
   ;

/* End of grammar */
%%

