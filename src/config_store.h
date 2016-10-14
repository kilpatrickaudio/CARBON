/*
 * Flash Config Storage - Wear Leveling Version for STM32
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <inttypes.h>

// init the config store
void config_store_init(void);

// run the config storage task
void config_store_timer_task(void);

// gets a config byte
int32_t config_store_get_val(int32_t addr);

// sets a config byte
void config_store_set_val(int32_t addr, int32_t val);

// wipe the config memory so that it will be generated fresh
// returns -1 on error
int config_store_wipe_flash(void);

#endif

