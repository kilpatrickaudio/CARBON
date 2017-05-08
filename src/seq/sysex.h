/*
 * CARBON Sequencer SYSEX Handler
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2015: Kilpatrick Audio
 *
 * This file is part of CARBON.
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
#ifndef SYSEX_H
#define SYSEX_H

#include "../midi/midi_utils.h"

// settings (possible shared by other modules)
// MMA ID
#define SYSEX_MMA_ID0 0x00
#define SYSEX_MMA_ID1 0x01
#define SYSEX_MMA_ID2 0x72


// init the sysex handler
void sysex_init(void);

// handle processing of requests that can take time
void sysex_timer_task(void);

// handle a portion of SYSEX message received
void sysex_handle_msg(struct midi_msg *msg);

#endif

