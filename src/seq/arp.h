/*
 * CARBON Arpeggiator
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
#ifndef ARP_H
#define ARP_H

#include "../midi/midi_utils.h"
#include "arp_progs.h"

// init the arp
void arp_init(void);

// run the arp - called on each clock tick
void arp_run(int tick_count);

// set whether the external sequencer is playing or not
// this affects whether we resync the arp start to our own clock
void arp_set_seq_enable(int enable);

// set the arp enable - 0 = off, 1 = on
void arp_set_arp_enable(int track, int enable);

// handle a input from the keyboard
void arp_handle_input(int track, struct midi_msg *msg);

// clear all input notes and stop notes - live input went away
void arp_clear_input(int track);

// set the arp type
void arp_set_type(int track, int type);

// set the arp speed
void arp_set_speed(int track, int speed);

// set the arp gate time - ticks
void arp_set_gate_time(int track, int time);

//
// helper functions
//
// convert an arp type to a text string
void arp_type_to_name(char *text, int type);

#endif

