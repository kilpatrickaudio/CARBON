/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probability_seed.h"
#include "stm32f4xx_hal.h"

uint32_t probability_startup_seed(void) {
    uint32_t seed = HAL_GetTick() ^ SysTick->VAL ^ UINT32_C(0x6d2b79f5);
    uint32_t saved_clock = RCC->AHB2ENR & RCC_AHB2ENR_RNGEN;
    uint32_t saved_control;
    unsigned attempt;
    __HAL_RCC_RNG_CLK_ENABLE();
    saved_control = RNG->CR;
    RNG->CR = RNG_CR_RNGEN;
    // No interrupts or clock reconfiguration; at most 100000 startup polls.
    for(attempt = 0; attempt < 100000; attempt ++) {
        uint32_t status = RNG->SR;
        if(status & (RNG_SR_CECS | RNG_SR_SECS)) break;
        if(status & RNG_SR_DRDY) {
            seed ^= RNG->DR;
            break;
        }
    }
    RNG->CR = saved_control;
    if(!saved_clock) __HAL_RCC_RNG_CLK_DISABLE();
    return seed;
}
