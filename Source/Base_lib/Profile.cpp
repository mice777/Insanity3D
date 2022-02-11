#include <windows.h>
#include <profile.h>

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
                              //block profiling

                              //block: 0=inner, 1=outer

#define NUM_BLOCK_SAMPLES 16  //must be 2^n
static float block_sum[2][NUM_BLOCK_SAMPLES];
static dword block_indx;
static bool in_prof;

//----------------------------
                              //simple profiling

static LARGE_INTEGER li;
static dword prof_cost;
static float prof_freq;

void BegProf(){ QueryPerformanceCounter(&li); }

//----------------------------

float EndProf(){

   LARGE_INTEGER li1;
   QueryPerformanceCounter(&li1);
   int delta = li1.LowPart - li.LowPart - prof_cost;
   float val = (float)Max(0, delta);
   li = li1;
   val = (val / prof_freq * 1000.0f);
   return val;
}

//----------------------------

static float EndProfSim(){

   LARGE_INTEGER li1;
   QueryPerformanceCounter(&li1);
   int delta = li1.LowPart - li.LowPart - prof_cost;
   float val = (float)Max(0, delta);
   li = li1;
   val = (val / prof_freq * 1000.0f);
   return (float)delta;
}

//----------------------------

static class C_profiler_auto_init{
public:

   static void BegProfBlockSim(){
      if(!in_prof){
         BegProf();
         in_prof = true;
      }
   }

   static dword EndProfBlockSim(){

      if(!in_prof)
         return 0;
      dword ret = FloatToInt(EndProfSim());
      in_prof = false;
      return ret;
   }

   C_profiler_auto_init(){
                              //determine frequency
      LARGE_INTEGER li;
      QueryPerformanceFrequency(&li);
      prof_freq = li.LowPart + 65536.0f * li.HighPart;
                              //determine profiler overhead
      BegProf(); EndProf();
                              //simulate calls to BegProfBlock and EndProfBlock
      BegProfBlockSim();
      prof_cost = (dword)EndProfBlockSim();
      ++prof_cost;
   }
} profiler_auto_calibrate;

//----------------------------
//----------------------------

void BegProfBlock(){

   if(!in_prof){
      BegProf();
      in_prof = true;
   }
}

//----------------------------

void EndProfBlock(){

   if(in_prof){
      block_sum[0][block_indx] += EndProf();
      in_prof = false;
   }
}

//----------------------------

static bool init2;
static bool init3;

float GetProfBlock(){

   if(in_prof) EndProfBlock();

   if(!init2){
      block_sum[0][block_indx] = 0.0f;
      block_sum[1][block_indx] = 0.0f;
      init2 = true;
      return 0.0f;
   }

   float f[2];
   ++block_indx;
   block_indx &= NUM_BLOCK_SAMPLES-1;
   //for(int j=0; j<2; j++){
   int j=0;
   {
      f[j] = 0.0;
      for(int i=NUM_BLOCK_SAMPLES; i--; )
         f[j] += block_sum[j][i];
      block_sum[j][block_indx] = 0.0f;
   }
   if(!init3){
      if(block_indx)
         return 0.0f;
      init3 = true;
   }
   return f[0] / NUM_BLOCK_SAMPLES;
}

//----------------------------
//----------------------------

