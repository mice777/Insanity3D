#ifndef __GRASSTEST_H_
#define __GRASSTEST_H_

#include <c_unknwn.hpp>

//----------------------------
                              //class for adding details to terrain
                              // (placing models onto terrain level)
class C_terrain_detail: public C_unknown{
public:
//----------------------------
// Update the class.
   virtual void Update(int time, PI3D_camera camera) = 0;
};

//----------------------------
struct S_terrdetail_init{
   struct S_model_set{
      struct S_model_source{
         C_str filename;
         float use_ratio;
         bool normal_align;
         S_model_source(): use_ratio(1.0f), normal_align(false){}
         S_model_source(const C_str &n, float r, bool al): filename(n), use_ratio(r), normal_align(al){}
         /*
         S_model_source(const S_model_source &s){ operator =(s); }
         void operator =(const S_model_source &s){
            filename = s.filename;
            use_ratio = s.use_ratio;
            normal_align = s.normal_align;
         }
         */
      };
                              //models loaded in this set (names plus ratio)
      C_vector<S_model_source> model_sources;
                              //materials on which this set is applied (wildmasked names)
      C_vector<C_str> material_dests;
      float density;          //how many models per 1 m^2
      float scale_thresh;     //how much we may scale down/up
      //bool normal_align;

      S_model_set(): density(1.0f), scale_thresh(0.0f){}
      /*
      S_model_set(const S_model_set &s){ operator =(s); }
      void operator =(const S_model_set &s){
         model_sources = s.model_sources;
         material_dests = s.material_dests;
         density = s.density;
         scale_thresh = s.scale_thresh;
         //normal_align = s.normal_align;
      }
      */
   };

   C_vector<S_model_set> model_sets;
   C_str terrain_name_base;
   float visible_distance;
   long count_down; // if != 0 models will change dynamicaly their postion after count_down sec

   S_terrdetail_init():count_down(0){}
private:
   S_terrdetail_init(const S_terrdetail_init&);
   void operator =(const S_terrdetail_init&);
};

typedef C_terrain_detail* PC_terrain_detail;

//----------------------------
// Create the class - working on specified scene.
// Parameters:
//    model_sets ... multiple sets with model names used in each set
//    terrain_names ... wildmasked names of terrain
C_terrain_detail *CreateTerrainDetail(PI3D_scene, const S_terrdetail_init &init_data);

//----------------------------

#endif
