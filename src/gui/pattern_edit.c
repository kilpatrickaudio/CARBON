/*
 * CARBON Pattern Editor
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2018: Kilpatrick Audio
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
#include "pattern_edit.h"
#include "gui.h"
#include "panel_menu.h"
#include "../config.h"
#include "../gfx.h"
#include "../seq/pattern.h"
#include "../seq/seq_ctrl.h"
#include "../seq/song.h"
#include "../util/log.h"
#include "../util/panel_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"
#include <stdio.h>
#include <string.h>

struct pattern_edit_state {
    int enable;  // 0 = disabled, 1 = enabled
    int edit_timeout;  // timeout counter for editing
    int step_pos;  // current step pos for editing
};
struct pattern_edit_state pedits;

// local functions
void pattern_edit_refresh(void);

// init the step edit mode
void pattern_edit_init(void) {
    pedits.enable = 0;
    pedits.edit_timeout = 0;
    pedits.step_pos = 0;
    // register events
    state_change_register(pattern_edit_handle_state_change, SCEC_CTRL);
    state_change_register(pattern_edit_handle_state_change, SCEC_SONG);
}

// run step edit timer task
void pattern_edit_timer_task(void) {
    // time out step edit mode
    if(pedits.edit_timeout) {
        pedits.edit_timeout --;
        if(pedits.edit_timeout == 0) {
            pattern_edit_set_enable(0);
        }
    }
}

// handle state change
void pattern_edit_handle_state_change(int event_type, int *data, int data_len) {
    switch(event_type) {
        case SCE_ENG_CURRENT_SCENE:
        case SCE_SONG_PATTERN_TYPE:
            if(pedits.enable) {
                pattern_edit_refresh();
            }
            break;
    }
}

// get whether step edit mode is enabled
int pattern_edit_get_enable(void) {
    return pedits.enable;
}

// start and stop step edit mode
void pattern_edit_set_enable(int enable) {
    // start edit
    if(enable) {
        pedits.enable = 1;
        pedits.edit_timeout = panel_menu_get_timeout();
        panel_menu_set_mode(PANEL_MENU_NONE);  // disable any menu mode
        gui_grid_clear_overlay();
        gui_grid_set_overlay_enable(1);
        gui_clear_status_text_all();
        gui_set_status_override(1);  // take over status display
        pattern_edit_adjust_cursor(0, 0);  // causes the step displayed
        pattern_edit_refresh();
    }
    // stop edit - only if we're actually enabled
    else if(pedits.enable) {
        pedits.enable = 0;
        pedits.edit_timeout = 0;
        gui_grid_set_overlay_enable(0);
        gui_clear_status_text_all();  // XXX get rid of this
        gui_set_status_override(0);  // give back status display
    }
}

// adjust the cursor position
void pattern_edit_adjust_cursor(int change, int shift) {
    int val;
    pedits.edit_timeout = panel_menu_get_timeout();

    val = pedits.step_pos + change;
    if(val < 0) {
        val = SEQ_NUM_STEPS - 1;
    }
    else if(val >= SEQ_NUM_STEPS) {
        val = 0;
    }
    // disable old step
    gui_grid_set_overlay_color(pedits.step_pos, GUI_OVERLAY_BLANK);
    // enable new step
    pedits.step_pos = val;
    pattern_edit_refresh();
    gui_grid_set_overlay_color(pedits.step_pos, GUI_OVERLAY_HIGH);
}

// adjust the step
void pattern_edit_adjust_step(int change, int shift) {
    int scene, track, pattern;
    pedits.edit_timeout = panel_menu_get_timeout();
    scene = seq_ctrl_get_scene();
    track = seq_ctrl_get_first_track();
    pattern = song_get_pattern_type(scene, track);
    if(pattern == PATTERN_AS_RECORDED) {
        return;
    }
    if(change > 0) {
        pattern_set_step_enable(pattern, pedits.step_pos, 1);
    }
    else if(change < 0) {
        pattern_set_step_enable(pattern, pedits.step_pos, 0);
    }
    pattern_edit_refresh();
}

// restore the current pattern from ROM
void pattern_edit_restore_pattern(void) {
    int scene, track, pattern;
    scene = seq_ctrl_get_scene();
    track = seq_ctrl_get_first_track();
    pattern = song_get_pattern_type(scene, track);
    pattern_restore_pattern(pattern);
}

// local functions
void pattern_edit_refresh(void) {
    char tempstr[GFX_LABEL_LEN];
    int scene, track, pattern;

    scene = seq_ctrl_get_scene();
    track = seq_ctrl_get_first_track();
    pattern = song_get_pattern_type(scene, track);

    gui_clear_status_text_all();
    sprintf(tempstr, "Pattern Edit - Pattern: %2d", (pattern + 1));
    gui_set_status_text(0, tempstr);
    if(pattern == PATTERN_AS_RECORDED) {
        sprintf(tempstr, "Pattern Locked");
    }
    else if(pattern_get_step_enable(scene, track, pattern, pedits.step_pos)) {
        sprintf(tempstr, "Step: %2d - On", (pedits.step_pos + 1));
    }
    else {
        sprintf(tempstr, "Step: %2d - Off", (pedits.step_pos + 1));
    }
    gui_set_status_text(2, tempstr);
}
