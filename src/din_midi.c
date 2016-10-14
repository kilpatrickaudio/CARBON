/*
 * CARBON Sequencer DIN MIDI Interface
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
 *  - PA0       - MIDI TX1                  - UART4 TX
 *  - PA1       - MIDI RX1                  - UART4 RX
 *  - PA2       - MIDI TX2                  - USART2 TX
 *
 */
#include "din_midi.h"
#include "config.h"
#include "debug.h"
#include "stm32f4xx_hal.h"
#include "midi/midi_stream.h"
#include <inttypes.h>
#include "util/log.h"

// FIFOs for use with DMA
#define DIN_MIDI_TX_BUFSIZE 16
#define DIN_MIDI_RX_BUFSIZE 16
#define DIN_MIDI_RX_BUFMASK (DIN_MIDI_RX_BUFSIZE - 1)
uint8_t din_midi_tx1_buf[DIN_MIDI_TX_BUFSIZE];
uint8_t din_midi_rx1_buf[DIN_MIDI_RX_BUFSIZE];
uint8_t din_midi_tx2_buf[DIN_MIDI_TX_BUFSIZE];
int din_midi_rx_inp;
int din_midi_rx_outp;
int din_midi_tx1_done;
int din_midi_tx2_done;
UART_HandleTypeDef din_midi1_handle;  // DIN1 RX and TX - UART4
UART_HandleTypeDef din_midi2_handle;  // DIN2 TX - USART2
DMA_HandleTypeDef din_midi1_dma_rx_handle;  // DMA handle for DIN1 RX
DMA_HandleTypeDef din_midi1_dma_tx_handle;  // DMA handle for DIN1 TX
DMA_HandleTypeDef din_midi2_dma_tx_handle;  // DMA handle for DIN2 TX

// init the DIN MIDI
void din_midi_init(void) {
    // setup DIN RX1 and TX1
    din_midi1_handle.Instance          = UART4;
    din_midi1_handle.Init.BaudRate     = 31250;
    din_midi1_handle.Init.WordLength   = UART_WORDLENGTH_8B;
    din_midi1_handle.Init.StopBits     = UART_STOPBITS_1;
    din_midi1_handle.Init.Parity       = UART_PARITY_NONE;
    din_midi1_handle.Init.Mode         = UART_MODE_TX_RX;
    if(HAL_UART_Init(&din_midi1_handle) != HAL_OK) {
        // XXX handle error
    }
    
    // setup DIN TX2
    din_midi2_handle.Instance          = USART2;
    din_midi2_handle.Init.BaudRate     = 31250;
    din_midi2_handle.Init.WordLength   = USART_WORDLENGTH_8B;
    din_midi2_handle.Init.StopBits     = USART_STOPBITS_1;
    din_midi2_handle.Init.Parity       = USART_PARITY_NONE;
    din_midi2_handle.Init.Mode         = USART_MODE_TX;
    if(HAL_UART_Init(&din_midi2_handle) != HAL_OK) {
        // XXX handle error
    }

    // reset the buffer pointers
    din_midi_rx_inp = 0;
    din_midi_rx_outp = 0;
    din_midi_tx1_done = 1;  // start with done transfer
    din_midi_tx2_done = 1;  // start with done transfer
    
    // start the first DMA RX transfer
    if(HAL_UART_Receive_DMA(&din_midi1_handle, (uint8_t *)din_midi_rx1_buf, 
            DIN_MIDI_RX_BUFSIZE) != HAL_OK) {
        // XXX handle error
    }    
}

// run the DIN MIDI timer task
void din_midi_timer_task(void) {
    int count;
    struct midi_msg msg;
   
    // DIN1 TX
    if(midi_stream_data_available(MIDI_PORT_DIN1_OUT) && din_midi_tx1_done) {
        count = 0;
        while(midi_stream_data_available(MIDI_PORT_DIN1_OUT) && 
                count < (DIN_MIDI_TX_BUFSIZE - 2)) {
            midi_stream_receive_msg(MIDI_PORT_DIN1_OUT, &msg);
            switch(msg.len) {
                case 1:
                    din_midi_tx1_buf[count++] = msg.status;
                    break;
                case 2:
                    din_midi_tx1_buf[count++] = msg.status;
                    din_midi_tx1_buf[count++] = msg.data0;
                    break;
                case 3:
                    din_midi_tx1_buf[count++] = msg.status;
                    din_midi_tx1_buf[count++] = msg.data0;
                    din_midi_tx1_buf[count++] = msg.data1;
                    break;
            }            
        }
        if(count > 0) {
            din_midi_tx1_done = 0;
            HAL_UART_Transmit_DMA(&din_midi1_handle, (uint8_t *)din_midi_tx1_buf, count);
        }
    }

    // DIN2 TX
    if(midi_stream_data_available(MIDI_PORT_DIN2_OUT) && din_midi_tx2_done) {
        count = 0;
        while(midi_stream_data_available(MIDI_PORT_DIN2_OUT) && 
                count < (DIN_MIDI_TX_BUFSIZE - 2)) {
            midi_stream_receive_msg(MIDI_PORT_DIN2_OUT, &msg);
            switch(msg.len) {
                case 1:
                    din_midi_tx2_buf[count++] = msg.status;
                    break;
                case 2:
                    din_midi_tx2_buf[count++] = msg.status;
                    din_midi_tx2_buf[count++] = msg.data0;
                    break;
                case 3:
                    din_midi_tx2_buf[count++] = msg.status;
                    din_midi_tx2_buf[count++] = msg.data0;
                    din_midi_tx2_buf[count++] = msg.data1;
                    break;
            }            
        }
        if(count > 0) {
            din_midi_tx2_done = 0;
            HAL_UART_Transmit_DMA(&din_midi2_handle, (uint8_t *)din_midi_tx2_buf, count);
        }
    }

    // DIN1 RX
    din_midi_rx_inp = DIN_MIDI_RX_BUFSIZE - __HAL_DMA_GET_COUNTER(&din_midi1_dma_rx_handle);
    while(din_midi_rx_inp != din_midi_rx_outp) {
        midi_stream_send_byte(MIDI_PORT_DIN1_IN, din_midi_rx1_buf[din_midi_rx_outp]);
        din_midi_rx_outp = (din_midi_rx_outp + 1) & DIN_MIDI_RX_BUFMASK;
    }
}

//
// callbacks
//
// the UART transfer is complete
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
    if(huart == &din_midi1_handle) {
        din_midi_tx1_done = 1;
        return;
    }
    if(huart == &din_midi2_handle) {
        din_midi_tx2_done = 1;    
        return;
    }
}

// init the UART
void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    static GPIO_InitTypeDef GPIO_InitStruct;

    // DIN1 TX / RX
    if(huart == &din_midi1_handle) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();
        
        // configure pins
        GPIO_InitStruct.Pin = (GPIO_PIN_0 | GPIO_PIN_1);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
        GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
       
        //
        // configure TX DMA
        //
        din_midi1_dma_tx_handle.Instance = DMA1_Stream4;
        din_midi1_dma_tx_handle.Init.Channel = DMA_CHANNEL_4;
        din_midi1_dma_tx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
        din_midi1_dma_tx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
        din_midi1_dma_tx_handle.Init.MemInc  = DMA_MINC_ENABLE;
        din_midi1_dma_tx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        din_midi1_dma_tx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        din_midi1_dma_tx_handle.Init.Mode = DMA_NORMAL;
        din_midi1_dma_tx_handle.Init.Priority = DMA_PRIORITY_LOW;
        din_midi1_dma_tx_handle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        din_midi1_dma_tx_handle.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
        din_midi1_dma_tx_handle.Init.MemBurst = DMA_MBURST_SINGLE;
        din_midi1_dma_tx_handle.Init.PeriphBurst = DMA_PBURST_SINGLE;
        HAL_DMA_Init(&din_midi1_dma_tx_handle);
        
        // link the DMA with the USART
        __HAL_LINKDMA(huart, hdmatx, din_midi1_dma_tx_handle);
                
        //
        // configure RX DMA
        //
        din_midi1_dma_rx_handle.Instance = DMA1_Stream2;
        din_midi1_dma_rx_handle.Init.Channel = DMA_CHANNEL_4;
        din_midi1_dma_rx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
        din_midi1_dma_rx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
        din_midi1_dma_rx_handle.Init.MemInc  = DMA_MINC_ENABLE;
        din_midi1_dma_rx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        din_midi1_dma_rx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        din_midi1_dma_rx_handle.Init.Mode = DMA_CIRCULAR;
        din_midi1_dma_rx_handle.Init.Priority = DMA_PRIORITY_LOW;
        din_midi1_dma_rx_handle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        din_midi1_dma_rx_handle.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
        din_midi1_dma_rx_handle.Init.MemBurst = DMA_MBURST_SINGLE;
        din_midi1_dma_rx_handle.Init.PeriphBurst = DMA_PBURST_SINGLE;
        HAL_DMA_Init(&din_midi1_dma_rx_handle);

        // link the DMA with the UART       
        __HAL_LINKDMA(huart, hdmarx, din_midi1_dma_rx_handle);
        
        // set up the interrupt handlers - this resets state and stuff
        HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, INT_PRIO_DIN_MIDI_DMA_TX1, 0);  // DIN 1 TX
        HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

        HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, INT_PRIO_DIN_MIDI_DMA_RX1, 0);  // DIN 1 RX
        HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
        
        HAL_NVIC_SetPriority(UART4_IRQn, INT_PRIO_DIN_MIDI_DMA_UART1, 0);  // DIN 1 USART
        HAL_NVIC_EnableIRQ(UART4_IRQn);  
    }
    // DIN2 TX
    else if(huart == &din_midi2_handle) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        // configure pins
        GPIO_InitStruct.Pin = (GPIO_PIN_2);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
        
        //
        // configure TX DMA
        //
        din_midi2_dma_tx_handle.Instance = DMA1_Stream6;
        din_midi2_dma_tx_handle.Init.Channel = DMA_CHANNEL_4;
        din_midi2_dma_tx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
        din_midi2_dma_tx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
        din_midi2_dma_tx_handle.Init.MemInc  = DMA_MINC_ENABLE;
        din_midi2_dma_tx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        din_midi2_dma_tx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        din_midi2_dma_tx_handle.Init.Mode = DMA_NORMAL;
        din_midi2_dma_tx_handle.Init.Priority = DMA_PRIORITY_LOW;
        din_midi2_dma_tx_handle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        din_midi2_dma_tx_handle.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
        din_midi2_dma_tx_handle.Init.MemBurst = DMA_MBURST_SINGLE;
        din_midi2_dma_tx_handle.Init.PeriphBurst = DMA_PBURST_SINGLE;
        HAL_DMA_Init(&din_midi2_dma_tx_handle);
        
        // link the DMA with the USART
        __HAL_LINKDMA(huart, hdmatx, din_midi2_dma_tx_handle);
        
        // set up the interrupt handlers - this resets state and stuff
        HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, INT_PRIO_DIN_MIDI_DMA_TX2, 0);  // DIN 2 TX
        HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
        
        HAL_NVIC_SetPriority(USART2_IRQn, INT_PRIO_DIN_MIDI_DMA_UART2, 0);  // DIN 2 UART
        HAL_NVIC_EnableIRQ(USART2_IRQn);    
    }
}

