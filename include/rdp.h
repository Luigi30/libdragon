/**
 * @file rdp.h
 * @brief Hardware Display Interface
 * @ingroup rdp
 */
#ifndef __LIBDRAGON_RDP_H
#define __LIBDRAGON_RDP_H

#include "display.h"
#include "graphics.h"

/**
 * @addtogroup rdp
 * @{
 */

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

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

void rdp_init( void );
bool rdp_attach_display( display_list_t **list, display_context_t disp );
void rdp_detach_display( display_list_t **list );
void rdp_sync( display_list_t **list, sync_t sync );
void rdp_set_clipping( display_list_t **list, uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by );
void rdp_set_default_clipping( display_list_t **list );
void rdp_enable_primitive_fill( display_list_t **list );
void rdp_enable_blend_fill( display_list_t **list );
void rdp_enable_texture_copy( display_list_t **list );
uint32_t rdp_load_texture( display_list_t **list, uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite );
uint32_t rdp_load_texture_stride( display_list_t **list, uint32_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int offset );
void rdp_draw_textured_rectangle( uint32_t texslot, int tx, int ty, int bx, int by );
void rdp_draw_textured_rectangle_scaled( uint32_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale );
void rdp_draw_sprite( uint32_t texslot, int x, int y );
void rdp_draw_sprite_scaled( uint32_t texslot, int x, int y, double x_scale, double y_scale );
void rdp_set_primitive_color( display_list_t **list, uint32_t color );
void rdp_set_blend_color( display_list_t **list, uint32_t color );
void rdp_draw_filled_rectangle( display_list_t **list, int tx, int ty, int bx, int by );
void rdp_draw_filled_triangle( display_list_t **list, float x1, float y1, float x2, float y2, float x3, float y3 );
void rdp_set_texture_flush( flush_t flush );
void rdp_close( void );

void rdp_end_display_list( display_list_t **list );
void rdp_execute_display_list( display_list_t *list, int display_list_length  );

#ifdef __cplusplus
}
#endif

#endif
