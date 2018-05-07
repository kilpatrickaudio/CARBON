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
#include "power_ctrl.h"
#include "ioctl.h"
#include "config.h"
#include "gui/gui.h"
#include "seq/seq_ctrl.h"
#include "usbh_midi/usbh_midi.h"
#include "util/log.h"
#include "util/state_change.h"
#include "util/state_change_events.h"
#include "stm32f4xx_hal.h"
#include <inttypes.h>

// settings
#define POWER_CTRL_SW_DEBOUNCE_COUNT 5

struct power_ctrl {
    int timer_div;
    uint8_t desired_power_state;
    uint8_t power_state;
    int8_t power_sw_count;
};
struct power_ctrl pwrs;

// local functions
void power_ctrl_change_state(int state);

// init the power control
void power_ctrl_init(void) {
    pwrs.timer_div = 0;
    pwrs.desired_power_state = POWER_CTRL_STATE_STANDBY;
    pwrs.power_state = -1;  // cause a state change
    power_ctrl_change_state(POWER_CTRL_STATE_STANDBY);

#ifdef POWER_CTRL_POWER_ON_AUTO
    power_ctrl_change_state(POWER_CTRL_STATE_TURNING_ON);
#endif
}

// run the power control timer task
void power_ctrl_timer_task(void) {
    static int next_state = -1;  // handle release of button before changing

    // power switch is pressed
    if(ioctl_get_power_sw()) {
        // do switch debouncing
        if(pwrs.power_sw_count < POWER_CTRL_SW_DEBOUNCE_COUNT) {
            pwrs.power_sw_count ++;
            // toggle state
            if(pwrs.power_sw_count == POWER_CTRL_SW_DEBOUNCE_COUNT) {
                // turn on from idle
                if(pwrs.power_state == POWER_CTRL_STATE_STANDBY) {
                    next_state = POWER_CTRL_STATE_TURNING_ON;
                }
                // turn off to interface
                else if(pwrs.power_state == POWER_CTRL_STATE_ON) {
                    next_state = POWER_CTRL_STATE_TURNING_OFF;
                }
                // turning off completely
                else if(pwrs.power_state == POWER_CTRL_STATE_IF) {
                    next_state = POWER_CTRL_STATE_TURNING_STANDBY;
                }
            }
        }
    }
    // power switch is not pressed
    else if(pwrs.power_sw_count) {
        pwrs.power_sw_count --;
        if(pwrs.power_sw_count == 0 && next_state != -1) {
            power_ctrl_change_state(next_state);
            next_state = -1;
        }
    }

    // if we're not already errored out - detect input voltage and average it
    if((pwrs.timer_div & 0x3f) == 0 && pwrs.power_state != POWER_CTRL_STATE_ERROR) {
        if(ioctl_get_dc_vsense() < POWER_CTRL_DC_MIN_CUTOFF) {
//            log_debug("pctt - power error: %d", ioctl_get_dc_vsense());
            power_ctrl_change_state(POWER_CTRL_STATE_ERROR);
        }
    }

    // do state changes more slowly
    if((pwrs.timer_div & 0x3f) == 0) {
        // time to change state
        if(pwrs.desired_power_state != pwrs.power_state) {
            pwrs.power_state = pwrs.desired_power_state;
            // change state
            switch(pwrs.desired_power_state) {
                case POWER_CTRL_STATE_STANDBY:
//                    log_debug("pctt - standby");
                    // set order critical stuff
                    ioctl_set_analog_pwr_ctrl(0);
                    gui_set_lcd_power(0);
                    usbh_midi_set_vbus(0);  // turn off USB host port
                    // fire event for other modules
                    state_change_fire1(SCE_POWER_STATE, POWER_CTRL_STATE_STANDBY);
                    break;
                case POWER_CTRL_STATE_IF:
//                    log_debug("pctt - interface");
                    // set order critical stuff
                    ioctl_set_analog_pwr_ctrl(1);
                    gui_set_lcd_power(0);
                    usbh_midi_set_vbus(1);  // turn on USB host port
                    // fire event for other modules
                    state_change_fire1(SCE_POWER_STATE, POWER_CTRL_STATE_IF);
                    break;
                case POWER_CTRL_STATE_TURNING_OFF:
//                    log_debug("pctt - turning off (to IF)");
                    // fire event for other modules
                    state_change_fire1(SCE_POWER_STATE, POWER_CTRL_STATE_TURNING_OFF);
                    power_ctrl_change_state(POWER_CTRL_STATE_IF);
                    break;
                case POWER_CTRL_STATE_TURNING_ON:
//                    log_debug("pctt - turning on");
                    // set order critical stuff
                    ioctl_set_analog_pwr_ctrl(1);
                    power_ctrl_change_state(POWER_CTRL_STATE_ON);
                    // fire event for other modules
                    state_change_fire1(SCE_POWER_STATE, POWER_CTRL_STATE_TURNING_ON);
                    break;
                case POWER_CTRL_STATE_TURNING_STANDBY:
                    // reboot back to power applied state
                    HAL_NVIC_SystemReset();
                    break;
                case POWER_CTRL_STATE_ON:
//                    log_debug("pctt - on");
                    // set order critical stuff
                    gui_set_lcd_power(1);  // causes power and init to be performed on main thread
                    usbh_midi_set_vbus(1);  // turn on USB host port
                    // fire event for other modules
                    state_change_fire1(SCE_POWER_STATE, POWER_CTRL_STATE_ON);
                    break;
                case POWER_CTRL_STATE_ERROR:
                    gui_set_lcd_power(0);
                    ioctl_set_analog_pwr_ctrl(0);
                    // fire event for other modules
                    state_change_fire1(SCE_POWER_STATE, POWER_CTRL_STATE_ERROR);
                    break;
            }
        }
    }
    pwrs.timer_div ++;
}

// get the current power state
int power_ctrl_get_power_state(void) {
    return pwrs.power_state;
}

//
// local functions
//
// change the power control state
void power_ctrl_change_state(int state) {
    pwrs.desired_power_state = state;
}
