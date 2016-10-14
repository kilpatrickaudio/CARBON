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
#include "scale.h"
#include "../tables/scale_tables.h"
#include "../config.h"
#include <stdio.h>

// convert a scale type to a name
void scale_type_to_name(char *str, unsigned char scale) {
	if(scale == SCALE_CHROMATIC) sprintf(str, "Chromatic");
	else if(scale == SCALE_MAJOR) sprintf(str, "Major");
	else if(scale == SCALE_NAT_MINOR) sprintf(str, "Nat Minor");
	else if(scale == SCALE_HAR_MINOR) sprintf(str, "Har Minor");
	else if(scale == SCALE_DORIAN) sprintf(str, "Dorian");
	else if(scale == SCALE_WHOLE) sprintf(str, "Whole");
	else if(scale == SCALE_PENT) sprintf(str, "Pentatonic");
	else if(scale == SCALE_DIM) sprintf(str, "Diminished");
}

// quantize a note to the current scale
unsigned char scale_quantize(unsigned char note, unsigned char scale) {
	unsigned char nt;
	int i, shift;
	shift = (note / 12) * 12;
	nt = note - shift;
	switch(scale) {
        case SCALE_MAJOR:
			for(i = SCALE_LEN_MAJOR - 1; i >= 0; i --) {
				if(scale_major[i] <= nt) {
					nt = scale_major[i];
					break;
				}
			}
		    break;
		case SCALE_NAT_MINOR:
			for(i = SCALE_LEN_NAT_MINOR - 1; i >= 0; i --) {
				if(scale_nat_minor[i] <= nt) {
					nt = scale_nat_minor[i];
					break;
				}
			}
		    break;
		case SCALE_HAR_MINOR:
			for(i = SCALE_LEN_HAR_MINOR - 1; i >= 0; i --) {
				if(scale_har_minor[i] <= nt) {
					nt = scale_har_minor[i];
					break;
				}
			}
		    break;
		case SCALE_DORIAN:
			for(i = SCALE_LEN_DORIAN - 1; i >= 0; i --) {
				if(scale_dorian[i] <= nt) {
					nt = scale_dorian[i];
					break;
				}
			}		
		    break;
		case SCALE_WHOLE:
			for(i = SCALE_LEN_WHOLE - 1; i >= 0; i --) {
				if(scale_whole[i] <= nt) {
					nt = scale_whole[i];
					break;
				}
			}
		    break;
		case SCALE_PENT:
			for(i = SCALE_LEN_PENT - 1; i >= 0; i --) {
				if(scale_pent[i] <= nt) {
					nt = scale_pent[i];
					break;
				}
			}
		    break;
		case SCALE_DIM:
			for(i = SCALE_LEN_DIM - 1; i >= 0; i --) {
				if(scale_dim[i] <= nt) {
					nt = scale_dim[i];
					break;
				}
			}
		    break;
	}
	return nt + shift;
}

