// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <mikmod.h>
#include "sound/music.h"
#include "sound/audio.h"
#include "config.h"

uint8 music_on_flag = 1;
uint8 *music_data;
uint32 music_data_length;

static MODULE *bgm = NULL;

const char music_filename_tbl[][13] = {
    "MCAVES.IMF",
    "MSCARRY.IMF",
    "MBOSS.IMF",
    "MRUNAWAY.IMF",
    "MCIRCUS.IMF",
    "MTEKWRD.IMF",
    "MEASYLEV.IMF",
    "MROCKIT.IMF",
    "MHAPPY.IMF",
    "MDEVO.IMF",
    "MDADODA.IMF",
    "MBELLS.IMF",
    "MDRUMS.IMF",
    "MBANJO.IMF",
    "MEASY2.IMF",
    "MTECK2.IMF",
    "MTECK3.IMF",
    "MTECK4.IMF",
    "MZZTOP.IMF"};

void load_music(uint16 new_music_index)
{
    if (bgm)
    {
        Player_Stop();
        Player_Free(bgm);
    }
    char *path = get_game_dir_full_path(music_filename_tbl[new_music_index]);
    bgm = Player_Load(path, 127, 0);
    play_music();
    free(path);
}

void music_init()
{
}

void stop_music()
{
    Player_Stop();
    Player_Free(bgm);
    bgm = NULL;
}

void play_music()
{
    Player_Start(bgm);
    Player_SetVolume(128);
}

void music_close()
{
    
}