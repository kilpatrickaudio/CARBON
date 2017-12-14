/*
 * CARBON Scale Tables
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
#ifndef SCALE_TABLES_H
#define SCALE_TABLES_H
 
#define SCALE_LEN_MAJOR 7
#define SCALE_LEN_HAR_MINOR 7
#define SCALE_LEN_NAT_MINOR 7
#define SCALE_LEN_DORIAN 7
#define SCALE_LEN_WHOLE 6
#define SCALE_LEN_PENT 5
#define SCALE_LEN_DIM 8
// new scales added in ver. 1.12
#define SCALE_LEN_PHRYGIAN 7
#define SCALE_LEN_LYDIAN 7
#define SCALE_LEN_MIXOLYDIAN 7
#define SCALE_LEN_LOCRIAN 7
#define SCALE_LEN_PENT_MINOR 5
#define SCALE_LEN_BLUES 6
#define SCALE_LEN_HALF_DIM 7
#define SCALE_LEN_SEVEN_CHORD 4

unsigned char scale_major[] = {
0	,
2	,
4	,
5	,
7	,
9	,
11	
};

unsigned char scale_har_minor[] = {
0	,
2	,
3	,
5	,
7	,
8	,
11	
};

unsigned char scale_nat_minor[] = {
0	,
2	,
3	,
5	,
7	,
8	,
10	
};

unsigned char scale_dorian[] = {
0	,
2	,
3	,
5	,
7	,
9	,
10	
};

unsigned char scale_whole[] = {
0	,
2	,
4	,
6	,
8	,
10	
};

unsigned char scale_pent[] = {
0	,
2	,
4	,
7	,
9	
};

unsigned char scale_dim[] = {
0	,
2	,
3	,
5	,
6	,
8	,
9	,
11	
};

// new scales added in ver. 1.12
unsigned char scale_phrygian[] = {
0	,
1	,
3	,
5	,
7	,
8	,
10	
};

unsigned char scale_lydian[] = {
0	,
2	,
4	,
6	,
7	,
9	,
11	
};

unsigned char scale_mixolydian[] = {
0	,
2	,
4	,
5	,
7	,
9	,
10	
};

unsigned char scale_locrian[] = {
0	,
1	,
3	,
5	,
6	,
8	,
10	
};

unsigned char scale_pent_minor[] = {
0	,
3	,
5	,
7	,
10	
};

unsigned char scale_blues[] = {
0	,
3	,
5	,
6   ,
7	,
10	
};

unsigned char scale_half_dim[] = {
0	,
2	,
3	,
5	,
6	,
8	,
10		
};

unsigned char scale_seven_chord[] = {
0	,
4	,
7	,
11		
};

#endif

