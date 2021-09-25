// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <mikmod.h>
#include <malloc.h>
#include <string.h>
#include "sound/sfx.h"
#include "sound/audio.h"
#include "files/file.h"
#include "game.h"

#define _swap16(x) ((uint16_t)(((uint16_t)(x) << 8) | ((uint16_t)(x) >> 8)))
#define _swap32(x) ((uint32_t)(((uint32_t)(x) << 24) | (((uint32_t)(x) << 8) & 0x00FF0000) | \
                   (((uint32_t)(x) >> 8) & 0x0000FF00) | ((uint32_t)(x) >> 24)))
#define SFX_SAMPLE_RATE 140
#define PC_PIT_RATE 1193181
#define WAVE_AMPLITUDE_VALUE 3500

typedef struct Sfx
{
    uint8 priority;
    uint32_t alen;
    uint8_t *abuf;
    MREADER *mem_reader;
    SAMPLE *sample;
    uint32_t voice;
} Sfx;

Sfx *sfxs;
uint8 num_sfx = 0;
uint8 sfx_on_flag = 1;

typedef struct _RAMREADER
{
    MREADER core;
    const void *buffer;
    long len;
    long pos;
} RAMREADER;

typedef struct wav_header
{
    // RIFF Header
    char riff_header[4];
    int wav_size;
    char wave_header[4];

    // Format Header
    char fmt_header[4];
    int fmt_chunk_size;
    short audio_format;
    short num_channels;
    int sample_rate;
    int byte_rate;
    short sample_alignment;
    short bit_depth;

    // Data
    char data_header[4];
    int data_bytes;
} wav_header;

static BOOL _reader_Eof(MREADER *reader)
{
    RAMREADER *mr = (RAMREADER *)reader;
    if (!mr)
        return 1;
    if (mr->pos >= mr->len)
        return 1;
    return 0;
}

static BOOL _reader_Read(MREADER *reader, void *ptr, size_t size)
{
    unsigned char *d;
    const unsigned char *s;
    RAMREADER *mr;
    long siz;
    BOOL ret;

    if (!reader || !size)
        return 0;

    mr = (RAMREADER *)reader;
    siz = (long)size;
    if (mr->pos >= mr->len)
        return 0; /* @ eof */
    if (mr->pos + siz > mr->len)
    {
        siz = mr->len - mr->pos;
        ret = 0; /* not enough remaining bytes */
    }
    else
    {
        ret = 1;
    }

    s = (const unsigned char *)mr->buffer;
    s += mr->pos;
    mr->pos += siz;
    d = (unsigned char *)ptr;

    while (siz)
    {
        *d++ = *s++;
        siz--;
    }

    return ret;
}

static int _reader_Get(MREADER *reader)
{
    RAMREADER *mr;
    int c;

    mr = (RAMREADER *)reader;
    if (mr->pos >= mr->len)
        return EOF;
    c = ((const unsigned char *)mr->buffer)[mr->pos];
    mr->pos++;

    return c;
}

static int _reader_Seek(MREADER *reader, long offset, int whence)
{
    RAMREADER *mr;

    if (!reader)
        return -1;
    mr = (RAMREADER *)reader;
    switch (whence)
    {
    case SEEK_CUR:
        mr->pos += offset;
        break;
    case SEEK_SET:
        mr->pos = reader->iobase + offset;
        break;
    case SEEK_END:
        mr->pos = mr->len + offset;
        break;
    default: /* invalid */
        return -1;
    }
    if (mr->pos < reader->iobase)
    {
        mr->pos = mr->core.iobase;
        return -1;
    }
    if (mr->pos > mr->len)
    {
        mr->pos = mr->len;
    }
    return 0;
}

static long _reader_Tell(MREADER *reader)
{
    if (reader)
    {
        return ((RAMREADER *)reader)->pos - reader->iobase;
    }
    return 0;
}

MREADER *my_new_mem_reader(const void *buffer, long len)
{
    RAMREADER *reader = (RAMREADER *)calloc(1, sizeof(RAMREADER));
    if (reader)
    {
        reader->core.Eof = &_reader_Eof;
        reader->core.Read = &_reader_Read;
        reader->core.Get = &_reader_Get;
        reader->core.Seek = &_reader_Seek;
        reader->core.Tell = &_reader_Tell;
        reader->buffer = buffer;
        reader->len = len;
        reader->pos = 0;
    }
    return (MREADER *)reader;
}

static int get_num_sfx(const char *filename)
{
    File file;
    open_file(filename, &file);
    file_seek(&file, 6);
    int count = file_read2(&file);
    file_close(&file);
    return count;
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
    if (audioConfig.format == AUDIO_FLOAT32_SIGNED_LSB)
    {
        *(float *)(buf + index * audioConfig.numChannels * audioConfig.bytesPerSample) = (float)sample / 234234;
        if (audioConfig.numChannels == 2)
        {
            *(float *)(buf + index * audioConfig.numChannels * audioConfig.bytesPerSample + audioConfig.bytesPerSample) = (float)sample / 234234;
        }
    }
    else
    {
        *(sint16 *)(buf + index * audioConfig.numChannels * audioConfig.bytesPerSample) = sample;
        if (audioConfig.numChannels == 2)
        {
            *(sint16 *)(buf + index * audioConfig.numChannels * audioConfig.bytesPerSample + audioConfig.bytesPerSample) = sample;
        }
    }
}

static Sfx *convert_sfx_to_wave(Sfx *chunk, File *file, int offset, int num_samples)
{
    int sample_length = (audioConfig.sampleRate / SFX_SAMPLE_RATE);
    chunk->alen = (uint32_t)(num_samples * sample_length * audioConfig.numChannels * audioConfig.bytesPerSample);
    chunk->abuf = (uint8_t *)malloc(sizeof(wav_header) + chunk->alen);
    assert(chunk->abuf != NULL);

    file_seek(file, offset);

    int16_t *wave_data = (int16_t *)((uint32_t)chunk->abuf + sizeof(wav_header));

    int16_t beepWaveVal = WAVE_AMPLITUDE_VALUE;
    uint16_t beepHalfCycleCounter = 0;
    for (int i = 0; i < num_samples; i++)
    {
        uint16_t sample = file_read2(file);
        if (sample)
        {
            float freq = PC_PIT_RATE / (float)sample;
            int half_cycle_length = (int)(audioConfig.sampleRate / (freq * 2));
            for (int sampleCounter = 0; sampleCounter < sample_length; sampleCounter++)
            {
                writeSample((uint8_t *)wave_data, i * sample_length + sampleCounter, beepWaveVal);
                beepHalfCycleCounter++;
                if (beepHalfCycleCounter >= half_cycle_length)
                {
                    beepHalfCycleCounter = half_cycle_length != 0 ?
                                           (uint16_t)(beepHalfCycleCounter % half_cycle_length) : (uint16)0;
                    beepWaveVal = -beepWaveVal;
                }
            }
        }
        else
        {
            memset(&wave_data[i * sample_length * audioConfig.numChannels], 0, sample_length * audioConfig.numChannels * audioConfig.bytesPerSample); //silence
        }
    }

    wav_header *header = (wav_header *)chunk->abuf;
    strcpy(header->riff_header, "RIFF");
    strcpy(header->wave_header, "WAVE");
    strcpy(header->fmt_header, "fmt ");
    strcpy(header->data_header, "data");
    header->wav_size = _swap32(chunk->alen + sizeof(wav_header) - 8);
    header->fmt_chunk_size = _swap32(16);
    header->audio_format = _swap16(1); //Should be 1 for PCM.
    header->num_channels = _swap16(audioConfig.numChannels);
    header->sample_rate = _swap32(audioConfig.sampleRate);
    header->byte_rate = _swap32(audioConfig.sampleRate * audioConfig.numChannels * audioConfig.bytesPerSample);
    header->sample_alignment = _swap16(audioConfig.numChannels * audioConfig.bytesPerSample);
    header->bit_depth = _swap16(16);
    header->data_bytes = _swap32(chunk->alen);
    chunk->alen += sizeof(wav_header);
    chunk->alen += sizeof(wav_header);
    return chunk;
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
        sfx->sample = NULL;
        int num_samples = get_num_samples(&file, offset, i, count);
        convert_sfx_to_wave(sfx, &file, offset, num_samples);
        sfx->mem_reader = my_new_mem_reader(sfx->abuf, sfx->alen);
        sfx->sample = Sample_LoadGeneric(sfx->mem_reader);
        if (!sfx->sample)
        {
            debugf("Error loading  %d\n", sfx_offset + i);
            assert(0);
        }
            
    }
    file_close(&file);
    return MAX_SAMPLES_PER_FILE;
}

void load_sfx()
{
    num_sfx = 0;
    num_sfx += get_num_sfx("SOUNDS.MNI");
    num_sfx += get_num_sfx("SOUNDS2.MNI");
    num_sfx += get_num_sfx("SOUNDS3.MNI");

    sfxs = (Sfx *)malloc(sizeof(Sfx) * num_sfx);

    int sfx_offset = load_sfx_file("SOUNDS.MNI", 0);
    sfx_offset += load_sfx_file("SOUNDS2.MNI", sfx_offset);
    load_sfx_file("SOUNDS3.MNI", sfx_offset);
}

void play_sfx(int sfx_number)
{
    if (!sfx_number) return;

    sfx_number--;
    if (sfxs[sfx_number].sample)
    {
        sfxs[sfx_number].voice = Sample_Play(sfxs[sfx_number].sample, 0, 0);
    }
}

void sfx_close()
{

}
