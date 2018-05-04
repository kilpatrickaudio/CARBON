/*
 * CARBON Sequencer Panel Menus
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
#ifndef PANEL_MENU_H
#define PANEL_MENU_H

//
// panel menu modes
//
#define PANEL_MENU_NONE 0
#define PANEL_MENU_SWING 1
#define PANEL_MENU_TONALITY 2
#define PANEL_MENU_ARP 3
#define PANEL_MENU_LOAD 4
#define PANEL_MENU_SAVE 5
#define PANEL_MENU_MIDI 6
#define PANEL_MENU_SYS 7
#define PANEL_MENU_CLOCK 8
#define PANEL_MENU_EGG 255

//
// panel menu parameters
//
// swing
#define PANEL_MENU_SWING_NUM_SUBMODES 1
#define PANEL_MENU_SWING_SWING 0  // global
// tonality
#define PANEL_MENU_TONALITY_NUM_SUBMODES 6
#define PANEL_MENU_TONALITY_SCALE 0  // per scene / track
#define PANEL_MENU_TONALITY_TRANSPOSE 1  // per scene / track
#define PANEL_MENU_TONALITY_BIAS_TRACK 2  // per scene / track
#define PANEL_MENU_TONALITY_TRACK_TYPE 3  // per track
#define PANEL_MENU_TONALITY_MAGIC_RANGE 4  // per song
#define PANEL_MENU_TONALITY_MAGIC_CHANCE 5  // per song
// arp
#define PANEL_MENU_ARP_NUM_SUBMODES 3
#define PANEL_MENU_ARP_TYPE 0  // per scene / track
#define PANEL_MENU_ARP_SPEED 1  // per scene / track
#define PANEL_MENU_ARP_GATE_TIME 2  // per scene / track
// load
#define PANEL_MENU_LOAD_NUM_SUBMODES 2  // only the first two can be selected
#define PANEL_MENU_LOAD_LOAD 0
#define PANEL_MENU_LOAD_CLEAR 1
#define PANEL_MENU_LOAD_LOAD_CONFIRM 2  // not selectable - used when loading
#define PANEL_MENU_LOAD_LOAD_ERROR 3  // not selectable - used when loading
#define PANEL_MENU_LOAD_CLEAR_CONFIRM 4  // not selectable - used when clearing
// save
#define PANEL_MENU_SAVE_NUM_SUBMODES 1  // only the first can be selected
#define PANEL_MENU_SAVE_SAVE 0
#define PANEL_MENU_SAVE_SAVE_CONFIRM 1  // not selectable - used when saving
#define PANEL_MENU_SAVE_SAVE_ERROR 2  // not selectable - used when saving
// MIDI
#define PANEL_MENU_MIDI_NUM_SUBMODES 10
#define PANEL_MENU_MIDI_PROGRAM_A 0  // per track
#define PANEL_MENU_MIDI_PROGRAM_B 1  // per track
#define PANEL_MENU_MIDI_TRACK_OUTA_PORT 2  // per track
#define PANEL_MENU_MIDI_TRACK_OUTB_PORT 3  // per tracl
#define PANEL_MENU_MIDI_TRACK_OUTA_CHAN 4  // per track
#define PANEL_MENU_MIDI_TRACK_OUTB_CHAN 5  // per track
#define PANEL_MENU_MIDI_KEY_SPLIT 6  // per track
#define PANEL_MENU_MIDI_KEY_VELOCITY 7  // per song
#define PANEL_MENU_MIDI_REMOTE_CTRL 8  // per song
#define PANEL_MENU_MIDI_AUTOLIVE 9  // per song
// sys
#define PANEL_MENU_SYS_NUM_SUBMODES 20
#define PANEL_MENU_SYS_VERSION 0  // global
#define PANEL_MENU_SYS_CVGATE_PAIRS 1  // per song
#define PANEL_MENU_SYS_CV_BEND_RANGE 2  // per song
#define PANEL_MENU_SYS_CVGATE_OUTPUT_MODE1 3  // per song
#define PANEL_MENU_SYS_CVGATE_OUTPUT_MODE2 4  // per song
#define PANEL_MENU_SYS_CVGATE_OUTPUT_MODE3 5  // per song
#define PANEL_MENU_SYS_CVGATE_OUTPUT_MODE4 6  // per song
#define PANEL_MENU_SYS_CV_SCALING1 7  // per song
#define PANEL_MENU_SYS_CV_SCALING2 8  // per song
#define PANEL_MENU_SYS_CV_SCALING3 9  // per song
#define PANEL_MENU_SYS_CV_SCALING4 10  // per song
#define PANEL_MENU_SYS_CVCAL1 11  // per song
#define PANEL_MENU_SYS_CVCAL2 12  // per song
#define PANEL_MENU_SYS_CVCAL3 13  // per song
#define PANEL_MENU_SYS_CVCAL4 14  // per song
#define PANEL_MENU_SYS_CVOFFSET1 15  // per song
#define PANEL_MENU_SYS_CVOFFSET2 16  // per song
#define PANEL_MENU_SYS_CVOFFSET3 17  // per song
#define PANEL_MENU_SYS_CVOFFSET4 18  // per song
#define PANEL_MENU_SYS_MENU_TIMEOUT 19 // global
// clock
#define PANEL_MENU_CLOCK_NUM_SUBMODES 10
#define PANEL_MENU_CLOCK_STEP_LEN 0  // per scene / track
#define PANEL_MENU_CLOCK_METRONOME_MODE 1  // per song
#define PANEL_MENU_CLOCK_METRONOME_SOUND_LEN 2  // per song
#define PANEL_MENU_CLOCK_TX_DIN1 3  // per song
#define PANEL_MENU_CLOCK_TX_DIN2 4  // per song
#define PANEL_MENU_CLOCK_TX_CV 5  // per song
#define PANEL_MENU_CLOCK_TX_USB_HOST 6  // per song
#define PANEL_MENU_CLOCK_TX_USB_DEV 7  // per song
#define PANEL_MENU_CLOCK_SOURCE 8  // per song
#define PANEL_MENU_CLOCK_SCENE_SYNC 9  // per song

// init the panel menu
void panel_menu_init(void);

// run the panel menu timer task
void panel_menu_timer_task(void);

// get the current panel menu mode
int panel_menu_get_mode(void);

// handle entering or leaving a menu mode
void panel_menu_set_mode(int mode);

// select the submenu mode
void panel_menu_adjust_cursor(int change, int shift);

// adjust the value (if edit is selected)
void panel_menu_adjust_value(int change, int shift);

// get the panel menu timeout
int panel_menu_get_timeout(void);

// set the panel menu timeout
void panel_menu_set_timeout(int timeout);

#endif
