/*
 * CARBON Sequencer Graphics Routines
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
#ifndef GFX_H
#define GFX_H

#include <inttypes.h>
#include "config.h"

struct gfx_label {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t font;
    uint32_t fg_color;
    uint32_t bg_color;
    char text[GFX_LABEL_LEN];  // text string
    char highlight[GFX_LABEL_LEN];  // highlight mode for each char
    uint8_t dirty;
};

// highlight modes
#define GFX_HIGHLIGHT_MAX_MODES 2
#define GFX_HIGHLIGHT_NORMAL 0
#define GFX_HIGHLIGHT_INVERT 1

// fonts
#define GFX_FONT_SMALLTEXT_8X10 0  // new regular font
#define GFX_FONT_SYSTEM_8X12 1  // original screen
#define GFX_FONT_SYSTEM_8X13 2  // higher DPI screen

// init the graphics
int gfx_init(void);

// destroy the graphics
void gfx_close(void);

// commit the changes to the screen
void gfx_commit(void);

// init the actual LCD (controls power) - this takes a long time
void gfx_init_lcd(void);

// deinit the actual LCD (controls power)
void gfx_deinit_lcd(void);

// clear the screen and commit changes
void gfx_clear_screen(uint32_t color);

// get the screen width
int gfx_get_screen_w(void);

// get the screen height
int gfx_get_screen_h(void);

// draw a filled rectangle
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);

// load a font
int gfx_load_font(int num, char *font_filename, int size);

// draw text to the screen
void gfx_draw_string(struct gfx_label *label);

#endif

