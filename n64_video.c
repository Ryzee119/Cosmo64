// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <malloc.h>
#include <SDL_timer.h>
#include "video.h"
#include "palette.h"
#include "tile.h"
#include "input.h"
#include "b800_font.h"
#include "ugfx.h"

#define VideoSurface SDL_Surface

static VideoSurface game_surface;
static VideoSurface text_surface;

static display_context_t disp = 0;
static ugfx_buffer_t *render_commands;
static uint32_t display_width;
static uint32_t display_height;

static bool palette_dirty = false;
static uint16_t *_palette1;
static uint16_t *_palette2;
static const uint8_t GAME_PALETTE = 1;
static const uint8_t TEXT_PALETTE = 2;

static bool is_game_mode = true;
static bool video_has_initialised = false;

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
    surface->format->palette = (SDL_Palette *)malloc(sizeof(SDL_Palette));
    surface->format->BytesPerPixel = 1;

    return true;
}

bool video_init()
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE_FETCH_ALWAYS);
    ugfx_init(UGFX_DEFAULT_RDP_BUFFER_SIZE);

    display_width = 320;
    display_height = 240;

    _palette1 = (uint16_t *)memalign(64, sizeof(uint16_t) * 16);
    _palette2 = (uint16_t *)memalign(64, sizeof(uint16_t) * 16);
    assert(_palette1 != NULL);
    assert(_palette2 != NULL);

    //Create the game surface and load/apply palette. This is the main 320x200 game screen.
    init_surface(&game_surface, SCREEN_WIDTH, SCREEN_HEIGHT);
    set_palette_on_surface(&game_surface);
    memcpy(_palette1, game_surface.format->palette->colors, sizeof(uint16_t) * 16);
    data_cache_hit_writeback_invalidate(_palette1, sizeof(uint16_t) * 16);

    //Create the text surface and load/apply palette. This is the 640x400 DOS screen that shows up on exit.
    init_surface(&text_surface, SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2);
    set_palette_on_surface(&text_surface);
    memcpy(_palette2, text_surface.format->palette->colors, sizeof(uint16_t) * 16);
    data_cache_hit_writeback_invalidate(_palette2, sizeof(uint16_t) * 16);

    //Load the Game and Text palette into TMEM into Tile 2 and 3
    ugfx_command_t commands[] = {
        //Load the Game palette
        ugfx_set_tile(UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_4B, 16, 0x800 + (GAME_PALETTE * 0x100), 2, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        ugfx_set_texture_image((uint32_t)_palette1, UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_16B, 16 - 1),
        ugfx_load_tlut(0 << 2, 0, 15 << 2, 0, 2),
        ugfx_sync_tile(),

        //Load the 'DOS' text screen palette
        ugfx_set_tile(UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_4B, 16, 0x800 + (TEXT_PALETTE * 0x100), 3, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        ugfx_set_texture_image((uint32_t)_palette2, UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_16B, 16 - 1),
        ugfx_load_tlut(0 << 2, 0, 15 << 2, 0, 3),
        ugfx_sync_tile(),

        ugfx_sync_full(),
        ugfx_finalize(),
    };
    data_cache_hit_writeback(commands, sizeof(commands));
    ugfx_load(commands, sizeof(commands) / sizeof(*commands));
    rsp_run();

    render_commands = ugfx_buffer_new(2048);

    set_game_mode();
    video_has_initialised = true;

    return video_has_initialised;
}

void video_shutdown()
{
    ugfx_close();
    ugfx_buffer_free(render_commands);
    free(_palette1);
    free(_palette2);
    free(game_surface.format->palette);
    free(game_surface.format);
    free(game_surface.pixels);
    free(text_surface.format->palette);
    free(text_surface.format);
    free(text_surface.pixels);
}

void set_text_mode()
{
    if (!is_game_mode)
    {
        return;
    }
    is_game_mode = false;
}

void set_game_mode()
{
    if (is_game_mode)
    {
        return;
    }
    is_game_mode = true;
}

void video_update()
{
    VideoSurface *src = is_game_mode ? &game_surface : &text_surface;
    uint8_t pal_slot = is_game_mode ? GAME_PALETTE : TEXT_PALETTE;
    data_cache_hit_writeback_invalidate(src->pixels, src->w * src->h);

    //We draw the screen from top to bottom.
    //We can only draw 2048bytes per loop and for simplicity we want it to be a multiple
    //of the width.
    int x_per_loop = src->w;
    int y_per_loop = (is_game_mode) ? 5 : 1;
    int current_y = 0;
    int chunk_size = x_per_loop * y_per_loop;
    assert(chunk_size <= 2048);

    while (!(disp = display_lock()));
    ugfx_buffer_clear(render_commands);
    ugfx_buffer_push(render_commands, ugfx_set_display(disp));
    ugfx_buffer_push(render_commands, ugfx_set_scissor(0, 0, display_width << 2, display_height << 2, UGFX_SCISSOR_DEFAULT));
    ugfx_buffer_push(render_commands, ugfx_set_other_modes(UGFX_CYCLE_COPY | UGFX_TLUT_RGBA16));
    ugfx_buffer_push(render_commands, ugfx_sync_pipe());

    if (palette_dirty)
    {
        ugfx_buffer_push(render_commands, ugfx_set_tile(UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_4B, 16, 0x800 + (GAME_PALETTE * 0x100), 2, 0, 0, 0, 0, 0, 0, 0, 0, 0));
        ugfx_buffer_push(render_commands, ugfx_set_texture_image((uint32_t)_palette1, UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_16B, 16 - 1));
        ugfx_buffer_push(render_commands, ugfx_load_tlut(0 << 2, 0, 15 << 2, 0, 2));
        ugfx_buffer_push(render_commands, ugfx_sync_tile());
        palette_dirty = false;
    }

    uint8_t *ptr = src->pixels;
    while (current_y < 240)
    {
        //Load the 8bit texture into tile 0:
        ugfx_buffer_push(render_commands, ugfx_set_tile(UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_8B, x_per_loop / 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
        ugfx_buffer_push(render_commands, ugfx_set_texture_image((uint32_t)ptr, UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_8B, x_per_loop - 1));
        ugfx_buffer_push(render_commands, ugfx_load_tile(0 << 2, 0 << 2, (x_per_loop - 1) << 2, (y_per_loop - 1) << 2, 0));
        ugfx_buffer_push(render_commands, ugfx_sync_tile());

        //Apply the palette onto the loaded tile, then set it to tile 1:
        ugfx_buffer_push(render_commands, ugfx_set_tile(UGFX_FORMAT_INDEX, UGFX_PIXEL_SIZE_8B, x_per_loop / 8, 0, 1, pal_slot, 0, 0, 0, 0, 0, 0, 0, 0));
        ugfx_buffer_push(render_commands, ugfx_set_tile_size(0, 0, (x_per_loop - 1) << 2, (y_per_loop - 1) << 2, 1));

        //Draw a line from tile 1
        ugfx_buffer_push(render_commands, ugfx_texture_rectangle(1, 0, current_y << 2, x_per_loop << 2, (current_y + 1) << 2));
        ugfx_buffer_push(render_commands, ugfx_texture_rectangle_tcoords(0 << 5, 0 << 5, 4 << 10, 1 << 10));
        current_y++;

        if (is_game_mode)
        {
            //Can draw 5 lines at once at this games standard surface width, note the 1st line is drawn twice so that over 200 lines it is scaled to 240)
            ugfx_buffer_push(render_commands, ugfx_texture_rectangle(1, 0, (current_y) << 2, x_per_loop << 2, (current_y + y_per_loop - 1) << 2));
            ugfx_buffer_push(render_commands, ugfx_texture_rectangle_tcoords(0 << 5, 0 << 5, 4 << 10, 1 << 10));
            current_y += y_per_loop;
        }
        ptr += chunk_size;
    }

    ugfx_buffer_push(render_commands, ugfx_sync_pipe());
    ugfx_buffer_push(render_commands, ugfx_sync_full());
    ugfx_buffer_push(render_commands, ugfx_finalize());

    data_cache_hit_writeback(ugfx_buffer_data(render_commands), ugfx_buffer_length(render_commands) * sizeof(ugfx_command_t));
    ugfx_load(ugfx_buffer_data(render_commands), ugfx_buffer_length(render_commands));

    rsp_run_async();
    SDL_Delay(0); //Pump audio backend update
    rsp_wait();
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
    data_cache_hit_writeback_invalidate(_palette1, sizeof(uint16_t) * 16);
    palette_dirty =true;
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
    return;
}

void video_set_scale_factor(int scale_factor)
{
    return;
}
