   
// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include "sound/audio.h"
#include "sound/music.h"

#define AUDIO_DESIRED_SAMPLE_RATE 22050
#define AUDIO_DESIRED_NUM_CHANNELS 1
static timer_link_t *audio_timer;
void sfx_close(void);
void music_close(void);

AudioConfig audioConfig;

void cosmo_audio_init()
{
    if (audioConfig.enabled == true)
    {
        return;
    }

    audio_init(AUDIO_DESIRED_SAMPLE_RATE, 4);
	mixer_init(16);  // Initialize up to 16 channels
    timer_init();
    audioConfig.bytesPerSample = 2;
    audioConfig.numChannels = 1;
    audioConfig.sampleRate = AUDIO_DESIRED_SAMPLE_RATE;
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

    delete_timer(audio_timer);
    sfx_close();
    music_close();
    audio_close();
    audioConfig.enabled = false;
}
