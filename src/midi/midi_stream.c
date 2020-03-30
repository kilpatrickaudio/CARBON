/*
 * MIDI Stream Handler - Parse and Generate MIDI Byte Streams
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
 * Description:
 *
 * This is a MIDI stream handler that can deal with MIDI data going various
 * places within a system. The data is queued inside this module and can be
 * collected as needed by a consumer. If a queue is full data is not entered
 * and an error is returned.
 *
 * SYSEX Messages:
 * 
 * SYSEX messages do not fit within a single struct midi_msg type so they are
 * handled in chunks. When sending a SYSEX messge the message is broken up into
 * chunks of 1-3 bytes each. The original message must contain the framing bytes
 * 0xf0 and 0xf7. In this case the status bytes of each message may contain user
 * data. Inserting realtime data within the stream is not supported.
 *
 * When consuming messages that may contain SYSEX messages the status byte in
 * each message should be used to distinguish between SYSEX and other kinds of
 * messages. A special parser should be made in the consuming module that 
 * receives data and stores it until a complete SYSEX message is received or
 * another valid status byte is encountered.
 *
 */
#include "midi_stream.h"
#include "../config.h"
#include "midi_protocol.h"
#include "../util/log.h"
#include <stdlib.h>

// state
#define MIDI_STREAM_BYTE_IDLE 0
#define MIDI_STREAM_BYTE_DATA0 1
#define MIDI_STREAM_BYTE_DATA1 2
#define MIDI_STREAM_BYTE_SYSEX_DATA0 3
#define MIDI_STREAM_BYTE_SYSEX_DATA1 4
uint8_t midi_stream_send_byte_state[MIDI_MAX_PORTS];  // byte stream state

// message queues
// put data into CCMRAM instead of regular RAM
struct midi_msg midi_stream_queue[MIDI_MAX_PORTS][MIDI_STREAM_BUFSIZE] __attribute__ ((section (".ccm")));
int midi_stream_queue_inp[MIDI_MAX_PORTS];
int midi_stream_queue_outp[MIDI_MAX_PORTS];

// init the MIDI streams
void midi_stream_init(void) {
    int i;
    for(i = 0; i < MIDI_MAX_PORTS; i ++) {
        midi_stream_send_byte_state[i] = MIDI_STREAM_BYTE_IDLE;
        midi_stream_queue_inp[i] = 0;
        midi_stream_queue_outp[i] = 0;
    }
}

// put a message into a stream - the msg will be copied
// returns 0 on success and -1 if the stream is full, -2 if the port is invalid
int midi_stream_send_msg(struct midi_msg *msg) {
    if(msg->port < 0 || msg->port >= MIDI_MAX_PORTS) {
        log_error("mssm - port invalid: %d", msg->port);
        return -2;
    }
    // check if the buffer is full
    if(((midi_stream_queue_inp[msg->port] - midi_stream_queue_outp[msg->port]) &
            MIDI_STREAM_BUFMASK) == (MIDI_STREAM_BUFSIZE - 1)) {
        return -1;
    }
    midi_utils_copy_msg(&midi_stream_queue[msg->port][midi_stream_queue_inp[msg->port]], msg);
    midi_stream_queue_inp[msg->port] = 
        (midi_stream_queue_inp[msg->port] + 1) & MIDI_STREAM_BUFMASK;      
    return 0;
}

// put a SYSEX message into a stream - the data will be copied
// data is converted in 1-3 byte chunks and sent in midi_message structs
// returns 0 on success and -1 if the stream is full or msg too long
// returns -2 if the port is invalid
int midi_stream_send_sysex_msg(int port, uint8_t *buf, int len) {
    int num_msg, i;
    struct midi_msg msg;
    if(port < 0 || port >= MIDI_MAX_PORTS) {
        log_error("msssm - port invalid: %d", port);
        return -2;
    }
    if(len < 1 || len > MIDI_STREAM_SYSEX_MAXLEN) {
        return -1;
    }
    // check if the buffer is full
    // we can fit up to 3 bytes in each MIDI message
    num_msg = (len / 3);
    if(len % 3) {
        num_msg ++;
    }
    if(((midi_stream_queue_inp[port] - midi_stream_queue_outp[port]) &&
            MIDI_STREAM_BUFMASK) >= (MIDI_STREAM_BUFSIZE - num_msg)) {
        return -1;
    }
    msg.port = port;
    for(i = 0; i < len; i += 3) {
        if(len - i >= 3) {
            msg.len = 3;
        }
        else {
            msg.len = (len - i);
        }
        msg.status = buf[i];
        msg.data0 = buf[i+1];
        msg.data1 = buf[i+2];
        midi_stream_send_msg(&msg);
    }
    return 0;
}

// put a byte into a stream - bytes are stored and inputted into
// the stream as complete messages based on their contents
// returns 0 on success and -1 if the stream is full, -2 if the port is invalid
int midi_stream_send_byte(int port, uint8_t send_byte) {
    struct midi_msg msg;
    int stat, chan;
    static uint8_t rx_chan[MIDI_MAX_PORTS];  // current message channel
    static uint8_t rx_status[MIDI_MAX_PORTS];  // current status byte
    static uint8_t rx_data0[MIDI_MAX_PORTS];  // data0 byte
    static uint8_t rx_data1[MIDI_MAX_PORTS];  // data1 byte
   
    if(port < 0 || port >= MIDI_MAX_PORTS) {
        log_error("mssb - port invalid: %d", port);
        return -2;
    }
   
    // XXX fix to work with running status properly
    // status byte received
    if(send_byte & 0x80) {
        stat = (send_byte & 0xf0);
        chan = (send_byte & 0x0f);
        
        // system messages
        if(stat == 0xf0) {
            switch(send_byte) {
                //
                // system common messages (non-SYSEX)
                //
                case MIDI_MTC_QFRAME:
                    // not supported on receive
                    break;
                case MIDI_SONG_POSITION:
                case MIDI_SONG_SELECT:
                    rx_chan[port] = 255;  // clear running status channel
                    rx_status[port] = send_byte;
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_DATA0;
                    break;
                case MIDI_TUNE_REQUEST:
                    midi_utils_enc_tune_request(&msg, port);
                    break;            
                //
                // system realtime messages - does not reset running status
                //
                case MIDI_TIMING_TICK:
                    midi_utils_enc_timing_tick(&msg, port);
                    midi_stream_send_msg(&msg);
                    break;
                case MIDI_CLOCK_START:
                    midi_utils_enc_clock_start(&msg, port);
                    midi_stream_send_msg(&msg);
                    break;
                case MIDI_CLOCK_CONTINUE:
                    midi_utils_enc_clock_continue(&msg, port);
                    midi_stream_send_msg(&msg);
                    break;
                case MIDI_CLOCK_STOP:
                    midi_utils_enc_clock_stop(&msg, port);
                    midi_stream_send_msg(&msg);
                    break;
                case MIDI_ACTIVE_SENSING:
                    midi_utils_enc_active_sensing(&msg, port);
                    midi_stream_send_msg(&msg);
                    break;
                case MIDI_SYSTEM_RESET:
                    rx_chan[port] = 255;  // clear running status channel
                    rx_status[port] = 0;  // clear running status
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_IDLE;
                    midi_utils_enc_system_reset(&msg, port);
                    midi_stream_send_msg(&msg);
                    break;
                //
                // SYSEX messages
                //
                case MIDI_SYSEX_START:                    
                    rx_chan[port] = 255;  // clear running status channel
                    rx_status[port] = send_byte;
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_SYSEX_DATA0;
                    break;                    
                case MIDI_SYSEX_END:
                    // 0xf7 received while waiting for 3rd byte
                    if(midi_stream_send_byte_state[port] == MIDI_STREAM_BYTE_SYSEX_DATA1) {
                        midi_utils_enc_3byte(&msg, port, rx_status[port], 
                            rx_data0[port], send_byte);
                    }
                    // 0xf7 received while waiting for 2nd byte
                    else if(midi_stream_send_byte_state[port] == MIDI_STREAM_BYTE_SYSEX_DATA0) {
                        midi_utils_enc_2byte(&msg, port, rx_status[port], send_byte);
                    }
                    // 0xf7 received when no bytes are waiting
                    else {
                        midi_utils_enc_1byte(&msg, port, send_byte);
                    }
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_IDLE;
                    midi_stream_send_msg(&msg);                     
                    break;
                default:
                    // undefined system realtime or common message
                    rx_chan[port] = 255;  // clear running status channel
                    rx_status[port] = 0;  // clear running status
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_IDLE;
                    break;
            }
        }
        // channel messages
        else {
            // do we have a sysex message current receiving?
            if(midi_stream_send_byte_state[port] == MIDI_STREAM_BYTE_SYSEX_DATA0 ||
                    midi_stream_send_byte_state[port] == MIDI_STREAM_BYTE_SYSEX_DATA1) {
                msg.port = port;
                msg.len = 1;
                msg.status = MIDI_SYSEX_END;
                midi_stream_send_msg(&msg);
            }
            // deal with channel messages
            switch(stat) {
                case MIDI_NOTE_OFF:
                case MIDI_NOTE_ON:
                case MIDI_POLY_KEY_PRESSURE:
                case MIDI_CONTROL_CHANGE:
                case MIDI_PROGRAM_CHANGE:
                case MIDI_CHANNEL_PRESSURE:
                case MIDI_PITCH_BEND:
                    rx_chan[port] = chan;
                    rx_status[port] = stat;
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_DATA0;
                    break;
                default:
                    break;
            }
        }
        return 0;
    }

    // non-status byte received
    // deal with states
    switch(midi_stream_send_byte_state[port]) {
        case MIDI_STREAM_BYTE_DATA0:  // waiting on first data byte
            rx_data0[port] = send_byte;
            // figure out what action to do next based on status
            switch(rx_status[port]) {
                case MIDI_SONG_SELECT:
                    midi_utils_enc_song_select(&msg, port, send_byte);
                    midi_stream_send_msg(&msg);
                    rx_chan[port] = 255;  // clear running status channel
                    rx_status[port] = 0;  // clear running status
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_IDLE;
                    break;
                case MIDI_PROGRAM_CHANGE:
                    midi_utils_enc_program_change(&msg, port, rx_chan[port], send_byte);
                    midi_stream_send_msg(&msg);
                    // stay in this state for running status
                    break;
                case MIDI_CHANNEL_PRESSURE:
                    midi_utils_enc_channel_pressure(&msg, port, rx_chan[port], send_byte);
                    midi_stream_send_msg(&msg);
                    // stay in this state for running status
                    break;
                default:
                    // must be a 3 byte message
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_DATA1;
                    break;
            }
            break;
        case MIDI_STREAM_BYTE_DATA1:  // waiting on second data byte
            rx_data1[port] = send_byte;
            // figure out what action to do next based on status
            switch(rx_status[port]) {
                case MIDI_NOTE_OFF:
                    midi_utils_enc_note_off(&msg, port, rx_chan[port], 
                        rx_data0[port], rx_data1[port]);
                    midi_stream_send_msg(&msg);                    
                    break;
                case MIDI_NOTE_ON:
                    // note off encoded as note on with velocity 0
                    if(rx_data1[port] == 0x00) {
                        midi_utils_enc_note_off(&msg, port, rx_chan[port], 
                            rx_data0[port], 0x40);
                    }
                    else {
                        midi_utils_enc_note_on(&msg, port, rx_chan[port], 
                            rx_data0[port], rx_data1[port]);
                    }
                    midi_stream_send_msg(&msg);                    
                    break;
                case MIDI_POLY_KEY_PRESSURE:
                    midi_utils_enc_key_pressure(&msg, port, rx_chan[port], 
                        rx_data0[port], rx_data1[port]);
                    midi_stream_send_msg(&msg); 
                    break;
                case MIDI_CONTROL_CHANGE:
                    midi_utils_enc_control_change(&msg, port, rx_chan[port], 
                        rx_data0[port], rx_data1[port]);
                    midi_stream_send_msg(&msg); 
                    break;
                case MIDI_PITCH_BEND:
                    midi_utils_enc_pitch_bend(&msg, port, rx_chan[port], 
                        ((rx_data1[port] << 7) | rx_data0[port]) - 8192);
                    midi_stream_send_msg(&msg); 
                    break;
                case MIDI_SONG_POSITION:
                    midi_utils_enc_song_position(&msg, port, 
                        rx_data1[port] << 7 | rx_data0[port]);
                    rx_chan[port] = 255;  // clear running status channel
                    rx_status[port] = 0;  // clear running status
                    midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_IDLE;
                    return 0;  // avoid changing running status state back
            }
            midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_DATA0;  // running status
            break;
        case MIDI_STREAM_BYTE_SYSEX_DATA0:  // waiting on first sysex byte
            rx_data0[port] = send_byte;
            midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_SYSEX_DATA1;
            break;
        case MIDI_STREAM_BYTE_SYSEX_DATA1:  // waiting on second sysex byte
            rx_data1[port] = send_byte;
            midi_utils_enc_3byte(&msg, port, rx_status[port], 
                rx_data0[port], rx_data1[port]);
            midi_stream_send_msg(&msg); 
            midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_IDLE;
            break;
        case MIDI_STREAM_BYTE_IDLE:
        default:  // non-status byte received - probably sysex continuation
            rx_status[port] = send_byte;
            midi_stream_send_byte_state[port] = MIDI_STREAM_BYTE_SYSEX_DATA0;
            break;
    }
    return 0;
}

// check if there are messages in the stream
// returns 1 if there is data available, -1 if port is invalid, otherwise returns 0
int midi_stream_data_available(int port) {
    if(port < 0 || port >= MIDI_MAX_PORTS) {
        log_error("msda - port invalid: %d", port);
        return -1;
    }
    if(midi_stream_queue_inp[port] != midi_stream_queue_outp[port]) {
        return 1;
    }
    return 0;
}

// get a message from a stream - the msg will be copied
// returns 0 on success and -1 if there is no data to get, -2 if port is invalid
int midi_stream_receive_msg(int port, struct midi_msg *msg) {
    if(port < 0 || port >= MIDI_MAX_PORTS) {
        log_error("msda - port invalid: %d", port);
        return -2;
    }
    if(midi_stream_queue_inp[port] == midi_stream_queue_outp[port]) {
        return -1;
    }
    midi_utils_copy_msg(msg, &midi_stream_queue[port][midi_stream_queue_outp[port]]);
    midi_stream_queue_outp[port] = (midi_stream_queue_outp[port] + 1) & MIDI_STREAM_BUFMASK;
    return 0;
}


