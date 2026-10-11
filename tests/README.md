# Conditional-gate host tests

From the repository root:

```sh
sh tests/run-host-tests.sh
```

The `.c.in` extension keeps this host-only main out of makegen's recursive
firmware source scan. This test exercises the production inline evaluator,
not a duplicate implementation. It covers probability endpoints, skipped
gates, fresh visits, OR history, history preservation, independent streams
and reproducibility. The engine harness includes the production engine source, MIDI encoders,
sequence utilities and internal-CC conversion helpers. Song/control/hardware
services are fixtures. It exercises preparation, playback, note management,
track resets and the production tick loop with address/undefined-behavior
sanitizers: mixed Note/CC/PC output, skipped bias/CCi, muted/Live sources,
masked visits, note-offs, Stop/Continue and tick-zero reset. Both track-order
directions are tested for CCi making targets newly due or unmasked.

Link-time dead stripping removes unused firmware services; no production
conditional compilation or special test-only engine behavior is introduced.
This is not hardware testing or flash-backed song-storage/preview coverage.

The Chapter 9 prototype uses event code 2 for Probability, data0=0..100.
Shift + Transpose cycles Note -> CC -> CCi -> PC -> Probability -> Note.
Probability defaults to 50%; Transpose adjusts its first value. Preview
ignores the gate and auditions ordinary notes/CC/PC; CCi remains playback-only.

Song-record size and event offsets are unchanged. The local firmware/song version
is 1.30 and uses a display-only `johanwk-ch9-dev` identity; this is not an
official release-number allocation. Do not downgrade a
song containing gates: older firmware cannot interpret them and may suppress
later events. Back up songs before testing this development firmware.

## All four gate types

Event codes 2/3/4/5 are Probability/Euclidean/Pre/Non-Pre. The type cycle is
Note -> CC -> CCi -> PC -> Prb -> Euc -> Pre -> !Pr -> Note. Euclidean
defaults to 1:2, rotation 0. Transpose selects its pair (28 entries through
7:8); velocity edits rotation. Pair changes normalize rotation modulo B.
Pre and !Pr have no numeric fields. All gate lengths are unused.

Euclidean positions start at zero, advance after a boundary step, and
advance once for an effective direction reversal. Same-direction assignments
do nothing. One-step and full 64-step windows count correctly; out-of-window
corrections do not count. Position is stored modulo 840 (LCM of 2..8), so
wrap preserves every supported denominator's phase.

Pre reads prior contributing-visit OR history, committed after the visit.
Non-Pre negates that history but never contributes. Skipped gates do not
contribute; no-gate visits preserve history. Scene position resets preserve
state; transport and track resets restore zero cycle and false history.

Tests cover every pair/rotation's hit count, cycle modulo wrap, one-step and
64-step windows in both directions, actual reversals versus reassignment,
window corrections, reset overriding direction restoration, Pre snapshot
timing, OR history and non-Pre non-contribution. Hardware verification,
preview/editor gesture tests and flash-backed persistence remain open.

## Clock, dependency and ordering coverage

The fifth suite links the production MIDI clock timer to the production
engine through host callbacks matching seq_ctrl's routing. It verifies
Start reset, Stop/Continue preservation, requested Reset, and probability
stream progression during 80% swing. Callback routing is supplied by the
harness; real MIDI port reception and physical timing are not covered.

CCi regression tests include backward eligibility chains, cyclic Bias Track
mappings, and reversal of an already prepared Euclidean track. Preparation
is ordered by track number with bounded rescans of unprepared tracks. Each
prepared visit stays frozen for that tick; later CCi changes affect cached
parameters/cycle state immediately, but do not reevaluate its gates.

All sanitized suites fail on undefined-behavior reports. On Apple clang,
`__weak` is explicitly rebound to the C weak-function attribute for the
clock callbacks; its built-in Objective-C meaning is not suitable here.

## Revised probability initialization

Probability streams are initialized once at engine startup from the STM32
hardware random source, with a bounded timing fallback. Musical resets
clear history and loop position, but preserve probability state. Song loads
also preserve probability state. Fixed initialization in host fixtures makes
tests reproducible; it is not the device's startup seed policy. Practical
hardware testing confirmed different random events after reboot.

Chapter 10 adds root/output, pitch integration, song-layout and root-UI suites.
The integration suite runs production engine, arp and output processing together
with song/hardware fixtures. It covers bipolar CCi17, keyboard/Bias offsets,
scene activation, delayed starts, ratchets, voice stealing, live pressure,
repeated attacks and damper cleanup. Layout assertions compare all persistent
field offsets with the c7c4a02 layout. UI tests exercise shared label buffers
for both LCD types; they do not replace a physical LCD audition.

Harmony changes apply to subsequent note attacks; already sounding voices keep
their pitch until release. Legacy Root Off transpose-only revoicing is retained.
