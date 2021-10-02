// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include "game.h"
#include "sound/music.h"
#include "sound/audio.h"
#include "sound/opl.h"
#include "config.h"

#define MUSIC_INSTRUCTION_RATE 560 //Hz
#define ADLIB_OP_SIZE 4
#define MUSIC_NUM_CHANNELS 1
#define MUSIC_BYTES_PER_SAMPLE 2
#define MUSIC_SAMPLE_RATE 9600
#define MUSIC_CHANNEL 0

uint8 music_on_flag = 1;
static waveform_t music;
static uint8 *music_data = NULL;
static uint32 music_data_length;
static sint8 music_index = -1;
static uint32 adlib_instruction_position = 0;
static uint32 delay_counter = 0;

static const char music_filename_tbl[][13] = {
    "MCAVES.MNI",
    "MSCARRY.MNI",
    "MBOSS.MNI",
    "MRUNAWAY.MNI",
    "MCIRCUS.MNI",
    "MTEKWRD.MNI",
    "MEASYLEV.MNI",
    "MROCKIT.MNI",
    "MHAPPY.MNI",
    "MDEVO.MNI",
    "MDADODA.MNI",
    "MBELLS.MNI",
    "MDRUMS.MNI",
    "MBANJO.MNI",
    "MEASY2.MNI",
    "MTECK2.MNI",
    "MTECK3.MNI",
    "MTECK4.MNI",
    "MZZTOP.MNI"};

static uint32 get_delay(uint32 instruction_num)
{
    return (MUSIC_SAMPLE_RATE / MUSIC_INSTRUCTION_RATE) *
           (uint32)((uint16)music_data[instruction_num * ADLIB_OP_SIZE + 2] +
                    ((uint16)music_data[instruction_num * ADLIB_OP_SIZE + 3] << 8));
}

static void generate_music(Uint8 *stream, int len)
{
    int num_samples = len / MUSIC_NUM_CHANNELS / MUSIC_BYTES_PER_SAMPLE;
    uint8 is_stereo = MUSIC_NUM_CHANNELS == 2 ? 1 : 0;

    for (int i = num_samples; i > 0;)
    {
        if (delay_counter == 0)
        {
            adlib_write(music_data[adlib_instruction_position * ADLIB_OP_SIZE], music_data[adlib_instruction_position * ADLIB_OP_SIZE + 1]);
            delay_counter = get_delay(adlib_instruction_position);
            adlib_instruction_position++;
            if (adlib_instruction_position * ADLIB_OP_SIZE >= music_data_length)
            {
                adlib_instruction_position = 0;
            }
        }
        if (delay_counter > i)
        {
            delay_counter -= i;
            adlib_getsample(stream, i, is_stereo, audioConfig.format);
            return;
        }
        if (delay_counter <= i)
        {
            i -= delay_counter;
            adlib_getsample(stream, delay_counter, is_stereo, audioConfig.format);
            stream += delay_counter * MUSIC_NUM_CHANNELS * MUSIC_BYTES_PER_SAMPLE;
            delay_counter = 0;
        }
    }
}

static void music_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx;
    uint8_t *dst = CachedAddr(samplebuffer_append(sbuf, wlen));
    generate_music(dst, wlen * 2);
    data_cache_hit_writeback_invalidate(dst, 2 * wlen);
}

void load_music(uint16 new_music_index)
{
    if (!audioConfig.enabled)
    {
        return;
    }

    adlib_instruction_position = 0;
    delay_counter = 0;

    if (music_index != -1)
    {
        stop_music();
    }

    if (music_index == new_music_index)
    {
        play_music();
        return;
    }

    music_index = new_music_index;
    music_data = load_file_in_new_buf(music_filename_tbl[music_index], &music_data_length);
    assert(music_data != NULL);
    play_music();
}

void music_init()
{
    generator_add = 0; //Fixes warning of unused variable in opl.h
    adlib_init(MUSIC_SAMPLE_RATE);
}

void stop_music()
{
    mixer_ch_stop(MUSIC_CHANNEL);
    music_index = -1;
}

void play_music()
{
    music.bits = 16;
    music.channels = 1;
    music.frequency = MUSIC_SAMPLE_RATE;
    music.len = 0;
    music.read = music_read;
    music.loop_len = WAVEFORM_UNKNOWN_LEN;
    music.ctx = (void *)&music;
    mixer_ch_play(MUSIC_CHANNEL, &music);
}

void music_close()
{
    stop_music();
}
