/*
 * CARBON Metronome
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
#ifndef METRONOME_H
#define METRONOME_H

// init the metronome
void metronome_init(void);

// realtime tasks for the metronome
void metronome_timer_task(void);

// run the metronome on music timing
void metronome_run(int tick_count);

// handle a beat cross event
void metronome_beat_cross(void);

// stop the current sound
void metronome_stop_sound(void);

//
// callback to handle mode changed
//
// the metronome mode changed
void metronome_mode_changed(int mode);

// the metronome sound length changed
void metronome_sound_len_changed(int len);

#endif

