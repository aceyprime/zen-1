#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the FuMi-1 host simulator (the whole app on this machine) into build/host/zen_host.
# Same sources as the firmware, -ffp-contract=off like the device (no fused multiply-add).
# host/build_host.sh smk37: the SMK-37 build's UI (-DBOARD_SMK37=1) into build/host/zen_host_smk37.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
OUTB=build/host/zen_host
DEFS=
if [ "${1:-}" = smk37 ]; then
    OUTB=build/host/zen_host_smk37
    DEFS=-DBOARD_SMK37=1
fi
mkdir -p build/gen build/host
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
# -D_POSIX_C_SOURCE: clock_gettime under -std=c99 on glibc (the build failed on Linux)
$CC -O2 -ffp-contract=off -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    $DEFS -DOM_HOST -DFM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen -o $OUTB host/zen_host.c \
    firmware/src/dsp/fumi.c -lm
echo "host: $OUTB"
