#include "all.h"
#include "common.h"

//----------------------------

class C_edit_Tape: public C_editor_item{
   virtual const char *GetName() const{ return "Tape"; }

   C_smart_ptr<C_editor_item_MouseEdit> e_mouseedit;
   C_smart_ptr<C_editor_item_Stats> e_stats;

   enum{
      E_TAPE_TOGGLE = 1000,
      E_TAPE_CLICK,
   };

   bool tape_on;
   S_vector from_point;

//----------------------------

   void CloseTapeMode(){
      if(e_mouseedit->GetUserPick()==this){
         e_mouseedit->SetUserPick(NULL, 0);
      }
      ed->Message("Tape off");
      tape_on = false;
   }

//----------------------------

   bool PickPoint(S_vector &pos){

      PIGraph igraph = ed->GetIGraph();
      dword mx = igraph->Mouse_x();
      dword my = igraph->Mouse_y();
      PI3D_scene scn = ed->GetScene();
      I3D_collision_data cd;
      I3D_RESULT ir = scn->UnmapScreenPoint(mx, my, cd.from, cd.dir);
      if(I3D_FAIL(ir))
         return false;
      
      cd.flags = I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_RAY;
      if(!scn->TestCollision(cd))
         return false;
      pos = cd.ComputeHitPos();
      return true;
   }

//----------------------------

public:
   C_edit_Tape():
      tape_on(false)
   {}
   virtual bool Init(){

      e_mouseedit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      e_stats = (PC_editor_item_Stats)ed->FindPlugin("Stats");
      if(!e_mouseedit || !e_stats)
         return false;

#define MB "&View\\"
      ed->AddShortcut(this, E_TAPE_TOGGLE, MB"&Tape\tShift+T", K_T, SKEY_SHIFT);
      return true;
   }
   virtual void Close(){ 
   }

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){
      if(tape_on){
         if(e_mouseedit->GetUserPick()!=this){
            CloseTapeMode();
         }
      }
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){
      case E_TAPE_TOGGLE:
         if(!tape_on){
                              //must be in edit mode
            if(e_mouseedit->GetCurrentMode()!=0){
               ed->Message("Cannot use Tape in View mode");
               break;
            }
            if(!PickPoint(from_point)){
               ed->Message("Tape: failed to detect collision");
               break;
            }
            e_mouseedit->SetUserPick(this, E_TAPE_CLICK, LoadCursor(GetHInstance(), "IDC_CURSOR_TAPE"));

            tape_on = true;
            ed->Message("Tape on");
         }else{
            CloseTapeMode();
         }
         break;

      case E_TAPE_CLICK:
         PickPoint(from_point);
         return true;
      }
      return 0;
   }

//----------------------------

   virtual void Render(){

      if(!tape_on)
         return;
      C_str text = "Tape:\n";
      if(e_mouseedit->GetCurrentMode()!=0){
         text += "<view mode>";
      }else{
         S_vector to_point;
         if(!PickPoint(to_point)){
            text += "<no collision>";
         }else{
            S_vector delta = to_point - from_point;
            float len = delta.Magnitude();
            text += C_fstr("Distance: %.2f\nHeight: %.2f", len, delta.y);
            PI3D_scene scn = ed->GetScene();
            PI3D_driver drv = ed->GetDriver();
            bool was_zb = drv->GetState(RS_USEZB);
            if(was_zb) drv->SetState(RS_USEZB, false);

            scn->DrawLine(from_point, S_vector(to_point.x, from_point.y, to_point.z), 0x800000ff);
            scn->DrawLine(to_point, to_point + S_vector(0, -delta.y, 0), 0x8000ff00);
            scn->DrawLine(from_point, to_point, 0xffffffff);

                              //draw ticks on main axis
            dword num_ticks;
            float tick_dist = .1f;
            if(len >= 10.0f){
               tick_dist = 1.0f;
               if(len >= 100.0f){
                  tick_dist = 10.0f;
               }
            }

            num_ticks = (dword)(len / tick_dist + .99f);
            S_normal line_dir = delta;
            const S_vector &cam_pos = scn->GetActiveCamera()->GetWorldPos();
            for(dword i=0; i<num_ticks; i++){
               S_vector p = from_point + line_dir * (tick_dist * (float)i);
               dword color = 0x40ffffff;
               float dist = tick_dist;
               if(!(i%10)){
                  color = 0x80ffffff;
                  dist *= 2.0f;
               }
               S_normal tick_dir = line_dir.Cross(p-cam_pos);
               if(tick_dir.y < 0.0f)
                  dist = -dist;
               scn->DrawLine(p, p + tick_dir * dist, color);
            }
            if(was_zb) drv->SetState(RS_USEZB, true);
         }
      }
      e_stats->RenderStats(.84f, .67f, .15f, .075f, text, .022f);
   }

   virtual bool LoadState(C_chunk &ck){ 
      return false; 
   }
   virtual bool SaveState(C_chunk &ck) const{ 
      return false; 
   }
};

//----------------------------

void CreateTape(PC_editor ed){
   PC_editor_item ei = new C_edit_Tape;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
