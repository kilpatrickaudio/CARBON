/*
 * MIDI Utils
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2014: Kilpatrick Audio
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
#include "midi_utils.h"
#include "../util/log.h"
#include <stdlib.h>

//
// MIDI message helpers
//
// overwrite a channel (for voice messages) and port (all messages)
void midi_utils_rewrite_dest(struct midi_msg *msg, int port, int channel) {
    switch(msg->status & 0xf0) {
        case MIDI_NOTE_OFF:
        case MIDI_NOTE_ON:
        case MIDI_POLY_KEY_PRESSURE:
        case MIDI_CONTROL_CHANGE:
        case MIDI_PROGRAM_CHANGE:
        case MIDI_CHANNEL_PRESSURE:
        case MIDI_PITCH_BEND:
            msg->status = (msg->status & 0xf0) | (channel & 0x0f);
            break;
        default:
            // no channel can be rewritten for non-voice messages
            break;
    }
    msg->port = port;
}

// rewrite a note on message with velocity 0 into a note off message
// otherwise just do nothing
void midi_utils_rewrite_note_off(struct midi_msg *msg) {
    if((msg->status & 0xf0) == MIDI_NOTE_ON &&
            msg->data1 == 0x00) {
        msg->status = MIDI_NOTE_OFF | (msg->status & 0x0f);
        msg->data1 = 0x40;
    }
}

// convert a note on into a note off message
void midi_utils_note_on_to_off(struct midi_msg *msg) {
    if((msg->status & 0xf0) == MIDI_NOTE_ON) {
        msg->status = MIDI_NOTE_OFF | (msg->status & 0x0f);
        msg->data1 = 0x40;
    }
}

// copy a midi message
void midi_utils_copy_msg(struct midi_msg *dest, struct midi_msg *src) {
    dest->port = src->port;
    dest->len = src->len;
    dest->status = src->status;
    dest->data0 = src->data0;
    dest->data1 = src->data1;
}

// compare two messages to see if the contents match
int midi_utils_compare_msg(struct midi_msg *msg1, struct midi_msg *msg2) {
    if(msg1->port != msg2->port) {
        return 0;
    }
    if(msg1->len != msg2->len) {
        return 0;
    }
    if(msg1->status != msg2->status) {
        return 0;
    }
    if(msg1->data0 != msg2->data0) {
        return 0;
    }
    if(msg1->data1 != msg2->data1) {
        return 0;
    }
    return 1;
}

// compare two note messages to see if one is the note off to the preceeding note on
// assumes the note off / are the same other than status high nibble and the velocity
int midi_utils_compare_note_msg(struct midi_msg *note_on, struct midi_msg *note_off) {
    if(note_on->port != note_off->port) {
        return 0;
    }
    if(note_on->len != note_off->len) {
        return 0;
    }
    if((note_on->status & 0xf0) != MIDI_NOTE_ON) {
        return 0;
    }
    if((note_off->status & 0xf0) != MIDI_NOTE_OFF) {
        return 0;
    }
    if((note_on->status & 0x0f) != (note_off->status & 0x0f)) {
        return 0;
    }
    if(note_on->data0 != note_off->data0) {
        return 0;
    }
    return 1;

}

// print out the contents of a midi message
void midi_utils_print_msg(struct midi_msg *msg) {
    log_debug("mupm - prt: %d - len: %d - " \
        "ch: %d - st: %02x - d0: %02x - d1: %02x",
        msg->port,
        msg->len,
        (msg->status & 0x0f),
        msg->status,
        msg->data0,
        msg->data1);
}

// check if a message is probably part of a SYSEX message
// - if the message has a status byte of 0xf0
// - if the message contains 0xf7
// - if the message has a status byte that does not have MSB set
// returns 1 if the message is probably a SYSEX message, or 0 otherwise
int midi_utils_is_sysex_msg(struct midi_msg *msg) {
    if(msg->status == MIDI_SYSEX_START) {
        return 1;
    }
    if(!(msg->status & 0x80)) {
        return 1;
    }
    if(msg->status == 0xf7) {
        return 1;
    }
    if(msg->len > 1 && msg->data0 == 0xf7) {
        return 1;
    }
    if(msg->len > 2 && msg->data1 == 0xf7) {
        return 1;
    }
    return 0;
}

// check if a message is a clock message
// - if the message is a start, continue, stop, tick or song position msg
// returns 1 if the message is a clock message, or 0 otherwise
int midi_utils_is_clock_msg(struct midi_msg *msg) {
    switch(msg->status) {
        case MIDI_SONG_POSITION:
        case MIDI_TIMING_TICK:
        case MIDI_CLOCK_START:
        case MIDI_CLOCK_CONTINUE:
        case MIDI_CLOCK_STOP:
            return 1;
    }
    return 0;
}


//
// MIDI event helpers
//
// copy a midi event
void midi_utils_copy_event(struct midi_event *dest, struct midi_event *src) {
    dest->tick_pos = src->tick_pos;
    dest->tick_len = src->tick_len;
    dest->msg.port = src->msg.port;
    dest->msg.len = src->msg.len;
    dest->msg.status = src->msg.status;
    dest->msg.data0 = src->msg.data0;
    dest->msg.data1 = src->msg.data1;
}

//
// MIDI message encoding routines
//
// encode note on
void midi_utils_enc_note_on(struct midi_msg *msg, int port, int channel, 
        int note, int velocity) {
    msg->port = port;
    msg->len = 3;
    msg->status = MIDI_NOTE_ON | (channel & 0x0f);
    msg->data0 = note & 0x7f;
    msg->data1 = velocity & 0x7f;
}

// encode note off
void midi_utils_enc_note_off(struct midi_msg *msg, int port, int channel, 
        int note, int velocity) {
    msg->port = port;
    msg->len = 3;
    msg->status = MIDI_NOTE_OFF | (channel & 0x0f);
    msg->data0 = note & 0x7f;
    msg->data1 = velocity & 0x7f;
}

// encode poly key pressure
void midi_utils_enc_key_pressure(struct midi_msg *msg, int port, int channel, 
        int note, int pressure) {
    msg->port = port;
    msg->len = 3;
    msg->status = MIDI_POLY_KEY_PRESSURE | (channel & 0x0f);
    msg->data0 = note & 0x7f;
    msg->data1 = pressure & 0x7f;
}

// encode control change
void midi_utils_enc_control_change(struct midi_msg *msg, int port, int channel, 
        int controller, int value) {
    msg->port = port;
    msg->len = 3;
    msg->status = MIDI_CONTROL_CHANGE | (channel & 0x0f);
    msg->data0 = controller & 0x7f;
    msg->data1 = value & 0x7f;
}

// encode program change
void midi_utils_enc_program_change(struct midi_msg *msg, int port, int channel, 
        int program) {
    msg->port = port;
    msg->len = 2;
    msg->status = MIDI_PROGRAM_CHANGE | (channel & 0x0f);
    msg->data0 = program & 0x7f;
}

// encode channel pressure
void midi_utils_enc_channel_pressure(struct midi_msg *msg, int port, int channel, 
        int pressure) {
    msg->port = port;
    msg->len = 2;
    msg->status = MIDI_CHANNEL_PRESSURE | (channel & 0x0f);
    msg->data0 = pressure & 0x7f;
}

// encode pitch bend
void midi_utils_enc_pitch_bend(struct midi_msg *msg, int port, int channel, 
        int bend) {
    msg->port = port;
    msg->len = 3;
    msg->status = MIDI_PITCH_BEND | (channel & 0x0f);
    msg->data0 = (bend + 8192) & 0x7f;
    msg->data1 = ((bend + 8192) >> 7) & 0x7f;
}

// encode mtc qframe
void midi_utils_enc_mtc_qframe(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_MTC_QFRAME;
}

// encode song position
void midi_utils_enc_song_position(struct midi_msg *msg, int port, int pos) {
    msg->port = port;
    msg->len = 3;
    msg->status = MIDI_SONG_POSITION;
    msg->data0 = pos & 0x7f;
    msg->data1 = (pos >> 7) & 0x7f;
}

// encode song select
void midi_utils_enc_song_select(struct midi_msg *msg, int port, int song) {
    msg->port = port;
    msg->len = 2;
    msg->status = MIDI_SONG_SELECT;
    msg->data0 = song & 0x7f;
}

// encode tune request
void midi_utils_enc_tune_request(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_TUNE_REQUEST;
}

// encode timing tick
void midi_utils_enc_timing_tick(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_TIMING_TICK;
}

// encode clock start
void midi_utils_enc_clock_start(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_CLOCK_START;
}

// encode clock continue
void midi_utils_enc_clock_continue(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_CLOCK_CONTINUE;
}

// encode clock stop
void midi_utils_enc_clock_stop(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_CLOCK_STOP;
}

// encode active sensing
void midi_utils_enc_active_sensing(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_ACTIVE_SENSING;
}

// encode system reset
void midi_utils_enc_system_reset(struct midi_msg *msg, int port) {
    msg->port = port;
    msg->len = 1;
    msg->status = MIDI_SYSTEM_RESET;
}

// encode an arbitrary message with 1 byte
void midi_utils_enc_1byte(struct midi_msg *msg, int port, int status) {
    msg->port = port;
    msg->len = 1;
    msg->status = status;
}

// encode an arbitrary message with 2 bytes
void midi_utils_enc_2byte(struct midi_msg *msg, int port, int status,
        int data0) {
    msg->port = port;
    msg->len = 2;
    msg->status = status;
    msg->data0 = data0;
}

// encode an arbitrary message with 3 bytes
void midi_utils_enc_3byte(struct midi_msg *msg, int port, int status,
        int data0, int data1) {
    msg->port = port;
    msg->len = 3;
    msg->status = status;
    msg->data0 = data0;
    msg->data1 = data1;
}
