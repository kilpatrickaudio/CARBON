/*
 * CARBON Song Management
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
#ifndef SONG_H
#define SONG_H

#include "../midi/midi_protocol.h"
#include "../cvproc.h"
#include <inttypes.h>

// note modes
#define SONG_NOTE_MODE_PER_SONG 0
#define SONG_NOTE_MODE_PER_SCENE 1
// MIDI / CV/gate settings
#define SONG_PORT_DISABLE -1
#define SONG_MIDI_PROG_NULL -1
// MIDI clock source
#define SONG_MIDI_CLOCK_SOURCE_INT -1  // internal clock
#define SONG_MIDI_CLOCK_SOURCE_DIN1_IN (MIDI_PORT_DIN1_IN - MIDI_PORT_IN_OFFSET)  // DIN1 in
#define SONG_MIDI_CLOCK_SOURCE_USB_HOST_IN (MIDI_PORT_USB_HOST_IN - MIDI_PORT_IN_OFFSET)  // USB host in
#define SONG_MIDI_CLOCK_SOURCE_USB_DEV_IN (MIDI_PORT_USB_DEV_IN1 - MIDI_PORT_IN_OFFSET)  // USB device in
// CV outputs
#define SONG_CVGATE_NUM_OUTPUTS (CVPROC_NUM_OUTPUTS)
// CV/gate pairs
#define SONG_CVGATE_NUM_PAIRS (CVPROC_NUM_PAIRS)
#define SONG_CVGATE_PAIR_ABCD (CVPROC_PAIRS_ABCD)
#define SONG_CVGATE_PAIR_AABC (CVPROC_PAIRS_AABC)
#define SONG_CVGATE_PAIR_AABB (CVPROC_PAIRS_AABB)
#define SONG_CVGATE_PAIR_AAAA (CVPROC_PAIRS_AAAA)
// CV/gate output modes
#define SONG_CVGATE_MODE_VELO (CVPROC_MODE_VELO)
#define SONG_CVGATE_MODE_NOTE (CVPROC_MODE_NOTE)
// other modes are CC numbers from 0-120
#define SONG_CVGATE_MODE_MAX (CVPROC_MODE_MAX)
// CV output scaling modes
#define SONG_CV_SCALING_MAX (CVPROC_CV_SCALING_MAX)
#define SONG_CV_SCALING_1VOCT (CVPROC_CV_SCALING_1VOCT)  // 1V/octave
#define SONG_CV_SCALING_1P2VOCT (CVPROC_CV_SCALING_1P2VOCT)  // 1.2V/octave
#define SONG_CV_SCALING_HZ_V (CVPROC_CV_SCALING_HZ_V)  // Hz per volt - XXX unsupported
// key split
#define SONG_KEY_SPLIT_OFF 0  // notes will play with any key
#define SONG_KEY_SPLIT_LEFT 1  // notes will play for left hand
#define SONG_KEY_SPLIT_RIGHT 2  // notes will play for right hand
// metronome modes
#define SONG_METRONOME_OFF 0  // metronome off
#define SONG_METRONOME_INTERNAL 1  // internal speaker
#define SONG_METRONOME_CV_RESET 2  // CV reset output
#define SONG_METRONOME_NOTE_LOW 24  // values of 24-84 are note numbers C1-C6
#define SONG_METRONOME_NOTE_HIGH 84
// song track types
#define SONG_TRACK_TYPE_VOICE 0  // transpose affects this track
#define SONG_TRACK_TYPE_DRUM 1  // transpose does not affect this track
// bias track
#define SONG_TRACK_BIAS_NULL -1
// song list scenes
#define SONG_LIST_SCENE_NULL -1
// song magic number token
#define SONG_MAGIC_NUM 0x534f4e47  // "SONG" in big endian

// a single track event like a note event
struct track_event {
    uint8_t type;
    uint8_t data0;  // note number or CC
    uint8_t data1;  // velocity or value
    uint8_t dummy;  // padding to make an even number of bytes
    uint16_t length;  // note length (ticks)
};

// song event types - map to real status messages where possible
#define SONG_EVENT_NULL 0
#define SONG_EVENT_NOTE MIDI_NOTE_ON
#define SONG_EVENT_CC MIDI_CONTROL_CHANGE

// init the song
void song_init(void);

// run the task to take care of loading or saving
void song_timer_task(void);

// clear the song in RAM back to defaults
void song_clear(void);

// load a song from flash memory to RAM
// change events are not generated for each item loaded
// it is up to the system to use the SCE_SONG_LOADED event
// to update whatever state they may need
int song_load(int song_num);

// save the current song to flash mem from RAM
int song_save(int song_num);

// copy a scene to another scene replacing all data
void song_copy_scene(int dest, int src);

//
// getters and setters
//
//
// global params (per song)
//
// get the song version
// - version major: upper 16 bits
// - version minor: lower 16 bits
uint32_t song_get_song_version(void);

// reset the song version to current version
void song_set_version_to_current(void);

// get the song tempo
float song_get_tempo(void);

// set the song tempo
void song_set_tempo(float tempo);

// get the swing
int song_get_swing(void);

// set the swing
void song_set_swing(int swing);

// get the metronome mode
int song_get_metronome_mode(void);

// set the metronome mode
void song_set_metronome_mode(int mode);

// get the metronome sound length
int song_get_metronome_sound_len(void);

// set the metronome sound length
void song_set_metronome_sound_len(int len);

// get the MIDI input key velocity scaling
int song_get_key_velocity_scale(void);

// set the MIDI input key velocity scaling
void song_set_key_velocity_scale(int velocity);

// get the CV bend range
int song_get_cv_bend_range(void);

// set the CV bend range
void song_set_cv_bend_range(int semis);

// get the CV/gate channel pairings
int song_get_cvgate_pairs(void);

// set the CV/gate channel pairings
void song_set_cvgate_pairs(int pairs);

// get the CV/gate pair mode for an output (0-3 = A-D)
int song_get_cvgate_pair_mode(int pair);

// set the CV/gate pair mode for an output (0-3 = A-D)
void song_set_cvgate_pair_mode(int pair, int mode);

// get the CV output scaling for an output
int song_get_cv_output_scaling(int out);

// set the CV output scaling for an output
void song_set_cv_output_scaling(int out, int mode);

// get the CV calibration value for an output - returns -1 on error
int song_get_cvcal(int out);

// set the CV calibration value for an output
void song_set_cvcal(int out, int val);

// get a MIDI port clock out enable setting - returns -1 on error
// port must be a MIDI output port
int song_get_midi_port_clock_out(int port);

// set a MIDI port clock out enable setting
// port must be a MIDI output port
void song_set_midi_port_clock_out(int port, int ppq);

// get the MIDI clock source - see lookup in song.h
int song_get_midi_clock_source(void);

// set the MIDI clock source - see lookup in song.h
void song_set_midi_clock_source(int source);

// get whether MIDI remote control is enabled
int song_get_midi_remote_ctrl(void);

// set whether MIDI remote control is enabled
void song_set_midi_remote_ctrl(int enable);

//
// song list params (per song)
//
// insert a blank entry before the selected entry (move everything down)
void song_add_song_list_entry(int entry);

// remove an entry in the song list
void song_remove_song_list_entry(int entry);

// get the scene for an entry in the song list
int song_get_song_list_scene(int entry);

// set the scene for an entry in the song list
void song_set_song_list_scene(int entry, int scene);

// get the length of the entry in beats
int song_get_song_list_length(int entry);

// set the length of the entry in beats
void song_set_song_list_length(int entry, int length);

// get the KB trans
int song_get_song_list_kbtrans(int entry);

// set the KB trans
void song_set_song_list_kbtrans(int entry, int kbtrans);

//
// track params (per track)
//
// get the MIDI program for a track output
int song_get_midi_program(int track, int mapnum);

// set the MIDI program for a track output
void song_set_midi_program(int track, int mapnum, int program);

// get a MIDI port mapping for a track - returns -2 on error, -1 on unmapped
int song_get_midi_port_map(int track, int mapnum);

// set a MIDI port mapping for a track
void song_set_midi_port_map(int track, int mapnum, int port);

// get a MIDI channel mapping for a track - returns -1 on error
int song_get_midi_channel_map(int track, int mapnum);

// set a MIDI port mapping for a track
void song_set_midi_channel_map(int track, int mapnum, int channel);

// get the MIDI input key split mode
int song_get_key_split(int track);

// set the MIDI input key split mode
void song_set_key_split(int track, int mode);

// get the track type - returns -1 on error
int song_get_track_type(int track);

// set the track type
void song_set_track_type(int track, int mode);

//
// track params (per scene)
//
// get the step length - returns -1 on error
int song_get_step_length(int scene, int track);

// set the step length
void song_set_step_length(int scene, int track, int length);

// get the tonality on a track - returns -1 on error
int song_get_tonality(int scene, int track);

// set the tonality on a track
void song_set_tonality(int scene, int track, int tonality);

// get the transpose on a track - returns -1 on error
int song_get_transpose(int scene, int track);

// set the transpose on a track
void song_set_transpose(int scene, int track, int transpose);

// get the bias track for this track - returns -1 on error
int song_get_bias_track(int scene, int track);

// set the bias track for this track
void song_set_bias_track(int scene, int track, int bias_track);

// get the motion start on a track - returns -1 on error
int song_get_motion_start(int scene, int track);

// set the motion start on a track
void song_set_motion_start(int scene, int track, int start);

// get the motion length on a track - returns -1 on error
int song_get_motion_length(int scene, int track);

// set the motion length on a track
void song_set_motion_length(int scene, int track, int length);

// get the gate time on a track in ticks - returns -1 on error
int song_get_gate_time(int scene, int track);

// set the gate time on a track in ticks
void song_set_gate_time(int scene, int track, int time);

// get the pattern type on a track - returns -1 on error
int song_get_pattern_type(int scene, int track);

// set the pattern type on a track
void song_set_pattern_type(int scene, int track, int pattern);

// get the playback direction on a track - returns -1 on error
int song_get_motion_dir(int scene, int track);

// set the playback direction on a track
void song_set_motion_dir(int scene, int track, int reverse);

// get the mute state of a track - returns -1 on error
int song_get_mute(int scene, int track);

// sets the mute state of a track
void song_set_mute(int scene, int track, int mute);

// get the arp type on a track - returns -1 on error
int song_get_arp_type(int scene, int track);

// set the arp type on a track
void song_set_arp_type(int scene, int track, int type);

// get the arp speed on a track - returns -1 on error
int song_get_arp_speed(int scene, int track);

// set the arp speed on a track
void song_set_arp_speed(int scene, int track, int speed);

// get the arp gate time in tick
int song_get_arp_gate_time(int scene, int track);

// set the arp gate time in ticks
void song_set_arp_gate_time(int scene, int track, int time);

// check if the arp is enabled on a track
int song_get_arp_enable(int scene, int track);

// set if the arp is enabled on a track
void song_set_arp_enable(int scene, int track, int enable);

//
// track event getters and setters
//
// clear all events on a step
void song_clear_step(int scene, int track, int step);

// clear a specific event on a step
void song_clear_step_event(int scene, int track, int step, int slot);

// get the number of active events on a step - returns -1 on error
// this should not be used for iterating since the slots might be fragmented
int song_get_num_step_events(int scene, int track, int step);

// add an event to a step - returns -1 on error (no more slots)
// the event data is copied into the song
int song_add_step_event(int scene, int track, int step, 
    struct track_event *event);

// set/replace the value of a slot - returns -1 on error (invalid slot)
int song_set_step_event(int scene, int track, int step, int slot, 
    struct track_event *event);

// get an event from a step - returns -1 on error (invalid / blank slot)
// the event data is copied into the pointed to struct - returns -1 on error
int song_get_step_event(int scene, int track, int step, int slot, 
    struct track_event *event);

// get the start delay for a step - returns -1 on error
int song_get_start_delay(int scene, int track, int step);

// set the start delay for a step
void song_set_start_delay(int scene, int track, int step, int delay);

// get the ratchet mode for a step - returns -1 on error
int song_get_ratchet_mode(int scene, int track, int step);

// set the ratchet mode for a step
void song_set_ratchet_mode(int scene, int track, int step, int ratchet);

#endif



