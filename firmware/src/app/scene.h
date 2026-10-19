/* SPDX-License-Identifier: GPL-3.0-only */
/* ZEN-1: what a scene is. A scene owns its world (physics, seeded), its picture (render8.c) and the sound
 * its world makes (fm_voice / fm_bed on parts 1..3). The ambient glue in ui.c calls these; nothing here blocks
 * and every loop is bounded (the watchdog). Included by ui.c. */
#pragma once

/* a scene's knobs: four pages of four (KNOB 1..4; FX steps the page), kept in the project and given to the
 * scene at enter (a copy the glue writes back after each turn). kv[0..3] the scene's, kv[4..7] MIX (the levels),
 * kv[8..11] MORE, kv[12..15] DRIFT */
/* the water beds' names (FM_WATER_*: the knob "Water is"; each scene lists them in its own order) */
static const char *const SC_WATER_NAME[FM_NWATER] = {"Bubbles", "Brook", "White", "Pink", "Deep", "Rain", "Trickle",
                                                     "Soft bubble"};

#define SC_NKNOB 20                         /* (the project keeps 20 a scene uses the first 16: four pages) */
#define SC_NPAGE 4                          /* (fewer settings: the scene's, Volume, Texture, Nature) */
#define SC_NUSED 16
#define SC_MOOD_NATURAL 0                   /* the Mood knob: 0 Natural (the scene's own day or night, the
                                             * default), then a scene's own, each holding its time. Every mood keeps the player's tweaks (ui.c amb_mood_*) */
#define SC_NMOOD_MAX 5
/* a function that runs seldom (a picture made, a scene starting, a knob turned): compiled for size, not speed (the
 * app's code must fit its flash) */
#if defined(__clang__)
#define SC_SMALL __attribute__((minsize))
#else
#define SC_SMALL
#endif
typedef struct {
    const char *name;
    uint8_t slot;                          /* its place in the project (knobs, the kept picture): fixed for good */
    void (*defaults)(int16_t *kv);         /* the knobs as a scene starts the first time */
    void (*enter)(uint32_t seed, int16_t *kv);   /* start: palette, sound (parts, beds); background() follows.
                                            * kv lives on (the project): the scene reads and changes it */
    void (*leave)(void);                   /* stop: every voice off, the beds to 0 */
    void (*background)(void);              /* draw r8_bg (at enter, and when the style changes) */
    void (*step)(uint32_t dt_ms);          /* the world moves on; dt is clamped to 50 ms */
    void (*draw)(void);                    /* the frame into r8_fb (start from r8_copy(&r8_fb, &r8_bg)) */
    void (*key)(int k, int on);            /* a panel key, 0..26 = F3..G5 (white and black, as plat_keys) */
    void (*knob)(int i, int32_t detents);  /* knob i of SC_NKNOB (page * 4 + KNOB 1..4) */
    void (*knob_text)(int i, char *name, char *value);   /* each up to 11 characters; an empty name: no knob */
    void (*regrow)(uint32_t seed);         /* a new picture from this seed, the world going on (0: the scene has none) */
    uint8_t sound;                         /* its sounds' version (kept for the project; nothing renewed) */
    void (*renew)(int16_t *kv, int from);  /* (unused: 0) */
    void (*button)(int b);                 /* a button the glue leaves to the scene (B_*), or 0 */
    int nmood;                             /* its moods: Natural and its own, at most SC_NMOOD_MAX */
    void (*mood_kv)(int m, int16_t *kv);   /* mood m's knobs as it was made (the defaults, the mood's set) */
    const char *(*mood_name)(int m);
    uint8_t own_swap;                      /* 1: its own two knobs (12, 13) the other way round on the Nature page
                                            * (Hotaru's Weather first) */
} scene_t;


/* the parts of a scene's frame, timed (testing builds: the HUD's third line, Sakura was slower on the FM-1
 * than the host foretold, and the host can't say why). A scene names its parts (one letter each, SC_NPT at most),
 * calls SC_PT_START() at the top of its draw and SC_PT(i) after part i; the glue shows each, ms a frame (x.y) */
#define SC_NPT 6
#if ZN_HUD
static uint32_t sc_pt_us[SC_NPT], sc_pt_t;
static const char *sc_pt_tag = "";
#define SC_PT_TAGS(s) (sc_pt_tag = (s))
#define SC_PT_START() (sc_pt_t = plat_us())
#define SC_PT(i)                                                                                                     \
    do {                                                                                                             \
        uint32_t n_ = plat_us();                                                                                     \
        sc_pt_us[i] += n_ - sc_pt_t;                                                                                 \
        sc_pt_t = n_;                                                                                                \
    } while (0)
#else
#define SC_PT_TAGS(s) ((void)0)
#define SC_PT_START() ((void)0)
#define SC_PT(i) ((void)0)
#endif

/* a mood (after Avonlea's presets): the knobs it sets, and their values */
typedef struct {
    uint8_t k;
    int16_t v;
} sc_set_t;
static void sc_mood_set(int16_t *kv, const sc_set_t *s, int n)
{
    int i;
    for (i = 0; i < n; i++)
        kv[s[i].k] = s[i].v;
}
/* the mood a knob value holds (always one of the scene's n; anything else, as from an old save, Natural) */
static int sc_mood_base(int v, int n) { return v >= 0 && v < n ? v : SC_MOOD_NATURAL; }

/* a mood's time of day reached gently: over 30 s,
 * eased at both ends, the shorter way round (Night to Morning through the small hours; Afternoon to Morning back
 * through the day, not round through a whole night) */
#define SC_GLIDE_S 30.0f
typedef struct {
    float hold_was, from, dist, t;
} sc_glide_t;
static void sc_glide_at(sc_glide_t *g, float hold)  /* (a scene starting: there already) */
{
    g->hold_was = hold;
    g->t = SC_GLIDE_S;
}
static float sc_glide(sc_glide_t *g, float day, float hold, float dt)   /* the day now, toward a mood's (hold >= 0) */
{
    float u;
    if (hold != g->hold_was) {                      /* a new mood: from here, the shorter way */
        g->hold_was = hold;
        g->from = day;
        g->dist = hold - day;
        g->dist -= fm_floorf(g->dist + 0.5f);
        g->t = 0.0f;
    }
    if (g->t >= SC_GLIDE_S)
        return hold;
    g->t += dt;
    u = fm_clampf(g->t / SC_GLIDE_S, 0.0f, 1.0f);
    u = u * u * (3.0f - 2.0f * u);
    day = g->from + g->dist * u;
    return day - fm_floorf(day);
}

/* rain falling (when rain began "all the drops seem to fall together"): each drop comes in on its
 * own, at a random moment and place; how many fall follows *amt, which eases toward the rain (want, 0..1) over ~7 s
 * and counts squared, so a few come first, then more; when it stops, the drops in the air fall on out */
typedef struct {
    float x, y;
    uint8_t on;
} sc_drop_t;
static float sc_frand(void);
static void sc_rain_step(sc_drop_t *d, int nd, float *amt, float want, float vx, float vy, float dt)
{
    int i, n;
    *amt += fm_clampf(want - *amt, -dt * 0.15f, dt * 0.15f);
    n = (int)(*amt * *amt * (float)nd + 0.5f);
    for (i = 0; i < nd; i++) {
        if (d[i].on) {
            d[i].y += vy * dt;
            d[i].x += vx * dt;
            if (d[i].y > 240.0f || d[i].x > 250.0f) {
                d[i].on = (uint8_t)(i < n);           /* (on again from the top, or done) */
                d[i].x = -20.0f + 260.0f * sc_frand();
                d[i].y = -10.0f * sc_frand();
            }
        } else if (i < n && sc_frand() < dt * 1.2f) { /* (its own moment to begin) */
            d[i].on = 1;
            d[i].x = -20.0f + 260.0f * sc_frand();
            d[i].y = -10.0f * sc_frand();
        }
    }
}

static uint32_t sc_rnd_state = 1u;
static uint32_t sc_rnd(void)                /* xorshift32: the scenes' randomness, seeded at enter */
{
    uint32_t x = sc_rnd_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return sc_rnd_state = x;
}
static float sc_frand(void) { return (float)(sc_rnd() >> 8) * (1.0f / 16777216.0f); }   /* 0 .. 1 */
static void sc_seed(uint32_t s) { sc_rnd_state = s ? s : 1u; }

/* a list knob: one choice a detent, past the last back to the first and past the first to the last */
static void sc_knob_wrap(int16_t *v, int32_t e, int lo, int hi)
{
    int32_t n = hi - lo + 1, x = ((int32_t)*v - lo + e) % n;
    if (x < 0)
        x += n;
    *v = (int16_t)(lo + x);
}

/* Air is: Soft, Breeze, Insects (FM_AIR_*; Leaves went for the room: a saved Leaves is the breeze) */
static void sc_air_wrap(int16_t *v, int32_t e)
{
    static const int16_t AIRS[3] = {FM_AIR_BREEZE_SYNTH, FM_AIR_BREEZE, FM_AIR_INSECTS};
    int i = *v == FM_AIR_INSECTS ? 2 : *v == FM_AIR_BREEZE_SYNTH ? 0 : 1;
    i = (int)(((i + e) % 3 + 3) % 3);
    *v = AIRS[i];
}

/* a knob's value moves by detents x step, inside lo..hi; sc_knob_mul: the glue's acceleration for this turn (a slow
 * turn 1 a detent, a quick one up to 6, as the instrument's knobs: every number steps by 1) */
static int32_t sc_knob_mul = 1;
static void sc_knob_turn(int16_t *v, int32_t e, int lo, int hi, int step)
{
    int32_t x = (int32_t)*v + e * step * sc_knob_mul;
    *v = (int16_t)(x < lo ? lo : x > hi ? hi : x);
}

/* ---------------------------------------------------------------- the moon --- */
/* the moon of Hotaru and Sakura: its size and phase each picture, its face from a real map, a soft glow round it */
typedef struct {
    float r, light, off, side;                  /* its radius (px at 240); how much is lit (0.3..1); its terminator
                                                 * (c: -1 full .. 0.6 a crescent); which side is lit (1 right) */
} sc_moon_t;
/* its size and phase: full and gibbous most nights, a crescent now and then; rs: its size in this scene */
SC_SMALL static void sc_moon_pick(sc_moon_t *m, float rs)
{
    float u = sc_frand();
    m->r = 8.5f * (1.0f + 1.4f * sc_frand()) * rs;   /* (larger, up to ~20 px) */
    /* the terminator, a half ellipse: lit where x >= c * sqrt(r^2 - y^2) (x mirrored when waning): c -1 full,
     * -0.5 gibbous, 0 half, 0.6 a crescent (a disc of shadow had cut the half and gibbous moons concave) */
    m->light = u < 0.4f ? 1.0f : u < 0.7f ? 0.8f : u < 0.9f ? 0.55f : 0.3f;
    m->off = u < 0.4f ? -1.0f : u < 0.7f ? -0.5f : u < 0.9f ? 0.0f : 0.6f;
    m->side = sc_frand() < 0.5f ? 1.0f : -1.0f;  /* waxing (lit on the right) or waning */
}
/* another moon (a new night or picture) from its own seed: the scene's randomness left as it was */
SC_SMALL static void sc_moon_repick(sc_moon_t *m, float rs, uint32_t salt)
{
    uint32_t keep = sc_rnd_state;
    sc_seed(salt);
    sc_moon_pick(m, rs);
    sc_rnd_state = keep;
}
/* when it rises, by its phase, as in life (0 dusk .. 1 dawn): a full moon at dusk, a waxing half already high, a
 * waxing crescent low in the west, a waning moon later in the night. Where it is across its arc at `night`: 0 rising
 * (the left, east) .. 1 setting (the right, west); outside 0..1 it is down */
static float sc_moon_t_of(float night, float off, float side)
{
    float age = off < -0.99f ? 0.5f : off < -0.4f ? 0.38f : off < 0.3f ? 0.25f : 0.12f,
          rise = side > 0.0f ? (age - 0.5f) * 2.0f : (0.5f - age) * 2.0f;
    return night - rise;
}

/* the moon's face: the near side of a real map (the drawn seas read as "a peanut"): NASA's CGI Moon
 * Kit, public domain, shrunk by tools/zen_moon.py (assets/images/README.md) */
#include "zen_moon.h"
SC_SMALL static int sc_moon_map(float u, float v)        /* its brightness at (u, v) in radii (x right, y down): 1 .. 255 */
{
    float fx = (u * 0.5f + 0.5f) * (float)ZN_MOON_N - 0.5f, fy = (v * 0.5f + 0.5f) * (float)ZN_MOON_N - 0.5f, ax, ay;
    int x0 = (int)fm_floorf(fx), y0 = (int)fm_floorf(fy), x1, y1, a, b, c, d;
    ax = fx - (float)x0;
    ay = fy - (float)y0;
    x0 = x0 < 0 ? 0 : x0 >= ZN_MOON_N ? ZN_MOON_N - 1 : x0;
    y0 = y0 < 0 ? 0 : y0 >= ZN_MOON_N ? ZN_MOON_N - 1 : y0;
    x1 = x0 + 1 < ZN_MOON_N ? x0 + 1 : x0;
    y1 = y0 + 1 < ZN_MOON_N ? y0 + 1 : y0;
    a = ZN_MOON[y0 * ZN_MOON_N + x0];
    b = ZN_MOON[y0 * ZN_MOON_N + x1];
    c = ZN_MOON[y1 * ZN_MOON_N + x0];
    d = ZN_MOON[y1 * ZN_MOON_N + x1];
    if (!a || !b || !c || !d) {                     /* (at the rim: the nearest that is on the disc) */
        int m = a > b ? a : b;
        m = m > c ? m : c;
        return m > d ? m : d;
    }
    return (int)((float)a + ((float)b - (float)a) * ax + ((float)c - (float)a) * ay +
                 ((float)a - (float)b - (float)c + (float)d) * ax * ay);
}

/* the moon's face, worked out once (the seas a pixel at a time were most of the moon's 5.9 ms a frame on the
 * FM-1): a stamp of its disc (0 outside it; 1 lit, 2 toward the rim, 3 a sea, 4 in shadow), redone only when the
 * picture or the scale changes (sc_mst_k < 0); the moon moves by whole pixels across the night. One for both
 * scenes: only one is ever drawn */
#define SC_MST 48
static uint8_t sc_mst[SC_MST * SC_MST];
static float sc_mst_k = -1.0f;                  /* the scale it was made at (< 0: to be made) */
SC_SMALL static void sc_moon_stamp(const sc_moon_t *m, float k)
{
    float r = m->r * k, c = (float)(SC_MST / 2), soft = fm_maxf(1.2f, 0.12f * r), r2 = r * r, ir2 = 1.0f / r2;
    int32_t x, y;
    for (y = 0; y < SC_MST; y++) {
        float dy = (float)y + 0.5f - c, dy2 = dy * dy,
              half = m->off < -0.99f ? 0.0f : m->off * fm_sqrtf(fm_maxf(0.0f, r2 - dy2));
        for (x = 0; x < SC_MST; x++) {
            uint8_t *p = sc_mst + y * SC_MST + x;
            float dx = (float)x + 0.5f - c, d2 = dx * dx + dy2;
            *p = 0;
            if (d2 < r2) {
                float u = dx / r, v = dy / r, rr = d2 * ir2,
                      sh = m->off < -0.99f ? -1e9f : half - dx * m->side,   /* > 0: in shadow, px */
                      sea = fm_clampf((165.0f - (float)sc_moon_map(u, v)) * (1.0f / 110.0f), 0.0f, 1.0f);   /* (the
                                                       * map: its dark seas, dithered into the highlands) */
                if (sh > soft) {                      /* in the shadow (the phase): the faint earthshine */
                    *p = 4;
                    continue;
                }
                if (sea > 0.0f && r8_dith(x, y, (int)(sea * 16.0f)))
                    *p = 3;
                else if (rr > 0.6f && r8_dith(x, y, (int)((rr - 0.6f) * 2.5f * 12.0f)))
                    *p = 2;                 /* a gentle darkening toward the rim */
                else
                    *p = 1;
                if (sh > -soft && !r8_dith(x, y, (int)((soft - sh) / (2.0f * soft) * 16.0f)))
                    *p = 4;                 /* the terminator: lit fading into shadow */
            }
        }
    }
    sc_mst_k = k;
}

/* the moon at (cx, cy) on the screen (px), its face from the stamp in colors moon .. moon + 3 (lit, toward the rim, a
 * sea, in shadow), only over what is below `top` (the sky: hills and trees stand in front); its glow round it in
 * nh steps (halo the brightest). How much of its disc is seen: 0..1 */
static float sc_moon_draw(surf_t *s, float k, const sc_moon_t *m, float cx, float cy, uint8_t top, uint8_t moon,
                          uint8_t halo, int nh)
{
    float r = m->r * k, hr = r * (1.4f + 0.1f * m->light) + 1.0f * k,   /* (a thin soft rim, about a
                                                       * quarter of its width: the moon's light on the air) */
          r2 = r * r, hr2 = hr * hr, ih = 1.0f / (hr2 - r2);
    int32_t ox = (int32_t)cx - SC_MST / 2, oy = (int32_t)cy - SC_MST / 2,
            x, y, x0, x1, y0, y1;
    float mx = (float)(ox + SC_MST / 2), my = (float)(oy + SC_MST / 2);   /* (on whole pixels, as the stamp) */
    uint32_t on = 0, seen = 0;
    if (sc_mst_k != k)
        sc_moon_stamp(m, k);
    x0 = (int32_t)(mx - hr);
    x1 = (int32_t)(mx + hr);
    y0 = (int32_t)(my - hr);
    y1 = (int32_t)(my + hr);
    for (y = y0 < 0 ? 0 : y0; y <= y1 && y < s->h; y++) {
        float dy = (float)y + 0.5f - my, dy2 = dy * dy;
        uint8_t *row = s->px + y * s->w;
        if (dy2 > hr2)
            continue;
        for (x = x0 < 0 ? 0 : x0; x <= x1 && x < s->w; x++) {
            uint8_t *p = row + x;
            float dx = (float)x + 0.5f - mx, d2 = dx * dx + dy2;
            if (d2 < r2)
                on++;                                 /* (the disc on the screen; below, the part seen) */
            if (d2 >= hr2 || *p >= top)               /* only over the sky: the hills and trees stand in front */
                continue;
            if (d2 < r2) {
                int32_t sx = x - ox, sy = y - oy;
                if (sx >= 0 && sy >= 0 && sx < SC_MST && sy < SC_MST && sc_mst[sy * SC_MST + sx]) {
                    *p = (uint8_t)(moon + sc_mst[sy * SC_MST + sx] - 1);
                    seen++;
                }
            } else {
                float u = (d2 - r2) * ih, g = (1.0f - u) * (1.0f - u) * 4.0f;   /* (0..1 across the glow, in squared
                                                       * distance; its light, 4 at the moon .. 0: the levels dithered
                                                       * into each other, no ring) */
                int f = (int)(g * 16.0f), lv = (f >> 4) + r8_dith(x, y, f & 15);
                if (lv > 0)
                    *p = (uint8_t)(halo + nh - (lv > nh ? nh : lv));
            }
        }
    }
    return on ? (float)seen / (float)on : 0.0f;
}

