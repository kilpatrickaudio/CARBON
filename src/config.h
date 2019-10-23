/*
 * CARBON Sequencer Config
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
#ifndef CONFIG_H
#define CONFIG_H

// global
#define CARBON_VERSION_MAJOR 1
#define CARBON_VERSION_MINOR 23
#define CARBON_VERSION_MAJMIN ((CARBON_VERSION_MAJOR << 16) | CARBON_VERSION_MINOR)

// memory mapping
// - while not actual config data this is a nice place to mention it
// - the STM32F407 has 128K (RAM) + 64K (CCMRAM)
// - due to large block required by the song in RAM some blocks
//   are placed specifically in CCMRAM instead:
// - CCMRAM blocks:
//   - config_store.c       - cfgss
//   - lcd_fsmc_if.c        - lcds
//   - midi_stream.c        - midi_stream_queue
//   - gui.c                - gstate
//

// interrupt priorities
#define INT_PRIO_SPI_PANEL 0  // highest prio - avoids SPI lockup
#define INT_PRIO_SYSTICK 1  // needs to be higher than everything else
#define INT_PRIO_SPI_FLASH_DMA_TX 2
#define INT_PRIO_SPI_FLASH_DMA_RX 2
#define INT_PRIO_SPI_ANALOG_OUT 2
#define INT_PRIO_DIN_MIDI_DMA_TX1 6
#define INT_PRIO_DIN_MIDI_DMA_RX1 6
#define INT_PRIO_DIN_MIDI_DMA_UART1 6
#define INT_PRIO_DIN_MIDI_DMA_TX2 6
#define INT_PRIO_DIN_MIDI_DMA_UART2 6
#define INT_PRIO_USBD_CORE 7
#define INT_PRIO_USBH_CORE 8  // must be the same prio as the USBH timer

// SPI channels (for HAL callback dispatching)
#define SPI_NUM_CHANNELS 3
#define SPI_CHANNEL_DAC 0  // SPI1
#define SPI_CHANNEL_PANEL 1  // SPI2
#define SPI_CHANNEL_ROM 2  // SPI3

// song size
// XXX this changes the total number of songs that can be stored in flash
//#define SONG_NOTES_PER_SCENE  // enables larger song with notes per scene

// external flash storage (on SPI flash)
#define EXT_FLASH_SONG_OFFSET 0x000000
#ifdef SONG_NOTES_PER_SCENE
  #define EXT_FLASH_SONG_SIZE 0x16000
#else
  #define EXT_FLASH_SONG_SIZE 0x5000
#endif
#define EXT_FLASH_CONFIG_OFFSET 0x160000
#define EXT_FLASH_CONFIG_SIZE 0x1000

// config store
#define CONFIG_STORE_WRITEBACK_INTERVAL 0xffff
#define CONFIG_STORE_ITEM_SIZE 4  // number of bytes per item
#define CONFIG_STORE_NUM_ITEMS 128  // max must be a power of 2
#define CONFIG_STORE_LAST_SONG 0
#define CONFIG_STORE_IFACE_CV_PROGRAM 1
#define CONFIG_STORE_IFACE_CV_BEND_RANGE 2
#define CONFIG_STORE_IFACE_ANALOG_CLOCK_DIV 3
#define CONFIG_STORE_MENU_TIMEOUT 4
#define CONFIG_STORE_GUI_DISP_TYPE 5
#define CONFIG_STORE_PATTERN_BANK 62  // start of the pattern bank
// patterns consume 65 words (260 bytes) of space
#define CONFIG_STORE_TOKEN (CONFIG_STORE_NUM_ITEMS - 1)  // must be last item

// power
#define POWER_CTRL_DC_MIN_CUTOFF 14000  // mV input threshold for power error

// timing
#define SEQ_TASK_INTERVAL_US 1000  // us interval for realtime sequencer stuff

// display
#define LCD_W 320
#define LCD_H 480
#define LCD_X_OFFSET 0  // used by LCD drive to offset 0,0 position (deprecated)
#define LCD_Y_OFFSET 0  // used by LCD drive to offset 0,0 position (deprecated)
#define GFX_LABEL_LEN 64  // max length of a label

// panel
#define PANEL_SHIFT_TAP_TIMEOUT 300  // ms to time out double tapping
#define PANEL_MENU_TIMEOUT_DEFAULT 10000  // default ms to time out the menu display
#define PANEL_MENU_TIMEOUT_MIN 1000  // min menu timeout
#define PANEL_MENU_TIMEOUT_MAX 60000  // max menu timeout - max = no timeout
#define PANEL_MENU_CONFIRM_TIMEOUT 1000  // ms to time out the menu after confirming
#define PANEL_KEYS_VELOCITY 100
//#define PANEL_IF_DISABLE_BL  // uncomment to disable the backlight
#define PANEL_IF_BL_COMMON_ANODE  // uncomment if the RGB LEDs are common anode

// MIDI settings
#define MIDI_NUM_CHANNELS 16  // number of channels on a port
#define MIDI_MAX_PORTS 13
#define MIDI_DEV_TYPE 0x49
// MIDI ports
// outputs
#define MIDI_PORT_NUM_TRACK_OUTPUTS 5  // number of outputs on each track
#define MIDI_PORT_DIN1_OUT 0  // track output
#define MIDI_PORT_DIN2_OUT 1  // track output
#define MIDI_PORT_CV_OUT 2  // track output
#define MIDI_PORT_USB_HOST_OUT 3  // track output
#define MIDI_PORT_USB_DEV_OUT1 4  // track output (seq mode) / panel out (interface mode)
#define MIDI_PORT_USB_DEV_OUT2 5  // disabled (seq mode) / DIN1 in (interface mode)
#define MIDI_PORT_USB_DEV_OUT3 6  // disabled (seq mode) / USB host IN (interface mode)
#define MIDI_PORT_SYSEX_OUT (MIDI_PORT_USB_DEV_OUT1)  // SYSEX output
#define MIDI_IFACE_PANEL_OUTPUT_PORT (MIDI_PORT_USB_DEV_OUT1)  // interface out (power down)
// inputs
#define MIDI_PORT_NUM_INPUTS 3  // number of inputs for external MIDI
#define MIDI_PORT_DIN1_IN 7  // clock/performance in (seq mode) / to USB host (interface mode)
#define MIDI_PORT_USB_HOST_IN 8  // clock/performance in (seq mode) / to USB host (interface mode)
#define MIDI_PORT_USB_DEV_IN1 9  // SYSEX/clock/performance in (seq mode) / panel / CV out (interface mode)
#define MIDI_PORT_USB_DEV_IN2 10  // disabled (seq mode) / DIN1 out (interface mode)
#define MIDI_PORT_USB_DEV_IN3 11  // disabled (seq mode) / DIN2 out (interface mode)
#define MIDI_PORT_USB_DEV_IN4 12  // disabled (seq mode) / USB host OUT (interface mode)
#define MIDI_PORT_IN_OFFSET (MIDI_PORT_DIN1_IN)  // offset for MIDI input ports
#define MIDI_PORT_SYSEX_IN (MIDI_PORT_USB_DEV_IN1)  // SYSEX input
// CV/gate
#define CVPROC_NUM_PAIRS 4
#define CVPROC_NUM_OUTPUTS 4
#define CVPROC_CVCAL_SEMI_SIZE_1VOCT 575  // nominal semitone size: 1V/oct (x16)
#define CVPROC_CVCAL_SEMI_SIZE_1P2VOCT 690  // nominal semitone size: 1.2V/oct (x16)
#define CVPROC_CVCAL_MIN -100  // min setting for CV calibration
#define CVPROC_CVCAL_MAX 100  // max setting for CV calibration
#define CVPROC_BEND_RANGE_MIN 1  // min setting for CV bend range
#define CVPROC_BEND_RANGE_MAX 12  // max setting for CV bend range
#define CVPROC_CVOFFSET_MIN -450  // min setting for CV offset
#define CVPROC_CVOFFSET_MAX 450  // max setting for CV offset

// MIDI clock
#define MIDI_CLOCK_TASK_INTERVAL_US (SEQ_TASK_INTERVAL_US)
#define MIDI_CLOCK_DEFAULT_TEMPO 60.0
#define MIDI_CLOCK_TEMPO_MIN 30.0  // BPM
#define MIDI_CLOCK_TEMPO_MAX 300.0  // BPM
#define MIDI_CLOCK_SWING_MIN 50  // percent
#define MIDI_CLOCK_SWING_MAX 80  // percent
#define MIDI_CLOCK_PPQ 96
#define MIDI_CLOCK_UPSAMPLE (MIDI_CLOCK_PPQ / 24)
#define BEAT_LED_TIMEOUT 100  // ms
#define CLOCK_OUT_PULSE_LEN 4  // ms

// metronome
#define METRONOME_MIDI_TRACK 5  // track 6
#define METRONOME_SOUND_LENGTH_DEFAULT 100  // ms
#define METRONOME_SOUND_LENGTH_MIN 5
#define METRONOME_SOUND_LENGTH_MAX 160
#define METRONOME_NOTE_VELOCITY 100

// sequencer
#define SEQ_SONG_LIST_ENTRIES 64
#define SEQ_SONG_LIST_DEFAULT_SCENE 0
#define SEQ_SONG_LIST_DEFAULT_LENGTH 16
#define SEQ_SONG_LIST_DEFAULT_KBTRANS 0
#define SEQ_SONG_LIST_MIN_LENGTH 1
#define SEQ_SONG_LIST_MAX_LENGTH 256
#ifdef SONG_NOTES_PER_SCENE
  #define SEQ_NUM_SONGS 16
#else
  #define SEQ_NUM_SONGS 64
#endif
#define SEQ_NUM_SCENES 6
#define SEQ_NUM_TRACKS 6
#define SEQ_NUM_STEPS 64
#define SEQ_TRACK_POLY 6
#define SEQ_SWING_MIN (MIDI_CLOCK_SWING_MIN)  // percent
#define SEQ_SWING_MAX (MIDI_CLOCK_SWING_MAX)  // percent
#define SEQ_TRANSPOSE_CENTRE 60  // the centre note for transposing and bias tracks
#define SEQ_TRANSPOSE_MIN -24  // min transpose setting for all uses (track, kbtrans, song)
#define SEQ_TRANSPOSE_MAX 24  // max transpose setting for all uses (track, kbtrans, song)
#define SEQ_MIN_NOTE 0  // lowest supported MIDI note
#define SEQ_MAX_NOTE 127  // highest supported MIDI note
#define SEQ_KEY_VEL_SCALE_MIN -100
#define SEQ_KEY_VEL_SCALE_MAX 100
#define SEQ_GATE_TIME_MIN 1  // gate time track override 1%
#define SEQ_GATE_TIME_MAX 255 // gate time track override 200%
#define SEQ_GATE_TIME_STEP_SIZE 8  // gate time track override
#define SEQ_NUM_PATTERNS 32  // enumerated
#define SEQ_NUM_TRACK_OUTPUTS 2  // number of MIDI outputs from a track
#define SEQ_KEY_SPLIT_NOTE 60
#define SEQ_RATCHET_MIN 1  // min ratchet setting
#define SEQ_RATCHET_MAX 8  // max ratchet setting
#define SEQ_START_DELAY_MIN 0  // min start delay
#define SEQ_START_DELAY_MAX (MIDI_CLOCK_PPQ)  // 1 beat
#define STEP_EDIT_NEW_NOTE_VELOCITY 100
#define STEP_EDIT_LOWEST_NOTE (SEQ_MIN_NOTE)
#define STEP_EDIT_HIGHEST_NOTE (SEQ_MAX_NOTE)
#define STEP_EDIT_SHORTEST_NOTE 1
#define STEP_EDIT_LONGEST_NOTE (MIDI_CLOCK_UPSAMPLE * 384)
#define STEP_EDIT_NEW_NOTE 60
#define STEP_EDIT_NEW_CC 1
#define STEP_EDIT_NEW_CC_VAL 64
#define SEQ_ENGINE_KEY_TRANSPOSE_CENTRE 60
#define SEQ_ENGINE_KEY_TRANSPOSE_MIN -24
#define SEQ_ENGINE_KEY_TRANSPOSE_MAX 24
// arp
#define ARP_GATE_TIME_MIN 1
#define ARP_GATE_TIME_MAX 384

//
// debug stuff
//
// debug pins to use for stuff
//#define DEBUG_USBD_PINS  // uncomment to use USB device port pins for debug
//#define DEBUG_USBH_PINS  // uncomment to use USB host port pins for debug
#define DEBUG_TP123  // uncomment to use testpoints 1-3 for debug
// debug MIDI
#define DEBUG_MIDI_PORT MIDI_PORT_DIN2_OUT  // where debug text is sent
//#define DEBUG_MIDI_PORT MIDI_PORT_USB_DEV_OUT1  // where debug text is sent

//
// development mode
//
//#define DEBUG_DEVEL  // uncomment to enable normal development debugging
#ifdef DEBUG_DEVEL
// instrumentation
//#define DEBUG_RT_TIMING  // uncomment to enable debug timing of the RT thread
// debug messages
#define LOG_PRINT_ENABLE  // uncomment to allow log_ messages to render strings
#define DEBUG_OVER_MIDI  // uncomment to route log messages to MIDI / enable active sensing
// power control at startup (for debugging)
#define POWER_CTRL_POWER_ON_AUTO  // uncomment to make power come up automatically
#endif

//
// remote LCD mode - for demonstration / development purposes
//
// WARNING: DO NOT ENABLE THIS IN PRODUCTION CODE
// The remote LCD mode will send large amounts of data over the USB dev port.
//
//#define GFX_REMLCD_MODE  // uncomment this to enable remote LCD mode
#define GFX_REMLCD_MIDI_PORT (MIDI_PORT_USB_DEV_OUT2)  // spare port for remlcd mode
#define GFX_REMLCD_BYTES_PER_MS 48  // number of bytes that will be sent per ms

#endif
