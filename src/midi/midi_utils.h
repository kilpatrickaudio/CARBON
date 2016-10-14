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
#ifndef MIDI_UTILS_H
#define MIDI_UTILS_H

#include <inttypes.h>
#include <sys/queue.h>
#include "midi_protocol.h"

// MIDI message
struct midi_msg {
    int port;  // the port number - 0 if not used
	uint8_t len;  // the message length in bytes
    uint8_t status;  // status byte
    uint8_t data0;  // data0 byte
    uint8_t data1;  // data1 byte
};

// MIDI event data
struct midi_event {
    LIST_ENTRY(midi_event) events;
    uint32_t tick_pos;  // position of this event
    uint32_t tick_len;  // length of this event (specifically for notes)
    struct midi_msg msg;  // the MIDI message for this event
};

// MIDI source or destination port / channel / control
struct midi_target {
	int port;
	int channel;
	int control;
};

//
// MIDI message helpers
//
// overwrite a channel (for voice messages) and port (all messages)
void midi_utils_rewrite_dest(struct midi_msg *msg, int port, int channel);

// rewrite a note on message with velocity 0 into a note off message
// otherwise just do nothing
void midi_utils_rewrite_note_off(struct midi_msg *msg);

// convert a note on into a note off message
void midi_utils_note_on_to_off(struct midi_msg *msg);

// copy a midi message
void midi_utils_copy_msg(struct midi_msg *dest, struct midi_msg *src);

// compare two messages to see if the contents match
int midi_utils_compare_msg(struct midi_msg *msg1, struct midi_msg *msg2);

// compare two note messages to see if one is the note off to the preceeding note on
// assumes the note off / are the same other than status high nibble and the velocity
// ignores the tick_pos
int midi_utils_compare_note_msg(struct midi_msg *note_on, struct midi_msg *note_off);

// print out the contents of a midi message
void midi_utils_print_msg(struct midi_msg *msg);

// check if a message is probably part of a SYSEX message
// - if the message has a status byte of 0xf0
// - if the message contains 0xf7
// - if the message has a status byte that does not have MSB set
// returns 1 if the message is probably a SYSEX message, or 0 otherwise
int midi_utils_is_sysex_msg(struct midi_msg *msg);

// check if a message is a clock message
// - if the message is a start, continue, stop, tick or song position msg
// returns 1 if the message is a clock message, or 0 otherwise
int midi_utils_is_clock_msg(struct midi_msg *msg);

//
// MIDI event helpers
//
// copy a midi event
void midi_utils_copy_event(struct midi_event *dest, struct midi_event *src);

//
// MIDI message encoding routines
//
// encode note on
void midi_utils_enc_note_on(struct midi_msg *msg, int port, int channel, 
        int note, int velocity);

// encode note off
void midi_utils_enc_note_off(struct midi_msg *msg, int port, int channel, 
        int note, int velocity);

// encode poly key pressure
void midi_utils_enc_key_pressure(struct midi_msg *msg, int port, int channel, 
        int note, int pressure);

// encode control change
void midi_utils_enc_control_change(struct midi_msg *msg, int port, int channel, 
        int controller, int value);

// encode program change
void midi_utils_enc_program_change(struct midi_msg *msg, int port, int channel, 
        int program);

// encode channel pressure
void midi_utils_enc_channel_pressure(struct midi_msg *msg, int port, int channel, 
        int pressure);

// encode pitch bend
void midi_utils_enc_pitch_bend(struct midi_msg *msg, int port, int channel, 
        int bend);

// encode mtc qframe
void midi_utils_enc_mtc_qframe(struct midi_msg *msg, int port);

// encode song position
void midi_utils_enc_song_position(struct midi_msg *msg, int port, int pos);

// encode song select
void midi_utils_enc_song_select(struct midi_msg *msg, int port, int song);

// encode tune request
void midi_utils_enc_tune_request(struct midi_msg *msg, int port);

// encode timing tick
void midi_utils_enc_timing_tick(struct midi_msg *msg, int port);

// encode clock start
void midi_utils_enc_clock_start(struct midi_msg *msg, int port);

// encode clock continue
void midi_utils_enc_clock_continue(struct midi_msg *msg, int port);

// encode clock stop
void midi_utils_enc_clock_stop(struct midi_msg *msg, int port);

// encode active sensing
void midi_utils_enc_active_sensing(struct midi_msg *msg, int port);

// encode system reset
void midi_utils_enc_system_reset(struct midi_msg *msg, int port);

// encode an arbitrary message with 1 byte
void midi_utils_enc_1byte(struct midi_msg *msg, int port, int status);

// encode an arbitrary message with 2 bytes
void midi_utils_enc_2byte(struct midi_msg *msg, int port, int status,
        int data0);

// encode an arbitrary message with 3 bytes
void midi_utils_enc_3byte(struct midi_msg *msg, int port, int status,
        int data0, int data1);

#endif
