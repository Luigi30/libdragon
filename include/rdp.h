/**
 * @file rdp.h
 * @brief Hardware Display Interface
 * @ingroup rdp
 */
#ifndef __LIBDRAGON_RDP_H
#define __LIBDRAGON_RDP_H

#include "display.h"
#include "graphics.h"
#include "fixed.h"

#define MMIO8(x)  (*(volatile uint8_t *)(x))
#define MMIO16(x) (*(volatile uint16_t *)(x))
#define MMIO32(x) (*(volatile uint32_t *)(x))

/*
 * Maximum z-buffer value, used to initialize the z-buffer.
 * Note : this number is NOT the viewport z-scale constant.
 * See the comment next to G_MAXZ for more info.
 */
#define G_MAXFBZ                0x3fff  /* 3b exp, 11b mantissa */

#define GPACK_RGBA5551(r, g, b, a)      ((((r)<<8) & 0xf800) |          \
                                         (((g)<<3) & 0x7c0) |           \
                                         (((b)>>2) & 0x3e) | ((a) & 0x1))
#define GPACK_ZDZ(z, dz)                ((z) << 2 | (dz))

typedef enum {
    RDP_IMAGE_RGBA,
    RDP_IMAGE_YUV,
    RDP_IMAGE_COLORINDEX,
    RDP_IMAGE_IA,
    RDP_IMAGE_I
} RDP_IMAGE_DATA_FORMAT;

typedef enum {
    RDP_PIXEL_4BIT,
    RDP_PIXEL_8BIT,
    RDP_PIXEL_16BIT,
    RDP_PIXEL_32BIT
} RDP_PIXEL_WIDTH;

/**
 * @addtogroup rdp
 * @{
 */

typedef enum {
    TEXSLOT_0,
    TEXSLOT_1,
    TEXSLOT_2,
    TEXSLOT_3,
    TEXSLOT_4,
    TEXSLOT_5,
    TEXSLOT_6,
    TEXSLOT_7
} texslot_t;

typedef struct
{
    uint32_t hi;
    uint32_t lo;
} words64_t;

typedef union
{
    words64_t words;
    uint64_t command;
} display_list_t;

/**
 * @brief Mirror settings for textures
 */
typedef enum
{
    /** @brief Disable texture mirroring */
    MIRROR_DISABLED,
    /** @brief Enable texture mirroring */
    MIRROR_ENABLED
} mirror_t;

/**
 * @brief RDP sync operations
 */
typedef enum
{
    /** @brief Wait for any operation to complete before causing a DP interrupt */
    SYNC_FULL,
    /** @brief Sync the RDP pipeline */
    SYNC_PIPE,
    /** @brief Block until all texture load operations are complete */
    SYNC_LOAD,
    /** @brief Block until all tile operations are complete */
    SYNC_TILE
} sync_t;

/**
 * @brief Caching strategy for loaded textures
 */
typedef enum
{
    /** @brief Textures are assumed to be pre-flushed */
    FLUSH_STRATEGY_NONE,
    /** @brief Cache will be flushed on all incoming textures */
    FLUSH_STRATEGY_AUTOMATIC
} flush_t;

/**
 * @brief Location of the display list - RDRAM or DMEM?
 */
typedef enum
{
    /** @brief Display list is in RDRAM. */
    DISPLAY_LIST_RDRAM,
    /** @brief Display list is in DMEM. */
    DISPLAY_LIST_DMEM
} display_list_location_t;

// Color Combiner modes.
// C0 = cycle 0; C1 = cycle 1
// SUBA, SUBB, MUL, ADD
// (A-B)*C+D

// Cycle 0
#define CC_C0_RGB_SUBA_COMBINED_COLOR   (0 << 52)
#define CC_C0_RGB_SUBA_TEXEL0_COLOR     (1 << 52)
#define CC_C0_RGB_SUBA_TEXEL1_COLOR     (2 << 52)
#define CC_C0_RGB_SUBA_PRIM_COLOR       (3 << 52)
#define CC_C0_RGB_SUBA_SHADE_COLOR      (4 << 52)
#define CC_C0_RGB_SUBA_ENV_COLOR        (5 << 52)
#define CC_C0_RGB_RGB_SUBA_ONE_COLOR    (6 << 52)
#define CC_C0_RGB_SUBA_NOISE_COLOR      (7 << 52)
#define CC_C0_RGB_SUBA_ZERO_COLOR       (8 << 52)

#define CC_C0_RGB_SUBB_COMBINED_COLOR   (0 << 28)
#define CC_C0_RGB_SUBB_TEXEL0_COLOR     (1 << 28)
#define CC_C0_RGB_SUBB_TEXEL1_COLOR     (2 << 28)
#define CC_C0_RGB_SUBB_PRIM_COLOR       (3 << 28)
#define CC_C0_RGB_SUBB_SHADE_COLOR      (4 << 28)
#define CC_C0_RGB_SUBB_ENV_COLOR        (5 << 28)
#define CC_C0_RGB_SUBB_ILLEGAL_COLOR    (6 << 28)
#define CC_C0_RGB_SUBB_K4_COLOR         (7 << 28)
#define CC_C0_RGB_SUBB_ZERO_COLOR       (8 << 28)

#define CC_C0_RGB_MUL_COMBINED_COLOR       (0 << 47)
#define CC_C0_RGB_MUL_TEXEL0_COLOR         (1 << 47)
#define CC_C0_RGB_MUL_TEXEL1_COLOR         (2 << 47)
#define CC_C0_RGB_MUL_PRIM_COLOR           (3 << 47)
#define CC_C0_RGB_MUL_SHADE_COLOR          (4 << 47)
#define CC_C0_RGB_MUL_ENV_COLOR            (5 << 47)
#define CC_C0_RGB_MUL_KEY_SCALE            (6 << 47)
#define CC_C0_RGB_MUL_COMBINED_ALPHA       (7 << 47)
#define CC_C0_RGB_MUL_TEXEL0_ALPHA         (8 << 47)
#define CC_C0_RGB_MUL_TEXEL1_ALPHA         (9 << 47)
#define CC_C0_RGB_MUL_PRIM_ALPHA           (10 << 47)
#define CC_C0_RGB_MUL_SHADE_ALPHA          (11 << 47)
#define CC_C0_RGB_MUL_ENV_ALPHA            (12 << 47)
#define CC_C0_RGB_MUL_LOD_FRACTION         (13 << 47)
#define CC_C0_RGB_MUL_PRIM_LOD_FRACTION    (14 << 47)
#define CC_C0_RGB_MUL_K5_COLOR             (15 << 47)
#define CC_C0_RGB_MUL_ZERO_COLOR           (16 << 47)

#define CC_C0_RGB_ADD_COMBINED_COLOR       (0 << 15) 
#define CC_C0_RGB_ADD_TEXEL0_COLOR         (1 << 15) 
#define CC_C0_RGB_ADD_TEXEL1_COLOR         (2 << 15) 
#define CC_C0_RGB_ADD_PRIM_COLOR           (3 << 15) 
#define CC_C0_RGB_ADD_SHADE_COLOR          (4 << 15) 
#define CC_C0_RGB_ADD_ENV_COLOR            (5 << 15) 
#define CC_C0_RGB_ADD_ONE_COLOR            (6 << 15) 
#define CC_C0_RGB_ADD_ZERO_COLOR           (7 << 15) 

// Cycle 1
#define CC_C1_RGB_SUBA_COMBINED_COLOR   (0 << 37)
#define CC_C1_RGB_SUBA_TEXEL0_COLOR     (1 << 37)
#define CC_C1_RGB_SUBA_TEXEL1_COLOR     (2 << 37)
#define CC_C1_RGB_SUBA_PRIM_COLOR       (3 << 37)
#define CC_C1_RGB_SUBA_SHADE_COLOR      (4 << 37)
#define CC_C1_RGB_SUBA_ENV_COLOR        (5 << 37)
#define CC_C1_RGB_SUBA_ONE_COLOR        (6 << 37)
#define CC_C1_RGB_SUBA_NOISE_COLOR      (7 << 37)
#define CC_C1_RGB_SUBA_ZERO_COLOR       (8 << 37)

#define CC_C1_RGB_SUBB_COMBINED_COLOR   (0 << 24)
#define CC_C1_RGB_SUBB_TEXEL0_COLOR     (1 << 24)
#define CC_C1_RGB_SUBB_TEXEL1_COLOR     (2 << 24)
#define CC_C1_RGB_SUBB_PRIM_COLOR       (3 << 24)
#define CC_C1_RGB_SUBB_SHADE_COLOR      (4 << 24)
#define CC_C1_RGB_SUBB_ENV_COLOR        (5 << 24)
#define CC_C1_RGB_SUBB_ILLEGAL_COLOR    (6 << 24)
#define CC_C1_RGB_SUBB_K4_COLOR         (7 << 24)
#define CC_C1_RGB_SUBB_ZERO_COLOR       (8 << 24)

#define CC_C1_RGB_MUL_COMBINED_COLOR        (0 << 32)
#define CC_C1_RGB_MUL_TEXEL0_COLOR          (1 << 32)
#define CC_C1_RGB_MUL_TEXEL1_COLOR          (2 << 32)
#define CC_C1_RGB_MUL_PRIM_COLOR            (3 << 32)
#define CC_C1_RGB_MUL_SHADE_COLOR           (4 << 32)
#define CC_C1_RGB_MUL_ENV_COLOR             (5 << 32)
#define CC_C1_RGB_MUL_KEY_SCALE             (6 << 32)
#define CC_C1_RGB_MUL_COMBINED_ALPHA        (7 << 32)
#define CC_C1_RGB_MUL_TEXEL0_ALPHA          (8 << 32)
#define CC_C1_RGB_MUL_TEXEL1_ALPHA          (9 << 32)
#define CC_C1_RGB_MUL_PRIM_ALPHA            (10 << 32)
#define CC_C1_RGB_MUL_SHADE_ALPHA           (11 << 32)
#define CC_C1_RGB_MUL_ENV_ALPHA             (12 << 32)
#define CC_C1_RGB_MUL_LOD_FRACTION          (13 << 32)
#define CC_C1_RGB_MUL_PRIM_LOD_FRACTION     (14 << 32)
#define CC_C1_RGB_MUL_K5_COLOR              (15 << 32)
#define CC_C1_RGB_MUL_ZERO_COLOR            (16 << 32)

#define CC_C1_RGB_ADD_COMBINED_COLOR        (0 << 6) 
#define CC_C1_RGB_ADD_TEXEL0_COLOR          (1 << 6) 
#define CC_C1_RGB_ADD_TEXEL1_COLOR          (2 << 6) 
#define CC_C1_RGB_ADD_PRIM_COLOR            (3 << 6) 
#define CC_C1_RGB_ADD_SHADE_COLOR           (4 << 6) 
#define CC_C1_RGB_ADD_ENV_COLOR             (5 << 6) 
#define CC_C1_RGB_ADD_ONE_COLOR             (6 << 6) 
#define CC_C1_RGB_ADD_ZERO_COLOR            (7 << 6) 

// Alpha Combine
// Cycle 0
#define CC_C0_ALPHA_MUL_LODFRAC         (0 << 41)
#define CC_C0_ALPHA_MUL_TEXEL0          (1 << 41)
#define CC_C0_ALPHA_MUL_TEXEL1          (2 << 41)
#define CC_C0_ALPHA_MUL_PRIM            (3 << 41)
#define CC_C0_ALPHA_MUL_SHADE           (4 << 41)
#define CC_C0_ALPHA_MUL_ENV             (5 << 41)
#define CC_C0_ALPHA_MUL_PRIMLODFRAC     (6 << 41)
#define CC_C0_ALPHA_MUL_ZERO            (7 << 41)

#define CC_C0_ALPHA_ADD_COMBINED        (0 << 9)
#define CC_C0_ALPHA_ADD_TEXEL0          (1 << 9)
#define CC_C0_ALPHA_ADD_TEXEL1          (2 << 9)
#define CC_C0_ALPHA_ADD_PRIM            (3 << 9)
#define CC_C0_ALPHA_ADD_SHADE           (4 << 9)
#define CC_C0_ALPHA_ADD_ENV             (5 << 9)
#define CC_C0_ALPHA_ADD_ONE             (6 << 9)
#define CC_C0_ALPHA_ADD_ZERO            (7 << 9)

//Cycle 1
#define CC_C1_ALPHA_MUL_LODFRAC         (0 << 18)
#define CC_C1_ALPHA_MUL_TEXEL0          (1 << 18)
#define CC_C1_ALPHA_MUL_TEXEL1          (2 << 18)
#define CC_C1_ALPHA_MUL_PRIM            (3 << 18)
#define CC_C1_ALPHA_MUL_SHADE           (4 << 18)
#define CC_C1_ALPHA_MUL_ENV             (5 << 18)
#define CC_C1_ALPHA_MUL_PRIMLODFRAC     (6 << 18)
#define CC_C1_ALPHA_MUL_ZERO            (7 << 18)

#define CC_C1_ALPHA_ADD_COMBINED        (0 << 0)
#define CC_C1_ALPHA_ADD_TEXEL0          (1 << 0)
#define CC_C1_ALPHA_ADD_TEXEL1          (2 << 0)
#define CC_C1_ALPHA_ADD_PRIM            (3 << 0)
#define CC_C1_ALPHA_ADD_SHADE           (4 << 0)
#define CC_C1_ALPHA_ADD_ENV             (5 << 0)
#define CC_C1_ALPHA_ADD_ONE             (6 << 0)
#define CC_C1_ALPHA_ADD_ZERO            (7 << 0)


// Set Other Modes
#define MODE_ATOMIC_PRIM                (1 << 55)

#define MODE_CYCLE_TYPE_1CYCLE          (0 << 52)
#define MODE_CYCLE_TYPE_2CYCLE          (1 << 52)
#define MODE_CYCLE_TYPE_COPY            (2 << 52)
#define MODE_CYCLE_TYPE_FILL            (3 << 52)

#define MODE_PERSP_TEX_EN               (1 << 51)
#define MODE_DETAIL_TEX_EN              (1 << 50)
#define MODE_SHARPEN_TEX_EN             (1 << 49)
#define MODE_TEX_LOD_EN                 (1 << 48)
#define MODE_EN_TLUT                    (1 << 47)
#define MODE_TLUT_TYPE                  (1 << 46)
#define MODE_SAMPLE_TYPE                (1 << 45)
#define MODE_MID_TEXEL                  (1 << 44)
#define MODE_BI_LERP_0                  (1 << 43)
#define MODE_BI_LERP_1                  (1 << 42)
#define MODE_CONVERT_ONE                (1 << 41)
#define MODE_KEY_EN                     (1 << 40)

#define MODE_RGB_DITHER_SEL_MAGIC       (0 << 38)
#define MODE_RGB_DITHER_SEL_BAYER       (1 << 38)
#define MODE_RGB_DITHER_SEL_NOISE       (2 << 38)
#define MODE_RGB_DITHER_SEL_NONE        (3 << 38)

#define MODE_ALPHA_DITHER_SEL_PATTERN   (0 << 36)
#define MODE_ALPHA_DITHER_SEL_NOTPATTERN (1 << 36)
#define MODE_ALPHA_DITHER_SEL_NOISE     (2 << 36)
#define MODE_ALPHA_DITHER_SEL_NONE      (3 << 36)

// double-check the blend functions
#define MODE_BLEND_M1A_C0_PIXEL         (0 << 30)
#define MODE_BLEND_M1A_C0_MEMORY        (1 << 30)
#define MODE_BLEND_M1A_C0_BLEND         (2 << 30)
#define MODE_BLEND_M1A_C0_FOG           (3 << 30)

#define MODE_BLEND_M1A_C1_PIXEL         (0 << 28)
#define MODE_BLEND_M1A_C1_MEMORY        (1 << 28)
#define MODE_BLEND_M1A_C1_BLEND         (2 << 28)
#define MODE_BLEND_M1A_C1_FOG           (3 << 28)

#define MODE_BLEND_M1B_C0_PIXEL         (0 << 26)
#define MODE_BLEND_M1B_C0_FOG           (1 << 26)
#define MODE_BLEND_M1B_C0_SHADE         (2 << 26)
#define MODE_BLEND_M1B_C0_ZERO          (3 << 26)

#define MODE_BLEND_M1B_C1_PIXEL         (0 << 24)
#define MODE_BLEND_M1B_C1_FOG           (1 << 24)
#define MODE_BLEND_M1B_C1_SHADE         (2 << 24)
#define MODE_BLEND_M1B_C1_ZERO          (3 << 24)

#define MODE_BLEND_M2A_C0_PIXEL         (0 << 22)
#define MODE_BLEND_M2A_C0_MEMORY        (1 << 22)
#define MODE_BLEND_M2A_C0_BLEND         (2 << 22)
#define MODE_BLEND_M2A_C0_FOG           (3 << 22)

#define MODE_BLEND_M2A_C1_PIXEL         (0 << 20)
#define MODE_BLEND_M2A_C1_MEMORY        (1 << 20)
#define MODE_BLEND_M2A_C1_BLEND         (2 << 20)
#define MODE_BLEND_M2A_C1_FOG           (3 << 20)

#define MODE_BLEND_M2B_C0_INVPIXEL      (0 << 18)
#define MODE_BLEND_M2B_C0_MEMORY        (1 << 18)
#define MODE_BLEND_M2B_C0_ONE           (2 << 18)
#define MODE_BLEND_M2B_C0_ZERO          (3 << 18)

#define MODE_BLEND_M2B_C1_INVPIXEL      (0 << 16)
#define MODE_BLEND_M2B_C1_MEMORY        (1 << 16)
#define MODE_BLEND_M2B_C1_ONE           (2 << 16)
#define MODE_BLEND_M2B_C1_ZERO          (3 << 16)

#define MODE_FORCE_BLEND                (1 << 14)
#define MODE_ALPHA_CVG_SELECT           (1 << 13)
#define MODE_CVG_TIMES_ALPHA            (1 << 12)

#define MODE_Z_MODE_OPAQUE              (0 << 10)
#define MODE_Z_MODE_INTERPENETRATING    (1 << 10)
#define MODE_Z_MODE_TRANSPARENT         (2 << 10)
#define MODE_Z_MODE_DECAL               (3 << 10)

#define MODE_CVG_DEST_CLAMP             (0 << 8)
#define MODE_CVG_DEST_WRAP              (1 << 8)
#define MODE_CVG_DEST_ZAP               (2 << 8)
#define MODE_CVG_DEST_SAVE              (3 << 8)

#define MODE_COLOR_ON_CVG               (1 << 7)
#define MODE_IMAGE_READ_EN              (1 << 6)
#define MODE_Z_UPDATE_EN                (1 << 5)
#define MODE_Z_COMPARE_EN               (1 << 4)
#define MODE_ANTIALIAS_EN               (1 << 3)
#define MODE_Z_SOURCE_SEL               (1 << 2)
#define MODE_DITHER_ALPHA_EN            (1 << 1)
#define MODE_ALPHA_COMPARE_EN           (1 << 0)

/* compatibility with N64 libs */
#define	G_BL_CLR_IN	    0
#define	G_BL_CLR_MEM	1
#define	G_BL_CLR_BL	    2
#define	G_BL_CLR_FOG	3
#define	G_BL_1MA	    0
#define	G_BL_A_MEM	    1
#define	G_BL_A_IN	    0
#define	G_BL_A_FOG	    1
#define	G_BL_A_SHADE	2
#define	G_BL_1		    2
#define	G_BL_0		    3

#define	GBL_c1(m1a, m1b, m2a, m2b)	\
	(m1a) << 30 | (m1b) << 26 | (m2a) << 22 | (m2b) << 18
#define	GBL_c2(m1a, m1b, m2a, m2b)	\
	(m1a) << 28 | (m1b) << 24 | (m2a) << 20 | (m2b) << 16

#define	RM_AA_ZB_OPA_SURF(clk)					        \
	GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#ifdef __cplusplus
extern "C" {
#endif

void rdp_init( void );
bool rdp_attach_display( display_list_t **list, display_context_t disp );
void rdp_detach_display( display_list_t **list );
void rdp_sync( display_list_t **list, sync_t sync );
void rdp_set_clipping( display_list_t **list, uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by );
void rdp_set_default_clipping( display_list_t **list );
void rdp_set_fill_mode( display_list_t **list );
void rdp_set_1cycle_mode( display_list_t **list );
void rdp_enable_blend_fill( display_list_t **list );
void rdp_enable_texture_copy( display_list_t **list );
uint32_t rdp_load_texture( display_list_t **list, texslot_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite );
uint32_t rdp_load_texture_stride( display_list_t **list, texslot_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int offset );
void rdp_draw_textured_rectangle( display_list_t **list, texslot_t texslot, int tx, int ty, int bx, int by );
void rdp_draw_textured_rectangle_scaled( display_list_t **list, texslot_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale, int s_ul, int t_ul );
void rdp_draw_sprite( display_list_t **list, texslot_t texslot, int x, int y );
void rdp_draw_sprite_scaled( display_list_t **list, texslot_t texslot, int x, int y, double x_scale, double y_scale );
void rdp_set_primitive_color( display_list_t **list, uint32_t color );
void rdp_set_blend_color( display_list_t **list, uint32_t color );
void rdp_set_env_color( display_list_t **list, uint32_t color );
void rdp_draw_filled_rectangle( display_list_t **list, int tx, int ty, int bx, int by );
void rdp_draw_filled_triangle( display_list_t **list, float x1, float y1, float x2, float y2, float x3, float y3 );
void rdp_draw_filled_triangle_fixed( display_list_t **list, Fixed x1, Fixed y1, Fixed x2, Fixed y2, Fixed x3, Fixed y3 );
void rdp_set_texture_flush( flush_t flush );
void rdp_close( void );
void rdp_set_combine_mode( display_list_t **list, uint64_t combine_mode );
void rdp_set_other_modes( display_list_t **list, uint64_t mode_bits );
void rdp_set_fill_color( display_list_t **list, uint32_t color );
void rdp_set_color_image( display_list_t **list, RDP_IMAGE_DATA_FORMAT format, RDP_PIXEL_WIDTH pixelwidth, uint16_t imagewidth, uint16_t *buffer );
void rdp_set_z_image( display_list_t **list, uint16_t *buffer );

void rdp_load_texture_test( display_list_t **list, texslot_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int sl, int tl, int sh, int th );

void rdp_end_display_list( display_list_t **list );
void rdp_execute_display_list( display_list_t *list, int display_list_length, display_list_location_t location );

#ifdef __cplusplus
}
#endif

#endif
