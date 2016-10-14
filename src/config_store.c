/*
 * Flash Config Storage - Wear Leveling Version for STM32
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
#include "config_store.h"
#include "config.h"
#include "ext_flash.h"
#include "util/log.h"
#include "util/state_change.h"
#include "util/state_change_events.h"

// settings
#define CONFIG_STORE_MAGIC_TOKEN 0x434f4e46  // "CONF" in big endian

// I/O states
#define CONFIG_STORE_IO_STATE_NOT_LOADED 0
#define CONFIG_STORE_IO_STATE_LOADED 1
#define CONFIG_STORE_IO_STATE_LOADING 2
#define CONFIG_STORE_IO_STATE_SAVING 3
#define CONFIG_STORE_IO_STATE_ERASING 4

// config store state
struct config_store_state {
    int32_t config_ram[CONFIG_STORE_NUM_ITEMS];
    int32_t config_offset;  // offset address of currently loaded state
    int dirty;  // 0 = no changes to be written back, 1 = config changes
    int io_state;
    // XXX this wastes a lot of RAM
    uint8_t io_buf[EXT_FLASH_SECTOR_SIZE];  // buffer for I/O to other modules
};
// put data into CCMRAM instead of regular RAM
struct config_store_state cfgss __attribute__ ((section (".ccm")));

// local functions
int config_store_load_start(void);
int config_store_load_done(void);
int config_store_writeback_start(void);
void config_store_clear(void);

// init the config store
void config_store_init(void) {
    cfgss.config_offset = 0;
    cfgss.dirty = 0;
    cfgss.io_state = CONFIG_STORE_IO_STATE_NOT_LOADED;  // force reload
    config_store_clear();
}

// run the config storage task
void config_store_timer_task(void) {
    static int timer_div = 0;    
    switch(cfgss.io_state) {
        case CONFIG_STORE_IO_STATE_NOT_LOADED:
            // start load
            if(config_store_load_start() == -1) {
                log_error("cstt - start load error");
            }
            else {
                cfgss.io_state = CONFIG_STORE_IO_STATE_LOADING;
            }
            break;
        case CONFIG_STORE_IO_STATE_LOADED:
            // write back data
            if(cfgss.dirty && (timer_div & CONFIG_STORE_WRITEBACK_INTERVAL) == 0) {
                if(config_store_writeback_start() == -1) {
                    log_error("cstt - writeback start error");
                }
                else {
                    cfgss.io_state = CONFIG_STORE_IO_STATE_SAVING;
                }
            }
            break;
        case CONFIG_STORE_IO_STATE_LOADING:
            // check the flash to see if we're done
            switch(ext_flash_get_state()) {
                case EXT_FLASH_STATE_LOAD_ERROR:
                    config_store_clear();  // clear the store
                    cfgss.dirty = 0;  // make sure we don't write back
                    cfgss.io_state = CONFIG_STORE_IO_STATE_LOADED;
                    // fire event
                    state_change_fire0(SCE_CONFIG_CLEARED);
                    break;
                case EXT_FLASH_STATE_LOAD_DONE:
                    if(config_store_load_done() == -1) {
                        config_store_clear();  // clear the store
                        cfgss.dirty = 0;  // make sure we don't write back
                        cfgss.io_state = CONFIG_STORE_IO_STATE_LOADED;
                        // fire event
                        state_change_fire0(SCE_CONFIG_CLEARED);
                    }
                    else {
                        cfgss.io_state = CONFIG_STORE_IO_STATE_LOADED;
                        // fire event
                        state_change_fire0(SCE_CONFIG_LOADED);
                    }
                    break;
            }
            break;
        case CONFIG_STORE_IO_STATE_SAVING:
            // check the flash to see if we're done
            switch(ext_flash_get_state()) {
                case EXT_FLASH_STATE_SAVE_ERROR:
                    cfgss.io_state = CONFIG_STORE_IO_STATE_LOADED;  // try saving again
                    break;
                case EXT_FLASH_STATE_SAVE_DONE:
                    cfgss.io_state = CONFIG_STORE_IO_STATE_LOADED;
                    cfgss.dirty = 0;  // clean
                    break;
            }
            break;
        case CONFIG_STORE_IO_STATE_ERASING:
            // check the flash to see if we're done
            switch(ext_flash_get_state()) {
                case EXT_FLASH_STATE_SAVE_ERROR:
                case EXT_FLASH_STATE_SAVE_DONE:
                    log_debug("cstt - config store wiped");
                    cfgss.io_state = CONFIG_STORE_IO_STATE_LOADED;
                    cfgss.dirty = 0;  // clean
                    break;
            }
            break;
    }
    timer_div ++;
}

// gets a config byte
int32_t config_store_get_val(int32_t addr) {
    if(addr < 0 || addr >= CONFIG_STORE_NUM_ITEMS) {
        return 0;
    }
    return cfgss.config_ram[addr];
}

// sets a config byte
void config_store_set_val(int32_t addr, int32_t val) {
    if(addr < 0 || addr >= CONFIG_STORE_NUM_ITEMS) {
        return;
    }
    // avoid storing non-changing data
    if(cfgss.config_ram[addr] == val) {
	    return;
    }
    cfgss.config_ram[addr] = val;
    cfgss.dirty = 1;
}

// wipe the config memory so that it will be generated fresh
// returns -1 on error
int config_store_wipe_flash(void) {
    int i;
    if(ext_flash_get_state() != EXT_FLASH_STATE_IDLE) {
        return -1;
    }
    for(i = 0; i < (CONFIG_STORE_NUM_ITEMS * CONFIG_STORE_ITEM_SIZE); i ++) {
        cfgss.io_buf[i] = 0xff;  // blank
    }
    // erase the flash and write nothing
    if(ext_flash_save(EXT_FLASH_CONFIG_OFFSET, (CONFIG_STORE_NUM_ITEMS * 
            CONFIG_STORE_ITEM_SIZE), cfgss.io_buf) == -1) {
        log_error("cswf - error starting save");
        return -1;
    }
    cfgss.io_state = CONFIG_STORE_IO_STATE_ERASING;
    return 0;
}

//
// local functions
//
// start the load process
int config_store_load_start(void) {
    if(ext_flash_get_state() != EXT_FLASH_STATE_IDLE) {
        return -1;
    }
    // start load - bring entire sector into RAM
    if(ext_flash_load(EXT_FLASH_CONFIG_OFFSET,
            EXT_FLASH_CONFIG_SIZE, (uint8_t *)&cfgss.io_buf) == -1) {
        log_error("csls - load start error");
        return -1;
    }
    return 0;
}

// complete the load process - returns -1 if the store was blank
int config_store_load_done(void) {
    int i, token_pos, inpos;
    // search backwards for the last token in the config sector
    for(i = (EXT_FLASH_CONFIG_SIZE - 
            (CONFIG_STORE_NUM_ITEMS * CONFIG_STORE_ITEM_SIZE)); 
            i >= 0;
            i -= (CONFIG_STORE_NUM_ITEMS * CONFIG_STORE_ITEM_SIZE)) {
        token_pos = i + (CONFIG_STORE_TOKEN * CONFIG_STORE_ITEM_SIZE);
        uint32_t val = (cfgss.io_buf[token_pos] << 24) |
            (cfgss.io_buf[token_pos+1] << 16) |
            (cfgss.io_buf[token_pos+2] << 8) |
            cfgss.io_buf[token_pos+3];
        // magic token found
        if(val == CONFIG_STORE_MAGIC_TOKEN) {
            cfgss.config_offset = i;
            log_debug("csld - token found at: %d", cfgss.config_offset);
            // copy the I/O buf to RAM
            inpos = i;
            for(i = 0; i < CONFIG_STORE_NUM_ITEMS; i ++) {
                cfgss.config_ram[i] = (cfgss.io_buf[inpos] << 24) |
                    (cfgss.io_buf[inpos+1] << 16) |
                    (cfgss.io_buf[inpos+2] << 8) |
                    cfgss.io_buf[inpos+3];
                inpos += 4;
            }
            return 0;
        }
    }
    log_debug("csld - token not found");
    return -1;
}

// start the writeback process
int config_store_writeback_start(void) {
    int32_t i, outpos;
    // make sure the token appears
    cfgss.config_ram[CONFIG_STORE_TOKEN] = CONFIG_STORE_MAGIC_TOKEN;
    // copy the RAM to the I/O buf
    outpos = 0;
    for(i = 0; i < CONFIG_STORE_NUM_ITEMS; i ++) {
        cfgss.io_buf[outpos++] = (cfgss.config_ram[i] >> 24) & 0xff;
        cfgss.io_buf[outpos++] = (cfgss.config_ram[i] >> 16) & 0xff;
        cfgss.io_buf[outpos++] = (cfgss.config_ram[i] >> 8) & 0xff;
        cfgss.io_buf[outpos++] = cfgss.config_ram[i] & 0xff;
    }
    // time to erase and start again
    cfgss.config_offset += (CONFIG_STORE_NUM_ITEMS * CONFIG_STORE_ITEM_SIZE);
    if(cfgss.config_offset >= EXT_FLASH_CONFIG_SIZE) {
        cfgss.config_offset = 0;  // reset to start (recycle entire sector)
        log_debug("csws - erase pos: %d", cfgss.config_offset);
        if(ext_flash_save(EXT_FLASH_CONFIG_OFFSET, (CONFIG_STORE_NUM_ITEMS * 
                CONFIG_STORE_ITEM_SIZE), cfgss.io_buf) == -1) {
            return -1;
        }
    }
    // just add to the existing thing
    else {
        log_debug("csws - noerase pos: %d", cfgss.config_offset);
        if(ext_flash_save_noerase(EXT_FLASH_CONFIG_OFFSET + cfgss.config_offset, 
                (CONFIG_STORE_NUM_ITEMS * CONFIG_STORE_ITEM_SIZE), 
                cfgss.io_buf) == -1) {
            return -1;
        }        
    }
    return 0;
}

// clear the config store
void config_store_clear(void) {
    int i;
    for(i = 0; i < (CONFIG_STORE_NUM_ITEMS - 1); i ++) {
        cfgss.config_ram[i] = 0xffffffff;
    }
}

