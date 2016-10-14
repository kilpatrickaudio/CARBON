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
#include "metronome.h"
#include "clock.h"
#include "outproc.h"
#include "seq_ctrl.h"
#include "song.h"
#include "../config.h"
#include "../analog_out.h"
#include "../gui/panel.h"
#include "../midi/midi_utils.h"

struct metronome_state {
    int mode;  // local cache of the metronome mode
    int div_count;
    int sound_timeout;  // the timeout for turning off the sound
    int sound_note;  // the note that was most recently used for making a pulse
    int track_mute;
    int beat_cross;
};
struct metronome_state mnstate;

// init the metronome
void metronome_init(void) {
    mnstate.div_count = 0;
    mnstate.sound_timeout = 0;
    mnstate.mode = song_get_metronome_mode();
    mnstate.beat_cross = 0;
}

// realtime tasks for the metronome
void metronome_timer_task(void) {
    static int task_div = 0;

    // run this once every 4 intervals (1000us)    
    if((task_div & 0x03) == 0) {
        // time out sound
        if(mnstate.sound_timeout) {
            mnstate.sound_timeout --;
            if(mnstate.sound_timeout == 0) {
                metronome_stop_sound();
            }
        }    
    }
}

// run the metronome on music timing
void metronome_run(int tick_count) {
    struct midi_msg send_msg;
    // pulse when playing
    if(mnstate.beat_cross) {
        mnstate.beat_cross = 0;
        panel_blink_beat_led();
        // only play metronome while running
        if(clock_get_running()) {
            panel_blink_beat_led();
            // if we're recording make the sound
            if(seq_ctrl_get_record_mode() != SEQ_CTRL_RECORD_IDLE) {
                switch(mnstate.mode) {
                    case SONG_METRONOME_OFF:
                        // no action
                        break;
                    case SONG_METRONOME_INTERNAL:
                        mnstate.sound_timeout = METRONOME_SOUND_LENGTH;
                        analog_out_beep_metronome(1);
                        break;
                    case SONG_METRONOME_CV_RESET:
                        mnstate.sound_timeout = METRONOME_SOUND_LENGTH;
                        analog_out_set_reset(1);
                        break;
                    default:
                        // observe mute state on track
                        if(seq_ctrl_get_mute_select(METRONOME_MIDI_TRACK)) {
                            break;
                        }
                        mnstate.sound_note = mnstate.mode;  // make sure we turn off the same note
                        mnstate.sound_timeout = METRONOME_SOUND_LENGTH;
                        midi_utils_enc_note_on(&send_msg, 0, 0, 
                            mnstate.sound_note, METRONOME_NOTE_VELOCITY);
                        outproc_deliver_msg(seq_ctrl_get_scene(), 
                            METRONOME_MIDI_TRACK, &send_msg, 
                            OUTPROC_DELIVER_A, OUTPROC_OUTPUT_RAW);
                        return;
                }
            }
        }
    }
}

// handle a beat cross event
void metronome_beat_cross(void) {
    mnstate.beat_cross = 1;
}

// stop the current sound
void metronome_stop_sound(void) {
    struct midi_msg send_msg;
    switch(mnstate.mode) {
        case SONG_METRONOME_OFF:
            // no action
            break;
        case SONG_METRONOME_INTERNAL:
            analog_out_beep_metronome(0);
            break;
        case SONG_METRONOME_CV_RESET:
            analog_out_set_reset(0);
            break;
        default:    
            if(mnstate.sound_note == -1) {
                return;
            }
            midi_utils_enc_note_off(&send_msg, 0, 0, mnstate.sound_note, 0x40);
            outproc_deliver_msg(seq_ctrl_get_scene(), METRONOME_MIDI_TRACK, 
                &send_msg, OUTPROC_DELIVER_A, OUTPROC_OUTPUT_RAW);
            mnstate.sound_note = -1;
            return;
    }
}

//
// callback to handle mode changed
//
// the metronome mode changed
void metronome_mode_changed(int mode) {
    mnstate.mode = mode;
    metronome_stop_sound();
}

