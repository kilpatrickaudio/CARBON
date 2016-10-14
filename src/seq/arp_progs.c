/*
 * CARBON Arpeggiator Programs
 *
 * Copyright 2015: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
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
#include "arp_progs.h"
#include "arp.h"
#include "../config.h"

// current arp program store
struct arp_prog aprog[SEQ_NUM_TRACKS];
int aprog_load_track;  // global used for loading programs

// local functions
void arp_progs_clear(void);
void arp_progs_addin(int inst, int arg);
void arp_progs_generate_up(int octaves);
void arp_progs_generate_down(int octaves);
void arp_progs_generate_updown(int octaves);
void arp_progs_generate_random(int octaves);

// init the arp programs
void arp_progs_init(void) {
    int track;
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        aprog_load_track = track;
        arp_progs_clear();
    }
}

// load a program
void arp_progs_load(int track, int prog) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        return;
    }
    aprog_load_track = track;
    arp_progs_clear();
    switch(prog) {
        case ARP_TYPE_UP1:
            arp_progs_generate_up(1);
            break;
        case ARP_TYPE_UP2:
            arp_progs_generate_up(2);
            break;
        case ARP_TYPE_UP3:
            arp_progs_generate_up(3);
            break;
        case ARP_TYPE_UP4:
            arp_progs_generate_up(4);
            break;
        case ARP_TYPE_DOWN1:
            arp_progs_generate_down(1);
            break;
        case ARP_TYPE_DOWN2:
            arp_progs_generate_down(2);
            break;
        case ARP_TYPE_DOWN3:
            arp_progs_generate_down(3);
            break;
        case ARP_TYPE_DOWN4:
            arp_progs_generate_down(4);
            break;
        case ARP_TYPE_UPDOWN1:
            arp_progs_generate_updown(1);
            break;
        case ARP_TYPE_UPDOWN2:
            arp_progs_generate_updown(2);
            break;
        case ARP_TYPE_UPDOWN3:
            arp_progs_generate_updown(3);
            break;
        case ARP_TYPE_UPDOWN4:
            arp_progs_generate_updown(4);
            break;
        case ARP_TYPE_RANDOM1:
            arp_progs_generate_random(1);
            break;
        case ARP_TYPE_RANDOM2:
            arp_progs_generate_random(2);
            break;
        case ARP_TYPE_RANDOM3:
            arp_progs_generate_random(3);
            break;
        case ARP_TYPE_RANDOM4:
            arp_progs_generate_random(4);
            break;
        default:
            break;
    }
}

//
// local functions
//
// clear the program
void arp_progs_clear(void) {
    int i;
    // clear the live program
    for(i = 0; i < ARP_PROG_MAX_PROG_LEN; i ++) {
        aprog[aprog_load_track].prog[i][ARP_PROG_INST] = AP_NOP;
        aprog[aprog_load_track].prog[i][ARP_PROG_ARG] = 0;
    }
    aprog[aprog_load_track].inst_count = 0;
}

// add an instruction to the current program
void arp_progs_addin(int inst, int arg) {
    if(aprog[aprog_load_track].inst_count == ARP_PROG_MAX_PROG_LEN) {
        return;
    }
    aprog[aprog_load_track].prog[aprog[aprog_load_track].inst_count][ARP_PROG_INST] = inst;
    aprog[aprog_load_track].prog[aprog[aprog_load_track].inst_count][ARP_PROG_ARG] = arg;
    aprog[aprog_load_track].inst_count ++;
}

// generate an up arp program
void arp_progs_generate_up(int octaves) {
    // labels
    enum {
        INIT,
        START,
        UP_LOOP,
        TRANS
    };
    // regs
    enum {
        OCT_COUNT
    };

    // setup
    arp_progs_addin(AP_LABEL, INIT);
    arp_progs_addin(AP_SNAPSHOT, 0);
    arp_progs_addin(AP_LOADL, 0);  // init transpose
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_addin(AP_LOADL, octaves);  // init octave counter
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    // play notes in octave
    arp_progs_addin(AP_LABEL, START);
    arp_progs_addin(AP_FIND_LOWEST_NOTE, INIT);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_addin(AP_LABEL, UP_LOOP);
    arp_progs_addin(AP_FIND_HIGHER_NOTE, TRANS);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_JUMP, UP_LOOP);
    // do transposing
    arp_progs_addin(AP_LABEL, TRANS);
    arp_progs_addin(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_addin(AP_SUBL, 1);
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    arp_progs_addin(AP_JZ, INIT);  // restart if done all octaves
    arp_progs_addin(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_addin(AP_ADDL, 12);
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_addin(AP_JUMP, START);  // do next octave
}

// generate an down arp program
void arp_progs_generate_down(int octaves) {
    // labels
    enum {
        INIT,
        START,
        DOWN_LOOP,
        TRANS
    };
    // regs
    enum {
        OCT_COUNT
    };

    // setup
    arp_progs_addin(AP_LABEL, INIT);
    arp_progs_addin(AP_SNAPSHOT, 0);
    arp_progs_addin(AP_LOADL, octaves);  // init octave counter
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    arp_progs_addin(AP_SUBL, 1);  // subtract 1 octave so we don't start higher for 1 oct
    arp_progs_addin(AP_MULL, 12);
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);  // set note offset to higher octaves
    // play notes in octave
    arp_progs_addin(AP_LABEL, START);
    arp_progs_addin(AP_FIND_HIGHEST_NOTE, INIT);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_addin(AP_LABEL, DOWN_LOOP);
    arp_progs_addin(AP_FIND_LOWER_NOTE, TRANS);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_JUMP, DOWN_LOOP);
    // do transposing
    arp_progs_addin(AP_LABEL, TRANS);
    arp_progs_addin(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_addin(AP_SUBL, 1);
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    arp_progs_addin(AP_JZ, INIT);  // restart if done all octaves
    arp_progs_addin(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_addin(AP_ADDL, -12);
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_addin(AP_JUMP, START);  // do next octave
}

// generate an up/down arp program
void arp_progs_generate_updown(int octaves) {
    // labels
    enum {
        INIT_UP,
        START_UP,
        UP_LOOP,
        TRANS_UP,
        INIT_DOWN,
        START_DOWN,
        DOWN_LOOP,
        TRANS_DOWN
    };
    // regs
    enum {
        OCT_COUNT
    };

    // setup
    arp_progs_addin(AP_LABEL, INIT_UP);
    arp_progs_addin(AP_SNAPSHOT, 0);
    arp_progs_addin(AP_LOADL, 0);  // init transpose
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_addin(AP_LOADL, octaves);  // init octave counter
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    // play notes in octave
    arp_progs_addin(AP_LABEL, START_UP);
    arp_progs_addin(AP_FIND_LOWEST_NOTE, INIT_UP);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_addin(AP_LABEL, UP_LOOP);
    arp_progs_addin(AP_FIND_HIGHER_NOTE, TRANS_UP);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_JUMP, UP_LOOP);
    // do transposing
    arp_progs_addin(AP_LABEL, TRANS_UP);
    arp_progs_addin(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_addin(AP_SUBL, 1);
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    arp_progs_addin(AP_JZ, INIT_DOWN);  // restart if done all octaves
    arp_progs_addin(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_addin(AP_ADDL, 12);
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_addin(AP_JUMP, START_UP);  // do next octave
    // setup down
    arp_progs_addin(AP_LABEL, INIT_DOWN);
    arp_progs_addin(AP_LOADL, octaves);  // init octave counter
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    arp_progs_addin(AP_SUBL, 1);  // subtract 1 octave so we don't start higher for 1 oct
    arp_progs_addin(AP_MULL, 12);
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);  // set note offset to higher octaves
    // play notes in octave
    arp_progs_addin(AP_LABEL, START_DOWN);
    arp_progs_addin(AP_FIND_HIGHEST_NOTE, INIT_UP);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_LABEL, DOWN_LOOP);
    arp_progs_addin(AP_FIND_LOWER_NOTE, TRANS_DOWN);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_JUMP, DOWN_LOOP);
    // do transposing
    arp_progs_addin(AP_LABEL, TRANS_DOWN);
    arp_progs_addin(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_addin(AP_SUBL, 1);
    arp_progs_addin(AP_STOREF, OCT_COUNT);
    arp_progs_addin(AP_JZ, INIT_UP);  // restart if done all octaves
    arp_progs_addin(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_addin(AP_ADDL, -12);
    arp_progs_addin(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_addin(AP_JUMP, START_DOWN);  // do next octave   
}

// generate a random arp patter
void arp_progs_generate_random(int octaves) {
    // labels
    enum {
        INIT
    };
    // regs
    enum {
        TRANS
    };

    // setup
    arp_progs_addin(AP_LABEL, INIT);
    arp_progs_addin(AP_SNAPSHOT, 0);
    // do transposing 
    if(octaves > 1) {
        arp_progs_addin(AP_RAND, octaves);
        arp_progs_addin(AP_MULL, 12);
        arp_progs_addin(AP_STOREF, TRANS);
    }
    else {
        arp_progs_addin(AP_LOADL, 0);
        arp_progs_addin(AP_STOREF, TRANS);    
    }
    // get note    
    arp_progs_addin(AP_FIND_RANDOM_NOTE, INIT);
    arp_progs_addin(AP_ADDF, TRANS);
    arp_progs_addin(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_addin(AP_JUMP, INIT);
}


