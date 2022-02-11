
//----------------------------

class C_edit_Usability: public C_editor_item{
public:
   enum E_USABILITY_STAT{
      US_KEYHIT,                 //data: plugin + key combination
      US_MENUHIT,                //data: ''
      US_RELOAD,                 //data: mission name
      US_LOAD,                   //data: ''
      US_START,
      US_CLOSE,
      US_TOOLBAR,                //data: tooltip

      US_LAST
   };

   virtual void AddStats(E_USABILITY_STAT type,
      PC_editor_item plugin, dword action_id, IG_KEY key = K_NOKEY, dword key_modifier = 0) = 0;

   virtual bool IsFirstRunToday() const = 0;
};

//----------------------------