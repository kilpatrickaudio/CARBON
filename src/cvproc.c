/*
 * CARBON Sequencer CV/Gate Processor
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
#include "cvproc.h"
#include "config.h"
#include "analog_out.h"
#include "midi/midi_protocol.h"
#include "midi/midi_stream.h"
#include "midi/midi_utils.h"
#include "util/log.h"
#include <inttypes.h>

// state defnes
#define CVPROC_GATE_OFF 0
#define CVPROC_GATE_ON 1

// settings
#define CVPROC_POLY_VOICE_COUNT 4  // max number of polyphonic voices (in AAAA mode)
#define CVPROC_MONO_DEPTH 8  // number of notes to store - must be a power of 2
#define CVPROC_MONO_DEPTH_MASK (CVPROC_MONO_DEPTH - 1)
#define CVPROC_NOTE_MIN (CVPROC_BEND_RANGE_MAX)
#define CVPROC_NOTE_MAX (127 - CVPROC_BEND_RANGE_MAX)

// scale lookup
#define CVPROC_SCALE_NUM_NOTES 128

// state
struct cvproc_state {
    // settings
    int pairs;  // the output pairing arrangement 
    int pair_mode[CVPROC_NUM_PAIRS];  // pair mode (NOTE or CC)
    int cvcal[CVPROC_NUM_OUTPUTS];  // calibration (octave span) for output
    int output_scaling[CVPROC_NUM_OUTPUTS];  // output scaling
    int bend_range;  // pitch bend range
    // common state
    uint8_t damper[CVPROC_NUM_OUTPUTS];  // damper state for each pair
    uint8_t out_offset[CVPROC_NUM_PAIRS];  // the start offset mono / poly group
    // mono state
    int8_t mono_voice_prio[CVPROC_NUM_PAIRS][CVPROC_MONO_DEPTH];  // list of notes in order
    uint8_t mono_voice_pos[CVPROC_NUM_PAIRS];  // list position of last added note
    // poly state
    uint8_t poly_num_voices[CVPROC_NUM_PAIRS];  // number of voices in each group
    int8_t poly_voice_alloc[CVPROC_NUM_PAIRS][CVPROC_POLY_VOICE_COUNT];  // voice alloc
    // output state
    int8_t out_note[CVPROC_NUM_OUTPUTS];  // the current note at the output
    int16_t out_bend[CVPROC_NUM_OUTPUTS];  // the current bend value (DAC offset)
    // output tuning
    uint16_t cvproc_scale[CVPROC_NUM_OUTPUTS][CVPROC_SCALE_NUM_NOTES];
};
struct cvproc_state cvstate;

// MIDI callback function
void cvproc_midi_tx_loopf(struct midi_msg *msg);
// local functions
void cvproc_mono_handler(int pair, struct midi_msg *msg);
void cvproc_poly_handler(int pair, struct midi_msg *msg);
void cvproc_cc_handler(int pair, struct midi_msg *msg);
void cvproc_reset_state(void);
void cvproc_reset_pair(int pair);
void cvproc_set_note(int out, int note, int gate);
void cvproc_set_velo(int out, int velo, int gate);
void cvproc_set_bend(int out, int bend);
void cvproc_build_scale(int out);

// init the CV/gate processor
void cvproc_init(void) {
    int i;

    // reset stuff
    for(i = 0; i < CVPROC_NUM_PAIRS; i ++) {
        cvstate.out_note[i] = CVPROC_DEFAULT_NOTE;
        cvstate.out_bend[i] = 0;
        cvproc_set_pair_mode(i, CVPROC_MODE_NOTE);
    }

    // make scale lookup table
    for(i = 0; i < CVPROC_NUM_OUTPUTS; i ++) {
        cvstate.cvcal[i] = 0;
        cvstate.output_scaling[i] = CVPROC_CV_SCALING_1VOCT;
        cvproc_build_scale(i);
    }
    cvproc_set_pairs(CVPROC_PAIRS_ABCD);  // this causes a state reset
    cvproc_set_bend_range(2);  // default
}

// run the timer task
void cvproc_timer_task(void) {
    struct midi_msg msg;
    int pair;
    // CV/gate
    if(midi_stream_data_available(MIDI_PORT_CV_OUT)) {
        while(midi_stream_data_available(MIDI_PORT_CV_OUT)) {
            midi_stream_receive_msg(MIDI_PORT_CV_OUT, &msg);
            pair = msg.status & 0x0f;  // channel is which pair to use
            switch(cvstate.pairs) {
                case CVPROC_PAIRS_ABCD:
                    switch(pair) {
                        case 0:  // A mono
                        case 1:  // B mono
                        case 2:  // C mono
                        case 3:  // D mono
                            // note / velocity
                            if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE ||
                                    cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                                cvproc_mono_handler(pair, &msg);
                            }
                            // CC
                            else {
                                cvproc_cc_handler(pair, &msg);
                            }
                            break;
                    }
                    break;
                case CVPROC_PAIRS_AABC:
                    switch(pair) {
                        case 0:  // A poly
                            // note / velocity
                            if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE ||
                                    cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                                cvproc_poly_handler(pair, &msg);
                            }
                            // CC
                            else {
                                cvproc_cc_handler(pair, &msg);                            
                            }
                            break;
                        case 1:  // B mono
                        case 2:  // C mono
                            // note / velocity
                            if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE ||
                                    cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                                cvproc_mono_handler(pair, &msg);
                            }
                            // CC
                            else {
                                cvproc_cc_handler(pair, &msg);                            
                            }
                    }
                    break;
                case CVPROC_PAIRS_AABB:
                    switch(pair) {
                        case 0:  // A poly
                        case 1:  // B poly
                            // note / velocity
                            if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE ||
                                    cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                                cvproc_poly_handler(pair, &msg);
                            }
                            // CC
                            else {
                                cvproc_cc_handler(pair, &msg);                            
                            }
                            break;
                    }
                    break;
                case CVPROC_PAIRS_AAAA:
                    if(pair != 0) {
                        break;
                    }
                    // note / velocity
                    if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE ||
                            cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                        cvproc_poly_handler(0, &msg);
                    }
                    // CC
                    else {
                        cvproc_cc_handler(pair, &msg);                    
                    }
                    break;
            }
        }
    }
}

// set the CV/gate processor pairing mode
void cvproc_set_pairs(int pairs) {
    if(pairs < 0 || pairs >= CVPROC_NUM_PAIRS) {
        log_error("csp - pairs invalid: %d", pairs);
        return;
    }
    // set all these even for mono voices because CC uses them
    cvstate.poly_num_voices[0] = 1;
    cvstate.poly_num_voices[1] = 1;
    cvstate.poly_num_voices[2] = 1;
    cvstate.poly_num_voices[3] = 1;
    cvstate.pairs = pairs;
    switch(cvstate.pairs) {
        case CVPROC_PAIRS_ABCD:
            cvstate.out_offset[0] = 0;
            cvstate.out_offset[1] = 1;
            cvstate.out_offset[2] = 2;
            cvstate.out_offset[3] = 3;
            break;
        case CVPROC_PAIRS_AABC:
            cvstate.poly_num_voices[0] = 2;  // AA has 2 poly voices
            cvstate.out_offset[0] = 0;
            cvstate.out_offset[1] = 2;
            cvstate.out_offset[2] = 3;
            cvstate.out_offset[0] = 0;
            cvstate.out_offset[1] = 2;
            break;
        case CVPROC_PAIRS_AABB:
            cvstate.poly_num_voices[0] = 2;  // AA has 2 poly voices
            cvstate.poly_num_voices[1] = 2;  // BB has 2 poly voices
            break;
        case CVPROC_PAIRS_AAAA:
            cvstate.poly_num_voices[0] = 4;  // AAAA has 4 poly voices
            cvstate.out_offset[0] = 0;
            break;
    }
    // turn off all outputs and clear state
    cvproc_reset_state();
}

// set the CV/gate pair mode - pair 0-3 = A-D
void cvproc_set_pair_mode(int pair, int mode) {
    if(pair < 0 || pair >= CVPROC_NUM_PAIRS) {
        log_error("cspm - pair invalid: %d", pair);
        return;
    }
    if(mode < CVPROC_MODE_VELO || mode > CVPROC_MODE_MAX) {
        log_error("cspm - mode invalid: %d", mode);
        return;
    }
    cvstate.pair_mode[pair] = mode;
    cvproc_reset_pair(pair);
}

// set the CV bend range for pitch bend
void cvproc_set_bend_range(int range) {
    if(range < CVPROC_BEND_RANGE_MIN || range > CVPROC_BEND_RANGE_MAX) {
        log_error("csbr - range invalid: %d", range);
        return;
    }
    cvstate.bend_range = range;
}

// set the CV output scaling for an output (0-3 = A-D)
void cvproc_set_output_scaling(int out, int mode) {
    if(out < 0 || out >= CVPROC_NUM_OUTPUTS) {
        log_error("csos - out invalid: %d", out);
        return;
    }
    if(mode < 0 || mode > CVPROC_CV_SCALING_MAX) {
        log_error("csos - mode invalid: %d", mode);
        return;
    }
    cvstate.output_scaling[out] = mode;
    cvproc_build_scale(out);
}

// set the scale of an output
void cvproc_set_cvcal(int out, int scale) {
    if(out < 0 || out >= CVPROC_NUM_OUTPUTS) {
        log_error("csc - out invalid: %d", out);
        return;
    }
    if(scale < CVPROC_CVCAL_MIN || scale > CVPROC_CVCAL_MAX) {
        log_error("csc - scale invalid: %d", scale);
        return;
    }
    cvstate.cvcal[out] = scale;
    cvproc_build_scale(out);    
}
    

//
// local functions
//
// handle mono event
void cvproc_mono_handler(int pair, struct midi_msg *msg) {
    int i, held;
    if(pair < 0 || pair >= CVPROC_NUM_PAIRS) {
        log_error("cmh - pair invalid: %d", pair);
        return;
    }    
    switch(msg->status & 0xf0) {
        case MIDI_NOTE_OFF:
            // check note range
            if(msg->data0 < CVPROC_NOTE_MIN || msg->data0 > CVPROC_NOTE_MAX) {
                return;
            }
            // clear the note from the list if it's already there
            for(i = 0; i < CVPROC_MONO_DEPTH; i ++) {
                if(cvstate.mono_voice_prio[pair][i] == msg->data0) {
                    cvstate.mono_voice_prio[pair][i] = -1;
                }
            }
            // our current note is still active
            if(cvstate.mono_voice_prio[pair][cvstate.mono_voice_pos[pair]] != -1) {
                return;
            }
            // find a new note in the history
            i = (cvstate.mono_voice_pos[pair] - 1) & CVPROC_MONO_DEPTH_MASK;
            while(i != cvstate.mono_voice_pos[pair]) {
                // we found another note
                if(cvstate.mono_voice_prio[pair][i] != -1) {
                    // note
                    if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE) {
                        cvproc_set_note(cvstate.out_offset[pair], 
                            cvstate.mono_voice_prio[pair][i], CVPROC_GATE_ON); 
                    }
                    // do not change velo when releasing held notes
                    cvstate.mono_voice_pos[pair] = i;
                    return;
                }
                i = (i - 1) & CVPROC_MONO_DEPTH_MASK;
            }
            // damper not held - kill notes
            if(cvstate.damper[pair] == 0) {
                // note
                if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE) {            
                    cvproc_set_note(cvstate.out_offset[pair], 
                        msg->data0, CVPROC_GATE_OFF);
                }
                // velo
                else if(cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                    cvproc_set_velo(cvstate.out_offset[pair], msg->data1, 
                        CVPROC_GATE_OFF);
                }
            }
            break;
        case MIDI_NOTE_ON:
            // check note range
            if(msg->data0 < CVPROC_NOTE_MIN || msg->data0 > CVPROC_NOTE_MAX) {
                return;
            }
            // clear the note from the list if it's already there
            held = 0;
            for(i = 0; i < CVPROC_MONO_DEPTH; i ++) {
                // clear existing instance of the same note
                if(cvstate.mono_voice_prio[pair][i] == msg->data0) {
                    cvstate.mono_voice_prio[pair][i] = -1;
                }
                // check if there is a note here
                if(cvstate.mono_voice_prio[pair][i] != -1) {
                    held = 1;
                }
            }
            // XXX handle retrig
            cvstate.mono_voice_pos[pair] = (cvstate.mono_voice_pos[pair] + 1) &
                CVPROC_MONO_DEPTH_MASK;
            cvstate.mono_voice_prio[pair][cvstate.mono_voice_pos[pair]] = msg->data0;
            // note
            if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE) {        
                cvproc_set_note(cvstate.out_offset[pair], msg->data0, CVPROC_GATE_ON);
            }
            // velo - only update if it's the first note
            else if(cvstate.pair_mode[pair] == CVPROC_MODE_VELO && !held) {
                cvproc_set_velo(cvstate.out_offset[pair], msg->data1, 
                    CVPROC_GATE_ON);            
            }
            break;
        case MIDI_CONTROL_CHANGE:
            // damper pedal
            if(msg->data0 == MIDI_CONTROLLER_DAMPER) {
                // down
                if(msg->data1 == 0x7f) {
                    cvstate.damper[pair] = 1;
                }
                // up
                else if(msg->data1 == 0) {
                    cvstate.damper[pair] = 0;
                    // search for note held down
                    for(i = 0; i < CVPROC_MONO_DEPTH; i ++) {
                        if(cvstate.mono_voice_prio[pair][i] != -1) {
                            return;
                        }
                    }
                    // kill output
                    // note
                    if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE) {        
                        cvproc_set_note(cvstate.out_offset[pair], 
                            cvstate.out_note[pair], CVPROC_GATE_OFF);
                    }
                    // velo
                    else if(cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                        cvproc_set_velo(cvstate.out_offset[pair], msg->data1, 
                            CVPROC_GATE_OFF);
                    }
                }
            }
            break;
        case MIDI_PITCH_BEND:
            cvproc_set_bend(cvstate.out_offset[pair], 
                ((msg->data1 << 7) | msg->data0) - 8192);
            break;
    }
}

// handle poly event
void cvproc_poly_handler(int pair, struct midi_msg *msg) {
    int i, slot, temp;
    if(pair < 0 || pair >= CVPROC_NUM_PAIRS) {
        log_error("cph - pair invalid: %d", pair);
        return;
    }
    if(cvstate.poly_num_voices[pair] == 0) {
        log_warn("cph - 0 voices for chan: %d", pair);
        return;
    }

    switch(msg->status & 0xf0) {
        case MIDI_NOTE_OFF:
            // remove the note from the voice alloc
            for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                if(cvstate.poly_voice_alloc[pair][i] == msg->data0) {
                    cvstate.poly_voice_alloc[pair][i] = -1;  // free slot
                    // turn off note
                    if(cvstate.damper[pair] == 0) {
                        // note
                        if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE) {        
                            cvproc_set_note(cvstate.out_offset[pair] + i, 
                                msg->data0, CVPROC_GATE_OFF);
                        }
                        // velo
                        else if(cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                            cvproc_set_velo(cvstate.out_offset[pair] + i, 
                                msg->data1, CVPROC_GATE_OFF);
                        }
                    }
                }
            }
            break;
        case MIDI_NOTE_ON:
            slot = -1;
            // find a free voice slot
            for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                if(cvstate.poly_voice_alloc[pair][i] == -1) {
                    slot = i;
                    break;
                }
            }
            // no mode voice slots
            if(slot == -1) {
                return;
            }
            // allocate and turn on the note
            cvstate.poly_voice_alloc[pair][slot] = msg->data0;
            // note
            if(cvstate.pair_mode[pair] == CVPROC_MODE_NOTE) {        
                cvproc_set_note(cvstate.out_offset[pair] + slot, 
                    msg->data0, CVPROC_GATE_ON);
            }
            // velo
            if(cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                cvproc_set_velo(cvstate.out_offset[pair] + slot, 
                    msg->data1, CVPROC_GATE_ON);            
            }
            break;
        case MIDI_CONTROL_CHANGE:
            // damper pedal
            if(msg->data0 == MIDI_CONTROLLER_DAMPER) {
                // down
                if(msg->data1 == 0x7f) {
                    cvstate.damper[pair] = 1;
                }
                // up
                else if(msg->data1 == 0) {
                    cvstate.damper[pair] = 0;
                    // should we release all notes
                    for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                        if(cvstate.poly_voice_alloc[pair][i] != -1) {
                            return;
                        }
                    }
                    for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                        // kill output
                        // note
                        if(cvstate.pair_mode[pair] + i == CVPROC_MODE_NOTE) {        
                            cvproc_set_note(cvstate.out_offset[pair] + i, 
                                cvstate.out_note[pair + i], CVPROC_GATE_OFF);
                        }
                        // velo
                        else if(cvstate.pair_mode[pair] == CVPROC_MODE_VELO) {
                            cvproc_set_velo(cvstate.out_offset[pair] + i,
                                msg->data1, CVPROC_GATE_OFF);
                        }
                    }
                }
            }
            break;
        case MIDI_PITCH_BEND:
            temp = ((msg->data1 << 7) | msg->data0) - 8192;
            for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                cvproc_set_bend(cvstate.out_offset[pair] + i, temp);
            }
            break;
    }
}

// handle CC
void cvproc_cc_handler(int pair, struct midi_msg *msg) {
    int cc, out;
    if(pair < 0 || pair >= CVPROC_NUM_PAIRS) {
        log_error("cch - pair invalid: %d", pair);
        return;
    }
    // use poly num voices to figure out how many channels are on this pair
    if(cvstate.poly_num_voices[pair] == 0) {
        log_warn("cch - 0 voices for chan: %d", pair);
        return;
    }
    // ignore all but CC
    if((msg->status & 0xf0) != MIDI_CONTROL_CHANGE) {
        return;
    }
    // get out if the controller is out of range for us
    cc = msg->data0;
    if(cc < cvstate.pair_mode[pair] || cc >= (cvstate.pair_mode[pair] +
            cvstate.poly_num_voices[pair])) {
        return;
    }
    out = (cc - cvstate.pair_mode[pair]) + cvstate.out_offset[pair];
    analog_out_set_cv(out, (msg->data1 << 5));
    // control gate based on CC level threshold
    if(msg->data1 & 0x40) {
        analog_out_set_gate(out, 1);
    }
    else {
        analog_out_set_gate(out, 0);    
    }
}

// turn off all notes and reset state
void cvproc_reset_state(void) {
    int i, j;
    // turn off / reset outputs
    for(i = 0; i < CVPROC_NUM_OUTPUTS; i ++) {
        cvproc_set_note(i, CVPROC_DEFAULT_NOTE, CVPROC_GATE_OFF);
        cvproc_set_bend(i, 0);
    }
    
    // clear mono state
    for(i = 0; i < CVPROC_NUM_PAIRS; i ++) {
        for(j = 0; j < CVPROC_MONO_DEPTH; j ++) {
            cvstate.mono_voice_prio[i][j] = -1;
        }
        cvstate.mono_voice_pos[i] = 0;
    }

    // clear poly state
    for(i = 0; i < CVPROC_NUM_PAIRS; i ++) {
        for(j = 0; j < CVPROC_POLY_VOICE_COUNT; j ++) {
            cvstate.poly_voice_alloc[i][j] = -1;
        }
    }
    
    // clear other state
    for(i = 0; i < CVPROC_NUM_PAIRS; i ++) {
        cvstate.damper[i] = 0;
    }    
}

// reset a specific pair
void cvproc_reset_pair(int pair) {
    int i;
    switch(cvstate.pairs) {
        case CVPROC_PAIRS_ABCD:
            cvstate.damper[cvstate.out_offset[pair]] = 0;
            for(i = 0; i < CVPROC_MONO_DEPTH; i ++) {
                cvstate.mono_voice_prio[pair][i] = -1;
            }
            cvstate.mono_voice_pos[pair] = 0;
            cvproc_set_note(cvstate.out_offset[pair], 
                CVPROC_DEFAULT_NOTE, CVPROC_GATE_OFF);
            cvproc_set_bend(pair, 0);
            break;
        case CVPROC_PAIRS_AABC:
            switch(pair) {
                case 0:  // A poly
                    cvstate.damper[cvstate.out_offset[pair]] = 0;
                    for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                        cvstate.poly_voice_alloc[pair][i] = -1;
                        cvproc_set_note(cvstate.out_offset[pair] + i, 
                            CVPROC_DEFAULT_NOTE, CVPROC_GATE_OFF);
                        cvproc_set_bend(cvstate.out_offset[pair] + i, 0);
                    }
                    break;
                case 1:  // B mono
                case 2:  // C mono
                    cvstate.damper[cvstate.out_offset[pair]] = 0;
                    for(i = 0; i < CVPROC_MONO_DEPTH; i ++) {
                        cvstate.mono_voice_prio[pair][i] = -1;
                    }
                    cvstate.mono_voice_pos[pair] = 0;
                    cvproc_set_note(cvstate.out_offset[pair], 
                        CVPROC_DEFAULT_NOTE, CVPROC_GATE_OFF);
                    cvproc_set_bend(pair, 0);
                    break;
            }
            break;
        case CVPROC_PAIRS_AABB:
            switch(pair) {
                case 0:  // A poly
                case 1:  // B poly
                    cvstate.damper[cvstate.out_offset[pair]] = 0;
                    for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                        cvstate.poly_voice_alloc[pair][i] = -1;
                        cvproc_set_note(cvstate.out_offset[pair] + i, 
                            CVPROC_DEFAULT_NOTE, CVPROC_GATE_OFF);
                        cvproc_set_bend(cvstate.out_offset[pair] + i, 0);
                    }
                    break;
            }
            break;
        case CVPROC_PAIRS_AAAA:
            if(pair != 0) {
                break;
            }
            cvstate.damper[cvstate.out_offset[pair]] = 0;
            for(i = 0; i < cvstate.poly_num_voices[pair]; i ++) {
                cvstate.poly_voice_alloc[pair][i] = -1;
                cvproc_set_note(cvstate.out_offset[pair] + i, 
                    CVPROC_DEFAULT_NOTE, CVPROC_GATE_OFF);
                cvproc_set_bend(cvstate.out_offset[pair] + i, 0);
            }
            break;
    }
}

// set a note on an output
void cvproc_set_note(int out, int note, int gate) {
    if(out < 0 || out >= CVPROC_NUM_OUTPUTS) {
        log_error("csn - out invalid: %d", out);
        return;
    }
    if(note < 0 || note >= CVPROC_SCALE_NUM_NOTES) {
        log_error("csn - note invalid: %d", note);
        return;
    }
    analog_out_set_cv(out, cvstate.cvproc_scale[out][note] + cvstate.out_bend[out]);
    analog_out_set_gate(out, gate);
    cvstate.out_note[out] = note;
}

// set a velo on an output
void cvproc_set_velo(int out, int velo, int gate) {
    if(out < 0 || out >= CVPROC_NUM_OUTPUTS) {
        log_error("csv - out invalid: %d", out);
        return;
    }
    if(velo < 0 || velo >= 0x7f) {
        log_error("csv - velo invalid: %d", velo);
        return;
    }
    // only update velo for note on
    if(gate == CVPROC_GATE_ON) {
        analog_out_set_cv(out, velo << 5);  // convert to 12 bit
    }
    analog_out_set_gate(out, gate);
}

// set a bend on an output
void cvproc_set_bend(int out, int bend) {
    int note;
    if(out < 0 || out >= CVPROC_NUM_OUTPUTS) {
        log_error("csb - chan invalid: %d", out);
        return;
    }
    if(bend < -8192 || bend > 8191) {
        log_error("csb - bend invalid: %d", bend);
        return;
    }
    
    // bend up
    if(bend >= 0) {
        note = cvstate.out_note[out];
        cvstate.out_bend[out] = ((cvstate.cvproc_scale[out][note + cvstate.bend_range] - 
                cvstate.cvproc_scale[out][note]) * bend) >> 13;
    }
    // bend down
    else {
        note = cvstate.out_note[out];
        cvstate.out_bend[out] = ((cvstate.cvproc_scale[out][note] - 
                cvstate.cvproc_scale[out][note - cvstate.bend_range]) * bend) >> 13;    
    }
    analog_out_set_cv(out, cvstate.cvproc_scale[out][note] + cvstate.out_bend[out]);
}

// build the scale for an output based on scaling and cvcal
void cvproc_build_scale(int out) {
    int i, temp;
    int step_size;
    int val;  // temp value (value is 16x greater for better resolution)

    // choose the correct note spacing
    switch(cvstate.output_scaling[out]) {
        case CVPROC_CV_SCALING_1P2VOCT:
            step_size = cvstate.cvcal[out] + CVPROC_CVCAL_SEMI_SIZE_1P2VOCT;
            break;
        case CVPROC_CV_SCALING_1VOCT:
        default:
            step_size = cvstate.cvcal[out] + CVPROC_CVCAL_SEMI_SIZE_1VOCT;
            break;
    }
    
    // go from middle C up
    val = 0x800 << 4;
    for(i = 60; i < CVPROC_SCALE_NUM_NOTES; i ++) {
        temp = val >> 4;
        // hit endpoint
        if(temp > 0xfff) {
            cvstate.cvproc_scale[out][i] = 0xfff;  // clamp
        }
        else {
            cvstate.cvproc_scale[out][i] = temp;
        }
        val += step_size;
    }
    
    // go from B below middle C down
    val = 0x800 << 4;
    val -= step_size;
    for(i = 59; i >= 0; i --) {
        temp = val >> 4;
        if(temp < 0) {
            cvstate.cvproc_scale[out][i] = 0;  // clamp
        }
        else {
            cvstate.cvproc_scale[out][i] = temp;
        }
        val -= step_size;
    }
}

