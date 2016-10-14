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
#ifndef GUI_H
#define GUI_H

#include "../config.h"
#include <inttypes.h>

// GUI overlay colors
#define GUI_OVERLAY_BLANK 0
#define GUI_OVERLAY_LOW 1
#define GUI_OVERLAY_MED 2
#define GUI_OVERLAY_HIGH 3

// init the GUI
int gui_init(void);

// close the GUI
void gui_close(void);

// run the refresh task - run on the main polling loop
void gui_refresh_task(void);

// force all GUI items to refresh
void gui_force_refresh(void);

// clear the screen
void gui_clear_screen(void);

// enable or disable the entire GUI so we can use the screen ourselves
void gui_set_enable(int enable);

// set the LCD power state 
// schedule a power-up and reinit (power on)
// turns off power immediately (power off)
void gui_set_lcd_power(int state);

// enable or disable the grid overlay
void gui_grid_set_overlay_enable(int enable);

// clear the grid overlay
void gui_grid_clear_overlay(void);

// set a grid overlay color index
void gui_grid_set_overlay_color(int step, int index);

//
// status display helpers
//
// set the status / menu override state
void gui_set_status_override(int enable);

// clear all status text
void gui_clear_status_text_all(void);

// clear a line of status text
void gui_clear_status_text(int line);

// set a line of status text
void gui_set_status_text(int line, char *text);

// set a portion of a status line
// will clear the range and truncate the line if it's too long
void gui_set_status_text_part(int line, int startch, int lench, char *text);

// set the highlight of a status line
void gui_set_status_highlight_part(int line, int startch, 
    int lench, int mode);

//
// menu control
//
// clear the menu
void gui_clear_menu(void);

// set the menu title
void gui_set_menu_title(char *text);

// set the menu next/prev
void gui_set_menu_prev_next(int prev, int next);

// set the menu subtitle
void gui_set_menu_subtitle(char *text);

// set the menu param
void gui_set_menu_param(char *text);

// set the menu value
void gui_set_menu_value(char *text);

// set the edit mode on the menu - highlight the field
void gui_set_menu_edit(int edit);

#endif

