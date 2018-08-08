/*
 * CARBON Sequencer Main
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
#include "main.h"
#include "stm32f4xx_hal.h"
#include "config.h"
#include "ioctl.h"
#include "analog_out.h"
#include "config_store.h"
#include "cvproc.h"
#include "debug.h"
#include "delay.h"
#include "din_midi.h"
#include "ext_flash.h"
#include "power_ctrl.h"
#include "panel_if.h"
#include "spi_callbacks.h"
#include "util/time_utils.h"
#include "util/log.h"
#include "midi/midi_utils.h"
#include "midi/midi_stream.h"
#include "usbd_midi/usbd_midi.h"
#include "usbh_midi/usbh_midi.h"
#include "gui/panel.h"
#include "seq/seq_ctrl.h"
#include "gfx.h"
#include <stdlib.h>

#ifdef DEBUG_RT_TIMING
#warning main RT task timing enabled
#endif

// local functions
static void SystemClock_Config(void);
int startup_wait;

// main!
int main(void) {
    btime start_time;
    startup_wait = 1;

    // sysem setup
    HAL_Init();
    SystemClock_Config();

    // seed random number generator
    srand(0x12345678);  // need a more random seed

    // hardware init
    delay_init();  // must be before LCD driver
    spi_callbacks_init();  // must be before anything uses SPI
    debug_init();
    log_init();
    ioctl_init();
    analog_out_init();
    midi_stream_init();
    panel_if_init();
    ext_flash_init();
    config_store_init();
    usbd_midi_init();
    usbh_midi_init();
    din_midi_init();
    cvproc_init();  // analog_out must be set up already
    analog_out_init();
    // module init
    gfx_init();  // start up graphics driver and hardware
    seq_ctrl_init();  // everything must be ready
    power_ctrl_init();  // must cme after ioctl_init()

#ifdef DEBUG_RT_TIMING
    // set up instrumentation
    CoreDebug->DEMCR &= ~0x01000000;  // set up counter
    CoreDebug->DEMCR |=  0x01000000;
    DWT->CTRL &= ~0x00000001;  // enable counter
    DWT->CTRL |=  0x00000001;
    DWT->CYCCNT = 0; // reset the counter
#endif

    // unblock RT thread
    startup_wait = 0;  // cause main timer task to begin

    // power up timeout - 10ms
    start_time = time_utils_get_btime();
    while((int)(time_utils_get_btime() - start_time) < 10000);
    seq_ctrl_ui_task();

    // whee!
    while(1) {
        seq_ctrl_ui_task();
    }
}

// handle tasks for all parts of the system - runs every 500us
void main_timer_task(void) {
    static int32_t current_time = 0;
    static int task_div = 0;
#ifdef DEBUG_RT_TIMING
    uint32_t task_timing;
    static uint32_t task_timing_min = 0x7fffffff;
    static uint32_t task_timing_max = 0;
#endif

    // do this always - even before startup - 1000us
    if((task_div & 0x01) == 0) {
        delay_timer_task();
    }

    // XXX this could be more elegant
    // not ready to go yet
    if(startup_wait) {
        return;
    }

#ifdef DEBUG_RT_TIMING
    // handle task timing
    DWT->CYCCNT = 0;  // reset cycle counter
#endif

    // tasks - every 1000us
    if((task_div & 0x01) == 0) {
        time_utils_set_btime(current_time);
        panel_if_timer_task();  // do this first for nice LED dimming
        seq_ctrl_rt_task();  // sequencer realtime stuff
        din_midi_timer_task();  // hardware MIDI I/O
        ext_flash_timer_task();  // loading/saving to external flash
        usbd_midi_timer_task();  // USB device
        usbh_midi_timer_task();  // USB host
        config_store_timer_task();  // system config loading / saving
        cvproc_timer_task();  // hardware CV/gate/clock outputs
        power_ctrl_timer_task();  // power control monitoring / switching
    }

    // hardware I/O - every 250us
    ioctl_timer_task();
    analog_out_timer_task();

    // nom entropy to make it more random (since we only have one seed)
    if((task_div & 0xff) == 0) {
        rand();
    }

    // keep track of running time
    current_time += 500;  // XXX this could be done better
    task_div ++;

#ifdef DEBUG_OVER_MIDI
    if((task_div & 0xfff) == 0) {
        debug_send_active_sensing();
    }
#endif

#ifdef DEBUG_RT_TIMING
    // handle task timing
    task_timing = DWT->CYCCNT;  // get timing
    if(task_timing < task_timing_min) {
        task_timing_min = task_timing;
    }
    if(task_timing > task_timing_max) {
        task_timing_max = task_timing;
    }
    if((task_div & 0x7ff) == 0) {
        task_timing_min = (task_timing_min * 6) / 1000;  // convert to us (168MHz clock)
        task_timing_max = (task_timing_max * 6) / 1000;  // convert to us (168MHz clock)
        log_debug("RT timing - min: %d us - max: %d us",
            task_timing_min , task_timing_max);
        task_timing_min = 0x7fffffff;
        task_timing_max = 0;
    }
#endif
}

//
// STM32 stuff
//
/**
  * @brief  System Clock Configuration
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void) {
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    // Enable Power Control clock
    __HAL_RCC_PWR_CLK_ENABLE();

    // The voltage scaling allows optimizing the power consumption when the
    // device is clocked below the maximum system frequency, to update the
    // voltage scaling value regarding system frequency refer to product datashee.
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // set up clocks
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

    // set systick interval
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/2000);  // 500 us
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    // STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported
    if (HAL_GetREVID() == 0x1001) {
        /* Enable the Flash prefetch */
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line) {
  while(1) {
  }
}
#endif
