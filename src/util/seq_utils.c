/*
 * CARBON Sequencer Utils
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
#include "seq_utils.h"
#include "../config.h"
#include <stdio.h>

// step length in ticks
const int seq_utils_step_size[] = {
    (3 * CLOCK_MIDI_UPSAMPLE), 
    (4.5 * CLOCK_MIDI_UPSAMPLE), 
    (6 * CLOCK_MIDI_UPSAMPLE), 
    (9 * CLOCK_MIDI_UPSAMPLE), 
    (12 * CLOCK_MIDI_UPSAMPLE), 
    (18 * CLOCK_MIDI_UPSAMPLE), 
    (24 * CLOCK_MIDI_UPSAMPLE), 
    (32 * CLOCK_MIDI_UPSAMPLE), 
    (48 * CLOCK_MIDI_UPSAMPLE), 
    (72 * CLOCK_MIDI_UPSAMPLE), 
    (96 * CLOCK_MIDI_UPSAMPLE)
};

// convert a MIDI-style encoder value to a change amount
// input is 0x01-0x3f for up, and 0x7f down to 0x40 for down
int seq_utils_enc_val_to_change(int val) {
    if(val >= 0x01 && val <= 0x3f) {
        return val;
    }
    else if(val >= 0x40 && val <= 0x7f) {
        return -(0x80 - val);
    }
    return 0;
}

// clamp the value to a range
int seq_utils_clamp(int val, int min, int max) {
    if(val < min) {
        return min;
    }
    if(val > max) {
        return max;
    }
    return val;
}

// wrap a value within a range
int seq_utils_wrap(int val, int min, int max) {
    if(val < min) {
        return max;
    }
    if(val > max) {
        return min;
    }
    return val;
}

// convert a bit position into a count (first bit encountered is returned)
// returns -1 if no bits are set
int seq_utils_bits_to_count(int bits) {
    int i;
    int temp = bits;
    for(i = 0; i < 32; i ++) {
        if(temp & 0x01) {
            return i;
        }
        temp = temp >> 1;
    }
    return -1;
}

// check if a step is within a motion start / length range
int seq_utils_is_step_active(int step, int motion_start, 
        int motion_len, int num_steps) {
    if(((step - motion_start) & (num_steps - 1)) < motion_len) {
        return 1;
    }
    return 0;
}

// convert a step length to a number of ticks - returns 0 on error
int seq_utils_step_len_to_ticks(int speed) {
    if(speed < 0 || speed >= SEQ_UTILS_STEP_LENS) {
        return 0;
    }
    return seq_utils_step_size[speed];
}

// warp a change so that the change is scaled to be a percentage of oldval
// returns the change to be added to the oldval externally
int seq_utils_warp_change(int oldval, int change, int divisor) {
    return ((oldval / divisor) + 1) * change;
}

// convert a clock PPQ to a divisor based on the master clock
// returns 0 if the clock is disabled or PPQ is invalid
int seq_utils_clock_pqq_to_divisor(int ppq) {
    switch(ppq) {
        case SEQ_UTILS_CLOCK_1PPQ:
            return (CLOCK_PPQ / 1);
        case SEQ_UTILS_CLOCK_2PPQ:
            return (CLOCK_PPQ / 2);
        case SEQ_UTILS_CLOCK_3PPQ:
            return (CLOCK_PPQ / 3);
        case SEQ_UTILS_CLOCK_4PPQ:
            return (CLOCK_PPQ / 4);
        case SEQ_UTILS_CLOCK_6PPQ:
            return (CLOCK_PPQ / 6);
        case SEQ_UTILS_CLOCK_8PPQ:
            return (CLOCK_PPQ / 8);
        case SEQ_UTILS_CLOCK_12PPQ:
            return (CLOCK_PPQ / 12);
        case SEQ_UTILS_CLOCK_24PPQ:
            return (CLOCK_PPQ / 24);
        default:
            return 0;
    }
}

// check to see if pos is within the start + length range within total_len
int seq_utils_get_wrapped_range(int pos, int start, int length, int total_len) {
    int offset = (pos - start) % total_len;
    if(offset < 0) {
        offset += total_len;
    }

    if((offset % total_len) < length) {
        return 1;
    }
    return 0;
}


