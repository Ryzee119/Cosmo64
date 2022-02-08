// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <stdio.h>
#include "sound/audio.h"
#include "sound/music.h"
#include "game.h"
#include "map.h"
#include "dialog.h"
#include "video.h"
#include "status.h"
#include "config.h"
#include "high_scores.h"
#include "demo.h"
#include "b800.h"
#include "input.h"

extern char *save_directory;
void cosmo_audio_init();
int cleanup_and_exit();

typedef struct sram_files_t
{
    const char *name;
    uint32_t size;
    uint32_t offset; //Track position of the file cursor
} sram_files_t;
int sramfs_init(sram_files_t *files, int num_files);

#define MAX_SRAM_FILES 10
#ifdef EP3
static sram_files_t sram_files[MAX_SRAM_FILES] = {
    {"COSMO3.CFG", 256, 0},
    {"COSMO3.SV1", 128, 0},
    {"COSMO3.SV2", 128, 0},
    {"COSMO3.SV3", 128, 0},
    {"COSMO3.SV4", 128, 0},
    {"COSMO3.SV5", 128, 0},
    {"COSMO3.SV6", 128, 0},
    {"COSMO3.SV7", 128, 0},
    {"COSMO3.SV8", 128, 0},
    {"COSMO3.SV9", 128, 0},
};
#elif EP2
static sram_files_t sram_files[MAX_SRAM_FILES] = {
    {"COSMO2.CFG", 256, 0},
    {"COSMO2.SV1", 128, 0},
    {"COSMO2.SV2", 128, 0},
    {"COSMO2.SV3", 128, 0},
    {"COSMO2.SV4", 128, 0},
    {"COSMO2.SV5", 128, 0},
    {"COSMO2.SV6", 128, 0},
    {"COSMO2.SV7", 128, 0},
    {"COSMO2.SV8", 128, 0},
    {"COSMO2.SV9", 128, 0},
};
#else
static sram_files_t sram_files[MAX_SRAM_FILES] = {
    {"COSMO1.CFG", 256, 0},
    {"COSMO1.SV1", 128, 0},
    {"COSMO1.SV2", 128, 0},
    {"COSMO1.SV3", 128, 0},
    {"COSMO1.SV4", 128, 0},
    {"COSMO1.SV5", 128, 0},
    {"COSMO1.SV6", 128, 0},
    {"COSMO1.SV7", 128, 0},
    {"COSMO1.SV8", 128, 0},
    {"COSMO1.SV9", 128, 0},
};
#endif

int main(void)
{
    debug_init(DEBUG_FEATURE_LOG_ISVIEWER);
    dfs_init(DFS_DEFAULT_LOCATION);

    save_directory = malloc(32);
    strcpy(save_directory, "sram:/");
    load_config_from_command_line(0, NULL);

    sramfs_init(sram_files, MAX_SRAM_FILES);

    #ifdef EP3
	set_episode_number(3);
	#elif EP2
	set_episode_number(2);
	#else
	set_episode_number(1);
	#endif

    video_init();
    cosmo_audio_init();
    input_init();
    game_init();

    video_fill_screen_with_black();

    if (!is_quick_start())
    {
        a_game_by_dialog();
        game_play_mode = main_menu();
    }
    else
    {
        set_initial_game_state();
        game_play_mode = PLAY_GAME;
    }

    while (game_play_mode != QUIT_GAME)
    {
        load_level(current_level);

        if (game_play_mode == PLAY_DEMO)
        {
            load_demo();
        }

        game_loop();
        stop_music();
        if (game_play_mode == PLAY_GAME)
        {
            show_high_scores();
        }
        game_play_mode = main_menu();
    }

    stop_music();
    display_exit_text();

    return cleanup_and_exit();
}

int cleanup_and_exit()
{
    write_config_file();
    config_cleanup();
    video_shutdown();
    audio_shutdown();
    input_shutdown();
    return 0;
}
