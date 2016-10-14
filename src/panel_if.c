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
 * Hardware I/O:
 *  - PB9       - PANEL_RCLK        - /SPI2 SS
 *  - PB10      - PANEL_SCLK        - SPI2 SCLK
 *  - PC2       - PANEL_MISO        - SPI2 MISO
 *  - PC3       - PANEL_MOSI        - SPI2 MOSI
 *
 */
#include "panel_if.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include "spi_callbacks.h"
#include "switch_filter.h"
#include "gui/panel.h"
#include "seq/seq_ctrl.h"
#include "util/log.h"

// SPI stuff
SPI_HandleTypeDef panel_spi_handle;  // SPI2 panel interface
#define PANEL_IF_BUFSIZE 4
uint8_t panel_spi_rx_buf[PANEL_IF_BUFSIZE];

// LED framebuffer
#define PANEL_IF_NUM_LEDS 32
#define PANEL_IF_LED_PHASES 8
#define PANEL_IF_LED_SHIFT_BITS 5  // number of bits to shit from 8 to n bits
#define PANEL_IF_LED_PHASE_MASK (PANEL_IF_LED_PHASES - 1)
uint8_t panel_if_led_fb[PANEL_IF_LED_PHASES][PANEL_IF_BUFSIZE];
uint8_t panel_if_blink[PANEL_IF_NUM_LEDS][2];  // off and on phase times - must be >0
uint8_t panel_if_blink_state[PANEL_IF_NUM_LEDS][2];  // [0] = countown, [1] = phase

// callback handlers
void panel_if_spi_init_cb(void);
void panel_if_spi_txrx_cplt_cb(void);

// local functions
void panel_if_decode_led(int led, uint8_t level);
void panel_if_write_pwm(int bank, int bit, uint8_t level);

// init the panel interface
void panel_if_init(void) {
    int i, j;
    
    // clear the LED framebuffer
    for(j = 0; j < PANEL_IF_LED_PHASES; j ++) {
        for(i = 0; i < PANEL_IF_BUFSIZE; i ++) {
            panel_if_led_fb[j][i] = 0;
        }
    }
#ifdef PANEL_IF_BL_COMMON_ANODE
        panel_if_decode_led(PANEL_LED_BL_RR, 0xff);
        panel_if_decode_led(PANEL_LED_BL_RG, 0xff);
        panel_if_decode_led(PANEL_LED_BL_RB, 0xff);    
        panel_if_decode_led(PANEL_LED_BL_LR, 0xff);
        panel_if_decode_led(PANEL_LED_BL_LG, 0xff);
        panel_if_decode_led(PANEL_LED_BL_LB, 0xff);
#endif    
    // clear blink setting
    for(i = 0; i < PANEL_IF_NUM_LEDS; i ++) {
        panel_if_blink[i][0] = 0;
        panel_if_blink[i][1] = 0;
    }
    // clear receive buffer
    for(i = 0; i < PANEL_IF_BUFSIZE; i ++) {
        panel_spi_rx_buf[i] = 0xff;  // open switches
    }
    
    // set up the switch debouncing code  
    switch_filter_init(10, 2, 2);        
    switch_filter_set_encoder(0);
    switch_filter_set_encoder(2);
    switch_filter_set_encoder(4);
    switch_filter_set_encoder(10);
    switch_filter_set_encoder(12);
    switch_filter_set_encoder(14);
        
    // register the SPI callbacks
    spi_callbacks_register_handle(SPI_CHANNEL_PANEL, &panel_spi_handle,
        panel_if_spi_init_cb);
    spi_callbacks_register_txrx_cb(SPI_CHANNEL_PANEL, 
        panel_if_spi_txrx_cplt_cb);

    // setup SPI
    panel_spi_handle.Instance = SPI2;
    panel_spi_handle.Init.Mode = SPI_MODE_MASTER;
    panel_spi_handle.Init.Direction = SPI_DIRECTION_2LINES;
    panel_spi_handle.Init.DataSize = SPI_DATASIZE_8BIT;
    panel_spi_handle.Init.CLKPolarity = SPI_POLARITY_HIGH;
    panel_spi_handle.Init.CLKPhase = SPI_PHASE_1EDGE;
    panel_spi_handle.Init.NSS = SPI_NSS_SOFT;
    panel_spi_handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    panel_spi_handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
    panel_spi_handle.Init.TIMode = SPI_TIMODE_DISABLE;
    panel_spi_handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    panel_spi_handle.Init.CRCPolynomial = 10;
    if(HAL_SPI_Init(&panel_spi_handle) != HAL_OK) {
        // XXX handle error
    }
}

// run the panel timer task
void panel_if_timer_task(void) {
    static int led_phase = 0;
    static int count = 0;
    int sw, val, i;

    // if ready to start SPI transfer
    if(HAL_SPI_GetState(&panel_spi_handle) == HAL_SPI_STATE_READY) {
        // process the switch inputs - 1 register per call
        switch_filter_set_val(0, 8, ~panel_spi_rx_buf[0]);
        switch_filter_set_val(8, 8, ~panel_spi_rx_buf[1]);
        switch_filter_set_val(16, 8, ~panel_spi_rx_buf[2]);
        switch_filter_set_val(24, 8, ~panel_spi_rx_buf[3]);

        // start the next transfer
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 1);  // CS goes high to latch data
        if(HAL_SPI_TransmitReceive_IT(&panel_spi_handle,
                (uint8_t *)panel_if_led_fb[led_phase], 
                (uint8_t *)panel_spi_rx_buf, PANEL_IF_BUFSIZE) != HAL_OK) {
            // XXX handle error
        }
        led_phase = (led_phase + 1) & PANEL_IF_LED_PHASE_MASK;  // increment PWM

        // deliver key events to panel
        while((sw = switch_filter_get_event()) != 0) {
            switch(sw & 0xf000) {
                case SW_CHANGE_UNPRESSED:
                    val = 0;
                    break;
                case SW_CHANGE_PRESSED:
                    val = 1;
                    break;
                case SW_CHANGE_ENC_MOVE_CW:
                    val = 1;  // inverted
                    break;
                case SW_CHANGE_ENC_MOVE_CCW:
                    val = 127;  // inverted
                    break;
                default:
                    val = 0;
                    break;
            }
            sw &= 0xfff;
            switch(sw) {
                case 0x10:  // scene
                    seq_ctrl_panel_input(PANEL_SW_SCENE, val);
                    break;
                case 0x11:  // arp
                    seq_ctrl_panel_input(PANEL_SW_ARP, val);
                    break;
                case 0x12:  // live
                    seq_ctrl_panel_input(PANEL_SW_LIVE, val);
                    break;
                case 0x13:  // 1
                    seq_ctrl_panel_input(PANEL_SW_1, val);
                    break;
                case 0x14:  // 2
                    seq_ctrl_panel_input(PANEL_SW_2, val);
                    break;
                case 0x15:  // 3
                    seq_ctrl_panel_input(PANEL_SW_3, val);
                    break;
                case 0x16:  // 4
                    seq_ctrl_panel_input(PANEL_SW_4, val);
                    break;
                case 0x17:  // 5
                    seq_ctrl_panel_input(PANEL_SW_5, val);
                    break;
                case 0x08:  // 6
                    seq_ctrl_panel_input(PANEL_SW_6, val);
                    break;
                case 0x18:  // menu
                    seq_ctrl_panel_input(PANEL_SW_MIDI, val);
                    break;
                case 0x19:  // clock
                    seq_ctrl_panel_input(PANEL_SW_CLOCK, val);
                    break;
                case 0x1a:  // dir
                    seq_ctrl_panel_input(PANEL_SW_DIR, val);
                    break;
                case 0x1b:  // tonality
                    seq_ctrl_panel_input(PANEL_SW_TONALITY, val);
                    break;
                case 0x1c:  // load
                    seq_ctrl_panel_input(PANEL_SW_LOAD, val);
                    break;
                case 0x1d:  // run/stop
                    seq_ctrl_panel_input(PANEL_SW_RUN_STOP, val);
                    break;
                case 0x1e:  // record
                    seq_ctrl_panel_input(PANEL_SW_RECORD, val);
                    break;
                case 0x1f:  // edit
                    seq_ctrl_panel_input(PANEL_SW_EDIT, val);
                    break;
                case 0x09:  // shift
                    seq_ctrl_panel_input(PANEL_SW_SHIFT, val);
                    break;
                case 0x06:  // keys
                    seq_ctrl_panel_input(PANEL_SW_SONG_MODE, val);            
                    break;
                case 0x0a:  // speed                
                    seq_ctrl_panel_input(PANEL_ENC_SPEED, val);
                    break;
                case 0x0c:  // gate time
                    seq_ctrl_panel_input(PANEL_ENC_GATE_TIME, val);
                    break;
                case 0x0e:  // motion start
                    seq_ctrl_panel_input(PANEL_ENC_MOTION_START, val);
                    break;
                case 0x00:  // transpose
                    seq_ctrl_panel_input(PANEL_ENC_TRANSPOSE, val);
                    break;
                case 0x04:  // pattern type
                    seq_ctrl_panel_input(PANEL_ENC_PATTERN_TYPE, val);
                    break;
                case 0x02:  // motion length
                    seq_ctrl_panel_input(PANEL_ENC_MOTION_LENGTH, val);
                    break;
            }
        }
        
        // handle LED blinking  
        if((count & 0x03) == 0) {        
            for(i = 0; i < PANEL_IF_NUM_LEDS; i ++) {
                // only handle LEDs that are supposed to be blinking
                if(panel_if_blink[i][0]) {
                    // time out phase
                    if(panel_if_blink_state[i][0]) {
                        panel_if_blink_state[i][0] --;
                        // time to switch state
                        if(panel_if_blink_state[i][0] == 0) {
                            // switch on to off
                            if(panel_if_blink_state[i][1]) {
                                panel_if_decode_led(i, 0x00);  // turn off LED
                                panel_if_blink_state[i][1] = 0;  // switch to off
                                panel_if_blink_state[i][0] = panel_if_blink[i][0];  // off timeout
                            }
                            // switch off to on
                            else {
                                panel_if_decode_led(i, 0xff);  // turn on LED
                                panel_if_blink_state[i][1] = 1;  // switch to on
                                panel_if_blink_state[i][0] = panel_if_blink[i][1];  // on timeout
                            }
                        }
                    }
                }
            }
        }
        count ++;
    } 
}

// set an LED brightness level - level: 0-255 = 0-100%
void panel_if_set_led(int led, uint8_t level) {
    // reset blink setting
    if(led >= 0 && led < PANEL_IF_NUM_LEDS) {
        panel_if_blink[led][0] = 0;
        panel_if_blink[led][1] = 0;
    }
    // set new level
    panel_if_decode_led(led, level);
}

// set an RGB LED - side: 0 = left, 1 = right
// color - 0xXXRRGGBB
void panel_if_set_rgb(int side, uint32_t color) {
#ifndef PANEL_IF_DISABLE_BL
#ifdef PANEL_IF_BL_COMMON_ANODE
    if(side) {
        panel_if_decode_led(PANEL_LED_BL_RR, 0xff - ((color >> 16) & 0xff));
        panel_if_decode_led(PANEL_LED_BL_RG, 0xff - ((color >> 8) & 0xff));
        panel_if_decode_led(PANEL_LED_BL_RB, 0xff - (color & 0xff));    
    }
    else {
        panel_if_decode_led(PANEL_LED_BL_LR, ~((color >> 16) & 0xff));
        panel_if_decode_led(PANEL_LED_BL_LG, ~((color >> 8) & 0xff));
        panel_if_decode_led(PANEL_LED_BL_LB, ~(color & 0xff));
    }
#else
    if(side) {
        panel_if_decode_led(PANEL_LED_BL_RR, (color >> 16) & 0xff);
        panel_if_decode_led(PANEL_LED_BL_RG, (color >> 8) & 0xff);
        panel_if_decode_led(PANEL_LED_BL_RB, color & 0xff);    
    }
    else {
        panel_if_decode_led(PANEL_LED_BL_LR, (color >> 16) & 0xff);
        panel_if_decode_led(PANEL_LED_BL_LG, (color >> 8) & 0xff);
        panel_if_decode_led(PANEL_LED_BL_LB, color & 0xff);
    }
#endif
#endif
}

// set an LED to blink
void panel_if_blink_led(int led, uint8_t off, uint8_t on) {
    if(led < 0 || led >= PANEL_IF_NUM_LEDS) {
        return;
    }
    panel_if_blink[led][0] = off;
    panel_if_blink[led][1] = on;
    panel_if_blink_state[led][0] = 1;  // about to time out and switch to on phase
    panel_if_blink_state[led][1] = 0;  // off phase
}

//
// callback handlers
//
// handle SPI init callback
void panel_if_spi_init_cb(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    
    //
    // configure clocks and ports
    // 
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    // RCLK - GPIO
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 1);  // CS defaults high

    // SPI pins
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    
    // SCLK
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
   
    // MISO
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // MOSI
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // enable interrupt for SPI
    HAL_NVIC_SetPriority(SPI2_IRQn, INT_PRIO_SPI_PANEL, 0);
    HAL_NVIC_EnableIRQ(SPI2_IRQn);
}

// handle SPI transfer complete callback
void panel_if_spi_txrx_cplt_cb(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 0);  // CS goes low
}

//
// local functions
//
// decode and write to the correct LED
void panel_if_decode_led(int led, uint8_t level) {
    switch(led) {
        case PANEL_LED_ARP:
            panel_if_write_pwm(1, 4, level);
            break;
        case PANEL_LED_LIVE:
            panel_if_write_pwm(1, 5, level);
            break;
        case PANEL_LED_1:
            panel_if_write_pwm(1, 6, level);
            break;
        case PANEL_LED_2:
            panel_if_write_pwm(2, 5, level);
            break;
        case PANEL_LED_3:
            panel_if_write_pwm(2, 0, level);
            break;
        case PANEL_LED_4:
            panel_if_write_pwm(2, 4, level);
            break;
        case PANEL_LED_5:
            panel_if_write_pwm(2, 1, level);
            break;
        case PANEL_LED_6:
            panel_if_write_pwm(2, 2, level);
            break;
        case PANEL_LED_CLOCK:
            panel_if_write_pwm(1, 7, level);
            break;
        case PANEL_LED_DIR:
            panel_if_write_pwm(1, 0, level);
            break;
        case PANEL_LED_RUN_STOP:
            panel_if_write_pwm(2, 7, level);
            break;
        case PANEL_LED_RECORD:
            panel_if_write_pwm(2, 3, level);
            break;
        case PANEL_LED_SONG_MODE:
            panel_if_write_pwm(2, 6, level);
            break;
        case PANEL_LED_BL_LR:
            panel_if_write_pwm(3, 1, level);
            break;
        case PANEL_LED_BL_LG:
            panel_if_write_pwm(3, 3, level);
            break;
        case PANEL_LED_BL_LB:
            panel_if_write_pwm(3, 2, level);
            break;
        case PANEL_LED_BL_RR:
            panel_if_write_pwm(3, 5, level);
            break;
        case PANEL_LED_BL_RG:
            panel_if_write_pwm(3, 7, level);
            break;
        case PANEL_LED_BL_RB:
            panel_if_write_pwm(3, 6, level);
            break;
    }
}

// write the PWM waveform for an LED
void panel_if_write_pwm(int bank, int bit, uint8_t level) {
    int phase, lvl;
    if(bank < 0 || bank >= PANEL_IF_BUFSIZE) {
        return;
    }
    if(bit < 0 || bit > 7) {
        return;
    }
    lvl = (level >> PANEL_IF_LED_SHIFT_BITS) & PANEL_IF_LED_PHASE_MASK;
    for(phase = 0; phase < PANEL_IF_LED_PHASES; phase ++) {
        if(level && (lvl >= phase)) {
            panel_if_led_fb[phase][bank] |= (1 << bit);
        }
        else {
            panel_if_led_fb[phase][bank] &= ~(1 << bit);        
        }
    }
}

