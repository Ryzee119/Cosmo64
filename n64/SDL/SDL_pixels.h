#ifndef SDL_pixels_h_
#define SDL_pixels_h_

#include "SDL_stdinc.h"

typedef struct SDL_Color
{
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
} SDL_Color;
#define SDL_Colour SDL_Color

typedef struct SDL_Palette
{
    int ncolors;
    uint16_t colors[16];
    Uint32 version;
    int refcount;
} SDL_Palette;

typedef struct SDL_PixelFormat
{
    Uint32 format;
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 padding[2];
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    Uint32 Amask;
    Uint8 Rloss;
    Uint8 Gloss;
    Uint8 Bloss;
    Uint8 Aloss;
    Uint8 Rshift;
    Uint8 Gshift;
    Uint8 Bshift;
    Uint8 Ashift;
    int refcount;
    struct SDL_PixelFormat *next;
} SDL_PixelFormat;

__attribute__((unused)) //Why do I need this?
static int SDL_SetPaletteColors(SDL_Palette *palette,
                                const SDL_Color *colors,
                                int firstcolor, int ncolors)
{
    if (!palette)
    {
        return -1;
    }

    uint8_t r, g, b;
    for (int i = firstcolor; i < (firstcolor + ncolors); i++)
    {
        r = colors[i - firstcolor].r;
        g = colors[i - firstcolor].g;
        b = colors[i - firstcolor].b;
        uint16_t c = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 0x01; //rgba 5551
        palette->colors[i] = c;
    }
    ++palette->version;
    return 0;
}

#endif
