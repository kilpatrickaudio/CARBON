/*
 * CARBON Sequencer Panel Interface Driver
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
#ifndef PANEL_IF_H
#define PANEL_IF_H

#include <inttypes.h>

// init the panel interface
void panel_if_init(void);

// run the panel interface timer task
void panel_if_timer_task(void);

// set an LED brightness level - level: 0-255 = 0-100%
// setting this cancels blinking
void panel_if_set_led(int led, uint8_t level);

// set an RGB LED - side: 0 = left, 1 = right
// color - 0xXXRRGGBB
void panel_if_set_rgb(int side, uint32_t color);

// set an LED to blink
void panel_if_blink_led(int led, uint8_t off, uint8_t on);

#endif

