   
// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <mikmod.h>
#include "sound/audio.h"
#include "sound/music.h"

#define AUDIO_DESIRED_SAMPLE_RATE 22050
#define AUDIO_DESIRED_NUM_CHANNELS 1
MIKMODAPI extern UWORD md_mode __attribute__((section (".data")));
MIKMODAPI extern UWORD md_mixfreq __attribute__((section (".data")));
static timer_link_t *audio_timer;
void sfx_close(void);
void music_close(void);

AudioConfig audioConfig;

static void audio_service(int ovfl)
{
    if (audio_can_write())
    {
        MikMod_Update();
    }
}

void cosmo_audio_init()
{
    if (audioConfig.enabled == true)
    {
        return;
    }

    audio_init(AUDIO_DESIRED_SAMPLE_RATE, 2);
    timer_init();
    audioConfig.bytesPerSample = 2;
    audioConfig.numChannels = 1;
    audioConfig.sampleRate = AUDIO_DESIRED_SAMPLE_RATE;
    audioConfig.format = AUDIO_INT16_SIGNED_LSB;
    audioConfig.enabled = true;
    MikMod_RegisterAllDrivers();
    MikMod_RegisterAllLoaders();
    md_mode &= ~DMODE_SURROUND;
    md_mode |= DMODE_STEREO | DMODE_16BITS | DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX;
    md_mixfreq = AUDIO_DESIRED_SAMPLE_RATE;
    MikMod_Init("");
    MikMod_SetNumVoices(-1, 5);
    MikMod_EnableOutput();

    audio_timer = new_timer(TIMER_TICKS(1000000 / 60), TF_CONTINUOUS, audio_service);

    music_init();
}

void audio_shutdown()
{
    if (audioConfig.enabled == false)
    {
        return;
    }

    delete_timer(audio_timer);
    MikMod_DisableOutput();
    sfx_close();
    music_close();
    audio_close();
    MikMod_Exit();
    audioConfig.enabled = false;
}
