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
#include "gfx.h"
#include "config.h"
#include <string.h>
#include "debug.h"
#include "ILI948x_drv.h"

// fonts
#define GFX_NUM_FONTS 4
char GFX_FONT_WIDTH[] = {8, 8, 8, 9};
char GFX_FONT_HEIGHT[] = {8, 12, 10, 13};
extern const uint8_t font_smalltext_8x8_bitmap[95][8];
extern const uint8_t font_system_8x12_bitmap[95][12];
extern const uint8_t font_smalltext_8x9_bitmap[95][10];
extern const uint8_t font_system_8x13_bitmap[95][13];

// LCD macros
#define LCD_DRV_INIT ILI948x_drv_init
#define LCD_DRV_INIT_LCD ILI948x_drv_init_LCD
#define LCD_DRV_DEINIT_LCD ILI948x_drv_deinit_LCD
#define LCD_DRV_SET_XY ILI948x_drv_set_xy
#define LCD_DRV_CLEAR ILI948x_drv_clear
#define LCD_DRV_SEND_PIXELS ILI948x_drv_send_pixels

// local functions
uint16_t gfx_color_32to16(uint32_t color);
uint32_t gfx_get_font_row(int font, char ch, int row);

// init the graphics
int gfx_init(void) {
    LCD_DRV_INIT();  // start LCD driver
    return 0;
}

// destroy the graphics
void gfx_close(void) {
    // no effect
}

// commit the changes to the screen
void gfx_commit(void) {
    // no effect
}

// init the actual LCD (controls power) - this takes a long time
void gfx_init_lcd(void) {
    LCD_DRV_INIT_LCD();
}

// deinit the actual LCD (controls power)
void gfx_deinit_lcd(void) {
    LCD_DRV_DEINIT_LCD();
}

// clear the screen and commit changes
void gfx_clear_screen(uint32_t color) {
    LCD_DRV_CLEAR(gfx_color_32to16(color));
}

// get the screen width
int gfx_get_screen_w(void) {
    return LCD_W;
}

// get the screen height
int gfx_get_screen_h(void) {
    return LCD_H;
}

// draw a filled rectangle
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    int i;
    uint16_t buf[LCD_W];    
    for(i = 0; i < w; i ++) {
        buf[i] = gfx_color_32to16(color);
    }
    LCD_DRV_SET_XY(x, y, w, h);
    for(i = 0; i < h; i ++) {
        LCD_DRV_SEND_PIXELS(buf, w);
    }
}

// load a font
int gfx_load_font(int num, char *font_filename, int size) {
    return 0;  // unnecessary for ROM font
}

// draw text to the screen
void gfx_draw_string(struct gfx_label *label) {
    uint16_t buf[32];  // XXX widest char
    uint16_t fg_color, bg_color;
    uint16_t color1, color2;
    int textlen, textpos, charw, charh;
    int row, col;
    int xpos = label->x;
    int ypos = label->y;
    uint32_t tempix;
    // invalid font
    if(label->font < 0 || label->font >= GFX_NUM_FONTS) {
        return;
    }
    // null string
    if(*(label->text) == 0x00) {
        return;
    }
    
    // setup
    textlen = strlen(label->text);
    charw = GFX_FONT_WIDTH[label->font];
    charh = GFX_FONT_HEIGHT[label->font];
    fg_color = gfx_color_32to16(label->fg_color);
    bg_color = gfx_color_32to16(label->bg_color);
    
    // render each char
    for(textpos = 0; textpos < textlen; textpos ++) {
        if(label->highlight[textpos] == GFX_HIGHLIGHT_INVERT) {
            color2 = fg_color;
            color1 = bg_color;
        }
        else {
            color1 = fg_color;
            color2 = bg_color;
        }
        // render each row of the char
        for(row = 0; row < charh; row ++) {
            tempix = gfx_get_font_row(label->font, label->text[textpos], row);
            // render each pixel of the row
            for(col = 0; col < charw; col ++) {
                if(tempix & 0x01) {
                    buf[col] = color1;
                }
                else {
                    buf[col] = color2;
                }
                tempix = tempix >> 1;
            }
            LCD_DRV_SET_XY(xpos, ypos + row, charw, 1);
            LCD_DRV_SEND_PIXELS(buf, charw);
        }
        xpos += charw;  // move over
    }
}

//
// local functions
//
// convert a 32 bit color (AARRGGBB) into 16 bit (565)
// the A channel and LSBs in each colour channel are discarded
uint16_t gfx_color_32to16(uint32_t color) {
    return (color & 0x00f80000) >> 8 |
        (color & 0x0000fc00) >> 5 |
        (color & 0x000000f8) >> 3;
}

// get a row from a font
uint32_t gfx_get_font_row(int font, char ch, int row) {
    switch(font) {
        case GFX_FONT_SMALLTEXT_8X8:
            return font_smalltext_8x8_bitmap[(int)ch - 32][row];              
        case GFX_FONT_SYSTEM_8X12:
            return font_system_8x12_bitmap[(int)ch - 32][row];
        case GFX_FONT_SMALLTEXT_8X9:
            return font_smalltext_8x9_bitmap[(int)ch - 32][row];              
        case GFX_FONT_SYSTEM_8X13:
            return font_system_8x13_bitmap[(int)ch - 32][row];
        default:
            return 0;
    }
}
