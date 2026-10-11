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

# Apple clang predefines __weak for Objective-C, not ELF-style callbacks.
"$CC" -U__weak '-D__weak=__attribute__((weak))' \
    -x c -std=c11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-sign-compare \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    tests/clock_test.c.in src/midi/midi_clock.c src/midi/midi_utils.c \
    src/util/seq_utils.c src/seq/midi_ctrl.c "$LINK_GC" -o "$TMP/clock"
"$TMP/clock"
