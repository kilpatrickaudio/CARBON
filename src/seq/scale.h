/*
 * CARBON Scale Processing
 *
 * Copyright 2015: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
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
#ifndef SCALE_H
#define SCALE_H
 
 // tonalities
#define SCALE_NUM_TONALITIES 16
#define SCALE_CHROMATIC 0
#define SCALE_MAJOR 1
#define SCALE_NAT_MINOR 2
#define SCALE_HAR_MINOR 3
#define SCALE_DORIAN 4
#define SCALE_WHOLE 5
#define SCALE_PENT 6
#define SCALE_DIM 7
// new scales added in ver. 1.12
// provided by Bill Butler, checked/tweaked by Andrew Kilpatrick
#define SCALE_PHRYGIAN 8
#define SCALE_LYDIAN 9
#define SCALE_MIXOLYDIAN 10
#define SCALE_LOCRIAN 11
#define SCALE_PENT_MINOR 12
#define SCALE_BLUES 13
#define SCALE_HALF_DIM 14
#define SCALE_SEVEN_CHORD 15

// convert a scale type to a name
void scale_type_to_name(char *str, unsigned char scale);

// quantize a note to the selectec scale
unsigned char scale_quantize(unsigned char note, unsigned char scale);

#endif
