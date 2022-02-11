#include "all.h"
#include "common.h"
#include <insanity\3DTexts.h>

//----------------------------
//----------------------------

class C_console{
   struct S_out_text{
      C_smart_ptr<C_text> textp;
      int num_lines;
      S_out_text(): textp(NULL)
      {}
      S_out_text(PC_poly_text texts, const char *cp, float text_x, float text_y, float line_top, float line_size,
         dword color, dword justify, float fmt_rect[4], bool shd):
         num_lines(1)
      {
         S_text_create tc;
         tc.tp = cp;
         tc.color = color;
         tc.x = text_x;
         tc.y = text_y + line_top;
         tc.size = line_size;
         tc.justify = justify;
         tc.wide = false;
         tc.fmt_rect = fmt_rect;
         tc.shadow = shd;
         tc.num_lines = &num_lines;
         textp = texts->CreateText(tc);
         if(textp)
            textp->Release();
      }
   };

   C_vector<struct S_out_text*> out_texts;
   float text_x, text_y;
   int out_text_time;
   float line_dist;
   float line_top, lines_height;
   float line_size;
   dword justify;
   dword scroll_time;
   float fmt_rect[4];
   bool has_shadow;

   void OrderTexts(){

      text_y = 0.0f;
      for(dword i=0; i<out_texts.size(); i++){
         text_y += line_dist;
         const S_vector2 &pos = out_texts[i]->textp->GetPos();
         out_texts[i]->textp->SetPos(S_vector2(pos.x, text_y + line_top));
         text_y += line_dist * (out_texts[i]->num_lines - 1);
      }
   }

public:
   C_console(float left, float top, float ls, float ld,
      dword st = 3000, float height = .75f, dword j = 0, bool shd = false):
      text_x(left),
      text_y(0.0f),
      line_top(top),
      lines_height(height),
      line_size(ls),
      line_dist(ld),
      scroll_time(st),
      justify(j),
      has_shadow(shd),
      out_text_time(0)
   {
      static float default_fmt_rect[4] = {.01f, 0, .98f, 0};
      memcpy(fmt_rect, default_fmt_rect, sizeof(fmt_rect));
   }

   ~C_console(){
      Clear();
   }

//----------------------------

   void OutText(PC_poly_text texts, const char *cp, bool join = false, dword color = 0xffffffff){

      if(join && out_texts.size()){
         delete out_texts.back();
         out_texts.pop_back();
         text_y -= line_dist;
         if(out_texts.size())
            text_y -= line_dist * (out_texts.back()->num_lines - 1);
      }

      if(!out_texts.size())
         out_text_time = scroll_time * 2;

      text_y += line_dist;
      if(out_texts.size())
         text_y += line_dist * (out_texts.back()->num_lines - 1);
      out_texts.push_back(new S_out_text(texts, cp, text_x, text_y, line_top, line_size, color,
         justify, fmt_rect, has_shadow));
                                 //don't bother with text which failed to create
      if(!out_texts.back()->textp){
         delete out_texts.back();
         out_texts.pop_back();
      }

      if(out_texts.size() > (lines_height / line_dist)){
         delete out_texts.front();
         out_texts.erase(out_texts.begin());
         OrderTexts();
      }
   }

//----------------------------

   void Clear(){

      for(dword i=0; i<out_texts.size(); i++)
         delete out_texts[i];
      out_texts.clear();
      text_y = 0.0f;
   }

//----------------------------

   void Tick(int t){

      if(out_texts.size()){   //scroll console messages
         while(out_texts.size()){
            int i = t * Min(18, FloatToInt((float)exp(out_texts.size() * .18f)));
            if((out_text_time -= i)>0)
               break;
            out_text_time = Max(0, out_text_time + (int)scroll_time);
            delete out_texts.front();
            out_texts.erase(out_texts.begin());
            OrderTexts();
         }
      }
   }

//----------------------------
   void SetFormatRect(float left, float right){
      fmt_rect[0] = left; fmt_rect[2] = right;
   }

//----------------------------
// Set flash of last line - on = flash, off = back to normal.
   void FlashLine(bool on){
      if(out_texts.size()){
         out_texts.back()->textp->SetColor(!on ? 0xffffffff : 0xff606030);
      }
   }
};

//----------------------------
//----------------------------

class C_edit_Console: public C_editor_item_Console{
   virtual const char *GetName() const{ return "Console"; }

   C_console console_debug, console_print;
//----------------------------
//----------------------------

public:
   C_edit_Console():
      console_debug(.01f, .15f, .022f, .022f),
      console_print(.01f, .09f, .022f, .025f)
   {
   }

   virtual bool Init(){

      return true;
   }

//----------------------------

   virtual void Close(){
      console_debug.Clear();
      console_print.Clear();
   }

//----------------------------

   virtual void Debug(const char *cp, dword color = 0xc0ffffff){
      console_debug.OutText(ed->GetTexts(), cp, false, color);
   }

//----------------------------

   virtual void Print(const char *cp, dword color = 0xffffffff){
      console_print.OutText(ed->GetTexts(), cp, false, color);
   }

//----------------------------

   virtual dword Action(int id, void *context){
      switch(id){
      case 1000:
         console_debug.Clear();
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      console_print.Tick(time);
   }

//----------------------------

   virtual bool LoadState(C_chunk &ck){

      return true;
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      return true;
   }
};

//----------------------------

C_editor_item_Console *CreateConsole(PC_editor ed){
   C_editor_item_Console *ei = new C_edit_Console;
   ed->InstallPlugin(ei);
   ei->Release();
   return ei;
}

//----------------------------
