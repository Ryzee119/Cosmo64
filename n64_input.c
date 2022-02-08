// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <SDL_timer.h>
#include "input.h"
#include "dialog.h"
#include "demo.h"

SDL_Keycode cfg_up_key = SDLK_UP;
SDL_Keycode cfg_down_key = SDLK_DOWN;
SDL_Keycode cfg_left_key = SDLK_LEFT;
SDL_Keycode cfg_right_key = SDLK_RIGHT;
SDL_Keycode cfg_jump_key = SDLK_a;
SDL_Keycode cfg_bomb_key = SDLK_b;

uint8 bomb_key_pressed = 0;
uint8 jump_key_pressed = 0;
uint8 up_key_pressed = 0;
uint8 down_key_pressed = 0;
uint8 left_key_pressed = 0;
uint8 right_key_pressed = 0;

//This is needed because the game manipulates up_key_pressed as part of the hover board logic. This is the actual
//key pressed state.
uint8 input_up_key_pressed = 0;

//modifies the left, right and jump key presses TODO this isn't wired up yet. It might disable player input.
uint8 byte_2E17C;

bool input_init()
{
    controller_init();
    reset_player_control_inputs();
    return true;
}

void input_shutdown()
{
}

void wait_for_time_or_key(int delay_in_game_cycles)
{
    struct controller_data keys;
    reset_player_control_inputs();

    uint32_t timeout = get_ticks_ms() + (8 * delay_in_game_cycles);
    while (get_ticks_ms() < timeout)
    {
        controller_scan();
        keys = get_keys_pressed();
        if (keys.c->data & 0xFFFFFF00)
        {
            return;
        }
    }
}

input_state_enum read_input()
{
    if(game_play_mode == PLAY_DEMO)
    {
        if(poll_for_key_press(false) != SDLK_UNKNOWN || read_input_from_demo())
        {
            return QUIT;
        }
        return CONTINUE;
    }

    controller_scan();
    struct controller_data keys;

    keys = get_keys_pressed();
    if(keys.c[0].err) return CONTINUE;
    if (keys.c[0].L && keys.c[0].R) return QUIT;

    up_key_pressed = keys.c[0].up;
    input_up_key_pressed = keys.c[0].up;
    down_key_pressed = keys.c[0].down;
    left_key_pressed = keys.c[0].left;
    right_key_pressed = keys.c[0].right;
    jump_key_pressed = keys.c[0].A;
    bomb_key_pressed = keys.c[0].B;

    if (keys.c[0].x >  20) right_key_pressed = 1;
    if (keys.c[0].x < -20) left_key_pressed = 1;
    if (keys.c[0].y >  20) up_key_pressed = 1;
    if (keys.c[0].y >  20) input_up_key_pressed = 1;
    if (keys.c[0].y < -20) down_key_pressed = 1;

    keys = get_keys_down();
    if (keys.c[0].start)
    {
        switch(help_menu_dialog())
        {
            case 0 : break;
            case 1 : break;
            case 2 : return QUIT;
            default : break;
        }
    }
    return CONTINUE;
}

void reset_player_control_inputs()
{
    up_key_pressed = 0;
    input_up_key_pressed = 0;
    down_key_pressed = 0;
    left_key_pressed = 0;
    right_key_pressed = 0;
    bomb_key_pressed = 0;
    jump_key_pressed = 0;
}

SDL_Keycode poll_for_key_press(bool allow_key_repeat)
{
    controller_scan();
    struct controller_data keys = get_keys_down();
    if (keys.c[0].err)
    {
        return SDLK_UNKNOWN;
    }
    if (keys.c[0].start) return SDLK_RETURN;
    if (keys.c[0].A)     return SDLK_RETURN;
    if (keys.c[0].B)     return SDLK_ESCAPE;
    if (keys.c[0].R)     return SDLK_PAGEUP;
    if (keys.c[0].up)    return SDLK_UP;
    if (keys.c[0].down)  return SDLK_DOWN;
    if (keys.c[0].left)  return SDLK_LEFT;
    if (keys.c[0].right) return SDLK_RIGHT;

    if (keys.c[0].x >  20) return SDLK_RIGHT;
    if (keys.c[0].x < -20) return SDLK_LEFT;
    if (keys.c[0].y >  20) return SDLK_UP;
    if (keys.c[0].y < -20) return SDLK_DOWN;

    return SDLK_UNKNOWN;
}

void cosmo_wait(int delay)
{
    SDL_Delay(8 * delay);
}

void set_input_command_key(InputCommand command, SDL_Keycode keycode)
{
    return;
}

SDL_Keycode get_input_command_key(InputCommand command)
{
    switch (command)
    {
        case CMD_KEY_UP : return cfg_up_key;
        case CMD_KEY_DOWN : return cfg_down_key;
        case CMD_KEY_LEFT : return cfg_left_key;
        case CMD_KEY_RIGHT : return cfg_right_key;
        case CMD_KEY_JUMP : return cfg_jump_key;
        case CMD_KEY_BOMB : return cfg_bomb_key;
        case CMD_KEY_OTHER : break;
        default : break;
    }

    return SDLK_UNKNOWN;
}

const char *get_command_key_string(InputCommand command)
{
    switch (command)
    {
        case CMD_KEY_UP  :   return "UP";
        case CMD_KEY_DOWN  : return "DOWN";
        case CMD_KEY_LEFT  : return "LEFT";
        case CMD_KEY_RIGHT : return "RIGHT";
        case CMD_KEY_JUMP  : return "A";
        case CMD_KEY_BOMB  : return "B";
        case CMD_KEY_OTHER : break;
        default : break;
    }
    return "";
}

void flush_input()
{
    controller_scan();
}

bool is_return_key(SDL_Keycode key)
{
    return (key == SDLK_RETURN);
}

HintDialogInput hint_dialog_get_input(HintDialogInput input)
{
    return EXIT;
}
