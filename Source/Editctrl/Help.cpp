#include "all.h"
#include "ectrl_i.h"

bool ToggleIns(C_window *wp, int undo_buffer);

//----------------------------

static byte GetShifts(dword key_data){
   byte sh=0;
   if(key_data&(KF_ALTDOWN<<16)) sh |= KEY_ALT;
   if(GetAsyncKeyState(VK_SHIFT)&0x8000) sh |= KEY_SHIFT;
   if(GetAsyncKeyState(VK_CONTROL)&0x8000) sh |= KEY_CTRL;
   return sh;
}

//----------------------------

int C_block::Adjust(C_window *wp, bool shift, int x, int y, int prev_x, int prev_y, int undo_buff){
   int i=0;
   if(shift){
                                    //shift-marking
      if(mode!=OFF && type!=CUA){
         ::UnMark(wp);
         i=2;
      }
      if(prev_x==x && prev_y==y) return 0;
      if(mode!=EXPAND) MarkAny(wp, CUA, prev_x, prev_y);
   }else
   if(IsCUA()){
      if(x==prev_x && y==prev_y) return 0;
      ::UnMark(wp);
      return 2;
   }
   if(mode!=EXPAND) return 0;
   switch(type){
   case CUA:
                                    //save
      if(mode==EXPAND){
         wp->undo[undo_buff].Add(C_undo_element::CUAPOS, x2, y2);
      }
   case LINE:
      i |= y2==y ? 1 : 2;
      break;
   case COLUMN:
      i |= 2;
      break;
   }
   x2=x;
   y2=y;
   SetMinMax();
   return i;
}

//----------------------------

void C_block::InsertArea(C_window *wp, int undo_buf, int line, int l, int sx){
   if(!IsEmpty() && type==COLUMN && line==ymin && line==ymax){
      if(l<xmin || (l+sx)<=(xmax+1)){
         wp->undo[undo_buf].Add(C_undo_element::MARKBLOCK,
            (type<<16)|mode, (x1<<16)|y1, (x2<<16)|y2);
      }
      if(l<xmin){
         (x1<x2 ? x1:x2) +=sx;
      }
      if((l+sx)<=(xmax+1)){
         (x1>x2 ? x1:x2) +=sx;
      }
      SetMinMax();
   }
}

//----------------------------

void C_block::KillArea(C_window *wp, int undo_buf, int line, int l, int sx){
   if(!IsEmpty() && type==COLUMN && line==ymin && line==ymax){
      if(l<xmin || (l+sx-1)<=xmax){
         wp->undo[undo_buf].Add(C_undo_element::MARKBLOCK,
            (type<<16)|mode, (x1<<16)|y1, (x2<<16)|y2);
      }
      if(l<xmin){
         (x1<x2 ? x1:x2) -=sx;
      }
      if((l+sx-1)<=xmax){
         (x1>x2 ? x1:x2) -=sx;
      }
      SetMinMax();
      if(xmax<xmin) code_manager.edit_functions[UNMARKBLOCK-FIRST_COMMAND](wp);
   }
}

//----------------------------

void C_block::InsertLine(C_window *wp, int undo_buf, int line){
   if(!IsEmpty() && type!=CUA){
      if(line<ymin || line<=ymax){
         wp->undo[undo_buf].Add(C_undo_element::MARKBLOCK,
            (type<<16)|mode, (x1<<16)|y1, (x2<<16)|y2);
      }
      if(line<ymin){
         (y1<y2 ? y1:y2)++;
      }
      if(line<=ymax){
         (y1>y2 ? y1:y2)++;
      }
      SetMinMax();
   }
}

//----------------------------

void C_block::KillLine(C_window *wp, int undo_buf, int line){
   if(!IsEmpty() && type!=CUA){
      if(line<ymin || line<=ymax){
         wp->undo[undo_buf].Add(C_undo_element::MARKBLOCK,
            (type<<16)|mode, (x1<<16)|y1, (x2<<16)|y2);
      }
      if(line<ymin){
         (y1<y2 ? y1:y2)--;
         --ymin;
      }
      if(line<=ymax){
         (y1>y2 ? y1:y2)--;
         --ymax;
      }
      if(ymax<ymin) code_manager.edit_functions[UNMARKBLOCK-FIRST_COMMAND](wp);
   }
}

//----------------------------
//----------------------------

short C_key_code::default_key_table[]={
//table: key code, key modifier, command(s), BR
                                       //basic movements
   K_CURSORLEFT,        KEY_IGNORE_SHIFT, CURSORLEFT,    BR,
   K_CURSORRIGHT,       KEY_IGNORE_SHIFT, CURSORRIGHT,   BR,
   K_CURSORUP,          KEY_IGNORE_SHIFT, CURSORUP,      BR,
   K_CURSORDOWN,        KEY_IGNORE_SHIFT, CURSORDOWN,    BR,
   K_HOME,              KEY_IGNORE_SHIFT, BEGLINE,       BR,
   K_END,               KEY_IGNORE_SHIFT, ENDLINE,       BR,
   K_PAGEUP,            KEY_IGNORE_SHIFT, PAGEUP,        BR,
   K_PAGEDOWN,          KEY_IGNORE_SHIFT, PAGEDOWN,      BR,
                                 //control keys
   K_BACKSPACE,         0,             BACKSPACE,     BR,
   K_INS,               0,             TOGGLEINSERT,  BR,
   K_DEL,               0,             ISCUABLOCK, JFALSE, 2,
                                       DELETEBLOCK, RETURN, DELCH, BR,
   K_ENTER,             0,             ENTER,         BR,
   K_TAB,               0,             TABRT,         BR,
   K_TAB,               KEY_SHIFT|KEY_DISABLE_SHIFT, TABLT,         BR,

   K_CURSORLEFT,        KEY_IGNORE_SHIFT|KEY_CTRL,      WORDLEFT,      BR,
   K_CURSORRIGHT,       KEY_IGNORE_SHIFT|KEY_CTRL,      WORDRIGHT,     BR,
   K_PAGEUP,            KEY_IGNORE_SHIFT|KEY_CTRL,      BEGFILE,       BR,
   K_PAGEDOWN,          KEY_IGNORE_SHIFT|KEY_CTRL,      ENDFILE,       BR,
   K_HOME,              KEY_IGNORE_SHIFT|KEY_CTRL,      BEGSCREEN,     BR,
   K_END,               KEY_IGNORE_SHIFT|KEY_CTRL,      ENDSCREEN,     BR,

   K_CURSORUP,          KEY_IGNORE_SHIFT|KEY_CTRL,      SCROLLDOWN,    BR,
   K_CURSORDOWN,        KEY_IGNORE_SHIFT|KEY_CTRL,      SCROLLUP,      BR,
   K_CURSORUP,          KEY_CTRL|KEY_ALT, SCROLLDOWN, CURSORDOWN,    BR,
   K_CURSORDOWN,        KEY_CTRL|KEY_ALT, SCROLLUP, CURSORUP,      BR,
   K_BACKSPACE,         KEY_CTRL,      DELLINE,       BR,

   K_ESC,               0,             ESCAPE,        BR,

   BR                                //last
};

//----------------------------

C_cursor::C_cursor():
   x(0), y(0),
   prv_x(0), prv_y(0),
   redraw(1)
{
}

//----------------------------
//----------------------------

bool C_undo_buffer::Add(C_undo_element::E_id id, dword d1, dword d2, dword d3){
   int i;
   switch(id){
   case C_undo_element::POSITION:
   case C_undo_element::SCROLL:
      if(elements[PrevPos(pos)] && elements[PrevPos(pos)]->id==id) return true;
      elements[pos] = new C_undo_pos(id, d1, d2);
      break;
   case C_undo_element::PUTCHAR:
   case C_undo_element::UNMARKBLOCK:
   case C_undo_element::FILEMODIFY:
      elements[pos] = new C_undo_element(id);
      break;
   case C_undo_element::DELCHAR:
   case C_undo_element::DEL_LINE:
   case C_undo_element::STATECHANGE:
      elements[pos] = new C_undo_data1(id, d1);
      break;
   case C_undo_element::LINE:
      elements[pos] = new C_undo_line((C_document_line*)d1, d2);
      break;
   case C_undo_element::LINEPART:
      elements[pos] = new C_undo_linepart(d1, d2, d3);
      break;
   case C_undo_element::KILLPART:
   case C_undo_element::MARKBLOCK:
   case C_undo_element::CUAPOS:
      elements[pos] = new C_undo_data3(id, d1, d2, d3);
      break;
   case C_undo_element::MARK:
                                    //undo steps are separated by
                                    //NULL elements
                                    //avoid grouping multiple NULLs
      if(pos!=bottom_pos && !elements[PrevPos(pos)]){
         return true;
      }
      break;
   default:
      return false;
   }
   pos = NextPos(pos);
   if(elements[pos] || pos==bottom_pos){
                                    //kill this undo level
      i=pos;
      do{
         delete elements[i];
         elements[i]=NULL;
      }while(i=NextPos(i), elements[i]);
      bottom_pos=i;
   }
   return true;
}

//----------------------------

bool C_undo_buffer::Undo(C_window *wp, int redo_buf){
                                       //avoid empty undo
   if(pos!=bottom_pos && !elements[PrevPos(pos)])
      pos=PrevPos(pos);

   if(pos==bottom_pos) return false;   //no more undos
   dword d1, d2, d3;
   C_document_line *lp;
   int i;
   do{
      pos = PrevPos(pos);
      if(!elements[pos]) break;
      switch(elements[pos]->id){
      case C_undo_element::POSITION:
         elements[pos]->GetData(&d1, &d2);
         SetCursorPos1(wp, d1, d2, redo_buf);
         break;
      case C_undo_element::SCROLL:
         elements[pos]->GetData(&d1, &d2);
         wp->SetScrollPos(d1, d2, redo_buf);
         break;
      case C_undo_element::PUTCHAR:
                                       //delete char on the position
         DelChar(wp, false, redo_buf);
         break;
      case C_undo_element::DELCHAR:
                                       //delete char on the position
         elements[pos]->GetData(&d1);
         PutChar(wp, (char)d1, redo_buf);
         break;
      case C_undo_element::DEL_LINE:
         elements[pos]->GetData(&d1);
         DeleteLine(wp, d1, redo_buf);
         break;
      case C_undo_element::LINE:
         elements[pos]->GetData((dword*)&lp, &d2);
         InsertLine(wp, lp->text, lp->text_len, d2, redo_buf);
         break;
      case C_undo_element::KILLPART:
         elements[pos]->GetData(&d1, &d2, &d3);
         KillArea(wp, d2, d1, d3, redo_buf);
         break;
      case C_undo_element::LINEPART:
         elements[pos]->GetData((dword*)&lp, &d1, &d2);
         AddArea(wp, lp->text, d1, lp->text_len, d2, redo_buf);
         break;
      case C_undo_element::MARKBLOCK:
         if(!wp->block.IsEmpty()){
            wp->undo[redo_buf].Add(C_undo_element::MARKBLOCK,
               (wp->block.type<<16)|wp->block.mode,
               (wp->block.x1<<16)|wp->block.y1,
               (wp->block.x2<<16)|wp->block.y2);
         }
         elements[pos]->GetData(&d1, &d2, &d3);
         wp->block.SetMark((E_block_type)(d1>>16),
            (C_block::E_mode)(d1&0x7fff),
            d2>>16, d2&0x7fff,
            d3>>16, d3&0x7fff);
         wp->doc.redraw=1;
         wp->undo[redo_buf].Add(C_undo_element::UNMARKBLOCK);
         break;
      case C_undo_element::UNMARKBLOCK:
         UnMark(wp, redo_buf);
         break;
      case C_undo_element::CUAPOS:
         elements[pos]->GetData(&d1, &d2, &d3);
         i = wp->block.Adjust(wp, 1, d1, d2, 0, 0, redo_buf);
         if(i&2) wp->doc.redraw = true;
         if(i&1) wp->doc.redraw_line = true;
         break;
      case C_undo_element::FILEMODIFY:
         if(wp->doc.SetModify(false))
            SetWindowText(wp->ec->hwnd, wp->doc.title);
         break;
      case C_undo_element::STATECHANGE:
         elements[pos]->GetData(&d1, &d2, &d3);
         switch(d1){
         case 0:
            ToggleIns(wp, redo_buf);
            break;
         }
         break;
      default:
         break;
      }
                                       //kill undo element
      delete elements[pos];
      elements[pos] = NULL;
   }while(true);
   return true;
}

//----------------------------
//----------------------------

int C_key_code::Get(short *user_macs, dword vkey, dword key_data,
   bool *shift_down, bool allow_esc){

   if(shift_down!=NULL) *shift_down=0;
   int i;
   if(curr_ptr && *curr_ptr==BR){
      if(!Pop((void**)&curr_ptr)) curr_ptr=NULL;
   }
   if(!curr_ptr){
                                       //get shift keys status
      byte shift_code=GetShifts(key_data);
      code_shift_down=!!(shift_code&KEY_SHIFT);
                                       //if esc not allowed, return
      if(!allow_esc){
         switch(vkey){
         case K_ESC: return ESCAPE;
         case K_ENTER: return ENTER;
         }
      }

      for(i=0; i<2; i++){
         if(!i){
            curr_ptr=user_macs;
            if(!curr_ptr || *curr_ptr==BR) continue;
         }else{
                                       //default table
            curr_ptr = default_key_table;
         }
         do{
            if(curr_ptr[0]==vkey)
            if((curr_ptr[1]&(KEY_SHIFT|KEY_ALT|KEY_CTRL))==shift_code ||
               ((curr_ptr[1]&KEY_IGNORE_SHIFT) &&
               (curr_ptr[1]&(KEY_ALT|KEY_CTRL)) == (shift_code&(KEY_ALT|KEY_CTRL)))){
               if(curr_ptr[1]&KEY_DISABLE_SHIFT) code_shift_down=0;
               curr_ptr+=2;
               goto ok;
            }
                                       //get next entry
            curr_ptr+=2;
            while(*curr_ptr!=BR) ++curr_ptr;
            ++curr_ptr;
         }while(*curr_ptr!=BR);
      }
      curr_ptr=NULL;
                                       //no code
      return 0;
   }
ok:
   int code = *curr_ptr++;
   if(*curr_ptr==BR){
      if(!Pop((void**)&curr_ptr)) 
         curr_ptr = NULL;
   }
   if(shift_down) *shift_down = code_shift_down;
   return code;
}

//----------------------------
