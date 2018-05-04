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
#include "seq_engine.h"
#include "seq_ctrl.h"
#include "arp.h"
#include "metronome.h"
#include "midi_ctrl.h"
#include "song.h"
#include "pattern.h"
#include "outproc.h"
#include "sysex.h"
#include "../gfx.h"
#include "../gui/gui.h"
#include "../gui/panel.h"
#include "../gui/step_edit.h"
#include "../midi/midi_clock.h"
#include "../midi/midi_stream.h"
#include "../midi/midi_utils.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"
#include <limits.h>

// internal settings
#define SEQ_ENGINE_MAX_NOTES 16  // active notes per track
#define SEQ_ENGINE_KEYBOARD_Q_LEN 16  // number of events in the keyboard queue

// setting
#define SEQ_ENGINE_RECORD_EVENTS_MAX (SEQ_NUM_STEPS * SEQ_TRACK_POLY)  // number of events for recording

// active note state
struct seq_engine_active_note {
    struct midi_event note;  // active note
    int16_t start_delay_countdown;  // number of ticks before event start
    int8_t ratchet_note_count;  // number of notes to play
    int8_t ratchet_note_countdown;  // number of remaining ratchets remaining
    int16_t ratchet_note_length;  // length of each ratchet part (based on total note len)
    int16_t ratchet_note_length_countdown;  // tick countdown for each ratchet repeat time
    int16_t ratchet_gate_length;  // gate time for each ratchet note (must be <= note length)
    int16_t ratchet_gate_length_countdown;  // tick countdown for each ratchet gate
};

//
// engine state
//
struct seq_engine_state {
    // local cache for stuff we need to check a lot
    int midi_clock_source;  // see lookup in song.h
    int beat_cross;  // a downbeat was crossed
    int scene_current;  // current scene (set in engine loop)
    int scene_next;  // next scene to be changed to (set by external control)
    int first_track;  // which track is the first input
    int key_velocity_scale;  // scale for input velocity - -100 to +100
    int key_split[SEQ_NUM_TRACKS];  // 0 = off, 1 = left, 2 = right
    int bias_track_map[SEQ_NUM_TRACKS];  // source of bias - -1 = disable, 0-5 = track 1-6
    int arp_enable[SEQ_NUM_TRACKS];  // 0 = off, 1 = on
    int step_size[SEQ_NUM_TRACKS];  // step size in ticks
    int motion_start[SEQ_NUM_TRACKS];  // 0-63 = start
    int motion_len[SEQ_NUM_TRACKS];  // 1-64 = length
    int dir_reverse[SEQ_NUM_TRACKS];  // 0 = normal, 1 = reverse
    int gate_time[SEQ_NUM_TRACKS];  // gate time in ticks
    int track_type[SEQ_NUM_TRACKS];  // track type
    int track_mute[SEQ_NUM_TRACKS];  // track mute
    // playback state
    int clock_div_count[SEQ_NUM_TRACKS];  // clock divider count
    int step_pos[SEQ_NUM_TRACKS];  // current step position
    int bias_track_output[SEQ_NUM_TRACKS];  // bias track output
    int kbtrans;  // keyboard transpose state for tracks (no effect during song mode)
    int autolive;  // the autolive state
    // song mode
    struct song_mode_state sngmode;  // the song mode current data
    // record state
    int record_pos;  // step recording = step position
                     // RT recording = recording start tick per track
    int record_event_count;  // number of recorded events in live mode
    // live state
    uint8_t live_damper_pedal[SEQ_NUM_TRACKS];  // 0 = no hold, 1 = hold
    uint8_t live_active_bend[SEQ_NUM_TRACKS];  // pitch bend activated on this track
    // note timeouts and event queues
    struct seq_engine_active_note track_active_notes[SEQ_NUM_TRACKS][SEQ_ENGINE_MAX_NOTES];  // active play notes
    struct midi_msg live_active_notes[SEQ_NUM_TRACKS][SEQ_ENGINE_MAX_NOTES];  // stores note on msgs
    struct midi_event record_events[SEQ_ENGINE_RECORD_EVENTS_MAX];  // recording temp notes
};
struct seq_engine_state sestate;

//
// local functions
//
// MIDI handlers
void seq_engine_handle_midi_msg(struct midi_msg *msg);
// song mode
void seq_engine_song_mode_process(void);
void seq_engine_song_mode_enable_changed(int enable);
int seq_engine_song_mode_find_next_scene(int current_entry);
void seq_engine_song_mode_load_entry(int entry);
// track event handlers
void seq_engine_track_play_step(int track, int step);
void seq_engine_track_start_note(int track, int step, int length, struct midi_msg *on_msg);
void seq_engine_track_manage_notes(int track);
void seq_engine_track_stop_all_notes(int track);
void seq_engine_track_set_bias_output(int track, int bias_note);
// live input handling
void seq_engine_live_send_msg(int track, struct midi_msg *msg);
int seq_engine_live_enqueue_note(int track, struct midi_msg *on_msg);
void seq_engine_live_dequeue_note(int track, struct midi_msg *off_msg);
int seq_engine_live_get_num_notes(int track);
void seq_engine_live_stop_all_notes(int track);
void seq_engine_live_passthrough(int track, struct midi_msg *msg);
// recording stuff
void seq_engine_record_event(struct midi_msg *msg);
void seq_engine_step_sequence_advance(void);
void seq_engine_step_sequence_shuttle(int change);
void seq_engine_record_write_tracks(void);
// change event handlers
void seq_engine_live_mode_changed(int newval);
void seq_engine_autolive_mode_changed(int newval);
void seq_engine_track_select_changed(int track, int newval);
void seq_engine_mute_select_changed(int scene, int track, int newval);
void seq_engine_cvgate_mode_changed(void);
void seq_engine_key_split_changed(int track, int mode);
void seq_engine_arp_type_changed(int scene, int track, int type);
void seq_engine_arp_speed_changed(int scene, int track, int speed);
void seq_engine_arp_gate_time_changed(int scene, int track, int time);
void seq_engine_arp_enable_changed(int scene, int track, int enable);
// misc
void seq_engine_song_loaded(int song);
void seq_engine_recalc_params(void);
int seq_engine_is_first_step(int track);
int seq_engine_move_to_next_step(int track);
int seq_engine_compute_next_pos(int track, int *pos, int change);
void seq_engine_change_scene_synced(void);
void seq_engine_cancel_pending_scene_change(void);
void seq_engine_reset_all_tracks_pos(void);
void seq_engine_send_program(int track, int mapnum);
void seq_engine_send_all_notes_off(int track);
void seq_engine_highlight_step_record_pos();
int seq_engine_check_key_split_range(int mode, int note);

// init the sequencer engine
void seq_engine_init(void) {
    int i, j;

    // init submodules
    arp_init();
    metronome_init();
    outproc_init();
    midi_ctrl_init();

    // reset the note lists
    for(j = 0; j < SEQ_NUM_TRACKS; j ++) {
        for(i = 0; i < SEQ_ENGINE_MAX_NOTES; i ++) {
            sestate.track_active_notes[j][i].note.msg.status = 0;
            sestate.live_active_notes[j][i].status = 0;
        }
        sestate.live_active_bend[j] = 0;
    }

    // reset record info
    sestate.record_pos = 0;
    sestate.record_event_count = 0;

    // reset playback info
    for(j = 0; j < SEQ_NUM_TRACKS; j ++) {
        sestate.bias_track_output[j] = 0;
    }

    // reset live info
    for(j = 0; j < SEQ_NUM_TRACKS; j ++) {
        sestate.live_damper_pedal[j] = 0;
    }
    seq_engine_set_kbtrans(0);
    sestate.autolive = 0;

    // reset song mode info
    sestate.sngmode.enable = 0;
    sestate.sngmode.current_entry = -1;
    sestate.sngmode.current_scene = 0;
    sestate.sngmode.beat_count = 0;
    sestate.sngmode.total_beats = 0;

    // make sure the first scene is selected
    sestate.scene_current = SEQ_NUM_SCENES - 1;  // force scene to change later

    // reset misc stuff
    sestate.beat_cross = 0;

    // build the cache
    seq_engine_recalc_params();

    // register for events
    state_change_register(seq_engine_handle_state_change, SCEC_SONG);
    state_change_register(seq_engine_handle_state_change, SCEC_CTRL);
}

// realtime tasks for the engine - handling keyboard input
void seq_engine_timer_task(void) {
    struct midi_msg msg;
    static int count = 0;
#ifdef GFX_REMLCD_MODE
    int tx_byte, tx_count;
#endif

    //
    // handle MIDI inputs
    //
    // make sure we're not in a locked out situation
    if(!seq_ctrl_is_run_lockout()) {
        // DIN 1 IN - performance and clock input
        while(midi_stream_data_available(MIDI_PORT_DIN1_IN)) {
            midi_stream_receive_msg(MIDI_PORT_DIN1_IN, &msg);
            // performance
            seq_engine_handle_midi_msg(&msg);
        }
        // USB device IN1 (from PC) - performance, clock and SYSEX input
        while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN1)) {
            midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN1, &msg);
            // SYSEX
            if(midi_utils_is_sysex_msg(&msg) && msg.port == MIDI_PORT_SYSEX_IN) {
                sysex_handle_msg(&msg);
            }
            // performance
            else {
                seq_engine_handle_midi_msg(&msg);
            }
        }
        // USB device IN2 (from PC) - unused in normal mode
        while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN2)) {
            midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN2, &msg);
        }
        // USB device IN3 (from PC) - unused in normal mode
        while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN3)) {
            midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN3, &msg);
        }
        // USB device IN4 (from PC) - unused in normal mode
        while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN4)) {
            midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN4, &msg);
        }
        // USB host IN (USB device) - performance and clock input
        while(midi_stream_data_available(MIDI_PORT_USB_HOST_IN)) {
            midi_stream_receive_msg(MIDI_PORT_USB_HOST_IN, &msg);
            // performance
            seq_engine_handle_midi_msg(&msg);
        }
    }

    // other tasks
    metronome_timer_task();
    // recalculate stuff often - must be responsive enough to work well in
    // performance but not too often to create a huge CPU overhead
    if((count & 0x3f) == 0) {
        seq_engine_recalc_params();
    }
    count ++;

#ifdef GFX_REMLCD_MODE
    // deliver bytes destined for the remote LCD port
    for(tx_count = 0; tx_count < GFX_REMLCD_BYTES_PER_MS; tx_count ++) {
        tx_byte = gfx_remlcd_get_byte();
        if(tx_byte == -1) {
            break;
        }
        midi_stream_send_byte(GFX_REMLCD_MIDI_PORT, tx_byte);
    }
#endif
}

// run the sequencer - called on each clock tick
void seq_engine_run(uint32_t tick_count) {
    struct track_event event;
    int i, track, live_active;

    // position was reset
    if(tick_count == 0) {
        seq_engine_recalc_params();
        seq_engine_reset_all_tracks_pos();
    }

    // other engine music tasks
    metronome_run(tick_count);
    step_edit_run(tick_count);

    // only process events if the clock is running
    if(midi_clock_get_running()) {
        // if we crossed a beat it might be time to change scenes
        if(sestate.beat_cross) {
            sestate.beat_cross = 0;
            // recording and playback processing of song mode
            if(sestate.sngmode.enable) {
                seq_engine_song_mode_process();
                seq_engine_change_scene_synced();
            }
            // scene sync beat mode
            if(song_get_scene_sync() == SONG_SCENE_SYNC_BEAT) {
                seq_engine_change_scene_synced();
            }
        }

        // scene sync mode loop 1 - not necessarily beat synced
        if(song_get_scene_sync() == SONG_SCENE_SYNC_TRACK1 &&
                sestate.clock_div_count[0] == 0 &&
                sestate.step_pos[0] == sestate.motion_start[0]) {
            seq_engine_change_scene_synced();
        }

        // resolve bias track outputs (before step playback on each track)
        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            // run the step
            if(sestate.clock_div_count[track] == 0) {
                // get events that are on this step
                if(pattern_get_step_enable(sestate.scene_current, track,
                        song_get_pattern_type(sestate.scene_current, track),
                        sestate.step_pos[track])) {
                    // use first note on a step as the bias track value
                    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
                        if(song_get_step_event(sestate.scene_current, track,
                                sestate.step_pos[track], i, &event) != -1) {
                            if(event.type == SONG_EVENT_NOTE) {
                                seq_engine_track_set_bias_output(track, event.data0);
                                break;
                            }
                        }
                    }
                }
            }
        }

        // process track notes / recording start/stop
        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            // precompute live state on the track
            if(seq_ctrl_get_track_select(track) &&
                    seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_ON) {
                live_active = 1;
            }
            else {
                live_active = 0;
            }

            // manage notes on this track - check on every tick
            seq_engine_track_manage_notes(track);

            // give a half-step lead-in to start RT recording
            // this is so we can catch notes played a half step early
            // only if we are armed and in the last half of the step time
            // all tracks will be armed together
            if(track == sestate.first_track &&
                    seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_ARM &&
                    seq_engine_is_first_step(track) &&
                    (sestate.clock_div_count[track] >
                    (sestate.step_size[track] >> 1))) {
                // all selected tracks will go into run mode
                seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_RT);
            }

            // run the step
            if(sestate.clock_div_count[track] == 0) {
                // handle starting and stopping recording - start of a loop
                // only work on tracks which are selected for recording
                if(seq_engine_is_first_step(track) &&
                        track == sestate.first_track) {
                    // we are armed for recording - let's start RT recording
                    // this would only be triggered if we start running while
                    // record is already armed
                    if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_ARM) {
                        sestate.record_pos = tick_count - (sestate.step_size[track] >> 1);
                        // all selected tracks will go into run mode
                        seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_RT);
                    }
                }

                // play events that are on this step
                if(!sestate.track_mute[track] &&
                        (!live_active ||
                        (live_active && seq_ctrl_get_record_mode() != SEQ_CTRL_RECORD_IDLE) ||
                        sestate.track_type[track] == SONG_TRACK_TYPE_DRUM) &&
                        pattern_get_step_enable(sestate.scene_current, track,
                            song_get_pattern_type(sestate.scene_current, track),
                            sestate.step_pos[track])) {
                    // play the events on this step
                    seq_engine_track_play_step(track, sestate.step_pos[track]);
                }

                // fire event
                state_change_fire2(SCE_ENG_ACTIVE_STEP, track,
                    sestate.step_pos[track]);

                // manage step position
                seq_engine_move_to_next_step(track);
            }

            // give half step lead-out to RT recording
            if(track == sestate.first_track &&
                seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_RT &&
                    seq_engine_is_first_step(track) &&
                    (sestate.clock_div_count[track] ==
                    (sestate.step_size[track] >> 1))) {
                // handle record finalizing if we actually got some notes
                if(sestate.record_event_count > 0) {
                    // write out data
                    seq_engine_record_write_tracks();
                    // call up "as recorded" pattern
                    seq_ctrl_set_pattern_type(track, PATTERN_AS_RECORDED);
                    // cancel live mode so we will hear the track right away
                    seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_OFF);
                }
                // start recording again
                seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_RT);
            }
            // handle step timing
            sestate.clock_div_count[track] ++;
            if(sestate.clock_div_count[track] >= sestate.step_size[track]) {
                sestate.clock_div_count[track] = 0;
            }
        }
    }

    // run after playback because we might be generating arp input during playback
    arp_run(tick_count);
}

// the sequencer has been started or stopped
void seq_engine_set_run_state(int run) {
    int track, mapnum;
    // starting
    if(run) {
        seq_engine_recalc_params();  // make sure we have the right stuff
        arp_set_seq_enable(1);
    }
    // stopping
    else {
        // stop all currently playing notes and stop arp
        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            seq_engine_track_stop_all_notes(track);
            // if live is not on then we send all notes off
            if(!(seq_ctrl_get_track_select(track) &&
                    seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_ON)) {
                seq_engine_send_all_notes_off(track);
            }
        }
        // send program change for all tracks
        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            for(mapnum = 0; mapnum < SEQ_NUM_TRACK_OUTPUTS; mapnum ++) {
                seq_engine_send_program(track, mapnum);
            }
        }
        arp_set_seq_enable(0);
        // if we were going to change a scene and it didn't happen, just forget it
        if(sestate.scene_current != sestate.scene_next) {
            seq_engine_cancel_pending_scene_change();
        }
    }
}

// stop notes on a particular track
// this is used when changing MIDI channels or ports
void seq_engine_stop_notes(int track) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sesn - track invalid: %d", track);
        return;
    }
    seq_engine_track_stop_all_notes(track);
    seq_engine_live_stop_all_notes(track);
    if(track == METRONOME_MIDI_TRACK) {
        metronome_stop_sound();  // causes note to stop
    }
}

// stop live notes
// this is used when adjusting the key split, etc.
void seq_engine_stop_live_notes(void) {
    int track;
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        seq_engine_live_stop_all_notes(track);
    }
}

// reset an individual track
void seq_engine_reset_track(int track) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sert - track invalid: %d", track);
        return;
    }
    seq_engine_cancel_pending_scene_change();
    sestate.clock_div_count[track] = 0;
    sestate.step_pos[track] = sestate.motion_start[track];
    // fire event
    state_change_fire2(SCE_ENG_ACTIVE_STEP, track, sestate.step_pos[track]);
}

// get the current scene
int seq_engine_get_current_scene(void) {
    return sestate.scene_current;
}

// change the scene
void seq_engine_change_scene(int scene) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("secs: scene invalid: %d", scene);
        return;
    }
    sestate.scene_next = scene;
    // if we're stopped we need to change right away
    if(!midi_clock_get_running()) {
        seq_engine_change_scene_synced();
    }
}

// handle state change
void seq_engine_handle_state_change(int event_type, int *data, int data_len) {
    switch(event_type) {
        case SCE_SONG_LOADED:
            seq_engine_song_loaded(data[0]);
            break;
        case SCE_SONG_TONALITY:
            outproc_tonality_changed(data[0], data[1]);
            break;
        case SCE_SONG_TRANSPOSE:
            outproc_transpose_changed(data[0], data[1]);
            break;
        case SCE_SONG_MUTE:
            seq_engine_mute_select_changed(data[0], data[1], data[2]);
            break;
        case SCE_CTRL_TRACK_SELECT:
            seq_engine_track_select_changed(data[0], data[1]);
            break;
        case SCE_CTRL_LIVE_MODE:
            seq_engine_live_mode_changed(data[0]);
            break;
        case SCE_CTRL_SONG_MODE:
            seq_engine_song_mode_enable_changed(data[0]);
            break;
        case SCE_CTRL_CLOCK_BEAT:
            sestate.beat_cross = 1;
            metronome_beat_cross();
            break;
        case SCE_SONG_METRONOME_MODE:
            metronome_mode_changed(data[0]);
            break;
        case SCE_SONG_METRONOME_SOUND_LEN:
            metronome_sound_len_changed(data[0]);
            break;
        case SCE_SONG_KEY_SPLIT:
            seq_engine_key_split_changed(data[0], data[1]);
            break;
        case SCE_SONG_ARP_TYPE:
            seq_engine_arp_type_changed(data[0], data[1], data[2]);
            break;
        case SCE_SONG_ARP_SPEED:
            seq_engine_arp_speed_changed(data[0], data[1], data[2]);
            break;
        case SCE_SONG_ARP_GATE_TIME:
            seq_engine_arp_gate_time_changed(data[0], data[1], data[2]);
            break;
        case SCE_SONG_ARP_ENABLE:
            seq_engine_arp_enable_changed(data[0], data[1], data[2]);
            break;
        case SCE_SONG_MIDI_PROGRAM:
            seq_engine_send_program(data[0], data[1]);
            break;
        case SCE_SONG_MIDI_AUTOLIVE:
            seq_engine_autolive_mode_changed(data[0]);
            break;
        default:
            break;
    }
}

// the step record position was manually changed
void seq_engine_step_rec_pos_changed(int change) {
    // see if we need to start recording mode
    // this might be the first event that enables recording
    if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_ARM) {
        // no clock - time for step mode
        if(!midi_clock_get_running()) {
            seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_STEP);
        }
    }
    seq_engine_step_sequence_shuttle(change);
}

// recording was started or stopped by the user
void seq_engine_record_mode_changed(int oldval, int newval) {
    switch(newval) {
        case SEQ_CTRL_RECORD_IDLE:
            if(oldval == SEQ_CTRL_RECORD_RT) {
                seq_engine_record_write_tracks();  // try writing data if we canceled mid-way
            }
            seq_engine_live_stop_all_notes(sestate.first_track);
            gui_grid_set_overlay_enable(0);
            break;
        case SEQ_CTRL_RECORD_ARM:
            break;
        case SEQ_CTRL_RECORD_STEP:
            sestate.record_pos = sestate.motion_start[sestate.first_track];
            gui_grid_clear_overlay();
            gui_grid_set_overlay_enable(1);
            sestate.record_event_count = 0;  // reset note count
            seq_engine_highlight_step_record_pos();
            break;
        case SEQ_CTRL_RECORD_RT:
            sestate.record_pos = midi_clock_get_tick_pos();
            sestate.record_event_count = 0;  // reset note count
            break;
        default:
            break;
    }
}

// get the song mode state (for display)
struct song_mode_state *seq_engine_get_song_mode_state(void) {
    return &sestate.sngmode;
}

// reset the song mode to the beginning
void seq_engine_song_mode_reset(void) {
    int entry = seq_engine_song_mode_find_next_scene(-1);
    // no entry found - just turn off song mode
    if(entry == -1) {
        seq_ctrl_set_song_mode(0);  // this will cause us to get called again
        return;
    }
    // load the entry and reset stuff
    seq_engine_song_mode_load_entry(entry);
    // fire event
    state_change_fire0(SCE_ENG_SONG_MODE_STATUS);
}

//
// keyboard transpose
//
// set the KB transpose
void seq_engine_set_kbtrans(int kbtrans) {
    int val = kbtrans;
    // shift up by octave into valid range
    while(val < SEQ_ENGINE_KEY_TRANSPOSE_MIN) {
        val += 12;
    }
    // shift down by octave into valid range
    while(val > SEQ_ENGINE_KEY_TRANSPOSE_MAX) {
        val -= 12;
    }
    sestate.kbtrans = val;
    // fire event
    state_change_fire1(SCE_ENG_KBTRANS, sestate.kbtrans);
}

//
// arp note control - for output from arp
//
// start an arp note on a track
void seq_engine_arp_start_note(int track, struct midi_msg *msg) {
    outproc_deliver_msg(sestate.scene_current, track,
        msg, OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);  // send note
}

// stop an arp note on a track
void seq_engine_arp_stop_note(int track, struct midi_msg *msg) {
    outproc_deliver_msg(sestate.scene_current, track,
        msg, OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);  // send note
}

//
// local functions
//
//
// MIDI input
//
// handle MIDI from keyboards and MIDI input ports
void seq_engine_handle_midi_msg(struct midi_msg *msg) {
    struct midi_msg send_msg;
    int track, live_mode, record_mode, run_state, step_edit_enable;

    // handle system command and realtime
    if((msg->status & 0xf0) == 0xf0) {
        switch(msg->status) {
            //
            // clock messages
            //
            case MIDI_TIMING_TICK:
                // only deliver clock msg if port is enabled for clock in
                if(sestate.midi_clock_source == msg->port - MIDI_PORT_IN_OFFSET) {
                    midi_clock_midi_rx_tick();
                }
                break;
            case MIDI_CLOCK_START:
                // only deliver clock msg if port is enabled for clock in
                if(sestate.midi_clock_source == msg->port - MIDI_PORT_IN_OFFSET) {
                    midi_clock_midi_rx_start();
                }
                break;
            case MIDI_CLOCK_CONTINUE:
                // only deliver clock msg if port is enabled for clock in
                if(sestate.midi_clock_source == msg->port - MIDI_PORT_IN_OFFSET) {
                    midi_clock_midi_rx_continue();
                }
                break;
            case MIDI_CLOCK_STOP:
                // only deliver clock msg if port is enabled for clock in
                if(sestate.midi_clock_source == msg->port - MIDI_PORT_IN_OFFSET) {
                    midi_clock_midi_rx_stop();
                }
                break;
        }
    }
    // handle channel messages
    else {
        midi_utils_copy_msg(&send_msg, msg);  // copy so we can modify contents
        switch(send_msg.status & 0xf0) {
            case MIDI_NOTE_ON:
                // do keyboard velocity scaling
                send_msg.data1 = seq_utils_clamp(send_msg.data1 +
                    sestate.key_velocity_scale, 1, 0x7f);
                break;
            case MIDI_CONTROL_CHANGE:
                // ignore CC >= 120
                if(send_msg.data0 >= MIDI_CONTROLLER_ALL_SOUNDS_OFF) {
                    return;
                }
                break;
        }

        // check each track
        live_mode = seq_ctrl_get_live_mode();
        record_mode = seq_ctrl_get_record_mode();
        run_state = seq_ctrl_get_run_state();
        step_edit_enable = step_edit_get_enable();

        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            // is the track unselected?
            if(seq_ctrl_get_track_select(track) == 0) {
                continue;
            }
            // handle live mode and recording (handles note list)
            // this will send even if we are muted since recording
            // can happen if a track is muted
            if(seq_ctrl_get_track_select(track) &&
                    (live_mode == SEQ_CTRL_LIVE_ON ||
                    (sestate.autolive && live_mode != SEQ_CTRL_LIVE_KBTRANS) ||
                    record_mode != SEQ_CTRL_RECORD_IDLE ||
                    (step_edit_enable && !run_state))) {
                seq_engine_live_send_msg(track, &send_msg);
            }
            // handle pass-through messages all the time
            seq_engine_live_passthrough(track, &send_msg);
        }
        // handle recording - must be after live send so we can use note list
        seq_engine_record_event(&send_msg);
        // handle transposing voice tracks
        // we can't be recording or in live mode, or editing
        if(seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_KBTRANS &&
                seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_IDLE &&
                !step_edit_get_enable() &&
                !sestate.sngmode.enable) {
            if((send_msg.status & 0xf0) == MIDI_NOTE_ON) {
                seq_engine_set_kbtrans(send_msg.data0 - SEQ_TRANSPOSE_CENTRE);
            }
        }
        // step edit wants all MIDI input on the keyboard control channel
        step_edit_handle_input(&send_msg);
    }
    // MIDI control wants all MIDI input on every channel
    midi_ctrl_handle_midi_msg(msg);  // send the raw data to preserve channel
}

//
// song mode
//
// process song mode stuff at a beat time
// called at the start of each beat before any notes are emitted
void seq_engine_song_mode_process(void) {
    int entry;
    // check that we are running
    if(sestate.sngmode.enable == 0) {
        return;
    }

    // see if it's time to change scenes
    if(sestate.sngmode.beat_count >= sestate.sngmode.total_beats) {
        entry = seq_engine_song_mode_find_next_scene(sestate.sngmode.current_entry);
        // no entry found - just turn off song mode
        if(entry == -1) {
            seq_ctrl_set_song_mode(0);
            return;
        }
        // load the entry and reset stuff
        seq_engine_song_mode_load_entry(entry);
        sestate.scene_next = sestate.sngmode.current_scene;
        sestate.sngmode.beat_count ++;
        // fire event
        state_change_fire0(SCE_ENG_SONG_MODE_STATUS);
    }
    // otherwise let's just increment the beat count
    else {
        sestate.sngmode.beat_count ++;
        // fire event
        state_change_fire0(SCE_ENG_SONG_MODE_STATUS);
    }
}

// song mode enable changed
void seq_engine_song_mode_enable_changed(int enable) {
    int entry;
    // we just turned off - just stop stuff and go away
    if(enable == 0) {
        sestate.sngmode.enable = 0;
        seq_engine_set_kbtrans(0);  // reset transpose
        return;
    }
    entry = sestate.sngmode.current_entry;
    // don't change the current entry except if it has become null
    if(song_get_song_list_scene(sestate.sngmode.current_entry) ==
            SONG_LIST_SCENE_NULL) {
        entry = seq_engine_song_mode_find_next_scene(entry);
    }
    // if the entry is null we probably ran off the end - try searching again
    if(entry == -1) {
        entry = seq_engine_song_mode_find_next_scene(-1);
    }
    // no entry found - just turn off song mode
    if(entry == -1) {
        seq_ctrl_set_song_mode(0);  // this will cause us to get called again
        return;
    }
    // load the entry and reset stuff
    seq_engine_song_mode_load_entry(entry);
    sestate.sngmode.enable = 1;
    // issue a reset to the playback
    seq_ctrl_reset_pos();
    // fire event
    state_change_fire0(SCE_ENG_SONG_MODE_STATUS);
}

// find the next song mode scene entry
// returns the entry or -1 if none is found
int seq_engine_song_mode_find_next_scene(int current_entry) {
    int i;
    for(i = (current_entry + 1); i < SEQ_SONG_LIST_ENTRIES; i ++) {
        if(song_get_song_list_scene(i) != SONG_LIST_SCENE_NULL) {
            return i;
        }
    }
    return -1;
}

// load a song mode entry and notify listeners
void seq_engine_song_mode_load_entry(int entry) {
    // invalid entry
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        // invalid entry - just turn off song mode
        seq_ctrl_set_song_mode(0);  // this will cause us to get called again
        return;
    }
    // set state
    sestate.sngmode.current_entry = entry;
    sestate.sngmode.current_scene =
        song_get_song_list_scene(sestate.sngmode.current_entry);
    sestate.sngmode.beat_count = 0;  // reset the current scene playback
    sestate.sngmode.total_beats =
        song_get_song_list_length(sestate.sngmode.current_entry);
    seq_engine_set_kbtrans(song_get_song_list_kbtrans(sestate.sngmode.current_entry));
    // change scenes
    seq_engine_change_scene(sestate.sngmode.current_scene);
}

//
// track event handling
//
void seq_engine_track_play_step(int track, int step) {
    int i, bias, temp;
    struct track_event event;
    struct midi_msg msg;

    // play each event on the step
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        // get the event and make sure it's valid
        if(song_get_step_event(sestate.scene_current, track, step, i, &event) != -1) {
            // handle event types
            switch(event.type) {
                case SONG_EVENT_NOTE:
                    bias = 0;
                    // if bias track is enabled and not our own track
                    if(sestate.bias_track_map[track] != track &&
                            sestate.bias_track_map[track] != SONG_TRACK_BIAS_NULL) {
                        bias = sestate.bias_track_output[sestate.bias_track_map[track]];
                    }

                    // drum track - bias / no kbtrans
                    if(sestate.track_type[track] == SONG_TRACK_TYPE_DRUM) {
                        midi_utils_enc_note_on(&msg, 0, 0,
                            seq_utils_clamp(event.data0 + bias, 0, 127),
                            event.data1);
                    }
                    // voice track - bias + kbtrans + songmode.kbtrans
                    else {
                        // scale by keyboard transpose
                        temp = event.data0 + sestate.kbtrans + bias;
                        if(!seq_utils_check_note_range(temp)) {
                            return;
                        }
                        midi_utils_enc_note_on(&msg, 0, 0, temp, event.data1);
                    }
                    seq_engine_track_start_note(track, step, event.length, &msg);
                    break;
                case SONG_EVENT_CC:
                    // send event directly
                    midi_utils_enc_control_change(&msg, 0, 0, event.data0, event.data1);
                    outproc_deliver_msg(sestate.scene_current, track, &msg,
                        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
                    break;
                case SONG_EVENT_NULL:
                default:
                    log_warn("sese - unknown event type: %d", event.type);
                    return;
            }
        }
    }
}

// start a note on a track playback - also figures out ratcheting and start delay
void seq_engine_track_start_note(int track, int step, int length, struct midi_msg *on_msg) {
    int i, free_slot = -1, total_len;
    int min_time_remain = 0xffff;
    int min_time_remain_slot = -1;
    // search for a free slot to put this event
    for(i = 0; i < SEQ_ENGINE_MAX_NOTES; i ++) {
        // slot is free
        if(sestate.track_active_notes[track][i].note.msg.status == 0) {
            free_slot = i;
        }
        // slot is in use
        else if(sestate.track_active_notes[track][i].note.tick_len < min_time_remain) {
            min_time_remain = sestate.track_active_notes[track][i].note.tick_len;
            min_time_remain_slot = i;
        }
    }
    // if there are no free slots, kill the shortest note and free the slot
    if(free_slot == -1) {
        // arp input
        if(sestate.arp_enable[track]) {
            arp_handle_input(track,
                &sestate.track_active_notes[track][min_time_remain_slot].note.msg);
        }
        // normal playback
        else {
            outproc_deliver_msg(sestate.scene_current, track,
                &sestate.track_active_notes[track][min_time_remain_slot].note.msg,
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
        }
        free_slot = min_time_remain_slot;
    }
    // now we have a slot we can use for this new note
    // put the note data into the slot and ensure it has a valid length
    midi_utils_copy_msg(&sestate.track_active_notes[track][free_slot].note.msg, on_msg);
    // figure out the total length scaled by gate override (for non-ratchet mode)
    total_len = (length * sestate.gate_time[track]) >> 7;
    if(total_len < 1) {
        sestate.track_active_notes[track][free_slot].note.tick_len = 1;
    }
    else {
        sestate.track_active_notes[track][free_slot].note.tick_len = total_len;
    }
    // get the track params for the step
    sestate.track_active_notes[track][free_slot].start_delay_countdown =
        song_get_start_delay(sestate.scene_current, track, step);
    sestate.track_active_notes[track][free_slot].ratchet_note_count =
        song_get_ratchet_mode(sestate.scene_current, track, step);

    // calculate ratchet stuff if we are making more than 1 note
    if(sestate.track_active_notes[track][free_slot].ratchet_note_count > 1) {
        // ratchet countdown
        sestate.track_active_notes[track][free_slot].ratchet_note_countdown =
            sestate.track_active_notes[track][free_slot].ratchet_note_count;
        // note length - based on step len not including gate time override
        sestate.track_active_notes[track][free_slot].ratchet_note_length =
            length / sestate.track_active_notes[track][free_slot].ratchet_note_count;
        sestate.track_active_notes[track][free_slot].ratchet_note_length_countdown =
            sestate.track_active_notes[track][free_slot].ratchet_note_length;
        // gate length - based on the note length scaled by the gate time
        // made to be 50% duty cycle by default (at 100% gate length override)
        sestate.track_active_notes[track][free_slot].ratchet_gate_length =
            (sestate.track_active_notes[track][free_slot].ratchet_note_length *
            sestate.gate_time[track]) >> 8;
        // make sure gate length is not longer than note length
        if(sestate.track_active_notes[track][free_slot].ratchet_gate_length >
                sestate.track_active_notes[track][free_slot].ratchet_note_length) {
            sestate.track_active_notes[track][free_slot].ratchet_gate_length =
                sestate.track_active_notes[track][free_slot].ratchet_note_length;
        }
        sestate.track_active_notes[track][free_slot].ratchet_gate_length_countdown =
            sestate.track_active_notes[track][free_slot].ratchet_gate_length;
    }

    // arp tracks always play immediately regardless of start delay
    if(sestate.arp_enable[track]) {
        arp_handle_input(track, on_msg);
        // reset delay countdown so we don't start the note again in the manager
        sestate.track_active_notes[track][free_slot].start_delay_countdown = 0;
        // we can't use ratcheting for arp tracks so let's just reset it so we don't use it
        sestate.track_active_notes[track][free_slot].ratchet_note_count = 1;
    }
    // normal notes play now if the start delay is zero
    else if(sestate.track_active_notes[track][free_slot].start_delay_countdown == 0) {
        outproc_deliver_msg(sestate.scene_current, track, on_msg,
            OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
    }
}

// manage notes - do ratcheting, delayed start and timeout
void seq_engine_track_manage_notes(int track) {
    struct midi_msg msg;
    int note;
    // find notes that are in use
    for(note = 0; note < SEQ_ENGINE_MAX_NOTES; note ++) {
        // check if the slot is in use
        if(sestate.track_active_notes[track][note].note.msg.status != 0) {
            // see if we are still waiting to start the note
            if(sestate.track_active_notes[track][note].start_delay_countdown) {
                sestate.track_active_notes[track][note].start_delay_countdown --;
                // time to start the note for reals - everything else is set up
                if(sestate.track_active_notes[track][note].start_delay_countdown == 0) {
                    outproc_deliver_msg(sestate.scene_current, track,
                        &sestate.track_active_notes[track][note].note.msg,
                        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
                }
            }
            // ratcheting is enabled so let's deal with that
            else if(sestate.track_active_notes[track][note].ratchet_note_count > 1) {
                sestate.track_active_notes[track][note].ratchet_gate_length_countdown --;
                // gate timed out
                if(sestate.track_active_notes[track][note].ratchet_gate_length_countdown <= 0) {
                    // make a copy and convert to note off
                    midi_utils_copy_msg(&msg, &sestate.track_active_notes[track][note].note.msg);
                    midi_utils_note_on_to_off(&msg);
                    outproc_deliver_msg(sestate.scene_current, track, &msg,
                        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
                }
                sestate.track_active_notes[track][note].ratchet_note_length_countdown --;
                // note timed out
                if(sestate.track_active_notes[track][note].ratchet_note_length_countdown <= 0) {
                    // see if we should start the note again
                    sestate.track_active_notes[track][note].ratchet_note_countdown --;
                    if(sestate.track_active_notes[track][note].ratchet_note_countdown > 0) {
                        outproc_deliver_msg(sestate.scene_current, track,
                            &sestate.track_active_notes[track][note].note.msg,
                            OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
                        // reset stuff for the next note
                        sestate.track_active_notes[track][note].ratchet_note_length_countdown =
                            sestate.track_active_notes[track][note].ratchet_note_length;
                        sestate.track_active_notes[track][note].ratchet_gate_length_countdown =
                            sestate.track_active_notes[track][note].ratchet_gate_length;
                    }
                    // no more ratchet notes to play - just free the slot
                    else {
                        sestate.track_active_notes[track][note].note.msg.status = 0;  // free slot
                    }
                }
            }
            // the note is playing without ratcheting so let's just time it out
            else {
                sestate.track_active_notes[track][note].note.tick_len --;
                // time to kill the note and free the slot
                if(sestate.track_active_notes[track][note].note.tick_len == 0) {
                    // make a copy and convert to note off
                    midi_utils_copy_msg(&msg, &sestate.track_active_notes[track][note].note.msg);
                    midi_utils_note_on_to_off(&msg);
                    // arp input
                    if(sestate.arp_enable[track]) {
                        arp_handle_input(track, &msg);
                    }
                    // normal playback
                    else {
                        outproc_deliver_msg(sestate.scene_current, track, &msg,
                            OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
                    }
                    sestate.track_active_notes[track][note].note.msg.status = 0;  // free slot
                }
            }
        }
    }
}

// stop all playing notes
void seq_engine_track_stop_all_notes(int track) {
    struct midi_msg msg;
    int note;
    for(note = 0; note < SEQ_ENGINE_MAX_NOTES; note ++) {
        // note is active
        if(sestate.track_active_notes[track][note].note.msg.status != 0) {
            // make a copy and convert to note off
            midi_utils_copy_msg(&msg,
                &sestate.track_active_notes[track][note].note.msg);
            midi_utils_note_on_to_off(&msg);
            // arp input
            if(sestate.arp_enable[track]) {
                arp_handle_input(track, &msg);
            }
            // normal playback
            else {
                outproc_deliver_msg(sestate.scene_current, track, &msg,
                    OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
            }
            sestate.track_active_notes[track][note].note.msg.status = 0;  // free note
        }
    }
}

// set the track bias output
void seq_engine_track_set_bias_output(int track, int bias_note) {
    int val = bias_note;
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("setsbo - track invalid: %d", track);
        return;
    }
    // shift up by octave into valid range
    while(val < SEQ_ENGINE_KEY_TRANSPOSE_MIN) {
        val += 12;
    }
    // shift down by octave into valid range
    while(val > SEQ_ENGINE_KEY_TRANSPOSE_MAX) {
        val -= 12;
    }
    sestate.bias_track_output[track] = (bias_note - SEQ_TRANSPOSE_CENTRE);
}

//
// live event handling
//
// send messages from the live input
// events get filtered for arp and key split here
// by the time we get here we know that live mode is on or we're not recording
void seq_engine_live_send_msg(int track, struct midi_msg *msg) {
    // handle event types
    switch(msg->status & 0xf0) {
        case MIDI_NOTE_OFF:
            // figure out key split mode - we care about this note if:
            //  - there is only one track selected
            //  - if there are multiple tracks selected and:
            //    - note matches the key split range of this track
            if(seq_ctrl_get_num_tracks_selected() < 2 ||
                    seq_engine_check_key_split_range(sestate.key_split[track],
                    msg->data0)) {
                // if arp is enabled on this track then send the messagfe there
                if(sestate.arp_enable[track]) {
                    arp_handle_input(track, msg);
                }
                // otherwise play it live
                else {
                    seq_engine_live_dequeue_note(track, msg);  // remove unprocessed pitch
                    outproc_deliver_msg(sestate.scene_current, track, msg,
                        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);  // send note
                }
            }
            break;
        case MIDI_NOTE_ON:
            // figure out key split mode - we care about this note if:
            //  - there is only one track selected
            //  - if there are multiple tracks selected and:
            //    - note matches the key split range of this track
            if(seq_ctrl_get_num_tracks_selected() < 2 ||
                    seq_engine_check_key_split_range(sestate.key_split[track],
                    msg->data0)) {
                // if arp is enabled on this track then send the message there
                if(sestate.arp_enable[track]) {
                    arp_handle_input(track, msg);
                }
                // otherwise play it live
                else {
                    // store unprocessed pitch
                    if(seq_engine_live_enqueue_note(track, msg) == -1) {
                        return;
                    }
                    outproc_deliver_msg(sestate.scene_current, track, msg,
                        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);  // send note
                }
            }
            break;
        case MIDI_POLY_KEY_PRESSURE:
            // figure out key split mode - we care about this note if:
            //  - there is only one track selected
            //  - if there are multiple tracks selected and:
            //    - note matches the key split range of this track
            if(seq_ctrl_get_num_tracks_selected() < 2 ||
                    seq_engine_check_key_split_range(sestate.key_split[track],
                    msg->data0)) {
                outproc_deliver_msg(sestate.scene_current, track, msg,
                    OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
            }
            break;
        case MIDI_CONTROL_CHANGE:
            // keep track of damper pedal
            if(msg->data0 == MIDI_CONTROLLER_DAMPER) {
                if(msg->data1 > 0) {
                    sestate.live_damper_pedal[track] = 1;
                }
                else {
                    sestate.live_damper_pedal[track] = 0;
                }
            }
            outproc_deliver_msg(sestate.scene_current, track, msg,
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
            break;
        case MIDI_CHANNEL_PRESSURE:
            outproc_deliver_msg(sestate.scene_current, track, msg,
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
            break;
        default:
            break;
    }
}

// queue note that is currently pressed down so we can stop it later
// returns -1 if there are no more slots to queue the note
int seq_engine_live_enqueue_note(int track, struct midi_msg *on_msg) {
    int i, free_slot = -1;
    // search for a free slot to put the note
    for(i = 0; i < SEQ_ENGINE_MAX_NOTES; i ++) {
        if(sestate.live_active_notes[track][i].status == 0) {
            free_slot = i;
            break;
        }
    }
    // no free slots - return error
    if(free_slot == -1) {
        return -1;
    }
    midi_utils_copy_msg(&sestate.live_active_notes[track][free_slot], on_msg);
    return 0;
}

// dequeue note that is currently pressed down so we can stop it later
void seq_engine_live_dequeue_note(int track, struct midi_msg *off_msg) {
    int i;
    // search for the note on that corresponds to our note off
    for(i = 0; i < SEQ_ENGINE_MAX_NOTES; i ++) {
        // check if the note on in the queue corresponds to our note off
        if(sestate.live_active_notes[track][i].status != 0 &&
                midi_utils_compare_note_msg(&sestate.live_active_notes[track][i], off_msg)) {
            sestate.live_active_notes[track][i].status = 0;  // free the slot
            break;
        }
    }
}

// get the number of currently held live notes on a track
int seq_engine_live_get_num_notes(int track) {
    int i, count = 0;
    // search for live notes
    for(i = 0; i < SEQ_ENGINE_MAX_NOTES; i ++) {
        if(sestate.live_active_notes[track][i].status != 0) {
            count ++;
        }
    }
    return count;
}

// stop all live notes on a track - used if we disable live mode or switch tracks
void seq_engine_live_stop_all_notes(int track) {
    struct midi_msg msg;
    int i;
    // search for the notes that we need to turn off
    for(i = 0; i < SEQ_ENGINE_MAX_NOTES; i ++) {
        if(sestate.live_active_notes[track][i].status != 0) {
            midi_utils_note_on_to_off(&sestate.live_active_notes[track][i]);
            outproc_deliver_msg(sestate.scene_current, track,
                &sestate.live_active_notes[track][i],
                OUTPROC_DELIVER_BOTH,
                OUTPROC_OUTPUT_PROCESSED);
            sestate.live_active_notes[track][i].status = 0;  // free the slot
        }
    }
    // turn off the damper pedal if we had it down
    if(sestate.live_damper_pedal[track]) {
        midi_utils_enc_control_change(&msg, 0, 0, MIDI_CONTROLLER_DAMPER, 0);
        outproc_deliver_msg(sestate.scene_current, track, &msg,
            OUTPROC_DELIVER_BOTH,
            OUTPROC_OUTPUT_PROCESSED);
    }
    // reset pitch bend if we used it
    if(sestate.live_active_bend[track]) {
        midi_utils_enc_pitch_bend(&msg, 0, 0, 0);
        outproc_deliver_msg(sestate.scene_current, track, &msg,
            OUTPROC_DELIVER_BOTH,
            OUTPROC_OUTPUT_PROCESSED);
        sestate.live_active_bend[track] = 0;
    }
}

// handle some keyboard messages always
void seq_engine_live_passthrough(int track, struct midi_msg *msg) {
    switch(msg->status & 0xf0) {
        case MIDI_CONTROL_CHANGE:
            switch(msg->data0) {
                case MIDI_CONTROLLER_ALL_SOUNDS_OFF:
                    outproc_deliver_msg(sestate.scene_current, track, msg,
                        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
                    break;
            }
            break;
        case MIDI_PROGRAM_CHANGE:
            // set the program on the A output only
            seq_ctrl_set_midi_program(track, 0, msg->data0);
            break;
        case MIDI_PITCH_BEND:
            // echo to the track
            outproc_deliver_msg(sestate.scene_current, track, msg,
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_PROCESSED);
            sestate.live_active_bend[track] = 1;  // mark that we bent
            break;
        default:
            break;
    }
}

//
// recording
//
// record the event
void seq_engine_record_event(struct midi_msg *msg) {
    int i;
    struct track_event trkevent;

    // see if we need to start recording mode
    // this might be the first event that enables recording
    if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_ARM) {
        // no clock - time for step mode
        if(!midi_clock_get_running()) {
            seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_STEP);
        }
    }

    // handle step record mode
    // make sure we're still recording on this track
    if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_STEP) {
        // handle held note list
        switch(msg->status & 0xf0) {
            case MIDI_NOTE_OFF:
                // remove note
                for(i = 0; i < SEQ_TRACK_POLY; i ++) {
                    if(sestate.record_events[i].msg.status == MIDI_NOTE_ON &&
                            sestate.record_events[i].msg.data0 == msg->data0) {
                        sestate.record_events[i].msg.status = 0;
                        sestate.record_event_count --;
                    }
                }
                break;
            case MIDI_NOTE_ON:
                // keep track of note for advancing
                for(i = 0; i < SEQ_TRACK_POLY; i ++) {
                    if(sestate.record_events[i].msg.status == 0) {
                        sestate.record_events[i].msg.status = MIDI_NOTE_ON;
                        sestate.record_events[i].msg.data0 = msg->data0;
                        sestate.record_event_count ++;
                        break;
                    }
                }
                break;
        }

        // handle different message types
        switch(msg->status & 0xf0) {
            case MIDI_NOTE_OFF:
                // time to advance
                if(sestate.record_event_count <= 0) {
                    seq_engine_step_sequence_advance();
                    sestate.record_event_count = 0;
                }
                break;
            case MIDI_NOTE_ON:
                // make sure we have enough space to record
                if(sestate.record_event_count < SEQ_TRACK_POLY) {
                    // record the note
                    trkevent.type = SONG_EVENT_NOTE;
                    trkevent.data0 = msg->data0;
                    trkevent.data1 = msg->data1;
                    trkevent.length = sestate.step_size[sestate.first_track];
                    song_add_step_event(sestate.scene_current,
                        sestate.first_track, sestate.record_pos, &trkevent);
                }
                break;
            case MIDI_CONTROL_CHANGE:
                // handle damper pedal for inserting rests - all notes must be released
                if(msg->data0 == MIDI_CONTROLLER_DAMPER && msg->data1 == 127 &&
                        sestate.record_event_count == 0) {
                    song_clear_step(sestate.scene_current,
                        sestate.first_track, sestate.record_pos);
                    // move ahead to skip step
                    seq_engine_step_sequence_advance();
                }
                // other kinds of CC
                else if(msg->data0 < MIDI_CONTROLLER_ALL_SOUNDS_OFF) {
                    // add / edit the CC
                    trkevent.type = SONG_EVENT_CC;
                    trkevent.data0 = msg->data0;
                    trkevent.data1 = msg->data1;
                    song_add_step_event(sestate.scene_current,
                        sestate.first_track, sestate.record_pos, &trkevent);
                }
                break;
            default:
                // only NOTE events can be recorded in step sequencer mode
                break;
        }
    }
    // handle realtime record
    else if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_RT) {
        // make sure there is enough space in the list
        if(sestate.record_event_count == SEQ_ENGINE_RECORD_EVENTS_MAX) {
            return;
        }
        // handle different message types
        switch(msg->status & 0xf0) {
            case MIDI_NOTE_OFF:
                // search for note on already in recording list
                for(i = 0; i < sestate.record_event_count; i ++) {
                    // make sure this is the note we want
                    if(sestate.record_events[i].tick_len == 0 &&
                            sestate.record_events[i].msg.status == MIDI_NOTE_ON &&
                            sestate.record_events[i].msg.data0 == msg->data0) {
                        // record the note length to the existing data
                        sestate.record_events[i].tick_len = midi_clock_get_tick_pos() -
                            sestate.record_events[i].tick_pos;
                        break;
                    }
                }
                break;
            case MIDI_NOTE_ON:
                // add note to recording list
                sestate.record_events[sestate.record_event_count].tick_pos = midi_clock_get_tick_pos();
                sestate.record_events[sestate.record_event_count].tick_len = 0;
                sestate.record_events[sestate.record_event_count].msg.port = 0;
                sestate.record_events[sestate.record_event_count].msg.len = 3;
                sestate.record_events[sestate.record_event_count].msg.status = MIDI_NOTE_ON;
                sestate.record_events[sestate.record_event_count].msg.data0 = msg->data0;
                sestate.record_events[sestate.record_event_count].msg.data1 = msg->data1;
                sestate.record_event_count ++;
                break;
            case MIDI_CONTROL_CHANGE:
                // add CC to recording list
                sestate.record_events[sestate.record_event_count].tick_pos = midi_clock_get_tick_pos();
                sestate.record_events[sestate.record_event_count].tick_len = 0;  // unused
                sestate.record_events[sestate.record_event_count].msg.port = 0;
                sestate.record_events[sestate.record_event_count].msg.len = 3;
                sestate.record_events[sestate.record_event_count].msg.status = MIDI_CONTROL_CHANGE;
                sestate.record_events[sestate.record_event_count].msg.data0 = msg->data0;
                sestate.record_events[sestate.record_event_count].msg.data1 = msg->data1;
                sestate.record_event_count ++;
                break;
            default:
                break;
        }
    }
}

// advance the step sequencer position and possibly end recording
void seq_engine_step_sequence_advance(void) {
    sestate.record_pos = (sestate.record_pos + 1) & (SEQ_NUM_STEPS - 1);
    // end the sequence
    if(sestate.record_pos == ((sestate.motion_start[sestate.first_track] +
            sestate.motion_len[sestate.first_track]) & (SEQ_NUM_STEPS - 1))) {
        seq_ctrl_set_record_mode(SEQ_CTRL_RECORD_IDLE);  // end recording mode
        seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_OFF);  // turn off live mode
        return;
    }
    seq_engine_highlight_step_record_pos();
}

// move the step sequence position manually
void seq_engine_step_sequence_shuttle(int change) {
    // compute new position and wrap around
    seq_engine_compute_next_pos(sestate.first_track,
        &sestate.record_pos, change);
    seq_engine_highlight_step_record_pos();
}

// write the recorded tracks to the song
// if there is no data then the song is not overwritten
void seq_engine_record_write_tracks(void) {
    int i, j, rn, step, temp;
    int damper_held;
    int used_notes[128];  // note numbers
    struct track_event trkevent;

    // ignore blank recording
    if(sestate.record_event_count == 0) {
        return;
    }

    // keep track of damper state
    damper_held = 0;

    // event times are offset by approx. 1/2 step time
    // for drum track mode we need to figure out which notes are in our
    // recording and then clear them from the existing recording (selective
    // overdub) mode
    if(sestate.track_type[sestate.first_track] == SONG_TRACK_TYPE_DRUM) {
        // clear the used notes list
        for(i = 0; i < 128; i ++) {
            used_notes[i] = 0;
        }
        // scan the note list and figure out which pitches are in use
        for(rn = 0; rn < sestate.record_event_count; rn ++) {
            // make sure this event fits within our desired step range
            if(sestate.record_events[rn].tick_pos < sestate.record_pos ||
                    sestate.record_events[rn].tick_pos >=
                    (sestate.record_pos + (sestate.motion_len[sestate.first_track] *
                    sestate.step_size[sestate.first_track]))) {
                continue;
            }
            // flag the note
            if(sestate.record_events[rn].msg.status == MIDI_NOTE_ON) {
                used_notes[sestate.record_events[rn].msg.data0 & 0x7f] = 1;
            }
        }
        // scan the existing track and remove notes as necessary
        for(i = 0; i < sestate.motion_len[sestate.first_track]; i ++) {
            step = (sestate.motion_start[sestate.first_track] + i) &
                (SEQ_NUM_STEPS - 1);
            // check events on step that match a note that exists in our recording
            for(j = 0; j < SEQ_TRACK_POLY; j ++) {
                if(song_get_step_event(sestate.scene_current,
                        sestate.first_track, step, j, &trkevent) == -1) {
                    continue;
                }
                if(trkevent.type == SONG_EVENT_NOTE && used_notes[trkevent.data0 & 0x7f] == 1) {
                    song_clear_step_event(sestate.scene_current,
                        sestate.first_track, step, j);
                }
            }
        }
    }

    // attempt to insert recorded events into track
    for(rn = 0; rn < sestate.record_event_count; rn ++) {
        // make sure this event fits within our desired step range
        if(sestate.record_events[rn].tick_pos < sestate.record_pos ||
                sestate.record_events[rn].tick_pos >=
                (sestate.record_pos + (sestate.motion_len[sestate.first_track] *
                sestate.step_size[sestate.first_track]))) {
            continue;
        }
        // calculate actual step on track
        step = (((sestate.record_events[rn].tick_pos - sestate.record_pos) /
            sestate.step_size[sestate.first_track]) +
            sestate.motion_start[sestate.first_track]) & (SEQ_NUM_STEPS - 1);
        // insert event
        switch(sestate.record_events[rn].msg.status) {
            case MIDI_NOTE_ON:
                // key split excludes this note on this track
                if(seq_ctrl_get_num_tracks_selected() > 1 &&
                        seq_engine_check_key_split_range(sestate.key_split[sestate.first_track],
                        trkevent.data0) == 0) {
                    break;
                }
                trkevent.type = SONG_EVENT_NOTE;
                trkevent.data0 = sestate.record_events[rn].msg.data0;
                trkevent.data1 = sestate.record_events[rn].msg.data1;
                // note was held down past loop end (tick_len is 0)
                if(sestate.record_events[rn].tick_len == 0) {
                    trkevent.length = (((sestate.motion_start[sestate.first_track] +
                        sestate.motion_len[sestate.first_track]) - step) &
                        (SEQ_NUM_STEPS - 1)) * sestate.step_size[sestate.first_track];
                }
                // proper note length
                else {
                    trkevent.length = sestate.record_events[rn].tick_len;
                }
                song_add_step_event(sestate.scene_current,
                    sestate.first_track, step, &trkevent);
                break;
            case MIDI_CONTROL_CHANGE:
                temp = 0;  // update flag
                // check to see if we already have this CC on this step
                for(i = 0; i < SEQ_TRACK_POLY; i ++) {
                    // no event in slot
                    if(song_get_step_event(sestate.scene_current,
                            sestate.first_track, step, i, &trkevent) == -1) {
                        continue;
                    }
                    // CC matches - update value
                    if(trkevent.type == SONG_EVENT_CC &&
                            trkevent.data0 == sestate.record_events[rn].msg.data0) {
                        trkevent.data1 = sestate.record_events[rn].msg.data1;  // update value
                        song_set_step_event(sestate.scene_current,
                            sestate.first_track, step, i, &trkevent);
                        temp = 1;  // update flag
                        // check to see if we got the damper
                        if(sestate.record_events[rn].msg.data0 == MIDI_CONTROLLER_DAMPER) {
                            if(sestate.record_events[rn].msg.data1 > 0) {
                                damper_held = 1;
                            }
                            else {
                                damper_held = 0;
                            }
                        }
                        break;
                    }
                }
                // CC is not already on step - add it
                if(temp == 0) {
                    trkevent.type = SONG_EVENT_CC;
                    trkevent.data0 = sestate.record_events[rn].msg.data0;
                    trkevent.data1 = sestate.record_events[rn].msg.data1;
                    song_add_step_event(sestate.scene_current,
                        sestate.first_track, step, &trkevent);
                    // check to see if we got the damper
                    if(sestate.record_events[rn].msg.data0 == MIDI_CONTROLLER_DAMPER) {
                        if(sestate.record_events[rn].msg.data1 > 0) {
                            damper_held = 1;
                        }
                        else {
                            damper_held = 0;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
    // if the damper is still down at the end of the track
    // turn it off on the last step
    if(damper_held) {
        trkevent.type = SONG_EVENT_CC;
        trkevent.data0 = MIDI_CONTROLLER_DAMPER;
        trkevent.data1 = 0;
        step = (sestate.motion_start[sestate.first_track] +
            sestate.motion_len[sestate.first_track] - 1) & (SEQ_NUM_STEPS - 1);
        song_add_step_event(sestate.scene_current,
            sestate.first_track, step, &trkevent);
    }
}

//
// change event handlers
//
// the live mode changed
void seq_engine_live_mode_changed(int newval) {
    int track;
    switch(newval) {
        case SEQ_CTRL_LIVE_ON:
            // kill playback notes if live is turned on
            for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
                // if the track is selected and not muted
                if(seq_ctrl_get_track_select(track) && !sestate.track_mute[track]) {
                    // if arp enabled make sure we clear its input
                    if(sestate.arp_enable[track]) {
                        arp_clear_input(track);
                    }
                    seq_engine_track_stop_all_notes(track);
                }
            }
            break;
        case SEQ_CTRL_LIVE_OFF:
            // kill live notes if live is turned off
            for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
                // if the track is selected and not muted
                if(seq_ctrl_get_track_select(track) && !sestate.track_mute[track]) {
                    // if arp enabled make sure we clear its input
                    if(sestate.arp_enable[track]) {
                        arp_clear_input(track);
                    }
                    seq_engine_live_stop_all_notes(track);
                }
            }
            break;
        default:
            break;
    }
}

// the autolive mode changed
void seq_engine_autolive_mode_changed(int newval) {
    // disabling
    if(!newval) {
        // if we are not also in LIVE mode we should force notes to stop
        if(seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_OFF) {
            seq_engine_live_mode_changed(SEQ_CTRL_LIVE_OFF);
        }
    }
    sestate.autolive = newval;
}

// track select changed on a track
void seq_engine_track_select_changed(int track, int newval) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("setsc - track invalid: %d", track);
        return;
    }
    if(!newval) {
        // if live mode is active get rid of all live notes
        if(sestate.autolive || seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_ON) {
            // if arp enabled make sure we clear its input
            if(sestate.arp_enable[track]) {
                arp_clear_input(track);
            }
            seq_engine_live_stop_all_notes(track);
        }
    }
}

// mute select changed on a track
void seq_engine_mute_select_changed(int scene, int track, int newval) {
    // ignore scenes we are not handling now
    if(scene != sestate.scene_current) {
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("semsc - track invalid: %d", track);
        return;
    }
    if(newval) {
        sestate.track_mute[track] = 1;
        // stop notes on track that is now muted
        seq_engine_live_stop_all_notes(track);
    }
    else {
        sestate.track_mute[track] = 0;
    }
}

// the key split setting changed
void seq_engine_key_split_changed(int track, int mode) {
    int i;
    // stop all notes because we might not be able to turn them off now
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        seq_engine_live_stop_all_notes(i);
    }
}

// the arp type changed on a track
void seq_engine_arp_type_changed(int scene, int track, int type) {
    // ignore scenes we are not handling now
    if(scene != sestate.scene_current) {
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("seatc - track invalid: %d", track);
        return;
    }
    arp_set_type(track, type);
}

// the arp speed changed on a track
void seq_engine_arp_speed_changed(int scene, int track, int speed) {
    // ignore scenes we are not handling now
    if(scene != sestate.scene_current) {
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("seasc - track invalid: %d", track);
        return;
    }
    arp_set_speed(track, speed);
}

// the arp gate time changed on a track
void seq_engine_arp_gate_time_changed(int scene, int track, int time) {
    // ignore scenes we are not handling now
    if(scene != sestate.scene_current) {
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("seagtc - track invalid: %d", track);
        return;
    }
    arp_set_gate_time(track, time);
}

// the arp enable changed on a track
void seq_engine_arp_enable_changed(int scene, int track, int enable) {
    int i;
    // ignore scenes we are not handling now
    if(scene != sestate.scene_current) {
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("seaec - track invalid: %d", track);
        return;
    }

    // arp is enabled
    if(enable) {
        arp_set_arp_enable(track, 1);  // enable arp
        // if live is enabled on the track
        if(seq_ctrl_get_track_select(track) &&
               seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_ON) {
            // let's transfer notes to the arp
            for(i = 0; i < SEQ_ENGINE_MAX_NOTES; i ++) {
                // note is active
                if(sestate.live_active_notes[track][i].status != 0) {
                    arp_handle_input(track, &sestate.live_active_notes[track][i]);
                }
            }
            // turn off live notes
            seq_engine_live_stop_all_notes(track);
        }
        // if we're doing playback then stop all the playback notes on the track
        else {
            seq_engine_track_stop_all_notes(track);
        }
    }
    // arp is disabled
    else {
        arp_set_arp_enable(track, 0);  // disable arp
    }
    outproc_stop_all_notes(track);  // make sure notes are off
}

//
// misc
//
// a song was loaded - time to reset stuff
void seq_engine_song_loaded(int song) {
    int track, mapnum;
    sestate.scene_current = (SEQ_NUM_SCENES - 1);
    sestate.scene_next = 0;
    seq_engine_set_kbtrans(0);  // reset kbtrans
    // load stuff from song
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        for(mapnum = 0; mapnum < SEQ_NUM_TRACK_OUTPUTS; mapnum ++) {
            seq_engine_send_program(track, mapnum);
        }
    }
    sestate.autolive = song_get_midi_autolive();
    seq_engine_recalc_params();
}

// recalculate the running parameters from the song
void seq_engine_recalc_params(void) {
    int track, dist_start, dist_end;
    sestate.midi_clock_source = song_get_midi_clock_source();
    sestate.first_track = seq_ctrl_get_first_track();
    sestate.key_velocity_scale = song_get_key_velocity_scale();
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        sestate.key_split[track] =
            song_get_key_split(track);
        sestate.bias_track_map[track] =
            song_get_bias_track(sestate.scene_current, track);
        sestate.arp_enable[track] =
            song_get_arp_enable(sestate.scene_current, track);
        sestate.step_size[track] =
            seq_utils_step_len_to_ticks(song_get_step_length(sestate.scene_current, track));
        sestate.motion_start[track] =
            song_get_motion_start(sestate.scene_current, track);
        sestate.motion_len[track] =
            song_get_motion_length(sestate.scene_current, track);
        sestate.dir_reverse[track] =
            song_get_motion_dir(sestate.scene_current, track);
        sestate.gate_time[track] =
            song_get_gate_time(sestate.scene_current, track);
        sestate.track_type[track] =
            song_get_track_type(track);
        sestate.track_mute[track] =
            song_get_mute(sestate.scene_current, track);
        // if we're stopped we need to check if the playback position is
        // outside of the new motion range and correct it if so
        if(!seq_ctrl_get_run_state()) {
            // check the range
            if(!seq_utils_get_wrapped_range(sestate.step_pos[track],
                    sestate.motion_start[track], sestate.motion_len[track],
                    SEQ_NUM_STEPS)) {
                dist_start = (sestate.motion_start[track] - sestate.step_pos[track]) &
                    (SEQ_NUM_STEPS - 1);
                dist_end = (sestate.step_pos[track] - sestate.motion_start[track] +
                    sestate.motion_len[track] + 1) & (SEQ_NUM_STEPS - 1);
                // we moved to before
                if(dist_end > dist_start) {
                    sestate.step_pos[track] = sestate.motion_start[track];
                }
                // we moved to after
                else {
                    sestate.step_pos[track] = (sestate.motion_start[track] +
                        sestate.motion_len[track] - 1) & (SEQ_NUM_STEPS - 1);
                }
                // fire event
                state_change_fire2(SCE_ENG_ACTIVE_STEP, track,
                    sestate.step_pos[track]);
            }
        }
    }
}

// check if the current step pos of a track is the first step
// this handles direction of playback
int seq_engine_is_first_step(int track) {
    // backwards
    if(sestate.dir_reverse[track] &&
            sestate.step_pos[track] == ((sestate.motion_start[track] +
            sestate.motion_len[track] - 1) & (SEQ_NUM_STEPS - 1))) {
        return 1;
    }
    // forwards
    if(!sestate.dir_reverse[track] &&
            sestate.step_pos[track] == sestate.motion_start[track]) {
        return 1;
    }
    return 0;
}

// move to the next step on a track - this handles direction of playback
// returns 1 if the position wrapped around to the start/end
int seq_engine_move_to_next_step(int track) {
    // backwards
    if(sestate.dir_reverse[track]) {
        return seq_engine_compute_next_pos(track,
            &sestate.step_pos[track], -1);
    }
    // forwards
    return seq_engine_compute_next_pos(track,
        &sestate.step_pos[track], 1);
}

// compute the position of the next step and return it - handling wrapping
// returns 1 if the position wrapped around to the start/end
int seq_engine_compute_next_pos(int track, int *pos, int change) {
    int newpos;
    if(change == 0) {
        return 0;  // didn't wrap
    }
    // get new pos
    newpos = (*pos + change) & (SEQ_NUM_STEPS - 1);
    // check if we wrapped forward
    if(change > 0 && ((newpos - sestate.motion_start[track]) & (SEQ_NUM_STEPS - 1)) >=
            sestate.motion_len[track]) {
        *pos = sestate.motion_start[track];
        return 1;
    }
    // check if we wrapped backward
    if(change < 0 && ((newpos - sestate.motion_start[track]) & (SEQ_NUM_STEPS - 1)) >=
            sestate.motion_len[track]) {
        *pos = (sestate.motion_start[track] +
            sestate.motion_len[track] - 1) & (SEQ_NUM_STEPS - 1);
        return 1;
    }
    *pos = newpos;
    return 0;  // didn't wrap
}

// chance scenes since we are now synchronized
void seq_engine_change_scene_synced(void) {
    int track;
    // no change
    if(sestate.scene_current == sestate.scene_next) {
        return;
    }
    sestate.scene_current = sestate.scene_next;
    seq_engine_recalc_params();
    seq_engine_reset_all_tracks_pos();

    // fire event
    state_change_fire1(SCE_ENG_CURRENT_SCENE, sestate.scene_current);

    // update engine with new transpose info
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        // update outproc for tonality and transpose
        outproc_transpose_changed(sestate.scene_current, track);
        outproc_tonality_changed(sestate.scene_current, track);
        // update arp settings for new scene
        arp_set_arp_enable(track,
            song_get_arp_enable(sestate.scene_current, track));
        arp_set_type(track,
            song_get_arp_type(sestate.scene_current, track));
        arp_set_speed(track,
            song_get_arp_speed(sestate.scene_current, track));
        arp_set_gate_time(track,
            song_get_arp_gate_time(sestate.scene_current, track));
    }
}

// cancel a pending scene change
void seq_engine_cancel_pending_scene_change(void) {
    sestate.scene_next = sestate.scene_current;
}

// reset the position of all tracks to the scene defaults
void seq_engine_reset_all_tracks_pos(void) {
    int track;
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        sestate.clock_div_count[track] = 0;
        if(sestate.dir_reverse[track]) {
            sestate.step_pos[track] = (sestate.motion_start[track] +
                sestate.motion_len[track] - 1) & (SEQ_NUM_STEPS - 1);
        }
        else {
            sestate.step_pos[track] = sestate.motion_start[track];
        }
        // fire event
        state_change_fire2(SCE_ENG_ACTIVE_STEP, track, sestate.step_pos[track]);
    }
}

// send the current MIDI program on a track
void seq_engine_send_program(int track, int mapnum) {
    int prog;
    struct midi_msg send_msg;
    prog = song_get_midi_program(track, mapnum);
    if(prog != SONG_MIDI_PROG_NULL) {
        midi_utils_enc_program_change(&send_msg, 0, 0, prog);
        outproc_deliver_msg(sestate.scene_current, track, &send_msg,
            mapnum, OUTPROC_OUTPUT_PROCESSED);
    }
}

// send all notes off on a track
void seq_engine_send_all_notes_off(int track) {
    struct midi_msg send_msg;
    midi_utils_enc_control_change(&send_msg, 0, 0,
        MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
    outproc_deliver_msg(sestate.scene_current, track, &send_msg,
        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);
}

// highlight the step position on the first selected track
void seq_engine_highlight_step_record_pos() {
    gui_grid_clear_overlay();
    gui_grid_set_overlay_color(sestate.record_pos, GUI_OVERLAY_HIGH);
}

// checks if a note is within the supplied key split range
// returns 1 if the note is in range, 0 otherwise
int seq_engine_check_key_split_range(int mode, int note) {
    switch(mode) {
        case SONG_KEY_SPLIT_LEFT:  // left hand
            if(note < SEQ_KEY_SPLIT_NOTE) {
                return 1;
            }
            return 0;
        case SONG_KEY_SPLIT_RIGHT:  // right hand
            if(note >= SEQ_KEY_SPLIT_NOTE) {
                return 1;
            }
            return 0;
        case SONG_KEY_SPLIT_OFF:  // no key split
        default:
            return 1;
    }
}
