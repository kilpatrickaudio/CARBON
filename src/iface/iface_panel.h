/*
 * CARBON Interface Panel Handler
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
#ifndef IFACE_PANEL_H
#define IFACE_PANEL_H

#include "../midi/midi_utils.h"

// CC assignments for buttons
#define IFACE_PANEL_SW_SCENE 0
#define IFACE_PANEL_SW_ARP 1
#define IFACE_PANEL_SW_LIVE 2
#define IFACE_PANEL_SW_1 3
#define IFACE_PANEL_SW_2 4
#define IFACE_PANEL_SW_3 5
#define IFACE_PANEL_SW_4 6
#define IFACE_PANEL_SW_5 7
#define IFACE_PANEL_SW_6 8
#define IFACE_PANEL_SW_MIDI 9
#define IFACE_PANEL_SW_CLOCK 10
#define IFACE_PANEL_SW_DIR 11
#define IFACE_PANEL_SW_TONALITY 12
#define IFACE_PANEL_SW_LOAD 13
#define IFACE_PANEL_SW_RUN_STOP 14
#define IFACE_PANEL_SW_RECORD 15
#define IFACE_PANEL_SW_EDIT 16
#define IFACE_PANEL_SW_SHIFT 17
#define IFACE_PANEL_SW_SONG_MODE 18
// CC assignments for knobs
#define IFACE_PANEL_ENC_SPEED 19
#define IFACE_PANEL_ENC_GATE_TIME 20
#define IFACE_PANEL_ENC_MOTION_START 21
#define IFACE_PANEL_ENC_MOTION_LENGTH 22
#define IFACE_PANEL_ENC_PATTERN_TYPE 23
#define IFACE_PANEL_ENC_TRANSPOSE 24
// LED assignments
#define IFACE_PANEL_LED_ARP (IFACE_PANEL_SW_ARP)
#define IFACE_PANEL_LED_LIVE (IFACE_PANEL_SW_LIVE)
#define IFACE_PANEL_LED_1 (IFACE_PANEL_SW_1)
#define IFACE_PANEL_LED_2 (IFACE_PANEL_SW_2)
#define IFACE_PANEL_LED_3 (IFACE_PANEL_SW_3)
#define IFACE_PANEL_LED_4 (IFACE_PANEL_SW_4)
#define IFACE_PANEL_LED_5 (IFACE_PANEL_SW_5)
#define IFACE_PANEL_LED_6 (IFACE_PANEL_SW_6)
#define IFACE_PANEL_LED_CLOCK (IFACE_PANEL_SW_CLOCK)
#define IFACE_PANEL_LED_DIR (IFACE_PANEL_SW_DIR)
#define IFACE_PANEL_LED_RUN_STOP (IFACE_PANEL_SW_RUN_STOP)
#define IFACE_PANEL_LED_RECORD (IFACE_PANEL_SW_RECORD)
#define IFACE_PANEL_LED_SONG_MODE (IFACE_PANEL_SW_SONG_MODE)
#define IFACE_PANEL_LED_BL_LEFT (IFACE_PANEL_ENC_SPEED)
#define IFACE_PANEL_LED_BL_RIGHT (IFACE_PANEL_ENC_TRANSPOSE)


// init the interface panel
void iface_panel_init(void);

// handle input to the interface panel
void iface_panel_handle_input(int ctrl, int val);

// LED control
void iface_panel_handle_led(struct midi_msg *msg);

#endif

