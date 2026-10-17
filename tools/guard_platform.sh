#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FuMi-1 platform guard: the paths below are FoMni's platform (Felucca's HAL, update loader, USB,
# storage, screen driver, packaging and install tools) and are kept byte-for-byte as upstream. The
# rule, from the project's flashing-safety notes: all FuMi code lives in firmware/src/app/ and
# firmware/src/dsp/. This script diffs the protected paths against the upstream commit recorded in
# docs/upstream.md and fails if anything differs or a new file appeared inside a protected directory.
#   tools/guard_platform.sh            (run by tests/run_tests.sh and before every build)
set -u
cd "$(dirname "$0")/.."
UPSTREAM=b81eb6b661c35948ec2d1059664d4e5790999305
PROTECTED="firmware/hal firmware/loader firmware/crt0.S firmware/app.ld
firmware/src/ota.c firmware/src/usb.c firmware/src/storage.c firmware/src/lcd.c firmware/src/gfx.c
firmware/src/libc.c firmware/src/midi_uart.c
tools/fm1pkg_make.py tools/fm1_rescue.py tools/fm1_rescue.sh tools/lz4blk.py
tools/get_toolchain.sh web/fm1pkg.js"
# the installers follow a later FoMni commit: back to stock from M-VAVE's FM1 Open firmware (6c32623)
INSTALL_UPSTREAM=6c326230af03c66a77ba68e1af72cf3341e34208
INSTALLERS="tools/fm1_install.py web/fm1ota.js"
if ! git cat-file -e "$UPSTREAM^{commit}" 2>/dev/null || ! git cat-file -e "$INSTALL_UPSTREAM^{commit}" 2>/dev/null; then
    echo "guard: upstream commits $UPSTREAM / $INSTALL_UPSTREAM are not in this repository (git remote add fomni https://github.com/charlesvestal/fm1-fomni && git fetch fomni)"
    exit 1
fi
fail=0
# the one line allowed to differ: the USB product name (usb.c STR2: "ZEN-1 FM-1", FoMni's "FoMni FM-1"; the same
# length, so nothing else moves). Nothing else in the platform may change.
ALLOW='^static const uint8_t STR2\[\] = \{22, 3, '
# shellcheck disable=SC2086
if ! git diff --quiet -I"$ALLOW" "$UPSTREAM" -- $PROTECTED; then
    echo "guard: protected platform files differ from upstream $UPSTREAM:"
    # shellcheck disable=SC2086
    git diff --stat -I"$ALLOW" "$UPSTREAM" -- $PROTECTED | sed 's/^/       /'
    fail=1
fi
# shellcheck disable=SC2086
if ! git diff --quiet "$INSTALL_UPSTREAM" -- $INSTALLERS; then
    echo "guard: the installers differ from upstream $INSTALL_UPSTREAM:"
    # shellcheck disable=SC2086
    git diff --stat "$INSTALL_UPSTREAM" -- $INSTALLERS | sed 's/^/       /'
    fail=1
fi
# shellcheck disable=SC2086
if ! git diff --quiet -I"$ALLOW" -- $PROTECTED $INSTALLERS; then
    echo "guard: protected platform files have uncommitted changes:"
    # shellcheck disable=SC2086
    git diff --stat -I"$ALLOW" -- $PROTECTED $INSTALLERS | sed 's/^/       /'
    fail=1
fi
new=$(git ls-files --others --exclude-standard -- firmware/hal firmware/loader)
if [ -n "$new" ]; then
    echo "guard: new files inside protected directories:"
    echo "$new" | sed 's/^/       /'
    fail=1
fi
[ $fail -eq 0 ] && echo "guard: platform files match upstream $UPSTREAM (the installers $INSTALL_UPSTREAM)"
exit $fail
