/*
 * SPI Flash Memory Interface (Spansion S25FL116K0XMFI013 2MB)
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
#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include <inttypes.h>

// settings
#define SPI_FLASH_PAGE_SIZE 0x100
#define SPI_FLASH_SECTOR_SIZE 0x1000
#define SPI_FLASH_MEMORY_SIZE 0x200000

// SPI flash commands
#define SPI_FLASH_CMD_READ_SFDP 1  // read from the SFDP register
#define SPI_FLASH_CMD_READ_MFG_DEV_ID 2  // read the MFG / DEV ID
#define SPI_FLASH_CMD_READ_STATUS_REG 3  // read the status register
#define SPI_FLASH_CMD_READ_MEM 4  // read memory
#define SPI_FLASH_CMD_WRITE_ENABLE 5  // write enable
#define SPI_FLASH_CMD_WRITE_MEM 6  // write memory - page program
#define SPI_FLASH_CMD_ERASE_MEM 7  // erase memory - sector erase

// SPI flash state
#define SPI_FLASH_STATE_IDLE 0
#define SPI_FLASH_STATE_READ_SFDP 1  // read SFDP register
#define SPI_FLASH_STATE_READ_SFDP_DONE 2  // read SFDP register done
#define SPI_FLASH_STATE_READ_MFG_DEV_ID 3  // read MFG / DEV ID register
#define SPI_FLASH_STATE_READ_MFG_DEV_ID_DONE 4  // read MFG / DEV ID register done
#define SPI_FLASH_STATE_READ_STATUS_REG 5  // read status register
#define SPI_FLASH_STATE_READ_STATUS_REG_DONE 6  // read status register done
#define SPI_FLASH_STATE_READ_MEM 7  // read flash mem
#define SPI_FLASH_STATE_READ_MEM_DONE 8  // read flash mem done
#define SPI_FLASH_STATE_WRITE_ENABLE 9  // enable writes
#define SPI_FLASH_STATE_WRITE_ENABLE_DONE 10  // write enabled
// writing and erasing require polling the BUSY flag
// in the status register to make sure the process is complete
#define SPI_FLASH_STATE_WRITE_MEM 11  // write flash mem - must be write enabled
#define SPI_FLASH_STATE_WRITE_MEM_DONE 12  // write flash mem done
#define SPI_FLASH_STATE_ERASE_MEM 13  // sector erase flash mem - must be write enabled
#define SPI_FLASH_STATE_ERASE_MEM_DONE 14  // sector erase mem done

// SPI flash errors
#define SPI_FLASH_ERROR_OK 0
#define SPI_FLASH_ERROR_BUSY -1  // module is busy
#define SPI_FLASH_ERROR_INVALID_STATE -2  // an invalid state was encountered
#define SPI_FLASH_ERROR_START_ERROR -3  // transfer could not start
#define SPI_FLASH_ERROR_INVALID_PARAMS -4  // invalid params are given for command
#define SPI_FLASH_ERROR_TIMEOUT -5  // a timeout occured waiting for operation

// init the SPI flash interface
void spi_flash_init(void);

// get the SPI flash state
int spi_flash_get_state(void);

// starts an SPI flash command
// returns -1 if the module is busy or cmd is invalid
int spi_flash_start_cmd(int cmd, uint32_t addr, uint8_t *tx_data, int len);

// get the result of an SPI flash command
// rx_data must be large enough to accommodate expected data
// returns the length of the resulting data
// returns -1 if the module is busy or cmd was not run
int spi_flash_get_result(uint8_t *rx_data);

#endif


