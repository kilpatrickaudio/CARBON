/*
 * CARBON State Change Event Types
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2016: Kilpatrick Audio
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
#ifndef STATE_CHANGE_EVENTS_H
#define STATE_CHANGE_EVENTS_H

// event class types
enum STATE_CHANGE_EVENT_CLASS {
    SCEC_SONG = 0x010000,
    SCEC_CTRL = 0x020000,
    SCEC_ENG = 0x030000,
    SCEC_CONFIG = 0x040000,
    SCEC_POWER = 0x050000
};

// event types
enum STATE_CHANGE_EVENT {
    // song events
    SCE_SONG_CLEARED = SCEC_SONG,  // arg0 = song_num
    SCE_SONG_LOADED,  // arg0 = song_num
    SCE_SONG_LOAD_ERROR,  // arg0 = song_num
    SCE_SONG_SAVED,  // arg0 = song_num
    SCE_SONG_SAVE_ERROR,  // arg0 = song_num
    SCE_SONG_TEMPO,  // no args - need to get due to float type
    SCE_SONG_SWING,  // arg0 = swing
    SCE_SONG_METRONOME_MODE,  // arg0 = mode
    SCE_SONG_METRONOME_SOUND_LEN,  // arg0 = len
    SCE_SONG_KEY_VELOCITY_SCALE,  // arg0 = scale
    SCE_SONG_CV_BEND_RANGE,  // arg0 = bend range
    SCE_SONG_CV_GATE_PAIRS,  // arg0 = CV/gate pairs
    SCE_SONG_CV_GATE_PAIR_MODE,  // arg0 = CV/gate pair, arg1 = mode
    SCE_SONG_CV_OUTPUT_SCALING,  // arg0 = CV output, arg1 = mode
    SCE_SONG_CVCAL,  // arg0 = channel, arg1 = cal
    SCE_SONG_CVOFFSET,  // arg0 = channel, arg1 = offset
    SCE_SONG_MIDI_PORT_CLOCK_OUT,  // arg0 = port, arg1 = ppq
    SCE_SONG_MIDI_CLOCK_SOURCE,  // arg0 = source
    SCE_SONG_MIDI_REMOTE_CTRL,  // arg0 = enable
    SCE_SONG_MIDI_AUTOLIVE,  // arg0 = enable
    SCE_SONG_LIST_SCENE,  // arg0 = entry, arg1 = scene
    SCE_SONG_LIST_LENGTH,  // arg0 = entry, arg1 = length
    SCE_SONG_LIST_KBTRANS,  // arg0 = entry, arg1 = kbtrans
    SCE_SONG_MIDI_PROGRAM,  // arg0 = track, arg1 = mapnum, arg2 = program
    SCE_SONG_MIDI_PORT_MAP,  // arg0 = track, arg1 = mapnum, arg3 = port
    SCE_SONG_MIDI_CHANNEL_MAP,  // arg0 = track, arg1 = mapnum, arg2 = channel
    SCE_SONG_KEY_SPLIT,  // arg0 = track, arg1 = mode
    SCE_SONG_TRACK_TYPE,  // arg0 = track, arg1 = mode
    SCE_SONG_STEP_LEN,  // arg0 = scene, arg1 = track, arg2 = length
    SCE_SONG_TONALITY,  // arg0 = scene, arg1 = track, arg2 = tonality
    SCE_SONG_TRANSPOSE,  // arg0 = scene, arg1 = track, arg2 = transpose
    SCE_SONG_BIAS_TRACK,  // arg0 = scene, arg1 = track, arg2 = bias track
    SCE_SONG_MOTION_START,  // arg0 = scene, arg1 = track, arg2 = start
    SCE_SONG_MOTION_LENGTH,  // arg0 = scene, arg1 = track, arg2 = length
    SCE_SONG_GATE_TIME,  // arg0 = scene, arg1 = track, arg2 = time
    SCE_SONG_PATTERN_TYPE,  // arg0 = scene, arg1 = track, arg2 = pattern
    SCE_SONG_MOTION_DIR,  // arg0 = scene, arg1 = track, arg3 = reverse
    SCE_SONG_MUTE,  // arg0 = scene, arg1 = track, arg2 = mute
    SCE_SONG_ARP_TYPE,  // arg0 = scene, arg1 = track, arg2 = type
    SCE_SONG_ARP_SPEED,  // arg0 = scene, arg1 = track, arg2 = speed
    SCE_SONG_ARP_GATE_TIME,  // arg0 = scene, arg1 = track, arg2 = time
    SCE_SONG_ARP_ENABLE,  // arg0 = scene, arg1 = track, arg2 = enable
    SCE_SONG_CLEAR_STEP,  // arg0 = scene, arg1 = track, arg2 = step
    SCE_SONG_CLEAR_STEP_EVENT,  // arg0 = scene, arg1 = track, arg2 = step
    SCE_SONG_ADD_STEP_EVENT,  // arg0 = scene, arg1 = track, arg2 = step
    SCE_SONG_SET_STEP_EVENT,  // arg0 = scene, arg1 = track, arg2 = step
    SCE_SONG_START_DELAY,  // arg0 = scene, arg1 = track, arg2 = step
    SCE_SONG_RATCHET_MODE,  // arg0 = scene, arg1 = track, arg1 = step
    // control events
    SCE_CTRL_RUN_STATE = SCEC_CTRL,  // arg0 = state
    SCE_CTRL_TRACK_SELECT,  // arg0 = track, arg1 = select
    SCE_CTRL_FIRST_TRACK,  // arg0 = track
    SCE_CTRL_SONG_MODE,  // arg0 = song mode
    SCE_CTRL_LIVE_MODE,  // arg0 = live mode
    SCE_CTRL_RECORD_MODE,  // arg0 = record mode
    SCE_CTRL_CLOCK_BEAT,  // no args
    SCE_CTRL_EXT_TEMPO,  // no args
    SCE_CTRL_EXT_SYNC,  // arg0: ext synced
    // engine events
    SCE_ENG_CURRENT_SCENE = SCEC_ENG,  // arg0 = scene
    SCE_ENG_ACTIVE_STEP,  // arg0 = track, arg1 = step
    SCE_ENG_SONG_MODE_STATUS,  // arg0 = none
    SCE_ENG_KBTRANS,  // arg0 = trans
    // config events
    SCE_CONFIG_LOADED = SCEC_CONFIG,  // no args
    SCE_CONFIG_CLEARED,  // no args
    // power events
    SCE_POWER_STATE = SCEC_POWER  // arg0 = state
};

#endif

