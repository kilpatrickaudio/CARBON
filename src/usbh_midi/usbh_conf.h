/*
 * USB MIDI Host for STM32 - Config / Hardware Driver
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
#ifndef USBH_CONF_H
#define USBH_CONF_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../config.h"

//
// settings
//
// MIDI port settings
#define USBH_MIDI_PORT_IN (MIDI_PORT_USB_HOST_IN)  // base port for USBH IN
                                                  // additional ports are sequential
#define USBH_MIDI_PORT_OUT (MIDI_PORT_USB_HOST_OUT)  // base port for USBH OUT
                                                    // additional ports are sequential
#define USBH_MIDI_NUM_PORTS 1  // number of USBH ports (max 16)
#define USBH_MIDI_RX_BUFSIZE 64  // size of IN endpoint buffer in bytes
#define USBH_MIDI_TX_BUFSIZE 64  // size of OUT endpoint buffer in bytes
// host settings
#define USBH_MAX_NUM_ENDPOINTS                2
#define USBH_MAX_NUM_INTERFACES               2
#define USBH_MAX_NUM_CONFIGURATION            1
#define USBH_MAX_NUM_SUPPORTED_CLASS          1
#define USBH_KEEP_CFG_DESCRIPTOR              0
#define USBH_MAX_SIZE_CONFIGURATION           0x200
#define USBH_MAX_DATA_BUFFER                  0x200
//#define USBH_DEBUG_LEVEL                      3  // max output
#define USBH_DEBUG_LEVEL                      0  // no output
#define USBH_USE_OS                           0

// CMSIS OS macros
#if (USBH_USE_OS == 1)
  #include "cmsis_os.h"
  #define   USBH_PROCESS_PRIO    osPriorityNormal
#endif

// language config
#define USBH_malloc               malloc
#define USBH_free                 free
#define USBH_memset               memset
#define USBH_memcpy               memcpy
    
// functions
// set the USB VBUS power control state
void usbh_conf_set_vbus(int state);
    
// debug macros
#if (USBH_DEBUG_LEVEL > 0)
#define USBH_UsrLog(...)   printf(__VA_ARGS__);\
                           printf("\n");
#else
#define USBH_UsrLog(...)   
#endif 
                            
#if (USBH_DEBUG_LEVEL > 1)

#define USBH_ErrLog(...)   printf("ERROR: ") ;\
                           printf(__VA_ARGS__);\
                           printf("\n");
#else
#define USBH_ErrLog(...)   
#endif 
                                                      
#if (USBH_DEBUG_LEVEL > 2)                         
#define USBH_DbgLog(...)   printf("DEBUG : ") ;\
                           printf(__VA_ARGS__);\
                           printf("\n");
#else
#define USBH_DbgLog(...)                         
#endif

#endif
