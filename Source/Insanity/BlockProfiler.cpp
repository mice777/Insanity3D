#include "pch.h"
#include <profile.h>
#include <malloc.h>

const dword NUM_SAMPLES = 8;
const float PROF_BASE_Y = .05f;

//----------------------------

static const word indx[] = {
   0, 1, 2,
   2, 1, 3
};

//----------------------------
//----------------------------

class C_block_profiler_imp: public C_block_profiler{

   E_MODE mode;

   __int64 beg_time;          //beginning time retrieved by rdtsc
   dword beg_msec;            //beginning time in msec

   float frame_average[NUM_SAMPLES];
   dword avg_count;
   C_str profiler_name;

   struct S_bar{
      S_vector2 pos, size;
      dword color;

      void Render(PI3D_scene scn, PIGraph ig) const{

         float sx = ig->Scrn_sx();
         //float sy = ig->Scrn_sy();
         S_vectorw verts[4];
         for(dword i=4; i--; ){
            S_vectorw &v = verts[i];
            v.x = pos.x * sx;
            v.y = pos.y * sx;
            if(i&1)
               v.x += size.x * sx;
            if(i&2)
               v.y += size.y * sx;
            v.z = 0.0f;
            v.w = 1.0f;
         }
         scn->DrawTriangles(verts, 4, I3DVC_XYZRHW, indx, 6, color);
      }
   };

   struct S_block{
      C_str name;
      dword num_calls;
      __int64 time;
      float averages[NUM_SAMPLES];
      dword calls_average[NUM_SAMPLES];
      dword index;            //sorted index
      bool was_shown;
      S_bar bar;
      C_smart_ptr<C_text> txt;
   };
   S_block *blocks;
   dword num_blocks;

   int sort_countdown;        //counter for re-sorting the blocks

   friend class C_prof;

   C_vector<dword> in_block;

   C_smart_ptr<IGraph> igraph;
   C_smart_ptr<C_poly_text> texts;
   C_smart_ptr<C_text> title;
   S_bar bar_title;

//----------------------------

   virtual void Begin(dword blk_index){

      assert(blk_index < num_blocks-1);
      in_block.push_back(blk_index);
      ++blocks[blk_index].num_calls;
   }

//----------------------------

   virtual void End(dword blk_index, const __int64 &t){

      assert(in_block.size() && in_block.back()==blk_index);
      in_block.pop_back();
                              //store to the counter
      blocks[blk_index].time += t;
                              //if self-time counted, subtract from parent block
      if(in_block.size()){
         if(mode==MODE_SELF || in_block.back()==blk_index)
            blocks[in_block.back()].time -= t;
      }else
         blocks[num_blocks-1].time += t;
   }

public:
   C_block_profiler_imp():
      mode(MODE_NO),
      avg_count(0),
      sort_countdown(0),
      num_blocks(0),
      blocks(NULL)
   {
      memset(frame_average, 0, sizeof(frame_average));
   }

//----------------------------

   virtual ~C_block_profiler_imp(){

      delete[] blocks;
      title = NULL;
      texts = NULL;
   }

//----------------------------

   bool Init(IGraph *ig, const C_poly_text *font, const char *_pn, const char *block_names){

      igraph = ig;
      profiler_name = _pn;
                              //count number of blocks
      {
         const char *cp = block_names;
         for(num_blocks=0; *cp; num_blocks++)
            cp += strlen(cp) + 1;
      }

                              //note: last block is internal, for counting out-of-blocks time
      ++num_blocks;
      blocks = new S_block[num_blocks];
      memset(blocks, 0, num_blocks*sizeof(S_block));

      for(dword i=0; i<num_blocks; i++){
         S_block &bl = blocks[i];
         if(i < num_blocks-1){
            bl.name = block_names;
            block_names += strlen(block_names) + 1;
         }else
            bl.name = "* rest";
         bl.index = i;
         bl.was_shown = false;

         bl.bar.pos.Zero();
         bl.bar.size.Zero();
         bl.bar.pos.x = .2f;
         bl.bar.size.y = .012f;
         bl.bar.color = 0;
      }
      bar_title.pos.x = .2f;
      bar_title.pos.y = PROF_BASE_Y+.01f - .022f;
      bar_title.size.x = 0;
      bar_title.size.y = .012f;
      bar_title.color = 0xffffffff;

      PC_poly_text tp;
      CreatePolyText(font, &tp);
      texts = tp;
      tp->Release();

      return true;
   }

//----------------------------

   virtual void SetMode(E_MODE m){ mode = m; }
   virtual E_MODE GetMode() const{ return mode; }

//----------------------------

   virtual void Clear();

//----------------------------

   virtual void PrepareForRender();

//----------------------------

   virtual void Render(PI3D_scene);

};

//----------------------------

void C_block_profiler_imp::Clear(){

   if(!igraph) return;
   for(dword i=num_blocks; i--; ){
      S_block &bl = blocks[i];
      bl.time = 0;
      bl.num_calls = 0;
   }
   in_block.clear();
   in_block.reserve(16);
                           //get beginning time
   beg_msec = igraph->ReadTimer();

   __int64 &bgt = beg_time;
   __asm{
      rdtsc
      mov ebx, this
      //lea ebx, [ebx].beg_time
      mov ebx, bgt
      mov [ebx+0], eax
      mov [ebx+4], edx
   }
}

//----------------------------

void C_block_profiler_imp::PrepareForRender(){

   if(mode!=MODE_NO){
                              //get rdtsc delta
      __int64 rdtsc_frame;
      __asm{
         rdtsc
         mov ebx, this
         lea ebx, [ebx].beg_time
         sub eax, [ebx+0]
         sbb edx, [ebx+4]
         mov dword ptr rdtsc_frame, eax
         mov dword ptr rdtsc_frame+4, edx
      }
      dword msec = igraph->ReadTimer() - beg_msec;
                              //get all non-counted time
      blocks[num_blocks-1].time = rdtsc_frame - blocks[num_blocks-1].time;

      frame_average[avg_count] = (float)msec;
      float f_msec = 0.0f;
      for(dword si=NUM_SAMPLES; si--; )
         f_msec += frame_average[si];
      f_msec = f_msec / NUM_SAMPLES;

      S_text_create ct;
      ct.x = .01f;
      ct.size = .022f;
                              //display values converted to time
      C_fstr txt("%s: %.2f ms", (const char*)profiler_name, f_msec);
      ct.y = PROF_BASE_Y;
      ct.tp = (const char*)txt;
      if(texts){
         title = texts->CreateText(ct);
         title->Release();
      }

      const float r_max_time = 1.0f / 100.0f;

      float scale = Min(1.0f, f_msec * r_max_time);
      bar_title.size.x = scale*.79f;

      float *totals = new(alloca(num_blocks*sizeof(float))) float[num_blocks];
      float max_time = 0.0f;
      for(dword i=0; i<num_blocks; i++){
         S_block &bl = blocks[i];
         bl.txt = NULL;
         //if(!bl.num_calls && i!=PROF_internal) continue;
         float f_time = bl.time * (__int64)msec / rdtsc_frame;
         bl.averages[avg_count] = f_time;
         bl.calls_average[avg_count] = bl.num_calls;

         float f_total = 0;
         dword calls = 0;
         for(dword si=NUM_SAMPLES; si--; ){
            f_total += bl.averages[si];
            calls += bl.calls_average[si];
         }
         f_total = f_total / NUM_SAMPLES;
         totals[i] = f_total;
         float THRESH = .01f;
         if(!bl.was_shown)
            THRESH *= 20.0f;
         if(f_total <= THRESH){
            bl.was_shown = false;
            continue;
         }
         bl.was_shown = true;

         calls = (calls+NUM_SAMPLES/2) / NUM_SAMPLES;
         {
            C_fstr txt("%s: %.2f ms", (const char*)bl.name, f_total);
            if(calls)
               txt += C_fstr(" (%i)", calls);
            ct.y = PROF_BASE_Y + (float)(bl.index+1) * .022f;
            ct.tp = (const char*)txt;
            bl.txt = texts->CreateText(ct);
            bl.txt->Release();
         }
         {
            float scale = Min(1.0f, f_total * r_max_time);
            bl.bar.pos.y = PROF_BASE_Y+.01f + (float)bl.index*.022f;
            bl.bar.size.x = scale*.79f;
         }
         max_time = Max(max_time, f_total);
      }

                              //re-color bars
      for(i=0; i<num_blocks; i++){
         if(!blocks[i].txt)
            continue;
         float scale = totals[i] / max_time;
         int r, g, b;
         b = 0;
         r = FloatToInt(255.0f * scale);
         g = 255 - r;
         blocks[i].bar.color =0x80000000 | (r<<16) | (g<<8) | b;
      }

      if(++avg_count == NUM_SAMPLES)
         avg_count = 0;
   }
   Clear();
}

//----------------------------

void C_block_profiler_imp::Render(PI3D_scene scene){

   if(!mode)
      return;
   PI3D_driver drv = scene->GetDriver();
   bool was_zb = drv->GetState(RS_USEZB);
   if(was_zb) drv->SetState(RS_USEZB, false);

                              //render all bars first
   drv->SetTexture(NULL);
   for(dword i=0; i<num_blocks; i++){
      const S_block &bl = blocks[i];
      if(bl.txt)
         bl.bar.Render(scene, igraph);
   }
   bar_title.Render(scene, igraph);

                              //render texts above bars
   if(texts)
      texts->Render(scene);

   if(was_zb) drv->SetState(RS_USEZB, true);
}

//----------------------------
//----------------------------

C_block_profiler *CreateBlockProfiler(IGraph *igraph, const C_poly_text *font,
   const char *profiler_name, const char *block_names){

   C_block_profiler_imp *bp = new C_block_profiler_imp;
   if(!bp->Init(igraph, font, profiler_name, block_names)){
      delete bp;
      bp = NULL;
   }
   return bp;
}

//----------------------------
//----------------------------
