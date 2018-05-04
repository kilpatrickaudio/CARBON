/*
 * CARBON Sequencer Engine
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
#ifndef SEQ_ENGINE_H
#define SEQ_ENGINE_H

#include "seq_ctrl.h"
#include "../midi/midi_utils.h"
#include <inttypes.h>

//
// song mode state
//
struct song_mode_state {
    int enable;  // 0 = disable, 1 = enable
    int current_entry;  // the current song list entry to play from (or -1 if null)
    int current_scene;  // the current scene associated with this entry
    int beat_count;  // number of beats that has been played on this entry
    int total_beats;  // total number of beats to play for this entry
};

// init the sequencer engine
void seq_engine_init(void);

// realtime tasks for the engine - handling keyboard input
void seq_engine_timer_task(void);

// run the sequencer - called on each clock tick
void seq_engine_run(uint32_t tick_count);

// the sequencer has been started or stopped
void seq_engine_set_run_state(int run);

// stop notes on a particular track
// this is used when changing MIDI channels or ports
void seq_engine_stop_notes(int track);

// stop live notes
// this is used when adjusting the key split, etc.
void seq_engine_stop_live_notes(void);

// reset an individual track
// this resets the playback position of a track
void seq_engine_reset_track(int track);

// get the current scene
int seq_engine_get_current_scene(void);

// change the scene
void seq_engine_change_scene(int scene);

// handle state change
void seq_engine_handle_state_change(int event_type, int *data, int data_len);

// the step record position was manually changed
void seq_engine_step_rec_pos_changed(int change);

// recording was started or stopped by the user
void seq_engine_record_mode_changed(int oldval, int newval);

// get the song mode state (for display)
struct song_mode_state *seq_engine_get_song_mode_state(void);

// reset the song mode to the beginning
void seq_engine_song_mode_reset(void);

// set the KB transpose
void seq_engine_set_kbtrans(int kbtrans);

//
// arp note control - for output from arp
//
// start an arp note on correct tracks
void seq_engine_arp_start_note(int track, struct midi_msg *msg);

// stop an arp note on correct tracks
void seq_engine_arp_stop_note(int track, struct midi_msg *msg);

//
// MIDI input
//
// handle MIDI input from a controller / keyboard
void seq_engine_midi_rx_msg(struct midi_msg *msg);

#endif
