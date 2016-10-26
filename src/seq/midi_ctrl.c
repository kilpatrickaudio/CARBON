/*
 * CARBON MIDI Control of Playback / Sequencer State
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
 * MIDI Control Overview:
 *  - MIDI control is only active when the MIDI Rmt Ctrl option is enabled
 *  - each track can be controlled on a separate MIDI channel
 *  - channel assignments:
 *    - channel 10 - control the current track(s)
 *    - channel 11 - track 1 control
 *    - channel 12 - track 2 control
 *    - channel 13 - track 3 control
 *    - channel 14 - track 4 control
 *    - channel 15 - track 5 control
 *    - channel 16 - track 6 control
 *
 * MIDI Note Control: (works on any channel: 10-16)
 *  - trigger scene 1       - NOTE 24 (C1)
 *  - trigger scene 2       - NOTE 26 (D1)
 *  - trigger scene 3       - NOTE 28 (E1)
 *  - trigger scene 4       - NOTE 29 (F1)
 *  - trigger scene 5       - NOTE 31 (G1)
 *  - trigger scene 6       - NOTE 33 (A1)
 *  - reset track 1         - NOTE 36 (C2)
 *  - run                   - NOTE 37 (C#2)
 *  - reset track 2         - NOTE 38 (D2)
 *  - stop                  - NOTE 39 (D#2)
 *  - reset track 3         - NOTE 40 (E2)
 *  - reset track 4         - NOTE 41 (F2)
 *  - reset                 - NOTE 42 (F#2)
 *  - reset track 5         - NOTE 43 (G2)
 *  - reset track 6         - NOTE 45 (A2)
 *  - record                - NOTE 46 (A#2)
 *  - KB transpose          - NOTE 48-72 = transpose -12 to +12
 *
 * MIDI CC Control:
 *  - step length           - CC 16     - val: 0-127 = 32nd to whole
 *  - track transpose       - CC 17     - val: 0-127 = -24 to +24 (0 at val 64)
 *  - track mute            - CC 18     - val: 0-63 = off, 64-127 = on
 *  - motion start          - CC 19     - val: 0-127 = position 1-64
 *  - motion length         - CC 20     - val: 0-127 = length 1-64
 *  - motion dir            - CC 21     - val: 0-63 = off, 64-127 = on
 *  - gate time             - CC 22     - val: 0-127 = 0-200%
 *  - pattern type          - CC 23     - val: 0-127 = type 1-32
 *  - arp enable            - CC 24     - val: 0-63 = off, 64-127 = on
 *  - arp type              - CC 25     - val: 0-127 = type 1-16
 *  - arp speed             - CC 26     - val: 0-127 = 32nd to whole
 *  - arp gate time         - CC 27     - val: 0-127 = min to max
 *  - run/stop              - CC 80     - val: 0-63 = stop, 64-127 = run
 *
 */
#include "midi_ctrl.h"
#include "arp_progs.h"
#include "seq_ctrl.h"
#include "../midi/midi_protocol.h"
#include "../util/log.h"
#include "../util/seq_utils.h"

// channel defines
#define MIDI_CTRL_CHAN_OMNI 9
#define MIDI_CTRL_CHAN_TRACK_1 10
#define MIDI_CTRL_CHAN_TRACK_2 11
#define MIDI_CTRL_CHAN_TRACK_3 12
#define MIDI_CTRL_CHAN_TRACK_4 13
#define MIDI_CTRL_CHAN_TRACK_5 14
#define MIDI_CTRL_CHAN_TRACK_6 15
// note defines
#define MIDI_CTRL_NOTE_SCENE_1 24
#define MIDI_CTRL_NOTE_SCENE_2 26
#define MIDI_CTRL_NOTE_SCENE_3 28
#define MIDI_CTRL_NOTE_SCENE_4 29
#define MIDI_CTRL_NOTE_SCENE_5 31
#define MIDI_CTRL_NOTE_SCENE_6 33
#define MIDI_CTRL_NOTE_RESET_TRACK_1 36
#define MIDI_CTRL_NOTE_RUN 37
#define MIDI_CTRL_NOTE_RESET_TRACK_2 38
#define MIDI_CTRL_NOTE_STOP 39
#define MIDI_CTRL_NOTE_RESET_TRACK_3 40
#define MIDI_CTRL_NOTE_RESET_TRACK_4 41
#define MIDI_CTRL_NOTE_RESET 42
#define MIDI_CTRL_NOTE_RESET_TRACK_5 43
#define MIDI_CTRL_NOTE_RESET_TRACK_6 45
#define MIDI_CTRL_NOTE_RECORD 46
#define MIDI_CTRL_NOTE_KBTRANS_MIN 48
#define MIDI_CTRL_NOTE_KBTRANS_MAX 72
#define MIDI_CTRL_NOTE_KBTRANS_OFFSET 60
// CC defines
#define MIDI_CTRL_CC_STEP_LENGTH 16
#define MIDI_CTRL_CC_TRACK_TRANSPOSE 17
#define MIDI_CTRL_CC_TRACK_MUTE 18
#define MIDI_CTRL_CC_MOTION_START 19
#define MIDI_CTRL_CC_MOTION_LENGTH 20
#define MIDI_CTRL_CC_MOTION_DIR 21
#define MIDI_CTRL_CC_GATE_TIME 22
#define MIDI_CTRL_CC_PATTERN_TYPE 23
#define MIDI_CTRL_CC_ARP_ENABLE 24
#define MIDI_CTRL_CC_ARP_TYPE 25
#define MIDI_CTRL_CC_ARP_SPEED 26
#define MIDI_CTRL_CC_ARP_GATE_TIME 27
#define MIDI_CTRL_CC_RUN_STOP 80

// init the MIDI control module
void midi_ctrl_init(void) {

}

// handle a MIDI message from the keyboard / MIDI input
void midi_ctrl_handle_midi_msg(struct midi_msg *msg) {
    int stat = msg->status & 0xf0;
    int chan = msg->status & 0x0f;
    int track;

    // handle system common messages first
    switch(msg->status) {
        case MIDI_SONG_SELECT:
            // only allow the song numbers we support
            if(msg->data0 > (SEQ_NUM_SONGS - 1)) {
                return;
            }
            seq_ctrl_load_song(msg->data0);
            return;
        default:  // fall through to channel handling
            break;
    }

    // ignore stuff we don't care about
    if(!(stat == MIDI_NOTE_ON || stat == MIDI_CONTROL_CHANGE)) {
        return;
    }

    // ignore channels we don't care about
    if(chan < MIDI_CTRL_CHAN_OMNI) {
        return;
    }

    // note control
    if(stat == MIDI_NOTE_ON) {
        // ignore note on with velocity of 0 (note off)
        if(msg->data1 == 0) {
            return;
        }
        switch(msg->data0) {
            case MIDI_CTRL_NOTE_SCENE_1:  // trigger scene 1
                seq_ctrl_set_scene(0);
                break;
            case MIDI_CTRL_NOTE_SCENE_2:  // trigger scene 2
                seq_ctrl_set_scene(1);
                break;
            case MIDI_CTRL_NOTE_SCENE_3:  // trigger scene 3
                seq_ctrl_set_scene(2);
                break;
            case MIDI_CTRL_NOTE_SCENE_4:  // trigger scene 4
                seq_ctrl_set_scene(3);
                break;
            case MIDI_CTRL_NOTE_SCENE_5:  // trigger scene 5
                seq_ctrl_set_scene(4);
                break;
            case MIDI_CTRL_NOTE_SCENE_6:  // trigger scene 6
                seq_ctrl_set_scene(5);
                break;
            case MIDI_CTRL_NOTE_RESET_TRACK_1:
                seq_ctrl_reset_track(0);
                break;
            case MIDI_CTRL_NOTE_RUN:
                seq_ctrl_set_run_state(1);
                break;
            case MIDI_CTRL_NOTE_RESET_TRACK_2:
                seq_ctrl_reset_track(1);
                break;
            case MIDI_CTRL_NOTE_STOP:
                seq_ctrl_set_run_state(0);
                break;
            case MIDI_CTRL_NOTE_RESET_TRACK_3:
                seq_ctrl_reset_track(2);
                break;
            case MIDI_CTRL_NOTE_RESET_TRACK_4:
                seq_ctrl_reset_track(3);
                break;
            case MIDI_CTRL_NOTE_RESET:
                seq_ctrl_reset_pos();
                break;
            case MIDI_CTRL_NOTE_RESET_TRACK_5:
                seq_ctrl_reset_track(4);
                break;
            case MIDI_CTRL_NOTE_RESET_TRACK_6:
                seq_ctrl_reset_track(5);
                break;
            case MIDI_CTRL_NOTE_RECORD:
                seq_ctrl_record_pressed();
                break;
            default:
                if(msg->data0 >= MIDI_CTRL_NOTE_KBTRANS_MIN &&
                        msg->data0 <= MIDI_CTRL_NOTE_KBTRANS_MAX) {
                    seq_ctrl_set_kbtrans(msg->data0 - MIDI_CTRL_NOTE_KBTRANS_OFFSET);
                }
                break;
        }
    }
    // CC control
    else if(stat == MIDI_CONTROL_CHANGE) {
        // figure out track
        if(chan == MIDI_CTRL_CHAN_OMNI) {
            track = SEQ_CTRL_TRACK_OMNI;
        }
        else {
            track = chan - MIDI_CTRL_CHAN_TRACK_1;
        }

        switch(msg->data0) {
            case MIDI_CTRL_CC_STEP_LENGTH:
                seq_ctrl_set_step_length(track, 
                    seq_utils_clamp((msg->data1 >> 3), 
                    0, SEQ_UTILS_STEP_LENS - 1));
                break;
            case MIDI_CTRL_CC_TRACK_TRANSPOSE:
                seq_ctrl_set_transpose(track,
                    seq_utils_clamp((msg->data1 >> 1) - 32,
                    SEQ_TRANSPOSE_MIN, SEQ_TRANSPOSE_MAX));
                break;
            case MIDI_CTRL_CC_TRACK_MUTE:
                seq_ctrl_set_mute_select(track, msg->data1 >> 6);
                break;
            case MIDI_CTRL_CC_MOTION_START:
                seq_ctrl_set_motion_start(track,
                    seq_utils_clamp((msg->data1 >> 1),
                    0, SEQ_NUM_STEPS - 1));
                break;
            case MIDI_CTRL_CC_MOTION_LENGTH:
                seq_ctrl_set_motion_length(track,
                    seq_utils_clamp((msg->data1 >> 1) + 1,
                    1, SEQ_NUM_STEPS));
                break;
            case MIDI_CTRL_CC_MOTION_DIR:
                seq_ctrl_set_motion_dir(track, msg->data1 >> 6);
                break;
            case MIDI_CTRL_CC_GATE_TIME:
                seq_ctrl_set_gate_time(track, (msg->data1 << 1) + 1);
                break;
            case MIDI_CTRL_CC_PATTERN_TYPE:
                seq_ctrl_set_pattern_type(track, msg->data1 >> 2);
                break;
            case MIDI_CTRL_CC_ARP_ENABLE:
                seq_ctrl_set_arp_enable(track, msg->data1 >> 6);
                break;
            case MIDI_CTRL_CC_ARP_TYPE:
                seq_ctrl_set_arp_type(track,
                    seq_utils_clamp(msg->data1 >> 3,
                    0, ARP_NUM_TYPES - 1));
                break;
            case MIDI_CTRL_CC_ARP_SPEED:
                seq_ctrl_set_arp_speed(track, 
                    seq_utils_clamp((msg->data1 >> 3), 
                    0, SEQ_UTILS_STEP_LENS - 1));
                break;
            case MIDI_CTRL_CC_ARP_GATE_TIME:
                seq_ctrl_set_arp_gate_time(track,
                    seq_utils_clamp((msg->data1 << 2) + 1, 
                    ARP_GATE_TIME_MIN, ARP_GATE_TIME_MAX));
                break;
            case MIDI_CTRL_CC_RUN_STOP:
                seq_ctrl_set_run_state(msg->data1 >> 6);
                break;
        }
    }
}

