/*
 * CARBON Step Editor
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
#ifndef SONG_EDIT_H
#define SONG_EDIT_H

// init the song edit mode
void song_edit_init(void);

// run the song edit timer task
void song_edit_timer_task(void);

// get whether song edit mode is enabled
int song_edit_get_enable(void);

// start and stop song edit mode
void song_edit_set_enable(int enable);

// adjust the cursor position
void song_edit_adjust_cursor(int change, int shift);

// adjust the scene
void song_edit_adjust_scene(int change, int shift);

// adjust the length
void song_edit_adjust_length(int change, int shift);

// adjust the kb trans
void song_edit_adjust_kbtrans(int change, int shift);

// clear the current step
void song_edit_remove_step(void);

#endif

