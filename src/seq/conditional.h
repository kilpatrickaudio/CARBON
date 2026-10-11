/* CARBON conditional slot evaluator.
 * Copyright 2026 Johan W.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef CONDITIONAL_H
#define CONDITIONAL_H

#include "song.h"

struct conditional_state {
    uint32_t random;
    uint16_t cycle;  // modulo LCM(2..8)=840, preserves every pair's phase
    uint8_t history;
};

struct conditional_visit {
    uint8_t blocked;
    uint8_t contributed;
    uint8_t any_true;
};

static inline void conditional_seed(struct conditional_state *state, int track,
        uint32_t seed) {
    state->random = seed ^
        (UINT32_C(0x9e3779b9) * (uint32_t)(track + 1));
    // xorshift32 cannot advance from zero.
    if(!state->random) state->random = UINT32_C(0x6d2b79f5);
    state->history = 0;
    state->cycle = 0;
}

/* Fixed initialization is useful for reproducible host fixtures only. */
static inline void conditional_init(struct conditional_state *state, int track) {
    conditional_seed(state, track, UINT32_C(0x6d2b79f5));
}

/* Musical resets must not restart probability variation. */
static inline void conditional_reset(struct conditional_state *state, int track) {
    (void)track;
    state->history = 0;
    state->cycle = 0;
}

/* Exactly one xorshift32 draw per encountered probability gate, including
 * endpoints. Independent streams prevent editing/Magic from changing playback.
 */
static inline int conditional_probability(struct conditional_state *state,
        unsigned chance) {
    uint32_t x = state->random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state->random = x;
    if(chance > 100) chance = 100;
    return ((uint64_t)x * 100 >> 32) < chance;
}

/* Pair order: increasing B, then A. Invalid stored indices normalize to 7:8. */
static inline void conditional_pair(unsigned index, unsigned *a, unsigned *b) {
    if(index > 27) index = 27;
    *b = 2;
    while(index >= *b - 1) { index -= *b - 1; (*b) ++; }
    *a = index + 1;
}

static inline void conditional_advance(struct conditional_state *state) {
    state->cycle = (state->cycle + 1) % 840;
}

static inline int conditional_euclidean(unsigned position, unsigned index,
        unsigned rotation) {
    unsigned a, b, r;
    conditional_pair(index, &a, &b);
    r = (position + b - rotation % b) % b;
    return ((r * a) % b) < a;
}

/* Return true only for ordinary slots permitted to execute. Empty slots do
 * not terminate a visit. False gates block every later slot, including gates.
 */
static inline int conditional_slot(struct conditional_state *state,
        struct conditional_visit *visit, const struct track_event *event) {
    int pass;
    if(visit->blocked || event->type == SONG_EVENT_NULL) return 0;
    switch(event->type) {
        case SONG_EVENT_PROBABILITY:
            pass = conditional_probability(state, event->data0);
            break;
        case SONG_EVENT_EUCLIDEAN:
            pass = conditional_euclidean(state->cycle, event->data0, event->data1);
            break;
        case SONG_EVENT_PRE:
            pass = state->history;
            break;
        case SONG_EVENT_NON_PRE:
            // History is unchanged until commit, so all gates see prior visit.
            visit->blocked = state->history;
            return 0;
        default:
            return 1;
    }
    visit->contributed = 1;
    visit->any_true |= pass;
    visit->blocked = !pass;
    return 0;
}

static inline void conditional_commit(struct conditional_state *state,
        const struct conditional_visit *visit) {
    if(visit->contributed) state->history = visit->any_true;
}

#endif
