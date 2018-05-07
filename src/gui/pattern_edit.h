/*
 * CARBON Pattern Editor
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2018: Kilpatrick Audio
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
#ifndef PATTERN_EDIT_H
#define PATTERN_EDIT_H

#include <inttypes.h>

// init the step edit mode
void pattern_edit_init(void);

// run step edit timer task
void pattern_edit_timer_task(void);

// handle state change
void pattern_edit_handle_state_change(int event_type, int *data, int data_len);

// get whether step edit mode is enabled
int pattern_edit_get_enable(void);

// start and stop step edit mode
void pattern_edit_set_enable(int enable);

// adjust the cursor position
void pattern_edit_adjust_cursor(int change, int shift);

// adjust the enable state
void pattern_edit_adjust_step(int change, int shift);

// restore the current pattern from ROM
void pattern_edit_restore_pattern(void);

#endif
