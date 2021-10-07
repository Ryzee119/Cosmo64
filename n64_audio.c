   
// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include "sound/audio.h"
#include "sound/music.h"

#define AUDIO_DESIRED_SAMPLE_RATE 44100
#define AUDIO_DESIRED_NUM_CHANNELS 2
#define AUDIO_BYTES_PER_SAMPLE 2
void sfx_close(void);
void music_close(void);

AudioConfig audioConfig;

void cosmo_audio_init()
{
    if (audioConfig.enabled == true)
    {
        return;
    }

    audio_init(AUDIO_DESIRED_SAMPLE_RATE, 2);
    mixer_init(16);  // Initialize up to 16 channels
    timer_init();
    audioConfig.format = AUDIO_INT16_SIGNED_LSB;
    audioConfig.enabled = true;
    music_init();
}

void audio_shutdown()
{
    if (audioConfig.enabled == false)
    {
        return;
    }

    sfx_close();
    music_close();
    audio_close();
    audioConfig.enabled = false;
}
