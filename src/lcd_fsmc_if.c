/*
 * LCD Parallel FSMC Driver
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
 *  - PD14      - LCD_D0                - output / FSMC_D0
 *  - PD15      - LCD_D1                - output / FSMC_D1
 *  - PD0       - LCD_D2                - output / FSMC_D2
 *  - PD1       - LCD_D3                - output / FSMC_D3
 *  - PE7       - LCD_D4                - output / FSMC_D4
 *  - PE8       - LCD_D5                - output / FSMC_D5
 *  - PE9       - LCD_D6                - output / FSMC_D6
 *  - PE10      - LCD_D7                - output / FSMC_D7
 *
 *  - PD3       - LCD_RS                - output
 *  - PD4       - /LCD_RD               - output / FSMC_NRE
 *  - PD5       - /LCD_WR               - output / FSMC_NWE
 *  - PD7       - /LCD_CS               - output / FSMC_NCE2
 *
 */
#include "lcd_fsmc_if.h"
#include "stm32f4xx_hal.h"

SRAM_HandleTypeDef lcd_sram;
FSMC_NORSRAM_TimingTypeDef lcd_timing;
#define SRAM_BANK_ADDR ((uint32_t)0x60000000)
#define LCD_FSMC_RS_L GPIOD->BSRR = (uint32_t)0x0008 << 16
#define LCD_FSMC_RS_H GPIOD->BSRR = 0x0008

#define LCD_ENDIAN_FIX  // uncomment to swap bytes for 16 bit pixels
#ifdef LCD_ENDIAN_FIX
#define LCD_BUF_LEN 1024
#endif

struct lcd_fsmc_state {
    int init;
#ifdef LCD_ENDIAN_FIX
    uint16_t lcd_buf[LCD_BUF_LEN];
#endif
};
// put data into CCMRAM instead of regular RAM
struct lcd_fsmc_state lcds __attribute__ ((section (".ccm")));

// init the FSMC module
void lcd_fsmc_if_init(void) {
    lcds.init = 0;
}

// init the interface
void lcd_fsmc_if_init_if(void) {
    // set up the SRAM (LCD) device
    lcd_sram.Instance = FSMC_NORSRAM_DEVICE;  // bank 1
    lcd_sram.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;  // bank 1E

/*
    // test
    lcd_timing.AddressSetupTime = 10;  // 0-16 clocks
    lcd_timing.AddressHoldTime = 10;  // no effect in mode A
    lcd_timing.DataSetupTime = 16;  // 1-256 clocks
    lcd_timing.BusTurnAroundDuration = 10;  // 0-15 clocks
    lcd_timing.CLKDivision = 0;  // no effect in mode A
    lcd_timing.DataLatency = 0;  // no effect in mode A
    lcd_timing.AccessMode = FSMC_ACCESS_MODE_A;
*/

    // orig
    lcd_timing.AddressSetupTime = 10;  // 0-16 clocks
    lcd_timing.AddressHoldTime = 10;  // no effect in mode A
    lcd_timing.DataSetupTime = 10;  // 1-256 clocks
    lcd_timing.BusTurnAroundDuration = 10;  // 0-15 clocks
    lcd_timing.CLKDivision = 0;  // no effect in mode A
    lcd_timing.DataLatency = 0;  // no effect in mode A
    lcd_timing.AccessMode = FSMC_ACCESS_MODE_A;

    lcd_sram.Init.NSBank = FSMC_NORSRAM_BANK1;
    lcd_sram.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    lcd_sram.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    lcd_sram.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_8;  // 8 bit bus
    lcd_sram.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    lcd_sram.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    lcd_sram.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    lcd_sram.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    lcd_sram.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    lcd_sram.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    lcd_sram.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
    lcd_sram.Init.AsynchronousWait = FSMC_EXTENDED_MODE_DISABLE;
    lcd_sram.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;

    // init SRAM controller
    if(HAL_SRAM_Init(&lcd_sram, &lcd_timing, &lcd_timing) != HAL_OK) {
        // XXX handle error
    }
    lcds.init = 1;
}

// deinit the interface
void lcd_fsmc_if_deinit_if(void) {
    HAL_SRAM_DeInit(&lcd_sram);
    LCD_FSMC_RS_L;
    lcds.init = 0;
}

// write to the LCD with 8 bit buffer - len = total bytes
void lcd_fsmc_if_write8(uint8_t *buf, int len, int rs) {
    if(!lcds.init) {
        return;
    }
    if(rs) {
        LCD_FSMC_RS_H;
    }
    else {
        LCD_FSMC_RS_L;
    }

    HAL_SRAM_Write_8b(&lcd_sram, (uint32_t *)SRAM_BANK_ADDR, buf, len);
    while(HAL_SRAM_GetState(&lcd_sram) != HAL_SRAM_STATE_READY);
}

// read from the LCD with 8 bit buffer - len = total bytes
void lcd_fsmc_if_read8(uint8_t *buf, int len, int rs) {
    if(!lcds.init) {
        return;
    }
    if(rs) {
        LCD_FSMC_RS_H;
    }
    else {
        LCD_FSMC_RS_L;
    }

    HAL_SRAM_Read_8b(&lcd_sram, (uint32_t *)SRAM_BANK_ADDR, buf, len);
    while(HAL_SRAM_GetState(&lcd_sram) != HAL_SRAM_STATE_READY);
}

// write to the LCD with 16 bit buffer - len = total words
void lcd_fsmc_if_write16(uint16_t *buf, int len, int rs) {
#ifdef LCD_ENDIAN_FIX
    int i, writelen;
#endif
    if(!lcds.init) {
        return;
    }
    if(rs) {
        LCD_FSMC_RS_H;
    }
    else {
        LCD_FSMC_RS_L;
    }
#ifdef LCD_ENDIAN_FIX
    // XXX fix endiannesss problem
    for(i = 0; i < len && i < LCD_BUF_LEN; i ++) {
        lcds.lcd_buf[i] = __REV16(buf[i]);
    }
    writelen = len;
    if(writelen > LCD_BUF_LEN) {
        writelen = LCD_BUF_LEN;
    }

    HAL_SRAM_Write_16b(&lcd_sram, (uint32_t *)SRAM_BANK_ADDR, lcds.lcd_buf, writelen);
    while(HAL_SRAM_GetState(&lcd_sram) != HAL_SRAM_STATE_READY);
#else
    HAL_SRAM_Write_16b(&lcd_sram, (uint32_t *)SRAM_BANK_ADDR, buf, len);
    while(HAL_SRAM_GetState(&lcd_sram) != HAL_SRAM_STATE_READY);
#endif
}

//
// callback handlers
//
// callback for SRAM init
void HAL_SRAM_MspInit(SRAM_HandleTypeDef *hsram) {
    GPIO_InitTypeDef GPIO_InitStruct;

    // enable FSMC clock
    __HAL_RCC_FSMC_CLK_ENABLE();

    // enable GPIO clocks
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // common GPIO settings - switch to FSMC
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;

    // GPIOD - FSMC stuff
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | \
        GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // GPIOE - FSMC stuff
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // RS pin - enable output
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

// callback from SRAM deinit
void HAL_SRAM_MspDeInit(SRAM_HandleTypeDef *hsram) {
    GPIO_InitTypeDef GPIO_InitStruct;

    // enable FSMC clock
    __HAL_RCC_FSMC_CLK_ENABLE();

    // enable GPIO clock
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // common GPIO settings - switch to input with pulldown
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;

    // GPIOD - FSMC pins
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | \
        GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // GPIOE - FSMC pins
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // RS pin - disable output and pull down
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}
