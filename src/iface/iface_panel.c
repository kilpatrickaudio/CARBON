/*
 * CARBON Interface Panel Handler
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
 * Panel Control:
 *  - panel buttons and encoders are sent on channel 16
 *
 * LED Control:
 *  - LEDs are controlled on channel 16
 *  - the same CC assignments are used as for panel controls
 *  - the following buttons have LEDs which can be controled:
 *    - ARP
 *    - LIVE
 *    - 1-6
 *    - CLOCK
 *    - DIR
 *    - RUN_STOP
 *    - REC
 *    - SONG_MODE
 *  - the backlight RGB LEDs are controlled via the encoder channels:
 *    - the format is bits 2:0 = RGB state
 *    - SPEED - left
 *    - TRANSPOSE - right
 *
 */
#include "iface_panel.h"
#include "iface_midi_router.h"
#include "../config.h"
#include "../gui/panel.h"
#include "../midi/midi_protocol.h"
#include "../midi/midi_stream.h"
#include "../midi/midi_utils.h"
#include "../util/log.h"

// init the interface panel
void iface_panel_init(void) {

}

// handle input to the interface panel
void iface_panel_handle_input(int ctrl, int val) {
    struct midi_msg send_msg;
    // key press / encoder
    if(val) {
        switch(ctrl) {
            case PANEL_SW_SCENE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_SCENE, 127);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_SW_ARP:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_ARP, 127);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_SW_LIVE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_LIVE, 127);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_SW_1:
                midi_utils_enc_control_change(&send_msg,MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_1, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_2:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_2, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_3:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_3, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_4:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_4, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_5:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_5, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_6:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_6, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_MIDI:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_MIDI, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_CLOCK:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_CLOCK, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_DIR:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_DIR, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_TONALITY:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_TONALITY, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_LOAD:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_LOAD, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_RUN_STOP:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_RUN_STOP, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_RECORD:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_RECORD, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_EDIT:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_EDIT, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_SHIFT:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_SHIFT, 127);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_SONG_MODE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_SONG_MODE, 127);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_ENC_SPEED:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_ENC_SPEED, val);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_ENC_GATE_TIME:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_ENC_GATE_TIME, val);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_ENC_MOTION_START:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_ENC_MOTION_START, val);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_ENC_MOTION_LENGTH:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_ENC_MOTION_LENGTH, val);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_ENC_PATTERN_TYPE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_ENC_PATTERN_TYPE, val);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_ENC_TRANSPOSE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_ENC_TRANSPOSE, val);
                midi_stream_send_msg(&send_msg);
                break;
            default:
                log_error("iphi - invalid ctrl: %d", ctrl);
                break;
        }
    }
    // key release
    else {
        switch(ctrl) {
            case PANEL_SW_SCENE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_SCENE, 0);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_SW_ARP:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_ARP, 0);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_SW_LIVE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_LIVE, 0);
                midi_stream_send_msg(&send_msg);
                break;
            case PANEL_SW_1:
                midi_utils_enc_control_change(&send_msg,MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_1, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_2:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_2, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_3:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_3, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_4:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_4, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_5:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_5, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_6:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_6, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_MIDI:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_MIDI, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_CLOCK:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_CLOCK, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_DIR:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_DIR, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_TONALITY:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_TONALITY, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_LOAD:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_LOAD, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_RUN_STOP:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_RUN_STOP, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_RECORD:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_RECORD, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_EDIT:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_EDIT, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_SHIFT:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_SHIFT, 0);
                midi_stream_send_msg(&send_msg);
                break;                
            case PANEL_SW_SONG_MODE:
                midi_utils_enc_control_change(&send_msg, MIDI_IFACE_PANEL_OUTPUT_PORT,
                    IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN, IFACE_PANEL_SW_SONG_MODE, 0);
                midi_stream_send_msg(&send_msg);
                break;
            default:
                log_error("iphi - invalid ctrl: %d", ctrl);
                break;
        }    
    }
}

// LED control
void iface_panel_handle_led(struct midi_msg *msg) {
    int state;
    if((msg->status & 0xf0) != MIDI_CONTROL_CHANGE) {
        return;
    }
    if(msg->data1 > 0x3f) {
        state = PANEL_LED_STATE_ON;
    }
    else {
        state = PANEL_LED_STATE_OFF;
    }
    switch(msg->data0) {
        case IFACE_PANEL_LED_ARP:
            panel_set_led(PANEL_LED_ARP, state);
            break;
        case IFACE_PANEL_LED_LIVE:
            panel_set_led(PANEL_LED_LIVE, state);
            break;
        case IFACE_PANEL_LED_1:
            panel_set_led(PANEL_LED_1, state);
            break;
        case IFACE_PANEL_LED_2:
            panel_set_led(PANEL_LED_2, state);
            break;
        case IFACE_PANEL_LED_3:
            panel_set_led(PANEL_LED_3, state);
            break;
        case IFACE_PANEL_LED_4:
            panel_set_led(PANEL_LED_4, state);
            break;
        case IFACE_PANEL_LED_5:
            panel_set_led(PANEL_LED_5, state);
            break;
        case IFACE_PANEL_LED_6:
            panel_set_led(PANEL_LED_6, state);
            break;
        case IFACE_PANEL_LED_CLOCK:
            panel_set_led(PANEL_LED_CLOCK, state);
            break;
        case IFACE_PANEL_LED_DIR:
            panel_set_led(PANEL_LED_DIR, state);
            break;
        case IFACE_PANEL_LED_RUN_STOP:
            panel_set_led(PANEL_LED_RUN_STOP, state);
            break;
        case IFACE_PANEL_LED_RECORD:
            panel_set_led(PANEL_LED_RECORD, state);
            break;
        case IFACE_PANEL_LED_SONG_MODE:
            panel_set_led(PANEL_LED_SONG_MODE, state);
            break;
        case IFACE_PANEL_LED_BL_LEFT:
            panel_set_bl_led(0, msg->data1 & 0x07);
            break;
        case IFACE_PANEL_LED_BL_RIGHT:
            panel_set_bl_led(1, msg->data1 & 0x07);
            break;
    }
}


