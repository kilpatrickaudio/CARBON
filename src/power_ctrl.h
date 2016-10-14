/*
 * CARBON Power Controller
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
#ifndef POWER_CTRL_H
#define POWER_CTRL_H

// power states
#define POWER_CTRL_STATE_STANDBY 0
#define POWER_CTRL_STATE_IF 1
#define POWER_CTRL_STATE_TURNING_OFF 2
#define POWER_CTRL_STATE_TURNING_ON 3
#define POWER_CTRL_STATE_TURNING_STANDBY 4
#define POWER_CTRL_STATE_ON 5
#define POWER_CTRL_STATE_ERROR 6

// init the power control
void power_ctrl_init(void);

// run the power control timer task
void power_ctrl_timer_task(void);

// get the current power state
int power_ctrl_get_power_state(void);

#endif

