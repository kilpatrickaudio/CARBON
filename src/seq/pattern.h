/*
 * CARBON Sequencer Pattern Lookup
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
#ifndef PATTERN_H
#define PATTERN_H

// pattern defines
#define PATTERN_AS_RECORDED 31

// init the pattern lookup
void pattern_init(void);

// handle state change
void pattern_handle_state_change(int event_type,
    int *data, int data_len);

// load the patterns from the config store
void pattern_load_patterns(void);

// restore a pattern from ROM
void pattern_restore_pattern(int pattern);

// check whether the current step is enabled on a pattern
int pattern_get_step_enable(int scene, int track, int pattern, int step);

// adjust the step enable for a pattern - PATTERN_AS_RECORDED is read-only
void pattern_set_step_enable(int pattern, int step, int enable);

#endif
