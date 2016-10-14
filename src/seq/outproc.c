/*
 * CARBON MIDI Output Processing
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
#include "outproc.h"
#include "scale.h"
#include "seq_engine.h"
#include "song.h"
#include "../midi/midi_stream.h"
#include "../config.h"
#include "../util/log.h"

// macros
#define PROCESS_NOTE(scene, track, send_msg) \
    send_msg.data0 = scale_quantize(send_msg.data0, opstate.current_tonality[track]); \
    send_msg.data0 = send_msg.data0 + opstate.current_transpose[track];

// internal settings
#define OUTPROC_MAX_NOTES 16  // active notes per track

// outproc state
struct outproc_state {
    struct midi_msg output_notes[SEQ_NUM_TRACKS][OUTPROC_MAX_NOTES];  // stores note on msgs
    int current_transpose[SEQ_NUM_TRACKS];
    int current_tonality[SEQ_NUM_TRACKS];
};
struct outproc_state opstate;

// local functions
int outproc_enqueue_note(int track, struct midi_msg *on_msg);
void outproc_dequeue_note(int track, struct midi_msg *off_msg);
int outproc_get_num_notes(int track);

// init the output processor
void outproc_init(void) {
    int i, j;
    // reset the state
    for(j = 0; j < SEQ_NUM_TRACKS; j ++) {
        for(i = 0; i < OUTPROC_MAX_NOTES; i ++) {
            opstate.output_notes[j][i].status = 0;
        }
        opstate.current_transpose[j] = 0;
        opstate.current_tonality[j] = SCALE_CHROMATIC;
    }
}

// the transpose changed on a track
void outproc_transpose_changed(int scene, int track) {
    int i, new_transpose;
    struct midi_msg send_msg;    
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("otrc - track invalid: %d", track);
        return;
    }
    if(scene != seq_engine_get_current_scene()) {
        return;
    }
    new_transpose = song_get_transpose(scene, track);
    if(outproc_get_num_notes(track) == 0) {
        opstate.current_transpose[track] = new_transpose;
        return;
    }    
    if(new_transpose == opstate.current_transpose[track]) {
        return;
    }

    // search for the notes that need to be transposed
    for(i = 0; i < OUTPROC_MAX_NOTES; i ++) {
        if(opstate.output_notes[track][i].status != 0) {
            // turn off existing note
            midi_utils_copy_msg(&send_msg, 
                &opstate.output_notes[track][i]);  // copy so we can modify
            // XXX check this
            send_msg.data0 = scale_quantize(send_msg.data0, opstate.current_tonality[track]);
            midi_utils_note_on_to_off(&send_msg);
            send_msg.data0 = send_msg.data0 + opstate.current_transpose[track];       
            outproc_deliver_msg(scene, track, &send_msg, 
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);

            // transpose note
            midi_utils_copy_msg(&send_msg, 
                &opstate.output_notes[track][i]);  // copy so we can modify
            send_msg.data0 = scale_quantize(send_msg.data0, opstate.current_tonality[track]);
            send_msg.data0 = send_msg.data0 + new_transpose;
            // note became invalid
            if(send_msg.data0 < 0 || send_msg.data0 > 127) {
                opstate.output_notes[track][i].status = 0;  // free the slot
                continue;
            }
            outproc_deliver_msg(scene, track, &send_msg, 
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);
        }
    }
    opstate.current_transpose[track] = new_transpose;
}

// the tonality changed on a track
void outproc_tonality_changed(int scene, int track) {
    int i, new_tonality;
    struct midi_msg send_msg;    
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("otoc - track invalid: %d", track);
        return;
    }
    if(scene != seq_engine_get_current_scene()) {
        return;
    }
    new_tonality = song_get_tonality(scene, track);
    if(outproc_get_num_notes(track) == 0) {
        opstate.current_tonality[track] = new_tonality;
        return;
    }    
    if(new_tonality == opstate.current_tonality[track]) {
        return;
    }
    
    // search for the notes that need to be turned off
    for(i = 0; i < OUTPROC_MAX_NOTES; i ++) {
        if(opstate.output_notes[track][i].status != 0) {
            // turn off existing note
            midi_utils_copy_msg(&send_msg, 
                &opstate.output_notes[track][i]);  // copy so we can modify
            // XXX check this
            send_msg.data0 = scale_quantize(send_msg.data0, opstate.current_tonality[track]);
            midi_utils_note_on_to_off(&send_msg);
            send_msg.data0 = send_msg.data0 + opstate.current_transpose[track];       
            outproc_deliver_msg(scene, track, &send_msg, 
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);
        }
    }
    opstate.current_tonality[track] = new_tonality;    
}

// deliver a track message to assigned outputs
void outproc_deliver_msg(int scene, int track, struct midi_msg *msg, 
        int deliver, int process) {
    struct midi_msg send_msg;
    int out, port, channel;

    // generate message for each output port for this track
    for(out = 0; out < SEQ_NUM_TRACK_OUTPUTS; out ++) {
        // skip ports that we don't want
        if(deliver == OUTPROC_DELIVER_A && out == 1) {
            continue;
        }
        else if(deliver == OUTPROC_DELIVER_B && out == 0) {
            continue;
        }
    
        // get port mapping
        port = song_get_midi_port_map(track, out);
        if(port == SONG_PORT_DISABLE) {
            continue;  // port not assigned
        }
        // get channel mapping
        channel = song_get_midi_channel_map(track, out);
        
        // handle event types    
        switch(msg->status & 0xf0) {
            case MIDI_NOTE_OFF:
                midi_utils_enc_note_off(&send_msg, port, channel, msg->data0, msg->data1);
                // do note processing / queue
                if(process == OUTPROC_OUTPUT_PROCESSED) {
                    // dequeue note before processing
                    outproc_dequeue_note(track, &send_msg);
                    // process note
                    PROCESS_NOTE(scene, track, send_msg);
                }
                midi_stream_send_msg(&send_msg);
                break;
            case MIDI_NOTE_ON:
                midi_utils_enc_note_on(&send_msg, port, channel, msg->data0, msg->data1);
                // do note processing / queue
                if(process == OUTPROC_OUTPUT_PROCESSED) {
                    // enqueue note before processing
                    if(outproc_enqueue_note(track, &send_msg) == -1) {
                        return;  // no free slots
                    }
                    // process note
                    PROCESS_NOTE(scene, track, send_msg);
                }
                midi_stream_send_msg(&send_msg);
                break;
            case MIDI_POLY_KEY_PRESSURE:
                midi_utils_enc_key_pressure(&send_msg, port, channel, msg->data0, msg->data1);
                // do quantizing and track transpose
                if(process == OUTPROC_OUTPUT_PROCESSED) {
                    // process note
                    PROCESS_NOTE(scene, track, send_msg);
                }
                midi_stream_send_msg(&send_msg);                
                break;
            case MIDI_CONTROL_CHANGE:
                midi_utils_enc_control_change(&send_msg, port, channel, msg->data0, msg->data1);
                midi_stream_send_msg(&send_msg);
                break;
            case MIDI_PROGRAM_CHANGE:
                midi_utils_enc_program_change(&send_msg, port, channel, msg->data0);
                midi_stream_send_msg(&send_msg);
                break;
            case MIDI_CHANNEL_PRESSURE:
                midi_utils_enc_channel_pressure(&send_msg, port, channel, msg->data0);
                midi_stream_send_msg(&send_msg);
                break;
            case MIDI_PITCH_BEND:
                midi_utils_enc_pitch_bend(&send_msg, port, channel, (msg->data0 | 
                    (msg->data1 << 7)) - 8192);
                midi_stream_send_msg(&send_msg);
                break;
            default:
                break;
        }
    }
}

// stop all notes on a track
void outproc_stop_all_notes(int track) {
    int i;
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("osan - track invalid: %d", track);
        return;
    }
    // search for the notes that need to be turned off
    for(i = 0; i < OUTPROC_MAX_NOTES; i ++) {
        if(opstate.output_notes[track][i].status != 0) {
            // turn off existing note
            outproc_deliver_msg(0, track, &opstate.output_notes[track][i], 
                OUTPROC_DELIVER_BOTH, OUTPROC_OUTPUT_RAW);
            opstate.output_notes[track][i].status = 0;  // free slot
        }
    }
}

//
// local functions
//
// queue note that is currently playing so we can modify it later
// returns -1 if there are no more slots to queue the note
int outproc_enqueue_note(int track, struct midi_msg *on_msg) {
    int i, free_slot = -1;
    // search for a free slot to put the note
    for(i = 0; i < OUTPROC_MAX_NOTES; i ++) {
        if(opstate.output_notes[track][i].status == 0) {
            free_slot = i;
            break;
        }
    }
    // no free slots - return error
    if(free_slot == -1) {
        return -1;
    }
    midi_utils_copy_msg(&opstate.output_notes[track][free_slot], on_msg);
    return 0;
}

// dequeue note that is currently playing so we can modify it later
void outproc_dequeue_note(int track, struct midi_msg *off_msg) {
    int i;
    // search for the note on that corresponds to our note off
    for(i = 0; i < OUTPROC_MAX_NOTES; i ++) {
        // check if the note on in the queue corresponds to our note off
        if(opstate.output_notes[track][i].status != 0 &&
                midi_utils_compare_note_msg(&opstate.output_notes[track][i], off_msg)) {
            opstate.output_notes[track][i].status = 0;  // free the slot
            break;
        }
    }
} 

// get the number of currently held live notes on a track
int outproc_get_num_notes(int track) {
    int i, count = 0;
    // search for live notes
    for(i = 0; i < OUTPROC_MAX_NOTES; i ++) {
        if(opstate.output_notes[track][i].status != 0) {
            count ++;
        }
    }
    return count;
}
