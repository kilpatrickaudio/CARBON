/*
 * CARBON Sequencer Clock Output Module
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
#include "clock_out.h"
#include "song.h"
#include "../analog_out.h"
#include "../config.h"
#include "../midi/midi_stream.h"
#include "../midi/midi_utils.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"

struct clock_out_state {
    uint8_t desired_run_state;  // desired run state - 0 = stop, 1 = run
    uint8_t run_state;  // actual run state - 0 = stopped, 1 = running
    uint8_t out_ppq[MIDI_PORT_NUM_TRACK_OUTPUTS];  // output PPQ setting
    uint8_t out_div_run[MIDI_PORT_NUM_TRACK_OUTPUTS];  // output divide counter - running
    uint8_t out_div_stop[MIDI_PORT_NUM_TRACK_OUTPUTS];  // output divide counter - stopped
    uint8_t analog_clock_timeout;  // timeout counter for analog clock
    uint8_t analog_clock_delay_trigger;  // trigger the clock after the reset is finished
    uint8_t analog_reset_timeout;  // timeout counter for analog reset
};
struct clock_out_state clkouts;

// local functions
void clock_out_set_output(int output, int ppq);
void clock_out_set_run_state(int run);
void clock_out_generate_start(int32_t tick_count);
void clock_out_generate_stop(void);

// init the clock output module
void clock_out_init(void) {
    int i;
    clkouts.desired_run_state = 0;
    clkouts.run_state = 0;
    for(i = 0; i < MIDI_PORT_NUM_TRACK_OUTPUTS; i ++) {
        clkouts.out_ppq[i] = seq_utils_clock_pqq_to_divisor(SEQ_UTILS_CLOCK_OFF);
        clkouts.out_div_run[i] = 0;
        clkouts.out_div_stop[i] = 0;
    }
    clkouts.analog_clock_timeout = 0;
    clkouts.analog_clock_delay_trigger = 0;
    clkouts.analog_reset_timeout = 0;
    // register for events    
    state_change_register(clock_out_handle_state_change, SCEC_SONG);
    state_change_register(clock_out_handle_state_change, SCEC_CTRL);
}

// realtime tasks for the clock output
void clock_out_timer_task(void) {
    // time out clock / reset output pulses - analog output only
    if(clkouts.analog_clock_delay_trigger && 
            clkouts.analog_reset_timeout == 0) {
        clkouts.analog_clock_delay_trigger = 0;
        if(clkouts.run_state) {
            analog_out_set_clock(1);
            clkouts.analog_clock_timeout = (CLOCK_OUT_PULSE_LEN + 1);
        }
    }
    if(clkouts.analog_clock_timeout) {
        clkouts.analog_clock_timeout --;
        if(clkouts.analog_clock_timeout == 0) {
            analog_out_set_clock(0);            
        }
    }
    if(clkouts.analog_reset_timeout) {
        clkouts.analog_reset_timeout --;
        if(clkouts.analog_reset_timeout == 0) {
            analog_out_set_reset(0);
        }
    }
}

// run the clock output - called on each clock tick
void clock_out_run(uint32_t tick_count) {
    struct midi_msg send_msg;
    int i;
    // position was reset
    if(tick_count == 0) {
        for(i = 0; i < MIDI_PORT_NUM_TRACK_OUTPUTS; i ++) {
            clkouts.out_div_run[i] = 0;
            clkouts.out_div_stop[i] = 0;
        }
    }
    
    // run state changed
    if(clkouts.desired_run_state != clkouts.run_state) {
        clkouts.run_state = clkouts.desired_run_state;
        // if we are starting - generate MIDI START, CONTINUE and analog reset pulse
        if(clkouts.desired_run_state) {
            clock_out_generate_start(tick_count);
         }
        // if we are stopping - generate MIDI STOP
        else {
            clock_out_generate_stop();
        }
    }
    // position was reset during running
    else if(clkouts.run_state && tick_count == 0) {
        clock_out_generate_start(tick_count);        
    }

    // generate clock pulses
    // handle each track output
    for(i = 0; i < MIDI_PORT_NUM_TRACK_OUTPUTS; i ++) {
        // output is disabled
        if(clkouts.out_ppq[i] == 0) {
            continue;
        }
        // clock pulse should be issued
        if((clkouts.run_state == 1 && clkouts.out_div_run[i] == 0) ||
                (clkouts.run_state == 0 && clkouts.out_div_stop[i] == 0)) {
            // analog out
            if(i == MIDI_PORT_CV_OUT) {
                // if a reset pulse has been issued then let's make a clock after
                if(clkouts.analog_reset_timeout) {
                    clkouts.analog_clock_delay_trigger = 1;
                }
                // only generate analog clock if the output is running
                else if(clkouts.run_state) {
                    analog_out_set_clock(1);
                    clkouts.analog_clock_timeout = (CLOCK_OUT_PULSE_LEN + 1);
                }
            }
            // MIDI out
            else {
                midi_utils_enc_timing_tick(&send_msg, i);
                midi_stream_send_msg(&send_msg);            
            }
        }
        // increment the divider
        if(clkouts.run_state) {
            clkouts.out_div_run[i] = (clkouts.out_div_run[i] + 1) % clkouts.out_ppq[i];
        }
        else {
            clkouts.out_div_stop[i] = (clkouts.out_div_stop[i] + 1) % clkouts.out_ppq[i];
        }
    }
}

// handle state change
void clock_out_handle_state_change(int event_type, int *data, int data_len) {
    switch(event_type) {
        case SCE_SONG_LOADED:
            clock_out_set_output(MIDI_PORT_DIN1_OUT, 
                song_get_midi_port_clock(MIDI_PORT_DIN1_OUT));
            clock_out_set_output(MIDI_PORT_DIN2_OUT, 
                song_get_midi_port_clock(MIDI_PORT_DIN2_OUT));
            clock_out_set_output(MIDI_PORT_CV_OUT, 
                song_get_midi_port_clock(MIDI_PORT_CV_OUT));
            clock_out_set_output(MIDI_PORT_USB_HOST_OUT, 
                song_get_midi_port_clock(MIDI_PORT_USB_HOST_OUT));
            clock_out_set_output(MIDI_PORT_USB_DEV_OUT1, 
                song_get_midi_port_clock(MIDI_PORT_USB_DEV_OUT1));
            break;
        case SCE_SONG_MIDI_PORT_CLOCK:
            clock_out_set_output(data[0], data[1]);
            break;
        case SCE_CTRL_RUN_STATE:
            clock_out_set_run_state(data[0]);
            break;
        default:
            break;
    }
}

//
// local functions
//
// the clock setting for an output changed
void clock_out_set_output(int output, int ppq) {
    if(output < 0 || output >= MIDI_PORT_NUM_TRACK_OUTPUTS) {
        log_error("coso - output invalid: %d", output);
        return;
    }
    if(ppq < 0 || ppq >= SEQ_UTILS_CLOCK_PPQS) {
        log_error("coso - ppq invalid: %d", ppq);
        return;
    }
    clkouts.out_ppq[output] = seq_utils_clock_pqq_to_divisor(ppq);
}

// the clock module has been started or stopped
void clock_out_set_run_state(int run) {
    if(run) {
        clkouts.desired_run_state = 1;
    }
    else {
        clkouts.desired_run_state = 0;    
    }
}

// generate start or continue messages or reset pulses
void clock_out_generate_start(int32_t tick_count) {
    struct midi_msg send_msg;
    int i;
    // handle each track output
    for(i = 0; i < MIDI_PORT_NUM_TRACK_OUTPUTS; i ++) {
        // output is disabled
        if(clkouts.out_ppq[i] == 0) {
            continue;
        }
        // handle analog out differently
        if(i == MIDI_PORT_CV_OUT) {
            // produce reset if we're at zero time
            if(tick_count == 0) {
                analog_out_set_reset(1);
                clkouts.analog_reset_timeout = (CLOCK_OUT_PULSE_LEN + 1);
            }
        }
        // handle MIDI out
        else {
            // MIDI start
            if(tick_count == 0) {
                midi_utils_enc_clock_start(&send_msg, i);
            }
            // MIDI continue
            else {
                midi_utils_enc_clock_continue(&send_msg, i);
            }
            midi_stream_send_msg(&send_msg);
        }
    } 
}

// generate stop messages
void clock_out_generate_stop(void) {
    struct midi_msg send_msg;
    int i;
    // handle each track output
    for(i = 0; i < MIDI_PORT_NUM_TRACK_OUTPUTS; i ++) {
        // output is disabled
        if(clkouts.out_ppq[i] == 0) {
            continue;
        }
        clkouts.out_div_stop[i] = clkouts.out_div_run[i];  // copy current val
        // handle MIDI out
        if(i != MIDI_PORT_CV_OUT) {
            midi_utils_enc_clock_stop(&send_msg, i);
            midi_stream_send_msg(&send_msg);
        }
    }
}
