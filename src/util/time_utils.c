/*
 * Time Utilities
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2014: Kilpatrick Audio
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
#include "time_utils.h"
#include <stdio.h>
#include <time.h>
#include "log.h"

btime time_utils_current_time = 0;

// get the current time
btime time_utils_get_btime(void) {
    return time_utils_current_time;
}

// set the current time (used for embedded version without a clock system)
void time_utils_set_btime(btime newtime) {
    time_utils_current_time = newtime;
}

