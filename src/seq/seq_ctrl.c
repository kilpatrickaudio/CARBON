/*
 * Carbon Sequencer Controller
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
#include "seq_ctrl.h"
#include "seq_engine.h"
#include "arp.h"
#include "metronome.h"
#include "outproc.h"
#include "pattern.h"
#include "scale.h"
#include "song.h"
#include "sysex.h"
#include "../config_store.h"
#include "../cvproc.h"
#include "../power_ctrl.h"
#include "../gui/gui.h"
#include "../gui/panel.h"
#include "../gui/step_edit.h"
#include "../gui/song_edit.h"
#include "../iface/iface_panel.h"
#include "../iface/iface_midi_router.h"
#include "../midi/midi_clock.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"
#include "../midi/midi_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

//
// sequencer state
//
struct seq_state {
    int current_song;  // the current song
    int song_mode;  // 0 = disable, 1 = enable
    int live_mode;  // 0 = normal, 1 = live playthrough, 2 = kb trans
    int track_select[SEQ_NUM_TRACKS];  // track select state for each track
    int first_track;  // first selected track for each track
    int record_mode;  // see lookup table
    int run_lockout;  // 0 = normal, 1 = lockout functions (we are loading or saving)
};
struct seq_state sstate;  // used by seq_engine as an extern

// local functions
void seq_ctrl_cancel_record(void);
void seq_ctrl_refresh_modules(void);
void seq_ctrl_set_current_song(int song);
void seq_ctrl_set_run_lockout(int lockout);

// init the sequencer controller
void seq_ctrl_init(void) {
    sstate.current_song = 0;  // default
    seq_ctrl_set_run_lockout(0);  // not locked out
    // init sequencer modules
    state_change_init();  // run this first
    gui_init();
    midi_clock_init();
    song_init();
    seq_engine_init();  // song must be loaded before this
    step_edit_init();
    song_edit_init();
    sysex_init();
    panel_init();
    pattern_init();
    // init interface (power down) modules
    iface_panel_init();
    iface_midi_router_init();
    // register for events    
    state_change_register(seq_ctrl_handle_state_change, SCEC_SONG);
    state_change_register(seq_ctrl_handle_state_change, SCEC_CONFIG);
    state_change_register(seq_ctrl_handle_state_change, SCEC_POWER);
}

// run the sequencer control realtime task - run on RT interrupt at 1000us interval
void seq_ctrl_rt_task(void) {
    // only run this if we are on
    if(power_ctrl_get_power_state() == POWER_CTRL_STATE_ON) {
        midi_clock_timer_task();  // all music timing starts here
        seq_engine_timer_task();  // must run after clock for correct timing
        step_edit_timer_task();  // handle timeout of step edit mode
        song_edit_timer_task();  // handle timeout of song edit mode
        sysex_timer_task();  // handle processing of SYSEX messages
        song_timer_task();  // handle loading and saving
    }
    // run this if we are off
    else {
        iface_midi_router_timer_task();
    }
    panel_timer_task();  // handle input from user which might be realtime
}

// run the sequencer control UI task - run on main loop
void seq_ctrl_ui_task(void) {
    // block GUI updates during load or save
    if(sstate.run_lockout) {
        return;
    }
    gui_refresh_task();
}

// handle a control from the panel
void seq_ctrl_panel_input(int ctrl, int val) {
    // block panel input if we're locked out
    if(sstate.run_lockout) {
        return;
    }
    panel_handle_input(ctrl, val);
}

// check if run lockout is enabled (used when loading and saving)
int seq_ctrl_is_run_lockout(void) {
    if(sstate.run_lockout) {
        return 1;
    }
    return 0;
}

// handle state change
void seq_ctrl_handle_state_change(int event_type, int *data, int data_len) {
    int i;
    switch(event_type) {
        case SCE_SONG_LOADED:
            log_debug("schsc - song loaded: %d", (data[0] + 1));
            seq_ctrl_set_run_lockout(0);  // enable the UI and MIDI
            seq_ctrl_set_current_song(data[0]);
            seq_ctrl_refresh_modules();  // make sure all song data is updatd in the system
            midi_clock_request_reset_pos();  // reset the clock position
            // turn off modes 
            seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_OFF);
            step_edit_set_enable(0);
            song_edit_set_enable(0);
            // set selections
            for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
                seq_ctrl_set_track_select(i, 0);
                seq_ctrl_set_mute_select(i, 0);
            }
            seq_ctrl_set_song_mode(0);
            seq_ctrl_set_scene(0);
            seq_ctrl_set_track_select(0, 1);
            seq_ctrl_adjust_clock_source(0);  // force clock update
            state_change_fire1(SCE_CTRL_FIRST_TRACK, sstate.first_track);  // force update
            break;
        case SCE_SONG_CLEARED:
            seq_ctrl_set_run_lockout(0);  // enable the UI and MIDI
            seq_ctrl_refresh_modules();  // make sure all song data is updatd in the system
            midi_clock_request_reset_pos();  // reset the clock position
            // turn off modes 
            seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_OFF);
            step_edit_set_enable(0);
            song_edit_set_enable(0);
            // set selections
            for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
                seq_ctrl_set_track_select(i, 0);
                seq_ctrl_set_mute_select(i, 0);
            }
            // final startup
            seq_ctrl_set_run_state(0);
            seq_ctrl_set_scene(0);
            seq_ctrl_set_track_select(0, 0);  // reset so we can update 
            sstate.first_track = (SEQ_NUM_TRACKS - 1);  // cause force update
            seq_ctrl_set_track_select(0, 1);  // select first track            
            seq_ctrl_adjust_clock_source(0);  // force clock update
            seq_ctrl_set_song_mode(0);
            break;
        case SCE_SONG_LOAD_ERROR:
            log_debug("schsc - song load error: %d", (data[0] + 1));
            seq_ctrl_set_run_lockout(0);  // enable the UI and MIDI
            seq_ctrl_set_current_song(data[0]);
            break;
        case SCE_SONG_SAVED:
            log_debug("schsc - song saved: %d", (data[0] + 1));
            seq_ctrl_set_run_lockout(0);  // enable the UI and MIDI
            seq_ctrl_set_current_song(data[0]);
            break;
        case SCE_SONG_SAVE_ERROR:
            log_debug("schsc - song save error: %d", (data[0] + 1));
            seq_ctrl_set_run_lockout(0);  // enable the UI and MIDI
            break;
        case SCE_SONG_TEMPO:
            if(!midi_clock_is_ext_synced()) {
                midi_clock_set_tempo(song_get_tempo());   
            }
            break;
        case SCE_SONG_CV_BEND_RANGE:
            cvproc_set_bend_range(data[0]);
            break;
        case SCE_SONG_CV_GATE_PAIRS:
            cvproc_set_pairs(data[0]);
            break;
        case SCE_SONG_CV_GATE_PAIR_MODE:
            cvproc_set_pair_mode(data[0], data[1]);
            break;
        case SCE_SONG_CV_OUTPUT_SCALING:
            cvproc_set_output_scaling(data[0], data[1]);
            break;
        case SCE_SONG_CVCAL:
            cvproc_set_cvcal(data[0], data[1]);
            break;
        case SCE_CONFIG_LOADED:
            log_debug("scrt - config loaded");
            gui_startup();  // start the GUI now that we know which LCD type we have
            break;
        case SCE_CONFIG_CLEARED:
            log_debug("scrt - config cleared");
            gui_startup();  // start the GUI now that we know which LCD type we have
            // default config stuff
            seq_ctrl_clear_song();
            seq_ctrl_set_current_song(0);
            break;
        case SCE_POWER_STATE:
            switch(data[0]) {
                case POWER_CTRL_STATE_TURNING_OFF:
                    log_debug("scstop");
                    seq_ctrl_set_run_state(0);  // stop sequencer
                    seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_IDLE);  // stop recording
                    break;
                case POWER_CTRL_STATE_ON:
                    // startup
                    log_debug("scstart");
                    seq_ctrl_load_song(config_store_get_val(CONFIG_STORE_LAST_SONG));
                    break;
                default:
                    break;
            }
            break;
    }
}

//
// running edit
//
// get the currently loaded song
int seq_ctrl_get_current_song(void) {
    return sstate.current_song;
}

// load a song - returns -1 on error
int seq_ctrl_load_song(int song) {
    if(song < 0 || song >= SEQ_NUM_SONGS) {
        log_error("scls - song invalid: %d", song);
        return -1;
    }
    // cancel recording if active
    seq_ctrl_cancel_record();
    seq_ctrl_set_run_lockout(1);  // lock out UI and MIDI
    seq_ctrl_set_run_state(0);
    // start the loading process
    if(song_load(song) == -1) {
        seq_ctrl_clear_song();  // clear if song couldn't be loaded
    }
    return 0;
}

// save a song - returns -1 on error
int seq_ctrl_save_song(int song) {
    if(song < 0 || song >= SEQ_NUM_SONGS) {
        log_error("scss - song invalid: %d", song);
        return -1;
    }
    // cancel recording if active
    seq_ctrl_cancel_record();
    seq_ctrl_set_run_lockout(0);  // lock out UI and MIDI
    seq_ctrl_set_run_state(0);
    song_save(song);  // start the saving process
    return 0;
}

// clear the current song
void seq_ctrl_clear_song(void) {
    // cancel recording if active
    seq_ctrl_cancel_record();
    seq_ctrl_set_run_lockout(0);  // lock out UI and MIDI
    seq_ctrl_set_run_state(0);
    song_clear();
}

// get the scene
int seq_ctrl_get_scene(void) {
    return seq_engine_get_current_scene();
}

// set the scene
void seq_ctrl_set_scene(int scene) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("scss - scene invalid: %d", scene);
        return;
    }
    // cancel recording if active
    seq_ctrl_cancel_record();
    // request the engine to change the scene at the next opportunity
    seq_engine_change_scene(scene);
}

// copy current scene to selected scene
void seq_ctrl_copy_scene(int dest_scene) {
    if(dest_scene < 0 || dest_scene >= SEQ_NUM_SCENES) {
        log_error("sccs - scene invalid: %d", dest_scene);
        return;
    }
    // cancel recording if active
    seq_ctrl_cancel_record();
    song_copy_scene(dest_scene, seq_engine_get_current_scene());
}

// get the sequencer run state
int seq_ctrl_get_run_state(void) {
    return midi_clock_get_running();
}

// set the sequencer run state
void seq_ctrl_set_run_state(int run) {
    if(run) {
        midi_clock_request_continue();
    }
    else {
        midi_clock_request_stop();
    }
}

// reset the playback to the start - does not change playback state
void seq_ctrl_reset_pos(void) {
    midi_clock_request_reset_pos();  // reset the clock position
}

// reset a single track
void seq_ctrl_reset_track(int track) {
    // cancel recording if active
    seq_ctrl_cancel_record();
    // reset only the current track
    seq_engine_reset_track(track);
}

// get the number of currently selected tracks
int seq_ctrl_get_num_tracks_selected(void) {
    int track, num = 0;
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            num ++;
        }
    }
    return num;
}

// get the current track selector - returns -1 on error
int seq_ctrl_get_track_select(int track) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("scgts - track invalid: %d", track);
        return -1;
    }
    return sstate.track_select[track];
}

// set the current track selector
void seq_ctrl_set_track_select(int track, int select) {
    int i, temp;
    // cancel recording if active
    seq_ctrl_cancel_record();
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("scsts - track invalid: %d", track);
        return;
    }

    if(select) {
        temp = 1;
    }
    else {
        temp = 0;
    }

    // select changed
    if(temp != sstate.track_select[track]) {
        sstate.track_select[track] = temp;
        // fire event
        state_change_fire2(SCE_CTRL_TRACK_SELECT, track, 
            sstate.track_select[track]);
    }

    // recalculate the first track
    temp = 0;
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        if(sstate.track_select[i]) {
            temp = i;
            break;
        }
    }
    // first selected track has changed
    if(sstate.first_track != temp) {
        sstate.first_track = temp;
        // fire event
        state_change_fire1(SCE_CTRL_FIRST_TRACK, sstate.first_track);
    }
}

// get the song mode state
int seq_ctrl_get_song_mode(void) {
    return sstate.song_mode;
}

// set the song mode state
void seq_ctrl_set_song_mode(int enable) {
    if(enable) {
        sstate.song_mode = 1;
    }
    else {
        sstate.song_mode = 0;
    }
    // fire event
    state_change_fire1(SCE_CTRL_SONG_MODE, sstate.song_mode);    
}

// toggle the song mode state
void seq_ctrl_toggle_song_mode(void) {
    if(seq_ctrl_get_song_mode()) {
        seq_ctrl_set_song_mode(0);
    }
    else {
        seq_ctrl_set_song_mode(1);
    }
}

// get the live mode state
int seq_ctrl_get_live_mode(void) {
    return sstate.live_mode;
}

// set the live mode state
void seq_ctrl_set_live_mode(int mode) {
    switch(mode) {
        case SEQ_CTRL_LIVE_ON:
            sstate.live_mode = SEQ_CTRL_LIVE_ON;
            break;
        case SEQ_CTRL_LIVE_KBTRANS:
            sstate.live_mode = SEQ_CTRL_LIVE_KBTRANS;
            break;
        case SEQ_CTRL_LIVE_OFF:
        default:
            sstate.live_mode = SEQ_CTRL_LIVE_OFF;
            break;
    }
    // fire event
    state_change_fire1(SCE_CTRL_LIVE_MODE, sstate.live_mode);
}

// get the first selected track
int seq_ctrl_get_first_track(void) {
    return sstate.first_track;
}

// the record button was pressed
void seq_ctrl_record_pressed(void) {
    switch(sstate.record_mode) {
        case SEQ_CTRL_RECORD_IDLE:
            seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_ARM);  // arm for record
            break;
        case SEQ_CTRL_RECORD_ARM:
        case SEQ_CTRL_RECORD_STEP:
        case SEQ_CTRL_RECORD_RT:
            seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_IDLE);  // go back to idle
            break;
        default:
            seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_IDLE);
            break;
    }
}

// get the record mode
int seq_ctrl_get_record_mode(void) {
    return sstate.record_mode;
}

// change the record mode - to be used by seq_ctrl and seq_engine
void seq_ctrl_set_record_mode(int mode) {
    sstate.record_mode = mode;
    // disable editing modes
    if(song_edit_get_enable()) {
        song_edit_set_enable(0);
    }
    if(step_edit_get_enable()) {
        step_edit_set_enable(0);
    }
    // disable KB trans mode
    if(seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_KBTRANS) {
        seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_OFF);
    }
    // call this directly
    seq_engine_record_mode_changed(sstate.record_mode);
    // fire event
    state_change_fire1(SCE_CTRL_RECORD_MODE, sstate.record_mode);
}

// set the KB transpose - this is for use via MIDI regardless of LIVE mode
void seq_ctrl_set_kbtrans(int kbtrans) {
    seq_engine_set_kbtrans(kbtrans);
}

//
// global params (per song)
//
// adjust the CV cal on a channel
void seq_ctrl_adjust_cvcal(int channel, int change) {
    if(channel < 0 || channel >= CVPROC_NUM_OUTPUTS) {
        log_error("scacc - channel invalid: %d", channel);
        return;
    }
    song_set_cvcal(channel, seq_utils_clamp(song_get_cvcal(channel) + change,
        CVPROC_CVCAL_MIN, CVPROC_CVCAL_MAX));
}

// set the tempo
void seq_ctrl_set_tempo(float tempo) {
    song_set_tempo(midi_clock_get_tempo());
}

// adjust the tempo by a number of single units, fine = 0.1 changes
void seq_ctrl_adjust_tempo(int change, int fine) {
    if(fine) {
        song_set_tempo(song_get_tempo() + ((float)change * 0.1));
    }
    else {
        song_set_tempo((int)song_get_tempo() + change);
    }
}

// the tempo was tapped from the panel
void seq_ctrl_tap_tempo(void) {
   midi_clock_tap_tempo();
}

// adjust the swing setting
void seq_ctrl_adjust_swing(int change) {
    song_set_swing(seq_utils_clamp(song_get_swing() + change, 
        SEQ_SWING_MIN, SEQ_SWING_MAX));
    midi_clock_set_swing(song_get_swing());
}

// adjust the metronome mode
void seq_ctrl_adjust_metronome_mode(int change) {
    int val = seq_utils_clamp(song_get_metronome_mode() + change,
        0, SONG_METRONOME_NOTE_HIGH);
    if(val > SONG_METRONOME_CV_RESET && val < SONG_METRONOME_NOTE_LOW) {
        if(change > 0) {
            val = SONG_METRONOME_NOTE_LOW;
        }
        else if(change < 0) {
            val = SONG_METRONOME_CV_RESET;
        }
    }
    song_set_metronome_mode(val);
}

// adjust the metronome sound len
void seq_ctrl_adjust_metronome_sound_len(int change) {
    int val = seq_utils_clamp(song_get_metronome_sound_len() + change,
        METRONOME_SOUND_LENGTH_MIN, METRONOME_SOUND_LENGTH_MAX);
    song_set_metronome_sound_len(val);
}

// adjust the key velocity on the input
void seq_ctrl_adjust_key_velocity_scale(int change) {
    song_set_key_velocity_scale(
        seq_utils_clamp(song_get_key_velocity_scale() + change,
        SEQ_KEY_VEL_SCALE_MIN, SEQ_KEY_VEL_SCALE_MAX));
}

// adjust the CV/gate bend range
void seq_ctrl_adjust_cv_bend_range(int change) {
    song_set_cv_bend_range(seq_utils_clamp(song_get_cv_bend_range() + change,
        CVPROC_BEND_RANGE_MIN, CVPROC_BEND_RANGE_MAX));
}

// adjust the CV/gate pairs
void seq_ctrl_adjust_cvgate_pairs(int change) {
    song_set_cvgate_pairs(
        seq_utils_clamp(song_get_cvgate_pairs() + change,
        0, (SONG_CVGATE_NUM_PAIRS - 1)));
}

// adjust the CV/gate pair mode
void seq_ctrl_adjust_cvgate_pair_mode(int pair, int change) {
    if(pair < 0 || pair >= CVPROC_NUM_PAIRS) {
        log_error("scacpm - pair invalid: %d", pair);
        return;
    }
    song_set_cvgate_pair_mode(pair, 
        seq_utils_clamp(song_get_cvgate_pair_mode(pair) + change,
        SONG_CVGATE_MODE_VELO, SONG_CVGATE_MODE_MAX));
}

// adjust the CV output scaling mode
void seq_ctrl_adjust_cv_output_scaling(int out, int change) {
    if(out < 0 || out >= CVPROC_NUM_OUTPUTS) {
        log_error("scacos - out invalid: %d", out);
        return;
    }
    song_set_cv_output_scaling(out,
        seq_utils_clamp(song_get_cv_output_scaling(out) + change,
        0, SONG_CV_SCALING_MAX));
}

// adjust the clock out rate on a port
void seq_ctrl_adjust_clock_out_rate(int port, int change) {
    if(port < 0 || port >= MIDI_PORT_NUM_TRACK_OUTPUTS) {
        log_error("scacor - port invalid: %d", port);
        return;
    }
    song_set_midi_port_clock_out(port,
        seq_utils_clamp(song_get_midi_port_clock_out(port) + change,
        0, (SEQ_UTILS_CLOCK_PPQS - 1)));
}

// adjust the clock source
void seq_ctrl_adjust_clock_source(int change) {
    song_set_midi_clock_source(seq_utils_clamp(song_get_midi_clock_source() + change,
        SONG_MIDI_CLOCK_SOURCE_INT, SONG_MIDI_CLOCK_SOURCE_USB_DEV_IN));
    // update the MIDI clock
    if(song_get_midi_clock_source() == SONG_MIDI_CLOCK_SOURCE_INT) {
        midi_clock_set_source(MIDI_CLOCK_INTERNAL);
    }
    else {
        midi_clock_set_source(MIDI_CLOCK_EXTERNAL);
    }
}

// adjust the MIDI remote control state
void seq_ctrl_adjust_midi_remote_ctrl(int change) {
    song_set_midi_remote_ctrl(seq_utils_clamp(song_get_midi_remote_ctrl() + change,
        0, 1));
}

//
// track params (per track)
//
// adjust the program of selected tracks
void seq_ctrl_adjust_midi_program(int mapnum, int change) {
    int track;
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("scamp - mapnum invalid: %d", mapnum);
        return;
    }
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(seq_ctrl_get_track_select(track)) {
            seq_ctrl_set_midi_program(track, mapnum,
                seq_utils_clamp(song_get_midi_program(sstate.first_track, 
                mapnum) + change, SONG_MIDI_PROG_NULL, 0x7f));
        }
    }
}

// set the program of selected tracks
void seq_ctrl_set_midi_program(int track, int mapnum, int program) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("scsmp - track invalid: %d", track);
        return;
    }
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("scsmp - mapnum invalid: %d", mapnum);
        return;
    }
    // don't change program if the output is CV/gate
    if(song_get_midi_port_map(track, mapnum) == MIDI_PORT_CV_OUT) {
        return;
    }
    song_set_midi_program(track, mapnum, seq_utils_clamp(program, 
        SONG_MIDI_PROG_NULL, 0x7f));
}

// adjust the port mapping of an output on a track
void seq_ctrl_adjust_midi_port(int mapnum, int change) {
    int val, track;
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("scamp - mapnum invalid: %d", mapnum);
        return;
    }
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(seq_ctrl_get_track_select(track)) {    
            val = seq_utils_clamp(song_get_midi_port_map(track, mapnum) + change,
                SONG_PORT_DISABLE, (MIDI_PORT_NUM_TRACK_OUTPUTS - 1));
            // only process if port has actually changed
            if(val != song_get_midi_port_map(track, mapnum)) {
                // stop all notes on port
                seq_engine_stop_notes(track);
                song_set_midi_port_map(track, mapnum, val);
                // if we changed the CV/gate port we should reset the channel
                // XXX todo
            }
        }
    }
}

// adjust the channel of an output
void seq_ctrl_adjust_midi_channel(int mapnum, int change) {
    int track;
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("scamc - mapnum invalid: %d", mapnum);
        return;
    }
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(seq_ctrl_get_track_select(track)) {    
            switch(song_get_midi_port_map(track, mapnum)) {
                case MIDI_PORT_DIN1_OUT:
                case MIDI_PORT_DIN2_OUT:
                case MIDI_PORT_USB_DEV_OUT1:
                case MIDI_PORT_USB_HOST_OUT:
                    seq_engine_stop_notes(track);
                    song_set_midi_channel_map(track, mapnum,
                        seq_utils_clamp(song_get_midi_channel_map(track, mapnum) + change,
                        0, (MIDI_NUM_CHANNELS - 1)));
                    break;
                case MIDI_PORT_CV_OUT:
                    seq_engine_stop_notes(track);
                    song_set_midi_channel_map(track, mapnum,
                        seq_utils_clamp(song_get_midi_channel_map(track, mapnum) + change,
                            0, (CVPROC_NUM_OUTPUTS - 1)));
                    break;
                default:
                    return;
            }
        }
    }
}

// adjust the key split on the input
void seq_ctrl_adjust_key_split(int change) {
    int track;
    seq_engine_stop_live_notes();    
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(seq_ctrl_get_track_select(track)) {        
            song_set_key_split(track, 
                seq_utils_clamp(song_get_key_split(track) + change, 
                SONG_KEY_SPLIT_OFF, SONG_KEY_SPLIT_RIGHT));
        }
    }
}

// adjust the track type of selected tracks
void seq_ctrl_adjust_track_type(int change) {     
    int track;   
    int val = seq_utils_clamp(song_get_track_type(sstate.first_track) + 
        change, 0, 1);

    // edit all selected tracks based on value of the first track
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(seq_ctrl_get_track_select(track)) {
            song_set_track_type(track, val);
        }
    }
}

//
// track params (per scene)
//
// set the step length of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_step_length(int track, int length) {
    int i, val;
    val = seq_utils_clamp(length, 0, (SEQ_UTILS_STEP_LENS - 1));    
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_step_length(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scssl - track invalid: %d", track);
            return;
        }
        song_set_step_length(seq_engine_get_current_scene(), track, val);
    }
}

// adjust the step length of the selected scene and track(s)
void seq_ctrl_adjust_step_length(int change) {
    int track, val;
    val = seq_utils_clamp(song_get_step_length(seq_engine_get_current_scene(), 
        sstate.first_track) + change, 
        0, (SEQ_UTILS_STEP_LENS - 1));
    // update selected track values
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_step_length(seq_engine_get_current_scene(), track, val);
        }
    }
}

// adjust the tonality setting of selected tracks
void seq_ctrl_adjust_tonality(int change) {
    int track;
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_tonality(seq_engine_get_current_scene(), track,
                seq_utils_clamp(song_get_tonality(
                seq_engine_get_current_scene(),
                track) + change, 0, (SCALE_NUM_TONALITIES - 1)));
        }
    }
}

// set the transpose of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_transpose(int track, int transpose) {
    int i, val;
    val = seq_utils_clamp(transpose,
        SEQ_TRANSPOSE_MIN, SEQ_TRANSPOSE_MAX);
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_transpose(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scst - track invalid: %d", track);
            return;
        }
        song_set_transpose(seq_engine_get_current_scene(), track, val);
    }
}

// adjust the transpose of selected tracks
void seq_ctrl_adjust_transpose(int change) {
    int track;
    int transpose;
    transpose = seq_utils_clamp(song_get_transpose(seq_engine_get_current_scene(), 
        sstate.first_track) + change, 
        SEQ_TRANSPOSE_MIN, SEQ_TRANSPOSE_MAX);
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_transpose(seq_engine_get_current_scene(), track, transpose);
        }
    }        
}

// adjust the bias track of selected tracks
void seq_ctrl_adjust_bias_track(int change) {
    int track;
    int bias_track = seq_utils_clamp(song_get_bias_track(seq_engine_get_current_scene(),
        sstate.first_track) + change, -1, (SEQ_NUM_TRACKS - 1));
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_bias_track(seq_engine_get_current_scene(), track, bias_track);
        }
    }
}

// set the motion start of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_motion_start(int track, int start) {
    int i, val;
    val = seq_utils_wrap(start, 0, SEQ_NUM_STEPS - 1);
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_motion_start(seq_engine_get_current_scene(), i, val);            
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsms - track invalid: %d", track);
            return;
        }
        song_set_motion_start(seq_engine_get_current_scene(), track, val);
    }
}

// adjust the motion start of the selected scene and track(s)
void seq_ctrl_adjust_motion_start(int change) {
    int track, val;
    // cancel recording if active
    seq_ctrl_cancel_record();
    val = seq_utils_wrap(song_get_motion_start(seq_engine_get_current_scene(), 
        sstate.first_track) + change,
        0, SEQ_NUM_STEPS - 1);
    // update selected track values
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_motion_start(seq_engine_get_current_scene(), track, val);
        }
    }
}

// set the motion length of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_motion_length(int track, int length) {
    int i, val;
    val = seq_utils_clamp(length, 1, SEQ_NUM_STEPS);
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_motion_length(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsml - track invalid: %d", track);
            return;
        }
        song_set_motion_length(seq_engine_get_current_scene(), track, val);
    }
}

// adjust the motion length of the selected scene and track(s)
void seq_ctrl_adjust_motion_length(int change) {
    int track, val;
    // cancel recording if active
    seq_ctrl_cancel_record();
    val = seq_utils_clamp(song_get_motion_length(seq_engine_get_current_scene(), 
        sstate.first_track) + change,
        1, SEQ_NUM_STEPS);
    // update selected track values
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_motion_length(seq_engine_get_current_scene(), track, val);
        }
    }
}

// set the gate time of a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_gate_time(int track, int time) {
    int i, val;
    // XXX is this being calculated properly?
    val = seq_utils_clamp(time, SEQ_GATE_TIME_MIN, SEQ_GATE_TIME_MAX);
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_gate_time(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsml - track invalid: %d", track);
            return;
        }
        song_set_gate_time(seq_engine_get_current_scene(), track, val);
    }    
}

// adjust the gate time of the selected scene and track(s)
void seq_ctrl_adjust_gate_time(int change) {
    int track, val;
    // XXX is this being calculated properly?
    val = seq_utils_clamp(song_get_gate_time(seq_engine_get_current_scene(), 
        sstate.first_track) + 
        (change * SEQ_GATE_TIME_STEP_SIZE),
        SEQ_GATE_TIME_MIN, SEQ_GATE_TIME_MAX);
    // update selected track values
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_gate_time(seq_engine_get_current_scene(), track, val);
        }
    }
}

// set a pattern type to a specific index - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_pattern_type(int track, int pattern) {
    int i;
    if(pattern < 0 || pattern >= SEQ_NUM_PATTERNS) {
        return;
    }
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_pattern_type(seq_engine_get_current_scene(), i, pattern);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scspt - track invalid: %d", track);
            return;
        }
        song_set_pattern_type(seq_engine_get_current_scene(), track, pattern);
    }
}

// adjust the pattern type of the selected scene and track(s)
void seq_ctrl_adjust_pattern_type(int change) {
    int track, val;
    val = seq_utils_clamp(song_get_pattern_type(seq_engine_get_current_scene(), 
        sstate.first_track) + change,
        0, SEQ_NUM_PATTERNS - 1);
    // update selected track values
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            seq_ctrl_set_pattern_type(track, val);
        }
    }
}

// set the motion dir of the track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_motion_dir(int track, int dir) {
    int i, val;
    // cancel recording if active
    seq_ctrl_cancel_record();
    if(dir) {
        val = 1;
    }
    else {
        val = 0;
    }
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_motion_dir(seq_engine_get_current_scene(), i, val);            
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsmd - track invalid: %d", track);
            return;
        }
        song_set_motion_dir(seq_engine_get_current_scene(), track, val);        
    }
}

// adjust the motion direction of the selected scene and track(s)
void seq_ctrl_flip_motion_dir(void) {
    int track, val;
    // cancel recording if active
    seq_ctrl_cancel_record();
    val = song_get_motion_dir(seq_engine_get_current_scene(), 
        sstate.first_track);
    if(val) {
        val = 0;
    }
    else {
        val = 1;
    }
    // update selected track values
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_motion_dir(seq_engine_get_current_scene(), track, val);
        }
    }
}

// get the current mute selector - returns -1 on error
int seq_ctrl_get_mute_select(int track) {
    return song_get_mute(seq_engine_get_current_scene(), track);
}

// set the current mute selector - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_mute_select(int track, int mute) {
    int i, val;
    // cancel recording if active
    seq_ctrl_cancel_record();
    if(mute) {
        val = 1;
    }
    else {
        val = 0;
    }
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_mute(seq_engine_get_current_scene(), i, val);                       
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsms - track invalid: %d", track);
            return;
        }
        song_set_mute(seq_engine_get_current_scene(), track, val);       
    }
}

// set the arp type on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_type(int track, int type) {
    int i, val;
    val = seq_utils_clamp(type, 0, (ARP_NUM_TYPES - 1));
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_arp_type(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsat - track invalid: %d", track);
            return;
        }
        song_set_arp_type(seq_engine_get_current_scene(), track, val);       
    }
}

// adjust the arp type on selected tracks
void seq_ctrl_adjust_arp_type(int change) {
    int track, val;
    val = seq_utils_clamp(song_get_arp_type(seq_engine_get_current_scene(), 
        sstate.first_track) + change, 0, (ARP_NUM_TYPES - 1));
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {    
            song_set_arp_type(seq_engine_get_current_scene(), track, val);
        }
    }        
}

// set the arp speed on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_speed(int track, int speed) {
    int i, val;
    val = seq_utils_clamp(speed, 0, (SEQ_UTILS_STEP_LENS - 1));
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_arp_speed(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsas - track invalid: %d", track);
            return;
        }
        song_set_arp_speed(seq_engine_get_current_scene(), track, val);
    }
}

// adjust the arp speed on selected tracks
void seq_ctrl_adjust_arp_speed(int change) {
    int track, val;
    val = seq_utils_clamp(song_get_arp_speed(seq_engine_get_current_scene(),
        sstate.first_track) + change, 0, (SEQ_UTILS_STEP_LENS - 1));
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {            
            song_set_arp_speed(seq_engine_get_current_scene(), track, val);
        }
    }
}

// set the arp gate time on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_gate_time(int track, int time) {
    int i, val;
    val = seq_utils_clamp(time, ARP_GATE_TIME_MIN, ARP_GATE_TIME_MAX);
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_arp_gate_time(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsagt - track invalid: %d", track);
            return;
        }
        song_set_arp_gate_time(seq_engine_get_current_scene(), track, val);        
    }
}

// adjust the arp gate time on selected tracks
void seq_ctrl_adjust_arp_gate_time(int change) {
    int track, val;
    val = seq_utils_clamp(song_get_arp_gate_time(seq_engine_get_current_scene(),
        sstate.first_track) + change, ARP_GATE_TIME_MIN, ARP_GATE_TIME_MAX);
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {        
            song_set_arp_gate_time(seq_engine_get_current_scene(), track, val);
        }
    }
}

// get whether arp mode is enabled on a track
int seq_ctrl_get_arp_enable(int track) {
    return song_get_arp_enable(seq_engine_get_current_scene(), track);
}

// set whether arp is enabled on a track - supports SEQ_CTRL_TRACK_OMNI
void seq_ctrl_set_arp_enable(int track, int enable) {
    int i, val;
    if(enable) {
        val = 1;
    }
    else {
        val = 0;
    }
    // modify all tracks
    if(track == SEQ_CTRL_TRACK_OMNI) {
        for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
            song_set_arp_enable(seq_engine_get_current_scene(), i, val);
        }
    }
    // modify a single track
    else {
        if(track < 0 || track >= SEQ_NUM_TRACKS) {
            log_error("scsagt - track invalid: %d", track);
            return;
        }
        song_set_arp_enable(seq_engine_get_current_scene(), track, val);
    }
}

// toggle whether arp mode is enabled on the current tracks
void seq_ctrl_flip_arp_enable(void) {
    int track, val;
    // cancel recording if active
    seq_ctrl_cancel_record();
    val = song_get_arp_enable(seq_engine_get_current_scene(), 
        sstate.first_track);
    if(val) {
        val = 0;
    }
    else {
        val = 1;
    }
    // update selected track values
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(sstate.track_select[track]) {
            song_set_arp_enable(seq_engine_get_current_scene(), track, val);
        }
    }
}

// make magic on tracks - affects the current active tracks / region
void seq_ctrl_make_magic(void) {
    int track, i, start, len, step;
    struct track_event event;
    // cancel recording if active
    seq_ctrl_cancel_record();
    // process currently selected tracks
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(!sstate.track_select[track]) {
            continue;
        }
        // process steps in the active region
        start = song_get_motion_start(seq_engine_get_current_scene(), track);
        len = song_get_motion_length(seq_engine_get_current_scene(), track);
        // check each step
        for(i = 0; i < len; i ++) {
            step = (start + i) & (SEQ_NUM_STEPS - 1);
            song_clear_step(seq_engine_get_current_scene(), track, step);
            // make a new random note
            event.type = SONG_EVENT_NOTE;
            event.data0 = (rand() & 0x1f) + 48;  // 32 note range
            event.data1 = (rand() & 0x3f) + 64;  // top half
            event.length = seq_utils_step_len_to_ticks(
                song_get_step_length(seq_engine_get_current_scene(), track)) >> 1;
            song_set_step_event(seq_engine_get_current_scene(), track, 
                step, 0, &event);
        }
    }
}

// clear tracks - affects the current active tracks / region
void seq_ctrl_make_clear(void) {
    int track, i, start, len, step;
    // cancel recording if active
    seq_ctrl_cancel_record();
    // process currently selected tracks
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(!sstate.track_select[track]) {
            continue;
        }
        // process steps in the active region
        start = song_get_motion_start(seq_engine_get_current_scene(), track);
        len = song_get_motion_length(seq_engine_get_current_scene(), track);
        // check each step
        for(i = 0; i < len; i ++) {
            step = (start + i) & (SEQ_NUM_STEPS - 1);
            song_clear_step(seq_engine_get_current_scene(), track, step);
        }
    }
}

//
// callbacks
//
// a beat was crossed
void midi_clock_beat_crossed(void) {
    state_change_fire0(SCE_CTRL_CLOCK_BEAT);
}

// the run state changed
void midi_clock_run_state_changed(int running) {
    seq_engine_set_run_state(running);
    // starting
    if(running) {
        // if we were already in step record we should cancel recording
        if(sstate.record_mode == SEQ_CTRL_RECORD_STEP) {        
            seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_IDLE);
        }        
    }
    // stopping
    else {
        // cancel recording when stopping
        if(sstate.record_mode != SEQ_CTRL_RECORD_IDLE) {
            seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_IDLE);
        }
    }
    // fire event
    state_change_fire1(SCE_CTRL_RUN_STATE, running);
}

// the tap tempo locked
void midi_clock_tap_locked(void) {
    song_set_tempo(midi_clock_get_tempo());
}

// the clock ticked
void midi_clock_ticked(uint32_t tick_count) {
    // do all sequencer music processing
    seq_engine_run(tick_count);
}

// the clock position was reset
void midi_clock_pos_reset(void) {
    // cancel recording if active
    seq_ctrl_cancel_record();
    // if we are in song mode, reset the song position too
    if(sstate.song_mode) {
        seq_engine_song_mode_reset();
    }
}

// the externally locked tempo changed
void midi_clock_ext_tempo_changed(void) {
    // fire event
    state_change_fire0(SCE_CTRL_EXT_TEMPO);
}


//
// local functions
//
// cancel record if we do something that would conflict with record
void seq_ctrl_cancel_record(void) {
    if(sstate.record_mode != SEQ_CTRL_RECORD_IDLE) {
        seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_IDLE);  // stop recording
    }
}

// refresh modules when the song is loaded or cleared
void seq_ctrl_refresh_modules(void) {
    int i, scene, track;
    int song_ver = song_get_song_version();
    
    log_debug("song ver: %x", song_ver);
    //
    // do song version upgrades based on song version and system version
    //
    // song version <=1.02
    if(song_ver <= 0x00010002) {
        song_set_metronome_sound_len(METRONOME_SOUND_LENGTH_DEFAULT);
        // deprecated in ver. 1.08 and above
//        song_set_midi_port_clock_in(MIDI_PORT_DIN1_IN, 0);  // disable
//        song_set_midi_port_clock_in(MIDI_PORT_USB_HOST_IN, 0);  // disable
//        song_set_midi_port_clock_in(MIDI_PORT_USB_DEV_IN1, 0);  // disable
        // remap old step lengths and arp speeds from 1.02 to 1.03 format
        // this adds support for triplets
        for(scene = 0; scene < SEQ_NUM_SCENES; scene ++) {
            for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
                song_set_step_length(scene, track,
                    seq_utils_remap_step_len_102(song_get_step_length(scene, track)));
                song_set_arp_speed(scene, track,
                    seq_utils_remap_step_len_102(song_get_arp_speed(scene, track)));
            }
        }
    }
    // song version <= 1.07
    if(song_ver <= 0x00010007) {
        // fix the MIDI clock source stuff - default to internal clock
        song_set_midi_clock_source(SONG_MIDI_CLOCK_SOURCE_INT);
    }

    // make sure we save back the current version
    if(song_ver != CARBON_VERSION_MAJMIN) {
        song_set_version_to_current();
    }
    
    // set up clock / reset position
    if(!midi_clock_is_ext_synced()) {
        midi_clock_set_tempo(song_get_tempo());   
    }
    midi_clock_set_swing(song_get_swing());     
    metronome_mode_changed(song_get_metronome_mode());
    metronome_sound_len_changed(song_get_metronome_sound_len());
    cvproc_set_bend_range(song_get_cv_bend_range());
    cvproc_set_pairs(song_get_cvgate_pairs());
    for(i = 0; i < CVPROC_NUM_PAIRS; i ++) {
        cvproc_set_pair_mode(i, song_get_cvgate_pair_mode(i));
        cvproc_set_output_scaling(i, song_get_cv_output_scaling(i));
    }
    // set up CV cal based on saved song data
    for(i = 0; i < CVPROC_NUM_OUTPUTS; i ++) {
        cvproc_set_cvcal(i, song_get_cvcal(i));
    }
}

// set the current song and store the value in the config memory
void seq_ctrl_set_current_song(int song) {
    if(song < 0 || song >= SEQ_NUM_SONGS) {
        log_error("scscs - song invalid: %d", song);
        return;
    }
    sstate.current_song = song;
    config_store_set_val(CONFIG_STORE_LAST_SONG, song);
}

// set the run lockout state (for loading and saving)
void seq_ctrl_set_run_lockout(int lockout) {
    if(lockout) {
        sstate.run_lockout = 1;
        // inject shift release since we might not see it when the panel is locked
        panel_handle_input(PANEL_SW_SHIFT, 0);
    }
    else {
        sstate.run_lockout = 0;    
    }
}

