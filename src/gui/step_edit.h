/*
 * CARBON Step Editor
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
#ifndef STEP_EDIT_H
#define STEP_EDIT_H

#include "../midi/midi_utils.h"
#include <inttypes.h>

// init the step edit mode
void step_edit_init(void);

// run step edit timer task
void step_edit_timer_task(void);

// the clock ticked - do note timeouts for preview
void step_edit_run(uint32_t tick_count);

// handle state change
void step_edit_handle_state_change(int event_type, int *data, int data_len);

// handle a input from the keyboard
void step_edit_handle_input(struct midi_msg *msg);

// get whether step edit mode is enabled
int step_edit_get_enable(void);

// start and stop step edit mode
void step_edit_set_enable(int enable);

// adjust the cursor position
void step_edit_adjust_cursor(int change, int shift);

// adjust the step note
void step_edit_adjust_note(int change, int shift);

// adjust the step velocity
void step_edit_adjust_velocity(int change, int shift);

// adjust the step gate time
void step_edit_adjust_gate_time(int change, int shift);

// adjust the start delay
void step_edit_adjust_start_delay(int change, int shift);

// adjust the ratchet mode
void step_edit_adjust_ratchet_mode(int change, int shift);

// adjust the event probability
void step_edit_adjust_probability(int change, int shift);

// clear the current step
void step_edit_clear_step(void);

#endif

