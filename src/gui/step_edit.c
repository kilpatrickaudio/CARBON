/*
 * CARBON Step Editor
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
#include "step_edit.h"
#include "../seq/conditional.h"
#include "gui.h"
#include "panel_menu.h"
#include "../config.h"
#include "../gfx.h"
#include "../seq/outproc.h"
#include "../seq/arp.h"
#include "../seq/scale.h"
#include "../seq/midi_ctrl.h"
#include "../seq/seq_ctrl.h"
#include "../seq/seq_engine.h"
#include "../seq/song.h"
#include "../util/log.h"
#include "../util/panel_utils.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"
#include <stdio.h>
#include <string.h>

// step adjust modes
#define STEP_EDIT_ADJUST_NOTE 0
#define STEP_EDIT_ADJUST_VELO 1
#define STEP_EDIT_ADJUST_GATE_TIME 2
#define STEP_EDIT_ADJUST_START_DELAY 3

// constants
#define STEP_EDIT_EVENT_POS_ALL -1
#define STEP_EDIT_NOTE_SLOT_FREE -1
#define STEP_EDIT_CCI_HINT_MS 2000

struct step_edit_state {
    int enable;  // 0 = disabled, 1 = enabled
    int edit_timeout;  // timeout counter for editing
    int scene;  // selected edit scene
    int track;  // selected edit track
    int step_pos;  // current step pos for editing
    int event_pos;  // the event edit pos or STEP_EDIT_EVENT_POS_ALL for global
    int cci_hint_timeout;  // milliseconds remaining for title help
    int cci_hint_controller;
    int cci_hint_value;
    int cci_hint_show_value;
    int last_adjust_type;  // the last type of adjustment
    // preview events
    int8_t playing_notes[SEQ_TRACK_POLY];  // playing notes
    uint16_t playing_note_timeouts[SEQ_TRACK_POLY];  // timeouts for playing notes
    // recording input events
    int8_t recording_notes[SEQ_TRACK_POLY];  // keep track of input notes
};
struct step_edit_state sedits;

// local functions
void step_edit_adjust_step(int change, int mode, int single, int shift);
void step_edit_update_display(void);
static void step_edit_update_title(void);
static void step_edit_show_cci_hint(void);
void step_edit_play_step(void);
void step_edit_stop_notes(void);
void step_edit_send_note_on(int note, int velocity);
void step_edit_send_note_off(int note);
void step_edit_send_cc(int controller, int value);
void step_edit_send_program_change(int program);
void step_edit_clear_recording_notes(void);
void step_edit_add_recording_note(int note);
void step_edit_remove_recording_note(int note);
int step_edit_get_num_recording_notes(void);
int step_edit_does_step_contain_event(int type, int data0);

// init the step edit mode
void step_edit_init(void) {
    sedits.enable = 0;
    sedits.edit_timeout = 0;
    sedits.cci_hint_timeout = 0;
    sedits.scene = 0;
    sedits.track = 0;
    sedits.step_pos = 0;
    sedits.event_pos = STEP_EDIT_EVENT_POS_ALL;
    sedits.last_adjust_type = STEP_EDIT_ADJUST_NOTE;
    // register events
    state_change_register(step_edit_handle_state_change, SCEC_CTRL);
    state_change_register(step_edit_handle_state_change, SCEC_ENG);
}

// run step edit timer task
void step_edit_timer_task(void) {
    if(sedits.enable && sedits.cci_hint_timeout > 0) {
        if(--sedits.cci_hint_timeout == 0 ||
                (sedits.cci_hint_show_value &&
                 (sedits.cci_hint_controller == MIDI_CTRL_CC_EUCLIDEAN_ROTATION ||
                  sedits.cci_hint_controller == MIDI_CTRL_CC_EUCLIDEAN_DENSITY) &&
                 sedits.cci_hint_timeout % 100 == 0))
            step_edit_update_title();
    }
    // time out step edit mode
    if(sedits.edit_timeout) {
        sedits.edit_timeout --;
        if(sedits.edit_timeout == 0) {
            step_edit_set_enable(0);
        }
    }
}

// the clock ticked - do note timeouts for preview
void step_edit_run(uint32_t tick_count) {
    int i;

    // time out note notes
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        if(sedits.playing_note_timeouts[i]) {
            sedits.playing_note_timeouts[i] --;
            // time to turn off the note
            if(sedits.playing_note_timeouts[i] == 0 &&
                    sedits.playing_notes[i] != STEP_EDIT_NOTE_SLOT_FREE) {
                step_edit_send_note_off(sedits.playing_notes[i]);
                sedits.playing_notes[i] = STEP_EDIT_NOTE_SLOT_FREE;
            }
        }
    }
}

// handle state change
void step_edit_handle_state_change(int event_type, int *data, int data_len) {
    switch(event_type) {
        case SCE_ENG_CURRENT_SCENE:
        case SCE_CTRL_FIRST_TRACK:
            // if we're already enabled we should reset it
            if(sedits.enable) {
                step_edit_set_enable(0);
                step_edit_set_enable(1);
            }
            break;
    }
}

// handle a input from the keyboard
void step_edit_handle_input(struct midi_msg *msg) {
    int i;
    struct track_event event;
    // we only care about MIDI if we're enabled
    if(!sedits.enable) {
        return;
    }

    // handle incoming MIDI types
    switch(msg->status & 0xf0) {
        case MIDI_NOTE_ON:
            sedits.edit_timeout = panel_menu_get_timeout();
            // if this is the first note we need to clear other notes off the step
            if(step_edit_get_num_recording_notes() == 0) {
                for(i = 0; i < SEQ_TRACK_POLY; i ++) {
                    // no data on step
                    if(song_get_step_event(sedits.scene, sedits.track,
                            sedits.step_pos, i, &event) == -1) {
                        continue;
                    }
                    // for voice tracks we clear the whole step
                    if(event.type == SONG_EVENT_NOTE &&
                            song_get_track_type(sedits.track) == SONG_TRACK_TYPE_VOICE) {
                        song_clear_step_event(sedits.scene, sedits.track,
                            sedits.step_pos, i);
                    }
                }
            }
            // add / edit the new note
            event.type = SONG_EVENT_NOTE;
            event.data0 = msg->data0;
            event.data1 = msg->data1;
            event.length = seq_utils_step_len_to_ticks(
                song_get_step_length(sedits.scene, sedits.track));
            song_add_step_event(sedits.scene, sedits.track,
                sedits.step_pos, &event);
            step_edit_update_display();
            step_edit_add_recording_note(msg->data0);
            break;
        case MIDI_NOTE_OFF:
            sedits.edit_timeout = panel_menu_get_timeout();
            step_edit_remove_recording_note(msg->data0);
            break;
        case MIDI_CONTROL_CHANGE:
            sedits.edit_timeout = panel_menu_get_timeout();
            // add / edit the new CC
            event.type = SONG_EVENT_CC;
            event.data0 = msg->data0;
            event.data1 = msg->data1;
            song_add_step_event(sedits.scene, sedits.track,
                sedits.step_pos, &event);
            step_edit_update_display();
            break;
    }
}

// get whether step edit mode is enabled
int step_edit_get_enable(void) {
    return sedits.enable;
}

// start and stop step edit mode
void step_edit_set_enable(int enable) {
    sedits.cci_hint_timeout = 0;
    // start edit
    if(enable) {
        // we're already editing - just trigger the note
        if(sedits.enable) {
            step_edit_adjust_cursor(0, 0);  // causes the step to be played / displayed
            sedits.edit_timeout = panel_menu_get_timeout();
        }
        // start editing from scratch
        else {
            sedits.enable = 1;
            sedits.edit_timeout = panel_menu_get_timeout();
            sedits.scene = seq_ctrl_get_scene();
            sedits.track = seq_ctrl_get_first_track();
            panel_menu_set_mode(PANEL_MENU_NONE);  // disable any menu mode
            gui_grid_clear_overlay();
            gui_grid_set_overlay_enable(1);
            gui_clear_status_text_all();
            gui_set_status_override(1);  // take over status display
            step_edit_adjust_cursor(0, 0);  // causes the step to be played / displayed
            step_edit_clear_recording_notes();
        }
    }
    // stop edit - only if we're actually enabled
    else if(sedits.enable) {
        sedits.enable = 0;
        sedits.edit_timeout = 0;
        step_edit_stop_notes();
        gui_grid_set_overlay_enable(0);
        gui_clear_status_text_all();  // XXX get rid of this
        gui_set_status_override(0);  // give back status display
    }
}

// adjust the cursor position
void step_edit_adjust_cursor(int change, int shift) {
    int val;
    sedits.edit_timeout = panel_menu_get_timeout();
    sedits.cci_hint_timeout = 0;

    // select event pos
    if(shift) {
        sedits.event_pos = seq_utils_clamp(sedits.event_pos + change,
            STEP_EDIT_EVENT_POS_ALL, SEQ_TRACK_POLY - 1);
        step_edit_show_cci_hint();
        step_edit_update_display();
    }
    // select step
    else {
        val = sedits.step_pos + change;
        if(val < 0) {
            val = SEQ_NUM_STEPS - 1;
        }
        else if(val >= SEQ_NUM_STEPS) {
            val = 0;
        }
        // disable old step
        step_edit_stop_notes();
        gui_grid_set_overlay_color(sedits.step_pos, GUI_OVERLAY_BLANK);
        // enable new step
        sedits.step_pos = val;
        // display / play step
        step_edit_update_display();
        step_edit_play_step();
        gui_grid_set_overlay_color(sedits.step_pos, GUI_OVERLAY_HIGH);
    }
}

// adjust the step note
void step_edit_adjust_note(int change, int shift) {
    struct track_event event;
    sedits.edit_timeout = panel_menu_get_timeout();

    // shift + transpose cycles the selected event type
    if(shift && sedits.event_pos != STEP_EDIT_EVENT_POS_ALL &&
            song_get_step_event(sedits.scene, sedits.track, sedits.step_pos,
            sedits.event_pos, &event) != -1) {
        static const uint8_t types[] = {SONG_EVENT_NOTE, SONG_EVENT_CC,
            SONG_EVENT_INTERNAL_CC, SONG_EVENT_PROGCHG, SONG_EVENT_PROBABILITY,
            SONG_EVENT_EUCLIDEAN, SONG_EVENT_PRE, SONG_EVENT_NON_PRE, SONG_EVENT_HARMONY};
        int index;
        for(index = 0; index < 9; index ++) {
            if(types[index] == event.type) break;
        }
        index = (index + (change > 0 ? 1 : 8)) % 9;
        event.type = types[index];
        event.data0 = 0;
        event.data1 = 0;
        event.length = 0;
        switch(event.type) {
            case SONG_EVENT_NOTE:
                event.data0 = STEP_EDIT_NEW_NOTE;
                event.data1 = STEP_EDIT_NEW_NOTE_VELOCITY;
                event.length = seq_utils_step_len_to_ticks(
                    song_get_step_length(sedits.scene, sedits.track)) >> 1;
                break;
            case SONG_EVENT_CC:
                event.data0 = STEP_EDIT_NEW_CC;
                event.data1 = STEP_EDIT_NEW_CC_VAL;
                break;
            case SONG_EVENT_INTERNAL_CC:
                event.data0 = MIDI_CTRL_CC_EUCLIDEAN_DENSITY;
                event.data1 = STEP_EDIT_NEW_CC_VAL;
                break;
            case SONG_EVENT_HARMONY:
                event.data0 = SONG_HARMONY_SCALE_KEEP;
                event.data1 = SONG_HARMONY_ROOT_KEEP;
                break;
            case SONG_EVENT_PROBABILITY:
                event.data0 = 50;
                break;
            default:
                // PC=1, Euclidean=1:2 rotation 0, Pre/non-Pre unused.
                break;
        }
        song_set_step_event(sedits.scene, sedits.track, sedits.step_pos,
            sedits.event_pos, &event);
        sedits.cci_hint_timeout = 0;
        step_edit_show_cci_hint();
        step_edit_update_display();
        return;
    }

    if(sedits.event_pos == STEP_EDIT_EVENT_POS_ALL) {
        step_edit_adjust_step(change, STEP_EDIT_ADJUST_NOTE, 0, shift);
    }
    else {
        step_edit_adjust_step(change, STEP_EDIT_ADJUST_NOTE, 1, shift);
    }
}

// adjust the step velocity
void step_edit_adjust_velocity(int change, int shift) {
    sedits.edit_timeout = panel_menu_get_timeout();
    if(sedits.event_pos == STEP_EDIT_EVENT_POS_ALL) {
        step_edit_adjust_step(change, STEP_EDIT_ADJUST_VELO, 0, shift);
    }
    else {
        step_edit_adjust_step(change, STEP_EDIT_ADJUST_VELO, 1, shift);
    }
}

// adjust the step gate time
void step_edit_adjust_gate_time(int change, int shift) {
    sedits.edit_timeout = panel_menu_get_timeout();
    if(sedits.event_pos == STEP_EDIT_EVENT_POS_ALL) {
        step_edit_adjust_step(change, STEP_EDIT_ADJUST_GATE_TIME, 0, shift);
    }
    else {
        step_edit_adjust_step(change, STEP_EDIT_ADJUST_GATE_TIME, 1, shift);
    }
}

// adjust the start delay
void step_edit_adjust_start_delay(int change, int shift) {
    sedits.edit_timeout = panel_menu_get_timeout();
    step_edit_adjust_step(change, STEP_EDIT_ADJUST_START_DELAY, 0, shift);
}

// adjust the ratchet mode
void step_edit_adjust_ratchet_mode(int change, int shift) {
    sedits.edit_timeout = panel_menu_get_timeout();
    song_set_ratchet_mode(sedits.scene, sedits.track, sedits.step_pos,
        seq_utils_clamp(song_get_ratchet_mode(sedits.scene, sedits.track,
        sedits.step_pos) + change,
        SEQ_RATCHET_MIN, SEQ_RATCHET_MAX));
    // display / play step
    step_edit_update_display();
    step_edit_play_step();
}

// clear the current step
void step_edit_clear_step(void) {
    sedits.cci_hint_timeout = 0;
    sedits.edit_timeout = panel_menu_get_timeout();
    step_edit_stop_notes();
    // clear specific event
    if(sedits.event_pos != STEP_EDIT_EVENT_POS_ALL) {
        song_clear_step_event(sedits.scene, sedits.track,
            sedits.step_pos, sedits.event_pos);
    }
    // clear all events on step
    else {
        song_clear_step(sedits.scene, sedits.track, sedits.step_pos);
    }
    // display / play step
    step_edit_update_display();
}

//
// local functions
//
// One detent per distinct playback setting, using a stable representative raw
// value for each bucket. External CCs retain their ordinary raw-value editor.
static int step_edit_adjust_cci_value(int controller, int raw, int change) {
    int direction = change > 0 ? 1 : -1;
    int steps = seq_utils_clamp(change > 0 ? change : -change, 0, 128);
    int decoded, next, candidate;
    if(!change) return raw;
    if(controller == MIDI_CTRL_CC_EUCLIDEAN_ROTATION) {
        // Usability compromise: retain intermediate fractions for all targets,
        // rather than skipping buckets based on the first target's length.
        return seq_utils_clamp(raw + change * 2, 0, 127);
    }
    if(!midi_ctrl_internal_cc_supported(controller))
        return seq_utils_clamp(raw + change * 2, 0, 127);
    while(steps-- > 0) {
        decoded = midi_ctrl_internal_cc_to_value(controller, raw, SEQ_NUM_STEPS);
        candidate = raw + direction;
        while(candidate >= 0 && candidate <= 127 &&
                midi_ctrl_internal_cc_to_value(controller, candidate, SEQ_NUM_STEPS) == decoded)
            candidate += direction;
        if(candidate < 0 || candidate > 127) break;
        next = midi_ctrl_internal_cc_to_value(controller, candidate, SEQ_NUM_STEPS);
        // Canonical first raw value makes forward/backward navigation stable.
        while(candidate > 0 &&
                midi_ctrl_internal_cc_to_value(controller, candidate - 1, SEQ_NUM_STEPS) == next)
            candidate--;
        raw = candidate;
    }
    return raw;
}

// adjust all events on a step
void step_edit_adjust_step(int change, int mode, int single, int shift) {
    int inhibit_preview = 0;
    struct track_event event;
    int i, start, num, temp;

    // stop playing before editing
    step_edit_stop_notes();

    // create a note or a CC depending on what has been turned
    // if there are no events or the current edit pos is blank
    if(song_get_num_step_events(sedits.scene, sedits.track, sedits.step_pos) == 0 ||
            (sedits.event_pos != STEP_EDIT_EVENT_POS_ALL &&
            song_get_step_event(sedits.scene, sedits.track, sedits.step_pos,
            sedits.event_pos, &event) == -1)) {
        // figure out single slot position
        if(single && sedits.event_pos != STEP_EDIT_EVENT_POS_ALL) {
            start = sedits.event_pos;
        }
        // otherwise insert the event on the first slot
        else {
            start = 0;
        }
        // if shift was pressed we insert a CC
        if(shift) {
            // create a new CC value
            event.type = SONG_EVENT_CC;
            // prevent inserting a duplicate value on another slot
            if(step_edit_does_step_contain_event(SONG_EVENT_CC, STEP_EDIT_NEW_CC) != -1) {
                event.data0 = STEP_EDIT_NEW_CC + 1;
            }
            else {
                event.data0 = STEP_EDIT_NEW_CC;
            }
            event.data1 = STEP_EDIT_NEW_CC_VAL;
            inhibit_preview = 1;  // user must retrigger edit to hear
        }
        // otherwise insert a note
        else {
            // create a new note
            event.type = SONG_EVENT_NOTE;
            // prevent inserting a duplicate value on another slot
            if(step_edit_does_step_contain_event(SONG_EVENT_NOTE, STEP_EDIT_NEW_NOTE) != -1) {
                event.data0 = STEP_EDIT_NEW_NOTE + 1;
            }
            else {
                event.data0 = STEP_EDIT_NEW_NOTE;
            }
            event.data1 = STEP_EDIT_NEW_NOTE_VELOCITY;
            // default to half the step len
            event.length = seq_utils_step_len_to_ticks(
                song_get_step_length(seq_ctrl_get_scene(), sedits.track)) >> 1;
        }
        song_set_step_event(sedits.scene, sedits.track, sedits.step_pos,
            start, &event);
    }
    // otherwise let's edit the existing events
    else {
        // figure out edit pos for single
        if(single && sedits.event_pos != STEP_EDIT_EVENT_POS_ALL) {
            start = sedits.event_pos;
            num = 1;
        }
        // otherwise edit all
        else {
            start = 0;
            num = SEQ_TRACK_POLY;
        }

        // modify events and put them back into the step
        for(i = start; i < (start + num); i ++) {
            // get the slot event
            if(song_get_step_event(sedits.scene, sedits.track,
                    sedits.step_pos, i, &event) == -1) {
                continue;
            }
            // edit the contents
            switch(mode) {
                case STEP_EDIT_ADJUST_NOTE:  // note number and CC number
                    // note
                    if(event.type == SONG_EVENT_NOTE) {
                        event.data0 += change;
                        // the new value would collide with another slot
                        if(step_edit_does_step_contain_event(SONG_EVENT_NOTE, event.data0) != -1) {
                            if(change > 0) {
                                event.data0 ++;
                            }
                            else {
                                event.data0 --;
                            }
                        }
                        // note went out of range
                        if(event.data0 < STEP_EDIT_LOWEST_NOTE ||
                                event.data0 > STEP_EDIT_HIGHEST_NOTE) {
                            continue;
                        }
                        sedits.last_adjust_type = STEP_EDIT_ADJUST_NOTE;
                    }
                    // external CC
                    else if(event.type == SONG_EVENT_CC) {
                        event.data0 += change;
                        // the new value would collide with another slot
                        if(step_edit_does_step_contain_event(event.type, event.data0) != -1) {
                            if(change > 0) {
                                event.data0 ++;
                            }
                            else {
                                event.data0 --;
                            }
                        }
                        inhibit_preview = 1;  // user must retrigger edit to hear
                        // CC went out of range
                        if(event.data0 < 0 || event.data0 > 127) {
                            continue;
                        }
                    }
                    // internal CC: only the per-track CC 16-29 destinations
                    else if(event.type == SONG_EVENT_INTERNAL_CC) {
                        event.data0 = seq_utils_clamp(event.data0 + change,
                            MIDI_CTRL_CC_STEP_LENGTH,
                            MIDI_CTRL_CC_EUCLIDEAN_ROTATION);
                        if(single) {
                            sedits.cci_hint_controller = event.data0;
                            sedits.cci_hint_show_value = 0;
                            sedits.cci_hint_timeout = STEP_EDIT_CCI_HINT_MS;
                        }
                        inhibit_preview = 1;
                    }
                    else if(event.type == SONG_EVENT_HARMONY) {
                        event.data0 = seq_utils_clamp(event.data0 + change,
                            SONG_HARMONY_SCALE_KEEP, SONG_HARMONY_SCALE_MAX);
                        inhibit_preview = 1;
                    }
                    // probability occupies the first numeric field only
                    else if(event.type == SONG_EVENT_PROBABILITY) {
                        event.data0 = seq_utils_clamp(event.data0 + change, 0, 100);
                        inhibit_preview = 1;
                    }
                    else if(event.type == SONG_EVENT_EUCLIDEAN) {
                        unsigned a, b;
                        event.data0 = seq_utils_clamp(event.data0 + change, 0, 27);
                        conditional_pair(event.data0, &a, &b);
                        event.data1 %= b;
                        inhibit_preview = 1;
                    }
                    // program change
                    else if(event.type == SONG_EVENT_PROGCHG) {
                        event.data0 = seq_utils_clamp(event.data0 + change, 0, 127);
                        inhibit_preview = 1;  // user must retrigger edit to hear
                    }
                    // put back modified event
                    song_set_step_event(sedits.scene, sedits.track,
                        sedits.step_pos, i, &event);
                    break;
                case STEP_EDIT_ADJUST_VELO:  // note velocity and CC value
                    // note or CC
                    if(event.type == SONG_EVENT_NOTE) {
                        event.data1 = seq_utils_clamp(event.data1 + change * 2, 0, 127);
                    }
                    // external or internal CC
                    else if(event.type == SONG_EVENT_CC ||
                            event.type == SONG_EVENT_INTERNAL_CC) {
                        event.data1 = event.type == SONG_EVENT_INTERNAL_CC
                            ? step_edit_adjust_cci_value(event.data0, event.data1, change)
                            : seq_utils_clamp(event.data1 + change * 2, 0, 127);
                        if(single && event.type == SONG_EVENT_INTERNAL_CC) {
                            sedits.cci_hint_controller = event.data0;
                            sedits.cci_hint_value = event.data1;
                            sedits.cci_hint_show_value = 1;
                            sedits.cci_hint_timeout = STEP_EDIT_CCI_HINT_MS;
                        }
                        inhibit_preview = 1;  // user must retrigger edit to hear
                    }
                    else if(event.type == SONG_EVENT_HARMONY) {
                        event.data1 = seq_utils_clamp(event.data1 + change,
                            SONG_HARMONY_ROOT_KEEP, SONG_HARMONY_ROOT_MAX);
                        inhibit_preview = 1;
                    }
                    else if(event.type == SONG_EVENT_EUCLIDEAN) {
                        unsigned a, b;
                        conditional_pair(event.data0, &a, &b);
                        event.data1 = seq_utils_clamp(event.data1 + change, 0, b - 1);
                        inhibit_preview = 1;
                    }
                    sedits.last_adjust_type = STEP_EDIT_ADJUST_VELO;
                    // put back modified event
                    song_set_step_event(sedits.scene, sedits.track,
                        sedits.step_pos, i, &event);
                    break;
                case STEP_EDIT_ADJUST_GATE_TIME:
                    if(event.type == SONG_EVENT_NOTE) {
                        event.length = seq_utils_clamp(event.length +
                            seq_utils_warp_change(event.length, change, 10),
                            STEP_EDIT_SHORTEST_NOTE, STEP_EDIT_LONGEST_NOTE);
                    }
                    sedits.last_adjust_type = STEP_EDIT_ADJUST_GATE_TIME;
                    inhibit_preview = 1;  // user must retrigger edit to hear
                    // put back modified event
                    song_set_step_event(sedits.scene, sedits.track,
                        sedits.step_pos, i, &event);
                    break;
                case STEP_EDIT_ADJUST_START_DELAY:
                    if(event.type == SONG_EVENT_NOTE) {
                        temp = song_get_start_delay(sedits.scene,
                            sedits.track, sedits.step_pos);
                        song_set_start_delay(sedits.scene, sedits.track,
                            sedits.step_pos,
                            seq_utils_clamp(temp +
                            seq_utils_warp_change(temp, change, 10),
                            SEQ_START_DELAY_MIN, SEQ_START_DELAY_MAX));
                    }
                    sedits.last_adjust_type = STEP_EDIT_ADJUST_START_DELAY;
                    inhibit_preview = 1;  // user must retrigger edit to hear
                    break;
            }
        }
    }
    // display / play step
    step_edit_update_display();
    if(!inhibit_preview) {
        step_edit_play_step();
    }
}

// CCi descriptions match the manual's internal stepped automation table.
static const char *step_edit_cci_name(int controller) {
    static const char *const names[] = {
        "Step length", "Track transpose", "Track mute", "Motion start",
        "Motion length", "Motion direction", "Gate time", "Pattern type",
        "Arp enable", "Arp type", "Arp speed", "Arp gate time",
        "Euclidean density", "Euclidean rotation"
    };
    if(controller < MIDI_CTRL_CC_STEP_LENGTH || controller > MIDI_CTRL_CC_EUCLIDEAN_ROTATION)
        return NULL;
    return names[controller - MIDI_CTRL_CC_STEP_LENGTH];
}

static void step_edit_show_cci_hint(void) {
    struct track_event event;
    if(sedits.event_pos < 0) return;
    if(song_get_step_event(sedits.scene, sedits.track, sedits.step_pos,
            sedits.event_pos, &event) == -1 || event.type != SONG_EVENT_INTERNAL_CC ||
            !step_edit_cci_name(event.data0)) return;
    sedits.cci_hint_controller = event.data0;
    sedits.cci_hint_value = event.data1;
    sedits.cci_hint_show_value = 0;
    sedits.cci_hint_timeout = STEP_EDIT_CCI_HINT_MS;
}

// Both Euclidean hints use the first receiving track, not the source length.
static int step_edit_first_bias_target(int *length) {
    int target;
    for(target = 0; target < SEQ_NUM_TRACKS; target++) {
        if(target == sedits.track ||
                song_get_bias_track(sedits.scene, target) != sedits.track) continue;
        *length = sedits.scene == seq_engine_get_current_scene()
            ? seq_engine_get_effective_motion_length(target)
            : song_get_motion_length(sedits.scene, target);
        *length = seq_utils_clamp(*length, 1, SEQ_NUM_STEPS);
        return target;
    }
    return -1;
}

// Decode through the same conversion as playback; never alter stored MIDI values.
static void step_edit_cci_value_title(char *title, int controller, int raw) {
    char value[32];
    int decoded = midi_ctrl_internal_cc_to_value(controller, raw, SEQ_NUM_STEPS);
    const char *label = "Value";
    switch(controller) {
        case MIDI_CTRL_CC_STEP_LENGTH:
        case MIDI_CTRL_CC_ARP_SPEED:
            panel_utils_step_len_to_str(value, decoded);
            label = controller == MIDI_CTRL_CC_STEP_LENGTH ? "Step length" : "Arp speed";
            break;
        case MIDI_CTRL_CC_TRACK_TRANSPOSE:
            label = "Transpose";
            snprintf(value, sizeof(value), "%+d semis", decoded);
            break;
        case MIDI_CTRL_CC_TRACK_MUTE:
        case MIDI_CTRL_CC_ARP_ENABLE:
            label = controller == MIDI_CTRL_CC_TRACK_MUTE ? "Mute" : "Arp enable";
            snprintf(value, sizeof(value), "%s", decoded ? "On" : "Off");
            break;
        case MIDI_CTRL_CC_MOTION_DIR:
            label = "Direction";
            snprintf(value, sizeof(value), "%s", decoded ? "Reverse" : "Forward");
            break;
        case MIDI_CTRL_CC_MOTION_START:
        case MIDI_CTRL_CC_MOTION_LENGTH:
            label = controller == MIDI_CTRL_CC_MOTION_START ? "Start" : "Length";
            snprintf(value, sizeof(value), "%d", decoded + (controller == MIDI_CTRL_CC_MOTION_START));
            break;
        case MIDI_CTRL_CC_GATE_TIME:
            label = "Gate time";
            // Match gui_update_gate_time_override, including max=200%.
            snprintf(value, sizeof(value), "%d%%", (decoded + 1) * 200 / 256);
            break;
        case MIDI_CTRL_CC_PATTERN_TYPE:
            label = "Pattern type";
            snprintf(value, sizeof(value), "%d", decoded + 1);
            break;
        case MIDI_CTRL_CC_ARP_TYPE:
            label = "Arp type";
            arp_type_to_name(value, decoded);
            break;
        case MIDI_CTRL_CC_ARP_GATE_TIME:
            label = "Arp gate";
            snprintf(value, sizeof(value), "%d ticks", decoded);
            break;
        case MIDI_CTRL_CC_EUCLIDEAN_DENSITY: {
            int length, target = step_edit_first_bias_target(&length);
            if(target < 0)
                snprintf(title, GFX_LABEL_LEN, "CCi 28: %d%% No Bias target", decoded);
            else if(decoded == 100)
                // 100% restores the selected pattern, not an all-hits mask.
                snprintf(title, GFX_LABEL_LEN, "CCi 28: T%d 100%% Pattern", target + 1);
            else
                snprintf(title, GFX_LABEL_LEN, "CCi 28: T%d %d%% %d/%d",
                    target + 1, decoded, (decoded * length + 50) / 100, length);
            return;
        }
        case MIDI_CTRL_CC_EUCLIDEAN_ROTATION: {
            int length, target = step_edit_first_bias_target(&length);
            if(target >= 0) {
                decoded = midi_ctrl_internal_cc_to_value(controller, raw, length);
                snprintf(title, GFX_LABEL_LEN, "CCi 29: T%d Rot %d/%d",
                    target + 1, decoded, length);
                return;
            }
            snprintf(title, GFX_LABEL_LEN, "CCi 29: No Bias target");
            return;
        }
        default:
            snprintf(value, sizeof(value), "%d", raw);
            break;
    }
    snprintf(title, GFX_LABEL_LEN, "CCi %d: %s %s", controller, label, value);
}

static void step_edit_update_title(void) {
    char title[GFX_LABEL_LEN];
    const char *name = step_edit_cci_name(sedits.cci_hint_controller);
    struct track_event event;
    if(sedits.event_pos >= 0 && song_get_step_event(sedits.scene, sedits.track,
            sedits.step_pos, sedits.event_pos, &event) != -1 && event.type == SONG_EVENT_HARMONY) {
        char scale[32];
        if(event.data0 == SONG_HARMONY_SCALE_KEEP) snprintf(scale, sizeof(scale), "Keep");
        else scale_type_to_name(scale, seq_utils_clamp((int)event.data0 - 1, 0, SCALE_NUM_TONALITIES - 1));
        snprintf(title, sizeof(title), "Hrm: %s / %s", scale,
            event.data1 == SONG_HARMONY_ROOT_KEEP ? "Keep" :
            scale_root_name((int)event.data1 - SONG_HARMONY_ROOT_C));
        gui_set_status_text(0, title);
        return;
    }
    if(sedits.cci_hint_timeout > 0 && name && sedits.cci_hint_show_value)
        step_edit_cci_value_title(title, sedits.cci_hint_controller, sedits.cci_hint_value);
    else if(sedits.cci_hint_timeout > 0 && name)
        snprintf(title, sizeof(title), "CCi %d: %s", sedits.cci_hint_controller, name);
    else
        snprintf(title, sizeof(title), "Edit - Track: %2d Step: %2d",
            sedits.track + 1, sedits.step_pos + 1);
    gui_set_status_text(0, title);
}

// update the display
void step_edit_update_display(void) {
    int i, xpos, temp;
    char tempstr[GFX_LABEL_LEN];
    struct track_event event;
    gui_clear_status_text_all();
    step_edit_update_title();

    // left title
    gui_set_status_text_part(1, 0, 3, "R  ");  // ratchet title
    sprintf(tempstr, "%d  ", song_get_ratchet_mode(sedits.scene,
        sedits.track, sedits.step_pos));
    gui_set_status_text_part(2, 0, 3, tempstr);  // ratchet setting
    switch(sedits.last_adjust_type) {
        case STEP_EDIT_ADJUST_NOTE:
        case STEP_EDIT_ADJUST_VELO:
            gui_set_status_text_part(3, 0, 2, "V");
            break;
        case STEP_EDIT_ADJUST_GATE_TIME:
            gui_set_status_text_part(3, 0, 2, "G");
            break;
        case STEP_EDIT_ADJUST_START_DELAY:
            gui_set_status_text_part(3, 0, 2, "D");
            break;
    }

    //
    // handle event pos highlight
    //
    // global event pos
    if(sedits.event_pos >= 0) {
        for(i = 0; i < SEQ_TRACK_POLY; i ++) {
            if(i == sedits.event_pos) {
                gui_set_status_highlight_part(2, 3 + (4 * i), 3,
                    GFX_HIGHLIGHT_INVERT);
                gui_set_status_highlight_part(3, 3 + (4 * i), 3,
                    GFX_HIGHLIGHT_INVERT);
            }
            else {
                gui_set_status_highlight_part(2, 3 + (4 * i), 3,
                    GFX_HIGHLIGHT_NORMAL);
                gui_set_status_highlight_part(3, 3 + (4 * i), 3,
                    GFX_HIGHLIGHT_NORMAL);
            }
        }
    }
    else {
        gui_set_status_highlight_part(2, 3, 25, GFX_HIGHLIGHT_NORMAL);
        gui_set_status_highlight_part(3, 3, 25, GFX_HIGHLIGHT_NORMAL);
    }

    xpos = 3;
    //
    // display data for each event
    //
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        // get event
        song_get_step_event(sedits.scene, sedits.track,
            sedits.step_pos, i, &event);

        // display the step
        switch(event.type) {
            case SONG_EVENT_NOTE:
                gui_set_status_text_part(1, xpos, 4, "N");
                panel_utils_note_to_name(tempstr, event.data0, 1, 1);
                gui_set_status_text_part(2, xpos, 4, tempstr);
                switch(sedits.last_adjust_type) {
                    case STEP_EDIT_ADJUST_NOTE:
                    case STEP_EDIT_ADJUST_VELO:
                        // display note
                        sprintf(tempstr, "%-3d", event.data1);
                        gui_set_status_text_part(3, xpos, 4, tempstr);
                        break;
                    case STEP_EDIT_ADJUST_GATE_TIME:
                        // display length
                        panel_utils_gate_time_to_str(tempstr, event.length);
                        gui_set_status_text_part(3, xpos, 4, tempstr);
                        break;
                    case STEP_EDIT_ADJUST_START_DELAY:
                        // display time
                        temp = song_get_start_delay(sedits.scene, sedits.track,
                                sedits.step_pos);
                        if(temp == 0) {
                            sprintf(tempstr, "OFF");
                        }
                        else {
                            panel_utils_gate_time_to_str(tempstr, temp);
                        }
                        gui_set_status_text_part(3, xpos, 4, tempstr);
                        break;
                }
                break;
            case SONG_EVENT_CC:
                gui_set_status_text_part(1, xpos, 4, "CC");
                sprintf(tempstr, "%-3d", event.data0);
                gui_set_status_text_part(2, xpos, 4, tempstr);
                sprintf(tempstr, "%-3d", event.data1);
                gui_set_status_text_part(3, xpos, 4, tempstr);
                break;
            case SONG_EVENT_INTERNAL_CC:
                gui_set_status_text_part(1, xpos, 4, "CCi");
                sprintf(tempstr, "%-3d", event.data0);
                gui_set_status_text_part(2, xpos, 4, tempstr);
                sprintf(tempstr, "%-3d", event.data1);
                gui_set_status_text_part(3, xpos, 4, tempstr);
                break;
            case SONG_EVENT_HARMONY:
                gui_set_status_text_part(1, xpos, 4, "Hrm");
                if(event.data0 == SONG_HARMONY_SCALE_KEEP) sprintf(tempstr, "Keep");
                else sprintf(tempstr, "%d", event.data0);
                gui_set_status_text_part(2, xpos, 4, tempstr);
                gui_set_status_text_part(3, xpos, 4, event.data1 == SONG_HARMONY_ROOT_KEEP
                    ? "Keep" : (char *)scale_root_name((int)event.data1 - SONG_HARMONY_ROOT_C));
                break;
            case SONG_EVENT_PROBABILITY:
                gui_set_status_text_part(1, xpos, 4, "Prb");
                sprintf(tempstr, "%d%%", event.data0);
                gui_set_status_text_part(2, xpos, 4, tempstr);
                gui_set_status_text_part(3, xpos, 4, "---");
                break;
            case SONG_EVENT_EUCLIDEAN: {
                unsigned a, b;
                conditional_pair(event.data0, &a, &b);
                gui_set_status_text_part(1, xpos, 4, "Euc");
                sprintf(tempstr, "%u:%u", a, b);
                gui_set_status_text_part(2, xpos, 4, tempstr);
                sprintf(tempstr, "%u", event.data1 % b);
                gui_set_status_text_part(3, xpos, 4, tempstr);
                break;
            }
            case SONG_EVENT_PRE:
            case SONG_EVENT_NON_PRE:
                gui_set_status_text_part(1, xpos, 4,
                    event.type == SONG_EVENT_PRE ? "Pre" : "!Pr");
                gui_set_status_text_part(2, xpos, 4, "---");
                gui_set_status_text_part(3, xpos, 4, "---");
                break;
            case SONG_EVENT_PROGCHG:
                gui_set_status_text_part(1, xpos, 4, "PC");
                sprintf(tempstr, "%-3d", event.data0 + 1);
                gui_set_status_text_part(2, xpos, 4, tempstr);
                gui_set_status_text_part(3, xpos, 4, "---");
                break;
            default:  // blank slot
                gui_set_status_text_part(1, xpos, 4, "   ");
                gui_set_status_text_part(2, xpos, 4, "---");
                gui_set_status_text_part(3, xpos, 4, "---");
                break;
        }
        xpos += 4;
    }
}

// play events on the current step
void step_edit_play_step(void) {
    struct track_event event;
    int i;

    // don't play the step if we are running
    if(seq_ctrl_get_run_state()) {
        return;
    }

    // play each note on step
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        // get the event and play the note
        if(song_get_step_event(sedits.scene, sedits.track,
                sedits.step_pos, i, &event) == -1) {
            continue;
        }

        // play the step
        switch(event.type) {
            case SONG_EVENT_NOTE:
                // stop the note slot if it's not free
                if(sedits.playing_notes[i] != STEP_EDIT_NOTE_SLOT_FREE) {
                    step_edit_send_note_off(sedits.playing_notes[i]);
                    sedits.playing_notes[i] = STEP_EDIT_NOTE_SLOT_FREE;
                    sedits.playing_note_timeouts[i] = 0;  // time out note
                }
                // store the note data
                sedits.playing_notes[i] = event.data0;
                sedits.playing_note_timeouts[i] = event.length;
                step_edit_send_note_on(event.data0, event.data1);
                break;
            case SONG_EVENT_CC:
                step_edit_send_cc(event.data0, event.data1);
                break;
            case SONG_EVENT_INTERNAL_CC:
                // Internal automation is only applied during running playback.
                break;
            case SONG_EVENT_PROGCHG:
                step_edit_send_program_change(event.data0);
                break;
            default:
                continue;
        }
    }
}

// stop the notes on the current step
void step_edit_stop_notes(void) {
    int i;
    // stop each note on step
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        if(sedits.playing_notes[i] != STEP_EDIT_NOTE_SLOT_FREE) {
            step_edit_send_note_off(sedits.playing_notes[i]);
            sedits.playing_notes[i] = STEP_EDIT_NOTE_SLOT_FREE;
            sedits.playing_note_timeouts[i] = 0;  // time out note
        }
    }
}

// send a note on message
void step_edit_send_note_on(int note, int velocity) {
    struct midi_msg msg;
    midi_utils_enc_note_on(&msg, 0, 0, note, velocity);
    outproc_deliver_msg(sedits.scene, sedits.track, &msg,
        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);  // send note
}

// send a note off message
void step_edit_send_note_off(int note) {
    struct midi_msg msg;
    midi_utils_enc_note_off(&msg, 0, 0, note, 0x40);
    outproc_deliver_msg(sedits.scene, sedits.track, &msg,
        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);  // send note
}

// send a control change
void step_edit_send_cc(int controller, int value) {
    struct midi_msg msg;
    midi_utils_enc_control_change(&msg, 0, 0, controller, value);
    outproc_deliver_msg(sedits.scene, sedits.track, &msg,
        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);  // send CC
}

// send a program change
void step_edit_send_program_change(int program) {
    struct midi_msg msg;
    midi_utils_enc_program_change(&msg, 0, 0, program);
    outproc_deliver_msg(sedits.scene, sedits.track, &msg,
        OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);  // send program change
}

// clear the recording notes
void step_edit_clear_recording_notes(void) {
    int i;
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        sedits.recording_notes[i] = STEP_EDIT_NOTE_SLOT_FREE;
    }
}

// add a recording note
void step_edit_add_recording_note(int note) {
    int i;
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        if(sedits.recording_notes[i] == STEP_EDIT_NOTE_SLOT_FREE) {
            sedits.recording_notes[i] = note;
            return;
        }
    }
}

// remove a recording note
void step_edit_remove_recording_note(int note) {
    int i;
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        if(sedits.recording_notes[i] == note) {
            sedits.recording_notes[i] = STEP_EDIT_NOTE_SLOT_FREE;
        }
    }
}

// get the current number of recording notes
int step_edit_get_num_recording_notes(void) {
    int i, num = 0;
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        if(sedits.recording_notes[i] != STEP_EDIT_NOTE_SLOT_FREE) {
            num ++;
        }
    }
    return num;
}

// checks whether the step contains an event
// returns -1 if the event is not found, or the slot number if found
int step_edit_does_step_contain_event(int type, int data0) {
    struct track_event event;
    int i;
    // see if the event is already there
    for(i = 0; i < SEQ_TRACK_POLY; i ++) {
        // no data on step
        if(song_get_step_event(sedits.scene, sedits.track,
                sedits.step_pos, i, &event) == -1) {
            continue;
        }
        // event is found on a slot
        if(event.type == type && event.data0 == data0) {
            return i;
        }
    }
    return -1;
}
