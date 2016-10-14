/*
 * USB MIDI Device for STM32 - Class Driver
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio with template code from ST
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
#ifndef USBD_MIDI_H
#define USBD_MIDI_H

#include "usbd_ioreq.h"
#include "usbd_def.h"
#include "../config.h"  // MIDI port settings

// transmit queue
#define USBD_MIDI_TX_QUEUE_BUFSIZE 64
#define USBD_MIDI_TX_QUEUE_BUFMASK (USBD_MIDI_TX_QUEUE_BUFSIZE - 1)
// descriptor / endpoint settings
#define USBD_MIDI_IN_EP 0x81
#define USBD_MIDI_OUT_EP 0x01
#define USBD_MIDI_EP_SIZE 64
#define USBD_MAX_STR_DESC_SIZ         0x100
#define USBD_SUPPORT_USER_STRING      0  // enable not supported
// device descriptor constants
#define DEVICE_ID1 (0x1FFF7A10)  // STM32 UID
#define DEVICE_ID2 (0x1FFF7A14)  // STM32 UID
#define DEVICE_ID3 (0x1FFF7A18)  // STM32 UID
#define USB_SIZ_STRING_SERIAL 0x1A

// exported typedefs
typedef struct {
    uint32_t data[USBD_MIDI_EP_SIZE / 4];
    uint8_t *RxBuffer;
    uint8_t *TxBuffer;
    uint32_t RxLength;
    uint32_t TxLength;
  __IO uint32_t TxState;     
  __IO uint32_t RxState;   
} USBD_MIDI_HandleTypeDef;

// external variables
extern USBD_ClassTypeDef  USBD_MIDI_ClassDriver;
#define USBD_MIDI_CLASS &USBD_MIDI_ClassDriver
extern USBD_DescriptorsTypeDef USBD_MIDI_Desc;

//
// public functions
//
// init the USBD MIDI device
void usbd_midi_init(void);

// run the USBD timer task
void usbd_midi_timer_task(void);

#endif
