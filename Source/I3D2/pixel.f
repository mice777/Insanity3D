#include "pixel.h"

ps.1.1

;----------------------------
#beginfragment tex_1
tex t1
#endfragment

#beginfragment tex_0
tex t0
#endfragment

#beginfragment tex_2
tex t2
#endfragment

#beginfragment tex_3
tex t3
#endfragment

//----------------------------

#beginfragment tex_1_bem
texbem t1, t0
#endfragment

#beginfragment tex_2_bem
texbem t2, t1
#endfragment

#beginfragment tex_3_bem
texbem t3, t2
#endfragment

//----------------------------

#beginfragment tex_1_beml
texbeml t1, t0
#endfragment

#beginfragment tex_2_beml
texbeml t2, t1
#endfragment

#beginfragment tex_3_beml
texbeml t3, t2
#endfragment

;----------------------------
#beginfragment texkill_0
texkill t0
#endfragment

;----------------------------
#beginfragment texkill_1
texkill t1
#endfragment

;----------------------------
#beginfragment texkill_2
texkill t2
#endfragment

;----------------------------
#beginfragment texkill_3
texkill t3
#endfragment

;----------------------------
; Simple copy of diffuse color.
#beginfragment v0_copy
mov r0, v0
#endfragment

;----------------------------
; Simple copy of diffuse color.
#beginfragment v0_mul2x
mov_x2 r0, v0
#endfragment

;----------------------------
; Simple copy of texture.
#beginfragment t0_copy
mov r0, t0
#endfragment

;----------------------------
; Copy of texture.
#beginfragment t0_copy_inv
mov r0, 1-t0
#endfragment

;----------------------------
; Simple copy of texture.
#beginfragment t1_copy
mov r0, t1
#endfragment

;----------------------------
; Modulation in stage 0.
#beginfragment mod_t0_v0
mul r0, v0, t0
#endfragment

;----------------------------
; Modulation with t1
#beginfragment mod_r0_t1
mul r0, r0, t1
#endfragment

;----------------------------
; Modulation in stage 0.
#beginfragment modx2_t0_v0
mul_x2 r0.rgb, v0, t0
+mul r0.a, v0.a, t0.a

#endfragment

;----------------------------
; Simple copy of constant color
#beginfragment color_copy
mov r0, CP_COLOR
#endfragment

;----------------------------
; Simple copy of constant color
#beginfragment mod_color_v0
mul r0, CP_COLOR, v0
#endfragment

;----------------------------
; Copy of black color
#beginfragment copy_black
mov r0.rgb, CP_ZERO
mov r0.a, CP_ONE.a
#endfragment

;----------------------------
; Copy of black color with alpha from t0
#beginfragment copy_black_t0a
mov r0.rgb, CP_ZERO
+mov r0.a, t0.a
#endfragment

;----------------------------
#beginfragment mod_t0_constcolor
mul r0, t0, CP_COLOR
#endfragment

;----------------------------
#beginfragment mod_r0_t1
mul r0, r0, t1
#endfragment

;----------------------------
#beginfragment grayscale
dp3 r_gray, r0, CP_FACTOR
lrp r0, CP_FACTOR.a, r0, r_gray
#endfragment

;----------------------------
#beginfragment modx2_r0_t0
mul_x2 r0.rgb, r0, t0
+mul r0.a, r0.a, t0.a
#endfragment

;----------------------------
#beginfragment modx2_r0_t1
mul_x2 r0.rgb, r0, t1
+mul r0.a, r0.a, t1.a
#endfragment

;----------------------------
#beginfragment modx2_r0_t2
mul_x2 r0.rgb, r0, t2
+mul r0.a, r0.a, t2.a
#endfragment

;----------------------------
#beginfragment modx2_r0_t3
mul_x2 r0.rgb, r0, t3
+mul r0.a, r0.a, t3.a
#endfragment

;----------------------------
#beginfragment modx2_t0_t1
mul_x2 r0.rgb, t0, t1
+mul r0.a, t0.a, t1.a
#endfragment

;----------------------------
#beginfragment modx2_r0_r1
mul_x2 r0.rgb, r0, r1
+mul r0.a, r0.a, r1.a
#endfragment

;----------------------------
#beginfragment modx2_t0_r1
mul_x2 r0.rgb, t0, r1
+mul r0.a, t0.a, r1.a
#endfragment

;----------------------------
#beginfragment add_t0_v0
add r0.rgb, t0, v0
mov r0.a, v0.a
#endfragment

;----------------------------
#beginfragment add_t1_v0
add r0, t1, v0
#endfragment

;----------------------------
; Shadow cast fragment.
#beginfragment shadow_cast

mov r0.rgb, CP_COLOR
+mov r0.a, t0.a

#endfragment

;----------------------------
; Shadow receive fragment.
#beginfragment shadow_receive

mov r0.rgb, t0
+mov r0.a, v0.a

#endfragment


;----------------------------
; Copy r0 blue to alpha.
#beginfragment r0_b_2_a

mov r0.a, r0.b

#endfragment


;----------------------------

#beginfragment blend_by_alpha
                              //modulate t1 and t2 using t0.a
mul r1, t1, 1-t0.a
mad r1, t2, t0.a, r1

#endfragment


//----------------------------
#beginfragment night_view

dp3 r0, t0, CP_COLOR1                      ; put to grayscale
;mad r1, r0, CP_FACTOR1, CP_FACTOR   ; select green or white, based on brightness
mul r0, 1-r0, 1-r0                  ; increase curve by multiplying 1-inversed values together few times
mul r0, r0, r0
mad r1, 1-r0, CP_FACTOR1, CP_FACTOR ; select green or white, based on brightness
;mul r0, r0, r0
mul r0, r0, r0
mul r0, r0, r0

; multiply by selected green-white color, and add some base color
mad r0, 1-r0, r1, CP_COLOR

#endfragment


;----------------------------
; Test fragment.
#beginfragment ps_test_bump

dp3_sat r1, t1_bx2, v1_bx2
;mul r1, t2, r1.a

;add r0.rgb, r0, r1
;mul r1, r1, t1.a
;mad r0, r1, t2, r0
;add_d2 r1, r1, CP_ONE

lrp r1, t1.a, r1, CP_ONE

mad r1, r1, t0, r0

                              //environment
;mul_x2 r1, r1, t2

mov r0, r1

;mov r0, t1.a


#endfragment


;----------------------------
; Test fragment.
#beginfragment ps_test

nop

#endfragment