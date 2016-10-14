/*
 * External Flash Access Routines
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
 */
#include "ext_flash.h"
#include "util/log.h"
#include <string.h>

//
// I/O stuff
//
// state for loading and saving
#define EXT_FLASH_SUBSTATE_LOAD_IDLE 0
#define EXT_FLASH_SUBSTATE_LOAD_READ_START 1
#define EXT_FLASH_SUBSTATE_LOAD_READ_DONE 2
#define EXT_FLASH_SUBSTATE_SAVE_IDLE 0
#define EXT_FLASH_SUBSTATE_SAVE_ERASE_WE_START 1
#define EXT_FLASH_SUBSTATE_SAVE_ERASE_START 2
#define EXT_FLASH_SUBSTATE_SAVE_ERASE_BUSY_CHECK_START 3
#define EXT_FLASH_SUBSTATE_SAVE_ERASE_BUSY_CHECK_DONE 4
#define EXT_FLASH_SUBSTATE_SAVE_WRITE_EN_START 5
#define EXT_FLASH_SUBSTATE_SAVE_WRITE_START 6
#define EXT_FLASH_SUBSTATE_SAVE_WRITE_BUSY_CHECK_START 7
#define EXT_FLASH_SUBSTATE_SAVE_WRITE_BUSY_CHECK_DONE 8

struct ext_flash_state {
    int state;  // I/O state - are we loading, saving, etc.?
    int substate;  // sub I/O state
    int32_t flash_addr;  // the start address in the flash to read or write
    int32_t rw_len;  // the length in bytes to read or write    
    int32_t addrp;  // address counter
    int32_t last_io_len;  // length of the last I/O
    uint8_t *iop;  // pointer to the buffer to load or save
};
struct ext_flash_state extfs;

// local functions
int32_t ext_flash_get_remain_io_bytes(void);

// init external flash
void ext_flash_init(void) {
    spi_flash_init();
    extfs.state = EXT_FLASH_STATE_IDLE;
}

// run the external flash timer task to handle data transfer
void ext_flash_timer_task(void) {
    uint8_t buf[SPI_FLASH_PAGE_SIZE];
    int ret;

    switch(extfs.state) {
        case EXT_FLASH_STATE_LOAD:
            switch(extfs.substate) {
                case EXT_FLASH_SUBSTATE_LOAD_IDLE:
                    // make sure the flash is ready
                    if(spi_flash_get_state() != SPI_FLASH_STATE_IDLE) {
                        log_error("eftt - load idle flash busy");
                        return;
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_LOAD_READ_START;
                    break;
                case EXT_FLASH_SUBSTATE_LOAD_READ_START:
                    // make sure the flash is ready
                    if(spi_flash_get_state() != SPI_FLASH_STATE_IDLE) {
                        return;
                    }
                    // should we read a full page or less than?
                    if(ext_flash_get_remain_io_bytes() >= SPI_FLASH_PAGE_SIZE) {
                        extfs.last_io_len = SPI_FLASH_PAGE_SIZE;
                    }
                    else {
                        extfs.last_io_len = ext_flash_get_remain_io_bytes();
                    }
                    // start the read process
                    ret = spi_flash_start_cmd(SPI_FLASH_CMD_READ_MEM, 
                        extfs.addrp, NULL, extfs.last_io_len);
                    if(ret != SPI_FLASH_ERROR_OK) {
                        log_error("eftt - load flash read error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_LOAD_ERROR;  // cancel
                        return;                 
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_LOAD_READ_DONE;                
                    break;
                case EXT_FLASH_SUBSTATE_LOAD_READ_DONE:
                    // make sure the flash is ready to read the result
                    if(spi_flash_get_state() != SPI_FLASH_STATE_READ_MEM_DONE) {
                        return;
                    }
                    // read the data out of the result
                    ret = spi_flash_get_result(buf);
                    if(ret == -1) {
                        log_error("eftt - load flash get result error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_LOAD_ERROR;  // cancel
                        return;
                    }
                    // copy data into the song
                    memcpy(extfs.iop, buf, extfs.last_io_len);
                    extfs.addrp += extfs.last_io_len;
                    extfs.iop += extfs.last_io_len;
                    extfs.substate = EXT_FLASH_SUBSTATE_LOAD_READ_START;
                    // load is complete - we read enough data
                    if(ext_flash_get_remain_io_bytes() <= 0) {
                        extfs.state = EXT_FLASH_STATE_LOAD_DONE;
                    }
                    break;
            }
            break;
        case EXT_FLASH_STATE_SAVE:
        case EXT_FLASH_STATE_SAVE_NOERASE:
            switch(extfs.substate) {
                case EXT_FLASH_SUBSTATE_SAVE_IDLE:
                    // make sure the flash is ready
                    if(spi_flash_get_state() != SPI_FLASH_STATE_IDLE) {
                        log_error("eftt - save idle flash busy");
                        return;
                    }
                    // if we're saving normally we need to erase first
                    if(extfs.state == EXT_FLASH_STATE_SAVE) {
                        extfs.substate = EXT_FLASH_SUBSTATE_SAVE_ERASE_WE_START;
                    }
                    // if we're saving in noerase mode then we just start writing
                    else {
                        extfs.substate = EXT_FLASH_SUBSTATE_SAVE_WRITE_EN_START;
                    }
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_ERASE_WE_START:
                    // make sure the flash is ready
                    if(spi_flash_get_state() != SPI_FLASH_STATE_IDLE) {
                        return;
                    }
                    // start the write enable process
                    ret = spi_flash_start_cmd(SPI_FLASH_CMD_WRITE_ENABLE,
                        0, NULL, 0);
                    if(ret != SPI_FLASH_ERROR_OK) {
                        log_error("eftt - save flash we start error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;                 
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_ERASE_START;
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_ERASE_START:
                    // make sure the flash is done write enabling
                    if(spi_flash_get_state() != SPI_FLASH_STATE_WRITE_ENABLE_DONE) {
                        return;
                    }
                    // read result to clear state
                    ret = spi_flash_get_result(buf);
                    if(ret == -1) {
                        log_error("eftt - save flash erase start error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;
                    }
                    // start the sector erase process
                    ret = spi_flash_start_cmd(SPI_FLASH_CMD_ERASE_MEM,
                        extfs.addrp, NULL, 0);
                    if(ret != SPI_FLASH_ERROR_OK) {
                        log_error("eftt - save flash erase error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;                 
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_ERASE_BUSY_CHECK_START;    
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_ERASE_BUSY_CHECK_START:
                    // make sure the flash is done erasing or is idle (retry busy check)
                    // this just means the command is sent
                    if(spi_flash_get_state() != SPI_FLASH_STATE_ERASE_MEM_DONE &&
                            spi_flash_get_state() != SPI_FLASH_STATE_IDLE) {
                        return;
                    }
                    // read result to clear state
                    ret = spi_flash_get_result(buf);
                    if(ret == -1) {
                        log_error("eftt - save flash erase busy check start error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;
                    }
                    // get the busy flag - read the status register
                    ret = spi_flash_start_cmd(SPI_FLASH_CMD_READ_STATUS_REG, 0, NULL, 0);
                    if(ret != SPI_FLASH_ERROR_OK) {
                        log_error("eftt - save flash erase busy check error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;                 
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_ERASE_BUSY_CHECK_DONE;
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_ERASE_BUSY_CHECK_DONE:
                    // make sure we got the status register
                    if(spi_flash_get_state() != SPI_FLASH_STATE_READ_STATUS_REG_DONE) {
                        return;
                    }
                    // check the received busy flag
                    ret = spi_flash_get_result(buf);
                    if(ret == -1) {
                        log_error("eftt - save flash erase busy check result error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;
                    }
                    // device is busy
                    if((buf[0] & 0x01) == 1) {
                        // check the status reg again
                        extfs.substate = EXT_FLASH_SUBSTATE_SAVE_ERASE_BUSY_CHECK_START;
                    }
                    // device is done erasing
                    else {
                        // move to the next sector
                        extfs.addrp += SPI_FLASH_SECTOR_SIZE;
                        extfs.substate = EXT_FLASH_SUBSTATE_SAVE_ERASE_WE_START;
                        // erasing is done - reset pointers for writing
                        if(extfs.addrp >= (extfs.flash_addr + extfs.rw_len)) {
                            extfs.addrp = extfs.flash_addr;
                            extfs.substate = EXT_FLASH_SUBSTATE_SAVE_WRITE_EN_START;
                        }
                    }
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_WRITE_EN_START:
                    // make sure the flash is ready
                    if(spi_flash_get_state() != SPI_FLASH_STATE_IDLE) {
                        return;
                    }
                    // start the write enable process
                    ret = spi_flash_start_cmd(SPI_FLASH_CMD_WRITE_ENABLE, 
                        extfs.addrp, NULL, 0);
                    if(ret != SPI_FLASH_ERROR_OK) {
                        log_error("eftt - save flash write we error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;                 
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_WRITE_START;
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_WRITE_START:
                    // make sure the flash is done write enabling
                    if(spi_flash_get_state() != SPI_FLASH_STATE_WRITE_ENABLE_DONE) {
                        return;
                    }
                    // read result to clear state
                    ret = spi_flash_get_result(buf);
                    if(ret == -1) {
                        log_error("eftt - save flash write we result error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;
                    }
                    // should we write a full page or less than?
                    if(ext_flash_get_remain_io_bytes() >= SPI_FLASH_PAGE_SIZE) {
                        extfs.last_io_len = SPI_FLASH_PAGE_SIZE;
                    }
                    else {
                        extfs.last_io_len = ext_flash_get_remain_io_bytes();
                    }
                    // start the write process
                    ret = spi_flash_start_cmd(SPI_FLASH_CMD_WRITE_MEM,
                        extfs.addrp, extfs.iop, extfs.last_io_len);
                    if(ret != SPI_FLASH_ERROR_OK) {
                        log_error("eftt - save flash write error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;                 
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_WRITE_BUSY_CHECK_START;    
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_WRITE_BUSY_CHECK_START:
                    // make sure the flash is done writing or is idle (retry busy check)
                    // this just means the command is sent
                    if(spi_flash_get_state() != SPI_FLASH_STATE_WRITE_MEM_DONE &&
                            spi_flash_get_state() != SPI_FLASH_STATE_IDLE) {
                        return;
                    }
                    // read result to clear state
                    ret = spi_flash_get_result(buf);
                    if(ret == -1) {
                        log_error("eftt - save flash write busy check start error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;
                    }
                    // get the busy flag - read the status register
                    ret = spi_flash_start_cmd(SPI_FLASH_CMD_READ_STATUS_REG, 
                        0, NULL, 0);
                    if(ret != SPI_FLASH_ERROR_OK) {
                        log_error("eftt - save flash write busy check error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;                 
                    }
                    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_WRITE_BUSY_CHECK_DONE;
                    break;
                case EXT_FLASH_SUBSTATE_SAVE_WRITE_BUSY_CHECK_DONE:
                    // make sure we got the status register
                    if(spi_flash_get_state() != SPI_FLASH_STATE_READ_STATUS_REG_DONE) {
                        return;
                    }
                    // check the received busy flag
                    ret = spi_flash_get_result(buf);
                    if(ret == -1) {
                        log_error("eftt - save flash write busy check result error: %d", ret);
                        extfs.state = EXT_FLASH_STATE_SAVE_ERROR;  // cancel
                        return;
                    }
                    // device is busy
                    if((buf[0] & 0x01) == 1) {
                        // check the status reg again
                        extfs.substate = EXT_FLASH_SUBSTATE_SAVE_WRITE_BUSY_CHECK_START;
                    }
                    // device is done writing
                    else {
                        // move to the next sector
                        extfs.addrp += extfs.last_io_len;
                        extfs.iop += extfs.last_io_len;
                        extfs.substate = EXT_FLASH_SUBSTATE_SAVE_WRITE_EN_START;
                        // writing is done
                        if(ext_flash_get_remain_io_bytes() <= 0) {
                            extfs.state = EXT_FLASH_STATE_SAVE_DONE;
                        }
                    }
                    break;
            }
            break;
        default:
            // no action
            break;
    }
}

// get the state of the external flash
// if we are in an error state the error will be cleared to IDLE after read
int ext_flash_get_state(void) {
    int state = extfs.state;
    switch(extfs.state) {
        case EXT_FLASH_STATE_LOAD_ERROR:
        case EXT_FLASH_STATE_LOAD_DONE:
        case EXT_FLASH_STATE_SAVE_ERROR:
        case EXT_FLASH_STATE_SAVE_DONE:
            extfs.state = EXT_FLASH_STATE_IDLE;
            break;
        default:
            // no action
            break;
    }
    return state;
}

// start load from external flash - return -1 in case of error
int ext_flash_load(int32_t addr, int len, uint8_t *loadp) {
    // can't load if we're busy
    if(extfs.state != EXT_FLASH_STATE_IDLE) {
        return -1;
    }
    extfs.flash_addr = addr;
    extfs.addrp = addr;
    extfs.rw_len = len;
    extfs.iop = loadp;
    extfs.state = EXT_FLASH_STATE_LOAD;
    extfs.substate = EXT_FLASH_SUBSTATE_LOAD_IDLE;
    return 0;
}

// start save to external flash - return -1 in case of error
int ext_flash_save(int32_t addr, int len, uint8_t *savep) {
    // can't save if we're busy
    if(extfs.state != EXT_FLASH_STATE_IDLE) {
        return -1;
    }
    extfs.flash_addr = addr;
    extfs.addrp = addr;
    extfs.rw_len = len;
    extfs.iop = savep;
    extfs.state = EXT_FLASH_STATE_SAVE;
    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_IDLE;
    return 0;
}

// start save to external flash without performing a sector erase first
// returns -1 in case of error
int ext_flash_save_noerase(int32_t addr, int len, uint8_t *savep) {
    // can't save if we're busy
    if(extfs.state != EXT_FLASH_STATE_IDLE) {
        return -1;
    }
    extfs.flash_addr = addr;
    extfs.addrp = addr;
    extfs.rw_len = len;
    extfs.iop = savep;
    extfs.state = EXT_FLASH_STATE_SAVE_NOERASE;
    extfs.substate = EXT_FLASH_SUBSTATE_SAVE_IDLE;
    return 0;
}

// get the size of the external flash
int32_t ext_flash_get_mem_size(void) {
    return SPI_FLASH_MEMORY_SIZE;
}

//
// local functions
//
// get the number of remaining I/O bytes to read or write
int32_t ext_flash_get_remain_io_bytes(void) {
    return (extfs.flash_addr + extfs.rw_len) - extfs.addrp;
}


