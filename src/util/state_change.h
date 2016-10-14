/*
 * CARBON State Change Dispatch System
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
#ifndef STATE_CHANGE_H
#define STATE_CHANGE_H

// init the state change system
void state_change_init(void);

// register a listener
void state_change_register(void (*handler)(int, int*, int), int event_class);

// fire an event
void state_change_fire(int event_type, int *data, int data_len);

// fire an event with no arguments
void state_change_fire0(int event_type);

// fire an event with 1 argument
void state_change_fire1(int event_type, int data0);

// fire an event with 2 arguments
void state_change_fire2(int event_type, int data0, int data1);

// fire an event with 3 arguments
void state_change_fire3(int event_type, int data0, int data1, int data2);

#endif

