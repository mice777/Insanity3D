#include "pch.h"


static const char TEXT_MESSAGE[] = "Initializing texts";

//----------------------------
                              //leave implementation private to this source file

                              //akk_texts implementation - using STL map
class C_all_texts_rep{
   struct S_text{
      C_str str;
      bool unicode;

      S_text(){}
      S_text(const S_text &t){ operator =(t); }
      S_text &operator =(const S_text &t){
         str = t.str;
         unicode = t.unicode;
         return *this;
      }
   };
   //typedef map<int, S_text> t_text_map;
   typedef map<C_str, S_text> t_text_map;
   t_text_map m;
   friend C_all_texts;
public:

   void AddLines(const char *name, C_cache &is){

      int i;
      char line[4096], *cp;
      int line_num = 0;
      while(!is.eof()){
         ++line_num;
         is.getline(line, 4096);
         cp = line;
         if(!*cp || *cp==';') continue;

         if(*cp != '~'){
            OsMessageBox(NULL, C_fstr("File '%s': invalid text identification on line %i", name, line_num), TEXT_MESSAGE, MBOX_OK);
            continue;
         }
         ++cp;

         char *id_str = cp;
         i = strlen(id_str);
         while(i && isspace(id_str[i-1])) --i;
         if(!i){
            OsMessageBox(NULL, C_xstr("File '%': invalid ID '%' (line %)") % name % id_str % line_num, TEXT_MESSAGE, MBOX_OK);
         }
         id_str[i] = 0;
         /*
         char id_str[256];
         int num = sscanf(cp, "%256s%n", id_str, &i);
         if(!num || num==EOF)
            continue;
            */

         t_text_map::iterator it;
#ifdef _DEBUG
                                 //check duplication
         it = m.find(id_str);
         if(it!=m.end()){
            OsMessageBox(NULL, C_fstr("File '%s': text duplication on ID '%s' (line %i)", name, id_str, line_num), TEXT_MESSAGE, MBOX_OK);
         }
#endif
         it = m.insert(pair<C_str, S_text>(id_str, S_text())).first;

         C_str str;
         while(!is.eof()){
            ++line_num;
            is.getline(line, 4096);
            cp = line;
            if(!*cp) break;
            if(*cp==';') continue;
                              //check special option - explicit empty line
            if(*cp=='~')
               *cp = 0;
                              //check redunant spaces
            for(int i=strlen(cp); i--; ){
               if(!isspace((byte)cp[i]))
                  break;
               OsMessageBox(NULL, C_fstr("File '%s': redundant space on line %i", name, line_num), TEXT_MESSAGE, MBOX_OK);
            }
            if(str.Size())
               str += "\n";
            str += cp;
         }
                              //store the text
         (*it).second.str = str;
         (*it).second.unicode = false;
         /*
         cp += i;
         while(*cp && *cp!='"') ++cp;
         if(*cp++){
            i = 0;
            
            bool loop = true;
            while(loop)
            switch(cp[i]){
            case '"':
               {
                              //check to eol to find bracing errors
                  for(int j=i+1; cp[j] && isspace(cp[j]); j++);
                  if(cp[j] && cp[j]!=';'){
                     MessageBox(NULL, C_fstr("File %s: text error on line %i\n(text after last quote)", name, line_num),
                        TEXT_MESSAGE, MB_OK);
                  }
                  cp[i] = 0;
                  loop = false;
               }
               break;
            case '\\':
               cp[i] = '\n';
               break;
            case 0:
               {
                  MessageBox(NULL, C_fstr("File %s: text error on line %i\n(no ending quote)", name, line_num),
                     TEXT_MESSAGE, MB_OK);
                  loop = false;
               }
               break;
            default: 
               ++i;
            }
            it = m.insert(pair<C_str, S_text>(id_str, S_text())).first;
            (*it).second.str = cp;
            (*it).second.unicode = false;
         }
         */
      }
   }

//----------------------------

   void AddLinesW(const char *name, C_cache &is){
                              //read whole file
      dword num_chars = is.filesize()/sizeof(wchar_t);
      wchar_t *buf = new wchar_t[num_chars];
      is.read((char*)buf, is.filesize());

      dword curr_pos = 0;

      int i;
      bool line1_read = false;
      while(curr_pos < num_chars){
                              //find end of current line
         for(dword eol = curr_pos; eol<num_chars; eol++){
            if(buf[eol]=='\r' && buf[eol+1]=='\n')
               break;
         }

         if(!line1_read){     //ignore 1st line
            line1_read = true;
            curr_pos = eol + 2;
            continue;
         }
                              //skip empty lines and comments
         if(eol!=curr_pos && buf[curr_pos]!=';'){
                              //scan number
            wchar_t id_str_w[256];
            char id_str[256];
            int num = swscanf(&buf[curr_pos], L"%256s%n", id_str_w, &i);
            if(!num || num==EOF)
               continue;
            dword i;
            for(i=0; id_str_w[i]; i++)
               id_str[i] = id_str_w[i];
            /*
            int id = 0x80000000;
            for(; buf[curr_pos]<256 && isdigit(buf[curr_pos]); curr_pos++)
               id = id*10 + buf[curr_pos]-'0';

            if(id<0){
               curr_pos = eol + 2;
               continue;
            }
            */

            t_text_map::iterator it;
#ifdef _DEBUG
                                 //check duplication
            it = m.find(C_str(id_str));
            if(it!=m.end()){
               OsMessageBox(NULL, C_fstr("File %s: text duplication on ID '%s'", name, id_str),
                  TEXT_MESSAGE, MBOX_OK);
            }
#endif
                              //move to beginning
            while(curr_pos<eol && buf[curr_pos]!='\"') ++curr_pos;
            ++curr_pos;
                              //find end
            for(i=curr_pos; i<eol && buf[i]!='\"'; ++i){
               wchar_t c = buf[i];
               switch(c){
               case '\\':
                  buf[i] = '\n';
                  break;
               }
            }
                              //make string
            C_str str;
            str.Assign((const char*)&buf[(int)curr_pos], (i-curr_pos)*sizeof(wchar_t) + 2);
            str[str.Size()-2] = 0;
            str[str.Size()-1] = 0;

                              //save
            it = m.insert(pair<C_str, S_text>(id_str, S_text())).first;
            (*it).second.str = str; 
            (*it).second.unicode = true;
         }
                              //next line
         curr_pos = eol + 2;
      }
      delete[] buf;
   }

//----------------------------

   bool AddFile(class C_cache &is, const char *name){

                              //read 1st word to detect unicode format
      word w;
      is.read((char*)&w, sizeof(word));
      is.seekp(0);
      if(w!=0xfeff){
         AddLines(name, is);
      }else{
         AddLinesW(name, is);
      }
      return true;
   }

//----------------------------

   bool AddFile(class C_cache &is){
      return AddFile(is, "unknown");
   }

//----------------------------

   bool AddFile(const char *name){
      
      C_cache is;
      if(!is.open(name, CACHE_READ))
         return false;
      return AddFile(is, name);
   }
};

//----------------------------

C_all_texts::C_all_texts():
   rep(NULL)
{}

//----------------------------

C_all_texts &C_all_texts::operator =(const C_all_texts &at){

   Close();
   if(at.rep){
      rep = new C_all_texts_rep;
      rep->m = at.rep->m;
   }
   return *this;
}

//----------------------------

bool C_all_texts::AddFile(const char *name){

   if(!rep)
      rep = new C_all_texts_rep;
   return rep->AddFile(name);
}

//----------------------------

bool C_all_texts::AddFile(class C_cache &is){

   if(!rep)
      rep = new C_all_texts_rep;
   return rep->AddFile(is);
}

//----------------------------

void C_all_texts::Close(){

   delete rep;
   rep = NULL;
}

//----------------------------

const C_str &C_all_texts::operator [](const C_str &id) const{

   if(!rep){
      static C_str err_str;
      err_str = C_fstr("error: text '%s' not defined", (const char*)id);
      return err_str;
   }
   C_all_texts_rep::t_text_map::const_iterator it;
   it = rep->m.find(id);
   if(it==rep->m.end()){
      static C_str err_str;
      err_str = C_fstr("error: text '%s' not defined", (const char*)id);
      return err_str;
   }
   return (*it).second.str;
}

//----------------------------

const C_str &C_all_texts::operator [](int id) const{
   return operator [](C_fstr("%i", id));
}

//----------------------------

bool C_all_texts::IsDefined(const C_str &id) const{

   C_all_texts_rep::t_text_map::const_iterator it;
   it = rep->m.find(id);
   return (it!=rep->m.end());
}

//----------------------------

bool C_all_texts::IsDefined(int id) const{
   return IsDefined(C_fstr("%i", id));
}

//----------------------------

bool C_all_texts::IsWide(const C_str &id) const{

   C_all_texts_rep::t_text_map::const_iterator it;
   it = rep->m.find(id);
   if(it==rep->m.end())
      return false;
   return (*it).second.unicode;
}

//----------------------------

bool C_all_texts::IsWide(int id) const{
   return IsWide(C_fstr("%i", id));
}

//----------------------------

void C_all_texts::EnumTexts(void(*cbEnum)(const C_str&, const C_str&, void *context), void *context){

   C_all_texts_rep::t_text_map::const_iterator it;
   for(it=rep->m.begin(); it!=rep->m.end(); it++){
      (*cbEnum)((*it).first, (*it).second.str, context);
   }
}

//----------------------------

/*
void C_all_texts::DelTexts(int base_id, int num_ids){

   C_all_texts_rep::t_text_map::iterator it, it_next;
   for(it=rep->m.begin(); it!=rep->m.end(); ){
      int id = (*it).first;
      if(id >= base_id && id < (base_id + num_ids))
         it = rep->m.erase(it);
      else
         ++it;
   }
}
*/

//----------------------------
//----------------------------
