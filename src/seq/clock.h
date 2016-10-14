/*
 * CARBON Sequencer Clock
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
#ifndef CLOCK_H
#define CLOCK_H

#include <inttypes.h>
#include "../midi/midi_utils.h"

// clock source
#define CLOCK_EXTERNAL 0
#define CLOCK_INTERNAL 1

// init the MIDI clock
void clock_init(void);

// run the MIDI clock timer task - 1000us interval
void clock_timer_task(void);

// get the MIDI clock source
int clock_get_source(void);

// get the clock tempo (internal clock)
float clock_get_tempo(void);

// set the clock tempo (internal clock)
void clock_set_tempo(float tempo);

// set the clock swing
void clock_set_swing(int swing);

// handle tap tempo
void clock_tap_tempo(void);

// reset the clock to the start
void clock_reset_pos(void);

// get the current clock tick position
uint32_t clock_get_tick_pos(void);

// get the running state of the clock
int clock_get_running(void);

// start and stop the clock
void clock_set_running(int running);

//
// external clock inputs
//
// a MIDI tick was received
void clock_midi_rx_tick(void);

#endif
