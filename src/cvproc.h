/*
 * CARBON Sequencer CV/Gate Processor
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
#ifndef CVPROC_H
#define CVPROC_H

// CV/gate pairs
#define CVPROC_PAIRS_ABCD 0
#define CVPROC_PAIRS_AABC 1
#define CVPROC_PAIRS_AABB 2
#define CVPROC_PAIRS_AAAA 3
// CV/gate pair
#define CVPROC_PAIR_A 0
#define CVPROC_PAIR_B 1
#define CVPROC_PAIR_C 2
#define CVPROC_PAIR_D 3
// CV/gate output modes
#define CVPROC_MODE_VELO -2
#define CVPROC_MODE_NOTE -1
#define CVPROC_MODE_MAX 120
// defaults
#define CVPROC_DEFAULT_NOTE 60
// CV output scaling modes
//#define CVPROC_CV_SCALING_MAX 2
#define CVPROC_CV_SCALING_MAX 1
#define CVPROC_CV_SCALING_1VOCT 0  // 1V/octave
#define CVPROC_CV_SCALING_1P2VOCT 1  // 1.2V/octave
#define CVPROC_CV_SCALING_HZ_V 2  // Hz per volt - XXX unsupported

// init the CV/gate processor
void cvproc_init(void);

// run the timer task
void cvproc_timer_task(void);

// set the CV/gate processor pairing mode
void cvproc_set_pairs(int pairs);

// set the CV/gate pair mode - pair 0-3 = A-D
void cvproc_set_pair_mode(int pair, int mode);

// set the CV bend range for pitch bend
void cvproc_set_bend_range(int range);

// set the CV output scaling for an output (0-3 = A-D)
void cvproc_set_output_scaling(int out, int mode);

// set the scale of an output
void cvproc_set_cvcal(int out, int scale);

// set the offset for an output
void cvproc_set_cvoffset(int out, int offset);

// set the gate delay for an output
void cvproc_set_cvgatedelay(int out, int delay);

#endif


