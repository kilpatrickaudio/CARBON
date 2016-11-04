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
#ifndef SEQ_UTILS_H
#define SEQ_UTILS_H

// step length
#define SEQ_UTILS_STEP_LENS 17  // enumerated
#define SEQ_UTILS_STEP_32ND_T 0
#define SEQ_UTILS_STEP_32ND 1
#define SEQ_UTILS_STEP_16TH_T 2
#define SEQ_UTILS_STEP_DOT_32ND 3
#define SEQ_UTILS_STEP_16TH 4
#define SEQ_UTILS_STEP_8TH_T 5
#define SEQ_UTILS_STEP_DOT_16TH 6
#define SEQ_UTILS_STEP_8TH 7
#define SEQ_UTILS_STEP_QUARTER_T 8
#define SEQ_UTILS_STEP_DOT_8TH 9
#define SEQ_UTILS_STEP_QUARTER 10
#define SEQ_UTILS_STEP_HALF_T 11
#define SEQ_UTILS_STEP_DOT_QUARTER 12
#define SEQ_UTILS_STEP_HALF 13
#define SEQ_UTILS_STEP_WHOLE_T 14
#define SEQ_UTILS_STEP_DOT_HALF 15
#define SEQ_UTILS_STEP_WHOLE 16

// clock PPQ
#define SEQ_UTILS_CLOCK_PPQS 9  // enumerated
#define SEQ_UTILS_CLOCK_OFF 0
#define SEQ_UTILS_CLOCK_1PPQ 1
#define SEQ_UTILS_CLOCK_2PPQ 2
#define SEQ_UTILS_CLOCK_3PPQ 3
#define SEQ_UTILS_CLOCK_4PPQ 4
#define SEQ_UTILS_CLOCK_6PPQ 5
#define SEQ_UTILS_CLOCK_8PPQ 6
#define SEQ_UTILS_CLOCK_12PPQ 7
#define SEQ_UTILS_CLOCK_24PPQ 8

// convert a MIDI-style encoder value to a change amount
// input is 0x01-0x3f for up, and 0x7f down to 0x40 for down
int seq_utils_enc_val_to_change(int val);

// clamp the value to a range
int seq_utils_clamp(int val, int min, int max);

// wrap a value within a range
int seq_utils_wrap(int val, int min, int max);

// convert a bit position into a count (first bit encountered is returned)
int seq_utils_bits_to_count(int bits);

// check if a step is within a motion start / length range
int seq_utils_is_step_active(int step, int motion_start, 
        int motion_len, int num_steps);

// convert a step length to a number of ticks
int seq_utils_step_len_to_ticks(int speed);

// remap a step len for ver <= 1.02 to the new step len entry
// this adds support for triplets
int seq_utils_remap_step_len_102(int oldspeed);

// warp a change so that the change is scaled based on the oldval
// returns the change to be added to the oldval externally
int seq_utils_warp_change(int oldval, int change, int divisor);

// convert a clock PPQ to a divisor based on the master clock
// returns 0 if the clock is disabled or PPQ is invalid
int seq_utils_clock_pqq_to_divisor(int ppq);

// check to see if pos is within the start + length range within total_len
int seq_utils_get_wrapped_range(int pos, int start, int length, int total_len);

#endif
