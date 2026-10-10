#!/bin/bash
# Generate and build the CARBON release image while retaining a useful log.
# This script does not flash the device.

set -u
set -o pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR" || exit 1

if command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 1)
elif command -v getconf >/dev/null 2>&1; then
    JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
else
    JOBS=1
fi

TIMESTAMP=$(date '+%Y%m%d-%H%M%S')
LOG_DIR="build/logs"
LOG_FILE="$LOG_DIR/release-$TIMESTAMP.log"
SUMMARY_FILE="$LOG_DIR/release-$TIMESTAMP-summary.log"
mkdir -p "$LOG_DIR"

# Prevent a failed build from being mistaken for an older successful image.
rm -f main main.bin main.map Makefile

printf 'CARBON release build\n'
printf 'Started: %s\n' "$(date)"
printf 'Directory: %s\n' "$SCRIPT_DIR"
printf 'Git: %s%s\n' \
    "$(git rev-parse --short HEAD 2>/dev/null || echo unknown)" \
    "$(git diff --quiet --ignore-submodules HEAD 2>/dev/null || printf ' (dirty)')"
printf 'Jobs: %s\n' "$JOBS"
printf 'Full log: %s\n\n' "$LOG_FILE"

{
    ./makegen-run-release.sh &&
    test -s Makefile &&
    make -j"$JOBS"
} 2>&1 | tee "$LOG_FILE"
BUILD_STATUS=${PIPESTATUS[0]}

{
    printf '\n=== Release build summary ===\n'
    printf 'BUILD_EXIT=%s\n' "$BUILD_STATUS"

    if [ "$BUILD_STATUS" -eq 0 ] && [ -f main ] && [ -f main.bin ]; then
        arm-none-eabi-size main
        printf 'main.bin bytes: %s\n' "$(wc -c < main.bin | tr -d '[:space:]')"
        if command -v shasum >/dev/null 2>&1; then
            shasum -a 256 main.bin
        elif command -v sha256sum >/dev/null 2>&1; then
            sha256sum main.bin
        else
            printf 'SHA-256 unavailable (install shasum or sha256sum)\n'
        fi
    else
        printf 'No successful release artifact was produced.\n'
    fi

    printf '\nWarnings/errors:\n'
    grep -E 'warning:|error:|region .* overflowed|undefined reference' "$LOG_FILE" || true
    printf '\nFull log: %s\n' "$LOG_FILE"
} | tee "$SUMMARY_FILE"

printf 'Summary: %s\n' "$SUMMARY_FILE"
exit "$BUILD_STATUS"
