/*
 * CARBON Sequencer Analog Outputs
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
#ifndef ANALOG_OUT_H
#define ANALOG_OUT_H

// init the analog outs
void analog_out_init(void);

// run the analog out timer task
void analog_out_timer_task(void);

// set CV output value - chan: 0-3 = CV 1-4, val: 12 bit
void analog_out_set_cv(int chan, int val);

// set a gate output - chan: 0-3 = GATE 1-4, state: 0 = off, 1 = on
void analog_out_set_gate(int chan, int state);

// set the clock output
void analog_out_set_clock(int state);

// set the reset output
void analog_out_set_reset(int state);

// beep the metronome
void analog_out_beep_metronome(int enable);

#endif

