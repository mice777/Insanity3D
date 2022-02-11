#ifndef __FPS_COUNTER_H_
#define __FPS_COUNTER_H_

//----------------------------
// Copyright (c) Lonely Cat Games  All rights reserved.
// FPS counter class.
//----------------------------

class C_fps_counter{
   int *samples;              //collect n last samples
   int num_samples;           //number of samples
   int sample_pos;            //buffer index, looping around
   bool is_init;              //false while heating up
   int total_time;            //total time for average fps
   int frame_count;           //frame counter for average fps

   float curr_count;          //computed once per tick
public:
   C_fps_counter(int ns = 32):
      num_samples(ns),
      samples(new int[ns])
   {
      Reset();
      ResetAverage();
   }

   ~C_fps_counter(){
      delete[] samples;
   }

//----------------------------
// Update class with time since last frame, and return FPS count.
   void Tick(int t){
                              //average count
      ++frame_count;
      total_time += t;

      samples[sample_pos] = t;
      if(++sample_pos==num_samples){
         is_init = true;
         sample_pos = 0;
      }

      if(!is_init){
         curr_count = 0.0f;
      }else{
         int ret = 0;
         for(int i=0; i<num_samples; i++)
            ret += samples[i];
         if(ret<10)
            curr_count = 0.0f;
         else
            curr_count = (float)num_samples * 1000.0f / (float)ret;
      }
   }

//----------------------------
// Get fps counter value.
   inline float GetCount(){ return curr_count; }

//----------------------------
// Reset counter - start over from beginning.
   void Reset(){
      for(int i=num_samples; i--; )
         samples[i] = 0;
      sample_pos = 0;
      is_init = false;
      curr_count = 0.0f;
   }

//----------------------------
// Reset average FPS counter.
   inline void ResetAverage(){
      frame_count = 0;
      total_time = 1;           //avoid divide by zero
   }

//----------------------------
// Get average FPS since initializing, or calling ResetAverage.
   inline float GetAverageFPS() const{
      return (float)frame_count*1000.0f / (float)total_time;
   }

//----------------------------
// Get number of frames counted.
   inline int GetFrameCount() const{ return frame_count; }
};


//----------------------------
#endif

