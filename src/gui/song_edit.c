/*
 * CARBON Song Editor
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
#include "song_edit.h"
#include "gui.h"
#include "panel_menu.h"
#include "../gfx.h"
#include "../config.h"
#include "../seq/song.h"
#include "../util/log.h"
#include "../util/panel_utils.h"
#include "../util/seq_utils.h"
#include <stdio.h>
#include <string.h>

// settings
#define SONG_EDIT_DISPLAY_SLOTS 3

struct song_edit_state {
    int enable;  // 0 = disabled, 1 = enabled
    int edit_timeout;  // timeout counter for editing
    int edit_pos;  // which entry we are editing
    int display_pos;  // the first position in the displayed list
};
struct song_edit_state sngedits;

// local functions
void song_edit_update_display(void);

// init the song edit mode
void song_edit_init(void) {
    sngedits.enable = 0;
    sngedits.edit_timeout = 0;
    sngedits.edit_pos = 0;
    sngedits.display_pos = 0;
}

// run the song edit timer task
void song_edit_timer_task(void) {
    // time out song edit mode
    if(sngedits.edit_timeout) {
        sngedits.edit_timeout --;
        if(sngedits.edit_timeout == 0) {
            song_edit_set_enable(0);
        }
    }
}

// get whether song edit mode is enabled
int song_edit_get_enable(void) {
    return sngedits.enable;
}

// start and stop song edit mode
void song_edit_set_enable(int enable) {
    // start edit
    if(enable) {
        sngedits.enable = 1;
        sngedits.edit_timeout = panel_menu_get_timeout();
        panel_menu_set_mode(PANEL_MENU_NONE);  // disable any menu mode
        gui_grid_clear_overlay();
        gui_grid_set_overlay_enable(1);
        gui_clear_status_text_all();
        gui_set_status_override(1);  // take over status display
        song_edit_adjust_cursor(0, 0);  // causes the song list to be drawn
    }
    // stop edit - only if we're actually enabled
    else if(sngedits.enable) {
        sngedits.enable = 0;
        sngedits.edit_timeout = 0;
        gui_grid_set_overlay_enable(0);
        gui_clear_status_text_all();  // XXX get rid of this
        gui_set_status_override(0);  // give back status display
    }
}

// adjust the cursor position
void song_edit_adjust_cursor(int change, int shift) {
    // insert an entry before the current one and push everything down
    if(shift) {
        song_add_song_list_entry(sngedits.edit_pos);
    }
    // adjust the cursor position
    else {
        sngedits.edit_pos = seq_utils_clamp(sngedits.edit_pos + change,
            0, (SEQ_SONG_LIST_ENTRIES - 1));
    }
    song_edit_update_display();
}

// adjust the scene
void song_edit_adjust_scene(int change, int shift) {
    int scene = song_get_song_list_scene(sngedits.edit_pos);
    // current slot is empty
    if(scene == SONG_LIST_SCENE_NULL) {
        song_set_song_list_scene(sngedits.edit_pos, 0);  // scene 1
    }
    // edit the existing scene
    else {
        song_set_song_list_scene(sngedits.edit_pos,
            seq_utils_clamp(scene + change, 0, SEQ_NUM_SCENES));
    }
    song_edit_update_display();
}

// adjust the length
void song_edit_adjust_length(int change, int shift) {
    // current slot is empty
    if(song_get_song_list_scene(sngedits.edit_pos) == SONG_LIST_SCENE_NULL) {
        return;
    }
    song_set_song_list_length(sngedits.edit_pos,
        seq_utils_clamp(song_get_song_list_length(sngedits.edit_pos) + change,
            SEQ_SONG_LIST_MIN_LENGTH, SEQ_SONG_LIST_MAX_LENGTH));
    song_edit_update_display();
}

// adjust the kb trans
void song_edit_adjust_kbtrans(int change, int shift) {
    // current slot is empty
    if(song_get_song_list_scene(sngedits.edit_pos) == SONG_LIST_SCENE_NULL) {
        return;
    }
    song_set_song_list_kbtrans(sngedits.edit_pos,
        seq_utils_clamp(song_get_song_list_kbtrans(sngedits.edit_pos) + change,
            SEQ_TRANSPOSE_MIN, SEQ_TRANSPOSE_MAX));
    song_edit_update_display();
}

// remove the current step
void song_edit_remove_step(void) {
    song_remove_song_list_entry(sngedits.edit_pos);
    song_edit_update_display();
}

//
// local functions
//
// update the song display
void song_edit_update_display(void) {
    int row, entry, scene;
    char tempstr[GFX_LABEL_LEN];
    sprintf(tempstr, "Song Event List");
    gui_set_status_text(0, tempstr);

    // figure out scrolling
    panel_utils_scroll_list(SONG_EDIT_DISPLAY_SLOTS,
        sngedits.edit_pos, &sngedits.display_pos);
    for(row = 0; row < SONG_EDIT_DISPLAY_SLOTS; row ++) {
        entry = sngedits.display_pos + row;
        if(entry == sngedits.edit_pos) {
            sprintf(tempstr, ">%3d: ", (entry + 1));
        }
        else {
            sprintf(tempstr, " %3d: ", (entry + 1));
        }
        gui_set_status_text_part(row + 1, 0, 6, tempstr);
        // get scene
        scene = song_get_song_list_scene(entry);
        if(scene == SONG_LIST_SCENE_NULL) {
            gui_set_status_text_part(row + 1, 6, 7, "-------");  // scene
            gui_set_status_text_part(row + 1, 14, 5, "-----");  // bars/beats
            gui_set_status_text_part(row + 1, 20, 3, "---");  // kbtrans
        }
        else {
            sprintf(tempstr, "Scene %d", (scene + 1));
            gui_set_status_text_part(row + 1, 6, 7, tempstr);  // scene
            sprintf(tempstr, "%5d", song_get_song_list_length(entry));
            gui_set_status_text_part(row + 1, 14, 5, tempstr);  // bars/beats
            panel_utils_transpose_to_str(tempstr,
                song_get_song_list_kbtrans(entry));
            gui_set_status_text_part(row + 1, 20, 3, tempstr);  // kbtrans
        }
    }
    // make the mode state active for a while
    sngedits.edit_timeout = panel_menu_get_timeout();
}
