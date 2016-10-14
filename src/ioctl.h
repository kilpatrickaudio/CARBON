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
#ifndef IOCTL_H
#define IOCTL_H

// init the IOCTL
void ioctl_init(void);

// run the IOCTL timer task
void ioctl_timer_task(void);

// set the analog power control state
void ioctl_set_analog_pwr_ctrl(int state);

// get the state of the power button
int ioctl_get_power_sw(void);

// get the current DC vsense value in mV
int ioctl_get_dc_vsense(void);

#endif

