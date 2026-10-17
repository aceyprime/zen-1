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
