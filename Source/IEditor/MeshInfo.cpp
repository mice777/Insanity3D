#include "all.h"
#include "common.h"
#include <win_res.h>


//----------------------------

#define COLOR_FACE 0x8000ff00
#define COLOR_VERTEX 0xff0000ff

//----------------------------

class C_edit_MeshInfo: public C_editor_item{
   virtual const char *GetName() const{ return "MeshInfo"; }

   C_smart_ptr<C_editor_item_Selection> e_slct;
   enum{
      E_MI_TOGGLE = 1000,
      E_MI_SEL_CHANGE,
      E_MI_PICKED,
   };
   C_str frm_name;
   bool enabled;
   bool reset;

   C_smart_ptr<I3D_material> mat_letters;

                              //currently selected frame, NULL if no suitable found
   C_smart_ptr<I3D_frame> sel_frm;
                              //all letters
   struct S_letter_info{
      int num;
      S_vector pos;
      S_vector norm;
      float size;
      dword color;
   };
   C_vector<S_letter_info> letters;
                              //lines
   C_vector<S_vector> v_lines[2];
   C_vector<word> i_lines[2];

//----------------------------

   void BuildInfo(){

      DeleteInfo();

      PI3D_frame frm = NULL;
      if(frm_name.Size())
         frm = ed->GetScene()->FindFrame(frm_name);
      if(!frm){
         const C_vector<PI3D_frame> &sel_list = e_slct->GetCurSel();
         if(sel_list.size()==1)
            frm = sel_list.front();
      }
      if(frm && frm->GetType()==FRAME_VISUAL){
         PI3D_visual vis = I3DCAST_VISUAL(frm);
         PI3D_mesh_base mb = vis->GetMesh();
         if(mb){
            sel_frm = frm;

            const void *verts = mb->LockVertices();
            int numv = mb->NumVertices();
            dword vstride = mb->GetSizeOfVertex();
            dword numf = mb->NumFaces();
            //CPI3D_triface fcs = mb->LockFaces();
            C_buffer<I3D_triface> fcs(numf);
            mb->GetFaces(fcs.begin());
                           //cached square roots of face sizes
            C_vector<float> face_sizes(numf);
                           //build faces' letters
            for(int fi=numf; fi--; ){
               const I3D_triface &fc = fcs[fi];
               const S_vector &v0 = *(S_vector*)(((byte*)verts) + fc[0]*vstride);
               const S_vector &v1 = *(S_vector*)(((byte*)verts) + fc[1]*vstride);
               const S_vector &v2 = *(S_vector*)(((byte*)verts) + fc[2]*vstride);

               letters.push_back(S_letter_info());
               S_letter_info &li = letters.back();
               li.num = fi;
               li.pos = (v0 + v1 + v2) / 3.0f;
               float fc_sz = (float)sqrt(fc.ComputeSurfaceArea((const S_vector*)verts, vstride));
               face_sizes[fi] = fc_sz;
               li.size = fc_sz * .1f;
               li.color = COLOR_FACE;

               li.norm.GetNormal(v0, v1, v2);
               li.norm.Normalize();
               S_vector norm = li.norm * (li.size * 2.0f);

               i_lines[0].push_back((word)v_lines[0].size());
               i_lines[0].push_back((word)(v_lines[0].size()+1));
               v_lines[0].push_back(li.pos);
               v_lines[0].push_back(li.pos+norm);
            }

                           //build vertices' letters
            for(int vi=numv; vi--; ){
               const S_vector &v = *(S_vector*)(((byte*)verts) + vi*vstride);
               const S_vector &n = (&v)[1];

               letters.push_back(S_letter_info());
               S_letter_info &li = letters.back();
               li.num = vi;
               li.color = COLOR_VERTEX;
               li.norm = n;

               int best_fi = -1;
               float best_a = 0.0f;
                           //find most suitable face
               for(int fi=numf; fi--; ){
                  const I3D_triface &fc = fcs[fi];
                  int ii = fc.FindIndex((word)vi);
                  if(ii!=-1){
                     const S_vector &v1 = *(S_vector*)(((byte*)verts) + fc[(ii+1)%3]*vstride);
                     const S_vector &v2 = *(S_vector*)(((byte*)verts) + fc[(ii+2)%3]*vstride);

                     S_vector dir1 = v1 - v;
                     S_vector dir2 = v2 - v;
                     float a = 0.0f;
                     {
                        a = face_sizes[fi];
                        a *= a;
                        a *= dir1.AngleTo(dir2);
                     }
                     if(best_a <= a){
                        best_a = a;
                        best_fi = fi;
                     }
                  }
               }
               li.pos = v;
               if(best_fi!=-1){
                           //compute size and position of letter
                  const I3D_triface &fc = fcs[best_fi];
                  int ii = fc.FindIndex((word)vi);
                  const S_vector &v1 = *(S_vector*)(((byte*)verts) + fc[(ii+1)%3]*vstride);
                  const S_vector &v2 = *(S_vector*)(((byte*)verts) + fc[(ii+2)%3]*vstride);
                  S_vector edge_center = (v2 + v1) * .5f;
                  S_vector dir = edge_center - v;
                  li.pos += dir * .15f;

                  float fc_sz = face_sizes[best_fi];
                  float sz = fc_sz * .06f;

                  li.size = sz;
               }else{
                  li.size = .05f;
                           //unused verts by red
                  li.color = 0x40ff0000;
               }
               i_lines[1].push_back((word)v_lines[1].size());
               i_lines[1].push_back((word)(v_lines[1].size()+1));
               v_lines[1].push_back(v);
               v_lines[1].push_back(v + n * (li.size*2.0f));
            }
            //mb->UnlockFaces();
            mb->UnlockVertices();
         }
      }
   }

//----------------------------

   void DeleteInfo(){

      letters.clear();
      sel_frm = NULL;
      for(int i=0; i<2; i++){
         v_lines[i].clear();
         i_lines[i].clear();
      }
   }

//----------------------------

   virtual void AfterLoad(){
      reset = true;
      DeleteInfo();
   }
   virtual void BeforeFree(){
      reset = true;
      DeleteInfo();
   }

//----------------------------

public:
   C_edit_MeshInfo():
      reset(true),
      enabled(false)
   {}

   virtual bool Init(){
      e_slct = (PC_editor_item_Selection)ed->FindPlugin("Selection");
      if(!e_slct)
         return false;
      {
         PI3D_driver drv = ed->GetDriver();

         C_cache ck;
         if(OpenResource(GetHInstance(), "BINARY", "LETTERS", ck)){
            I3D_CREATETEXTURE ct;
            memset(&ct, 0, sizeof(ct));
            ct.flags = TEXTMAP_MIPMAP | TEXTMAP_DIFFUSE | TEXTMAP_USE_CACHE | TEXTMAP_TRANSP;

            PI3D_texture tp;
            ct.ck_diffuse = &ck;
            I3D_RESULT ir = drv->CreateTexture(&ct, &tp);
            if(I3D_SUCCESS(ir)){
               PI3D_material mat = drv->CreateMaterial();
               mat_letters = mat;
               mat->SetTexture(MTI_DIFFUSE, tp);
               mat->Release();
               tp->Release();
            }
         }
      }

#define DS "&View\\&Debug\\"
      ed->AddShortcut(this, E_MI_TOGGLE, DS"Mesh info\tCtrl+Alt+Shift+I", K_I, SKEY_CTRL|SKEY_ALT|SKEY_SHIFT);

      ed->CheckMenu(this, E_MI_TOGGLE, enabled);

                              //receive notifications from selection
      e_slct->AddNotify(this, E_MI_SEL_CHANGE);
      return true;
   }

//----------------------------

   virtual dword Action(int id, void *context){

      switch(id){
      case E_MI_TOGGLE:
         {
            enabled = !enabled;
            ed->CheckMenu(this, E_MI_TOGGLE, enabled);
            ed->Message(C_fstr("Mesh info %s", enabled ? "enebled" : "disabled"));

            PC_editor_item_MouseEdit m_edit = (PC_editor_item_MouseEdit)ed->FindPlugin("MouseEdit");
            if(enabled){
                                 //switch to user pick mode
               m_edit->SetUserPick(this, E_MI_PICKED, LoadCursor(GetHInstance(), "IDC_MESH_INFO"));

               ed->Message("Select mesh for detailed info");
            }else{
               if(m_edit->GetUserPick()==this)
                  m_edit->SetUserPick(NULL, 0);
            }
         }
         break;

      case E_MI_PICKED:
         {
            S_MouseEdit_picked *mp = (S_MouseEdit_picked*)context;
            if(mp->frm_picked){
               frm_name = mp->frm_picked->GetName();
            }else{
               frm_name = NULL;
            }
            ed->Message(C_fstr("Select mesh: %s", frm_name.Size() ? (const char*)frm_name : "<selection>"));
            reset = true;
            return true;
         }
         break;

      case E_MI_SEL_CHANGE:
         if(frm_name.Size())
            break;
         reset = true;
         DeleteInfo();
         break;
      }
      return 0;
   }

//----------------------------

   virtual void Render(){

      if(!enabled)
         return;
      if(reset){
         BuildInfo();
         reset = false;
      }
      if(!sel_frm)
         return;

      PI3D_scene scn = ed->GetScene();
      PI3D_driver drv = ed->GetDriver();

      bool is_wire = drv->GetState(RS_WIREFRAME);
      if(is_wire) drv->SetState(RS_WIREFRAME, false);

      const S_matrix &m_cam = scn->GetActiveCamera()->GetMatrix();
      const S_vector &right = m_cam(0);

      const S_matrix &tm = sel_frm->GetMatrix();
      float m_scale = tm(0).Magnitude();

      scn->SetRenderMatrix(tm);
      scn->DrawLines(&v_lines[0].front(), v_lines[0].size(), &i_lines[0].front(), i_lines[0].size(), COLOR_FACE);
      scn->DrawLines(&v_lines[1].front(), v_lines[1].size(), &i_lines[1].front(), i_lines[1].size(), COLOR_VERTEX);
                              //render all letters
      for(int i=letters.size(); i--; ){
         const S_letter_info &li = letters[i];
         int num = li.num;
         float sz = li.size * m_scale;

         for(int num_digits=1, tmp=num; tmp>=10; tmp /= 10, ++num_digits);
         S_vector shift = right * (sz*.8f);
         //const S_vector z_offset = m_cam(2) * -sz * .5f;
         const S_vector z_offset = li.norm * sz;
         S_vector pos = (li.pos * tm) + z_offset + shift * ((num_digits-1)*.5f);
         for(int di=0, curr=num; di<num_digits; di++, pos -= shift, curr /= 10){
            const float WIDTH = (12.0f / 128.0f);
            I3D_text_coor tx[2];
            tx[0].y = 0.0f;
            tx[1].y = 1.0f;
            tx[0].x = (float)(curr%10) * WIDTH;
            tx[1].x = tx[0].x + WIDTH;
            scn->DrawSprite(pos, mat_letters, li.color, sz, tx);
         }
      }

      if(is_wire) drv->SetState(RS_WIREFRAME, true);
   }

//----------------------------

   virtual bool SaveState(C_chunk &ck) const{

      ck.Write(&enabled, sizeof(enabled));
      ck.Write(&frm_name[0], frm_name.Size()+1);
      return true;
   }

   virtual bool LoadState(C_chunk &ck){

      ck.Read(&enabled, sizeof(enabled));
      frm_name = ck.ReadString();

      ed->CheckMenu(this, E_MI_TOGGLE, enabled);
      reset = true;
      return true;
   }
};

//----------------------------

void CreateMeshInfo(PC_editor ed){
   PC_editor_item ei = new C_edit_MeshInfo;
   ed->InstallPlugin(ei);
   ei->Release();
}

//----------------------------
