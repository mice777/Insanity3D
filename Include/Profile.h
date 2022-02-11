#include <rules.h>
#ifndef __PROFILE_H
#define __PROFILE_H

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// Profiler functions.
//----------------------------

                              //simple profiling
                              // returns time between calling
                              // BegProf and EndProf
void BegProf();
float EndProf();

                              //block profiling
void BegProfBlock();          //begin profiling
void EndProfBlock();          //stop profiling
float GetProfBlock();         //get averaged sum of profile time

#define PR_BEG BegProfBlock()
#define PR_END EndProfBlock()
#define PR_GET GetProfBlock()

//----------------------------
// Block profiler class - main interface, responsible for collecting all blocks,
// building statistics and rendering.
class C_block_profiler{
   friend class C_block_profiler_entry;
//----------------------------
// This method is called exclusively by C_block_profiler_entry to store collected data.
   virtual void Begin(dword blk_index) = 0;
   virtual void End(dword blk_index, const __int64 &t) = 0;
public:
                              //profiling mode
   enum E_MODE{
      MODE_NO,                //no profiling
      MODE_ABSOLUTE,          //absolute time computed in each block
      MODE_SELF,              //self time computed in each block
   };

   virtual ~C_block_profiler(){}

//----------------------------
// Set operational mode - one of E_MODE values.
   virtual void SetMode(E_MODE) = 0;
   virtual E_MODE GetMode() const = 0;

//----------------------------
// Clear all profiling data.
   virtual void Clear() = 0;

//----------------------------
// Prepare values for rendering, and clear values.
   virtual void PrepareForRender() = 0;

//----------------------------
// Render currently prepared values.
   virtual void Render(class I3D_scene*) = 0;
};

//----------------------------
// Create block profiler class.
// The number of blocks is determined from 'block_names' - it provides names of blocks separated by \0 characters.
C_block_profiler *CreateBlockProfiler(class IGraph *igraph, const class C_poly_text *font,
   const char *profiler_name, const char *block_names);

//----------------------------
// Single block-profiler entry - class which counts time between its creation (in constructor)
// and its destruction (in destructor). It calls block profiler to strore collected data.

class C_block_profiler_entry{
   __int64 prof_beg_time;
   dword blk_index;
   C_block_profiler *prof;
public:
   C_block_profiler_entry(dword bi, C_block_profiler *bp):
      blk_index(bi),
      prof(bp)
   {
      if(prof){
                              //get beginning time
         __asm{
            rdtsc
            mov ebx, this
            lea ebx, [ebx].prof_beg_time
            mov [ebx+0], eax
            mov [ebx+4], edx
         }
         prof->Begin(blk_index);
      }
   }
   ~C_block_profiler_entry(){
      if(prof){
                              //get time delta
         __int64 t;
         __asm{
            rdtsc
            mov ebx, this
            lea ebx, [ebx].prof_beg_time
            sub eax, [ebx+0]
            sbb edx, [ebx+4]
            mov dword ptr t, eax
            mov dword ptr t+4, edx
         }
         prof->End(blk_index, t);
      }
   }
};


//----------------------------

#endif //__PROFILE_H