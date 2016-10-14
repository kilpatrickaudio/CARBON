/*
 * CARBON Sequencer IOCTLs
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
#include "ioctl.h"
#include "stm32f4xx_hal.h"
#include <inttypes.h>

// settings
#define IOCTL_DC_VSENSE_LEN 8  // sample average count
    
ADC_HandleTypeDef ioctl_adc_handle;

// IOCTL state
struct ioctl_state {
    uint16_t adc_val;  // recent ADC val
    int dc_vsense_acc;  // accumulator for DC vsense
    int dc_vsense_count;  // DC vsense counter for averaging
    int dc_vsense_avg;  // DC vsense averaged val
};
struct ioctl_state ios;

// init the IOCTL
void ioctl_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_ChannelConfTypeDef adc_channel_conf;
    
    ios.dc_vsense_acc = 0;
    ios.dc_vsense_count = 0;
    ios.dc_vsense_avg = 24000;  // default to a good value

    // set up I/O
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // analog power control - PC1
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct); 

    // power button - PC0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct); 

    // analog input for DC voltage sense - PA3    
    // set up the module
    ioctl_adc_handle.Instance = ADC3;
    ioctl_adc_handle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
    ioctl_adc_handle.Init.Resolution = ADC_RESOLUTION_12B;
    ioctl_adc_handle.Init.ScanConvMode = DISABLE;
    ioctl_adc_handle.Init.ContinuousConvMode = ENABLE;
    ioctl_adc_handle.Init.DiscontinuousConvMode = DISABLE;
    ioctl_adc_handle.Init.NbrOfDiscConversion = 0;
    ioctl_adc_handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    ioctl_adc_handle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
    ioctl_adc_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    ioctl_adc_handle.Init.NbrOfConversion = 1;
    ioctl_adc_handle.Init.DMAContinuousRequests = ENABLE;
    ioctl_adc_handle.Init.EOCSelection = DISABLE;
    if(HAL_ADC_Init(&ioctl_adc_handle) != HAL_OK) {
        // XXX handle error
    }
    // configure ADC channel
    adc_channel_conf.Channel = ADC_CHANNEL_3;
    adc_channel_conf.Rank = 1;
    adc_channel_conf.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    adc_channel_conf.Offset = 0;
    if(HAL_ADC_ConfigChannel(&ioctl_adc_handle, &adc_channel_conf) != HAL_OK) {
        // XXX handle error
    }

    // start conversion
    if(HAL_ADC_Start_DMA(&ioctl_adc_handle, (uint32_t*)&ios.adc_val, 1) != HAL_OK) {
        // XXX handle error
    }
    
    // reset stuff
    ioctl_set_analog_pwr_ctrl(0);  // off
}

// run the IOCTL timer task
void ioctl_timer_task(void) {
    static int task_count = 0;

    // measure DC vsense
    if((task_count & 0x0f) == 0) {
        ios.dc_vsense_acc += ios.adc_val;
        ios.dc_vsense_count ++;
        if(ios.dc_vsense_count == IOCTL_DC_VSENSE_LEN) {
            ios.dc_vsense_avg = ios.dc_vsense_acc / IOCTL_DC_VSENSE_LEN;
            ios.dc_vsense_acc = 0;
            ios.dc_vsense_count = 0;
        }
    }
    task_count ++;
}

// set the analog power control state - 0 = off, 1 = on
void ioctl_set_analog_pwr_ctrl(int state) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, state & 0x01);
}

// get the state of the power button - 0 = not pressed, 1 = pressed
int ioctl_get_power_sw(void) {
    return (~HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0)) & 0x01;
}

// get the current DC vsense value in mV
int ioctl_get_dc_vsense(void) {
    return (int)((float)(ios.dc_vsense_avg) * 9.87f);
}

//
// callback to handle ADC init
//
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc) {
    GPIO_InitTypeDef GPIO_InitStruct;
    static DMA_HandleTypeDef adc_dma_handle;

    // set up stuff that would normally be in the MSP callback
    // enable GPIO clock
    __HAL_RCC_GPIOA_CLK_ENABLE();
    // ADC clock enable
    __HAL_RCC_ADC3_CLK_ENABLE();
    // DMA2 clock enable
    __HAL_RCC_DMA2_CLK_ENABLE();
    // configure GPIO 
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // configure DMA streams
    adc_dma_handle.Instance = DMA2_Stream0;    
    adc_dma_handle.Init.Channel = DMA_CHANNEL_2;
    adc_dma_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    adc_dma_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    adc_dma_handle.Init.MemInc = DMA_MINC_ENABLE;
    adc_dma_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    adc_dma_handle.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    adc_dma_handle.Init.Mode = DMA_CIRCULAR;
    adc_dma_handle.Init.Priority = DMA_PRIORITY_HIGH;
    adc_dma_handle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;         
    adc_dma_handle.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
    adc_dma_handle.Init.MemBurst = DMA_MBURST_SINGLE;
    adc_dma_handle.Init.PeriphBurst = DMA_PBURST_SINGLE; 
    HAL_DMA_Init(&adc_dma_handle);
    // associate the initialized DMA handle to the the ADC handle
    __HAL_LINKDMA(hadc, DMA_Handle, adc_dma_handle);
    // DMA is not required because ADC is continuous
}
