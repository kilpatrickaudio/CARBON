/*
 * CARBON Sequencer GUI
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
#include "gui.h"
#include "panel.h"
#include "../config.h"
#include "../gfx.h"
#include "../seq/pattern.h"
#include "../seq/scale.h"
#include "../seq/seq_ctrl.h"
#include "../seq/seq_engine.h"
#include "../seq/song.h"
#include "../seq/clock.h"
#include "../util/log.h"
#include "../util/panel_utils.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// labels
//
#define GUI_NUM_STATUS_LINES 4
#define GUI_STATUS_LINE_LEN 28
#define GUI_MAX_LABELS 13  // set this to the total number of labels
#define GUI_LABEL_SONG 0  // song display
#define GUI_LABEL_SCENE 1  // scene number
#define GUI_LABEL_RUN 2  // run indicator
#define GUI_LABEL_REC 3  // rec indicator
#define GUI_LABEL_TEMPO 4  // tempo display
#define GUI_LABEL_LIVE 5  // live indicator
#define GUI_LABEL_SONG_MODE 6  // song mode indicator
#define GUI_LABEL_STATUS_L1 7  // status line 1
#define GUI_LABEL_STATUS_L2 8  // status line 2
#define GUI_LABEL_STATUS_L3 9  // status line 3
#define GUI_LABEL_STATUS_L4 10  // status line 4
#define GUI_LABEL_KEYTRANS 11  // keyboard transpose
#define GUI_LABEL_CLKSRC 12  // clock source

//
// colors
//
#define GUI_BG_COLOR 0xff000000
#define GUI_FONT_COLOR_NORMAL 0xffeeeeee
#define GUI_FONT_COLOR_NORMAL_DIM 0xff696969
#define GUI_FONT_COLOR_RED 0xffff0000
#define GUI_FONT_COLOR_RED_DIM 0xff990000
#define GUI_FONT_COLOR_GREEN 0xff00ff00
#define GUI_FONT_COLOR_GREEN_DIM 0xff669966
#define GUI_FONT_COLOR_YELLOW 0xffffff00
#define GUI_FONT_COLOR_YELLOW_DIM 0xff996600
#define GUI_FONT_COLOR_MAGENTA 0xffffff00
#define GUI_FONT_COLOR_MAGENTA_DIM 0xff999900
#define GUI_FONT_COLOR_CYAN 0xff00ffff
#define GUI_FONT_COLOR_CYAN_DIM 0xff009999
#define GUI_FONT_COLOR_GREY 0xff999999
#define GUI_FONT_COLOR_DARK_GREY 0xff666666
#define GUI_TEXT_BG_COLOR 0xff000000
#define GUI_MIDI_IND_BG_COLOR 0xff666666
#define GUI_FONT_COLOR_HIGHLIGHT_BG 0xff333333
// grid squares
#define GUI_GRID_BG_COLOR 0xff000000
#define GUI_TRACK_UNSELECT_COLOR 0xff333333
uint32_t GUI_GRID_TRACK_COLOR_ACTIVE[] = {
    0xffff0000,
    0xffffcc00,
    0xff99ff33,
    0xff00ffff,
    0xffffffff,
    0xffff33ff
};
uint32_t GUI_GRID_TRACK_COLOR_NORMAL[][2] = {
    {0xff696969, 0xff990000},
    {0xff696969, 0xff990000},
    {0xff696969, 0xff990000},
    {0xff696969, 0xff990000},
    {0xff696969, 0xff990000},
    {0xff696969, 0xff990000}
};
uint32_t GUI_GRID_TRACK_COLOR_MUTED[][2] = {
    {0xff222222, 0xff444444},
    {0xff222222, 0xff444444},
    {0xff222222, 0xff444444},
    {0xff222222, 0xff444444},
    {0xff222222, 0xff444444},
    {0xff222222, 0xff444444}
};
uint32_t GUI_GRID_TRACK_COLOR_OFF[][2] = {
    {0xff000000, 0xff222222},
    {0xff000000, 0xff222222},
    {0xff000000, 0xff222222},
    {0xff000000, 0xff222222},
    {0xff000000, 0xff222222},
    {0xff000000, 0xff222222}
};
// overlay colors
#define GUI_OVERLAY_COLOR_BLANK 0x00000000
#define GUI_OVERLAY_COLOR_LOW 0xff000060
#define GUI_OVERLAY_COLOR_MED 0xff0000c0
#define GUI_OVERLAY_COLOR_HIGH 0xffc0c000

//
// sizes / positions / fonts
//
// 320x480 3.95" display
#if defined(GUI_DISP_TYPE_A)
// font indexes
#define GUI_FONT_NORMAL GFX_FONT_SMALLTEXT_8X10
#define GUI_FONT_HEADING GFX_FONT_SYSTEM_8X12
// grid settings
#define GUI_GRID_X 0
#define GUI_GRID_Y 50
#define GUI_GRID_W 238
#define GUI_GRID_H 238
#define GUI_GRID_SQUARE_X (GUI_GRID_X + GUI_GRID_SQUARE_SPACE) + 2
#define GUI_GRID_SQUARE_Y (GUI_GRID_Y + GUI_GRID_SQUARE_SPACE) + 2
#define GUI_GRID_SQUARE_W 27
#define GUI_GRID_SQUARE_H 27
#define GUI_GRID_START_SQUARE_W 3
#define GUI_GRID_TRACK_H 4
#define GUI_GRID_SQUARE_SPACE 2
#define GUI_PREVIEW_X 5
#define GUI_PREVIEW_Y 291
#define GUI_PREVIEW_SQUARE_W 4
#define GUI_PREVIEW_SQUARE_H 4
#define GUI_PREVIEW_GRID_SPACING 39
#define GUI_PREVIEW_SELECT_Y 326
#define GUI_PREVIEW_SELECT_H 4
#define GUI_PREVIEW_ARP_Y 332
// text positions
#define GUI_LABEL_SONG_X 3
#define GUI_LABEL_SONG_Y 10
#define GUI_LABEL_TEMPO_X 80
#define GUI_LABEL_TEMPO_Y 10
#define GUI_LABEL_SCENE_X 176
#define GUI_LABEL_SCENE_Y 10
#define GUI_LABEL_RUN_X 3
#define GUI_LABEL_RUN_Y 28
#define GUI_LABEL_REC_X 36
#define GUI_LABEL_REC_Y 28
#define GUI_LABEL_CLKSRC_X 80
#define GUI_LABEL_CLKSRC_Y 28
#define GUI_LABEL_KEYTRANS_X 130
#define GUI_LABEL_KEYTRANS_Y 28
#define GUI_LABEL_LIVE_X 200
#define GUI_LABEL_LIVE_Y 28
#define GUI_LABEL_SONG_MODE_X 3
#define GUI_LABEL_SONG_MODE_Y 40
#define GUI_LABEL_STATUS_L1_X 8
#define GUI_LABEL_STATUS_L1_Y 345
#define GUI_LABEL_STATUS_L2_X 8
#define GUI_LABEL_STATUS_L2_Y 357
#define GUI_LABEL_STATUS_L3_X 8
#define GUI_LABEL_STATUS_L3_Y 369
#define GUI_LABEL_STATUS_L4_X 8
#define GUI_LABEL_STATUS_L4_Y 381
// 320x48 3.5" display
#elif defined(GUI_DISP_TYPE_B)
// font indexes
#define GUI_FONT_NORMAL GFX_FONT_SMALLTEXT_8X10
#define GUI_FONT_HEADING GFX_FONT_SYSTEM_8X13
// grid settings
#define GUI_GRID_X 0
#define GUI_GRID_Y 55
#define GUI_GRID_W 238
#define GUI_GRID_H 238
#define GUI_GRID_SQUARE_X (GUI_GRID_X + GUI_GRID_SQUARE_SPACE) + 2
#define GUI_GRID_SQUARE_Y (GUI_GRID_Y + GUI_GRID_SQUARE_SPACE) + 2
#define GUI_GRID_SQUARE_W 31
#define GUI_GRID_SQUARE_H 31
#define GUI_GRID_START_SQUARE_W 3
#define GUI_GRID_TRACK_H 4
#define GUI_GRID_SQUARE_SPACE 2
#define GUI_PREVIEW_X 5
#define GUI_PREVIEW_Y 329
#define GUI_PREVIEW_SQUARE_W 5
#define GUI_PREVIEW_SQUARE_H 5
#define GUI_PREVIEW_GRID_SPACING 44
#define GUI_PREVIEW_SELECT_Y 372
#define GUI_PREVIEW_SELECT_H 5
#define GUI_PREVIEW_ARP_Y 379
// text positions
#define GUI_LABEL_SONG_X 3
#define GUI_LABEL_SONG_Y 10
#define GUI_LABEL_TEMPO_X 95
#define GUI_LABEL_TEMPO_Y 10
#define GUI_LABEL_SCENE_X 205
#define GUI_LABEL_SCENE_Y 10
#define GUI_LABEL_RUN_X 3
#define GUI_LABEL_RUN_Y 30
#define GUI_LABEL_REC_X 36
#define GUI_LABEL_REC_Y 30
#define GUI_LABEL_CLKSRC_X 90
#define GUI_LABEL_CLKSRC_Y 30
#define GUI_LABEL_KEYTRANS_X 160
#define GUI_LABEL_KEYTRANS_Y 30
#define GUI_LABEL_LIVE_X 235
#define GUI_LABEL_LIVE_Y 30
#define GUI_LABEL_SONG_MODE_X 3
#define GUI_LABEL_SONG_MODE_Y 43
#define GUI_LABEL_STATUS_L1_X 8
#define GUI_LABEL_STATUS_L1_Y 394
#define GUI_LABEL_STATUS_L2_X 8
#define GUI_LABEL_STATUS_L2_Y 406
#define GUI_LABEL_STATUS_L3_X 8
#define GUI_LABEL_STATUS_L3_Y 418
#define GUI_LABEL_STATUS_L4_X 8
#define GUI_LABEL_STATUS_L4_Y 430
#endif

//
// state
//
struct gui_state {
    // grid square colors
    uint32_t grid_state[SEQ_NUM_STEPS];  // colors of main grid
    uint32_t grid_overlay[SEQ_NUM_STEPS];  // colors of the grid overlay
    int grid_overlay_enable;  // 0 = disable, 1 = enable
    uint32_t preview_state[SEQ_NUM_TRACKS][SEQ_NUM_STEPS];  // colors of preview grids
    uint32_t track_select_state[SEQ_NUM_TRACKS];  // colors of track select state
    uint32_t arp_enable_state[SEQ_NUM_TRACKS];  // colors of the arp enable state
    //
    // MIDI activity timeouts
    //
    uint8_t midi_act_timeouts[8];  // timeouts for showing MIDI activity
    //
    // current state cache from sequencer
    //
    // grid displays and track into (required per track)
    uint8_t motion_step[SEQ_NUM_TRACKS][SEQ_NUM_STEPS];  // step enables for each track
    uint8_t motion_start[SEQ_NUM_TRACKS];  // motion start pos for each track
    uint8_t motion_length[SEQ_NUM_TRACKS];  // motion length for each track
    uint8_t pattern_type[SEQ_NUM_TRACKS];  // pattern type for each track
    uint8_t track_select[SEQ_NUM_TRACKS];  // track select state for each track
    uint8_t arp_enable[SEQ_NUM_TRACKS];  // arp enable state for each track
    uint8_t track_mute[SEQ_NUM_TRACKS];  // track mute state for each track
    uint8_t active_step[SEQ_NUM_TRACKS];  // currently active step for each track
    // cached status for display
    uint8_t current_scene;  // the current scene
    uint8_t first_track;  // the first selected track from the sequencer
    // status display mode
    uint8_t status_override;  // 1 = status is in use by function, 0 = status shows track info
    // labels and temp text
    struct gfx_label glabels[GUI_MAX_LABELS];
    char status_lines[GUI_NUM_STATUS_LINES][GFX_LABEL_LEN];  // temp strs that can be built up
    // refresh stuff
    uint8_t force_refresh;  // 0 = no refresh, 1 = force refresh
    uint8_t force_reinit;  // 0 = no reinit, 1 = reinit
    // override
    uint8_t enabled;  // 0 = disabled, 1 = enabled
    uint8_t desired_enable_state;  // 0 = disable, 1 = enable
};
// put data into CCMRAM instead of regular RAM
struct gui_state gstate __attribute__ ((section (".ccm")));

char tempstr[GFX_LABEL_LEN];

// local functions
void gui_handle_state_change(int event_type, int *data, int data_len);
void gui_draw_grid_bg(void);
int gui_draw_main_grid(void);
int gui_draw_preview_grids(void);
int gui_draw_labels(void);
void gui_set_label(int index, char *text);
void gui_set_label_highlight(int index, int startch, int lench, int mode);
void gui_set_label_prefs(int index, int x, int y, int w, int h, 
    int font, uint32_t fg_color, uint32_t bg_color);
void gui_set_label_color(int index, uint32_t fg_color, 
    uint32_t bg_color);
void gui_calculate_motion_steps(int track);
void gui_update_song(int song);
void gui_update_scene(int scene);
void gui_update_track_select(int track, int select);
void gui_update_first_track(int first);
void gui_update_tempo(float tempo);
void gui_update_run_enable(int enable);
void gui_update_rec_mode(int mode);
void gui_update_clock_source(int source);
void gui_update_live_mode(int mode);
void gui_update_keyboard_transpose(int transpose);
void gui_update_track_mute(int scene, int track, int mute);
void gui_update_bias_track(int scene, int track, int bias_track);
void gui_update_track_transpose(int scene, int track, int transpose);
void gui_update_arp_enable(int scene, int track, int enable);
void gui_update_tonality(int scene, int track, int tonality);
void gui_update_motion_start(int scene, int track, int pos);
void gui_update_motion_length(int scene, int track, int length);
void gui_update_motion_dir(int scene, int track, int rev);
void gui_update_step_length(int scene, int track, int len);
void gui_update_gate_time_override(int scene, int track, int time);
void gui_update_pattern_type(int scene, int track, int pattern);
void gui_update_track_type(int scene, int track, int type);
void gui_update_active_step(int track, int step);
void gui_update_song_mode(void);

// init the GUI
int gui_init(void) {
    int i, j;
	log_info("starting up GUI...");

#ifdef DEBUG_SIM
	// load fonts in simulation mode
	// (embedded mode will already have the fonts available)
	if(gfx_load_font(GUI_FONT_NORMAL, "LiberationMono-Regular.ttf", 12) != 0) {
	    return -1;
	}
	// load fonts in simulation mode
	// (embedded mode will already have the fonts available)
	if(gfx_load_font(GUI_FONT_HEADING, "LiberationMono-Regular.ttf", 14) != 0) {
	    return -1;
	}
#endif

    //
    // reset stuff
    //
    gstate.enabled = 1;  // enabled
    gstate.desired_enable_state = 1;  // enable

    // reset all the labels
    for(i = 0; i < GUI_MAX_LABELS; i ++) {
        gstate.glabels[i].x = -1;
        gstate.glabels[i].y = -1;
        gstate.glabels[i].w = 10;
        gstate.glabels[i].h = 10;
        gstate.glabels[i].font = 0;
        gstate.glabels[i].fg_color = 0xffffffff;
        gstate.glabels[i].bg_color = 0xff111111;
        sprintf(gstate.glabels[i].text, " ");
        for(j = 0; j < GFX_LABEL_LEN; j ++) {
            gstate.glabels[i].highlight[j] = GFX_HIGHLIGHT_NORMAL;
        }
        gstate.glabels[i].dirty = 1;
    }
    
    //
    // set the prefs for each label
    //
    gui_set_label_prefs(GUI_LABEL_SONG, GUI_LABEL_SONG_X, GUI_LABEL_SONG_Y, 
        60, 12, GUI_FONT_HEADING, GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);

    gui_set_label_prefs(GUI_LABEL_TEMPO, GUI_LABEL_TEMPO_X, GUI_LABEL_TEMPO_Y,
        80, 12, GUI_FONT_HEADING, GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);        

    gui_set_label_prefs(GUI_LABEL_SCENE, GUI_LABEL_SCENE_X, GUI_LABEL_SCENE_Y,
        60, 12, GUI_FONT_HEADING, GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);

    gui_set_label_prefs(GUI_LABEL_RUN, GUI_LABEL_RUN_X, GUI_LABEL_RUN_Y, 
        20, 12, GUI_FONT_NORMAL, GUI_FONT_COLOR_GREEN_DIM, GUI_TEXT_BG_COLOR);
    gui_set_label(GUI_LABEL_RUN, "RUN");
    
    gui_set_label_prefs(GUI_LABEL_REC, GUI_LABEL_REC_X, GUI_LABEL_REC_Y,
        20, 12, GUI_FONT_NORMAL, GUI_FONT_COLOR_RED_DIM, GUI_TEXT_BG_COLOR);
    gui_set_label(GUI_LABEL_REC, "REC");

    gui_set_label_prefs(GUI_LABEL_CLKSRC, GUI_LABEL_CLKSRC_X, GUI_LABEL_CLKSRC_Y,
        20, 12, GUI_FONT_NORMAL, GUI_FONT_COLOR_GREY, GUI_TEXT_BG_COLOR);
    gui_set_label(GUI_LABEL_CLKSRC, "INT");

    gui_set_label_prefs(GUI_LABEL_KEYTRANS, GUI_LABEL_KEYTRANS_X, GUI_LABEL_KEYTRANS_Y,
        60, 12, GUI_FONT_NORMAL, GUI_FONT_COLOR_MAGENTA_DIM, GUI_TEXT_BG_COLOR);
    gui_update_keyboard_transpose(0);
        
    gui_set_label_prefs(GUI_LABEL_LIVE, GUI_LABEL_LIVE_X, GUI_LABEL_LIVE_Y,
        30, 12, GUI_FONT_NORMAL, GUI_FONT_COLOR_CYAN_DIM, GUI_TEXT_BG_COLOR);
    gui_set_label(GUI_LABEL_LIVE, "LIVE");

    gui_set_label_prefs(GUI_LABEL_SONG_MODE, GUI_LABEL_SONG_MODE_X, GUI_LABEL_SONG_MODE_Y,
        20, 12, GUI_FONT_NORMAL, GUI_FONT_COLOR_YELLOW_DIM, GUI_TEXT_BG_COLOR);
    gui_set_label(GUI_LABEL_SONG_MODE, "SONG MODE");

    gui_set_label_prefs(GUI_LABEL_STATUS_L1, GUI_LABEL_STATUS_L1_X, GUI_LABEL_STATUS_L1_Y, 
        224, 10, GUI_FONT_NORMAL, GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);
        
    gui_set_label_prefs(GUI_LABEL_STATUS_L2, GUI_LABEL_STATUS_L2_X, GUI_LABEL_STATUS_L2_Y,
        224, 10, GUI_FONT_NORMAL, GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);

    gui_set_label_prefs(GUI_LABEL_STATUS_L3, GUI_LABEL_STATUS_L3_X, GUI_LABEL_STATUS_L3_Y,
        224, 10, GUI_FONT_NORMAL, GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);

    gui_set_label_prefs(GUI_LABEL_STATUS_L4, GUI_LABEL_STATUS_L4_X, GUI_LABEL_STATUS_L4_Y,
        224, 10, GUI_FONT_NORMAL, GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);

    // reset the main grid state
    for(i = 0; i < SEQ_NUM_STEPS; i ++) {
        gstate.grid_state[i] = 0;
        gstate.grid_overlay[i] = 0;
    }
    gstate.grid_overlay_enable = 0;
    
    // reset the preview grid states
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        gstate.track_select_state[i] = 0x00;
        gstate.arp_enable_state[i] = 0x00;
        for(j = 0; j < SEQ_NUM_STEPS; j ++) {
            gstate.preview_state[i][j] = 0x00;
        }
    }

    // reset the grid cache
    for(i = 0; i < SEQ_NUM_TRACKS; i ++) {
        gstate.motion_start[i] = 0;
        gstate.motion_length[i] = 0;
        gstate.pattern_type[i] = 0;
        gstate.active_step[i] = 0;
        gstate.track_select[i] = 0;
        gstate.arp_enable[i] = 0;
        gstate.track_mute[i] = 0;
        for(j = 0; j < SEQ_NUM_STEPS; j ++) {
            gstate.motion_step[i][j] = 0;
        }
    }

	// draw display for the first time
	gfx_clear_screen(GUI_BG_COLOR);	
	gui_draw_grid_bg();
	// mark components dirty
    for(i = 0; i < GUI_MAX_LABELS; i ++) {
        gstate.glabels[i].dirty = 1;
    }
    for(i = 0; i < SEQ_NUM_STEPS; i ++) {
        gstate.grid_state[i] = 0;
    }
    gui_set_status_override(0);
    gstate.force_refresh = 0;
    gstate.force_reinit = 0;
	
    // register for events
    state_change_register(gui_handle_state_change, SCEC_SONG);
    state_change_register(gui_handle_state_change, SCEC_CTRL);
    state_change_register(gui_handle_state_change, SCEC_ENG);
    state_change_register(gui_handle_state_change, SCEC_CLK);
    return 0;
}

// close the GUI
void gui_close(void) {
	gfx_close();
}

// run the refresh task - run on the main polling loop
void gui_refresh_task(void) {
	int dirty = 0;
	int i, track, step;
    // change enable state
    if(gstate.enabled != gstate.desired_enable_state) {
        if(gstate.desired_enable_state) {
            gui_force_refresh();
        }
        else {
            gui_clear_screen();
        }
        gstate.enabled = gstate.desired_enable_state;
    }

    // don't draw if not enabled
    if(!gstate.enabled) {
        return;
    }

	// force LCD reinit
	if(gstate.force_reinit) {
	    gstate.force_reinit = 0;
	    gstate.force_refresh = 1;
	    gfx_init_lcd();  // power on and init LCD
	}
	// force refresh
	if(gstate.force_refresh) {
        gstate.force_refresh = 0;
	    gui_clear_screen();
	    // dirty all the labels
        for(i = 0; i < GUI_MAX_LABELS; i ++) {
            gstate.glabels[i].dirty = 1;
        }
        // dirty all the grid state
        for(step = 0; step < SEQ_NUM_STEPS; step ++) {
            gstate.grid_state[step] = 0;
            gstate.grid_overlay[step] = 0;
        }
        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            gstate.track_select_state[track] = 0;
            gstate.arp_enable_state[track] = 0;
            for(step = 0; step < SEQ_NUM_STEPS; step ++) {
                gstate.preview_state[track][step] = 0;
            }
        } 
	}
    dirty += gui_draw_labels();
	// attempt to redraw the squares
	dirty += gui_draw_preview_grids();
    dirty += gui_draw_main_grid();
	if(dirty) {
		gfx_commit();
	}
}

// force all GUI items to refresh
void gui_force_refresh(void) {
    gstate.force_refresh = 1;
}

// clear the screen
void gui_clear_screen(void) {
    gfx_clear_screen(0);
}

// enable or disable the entire GUI so we can use the screen ourselves
void gui_set_enable(int enable) {
    gstate.desired_enable_state = enable;
}

// set the LCD power state
// schedule a power-up and reinit (power on)
// turns off power immediately (power off)
void gui_set_lcd_power(int state) {
    if(state) {
        gstate.force_reinit = 1;  // request a power on and reinit
    }
    else {
        gfx_deinit_lcd();  // power off immediately
    }
}

// enable or disable the grid overlay
void gui_grid_set_overlay_enable(int enable) {
    if(enable) {
        gstate.grid_overlay_enable = 1;
    }
    else {
        gstate.grid_overlay_enable = 0;
    }
}

// clear the grid overlay
void gui_grid_clear_overlay(void) {
    int i;
    for(i = 0; i < SEQ_NUM_STEPS; i ++) {
        gstate.grid_overlay[i] = 0;
    }
}

// set a grid overlay color index
void gui_grid_set_overlay_color(int step, int index) {
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        return;
    }
    switch(index) {
        case GUI_OVERLAY_BLANK:
            gstate.grid_overlay[step] = GUI_OVERLAY_COLOR_BLANK;
            break;
        case GUI_OVERLAY_LOW:
            gstate.grid_overlay[step] = GUI_OVERLAY_COLOR_LOW;
            break;
        case GUI_OVERLAY_MED:
            gstate.grid_overlay[step] = GUI_OVERLAY_COLOR_MED;
            break;
        case GUI_OVERLAY_HIGH:
            gstate.grid_overlay[step] = GUI_OVERLAY_COLOR_HIGH;
            break;
    }
}

//
// status display helpers
//
// set the status / menu override state
void gui_set_status_override(int enable) {
    // overriding the status display for menu / status info
    if(enable) {
        gstate.status_override = 1;
    }
    // reverting back to locally generated info display
    else {
        gstate.status_override = 0;
        gui_clear_menu();
        gui_update_first_track(gstate.first_track);  // cause info to be refreshed
    }
}

// clear all status text
void gui_clear_status_text_all(void) {
    gui_clear_status_text(0);
    gui_clear_status_text(1);
    gui_clear_status_text(2);
    gui_clear_status_text(3);
}

// clear a line of status text
void gui_clear_status_text(int line) {
    int i;
    if(line < 0 || line >= GUI_NUM_STATUS_LINES) {
        return;
    }
    for(i = 0; i < GUI_STATUS_LINE_LEN; i ++) {
        gstate.status_lines[line][i] = ' ';
    }
    gstate.status_lines[line][i] = 0x00;
    gui_set_label(GUI_LABEL_STATUS_L1 + line, gstate.status_lines[line]);
    gui_set_label_highlight(GUI_LABEL_STATUS_L1 + line, 
        0, GUI_STATUS_LINE_LEN, GFX_HIGHLIGHT_NORMAL);
}

// set a line of status text
void gui_set_status_text(int line, char *text) {
    int i, len;
    if(line < 0 || line >= GUI_NUM_STATUS_LINES) {
        return;
    }
    len = strlen(text);
    if(len > GUI_STATUS_LINE_LEN) {
        len = GUI_STATUS_LINE_LEN;
    }    
    memcpy(gstate.status_lines[line], text, len);    
    // pad out line
    for(i = strlen(text); i < GUI_STATUS_LINE_LEN; i ++) {
        gstate.status_lines[line][i] = ' ';
    }
    gstate.status_lines[line][i] = 0x00;
    gui_set_label(GUI_LABEL_STATUS_L1 + line, gstate.status_lines[line]);
}

// set a portion of a status line
// this will clear the range and truncate the line if it's too long
void gui_set_status_text_part(int line, int startch, int lench, char *text) {
    int i, len;
    if(line < 0 || line >= 4) {
        log_error("gsstp - line invalid: %d", line);
        return;
    }
    if(startch < 0 || lench < 1 || (startch + lench) > (GFX_LABEL_LEN - 1)) {
        log_error("gsstp - range invalid - s: %d - l: %d", startch, lench);
        return;
    }
    len = strlen(text);
    if(len > lench) {
        len = lench;
    }
    memcpy(gstate.status_lines[line] + startch, text, len);
    for(i = startch + strlen(text); i < (startch + lench); i ++) {
        gstate.status_lines[line][i] = ' ';
    }
    gstate.status_lines[line][GUI_STATUS_LINE_LEN] = 0x00;
    gui_set_label(GUI_LABEL_STATUS_L1 + line, gstate.status_lines[line]);
}

// set the highlight of a status line
void gui_set_status_highlight_part(int line, int startch, 
        int lench, int mode) {
    if(line < 0 || line >= 4) {
        log_error("gsshp - line invalid: %d", line);
        return;
    }
    if(startch < 0 || lench < 1 || (startch + lench) >= GFX_LABEL_LEN) {
        log_error("gsshp - range invalid - s: %d - l: %d", startch, lench);
        return;
    }
    gui_set_label_highlight(GUI_LABEL_STATUS_L1 + line, startch, lench, mode);
}

//
// menu control
//
// clear the menu
void gui_clear_menu(void) {
    gui_clear_status_text(0);
    gui_clear_status_text(1);
    gui_clear_status_text(2);
    gui_clear_status_text(3);
}

// set the menu title
void gui_set_menu_title(char *text) {
    gui_set_status_text_part(0, 0, 14, text);
}

// set the menu next/prev
void gui_set_menu_prev_next(int prev, int next) {
    if(!prev && next) {
        gui_set_status_text_part(0, 14, 13, "            >");
    }
    else if(prev && !next) {
        gui_set_status_text_part(0, 14, 13, "          <  ");    
    }
    else if(prev && next) {
        gui_set_status_text_part(0, 14, 13, "          < >");    
    }
    else {
        gui_set_status_text_part(0, 14, 13, "             ");
    }
}

// set the menu subtitle
void gui_set_menu_subtitle(char *text) {
    gui_set_status_text(1, text);
}

// set the menu param
void gui_set_menu_param(char *text) {
    gui_set_status_text_part(3, 0, 14, text);
}

// set the menu value
void gui_set_menu_value(char *text) {
    gui_set_status_text_part(3, 14, 13, text);
}

// set the edit mode on the menu
void gui_set_menu_edit(int edit) {
    if(edit) {
        gui_set_label_highlight(GUI_LABEL_STATUS_L4, 14, 14, GFX_HIGHLIGHT_INVERT);
    }
    else {
        gui_set_label_highlight(GUI_LABEL_STATUS_L4, 14, 14, GFX_HIGHLIGHT_NORMAL);
    }
}

//
// local functions
//
// handle state change
void gui_handle_state_change(int event_type, int *data, int data_len) {
    switch(event_type) {
        case SCE_SONG_CLEARED:
            gui_update_song(data[0]);
            gui_update_tempo(song_get_tempo());
            gui_update_scene(seq_ctrl_get_scene());
            gui_update_song_mode();
            break;
        case SCE_SONG_LOADED:
            gui_update_song(data[0]);
            gui_update_tempo(song_get_tempo());
            gui_update_scene(seq_ctrl_get_scene());
            gui_update_song_mode();
            break;        
        case SCE_SONG_SAVED:
            gui_update_song(data[0]);
            break;
        case SCE_CTRL_TRACK_SELECT:
            gui_update_track_select(data[0], data[1]);
            break;
        case SCE_CTRL_FIRST_TRACK:
            gui_update_first_track(data[0]);
            break;            
        case SCE_SONG_TEMPO:
            gui_update_tempo(song_get_tempo());
            break;
        case SCE_CTRL_RUN_STATE:
            gui_update_run_enable(data[0]);
            break;
        case SCE_CTRL_RECORD_MODE:
            gui_update_rec_mode(data[0]);
            break;
        case SCE_CTRL_LIVE_MODE:
            gui_update_live_mode(data[0]);
            break;
        case SCE_CTRL_SONG_MODE:
        case SCE_ENG_SONG_MODE_STATUS:
            gui_update_song_mode();
            break;
        case SCE_SONG_MUTE:
            gui_update_track_mute(data[0], data[1], data[2]);
            break;
        case SCE_SONG_TRANSPOSE:
            gui_update_track_transpose(data[0], data[1], data[2]);
            break;
        case SCE_SONG_ARP_ENABLE:
            gui_update_arp_enable(data[0], data[1], data[2]);
            break;
        case SCE_SONG_TONALITY:
            gui_update_tonality(data[0], data[1], data[2]);
            break;
        case SCE_SONG_MOTION_START:
            gui_update_motion_start(data[0], data[1], data[2]);
            break;
        case SCE_SONG_MOTION_LENGTH:
            gui_update_motion_length(data[0], data[1], data[2]);
            break;
        case SCE_SONG_MOTION_DIR:
            gui_update_motion_dir(data[0], data[1], data[2]);
            break;
        case SCE_SONG_STEP_LEN:
            gui_update_step_length(data[0], data[1], data[2]);
            break;
        case SCE_SONG_GATE_TIME:
            gui_update_gate_time_override(data[0], data[1], data[2]);
            break;
        case SCE_SONG_PATTERN_TYPE:
            gui_update_pattern_type(data[0], data[1], data[2]);            
            break;
        case SCE_SONG_TRACK_TYPE:
            gui_update_track_type(data[0], data[1], data[2]);            
            break;
        case SCE_ENG_CURRENT_SCENE:
            gui_update_scene(data[0]);
            break;
        case SCE_ENG_ACTIVE_STEP:
            gui_update_active_step(data[0], data[1]);
            break;
        case SCE_ENG_KBTRANS:
            gui_update_keyboard_transpose(data[0]);            
            break;
        case SCE_CLK_SOURCE:
            gui_update_clock_source(data[0]);
            break;
        default: 
            break;
    }
}

// draw the grid background
void gui_draw_grid_bg(void) {
    gfx_fill_rect(GUI_GRID_X, GUI_GRID_Y, 
        GUI_GRID_W, GUI_GRID_H, GUI_GRID_BG_COLOR);
}

// draw the grid squares if they are dirty
int gui_draw_main_grid(void) {
    int step, track, x, y;
    int dirty = 0;
    uint32_t color;
    track = seq_ctrl_get_first_track();
    for(step = 0; step < SEQ_NUM_STEPS; step ++) {
        // only redraw if the square has changed
        if(gstate.grid_overlay_enable && gstate.grid_overlay[step]) {
            color = gstate.grid_overlay[step];
        }
        else {
            color = gstate.preview_state[track][step];
        }
        if(gstate.grid_state[step] != color) {
            x = step & 0x07;
            y = (step >> 3) & 0x07;        
            gstate.grid_state[step] = color;
            gfx_fill_rect(GUI_GRID_SQUARE_X + 
                ((GUI_GRID_SQUARE_W + GUI_GRID_SQUARE_SPACE) * x), 
                GUI_GRID_SQUARE_Y + ((GUI_GRID_SQUARE_H + GUI_GRID_SQUARE_SPACE) * y),
                GUI_GRID_SQUARE_W, GUI_GRID_SQUARE_H,
                color);
            dirty = 1;
        }
    }
    return dirty;
}

// draw mini preview grids
int gui_draw_preview_grids(void) {
    int step, track, x, y;
    int dirty = 0;
    uint32_t color;
    // render each track grid
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        // figure out the colour for each step
        for(step = 0; step < SEQ_NUM_STEPS; step ++) {
            // figure out what the current color should be on the square
            // muted
            if(gstate.track_mute[track]) {
                // current step
                if(step == gstate.active_step[track]) {
                    color = GUI_GRID_TRACK_COLOR_ACTIVE[track];
                }
                // pattern enabled on step
                else if(pattern_get_step_enable(gstate.current_scene, track, 
                        gstate.pattern_type[track], step)) {
                    color = GUI_GRID_TRACK_COLOR_MUTED[track][gstate.motion_step[track][step]];
                }
                // off
                else {
                    color = GUI_GRID_TRACK_COLOR_OFF[track][gstate.motion_step[track][step]];
                }
            }
            // not muted
            else {
                // current step
                if(step == gstate.active_step[track]) {
                    color = GUI_GRID_TRACK_COLOR_ACTIVE[track];
                }
                // pattern enabled on step
                else if(pattern_get_step_enable(gstate.current_scene, track, 
                        gstate.pattern_type[track], step)) {
                    color = GUI_GRID_TRACK_COLOR_NORMAL[track][gstate.motion_step[track][step]];
                }
                // off
                else {
                    color = GUI_GRID_TRACK_COLOR_OFF[track][gstate.motion_step[track][step]];
                }
            }
            // only redraw if the square has changed
            if(gstate.preview_state[track][step] != color) {
                x = step & 0x07;
                y = (step >> 3) & 0x07;        
                gfx_fill_rect(GUI_PREVIEW_X + (GUI_PREVIEW_SQUARE_W * x) +
                  (GUI_PREVIEW_GRID_SPACING * track), 
                  GUI_PREVIEW_Y + (GUI_PREVIEW_SQUARE_H * y),
                  GUI_PREVIEW_SQUARE_W, GUI_PREVIEW_SQUARE_H,
                  color);
                gstate.preview_state[track][step] = color;
                dirty = 1;
            }
        }
        // track select indicator
        if(gstate.track_select[track]) {
            color = GUI_GRID_TRACK_COLOR_ACTIVE[track];
        }
        else {
            color = GUI_TRACK_UNSELECT_COLOR;
        }
        // only redraw if the select bar has changed
        if(color != gstate.track_select_state[track]) {
            gfx_fill_rect(GUI_PREVIEW_X + (GUI_PREVIEW_GRID_SPACING * track), 
              GUI_PREVIEW_SELECT_Y,
              (GUI_PREVIEW_SQUARE_W * 8), GUI_PREVIEW_SELECT_H,
              color);            
            gstate.track_select_state[track] = color;
        }
        // arp enable color        
        if(gstate.arp_enable[track]) {
            color = GUI_FONT_COLOR_YELLOW;
        }
        else {
            color = GUI_FONT_COLOR_YELLOW_DIM;            
        }        
        // only redraw if the arp bar has changed
        if(color != gstate.arp_enable_state[track]) {
            gfx_fill_rect(GUI_PREVIEW_X + (GUI_PREVIEW_GRID_SPACING * track), 
              GUI_PREVIEW_ARP_Y,
              (GUI_PREVIEW_SQUARE_W * 8), GUI_PREVIEW_SELECT_H,
              color);            
            gstate.arp_enable_state[track] = color;
        }
    }
    return dirty;
}

// draw the song display if it is dirty
int gui_draw_labels(void) {
    int i, dirty = 0;
    for(i = 0; i < GUI_MAX_LABELS; i ++) {
        if(gstate.glabels[i].x == -1 || gstate.glabels[i].dirty == 0) {
            continue;
        }
        gfx_fill_rect(gstate.glabels[i].x, gstate.glabels[i].y,
            gstate.glabels[i].w, gstate.glabels[i].h, gstate.glabels[i].bg_color);
        gfx_draw_string(&gstate.glabels[i]);
        gstate.glabels[i].dirty = 0;
        dirty = 1;
    }
    return dirty;
}

// set a label
void gui_set_label(int index, char *text) {
    if(index < 0 || index >= GUI_MAX_LABELS) {
        log_error("gsl - index invalid: %d", index);
        return;
    }
    // string is unchanged - don't redraw
    if(!strcmp(text, gstate.glabels[index].text)) {
        return;
    }    
    // don't crash with zero length strings
    if(*text == 0x00) {
        strcpy(gstate.glabels[index].text, " ");
    }
    else {
        strcpy(gstate.glabels[index].text, text);
    }
    gstate.glabels[index].dirty = 1;
}

// set label highlight
void gui_set_label_highlight(int index, int startch, int lench, int mode) {
    int i;
    if(index < 0 || index >= GUI_MAX_LABELS) {
        log_error("gslh - index invalid: %d", index);
        return;
    }
    if(mode < 0 || mode >= GFX_HIGHLIGHT_MAX_MODES) {
        log_error("gslh - mode invalid: %d", mode);
        return;
    }
    if(startch < 0 || lench < 1 || (startch + lench) > GFX_LABEL_LEN) {
        log_error("gslh - range invalid - s: %d - l: %d", startch, lench);
        return;
    }
    for(i = startch; i < (startch + lench); i ++) {
        gstate.glabels[index].highlight[i] = mode;
    }
    gstate.glabels[index].dirty = 1;
}

// set the prefs for a label
void gui_set_label_prefs(int index, int x, int y, int w, int h, 
        int font, uint32_t fg_color, uint32_t bg_color) {
    if(index < 0 || index >= GUI_MAX_LABELS) {
        log_error("gslp - index invalid: %d", index);
        return;
    }        
    gstate.glabels[index].x = x;
    gstate.glabels[index].y = y;
    gstate.glabels[index].w = w;
    gstate.glabels[index].h = h;
    gstate.glabels[index].font = font;
    gstate.glabels[index].fg_color = fg_color;
    gstate.glabels[index].bg_color = bg_color;
    gstate.glabels[index].dirty = 1;
}

// set the color of a label
void gui_set_label_color(int index, uint32_t fg_color, uint32_t bg_color) {
    if(index < 0 || index >= GUI_MAX_LABELS) {
        log_error("gslc - index invalid: %d", index);
        return;
    }        
    gstate.glabels[index].fg_color = fg_color;
    gstate.glabels[index].bg_color = bg_color;
    gstate.glabels[index].dirty = 1;
}

// calculate the motion squares for a track
void gui_calculate_motion_steps(int track) {
    int i;
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gcms - track invalid: %d", track);
        return;
    }
    // figure out which steps are enabled for motion
    for(i = 0; i < SEQ_NUM_STEPS; i ++) {
        gstate.motion_step[track][i] = seq_utils_is_step_active(i, 
                gstate.motion_start[track], 
                gstate.motion_length[track], SEQ_NUM_STEPS);
    }
}

// update the song number display
void gui_update_song(int song) {
    sprintf(tempstr, "SONG %d", (song + 1));
    gui_set_label(GUI_LABEL_SONG, tempstr);
}

// update the scene because it probably changed
void gui_update_scene(int scene) {
    int track;
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gus - scene invalid: %d", scene);
        return;
    }
    sprintf(tempstr, "SCENE %d", (scene + 1));
    gui_set_label(GUI_LABEL_SCENE, tempstr);
    
    gstate.current_scene = scene;
    
    // refresh all display info
    // we need this stuff for all tracks
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        gui_update_step_length(scene, track,
            song_get_step_length(scene, track));
        gui_update_motion_start(scene, track, 
            song_get_motion_start(scene, track));
        gui_update_motion_length(scene, track,
            song_get_motion_length(scene, track));
        gui_update_pattern_type(scene, track,
            song_get_pattern_type(scene, track));
        gui_update_arp_enable(scene, track,
            song_get_arp_enable(scene, track));
        gui_update_track_mute(scene, track,
            song_get_mute(scene, track));
        gui_update_track_select(track, 
            seq_ctrl_get_track_select(track));
    }
}

// update the track select state
void gui_update_track_select(int track, int select) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("guts - track invalid: %d", track);
        return;
    }    
    if(select) {
        gstate.track_select[track] = 1;
    }
    else {
        gstate.track_select[track] = 0;    
    }
    
    // check to see if the first track changed
    if(seq_ctrl_get_first_track() != gstate.first_track) {
        gui_update_first_track(seq_ctrl_get_first_track());
    }
}

// update the first selected track
void gui_update_first_track(int first) {
    if(first < 0 || first >= SEQ_NUM_TRACKS) {
        log_error("guft - first invalid: %d", first);
        return;
    }
    gstate.first_track = first;
    
    // track number
    if(!gstate.status_override) {
        sprintf(tempstr, "Track: %d", (gstate.first_track + 1));
        gui_set_status_text_part(0, 0, 8, tempstr);
    }
    gui_update_track_transpose(gstate.current_scene, first, 
        song_get_transpose(gstate.current_scene, first));
    gui_update_tonality(gstate.current_scene, first,
        song_get_tonality(gstate.current_scene, first));
    gui_update_bias_track(gstate.current_scene, first,
        song_get_bias_track(gstate.current_scene, first));
    gui_update_motion_start(gstate.current_scene, first,
        song_get_motion_start(gstate.current_scene, first));
    gui_update_motion_length(gstate.current_scene, first,
        song_get_motion_length(gstate.current_scene, first));
    gui_update_motion_dir(gstate.current_scene, first,
        song_get_motion_dir(gstate.current_scene, first));
    gui_update_step_length(gstate.current_scene, first,
        song_get_step_length(gstate.current_scene, first));
    gui_update_gate_time_override(gstate.current_scene, first, 
        song_get_gate_time(gstate.current_scene, first));
    gui_update_track_type(gstate.current_scene, first,
        song_get_track_type(first));      
    gui_update_track_mute(gstate.current_scene, first,
        song_get_mute(gstate.current_scene, first));
}

// update the tempo display
void gui_update_tempo(float tempo) {
    int whole = (int)tempo;
    int fract = (int)(tempo * 10) % 10;
    sprintf(tempstr, "%d.%d BPM", whole, fract);
    gui_set_label(GUI_LABEL_TEMPO, tempstr);
}

// update the run indicator state
void gui_update_run_enable(int enable) {
    if(enable) {
        gui_set_label_color(GUI_LABEL_RUN, 
            GUI_FONT_COLOR_GREEN, GUI_TEXT_BG_COLOR);
    }
    else {
        gui_set_label_color(GUI_LABEL_RUN, 
            GUI_FONT_COLOR_GREEN_DIM, GUI_TEXT_BG_COLOR);    
    }
}

// update the rec indicator state
void gui_update_rec_mode(int mode) {
    switch(mode) {
        case SEQ_CTRL_RECORD_ARM:
            gui_set_label_color(GUI_LABEL_REC,
                GUI_FONT_COLOR_RED, GUI_TEXT_BG_COLOR);
            break;
        case SEQ_CTRL_RECORD_STEP:
            gui_set_label_color(GUI_LABEL_REC,
                GUI_FONT_COLOR_RED, GUI_TEXT_BG_COLOR);
            break;
        case SEQ_CTRL_RECORD_RT:
            gui_set_label_color(GUI_LABEL_REC,
                GUI_FONT_COLOR_RED, GUI_TEXT_BG_COLOR);
            break;    
        case SEQ_CTRL_RECORD_IDLE:
        default:
            gui_set_label_color(GUI_LABEL_REC,
                GUI_FONT_COLOR_RED_DIM, GUI_TEXT_BG_COLOR);    
            break;
    }
}

// update the clock source mode
void gui_update_clock_source(int source) {
    switch(source) {
        case CLOCK_EXTERNAL:
            gui_set_label(GUI_LABEL_CLKSRC, "EXT");
            gui_set_label_color(GUI_LABEL_TEMPO,
                GUI_FONT_COLOR_DARK_GREY, GUI_TEXT_BG_COLOR);    
            break;
        case CLOCK_INTERNAL:
            gui_set_label(GUI_LABEL_CLKSRC, "INT");
            gui_set_label_color(GUI_LABEL_TEMPO,
                GUI_FONT_COLOR_NORMAL, GUI_TEXT_BG_COLOR);    
            break;
        default:
            break;
    }
}

// update the live indicator state
void gui_update_live_mode(int mode) {
    int live;
    int kbtrans;

    switch(mode) {
        case SEQ_CTRL_LIVE_ON:
            live = 1;
            kbtrans = 0;
            break;
        case SEQ_CTRL_LIVE_KBTRANS:
            live = 0;
            kbtrans = 1;
            break;
        case SEQ_CTRL_LIVE_OFF:
        default:
            live = 0;
            kbtrans = 0;
            break;
    }

    if(live) {
        gui_set_label_color(GUI_LABEL_LIVE,
            GUI_FONT_COLOR_CYAN, GUI_TEXT_BG_COLOR);
    }
    else {
        gui_set_label_color(GUI_LABEL_LIVE,
            GUI_FONT_COLOR_CYAN_DIM, GUI_TEXT_BG_COLOR);
    }
    if(kbtrans) {
        gui_set_label_color(GUI_LABEL_KEYTRANS,
            GUI_FONT_COLOR_MAGENTA, GUI_TEXT_BG_COLOR);
    }
    else {
        gui_set_label_color(GUI_LABEL_KEYTRANS,
            GUI_FONT_COLOR_MAGENTA_DIM, GUI_TEXT_BG_COLOR);
    }
}

// update the current keyboard transpose
void gui_update_keyboard_transpose(int transpose) {
    if(transpose == 0) {
        gui_set_label(GUI_LABEL_KEYTRANS, "KB: 0");
    }
    else {
        sprintf(tempstr, "KB:%+-2d", transpose);
        gui_set_label(GUI_LABEL_KEYTRANS, tempstr);    
    }
}

// update the track mute state
void gui_update_track_mute(int scene, int track, int mute) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gutm - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gutm - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // store mute data for preview displays
    if(mute) {
        gstate.track_mute[track] = 1;
    }
    else {
        gstate.track_mute[track] = 0;    
    }
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        if(gstate.track_mute[track]) {
            gui_set_status_text_part(0, 13, 4, "Mute");
        }
        else {
            gui_set_status_text_part(0, 13, 4, "  On");
        }
    }
}

// uopdate the bias track
void gui_update_bias_track(int scene, int track, int bias_track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gubt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gubt - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        if(bias_track == SONG_TRACK_BIAS_NULL || bias_track == track) {
            gui_set_status_text_part(0, 8, 3, " <X");
        }
        else {
            sprintf(tempstr, " <%d", (bias_track + 1));
            gui_set_status_text_part(0, 8, 3, tempstr);
        }
    }
}

// update the track transpose
void gui_update_track_transpose(int scene, int track, int transpose) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gutt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gutt - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // we only care about the first track
    if(track != gstate.first_track) {
        return;
    }
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        gui_set_status_text_part(0, 19, 6, "Tran: ");
        panel_utils_transpose_to_str(tempstr, transpose);
        gui_set_status_text_part(0, 25, 3, tempstr);
    }    
}

// update the track arp enable state
void gui_update_arp_enable(int scene, int track, int enable) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("guae - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("guae - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // store for preview displays
    if(enable) {
        gstate.arp_enable[track] = 1;
    }
    else {
        gstate.arp_enable[track] = 0;
    }
}

// update the tonality display
void gui_update_tonality(int scene, int track, int tonality) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gut - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gut - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // we only care about the first track
    if(track != gstate.first_track) {
        return;
    }
    // update info display
    if(!gstate.status_override) {
        gui_set_status_text_part(3, 0, 6, "Tona: ");
        scale_type_to_name(tempstr, 
            song_get_tonality(seq_ctrl_get_scene(), gstate.first_track));
        gui_set_status_text_part(3, 7, 10, tempstr);
    }    
}

// update the start position of a track
void gui_update_motion_start(int scene, int track, int pos) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gums - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gums - track invalid: %d", track);
        return;
    }
    if(pos < 0 || pos >= SEQ_NUM_STEPS) {
        log_error("gums - pos invalid: %d", pos);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // store data for preview displays
    gstate.motion_start[track] = pos;
    // update the info display
    if(!gstate.status_override && track == gstate.first_track) {
        sprintf(tempstr, "Start: %d", (gstate.motion_start[track] + 1));
        gui_set_status_text_part(1, 0, 9, tempstr);
    }    
    // cause the motion steps to be recalculated
    gui_calculate_motion_steps(track);
}

// update the end position of a track
void gui_update_motion_length(int scene, int track, int length) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("guml - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("guml - track invalid: %d", track);
        return;
    }
    if(length < 1 || length > SEQ_NUM_STEPS) {
        log_error("guml - length invalid: %d", length);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // store data for preview displays
    gstate.motion_length[track] = length;
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        sprintf(tempstr, "Len: %d", gstate.motion_length[track]);
        gui_set_status_text_part(1, 10, 7, tempstr);
    }    
    // cause the motion steps to be recalculated
    gui_calculate_motion_steps(track);
}

// update the motion direction for a track
void gui_update_motion_dir(int scene, int track, int rev) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gumd - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gumd - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // we only care about the first track
    if(track != gstate.first_track) {
        return;
    }    
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        if(rev) {
            gui_set_status_text_part(1, 19, 3, "REV");
        }
        else {
            gui_set_status_text_part(1, 19, 3, "FWD");        
        }
    }        
}

// update the step length for a track
void gui_update_step_length(int scene, int track, int len) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gusl - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gusl - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // we only care about the first track
    if(track != gstate.first_track) {
        return;
    }
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        gui_set_status_text_part(2, 0, 6, "Step: ");
        panel_utils_step_len_to_str(tempstr, len);
        gui_set_status_text_part(2, 7, 6, tempstr);
    }    
}

// update the gate time override - 0-255 = 0-200%
void gui_update_gate_time_override(int scene, int track, int time) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gugto - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gugto - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // we only care about the first track
    if(track != gstate.first_track) {
        return;
    }    
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        sprintf(tempstr, "Gate: %d%%", (time * 200 / 256));
        gui_set_status_text_part(2, 13, 10, tempstr);
    }    
}

// update the pattern type for a track
void gui_update_pattern_type(int scene, int track, int pattern) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gupt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gupt - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // store data for preview displays
    gstate.pattern_type[track] = pattern;
}

// update the track type of a track
void gui_update_track_type(int scene, int track, int type) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("gutt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("gutt - track invalid: %d", track);
        return;
    }
    // ignore scenes other than ours
    if(scene != gstate.current_scene) {
        return;
    }
    // we only care about the first track
    if(track != gstate.first_track) {
        return;
    }
    // update info display
    if(!gstate.status_override && track == gstate.first_track) {
        if(type == SONG_TRACK_TYPE_DRUM) {
            gui_set_status_text_part(3, 19, 10, "DRUM");
        }
        else {
            gui_set_status_text_part(3, 19, 10, "VOICE");        
        }
    }
}

// update the currently active step or -1 to disable
void gui_update_active_step(int track, int step) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("guas - track invalid: %d", track);
        return;
    }
    // store data for preview displays
    gstate.active_step[track] = step;
}

// update the song mode status display
void gui_update_song_mode(void) {
    struct song_mode_state *sngmode;
    sngmode = seq_engine_get_song_mode_state();

    if(seq_ctrl_get_song_mode()) {
        gui_set_label_color(GUI_LABEL_SONG_MODE,
            GUI_FONT_COLOR_YELLOW, GUI_TEXT_BG_COLOR);
    }
    else {
        gui_set_label_color(GUI_LABEL_SONG_MODE,
            GUI_FONT_COLOR_YELLOW_DIM, GUI_TEXT_BG_COLOR);
    }
    // display the status bar - song mode disabled
    if(sngmode->current_entry == -1 || !seq_ctrl_get_song_mode()) {
        sprintf(tempstr, "SONG - Slot: x: x/x  ");
    }
    // display the status bar - song mode enabled
    else {
        sprintf(tempstr, "SONG - Slot: %d: %d/%d  ",
            (sngmode->current_entry + 1), 
            seq_utils_clamp(sngmode->beat_count, 
                SEQ_SONG_LIST_MIN_LENGTH,
                SEQ_SONG_LIST_MAX_LENGTH),          
            sngmode->total_beats);
    }
    gui_set_label(GUI_LABEL_SONG_MODE, tempstr);
}

