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
 */
#ifndef IFACE_MIDI_ROUTER_H
#define IFACE_MIDI_ROUTER_H

// settings
#define IFACE_MIDI_ROUTER_PANEL_CTRL_CHAN 15  // channel for panel knob and LED control

// init the interface MIDI router
void iface_midi_router_init(void);

// run the interface MIDI router timer task
void iface_midi_router_timer_task(void);

// handle state change
void iface_midi_router_handle_state_change(int event_type, 
    int *data, int data_len);

#endif

