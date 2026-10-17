#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ZEN-1's recorded sounds -> firmware/src/dsp/zen_samples.h (looped, IMA ADPCM in blocks, in flash).

  tools/zen_samples.py firmware/src/dsp/zen_samples.h [--rate 16000]

The loops are coded at 16 kHz (the room in flash; the FM-1's speaker plays little above 8 kHz).
`--rate 22050` makes the earlier set again from the same recordings (the originals stay in assets/sounds/).

Each sound is a public-domain (CC0) recording kept in assets/sounds/ with its source noted below. A segment is
cut where the recording is steadiest, made into a seamless loop (its last 0.5 s crossfades, equal power, into its
start), resampled to 22050 Hz mono, normalized, and coded as 4-bit IMA ADPCM in blocks of ZN_BLOCK samples, each
block starting with its own predictor and step index so playback can start anywhere (bed.c plays two heads, one a
side, half a loop apart). Needs ffmpeg, numpy, scipy.
"""
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
RATE = 16000                       # (--rate)
BLOCK = 1024                      # samples a block: 4 header bytes + 512 data bytes

# (key, file, start s, length s, high-pass Hz (0: none), source). Each loop is normalized to the brook's loudness
# (RMS), so the beds' gains in bed.c hold whichever is chosen.
SOUNDS = [
    # 41.5 s + 8.5 s (+ the 0.5 s crossfade): the stretch without birds. 36.5 s + 10 s caught a burst of
    # chirps at 40.2-40.7 s ; bird calls found as narrow peaks in
    # 2.5-8 kHz standing 8 dB above the band's median, none here
    ("BROOK", "assets/sounds/stream-446019-preview.mp3", 41.5, 8.5, 0,
     "Gentle Stream Natural Stream Sound, BurghRecords, https://freesound.org/s/446019/ (CC0 1.0)"),
    # the steadiest 6 s of the recording (0.65 dB), no birds, no knocks, little rumble
    ("BREEZE", "assets/sounds/breeze-523389-preview.mp3", 106.3, 6.0, 120,
     "Forest, close up of trees rustling in the wind, Anya_Media, https://freesound.org/s/523389/ (CC0 1.0)"),
    # crickets; their background hum (half the energy, under 1 kHz) taken away above 1.2 kHz
    ("INSECTS", "assets/sounds/crickets-522299-preview.mp3", 196.5, 6.0, 1200,
     "Crickets At Night - Raw sound, Defelozedd94, https://freesound.org/s/522299/ (CC0 1.0)"),
]

# One-shots: single calls, played whole at a scene's moment (bed.c's shot players), at a lower rate (their
# energy is below ~2.5 kHz): (key, file, rate, high-pass Hz, [(start s, end s), ...], source). All the calls of a key
# share one gain (the loudest at -1 dBFS), so their own differences stay.
SHOT_RATE = 11025
ONESHOTS = [
    # a UK common frog's purring croaks, ~10 pulses a second at ~400-500 Hz: the "pond frog" liked; six
    # calls, short and long, each with its own shape (found by their energy: the floor -75 dB, the calls -37..-53)
    ("FROG", "assets/sounds/frog-532235-preview.mp3", SHOT_RATE, 150,
     [(0.85, 1.22), (2.36, 2.72), (5.52, 6.00), (11.42, 12.50), (21.42, 22.40), (34.33, 35.18)],
     "Frog Croaking (UK Common Frog), shaunhillyard, https://freesound.org/s/532235/ (CC0 1.0)"),
    # a bush warbler (uguisu) near Yokohama: four songs, each a long low "hoo" (~0.9-1.2 kHz) and a quick bright
    # "hokekyo" (2-3 kHz), found by the two bands' energy; the city's traffic below them taken away (high-pass 700 Hz)
    ("BIRD", "assets/sounds/uguisu-524526-preview.mp3", SHOT_RATE, 700,
     [(0.0, 2.3), (11.4, 12.7), (19.6, 22.3), (28.5, 30.2)],
     "Japanese bush warbler (uguisu) in a city ambiance, warukunai, https://freesound.org/s/524526/ (CC0 1.0)"),
    # Koi's shishi-odoshi knock (the FM one was "bell-like"): a long hollow tube flicked with a finger, two of
    # its deeper hits (~470 Hz, gone in ~60 ms): a hollow bamboo "tok"
    ("KNOCK", "assets/sounds/tube-140694-preview.mp3", SHOT_RATE, 120, [(1.10, 1.45), (2.01, 2.40)],
     "Packaging Paper Hollow Tube Percussion Hit, afleetingspeck, https://freesound.org/s/140694/ (CC0 1.0)"),
    # Hotaru's owl (the synthesized hoot sat at ~300 Hz and was never heard): tawny owls, hoots at ~800-900 Hz
    ("OWL", "assets/sounds/owl-465697-preview.mp3", SHOT_RATE, 250, [(0.10, 4.30)],
     "Owl Hoot, Breviceps, https://freesound.org/s/465697/ (CC0 1.0)"),
    ("OWL2", "assets/sounds/owl-454842-preview.mp3", SHOT_RATE, 250, [(2.60, 3.75), (8.95, 10.45)],
     "HOOT.wav, thegoosington, https://freesound.org/s/454842/ (CC0 1.0)"),
]

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
        107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
        4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
        22385, 24623, 27086, 29794, 32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]


def decode_file(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-ac", "1", "-ar", str(RATE), "-f", "s16le", "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0


def make_loop(x, start, length, hp=0, fade=0.5):
    a, n, f = int(start * RATE), int(length * RATE), int(fade * RATE)
    seg = x[max(0, a - RATE):a + n + f + RATE].copy()          # (a second either side for the filter to settle)
    if hp:
        from scipy.signal import butter, sosfiltfilt
        seg = sosfiltfilt(butter(4, hp, "highpass", fs=RATE, output="sos"), seg)
    seg = seg[a - max(0, a - RATE):][:n + f]
    t = np.linspace(0.0, 1.0, f)
    head, tail = seg[:f], seg[n:n + f]
    seg[:f] = head * np.sin(0.5 * np.pi * t) + tail * np.cos(0.5 * np.pi * t)   # the tail flows into the start
    seg = seg[:n]
    seg -= seg.mean()
    return seg


def adpcm_block(s):
    """one block: predictor (int16 LE), index, 0, then 2 samples a byte (low nibble first); the first sample is the
    predictor itself"""
    pred, idx = int(s[0]), 0
    # a good starting step: the size of the first differences
    d0 = np.abs(np.diff(s[:64])).mean() if len(s) > 1 else 0
    while idx < 88 and STEP[idx] < d0:
        idx += 1
    out = bytearray([pred & 255, (pred >> 8) & 255, idx, 0])
    nib = []
    for v in s[1:]:
        step, diff, code = STEP[idx], int(v) - pred, 0
        if diff < 0:
            code, diff = 8, -diff
        dq = step >> 3
        if diff >= step:
            code |= 4
            diff -= step
            dq += step
        if diff >= step >> 1:
            code |= 2
            diff -= step >> 1
            dq += step >> 1
        if diff >= step >> 2:
            code |= 1
            dq += step >> 2
        pred = max(-32768, min(32767, pred - dq if code & 8 else pred + dq))
        idx = max(0, min(88, idx + INDEX[code & 7]))
        nib.append(code)
    nib.append(0)                                                  # (pad: BLOCK - 1 codes, an even count)
    for i in range(0, BLOCK - 1, 2):
        out.append(nib[i] | nib[i + 1] << 4)
    return out


def adpcm_decode(data, n):
    """the C decoder's twin, for the self-check"""
    y = []
    for b in range(0, len(data), 4 + BLOCK // 2):
        blk = data[b:b + 4 + BLOCK // 2]
        pred = int.from_bytes(blk[0:2], "little", signed=True)
        idx = blk[2]
        y.append(pred)
        for k in range(1, BLOCK):
            byte = blk[4 + (k - 1) // 2]
            code = (byte >> (4 * ((k - 1) & 1))) & 15
            step = STEP[idx]
            dq = step >> 3
            if code & 4:
                dq += step
            if code & 2:
                dq += step >> 1
            if code & 1:
                dq += step >> 2
            pred = max(-32768, min(32767, pred - dq if code & 8 else pred + dq))
            idx = max(0, min(88, idx + INDEX[code & 7]))
            y.append(pred)
    return np.array(y[:n], dtype=np.float64)


def main(path):
    L = ["/* generated by tools/zen_samples.py: ZEN-1's recorded sounds, looped, IMA ADPCM in blocks (in flash) */",
         "#pragma once", "#include <stdint.h>", f"#define ZN_SAMPLE_RATE {RATE}", f"#define ZN_BLOCK {BLOCK}",
         f"#define ZN_BLOCK_BYTES {4 + BLOCK // 2}"]
    loops = [(key, make_loop(decode_file(ROOT / file), start, length, hp), source, start)
             for key, file, start, length, hp, source in SOUNDS]
    ref = loops[0][1]
    ref_rms = np.sqrt((ref ** 2).mean()) * 0.89 / np.abs(ref).max()   # the brook at -1 dBFS peak: everyone's loudness
    names = []
    for key, loop, source, start in loops:
        g = ref_rms / max(1e-9, np.sqrt((loop ** 2).mean()))
        if np.abs(loop).max() * g > 0.98:                                    # never clip: such a loop comes out quieter
            g = 0.98 / np.abs(loop).max()
        loop = loop * g
        n = len(loop) // BLOCK * BLOCK
        s = np.round(loop[:n] * 32767).astype(np.int64)
        data = bytearray()
        for b in range(0, n, BLOCK):
            data += adpcm_block(s[b:b + BLOCK])
        err = adpcm_decode(data, n) - s
        snr = 10 * np.log10((s.astype(float) ** 2).mean() / max(1e-9, (err ** 2).mean()))
        if snr < 15:                                                     # (noise-like beds: the coding noise is like the sound)
            raise SystemExit(f"{key}: ADPCM SNR {snr:.1f} dB, too low")
        rel = 20 * np.log10(np.sqrt((loop ** 2).mean()) / ref_rms)
        L.append(f"/* {source}: {start:.1f} s + {n / RATE:.2f} s, ADPCM SNR {snr:.1f} dB, loudness {rel:+.1f} dB */")
        L.append(f"#define ZN_{key}_LEN {n}")
        L.append(f"static const uint8_t ZN_{key}[{len(data)}] = {{")
        for i in range(0, len(data), 24):
            L.append("    " + ", ".join(str(v) for v in data[i:i + 24]) + ",")
        L.append("};")
        names.append(key)
        print(f"sample {key}: {n / RATE:.2f} s, {len(data)} B, SNR {snr:.1f} dB, loudness {rel:+.1f} dB")
    L.append(f"#define ZN_SHOT_RATE {SHOT_RATE}")
    for key, file, rate, hp, segs, source in ONESHOTS:
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(ROOT / file), "-ac", "1", "-ar", str(rate), "-f", "s16le",
                              "-"], capture_output=True, check=True).stdout
        x = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
        if hp:
            from scipy.signal import butter, sosfiltfilt
            x = sosfiltfilt(butter(4, hp, "highpass", fs=rate, output="sos"), x)
        cuts = []
        for a0, a1 in segs:
            c = x[int(a0 * rate):int(a1 * rate)].copy()
            f = int(0.01 * rate)
            c[:f] *= np.linspace(0.0, 1.0, f)                            # 10 ms in and out: no click
            c[-f:] *= np.linspace(1.0, 0.0, f)
            c -= c.mean()
            cuts.append(c)
        g = 0.89 / max(np.abs(c).max() for c in cuts)                    # the loudest call at -1 dBFS
        if key in ("BIRD", "OWL", "OWL2"):                                                # (the songs each their own: one gain left
            cuts = [c * (0.89 / np.abs(c).max()) / g for c in cuts]      # the quietest 8 dB under the loudest)
        L.append(f"/* {source}: {len(cuts)} calls, {rate} Hz */")
        tab = []
        for i, c in enumerate(cuts):
            n = (len(c) + BLOCK - 1) // BLOCK * BLOCK
            sv = np.zeros(n)
            sv[:len(c)] = c * g
            si = np.round(sv * 32767).astype(np.int64)
            data = bytearray()
            for b0 in range(0, n, BLOCK):
                data += adpcm_block(si[b0:b0 + BLOCK])
            L.append(f"static const uint8_t ZN_{key}_{i}[{len(data)}] = {{")
            for j in range(0, len(data), 24):
                L.append("    " + ", ".join(str(v) for v in data[j:j + 24]) + ",")
            L.append("};")
            tab.append(f"{{ZN_{key}_{i}, {len(c)}}}")
            print(f"shot {key}_{i}: {len(c) / rate:.2f} s, {len(data)} B")
        L.append(f"#define ZN_{key}_N {len(cuts)}")
        L.append(f"static const struct {{ const uint8_t *data; int32_t len; }} ZN_{key}_TAB[ZN_{key}_N] = {{")
        L.append("    " + ", ".join(tab) + ",")
        L.append("};")
    L.append("enum { " + ", ".join(f"ZN_S_{k}" for k in names) + ", ZN_NSAMPLE };")
    L.append("static const struct { const uint8_t *data; int32_t len; } ZN_SAMPLE_TAB[ZN_NSAMPLE] = {")
    L += [f"    {{ZN_{k}, ZN_{k}_LEN}}," for k in names]
    L.append("};")
    Path(path).write_text("\n".join(L) + "\n")


if __name__ == "__main__":
    if "--rate" in sys.argv:
        RATE = int(sys.argv[sys.argv.index("--rate") + 1])
    main(sys.argv[1])
