/*
 * CARBON Sequencer Graphics Routines
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
 *
 * Remote LCD Mode:
 *  The remote LCD mode is used to send drawing commands over MIDI so
 *  that a PC-based graphics simulator can generate real-time screen
 *  data for demonstration / development purposes. It is necessary to be 
 *  able to handle a complete screen refresh which could contain many
 *  objects, therefore a local queue is used to store the screen draw data.
 *  A clear screen data sets a special flag which should be checked by the
 *  MIDI sender. If the flag is set the queue should be emptied until the
 *  clear screen message is found. These messages should not be sent to the
 *  host. The clear screen message and any subsequent messages should be
 *  sent to the host.
 *
 *  Usage notes:
 *  - The remote display must know the resoluton of the screen. (320x480)
 *  - The remote display must have the built-in bitmapped fonts and know
 *    how they are mapped to internal font numbers.
 *  - Colours are transmitted in 16 bit (565) colour format.
 *
 *  Commands / data format:
 *  - clear screen                          - clear screen to a color
 *      byte 0: GFX_REMLCD_CMD_CLEAR_SCREEN
 *      byte 1: color0                      - color bits 15:14
 *      byte 2: color1                      - color bits 13:7
 *      byte 3: color2                      - color bits 6:0
 *  - fill rect                             - draw a filled rectangle
 *      byte 0: GFX_REMLCD_CMD_FILL_RECT
 *      byte 1: x_hi                        - X pos - high 7 bits
 *      byte 2: x_lo                        - X pos - low 7 bits
 *      byte 3: y_hi                        - Y pos - high 7 bits
 *      byte 4: y_lo                        - Y pos - low 7 bits
 *      byte 5: w_hi                        - width - high 7 bits
 *      byte 6: w_lo                        - width - low 7 bits
 *      byte 7: h_hi                        - height - high 7 bits
 *      byte 8: h_lo                        - height - low 7 bits
 *      byte 9: color0                      - color bits 15:14
 *      byte 10: color1                     - color bits 13:7
 *      byte 11: color2                     - color bits 6:0
 *  - draw string                           - draw a string
 *      byte 0: GFX_REMLCD_CMD_DRAW_STRING
 *      byte 1: x_hi                        - X pos - high 7 bits
 *      byte 2: x_lo                        - X pos - low 7 bits
 *      byte 3: y_hi                        - Y pos - high 7 bits
 *      byte 4: y_lo                        - Y pos - low 7 bits
 *      byte 5: w_hi                        - width - high 7 bits
 *      byte 6: w_lo                        - width - low 7 bits
 *      byte 7: h_hi                        - height - high 7 bits
 *      byte 8: h_lo                        - height - low 7 bits
 *      byte 9: font                        - font number
 *      byte 10: fg_color0                  - fg_color bits 15:14
 *      byte 11: fg_color1                  - fg_color bits 13:7
 *      byte 12: fg_color2                  - fg_color bits 6:0
 *      byte 13: bg_color0                  - bg_color bits 15:14
 *      byte 14: bg_color1                  - bg_color bits 13:7
 *      byte 15: bg_color2                  - bg_color bits 6:0
 *      byte 16: text_len                   - the number of text bytes
 *      byte 17:...                         - text bytes (len = text_len)
 *      byte ...:...                        - highlight bytes (len = text_len)
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

// stuff needed for the remote LCD mode
#ifdef GFX_REMLCD_MODE
#warning GFX_REMLCD_MODE enabled - do not use for production!
// commands
#define GFX_REMLCD_CMD_CLEAR_SCREEN 0xf1
#define GFX_REMLCD_CMD_FILL_RECT 0xf2
#define GFX_REMLCD_CMD_DRAW_STRING 0xf3

struct gfx_remote_lcd {
    uint8_t cmd_buf[GFX_REMLCD_CMD_BUFLEN];
    int inp;
    int outp;
};
struct gfx_remote_lcd gfx_remlcd;
#endif

// fonts
#define GFX_NUM_FONTS 3
char GFX_FONT_WIDTH[] = {8, 8, 8};
char GFX_FONT_HEIGHT[] = {10, 12, 13};
extern const uint8_t font_smalltext_8x10_bitmap[95][10];
extern const uint8_t font_system_8x12_bitmap[95][12];
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
#ifdef GFX_REMLCD_MODE
void gfx_remlcd_write_byte(uint8_t byte);
#endif

// init the graphics
int gfx_init(void) {
    LCD_DRV_INIT();  // start LCD driver
#ifdef GFX_REMLCD_MODE
    gfx_remlcd.inp = 0;
    gfx_remlcd.outp = 0;
#endif
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
#ifdef GFX_REMLCD_MODE
    gfx_remlcd_write_byte(GFX_REMLCD_CMD_CLEAR_SCREEN);
    gfx_remlcd_write_byte((gfx_color_32to16(color) >> 14) & 0x03);
    gfx_remlcd_write_byte((gfx_color_32to16(color) >> 7) & 0x7f);
    gfx_remlcd_write_byte(gfx_color_32to16(color) & 0x7f);
#endif
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
#ifdef GFX_REMLCD_MODE
    gfx_remlcd_write_byte(GFX_REMLCD_CMD_FILL_RECT);
    gfx_remlcd_write_byte((x >> 7) & 0x7f);
    gfx_remlcd_write_byte(x & 0x7f);
    gfx_remlcd_write_byte((y >> 7) & 0x7f);
    gfx_remlcd_write_byte(y & 0x7f);
    gfx_remlcd_write_byte((w >> 7) & 0x7f);
    gfx_remlcd_write_byte(w & 0x7f);
    gfx_remlcd_write_byte((h >> 7) & 0x7f);
    gfx_remlcd_write_byte(h & 0x7f);
    gfx_remlcd_write_byte((gfx_color_32to16(color) >> 14) & 0x03);
    gfx_remlcd_write_byte((gfx_color_32to16(color) >> 7) & 0x7f);
    gfx_remlcd_write_byte(gfx_color_32to16(color) & 0x7f);
#endif
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
    
#ifdef GFX_REMLCD_MODE
    gfx_remlcd_write_byte(GFX_REMLCD_CMD_DRAW_STRING);
    gfx_remlcd_write_byte((label->x >> 7) & 0x7f);
    gfx_remlcd_write_byte(label->x & 0x7f);
    gfx_remlcd_write_byte((label->y >> 7) & 0x7f);
    gfx_remlcd_write_byte(label->y & 0x7f);
    gfx_remlcd_write_byte((label->w >> 7) & 0x7f);
    gfx_remlcd_write_byte(label->w & 0x7f);
    gfx_remlcd_write_byte((label->h >> 7) & 0x7f);
    gfx_remlcd_write_byte(label->h & 0x7f);
    gfx_remlcd_write_byte(label->font & 0x7f);
    gfx_remlcd_write_byte((fg_color >> 14) & 0x03);
    gfx_remlcd_write_byte((fg_color >> 7) & 0x7f);
    gfx_remlcd_write_byte(fg_color & 0x7f);
    gfx_remlcd_write_byte((bg_color >> 14) & 0x03);
    gfx_remlcd_write_byte((bg_color >> 7) & 0x7f);
    gfx_remlcd_write_byte(bg_color & 0x7f);
    gfx_remlcd_write_byte(textlen & 0x7f);
    for(textpos = 0; textpos < textlen; textpos ++) {
        gfx_remlcd_write_byte(label->text[textpos]);
    }
    for(textpos = 0; textpos < textlen; textpos ++) {
        gfx_remlcd_write_byte(label->highlight[textpos]);
    }
#endif

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

#ifdef GFX_REMLCD_MODE
// get a command byte from the remote LCD queue or -1 if no data is available
int gfx_remlcd_get_byte(void) {
    uint8_t byte;
    if(gfx_remlcd.inp == gfx_remlcd.outp) {
        return -1;
    }
    byte = gfx_remlcd.cmd_buf[gfx_remlcd.outp];
    gfx_remlcd.outp = (gfx_remlcd.outp + 1) & GFX_REMLCD_CMD_BUFMASK;
    return byte;
}
#endif

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
        case GFX_FONT_SMALLTEXT_8X10:
            return font_smalltext_8x10_bitmap[(int)ch - 32][row];              
        case GFX_FONT_SYSTEM_8X12:
            return font_system_8x12_bitmap[(int)ch - 32][row];
        case GFX_FONT_SYSTEM_8X13:
            return font_system_8x13_bitmap[(int)ch - 32][row];
        default:
            return 0;
    }
}

#ifdef GFX_REMLCD_MODE
// write a byte to the remote LCD command buffer
void gfx_remlcd_write_byte(uint8_t byte) {
    gfx_remlcd.cmd_buf[gfx_remlcd.inp] = byte;
    gfx_remlcd.inp = (gfx_remlcd.inp + 1) & GFX_REMLCD_CMD_BUFMASK;
}
#endif

