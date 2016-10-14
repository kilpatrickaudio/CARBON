/*
 * CARBON Sequencer Clock Output Module
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
#ifndef CLOCK_OUT_H
#define CLOCK_OUT_H

#include <inttypes.h>

// init the clock output module
void clock_out_init(void);

// realtime tasks for the clock output
void clock_out_timer_task(void);

// run the clock output - called on each clock tick
void clock_out_run(uint32_t tick_count);

// handle state change
void clock_out_handle_state_change(int event_type, int *data, int data_len);

#endif
