/* SPDX-License-Identifier: GPL-3.0-only */
/* dsp/fumi.c: the instrument on the host. Pitch of the keys through the whole engine, the tunings, 余韻,
 * polyphony and stealing, 単音, トリラー, ビブラート, the bend buttons, patches, filters, reverb, levels. */
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tu.h"
#include "tu_yin.h"
#include "../../firmware/src/dsp/fumi.c"

void fm_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static float buf[3 * 44100];                      /* mono (left) */
static int32_t blk[2 * 256];

/* render n samples (a multiple of 256) into buf from pos; returns the peak */
static float run(int pos, int n)
{
    int i, k;
    float pk = 0;
    for (i = 0; i < n; i += 256) {
        fm_render(blk, 256, 4096);
        for (k = 0; k < 256; k++) {
            float l = (float)blk[2 * k] / 8388608.0f;
            buf[pos + i + k] = l;
            if (fabsf(l) > pk)
                pk = fabsf(l);
        }
    }
    return pk;
}
static float peak_of(int from, int to)
{
    float pk = 0;
    int i;
    for (i = from; i < to; i++)
        if (fabsf(buf[i]) > pk)
            pk = fabsf(buf[i]);
    return pk;
}
static double db(float x) { return 20.0 * log10(x > 1e-9f ? (double)x : 1e-9); }
static int koto_a(void)                           /* the slot of FuMi's own KOTO A, "Koto Pluck" (the picked patches come first) */
{
    int i;
    for (i = 0; i < fm_patch_count(); i++)
        if (!strcmp(fm_patch_name(i), "Koto Pluck"))
            return i;
    return 0;
}
static int onsets(int from, int to)               /* rises of 4 dB within 10 ms of the 5 ms RMS envelope: a koto
                                                   * decaying at the ST-50's -28 dB/s drops only 5-6 dB between
                                                   * re-plucks at the middle trill rate */
{
    int i, n = 0, w = 220, last = -100000;
    for (i = from + 2 * w; i < to; i += 44) {
        double a = 0, b = 0;
        int k;
        for (k = 0; k < w; k++) {
            a += (double)buf[i - 2 * w + k] * buf[i - 2 * w + k];
            b += (double)buf[i - w + k] * buf[i - w + k];
        }
        if (b > 2.5 * a && b > 1e-6 && i - last > 2000) {
            n++;
            last = i;
        }
    }
    return n;
}

/* a sustaining test voice: OP1 carrier, level 99, held at L3 = 99, release R4 = 99 (fast) */
static void sustain_patch(uint8_t *pk, int r4)
{
    uint8_t v[FP_SIZE + 1];
    int k, i;
    memset(v, 0, sizeof v);
    for (k = 0; k < 6; k++) {
        uint8_t *op = v + k * FP_OP;
        for (i = 0; i < 4; i++) {
            op[FP_R1 + i] = 99;
            op[FP_L1 + i] = (uint8_t)(i < 3 ? 99 : 0);
        }
        op[FP_R1 + 3] = (uint8_t)r4;
        op[FP_BP] = 39;
        op[FP_FC] = 1;
        op[FP_DET] = 7;
        op[FP_OL] = (uint8_t)(k == 5 ? 99 : 0);
    }
    for (i = 0; i < 4; i++) {
        v[FP_PR1 + i] = 99;
        v[FP_PL1 + i] = 50;
    }
    v[FP_OKS] = 1;
    v[FP_LFS] = 35;
    v[FP_LPMS] = 3;
    v[FP_TRNSP] = 24;
    memcpy(v + FP_NAME, "SUSTAIN   ", 10);
    fm6_pack(v, pk);
}

static void reset(void)
{
    uint8_t pk[128];
    fm_init();
    sustain_patch(pk, 99);
    fm_patch_set(pk);
    fm_set(P_REVERB, 0);
    fm_set(P_HIGHCUT, 100);
    fm_set(P_LOWCUT, 0);
    fm_set(P_YOIN, 0);
    fm_set(P_LEVEL, 60);
    run(0, 256);
}

/* ---- ZEN-1's parts: scene notes on their own patches, pan, length, release, level */
static float rms_lr[2];
static void run_lr(int n)                         /* n samples (a multiple of 256): the RMS of each side */
{
    double a[2] = {0, 0};
    int i, k;
    for (i = 0; i < n; i += 256) {
        fm_render(blk, 256, 4096);
        for (k = 0; k < 512; k++) {
            double x = (double)blk[k] / 8388608.0;
            a[k & 1] += x * x;
        }
    }
    rms_lr[0] = (float)sqrt(a[0] / n);
    rms_lr[1] = (float)sqrt(a[1] / n);
}
static int slot_of(const char *name)
{
    int i;
    for (i = 0; i < fm_patch_count(); i++)
        if (!strcmp(fm_patch_name(i), name))
            return i;
    return -1;
}

/* ZEN-1's scene patches (zen_patches.h): each one sounds at a scene's pitch, about as loud as the Koto (its
 * level matched within a few dB, tools/zen_voicing.c), well inside full scale, and mostly where the FM-1's speaker
 * plays (above ~300 Hz): the knock and the drone too */
static void zen_patches_test(void)
{
    int i;
    float ref = 0;
    fm_set(P_REVERB, 0);
    fm_part_level(1, 100);
    fm_part_release(1, 300);
    for (i = -1; i < ZN_NPATCH; i++) {
        int p = i < 0 ? 0 : ZN_PATCH_BASE + i, k, n = 0;
        float cents = (p == ZN_SOFT_PAD || p == ZN_DEEP_PAD) ? -1100.0f : -300.0f, pk, r;
        double e = 0, eh = 0, lp = 0;
        fm_part_patch(1, p);
        run(0, 256);
        fm_voice(FM_VOICE_ID + 20, 1, cents, 80, 64, (p == ZN_SOFT_PAD || p == ZN_DEEP_PAD) ? 3000 : 600);
        pk = run(0, 3 * 44100 - 256);
        for (k = 0; k < 3 * 44100 - 256; k++) {   /* the 300 Hz one-pole: the share above it */
            double v = buf[k];
            lp += 0.0418 * (v - lp);
            e += v * v;
            eh += (v - lp) * (v - lp);
            n++;
        }
        r = (float)sqrt(e / n);
        if (i < 0) {
            ref = r;
            continue;
        }
        printf("  %-10.10s rms %+5.1f dB to the Koto, peak %.1f dBFS, %2.0f%% above ~300 Hz\n", (const char *)ZN_PATCH[i] + 118,
               db(r) - db(ref), db(pk), 100.0 * eh / (e + 1e-20));
        CHECK("a scene patch sounds, and within full scale", r > 1e-5f && pk < 0.9f);
        CHECK("it sits within 14 dB of the Koto (the pads rise slowly: their level over 3 s)", fabs(db(r) - db(ref)) < 14.0);
        CHECK("and is heard on the FM-1's speaker (a third or more above ~300 Hz)", eh > 0.33 * e);
        fm_voice_all_off();
        run(0, 44032);
    }
}

static void parts_test(void)
{
    int sho = slot_of("Sho"), taiko = slot_of("Taiko"), i;
    CHECK("the built-in Sho and Taiko exist", sho >= 0 && taiko >= 0);
    fm_set(P_VOICE, 0);                            /* the instrument: Koto, on part 0 */
    fm_set(P_REVERB, 0);
    fm_part_patch(1, sho);
    fm_part_patch(2, taiko);
    fm_part_release(1, 100);
    run(0, 256);

    /* a held scene note on part 1 plays its own patch at its own pitch, beside the instrument */
    fm_voice(FM_VOICE_ID, 1, 0.0f, 100, 64, 0);
    fm_key(5, 1);
    run(0, 22016);
    CHECK("a scene note and a key sound together", fm_nvoices == 2);
    fm_key(5, 0);
    run(0, 44032);
    CHECK("the key's 余韻 ends; the held scene note does not", fm_nvoices == 1);
    CHECK_NEAR("the scene note's pitch (A4 by cents)", tu_yin(buf + 11000, 8000, 44100.0), 440.0, 1.0);
    CHECK("the instrument still shows its own patch", !strncmp((const char *)fm_patch_label, fm_patch_name(0), 4));
    fm_voice_off(FM_VOICE_ID);
    run(0, 22016);
    CHECK("fm_voice_off: released within the part's 100 ms", fm_nvoices == 0);

    /* pan: hard left is silent on the right; the center is equal */
    fm_voice(FM_VOICE_ID + 1, 1, 0.0f, 100, 0, 0);
    run_lr(8192);
    CHECK("pan 0: left only", rms_lr[0] > 0.001f && rms_lr[1] < rms_lr[0] * 0.01f);
    fm_voice_off(FM_VOICE_ID + 1);
    run(0, 22016);
    fm_voice(FM_VOICE_ID + 2, 1, 0.0f, 100, 64, 0);
    run_lr(8192);
    CHECK_NEAR("pan 64: the sides within 0.1 dB", db(rms_lr[0]) - db(rms_lr[1]), 0.0, 0.1);
    fm_voice_off(FM_VOICE_ID + 2);
    run(0, 22016);

    /* length: a 200 ms note releases by itself */
    fm_voice(FM_VOICE_ID + 3, 1, 0.0f, 100, 64, 200);
    run(0, 4096);
    CHECK("a timed note sounds", fm_nvoices == 1);
    run(0, 22016);
    CHECK("and is gone after its 200 ms and the 100 ms release", fm_nvoices == 0);

    /* level: part 1 at 0 is silent; velocity: a soft note is quieter than a hard one */
    fm_part_level(1, 0);
    fm_voice(FM_VOICE_ID + 4, 1, 0.0f, 100, 64, 0);
    run_lr(8192);
    CHECK("part level 0: silent", rms_lr[0] < 1e-6f);
    fm_voice_off(FM_VOICE_ID + 4);
    fm_part_level(1, 100);
    run(0, 22016);
    {
        float soft, hard;
        fm_voice(FM_VOICE_ID + 5, 2, -1200.0f, 30, 64, 0);
        run_lr(8192);
        soft = rms_lr[0];
        fm_panic();
        run(0, 256);
        fm_voice(FM_VOICE_ID + 5, 2, -1200.0f, 127, 64, 0);
        run_lr(8192);
        hard = rms_lr[0];
        printf("  taiko: velocity 30 %.1f dB, 127 %.1f dB\n", db(soft), db(hard));
        CHECK("velocity: a soft taiko is quieter than a hard one", hard > soft * 1.2f);
        fm_panic();
        run(0, 256);
    }

    /* glide: a held scene note moves to a fifth up without a new attack (a koi turning) */
    fm_voice(FM_VOICE_ID + 6, 1, 0.0f, 100, 64, 0);
    run(0, 11008);
    fm_voice_glide(FM_VOICE_ID + 6, 700.0f, 80);
    run(0, 1024);
    {
        int v = find_voice(FM_VOICE_ID + 6);
        float mid = v >= 0 ? vc[v].pitch.y : -1.0f;
        CHECK("glide: on its way after 23 ms", mid > 50.0f && mid < 650.0f);
        run(0, 22016);
        CHECK_NEAR("glide: arrives a fifth up (cents)", v >= 0 ? vc[v].pitch.y : -1.0f, 700.0, 0.5);
        CHECK("glide: the same voice, never re-plucked", find_voice(FM_VOICE_ID + 6) == v && fm_nvoices == 1);
    }
    fm_voice_glide(FM_VOICE_ID + 6, 0.0f, 80);
    fm_voice_off(FM_VOICE_ID + 6);
    run(0, 22016);

    /* the nature beds: each sounds, sits well below a koto note, stays inside full scale, and goes away */
    {
        static const char *const NAME[FM_NBED] = {"wind", "stream", "rain"};
        float ref;
        int b;
        fm_voice(FM_VOICE_ID + 7, 3, 0.0f, 100, 64, 0);
        fm_part_patch(3, koto_a());
        run(0, 256);
        fm_voice(FM_VOICE_ID + 7, 3, 0.0f, 100, 64, 0);
        run_lr(8192);
        ref = rms_lr[0];
        fm_panic();
        run(0, 22016);
        for (b = 0; b < FM_NBED; b++) {
            float lo, hi;
            fm_bed(b, 100);
            fm_bed_mod(b, 0);
            run(0, 88064);                         /* the level glides in */
            run_lr(44032);
            lo = rms_lr[0];
            fm_bed_mod(b, 100);
            run(0, 22016);
            run_lr(44032);
            hi = rms_lr[0];
            printf("  bed %-6s: %.1f dB (amount 0), %.1f dB (amount 100); a koto note %.1f dB\n", NAME[b], db(lo), db(hi),
                   db(ref));
            CHECK("bed: sounds", lo > ref * (b == FM_BED_RAIN ? 0.003f : 0.01f));   /* (the rain: drops alone, no hiss,
                                                                                     * sparse when light) */
            CHECK("bed: below a koto note", hi < ref);
            CHECK("bed: the amount raises it", hi > lo);
            CHECK("bed: both sides", rms_lr[1] > rms_lr[0] * 0.5f);
            fm_bed(b, 0);
            fm_bed_mod(b, 0);
            run(0, 132096);
            run_lr(8192);
            CHECK("bed: at 0 it is gone", rms_lr[0] < ref * 0.001f);
        }
        /* the recorded brook: it plays on both sides, different there (half a loop apart), and its loop has no
         * seam (no sample-to-sample jump far above the recording's own; the whole 10 s loop is crossed) */
        fm_bed_style(FM_BED_STREAM, 1);
        fm_bed(FM_BED_STREAM, 100);
        run(0, 88064);
        {
            double l = 0, r = 0, lr = 0, jump = 0, typ = 0;
            int i2, k2, nb = (int)(11.0 * 44100 / 256), cnt = 0;
            float prev = 0;
            for (i2 = 0; i2 < nb; i2++) {
                fm_render(blk, 256, 4096);
                for (k2 = 0; k2 < 256; k2++) {
                    float a = (float)blk[2 * k2] / 8388608.0f, c = (float)blk[2 * k2 + 1] / 8388608.0f, d = fabsf(a - prev);
                    l += (double)a * a;
                    r += (double)c * c;
                    lr += (double)a * c;
                    typ += d;
                    cnt++;
                    if (d > jump)
                        jump = d;
                    prev = a;
                }
            }
            typ /= cnt;
            printf("  brook: %.1f dB, L/R correlation %.2f, largest step %.1fx the mean\n", 10 * log10(l / cnt), lr / sqrt(l * r), jump / typ);
            CHECK("brook: sounds", l / cnt > 1e-9);
            CHECK("brook: the sides differ (wide)", lr / sqrt(l * r) < 0.5);
            CHECK("brook: no seam in the loop", jump < 25.0 * typ);
        }
        fm_bed(FM_BED_STREAM, 0);
        fm_bed_style(FM_BED_STREAM, 0);
        run(0, 132096);
    }

    /* tape: off, silence is silence; on, a faint hiss in it (far under a note), and gone again at 0 */
    {
        float quiet, hiss;
        fm_panic();
        fm_set(P_REVERB, 0);
        run(0, 44032);
        run_lr(22016);
        quiet = rms_lr[0];
        fm_tape(100);
        run(0, 22016);
        run_lr(22016);
        hiss = rms_lr[0];
        printf("  tape: silence %.1f dB, with tape 100 %.1f dB\n", db(quiet), db(hiss));
        CHECK("tape off: silence", quiet < 1e-6f);
        CHECK("tape on: a faint hiss", hiss > 1e-6f && hiss < 0.001f);
        fm_tape(0);
        run(0, 22016);
        run_lr(22016);
        CHECK("tape back to 0: silence again", rms_lr[0] < 1e-6f);
    }

    /* cost: a layered scene, eight voices across three parts, with the reverb */
    fm_set(P_REVERB, 60);
    fm_part_patch(3, slot_of("Koto Ring"));
    for (i = 0; i < FM_NBED; i++) {
        fm_bed(i, 100);
        fm_bed_mod(i, 100);
    }
    for (i = 0; i < FM_NVOICE; i++)
        fm_voice(FM_VOICE_ID + 10 + i, 1 + i % 3, (float)(i * 150 - 900), 90, i * 11, 0);
    {
        clock_t t0 = clock();
        double s;
        run_lr(44032);
        s = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("  cost: %d layered voices (3 parts, panned) + reverb + all beds: 1 s in %.4f s of host time\n", FM_NVOICE, s);
        CHECK("layered: faster than real time on the host", s < 1.0);
        CHECK("layered: every voice sounds", fm_nvoices == FM_NVOICE);
    }
    for (i = 0; i < FM_NBED; i++)
        fm_bed(i, 0);
    fm_panic();
    run(0, 256);
}

int main(void)
{
    int i;
    double hz;
    reset();
    CHECK("silence at rest", run(0, 2048) == 0.0f);

    /* pitch through the whole engine: 1本, 三 = A3 = 220 Hz; SUIKO moves fa by -20.8 and leaves la alone */
    fm_key(5, 1);
    run(0, 22016);
    hz = tu_yin(buf + 11000, 8000, 44100.0);
    CHECK_NEAR("white key 6 (三) at 1本 = 220 Hz", hz, 220.0, 0.3);
    fm_key(5, 0);
    run(0, 4096);
    fm_key(1, 1);
    run(0, 22016);
    CHECK_NEAR("white key 2 (fa) in SUIKO: -2300 - 20.8 cents from A4", tu_cents(tu_yin(buf + 11000, 12000, 44100.0), 440.0), -2320.8, 1.0);
    fm_set(P_TUNING, TN_EQUAL);
    run(0, 22016);
    CHECK_NEAR("switching to 平均律 while held: the note arrives at -2300", tu_cents(tu_yin(buf + 11000, 12000, 44100.0), 440.0), -2300.0, 1.0);
    fm_set(P_TUNING, TN_SUIKO);
    fm_key(1, 0);
    run(0, 4096);
    fm_key(2, 1);                                  /* la: no offset in SUIKO */
    run(0, 22016);
    CHECK_NEAR("white key 3 (la) in SUIKO: -1900 cents", tu_cents(tu_yin(buf + 11000, 12000, 44100.0), 440.0), -1900.0, 1.0);
    fm_key(2, 0);
    fm_set(P_HON, 0);                              /* 水4 */
    run(0, 4096);
    fm_key(0, 1);
    run(0, 44032);
    CHECK_NEAR("水4 white key 1 = F2 = 87.31 Hz", tu_yin(buf + 22000, 20000, 44100.0), 87.307, 0.3);
    fm_key(0, 0);
    fm_set(P_HON, 4);
    run(0, 4096);
    fm_key(16, 1);                                 /* black key 1 = fa# over the low octave */
    run(0, 22016);
    CHECK_NEAR("black key 1 (upper row) = fa# = -2200 cents", tu_cents(tu_yin(buf + 11000, 12000, 44100.0), 440.0), -2200.0, 1.0);
    fm_key(16, 0);
    fm_note(FM_NOTE_ID, 1, 702.0f);
    run(0, 22016);
    CHECK_NEAR("fm_note at +702 cents from A4", tu_cents(tu_yin(buf + 11000, 6000, 44100.0), 440.0), 702.0, 1.0);
    fm_note(FM_NOTE_ID, 0, 0);
    run(0, 4096);

    /* 余韻: short = damped within 100 ms of key-up; long = still ringing a second later */
    fm_set(P_YOIN, 0);
    fm_key(5, 1);
    run(0, 11008);
    fm_key(5, 0);
    run(11008, 44032);
    CHECK("余韻 short: -60 dB within 100 ms of key-up", db(peak_of(11008 + 4410, 11008 + 8820)) < -60.0);
    fm_set(P_YOIN, 100);
    fm_key(5, 1);
    run(0, 11008);
    fm_key(5, 0);
    run(11008, 44032);
    CHECK("余韻 long: one second after key-up still within 24 dB of the level at key-up",
          db(peak_of(11008 + 44032 - 4410, 11008 + 44032)) > db(peak_of(11008 - 4410, 11008)) - 24.0);
    CHECK("and it decays", peak_of(11008 + 44032 - 4410, 11008 + 44032) < peak_of(11008, 11008 + 4410));
    fm_set(P_YOIN, 0);

    /* polyphony: every voice at once stays inside full scale; one more steals without a crash */
    for (i = 0; i <= FM_NVOICE; i++)
        fm_key(i, 1);
    {
        float pk = run(0, 22016);
        CHECK("FM_NVOICE + 1 keys: output inside full scale, and there", pk < 1.0f && pk > 0.003f);
        CHECK("every voice sounds", fm_nvoices == FM_NVOICE);
    }
    for (i = 0; i <= FM_NVOICE; i++)
        fm_key(i, 0);
    run(0, 8192);
    CHECK("all released: no voices", fm_nvoices == 0);

    /* 単音: last-note priority, and back to the held note on release */
    fm_set(P_MONO, 1);
    fm_key(5, 1);
    run(0, 8192);
    fm_key(10, 1);                                 /* 八: an octave up */
    run(0, 22016);
    CHECK("mono: one voice", fm_nvoices == 1);
    CHECK_NEAR("mono: the last key sounds (A4)", tu_yin(buf + 11000, 8000, 44100.0), 440.0, 0.5);
    fm_key(10, 0);
    run(0, 22016);
    CHECK_NEAR("mono: back to the held key (A3)", tu_yin(buf + 11000, 8000, 44100.0), 220.0, 0.5);
    fm_key(5, 0);
    fm_set(P_MONO, 0);
    run(0, 8192);

    /* トリラー: the held note re-plucked at the rate (on a decaying voice: the built-in koto) */
    fm_set(P_VOICE, koto_a());
    fm_set(P_TRILL_ON, 1);
    fm_set(P_TRILL_RATE, 50);
    fm_set(P_TRILL_VAR, 0);
    fm_key(5, 1);
    run(0, 44032);
    CHECK("trill: at least four re-plucks a second at the middle rate", onsets(2000, 44032) >= 4);
    fm_key(5, 0);
    fm_set(P_TRILL_ON, 0);
    run(0, 8192);
    {
        uint8_t pk[128];
        sustain_patch(pk, 99);
        fm_patch_set(pk);
        run(0, 256);
    }

    /* ビブラート: the pitch moves */
    fm_set(P_VIB_ON, 1);
    fm_set(P_VIB_RATE, 100);
    fm_set(P_VIB_DEPTH, 100);
    fm_key(5, 1);
    run(0, 44032);
    {
        double lo = 1e9, hi = 0;
        for (i = 20000; i < 40000; i += 1000) {
            double f = tu_yin(buf + i, 2000, 44100.0);
            if (f > 0) {
                lo = f < lo ? f : lo;
                hi = f > hi ? f : hi;
            }
        }
        CHECK("vibrato at full depth: at least 40 cents of movement", tu_cents(hi, lo) > 40.0);
    }
    fm_key(5, 0);
    fm_set(P_VIB_ON, 0);
    run(0, 8192);

    /* the bend buttons: OCT+ with the note held bends it up a semitone and back */
    fm_key(5, 1);
    run(0, 8192);
    fm_bend(BEND_UP, 1, 1);
    run(0, 22016);
    CHECK_NEAR("OCT+ held: +100 cents", tu_cents(tu_yin(buf + 11000, 8000, 44100.0), 220.0), 100.0, 2.0);
    fm_bend(BEND_UP, 0, 1);
    run(0, 22016);
    CHECK_NEAR("released: back to the note", tu_cents(tu_yin(buf + 11000, 8000, 44100.0), 220.0), 0.0, 2.0);
    fm_set(P_BEND_UP, BEND_T_WHOLE);
    fm_bend(BEND_UP, 1, 1);
    run(0, 22016);
    CHECK_NEAR("whole-tone target: +200", tu_cents(tu_yin(buf + 11000, 8000, 44100.0), 220.0), 200.0, 2.0);
    fm_bend(BEND_UP, 0, 1);
    fm_set(P_BEND_UP, BEND_T_SCALE);
    run(0, 22016);
    fm_bend(BEND_UP, 1, 1);
    run(0, 22016);
    CHECK_NEAR("next-scale-note target from mi: fa, +100 - 20.8", tu_cents(tu_yin(buf + 11000, 8000, 44100.0), 220.0), 79.2, 2.0);
    fm_bend(BEND_UP, 0, 1);
    fm_set(P_BEND_UP, BEND_T_SEMI);
    fm_key(5, 0);
    run(0, 22016);

    /* patches: set / get round trip, the built-in slots, and the label */
    {
        uint8_t a[128], b[128];
        sustain_patch(a, 50);
        fm_patch_set(a);
        run(0, 256);
        fm_patch_get(b);
        CHECK("patch set / get round trip", !memcmp(a, b, 128));
        CHECK("a sent patch is custom", fm_patch_custom == 1);
        CHECK("built-in patches exist", fm_patch_count() >= 2 && fm_patch_name(0)[0] != 0);
        fm_set(P_VOICE, koto_a());
        run(0, 256);
        CHECK("P_VOICE loads a built-in", fm_patch_custom == 0 && !strncmp((const char *)fm_patch_label, "Koto Pluck", 10));
        fm_key(5, 1);
        {
            float pk = run(0, 22016);
            CHECK("the built-in koto sounds", pk > 0.001f);
        }
        fm_key(5, 0);
        run(0, 22016);
        fm_patch_set(a);
        sustain_patch(a, 99);
        fm_patch_set(a);
        run(0, 256);
    }

    /* the high cut darkens: fewer zero crossings of a bright (modulated) patch */
    {
        uint8_t v[FP_SIZE + 1], pk[128];
        int zc0 = 0, zc1 = 0;
        fm_patch_get(pk);
        fm6_unpack(pk, v);
        v[4 * FP_OP + FP_OL] = 90;                 /* OP2 modulates OP1 hard, ratio 7 */
        v[4 * FP_OP + FP_FC] = 7;
        fm6_pack(v, pk);
        fm_patch_set(pk);
        fm_set(P_HIGHCUT, 100);
        fm_key(5, 1);
        run(0, 22016);
        for (i = 11001; i < 22016; i++)
            zc0 += (buf[i] >= 0) != (buf[i - 1] >= 0);
        fm_set(P_HIGHCUT, 0);
        run(0, 22016);
        for (i = 11001; i < 22016; i++)
            zc1 += (buf[i] >= 0) != (buf[i - 1] >= 0);
        CHECK("high cut at minimum: far fewer zero crossings", zc1 < zc0 / 2);
        fm_set(P_HIGHCUT, 100);
        fm_key(5, 0);
        run(0, 8192);
        sustain_patch(pk, 99);
        fm_patch_set(pk);
    }

    /* reverb: a tail after the dry note has gone */
    fm_set(P_REVERB, 100);
    fm_set(P_REV_SIZE, 100);
    run(0, 256);
    fm_key(5, 1);
    run(0, 11008);
    fm_key(5, 0);
    run(11008, 44032);
    CHECK("reverb: still above -50 dB 300 ms after the dry note stopped", db(peak_of(11008 + 13230, 11008 + 17640)) > -50.0);
    fm_set(P_REVERB, 0);
    fm_set(P_REV_SIZE, 0);                         /* let the plate's tail die before the next checks */
    run(0, 44032);

    /* the level: 0 is silent; the ornaments: DAMP silences held notes */
    fm_set(P_LEVEL, 0);
    fm_key(5, 1);
    run(0, 8192);
    CHECK("level 0: silent", peak_of(6144, 8192) == 0.0f);
    fm_set(P_LEVEL, 60);
    fm_set(P_YOIN, 100);
    run(0, 8192);
    fm_ornament(ORN_DAMP, 1);
    run(0, 8192);
    CHECK("DAMP held: the note is gone within 100 ms", db(peak_of(4410, 8192)) < -50.0);
    fm_ornament(ORN_DAMP, 0);
    fm_key(5, 0);
    fm_set(P_YOIN, 0);
    run(0, 8192);

    /* keyboard scaling: the built-in koto (rate and level scaling on its carriers) decays faster at the top
     * of the range than at the bottom; a constant note number into the FM6 core would make them identical */
    fm_set(P_VOICE, koto_a());
    fm_set(P_YOIN, 100);
    run(0, 256);
    {
        double lo, hi;
        fm_key(0, 1);
        run(0, 44032 + 11008);
        lo = db(peak_of(44032, 44032 + 11008));
        fm_key(0, 0);
        fm_panic();
        run(0, 2048);
        fm_key(15, 1);
        run(0, 44032 + 11008);
        hi = db(peak_of(44032, 44032 + 11008));
        fm_key(15, 0);
        fm_panic();
        run(0, 2048);
        printf("  koto a second in: key 1 %.1f dB, key 16 %.1f dB\n", lo, hi);
        CHECK("the top key has decayed at least 6 dB more than the bottom key a second in", lo - hi > 6.0);
    }
    fm_set(P_YOIN, 0);
    {
        uint8_t pk[128];
        sustain_patch(pk, 99);
        fm_patch_set(pk);
        run(0, 256);
    }

    /* the A reference: 430 Hz puts 三 at 215 Hz */
    fm_set(P_A4, 430);
    fm_key(5, 1);
    run(0, 22016);
    CHECK_NEAR("A = 430: 三 at 1本 = 215 Hz", tu_yin(buf + 11000, 8000, 44100.0), 215.0, 0.3);
    fm_key(5, 0);
    fm_set(P_A4, 440);
    run(0, 8192);
    fm_note(FM_NOTE_ID, 1, 0.0f);
    fm_set(P_A4, 430);
    run(0, 22016);
    CHECK_NEAR("a note by absolute pitch ignores the A reference", tu_yin(buf + 11000, 8000, 44100.0), 440.0, 0.5);
    fm_note(FM_NOTE_ID, 0, 0.0f);
    fm_set(P_A4, 440);
    run(0, 8192);

    /* cost and headroom: the six-operator koto, eight voices held, reverb; the pre-clip peak (fm_peak) stays
     * inside what the soft clip handles gracefully. The device figure comes from plat_cpu_pct() at M4 */
    fm_set(P_VOICE, koto_a());
    fm_set(P_REVERB, 50);
    fm_set(P_LEVEL, 80);
    run(0, 256);
    for (i = 0; i < 8; i++)
        fm_key(i, 1);
    {
        clock_t t0 = clock();
        float pre = 0;
        int k;
        for (k = 0; k < 44032; k += 256) {
            run(k, 256);
            if (fm_peak > pre)
                pre = fm_peak;
        }
        {
            double s = (double)(clock() - t0) / CLOCKS_PER_SEC;
            printf("  cost: KOTO A x 8 voices + reverb: 1 s in %.4f s of host time (%.0f ns a frame); pre-clip peak %.2f\n", s, s * 1e9 / 44032.0, (double)pre);
            CHECK("renders faster than real time on the host", s < 1.0);
            CHECK("pre-clip peak of eight koto attacks at level 80 stays under 2 (the soft clip rounds the rest)", pre < 2.0f);
            CHECK("and they are loud enough to matter", pre > 0.02f);
        }
    }
    for (i = 0; i < 8; i++)
        fm_key(i, 0);
    fm_panic();
    run(0, 256);
    CHECK("panic: quiet", fm_nvoices == 0);
    parts_test();
    zen_patches_test();
    return tu_done("zen");
}
