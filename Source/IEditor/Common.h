#include "resource.h"

//----------------------------

#pragma warning(disable:4238)// nonstandard extension used : class rvalue used as lvalue


//----------------------------

C_str FloatStrip(const C_str &str);
C_str GetDlgItemText(HWND hwnd, int dlg_id);

//----------------------------

#if defined _MSC_VER && 1

inline int FloatToInt(float f){
   __asm{
      fld f
      fistp f 
   }
   return *(int*)&f;
}
#else
inline int FloatToInt(float f){
   return (int)f;
}
#endif

//----------------------------
// Check if name is already used on some frame in scene.
bool IsNameInScene(PI3D_scene scene, const C_str &str1);

PI3D_visual CastVisual(PC_editor ed, PI3D_visual vis, dword visual_type,
   PC_editor_item_Modify, PC_editor_item_Selection);
HINSTANCE GetHInstance();
void InitDlg(PIGraph igraph, void *hwnd);

//----------------------------
// Create unique frame name. 'buf' contain base of name, if it contains digits at the end,
// those are stripped and replaced by new digits.
void MakeSceneName(PI3D_scene scene, char *buf, bool suffix_hint = true);

//----------------------------
bool IsProgrammersPC(PC_editor ed);

//----------------------------
// Setup all children volumes of frame to be static or dynamic.
void SetupVolumesStatic(PI3D_frame, bool);

//----------------------------
// Setup all children volumes of frm to have owner frm_owner.
void SetupVolumesOwner(PI3D_frame frm, PI3D_frame frm_owner);

//----------------------------
// Let user select name.
// If 'hwnd_parent' is NULL, HWND of IGraph is used.
bool SelectName(PIGraph, HWND hwnd_parent, const char *title, C_str &name, const char *desc = NULL);

//----------------------------

struct S_load_err{              
   PC_editor_item_Log e_log;
   bool first;
   const char *msg;
   S_load_err(PC_editor_item_Log ei, const char *cp):
      e_log(ei), msg(cp), first(true)
   {}
   static void OpenErrFnc(const char *cp, void *context){
      S_load_err *ep = (S_load_err*)context;
      if(ep->first){
         ep->first = false;
         ep->e_log->AddText(C_fstr("*** %s load errors ***", ep->msg));
      }
      ep->e_log->AddText(cp);
   }
};

#define OPEN_ERR_LOG(ed, name) S_load_err::OpenErrFnc, (void*)&S_load_err((PC_editor_item_Log)ed->FindPlugin("Log"), name)

//----------------------------

#define DEBUGLOG(msg) ed->FindPlugin("Log")->Action(E_LOG_ADD_TEXT, (void*)msg)

//----------------------------
// Compute contour from provided bounding box, as seen from specified point.
// Parameters:
//    bb ... bounding box in local coords
//    tm ... matrix used to transform bounding box
//    look ... point from which we look at the box
//    contour_points ... points of countour points, in CW order
//    num_cpts ... number of countour points
//    orthogonal ... true if orthogonal projection is used
// Return values:
//    contour_points and num_cpts contain valid contour when the function returns,
//    if look point is inside of box, num_cpts is set to zero
void ComputeContour(const I3D_bbox &bb, const S_matrix &tm, const S_vector &look,
   S_vector contour_points[6], dword &num_cpts, bool orthogonal);

//----------------------------
// Check if contour intersects with frustum. The contour doesn't need to be planar, but it must be viewed
// from one of corner points of the frustum.
// Parameters:
//    planes, num_planes ... view frustum
//    hull_pts ... points on sides of the frustum (but not cam_pos)
//    verts, num_verts ... vertices of contour
//    cam_pos ... position of camera
bool CheckFrustumIntersection(const S_plane *planes, dword num_planes, const S_vector *hull_pts,
   const S_vector *verts, dword num_verts, const S_vector &cam_pos);

//----------------------------
// Find specified 32-bit pointer in array of pointers,
// return index of pointer, or -1 if pointer not found.
inline int FindPointerInArray(void **vp, int array_len, void *what){
   for(int i(array_len); i--; ) if(vp[i]==what) break;
   return i;
}

//----------------------------
// Make map of unique vertices - point duplicated vertices at back to their matching ones at front of buffer.
void MakeVertexMapping(const S_vector *vp, dword pitch, int numv, word *v_map, float thresh = .001f);

//----------------------------
// Find specified edge (given by 2 indicies) in list, return index,
// or -1 if not found.
inline int FindEdge(const I3D_edge *edges, dword num_edges, const I3D_edge &e){
   return FindPointerInArray((void**)edges, num_edges, *(void**)&e);
}

//----------------------------
// Remove specified edge from list, if it is found, the return value is true.
inline bool RemoveEdge(C_vector<I3D_edge> &edges, const I3D_edge &e){
   int i = FindEdge(&edges.front(), edges.size(), e);
   if(i==-1) return false;
   edges[i] = edges.back(); edges.pop_back();
   return true;
}

//----------------------------
// Add edge into list, or split with inversed edge (remove those from list),
// return value is true if edge is added, or false if it is split.
inline bool AddOrSplitEdge(C_vector<I3D_edge> &edge_list, const I3D_edge &e){
   if(RemoveEdge(edge_list, I3D_edge(e[1], e[0])))
      return false;
   edge_list.push_back(e);
   return true;
}

//----------------------------
                              //find specified pointer in array of 32-bit pointers (asm optimized)
#ifdef _MSC_VER

inline int FindPointerIndex(void **vp, int array_len, void *what){
   int rtn;
   __asm{
      push ecx
      mov edi, vp
      mov edx, array_len
      mov eax, what
      mov ecx, edx
      repne scasd
      jz ok
      mov edx, ecx
   ok:
      sub edx, ecx
      dec edx
      mov rtn, edx
      pop ecx
   }
   return rtn;
}
   
#else

inline int FindPointerIndex(void **vp, int array_len, void *what){
   for(int i(array_len); i--; ) if(vp[i]==what) break;
   return i;
}

#endif

//----------------------------

inline dword ConvertColor(const S_vector &v, byte alpha){
   return (alpha<<24) | (int(v.x*255.0f)<<16) | (int(v.y*255.0f)<<8) | (int(v.z*255.0f)<<0);
}

inline S_vectorw ConvertColor(dword c){
   return S_vectorw(((c>>16)&255)/255.0f, ((c>>8)&255)/255.0f, ((c>>0)&255)/255.0f, ((c>>24)&255)/255.0f);
}

//----------------------------
// Get size of file, or -1 if error occured.
inline dword GetFileSize(const char *fname){

   PC_dta_stream dta = DtaCreateStream(fname);
   if(!dta)
      return (dword)-1;
   dword size = dta->GetSize();
   dta->Release();
   return size;
}

//----------------------------
// Try to traverse hierarchy and select topmost parent model frame of given frame.
// If 'frm' is not child of model, the returned value is 'frm'.
// When 'return_topmost' is true, top model parent is returned, otherwise 1st model parent is returned.
PI3D_frame TrySelectTopModel(PI3D_frame frm, bool return_topmost);

//----------------------------
// Let user visually select one item from a list of items. The returned value is
// index of item, or -1 if selection is cancelled.
// The input parameter points to list of strings to choose from, terminated by '\0'.
// If 'hwnd_parent' is NULL, igraph's HWND is used.
int ChooseItemFromList(PIGraph, HWND hwnd_parent, const char *title, const char *item_list, int curr_sel = -1);

//----------------------------

int GetPropertySize(I3D_PROPERTYTYPE ptype, dword value);

//----------------------------

bool GetBrowsePath(PC_editor ed, const char *title, C_str &filename, const char *init_dir, const char *extensions);

//----------------------------

float GetModelBrightness(CPI3D_model mod);
void SetModelBrightness(PI3D_model mod, float b);

//----------------------------
                              //window controls' help data
struct S_ctrl_help_info{
   word ctrl_id;              //control ID (0 is terminator)
   const char *text;          //help text
};

//----------------------------
// Display context help associated with specified window.
void DisplayHelp(HWND hwnd, dword ctrl_id, const char *txt);
void DisplayHelp(HWND hwnd, dword ctrl_id, const S_ctrl_help_info *hi);

//----------------------------
