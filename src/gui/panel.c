/*
 * CARBON Panel Handler
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
#include "panel.h"
#include "gui.h"
#include "panel_menu.h"
#include "pattern_edit.h"
#include "step_edit.h"
#include "song_edit.h"
#include "../iface/iface_panel.h"
#include "../midi/midi_stream.h"
#include "../midi/midi_utils.h"
#include "../seq/seq_ctrl.h"
#include "../seq/song.h"
#include "../seq/seq_engine.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"
#include "../panel_if.h"
#include "../power_ctrl.h"
#include <stdio.h>
#include <stdlib.h>

// edit modes to be selected for control routing
#define PANEL_EDIT_MODE_NONE 0
#define PANEL_EDIT_MODE_STEP 1
#define PANEL_EDIT_MODE_SONG 2
#define PANEL_EDIT_MODE_PATTERN 3

// panel power state for LEDs
#define PANEL_POWER_STATE_STANDBY 0
#define PANEL_POWER_STATE_IF 1
#define PANEL_POWER_STATE_ON 2
#define PANEL_POWER_STATE_ERROR 3

// settings
#define PANEL_KEY_QUEUE_LEN 256  // must be a power of 2
#define PANEL_KEY_QUEUE_MASK (PANEL_KEY_QUEUE_LEN - 1)

// queue definition for handling keys
struct panel_key_queue {
    int ctrl;
    int val;
};

//
// panel state
//
struct panel_state {
    int8_t shift_state;  // shift button state
    int16_t shift_tap_timeout;  // shift timeout
    int8_t scene_state;  // scene button state
    int8_t track_hold_state[SEQ_NUM_TRACKS];  // track buttons being held
    int8_t bl_record;  // backlight record state
    int8_t bl_live;  // backlight live state
    int8_t bl_scene_hold;  // backlight scene hold state
    int8_t bl_edit;  // backlight edit state
    int8_t bl_song_mode;  // backlight song mode state
    int8_t bl_power_state;  // power state for LEDs
    uint8_t led_state[PANEL_LED_NUM_LEDS];  // current LED state
    uint8_t led_restore_state[PANEL_LED_NUM_LEDS];  // restore LED state
    struct panel_key_queue key_queue[PANEL_KEY_QUEUE_LEN];  // input queue
    int key_queue_inp;  // input queue in pos
    int key_queue_outp;  // input queue out pos
    int beat_led_timeout;  // timeout for beat LED
};

//
// panel state
//
struct panel_state pstate;

// local functions
void panel_clear_leds(void);
void panel_handle_state_change(int event_type, int *data, int data_len);
void panel_handle_key_queue(void);
void panel_handle_if_input(int ctrl, int val);
void panel_handle_seq_input(int ctrl, int val);
void panel_handle_track_select(int track, int state);
void panel_handle_mute_select(int track);
void panel_handle_reset(void);
void panel_update_bl_display(void);
void panel_update_arp_led(void);
void panel_update_song_led(void);
void panel_update_live_led(void);
void panel_update_dir_led(void);
void panel_update_record_led(void);
void panel_update_run_led(int state);
void panel_update_track_led(int track, int state);
int panel_get_edit_mode(void);
void panel_cancel_edit_mode(void);
void panel_handle_shift_double_tap(void);

// init the panel
void panel_init(void) {
    int i;

    // panel input queue
    pstate.key_queue_inp = 0;
    pstate.key_queue_outp = 0;
    pstate.beat_led_timeout = 0;

    // init modes
    pstate.shift_state = 0;
    pstate.shift_tap_timeout = 0;
    pstate.scene_state = 0;

    // turn off all panel LEDs
    for(i = 0; i < PANEL_LED_NUM_LEDS; i ++) {
        pstate.led_state[i] = PANEL_LED_STATE_OFF;
        pstate.led_restore_state[i] = pstate.led_state[i];
    }
    panel_clear_leds();

    // clear track selectors
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        pstate.track_hold_state[i] = 0;
    }

    // select track 1
    panel_handle_track_select(0, 1);  // press
    panel_handle_track_select(0, 0);  // release

    // set the backlight modes
    pstate.bl_record = 0;
    pstate.bl_live = 0;
    pstate.bl_song_mode = 0;
    pstate.bl_scene_hold = 0;
    pstate.bl_edit = 0;
    pstate.bl_power_state = PANEL_POWER_STATE_STANDBY;
    panel_update_bl_display();

    // init the panel menu
    panel_menu_init();

    // register for events
    state_change_register(panel_handle_state_change, SCEC_SONG);
    state_change_register(panel_handle_state_change, SCEC_CTRL);
    state_change_register(panel_handle_state_change, SCEC_ENG);
    state_change_register(panel_handle_state_change, SCEC_POWER);
}

// run the panel timer task - run on the realtime thread
void panel_timer_task(void) {
    // handle panel input
    panel_handle_key_queue();

    // handle beat LED
    if(pstate.beat_led_timeout) {
        pstate.beat_led_timeout --;
        if(pstate.beat_led_timeout == 0) {
            panel_set_led(PANEL_LED_CLOCK, 0);
        }
    }

    // timeout the shift double tap counter
    if(pstate.shift_tap_timeout) {
        pstate.shift_tap_timeout --;
    }

    // menu tasks
    panel_menu_timer_task();

    // misc
    panel_update_bl_display();
}

// handle a control from the panel - this may be called on an interrupt
// the event is buffered until the next panel timer task can handle it
void panel_handle_input(int ctrl, int val) {
    // queue the input to be processed on another thread
    pstate.key_queue[pstate.key_queue_inp].ctrl = ctrl;
    pstate.key_queue[pstate.key_queue_inp].val = val;
    pstate.key_queue_inp = (pstate.key_queue_inp + 1) & PANEL_KEY_QUEUE_MASK;
}

//
// getters and setters
//
// blink the beat LED
void panel_blink_beat_led(void) {
    panel_set_led(PANEL_LED_CLOCK, PANEL_LED_STATE_ON);
    pstate.beat_led_timeout = BEAT_LED_TIMEOUT;
}

// set the state of a panel LED
void panel_set_led(int led, int state) {
    if(led < 0 || led >= PANEL_LED_NUM_LEDS) {
        log_error("psl - led invalid: %d", led);
        return;
    }
    pstate.led_state[led] = state;
    switch(state) {
        case PANEL_LED_STATE_DIM:
            panel_if_set_led(led, 0x10);
            break;
        case PANEL_LED_STATE_ON:
            panel_if_set_led(led, 0xff);
            break;
        case PANEL_LED_STATE_BLINK:
            panel_if_blink_led(led, PANEL_LED_BLINK_OFF, PANEL_LED_BLINK_ON);
            break;
        case PANEL_LED_STATE_OFF:
        default:
            panel_if_set_led(led, 0x00);
            break;
    }
}

// set the state of a panel RGB LED - bits 2:0 = rgb state
void panel_set_bl_led(int led, int state) {
    int32_t color = 0;
    if(led < 0 || led > 1) {
        log_error("psbl - led invalid: %d", led);
        return;
    }
    if(state & 0x01) {
        color |= 0x0000ff;
    }
    if(state & 0x02) {
        color |= 0x00ff00;
    }
    if(state & 0x04) {
        color |= 0xff0000;
    }
    panel_if_set_rgb(led, color);
}

//
// local functions
//
// clear all panel LEDs
void panel_clear_leds(void) {
    int i;
    for(i = 0; i < PANEL_LED_NUM_LEDS; i ++) {
        pstate.led_restore_state[i] = pstate.led_state[i];
        panel_set_led(i, PANEL_LED_STATE_OFF);
    }
}

// restore panel LEDs to previous state
void panel_restore_leds(void) {
    int i;
    for(i = 0; i < PANEL_LED_NUM_LEDS; i ++) {
        panel_set_led(i, pstate.led_restore_state[i]);
    }
}

// handle state change
void panel_handle_state_change(int event_type, int *data, int data_len) {
    switch(event_type) {
        case SCE_SONG_MOTION_DIR:
            panel_update_dir_led();
            break;
        case SCE_SONG_ARP_ENABLE:
            panel_update_arp_led();
            break;
        case SCE_CTRL_RUN_STATE:
            panel_update_run_led(data[0]);
            break;
        case SCE_CTRL_TRACK_SELECT:
            // update all the things that depend on track
            panel_update_track_led(data[0], data[1]);
            break;
        case SCE_CTRL_FIRST_TRACK:
            panel_update_arp_led();
            panel_update_dir_led();
            break;
        case SCE_CTRL_SONG_MODE:
            panel_update_song_led();
            break;
        case SCE_CTRL_LIVE_MODE:
            panel_update_live_led();
            panel_update_bl_display();
            break;
        case SCE_CTRL_RECORD_MODE:
            panel_update_record_led();
            panel_update_bl_display();
            break;
        case SCE_ENG_CURRENT_SCENE:
            panel_update_arp_led();
            panel_update_dir_led();
            break;
        case SCE_POWER_STATE:
            switch(data[0]) {
                case POWER_CTRL_STATE_STANDBY:
                    // turn off all panel LEDs
                    panel_clear_leds();
                    pstate.bl_power_state = PANEL_POWER_STATE_STANDBY;
                    panel_update_bl_display();
                    break;
                case POWER_CTRL_STATE_IF:
                    // turn off all panel LEDs
                    panel_clear_leds();
                    pstate.bl_power_state = PANEL_POWER_STATE_IF;
                    panel_update_bl_display();
                    break;
                case POWER_CTRL_STATE_ON:
                    // restore all panel LEDs
                    panel_restore_leds();
                    pstate.bl_power_state = PANEL_POWER_STATE_ON;
                    panel_update_bl_display();
                    break;
                case POWER_CTRL_STATE_ERROR:
                    pstate.bl_power_state = PANEL_POWER_STATE_ERROR;
                    panel_clear_leds();
                    panel_update_bl_display();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

// handles a key queue
void panel_handle_key_queue(void) {
    int ctrl;  // the pressed key
    int val;  // the up/down state

    // nothing to do
    if(pstate.key_queue_inp == pstate.key_queue_outp) {
        return;
    }

    // take an event out of the queue
    ctrl = pstate.key_queue[pstate.key_queue_outp].ctrl;
    val = pstate.key_queue[pstate.key_queue_outp].val;
    pstate.key_queue_outp = (pstate.key_queue_outp + 1) & PANEL_KEY_QUEUE_MASK;

    // route the panel input according to power state
    if(power_ctrl_get_power_state() == POWER_CTRL_STATE_ON) {
        panel_handle_seq_input(ctrl, val);
    }
    else {
        iface_panel_handle_input(ctrl, val);
    }
}

// handle the sequencer input (normal running mode)
void panel_handle_seq_input(int ctrl, int val) {
    // key press / encoder
    if(val) {
        switch(ctrl) {
            case PANEL_SW_SCENE:
                // song menu
                if(pstate.shift_state) {
                    // turn off song edit mode if we are on
                    if(panel_get_edit_mode() == PANEL_EDIT_MODE_SONG) {
                        song_edit_set_enable(0);
                    }
                    // record is not active so we can start song edit
                    else if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_IDLE) {
                        panel_cancel_edit_mode();
                        song_edit_set_enable(1);
                    }
                }
                // scene
                else {
                    pstate.scene_state = 1;  // scene is pressed
                    panel_update_bl_display();
                }
                break;
            case PANEL_SW_ARP:
                panel_cancel_edit_mode();
                // arp menu (without changing state)
                if(pstate.shift_state) {
                    panel_menu_set_mode(PANEL_MENU_ARP);
                }
                // arp
                else {
                    seq_ctrl_flip_arp_enable();
                    if(seq_ctrl_get_arp_enable(seq_ctrl_get_first_track())) {
                        // prevent menu from dismissing if we entered it with shift
                        if(panel_menu_get_mode() != PANEL_MENU_ARP) {
                            panel_menu_set_mode(PANEL_MENU_ARP);  // call up menu
                        }
                    }
                    else {
                        if(panel_menu_get_mode() == PANEL_MENU_ARP) {
                            panel_menu_set_mode(PANEL_MENU_NONE);  // dismiss menu
                        }
                    }
                }
                break;
            case PANEL_SW_LIVE:
                if(pstate.shift_state) {
                    if(seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_KBTRANS) {
                        seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_OFF);
                    }
                    else {
                        seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_KBTRANS);
                    }
                }
                else {
                    if(seq_ctrl_get_live_mode() == SEQ_CTRL_LIVE_OFF) {
                        seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_ON);
                    }
                    else {
                        seq_ctrl_set_live_mode(SEQ_CTRL_LIVE_OFF);
                    }
                }
                break;
            case PANEL_SW_1:
                // scene select 1
                if(pstate.scene_state) {
                    // shift-select copies current scene to selected scene
                    if(pstate.shift_state) {
                        seq_ctrl_copy_scene(0);
                    }
                    // otherwise just set the scene
                    else {
                        seq_ctrl_set_scene(0);
                    }
                }
                // track mute
                else if(pstate.shift_state) {
                    panel_handle_mute_select(0);
                }
                // track select 1
                else {
                    panel_handle_track_select(0, 1);
                }
                break;
            case PANEL_SW_2:
                // scene select 2
                if(pstate.scene_state) {
                    // shift-select copies current scene to selected scene
                    if(pstate.shift_state) {
                        seq_ctrl_copy_scene(1);
                    }
                    // otherwise just set the scene
                    else {
                        seq_ctrl_set_scene(1);
                    }
                }
                // track mute
                else if(pstate.shift_state) {
                    panel_handle_mute_select(1);
                }
                // track select 2
                else {
                    panel_handle_track_select(1, 1);
                }
                break;
            case PANEL_SW_3:
                // scene select 3
                if(pstate.scene_state) {
                    // shift-select copies current scene to selected scene
                    if(pstate.shift_state) {
                        seq_ctrl_copy_scene(2);
                    }
                    // otherwise just set the scene
                    else {
                        seq_ctrl_set_scene(2);
                    }
                }
                // track mute
                else if(pstate.shift_state) {
                    panel_handle_mute_select(2);
                }
                // track select 3
                else {
                    panel_handle_track_select(2, 1);
                }
                break;
            case PANEL_SW_4:
                // scene select 4
                if(pstate.scene_state) {
                    // shift-select copies current scene to selected scene
                    if(pstate.shift_state) {
                        seq_ctrl_copy_scene(3);
                    }
                    // otherwise just set the scene
                    else {
                        seq_ctrl_set_scene(3);
                    }
                }
                // track mute
                else if(pstate.shift_state) {
                    panel_handle_mute_select(3);
                }
                // track select 4
                else {
                    panel_handle_track_select(3, 1);
                }
                break;
            case PANEL_SW_5:
                // scene select 5
                if(pstate.scene_state) {
                    // shift-select copies current scene to selected scene
                    if(pstate.shift_state) {
                        seq_ctrl_copy_scene(4);
                    }
                    // otherwise just set the scene
                    else {
                        seq_ctrl_set_scene(4);
                    }
                }
                // track mute
                else if(pstate.shift_state) {
                    panel_handle_mute_select(4);
                }
                // track select 5
                else {
                    panel_handle_track_select(4, 1);
                }
                break;
            case PANEL_SW_6:
                // scene select 6
                if(pstate.scene_state) {
                    // shift-select copies current scene to selected scene
                    if(pstate.shift_state) {
                        seq_ctrl_copy_scene(5);
                    }
                    // otherwise just set the scene
                    else {
                        seq_ctrl_set_scene(5);
                    }
                }
                // track mute
                else if(pstate.shift_state) {
                    panel_handle_mute_select(5);
                }
                // track select 6
                else {
                    panel_handle_track_select(5, 1);
                }
                break;
            case PANEL_SW_MIDI:
                panel_cancel_edit_mode();
                // sys
                if(pstate.shift_state) {
                    panel_menu_set_mode(PANEL_MENU_SYS);
                }
                // MIDI
                else {
                    panel_menu_set_mode(PANEL_MENU_MIDI);
                }
                break;
            case PANEL_SW_CLOCK:
                // tap
                if(pstate.shift_state) {
                    seq_ctrl_tap_tempo();
                }
                // clock
                else {
                    panel_menu_set_mode(PANEL_MENU_CLOCK);
                }
                break;
            case PANEL_SW_DIR:
                // magic
                if(pstate.shift_state) {
                    seq_ctrl_make_magic();
                }
                // dir
                else {
                    seq_ctrl_flip_motion_dir();
                }
                break;
            case PANEL_SW_TONALITY:
                panel_cancel_edit_mode();
                // swing
                if(pstate.shift_state) {
                    panel_menu_set_mode(PANEL_MENU_SWING);
                }
                // tonality
                else {
                    panel_menu_set_mode(PANEL_MENU_TONALITY);
                }
                break;
            case PANEL_SW_LOAD:
                panel_cancel_edit_mode();
                // save
                if(pstate.shift_state) {
                    panel_menu_set_mode(PANEL_MENU_SAVE);
                }
                // load
                else {
                    panel_menu_set_mode(PANEL_MENU_LOAD);
                }
                break;
            case PANEL_SW_RUN_STOP:
                // reset
                if(pstate.shift_state) {
                    panel_handle_reset();
                }
                // run/stop
                else {
                    if(seq_ctrl_get_run_state()) {
                        seq_ctrl_set_run_state(0);
                    }
                    else {
                        seq_ctrl_set_run_state(1);
                    }
                }
                break;
            case PANEL_SW_RECORD:
                // clear
                if(pstate.shift_state) {
                    switch(panel_get_edit_mode()) {
                        case PANEL_EDIT_MODE_STEP:
                            step_edit_clear_step();
                            break;
                        case PANEL_EDIT_MODE_SONG:
                            song_edit_remove_step();
                            break;
                        case PANEL_EDIT_MODE_PATTERN:
                            pattern_edit_restore_pattern();
                            break;
                        default:
                            seq_ctrl_make_clear();
                            break;
                    }
                }
                // record
                else {
                    seq_ctrl_record_pressed();
                }
                break;
            case PANEL_SW_EDIT:
                // record is not active so we can start an editing mode
                if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_IDLE) {
                    // pattern edit
                    if(pstate.shift_state) {
                        if(panel_get_edit_mode() == PANEL_EDIT_MODE_PATTERN) {
                            pattern_edit_set_enable(0);
                        }
                        else {
                            panel_cancel_edit_mode();
                            pattern_edit_set_enable(1);
                        }
                    }
                    // step edit
                    else {
                        // step edit retrigger
                        if(panel_get_edit_mode() == PANEL_EDIT_MODE_STEP) {
                            step_edit_set_enable(1);
                        }
                        // start step edit from scratch
                        else {
                            panel_cancel_edit_mode();
                            step_edit_set_enable(1);
                        }
                    }
                }
                break;
            case PANEL_SW_SHIFT:
                pstate.shift_state = 1;  // shift is pressed
                panel_handle_shift_double_tap();  // handle canceling of modes
                break;
            case PANEL_SW_SONG_MODE:
                seq_ctrl_toggle_song_mode();
#ifdef GFX_REMLCD_MODE
                // allow screen to be redrawn for client
                gui_force_refresh();
#endif
                break;
            case PANEL_ENC_SPEED:
                switch(panel_get_edit_mode()) {
                    case PANEL_EDIT_MODE_STEP:
                        step_edit_adjust_start_delay(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_SONG:
                    case PANEL_EDIT_MODE_PATTERN:
                    default:
                        // tempo fine
                        if(pstate.shift_state) {
                            seq_ctrl_adjust_tempo(seq_utils_enc_val_to_change(val), 1);
                        }
                        // tempo coarse
                        else {
                            seq_ctrl_adjust_tempo(seq_utils_enc_val_to_change(val), 0);
                        }
                        break;
                }
                break;
            case PANEL_ENC_GATE_TIME:
                switch(panel_get_edit_mode()) {
                    case PANEL_EDIT_MODE_STEP:
                        step_edit_adjust_gate_time(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_SONG:
                    case PANEL_EDIT_MODE_PATTERN:
                        // no action
                        break;
                    default:
                        // fine gate time
                        if(pstate.shift_state) {
                            seq_ctrl_adjust_gate_time(seq_utils_enc_val_to_change(val));
                        }
                        // coarse gate time
                        else {
                            seq_ctrl_adjust_gate_time(seq_utils_enc_val_to_change(val) *
                                SEQ_GATE_TIME_STEP_SIZE);
                        }
                        break;
                }
                break;
            case PANEL_ENC_MOTION_START:
                switch(panel_get_edit_mode()) {
                    case PANEL_EDIT_MODE_STEP:
                        step_edit_adjust_cursor(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_SONG:
                        song_edit_adjust_cursor(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_PATTERN:
                        pattern_edit_adjust_cursor(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    default:
                        if(panel_menu_get_mode() != PANEL_MENU_NONE) {
                            panel_menu_adjust_cursor(
                                seq_utils_enc_val_to_change(val),
                                pstate.shift_state);
                        }
                        // step record mode
                        else if(seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_ARM ||
                                seq_ctrl_get_record_mode() == SEQ_CTRL_RECORD_STEP) {
                            seq_engine_step_rec_pos_changed(seq_utils_enc_val_to_change(val));
                        }
                        // normal mode
                        else {
                            seq_ctrl_adjust_motion_start(seq_utils_enc_val_to_change(val));
                        }
                        break;
                }
                break;
            case PANEL_ENC_MOTION_LENGTH:
                switch(panel_get_edit_mode()) {
                    case PANEL_EDIT_MODE_STEP:
                        step_edit_adjust_velocity(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_SONG:
                        song_edit_adjust_length(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_PATTERN:
                        pattern_edit_adjust_step(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    default:
                        // menu mode for encoder
                        if(panel_menu_get_mode() != PANEL_MENU_NONE) {
                            panel_menu_adjust_value(seq_utils_enc_val_to_change(val),
                                pstate.shift_state);
                        }
                        // track step length
                        else if(pstate.shift_state) {
                            seq_ctrl_adjust_step_length(seq_utils_enc_val_to_change(val));
                        }
                        // motion length
                        else {
                            seq_ctrl_adjust_motion_length(seq_utils_enc_val_to_change(val));
                        }
                        break;
                }
                break;
            case PANEL_ENC_PATTERN_TYPE:
                switch(panel_get_edit_mode()) {
                    case PANEL_EDIT_MODE_STEP:
                        step_edit_adjust_ratchet_mode(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_SONG:
                        song_edit_adjust_scene(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_PATTERN:
                    default:
                        seq_ctrl_adjust_pattern_type(seq_utils_enc_val_to_change(val));
                        break;
                }
                break;
            case PANEL_ENC_TRANSPOSE:
                switch(panel_get_edit_mode()) {
                    case PANEL_EDIT_MODE_STEP:
                        step_edit_adjust_note(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_SONG:
                        song_edit_adjust_kbtrans(seq_utils_enc_val_to_change(val),
                            pstate.shift_state);
                        break;
                    case PANEL_EDIT_MODE_PATTERN:
                        // no action
                        break;
                    default:
                        seq_ctrl_adjust_transpose(seq_utils_enc_val_to_change(val));
                        break;
                }
                break;
            default:
                log_error("phsi - invalid ctrl: %d", ctrl);
                break;
        }
    }
    // key release
    else {
        switch(ctrl) {
            case PANEL_SW_SCENE:
                pstate.scene_state = 0;
                panel_update_bl_display();
                break;
            case PANEL_SW_ARP:
                break;
            case PANEL_SW_LIVE:
                break;
            case PANEL_SW_1:
                panel_handle_track_select(0, 0);
                break;
            case PANEL_SW_2:
                panel_handle_track_select(1, 0);
                break;
            case PANEL_SW_3:
                panel_handle_track_select(2, 0);
                break;
            case PANEL_SW_4:
                panel_handle_track_select(3, 0);
                break;
            case PANEL_SW_5:
                panel_handle_track_select(4, 0);
                break;
            case PANEL_SW_6:
                panel_handle_track_select(5, 0);
                break;
            case PANEL_SW_MIDI:
                break;
            case PANEL_SW_CLOCK:
                break;
            case PANEL_SW_DIR:
                break;
            case PANEL_SW_TONALITY:
                break;
            case PANEL_SW_LOAD:
                break;
            case PANEL_SW_RUN_STOP:
                break;
            case PANEL_SW_RECORD:
                break;
            case PANEL_SW_EDIT:
                break;
            case PANEL_SW_SHIFT:
                pstate.shift_state = 0;  // always reset this
                break;
            case PANEL_SW_SONG_MODE:
                break;
            default:
                log_error("phsi - invalid ctrl: %d", ctrl);
                break;
        }
    }
}

// handle track select
void panel_handle_track_select(int track, int state) {
    int i;
    static int held = 0;
    int current_select[SEQ_NUM_TRACKS];
    int new_select[SEQ_NUM_TRACKS];
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("phts - track invalid: %d", track);
        return;
    }

    // get current select
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        if(seq_ctrl_get_track_select(i)) {
            current_select[i] = 1;
            new_select[i] = 1;
        }
        else {
            current_select[i] = 0;
            new_select[i] = 0;
        }
    }

    // pressed
    if(state) {
        // single select mode
        if(!held) {
            // turn off other tracks
            for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
                if(i == track) {
                    continue;
                }
                new_select[i] = 0;
            }
        }
        new_select[track] = 1;
        pstate.track_hold_state[track] = 1;
    }
    // released
    else {
        pstate.track_hold_state[track] = 0;
    }

    // update state - select select before unselect
    // XXX using bit fields could make sending this data to the selector smoother
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        if(new_select[i] != current_select[i] &&
                new_select[i]) {
            seq_ctrl_set_track_select(i, new_select[i]);
        }
    }
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        if(new_select[i] != current_select[i] &&
                !new_select[i]) {
            seq_ctrl_set_track_select(i, new_select[i]);
        }
    }

    // check if something is held
    held = 0;
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        if(pstate.track_hold_state[i]) {
            held = 1;
        }
    }
}

// handle mute toggling for a track
void panel_handle_mute_select(int track) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("phms - track invalid: %d", track);
        return;
    }
    if(seq_ctrl_get_mute_select(track)) {
        seq_ctrl_set_mute_select(track, 0);
    }
    else {
        seq_ctrl_set_mute_select(track, 1);
    }
}

// handle resetting a track or the whole sequencer
void panel_handle_reset(void) {
    int i;
    int track_held = 0;
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        if(pstate.track_hold_state[i]) {
            seq_ctrl_reset_track(i);
            track_held = 1;
        }
    }
    if(track_held) {
        return;
    }
    seq_ctrl_reset_pos();
}

// update the backlight display
void panel_update_bl_display(void) {
    static uint32_t color = 0xffffffff;  // impossible color
    uint32_t new_color;
    // these are arranged in priority order highest to lowest
    if(pstate.bl_power_state == PANEL_POWER_STATE_ERROR) {
        new_color = PANEL_BL_COLOR_POWER_ERROR;
    }
    else if(pstate.bl_power_state == PANEL_POWER_STATE_STANDBY) {
        new_color = PANEL_BL_COLOR_POWER_OFF;
    }
    else if(pstate.bl_power_state == PANEL_POWER_STATE_IF) {
        new_color = PANEL_BL_COLOR_POWER_IF_MODE;
    }
    else if(pstate.bl_record) {
        new_color = PANEL_BL_COLOR_RECORD;
    }
    else if(pstate.scene_state) {
        new_color = PANEL_BL_COLOR_SCENE_HOLD;
    }
    else if(pstate.bl_edit) {
        new_color = PANEL_BL_COLOR_EDIT;
    }
    else if(pstate.bl_live == SEQ_CTRL_LIVE_KBTRANS) {
        new_color = PANEL_BL_COLOR_KBTRANS;
    }
    else if(pstate.bl_live == SEQ_CTRL_LIVE_ON) {
        new_color = PANEL_BL_COLOR_LIVE;
    }
    else if(pstate.bl_song_mode) {
        new_color = PANEL_BL_COLOR_SONG_MODE;
    }
    else {
        new_color = PANEL_BL_COLOR_DEFAULT;
    }
    if(color != new_color) {
        color = new_color;
        panel_if_set_rgb(0, color);
        panel_if_set_rgb(1, color);
    }
}

// update the ARP LED
void panel_update_arp_led(void) {
    static int state = 0;
    int newstate = song_get_arp_enable(seq_ctrl_get_scene(),
        seq_ctrl_get_first_track());
    if(newstate != state) {
        if(newstate) {
            panel_set_led(PANEL_LED_ARP, PANEL_LED_STATE_ON);
        }
        else {
            panel_set_led(PANEL_LED_ARP, PANEL_LED_STATE_OFF);
        }
        state = newstate;
    }
}

// update the SONG mode LED
void panel_update_song_led(void) {
    if(seq_ctrl_get_song_mode()) {
        panel_set_led(PANEL_LED_SONG_MODE, PANEL_LED_STATE_ON);
        pstate.bl_song_mode = 1;
    }
    else {
        panel_set_led(PANEL_LED_SONG_MODE, PANEL_LED_STATE_OFF);
        pstate.bl_song_mode = 0;
    }
}

// update the LIVE LED
void panel_update_live_led(void) {
    static int state = SEQ_CTRL_LIVE_OFF;
    int newstate = seq_ctrl_get_live_mode();
    if(newstate != state) {
        switch(newstate) {
            case SEQ_CTRL_LIVE_ON:
                panel_set_led(PANEL_LED_LIVE, PANEL_LED_STATE_ON);
                break;
            case SEQ_CTRL_LIVE_KBTRANS:
                panel_set_led(PANEL_LED_LIVE, PANEL_LED_STATE_BLINK);
                break;
            case SEQ_CTRL_LIVE_OFF:
            default:
                panel_set_led(PANEL_LED_LIVE, PANEL_LED_STATE_OFF);
                break;
        }
        state = newstate;
        pstate.bl_live = newstate;
    }
}

// update the DIR LED
void panel_update_dir_led(void) {
    static int state = 0;
    int newstate = song_get_motion_dir(seq_ctrl_get_scene(),
        seq_ctrl_get_first_track());
    if(newstate != state) {
        if(newstate) {
            panel_set_led(PANEL_LED_DIR, PANEL_LED_STATE_ON);
        }
        else {
            panel_set_led(PANEL_LED_DIR, PANEL_LED_STATE_OFF);
        }
        state = newstate;
    }
}

// update the record LED
void panel_update_record_led(void) {
    static int state = SEQ_CTRL_RECORD_IDLE;
    int newstate = seq_ctrl_get_record_mode();
    if(newstate != state) {
        switch(newstate) {
            case SEQ_CTRL_RECORD_ARM:
                panel_set_led(PANEL_LED_RECORD, PANEL_LED_STATE_BLINK);
                break;
            case SEQ_CTRL_RECORD_STEP:
                panel_set_led(PANEL_LED_RECORD, PANEL_LED_STATE_ON);
                break;
            case SEQ_CTRL_RECORD_RT:
                panel_set_led(PANEL_LED_RECORD, PANEL_LED_STATE_ON);
                break;
            case SEQ_CTRL_RECORD_IDLE:
            default:
                panel_set_led(PANEL_LED_RECORD, PANEL_LED_STATE_OFF);
                break;
        }
        state = newstate;
        pstate.bl_record = newstate;
    }
}

// update the run LED
void panel_update_run_led(int state) {
    static int oldstate = 0;
    if(state != oldstate) {
        if(state) {
            panel_set_led(PANEL_LED_RUN_STOP, PANEL_LED_STATE_ON);
        }
        else {
            panel_set_led(PANEL_LED_RUN_STOP, PANEL_LED_STATE_OFF);
        }
        oldstate = state;
    }
}

// update a track select LED
void panel_update_track_led(int track, int state) {
    static int oldstate[SEQ_NUM_TRACKS] = { 0 };
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("putl - track invalid: %d", track);
        return;
    }
    if(state != oldstate[track]) {
        if(state) {
            panel_set_led(PANEL_LED_1 + track, PANEL_LED_STATE_ON);
        }
        else {
            panel_set_led(PANEL_LED_1 + track, PANEL_LED_STATE_OFF);
        }
        oldstate[track] = state;
    }
}

// get the current edit mode
int panel_get_edit_mode(void) {
    if(song_edit_get_enable()) {
        return PANEL_EDIT_MODE_SONG;
    }
    else if(step_edit_get_enable()) {
        return PANEL_EDIT_MODE_STEP;
    }
    else if(pattern_edit_get_enable()) {
        return PANEL_EDIT_MODE_PATTERN;
    }
    return PANEL_EDIT_MODE_NONE;
}

// cancel edit mode
void panel_cancel_edit_mode(void) {
    // step edit mode is active
    if(panel_get_edit_mode() == PANEL_EDIT_MODE_STEP) {
        step_edit_set_enable(0);  // cancel step edit
    }
    // song edit mode is active
    else if(panel_get_edit_mode() == PANEL_EDIT_MODE_SONG) {
        song_edit_set_enable(0);  // cancel song edit
    }
    // pattern edit is active
    else if(panel_get_edit_mode() == PANEL_EDIT_MODE_PATTERN) {
        pattern_edit_set_enable(0);  // cancel pattern edit
    }
}

// handle shift double-tap to exit menu
void panel_handle_shift_double_tap(void) {
    if(pstate.shift_tap_timeout) {
        // step edit mode is active
        if(panel_get_edit_mode() == PANEL_EDIT_MODE_STEP) {
            step_edit_set_enable(0);
        }
        // song edit mode is active
        else if(panel_get_edit_mode() == PANEL_EDIT_MODE_SONG) {
            song_edit_set_enable(0);
        }
        // normal mode
        else {
            panel_menu_set_mode(PANEL_MENU_NONE);
        }
    }
    pstate.shift_tap_timeout = PANEL_SHIFT_TAP_TIMEOUT;
}
