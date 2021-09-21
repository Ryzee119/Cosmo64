// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <malloc.h>
#include "video.h"
#include "palette.h"
#include "tile.h"
#include "input.h"
#include "b800_font.h"
#include "n64/rdl.h"
#include "n64/rdp_commands.h"

#define N64_MIN(a,b) (((a)<(b))?(a):(b))
#define N64_MAX(a,b) (((a)>(b))?(a):(b))

#define VideoSurface SDL_Surface

VideoSurface game_surface;
VideoSurface text_surface;

static display_context_t disp = 0;

#define NUM_DISPLAY_LISTS 2
static RdpDisplayList *dls[NUM_DISPLAY_LISTS] = {NULL};
static RdpDisplayList *dl;
static uint16_t *_palette1;
static uint16_t *_palette2;
static const uint8_t GAME_PALETTE = 0;
static const uint8_t TEXT_PALETTE = 1;

bool is_game_mode = true;
bool is_fullscreen = false;
bool video_has_initialised = false;
int video_scale_factor = DEFAULT_SCALE_FACTOR;

void video_fill_surface_with_black(VideoSurface *surface);

void fade_to_black_speed_3()
{
    fade_to_black(3);
}

bool init_surface(VideoSurface *surface, int width, int height)
{
    surface->pixels = memalign(64, width * height);
    assert(surface->pixels != NULL);
    surface->w = width;
    surface->h = height;
    surface->pitch = width * sizeof(uint8_t);
    
    surface->format = (SDL_PixelFormat *)malloc(sizeof(SDL_PixelFormat));
    surface->format->palette = (SDL_Palette  *)malloc(sizeof(SDL_Palette));
    surface->format->BytesPerPixel = 1;
    
    return true;
}

bool video_init()
{
    debug_init(DEBUG_FEATURE_LOG_ISVIEWER);
    dfs_init(DFS_DEFAULT_LOCATION);
    init_interrupts();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    rdp_init();
    for (int i = 0; i < NUM_DISPLAY_LISTS; i++)
    {
        dls[i] = rdl_heap_alloc(2048);
        rdl_reset(dls[i]);
    }
    dl = dls[0];

    _palette1 = (uint16_t *)memalign(64, sizeof(uint16_t) * 16);
    _palette2 = (uint16_t *)memalign(64, sizeof(uint16_t) * 16);
    assert(_palette1 != NULL);
    assert(_palette2 != NULL);

    //Create the game surface and load/apply palette.
    init_surface(&game_surface, SCREEN_WIDTH, SCREEN_HEIGHT);
    set_palette_on_surface(&game_surface);
    game_surface.format->palette->refcount = GAME_PALETTE;
    memcpy(_palette1, game_surface.format->palette->colors, sizeof(uint16_t) * 16);
    data_cache_hit_writeback_invalidate(_palette1, sizeof(uint16_t) * 16);
    rdl_push(dl,
        RdpSyncTile(),
        MRdpLoadPalette16(2, (uint32_t)_palette1, RDP_AUTO_TMEM_SLOT(GAME_PALETTE)),
        RdpSyncTile()
    );

    //Create the text surface and load/apply palette.
    init_surface(&text_surface, SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2);
    set_palette_on_surface(&text_surface);
    video_fill_surface_with_black(&text_surface);
    text_surface.format->palette->refcount = TEXT_PALETTE;
    memcpy(_palette2, text_surface.format->palette->colors, sizeof(uint16_t) * 16);
    data_cache_hit_writeback_invalidate(_palette2, sizeof(uint16_t) * 16);
    rdl_push(dl,
        RdpSyncTile(),
        MRdpLoadPalette16(2, (uint32_t)_palette2, RDP_AUTO_TMEM_SLOT(TEXT_PALETTE)),
        RdpSyncTile()
    );

    set_game_mode();
    video_has_initialised = true;
    return true;
}

void video_shutdown()
{
    rdp_detach_display();
    for (int i = 0; i < NUM_DISPLAY_LISTS; i++)
    {
        rdl_free(dls[i]);
    }
    rdp_close();
    free(_palette1);
    free(_palette2);
    free(game_surface.format->palette);
    free(game_surface.format);
    free(game_surface.pixels);
    free(text_surface.format->palette);
    free(text_surface.pixels);
}

void set_text_mode()
{
    if (!is_game_mode)
        return;
    //SDL_RenderSetLogicalSize(renderer, text_surface.w, text_surface.h);
    is_game_mode = false;
}

void set_game_mode()
{
    if (is_game_mode)
        return;
    //SDL_RenderSetLogicalSize(renderer, game_surface.w, game_surface.h);
    is_game_mode = true;
}

void video_update()
{
    VideoSurface *src = is_game_mode ? &game_surface : &text_surface;
    uint8_t pal_slot = is_game_mode ? GAME_PALETTE : TEXT_PALETTE;
    data_cache_hit_writeback_invalidate(src->pixels,  src->w * src->h);
    while (!(disp = display_lock())) {}

    //We draw the screen from top to bottom.
    //We can only draw 2048bytes per loop and for simplicity we want it to be a multiple
    //of the width.
    int x_per_loop = src->w;
    int y_per_loop = 5;
    int current_y = 0; //The texture is offset on the Yaxis by this many pixels
    int chunk_size = x_per_loop * y_per_loop;
    assert(chunk_size <= 2048);
    assert(y_per_loop > 0);

    rdp_attach_display(disp);
    rdp_set_clipping(0, 0, N64_MIN(SCREEN_WIDTH, src->w),240);

    rdl_push(dl,
        RdpSyncPipe(),
        RdpSetOtherModes(SOM_CYCLE_COPY | SOM_ENABLE_TLUT_RGB16)
    );

    uint8_t *ptr = src->pixels;
    while(current_y < 240)
    {

        //Load y_per_loop * x_per_loop pixels into TMEM, and apply palette from pal_slot
        //Then draw to framebuffer
        rdl_push(dl,
            MRdpLoadTex8bpp(0, (uint32_t)ptr, x_per_loop, y_per_loop, x_per_loop, RDP_AUTO_TMEM_SLOT(0), RDP_AUTO_PITCH),
            MRdpSetTile8bpp(1, RDP_AUTO_TMEM_SLOT(0), RDP_AUTO_PITCH, RDP_AUTO_TMEM_SLOT(pal_slot), x_per_loop, y_per_loop),
            //Draw the first line twice. (1/5 lines is drawn twice, so that over 200 lines, this will scale to 240 on screen)
            RdpTextureRectangle1I(1, 0, current_y + 0, 0 + x_per_loop, current_y + 1),
            RdpTextureRectangle2I(0, 0, 4, 1),
            //Draw the rest of the lines (including the above line)
            RdpTextureRectangle1I(1, 0, current_y + 1, 0 + x_per_loop, current_y + y_per_loop),
            RdpTextureRectangle2I(0, 0, 4, 1)
        );
        current_y += y_per_loop + 1;
        ptr += chunk_size;
    }

    rdl_flush(dl);
    rdl_exec(dl);
    for (int i = 0; i < NUM_DISPLAY_LISTS; i++)
    {
        if (dl == dls[i])
        {
            dl = dls[(i + 1) % NUM_DISPLAY_LISTS];
            break;
        }
    }
    rdl_reset(dl);
    rdp_detach_display();
    display_show(disp);
}

void video_draw_tile(Tile *tile, uint16 x, uint16 y)
{
    uint8 *pixel = (uint8 *)game_surface.pixels + x + y * SCREEN_WIDTH;
    uint8 *tile_pixel = tile->pixels;
    if (tile->type == SOLID)
    {
        for (int i = 0; i < TILE_HEIGHT; i++)
        {
            memcpy(pixel, tile_pixel, TILE_WIDTH);
            pixel += SCREEN_WIDTH;
            tile_pixel += TILE_WIDTH;
        }
    }
    else
    {
        for (int i = 0; i < TILE_HEIGHT; i++)
        {
            for (int j = 0; j < TILE_WIDTH; j++)
            {
                if (*tile_pixel != TRANSPARENT_COLOR)
                {
                    *pixel = *tile_pixel;
                }
                pixel++;
                tile_pixel++;
            }
            pixel += SCREEN_WIDTH - TILE_WIDTH;
        }
    }
}

void video_draw_font_tile(Tile *tile, uint16 x, uint16 y, uint8 font_color)
{
    uint8 *pixel = (uint8 *)game_surface.pixels + x + y * SCREEN_WIDTH;
    uint8 *tile_pixel = tile->pixels;

    for (int i = 0; i < TILE_HEIGHT; i++)
    {
        for (int j = 0; j < TILE_WIDTH; j++)
        {
            if (*tile_pixel != TRANSPARENT_COLOR)
            {
                *pixel = *tile_pixel == 0xf ? font_color : *tile_pixel;
            }
            pixel++;
            tile_pixel++;
        }
        pixel += SCREEN_WIDTH - TILE_WIDTH;
    }
}

void video_draw_tile_solid_white(Tile *tile, uint16 x, uint16 y)
{
    uint8 *pixel = (uint8 *)game_surface.pixels + x + y * SCREEN_WIDTH;
    uint8 *tile_pixel = tile->pixels;
    if (tile->type == SOLID)
    {
        for (int i = 0; i < TILE_HEIGHT; i++)
        {
            memcpy(pixel, tile_pixel, TILE_WIDTH);
            pixel += SCREEN_WIDTH;
            tile_pixel += TILE_WIDTH;
        }
    }
    else
    {
        for (int i = 0; i < TILE_HEIGHT; i++)
        {
            for (int j = 0; j < TILE_WIDTH; j++)
            {
                if (*tile_pixel != TRANSPARENT_COLOR)
                {
                    *pixel = 0xf;
                }
                pixel++;
                tile_pixel++;
            }
            pixel += SCREEN_WIDTH - TILE_WIDTH;
        }
    }
}

void video_draw_tile_mode3(Tile *tile, uint16 x, uint16 y)
{
    uint8 *pixel = (uint8 *)game_surface.pixels + x + y * SCREEN_WIDTH;
    uint8 *tile_pixel = tile->pixels;
    if (tile->type == TRANSPARENT)
    {
        for (int i = 0; i < TILE_HEIGHT; i++)
        {
            for (int j = 0; j < TILE_WIDTH; j++)
            {
                if (*tile_pixel != TRANSPARENT_COLOR)
                {
                    *pixel |= 8;
                }
                pixel++;
                tile_pixel++;
            }
            pixel += SCREEN_WIDTH - TILE_WIDTH;
        }
    }
}

void video_draw_highlight_effect(uint16 x, uint16 y, uint8 type)
{
    uint8 *pixel = (uint8 *)game_surface.pixels + x + y * SCREEN_WIDTH;
    for (int i = 0; i < TILE_HEIGHT; i++)
    {
        for (int j = 0; j < TILE_WIDTH; j++)
        {
            if ((type == 0 && i + j >= 7) ||
                type == 1 ||
                (type == 2 && i >= j))
            {
                *pixel |= 8; //set intensity bit
            }
            pixel++;
        }
        pixel += SCREEN_WIDTH - TILE_WIDTH;
    }
}

void video_draw_tile_with_clip_rect(Tile *tile, uint16 x, uint16 y, uint16 clip_x, uint16 clip_y, uint16 clip_w, uint16 clip_h)
{
    uint16 tx = 0;
    uint16 ty = 0;
    uint16 w = TILE_WIDTH;
    uint16 h = TILE_HEIGHT;

    if (x + w < clip_x ||
        y + h < clip_y ||
        x > clip_x + clip_w ||
        y > clip_y + clip_h)
    {
        return;
    }

    if (x < clip_x)
    {
        tx = (clip_x - x);
        w = TILE_WIDTH - tx;
        x = clip_x;
    }

    if (x + w > clip_x + clip_w)
    {
        w -= ((x + w) - (clip_x + clip_w));
    }

    if (y < clip_y)
    {
        ty = (clip_y - y);
        h = TILE_HEIGHT - ty;
        y = clip_y;
    }

    if (y + h > clip_y + clip_h)
    {
        h -= ((y + h) - (clip_y + clip_h));
    }

    uint8 *pixel = (uint8 *)game_surface.pixels + x + y * SCREEN_WIDTH;
    uint8 *tile_pixel = &tile->pixels[tx + ty * TILE_WIDTH];
    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            if (tile_pixel[j] != TRANSPARENT_COLOR)
            {
                pixel[j] = tile_pixel[j];
            }
        }
        pixel += SCREEN_WIDTH;
        tile_pixel += TILE_WIDTH;
    }
}

void video_draw_tile_flipped(Tile *tile, uint16 x, uint16 y)
{
    uint8 *pixel = (uint8 *)game_surface.pixels + x + (y + TILE_HEIGHT - 1) * SCREEN_WIDTH;
    uint8 *tile_pixel = tile->pixels;
    for (int i = 0; i < TILE_HEIGHT; i++)
    {
        for (int j = 0; j < TILE_WIDTH; j++)
        {
            if (*tile_pixel != TRANSPARENT_COLOR)
            {
                *pixel = *tile_pixel;
            }
            pixel++;
            tile_pixel++;
        }
        pixel -= (SCREEN_WIDTH + TILE_WIDTH);
    }
}

void video_update_palette(int palette_index, SDL_Color new_color)
{
    SDL_SetPaletteColors(game_surface.format->palette, &new_color, palette_index, 1);
    memcpy(_palette1, game_surface.format->palette->colors, sizeof(uint16_t) * 16);
    //Update the palette into TMEM
    rdl_push(dl,
        RdpSyncTile(),
        MRdpLoadPalette16(2, (uint32_t)_palette1, RDP_AUTO_TMEM_SLOT(GAME_PALETTE)),
        RdpSyncTile()
    );
}

void fade_to_black(uint16 wait_time)
{
    for (int i = 0; i < 16; i++)
    {
        cosmo_wait(wait_time);
        set_palette_color(i, 0);
        video_update();
    }
}

void fade_to_white(uint16 wait_time)
{
    for (int i = 0; i < 16; i++)
    {
        cosmo_wait(wait_time);
        set_palette_color(i, 23);
        video_update();
    }
}

void fade_in_from_black(uint16 wait_time)
{
    int j = 0;
    for (int i = 0; i < 16; i++)
    {
        if (i == 8)
        {
            j = 8;
        }
        set_palette_color(i, i + j);
        video_update();
        cosmo_wait(wait_time);
    }
}

void fade_in_from_black_with_delay_3()
{
    fade_in_from_black(3);
}

void video_fill_surface_with_black(VideoSurface *surface)
{
    memset(surface->pixels, 0, surface->w * surface->h);
}

void video_fill_screen_with_black()
{
    video_fill_surface_with_black(&game_surface);
}

void video_draw_fullscreen_image(uint8 *pixels)
{
    memcpy(game_surface.pixels, pixels, SCREEN_WIDTH * SCREEN_HEIGHT);
}

void video_draw_text(uint8 character, int fg, int bg, int x, int y)
{
    uint8 *pixel = (uint8 *)text_surface.pixels + x * B800_FONT_WIDTH + y * B800_FONT_HEIGHT * text_surface.w;

    int count = 0;
    for (int i = 0; i < B800_FONT_HEIGHT; i++)
    {
        for (int j = 0; j < B800_FONT_WIDTH; j++)
        {
            int p = (int10_font_16[character * B800_FONT_HEIGHT + (count / 8)] >> (7 - (count % 8))) & 1;
            pixel[j] = p ? (uint8)fg : (uint8)bg;
            count++;
        }
        pixel += text_surface.w;
    }
}

void video_set_fullscreen(bool new_state)
{
}

void video_set_scale_factor(int scale_factor)
{
    video_scale_factor = scale_factor;
}
