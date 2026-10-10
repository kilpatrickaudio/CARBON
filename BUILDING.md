# Building the unofficial CARBON firmware

These instructions describe the release build for `1.29-johanwk`. This is
unofficial firmware, not a Kilpatrick Audio release. See also the
[user guide](carbon-fw-2026-johanw.org).

## Requirements

The development firmware was built on macOS with:

- Arm GNU Toolchain **15.3.Rel1**, GCC **15.3.1** (arm-none-eabi).
- A host C compiler (Apple Clang was used) to compile `makegen`.
- Bash, Git, Make, and standard Unix command-line utilities.
- Patched `makegen`, as described below.
- `shasum` or `sha256sum` to record the binary checksum.
- `dfu-util` only for optional STM32 ROM DFU flashing.

Install the bare-metal **arm-none-eabi** toolchain, not an Arm Linux compiler.
Put its `bin` directory and your installed `makegen` on `PATH`. The scripts
resolve tools through `PATH`; no particular installation directory is required.
For example, substitute your actual toolchain directory:

```sh
export PATH=/path/to/arm-gnu-toolchain/bin:$HOME/bin:$PATH
command -v makegen arm-none-eabi-gcc arm-none-eabi-as arm-none-eabi-objcopy arm-none-eabi-size
arm-none-eabi-gcc --version
```

Quote paths containing spaces when setting `PATH`. The toolchain supplies the
assembler, linker, Newlib, `objcopy`, and `size`. Other compiler versions have
not been established as producing the same binary; these instructions are not
a claim of bit-for-bit reproducibility across hosts.

## Build and install patched makegen

Source: <https://github.com/hires/makegen>

The required fix is described in [makegen PR #1](https://github.com/hires/makegen/pull/1):
“Send stderr to console, not Makefile (needed when tools emit warnings).”
Unpatched `makegen` redirects compiler diagnostics into the generated Makefile,
which can make modern-toolchain builds fail.

The tested local patched commit was
`fef8525b74dfba4b5873aac65d1552be602407cd`, based on upstream
`f54c6e8e161a484938ed92288efd04772641d031`. The patched commit is local;
do not expect to fetch that commit from upstream. Reproduce its source in a
separate tool checkout as follows:

```sh
git clone https://github.com/hires/makegen.git makegen-tool
cd makegen-tool
git checkout --detach f54c6e8e161a484938ed92288efd04772641d031
```

In `makegen.c`, remove the literal ` 2>&1` from the two compiler command strings
in `print_source_targets`: one for C dependencies and one for C++ dependencies
(lines 452 and 469 in this revision). Leave the rest of the source unchanged.
This is exactly the two-line change in the tested local patched commit.
Check the change with `git diff`, then compile and install:

```sh
cc -o makegen makegen.c
mkdir -p ~/bin
install -m 755 makegen ~/bin/makegen
```

Ensure `~/bin` is on `PATH` and that `command -v makegen` selects this patched
executable, not another installation. Return to the CARBON repository for the
following commands.

## Build CARBON

Run from the CARBON repository root:

```sh
./build-release.sh
```

The wrapper generates the Makefile, builds in parallel, and returns the build
exit status. It **does not flash hardware**. Outputs are:

- `main`: ARM ELF executable.
- `main.bin`: application image.
- `main.map`: link map.
- `build/`: object files and timestamped logs under `build/logs/`.

The summary records build status, text/data/BSS sizes, binary byte count,
SHA-256 when a checksum tool is available, and matching warnings/errors.
The wrapper prints the Git revision and tracked dirty state to the console;
record those with the compiler version when identifying a candidate binary.

The wrapper removes old final artifacts and regenerates the Makefile, but
retains object files. After changing branches or toolchains, force all targets
to rebuild rather than relying on stale objects:

```sh
MAKEFLAGS=-B ./build-release.sh
```

Build from the intended committed source revision. Generated binaries and logs
are ignored by Git and should not be committed.

## Release address and optional flashing

The release configuration uses `VTOR_BL` and
`src/STM32F407VE_FLASH-release.ld`, with the application vector table at
**0x08020000**. This leaves the MIDI bootloader below that address intact.
Before flashing, inspect `main.map` to confirm `.isr_vector` is at that address.

Back up songs first. Compatibility with older firmware after saving songs using
new event types or step lengths is not guaranteed.

For the verified ROM DFU procedure, hold the power button while applying power.
Use `dfu-util -l` to confirm device `0483:df11` and alt setting 0, then run from
the directory containing the intended `main.bin`:

```sh
dfu-util -d 0483:df11 -a 0 \
  --dfuse-address 0x08020000:leave \
  -D main.bin
```

**Do not use the stock `dfu.sh` for this release image.** It writes at
`0x08000000` and can overwrite the MIDI bootloader. The development build
variant also uses `0x08000000`; it is not the bootloader-preserving release
configuration documented here. Do not assume KAUpdate/WebMIDI installation
has been verified for this unofficial image.

Allow CARBON to restart and check its version display, normal playback, MIDI,
and CV/gate operation before relying on it. Firmware installation and use are
at your own risk.
