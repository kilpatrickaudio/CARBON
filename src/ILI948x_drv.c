/*
 * ILI9486/8 LCD Driver - 8 bit parallel mode using FSMC
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
 * Hardware I/O:
 *  - PC6       - LCD_PCTRL             - output
 *  - PD2       - /LCD_RST              - output
 */
#include "ILI948x_drv.h"
#include "lcd_drv.h"
#include "stm32f4xx_hal.h"
#include "delay.h"
#include "config.h"
#include "debug.h"
#include "lcd_fsmc_if.h"
#include "util/log.h"

// state
#define LCD_CMD_DATA_LEN 64
extern uint8_t lcd_cmd;
extern uint8_t lcd_cmd_data[LCD_CMD_DATA_LEN];
extern int lcd_cmd_data_count;

//
// pin control macros
//
// global macros used in any mode
#define ILI948x_RST_L GPIOD->BSRR = (uint32_t)0x0004 << 16
#define ILI948x_RST_H GPIOD->BSRR = 0x0004
#define ILI948x_PCTRL_L GPIOC->BSRR = (uint32_t)0x0040 << 16
#define ILI948x_PCTRL_H GPIOC->BSRR = 0x0040

// local functions
void ILI948x_drv_start_cmd(uint8_t cmd);
void ILI948x_drv_add_cmd_data(uint8_t data);
void ILI948x_drv_send_cmd(void);
void ILI948x_drv_send_pixels(uint16_t *fb, int len);
void ILI948x_drv_read_cmd(int read_len);

// init the LCD driver
void ILI948x_drv_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct;

    // set up I/O
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    // global pin setup
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);    

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct); 
   
    ILI948x_RST_L;  // reset
    ILI948x_PCTRL_L;  // power off
    lcd_fsmc_if_init();  // init the FSMC module
}

// init the actual LCD - takes a long time
void ILI948x_drv_init_LCD(void) {
    int id;

    ILI948x_RST_H;  // reset low
    ILI948x_PCTRL_H;  // power on
    delay_ms(100);
    lcd_fsmc_if_init_if();  // init the FSMC (enables pins)

    // reset device
    ILI948x_RST_H;
    delay_ms(1);
    ILI948x_RST_L;
    delay_ms(100);
    ILI948x_RST_H;
    delay_ms(150);

    // get the screen ID (changes how we do the init)
    id = ILI948x_drv_get_id();
//    log_debug("idil - screen ID: %x", id);

    // display off
    ILI948x_drv_start_cmd(0x28);
    ILI948x_drv_send_cmd();
    
    // power control 1
    ILI948x_drv_start_cmd(0xc0);
    ILI948x_drv_add_cmd_data(0x10);
    ILI948x_drv_add_cmd_data(0x10);
    ILI948x_drv_send_cmd();

    // power control 2
    ILI948x_drv_start_cmd(0xc1);
    ILI948x_drv_add_cmd_data(0x41);
    ILI948x_drv_send_cmd();

    // vcom control 1
    ILI948x_drv_start_cmd(0xc5);
    ILI948x_drv_add_cmd_data(0x00);
    ILI948x_drv_add_cmd_data(0x22);
    ILI948x_drv_add_cmd_data(0x80);
    ILI948x_drv_add_cmd_data(0x40);
    ILI948x_drv_send_cmd();

    // memory access
    ILI948x_drv_start_cmd(0x36);
//    ILI948x_drv_add_cmd_data(0x88);  // inverted
    ILI948x_drv_add_cmd_data(0x48);  // normal
    ILI948x_drv_send_cmd();

    // interface
    ILI948x_drv_start_cmd(0xb0);
    ILI948x_drv_add_cmd_data(0x00);
    ILI948x_drv_send_cmd();

    // frame rate control
    ILI948x_drv_start_cmd(0xb1);
    ILI948x_drv_add_cmd_data(0xb0);
    ILI948x_drv_add_cmd_data(0x11);
    ILI948x_drv_send_cmd();
    
    // inversion control
    ILI948x_drv_start_cmd(0xb4);
    ILI948x_drv_add_cmd_data(0x02);
    ILI948x_drv_send_cmd();

    // display function control
    ILI948x_drv_start_cmd(0xb6);
    ILI948x_drv_add_cmd_data(0x02);
    ILI948x_drv_add_cmd_data(0x02);
    ILI948x_drv_add_cmd_data(0x3b);
    ILI948x_drv_send_cmd();
    
    // entry mode
    ILI948x_drv_start_cmd(0xb7);
    ILI948x_drv_add_cmd_data(0xc6);
    ILI948x_drv_send_cmd();

    // interlaced pixel format
    ILI948x_drv_start_cmd(0x3a);
    ILI948x_drv_add_cmd_data(0x55);
    ILI948x_drv_send_cmd();

    // adjustment control 3
    ILI948x_drv_start_cmd(0xf7);
    ILI948x_drv_add_cmd_data(0xa9);
    ILI948x_drv_add_cmd_data(0x51);
    ILI948x_drv_add_cmd_data(0x2c);
    ILI948x_drv_add_cmd_data(0x82);
    ILI948x_drv_send_cmd();

    // gamma setting (ILI9486)
    if(id == 0x9486) {
        ILI948x_drv_start_cmd(0xe0);
        ILI948x_drv_add_cmd_data(0x0f);
        ILI948x_drv_add_cmd_data(0x1f);
        ILI948x_drv_add_cmd_data(0x1c);
        ILI948x_drv_add_cmd_data(0x0c);
        ILI948x_drv_add_cmd_data(0x0f);
        ILI948x_drv_add_cmd_data(0x08);
        ILI948x_drv_add_cmd_data(0x48);
        ILI948x_drv_add_cmd_data(0x98);
        ILI948x_drv_add_cmd_data(0x37);
        ILI948x_drv_add_cmd_data(0x0a);
        ILI948x_drv_add_cmd_data(0x13);
        ILI948x_drv_add_cmd_data(0x04);
        ILI948x_drv_add_cmd_data(0x11);
        ILI948x_drv_add_cmd_data(0x0d);
        ILI948x_drv_add_cmd_data(0x00);
        ILI948x_drv_send_cmd();

        ILI948x_drv_start_cmd(0xe1);
        ILI948x_drv_add_cmd_data(0x0f);
        ILI948x_drv_add_cmd_data(0x32);
        ILI948x_drv_add_cmd_data(0x2e);
        ILI948x_drv_add_cmd_data(0x0b);
        ILI948x_drv_add_cmd_data(0x0d);
        ILI948x_drv_add_cmd_data(0x05);
        ILI948x_drv_add_cmd_data(0x47);
        ILI948x_drv_add_cmd_data(0x75);
        ILI948x_drv_add_cmd_data(0x37);
        ILI948x_drv_add_cmd_data(0x06);
        ILI948x_drv_add_cmd_data(0x10);
        ILI948x_drv_add_cmd_data(0x03);
        ILI948x_drv_add_cmd_data(0x24);
        ILI948x_drv_add_cmd_data(0x20);
        ILI948x_drv_add_cmd_data(0x00);
        ILI948x_drv_send_cmd();
    }

    // sleep out
    ILI948x_drv_start_cmd(0x11);
    ILI948x_drv_send_cmd();

    delay_ms(150);

    // display on
    ILI948x_drv_start_cmd(0x29);
    ILI948x_drv_send_cmd();      
    ILI948x_drv_clear(0);    
}

// deinit the LCD
void ILI948x_drv_deinit_LCD(void) {
    lcd_fsmc_if_deinit_if();
    ILI948x_RST_L;  // reset
    ILI948x_PCTRL_L;  // power off    
}

// get the ID (model) of the display
uint32_t ILI948x_drv_get_id(void) {
    // read ID4
    ILI948x_drv_start_cmd(0xd3);
    ILI948x_drv_read_cmd(4);
    return (lcd_cmd_data[0] << 24) |
        (lcd_cmd_data[1] << 16) |
        (lcd_cmd_data[2] << 8) |
        lcd_cmd_data[3];        
}

// set the XY position for drawing
void ILI948x_drv_set_xy(int x, int y, int w, int h) {
    int x2, y2;
    if(x < 0 || y < 0 || w < 1 || h < 1) {
        return;
    }
    x2 = x + w - 1;
    y2 = y + h - 1;
    if(x2 > (LCD_W - 1)) {
        x2 = LCD_W - 1;
    }
    if(y2 > (LCD_H - 1)) {
        y2 = LCD_H - 1;
    }
    
    // set column address
    ILI948x_drv_start_cmd(0x2a);
    ILI948x_drv_add_cmd_data((x + LCD_X_OFFSET) >> 8);
    ILI948x_drv_add_cmd_data(x + LCD_X_OFFSET);
    ILI948x_drv_add_cmd_data((x2 + LCD_X_OFFSET) >> 8);
    ILI948x_drv_add_cmd_data(x2 + LCD_X_OFFSET);
    ILI948x_drv_send_cmd();
    
    // set page address
    ILI948x_drv_start_cmd(0x2b);
    ILI948x_drv_add_cmd_data((y + LCD_Y_OFFSET) >> 8);
    ILI948x_drv_add_cmd_data(y + LCD_Y_OFFSET);
    ILI948x_drv_add_cmd_data((y2 + LCD_Y_OFFSET) >> 8);
    ILI948x_drv_add_cmd_data(y2 + LCD_Y_OFFSET);
    ILI948x_drv_send_cmd();

    ILI948x_drv_start_cmd(0x2c);
    ILI948x_drv_send_cmd();
}

// clear the screen to the specified color
void ILI948x_drv_clear(uint16_t color) {
    int i;
    uint16_t buf[LCD_W];
    for(i = 0; i < LCD_W; i ++) {
        buf[i] = color;
    }

    // set column address
    ILI948x_drv_start_cmd(0x2a);
    ILI948x_drv_add_cmd_data(0);
    ILI948x_drv_add_cmd_data(0);
    ILI948x_drv_add_cmd_data((LCD_W - 1) >> 8);
    ILI948x_drv_add_cmd_data((LCD_W - 1) & 0xff);
    ILI948x_drv_send_cmd();
    
    // set page address
    ILI948x_drv_start_cmd(0x2b);
    ILI948x_drv_add_cmd_data(0);
    ILI948x_drv_add_cmd_data(0);
    ILI948x_drv_add_cmd_data((LCD_H - 1) >> 8);
    ILI948x_drv_add_cmd_data((LCD_H - 1) & 0xff);
    ILI948x_drv_send_cmd();

    ILI948x_drv_start_cmd(0x2c);
    ILI948x_drv_send_cmd();

    for(i = 0; i < LCD_H; i ++) {
        ILI948x_drv_send_pixels(buf, LCD_W);
    }
}

// send pixel data to the LCD
void ILI948x_drv_send_pixels(uint16_t *fb, int len) {
    if(len < 1) {
        return;
    }
    lcd_fsmc_if_write16(fb, len, 1);    
}

//
// local functions
//
// start a new command
void ILI948x_drv_start_cmd(uint8_t cmd) {
    lcd_cmd = cmd;
    lcd_cmd_data_count = 0;
}

// append data to a command
void ILI948x_drv_add_cmd_data(uint8_t data) {
    if(lcd_cmd_data_count >= LCD_CMD_DATA_LEN) {
        return;
    }
    lcd_cmd_data[lcd_cmd_data_count] = data;
    lcd_cmd_data_count ++;
}

// send the buffered command
void ILI948x_drv_send_cmd(void) {
    lcd_fsmc_if_write8(&lcd_cmd, 1, 0);
    lcd_fsmc_if_write8(lcd_cmd_data, lcd_cmd_data_count, 1);
}

// send a command a read a result
void ILI948x_drv_read_cmd(int read_len) {
    lcd_fsmc_if_write8(&lcd_cmd, 1, 0);
    lcd_fsmc_if_read8(lcd_cmd_data, read_len, 1);
}

