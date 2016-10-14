/*
 * CARBON Sequencer SYSEX Handler
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
 * SYSEX Command Protocol:
 * - error response             - 0x01  <- from device
 *   - 0xf0 - sysex start
 *   - 0x00 - MMA ID
 *   - 0x01 - MMA ID
 *   - 0x72 - MMA ID
 *   - 0x49 - device type
 *   - 0x01 - error response
 *   - 0xaa - original cmd
 *   - 0xbb - error code
 *   - 0xf7 - sysex end
 *
 * - wipe config store          - 0x6f  -> to device
 *   - 0xf0 - sysex start
 *   - 0x00 - MMA ID
 *   - 0x01 - MMA ID
 *   - 0x72 - MMA ID
 *   - 0x49 - device type
 *   - 0x01 - error response
 *   - 0xf7 - sysex end
 *
 * - read ext flash data        - 0x70  -> to device
 *   - 0xf0 - sysex start
 *   - 0x00 - MMA ID
 *   - 0x01 - MMA ID
 *   - 0x72 - MMA ID
 *   - 0x49 - device type
 *   - 0x70 - read ext flash data
 *   - 0x0c - address bits 23-20
 *   - 0x0d - address bits 19-16
 *   - 0x0e - address bits 15-12
 *   - 0x0f - address bits 11-8
 *   - 0x0g - address bits 7-4
 *   - 0x0h - address bits 3-0
 *   - 0xii - read length (1-64 bytes)
 *   - 0xf7 - sysex end 
 *
 * - readback ext flash data    - 0x71  <- from device
 *   - 0xf0 - sysex start
 *   - 0x00 - MMA ID
 *   - 0x01 - MMA ID
 *   - 0x72 - MMA ID
 *   - 0x49 - device type
 *   - 0x71 - readback ext flash data
 *   - 0x0c - address bits 23-20
 *   - 0x0d - address bits 19-16
 *   - 0x0e - address bits 15-12
 *   - 0x0f - address bits 11-8
 *   - 0x0g - address bits 7-4
 *   - 0x0h - address bits 3-0
 *   - 0xii - read length (1-64 bytes)
 *   - ...  - 2-128 data bytes (1-64 bytes in high/low nibbles LSB only)
 *   - 0xf7 - sysex end
 *
 * - write ext flash buf        - 0x72  -> to device
 *   - 0xf0 - sysex start
 *   - 0x00 - MMA ID
 *   - 0x01 - MMA ID
 *   - 0x72 - MMA ID
 *   - 0x49 - device type
 *   - 0x72 - write ext flash buf
 *   - 0x0c - offset bits 23-20
 *   - 0x0d - offset bits 19-16
 *   - 0x0e - offset bits 15-12
 *   - 0x0f - offset bits 11-8
 *   - 0x0g - offset bits 7-4
 *   - 0x0h - offset bits 3-0
 *   - 0xii - write length (1-64 bytes)
 *   - ...  - 2-128 data bytes (1-64 bytes in high/low nibbles LSB only)
 *   - 0xf7 - sysex end
 *
 * - write ext flash commit     - 0x73  -> to device
 *   - 0xf0 - sysex start
 *   - 0x00 - MMA ID
 *   - 0x01 - MMA ID
 *   - 0x72 - MMA ID
 *   - 0x49 - device type
 *   - 0x73 - write ext flash commit
 *   - 0x0c - address bits 23-20
 *   - 0x0d - address bits 19-16
 *   - 0x0e - address bits 15-12
 *   - 0x0f - address bits 11-8
 *   - 0x0g - address bits 7-4
 *   - 0x0h - address bits 3-0
 *   - 0x0i - write len bits 15-12
 *   - 0x0j - write len bits 11-8
 *   - 0x0k - write len bits 7-4
 *   - 0x0l - write len bits 3-0
 *   - 0xf7 - sysex end
 *
 * - device type query          - 0x7c  -> to device
 *  - 0xf0	- sysex start
 *  - 0x00	- MMA ID
 *  - 0x01	- MMA ID
 *  - 0x72	- MMA ID
 *  - 0x7c	- device type query
 *  - 0xf7	- sysex end
 *
 * - device type response       - 0x7d  <- from device
 *   - 0xf0	- sysex start
 *   - 0x00	- MMA ID
 *   - 0x01	- MMA ID
 *   - 0x72	- MMA ID
 *   - 0xdd	- device type code (this is here due to legacy)
 *   - 0x7d	- device type response
 *   - 0xdd	- device type code
 *   - 0xf7	- sysex end
 *
 * - restart device             - 0x7e  -> to device
 *   - 0xf0	- sysex start       
 *   - 0x00	- MMA ID
 *   - 0x01	- MMA ID
 *   - 0x72	- MMA ID
 *   - 0x7e	- restart device
 *   - 0xdd	- device type code
 *   - 0x4b	- 0x4b = 'K'
 *   - 0x49	- 0x49 = 'I'
 *   - 0x4c	- 0x4c = 'L'
 *   - 0x4c	- 0x4c = 'L'
 *   - 0xf7	- sysex end
 *
 * SYSEX Error Response Codes:
 * - ERROR_OK                   - 0x01
 * - ERROR_BAD_ADDRESS          - 0x02
 * - ERROR_BAD_LENGTH           - 0x03
 */
#include "sysex.h"
#include "../config.h"
#include "../config_store.h"
#include "../ext_flash.h"
#include "../midi/midi_stream.h"
#include "../midi/midi_protocol.h"
#include "../util/log.h"
#include "stm32f4xx_hal.h"
#include <inttypes.h>

//
// SYSEX commands / protocol
//
// CARBON commands
#define SYSEX_CMD_ERROR_CODE 0x01  // from device
#define SYSEX_CMD_WIPE_CONFIG_STORE 0x6f  // to device
#define SYSEX_CMD_READ_EXT_FLASH 0x70  // to device
#define SYSEX_CMD_READBACK_EXT_FLASH 0x71  // from device
#define SYSEX_CMD_WRITE_EXT_FLASH_BUF 0x72  // to device
#define SYSEX_CMD_WRITE_EXT_FLASH_COMMIT 0x73  // to device
// global commands
#define SYSEX_CMD_DEV_TYPE 0x7c  // to device
#define SYSEX_CMD_DEV_RESPONSE 0x7d  // from device
#define SYSEX_CMD_RESTART 0x7e  // to device
// error response codes
#define SYSEX_ERROR_OK 0x01
#define SYSEX_ERROR_BAD_ADDRESS 0x02
#define SYSEX_ERROR_BAD_LENGTH 0x03
#define SYSEX_ERROR_MALFORMED_MSG 0x04
#define SYSEX_ERROR_EXT_FLASH_ERROR 0x05
// MMA ID
#define SYSEX_MMA_ID0 0x00
#define SYSEX_MMA_ID1 0x01
#define SYSEX_MMA_ID2 0x72

// settings
#define SYSEX_MAX_LEN 200
#define SYSEX_MAX_READ_LEN 64

// processing states
#define SYSEX_STATE_IDLE 0
#define SYSEX_STATE_READ_EXT_FLASH 1
#define SYSEX_STATE_WRITE_EXT_FLASH 2

// sysex module data
struct sysex_state {
    int state;  // current command state
    uint8_t rx_buf[SYSEX_MAX_LEN];  // RX buffer for SYSEX reception
    int rx_len;  // length of RX message
    int32_t addr;  // the address of the thing we are reading or writing
    uint8_t io_buf[EXT_FLASH_SECTOR_SIZE];  // buffer for I/O to other modules
    int io_len;  // length of I/O
};
struct sysex_state syxs;

// local functions
void sysex_process(void);
void sysex_send_read_ext_mem_result(void);
void sysex_send_devtype_response(void);
void sysex_send_error_response(int cmd, int errorcode);
void sysex_nibbles_to_bytes(uint8_t *inbuf, uint8_t *outbuf, int in_len);

// init the sysex handler
void sysex_init(void) {
    syxs.rx_len = 0;
    syxs.state = SYSEX_STATE_IDLE;
}

// handle processing of requests that can take time
void sysex_timer_task(void) {
    if(syxs.state == SYSEX_STATE_IDLE) {
        return;
    }
    switch(syxs.state) {
        case SYSEX_STATE_READ_EXT_FLASH:
            switch(ext_flash_get_state()) {
                case EXT_FLASH_STATE_LOAD:
                    // load in progress
                    break;
                case EXT_FLASH_STATE_LOAD_ERROR:
                    syxs.state = SYSEX_STATE_IDLE;
                    sysex_send_error_response(SYSEX_CMD_READBACK_EXT_FLASH, 
                        SYSEX_ERROR_EXT_FLASH_ERROR);
                    break;
                case EXT_FLASH_STATE_LOAD_DONE:                    
                    syxs.state = SYSEX_STATE_IDLE;
                    sysex_send_read_ext_mem_result();
                    break;
                default:
                    break;
            }
            break;
    }
}

// handle a portion of SYSEX message received
void sysex_handle_msg(struct midi_msg *msg) {
    // receiving this message would make buffer overrun
    if((syxs.rx_len + msg->len) > SYSEX_MAX_LEN) {
        syxs.rx_len = 0;
        return;
    }

    // 1 byte
    if(msg->len > 0) {
        syxs.rx_buf[syxs.rx_len++] = msg->status;
        if(msg->status == MIDI_SYSEX_END) {
            sysex_process();
            syxs.rx_len = 0;            
            return;
        }
    }
    // 2 bytes
    if(msg->len > 1) {
        syxs.rx_buf[syxs.rx_len++] = msg->data0;
        if(msg->data0 == MIDI_SYSEX_END) {
            sysex_process();
            syxs.rx_len = 0;            
            return;
        }
    }
    // 3 bytes
    if(msg->len > 2) {
        syxs.rx_buf[syxs.rx_len++] = msg->data1;
        if(msg->data1 == MIDI_SYSEX_END) {
            sysex_process();
            syxs.rx_len = 0;            
            return;
        }
    }
}

//
// local functions
//
// process a SYSEX message that has been completely received
void sysex_process(void) {
    if(syxs.rx_len < 6) {
        return;
    }
    // check MMA ID
    if(syxs.rx_buf[1] != SYSEX_MMA_ID0) {
        return;
    }
    if(syxs.rx_buf[2] != SYSEX_MMA_ID1) {
        return;
    }
    if(syxs.rx_buf[3] != SYSEX_MMA_ID2) {
        return;
    }
    // parse global command / device type
    switch(syxs.rx_buf[4]) {
        // global Kilpatrick commands
        case SYSEX_CMD_DEV_TYPE:
            if(syxs.rx_len != 6) {
                sysex_send_error_response(syxs.rx_buf[4], 
                    SYSEX_ERROR_MALFORMED_MSG);
                return;
            }
            sysex_send_devtype_response();
            break;
        case SYSEX_CMD_RESTART:
            if(syxs.rx_len != 11) {
                sysex_send_error_response(syxs.rx_buf[4], 
                    SYSEX_ERROR_MALFORMED_MSG);
                return;
            }
            // check that the dev type and reset message matches
            if(syxs.rx_buf[5] != MIDI_DEV_TYPE) return;
            if(syxs.rx_buf[6] != 'K') return;
            if(syxs.rx_buf[7] != 'I') return;
            if(syxs.rx_buf[8] != 'L') return;
            if(syxs.rx_buf[9] != 'L') return;
            HAL_NVIC_SystemReset();
            break;
        case MIDI_DEV_TYPE:  // CARBON message
            if(syxs.rx_len < 7) {
                sysex_send_error_response(syxs.rx_buf[4], 
                    SYSEX_ERROR_MALFORMED_MSG);
                return;
            }
            // process each command
            switch(syxs.rx_buf[5]) {
                case SYSEX_CMD_WIPE_CONFIG_STORE:
                    if(syxs.rx_len != 7) {
                        sysex_send_error_response(syxs.rx_buf[5],
                            SYSEX_ERROR_MALFORMED_MSG);
                        return;
                    }
                    config_store_wipe_flash();
                    sysex_send_error_response(syxs.rx_buf[5],
                        SYSEX_ERROR_OK);
                    break;
                case SYSEX_CMD_READ_EXT_FLASH:
                    if(syxs.rx_len != 14) {
                        sysex_send_error_response(syxs.rx_buf[5],
                            SYSEX_ERROR_MALFORMED_MSG);
                        return;
                    }
                    syxs.addr = (syxs.rx_buf[6] & 0x0f) << 20;
                    syxs.addr |= (syxs.rx_buf[7] & 0x0f) << 16;
                    syxs.addr |= (syxs.rx_buf[8] & 0x0f) << 12;
                    syxs.addr |= (syxs.rx_buf[9] & 0x0f) << 8;
                    syxs.addr |= (syxs.rx_buf[10] & 0x0f) << 4;
                    syxs.addr |= syxs.rx_buf[11] & 0x0f;
                    syxs.io_len = syxs.rx_buf[12];
                    if(syxs.io_len > SYSEX_MAX_READ_LEN) {
                        sysex_send_error_response(syxs.rx_buf[5], 
                            SYSEX_ERROR_BAD_LENGTH);
                        return;
                    }
                    if(syxs.addr + syxs.io_len > ext_flash_get_mem_size()) {
                        sysex_send_error_response(syxs.rx_buf[5], 
                            SYSEX_ERROR_BAD_ADDRESS);                        
                        return;
                    }
                    // start the read process
                    log_debug("sp - read ext flash - addr: 0x%06x - len: %d",
                        syxs.addr, syxs.io_len);
                    if(ext_flash_load(syxs.addr, syxs.io_len, (uint8_t *)&syxs.io_buf) == -1) {
                        sysex_send_error_response(syxs.rx_buf[5], 
                            SYSEX_ERROR_EXT_FLASH_ERROR);
                        return;
                    }
                    syxs.state = SYSEX_STATE_READ_EXT_FLASH;
                    break;
                case SYSEX_CMD_WRITE_EXT_FLASH_BUF:
                    if(syxs.rx_len < 16) {
                        sysex_send_error_response(syxs.rx_buf[5],
                            SYSEX_ERROR_MALFORMED_MSG);
                        return;
                    }
                    {
                        int offset, len;
                        offset = (syxs.rx_buf[6] & 0x0f) << 20;
                        offset |= (syxs.rx_buf[7] & 0x0f) << 16;
                        offset |= (syxs.rx_buf[8] & 0x0f) << 12;
                        offset |= (syxs.rx_buf[9] & 0x0f) << 8;
                        offset |= (syxs.rx_buf[10] & 0x0f) << 4;
                        offset |= syxs.rx_buf[11] & 0x0f;
                        len = syxs.rx_buf[12];
                        if(offset + len > EXT_FLASH_SECTOR_SIZE) {
                            sysex_send_error_response(syxs.rx_buf[5],
                                SYSEX_ERROR_BAD_LENGTH);                            
                        }
                        // convert nibbles to bytes / copy to io_buf
                        sysex_nibbles_to_bytes(syxs.rx_buf + 13, 
                            syxs.io_buf + offset, len);
                        sysex_send_error_response(syxs.rx_buf[5],
                            SYSEX_ERROR_OK);
                    }
                    break;                
                case SYSEX_CMD_WRITE_EXT_FLASH_COMMIT:
                    if(syxs.rx_len < 17) {
                        sysex_send_error_response(syxs.rx_buf[5],
                            SYSEX_ERROR_MALFORMED_MSG);
                        return;
                    }
                    {
                        int addr, len;
                        addr = (syxs.rx_buf[6] & 0x0f) << 20;
                        addr |= (syxs.rx_buf[7] & 0x0f) << 16;
                        addr |= (syxs.rx_buf[8] & 0x0f) << 12;
                        addr |= (syxs.rx_buf[9] & 0x0f) << 8;
                        addr |= (syxs.rx_buf[10] & 0x0f) << 4;
                        addr |= syxs.rx_buf[11] & 0x0f;
                        len = (syxs.rx_buf[12] & 0x0f) << 12;
                        len |= (syxs.rx_buf[13] & 0x0f) << 8;
                        len |= (syxs.rx_buf[14] & 0x0f) << 4;
                        len |= syxs.rx_buf[15] & 0x0f;
                        // start the write process
                        log_debug("save addr: 0x%06x len: %d", addr, len);
                        if(ext_flash_save(addr, len, (uint8_t *)&syxs.io_buf) == -1) {
                            sysex_send_error_response(syxs.rx_buf[5], 
                                SYSEX_ERROR_EXT_FLASH_ERROR);
                            return;
                        }
                        sysex_send_error_response(syxs.rx_buf[5],
                            SYSEX_ERROR_OK);
                    }
                    break;
            }
            break;
    }
}


// send a flash read mem result
void sysex_send_read_ext_mem_result(void) {
    uint8_t tx_buf[SYSEX_MAX_LEN];
    int i, tx_count = 0;
    tx_buf[tx_count++] = MIDI_SYSEX_START;
    tx_buf[tx_count++] = SYSEX_MMA_ID0;
    tx_buf[tx_count++] = SYSEX_MMA_ID1;
    tx_buf[tx_count++] = SYSEX_MMA_ID2;
    tx_buf[tx_count++] = MIDI_DEV_TYPE;
    tx_buf[tx_count++] = SYSEX_CMD_READBACK_EXT_FLASH;
    tx_buf[tx_count++] = (syxs.addr & 0xf00000) >> 20;
    tx_buf[tx_count++] = (syxs.addr & 0x0f0000) >> 16;
    tx_buf[tx_count++] = (syxs.addr & 0x00f000) >> 12;
    tx_buf[tx_count++] = (syxs.addr & 0x000f00) >> 8;
    tx_buf[tx_count++] = (syxs.addr & 0x0000f0) >> 4;
    tx_buf[tx_count++] = (syxs.addr & 0x00000f);
    tx_buf[tx_count++] = syxs.io_len & 0x7f;
    for(i = 0; i < syxs.io_len; i ++) {
        tx_buf[tx_count++] = (syxs.io_buf[i] & 0xf0) >> 4;
        tx_buf[tx_count++] = syxs.io_buf[i] & 0x0f;
    }
    tx_buf[tx_count++] = MIDI_SYSEX_END;    
    midi_stream_send_sysex_msg(MIDI_PORT_SYSEX_OUT, tx_buf, tx_count);
}

// send the devtype response
void sysex_send_devtype_response(void) {
    uint8_t tx_buf[SYSEX_MAX_LEN];
    int tx_count = 0;
    tx_buf[tx_count++] = MIDI_SYSEX_START;
    tx_buf[tx_count++] = SYSEX_MMA_ID0;
    tx_buf[tx_count++] = SYSEX_MMA_ID1;
    tx_buf[tx_count++] = SYSEX_MMA_ID2;
    tx_buf[tx_count++] = MIDI_DEV_TYPE;
    tx_buf[tx_count++] = SYSEX_CMD_DEV_RESPONSE;
    tx_buf[tx_count++] = MIDI_DEV_TYPE;
    tx_buf[tx_count++] = MIDI_SYSEX_END;    
    midi_stream_send_sysex_msg(MIDI_PORT_SYSEX_OUT, tx_buf, tx_count);
}

// send an error code response
void sysex_send_error_response(int cmd, int errorcode) {
    uint8_t tx_buf[SYSEX_MAX_LEN];
    int tx_count = 0;
    tx_buf[tx_count++] = MIDI_SYSEX_START;
    tx_buf[tx_count++] = SYSEX_MMA_ID0;
    tx_buf[tx_count++] = SYSEX_MMA_ID1;
    tx_buf[tx_count++] = SYSEX_MMA_ID2;
    tx_buf[tx_count++] = MIDI_DEV_TYPE;
    tx_buf[tx_count++] = SYSEX_CMD_ERROR_CODE;
    tx_buf[tx_count++] = cmd & 0x7f;
    tx_buf[tx_count++] = errorcode & 0x7f;
    tx_buf[tx_count++] = MIDI_SYSEX_END;        
    midi_stream_send_sysex_msg(MIDI_PORT_SYSEX_OUT, tx_buf, tx_count);
}

// convert a buffer off 4-bit nibbles to a buffer of 8-bit bytes
void sysex_nibbles_to_bytes(uint8_t *inbuf, uint8_t *outbuf, int in_len) {
    int i, out_count;
    if(in_len < 2) {
        log_error("sntb - in_len must be >= 2: %d", in_len);
        return;
    }
    out_count = 0;
    for(i = 0; i < in_len; i += 2) {
        outbuf[out_count] = ((inbuf[i] & 0x0f) << 4) |
            (inbuf[i+1] & 0x0f);
    }
}

