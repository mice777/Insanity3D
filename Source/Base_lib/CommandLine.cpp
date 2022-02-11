#include <cmdline.h>
#include <insanity\os.h>

//----------------------------

void C_command_line_base::DisplayError(const char *err_msg, const char *app_name){

   OsMessageBox(NULL, err_msg, app_name, MBOX_OK);
}

//----------------------------

bool C_command_line_base::ReadInt(const char **cl, int &ret){

   int num;
   int i = sscanf(*cl, "%i%n", &ret, &num);
   if(i!=1)
      return false;
   *cl += num;
   return true;
}

//----------------------------

bool C_command_line_base::ReadString(const char **cl, C_str &ret){

   char param[512];

   while(isspace(**cl))
      ++*cl;
   bool is_quoted = false;
   if(**cl == '"'){
      is_quoted = true;
      ++*cl;
   }
   if(!**cl)
      return false;
   for(int i=0; i<sizeof(param)-1 && **cl; i++){
      char c = **cl;
      (*cl)++;
      if(is_quoted){
         if(c=='"')
            break;
      }else
      if(isspace(c)){
         break;
      }
      param[i] = c;
   }
   param[i] = 0;
   ret = param;
   return true;
}

//----------------------------

bool C_command_line_base::Scan(const char *cl, const char *app_name){

   const S_scan_data *sd = GetScanData();
   assert(sd);

   char keyword[256];
   int i;
   while(*cl){
                              //get keyword
      int kw_len;
      i = sscanf(cl, "%255s%n", keyword, &kw_len);
      if(i!=1)
         break;
      cl += kw_len;
      if(num_match_chars && kw_len<num_match_chars){
         goto fail;
      }

                              //find command scan data
      const S_scan_data *sdp;
      if(num_match_chars){
         sdp = 0;
         int best_match = 0;
         for(const S_scan_data *sdp1 = sd; sdp1->command_name; ++sdp1){
            int l = strlen(sdp1->command_name);
            l = Min(l, kw_len);
            int i;
            for(i=0; i<l; i++){
               if(tolower(keyword[i]) != tolower(sdp1->command_name[i]))
                  break;
            }
            if(i<num_match_chars)
               continue;
            if(best_match<i){
               best_match = i;
               sdp = sdp1;
            }
         }
      }else{
         for(sdp = sd; sdp->command_name; ++sdp){
            if(!stricmp(sdp->command_name, keyword))
               break;
         }
      }
      if(!sdp || !sdp->command_name)
         goto fail;

                              //read additional parameters
      dword op = sdp->operation;
      void *ptr = (void*)sdp->data_offset;
      if(!(op&CMD_GLOBAL_PTR))
         ((byte*&)ptr) += (dword)this;

      switch(op&0xffff){
      case CMD_NOP: break;
      case CMD_BOOL: *(bool*)ptr = true; break;

      case CMD_BOOL_CLEAR: *(bool*)ptr = false; break;
      case CMD_INT:
         if(!ReadInt(&cl, *(int*)ptr))
            goto bad_param;
         break;
      case CMD_INT_X2:
         {
            int *ip = (int*)ptr;
            if(!ReadInt(&cl, ip[0]))
               goto bad_param;
            if(!ReadInt(&cl, ip[1]))
               goto bad_param;
         }
         break;
      case CMD_STRING:
         if(!ReadString(&cl, *(C_str*)ptr))
            goto bad_param;
         break;
      default:
         assert(0);
      }
                              //call (optional) callback
      t_Callback cb = sdp->Callback;
      if(cb){
         bool st = (this->*cb)(&cl);
         if(!st)
            return false;
      }
   }
   return true;
fail:
   DisplayError(C_fstr("Unknown command-line option: %s", keyword), app_name);
   return false;
bad_param:
   DisplayError(C_fstr("Bad parameters for command-line option: %s", keyword), app_name);
   return false;
}

//----------------------------
