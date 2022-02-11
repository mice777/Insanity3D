#include "all.h"
#include "..\..\Source\IEditor\resource.h"
#include "SysTables.h"
#include "Main.h"
#include "gamemission.h"

//----------------------------

static const C_table_element te_mat_table[] = {
   {TE_ARRAY, TAB_S32_MAT_NAME,     "Materials", MAX_MATERIALS},
      {TE_BRANCH, TAB_S32_MAT_NAME, NULL, 7, (dword)"%[0]"},
         {TE_STRING, TAB_S32_MAT_NAME, "Friendly name", 32, 0, 0, "Name of material, as presented to user in various places."},
         {TE_ENUM, TAB_E_MAT_CATEGORY, "Category", (dword)
            "Default\0"
            "Glass\0"
            "Stairs\0"
            "Border\0"
            "Callback\0"
            "SwimSurface\0"
            "WaterSurface\0"
            "Wound\0"
            "DrownSurface\0"
            "Ladder\0",
            0, 0, "Material category - this is used for definition of special behavior of material."
         },
         {TE_FLOAT, TAB_F_MAT_DENSITY, "Density", 0, 0, 1000,
            "Density of material, used for physics. The units are in kg/m^3."},
         {TE_FLOAT, TAB_F_MAT_FRICTION, "Friction", 0, 1, 1.0f, "Friction, by which this material applies onto physics objects. Set to 0 to simulate gliding (ice-like) surfaces, set to 1 to simulate rough surfaces."},
         {TE_INT, TAB_I_MAT_SOUND_LIKE, "Sounds like", 0, 0, -1, "Index of other material, how this material should sound like. Set to -1 to make this material 'use itself'. Set to index of some other material to use all sound settings from the other mat."},
         {TE_COLOR_RGB, TAB_F_MAT_DEBUG_COLOR, "Debug color", 0x800080, 0, 0, "Material visualization color in editor."},
         {TE_INT, TAB_I_MAT_DEBUG_ALPHA, "Debug alpha", 0, 100, 60, "Material visualization opacity."},

   {TE_ARRAY, TAB_S24_MAT_MODELSET_NAME, "TerrainDetail models", 20, 0, 0, "Settings of models, which are usable as Terrain detail sets in missions."},
      {TE_ARRAY, TAB_S24_MAT_MODELSET_NAME, "Set", TAB_MAT_NUM_MODELS_IN_SET, (dword)"%[0]", 0,
         "Single set. The first item in the array specifies name of set, and others items specify actual models, from which the set is made of."},
         {TE_BRANCH, TAB_S24_MAT_MODELSET_NAME, NULL, 3, (dword)"%[0]  (%[1])", 0,
            "Setting of single model (or set if it is first item in array)."},
            {TE_STRING, TAB_S24_MAT_MODELSET_NAME, "Name:", 24, 0, 0,
               "Name of model (or set). Model name may be among models, or missions (if it is a mission model, you'll need to add '+' mark in front of name). You may also make reference to other set, instead of specifying name of model here. To do so, type '#n', where 'n' is replaced by index of set you want to reference. This way you can make composites of sets by merging multiple sets together."},
            {TE_FLOAT, TAB_F_MAT_MODELSET_RATIO, "Use ratio:", 0, 1, 1,
               "For Set name (1st item in array):\n"
               "This is the density of set.\n"
               "\n"
               "For model name:\n"
               "This is the ratio, by which the model is selected. If there're more models in a set, the chance one will be picked is greater if this ratio is higher than others."},
            {TE_BOOL, TAB_B_MAT_MODELSET_ALIGN, "Normal align:", 0, 0, 0,
               "If this is set, the model will be aligned to face normal."},
         
   {TE_NULL}
};
static const C_table_template templ_mat_table = { "Material table", te_mat_table};

PC_table tab_materials;

//----------------------------

#define MAX_SOUND_ENVIRONMENTS 32

static const C_table_element te_snd_envs[] = {
   {TE_ARRAY, I3D_SENV_S_NAME, "Environments", MAX_SOUND_ENVIRONMENTS, 0, 0, "Sound environment sets. Each sets define sound parameters in particular room (or sector). The sound settings is applied by selection of sound set in properties of Sector."},
      {TE_BRANCH, I3D_SENV_S_NAME, NULL, 6, (dword)"%[0]"},
         {TE_STRING, I3D_SENV_S_NAME, "Name", 24, 0, 0, "Friedly name of sound set. This name will be displayed as selection in Sector Properties."},
         {TE_BRANCH, 0, "Room level", 2, (dword)"LF: %[0] HF: %[1]", 0, "Master volume for reflected sounds (both early reflections and reverbations) that are added to sounds.\nThis is the amount of reflections and reverbations added to the sound mix."},
            {TE_FLOAT, I3D_SENV_F_ROOM_LEVEL_LF, "Low frequency", 0, 1, .75f, "Room level applied at high frequencies."},
            {TE_FLOAT, I3D_SENV_F_ROOM_LEVEL_HF, "High frequency", 0, 1, .75f, "Room level applied at low frequencies."},

         {TE_BRANCH, 0, "Decay", 2, (dword)"Time: %[0] ms HF: %[1]", 0, "Reverbation decay time. "},
            {TE_INT, I3D_SENV_I_REVERB_DECAY_TIME, "Time (msec)", 100, 20000, 1500, "Decay time. Value set to 100 is for small rooms with very dead surfaces, value set to 20000 is for large room with very live surfaces."},
            {TE_FLOAT, I3D_SENV_F_DECAY_HF_RATIO, "HF ratio", 0, 1, .75f, "Ratio of spectral quality of reverbation decay."},

         {TE_BRANCH, 0, "Reflections", 2, (dword)"Ratio: %[0] Delay: %[1] ms", 0, "Overall amount of initial reflections relative to the Room property."},
            {TE_FLOAT, I3D_SENV_F_REFLECTIONS_RATIO, "Ratio", 0, 1, .75f, "The Reflections ratio. This property doesn't affect the subsequent reverbation decay.\nBy increasing the amount of initial reflections, you can simulate a narrow space with closer walls, especially effective if you associate the initial reflections increase with a reduction in Reflections Delay.\nTo simulate open or semi-open environments, you can maintain the amount of early reflections while reducing the value of Reverb property, which controls the amount of later reflections."},
            {TE_INT, I3D_SENV_I_REFLECTIONS_DELAY, "Delay (msec)", 0, 300, 7, "Amount of delay from the arrival of time of the direct path to the first reflection from the source. You can reduce or increase this property to simulate closer or more distant reflective surfaces - and therefore control the perceived size of the room."},

         {TE_BRANCH, 0, "Reverbation", 2, (dword)"Level: %[0] Delay: %[1] ms", 0, "Overall amount of later reverbation relative to the Room property.\nNote that Reverb and Decay Time are independent properties - if you adjust Decay time without changing Reverb, the total intensity of the late reverbation remains constant."},
            {TE_FLOAT, I3D_SENV_F_REVERB_RATIO, "Level", 0, 1, .75f, "Overall amount of later reverbation relative to the Room property."},
            {TE_INT, I3D_SENV_I_REVERB_DELAY, "Delay (msec)", 0, 100, 10, "Begin time of the late reverbation relative to the time of initial reflection (the first of the early reflections). Redusing or increasing this value is useful for simulating a smaller or larger room."},
   
         {TE_FLOAT, I3D_SENV_F_ENV_DIFFUSION, "Env. diffusion", 0, 1, 1.0f, "Echo density in reverbation decay.\nValue 1.0 provides highest density. Reducin the density gives the reverbation a more 'grainy' character. If you set the value to 0.0, the reverbation decay sounds like a succession of distinct echoes."},
         /*
         {TE_INT, I3D_SENV_I_SWITCH_FADE, "Fade", 0, 20000, 1000},
         {TE_BRANCH, 0, "Emulation (non-EAX)", 1, (dword)"Volume %[0]"},
            {TE_FLOAT, I3D_SENV_F_EMUL_VOLUME, "Volume", 0, 1, 1},
            */
   {TE_NULL}
};

static const C_table_template templ_sound_envs = { "Sound environments", te_snd_envs };

PC_table tab_sound_envs;

void ApplySoundEnvTable(){

   PC_table tab = tab_sound_envs;
   for(int i=tab->ArrayLen(I3D_SENV_S_NAME); i--; ){
      const char *name = tab->ItemS(I3D_SENV_S_NAME, i);
      if(*name){
         driver->SetSoundEnvData(i, I3D_SENV_S_NAME, (dword)name);
         static const I3D_SOUND_ENV_DATA_INDEX indxs[] = {
            I3D_SENV_F_ROOM_LEVEL_LF, I3D_SENV_F_ROOM_LEVEL_HF, I3D_SENV_I_REVERB_DECAY_TIME,
            I3D_SENV_F_DECAY_HF_RATIO, I3D_SENV_F_REFLECTIONS_RATIO, I3D_SENV_I_REFLECTIONS_DELAY,
            I3D_SENV_F_REVERB_RATIO, I3D_SENV_I_REVERB_DELAY, I3D_SENV_F_ENV_DIFFUSION,
            //I3D_SENV_F_EMUL_VOLUME,
         };
         for(int j=sizeof(indxs)/sizeof(I3D_SOUND_ENV_DATA_INDEX); j--; ){
            dword data;
            I3D_SOUND_ENV_DATA_INDEX id = indxs[j];
            switch(id){
            case I3D_SENV_F_ROOM_LEVEL_LF: case I3D_SENV_F_ROOM_LEVEL_HF:
            case I3D_SENV_F_DECAY_HF_RATIO:
            case I3D_SENV_F_REFLECTIONS_RATIO:
            case I3D_SENV_F_REVERB_RATIO:
            case I3D_SENV_F_ENV_DIFFUSION:
               data = I3DFloatAsInt(tab->ItemF(id, i));
               break;
            case I3D_SENV_I_REVERB_DECAY_TIME:
            case I3D_SENV_I_REFLECTIONS_DELAY:
            case I3D_SENV_I_REVERB_DELAY:
               data = tab->ItemI(id, i);
               break;
            default: assert(0); data = 0;
            }
            driver->SetSoundEnvData(i, id, data);
         }
      }
   }
#ifdef EDITOR
   if(editor){
                              //build table pairs
      typedef pair<int, const char*> t_mp;
      C_vector<t_mp> pairs;
      for(dword i=0; i<tab->ArrayLen(I3D_SENV_S_NAME); i++){
         const char *name = tab->ItemS(I3D_SENV_S_NAME, i);
         if(*name)
            pairs.push_back(t_mp(i, name));
      }
      pairs.push_back(t_mp(-1, NULL));
      editor->BroadcastAction(EDIT_ACTION_FEED_ENVIRONMENTS, &(*pairs.begin()));
   }
#endif
}

//----------------------------

enum E_TAB_EDIT_INDEX{
   TAB_MATERIALS,
   //TAB_VEHICLE,
   TAB_GAME_CFG,
   TAB_INVENTORY,
   TAB_ANIMALS,
   TAB_ATTACKER,
   TAB_SOUND_ENVS,
   TAB_MISSION_CFG,
   TAB_LAST
};

static const struct{
   PC_table *tab;
   const C_table_template *templ;
   const char *tab_name;
} tab_info[] = {
   {&tab_materials,     &templ_mat_table,       "Materials.tab"},
   {&tab_sound_envs,    &templ_sound_envs,      "Sound_envs.tab"},
   {NULL}
};

//----------------------------
// Update material table into editor.
void ApplyMaterialTable(){

#ifdef EDITOR
   PC_table tab = tab_materials;
   if(editor){
                              //build material pairs
      typedef pair<int, const char*> t_mp;
      C_vector<t_mp> mat_pairs;
      for(dword i=0; i<tab->ArrayLen(TAB_S32_MAT_NAME); i++){
         const char *mat_name = tab->ItemS(TAB_S32_MAT_NAME, i);
         if(*mat_name)
            mat_pairs.push_back(t_mp(i, mat_name));
      }
      mat_pairs.push_back(t_mp(-1, NULL));
      editor->BroadcastAction(EDIT_ACTION_FEED_MATERIALS, &(*mat_pairs.begin()));
   }
                              //apply visualization color
   {
      driver->ClearCollisionMaterials();

      struct S_rgb{ byte c[3]; } *rgb = (S_rgb*)tab->Item(TAB_F_MAT_DEBUG_COLOR);
      for(dword i=0; i<tab->ArrayLen(TAB_S32_MAT_NAME); i++){
         const char *name = tab->ItemS(TAB_S32_MAT_NAME, i);
         if(*name){
            const S_rgb &c = rgb[i];

            I3D_collision_mat mi;
            mi.color = (c.c[0]<<16) | (c.c[1]<<8) | c.c[2];
            dword alpha = tab->ItemI(TAB_I_MAT_DEBUG_ALPHA, i);
            alpha = alpha * 255 / 100;
            mi.color |= alpha<<24;
            mi.name = name;
            driver->SetCollisionMaterial(i, mi);
         }
      }
   }
#endif//EDITOR
}

//----------------------------

extern const C_table_template templ_mission_cfg;

static const C_table_element te_miss_cfg[] = {
   {TE_BRANCH, 0, "Weather", 2, 0, 0, "Setting of weather system in the mission. This allows setting of particular effect (e.g. snow, rain, etc), and additional parameters like density."},
      {TE_ENUM, C_game_mission::TAB_E_WEATHER_EFFECT, "Effect", (dword)"No\0Rain\0Snow\0", 0, 0, "Weather effect in the mission."},
      {TE_INT, C_game_mission::TAB_I_WEATHER_DENSITY, "Density", 1, 500, 80,
         "Number of displayed elements."},
      //{TE_FLOAT, C_game_mission::TAB_F_WEATHER_MAX_WIND, "Max wind", 0, 50, 0, "Wind-power which affect the weather effect (max wind speed, in m/s)"},
      //{TE_FLOAT, C_game_mission::TAB_F_WEATHER_WIND_CHANGE_RATE, "Wind change rate", 0, 1, 0, "Weather wind change speed."},
      //{TE_INT, C_game_mission::TAB_I_ENVIROMENT_TEMPERATURE, "Enviroment temperature", -30, 40, 20, "Enviromet temperature affects many features (breath smoke, flies...)"},
      //{TE_FLOAT, C_game_mission::TAB_F_LIGHTING_SRIKE, "Lighting strike frequention", 0, 1, 0, "How often the ground will be striked by lighting bolt. (frequtntion is the number of lightings per minute)"},
   {TE_BRANCH, 0, "Terrain detail", 4, (dword)"%[0]", 0, "Setting of terrain detail in the mission. This setting uses model sets defined in material table."},
      {TE_STRING, C_game_mission::TAB_S20_TERRDET_MASK, "Visual mask", 20, 0, 0, "Name of visuals on which Terrain Detail will be applied. Only visuals which name matches this mask will be used.\nYou may use wildchars (* and ?)."},
      {TE_BOOL, C_game_mission::TAB_B_TERRDET_USE, "Use", 0, 0, 1, "Enable or disable usage of Terrain Detail system."},
      {TE_FLOAT, C_game_mission::TAB_F_TERRDET_VISIBILITY, "Visible distance", 1, 100, 20, "Distance from camera, until which Terrain Detail system will be applied."},
      {TE_ARRAY, C_game_mission::TAB_S20_TERRDET_MATMASK, "Material sets", 16, 0, 0, "Material sets - assignment of model sets to particular materials."},
         {TE_BRANCH, C_game_mission::TAB_S20_TERRDET_MATMASK, NULL, 2, (dword)"%[0]  (%[1])"},
            {TE_STRING, C_game_mission::TAB_S20_TERRDET_MATMASK, "Material mask", 20, 0, 0, "Name of material where model set will be applied.\nYou may use wildchars (* and ?)."},
            {TE_ENUM, C_game_mission::TAB_E_TERRDET_MODELSET, "Model set", NULL, 0, 0, "Model set. These sets are defined in material table and are common for all missions."},

   TE_NULL
};

C_table_template const templ_mission_cfg = { "Mission config", te_miss_cfg };

//----------------------------
//----------------------------

#ifdef EDITOR

#include <windows.h>
#include <commctrl.h>
#include <insanity\sourcecontrol.h>

//----------------------------

static const struct S_tabedit_column{
   const char *title;
   dword width;
} tablist_list[] = {
   {"Table", 90},
   {"Desciption", 400},
};

//----------------------------
                              //editor plugin: SysConfig
class C_edit_SysConfig: public C_editor_item{
   virtual const char *GetName() const{ return "SysConfig"; }

   C_smart_ptr<C_game_mission> mission;
   C_smart_ptr<C_editor_item_Properties> e_props;

   void *hwnd;
   int pos_size[4];
   int last_edited_table;

   E_TAB_EDIT_INDEX curr_table;

   static void TABAPI cbTab(PC_table tab, dword msg, dword cb_user, dword prm2, dword prm3){
      C_edit_SysConfig *ec = (C_edit_SysConfig*)cb_user;
      switch(msg) {

      case TCM_CLOSE:
         {
            igraph->RemoveDlgHWND(ec->hwnd);
            ec->hwnd = NULL;
/*
            if(tab->IsModified())
               tab->Save((dword)"tables\\sys_cfg.tab", TABOPEN_FILENAME);
*/
            int *pos_size = (int*)prm2;
            memcpy(ec->pos_size, pos_size, sizeof(ec->pos_size));
            ec->curr_table = TAB_LAST;
         }
         break;

      case TCM_MODIFY:
         switch(ec->curr_table){
         case TAB_MISSION_CFG:
            ec->ed->SetModified();
            break;
         case TAB_MATERIALS:
            ApplyMaterialTable();
            break;
         case TAB_SOUND_ENVS:
            ApplySoundEnvTable();
            break;
         }
         break;

      case TCM_GET_ENUM_STRING:
         switch(ec->curr_table){
         case TAB_MISSION_CFG:
            switch(prm2){
            case C_game_mission::TAB_E_TERRDET_MODELSET:
               {
                              //generate enum string from model sets
                  C_str &s = *(C_str*)prm3;
                  C_vector<char> buf;
                  int num_sets = tab_materials->ArrayLen(TAB_S24_MAT_MODELSET_NAME) / TAB_MAT_NUM_MODELS_IN_SET;
                  for(int i=0; i<num_sets; i++){
                     const char *set_name = tab_materials->ItemS(TAB_S24_MAT_MODELSET_NAME, i * TAB_MAT_NUM_MODELS_IN_SET);
                     if(!*set_name)
                        set_name = "-";
                     buf.insert(buf.end(), set_name, set_name+strlen(set_name)+1);
                  }
                              //strip unused ones from end
                  while(buf.size() && buf.end()[-2]=='-'){
                     buf.pop_back(); buf.pop_back();
                  }
                  buf.push_back(0);
                  s.Assign(&(*buf.begin()), buf.size());
               }
               break;
            default: assert(0);
            }
            break;
         }
         break;
      }
   }

//----------------------------

   void EditTable(E_TAB_EDIT_INDEX tt){

      if(!hwnd){
         PC_table tab = NULL;
         const C_table_template *templ = NULL;
         switch(tt){
         case TAB_MISSION_CFG:
            if(ed->CanModify()){
               tab = const_cast<PC_table>(mission->GetConfigTab()); templ = &templ_mission_cfg;
            }
            break;
         default:
            tab = *tab_info[tt].tab;
            templ = tab_info[tt].templ;
         }
         if(!tab) return;

         curr_table = tt;
         const dword flags = TABEDIT_CENTER | TABEDIT_MODELESS | TABEDIT_IMPORT | TABEDIT_EXPORT;
         hwnd = tab->Edit(templ,
            igraph->GetHWND(), cbTab, (dword)this,
            flags,
            pos_size);
         if(hwnd) igraph->AddDlgHWND(hwnd);
      }
   }

//----------------------------

   HWND hwnd_list;            //handle of sheet containing list of tables
   bool in_prop;

//----------------------------

   static void SetSSButtonState(HWND hwnd){

      bool en_out = false;
      bool en_in = false;
      int sel_i = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETSELECTIONMARK, 0, 0);
      if(sel_i!=-1){
         en_out = OsIsFileReadOnly(C_fstr("Tables\\%s", tab_info[sel_i].tab_name));
         en_in = !en_out;
      }
      EnableWindow(GetDlgItem(hwnd, IDC_TABEDIT_CK_OUT), en_out);
      EnableWindow(GetDlgItem(hwnd, IDC_TABEDIT_CK_IN), en_in);
   }

//----------------------------

   void InitTabEditList(HWND hwnd) const{

      SendDlgItemMessage(hwnd, IDC_LIST, LVM_DELETEALLITEMS, 0, 0);
                        //make columns
      for(int i=0; i < (sizeof(tablist_list)/sizeof(tablist_list[0])); i++){
         LVCOLUMN lvc;
         memset(&lvc, 0, sizeof(lvc));
         lvc.mask = LVCF_WIDTH;
         lvc.cx = tablist_list[i].width;
         if(tablist_list[i].title){
            lvc.pszText = (char*)tablist_list[i].title;
            lvc.cchTextMax = strlen(tablist_list[i].title);
            lvc.mask |= LVCF_TEXT;
         }
         SendDlgItemMessage(hwnd, IDC_LIST, LVM_INSERTCOLUMN, i , (dword)&lvc);
      }
      SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETEXTENDEDLISTVIEWSTYLE,
         LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES, 
         LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

      static const char *tab_names[][2] = {
         {"Materials", "Materials, material sounds, modeldetail sets"},
         {"Sound envs", "Sound environment sets, applied to sectors"},
         NULL
      };
      LVITEM lvi;
      memset(&lvi, 0, sizeof(lvi));
      for(i=0; tab_names[i][0]; i++){
         lvi.mask = LVIF_STATE;
         lvi.iSubItem = 0;
         lvi.iItem = i;
         lvi.state = 0;
         if(i==last_edited_table)
            lvi.state = LVIS_FOCUSED | LVIS_SELECTED;
         SendDlgItemMessage(hwnd, IDC_LIST, LVM_INSERTITEM, 0, (LPARAM)&lvi);
         lvi.mask = LVIF_TEXT;
         {
            bool rd_only = OsIsFileReadOnly(C_fstr("Tables\\%s", tab_info[i].tab_name));
            C_fstr txt("%s%s", rd_only ? "" : "* ", tab_names[i][0]);
            lvi.iSubItem = 0;
            lvi.pszText = (char*)(const char*)txt;
            lvi.cchTextMax = strlen(lvi.pszText);
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETITEM, 0, (LPARAM)&lvi);
         }
         {
            lvi.iSubItem = 1;
            lvi.pszText = (char*)tab_names[i][1];
            lvi.cchTextMax = strlen(lvi.pszText);
            SendDlgItemMessage(hwnd, IDC_LIST, LVM_SETITEM, 0, (LPARAM)&lvi);
         }
      }
   }

//----------------------------

   bool dlgSheet(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){

      case WM_INITDIALOG:
         {
            InitTabEditList(hwnd);
            SetSSButtonState(hwnd);
         }
         return 1;

      case WM_NOTIFY:
         {
            LPNMHDR pnmh = (LPNMHDR)lParam; 
            switch(pnmh->code){
            case LVN_KEYDOWN:
               {
                  LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN)lParam; 
                  switch(pnkd->wVKey){
                  case VK_SPACE:
                     EditTable((E_TAB_EDIT_INDEX)SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETSELECTIONMARK, 0, 0));
                     break;
                  }
               }
               break;

            case NM_DBLCLK :
               EditTable((E_TAB_EDIT_INDEX)SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETSELECTIONMARK, 0, 0));
               break;

            case LVN_ITEMCHANGED:
               {
                  NMLISTVIEW *lv = (NMLISTVIEW*)lParam;
                  if(lv->uChanged&LVIF_STATE){
                     last_edited_table = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETSELECTIONMARK, 0, 0);
                  }
                  SetSSButtonState(hwnd);
               }
               break;

            case NM_CLICK:
               {
                  last_edited_table = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETSELECTIONMARK, 0, 0);
                  SetSSButtonState(hwnd);
               }
               break;
            }
         }
         break;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDC_TABEDIT_CLOSE:
            {
               e_props->RemoveSheet(hwnd);
               in_prop = false;
            }
            break;

         case IDCANCEL:
            SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
            break;

         case IDC_TABEDIT_CK_OUT:
         case IDC_TABEDIT_CK_IN:
            {
               bool out = (LOWORD(wParam)==IDC_TABEDIT_CK_OUT);
               int sel_i = SendDlgItemMessage(hwnd, IDC_LIST, LVM_GETSELECTIONMARK, 0, 0);
               if(sel_i==-1)
                  break;
               C_fstr tab_name("Tables\\%s", tab_info[sel_i].tab_name);
               bool ok = false;
               PC_table tab = *tab_info[sel_i].tab;
               if(out){
                  ok = SCCheckOutFile("Insanity3d\\Editor", tab_name);
                              //re-load table
                  tab->Load((const char*)tab_name, TABOPEN_FILENAME | TABOPEN_UPDATE);
               }else{
                              //save table (if necessary)
                  if(tab->IsModified()){
                     tab->Save((const char*)tab_name, TABOPEN_FILENAME);
                  }
                  ok = SCCheckInFile("Insanity3d\\Editor", tab_name, false);
               }
               if(ok){
                  ed->Message(C_fstr("File '%s' checked %s.", (const char*)tab_name, out ? "out" : "in"));
                  InitTabEditList(hwnd);
                  SetSSButtonState(hwnd);
               }
            }
            break;
         }
         break;
      }
      return 0;
   }

   static BOOL CALLBACK dlgSheet_thunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      C_edit_SysConfig *ei = (C_edit_SysConfig*)GetWindowLong(hwnd, GWL_USERDATA);
      if(!ei){
         if(uMsg!=WM_INITDIALOG)
            return 0;
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
         ei = (C_edit_SysConfig*)lParam;
         assert(ei);
      }
      return ei->dlgSheet(hwnd, uMsg, wParam, lParam);
   }

//----------------------------

public:

   C_edit_SysConfig(C_game_mission &m1):
      mission(&m1),
      curr_table(TAB_LAST),
      last_edited_table(0),
      hwnd_list(NULL),
      in_prop(false),
      hwnd(NULL)
   {
      pos_size[0] = 0;
      pos_size[1] = 0;
      pos_size[2] = 80;
      pos_size[3] = 400;
   }

//----------------------------

   virtual bool Init(){
      e_props = (PC_editor_item_Properties)ed->FindPlugin("Properties");
      if(!e_props)
         return false;

      ed->AddShortcut(this, 1000, "%0 &File\\%31 Table editor\tCtrl+T", K_T, SKEY_CTRL);

      ed->AddShortcut(this, TAB_GAME_CFG, "%0 &File\\%31 Edit table\\&Game config\tCtrl+Shift+Alt+G", K_G, SKEY_SHIFT|SKEY_CTRL|SKEY_ALT);
      ed->AddShortcut(this, TAB_MISSION_CFG, "%0 &File\\%31 Edit table\\M&ission config\tCtrl+Enter", K_ENTER, SKEY_CTRL);

      /*
      {
                              //initialize toolbar
         PC_toolbar tb = ed->GetToolbar("Standard");
         S_toolbar_button tbs[] = {
            {1000,  7, "Table editor"},
            {0, -1},
         };
         tb->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_FILE", GetModuleHandle(NULL), 1);
      }
      */
      return true;
   }

//----------------------------

   virtual void Close(){
      if(hwnd)
         OsDestroyWindow(hwnd);
      if(hwnd_list){
         if(in_prop){
            e_props->RemoveSheet(hwnd_list);
            in_prop = false;
         }
         DestroyWindow(hwnd_list);
         hwnd_list = NULL;
      }
   }

   virtual dword Action(int id, void *context) {

      if(id>=0 && id<TAB_LAST){
         EditTable((E_TAB_EDIT_INDEX)id);
         return 0;
      }
      switch(id){
      case 1000:
         {
            if(in_prop){
               //e_props->RemoveSheet(hwnd_list);
               //in_prop = false;
               e_props->ShowSheet(hwnd_list);
               e_props->Activate();
               break;
            }
                              //load dialog and add it
            if(!hwnd_list){
               hwnd_list = CreateDialogParam(GetModuleHandle(NULL), "IDD_TAB_EDIT", (HWND)ed->GetIGraph()->GetHWND(),
                  dlgSheet_thunk, (LPARAM)this);
               if(!hwnd_list)
                  break;
            }
            e_props->AddSheet(hwnd_list, true);
            SetFocus(hwnd_list);
            in_prop = true;
         }
         break;
      }
      return 0;
   }

   bool LoadState(C_chunk &ck){ 
      ck.Read(pos_size, sizeof(pos_size));
      ck.Read(&last_edited_table, sizeof(last_edited_table));
      return true; 
   }

   bool SaveState(C_chunk &ck) const{ 
      ck.Write(pos_size, sizeof(pos_size));
      ck.Write(&last_edited_table, sizeof(last_edited_table));
      return true; 
   }
};

//----------------------------
// Install sysconfig plugin.
void InitSysConfigPlugin(PC_editor editor, C_game_mission &m1){
   C_edit_SysConfig *em = new C_edit_SysConfig(m1); editor->InstallPlugin(em); em->Release();
}

#endif //EDITOR


//----------------------------

void TablesCloseAll(){

   for(int i=0; tab_info[i].tab; i++){
      PC_table tab = *tab_info[i].tab;
      if(!tab) continue;
#ifdef EDITOR
      if(tab->IsModified()){
         C_fstr tab_name("tables\\%s", tab_info[i].tab_name);
         while(true){
            bool ok = tab->Save((const char*)tab_name, TABOPEN_FILENAME);
            if(ok)
               break;
                              //failed to save table, issue warning
            int i = OsMessageBox(NULL, tab_name, "Failed to save table:", MBOX_RETRYCANCEL);
            if(i != MBOX_RETURN_YES)         //no retry?
               break;
         }
      }
#endif
      tab->Release();
      *tab_info[i].tab = NULL;
   }
}

//----------------------------

bool TablesOpenAll(){

   for(int i=0; tab_info[i].tab; i++){
      PC_table tab = CreateTable();
      *tab_info[i].tab = tab;
      tab->Load(tab_info[i].templ, TABOPEN_TEMPLATE);
      if(tab_info[i].tab_name)
         tab->Load((const char*)C_fstr("tables\\%s", tab_info[i].tab_name), TABOPEN_FILENAME | TABOPEN_UPDATE);
   }
   return true;
}

//----------------------------
//----------------------------
