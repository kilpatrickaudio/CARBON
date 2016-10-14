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
 */
#ifndef MIDI_STREAM_H
#define MIDI_STREAM_H

#include <inttypes.h>
#include "midi_utils.h"

// settings
#define MIDI_STREAM_BUFSIZE 256  // msg queue size (must be a power of 2)
#define MIDI_STREAM_BUFMASK (MIDI_STREAM_BUFSIZE - 1)
#define MIDI_STREAM_SYSEX_MAXLEN 200  // bytes

// init the MIDI streams
void midi_stream_init(void);

// put a message into a stream - the msg will be copied
// returns 0 on success and -1 if the stream is full, -2 if the port is invalid
int midi_stream_send_msg(struct midi_msg *msg);

// put a SYSEX message into a stream - the data will be copied
// data is converted in 1-3 byte chunks and sent in midi_message structs
// returns 0 on success and -1 if the stream is full or msg too long
// returns -2 if the port is invalid
int midi_stream_send_sysex_msg(int port, uint8_t *buf, int len);

// put a byte into a stream - bytes are stored and inputted into
// the stream as complete messages based on their contents
// returns 0 on success and -1 if the stream is full, -2 if the port is invalid
int midi_stream_send_byte(int port, uint8_t send_byte);

// check if there are messages in the stream
// returns 1 if there is data available, -1 if port is invalid, otherwise returns 0
int midi_stream_data_available(int port);

// get a message from a stream - the msg will be copied
// returns 0 on success and -1 if there is no data to get, -2 if port is invalid
int midi_stream_receive_msg(int port, struct midi_msg *msg);

#endif

