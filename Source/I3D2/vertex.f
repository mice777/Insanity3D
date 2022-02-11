#include "vertex.h"

vs.1.1

;--------------------------------------------------------
; Transformations.
;--------------------------------------------------------


;----------------------------
; Simple transformation.
#beginfragment transform

m4x4 oPos, INPUT_POSITION, CV_MAT_TRANSFORM

#endfragment

;----------------------------
; Simple multiplication of input pos by vector.
#beginfragment mul_transform

mul oPos, INPUT_POSITION, CV_MAT_TRANSFORM

#endfragment


;----------------------------
; Normalize vector to unith-length. This macro also sets w component of the vector by its lenth (as side-effect).
macro normalize_vector V

dp3 %V.w, %V, %V
rsq %V.w, %V.w
mul %V.xyz, %V.xyz, %V.w

endm


;----------------------------
; Transform input vector by matrix expressed in 2 const regs by [quatenrion] and [position, scale]
macro multiply_pos_by_quaternion IN_POS OUT_POS CV_QUAT CV_POS_SCALE

                              //v_coef
dp3 r_coef.x, %CV_QUAT, -%CV_QUAT
mad r_coef.x, %CV_QUAT.w, %CV_QUAT.w, r_coef.x

                              //c_coef
mov r_coef.z, %CV_QUAT.w

                              //u_coef
dp3 r_coef.y, %CV_QUAT.xyz, %IN_POS.xyz
                              //multiply c_coef and u_coef by 2
add r_coef.yz, r_coef, r_coef

                              //cross-product
mul r_cross.xyz, -%CV_QUAT.yzx, %IN_POS.zxy
mad r_cross.xyz,  %CV_QUAT.zxy, %IN_POS.yzx, r_cross.xyz

                              //world_pos = (v_coef * src + u_coef * q + c_coef * R1) * scale + pos
                              //c_coef * R1
mul %OUT_POS.xyz, r_cross.xyz, r_coef.z
                              // += u_coef * q
mad %OUT_POS.xyz, %CV_QUAT.xyz, r_coef.y, %OUT_POS.xyz
                              // += v_coef * src 
mad %OUT_POS.xyz, %IN_POS.xyz, r_coef.x, %OUT_POS.xyz
                              //scale and translate
mad %OUT_POS.xyz, %OUT_POS.xyz, %CV_POS_SCALE.w, %CV_POS_SCALE.xyz

endm


;----------------------------
; Transform input vector and normal by matrix expressed in 2 const regs by [quatenrion] and [position, scale]
macro multiply_pos_normal_by_quaternion IN_POS IN_NORMAL OUT_POS OUT_NORMAL CV_QUAT CV_POS_SCALE

                              //v_coef (in x)
dp3 r_coef.x, %CV_QUAT, -%CV_QUAT
mad r_coef.x, %CV_QUAT.w, %CV_QUAT.w, r_coef.x

                              //c_coef (in y)
mov r_coef.y, %CV_QUAT.w

                              //u_coef (in y (pos) and w (normal))
dp3 r_coef.z, %CV_QUAT.xyz, %IN_POS.xyz
dp3 r_coef.w, %CV_QUAT.xyz, %IN_NORMAL.xyz
                              //multiply c_coef and u_coefs by 2
add r_coef.yzw, r_coef, r_coef

                              //cross-products
mul r_cross_p.xyz, -%CV_QUAT.yzx, %IN_POS.zxy
mad r_cross_p.xyz,  %CV_QUAT.zxy, %IN_POS.yzx, r_cross_p.xyz

mul r_cross_n.xyz, -%CV_QUAT.yzx, %IN_NORMAL.zxy
mad r_cross_n.xyz,  %CV_QUAT.zxy, %IN_NORMAL.yzx, r_cross_n.xyz

                              //world_pos = (v_coef * src + u_coef * q + c_coef * R1) * scale + pos
                              //c_coef * R1
mul %OUT_POS.xyz, r_cross_p.xyz, r_coef.y
                              // += u_coef * q
mad %OUT_POS.xyz, %CV_QUAT.xyz, r_coef.z, %OUT_POS.xyz
                              // += v_coef * src 
mad %OUT_POS.xyz, %IN_POS.xyz, r_coef.x, %OUT_POS.xyz
                              //scale and translate
mad %OUT_POS.xyz, %OUT_POS.xyz, %CV_POS_SCALE.w, %CV_POS_SCALE.xyz

                              //world normal = v_coef * src + u_coef * q + c_coef * R1
mul %OUT_NORMAL.xyz, r_cross_n.xyz, r_coef.y
mad %OUT_NORMAL.xyz, %CV_QUAT.xyz, r_coef.w, %OUT_NORMAL.xyz
mad %OUT_NORMAL.xyz, %IN_NORMAL.xyz, r_coef.x, %OUT_NORMAL.xyz

endm


;----------------------------
; Transform using matrix palette skinning.
#beginfragment transform_matrix_palette

; transform pos and normal by matrix 0
mov a0.x, INPUT_BLENDWEIGHT.x
m4x3 r_v0.xyz, INPUT_POSITION, c[a0.x + CV_MAT_PALETTE_BASE]
m3x3 r_n0.xyz, INPUT_NORMAL, c[a0.x + CV_MAT_PALETTE_BASE]

;multiply_pos_normal_by_quaternion INPUT_POSITION INPUT_NORMAL r_v0 r_n0 c[a0.x+CV_MAT_PALETTE_BASE] c[a0.x+CV_MAT_PALETTE_BASE+1]

; transform pos and normal by matrix 1
mov a0.x, INPUT_BLENDWEIGHT.y
m4x3 r_v1.xyz, INPUT_POSITION, c[a0.x + CV_MAT_PALETTE_BASE]
m3x3 r_n1.xyz, INPUT_NORMAL, c[a0.x + CV_MAT_PALETTE_BASE]

;multiply_pos_normal_by_quaternion INPUT_POSITION INPUT_NORMAL r_v1 r_n1 c[a0.x+CV_MAT_PALETTE_BASE] c[a0.x+CV_MAT_PALETTE_BASE+1]

; linear blend by weight
mul r_world_pos.xyz, r_v0.xyz, INPUT_BLENDWEIGHT.z
sub r_inv_weight.x, CV_ONE, INPUT_BLENDWEIGHT.z
mad r_world_pos.xyz, r_v1.xyz, r_inv_weight.x, r_world_pos.xyz
; transform from world to homogeneous clipping space
mov r_world_pos.w, CV_ONE
m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM
; blend normal
mul r_world_normal.xyz, r_n0.xyz, INPUT_BLENDWEIGHT.z
mad r_world_normal.xyz, r_n1.xyz, r_inv_weight.x, r_world_normal.xyz

normalize_vector r_world_normal

#endfragment


;----------------------------
; Transform using matrix palette skinning.
#beginfragment transform_matrix_palette_pos_only

; transform by matrix 0
mov a0.x, INPUT_BLENDWEIGHT.x
m4x3 r_v0.xyz, INPUT_POSITION, c[a0.x + CV_MAT_PALETTE_BASE]
; transform by matrix 1
mov a0.x, INPUT_BLENDWEIGHT.y
m4x3 r_v1.xyz, INPUT_POSITION, c[a0.x + CV_MAT_PALETTE_BASE]
; linear blend by weight
mul r_world_pos.xyz, r_v0.xyz, INPUT_BLENDWEIGHT.z
sub r_inv_weight.x, CV_ONE, INPUT_BLENDWEIGHT.z
mov r_world_pos.w, CV_ONE
mad r_world_pos.xyz, r_v1.xyz, r_inv_weight.x, r_world_pos.xyz
; transform from world to homogeneous clipping space
m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM

#endfragment


;----------------------------
; Transform using matrix index skinning. (not used)
#beginfragment transform_matrix_index_blend

; transform by matrix 0
;mov a0.x, INPUT_BLENDWEIGHT.x
;m4x3 r_v0.xyz, INPUT_POSITION, c[a0.x + CV_MAT_PALETTE_BASE]
;m3x3 r_n0.xyz, INPUT_NORMAL,   c[a0.x + CV_MAT_PALETTE_BASE]

; transform by matrix 1
;mov a0.x, INPUT_BLENDWEIGHT.y
;m4x3 r_v1.xyz, INPUT_POSITION, c[a0.x + CV_MAT_PALETTE_BASE]
;m3x3 r_n1.xyz, INPUT_NORMAL,   c[a0.x + CV_MAT_PALETTE_BASE]

; transform by matrix 2
;mov a0.x, INPUT_BLENDWEIGHT.z
;m4x3 r_v2.xyz, INPUT_POSITION, c[a0.x + CV_MAT_PALETTE_BASE]
;m3x3 r_n2.xyz, INPUT_NORMAL,   c[a0.x + CV_MAT_PALETTE_BASE]

; compute last weight
;dp3 r_last_weight.x, INPUT_WEIGHT, CV_ONE
;sub r_last_weight.x, CV_ONE, r_last_weight.x

; linear blend by weights
;mul r_world_pos.xyz, r_v0.xyz, INPUT_WEIGHT.x
;mad r_world_pos.xyz, r_v1.xyz, INPUT_WEIGHT.y,  r_world_pos.xyz
;mad r_world_pos.xyz, r_v2.xyz, r_last_weight.x, r_world_pos.xyz

; transform from world to homogeneous clipping space
;mov r_world_pos.w, CV_ONE
;m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM

; blend normal
;mul r_world_normal.xyz, r_n0.xyz, INPUT_WEIGHT.x
;mad r_world_normal.xyz, r_n1.xyz, INPUT_WEIGHT.y,  r_world_normal.xyz
;mad r_world_normal.xyz, r_n2.xyz, r_last_weight.x, r_world_normal.xyz

;normalize_vector r_world_normal
nop

#endfragment


;----------------------------
; Transform using weighted morphing.
#beginfragment transform_weighted_morph1_os

mov r_src, INPUT_POSITION
mad r_src.xyz, INPUT_BLENDWEIGHT.x, c[CV_MORPH_REGIONS_BASE].xyz, r_src
;mad r_src.xyz, INPUT_TEXTURE01.z, c[CV_MORPH_REGIONS_BASE].xyz, r_src
m4x4 oPos, r_src, CV_MAT_TRANSFORM

#endfragment

;----------------------------

#beginfragment transform_weighted_morph1_ws

mov r_src, INPUT_POSITION
mad r_src.xyz, INPUT_BLENDWEIGHT.x, c[CV_MORPH_REGIONS_BASE].xyz, r_src
;mad r_src.xyz, INPUT_TEXTURE01.x, c[CV_MORPH_REGIONS_BASE].xyz, r_src

mov r_world_normal, INPUT_NORMAL
;m4x3 r_world_pos.xyz, r_src, c[CV_MAT_PALETTE_BASE+16*3]
dp4 r_world_pos.x, r_src, c[CV_MAT_PALETTE_BASE + 16*3 + 0]
dp4 r_world_pos.y, r_src, c[CV_MAT_PALETTE_BASE + 16*3 + 1]
dp4 r_world_pos.z, r_src, c[CV_MAT_PALETTE_BASE + 16*3 + 2]

mov r_world_pos.w, CV_ONE
m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM

#endfragment

;----------------------------
; Transform using weighted morphing and index into morphing matrix.
#beginfragment transform_weighted_morph2_os

mov a0.x, INPUT_BLENDWEIGHT.y
mov r_src, INPUT_POSITION
mad r_src.xyz, INPUT_BLENDWEIGHT.x, c[a0.x + CV_MORPH_REGIONS_BASE].xyz, r_src
m4x4 oPos, r_src, CV_MAT_TRANSFORM

#endfragment

;----------------------------

#beginfragment transform_weighted_morph2_ws

mov a0.x, INPUT_BLENDWEIGHT.y
mov r_src, INPUT_POSITION
mad r_src.xyz, INPUT_BLENDWEIGHT.x, c[a0.x + CV_MORPH_REGIONS_BASE].xyz, r_src

mov r_world_normal, INPUT_NORMAL
;mad r_world_normal.xyz, INPUT_BLENDWEIGHT.x, c[a0.x + CV_MORPH_REGIONS_BASE].xyz, r_world_normal
;normalize_vector r_world_normal

dp4 r_world_pos.x, r_src, c[CV_MAT_PALETTE_BASE + 16*3 + 0]
dp4 r_world_pos.y, r_src, c[CV_MAT_PALETTE_BASE + 16*3 + 1]
dp4 r_world_pos.z, r_src, c[CV_MAT_PALETTE_BASE + 16*3 + 2]
mov r_world_pos.w, CV_ONE
m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM

#endfragment

;----------------------------
; Transform discharge - align vertices to look at the camera
#beginfragment transform_discharge

mov a0.x, INPUT_POSITION.y

mov r_p.xyz, c[CV_MAT_PALETTE_BASE+0+a0.x].xyz
mov r_dir, c[CV_MAT_PALETTE_BASE+1+a0.x].xyz
add r_cam_dir.xyz, r_p.xyz, -c_cam_loc_pos.xyz
                              //cross-product
mul r_right.xyz, r_dir.yzx, r_cam_dir.zxy
mad r_right.xyz, -r_cam_dir.yzx, r_dir.zxy, r_right.xyz
dp3 r_right.w, r_right, r_right
rsq r_right.w, r_right.w
mul r_right.xyz, r_right.xyz, r_right.w

mul r_add.xyz, r_right, INPUT_POSITION.x
add r_loc_pos.xyz, r_p.xyz, r_add.xyz
mov r_loc_pos.w, CV_ONE

                              //compute world position (note: do not use m4x3, nvlink allocates same src and dest regs, which causes bugs
dp4 r_world_pos.x, r_loc_pos, c_mat_frame++
dp4 r_world_pos.y, r_loc_pos, c_mat_frame++
dp4 r_world_pos.z, r_loc_pos, c_mat_frame
mov r_world_pos.w, CV_ONE

m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM

                              //compute texture coordinates and color
;mov oT0.xy, INPUT_POSITION.zw
mul oT0.xy, INPUT_TEXTURE0, CV_SHORT2FLOAT
mov r_diffuse, CV_AMBIENT
mov r_alpha.x, c[CV_MAT_PALETTE_BASE+0+a0.x].w
mul r_diffuse.w, r_alpha.x, r_diffuse.w
mov oD0, r_diffuse

#endfragment


;----------------------------
; Transform particle - special use of constant registers
#beginfragment transform_particle

                              //transform using index
;mov a0.x, v1.z
mov a0.x, INPUT_POSITION.z
mov r_pos.xy, INPUT_POSITION.xy
mov r_pos.z, CV_ZERO
mov r_pos.w, CV_ONE
;m4x3 r_world_pos.xyz, r0, c[CV_PARTICLE_MATRIX_BASE + a0.x]
dp4 r_world_pos.x, r_pos, c[CV_PARTICLE_MATRIX_BASE + 0 + a0.x]
dp4 r_world_pos.y, r_pos, c[CV_PARTICLE_MATRIX_BASE + 1 + a0.x]
dp4 r_world_pos.z, r_pos, c[CV_PARTICLE_MATRIX_BASE + 2 + a0.x]
mov r_world_pos.w, CV_ONE
m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM
                              //copy diffuse
mov oD0, c[CV_PARTICLE_MATRIX_BASE+3+a0.x]

#endfragment


;----------------------------
; Debugging.
#beginfragment test_fire

                              //get rectangle index (0 ... 3)
mov a0.x, INPUT_BLENDWEIGHT.x
mov r_uv_off, c[CV_PARTICLE_MATRIX_BASE+a0.x]
                              //compute world position
mov a0.x, INPUT_BLENDWEIGHT.y
mul r_world_pos, c[CV_PARTICLE_MATRIX_BASE+4+a0.x], r_uv_off.z
mad r_world_pos.xyz, c[CV_PARTICLE_MATRIX_BASE+5+a0.x], r_uv_off.w, r_world_pos
mul r_scale.x, v1.x, CV_SHORT2FLOAT
mad r_world_pos, r_world_pos, r_scale.x, INPUT_POSITION

m4x4 oPos, r_world_pos, CV_MAT_TRANSFORM
                              
                              //make uv coordinate (based on rectangle index and used tile)
mad oT0.xy, c[CV_PARTICLE_MATRIX_BASE+1].xy, v1.y, r_uv_off.xy

#endfragment


//----------------------------
; Debugging.
#beginfragment test

mul oPos, INPUT_POSITION, CV_MAT_TRANSFORM

#endfragment


//----------------------------
#beginfragment bump_os

; directional light
;mov oD1.xyz, c[95].xyz

#define CV_LIGHT_DIR c[95].xyz
dp3 r_ldir.x, CV_LIGHT_DIR, INPUT_TEXTURE_SPACE_S.xyz
dp3 r_ldir.y, CV_LIGHT_DIR, INPUT_TEXTURE_SPACE_T.xyz
dp3 r_ldir.z, CV_LIGHT_DIR, INPUT_TEXTURE_SPACE_SxT.xyz

                              //put to 0...1 range
mad r_ldir.xyz, -r_ldir, CV_HALF, CV_HALF

mov oD1.xyz, r_ldir.xyz

; point light
;sub r_dir.xyz, INPUT_POSITION.xyz, c[94]
;dp3 r_dir.w, r_dir, r_dir
;rsq r_dir.w, r_dir.w
;mul r_dir.xyz, r_dir.xyz, r_dir.w
;mul r_dir.xyz, r_dir.xyz, CV_HALF
;add oD1.xyz, r_dir.xyz, CV_HALF
;mul oD1.xyz, r_dir.xyz, r_dir.w

#endfragment

//----------------------------
#beginfragment bump_ws
nop
#endfragment

;--------------------------------------------------------
; UV coordinates.
;--------------------------------------------------------


;----------------------------
; Pick UV coordinate

#beginfragment pick_uv_0
mov r_uv.xy, INPUT_TEXTURE0
#endfragment

#beginfragment pick_uv_1
mov r_uv.xy, INPUT_TEXTURE1
#endfragment

#beginfragment pick_uv_2
mov r_uv.xy, INPUT_TEXTURE2
#endfragment

#beginfragment pick_uv_3
mov r_uv.xy, INPUT_TEXTURE3
#endfragment

//----------------------------
; Store UV coordinate

#beginfragment store_uv_0
mov oT0.xy, r_uv.xy
#endfragment

#beginfragment store_uv_1
mov oT1.xy, r_uv.xy
#endfragment

#beginfragment store_uv_2
mov oT2.xy, r_uv.xy
#endfragment

#beginfragment store_uv_3
mov oT3.xy, r_uv.xy
#endfragment

#beginfragment store_uv_4
mov oT4.xy, r_uv.xy
#endfragment

#beginfragment store_uv_5
mov oT5.xy, r_uv.xy
#endfragment

#beginfragment store_uv_6
mov oT6.xy, r_uv.xy
#endfragment

#beginfragment store_uv_7
mov oT7.xy, r_uv.xy
#endfragment

#beginfragment store_uvw_0
mov oT0.xyz, r_uvw.xyz
#endfragment

#beginfragment store_uvw_1
mov oT1.xyz, r_uvw.xyz
#endfragment

#beginfragment store_uvw_2
mov oT2.xyz, r_uvw.xyz
#endfragment

#beginfragment store_uvw_3
mov oT3.xyz, r_uvw.xyz
#endfragment

#beginfragment store_uvw_4
mov oT4.xyz, r_uvw.xyz
#endfragment

#beginfragment store_uvw_5
mov oT5.xyz, r_uvw.xyz
#endfragment

#beginfragment store_uvw_6
mov oT6.xyz, r_uvw.xyz
#endfragment

#beginfragment store_uvw_7
mov oT7.xyz, r_uvw.xyz
#endfragment

#beginfragment transform_uv0
mov r_uv.z, CV_ONE
dp3 oT0.x, r_uv.xyz, c_uv_transform.xyz++
dp3 oT0.y, r_uv.xyz, c_uv_transform.xyz
#endfragment

;----------------------------
; Generate uv coordinates for environment mapping - base part.
; Outputs:
;  r_r - reflected normal
;  r_r_sz - size of reflected normal
macro MAC_ENVMAP_BASE POS, NORM

sub r_dir_to_cam.xyz, %POS.xyz, c_cam_loc_pos.xyz
dp3 r_dir_to_cam.w, %NORM, r_dir_to_cam.xyz
add r_dir_to_cam.w, r_dir_to_cam.w, r_dir_to_cam.w
mad r_r, %NORM, r_dir_to_cam.w, -r_dir_to_cam
dp3 r_r_squared.x, r_r, r_r
rsq r_r_sz.x, r_r_squared.x

endm

;----------------------------
; Generate uv coordinates for environment mapping.
macro MAC_GENERATE_ENVMAP POS, NORM

MAC_ENVMAP_BASE %POS, %NORM
mul r_r_half.xy, r_r_sz.x, CV_HALF_NEGHALF
mad r_uv.xy, r_r.xz, r_r_half.xy, CV_HALF

endm


;----------------------------
; Generate uv coordinates for environment mapping in os.
#beginfragment generate_envmap_uv_os
MAC_GENERATE_ENVMAP INPUT_POSITION, INPUT_NORMAL
#endfragment


;----------------------------
; Generate uv coordinates for environment mapping in ws.
#beginfragment generate_envmap_uv_ws
MAC_GENERATE_ENVMAP r_world_pos, r_world_normal
#endfragment

;----------------------------
; Generate uv coordinates for environment mapping.
macro MAC_GENERATE_ENVMAP_CUBE POS, NORM

MAC_ENVMAP_BASE %POS, %NORM
mul r_uvw.xyz, r_r.xyz, -r_r_sz.x

endm


;----------------------------
; Generate uv coordinates for environment mapping in os.
#beginfragment generate_envmap_cube_uv_os
MAC_GENERATE_ENVMAP_CUBE INPUT_POSITION, INPUT_NORMAL
#endfragment


;----------------------------
; Generate uv coordinates for environment mapping in ws.
#beginfragment generate_envmap_cube_uv_ws
MAC_GENERATE_ENVMAP_CUBE r_world_pos, r_world_normal
#endfragment


;----------------------------
; Shift and multiply uv coordinates from stage 0.
#beginfragment shift_uv
add r_uv.xy, INPUT_TEXTURE0, c_uv_shift.xy
#endfragment


;----------------------------
; Scale current uv coordinates by constant x.
#beginfragment multiply_uv_by_xy
mul r_uv.xy, r_uv.xy, CV_UV_SHIFT_SCALE.xy
#endfragment

;----------------------------
; Scale current uv coordinates by constant y.
#beginfragment multiply_uv_by_z
mul r_uv.xy, r_uv.xy, CV_UV_SHIFT_SCALE.z
#endfragment

;----------------------------
; <backup>
;#beginfragment texture_project

; transform UV set 0 - used for projected texture
;dp4 oT0.x, INPUT_POSITION, c[CV_MAT_TEXTURE+0]
;dp4 oT0.y, INPUT_POSITION, c[CV_MAT_TEXTURE+1]

; don't project on vertices pointing away of source
; (commented out, this test already done in RenderSolidMesh)
;dp3 r_tmp.x, -INPUT_NORMAL, c[CV_MAT_TEXTURE+3]
;sge r_tmp.y, r_tmp.x, CV_ZERO

; transform 1D set 1 - used for shadow intensity and clamping
;mov oT1.y, CV_ZERO
; compute distance from shadow source
;dp4 r_tmp.z, INPUT_POSITION, c[CV_MAT_TEXTURE+2]
;add r_tmp.z, CV_ONE, -r_tmp.z
;add oT1.x, CV_ONE, -r_tmp.z

; combine intensity (r_tmp.x) with bool status (r_tmp.y)
;mul r_tmp.z, r_tmp.x, r_tmp.z
; (commented out, this test already done in RenderSolidMesh)
;mul oT1.x, r_tmp.y, r_tmp.z

;#endfragment


;----------------------------
; Shadow projection - texture UV0 computation.
#beginfragment texture_project

; transform UV set 0 - used for projected texture
dp4 oT0.x, INPUT_POSITION, c[CV_MAT_TEXTURE+0]
dp4 oT0.y, INPUT_POSITION, c[CV_MAT_TEXTURE+1]

; transform 1D set 1 - used for shadow intensity and clamping
mov oT1.y, CV_ZERO
; compute distance from shadow source
dp4 r_tmp.z, INPUT_POSITION, c[CV_MAT_TEXTURE+2]
add oT1.x, CV_ONE, -r_tmp.z

#endfragment


;----------------------------
#beginfragment make_rect_uv

;add oT0.x, CV_HALF, INPUT_POSITION.x
;sub oT0.y, CV_HALF, INPUT_POSITION.y
   ; use one opcode - although it flips texture by Y
sub oT0.xy, CV_HALF, INPUT_POSITION.xy
#endfragment


;----------------------------
; UV generation for clipping by texture.
macro MAX_TEXTURE_CLIP POS, DEST

;mov %DEST.x, CV_HALF
dp3 r_dist.x, %POS, c_txt_clip
add %DEST.x, r_dist.x, c_txt_clip.w
mov %DEST.y, CV_ZERO

endm


#beginfragment txt_clip_os_1
MAX_TEXTURE_CLIP INPUT_POSITION, oT1
#endfragment

#beginfragment txt_clip_os_2
MAX_TEXTURE_CLIP INPUT_POSITION, oT2
#endfragment

#beginfragment txt_clip_os_3
MAX_TEXTURE_CLIP INPUT_POSITION, oT3
#endfragment

#beginfragment txt_clip_ws_1
MAX_TEXTURE_CLIP r_world_pos, oT1
#endfragment

#beginfragment txt_clip_ws_2
MAX_TEXTURE_CLIP r_world_pos, oT2
#endfragment

#beginfragment txt_clip_ws_3
MAX_TEXTURE_CLIP r_world_pos, oT3
#endfragment

;--------------------------------------------------------
; Fog.
;--------------------------------------------------------


;----------------------------
; Compute simple height fog.
#beginfragment fog_simple_os

; re-calc z component of position
dp4 r_intensity.x, INPUT_POSITION, CV_MAT_TRANSFORM0_2
; cameraspace depth (z) - fog start
sub r_intensity.x, r_intensity.x, CV_FOG_START
; 1.0 - (z - fog_start) * 1/fog_range
; because 1.0 means no fog, and 0.0 means full fog
mul r_intensity.x, r_intensity.x, CV_FOG_R_RANGE
sub oFog, CV_ONE, r_intensity.x


#endfragment


;----------------------------
; Compute simple height fog.
#beginfragment fog_simple_ws

; re-calc z component of position
dp4 r_intensity.x, r_world_pos, CV_MAT_TRANSFORM0_2
; cameraspace depth (z) - fog start
sub r_intensity.x, r_intensity.x, CV_FOG_START
; 1.0 - (z - fog_start) * 1/fog_range
; because 1.0 means no fog, and 0.0 means full fog
mul r_intensity.x, r_intensity.x, CV_FOG_R_RANGE
sub oFog, CV_ONE, r_intensity.x

#endfragment


;----------------------------
; Begin with fog computation - fill with base value.
#beginfragment fog_begin
mov r_fog.x, CV_ZERO
#endfragment

;----------------------------
; Finish fog computation - output value.
#beginfragment fog_end
; make one's complement, because 1.0 means no fog, and 0.0 means full fog
sub oFog, CV_ONE, r_fog.x
#endfragment


;----------------------------
; Compute height fog. Macro, parameter POS may be
; either INPUT_POSITION (objectspace), or r_world_pos (worldspace).
macro MAC_FOG_HEIGHT POS

; re-calc z component of position
dp4 r_intensity.x, %POS, CV_MAT_TRANSFORM0_2
; cameraspace depth (z) - fog start
sub r_intensity.x, r_intensity.x, CV_FOG_START
mul r_intensity.x, r_intensity.x, CV_FOG_R_RANGE

; put into range 0.0 ... 1.0
min r_intensity.x, r_intensity.x, CV_ONE
max r_intensity.x, r_intensity.x, CV_ZERO
add r_fog.x, r_fog.x, r_intensity.x

endm


;----------------------------
; Compute vertex fog in os.
#beginfragment fog_height
MAC_FOG_HEIGHT INPUT_POSITION
#endfragment


;----------------------------
; Compute vertex fog, in ws.
#beginfragment fog_height_ws
MAC_FOG_HEIGHT r_world_pos
#endfragment


;----------------------------
; Single layered fog. Macro, parameter POS may be
; either INPUT_POSITION (objectspace), or r_world_pos (worldspace).
macro MAC_FOG_LAYERED POS

; compute distance to fog's plane                                 
dp3 r_intensity.x, c_light_layered_plane.xyz, %POS                 
add r_intensity.x, r_intensity.x, c_light_layered_plane.w++
; put into range min ... 1.0
max r_intensity.y, r_intensity.x, CV_ZERO
min r_intensity.z, r_intensity.y, c_light_layered_color_r.w++
; ignore values out of range (use cut-sphere frustrum)
sub r_dir.xyz, %POS.xyz, c_light_layered_pos.xyz
dp3 r_dist_2.x, r_dir, r_dir
slt r_intensity.w, r_dist_2.x, c_light_layered_pos.w++
mad r_fog.x, r_intensity.z, r_intensity.w, r_fog.x

endm


;----------------------------
; Compute layered fog in os.
#beginfragment fog_layered_os
MAC_FOG_LAYERED INPUT_POSITION
#endfragment


;----------------------------
; Compute layered fog in ws.
#beginfragment fog_layered_ws
MAC_FOG_LAYERED r_world_pos
#endfragment


;----------------------------
; Compute single point fog. Macro, parameter POS may be
; either INPUT_POSITION (objectspace), or r_world_pos (worldspace).
macro MAC_FOG_POINT POS
; compute dir from vertex to cone top
sub r_dir_to_cone.xyz, %POS.xyz, c_light_pfog_conetop_rcca
; compute reciprocal dir's size
dp3 r_r_dist.x, r_dir_to_cone.xyz, r_dir_to_cone.xyz
rsq r_r_dist.x, r_r_dist.x
; r_cos = r_dir_to_cone.Dot(cone_axis) / r_dir_to_cone_size
dp3 r_scaled_cos.x, r_dir_to_cone.xyz, c_light_pfog_conedir_mult.xyz
; divide by dir's size
; adjust by near range (add sin of near range)
; make one's complement (1 - r_cos)
mad r_compl_cos.x, -r_scaled_cos.x, r_r_dist.x, CV_ONE
; multiply by (1 - (1 - cos(cone angle))), make one's complement
mul r_intensity.x, r_compl_cos.x, c_light_pfog_conetop_rcca.w++
sub r_intensity.x, CV_ONE, r_intensity.x
; multiply by mult
mul r_intensity.x, r_intensity.x, c_light_pfog_conedir_mult.w++
; put into range 0.0 ... max_power
max r_intensity.x, r_intensity.x, CV_ZERO
min r_intensity, r_intensity.x, c_light_pfog_color_power.w++

add r_fog.x, r_fog.x, r_intensity.x
endm


;----------------------------
; Compute point fog, objectspace.
#beginfragment fog_point_os
MAC_FOG_POINT INPUT_POSITION
#endfragment


;----------------------------
; Compute point fog, worldspace.
#beginfragment fog_point_ws
MAC_FOG_POINT r_world_pos
#endfragment


;--------------------------------------------------------
; Lighting.
;--------------------------------------------------------


;----------------------------
; Begin with diffuse computation - fill with base color.
#beginfragment light_begin
mov r_diffuse, CV_AMBIENT
#endfragment


;----------------------------
; Finish diffuse computation - output color.
#beginfragment light_end
mov oD0, r_diffuse
#endfragment


;----------------------------
; Multiply current diffuse value by vertex alpha.
#beginfragment light_end_alpha
mov oD0.xyz, r_diffuse
mul oD0.w, r_diffuse.w, INPUT_ALPHA
#endfragment


;----------------------------
; Plain copy of diffuse color.
#beginfragment diffuse_copy
mov oD0, INPUT_DIFFUSE
#endfragment


;----------------------------
; Copy diffuse color, multiply alpha.
#beginfragment diffuse_mul_alpha
mov r_src, INPUT_DIFFUSE
mov oD0.xyz, r_src
mul oD0.w, r_src.w, INPUT_ALPHA
#endfragment


;----------------------------
; Plain alpha copy.
#beginfragment copy_alpha
mov oD0.xyz, CV_ONE
mov oD0.w, INPUT_ALPHA
#endfragment


;----------------------------
; Begin with specular computation - fill with zero color.
#beginfragment specular_begin
mov r_specular, CV_ZERO
#endfragment


;----------------------------
; Finish specular computation - output color.
#beginfragment specular_end
mov oD1, r_specular
#endfragment


;----------------------------
; Single directional light.
macro MAC_LIGHT_DIRECTIONAL NORM

; generate intensity based on angle
dp3 r_intensity.x, c_light_dir_normal++, -%NORM
; put into range 0.0 ... 1.0, setup zero alpha multiplier
max r_intensity.x, r_intensity.x, CV_ZERO
min r_intensity, r_intensity.x, CV_1110
; get final color, using diffuse color
mad r_diffuse, r_intensity, c_light_dir_diffuse++, r_diffuse

endm


;----------------------------
; Single directional light in os.
#beginfragment light_directional_os
MAC_LIGHT_DIRECTIONAL INPUT_NORMAL
#endfragment


;----------------------------
; Single directional light in sw.
#beginfragment light_directional_ws
MAC_LIGHT_DIRECTIONAL r_world_normal
#endfragment


;----------------------------
; Single point light.
macro MAC_LIGHT_POINT POS, NORM

; get vector from vertex to light
sub r_dir_to_light.xyz, c_light_point_pos_fr.xyz, %POS.xyz
; get reciprocal of distance
dp3 r_dir_to_light.w, r_dir_to_light, r_dir_to_light
rsq r_dir_to_light.w, r_dir_to_light.w
; get distance
rcp r_dist.x, r_dir_to_light.w
; compute attenuation
sub r_intensity.x, c_light_point_pos_fr.w++, r_dist.x
max r_intensity.x, r_intensity.x, CV_ZERO
mul r_intensity.x, r_intensity.x, c_light_point_diffuse_rrd.w
; clamp product
;max r_dir_to_light.xyz, r_dir_to_light.xyz, CV_ZERO
; normalize direction vector
mul r_dir_to_light.xyz, r_dir_to_light.xyz, r_dir_to_light.w
; generate intensity based on angle
dp3 r_intensity.y, %NORM, r_dir_to_light
; combine angle with attenuation
mul r_intensity.x, r_intensity.x, r_intensity.y
; put into range 0.0 ... 1.0, setup zero alpha multiplier
max r_intensity.x, r_intensity.x, CV_ZERO
min r_intensity, r_intensity.x, CV_1110
; get final color, using diffuse color
mad r_diffuse, r_intensity, c_light_point_diffuse_rrd++, r_diffuse

endm


;----------------------------
; Single point light in os.
#beginfragment light_point_os
MAC_LIGHT_POINT INPUT_POSITION, INPUT_NORMAL
#endfragment


;----------------------------
; Single point light in ws.
#beginfragment light_point_ws
MAC_LIGHT_POINT r_world_pos, r_world_normal
#endfragment


;----------------------------
; Single spot light.
macro MAC_LIGHT_SPOT POS, NORM

; get vector from vertex to light
sub r_dir_to_light.xyz, c_light_point_pos_fr.xyz, %POS.xyz
; get reciprocal of distance
dp3 r_dir_to_light.w, r_dir_to_light, r_dir_to_light
rsq r_dir_to_light.w, r_dir_to_light.w
; get distance
rcp r_dist.x, r_dir_to_light.w
; normalize direction vector
mul r_dir_to_light.xyz, r_dir_to_light.xyz, r_dir_to_light.w
; generate intensity based on angle
dp3 r_intensity.y, %NORM, r_dir_to_light

; compute attenuation
sub r_intensity.x, c_light_point_pos_fr.w++, r_dist.x
max r_intensity.xy, r_intensity.xy, CV_ZERO
mul r_intensity.x, r_intensity.x, c_light_point_diffuse_rrd.w

; compute intensity based on position inside of cone
; algo: we compute dot-product of light dir vector and point's dir to light,
;        multiplied by such values, that zero is produced at outer cone, and one at inner cone
;        then we clamp to 0 ... 1, and we have cone's intensity affected by both inner and outer cone
;     1. get cosine in range +n ... -n (it's scaled by multiplier encoded in light's dir vector)
;     2. add base value, so that zero appears at outer cone, and one at inner cone
;     3. clamp to 0 ... 1 (note that clamping to 0 is done few instructions later)
dp3 r_intensity.z, r_dir_to_light, -c_light_spot_dir_cos.xyz
add r_intensity.z, r_intensity.z, c_light_spot_dir_cos.w++
min r_intensity.z, r_intensity.z, CV_ONE

; combine angle with attenuation
mul r_intensity.x, r_intensity.x, r_intensity.y
; put into range 0.0 ... 1.0, setup zero alpha multiplier
max r_intensity.xz, r_intensity.xz, CV_ZERO
; combine result with cone
mul r_intensity.x, r_intensity.z, r_intensity.x
;mov r_intensity.x, r_intensity.z

min r_intensity, r_intensity.x, CV_1110

; get final color, using diffuse color
mad r_diffuse, r_intensity, c_light_point_diffuse_rrd++, r_diffuse

endm


;----------------------------
; Single spot light in os.
#beginfragment light_spot_os
MAC_LIGHT_SPOT INPUT_POSITION, INPUT_NORMAL
#endfragment


;----------------------------
; Single spot light in ws.
#beginfragment light_spot_ws
MAC_LIGHT_SPOT r_world_pos, r_world_normal
#endfragment


;----------------------------
; Single point ambient light. Macro, parameter POS may be
; either INPUT_POSITION (objectspace), or r_world_pos (worldspace).
macro MAC_LIGHT_POINT_AMBIENT POS

; get vector from vertex to light
sub r_dir_to_light, c_light_point_pos_fr, %POS
; get reciprocal of distance
dp3 r_dir_to_light.w, r_dir_to_light, r_dir_to_light
rsq r_dir_to_light.w, r_dir_to_light.w
; get distance
rcp r_dist.x, r_dir_to_light.w
; compute attenuation
sub r_intensity.x, c_light_point_pos_fr.w++, r_dist.x
mul r_intensity.x, r_intensity.x, c_light_point_diffuse_rrd.w
; put into range 0.0 ... 1.0
min r_intensity.x, r_intensity.x, CV_ONE
max r_intensity.x, r_intensity.x, CV_ZERO
; get final color, using diffuse color
mad r_diffuse, r_intensity.x, c_light_point_diffuse_rrd++, r_diffuse

endm


;----------------------------
; Single point ambient light in os.
#beginfragment light_point_ambient_os
MAC_LIGHT_POINT_AMBIENT INPUT_POSITION
#endfragment

;----------------------------
; Single point ambient light in ws.
#beginfragment light_point_ambient_ws
MAC_LIGHT_POINT_AMBIENT r_world_pos
#endfragment


;----------------------------
; Simple directional light (dp3) for debugging and editing purposes.
#beginfragment simple_dir_light
dp3 r_tmp.x, INPUT_NORMAL, c[CV_MAT_PALETTE_BASE]
mad oD0.xyz, r_tmp.x, CV_HALF, CV_HALF
;mov oD0.w, CV_ONE
#endfragment

;----------------------------
; Projection of texkill plane.
#beginfragment texkill_project_os

dp3 r_dist.x, CV_TEXKILL_PLANE.xyz, INPUT_POSITION.xyz
add r_uvw.xyz, r_dist.x, CV_TEXKILL_PLANE.w

#endfragment

;----------------------------
#beginfragment texkill_project_ws

dp3 r_dist.x, CV_TEXKILL_PLANE.xyz, r_world_pos.xyz
add r_uvw.xyz, r_dist.x, CV_TEXKILL_PLANE.w

#endfragment

;----------------------------

