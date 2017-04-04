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
    (2 * MIDI_CLOCK_UPSAMPLE),      // 32nd T       - xxx       - new 0
    (3 * MIDI_CLOCK_UPSAMPLE),      // 32nd         - old 0     - new 1
    (4 * MIDI_CLOCK_UPSAMPLE),      // 16th T       - xxx       - new 2
    (4.5 * MIDI_CLOCK_UPSAMPLE),    // .32nd        - old 1     - new 3
    (6 * MIDI_CLOCK_UPSAMPLE),      // 16th         - old 2     - new 4
    (8 * MIDI_CLOCK_UPSAMPLE),      // 8th T        - xxx       - new 5
    (9 * MIDI_CLOCK_UPSAMPLE),      // .16th        - old 3     - new 6
    (12 * MIDI_CLOCK_UPSAMPLE),     // 8th          - old 4     - new 7
    (16 * MIDI_CLOCK_UPSAMPLE),     // quarter T    - xxx       - new 8
    (18 * MIDI_CLOCK_UPSAMPLE),     // .8th         - old 5     - new 9
    (24 * MIDI_CLOCK_UPSAMPLE),     // quarter      - old 6     - new 10
    (32 * MIDI_CLOCK_UPSAMPLE),     // half T       - xxx       - new 11
    (36 * MIDI_CLOCK_UPSAMPLE),     // .quarter     - old 7     - new 12
    (48 * MIDI_CLOCK_UPSAMPLE),     // half         - old 8     - new 13
    (64 * MIDI_CLOCK_UPSAMPLE),     // whole T      - xxx       - new 14
    (72 * MIDI_CLOCK_UPSAMPLE),     // .half        - old 9     - new 15
    (96 * MIDI_CLOCK_UPSAMPLE)      // whole        - old 10    - new 16
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

// remap a step len for ver <= 1.02 to the new step len entry
// this adds support for triplets
int seq_utils_remap_step_len_102(int oldspeed) {
    switch(oldspeed) {
        case 0:  // old 32nd
            return 1;
        case 1:  // old .32nd
            return 3;
        case 2:  // old 16th
            return 4;
        case 3:  // old .16th
            return 6;
        case 4:  // old 8th
            return 7;
        case 5:  // old .8th
            return 9;
        case 6:  // old quarter
            return 10;
        case 7:  // old .quarter
            return 12;
        case 8:  // old half
            return 13;
        case 9:  // old .half
            return 15;
        case 10:  // old whole
            return 16;
        default:
            return 4;  // new 16th
    }
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
            return (MIDI_CLOCK_PPQ / 1);
        case SEQ_UTILS_CLOCK_2PPQ:
            return (MIDI_CLOCK_PPQ / 2);
        case SEQ_UTILS_CLOCK_3PPQ:
            return (MIDI_CLOCK_PPQ / 3);
        case SEQ_UTILS_CLOCK_4PPQ:
            return (MIDI_CLOCK_PPQ / 4);
        case SEQ_UTILS_CLOCK_6PPQ:
            return (MIDI_CLOCK_PPQ / 6);
        case SEQ_UTILS_CLOCK_8PPQ:
            return (MIDI_CLOCK_PPQ / 8);
        case SEQ_UTILS_CLOCK_12PPQ:
            return (MIDI_CLOCK_PPQ / 12);
        case SEQ_UTILS_CLOCK_24PPQ:
            return (MIDI_CLOCK_PPQ / 24);
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

// check if a note is within the supported range - returns 1 on success, 0 on error
int seq_utils_check_note_range(int note) {
    if(note < 0 || note > 127) {
        return 0;
    }
    return 1;
}

