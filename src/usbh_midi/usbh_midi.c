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
#include "usbh_midi.h"
#include "usbh_conf.h"
#include "../midi/midi_stream.h"
#include "../util/log.h"
#include "../debug.h"
#include "../system_stm32f4xx.h"

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
static USBH_StatusTypeDef USBH_MIDI_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MIDI_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MIDI_SOFProcess(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MIDI_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MIDI_Process(USBH_HandleTypeDef *phost);

// local functions
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

// mapping of functions to the class driver
USBH_ClassTypeDef USBH_MIDI_ClassDriver = {
  "MIDI",
  USBH_AUDIO_CLASS,
  USBH_MIDI_InterfaceInit,
  USBH_MIDI_InterfaceDeInit,
  USBH_MIDI_ClassRequest,
  USBH_MIDI_Process,  // BgndProcess
  USBH_MIDI_SOFProcess,
  NULL,
};

// data buffers
uint8_t usbh_rxbuf[USBH_MIDI_RX_BUFSIZE];
uint8_t usbh_txbuf[USBH_MIDI_TX_BUFSIZE];

// USB host handle
USBH_HandleTypeDef hUSBHost;
// timer for polling the USB host
TIM_HandleTypeDef usbh_task_timer;

// init the USB MIDI host
void usbh_midi_init(void) {
    // init host library
    USBH_Init(&hUSBHost, USBH_UserProcess, 0);
    // add supporting class 
    USBH_RegisterClass(&hUSBHost, USBH_MIDI_CLASSDRIVER);
    // start the host
    USBH_Start(&hUSBHost);
    
    // start the timer to handle USB host tasks
    __HAL_RCC_TIM3_CLK_ENABLE();
    usbh_task_timer.Instance = TIM3;
    usbh_task_timer.Init.Period = (SystemCoreClock / 4) / 1200;  // a bit faster than 1ms
    usbh_task_timer.Init.Prescaler = 1;
    usbh_task_timer.Init.ClockDivision = 0;
    usbh_task_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    HAL_TIM_Base_Init(&usbh_task_timer);
    HAL_TIM_Base_Start_IT(&usbh_task_timer);
    HAL_NVIC_SetPriority(TIM3_IRQn, INT_PRIO_USBH_TIMER, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

// run the USB host polling tasks
void usbh_midi_task(void) {
    // USB host background tasks
    USBH_Process(&hUSBHost);
}

// set the USB VBUS state
void usbh_midi_set_vbus(int state) {
    usbh_conf_set_vbus(state);
}

//
// callbacks registered with USB core
//
/**
  * @brief  USBH_MIDI_InterfaceInit 
  *         The function init the MIDI class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_MIDI_InterfaceInit(USBH_HandleTypeDef *phost) {	
    uint8_t interface;
    USBH_MIDI_HandleTypedef *MIDI_handle;
    
    // find the interface on the device that matches what we want
    interface = USBH_FindInterface(phost, 
        USBH_AUDIO_CLASS,  // class
        USBH_MIDI_STREAMING,  // subclass
        USBH_MIDI_PROTOCOL);  // protocol
  
    // no valid interface
    if(interface == 0xFF) {    
        USBH_DbgLog("usbh_mii - no class found for MIDI interface", 
            phost->pActiveClass->Name);         
    }
    else {
        USBH_DbgLog("usbh_mii - class found for MIDI interface", 
            phost->pActiveClass->Name);         
        USBH_SelectInterface(phost, interface);
        phost->pActiveClass->pData = 
            (USBH_MIDI_HandleTypedef *)USBH_malloc(sizeof(USBH_MIDI_HandleTypedef));
        MIDI_handle = (USBH_MIDI_HandleTypedef*)phost->pActiveClass->pData; 
         
        // collect the class specific endpoint address and length
        if(phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 
                0x80) {      
            MIDI_handle->DataItf.InEp = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
            MIDI_handle->DataItf.InEpSize = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
        }
        else {
            MIDI_handle->DataItf.OutEp = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
            MIDI_handle->DataItf.OutEpSize = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
        }
        if(phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress & 
                0x80) {      
            MIDI_handle->DataItf.InEp = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress;
            MIDI_handle->DataItf.InEpSize = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
        }
        else {
            MIDI_handle->DataItf.OutEp = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress;
            MIDI_handle->DataItf.OutEpSize = 
                phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
        }    
  
        // Allocate the length for host channel number out
        MIDI_handle->DataItf.OutPipe = 
            USBH_AllocPipe(phost, MIDI_handle->DataItf.OutEp);
  
        // Allocate the length for host channel number in
        MIDI_handle->DataItf.InPipe = 
            USBH_AllocPipe(phost, MIDI_handle->DataItf.InEp);  
  
        // Open channel for OUT endpoint
        USBH_OpenPipe(phost,
            MIDI_handle->DataItf.OutPipe,
            MIDI_handle->DataItf.OutEp,
            phost->device.address,
            phost->device.speed,
            USB_EP_TYPE_BULK,
            MIDI_handle->DataItf.OutEpSize);  
        // Open channel for IN endpoint
        USBH_OpenPipe(phost,
            MIDI_handle->DataItf.InPipe,
            MIDI_handle->DataItf.InEp,
            phost->device.address,
            phost->device.speed,
            USB_EP_TYPE_BULK,
            MIDI_handle->DataItf.InEpSize);
        // reset stuff
        MIDI_handle->RxDataState = XFER_IDLE;
        MIDI_handle->pRxData = usbh_rxbuf;
        MIDI_handle->pTxData = usbh_txbuf;
        
        USBH_LL_SetToggle(phost, MIDI_handle->DataItf.OutPipe, 0);
        USBH_LL_SetToggle(phost, MIDI_handle->DataItf.InPipe, 0);
        return USBH_OK; 
    }
    return USBH_FAIL;
}

/**
  * @brief  USBH_MIDI_InterfaceDeInit 
  *         The function DeInit the Pipes used for the MIDI class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_MIDI_InterfaceDeInit(USBH_HandleTypeDef *phost) {
    USBH_MIDI_HandleTypedef *MIDI_handle = 
        (USBH_MIDI_HandleTypedef*)phost->pActiveClass->pData;
    
    if(MIDI_handle->DataItf.InPipe) {
        USBH_ClosePipe(phost, MIDI_handle->DataItf.InPipe);
        USBH_FreePipe(phost, MIDI_handle->DataItf.InPipe);
        MIDI_handle->DataItf.InPipe = 0;  // reset the pipe as free
    }
  
    if(MIDI_handle->DataItf.OutPipe) {
        USBH_ClosePipe(phost, MIDI_handle->DataItf.OutPipe);
        USBH_FreePipe(phost, MIDI_handle->DataItf.OutPipe);
        MIDI_handle->DataItf.OutPipe = 0;  // reset the pipe as free
    } 
  
    if(phost->pActiveClass->pData) {
        USBH_free(phost->pActiveClass->pData);
        phost->pActiveClass->pData = 0;
    }
    return USBH_OK;
}

/**
  * @brief  USBH_MIDI_SOFProcess 
  *         The function is for managing SOF callback 
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_MIDI_SOFProcess(USBH_HandleTypeDef *phost) {
    // this is called every frame only when a device is attached
 
    // XXX it might be necessary to check for data and 
    // prime the IN EP here

    return USBH_OK;  
}

/**
  * @brief  USBH_MIDI_ClassRequest 
  *         The function is responsible for handling Standard requests
  *         for MIDI class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_MIDI_ClassRequest(USBH_HandleTypeDef *phost) {   
    // only runs once during setup
    return USBH_OK;
}

/**
  * @brief  USBH_MID_Process 
  *         The function is for managing state machine for MIDI data transfers 
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_MIDI_Process(USBH_HandleTypeDef *phost) {
    // this is called on the task timer as a callback from USBH_Process()
    // this only runs when a device is attached
    struct midi_msg msg;
    int length, cable, cin, i;
    USBH_URBStateTypeDef urb_status = USBH_URB_IDLE;
    USBH_MIDI_HandleTypedef *MIDI_handle = 
        (USBH_MIDI_HandleTypedef*)phost->pActiveClass->pData;
    
    //
    // handle IN data
    //
    urb_status = USBH_LL_GetURBState(phost, MIDI_handle->DataItf.InPipe);
    switch(urb_status) {
        case USBH_URB_IDLE:
            USBH_BulkReceiveData(phost,
                MIDI_handle->pRxData, 
                MIDI_handle->DataItf.InEpSize, 
                MIDI_handle->DataItf.InPipe);        
            break;
        case USBH_URB_DONE:
            length = USBH_LL_GetLastXferSize(phost, MIDI_handle->DataItf.InPipe);
            // MIDI packets must be 4 bytes long
            if(length < 4) {
                break;
            }
            // parse each packet within the stream
            for(i = 0; i < length; i += 4) {
                cable = (MIDI_handle->pRxData[i] >> 4) & 0x0f;
                if(cable >= USBH_MIDI_NUM_PORTS) {
                    continue;
                }
                cin = MIDI_handle->pRxData[i] & 0x0f;
                switch(cin) {
                    case MIDI_CIN_1_BYTE_MESSAGE:  // one byte
                    case MIDI_CIN_SINGLE_BYTE:
                        midi_stream_send_byte((USBH_MIDI_PORT_IN + cable),
                            MIDI_handle->pRxData[i + 1]);
                        break;
                    case MIDI_CIN_2_BYTE_MESSAGE:  // two bytes
                    case MIDI_CIN_SYSEX_ENDS_2:
                    case MIDI_CIN_PROGRAM_CHANGE:
                    case MIDI_CIN_CHANNEL_PRESSURE:
                        midi_stream_send_byte((USBH_MIDI_PORT_IN + cable),
                            MIDI_handle->pRxData[i + 1]);
                        midi_stream_send_byte((USBH_MIDI_PORT_IN + cable),
                            MIDI_handle->pRxData[i + 2]);
                        break;
                    case MIDI_CIN_3_BYTE_MESSAGE:  // three bytes
                    case MIDI_CIN_SYSEX_START:
                    case MIDI_CIN_SYSEX_ENDS_3:
                    case MIDI_CIN_NOTE_OFF:
                    case MIDI_CIN_NOTE_ON:
                    case MIDI_CIN_POLY_KEY_PRESSURE:
                    case MIDI_CIN_CONTROL_CHANGE:
                    case MIDI_CIN_PITCH_BEND:
                        midi_stream_send_byte((USBH_MIDI_PORT_IN + cable),
                            MIDI_handle->pRxData[i + 1]);
                        midi_stream_send_byte((USBH_MIDI_PORT_IN + cable),
                            MIDI_handle->pRxData[i + 2]);
                        midi_stream_send_byte((USBH_MIDI_PORT_IN + cable),
                            MIDI_handle->pRxData[i + 3]);
                        break;
                    case MIDI_CIN_CABLE_EVENTS_RESERVED:
                    case MIDI_CIN_MISC_FUNCTION_RESERVED:
                    default:
                        // we can't do anything with this
                        break;
                }
            }
            // start a new reception
            USBH_BulkReceiveData(phost,
                MIDI_handle->pRxData, 
                MIDI_handle->DataItf.InEpSize, 
                MIDI_handle->DataItf.InPipe);        
            break;
        case USBH_URB_STALL:
        case USBH_URB_NOTREADY:
        case USBH_URB_NYET:
        case USBH_URB_ERROR:        
        default:
            // fall through
            USBH_BulkReceiveData(phost,
                MIDI_handle->pRxData, 
                MIDI_handle->DataItf.InEpSize, 
                MIDI_handle->DataItf.InPipe);
            break;
    }

    //
    // handle OUT data
    //
    urb_status = USBH_LL_GetURBState(phost, MIDI_handle->DataItf.OutPipe);
    if(urb_status == USBH_URB_IDLE || urb_status == USBH_URB_DONE) {
        length = 0;
        // check all ports
        for(cable = 0; cable < USBH_MIDI_NUM_PORTS; cable ++) {
            // make sure the EP buffer has room for new data
            if(length >= MIDI_handle->DataItf.OutEpSize) {
                break;
            }
            // process each message for port
            while(midi_stream_data_available(USBH_MIDI_PORT_OUT + cable)) {
                midi_stream_receive_msg(USBH_MIDI_PORT_OUT + cable, &msg);                        
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
                usbh_txbuf[length] = ((cable << 4) & 0xf0) | (cin & 0x0f);
                usbh_txbuf[length + 1] = msg.status;
                if(msg.len > 1) {
                    usbh_txbuf[length + 2] = msg.data0;
                }
                else {
                    usbh_txbuf[length + 2] = 0;
                }
                if(msg.len > 2) {
                    usbh_txbuf[length + 3] = msg.data1;
                }
                else {
                    usbh_txbuf[length + 3] = 0;
                }
                length += 4;
            }
        }
        // send the packet if we got some data
        if(length > 0) {
            MIDI_handle->TxDataLength = length;        
            USBH_BulkSendData(phost,
                MIDI_handle->pTxData, 
                MIDI_handle->TxDataLength, 
                MIDI_handle->DataItf.OutPipe,
                1);
        }
    }    
    return USBH_OK;
}

//
// local functions
//
/**
  * @brief  User Process
  * @param  phost: Host Handle
  * @param  id: Host Library user message ID
  * @retval None
  */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id) {
    switch(id) { 
        case HOST_USER_SELECT_CONFIGURATION:
            break;    
        case HOST_USER_DISCONNECTION:
            USBH_DbgLog("usbh disconnect");
            break;    
        case HOST_USER_CLASS_ACTIVE:
            USBH_DbgLog("usbh class active");
            break;   
        case HOST_USER_CLASS_SELECTED:
            USBH_DbgLog("usbh class selected");
            break; 
        case HOST_USER_CONNECTION:
            USBH_DbgLog("usbh connect");
            break;        
        default:
        break; 
    }
}
