/*
 * CARBON Song Management
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
#include "song.h"
#include "arp.h"
#include "scale.h"
#include "../config.h"
#include "../ext_flash.h"
#include "../util/log.h"
#include "../util/seq_utils.h"
#include "../util/state_change.h"
#include "../util/state_change_events.h"

// list of notes to reset steps to on init
uint8_t song_reset_scale[] = {
    60, 62, 64, 65, 67, 69, 71, 72
};

// global params for track
struct track_param {
    // MIDI
    int8_t midi_program[SEQ_NUM_TRACK_OUTPUTS];  // midi_program for each output
    int8_t midi_output_port[SEQ_NUM_TRACK_OUTPUTS];  // port number for each output
    int8_t midi_output_chan[SEQ_NUM_TRACK_OUTPUTS];  // channel number for each output
    int8_t midi_key_split;  // 0 = off, 1 = left, 2 = right, 3 = dual
    int8_t track_type;  // 0 = voice, 1 = drum
};

// step modification params
struct track_step_param {
    uint8_t start_delay;  // the number of ticks before step start
    uint8_t ratchet;  // the number of notes to play on a step - 1-4
};

// scene data for track
struct track_scene {
    // clock
    int8_t step_len;   // step length - lookup
    // tonality
    int8_t tonality;  // see types defined
    int8_t transpose;  // -24 to +24 semitones
    int8_t bias_track;  // -1 = disable, 0-5 = track (track cannot equal current track)
    // track setting
    int8_t motion_start;  // 0-63 = start
    int8_t motion_len;  // 1-64 = length
    uint8_t gate_time;  // length override - 0-255 = 0-200%
    int8_t pattern_type;  // 0 = off, 1-30 = preset, 31 = as recorded
    int8_t dir_reverse;  // 0 = normal, 1 = reverse
    int8_t mute;  // 0 = normal, 1 = muted
    // arp params
    int8_t arp_speed;  // arp speed - lookup
    int8_t arp_type;  // arp type - lookup
    int16_t arp_gate_time;  // arp gate time - ticks
    int16_t arp_enable;  // 0 = disable, 1 = enable
};

// song list entry data
struct song_list_entry {
    int8_t scene;  // the scene to play
    int16_t length_beats;  // number of beats to play - -1 = NULL, 1-256 = 1-256 beats
    int8_t kbtrans;  // keyboard transpose for playback
};

// a complete song
struct song_data {
    // firwmare version that wrote this song - this should be first
    uint32_t song_version;  // version major: upper 16 bits, version mintor: lower 16 bits
    // global
    float tempo;  // song tempo in BPM
    int8_t swing;  // track swing - 50-80 = 50-80%
    int8_t metronome;  // 0 = off, 1 = internal, 24-84 = track 6 note
    int16_t midi_key_vel_scale;  // -100 to +100
    int8_t cv_bend_range;  // number of semis to bend
    int8_t cvgate_pairs;  // see values
    int8_t cvgate_pair_mode[SONG_CVGATE_NUM_PAIRS];  // see values
    int8_t cv_output_scaling[SONG_CVGATE_NUM_OUTPUTS];  // see value
    int8_t cvcal[SONG_CVGATE_NUM_OUTPUTS];  // cal value used to init the CV scale
    int midi_clock_out[MIDI_PORT_NUM_TRACK_OUTPUTS];  // PPQ setting
    // song list
    struct song_list_entry snglist[SEQ_SONG_LIST_ENTRIES];  // song list
    // track/scene params
    struct track_param trkparam[SEQ_NUM_TRACKS];  // params for tracks
    struct track_scene trkscene[SEQ_NUM_SCENES][SEQ_NUM_TRACKS];  // scene data for tracks
#ifdef SONG_NOTES_PER_SCENE
#warning song compiled with notes per scene
    struct track_step_param trkstepparam[SEQ_NUM_TRACKS][SEQ_NUM_SCENES][SEQ_NUM_STEPS];  // step params
    struct track_event trkevents[SEQ_NUM_TRACKS][SEQ_NUM_SCENES][SEQ_NUM_STEPS][SEQ_TRACK_POLY];  // events
#else
#warning song compiled with notes per song
    struct track_step_param trkstepparam[SEQ_NUM_TRACKS][SEQ_NUM_STEPS];  // step params
    struct track_event trkevents[SEQ_NUM_TRACKS][SEQ_NUM_STEPS][SEQ_TRACK_POLY];  // events
#endif
    //
    // additional data (from original song layout
    //  - do not move this up or you will break existing songs
    //  - new data here must be checked against the song version number
    //
    // CARBON version 1.01
    uint8_t midi_remote_ctrl;  // MIDI remote control - 0 = disabled, 1 = enabled
    // CARBON version 1.03
    uint8_t metronome_sound_len;  // metronome sound length in ms
    int8_t midi_clock_source;  // see lookup table
    uint8_t dummy999[2];  // DO NOT REMOVE THIS - fills in old MIDI clock in space
    // CARBON version 1.12
    int16_t cvoffset[SONG_CVGATE_NUM_OUTPUTS];  // CV output offset
    uint8_t midi_autolive;  // autolive function - 0 = off, 1 = on
    // CARBON version 1.14
    uint8_t scene_sync;  // scene sync type - 0 = beat, 1 = track 1 end
    uint8_t magic_range;  // magic seed range in semitones
    uint8_t magic_chance;  // magic change amount in percent

    // dummy padding - to make it an even number of 4096 byte sectors in the flash
    // - be VERY careful that this is correct or other RAM could be overwritten
#ifdef SONG_NOTES_PER_SCENE
    uint8_t dummy0[1024];
    uint8_t dummy1[473];
#else
    uint8_t dummy0[1024];
    uint8_t dummy1[1024];
    uint8_t dummy2[1024];
    uint8_t dummy3[1024];
    uint8_t dummy4[700];
    uint8_t dummy5[13];
#endif
    // token to identify correct loading of file
    uint32_t magic_num;
};
struct song_data song;

// state for stuff that isn't saved in the song
#define SONG_IO_STATE_IDLE 0
#define SONG_IO_STATE_LOAD 1
#define SONG_IO_STATE_SAVE 2
struct song_state {
    int state;  // flag to indicate if we are loading or saving
    int loadsave_song;  // which song number is loading or saving
};
struct song_state songs;

// init the song
void song_init(void) {
    songs.state = SONG_IO_STATE_IDLE;
    songs.loadsave_song = 0;
    song_clear();
}

// run the task to take care of loading or saving
void song_timer_task(void) {
    if(songs.state == SONG_IO_STATE_IDLE) {
        return;
    }
    // check the flash to see if we're done
    switch(ext_flash_get_state()) {
        case EXT_FLASH_STATE_LOAD:
            // load in progress
            break;
        case EXT_FLASH_STATE_LOAD_ERROR:
            songs.state = SONG_IO_STATE_IDLE;
            state_change_fire1(SCE_SONG_LOAD_ERROR, songs.loadsave_song);
            song_clear();  // clear the song instead
            break;
        case EXT_FLASH_STATE_LOAD_DONE:
            songs.state = SONG_IO_STATE_IDLE;
            // check magic number to make sure we loaded correctly
            if(song.magic_num != SONG_MAGIC_NUM) {
                song_clear();  // clear the song instead
                state_change_fire1(SCE_SONG_LOAD_ERROR, songs.loadsave_song);
            }
            else {
                state_change_fire1(SCE_SONG_LOADED, songs.loadsave_song);
            }
            break;
        case EXT_FLASH_STATE_SAVE:
            // save in progress
            break;
        case EXT_FLASH_STATE_SAVE_ERROR:
            songs.state = SONG_IO_STATE_IDLE;
            state_change_fire1(SCE_SONG_SAVE_ERROR, songs.loadsave_song);
            break;
        case EXT_FLASH_STATE_SAVE_DONE:
            songs.state = SONG_IO_STATE_IDLE;
            state_change_fire1(SCE_SONG_SAVED, songs.loadsave_song);
            break;
        default:
            songs.state = SONG_IO_STATE_IDLE;
            log_error("stt - idle state found");
            break;
    }
}

// clear the song in RAM back to defaults
void song_clear(void) {
    int scene, track, step, mapnum, port, i;
    struct track_event event;

    //
    // reset global stuff
    //
    // global params (per song)
    song_set_tempo(120.0);
    song_set_swing(50);
    song_set_metronome_mode(SONG_METRONOME_INTERNAL);
    song_set_metronome_sound_len(METRONOME_SOUND_LENGTH_DEFAULT);
    song_set_key_velocity_scale(0);
    song_set_cv_bend_range(2);
    song_set_cvgate_pairs(SONG_CVGATE_PAIR_ABCD);
    song_set_cvgate_pair_mode(0, SONG_CVGATE_MODE_NOTE);
    song_set_cvgate_pair_mode(1, SONG_CVGATE_MODE_NOTE);
    song_set_cvgate_pair_mode(2, SONG_CVGATE_MODE_NOTE);
    song_set_cvgate_pair_mode(3, SONG_CVGATE_MODE_NOTE);
    song_set_cv_output_scaling(0, SONG_CV_SCALING_1VOCT);
    song_set_cv_output_scaling(1, SONG_CV_SCALING_1VOCT);
    song_set_cv_output_scaling(2, SONG_CV_SCALING_1VOCT);
    song_set_cv_output_scaling(3, SONG_CV_SCALING_1VOCT);
    for(i = 0; i < SONG_CVGATE_NUM_OUTPUTS; i ++) {
        song_set_cvcal(i, 0);  // no cal
        song_set_cvoffset(i, 0);  // no offset
    }
    for(port = 0; port < MIDI_PORT_NUM_TRACK_OUTPUTS; port ++) {
        song_set_midi_port_clock_out(port, SEQ_UTILS_CLOCK_OFF);
    }
    for(port = 0; port < MIDI_PORT_NUM_INPUTS; port ++) {
        song_set_midi_port_clock_out(port, 0);  // disable
    }
    song_set_midi_clock_source(SONG_MIDI_CLOCK_SOURCE_INT);
    song_set_midi_remote_ctrl(0);  // disable
    song_set_midi_autolive(1);  // enable on new songs
    song_set_scene_sync(SONG_SCENE_SYNC_BEAT);  // default to previous behaviour
    song_set_magic_range(12);  // +/- 1 octave
    song_set_magic_chance(100);  // 100% chance

    // song list
    for(i = 0; i < SEQ_SONG_LIST_ENTRIES; i ++) {
        // default stuff to null
        song.snglist[i].scene = SONG_LIST_SCENE_NULL;
        song.snglist[i].length_beats = SEQ_SONG_LIST_DEFAULT_LENGTH;
        song.snglist[i].kbtrans = SEQ_SONG_LIST_DEFAULT_KBTRANS;
    }

    // track params (per track)
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        // default all outs
        for(mapnum = 0; mapnum < SEQ_NUM_TRACK_OUTPUTS; mapnum ++) {
            song_set_midi_program(track, mapnum, SONG_MIDI_PROG_NULL);
            song_set_midi_port_map(track, mapnum, MIDI_PORT_DIN1_OUT);
            song_set_midi_channel_map(track, mapnum, track);
        }
        // disable the second port
        song_set_midi_port_map(track, 1, SONG_PORT_DISABLE);
        song_set_key_split(track, SONG_KEY_SPLIT_OFF);
        song_set_track_type(track, SONG_TRACK_TYPE_VOICE);
    }

    // track params (per scene)
    for(scene = 0; scene < SEQ_NUM_SCENES; scene ++) {
        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            song_set_step_length(scene, track, SEQ_UTILS_STEP_16TH);  // 16th
            song_set_tonality(scene, track, SCALE_CHROMATIC);  // all notes
            song_set_transpose(scene, track, 0);  // no transpose
            song_set_bias_track(scene, track, SONG_TRACK_BIAS_NULL);  // no bias
            song_set_motion_start(scene, track, 0);  // step 1
            song_set_motion_length(scene, track, SEQ_NUM_STEPS);  // total setsp
            song_set_gate_time(scene, track, 0x80);  // 0x80 = 100%
            song_set_pattern_type(scene, track, 31);  // as recorded
            song_set_motion_dir(scene, track, 0);  // normal
            song_set_mute(scene, track, 0);  // normal
            song_set_arp_type(scene, track, ARP_TYPE_UP1);  // first selection
            song_set_arp_speed(scene, track, SEQ_UTILS_STEP_16TH);  // default
            song_set_arp_gate_time(scene, track,
                seq_utils_step_len_to_ticks(song_get_arp_speed(scene,
                track)) >> 1);  // step length / 2
            song_set_arp_enable(scene, track, 0);  // disable arp
        }
    }

    // reset track events
    for(scene = 0; scene < SEQ_NUM_SCENES; scene ++) {
        for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
            // put some notes in the tracks
            for(step = 0; step < SEQ_NUM_STEPS; step ++) {
                song_clear_step(scene, track, step);
                event.type = SONG_EVENT_NOTE;
                event.data0 = song_reset_scale[step % 8];  // seed with notes
                event.data1 = 0x60;  // velocity
                event.length = 20;  // length in ticks
                song_add_step_event(scene, track, step, &event);
                // clear step modes
                song_set_ratchet_mode(scene, track, step, SEQ_RATCHET_MIN);
                song_set_start_delay(scene, track, step, 0);
            }
        }
    }
    song_set_version_to_current();
    song.magic_num = SONG_MAGIC_NUM;

    // XXX debug
//    log_debug("sc - song len: %d", sizeof(struct song_data));

    // fire event
    state_change_fire1(SCE_SONG_CLEARED, songs.loadsave_song);
}

// load a song from flash memory to RAM - returns -1 on error
// change events are not generated for each item loaded
// it is up to the system to use the SCE_SONG_LOADED event
// to update whatever state they may need
int song_load(int song_num) {
    if(song_num < 0 || song_num > (SEQ_NUM_SONGS - 1)) {
        log_error("sl - song_num invalid: %d", song_num);
        return -1;
    }
    if(ext_flash_load(EXT_FLASH_SONG_OFFSET + (EXT_FLASH_SONG_SIZE * song_num),
            EXT_FLASH_SONG_SIZE, (uint8_t *)&song) == -1) {
        log_error("sl - song load start error");
        return -1;
    }
    songs.loadsave_song = song_num;  // which song we are loading
    songs.state = SONG_IO_STATE_LOAD;
    return 0;
}

// save the current song to flash mem from RAM - returns -1 on error
int song_save(int song_num) {
    if(song_num < 0 || song_num > (SEQ_NUM_SONGS - 1)) {
        log_error("ss - song_num invalid: %d", song_num);
        return -1;
    }
    if(ext_flash_save(EXT_FLASH_SONG_OFFSET + (EXT_FLASH_SONG_SIZE * song_num),
            EXT_FLASH_SONG_SIZE, (uint8_t *)&song) == -1) {
        log_error("ss - song save start error");
        return -1;
    }
    songs.loadsave_song = song_num;  // which song we are loading
    songs.state = SONG_IO_STATE_SAVE;
    return 0;
}

// copy a scene to another scene replacing all data
void song_copy_scene(int dest, int src) {
    int track;
    if(dest < 0 || src >= SEQ_NUM_SCENES) {
        log_error("scs - dest invalid: %d", dest);
        return;
    }
    if(src < 0 || src >= SEQ_NUM_SCENES) {
        log_error("scs - src invalid: %d", src);
        return;
    }
    // copy all trkscene items
    for(track = 0; track < SEQ_NUM_TRACKS; track ++) {
        song_set_step_length(dest, track, song_get_step_length(src, track));
        song_set_tonality(dest, track, song_get_tonality(src, track));
        song_set_transpose(dest, track, song_get_transpose(src, track));
        song_set_bias_track(dest, track, song_get_bias_track(src, track));
        song_set_motion_start(dest, track, song_get_motion_start(src, track));
        song_set_motion_length(dest, track, song_get_motion_length(src, track));
        song_set_gate_time(dest, track, song_get_gate_time(src, track));
        song_set_pattern_type(dest, track, song_get_pattern_type(src, track));
        song_set_motion_dir(dest, track, song_get_motion_dir(src, track));
        song_set_mute(dest, track, song_get_mute(src, track));
        song_set_arp_speed(dest, track, song_get_arp_speed(src, track));
        song_set_arp_type(dest, track, song_get_arp_type(src, track));
        song_set_arp_gate_time(dest, track, song_get_arp_gate_time(src, track));
        song_set_arp_enable(dest, track, song_get_arp_enable(src, track));
    }
}

//
// getters and setters
//
//
// global params (per song)
//
// get the song version
// - version major: upper 16 bits
// - version minor: lower 16 bits
uint32_t song_get_song_version(void) {
    return song.song_version;
}

// reset the song version to current version
void song_set_version_to_current(void) {
    song.song_version = CARBON_VERSION_MAJMIN;
}

// get the song tempo
float song_get_tempo(void) {
    return song.tempo;
}

// set the song tempo
void song_set_tempo(float tempo) {
    if(tempo < MIDI_CLOCK_TEMPO_MIN || tempo > (MIDI_CLOCK_TEMPO_MAX + 0.1)) {
        log_error("sst - tempo invalid: %f", tempo);
        return;
    }
    song.tempo = tempo;
    // fire event
    state_change_fire0(SCE_SONG_TEMPO);
}

// get the swing
int song_get_swing(void) {
    return song.swing;
}

// set the swing
void song_set_swing(int swing) {
    if(swing < SEQ_SWING_MIN || swing > SEQ_SWING_MAX) {
        log_error("ssw - swing invalid: %d", swing);
        return;
    }
    song.swing = swing;
    // fire event
    state_change_fire1(SCE_SONG_SWING, swing);
}

// get the metronome mode
int song_get_metronome_mode(void) {
    return song.metronome;
}

// set the metronome mode
void song_set_metronome_mode(int mode) {
    if(mode < 0 || mode > SONG_METRONOME_NOTE_HIGH ||
        (mode > SONG_METRONOME_CV_RESET && mode < SONG_METRONOME_NOTE_LOW)) {
        log_error("ssm - mode invalid: %d", mode);
        return;
    }
    song.metronome = mode;
    // fire event
    state_change_fire1(SCE_SONG_METRONOME_MODE, mode);
}

// get the metronome sound length
int song_get_metronome_sound_len(void) {
    return song.metronome_sound_len;
}

// set the metronome sound length
void song_set_metronome_sound_len(int len) {
    if(len < METRONOME_SOUND_LENGTH_MIN || len > METRONOME_SOUND_LENGTH_MAX) {
        log_error("ssmsl - len invalid: %d", len);
        return;
    }
    song.metronome_sound_len = len;
    // fire event
    state_change_fire1(SCE_SONG_METRONOME_SOUND_LEN, len);
}

// get the MIDI input key velocity scaling
int song_get_key_velocity_scale(void) {
    return song.midi_key_vel_scale;
}

// set the MIDI input key velocity scaling
void song_set_key_velocity_scale(int velocity) {
    if(velocity < SEQ_KEY_VEL_SCALE_MIN ||
            velocity > SEQ_KEY_VEL_SCALE_MAX) {
        log_error("sskv - velocity invalid: %d", velocity);
        return;
    }
    song.midi_key_vel_scale = velocity;
    // fire event
    state_change_fire1(SCE_SONG_KEY_VELOCITY_SCALE, velocity);
}

// get the CV bend range
int song_get_cv_bend_range(void) {
    return song.cv_bend_range;
}

// set the CV bend range
void song_set_cv_bend_range(int semis) {
    if(semis < CVPROC_BEND_RANGE_MIN || semis > CVPROC_BEND_RANGE_MAX) {
        log_error("sscbr - semis invalid: %d", semis);
        return;
    }
    song.cv_bend_range = semis;
    // fire event
    state_change_fire1(SCE_SONG_CV_BEND_RANGE, semis);
}

// get the CV/gate channel pairings
int song_get_cvgate_pairs(void) {
    return song.cvgate_pairs;
}

// set the CV/gate channel pairings
void song_set_cvgate_pairs(int pairs) {
    if(pairs < 0 || pairs >= SONG_CVGATE_NUM_PAIRS) {
        log_error("ssccp - pairs invalid: %d", pairs);
        return;
    }
    song.cvgate_pairs = pairs;
    // fire event
    state_change_fire1(SCE_SONG_CV_GATE_PAIRS, pairs);
}

// get the CV/gate pair mode for an output (0-3 = A-D)
int song_get_cvgate_pair_mode(int pair) {
    if(pair < 0 || pair >= SONG_CVGATE_NUM_PAIRS) {
        log_error("sgcpm - pair invalid: %d", pair);
        return -2;  // -1 is reserved for note
    }
    return song.cvgate_pair_mode[pair];
}

// set the CV/gate pair mode for an output (0-3 = A-D)
void song_set_cvgate_pair_mode(int pair, int mode) {
    if(pair < 0 || pair >= SONG_CVGATE_NUM_PAIRS) {
        log_error("sscpm - pair invalid: %d", pair);
        return;
    }
    if(mode < SONG_CVGATE_MODE_VELO || mode > SONG_CVGATE_MODE_MAX) {
        log_error("sscpm - mode invalid: %d", mode);
        return;
    }
    song.cvgate_pair_mode[pair] = mode;
    // fire event
    state_change_fire2(SCE_SONG_CV_GATE_PAIR_MODE, pair, mode);
}

// get the CV output scaling for an output
int song_get_cv_output_scaling(int out) {
    if(out < 0 || out >= SONG_CVGATE_NUM_OUTPUTS) {
        log_error("sgcos - out invalid: %d", out);
        return -1;
    }
    return song.cv_output_scaling[out];
}

// set the CV output scaling for an output
void song_set_cv_output_scaling(int out, int mode) {
    if(out < 0 || out >= SONG_CVGATE_NUM_OUTPUTS) {
        log_error("sscos - out invalid: %d", out);
        return;
    }
    if(mode < 0 || mode > SONG_CV_SCALING_MAX) {
        log_error("sscos - mode invalid: %d", mode);
        return;
    }
    song.cv_output_scaling[out] = mode;
    // fire event
    state_change_fire2(SCE_SONG_CV_OUTPUT_SCALING, out, mode);
}

// get the CV calibration value for an output - returns -1 on error
int song_get_cvcal(int out) {
    if(out < 0 || out >= SONG_CVGATE_NUM_OUTPUTS) {
        log_error("sgcc - out invalid: %d", out);
        return -1;
    }
    return song.cvcal[out];
}

// set the CV calibration value for an output
void song_set_cvcal(int out, int val) {
    if(out < 0 || out >= SONG_CVGATE_NUM_OUTPUTS) {
        log_error("sscc - out invalid: %d", out);
        return;
    }
    if(val < CVPROC_CVCAL_MIN || val > CVPROC_CVCAL_MAX) {
        log_error("sscc - val invalid: %d", val);
        return;
    }
    song.cvcal[out] = val;
    // fire event
    state_change_fire2(SCE_SONG_CVCAL, out, val);
}

// get the CV offset for an output
int song_get_cvoffset(int out) {
    if(out < 0 || out >= SONG_CVGATE_NUM_OUTPUTS) {
        log_error("sgco - out invalid: %d", out);
        return -1;
    }
    return song.cvoffset[out];
}

// set the CV offset for an output
void song_set_cvoffset(int out, int offset) {
    if(out < 0 || out >= SONG_CVGATE_NUM_OUTPUTS) {
        log_error("ssco - out invalid: %d", out);
        return;
    }
    if(offset < CVPROC_CVOFFSET_MIN || offset > CVPROC_CVOFFSET_MAX) {
        log_error("ssco - offset invalid: %d", offset);
        return;
    }
    song.cvoffset[out] = offset;
    // fire event
    state_change_fire2(SCE_SONG_CVOFFSET, out, offset);
}

// get a MIDI port clock out enable setting - returns -1 on error
int song_get_midi_port_clock_out(int port) {
    if(port < 0 || port >= MIDI_PORT_NUM_TRACK_OUTPUTS) {
        log_error("sgmpco - port invalid: %d", port);
        return -1;
    }
    return song.midi_clock_out[port];
}

// set a MIDI port clock out enable setting
void song_set_midi_port_clock_out(int port, int ppq) {
    if(port < 0 || port >= MIDI_PORT_NUM_TRACK_OUTPUTS) {
        log_error("ssmpco - port invalid: %d", port);
        return;
    }
    if(ppq < 0 || ppq >= SEQ_UTILS_CLOCK_PPQS) {
        log_error("ssmpco - ppq invalid: %d", ppq);
        return;
    }
    song.midi_clock_out[port] = ppq;
    // fire event
    state_change_fire2(SCE_SONG_MIDI_PORT_CLOCK_OUT, port, ppq);
}

// get the MIDI clock source - see lookup in song.h
int song_get_midi_clock_source(void) {
    return song.midi_clock_source;
}

// set the MIDI clock source - see lookup in song.h
void song_set_midi_clock_source(int source) {
    if(source < SONG_MIDI_CLOCK_SOURCE_INT ||
            source > SONG_MIDI_CLOCK_SOURCE_USB_DEV_IN) {
        log_error("ssmcs - source invalid: %d", source);
        return;
    }
    song.midi_clock_source = source;
    // fire event
    state_change_fire1(SCE_SONG_MIDI_CLOCK_SOURCE, song.midi_clock_source);
}

// get whether MIDI remote control is enabled
int song_get_midi_remote_ctrl(void) {
    return song.midi_remote_ctrl;
}

// set whether MIDI remote control is enabled
void song_set_midi_remote_ctrl(int enable) {
    if(enable) {
        song.midi_remote_ctrl = 1;
    }
    else {
        song.midi_remote_ctrl = 0;
    }
    // fire event
    state_change_fire1(SCE_SONG_MIDI_REMOTE_CTRL, song.midi_remote_ctrl);
}

// get whether autolive is enabled
int song_get_midi_autolive(void) {
    return song.midi_autolive;
}

// set whether autolive is enable
void song_set_midi_autolive(int enable) {
    if(enable) {
        song.midi_autolive = 1;
    }
    else {
        song.midi_autolive = 0;
    }
    // fire event
    state_change_fire1(SCE_SONG_MIDI_AUTOLIVE, song.midi_autolive);
}

// get the scene sync mode
int song_get_scene_sync(void) {
    return song.scene_sync;
}

// set the scene sync mode
void song_set_scene_sync(int mode) {
    if(mode < SONG_SCENE_SYNC_BEAT || mode > SONG_SCENE_SYNC_TRACK1) {
        log_error("ssss - mode invalid: %d", mode);
        return;
    }
    song.scene_sync = mode;
    // fire event
    state_change_fire1(SCE_SONG_SCENE_SYNC, song.scene_sync);
}

// get the magic range
int song_get_magic_range(void) {
    return song.magic_range;
}

// set the magic range
void song_set_magic_range(int range) {
    if(range < SONG_MAGIC_RANGE_MIN || range > SONG_MAGIC_RANGE_MAX) {
        log_error("ssmr - range invalid: %d", range);
        return;
    }
    song.magic_range = range;
    // fire event
    state_change_fire1(SCE_SONG_MAGIC_RANGE, song.magic_range);
}

// get the magic chance
int song_get_magic_chance(void) {
    return song.magic_chance;
}

// set the magic chance
void song_set_magic_chance(int chance) {
    if(chance < SONG_MAGIC_CHANCE_MIN || chance > SONG_MAGIC_CHANCE_MAX) {
        log_error("ssmc - chance invalid: %d", chance);
        return;
    }
    song.magic_chance = chance;
    // fire event
    state_change_fire1(SCE_SONG_MAGIC_CHANCE, song.magic_chance);
}

//
// song list params (per song)
//
// insert a blank entry before the selected entry (move everything down)
void song_add_song_list_entry(int entry) {
    int i;
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("sasle - entry invalid: %d", entry);
        return;
    }
    // move all entries backward from this point onward
    for(i = (SEQ_SONG_LIST_ENTRIES - 1); i >= entry; i --) {
        // detect whether the overwrite will produce any change to the current entry
        if(song.snglist[i].scene != song.snglist[i - 1].scene ||
                song.snglist[i].length_beats != song.snglist[i - 1].length_beats ||
                song.snglist[i].kbtrans != song.snglist[i - 1].kbtrans) {
            // overwrite the next entry with the previous entry
            song.snglist[i].scene = song.snglist[i - 1].scene;
            song.snglist[i].length_beats = song.snglist[i - 1].length_beats;
            song.snglist[i].kbtrans = song.snglist[i - 1].kbtrans;
            // notify listeners that our slot has changed
            state_change_fire2(SCE_SONG_LIST_SCENE, i, song.snglist[i].scene);
            state_change_fire2(SCE_SONG_LIST_LENGTH, i, song.snglist[i].length_beats);
            state_change_fire2(SCE_SONG_LIST_KBTRANS, i, song.snglist[i].kbtrans);
        }
    }
    song.snglist[entry].scene = SONG_LIST_SCENE_NULL;
    song.snglist[entry].length_beats = SEQ_SONG_LIST_DEFAULT_LENGTH;
    song.snglist[entry].kbtrans = SEQ_SONG_LIST_DEFAULT_KBTRANS;
    // notify listeners that our slot has changed
    state_change_fire2(SCE_SONG_LIST_SCENE, entry, song.snglist[entry].scene);
    state_change_fire2(SCE_SONG_LIST_LENGTH, entry, song.snglist[entry].length_beats);
    state_change_fire2(SCE_SONG_LIST_KBTRANS, entry, song.snglist[entry].kbtrans);
}

// remove an entry in the song list
void song_remove_song_list_entry(int entry) {
    int i;
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("srsle - entry invalid: %d", entry);
        return;
    }
    // move all entries forward from this point onward
    for(i = entry; i < (SEQ_SONG_LIST_ENTRIES - 1); i ++) {
        // detect whether the overwrite will produce any change to the current entry
        if(song.snglist[i].scene != song.snglist[i + 1].scene ||
                song.snglist[i].length_beats != song.snglist[i + 1].length_beats ||
                song.snglist[i].kbtrans != song.snglist[i + 1].kbtrans) {
            // overwrite this entry with the next entry
            song.snglist[i].scene = song.snglist[i + 1].scene;
            song.snglist[i].length_beats = song.snglist[i + 1].length_beats;
            song.snglist[i].kbtrans = song.snglist[i + 1].kbtrans;
            // notify listeners that our slot has changed
            state_change_fire2(SCE_SONG_LIST_SCENE, i, song.snglist[i].scene);
            state_change_fire2(SCE_SONG_LIST_LENGTH, i, song.snglist[i].length_beats);
            state_change_fire2(SCE_SONG_LIST_KBTRANS, i, song.snglist[i].kbtrans);
        }
    }
}

// get the scene for an entry in the song list
int song_get_song_list_scene(int entry) {
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("sgsls - entry invalid: %d", entry);
        return SONG_LIST_SCENE_NULL;
    }
    return song.snglist[entry].scene;
}

// set the scene for an entry in the song list
void song_set_song_list_scene(int entry, int scene) {
    int init = 0;
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("sssls - entry invalid: %d", entry);
        return;
    }
    if(scene < 0 || scene > SONG_LIST_SCENE_REPEAT) {
        log_error("sssls - scene invalid: %d", scene);
        return;
    }
    // see if we should load defaults into the slot (if it was null before)
    if(song.snglist[entry].scene == SONG_LIST_SCENE_NULL) {
        init = 1;
    }
    song.snglist[entry].scene = scene;
    // fire event
    state_change_fire2(SCE_SONG_LIST_SCENE, entry, scene);
    // if this slot was unused before then populate with defaults
    if(init) {
        song_set_song_list_length(entry, SEQ_SONG_LIST_DEFAULT_LENGTH);
        song_set_song_list_kbtrans(entry, SEQ_SONG_LIST_DEFAULT_KBTRANS);
    }
}

// get the length of the entry in beats
int song_get_song_list_length(int entry) {
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("sgsll - entry invalid: %d", entry);
        return 0;
    }
    return song.snglist[entry].length_beats;
}

// set the length of the entry in beats
void song_set_song_list_length(int entry, int length) {
    int init = 0;
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("sssll - entry invalid: %d", entry);
        return;
    }
    if(length < 0 || length >= 0xffff) {
        log_error("sssll - length invalid: %d", length);
        return;
    }
    // see if we should load defaults into the slot (if it was null before)
    if(song.snglist[entry].scene == SONG_LIST_SCENE_NULL) {
        init = 1;
    }
    song.snglist[entry].length_beats = length;
    // if this slot was unused before then populate with defaults
    if(init) {
        song_set_song_list_scene(entry, SEQ_SONG_LIST_DEFAULT_SCENE);  // first
        song_set_song_list_kbtrans(entry, SEQ_SONG_LIST_DEFAULT_KBTRANS);
    }
    // fire event
    state_change_fire2(SCE_SONG_LIST_LENGTH, entry, length);
}

// get the KB trans
int song_get_song_list_kbtrans(int entry) {
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("sgslkt - entry invalid: %d", entry);
        return 0;
    }
    return song.snglist[entry].kbtrans;
}

// set the KB trans
void song_set_song_list_kbtrans(int entry, int kbtrans) {
    int init = 0;
    if(entry < 0 || entry >= SEQ_SONG_LIST_ENTRIES) {
        log_error("ssslkt - entry invalid: %d", entry);
        return;
    }
    if(kbtrans < SEQ_ENGINE_KEY_TRANSPOSE_MIN ||
            kbtrans > SEQ_ENGINE_KEY_TRANSPOSE_MAX) {
        log_error("ssslkt - kbtrans invalid: %d", kbtrans);
        return;
    }
    // see if we should load defaults into the slot (if it was null before)
    if(song.snglist[entry].scene == SONG_LIST_SCENE_NULL) {
        init = 1;
    }
    song.snglist[entry].kbtrans = kbtrans;
    // if this slot was unused before then populate with defaults
    if(init) {
        song_set_song_list_scene(entry, SEQ_SONG_LIST_DEFAULT_SCENE);  // first
        song_set_song_list_length(entry, SEQ_SONG_LIST_DEFAULT_LENGTH);
    }
    // fire event
    state_change_fire2(SCE_SONG_LIST_KBTRANS, entry, kbtrans);
}

//
// track params (per track)
//
// get the MIDI program for a track output
int song_get_midi_program(int track, int mapnum) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgmp - track invalid: %d", track);
        return -1;
    }
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("sgmp - mapnum invalid: %d", mapnum);
        return -1;
    }
    return song.trkparam[track].midi_program[mapnum];
}

// set the MIDI program for a track output
void song_set_midi_program(int track, int mapnum, int program) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssmp - track invalid: %d", track);
        return;
    }
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("ssmp - mapnum invalid: %d", mapnum);
        return;
    }
    if(program < SONG_MIDI_PROG_NULL || program > 127) {
        log_error("ssmp - program invalid: %d", program);
        return;
    }
    song.trkparam[track].midi_program[mapnum] = program;
    // fire event
    state_change_fire3(SCE_SONG_MIDI_PROGRAM, track, mapnum, program);
}

// get a MIDI port mapping for a track - returns -2 on error, -1 on unmapped
int song_get_midi_port_map(int track, int mapnum) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgmpm - track invalid: %d", track);
        return -2;
    }
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("sgmpm - mapnum invalid: %d", mapnum);
        return -2;
    }
    return song.trkparam[track].midi_output_port[mapnum];
}

// set a MIDI port mapping for a track
void song_set_midi_port_map(int track, int mapnum, int port) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssmpm - track invalid: %d", track);
        return;
    }
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("ssmpm - mapnum invalid: %d", mapnum);
        return;
    }
    if(port < -1 || port >= MIDI_PORT_NUM_TRACK_OUTPUTS) {
        log_error("ssmpm - port invalid: %d", port);
        return;
    }
    song.trkparam[track].midi_output_port[mapnum] = port;
    // fire event
    state_change_fire3(SCE_SONG_MIDI_PORT_MAP, track, mapnum, port);
}

// get a MIDI channel mapping for a track - returns -1 on error
int song_get_midi_channel_map(int track, int mapnum) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgmcm - track invalid: %d", track);
        return -1;
    }
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("ssgmcm - mapnum invalid: %d", mapnum);
        return -1;
    }
    return song.trkparam[track].midi_output_chan[mapnum];
}

// set a MIDI port mapping for a track
void song_set_midi_channel_map(int track, int mapnum, int channel) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssmcm - track invalid: %d", track);
        return;
    }
    if(mapnum < 0 || mapnum >= SEQ_NUM_TRACK_OUTPUTS) {
        log_error("ssmcm - mapnum invalid: %d", mapnum);
        return;
    }
    if(channel < 0 || channel >= MIDI_NUM_CHANNELS) {
        log_error("ssmcm - channel invalid: %d", channel);
        return;
    }
    song.trkparam[track].midi_output_chan[mapnum] = channel;
    // fire event
    state_change_fire3(SCE_SONG_MIDI_CHANNEL_MAP, track, mapnum, channel);
}

// get the MIDI input key split mode
int song_get_key_split(int track) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgks - track invalid: %d", track);
        return -1;
    }
    return song.trkparam[track].midi_key_split;
}

// set the MIDI input key split mode
void song_set_key_split(int track, int mode) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssks - track invalid: %d", track);
        return;
    }
    if(mode < SONG_KEY_SPLIT_OFF || mode > SONG_KEY_SPLIT_RIGHT) {
        log_error("ssks - mode invalid: %d", mode);
        return;
    }
    song.trkparam[track].midi_key_split = mode;
    // fire event
    state_change_fire2(SCE_SONG_KEY_SPLIT, track, mode);
}

// get the track type - returns -1 on error
int song_get_track_type(int track) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgtt - track invalid: %d", track);
        return -1;
    }
    return song.trkparam[track].track_type;
}

// set the track type
void song_set_track_type(int track, int mode) {
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sstt - track invalid: %d", track);
        return;
    }
    switch(mode) {
        case SONG_TRACK_TYPE_DRUM:
            song.trkparam[track].track_type = SONG_TRACK_TYPE_DRUM;
            break;
        case SONG_TRACK_TYPE_VOICE:
        default:
            song.trkparam[track].track_type = SONG_TRACK_TYPE_VOICE;
            break;
    }
    // fire event
    state_change_fire2(SCE_SONG_TRACK_TYPE, track, song.trkparam[track].track_type);
}

//
// track params (per scene)
//
// get the step length - returns -1 on error
int song_get_step_length(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgsl - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgsl - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].step_len;
}

// set the step length
void song_set_step_length(int scene, int track, int length) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sssl - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sssl - track invalid: %d", track);
        return;
    }
    if(length < 0 || length >= SEQ_UTILS_STEP_LENS) {
        log_error("sssl - length invalid: %d", length);
        return;
    }
    song.trkscene[scene][track].step_len = length;
    // fire event
    state_change_fire3(SCE_SONG_STEP_LEN, scene, track, length);
}

// get the tonality on a track - returns -1 on error
int song_get_tonality(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgt - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgt - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].tonality;
}

// set the tonality on a track
void song_set_tonality(int scene, int track, int tonality) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sst - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sst - track invalid: %d", track);
        return;
    }
    if(tonality < 0 || tonality >= SCALE_NUM_TONALITIES) {
        log_error("sst - tonality invalid: %d", tonality);
        return;
    }
    song.trkscene[scene][track].tonality = tonality;
    // fire event
    state_change_fire3(SCE_SONG_TONALITY, scene, track, tonality);
}

// get the transpose on a track - returns -1 on error
int song_get_transpose(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgt - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgt - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].transpose;
}

// set the transpose on a track
void song_set_transpose(int scene, int track, int transpose) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sst - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sst - track invalid: %d", track);
        return;
    }
    if(transpose < SEQ_TRANSPOSE_MIN || transpose > SEQ_TRANSPOSE_MAX) {
        log_error("sst - transpose invalid: %d", transpose);
        return;
    }
    song.trkscene[scene][track].transpose = transpose;
    // fire event
    state_change_fire3(SCE_SONG_TRANSPOSE, scene, track, transpose);
}

// get the bias track for this track - returns -1 on error
int song_get_bias_track(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgbt - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgbt - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].bias_track;
}

// set the bias track for this track
void song_set_bias_track(int scene, int track, int bias_track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssbt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssbt - track invalid: %d", track);
        return;
    }
    if(bias_track < SONG_TRACK_BIAS_NULL || bias_track >= SEQ_NUM_TRACKS) {
        log_error("ssbt - bias track invalid: %d", bias_track);
        return;
    }
    song.trkscene[scene][track].bias_track = bias_track;
    // fire event
    state_change_fire3(SCE_SONG_BIAS_TRACK, scene, track, bias_track);
}

// get the motion start on a track - returns -1 on error
int song_get_motion_start(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgms - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgms - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].motion_start;
}

// set the motion start on a track
void song_set_motion_start(int scene, int track, int start) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssms - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssms - track invalid: %d", track);
        return;
    }
    if(start < 0 || start >= SEQ_NUM_STEPS) {
        log_error("ssms - start invalid: %d", start);
        return;
    }
    song.trkscene[scene][track].motion_start = start;
    // fire event
    state_change_fire3(SCE_SONG_MOTION_START, scene, track, start);
}

// get the motion length on a track - returns -1 on error
int song_get_motion_length(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgml - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgml - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].motion_len;
}

// set the motion length on a track
void song_set_motion_length(int scene, int track, int length) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssml - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssml - track invalid: %d", track);
        return;
    }
    if(length < 1 || length > SEQ_NUM_STEPS) {
        log_error("ssml - length invalid: %d", length);
        return;
    }
    song.trkscene[scene][track].motion_len = length;
    // fire event
    state_change_fire3(SCE_SONG_MOTION_LENGTH, scene, track, length);
}

// get the gate time on a track in ticks - returns -1 on error
int song_get_gate_time(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sggt - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sggt - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].gate_time;
}

// set the gate time on a track in ticks
void song_set_gate_time(int scene, int track, int time) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssgt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssgt - track invalid: %d", track);
        return;
    }
    if(time < SEQ_GATE_TIME_MIN || time > SEQ_GATE_TIME_MAX) {
        log_error("ssgt - time invalid: %d", time);
        return;
    }
    song.trkscene[scene][track].gate_time = time;
    // fire event
    state_change_fire3(SCE_SONG_GATE_TIME, scene, track, time);
}

// get the pattern type on a track - returns -1 on error
int song_get_pattern_type(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgpt - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgpt - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].pattern_type;
}

// set the pattern type on a track
void song_set_pattern_type(int scene, int track, int pattern) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sspt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sspt - track invalid: %d", track);
        return;
    }
    if(pattern < 0 || pattern >= SEQ_NUM_PATTERNS) {
        log_error("sspt - pattern invalid: %d", pattern);
        return;
    }
    song.trkscene[scene][track].pattern_type = pattern;
    // fire event
    state_change_fire3(SCE_SONG_PATTERN_TYPE, scene, track, pattern);
}

// get the playback direction on a track - returns -1 on error
int song_get_motion_dir(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgmd - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgmd - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].dir_reverse;
}

// set the playback direction on a track
void song_set_motion_dir(int scene, int track, int reverse) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssmd - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssmd - track invalid: %d", track);
        return;
    }
    if(reverse) {
        song.trkscene[scene][track].dir_reverse = 1;
    }
    else {
        song.trkscene[scene][track].dir_reverse = 0;
    }
    // fire event
    state_change_fire3(SCE_SONG_MOTION_DIR, scene, track,
        song.trkscene[scene][track].dir_reverse);
}

// get the mute state of a track - returns -1 on error
int song_get_mute(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgm - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgm - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].mute;
}

// sets the mute state of a track
void song_set_mute(int scene, int track, int mute) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssm - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssm - track invalid: %d", track);
        return;
    }
    if(mute) {
        song.trkscene[scene][track].mute = 1;
    }
    else {
        song.trkscene[scene][track].mute = 0;
    }
    // fire event
    state_change_fire3(SCE_SONG_MUTE, scene, track,
        song.trkscene[scene][track].mute);
}

// get the arp type on a track - returns -1 on error
int song_get_arp_type(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgat - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgat - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].arp_type;
}

// set the arp type on a track
void song_set_arp_type(int scene, int track, int type) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssat - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssat - track invalid: %d", track);
        return;
    }
    if(type < 0 || type >= ARP_NUM_TYPES) {
        log_error("ssat - arp type invalid: %d", type);
        return;
    }
    song.trkscene[scene][track].arp_type = type;
    // fire event
    state_change_fire3(SCE_SONG_ARP_TYPE, scene, track, type);
}

// get the arp speed on a track - returns -1 on error
int song_get_arp_speed(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgas - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgas - track invalid: %d", track);
        return -1;
    }
    return song.trkscene[scene][track].arp_speed;
}

// set the arp speed on a track
void song_set_arp_speed(int scene, int track, int speed) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssas - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssas - track invalid: %d", track);
        return;
    }
    if(speed < 0 || speed >= SEQ_UTILS_STEP_LENS) {
        log_error("ssas - speed invalid: %d", speed);
        return;
    }
    song.trkscene[scene][track].arp_speed = speed;
    // fire event
    state_change_fire3(SCE_SONG_ARP_SPEED, scene, track, speed);
}

// get the arp gate time on a track
int song_get_arp_gate_time(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgagt - scene invalid: %d", scene);
        return 0;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgagt - track invalid: %d", track);
        return 0;
    }
    return song.trkscene[scene][track].arp_gate_time;
}

// set the arp gate time on a track
void song_set_arp_gate_time(int scene, int track, int time) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssagt - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssagt - track invalid: %d", track);
        return;
    }
    if(time < ARP_GATE_TIME_MIN || time > ARP_GATE_TIME_MAX) {
        log_error("ssagt - time invalid: %d", time);
        return;
    }
    song.trkscene[scene][track].arp_gate_time = time;
    // fire event
    state_change_fire3(SCE_SONG_ARP_GATE_TIME, scene, track, time);
}

// check if the arp is enabled on a track
int song_get_arp_enable(int scene, int track) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgae - scene invalid: %d", scene);
        return 0;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgae - track invalid: %d", track);
        return 0;
    }
    return song.trkscene[scene][track].arp_enable;
}

// set if the arp is enabled on a track
void song_set_arp_enable(int scene, int track, int enable) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssae - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssae - track invalid: %d", track);
        return;
    }
    if(enable) {
        song.trkscene[scene][track].arp_enable = 1;
    }
    else {
        song.trkscene[scene][track].arp_enable = 0;
    }
    // fire event
    state_change_fire3(SCE_SONG_ARP_ENABLE, scene, track,
        song.trkscene[scene][track].arp_enable);
}

//
// track event getters and setters
//
// clear all events on a step
void song_clear_step(int scene, int track, int step) {
    int poly;
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("scs - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("scs - track invalid: %d", track);
        return;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("scs - step invalid: %d", step);
        return;
    }
    for(poly = 0; poly < SEQ_TRACK_POLY; poly ++) {
#ifdef SONG_NOTES_PER_SCENE
        song.trkevents[scene][track][step][poly].type = SONG_EVENT_NULL;
#else
        song.trkevents[track][step][poly].type = SONG_EVENT_NULL;
#endif
    }
    song_set_ratchet_mode(scene, track, step, SEQ_RATCHET_MIN);
    song_set_start_delay(scene, track, step, SEQ_START_DELAY_MIN);
    // fire event
    state_change_fire3(SCE_SONG_CLEAR_STEP, scene, track, step);
}

// clear a specific event on a step
void song_clear_step_event(int scene, int track, int step, int slot) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("scse - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("scse - track invalid: %d", track);
        return;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("scse - step invalid: %d", step);
        return;
    }
    if(slot < 0 || slot >= SEQ_TRACK_POLY) {
        log_error("scse - slot invalid: %d", slot);
        return;
    }
#ifdef SONG_NOTES_PER_SCENE
    song.trkevents[scene][track][step][slot].type = SONG_EVENT_NULL;
#else
    song.trkevents[track][step][slot].type = SONG_EVENT_NULL;
#endif
    // fire event
    state_change_fire3(SCE_SONG_CLEAR_STEP_EVENT, scene, track, step);
}

// get the number of active events on a step - returns -1 on error
// this should not be used for iterating since the slots might be fragmented
int song_get_num_step_events(int scene, int track, int step) {
    int poly, num_events;
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgnse - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgnse - track invalid: %d", track);
        return -1;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("sgnse - step invalid: %d", step);
        return -1;
    }
    num_events = 0;
    for(poly = 0; poly < SEQ_TRACK_POLY; poly ++) {
#ifdef SONG_NOTES_PER_SCENE
        if(song.trkevents[scene][track][step][poly].type != SONG_EVENT_NULL) {
#else
        if(song.trkevents[track][step][poly].type != SONG_EVENT_NULL) {
#endif
            num_events ++;
        }
    }
    return num_events;
}

// add an event to a step - returns -1 on error (no more slots)
// the event data is copied into the song
int song_add_step_event(int scene, int track, int step,
        struct track_event *event) {
    int poly, existing_slot, blank_slot, slot;
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sase - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sase - track invalid: %d", track);
        return -1;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("sase - step invalid: %d", step);
        return -1;
    }
    // search for a blank slot or a slot with the same event type
    existing_slot = -1;
    blank_slot = -1;
    for(poly = 0; poly < SEQ_TRACK_POLY; poly ++) {
        // a blank slot was found
#ifdef SONG_NOTES_PER_SCENE
        if(song.trkevents[scene][track][step][poly].type == SONG_EVENT_NULL &&
#else
        if(song.trkevents[track][step][poly].type == SONG_EVENT_NULL &&
#endif
                blank_slot == -1) {
            blank_slot = poly;
        }
        // an existing slot was found with the same note or CC
#ifdef SONG_NOTES_PER_SCENE
        if(song.trkevents[scene][track][step][poly].type == event->type &&
                song.trkevents[scene][track][step][poly].data0 == event->data0) {
#else
        if(song.trkevents[track][step][poly].type == event->type &&
                song.trkevents[track][step][poly].data0 == event->data0) {
#endif
            existing_slot = poly;
            break;  // this is all we need
        }
    }
    slot = -1;
    if(existing_slot != -1) {
        slot = existing_slot;
    }
    else if(blank_slot != -1) {
        slot = blank_slot;
    }
    // check that we have a free slot
    if(slot == -1) {
        log_error("sase - nofree slots");
        return -1;
    }
    // copy the data to the song
#ifdef SONG_NOTES_PER_SCENE
    song.trkevents[scene][track][step][slot].type = event->type;
    song.trkevents[scene][track][step][slot].data0 = event->data0;
    song.trkevents[scene][track][step][slot].data1 = event->data1;
    song.trkevents[scene][track][step][slot].length = event->length;
#else
    song.trkevents[track][step][slot].type = event->type;
    song.trkevents[track][step][slot].data0 = event->data0;
    song.trkevents[track][step][slot].data1 = event->data1;
    song.trkevents[track][step][slot].length = event->length;
#endif
    // fire event
    state_change_fire3(SCE_SONG_ADD_STEP_EVENT, scene, track, step);
    return 0;
}

// set/replace the value of a slot - returns -1 on error (invalid slot)
int song_set_step_event(int scene, int track, int step,
        int slot, struct track_event *event) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssse - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssse - track invalid: %d", track);
        return -1;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("ssse - step invalid: %d", step);
        return -1;
    }
    if(slot < 0 || slot >= SEQ_TRACK_POLY) {
        log_error("ssse - slot invalid: %d", slot);
        return -1;
    }
#ifdef SONG_NOTES_PER_SCENE
    song.trkevents[scene][track][step][slot].type = event->type;
    song.trkevents[scene][track][step][slot].data0 = event->data0;
    song.trkevents[scene][track][step][slot].data1 = event->data1;
    song.trkevents[scene][track][step][slot].length = event->length;
#else
    song.trkevents[track][step][slot].type = event->type;
    song.trkevents[track][step][slot].data0 = event->data0;
    song.trkevents[track][step][slot].data1 = event->data1;
    song.trkevents[track][step][slot].length = event->length;
#endif
    // fire event
    state_change_fire3(SCE_SONG_SET_STEP_EVENT, scene, track, step);
    return 0;
}

// get an event from a step - returns -1 on error (invalid / blank slot)
// the event data is copied into the pointed to struct - returns -1 on error
int song_get_step_event(int scene, int track, int step, int slot,
        struct track_event *event) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgse - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgse - track invalid: %d", track);
        return -1;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("sgse - step invalid: %d", step);
        return -1;
    }
    if(slot < 0 || slot >= SEQ_TRACK_POLY) {
        log_error("sgse - slot invalid: %d", slot);
        return -1;
    }
    // copy the data from the song
#ifdef SONG_NOTES_PER_SCENE
    event->type = song.trkevents[scene][track][step][slot].type;
    event->data0 = song.trkevents[scene][track][step][slot].data0;
    event->data1 = song.trkevents[scene][track][step][slot].data1;
    event->length = song.trkevents[scene][track][step][slot].length;
#else
    event->type = song.trkevents[track][step][slot].type;
    event->data0 = song.trkevents[track][step][slot].data0;
    event->data1 = song.trkevents[track][step][slot].data1;
    event->length = song.trkevents[track][step][slot].length;
#endif
    // blank slot
    if(event->type == SONG_EVENT_NULL) {
        return -1;
    }
    return 0;
}

// get the start delay for a step - returns -1 on error
int song_get_start_delay(int scene, int track, int step) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgsd - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgsd - track invalid: %d", track);
        return -1;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("sgsd - step invalid: %d", step);
        return -1;
    }
#ifdef SONG_NOTES_PER_SCENE
    return song.trkstepparam[scene][track][step].start_delay;
#else
    return song.trkstepparam[track][step].start_delay;
#endif
}

// set the start delay for a step
void song_set_start_delay(int scene, int track, int step, int delay) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sssd - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sssd - track invalid: %d", track);
        return;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("sssd - step invalid: %d", step);
        return;
    }
    if(delay < SEQ_START_DELAY_MIN || delay > SEQ_START_DELAY_MAX) {
        log_error("sssd - delay invalid: %d", delay);
        return;
    }
#ifdef SONG_NOTES_PER_SCENE
    song.trkstepparam[scene][track][step].start_delay = delay;
#else
    song.trkstepparam[track][step].start_delay = delay;
#endif
    // fire event
    state_change_fire3(SCE_SONG_START_DELAY, scene, track, step);
}

// get the ratchet mode for a step - returns -1 on error
int song_get_ratchet_mode(int scene, int track, int step) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("sgrm - scene invalid: %d", scene);
        return -1;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("sgrm - track invalid: %d", track);
        return -1;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("sgrm - step invalid: %d", step);
        return -1;
    }
#ifdef SONG_NOTES_PER_SCENE
    return song.trkstepparam[scene][track][step].ratchet;
#else
    return song.trkstepparam[track][step].ratchet;
#endif
}

// set the ratchet mode for a step
void song_set_ratchet_mode(int scene, int track, int step, int ratchet) {
    if(scene < 0 || scene >= SEQ_NUM_SCENES) {
        log_error("ssrm - scene invalid: %d", scene);
        return;
    }
    if(track < 0 || track >= SEQ_NUM_TRACKS) {
        log_error("ssrm - track invalid: %d", track);
        return;
    }
    if(step < 0 || step >= SEQ_NUM_STEPS) {
        log_error("ssrm - step invalid: %d", step);
        return;
    }
    if(ratchet < SEQ_RATCHET_MIN || ratchet > SEQ_RATCHET_MAX) {
        log_error("ssrm - ratchet invalid: %d", ratchet);
        return;
    }
#ifdef SONG_NOTES_PER_SCENE
    song.trkstepparam[scene][track][step].ratchet = ratchet;
#else
    song.trkstepparam[track][step].ratchet = ratchet;
#endif
    // fire event
    state_change_fire3(SCE_SONG_RATCHET_MODE, scene, track, step);
}
