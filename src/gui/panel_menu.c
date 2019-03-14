/*
 * CARBON Sequencer Panel Menus
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
#include "panel_menu.h"
#include "gui.h"
#include "../config.h"
#include "../config_store.h"
#include "../seq/arp.h"
#include "../seq/scale.h"
#include "../seq/seq_ctrl.h"
#include "../seq/song.h"
#include "../util/log.h"
#include "../util/panel_utils.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"
#include <stdio.h>

// panel menu state
struct panel_menu_state {
    int menu_mode;  // the current menu mode
    int menu_submode;  // the submode of the current mode
    int num_submodes;  // the number of submodes
    int menu_timeout;  // the timeout setting
    int menu_timeout_count;  // timeout to dismiss the menu
    int load_save_song;  // selected load or save song
};
struct panel_menu_state pmstate;

// local functions
int panel_menu_get_timeout(void);
void panel_menu_set_timeout(int timeout);
void panel_menu_handle_state_change(int event_type, int *data, int data_len);
void panel_menu_track_select_changed(void);
void panel_menu_update_prev_next(void);
// menu displays
void panel_menu_update_display(void);
void panel_menu_display_swing(void);
void panel_menu_display_tonality(void);
void panel_menu_display_arp(void);
void panel_menu_display_load(void);
void panel_menu_display_save(void);
void panel_menu_display_midi(void);
void panel_menu_display_sys(void);
void panel_menu_display_clock(void);
// menu editors
void panel_menu_edit_swing(int change);
void panel_menu_edit_tonality(int change);
void panel_menu_edit_arp(int change);
void panel_menu_edit_load(int change);
void panel_menu_edit_save(int change);
void panel_menu_edit_midi(int change);
void panel_menu_edit_sys(int change);
void panel_menu_edit_clock(int change);

// init the panel menu
void panel_menu_init(void) {
    pmstate.menu_mode = PANEL_MENU_NONE;
    pmstate.menu_submode = 0;
    pmstate.num_submodes = 0;
    pmstate.menu_timeout = PANEL_MENU_TIMEOUT_DEFAULT;
    pmstate.menu_timeout_count = 0;

    // register for events
    state_change_register(panel_menu_handle_state_change, SCEC_SONG);
    state_change_register(panel_menu_handle_state_change, SCEC_CTRL);
    state_change_register(panel_menu_handle_state_change, SCEC_ENG);
    state_change_register(panel_menu_handle_state_change, SCEC_CONFIG);
}

// run the panel timer task - run on the realtime thread
void panel_menu_timer_task(void) {
    // handle menu timeout
    if(pmstate.menu_timeout_count) {
        // only time out if we're not set to max = special non-timeout setting
        if(pmstate.menu_timeout_count != PANEL_MENU_TIMEOUT_MAX) {
            pmstate.menu_timeout_count --;
            if(pmstate.menu_timeout_count == 0) {
                panel_menu_set_mode(PANEL_MENU_NONE);
            }
        }
    }
}

// get the current panel menu mode
int panel_menu_get_mode(void) {
    return pmstate.menu_mode;
}

// handle entering or leaving a menu mode
void panel_menu_set_mode(int mode) {
    // same menu mode pressed - perhaps we need to dismiss it?
    if(mode == pmstate.menu_mode) {
        switch(pmstate.menu_mode) {
            case PANEL_MENU_NONE:
            case PANEL_MENU_SWING:
            case PANEL_MENU_TONALITY:
            case PANEL_MENU_ARP:
            case PANEL_MENU_MIDI:
            case PANEL_MENU_SYS:
            case PANEL_MENU_CLOCK:
                gui_set_status_override(0);  // give back status display
                pmstate.menu_mode = PANEL_MENU_NONE;
                pmstate.menu_timeout_count = 0;
                break;
            case PANEL_MENU_LOAD:
                if(pmstate.menu_submode == PANEL_MENU_LOAD_LOAD) {
                    seq_ctrl_load_song(pmstate.load_save_song);
                }
                else if(pmstate.menu_submode == PANEL_MENU_LOAD_CLEAR) {
                    seq_ctrl_clear_song();
                }
                pmstate.menu_timeout_count = PANEL_MENU_CONFIRM_TIMEOUT;
                panel_menu_update_display();
                break;
            case PANEL_MENU_SAVE:
                if(pmstate.menu_submode == PANEL_MENU_SAVE_SAVE) {
                    seq_ctrl_save_song(pmstate.load_save_song);
                }
                pmstate.menu_timeout_count = PANEL_MENU_CONFIRM_TIMEOUT;
                panel_menu_update_display();
                break;
            default:
                log_error("pmsm - invalid mode: %d", mode);
                return;
        }
    }
    // a different / new menu was requested - call it up
    else {
        switch(mode) {
            case PANEL_MENU_NONE:
                pmstate.menu_mode = mode;
                pmstate.num_submodes = 0;
                gui_set_status_override(0);  // give back status display
                return;
                break;
            case PANEL_MENU_SWING:
                pmstate.num_submodes = PANEL_MENU_SWING_NUM_SUBMODES;
                break;
            case PANEL_MENU_TONALITY:
                pmstate.num_submodes = PANEL_MENU_TONALITY_NUM_SUBMODES;
                break;
            case PANEL_MENU_ARP:
                pmstate.num_submodes = PANEL_MENU_ARP_NUM_SUBMODES;
                break;
            case PANEL_MENU_LOAD:
                pmstate.num_submodes = PANEL_MENU_LOAD_NUM_SUBMODES;
                pmstate.load_save_song = seq_ctrl_get_current_song();
                break;
            case PANEL_MENU_SAVE:
                pmstate.num_submodes = PANEL_MENU_SAVE_NUM_SUBMODES;
                pmstate.load_save_song = seq_ctrl_get_current_song();
                break;
            case PANEL_MENU_MIDI:
                pmstate.num_submodes = PANEL_MENU_MIDI_NUM_SUBMODES;
                break;
            case PANEL_MENU_SYS:
                pmstate.num_submodes = PANEL_MENU_SYS_NUM_SUBMODES;
                break;
            case PANEL_MENU_CLOCK:
                pmstate.num_submodes = PANEL_MENU_CLOCK_NUM_SUBMODES;
                break;
            default:
                log_error("pmsm - invalid mode: %d", mode);
                return;
        }
        gui_set_status_override(1);  // take over status display
        gui_clear_menu();
        pmstate.menu_mode = mode;
        pmstate.menu_submode = 0;
        panel_menu_update_display();
        if(pmstate.menu_mode != PANEL_MENU_NONE) {
            pmstate.menu_timeout_count = pmstate.menu_timeout;
        }
    }
}

// select the submenu mode
void panel_menu_adjust_cursor(int change, int shift) {
    pmstate.menu_submode += change;
    if(pmstate.menu_submode >= pmstate.num_submodes) {
        pmstate.menu_submode = pmstate.num_submodes - 1;
    }
    if(pmstate.menu_submode < 0) {
        pmstate.menu_submode = 0;
    }
    panel_menu_update_display();
    pmstate.menu_timeout_count = pmstate.menu_timeout;
}

// adjust the value (if edit is selected)
void panel_menu_adjust_value(int change, int shift) {
    // editing the value
    switch(pmstate.menu_mode) {
        case PANEL_MENU_NONE:
            // no action
            break;
        case PANEL_MENU_SWING:
            panel_menu_edit_swing(change);
            break;
        case PANEL_MENU_TONALITY:
            panel_menu_edit_tonality(change);
            break;
        case PANEL_MENU_ARP:
            panel_menu_edit_arp(change);
            break;
        case PANEL_MENU_LOAD:
            panel_menu_edit_load(change);
            break;
        case PANEL_MENU_SAVE:
            panel_menu_edit_save(change);
            break;
        case PANEL_MENU_MIDI:
            panel_menu_edit_midi(change);
            break;
        case PANEL_MENU_SYS:
            panel_menu_edit_sys(change);
            break;
        case PANEL_MENU_CLOCK:
            panel_menu_edit_clock(change);
            break;
        default:
            return;
    }
    pmstate.menu_timeout_count = pmstate.menu_timeout;
}

// get the panel menu timeout
int panel_menu_get_timeout(void) {
    return pmstate.menu_timeout;
}

// set the panel menu timeout
void panel_menu_set_timeout(int timeout) {
    // fix value if it's invalid
    if(timeout < PANEL_MENU_TIMEOUT_MIN || timeout > PANEL_MENU_TIMEOUT_MAX) {
        pmstate.menu_timeout = PANEL_MENU_TIMEOUT_DEFAULT;
    }
    else {
        pmstate.menu_timeout = timeout;
    }
    config_store_set_val(CONFIG_STORE_MENU_TIMEOUT, pmstate.menu_timeout);
}

//
// local functions
//
// handle state change
void panel_menu_handle_state_change(int event_type, int *data, int data_len) {
    switch(event_type) {
        case SCE_SONG_SWING:
            if(pmstate.menu_mode == PANEL_MENU_SWING &&
                    pmstate.menu_submode == PANEL_MENU_SWING_SWING) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_TONALITY:
            if(pmstate.menu_mode == PANEL_MENU_TONALITY &&
                    pmstate.menu_submode == PANEL_MENU_TONALITY_SCALE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_TRANSPOSE:
            if(pmstate.menu_mode == PANEL_MENU_TONALITY &&
                    pmstate.menu_submode == PANEL_MENU_TONALITY_TRANSPOSE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_BIAS_TRACK:
            if(pmstate.menu_mode == PANEL_MENU_TONALITY &&
                    pmstate.menu_submode == PANEL_MENU_TONALITY_BIAS_TRACK) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_TRACK_TYPE:
            if(pmstate.menu_mode == PANEL_MENU_TONALITY &&
                    pmstate.menu_submode == PANEL_MENU_TONALITY_TRACK_TYPE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_ARP_TYPE:
            if(pmstate.menu_mode == PANEL_MENU_ARP &&
                    pmstate.menu_submode == PANEL_MENU_ARP_TYPE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_ARP_SPEED:
            if(pmstate.menu_mode == PANEL_MENU_ARP &&
                    pmstate.menu_submode == PANEL_MENU_ARP_SPEED) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_ARP_GATE_TIME:
            if(pmstate.menu_mode == PANEL_MENU_ARP &&
                    pmstate.menu_submode == PANEL_MENU_ARP_GATE_TIME) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_LOADED:
            if(pmstate.menu_mode != PANEL_MENU_LOAD) {
                return;
            }
            pmstate.menu_submode = PANEL_MENU_LOAD_LOAD_CONFIRM;
            pmstate.menu_timeout_count = PANEL_MENU_CONFIRM_TIMEOUT;
            panel_menu_update_display();
            break;
        case SCE_SONG_LOAD_ERROR:
            if(pmstate.menu_mode != PANEL_MENU_LOAD) {
                return;
            }
            pmstate.menu_submode = PANEL_MENU_LOAD_LOAD_ERROR;
            pmstate.menu_timeout_count = PANEL_MENU_CONFIRM_TIMEOUT;
            panel_menu_update_display();
            break;
        case SCE_SONG_CLEARED:
            if(pmstate.menu_mode != PANEL_MENU_LOAD) {
                return;
            }
            // if we were trying to load instead of clear show the load error
            if(pmstate.menu_submode == PANEL_MENU_LOAD_LOAD ||
                    pmstate.menu_submode == PANEL_MENU_LOAD_LOAD_ERROR) {
                pmstate.menu_submode = PANEL_MENU_LOAD_LOAD_ERROR;
            }
            else {
                pmstate.menu_submode = PANEL_MENU_LOAD_CLEAR_CONFIRM;
            }
            pmstate.menu_timeout_count = PANEL_MENU_CONFIRM_TIMEOUT;
            panel_menu_update_display();
            break;
        case SCE_SONG_SAVED:
            if(pmstate.menu_mode != PANEL_MENU_SAVE) {
                return;
            }
            pmstate.menu_mode = PANEL_MENU_SAVE;
            pmstate.menu_submode = PANEL_MENU_SAVE_SAVE_CONFIRM;
            pmstate.menu_timeout_count = PANEL_MENU_CONFIRM_TIMEOUT;
            panel_menu_update_display();
            break;
        case SCE_SONG_SAVE_ERROR:
            if(pmstate.menu_mode != PANEL_MENU_SAVE) {
                return;
            }
            pmstate.menu_submode = PANEL_MENU_SAVE_SAVE_ERROR;
            pmstate.menu_timeout_count = PANEL_MENU_CONFIRM_TIMEOUT;
            panel_menu_update_display();
            break;
        case SCE_SONG_MIDI_PROGRAM:
            if(pmstate.menu_mode == PANEL_MENU_MIDI &&
                    (pmstate.menu_submode == PANEL_MENU_MIDI_PROGRAM_A ||
                    pmstate.menu_submode == PANEL_MENU_MIDI_PROGRAM_B)) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_MIDI_PORT_MAP:
            if(pmstate.menu_mode == PANEL_MENU_MIDI &&
                    (pmstate.menu_submode == PANEL_MENU_MIDI_TRACK_OUTA_PORT ||
                    pmstate.menu_submode == PANEL_MENU_MIDI_TRACK_OUTB_PORT)) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_MIDI_CHANNEL_MAP:
            if(pmstate.menu_mode == PANEL_MENU_MIDI &&
                    (pmstate.menu_submode == PANEL_MENU_MIDI_TRACK_OUTA_CHAN ||
                    pmstate.menu_submode == PANEL_MENU_MIDI_TRACK_OUTB_CHAN)) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_KEY_SPLIT:
            if(pmstate.menu_mode == PANEL_MENU_MIDI &&
                    pmstate.menu_submode == PANEL_MENU_MIDI_KEY_SPLIT) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_KEY_VELOCITY_SCALE:
            if(pmstate.menu_mode == PANEL_MENU_MIDI &&
                    pmstate.menu_submode == PANEL_MENU_MIDI_KEY_VELOCITY) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_CV_GATE_PAIRS:
            if(pmstate.menu_mode == PANEL_MENU_SYS &&
                    pmstate.menu_submode == PANEL_MENU_SYS_CVGATE_PAIRS) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_CV_BEND_RANGE:
            if(pmstate.menu_mode == PANEL_MENU_SYS &&
                    pmstate.menu_submode == PANEL_MENU_SYS_CV_BEND_RANGE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_CVCAL:
            if(pmstate.menu_mode == PANEL_MENU_SYS &&
                    (pmstate.menu_submode == PANEL_MENU_SYS_CVCAL1 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVCAL2 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVCAL3 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVCAL4)) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_CVOFFSET:
            if(pmstate.menu_mode == PANEL_MENU_SYS &&
                    (pmstate.menu_submode == PANEL_MENU_SYS_CVOFFSET1 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVOFFSET2 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVOFFSET3 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVOFFSET4)) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_CVGATEDELAY:
            if(pmstate.menu_mode == PANEL_MENU_SYS &&
                    (pmstate.menu_submode == PANEL_MENU_SYS_CVGATEDELAY1 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVGATEDELAY2 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVGATEDELAY3 ||
                    pmstate.menu_submode == PANEL_MENU_SYS_CVGATEDELAY4)) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_STEP_LEN:
            if(pmstate.menu_mode == PANEL_MENU_CLOCK &&
                    pmstate.menu_submode == PANEL_MENU_CLOCK_STEP_LEN) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_METRONOME_MODE:
            if(pmstate.menu_mode == PANEL_MENU_CLOCK &&
                    pmstate.menu_submode == PANEL_MENU_CLOCK_METRONOME_MODE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_MIDI_PORT_CLOCK_OUT:
            if(pmstate.menu_mode == PANEL_MENU_CLOCK &&
                    (pmstate.menu_submode == PANEL_MENU_CLOCK_TX_DIN1 ||
                    pmstate.menu_submode == PANEL_MENU_CLOCK_TX_DIN2 ||
                    pmstate.menu_submode == PANEL_MENU_CLOCK_TX_USB_DEV ||
                    pmstate.menu_submode == PANEL_MENU_CLOCK_TX_USB_HOST ||
                    pmstate.menu_submode == PANEL_MENU_CLOCK_TX_CV)) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_MIDI_CLOCK_SOURCE:
            if(pmstate.menu_mode == PANEL_MENU_CLOCK &&
                    pmstate.menu_submode == PANEL_MENU_CLOCK_SOURCE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_SCENE_SYNC:
            if(pmstate.menu_mode == PANEL_MENU_CLOCK &&
                    pmstate.menu_submode == PANEL_MENU_CLOCK_SCENE_SYNC) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_MAGIC_RANGE:
            if(pmstate.menu_mode == PANEL_MENU_TONALITY &&
                    pmstate.menu_submode == PANEL_MENU_TONALITY_MAGIC_RANGE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_SONG_MAGIC_CHANCE:
            if(pmstate.menu_mode == PANEL_MENU_TONALITY &&
                    pmstate.menu_submode == PANEL_MENU_TONALITY_MAGIC_CHANCE) {
                panel_menu_update_display();
                pmstate.menu_timeout_count = pmstate.menu_timeout;
            }
            break;
        case SCE_CTRL_FIRST_TRACK:
        case SCE_ENG_CURRENT_SCENE:
            panel_menu_track_select_changed();
            break;
        case SCE_CONFIG_LOADED:
            panel_menu_set_timeout(config_store_get_val(CONFIG_STORE_MENU_TIMEOUT));
            break;
        case SCE_CONFIG_CLEARED:
            panel_menu_set_timeout(PANEL_MENU_TIMEOUT_DEFAULT);
            break;
        default:
            break;
    }
}

// the track selector changed - we might need to change menu context
void panel_menu_track_select_changed(void) {
    switch(pmstate.menu_mode) {
        case PANEL_MENU_TONALITY:
        case PANEL_MENU_ARP:
        case PANEL_MENU_MIDI:
        case PANEL_MENU_SYS:
        case PANEL_MENU_CLOCK:
        case PANEL_MENU_SWING:
            panel_menu_update_display();
            pmstate.menu_timeout_count = pmstate.menu_timeout;
            break;
        case PANEL_MENU_LOAD:
        case PANEL_MENU_SAVE:
        case PANEL_MENU_NONE:
        default:
            return;
    }
}

// update previous / next
void panel_menu_update_prev_next(void) {
    int prev = 0;
    int next = 0;
    if(pmstate.menu_submode) {
        prev = 1;
    }
    if(pmstate.menu_submode < (pmstate.num_submodes - 1)) {
        next = 1;
    }
    gui_set_menu_prev_next(prev, next);
}

//
// menu displays
//
// update the menu display
void panel_menu_update_display(void) {
    // make the display ready since it might have been used
    // by another module for displaying non-menu text
    switch(pmstate.menu_mode) {
        case PANEL_MENU_NONE:
            return;
        case PANEL_MENU_SWING:
            panel_menu_display_swing();
            break;
        case PANEL_MENU_TONALITY:
            panel_menu_display_tonality();
            break;
        case PANEL_MENU_ARP:
            panel_menu_display_arp();
            break;
        case PANEL_MENU_LOAD:
            panel_menu_display_load();
            break;
        case PANEL_MENU_SAVE:
            panel_menu_display_save();
            break;
        case PANEL_MENU_MIDI:
            panel_menu_display_midi();
            break;
        case PANEL_MENU_SYS:
            panel_menu_display_sys();
            break;
        case PANEL_MENU_CLOCK:
            panel_menu_display_clock();
            break;
        default:
            log_error("pmud - invalid mode: %d", pmstate.menu_mode);
            return;
    }
    panel_menu_update_prev_next();
}

// display the swing menu
void panel_menu_display_swing(void) {
    char tempstr[64];
    gui_set_menu_title("SWING");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_SWING_SWING:
            gui_set_menu_param("Swing");
            sprintf(tempstr, "%d%%", song_get_swing());
            gui_set_menu_value(tempstr);
            break;
    }
}

// display the tonality menu
void panel_menu_display_tonality(void) {
    char tempstr[64];
    int temp;
    gui_set_menu_title("TONALITY");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_TONALITY_SCALE:
            sprintf(tempstr, "Scene %d Track %d",
                (seq_ctrl_get_scene() + 1),
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Scale");
            scale_type_to_name(tempstr,
                song_get_tonality(seq_ctrl_get_scene(),
                seq_ctrl_get_first_track()));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_TONALITY_TRANSPOSE:
            sprintf(tempstr, "Scene %d Track %d",
                (seq_ctrl_get_scene() + 1),
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Transpose");
            // drum mode
            if(song_get_track_type(seq_ctrl_get_first_track()) ==
                    SONG_TRACK_TYPE_DRUM) {
                gui_set_menu_value("-- (DRUM)");
            }
            // voice mode
            else {
                panel_utils_transpose_to_str(tempstr,
                    song_get_transpose(seq_ctrl_get_scene(),
                    seq_ctrl_get_first_track()));
                gui_set_menu_value(tempstr);
            }
            break;
        case PANEL_MENU_TONALITY_BIAS_TRACK:
            sprintf(tempstr, "Scene %d Track %d",
                (seq_ctrl_get_scene() + 1),
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Bias Track");
            temp = song_get_bias_track(seq_ctrl_get_scene(),
                seq_ctrl_get_first_track());
            if(temp == SONG_TRACK_BIAS_NULL) {
                gui_set_menu_value("DISABLED");
            }
            else {
                sprintf(tempstr, "%d", (temp + 1));
                gui_set_menu_value(tempstr);
            }
            break;
        case PANEL_MENU_TONALITY_TRACK_TYPE:
            sprintf(tempstr, "Track %d",
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Track Type");
            if(song_get_track_type(seq_ctrl_get_first_track()) ==
                    SONG_TRACK_TYPE_DRUM) {
                gui_set_menu_value("DRUM");
            }
            else {
                gui_set_menu_value("VOICE");
            }
            break;
        case PANEL_MENU_TONALITY_MAGIC_RANGE:
            gui_set_menu_subtitle("Magic Range");
            gui_set_menu_param("Range");
            sprintf(tempstr, "%2d", song_get_magic_range());
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_TONALITY_MAGIC_CHANCE:
            gui_set_menu_subtitle("Magic Chance");
            gui_set_menu_param("Chance");
            sprintf(tempstr, "%3d%%", song_get_magic_chance());
            gui_set_menu_value(tempstr);
            break;
    }
}

// display the arp menu
void panel_menu_display_arp(void) {
    char tempstr[64];
    gui_set_menu_title("ARP");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_ARP_TYPE:
            sprintf(tempstr, "Scene %d Track %d",
                (seq_ctrl_get_scene() + 1),
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Arp Type");
            arp_type_to_name(tempstr,
                song_get_arp_type(seq_ctrl_get_scene(),
                seq_ctrl_get_first_track()));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_ARP_SPEED:
            sprintf(tempstr, "Scene %d Track %d",
                (seq_ctrl_get_scene() + 1),
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Arp Speed");
            panel_utils_step_len_to_str(tempstr,
                song_get_arp_speed(seq_ctrl_get_scene(),
                seq_ctrl_get_first_track()));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_ARP_GATE_TIME:
            sprintf(tempstr, "Scene %d Track %d",
                (seq_ctrl_get_scene() + 1),
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Arp Gate Len");
            sprintf(tempstr, "%d",
                 song_get_arp_gate_time(seq_ctrl_get_scene(),
                 seq_ctrl_get_first_track()));
            gui_set_menu_value(tempstr);
            break;
    }
}

// display the load menu
void panel_menu_display_load(void) {
    char tempstr[64];
    gui_set_menu_title("LOAD");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_LOAD_LOAD:
            gui_set_menu_subtitle("Load Song");
            gui_set_menu_param("Song");
            sprintf(tempstr, "%d", (pmstate.load_save_song + 1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_LOAD_CLEAR:
            gui_set_menu_subtitle("Clear Current Song");
            gui_set_menu_param("");
            gui_set_menu_value("");
            break;
        case PANEL_MENU_LOAD_LOAD_CONFIRM:
            gui_set_menu_subtitle("Load Song");
            gui_set_menu_param("Song");
            sprintf(tempstr, "%d Loaded", (pmstate.load_save_song + 1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_LOAD_LOAD_ERROR:
            gui_set_menu_subtitle("Load Song");
            gui_set_menu_param("Song");
            sprintf(tempstr, "%d Load Error", (pmstate.load_save_song + 1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_LOAD_CLEAR_CONFIRM:
            gui_set_menu_subtitle("Clear Current Song");
            gui_set_menu_param("Song");
            gui_set_menu_value("Cleared");
            break;
    }
}

// display the save menu
void panel_menu_display_save(void) {
    char tempstr[64];
    gui_set_menu_title("SAVE");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_SAVE_SAVE:
            gui_set_menu_subtitle("Save Song");
            gui_set_menu_param("Song");
            sprintf(tempstr, "%d", (pmstate.load_save_song + 1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_SAVE_SAVE_CONFIRM:
            gui_set_menu_subtitle("Save Song");
            gui_set_menu_param("Song");
            sprintf(tempstr, "%d Saved", (pmstate.load_save_song + 1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_SAVE_SAVE_ERROR:
            gui_set_menu_subtitle("Save Song");
            gui_set_menu_param("Song");
            sprintf(tempstr, "%d Save Error", (pmstate.load_save_song + 1));
            gui_set_menu_value(tempstr);
            break;
    }
}

// display the MIDI menu
void panel_menu_display_midi(void) {
    char tempstr[64];
    int track = seq_ctrl_get_first_track();
    int temp;
    gui_set_menu_title("MIDI");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_MIDI_PROGRAM_A:
            sprintf(tempstr, "Track %d", (track + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Program A");
            // CV/gate doesn't have programs
            if(song_get_midi_port_map(track, 0) == MIDI_PORT_CV_OUT) {
                panel_utils_get_blank_str(tempstr);
            }
            // MIDI output
            else {
                temp = song_get_midi_program(track, 0);
                if(temp < 0) {
                    panel_utils_get_blank_str(tempstr);
                }
                else {
                    sprintf(tempstr, "%d", temp + 1);
                }
            }
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_PROGRAM_B:
            sprintf(tempstr, "Track %d", (track + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Program B");
            // CV/gate doesn't have programs
            if(song_get_midi_port_map(track, 1) == MIDI_PORT_CV_OUT) {
                panel_utils_get_blank_str(tempstr);
            }
            // MIDI output
            else {
                temp = song_get_midi_program(track, 1);
                if(temp < 0) {
                    panel_utils_get_blank_str(tempstr);
                }
                else {
                    sprintf(tempstr, "%d", temp + 1);
                }
            }
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTA_PORT:
            sprintf(tempstr, "Track %d", (track + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Out Port A");
            panel_utils_port_str(tempstr, song_get_midi_port_map(track, 0));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTB_PORT:
            sprintf(tempstr, "Track %d", (track + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Out Port B");
            panel_utils_port_str(tempstr, song_get_midi_port_map(track, 1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTA_CHAN:
            sprintf(tempstr, "Track %d", (track + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Out Chan A");
            panel_utils_channel_str(tempstr,
                song_get_midi_port_map(track, 0),
                song_get_midi_channel_map(track, 0));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTB_CHAN:
            sprintf(tempstr, "Track %d", (track + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Out Chan B");
            panel_utils_channel_str(tempstr,
                song_get_midi_port_map(track, 1),
                song_get_midi_channel_map(track, 1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_KEY_SPLIT:
            sprintf(tempstr, "Track %d", (track + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Key Split");
            panel_utils_key_split_str(tempstr, song_get_key_split(track));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_KEY_VELOCITY:
            gui_set_menu_param("Key Vel Scale");
            gui_set_menu_subtitle("");
            sprintf(tempstr, "%d", song_get_key_velocity_scale());
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_REMOTE_CTRL:
            gui_set_menu_param("MIDI Rmt Ctrl");
            gui_set_menu_subtitle("");
            panel_utils_onoff_str(tempstr, song_get_midi_remote_ctrl());
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_MIDI_AUTOLIVE:
            gui_set_menu_param("MIDI Autolive");
            gui_set_menu_subtitle("");
            panel_utils_onoff_str(tempstr, song_get_midi_autolive());
            gui_set_menu_value(tempstr);
            break;
    }
}

// display the system menu
void panel_menu_display_sys(void) {
    char tempstr[64];
    int temp;
    gui_set_menu_title("SYSTEM");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_SYS_VERSION:
            gui_set_menu_subtitle("Firmware Release");
            gui_set_menu_param("Ver:");
            sprintf(tempstr, "%d.%02d LCD: %c",
                CARBON_VERSION_MAJOR,
                CARBON_VERSION_MINOR,
                (gui_get_screen_type() + 65));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_SYS_CVGATE_PAIRS:
            gui_set_menu_subtitle("");
            gui_set_menu_param("CV/Gate Pairs");
            switch(song_get_cvgate_pairs()) {
                case SONG_CVGATE_PAIR_ABCD:
                    gui_set_menu_value("ABCD");
                    break;
                case SONG_CVGATE_PAIR_AABC:
                    gui_set_menu_value("AABC");
                    break;
                case SONG_CVGATE_PAIR_AABB:
                    gui_set_menu_value("AABB");
                    break;
                case SONG_CVGATE_PAIR_AAAA:
                    gui_set_menu_value("AAAA");
                    break;
                default:
                    gui_set_menu_value(" ");
                    break;
            }
            break;
        case PANEL_MENU_SYS_CV_BEND_RANGE:
            gui_set_menu_subtitle("");
            gui_set_menu_param("CV Bend Range");
            sprintf(tempstr, "%d", song_get_cv_bend_range());
            gui_set_menu_value(tempstr);
            break;
            break;
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE1:
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE2:
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE3:
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVGATE_OUTPUT_MODE1;
            gui_set_menu_subtitle("CV Output Mode");
            panel_utils_cvgate_pair_to_str(tempstr, temp);
            gui_set_menu_param(tempstr);
            panel_utils_cvgate_pair_mode_to_str(tempstr,
                song_get_cvgate_pair_mode(temp));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_SYS_CV_SCALING1:
        case PANEL_MENU_SYS_CV_SCALING2:
        case PANEL_MENU_SYS_CV_SCALING3:
        case PANEL_MENU_SYS_CV_SCALING4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CV_SCALING1;
            gui_set_menu_subtitle("CV Output Scaling");
            sprintf(tempstr, "CV Output %d", (temp + 1));
            gui_set_menu_param(tempstr);
            panel_utills_cv_output_scaling_to_str(tempstr,
                song_get_cv_output_scaling(temp));
            gui_set_menu_value(tempstr);

            break;
        case PANEL_MENU_SYS_CVCAL1:
        case PANEL_MENU_SYS_CVCAL2:
        case PANEL_MENU_SYS_CVCAL3:
        case PANEL_MENU_SYS_CVCAL4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVCAL1;
            gui_set_menu_subtitle("CV Span Calibrate");
            sprintf(tempstr, "CV Span %d", (temp + 1));
            gui_set_menu_param(tempstr);
            sprintf(tempstr, "%d", song_get_cvcal(temp));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_SYS_CVOFFSET1:
        case PANEL_MENU_SYS_CVOFFSET2:
        case PANEL_MENU_SYS_CVOFFSET3:
        case PANEL_MENU_SYS_CVOFFSET4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVOFFSET1;
            gui_set_menu_subtitle("CV Output Offset");
            sprintf(tempstr, "CV Offset %d", (temp + 1));
            gui_set_menu_param(tempstr);
            sprintf(tempstr, "%d", song_get_cvoffset(temp));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_SYS_CVGATEDELAY1:
        case PANEL_MENU_SYS_CVGATEDELAY2:
        case PANEL_MENU_SYS_CVGATEDELAY3:
        case PANEL_MENU_SYS_CVGATEDELAY4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVGATEDELAY1;
            gui_set_menu_subtitle("CV Gate Delay");
            sprintf(tempstr, "Gate Delay %d", (temp + 1));
            gui_set_menu_param(tempstr);
            sprintf(tempstr, "%d", song_get_cvgatedelay(temp));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_SYS_MENU_TIMEOUT:
            gui_set_menu_subtitle("Menu Timeout");
            gui_set_menu_param("Timeout");
            if(pmstate.menu_timeout == PANEL_MENU_TIMEOUT_MAX) {
                sprintf(tempstr, "SHIFT EXIT");
            }
            else {
                sprintf(tempstr, "%ds", (pmstate.menu_timeout / 1000));
            }
            gui_set_menu_value(tempstr);
            break;
    }
}

// display the clock menu
void panel_menu_display_clock(void) {
    char tempstr[64];
    char tempstr2[64];
    int temp;
    gui_set_menu_title("CLOCK");
    switch(pmstate.menu_submode) {
        case PANEL_MENU_CLOCK_STEP_LEN:
            sprintf(tempstr, "Scene %d Track %d",
                (seq_ctrl_get_scene() + 1),
                (seq_ctrl_get_first_track() + 1));
            gui_set_menu_subtitle(tempstr);
            gui_set_menu_param("Step Length");
            panel_utils_step_len_to_str(tempstr,
                song_get_step_length(seq_ctrl_get_scene(),
                seq_ctrl_get_first_track()));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_METRONOME_MODE:
            gui_set_menu_subtitle("");
            gui_set_menu_param("Metronome");
            temp = song_get_metronome_mode();
            switch(temp) {
                case SONG_METRONOME_OFF:
                    panel_utils_onoff_str(tempstr, temp);
                    break;
                case SONG_METRONOME_INTERNAL:
                    sprintf(tempstr, "INTERNAL");
                    break;
                case SONG_METRONOME_CV_RESET:
                    sprintf(tempstr, "CV Reset");
                    break;
                default:
                    panel_utils_note_to_name(tempstr2, temp, 1, 0);
                    sprintf(tempstr, "Track 6 %s", tempstr2);
                    break;
            }
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_METRONOME_SOUND_LEN:
            gui_set_menu_subtitle("");
            gui_set_menu_param("Metronome Len");
            sprintf(tempstr, "%dms", song_get_metronome_sound_len());
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_TX_DIN1:
            gui_set_menu_subtitle("MIDI Clock OUT");
            gui_set_menu_param("MIDI DIN 1");
            panel_utils_clock_ppq_to_str(tempstr,
                song_get_midi_port_clock_out(MIDI_PORT_DIN1_OUT));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_TX_DIN2:
            gui_set_menu_subtitle("MIDI Clock OUT");
            gui_set_menu_param("MIDI DIN 2");
            panel_utils_clock_ppq_to_str(tempstr,
                song_get_midi_port_clock_out(MIDI_PORT_DIN2_OUT));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_TX_CV:
            gui_set_menu_subtitle("MIDI Clock OUT");
            gui_set_menu_param("CV/GATE");
            panel_utils_clock_ppq_to_str(tempstr,
                song_get_midi_port_clock_out(MIDI_PORT_CV_OUT));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_TX_USB_HOST:
            gui_set_menu_subtitle("MIDI Clock OUT");
            gui_set_menu_param("MIDI USB HOST");
            panel_utils_clock_ppq_to_str(tempstr,
                song_get_midi_port_clock_out(MIDI_PORT_USB_HOST_OUT));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_TX_USB_DEV:
            gui_set_menu_subtitle("MIDI Clock OUT");
            gui_set_menu_param("MIDI USB DEV");
            panel_utils_clock_ppq_to_str(tempstr,
                song_get_midi_port_clock_out(MIDI_PORT_USB_DEV_OUT1));
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_SOURCE:
            gui_set_menu_subtitle("MIDI Clock Source");
            gui_set_menu_param("Source");
            panel_utils_clock_source_str(tempstr,
                song_get_midi_clock_source());
            gui_set_menu_value(tempstr);
            break;
        case PANEL_MENU_CLOCK_SCENE_SYNC:
            gui_set_menu_subtitle("Scene Change Sync");
            gui_set_menu_param("Mode");
            switch(song_get_scene_sync()) {
                case SONG_SCENE_SYNC_BEAT:
                    gui_set_menu_value("Beat");
                    break;
                case SONG_SCENE_SYNC_TRACK1:
                    gui_set_menu_value("Track 1 End");
                    break;
            }
            break;
    }
}

//
// menu editors
//
// edit a value in the song menu
void panel_menu_edit_song(int change) {
    switch(pmstate.menu_submode) {
        default:
            return;
    }
    panel_menu_update_display();
}

// edit a value in the swing menu
void panel_menu_edit_swing(int change) {
    switch(pmstate.menu_submode) {
        case PANEL_MENU_SWING_SWING:
            seq_ctrl_adjust_swing(change);
            break;
    }
    panel_menu_update_display();
}

// edit a value in the tonality menu
void panel_menu_edit_tonality(int change) {
    switch(pmstate.menu_submode) {
        case PANEL_MENU_TONALITY_SCALE:
            seq_ctrl_adjust_tonality(change);
            break;
        case PANEL_MENU_TONALITY_TRANSPOSE:
            seq_ctrl_adjust_transpose(change);
            break;
        case PANEL_MENU_TONALITY_BIAS_TRACK:
            seq_ctrl_adjust_bias_track(change);
            break;
        case PANEL_MENU_TONALITY_TRACK_TYPE:
            seq_ctrl_adjust_track_type(change);
            break;
        case PANEL_MENU_TONALITY_MAGIC_RANGE:
            seq_ctrl_adjust_magic_range(change);
            break;
        case PANEL_MENU_TONALITY_MAGIC_CHANCE:
            seq_ctrl_adjust_magic_chance(change);
            break;
    }
    panel_menu_update_display();
}

// edit a value in the arp menu
void panel_menu_edit_arp(int change) {
    switch(pmstate.menu_submode) {
        case PANEL_MENU_ARP_TYPE:
            seq_ctrl_adjust_arp_type(change);
            break;
        case PANEL_MENU_ARP_SPEED:
            seq_ctrl_adjust_arp_speed(change);
            break;
        case PANEL_MENU_ARP_GATE_TIME:
            seq_ctrl_adjust_arp_gate_time(change);
            break;
    }
    panel_menu_update_display();
}

// edit a value in the load menu
void panel_menu_edit_load(int change) {
    switch(pmstate.menu_submode) {
        case PANEL_MENU_LOAD_LOAD:
            pmstate.load_save_song =
                seq_utils_clamp(pmstate.load_save_song + change,
                0, (SEQ_NUM_SONGS - 1));
            break;
        case PANEL_MENU_LOAD_CLEAR:
            break;
    }
    panel_menu_update_display();
}

// edit a value in the save menu
void panel_menu_edit_save(int change) {
    switch(pmstate.menu_submode) {
        case PANEL_MENU_SAVE_SAVE:
            pmstate.load_save_song =
                seq_utils_clamp(pmstate.load_save_song + change,
                0, (SEQ_NUM_SONGS - 1));
            break;
    }
    panel_menu_update_display();
}

// edit a value in the MIDI menu
void panel_menu_edit_midi(int change) {
    switch(pmstate.menu_submode) {
        case PANEL_MENU_MIDI_PROGRAM_A:
            seq_ctrl_adjust_midi_program(0, change);
            break;
        case PANEL_MENU_MIDI_PROGRAM_B:
            seq_ctrl_adjust_midi_program(1, change);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTA_CHAN:
            seq_ctrl_adjust_midi_channel(0, change);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTB_CHAN:
            seq_ctrl_adjust_midi_channel(1, change);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTA_PORT:
            seq_ctrl_adjust_midi_port(0, change);
            break;
        case PANEL_MENU_MIDI_TRACK_OUTB_PORT:
            seq_ctrl_adjust_midi_port(1, change);
            break;
        case PANEL_MENU_MIDI_KEY_SPLIT:
            seq_ctrl_adjust_key_split(change);
            break;
        case PANEL_MENU_MIDI_KEY_VELOCITY:
            seq_ctrl_adjust_key_velocity_scale(change);
            break;
        case PANEL_MENU_MIDI_REMOTE_CTRL:
            seq_ctrl_adjust_midi_remote_ctrl(change);
            break;
        case PANEL_MENU_MIDI_AUTOLIVE:
            seq_ctrl_adjust_midi_autolive(change);
            break;
    }
    panel_menu_update_display();
}

// edit a value in the system menu
void panel_menu_edit_sys(int change) {
    int temp;
    switch(pmstate.menu_submode) {
        case PANEL_MENU_SYS_VERSION:
            break;
        case PANEL_MENU_SYS_CV_BEND_RANGE:
            seq_ctrl_adjust_cv_bend_range(change);
            break;
        case PANEL_MENU_SYS_CVGATE_PAIRS:
            seq_ctrl_adjust_cvgate_pairs(change);
            break;
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE1:
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE2:
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE3:
        case PANEL_MENU_SYS_CVGATE_OUTPUT_MODE4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVGATE_OUTPUT_MODE1;
            seq_ctrl_adjust_cvgate_pair_mode(temp, change);
            break;
        case PANEL_MENU_SYS_CV_SCALING1:
        case PANEL_MENU_SYS_CV_SCALING2:
        case PANEL_MENU_SYS_CV_SCALING3:
        case PANEL_MENU_SYS_CV_SCALING4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CV_SCALING1;
            seq_ctrl_adjust_cv_output_scaling(temp, change);
            break;
        case PANEL_MENU_SYS_CVCAL1:
        case PANEL_MENU_SYS_CVCAL2:
        case PANEL_MENU_SYS_CVCAL3:
        case PANEL_MENU_SYS_CVCAL4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVCAL1;
            seq_ctrl_adjust_cvcal(temp, change);
            break;
        case PANEL_MENU_SYS_CVOFFSET1:
        case PANEL_MENU_SYS_CVOFFSET2:
        case PANEL_MENU_SYS_CVOFFSET3:
        case PANEL_MENU_SYS_CVOFFSET4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVOFFSET1;
            seq_ctrl_adjust_cvoffset(temp, change);
            break;
        case PANEL_MENU_SYS_CVGATEDELAY1:
        case PANEL_MENU_SYS_CVGATEDELAY2:
        case PANEL_MENU_SYS_CVGATEDELAY3:
        case PANEL_MENU_SYS_CVGATEDELAY4:
            temp = pmstate.menu_submode - PANEL_MENU_SYS_CVGATEDELAY1;
            seq_ctrl_adjust_cvgatedelay(temp, change);
            break;
        case PANEL_MENU_SYS_MENU_TIMEOUT:
            panel_menu_set_timeout(seq_utils_clamp(pmstate.menu_timeout +
                (change * 1000),
                PANEL_MENU_TIMEOUT_MIN, PANEL_MENU_TIMEOUT_MAX));
            break;
    }
    panel_menu_update_display();
}

// edit a value in the clock menu
void panel_menu_edit_clock(int change) {
    switch(pmstate.menu_submode) {
        case PANEL_MENU_CLOCK_STEP_LEN:
            seq_ctrl_adjust_step_length(change);
            break;
        case PANEL_MENU_CLOCK_METRONOME_MODE:
            seq_ctrl_adjust_metronome_mode(change);
            break;
        case PANEL_MENU_CLOCK_METRONOME_SOUND_LEN:
            seq_ctrl_adjust_metronome_sound_len(change);
            break;
        case PANEL_MENU_CLOCK_TX_DIN1:
            seq_ctrl_adjust_clock_out_rate(MIDI_PORT_DIN1_OUT, change);
            break;
        case PANEL_MENU_CLOCK_TX_DIN2:
            seq_ctrl_adjust_clock_out_rate(MIDI_PORT_DIN2_OUT, change);
            break;
        case PANEL_MENU_CLOCK_TX_USB_DEV:
            seq_ctrl_adjust_clock_out_rate(MIDI_PORT_USB_DEV_OUT1, change);
            break;
        case PANEL_MENU_CLOCK_TX_USB_HOST:
            seq_ctrl_adjust_clock_out_rate(MIDI_PORT_USB_HOST_OUT, change);
            break;
        case PANEL_MENU_CLOCK_TX_CV:
            seq_ctrl_adjust_clock_out_rate(MIDI_PORT_CV_OUT, change);
            break;
        case PANEL_MENU_CLOCK_SOURCE:
            seq_ctrl_adjust_clock_source(change);
            break;
        case PANEL_MENU_CLOCK_SCENE_SYNC:
            seq_ctrl_adjust_scene_sync(change);
            break;
    }
    panel_menu_update_display();
}
