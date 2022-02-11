

// Zero. half and one
#define CV_ZERO c0.x
#define CV_HALF c0.y
#define CV_ONE  c0.z
//#define CV_TWO  c0.w
#define CV_SHORT2FLOAT  c0.w  //1/256 - for conversion of short uv values to float uvs in range +-128

#define CV_HALF_NEGHALF c0.yw    //special value: [.5f, -.5f], used with env mapping

// vector(1, 1, 1, 0)
#define CV_1110 c0.zzzx

// vector(0, 0, 0, 1)
#define CV_0001 c0.xxxz

// Transposed transformation matrix,
// usually it is world*view*projection,
// but some algos may use different
#define CV_MAT_TRANSFORM c1
#define CV_MAT_TRANSFORM0_0 c1
#define CV_MAT_TRANSFORM0_1 c2
#define CV_MAT_TRANSFORM0_2 c3
#define CV_MAT_TRANSFORM0_3 c4

// Fog parameters
#define CV_FOG_START c5.x
#define CV_FOG_END c5.y
#define CV_FOG_R_RANGE c5.z
                              //emmisive + ambient
#define CV_AMBIENT c6

// value used for debugging
#define CV_DEBUG c7
#define CV_TEXKILL_PLANE c7

// base for matrix palette skinning (3 slots per joint)
#define CV_MAT_PALETTE_BASE 9

// base for procedural morphing (1 slot per region)
#define CV_MORPH_REGIONS_BASE 9

// matrix for texture transformation (shadow projection)
#define CV_MAT_TEXTURE 9

// uv shifting constants
#define CV_UV_SHIFT_SCALE c8

// particle rendering
#define CV_PARTICLE_MATRIX_BASE 8

// maaping of sources to input registers
#define INPUT_POSITION v0
#define INPUT_TEXTURE0 v1.xy
#define INPUT_TEXTURE1 v1.zw
#define INPUT_TEXTURE2 v3.xy  
#define INPUT_TEXTURE3 v3.zw  
#define INPUT_NORMAL v2
#define INPUT_DIFFUSE v2
#define INPUT_WEIGHT v3
#define INPUT_ALPHA v4.x
#define INPUT_BLENDWEIGHT v4

#define INPUT_TEXTURE_SPACE_S v5
#define INPUT_TEXTURE_SPACE_T v6
#define INPUT_TEXTURE_SPACE_SxT v7

// Matix palette blending uses INPUT_INDICES and following values:
// x = weight, y = index0 [, z = index1 [, w = index2] ]

;----------------------------
