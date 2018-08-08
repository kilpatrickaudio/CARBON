/*
 * USB MIDI Host for STM32 - Class Driver
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
#ifndef USBH_MIDI_H
#define USBH_MIDI_H

#include "usbh_core.h"

// USB classes
#define USBH_AUDIO_CLASS  0x01
#define USBH_MIDI_STREAMING 0x03
#define USBH_MIDI_PROTOCOL 0x00

// data transfer state
typedef enum {
    XFER_IDLE = 0,
    XFER_RUN,
    XFER_RUN_WAIT,
    XFER_BUFFER_FULL
} USBH_MIDI_DataXferStateTypedef;

// interface typedef
typedef struct {
    uint8_t InPipe;
    uint8_t OutPipe;
    uint8_t OutEp;
    uint8_t InEp;
    uint16_t OutEpSize;
    uint16_t InEpSize;
} USBH_MIDI_DataItfTypedef;

// structure for MIDI process
typedef struct _USBH_MIDI_HandleTypeDef {
    USBH_MIDI_DataXferStateTypedef RxDataState;
    USBH_MIDI_DataItfTypedef DataItf;
    uint8_t *pTxData;
    uint8_t *pRxData;
    uint32_t TxDataLength;
    uint32_t RxDataLength;
} USBH_MIDI_HandleTypedef;

extern USBH_ClassTypeDef USBH_MIDI_ClassDriver;
#define USBH_MIDI_CLASSDRIVER  &USBH_MIDI_ClassDriver

//
// public functions
//
// init the USB MIDI host
void usbh_midi_init(void);

// run the USB host polling tasks
void usbh_midi_timer_task(void);

// set the USB VBUS state
void usbh_midi_set_vbus(int state);

#endif
