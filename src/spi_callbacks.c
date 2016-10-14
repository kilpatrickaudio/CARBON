/*
 * CARBON Sequencer SPI Callback Handler
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
 * Notes on this module:
 *
 * Due to the fact that the STM32 Standard Peripheral Library
 * uses callbacks to configure the SPI peripheral, it is necessary
 * to implement a single callback to handle all three SPI ports for
 * different functions. This module acts as a callback dispatcher so
 * that each of the modules which use SPI can have code which is
 * unrelated to the others. The library is braindead so this is the
 * workaround.
 *
 */
#include "spi_callbacks.h"
#include "config.h"
#include "util/log.h"

SPI_HandleTypeDef *spi_handle[SPI_NUM_CHANNELS];
void (*spi_init_cb[SPI_NUM_CHANNELS])(void);
void (*spi_tx_cplt_cb[SPI_NUM_CHANNELS])(void);
void (*spi_rx_cplt_cb[SPI_NUM_CHANNELS])(void);
void (*spi_txrx_cplt_cb[SPI_NUM_CHANNELS])(void);
void (*spi_tx_half_cplt_cb[SPI_NUM_CHANNELS])(void);
void (*spi_rx_half_cplt_cb[SPI_NUM_CHANNELS])(void);
void (*spi_txrx_half_cplt_cb[SPI_NUM_CHANNELS])(void);
void (*spi_error_cb[SPI_NUM_CHANNELS])(void);

// init the SPI callbacks handler
void spi_callbacks_init(void) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        spi_handle[i] = NULL;
        spi_init_cb[i] = NULL;
        spi_tx_cplt_cb[i] = NULL;
        spi_rx_cplt_cb[i] = NULL;
        spi_txrx_cplt_cb[i] = NULL;
        spi_tx_half_cplt_cb[i] = NULL;
        spi_rx_half_cplt_cb[i] = NULL;
        spi_txrx_half_cplt_cb[i] = NULL;
        spi_error_cb[i] = NULL;
    }
}

// register SPI handle for a channel
void spi_callbacks_register_handle(int channel, 
        SPI_HandleTypeDef *hspi, void *init_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_handle[channel] = hspi;
    spi_init_cb[channel] = init_cb;
}    

// register a TX complete callback
void spi_callbacks_register_tx_cb(int channel, void *tx_cplt_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_tx_cplt_cb[channel] = tx_cplt_cb;
}

// register an RX complete callback
void spi_callbacks_register_rx_cb(int channel, void *rx_cplt_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_rx_cplt_cb[channel] = rx_cplt_cb;
}

// register a TXRX complete callback
void spi_callbacks_register_txrx_cb(int channel, void *txrx_cplt_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_txrx_cplt_cb[channel] = txrx_cplt_cb;
}


// register a TX half complete callback
void spi_callbacks_register_tx_half_cb(int channel, void *tx_half_cplt_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_tx_half_cplt_cb[channel] = tx_half_cplt_cb;
}

// register a RX half complete callback
void spi_callbacks_register_rx_half_cb(int channel, void *rx_half_cplt_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_rx_half_cplt_cb[channel] = rx_half_cplt_cb;
}

// register a TXRX half complete callback
void spi_callbacks_register_txrx_half_cb(int channel, void *txrx_half_cplt_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_txrx_half_cplt_cb[channel] = txrx_half_cplt_cb;
}

// register error callback
void spi_callbacks_register_error_cb(int channel, void *error_cb) {
    if(channel < 0 || channel >= SPI_NUM_CHANNELS) {
        return;
    }
    spi_error_cb[channel] = error_cb;
}

//
// callbacks
//
// handle SPI setup
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_init_cb[i] != NULL) {
            (*spi_init_cb[i])();  // call the stored callback
            return;
        }
    }
}

// handle TX transfer complete
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_tx_cplt_cb[i] != NULL) {
            (*spi_tx_cplt_cb[i])();  // call the stored callback
            return;
        }
    }
}

// handle RX transfer complete
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_rx_cplt_cb[i] != NULL) {
            (*spi_rx_cplt_cb[i])();  // call the stored callback
            return;
        }
    }
}

// handle TXRX transfer complete
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_txrx_cplt_cb[i] != NULL) {
            (*spi_txrx_cplt_cb[i])();  // call the stored callback
            return;
        }
    }
}

// handle TX half complete
void HAL_SPI_TxHalfCpltCallback(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_tx_half_cplt_cb[i] != NULL) {
            (*spi_tx_half_cplt_cb[i])();  // call the stored callback
            return;
        }
    }
}

// handle RX half complete
void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_rx_half_cplt_cb[i] != NULL) {
            (*spi_rx_half_cplt_cb[i])();  // call the stored callback
            return;
        }
    }
}

// handle TXRX half complete
void HAL_SPI_TxRxHalfCpltCallback(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_txrx_half_cplt_cb[i] != NULL) {
            (*spi_txrx_half_cplt_cb[i])();  // call the stored callback
            return;
        }
    }
}

// handle error
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    int i;
    for(i = 0; i < SPI_NUM_CHANNELS; i ++) {
        // is this the channel we want?
        if(hspi == spi_handle[i] && spi_error_cb[i] != NULL) {
            (*spi_error_cb[i])();  // call the stored callback
            return;
        }
    }
}

