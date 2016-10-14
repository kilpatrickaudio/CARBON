/*
 * CARBON Sequencer Analog Outputs
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
 * Hardware I/O:
 *  - PA4       - /DAC0_SS          - PA4 GPIO
 *  - PE2       - /DAC1_SS          - PE2 GPIO
 *  - PE3       - /GATECLK_SS       - PE3 GPIO
 *  - PA5       - DAC_SCLK          - SPI1 SCLK
 *  - PA7       - DAC_MOSI          - SPI1 MOSI
 *
 */
#include "analog_out.h"
#include "stm32f4xx_hal.h"
#include "config.h"
#include "spi_callbacks.h"
#include <inttypes.h>

// settings
#define AOUT_CVGATE_NUM_CHANS 4

SPI_HandleTypeDef aout_spi_handle;  // SPI1 DAC and gate/clock outs

// state
struct analog_out_state {
    int cv_desired[AOUT_CVGATE_NUM_CHANS];
    int cv_current[AOUT_CVGATE_NUM_CHANS];
    int gate_current;
    int gate_desired;
    int beep_enable;
};
struct analog_out_state aouts;

// callback handlers
void aout_spi_init_cb(void);
void aout_spi_tx_cplt_cb(void);

// local functions
void aout_update_dac(int chan);
void aout_update_gate(void);

// init the analog outs
void analog_out_init(void) {
    int i;

    // clear state
    for(i = 0; i < AOUT_CVGATE_NUM_CHANS; i ++) {
        aouts.cv_current[i] = 0xfff;
        aouts.cv_desired[i] = 0x800;  // force update
    }
    aouts.gate_current = 0xff;
    aouts.gate_desired = 0;  // force update
    aouts.beep_enable = 0;

    // register the SPI callbacks
    spi_callbacks_register_handle(SPI_CHANNEL_DAC, &aout_spi_handle,
        aout_spi_init_cb);
    spi_callbacks_register_tx_cb(SPI_CHANNEL_DAC, aout_spi_tx_cplt_cb);

    // setup SPI
    aout_spi_handle.Instance = SPI1;
    aout_spi_handle.Init.Mode = SPI_MODE_MASTER;
    aout_spi_handle.Init.Direction = SPI_DIRECTION_1LINE;
    aout_spi_handle.Init.DataSize = SPI_DATASIZE_8BIT;
    aout_spi_handle.Init.CLKPolarity = SPI_POLARITY_HIGH;
    aout_spi_handle.Init.CLKPhase = SPI_PHASE_1EDGE;
    aout_spi_handle.Init.NSS = SPI_NSS_SOFT;
    aout_spi_handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    aout_spi_handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
    aout_spi_handle.Init.TIMode = SPI_TIMODE_DISABLE;    
    aout_spi_handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    aout_spi_handle.Init.CRCPolynomial = 10;
    if(HAL_SPI_Init(&aout_spi_handle) != HAL_OK) {
        // XXX handle error
    }
}

// run the analog out timer task
void analog_out_timer_task(void) {
    static int phase = 0;

    if(HAL_SPI_GetState(&aout_spi_handle) == HAL_SPI_STATE_READY) {
        switch(phase) {
            case 0x00:
                aout_update_dac(0);
                break;
            case 0x02:
                aout_update_dac(1);
                break;
            case 0x04:
                aout_update_dac(2);
                break;
            case 0x06:
                aout_update_dac(3);
                break;
            case 0x01:
            case 0x03:
            case 0x05:
            case 0x07:
                // beep the metronome speaker
                if(aouts.beep_enable) {
                    if(phase & 0x02) {
                        aouts.gate_desired |= 0x01;
                    }
                    else {
                        aouts.gate_desired &= ~0x01;
                    }
                }
                else {
                    aouts.gate_desired &= ~0x01;                
                }
                aout_update_gate();
                break;
        }        
        phase = (phase + 1) & 0x07;
    }
}

// set CV output value - chan: 0-3 = CV 1-4, val: 12 bit
void analog_out_set_cv(int chan, int val) {
    if(chan < 0 || chan >= AOUT_CVGATE_NUM_CHANS) {
        return;
    }
    aouts.cv_desired[chan] = val & 0xfff;
}

// set a gate output - chan: 0-3 = GATE 1-4, state: 0 = off, 1 = on
void analog_out_set_gate(int chan, int state) {
    int bit;
    switch(chan) {
        case 0:
            bit = 0x02;
            break;
        case 1:
            bit = 0x08;
            break;
        case 2:
            bit = 0x10;
            break;
        case 3:
            bit = 0x20;
            break;
        default:
            return;
    }
    if(state) {
        aouts.gate_desired |= bit;
    }
    else {
        aouts.gate_desired &= ~bit;
    }
}

// set the clock output
void analog_out_set_clock(int state) {
    if(state) {
        aouts.gate_desired |= 0x40;
    }
    else {
        aouts.gate_desired &= ~0x40;
    }
}

// set the reset output
void analog_out_set_reset(int state) {
    if(state) {
        aouts.gate_desired |= 0x80;
    }
    else {
        aouts.gate_desired &= ~0x80;
    }
}

// beep the metronome
void analog_out_beep_metronome(int enable) {
    if(enable) {
        aouts.beep_enable = 1;
    }
    else {
        aouts.beep_enable = 0;
    }
}

//
// callback handlers
//
// handle SPI init callback
void aout_spi_init_cb(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    
    //
    // configure clocks and ports
    // 
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    // slave selects
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, 1);  // default high
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, 1);  // default high
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 1);  // default high
    
    // SPI pins
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);    

    //
    // setup interrupts
    //
    // transfer complete interrupt
    HAL_NVIC_SetPriority(SPI1_IRQn, INT_PRIO_SPI_ANALOG_OUT, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

// handle SPI transfer complete callback
void aout_spi_tx_cplt_cb(void) {
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, 1);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, 1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 1);
}

//
// local functions
//
// update a DAC if necessary
void aout_update_dac(int chan) {
    int dirty = 0;
    int val;
    static uint8_t buf[2];

    switch(chan) {
        case 0:
            if(aouts.cv_current[0] != aouts.cv_desired[0]) {
                val = aouts.cv_desired[0];
                aouts.cv_current[0] = aouts.cv_desired[0];
                dirty = 1;
            }
            break;
        case 1:
            if(aouts.cv_current[1] != aouts.cv_desired[1]) {
                val = aouts.cv_desired[1];
                aouts.cv_current[1] = aouts.cv_desired[1];
                dirty = 1;
            }
            break;
        case 2:
            if(aouts.cv_current[2] != aouts.cv_desired[2]) {
                val = aouts.cv_desired[2];
                aouts.cv_current[2] = aouts.cv_desired[2];
                dirty = 1;
            }
            break;
        case 3:
            if(aouts.cv_current[3] != aouts.cv_desired[3]) {
                val = aouts.cv_desired[3];
                aouts.cv_current[3] = aouts.cv_desired[3];
                dirty = 1;
            }
            break;
        default:
            return;            
    }
    
    // nothing changed
    if(!dirty) {
        return;
    }
    
    //
    // write the message
    //
    buf[0] = 0x30;
    if(chan & 0x01) {
        buf[0] = 0xb0;
    }
    buf[0] |= (val >> 8) & 0x0f;
    buf[1] = val & 0xff;
    // DAC1
    if(chan & 0x02) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, 0);
    }
    // DAC0
    else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 0);
    }
    HAL_SPI_Transmit_IT(&aout_spi_handle, buf, 2);
}

// update the gate, clock, reset outputs if necessary
void aout_update_gate(void) {
    static uint8_t buf[2];
    if(aouts.gate_current != aouts.gate_desired) {
        buf[0] = 0;
        buf[1] = aouts.gate_desired;
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, 0);
        HAL_SPI_Transmit_IT(&aout_spi_handle, buf, 2);
        aouts.gate_current = aouts.gate_desired;
    }
}

