/*
 * CARBON MIDI Output Processing
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2015: Kilpatrick Audio
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
#ifndef OUTPROC_H
#define OUTPROC_H

#include "../midi/midi_utils.h"

// output delivery
#define OUTPROC_DELIVER_A 0
#define OUTPROC_DELIVER_B 1
#define OUTPROC_DELIVER_BOTH 2
// outproc processing state
#define OUTPROC_OUTPUT_RAW 0
#define OUTPROC_OUTPUT_PROCESSED 1

// init the output processor
void outproc_init(void);

// the transpose changed on a track
void outproc_transpose_changed(int scene, int track);

// the tonality changed on a track
void outproc_tonality_changed(int scene, int track);

// deliver a track message to assigned outputs
void outproc_deliver_msg(int scene, int track, 
    struct midi_msg *msg, int deliver, int process);

// stop all notes on a track
void outproc_stop_all_notes(int track);

#endif

