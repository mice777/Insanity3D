#include "..\common\pch.h"
#include "common.h"
#include <sortlist.hpp>

//----------------------------

void CenterDialog(HWND hwnd){

   int cx = GetSystemMetrics(SM_CXSCREEN);
   int cy = GetSystemMetrics(SM_CYSCREEN);
                              
   RECT rc;
   GetWindowRect((HWND)hwnd, &rc);
   int x = rc.left + (cx - (rc.right  - rc.left)) / 2;
   int y = rc.top  + (cy - (rc.bottom - rc.top )) / 2;
   x = Max(0, x);
   y = Max(0, y);
   SetWindowPos((HWND)hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

//----------------------------

void MakeVertexMapping(const S_vector *vp, dword pitch, dword numv, dword *v_map, float thresh){

   if(!numv)
      return;
                              //sort vertices by one axis
   C_sort_list<int> sorted_vertes(numv);
   for(dword i=numv; i--; ){
      const S_vector &v = *(S_vector*)(((byte*)vp)+pitch*i);
      sorted_vertes.Add(i, int(v.x * 1000.0f));
   }
   sorted_vertes.Sort();
                              //go through all vertices in the array, search neighbours until thresh
   for(i=numv; i--; ){
                              //get vertex index
      int vi = sorted_vertes[i];
                              //already processed?
      if(vi==-1)
         continue;
                              //originally map it to itself
      v_map[vi] = vi;
      const S_vector &v = *(S_vector*)(((byte*)vp)+pitch*vi);
                              //check neighbour vertices down
      for(dword i1=i; i1--; ){
         int vi1 = sorted_vertes[i1];
                              //already processed?
         if(vi1==-1)
            continue;
         const S_vector &v1 = *(S_vector*)(((byte*)vp)+pitch*vi1);
                              //first check distance on the sortex axis, break as soon as it's greater than thresh
         float delta = I3DFabs(v1.x - v.x);
         if(delta >= thresh)
            break;
         delta += I3DFabs(v1.y - v.y) + I3DFabs(v1.z - v.z);
         if(delta < thresh){
            if(vi < vi1){
               sorted_vertes[(int)i1] = -1;
               v_map[vi1] = vi;
            }else{
               v_map[vi] = vi1;
            }
         }
      }
   }
                              //fix-up - go from bottom up, make always higher index point down
   for(i=0; i<numv; i++){
      dword vi = v_map[i];
      if(vi > i){
                              //point higher index to lower index
         v_map[vi] = i;
         v_map[i] = i;
      }else
      if(vi!=i){
                              //point to original vertex only
         while(v_map[vi] != vi)
            vi = v_map[vi];
         v_map[i] = vi;
      }
   }

#ifdef _DEBUG
   for(i=numv; i--; ){
      dword vi = v_map[i];
      assert(vi <= i);
      assert(v_map[vi] == vi);
   }
#endif
}

//----------------------------
