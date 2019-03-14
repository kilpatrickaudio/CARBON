/*
 * CARBON Sequencer Controller
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
#ifndef SEQ_CTRL_H
#define SEQ_CTRL_H
#include "../config.h"
#include <inttypes.h>

// record modes
#define SEQ_CTRL_RECORD_IDLE 0
#define SEQ_CTRL_RECORD_ARM 1
#define SEQ_CTRL_RECORD_STEP 2
#define SEQ_CTRL_RECORD_RT 3

// live modes
#define SEQ_CTRL_LIVE_OFF 0
#define SEQ_CTRL_LIVE_ON 1
#define SEQ_CTRL_LIVE_KBTRANS 2

// track selection for track param setters
#define SEQ_CTRL_TRACK_OMNI -1  // all selected tracks will be set
                                // used for MIDI remote control

// init the sequencer controller
void seq_ctrl_init(void);

// run the sequencer control realtime task
void seq_ctrl_rt_task(void);

// run the sequencer control UI task
void seq_ctrl_ui_task(void);

// handle a control from the panel
void seq_ctrl_panel_input(int ctrl, int val);

// check if run lockout is enabled (used when loading and saving)
int seq_ctrl_is_run_lockout(void);

// handle state change
void seq_ctrl_handle_state_change(int event_type, int *data, int data_len);

//
// running edit
//
// get the currently loaded song
int seq_ctrl_get_current_song(void);

// load a song - returns -1 on error
int seq_ctrl_load_song(int song);

// save a song - returns -1 on error
int seq_ctrl_save_song(int song);

// clear the current song
void seq_ctrl_clear_song(void);

// get the scene
int seq_ctrl_get_scene(void);

// set the scene
void seq_ctrl_set_scene(int scene);

// copy current scene to selected scene
void seq_ctrl_copy_scene(int dest_scene);

// get the sequencer run state
int seq_ctrl_get_run_state(void);

// change the sequencer run state
void seq_ctrl_set_run_state(int run);

// reset the playback to the start - does not change playback state
void seq_ctrl_reset_pos(void);

// reset a single track
void seq_ctrl_reset_track(int track);

// get the number of currently selected tracks
int seq_ctrl_get_num_tracks_selected(void);

// get the current track selector - returns -1 on error
int seq_ctrl_get_track_select(int track);

// set the current track selector
void seq_ctrl_set_track_select(int track, int select);

// get the song mode state
int seq_ctrl_get_song_mode(void);

// set the song mode state
void seq_ctrl_set_song_mode(int enable);

// toggle the song mode state
void seq_ctrl_toggle_song_mode(void);

// get the live mode state
int seq_ctrl_get_live_mode(void);

// set the live mode state
void seq_ctrl_set_live_mode(int enable);

// get the first selected track
int seq_ctrl_get_first_track(void);

// the record button was pressed
void seq_ctrl_record_pressed(void);

// get the record mode
int seq_ctrl_get_record_mode(void);

// change the record mode - to be used by seq_ctrl and seq_engine
void seq_ctrl_set_record_mode(int mode);

// set the KB transpose - this is for use via MIDI regardless of LIVE mode
void seq_ctrl_set_kbtrans(int kbtrans);

//
// global params (per song)
//
// adjust the CV cal on a channel
void seq_ctrl_adjust_cvcal(int channel, int change);

// adjust the CV offset on a channel
void seq_ctrl_adjust_cvoffset(int channel, int change);

// adjust the CV gate delay on a channel
void seq_ctrl_adjust_cvgatedelay(int channel, int change);

// set the tempo
void seq_ctrl_set_tempo(float tempo);

// adjust the tempo by a number of single units, fine = 0.1 changes
void seq_ctrl_adjust_tempo(int change, int fine);

// the tempo was tapped from the panel
void seq_ctrl_tap_tempo(void);

// adjust the swing setting
void seq_ctrl_adjust_swing(int change);

// adjust the metronome mode
void seq_ctrl_adjust_metronome_mode(int change);

// adjust the metronome sound len
void seq_ctrl_adjust_metronome_sound_len(int change);

// adjust the key velocity on the input
void seq_ctrl_adjust_key_velocity_scale(int change);

// adjust the CV/gate bend range
void seq_ctrl_adjust_cv_bend_range(int change);

// adjust the CV/gate pairs
void seq_ctrl_adjust_cvgate_pairs(int change);

// adjust the CV/gate pair mode
void seq_ctrl_adjust_cvgate_pair_mode(int pair, int change);

// adjust the CV output scaling mode
void seq_ctrl_adjust_cv_output_scaling(int pair, int change);

// adjust the clock out rate on a port
void seq_ctrl_adjust_clock_out_rate(int port, int change);

// adjust the clock source
void seq_ctrl_adjust_clock_source(int change);

// adjust the MIDI remote control state
void seq_ctrl_adjust_midi_remote_ctrl(int change);

// adjust the MIDI autolive state
void seq_ctrl_adjust_midi_autolive(int change);

// adjust the scene sync mode
void seq_ctrl_adjust_scene_sync(int change);

// adjust the magic range
void seq_ctrl_adjust_magic_range(int change);

// adjust the magic chance
void seq_ctrl_adjust_magic_chance(int change);

//
// track params (per track)
//
// adjust the program of selected tracks
void seq_ctrl_adjust_midi_program(int mapnum, int change);

// set the program of a track
void seq_ctrl_set_midi_program(int track, int mapnum, int program);

// adjust the port mapping of an output on selected tracks
void seq_ctrl_adjust_midi_port(int mapnum, int change);

// adjust the channel of an output on selected tracks
void seq_ctrl_adjust_midi_channel(int mapnum, int change);

// adjust the key split on selected tracks
void seq_ctrl_adjust_key_split(int change);

// adjust the track type of selected tracks
void seq_ctrl_adjust_track_type(int change);

//
// track params (per scene)
//
// set the step length of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_step_length(int track, int length);

// adjust the step length of the selected tracks
void seq_ctrl_adjust_step_length(int change);

// adjust the tonality setting of selected tracks
void seq_ctrl_adjust_tonality(int change);

// set the transpose of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_transpose(int track, int transpose);

// adjust the transpose of selected tracks
void seq_ctrl_adjust_transpose(int change);

// adjust the bias track of selected tracks
void seq_ctrl_adjust_bias_track(int change);

// set the motion start of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_motion_start(int track, int start);

// adjust the motion start of the selected tracks
void seq_ctrl_adjust_motion_start(int change);

// set the motion length of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_motion_length(int track, int length);

// adjust the motion length of the selected tracks
void seq_ctrl_adjust_motion_length(int change);

// set the gate time of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_gate_time(int track, int time);

// adjust the gate time of the selected tracks
void seq_ctrl_adjust_gate_time(int change);

// set a pattern type to a specific index - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_pattern_type(int track, int pattern);

// adjust the pattern type of the selected tracks
void seq_ctrl_adjust_pattern_type(int change);

// set the motion dir of the track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_motion_dir(int track, int dir);

// flip the motion direction of the selected tracks
void seq_ctrl_flip_motion_dir(void);

// get the current mute selector - returns -1 on error
int seq_ctrl_get_mute_select(int track);

// set the current mute selector - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_mute_select(int track, int mute);

// set the arp type on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_type(int track, int type);

// adjust the arp type on selected tracks
void seq_ctrl_adjust_arp_type(int change);

// set the arp speed on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_speed(int track, int speed);

// adjust the arp speed on selected tracks
void seq_ctrl_adjust_arp_speed(int change);

// set the arp gate time on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_gate_time(int track, int time);

// adjust the arp gate time on selected tracks
void seq_ctrl_adjust_arp_gate_time(int change);

// get whether arp mode is enabled on a track
int seq_ctrl_get_arp_enable(int track);

// set whether arp is enabled on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_enable(int track, int enable);

// toggle whether arp mode is enabled on the current tracks
void seq_ctrl_flip_arp_enable(void);

// make magic on tracks - affects the current active tracks / region
void seq_ctrl_make_magic(void);

// clear tracks - affects the current active tracks / region
void seq_ctrl_make_clear(void);

#endif
