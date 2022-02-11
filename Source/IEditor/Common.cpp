#include "all.h"
#include "common.h"


//----------------------------

void SetupVolumesStatic(PI3D_frame frm, bool on){

   struct S_hlp{
      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
         frm->SetFlags(c ? I3D_FRMF_STATIC_COLLISION : 0, I3D_FRMF_STATIC_COLLISION);
         return I3DENUMRET_OK;
      }
   };
   frm->EnumFrames(S_hlp::cbEnum, on, ENUMF_VOLUME);
}

//----------------------------

void SetupVolumesOwner(PI3D_frame frm, PI3D_frame frm_owner){

   struct S_hlp{
      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){
         I3DCAST_VOLUME(frm)->SetOwner((PI3D_frame)c);
         return I3DENUMRET_OK;
      }
   };
   frm->EnumFrames(S_hlp::cbEnum, (dword)frm_owner, ENUMF_VOLUME);
}

//----------------------------

C_str FloatStrip(const C_str &str){

   const char *cp = strchr(str, '.');
   if(!cp)
      return str;
   dword dot_pos = cp - str;

   for(dword i=str.Size()-1; i > dot_pos+1; i--){
      if(str[i]!='0')
         break;
   }
   ++i;
   if(i==str.Size())
      return str;
   C_str ret;
   ret.Assign(NULL, i);
   memcpy(&ret[0], (const char*)str, i);
   ret[i] = 0;
   return ret;
   /*
   for(dword i=str.Size(); i--; ){
      if(str[i]=='.')
         break;
   }
   if(i==-1)
      return str;
   C_str ret = str;
   for(i=ret.Size(); i--; ){
      if(ret[i]!='0')
         break;
   }
   if(ret[i]=='.')
      ++i;
   ret[i+1] = 0;
   return ret;
   */
}

//----------------------------

C_str GetDlgItemText(HWND hwnd, int dlg_id){
   char buf[256];
   return GetDlgItemText(hwnd, dlg_id, buf, sizeof(buf)) ?
      buf : NULL;
}

//----------------------------

bool SelectName(PIGraph igraph, HWND hwnd_parent, const char *title, C_str &name, const char *desc){

   char buf[256];
   strcpy(buf, name);
   struct S_hlp{
      PIGraph igraph;
      const char *frm_name;
      const char *title;
      const char *desc;
      static BOOL CALLBACK dlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

         switch(uMsg){
         case WM_INITDIALOG:
            {
               S_hlp *hlp = (S_hlp*)lParam;
               InitDlg(hlp->igraph, hwnd);
               SetWindowText(hwnd, hlp->title);
               //SetDlgItemText(hwnd, IDC_STATIC_NAME, hlp->title);
               SetDlgItemText(hwnd, IDC_EDIT, hlp->frm_name);
               ShowWindow(hwnd, SW_SHOW);
               SetWindowLong(hwnd, GWL_USERDATA, lParam);
               if(hlp->desc)
                  SetDlgItemText(hwnd, IDC_STATIC_NAME, hlp->desc);
            }
            return 1;
         case WM_COMMAND:
            switch(LOWORD(wParam)){
            case IDCANCEL: EndDialog(hwnd, 0); break;
            case IDOK:
               {
                  S_hlp *hlp = (S_hlp*)GetWindowLong(hwnd, GWL_USERDATA);
                  SendDlgItemMessage(hwnd, IDC_EDIT, WM_GETTEXT, 256, (LPARAM)hlp->frm_name);
                  EndDialog(hwnd, 1);
               }
               break;
            }
            break;
         }
         return 0;
      }
   } hlp = {igraph, buf, title, desc};
   HWND prev_hwnd = GetActiveWindow();
   if(!hwnd_parent)
      hwnd_parent = (HWND)igraph->GetHWND();
   int i = DialogBoxParam(GetHInstance(), "GET_MODELNAME", hwnd_parent, S_hlp::dlgProc, (LPARAM)&hlp);
   if(i)
      name = buf;
   SetActiveWindow(prev_hwnd);
   return i;
}

//----------------------------

bool IsNameInScene(PI3D_scene scene, const C_str &str1){

   struct S_nc{
      static I3DENUMRET I3DAPI cbNameCheck(PI3D_frame frm, dword str1){
         C_str **str=(C_str**)str1;
         //if(strcmp(frm->GetName(), **str)) return I3DENUMRET_OK;
         if(frm->GetName() != **str) return I3DENUMRET_OK;
         *str = NULL;
         return I3DENUMRET_CANCEL;
      }
   };
   const C_str *str = &str1;
   scene->EnumFrames(S_nc::cbNameCheck, (dword)&str);
   return !str;
}

//----------------------------

PI3D_visual CastVisual(PC_editor ed, PI3D_visual vis, dword visual_type,
   PC_editor_item_Modify e_modify, PC_editor_item_Selection e_slct){

   PI3D_scene scn = ed->GetScene();
   PI3D_visual new_vis = I3DCAST_VISUAL(scn->CreateFrame(FRAME_VISUAL, visual_type));
   bool ret = false;
   if(new_vis){
      I3D_RESULT ir = new_vis->Duplicate(vis);
      if(I3D_SUCCESS(ir)){
         new_vis->LinkTo(vis->GetParent());
         while(vis->NumChildren()) vis->GetChildren()[0]->LinkTo(new_vis);

         vis->LinkTo(NULL);

         e_modify->AddFrameFlags(vis, E_MODIFY_FLG_VISUAL);
         e_modify->ReplaceFrame(vis, new_vis);

         e_slct->AddFrame(new_vis);

                           //check if scene or model owns it
         for(PI3D_frame model_root=new_vis; (model_root=model_root->GetParent(), model_root) && model_root->GetType()!=FRAME_MODEL; );
         if(model_root){
            I3DCAST_MODEL(model_root)->RemoveFrame(vis);
            I3DCAST_MODEL(model_root)->AddFrame(new_vis);
         }else{
            scn->RemoveFrame(vis);
            scn->AddFrame(new_vis);
         }

         ed->SetModified();
         ret = true;
      }
      new_vis->Release();
   }
   return ret ? new_vis : NULL;
}

//----------------------------
/*
void ComputeContour(const I3D_bbox &bb_in, const S_matrix &tm, const S_vector &look,
   S_vector contour_points[6], dword &num_cpts, bool orthogonal){

   num_cpts = 0;

   enum{
      EDGE_DIR_NEG = 0,       //edge pointing to negative direction
      EDGE_DIR_POS = 1,       //edge pointing to positive direction
      EDGE_VISIBLE = 2,       //edge is visible
      EDGE_DIR_MASK = 1,
   };
                              //edge check-in flags - used XOR, so that when edge is
                              // marked twice, its EDGE_VISIBLE flag is cleared
   union{
      byte edge_check_in[12];
      dword dw[3];
   };
   dw[0] = 0; dw[1] = 0; dw[2] = 0;
   
                              //transform edge bbox
   I3D_bbox bb_trans;
   bb_trans.min = bb_in.min * tm;
   bb_trans.max = bb_in.max * tm;

   if(!orthogonal){
      S_vector dir_min = look - bb_trans.min;
      S_vector dir_max = look - bb_trans.max;

      bool inside = true;

      if(dir_min.Dot(tm(0)) < 0.0f){
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }else
      if(dir_max.Dot(tm(0)) >= 0.0f){
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }

      if(dir_min.Dot(tm(1)) < 0.0f){
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }else
      if(dir_max.Dot(tm(1)) >= 0.0f){
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }

      if(dir_min.Dot(tm(2)) < 0.0f){
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }else
      if(dir_max.Dot(tm(2)) >= 0.0f){
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }
      if(inside)
         return;
   }else{
      if(look.Dot(tm(0)) < 0.0f){
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }else{
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }

      if(look.Dot(tm(1)) < 0.0f){
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }else{
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }

      if(look.Dot(tm(2)) < 0.0f){
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }else{
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }
   }
                              //get any beginning edge
   for(int ei_beg=12; ei_beg--; ){
      if(edge_check_in[ei_beg]&EDGE_VISIBLE)
         break;
   }
   assert(ei_beg!=-1);

   S_vector bbd_t[3];         //three C_vector aligned with tm axes, scaled by bbox
   bbd_t[0] = tm(0) * (bb_in.max.x - bb_in.min.x);
   bbd_t[1] = tm(1) * (bb_in.max.y - bb_in.min.y);
   bbd_t[2] = tm(2) * (bb_in.max.z - bb_in.min.z);

   static const struct{
      byte vi[2];             //[neg|pos]
   } vertex_indicies[12] = {
      0, 1,    2, 3,    4, 5,    6, 7,
      0, 2,    1, 3,    4, 6,    5, 7,
      0, 4,    1, 5,    2, 6,    3, 7,
   };
                           //links to 2 edges for each edge
   static const struct{
      byte link[2][2];        //[neg|pos][]
   } edge_links[12] = {
      {4, 8, 5, 9},  //0
      {4, 10, 5, 11},//1
      {6, 8, 7, 9},  //2
      {6, 10, 7, 11},//3
      {0, 8, 1, 10}, //4
      {0, 9, 1, 11}, //5
      {2, 8, 3, 10}, //6
      {2, 9, 3, 11}, //7
      {0, 4, 2, 6},  //8
      {0, 5, 2, 7},  //9
      {1, 4, 3, 6},  //10
      {1, 5, 3, 7}   //11
   };


   int ei = ei_beg;
   do{
      int vi = vertex_indicies[ei].vi[edge_check_in[ei]&EDGE_DIR_MASK];

      assert(num_cpts < 6);
      S_vector &cp = contour_points[num_cpts++];
      cp = bb_trans.min;
      if(vi&1) cp += bbd_t[0];
      if(vi&2) cp += bbd_t[1];
      if(vi&4) cp += bbd_t[2];

                              //move to the next visible edge
      int ei1 = edge_links[ei].link[edge_check_in[ei]&EDGE_DIR_MASK][0];
      ei = edge_links[ei].link[edge_check_in[ei]&EDGE_DIR_MASK][!(edge_check_in[ei1]&EDGE_VISIBLE)];
   } while(ei!=ei_beg);
}
*/
//----------------------------

bool CheckFrustumIntersection(const S_plane *planes, dword num_planes, const S_vector *hull_pts,
   const S_vector *verts, dword num_verts, const S_vector &cam_pos){

   bool penetrated = false;
   dword clip_union = (1<<num_planes) - 1;
   for(int p2i=num_verts, p2i_next=0; p2i--; p2i_next=p2i){
      const S_vector &v_p2 = verts[p2i];
      const S_vector dir_p2 = verts[p2i_next] - v_p2;

                              //flag specifying that v_p2 is inside
      bool v_p2_in = true;

      float last_enter = -1e+16f, first_leave = 1e+16f;
                              //check this edge against all planes of poly1
      dword plane_in_flags = 0;
      dword plane_mask = 1;
      for(int p1i=num_planes; p1i--; plane_mask<<=1){
         const S_plane &p1_edge_plane = planes[p1i];
         float d = v_p2.DistanceToPlane(p1_edge_plane);
                              //check if out of one plane
         if(d < 0.0f){
            plane_in_flags |= plane_mask;
         }else{
            v_p2_in = false;
         }
         if(!penetrated){
                              //get distance at which we penetrate it
            float f = p1_edge_plane.normal.Dot(dir_p2);
            if(!IsAbsMrgZero(f)){
               float r = -d / f;
               if(f < 0.0f){
                  last_enter = Max(last_enter, r);
               }else{
                  first_leave = Min(first_leave, r);
               }
            }else
            if(d >= 0.0f){
                              //missing the plane, can't hit poly
               last_enter = 1e+16f;
            }
         }
      }
      if(v_p2_in)
         return true;
      clip_union &= ~plane_in_flags;
      if(!penetrated){
         if(last_enter >= 0.0f && first_leave <= 1.0f){
            penetrated = (last_enter < first_leave);
            if(penetrated)
               return true;
         }
                              //now compute p2 edge plane, and detect if frustum is out
         S_plane pl;
         pl.normal.GetNormal(cam_pos, v_p2, v_p2+dir_p2);
         pl.d = -pl.normal.Dot(cam_pos);
         for(int i=num_planes; i--; ){
            float d = hull_pts[i].DistanceToPlane(pl);
            if(d < 0.0f)
               break;
         }
         if(i==-1)
            return false;
      }
   }
                              //if all verts are out of single plane, intersection = false
   return (!clip_union);
}

//----------------------------

void MakeVertexMapping(const S_vector *vp, dword pitch, int numv, word *v_map, float thresh){

   if(!numv) return;
#ifndef _MSC_VER
//#if 1

   for(int i=0; i<numv; i++){
      const S_vector &v1 = *(S_vector*)(((byte*)vp) + pitch * i);
      for(int j=0; j<i; j++){
         const S_vector &v2 = *(S_vector*)(((byte*)vp) + pitch * j);
         if((Fabs(v1.x-v2.x) + Fabs(v1.y-v2.y) + Fabs(v1.z-v2.z)) < thresh)
            break;
      }
      v_map[i] = j;
   }
#else

   __asm{
      push ecx

      mov ecx, 0
      mov esi, vp
      mov ebx, v_map
      push eax
      mov eax, thresh
   l1:
      mov edx, 0
      cmp edx, ecx
      jz sk1
      mov edi, vp
   l2:
      fld dword ptr[esi+0]
      fsub dword ptr[edi+0]
      fabs
      
      fld dword ptr[esi+4]
      fsub dword ptr[edi+4]
      fabs
      faddp st(1), st
      
      fld dword ptr[esi+8]
      fsub dword ptr[edi+8]
      fabs
      faddp st(1), st
      fstp dword ptr[esp]

      cmp eax, [esp]
      ja sk1

      add edi, pitch
      inc edx
      cmp edx, ecx
      jnz l2
   sk1:
      mov [ebx+ecx*2], dx
      add esi, pitch
      inc ecx
      cmp ecx, numv
      jne l1
      pop eax

      pop ecx
   }
#endif
}


//----------------------------

void InitDlg(PIGraph igraph, void *hwnd){
                              //center dlg and flip to GDI
   //igraph->FlipToGDI();
   POINT ig_pos = {0, 0};
   ClientToScreen((HWND)igraph->GetHWND(), &ig_pos);

   RECT rc;
   GetWindowRect((HWND)hwnd, &rc);
   int x = ig_pos.x + ((int)igraph->Scrn_sx() - (rc.right  - rc.left)) / 2;
   int y = ig_pos.y  + ((int)igraph->Scrn_sy() - (rc.bottom - rc.top )) / 2;
   x = Max(0, x);
   y = Max(0, y);
   SetWindowPos((HWND)hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

//----------------------------

void MakeSceneName(PI3D_scene scene, char *buf, bool suffix_hint){

                              //try to extract number
   if(!suffix_hint && !IsNameInScene(scene, buf))
      return;

                              //collect all scene names into a list
   set<C_str> name_list;
   struct S_hlp{
      static I3DENUMRET I3DAPI cbEnum(PI3D_frame frm, dword c){

         set<C_str> *name_list = (set<C_str>*)c;
         (*name_list).insert(frm->GetName());
         return I3DENUMRET_OK;
      }
   };
   scene->EnumFrames(S_hlp::cbEnum, (dword)&name_list);

   int i;
   i = strlen(buf);
   while(i-- && isdigit(buf[i]));
   ++i;
   int num = 0;

   sprintf(&buf[i], num<10 ? "%.2i" : "%i", num);
   if(name_list.find(buf) == name_list.end())
      return;
   while(true){
      ++num;
      sprintf(&buf[i], num<10 ? "%.2i" : "%i", num);
      if(name_list.find(buf) == name_list.end())
         return;
   }
   sprintf(&buf[i], num<10 ? "%.2i" : "%i", num);
}

//----------------------------

static bool WinGetComputerName(C_str &str){

   char buf[255];
   dword sz = sizeof(buf);
   if(!GetComputerName(buf, &sz)){
      str = NULL;
      return false;
   }
   str = buf;
   return true;
}

//----------------------------

bool IsProgrammersPC(PC_editor ed){

   PC_editor_item ei = ed->FindPlugin("Access");
   if(ei)
      return (ei->Action(0) >= 2);
   return false;
}

//----------------------------

PI3D_frame TrySelectTopModel(PI3D_frame frm, bool return_topmost){

   PI3D_frame f1 = frm;
   while(f1 && (f1=f1->GetParent(), f1)){
      if(f1->GetType()==FRAME_MODEL){
         frm = f1;
         if(!return_topmost)
            break;
      }
   }
   return frm;
}

//----------------------------

int ChooseItemFromList(PIGraph igraph, HWND hwnd_parent, const char *title, const char *item_list, int curr_sel){

   struct S_hlp{
      const char *title;
      const char *item_list;
      int curr_sel;
      PIGraph igraph;

      static BOOL CALLBACK dlgListSelect(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

         switch(uMsg){
         case WM_INITDIALOG:
            {
               S_hlp *hp = (S_hlp*)lParam;
               InitDlg(hp->igraph, hwnd);
               SetWindowText(hwnd, hp->title);
               const char *cp = hp->item_list;
               while(*cp){
                  SendDlgItemMessage(hwnd, IDC_ITEM_SELECT, LB_ADDSTRING, 0, (LPARAM)cp);
                  cp += strlen(cp) + 1;
               }
               SendDlgItemMessage(hwnd, IDC_ITEM_SELECT, LB_SETCURSEL, hp->curr_sel, 0);
               ShowWindow(hwnd, SW_SHOW);
            }
            return 1;
         case WM_COMMAND:
            switch(LOWORD(wParam)){
            case IDOK:
               {
                  EndDialog(hwnd, SendDlgItemMessage(hwnd, IDC_ITEM_SELECT, LB_GETCURSEL, 0, 0));
               }
               break;
            case IDCANCEL: 
               EndDialog(hwnd, -1); 
               break;
            case IDC_ITEM_SELECT:
               if(HIWORD(wParam) == LBN_DBLCLK){
                  EndDialog(hwnd, SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0));
               }
               break;
            }
            break;
         }
         return 0;
      }
   } hlp;
   hlp.title = title;
   hlp.item_list = item_list;
   hlp.curr_sel = curr_sel;
   hlp.igraph = igraph;

   if(!hwnd_parent)
      hwnd_parent = (HWND)igraph->GetHWND();
   return DialogBoxParam(GetHInstance(), "IDD_LISTSELECT", hwnd_parent,
      S_hlp::dlgListSelect, (LPARAM)&hlp);
}

//----------------------------

int GetPropertySize(I3D_PROPERTYTYPE ptype, dword value){

   switch(ptype){
   case I3DPROP_NULL: return 0;
   case I3DPROP_BOOL: return 1;
   case I3DPROP_INT: return sizeof(int);
   case I3DPROP_FLOAT: return sizeof(float);
   case I3DPROP_COLOR: return sizeof(S_vector);
   case I3DPROP_VECTOR3: return sizeof(S_vector);
   case I3DPROP_VECTOR4: return sizeof(S_vectorw);
   case I3DPROP_STRING: return strlen((const char*)value) + 1;
   case I3DPROP_ENUM: return 4;
   }
   assert(0);
   return 0;
}

//----------------------------

float GetModelBrightness(CPI3D_model mod){

   CPI3D_frame const *frms = mod->GetFrames();
   for(dword i=mod->NumFrames(); i--; ){
      CPI3D_frame frm = *frms++;
      if(frm->GetType()==FRAME_VISUAL)
         return I3DCAST_CVISUAL(frm)->GetBrightness(I3D_VIS_BRIGHTNESS_NOAMBIENT);
   }
   return 1.0f;
}

//----------------------------

void SetModelBrightness(PI3D_model mod, float brightness){

   const PI3D_frame *frms = mod->GetFrames();
   for(dword i=mod->NumFrames(); i--; ){
      PI3D_frame frm = *frms++;
      if(frm->GetType()==FRAME_VISUAL)
         I3DCAST_VISUAL(frm)->SetBrightness(I3D_VIS_BRIGHTNESS_NOAMBIENT, brightness);
   }
}

//----------------------------

void DisplayHelp(HWND hwnd, dword ctrl_id, const char *txt){

   RECT rc;
   GetWindowRect(GetDlgItem(hwnd, ctrl_id), &rc);
   POINT pt;
   pt.x = rc.left;
   pt.y = rc.bottom;

   ScreenToClient(hwnd, &pt);
   pt.x -= 40;
   pt.y += 2;
   OsDisplayHelpWindow(txt, hwnd, pt.x, pt.y, 150);
}

//----------------------------

void DisplayHelp(HWND hwnd, dword ctrl_id, const S_ctrl_help_info *hi){

   const char *txt = "<no help>";
   for(dword i=0; hi[i].ctrl_id; i++){
      if(hi[i].ctrl_id==ctrl_id){
         txt = hi[i].text;
         break;
      }
   }
   DisplayHelp(hwnd, ctrl_id, txt);
}

//----------------------------

bool GetBrowsePath(PC_editor ed, const char *title, C_str &filename, const char *init_dir, const char *extensions){

   char buf[256];
   OPENFILENAME on;
   memset(&on, 0, sizeof(on));
   on.lStructSize = sizeof(on);

   on.hwndOwner = (HWND)ed->GetIGraph()->GetHWND();
   on.lpstrFilter = extensions;

   on.nFilterIndex = 1;
   on.lpstrFile = buf;
   buf[0] = 0;
   on.nMaxFile = 256;
   on.lpstrInitialDir = init_dir;
   on.lpstrTitle = title;
   on.Flags |= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;// | OFN_EXPLORER;
   on.lpstrDefExt = "i3d";

   bool b = GetOpenFileName(&on);
   if(b)
      filename = buf;
   return b;
}

//----------------------------
//----------------------------
