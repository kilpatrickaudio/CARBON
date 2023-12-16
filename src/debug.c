/*
 * CARBON Sequencer Debugging Helpers
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
#include "debug.h"
#include "config.h"
#include "midi/midi_utils.h"
#include "midi/midi_stream.h"
#include "stm32f4xx_hal.h"

#define DEBUG_TEXT_MAXLEN 190  // shorter than max SYSEX len

// init the debug
void debug_init(void) {
#ifdef DEBUG_USBD_PINS
    GPIO_InitTypeDef GPIO_InitStruct;
   
    // XXX debug pins - USB device port
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12 ;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    debug_set_pin(0, 0);
    debug_set_pin(1, 0);
    debug_set_pin(2, 0);
#elif defined(DEBUG_USBH_PINS)
    GPIO_InitTypeDef GPIO_InitStruct;
   
    // XXX debug pins - USB host port
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 ;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    debug_set_pin(0, 0);
    debug_set_pin(1, 0);
    debug_set_pin(2, 0);
#elif defined(DEBUG_TP123)
    GPIO_InitTypeDef GPIO_InitStruct;
   
    // XXX debug pin - test points 1-3
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    debug_set_pin(0, 0);
    debug_set_pin(1, 0);
    debug_set_pin(2, 0);
#else
#warning no debug pins defined - debug_set_pin() will not function
#endif
}

// send active sensing to the debug port
void debug_send_active_sensing(void) {
#ifdef DEBUG_OVER_MIDI 
    struct midi_msg msg;
    midi_utils_enc_active_sensing(&msg, DEBUG_MIDI_PORT);
    midi_stream_send_msg(&msg);
#ifdef GFX_REMLCD_MODE
//    midi_utils_enc_active_sensing(&msg, GFX_REMLCD_MIDI_PORT);
//    midi_stream_send_msg(&msg);
#endif
#endif
}

#ifdef DEBUG_OVER_MIDI 
// get printf output and send it over MIDI
int _write(int fd, char *ptr, int len) {
    static uint8_t buf[DEBUG_TEXT_MAXLEN + 6];  // string plus header/footer
    static int buf_len = 5;
    int incount = 0;
    // build up the SYSEX message
    while(*ptr && (incount < len)) {
        // copy string
        buf[buf_len] = (uint8_t)*ptr; 
        buf_len ++; 
        // is it time to send the message?
        if(*ptr == '\n' || buf_len == (DEBUG_TEXT_MAXLEN + 6 - 1)) {
            // SYSEX wrapper
            buf[0] = 0xf0;
            buf[1] = 0x00;
            buf[2] = 0x01;
            buf[3] = 0x72;
            buf[4] = 0x01;
            buf[buf_len] = 0xf7;
            buf_len ++;
            // send message
            midi_stream_send_sysex_msg(DEBUG_MIDI_PORT, buf, buf_len);
            buf_len = 5;
        }
        incount ++;
        ptr ++;
    }
    return incount;
}
#else
// get printf output and send it over MIDI
int _write(int fd, char *ptr, int len) {
    return len;
}
#endif

// set a debug pin
void debug_set_pin(int pin, int state) {
#ifdef DEBUG_USBD_PINS
    switch(pin) {
        case 0:  // dev VBUS
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, state & 0x01);
            break;
        case 1:  // dev D-
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, state & 0x01);
            break;
        case 2:  // dev D+
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, state & 0x01);
            break;
    }
#elif defined(DEBUG_USBH_PINS)
    switch(pin) {
        case 0:  // host VBUS
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, state & 0x01);
            break;
        case 1:  // host D-
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, state & 0x01);
            break;
        case 2:  // host D+
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, state & 0x01);
            break;
    }
#elif defined(DEBUG_TP123)
    switch(pin) {
        case 0:  // TP1
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, state & 0x01);
            break;
        case 1:  // TP2
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, state & 0x01);
            break;
        case 2:  // TP3
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, state & 0x01);
            break;
    }
#endif    
}

