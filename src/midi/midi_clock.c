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
#include "midi_clock.h"
#include "../tables/swing_table.h"
#include "../config.h"
#include "../util/log.h"
#include "../util/seq_utils.h"

// setings
#define MIDI_CLOCK_US_PER_TICK_MAX ((uint64_t)(60000000.0 / (MIDI_CLOCK_TEMPO_MIN * (float)MIDI_CLOCK_PPQ)))
#define MIDI_CLOCK_US_PER_TICK_MIN ((uint64_t)(60000000.0 / (MIDI_CLOCK_TEMPO_MAX * (float)MIDI_CLOCK_PPQ)))
//#define MIDI_CLOCK_LOCK_ADJUST 500  // lock adjust amount
#define MIDI_CLOCK_TAP_TIMEOUT 2500000  // us (just longer than 30BPM)
#define MIDI_CLOCK_TAP_HIST_LEN 2  // taps in history buffer - required taps will be +1
//#define MIDI_CLOCK_EXTERNAL_TIMEOUT 250000  // us
//#define MIDI_CLOCK_EXTERNAL_HIST_LEN 8  // ticks in the history buffer (power of 2)

enum {
    MIDI_CLOCK_RUNSTOP_IDLE,  // no action
    MIDI_CLOCK_RUNSTOP_START,  // start requested
    MIDI_CLOCK_RUNSTOP_CONTINUE,  // continue requested
    MIDI_CLOCK_RUNSTOP_STOP  // stop requested
};

// state
struct midi_clock_state {
    // state
    int desired_source;  // 0 = external, 1 = internal
    int source;   // 0 = external, 1 = internal
    int desired_run_state;  // 0 = stopped, 1 = running
    int run_state;  // 0 = stopped, 1 = running
    int desired_swing;  // (0-30 = 50-80%)
    int swing;  // (50-80 = 50-80%)
    int runstop_f;  // flag indicates we want to change the playback state
    int reset_f;  // flag to signal we want to reset playback (but not change playback state)
    int ext_tickf;  // external tick received flag
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
/*
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
    int32_t ext_recover_hist[MIDI_CLOCK_EXTERNAL_HIST_LEN];  // historical periods for averaging
*/
    //
    // tap tempo state
    //
    int tap_beat_f;  // tap tempo beat was received
    uint64_t tap_clock_last_tap;  // last tap time
    int tap_clock_period;  // tap clock period - averaged
    int tap_hist_count;  // number of historical taps
    uint64_t tap_hist[MIDI_CLOCK_TAP_HIST_LEN];  // historical taps
};
struct midi_clock_state mcs;

// local functions
void midi_clock_reset_pos(void);

// init the clock
void midi_clock_init(void) {
    // general / internal clock
    mcs.desired_source = MIDI_CLOCK_INTERNAL;
    mcs.source = MIDI_CLOCK_INTERNAL;
    mcs.desired_run_state = 0;
    mcs.run_state = 0;
    mcs.desired_swing = MIDI_CLOCK_SWING_MIN;
    mcs.swing = MIDI_CLOCK_SWING_MIN;
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_IDLE;
    mcs.reset_f = 0;
    mcs.ext_tickf = 0;
    mcs.run_tick_count = 0;
    mcs.stop_tick_count = 0;
    midi_clock_set_tempo(MIDI_CLOCK_DEFAULT_TEMPO);
    mcs.time_count = 0;
    mcs.next_tick_time = 0;
    // tap tempo
    mcs.tap_beat_f = 0;
    mcs.tap_clock_last_tap = 0;
    mcs.tap_clock_period = 0;
    mcs.tap_hist_count = 0;
}

// run the MIDI clock timer task
// call at MIDI_CLOCK_TASK_INTERVAL_US interval
void midi_clock_timer_task(void) {
    int i;
    uint32_t tick_count;
    int32_t temp;
//    int error = 0;

    // handle playback state change flags
    switch(mcs.runstop_f) {
        case MIDI_CLOCK_RUNSTOP_START:
            mcs.desired_run_state = 1;
            midi_clock_reset_pos();
            mcs.runstop_f = MIDI_CLOCK_RUNSTOP_IDLE;
            break;
        case MIDI_CLOCK_RUNSTOP_CONTINUE:
            mcs.desired_run_state = 1;
            mcs.runstop_f = MIDI_CLOCK_RUNSTOP_IDLE;
            break;
        case MIDI_CLOCK_RUNSTOP_STOP:
            mcs.desired_run_state = 0;
            mcs.runstop_f = MIDI_CLOCK_RUNSTOP_IDLE;
            break;
    }

    // handle reset flags
    if(mcs.reset_f) {
        midi_clock_reset_pos();
        mcs.reset_f = 0;
    }

    //
    // run clock timebase
    //
    mcs.time_count += MIDI_CLOCK_TASK_INTERVAL_US;
    // internal clock
    if(mcs.source == MIDI_CLOCK_INTERNAL) {
        // decide if we should issue a clock
        if(mcs.time_count > mcs.next_tick_time) {
            // if run state changed
            if(mcs.run_state != mcs.desired_run_state) {
                mcs.run_state = mcs.desired_run_state;
                // stopping
                if(mcs.run_state == 0) {
                    mcs.stop_tick_count = mcs.run_tick_count;
                }
                midi_clock_run_state_changed(mcs.run_state);
            }
            // get the correct tick count
            if(mcs.run_state) {
                tick_count = mcs.run_tick_count;
            }
            else {
                tick_count = mcs.stop_tick_count;
            }
            // calculate beat cross before processing sequencer stuff
            if((tick_count % MIDI_CLOCK_PPQ) == 0) {
                // if the swing is adjusting we change it now
                if(mcs.desired_swing != mcs.swing) {
                    mcs.desired_swing = mcs.swing;
                }
                log_debug("beat! - runstate: %d", mcs.run_state);
                midi_clock_beat_crossed();
            }            
            // generate correct number of pulses for current swing mode
            for(i = 0; i < swing[mcs.swing][tick_count % MIDI_CLOCK_PPQ]; i ++) {
                midi_clock_ticked(tick_count);
            }
            tick_count ++;
            mcs.next_tick_time += mcs.int_us_per_tick;
            // write back the tick count
            if(mcs.run_state) {
                mcs.run_tick_count = tick_count;
            }
            else {
                mcs.stop_tick_count = tick_count;
            }
        }
    }
/*
    // external clock
    else {
        // decide if we should issue a clock
        if(mcs.time_count > mcs.next_tick_time) {
            // generate correct number of pulses for current swing mode
            for(i = 0; i < swing[mcs.current_swing][mcs.ext_generate_run_tick_pos % MIDI_CLOCK_PPQ]; i ++) {
                if(mcs.run_state) {
//                    seq_ctrl_clock_tick(mcs.ext_generate_run_tick_pos);
                }
                else {
//                    seq_ctrl_clock_tick(mcs.ext_generate_tick_count);
                }
            }
            // running
            if(mcs.run_state) {
                // calculate beat cross before processing sequencer stuff
                if((mcs.ext_generate_run_tick_pos % MIDI_CLOCK_PPQ) == 0) {
                    // if the swing is adjusting we change it now
                    if(mcs.current_swing != mcs.next_swing) {
                        mcs.current_swing = mcs.next_swing;
                    }
                    // fire event
//                    state_change_fire0(SCE_CLK_BEAT);
                }            
                mcs.ext_generate_run_tick_pos ++;
                // calculate error
                error = mcs.ext_recover_run_tick_pos - mcs.ext_generate_run_tick_pos;
            }
            // stopped
            else {
                mcs.ext_generate_tick_count ++;
                // calculate error
                error = mcs.ext_recover_tick_count - mcs.ext_generate_tick_count;
            }

            // calculate next tick time and adjust for error
            if(error > 0) {
                mcs.next_tick_time += mcs.ext_us_per_tick - MIDI_CLOCK_LOCK_ADJUST;
            }
            else if(error < 0) {
                mcs.next_tick_time += mcs.ext_us_per_tick + MIDI_CLOCK_LOCK_ADJUST;
            }
            else {
                mcs.next_tick_time += mcs.ext_us_per_tick;
            }
        }
    }

    //
    // recover external clock
    // - interval range: 30 to 300 BPM = 83300us to 8330us per MIDI tick
    //
    // MIDI tick was received
    if(mcs.ext_tickf) {
        mcs.ext_tickf = 0;
        // manage clock timing
        mcs.ext_recover_hist[mcs.ext_recover_hist_pos & 
            (MIDI_CLOCK_EXTERNAL_HIST_LEN - 1)] = 
            mcs.time_count - mcs.ext_recover_last_tick;
        mcs.ext_recover_last_tick = mcs.time_count;
        mcs.ext_recover_hist_pos ++;
        mcs.ext_recover_tick_count += MIDI_CLOCK_UPSAMPLE;
        // we've received enough ticks
        if(mcs.ext_recover_hist_pos > MIDI_CLOCK_EXTERNAL_HIST_LEN) {
            // average the received clock periods
            temp = 0;
            for(i = 0; i < MIDI_CLOCK_EXTERNAL_HIST_LEN; i ++) {
                temp += mcs.ext_recover_hist[i];
            }
            temp /= MIDI_CLOCK_EXTERNAL_HIST_LEN;  // average
            temp /= MIDI_CLOCK_UPSAMPLE;  // upsample
            // ensure that tempo fits in the valid range before accepting
            if(temp < MIDI_CLOCK_US_PER_TICK_MIN) {
                mcs.ext_us_per_tick = MIDI_CLOCK_US_PER_TICK_MIN;
            }
            else if(temp > MIDI_CLOCK_US_PER_TICK_MAX) {
                mcs.ext_us_per_tick = MIDI_CLOCK_US_PER_TICK_MAX;
            }
            else {
                mcs.ext_us_per_tick = temp;
            }
            // should we switch clock source to external?
            if(mcs.source == MIDI_CLOCK_INTERNAL) {
                midi_clock_set_source(MIDI_CLOCK_EXTERNAL);
                mcs.next_tick_time = mcs.time_count;  // issue tick right away
                mcs.ext_generate_tick_count = mcs.ext_recover_tick_count;  // jam
                mcs.ext_generate_run_tick_pos = 0;
                mcs.ext_recover_run_tick_pos = 0;
            }
        }
        // MIDI continue received
        if(mcs.ext_continuef) {
            mcs.ext_continuef = 0;
            mcs.run_state = 1;
            // dispatch run state since this came from outside
//            seq_ctrl_set_run_state(1);
        }
        // generate run ticks
        else if(mcs.run_state) {
            mcs.ext_recover_run_tick_pos += MIDI_CLOCK_UPSAMPLE;
        }
    }
    // time out external clock mode
    if(mcs.source == MIDI_CLOCK_EXTERNAL &&
            (mcs.time_count - mcs.ext_recover_last_tick) > MIDI_CLOCK_EXTERNAL_TIMEOUT) {
        midi_clock_set_source(MIDI_CLOCK_INTERNAL);
        mcs.run_state = 0;  // stop clock on external clock loss
        // dispatch run state since this came from outside
//        seq_ctrl_set_run_state(0);
    }
*/

    //
    // recover tap tempo input
    //
    // we can't do this when we're running on external clock
    if(mcs.tap_beat_f && mcs.source == MIDI_CLOCK_INTERNAL) {
        mcs.tap_beat_f = 0;
        mcs.tap_hist[mcs.tap_hist_count % MIDI_CLOCK_TAP_HIST_LEN] =
            mcs.time_count - mcs.tap_clock_last_tap;
        mcs.tap_clock_last_tap = mcs.time_count;
        mcs.tap_hist_count ++;
        // we've received enough taps
        if(mcs.tap_hist_count > MIDI_CLOCK_TAP_HIST_LEN) {
            mcs.tap_clock_period = 0;
            for(i = 0; i < MIDI_CLOCK_TAP_HIST_LEN; i ++) {
                mcs.tap_clock_period += mcs.tap_hist[i];
            }
            mcs.tap_clock_period /= MIDI_CLOCK_TAP_HIST_LEN;
            temp = (mcs.tap_clock_period / MIDI_CLOCK_PPQ);            
            // ensure that tempo fits in the valid range before accepting
            if(temp < MIDI_CLOCK_US_PER_TICK_MIN) {
                mcs.int_us_per_tick = MIDI_CLOCK_US_PER_TICK_MIN;
            }
            else if(temp > MIDI_CLOCK_US_PER_TICK_MAX) {
                mcs.int_us_per_tick = MIDI_CLOCK_US_PER_TICK_MAX;
            }
            else {
                mcs.int_us_per_tick = temp;
            }
            midi_clock_tap_locked();
        }
    }
    // time out tap tempo history
    if(mcs.tap_hist_count && 
            (mcs.time_count - mcs.tap_clock_last_tap) > MIDI_CLOCK_TAP_TIMEOUT) {
        mcs.tap_hist_count = 0;
    }
}

// get the clock source
int midi_clock_get_source(void) {
    return mcs.source;
}

// get the clock tempo (internal clock)
float midi_clock_get_tempo(void) {
    return 60000000.0 / (float)MIDI_CLOCK_PPQ / (float)mcs.int_us_per_tick;
}

// set the clock tempo (internal clock)
void midi_clock_set_tempo(float tempo) {
    mcs.int_us_per_tick = (uint64_t)(60000000.0 / (tempo * (float)MIDI_CLOCK_PPQ));
}

// get the clock swing
int midi_clock_get_swing(void) {
    return mcs.swing;
}

// set the clock swing
void midi_clock_set_swing(int swing) {
    mcs.desired_swing = (seq_utils_clamp(swing, 
        MIDI_CLOCK_SWING_MIN, MIDI_CLOCK_SWING_MAX) - MIDI_CLOCK_SWING_MIN);
}

// handle tap tempo
void midi_clock_tap_tempo(void) {
    mcs.tap_beat_f = 1;
}

// handle a request to continue playback
void midi_clock_request_continue(void) {
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_CONTINUE;
}

// handle a request to stop playback
void midi_clock_request_stop(void) {
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_STOP;
}

// handle a request to reset the playback position
void midi_clock_request_reset_pos(void) {
    mcs.reset_f = 1;
}

// get the current clock tick position
uint32_t midi_clock_get_tick_pos(void) {
    if(mcs.run_state) {
        return mcs.run_tick_count;
    }
    return mcs.stop_tick_count;
}

// get the running state of the clock
int midi_clock_get_running(void) {
    return mcs.run_state;
}

//
// external clock inputs
//
// a MIDI tick was received
void midi_clock_midi_rx_tick(void) {
    mcs.ext_tickf = 1;
}

// a MIDI clock start was received
void midi_clock_midi_rx_start(void) {
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_START;
}

// a MIDI clock continue was received
void midi_clock_midi_rx_continue(void) {
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_CONTINUE;
}

// a MIDI clock stop was received
void midi_clock_midi_rx_stop(void) {
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_STOP;
}

//
// callbacks - these should be overriden if needed
//
// a beat was crossed
__weak void midi_clock_beat_crossed(void) {
    // unimplemented stub
}

// the run state changed
__weak void midi_clock_run_state_changed(int running) {
    // unimplemented stub
}

// the clock source changed
__weak void midi_clock_source_changed(int source) {
    // unimplemented stub
}

// the tap tempo locked
__weak void midi_clock_tap_locked(void) {
    // unimplemented stub
}

// the clock ticked
__weak void midi_clock_ticked(uint32_t tick_count) {
    // unimplemented stub
}

// the clock position was reset
__weak void midi_clock_pos_reset(void) {
    // unimplemented stub
}

//
// local functions
//
// reset the position
void midi_clock_reset_pos(void) {
    mcs.run_tick_count = 0;
    mcs.stop_tick_count = 0;
}

