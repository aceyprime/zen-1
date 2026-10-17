#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The site's gallery: every scene and mood, from the host simulator (tools/gallery.zen), into docs/img/gallery.
# Built as a release is (ZN_HUD=0: no testing tools, the status card without its raw battery reading), then the host
# built back as the tests have it.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc} -DZN_HUD=0" sh host/build_host.sh >/dev/null
mkdir -p docs/img/gallery
build/host/zen_host tools/gallery.zen docs/img/gallery | tail -1
rm -f docs/img/gallery/midi_out.txt
sh host/build_host.sh >/dev/null
