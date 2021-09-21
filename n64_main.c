// SPDX-License-Identifier: GPL-2.0

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

void cosmo_audio_init();
int cleanup_and_exit();

int main(void)
{
    load_config_from_command_line(0, NULL);

    set_episode_number(1);

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
