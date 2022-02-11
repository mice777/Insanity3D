#include "all.h"
#include "common.h"
#include "scene.h"
#include "camera.h"
#include "visual.h"
#include <d3d9types.h>
#include <integer.h>

//----------------------------

D3DVERTEXELEMENT9 D3DVERTEXELEMENT9_END = D3DDECL_END();

//----------------------------

const dword next_tri_indx[3] = { 1, 2, 0 };
const dword prev_tri_indx[3] = { 2, 0, 1 };

//----------------------------

void S_preprocess_context::Sort(){

   int i = prim_list.size();
   prim_sort_list.Reserve(i);
   while(i--)
      prim_sort_list.Add(i, prim_list[i].sort_value);
   prim_sort_list.Sort();
}

//----------------------------

void S_preprocess_context::Reset(){

   opaque = 0;
   ckey_alpha = 0;
   alpha_zwrite = 0;
   alpha_nozwrite = 0;
   alpha_noz = 0;
   prim_list.clear();
   prim_sort_list.Clear();
#ifndef GL
   shadow_casters.clear();
   shadow_receivers.clear();
#endif
   mirror_data.clear();
   occ_list.clear();

   LOD_factor = scene->GetActiveCamera1()->GetFOV1() / (65.0f * PI / 180.0f);
   LOD_factor *= scene->GetDriver1()->lod_scale;
   force_alpha = I3DIntAsFloat(FLOAT_ONE_BITMASK);
}

//----------------------------

void S_preprocess_context::SortByDistance(){
   int i = prim_list.size();
   prim_sort_list.Reserve(i);
   while(i--) prim_sort_list.Add(i, -FloatToInt(prim_list[i].vis->GetPos().Magnitude()*65536.0f));
   prim_sort_list.Sort();
}

//----------------------------

bool CheckFrustumIntersection(const S_plane *planes, dword num_planes, const S_vector *hull_pts,
   const S_vector *verts, dword num_verts, const S_vector &view_pos, bool ortho_proj, bool inv_cull){

   bool penetrated = false;
   dword clip_union = (1<<num_planes) - 1;
   for(int p2i=num_verts, p2i_next=0; p2i--; p2i_next=p2i){
      const S_vector &v_p2 = verts[p2i];
      const S_vector dir_p2 = verts[p2i_next] - v_p2;

                              //flag specifying that v_p2 is inside
      bool v_p2_in = true;

      float last_enter = -1e+16f, first_leave = 1e+16f;
                              //check this edge against all planes of poly1
      dword plane_in_flags = 0;
      dword plane_mask = 1;
      for(int p1i=num_planes; p1i--; plane_mask<<=1){
         const S_plane &p1_edge_plane = planes[p1i];
         float d = v_p2.DistanceToPlane(p1_edge_plane);
                              //check if out of one plane
         if(d < 0.0f){
            plane_in_flags |= plane_mask;
         }else{
            v_p2_in = false;
         }
         if(!penetrated){
                              //get distance at which we penetrate it
            float f = p1_edge_plane.normal.Dot(dir_p2);
            if(!IsAbsMrgZero(f)){
               float r = -d / f;
               if(f < 0.0f){
                  last_enter = Max(last_enter, r);
               }else{
                  first_leave = Min(first_leave, r);
               }
            }else
            if(d >= 0.0f){
                              //missing the plane, can't hit poly
               last_enter = 1e+16f;
            }
         }
      }
      if(v_p2_in)
         return true;
      clip_union &= ~plane_in_flags;
      if(!penetrated){
         if(last_enter >= 0.0f && first_leave <= 1.0f){
            penetrated = (last_enter < first_leave);
            if(penetrated)
               return true;
         }
                              //now compute p2 edge plane, and detect if frustum is out of this
         S_plane pl;
         if(!ortho_proj){
                              //perspective projection, use viewer point
            pl.normal.GetNormal(view_pos, v_p2, v_p2+dir_p2);
#if defined _DEBUG && 0
            pl.normal.Normalize();
#endif
            pl.d = -pl.normal.Dot(view_pos);
         }else{
                              //orthogonal projection, use look direction
            pl.normal = dir_p2.Cross(view_pos);
            pl.d = -pl.normal.Dot(v_p2);
         }
         if(inv_cull)
            pl.Invert();
         for(int i=num_planes; i--; ){
            float d = hull_pts[i].DistanceToPlane(pl);
            if(d < 0.0f)
               break;
         }
         if(i==-1)
            return false;
      }
   }
                              //if all verts are out of single plane, intersection = false
   return (!clip_union);
}

//----------------------------

dword CountBits(const S_pixelformat &fmt){

   if(fmt.flags&PIXELFORMAT_PALETTE)
      return 8;
   return CountBits(fmt.a_mask | fmt.r_mask | fmt.g_mask | fmt.b_mask);
}

//----------------------------

#if defined _MSC_VER && 0

#pragma warning(disable:4035)
__declspec(naked) bool SphereCollide(const I3D_bsphere &vf_bs, const I3D_bsphere &bs2){
   __asm{
      push edi
      mov eax, [esp+4+4]
      mov edi, [esp+8+4]
      push eax                //make stack variable
      fld dword ptr[eax+0]
      fsub dword ptr[edi+0]
      fmul st, st
      fld dword ptr[eax+4]
      fsub dword ptr[edi+4]
      fmul st, st
      faddp st(1), st
      fld dword ptr[eax+8]
      fsub dword ptr[edi+8]
      fmul st, st
      faddp st(1), st
      fld dword ptr[eax+12]
      fadd dword ptr[edi+12]
      fmul st, st
      fsubp st(1), st
      fstp dword ptr[esp]
      pop eax
      pop edi
      shr eax, 31
      ret 8
   }
}
#pragma warning(default:4035)

#endif

//----------------------------

//----------------------------
// check sphere intersection with viewing frustum
// return false if sphere is outside frustum
// set clip flag if sphere collides with any plane
//----------------------------

#ifdef _MSC_VER_

/*
__declspec(naked) bool SphereInFrustum_x86(const S_expanded_view_frustum &vf, const I3D_bsphere &bsphere, bool &clip){
   __asm{
      push ecx
      mov ecx, [esp+8+4]
      //call Prefetch1
      push ebx
      push edx
      push esi
      push edi
                              //registers:

                              //eax - free
                              //ebx - sphere_radius bitmask
                              // dl - ret
                              // dh - clip
                              //esi - vf*
                              //edi - loop counter

                              //st(0) .. st(2) - sphere_pos
                              //st(3) - -sphere_radius
      fld dword ptr [ecx+0xc]
      fchs

      mov ebx, [ecx+0xc]
      fld dword ptr [ecx+8]
      fld dword ptr [ecx+4]
      fld dword ptr [ecx+0]

      //mov esi, vf
      mov esi, [esp+4+20]
      mov edx, 0x0001
                              //get number of planes
      mov edi, [esi + (SIZE S_plane) * MAX_FRUSTUM_CLIP_PLANES]

   main_loop:

      //lea ecx, [esi+16]
      //call Prefetch1
                              //Dot
      fld dword ptr [esi + MAX_FRUSTUM_CLIP_PLANES*0]
      fmul st, st(1)
      fld dword ptr [esi + MAX_FRUSTUM_CLIP_PLANES*4]
      fmul st, st(3)
      faddp st(1), st
      fld dword ptr [esi + MAX_FRUSTUM_CLIP_PLANES*8]
      fmul st, st(4)
      faddp st(1), st

                              //add view_frustum_d
      fadd dword ptr [esi + MAX_FRUSTUM_CLIP_PLANES*12]


                              //compare against negative radius
      fcom st(4)

      add esi, 4
                              //compare against sphere_radius
      fstp dword ptr [esp+4+20]
      cmp [esp+4+20], ebx
      jge skip1

                              //compare against negative radius (cont.)
      fstsw ax
      sahf
      jb skip2
      mov dh, 1
   skip2:
                              //next side

      dec edi
      jnz main_loop
                              //simulate cmp eax, dword (2 bytes left)
      _emit 0x3d
      _emit 0
      _emit 0
   skip1:                     //test failed
      mov dl, 0               //2 bytes long

                              //remove stuff from FPU
      fstp dword ptr [esp+4+20]
      fstp dword ptr [esp+4+20]
      fstp dword ptr [esp+4+20]
      xor eax, eax            //return value in dword
      fstp dword ptr [esp+4+20]
                              //store values
      mov edi, [esp+12+20]
      mov al, dl
      mov [edi], dh

      pop edi
      pop esi
      pop edx
      pop ebx
      pop ecx

      ret 12
   }
}

//----------------------------

static __int64 sign_64 = 0x8000000080000000;

__declspec(naked) bool SphereInFrustum_3DNow(const S_expanded_view_frustum &vf, const I3D_bsphere &bsphere, bool &clip){

   __int64 tmp0, tmp1;
   __asm{
      femms
                              //eax - free
                              //esi - vf*
                              //edi - loop counter
      mov eax, [esp+8]        //get bsphere
      prefetch [eax]

      push ebx
      push esi
      push edi

      mov esi, [esp+4+12]     //get expanded view frustum

                              //MM0 - 2X bsphere.pos.x
                              //MM1 - 2X bsphere.pos.y
                              //MM2 - 2X bsphere.pos.z
                              //MM3 - 2X bsphere.radius
                              //mm6 - clip flags
      mov ebx, [eax+0]
      mov edi, [eax+4]
      mov dword ptr[tmp0+0], ebx
      mov dword ptr[tmp0+4], ebx
      mov dword ptr[tmp1+0], edi
      mov dword ptr[tmp1+4], edi
      movq mm0, tmp0
      movq mm1, tmp1

      mov ebx, [eax+8]
      mov edi, [eax+12]
      mov dword ptr[tmp0+0], ebx
      mov dword ptr[tmp0+4], ebx
      mov dword ptr[tmp1+0], edi
      mov dword ptr[tmp1+4], edi
      movq mm2, tmp0
      movq mm3, tmp1

                              //default clipping = no
      movq mm6, sign_64

                              //get number of planes
      mov edi, [esi + (SIZE S_plane) * MAX_FRUSTUM_CLIP_PLANES]
      inc edi
                              //process 2 planes simultaneously
      shr edi, 1
      jz out1
   loop1:
                              //make x
      movq mm4, [esi + MAX_FRUSTUM_CLIP_PLANES * 0]
      pfmul mm4, mm0
                              //make y
      movq mm5, [esi + MAX_FRUSTUM_CLIP_PLANES * 4]
      pfmul mm5, mm1
                              //add x + y
      pfadd mm4, mm5
                              //make z
      movq mm5, [esi + MAX_FRUSTUM_CLIP_PLANES * 8]
      pfmul mm5, mm2
                              //add (x + y) + z
      pfadd mm4, mm5
                              //add d
      pfadd mm4, [esi + MAX_FRUSTUM_CLIP_PLANES * 12]
                              //distances to 2 planes are in MM4

                              //update clip flags
      movq mm5, mm3
      pfadd mm5, mm4
      pand mm6, mm5
                              //check if out
      pfsub mm4, mm3
      pand mm4, sign_64
      //pcmpeq mm4, sign_64
      jnz false1

                              //move to next four floats
      add esi, 8
      dec edi
      jnz loop1
   out1:
                              //store clipping
      mov eax, [esp+12+12]
      cmp ebx, 0xf
      setnz [eax]
                              //Z = 0
      cmp ebx, ebx
   false1:
      setz al
   
      pop edi
      pop esi
      pop ebx

      and eax, 1

      femms

      ret 12
   }
}

//----------------------------

__declspec(naked) bool SphereInFrustum_SSE(const S_expanded_view_frustum &vf, const I3D_bsphere &bsphere, bool &clip){

   __asm{
                                 ;eax - free
                                 ;ebx - clip flags
                                 ;esi - vf*
                                 ;edi - loop counter
      mov eax, [esp+8]           ;get bsphere
      prefetcht0 [eax]

      push ebx
      push esi
      push edi

      mov esi, [esp+4+12]        ;get expanded view frustum

                                 ;XMM0 - 4X bsphere.pos.x
                                 ;XMM1 - 4X bsphere.pos.y
                                 ;XMM2 - 4X bsphere.pos.z
                                 ;XMM3 - 4X bsphere.radius
      movups xmm0, [eax]
      prefetcht0 [esi]
      movaps xmm1, xmm0
      movaps xmm2, xmm0
      movaps xmm3, xmm0
      shufps xmm0, xmm0, 0x00
      shufps xmm1, xmm1, 0x55
      shufps xmm2, xmm2, 0xaa
      shufps xmm3, xmm3, 0xff

                                 ;default clipping = no
      mov ebx, 0xf

                                 ;get number of planes
      mov edi, [esi + (SIZE S_plane) * MAX_FRUSTUM_CLIP_PLANES]
      add edi, 3
      shr edi, 2
      jz out2
   loop2:
      prefetcht0 [esi + 16]
                                 ;make x
      movups xmm4, [esi + 0]  
      prefetcht0 [esi + 16 + MAX_FRUSTUM_CLIP_PLANES*4]
      mulps xmm4, xmm0
                                 ;make y
      prefetcht0 [esi + 16 + MAX_FRUSTUM_CLIP_PLANES*8]
      movups xmm5, [esi + MAX_FRUSTUM_CLIP_PLANES*4]
      prefetcht0 [esi + 16 + MAX_FRUSTUM_CLIP_PLANES*12]
      mulps xmm5, xmm1
                                 ;add x + y
      addps xmm4, xmm5
                                 ;make z
      movups xmm5, [esi + MAX_FRUSTUM_CLIP_PLANES*8]
      mulps xmm5, xmm2
                                 ;add (x + y) + z
      addps xmm4, xmm5
                                 ;add d
      movups xmm5, [esi + MAX_FRUSTUM_CLIP_PLANES*12]
      addps xmm4, xmm5
                                 ;distances to 4 planes are in xmm4

                                 ;update clip flags
      movaps xmm5, xmm3
      addps xmm5, xmm4
      movmskps eax, xmm5
      and ebx, eax
                                 ;check if out
      subps xmm4, xmm3
      movmskps eax, xmm4
      cmp eax, 0xf
      jnz false2

                                 ;move to next four floats
      add esi, 16             
      dec edi
      jnz loop2
   out2:
                                 ;store clipping
      mov eax, [esp+12+12]
      cmp ebx, 0xf
      setnz [eax]
                                 ;Z = 0
      cmp ebx, ebx
   false2:
      setz al
   
      pop edi
      pop esi
      pop ebx

      and eax, 1

      ret 12

   }
}
*/

//----------------------------

#pragma warning(disable:4035)
__declspec(naked) bool SphereInVF(const S_view_frustum_base &vf, const I3D_bsphere &bsphere, bool &clip){

   __asm{
      push ecx
      mov ecx, [esp+8+4]
      //call Prefetch1
      push ebx
      push edx
      push esi
      push edi
                              //registers:

                              //eax - free
                              //ebx - sphere_radius bitmask
                              // dl - ret
                              // dh - clip
                              //esi - vf*
                              //edi - loop counter

                              //st(0) .. st(2) - sphere_pos
                              //st(3) - -sphere_radius
      fld dword ptr [ecx+0xc]
      fchs

      mov ebx, [ecx+0xc]
      fld dword ptr [ecx+8]
      fld dword ptr [ecx+4]
      fld dword ptr [ecx+0]

      //mov esi, vf
      mov esi, [esp+4+20]
      mov edx, 0x0001
                              //get number of planes
      mov edi, [esi + (SIZE S_plane) * MAX_FRUSTUM_CLIP_PLANES]

   main_loop:

      lea ecx, [esi+16]
      //call Prefetch1
                              //Dot
      fld dword ptr [esi+0]
      fmul st, st(1)
      fld dword ptr [esi+4]
      fmul st, st(3)
      faddp st(1), st
      fld dword ptr [esi+8]
      fmul st, st(4)
      faddp st(1), st

                              //add view_frustum_d
      fadd dword ptr [esi+12]


                              //compare against negative radius
      fcom st(4)

      add esi, 16
                              //compare against sphere_radius
      fstp dword ptr [esp+4+20]
      cmp [esp+4+20], ebx
      jge skip1

                              //compare against negative radius (cont.)
      fstsw ax
      sahf
      jb skip2
      mov dh, 1
   skip2:
                              //next side

      dec edi
      jnz main_loop
                              //simulate cmp eax, dword (2 bytes left)
      //_emit 0x3d
      //_emit 0
      //_emit 0
      jmp skip3
   skip1:                     //test failed
      mov dl, 0               //2 bytes long
   skip3:

                              //remove stuff from FPU
      fstp dword ptr [esp+4+20]
      fstp dword ptr [esp+4+20]
      fstp dword ptr [esp+4+20]
      xor eax, eax            //return value in dword
      fstp dword ptr [esp+4+20]
                              //store values
      mov edi, [esp+12+20]
      mov al, dl
      mov [edi], dh

      pop edi
      pop esi
      pop edx
      pop ebx
      pop ecx

      ret 12
   }
}
#pragma warning(default:4035)

//----------------------------
#else //_MSC_VER

bool SphereInVF(const S_view_frustum_base &vf, const I3D_bsphere &bsphere, bool &clip){

   clip = false;
   float neg_radius = -bsphere.radius;
   const S_plane *pp = vf.clip_planes;

   int i = vf.num_clip_planes;
   do{
      float d = pp->d + pp->normal.Dot(bsphere.pos);

      if(d >= bsphere.radius) return false;
      if(d >= neg_radius) clip = true;
   }while(++pp, --i);

   return true;
}

#endif   //!_MSC_VER

//----------------------------

bool AreAllVerticesInVF(const S_view_frustum_base &vf, const S_vector *verts, dword num_verts){

                              //transform input vertices

#if defined _MSC_VER && 0

   byte rtn = true;
   __asm{
      push ecx
                              //eax = current status
                              //ebx = current vertex
                              //ecx = vertex count
                              //edx = plane counter
                              //esi = current vertex
                              //edi = current plane
      push eax                //local variable
      mov esi, verts
      mov ecx, num_verts
   l:
                              //check all planes
      mov edi, vf
      mov edx, [edi + SIZE S_plane * MAX_FRUSTUM_CLIP_PLANES]
   ll1:
                              //compute dot-product
      fld dword ptr[esi + 0]
      fmul dword ptr[edi + 0]
      fld dword ptr[esi + 4]
      fmul dword ptr[edi + 4]
      faddp st(1), st
      fld dword ptr[esi + 8]
      fmul dword ptr[edi + 8]
      faddp st(1), st
      fadd dword ptr[edi + 12]
                              //compare to zero
      fstp dword ptr[esp]
      add edi, SIZE S_plane
      test byte ptr[esp+3], 0x80
      jnz ok1

      mov rtn, 0
      jmp end
   ok1:
      dec edx
      jnz ll1

      add esi, SIZE S_vector
      dec ecx
      jnz l
   end:

      pop eax                 //clean stack

      pop ecx
   }
   return rtn;

#else

   for(dword i=0; i<num_verts; i++){
      const S_plane *pp = vf.clip_planes;
      int j = vf.num_clip_planes;
      do{
         float d = pp->d + pp->normal.Dot(verts[i]);
         if(d >= 0.0f)
            return false;
      }while(++pp, --j);
   }

#endif
   return true;
}

//----------------------------

bool IsAnyVertexInVF(const S_view_frustum &vf, const S_vector *verts, dword num_verts){

                              //transform input vertices

#if defined _MSC_VER && 0

   byte rtn = false;
   __asm{
      push ecx
                              //eax = current status
                              //ebx = current vertex
                              //ecx = vertex count
                              //edx = plane counter
                              //esi = current vertex
                              //edi = current plane
      push eax                //local variable
      mov esi, verts
      mov ecx, num_verts
   l:
                              //check all planes
      mov edi, vf
      mov edx, [edi + SIZE S_plane * MAX_FRUSTUM_CLIP_PLANES]
   ll1:
                              //compute dot-product
      fld dword ptr[esi + 0]
      fmul dword ptr[edi + 0]
      fld dword ptr[esi + 4]
      fmul dword ptr[edi + 4]
      faddp st(1), st
      fld dword ptr[esi + 8]
      fmul dword ptr[edi + 8]
      faddp st(1), st
      fadd dword ptr[edi + 12]
                              //compare to zero
      fstp dword ptr[esp]
      add edi, SIZE S_plane
      test byte ptr[esp+3], 0x80
      jz ok1

      mov rtn, 1
      jmp end
   ok1:
      dec edx
      jnz ll1

      add esi, SIZE S_vector
      dec ecx
      jnz l
   end:

      pop eax                 //clean stack

      pop ecx
   }
   return rtn;

#else

   for(dword i=0; i<num_verts; i++){
      const S_plane *pp = vf.clip_planes;
      int j = vf.num_clip_planes;
      do{
         float d = pp->d + pp->normal.Dot(verts[i]);
         if(d >= 0.0f)
            goto next;
      }while(++pp, --j);
      return true;
   next:{}
   }

#endif
   return false;
}

//----------------------------
                              //stuff for computing contour:

                              //links to 2 edges for each edge
static const struct{
   byte link[2][2];           //[neg|pos][]
} edge_links[12] = {
   {4, 8, 5, 9},  //0
   {4, 10, 5, 11},//1
   {6, 8, 7, 9},  //2
   {6, 10, 7, 11},//3
   {0, 8, 1, 10}, //4
   {0, 9, 1, 11}, //5
   {2, 8, 3, 10}, //6
   {2, 9, 3, 11}, //7
   {0, 4, 2, 6},  //8
   {0, 5, 2, 7},  //9
   {1, 4, 3, 6},  //10
   {1, 5, 3, 7}   //11
};

static const struct{
   byte vi[2];                //[neg|pos]
} vertex_indicies[12] = {
   0, 1,    2, 3,    4, 5,    6, 7,
   0, 2,    1, 3,    4, 6,    5, 7,
   0, 4,    1, 5,    2, 6,    3, 7,
};

void ComputeContour(const I3D_bbox &bb_in, const S_matrix &tm, const S_vector &look,
   S_vector contour_points[6], dword &num_cpts, bool orthogonal){

   num_cpts = 0;

   enum{
      EDGE_DIR_NEG = 0,       //edge pointing to negative direction
      EDGE_DIR_POS = 1,       //edge pointing to positive direction
      EDGE_VISIBLE = 2,       //edge is visible
      EDGE_DIR_MASK = 1,
   };
                              //edge check-in flags - used XOR, so that when edge is
                              // marked twice, its EDGE_VISIBLE flag is cleared
   union{
      byte edge_check_in[12];
      dword dw[3];
   };
   dw[0] = 0; dw[1] = 0; dw[2] = 0;
   
                              //transform edge bbox
   I3D_bbox bb_trans;
   bb_trans.min = bb_in.min * tm;
   bb_trans.max = bb_in.max * tm;

   if(!orthogonal){
      S_vector dir_min = look - bb_trans.min;
      S_vector dir_max = look - bb_trans.max;

      bool inside = true;

      if(dir_min.Dot(tm(0)) < 0.0f){
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }else
      if(dir_max.Dot(tm(0)) >= 0.0f){
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }

      if(dir_min.Dot(tm(1)) < 0.0f){
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }else
      if(dir_max.Dot(tm(1)) >= 0.0f){
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }

      if(dir_min.Dot(tm(2)) < 0.0f){
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }else
      if(dir_max.Dot(tm(2)) >= 0.0f){
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         inside = false;
      }
      if(inside)
         return;
   }else{
      if(look.Dot(tm(0)) < 0.0f){
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }else{
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }

      if(look.Dot(tm(1)) < 0.0f){
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[9]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[8]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }else{
         edge_check_in[10] ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[11] ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }

      if(look.Dot(tm(2)) < 0.0f){
         edge_check_in[4]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[1]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[5]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[0]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }else{
         edge_check_in[2]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[7]  ^= EDGE_VISIBLE | EDGE_DIR_POS;
         edge_check_in[3]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
         edge_check_in[6]  ^= EDGE_VISIBLE | EDGE_DIR_NEG;
      }
   }
                              //get any beginning edge
   for(int ei_beg=12; ei_beg--; ){
      if(edge_check_in[ei_beg]&EDGE_VISIBLE)
         break;
   }
   assert(ei_beg!=-1);

   S_vector bbd_t[3];         //three vector aligned with tm axes, scaled by bbox
   bbd_t[0] = tm(0) * (bb_in.max.x - bb_in.min.x);
   bbd_t[1] = tm(1) * (bb_in.max.y - bb_in.min.y);
   bbd_t[2] = tm(2) * (bb_in.max.z - bb_in.min.z);

   int ei = ei_beg;
   do{
      int vi = vertex_indicies[ei].vi[edge_check_in[ei]&EDGE_DIR_MASK];

      assert(num_cpts < 6);
      S_vector &cp = contour_points[num_cpts++];
      cp = bb_trans.min;
      if(vi&1) cp += bbd_t[0];
      if(vi&2) cp += bbd_t[1];
      if(vi&4) cp += bbd_t[2];

                              //move to the next visible edge
      int ei1 = edge_links[ei].link[edge_check_in[ei]&EDGE_DIR_MASK][0];
      ei = edge_links[ei].link[edge_check_in[ei]&EDGE_DIR_MASK][!(edge_check_in[ei1]&EDGE_VISIBLE)];
   } while(ei!=ei_beg);
}

//----------------------------

dword GetSizeOfVertex(dword fvf_flags){

   dword rtn = 0;
   switch(fvf_flags&D3DFVF_POSITION_MASK){
   case D3DFVF_XYZ: rtn += sizeof(float) * 3; break;
   case D3DFVF_XYZRHW: rtn += sizeof(float) * 4; break;
   default:                   //D3DFVF_XYZB?
      rtn += sizeof(float)*(3+((fvf_flags&D3DFVF_POSITION_MASK)/2-2));
   }

   if(fvf_flags&D3DFVF_NORMAL) rtn += sizeof(float) * 3;
   if(fvf_flags&D3DFVF_DIFFUSE) rtn += sizeof(dword);
   if(fvf_flags&D3DFVF_SPECULAR) rtn += sizeof(dword);
   if(fvf_flags&D3DFVF_TEXTURE_SPACE) rtn += sizeof(S_vector) * 3;

   static const dword tex_size[] = {2, 3, 4, 1};

   for(int i = (fvf_flags&D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT; i--; ){
      dword coord_code = (fvf_flags >> (16+i*2)) & 0x3;
      rtn += tex_size[coord_code] * sizeof(float);
   }

   return rtn;
}

//----------------------------

int GetVertexNormalOffset(dword fvf_flags){
   if(!(fvf_flags&D3DFVF_NORMAL))
      return -1;
   switch(fvf_flags&D3DFVF_POSITION_MASK){
   case D3DFVF_XYZ: return sizeof(float) * 3; break;
   case D3DFVF_XYZRHW: return sizeof(float) * 4; break;
   default: return sizeof(float)*(3+((fvf_flags&D3DFVF_POSITION_MASK)/2-2));
   }
}

//----------------------------

int GetVertexUvOffset(dword fvf_flags){
   if(!(fvf_flags&D3DFVF_TEXCOUNT_MASK))
      return -1;
   int rtn = GetVertexNormalOffset(fvf_flags|D3DFVF_NORMAL);
   if(fvf_flags&D3DFVF_NORMAL) rtn += sizeof(float)*3;
   if(fvf_flags&D3DFVF_DIFFUSE) rtn += sizeof(dword);
   if(fvf_flags&D3DFVF_SPECULAR) rtn += sizeof(dword);
   if(fvf_flags&D3DFVF_TEXTURE_SPACE) rtn += sizeof(S_vector) * 3;
   return rtn;
}

//----------------------------

bool IsPointInBBox(const S_vector &in_v, const I3D_bbox &bb, const S_matrix &m_bb_inv, float &weight){

                              //get point in local coords of bbox
   S_vector v = in_v * m_bb_inv;
   if(v.x > bb.min.x && v.x < bb.max.x &&
      v.y > bb.min.y && v.y < bb.max.y &&
      v.z > bb.min.z && v.z < bb.max.z){

      float f = (bb.max.z - bb.min.z);
      assert(!IsMrgZeroLess(f));
      weight = (v.z - bb.min.z) / f;
      return true;
   }
   return false;
}

//----------------------------

bool IsPointInBBoxXY(const S_vector &in_v, const I3D_bbox &bb, const S_matrix &m_bb_inv, float &weight){

                              //get point in local coords of bbox
   S_vector v = in_v * m_bb_inv;
   if(v.x > bb.min.x && v.x < bb.max.x &&
      v.y > bb.min.y && v.y < bb.max.y){

      float f = (bb.max.z - bb.min.z);
      assert(!IsMrgZeroLess(f));
      weight = (v.z - bb.min.z) / f;
      return true;
   }
   return false;
}

//----------------------------

void LightUpdateColor(PI3D_driver drv, const S_vector &color, float power, S_vector *col, dword *dw_color, bool surf_match){

   *col = (color * power);
                              //match dword color to current pixel format
   int c[3] = {
      FloatToInt((*col)[0]*255.0f),
      FloatToInt((*col)[1]*255.0f),
      FloatToInt((*col)[2]*255.0f)
   };
   if(surf_match){
      const S_pixelformat &pf = *drv->GetGraphInterface()->GetPixelFormat();
      for(int i=0; i<3; i++){
         dword dw;
         const dword &mask = (&pf.r_mask)[i];
         dw = FindLastBit(mask);
         dw = _rotr(mask, dw-7);
         c[i] += ((~(signed char)dw)+1)/2;
         c[i] = Min(255, c[i]) & dw;
      }
   }
   for(int i=0; i<3; i++) c[i] = Max(0, Min(255, c[i]));

   *dw_color = 0xff000000 | (c[0]<<16) | (c[1]<<8) | c[2];
}

//----------------------------

int GetVertexComponentOffset(dword fvf_flags, dword fvf_component){

                              //check if component is present
   if(!(fvf_flags&fvf_component&(D3DFVF_POSITION_MASK|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_SPECULAR))){
                              //check which texture component is being queried
      if(((fvf_component&D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT) >=
         ((fvf_flags&D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT))
      return -1;
   }

   int offs = 0;
   if(!(fvf_component&D3DFVF_POSITION_MASK)){
      switch(fvf_flags&D3DFVF_POSITION_MASK){
      case D3DFVF_XYZ: offs += sizeof(S_vector); break;
      case D3DFVF_XYZB1: offs += sizeof(S_vector) + sizeof(float) * 1; break;
      case D3DFVF_XYZB2: offs += sizeof(S_vector) + sizeof(float) * 2; break;
      case D3DFVF_XYZB3: offs += sizeof(S_vector) + sizeof(float) * 3; break;
      case D3DFVF_XYZB4: offs += sizeof(S_vector) + sizeof(float) * 4; break;
      case D3DFVF_XYZB5: offs += sizeof(S_vector) + sizeof(float) * 5; break;
      case D3DFVF_XYZRHW: offs += sizeof(S_vectorw); break;
      default:
         assert(0);
      }
      if(!(fvf_component&D3DFVF_NORMAL)){
         if(fvf_flags&D3DFVF_NORMAL)
            offs += sizeof(S_vector);
         if(!(fvf_component&D3DFVF_PSIZE)){
            if(fvf_flags&D3DFVF_PSIZE)
               offs += sizeof(float);
            if(!(fvf_component&D3DFVF_DIFFUSE)){
               if(fvf_flags&D3DFVF_DIFFUSE)
                  offs += sizeof(dword);
               if(!(fvf_component&D3DFVF_SPECULAR)){
                  if(fvf_flags&D3DFVF_SPECULAR)
                     offs += sizeof(dword);
                                 //texture component is being queried
                  dword txtc = (fvf_component&D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
                  for(dword i=0; i<txtc; i++){
                     dword coord_code = (fvf_flags >> (16+i*2)) & 0x3;
                     static const dword tex_size[] = {2, 3, 4, 1};
                     offs += tex_size[coord_code] * sizeof(float);
                  }
               }
            }
         }
      }
   }
   return offs;
}

//----------------------------

bool I3D_texel_collision::ComputeUVFromPoint(const I3D_text_coor &uv0, const I3D_text_coor &uv1, const I3D_text_coor &uv2){

   float u1, u2;
   S_vector edge_dir = *v[2] - *v[1];

   S_vector p_dir = point_on_plane - *v[0];
   if(!LineIntersection(*v[0], p_dir, *v[1], edge_dir, u1, u2))
      return false;
#if 0
   drv->DebugLine(v0, v0+p_dir, 1, 0xffff0000);
   drv->DebugLine(v1, v1+edge_dir, 1, 0xff00ff00);
   drv->DEBUG(C_fstr("u1 = %.2f   u2 = %.2f", u1, u2));
#endif
   if(IsAbsMrgZero(u1))
      return false;
   S_vector pp = *v[0] + p_dir * u1;
   //drv->DebugLine(point_on_plane, pp, 1, 0xff0000ff);
                              //get uv on edge point
   float e_len_2 = edge_dir.Square();
   if(IsAbsMrgZero(e_len_2))
      return false;
   u2 = (pp - *v[1]).Magnitude() / I3DSqrt(e_len_2);

#if 0
   drv->DEBUG(C_fstr("UV0:  %.2f     %.2f", uv0.u, uv0.v));
   drv->DEBUG(C_fstr("UV1:  %.2f     %.2f", uv1.u, uv1.v));
   drv->DEBUG(C_fstr("UV2:  %.2f     %.2f", uv2.u, uv2.v));
#endif
   tex = uv1 + (uv2 - uv1) * u2;

   u1 = 1.0f / u1;
   tex = uv0 + (tex - uv0) * u1;

                              //clamp, rather than tile
   tex.x = Max(0.0f, Min(tex.x, .9999f));
   tex.y = Max(0.0f, Min(tex.y, .9999f));
   /*
   tex.x = (float)fmod(tex.x, 1.0f);
   tex.y = (float)fmod(tex.y, 1.0f);
   if(tex.x<0.0f) tex.x += 1.0f;
   if(tex.y<0.0f) tex.y += 1.0f;
   */

   return true;
}

//----------------------------

C_str MakeCubicTextureName(const C_str &name, dword side){

   assert(side<6);
   C_str ret = name;
   const char *ext = NULL;
   for(dword i=ret.Size(); i--; ){
      if(ret[i]=='.'){
         ret[i] = 0;
         ext = &name[i];
         break;
      }
   }
   static const char side_char[] = {'r', 'l', 't', 'b', 'f', 'k'};
   ret = C_fstr("%s_%c", (const char*)ret, side_char[side]);
   if(ext)
      ret += ext;
   return ret;
}

//----------------------------

#ifdef _DEBUG

const char block_profile_names[] =
   "Render\0"
   "  Clear\0"
   "  Mirror\0"
   "  Shadows\0"
   "  Preprocess\0"
   "  Draw\0"
   "Collisions\0"
   "Anims\0"
   "Particle\0"
   "Proc. txt\0"
   "Test\0"
;

#endif

//----------------------------
//----------------------------

