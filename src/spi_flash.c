/*
 * SPI Flash Memory Interface (Spansion S25FL116K0XMFI013 2MB)
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2015: Kilpatrick Audio
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
 * Hardware I/O:
 *  - PD6       - /ROM_SS           - output - active low
 *  - PC10      - ROM_SCLK          - SPI3_SCLK
 *  - PC12      - ROM_MOSI          - SPI3_MOSI
 *  - PC11      - ROM_MISO          - SPI3_MISO
 *
 */
#include "spi_flash.h"
#include "stm32f4xx_hal.h"
#include "spi_callbacks.h"
#include "config.h"
#include "util/log.h"

// settings
#define SPI_FLASH_PAYLOAD_LEN 256  // max len of TX or RX payload
#define SPI_FLASH_HEADER_LEN 32  // max len of SPI protocol header
#define SPI_FLASH_IF_BUFSIZE (SPI_FLASH_PAYLOAD_LEN + SPI_FLASH_HEADER_LEN)

// SPI flash state
struct spi_flash_state {
    int state;  // the flash state
    uint8_t rx_buf[SPI_FLASH_IF_BUFSIZE];  // internal RX buf
    uint8_t tx_buf[SPI_FLASH_IF_BUFSIZE];  // internal TX buf
    int xfer_len;  // length of transfer
};
struct spi_flash_state sflashs;

SPI_HandleTypeDef spi_flash_spi_handle;  // SPI3 flash interface

// callback handlers
void spi_flash_spi_init_cb(void);
void spi_flash_spi_txrx_cplt_cb(void);

void spi_flash_spi_tx_cplt_cb(void);
void spi_flash_spi_rx_cplt_cb(void);
void spi_flash_spi_tx_half_cplt_cb(void);
void spi_flash_spi_rx_half_cplt_cb(void);
void spi_flash_spi_txrx_half_cplt_cb(void);
void spi_flash_spi_error_cb(void);

// local functions
int spi_flash_start_xfer(uint8_t *tx_buf, uint8_t *rx_buf, int len);

// init the SPI flash interface
void spi_flash_init(void) {
    // register the SPI callbacks
    spi_callbacks_register_handle(SPI_CHANNEL_ROM, &spi_flash_spi_handle,
        spi_flash_spi_init_cb);
    spi_callbacks_register_txrx_cb(SPI_CHANNEL_ROM, 
        spi_flash_spi_txrx_cplt_cb);

    // setup SPI
    spi_flash_spi_handle.Instance = SPI3;
    spi_flash_spi_handle.Init.Mode = SPI_MODE_MASTER;
    spi_flash_spi_handle.Init.Direction = SPI_DIRECTION_2LINES;
    spi_flash_spi_handle.Init.DataSize = SPI_DATASIZE_8BIT;
    spi_flash_spi_handle.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi_flash_spi_handle.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi_flash_spi_handle.Init.NSS = SPI_NSS_SOFT;
//    spi_flash_spi_handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    spi_flash_spi_handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    spi_flash_spi_handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi_flash_spi_handle.Init.TIMode = SPI_TIMODE_DISABLE;    
    spi_flash_spi_handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    spi_flash_spi_handle.Init.CRCPolynomial = 10;
    if(HAL_SPI_Init(&spi_flash_spi_handle) != HAL_OK) {
        // XXX handle error
    }
    
    // reset stuff
    sflashs.state = SPI_FLASH_STATE_IDLE;
}

// get the SPI flash state
int spi_flash_get_state(void) {
    return sflashs.state;
}

// starts an SPI flash command
// cmd = command to execute
// addr = address in memory to read/write (or 0 if not needed)
// tx_data = pointer to transmit data (or null if not needed)
// len = the length of data to read or write (or 0 if not needed)
// returns error if the module is busy or cmd is invalid
int spi_flash_start_cmd(int cmd, uint32_t addr, uint8_t *tx_data, int len) {
    int i, ret;
    if(sflashs.state != SPI_FLASH_STATE_IDLE) {
        return SPI_FLASH_ERROR_BUSY;
    }
    
    switch(cmd) {
        case SPI_FLASH_CMD_READ_SFDP:
            if(len > SPI_FLASH_PAYLOAD_LEN) {
                return SPI_FLASH_ERROR_INVALID_PARAMS;
            }
            sflashs.tx_buf[0] = 0x5a;  // read SFDP
            sflashs.tx_buf[1] = 0;  // addr (not used)
            sflashs.tx_buf[2] = 0;  // addr (not used)
            sflashs.tx_buf[3] = addr & 0xff;  // addr 7:0
            sflashs.tx_buf[4] = 0x00;  // dummy
            sflashs.xfer_len = 5 + len;
            sflashs.state = SPI_FLASH_STATE_READ_SFDP;
            ret = spi_flash_start_xfer(sflashs.tx_buf, sflashs.rx_buf, 
                sflashs.xfer_len);
            if(ret != SPI_FLASH_ERROR_OK) {
                sflashs.state = SPI_FLASH_STATE_IDLE;
                return ret;
            }
            break;
        case SPI_FLASH_CMD_READ_MFG_DEV_ID:
            sflashs.tx_buf[0] = 0x90;  // read mfg / device ID
            sflashs.tx_buf[1] = 0;  // addr (not used)
            sflashs.tx_buf[2] = 0;  // addr (not used)
            sflashs.tx_buf[3] = 0;  // mfg and then dev ID
            sflashs.xfer_len = 6;
            sflashs.state = SPI_FLASH_STATE_READ_MFG_DEV_ID;
            ret = spi_flash_start_xfer(sflashs.tx_buf, sflashs.rx_buf, 
                sflashs.xfer_len);
            if(ret != SPI_FLASH_ERROR_OK) {
                sflashs.state = SPI_FLASH_STATE_IDLE;
                return ret;
            }
            break;
        case SPI_FLASH_CMD_READ_STATUS_REG:
            sflashs.tx_buf[0] = 0x05;  // read status register 1
            sflashs.tx_buf[1] = 0;  // addr (not used)
            sflashs.xfer_len = 2;
            sflashs.state = SPI_FLASH_STATE_READ_STATUS_REG;
            ret = spi_flash_start_xfer(sflashs.tx_buf, sflashs.rx_buf, 
                sflashs.xfer_len);
            if(ret != SPI_FLASH_ERROR_OK) {
                sflashs.state = SPI_FLASH_STATE_IDLE;
                return ret;
            }
            break;        
        case SPI_FLASH_CMD_READ_MEM:
            if(len > SPI_FLASH_PAYLOAD_LEN) {
                return SPI_FLASH_ERROR_INVALID_PARAMS;
            }
            sflashs.tx_buf[0] = 0x03;  // read mem
            sflashs.tx_buf[1] = (addr & 0xff0000) >> 16;  // addr 23-16
            sflashs.tx_buf[2] = (addr & 0x00ff00) >> 8;  // addr 15-8
            sflashs.tx_buf[3] = (addr & 0x0000ff);  // addr 7-0
            sflashs.xfer_len = len + 4;
            sflashs.state = SPI_FLASH_STATE_READ_MEM;
            ret = spi_flash_start_xfer(sflashs.tx_buf, sflashs.rx_buf, 
                sflashs.xfer_len);
            if(ret != SPI_FLASH_ERROR_OK) {
                sflashs.state = SPI_FLASH_STATE_IDLE;
                return ret;
            }
            break;
        case SPI_FLASH_CMD_WRITE_ENABLE:
            if(len > SPI_FLASH_PAYLOAD_LEN) {
                return SPI_FLASH_ERROR_INVALID_PARAMS;
            }
            sflashs.tx_buf[0] = 0x06;  // write enable
            sflashs.xfer_len = 1;
            sflashs.state = SPI_FLASH_STATE_WRITE_ENABLE;
            ret = spi_flash_start_xfer(sflashs.tx_buf, sflashs.rx_buf, 
                sflashs.xfer_len);
            if(ret != SPI_FLASH_ERROR_OK) {
                sflashs.state = SPI_FLASH_STATE_IDLE;
                return ret;
            }
            break;
        case SPI_FLASH_CMD_WRITE_MEM:
            if(len > SPI_FLASH_PAYLOAD_LEN) {
                return SPI_FLASH_ERROR_INVALID_PARAMS;
            }
            // run the write command
            sflashs.tx_buf[0] = 0x02;  // page program
            sflashs.tx_buf[1] = (addr & 0xff0000) >> 16;  // addr 23-16
            sflashs.tx_buf[2] = (addr & 0x00ff00) >> 8;  // addr 15-8
            sflashs.tx_buf[3] = (addr & 0x0000ff);  // addr 7-0
            sflashs.xfer_len = len + 4;
            for(i = 0; i < len; i ++) {
                sflashs.tx_buf[4 + i] = tx_data[i];
            }     
            sflashs.state = SPI_FLASH_STATE_WRITE_MEM;
            ret = spi_flash_start_xfer(sflashs.tx_buf, sflashs.rx_buf, 
                sflashs.xfer_len);
            if(ret != SPI_FLASH_ERROR_OK) {
                sflashs.state = SPI_FLASH_STATE_IDLE;
                return ret;
            }
            break;
        case SPI_FLASH_CMD_ERASE_MEM:
            if(len > SPI_FLASH_PAYLOAD_LEN) {
                return SPI_FLASH_ERROR_INVALID_PARAMS;
            }
            // run the erase command
            sflashs.tx_buf[0] = 0x20;  // erase sector
            sflashs.tx_buf[1] = (addr & 0xff0000) >> 16;  // addr 23-16
            sflashs.tx_buf[2] = (addr & 0x00ff00) >> 8;  // addr 15-8
            sflashs.tx_buf[3] = (addr & 0x0000ff);  // addr 7-0
            sflashs.xfer_len = 4;
            sflashs.state = SPI_FLASH_STATE_ERASE_MEM;
            ret = spi_flash_start_xfer(sflashs.tx_buf, sflashs.rx_buf, 
                sflashs.xfer_len);
            if(ret != SPI_FLASH_ERROR_OK) {
                log_debug("boo");
                sflashs.state = SPI_FLASH_STATE_IDLE;
                return ret;
            }            
            break;
        default:
            return SPI_FLASH_ERROR_INVALID_STATE;
    }
    return 0;
}

// get the result of an SPI flash command and reset the state to idle
// rx_data must be large enough to accommodate expected data
// returns the length of the resulting data
// returns error if the module is busy or cmd was not run
int spi_flash_get_result(uint8_t *rx_data) {
    int i, ret;
    
    // if the module is still busy
    if(HAL_SPI_GetState(&spi_flash_spi_handle) != HAL_SPI_STATE_READY) {
        return SPI_FLASH_ERROR_BUSY;
    }

    // handle states
    switch(sflashs.state) {
        case SPI_FLASH_STATE_READ_SFDP_DONE:
            for(i = 0; i < (sflashs.xfer_len - 5); i ++) {
                rx_data[i] = sflashs.rx_buf[i + 5];
            }            
            sflashs.state = SPI_FLASH_STATE_IDLE;
            ret = sflashs.xfer_len - 5;
            break;
        case SPI_FLASH_STATE_READ_MFG_DEV_ID_DONE:
            for(i = 0; i < 2; i ++) {
                rx_data[i] = sflashs.rx_buf[i + 4];
            }            
            sflashs.state = SPI_FLASH_STATE_IDLE;
            ret = 2;
            break;
        case SPI_FLASH_STATE_READ_STATUS_REG_DONE:
            rx_data[0] = sflashs.rx_buf[1];
            sflashs.state = SPI_FLASH_STATE_IDLE;
            ret = 1;
            break;
        case SPI_FLASH_STATE_READ_MEM_DONE:
            for(i = 0; i < (sflashs.xfer_len - 4); i ++) {
                rx_data[i] = sflashs.rx_buf[i + 4];
            }
            sflashs.state = SPI_FLASH_STATE_IDLE;
            ret = sflashs.xfer_len - 4;
            break;
        case SPI_FLASH_STATE_WRITE_ENABLE_DONE:
            sflashs.state = SPI_FLASH_STATE_IDLE;
            ret = SPI_FLASH_ERROR_OK;
            break;
        case SPI_FLASH_STATE_WRITE_MEM_DONE:
            sflashs.state = SPI_FLASH_STATE_IDLE;;
            ret = sflashs.xfer_len - 4;
            break;
        case SPI_FLASH_STATE_ERASE_MEM_DONE:
            sflashs.state = SPI_FLASH_STATE_IDLE;        
            ret = SPI_FLASH_ERROR_OK;            
            break;
        default:
            return SPI_FLASH_ERROR_INVALID_STATE;
    }
    return ret;
}

//
// callback handlers
//
// handle SPI init callback
void spi_flash_spi_init_cb(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    static DMA_HandleTypeDef hdma_tx;  // must be static
    static DMA_HandleTypeDef hdma_rx;  // must be static
    
    //
    // configure clocks and ports
    // 
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // /ROM_SS - GPIO
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, 1);  // CS defaults high

    // SPI pins
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    
    // SCLK
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
   
    // MISO
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // MOSI
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  
    //
    // setup DMA streams
    //
    // DMA TX - DMA1 Stream 5 Channel 0
    hdma_tx.Instance = DMA1_Stream5;
    hdma_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_tx.Init.Mode = DMA_NORMAL;
    hdma_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;         
    hdma_tx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_tx.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;
    HAL_DMA_Init(&hdma_tx);   
    // link the DMA with the SPI handle
    __HAL_LINKDMA(&spi_flash_spi_handle, hdmatx, hdma_tx);
    
    // DMA RX - DMA1 Stream 0 Channel 0
    hdma_rx.Instance = DMA1_Stream0;
    hdma_rx.Init.Channel = DMA_CHANNEL_0;
    hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_rx.Init.Mode = DMA_NORMAL;
    hdma_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;         
    hdma_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_rx.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;
    HAL_DMA_Init(&hdma_rx);
    // link the DMA with the SPI handle
    __HAL_LINKDMA(&spi_flash_spi_handle, hdmarx, hdma_rx);
    
    //
    // setup interrupts
    //
    // TX complete interrupt
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, INT_PRIO_SPI_FLASH_DMA_TX, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    
    // RX complete interrupt
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, INT_PRIO_SPI_FLASH_DMA_RX, 0);   
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

// handle SPI transfer complete callback
void spi_flash_spi_txrx_cplt_cb(void) {
    __HAL_UNLOCK(spi_flash_spi_handle.hdmatx);
    __HAL_UNLOCK(spi_flash_spi_handle.hdmarx);

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, 1);  // raise CS line    
    // switch states when transfer completes
    switch(sflashs.state) {
        case SPI_FLASH_STATE_READ_SFDP:
            sflashs.state = SPI_FLASH_STATE_READ_SFDP_DONE;
            break;
        case SPI_FLASH_STATE_READ_MFG_DEV_ID:
            sflashs.state = SPI_FLASH_STATE_READ_MFG_DEV_ID_DONE;
            break;
        case SPI_FLASH_STATE_READ_STATUS_REG:
            sflashs.state = SPI_FLASH_STATE_READ_STATUS_REG_DONE;
            break;
        case SPI_FLASH_STATE_READ_MEM:
            sflashs.state = SPI_FLASH_STATE_READ_MEM_DONE;
            break;
        case SPI_FLASH_STATE_WRITE_ENABLE:
            sflashs.state = SPI_FLASH_STATE_WRITE_ENABLE_DONE;
            break;
        case SPI_FLASH_STATE_WRITE_MEM:
            sflashs.state = SPI_FLASH_STATE_WRITE_MEM_DONE;
            break;
        case SPI_FLASH_STATE_ERASE_MEM:
            sflashs.state = SPI_FLASH_STATE_ERASE_MEM_DONE;
            break;
        default:
            sflashs.state = SPI_FLASH_STATE_IDLE;
            break;
    }
}

// start a transfer - returns -1 if the transfer could not be started
int spi_flash_start_xfer(uint8_t *tx_buf, uint8_t *rx_buf, int len) {
    // start the transfer
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, 0);  // lower the CS line
    if(HAL_SPI_TransmitReceive_DMA(&spi_flash_spi_handle,
            (uint8_t *)tx_buf,
            (uint8_t *)rx_buf, 
            len) != HAL_OK) {
        return SPI_FLASH_ERROR_START_ERROR;
    }
    return SPI_FLASH_ERROR_OK;
}

