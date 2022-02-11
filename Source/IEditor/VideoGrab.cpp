#include "all.h"
#include "common.h"

//----------------------------
                              //default extension - used if user doesn't specify one

#define DEFAULT_EXTENSION "bmp"

//----------------------------

class C_edit_VideoGrab: public C_editor_item{
   virtual const char *GetName() const{ return "VideoGrab"; }

//----------------------------

   enum{
      ACTION_GRAB,
   };

//----------------------------

   bool in_grab;
   C_vector<C_smart_ptr<IImage> > images;

   dword size_x, size_y;
   //dword fps;

   //dword last_timer;

//----------------------------

   BOOL dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      switch(uMsg){
      case WM_INITDIALOG:
         OsCenterWindow(hwnd, ed->GetIGraph()->GetHWND());
                              //fill init params
         SetDlgItemInt(hwnd, IDC_SIZE_X, size_x, false);
         SetDlgItemInt(hwnd, IDC_SIZE_Y, size_y, false);
         //SetDlgItemInt(hwnd, IDC_FPS, fps, false);
         return 1;

      case WM_COMMAND:
         switch(LOWORD(wParam)){
         case IDOK:
                              //collect parameters
            size_x = GetDlgItemInt(hwnd, IDC_SIZE_X, NULL, false);
            size_y = GetDlgItemInt(hwnd, IDC_SIZE_Y, NULL, false);
            //fps = GetDlgItemInt(hwnd, IDC_FPS, NULL, false);
            EndDialog(hwnd, 1);
            break;
         case IDCANCEL: EndDialog(hwnd, 0); break;
         }
         break;
      }
      return 0;
   }

   static BOOL CALLBACK dlgThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

      if(uMsg==WM_INITDIALOG)
         SetWindowLong(hwnd, GWL_USERDATA, lParam);
      C_edit_VideoGrab *ep = (C_edit_VideoGrab*)GetWindowLong(hwnd, GWL_USERDATA);
      if(ep)
         return ep->dlgProc(hwnd, uMsg, wParam, lParam);
      return 0;
   }

//----------------------------
public:
   C_edit_VideoGrab():
      in_grab(false),
      //fps(20),
      size_x(400),
      size_y(300)
   {}

   virtual bool Init(){
#define DS "&View\\&Video\\"
      ed->AddShortcut(this, ACTION_GRAB, DS"&Grab being/end\tScrollLock", K_SCROLLLOCK, 0);
      return true;
   }

   virtual dword Action(int id, void *context){

      switch(id){
      case ACTION_GRAB:
         if(!in_grab){
                              //ask user about params
            int i = DialogBoxParam(GetHInstance(), "VIDEOGRAB", (HWND)ed->GetIGraph()->GetHWND(), dlgThunk, (LPARAM)this);
            if(!i)
               break;
            in_grab = true;
            ed->Message("Video grab begin.");
            //last_timer = ed->GetIGraph()->ReadTimer();
         }else{
            in_grab = false;
            ed->Message("Video grab finished.", 0, EM_MESSAGE, true);
            if(!images.size())
               break;

            PIGraph igraph = ed->GetIGraph();

            char buf[MAX_PATH];

            OPENFILENAME on;
            memset(&on, 0, sizeof(on));
            on.lStructSize = sizeof(on);
            on.hwndOwner = (HWND)igraph->GetHWND();
            on.lpstrFilter = "Image (*.bmp, *.png)\0*.bmp;*.png\0All files\0*.*\0";
            on.nFilterIndex = 0;
            on.lpstrFile = buf;
            buf[0] = 0;
            on.nMaxFile = MAX_PATH;
            //on.lpstrInitialDir =
            on.lpstrTitle = "Save image sequence as (enter name base)";
            on.Flags |= OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_NOTESTFILECREATE | OFN_EXPLORER;
            //on.lpstrDefExt = def_ext;

            bool b = GetSaveFileName(&on);
            if(b){
               igraph->EnableSuspend(false);
               C_str ext = DEFAULT_EXTENSION;
               for(dword i=strlen(buf); i--; ){
                  if(buf[i]=='\\')
                     break;
                  if(buf[i]=='.'){
                     ext = &buf[i+1];
                     buf[i] = 0;
                     break;
                  }
               }

               dword num = images.size();
               for(i=0; i<num; i++){
                  C_fstr fname_full("%s_%.4i.%s", buf, i, (const char*)ext);
                  ed->Message(C_fstr("Saving image %i/%i", i+1, num), 0, EM_MESSAGE, true);
                  PIImage img = images[i];
                  //img->Draw();
                  //igraph->UpdateScreen();
                  C_cache ck;
                  ck.open(fname_full, CACHE_WRITE);
                  img->SaveShot(&ck, ext);
                  ck.close();
                  images[i] = NULL;
                  if(igraph->ReadKey(true)==K_ESC)
                     break;
               }
               igraph->EnableSuspend(true);
            }
            images.clear();
            ed->Message("Video grab finished.");
         }
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      if(in_grab){
         PIGraph igraph = ed->GetIGraph();
         PIImage img = igraph->CreateBackbufferCopy(size_x, size_y, true);
         if(img){
                              //copy to sysmem image, so that we don't run out of video memory
            PIImage sysmem_img = igraph->CreateImage();
            if(sysmem_img->Open(NULL, IMGOPEN_SYSMEM | IMGOPEN_EMPTY, size_x, size_y, img->GetPixelFormat())){
               sysmem_img->CopyStretched(img);
               images.push_back(sysmem_img);
               ed->Message(C_fstr("Grab frame: %i", images.size()));
            }
            sysmem_img->Release();
            img->Release();
         }else{
            ed->Message("Failed to grab frame");
         }
         /*
                              //sleep at least for given time
         dword curr_timer = igraph->ReadTimer();
         dword delta = curr_timer - last_timer;
         dword want_frame_time = 1000 / fps;
         if(delta < want_frame_time){
            --delta;
            Sleep(want_frame_time - delta);
         }
         //ed->Message(C_fstr("curr: %i, d: %i, want: %i", curr_timer, delta, want_frame_time));
         last_timer = curr_timer + want_frame_time-delta;
         */
      }
   }

//----------------------------
   enum{
      SS_SX,
      SS_SY,
      //SS_FPS,
   };

//----------------------------

   virtual bool LoadState(C_chunk &ck){

      while(ck)
      switch(++ck){
      case SS_SX: ck >> size_x; break;
      case SS_SY: ck >> size_y; break;
      //case SS_FPS: ck >> fps; break;
      default: --ck;
      }
      return true;
   }
   virtual bool SaveState(C_chunk &ck) const{

      ck
         (SS_SX, size_x)
         (SS_SY, size_y)
         //(SS_FPS, fps)
         ;
      return false;
   }
};

//----------------------------

void CreateVideoGrab(PC_editor ed){
   PC_editor_item ei = new C_edit_VideoGrab;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
