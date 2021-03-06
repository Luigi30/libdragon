/**
 * @file rdp.c
 * @brief Hardware Display Interface
 * @ingroup rdp
 */
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "libdragon.h"

/**
 * @defgroup rdp Hardware Display Interface
 * @ingroup display
 * @brief Interface to the hardware sprite/triangle rasterizer (RDP).
 *
 * The hardware display interface sets up and talks with the RDP in order to render
 * hardware sprites, triangles and rectangles.  The RDP is a very low level rasterizer
 * and needs data in a very specific format.  The hardware display interface handles
 * this by building commands to be sent to the RDP.
 *
 * Before attempting to draw anything using the RDP, the hardware display interface
 * should be initialized with #rdp_init.  After the RDP is no longer needed, be sure
 * to free all resources using #rdp_close.
 *
 * Code wishing to use the hardware rasterizer should first acquire a display context
 * using #display_lock.  Once a display context has been acquired, the RDP can be
 * attached to the display context with #rdp_attach_display.  Once the display has been
 * attached, the RDP can be used to draw sprites, rectangles and textured/untextured
 * triangles to the display context.  Note that some functions require additional setup,
 * so read the descriptions for each function before use.  After code has finished
 * rendering hardware assisted graphics to the display context, the RDP can be detached
 * from the context using #rdp_detach_display.  After calling thie function, it is safe
 * to immediately display the rendered graphics to the screen using #display_show, or
 * additional software graphics manipulation can take place using functions from the
 * @ref graphics.
 *
 * Careful use of the #rdp_sync operation is required for proper rasterization.  Before
 * performing settings changes such as clipping changes or setting up texture or solid
 * fill modes, code should perform a #SYNC_PIPE.  A #SYNC_PIPE should be performed again
 * before any new texture load.  This is to ensure that the last texture operation is
 * completed before attempting to change texture memory.  Careful execution of texture
 * operations can allow code to skip some sync operations.  Be careful with excessive
 * sync operations as it can stall the pipeline and cause triangles/rectangles to be
 * drawn on the next display context instead of the current.
 *
 * #rdp_detach_display will automatically perform a #SYNC_FULL to ensure that everything
 * has been completed in the RDP.  This call generates an interrupt when complete which
 * signals the main thread that it is safe to detach.  Consequently, interrupts must be
 * enabled for proper operation.  This also means that code should under normal circumstances
 * never use #SYNC_FULL.
 * @{
 */

/**
 * @brief Grab the texture buffer given a display context
 *
 * @param[in] x
 *            The display context returned from #display_lock
 *
 * @return A pointer to the drawing surface for that display context.
 */
#define __get_buffer( x ) __safe_buffer[(x)-1]

/** @brief Size of the internal ringbuffer that holds pending RDP commands */
#define RINGBUFFER_SIZE  4096

/** 
 * @brief Size of the slack are of the ring buffer
 *
 * Data can be written into the slack area of the ring buffer by functions creating RDP commands.
 * However, when sending a completed command to the RDP, if the buffer has advanced into the slack,
 * it will be cleared and the pointer reset to start.  This is to stop any commands from being
 * split in the middle during wraparound.
 */
#define RINGBUFFER_SLACK 1024

/**
 * @brief Cached sprite structure
 * */

char _64Drive_buf[512];
typedef struct
{
    /** @brief S location of the top left of the texture relative to the original texture */
    uint32_t s;
    /** @brief T location of the top left of the texture relative to the original texture */
    uint32_t t;
    /** @brief Width of the texture */
    uint32_t width;
    /** @brief Height of the texture */
    uint32_t height;
} sprite_cache;

extern uint32_t __bitdepth;
extern uint32_t __width;
extern uint32_t __height;
extern void *__safe_buffer[];

/** @brief Ringbuffer where partially assembled commands will be placed before sending to the RDP */
static uint32_t rdp_ringbuffer[RINGBUFFER_SIZE / 4];
/** @brief Start of the command in the ringbuffer */
static uint32_t rdp_start = 0;
/** @brief End of the command in the ringbuffer */
static uint32_t rdp_end = 0;

/** @brief The current cache flushing strategy */
static flush_t flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

/** @brief Interrupt wait flag */
static volatile uint32_t wait_intr = 0;

/** @brief Array of cached textures in RDP TMEM indexed by the RDP texture slot */
static sprite_cache cache[8];

/** @brief Macro for advancing the display list pointer by one 64-bit command. */
#define ADVANCE_DISPLAY_LIST_PTR ( *list = (display_list_t *)(*list + 1) )

/**
 * @brief RDP interrupt handler
 *
 * This interrupt is called when a Sync Full operation has completed and it is safe to
 * use the output buffer with software
 */
static void __rdp_interrupt()
{
    /* Flag that the interrupt happened */
    wait_intr++;
}

/**
 * @brief Given a number, rount to a power of two
 *
 * @param[in] number
 *            A number that needs to be rounded
 * 
 * @return The next power of two that is greater than the number passed in.
 */
static inline uint32_t __rdp_round_to_power( uint32_t number )
{
    if( number <= 4   ) { return 4;   }
    if( number <= 8   ) { return 8;   }
    if( number <= 16  ) { return 16;  }
    if( number <= 32  ) { return 32;  }
    if( number <= 64  ) { return 64;  }
    if( number <= 128 ) { return 128; }

    /* Any thing bigger than 256 not supported */
    return 256;
}

/**
 * @brief Integer log base two of a number
 *
 * @param[in] number
 *            Number to take the log base two of
 *
 * @return Log base two of the number passed in.
 */
static inline uint32_t __rdp_log2( uint32_t number )
{
    switch( number )
    {
        case 4:
            return 2;
        case 8:
            return 3;
        case 16:
            return 4;
        case 32:
            return 5;
        case 64:
            return 6;
        case 128:
            return 7;
        default:
            /* Don't support more than 256 */
            return 8;
    }
}

/**
 * @brief Return the size of the current command buffered in the ring buffer
 *
 * @return The size of the command in bytes
 */
static inline uint32_t __rdp_ringbuffer_size( void )
{
    /* Normal length */
    return rdp_end - rdp_start;
}

/**
 * @brief Queue 32 bits of a command to the ring buffer
 *
 * @param[in] data
 *            32 bits of data to be queued at the end of the current command
 */
static void __rdp_ringbuffer_queue( uint32_t data )
{
    /* Only add commands if we have room */
    if( __rdp_ringbuffer_size() + sizeof(uint32_t) >= RINGBUFFER_SIZE ) { return; }

    /* Add data to queue to be sent to RDP */
    rdp_ringbuffer[rdp_end / 4] = data;
    rdp_end += 4;
}

/**
 * @brief Add a sentinel to the end of the display list for use with the library's send function.
 *
 * @param[in] list
 *            A display list pointer.
 */
void rdp_end_display_list( display_list_t **list )
{
    // Add a sentinel to the list to make sure we know when it's over.
    list[0]->command = 0x7FFFFFFFFFFFFFFF;
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Add a sentinel to the end of the display list for use with the library's send function.
 *
 * @param[in] list
 *            A display list pointer.
 */
void rdp_set_color_image( display_list_t **list, RDP_IMAGE_DATA_FORMAT format, RDP_PIXEL_WIDTH pixelwidth, uint16_t imagewidth, uint16_t *buffer )
{
    // Add a sentinel to the list to make sure we know when it's over.
    list[0]->words.hi = 0xBF000000 | (format << 20) | (pixelwidth << 19) | imagewidth;
    list[0]->words.lo = ((uint32_t)buffer) & 0x00FFFFFF;
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Add a sentinel to the end of the display list for use with the library's send function.
 *
 * @param[in] list
 *            A display list pointer.
 */
void rdp_set_z_image( display_list_t **list, uint16_t *buffer )
{
    // Add a sentinel to the list to make sure we know when it's over.
    list[0]->words.hi = 0xBE000000;
    list[0]->words.lo = (uint32_t)buffer;
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Send a complete display list to the RDP for execution.
 *
 * @param[in] list
 *            A pointer to the start of a display list.
 */
void rdp_execute_display_list( display_list_t *list, int size, display_list_location_t location )
{
    uint32_t length_in_uint64s = 0;

    while(list[length_in_uint64s].command != 0xFFFFFFFFFFFFFFFF)
    {
        length_in_uint64s++;
    }

    data_cache_hit_writeback_invalidate(list, size * sizeof(display_list_t));

    /* Make sure another thread doesn't attempt to render */
    disable_interrupts();

    /* Clear XBUS/Flush/Freeze */
    ((uint32_t *)0xA4100000)[3] = (location == DISPLAY_LIST_RDRAM ? 0x15 : 0x16);
    MEMORY_BARRIER();

    /* Don't saturate the RDP command buffer.  Another command could have been written
     * since we checked before disabling interrupts, but it is unlikely, so we probably
     * won't stall in this critical section long. */
    while( (((volatile uint32_t *)0xA4100000)[3] & 0x600) ) ;

    /* Send start and end of buffer location to kick off the command transfer */
    if(location == DISPLAY_LIST_RDRAM)
    {
        MEMORY_BARRIER();
        ((volatile uint32_t *)0xA4100000)[0] = ((uint32_t)list | 0xA0000000);
        MEMORY_BARRIER();
        ((volatile uint32_t *)0xA4100000)[1] = ((uint32_t)list | 0xA0000000) + length_in_uint64s*8;
        MEMORY_BARRIER();      
    }
    else
    {
        MEMORY_BARRIER();
        ((volatile uint32_t *)0xA4100000)[0] = ((uint32_t)list & 0x00000FFF);
        MEMORY_BARRIER();
        ((volatile uint32_t *)0xA4100000)[1] = ((uint32_t)list & 0x00000FFF) + length_in_uint64s*8;
        MEMORY_BARRIER();        
    }

    /* We are good now */
    enable_interrupts();
}

/**
 * @brief Initialize the RDP system
 */
void rdp_init( void )
{
    /* Default to flushing automatically */
    flush_strategy = FLUSH_STRATEGY_AUTOMATIC;

    /* Set the ringbuffer up */
    rdp_start = 0;
    rdp_end = 0;

    /* Set up interrupt for SYNC_FULL */
    register_DP_handler( __rdp_interrupt );
    set_DP_interrupt( 1 );
}

/**
 * @brief Close the RDP system
 *
 * This function closes out the RDP system and cleans up any internal memory
 * allocated by #rdp_init.
 */
void rdp_close( void )
{
    set_DP_interrupt( 0 );
    unregister_DP_handler( __rdp_interrupt );
}

/**
 * @brief Attach the RDP to a display context
 *
 * This function allows the RDP to operate on display contexts fetched with #display_lock.
 * This should be performed before any other operations to ensure that the RDP has a valid
 * output buffer to operate on.
 *
 * @param[in] disp
 *            A display context as returned by #display_lock
 */
bool rdp_attach_display( display_list_t **list, display_context_t disp )
{
    if( disp == 0 ) { return false; }

    /* Set the rasterization buffer */
    list[0]->words.hi = 0xBF000000 | ((__bitdepth == 2) ? 0x00100000 : 0x00180000) | (__width - 1);
    list[0]->words.lo = ((uint32_t)__get_buffer( disp )) & 0x00FFFFFF;
    ADVANCE_DISPLAY_LIST_PTR; 

    return true;
}

/**
 * @brief Detach the RDP from a display context
 *
 * @note This function requires interrupts to be enabled to operate properly.
 *
 * This function will ensure that all hardware operations have completed on an output buffer
 * before detaching the display context.  This should be performed before displaying the finished
 * output using #display_show
 */
void rdp_detach_display( display_list_t **list )
{
    /* Wait for SYNC_FULL to finish */
    wait_intr = 0;

    /* Force the RDP to rasterize everything and then interrupt us */
    rdp_sync( list, SYNC_FULL );


    if( INTERRUPTS_ENABLED == get_interrupts_state() )
    {
        /* Only wait if interrupts are enabled */
        //while( !wait_intr ) { ; }
    }

    /* Set back to zero for next detach */
    wait_intr = 0;
}

/**
 * @brief Perform a sync operation
 *
 * Do not use excessive sync operations between commands as this can
 * cause the RDP to stall.  If the RDP stalls due to too many sync
 * operations, graphics may not be displayed until the next render
 * cycle, causing bizarre artifacts.  The rule of thumb is to only add
 * a sync operation if the data you need is not yet available in the
 * pipeline.
 *
 * @param[in] sync
 *            The sync operation to perform on the RDP
 */
void rdp_sync( display_list_t **list, sync_t sync )
{
    switch( sync )
    {
        case SYNC_FULL:
            list[0]->words.hi = 0xA9000000;
            break;
        case SYNC_PIPE:
            list[0]->words.hi = 0xA7000000;
            break;
        case SYNC_TILE:
            list[0]->words.hi = 0xA8000000;
            break;
        case SYNC_LOAD:
            list[0]->words.hi = 0xA6000000;
            break;
    }

    list[0]->words.lo = 0x00000000;
    ADVANCE_DISPLAY_LIST_PTR;
}

/**
 * @brief Set the hardware clipping boundary
 *
 * @param[in] tx
 *            Top left X coordinate in pixels
 * @param[in] ty
 *            Top left Y coordinate in pixels
 * @param[in] bx
 *            Bottom right X coordinate in pixels
 * @param[in] by
 *            Bottom right Y coordinate in pixels
 */
void rdp_set_clipping( display_list_t **list, uint32_t tx, uint32_t ty, uint32_t bx, uint32_t by )
{
    /* Convert pixel space to screen space in command */
    list[0]->words.hi = ( 0xAD000000 | (tx << 14) | (ty << 2) );
    list[0]->words.lo = ( (bx << 14) | (by << 2) );
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Set the hardware clipping boundary to the entire screen
 */
void rdp_set_default_clipping( display_list_t **list )
{
    /* Clip box is the whole screen */
    rdp_set_clipping( list, 0, 0, __width, __height );
}

void rdp_set_fill_mode( display_list_t **list )
{
    /* Set other modes to fill and other defaults */
    list[0]->words.hi = ( 0xAFB000FF );
    list[0]->words.lo = ( 0x00004000 );
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Enable display of 2D filled (untextured) triangles
 *
 * This must be called before using #rdp_draw_filled_triangle.
 */
void rdp_enable_blend_fill( display_list_t **list )
{
    list[0]->words.hi = ( 0xAF8000FF );

    // ??? why is this necessary
    uint32_t ptr = ((uint32_t)list[0]) + 4;
    MMIO32(ptr) = 0x80000000;
    ADVANCE_DISPLAY_LIST_PTR; 
}

void rdp_set_other_modes( display_list_t **list, uint64_t mode_bits )
{
    /*
    list[0]->words.hi = ( 0xAF8000FF );

    // ??? why is this necessary
    uint32_t ptr = ((uint32_t)list[0]) + 4;
    MMIO32(ptr) = 0x00000000;
    ADVANCE_DISPLAY_LIST_PTR;
    */

   MMIO32(((uint32_t)list[0]) + 0) = 0xAF0000FF | (mode_bits >> 32);
   MMIO32(((uint32_t)list[0]) + 4) = (mode_bits & 0x00000000FFFFFFFF);
   ADVANCE_DISPLAY_LIST_PTR;
}

void rdp_set_combine_mode( display_list_t **list, uint64_t combine_mode )
{
    // Color formula: (A - B) * C + D

    list[0]->words.hi = ( 0xBC000000 | (combine_mode >> 32) );
    MMIO32(((uint32_t)list[0]) + 4) = (combine_mode & 0x00000000FFFFFFFF);

    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Enable display of 2D sprites
 *
 * This must be called before using #rdp_draw_textured_rectangle_scaled,
 * #rdp_draw_textured_rectangle, #rdp_draw_sprite or #rdp_draw_sprite_scaled.
 */
void rdp_enable_texture_copy( display_list_t **list )
{
    /* Set other modes to copy and other defaults */
    list[0]->words.hi = ( 0xAFA000FF );
    list[0]->words.lo = ( 0x00004000 );
    ADVANCE_DISPLAY_LIST_PTR;
}

/**
 * @brief Load a texture from RDRAM into RDP TMEM
 *
 * This function will take a texture from a sprite and place it into RDP TMEM at the offset and 
 * texture slot specified.  It is capable of pulling out a smaller texture from a larger sprite
 * map.
 *
 * @param[in] texslot
 *            The texture slot (0-7) to assign this texture to
 * @param[in] texloc
 *            The offset in RDP TMEM to place this texture
 * @param[in] mirror_enabled
 *            Whether to mirror this texture when displaying
 * @param[in] sprite
 *            Pointer to the sprite structure to load the texture out of
 * @param[in] sl
 *            The pixel offset S of the top left of the texture relative to sprite space
 * @param[in] tl
 *            The pixel offset T of the top left of the texture relative to sprite space
 * @param[in] sh
 *            The pixel offset S of the bottom right of the texture relative to sprite space
 * @param[in] th
 *            The pixel offset T of the bottom right of the texture relative to sprite space
 *
 * @return The amount of texture memory in bytes that was consumed by this texture.
 */
static uint32_t __rdp_load_texture( display_list_t **list, texslot_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int sl, int tl, int sh, int th )
{
    /* Invalidate data associated with sprite in cache */
    if( flush_strategy == FLUSH_STRATEGY_AUTOMATIC )
    {
        data_cache_hit_writeback_invalidate( sprite->data, sprite->width * sprite->height * sprite->bitdepth );
    }

    // SetTextureImage
    /* Point the RDP at the actual sprite data */
    list[0]->words.hi = ( 0xBD000000 | (sprite->format << (53-32)) | (sprite->pixel_size << (51-32)) | (sprite->width - 1) );
    list[0]->words.lo = ( (uint32_t)(sprite->data) );
    ADVANCE_DISPLAY_LIST_PTR;

    sprintf(_64Drive_buf, "SetTextureImage: format is %d, pixel_size is %d, width is %d, data ptr is %08X\n", 
        sprite->format, sprite->pixel_size, sprite->width - 1, (uint32_t)(sprite->data));
    //_64Drive_putstring(_64Drive_buf);

    /* Figure out the s,t coordinates of the sprite we are copying out of */
    int twidth = sh - sl + 1;
    int theight = th - tl + 1;

    /* Figure out the power of two this sprite fits into */
    uint32_t real_width  = __rdp_round_to_power( twidth );
    uint32_t real_height = __rdp_round_to_power( theight );
    uint32_t wbits = __rdp_log2( real_width );
    uint32_t hbits = __rdp_log2( real_height );

    /* Because we are dividing by 8, we want to round up if we have a remainder */
    int round_amount = (real_width % 8) ? 1 : 0;

    // SetTile
    /* Instruct the RDP to copy the sprite data out */
    list[0]->words.hi = ( 0xB5000000 | (sprite->format << (53-32)) | (sprite->pixel_size << (51-32)) |
                                       (((sprite->width / sprite->bitdepth) / 2) << 9) | ((texloc / 8) & 0x1FF) );
    list[0]->words.lo = ( ((texslot & 0x7) << 24) | (mirror_enabled == MIRROR_ENABLED ? 0x40100 : 0) | (hbits << 14) | (wbits << 4) );
    ADVANCE_DISPLAY_LIST_PTR;

    sprintf(_64Drive_buf, "SetTile: width in 64-bit words is %d. texture loaded to %08X in TMEM. hbits %d, wbits %d\n", 
        sprite->width / sprite->bitdepth, (texloc/8) & 0x1FF, hbits, wbits);
    //_64Drive_putstring(_64Drive_buf);

    // LoadSync
    rdp_sync(list, SYNC_LOAD);

    // LoadTile
    /* Copying out only a chunk this time */
    list[0]->words.hi = ( 0xB4000000 | (((sl << 2) & 0xFFF) << 12) | ((tl << 2) & 0xFFF) );
    list[0]->words.lo = ( ((texslot & 0x7) << 24) | (((sh << 2) & 0xFFF) << 12) | ((th << 2) & 0xFFF) );
    ADVANCE_DISPLAY_LIST_PTR;

    sprintf(_64Drive_buf, "LoadTile: sl %d tl %d sh %d th %d\n", 
        sl, tl, sh, th);
    //_64Drive_putstring(_64Drive_buf);    

	rdp_sync(list, SYNC_TILE);

    // SetTile
    /* Instruct the RDP to copy the sprite data out */
    list[0]->words.hi = ( 0xB5000000 | (sprite->format << (53-32)) | (sprite->pixel_size << (51-32)) |
                                       (((sprite->width / sprite->bitdepth) / 2) << 9) | ((texloc / 8) & 0x1FF) );
    list[0]->words.lo = ( ((texslot & 0x7) << 24) | (mirror_enabled == MIRROR_ENABLED ? 0x40100 : 0) | (hbits << 14) | (wbits << 4) );
    ADVANCE_DISPLAY_LIST_PTR;

    // SetTileSize
    list[0]->words.hi = ( 0xB2000000 );
    list[0]->words.lo = ( ((texslot & 0x7) << 24) | (((sh << 2) & 0xFFF) << 12) | ((th << 2) & 0xFFF) );
    ADVANCE_DISPLAY_LIST_PTR;

    /* Save sprite width and height for managed sprite commands */
    cache[texslot & 0x7].width = twidth - 1;
    cache[texslot & 0x7].height = theight - 1;
    cache[texslot & 0x7].s = sl;
    cache[texslot & 0x7].t = tl;
    
    //_64Drive_putstring("*** end ***");

    /* Return the amount of texture memory consumed by this texture */
    return ((real_width / 8) + round_amount) * 8 * real_height * sprite->bitdepth;
}


void rdp_load_texture_test( display_list_t **list, texslot_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int sl, int tl, int sh, int th )
{
    /*
    For example, the GBI macro gsDPLoadTextureTile performs all the tile and load commands necessary to load a texture tile. The sequence of commands is shown below (macros shown without parameters):

    gsDPSetTextureImage
    gsDPSetTile         // G_TX_LOADTILE
    gsDPLoadSync
    gsDPLoadTile        // G_TX_LOADTILE
    gsDPSetTile         // G_TX_RENDERTILE
    gsDPSetTileSize     // G_TX_RENDERTILE
    */

    //SetTextureImage
    list[0]->words.hi = ( 0xBD000000 | (3 << (51-32)) | (15 << (32-32)) );
    MMIO32(((uint32_t)list[0]) + 4) = (uint32_t)(sprite->data);
    ADVANCE_DISPLAY_LIST_PTR;

    //SetTile
    list[0]->words.hi = ( 0xB5000000 | (3 << (51-32)) | (8 << (41-32)) );
    MMIO32(((uint32_t)list[0]) + 4) = 0x00010040;
    ADVANCE_DISPLAY_LIST_PTR; 

    rdp_sync(list, SYNC_LOAD);

    //LoadTile
    list[0]->words.hi = ( 0xB4000000 );
    MMIO32(((uint32_t)list[0]) + 4) = 0x0003C03C;
    ADVANCE_DISPLAY_LIST_PTR; 

    rdp_sync(list, SYNC_TILE);

    //SetTileSize
    list[0]->words.hi = ( 0xB2000000 );
    MMIO32(((uint32_t)list[0]) + 4) = 0x0003C03C;
    ADVANCE_DISPLAY_LIST_PTR; 
}


/**
 * @brief Load a sprite into RDP TMEM
 *
 * @param[in] texslot
 *            The RDP texture slot to load this sprite into (0-7)
 * @param[in] texloc
 *            The RDP TMEM offset to place the texture at
 * @param[in] mirror_enabled
 *            Whether the sprite should be mirrored when displaying past boundaries
 * @param[in] sprite
 *            Pointer to sprite structure to load the texture from
 *
 * @return The number of bytes consumed in RDP TMEM by loading this sprite
 */
uint32_t rdp_load_texture( display_list_t **list, texslot_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite )
{
    if( !sprite ) { return 0; }

    //sprintf(_64Drive_buf, "rdp_load_texture: width %d height %d\n", sprite->width, sprite->height);
    //_64Drive_putstring(_64Drive_buf);

    return __rdp_load_texture( list, texslot, texloc, mirror_enabled, sprite, 0, 0, sprite->width - 1, sprite->height - 1 );
}

/**
 * @brief Load part of a sprite into RDP TMEM
 *
 * Given a sprite with vertical and horizontal slices defined, this function will load the slice specified in
 * offset into texture memory.  This is usefl for treating a large sprite as a tilemap.
 *
 * Given a sprite with 3 horizontal slices and two vertical slices, the offsets are as follows:
 *
 * <pre>
 * *---*---*---*
 * | 0 | 1 | 2 |
 * *---*---*---*
 * | 3 | 4 | 5 |
 * *---*---*---*
 * </pre>
 *
 * @param[in] texslot
 *            The RDP texture slot to load this sprite into (0-7)
 * @param[in] texloc
 *            The RDP TMEM offset to place the texture at
 * @param[in] mirror_enabled
 *            Whether the sprite should be mirrored when displaying past boundaries
 * @param[in] sprite
 *            Pointer to sprite structure to load the texture from
 * @param[in] offset
 *            Offset of the particular slice to load into RDP TMEM.
 *
 * @return The number of bytes consumed in RDP TMEM by loading this sprite
 */
uint32_t rdp_load_texture_stride( display_list_t **list, texslot_t texslot, uint32_t texloc, mirror_t mirror_enabled, sprite_t *sprite, int offset )
{
    if( !sprite ) { return 0; }

    /* Figure out the s,t coordinates of the sprite we are copying out of */
    int twidth = sprite->width / sprite->hslices;
    int theight = sprite->height / sprite->vslices;

    int sl = (offset % sprite->hslices) * twidth;
    int tl = (offset / sprite->hslices) * theight;
    int sh = sl + twidth - 1;
    int th = tl + theight - 1;

    return __rdp_load_texture( list, texslot, texloc, mirror_enabled, sprite, sl, tl, sh, th );
}

/**
 * @brief Draw a textured rectangle with a scaled texture
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture
 * at a scale other than 1.  This allows rectangles to be drawn with stretched or squashed textures.
 * If the rectangle is larger than the texture after scaling, it will be tiled or mirrored based on the
 * mirror setting given in the load texture command.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] tx
 *            The pixel X location of the top left of the rectangle
 * @param[in] ty
 *            The pixel Y location of the top left of the rectangle
 * @param[in] bx
 *            The pixel X location of the bottom right of the rectangle
 * @param[in] by
 *            The pixel Y location of the bottom right of the rectangle
 * @param[in] x_scale
 *            Horizontal scaling factor
 * @param[in] y_scale
 *            Vertical scaling factor
 */
void rdp_draw_textured_rectangle_scaled( display_list_t **list, texslot_t texslot, int tx, int ty, int bx, int by, double x_scale, double y_scale, int s_ul, int t_ul )
{
    //uint16_t s = 0;// cache[texslot & 0x7].s << 5;
    //uint16_t t = 0;//cache[texslot & 0x7].t << 5;

    //uint16_t s = cache[texslot & 0x7].s << 5;
    //uint16_t t = cache[texslot & 0x7].t << 5;

    uint16_t s = s_ul;
    uint16_t t = t_ul;

    /* Cant display < 0, so must clip size and move S,T coord accordingly */
    if( tx < 0 )
    {
        s += (int)(((double)((-tx) << 5)) * (1.0 / x_scale));
        tx = 0;
    }

    if( ty < 0 )
    {
        t += (int)(((double)((-ty) << 5)) * (1.0 / y_scale));
        ty = 0;
    }

    /* Calculate the scaling constants based on a 6.10 fixed point system */
    int xs = (int)((1.0 / x_scale) * 1024.0);
    int ys = (int)((1.0 / y_scale) * 1024.0);

    /* Set up rectangle position in screen space */
    MMIO32(((uint32_t)list[0]) + 0) = ( 0xA4000000 | (bx << 14) | (by << 2) );
    MMIO32(((uint32_t)list[0]) + 4) = ( ((texslot & 0x7) << 24) | (tx << 14) | (ty << 2) );
    ADVANCE_DISPLAY_LIST_PTR;

    /* Set up texture position and scaling to 1:1 copy */
    MMIO32(((uint32_t)list[0]) + 0) = ( (s << 16) | t );
    MMIO32(((uint32_t)list[0]) + 4) = ( (xs & 0xFFFF) << 16 | (ys & 0xFFFF) );
    ADVANCE_DISPLAY_LIST_PTR;

    

}

/**
 * @brief Draw a textured rectangle
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture.
 * If the rectangle is larger than the texture, it will be tiled or mirrored based on the* mirror setting
 * given in the load texture command.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] tx
 *            The pixel X location of the top left of the rectangle
 * @param[in] ty
 *            The pixel Y location of the top left of the rectangle
 * @param[in] bx
 *            The pixel X location of the bottom right of the rectangle
 * @param[in] by
 *            The pixel Y location of the bottom right of the rectangle
 */
void rdp_draw_textured_rectangle( display_list_t **list, texslot_t texslot, int tx, int ty, int bx, int by )
{
    /* Simple wrapper */
    rdp_draw_textured_rectangle_scaled( list, texslot, tx, ty, bx, by, 1.0, 1.0, 0, 0 );
}

/**
 * @brief Draw a texture to the screen as a sprite
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] x
 *            The pixel X location of the top left of the sprite
 * @param[in] y
 *            The pixel Y location of the top left of the sprite
 */
void rdp_draw_sprite( display_list_t **list, texslot_t texslot, int x, int y )
{
    /* Just draw a rectangle the size of the sprite */
    rdp_draw_textured_rectangle_scaled( list, texslot, x, y, x + cache[texslot & 0x7].width, y + cache[texslot & 0x7].height, 1.0, 1.0, 0, 0 );
}

/**
 * @brief Draw a texture to the screen as a scaled sprite
 *
 * Given an already loaded texture, this function will draw a rectangle textured with the loaded texture.
 *
 * Before using this command to draw a textured rectangle, use #rdp_enable_texture_copy to set the RDP
 * up in texture mode.
 *
 * @param[in] texslot
 *            The texture slot that the texture was previously loaded into (0-7)
 * @param[in] x
 *            The pixel X location of the top left of the sprite
 * @param[in] y
 *            The pixel Y location of the top left of the sprite
 * @param[in] x_scale
 *            Horizontal scaling factor
 * @param[in] y_scale
 *            Vertical scaling factor
 */
void rdp_draw_sprite_scaled( display_list_t **list, texslot_t texslot, int x, int y, double x_scale, double y_scale )
{
    /* Since we want to still view the whole sprite, we must resize the rectangle area too */
    int new_width = (int)(((double)cache[texslot & 0x7].width * x_scale) + 0.5);
    int new_height = (int)(((double)cache[texslot & 0x7].height * y_scale) + 0.5);
    
    /* Draw a rectangle the size of the new sprite */
    rdp_draw_textured_rectangle_scaled( list, texslot, x, y, x + new_width, y + new_height, x_scale, y_scale, 0, 0 );
}

/**
 * @brief Set the primitive draw color for subsequent filled primitive operations
 *
 * This function sets the color of all #rdp_draw_filled_rectangle operations that follow.
 * Note that in 16 bpp mode, the color must be a packed color.  This means that the high
 * 16 bits and the low 16 bits must both be the same color.  Use #graphics_make_color or
 * #graphics_convert_color to generate valid colors.
 *
 * @param[in] color
 *            Color to draw primitives in
 */
void rdp_set_primitive_color( display_list_t **list, uint32_t color )
{
    /* Set packed color */
    list[0]->words.hi = 0xB7000000;
    list[0]->words.lo = color;
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Set the blend draw color for subsequent filled primitive operations
 *
 * This function sets the color of all #rdp_draw_filled_triangle operations that follow.
 *
 * @param[in] color
 *            Color to draw primitives in
 */
void rdp_set_blend_color( display_list_t **list, uint32_t color )
{
    list[0]->words.hi = 0xB9000000;
    list[0]->words.lo = color;
    ADVANCE_DISPLAY_LIST_PTR; 
}

void rdp_set_env_color( display_list_t **list, uint32_t color )
{
    list[0]->words.hi = 0xBB000000;
    list[0]->words.lo = color;
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Draw a filled rectangle
 *
 * Given a color set with #rdp_set_primitive_color, this will draw a filled rectangle
 * to the screen.  This is most often useful for erasing a buffer before drawing to it
 * by displaying a black rectangle the size of the screen.  This is much faster than
 * setting the buffer blank in software.  However, if you are planning on drawing to
 * the entire screen, blanking may be unnecessary.  
 *
 * Before calling this function, make sure that the RDP is set to primitive mode by
 * calling #rdp_enable_primitive_fill.
 *
 * @param[in] tx
 *            Pixel X location of the top left of the rectangle
 * @param[in] ty
 *            Pixel Y location of the top left of the rectangle
 * @param[in] bx
 *            Pixel X location of the bottom right of the rectangle
 * @param[in] by
 *            Pixel Y location of the bottom right of the rectangle
 */
void rdp_draw_filled_rectangle( display_list_t **list, int tx, int ty, int bx, int by )
{
    if( tx < 0 ) { tx = 0; }
    if( ty < 0 ) { ty = 0; }

    list[0]->words.hi = 0xB6000000 | ( bx << 14 ) | ( by << 2 );
    list[0]->words.lo = ( tx << 14 ) | ( ty << 2 );
    ADVANCE_DISPLAY_LIST_PTR; 
}

/**
 * @brief Draw a filled triangle
 *
 * Given a color set with #rdp_set_blend_color, this will draw a filled triangle
 * to the screen. Vertex order is not important.
 *
 * Before calling this function, make sure that the RDP is set to blend mode by
 * calling #rdp_enable_blend_fill.
 *
 * @param[in] x1
 *            Pixel X1 location of triangle
 * @param[in] y1
 *            Pixel Y1 location of triangle
 * @param[in] x2
 *            Pixel X2 location of triangle
 * @param[in] y2
 *            Pixel Y2 location of triangle
 * @param[in] x3
 *            Pixel X3 location of triangle
 * @param[in] y3
 *            Pixel Y3 location of triangle
 */
void rdp_draw_filled_triangle_fixed( display_list_t **list, Fixed x1, Fixed y1, Fixed x2, Fixed y2, Fixed x3, Fixed y3 )
{
    Fixed temp_x, temp_y;

    /* sort vertices by Y ascending to find the major, mid and low edges */
    if( y1 > y2 ) { temp_x = x2, temp_y = y2; y2 = y1; y1 = temp_y; x2 = x1; x1 = temp_x; }
    if( y2 > y3 ) { temp_x = x3, temp_y = y3; y3 = y2; y2 = temp_y; x3 = x2; x2 = temp_x; }
    if( y1 > y2 ) { temp_x = x2, temp_y = y2; y2 = y1; y1 = temp_y; x2 = x1; x1 = temp_x; }

    /* calculate Y edge coefficients in 11.2 fixed format */
    // Convert Q16 to 11.2 fixed point
    int16_t yh = ((y1 & 0x07FF0000) >> 14) | (y1 & 0x00000002);
    int32_t ym = (((y2 & 0x07FF0000) >> 14) | (y2 & 0x00000002)) << 16; // high word
    int16_t yl = ((y3 & 0x07FF0000) >> 14) | (y3 & 0x00000002);

    /* calculate X edge coefficients in 16.16 fixed format */
    Fixed xh = x1;
    Fixed xm = x1;
    Fixed xl = x2;

    /* calculate inverse slopes in 16.16 fixed format */
    Fixed dxhdy = (y3 == y1) ? 0 : FX_Divide((x3 - x1), (y3 - y1));
    Fixed dxmdy = (y2 == y1) ? 0 : FX_Divide((x2 - x1), (y2 - y1));
    Fixed dxldy = (y3 == y2) ? 0 : FX_Divide((x3 - x2), (y3 - y2));

    int winding = (FX_Multiply(x1, y2) - FX_Multiply(x2, y1)) 
        + (FX_Multiply(x2, y3) - FX_Multiply(x3, y2))
        + (FX_Multiply(x3, y1) - FX_Multiply(x1, y3));
    int flip = (winding > 0 ? 1 : 0) << 23;

    list[0]->words.hi = ( 0x88000000 | flip | yl );
    list[0]->words.lo = ym | yh;
    ADVANCE_DISPLAY_LIST_PTR;
    list[0]->words.hi = xl;
    list[0]->words.lo = dxldy;
    ADVANCE_DISPLAY_LIST_PTR; 
    list[0]->words.hi = xh;
    list[0]->words.lo = dxhdy;
    ADVANCE_DISPLAY_LIST_PTR;
    list[0]->words.hi = xm;
    list[0]->words.lo = dxmdy;
    ADVANCE_DISPLAY_LIST_PTR;
}

void rdp_draw_filled_triangle( display_list_t **list, float x1, float y1, float x2, float y2, float x3, float y3 )
{
    float temp_x, temp_y;
    const float to_fixed_11_2 = 4.0f;
    const float to_fixed_16_16 = 65536.0f;
    
    /* sort vertices by Y ascending to find the major, mid and low edges */
    if( y1 > y2 ) { temp_x = x2, temp_y = y2; y2 = y1; y1 = temp_y; x2 = x1; x1 = temp_x; }
    if( y2 > y3 ) { temp_x = x3, temp_y = y3; y3 = y2; y2 = temp_y; x3 = x2; x2 = temp_x; }
    if( y1 > y2 ) { temp_x = x2, temp_y = y2; y2 = y1; y1 = temp_y; x2 = x1; x1 = temp_x; }

    /* calculate Y edge coefficients in 11.2 fixed format */
    int yh = y1 * to_fixed_11_2;
    int ym = (int)( y2 * to_fixed_11_2 ) << 16; // high word
    int yl = y3 * to_fixed_11_2;
    
    /* calculate X edge coefficients in 16.16 fixed format */
    int xh = x1 * to_fixed_16_16;
    int xm = x1 * to_fixed_16_16;
    int xl = x2 * to_fixed_16_16;
    
    /* calculate inverse slopes in 16.16 fixed format */
    int dxhdy = ( y3 == y1 ) ? 0 : ( ( x3 - x1 ) / ( y3 - y1 ) ) * to_fixed_16_16;
    int dxmdy = ( y2 == y1 ) ? 0 : ( ( x2 - x1 ) / ( y2 - y1 ) ) * to_fixed_16_16;
    int dxldy = ( y3 == y2 ) ? 0 : ( ( x3 - x2 ) / ( y3 - y2 ) ) * to_fixed_16_16;
    
    /* determine the winding of the triangle */
    int winding = ( x1 * y2 - x2 * y1 ) + ( x2 * y3 - x3 * y2 ) + ( x3 * y1 - x1 * y3 );
    int flip = ( winding > 0 ? 1 : 0 ) << 23;

    list[0]->words.hi = ( 0x88000000 | flip | yl );
    list[0]->words.lo = ym | yh;
    ADVANCE_DISPLAY_LIST_PTR;
    list[0]->words.hi = xl;
    list[0]->words.lo = dxldy;
    ADVANCE_DISPLAY_LIST_PTR; 
    list[0]->words.hi = xh;
    list[0]->words.lo = dxhdy;
    ADVANCE_DISPLAY_LIST_PTR;
    list[0]->words.hi = xm;
    list[0]->words.lo = dxmdy;
    ADVANCE_DISPLAY_LIST_PTR;    
}

/**
 * @brief Set the flush strategy for texture loads
 *
 * If textures are guaranteed to be in uncached RDRAM or the cache
 * is flushed before calling load operations, the RDP can be told
 * to skip flushing the cache.  This affords a good speedup.  However,
 * if you are changing textures in memory on the fly or otherwise do
 * not want to deal with cache coherency, set the cache strategy to
 * automatic to have the RDP flush cache before texture loads.
 *
 * @param[in] flush
 *            The cache strategy, either #FLUSH_STRATEGY_NONE or
 *            #FLUSH_STRATEGY_AUTOMATIC.
 */
void rdp_set_texture_flush( flush_t flush )
{
    flush_strategy = flush;
}

/** @} */
