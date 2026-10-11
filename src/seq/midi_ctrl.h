/*
 * CARBON MIDI Control of Playback / Sequencer State
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
#ifndef MIDI_CTRL_H
#define MIDI_CTRL_H

#include "../midi/midi_utils.h"

// per-track controller numbers shared by MIDI Remote and internal CC events
#define MIDI_CTRL_CC_STEP_LENGTH 16
#define MIDI_CTRL_CC_TRACK_TRANSPOSE 17
#define MIDI_CTRL_CC_TRACK_MUTE 18
#define MIDI_CTRL_CC_MOTION_START 19
#define MIDI_CTRL_CC_MOTION_LENGTH 20
#define MIDI_CTRL_CC_MOTION_DIR 21
#define MIDI_CTRL_CC_GATE_TIME 22
#define MIDI_CTRL_CC_PATTERN_TYPE 23
#define MIDI_CTRL_CC_ARP_ENABLE 24
#define MIDI_CTRL_CC_ARP_TYPE 25
#define MIDI_CTRL_CC_ARP_SPEED 26
#define MIDI_CTRL_CC_ARP_GATE_TIME 27
#define MIDI_CTRL_CC_EUCLIDEAN_DENSITY 28
#define MIDI_CTRL_CC_EUCLIDEAN_ROTATION 29
#define MIDI_CTRL_CC_RUN_STOP 80

// init the MIDI control module
void midi_ctrl_init(void);

// handle a MIDI message from the keyboard / MIDI input
void midi_ctrl_handle_midi_msg(struct midi_msg *msg);

// internal CC support
int midi_ctrl_internal_cc_supported(int controller);
int midi_ctrl_internal_cc_to_value(int controller, int value, int motion_length);

#endif

