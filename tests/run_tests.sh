#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FuMi-1 host tests: the platform guard, the maths, the platform pieces kept from Felucca / X0X / FoMni,
# the pitch core, the FM6 core, the instrument, the click regression test (and its proof), the no-double
# check, flash storage, the update path against the package, the simulator scenarios (tests/scenarios/*.zen:
# screenshots and audio in build/scenarios/), the browser build, and the Python tools' self-tests.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
FAIL=0
OUT=build/host
mkdir -p "$OUT" build/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
DSP="-O2 -ffp-contract=off -std=c99 -Wall -Wextra -Werror -Wno-unused-function -DFM_HOST -Ifirmware/src/dsp"
run guard sh tools/guard_platform.sh
run fastmath sh -c "$CC -O2 -ffp-contract=off -Wall -Wextra -Werror -o $OUT/fastmath_test tests/host/fastmath_test.c -lm && $OUT/fastmath_test"
run encoder sh -c "$CC -O2 -w -Ifirmware/hal -o $OUT/encoder_test tests/host/encoder_test.c && $OUT/encoder_test"
run uac sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uac_test tests/host/uac_test.c -lm && $OUT/uac_test"
run trs sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/trs_test tests/host/trs_test.c && $OUT/trs_test"
for t in tuning smooth bend fm6 zen click; do
    run "$t" sh -c "$CC $DSP -o $OUT/${t}_test tests/host/${t}_test.c -lm && $OUT/${t}_test"
done
# the click test must fail with the smoothing bypassed, or it tests nothing
run click-proof sh -c "$CC $DSP -DFM_NO_SMOOTH -o $OUT/click_nosmooth tests/host/click_test.c -lm && ! $OUT/click_nosmooth"
run koto-search sh -c "$CC -O2 -std=gnu99 -Wno-unused-function -Ifirmware/src/dsp -o $OUT/koto_search tools/koto_search.c -lm && $OUT/koto_search 1 20 >/dev/null"
# no double anywhere in the firmware's own code (the FPU is single precision)
run no-double sh -c "$CC -fsyntax-only -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -Wno-unused-function -DFM_HOST -Ifirmware/src/dsp firmware/src/dsp/fumi.c && $PY tools/check_no_double.py firmware/src/dsp/*.c firmware/src/dsp/*.h firmware/src/app/*.c firmware/src/app/*.h"
run tables sh -c "$PY tools/gen_fm6_tables.py $OUT/fm6_tables.h >/dev/null && cmp $OUT/fm6_tables.h firmware/src/dsp/fm6_tables.h && $PY tools/fumi_patches.py $OUT/fumi_patches.h >/dev/null && cmp $OUT/fumi_patches.h firmware/src/dsp/fumi_patches.h && $PY tools/zen_patches.py $OUT/zen_patches.h >/dev/null && cmp $OUT/zen_patches.h firmware/src/dsp/zen_patches.h"
# the kanji header is checked in; regenerated and compared when a Noto Sans CJK font is at hand
if $PY -c "import sys; sys.path.insert(0, 'tools'); import gen_kanji; sys.exit(0 if gen_kanji.find_font(None) else 1)" 2>/dev/null; then
    run kanji sh -c "$PY tools/gen_kanji.py $OUT/zen_kanji.h >/dev/null && cmp $OUT/zen_kanji.h firmware/src/app/zen_kanji.h"
else
    echo "  skip kanji (needs a Noto Sans CJK font: ZEN_CJK_FONT)"
fi
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
# the update path, against the newest firmware package, a dev build or a release (Felucca's tests; needs
# ./build.sh). (ZEN-1: release builds are build/zen-1-X.Y.Z.fwsc, so build/zen1.fwsc alone could be stale.)
PKG=$(ls -t build/zen1.fwsc build/zen-1-*.fwsc 2>/dev/null | head -1)
if [ -n "$PKG" ]; then
    echo "  (update path: $PKG)"
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test $PKG"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/zen1.bin > $OUT/old_app.bin && \
        $PY tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc $PKG"
    fi
    run installer $PY tests/install_test.py
    # The SMK-37 build (BOARD_SMK37): the flash map and store owner, the package layout, the OTA staging and
    # the loader windows, on SMK-layout packages made here from the FM-1 binaries (layout and protocol under test)
    run smk37-flash sh -c "$CC -O2 -Wall -DBOARD_SMK37=1 -Ifirmware/hal -o $OUT/smk_flash_test tests/smk_flash_test.c && \
        $OUT/smk_flash_test"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip smk37 package / update path (needs AC79_SDK, as the build)"
    else
    run smk37-package sh -c "$PY tools/fm1pkg_make.py --board smk37 build/zen1.bin build/loader/ota.bin \
        $OUT/smk_new.fwsc >/dev/null && $PY tests/smk_pkg_test.py $OUT/smk_new.fwsc"
    run smk37-ota-entry sh -c "$CC -DBOARD_SMK37=1 -o $OUT/ota_test_smk tests/ota_test.c && \
        $OUT/ota_test_smk $OUT/smk_new.fwsc"
    for m in 1 2; do                     # the MODEL=elite / mke builds of ota.c
        run smk37-ota-entry-model$m sh -c "$CC -DBOARD_SMK37=1 -DT_MODEL=$m -o $OUT/ota_test_smk_m$m tests/ota_test.c && \
            $OUT/ota_test_smk_m$m $OUT/smk_new.fwsc"
    done
    [ -f build/zen1-smk37.fwsc ] && run smk37-build-package python3 tests/smk_pkg_test.py build/zen1-smk37.fwsc "SMK-37 Pro_460"
    [ -f build/zen1-smk37-elite.fwsc ] && run smk37-build-package-elite python3 tests/smk_pkg_test.py build/zen1-smk37-elite.fwsc "SMK-37 Elite_460"
    [ -f build/zen1-smk37-mke.fwsc ] && run smk37-build-package-mke python3 tests/smk_pkg_test.py build/zen1-smk37-mke.fwsc "MKE-P37_460"
    run smk37-update-loader sh -c "head -c 100000 build/zen1.bin > $OUT/old_app.bin && \
        $PY tools/fm1pkg_make.py --board smk37 $OUT/old_app.bin build/loader/ota.bin $OUT/smk_old.fwsc >/dev/null && \
        $CC -DBOARD_SMK37=1 -o $OUT/ldr_test_smk tests/ldr_test.c && $OUT/ldr_test_smk $OUT/smk_old.fwsc $OUT/smk_new.fwsc"
    fi
else
    echo "  skip update-path tests (no build/zen1.fwsc: run ./build.sh)"
fi
run host-build sh host/build_host.sh
command -v node >/dev/null 2>&1 && run web-installer node web/test_web.mjs   # fm1ota.js / fm1pkg.js against a simulated device
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh web/emu/build.sh >/dev/null && node tests/host/emu_test.mjs build/emu/zen1.wasm"
    run bench node tests/host/bench_test.mjs build/emu/zen1.wasm
    run syx-js node tests/host/syx_test.mjs
    run session-js node tests/host/session_test.mjs
fi
if command -v node >/dev/null 2>&1; then
    run page-scripts $PY tests/page_script_test.py
fi
for s in tests/scenarios/*.zen; do
    n=$(basename "$s" .zen)
    mkdir -p "build/scenarios/$n"
    run "scenario-$n" build/host/zen_host "$s" "build/scenarios/$n"
done
# the SMK-37 build's UI in the simulator: its keys, pads and their lights, faders, wheels and extra buttons
run host-build-smk37 sh host/build_host.sh smk37
for s in tests/scenarios/smk37/*.zen; do
    n=$(basename "$s" .zen)
    mkdir -p "build/scenarios/smk37-$n"
    run "scenario-smk37-$n" build/host/zen_host_smk37 "$s" "build/scenarios/smk37-$n"
done
run syx-dump $PY tools/syx_dump.py --selftest
run analyze-ref $PY tools/analyze_ref.py --selftest
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
