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
    unsigned long long timeout = timer_ticks() + TICKS_FROM_MS(ms);
    do
    {
        if (audio_can_write())
        {
            short *buf = audio_write_begin();
            mixer_poll(buf, audio_get_buffer_length());
            audio_write_end();
        }
    } while (timer_ticks() < timeout);
}

#endif