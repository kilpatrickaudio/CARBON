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
#include <math.h>

// setings
#define MIDI_CLOCK_US_PER_TICK_MAX ((uint64_t)(60000000.0 / (MIDI_CLOCK_TEMPO_MIN * (float)MIDI_CLOCK_PPQ)))
#define MIDI_CLOCK_US_PER_TICK_MIN ((uint64_t)(60000000.0 / (MIDI_CLOCK_TEMPO_MAX * (float)MIDI_CLOCK_PPQ)))
#define MIDI_CLOCK_TAP_TIMEOUT 2500000  // us (just longer than 30BPM)
#define MIDI_CLOCK_TAP_HIST_LEN 2  // taps in history buffer - required taps will be +1
#define MIDI_CLOCK_EXT_HIST_LEN 8  // number of historical intervals to average (must be a power of 2)
#define MIDI_CLOCK_EXT_HIST_MASK (MIDI_CLOCK_EXT_HIST_LEN - 1)
#define MIDI_CLOCK_EXT_MIN_HIST 3  // number of interval samples needed before changing internal clock
#define MIDI_CLOCK_EXT_SYNC_TIMEOUT 125000  // timeout for receiving external sync (us)
#define MIDI_CLOCK_EXT_ERROR_ADJ 500  // lock adjust amount
#define MIDI_CLOCK_EXT_SYNC_TEMPO_FILTER 0.9  // tempo average filter coeff

enum {
    MIDI_CLOCK_RUNSTOP_IDLE,  // no action
    MIDI_CLOCK_RUNSTOP_START,  // start requested
    MIDI_CLOCK_RUNSTOP_CONTINUE,  // continue requested
    MIDI_CLOCK_RUNSTOP_STOP  // stop requested
};

// state
struct midi_clock_state {
    // general clock state
    int desired_source;  // 0 = external, 1 = internal
    int source;   // 0 = external, 1 = internal
    int desired_run_state;  // 0 = stopped, 1 = running
    int run_state;  // 0 = stopped, 1 = running
    int desired_swing;  // (0-30 = 50-80%)
    int swing;  // (50-80 = 50-80%)
    int runstop_f;  // flag indicates we want to change the playback state
    int reset_f;  // flag to signal we want to reset playback (but not change playback state)
    int ext_tick_f;  // external tick received flag
    uint64_t time_count;  // running time count
    uint64_t next_tick_time;  // time for the next tick
    // internal clock state
    int32_t run_tick_count;  // running tick count
    int32_t stop_tick_count;   // stopped tick count
    int32_t int_us_per_tick;  // number of us per tick (internal)
    // external clock recovery state
    int32_t ext_interval_hist[MIDI_CLOCK_EXT_HIST_LEN];  // historical interval history
    int32_t ext_interval_count;  // number of historical intervals measured
    int ext_sync_timeout;  // countdown for invalidating clock
    uint64_t ext_last_tick_time;  // time of the last tick received
    int32_t ext_run_tick_count;  // count of external ticks
    int ext_sync_tempo_average;  // average tempo for display
    // tap tempo state
    int tap_beat_f;  // tap tempo beat was received
    uint64_t tap_clock_last_tap;  // last tap time
    int tap_clock_period;  // tap clock period - averaged
    int tap_hist_count;  // number of historical taps
    uint64_t tap_hist[MIDI_CLOCK_TAP_HIST_LEN];  // historical taps
};
struct midi_clock_state mcs;

// local functions
void midi_clock_reset_pos(void);
void midi_clock_change_run_state(int run);

// init the clock
void midi_clock_init(void) {
    // general clock state
    mcs.desired_source = MIDI_CLOCK_INTERNAL;
    mcs.source = MIDI_CLOCK_INTERNAL;
    mcs.desired_run_state = 0;
    mcs.run_state = 0;
    mcs.desired_swing = MIDI_CLOCK_SWING_MIN + 1;
    midi_clock_set_swing(MIDI_CLOCK_SWING_MIN);
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_IDLE;
    mcs.reset_f = 0;
    mcs.ext_tick_f = 0;
    mcs.time_count = 0;
    mcs.next_tick_time = 0;
    // internal clock state
    mcs.run_tick_count = 0;
    mcs.stop_tick_count = 0;
    midi_clock_set_tempo(MIDI_CLOCK_DEFAULT_TEMPO);
    // external clock recovery state
    mcs.ext_interval_count = 0;
    mcs.ext_sync_timeout = 0;  // timed out
    mcs.ext_last_tick_time = 0;
    mcs.ext_run_tick_count = 0;
    mcs.ext_sync_tempo_average = mcs.int_us_per_tick;  // default
    // tap tempo
    mcs.tap_beat_f = 0;
    mcs.tap_clock_last_tap = 0;
    mcs.tap_clock_period = 0;
    mcs.tap_hist_count = 0;
}

// run the MIDI clock timer task
// call at MIDI_CLOCK_TASK_INTERVAL_US interval
void midi_clock_timer_task(void) {
    int i, error;
    uint32_t tick_count;
    int32_t temp;

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

    // handle source change
    if(mcs.source != mcs.desired_source) {
        mcs.source = mcs.desired_source;
        midi_clock_source_changed(mcs.source);
        midi_clock_change_run_state(0);  // force stop
    }

    // run clock timebase
    mcs.time_count += MIDI_CLOCK_TASK_INTERVAL_US;
    // decide if we should issue a clock
    if(mcs.time_count > mcs.next_tick_time) {
        // if run state changed
        if(mcs.run_state != mcs.desired_run_state) {
            // stopping
            if(mcs.desired_run_state == 0) {
                mcs.stop_tick_count = mcs.run_tick_count;
            }
            midi_clock_change_run_state(mcs.desired_run_state);
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
                mcs.swing = mcs.desired_swing;
            }
            midi_clock_beat_crossed();
            // update the recovered tempo display
            if(midi_clock_is_ext_synced()) {
//                log_debug("avg: %d - tempo: %f", mcs.ext_sync_tempo_average, midi_clock_get_tempo());
                midi_clock_ext_tempo_changed();
            }
            // XXX debug
//            log_debug("us: %d - run_tick: %d - ext_run_tick: %d - diff: %d",
//                mcs.int_us_per_tick,
//                mcs.run_tick_count, mcs.ext_run_tick_count,
//                (mcs.run_tick_count - mcs.ext_run_tick_count));
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

    // recover external clock and drive the internal clock
    if(mcs.source == MIDI_CLOCK_EXTERNAL) {
        // a tick was received
        if(mcs.ext_tick_f) {
            mcs.ext_tick_f = 0;
            // XXX debug
            if(!mcs.ext_sync_timeout) {
                log_debug("ext sync start");
            }
            mcs.ext_sync_timeout = MIDI_CLOCK_EXT_SYNC_TIMEOUT;
            // measure interval - skip the very first time since it will be wrong
            mcs.ext_interval_hist[(mcs.ext_interval_count - 1) & MIDI_CLOCK_EXT_HIST_MASK] =
                mcs.time_count - mcs.ext_last_tick_time;
            // average the number of samples we have
            temp = 0;
            for(i = 0; i < MIDI_CLOCK_EXT_HIST_LEN && i < mcs.ext_interval_count; i ++) {
                temp += mcs.ext_interval_hist[i];
            }
            // if we have at least MIDI_CLOCK_EXT_MIN_HIST samples let's update the internal clock
            if(i >= MIDI_CLOCK_EXT_MIN_HIST) {
                temp /= i;
                // set the internal clock to this value
                mcs.int_us_per_tick = temp / MIDI_CLOCK_UPSAMPLE;  // convert to 96PPQ
                // XXX debug
//                log_debug("i: %d - avg: %d - time: %lld - last: %lld - diff: %lld", i, temp,
//                    mcs.time_count, mcs.ext_last_tick_time, 
//                    (mcs.time_count - mcs.ext_last_tick_time));
                mcs.ext_sync_tempo_average = ((float)mcs.ext_sync_tempo_average * 
                    MIDI_CLOCK_EXT_SYNC_TEMPO_FILTER) +
                    ((float)mcs.int_us_per_tick * (1.0 - MIDI_CLOCK_EXT_SYNC_TEMPO_FILTER ));
            }
            // count external ticks if running
            if(mcs.run_state) {
                mcs.ext_run_tick_count += MIDI_CLOCK_UPSAMPLE;
                // compensate for error
                error = mcs.run_tick_count - mcs.ext_run_tick_count;
                if(error < 0) {
                    mcs.int_us_per_tick -= MIDI_CLOCK_EXT_ERROR_ADJ;
                }
                else if(error > 0) {
                    mcs.int_us_per_tick += MIDI_CLOCK_EXT_ERROR_ADJ;
                }
            }
            mcs.ext_last_tick_time = mcs.time_count;
            mcs.ext_interval_count ++;
        }
    }

    // timeout external sync
    if(mcs.ext_sync_timeout) {
        mcs.ext_sync_timeout -= MIDI_CLOCK_TASK_INTERVAL_US;
        if(mcs.ext_sync_timeout <= 0) {
            // XXX debug
            log_debug("ext sync lost");
            mcs.ext_interval_count = 0;  // reset
            mcs.runstop_f = MIDI_CLOCK_RUNSTOP_STOP;
        }
    }

    // recover tap tempo input
    // we can't do this when we're locked to external clock
    if(mcs.tap_beat_f && !mcs.ext_sync_timeout) {
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

// set the MIDI clock source
void midi_clock_set_source(int source) {
    if(source == MIDI_CLOCK_EXTERNAL) {
        mcs.desired_source = source;
    }
    else {
        mcs.desired_source = MIDI_CLOCK_INTERNAL;
    }
}

// check if the external clock is synced
int midi_clock_is_ext_synced(void) {
    if(mcs.source == MIDI_CLOCK_EXTERNAL && mcs.ext_sync_timeout) {
        return 1;
    }
    return 0;
}

// get the clock tempo (internal clock)
float midi_clock_get_tempo(void) {
    // external sync
    if(midi_clock_is_ext_synced()) {
        return 60000000.0 / (float)MIDI_CLOCK_PPQ / (float)mcs.ext_sync_tempo_average;
    }
    // internal clock
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
    if(swing < MIDI_CLOCK_SWING_MIN || swing > MIDI_CLOCK_SWING_MAX) {
        return;
    }
    mcs.desired_swing = seq_utils_clamp(swing, 
        MIDI_CLOCK_SWING_MIN, MIDI_CLOCK_SWING_MAX) - 
        MIDI_CLOCK_SWING_MIN;
}

// handle tap tempo
void midi_clock_tap_tempo(void) {
    mcs.tap_beat_f = 1;
}

// handle a request to continue playback
void midi_clock_request_continue(void) {
    // ignore request while sync is received
    if(mcs.ext_sync_timeout) {
        return;
    }
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_CONTINUE;
}

// handle a request to stop playback
void midi_clock_request_stop(void) {
    // ignore request while sync is received
    if(mcs.ext_sync_timeout) {
        return;
    }
    mcs.runstop_f = MIDI_CLOCK_RUNSTOP_STOP;
}

// handle a request to reset the playback position
void midi_clock_request_reset_pos(void) {
    // ignore request while sync is received
    if(mcs.ext_sync_timeout) {
        return;
    }
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
    mcs.ext_tick_f = 1;
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

// the externally locked tempo changed
__weak void midi_clock_ext_tempo_changed(void) {
    // unimplemented stub
}


//
// local functions
//
// reset the position
void midi_clock_reset_pos(void) {
    mcs.run_tick_count = 0;
    mcs.stop_tick_count = 0;
    mcs.ext_run_tick_count = 0;
}

// change the run state
void midi_clock_change_run_state(int run) {
    mcs.desired_run_state = run;
    mcs.run_state = run;
    midi_clock_run_state_changed(mcs.run_state);
}

