/*
 * MIDI Protocol Definitions
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
#ifndef MIDI_PROTOCOL_H
#define MIDI_PROTOCOL_H

// sizes
#define MIDI_MAX_RAW_LEN 1000
#define MIDI_NUM_CHANNELS 16
#define MIDI_NUM_BANKS 16384
#define MIDI_NUM_PROGRAMS 128

// status bytes
#define MIDI_NOTE_OFF 0x80
#define MIDI_NOTE_ON 0x90
#define MIDI_POLY_KEY_PRESSURE 0xa0
#define MIDI_CONTROL_CHANGE 0xb0
#define MIDI_PROGRAM_CHANGE 0xc0
#define MIDI_CHANNEL_PRESSURE 0xd0
#define MIDI_PITCH_BEND 0xe0
#define MIDI_SYSEX_START 0xf0
#define MIDI_MTC_QFRAME 0xf1
#define MIDI_SONG_POSITION 0xf2
#define MIDI_SONG_SELECT 0xf3
#define MIDI_TUNE_REQUEST 0xf6
#define MIDI_SYSEX_END 0xf7
#define MIDI_TIMING_TICK 0xf8
#define MIDI_CLOCK_START 0xfa
#define MIDI_CLOCK_CONTINUE 0xfb
#define MIDI_CLOCK_STOP 0xfc
#define MIDI_ACTIVE_SENSING 0xfe
#define MIDI_SYSTEM_RESET 0xff

// controller changes
#define MIDI_CONTROLLER_BANK_MSB 0
#define MIDI_CONTROLLER_BANK_LSB 32
#define MIDI_CONTROLLER_DAMPER 64
#define MIDI_CONTROLLER_ALL_SOUNDS_OFF 120
#define MIDI_CONTROLLER_ALL_NOTES_OFF 123

#endif
