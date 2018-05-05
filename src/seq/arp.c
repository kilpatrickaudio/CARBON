/*
 * CARBON Arpeggiator
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2015: Kilpatrick Audio
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
#include "arp.h"
#include "arp_progs.h"
#include "seq_engine.h"
#include "../config.h"
#include "../midi/midi_protocol.h"
#include "../midi/midi_utils.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

// access to programs
extern struct arp_prog aprog[SEQ_NUM_TRACKS];

// settings
#define ARP_MAX_HELD_NOTES 8
#define ARP_MAX_PLAYING_NOTES 8
#define ARP_NOTE_SLOT_FREE -1

// arp data
struct arp_state {
    int seq_enable;  // 0 = disable, 1 = enable - affects clock syncing
    int arp_enable;  // 0 = disable, 1 = enable
    int type;  // arp type
    int gate_time;  // arp gate time
    int step_size;  // arp step size in ticks
    int8_t held_notes[ARP_MAX_HELD_NOTES];  // held notes
    int8_t held_velo;  // held note velocity
    uint8_t held_note_count;  // number of held notes
    int8_t snapshot_notes[ARP_MAX_HELD_NOTES];  // snapshot of held notes
    int8_t playing_notes[ARP_MAX_PLAYING_NOTES];  // playing note
    uint8_t playing_note_count;  // number of playing notes
    int play_note_timeout;  // tick timeout for playing note
    // arp execution state
    int seq_clock_count;  // divider counter for sequencer running
    int freerun_clock_count;  // divider counter for freerunning
    int pc;  // current program counter location
    int x;  // the register used for temp storage during program run
    int regs[ARP_PROG_NUM_REGS];  // temp registers for programs to use
    int note_offset;  // the offset for the notes generated
};
struct arp_state astate[SEQ_NUM_TRACKS];

// local functions
void arp_reset_program(int track);
void arp_execute_step(int track);
int arp_find_lowest_note(int track);
int arp_find_highest_note(int track);
int arp_find_lower_note(int track, int note);
int arp_find_higher_note(int track, int note);
int arp_find_random_note(int track, int note);
void arp_start_note(int track, int note);
void arp_stop_all_notes(int track);
void arp_timeout_notes(int track);
int arp_find_label(int track, int label);
void arp_take_snapshot(int track);

// init the arp
void arp_init(void) {
    int track, i;
    arp_progs_init();

    // clear arp for all tracks
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        // reset playing notes
        for(i = 0; i < ARP_MAX_PLAYING_NOTES; i ++) {
            astate[track].playing_notes[i] = ARP_NOTE_SLOT_FREE;
        }
        astate[track].playing_note_count = 0;
        astate[track].play_note_timeout = 0;
        astate[track].gate_time = 1;

        // reset stuff
        arp_set_seq_enable(0);
        arp_set_arp_enable(track, 0);  // off - causes stuff to be reset
        arp_set_type(track, ARP_TYPE_UP1);
        arp_set_speed(track, SEQ_UTILS_STEP_16TH);
        arp_set_gate_time(track,
            seq_utils_step_len_to_ticks(SEQ_UTILS_STEP_32ND));
    }
}

// run the arp - called on each clock tick
void arp_run(int tick_count) {
    int track;

    // process arp for each track
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        // sequencer position was reset
        if(tick_count == 0) {
            astate[track].seq_clock_count = 0;
        }

        // run when arp is enabled
        if(astate[track].arp_enable) {
            // run the step
            if((astate[track].seq_enable && astate[track].seq_clock_count == 0) ||
                    (!astate[track].seq_enable && astate[track].freerun_clock_count == 0)) {
                // no notes are held down
                if(astate[track].held_note_count == 0) {
                    if(astate[track].playing_note_count > 0) {
                        arp_stop_all_notes(track);
                    }
                    arp_reset_program(track);
                }
                // run the step
                else {
                    arp_execute_step(track);
                }
            }
            // timeout notes
            else {
                arp_timeout_notes(track);
            }
        }

        // sequencer is running
        if(astate[track].seq_enable) {
            astate[track].seq_clock_count ++;
            if(astate[track].seq_clock_count >= astate[track].step_size) {
                astate[track].seq_clock_count = 0;
            }
        }
        // arp is freerunning
        else {
            astate[track].freerun_clock_count ++;
            if(astate[track].freerun_clock_count >= astate[track].step_size) {
                astate[track].freerun_clock_count = 0;
            }
        }
    }
}

// set whether the external sequencer is playing or not
// this affects whether we resync the arp start to our own clock
void arp_set_seq_enable(int enable) {
    int track;
    // enable and disable seq on all tracks
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        if(enable) {
            astate[track].seq_enable = 1;
        }
        else {
            astate[track].seq_enable = 0;
        }
    }
}

// set the arp enable - 0 = off, 1 = on
void arp_set_arp_enable(int track, int enable) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("asae - track invalid: %d", track);
        return;
    }

    // start
    if(enable) {
        arp_clear_input(track);
        arp_reset_program(track);
        astate[track].arp_enable = 1;
    }
    // stop
    else {
        arp_clear_input(track);
        arp_stop_all_notes(track);
        astate[track].arp_enable = 0;
    }
}

// handle a input from the keyboard - we only get notes if we're enabled
void arp_handle_input(int track, struct midi_msg *msg) {
    int i;
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ahi - track invalid: %d", track);
        return;
    }
    // handle each kind of message we care about
    switch(msg->status & 0xf0) {
        case MIDI_NOTE_OFF:
            // remove the note from the list
            for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
                if(astate[track].held_notes[i] == msg->data0) {
                    astate[track].held_notes[i] = ARP_NOTE_SLOT_FREE;
                    astate[track].held_note_count --;
                    break;
                }
            }
            break;
        case MIDI_NOTE_ON:
            // add the note to the list
            for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
                if(astate[track].held_notes[i] == ARP_NOTE_SLOT_FREE) {
                    astate[track].held_notes[i] = msg->data0;
                    // we use the first note pressed for the velocity
                    if(astate[track].held_note_count == 0) {
                        astate[track].held_velo = msg->data1;
                    }
                    astate[track].held_note_count ++;
                    break;
                }
            }
            // if this is our first note then we need to reset the
            // freerunning clock counter so we start playing right away
            if(astate[track].held_note_count == 1) {
                astate[track].freerun_clock_count = 0;
            }
            break;
        default:
            break;
    }
}

// clear all input notes and stop notes - live input went away
void arp_clear_input(int track) {
    int i;
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("aci - track invalid: %d", track);
        return;
    }

    // clear the held note list
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        astate[track].held_notes[i] = ARP_NOTE_SLOT_FREE;
    }
    astate[track].held_note_count = 0;
    // turn off all playing notes
    arp_stop_all_notes(track);
}

// set the arp type
void arp_set_type(int track, int type) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ast - track invalid: %d", track);
        return;
    }
    if(type < 0 || type >= ARP_NUM_TYPES) {
        log_error("ast - type invalid: %d", type);
        return;
    }
    astate[track].type = type;
    arp_reset_program(track);
    arp_progs_load(track, astate[track].type);  // load program
}

// set the arp speed
void arp_set_speed(int track, int speed) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ass - track invalid: %d", track);
        return;
    }
    if(speed < 0 || speed >= SEQ_UTILS_STEP_LENS) {
        log_error("ass - speed invalid: %d", speed);
        return;
    }
    astate[track].step_size = seq_utils_step_len_to_ticks(speed);
}

// set the gate time - ticks
void arp_set_gate_time(int track, int time) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("asagt - track invalid: %d", track);
        return;
    }
    if(time < ARP_GATE_TIME_MIN || time > ARP_GATE_TIME_MAX) {
        log_error("asagt - time invalid: %d", time);
        return;
    }
    astate[track].gate_time = time;
}

//
// local functions
//
// reset the program
void arp_reset_program(int track) {
    int i;
    astate[track].pc = 0;
    astate[track].x = 0;
    astate[track].note_offset = 0;
    // clear all registers
    for(i = 0; i < ARP_PROG_NUM_REGS; i ++) {
        astate[track].regs[i] = 0;
    }
    // clear the note snapshot
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        astate[track].snapshot_notes[i] = ARP_NOTE_SLOT_FREE;
    }
}

// execute the step
void arp_execute_step(int track) {
    int loop_count = 0;
    int arg, temp, note;

    while(loop_count < ARP_MAX_LOOP_COUNT) {
        arg = aprog[track].prog[astate[track].pc][ARP_PROG_ARG];

        // process the instruction
        switch(aprog[track].prog[astate[track].pc][ARP_PROG_INST]) {
            case AP_NOP:
                // do nothing
                break;
            case AP_SNAPSHOT:
                arp_take_snapshot(track);
                break;
            case AP_FIND_LOWEST_NOTE:
                note = arp_find_lowest_note(track);
                // no note found - jump to label from argument
                if(note == -1) {
                    temp = arp_find_label(track, arg);
                    // label not found - reset program
                    if(temp == -1) {
                        arp_reset_program(track);
                        return;
                    }
                    astate[track].pc = temp;
                }
                // note was found
                else {
                    astate[track].x = note;
                }
                break;
            case AP_FIND_HIGHEST_NOTE:
                note = arp_find_highest_note(track);
                // no note found - jump to label from argument
                if(note == -1) {
                    temp = arp_find_label(track, arg);
                    // label not found - reset program
                    if(temp == -1) {
                        arp_reset_program(track);
                        return;
                    }
                    astate[track].pc = temp;
                    break;
                }
                // note was found
                else {
                    astate[track].x = note;
                }
                break;
            case AP_FIND_LOWER_NOTE:
                note = arp_find_lower_note(track, astate[track].x);
                // no note found - jump to label from argument
                if(note == -1) {
                    temp = arp_find_label(track, arg);
                    // label not found - reset program
                    if(temp == -1) {
                        arp_reset_program(track);
                        return;
                    }
                    astate[track].pc = temp;
                    break;
                }
                // note was found
                else {
                    astate[track].x = note;
                }
                break;
            case AP_FIND_HIGHER_NOTE:
                note = arp_find_higher_note(track, astate[track].x);
                // no note found - jump to label from argument
                if(note == -1) {
                    temp = arp_find_label(track, arg);
                    // label not found - reset program
                    if(temp == -1) {
                        arp_reset_program(track);
                        return;
                    }
                    astate[track].pc = temp;
                    break;
                }
                // note was found
                else {
                    astate[track].x = note;
                }
                break;
            case AP_FIND_RANDOM_NOTE:
                note = arp_find_random_note(track, astate[track].x);
                // no note found - jump to label from argument
                if(note == -1) {
                    temp = arp_find_label(track, arg);
                    // label not found - reset program
                    if(temp == -1) {
                        arp_reset_program(track);
                        return;
                    }
                    astate[track].pc = temp;
                    break;
                }
                // note was found
                else {
                    astate[track].x = note;
                }
                break;
            case AP_PLAY_NOTE:
                arp_start_note(track, astate[track].x + astate[track].note_offset);
                break;
            case AP_WAIT:
                astate[track].pc ++;
                return;  // we need to return here to pause execution
            case AP_PLAY_NOTE_AND_WAIT:
                arp_stop_all_notes(track);
                arp_start_note(track, astate[track].x + astate[track].note_offset);
                astate[track].pc ++;
                return;  // we need to return here to pause execution
            case AP_LABEL:
                // no action - these instructions are scanned by the jump routine
                break;
            case AP_JUMP:
                temp = arp_find_label(track, arg);
                // label not found - reset program
                if(temp == -1) {
                    arp_reset_program(track);
                    return;
                }
                astate[track].pc = temp;
                break;
            case AP_LOADL:
                astate[track].x = arg;
                break;
            case AP_LOADF:
                switch(arg) {
                    case ARP_REG_NOTE_OFFSET:
                        astate[track].x = astate[track].note_offset;
                        break;
                    default:  // load a temp register
                        if(arg >= 0 && arg < ARP_PROG_NUM_REGS) {
                            astate[track].x = astate[track].regs[arg];
                        }
                        break;
                }
                break;
            case AP_STOREF:
                switch(arg) {
                    case ARP_REG_NOTE_OFFSET:
                        astate[track].note_offset = astate[track].x;
                        break;
                    default:  // load a temp register
                        if(arg >= 0 && arg < ARP_PROG_NUM_REGS) {
                            astate[track].regs[arg] = astate[track].x;
                        }
                        break;
                }
                break;
            case AP_ADDL:
                astate[track].x += arg;
                break;
            case AP_SUBL:
                astate[track].x -= arg;
                break;
            case AP_MULL:
                astate[track].x *= arg;
                break;
            case AP_ADDF:
                if(arg >= 0 && arg < ARP_PROG_NUM_REGS) {
                    astate[track].x += astate[track].regs[arg];
                }
                break;
            case AP_SUBF:
                if(arg >= 0 && arg < ARP_PROG_NUM_REGS) {
                    astate[track].x -= astate[track].regs[arg];
                }
                break;
            case AP_MULF:
                if(arg >= 0 && arg < ARP_PROG_NUM_REGS) {
                    astate[track].x *= astate[track].regs[arg];
                }
                break;
            case AP_JZ:
                if(astate[track].x == 0) {
                    temp = arp_find_label(track, arg);
                    // label not found - reset program
                    if(temp == -1) {
                        arp_reset_program(track);
                        return;
                    }
                    astate[track].pc = temp;
                }
                break;
            case AP_RAND:
                astate[track].x = rand() % arg;
                break;
        }
        astate[track].pc ++;
        // see if we ran off the end of the program
        if(astate[track].pc == ARP_PROG_MAX_PROG_LEN) {
            arp_reset_program(track);
            return;
        }
        loop_count ++;
    }
}

// find the lowest held note - returns note num or -1 if not found
int arp_find_lowest_note(int track) {
    int i, min = 255;
    if(astate[track].held_note_count == 0) {
        return -1;
    }
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        if(astate[track].snapshot_notes[i] != ARP_NOTE_SLOT_FREE &&
                astate[track].snapshot_notes[i] < min) {
            min = astate[track].snapshot_notes[i];
        }
    }
    if(min == 255) {
        return -1;
    }
    return min;
}

// find the highest held note - returns note num or -1 if not found
int arp_find_highest_note(int track) {
    int i, max = -1;
    if(astate[track].held_note_count == 0) {
        return -1;
    }
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        if(astate[track].snapshot_notes[i] != ARP_NOTE_SLOT_FREE &&
                astate[track].snapshot_notes[i] > max) {
            max = astate[track].snapshot_notes[i];
        }
    }
    return max;
}

// find a held note lower than note - returns note num or -1 if not found
int arp_find_lower_note(int track, int note) {
    int i, next = -1;
    if(astate[track].held_note_count == 0) {
        return -1;
    }
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        if(astate[track].snapshot_notes[i] != ARP_NOTE_SLOT_FREE &&
                astate[track].snapshot_notes[i] > next &&
                astate[track].snapshot_notes[i] < note) {
            next = astate[track].snapshot_notes[i];
        }
    }
    return next;
}

// find a held note higher than note - returns note num or -1 if not found
int arp_find_higher_note(int track, int note) {
    int i, next = 255;
    if(astate[track].held_note_count == 0) {
        return -1;
    }
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        if(astate[track].snapshot_notes[i] != ARP_NOTE_SLOT_FREE &&
                astate[track].snapshot_notes[i] < next &&
                astate[track].snapshot_notes[i] > note) {
            next = astate[track].snapshot_notes[i];
        }
    }
    if(next == 255) {
        return -1;
    }
    return next;
}

// find a random held note - returns note num or -1 if not found
int arp_find_random_note(int track, int note) {
    int i, rand_num_notes;
    int8_t rand_notes[ARP_MAX_HELD_NOTES];
    if(astate[track].held_note_count == 0) {
        return -1;
    }
    // make a list of held notes in order
    rand_num_notes = 0;
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        if(astate[track].snapshot_notes[i] != ARP_NOTE_SLOT_FREE) {
            rand_notes[rand_num_notes] = astate[track].snapshot_notes[i];
            rand_num_notes ++;
        }
    }
    return rand_notes[rand() % rand_num_notes];
}

// start a note playing
void arp_start_note(int track, int note) {
    struct midi_msg msg;
    int i;
    for(i = 0; i < ARP_MAX_PLAYING_NOTES; i ++) {
        if(astate[track].playing_notes[i] == ARP_NOTE_SLOT_FREE) {
            midi_utils_enc_note_on(&msg, 0, 0, note, astate[track].held_velo);
            seq_engine_arp_start_note(track, &msg);
            astate[track].playing_notes[i] = note;
            astate[track].playing_note_count ++;
            astate[track].play_note_timeout = astate[track].gate_time;
            return;
        }
    }
}

// stop all notes that are playing
void arp_stop_all_notes(int track) {
    struct midi_msg msg;
    int i;
    for(i = 0; i < ARP_MAX_PLAYING_NOTES; i ++) {
        if(astate[track].playing_notes[i] != ARP_NOTE_SLOT_FREE) {
            midi_utils_enc_note_off(&msg, 0, 0, astate[track].playing_notes[i], 0x40);
            seq_engine_arp_stop_note(track, &msg);
            astate[track].playing_notes[i] = ARP_NOTE_SLOT_FREE;
            astate[track].playing_note_count --;
        }
    }
    astate[track].play_note_timeout = 0;
}

// timeout notes
void arp_timeout_notes(int track) {
    if(astate[track].play_note_timeout) {
        astate[track].play_note_timeout --;
        if(astate[track].play_note_timeout == 0) {
            arp_stop_all_notes(track);
        }
    }
}

// find a label - returns instruction number of -1 if not found
int arp_find_label(int track, int label) {
    int i;
    for(i = 0; i < ARP_PROG_MAX_PROG_LEN; i ++) {
        if(aprog[track].prog[i][ARP_PROG_INST] == AP_LABEL) {
            if(aprog[track].prog[i][ARP_PROG_ARG] == label) {
                return i;
            }
        }
    }
    return -1;
}

// take a snapshot of all held notes
void arp_take_snapshot(int track) {
    int i;
    for(i = 0; i < ARP_MAX_HELD_NOTES; i ++) {
        astate[track].snapshot_notes[i] = astate[track].held_notes[i];
    }
}
