/*
 * Switch Filter
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2012: Kilpatrick Audio
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
#ifndef SWITCH_FILTER_H
#define SWITCH_FILTER_H

#include <inttypes.h>

// switch filter
#define SW_NUM_INPUTS 32

// switch change flags
#define SW_CHANGE_UNPRESSED 0x1000
#define SW_CHANGE_PRESSED 0x2000
#define SW_CHANGE_ENC_MOVE_CW 0x3000
#define SW_CHANGE_ENC_MOVE_CCW 0x4000

// initialize the filter code
void switch_filter_init(uint16_t sw_timeout, 
		uint16_t sw_debounce, uint16_t enc_timeout);

// set a pair of channels an an encoder - must be sequential 
void switch_filter_set_encoder(uint16_t start_chan);

// record a new sample value
void switch_filter_set_val(uint16_t start_chan, 
		uint16_t num_chans, int states);

// get the next event in the queue
int switch_filter_get_event(void);

#endif
