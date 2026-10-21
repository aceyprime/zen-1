/* SPDX-License-Identifier: GPL-3.0-only */
/* Hotaru 蛍: fireflies over a summer field at night, a slow stream across it. Reeds and grass sway in silhouette, a
 * moon (its size and phase the picture's own) crosses the sky, now and then a shooting star, and in Natural the night
 * goes from dusk blue through deep night to dawn every half hour (the most fireflies early, fewer toward dawn).
 * Each firefly blinks on its own clock and nudges the others toward its timing (coupled oscillators, Kuramoto's
 * model): over minutes the field falls into one gentle pulse, holds it, and drifts apart again as the coupling ebbs
 * (the Sync knob: how strong it can get). The fireflies keep drifting all the while, and glow softly between blinks;
 * their glows waver in the stream. A blink is a note, its pitch the firefly's height (or where it is across, or a
 * note of its own); blinks that come together are one soft chord of two or three notes, and a token bucket
 * (Activity) keeps them sparse. Under them: pond frogs calling and answering over a distant chorus of others, an
 * owl now and then far off, crickets, a quiet pad.
 *
 *   KNOB 1 Activity (fireflies, notes)   2 Time   3 Tone (Bell, Furin, Drop, Koto, Warm)   4 Space (reverb)
 *   MIX: Notes, Drone, Air, Frogs.   MORE: Echo, Water is (off by default), Air is, Owl (how often; 0: never)
 *   DRIFT: Change (the key / scale), Tape, Sync (how strongly they fall into step), Pitch is (Height, Across, Own)
 *   White keys  a firefly rises from the grass at that note's height and joins the rhythm
 *   Black keys  a note each, low to high over the white keys' range, and a dark firefly near its height
 *               flashes (none near: the nearest drifts there); the top one: a breeze
 *               rises from the left, the reeds lean slowly, and it carries the fireflies away to the right
 *
 * from on the FM-1: fewer fireflies, drifting on (they looked parked in the pulse), a steady soft
 * glow; the stream, the moon's size and phase, shooting stars with a chime, darker toward the front, the panel's
 * dark curve; frogs as click trains (a recorded frog only that); the owl for the
 * shakuhachi; the breeze rises slowly.
 *
 * The world is 240 x 240; drawing scales by r8_fb.w / 240 for pixel art. Every loop has a fixed bound. */
#define HT_MAXF 14
#define HT_NREED 18
#define HT_NQ 16
#define HT_HORIZON 150.0f
#define HT_WTOP 171.0f                          /* the stream: its far and near edges (they wave a little) */
#define HT_WBOT 206.0f                          /* (deeper, more of the reflections; was 187) */
#define HT_YTOP 112.0f                          /* the fireflies fly between these heights: low, over the stream and its
                                                 * grasses, as real ones do (a note is its height in this band) */
#define HT_YBOT 214.0f
#define HT_NFF 26                               /* far fireflies: tiny points along the far bank and the trees */
#define HT_KLO 6                                /* the notes: white keys 6..14 (~260 Hz up: the speaker carries them) */
#define HT_KN 9
#define HT_NTONE 5
#define HT_SOUND 6                              /* sounds' version: 2 levels set by ear, the owl; 3 the
                                                 * levels again */
#define HT_DARK 80                              /* the panel's dark curve (render8.c r8_dark): picked by eye for every
                                                 * scene with GLO on the FM-1 (100 in 110 from the
                                                 * calibration photo) */
enum { HT_SKY = 0, HT_STAR = 64, HT_HALO = 68, HT_MOON = 72, HT_HILL = 76, HT_GROUND = 80, HT_REED = 88, HT_GLOW = 96,
       HT_WATER = 104, HT_SHINE = 112, HT_SHOOT = 116, HT_FORE = 120, HT_STW = 128, HT_STB = 134,
       HT_STY = 136, HT_RAIN = 138, HT_FOGW = 140, HT_MW = 142, HT_TREE = 144, HT_STONE = 148, HT_RIM = 152,
       HT_HOUSE = 154, HT_FOGG = 156, HT_CLR = 160, HT_MVEIL = 232, HT_FOGL = 244 };   /* (each cloud's own shades: 18 a cloud, HT_CLR_N; the moon
                                                       * behind cloud: its three shades, four steps each) */
#define HT_CLR_N 18                             /* a cloud's shades: six by the sky's own color, six half lit by the
                                                 * moon, six lit (its silver edge) */   /* (the far woods and the branch over us; the stream's stones; moonlight on the ridge; a
                            * farmhouse window and its glow) */   /* (rain, light and dim; the mist's fog, two) */   /* (the sky 64 steps: the stars, six whites faint .. bright, a blue-white and a
                          * warm star with their halos) */
#define HT_KEY_ST 0.44f                    /* a white key's note: as loud as the fireflies' own (their blinks, measured on
 * the host: 0.44 on average, the same tone) (chose each scene's own); the low keys lifted:
 * HT_KEY_LIFT at the lowest. Before: 0.5, then 0.31 */
#define HT_KEY_LIFT 0.3f
#define HT_NCLOUD 4                             /* (the first far off, low and faint) */
/* the knobs (fewer, Mood first, named by what they do; by number; ui.c AMB_AT puts them on the four pages: Scene,
 * Volume, Nature, Music). Gone: Time (always 100 %), Change (the Key page's Drift), Tape, Sync (the moods set it: ht.sync) */
enum { HK_MOOD, HK_ACT, HK_TONE, HK_SPACE, HK_NOTES, HK_DRONE, HK_AIR, HK_FROGS, HK_AIRIS, HK_WATERIS, HK_ECHO, HK_DEPTH,
       HK_OWL, HK_WEATHER, HK_MELODY, HK_ARC };
static const int16_t HT_KNOB[SC_NKNOB][4] = {   /* lo, hi, step, default; SPACE is the reverb (P_REVERB) */
    {0, 4, 1, 0}, {0, 100, 1, 30}, {0, HT_NTONE - 1, 1, 0}, {0, 0, 0, 0},   /* Mood (Natural), Activity,
                                                                              * Instrument (Bell), Space */
    {0, 100, 1, 50}, {0, 100, 1, 30}, {0, 100, 1, 20}, {0, 100, 1, 30},       /* Volume: levels set by ear */
    {0, 3, 1, FM_AIR_INSECTS}, {0, FM_WATER_RAIN + 1, 1, 0}, {0, 100, 1, 40}, {0, 100, 1, 0},   /* Air Type,
                                                                              * Water Type (off, or a bed), Echo, Warmth */
    {0, 100, 1, 40}, {0, 3, 1, 0}, {0, 3, 1, 0}, {0, 100, 1, 50},             /* Nature: Owl Calls, Weather (Auto),
                                                                              * Phrases (Height ...), Tide */
};
/* the moods (after Avonlea's presets; Natural first, the night going round; the others hold theirs,
 * with their own moon): Firefly Festival, Quiet Night, Deep Night, Dusk; each sets the fireflies' Sync too (ht.sync) */
#define HT_NMOOD 5                              /* (in the order the night meets them: Dusk first) */
static const char *const HT_MOOD_NAME[HT_NMOOD] = {"Natural", "Dusk", "Festival", "Quiet Night", "Deep Night"};
static const float HT_MOOD_NIGHT[HT_NMOOD] = {-1.0f, 0.04f, 0.12f, 0.32f, 0.5f};   /* the night each holds (0 dusk,
                                                       * 0.5 the deepest): Festival early, when Genji fireflies are most */
static const uint8_t HT_MOOD_SYNC[HT_NMOOD] = {50, 50, 80, 50, 40};
static const int8_t HT_MOOD_SCALE[HT_NMOOD] = {-1, SC_YO, SC_YO, SC_MINYO, SC_IN};   /* (In only for Deep Night: sparse,
                                                       * dark; Natural: Yo at dusk and dawn, Min'yo through the night) */   /* (each its own key and scale) */
static const float HT_MOOD_KEY[HT_NMOOD] = {0.0f, 0.0f, 0.0f, 0.0f, -500.0f};
static const sc_set_t HT_MOODS[HT_NMOOD][9] = {
    {{HK_ACT, 30}, {HK_NOTES, 50}, {HK_DRONE, 30}, {HK_FROGS, 30}, {HK_ECHO, 40}, {HK_OWL, 40}, {HK_DEPTH, 0},
     {HK_WEATHER, 0}, {HK_AIR, 20}},
    {{HK_ACT, 45}, {HK_NOTES, 55}, {HK_DRONE, 25}, {HK_FROGS, 45}, {HK_ECHO, 35}, {HK_OWL, 20}, {HK_DEPTH, 15},
     {HK_WEATHER, 1}, {HK_AIR, 25}},                /* (Dusk, the sky's last glow, the first fireflies rising,
                                                     * the frogs loud; a gentle, warm sound) */
    {{HK_ACT, 75}, {HK_NOTES, 60}, {HK_DRONE, 25}, {HK_FROGS, 30}, {HK_ECHO, 35}, {HK_OWL, 30}, {HK_DEPTH, 0},
     {HK_WEATHER, 1}, {HK_AIR, 20}},
    {{HK_ACT, 30}, {HK_NOTES, 50}, {HK_DRONE, 30}, {HK_FROGS, 30}, {HK_ECHO, 40}, {HK_OWL, 40}, {HK_DEPTH, 10},
     {HK_WEATHER, 0}, {HK_AIR, 20}},
    {{HK_ACT, 12}, {HK_NOTES, 40}, {HK_DRONE, 40}, {HK_FROGS, 20}, {HK_ECHO, 60}, {HK_OWL, 70}, {HK_DEPTH, 45},
     {HK_WEATHER, 1}, {HK_AIR, 15}},                /* (Deep Night clear, mist and rain hid its stars) */
};
static const char *const HT_WEATHER_NAME[4] = {"Auto", "Clear", "Mist", "Rain"};
#define HT_NDROP 40
#define HT_NRING 18                             /* (three times the earlier six) */
#define HT_NSPL 4
static const uint8_t HT_TONE_SLOT[HT_NTONE] = {ZN_BELL, ZN_FURIN, ZN_DROP, FM_P_KOTO, FM_P_KOTO_WARM};
static const char *const HT_TONE_NAME[HT_NTONE] = {"Bell", "Furin", "Drop", "Koto", "Warm"};
#define HT_ID_STAR (FM_VOICE_ID + 55)           /* a shooting star's chime */

typedef struct {
    float x, y, h, tr, sp;                      /* where, heading (turns), its turning (turns a second), speed */
    float ph, w;                                /* the blink's phase (turns) and its own rate (blinks a second) */
    float b, fade;                              /* a blink (1, falling); being there (0..1: rising, settling) */
    float ty, ty_t;                             /* a height to rise to (born, or a key's), and for how long more */
    float perch_t;                              /* resting on a reed's top for this long more */
    float flash;                                /* a key's flash: a big bright glow, fading */
    int perch, key, settling, on, blow;          /* blow: carried off by the clearing breeze */
    uint8_t heike;                              /* a Heike-botaru: smaller, its flashes quicker, flickering, on its own */
} fly_t;

typedef struct {                                /* a frog's call: its recorded calls to go */
    uint32_t next;
    int keros, pan, vel, on;
} frog_t;

#define HT_NSTAR 60
static struct {
    int16_t x, y;                               /* where (world px) */
    uint8_t lv, tint, appear;                   /* its own brightness, 0 faint .. 5 the brightest (steady); its tint (0
                                                 * white, 1 blue-white, 2 warm); how deep the night before it shows */
} ht_st[HT_NSTAR];
static fly_t ht_f[HT_MAXF];
static struct {                                 /* the far fireflies: a point each, blinking with the field */
    float x, y, ph, w, b;
} ht_ff[HT_NFF];
#define HT_NSN 7
static struct {                                 /* stones in the stream, the water riffling past them */
    float x, y, rx;
} ht_sn[HT_NSN];
static struct {                                 /* night clouds: a few soft puffs, drifting by now and then */
    float x, y, v, wait;
    int np;
    float px[5], py[5], pr[5];
} ht_c[HT_NCLOUD];
static frog_t ht_fr[3];
static struct {
    float bx, by, h, lean, ph, top_x, top_y;
    uint8_t layer, cattail;
} ht_r[HT_NREED];
static struct {
    uint32_t at;
    int id, part, vel, pan, dur;
    float cents;
} ht_q[HT_NQ];
static struct {
    uint32_t seed;
    int16_t *kv;
    float t, night, depth, gust, gust_to, gust_dir, wave_ph, wave_per, wave, r_sync, r_avg, tokens, spawn_acc;
    float r_min;                                /* the lowest 20 s average after the first minute (the host's tests) */
    float frog_next, owl_next, pulse_left;
    sc_moon_t mn;                               /* the moon: its size and phase (scene.h) */
    float wt_ph, wb_ph;                         /* the stream's banks' waves */
    struct {
        float x, y, vx, vy, t;                  /* a shooting star: its head, velocity, age (s); t < 0: none */
    } star;
    float star_next;
    int glint[3];                               /* up to three stars slowly brightening and fading back, or -1, each */
    float glint_t[3], glint_next[3];            /* on its own random clock */
    uint32_t ms;
    int pulse_n, pulse_key[3], insects_sent, rel_key;
    float lean;                                 /* the reeds' lean with the breeze: eased, never a jump */
    uint32_t key_flashes;
    float pulse_x[3];
    uint32_t blinks, pulses, frogcalls, owls, released, scatters, stars;
    int wx;                                     /* the weather: 0 clear, 1 mist, 2 rain (what it is heading to) */
    float mist, rain, wx_next;                  /* how misty, how rainy now (0..1, eased); seconds to the next change */
    float deep;                                 /* the Deep Night mood (0..1, eased): the darkest sky, more stars, the
                                                 * Milky Way */
    sc_glide_t glide;                           /* the way to a mood's night */
    float mn_vis;                             /* how much of the moon's disc is seen this frame, 0..1 (behind
                                                 * the hills or below them, none: no light on the stream) */
    float rain_dt;                              /* the rain's time since its drops last moved; which frame */
    float drops;                                /* how much rain is falling (eased; sc_rain_step) */
    uint8_t rain_ph;
    int depth_sent;                             /* (the filter last asked for: Depth and the weather's) */
    float hold;                                 /* the night a mood holds, or -1: it goes round (Natural) */
    float emerge;                               /* Natural: the fireflies coming out after dusk (0.25 .. 1 over ~4 min) */
    int sync;                                   /* how readily the fireflies fall into step (the mood's) */
    float moon_top;                             /* the moon's highest this night: its arc's height */
    float nat_light, nat_off, nat_side;         /* the night's own moon (a mood may show another) */
    uint32_t pings, weathers;
    int wx_fast;                                /* the weather changed by hand: it comes quickly */
    float hx;                                   /* the farmhouse window's place along the horizon */
    uint8_t moons;                              /* the nights since the picture's own (each its own moon) */
    uint8_t mn_made;                            /* the picture's moon picked (drawing it again keeps tonight's) */
    struct {
        float t;
        int reed;
    } spl[HT_NSPL];                             /* a drop on a reed, catching the light */
} ht;
static sc_drop_t ht_drop[HT_NDROP];            /* rain falling (each on its own: sc_rain_step) */
static struct {
    float x, y, t;
} ht_ring[HT_NRING];                            /* rain's rings on the stream */

static float ht_scale(void) { return (float)r8_fb.w / 240.0f; }
SC_SMALL static uint32_t ht_mix(uint32_t a, uint32_t b, float t)   /* two colors, t 0..1 */
{
    uint32_t c = 0;
    int sh;
    t = fm_clampf(t, 0.0f, 1.0f);
    for (sh = 0; sh < 24; sh += 8) {
        float x = (float)((a >> sh) & 255u), y = (float)((b >> sh) & 255u);
        c |= (uint32_t)(x + (y - x) * t + 0.5f) << sh;
    }
    return c;
}
static float ht_wtop(float x) { return HT_WTOP + 2.5f * fm_sin_turns(x / 240.0f * 1.3f + ht.wt_ph); }
static float ht_wbot(float x) { return HT_WBOT + 3.0f * fm_sin_turns(x / 240.0f * 1.1f + ht.wb_ph); }

/* ------------------------------------------------------------ sounds --- */
static void ht_q_add(uint32_t delay_ms, int id, int part, float cents, int vel, int pan, int dur)
{
    int i;
    for (i = 0; i < HT_NQ; i++)
        if (!ht_q[i].at) {
            ht_q[i].at = ht.ms + delay_ms + 1u;
            ht_q[i].id = id;
            ht_q[i].part = part;
            ht_q[i].cents = cents;
            ht_q[i].vel = vel;
            ht_q[i].pan = pan;
            ht_q[i].dur = dur;
            return;
        }
}

static int ht_key_of(const fly_t *f)            /* a firefly's note now (Pitch is) */
{
    float u;
    if (ht.kv[HK_MELODY] == 2)
        return f->key;
    u = ht.kv[HK_MELODY] == 1 ? f->x / 240.0f : (HT_YBOT - f->y) / (HT_YBOT - HT_YTOP);
    if (ht.kv[HK_MELODY] == 3)
        return cd_loop_pick(fm_clampf(u, 0.0f, 0.999f));   /* (Loops: the loop at its height) */
    u = fm_clampf(u, 0.0f, 0.999f);
    return HT_KLO + (int)(u * (float)HT_KN);
}
static float ht_y_of_white(int key)             /* the height of a key's note (sixteen notes over the field) */
{
    float u = ((float)(key - HT_KLO) + 0.5f) / (float)FM_NWHITE;
    return HT_YBOT - fm_clampf(u, 0.0f, 1.0f) * (HT_YBOT - HT_YTOP);
}

/* the blinks of a moment become one soft chord: the first note at once, the others strummed after it; two notes
 * at most at the default Activity (three with more): in step, the field pulses every ~5 s */
static void ht_pulse_play(void)
{
    int i, j, n = 0, keys[3], most = 2 + (ht.kv[HK_ACT] >= 60);
    float xs[3];
    for (i = 0; i < ht.pulse_n && n < most; i++) {   /* distinct notes */
        for (j = 0; j < n && keys[j] != ht.pulse_key[i]; j++)
            ;
        if (j == n) {
            keys[n] = ht.pulse_key[i];
            xs[n++] = ht.pulse_x[i];
        }
    }
    ht.pulse_n = 0;
    if (!n || ht.tokens < 1.0f || cd_quiet())
        return;                                 /* sparse (or the notes resting): these blink silently */
    ht.tokens -= 1.0f;
    ht.pulses++;
    for (i = 0; i < n; i++) {
        float ml = ht.mn.light, st = ((n > 1 ? 0.32f : 0.42f) + 0.18f * sc_frand()) * (0.6f + 0.6f * ml);   /* (more strongly) */   /* (the
                                                       * moon: brighter full, darker as a crescent) */
        if (sc_frand() < 0.45f * (ml - 0.3f) / 0.7f && keys[i] + 5 < FM_NWHITE)
            keys[i] += 5;                             /* (a full moon: now and then an octave up) */
        else if (sc_frand() < 0.45f * (1.0f - ml) / 0.7f && keys[i] - 5 >= 0)
            keys[i] -= 5;                             /* (a crescent: now and then an octave down) */
        if (i == 0)
            cd_note_key(keys[0], xs[0] / 240.0f, st, 1400, 1, 0u);
        else                                    /* (strummed: 80..200 ms apart; echoes come from the first) */
            ht_q_add((uint32_t)(i * (80 + (int)(sc_rnd() % 60u))), CD_PLUCK + (cd.pluck_n++ & 15), 1, cd_cents(keys[i]),
                     38 + (int)(70.0f * st * 0.85f), cd_pan(xs[i] / 240.0f), 1400);
    }
}

/* frogs: pond frogs, a UK common frog's own calls (CC0, zen_samples.h: six, short and long), each a little
 * faster or slower (another frog), one or two a call. (The tree frog went, "not pleasant"; the synthesized
 * croak in "sounds awful".) Frogs 0 and 1 are the near two (one answers the other), frog 2 the far chorus:
 * soft, now and then, from all about. Each has its own bed.c slot. */
static void ht_frog_call(int i, uint32_t delay, int far)
{
    frog_t *f = &ht_fr[i];
    f->keros = far ? 1 : 1 + (int)(sc_rnd() % 2u);
    f->pan = far ? 10 + (int)(sc_rnd() % 108u) : (i ? 100 : 28);
    f->vel = far ? 40 + (int)(sc_rnd() % 14u) : 84 + (int)(sc_rnd() % 20u);
    f->next = ht.ms + delay + 1u;
    f->on = 1;
    if (!far)
        ht.frogcalls++;
}

static void ht_frogs_step(void)
{
    int i;
    for (i = 0; i < 3; i++) {
        frog_t *f = &ht_fr[i];
        int seg;
        if (!f->on || (int32_t)(ht.ms - f->next) < 0)
            continue;
        seg = (int)(sc_rnd() % (uint32_t)fm_frog_count());
        fm_frog(i, seg, f->vel * ht.kv[HK_FROGS] / 50, f->pan, 93 + (int)(sc_rnd() % 15u));   /* (Frogs 50: as voiced) */
        f->next += (uint32_t)fm_frog_ms(seg) + 500u + sc_rnd() % 600u;
        if (--f->keros <= 0)
            f->on = 0;
    }
    if (ht.kv[HK_FROGS] > 0 && !ht_fr[2].on)          /* the far chorus: now one, now another, with room between
                                                       * (3..9 s: never a constant rattle) */
        ht_frog_call(2, 3000u + sc_rnd() % 6000u, 1);
}

/* the owl (fukurō) far off: a low, soft "hoo ... ho-hoo" */
static void ht_owl_call(void)                   /* (a tawny owl's own hoots, CC0; the synthesized one sat at
                                                 * ~300 Hz, the speaker's floor, and was never heard) */
{
    int pan = sc_frand() < 0.5f ? 22 : 104, vel = (37 + (int)(sc_rnd() % 9u)) * (20 + ht.kv[HK_NOTES]) / 70;   /* (about half as loud) */
    fm_shot(3, FM_SHOT_OWL, (int)(sc_rnd() % (uint32_t)fm_shot_count(FM_SHOT_OWL)), vel, pan, 95 + (int)(sc_rnd() % 9u));
    ht.owls++;
}

/* --------------------------------------------------------- fireflies --- */
static fly_t *ht_fly_new(float x, float y)
{
    int i;
    for (i = 0; i < HT_MAXF; i++)
        if (!ht_f[i].on) {
            fly_t *f = &ht_f[i];
            memset(f, 0, sizeof *f);
            f->on = 1;
            f->x = x;
            f->y = y;
            f->h = sc_frand();
            f->sp = 3.5f + 3.0f * sc_frand();
            f->ph = sc_frand();
            f->w = 1.0f / (3.6f + 2.0f * sc_frand());   /* a flash every 3.6..5.6 s by itself (Genji-botaru, eastern) */
            f->heike = sc_frand() < 0.22f;            /* (now and then a Heike-botaru: its own, quicker) */
            if (f->heike)
                f->w *= 1.7f;
            f->perch = -1;
            f->ty = HT_YTOP + (HT_YBOT - 20.0f - HT_YTOP) * sc_frand();
            f->ty_t = 8.0f;
            f->key = HT_KLO + (int)(sc_rnd() % (uint32_t)HT_KN);
            return f;
        }
    return 0;
}

static int ht_alive(void)
{
    int i, n = 0;
    for (i = 0; i < HT_MAXF; i++)
        if (ht_f[i].on && !ht_f[i].settling && !ht_f[i].blow)
            n++;
    return n;
}

/* how many are out through the night, as Genji fireflies are: the most in its first hours (to 0.12), fewer to
 * midnight (0.65), fewer still toward dawn (0.5) */
static float ht_out(void)
{
    float n = ht.night;
    return n <= 0.12f ? 1.0f : n <= 0.5f ? 1.0f - 0.35f * (n - 0.12f) / 0.38f : 0.65f - 0.15f * (n - 0.5f) / 0.5f;
}

static int ht_want(void)                        /* how many, now: Activity, fewer in the deep of the night, in a mist,
                                                 * in rain */
{
    float a = (float)ht.kv[HK_ACT] * 0.01f, n = (6.0f + 5.0f * a) * ht_out();   /* 8 .. 5 .. 4 near (and the far
                                                       * ones, ht_want_far) */
    n *= (1.0f - 0.4f * ht.mist - 0.6f * ht.rain) * ht.emerge;
    return (int)(n + 0.5f);
}

static int ht_want_far(void)                    /* the far fireflies seen now: more with Activity, fewer in mist and rain,
                                                 * fewer in the deep of the night */
{
    float a = (float)ht.kv[HK_ACT] * 0.01f, n = (6.0f + 10.0f * a) * ht_out() * (1.0f - 0.5f * ht.mist - 0.8f * ht.rain);
    n *= ht.emerge;                                   /* (9 at a Natural dusk, 6 deep in the night) */
    return (int)(n + 0.5f);
}

/* the weather (after Avonlea's): Auto picks one each night (clear about half, mist a third, light rain the
 * rest) and now and then another in the night; or the knob holds one. It eases in and out over ~30 s */
static int ht_weather_pick(void)
{
    float u = sc_frand();
    return u < 0.5f ? 0 : u < 0.83f ? 1 : 2;
}
static void ht_weather_set(int now)             /* the knob's (now: at once, as a night starts) */
{
    int w = ht.kv[HK_WEATHER];
    ht.wx_fast = !now;                                /* (by hand: in ~5 s) */
    ht.wx = w ? w - 1 : ht_weather_pick();
    ht.wx_next = 180.0f + 240.0f * sc_frand();
    if (now) {
        ht.mist = ht.wx == 1 ? 1.0f : 0.0f;
        ht.rain = ht.wx == 2 ? 1.0f : 0.0f;
    }
    ht.weathers++;
}
static void ht_depth_send(void)                 /* Depth, a little more in a mist or rain, and the arc's */
{
    int d = ht.kv[HK_DEPTH] + (int)(25.0f * ht.mist + 15.0f * ht.rain);
    ht.depth_sent = d > 100 ? 100 : d;
    cd_depth_send(ht.depth_sent);
}

/* the clearing breeze (the top black key): it rises over ~2.5 s from the left and dies away over ~8 s; it
 * carries every firefly off to the right, and new ones rise from the grass after it, one by one */
static void ht_scatter(float dir)
{
    int i;
    ht.gust_to = 1.0f;
    ht.gust_dir = dir;
    ht.spawn_acc = -6.0f;                             /* (the field empty a few seconds after) */
    for (i = 0; i < HT_MAXF; i++)
        if (ht_f[i].on) {
            ht_f[i].blow = 1;
            ht_f[i].ph += sc_frand();                 /* out of step */
            ht_f[i].ph -= fm_floorf(ht_f[i].ph);
            ht_f[i].perch = -1;
            ht_f[i].perch_t = 0.0f;
        }
    ht.wave_ph = 0.0f;                                /* the coupling starts again from nothing */
    ht.scatters++;
}

/* --------------------------------------------------------------- picture --- */
static float ht_moon_y(void);
/* Natural goes round from dawn straight to dusk: a moon still up at dawn pales into the light over its last minute,
 * one already up at dusk comes out of it over the first; so it is never cut off, nor its phase changed, in sight */
static float ht_moon_tw(void) { return fm_clampf(fm_minf(ht.night, 1.0f - ht.night) * 30.0f, 0.0f, 1.0f); }
static void ht_palette(void)
{
    float d = ht.depth, ml = ht.mn.light;
    int i;
    {   /* the sky: dusk blue with a last warmth low down, to deep night */
        uint32_t top = ht_mix(ht_mix(0x23305Eu, 0x06080Fu, d), 0x020306u, 0.6f * ht.deep),   /* (Deep Night: darkest) */
                 mid = ht_mix(ht_mix(0x4A5C92u, 0x0B1226u, d), 0x060A16u, 0.5f * ht.deep),
                 low = ht_mix(0xC8968Au, 0x1C2840u, d);   /* (dusk: the last warm light low down; night: the
                                                       * horizon a little lighter than the zenith) */
        r8_pal_ramp(HT_SKY, 32, top, mid);            /* (64 steps, dithered between: 32 showed as bands) */
        r8_pal_ramp(HT_SKY + 32, 32, mid, low);
    }
    {   /* stars: out with the night, each at its own steady brightness (they flickered); most
         * faint, a few bright, those few faintly tinted */
        uint32_t sky = ht_mix(0x4A5C92u, 0x0B1226u, d);
        float n = (0.45f + 0.55f * d) * fm_maxf(0.08f, 1.0f - 0.55f * ht.mist - 0.8f * ht.rain);   /* (dimmer in a mist, mostly hidden
                                                       * in rain) */
        static const float B[6] = {0.44f, 0.52f, 0.61f, 0.71f, 0.85f, 1.0f};   /* (the faintest kept visible) */
        for (i = 0; i < 6; i++)
            r8_pal_set(HT_STW + i, ht_mix(sky, 0xF0F2FFu, B[i] * n));
        r8_pal_set(HT_STB, ht_mix(sky, 0xD2E2FFu, n));
        r8_pal_set(HT_STB + 1, ht_mix(sky, 0xD2E2FFu, 0.26f * n));
        r8_pal_set(HT_STY, ht_mix(sky, 0xFFE4B8u, n));
        r8_pal_set(HT_STY + 1, ht_mix(sky, 0xFFE4B8u, 0.26f * n));
        r8_pal_set(HT_STAR, ht_mix(sky, 0xF0F2FFu, 0.26f * n));   /* (a white star's halo) */
    }
    for (i = 0; i < 4; i++)                           /* the moon's halo: the sky, lighter (with how much is lit); a
                                                       * humid early-summer night: soft and wide */
        r8_pal_set(HT_HALO + i, ht_mix(ht_mix(0x4A5C92u, 0x0B1226u, d), 0xA8B4D0u, (0.03f + 0.045f * (float)(3 - i)) * (0.45f + 0.5f * ml)));
    r8_pal_set(HT_MOON, 0xFBF6E2u);                    /* the moon: lit, toward the rim, a sea, the earthshine */
    r8_pal_set(HT_MOON + 1, 0xDDD7C0u);
    r8_pal_set(HT_MOON + 2, 0xD6D0BAu);              /* (the seas: a soft gray, close to the highlands) */
    r8_pal_set(HT_MOON + 3, ht_mix(ht_mix(0x4A5C92u, 0x0B1226u, d), 0x8A96B4u, 0.22f));
    if (ht_moon_tw() < 1.0f) {                        /* (paling into the twilight: the sky where it is) */
        int si = (int)(ht_moon_y() * (63.0f / (HT_HORIZON + 10.0f)));
        uint32_t sk = r8_base[HT_SKY + (si < 0 ? 0 : si > 63 ? 63 : si)];
        for (i = 0; i < 4; i++) {
            r8_base[HT_MOON + i] = ht_mix(sk, r8_base[HT_MOON + i], ht_moon_tw());
            r8_base[HT_HALO + i] = ht_mix(sk, r8_base[HT_HALO + i], ht_moon_tw());
        }
    }
    /* far to near, lighter to darker: the hills, the field with a haze at the horizon, the stream, the foreground;
     * a little moonlight on them all with a fuller moon */
    r8_pal_ramp(HT_HILL, 4, ht_mix(0x2A3A52u, 0x0E1626u, d), ht_mix(0x1C2A3Au, 0x0A1018u, d));
    r8_pal_ramp(HT_GROUND, 8, ht_mix(ht_mix(0x34485Au, 0x121C26u, d), 0x5A6A80u, 0.08f * ml),
                ht_mix(0x14201Au, 0x070C09u, d));
    r8_pal_ramp(HT_WATER, 8, ht_mix(0x2C3C66u, 0x0E1630u, d), ht_mix(0x101828u, 0x06080Fu, d));
    r8_pal_ramp(HT_FORE, 4, ht_mix(0x0E1610u, 0x060806u, d), 0x020302u);
    r8_pal_ramp(HT_REED, 8, ht_mix(0x2A3A44u, 0x0E1620u, d), ht_mix(0x080E0Au, 0x030504u, d));   /* far .. near */
    {   /* the woods (darker than the hills), the branch over us; the stones; moonlight on the ridge; the farmhouse */
        float mv = ml * ht.mn_vis;
        r8_pal_ramp(HT_TREE, 4, ht_mix(0x1A2638u, 0x0A101Au, d), ht_mix(0x080C10u, 0x030406u, d));
        r8_pal_set(HT_STONE, ht_mix(ht_mix(0x3A4660u, 0x161C2Au, d), 0x9AA4BCu, 0.35f * mv));
        r8_pal_set(HT_STONE + 1, ht_mix(0x2A3448u, 0x10141Eu, d));
        r8_pal_set(HT_STONE + 2, ht_mix(0x1C2434u, 0x0A0E16u, d));
        r8_pal_set(HT_STONE + 3, ht_mix(0x141A26u, 0x06080Cu, d));
        r8_pal_set(HT_RIM, ht_mix(r8_base[HT_TREE + 1], 0x8A98B8u, 0.06f + 0.22f * mv));
        r8_pal_set(HT_RIM + 1, ht_mix(r8_base[HT_HILL + 1], 0x8A98B8u, 0.1f + 0.3f * mv));
        r8_pal_set(HT_HOUSE, ht_mix(r8_base[HT_TREE + 1], 0xFFC870u, fm_clampf(d * 2.5f - 0.2f, 0.0f, 1.0f)));
        r8_pal_set(HT_HOUSE + 1, ht_mix(r8_base[HT_TREE + 1], 0x5A4028u, fm_clampf(d * 2.5f - 0.2f, 0.0f, 1.0f)));
    }
    r8_pal_ramp(HT_GLOW, 7, 0x24361Au, 0xD8F070u);   /* a firefly: dim .. bright yellow-green */
    r8_pal_set(HT_GLOW + 7, 0xFCFFD8u);               /* the heart of a blink */
    r8_pal_ramp(HT_SHINE, 4, ht_mix(0x2C3C66u, 0xF4EED0u, 0.12f + 0.33f * ml), ht_mix(0x2C3C66u, 0xC8C4A8u, 0.06f + 0.16f * ml));
    r8_pal_ramp(HT_SHOOT, 4, 0xFFFFF4u, ht_mix(0x4A5C92u, 0x0B1226u, d));
    if (ht.mist > 0.01f || ht.rain > 0.01f) {   /* a mist over the field, the hills and the low sky; rain grays it
                                                 * all a little (the palette only, nothing more to draw) */
        uint32_t mc = ht_mix(ht_mix(0x8A90A0u, 0x2C3242u, d), 0xA0A6B4u, 0.15f * ml);
        float m = ht.mist, r = ht.rain * 0.3f;
        for (i = 0; i < 64; i++)                     /* (the sky: more toward the horizon) */
            r8_base[HT_SKY + i] = ht_mix(r8_base[HT_SKY + i], mc, m * 0.5f * (float)i / 63.0f + r);
        for (i = 0; i < 4; i++) {
            r8_base[HT_HILL + i] = ht_mix(r8_base[HT_HILL + i], mc, m * 0.65f + r);
            r8_base[HT_TREE + i] = ht_mix(r8_base[HT_TREE + i], mc, (i < 2 ? m * 0.5f : 0.0f) + r);
        }
        for (i = 0; i < 2; i++)
            r8_base[HT_RIM + i] = ht_mix(r8_base[HT_RIM + i], mc, m * 0.55f + r);
        for (i = 0; i < 8; i++) {
            r8_base[HT_GROUND + i] = ht_mix(r8_base[HT_GROUND + i], mc, m * (0.55f - 0.05f * (float)i) + r);
            r8_base[HT_WATER + i] = ht_mix(r8_base[HT_WATER + i], mc, m * 0.25f + r * 0.5f);
        }
        for (i = 0; i < 4; i++)
            r8_base[HT_HALO + i] = ht_mix(r8_base[HT_HALO + i], mc, m * 0.3f);
        for (i = 0; i < 2; i++)                      /* (the window, unlit: as the misty woods round it) */
            r8_base[HT_HOUSE + i] = ht_mix(r8_base[HT_HOUSE + i], mc, (m * 0.6f + r) * (1.0f - fm_clampf(d * 2.5f - 0.2f, 0.0f, 1.0f)));
    }
    {   /* the fog: the mist's color, a little moonlit; and in rain the moon behind cloud */
        uint32_t mc = ht_mix(ht_mix(0x8A90A0u, 0x262C3Au, d), 0x9AA0B0u, 0.12f * ml);
        static const uint8_t BK[3] = {HT_GROUND + 1, HT_WATER, HT_WATER + 4};   /* (what each bank lies on) */
        for (i = 0; i < 12; i++)                     /* the fog's shades: each bank's own, over what it lies on,
                                                       * thin .. thick (mixed only with the next: no pattern) */
            r8_pal_set(HT_FOGL + i, ht_mix(r8_base[BK[i >> 2]], mc, 0.17f * (float)((i & 3) + 1)));
        for (i = 0; i < 3; i++)                      /* a firefly's light spread in the fog, faint .. strong */
            r8_pal_set(HT_FOGG + i, ht_mix(r8_base[HT_FOGL + 1], 0xB8D860u, 0.12f + 0.13f * (float)i));
        for (i = 0; i < 2; i++)                      /* the farmhouse window's light in the fog, faint .. warm */
            r8_pal_set(HT_FOGW + i, ht_mix(r8_base[HT_FOGL + 2], 0xFFB860u, (0.1f + 0.12f * (float)i) * fm_clampf(d * 2.5f - 0.2f, 0.0f, 1.0f)));
        {   /* the Milky Way: the sky high up, a little lighter, cool */
            uint32_t sk = ht_mix(ht_mix(0x23305Eu, 0x06080Fu, d), 0x020306u, 0.6f * ht.deep);
            r8_pal_set(HT_MW, ht_mix(sk, 0x6A7290u, 0.16f));
            r8_pal_set(HT_MW + 1, ht_mix(sk, 0x6A7290u, 0.30f));
        }
        if (ht.rain > 0.01f) {
            uint32_t sk = r8_base[HT_SKY + 20];
            for (i = 0; i < 4; i++) {
                r8_base[HT_MOON + i] = ht_mix(r8_base[HT_MOON + i], sk, 0.8f * ht.rain);
                r8_base[HT_HALO + i] = ht_mix(r8_base[HT_HALO + i], sk, ht.rain);
            }
            for (i = 0; i < 64; i++)                  /* (the low cloud: the sky grayer, flatter) */
                r8_base[HT_SKY + i] = ht_mix(r8_base[HT_SKY + i], ht_mix(0x5A6070u, 0x1C2028u, d), 0.45f * ht.rain);
        }
    }
    {   /* the clouds, each its own shades from the sky at its height (so it lies in the sky, no pattern): six
         * steps from barely there to its thickest, then the same lit by the moon, half and full; and the moon behind
         * a cloud, each of its shades veiled in four steps */
        uint32_t gray = ht_mix(0x8A90A4u, 0x3C4458u, d), silver = ht_mix(0xC4C8D6u, 0x9AA0B4u, d);
        float mv = ht.mn_vis * (0.5f + 0.5f * ml);
        int c, j;
        for (c = 0; c < HT_NCLOUD; c++) {
            int si = (int)(ht_c[c].y * (63.0f / (HT_HORIZON + 10.0f)));
            uint32_t sk = r8_base[HT_SKY + (si < 0 ? 0 : si > 63 ? 63 : si)];
            float th = c == 0 ? 0.45f : 0.7f;         /* (the far one fainter) */
            for (j = 0; j < 6; j++) {
                float a = th * (float)(j + 1) * (1.0f / 6.0f);
                uint32_t un = ht_mix(sk, gray, a * 0.7f), li = ht_mix(sk, silver, a * (0.55f + 0.35f * mv));
                r8_pal_set(HT_CLR + c * HT_CLR_N + j, un);
                r8_pal_set(HT_CLR + c * HT_CLR_N + 6 + j, ht_mix(un, li, 0.5f * mv));
                r8_pal_set(HT_CLR + c * HT_CLR_N + 12 + j, ht_mix(un, li, mv));
            }
        }
        for (i = 0; i < 3; i++)
            for (j = 0; j < 4; j++)
                r8_pal_set(HT_MVEIL + i * 4 + j, ht_mix(r8_base[HT_MOON + i], r8_base[HT_CLR + HT_CLR_N + 3],
                                                       0.16f * (float)(j + 1)));
    }
    r8_pal_set(HT_RAIN, ht_mix(0x9AA6BEu, 0x56627Au, d));   /* rain: light, dim */
    r8_pal_set(HT_RAIN + 1, ht_mix(0x7884A0u, 0x404A60u, d));
    r8_pal_commit(0xFFFFFFu, 0, 0);
}


static void ht_mood_moon(int m);
SC_SMALL static void ht_background(void)
{
    surf_t *s = &r8_bg;
    float k = ht_scale();
    int32_t x, y, W = s->w, H = s->h, hz = (int32_t)(HT_HORIZON * k);
    int i;
    sc_seed(ht.seed);
    {   /* the moon: its size and phase (full and gibbous most nights, a crescent now and then); a new picture's
         * only: drawn again (the style turned) the moon stays as it is tonight */
        sc_moon_t m;
        float top;
        sc_moon_pick(&m, 1.0f);
        top = 120.0f + 30.0f * sc_frand();             /* (its arc a little higher or lower each night) */
        sc_mst_k = -1.0f;                             /* (its face to be made again) */
        if (!ht.mn_made) {
            ht.mn_made = 1;
            ht.mn = m;
            ht.moon_top = top;
            ht.nat_light = m.light;
            ht.nat_off = m.off;
            ht.nat_side = m.side;
            ht_mood_moon(ht.kv ? sc_mood_base(ht.kv[HK_MOOD], HT_NMOOD) : 0);
            if (ht.kv)                                /* (a crescent: longer echoes) */
                cd_echo_amount((float)ht.kv[HK_ECHO] * 0.01f + 0.3f * (1.0f - ht.mn.light));
        }
    }
    ht.wt_ph = sc_frand();
    ht.wb_ph = sc_frand();
    r8_vgrad_wide(s, 0, hz + (int32_t)(10.0f * k), HT_SKY, 64, 10);   /* (wide dither: no bands) */
    {   /* the stars: scattered, with small clumps and gaps (a third of them near
         * four loose centers), thinning toward the horizon; most faint, about ten medium, three or four bright, those
         * faintly blue-white or warm; each comes out at its own depth of the night, the brightest first. Drawn each
         * frame (ht_stars_draw): they fade near the moon, and low in the haze */
        float cxs[4], cys[4];
        for (i = 0; i < 4; i++) {
            cxs[i] = 20.0f + 200.0f * sc_frand();
            cys[i] = 10.0f + 90.0f * sc_frand();
        }
        for (i = 0; i < HT_NSTAR; i++) {
            float sx, sy, u;
            int tries;
            for (tries = 0; tries < 8; tries++) {
                if (i % 3 == 0) {
                    int c = (int)(sc_rnd() % 4u);
                    sx = cxs[c] + (sc_frand() + sc_frand() - 1.0f) * 22.0f;
                    sy = cys[c] + (sc_frand() + sc_frand() - 1.0f) * 16.0f;
                } else {
                    sx = 240.0f * sc_frand();
                    sy = 3.0f + (HT_HORIZON - 14.0f) * sc_frand();
                }
                if (sx >= 1.0f && sx < 239.0f && sy >= 3.0f && sy < HT_HORIZON - 10.0f &&
                    sc_frand() < 1.2f - sy / (HT_HORIZON - 10.0f))
                    break;                            /* (fewer low down) */
            }
            u = sc_frand();
            ht_st[i].x = (int16_t)fm_clampf(sx, 1.0f, 238.0f);
            ht_st[i].y = (int16_t)fm_clampf(sy, 3.0f, HT_HORIZON - 11.0f);
            ht_st[i].lv = (uint8_t)(i < 4 ? 4 + (i < 2) : i < 14 ? 2 + (u < 0.5f) : u < 0.55f ? 0 : 1);
            ht_st[i].tint = (uint8_t)(i == 1 ? 1 : i == 2 ? 2 : i == 3 && u < 0.5f ? 1 : 0);
            ht_st[i].appear = (uint8_t)(ht_st[i].lv >= 4 ? 10.0f * sc_frand() : (float)(5 - ht_st[i].lv) * 22.0f + 90.0f * sc_frand());
        }
    }
    {   /* far hills; woods along the far side (cedars and broadleaf crowns, darker than the hills); the field with a
         * haze at the horizon; the stream; the near bank, darkest at the bottom */
        float p1 = sc_frand(), p2 = sc_frand();
        static float woods[240];                      /* (the woods' top, by world column) */
        for (x = 0; x < 240; x++)
            woods[x] = 999.0f;
        /* the cedars (sugi) stand together, a stand or two a picture, as a plantation does; the rest broadleaf crowns,
         * a stray cedar now and then. A cedar far off is a soft cone: wide, not tall, its tip rounded */
        int nst = 1 + (sc_frand() < 0.5f);
        float stx[2], stw[2];
        for (i = 0; i < nst; i++) {
            stx[i] = 20.0f + 200.0f * sc_frand();
            stw[i] = 14.0f + 12.0f * sc_frand();
        }
        for (i = 0; i < 40; i++) {
            int st = i / 9, cedar = st < nst || sc_frand() < 0.04f, xx;
            float cx = st < nst ? stx[st] + (2.0f * sc_frand() - 1.0f) * stw[st] : 240.0f * sc_frand(),
                  base = HT_HORIZON + 1.0f,
                  h = cedar ? 7.0f + 9.0f * sc_frand() : 7.0f + 16.0f * sc_frand() * sc_frand(),
                  w = cedar ? 7.0f + 4.0f * sc_frand() : 5.0f + 7.0f * sc_frand();
            for (xx = (int)(cx - w); xx <= (int)(cx + w); xx++) {
                float u = ((float)xx + 0.5f - cx) / w, top;
                if (xx < 0 || xx >= 240 || u * u >= 1.0f)
                    continue;
                top = cedar ? base - h * (1.031f - fm_sqrtf(0.0625f + u * u)) / 0.781f - 1.0f   /* (a cone, its tip
                                                       * rounded: sqrt(0.25^2 + u^2) near the top, straight below) */
                            : base - h * fm_sqrtf(1.0f - u * u) * 0.8f;
                if (top < woods[xx])
                    woods[xx] = top;
            }
        }
        for (x = 0; x < W; x++) {
            float wx = ((float)x + 0.5f) / k, hill = HT_HORIZON - 10.0f - 9.0f * fm_sin_turns(wx / 240.0f * 0.9f + p1) -
                                                     4.0f * fm_sin_turns(wx / 240.0f * 2.3f + p2);
            float tree = hill - 4.0f - 0.9f * fm_sin_turns(wx / 90.0f + p2) - 0.4f * fm_sin_turns(wx / 37.0f + p1),
                  wd = woods[(int)wx < 240 ? (int)wx : 239];
            float wt = ht_wtop(wx), wb = ht_wbot(wx);
            for (y = (int32_t)(fm_minf(tree, wd) * k); y < H; y++) {
                float wy = ((float)y + 0.5f) / k;
                uint8_t c;
                if (y == (int32_t)(fm_minf(tree, wd) * k) && wd > tree && tree * k - (float)y > 0.5f)
                    continue;                         /* (its top pixel only when more than half covered: a clean edge;
                                                       * a dithered one left single pixels along a flat ridge) */
                if (wy >= wd && wy < HT_HORIZON + 2.0f)   /* the woods, a rim of moonlight on their tops */
                    c = (uint8_t)(wy < wd + 1.0f && r8_dith(x, y, 9) ? HT_RIM : HT_TREE + 1);   /* (a soft, broken
                                                       * rim: never a drawn outline) */
                else if (wy < hill)
                    c = HT_HILL + 1;
                else if (wy < HT_HORIZON + 4.0f)
                    c = (uint8_t)(HT_HILL + 2 + r8_dith(x, y, (int)((wy - hill) * 1.2f)));
                else if (wy < wt) {                   /* the far field: haze at the top, darker toward the stream */
                    int f = (int)((wy - HT_HORIZON - 4.0f) / (wt - HT_HORIZON - 4.0f + 1e-3f) * 7.0f * 16.0f);
                    c = (uint8_t)(HT_GROUND + (f >> 4) + r8_dith(x, y, f & 15));
                    if (c > HT_GROUND + 7)
                        c = HT_GROUND + 7;
                } else if (wy < wb) {                 /* the stream: the sky's dark blue, lighter at its far edge */
                    int f = (int)((wy - wt) / (wb - wt + 1e-3f) * 7.0f * 16.0f);
                    c = (uint8_t)(HT_WATER + (f >> 4) + r8_dith(x, y, f & 15));
                    if (c > HT_WATER + 7)
                        c = HT_WATER + 7;
                } else {                              /* the near bank: near black, darker yet at the bottom */
                    int f = (int)((wy - wb) / (240.0f - wb) * 3.0f * 16.0f);
                    c = (uint8_t)(HT_FORE + (f >> 4) + r8_dith(x, y, f & 15));
                    if (c > HT_FORE + 3)
                        c = HT_FORE + 3;
                }
                if (y >= 0 && y < H)
                    s->px[y * W + x] = c;
            }
        }
    }
    {   /* a farmhouse window in the woods, far off: lit as the night comes (its glow round it) */
        float hx = ht.hx = 30.0f + 180.0f * sc_frand(), hy = HT_HORIZON - 2.5f;
        r8_disc(s, hx * k, hy * k, 2.6f * k, HT_HOUSE + 1);
        r8_pset(s, (int32_t)(hx * k), (int32_t)(hy * k), HT_HOUSE);
        r8_pset(s, (int32_t)(hx * k) + 1, (int32_t)(hy * k), HT_HOUSE);
    }
    for (i = 0; i < HT_NSN; i++) {                    /* stones in the stream: low humps, moonlit on top */
        float sx = 10.0f + 220.0f * sc_frand(), wt = ht_wtop(sx), wb = ht_wbot(sx), u = sc_frand(),
              sy = wt + 3.0f + (wb - wt - 6.0f) * u, rx = (2.0f + 5.0f * u) * (0.7f + 0.6f * sc_frand());
        ht_sn[i].x = sx;
        ht_sn[i].y = sy;
        ht_sn[i].rx = rx;
        r8_quad(s, sx * k, sy * k, rx * k, rx * 0.45f * k, 0.0f, HT_STONE + 2);
        r8_quad(s, (sx - rx * 0.2f) * k, (sy - rx * 0.2f) * k, rx * 0.6f * k, rx * 0.18f * k, 0.0f, HT_STONE);
    }
    for (i = 0; i < 150; i++) {                       /* grass: a dense low band along the far bank; leafy weeds and
                                                       * sedge on the near bank; tall blades over us, from the bottom,
                                                       * dark against the water and the sky */
        int band = i < 70 ? 0 : i < 115 ? 1 : 2;
        float gx = 240.0f * sc_frand(), gy, gh, lean = (sc_frand() - 0.5f) * 0.5f, bw;
        uint8_t c;
        if (band == 0) {
            gy = ht_wtop(gx) + 1.0f;
            gh = 2.0f + 5.0f * sc_frand();
            c = (uint8_t)(HT_REED + 2 + (sc_rnd() & 1u));
            bw = 0.9f;
        } else if (band == 1) {
            gy = ht_wbot(gx) + 2.0f + 20.0f * sc_frand();
            gh = 5.0f + 16.0f * sc_frand();
            c = (uint8_t)(HT_REED + 5 + (sc_rnd() & 1u));
            bw = 1.3f;
        } else {
            gy = 242.0f;
            gh = 30.0f + 60.0f * sc_frand();
            c = HT_REED + 7;
            bw = 2.0f;
        }
        r8_limb(s, gx * k, gy * k, bw * k, (gx + lean * gh * 0.5f) * k, (gy - gh * 0.55f) * k, bw * 0.7f * k, c);
        r8_limb(s, (gx + lean * gh * 0.5f) * k, (gy - gh * 0.55f) * k, bw * 0.7f * k, (gx + lean * gh * 1.3f) * k,
                (gy - gh) * k, fm_maxf(0.3f * k, 0.5f), c);
    }
    for (i = 0; i < HT_NREED; i++) {                  /* reeds along our bank (drawn swaying, each frame); none on the
                                                       * far one: at that distance a reed is a pixel or two, not a pole */
        float u = sc_frand(), side = u < 0.5f ? u * 2.0f : (u - 0.5f) * 2.0f;
        ht_r[i].bx = u < 0.5f ? 4.0f + 90.0f * side * side : 236.0f - 90.0f * side * side;
        if (sc_frand() < 0.2f)
            ht_r[i].bx = 60.0f + 120.0f * sc_frand();
        ht_r[i].by = ht_wbot(ht_r[i].bx) + 1.0f + 28.0f * sc_frand() * sc_frand();
        ht_r[i].layer = (uint8_t)(3 + (int)((ht_r[i].by - HT_WBOT) / 50.0f * 3.99f));
        ht_r[i].h = 35.0f + 40.0f * sc_frand();
        ht_r[i].lean = (sc_frand() - 0.5f) * 0.25f;
        ht_r[i].ph = sc_frand();
        ht_r[i].cattail = (sc_rnd() % 7u) == 0u;     /* (now and then a cattail; mostly reeds and grass) */
        ht_r[i].top_x = ht_r[i].bx + ht_r[i].lean * ht_r[i].h;
        ht_r[i].top_y = ht_r[i].by - ht_r[i].h;
    }
    {   /* far to near: the nearest drawn last */
        int a, b;
        for (a = 1; a < HT_NREED; a++)
            for (b = a; b > 0 && ht_r[b - 1].by > ht_r[b].by; b--) {
                float t;
                uint8_t c;
#define HT_SWAP(f) (t = ht_r[b].f, ht_r[b].f = ht_r[b - 1].f, ht_r[b - 1].f = t)
                HT_SWAP(bx); HT_SWAP(by); HT_SWAP(h); HT_SWAP(lean); HT_SWAP(ph); HT_SWAP(top_x); HT_SWAP(top_y);
#undef HT_SWAP
                c = ht_r[b].layer; ht_r[b].layer = ht_r[b - 1].layer; ht_r[b - 1].layer = c;
                c = ht_r[b].cattail; ht_r[b].cattail = ht_r[b - 1].cattail; ht_r[b - 1].cattail = c;
            }
    }
    sc_seed(ht.seed * 2654435761u + 5u);              /* the world's randomness, apart from the picture's */
}

static float ht_moon_x(void);
static float ht_moon_y(void);
static void ht_mood_moon(int m);

/* a night cloud: a wisp, a few long thin streaks one after another, nearly
 * see-through; the first one far off (low, small, slow, faint), the others higher, longer and quicker */
static void ht_cloud_new(int c, float x)
{
    int i, far = c == 0;
    float sc = far ? 0.55f : 0.9f + 0.5f * sc_frand();
    ht_c[c].x = x;
    ht_c[c].y = far ? 100.0f + 15.0f * sc_frand() : 14.0f + 66.0f * sc_frand();
    ht_c[c].v = far ? 0.6f + 0.5f * sc_frand() : 1.6f + 2.4f * sc_frand();
    ht_c[c].np = 3 + (int)(sc_rnd() % 3u);
    ht_c[c].wait = 0.0f;
    for (i = 0; i < ht_c[c].np; i++) {
        ht_c[c].px[i] = (float)i * (18.0f + 10.0f * sc_frand()) * sc;       /* along it */
        ht_c[c].py[i] = (sc_frand() - 0.5f) * 6.0f * sc;
        ht_c[c].pr[i] = (16.0f + 16.0f * sc_frand()) * sc;                 /* a streak's half length */
    }
    if (x < 0.0f) {                                   /* (clouds "pop into existence"): a new one
                                                       * starts wholly beyond the left edge and drifts in */
        float ext = 0.0f;
        for (i = 0; i < ht_c[c].np; i++)
            ext = fm_maxf(ext, ht_c[c].px[i] + ht_c[c].pr[i]);
        ht_c[c].x = -ext - 4.0f;
    }
}
static int ht_cloud_gone(int c)                 /* wholly off the right edge (not only its first streak) */
{
    int i;
    for (i = 0; i < ht_c[c].np; i++)
        if (ht_c[c].x + ht_c[c].px[i] - ht_c[c].pr[i] < 244.0f)
            return 0;
    return 1;
}

/* a cloud over the sky: thin streaks, each a soft band whose middle line and thickness wander along it and whose
 * edges break up, densest along its middle. Each pixel takes one of the cloud's own shades by how thick the cloud is
 * there (mixed only with the next shade: no pattern); near the moon the lit shades (its silver edge), half lit a
 * little further off; across the moon, the moon veiled in steps by how thick it is */
static void ht_cloud_draw(surf_t *s, float k, int c)
{
    int i;
    float mx = ht_moon_x() * k, my = ht_moon_y() * k, mr = ht.mn.r * k * fm_sqrtf(ht.mn_vis),
          ph = (float)c * 0.37f;
    uint8_t c0 = (uint8_t)(HT_CLR + c * HT_CLR_N);
    for (i = 0; i < ht_c[c].np; i++) {
        float cx = (ht_c[c].x + ht_c[c].px[i]) * k, cy = (ht_c[c].y + ht_c[c].py[i]) * k, rx = ht_c[c].pr[i] * k,
              ry = ((c == 0 ? 3.5f : 6.0f) + ht_c[c].pr[i] * 0.12f) * k, pp = ph + (float)i * 0.29f;
        int32_t x, y, x0 = (int32_t)(cx - rx - 1.0f), x1 = (int32_t)(cx + rx + 1.0f);
        for (x = x0 < 0 ? 0 : x0; x <= x1 && x < s->w; x++) {
            float xw = ((float)x + 0.5f) / k - ht_c[c].x,  /* (its own place along it: the shape drifts with it) */
                  dx = ((float)x + 0.5f - cx) / rx, ax = 1.0f - dx * dx,
                  my0 = cy + 1.6f * k * fm_sin_turns(xw * 0.031f + pp),   /* (its middle line wandering) */
                  ryx = ry * (0.65f + 0.35f * fm_sin_turns(xw * 0.053f + pp * 1.7f)),   /* (its thickness) */
                  pa = 0.26f * fm_sin_turns(xw * 0.071f + pp), pb = xw * 0.017f + pp * 2.3f, ex = (float)x + 0.5f - mx,
                  ex2 = ex * ex, far2 = 4.1f * 4.1f * (mr * mr + 1e-3f);
            int32_t ya = (int32_t)(my0 - ryx - 1.0f), yb = (int32_t)(my0 + ryx + 1.0f);
            if (ax <= 0.0f)
                continue;
            for (y = ya < 0 ? 0 : ya; y <= yb && y < s->h; y++) {
                uint8_t *p = s->px + y * s->w + x;
                float dy = ((float)y + 0.5f - my0) / ryx, vy = 1.0f - dy * dy, a, v, ey, e2;
                int lv, set;
                if (vy <= 0.0f)
                    continue;
                a = ax * vy * vy * (0.74f + pa * fm_sin_turns((float)y / k * 0.21f + pb));   /* (thicker and thinner in
                                                       * soft patches) */
                v = a * 6.0f;
                lv = (int)v;
                lv += r8_dith(x, y, (int)((v - (float)lv) * 16.0f));
                if (lv <= 0)
                    continue;
                if (lv > 6)
                    lv = 6;
                if ((*p >= HT_MOON && *p < HT_MOON + 3) || (*p >= HT_MVEIL && *p < HT_MVEIL + 12)) {   /* the moon
                                                       * behind: veiled in steps */
                    int sh = *p >= HT_MVEIL ? (*p - HT_MVEIL) >> 2 : *p - HT_MOON, old = *p >= HT_MVEIL ? ((*p - HT_MVEIL) & 3) + 1 : 0,
                        vl = (lv * 4 + 3) / 6;
                    if (vl > old)
                        *p = (uint8_t)(HT_MVEIL + sh * 4 + vl - 1);
                    continue;
                }
                if (*p >= HT_CLR && *p < HT_CLR + HT_NCLOUD * HT_CLR_N) {   /* a cloud already: the thicker */
                    int old = (*p - HT_CLR) % 6 + 1;
                    if (lv <= old)
                        continue;
                } else if (*p >= HT_HILL && *p != HT_MW && *p != HT_MW + 1 && !(*p >= HT_STW && *p < HT_STY + 2) &&
                           *p != HT_STAR)
                    continue;                         /* (only over the sky, its stars and the Milky Way) */
                ey = (float)y + 0.5f - my;
                e2 = ex2 + ey * ey;
                set = 0;
                if (e2 < far2) {                      /* lit near the moon: full, then half, mixed */
                    float e = fm_sqrtf(e2) / (mr + 1e-3f);
                    set = e < 1.7f ? 2 : e < 2.9f ? 1 : 0;
                    if (set < 2 && r8_dith(x, y, (int)(((set ? 2.9f : 4.1f) - e) * 13.0f)))
                        set++;
                }
                *p = (uint8_t)(c0 + set * 6 + lv - 1);
            }
        }
    }
}

/* the moon's path by its phase, as in life: the night (0 dusk .. 1
 * dawn) against when this moon rose: a full moon at dusk, highest at midnight; a waxing half already high at dusk,
 * setting about midnight; a waxing crescent low in the west, setting early; a waning moon rising later in the night.
 * t: 0 rising (the left, east) .. 1 setting (the right, west); outside 0..1 it is below the hills (y > 182: hidden) */
/* a mood's moon reached gently: over 9 s, eased, it
 * glides from where it was to the new mood's place, its shadow sweeping across its face to the new phase (through the
 * full moon if it changes side), the face made again only when the shadow has moved */
#define HT_MG_S 13.5f
static struct {
    float t, x0, y0, l0, o0, s0, l1, o1, s1, o_made;
} ht_mg = {99.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static float ht_moon_t_of(float off, float side) { return sc_moon_t_of(ht.night, off, side); }
static float ht_moon_x_of(float off, float side) { return 24.0f + 192.0f * fm_clampf(ht_moon_t_of(off, side), -0.05f, 1.05f); }
/* (the path above: the moon's place for a phase, this night) */
static float ht_moon_y_of(float off, float side)
{
    float t = ht_moon_t_of(off, side);
    if (t < 0.0f || t > 1.0f)
        return 260.0f;                              /* (below the hills) */
    return 182.0f - ht.moon_top * fm_sin_turns(t * 0.5f);
}
static float ht_mg_u(void)
{
    float u = fm_clampf(ht_mg.t / HT_MG_S, 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}
static float ht_moon_t(void) { return ht_moon_t_of(ht_mg.t < HT_MG_S ? ht_mg.o1 : ht.mn.off, ht_mg.t < HT_MG_S ? ht_mg.s1 : ht.mn.side); }
static float ht_moon_x(void)
{
    if (ht_mg.t < HT_MG_S)
        return ht_mg.x0 + (ht_moon_x_of(ht_mg.o1, ht_mg.s1) - ht_mg.x0) * ht_mg_u();
    return ht_moon_x_of(ht.mn.off, ht.mn.side);
}
static float ht_moon_y(void)
{
    if (ht_mg.t < HT_MG_S)
        return ht_mg.y0 + (ht_moon_y_of(ht_mg.o1, ht_mg.s1) - ht_mg.y0) * ht_mg_u();
    return ht_moon_y_of(ht.mn.off, ht.mn.side);
}

/* a mood's moon: Festival a full moon (early in the night: rising over the hills), Deep Night a thin waxing crescent (set by then: none), Natural and Quiet Night the night's own. glide: on
 * its way there (a mood turned), else at once (a night begun) */
static void ht_mood_moon_to(int m, int glide)
{
    float l = ht.mn.light, o = ht.mn.off, sd = ht.mn.side;
    if (m == 2) {                                     /* (Festival) */
        l = 1.0f;
        o = -1.0f;
    } else if (m == 4) {                              /* (Deep Night) */
        l = 0.3f;
        o = 0.6f;
        sd = 1.0f;
    } else {
        l = ht.nat_light;
        o = ht.nat_off;
        sd = ht.nat_side;
    }
    if (!glide) {
        ht.mn.light = l;
        ht.mn.off = o;
        ht.mn.side = sd;
        ht_mg.t = 99.0f;
        sc_mst_k = -1.0f;                             /* (its face made again) */
        return;
    }
    ht_mg.x0 = ht_moon_x();                           /* (from where it is now, even mid-glide) */
    ht_mg.y0 = ht_moon_y();
    ht_mg.l0 = ht.mn.light;
    ht_mg.o0 = ht.mn.off;
    ht_mg.s0 = ht.mn.side;
    ht_mg.l1 = l;
    ht_mg.o1 = o;
    ht_mg.s1 = sd;
    ht_mg.o_made = ht.mn.off;
    ht_mg.t = 0.0f;
}
static void ht_mood_moon(int m) { ht_mood_moon_to(m, 0); }
static void ht_moon_glide(float dt)               /* (each step) */
{
    float u, o;
    if (ht_mg.t >= HT_MG_S)
        return;
    ht_mg.t += dt;
    u = ht_mg_u();
    ht.mn.light = ht_mg.l0 + (ht_mg.l1 - ht_mg.l0) * u;
    if (ht_mg.s0 == ht_mg.s1) {
        o = ht_mg.o0 + (ht_mg.o1 - ht_mg.o0) * u;
        ht.mn.side = ht_mg.s1;
    } else if (u < 0.5f) {                            /* (lit on the other side: through the full moon) */
        o = ht_mg.o0 + (-1.0f - ht_mg.o0) * u * 2.0f;
        ht.mn.side = ht_mg.s0;
    } else {
        o = -1.0f + (ht_mg.o1 + 1.0f) * (u - 0.5f) * 2.0f;
        ht.mn.side = ht_mg.s1;
    }
    if (ht_mg.t >= HT_MG_S) {
        ht.mn.light = ht_mg.l1;
        o = ht_mg.o1;
        ht.mn.side = ht_mg.s1;
        if (ht.kv)
            cd_echo_amount((float)ht.kv[HK_ECHO] * 0.01f + 0.3f * (1.0f - ht.mn.light));
    }
    ht.mn.off = o;
    if (fm_fabsf(o - ht_mg.o_made) > 0.03f || ht_mg.t >= HT_MG_S) {   /* (the shadow moved: the face again) */
        ht_mg.o_made = o;
        sc_mst_k = -1.0f;
    }
}

static void ht_draw_moon(surf_t *s, float k)     /* the moon (scene.h): only over the sky, the hills in front */
{
    ht.mn_vis = sc_moon_draw(s, k, &ht.mn, ht_moon_x() * k, ht_moon_y() * k, HT_MOON, HT_MOON, HT_HALO, 4) * ht_moon_tw();
}

/* the stream: the moon's light on it, a column of short wavering dashes under the moon; the fireflies' glows,
 * mirrored and pressed into its width, wavering */
static void ht_draw_stream(surf_t *s, float k)
{
    float mx = ht_moon_x(), my = ht_moon_y(), wt = ht_wtop(mx);
    int i, n;
    if (my < wt - 2.0f && ht.mn_vis > 0.03f) {     /* the moon is up, and seen (its light
                                                       * showed at dusk with the moon behind the hills): its glade on
                                                       * the water (chose a path of light the moon's
                                                       * own width, widening toward the near bank, its slices
                                                       * shimmering in place), as wide as the moon is seen */
        float wb = ht_wbot(mx), r = ht.mn.r * fm_sqrtf(ht.mn_vis);
        int slot = (int)(ht.t * 2.5f);                /* (the glints change a few times a second) */
        for (n = 0; n < 36; n++) {                    /* a glade of broken glints: short dashes scattered about the
                                                       * moon's line, wider and more scattered toward us */
            uint32_t h = r8_mix32((uint32_t)n * 2654435761u, (uint32_t)(slot + n * 7) / 3u);
            float u = (float)(h & 255u) / 255.0f, y = wt + 1.0f + (wb - wt - 2.0f) * u * u * 0.6f + (wb - wt - 2.0f) * 0.4f * u,
                  spread = r * (0.4f + 1.6f * u), x = mx + ((float)((h >> 8) & 255u) / 127.5f - 1.0f) * spread,
                  w = (0.6f + 2.2f * (float)((h >> 16) & 255u) / 255.0f) * (0.5f + 0.7f * u);
            int lv = (int)((h >> 24) & 3u) + (u > 0.6f ? 1 : 0);
            if (((h >> 26) & 3u) == 0u)
                continue;                             /* (gaps) */
            r8_hspan(s, (int32_t)(y * k), (int32_t)((x - w) * k), (int32_t)((x + w) * k), (uint8_t)(HT_SHINE + (lv > 3 ? 3 : lv)));
        }
    }
    for (i = 0; i < HT_NSN; i++) {                    /* the water riffling past the stones: a glint or two downstream */
        float sx = ht_sn[i].x + ht_sn[i].rx + 1.0f + 2.0f * fm_sin_turns(ht.t * 0.7f + (float)i * 0.4f);
        int32_t x = (int32_t)(sx * k), y = (int32_t)((ht_sn[i].y + 0.5f) * k);
        if (x >= 0 && y >= 0 && x < s->w && y < s->h && s->px[y * s->w + x] >= HT_WATER && s->px[y * s->w + x] < HT_WATER + 8)
            s->px[y * s->w + x] = (uint8_t)(HT_SHINE + 2 + ((int)(ht.t * 3.0f + (float)i) & 1));
    }
    for (i = 0; i < HT_MAXF; i++) {
        fly_t *f = &ht_f[i];
        float g, wt2, wb2, y, x;
        if (!f->on || f->fade < 0.05f)
            continue;
        g = 0.8f * f->b * f->fade;
        if (g < 0.1f)
            continue;                                 /* (mirrored only as it flashes) */
        wt2 = ht_wtop(f->x);
        wb2 = ht_wbot(f->x);
        y = wt2 + 1.5f + (wb2 - wt2 - 3.0f) * fm_clampf((wt2 - f->y) / (wt2 - HT_YTOP), 0.0f, 1.0f);
        x = f->x + 1.5f * fm_sin_turns(ht.t * 0.9f + f->x * 0.05f);
        r8_quad(s, x * k, y * k, (1.5f + 3.0f * g) * k, 0.6f * k, 0.0f, (uint8_t)(HT_GLOW + (int)(g * 3.5f)));
    }
    for (i = 0; i < HT_NFF && i < ht_want_far(); i++)   /* the far ones, just under the far bank */
        if (ht_ff[i].b > 0.5f) {
            float wt2 = ht_wtop(ht_ff[i].x);
            int32_t x = (int32_t)(ht_ff[i].x * k), y = (int32_t)((wt2 + 1.5f + (wt2 - ht_ff[i].y) * 0.3f) * k);
            if (x >= 0 && y >= 0 && x < s->w && y < s->h && s->px[y * s->w + x] >= HT_WATER && s->px[y * s->w + x] < HT_WATER + 8)
                s->px[y * s->w + x] = HT_GLOW + 2;
        }
}

/* Deep Night: a faint Milky Way slanting up across the sky, lumpy, with a dark rift
 * along it, and many more faint stars, thickest in it. The sky's own colors a step or two lighter (dithered), so it
 * stays as soft as the sky; hidden by mist and rain as the stars are */
static void ht_deep_draw(surf_t *s, float k)
{
    float a0 = ht.deep * fm_maxf(0.0f, 1.0f - 0.7f * ht.mist - ht.rain);
    int32_t x, y, i;
    if (a0 < 0.03f)
        return;
    for (x = 0; x < s->w; x++) {
        float wx = ((float)x + 0.5f) / k, cy = 118.0f - 0.42f * wx + 6.0f * fm_sin_turns(wx / 170.0f + 0.2f),
              w = 15.0f + 5.0f * fm_sin_turns(wx / 61.0f + 0.4f), lump = 0.65f + 0.35f * fm_sin_turns(wx / 23.0f) *
                                                                                 fm_sin_turns(wx / 41.0f + 0.3f);
        int32_t ya = (int32_t)((cy - w) * k), yb = (int32_t)((cy + w) * k);
        for (y = ya < 0 ? 0 : ya; y <= yb && y < s->h; y++) {
            float v = (((float)y + 0.5f) / k - cy) / w, a = (1.0f - v * v) * lump * a0, rift = v - 0.15f;
            uint8_t *p = s->px + y * s->w + x;
            if (*p >= HT_SKY + 64 || a <= 0.0f)
                continue;
            a *= 1.0f - 0.6f * fm_clampf(1.0f - rift * rift * 40.0f, 0.0f, 1.0f);   /* (the dark rift) */
            {
                int t = (int)(a * 32.0f), l = (t >> 4) + r8_dith(x, y, t & 15);
                if (l)
                    *p = (uint8_t)(HT_MW + (l > 1));
            }
        }
    }
    for (i = 0; i < 140; i++) {                       /* the faint stars only a dark night shows, most of them in it */
        uint32_t h = r8_mix32((uint32_t)i * 2654435761u, ht.seed ^ 0x5D3Bu);
        float sx = (float)(h % 2400u) * 0.1f, sy;
        int32_t px, py;
        uint8_t *p;
        if (i % 3)                                   /* (two in three along the band) */
            sy = 118.0f - 0.42f * sx + 6.0f * fm_sin_turns(sx / 170.0f + 0.2f) + ((float)((h >> 12) & 255u) - 127.5f) * 0.12f;
        else
            sy = 3.0f + (float)((h >> 12) % 1300u) * 0.1f;
        px = (int32_t)(sx * k);
        py = (int32_t)(sy * k);
        if (px < 0 || py < 0 || px >= s->w || py >= s->h || (float)((h >> 22) & 15u) >= a0 * 16.0f)
            continue;
        p = s->px + py * s->w + px;
        if (*p < HT_SKY + 64 || *p == HT_MW || *p == HT_MW + 1)
            *p = (uint8_t)(HT_STW + ((h >> 26) & 1u));
    }
}

/* the stars: each out once the night is deep enough for it (the brightest first), fainter low in the haze,
 * and near the moon, which outshines them; the brightest with a soft halo */
static void ht_deep_draw(surf_t *s, float k);
static void ht_stars_draw(surf_t *s, float k)
{
    ht_deep_draw(s, k);                               /* (Deep Night's: under the stars) */
    int i;
    float mx = ht_moon_x(), my = ht_moon_y(),
          glare = (ht.mn.r * (1.6f + 2.2f * ht.mn.light) + 12.0f) * ht.mn_vis,   /* (a moon not seen: no glare) */
          dep = ht.depth * 255.0f + 40.0f;
    for (i = 0; i < HT_NSTAR; i++) {
        float x = (float)ht_st[i].x, y = (float)ht_st[i].y, dx = x - mx, dy = y - my, d2 = dx * dx + dy * dy, lv;
        int32_t px = (int32_t)(x * k), py = (int32_t)(y * k), l;
        uint8_t *p;
        if (dep < (float)ht_st[i].appear)
            continue;
        lv = (float)ht_st[i].lv - fm_clampf((dep - (float)ht_st[i].appear) < 30.0f ? 1.0f : 0.0f, 0.0f, 1.0f) -
             2.0f * fm_clampf((y - 0.55f * HT_HORIZON) / (0.4f * HT_HORIZON), 0.0f, 1.0f);   /* (coming out; the haze) */
        if (d2 < glare * glare * 2.5f)
            lv -= d2 < glare * glare ? 3.0f : 1.5f;   /* (the moon's glare) */
        l = (int32_t)(lv + 0.5f);
        if (l < 0 || px < 0 || py < 0 || px >= s->w || py >= s->h)
            continue;
        p = s->px + py * s->w + px;
        if (*p >= HT_SKY + 64 && *p != HT_MW && *p != HT_MW + 1)
            continue;                                 /* (only over the sky, the Milky Way's too) */
        *p = (uint8_t)(l >= 4 && ht_st[i].tint ? (ht_st[i].tint == 1 ? HT_STB : HT_STY) : HT_STW + (l > 5 ? 5 : l));
        if (l >= 4) {                                 /* a soft halo round the brightest */
            static const int8_t NB[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            int j;
            uint8_t h = (uint8_t)(ht_st[i].tint == 1 ? HT_STB + 1 : ht_st[i].tint == 2 ? HT_STY + 1 : HT_STAR);
            for (j = 0; j < 4; j++) {
                int32_t qx = px + NB[j][0], qy = py + NB[j][1];
                if (qx >= 0 && qy >= 0 && qx < s->w && qy < s->h && s->px[qy * s->w + qx] < HT_SKY + 64)
                    s->px[qy * s->w + qx] = h;
            }
        }
    }
}

static void ht_draw(void)
{
    surf_t *s = &r8_fb;
    float k = ht_scale();
    int i;
    SC_PT_TAGS("mscwo");                             /* (testing builds: the HUD's parts) */
    SC_PT_START();
    r8_copy(s, &r8_bg);
    ht_draw_moon(s, k);
    SC_PT(0);
    ht_stars_draw(s, k);
    for (i = 0; i < 3; i++)                           /* stars slowly brightening, then back (never a flash) */
        if (ht.glint[i] >= 0) {
            float u = ht.glint_t[i] / 5.0f, e = u < 0.4f ? u / 0.4f : (1.0f - u) / 0.6f;
            int32_t px = (int32_t)((float)ht_st[ht.glint[i]].x * k), py = (int32_t)((float)ht_st[ht.glint[i]].y * k);
            if (e > 0.25f && px >= 0 && py >= 0 && px < s->w && py < s->h &&
                (s->px[py * s->w + px] < HT_SKY + 64 || (s->px[py * s->w + px] >= HT_STW && s->px[py * s->w + px] < HT_STY + 2)))
                s->px[py * s->w + px] = (uint8_t)(HT_SHOOT + (e > 0.8f ? 0 : e > 0.5f ? 1 : 2));
        }
    if (ht.star.t >= 0.0f) {                          /* a shooting star: a bright head and a fading tail */
        float u = ht.star.t / 0.7f, l = fm_sqrtf(ht.star.vx * ht.star.vx + ht.star.vy * ht.star.vy) + 1e-3f;
        for (i = 0; i < 14; i++) {
            float b = (float)i * 2.2f, x = ht.star.x - ht.star.vx / l * b, y = ht.star.y - ht.star.vy / l * b;
            int c = i / 4 + (u > 0.6f ? 1 : 0);
            uint8_t *p;
            int32_t px = (int32_t)(x * k), py = (int32_t)(y * k);
            if (px < 0 || py < 0 || px >= s->w || py >= s->h || c > 3)
                continue;
            p = s->px + py * s->w + px;
            if (*p < HT_MOON)                         /* only over the sky */
                *p = (uint8_t)(HT_SHOOT + c);
        }
    }
    SC_PT(1);
    for (i = 0; i < HT_NCLOUD; i++)
        if (ht_c[i].wait <= 0.0f)
            ht_cloud_draw(s, k, i);
    SC_PT(2);
    ht_draw_stream(s, k);
    for (i = 0; i < HT_NRING; i++)                    /* rain's rings on the stream (only on the water) */
        if (ht_ring[i].t > 0.0f) {
            float u = 1.0f - ht_ring[i].t / 1.2f, rx = (2.0f + 12.0f * u) * k, ry = rx * 0.3f;
            int32_t j, n = 8 + (int32_t)(rx * 7.0f);
            for (j = 0; j < n; j++) {
                float t = (float)j / (float)n;
                int32_t x = (int32_t)(ht_ring[i].x * k + rx * fm_sin_turns(t + 0.25f)),
                        y = (int32_t)(ht_ring[i].y * k + ry * fm_sin_turns(t));
                uint8_t *q;
                if (x < 0 || y < 0 || x >= s->w || y >= s->h)
                    continue;
                q = s->px + y * s->w + x;
                if ((*q >= HT_WATER && *q < HT_WATER + 8) && u < 0.85f)
                    *q = (uint8_t)(HT_SHINE + (int)(u * 3.5f));
            }
            if (u < 0.15f) {                          /* the splash, a moment */
                int32_t x = (int32_t)(ht_ring[i].x * k), y = (int32_t)(ht_ring[i].y * k) - 1;
                if (x >= 0 && y >= 0 && x < s->w && y < s->h)
                    s->px[y * s->w + x] = HT_SHINE;
            }
        }
    {                                                 /* rain falling: short slanting streaks */
        for (i = 0; i < HT_NDROP; i++)
            if (ht_drop[i].on)
            r8_limb(s, ht_drop[i].x * k, ht_drop[i].y * k, 0.6f, (ht_drop[i].x - 0.9f) * k, (ht_drop[i].y - 6.0f) * k,
                    0.5f, (uint8_t)(HT_RAIN + (i & 1)));
    }
    SC_PT(3);
    for (i = 0; i < HT_NREED; i++) {                  /* reeds, far to near, bending with the wind */
        float bx = ht_r[i].bx, by = ht_r[i].by, tx = ht_r[i].top_x, ty = ht_r[i].top_y;
        float mx = bx + (tx - bx) * 0.45f + (tx - bx - ht_r[i].lean * ht_r[i].h) * -0.25f, my = by - ht_r[i].h * 0.5f;
        uint8_t c = (uint8_t)(HT_REED + 1 + ht_r[i].layer);
        float w = 1.2f + 0.35f * (float)ht_r[i].layer;
        /* (at least a whole pixel wide, whatever the style: in pixel art, half the resolution, a stem thinner than a
         * pixel broke into pieces that blinked as it swayed) */
        r8_limb(s, bx * k, by * k, fm_maxf(w * k, 1.1f), mx * k, my * k, fm_maxf(w * 0.8f * k, 1.1f), c);
        r8_limb(s, mx * k, my * k, fm_maxf(w * 0.8f * k, 1.1f), tx * k, ty * k, fm_maxf(w * 0.45f * k, 1.0f), c);
        r8_limb(s, bx * k, by * k, fm_maxf(w * 1.2f * k, 1.1f), (bx + (tx - bx) * 0.5f + 9.0f * (ht_r[i].ph - 0.5f) * 2.0f) * k,
                (by - ht_r[i].h * 0.42f) * k, fm_maxf(0.4f * k, 1.0f), c);   /* a leaf */
        if (ht_r[i].cattail) {
            float dx = tx - mx, dy = ty - my, l = fm_sqrtf(dx * dx + dy * dy) + 1e-3f;
            r8_limb(s, (tx - dx / l * 3.0f) * k, (ty - dy / l * 3.0f) * k, 3.0f * k, (tx - dx / l * 11.0f) * k,
                    (ty - dy / l * 11.0f) * k, 3.0f * k, c);
        }
    }
    if (ht.mist > 0.02f) {                            /* the mist:
                                                       * wisps of fog lying low over the field and the stream in
                                                       * three banks, drifting, thick in places with gaps between;
                                                       * toward dawn they rise and thin. Each pixel takes one of its
                                                       * bank's shades by how thick the fog is there (mixed only with
                                                       * the next shade: no pattern), over the land, the water and
                                                       * the stones; the reeds stand in it, in front. A little thicker
                                                       * under the moon; the farmhouse window warm through it */
        static const float YC[3] = {HT_HORIZON + 6.0f, HT_WTOP + 2.0f, (HT_WTOP + HT_WBOT) * 0.5f + 4.0f}, HH[3] = {6.0f, 10.0f, 12.0f};
        float dawn = fm_clampf((ht.night - 0.75f) * 4.0f, 0.0f, 1.0f), mxw = ht_moon_x(),
              mlit = 0.6f * ht.mn_vis * ht.mn.light;
        int bnd;
        for (bnd = 0; bnd < 3; bnd++) {
            float yb = YC[bnd] - 5.0f * dawn, bf = (float)bnd;
            int32_t y, x, y0 = (int32_t)((yb - HH[bnd] - 5.0f) * k), y1 = (int32_t)((yb + HH[bnd] + 5.0f) * k);
            for (x = 0; x < s->w; x++) {          /* (each bank's middle and thickness wander along it: uneven,
                                                   * lying low, thicker in places, gaps between) */
                float xw = (float)x / k, yc = yb + 3.0f * fm_sin_turns(xw * 0.011f + bf * 0.4f + ht.t * 0.003f),
                      hh = HH[bnd] * (0.6f + 0.5f * fm_sin_turns(xw * 0.017f - ht.t * 0.005f + bf)),
                      n = 0.55f + 0.35f * fm_sin_turns(xw * 0.006f + ht.t * 0.004f + bf * 0.3f) +
                          0.2f * fm_sin_turns(xw * 0.023f - ht.t * 0.007f),
                      wa = 0.5f + 0.5f * fm_sin_turns(xw * 0.009f - ht.t * 0.0023f + bf * 0.7f),   /* (where the
                                                   * wisps are: they come and go along it) */
                      ex = (xw - mxw) * (1.0f / 70.0f), lift = mlit * fm_maxf(0.0f, 1.0f - ex * ex),
                      hw = (xw - ht.hx) * (1.0f / 9.0f);
                n = (n - 0.12f) * ht.mist * (1.0f - 0.55f * dawn);
                for (y = y0 < 0 ? 0 : y0; y <= y1 && y < s->h; y++) {
                    uint8_t *p = s->px + y * s->w + x;
                    float yw = (float)y / k, dy = (yw - yc) / fm_maxf(hh, 1.0f), v = 1.0f - dy * dy,
                          st = fm_sin_turns(xw * 0.021f + yw * 0.16f + ht.t * 0.006f + bf * 1.9f), a, hv;
                    int lv, old, l2;
                    if (v <= 0.0f)
                        continue;
                    a = v * v * n * (1.0f + 0.45f * wa * st) * (1.0f + lift) * 4.6f;   /* (the wisps: long thin
                                                   * strands, lying almost flat) */
                    lv = (int)a;
                    lv += r8_dith(x, y, (int)((a - (float)lv) * 16.0f));
                    if (lv <= 0)
                        continue;
                    if (lv > 4)
                        lv = 4;
                    old = *p >= HT_FOGL ? ((*p - HT_FOGL) & 3) + 1 : 0;
                    if (old >= lv)
                        continue;                 /* (where two banks meet: the thicker) */
                    if (!old && !(*p < HT_SKY + 64 || (*p >= HT_HILL && *p < HT_GROUND + 8) ||
                                  (*p >= HT_WATER && *p < HT_WATER + 8) || (*p >= HT_TREE && *p < HT_HOUSE + 2) ||
                                  *p == HT_MW || *p == HT_MW + 1))
                        continue;                 /* (the reeds, the fireflies, the glints: in front) */
                    hv = hw * hw + (yw - HT_HORIZON + 2.5f) * (yw - HT_HORIZON + 2.5f) * (1.0f / 81.0f);
                    l2 = lv < 2 || hv >= 1.0f ? 0 : (int)((1.0f - hv) * 32.0f);   /* (near it: two warm steps) */
                    l2 = (l2 >> 4) + r8_dith(x, y, l2 & 15);
                    *p = (uint8_t)(l2 ? HT_FOGW + (l2 > 1) : HT_FOGL + bnd * 4 + lv - 1);
                }
            }
        }
    }
    for (i = 0; i < HT_NSPL; i++)                     /* a drop on a reed catching the light */
        if (ht.spl[i].t > 0.0f) {
            int32_t x = (int32_t)(ht_r[ht.spl[i].reed].top_x * k), y = (int32_t)((ht_r[ht.spl[i].reed].top_y + 4.0f) * k);
            if (x >= 0 && y >= 0 && x < s->w && y < s->h)
                s->px[y * s->w + x] = HT_SHINE;
        }
    for (i = 0; i < HT_NFF && i < ht_want_far(); i++)   /* the far fireflies: a point each as it flashes */
        if (ht_ff[i].b > 0.12f) {
            int32_t x = (int32_t)(ht_ff[i].x * k), y = (int32_t)(ht_ff[i].y * k);
            if (x >= 0 && y >= 0 && x < s->w && y < s->h)
                s->px[y * s->w + x] = (uint8_t)(HT_GLOW + (ht_ff[i].b > 0.6f ? 6 : 4));
        }
    for (i = 0; i < HT_MAXF; i++) {                   /* the fireflies: dark between flashes, each flash swelling and
                                                       * fading; its light on the grass near it */
        fly_t *f = &ht_f[i];
        float g, x, y, z;
        if (!f->on || f->fade < 0.03f)
            continue;
        g = (0.04f + 0.9f * f->b) * f->fade * (1.0f - 0.35f * ht.rain);   /* (sheltering in rain: dimmer) */
        if (g < 0.08f && f->flash <= 0.05f)
            continue;                                 /* (between flashes: not seen) */
        z = f->heike ? 0.7f : 1.0f;
        x = f->x * k;
        y = f->y * k;
        if (g > 0.3f && f->flash <= 0.05f) {          /* its light on the blades and the bank round it */
            int32_t yy, xx, r = (int32_t)(7.0f * g * k) + 1;
            for (yy = (int32_t)y - r; yy <= (int32_t)y + r; yy++)
                for (xx = (int32_t)x - r; xx <= (int32_t)x + r; xx++) {
                    uint8_t *q;
                    int32_t d2 = (xx - (int32_t)x) * (xx - (int32_t)x) + (yy - (int32_t)y) * (yy - (int32_t)y);
                    if (xx < 0 || yy < 0 || xx >= s->w || yy >= s->h || d2 > r * r)
                        continue;
                    q = s->px + yy * s->w + xx;
                    if (((*q >= HT_REED && *q < HT_REED + 8) || (*q >= HT_GROUND && *q < HT_GROUND + 8) || (*q >= HT_FORE && *q < HT_FORE + 4)) &&
                        r8_dith(xx, yy, (int)(g * 12.0f * (1.0f - (float)d2 / (float)(r * r + 1)))))
                        *q = HT_GLOW;
                }
        }
        if (ht.mist > 0.2f && f->flash <= 0.05f) {   /* in a mist, the light spread into a soft blur, in
                                                       * steps thinning outward */
            float r = (3.0f + 6.0f * g) * ht.mist * k;
            int32_t yy, xx;
            for (yy = (int32_t)(y - r); yy <= (int32_t)(y + r); yy++)
                for (xx = (int32_t)(x - r); xx <= (int32_t)(x + r); xx++) {
                    float dx = ((float)xx + 0.5f - x) / r, dy = ((float)yy + 0.5f - y) / r, d2 = dx * dx + dy * dy;
                    uint8_t *q;
                    int lv;
                    if (d2 >= 1.0f || xx < 0 || yy < 0 || xx >= s->w || yy >= s->h)
                        continue;
                    q = s->px + yy * s->w + xx;
                    lv = (int)((1.0f - d2) * (1.0f - d2) * (1.0f + 2.5f * g) * 16.0f);   /* (shades by how near,
                                                       * mixed only with the next) */
                    lv = (lv >> 4) + r8_dith(xx, yy, lv & 15);
                    if (lv > 3)
                        lv = 3;
                    if (lv > 0 && !(*q >= HT_GLOW && *q < HT_GLOW + 8) && !(*q >= HT_FOGG + lv - 1 && *q < HT_FOGG + 3))
                        *q = (uint8_t)(HT_FOGG + lv - 1);
                }
        }
        if (f->flash > 0.05f) {                       /* a key's flash: a wide halo and a white heart */
            float fl = f->flash;
            r8_disc(s, x, y, (3.0f + 9.0f * fl) * k, (uint8_t)(HT_GLOW + 1 + (int)(fl * 2.0f)));
            r8_disc(s, x, y, (2.0f + 5.0f * fl) * k, (uint8_t)(HT_GLOW + 4 + (int)(fl * 2.0f)));
            r8_disc(s, x, y, (1.0f + 2.5f * fl) * k, HT_GLOW + 7);
            continue;
        }
        r8_disc(s, x, y, (1.2f + 3.6f * g) * z * k, (uint8_t)(HT_GLOW + (int)(g * 2.6f)));
        r8_disc(s, x, y, (0.8f + 2.0f * g) * z * k, (uint8_t)(HT_GLOW + 2 + (int)(g * 3.6f)));
        r8_disc(s, x, y, (0.5f + 0.9f * g) * z * k, (uint8_t)(g > 0.62f ? HT_GLOW + 7 : HT_GLOW + 6));
    }
    SC_PT(4);
}

/* ---------------------------------------------------------------- scene --- */
static void ht_water(void)
{
    if (ht.kv[HK_WATERIS] > 0) {
        fm_bed_style(FM_BED_STREAM, ht.kv[HK_WATERIS] - 1);
        fm_bed(FM_BED_STREAM, 40);
    } else {
        fm_bed(FM_BED_STREAM, 0);
    }
}

static void ht_mix_levels(void)
{
    fm_part_level(1, ht.kv[HK_NOTES]);
    fm_part_level(6, ht.kv[HK_NOTES]);                /* a shooting star's chime */
    cd_drone_level(ht.kv[HK_DRONE]);
    fm_bed(FM_BED_WIND, ht.kv[HK_AIR]);
    ht_water();
}

SC_SMALL static void ht_defaults(int16_t *kv)
{
    int i;
    for (i = 0; i < SC_NKNOB; i++)
        kv[i] = HT_KNOB[i][3];
}

SC_SMALL static void ht_enter(uint32_t seed, int16_t *kv)
{
    int i;
    memset(&ht, 0, sizeof ht);
    ht.kv = kv;
    for (i = 0; i < SC_NKNOB; i++)
        if (HT_KNOB[i][1] > HT_KNOB[i][0] && (kv[i] < HT_KNOB[i][0] || kv[i] > HT_KNOB[i][1]))
            kv[i] = HT_KNOB[i][3];
    for (i = 0; i < HT_MAXF; i++)
        ht_f[i].on = 0;
    for (i = 0; i < HT_NQ; i++)
        ht_q[i].at = 0;
    for (i = 0; i < 3; i++)
        ht_fr[i].on = 0;
    ht.seed = seed | 1u;
    ht.night = 0.05f + 0.15f * (float)(seed % 100u) * 0.01f;   /* early in the night: dusk, the most fireflies */
    ht.hold = HT_MOOD_NIGHT[sc_mood_base(kv[HK_MOOD], HT_NMOOD)];   /* (Custom: its mood's night) */
    if (ht.hold >= 0.0f)
        ht.night = ht.hold;
    ht.emerge = ht.hold < 0.0f ? 0.25f : 1.0f;        /* (Natural: a few at first, the rest coming out) */
    sc_glide_at(&ht.glide, ht.hold);
    ht.sync = HT_MOOD_SYNC[sc_mood_base(kv[HK_MOOD], HT_NMOOD)];
    ht.wave_ph = 0.0f;                                /* no coupling yet: free at first, in step in ~2 min */
    ht.wave_per = 300.0f;
    ht.frog_next = 4.0f;
    ht.owl_next = 40.0f;
    ht.star_next = 50.0f + 60.0f * (float)(seed % 64u) / 64.0f;
    ht.star.t = -1.0f;
    for (i = 0; i < 3; i++) {
        ht.glint[i] = -1;
        ht.glint_next[i] = 4.0f + 14.0f * sc_frand();
    }
    for (i = 0; i < HT_NCLOUD; i++) {                 /* one cloud already on its way, the others later */
        ht_cloud_new(i, -70.0f);
        ht_c[i].wait = i ? 20.0f + 90.0f * sc_frand() : 0.0f;
        if (!i)
            ht_c[i].x = 30.0f + 120.0f * sc_frand();
    }
    ht.r_min = 1.0f;
    for (i = 0; i < HT_NFF; i++) {                    /* the far fireflies: along the far bank, at the woods' foot */
        ht_ff[i].x = 240.0f * sc_frand();
        ht_ff[i].y = HT_HORIZON - 4.0f + (HT_WTOP - HT_HORIZON + 4.0f) * sc_frand();
        ht_ff[i].ph = sc_frand();
        ht_ff[i].w = 1.0f / (3.8f + 2.0f * sc_frand());
    }
    ht.tokens = 1.0f;
    ht.insects_sent = -1;
    sc_seed(ht.seed * 2654435761u + 5u);
    ht.depth = 0.5f - 0.5f * fm_sin_turns(ht.night + 0.25f);
    {
        int n = ht_want();
        for (i = 0; i < n; i++) {
            fly_t *f = ht_fly_new(10.0f + 220.0f * sc_frand(), HT_YTOP + (HT_YBOT - HT_YTOP) * sc_frand());
            if (f)
                f->fade = 1.0f;
        }
    }
    r8_dark(HT_DARK);
    cd_init(seed);
    cd_activity((float)kv[HK_ACT] * 0.01f);
    cd_echo_amount((float)kv[HK_ECHO] * 0.01f);
    cd_change_mode(1);                                /* (the key's drift: always slow) */
    if (sc_mood_base(kv[HK_MOOD], HT_NMOOD) > 0) {    /* (a named mood: its own key and scale) */
        int b = sc_mood_base(kv[HK_MOOD], HT_NMOOD);
        cd_tune(HT_MOOD_SCALE[b], HT_MOOD_KEY[b]);
    }
    cd_range(HT_KLO, HT_KN);
    fm_part_patch(1, HT_TONE_SLOT[kv[HK_TONE] % HT_NTONE]);
    fm_part_patch(2, ZN_SOFT_PAD);                    /* the drone */
    fm_part_patch(6, ZN_FURIN);                       /* a shooting star */
    fm_part_release(1, 2600);
    fm_part_release(2, 4000);
    fm_part_release(6, 2600);
    ht_mix_levels();
    cd_drone(1);
    fm_tape(0);
    fm_bed_mod(FM_BED_STREAM, 10);
    fm_bed_style(FM_BED_WIND, kv[HK_AIRIS]);
    fm_bed(FM_BED_RAIN, 0);
    ht.depth_sent = -1;
    cd_melody(kv[HK_MELODY] == 3);
    cd_arc_amount((float)kv[HK_ARC] * 0.01f);
    ht_weather_set(1);                                /* (the night's weather, from the first) */
    ht_depth_send();
}

SC_SMALL static void ht_leave(void)
{
    int b;
    cd_stop();
    fm_tape(0);
    fm_depth(0);
    r8_dark(0);
    for (b = 0; b < FM_NBED; b++)
        fm_bed(b, 0);
}

static void ht_fly_step(fly_t *f, float dt, float kc, float cx, float cy)
{
    float before = f->ph;
    /* the blink: its own rate, pulled toward the field's (Kuramoto: K R sin(psi - phi), here without the angle) */
    f->ph += dt * (f->w + kc * (cy * fm_sin_turns(f->ph + 0.25f) - cx * fm_sin_turns(f->ph)));
    if (f->ph >= 1.0f && before < 1.0f) {
        f->ph -= fm_floorf(f->ph);
        f->b = 1.0f;
        ht.blinks++;
        if (f->fade > 0.6f && !f->settling) {         /* heard: part of a chord, if one is gathering */
            if (!ht.pulse_n)
                ht.pulse_left = 0.25f;
            if (ht.pulse_n < 3) {
                ht.pulse_key[ht.pulse_n] = ht_key_of(f);
                ht.pulse_x[ht.pulse_n++] = f->x;
            }
        }
    }
    if (f->ph < 0.0f)
        f->ph -= fm_floorf(f->ph);
    f->b *= fm_maxf(0.0f, 1.0f - dt * (f->heike ? 4.0f : 1.5f));   /* a flash fades over ~1.5 s (a Heike's quicker) */
    f->flash *= fm_maxf(0.0f, 1.0f - dt * 1.6f);      /* (a key's flash fades over ~1.5 s) */
    if (f->settling) {                                /* down into the grass, going out */
        f->fade -= dt * 0.25f;
        f->y += dt * 4.0f;
        if (f->fade <= 0.0f)
            f->on = 0;
        return;
    }
    if (f->fade < 1.0f)
        f->fade = fm_minf(1.0f, f->fade + dt * 0.3f);
    if (f->perch >= 0) {                              /* resting on a reed's top a moment, swaying with it */
        f->x = ht_r[f->perch].top_x;
        f->y = ht_r[f->perch].top_y - 1.5f;
        f->perch_t -= dt;
        if (f->perch_t <= 0.0f)
            f->perch = -1;
        return;
    }
    /* always drifting (in a pulse they looked parked): a slow heading that turns by a wandering amount, flatter
     * up and down than across, steered back in from the edges */
    f->tr = fm_clampf(f->tr + (sc_frand() - 0.5f) * 0.5f * dt, -0.1f, 0.1f);
    {
        float want = -1.0f;
        if (f->x < 14.0f)
            want = 0.0f;
        else if (f->x > 226.0f)
            want = 0.5f;
        else if (f->y < HT_YTOP + 4.0f)
            want = 0.25f;
        else if (f->y > HT_YBOT - 4.0f)
            want = 0.75f;
        if (want >= 0.0f) {
            float d = want - f->h;
            d -= fm_floorf(d + 0.5f);
            f->h += d * 0.9f * dt;
        }
    }
    f->h += f->tr * dt;
    f->h -= fm_floorf(f->h);
    f->x += (f->sp * fm_sin_turns(f->h + 0.25f) + ht.gust * ht.gust_dir * (f->blow ? 30.0f : 7.0f) + (f->blow ? 10.0f : 0.0f)) * dt;
    if (f->ty_t > 0.0f) {                             /* rising to a height (born, or a key's) */
        f->ty_t -= dt;
        f->y += fm_clampf((f->ty - f->y) * 0.8f, -14.0f, 14.0f) * dt;
    } else {
        f->y += f->sp * 0.55f * fm_sin_turns(f->h) * dt;
    }
    if (f->blow && (f->x > 250.0f || f->x < -10.0f)) {   /* carried off by the breeze: gone */
        f->on = 0;
        return;
    }
    if (f->x < -10.0f || f->x > 250.0f)               /* (otherwise: back in from the other side) */
        f->x = f->x < 0.0f ? 248.0f : -8.0f;
    if (ht.gust < 0.1f && f->ty_t <= 0.0f && sc_frand() < dt * (0.004f + 0.1f * ht.rain)) {   /* rarely, a short rest on
                                                       * a reed; in rain, they shelter there */
        int r = (int)(sc_rnd() % HT_NREED);
        float dx = ht_r[r].top_x - f->x, dy = ht_r[r].top_y - f->y;
        if (dx * dx + dy * dy < 25.0f * 25.0f) {
            f->perch = r;
            f->perch_t = (3.0f + 3.0f * sc_frand()) * (1.0f + 3.0f * ht.rain);
        }
    }
}

static void ht_step(uint32_t dt_ms)
{
    float dt = (float)dt_ms * 0.001f, a = (float)ht.kv[HK_ACT] * 0.01f;
    uint32_t cdt = dt_ms;
    float cx = 0.0f, cy = 0.0f, kc;
    int i, n = 0;
    ht.t += dt;
    ht.ms += cdt;
    if (ht.hold < 0.0f) {                             /* the night: dusk (0) .. deep (0.5) .. dawn; half an hour
                                                       * (Natural) */
        ht.night += dt * (1.0f / 1800.0f);
        if (ht.night >= 1.0f) {                       /* a new night: another moon (its size the same), at once:
                                                       * never seen changing, as either is pale in the twilight
                                                       * (ht_moon_tw) */
            sc_moon_t m;
            sc_moon_repick(&m, 1.0f, ht.seed * 2246822519u + 977u * ++ht.moons);
            ht.nat_light = m.light;
            ht.nat_off = m.off;
            ht.nat_side = m.side;
            ht_mood_moon_to(ht.kv ? sc_mood_base(ht.kv[HK_MOOD], HT_NMOOD) : 0, 0);
            ht.night -= 1.0f;
            ht.emerge = 0.25f;                        /* (a new night: they come out again) */
            cd_dawn();
        }
        sc_glide_at(&ht.glide, ht.hold);
    } else                                            /* a mood holds its time: there in 20 s, gently */
        ht.night = sc_glide(&ht.glide, ht.night, ht.hold, dt);
    ht.emerge = fm_minf(1.0f, ht.emerge + dt * (ht.hold < 0.0f ? 1.0f / 240.0f : 1.0f / 30.0f));   /* (Natural: out
                                                       * over ~4 min; a mood: out as it glides there, ~30 s) */
    ht_moon_glide(dt);
    ht.depth = 0.5f - 0.5f * fm_sin_turns(ht.night + 0.25f);
    /* the breeze (a black key): it rises over ~2.5 s, then dies away over ~8 s (it came at once) */
    if (ht.gust_to > 0.0f) {
        ht.gust = fm_minf(1.0f, ht.gust + dt * 0.4f);
        if (ht.gust >= 1.0f)
            ht.gust_to = 0.0f;
    } else if (ht.gust > 0.0f) {
        ht.gust = fm_maxf(0.0f, ht.gust - dt * 0.125f);
    }
    {   /* the crickets fill in as the night deepens; a breeze or leaves lift only with a breath of wind (the amount
         * lifts them: a fixed night amount made them louder, "wind is way too loud") */
        int m = ht.kv[HK_AIRIS] == FM_AIR_INSECTS ? (int)(50.0f + 50.0f * ht.depth) : (int)(10.0f + 65.0f * ht.gust);
        if (m / 4 != ht.insects_sent / 4) {
            fm_bed_mod(FM_BED_WIND, m);
            ht.insects_sent = m;
        }
    }
    cd_step(cdt);
    cd_light(ht.night < 0.08f || ht.night > 0.92f ? SC_YO : SC_MINYO);   /* (Natural: Yo at dusk and dawn, Min'yo
                                                       * through the night) */
    for (i = 0; i < HT_NQ; i++)                       /* the scheduled notes: strums, the owl */
        if (ht_q[i].at && (int32_t)(ht.ms - ht_q[i].at) >= 0) {
            fm_voice(ht_q[i].id, ht_q[i].part, ht_q[i].cents, ht_q[i].vel, ht_q[i].pan, ht_q[i].dur);
            if (ht_q[i].part == 1)
                cd.notes++;
            ht_q[i].at = 0;
        }
    ht_frogs_step();
    {   /* the weather: on Auto, another now and then; easing toward it */
        float e = (ht.wx_fast ? 0.25f : 1.0f / 60.0f) * dt;   /* (a steady pace that ends: by hand ~4 s, on
                                                       * Auto ~1 min; the earlier easing trailed on) */
        if (!ht.kv[HK_WEATHER] && (ht.wx_next -= dt) <= 0.0f) {
            int w = ht_weather_pick();
            if (w == ht.wx)
                w = (w + 1 + (int)(sc_rnd() % 2u)) % 3;
            ht.wx = w;
            ht.wx_fast = 0;
            ht.wx_next = 180.0f + 240.0f * sc_frand();
            ht.weathers++;
        }
        ht.mist += fm_clampf((ht.wx == 1 ? 1.0f : 0.0f) - ht.mist, -e, e);
        ht.rain += fm_clampf((ht.wx == 2 ? 1.0f : 0.0f) - ht.rain, -e, e);
        ht.deep += ((sc_mood_base(ht.kv[HK_MOOD], HT_NMOOD) == 4 ? 1.0f : 0.0f) - ht.deep) * fm_minf(1.0f, dt * 0.3f);
        ht_depth_send();
        fm_bed(FM_BED_RAIN, (int)(38.0f * ht.rain));
        fm_bed_mod(FM_BED_RAIN, (int)(35.0f * ht.rain));       /* (how many drops: ~80 a second at full rain) */
        ht.rain_dt += dt;                             /* rain falling (photos, 33 fps in rain: the
                                                       * drops step every other frame, ~25-30 a second, so the screen
                                                       * they cover is sent half as often; the rest moves every frame.
                                                       * each drop on its own; those in the air fall on out
                                                       * when it stops) */
        if ((ht.rain_ph ^= 1u) == 0u) {
            sc_rain_step(ht_drop, HT_NDROP, &ht.drops, ht.rain, 30.0f, 200.0f, ht.rain_dt);
            ht.rain_dt = 0.0f;
        }
        if (ht.rain > 0.01f) {                        /* its rings on the stream */
            if (sc_frand() < ht.drops * ht.drops * dt * 12.0f)   /* (with the falling rain, a few first) */   /* (three times as many, each heard where it lands) */
                for (i = 0; i < HT_NRING; i++)
                    if (ht_ring[i].t <= 0.0f) {
                        ht_ring[i].x = 240.0f * sc_frand();
                        ht_ring[i].y = HT_WTOP + 4.0f + (HT_WBOT - HT_WTOP - 6.0f) * sc_frand();
                        ht_ring[i].t = 1.2f;
                        fm_drop(45 + (int)(sc_rnd() % 40u), cd_pan(ht_ring[i].x / 240.0f), 0);
                        break;
                    }
            if (sc_frand() < ht.drops * ht.drops * dt * 1.2f)   /* now and then a drop on a reed, catching the light */
                for (i = 0; i < HT_NSPL; i++)
                    if (ht.spl[i].t <= 0.0f) {
                        ht.spl[i].reed = (int)(sc_rnd() % HT_NREED);
                        ht.spl[i].t = 0.3f;
                        break;
                    }
        }
        for (i = 0; i < HT_NRING; i++)
            if (ht_ring[i].t > 0.0f)
                ht_ring[i].t -= dt;
        for (i = 0; i < HT_NSPL; i++)
            if (ht.spl[i].t > 0.0f)
                ht.spl[i].t -= dt;
    }
    ht.tokens = fm_minf(2.0f, ht.tokens + dt * (0.12f + 0.45f * a) * (0.6f + 0.7f * ht.mn.light));   /* a chord
                                                       * every ~3 s at the default (a full moon more, a crescent fewer) */
    if (ht.kv[HK_FROGS] > 0) {
        ht.frog_next -= dt;
        if (ht.frog_next <= 0.0f && !ht_fr[0].on && !ht_fr[1].on) {
            int frog = (int)(sc_rnd() & 1u);
            ht_frog_call(frog, 0u, 0);
            if (sc_frand() < 0.7f)                    /* the other answers */
                ht_frog_call(!frog, 2600u + sc_rnd() % 1200u, 0);
            ht.frog_next = (9.0f + 16.0f * sc_frand()) * (1.0f - 0.5f * ht.rain);   /* (more in rain) */
        }
    }
    if (ht.kv[HK_OWL] > 0) {
        ht.owl_next -= dt;
        if (ht.owl_next <= 0.0f) {
            ht_owl_call();
            ht.owl_next = (25.0f + 2.0f * (float)(100 - ht.kv[HK_OWL])) * (0.7f + 0.6f * sc_frand());
        }
    }
    for (i = 0; i < 3; i++) {                         /* each glint: ~2 s up, ~3 s back; its next at random */
        if (ht.glint[i] >= 0) {
            ht.glint_t[i] += dt;
            if (ht.glint_t[i] >= 5.0f)
                ht.glint[i] = -1;
        } else if ((ht.glint_next[i] -= dt) <= 0.0f) {
            ht.glint[i] = (int)(sc_rnd() % 14u);       /* (one of the brighter ones) */
            ht.glint_t[i] = 0.0f;
            if (ht.rain < 0.5f) {                     /* heard (after Avonlea's glints): as it brightens, a
                                                       * faint high bell, an octave over the chord's top */
                int kk = HT_KLO + HT_KN - 1 - (int)(sc_rnd() % 4u);
                ht_q_add(1600u, HT_ID_STAR + 1 + i, 6, cd_cents(kk) + 1200.0f, 18 + (int)(sc_rnd() % 8u),
                         cd_pan((float)ht_st[ht.glint[i]].x / 240.0f), 1600);
                ht.pings++;
            }
            ht.glint_next[i] = 6.0f + 18.0f * sc_frand();
        }
    }
    for (i = 0; i < HT_NCLOUD; i++) {                 /* the clouds drift across, and after one leaves, a while
                                                       * (30..150 s) before the next */
        if (ht_c[i].wait > 0.0f) {
            if ((ht_c[i].wait -= dt) <= 0.0f)
                ht_cloud_new(i, -70.0f);
            continue;
        }
        ht_c[i].x += ht_c[i].v * dt;
        if (ht_cloud_gone(i))
            ht_c[i].wait = 30.0f + 120.0f * sc_frand();
    }
    ht.star_next -= dt;                               /* now and then a shooting star, more in the deep of the night */
    if (ht.star_next <= 0.0f && ht.star.t < 0.0f) {
        float dir = sc_frand() < 0.5f ? -1.0f : 1.0f;
        ht.star.x = 40.0f + 160.0f * sc_frand();
        ht.star.y = 12.0f + 50.0f * sc_frand();
        ht.star.vx = dir * (150.0f + 80.0f * sc_frand());
        ht.star.vy = 60.0f + 50.0f * sc_frand();
        ht.star.t = 0.0f;
        ht.stars++;
        fm_voice(HT_ID_STAR, 6, cd_cents(HT_KLO + HT_KN - 1 - (int)(sc_rnd() % 3u)), 30, cd_pan(ht.star.x / 240.0f), 900);
        ht.star_next = (70.0f + 170.0f * sc_frand()) * (1.0f - 0.5f * ht.depth) *
                       (ht.wx == 0 ? 0.7f : 1.0f + 2.0f * (ht.mist + ht.rain));   /* (clear: more) */
    }
    if (ht.star.t >= 0.0f) {
        ht.star.t += dt;
        ht.star.x += ht.star.vx * dt;
        ht.star.y += ht.star.vy * dt;
        if (ht.star.t > 0.7f || ht.star.y > HT_HORIZON)
            ht.star.t = -1.0f;
    }
    /* the coupling's slow wave: none for a while, rising, held, ebbing (a new length each time) */
    ht.wave_ph += dt / ht.wave_per;
    if (ht.wave_ph >= 1.0f) {
        ht.wave_ph -= 1.0f;
        ht.wave_per = 240.0f + 180.0f * sc_frand();
    }
    ht.wave = fm_clampf(1.7f * (0.5f - 0.5f * fm_sin_turns(ht.wave_ph + 0.25f)) - 0.35f, 0.0f, 1.0f);
    kc = 0.32f * (float)ht.sync * 0.01f * ht.wave;   /* turns a second: their own rates differ by up to
                                                              * ~0.07, so past ~0.06 they fall into step */
    for (i = 0; i < HT_MAXF; i++)                     /* the field's rhythm: the mean of every firefly's phase (the
                                                       * Genji only: a Heike keeps its own) */
        if (ht_f[i].on && ht_f[i].fade > 0.5f && !ht_f[i].heike) {
            cx += fm_sin_turns(ht_f[i].ph + 0.25f);
            cy += fm_sin_turns(ht_f[i].ph);
            n++;
        }
    if (n) {
        cx /= (float)n;
        cy /= (float)n;
    }
    ht.r_sync = fm_sqrtf(cx * cx + cy * cy);
    ht.r_avg += (ht.r_sync - ht.r_avg) * fm_minf(1.0f, dt / 20.0f);   /* (its 20 s average: the host's tests) */
    if (ht.t > 60.0f && ht.r_avg < ht.r_min)
        ht.r_min = ht.r_avg;
    {   /* the population: rise from the grass, or settle (one at a time) */
        int alive = ht_alive(), want = ht_want();
        ht.spawn_acc += dt;
        if (alive < want && ht.spawn_acc > 3.0f) {
            ht_fly_new(20.0f + 200.0f * sc_frand(), HT_YBOT + 8.0f);
            ht.spawn_acc = 0.0f;
        } else if (alive > want && ht.spawn_acc > 6.0f) {
            for (i = 0; i < HT_MAXF; i++)
                if (ht_f[i].on && !ht_f[i].settling && ht_f[i].ty_t <= 0.0f) {
                    ht_f[i].settling = 1;
                    break;
                }
            ht.spawn_acc = 0.0f;
        }
    }
    ht.lean += (0.12f * ht.gust * ht.gust_dir - ht.lean) * fm_minf(1.0f, dt * 0.6f);   /* (eased: ~1.7 s) */
    for (i = 0; i < HT_NREED; i++) {                  /* the reeds: a slow sway, and the breeze */
        float sw = 0.05f * fm_sin_turns(ht.t * 0.16f + ht_r[i].ph) + 0.03f * fm_sin_turns(ht.t * 0.37f + ht_r[i].ph * 2.0f) +
                   ht.lean * (0.8f + 0.4f * ht_r[i].ph);
        ht_r[i].top_x = ht_r[i].bx + (ht_r[i].lean + sw) * ht_r[i].h;
        ht_r[i].top_y = ht_r[i].by - ht_r[i].h * (1.0f - 0.5f * sw * sw);
    }
    if (ht.pulse_n) {                                 /* a chord gathering: blinks within 250 ms are one */
        ht.pulse_left -= (float)cdt * 0.001f;
        if (ht.pulse_left <= 0.0f)
            ht_pulse_play();
    }
    for (i = 0; i < HT_MAXF; i++)
        if (ht_f[i].on)
            ht_fly_step(&ht_f[i], dt, ht_f[i].heike ? 0.0f : kc, cx, cy);
    for (i = 0; i < HT_NFF; i++) {                    /* the far ones: in step with the field as it pulses, drifting a
                                                       * little */
        float before = ht_ff[i].ph;
        ht_ff[i].ph += dt * (ht_ff[i].w + kc * (cy * fm_sin_turns(ht_ff[i].ph + 0.25f) - cx * fm_sin_turns(ht_ff[i].ph)));
        if (ht_ff[i].ph >= 1.0f || ht_ff[i].ph < 0.0f) {
            ht_ff[i].ph -= fm_floorf(ht_ff[i].ph);
            if (before < 1.0f && before >= 0.0f)
                ht_ff[i].b = 1.0f;
        }
        ht_ff[i].b *= fm_maxf(0.0f, 1.0f - dt * 3.0f);
        ht_ff[i].x += 0.4f * fm_sin_turns(ht.t * 0.05f + (float)i * 0.37f) * dt;
    }
    ht_palette();
}

SC_SMALL static void ht_key(int key, int on)
{
    int b;
    if (!on)
        return;
    for (b = 0; b < FM_NBLACK; b++)
        if (BLACK_K[b] == key) {
            if (b == FM_NBLACK - 1) {                 /* the top one: the clearing breeze */
                ht_scatter(1.0f);
            } else {                                  /* the others: a note each, low to high, and a firefly near its
                                                       * height flashes with it */
                static const uint8_t BK_W[FM_NBLACK - 1] = {0, 2, 3, 5, 7, 8, 10, 12, 13, 15};   /* (over the
                                                       * sixteen white keys' notes, low to high) */
                int k = HT_KLO + BK_W[b], j, best = -1, n = 0, dark = 0;
                float ty = ht_y_of_white(k), bd = 1e9f;
                for (j = 0; j < HT_MAXF; j++)         /* near its height (not one a key has just lit): a dark one,
                                                       * at random, so again lights another */
                    if (ht_f[j].on && !ht_f[j].settling && !ht_f[j].blow && ht_f[j].fade > 0.3f &&
                        ht_f[j].flash < 0.25f && fm_fabsf(ht_f[j].y - ty) < 16.0f) {
                        int dk = ht_f[j].b < 0.3f;
                        if (dk > dark)
                            n = 0, dark = dk;
                        if (dk == dark && sc_rnd() % (uint32_t)++n == 0u)
                            best = j;
                    }
                if (best < 0) {                       /* none near: the nearest, drifting to that height as it lights */
                    for (j = 0; j < HT_MAXF; j++)
                        if (ht_f[j].on && !ht_f[j].settling && !ht_f[j].blow && ht_f[j].fade > 0.3f) {
                            float d = fm_fabsf(ht_f[j].y - ty);
                            if (d < bd) {
                                bd = d;
                                best = j;
                            }
                        }
                    if (best >= 0) {
                        ht_f[best].ty = ty;
                        ht_f[best].ty_t = 8.0f;
                    }
                }
                if (best >= 0) {
                    ht_f[best].b = 1.0f;
                    ht_f[best].flash = 1.0f;          /* (a key's flash much brighter, bigger: easy to see) */
                }
                cd_note_key(k, best >= 0 ? ht_f[best].x / 240.0f : 0.5f, 0.45f, 1400, 1, 0u);
                ht.rel_key = k;
                ht.key_flashes++;
            }
            return;
        }
    for (b = 0; b < FM_NWHITE; b++)
        if (WHITE_K[b] == key) {
            int k = b + HT_KLO;                       /* (sixteen notes, each its own, rising) */
            float x = 12.0f + 216.0f * (float)b / (float)(FM_NWHITE - 1);
            fly_t *f;
            f = ht_fly_new(x, HT_YBOT + 6.0f);
            if (f) {
                int j;
                f->ty = ht_y_of_white(k);
                f->ty_t = 25.0f;
                f->key = k;
                for (j = 0; j < HT_MAXF; j++)         /* it joins the rhythm: a phase of the field, if there is one */
                    if (ht.r_sync > 0.5f && &ht_f[j] != f && ht_f[j].on && ht_f[j].fade > 0.5f) {
                        f->ph = ht_f[j].ph;
                        f->w = ht_f[j].w;
                        break;
                    }
                f->b = 1.0f;
                f->fade = 0.7f;
            }
            cd_note_key(k, x / 240.0f, HT_KEY_ST + HT_KEY_LIFT * (1.0f - (float)(k - HT_KLO) / (float)(HT_KN - 1)), 1400, 1, 0u);   /* its first light, heard at once */
            ht.released++;
            ht.rel_key = k;
            return;
        }
}

SC_SMALL static void ht_knob(int i, int32_t e)
{
    if (i == HK_MOOD) {                               /* a mood: every knob to the sound, its time, key, scale */
        int m = sc_mood_base(ht.kv[HK_MOOD], HT_NMOOD), j;   /* (the glue has put the mood's knobs in, as
                                                       * made or as the player left them: every one to the sound) */
        (void)e;
        for (j = 1; j < SC_NUSED; j++)
            if (j != HK_SPACE)
                ht_knob(j, 0);
        cd_tune(HT_MOOD_SCALE[m], HT_MOOD_KEY[m]);
        ht.hold = HT_MOOD_NIGHT[m];
        ht.sync = HT_MOOD_SYNC[m];
        ht_mood_moon_to(m, 1);                        /* (gliding there) */
        cd_activity((float)ht.kv[HK_ACT] * 0.01f);
        cd_echo_amount((float)ht.kv[HK_ECHO] * 0.01f + 0.3f * (1.0f - ht.mn.light));
        ht_mix_levels();
        ht_weather_set(0);
        ht_depth_send();
        return;
    }
    if (i == HK_DEPTH || i == HK_ARC) {
        sc_knob_turn(&ht.kv[i], e, 0, 100, 1);
        cd_arc_amount((float)ht.kv[HK_ARC] * 0.01f);
        ht_depth_send();
        return;
    }
    if (i == HK_MELODY) {                             /* Height, Across, Own, Loops */
        sc_knob_wrap(&ht.kv[i], e, 0, 3);
        cd_melody(ht.kv[i] == 3);
        return;
    }
    if (i == HK_WEATHER) {                            /* Auto, Clear, Mist, Rain: it eases in */
        sc_knob_wrap(&ht.kv[i], e, 0, 3);
        ht_weather_set(0);
        return;
    }
    if (i == HK_SPACE) {
        knob_set(P_REVERB, proj.par[P_REVERB] + (int)(e * sc_knob_mul));   /* (by 1) */
        return;
    }
    if (i < 0 || i >= SC_NKNOB || HT_KNOB[i][1] <= HT_KNOB[i][0])
        return;
    if (i == HK_AIRIS) {                              /* (Soft, Breeze, Insects) */
        sc_air_wrap(&ht.kv[i], e);
        fm_bed_style(FM_BED_WIND, ht.kv[i]);
        ht.insects_sent = -1;
        return;
    }
    if (i == HK_TONE || i == HK_WATERIS || i == HK_AIRIS)
        sc_knob_wrap(&ht.kv[i], e, HT_KNOB[i][0], HT_KNOB[i][1]);
    else
        sc_knob_turn(&ht.kv[i], e, HT_KNOB[i][0], HT_KNOB[i][1], HT_KNOB[i][2]);
    switch (i) {
    case HK_ACT: cd_activity((float)ht.kv[HK_ACT] * 0.01f); break;
    case HK_TONE: fm_part_patch(1, HT_TONE_SLOT[ht.kv[HK_TONE] % HT_NTONE]); break;
    case HK_ECHO: cd_echo_amount((float)ht.kv[HK_ECHO] * 0.01f + 0.3f * (1.0f - ht.mn.light)); break;
    case HK_WATERIS: ht_water(); break;
    case HK_AIRIS: fm_bed_style(FM_BED_WIND, ht.kv[HK_AIRIS]); ht.insects_sent = -1; break;
    case HK_NOTES: case HK_DRONE: case HK_AIR: case HK_FROGS: ht_mix_levels(); break;
    default: break;
    }
}

SC_SMALL static void ht_knob_text(int i, char *name, char *val)
{
    static const char *const N[SC_NUSED] = {"Mood", "Activity", "Instrument", "Space", "Notes", "Drone", "Air", "Frogs",
                                            "Air Type", "Water Type", "Echo", "Warmth", "Owl Calls", "Weather",
                                            "Phrases", "Tide"};
    static const char *const AIR[4] = {"Soft", "Breeze", "Breeze", "Insects"};
    static const char *const PITCH[4] = {"Height", "Across", "Own", "Loops"};
    if (i < 0 || i >= SC_NUSED || (i != HK_SPACE && HT_KNOB[i][1] <= HT_KNOB[i][0])) {
        name[0] = val[0] = 0;
        return;
    }
    copy_str(name, N[i], 11);
    switch (i) {
    case HK_MOOD: copy_str(val, HT_MOOD_NAME[sc_mood_base(ht.kv[HK_MOOD], HT_NMOOD)], 11); break;
    case HK_WEATHER: copy_str(val, HT_WEATHER_NAME[ht.kv[HK_WEATHER] & 3], 11); break;
    case HK_TONE: copy_str(val, HT_TONE_NAME[ht.kv[HK_TONE] % HT_NTONE], 11); break;
    case HK_SPACE: itoa_u((uint32_t)proj.par[P_REVERB], val); break;
    case HK_WATERIS: copy_str(val, ht.kv[HK_WATERIS] ? SC_WATER_NAME[(ht.kv[HK_WATERIS] - 1) % FM_NWATER] : "Off", 11);
        break;
    case HK_AIRIS: copy_str(val, AIR[ht.kv[HK_AIRIS] & 3], 11); break;
    case HK_MELODY: copy_str(val, PITCH[ht.kv[HK_MELODY] & 3], 11); break;
    default: itoa_u((uint32_t)ht.kv[i], val); break;
    }
}

/* a new field (PLAY; the glue fades through dark): the reeds, the stars, the hills, the stream, the moon; the
 * fireflies fly on */
SC_SMALL static void ht_regrow(uint32_t seed)
{
    int i;
    ht.seed = seed | 1u;
    ht.mn_made = 0;                                   /* (a new picture: its own moon) */
    ht.moons = 0;
    ht_background();
    for (i = 0; i < HT_MAXF; i++)
        ht_f[i].perch = -1;
}

/* knobs saved by an older sound version: the levels set by ear (Notes 75, Drone 30, Air 30, Frogs 50) and the owl's knob (it was
 * the flute's) to their defaults; the rest as they were */
/* a mood as it was made (the glue keeps the player's tweaks): the defaults, then the mood's own */
SC_SMALL static void ht_mood_kv(int m, int16_t *kv)
{
    ht_defaults(kv);
    m = sc_mood_base(m, HT_NMOOD);
    kv[HK_MOOD] = (int16_t)m;
    sc_mood_set(kv, HT_MOODS[m], 9);
}
static const char *ht_mood_name(int m) { return HT_MOOD_NAME[sc_mood_base(m, HT_NMOOD)]; }

static const scene_t SCENE_HOTARU = {"Hotaru", 3, ht_defaults, ht_enter, ht_leave, ht_background, ht_step, ht_draw, ht_key,
                                     ht_knob, ht_knob_text, ht_regrow, HT_SOUND, 0, 0, HT_NMOOD, ht_mood_kv, ht_mood_name, 1};
