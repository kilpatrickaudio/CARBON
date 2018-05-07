/*
 * CARBON Interface MIDI Router
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
 * MIDI Routing:
 *  - USB device IN 1           - to CV/gate outputs / panel LEDs
 *  - USB device IN 2           - to USB host OUT
 *  - USB device IN 3           - to DIN 1 OUT
 *  - USB device IN 4           - to DIN 2 OUT
 *  - USB device OUT 1          - panel controls (see iface_panel.c)
 *  - USB device OUT 2          - from USB host IN
 *  - USB device OUT 3          - from DIN IN
 *
 * LED Control:
 *  - LEDs are controlled on channel 16
 *  - see iface_panel.c for more details
 *
 * CV/gate Output:
 *  - program change on channel 1-4 sets the CV/gate output mode
 *  - control change messages for parameter control can be sent on channel 1-4
 *  - clock output follows MIDI clock
 *  - reset output follows MIDI START message (clock pulse is slightly delayed
 *    when START is received to allow start pulse to clear before clock pulse)
 *
 *  channel assignments for output
 *  - output A      - channel 1
 *  - output B      - channel 2
 *  - output C      - channel 3
 *  - output D      - channel 4
 *
 *  program change modes:
 *  - program 1     - ABCD mode - A = note, B = note, C = note, D = note
 *  - program 2     - ABCD mode - A = note, B = velo, C = note, D = note
 *  - program 3     - ABCD mode - A = note, B = velo, C = note, D = velo
 *  - program 4     - ABCD mode - A = velo, B = velo, C = velo, D = velo
 *  - program 5     - ABCD mode - A = note, B = note, C = CC #1, D = CC #16
 *  - program 6     - ABCD mode - A = note, B = velo, C = CC #1, D = CC #16
 *  - program 7     - ABCD mode - A = note, B = CC #16, C = CC #17, D = CC #18
 *  - program 8     - ABCD mode - A = CC #1, B = CC #16, C = CC #17, D = CC #18
 *  - program 9     - AABC mode - AA = note, B = note, C = note
 *  - program 10    - AABC mode - AA = note, B = note, C = velo
 *  - program 11    - AABC mode - AA = velo, B = note, C = note
 *  - program 12    - AABC mode - AA = velo, B = velo, C = velo
 *  - program 13    - AABC mode - AA = note, B = note, C = CC #1
 *  - program 14    - AABC mode - AA = note, B = velo, C = CC #1
 *  - program 15    - AABC mode - AA = note, B = CC #1, C = CC #16
 *  - program 16    - AABC mode - AA = velo, B = CC #1, C = CC #16
 *  - program 17    - AABB mode - AA = note, BB = note
 *  - program 18    - AABB mode - AA = note, BB = velo
 *  - program 19    - AABB mode - AA = velo, BB = velo
 *  - program 20    - AAAA mode - AAAA = note
 *  - program 21    - AAAA mode - AAAA = velo
 *
 *  control changes:
 *  - CC #20        - CV bend range 0-11 = 1-12 semitones
 *  - CC #21        - analog clock divider - see table in seq_utils.h
 *
 */
#include "iface_midi_router.h"
#include "iface_panel.h"
#include "../analog_out.h"
#include "../cvproc.h"
#include "../config.h"
#include "../config_store.h"
#include "../midi/midi_protocol.h"
#include "../midi/midi_stream.h"
#include "../midi/midi_utils.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"

// settings
#define IFACE_MIDI_ROUTER_CV_CHAN_MAX 3  // max MIDI channel to use for CV
#define IFACE_MIDI_ROUTER_CV_PROGRAM_MIN 0
#define IFACE_MIDI_ROUTER_CV_PROGRAM_MAX 21
#define IFACE_MIDI_ROUTER_CV_PROGRAM_DEFAULT 0  // program change 1
#define IFACE_MIDI_ROUTER_CV_BEND_RANGE_MIN (CVPROC_BEND_RANGE_MIN)
#define IFACE_MIDI_ROUTER_CV_BEND_RANGE_MAX (CVPROC_BEND_RANGE_MAX)
#define IFACE_MIDI_ROUTER_CV_BEND_RANGE_DEFAULT 2
#define IFACE_MIDI_ROUTER_CLOCK_DIV_MIN 0
#define IFACE_MIDI_ROUTER_CLOCK_DIV_MAX (SEQ_UTILS_CLOCK_PPQS)
#define IFACE_MIDI_ROUTER_CLOCK_DIV_DEFAULT (SEQ_UTILS_CLOCK_1PPQ)

// iface MIDI router state
struct iface_midi_router_state {
    // state for handing the clock/reset output
    uint8_t analog_clock_div;  // clock divisor (not the setting) for dividing MIDI clock
    uint8_t analog_clock_div_count;  // a counter to keep track of clock diving
    uint8_t analog_clock_enable;  // 0 = clock disabled, 1 = clock enabled
    uint8_t analog_clock_timeout;  // timeout counter for analog clock
    uint8_t analog_clock_delay_trigger;  // trigger the clock after the reset is finished
    uint8_t analog_reset_timeout;  // timeout counter for analog reset
};
struct iface_midi_router_state ifacemrs;

// local functions
void iface_midi_router_clear_config(void);
void iface_midi_router_load_config(void);
void iface_midi_router_set_program(int prog);
void iface_midi_router_set_bend_range(int bend);
void iface_midi_router_set_clock_div(int div);
void iface_midi_router_handle_cv_setting(struct midi_msg *msg);
// clock handling
void iface_midi_router_clock_task(void);
void iface_midi_router_handle_clock_msg(struct midi_msg *msg);
// LED control
void iface_midi_router_handle_led(struct midi_msg *msg);

// init the interface MIDI router
void iface_midi_router_init(void) {
    // reset stuff
    ifacemrs.analog_clock_enable = 1;  // default to enabled
    ifacemrs.analog_clock_timeout = 0;
    ifacemrs.analog_clock_delay_trigger = 0;
    ifacemrs.analog_reset_timeout = 0;

    // register for events
    state_change_register(iface_midi_router_handle_state_change, SCEC_CONFIG);
}

// run the interface MIDI router timer task
void iface_midi_router_timer_task(void) {
    struct midi_msg msg;
    int chan;

    // handle clock timing
    iface_midi_router_clock_task();

    //
    // handle MIDI inputs
    //
    // USB HOST IN - pass to USB DEV OUT 2
    while(midi_stream_data_available(MIDI_PORT_USB_HOST_IN)) {
        midi_stream_receive_msg(MIDI_PORT_USB_HOST_IN, &msg);
        msg.port = MIDI_PORT_USB_DEV_OUT2;
        midi_stream_send_msg(&msg);
    }
    // DIN 1 IN - pass to USB DEV OUT 3
    while(midi_stream_data_available(MIDI_PORT_DIN1_IN)) {
        midi_stream_receive_msg(MIDI_PORT_DIN1_IN, &msg);
        msg.port = MIDI_PORT_USB_DEV_OUT3;
        midi_stream_send_msg(&msg);
    }
    // USB DEV IN 1 - pass to CV out
    while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN1)) {
        midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN1, &msg);
        // channel mode messages
        if((msg.status & 0xf0) < 0xf0) {
            chan = msg.status & 0x0f;
            // CV/gate output channels
            if(chan <= IFACE_MIDI_ROUTER_CV_CHAN_MAX) {
                // route to CV/gate output module
                switch(msg.status & 0xf0) {
                    case MIDI_NOTE_OFF:
                    case MIDI_NOTE_ON:
                    case MIDI_CONTROL_CHANGE:
                        iface_midi_router_handle_cv_setting(&msg);
                        // fall through
                    case MIDI_PITCH_BEND:
                        midi_utils_rewrite_dest(&msg, MIDI_PORT_CV_OUT, msg.status & 0x0f);
                        midi_stream_send_msg(&msg);
                        break;
                    case MIDI_PROGRAM_CHANGE:
                        if(chan < CVPROC_NUM_PAIRS) {
                            iface_midi_router_set_program(msg.data0);
                        }
                        break;
                    default:
                        break;
                }
            }
            // panel LED control
            else if(chan == IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN) {
                iface_panel_handle_led(&msg);
            }
        }
        // system messages
        else {
            switch(msg.status) {
                case MIDI_TIMING_TICK:
                case MIDI_CLOCK_START:
                case MIDI_CLOCK_CONTINUE:
                case MIDI_CLOCK_STOP:
                    iface_midi_router_handle_clock_msg(&msg);
                    break;
            }
        }
    }
    // USB DEV IN 2 - pass to USB HOST OUT
    while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN2)) {
        midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN2, &msg);
        msg.port = MIDI_PORT_USB_HOST_OUT;
        midi_stream_send_msg(&msg);
    }
    // USB DEV IN 3 - pass to DIN1 OUT
    while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN3)) {
        midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN3, &msg);
        msg.port = MIDI_PORT_DIN1_OUT;
        midi_stream_send_msg(&msg);
    }
    // USB DEV IN 4 - pass to DIN2 OUT
    while(midi_stream_data_available(MIDI_PORT_USB_DEV_IN4)) {
        midi_stream_receive_msg(MIDI_PORT_USB_DEV_IN4, &msg);
        msg.port = MIDI_PORT_DIN2_OUT;
        midi_stream_send_msg(&msg);
    }
}

// handle state change
void iface_midi_router_handle_state_change(int event_type,
        int *data, int data_len) {
    switch(event_type) {
        case SCE_CONFIG_LOADED:
            iface_midi_router_load_config();
            break;
        case SCE_CONFIG_CLEARED:
            iface_midi_router_clear_config();
            break;
    }
}

//
// local functions
//
// clear the config to default values
void iface_midi_router_clear_config(void) {
    // set defaults
    iface_midi_router_set_program(IFACE_MIDI_ROUTER_CV_PROGRAM_DEFAULT);
    iface_midi_router_set_bend_range(IFACE_MIDI_ROUTER_CV_BEND_RANGE_DEFAULT);
    iface_midi_router_set_clock_div(IFACE_MIDI_ROUTER_CLOCK_DIV_DEFAULT);
}

// cause the MIDI router to load its config
void iface_midi_router_load_config(void) {
    // load from config
    iface_midi_router_set_program(config_store_get_val(CONFIG_STORE_IFACE_CV_PROGRAM));
    iface_midi_router_set_bend_range(config_store_get_val(CONFIG_STORE_IFACE_CV_BEND_RANGE));
    iface_midi_router_set_clock_div(config_store_get_val(CONFIG_STORE_IFACE_ANALOG_CLOCK_DIV));
}

// set the MIDI program and write back config
void iface_midi_router_set_program(int prog) {
    if(prog < IFACE_MIDI_ROUTER_CV_PROGRAM_MIN ||
            prog > IFACE_MIDI_ROUTER_CV_PROGRAM_MAX) {
        log_error("imrsp - prog invalid: %d", prog);
        return;
    }
    config_store_set_val(CONFIG_STORE_IFACE_CV_PROGRAM, prog);
    switch(prog) {
        case 0:  // program 1     - ABCD mode - A = note, B = note, C = note, D = note
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_D, CVPROC_MODE_NOTE);
            break;
        case 1:  // program 2     - ABCD mode - A = note, B = velo, C = note, D = note
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_D, CVPROC_MODE_NOTE);
            break;
        case 2:  // program 3     - ABCD mode - A = note, B = velo, C = note, D = velo
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_D, CVPROC_MODE_VELO);
            break;
        case 3:  // program 4     - ABCD mode - A = velo, B = velo, C = velo, D = velo
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_D, CVPROC_MODE_VELO);
            break;
        case 4:  // program 5     - ABCD mode - A = note, B = note, C = CC #1, D = CC #16
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_C, 1);  // CC #1
            cvproc_set_pair_mode(CVPROC_PAIR_D, 16); // CC #16
            break;
        case 5:  // program 6     - ABCD mode - A = note, B = velo, C = CC #1, D = CC #16
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_C, 1);  // CC #1
            cvproc_set_pair_mode(CVPROC_PAIR_D, 16); // CC #16
            break;
        case 6:  // program 7     - ABCD mode - A = note, B = CC #16, C = CC #17, D = CC #18
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, 16);  // CC #16
            cvproc_set_pair_mode(CVPROC_PAIR_C, 17);  // CC #17
            cvproc_set_pair_mode(CVPROC_PAIR_D, 18); // CC #18
            break;
        case 7:  // program 8     - ABCD mode - A = CC #1, B = CC #16, C = CC #17, D = CC #18
            cvproc_set_pairs(CVPROC_PAIRS_ABCD);
            cvproc_set_pair_mode(CVPROC_PAIR_A, 1);  // CC #1
            cvproc_set_pair_mode(CVPROC_PAIR_B, 16);  // CC #16
            cvproc_set_pair_mode(CVPROC_PAIR_C, 17);  // CC #17
            cvproc_set_pair_mode(CVPROC_PAIR_D, 18); // CC #18
            break;
        case 8:  // program 9     - AABC mode - AA = note, B = note, C = note
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_NOTE);
            break;
        case 9:  // program 10    - AABC mode - AA = note, B = note, C = velo
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_VELO);
            break;
        case 10:  // program 11    - AABC mode - AA = velo, B = note, C = note
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_NOTE);
            break;
        case 11:  // program 12    - AABC mode - AA = velo, B = velo, C = velo
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_C, CVPROC_MODE_VELO);
            break;
        case 12:  // program 13    - AABC mode - AA = note, B = note, C = CC #1
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_C, 1);  // CC #1
            break;
        case 13:  // program 14    - AABC mode - AA = note, B = velo, C = CC #1
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_C, 1);  // CC #1
            break;
        case 14:  // program 15    - AABC mode - AA = note, B = CC #1, C = CC #16
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, 1);  // CC #1
            cvproc_set_pair_mode(CVPROC_PAIR_C, 16);  // CC #16
            break;
        case 15:  // program 16    - AABC mode - AA = velo, B = CC #1, C = CC #16
            cvproc_set_pairs(CVPROC_PAIRS_AABC);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_B, 1);  // CC #1
            cvproc_set_pair_mode(CVPROC_PAIR_C, 16);  // CC #16
            break;
        case 16:  // program 17    - AABB mode - AA = note, BB = note
            cvproc_set_pairs(CVPROC_PAIRS_AABB);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_NOTE);
            break;
        case 17:  // program 18    - AABB mode - AA = note, BB = velo
            cvproc_set_pairs(CVPROC_PAIRS_AABB);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            break;
        case 18:  // program 19    - AABB mode - AA = velo, BB = velo
            cvproc_set_pairs(CVPROC_PAIRS_AABB);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_VELO);
            cvproc_set_pair_mode(CVPROC_PAIR_B, CVPROC_MODE_VELO);
            break;
        case 19:  // program 25    - AAAA mode - AAAA = note
            cvproc_set_pairs(CVPROC_PAIRS_AAAA);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_NOTE);
            break;
        case 20:  // program 26    - AAAA mode - AAAA = velo
            cvproc_set_pairs(CVPROC_PAIRS_AAAA);
            cvproc_set_pair_mode(CVPROC_PAIR_A, CVPROC_MODE_VELO);
            break;
    }
}

// set the MIDI bend range and write back config
void iface_midi_router_set_bend_range(int bend) {
    if(bend < IFACE_MIDI_ROUTER_CV_BEND_RANGE_MIN ||
            bend > IFACE_MIDI_ROUTER_CV_BEND_RANGE_MAX) {
        log_error("imrsbr - bend invalid: %d", bend);
        return;
    }
    config_store_set_val(CONFIG_STORE_IFACE_CV_BEND_RANGE, bend);
    cvproc_set_bend_range(bend);
}

// set the MIDI clock divider and write back cd
void iface_midi_router_set_clock_div(int div) {
    if(div < SEQ_UTILS_CLOCK_OFF || div > SEQ_UTILS_CLOCK_24PPQ) {
        log_error("imrscd - div invalid: %d", div);
        return;
    }
    config_store_set_val(CONFIG_STORE_IFACE_ANALOG_CLOCK_DIV, div);
    ifacemrs.analog_clock_div = seq_utils_clock_pqq_to_divisor(div) /
        MIDI_CLOCK_UPSAMPLE;
}

// handle CC messages for controlling interface functions
void iface_midi_router_handle_cv_setting(struct midi_msg *msg) {
    if((msg->status & 0xf0) != MIDI_CONTROL_CHANGE) {
        return;
    }
    switch(msg->data0) {
        case 20:  // CV bend range
            iface_midi_router_set_bend_range(seq_utils_clamp((msg->data1 + 1),
                IFACE_MIDI_ROUTER_CV_BEND_RANGE_MIN,
                IFACE_MIDI_ROUTER_CV_BEND_RANGE_MAX));
            break;
        case 21:  // clock divider
            iface_midi_router_set_clock_div(seq_utils_clamp(msg->data1,
                SEQ_UTILS_CLOCK_OFF,
                SEQ_UTILS_CLOCK_24PPQ));
            break;
    }
}

//
// clock handling
//
// run the MIDI clock task to handle timeouts of pulses, etc.
void iface_midi_router_clock_task(void) {
    // time out clock / reset output pulses - analog output only
    if(ifacemrs.analog_clock_delay_trigger &&
            ifacemrs.analog_reset_timeout == 0) {
        ifacemrs.analog_clock_delay_trigger = 0;
        if(ifacemrs.analog_clock_enable) {
            analog_out_set_clock(1);
            ifacemrs.analog_clock_timeout = (CLOCK_OUT_PULSE_LEN + 1);
        }
    }
    if(ifacemrs.analog_clock_timeout) {
        ifacemrs.analog_clock_timeout --;
        if(ifacemrs.analog_clock_timeout == 0) {
            analog_out_set_clock(0);
        }
    }
    if(ifacemrs.analog_reset_timeout) {
        ifacemrs.analog_reset_timeout --;
        if(ifacemrs.analog_reset_timeout == 0) {
            analog_out_set_reset(0);
        }
    }
}

// handle a clock message received from the host
void iface_midi_router_handle_clock_msg(struct midi_msg *msg) {
    // clock is off
    if(ifacemrs.analog_clock_div == 0) {
        return;
    }

    switch(msg->status) {
        case MIDI_TIMING_TICK:
            // time to issue a clock pulse - we are running
            if(ifacemrs.analog_clock_div_count == 0 &&
                    ifacemrs.analog_clock_enable == 1) {
                // reset is already activated - need to defer it
                if(ifacemrs.analog_reset_timeout) {
                    ifacemrs.analog_clock_delay_trigger = 1;  // defer the pulse
                }
                // normal clock pulse
                else {
                    ifacemrs.analog_clock_timeout = (CLOCK_OUT_PULSE_LEN + 1);
                    analog_out_set_clock(1);
                }
            }
            // keep track of clock dividing
            ifacemrs.analog_clock_div_count ++;
            if(ifacemrs.analog_clock_div_count >= ifacemrs.analog_clock_div) {
                ifacemrs.analog_clock_div_count = 0;
            }
            break;
        case MIDI_CLOCK_START:
            ifacemrs.analog_clock_enable = 1;
            ifacemrs.analog_clock_div_count = 0;
            ifacemrs.analog_reset_timeout = (CLOCK_OUT_PULSE_LEN + 1);
            analog_out_set_reset(1);
            break;
        case MIDI_CLOCK_CONTINUE:
            ifacemrs.analog_clock_enable = 1;
            break;
        case MIDI_CLOCK_STOP:
            ifacemrs.analog_clock_enable = 0;
            break;
    }
}
