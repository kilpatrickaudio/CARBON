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
#include "clock.h"
#include "seq_ctrl.h"
#include "../tables/swing_table.h"
#include "../config.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"

// setings
#define CLOCK_US_PER_TICK_MAX ((uint64_t)(60000000.0 / (CLOCK_TEMPO_MIN * (float)CLOCK_PPQ)))
#define CLOCK_US_PER_TICK_MIN ((uint64_t)(60000000.0 / (CLOCK_TEMPO_MAX * (float)CLOCK_PPQ)))
#define CLOCK_LOCK_ADJUST 500  // lock adjust amount

// state
struct clock_state {
    // state
    int source;   // CLOCK_EXTERNAL or CLOCK_INTERNAL
    int desired_run_state;  // 0 = stop, 1 = run
    int run_state;  // 0 = stopped, 1 = running
    int next_swing;  // clock swing to change to on the next beat
    int current_swing;  // clock swing (0-30 = 50-80%)
    //
    // general clock state
    //
    uint64_t time_count;  // running time count
    uint64_t next_tick_time;  // time for the next tick
    //
    // internal clock state
    //
    int32_t run_tick_count;  // running tick count
    int32_t stop_tick_count;   // stopped tick count
    int32_t int_us_per_tick;  // number of us per tick (internal)
    //
    // external clock state
    //
    int32_t ext_us_per_tick;  // number of us per tick (external)
    int32_t ext_generate_tick_count;  // generated clock ticks (for error adjustment)
    int32_t ext_generate_run_tick_pos;  // generated running tick pos
    // clock RX flags
    int ext_tickf;  // external tick received flag
    int ext_startf;  // external start received flag
    int ext_stopf;  // external stop received flag
    int ext_continuef;  // external continue received flag
    // recovery state
    uint64_t ext_recover_last_tick;  // last tick time - for tick period measurement
    int32_t ext_recover_run_tick_pos;  // recovered running tick pos
    int32_t ext_recover_tick_count;  // a count of ticks received
    int32_t ext_recover_hist_pos;  // the position in the history buffer
    int32_t ext_recover_hist[CLOCK_EXTERNAL_HIST_LEN];  // historical periods for averaging
    //
    // tap tempo state
    //
    int tap_beatf;  // tap tempo beat was received
    uint64_t tap_clock_last_tap;  // last tap time
    int tap_clock_period;  // tap clock period - averaged
    int tap_hist_count;  // number of historical taps
    uint64_t tap_hist[CLOCK_TAP_HIST_LEN];  // historical taps
};
struct clock_state clks;

// local functions
void clock_set_source(int source);

// init the clock
void clock_init(void) {
    clock_set_source(CLOCK_INTERNAL);
    clock_set_running(0);
    clock_reset_pos();
    clock_set_tempo(CLOCK_DEFAULT_TEMPO);
    clock_set_swing(50);
    clks.run_state = 0;
    clks.time_count = 0;
    clks.next_tick_time = 0;
    clks.ext_tickf = 0;
    clks.ext_startf = 0;
    clks.ext_stopf = 0;
    clks.ext_continuef = 0;
    clks.ext_recover_last_tick = 0;
    clks.ext_recover_tick_count = 0;
    clks.ext_recover_hist_pos = 0;
    clks.tap_beatf = 0;
    clks.tap_clock_last_tap = 0;
    clks.tap_clock_period = 0;
    clks.tap_hist_count = 0;
}

// run the clock timer task - 1000us interval
void clock_timer_task(void) {
    int i;
    uint32_t tick_count;
    int32_t temp;
    int error = 0;

    //
    // run clock timebase
    //
    clks.time_count += SEQ_TASK_INTERVAL_US;
    // internal clock
    if(clks.source == CLOCK_INTERNAL) {
        // decide if we should issue a clock
        if(clks.time_count > clks.next_tick_time) {
            // if run state changed
            if(clks.run_state != clks.desired_run_state) {
                clks.run_state = clks.desired_run_state;
                // stopping
                if(clks.run_state == 0) {
                    clks.stop_tick_count = clks.run_tick_count;
                }
            }
            // get the correct tick count
            if(clks.run_state) {
                tick_count = clks.run_tick_count;
            }
            else {
                tick_count = clks.stop_tick_count;
            }
            // calculate beat cross before processing sequencer stuff
            if((tick_count % CLOCK_PPQ) == 0) {
                // if the swing is adjusting we change it now
                if(clks.current_swing != clks.next_swing) {
                    clks.current_swing = clks.next_swing;
                }
                // fire event
                state_change_fire0(SCE_CLK_BEAT);
            }            
            // generate correct number of pulses for current swing mode
            for(i = 0; i < swing[clks.current_swing][tick_count % CLOCK_PPQ]; i ++) {
                seq_ctrl_clock_tick(tick_count);
            }
            tick_count ++;
            clks.next_tick_time += clks.int_us_per_tick;
            // write back the tick count
            if(clks.run_state) {
                clks.run_tick_count = tick_count;
            }
            else {
                clks.stop_tick_count = tick_count;
            }
        }
    }
    // external clock
    else {
        // decide if we should issue a clock
        if(clks.time_count > clks.next_tick_time) {
            // generate correct number of pulses for current swing mode
            for(i = 0; i < swing[clks.current_swing][clks.ext_generate_run_tick_pos % CLOCK_PPQ]; i ++) {
                if(clks.run_state) {
                    seq_ctrl_clock_tick(clks.ext_generate_run_tick_pos);
                }
                else {
                    seq_ctrl_clock_tick(clks.ext_generate_tick_count);
                }
            }
            // running
            if(clks.run_state) {
                // calculate beat cross before processing sequencer stuff
                if((clks.ext_generate_run_tick_pos % CLOCK_PPQ) == 0) {
                    // if the swing is adjusting we change it now
                    if(clks.current_swing != clks.next_swing) {
                        clks.current_swing = clks.next_swing;
                    }
                    // fire event
                    state_change_fire0(SCE_CLK_BEAT);
                }            
                clks.ext_generate_run_tick_pos ++;
                // calculate error
                error = clks.ext_recover_run_tick_pos - clks.ext_generate_run_tick_pos;
            }
            // stopped
            else {
                clks.ext_generate_tick_count ++;
                // calculate error
                error = clks.ext_recover_tick_count - clks.ext_generate_tick_count;
            }

            // calculate next tick time and adjust for error
            if(error > 0) {
                clks.next_tick_time += clks.ext_us_per_tick - CLOCK_LOCK_ADJUST;
            }
            else if(error < 0) {
                clks.next_tick_time += clks.ext_us_per_tick + CLOCK_LOCK_ADJUST;
            }
            else {
                clks.next_tick_time += clks.ext_us_per_tick;
            }
        }
    }

    //
    // recover external clock
    // - interval range: 30 to 300 BPM = 83300us to 8330us per MIDI tick
    //
    // MIDI tick was received
    if(clks.ext_tickf) {
        clks.ext_tickf = 0;
        // manage clock timing
        clks.ext_recover_hist[clks.ext_recover_hist_pos & 
            (CLOCK_EXTERNAL_HIST_LEN - 1)] = 
            clks.time_count - clks.ext_recover_last_tick;
        clks.ext_recover_last_tick = clks.time_count;
        clks.ext_recover_hist_pos ++;
        clks.ext_recover_tick_count += CLOCK_MIDI_UPSAMPLE;
        // we've received enough ticks
        if(clks.ext_recover_hist_pos > CLOCK_EXTERNAL_HIST_LEN) {
            // average the received clock periods
            temp = 0;
            for(i = 0; i < CLOCK_EXTERNAL_HIST_LEN; i ++) {
                temp += clks.ext_recover_hist[i];
            }
            temp /= CLOCK_EXTERNAL_HIST_LEN;  // average
            temp /= CLOCK_MIDI_UPSAMPLE;  // upsample
            // ensure that tempo fits in the valid range before accepting
            if(temp < CLOCK_US_PER_TICK_MIN) {
                clks.ext_us_per_tick = CLOCK_US_PER_TICK_MIN;
            }
            else if(temp > CLOCK_US_PER_TICK_MAX) {
                clks.ext_us_per_tick = CLOCK_US_PER_TICK_MAX;
            }
            else {
                clks.ext_us_per_tick = temp;
            }
            // should we switch clock source to external?
            if(clks.source == CLOCK_INTERNAL) {
                clock_set_source(CLOCK_EXTERNAL);
                clks.next_tick_time = clks.time_count;  // issue tick right away
                clks.ext_generate_tick_count = clks.ext_recover_tick_count;  // jam
                clks.ext_generate_run_tick_pos = 0;
                clks.ext_recover_run_tick_pos = 0;
            }
        }
        // MIDI continue received
        if(clks.ext_continuef) {
            clks.ext_continuef = 0;
            clks.run_state = 1;
            // dispatch run state since this came from outside
            seq_ctrl_set_run_state(1);
        }
        // generate run ticks
        else if(clks.run_state) {
            clks.ext_recover_run_tick_pos += CLOCK_MIDI_UPSAMPLE;
        }
    }
    // time out external clock mode
    if(clks.source == CLOCK_EXTERNAL &&
            (clks.time_count - clks.ext_recover_last_tick) > CLOCK_EXTERNAL_TIMEOUT) {
        clock_set_source(CLOCK_INTERNAL);
        clks.run_state = 0;  // stop clock on external clock loss
        // dispatch run state since this came from outside
        seq_ctrl_set_run_state(0);
    }

    //
    // recover tap tempo input
    //
    // we can't do this when we're running on external clock
    if(clks.tap_beatf && clks.source == CLOCK_INTERNAL) {
        clks.tap_beatf = 0;
        clks.tap_hist[clks.tap_hist_count % CLOCK_TAP_HIST_LEN] =
            clks.time_count - clks.tap_clock_last_tap;
        clks.tap_clock_last_tap = clks.time_count;
        clks.tap_hist_count ++;
        // we've received enough taps
        if(clks.tap_hist_count > CLOCK_TAP_HIST_LEN) {
            clks.tap_clock_period = 0;
            for(i = 0; i < CLOCK_TAP_HIST_LEN; i ++) {
                clks.tap_clock_period += clks.tap_hist[i];
            }
            clks.tap_clock_period /= CLOCK_TAP_HIST_LEN;
            temp = (clks.tap_clock_period / CLOCK_PPQ);            
            // ensure that tempo fits in the valid range before accepting
            if(temp < CLOCK_US_PER_TICK_MIN) {
                clks.int_us_per_tick = CLOCK_US_PER_TICK_MIN;
            }
            else if(temp > CLOCK_US_PER_TICK_MAX) {
                clks.int_us_per_tick = CLOCK_US_PER_TICK_MAX;
            }
            else {
                clks.int_us_per_tick = temp;
            }
            // fire event
            state_change_fire0(SCE_CLK_TAP_LOCK);
        }
    }
    // time out tap tempo history
    if(clks.tap_hist_count && 
            (clks.time_count - clks.tap_clock_last_tap) > CLOCK_TAP_TIMEOUT) {
        clks.tap_hist_count = 0;
    }
}

// get the clock source
int clock_get_source(void) {
    return clks.source;
}

// get the clock tempo (internal clock)
float clock_get_tempo(void) {
    return 60000000.0 / (float)CLOCK_PPQ / (float)clks.int_us_per_tick;
}

// set the clock tempo (internal clock)
void clock_set_tempo(float tempo) {
    clks.int_us_per_tick = (uint64_t)(60000000.0 / (tempo * (float)CLOCK_PPQ));
}

// set the clock swing
void clock_set_swing(int swing) {
    clks.next_swing = (seq_utils_clamp(swing, 
        SEQ_SWING_MIN, SEQ_SWING_MAX) - SEQ_SWING_MIN);
}

// handle tap tempo
void clock_tap_tempo(void) {
    clks.tap_beatf = 1;
}

// reset the clock to the start
void clock_reset_pos(void) {
    clks.run_tick_count = 0;
    clks.stop_tick_count = 0;
    clks.ext_generate_run_tick_pos = 0;
    clks.ext_recover_run_tick_pos = 0;
}

// get the current clock tick position
uint32_t clock_get_tick_pos(void) {
    if(clks.run_state) {
        return clks.run_tick_count;
    }
    return clks.stop_tick_count;
}

// get the running state of the clock
int clock_get_running(void) {
    return clks.run_state;
}

// start and stop the clock
void clock_set_running(int running) {
    if(clks.source == CLOCK_INTERNAL) {
        clks.desired_run_state = running;
    }
    // can only stop from the panel when in ext clock mode
    else if(!running) {
        clks.ext_stopf = 0;
        clks.run_state = 0;
        clks.ext_generate_tick_count = clks.ext_recover_tick_count;
    }
}

//
// external clock inputs
//
// a MIDI tick was received
void clock_midi_rx_tick(void) {
    clks.ext_tickf = 1;
}

// a MIDI clock start was received
void clock_midi_rx_start(void) {
    clks.ext_startf = 0;
    clks.run_state = 1;
    seq_ctrl_reset_pos();
    seq_ctrl_set_run_state(1);
}

// a MIDI clock continue was received
void clock_midi_rx_continue(void) {
    clks.ext_continuef = 1;
}

// a MIDI clock stop was received
void clock_midi_rx_stop(void) {
    clks.ext_stopf = 0;
    clks.run_state = 0;
    seq_ctrl_set_run_state(0);
    clks.ext_generate_tick_count = clks.ext_recover_tick_count;
}

//
// local functions
//
// set the clock source
void clock_set_source(int source) {
    if(source == CLOCK_INTERNAL) {
        clks.source = CLOCK_INTERNAL;
    }
    else {
        clks.source = CLOCK_EXTERNAL;
    }
    // fire event
    state_change_fire1(SCE_CLK_SOURCE, clks.source);
}

