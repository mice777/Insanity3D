#include "all.h"
#include "common.h"
#pragma warning(push,2)
#include <deque>
#pragma warning(pop)

//----------------------------

class C_edit_Undo_imp: public C_editor_item_Undo{
   virtual const char *GetName() const{ return "Undo"; }

//----------------------------

   enum E_ACTION_UNDO{           
      E_UNDO_UNDO,               // - undo until break
      E_UNDO_REDO,               // - redo until break
      E_UNDO_SAVE,               //(S_edit_Undo_2*) - save undo information
   };

//----------------------------
                              //internal undo/redo buffers
   struct S_undo_entry{
      PC_editor_item ei;      //NULL = break
      dword undo_id;
      C_buffer<byte> data;
      bool may_collapse;      //true if entry may be merged with another entry
      bool new_style;
      C_smart_ptr<I3D_frame> frm;
      S_undo_entry():
         ei(NULL),
         new_style(false),
         may_collapse(true)
      {}
      inline bool operator <(const S_undo_entry &ue) const{ return (this < &ue); }
   };
   C_smart_ptr<C_toolbar> toolbar;

   deque<S_undo_entry> undo_list[2];  //undo=0, redo=1
   bool in_undo;              //true when un-doing
   bool in_redo;              //true when re-doing

   bool in_begin;             //true during entering undo info
   C_chunk feed_chunk;
   byte *feed_buf;
   dword feed_size;

//----------------------------
                              //check if top entry has same params like this,
                              // delete it if true
   void CheckTop(const S_undo_entry &ue1){

      if(undo_list[in_undo].size()){
         S_undo_entry &ue = undo_list[in_undo].back();
         if(ue.may_collapse &&
            ue.ei==ue1.ei &&
            ue.undo_id==ue1.undo_id){

            undo_list[in_undo].pop_back();
         }
      }
   }

//----------------------------

   virtual C_chunk &Begin(PC_editor_item ei, dword undo_id, PI3D_frame frm){

      assert(!in_begin);
      in_begin = true;

      undo_list[in_undo].push_back(S_undo_entry());
      S_undo_entry &ue = undo_list[in_undo].back();
      ue.ei = ei;
      ue.new_style = true;
      ue.undo_id = undo_id;
      ue.frm = frm;

                              //clear redo list
      if(!in_undo && !in_redo)
         undo_list[1].clear();

                              //open feeding chunk
      feed_chunk.WOpen(&feed_buf, &feed_size);

      return feed_chunk;
   }

//----------------------------
// Finish storing.
   virtual void End(){

      assert(in_begin);
      in_begin = false;
                              //close main chunk
      feed_chunk.Close();
                              //store data
      S_undo_entry &ue = undo_list[in_undo].back();
      ue.data.assign(feed_buf, feed_buf+feed_size);
      feed_chunk.GetHandle()->FreeMem(feed_buf);
      feed_buf = NULL;
   }

//----------------------------

   virtual void Save(PC_editor_item ei, dword action_id, const void *buf, dword buf_size){

      //C_chunk &ck = Begin(ei, undo_id, NULL);
      //ck.Write(buf, buf_size);
      //End();

      undo_list[in_undo].push_back(S_undo_entry());
      S_undo_entry &ue = undo_list[in_undo].back();
      ue.ei = ei;
      ue.undo_id = action_id;
      ue.data.assign((byte*)buf, (byte*)buf + buf_size);

                              //clear redo list
      if(!in_undo && !in_redo)
         undo_list[1].clear();
   }

//----------------------------

   virtual bool IsTopEntry(PC_editor_item ei, dword undo_id, C_chunk *ck) const{

      if(undo_list[in_undo].size()){
         const S_undo_entry *ue = &undo_list[in_undo].back();
         if(!ue->ei)
            if(undo_list[in_undo].size()>=2)
               ue = &undo_list[in_undo][undo_list[in_undo].size()-2];
         if(ue->ei){
            if(ue->ei==ei && ue->undo_id==undo_id){
               if(ue->new_style)
                  if(ck)
                     ck->ROpen(ue->data.begin(), ue->data.size());
               return true;
            }
         }
      }
      return false;
   }

//----------------------------
// Clear all undo buffers.
   virtual void Clear(){

      assert(!in_begin);
      undo_list[0].clear();
      undo_list[1].clear();
   }

//----------------------------
// Remove top-most entry.
   virtual void PopTopEntry(){

      if(undo_list[in_undo].size()){
         if(!undo_list[in_undo].back().ei)
            undo_list[in_undo].pop_back();
         if(undo_list[in_undo].size()){
            undo_list[in_undo].pop_back();
         }
      }
   }

//----------------------------

   virtual void BeforeFree(){
      Clear();
   }

//----------------------------
public:
   C_edit_Undo_imp():
      in_undo(false),
      in_redo(false),
      in_begin(false),
      feed_buf(NULL)
   {}

//----------------------------

   virtual bool Init(){

      ed->AddShortcut(this, E_UNDO_UNDO, "%10 &Edit\\%0 &Undo\tCtrl+Z", K_Z, SKEY_CTRL);
      ed->AddShortcut(this, E_UNDO_REDO, "%10 &Edit\\%0 %a Redo\tCtrl+X", K_X, SKEY_CTRL);

      {
                              //init Standard toolbar
         HWND hwnd = (HWND)ed->GetIGraph()->GetHWND();
         RECT rc;
         GetClientRect(hwnd, &rc);
         rc.left = rc.right;
         ClientToScreen(hwnd, (POINT*)&rc);
         int x_pos = rc.left - 90, y_pos = rc.top;
         toolbar = ed->GetToolbar("Standard", x_pos, y_pos, 3);

         S_toolbar_button tbs[] = {
            {E_UNDO_UNDO,  0, "Undo"},
            {E_UNDO_REDO,  1, "Redo"},
            {0, -1},
         };
         toolbar->AddButtons(this, tbs, sizeof(tbs)/sizeof(tbs[0]), "IDB_TB_UNDO", GetHInstance(), 20);
      }
      return true;
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){
      case E_UNDO_UNDO:
      case E_UNDO_REDO:
         {
            assert(!in_begin);
            if(in_begin)
               break;
            bool redo = (id==E_UNDO_REDO);
            if(undo_list[redo].size()){
               in_undo = !redo;
               in_redo = redo;
                              //go down untill not empty or break reached
               if(undo_list[redo].size()){
                              //skip break, if any
                  if(!undo_list[redo].back().ei)
                     undo_list[redo].pop_back();

                  while(undo_list[redo].size()){
                              //get undo entry
                     S_undo_entry &ue = undo_list[redo].back();
                              //break - stop un-doing
                     if(!ue.ei)
                        break;
                              //call plugin action
                     if(ue.new_style){
                              //open memory chunk
                        C_chunk ck;
                        ck.ROpen(ue.data.begin(), ue.data.size());
                        ue.ei->Undo(ue.undo_id, ue.frm, ck);
                     }else
                        ue.ei->Action(ue.undo_id, ue.data.begin());
                              //pop entry
                     undo_list[redo].pop_back();
                  }
               }

               in_undo = false;
               in_redo = false;
            }else
               ed->Message(!redo ? "No more undos" : "Nothing to redo");
         }
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

                              //insert entry with NULL plugin name
      S_undo_entry ue;
      ue.undo_id = 0;
      CheckTop(ue);
      undo_list[in_undo].push_back(ue);

      in_undo = !in_undo;
      CheckTop(ue);
      undo_list[in_undo].push_back(ue);
      in_undo = !in_undo;
   }
};

//----------------------------

void CreateUndo(PC_editor ed){
   PC_editor_item ei = new C_edit_Undo_imp;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
