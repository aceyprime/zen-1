#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ZEN-1's own FM6 patches, for the scenes only -> firmware/src/dsp/zen_patches.h

  tools/zen_patches.py firmware/src/dsp/zen_patches.h

These are not instruments: PRESETS never reaches them, and they need no Japanese name on the screen. A scene
picks one with fm_part_patch(part, ZN_PATCH_BASE + n). Written as readable operator settings with the helpers of
tools/fumi_patches.py; no ROM data.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fumi_patches import op, voice, pack  # noqa: E402

# Soft Pad: the drone. (it was barely heard even at 100 - its tones lay under ~300 Hz, which
# the FM-1's speaker hardly plays. The modulators now give it partials 2..4: ~60 % of it in 300 Hz..2 kHz, ~1 %
# above, a warm "ooh" rather than the Sho's buzz.) On the FM-1 the Sho drone was too sharp and loud, "more like a fan
# whine". So: three nearly pure stacks (algorithm 5, low modulation index: a touch of warmth, no buzz), the
# second a few cents sharp of the first so the two drift in and out (a slow shimmer, not a beat), the third an
# octave up and quiet; carrier levels ~19 steps (~14 dB) under full: it sits beneath the koto, never on it; every envelope slow to rise (seconds) and level while held; a slow, shallow LFO on the
# loudness of the carriers. No velocity response.
PATCHES = [
    ("SOFT_PAD", voice("Soft Pad", 5, [
        op(r=(28, 40, 99, 35), l=(99, 96, 96, 0), ol=86, ams=1),                    # OP1 carrier
        op(r=(24, 40, 99, 35), l=(99, 92, 92, 0), ol=92, fc=1),                     # OP2: partials 2..4, where the speaker plays
        op(r=(25, 40, 99, 35), l=(99, 96, 96, 0), ol=82, det=10, ams=1),            # OP3 carrier, a few cents up
        op(r=(22, 40, 99, 35), l=(99, 92, 92, 0), ol=86, fc=2),                     # OP4: the 2nd and 3rd partials
        op(r=(20, 40, 99, 35), l=(99, 94, 94, 0), ol=80, fc=2, det=5, ams=1),       # OP5: the octave
        op(r=(20, 40, 99, 35), l=(80, 80, 80, 0), ol=0, fc=1),                      # OP6: silent
    ], fb=0, oks=0, lfs=12, lfd=0, lpmd=0, lamd=14, lks=0, lfw=4, lpms=0, trnsp=24)),
    # Soft Swell: a note that blooms rather than plucks (ambalek's "fall" plays its leaves with a slow attack and a
    # long release). A round carrier over a gentle 2:1 modulator, rising in ~0.4 s, then a long fall to nothing
    # (it rings ~4 s by itself, no sustain); a quieter octave stack with a touch of 3rd partial for air; a little
    # velocity response. For the distant petals, the heavy falls and the night.
    ("SOFT_SWELL", voice("Soft Swell", 5, [
        op(r=(52, 30, 20, 30), l=(99, 90, 0, 0), ol=92, kvs=2),                     # OP1 carrier: bloom, long fall
        op(r=(50, 28, 18, 30), l=(94, 82, 0, 0), ol=58, fc=2, kvs=2),               # OP2: warmth, falls with it
        op(r=(48, 30, 20, 30), l=(99, 88, 0, 0), ol=68, fc=2, det=9),               # OP3: the octave, quiet
        op(r=(45, 25, 15, 30), l=(90, 76, 0, 0), ol=44, fc=3),                      # OP4: a little air
        op(r=(50, 30, 20, 30), l=(99, 90, 0, 0), ol=70, det=5),                     # OP5: a slow chorus on OP1
        op(r=(50, 30, 20, 30), l=(80, 80, 0, 0), ol=0),                             # OP6: silent
    ], fb=0, oks=0, lfs=20, lfd=0, lpmd=0, lamd=0, lks=0, lfw=0, lpms=0, trnsp=24)),
    # ---- each scene its own sounds. Sakura keeps the
    # koto (spring, the classic); the pond gets water voices, a bowl, a deeper drone and a wooden knock; Sakura's
    # gusts a wind chime. Loudness matched to the Koto at the same velocity (tools/zen_voicing.c, within ~2 dB).
    # Deep Pad: the pond's drone. Soft Pad's stacks, darker: the modulators lower (fewer upper partials), the
    # octave stack an octave *under* instead (a depth on headphones; the FM-1's speaker still gets the body's
    # 2nd..3rd partials, so it is heard), a slower LFO.
    ("DEEP_PAD", voice("Deep Pad", 5, [
        op(r=(26, 40, 99, 33), l=(99, 96, 96, 0), ol=89, ams=1),                    # OP1 carrier
        op(r=(22, 40, 99, 33), l=(99, 92, 92, 0), ol=88, fc=1),                     # OP2: partials 2..3, softer
        op(r=(23, 40, 99, 33), l=(99, 96, 96, 0), ol=85, det=10, ams=1),            # OP3 carrier, a few cents up
        op(r=(20, 40, 99, 33), l=(99, 92, 92, 0), ol=80, fc=2),                     # OP4: the 2nd partial, less
        op(r=(18, 40, 99, 33), l=(99, 94, 94, 0), ol=80, fc=0, det=5, ams=1),       # OP5: the octave under (ratio 0.5)
        op(r=(20, 40, 99, 33), l=(80, 80, 80, 0), ol=0, fc=1),                      # OP6: silent
    ], fb=0, oks=0, lfs=8, lfd=0, lpmd=0, lamd=14, lks=0, lfw=4, lpms=0, trnsp=24)),
    # Drop: a water drop, "plip": a near-sine that starts a little flat and rises into tune in ~30 ms (the pitch
    # envelope from L4 to L1), a short glassy 3rd partial for the splash, a body that fades in about a second.
    ("DROP", voice("Drop", 5, [
        op(r=(99, 46, 28, 40), l=(99, 76, 0, 0), ol=99, kvs=2),                     # OP1 carrier: the drop
        op(r=(99, 72, 45, 50), l=(99, 40, 0, 0), ol=68, kvs=2),                     # OP2: a moment of brightness
        op(r=(99, 78, 50, 50), l=(99, 0, 0, 0), ol=78, fc=3, kvs=2),                # OP3: the splash (3rd partial, short)
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP4: off
        op(r=(99, 40, 26, 40), l=(99, 74, 0, 0), ol=88, det=10),                    # OP5: a little chorus on OP1
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP6: off
    ], fb=0, oks=0, pr=(82, 99, 99, 99), pl=(50, 50, 50, 43), lfs=30, lfd=0, lpmd=0, lamd=0, lks=0, lfw=0, lpms=0,
       trnsp=24)),
    # Mallet: a soft wooden bar (marimba, kalimba): the fundamental with a little warmth, and the bar's 4th
    # partial, which dies in a few tens of ms: the "tock" of a felt mallet, never a click.
    ("MALLET", voice("Mallet", 5, [
        op(r=(99, 46, 26, 45), l=(99, 78, 0, 0), ol=99, kvs=2),                     # OP1 carrier: the bar
        op(r=(99, 80, 50, 50), l=(99, 30, 0, 0), ol=62, kvs=2),                     # OP2: warmth at the strike
        op(r=(99, 82, 55, 50), l=(99, 0, 0, 0), ol=80, fc=4, kvs=3),                # OP3: the 4th partial, short
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP4: off
        op(r=(99, 42, 24, 45), l=(99, 82, 0, 0), ol=82, det=9),                     # OP5: the body, a touch apart
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP6: off
    ], fb=0, oks=0, lfs=30, lfd=0, lpmd=0, lamd=0, lks=0, lfw=0, lpms=0, trnsp=24)),
    # Bell: glassy and gentle: a carrier rounded by a modulator at 3.5 (the bell's clang, falling fast), a second
    # carrier at 2.76 (glass), a slow chorus.
    ("BELL", voice("Bell", 5, [
        op(r=(99, 30, 20, 30), l=(99, 84, 0, 0), ol=95, kvs=2),                     # OP1 carrier
        op(r=(99, 44, 26, 30), l=(99, 50, 0, 0), ol=60, fc=3, ff=17, kvs=3),        # OP2: the clang (3.5), falls
        op(r=(99, 40, 26, 30), l=(99, 64, 0, 0), ol=72, fc=2, ff=38, kvs=2),        # OP3: glass (2.76)
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP4: off
        op(r=(99, 28, 18, 30), l=(99, 86, 0, 0), ol=82, det=10),                    # OP5: the chorus
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP6: off
    ], fb=0, oks=0, lfs=30, lfd=0, lpmd=0, lamd=0, lks=0, lfw=0, lpms=0, trnsp=24)),
    # Rin: a singing bowl struck softly: no hard edge (the attack ~20 ms), partials at 1 and 2.7 with a slow beat
    # between two fundamentals a few cents apart, a faint 5th mode, a long ring (~8 s); a slow wobble of the
    # loudness. For the pond's soft events: a koi rising, a pebble, two passing, the memories (soft velocities
    # soften it more, so a memory still blooms). ~3 dB down, and
    # less velocity response, so the loud ones are nearer the soft ones.
    ("RIN", voice("Rin", 5, [
        op(r=(80, 22, 14, 25), l=(99, 86, 0, 0), ol=88, kvs=1, ams=1),              # OP1 carrier
        op(r=(85, 30, 20, 30), l=(99, 50, 0, 0), ol=44),                            # OP2: a little warmth
        op(r=(80, 25, 16, 25), l=(99, 80, 0, 0), ol=74, fc=2, ff=35, det=9, kvs=1), # OP3: the 2.7 mode
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP4: off
        op(r=(80, 22, 14, 25), l=(99, 86, 0, 0), ol=82, det=12, kvs=1, ams=1),      # OP5: the beat against OP1
        op(r=(85, 32, 22, 30), l=(99, 50, 0, 0), ol=34, fc=5, ff=10),               # OP6: the 5th mode, faint
    ], fb=0, oks=0, lfs=24, lfd=0, lpmd=0, lamd=8, lks=0, lfw=0, lpms=0, trnsp=24)),
    # Bamboo: the shishi-odoshi's knock: hollow wood, "tok": two inharmonic resonances (1 and 2.6) gone in ~0.15 s,
    # a click of noise (feedback) in the first few ms. At the note's pitch: an octave down (ratio 0.5) put it at
    # ~185 Hz, under what the FM-1's speaker plays (the Taiko knock was 98 % there; so it hardly sounded).
    # Still too quiet, so its tube and 2.6 mode ring longer (~0.25 s), the mode louder.
    ("BAMBOO", voice("Bamboo", 5, [
        op(r=(99, 48, 60, 60), l=(99, 0, 0, 0), ol=99, kvs=1),                      # OP1 carrier: the tube
        op(r=(99, 85, 70, 70), l=(99, 0, 0, 0), ol=70, fc=1, ff=50),                # OP2: knock texture (1.5)
        op(r=(99, 54, 70, 70), l=(99, 0, 0, 0), ol=94, fc=2, ff=30),                # OP3: the 2.6 mode
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP4: off
        op(r=(99, 96, 90, 90), l=(99, 0, 0, 0), ol=72, fc=3),                       # OP5: the click
        op(r=(99, 96, 90, 90), l=(99, 0, 0, 0), ol=80, fc=7),                       # OP6: noise, by feedback
    ], fb=7, oks=0, lfs=30, lfd=0, lpmd=0, lamd=0, lks=0, lfw=0, lpms=0, trnsp=24)),
    # Furin: a glass wind chime, an octave up (by the ratios), soft: a round fundamental, the 2.76 glass mode, a
    # faint 5.4 shimmer that fades first (nothing bright that lasts: the brook's birds were "distracting").
    ("FURIN", voice("Furin", 5, [
        op(r=(99, 34, 22, 34), l=(99, 80, 0, 0), ol=97, fc=2, kvs=2),               # OP1 carrier (2)
        op(r=(99, 50, 30, 34), l=(99, 40, 0, 0), ol=48, fc=4, kvs=2),               # OP2: a little edge, falls
        op(r=(99, 44, 30, 34), l=(99, 60, 0, 0), ol=82, fc=5, ff=10, det=9),        # OP3: glass (2 x 2.76)
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP4: off
        op(r=(99, 64, 40, 40), l=(99, 0, 0, 0), ol=46, fc=10, ff=8),                # OP5: the shimmer (2 x 5.4), short
        op(r=(99, 99, 99, 99), l=(0, 0, 0, 0), ol=0),                               # OP6: off
    ], fb=0, oks=0, lfs=30, lfd=0, lpmd=0, lamd=0, lks=0, lfw=0, lpms=0, trnsp=24)),
    # (Hotaru's Frog and Owl patches went: recordings call now; the shakuhachi the owl
    # replaced had fixed-frequency operators, "Stranger Things creepy")
    # (the earlier Whistle for the bush warbler went: a recording sings now)
]


def main(path):
    L = ["/* generated by tools/zen_patches.py: ZEN-1's own patches for the scenes, packed 128-byte voices */",
         "#pragma once", "#include <stdint.h>", "#define ZN_PATCH_BASE 100              /* fm_part_patch slots from here */",
         f"#define ZN_NPATCH {len(PATCHES)}"]
    L += [f"#define ZN_{key} (ZN_PATCH_BASE + {i})" for i, (key, _) in enumerate(PATCHES)]
    L.append("#ifdef ZN_PATCH_DATA                    /* the engine (fumi.c) only: the scenes need the ids */")
    L.append("static const uint8_t ZN_PATCH[ZN_NPATCH][128] = {")
    for key, v in PATCHES:
        b = pack(v)
        L.append(f"    {{ /* {key} */")
        for i in range(0, 128, 16):
            L.append("        " + ", ".join(f"{x:3d}" for x in b[i:i + 16]) + ",")
        L.append("    },")
    L += ["};", "#endif", ""]
    Path(path).write_text("\n".join(L))
    print(f"patches: {path} ({len(PATCHES)})")


if __name__ == "__main__":
    main(sys.argv[1])
