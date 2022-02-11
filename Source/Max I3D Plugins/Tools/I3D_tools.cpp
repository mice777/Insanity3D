/*----------------------------------------------------------------------*
 |
 | Insanity3D Tools plugin for 3DS MAX
 |
 | Author: Michal Bacik
 |
 *----------------------------------------------------------------------*/

#include "..\common\pch.h"
#include <utilapi.h>
#include "resource.h"
#include "..\common\common.h"

//----------------------------

extern ClassDesc *GetPluginDesc();

//----------------------------

HINSTANCE hInstance;

//----------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL,ULONG fdwReason,LPVOID lpvReserved){

   hInstance = hinstDLL;
   static bool controls_init = false;
   if(!controls_init){
      controls_init = true;
      InitCustomControls(hInstance);
      InitCommonControls();
   }
   return true;
}

//----------------------------

__declspec( dllexport ) const TCHAR* LibDescription(){
   return "I3D Tools";
}

//----------------------------

__declspec (dllexport) int LibNumberClasses(){
   return 1;
}

//----------------------------

__declspec(dllexport) ClassDesc *LibClassDesc(int i){
   switch(i) {
   case 0: return GetPluginDesc();
   default: return 0;
   }
}

//----------------------------

__declspec( dllexport ) ULONG LibVersion(){
   return VERSION_3DSMAX;
}

//----------------------------

TCHAR *GetString(int id){
   static TCHAR buf[256];

   if(hInstance)
      return LoadString(hInstance, id, buf, sizeof(buf)) ? buf : NULL;
   return NULL;
}

//----------------------------

class C_I3D_tools: public UtilityObj{

   INode *xref_node;

//----------------------------

   static bool FacesEqual(const Face &f0, const Face &f1){

      if(f0.v[0]==f1.v[0] && f0.v[1]==f1.v[1] && f0.v[2]==f1.v[2])
         return true;
      if(f0.v[0]==f1.v[1] && f0.v[1]==f1.v[2] && f0.v[2]==f1.v[0])
         return true;
      if(f0.v[0]==f1.v[2] && f0.v[1]==f1.v[0] && f0.v[2]==f1.v[1])
         return true;
      return false;
   }

//----------------------------

   bool WeldMeshVertices(Mesh &m, C_str &msg, const char *obj_name, float thresh){

                              //collect vertices into pool
      dword num_v = m.getNumVerts();
      C_buffer<S_vector> verts(num_v);
      
      for(dword i=num_v; i--; ){
         verts[i] = (S_vector&)m.getVert(i);
      }
      C_buffer<dword> v_map(num_v);
                              //determine which vertices are unique, and which map to others
      MakeVertexMapping(verts.begin(), sizeof(S_vector), num_v, v_map.begin(), thresh);

      dword num_unused = 0;
                              //detect unused vertices
      {
         C_buffer<bool> v_used(num_v, false);
         for(i=m.getNumFaces(); i--; ){
            dword *fc = m.faces[i].v;
            v_used[fc[0]] = true;
            v_used[fc[1]] = true;
            v_used[fc[2]] = true;
         }
         for(i=num_v; i--; ){
            if(!v_used[i]){
               v_map[i] = 0xffffffff;
               ++num_unused;
            }
         }
      }

                              //compact vertices, and create remapping info in v_map
      for(dword si=0, di=0; si<num_v; ++si){
         dword map_i = v_map[si];
         if(map_i==0xffffffff)
            continue;
         if(v_map[map_i]==0xffffffff)
            map_i = si;
         if(map_i==si){
            if(si!=di)
               m.getVert(di) = m.getVert(si);
            v_map[si] = di;
            ++di;
         }else{
            v_map[si] = v_map[map_i];
         }
      }
                              //remap faces
      for(i=m.getNumFaces(); i--; ){
         dword *fc = m.faces[i].v;
         fc[0] = v_map[fc[0]];
         fc[1] = v_map[fc[1]];
         fc[2] = v_map[fc[2]];
         assert(fc[0]<di && fc[1]<di && fc[2]<di);
      }
      if(num_v!=di){
                              //change number of vertices
         m.setNumVerts(di, true);

         msg += C_str(C_xstr("Mesh '%': % vertices weld") %obj_name %(num_v-di-num_unused));
         if(num_unused)
            msg += C_str(C_xstr(", % unused killed") %num_unused);
         msg += "\n";
      }
      return (num_v!=di);
   }

//----------------------------

   void SnapMeshVertices(Mesh &m, float thresh){

      dword num_v = m.getNumVerts();
      for(dword i=num_v; i--; ){
         S_vector &v = (S_vector&)m.getVert(i);
         for(int j=0; j<3; j++){
            float &val = v[j];
            float f = val / thresh;
            f = floor(f+.5f);
            val = f * thresh;
         }
      }
      m.buildBoundingBox();
   }

//----------------------------

   void CreateXrefPoint(){

      if(xref_node)
         return;

      Object *obj = (Object*)ip->CreateInstance(HELPER_CLASS_ID, Class_ID(POINTHELP_CLASS_ID, 0));
      assert(obj);
   
      IDerivedObject *dobj = CreateDerivedObject(obj);

      ip->ClearNodeSelection();
      INode *n = ip->CreateObjectNode(dobj);

                              //add xref to selection
      ip->SelectNode(n, false);
      xref_node = n;
   
      xref_node->SetName("Xref");
   }

//----------------------------

   void MakeControlsTCB(Control *control){

      const dword inherit = INHERIT_ALL;
      control->SetInheritanceFlags(inherit, false);

      Control *c;
      c = control->GetPositionController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_POSITION_CLASS_ID,0)){
         Control *tcb = (Control*)ip->CreateInstance(CTRL_POSITION_CLASS_ID, Class_ID(TCBINTERP_POSITION_CLASS_ID,0));
         if(!control->SetPositionController(tcb))
            tcb->DeleteThis();
      }

      c = control->GetRotationController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_ROTATION_CLASS_ID,0)){
         Control *tcb = (Control*)ip->CreateInstance(CTRL_ROTATION_CLASS_ID, Class_ID(TCBINTERP_ROTATION_CLASS_ID,0));
         if(!control->SetRotationController(tcb))
            tcb->DeleteThis();
      }

      c = control->GetRollController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_FLOAT_CLASS_ID,0)){
         Control *tcb = (Control*)ip->CreateInstance(CTRL_FLOAT_CLASS_ID, Class_ID(TCBINTERP_FLOAT_CLASS_ID,0));
         if(!control->SetRollController(tcb))
            tcb->DeleteThis();
      }

      c = control->GetScaleController();
      if(c && c->ClassID()!=Class_ID(TCBINTERP_SCALE_CLASS_ID,0)){
         Control *tcb = (Control*)ip->CreateInstance(CTRL_SCALE_CLASS_ID, Class_ID(TCBINTERP_SCALE_CLASS_ID,0));
         if(!control->SetScaleController(tcb))
            tcb->DeleteThis();
      }
   }

//----------------------------

   BOOL CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){

      switch(msg){
      case WM_INITDIALOG:
         Init(hwnd);
         ShowSelectionText(hwnd);
         SetDlgItemInt(hwnd, IDC_EDIT_UV, 2, false);
         SetDlgItemText(hwnd, IDC_EDIT_WELD_DIST, "0.001");
         SetDlgItemText(hwnd, IDC_EDIT_SNAP_DIST, "0.05");
         return true;

      case WM_DESTROY:
         Destroy(hwnd);
         return true;

      case WM_NOTIFY:
         {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            switch(nmhdr->idFrom){
            case IDC_SPIN_SNAP:
               switch(nmhdr->code){
               case UDN_DELTAPOS:
                  {
                     NMUPDOWN *ud = (NMUPDOWN*)lParam;
                     int delta = -ud->iDelta;

                     float f = 0;
                     char buf[256];
                     GetDlgItemText(hwnd, IDC_EDIT_SNAP_DIST, buf, sizeof(buf));
                     sscanf(buf, "%f", &f);
                     f += delta*.01f;
                     SetDlgItemText(hwnd, IDC_EDIT_SNAP_DIST, C_xstr("#.2%") %f);
                  }
                  break;
               }
               break;
            }
         }
         break;

      case WM_COMMAND:
         {
            dword id = LOWORD(wParam);
            switch(id){

            case IDC_XREF_REFRESH:
               {
                  HWND hwnd_xl = GetDlgItem(hwnd, IDC_LIST_XREFS);
                  SendMessage(hwnd_xl, LB_RESETCONTENT, 0, 0);
                  //ip->SetIncludeXRefsInHierarchy(true);

                  INode *root = ip->GetRootNode();
                  int n = root->GetXRefFileCount();
                  for(int i=n; i--; ){
                     TSTR fn = root->GetXRefFileName(i);
                     for(int ni=strlen(fn); ni--; ){
                        if(fn[ni]=='\\')
                           break;
                     }
                     int ii = SendMessage(hwnd_xl, LB_ADDSTRING, 0,
                        LPARAM((const char*)C_str(C_xstr("%. %") %(i+1) %(fn+ni+1))));
                     SendMessage(hwnd_xl, LB_SETITEMDATA, ii, i);
                  }
               }
               break;

            case IDC_LIST_XREFS:
               if(HIWORD(wParam)==LBN_SELCHANGE){
                  HWND hwnd_xl = HWND(lParam);

                  dword sel = SendMessage(hwnd_xl, LB_GETCURSEL, 0, 0);
                  sel = SendMessage(hwnd_xl, LB_GETITEMDATA, sel, 0);

                  INode *root = ip->GetRootNode();
                  dword n = root->GetXRefFileCount();
                  if(sel>=n)
                     break;

                  CreateXrefPoint();

                              //link to xref point
                  for(dword i=n; i--; )
                     root->SetXRefParent(i, NULL);

                  INode *xrf = root->GetXRefTree(sel);
                              //copy pos/rot
                  {
                     const Matrix3 tm = xrf->GetNodeTM(0);
                     S_vector p(tm[3][0], tm[3][1], tm[3][2]);
                     Control *control = xref_node->GetTMController();
                     MakeControlsTCB(control);
                     control->GetPositionController()->SetValue(0, (void*)&p);

                     AngAxis aa;
                     AffineParts parts;
                     decomp_affine(tm, &parts);
                     control->GetRotationController()->SetValue(0, &parts.q);
                  }

                  root->SetXRefParent(sel, xref_node);

                  ip->RedrawViews(ip->GetTime());
               }
               break;

            case IDC_REMOVE_UVW:
            case IDC_SELECT_DOUBLE_FACES:
            case IDC_WELD_VERTS:
            case IDC_SNAP_VERTS:
               {
                  int cn = ip->GetSelNodeCount();
                  int map_channel = GetDlgItemInt(hwnd, IDC_EDIT_UV, NULL, false);
                  float weld_thresh = 0;
                  float snap_thresh = 0;

                  switch(id){
                  case IDC_WELD_VERTS:
                     {
                        char buf[256];
                        GetDlgItemText(hwnd, IDC_EDIT_WELD_DIST, buf, sizeof(buf));
                        sscanf(buf, "%f", &weld_thresh);
                     }
                     break;
                  case IDC_SNAP_VERTS:
                     {
                        if(MessageBox(hwnd, "This will snap vertices of all selected objects to grid with specified spacing. No UNDO!\nNote: this doesn't account for object offset/rotation/scale, and works in object's local space.", "Snap vertices to grid", MB_YESNO) != IDYES)
                           return 0;

                        char buf[256];
                        GetDlgItemText(hwnd, IDC_EDIT_SNAP_DIST, buf, sizeof(buf));
                        sscanf(buf, "%f", &snap_thresh);
                        if(!snap_thresh)
                           return 0;
                     }
                     break;
                  }

                  C_str msg;
                  std::vector<INode*> new_sel;

                  for(int x=0; x<cn; x++){
                     INode *node = ip->GetSelNode(x);
                     /*
                                 //then osm stack
                     Object *obj = node->GetObjectRef();
                     /*
                                 //check to make sure no modifiers on the object
                     int ct = 0;
                     SClass_ID sc = obj->SuperClassID();
                     IDerivedObject *dobj;
                     if(sc == GEN_DERIVOB_CLASS_ID){
                        dobj = (IDerivedObject*)obj;

                        while(sc == GEN_DERIVOB_CLASS_ID){
                           ct +=  dobj->NumModifiers();
                           dobj = (IDerivedObject*)dobj->GetObjRef();
                           sc = dobj->SuperClassID();
                        }

                     }
                     dobj = node->GetWSMDerivedObject();
                     if(dobj)
                        ct += dobj->NumModifiers();

                     bool ok = false;
                     if(ct == 0)
                        */
                     {
                        ObjectState os = node->EvalWorldState(ip->GetTime());
                        if(os.obj && os.obj->SuperClassID() == GEOMOBJECT_CLASS_ID && os.obj->IsSubClassOf(triObjectClassID)){
                           Object *bobj = os.obj->FindBaseObject();
                           if(bobj->ClassID() ==Class_ID(EDITTRIOBJ_CLASS_ID,0)){
                              TriObject *T1 = (TriObject*)os.obj;
                              Mesh &m = T1->GetMesh();

                              switch(id){
                              case IDC_REMOVE_UVW:
                                 m.setNumMapVerts(map_channel, 0);
                                 m.setNumMapFaces(map_channel, 0);
                                 m.setMapSupport(map_channel, false);
                                 break;

                              case IDC_SELECT_DOUBLE_FACES:
                                 {
                                    dword num_faces = m.getNumFaces();
                                    BitArray &fsel = m.FaceSel();
                                    const Face *faces = m.faces;

                                    assert(fsel.GetSize()==(int)num_faces);
                                    fsel.ClearAll();
                                    dword num_dup = 0;
                                                //traverse all faces
                                    for(dword i=num_faces; i--; ){
                                       const Face &fc = faces[i];
                                                //compare with all other faces
                                       for(dword j=i; j--; ){
                                          const Face &fc1 = faces[j];
                                          if(FacesEqual(fc, fc1)){
                                                //mark the one with higher index
                                             fsel.Set(i);
                                             ++num_dup;
                                             break;
                                          }
                                       }
                                    }
                                    if(num_dup){
                                       new_sel.push_back(node);
                                       if(!msg.Size())
                                          msg = "Objects with duplicated faces:\n\n";
                                       msg += C_str(C_xstr("'%': % face(s)\n") %node->GetName() %num_dup);
                                    }
                                 }
                                 break;

                              case IDC_WELD_VERTS:
                                 if(WeldMeshVertices(m, msg, node->GetName(), weld_thresh))
                                    new_sel.push_back(node);
                                 break;

                              case IDC_SNAP_VERTS:
                                 SnapMeshVertices(m, snap_thresh);
                                 new_sel.push_back(node);
                                 break;

                              default: assert(0);
                              }
                              //ok = true;
                           }
                        }
                     }
                     //if(!ok)
                        //MessageBox(hwnd, "This tool works only on collapsed editable meshes.", "Error", MB_ICONEXCLAMATION);
                  }
                  switch(id){
                  case IDC_SELECT_DOUBLE_FACES:
                     if(msg.Size())
                        msg += "\nThe duplicated faces have been selected. Go to faces sub-selection mode to delete them.";
                     else
                        msg = "No duplicated faces found on current selection.";
                     break;
                  }
                  if(msg.Size())
                     MessageBox(hwnd, msg, "I3D Tools", MB_ICONINFORMATION);

                  ip->ClearNodeSelection();
                  for(dword i=new_sel.size(); i--; )
                     ip->SelectNode(new_sel[i], false);

                  ip->ForceCompleteRedraw();
               }
               break;
            }
         }
         break;
      }
      return FALSE;
   }

   static BOOL CALLBACK dlgProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){

      return i3d_tools.dlgProc(hwnd, msg, wParam, lParam);
   }

public:
   IUtil *iu;
   Interface *ip;
   HWND hwnd_panel;
   static C_I3D_tools i3d_tools;

   C_I3D_tools():
      iu(NULL),
      ip(NULL),
      xref_node(NULL),
      hwnd_panel(NULL)
   {
   }

//----------------------------

   void BeginEditParams(Interface *ip, IUtil *iu){

      this->iu = iu;
      this->ip = ip;
      hwnd_panel = ip->AddRollupPage(hInstance,
         MAKEINTRESOURCE(IDD_I3D_TOOLS),
         dlgProcThunk,
         "Parameters",
         0);
   }

//----------------------------

   void EndEditParams(Interface *ip,IUtil *iu){

      this->iu = NULL;
      this->ip = NULL;
      ip->DeleteRollupPage(hwnd_panel);
      hwnd_panel = NULL;
   }

//----------------------------

   void DeleteThis(){}
   void Init(HWND hwnd){}
   void Destroy(HWND hwnd){}

//----------------------------

   void ShowSelectionText(HWND hwnd){

      SetWindowText(GetDlgItem(hwnd, IDC_SEL), C_xstr("Objects selected: %") %ip->GetSelNodeCount());
   }

//----------------------------

   void SelectionSetChanged(Interface *ip, IUtil *iu){

      ShowSelectionText(hwnd_panel);

      if(xref_node){
         xref_node->Delete(0, true);
         xref_node = NULL;
         SendDlgItemMessage(hwnd_panel, IDC_LIST_XREFS, LB_SETCURSEL, -1, 0);
      }
   }
};

//----------------------------

C_I3D_tools C_I3D_tools::i3d_tools;

static class C_ClassDesc: public ClassDesc{
public:
   int IsPublic() {return 1;}
   void *Create(BOOL loading = FALSE){ return &C_I3D_tools::i3d_tools; }
   const TCHAR *ClassName(){ return "I3D Tools"; }
   SClass_ID SuperClassID() {return UTILITY_CLASS_ID;}
   Class_ID ClassID(){ return Class_ID(0x61f2594, 0x65953823); }
   const TCHAR *Category() {return _T("");}
} i3d_tools_desc;

ClassDesc *GetPluginDesc(){ return &i3d_tools_desc; }


//----------------------------


   
