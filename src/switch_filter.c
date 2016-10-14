/*
 * Switch Filter
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2012: Kilpatrick Audio
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
#include "switch_filter.h"
#include <stdlib.h>

// settings
int switch_sw_timeout;
int switch_sw_debounce;
int switch_enc_timeout;
struct switch_state {
	uint8_t timeout;  // timeout so we don't trigger this input again for a while
	uint8_t mode;  // input mode: switch or encoder
	uint8_t temp;  // temp value - debouncing, edge detect, etc.
	uint16_t change_f;  // pressed / unpressed flags, encoder state
};
struct switch_state sw_state[SW_NUM_INPUTS];
// switch modes
#define SW_MODE_BUTTON 1
#define SW_MODE_ENC_A 2
#define SW_MODE_ENC_B 3
// event queue
#define SW_EVENT_QUEUE_LEN 64
#define SW_EVENT_QUEUE_MASK (SW_EVENT_QUEUE_LEN - 1)
uint16_t switch_event_queue[SW_EVENT_QUEUE_LEN];
int switch_event_inp;
int switch_event_outp;

// local functions
void switch_filter_handle_enc(int basechan);

// initialize the debounce code
void switch_filter_init(uint16_t sw_timeout, uint16_t sw_debounce, 
		uint16_t enc_timeout) {
	int i;
	// settings
	switch_sw_timeout = sw_timeout;
	switch_sw_debounce = sw_debounce;
	switch_enc_timeout = enc_timeout;
	// reset stuff
	for(i = 0; i < SW_NUM_INPUTS; i ++) {
		sw_state[i].timeout = 0;
		sw_state[i].mode = SW_MODE_BUTTON;  // default
		sw_state[i].temp = 0;
		sw_state[i].change_f = SW_CHANGE_UNPRESSED;
	}
	for(i = 0; i < SW_EVENT_QUEUE_LEN; i ++) {
		switch_event_queue[i] = 0;
	}
	switch_event_inp = 0;
	switch_event_outp = 0;
}

// set a pair of channels an an encoder - must be sequential 
void switch_filter_set_encoder(uint16_t start_chan) {
	if(start_chan >= (SW_NUM_INPUTS - 1)) {
		return;
	}
	sw_state[start_chan].mode = SW_MODE_ENC_A;
	sw_state[start_chan].change_f = 0;  // master state for both channels
	sw_state[start_chan].temp = 0;  // input edge detection / debouncing
	sw_state[start_chan + 1].mode = SW_MODE_ENC_B;
}

// record a new sample value
void switch_filter_set_val(uint16_t start_chan, uint16_t num_chans, int states) {
	int i, temp, oldval;
	if((start_chan + num_chans) > SW_NUM_INPUTS) {
		return;
	}

	temp = states;

	// process each switch in the input
	for(i = start_chan; i < (start_chan + num_chans); i ++) {
		// we're in a timeout phase to limit switch speed
		if(sw_state[i].timeout) {
			sw_state[i].timeout --;
			temp = (temp >> 1);
			continue;
		}

		// buttons
		if(sw_state[i].mode == SW_MODE_BUTTON) {
			// switch is pressed
			if(temp & 0x01) {
				// register press
				if(sw_state[i].change_f == SW_CHANGE_UNPRESSED && 
				        sw_state[i].temp < switch_sw_debounce) {
					sw_state[i].temp ++;
					if(sw_state[i].temp == switch_sw_debounce) {
						sw_state[i].change_f = SW_CHANGE_PRESSED;
						switch_event_queue[switch_event_inp] = SW_CHANGE_PRESSED | (i & 0xfff);
						switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
						sw_state[i].timeout = switch_sw_timeout;
					}
				}
			}
			// switch is unpressed
			else {
				// register release
				if(sw_state[i].change_f == SW_CHANGE_PRESSED && 
				        sw_state[i].temp) {
					sw_state[i].temp --;
					if(sw_state[i].temp == 0) {
						sw_state[i].change_f = SW_CHANGE_UNPRESSED;
						switch_event_queue[switch_event_inp] = SW_CHANGE_UNPRESSED | (i & 0xfff);
						switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
						sw_state[i].timeout = switch_sw_timeout;
					}
				}
			}
		}
		// encoders
		else if(sw_state[i].mode == SW_MODE_ENC_A) {
			oldval = sw_state[i].temp;
			// switch closed
			if(temp & 0x01) {
				sw_state[i].temp |= 0x01;
			}
			// switch opened
			else {
				sw_state[i].temp &= ~0x01;
			}
			// only process if changed
			if(oldval != sw_state[i].temp) {
                switch_filter_handle_enc(i);
            }	
		}
		else if(sw_state[i].mode == SW_MODE_ENC_B) {
			oldval = sw_state[i-1].temp;
			// switch closed
			if(temp & 0x01) {
				sw_state[i-1].temp |= 0x02;
			}
			// switch opened
			else {
				sw_state[i-1].temp &= ~0x02;
			}
			// only process if changed
			if(oldval != sw_state[i-1].temp) {
                switch_filter_handle_enc(i-1);
            }
		}
		temp = (temp >> 1);
	}
}

// get the next event in the queue
int switch_filter_get_event(void) {
	int temp;
	if(switch_event_inp == switch_event_outp) {
		return 0;
	}
	temp = switch_event_queue[switch_event_outp];
	switch_event_outp = (switch_event_outp + 1) & SW_EVENT_QUEUE_MASK;
	return temp;
}

//
// local functions
//
//
// process an encoder
void switch_filter_handle_enc(int basechan) {
    switch(sw_state[basechan].temp) {
        case 0:  // idle
            // emit event        
            switch(sw_state[basechan].change_f) {
                case 0x03:  // disarm CW
					// generate event
					switch_event_queue[switch_event_inp] = 
					    SW_CHANGE_ENC_MOVE_CW | (basechan & 0xfff);
					switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
					sw_state[basechan].timeout = switch_enc_timeout;
					sw_state[basechan+1].timeout = switch_enc_timeout;
                    break;
                case 0x83:  // disarm CCW
					// generate event
					switch_event_queue[switch_event_inp] = 
					    SW_CHANGE_ENC_MOVE_CCW | (basechan & 0xfff);
					switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
					sw_state[basechan].timeout = switch_enc_timeout;
					sw_state[basechan+1].timeout = switch_enc_timeout;
                    break;
            }            
            sw_state[basechan].change_f = 0x00;
            break;
        case 0x01:  // arm CW / disarm CCW
            switch(sw_state[basechan].change_f) {
                case 0x00:  // arm CW
                    sw_state[basechan].change_f = 0x01;
                    break;
                case 0x82:  // disarm CW
                    sw_state[basechan].change_f ++;                    
                    break;
            }
            break;
        case 0x02:  // arm CCW / disarm CW
            switch(sw_state[basechan].change_f) {
                case 0x00:  // arm CCW
                    sw_state[basechan].change_f = 0x81;
                    break;
                case 0x02:  // disarm CCW
                    sw_state[basechan].change_f ++;                    
                    break;
            }
            break;
        case 0x03:  // both on
            sw_state[basechan].change_f ++;
            break;
    }
}


