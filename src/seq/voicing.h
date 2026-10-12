/* CARBON structural inversion and compact voicing.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * No chord recognition, scale semantics or previous-voicing history.
 */
#ifndef VOICING_H
#define VOICING_H
#include "../config.h"

// Pitches are signed intermediates. Caller retains slot/velocity/duration
// identity and applies ordinary pitch processing/range validation afterwards.
static inline void voicing_transform(int *pitches, int count, int rotations, int compact) {
    int base[SEQ_TRACK_POLY], voiced[SEQ_TRACK_POLY], group[SEQ_TRACK_POLY];
    int groups = 0, i, j, r, lowest, highest, bass;
    if(count < 2 || count > SEQ_TRACK_POLY) return;
    if(rotations < 0 || rotations > 5) return;
    // Lowest member of each pitch class represents its octave doubles.
    for(i = 0; i < count; i++) {
        for(j = 0; j < groups; j++) if(base[j] % 12 == pitches[i] % 12) break;
        if(j == groups) base[groups++] = pitches[i];
        else if(pitches[i] < base[j]) base[j] = pitches[i];
        group[i] = j;
    }
    if(groups < 2) return; // Single pitch-class sets remain unchanged.
    for(j = 0; j < groups; j++) voiced[j] = base[j];
    for(r = 0; r < rotations; r++) {
        lowest = 0; highest = -1;
        for(j = 1; j < groups; j++) if(voiced[j] < voiced[lowest]) lowest = j;
        for(i = 0; i < count; i++) {
            int pitch = voiced[group[i]] + pitches[i] - base[group[i]];
            if(pitch > highest) highest = pitch;
        }
        // First same-pitch-class octave strictly above the entire voicing.
        voiced[lowest] += ((highest - voiced[lowest]) / 12 + 1) * 12;
    }
    if(compact) {
        bass = voiced[0];
        for(j = 1; j < groups; j++) if(voiced[j] < bass) bass = voiced[j];
        for(j = 0; j < groups; j++)
            while(voiced[j] - 12 > bass) voiced[j] -= 12;
    }
    for(i = 0; i < count; i++) pitches[i] += voiced[group[i]] - base[group[i]];
}
#endif
