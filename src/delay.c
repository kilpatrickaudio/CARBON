/*
 * Delay Routines
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
#include "delay.h"
#include "stm32f4xx_hal.h"

volatile int delay_count;

// init the delay
void delay_init(void) {
    delay_count = 0;
}

// run the timer task every 1000us
void delay_timer_task(void) {
    delay_count ++;
}

// delay some number of milliseconds
void delay_ms(int millis) {
    int temp = millis;
    delay_count = 0;
    while(delay_count < temp);
}

