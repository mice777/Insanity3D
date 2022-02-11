#include "all.h"
#include "common.h"
#include <win_res.h>

//----------------------------

class C_edit_SubEdit: public C_editor_item{
                              //returns plugin's name
   virtual const char *GetName() const{ return "SubEdit"; }

//----------------------------

   enum E_ACTION_SUBEDIT{
      E_SUBEDIT_PICK_DEF = 1000,
      E_SUBEDIT_PICK_SEL,
      E_SUBEDIT_NEW_PT,
      E_SUBEDIT_PICK_SEL_HIDDEN,
      E_SUBEDIT_SET_INDICIES,    //(int *indices) - set current selection (array terminated by -1)
      E_SUBEDIT_TOGGLE_INDEX,
      E_SUBEDIT_MOVE_XZ,
      E_SUBEDIT_MOVE_Y,
      E_SUBEDIT_SCALE,
      E_SUBEDIT_DELETE,
      E_SUBEDIT_CLR_SEL,         //(bool suppress_message)
      E_SUBEDIT_INV_SEL,
      E_SUBEDIT_SET_SEL_IN_RGN,  //(S_MouseEdit_rectangular_pick*)

      E_SUBEDIT_SET_VERTS,       // - internal (undo)
   };

//----------------------------

   C_smart_ptr<C_editor_item_MouseEdit> e_medit;
   C_smart_ptr<C_editor_item_Undo> e_undo;
   C_smart_ptr<I3D_material> mat_edit;

                              //currently edited frame
   C_smart_ptr<I3D_frame> edit_frm;
   C_vector<C_smart_ptr<I3D_frame> > hidden_frames;

   set<dword> selection;      //sub-selection (indicies)
   bool undo_reset;
   bool sel_msg_reset;
   float vertex_render_size;

//----------------------------

   void CreateEditMat(){

      PI3D_driver drv = ed->GetDriver();

      mat_edit = ed->GetDriver()->CreateMaterial();
      const char *bitmap_name = "SPHERE";
      I3D_CREATETEXTURE ct;
      memset(&ct, 0, sizeof(ct));
      ct.flags = TEXTMAP_NOMIPMAP;

      C_cache ck;
      if(OpenResource(GetHInstance(), "BINARY", bitmap_name, ck)){
         PI3D_texture tp;
         ct.ck_diffuse = &ck;
         ct.flags |= TEXTMAP_DIFFUSE | TEXTMAP_USE_CACHE | TEXTMAP_TRANSP;
         I3D_RESULT ir = drv->CreateTexture(&ct, &tp);
         if(I3D_SUCCESS(ir)){
            mat_edit->SetTexture(MTI_DIFFUSE, tp);
            tp->Release();
         }
      }
      mat_edit->Release();
   }

//----------------------------
// Get picked vertex index, or -1 if out of range.
   int GetPointedVertex(){

      PI3D_scene scene = ed->GetScene();
      S_vector from, dir;
      if(I3D_FAIL(scene->UnmapScreenPoint(ed->GetIGraph()->Mouse_x(), ed->GetIGraph()->Mouse_y(), from, dir)))
         return -1;

      S_plane pl_pick;
      pl_pick.normal = dir;
      pl_pick.d = -(pl_pick.normal.Dot(from));

      float best_dist = 1e+16f;
      int best_index = -1;
      assert(edit_frm);
      switch(edit_frm->GetType()){
      case FRAME_OCCLUDER:
         {
            const S_matrix &m = edit_frm->GetMatrix();
            PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);
            dword numv = occ->NumVertices();
            const S_vector *verts = occ->LockVertices();
            //float min_angle = vertex_render_size * PI * .1f;
            float min_dist = vertex_render_size * .5f;
            for(int i=numv; i--; ){
               S_vector v = verts[i] * m;
                              //check distance
               float d = v.DistanceToPlane(pl_pick);
               if(d > best_dist)
                  continue;

               float dl = v.DistanceToLine(from, dir);
               if(dl > min_dist)
                  continue;
               /*
                              //check angle
               float a = dir.AngleTo(v-from);
               if(a > min_angle)
                  continue;
                  */
               best_dist = d;
               best_index = i;
            }
            occ->UnlockVertices();
         }
         break;
      }
      return best_index;
   }

//----------------------------

   void HidePointedVisuals(){

      PI3D_scene scene = ed->GetScene();
      S_vector from, dir;
      if(I3D_SUCCESS(scene->UnmapScreenPoint(ed->GetIGraph()->Mouse_x(), ed->GetIGraph()->Mouse_y(), from, dir))){

         while(true){
            I3D_collision_data cd;
            cd.from = from;
            cd.dir = dir;
            cd.flags = I3DCOL_EXACT_GEOMETRY | I3DCOL_COLORKEY | I3DCOL_RAY;
            ed->GetScene()->TestCollision(cd);
            PI3D_frame frm = cd.GetHitFrm();
            if(!frm || frm->GetType()!=FRAME_VISUAL)
               break;
            frm->SetOn(false);
            hidden_frames.push_back(frm);
         }
      }
   }

//----------------------------

   void UnhideHiddenVisuals(){

      for(int i=hidden_frames.size(); i--; )
         hidden_frames[i]->SetOn(true);
      hidden_frames.clear();
   }

//----------------------------
// Write info about current selection.
   void DisplaySelectionInfo() const{

      if(!edit_frm)
         return;
      PC_editor_item_Selection e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      if(!e_slct)
         return;
      dword sb_index = e_slct->GetStatusBarIndex();
      dword num_items = 0;
      switch(edit_frm->GetType()){
      case FRAME_OCCLUDER:
         num_items = I3DCAST_COCCLUDER(edit_frm)->NumVertices();
         break;
      default: assert(0);
      }
      ed->Message(C_fstr("Subsel: %i/%i", selection.size(), num_items), sb_index);
   }

//----------------------------
// Clear selection and save into UNDO buffer.
   void SaveSelectionUndo(){

      int *mem = new int[selection.size()+1], *mp = mem;
      for(set<dword>::const_iterator it=selection.begin(); it!=selection.end(); it++)
         *mp++ = (*it);
      *mp = -1;
      e_undo->Save(this, E_SUBEDIT_SET_INDICIES, mem, (selection.size()+1)*sizeof(int));
      delete[] mem;
   }

//----------------------------
// Translate selection into specified direction.
   void TranslateSelection(const S_vector &delta){

      if(!selection.size())
         return;

      assert(edit_frm);

      S_vector loc_dir = delta;

      if(edit_frm->GetParent())
         loc_dir %= edit_frm->GetInvMatrix();

      switch(edit_frm->GetType()){
      case FRAME_OCCLUDER:
         {
            SaveVertexUndo();

            PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);

            dword numv = occ->NumVertices();
            const S_vector *verts = occ->LockVertices();
            C_vector<S_vector> new_verts; new_verts.assign(verts, verts+numv);
            C_vector<S_vector> save_verts = new_verts;

            set<dword>::const_iterator it;
            for(it=selection.begin(); it!=selection.end(); it++){
               dword indx = (*it);
               assert(indx<numv);
               new_verts[indx] += loc_dir;
            }
            occ->UnlockVertices();

            I3D_RESULT ir = occ->Build(&new_verts.front(), new_verts.size());
            if(I3D_FAIL(ir)){
               ed->Message("Failed to scale vertices.");
               occ->Build(&save_verts.front(), save_verts.size());
            }

            set<dword> new_sel;
            verts = occ->LockVertices();
            numv = occ->NumVertices();
            for(it=selection.begin(); it!=selection.end(); it++){
               const S_vector &v = new_verts[*it];
               for(int i=numv; i--; ){
                  if(IsMrgZeroLess((verts[i]-v).Magnitude()))
                     break;
               }
               if(i!=-1)
                  new_sel.insert(i);
            }
            selection = new_sel;
            occ->UnlockVertices();
         }
         break;
      }
      ed->SetModified();
   }

//----------------------------
// Scale selection.
   void ScaleSelection(float scl_delta){

      if(selection.size() <= 1)
         return;

      assert(edit_frm);

      float loc_scl = scl_delta;

      if(edit_frm->GetParent())
         loc_scl *= edit_frm->GetInvMatrix()(0).Magnitude();

      switch(edit_frm->GetType()){
      case FRAME_OCCLUDER:
         {
            SaveVertexUndo();

            PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);

            dword numv = occ->NumVertices();
            const S_vector *verts = occ->LockVertices();
            C_vector<S_vector> new_verts; new_verts.assign(verts, verts+numv);
            C_vector<S_vector> save_verts = new_verts;

            set<dword>::const_iterator it;
            S_vector sel_center(0, 0, 0);
            for(it=selection.begin(); it!=selection.end(); it++)
               sel_center += verts[*it];
            sel_center /= (float)selection.size();

            for(it=selection.begin(); it!=selection.end(); it++){
               dword indx = (*it);
               assert(indx<numv);
               S_vector &v = new_verts[indx];
               S_vector dir = v - sel_center;
               v = sel_center;
               if(dir.Magnitude() > .01f)
                  v += dir * loc_scl;
            }
            occ->UnlockVertices();
            I3D_RESULT ir = occ->Build(&new_verts.front(), new_verts.size());
            if(I3D_FAIL(ir)){
               ed->Message("Failed to scale vertices.");
               occ->Build(&save_verts.front(), save_verts.size());
            }

            set<dword> new_sel;
            verts = occ->LockVertices();
            numv = occ->NumVertices();
            for(it=selection.begin(); it!=selection.end(); it++){
               const S_vector &v = new_verts[*it];
               for(int i=numv; i--; ){
                  if(IsMrgZeroLess((verts[i]-v).Magnitude()))
                     break;
               }
               if(i!=-1)
                  new_sel.insert(i);
            }
            selection = new_sel;
            occ->UnlockVertices();
         }
         break;
      }
      ed->SetModified();
   }

//----------------------------

   void DeleteSelection(){

      assert(edit_frm);
      if(!selection.size()){
         ed->Message("No vertex deleted.");
         return;
      }

      int num_del = 0;
      bool ok = false;

      switch(edit_frm->GetType()){
      case FRAME_OCCLUDER:
         {
            PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);
            SaveSelectionUndo();
            SaveVertexUndo();

            dword numv = occ->NumVertices();
            const S_vector *verts = occ->LockVertices();
            C_vector<S_vector> new_verts;
            new_verts.reserve(numv);

            num_del = selection.size();
            set<dword>::const_iterator it;
            for(int i=numv; i--; ){
                              //if index is not in selection, add it
               it = selection.find(i);
               if(it==selection.end())
                  new_verts.push_back(verts[i]);
            }
            occ->UnlockVertices();
            if(new_verts.size() < 4){
               ed->Message("Failed to delete vertex.");
            }else{
               occ->Build(&new_verts.front(), new_verts.size());
               selection.clear();
               ok = true;
            }
         }
         break;
      }
      if(ok){
         ed->SetModified();
         ed->Message(C_fstr("Vertices deleted: %i.", num_del));
         sel_msg_reset = true;
      }
   }

//----------------------------

   void SaveVertexUndo(){

      assert(edit_frm);

      bool undo_on = (!e_undo->IsTopEntry(this, E_SUBEDIT_SET_VERTS));
      if(!undo_on)
         return;

      switch(edit_frm->GetType()){
      case FRAME_OCCLUDER:
         {
            PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);

            dword numv = occ->NumVertices();
            const S_vector *verts = occ->LockVertices();
            C_vector<dword> undo_info;
            undo_info.push_back(numv);
            for(dword i=0; i<numv; i++){
               undo_info.push_back(I3DFloatAsInt(verts[i].x));
               undo_info.push_back(I3DFloatAsInt(verts[i].y));
               undo_info.push_back(I3DFloatAsInt(verts[i].z));
            }
            e_undo->Save(this, E_SUBEDIT_SET_VERTS, &undo_info.front(), undo_info.size()*sizeof(dword));
         }
         break;
      }
      undo_reset = false;
   }

//----------------------------

   virtual void BeforeFree(){
      UnhideHiddenVisuals();
   }

//----------------------------

   virtual bool SubObjectEdit(){

      PC_editor_item_Selection e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      PI3D_frame frm = e_slct->GetSingleSel();
      if(!frm)
         return false;

      bool ok = false;
      switch(frm->GetType()){
      case FRAME_OCCLUDER:
         if(I3DCAST_OCCLUDER(frm)->GetOccluderType()==I3DOCCLUDER_MESH)
            ok = true;
         break;
      }
      if(!ok)
         return false;
      S_MouseEdit_user_mode m;
      memset(&m, 0, sizeof(m));
      m.ei = this;
      for(int i=0; i<S_MouseEdit_user_mode::MODE_LAST; i++){
         S_MouseEdit_user_mode::S_mode &md = m.modes[i];
         switch(i){
         case S_MouseEdit_user_mode::MODE_DEFAULT:
            md.hcursor = LoadCursor(GetHInstance(), "IDC_OCCLUDER_DEF");
            md.action_id = E_SUBEDIT_PICK_DEF;
            break;
         case S_MouseEdit_user_mode::MODE_SELECT:
            md.hcursor = LoadCursor(GetHInstance(), "IDC_OCCLUDER_SEL");
            md.action_id = E_SUBEDIT_PICK_SEL;
            break;
         case S_MouseEdit_user_mode::MODE_LINK:
            md.hcursor = LoadCursor(GetHInstance(), "IDC_OCCLUDER_NEW");
            md.action_id = E_SUBEDIT_NEW_PT;
            break;
         case S_MouseEdit_user_mode::MODE_PICK_SPEC:
            md.hcursor = LoadCursor(GetHInstance(), "IDC_OCCLUDER_SEL1");
            md.action_id = E_SUBEDIT_PICK_SEL_HIDDEN;
            break;
         case S_MouseEdit_user_mode::MODE_MOVE_XZ: md.action_id = E_SUBEDIT_MOVE_XZ; break;
         case S_MouseEdit_user_mode::MODE_MOVE_Y: md.action_id = E_SUBEDIT_MOVE_Y; break;
         case S_MouseEdit_user_mode::MODE_SCALE: md.action_id = E_SUBEDIT_SCALE; break;
         case S_MouseEdit_user_mode::MODE_DELETE: md.action_id = E_SUBEDIT_DELETE; break;
         case S_MouseEdit_user_mode::MODE_CLEAR_SEL: md.action_id = E_SUBEDIT_CLR_SEL; break;
         case S_MouseEdit_user_mode::MODE_INV_SEL: md.action_id = E_SUBEDIT_INV_SEL; break;
         case S_MouseEdit_user_mode::MODE_SEL_IN_RECT: md.action_id = E_SUBEDIT_SET_SEL_IN_RGN; break;
         case S_MouseEdit_user_mode::MODE_ROTATE: break;
         case S_MouseEdit_user_mode::MODE_DOUBLECLICK: break;
         default: assert(0);
         }
      }
      e_medit->SetUserMode(&m);
      assert(!edit_frm);
      edit_frm = frm;
      Tick(0, 0, 0, 0, 0, 0);
      sel_msg_reset = true;
      return true;
   }

//----------------------------

   virtual void SubObjEditCancel(){
      if(edit_frm){
         edit_frm = NULL;
         selection.clear();
      }
   }

//----------------------------
public:
   C_edit_SubEdit():
      undo_reset(true),
      sel_msg_reset(false),
      vertex_render_size(0.0f)
   {
   }

//----------------------------

   virtual bool Init(){

      e_medit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
      e_undo = (PC_editor_item_Undo)ed->FindPlugin("Undo");
      if(!e_medit || !e_undo){
         e_medit = NULL;
         e_undo = NULL;
         return false;
      }

      return true;
   }

//----------------------------
                              //uninitialize plugin
   virtual void Close(){

      e_medit = NULL;
      mat_edit = NULL;
   }

//----------------------------
                              //tick - performed in every rendering frame
   virtual void Tick(byte skeys, int time, int mouse_rx, int mouse_ry, int mouse_rz, byte mouse_butt){

      UnhideHiddenVisuals();
      if(edit_frm){
                              //precompute several values
         switch(edit_frm->GetType()){
         case FRAME_OCCLUDER:
            {
               PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);
               dword numv = occ->NumVertices();
               const S_vector *verts = occ->LockVertices();

                              //get occluder's bbox
               I3D_bbox bb;
               bb.Invalidate();
               for(int i=numv; i--; ){
                  const S_vector &v = verts[i];
                  bb.min.Minimal(v);
                  bb.max.Maximal(v);
               }
               float diag = (bb.max - bb.min).Magnitude();
               diag *= edit_frm->GetMatrix()(0).Magnitude();
               vertex_render_size = diag * .025f;

               occ->UnlockVertices();
            }
            break;
         }

         //if(skeys==(SKEY_SHIFT|SKEY_CTRL|SKEY_ALT))
         if((mouse_butt&4) && e_medit->GetCurrentMode()==0){
            HidePointedVisuals();
         }
      }
      if(sel_msg_reset){
         DisplaySelectionInfo();
         sel_msg_reset = false;
      }
   }

//----------------------------
                              //action - called from menu/keypress, or by other plugins
   virtual dword Action(int id, void *context){

      switch(id){

      case E_SUBEDIT_PICK_DEF:
         {
            undo_reset = true;
            SaveSelectionUndo();
            selection.clear();
            int indx = GetPointedVertex();
            if(indx!=-1)
               selection.insert(indx);
            sel_msg_reset = true;
         }
         break;

      case E_SUBEDIT_PICK_SEL:
      case E_SUBEDIT_PICK_SEL_HIDDEN:
         if(!(ed->GetIGraph()->GetMouseButtons()&4)){
            undo_reset = true;
            int indx = GetPointedVertex();
            if(indx==-1)
               break;
            Action(E_SUBEDIT_TOGGLE_INDEX, &indx);
         }
         break;

      case E_SUBEDIT_NEW_PT:
         {
            int indx = GetPointedVertex();
            if(indx!=-1){
               ed->Message("Can't create new point over another point.");
               break;
            }
            const S_MouseEdit_picked *mp = (const S_MouseEdit_picked*)context;
            if(!mp->frm_picked){
               ed->Message("Can't create new point in air.");
               break;
            }

            switch(edit_frm->GetType()){
            case FRAME_OCCLUDER:
               {
                  SaveSelectionUndo();
                  selection.clear();
                  SaveVertexUndo();

                  PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);

                  dword numv = occ->NumVertices();
                  const S_vector *verts = occ->LockVertices();
                  C_vector<S_vector> new_verts; new_verts.assign(verts, verts+numv);
                  occ->UnlockVertices();

                  S_vector v = mp->pick_from + mp->pick_dir * mp->pick_dist;
                  v *= occ->GetInvMatrix();

                  new_verts.push_back(v);
                  occ->Build(&new_verts.front(), new_verts.size());

                  verts = occ->LockVertices();
                  for(int i=occ->NumVertices(); i--; ){
                     if(IsMrgZeroLess((verts[i]-v).Magnitude()))
                        break;
                  }
                  occ->UnlockVertices();
                  if(i!=-1){
                     selection.insert(i);
                     ed->Message("Vertex created.");
                  }else{
                     ed->Message("New vertex is fully inside of old convex hull.");
                  }
                  sel_msg_reset = true;
               }
               break;
            }
            undo_reset = true;
         }
         break;

      case E_SUBEDIT_MOVE_XZ:
      case E_SUBEDIT_MOVE_Y:
         {
            const S_vector *delta = (const S_vector*)context;
            TranslateSelection(*delta);
         }
         break;

      case E_SUBEDIT_SCALE:
         {
            float scl = I3DIntAsFloat((int)context);
            ScaleSelection(scl);
         }
         break;

      case E_SUBEDIT_SET_VERTS:
         {
            if(!edit_frm){
               ed->Message("E_SUBEDIT_SET_VERTS: not in subedit mode");
               break;
            }
            SaveVertexUndo();
            switch(edit_frm->GetType()){
            case FRAME_OCCLUDER:
               {
                  PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);
                  int numv = *((dword*&)context)++;
                  const S_vector *verts = (const S_vector*)context;
                  occ->Build(verts, numv);
               }
               break;
            }
         }
         break;

      case E_SUBEDIT_CLR_SEL:
         if(selection.size()){
                              //save undo
            SaveSelectionUndo();
            selection.clear();
            sel_msg_reset = true;
         }
         break;

      case E_SUBEDIT_INV_SEL:
         {
            SaveSelectionUndo();

            dword num_items = 0;
            switch(edit_frm->GetType()){
            case FRAME_OCCLUDER:
               num_items = I3DCAST_OCCLUDER(edit_frm)->NumVertices();
               break;
            default: assert(0);
            }
            C_vector<bool> b_set(num_items, false);
            for(set<dword>::const_iterator it=selection.begin(); it!=selection.end(); it++)
               b_set[*it] = true;
            selection.clear();
            for(dword i=num_items; i--; ){
               if(!b_set[i])
                  selection.insert(i);
            }
            sel_msg_reset = true;
         }
         break;

      case E_SUBEDIT_DELETE:
         DeleteSelection();
         break;

      case E_SUBEDIT_TOGGLE_INDEX:
         {
            int indx = *(int*)context;
            e_undo->Save(this, E_SUBEDIT_TOGGLE_INDEX, &indx, sizeof(int));
            set<dword>::iterator it = selection.find(indx);
            if(it!=selection.end()) selection.erase(it);
            else selection.insert(indx);
            sel_msg_reset = true;
         }
         break;

      case E_SUBEDIT_SET_INDICIES:
         {
            SaveSelectionUndo();
            selection.clear();
            int *ip = (int*)context;
            while(*ip!=-1)
               selection.insert(*ip++);
            sel_msg_reset = true;
         }
         break;

      case E_SUBEDIT_SET_SEL_IN_RGN:
         {
            const S_MouseEdit_rectangular_pick *pick_data = (S_MouseEdit_rectangular_pick*)context;
                              //clear selection first
            Action(E_SUBEDIT_CLR_SEL, 0);
                              //determine all points inside
            switch(edit_frm->GetType()){
            case FRAME_OCCLUDER:
               {
                  PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);
                  const S_matrix &m = occ->GetMatrix();
                  dword num_v = occ->NumVertices();
                  const S_vector *verts = occ->LockVertices();
                  for(dword vi=num_v; vi--; ){
                     S_vector v = verts[vi] * m;
                     for(int i=5; i--; ){
                        float d = v.DistanceToPlane(pick_data->vf[i]);
                        if(d >= 0.0f)
                           break;
                     }
                     if(i==-1)
                        selection.insert(vi);
                  }
                  occ->UnlockVertices();
               }
               break;
            }
         }
         break;
      }
      return 0;
   }

//----------------------------
                              //called for registered plugins during rendering
   virtual void Render(){

      if(!edit_frm)
         return;
      PI3D_scene scene = ed->GetScene();

      switch(edit_frm->GetType()){
      case FRAME_OCCLUDER:
         {
            PI3D_occluder occ = I3DCAST_OCCLUDER(edit_frm);

            if(!mat_edit)
               CreateEditMat();

            S_vector draw_offset = scene->GetActiveCamera()->GetWorldDir() * -.05f;
            S_matrix m = occ->GetMatrix();
            m(3) += draw_offset;

            int pointed = GetPointedVertex();
            dword numv = occ->NumVertices();
            const S_vector *verts = occ->LockVertices();
            set<dword>::const_iterator it;

                              //render verices
            for(int i=numv; i--; ){
               it = selection.find(i);
               if(it!=selection.end())
                  continue;
               scene->DrawSprite(verts[i] * m, mat_edit, 0x8000ff00, vertex_render_size);
            }
                              //render selection
            for(it=selection.begin(); it!=selection.end(); it++){
               int indx = (*it);
               scene->DrawSprite(verts[indx] * m, mat_edit, 0xffffff00, vertex_render_size*1.25f);
            }
            if(pointed!=-1){
               m(3) += draw_offset*.5f;
               scene->DrawSprite(verts[pointed] * m, mat_edit, 0xffffffff, vertex_render_size*.75f);
            }
            occ->UnlockVertices();
         }
         break;
      }
   }

//----------------------------
                              //store/restore plugin's state
   //virtual bool LoadState(C_chunk &ck){ return false; }
   //virtual bool SaveState(C_chunk &ck) const{ return false; }
};

//----------------------------

void CreateSubEdit(PC_editor ed){

   PC_editor_item ei = new C_edit_SubEdit;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
