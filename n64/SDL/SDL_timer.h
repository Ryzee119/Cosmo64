#ifndef _SDL_timer_h
#define _SDL_timer_h

#include <libdragon.h>
#include "SDL.h"

static inline Uint32 SDL_GetTicks()
{
    return timer_ticks() / (TICKS_PER_SECOND / 1000);
}

static inline void SDL_Delay(Uint32 ms)
{
    return wait_ms(ms);
}

#endif