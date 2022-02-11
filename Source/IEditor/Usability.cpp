#include "all.h"
#include "Usability.h"


//----------------------------
// Usability statistics collector.
// Written by: Michal Bacik, 20.1.2003
//
// This plug-in is used for determining statistical behavior of users,
// for deducing ways how to improve user interface of the editor.
//
// Collected statistics:
//    - items invoked by keyboard, and by menu
//    - missions loaded/reloaded
//    - number of startups and successive closings of IEditor
//
//----------------------------

#define MAX_KEEP_DAYS 7       //max # of days we're keeping locally
#define MAX_WRITE_TOPS 8      //number of "tops" displayed in report
#define STORE_PAUSE (1000*60*3)  //period, how often we save stats to registry (due to crashes, etc)


#ifdef _DEBUG
//#define DEBUG_SEND_WHEN_CLOSE //debugging - send data when closing
#endif

//----------------------------

static const char REG_BASE[] = "Software\\Insanity3D system\\Usability1";
static const char REG_KEY_NAME_DAYS[] = "Day data";

//----------------------------

class C_edit_Usability_imp: public C_edit_Usability{
                              //returns plugin's name
   virtual const char *GetName() const{ return "Usability"; }

//----------------------------
                                 //sinle entry about particular statistics action
   struct S_ui{
      E_USABILITY_STAT type;     //type of action
      //C_str data;                //associated data
      //PC_editor_item plugin;
      C_str plugin_name;
      dword action_id;

      bool operator <(const S_ui &ui) const{
         if(type < ui.type) return true;
         if(type > ui.type) return false;

         if(action_id < ui.action_id) return true;
         if(action_id > ui.action_id) return false;

         return (plugin_name < ui.plugin_name);
      }
   };

   typedef map<S_ui, dword> t_one_day_stats;

                              //stats associated with single day
   struct S_day_data{
      int day, month, year;
      t_one_day_stats one_day_stats;

      S_day_data(){}
      S_day_data(int y, int m, int d):
         year(y),
         month(m),
         day(d)
      {}

      void Save(C_cache &ck) const{
                              //header
         ck.write((char*)&day, sizeof(dword));
         ck.write((char*)&month, sizeof(dword));
         ck.write((char*)&year, sizeof(dword));

         dword num = one_day_stats.size();
         ck.write((char*)&num, sizeof(dword));

         for(t_one_day_stats::const_iterator it = one_day_stats.begin(); it!=one_day_stats.end(); it++){
            const S_ui &ui = it->first;
            ck.write(&ui.type, sizeof(word));
            ck.write((const char*)ui.plugin_name, ui.plugin_name.Size()+1);
            ck.write(&ui.action_id, sizeof(dword));
            ck.write(&it->second, sizeof(dword));
         }
      }

      void Load(C_cache &ck){
                              //header
         ck.read((char*)&day, sizeof(dword));
         ck.read((char*)&month, sizeof(dword));
         ck.read((char*)&year, sizeof(dword));

         dword num = 0;
         ck.read((char*)&num, sizeof(dword));
         while(num--){
            S_ui ui;
            dword count;
            char buf[512];
            ui.type = US_LAST;
            ck.read(&ui.type, sizeof(word));
            ck.getline(buf, sizeof(buf), 0);
            ui.plugin_name = buf;
            ck.read(&ui.action_id, sizeof(dword));
            ck.read(&count, sizeof(dword));
            one_day_stats[ui] = count;
         }
      }
   };

                              //stats of all the days
   typedef C_vector<S_day_data> t_all_stats;
   t_all_stats all_days;

   bool init_ok;
   int last_try_send_hour;
   int last_store_count;
   bool first_run_today;

//----------------------------

   virtual void AddStats(E_USABILITY_STAT type, PC_editor_item plugin, dword action_id,
      IG_KEY key = K_NOKEY, dword key_modifier = 0){

      SYSTEMTIME st;
      GetLocalTime(&st);

      S_day_data *day;
      if(!all_days.size()){
                           //add 1st day
         all_days.push_back(S_day_data(st.wYear, st.wMonth, st.wDay));
         day = &all_days.back();
      }else{
         day = &all_days.back();
         if(day->day!=st.wDay || day->month!=st.wMonth || day->year!=st.wYear){
                              //day elapsed, adding new
            if(all_days.size() >= MAX_KEEP_DAYS)
               all_days.erase(all_days.begin());
            all_days.push_back(S_day_data(st.wYear, st.wMonth, st.wDay));

            day = &all_days.back();
         }
      }
                              //increase usage count for this data type
      S_ui ui;
      ui.type = type;
      ui.action_id = action_id;
      if(plugin)
         ui.plugin_name = plugin->GetName();
      dword num = ++day->one_day_stats[ui];
      if(type==US_MENUHIT && num==3 && key!=K_NOKEY){
         char key_name[32];
         ed->GetIGraph()->GetKeyName(key, key_name, sizeof(key_name));

         MessageBox((HWND)ed->GetIGraph()->GetHWND(),
            C_xstr(
               "The action you're frequently using has following keyboard shortcut: %#%#%#%"
               )
               %(key_modifier&SKEY_CTRL ? "Ctrl+" : "")
               %(key_modifier&SKEY_SHIFT ? "Shift+" : "")
               %(key_modifier&SKEY_ALT ? "Alt+" : "")
               %key_name
               ,
            "Editor information",
            MB_OK);
      }

                        //try to send older day's info each hour
      if(all_days.size() > 1){
         if(last_try_send_hour != st.wHour){
            SendData();
            last_try_send_hour = st.wHour;
         }
      }
   }

//----------------------------

   virtual bool IsFirstRunToday() const{

      return first_run_today;
   }

//----------------------------

   void SaveStats(){

      int h = RegkeyCreate(REG_BASE, E_REGKEY_CURRENT_USER);
      if(h!=-1){
         C_cache ck;
         byte *mem = NULL;
         dword sz = 0;
         dword num;
         dword i;
         ck.open(&mem, &sz, CACHE_WRITE_MEM);
         num = all_days.size();
         ck.write((char*)&num, sizeof(num));
         for(i=0; i<num; i++)
            all_days[i].Save(ck);
         ck.close();
         RegkeyWdata(h, REG_KEY_NAME_DAYS, mem, sz);
         ck.FreeMem(mem);

         RegkeyClose(h);
      }
   }

//----------------------------

   void SendData(){

      /*
      char user_name[256+1];
      user_name[0] = 0;
      if(!GetEnvironmentVariable("SSUSER", user_name, sizeof(user_name))){
         dword sz = 256;
         GetUserName(user_name, &sz);
      }
      if(!user_name[0])
         return;

      static const char path[] = {
         "\\\\Mike\\The Game\\Users\\%s\\%s.ieditor"
      };
      if(all_days.size() > 1){
         FILE *fp = fopen(C_fstr(path, user_name, "stats"), "at");
         if(fp){
            for(dword di = 0; di<all_days.size()-1; di++){
               const S_day_data &day = all_days[di];
               C_fstr s("Date: %2i. %2i. %4i\n", day.day, day.month, day.year);
               fwrite((const char*)s, 1, s.Size(), fp);

               const t_one_day_stats &ds = day.one_day_stats;
               t_one_day_stats::const_iterator it;

               dword total_count[US_LAST];
               memset(total_count, 0, sizeof(total_count));

               typedef C_vector<pair<S_ui, dword> > t_top;
               t_top tops[US_LAST];

               for(it=ds.begin(); it!=ds.end(); it++){
                  const S_ui &ui = it->first;
                  dword count = it->second;
                              //count totals for particular stats type
                  total_count[ui.type] += count;
                              //prepare for sorting of tops
                  tops[ui.type].push_back(pair<S_ui, dword>(ui, count));
               }
               struct S_hlp{
                  static bool cbSort(const pair<S_ui, dword> &p1, const pair<S_ui, dword> &p2){
                     return (p1.second < p2.second);
                  }
               };

               for(dword i=0; i<US_LAST; i++){
                  C_str l;
                  dword total = total_count[i];
                  bool show_tops = true;

                  switch(i){
                  case US_KEYHIT:
                     l = C_fstr("Key command count: %i", total);
                     break;
                  case US_MENUHIT:
                     l = C_fstr("Menu command count: %i", total);
                     break;
                  case US_RELOAD:
                     l = C_fstr("Reload count: %i", total);
                     break;
                  case US_LOAD:
                     l = C_fstr("Load count: %i", total);
                     break;
                  case US_START:
                     l = C_fstr("Start count: %i", total);
                     show_tops = false;
                     break;
                  case US_CLOSE:
                     l = C_fstr("Close count: %i", total);
                     show_tops = false;
                     break;
                  case US_TOOLBAR:
                     l = C_fstr("Toolbar use: %i", total);
                     break;
                  }
                  if(show_tops){
                     t_top &top = tops[i];
                     sort(top.begin(), top.end(), S_hlp::cbSort);
                     l += ", Tops: ";
                     int num_tops = top.size();
                     int min_top = Max(0, num_tops - MAX_WRITE_TOPS);
                     for(int i = num_tops-1; i >= min_top; i--){
                        const S_ui &ui = top[i].first;
                        //l += C_fstr("\"%s\"(%i)  ", (const char*)ui.data, top[i].second);
                        l += C_fstr("\"%s\"(%i)  ", (const char*)ui.plugin_name, top[i].second);
                     }
                  }
                  l += "\n";
                  fwrite((const char*)l, 1, l.Size(), fp);
               }
               fwrite("\n", 1, 1, fp);
            }
            all_days.erase(all_days.begin(), all_days.begin()+all_days.size()-1);
            fclose(fp);
         }else{
                              //failed to open file
         }
      }
      */
   }

//----------------------------

   void LoadStats(){

      int h = RegkeyOpen(REG_BASE, E_REGKEY_CURRENT_USER);
      if(h!=-1){
         dword sz;
         sz = RegkeyDataSize(h, REG_KEY_NAME_DAYS);
         if(sz!=0xffffffff){
            byte *buf = new byte[sz];
            RegkeyRdata(h, REG_KEY_NAME_DAYS, buf, sz);
            C_cache ck;
            ck.open(&buf, &sz, CACHE_READ_MEM);

            dword num = 0;
            ck.read((char*)&num, sizeof(num));

            all_days.assign(num, S_day_data());
            for(dword i=0; i<num; i++)
               all_days[i].Load(ck);

            ck.close();
            delete[] buf;

            if(num){
               SYSTEMTIME st;
               GetLocalTime(&st);
               const S_day_data *day = &all_days.back();
               if(day->day==st.wDay && day->month==st.wMonth && day->year==st.wYear)
                  first_run_today = false;
            }
         }

         RegkeyClose(h);
      }
   }

//----------------------------

public:
   C_edit_Usability_imp():
      init_ok(false),
      last_try_send_hour(-1),
      last_store_count(STORE_PAUSE),
      first_run_today(true)
   {}

   virtual bool Init(){

      LoadStats();
      init_ok = true;
      AddStats(US_START, NULL, 0);
      return true;
   }

//----------------------------

   virtual void Close(){

      if(init_ok){
         AddStats(US_CLOSE, NULL, 0);
#ifdef DEBUG_SEND_WHEN_CLOSE
         all_days.push_back(S_day_data(0, 0, 0));
         SendData();
         all_days.clear();
#endif
         SaveStats();
         init_ok = false;
      }
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if((last_store_count -= time) <= 0){
         last_store_count = STORE_PAUSE;
         SaveStats();
      }
   }

//----------------------------
   /*
   virtual dword Action(int id, void *context){

      switch(id){
      case E_USABILITY_KEYHIT: AddStats(US_KEYHIT, (const char*)context); break;
      case E_USABILITY_MENUHIT: AddStats(US_MENUHIT, (const char*)context); break;
      case E_USABILITY_RELOAD: AddStats(US_RELOAD, (const char*)context); break;
      case E_USABILITY_LOAD: AddStats(US_LOAD, (const char*)context); break;
      case E_USABILITY_TOOLBAR: AddStats(US_TOOLBAR, (const char*)context); break;
      }
      return 0;
   }
   */

//----------------------------
                              //called for registered plugins during rendering
   //virtual void Render(){}
                              //store/restore plugin's state
   //virtual bool LoadState(C_chunk &ck){ return false; }
   //virtual bool SaveState(C_chunk &ck) const{ return false; }
};

//----------------------------

void CreateUsability(PC_editor editor){
   C_edit_Usability_imp *us = new C_edit_Usability_imp; editor->InstallPlugin(us); us->Release();    
}

//----------------------------
