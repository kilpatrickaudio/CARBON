/*
 * ILI9486/8 LCD Driver
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
#ifndef ILI948x_DRV_H
#define ILI948x_DRV_H

#include <inttypes.h>

// init the LCD driver
void ILI948x_drv_init(void);

// init the actual LCD - this takes a long time
void ILI948x_drv_init_LCD(void);

// deinit the LCD
void ILI948x_drv_deinit_LCD(void);

// get the ID (model) of the display
uint32_t ILI948x_drv_get_id(void);

// set the area for drawing
void ILI948x_drv_set_xy(int x, int y, int w, int h);

// clear the screen to the specified color
void ILI948x_drv_clear(uint16_t color);

// send pixel data to the LCD
void ILI948x_drv_send_pixels(uint16_t *fb, int len);

#endif

