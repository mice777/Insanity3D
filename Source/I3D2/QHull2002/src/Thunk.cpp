//#include "..\all.h"
#include <stdio.h>
extern"C"{
#include "mem.h"
#include "qhull.h"
#include "qset.h"
#include "poly.h"
}
#include "qhull_a.h"
#define NO_STARTUP
#define I3D_MATH_NO_LIB_REF
#define I3D_FULL
#include <i3d\i3d2.h>
//#include "..\common.h"
#include <vector>
#include <C_vector.h>
#include <assert.h>

#ifdef _MSC_VER
using namespace std;
#endif

/*
   C++ Thunk into QHull library.
   Created by: Michal Bacik
   Date: 27. 3. 2000
*/
#pragma warning(disable:4706)

//----------------------------

char qh_version[] = "QHull thunk 00/3/27";   //used for error messages

//----------------------------

struct I3D_face{
   dword num_points;
   word *points;
   I3D_face(): num_points(0), points(NULL){}
   I3D_face(dword num): num_points(num), points(num ? new word[num] : NULL){}
   I3D_face(const I3D_face &f): points(NULL){ operator =(f); }
   ~I3D_face(){ delete[] points; }
   I3D_face &operator =(const I3D_face &f){
      delete[] points;
      points = new word[num_points = f.num_points];
      memcpy(points, f.points, num_points*sizeof(word));
      return *this;
   }
   void Reserve(dword num){
      if(num_points!=num){
         delete[] points;
         points = new word[num_points = num];
      }
   }
   inline word &operator [](int i){ return points[i]; }
   inline const word &operator [](int i) const{ return points[i]; }
};

//----------------------------

bool qhCreateConvexHull(const C_vector<S_vector> &verts, C_vector<I3D_triface> *tri_faces,
   C_vector<I3D_face> *faces, C_vector<S_plane> *planes){

   int i;

   //_controlfp(_MCW_PC, _PC_64);
                              //make convex hull
   int exitcode = 
      qh_new_qhull(3,            //dimension (3D)
         verts.size(),
         (coordT*)&verts.front(),
         false,                  //ismalloc
         "qhull "
         "Pp "
         "QJ0.001 "
         //"Qx "
         //"A-0.99 "
         //"Qv "
         //"C-0 "
         "C-0.001 "
         //"E0.0001 "
         ,
         NULL,
         stderr);

   if(!exitcode){
      if(tri_faces)
         tri_faces->clear();
      if(faces)
         faces->clear();
                              //store faces
      facetT *facet;          //set by FORALLfacets

      FORALLfacets{
         setT *vertices;
         vertexT *vertex, **vertexp;

         vertices= qh_facet3vertex(facet);

         vector<word> vi;
         FOREACHvertex_(vertices){
            vi.push_back(qh_pointid(vertex->point));
         }

                              //add all tri-faces
         if(tri_faces){
            I3D_triface fc;
            fc[0] = vi[0];
                              //now construct faces
            for(i=2; i<(int)vi.size(); i++){
               fc[1] = vi[i-1];
               fc[2] = vi[i];
               tri_faces->push_back(fc);
            }
         }

                              //add all faces
         if(faces){
            faces->push_back(I3D_face(vi.size()));
            I3D_face &fc = faces->back();
            memcpy(fc.points, &vi.front(), fc.num_points*sizeof(word));
         }

         if(planes){
            planes->push_back(S_plane(S_vector(facet->normal[0], facet->normal[1], facet->normal[2]), facet->offset));
         }

         qh_settempfree(&vertices);
      }
   }
   qh_freeqhull(qh_ALL);      //free long memory
   int curlong, totlong;	   //memory remaining after qh_memfreeshort
   qh_memfreeshort(&curlong, &totlong);    //free short memory and memory allocator
   assert(!curlong && !totlong);

   return (exitcode==0);
}

//----------------------------