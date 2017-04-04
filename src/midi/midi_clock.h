/*
 * MIDI Clock with Internal and External Sync
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2017: Kilpatrick Audio
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
#ifndef MIDI_CLOCK_H
#define MIDI_CLOCK_H

#include <inttypes.h>
#include "midi_utils.h"

// make weak references work
#ifndef __weak
#define __weak   __attribute__((weak))
#endif

// clock source
#define MIDI_CLOCK_EXTERNAL 0
#define MIDI_CLOCK_INTERNAL 1

// init the MIDI clock
void midi_clock_init(void);

// run the MIDI clock timer task
// call at MIDI_CLOCK_TASK_INTERVAL_US interval
void midi_clock_timer_task(void);

// get the MIDI clock source
int midi_clock_get_source(void);

// set the MIDI clock source
int midi_clock_set_source(int source);

// get the clock tempo (internal clock)
float midi_clock_get_tempo(void);

// set the clock tempo (internal clock)
void midi_clock_set_tempo(float tempo);

// get the clock swing
int midi_clock_get_swing(void);

// set the clock swing
void midi_clock_set_swing(int swing);

// handle tap tempo
void midi_clock_tap_tempo(void);

// handle a request to continue playback
void midi_clock_request_continue(void);

// handle a request to stop playback
void midi_clock_request_stop(void);

// handle a request to reset the playback position
void midi_clock_request_reset_pos(void);

// get the current clock tick position
uint32_t midi_clock_get_tick_pos(void);

// get the running state of the clock
int midi_clock_get_running(void);

//
// external clock inputs
//
// a MIDI tick was received
void midi_clock_midi_rx_tick(void);

// a MIDI clock start was received
void midi_clock_midi_rx_start(void);

// a MIDI clock continue was received
void midi_clock_midi_rx_continue(void);

// a MIDI clock stop was received
void midi_clock_midi_rx_stop(void);

//
// callbacks - these should be overriden if needed
//
// a beat was crossed
void midi_clock_beat_crossed(void);

// the run state changed
void midi_clock_run_state_changed(int running);

// the clock source changed
void midi_clock_source_changed(int source);

// the tap tempo locked
void midi_clock_tap_locked(void);

// the clock ticked
void midi_clock_ticked(uint32_t tick_count);

// the clock position was reset
void midi_clock_pos_reset(void);

#endif
