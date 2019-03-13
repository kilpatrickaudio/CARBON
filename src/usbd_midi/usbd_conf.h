/*
 * USB MIDI Device for STM32 - Config / Hardware Driver
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
#ifndef USBD_CONF_H
#define USBD_CONF_H

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// settings
//
// MIDI port settings
#define USBD_MIDI_PORT_IN (MIDI_PORT_USB_DEV_IN1)  // base port for USBD IN (into device from PC)
                                                  // additional ports are sequential (max 4)
#define USBD_MIDI_PORT_OUT (MIDI_PORT_USB_DEV_OUT1)  // base port for USBD OUT (out of device to PC)
                                                    // additional ports are sequential (max 4)
#define USBD_MIDI_NUM_IN_PORTS 4  // number of USBD IN (into device) ports (max 4)
#define USBD_MIDI_NUM_OUT_PORTS 3  // number of USBD OUT (out of device) ports (max 4)
#define USBD_INTERFACE_DESC_LEN (7 + (USBD_MIDI_NUM_OUT_PORTS * 15) + \
    (USBD_MIDI_NUM_IN_PORTS * 15) + \
    9 + (4 + USBD_MIDI_NUM_IN_PORTS) + \
    9 + (4 + USBD_MIDI_NUM_OUT_PORTS))
#define USBD_MIDI_CONFIG_DESC_SIZE (9 + 9 + 9 + 9 + (USBD_INTERFACE_DESC_LEN))
// device settings
#define USBD_VID                      0xf055
#define USBD_PID                      0xf3f6
#define USBD_LANGID_STRING            0x0409  // english
#define USBD_MANUFACTURER_STRING      "Kilpatrick Audio"
#define USBD_PRODUCT_HS_STRING        "CARBON"
#define USBD_PRODUCT_FS_STRING        "CARBON"
#define USBD_CONFIGURATION_HS_STRING  "MIDI Config"
#define USBD_INTERFACE_HS_STRING      "MIDI Interface"
#define USBD_CONFIGURATION_FS_STRING  "MIDI Config"
#define USBD_INTERFACE_FS_STRING      "MIDI Interface"
#define USBD_SELF_POWERED             1
#define USBD_DEBUG_LEVEL              0
// descriptor stuff (must be in this file due to core requirement)
#define USBD_MAX_NUM_INTERFACES       2
#define USBD_MAX_NUM_CONFIGURATION    1
// language config
#define USBD_malloc               malloc
#define USBD_free                 free
#define USBD_memset               memset
#define USBD_memcpy               memcpy

// debug macros
#if (USBD_DEBUG_LEVEL > 0)
#define  USBD_UsrLog(...)   printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBD_UsrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 1)

#define  USBD_ErrLog(...)   printf("ERROR: ") ;\
                            printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBD_ErrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 2)
#define  USBD_DbgLog(...)   printf("DEBUG : ") ;\
                            printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBD_DbgLog(...)
#endif


#endif
