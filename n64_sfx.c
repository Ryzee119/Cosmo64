// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <string.h>
#include <stdlib.h>
#include "sound/sfx.h"
#include "sound/audio.h"
#include "files/file.h"
#include "game.h"

#define SFX_SAMPLE_RATE 140
#define PC_PIT_RATE 1193181
#define WAVE_AMPLITUDE_VALUE 3500

#define SFX_NUM_CHANNELS 1
#define SFX_BYTES_PER_SAMPLE 2
#define SFX_AUDIO_SAMPLE_RATE 22050
#define SFX_CHANNELS 15

uint8 sfx_on_flag = 1;

typedef struct Sfx
{
    uint8 priority;
    waveform_t wave;
    uint32_t alen;
    int16_t *abuf;
} Sfx;
static Sfx sfxs[MAX_SAMPLES_PER_FILE * 3];

static void sfx_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    Sfx *_sfx = (Sfx *)ctx;
    int16_t *dst = CachedAddr(samplebuffer_append(sbuf, wlen));
    int16_t *src = &_sfx->abuf[wpos];
    for (int i = 0; i < wlen; i++)
    {
        dst[i] = src[i];
    }
    data_cache_hit_writeback_invalidate(dst, SFX_BYTES_PER_SAMPLE * wlen);
}

static int get_num_samples(File *file, int offset, int index, int total)
{
    if (index < total - 1)
    {
        file_seek(file, (index + 2) * 16);
        int next_offset = file_read2(file);
        return ((next_offset - offset) / 2) - 1;
    }
    return ((file_get_filesize(file) - offset) / 2) - 1;
}

static void writeSample(uint8_t *buf, uint16_t index, int16_t sample)
{
    *(sint16 *)(buf + index * audioConfig.numChannels * audioConfig.bytesPerSample) = sample;
    if (audioConfig.numChannels == 2)
    {
        *(sint16 *)(buf + index * audioConfig.numChannels * audioConfig.bytesPerSample + audioConfig.bytesPerSample) = sample;
    }
}

static void convert_sfx_to_wave(Sfx *chunk, File *file, int offset, int num_samples)
{
    int sample_length = (SFX_AUDIO_SAMPLE_RATE / SFX_SAMPLE_RATE);
    chunk->alen = num_samples * sample_length * SFX_NUM_CHANNELS;
    chunk->abuf = (int16_t *)malloc(chunk->alen * SFX_BYTES_PER_SAMPLE);
    assert(chunk->abuf != NULL);

    file_seek(file, offset);

    int16_t *wave_data = chunk->abuf;

    int16_t beepWaveVal = WAVE_AMPLITUDE_VALUE;
    uint16_t beepHalfCycleCounter = 0;
    for (int i = 0; i < num_samples; i++)
    {
        uint16_t sample = file_read2(file);
        if (sample)
        {
            int freq = PC_PIT_RATE * 2 / sample;
            int half_cycle_length = (int)(SFX_AUDIO_SAMPLE_RATE / freq);
            for (int sampleCounter = 0; sampleCounter < sample_length; sampleCounter++)
            {
                writeSample((uint8_t *)wave_data, i * sample_length + sampleCounter, beepWaveVal);
                beepHalfCycleCounter++;
                if (beepHalfCycleCounter >= half_cycle_length)
                {
                    beepHalfCycleCounter = half_cycle_length != 0 ? (uint16_t)(beepHalfCycleCounter % half_cycle_length) : (uint16)0;
                    beepWaveVal = -beepWaveVal;
                }
            }
        }
        else
        {
            memset(&wave_data[i * sample_length * SFX_NUM_CHANNELS], 0, sample_length * SFX_NUM_CHANNELS * SFX_BYTES_PER_SAMPLE); //silence
        }
    }
    waveform_t *header = &chunk->wave;
    header->bits = SFX_BYTES_PER_SAMPLE * 8;
    header->channels = 1;
    header->frequency = SFX_AUDIO_SAMPLE_RATE;
    header->len = chunk->alen;
    header->loop_len = 0;
    header->read = sfx_read;
    header->ctx = (void *)chunk;
}

static int load_sfx_file(const char *filename, int sfx_offset)
{
    File file;
    open_file(filename, &file);
    file_seek(&file, 6);
    int count = file_read2(&file);
    for (int i = 0; i < MAX_SAMPLES_PER_FILE; i++)
    {
        file_seek(&file, (i + 1) * 16); //+1 to skip header.
        int offset = file_read2(&file);
        Sfx *sfx = &sfxs[sfx_offset + i];
        sfx->priority = file_read1(&file);
        int num_samples = get_num_samples(&file, offset, i, count);
        convert_sfx_to_wave(sfx, &file, offset, num_samples);
    }
    file_close(&file);
    return MAX_SAMPLES_PER_FILE;
}

void load_sfx()
{
    int sfx_offset = load_sfx_file("SOUNDS.MNI", 0);
    sfx_offset += load_sfx_file("SOUNDS2.MNI", sfx_offset);
    load_sfx_file("SOUNDS3.MNI", sfx_offset);
}

void play_sfx(int sfx_number)
{
    if (!sfx_number)
        return;

    sfx_number--;
    for (int channel = 1; channel < SFX_CHANNELS; channel++)
    {
        if (!mixer_ch_playing(channel))
        {
            mixer_ch_play(channel, &sfxs[sfx_number].wave);
            return;
        }
    }
}

void sfx_close()
{
    for (int channel = 1; channel < SFX_CHANNELS; channel++)
    {
        mixer_ch_stop(channel);
    }
    for (int i = 0; i < MAX_SAMPLES_PER_FILE * 3; i++)
    {
        if (sfxs[i].abuf)
        {
            free(sfxs[i].abuf);
        }
    }
}
