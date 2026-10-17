/* SPDX-License-Identifier: GPL-3.0-only */
/* ZEN-1's scene patches, measured against the Koto: each one played alone through the whole engine (no
 * reverb), at a scene's velocity and pitch, and its loudness (RMS over its first second), peak, how much of it
 * lies above 300 Hz (what the FM-1's speaker plays), above 5 kHz (the bright band), and how long it rings.
 *
 *   cc -O2 -std=c99 -DFM_HOST -Ifirmware/src/dsp -o build/zen_voicing tools/zen_voicing.c -lm && build/zen_voicing
 *
 * The bands come from one-pole filters: rough, but the same for every patch, so the comparisons hold. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/dsp/fumi.c"

void fm_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

#define SR 44100
#define NS (8 * SR)
static float x[NS];
static int32_t blk[2 * 256];

static void play(int pn, float cents, int vel, int dur_ms, int n)
{
    int i, k;
    fm_part_patch(1, pn);
    fm_part_release(1, 2600);
    fm_part_level(1, 100);
    for (i = 0; i < 40; i++)
        fm_render(blk, 256, 4096);                 /* settle */
    fm_voice(FM_VOICE_ID, 1, cents, vel, 64, dur_ms);
    for (i = 0; i < n; i += 256) {
        fm_render(blk, 256, 4096);
        for (k = 0; k < 256 && i + k < n; k++)
            x[i + k] = (float)blk[2 * k] / 8388608.0f;
    }
    fm_voice_all_off();
    for (i = 0; i < 2 * SR / 256; i++)
        fm_render(blk, 256, 4096);
}

static void measure(const char *name, int pn, float cents, int vel, int dur_ms)
{
    int i, n = NS, ring = 0;
    double e = 0, eh = 0, eb = 0, pk = 0, lp = 0, lp5 = 0, a = 1.0 - exp(-2.0 * 3.14159265358979 * 300.0 / SR),
           a5 = 1.0 - exp(-2.0 * 3.14159265358979 * 5000.0 / SR), top = 0;
    float env = 0;
    play(pn, cents, vel, dur_ms, n);
    if (getenv("ZN_DUMP")) {                        /* the samples, for a closer look (numpy) */
        char fn[96];
        FILE *f;
        snprintf(fn, sizeof fn, "%s/%s.f32", getenv("ZN_DUMP"), name);
        if ((f = fopen(fn, "wb")) != 0) {
            fwrite(x, sizeof x[0], (size_t)n, f);
            fclose(f);
        }
    }
    for (i = 0; i < SR; i++) {
        double v = x[i], h, b;
        lp += a * (v - lp);
        lp5 += a5 * (v - lp5);
        h = v - lp;
        b = v - lp5;
        e += v * v;
        eh += h * h;
        eb += b * b;
    }
    for (i = 0; i < n; i++) {
        double v = fabs(x[i]);
        if (v > pk)
            pk = v;
    }
    for (i = 0; i < n; i++) {                       /* the ring: last time the 10 ms envelope is within 40 dB of its top */
        env += 0.01f * (fabsf(x[i]) - env);
        if (env > top)
            top = env;
        if (env > top * 0.01)
            ring = i;
    }
    printf("%-11s %5.0f c v%3d  rms %6.1f dB  peak %6.1f dB  >300Hz %3.0f%%  >5kHz %4.1f%%  rings %4.2f s\n", name, cents,
           vel, 10 * log10(e / SR + 1e-20), 20 * log10(pk + 1e-12), 100 * eh / (e + 1e-20), 100 * eb / (e + 1e-20),
           (double)ring / SR);
}

int main(void)
{
    int koto = 0;
    fm_init();
    fm_set(P_REVERB, 0);
    fm_set(P_VOICE, 0);
    printf("-- the notes (a scene's middle: -300 cents, velocity 80)\n");
    measure("Koto", koto, -300, 80, 1300);
    measure("Koto Warm", 9, -300, 80, 1300);
    measure("Drop", ZN_DROP, -300, 80, 1300);
    measure("Mallet", ZN_MALLET, -300, 80, 1300);
    measure("Bell", ZN_BELL, -300, 80, 1300);
    measure("Harp", 10, -300, 60, 700);
    measure("Furin", ZN_FURIN, -300, 60, 700);
    printf("-- the soft events (part 4)\n");
    measure("Soft Swell", ZN_SOFT_SWELL, -300, 60, 2000);
    measure("Rin", ZN_RIN, -300, 60, 2000);
    measure("Rin soft", ZN_RIN, -300, 36, 2200);
    measure("Swell soft", ZN_SOFT_SWELL, -300, 36, 2200);
    printf("-- the knock (part 3, velocity 72, 160 ms)\n");
    measure("Taiko", 6, -300, 72, 160);
    measure("Bamboo", ZN_BAMBOO, -300, 72, 160);
    {   /* the top of what each plays (the default tuning, key and octave): Sakura's chime picks white keys 8..12,
         * the pond's bites reach the rim's key 15 */
        float c12 = fm_key_cents(12), c15 = fm_key_cents(15);
        printf("-- the top: white key 12 (%.0f c, Sakura's highest chime) and 15 (%.0f c, the pond's rim)\n", c12, c15);
        measure("Furin k12", ZN_FURIN, c12, 60, 900);
        measure("Harp k12", 10, c12, 60, 700);
        measure("Drop k15", ZN_DROP, c15, 80, 1300);
        measure("Bell k15", ZN_BELL, c15, 80, 1300);
        measure("Koto k15", koto, c15, 80, 1300);
    }
    printf("-- a frog's pulse (velocity 70, 35 ms) and the Shakuhachi (velocity 60, 1.4 s)\n");
    measure("Shakuhachi", 3, -900, 60, 1400);
    measure("Koto low", koto, -900, 60, 1400);
    printf("-- the drone (held 6 s, ~230 Hz)\n");
    measure("Soft Pad", ZN_SOFT_PAD, -1100, 100, 6000);
    measure("Deep Pad", ZN_DEEP_PAD, -1100, 100, 6000);
    return 0;
}
