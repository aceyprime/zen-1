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
# The SMK-37 target (BOARD_SMK37, SMK37.md) changes these platform files and adds two HAL headers. Each must be
# exactly the version recorded here (git hash-object): nothing else changes in them unnoticed. Their FM-1 build is
# byte-identical to upstream's.
SMK37_FILES="
7243d70064bae9001e0eedd88565b84da76b6e69 firmware/hal/fm1_cc.h
c5db7c2e6665d267623556177d2afbe939e8be99 firmware/hal/fm1_flash.h
e12a7f6ffefdee861b582d13244fcb5cd788596b firmware/hal/fm1_flash_map.h
2cf23896c943394b23432a0fdaeb550ba6b11c15 firmware/hal/fm1_input.h
f5f0a8c60760cd3935aebd0f9c9fc7b05c747a68 firmware/hal/fm1_lcd_hw.h
e34cb01dd27ea86b540c0647969a0cfe4bbe97a5 firmware/hal/smk_input.h
7e391bbb4e374aee88d2ad3a3623ec571cf60669 firmware/loader/ldr_core.c
1b256b3901d1b67e67657c5dff723e7fadbbc94f firmware/loader/loader.c
cf9a021821a461a91141f76137db07ca904ff3a5 firmware/src/lcd.c
e1e478086e34effede0aa7ea946cb0892a992078 firmware/src/ota.c
39296ee754ca88ed22570b5a6a51f2627cfb8c3c firmware/src/storage.c
ba9449f7bf90b9fe04f7ef4d850161f8c13ff99e firmware/src/usb.c
6d53ec16dba54c17474f15712d7244610f134841 tools/fm1_install.py
79749f44a6eef5be7a6668357dd7041c05f0758e tools/fm1pkg_make.py
2b849d575f0a902391fa6f3a7f768795f960aed9 web/fm1ota.js
12ce2f636bc7a80d04106325029c7b6857f3d9fc web/fm1pkg.js
"
if ! git cat-file -e "$UPSTREAM^{commit}" 2>/dev/null || ! git cat-file -e "$INSTALL_UPSTREAM^{commit}" 2>/dev/null; then
    echo "guard: upstream commits $UPSTREAM / $INSTALL_UPSTREAM are not in this repository (git remote add fomni https://github.com/charlesvestal/fm1-fomni && git fetch fomni)"
    exit 1
fi
fail=0
SMK37_EXCL=""
for f in $(echo "$SMK37_FILES" | awk 'NF == 2 {print $2}'); do
    SMK37_EXCL="$SMK37_EXCL :(exclude)$f"
done
echo "$SMK37_FILES" | awk 'NF == 2' > "${TMPDIR:-/tmp}/guard_smk37.$$"
while read -r h f; do
    if [ "$(git hash-object "$f" 2>/dev/null)" != "$h" ]; then
        echo "guard: $f is not the SMK-37 target's recorded version ($h)"
        fail=1
    fi
done < "${TMPDIR:-/tmp}/guard_smk37.$$"
rm -f "${TMPDIR:-/tmp}/guard_smk37.$$"
# the one line allowed to differ: the USB product name (usb.c STR2: "ZEN-1 FM-1", FoMni's "FoMni FM-1"; the same
# length, so nothing else moves). Nothing else in the platform may change.
ALLOW='^static const uint8_t STR2\[\] = \{22, 3, '
# shellcheck disable=SC2086
if ! git diff --quiet -I"$ALLOW" "$UPSTREAM" -- $PROTECTED $SMK37_EXCL; then
    echo "guard: protected platform files differ from upstream $UPSTREAM:"
    # shellcheck disable=SC2086
    git diff --stat -I"$ALLOW" "$UPSTREAM" -- $PROTECTED $SMK37_EXCL | sed 's/^/       /'
    fail=1
fi
# shellcheck disable=SC2086
if ! git diff --quiet "$INSTALL_UPSTREAM" -- $INSTALLERS $SMK37_EXCL; then
    echo "guard: the installers differ from upstream $INSTALL_UPSTREAM:"
    # shellcheck disable=SC2086
    git diff --stat "$INSTALL_UPSTREAM" -- $INSTALLERS $SMK37_EXCL | sed 's/^/       /'
    fail=1
fi
# shellcheck disable=SC2086
if ! git diff --quiet -I"$ALLOW" -- $PROTECTED $INSTALLERS $SMK37_EXCL; then
    echo "guard: protected platform files have uncommitted changes:"
    # shellcheck disable=SC2086
    git diff --stat -I"$ALLOW" -- $PROTECTED $INSTALLERS $SMK37_EXCL | sed 's/^/       /'
    fail=1
fi
new=$(git ls-files --others --exclude-standard -- firmware/hal firmware/loader $SMK37_EXCL)
if [ -n "$new" ]; then
    echo "guard: new files inside protected directories:"
    echo "$new" | sed 's/^/       /'
    fail=1
fi
[ $fail -eq 0 ] && echo "guard: platform files match upstream $UPSTREAM (the installers $INSTALL_UPSTREAM), the SMK-37 target's as recorded"
exit $fail
