/*
 * CARBON Panel Handler
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2015: Kilpatrick Audio
 *
 * This file is part of CARBON.
 *
 * CARBON is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CARBON is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CARBON.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef PANEL_H
#define PANEL_H

#include "../config.h"

// keys mode
#define PANEL_SW_NUM_KEYS 18  // number of keys in the keys mode

// panel controls
#define PANEL_SW_SCENE 0  // scene (song)
#define PANEL_SW_ARP 1  // arp (mute)
#define PANEL_SW_LIVE 2  // live
#define PANEL_SW_1 3  // 1
#define PANEL_SW_2 4  // 2
#define PANEL_SW_3 5  // 3
#define PANEL_SW_4 6  // 4
#define PANEL_SW_5 7  // 5
#define PANEL_SW_6 8  // 6
#define PANEL_SW_MIDI 9  // midi (setup)
#define PANEL_SW_CLOCK 10  // clock (tap)
#define PANEL_SW_DIR 11  // dir (magic)
#define PANEL_SW_TONALITY 12  // tonality (swing)
#define PANEL_SW_LOAD 13  // load (save)
#define PANEL_SW_RUN_STOP 14  // run/stop (reset)
#define PANEL_SW_RECORD 15  // record (clear)
#define PANEL_SW_EDIT 16  // edit
#define PANEL_SW_SHIFT 17  // shift
#define PANEL_SW_SONG_MODE 18  // song mode
#define PANEL_ENC_SPEED 19  // speed
#define PANEL_ENC_GATE_TIME 20  // gate time
#define PANEL_ENC_MOTION_START 21  // motion start
#define PANEL_ENC_MOTION_LENGTH 22  // motion length
#define PANEL_ENC_PATTERN_TYPE 23  // pattern type
#define PANEL_ENC_TRANSPOSE 24  // transpose

// panel LEDs
#define PANEL_LED_STATE_OFF 0
#define PANEL_LED_STATE_DIM 1
#define PANEL_LED_STATE_ON 2
#define PANEL_LED_STATE_BLINK 3
#define PANEL_LED_BLINK_OFF 0x1f
#define PANEL_LED_BLINK_ON 0x3f
#define PANEL_LED_NUM_LEDS 13
#define PANEL_LED_ARP 0
#define PANEL_LED_LIVE 1
#define PANEL_LED_1 2  // all track buttons must be sequential
#define PANEL_LED_2 3
#define PANEL_LED_3 4
#define PANEL_LED_4 5
#define PANEL_LED_5 6
#define PANEL_LED_6 7
#define PANEL_LED_CLOCK 8
#define PANEL_LED_DIR 9
#define PANEL_LED_RUN_STOP 10
#define PANEL_LED_RECORD 11
#define PANEL_LED_SONG_MODE 12
// these are used for decoding the colors into components (see panel_if.c)
#define PANEL_LED_BL_LR 13
#define PANEL_LED_BL_LG 14
#define PANEL_LED_BL_LB 15
#define PANEL_LED_BL_RR 16
#define PANEL_LED_BL_RG 17
#define PANEL_LED_BL_RB 18

// panel backlight colors
#define PANEL_BL_COLOR_DEFAULT 0x808080
#define PANEL_BL_COLOR_RECORD 0xff0000
#define PANEL_BL_COLOR_LIVE 0x00ff00
#define PANEL_BL_COLOR_KBTRANS 0xff8000
#define PANEL_BL_COLOR_SCENE_HOLD 0x0000ff
#define PANEL_BL_COLOR_EDIT 0xffa500
#define PANEL_BL_COLOR_SONG_MODE 0xff8800
#define PANEL_BL_COLOR_POWER_OFF 0x000000
#define PANEL_BL_COLOR_POWER_IF_MODE 0xff00ff
#define PANEL_BL_COLOR_POWER_ERROR 0xff0000

// init the panel
void panel_init(void);

// run the panel timer task
void panel_timer_task(void);

// handle a control from the panel
void panel_handle_input(int ctrl, int val);

//
// getters and setters
//
// blink the beat LED
void panel_blink_beat_led(void);

// set the state of a panel LED
void panel_set_led(int led, int state);

// set the state of a panel backlight RGB LED - bits 2:0 = rgb state
void panel_set_bl_led(int led, int rgb);

#endif

