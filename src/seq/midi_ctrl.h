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

// init the MIDI control module
void midi_ctrl_init(void);

// handle a MIDI message from the keyboard / MIDI input
void midi_ctrl_handle_midi_msg(struct midi_msg *msg);

#endif

