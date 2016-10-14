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
#include "usbd_midi.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_conf.h"
#include "../util/log.h"
#include "midi/midi_utils.h"
#include "midi/midi_stream.h"
#include "../debug.h"

// code index numbers
#define MIDI_CIN_MISC_FUNCTION_RESERVED         0x00
#define MIDI_CIN_CABLE_EVENTS_RESERVED          0x01
#define MIDI_CIN_2_BYTE_MESSAGE                 0x02
#define MIDI_CIN_MTC                            0x02
#define MIDI_CIN_SONG_SELECT                    0x02
#define MIDI_CIN_3_BYTE_MESSAGE                 0x03
#define MIDI_CIN_SONG_POSITION                  0x03
#define MIDI_CIN_SYSEX_START                    0x04
#define MIDI_CIN_SYSEX_CONTINUE                 0x04
#define MIDI_CIN_1_BYTE_MESSAGE                 0x05
#define MIDI_CIN_SYSEX_ENDS_1                   0x05
#define MIDI_CIN_SYSEX_ENDS_2                   0x06
#define MIDI_CIN_SYSEX_ENDS_3                   0x07
#define MIDI_CIN_NOTE_OFF                       0x08
#define MIDI_CIN_NOTE_ON                        0x09
#define MIDI_CIN_POLY_KEY_PRESSURE              0x0a
#define MIDI_CIN_CONTROL_CHANGE                 0x0b
#define MIDI_CIN_PROGRAM_CHANGE                 0x0c
#define MIDI_CIN_CHANNEL_PRESSURE               0x0d
#define MIDI_CIN_PITCH_BEND                     0x0e
#define MIDI_CIN_SINGLE_BYTE                    0x0f

// class functions
static uint8_t  USBD_MIDI_Init (USBD_HandleTypeDef *pdev, 
                               uint8_t cfgidx);
static uint8_t  USBD_MIDI_DeInit (USBD_HandleTypeDef *pdev, 
                                 uint8_t cfgidx);
static uint8_t  USBD_MIDI_Setup (USBD_HandleTypeDef *pdev, 
                                USBD_SetupReqTypedef *req);
static uint8_t  *USBD_MIDI_GetCfgDesc (uint16_t *length);
static uint8_t  USBD_MIDI_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_MIDI_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_MIDI_EP0_RxReady (USBD_HandleTypeDef *pdev);
static uint8_t  USBD_MIDI_SOF (USBD_HandleTypeDef *pdev);

// descriptor functions
uint8_t *USBD_MIDI_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MIDI_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MIDI_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MIDI_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MIDI_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MIDI_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MIDI_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
#ifdef USB_SUPPORT_USER_STRING_DESC
uint8_t *USBD_MIDI_USRStringDesc (USBD_SpeedTypeDef speed, uint8_t idx, uint16_t *length);  
#endif

// device descriptor
__ALIGN_BEGIN uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,                       // bLength */
    USB_DESC_TYPE_DEVICE,       // bDescriptorType */
    0x00,                       // bcdUSB - 2.00 (LSB)
    0x02,                       // bcdUSB - 2.00 (MSB)
    0x00,                       // bDeviceClass (defined by interface)
    0x00,                       // bDeviceSubClass
    0x00,                       // bDeviceProtocol
    USB_MAX_EP0_SIZE,           // bMaxPacketSize - 8 (changed in Core/Inc/usbd_def.h)
    LOBYTE(USBD_VID),           // idVendor
    HIBYTE(USBD_VID),           // idVendor
    LOBYTE(USBD_PID),           // idVendor
    HIBYTE(USBD_PID),           // idVendor
    0x01,                       // bcdDevice ver.0.01 (LSB)
    0x00,                       // bcdDevice ver.0.01 (MSB)
    USBD_IDX_MFC_STR,           // Index of manufacturer string: 0x01
    USBD_IDX_PRODUCT_STR,       // Index of product string: 0x02
    0x00,                       // Index of serial number string: unused
    USBD_MAX_NUM_CONFIGURATION  // bNumConfigurations: 1
};

// USB MIDI device Configuration Descriptor
static uint8_t USBD_MIDI_CfgDesc[USBD_MIDI_CONFIG_DESC_SIZE] = {
    0x09,  // bLength: 9
    USB_DESC_TYPE_CONFIGURATION,  // bDescriptorType: Configuration (0x02)
    (USBD_MIDI_CONFIG_DESC_SIZE & 0xff),  // wTotalLength: Bytes returned (LSB)
    ((USBD_MIDI_CONFIG_DESC_SIZE >> 8) & 0xff),  // wTotalLength (MSB)
    0x02,  // bNumInterfaces: 2 interfaces
    0x01,  // bConfigurationValue: Configuration value
    0x00,  // iConfiguration: unused
    0xC0,  // bmAttributes: self powered
    0x32,  // MaxPower 0 mA: this current is used for detecting Vbus  

    //
    // interface - number: 0
    //
    0x09,  // bLength: 9
    USB_DESC_TYPE_INTERFACE,  // bDescriptorType: Interface (0x04)
    0x00,  // bInterfaceNumber: 0
    0x00,  // bAlternateSetting: 0
    0x00,  // bNumEndpoints: 0
    0x01,  // bInterfaceClass: 1 (Audio)
    0x01,  // bInterfaceSubClass: 1 (Control Device)
    0x00,  // bInterfaceProtocol: 0
    0x00,  // iInterface: unused

    // audiocontrol interface descriptor
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x01,  // bDescriptorSubtype: 1 (HEADER)
    0x00,  // bcdADC: 1.00 (LSB)
    0x01,  // bcdADC: 1.00 (MSB)
    0x09,  // wTotalLength: 9 (LSB)
    0x00,  // wTotalLength: 9 (MSB)
    0x01,  // bInCollection: 1
    0x01,  // baInterfaceNr: 1
    
    //
    // interface - number: 1
    //
    0x09,  // bLength: 9
    USB_DESC_TYPE_INTERFACE,  // bDescriptorType: Interface (0x04)
    0x01,  // bInterfaceNumber: 1
    0x00,  // bAlternateSetting: 0
    0x02,  // bNumEndpoints: 2
    0x01,  // bInterfaceClass: 1 (Audio)
    0x03,  // bInterfaceSubClass: 3 (MIDI Streaming)
    0x00,  // bInterfaceProtocol: 0
    0x00,  // iInterface: unused

    // MIDIStreaming interface descriptor
    0x07,  // bLength: 7
    0x24,  // bDescriptorType: CS_INTERFACE
    0x01,  // bDescriptorSubtype: 1 (HEADER)
    0x00,  // bcdADC: 1.00 (LSB)
    0x01,  // bcdADC: 1.00 (MSB)
    (USBD_INTERFACE_DESC_LEN & 0xff),  // wTotalLength (LSB) (including length of this descriptor)
    ((USBD_INTERFACE_DESC_LEN >> 8) & 0xff),  // wTotalLength (MSB) (including length of this descriptor)
    
    // IN jack 1 (out of device to PC)
    // MIDI IN jack descriptor (external)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x02,  // bJackType: 2 (External)
    0x01,  // bJackID: 1
    0x00,  // iJack: unused    
    // MIDI OUT jack descriptor (embedded)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x02,  // bJackID: 2
    0x01,  // bNrInputPins: 1
    0x01,  // baSourceID: 1
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
    
#if USBD_MIDI_NUM_OUT_PORTS > 1
    // IN jack 2 (out of device to PC)
    // MIDI IN jack descriptor (external)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x02,  // bJackType: 2 (External)
    0x03,  // bJackID: 3
    0x00,  // iJack: unused    
    // MIDI OUT jack descriptor (embedded)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x04,  // bJackID: 4
    0x01,  // bNrInputPins: 1
    0x03,  // baSourceID: 3
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
#endif

#if USBD_MIDI_NUM_OUT_PORTS > 2
    // IN jack 3 (out of device to PC)
    // MIDI IN jack descriptor (external)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x02,  // bJackType: 2 (External)
    0x05,  // bJackID: 5
    0x00,  // iJack: unused    
    // MIDI OUT jack descriptor (embedded)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x06,  // bJackID: 6
    0x01,  // bNrInputPins: 1
    0x05,  // baSourceID: 5
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
#endif

#if USBD_MIDI_NUM_OUT_PORTS > 3
    // IN jack 4 (out of device to PC)
    // MIDI IN jack descriptor (external)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x02,  // bJackType: 2 (External)
    0x07,  // bJackID: 7
    0x00,  // iJack: unused    
    // MIDI OUT jack descriptor (embedded)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x08,  // bJackID: 8
    0x01,  // bNrInputPins: 1
    0x07,  // baSourceID: 7
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
#endif    

    // OUT jack 1 (into device from PC)
    // MIDI IN jack descriptor (embedded)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x09,  // bJackID: 9
    0x00,  // iJack: unused
    // MIDI OUT jack descriptor (exteral)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x02,  // bJackType: 2 (External)
    0x0a,  // bJackID: 10
    0x01,  // bNrInputPins: 1
    0x09,  // baSourceID: 9
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
    
#if USBD_MIDI_NUM_IN_PORTS > 1
    // OUT jack 2 (into device from PC)
    // MIDI IN jack descriptor (embedded)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x0b,  // bJackID: 11
    0x00,  // iJack: unused
    // MIDI OUT jack descriptor (exteral)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x02,  // bJackType: 2 (External)
    0x0c,  // bJackID: 12
    0x01,  // bNrInputPins: 1
    0x0b,  // baSourceID: 11
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
#endif

#if USBD_MIDI_NUM_IN_PORTS > 2
    // OUT jack 3 (into device from PC)
    // MIDI IN jack descriptor (embedded)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x0d,  // bJackID: 14
    0x00,  // iJack: unused
    // MIDI OUT jack descriptor (exteral)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x02,  // bJackType: 2 (External)
    0x0e,  // bJackID: 15
    0x01,  // bNrInputPins: 1
    0x0d,  // baSourceID: 14
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
#endif

#if USBD_MIDI_NUM_IN_PORTS > 3
    // OUT jack 4 (into device from PC)
    // MIDI IN jack descriptor (embedded)
    0x06,  // bLength: 6
    0x24,  // bDescriptorType: CS_INTERFACE
    0x02,  // bDescriptorSubtype: 2 (MIDI_IN_JACK)
    0x01,  // bJackType: 1 (Embedded)
    0x0f,  // bJackID: 15
    0x00,  // iJack: unused
    // MIDI OUT jack descriptor (exteral)
    0x09,  // bLength: 9
    0x24,  // bDescriptorType: CS_INTERFACE
    0x03,  // bDescriptorSubtype: 3 (MIDI_OUT_JACK)
    0x02,  // bJackType: 2 (External)
    0x10,  // bJackID: 16
    0x01,  // bNrInputPins: 1
    0x0f,  // baSourceID: 15
    0x01,  // baSourcePin: 1
    0x00,  // iJack: unused
#endif    

    // MS bulk OUT endpoint descriptor
    0x09,  // bLength: 9
    USB_DESC_TYPE_ENDPOINT,  // bDescriptorType: Endpoint
    USBD_MIDI_OUT_EP,  // bEndpointAddress
    0x02,  // bmAttributes: bulk, no sync, data
    USBD_MIDI_EP_SIZE,  // wMaxPacketSize (LSB)
    0x00,  // wMaxPacketSize (MSB)
    0x00,  // bInterval
    0x00,  // bRefresh
    0x00,  // bSynchAddress
    
    (4 + USBD_MIDI_NUM_IN_PORTS),  // bLength: 4 + num jacks into device (from PC)
    0x25,  // bDescriptorType: CS_ENDPOINT
    0x01,  // bDescriptorSubtype: 1 (GENERAL)
    (USBD_MIDI_NUM_IN_PORTS),  // bNumEmbMIDIJack: USBD_MIDI_NUM_IN_PORTS
    0x09,   // baAssocJackID: 9
#if USBD_MIDI_NUM_IN_PORTS > 1
    0x0b,   // bAssocJackID: 11
#endif
#if USBD_MIDI_NUM_IN_PORTS > 2
    0x0d,   // bAssocJackID: 13
#endif
#if USBD_MIDI_NUM_IN_PORTS > 3
    0x0f,   // bAssocJackID: 15
#endif

    // MS bulk IN endpoint descriptor
    0x09,  // bLength: 9
    USB_DESC_TYPE_ENDPOINT,  // bDescriptorType: Endpoint
    USBD_MIDI_IN_EP,  // bEndpointAddress
    0x02,  // bmAttributes: bulk, no sync, data
    USBD_MIDI_EP_SIZE,  // wMaxPacketSize (LSB)
    0x00,  // wMaxPacketSize (MSB)
    0x00,  // bInterval
    0x00,  // bRefresh
    0x00,  // bSynchAddress
    
    (4 + USBD_MIDI_NUM_OUT_PORTS),  // bLength: 4 + num jacks out of device (to PC)
    0x25,  // bDescriptorType: CS_ENDPOINT
    0x01,  // bDescriptorSubtype: 1 (GENERAL)
    (USBD_MIDI_NUM_OUT_PORTS),  // bNumEmbMIDIJack: 1
    0x02,   // baAssocJackID: 2
#if USBD_MIDI_NUM_OUT_PORTS > 1
    0x04,   // baAssocJackID: 4
#endif
#if USBD_MIDI_NUM_OUT_PORTS > 2
    0x06,   // baAssocJackID: 6
#endif
#if USBD_MIDI_NUM_OUT_PORTS > 3
    0x08   // baAssocJackID: 8
#endif
};

// local functions
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len);
static void Get_SerialNum(void);
void usbd_midi_prepare_in_ep(USBD_HandleTypeDef *pdev);

// mapping of functions to the class driver
USBD_ClassTypeDef USBD_MIDI_ClassDriver = {
    USBD_MIDI_Init,
    USBD_MIDI_DeInit,
    USBD_MIDI_Setup,
    NULL,  // EP0 TX sent
    USBD_MIDI_EP0_RxReady,
    USBD_MIDI_DataIn,
    USBD_MIDI_DataOut,
    USBD_MIDI_SOF,
    NULL,  // iso IN incomplete
    NULL,  // iso OUT incomplete
    USBD_MIDI_GetCfgDesc,
    USBD_MIDI_GetCfgDesc, 
    USBD_MIDI_GetCfgDesc,
    NULL  // device qualifier descriptor
};

// language descriptor
__ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,  // 4 
    USB_DESC_TYPE_STRING,  // 3
    LOBYTE(USBD_LANGID_STRING),  // 0x0409 (english)
    HIBYTE(USBD_LANGID_STRING),  // 0x0409 (english)
};

// serial number
uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] = {
    USB_SIZ_STRING_SERIAL,      
    USB_DESC_TYPE_STRING,    
};
__ALIGN_BEGIN uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

// descriptor callbacks
USBD_DescriptorsTypeDef USBD_MIDI_Desc = {
    USBD_MIDI_DeviceDescriptor,
    USBD_MIDI_LangIDStrDescriptor, 
    USBD_MIDI_ManufacturerStrDescriptor,
    USBD_MIDI_ProductStrDescriptor,
    USBD_MIDI_SerialStrDescriptor,
    USBD_MIDI_ConfigStrDescriptor,
    USBD_MIDI_InterfaceStrDescriptor,  
};

// USB device handle
USBD_HandleTypeDef USBD_Device;

//
// buffers
//
#define USBD_MIDI_BUFSIZE 256
uint8_t usbd_midi_rx_buf[USBD_MIDI_BUFSIZE];  // receive (OUT) endpoint buffer
uint8_t usbd_midi_tx_buf[USBD_MIDI_BUFSIZE];  // transmit (IN) endpoint
uint8_t usbd_midi_tx_q[USBD_MIDI_TX_QUEUE_BUFSIZE][4];  // transmit (IN) packet queue
uint32_t usbd_midi_tx_q_inp;  // transmit (IN) packet queue in pointer
uint32_t usbd_midi_tx_q_outp;  // transmit (IN) packet queue out pointer

// init the USBD MIDI device
void usbd_midi_init(void) {
    // init device library
    USBD_Init(&USBD_Device, &USBD_MIDI_Desc, 0);
    // add class support  
    USBD_RegisterClass(&USBD_Device, USBD_MIDI_CLASS);
    // start device  
    USBD_Start(&USBD_Device);

    // reset transmit packet queue
    usbd_midi_tx_q_inp = 0;
    usbd_midi_tx_q_outp = 0;    
}

// run the USBD timer task
void usbd_midi_timer_task(void) {
    struct midi_msg msg;
    int cable, cin;
    // check all ports
    for(cable = 0; cable < USBD_MIDI_NUM_OUT_PORTS; cable ++) {
        // make sure the EP buffer has room for new data
        if(((usbd_midi_tx_q_inp - usbd_midi_tx_q_outp) & USBD_MIDI_TX_QUEUE_BUFMASK) >
                (USBD_MIDI_TX_QUEUE_BUFSIZE - 4)) {
            return;
        }
        // process each message for port
        while(midi_stream_data_available(cable + USBD_MIDI_PORT_OUT)) {
            midi_stream_receive_msg(cable + USBD_MIDI_PORT_OUT, &msg);                        
            if(msg.len < 1) {
                continue;
            }
            // check voice messages (upper nibble only)
            switch(msg.status & 0xf0) {
	            case MIDI_NOTE_OFF:
	                cin = MIDI_CIN_NOTE_OFF;
		            break;
	            case MIDI_NOTE_ON:
	                cin = MIDI_CIN_NOTE_ON;
		            break;
	            case MIDI_POLY_KEY_PRESSURE:
	                cin = MIDI_CIN_POLY_KEY_PRESSURE;
		            break;
	            case MIDI_CONTROL_CHANGE:
	                cin = MIDI_CIN_CONTROL_CHANGE;
		            break;
	            case MIDI_PROGRAM_CHANGE:
	                cin = MIDI_CIN_PROGRAM_CHANGE;
		            break;
	            case MIDI_CHANNEL_PRESSURE:
	                cin = MIDI_CIN_CHANNEL_PRESSURE;
		            break;
	            case MIDI_PITCH_BEND:
	                cin = MIDI_CIN_PITCH_BEND;
		            break;
	            default:  // system common or realtime messages
	                switch(msg.status) {
	                    case MIDI_MTC_QFRAME:
	                        cin = MIDI_CIN_MTC;
	                        break;
	                    case MIDI_SONG_POSITION:
	                        cin = MIDI_CIN_SONG_POSITION;
		                    break;
	                    case MIDI_SONG_SELECT:
	                        cin = MIDI_CIN_SONG_SELECT;
		                    break;
	                    case MIDI_TUNE_REQUEST:
	                    case MIDI_TIMING_TICK:
	                    case MIDI_CLOCK_START:
	                    case MIDI_CLOCK_CONTINUE:
	                    case MIDI_CLOCK_STOP:
	                    case MIDI_ACTIVE_SENSING:
	                    case MIDI_SYSTEM_RESET:
	                        cin = MIDI_CIN_1_BYTE_MESSAGE;
		                    break;
	                    case MIDI_SYSEX_START:
	                        cin = MIDI_CIN_SYSEX_START;
	                        break;
	                    default:  // arbitrary data in status position (could be SYSEX)
	                        // 2 byte message
                            if(msg.len == 2) {
                                if(msg.data0 == MIDI_SYSEX_END) {
                                    cin = MIDI_CIN_SYSEX_ENDS_2;
                                }
                                else {
                                    cin = MIDI_CIN_2_BYTE_MESSAGE;
                                }
                            }
                            // 3 byte message
                            else if(msg.len == 3) {
                                if(msg.data1 == MIDI_SYSEX_END) {
                                    cin = MIDI_CIN_SYSEX_ENDS_3;
                                }
                                else {
                                    cin = MIDI_CIN_3_BYTE_MESSAGE;
                                }
                            }
                            // 1 byte message
                            else {
                                cin = MIDI_CIN_1_BYTE_MESSAGE;  // or SYSEX_ENDS_1
                            }
                            break;
	                }
	                break;		    
            };
            // add the USB MIDI packet            
            usbd_midi_tx_q[usbd_midi_tx_q_inp][0] = ((cable << 4) & 0xf0) | (cin & 0x0f);
            usbd_midi_tx_q[usbd_midi_tx_q_inp][1] = msg.status;
            if(msg.len > 1) {
                usbd_midi_tx_q[usbd_midi_tx_q_inp][2] = msg.data0;
            }
            else {
                usbd_midi_tx_q[usbd_midi_tx_q_inp][2] = 0;
            }
            if(msg.len > 2) {
                usbd_midi_tx_q[usbd_midi_tx_q_inp][3] = msg.data1;
            }
            else {
                usbd_midi_tx_q[usbd_midi_tx_q_inp][3] = 0;
            }
            usbd_midi_tx_q_inp = (usbd_midi_tx_q_inp + 1) & USBD_MIDI_TX_QUEUE_BUFMASK;
        }
    }
}

//
// callbacks registered with USB core
//
/**
  * @brief  USBD_MIDI_Init
  *         Initialize the MIDI interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_MIDI_Init(USBD_HandleTypeDef *pdev, 
        uint8_t cfgidx) {  
    USBD_MIDI_HandleTypeDef *hmidi;
  
    // open EP IN
    USBD_LL_OpenEP(pdev, USBD_MIDI_IN_EP,
        USBD_EP_TYPE_BULK, USBD_MIDI_EP_SIZE);
        
    // open EP OUT
    USBD_LL_OpenEP(pdev, USBD_MIDI_OUT_EP,
        USBD_EP_TYPE_BULK, USBD_MIDI_EP_SIZE);

    // set up device by storing stuff in the device handle
    pdev->pClassData = USBD_malloc(sizeof(USBD_MIDI_HandleTypeDef));
    if(pdev->pClassData == NULL) {
        return 1;
    }
    hmidi = (USBD_MIDI_HandleTypeDef *)pdev->pClassData;
    
    // map buffers
    hmidi->TxBuffer = usbd_midi_tx_buf;
    hmidi->TxLength = 0;
    hmidi->RxBuffer = usbd_midi_rx_buf;
    hmidi->TxState = 0;
    hmidi->RxState = 0;
    
    // prepare EP OUT to receive the first packet
    USBD_LL_PrepareReceive(pdev, USBD_MIDI_OUT_EP,
        hmidi->RxBuffer, USBD_MIDI_EP_SIZE);

    return 0;
}

/**
  * @brief  USBD_MIDI_Init
  *         DeInitialize the MIDI layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_MIDI_DeInit(USBD_HandleTypeDef *pdev, 
        uint8_t cfgidx) {
    // close EP IN
    USBD_LL_CloseEP(pdev, USBD_MIDI_IN_EP);
    
    // close EP OUT
    USBD_LL_CloseEP(pdev, USBD_MIDI_OUT_EP);
    
    // deinit physical interface
    if(pdev->pClassData != NULL) {
        USBD_free(pdev->pClassData);
        pdev->pClassData = NULL;
    }
    return USBD_OK;
}

/**
  * @brief  USBD_MIDI_Setup
  *         Handle the MIDI specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t USBD_MIDI_Setup(USBD_HandleTypeDef *pdev, 
        USBD_SetupReqTypedef *req) {
    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS :  
            switch (req->bRequest) {
                default:
                    USBD_CtlError (pdev, req);
                    return USBD_FAIL; 
            }
            break;    
        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest) {    
                default:
                    USBD_CtlError (pdev, req);
                    return USBD_FAIL;     
            }
    }
    return USBD_OK;
}

/**
  * @brief  USBD_MIDI_EP0_RxReady
  *         handle EP0 Rx Ready event

  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_MIDI_EP0_RxReady(USBD_HandleTypeDef *pdev) {
    return USBD_OK;
}

/**
  * @brief  USBD_MIDI_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USBD_MIDI_DataIn(USBD_HandleTypeDef *pdev, 
        uint8_t epnum) {
    USBD_MIDI_HandleTypeDef *hmidi = (USBD_MIDI_HandleTypeDef *)pdev->pClassData;
    if(pdev->pClassData != NULL) {
        hmidi->TxState = 0;  // reset TX flag
        usbd_midi_prepare_in_ep(pdev);  // check if another buffer should be sent
        return USBD_OK;
    }
    return USBD_FAIL;
}

/**
  * @brief  USBD_MIDI_DataOut
  *         handle data OUT Stage
  * @param  pdev: device instance

  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USBD_MIDI_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum) {
    int i, cable, cin;
    USBD_MIDI_HandleTypeDef *hmidi = (USBD_MIDI_HandleTypeDef *)pdev->pClassData;
    
    // get the received data length
    hmidi->RxLength = USBD_LL_GetRxDataSize(pdev, epnum);
    
    // USB data is immediately processed
    if(pdev->pClassData != NULL) {
        // parse each packet within the stream
        for(i = 0; i < hmidi->RxLength; i += 4) {
            cable = (hmidi->RxBuffer[i] >> 4) & 0x0f;
            if(cable >= USBD_MIDI_NUM_IN_PORTS) {
                continue;
            }
            cin = hmidi->RxBuffer[i] & 0x0f;
            
            switch(cin) {
                case MIDI_CIN_1_BYTE_MESSAGE:  // one byte
                case MIDI_CIN_SINGLE_BYTE:
                    midi_stream_send_byte((USBD_MIDI_PORT_IN + cable),
                        hmidi->RxBuffer[i + 1]);
                    break;
                case MIDI_CIN_2_BYTE_MESSAGE:  // two bytes
                case MIDI_CIN_SYSEX_ENDS_2:
                case MIDI_CIN_PROGRAM_CHANGE:
                case MIDI_CIN_CHANNEL_PRESSURE:
                    // XXX this could be done better
                    midi_stream_send_byte((USBD_MIDI_PORT_IN + cable),
                        hmidi->RxBuffer[i + 1]);
                    midi_stream_send_byte((USBD_MIDI_PORT_IN + cable),
                        hmidi->RxBuffer[i + 2]);
                    break;
                case MIDI_CIN_3_BYTE_MESSAGE:  // three bytes
                case MIDI_CIN_SYSEX_START:
                case MIDI_CIN_SYSEX_ENDS_3:
                case MIDI_CIN_NOTE_OFF:
                case MIDI_CIN_NOTE_ON:
                case MIDI_CIN_POLY_KEY_PRESSURE:
                case MIDI_CIN_CONTROL_CHANGE:
                case MIDI_CIN_PITCH_BEND:
                    // XXX this could be done better
                    midi_stream_send_byte((USBD_MIDI_PORT_IN + cable),
                        hmidi->RxBuffer[i + 1]);
                    midi_stream_send_byte((USBD_MIDI_PORT_IN + cable),
                        hmidi->RxBuffer[i + 2]);
                    midi_stream_send_byte((USBD_MIDI_PORT_IN + cable),
                        hmidi->RxBuffer[i + 3]);
                    break;
                case MIDI_CIN_CABLE_EVENTS_RESERVED:
                case MIDI_CIN_MISC_FUNCTION_RESERVED:
                default:
                    // we can't do anything with this
                    break;
            }
        }
        
        // XXX can we stall here if we can't take the data now?
        // prepare OUT EP to get next packet
        USBD_LL_PrepareReceive(pdev, USBD_MIDI_OUT_EP,
            hmidi->RxBuffer, USBD_MIDI_EP_SIZE);        
        return USBD_OK;            
    }   
    return USBD_FAIL;
}

/**
  * @brief  USBD_MIDI_SOF
  *         handle SOF event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_MIDI_SOF(USBD_HandleTypeDef *pdev) {
    USBD_MIDI_HandleTypeDef *hmidi = (USBD_MIDI_HandleTypeDef *)pdev->pClassData;

    // check if another buffer should be sent - the pump has not been primed
    if(pdev->pClassData != NULL && hmidi->TxState == 0) {
        usbd_midi_prepare_in_ep(pdev);
        return USBD_OK;
    }
    return USBD_OK;
}

/**
  * @brief  USBD_MIDI_GetCfgDesc 
  *         return configuration descriptor

  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_MIDI_GetCfgDesc(uint16_t *length) {
    *length = sizeof (USBD_MIDI_CfgDesc);
    return (uint8_t *)USBD_MIDI_CfgDesc;
}

//
// descriptor callbacks
//
/**
  * @brief  Returns the device descriptor. 
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_MIDI_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    *length = sizeof(USBD_DeviceDesc);
    return (uint8_t*)USBD_DeviceDesc;
}

/**
  * @brief  Returns the LangID string descriptor.        
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_MIDI_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    *length = sizeof(USBD_LangIDDesc);  
    return (uint8_t*)USBD_LangIDDesc;
}

/**
  * @brief  Returns the product string descriptor. 
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_MIDI_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    if(speed == 0) {   
        USBD_GetString((uint8_t *)USBD_PRODUCT_HS_STRING, USBD_StrDesc, length);
    }
    else {
        USBD_GetString((uint8_t *)USBD_PRODUCT_FS_STRING, USBD_StrDesc, length);    
    }
    return USBD_StrDesc;
}

/**
  * @brief  Returns the manufacturer string descriptor. 
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_MIDI_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

/**
  * @brief  Returns the serial number string descriptor.        
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_MIDI_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    *length = USB_SIZ_STRING_SERIAL;
    Get_SerialNum();
    return (uint8_t*)USBD_StringSerial;
}

/**
  * @brief  Returns the configuration string descriptor.    
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_MIDI_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    if(speed == USBD_SPEED_HIGH) {  
        USBD_GetString((uint8_t *)USBD_CONFIGURATION_HS_STRING, USBD_StrDesc, length);
    }
    else {
        USBD_GetString((uint8_t *)USBD_CONFIGURATION_FS_STRING, USBD_StrDesc, length); 
    }
    return USBD_StrDesc;  
}

/**
  * @brief  Returns the interface string descriptor.        
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_MIDI_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    if(speed == 0) {
        USBD_GetString((uint8_t *)USBD_INTERFACE_HS_STRING, USBD_StrDesc, length);
    }
    else {
        USBD_GetString((uint8_t *)USBD_INTERFACE_FS_STRING, USBD_StrDesc, length);
    }
    return USBD_StrDesc;  
}

//
// local functions
//
/**
  * @brief  Create the serial number string descriptor 
  * @param  None 
  * @retval None
  */
static void Get_SerialNum(void) {
    uint32_t deviceserial0, deviceserial1, deviceserial2;
    deviceserial0 = *(uint32_t*)DEVICE_ID1;
    deviceserial1 = *(uint32_t*)DEVICE_ID2;
    deviceserial2 = *(uint32_t*)DEVICE_ID3;
    deviceserial0 += deviceserial2;
    if (deviceserial0 != 0) {
        IntToUnicode (deviceserial0, &USBD_StringSerial[2] ,8);
        IntToUnicode (deviceserial1, &USBD_StringSerial[18] ,4);
    }
}

/**
  * @brief  Convert Hex 32Bits value into char 
  * @param  value: value to convert
  * @param  pbuf: pointer to the buffer 
  * @param  len: buffer length
  * @retval None
  */
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len) {
    uint8_t idx = 0;
  
    for( idx = 0; idx < len; idx ++) {
        if( ((value >> 28)) < 0xA ) {
            pbuf[ 2* idx] = (value >> 28) + '0';
        }
        else {
            pbuf[2* idx] = (value >> 28) + 'A' - 10; 
        }    
        value = value << 4;
    
        pbuf[ 2* idx + 1] = 0;
    }
}

// prepare the IN EP for a data transfer
void usbd_midi_prepare_in_ep(USBD_HandleTypeDef *pdev) {
    USBD_MIDI_HandleTypeDef *hmidi = (USBD_MIDI_HandleTypeDef *)pdev->pClassData;
    
    // add data to TX buffer
    hmidi->TxLength = 0;
    while(usbd_midi_tx_q_inp != usbd_midi_tx_q_outp) {
        hmidi->TxBuffer[hmidi->TxLength++] = usbd_midi_tx_q[usbd_midi_tx_q_outp][0];
        hmidi->TxBuffer[hmidi->TxLength++] = usbd_midi_tx_q[usbd_midi_tx_q_outp][1];
        hmidi->TxBuffer[hmidi->TxLength++] = usbd_midi_tx_q[usbd_midi_tx_q_outp][2];
        hmidi->TxBuffer[hmidi->TxLength++] = usbd_midi_tx_q[usbd_midi_tx_q_outp][3];
        usbd_midi_tx_q_outp = (usbd_midi_tx_q_outp + 1) & USBD_MIDI_TX_QUEUE_BUFMASK;
    }

    // start transfer
    if(hmidi->TxLength) {
        hmidi->TxState = 1;
        USBD_LL_Transmit(pdev, 
            USBD_MIDI_IN_EP, 
            hmidi->TxBuffer,
            hmidi->TxLength);    
    }
}

