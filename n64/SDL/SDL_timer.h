#ifndef _SDL_timer_h
#define _SDL_timer_h

#include <libdragon.h>
#include "SDL.h"

static inline Uint32 SDL_GetTicks()
{
    return get_ticks_ms();
}

static inline void SDL_Delay(Uint32 ms)
{
    return wait_ms(ms);
}

#endif