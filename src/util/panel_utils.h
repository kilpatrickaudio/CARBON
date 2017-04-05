/*
 * CARBON Sequencer Panel Utils
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
#ifndef PANEL_UTILS_H
#define PANEL_UTILS_H

// make a yes/no string
void panel_utils_yesno_str(char *tempstr, int val);

// make an on/off string
void panel_utils_onoff_str(char *tempstr, int val);

// make a port name
void panel_utils_port_str(char *tempstr, int port);

// make a channel name
void panel_utils_channel_str(char *tempstr, int port, int channel);

// make a clock source name
void panel_utils_clock_source_str(char *tempstr, int source);

// convert a MIDI note number to a name
void panel_utils_note_to_name(char *tempstr, int note, 
    int octdisp, int padding);

// convert a key split setting to a string
void panel_utils_key_split_str(char *tempstr, int key_split);

// convert a step length to a string
void panel_utils_step_len_to_str(char *tempstr, int speed);

// convert a clock PPQ to a string
void panel_utils_clock_ppq_to_str(char *tempstr, int ppq);

// convert a gate length to a string by rounding down
void panel_utils_gate_time_to_str(char *tempstr, int time);

// convert a transposition to a string
void panel_utils_transpose_to_str(char *tempstr, int trans);

// handle the scrolling of lists by managing the display and edit positions
// if the edit slot equals the returned list position it should be editable
void panel_utils_scroll_list(int numslots, int edit_pos, int *display_pos);

// convert a CV/gate pair to a string
void panel_utils_cvgate_pair_to_str(char *tempstr, int pair);

// convert a CV/gate output mode to a string
void panel_utils_cvgate_pair_mode_to_str(char *tempstr, int mode);

// convert a CV output scaling mode to a string
void panel_utills_cv_output_scaling_to_str(char *tempstr, int mode);

// get a "blank" string to use for an undefined value
void panel_utils_get_blank_str(char *tempstr);

#endif
