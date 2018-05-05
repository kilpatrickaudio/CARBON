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
#include <stdio.h>

// current arp program store
struct arp_prog aprog[SEQ_NUM_TRACKS];
int aprog_load_track;  // global used for loading programs

// local functions
void arp_progs_clear(void);
void arp_progs_ai(int inst, int arg);
void arp_progs_generate_up(int octaves);
void arp_progs_generate_down(int octaves);
void arp_progs_generate_updown(int octaves);
void arp_progs_generate_random(int octaves);
void arp_progs_generate_note_order(int octaves);
void arp_progs_generate_updown_norepeat(int octaves);
void arp_progs_generate_repeat(int notes, int rests);
void arp_progs_generate_up_low(int octaves);
void arp_progs_generate_down_high(int octaves);

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
        case ARP_TYPE_NOTE_ORDER1:
            arp_progs_generate_note_order(1);
            break;
        case ARP_TYPE_NOTE_ORDER2:
            arp_progs_generate_note_order(2);
            break;
        case ARP_TYPE_NOTE_ORDER3:
            arp_progs_generate_note_order(3);
            break;
        case ARP_TYPE_NOTE_ORDER4:
            arp_progs_generate_note_order(4);
            break;
        case ARP_TYPE_UPDOWN1_NR:
            arp_progs_generate_updown_norepeat(1);
            break;
        case ARP_TYPE_UPDOWN2_NR:
            arp_progs_generate_updown_norepeat(2);
            break;
        case ARP_TYPE_UPDOWN3_NR:
            arp_progs_generate_updown_norepeat(3);
            break;
        case ARP_TYPE_UPDOWN4_NR:
            arp_progs_generate_updown_norepeat(4);
            break;
        case ARP_TYPE_REPEAT1_0:
            arp_progs_generate_repeat(1, 0);
            break;
        case ARP_TYPE_REPEAT1_1:
            arp_progs_generate_repeat(1, 1);
            break;
        case ARP_TYPE_REPEAT2_1:
            arp_progs_generate_repeat(2, 1);
            break;
        case ARP_TYPE_REPEAT3_1:
            arp_progs_generate_repeat(3, 1);
            break;
        case ARP_TYPE_REPEAT4_1:
            arp_progs_generate_repeat(4, 1);
            break;
        case ARP_TYPE_UP_LOW1:
            arp_progs_generate_up_low(1);
            break;
        case ARP_TYPE_UP_LOW2:
            arp_progs_generate_up_low(2);
            break;
        case ARP_TYPE_UP_LOW3:
            arp_progs_generate_up_low(3);
            break;
        case ARP_TYPE_UP_LOW4:
            arp_progs_generate_up_low(4);
            break;
        case ARP_TYPE_DOWN_HIGH1:
            arp_progs_generate_down_high(1);
            break;
        case ARP_TYPE_DOWN_HIGH2:
            arp_progs_generate_down_high(2);
            break;
        case ARP_TYPE_DOWN_HIGH3:
            arp_progs_generate_down_high(3);
            break;
        case ARP_TYPE_DOWN_HIGH4:
            arp_progs_generate_down_high(4);
            break;
        default:
            break;
    }
}

//
// helper functions
//
// convert an arp type to a text string
void arp_type_to_name(char *text, int type) {
    switch(type) {
        case ARP_TYPE_UP1:
            sprintf(text, "Up 1");
            break;
        case ARP_TYPE_UP2:
            sprintf(text, "Up 2");
            break;
        case ARP_TYPE_UP3:
            sprintf(text, "Up 3");
            break;
        case ARP_TYPE_UP4:
            sprintf(text, "Up 4");
            break;
        case ARP_TYPE_DOWN1:
            sprintf(text, "Down 1");
            break;
        case ARP_TYPE_DOWN2:
            sprintf(text, "Down 2");
            break;
        case ARP_TYPE_DOWN3:
            sprintf(text, "Down 3");
            break;
        case ARP_TYPE_DOWN4:
            sprintf(text, "Down 4");
            break;
        case ARP_TYPE_UPDOWN1:
            sprintf(text, "Up/Down 1");
            break;
        case ARP_TYPE_UPDOWN2:
            sprintf(text, "Up/Down 2");
            break;
        case ARP_TYPE_UPDOWN3:
            sprintf(text, "Up/Down 3");
            break;
        case ARP_TYPE_UPDOWN4:
            sprintf(text, "Up/Down 4");
            break;
        case ARP_TYPE_RANDOM1:
            sprintf(text, "Random 1");
            break;
        case ARP_TYPE_RANDOM2:
            sprintf(text, "Random 2");
            break;
        case ARP_TYPE_RANDOM3:
            sprintf(text, "Random 3");
            break;
        case ARP_TYPE_RANDOM4:
            sprintf(text, "Random 4");
            break;
        case ARP_TYPE_NOTE_ORDER1:
            sprintf(text, "Order 1");
            break;
        case ARP_TYPE_NOTE_ORDER2:
            sprintf(text, "Order 2");
            break;
        case ARP_TYPE_NOTE_ORDER3:
            sprintf(text, "Order 3");
            break;
        case ARP_TYPE_NOTE_ORDER4:
            sprintf(text, "Order 4");
            break;
        case ARP_TYPE_UPDOWN1_NR:
            sprintf(text, "Up/Down 1 NR");
            break;
        case ARP_TYPE_UPDOWN2_NR:
            sprintf(text, "Up/Down 2 NR");
            break;
        case ARP_TYPE_UPDOWN3_NR:
            sprintf(text, "Up/Down 3 NR");
            break;
        case ARP_TYPE_UPDOWN4_NR:
            sprintf(text, "Up/Down 4 NR");
            break;
        case ARP_TYPE_REPEAT1_0:
            sprintf(text, "Repeat 1:0");
            break;
        case ARP_TYPE_REPEAT1_1:
            sprintf(text, "Repeat 1:1");
            break;
        case ARP_TYPE_REPEAT2_1:
            sprintf(text, "Repeat 2:1");
            break;
        case ARP_TYPE_REPEAT3_1:
            sprintf(text, "Repeat 3:1");
            break;
        case ARP_TYPE_REPEAT4_1:
            sprintf(text, "Repeat 4:1");
            break;
        case ARP_TYPE_UP_LOW1:
            sprintf(text, "Up (Low) 1");
            break;
        case ARP_TYPE_UP_LOW2:
            sprintf(text, "Up (Low) 2");
            break;
        case ARP_TYPE_UP_LOW3:
            sprintf(text, "Up (Low) 3");
            break;
        case ARP_TYPE_UP_LOW4:
            sprintf(text, "Up (Low) 4");
            break;
        case ARP_TYPE_DOWN_HIGH1:
            sprintf(text, "Down (High) 1");
            break;
        case ARP_TYPE_DOWN_HIGH2:
            sprintf(text, "Down (High) 2");
            break;
        case ARP_TYPE_DOWN_HIGH3:
            sprintf(text, "Down (High) 3");
            break;
        case ARP_TYPE_DOWN_HIGH4:
            sprintf(text, "Down (High) 4");
            break;
        default:
            sprintf(text, " ");
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
void arp_progs_ai(int inst, int arg) {
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
    arp_progs_ai(AP_LABEL, INIT);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, 0);  // init transpose
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    // play notes in octave
    arp_progs_ai(AP_LABEL, START);
    arp_progs_ai(AP_FIND_LOWEST_NOTE, INIT);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_ai(AP_LABEL, UP_LOOP);
    arp_progs_ai(AP_FIND_HIGHER_NOTE, TRANS);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, UP_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START);  // do next octave
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
    arp_progs_ai(AP_LABEL, INIT);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_SUBL, 1);  // subtract 1 octave so we don't start higher for 1 oct
    arp_progs_ai(AP_MULL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);  // set note offset to higher octaves
    // play notes in octave
    arp_progs_ai(AP_LABEL, START);
    arp_progs_ai(AP_FIND_HIGHEST_NOTE, INIT);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_ai(AP_LABEL, DOWN_LOOP);
    arp_progs_ai(AP_FIND_LOWER_NOTE, TRANS);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, DOWN_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, -12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START);  // do next octave
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
    arp_progs_ai(AP_LABEL, INIT_UP);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, 0);  // init transpose
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    // play notes in octave
    arp_progs_ai(AP_LABEL, START_UP);
    arp_progs_ai(AP_FIND_LOWEST_NOTE, INIT_UP);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_ai(AP_LABEL, UP_LOOP);
    arp_progs_ai(AP_FIND_HIGHER_NOTE, TRANS_UP);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, UP_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS_UP);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT_DOWN);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START_UP);  // do next octave
    // setup down
    arp_progs_ai(AP_LABEL, INIT_DOWN);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_SUBL, 1);  // subtract 1 octave so we don't start higher for 1 oct
    arp_progs_ai(AP_MULL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);  // set note offset to higher octaves
    // play notes in octave
    arp_progs_ai(AP_LABEL, START_DOWN);
    arp_progs_ai(AP_FIND_HIGHEST_NOTE, INIT_UP);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_LABEL, DOWN_LOOP);
    arp_progs_ai(AP_FIND_LOWER_NOTE, TRANS_DOWN);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, DOWN_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS_DOWN);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT_UP);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, -12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START_DOWN);  // do next octave
}

// generate a random arp pattern
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
    arp_progs_ai(AP_LABEL, INIT);
    arp_progs_ai(AP_SNAPSHOT, 0);
    // do transposing
    if(octaves > 1) {
        arp_progs_ai(AP_RAND, octaves);
        arp_progs_ai(AP_MULL, 12);
        arp_progs_ai(AP_STOREF, TRANS);
    }
    else {
        arp_progs_ai(AP_LOADL, 0);
        arp_progs_ai(AP_STOREF, TRANS);
    }
    // get note
    arp_progs_ai(AP_FIND_RANDOM_NOTE, INIT);
    arp_progs_ai(AP_ADDF, TRANS);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, INIT);
}

// generate a note order arp pattern
void arp_progs_generate_note_order(int octaves) {
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
    arp_progs_ai(AP_LABEL, INIT);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, 0);  // init transpose
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    // play notes in octave
    arp_progs_ai(AP_LABEL, START);
    arp_progs_ai(AP_FIND_OLDEST_NOTE, INIT);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_ai(AP_LABEL, UP_LOOP);
    arp_progs_ai(AP_FIND_NEWER_NOTE, TRANS);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, UP_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START);  // do next octave
}

// generate an up/down norepeat arp program
void arp_progs_generate_updown_norepeat(int octaves) {
    // labels
    enum {
        INIT_UP,
        LOOP_START_UP,
        START_UP,
        UP_LOOP,
        TRANS_UP,
        INIT_DOWN,
        START_DOWN,
        DOWN_LOOP,
        TRANS_DOWN,
        END_LOOP
    };
    // regs
    enum {
        OCT_COUNT,
        LOOPING,
        FIRST_OCTAVE
    };

    // initial setup
    arp_progs_ai(AP_LOADL, 0);  // init looping flag
    arp_progs_ai(AP_STOREF, LOOPING);
    // setup up
    arp_progs_ai(AP_LABEL, INIT_UP);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, 0);  // init transpose
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_LOADF, LOOPING);  // handle looping
    arp_progs_ai(AP_JZ, START_UP);  // normal (play lowest note)
    // eat lowest note
    arp_progs_ai(AP_FIND_LOWEST_NOTE, INIT_UP);
    arp_progs_ai(AP_JUMP, UP_LOOP);
    // play notes in octave
    arp_progs_ai(AP_LABEL, START_UP);
    arp_progs_ai(AP_FIND_LOWEST_NOTE, INIT_UP);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    arp_progs_ai(AP_LABEL, UP_LOOP);
    arp_progs_ai(AP_FIND_HIGHER_NOTE, TRANS_UP);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, UP_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS_UP);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT_DOWN);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START_UP);  // do next octave
    // setup down
    arp_progs_ai(AP_LABEL, INIT_DOWN);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_SUBL, 1);  // subtract 1 octave so we don't start higher for 1 oct
    arp_progs_ai(AP_MULL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);  // set note offset to higher octave
    arp_progs_ai(AP_FIND_HIGHEST_NOTE, INIT_UP);  // nom the highest note
    arp_progs_ai(AP_JUMP, DOWN_LOOP);  // skip the start so we don't play highest note
    // play notes in down octave
    arp_progs_ai(AP_LABEL, START_DOWN);
    arp_progs_ai(AP_FIND_HIGHEST_NOTE, INIT_UP);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_LABEL, DOWN_LOOP);
    arp_progs_ai(AP_FIND_LOWER_NOTE, TRANS_DOWN);
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_JUMP, DOWN_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS_DOWN);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, END_LOOP);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, -12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START_DOWN);  // do next octave
    // end of loop
    arp_progs_ai(AP_LABEL, END_LOOP);
    arp_progs_ai(AP_LOADL, 1);  // set looping flag
    arp_progs_ai(AP_STOREF, LOOPING);
    arp_progs_ai(AP_JUMP, INIT_UP);  // restart
}

// generate a repeats arp
void arp_progs_generate_repeat(int notes, int rests) {
    // labels
    enum {
        INIT,
        NOTE,
        PLAY_NOTE_LOOP,
        PLAY_WAIT,
        REST
    };
    // regs
    enum {
        NOTES_COUNT,
        RESTS_COUNT
    };

    // setup
    arp_progs_ai(AP_LABEL, INIT);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, notes);  // init notes counter
    arp_progs_ai(AP_STOREF, NOTES_COUNT);
    arp_progs_ai(AP_LOADL, rests);  // init rests counter
    arp_progs_ai(AP_STOREF, RESTS_COUNT);
    // play notes
    arp_progs_ai(AP_LABEL, NOTE);
    arp_progs_ai(AP_LOADF, NOTES_COUNT);
    arp_progs_ai(AP_JZ, REST);  // no more notes
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, NOTES_COUNT);
    arp_progs_ai(AP_FIND_LOWEST_NOTE, INIT);  // find and play lowest note
    arp_progs_ai(AP_PLAY_NOTE, 0);  // play note
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LABEL, PLAY_NOTE_LOOP);
    arp_progs_ai(AP_FIND_HIGHER_NOTE, PLAY_WAIT);  // find higher note
    arp_progs_ai(AP_PLAY_NOTE, 0);  // play note
    arp_progs_ai(AP_JUMP, PLAY_NOTE_LOOP);  // do next higher note
    arp_progs_ai(AP_LABEL, PLAY_WAIT);
    arp_progs_ai(AP_WAIT, 0);  // wait for next step
    arp_progs_ai(AP_JUMP, NOTE);  // try another note
    // play rests
    arp_progs_ai(AP_LABEL, REST);
    arp_progs_ai(AP_LOADF, RESTS_COUNT);
    arp_progs_ai(AP_JZ, INIT);  // no more rests
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, RESTS_COUNT);
    arp_progs_ai(AP_WAIT, 0);  // wait for next step
    arp_progs_ai(AP_JUMP, REST);  // try another rest
}

// generate an up low arp program
void arp_progs_generate_up_low(int octaves) {
    // labels
    enum {
        INIT,
        START,
        UP_LOOP,
        TRANS
    };
    // regs
    enum {
        OCT_COUNT,
        LAST_NOTE,
    };

    // setup
    arp_progs_ai(AP_LABEL, INIT);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, 0);  // init transpose
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    // play notes in octave
    arp_progs_ai(AP_LABEL, START);
    arp_progs_ai(AP_FIND_LOWEST_NOTE, INIT);  // find lowest note
    arp_progs_ai(AP_STOREF, LAST_NOTE);  // store it so we can restore
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);  // play lowest note
    arp_progs_ai(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    // up loop
    arp_progs_ai(AP_LABEL, UP_LOOP);
    arp_progs_ai(AP_LOADF, LAST_NOTE);  // restore the last note we found
    arp_progs_ai(AP_FIND_HIGHER_NOTE, TRANS);  // play a higher note
    arp_progs_ai(AP_STOREF, LAST_NOTE);  // store it so we can restore
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);  // play note
    arp_progs_ai(AP_FIND_HIGHER_NOTE, TRANS);  // ensure we don't play low note at top
    arp_progs_ai(AP_FIND_LOWEST_NOTE, INIT);  // find lowest note again
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);  // play note
    arp_progs_ai(AP_LOADF, LAST_NOTE);  // restore the last note we found
    arp_progs_ai(AP_JUMP, UP_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START);  // do next octave
}

// generate an down high arp program
void arp_progs_generate_down_high(int octaves) {
    // labels
    enum {
        INIT,
        START,
        DOWN_LOOP,
        TRANS
    };
    // regs
    enum {
        OCT_COUNT,
        LAST_NOTE
    };

    // setup
    arp_progs_ai(AP_LABEL, INIT);
    arp_progs_ai(AP_SNAPSHOT, 0);
    arp_progs_ai(AP_LOADL, octaves);  // init octave counter
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_SUBL, 1);  // subtract 1 octave so we don't start higher for 1 oct
    arp_progs_ai(AP_MULL, 12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);  // set note offset to higher octaves
    // play notes in octave
    arp_progs_ai(AP_LABEL, START);
    arp_progs_ai(AP_FIND_HIGHEST_NOTE, INIT);
    arp_progs_ai(AP_STOREF, LAST_NOTE);  // store it so we can restore
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);
    arp_progs_ai(AP_SNAPSHOT, 0);  // make sure we get all initial notes
    // down loop
    arp_progs_ai(AP_LABEL, DOWN_LOOP);
    arp_progs_ai(AP_LOADF, LAST_NOTE);  // restore the last note we found
    arp_progs_ai(AP_FIND_LOWER_NOTE, TRANS);  // play lower note
    arp_progs_ai(AP_STOREF, LAST_NOTE);  // store it so we can restore
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);  // play note
    arp_progs_ai(AP_FIND_LOWER_NOTE, TRANS);  // ensure we don't play highest note at bottom
    arp_progs_ai(AP_FIND_HIGHEST_NOTE, INIT);  // find highest note avain
    arp_progs_ai(AP_PLAY_NOTE_AND_WAIT, 0);  // play note
    arp_progs_ai(AP_LOADF, LAST_NOTE);  // restore the last note we found
    arp_progs_ai(AP_JUMP, DOWN_LOOP);
    // do transposing
    arp_progs_ai(AP_LABEL, TRANS);
    arp_progs_ai(AP_LOADF, OCT_COUNT);  // handle octave counter
    arp_progs_ai(AP_SUBL, 1);
    arp_progs_ai(AP_STOREF, OCT_COUNT);
    arp_progs_ai(AP_JZ, INIT);  // restart if done all octaves
    arp_progs_ai(AP_LOADF, ARP_REG_NOTE_OFFSET);  // handle transpose
    arp_progs_ai(AP_ADDL, -12);
    arp_progs_ai(AP_STOREF, ARP_REG_NOTE_OFFSET);
    arp_progs_ai(AP_JUMP, START);  // do next octave
}
