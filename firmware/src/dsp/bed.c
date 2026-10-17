/* SPDX-License-Identifier: GPL-3.0-only */
/* ZEN-1's nature beds: wind, a stream and rain (FM cannot make noise). Included by fumi.c; the beds go into
 * the mix ahead of the voicing stages, so the high cut tames them and the reverb takes them in.
 *
 *   wind     the air (FM_AIR_*): style 0, a synthesized breeze: a quiet bed of soft noise through a gentle
 *            low-pass, and now and then (every 8..25 s) a puff that swells over 2..3 s, blows a while and dies away
 *            over 5..8 s, brightening a little as it blows and drifting from one side to the other. The amount
 *            rising past half is a gust: a strong puff at once. (the earlier breeze swelled all the time: on the
 *            FM-1 "a hurricane with wind whipping constantly".) The sides are separate noises. Styles 1..3, recordings (CC0, zen_samples.h): a breeze in trees, rustling leaves, night
 *            insects; the amount lifts the breeze and the leaves a little (a gust), and is the night for the
 *            insects (they are faint by day).
 *   stream   the water (FM_WATER_*): style 0, bubbles: water modeled as many small bubbles, each a short sine
 *            that rises a little as it dies away (Minnaert: the smaller the bubble, the higher; van den Doel's
 *            decay and rise), tens a second, each on its own side, over a low murmur; the amount is the rush.
 *            Style 1, a recorded brook. Style 2, white noise (both sides their own). style 3 pink noise (-3 dB an octave: Paul Kellet's economy filter), style 4
 *            deep (brown: a leaky integral of the noise, the lowest rumble taken out, with some pink kept in the
 *            speaker's range so the FM-1's own speaker still has something to play), style 5 rain (the rain bed's
 *            drops, steady and moderate), style 6 a trickle (a thin stream of water into a stone basin:
 *            small high bubbles, fast and fairly even, over a soft fluttering flow). style 7, slow bubbles
 *            (the trickle "too distracting and high pitched"; bubbles liked, but too many): a still pond,
 *            one bigger, lower bubble every second or so (~0.3..2.5 s apart) over the murmur.
 *            (The first stream, a band-pass jumping about, sounded like wind whipping on the FM-1's speaker.)
 *   A recording plays as two heads, one a side, half a loop apart. The loops are all at one loudness (RMS).
 *   rain rain on water, after how rain is heard: most drops only a soft tick (a splash, broadband, ~1 ms); about one
 *            in five a "plip", the bubble it traps ringing (~0.8-3 kHz), its pitch rising as it dies, ~20-45 ms;
 *            no hiss under them: the drops alone;
 *            the whole swelling and easing over 5-20 s. Drops come at a
 *            rate the amount sets (unseen, far) and each drop a scene shows landing is heard too, at its place
 *            (fm_drop; on a lily pad a tick). After the rain, a minute of drips, larger and lower, thinning out.
 *
 * Each bed's level glides (about 1 s), so a scene change or a knob never steps. A bed at 0 costs nothing. */
#include "zen_samples.h"
#define ZN_WATER_GAIN 0.2f                          /* the recordings (all at one loudness), level with the bubbles */
#define ZN_AIR_GAIN 0.3f                            /* the air's recordings (+8 dB; at 100 they were too low) */
#define BED_WIND FM_BED_WIND
#define BED_STREAM FM_BED_STREAM
#define BED_RAIN FM_BED_RAIN
#define BED_N FM_NBED
#define BED_DROPS 8
#define BED_BUBBLES 10

typedef struct {
    float lp, bp;                                   /* Chamberlin state-variable filter */
} svf_t;
static inline float svf_bp(svf_t *s, float x, float f, float q)   /* f = 2 sin(pi fc / fs), q = 1 / Q */
{
    float hp = x - s->lp - q * s->bp;
    s->bp += f * hp;
    s->lp += f * s->bp;
    return s->bp;
}
static inline float svf_f(float hz) { return 2.0f * fm_sin_pi(hz * (1.0f / FM_SR)); }

typedef struct {
    float g, dg, ph, f, df, tap;                    /* gain and its decay; phase, pitch (turns a sample) and its rise;
                                                     * the tap at its start */
    float tapd, tapf;                               /* the tap's decay a sample, its band */
    float pl, pr;
    int32_t left;                                   /* samples to go */
    svf_t s;
} drop_t;

typedef struct {
    float ph, f, df, g, dg, pl, pr;                /* phase (turns), freq (turns a sample) and its rise, gain, decay */
    int32_t left;
} bubble_t;

typedef struct {                                   /* a head of a looped ADPCM recording */
    const uint8_t *data;
    int32_t len, pos, pred, idx;
    float s0, s1, frac;
} head_t;

static struct {
    float level[BED_N], target[BED_N], mod[BED_N]; /* level 0..1 (glides to target); amount 0..1 */
    uint8_t style[BED_N];
    uint32_t n1, n2;                                /* two noises (xorshift) */
    /* wind */
    float wl1, wl2, wr1, wr2;                       /* two-pole low-passes, a side */
    float w_walk, w_walk_v, w_a, w_gb, w_gp, w_pl, w_pr;   /* the base and the puff's gains, the puff's sides */
    float p_env, p_peak, p_pan0, p_pan1, p_lastmod;  /* the puff now (0..1), its height, where it blows from / to */
    int32_t p_phase, p_t, p_rise, p_hold, p_fall, p_next;   /* blocks: 0 idle, 1 rising, 2 holding, 3 falling */
    /* stream */
    bubble_t b[BED_BUBBLES];
    float mur1, mur2, mur_walk;
    float pk[2][3], br[2], brh[2];                  /* pink (Kellet, three poles a side); brown and its low cut */
    float tr_f, tr_flut, tr_ph;                     /* the trickle: its flow's band, flutter */
    svf_t tr_s[2];
    struct {                                        /* the frogs' recorded calls (one-shots) */
        const uint8_t *data;
        int32_t len, pos, pred, idx;
        float frac, step, s0, s1, g, pl, pr;
    } shot[FM_NSHOT];
    head_t hl, hr;                                  /* the water's recording */
    head_t al, ar;                                  /* the air's recording */
    /* rain */
    drop_t d[BED_DROPS];
    float sw, sw_t;                                 /* the rain's swell (x the amount) and where it heads */
    int32_t sw_n, drip;                             /* blocks to the next swell; blocks of drips left after rain */
    float drip_g;                                   /* how hard it rained */
} bed;

static inline float noise1(void)                    /* -1 .. 1 */
{
    bed.n1 ^= bed.n1 << 13;
    bed.n1 ^= bed.n1 >> 17;
    bed.n1 ^= bed.n1 << 5;
    return (float)(int32_t)bed.n1 * (1.0f / 2147483648.0f);
}
static inline float noise2(void)
{
    bed.n2 ^= bed.n2 << 13;
    bed.n2 ^= bed.n2 >> 17;
    bed.n2 ^= bed.n2 << 5;
    return (float)(int32_t)bed.n2 * (1.0f / 2147483648.0f);
}
static inline float urand(void) { return 0.5f + 0.5f * noise1(); }   /* 0 .. 1 */

static void bed_init(void)
{
    uint32_t i;
    uint8_t *z = (uint8_t *)&bed;
    for (i = 0; i < sizeof bed; i++)
        z[i] = 0;
    bed.n1 = 0x9E3779B9u;
    bed.n2 = 0x7F4A7C15u;
}

/* the two heads of a bed onto recording s: the left at the start, the right half a loop on (a block start) */
static void heads_set(head_t *l, head_t *r, int s)
{
    l->data = r->data = ZN_SAMPLE_TAB[s].data;
    l->len = r->len = ZN_SAMPLE_TAB[s].len;
    l->pos = 0;
    r->pos = l->len / 2 / ZN_BLOCK * ZN_BLOCK;
    l->frac = r->frac = 0.0f;
    l->s0 = l->s1 = r->s0 = r->s1 = 0.0f;
}

static void bed_style(int which, int style)
{
    if (which == BED_STREAM) {
        bed.style[which] = (uint8_t)(style < 0 ? 0 : style >= FM_NWATER ? FM_WATER_NOISE : style);
        if (bed.style[which] == FM_WATER_BROOK)
            heads_set(&bed.hl, &bed.hr, ZN_S_BROOK);
    } else if (which == BED_WIND) {
        static const uint8_t AIR_SAMPLE[4] = {0, ZN_S_BREEZE, ZN_S_BREEZE, ZN_S_INSECTS};   /* (Leaves went:
                                                                                     * the room; it plays the breeze) */
        bed.style[which] = (uint8_t)(style < 0 ? 0 : style > FM_AIR_INSECTS ? FM_AIR_INSECTS : style);
        if (bed.style[which])
            heads_set(&bed.al, &bed.ar, AIR_SAMPLE[bed.style[which]]);
    }
}

/* the one-shot sets (fumi.h FM_SHOT_*): a recording's calls, by set and number: 0 = there is one */
static int shot_seg(int set, int seg, const uint8_t **data, int32_t *len)
{
    switch (set) {
    case FM_SHOT_FROG:
        if (seg < 0 || seg >= ZN_FROG_N)
            return -1;
        *data = ZN_FROG_TAB[seg].data;
        *len = ZN_FROG_TAB[seg].len;
        return 0;
    case FM_SHOT_BIRD:
        if (seg < 0 || seg >= ZN_BIRD_N)
            return -1;
        *data = ZN_BIRD_TAB[seg].data;
        *len = ZN_BIRD_TAB[seg].len;
        return 0;
    case FM_SHOT_KNOCK:
        if (seg < 0 || seg >= ZN_KNOCK_N)
            return -1;
        *data = ZN_KNOCK_TAB[seg].data;
        *len = ZN_KNOCK_TAB[seg].len;
        return 0;
    case FM_SHOT_OWL:                                 /* (two recordings: one and two calls) */
        if (seg == 0) {
            *data = ZN_OWL_TAB[0].data;
            *len = ZN_OWL_TAB[0].len;
            return 0;
        }
        if (seg < 1 || seg > ZN_OWL2_N)
            return -1;
        *data = ZN_OWL2_TAB[seg - 1].data;
        *len = ZN_OWL2_TAB[seg - 1].len;
        return 0;
    default:
        return -1;
    }
}
int fm_shot_count(int set)
{
    return set == FM_SHOT_FROG ? ZN_FROG_N : set == FM_SHOT_BIRD ? ZN_BIRD_N : set == FM_SHOT_KNOCK ? ZN_KNOCK_N
         : set == FM_SHOT_OWL ? 1 + ZN_OWL2_N : 0;
}
int fm_shot_ms(int set, int seg)
{
    const uint8_t *d;
    int32_t n;
    return shot_seg(set, seg, &d, &n) ? 0 : (int)(n * 1000 / ZN_SHOT_RATE);
}
int fm_frog_count(void) { return fm_shot_count(FM_SHOT_FROG); }
int fm_frog_ms(int seg) { return fm_shot_ms(FM_SHOT_FROG, seg); }
int fm_bird_count(void) { return fm_shot_count(FM_SHOT_BIRD); }
int fm_bird_ms(int seg) { return fm_shot_ms(FM_SHOT_BIRD, seg); }

/* a frog's recorded call on shot slot i: seg of ZN_FROG_TAB, at rate (1.0: as recorded) */
static void shot_start(int i, int seg, int vel, int pan, float rate)
{
    float p = (float)pan * (1.0f / 127.0f), v = (float)vel * (1.0f / 127.0f);
    int set = seg >> 8;                               /* (the set in the high byte: FM_SHOT_*) */
    seg &= 255;
    if (i < 0 || i >= FM_NSHOT)
        return;
    if (shot_seg(set, seg, &bed.shot[i].data, &bed.shot[i].len)) {
        bed.shot[i].data = 0;
        return;
    }
    bed.shot[i].pos = 0;
    bed.shot[i].frac = 0.0f;
    bed.shot[i].step = (float)ZN_SHOT_RATE / FM_SR * fm_clampf(rate, 0.5f, 2.0f);
    bed.shot[i].s0 = bed.shot[i].s1 = 0.0f;
    bed.shot[i].g = (set == FM_SHOT_KNOCK ? 2.0f : 1.0f) * v * v;   /* (the host: ~7 dB under the blinks at Frogs 50;
                                                       * Koi's knock twice as loud, "should be louder") */
    bed.shot[i].pl = 1.41421356f * fm_sin_turns(0.25f - 0.25f * p);
    bed.shot[i].pr = 1.41421356f * fm_sin_turns(0.25f * p);
}

static void bed_set(int which, int pct)
{
    if (which >= 0 && which < BED_N)
        bed.target[which] = (float)(pct < 0 ? 0 : pct > 100 ? 100 : pct) * 0.01f;
}
static void bed_mod(int which, int pct)
{
    if (which >= 0 && which < BED_N)
        bed.mod[which] = (float)(pct < 0 ? 0 : pct > 100 ? 100 : pct) * 0.01f;
}

static void drop_go(drop_t *d, float g, float p, int kind)   /* kind 0 a tick, 1 a plip, 2 a drip (after rain) */
{
    float len;
    if (kind == 0) {                                /* a splash only: a soft tick, no ring */
        len = 0.004f + 0.006f * urand();
        d->g = 0.0f;
        d->f = 0.0f;
        d->df = 1.0f;
        d->tap = 1.1f * g;
        d->tapd = 0.985f;                           /* (~1.5 ms) */
        d->tapf = svf_f(2500.0f + 3500.0f * urand());
    } else {                                        /* a plip: the trapped bubble rings, its pitch rising */
        float u = urand(), hz = kind == 2 ? 500.0f + 700.0f * u : 800.0f + 2200.0f * u * u;
        len = kind == 2 ? 0.05f + 0.04f * urand() : 0.02f + 0.025f * urand();
        d->f = hz / FM_SR;
        d->df = 1.0f + (0.25f + 0.25f * urand()) / (len * FM_SR);   /* (up by a third to two thirds) */
        d->g = g;
        d->tap = 0.4f * g;
        d->tapd = 0.93f;
        d->tapf = svf_f(2500.0f);
    }
    d->ph = 0.0f;
    d->dg = fm_expf(-4.0f / (len * FM_SR));
    d->left = (int32_t)(FM_SR * len);
    d->pl = 1.41421356f * fm_sin_turns(0.25f - 0.25f * p);
    d->pr = 1.41421356f * fm_sin_turns(0.25f * p);
    d->s.lp = d->s.bp = 0.0f;
}
static void drop_start(drop_t *d) { drop_go(d, 0.25f + 0.35f * urand(), urand(), urand() < 0.2f); }   /* (an unseen one,
                                                                                                    * far) */
static void bed_drop(int vel, int pan, int pad)   /* a drop the scene shows: heard at its place, its size its loudness;
                                                     * on a lily pad a tick */
{
    int i, best = 0;
    for (i = 0; i < BED_DROPS; i++) {
        if (bed.d[i].left <= 0) {
            best = i;
            break;
        }
        if (bed.d[i].left < bed.d[best].left)
            best = i;
    }
    drop_go(&bed.d[best], 0.3f + 0.6f * (float)vel * (1.0f / 127.0f), (float)pan * (1.0f / 127.0f), !pad && urand() < 0.4f);
}

/* the next output sample of a head: ADPCM samples at ZN_SAMPLE_RATE, interpolated to the output's (any rate;
 * 16 kHz since, 22.05 kHz before) */
static const int16_t ADPCM_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767};
static const int8_t ADPCM_INDEX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static inline float head_next(head_t *h)
{
    h->frac += (float)ZN_SAMPLE_RATE * (1.0f / FM_SR);
    while (h->frac >= 1.0f) {
        int32_t b = h->pos / ZN_BLOCK, k = h->pos % ZN_BLOCK;
        const uint8_t *blk = h->data + b * ZN_BLOCK_BYTES;
        h->frac -= 1.0f;
        if (k == 0) {                                 /* a block's header: the predictor and the step index */
            h->pred = (int16_t)(blk[0] | blk[1] << 8);
            h->idx = blk[2] > 88 ? 88 : blk[2];
        } else {
            int32_t code = (blk[4 + (k - 1) / 2] >> (4 * ((k - 1) & 1))) & 15, step = ADPCM_STEP[h->idx], dq = step >> 3;
            if (code & 4)
                dq += step;
            if (code & 2)
                dq += step >> 1;
            if (code & 1)
                dq += step >> 2;
            h->pred += (code & 8) ? -dq : dq;
            h->pred = h->pred < -32768 ? -32768 : h->pred > 32767 ? 32767 : h->pred;
            h->idx += ADPCM_INDEX[code & 7];
            h->idx = h->idx < 0 ? 0 : h->idx > 88 ? 88 : h->idx;
        }
        if (++h->pos >= h->len)
            h->pos = 0;
        h->s0 = h->s1;
        h->s1 = (float)h->pred * (1.0f / 32768.0f);
    }
    return h->s0 + (h->s1 - h->s0) * h->frac;
}

/* once a block: the levels glide, the wind walks, the stream picks its next place, rain starts drops */
static void bed_control(void)
{
    int i;
    for (i = 0; i < BED_N; i++)
        bed.level[i] += (bed.target[i] - bed.level[i]) * 0.003f;   /* ~1 s at 32-sample blocks */
    if (bed.level[BED_WIND] > 1e-4f && !bed.style[BED_WIND]) {   /* the breeze: a quiet bed, and the puffs */
        const float BPS = FM_SR / (float)BLK;       /* blocks a second */
        float m = bed.mod[BED_WIND], fc, sh = 0.0f, prog;
        int gust = m > 0.5f && bed.p_lastmod <= 0.5f;
        bed.p_lastmod = m;
        if (gust || (bed.p_phase == 0 && --bed.p_next <= 0)) {   /* a new puff (a gust: a strong one, at once) */
            float side = urand() < 0.5f ? 0.0f : 1.0f;
            if (bed.p_phase == 0 || gust) {
                bed.p_phase = 1;
                bed.p_t = 0;
                bed.p_rise = (int32_t)((gust ? 1.2f : 2.0f + 1.5f * urand()) * BPS);
                bed.p_hold = (int32_t)((0.5f + 2.0f * urand()) * BPS);
                bed.p_fall = (int32_t)((5.0f + 3.5f * urand()) * BPS);
                bed.p_peak = gust ? 1.0f : 0.45f + 0.45f * urand();
                bed.p_pan0 = 0.15f + 0.7f * side * (0.6f + 0.4f * urand());
                bed.p_pan1 = 1.0f - bed.p_pan0;
            }
            bed.p_next = (int32_t)((8.0f + 17.0f * urand()) * BPS);
        }
        if (bed.p_phase) {                          /* raised-cosine rise, a hold, a raised-cosine fall */
            bed.p_t++;
            if (bed.p_phase == 1) {
                sh = 0.5f - 0.5f * fm_sin_turns(0.25f + 0.5f * (float)bed.p_t / (float)bed.p_rise);
                if (bed.p_t >= bed.p_rise) {
                    bed.p_phase = 2;
                    bed.p_t = 0;
                }
            } else if (bed.p_phase == 2) {
                sh = 1.0f;
                if (bed.p_t >= bed.p_hold) {
                    bed.p_phase = 3;
                    bed.p_t = 0;
                }
            } else {
                sh = 0.5f + 0.5f * fm_sin_turns(0.25f + 0.5f * (float)bed.p_t / (float)bed.p_fall);
                if (bed.p_t >= bed.p_fall) {
                    bed.p_phase = 0;
                    sh = 0.0f;
                }
            }
        }
        bed.p_env += (bed.p_peak * sh - bed.p_env) * 0.05f;
        bed.w_walk_v += 0.00015f * noise2() - 0.00004f * bed.w_walk;   /* the bed's own slow drift */
        bed.w_walk_v *= 0.999f;
        bed.w_walk = fm_clampf(bed.w_walk + bed.w_walk_v, -1.0f, 1.0f);
        fc = 520.0f * fm_exp2f(0.15f * bed.w_walk + 1.0f * bed.p_env);   /* ~520 Hz quiet .. ~1 kHz in a puff (from 380, for the FM-1's speaker) */
        bed.w_a = 1.0f - fm_expf(-FM_TWO_PI * fc / FM_SR);
        bed.w_gb = 0.075f * (1.0f + 0.2f * bed.w_walk);   /* (the bed +6.6 dB, the puffs +3 dB:
                                                         * "very faint", now about the recorded breeze's loudness) */
        bed.w_gp = 0.14f * bed.p_env;
        prog = bed.p_phase == 0 ? 1.0f
             : bed.p_phase == 1 ? 0.3f * (float)bed.p_t / (float)bed.p_rise
             : bed.p_phase == 2 ? 0.3f + 0.2f * (float)bed.p_t / (float)bed.p_hold : 0.5f + 0.5f * (float)bed.p_t / (float)bed.p_fall;
        {
            float p = bed.p_pan0 + (bed.p_pan1 - bed.p_pan0) * prog;   /* the puff drifts across */
            bed.w_pl = 1.41421356f * fm_sin_turns(0.25f - 0.25f * p);
            bed.w_pr = 1.41421356f * fm_sin_turns(0.25f * p);
        }
    }
    if (bed.level[BED_STREAM] > 1e-4f && bed.style[BED_STREAM] == FM_WATER_TRICKLE) {   /* the trickle's drops */
        float rate = 55.0f;                                          /* a second: a thin steady pour */
        if (urand() < rate * (float)BLK / FM_SR)
            for (i = 0; i < BED_BUBBLES; i++)
                if (bed.b[i].left <= 0) {
                    bubble_t *b = &bed.b[i];
                    float r = 1.2f + 1.6f * urand(), f0 = 3000.0f / r, d, p = 0.45f + 0.2f * urand();   /* 1..2.5 kHz,
                                                                                                  * near the middle */
                    d = 0.043f * f0 + 0.0014f * f0 * fm_sqrtf(f0);
                    b->ph = 0.0f;
                    b->f = f0 / FM_SR;
                    b->df = b->f * 0.4f * d / FM_SR;
                    b->g = 0.35f + 0.4f * urand();
                    b->dg = fm_expf(-d / FM_SR);
                    b->left = (int32_t)(5.0f / d * FM_SR);
                    b->pl = 1.41421356f * fm_sin_turns(0.25f - 0.25f * p);
                    b->pr = 1.41421356f * fm_sin_turns(0.25f * p);
                    break;
                }
        bed.tr_ph += 6.5f * (float)BLK / FM_SR;                      /* the flow flutters, ~6 Hz, unevenly */
        if (bed.tr_ph >= 1.0f)
            bed.tr_ph -= 1.0f;
        bed.tr_flut = 0.75f + 0.25f * fm_sin_turns(bed.tr_ph) + 0.1f * noise2();
    }
    if (bed.level[BED_STREAM] > 1e-4f && (bed.style[BED_STREAM] == FM_WATER_BUBBLES || bed.style[BED_STREAM] == FM_WATER_SLOW)) {
        int slow = bed.style[BED_STREAM] == FM_WATER_SLOW;           /* bubbles: start some */
        float rate = slow ? 0.8f : 45.0f + 140.0f * bed.mod[BED_STREAM];   /* a second */
        int n = 0;
        bed.mur_walk += (noise2() * 0.02f) - bed.mur_walk * 0.002f;
        while (n < 3 && urand() < rate * (float)BLK / FM_SR * (n ? 0.3f : 1.0f)) {
            for (i = 0; i < BED_BUBBLES; i++)
                if (bed.b[i].left <= 0) {
                    bubble_t *b = &bed.b[i];
                    float r = slow ? 3.5f + 5.0f * urand() : 2.0f + 6.0f * urand() * urand(), f0 = 3000.0f / r, d, p = urand();
                                                                        /* r mm: 375..1500 Hz (slow: 350..860 Hz) */
                    d = 0.043f * f0 + 0.0014f * f0 * fm_sqrtf(f0);                                /* decay a second */
                    b->ph = 0.0f;
                    b->f = f0 / FM_SR;
                    b->df = b->f * 0.6f * d / FM_SR;                    /* it rises as it dies */
                    b->g = (0.25f + 0.75f * urand()) * (r * 0.125f) * (slow ? 1.8f : 1.0f);   /* the bigger, the louder
                                                                                    * (slow: each heard on its own) */
                    b->dg = fm_expf(-d / FM_SR);
                    b->left = (int32_t)(5.0f / d * FM_SR);
                    b->pl = 1.41421356f * fm_sin_turns(0.25f - 0.25f * p);
                    b->pr = 1.41421356f * fm_sin_turns(0.25f * p);
                    break;
                }
            n++;
        }
    }
    {   /* the rain's swell: heavier and lighter by turns, a new aim every 5-20 s, glided to over seconds */
        const float BPS = FM_SR / (float)BLK;
        if (bed.sw <= 0.0f)
            bed.sw = bed.sw_t = 1.0f;
        if (--bed.sw_n <= 0) {
            bed.sw_t = 0.65f + 0.6f * urand();
            bed.sw_n = (int32_t)((5.0f + 15.0f * urand()) * BPS);
        }
        bed.sw += (bed.sw_t - bed.sw) * 0.0002f;
    }
    {   /* after the rain: a minute of drips from the leaves, larger and lower, thinning out */
        const int32_t DRIP = (int32_t)(60.0f * FM_SR / (float)BLK);
        float lrn = fm_maxf(bed.level[BED_RAIN], bed.style[BED_STREAM] == FM_WATER_RAIN ? bed.level[BED_STREAM] : 0.0f);
        if (lrn > 0.05f) {
            bed.drip = DRIP;
            bed.drip_g = fm_minf(1.0f, lrn * 1.5f);
        } else if (bed.drip > 0) {
            float f = (float)bed.drip-- / (float)DRIP;
            if (urand() < (0.08f + 1.2f * f * f) * bed.drip_g * (float)BLK / FM_SR)
                for (i = 0; i < BED_DROPS; i++)
                    if (bed.d[i].left <= 0) {
                        drop_go(&bed.d[i], (0.35f + 0.4f * urand()) * bed.drip_g, urand(), 2);
                        break;
                    }
        }
    }
    if (bed.level[BED_RAIN] > 1e-4f || (bed.level[BED_STREAM] > 1e-4f && bed.style[BED_STREAM] == FM_WATER_RAIN)) {
        float m = bed.level[BED_RAIN] > 1e-4f ? bed.mod[BED_RAIN] : 0.3f;
        float rate = (10.0f + 200.0f * m) * bed.sw;                /* drops a second (~10 light, ~210 at full) */
        if (urand() < rate * (float)BLK / FM_SR)
            for (i = 0; i < BED_DROPS; i++)
                if (bed.d[i].left <= 0) {
                    drop_start(&bed.d[i]);
                    break;
                }
    }
}

/* the frogs' calls into the mix, one block (a call is the scene's: it needs no bed level) */
static void frogs_render(float *ml, float *mr)
{
    int k, i;
    for (i = 0; i < FM_NSHOT; i++) {                         /* recorded calls: ADPCM, one sample after another, interpolated */
        if (!bed.shot[i].data)
            continue;
        for (k = 0; k < BLK; k++) {
            float y;
            bed.shot[i].frac += bed.shot[i].step;
            while (bed.shot[i].frac >= 1.0f) {
                int32_t pos = bed.shot[i].pos, b = pos / ZN_BLOCK, kk = pos % ZN_BLOCK;
                const uint8_t *blk = bed.shot[i].data + b * ZN_BLOCK_BYTES;
                bed.shot[i].frac -= 1.0f;
                if (pos >= bed.shot[i].len) {
                    bed.shot[i].data = 0;
                    break;
                }
                if (kk == 0) {
                    bed.shot[i].pred = (int16_t)(blk[0] | blk[1] << 8);
                    bed.shot[i].idx = blk[2] > 88 ? 88 : blk[2];
                } else {
                    int32_t code = (blk[4 + (kk - 1) / 2] >> (4 * ((kk - 1) & 1))) & 15, st = ADPCM_STEP[bed.shot[i].idx],
                            dq = st >> 3;
                    if (code & 4)
                        dq += st;
                    if (code & 2)
                        dq += st >> 1;
                    if (code & 1)
                        dq += st >> 2;
                    bed.shot[i].pred += (code & 8) ? -dq : dq;
                    bed.shot[i].pred = bed.shot[i].pred < -32768 ? -32768 : bed.shot[i].pred > 32767 ? 32767 : bed.shot[i].pred;
                    bed.shot[i].idx += ADPCM_INDEX[code & 7];
                    bed.shot[i].idx = bed.shot[i].idx < 0 ? 0 : bed.shot[i].idx > 88 ? 88 : bed.shot[i].idx;
                }
                bed.shot[i].pos++;
                bed.shot[i].s0 = bed.shot[i].s1;
                bed.shot[i].s1 = (float)bed.shot[i].pred * (1.0f / 32768.0f);
            }
            if (!bed.shot[i].data)
                break;
            y = bed.shot[i].g * (bed.shot[i].s0 + (bed.shot[i].s1 - bed.shot[i].s0) * bed.shot[i].frac);
            ml[k] += y * bed.shot[i].pl;
            mr[k] += y * bed.shot[i].pr;
        }
    }
}

/* add the beds to the mix (both sides), one block */
static void bed_render(float *ml, float *mr)
{
    int k, i;
    frogs_render(ml, mr);
    float lw = bed.level[BED_WIND], ls = bed.level[BED_STREAM], lr = bed.level[BED_RAIN];
    if (bed.style[BED_STREAM] == FM_WATER_RAIN && ls > lr)   /* the water as rain: the rain bed, at its level */
        lr = ls * 1.4f;                            /* (the host: about the brook's loudness) */
    if (lw > 1e-4f && !bed.style[BED_WIND]) {      /* the breeze: two poles of low-pass on each side's noise */
        float gl = lw * (bed.w_gb + bed.w_gp * bed.w_pl) * 3.0f, gr = lw * (bed.w_gb + bed.w_gp * bed.w_pr) * 3.0f,
              a = bed.w_a;
        for (k = 0; k < BLK; k++) {
            bed.wl1 += a * (noise1() - bed.wl1);
            bed.wl2 += a * (bed.wl1 - bed.wl2);
            bed.wr1 += a * (noise2() - bed.wr1);
            bed.wr2 += a * (bed.wr1 - bed.wr2);
            ml[k] += gl * bed.wl2;
            mr[k] += gr * bed.wr2;
        }
    }
    if (lw > 1e-4f && bed.style[BED_WIND]) {       /* a recording: the breeze and the leaves lift in a gust; the
                                                    * insects come with the night */
        float m = bed.mod[BED_WIND], g = lw * ZN_AIR_GAIN * (bed.style[BED_WIND] == FM_AIR_INSECTS ? 0.25f + 0.75f * m
                                                                                                       : 0.8f + 0.35f * m);
        for (k = 0; k < BLK; k++) {
            ml[k] += g * head_next(&bed.al);
            mr[k] += g * head_next(&bed.ar);
        }
    }
    if (ls > 1e-4f && (bed.style[BED_STREAM] == FM_WATER_BUBBLES || bed.style[BED_STREAM] == FM_WATER_SLOW)) {   /* bubbles,
                                                                                                         * over a murmur */
        float gm = ls * (0.012f + 0.006f * bed.mur_walk) * (bed.style[BED_STREAM] == FM_WATER_SLOW ? 2.5f : 1.0f),
              gb = ls * 0.14f;                         /* (slow: the murmur up, the water still heard between) */
        for (k = 0; k < BLK; k++) {
            float m;
            bed.mur1 += 0.03f * (noise1() - bed.mur1);         /* ~200 Hz */
            bed.mur2 += 0.03f * (bed.mur1 - bed.mur2);
            m = gm * bed.mur2 * 6.0f;
            ml[k] += m;
            mr[k] += m;
        }
        for (i = 0; i < BED_BUBBLES; i++) {
            bubble_t *b = &bed.b[i];
            if (b->left <= 0)
                continue;
            for (k = 0; k < BLK && b->left > 0; k++, b->left--) {
                float y = gb * b->g * fm_sin_turns(b->ph);
                b->ph += b->f;
                if (b->ph >= 1.0f)
                    b->ph -= 1.0f;
                b->f += b->df;
                b->g *= b->dg;
                ml[k] += y * b->pl;
                mr[k] += y * b->pr;
            }
        }
    }
    if (ls > 1e-4f && bed.style[BED_STREAM] == FM_WATER_NOISE) {   /* white noise, a noise a side */
        float g = ls * 0.028f;                     /* (level with the brook, as heard) */
        for (k = 0; k < BLK; k++) {
            ml[k] += g * noise1();
            mr[k] += g * noise2();
        }
    }
    if (ls > 1e-4f && (bed.style[BED_STREAM] == FM_WATER_PINK || bed.style[BED_STREAM] == FM_WATER_DEEP)) {
        int deep = bed.style[BED_STREAM] == FM_WATER_DEEP;
        float gp = ls * (deep ? 0.012f : 0.02f), gb = ls * 0.16f;  /* (the host: about the brook's loudness) */
        for (k = 0; k < BLK; k++) {
            int c;
            for (c = 0; c < 2; c++) {
                float w = c ? noise2() : noise1(), *p = bed.pk[c], y;
                p[0] = 0.99765f * p[0] + w * 0.0990460f;   /* Kellet's economy pink: three poles */
                p[1] = 0.96300f * p[1] + w * 0.2965164f;
                p[2] = 0.57000f * p[2] + w * 1.0526913f;
                y = (p[0] + p[1] + p[2] + w * 0.1848f) * gp;
                if (deep) {                                /* brown: the noise summed, leaking (~16 Hz), less its
                                                            * very lowest (~40 Hz) */
                    bed.br[c] = 0.9977f * bed.br[c] + w * 0.04f;
                    bed.brh[c] += 0.0057f * (bed.br[c] - bed.brh[c]);
                    y += (bed.br[c] - bed.brh[c]) * gb;
                }
                if (c)
                    mr[k] += y;
                else
                    ml[k] += y;
            }
        }
    }
    if (ls > 1e-4f && bed.style[BED_STREAM] == FM_WATER_TRICKLE) {   /* a soft flow, and the drops */
        float gf = ls * 0.04f * bed.tr_flut, f = svf_f(1800.0f), gd = ls * 0.12f;
        for (k = 0; k < BLK; k++) {
            ml[k] += gf * svf_bp(&bed.tr_s[0], noise1(), f, 0.9f);
            mr[k] += gf * svf_bp(&bed.tr_s[1], noise2(), f, 0.9f);
        }
        for (i = 0; i < BED_BUBBLES; i++) {
            bubble_t *b = &bed.b[i];
            if (b->left <= 0)
                continue;
            for (k = 0; k < BLK && b->left > 0; k++, b->left--) {
                float y = gd * b->g * fm_sin_turns(b->ph);
                b->ph += b->f;
                if (b->ph >= 1.0f)
                    b->ph -= 1.0f;
                b->f += b->df;
                b->g *= b->dg;
                ml[k] += y * b->pl;
                mr[k] += y * b->pr;
            }
        }
    }
    if (ls > 1e-4f && bed.style[BED_STREAM] == FM_WATER_BROOK) {   /* the recording: a head a side */
        float g = ls * ZN_WATER_GAIN;              /* (level with the bubbles, as heard on the speaker) */
        for (k = 0; k < BLK; k++) {
            ml[k] += g * head_next(&bed.hl);
            mr[k] += g * head_next(&bed.hr);
        }
    }
    for (i = 0; i < BED_DROPS; i++) {   /* the drops (and the drips after the rain) */
        drop_t *d = &bed.d[i];
        float g = 0.09f * fm_maxf(lr, 0.35f);           /* (a drop seen is heard even under a light rain) */
        if (d->left <= 0)
            continue;
        for (k = 0; k < BLK && d->left > 0; k++, d->left--) {
            float y = d->g * fm_sin_turns(d->ph) + d->tap * svf_bp(&d->s, noise1(), d->tapf, 0.6f);
            d->ph += d->f;
            if (d->ph >= 1.0f)
                d->ph -= 1.0f;
            d->f *= d->df;
            d->g *= d->dg;
            d->tap *= d->tapd;
            ml[k] += g * y * d->pl;
            mr[k] += g * y * d->pr;
        }
    }
}
