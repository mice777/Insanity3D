/*
   Lexer
   Copyright (c) 1998 Michal Bacik
*/
#include "all.h"
#include "yacc.h"

//----------------------------
                              //ANSI-C keywords
static const struct S_keyword{
   byte len;
   char kw[11];
}keywords[] = {
   {7, "typedef"     },
   //{6, "sizeof"      },
   {6, "extern"      },
   //{6, "static"      },
   //{4, "auto"        },
   //{8, "register"    },
   {4, "char"        },
   {5, "short"       },
   {3, "int"         },
   //{4, "long"        },
   //{6, "signed"      },
   //{8, "unsigned"    },
   {5, "float"       },
   //{6, "double"      },
   {5, "const"       },
   //{8, "volatile"    },
   {4, "void"        },
   {6, "struct"      },
   //{5, "union"       },
   {4, "enum"        },
   {4, "case"        },
   {7, "default"     },
   {2, "if"          },
   {4, "else"        },
   {6, "switch"      },
   {5, "while"       },
   {2, "do"          },
   {3, "for"         },
   //{4, "goto"        },
   {8, "continue"    },
   {5, "break"       },
   {6, "return"      },
   {4, "byte"        },
   {4, "word"        },
   {5, "dword"       },
   {6, "string"      },
   {4, "bool"        },
   {4, "true"        },
   {5, "false"       },
   {5, "table"       },
   {6, "branch"      },
                              //extended keywords
   //{9, "__stdcall"   },
}, directives[] = {
   {7, "include"     },
   {6, "define"      },
   {2, "if"          },
   {5, "ifdef"       },
   {6, "ifndef"      },
   {5, "endif"       },
   {4, "elif"        },
   {4, "else"        },
   {5, "undef"       },
};

//----------------------------
                              //find keyword among other keywords,
                              //return index of found keyword in table
                              //or return -1 if not found
inline int FindKeyword(const char *kw, int kw_len, const S_keyword *kw_list){

   int i=sizeof(keywords)/sizeof(S_keyword);
   while(i--)
      if(kw_list[i].len==kw_len && !strcmp(kw_list[i].kw, kw)) break;
   return i;
}

//----------------------------
                              //read literal - character constant after '\' char
static bool ReadLiteral(C_cache &ck, S_location &yyloc, char &c){

   if(ck.eof())
      return false;
   ck.read(&c, 1);
   ++yyloc.r;
   //int num_val;

   switch(c){
   //case 'a': c = '\a'; return true;
   //case 'b': c = '\b'; return true;
   //case 'f': c = '\f'; return true;
   case 'n': c = '\n'; return true;
   //case 'r': c = '\r'; return true;
   //case 't': c = '\t'; return true;
   //case 'v': c = '\v'; return true;
   //case '\?': c = '\?'; return true;
   case '~': c = '~'; return true;
   case '\"': c = '\"'; return true;
   case '\'': c = '\''; return true;
      /*
   case 'x': case 'X':        //hex number
      num_val = 0;

      while(true){
         ck.read(&c, 1);
         ++yyloc.r;
         if(ck.eof()){
            c = num_val;
            return false;
         }
         if(!isxdigit(c)){
            ck.seekg(ck.tellg()-1);
            --yyloc.r;
            c = num_val;
            return false;

         }
         num_val *= 16;
         num_val += isdigit(c) ? c - '0' :
            isupper(c) ? c - 'A' + 10 :
            c - 'a' + 10;
      }
      */
   default:
      return false;
   }
   return true;
}

//----------------------------
// Scan number from input. Result may be either float or integer.
static int ScanNumber(char c, C_cache &ck, C_c_type &yylval, S_location &yyloc){

   long double ld = 0.0;
   bool flt = false;
   while(true){
      if(c=='.'){
         flt=true;
         long double dd = .1;
         while(true){
            if(ck.eof()) break;
            ck.read(&c, 1);
            ++yyloc.r;
            if(!isdigit(c)){
               switch(tolower(c)){
               case 'f': break;
               case 'd': break;
               default: 
                  ck.seekg(ck.tellg()-1);
                  --yyloc.r;
               }
               break;
            }
            ld += dd*(c-'0');
            dd *= .1;
         }
         break;
      }
      ld *= 10.0;
      ld += c - '0';
      if(ck.eof()) break;
      ck.read(&c, 1);
      ++yyloc.r;
      if(!isdigit(c) && c!='.'){
         ck.seekg(ck.tellg()-1);
         --yyloc.r;
         break;
      }
   }
   if(flt){
      yylval->SetFValue((float)ld);
      yylval->SetType(TS_FLOAT);
   }else{
      yylval->SetValue((dword)ld);
      yylval->SetType(TS_INT);
   }
   yylval->AddTypeFlags(TYPE_CONST);
   return CONSTANT;
}

//----------------------------

enum E_PREP_DIR{
   PD_EOF,                    //end of file encountered
   PD_EOL,                    //end of line encountered
   PD_INVALID,                //invalid directive
   PD_INCLUDE,                
   PD_DEFINE,
   PD_IF,
   PD_IFDEF,
   PD_IFNDEF,
   PD_ENDIF,
   PD_ELIF,
   PD_ELSE,
   PD_UNDEF,
};

//----------------------------
// Read preprocessing directive. The returned value is one of E_PREP_DIR enumerated type.
static E_PREP_DIR ReadDirective(C_cache &ck, S_location &yyloc, C_str &err){

   char c;

   for(;;){
      if(ck.eof())
         return PD_EOF;
      ck.read(&c, 1);
      ++yyloc.r;
      if(!isspace(c))
         break;
      if(c=='\n')
         return PD_EOL;
   }
                              //get word
   char kw[64];
   kw[0] = c;
   int i = 1;
   while(true){
      if(ck.eof()) break;
      ck.read(&c, 1);
      ++yyloc.r;
      if(isspace(c)){
         if(c=='\n')
            yyloc.NewLine();
         break;
      }
      kw[i++] = c;
      if(i==sizeof(kw)-1)
      while(true){            
         if(ck.eof()) break;
                              //read the rest of word, but don't store more
         ck.read(&c, 1);
         ++yyloc.r;
         if(isspace(c)){
            if(c=='\n')
               yyloc.NewLine();
            break;
         }
      }
   }
   kw[i] = 0;
                              //scan for keyword among keywords
   i = FindKeyword(kw, i, directives);
   if(i==-1){
      err = "invalid preprocessor directive \'";
      err += kw;
      err += "\'";
   }

   return (E_PREP_DIR)(i+1+PD_INVALID);
}

//----------------------------
// Skip current line. Return true if successful, or false if EOL encountered.
static bool SkipLine(C_cache &ck, S_location &yyloc){

   while(true){
      char c;
      if(ck.eof())
         return false;
      ck.read(&c, 1);
      if(c=='\n'){
         yyloc.NewLine();
         return true;
      }
   }
}

//----------------------------
// Read next non-whitespace character, or return 0 if EOL.
static char ReadNonWSChar(C_cache &ck, S_location &yyloc){

   while(true){
      char c;
      if(ck.eof())
         return 0;
      ck.read(&c, 1);
      ++yyloc.r;
      if(!isspace(c))
         return c;
      if(c=='\n')
         yyloc.NewLine();
   }
}

//----------------------------
// Read text from input stream into the buffer, terminated by specified character,
// up to given buffer size.
// If EOL or EOF is encountered before terminator, or text is greater than buffer size, func returns false.
bool ReadText(C_cache &ck, char *buf, int maxlen, char term, S_location &yyloc){

   if(!maxlen)
      return false;
   while(true){
      char c;
      if(ck.eof())
         return 0;
      ck.read(&c, 1);
      ++yyloc.r;
      if(c==term)
         break;
      if(c=='\n'){
         ck.seekg(ck.tellg()-1);
         return false;
      }
      if(!--maxlen)
         return false;
      *buf++ = c;
   }
   *buf = 0;
   return true;
}

//----------------------------
// Lexically scan source.
// Return valid negative result and error message in err in case of error.
// Return 0 on EOF.
// Return -1 on fatal error.
// Otherwise return code of input, and negative code if invalid input.
// Set details about returned type into yylval.
int C_compile_process::yyLex(C_c_type &yylval, const C_scope &scope, C_str &err){

   C_cache &ck = *include_files.back().ck;

   char c;
   char buf[128];
                              //skip whitespace
   bool is_beg_line = (yyloc.r==1);
   for(;;){
      if(ck.eof()){
         if(include_files.size()==1)
            return 0;
         delete include_files.back().ck;
         include_files.pop_back();
         const S_incl_file &inc_f = include_files.back();
         yyloc = inc_f.loc;
         curr_dbg_info = &debug_info[inc_f.name];
         return yyLex(yylval, scope, err);
      }
      ck.read(&c, 1);

      ++yyloc.r;
      if(is_beg_line && c=='#'){
                              //preprocessor directive
         E_PREP_DIR dir = ReadDirective(ck, yyloc, err);
         switch(dir){
         case PD_EOF:
            break;
         case PD_EOL:
            yyloc.NewLine();
            break;
         case PD_INVALID:
            return -1;
         case PD_INCLUDE:
            c = ReadNonWSChar(ck, yyloc);
            switch(c){
            case '<':
               c = '>';
                              //flow...
            case '"':
                              //read filename
               if(ReadText(ck, buf, sizeof(buf), c, yyloc)){
                              //check if file already in incl. list
                  for(dword i=0; i<include_files.size(); i++)
                  if(include_files[i].name==buf){
                     err = C_fstr("file '%s' recursively includes itself", buf);
                     return -1;
                  }
                  if(include_files.size()>=8){
                     err = "include depth too big (8 files)";
                     return -1;
                  }
                              //try open file
                  C_cache *ck = new C_cache;
                              //search all include paths
                  bool ok = true;
                  if(!ck->open(buf, CACHE_READ)){
                     ck->close();
                     ok = false;
                     for(dword i=0; i<include_paths.size(); i++){
                        if(ck->open(include_paths[i]+buf, CACHE_READ)){
                           ok = true;
                           break;
                        }
                     }
                  }

                  if(!ok){
                     delete ck;
                     err = C_fstr("unable to open include file: '%s'", buf);
                     return -1;
                  }
                  include_files.back().loc = yyloc;
                  include_files.push_back(S_incl_file(ck, buf));
                  AddDependency(buf);
                  yyloc.Home();
                  curr_dbg_info = &debug_info[buf];
                  return yyLex(yylval, scope, err);
               }
                              //flow...
            default:
               SkipLine(ck, yyloc);
            case '\n':
               err = "invalid #include directive - end of line encountered";
               return -1;
            }
            break;
         }
      }else{
         if(!isspace(c))
            break;
         if(c=='\n'){
            is_beg_line = true;
            yyloc.NewLine();
         }
      }
   }

                              //process numbers
   if(c=='0'){
                              //hex number
      ck.read(&c, 1);
      ++yyloc.r;
      if(c=='x' || c=='X'){
         bool valid = true;
         dword dw = 0;
         while(true){
            ck.read(&c, 1);
            if(!isxdigit(c)){
               ck.seekg(ck.tellg()-1);
               if(isalnum(c) || c=='_'){
                  err = "invalid number";
                  valid = false;
               }
               break;
            }
            if(valid && (dw&0xf0000000)){
               valid = false;
               err = "too big hex number";
            }
            dw <<= 4;
            int num = isdigit(c) ? c-'0' :
               (tolower(c) - 'a' + 0xa);
            dw |= num;
         }
         yylval->SetValue(dw);
         yylval->SetType(TS_DWORD);

         if(!valid){
            return -CONSTANT;
         }
      }else
      if(isdigit(c)){
         return ScanNumber(c, ck, yylval, yyloc);
      }else
      if(!isalpha(c)){
                              //floating number?
         if(c=='.')
            return ScanNumber(c, ck, yylval, yyloc);
                              //plain zero
         ck.seekg(ck.tellg()-1);
         --yyloc.r;
         yylval->SetValue(0);
         yylval->SetType(TS_INT);
      }else{
                              //invalid number with ALPHA char after it
         err = "invalid number";
         return -CONSTANT;
      }
      yylval->AddTypeFlags(TYPE_CONST);
      return CONSTANT;
   }

   if(isdigit(c)){
      return ScanNumber(c, ck, yylval, yyloc);
   }

   if(c=='.'){
      ck.read(&c, 1);
      if(!isdigit(c)){
                              //plain dot
         ck.seekg(ck.tellg()-1);
         return '.';
      }
                              //float number
      ck.seekg(ck.tellg()-1);
      return ScanNumber('.', ck, yylval, yyloc);
   }

   if(isalpha(c) || c=='_'){
                              //get word
      char kw[64];
      kw[0] = c;
      int i = 1;
      while(!ck.eof()){
         ck.read(&c, 1);
         ++yyloc.r;
         if(!isalnum(c) && c!='_'){
                              //end of word
            --yyloc.r;
            ck.seekg(ck.tellg()-1);
            break;
         }
         kw[i++] = c;
         if(i==sizeof(kw)-1){
            while(!ck.eof()){
                              //read the rest of word, but don't store more
               ck.read(&c, 1);
               ++yyloc.r;
               if(!isalnum(c) && c!='_'){
                                 //end of word
                  --yyloc.r;
                  ck.seekg(ck.tellg()-1);
                  break;
               }
            }
            break;
         }
      }
      kw[i] = 0;
                              //scan for keyword among keywords
      i = FindKeyword(kw, i, keywords);
      if(i!=-1){
         i += TYPEDEF;
         return i;
      }
      C_str str = kw;
                              //look among type names in current scopes
      const C_c_type &type = scope.FindType(str);
      if(type){
         yylval = type;
         return TYPE_NAME;
      }
                              //identifier
      yylval->SetName(str);
      return IDENTIFIER;
   }

   if(c=='\"'){
                              //string literal
      const dword MAX_STRING_SIZE = 256;
      char string[MAX_STRING_SIZE+1];
      dword count = 0;
      bool b_err = false;
      do{
         if(ck.eof()){
            err = "invalid string - end of file encountered";
            return -1;
         }
         ck.read(&c, 1);
         ++yyloc.r;
         if(c=='\"'){
                              //skip whitespace and try to join another string
            do{
               if(ck.eof())
                  break;
               ck.read(&c, 1);
               ++yyloc.r;
               if(!isspace(c))
                  break;
               if(c=='\n')
                  yyloc.NewLine();
            }while(true);
            if(c=='\"')
               continue;
            ck.seekg(ck.tellg()-1);
            --yyloc.r;
            break;
         }else
         if(c=='~'){
            bool b = ReadLiteral(ck, yyloc, c);
            if(!b){
               err = C_fstr("invalid string - character '%c' is not a valid escape sequence", c);
               b_err = true;
            }
         }
         /*
         if(c=='\\' && count && string[count-1]=='\\'){
            err = "!\\\\";
            b_err = true;
         }
         */
         if(count==MAX_STRING_SIZE){
            err = "string too long";
            b_err = true;
         }
         if(count<MAX_STRING_SIZE)
            string[count++] = c;
      }while(true);
      string[count] = 0;
      yylval->SetName(string);
      return b_err ? -STRING_LITERAL : STRING_LITERAL;
   }

   if(c=='\''){
                              //character constant
      dword cc = 0;
      bool b_err = false;

      dword count = 0;
      while(true){
         if(ck.eof()){
            err = "invalid character constant - end of file encountered";
            b_err = true;
            break;
         }
         ck.read(&c, 1);
         ++yyloc.r;
         if(c=='\'')
            break;
         if(c=='~'){
            if(!ReadLiteral(ck, yyloc, c)){
               err = "invalid character constant";
               b_err = true;
            }
         }
         cc |= c << (8*count);
         ++count;
      }
      if(!count || count>4){
         err = "invalid character constant";
         b_err = true;
      }
                              //todo: add support for wider character contants, up to 4 bytes
      yylval->SetValue(cc);
      yylval->SetType(count<=1 ? TS_CHAR : TS_DWORD);
      yylval->AddTypeFlags(TYPE_CONST);
      return !b_err ? CONSTANT : -CONSTANT;
   }
                              //check operators
   int ret_val = c;
   char c1;
   if(ck.eof())
      c1 = '\0';
   else{
      ck.read(&c1, 1);
      ++yyloc.r;
   }

   switch(c){
   case '>':
      switch(c1){
      case '>':
         if(!ck.eof()){
            ck.read(&c1, 1);
            ++yyloc.r;
            if(c1=='='){
               yylval->SetValue(RIGHT_ASSIGN);
               return RIGHT_ASSIGN;
            }
         }
         ret_val = RIGHT_OP;
         break;
      case '=':
         yylval->SetValue(GE_OP);
         return GE_OP;
      }
      break;
   case '<':
      switch(c1){
      case '<':
         if(!ck.eof()){
            ck.read(&c1, 1);
            ++yyloc.r;
            if(c1=='='){
               yylval->SetValue(LEFT_ASSIGN);
               return LEFT_ASSIGN;
            }
         }
         ret_val = LEFT_OP;
         break;
      case '=':
         yylval->SetValue(LE_OP);
         return LE_OP;
      }
      break;
   case '+':
      switch(c1){
      case '=':
         yylval->SetValue(ADD_ASSIGN);
         return ADD_ASSIGN;
      case '+':
         yylval->SetValue(INC_OP);
         return INC_OP;
      }
      break;
   case '-':
      switch(c1){
      case '=':
         yylval->SetValue(SUB_ASSIGN);
         return SUB_ASSIGN;
      case '-':
         yylval->SetValue(DEC_OP);
         return DEC_OP;
      }
      break;
   case '*':
      if(c1=='='){
         yylval->SetValue(MUL_ASSIGN);
         return MUL_ASSIGN;
      }
      break;
   case '/':
      switch(c1){
      case '=':
         yylval->SetValue(DIV_ASSIGN);
         return DIV_ASSIGN;
      case '/':               //cpp-style comment
         do{
            if(ck.eof()) break;
            ck.read(&c, 1);
            ++yyloc.r;
         }while(c!='\n');
         yyloc.NewLine();
         return yyLex(yylval, scope, err);
      case '*':               //comment start
         byte phase=0;
         do{
            if(ck.eof()){
               err = "end of file encountered in comment";
               return -1;
            }
            ck.read(&c, 1);
            ++yyloc.r;
            if(c=='*')
               ++phase;
            else
            if(c=='/' && phase)
               break;
            else{
               if(c=='\n')
                  yyloc.NewLine();
               phase = 0;
            }
         }while(true);
         return yyLex(yylval, scope, err);
      }
      break;
   case '%':
      if(c1=='='){
         yylval->SetValue(MOD_ASSIGN);
         return MOD_ASSIGN;
      }
      break;
   case '&':
      switch(c1){
      case '=':
         yylval->SetValue(AND_ASSIGN);
         return AND_ASSIGN;
      case '&':
         yylval->SetValue(AND_OP);
         return AND_OP;
      }
      break;
   case '^':
      if(c1=='='){
         yylval->SetValue(XOR_ASSIGN);
         return XOR_ASSIGN;
      }
      break;
   case '|':
      switch(c1){
      case '=':
         yylval->SetValue(OR_ASSIGN);
         return OR_ASSIGN;
      case '|':
         yylval->SetValue(OR_OP);
         return OR_OP;
      }
      break;
   case '=':
      if(c1=='='){
         yylval->SetValue(EQ_OP);
         return EQ_OP;
      }
      break;
   case '!':
      if(c1=='='){
         yylval->SetValue(NE_OP);
         return NE_OP;
      }
      break;
   case '\\':
      if(c1=='\n'){
         yyloc.NewLine();
         return yyLex(yylval, scope, err);
      }
      break;
   }
   if(!ck.eof()){
      ck.seekg(ck.tellg()-1);
      --yyloc.r;
   }
   yylval->SetValue(ret_val);
   return ret_val;
}

//----------------------------
