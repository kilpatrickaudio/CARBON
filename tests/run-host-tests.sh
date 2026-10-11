#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
UBSAN_OPTIONS=halt_on_error=1
export UBSAN_OPTIONS
case $(uname -s) in
    Darwin) LINK_GC=-Wl,-dead_strip ;;
    *) LINK_GC=-Wl,--gc-sections ;;
esac
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    tests/conditional_test.c.in -o "$TMP/conditional"
"$TMP/conditional"

"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    tests/engine_test.c.in src/midi/midi_utils.c src/util/seq_utils.c \
    src/seq/midi_ctrl.c "$LINK_GC" -o "$TMP/engine"
"$TMP/engine"

"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    -Wno-deprecated-declarations \
    tests/editor_test.c.in src/midi/midi_utils.c src/util/seq_utils.c \
    src/util/panel_utils.c "$LINK_GC" -o "$TMP/editor"
"$TMP/editor"

"$CC" -x c -std=c11 -Wall -Wextra -Werror -Wno-cpp \
    -Wno-unused-parameter -Wno-sign-compare \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    tests/song_test.c.in src/util/seq_utils.c "$LINK_GC" -o "$TMP/song"
"$TMP/song"

"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    -Wno-deprecated-declarations tests/root_test.c.in src/seq/scale.c src/midi/midi_utils.c \
    src/util/seq_utils.c "$LINK_GC" -o "$TMP/root"
"$TMP/root"

"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare -Wno-deprecated-declarations \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    tests/pitch_integration_test.c.in src/seq/scale.c src/midi/midi_utils.c \
    src/util/seq_utils.c src/seq/midi_ctrl.c "$LINK_GC" -o "$TMP/pitch-integration"
"$TMP/pitch-integration"

"$CC" -x c -std=c11 -Wall -Wextra -Werror -Wno-cpp \
    -Wno-unused-parameter -Wno-sign-compare \
    -ffunction-sections -fdata-sections tests/song_layout_test.c.in \
    "$LINK_GC" -o "$TMP/song-layout"
"$TMP/song-layout"

"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare -Wno-deprecated-declarations \
    -Wno-tautological-constant-out-of-range-compare \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    tests/root_ui_test.c.in src/seq/scale.c src/util/panel_utils.c src/util/seq_utils.c \
    "$LINK_GC" -o "$TMP/root-ui"
"$TMP/root-ui"

# Apple clang predefines __weak for Objective-C, not ELF-style callbacks.
"$CC" -U__weak '-D__weak=__attribute__((weak))' \
    -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    tests/clock_test.c.in src/midi/midi_clock.c src/midi/midi_utils.c \
    src/util/seq_utils.c src/seq/midi_ctrl.c "$LINK_GC" -o "$TMP/clock"
"$TMP/clock"

"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare -Wno-deprecated-declarations \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    -Wno-tautological-constant-out-of-range-compare \
    tests/gui_status_test.c.in src/util/seq_utils.c "$LINK_GC" -o "$TMP/gui-status"
"$TMP/gui-status"
