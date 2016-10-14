/*
 * LCD Parallel FSMC Driver
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
#ifndef LCD_FSMC_IF_H
#define LCD_FSMC_IF_H

#include <inttypes.h>

// init the FSMC module
void lcd_fsmc_if_init(void);

// init the interface
void lcd_fsmc_if_init_if(void);

// deinit the interface
void lcd_fsmc_if_deinit_if(void);

// write to the LCD with 8 bit buffer - len = total bytes
void lcd_fsmc_if_write8(uint8_t *buf, int len, int rs);

// read from the LCD with 8 bit buffer - len = total bytes
void lcd_fsmc_if_read8(uint8_t *buf, int len, int rs);

// write to the LCD with 16 bit buffer - len = total words
void lcd_fsmc_if_write16(uint16_t *buf, int len, int rs);

#endif

