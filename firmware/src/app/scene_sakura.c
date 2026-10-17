/* SPDX-License-Identifier: GPL-3.0-only */
/* Sakura 桜: a cherry tree over a pond. Petals fall from its blossoms, flutter on the wind and land on the water;
 * where one lands a koto note sounds (where across the pond: which note; how fast it came down and how near:
 * how loud), a ripple spreads, and the petal floats off on the current. Two floating petals that touch make a
 * soft note together. Now and then a gust shakes a shower loose and stirs a wind chime (furin; or a run on the harp:
 * DRIFT's Gusts). A soft pad drone holds the harmony (conductor.c) under it all, and a day passes every twelve
 * minutes in the palette.
 *
 *   KNOB 1 Activity (petals, wind, how many notes)   KNOB 2 Time (the world's speed)
 *   KNOB 3 Tone (the koto: warm .. ringing)           KNOB 4 Space (reverb)
 *   MIX page (FX): KNOB 1 Notes (the koto's level)  2 Drone  3 Air  4 Water (the levels)
 *   MORE page (FX again): KNOB 1 Echo (notes come back softer, conductor.c)  2 Water (a recorded brook,
 *   synthesized bubbles, or white noise)  3 Air (a breeze, leaves, night insects (they come with the night),
 *   or a soft synthesized breeze)  4 Heavy
 *   DRIFT page: KNOB 1 Change (the key and scale drift)  2 Tape  3 Gusts (a wind chime, or the harp's run)
 *
 * Activity is how many petals are in the air at once (ambalek's "fall": a population, not a rate), so the
 * texture is steady; a gust shakes loose a shower on top.
 *   White keys  the koto's note at once, and the blossoms above shake: two of their petals land with it again
 *   Black keys  the harp, a note each, low to high (they were gusts: the gusts are the wind's own now)
 *
 * The world is in 240 x 240 coordinates; the drawing scales by r8_fb.w / 240 for pixel art. Every loop has a
 * fixed bound. */
#define SK_MAXP 112
#define SK_MAXR 16
#ifndef SK_CROWN
#define SK_CROWN 0.8f                       /* the crown's size (1: the earlier; 0.8: picked by eye) */
#endif
#define SK_MAXT 384                         /* branch tips: every one blossoms (a full crown has ~250) */
#define SK_WATER 164.0f                     /* the river's far edge: the foot of the far bank's stone wall */
enum { SK_SKY = 0, SK_MTN = 32, SK_BANK = 48, SK_WAT = 64, SK_SHIM = 80, SK_BARK = 88, SK_BLOS = 96, SK_PET = 112, SK_RIP = 120,
       SK_CLOUD = 128, SK_CLOUDF = 136, SK_CLOUDE = 140, SK_VIS = 144, SK_BLG = 152, SK_NITE = 248 };   /* (the
       * night's lights: a lantern, its glow, the lights on the water (warm, pink), a star, the moon; the morning's mist) */
#define SK_NLANT 7   /* (the blossoms'
                                                                                         * groups, 12 x 8) */   /* (the far clouds, the visitors and
                                                                              * the rain; the clouds' soft edges,
                                                                              * where the sunbeams were) */
#define SK_NDROP 48
#define SK_DARK 80
/* only the crown's outermost 30 % is drawn each frame (it sways the most); the rest is painted in (all of it
 * each frame was 17.6 ms a frame, F 27, on the FM-1) */
#define SK_NCLOUD 5                         /* the first two far off (small, slow, pale, low), three nearer */
#define SK_NPUFF 12
enum { PS_FREE, PS_AIR, PS_FLOAT };
static const uint8_t SK_TONE_SLOT[5] = {FM_P_KOTO_WARM, FM_P_KOTO, FM_P_KOTO_II, FM_P_KOTO_PLUCK, FM_P_KOTO_RING};   /* Koto Warm, Koto, Koto II, Koto Pluck, Koto Ring */
static const char *const SK_TONE_NAME[5] = {"Warm", "Koto", "Koto II", "Pluck", "Ring"};

/* the knobs (fewer, Mood first, named by what they do; by number; ui.c AMB_AT puts them on the four pages: Scene,
 * Volume, Nature, Music). Gone: Time (always 100 %), Change (always Slow), Tape */
enum { SKK_MOOD, SKK_ACT, SKK_TONE, SKK_SPACE, SKK_NOTES, SKK_DRONE, SKK_WIND, SKK_STREAM, SKK_AIR, SKK_WATER, SKK_ECHO,
       SKK_DEPTH, SKK_HEAVY, SKK_GUSTS, SKK_MELODY, SKK_ARC };
#define SK_SOUND 5                          /* Sakura's sounds' version (scene_t.sound): 1, defaults set by ear;
                                             * 2, Notes 80, Drone 15, Air 30, Water 30; 3, Arc 30; 4,
                                             * four pages, Natural, Tide 50 */
#define SK_KLO 3                            /* the notes: white keys 3..12 */
#define SK_KN 10
#define SK_NSTREAK 9                        /* light on the river: streaks drifting with the current (fewer) */
static const int16_t SK_KNOB[SC_NKNOB][4] = {   /* lo, hi, step a detent, default; SPACE is the reverb (P_REVERB) */
    {0, 4, 1, 0}, {0, 100, 1, 25}, {0, 4, 1, 1}, {0, 0, 0, 0},   /* Mood (Natural), Activity, Instrument,
                                                                              * Space */
    {0, 100, 1, 80}, {0, 100, 1, 15}, {0, 100, 1, 30}, {0, 100, 1, 30},       /* Volume: Notes, Drone, Air, Water */
    {0, 3, 1, FM_AIR_BREEZE_SYNTH}, {0, FM_WATER_RAIN, 1, FM_WATER_BROOK}, {0, 100, 1, 40}, {0, 100, 1, 0},   /*
                                                                              * Air Type (Soft), Water Type, Echo, Warmth */
    {0, 100, 1, 40}, {0, 1, 1, 1}, {0, 1, 1, 0}, {0, 100, 1, 50},             /* Nature: Clusters, Gust Sound (Harp),
                                                                              * Phrases, Tide (50) */
};
/* the moods (after Avonlea's presets; Natural first, the day going round; the others hold their time;
 * Golden Hour, and Evening named Night). Each its own key and scale: Natural the player's,
 * drifting */
#define SK_NMOOD 5
static const char *const SK_MOOD_NAME[SK_NMOOD] = {"Natural", "Morning", "Afternoon", "Golden", "Night"};
static const float SK_MOOD_DAY[SK_NMOOD] = {-1.0f, 0.06f, 0.30f, 0.45f, 0.62f};   /* the day each holds (0.25 noon,
                                                                                  * 0.5 dusk; -1 flows) */
static const int8_t SK_MOOD_SCALE[SK_NMOOD] = {-1, SC_YO, SC_YO, SC_MINYO, SC_MINYO};   /* (bright by day; warm, wistful
                                                       * from golden hour; Natural follows the light) */
static const float SK_MOOD_KEY[SK_NMOOD] = {0.0f, 0.0f, 0.0f, -500.0f, 0.0f};
static const sc_set_t SK_MOODS[SK_NMOOD][8] = {
    {{SKK_ACT, 25}, {SKK_NOTES, 80}, {SKK_DRONE, 15}, {SKK_WIND, 30}, {SKK_STREAM, 30}, {SKK_ECHO, 40}, {SKK_HEAVY, 40},
     {SKK_DEPTH, 0}},
    {{SKK_ACT, 45}, {SKK_NOTES, 80}, {SKK_DRONE, 12}, {SKK_WIND, 35}, {SKK_STREAM, 35}, {SKK_ECHO, 35}, {SKK_HEAVY, 40},
     {SKK_DEPTH, 10}},
    {{SKK_ACT, 35}, {SKK_NOTES, 85}, {SKK_DRONE, 12}, {SKK_WIND, 30}, {SKK_STREAM, 30}, {SKK_ECHO, 30}, {SKK_HEAVY, 50},
     {SKK_DEPTH, 0}},
    {{SKK_ACT, 20}, {SKK_NOTES, 75}, {SKK_DRONE, 20}, {SKK_WIND, 25}, {SKK_STREAM, 30}, {SKK_ECHO, 45}, {SKK_HEAVY, 35},
     {SKK_DEPTH, 25}},                             /* (Golden Hour: warm, unhurried) */
    {{SKK_ACT, 12}, {SKK_NOTES, 65}, {SKK_DRONE, 30}, {SKK_WIND, 20}, {SKK_STREAM, 25}, {SKK_ECHO, 55}, {SKK_HEAVY, 25},
     {SKK_DEPTH, 40}},
};

typedef struct {
    float x, y, vx, vy, a, va, ph, fr, fall, land, life, cool;
    float sz;                               /* its size and pace (1: from our tree; a far cherry's: smaller, slower) */
    uint8_t st, c;
    int8_t echo;                            /* a key's petal: lands with that key's note (>= 0), or unheard (-2) */
} petal_t;

static petal_t sk_p[SK_MAXP];
static struct {
    float x, y, len, age, life;
} sk_s[SK_NSTREAK];
static struct {
    float x, y, t;
} sk_r[SK_MAXR];
static struct {
    float x, y, px, py;                     /* the tip, and where its twig starts (the blossoms run along it) */
} sk_tip[SK_MAXT];
static uint16_t sk_bcnt[240];               /* blossoms over each column of the world (the shade on the water) */
static uint8_t sk_shade[240];               /* the crown's shadow on the water, by column (0..255): komorebi */
static uint8_t sk_fgx[240];                 /* the trunk and the bank on the water, per row: how far across (screen px) */
static int8_t sk_sin8[256];                 /* a sine, one turn (the water's bands) */
/* the blossoms in twelve groups of eight shades (the earlier rustle, the groups brightening and dimming in turn in the
 * wind; went: a gust made it flicker. The groups stay, all the blossoms' own shades) */
#define SK_NRG 1                            /* (the blossoms' groups are one now: their 88 entries are the river's) */
#define SK_RUST(g, c) (SK_BLG + (c))
/* the far bank and the river (in what the blossoms' groups used): the stone wall (ishigaki), the path along its top,
 * the far cherries' trunks, the reflections in the river (the hills, the bank's grass, the wall, the cherries, their
 * trunks), the stones in the shallows */
#define SK_WALL 160
#define SK_PATH 168
#define SK_FTRUNK 171
#define SK_REF 172
#define SK_SHAL 188
/* the sine and the square root, one copy each (the inline ones were copied at every use: flash) */
__attribute__((noinline)) static float sk_sin(float t) { return fm_sin_turns(t); }
__attribute__((noinline)) static float sk_sqrt(float x) { return fm_sqrtf(x); }
static int sk_wet(uint8_t c) { return (c >= SK_WAT && c < SK_WAT + 16) || (c >= SK_REF && c < SK_REF + 16) || (c >= SK_SHAL && c < SK_SHAL + 4); }
static struct {                             /* a black key's sparkle on the water: where, how long it has */
    float x, y, t;
} sk_spk[4];
#define SK_NSPK 4
static sc_drop_t sk_drop[SK_NDROP];         /* a shower's drops (each on its own: sc_rain_step) */
static struct {                             /* clouds: a few soft puffs each, drifting slowly behind the branch */
    float x, y, v;
    int np;
    float px[SK_NPUFF], py[SK_NPUFF], pr[SK_NPUFF], pp[SK_NPUFF];   /* each puff: where, its size, its breathing */
    float base, top, scale;                 /* its flat base and its top (below the center line), its size */
    int on;                                 /* in the sky (or waiting its turn: the sky clear there for a while) */
    float wait;                             /* seconds until it may come in again */
} sk_c[SK_NCLOUD];
static void sk_cloud_new(int c, float x);
static void sk_cloud_come(int c, float x);
/* the clouds made once per shape (they were most of Sakura's 5.6 ms of copy and clouds on the FM-1): each a
 * stamp (0 the sky), redrawn now and then as its puffs breathe, and set into the frame by whole pixels as it drifts */
#define SK_CSW 124
#define SK_CSH 40
static uint8_t sk_cst[SK_NCLOUD][SK_CSW * SK_CSH];
static int16_t sk_cso[SK_NCLOUD][2];        /* where its corner was on the screen, made */
static float sk_csx[SK_NCLOUD];             /* the cloud's x, made (it has drifted since) */
static uint8_t sk_cs_ok[SK_NCLOUD];         /* made (0: to be made: new, or the scale changed) */
static uint32_t sk_cs_n;                    /* (the frames: one cloud made again every few) */
#define SK_MAXH 2
static struct {                             /* heavy falls: a cluster of blossoms broken off, falling slow and straight */
    float x, y, vx, vy, a, land;
    int on;
} sk_h[SK_MAXH];

static struct {
    uint32_t seed;
    int ntip;
    int16_t *kv;                            /* the knobs (the project's, scene.h): SKK_* */
    float spawn_acc, gust, gust_dir, gust_next, t, day;
    float heavy_next;                       /* seconds to the next heavy fall */
    uint32_t landed, harp_keys, gathered;      /* (gathered: petals lying on our bank) */
    int key_note;                           /* the note the last key played (the host's tests) */
    float raft_next, rain, rain_left, rain_next;
    float ft_next;                          /* the far cherries' next petal */
    float paw_x, paw_y;                     /* a gust's dark patch on the water (a cat's paw), where it is */
    float drops;                            /* how much rain is falling (eased; sc_rain_step) */
    float hold;                             /* the day a mood holds (0..1), or -1: it goes round (Natural) */
    sc_glide_t glide;                       /* the way there */
    float lan[SK_NLANT], lan_t;             /* each lantern's light (0..1); since the lighting (or the dousing) began */
    uint8_t lan_on, lan_want;               /* lit for the night; each one's turn come (bits) */
    float harp_x, harp_y;                   /* where the last harp note's ring landed */
    struct {
        float x, y, vx, ph, base;
        int on;
    } bf;                                   /* a butterfly */
    float bf_next;
    struct {
        float x, y, x0, y0, x1, y1, t, perch, sing;
        int st, tip;                        /* 0 away, 1 flying in, 2 perched, 3 flying off */
    } bird;                                 /* a bush warbler (uguisu) */
    float bird_next;
    uint32_t ms, rafts, butterflies, birds, songs, showers;
} sk;

/* --------------------------------------------------------------- world --- */
static petal_t *sk_spawn(float x, float y)
{
    int i;
    for (i = 0; i < SK_MAXP; i++)
        if (sk_p[i].st == PS_FREE) {
            petal_t *p = &sk_p[i];
            p->st = PS_AIR;
            p->x = x + (sc_frand() - 0.5f) * 10.0f;
            p->y = y + (sc_frand() - 0.5f) * 6.0f;
            p->vx = 0.0f;
            p->vy = 4.0f;
            p->a = sc_frand();
            p->va = (sc_frand() - 0.5f) * 1.2f;
            p->ph = sc_frand();
            p->fr = 0.35f + 0.5f * sc_frand();
            p->fall = 13.0f + 10.0f * sc_frand();
            p->land = SK_WATER + 4.0f + (236.0f - SK_WATER - 4.0f) * sc_frand();   /* anywhere across the stream */
            p->life = 18.0f + 20.0f * sc_frand();
            p->cool = 0.0f;
            p->c = (uint8_t)(SK_PET + (sc_rnd() % 5u));
            p->echo = -1;
            p->sz = 1.0f;
            return p;
        }
    return 0;
}

static void sk_spawn_near(float x, int n, int echo)   /* n petals from the blossoms nearest x (echo >= 0: a key's:
                                                        * the first two land with its note, the rest unheard) */
{
    int i, k;
    for (k = 0; k < n; k++) {
        int best = -1;
        float bd = 1e9f;
        for (i = 0; i < sk.ntip; i++) {
            float d = fm_fabsf(sk_tip[i].x - x) + sc_frand() * 30.0f;
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
        if (best >= 0) {
            petal_t *p = sk_spawn(sk_tip[best].x, sk_tip[best].y);
            if (p && echo >= 0)
                p->echo = (int8_t)(k < 2 ? echo : -2);
        }
    }
}

static void sk_gust(float dir)
{
    int t = sk.ntip ? (int)(sc_rnd() % (uint32_t)sk.ntip) : 0;
    sk.gust = 1.0f;
    sk.gust_dir = dir;
    sk.paw_x = dir > 0.0f ? 30.0f : 230.0f;           /* its patch on the water, from upwind */
    sk.paw_y = SK_WATER + 18.0f + 40.0f * sc_frand();
    sk_spawn_near(sk.ntip ? sk_tip[t].x : 120.0f, 8 + (int)(sc_rnd() % 8u), -1);
    if (sk.kv[SKK_GUSTS])                             /* the harp: a run with the wind */
        cd_run(sk.ntip ? sk_tip[t].x / 240.0f : 0.5f, dir > 0.0f ? 1 : -1, 3 + (int)(sc_rnd() % 3u), 130u,
               0.5f + 0.3f * (float)sk.kv[SKK_ACT] * 0.01f);
    else                                              /* the furin: a strike or two, when the wind finds it */
        cd_chime(sk.ntip ? sk_tip[t].x / 240.0f : 0.5f, 1 + (int)(sc_rnd() % 2u), 0.4f + 0.3f * (float)sk.kv[SKK_ACT] * 0.01f);
}

/* a ripple at x, y, starting after `delay` seconds (a heavy fall: a second ring follows the first) */
static void sk_ripple_at(float x, float y, float delay)
{
    int i, old = 0;
    for (i = 0; i < SK_MAXR; i++) {
        if (sk_r[i].t == 0.0f || sk_r[i].t >= 1.8f) {
            old = i;
            break;
        }
        if (sk_r[i].t > sk_r[old].t)
            old = i;
    }
    sk_r[old].x = x;
    sk_r[old].y = y;
    sk_r[old].t = delay > 0.0f ? -delay : 0.001f;
}
static void sk_ripple(float x, float y) { sk_ripple_at(x, y, 0.0f); }

/* the current, px/s downstream: a third again quicker near the bank, the wind on the water adding a little; 2.2 x what
 * the petals rode before ("appear to be flowing") */
static float sk_wind(float x, float y);
static float sk_current(float near, float x, float y)
{
    return (2.0f + 0.12f * sk_wind(x, y)) * (0.4f + 0.8f * near) * 2.2f;
}

static float sk_wind(float x, float y)        /* px/s across, here and now */
{
    float a = (float)sk.kv[SKK_ACT] * 0.01f, base = 3.0f + 9.0f * a + 6.0f * sk_sin(sk.t * 0.011f);
    return (base + 48.0f * sk.gust * sk.gust_dir) * (0.75f + 0.25f * sk_sin(x * 0.004f + y * 0.006f + sk.t * 0.05f));
}

/* ------------------------------------------------------------- picture --- */
static float sk_k(void) { return (float)r8_fb.w / 240.0f; }

/* a limb drawn as the trunk is (sk_trunk): round, lit from the upper left, the shade worked out by the pixel and
 * dithered, its rim a shade darker; the ends round (the joints). Twigs under 2.5 px: one tone, the trunk's middle */
SC_SMALL static void sk_limb(surf_t *s, float k, float x0, float y0, float w0, float x1, float y1, float w1)
{
    float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, r = fm_maxf(w0, w1) * 0.5f + 1.0f, il2;
    int32_t x, y, xa, xb, ya, yb;
    if (fm_maxf(w0, w1) * k < 2.5f || l2 < 1e-4f) {
        r8_limb(s, x0 * k, y0 * k, w0 * k, x1 * k, y1 * k, w1 * k, (uint8_t)(SK_BARK + 3));
        return;
    }
    il2 = 1.0f / l2;
    xa = (int32_t)((fm_minf(x0, x1) - r) * k), xb = (int32_t)((fm_maxf(x0, x1) + r) * k);
    ya = (int32_t)((fm_minf(y0, y1) - r) * k), yb = (int32_t)((fm_maxf(y0, y1) + r) * k);
    for (y = ya < 0 ? 0 : ya; y <= yb && y < s->h; y++) {
        uint8_t *p = s->px + y * s->w;
        float py = ((float)y + 0.5f) / k;
        for (x = xa < 0 ? 0 : xa; x <= xb && x < s->w; x++) {
            float px = ((float)x + 0.5f) / k, t = fm_clampf(((px - x0) * dx + (py - y0) * dy) * il2, 0.0f, 1.0f),
                  ex = px - (x0 + dx * t), ey = py - (y0 + dy * t), hw = (w0 + (w1 - w0) * t) * 0.5f,
                  d2 = ex * ex + ey * ey, u, sh;
            int f;
            if (d2 >= hw * hw)
                continue;
            u = (ex * 0.6f + ey * 0.8f) / hw;        /* (toward the light -1, away +1) */
            sh = 2.4f + 2.6f * u - 1.8f * fm_maxf(0.0f, 1.0f - fm_fabsf(u + 0.5f) * 3.0f);   /* as the trunk */
            if ((hw - sk_sqrt(d2)) * k < 1.5f)
                sh += 0.8f;
            f = (int)(fm_clampf(sh, 0.0f, 6.99f) * 16.0f);
            p[x] = (uint8_t)(SK_BARK + (f >> 4) + r8_dith(x, y, f & 15));
        }
    }
}

static float sk_flare = 1.0f;                        /* a limb leaving the trunk: wider at its foot (sk_background) */
SC_SMALL static void sk_branch(surf_t *s, float k, float x, float y, float ang, float len, float w, int depth)
{
    float x1 = x + len * sk_sin(ang + 0.25f), y1 = y - len * sk_sin(ang),
          kink = (sc_frand() - 0.5f) * 0.16f * len,   /* a limb is never quite straight: bent once, near its middle */
          mx = (x + x1) * 0.5f + kink * sk_sin(ang), my = (y + y1) * 0.5f + kink * sk_sin(ang + 0.25f);
    int n, i;
    if (y1 > 132.0f && depth < 6)
        return;                                      /* nothing hangs down over the water: the pond stays open */
    sk_limb(s, k, x, y, w * sk_flare, mx, my, w * 0.85f);
    sk_limb(s, k, mx, my, w * 0.85f, x1, y1, w * 0.7f);
    sk_flare = 1.0f;
    {
        int end = depth == 0 || len < 7.0f;
        if ((end || (depth <= 2 && (sc_rnd() & 1u))) && sk.ntip < SK_MAXT) {
            sk_tip[sk.ntip].x = x1;                  /* blossoms at the tips, and on half the twigs before them */
            sk_tip[sk.ntip].y = y1;
            sk_tip[sk.ntip].px = mx;
            sk_tip[sk.ntip].py = my;
            sk.ntip++;
        }
        if (end)
            return;
    }
    n = depth > 4 ? 2 : 2 + (int)(sc_rnd() % 2u);
    for (i = 0; i < n; i++) {
        float spread = 0.05f + 0.07f * sc_frand(), a = ang + (i == 0 ? -spread : i == 1 ? spread : 0.02f);
        a += ((depth <= 3 ? -0.03f : 0.07f) - a) * 0.18f;   /* sakura reach wide and nearly level, the outer limbs
                                                       * sagging a little under their bloom */
        sk_branch(s, k, x1, y1, a, len * (0.72f + 0.16f * sc_frand()), w * 0.66f, depth - 1);
    }
}

/* the blossoms: painted in with the branches (the whole crown bends as one, sk_draw). Somei-Yoshino: sprays of
 * small flowers along the last twigs, sky and twigs showing between them. Each flower's shade (0..7) is how high it
 * sits, in the crown and in its spray: by day the tops lit nearly white and the undersides a cool mauve, at night
 * the same shades lit from below (sk_palette swaps the ramp). A few deep pink centers, and bare twig ends poking out */
SC_SMALL static void sk_blossom_add(float x, float y, float r, int shade, int set)
{
    float k = sk_k();
    int bx = (int)x, i;
    r = fm_maxf(r, 0.75f / k);                       /* (at least a pixel, in pixel art) */
    r8_disc(&r8_bg, x * k, y * k, r * k, (uint8_t)SK_RUST(set, shade < 0 ? 0 : shade > 7 ? 7 : shade));
    for (i = bx - 6; i <= bx + 6; i++)
        if (i >= 0 && i < 240)
            sk_bcnt[i]++;
}
SC_SMALL static void sk_blossoms(void)
{
    float k = sk_k(), top = 999.0f, bot = 0.0f, cx = 0.0f;
    int i, j;
    for (i = 0; i < 240; i++)
        sk_bcnt[i] = 0;
    for (i = 0; i < sk.ntip; i++) {
        top = fm_minf(top, sk_tip[i].y);
        bot = fm_maxf(bot, sk_tip[i].y);
        cx += sk_tip[i].x;
    }
    cx = sk.ntip ? cx / (float)sk.ntip : 120.0f;
    for (i = 0; i < sk.ntip; i++) {                  /* bare twig ends: drawn first, so only those reaching out
                                                       * past the flowers show */
        float dx = sk_tip[i].x - sk_tip[i].px, dy = sk_tip[i].y - sk_tip[i].py, l = sk_sqrt(dx * dx + dy * dy) + 1e-3f,
              e = 4.0f + 4.0f * sc_frand();
        if (sc_frand() < 0.5f)
            r8_limb(&r8_bg, sk_tip[i].x * k, sk_tip[i].y * k, 0.55f * k, (sk_tip[i].x + dx / l * e) * k,
                    (sk_tip[i].y + dy / l * e - 1.0f) * k, 0.35f * k, SK_BARK + 4);
    }
    for (j = 0; j < 3; j++)                          /* the undersides first, the lit tops over them */
        for (i = 0; i < sk.ntip; i++) {
            float tx = sk_tip[i].x, ty = sk_tip[i].y, dx = tx - sk_tip[i].px, dy = ty - sk_tip[i].py,
                  l = sk_sqrt(dx * dx + dy * dy) + 1e-3f, h = (ty - top) / (bot - top + 1.0f);
            int n, m = j == 0 ? 3 : j == 1 ? 3 : 2;
            for (n = 0; n < m; n++) {
                float t = 0.3f + 0.9f * sc_frand(), side = (sc_frand() - 0.5f) * 7.0f,
                      x = sk_tip[i].px + dx * t - dy / l * side, y = sk_tip[i].py + dy * t + dx / l * side,
                      r = (j == 2 ? 1.2f : 1.6f) + 1.9f * sc_frand(), up;
                if (j == 0)
                    y += 2.0f + 2.0f * sc_frand();   /* (the spray's underside) */
                else if (j == 2)
                    y -= 1.5f + 1.5f * sc_frand();   /* (its top, in the light) */
                up = (y - ty) / 6.0f;                /* -1 above its twig .. 1 below */
                sk_blossom_add(x, y, r, (int)(1.0f + 3.2f * h + 2.0f * up + 0.8f * (x - cx) / 60.0f +
                                             (sc_frand() - 0.5f) * 1.4f + 0.5f), (int)((uint32_t)(i * 7 + n) % 3u));
            }
        }
    for (i = 0; i < sk.ntip; i++) {                  /* a deep pink center here and there */
        float dx = sk_tip[i].x - sk_tip[i].px, dy = sk_tip[i].y - sk_tip[i].py;
        if (sc_frand() < 0.25f)
            r8_pset(&r8_bg, (int32_t)((sk_tip[i].px + dx * (0.5f + 0.5f * sc_frand())) * k),
                    (int32_t)((sk_tip[i].py + dy * (0.5f + 0.5f * sc_frand()) + 1.0f) * k), (uint8_t)(SK_BLOS + (sc_rnd() % 3u)));
    }
}

/* the far bank: a raised bank along the stream, a stone wall (ishigaki) from its grassy top down to the water, and
 * cherries along it (as along any city river in Japan in spring), near enough to see as trees: each a trunk and a
 * crown of pale blossom, lit from above, a few sprays hanging over the wall. Hazy with the distance (SK_FAR): misted
 * in the morning, dark at night. Their places are kept: they shed petals of their own */
#define SK_FAR (SK_BLOS + 8)
#define SK_FAR2 192                         /* the second row's cherries: paler, hazier (6) */
#define SK_EVG 198                          /* the evergreens' dark greens (4) */
#define SK_RNG 204                          /* the ranges behind: far (3), middle (4), near and wooded (5), snow */
#define SK_SNOW (SK_RNG + 12)
#define SK_FARL 217                         /* the far blossoms over a bonbori: lit warm at night (3) */
#define SK_FLW 220                          /* our bank's flowers: rapeseed yellow, its deeper yellow, a violet */
#define SK_FARW 223                         /* the far cherries' kinds (5 each): a near-white Somei-yoshino, */
#define SK_FARD 228                         /* a deeper pink yaezakura, a yamazakura (its copper new leaves), */
#define SK_FARY 233
#define SK_WIL 238                          /* and a weeping willow's pale greens (4) */
#define SK_NFT 18
static struct {
    float x, top, bot, w;
} sk_ft[SK_NFT];
static int sk_nft;
static float sk_fb_top(float xw)                   /* the far bank's top (its path), by world x */
{
    float u = xw / 240.0f;
    return 146.0f + 0.8f * sk_sin(u * 1.7f + 0.3f) + 0.5f * sk_sin(u * 5.1f);
}
/* the ranges behind, three, each further one paler and bluer (the air between): ridgelines new with each picture,
 * rounded peaks and saddles; the slopes facing the sun (the left) lighter; a haze low in each valley; the near range
 * wooded; on some pictures snow left on the far peaks */
static float sk_rg_f[3][5];
static float sk_ridge(int r, float xw)
{
    static const float B[3] = {114.0f, 126.0f, 137.0f}, A[3] = {30.0f, 16.0f, 9.0f};
    const float *f = sk_rg_f[r];
    float p1 = 0.5f + 0.5f * sk_sin(xw * f[0] + f[1]);   /* (a few peaks: rounded tops, broad feet) */
    return B[r] - A[r] * (0.6f * p1 * p1 * p1 * f[4] + 0.28f * (0.5f + 0.5f * sk_sin(xw * f[2] + f[3])) +
                          0.025f * sk_sin(xw * f[2] * 2.3f + f[1]) + 0.12f);
}
SC_SMALL static void sk_far_bank(surf_t *s, float k)
{
    int32_t x, y, W = s->w, wy = (int32_t)(SK_WATER * k);
    int r, snow = sc_frand() < 0.5f;
    uint16_t *rg = sk_bcnt;                           /* (a range's ridge by world column, x 16: the blossoms' count is
                                                       * made later, sk_blossoms) */
    for (r = 0; r < 3; r++) {
        static const uint8_t C0[3] = {SK_RNG, SK_RNG + 3, SK_RNG + 7}, NC[3] = {2, 3, 4};
        sk_rg_f[r][0] = (1.3f + 1.6f * sc_frand() + (float)r * 0.9f) / 240.0f;
        sk_rg_f[r][1] = sc_frand();
        sk_rg_f[r][2] = (1.5f + 2.0f * sc_frand() + (float)r) / 240.0f;
        sk_rg_f[r][3] = sc_frand();
        sk_rg_f[r][4] = 0.75f + 0.4f * sc_frand();
        for (x = 0; x < 240; x++)
            rg[x] = (uint16_t)(sk_ridge(r, (float)x + 0.5f) * 16.0f);
        for (x = 0; x < W; x++) {
            float xw = ((float)x + 0.5f) / k, ry = (float)rg[(int)xw < 240 ? (int)xw : 239] * (1.0f / 16.0f);
            int32_t m = (int32_t)(ry * k), t = (int32_t)(sk_fb_top(xw) * k);
            for (y = m < 0 ? 0 : m; y < t && y < wy; y++) {
                float dw = ((float)y + 0.5f) / k - ry;
                int sx = (int)(xw - dw * 0.7f);       /* (the slope's light along a spur slanting down from the
                                                       * ridge: no straight edges) */
                uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
                float sl = sx >= 2 && sx < 238 ? ((float)rg[sx + 2] - (float)rg[sx - 2]) * (1.0f / 16.0f) : 0.0f;
                int c = 1;                            /* (lit, between, in shade: dithered by the slope, no bands) */
                if (sl < -0.15f && r8_dith(x, y, (int)((-sl - 0.15f) * 40.0f)))
                    c = 0;
                else if (sl > 0.15f && r8_dith(x, y, (int)((sl - 0.15f) * 40.0f)))
                    c = 2;
                if (c > NC[r] - 1)
                    c = NC[r] - 1;
                if (dw > 7.0f + 4.0f * (float)r && r8_dith(x, y, (int)fm_minf(15.0f, dw - 7.0f - 4.0f * (float)r)))
                    c = 1;                            /* (the slopes' light only near the ridge) */
                if (r == 2 && c < 3 && ((h >> 11) % 5u) == 0u)
                    c = (h >> 17) & 1u ? 3 : c + 1;   /* (the woods: crowns lit and dark) */
                if (dw > 14.0f - 3.0f * (float)r && r8_dith(x, y, (int)fm_minf(12.0f, (dw - 14.0f + 3.0f * (float)r) * 1.5f)))
                    c = NC[r];                        /* the haze low in the valley */
                if (r == 0 && snow && ry < 96.0f && dw < (96.0f - ry) * 0.5f + 1.5f * (float)((h >> 9) % 3u))
                    s->px[y * W + x] = SK_SNOW;
                else
                    s->px[y * W + x] = (uint8_t)(C0[r] + c);
            }
        }
    }
    for (x = 0; x < W; x++) {
        float xw = ((float)x + 0.5f) / k, yt = sk_fb_top(xw);
        int32_t t = (int32_t)(yt * k);
        for (y = t; y < wy; y++) {
            float yw = ((float)y + 0.5f) / k, dw = yw - yt;
            uint8_t c;
            if (dw < 2.2f)                            /* the grassy top, the path's edge */
                c = (uint8_t)(dw < 0.9f ? SK_BANK + 3 : SK_PATH + 2);
            else {                                    /* the wall: courses of stones, staggered, dark joints, wet at the
                                                       * foot, moss here and there near the top */
                float cy = (dw - 2.2f) / 3.3f, fy = cy - fm_floorf(cy);
                int row = (int)cy;
                float sw = 6.0f + (float)((row * 7) % 3), sx = (xw + (float)(row & 1) * sw * 0.5f) / sw, fx = sx - fm_floorf(sx);
                uint32_t h = (uint32_t)(int32_t)fm_floorf(sx) * 2654435761u ^ (uint32_t)row * 40503u;
                if (fy < 0.2f || fx * sw < 0.85f)
                    c = SK_WALL + 7;
                else {
                    int sh = 1 + (int)((h >> 9) % 4u) + (fy > 0.75f ? 1 : 0) - (row == 0 ? 1 : 0) + (SK_WATER - yw < 2.5f ? 2 : 0);
                    c = (uint8_t)(SK_WALL + (sh < 0 ? 0 : sh > 6 ? 6 : sh));
                    if (row == 0 && ((h >> 3) % 9u) == 0u)
                        c = SK_BANK + 5;              /* moss */
                }
            }
            s->px[y * W + x] = c;
        }
    }
}
/* a far crown: 3..6 clumps side by side, wider than tall, a flatter underside; lit from the left and above (the
 * shaded side and the underside cooler); the sky through it here and there. c0: its ramp (nc colors, light..dark) */
static float sk_lan_x(int i);
SC_SMALL static void sk_ft_crown(surf_t *s, float k, float cx, float cy, float w, float h, uint8_t c0, int nc)
{
    int nl = 3 + (int)(sc_rnd() % 4u), l, j;
    for (l = 0; l < nl; l++) {
        float u = ((float)l + 0.5f) / (float)nl, lx = cx + (u - 0.5f) * w * 0.72f + (sc_frand() - 0.5f) * w * 0.1f,
              ly = cy - h * (0.12f + 0.16f * sc_frand()) * (1.0f - fm_fabsf(u - 0.5f) * 1.4f) +
                   (c0 != SK_EVG ? h * 0.5f * (u - 0.5f) * (u - 0.5f) : 0.0f),   /* (the ends bowed down) */
              rx = w * (0.2f + 0.1f * sc_frand()), ry = h * (0.34f + 0.14f * sc_frand());
        int n = (int)(rx * ry * 0.75f) + 4;
        for (j = 0; j < n; j++) {
            float a = sc_frand(), rr = sk_sqrt(sc_frand()), px = lx + rx * rr * sk_sin(a),
                  py = ly + ry * rr * sk_sin(a + 0.25f), hh, sd;
            {   /* the underside flatter; a cherry's arching down at the ends, the limbs heavy with blossom */
                float d = (px - cx) / (w * 0.5f), lo = 0.3f + (c0 != SK_EVG ? 0.22f * d * d : 0.0f);
                if (py > cy + h * lo)
                    py = cy + h * (lo - 0.05f * sc_frand());
            }
            if (sc_frand() < 0.1f)
                continue;
            hh = fm_clampf((py - (cy - h * 0.5f)) / h, 0.0f, 1.0f);
            sd = fm_clampf((px - cx) / w + 0.5f, 0.0f, 1.0f);
            {
                float e = rr > 0.7f ? (rr - 0.7f) * 3.3f : 0.0f;   /* (the rim darker, the shaded side's most, and
                                                                    * the underside: one tree reads against the next) */
                int c = (int)fm_clampf((hh * 0.65f + sd * 0.35f + e * (px > lx ? 0.3f : 0.1f) + (hh > 0.78f ? 0.15f : 0.0f)) *
                                       (float)(nc - 1) + (sc_frand() - 0.5f) * 1.6f, 0.0f, (float)(nc - 1)),
                    li = (int)fm_clampf((px - 7.0f) / 17.4f + 0.5f, 0.0f, 13.0f);
                if (c0 == SK_FAR && hh > 0.45f && fm_fabsf(px - sk_lan_x(li)) < 6.5f)
                    c = SK_FARL - SK_FAR + (c < 3 ? 0 : c < 5 ? 1 : 2);   /* (over a bonbori: its light at night) */
                r8_disc(s, px * k, py * k, fm_maxf((1.1f + 1.4f * sc_frand()) * k, 0.75f), (uint8_t)(c0 + c));
            }
        }
    }
}
/* a weeping tree: a willow (yanagi, c0 SK_WIL), common on the river's banks, or now and then a weeping cherry
 * (shidarezakura, wider, its curtains of blossom): a short trunk, a small crown, long strands hanging past the bank's
 * top toward the water; its width */
SC_SMALL static float sk_willow(surf_t *s, float k, float x, float yb, uint8_t c0, int nc, float big)
{
    float w = (18.0f + 8.0f * sc_frand()) * big, h = (20.0f + 8.0f * sc_frand()) * (0.6f + 0.4f * big),
          cx = x + (sc_frand() - 0.5f) * 2.0f, cy = yb - 9.0f - h * 0.35f, t;
    int j, n = 16 + (int)(w * 0.8f), sp = c0 == SK_WIL ? nc - 2 : (nc - 1) / 2;   /* (a cherry's strands the lighter
                                                                                 * half of its colors: blossom) */
    r8_limb(s, x * k, yb * k, 1.5f * k, cx * k, (yb - 9.0f) * k, 0.9f * k, SK_FTRUNK);
    sk_ft_crown(s, k, cx, cy, w * 0.8f, h * 0.55f, c0, nc);
    for (j = 0; j < n; j++) {
        float px = cx + (sc_frand() - 0.5f) * w * 0.9f, py = cy - h * 0.2f + sc_frand() * h * 0.3f,
              len = yb + 1.0f + 5.0f * sc_frand() - py, dx = (px - cx) / w * 0.25f;
        for (t = 0.0f; t < len; t += 1.0f)
            r8_disc(s, (px + dx * t) * k, (py + t) * k, fm_maxf((c0 == SK_WIL ? 0.6f : 0.75f) * k, 0.5f),
                    (uint8_t)(c0 + (c0 == SK_WIL) + (int)fm_minf((float)sp, (t / len * 2.5f + sc_frand() * 0.8f) * (float)sp * 0.5f)));
    }
    if (c0 != SK_WIL && sk_nft < SK_NFT) {           /* (a cherry: it sheds petals as the others) */
        sk_ft[sk_nft].x = cx;
        sk_ft[sk_nft].top = cy - h * 0.3f;
        sk_ft[sk_nft].bot = cy + h * 0.3f;
        sk_ft[sk_nft].w = w;
        sk_nft++;
    }
    return w;
}
/* the far bank's trees, each picture planted its own way: now and then a promenade, the crowns merging all along (as
 * on the famous rivers at full bloom, one kind); mostly groups of two to four with the bank and the hills between;
 * sometimes a few alone. One to three of them not cherries: an evergreen (a camphor, a pine) or a willow */
SC_SMALL static void sk_far_trees(surf_t *s, float k)
{
    float st = sc_frand(), x = -14.0f + 8.0f * sc_frand();
    int sty = st < 0.25f ? 0 : st < 0.85f ? 1 : 2, j, nt = 0, ingrp = 0, grp = 2 + (int)(sc_rnd() % 3u),
        nn = 1 + (int)(sc_rnd() % 3u), oth[3], wt = sc_frand() < 0.33f ? 2 + (int)(sc_rnd() % 7u) : -1;
    for (j = 0; j < 3; j++)
        oth[j] = j < nn ? 1 + j * 4 + (int)(sc_rnd() % 4u) : -1;
    sk_nft = 0;
    while (x < 250.0f) {                              /* the second row, further along the bank: smaller, paler, hazier,
                                                       * in clumps */
        int n = 2 + (int)(sc_rnd() % 4u);
        for (j = 0; j < n && x < 250.0f; j++) {
            float w = 12.0f + 12.0f * sc_frand(), h = 6.0f + 5.0f * sc_frand();
            sk_ft_crown(s, k, x, sk_fb_top(x) - 2.0f - h * 0.35f, w, h, SK_FAR2, 6);
            x += w * (0.55f + 0.3f * sc_frand());
        }
        x += (sty == 0 ? 2.0f : 10.0f) + 25.0f * sc_frand();
    }
    x = -10.0f + 10.0f * sc_frand();
    while (x < 252.0f && sk_nft < SK_NFT) {
        float big = sc_frand() < 0.2f ? 0.0f : sc_frand(), w = 16.0f + 32.0f * big,
              h = 11.0f + 21.0f * big * big * (0.7f + 0.3f * sc_frand()) + 4.0f * sc_frand(), yb = sk_fb_top(x) + 0.5f, th = 3.0f + 3.0f * sc_frand(),
              lean = (sc_frand() - 0.5f) * 3.0f, cx = x + lean, cy = yb - th - h * 0.3f, v = sc_frand();
        uint8_t c0 = SK_FAR;
        int n, nc = 7;
        if (nt == oth[0] || nt == oth[1] || nt == oth[2]) {
            if (sc_frand() < 0.45f)
                w = sk_willow(s, k, x, yb, SK_WIL, 4, 1.0f);
            else {                                    /* an evergreen: dark, the blossom stands out against it */
                float eh = 16.0f + 9.0f * sc_frand();
                w = 14.0f + 8.0f * sc_frand();
                r8_limb(s, x * k, yb * k, 1.2f * k, x * k, (yb - 4.0f) * k, 1.0f * k, SK_FTRUNK);
                sk_ft_crown(s, k, x, yb - 3.0f - eh * 0.42f, w, eh, SK_EVG, 4);
            }
            nt++;
            x += w * 0.75f;
            continue;
        }
        if (nt == wt) {                               /* one picture in three: a weeping cherry, pale or deep pink */
            w = sc_frand() < 0.5f ? sk_willow(s, k, x, yb, SK_FAR, 7, 1.35f) : sk_willow(s, k, x, yb, SK_FARD, 5, 1.35f);
            nt++;
            x += w * 0.7f;
            continue;
        }
        if (v < (sty ? 0.2f : 0.06f))                 /* (the kind: a promenade nearly all one) */
            c0 = SK_FARW, nc = 5;
        else if (v < (sty ? 0.34f : 0.09f))
            c0 = SK_FARD, nc = 5;
        else if (v < (sty ? 0.44f : 0.1f))
            c0 = SK_FARY, nc = 5;
        nt++;
        /* the trunk: short and thick, leaning a little, splitting low into limbs that spread */
        r8_limb(s, x * k, yb * k, 1.9f * k, cx * k, (yb - th) * k, 1.3f * k, SK_FTRUNK);
        for (j = 0; j < 3; j++) {
            float ex = cx + ((float)j - 1.0f) * w * (0.18f + 0.08f * sc_frand()), ey = cy + h * (0.05f - 0.1f * sc_frand());
            r8_limb(s, cx * k, (yb - th) * k, 1.0f * k, ex * k, ey * k, 0.45f * k, SK_FTRUNK);
        }
        sk_ft_crown(s, k, cx, cy, w, h, c0, nc);
        for (j = 0; j < 2; j++) {                     /* dark branches glimpsed through the blossom */
            float bx = cx + (sc_frand() - 0.5f) * w * 0.5f, by = cy + h * (0.12f + 0.12f * sc_frand());
            r8_limb(s, bx * k, by * k, 0.5f * k, (bx + (bx - cx) * 0.2f) * k, (by - 1.5f) * k, 0.35f * k, SK_FTRUNK);
        }
        {   /* the crown's shade on the bank's top and the wall's first course */
            int32_t xa = (int32_t)((cx - w * 0.4f) * k), xb = (int32_t)((cx + w * 0.4f) * k), px, py,
                    ya = (int32_t)((yb - 0.5f) * k), yz = (int32_t)((yb + 5.0f) * k);
            for (px = xa < 0 ? 0 : xa; px <= xb && px < s->w; px++)
                for (py = ya; py <= yz && py < s->h; py++) {
                    uint8_t *p = s->px + py * s->w + px;
                    if (*p == SK_BANK + 3 || *p == SK_PATH + 2)
                        *p = SK_BANK + 6;
                    else if (*p >= SK_WALL && *p < SK_WALL + 6)
                        *p = (uint8_t)(*p + 1);
                }
        }
        n = 2 + (int)(w * 0.08f);                     /* a few sprays hanging over the wall toward the water, soft
                                                       * and broken, the longest rare */
        for (j = 0; j < n; j++) {
            float px = cx + (sc_frand() - 0.5f) * w * 0.8f, py = cy + h * 0.3f, r = sc_frand(),
                  len = (SK_WATER - yb) * (0.12f + 0.55f * r * r * r) + 1.5f, dx = (sc_frand() - 0.5f) * 0.4f, t;
            for (t = 0.0f; t < len; t += 1.3f)
                if (t < 1.5f || sc_frand() < 0.7f)
                    r8_disc(s, (px + dx * t + 0.4f * sk_sin(t * 0.15f)) * k, (py + t) * k,
                            fm_maxf((1.5f - 0.7f * t / len) * k, 0.6f),
                            (uint8_t)(c0 + (int)((2.0f + fm_clampf(t / len * 3.0f + sc_frand() * 1.5f, 0.0f, 4.0f)) *
                                                 (float)(nc - 1) * (1.0f / 6.0f))));
        }
        sk_ft[sk_nft].x = cx;
        sk_ft[sk_nft].top = cy - h * 0.5f;
        sk_ft[sk_nft].bot = cy + h * 0.5f;
        sk_ft[sk_nft].w = w;
        sk_nft++;
        if (sty == 0)                                 /* the spacing: the planting's */
            x += w * (0.5f + 0.15f * sc_frand()) + (sc_frand() < 0.08f ? 8.0f + 10.0f * sc_frand() : 0.0f);
        else if (sty == 1) {
            x += w * (0.55f + 0.15f * sc_frand());
            if (++ingrp >= grp) {
                ingrp = 0;
                grp = 2 + (int)(sc_rnd() % 3u);
                x += 16.0f + 26.0f * sc_frand();
            }
        } else
            x += w * (0.75f + 0.2f * sc_frand()) + 14.0f + 24.0f * sc_frand();
    }
}

/* the river's reflections (drawn once, with the background): the far bank mirrored at the waterline (the hills, its
 * grass, the wall, the cherries and their trunks: darker and bluer, SK_REF), fading out toward us (near water shows
 * less of the bank, more of the sky); sk_draw wavers them and the ripples break them. And stones in the shallows
 * along our bank, seen through the water */
static uint8_t sk_ref_of(uint8_t c)
{
    if (c >= SK_RNG && c < SK_RNG + 13)
        return (uint8_t)(SK_REF + (c < SK_RNG + 3 ? 0 : c < SK_RNG + 7 ? 1 : 3));
    if (c >= SK_BANK && c < SK_BANK + 8)
        return (uint8_t)(SK_REF + 4 + (c - SK_BANK) / 4);
    if (c >= SK_WALL && c < SK_WALL + 8)
        return (uint8_t)(SK_REF + 6 + (c - SK_WALL) / 4);
    if (c >= SK_FAR && c < SK_FAR + 8)
        return (uint8_t)(SK_REF + 8 + (c - SK_FAR) / 2);
    if (c >= SK_FARL && c < SK_FARL + 3)
        return (uint8_t)(SK_REF + 9 + (c - SK_FARL));
    if (c >= SK_FAR2 && c < SK_FAR2 + 6)
        return (uint8_t)(SK_REF + 8 + (c - SK_FAR2) / 2);
    if (c >= SK_EVG && c < SK_EVG + 4)
        return (uint8_t)(SK_REF + 5);
    if (c >= SK_FARW && c < SK_FARY + 5)
        return (uint8_t)(SK_REF + 8 + (c - SK_FARW) % 5 * 3 / 4);
    if (c >= SK_WIL && c < SK_WIL + 4)
        return (uint8_t)(SK_REF + 4 + (c - SK_WIL) / 2);
    if (c >= SK_PATH && c <= SK_FTRUNK)
        return (uint8_t)(SK_REF + 12);
    return 0;
}
#define SK_REFROWS 48.0f                    /* how far toward us the reflections reach */
/* the far bank mirrored: drawn before our tree and bank, so the water shows the far bank behind the trunk too (the
 * trunk stands on our side, in front of the river: it hid its stretch of the reflection and cut a wedge in it) */
SC_SMALL static void sk_mirror(surf_t *s, float k)
{
    int32_t x, y, W = s->w, wy = (int32_t)(SK_WATER * k), ye = (int32_t)((SK_WATER + SK_REFROWS) * k);
    for (y = wy; y < ye && y < s->h; y++) {
        float u = (float)(y - wy) / (float)(ye - wy);   /* (whole near the wall, thinning out toward us) */
        int32_t ys = 2 * wy - y - 1, fade = u < 0.45f ? 16 : (int32_t)(16.0f * (1.0f - (u - 0.45f) / 0.55f));
        uint8_t *p = s->px + y * W;
        const uint8_t *q = s->px + (ys < 0 ? 0 : ys) * W;
        for (x = 0; x < W; x++) {                     /* (the whole row: the water behind the trunk too) */
            uint8_t r = sk_ref_of(q[x]);
            if (r && (p[x] >= SK_WAT && p[x] < SK_WAT + 16) && (fade >= 16 || r8_dith(x, y, fade)))
                p[x] = r;
        }
    }
}
SC_SMALL static void sk_reflect(surf_t *s, float k)    /* the shallows: stones along our bank, under the water */
{
    int32_t x, y, W = s->w;
    int i;
    for (i = 0; i < 9; i++) {
        float yw = 222.0f + 16.0f * sc_frand(), rx = 2.5f + 2.5f * sc_frand(), ry = 1.0f + 0.9f * sc_frand(), xw;
        int32_t yy = (int32_t)(yw * k), xa, xb, cy;
        if (yy >= s->h)
            continue;
        xw = (float)sk_fgx[yy < 240 ? yy : 239] / k + 3.0f + 22.0f * sc_frand();
        cy = yy;
        xa = (int32_t)((xw - rx) * k);
        xb = (int32_t)((xw + rx) * k);
        for (y = (int32_t)((yw - ry) * k); y <= (int32_t)((yw + ry) * k) && y < s->h; y++)
            for (x = xa; x <= xb && x < W; x++) {
                float dx = (((float)x + 0.5f) / k - xw) / rx, dy = (((float)y + 0.5f) / k - yw) / ry;
                uint8_t *p = s->px + y * W + x;
                if (x < 0 || dx * dx + dy * dy > 1.0f || !(*p >= SK_WAT && *p < SK_WAT + 16) || !r8_dith(x, y, 9))
                    continue;
                *p = (uint8_t)(SK_SHAL + (y < cy ? 0 : 1) + (dx > 0.3f ? 1 : 0));
            }
    }
}

/* the trunk: one smooth tapered shape on a gently curving line,
 * its edges blended, lit from the upper left (a light side, a shadow side), a few bark lines along it, the roots
 * flaring into the bank */
SC_SMALL static void sk_trunk(surf_t *s, float k)
{
    static const float LINE_U[4] = {-0.52f, -0.12f, 0.27f, 0.62f};
    int32_t y, x, y0 = (int32_t)(138.0f * k), y1 = s->h;
    for (y = y0; y < y1; y++) {
        float yw = ((float)y + 0.5f) / k, cx = 26.0f - (yw - 150.0f) * 0.135f + 1.4f * sk_sin((yw - 138.0f) * 0.005f),
              fl = fm_maxf(0.0f, (yw - 214.0f) / 26.0f), hw = 7.5f + 3.5f * (yw - 138.0f) / 100.0f + 8.0f * fl * fl,
              ihw = 1.0f / hw, dark = fm_maxf(0.0f, (yw - 222.0f) / 18.0f);
        int32_t xa = (int32_t)((cx - hw - 1.0f) * k), xb = (int32_t)((cx + hw + 1.0f) * k);
        uint8_t *p = s->px + y * s->w;
        for (x = xa < 0 ? 0 : xa; x <= xb && x < s->w; x++) {
            float xw = ((float)x + 0.5f) / k, u = (xw - cx) * ihw, edge = (1.0f - fm_fabsf(u)) * hw * k + 0.5f, sh;
            int i, f;
            if (edge < 0.5f)
                continue;                             /* (a clean edge: no stray pixels) */
            if (yw < 154.0f) {                        /* the top gives way to the limbs: over them, thinning out
                                                       * (dithered), so trunk and limb are one wood; elsewhere its
                                                       * shoulders rounded, no flat end */
                float a = (yw - 138.0f) * (1.0f / 16.0f);
                if (p[x] >= SK_BARK && p[x] < SK_BARK + 8) {
                    if (!r8_dith(x, y, (int)(a * 16.0f)))
                        continue;
                } else if (u * u > a * 2.0f)
                    continue;
            }
            sh = 2.4f + 2.6f * u - 1.8f * fm_maxf(0.0f, 1.0f - fm_fabsf(u + 0.5f) * 3.0f) + dark;   /* lit from the left */
            if (edge < 1.5f)
                sh += 0.8f;                           /* (its rim a shade darker: rounded) */
            for (i = 0; i < 4; i++) {                 /* bark lines, wandering a little */
                float lu = LINE_U[i] + 0.05f * sk_sin(yw * 0.045f + (float)i * 0.37f), e = fm_fabsf(u - lu);
                if (e < 0.07f)
                    sh += 1.0f * (1.0f - e * (1.0f / 0.07f));
            }
            {   /* a cherry's lenticels: short pale bands across the trunk, in broken rows */
                float ry = yw * (1.0f / 5.6f) + 0.15f * sk_sin(u * 0.4f + 0.1f);
                int row = (int)ry, seg = (int)((u + 1.0f) * 3.5f + (float)(row % 3) * 0.5f);
                uint32_t hh = (uint32_t)row * 2654435761u ^ (uint32_t)seg * 40503u;
                if (ry - (float)row < 0.2f && (hh >> 13) % 2u == 0u && yw < 226.0f)
                    sh -= 1.3f;
            }
            f = (int)(fm_clampf(sh, 0.0f, 6.99f) * 16.0f);
            p[x] = (uint8_t)(SK_BARK + (f >> 4) + r8_dith(x, y, f & 15));
        }
    }
}

/* the near bank: a grassy bank from the left edge, highest there, sloping down to a narrow strip along the foot of the
 * picture all the way across (framing the river); a dark earth lip at the waterline, grass below it. front: only its
 * rows in front of the trunk's foot (drawn again after the trunk) */
static float sk_bank_top(float xw)
{
    return xw < 86.0f ? 223.0f + 12.0f * fm_powf(fm_clampf(xw / 86.0f, 1e-4f, 1.0f), 1.4f)
                      : 235.0f + 0.6f * sk_sin(xw * 0.012f) + 0.4f * sk_sin(xw * 0.05f);
}
SC_SMALL static void sk_bank(surf_t *s, float k, int front)
{
    int32_t x, y, W = s->w;
    for (x = 0; x < W; x++) {
        float xw = ((float)x + 0.5f) / k, top = sk_bank_top(xw), foot = 233.5f + 1.2f * sk_sin(xw * 0.04f);
        int32_t ya = (int32_t)((front ? fm_maxf(top, foot) : top) * k);
        for (y = ya < 0 ? 0 : ya; y < s->h; y++) {
            float dd = ((float)y + 0.5f) / k - top;
            uint32_t h = ((uint32_t)x * 73856093u) ^ ((uint32_t)y * 19349663u);
            int f;
            if (dd < 1.4f && !front) {
                s->px[y * W + x] = (uint8_t)(SK_BARK + 6);   /* the earth at the waterline */
                continue;
            }
            f = (int)(fm_clampf(3.2f - 0.22f * dd + (float)((h >> 7) % 23u) * 0.06f, 0.0f, 5.99f) * 16.0f);
            s->px[y * W + x] = (uint8_t)(SK_BANK + 1 + (f >> 4) + r8_dith(x, y, f & 15));   /* (darker under the lip) */
        }
    }
}
/* reeds at the waterline (drawn each frame: they sway with the wind), and the petal rafts (hanaikada) */
#define SK_NREED 16
static struct {
    float x, y, h, lean;
} sk_reed[SK_NREED];
#define SK_NRAFT 4
static struct {
    float x, y, n;                          /* where; how many petals (0: none) */
    uint32_t seed;
} sk_raft[SK_NRAFT];
SC_SMALL static void sk_tufts(surf_t *s, float k)       /* grass tufts on the bank, flowers, a few fallen petals */
{
    int i, j;
    for (i = 0; i < 34; i++) {
        float xw = i < 16 ? 2.0f + 80.0f * sc_frand() : i < 28 ? 88.0f + 150.0f * sc_frand() : 2.0f + 70.0f * sc_frand(),
              yw = i < 28 ? sk_bank_top(xw) + 0.8f : 234.0f + 5.0f * sc_frand();
        int n = 2 + (int)(sc_rnd() % 3u);
        for (j = 0; j < n; j++) {
            float bx = xw + (float)j * 1.1f, h = 2.0f + 3.0f * sc_frand(), lean = (sc_frand() - 0.3f) * 1.6f;
            r8_limb(s, bx * k, yw * k, 1.0f * k, (bx + lean) * k, (yw - h) * k, 0.5f * k,
                    (uint8_t)(SK_BANK + ((i + j) & 1 ? 1 : 4)));
        }
    }
    for (i = 0; i < SK_NREED; i++) {                  /* the reeds' places: along the waterline, more by the trunk */
        sk_reed[i].x = i < 9 ? 44.0f + 42.0f * sc_frand() : 90.0f + 148.0f * sc_frand();
        sk_reed[i].y = sk_bank_top(sk_reed[i].x) + 1.0f;
        sk_reed[i].h = 7.0f + 8.0f * sc_frand();
        sk_reed[i].lean = (sc_frand() - 0.4f) * 2.5f;
    }
    for (i = 0; i < 5; i++) {                         /* rapeseed (nanohana): clumps of yellow on green stems */
        float cx = i < 3 ? 42.0f + 40.0f * sc_frand() : 95.0f + 140.0f * sc_frand(), by = sk_bank_top(cx) + 2.5f;
        int n = 4 + (int)(sc_rnd() % 5u);
        for (j = 0; j < n; j++) {
            float x = cx + (sc_frand() - 0.5f) * 9.0f, h = 4.0f + 5.0f * sc_frand(), tx = x + (sc_frand() - 0.4f) * 1.5f;
            r8_limb(s, x * k, by * k, 0.8f * k, tx * k, (by - h) * k, 0.5f * k, SK_BANK + 2);
            r8_disc(s, tx * k, (by - h) * k, fm_maxf(1.3f * k, 0.7f), SK_FLW);
            r8_disc(s, (tx + 0.6f) * k, (by - h + 0.7f) * k, fm_maxf(0.8f * k, 0.5f), SK_FLW + 1);
        }
    }
    for (i = 0; i < 12; i++) {                        /* violets and dandelions in the grass */
        float x = i < 7 ? 3.0f + 75.0f * sc_frand() : 90.0f + 145.0f * sc_frand(),
              y = fm_maxf(sk_bank_top(x) + 2.0f, 0.0f) + (i < 7 ? 8.0f * sc_frand() : 1.5f * sc_frand());
        if (y < 239.0f)
            r8_disc(s, x * k, y * k, fm_maxf(0.8f * k, 0.5f), (uint8_t)(i & 1 ? SK_FLW + 2 : SK_FLW));
    }
    for (i = 0; i < 3; i++) {                         /* fallen petals */
        float xw = 8.0f + 60.0f * sc_frand(), yw = fm_maxf(sk_bank_top(xw) + 3.0f, 230.0f) + 6.0f * sc_frand();
        if (yw < 239.0f)
            r8_quad(s, xw * k, yw * k, 1.6f * k, 0.7f * k, sc_frand(), (uint8_t)(SK_PET + 1 + (int)(sc_rnd() % 3u)));
    }
}
/* the trunk's foot: surface roots gripping the bank, reaching out over it (one toward the water), moss on top */
SC_SMALL static void sk_roots(surf_t *s, float k)
{
    static const float R[5][4] = {{24.0f, 226.0f, 44.0f, 231.5f}, {20.0f, 228.0f, 33.0f, 237.0f}, {8.0f, 227.0f, -1.0f, 232.0f},
                                  {12.0f, 229.0f, 6.0f, 238.5f}, {26.0f, 225.5f, 56.0f, 229.0f}};
    int i;
    for (i = 0; i < 5; i++) {
        float x0 = R[i][0], y0 = R[i][1], x1 = R[i][2] + (sc_frand() - 0.5f) * 4.0f, y1 = R[i][3],
              mx = (x0 + x1) * 0.5f, my = fm_minf(y0, y1) - 1.2f;   /* (arching over the ground) */
        r8_limb(s, x0 * k, y0 * k, 3.2f * k, mx * k, my * k, 2.0f * k, (uint8_t)(SK_BARK + 4));
        r8_limb(s, mx * k, my * k, 2.0f * k, x1 * k, y1 * k, 0.7f * k, (uint8_t)(SK_BARK + 5));
        r8_limb(s, x0 * k, (y0 - 1.2f) * k, 1.0f * k, mx * k, (my - 1.2f) * k, 0.6f * k, (uint8_t)(SK_BARK + 2));   /* (lit) */
        if (i < 4)
            r8_disc(s, mx * k, (my - 1.0f) * k, fm_maxf(1.0f * k, 0.6f), SK_BANK + 1);   /* moss */
    }
}
/* stones at the waterline: mossy, a couple stepping out into the shallows, a wet dark foot (after sk_fgx: on the
 * water, not of the bank) */
SC_SMALL static void sk_stones(surf_t *s, float k)
{
    int i;
    for (i = 0; i < 7; i++) {
        float x = i < 2 ? 50.0f + 30.0f * sc_frand() : i < 5 ? 58.0f + 34.0f * sc_frand() : 100.0f + 130.0f * sc_frand(),
              out = i < 2 ? 4.0f + 4.0f * sc_frand() : 0.5f, y = sk_bank_top(x) - out, rx = 2.2f + 2.4f * sc_frand(),
              ry = rx * (0.45f + 0.15f * sc_frand());
        int32_t xa = (int32_t)((x - rx) * k), xb = (int32_t)((x + rx) * k), ya = (int32_t)((y - ry) * k),
                yb = (int32_t)((y + ry * 0.6f) * k), px, py;
        for (py = ya < 0 ? 0 : ya; py <= yb && py < s->h; py++)
            for (px = xa < 0 ? 0 : xa; px <= xb && px < s->w; px++) {
                float dx = (((float)px + 0.5f) / k - x) / rx, dy = (((float)py + 0.5f) / k - y) / ry;
                int c;
                if (dx * dx + dy * dy > 1.0f)
                    continue;
                c = dy < -0.55f && dx < 0.4f ? (dx < -0.2f ? -1 : 1) : 2 + (int)((dx + 1.0f) * 1.5f) + (dy > 0.3f ? 2 : 0);
                s->px[py * s->w + px] = c < 0 ? SK_BANK + 2 : (uint8_t)(SK_WALL + (c > 7 ? 7 : c));   /* (moss on top) */
            }
        if (out > 1.0f) {                             /* in the water: a pale line where the current meets it */
            int32_t wy2 = (int32_t)((y + ry * 0.6f) * k);
            for (px = xa; px <= xb + 1 && wy2 < s->h; px++)
                if (px >= 0 && px < s->w && sk_wet(s->px[wy2 * s->w + px]))
                    s->px[wy2 * s->w + px] = SK_SHIM + 5;
        }
    }
}

SC_SMALL static void sk_background(void)
{
    memset(sk_cs_ok, 0, sizeof sk_cs_ok);       /* (the clouds made again: the scale may have changed) */
    surf_t *s = &r8_bg;
    float k = sk_k();
    int32_t x, y, W = s->w, wy = (int32_t)(SK_WATER * k);
    sc_seed(sk.seed);                                /* the same tree in either style */
    sk.gathered = 0;                                  /* (the petals that lay on our bank: swept with the picture) */
    r8_vgrad(s, 0, wy, SK_SKY, 32);
    sk_far_bank(s, k);                                /* the hills, the far bank and its wall */
    sk_far_trees(s, k);
    r8_vgrad(s, wy, s->h, SK_WAT, 16);               /* (the light on it: drawn each frame, sk_draw) */
    sk_mirror(s, k);                                  /* (the far bank in the water, before anything of ours) */
    sk.ntip = 0;
    sk_flare = 1.35f;
    sk_branch(s, k, 26.0f, 150.0f, 0.17f, 46.0f * SK_CROWN, 15.0f, 6);   /* the crown, leaning out over the pond */
    sk_flare = 1.35f;
    sk_branch(s, k, 26.0f, 140.0f, 0.29f, 26.0f * SK_CROWN, 8.0f, 4);    /* and one up behind */
    sk_bank(s, k, 0);                                 /* the near bank, the trunk standing on it, grass at its foot */
    sk_trunk(s, k);
    sk_bank(s, k, 1);
    sk_roots(s, k);
    sk_tufts(s, k);
    sk_blossoms();                                    /* (painted in, with the branches) */
    for (y = 0; y < s->h && y < 240; y++) {           /* the trunk and the bank on the water, by row */
        const uint8_t *p = s->px + y * W;
        int32_t b = 0;
        if (y >= wy && y < s->h)
            for (x = 0; x < W; x++)               /* (the strip along the foot: across; the reflection is water) */
                if (!sk_wet(p[x]))
                    b = x + 1;
        sk_fgx[y] = (uint8_t)b;
    }
    {   /* the crown by column: its shade on the water (a pool under it, shifted a little right: the sun high on the
         * left) */
        int i;
        for (i = 0; i < 240; i++) {
            int sx = i - 14 < 0 ? 0 : i - 14;
            sk_shade[i] = (uint8_t)(sk_bcnt[sx] * 9 > 255 ? 255 : sk_bcnt[sx] * 9);
        }
    }
    sk_reflect(s, k);                                 /* (the stones: after the bank's edge is known, sk_fgx) */
    sk_stones(s, k);
    sc_seed(sk.seed * 2654435761u + 1u);              /* the world's randomness, apart from the picture's */
}

/* how far into the night: 0 by day .. 1 dark (the lights on) */
static float sk_night(void) { return fm_clampf(-sk_sin(sk.day) * 2.5f + 0.1f, 0.0f, 1.0f); }

/* the lanterns: each its own flame and glow colors (the hills use 8 of
 * their 16, the bank 8 of its), so each brightens and dims on its own, smoothly: lit one after another along the bank
 * as the dark comes, put out the other way round at dawn */
#define SK_LANF (SK_MTN + 8)                /* the flames, one each */
#define SK_LANG (SK_BANK + 8)               /* their glows */
#define SK_STAR2 (SK_MTN + 15)              /* the fainter stars (the brighter: SK_NITE + 4) */
#define SK_NBON (SK_NLANT * 2)              /* bonbori along the whole bank, lit in pairs (a pair: one color) */
static float sk_lan_x(int i) { return 7.0f + (float)i * 17.4f + 2.0f * (float)((i * 7) % 3); }
static float sk_lan_y(int i) { return sk_fb_top(sk_lan_x(i)) - 3.5f; }   /* (its paper: along the far bank's path) */
/* a part of a bonbori, a world rect: behind our tree (not over it), in front of the far cherries */
static void sk_bon_rect(surf_t *s, float k, float x0, float y0, float x1, float y1, uint8_t c)
{
    int32_t xa = (int32_t)(x0 * k), xb = (int32_t)(x1 * k - 0.01f), ya = (int32_t)(y0 * k), yb = (int32_t)(y1 * k - 0.01f), x, y;
    for (y = ya < 0 ? 0 : ya; y <= yb && y < s->h; y++)
        for (x = xa < 0 ? 0 : xa; x <= xb && x < s->w; x++) {
            uint8_t *p = s->px + y * s->w + x;
            if (!((*p >= SK_BARK && *p < SK_CLOUD && (*p < SK_FAR || *p >= SK_FAR + 8)) || (*p >= SK_BLG && *p < SK_BLG + 8)))
                *p = c;
        }
}
static void sk_lanterns_step(float dt)
{
    float lit = sk_night();
    int on = sk.lan_on ? lit > 0.2f : lit > 0.45f, i;
    if (on != sk.lan_on) {
        sk.lan_on = (uint8_t)on;
        sk.lan_t = 0.0f;
    }
    sk.lan_t += dt;
    for (i = 0; i < SK_NLANT; i++) {
        float d = 1.4f * (float)(on ? i : SK_NLANT - 1 - i), want;
        if (sk.lan_t >= d)                            /* (its turn) */
            sk.lan_want = (uint8_t)(on ? sk.lan_want | 1u << i : sk.lan_want & ~(1u << i));
        want = (sk.lan_want >> i & 1u) ? 1.0f : 0.0f;
        sk.lan[i] += fm_clampf(want - sk.lan[i], -dt * 0.5f, dt * 0.5f);   /* (over 2 s) */
    }
}
SC_SMALL static void sk_lanterns_at(void)                  /* (a scene starting: as the time is) */
{
    int i;
    sk.lan_on = (uint8_t)(sk_night() > 0.45f);
    sk.lan_want = sk.lan_on ? (uint8_t)((1u << SK_NLANT) - 1u) : 0u;
    sk.lan_t = 99.0f;
    for (i = 0; i < SK_NLANT; i++)
        sk.lan[i] = sk.lan_on ? 1.0f : 0.0f;
}

/* the times of day drawn (the mist is the colors themselves, in the palette): the lanterns on the
 * far bank (unlit by day), at night their light and the lit crown's on the river in short solid streaks, stars, a
 * moon, all brightening with the dark. Only over sky, hills, bank and water: the tree stays in front */
static void sk_times_draw(surf_t *s, float k)
{
    float lit = sk_night();
    int32_t W = s->w, x, y, i;
    if (lit > 0.02f) {                                /* stars and a moon, only on the sky: their colors fade in */
        static const uint8_t SX[24] = {12, 30, 47, 61, 78, 96, 108, 121, 135, 147, 160, 171, 184, 196, 209, 221, 233, 140,
                                       168, 190, 214, 104, 226, 152};
        static const uint8_t SY[24] = {18, 9, 26, 14, 33, 8, 22, 40, 15, 31, 6, 44, 19, 36, 11, 28, 47, 58, 62, 54, 70, 66,
                                       82, 76};
        int32_t mx = (int32_t)(206.0f * k), my = (int32_t)(30.0f * k), mr = (int32_t)(6.0f * k);
        for (i = 0; i < 24; i++) {
            int32_t sx = (int32_t)((float)SX[i] * k), sy = (int32_t)((float)SY[i] * k);
            uint8_t *p = s->px + sy * W + sx;
            if (*p < SK_MTN)
                *p = (uint8_t)(i % 3 == 0 ? SK_NITE + 4 : SK_STAR2);
        }
        for (y = my - mr; y <= my + mr; y++)
            for (x = mx - mr; x <= mx + mr; x++) {
                uint8_t *p = s->px + y * W + x;
                if ((x - mx) * (x - mx) + (y - my) * (y - my) <= mr * mr && *p < SK_MTN)
                    *p = SK_NITE + 5;
            }
    }
    for (i = 0; i < SK_NBON; i++) {                   /* the bonbori (paper lanterns on posts), and their light on the
                                                       * river; by day cream paper under a dark cap */
        float lx = sk_lan_x(i), ly = sk_lan_y(i), b = sk.lan[i / 2];
        int32_t cx = (int32_t)(lx * k), cy = (int32_t)(ly * k), j;
        if (b > 0.02f)                                /* its glow round it */
            for (y = cy - 3; y <= cy + 3; y++)
                for (x = cx - 3; x <= cx + 3; x++) {
                    uint8_t *p;
                    int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                    if (x < 0 || y < 0 || x >= W || y >= s->h || d > 10)
                        continue;
                    p = s->px + y * W + x;
                    if (!((*p >= SK_BARK && *p < SK_CLOUD && (*p < SK_FAR || *p >= SK_FAR + 8)) || (*p >= SK_BLG && *p < SK_BLG + 8)))
                        *p = (uint8_t)(SK_LANG + i / 2);
                }
        sk_bon_rect(s, k, lx - 0.5f, ly + 1.5f, lx + 0.5f, ly + 4.0f, SK_WALL + 5);         /* the post (weathered wood) */
        sk_bon_rect(s, k, lx - 1.0f, ly - 1.5f, lx + 1.0f, ly + 1.5f, (uint8_t)(SK_LANF + i / 2));   /* the paper */
        sk_bon_rect(s, k, lx - 1.5f, ly - 2.5f, lx + 1.5f, ly - 1.5f, SK_WALL + 6);         /* the cap */
        for (j = 0; j < 14 && b > 0.05f; j++) {        /* its light on the water: short solid streaks toward us,
                                                       * wavering, growing as it brightens */
            float wy = SK_WATER + 2.0f + (float)j * 2.4f,
                  wx = lx + 1.5f * sk_sin(wy * 0.09f + sk.t * 0.6f + (float)i),
                  hl = b * (2.8f - (float)j * 0.13f) * (0.6f + 0.4f * sk_sin(wy * 0.21f + sk.t * 0.45f + (float)i * 0.7f));
            int32_t py = (int32_t)(wy * k), xa = (int32_t)((wx - hl) * k + 0.5f), xb = (int32_t)((wx + hl) * k - 0.5f);
            uint8_t *p = s->px + py * W;
            if (py < 0 || py >= s->h || xb < xa)
                continue;
            for (x = xa < 0 ? 0 : xa; x <= xb && x < W; x++)
                if (sk_wet(p[x]))
                    p[x] = SK_NITE + 2;
        }
    }
    if (lit > 0.05f)                                  /* the lit crown on the water: soft pink streaks under it */
        for (i = 0; i < 12; i++) {
            float wy = SK_WATER + 2.0f + (float)i * 2.7f, d = 1.0f - (float)i / 12.0f;
            int32_t py = (int32_t)(wy * k), n;
            uint8_t *p = s->px + py * W;
            if (py >= s->h)
                break;
            for (n = 0; n < 3; n++) {
                float u = (float)((i * 37 + n * 61) % 97) / 97.0f,
                      wx = 78.0f + 90.0f * u + 2.0f * sk_sin(wy * 0.13f + sk.t * 0.35f + (float)n),
                      hl = lit * d * (1.5f + 2.5f * (0.5f + 0.5f * sk_sin(sk.t * 0.2f + u * 3.0f)));
                int32_t xa = (int32_t)((wx - hl) * k + 0.5f), xb = (int32_t)((wx + hl) * k - 0.5f);
                for (x = xa < 0 ? 0 : xa; x <= xb && x < W; x++)
                    if (sk_wet(p[x]))
                        p[x] = SK_NITE + 3;
            }
        }
}

static void sk_palette(void)
{
    /* saturated on purpose: the FM-1's panel softens color a good deal */
    r8_pal_ramp(SK_SKY, 32, 0x5A8ED8u, 0xF2DCCCu);   /* spring's haze (haru-gasumi): a softer blue, paler low down */
    {   /* the ranges: far (lit, shade, haze), middle (lit, between, shade, haze), near and wooded (lit, between, shade,
         * the woods' dark, haze), the snow */
        static const uint32_t RNG[13] = {0xAEBCDAu, 0x9EAED0u, 0xC6D0E2u, 0x8A9EC8u, 0x7C92BEu, 0x6E84B0u, 0xA2B2D2u,
                                         0x6E9088u, 0x5E7E7Au, 0x506E6Cu, 0x42605Eu, 0x8EA6B0u, 0xEEF2F8u};
        int i;
        for (i = 0; i < 13; i++)
            r8_pal_set(SK_RNG + i, RNG[i]);
    }
    r8_pal_ramp(SK_BANK, 8, 0x5DA243u, 0x24561Fu);
    r8_pal_ramp(SK_WAT, 16, 0xA4CCD8u, 0x2C6874u);   /* a Japanese river in spring: jade, the sky in it far off */
    {
        static const uint32_t WALL[8] = {0xC9C2B4u, 0xB1AA9Cu, 0xA39C90u, 0x958E84u, 0x878078u, 0x6F6A64u, 0x5E5A56u,
                                         0x3E3A38u};
        static const uint32_t REF[16] = {0x5E7C94u, 0x58748Cu, 0x526C84u, 0x4C647Au, 0x3E6A56u, 0x365E4Cu, 0x6C7676u,
                                         0x58625Fu, 0xD0B4C2u, 0xC2A6B6u, 0xB498AAu, 0xA68A9Eu, 0x4A4448u, 0x3A5A62u,
                                         0x3A5A62u, 0x3A5A62u};
        int i;
        for (i = 0; i < 8; i++)
            r8_pal_set(SK_WALL + i, WALL[i]);
        for (i = 0; i < 16; i++)
            r8_pal_set(SK_REF + i, REF[i]);
        r8_pal_set(SK_PATH, 0xB8AC92u);
        r8_pal_set(SK_PATH + 1, 0xA89C84u);
        r8_pal_set(SK_PATH + 2, 0x7E9A5Eu);
        r8_pal_set(SK_FTRUNK, 0x4A3E44u);
        r8_pal_ramp(SK_SHAL, 4, 0x6E8A6Au, 0x3E5A44u);
    }
    r8_pal_ramp(SK_BARK, 8, 0x8C7C7Eu, 0x261D22u);   /* (a lighter top, the trunk's lit side): a cherry's gray,
                                                       * purplish brown */
    r8_pal_ramp(SK_BLOS, 8, 0xF4A2BEu, 0xE07CA0u);   /* the flowers' deep pink centers */
    r8_pal_ramp(SK_FAR, 8, 0xF4DEE4u, 0xA896B0u);    /* the far bank's cherries, hazy with distance: the lit side
                                                       * nearly white, the shade a cool mauve */
    r8_pal_ramp(SK_FAR2, 6, 0xE8DCE6u, 0xB8B0C6u);   /* the row behind: paler, bluer */
    r8_pal_ramp(SK_EVG, 4, 0x58764Eu, 0x22382Au);    /* a camphor's, a pine's dark green */
    r8_pal_ramp(SK_FARW, 5, 0xFAF2F2u, 0xA8A0B4u);   /* the kinds: near white, */
    r8_pal_ramp(SK_FARD, 5, 0xF0CCDAu, 0x9C7E98u);   /* deeper pink, */
    r8_pal_set(SK_FARY, 0xF4E4E4u);                   /* pale pink, the copper leaves only in the shade, */
    r8_pal_set(SK_FARY + 1, 0xEAD4D6u);
    r8_pal_set(SK_FARY + 2, 0xD8BEC2u);
    r8_pal_set(SK_FARY + 3, 0xB08070u);
    r8_pal_set(SK_FARY + 4, 0x84605Au);
    r8_pal_ramp(SK_WIL, 4, 0xC6D4A2u, 0x5E7650u);    /* a willow's spring green */
    r8_pal_set(SK_FARL, r8_base[SK_FAR + 2]);         /* (the far blossoms over the bonbori: by day as the rest) */
    r8_pal_set(SK_FARL + 1, r8_base[SK_FAR + 4]);
    r8_pal_set(SK_FARL + 2, r8_base[SK_FAR + 6]);
    r8_pal_set(SK_FLW, 0xF2D43Au);                    /* rapeseed (nanohana), its shade; violets */
    r8_pal_set(SK_FLW + 1, 0xC8A426u);
    r8_pal_set(SK_FLW + 2, 0x8C6CC4u);
    {   /* the blossoms: by day lit from above (0 the tops, nearly white .. 7 the undersides, a cool mauve: a
         * Somei-Yoshino is almost white, its pink in the shade and the centers), at night lit from below (yozakura:
         * the lanterns under the tree), the one blended into the other with the dark */
        static const uint32_t DAY[8] = {0xFFF9FBu, 0xFFEEF3u, 0xFCDDE7u, 0xF8CAD9u, 0xF1B6CBu, 0xE2A4BEu, 0xC596B4u,
                                        0xA488A8u};
        static const uint32_t NITE[8] = {0x8A6E92u, 0xA284A6u, 0xBC9CB8u, 0xD4B4C6u, 0xE8C8D2u, 0xF6DAD8u, 0xFFE8E0u,
                                         0xFFF2EAu};
        float lit = sk_night();
        int g, c;
        for (g = 0; g < SK_NRG; g++)
            for (c = 0; c < 8; c++)
                r8_pal_set(SK_RUST(g, c), r8_mix(DAY[c], NITE[c], lit));
    }
    r8_pal_ramp(SK_PET, 8, 0xFFF4F7u, 0xE7A2BBu);     /* petals: nearly white, a blush of pink */
    r8_pal_ramp(SK_RIP, 8, 0xF0F7FCu, 0x6F95B5u);
    {   /* the clouds: white tops to a blue-gray base by day; warm tops and a peach-gray base at golden hour; soft and
         * hazy in the morning; dim and moonlit gray at night; grayer in a shower. Far ones paler, nearer the sky's color.
         * (The edges' colors: SK_CLOUDE, the sky's.) */
        float lit = sk_night(), gold = fm_clampf(1.0f - fm_fabsf(sk.day - 0.45f) / 0.08f, 0.0f, 1.0f),
              mist = fm_clampf(1.0f - fm_fabsf(sk.day - 0.06f) / 0.08f, 0.0f, 1.0f);
        uint32_t top = r8_mix(0xFFFFFFu, 0xFFE2BEu, gold), base = r8_mix(0x8C9BB6u, 0xA8909Eu, gold),
                 sky = r8_mix(0x3E78D0u, 0xF5D2B4u, 0.55f);
        top = r8_mix(top, 0xEFF0F4u, 0.6f * mist);
        base = r8_mix(base, 0xB8C0CEu, 0.7f * mist);
        top = r8_mix(top, 0xA8B0C6u, lit);
        base = r8_mix(base, 0x505A74u, lit);
        top = r8_mix(top, 0xD8DCE2u, sk.rain);
        base = r8_mix(base, 0x6E7686u, sk.rain);
        r8_pal_ramp(SK_CLOUD, 8, top, base);
        r8_pal_ramp(SK_CLOUDF, 4, r8_mix(top, sky, 0.4f), r8_mix(base, sky, 0.55f));
        r8_pal_ramp(SK_CLOUDE, 4, r8_mix(top, sky, 0.3f), r8_mix(top, sky, 0.85f));
    }
    r8_pal_ramp(SK_SHIM, 8, 0x6E9FC8u, 0xEEF7FDu);
    r8_pal_set(SK_NITE, 0xFFC878u);                   /* a lantern's flame, its glow; the lights on the water, warm and
                                                       * pink; a star; the moon */
    r8_pal_set(SK_NITE + 1, 0xE0904Cu);
    r8_pal_set(SK_NITE + 2, 0xC88A5Cu);
    r8_pal_set(SK_NITE + 3, 0xC68AA8u);
    r8_pal_set(SK_VIS, 0xFFFCF0u);                    /* a butterfly: white, yellow, its dark wing tips */
    r8_pal_set(SK_VIS + 1, 0xF6E08Au);
    r8_pal_set(SK_VIS + 2, 0x5A4A3Au);
    r8_pal_set(SK_VIS + 3, 0x7E8C3Eu);                /* the bush warbler: olive, darker olive, its pale belly */
    r8_pal_set(SK_VIS + 4, 0x56602Au);
    r8_pal_set(SK_VIS + 5, 0xCFCDA0u);
    r8_pal_set(SK_VIS + 6, 0xD6E2F0u);                /* rain: light, dim */
    r8_pal_set(SK_VIS + 7, 0xA8B8CCu);   /* light on the water, faint .. bright (a streak fades
                                                      * through these: they blinked in place before) */
    {   /* the day: sk.day 0..1, 0 dawn, 0.25 noon, 0.5 dusk, 0.75 midnight */
        float sun = sk_sin(sk.day), glow = 1.0f - fm_fabsf(sun);
        uint32_t m = sun >= 0.0f ? 0xFFFFFFu
                                 : ((uint32_t)(255.0f + 170.0f * sun) << 16 | (uint32_t)(255.0f + 150.0f * sun) << 8 |
                                    (uint32_t)(255.0f + 70.0f * sun));
        uint32_t wash = sk.rain > 0.01f ? r8_mix(0xFFA066u, 0x8C96A8u, fm_minf(1.0f, sk.rain * 2.0f)) : 0xFFA066u;
        int amt = (int)(sk.rain > 0.01f ? fm_maxf(70.0f * glow * glow, 100.0f * sk.rain) : 70.0f * glow * glow), i;
        float lit = sk_night(), gold = fm_clampf(1.0f - fm_fabsf(sk.day - 0.45f) / 0.07f, 0.0f, 1.0f),
              mist = fm_clampf(1.0f - fm_fabsf(sk.day - 0.06f) / 0.07f, 0.0f, 1.0f) * (1.0f - sk.rain);
        r8_pal_commit(m, wash, amt);              /* (a shower: the light grayed) */
        r8_pal_commit_range(SK_CLOUD, 16, r8_mix(m, 0xFFFFFFu, 0.55f * sk_night()), wash, amt / 3);   /* (the clouds' own
                                                       * colors carry the time: no dusty tint, not night-black) */
        if (mist > 0.01f) {                       /* the morning's mist (the colors themselves, no dots): the
                                                   * far hills, the far bank and the far river paler, blue-gray */
            uint32_t mw = r8_mix(wash, 0xC6CED8u, mist);
            r8_pal_commit_range(SK_RNG, 13, m, mw, (int)fm_maxf((float)amt, 135.0f * mist));
            r8_pal_commit_range(SK_BANK, 8, m, mw, (int)fm_maxf((float)amt, 150.0f * mist));
            r8_pal_commit_range(SK_WAT, 6, m, mw, (int)fm_maxf((float)amt, 90.0f * mist));
            r8_pal_commit_range(SK_FAR, 8, m, mw, (int)fm_maxf((float)amt, 150.0f * mist));
            r8_pal_commit_range(SK_FAR2, 10, m, mw, (int)fm_maxf((float)amt, 175.0f * mist));
            r8_pal_commit_range(SK_FARL, 3, m, mw, (int)fm_maxf((float)amt, 150.0f * mist));
            r8_pal_commit_range(SK_FARW, 19, m, mw, (int)fm_maxf((float)amt, 150.0f * mist));
            r8_pal_commit_range(SK_WALL, 28, m, mw, (int)fm_maxf((float)amt, 130.0f * mist));   /* (the wall, the
                                                       * path, the far trunks and the reflections) */
        }
        if (lit > 0.01f || gold > 0.01f) {        /* golden hour, the blossoms warm orange; at night the tree lit up
                                                   * (yozakura: the crown pale pink-white against the dark): the two
                                                   * washes blended, nothing switched */
            uint32_t mm = r8_mix(m, 0xFFFFFFu, 0.85f * lit), w = r8_mix(0xFFA050u, 0xFFE0D0u, lit / (lit + gold));
            int a = (int)(fm_maxf(60.0f * gold, 25.0f * lit));   /* (the night's look is in the ramp itself) */
            r8_pal_commit_range(SK_BLOS, 8, mm, w, a);
            r8_pal_commit_range(SK_BLG, SK_NRG * 8, mm, w, a);
            r8_pal_commit_range(SK_PET, 8, mm, w, (int)fm_maxf(60.0f * gold, 70.0f * lit));
        }
        {   /* the far blossoms over the bonbori: by day as the rest, at night lit warm from below as they light */
            float lb = 0.0f;
            for (i = 0; i < SK_NLANT; i++)
                lb += sk.lan[i] * (1.0f / (float)SK_NLANT);
            for (i = 0; i < 3 && lb > 0.002f; i++) {
                r8_pal_set(SK_FARL + i, r8_mix(r8_base[SK_FAR + 2 + i * 2], 0xF6C8C4u, 0.5f * lb));
                r8_pal_commit_range(SK_FARL + i, 1, r8_mix(m, 0xFFFFFFu, 0.4f * lb), wash, (int)((float)amt * (1.0f - 0.5f * lb)));
            }
        }
        for (i = 0; i < SK_NLANT; i++) {          /* each pair of bonbori: cream paper, to its flame */
            float b = sk.lan[i];
            r8_pal_set(SK_LANF + i, r8_mix(0xE6DCC4u, 0xFFC878u, b));   /* (cream paper by day) */
            r8_pal_commit_range(SK_LANF + i, 1, r8_mix(m, 0xFFFFFFu, b), wash, (int)((float)amt * (1.0f - b)));
            r8_pal_set(SK_LANG + i, r8_mix(r8_base[SK_BANK + 3], 0xD08848u, 0.8f * b));
            r8_pal_commit_range(SK_LANG + i, 1, r8_mix(m, 0xFFFFFFu, b), wash, (int)((float)amt * (1.0f - b)));
        }
        {   /* the stars (the brighter first) and the moon, from the sky's color to their own as it darkens */
            float f1 = fm_clampf((lit - 0.1f) / 0.4f, 0.0f, 1.0f), f2 = fm_clampf((lit - 0.45f) / 0.45f, 0.0f, 1.0f),
                  fm = fm_clampf((lit - 0.15f) / 0.6f, 0.0f, 1.0f);
            uint32_t sky = r8_base[SK_SKY + 4];
            r8_pal_set(SK_NITE + 4, r8_mix(sky, 0xE8ECF8u, f1));
            r8_pal_commit_range(SK_NITE + 4, 1, r8_mix(m, 0xFFFFFFu, f1), wash, (int)((float)amt * (1.0f - f1)));
            r8_pal_set(SK_STAR2, r8_mix(sky, 0xB8C0D8u, f2));
            r8_pal_commit_range(SK_STAR2, 1, r8_mix(m, 0xFFFFFFu, f2), wash, (int)((float)amt * (1.0f - f2)));
            r8_pal_set(SK_NITE + 5, r8_mix(sky, 0xF2EEDCu, fm));
            r8_pal_commit_range(SK_NITE + 5, 1, r8_mix(m, 0xFFFFFFu, fm), wash, (int)((float)amt * (1.0f - fm)));
        }
        r8_pal_commit_range(SK_NITE, 4, 0xFFFFFFu, 0, 0);   /* (the lights on the water: never darkened) */
    }
}

/* -------------------------------------------------------------- scene --- */
/* the levels: the MIX page */
SC_SMALL static void sk_mix(void)
{
    fm_part_level(1, sk.kv[SKK_NOTES]);
    fm_part_level(3, sk.kv[SKK_NOTES]);               /* the gusts' chime (or harp) and the touches go with the notes */
    fm_part_level(4, sk.kv[SKK_NOTES]);               /* and the swells */
    fm_part_level(5, sk.kv[SKK_NOTES]);               /* and the heavy falls' bowl */
    fm_part_level(6, sk.kv[SKK_NOTES]);               /* and the black keys' harp */
    cd_drone_level(sk.kv[SKK_DRONE]);                 /* the conductor breathes it */
    fm_bed(FM_BED_WIND, sk.kv[SKK_WIND]);
    fm_bed(FM_BED_STREAM, sk.kv[SKK_STREAM]);
}

SC_SMALL static void sk_parts(void)
{
    fm_part_patch(1, SK_TONE_SLOT[sk.kv[SKK_TONE]]);
    fm_part_patch(2, ZN_SOFT_PAD);                    /* the drone: a soft pad (the Sho whined) */
    fm_part_patch(3, sk.kv[SKK_GUSTS] ? FM_P_HARP : ZN_FURIN);   /* gusts and touches: a wind chime, or the Harp */
    fm_part_patch(4, ZN_SOFT_SWELL);                  /* Soft Swell: far off, the night, the heavy falls */
    fm_part_release(1, 2600);
    fm_part_release(2, 4000);
    fm_part_release(3, 2200);
    fm_part_release(4, 3500);
    fm_part_patch(5, ZN_RIN);                         /* a soft, deep bowl under a heavy fall */
    fm_part_release(5, 3500);
    fm_part_patch(6, FM_P_HARP);                          /* the black keys' harp */
    fm_part_release(6, 2200);
    sk_mix();
}

SC_SMALL static void sk_defaults(int16_t *kv)
{
    int i;
    for (i = 0; i < SC_NKNOB; i++)
        kv[i] = SK_KNOB[i][3];
}

SC_SMALL static void sk_enter(uint32_t seed, int16_t *kv)
{
    int i;
    memset(&sk, 0, sizeof sk);
    memset(sk_raft, 0, sizeof sk_raft);
    memset(sk_spk, 0, sizeof sk_spk);
    memset(sk_cs_ok, 0, sizeof sk_cs_ok);
    sk.kv = kv;
    for (i = 0; i < SC_NKNOB; i++)                    /* a saved value out of range (a later version): the default */
        if (SK_KNOB[i][1] > SK_KNOB[i][0] && (kv[i] < SK_KNOB[i][0] || kv[i] > SK_KNOB[i][1]))
            kv[i] = SK_KNOB[i][3];
    for (i = 0; i < SK_MAXP; i++)
        sk_p[i].st = PS_FREE;
    for (i = 0; i < SK_MAXR; i++)
        sk_r[i].t = 0.0f;
    sk.seed = seed | 1u;
    sk.gust_next = 25.0f;
    sk.heavy_next = 20.0f + 20.0f * (float)(seed % 100u) * 0.01f;
    for (i = 0; i < SK_MAXH; i++)
        sk_h[i].on = 0;
    sk.raft_next = 40.0f + 40.0f * (float)(seed % 50u) * 0.02f;   /* the visitors, the rafts, a shower: in a while */
    sk.bf_next = 60.0f + 60.0f * (float)(seed % 37u) / 37.0f;
    sk.bird_next = 90.0f + 90.0f * (float)(seed % 41u) / 41.0f;
    sk.rain_next = 600.0f + 600.0f * (float)(seed % 53u) / 53.0f;
    for (i = 0; i < 256; i++)
        sk_sin8[i] = (int8_t)(127.0f * sk_sin((float)i * (1.0f / 256.0f)));
    for (i = 0; i < SK_NSTREAK; i++) {                /* the river's light: staggered, so they never come together */
        sk_s[i].life = 1.0f;
        sk_s[i].age = 1.0f + (float)i * 0.4f;
    }
    sc_seed(seed * 2246822519u + 7u);
    for (i = 0; i < SK_NCLOUD; i++)                   /* (the sky as it is now: after the day is set, below) */
        sk_cloud_new(i, -60.0f + 300.0f * ((float)i + sc_frand()) / (float)SK_NCLOUD);
    sk.day = 0.12f + 0.2f * (float)(seed % 100u) * 0.01f;   /* a morning or early afternoon */
    sk.hold = SK_MOOD_DAY[sc_mood_base(kv[SKK_MOOD], SK_NMOOD)];   /* (Custom: its mood's time) */
    if (sk.hold >= 0.0f)
        sk.day = sk.hold;
    sc_glide_at(&sk.glide, sk.hold);
    sk_lanterns_at();
    sc_seed(seed * 2246822519u + 11u);
    for (i = 0; i < SK_NCLOUD; i++) {                 /* which are up as it starts: as many as the time wants */
        float x = sk_c[i].x;
        sk_c[i].on = 0;
        sk_cloud_come(i, x);
    }
    sc_seed(sk.seed * 2654435761u + 1u);
    cd_init(seed);
    cd_activity((float)kv[SKK_ACT] * 0.01f);
    cd_echo_amount((float)kv[SKK_ECHO] * 0.01f);
    cd_change_mode(1);                                /* (the key's drift: always slow) */
    if (sc_mood_base(kv[SKK_MOOD], SK_NMOOD) > 0) {   /* (a named mood: its own key and scale) */
        int b = sc_mood_base(kv[SKK_MOOD], SK_NMOOD);
        cd_tune(SK_MOOD_SCALE[b], SK_MOOD_KEY[b]);
    }
    fm_tape(0);
    cd_range(SK_KLO, SK_KN);
    sk_parts();
    r8_dark(SK_DARK);                                 /* the panel's dark curve: picked by eye, every scene */
    cd_drone(1);
    fm_bed_mod(FM_BED_WIND, 10);
    fm_bed_mod(FM_BED_STREAM, 10);
    fm_bed_style(FM_BED_STREAM, kv[SKK_WATER]);     /* bubbles, the brook, white, pink, deep or rain */
    fm_bed_style(FM_BED_WIND, kv[SKK_AIR]);         /* a soft synthesized breeze, or a breeze, leaves or insects */
    fm_bed(FM_BED_RAIN, 0);
    cd_melody(kv[SKK_MELODY]);
    cd_arc_amount((float)kv[SKK_ARC] * 0.01f);
    cd_depth_send(kv[SKK_DEPTH]);
}

SC_SMALL static void sk_leave(void)
{
    int b;
    cd_stop();
    r8_dark(0);
    fm_tape(0);                                       /* tape is the scene's: the instrument never has it */
    fm_depth(0);                                      /* (and depth) */
    for (b = 0; b < FM_NBED; b++)
        fm_bed(b, 0);
}

/* heavy falls (after ambalek's "fall"): now and then (the Heavy knob: 0 never, 100 every half minute or
 * so) a cluster of blossoms breaks off, falls slower and straighter than a petal, lands with a double ring and a
 * low swell (conductor.c cd_heavy), and comes apart into floating petals */
static void sk_heavy_step(float dt)
{
    int i, j;
    float h = (float)sk.kv[SKK_HEAVY] * 0.01f;
    if (h > 0.0f && sk.ntip) {
        sk.heavy_next -= dt;
        if (sk.heavy_next <= 0.0f) {
            for (i = 0; i < SK_MAXH; i++)
                if (!sk_h[i].on) {
                    int t = (int)(sc_rnd() % (uint32_t)sk.ntip);
                    sk_h[i].on = 1;
                    sk_h[i].x = sk_tip[t].x;
                    sk_h[i].y = sk_tip[t].y;
                    sk_h[i].vx = 0.0f;
                    sk_h[i].vy = 2.0f;
                    sk_h[i].a = sc_frand();
                    sk_h[i].land = SK_WATER + 10.0f + 50.0f * sc_frand();
                    break;
                }
            sk.heavy_next = (150.0f - 120.0f * h) * (0.7f + 0.6f * sc_frand());
        }
    }
    for (i = 0; i < SK_MAXH; i++) {
        if (!sk_h[i].on)
            continue;
        sk_h[i].vx += (0.35f * sk_wind(sk_h[i].x, sk_h[i].y) - sk_h[i].vx) * fm_minf(1.0f, dt);   /* the wind moves it less */
        sk_h[i].vy += (24.0f - sk_h[i].vy) * fm_minf(1.0f, 0.8f * dt);
        sk_h[i].x += sk_h[i].vx * dt;
        sk_h[i].y += sk_h[i].vy * dt;
        sk_h[i].a += 0.08f * dt;
        if (sk_h[i].x < -20.0f || sk_h[i].x > 260.0f) {
            sk_h[i].on = 0;
            continue;
        }
        if (sk_h[i].y >= sk_h[i].land && sk_h[i].land > 240.0f) {   /* (landed out of view, nearer than the trunk:
                                                       * heard, not seen) */
            sk_h[i].on = 0;
            cd_heavy(fm_clampf(sk_h[i].x / 240.0f, 0.0f, 1.0f), 1.0f);
            continue;
        }
        if (sk_h[i].y >= sk_h[i].land) {             /* it lands: two rings, a low swell, and it comes apart */
            float near = (sk_h[i].land - SK_WATER) / (240.0f - SK_WATER);
            int32_t ly = (int32_t)(sk_h[i].land * sk_k());
            if (ly >= 0 && ly < 240 && sk_h[i].x * sk_k() < (float)sk_fgx[ly] + 2.0f) {
                sk_h[i].land = 252.0f;                /* (in front of the trunk or the bank: never behind them) */
                continue;
            }
            sk_h[i].on = 0;
            sk_ripple(sk_h[i].x, sk_h[i].land);
            sk_ripple_at(sk_h[i].x, sk_h[i].land, 0.4f);
            cd_heavy(fm_clampf(sk_h[i].x / 240.0f, 0.0f, 1.0f), 0.5f + 0.5f * near);
            for (j = 0; j < 5; j++) {
                int n;
                for (n = 0; n < SK_MAXP && sk_p[n].st != PS_FREE; n++)
                    ;
                if (n == SK_MAXP)
                    break;
                sk_spawn(sk_h[i].x, sk_h[i].land);
                sk_p[n].st = PS_FLOAT;
                sk_p[n].x = sk_h[i].x + (sc_frand() - 0.5f) * 10.0f;
                sk_p[n].y = sk_h[i].land + (sc_frand() - 0.5f) * 4.0f;
            }
        }
    }
}

static void sk_step(uint32_t dt_ms)
{
    float dt = (float)dt_ms * 0.001f, a = (float)sk.kv[SKK_ACT] * 0.01f;
    int i, j;
    uint32_t cdt = dt_ms;
    sk.t += dt;
    if (sk.hold < 0.0f) {   /* the day goes round: half an hour (Natural; was 12 min). (No new tree at
                             * midnight, PLAY grows one) */
        sk.day += dt * (1.0f / 1800.0f);
        if (sk.day >= 1.0f) {
            sk.day -= 1.0f;
            cd_dawn();                                /* a new day: perhaps a new key or scale (the slow drift) */
        }
        sc_glide_at(&sk.glide, sk.hold);
    } else                                            /* a mood holds its time: there in 20 s, gently, forward */
        sk.day = sc_glide(&sk.glide, sk.day, sk.hold, dt);
    sk_lanterns_step(dt);
    cd_step(cdt);
    {   /* Natural: the scale with the light: Yo from dawn through the afternoon, Min'yo from golden hour */
        float d = sk.day - fm_floorf(sk.day);
        cd_light(d > 0.97f || d < 0.4f ? SC_YO : SC_MINYO);
    }
    {   /* Natural: the sound with the time, each mood's character in its turn (the knobs as they are): livelier by
         * day, quieter and warmer toward the night */
        float sun = sk_sin(sk.day), lit = fm_clampf(sun * 1.5f + 0.3f, 0.0f, 1.0f);
        int nat = sk.kv[SKK_MOOD] == SC_MOOD_NATURAL;
        cd_activity(a * (nat ? 0.55f + 0.55f * lit : 1.0f));
        cd_depth_send(sk.kv[SKK_DEPTH] + (nat ? (int)(30.0f * (1.0f - lit)) : 0));   /* (and the arc's) */
    }
    {   /* the population: Activity's number of petals in the air, refilled a few a second at most */
        int air = 0, want = (int)((2.0f + 28.0f * a) * 0.8f + 0.5f);   /* (0.8: the smaller crown, and the heavy falls;
                                                       * a petal falls farther now, landing anywhere across the stream:
                                                       * as many land a second as before) */
        for (i = 0; i < SK_MAXP; i++)
            air += sk_p[i].st == PS_AIR && sk_p[i].sz > 0.9f;   /* (our tree's: the far cherries' come on top) */
        sk.spawn_acc += dt * 1.5f;
        if (sk.spawn_acc > 2.0f)
            sk.spawn_acc = 2.0f;
        while (sk.spawn_acc >= 1.0f && air < want) {
            sk.spawn_acc -= 1.0f;
            air++;
            if (sk.ntip) {
                int t = (int)(sc_rnd() % (uint32_t)sk.ntip);
                sk_spawn(sk_tip[t].x, sk_tip[t].y);
            }
        }
    }
    /* gusts: now and then by themselves, more often when active */
    sk.gust *= fm_expf(-dt * 0.3f);
    sk.paw_x += sk.gust_dir * 40.0f * dt;             /* (its patch on the water runs before it) */
    sk.gust_next -= dt * (0.4f + a);
    if (sk.gust_next <= 0.0f) {
        sk_gust(sc_frand() < 0.75f ? 1.0f : -1.0f);
        sk.gust_next = 30.0f + 60.0f * sc_frand();
    }
    if (sk.kv[SKK_AIR] == FM_AIR_INSECTS)           /* the insects: the night (noon 0, dusk 0.5, midnight 1) */
        fm_bed_mod(FM_BED_WIND, (int)(100.0f * fm_clampf(0.5f - 0.5f * sk_sin(sk.day), 0.0f, 1.0f)));
    else
        fm_bed_mod(FM_BED_WIND, (int)(10.0f + 25.0f * a + 65.0f * sk.gust));
    for (i = 0; i < SK_MAXP; i++) {
        petal_t *p = &sk_p[i];
        if (p->st == PS_AIR) {
            float fl, tvx, tvy;
            p->ph += p->fr * dt;
            if (p->ph >= 1.0f)
                p->ph -= 1.0f;
            fl = sk_sin(p->ph);
            tvx = (sk_wind(p->x, p->y) + fl * 14.0f) * p->sz;   /* (a far one: its pace as small as it looks) */
            tvy = p->fall * (0.5f + 0.5f * fm_fabsf(fl));     /* edge-on it drops, flat it floats */
            p->vx += (tvx - p->vx) * fm_minf(1.0f, 1.8f * dt);
            p->vy += (tvy - p->vy) * fm_minf(1.0f, 1.4f * dt);
            p->x += p->vx * dt;
            p->y += p->vy * dt;
            p->a += (p->va + fl * 0.4f) * dt;
            if (p->x < -24.0f || p->x > 264.0f || p->y > 250.0f) {
                p->st = PS_FREE;
                continue;
            }
            if (p->y >= p->land) {                            /* it lands: a ripple, and maybe a note */
                int32_t ly = (int32_t)(p->land * sk_k());
                int hid = ly >= 0 && ly < 240 && p->x * sk_k() < (float)sk_fgx[ly] + 1.0f;   /* (in front of the trunk
                                                       * or the bank: it lands nearer than they stand, out of view,
                                                       * never behind them: heard, not seen) */
                float near = (p->land - SK_WATER) / (240.0f - SK_WATER), sp = sk_sqrt(p->vx * p->vx + p->vy * p->vy);
                if (hid) {
                    int32_t lx = (int32_t)(p->x * sk_k());
                    uint8_t g = lx >= 0 && lx < r8_bg.w && ly < r8_bg.h ? r8_bg.px[ly * r8_bg.w + lx] : 0;
                    if (g >= SK_BANK && g < SK_BANK + 8 && sk.gathered < 400u) {   /* on our grass: it stays there,
                                                       * the petals gathering under the tree through the day */
                        r8_quad(&r8_bg, p->x * sk_k(), p->land * sk_k(), fm_maxf(1.5f * sk_k(), 0.6f),
                                fm_maxf(0.7f * sk_k(), 0.4f), p->a, (uint8_t)(SK_PET + 1 + (sc_rnd() % 3u)));
                        sk.gathered++;
                        p->st = PS_FREE;
                    } else
                        p->land = 260.0f;             /* (it falls on past the foot of the picture) */
                }
                else {
                    p->st = PS_FLOAT;
                    p->y = p->land;
                    sk_ripple(p->x, p->y);
                }
                sk.landed++;
                if (p->echo >= 0)                     /* a key's petal: its note again, softly (the melody comes
                                                       * back through the petals) */
                    cd_note_key(p->echo, fm_clampf(p->x / 240.0f, 0.0f, 1.0f), 0.18f, 1200, 1, 0u);
                else if (p->echo == -2)
                    ;                                 /* (the rest of a key's petals: unheard) */
                else if (sc_frand() < 0.06f + 0.9f * a + 0.22f * near) {   /* a quiet day: the far ones land unheard */
                    float pos = fm_clampf(p->x / 240.0f, 0.0f, 1.0f), st = fm_clampf(sp / 40.0f, 0.0f, 1.0f) * (0.45f + 0.55f * near),
                          night = fm_clampf(0.5f - 0.5f * sk_sin(sk.day), 0.0f, 1.0f);
                    if (sc_frand() < (near < 0.35f ? 0.55f : 0.0f) + 0.5f * night)
                        cd_swell(pos, st, 1800);        /* far off, or at night: a note that blooms */
                    else
                        cd_pluck(pos, st, 1200);
                }
            }
        } else if (p->st == PS_FLOAT) {
            float near = (p->y - SK_WATER) / (240.0f - SK_WATER), cur = sk_current(near, p->x, p->y);
            int32_t ry = (int32_t)(p->y * sk_k());
            if (ry >= 0 && ry < 240 && p->x < (float)sk_fgx[ry] / sk_k() + 12.0f && p->y > 205.0f)
                cur *= 0.12f;                         /* the eddy along our bank: they gather there, a raft */
            p->x += cur * dt;                         /* (the current shows in them) */
            p->a += 0.02f * dt;
            p->life -= dt;
            if (p->cool > 0.0f)
                p->cool -= dt;
            if (p->life <= 0.0f || p->x > 250.0f || p->x < -10.0f)
                p->st = PS_FREE;
        }
    }
    for (i = 0; i < SK_MAXP; i++) {                           /* floating petals that drift into each other */
        petal_t *p = &sk_p[i];
        if (p->st != PS_FLOAT || p->cool > 0.0f)
            continue;
        for (j = i + 1; j < SK_MAXP; j++) {
            petal_t *q = &sk_p[j];
            if (q->st != PS_FLOAT || q->cool > 0.0f)
                continue;
            if (fm_fabsf(p->x - q->x) < 3.5f && fm_fabsf(p->y - q->y) < 2.0f) {
                if (sc_frand() < 0.5f)                        /* (half as often, and seldom) */
                    cd_touch(p->x / 240.0f, 0.5f);
                p->cool = q->cool = 15.0f;
                sk_ripple((p->x + q->x) * 0.5f, p->y);
                break;
            }
        }
    }
    for (i = 0; i < SK_MAXR; i++)
        if (sk_r[i].t != 0.0f && sk_r[i].t < 1.8f) {   /* (below 0: a ring still waiting its turn) */
            sk_r[i].t += dt;
            if (sk_r[i].t == 0.0f)
                sk_r[i].t = 1e-4f;
        }
    sk_heavy_step(dt);
    for (i = 0; i < SK_NSPK; i++)
        if (sk_spk[i].t > 0.0f)
            sk_spk[i].t -= dt;
    for (i = 0; i < SK_NCLOUD; i++) {                 /* the clouds drift on (a little quicker in a shower); one gone
                                                       * off the right comes back new, or the sky stays clear a while */
        if (sk_c[i].on) {
            sk_c[i].x += sk_c[i].v * (1.0f + 0.5f * sk.rain) * dt;
            if (sk_c[i].x > 300.0f) {
                sk_c[i].on = 0;
                sk_c[i].wait = 20.0f + 70.0f * sc_frand();
            }
        } else if ((sk_c[i].wait -= dt) <= 0.0f)
            sk_cloud_come(i, -80.0f - 40.0f * sc_frand());
    }
    for (i = 0; i < SK_NSTREAK; i++) {                /* light on the river: it drifts with the current (as the
                                                       * floating petals: faster near), lives a few seconds, and a
                                                       * new one comes somewhere else; longer near the bank */
        float near;
        if (sk_s[i].age >= sk_s[i].life) {
            float u = sc_frand();
            sk_s[i].y = SK_WATER + 3.0f + (236.0f - SK_WATER) * sk_sqrt(u);
            near = (sk_s[i].y - SK_WATER) / (240.0f - SK_WATER);
            sk_s[i].x = 240.0f * sc_frand();          /* (the whole river: behind the trunk, it is hidden) */
            sk_s[i].len = 5.0f + 22.0f * near * (0.5f + sc_frand());
            sk_s[i].life = 3.0f + 4.0f * sc_frand();
            sk_s[i].age = 0.0f;
            continue;
        }
        near = (sk_s[i].y - SK_WATER) / (240.0f - SK_WATER);
        sk_s[i].x += sk_current(near, sk_s[i].x, sk_s[i].y) * dt * 0.75f;
        sk_s[i].age += dt;
    }
    sk.ms += cdt;
    if (sk_nft && (sk.ft_next -= dt) <= 0.0f) {       /* a far cherry sheds a petal: small, slow, a short fall to
                                                       * the far water, unheard */
        int t = (int)(sc_rnd() % (uint32_t)sk_nft);
        petal_t *p = sk_spawn(sk_ft[t].x + (sc_frand() - 0.5f) * sk_ft[t].w * 0.7f, sk_ft[t].bot - 2.0f);
        if (p) {
            p->land = SK_WATER + 1.0f + 7.0f * sc_frand();
            p->fall = 3.0f + 3.0f * sc_frand();
            p->sz = 0.6f;
            p->echo = -2;
        }
        sk.ft_next = (1.2f + 2.5f * sc_frand()) * (1.6f - (float)sk.kv[SKK_ACT] * 0.01f);
    }
    /* petal rafts (hanaikada): now and then one drifts in from upstream, or gathers in the slow water along a bank;
     * floating petals that touch one join it; they drift slowly and thin out as their petals sink */
    if ((sk.raft_next -= dt) <= 0.0f) {
        for (j = 0; j < SK_NRAFT && sk_raft[j].n > 0.0f; j++)
            ;
        if (j < SK_NRAFT) {
            float r = sc_frand();
            sk_raft[j].y = r < 0.2f ? SK_WATER + 2.5f + 3.0f * sc_frand() : SK_WATER + 26.0f + 40.0f * sc_frand();
            sk_raft[j].x = r < 0.6f ? -8.0f : 50.0f + 110.0f * sc_frand();
            sk_raft[j].n = 24.0f + 24.0f * sc_frand();
            sk_raft[j].seed = sc_rnd() | 1u;
            sk.rafts++;
        }
        sk.raft_next = 45.0f + 60.0f * sc_frand();
    }
    for (j = 0; j < SK_NRAFT; j++)
        if (sk_raft[j].n > 0.0f) {
            float near = (sk_raft[j].y - SK_WATER) / (240.0f - SK_WATER);
            int32_t ry = (int32_t)(sk_raft[j].y * sk_k());
            float cur = sk_current(near, sk_raft[j].x, sk_raft[j].y) * (near < 0.12f ? 0.12f : 0.25f);   /* (slow by
                                                       * the wall) */
            if (ry >= 0 && ry < 240 && sk_raft[j].x < (float)sk_fgx[ry] / sk_k() + 14.0f)
                cur *= 0.2f;                          /* (the eddy along our bank) */
            sk_raft[j].x += cur * dt;
            sk_raft[j].n -= dt * 0.025f;
            if (sk_raft[j].x > 262.0f)
                sk_raft[j].n = 0.0f;
            for (i = 0; i < SK_MAXP; i++) {
                petal_t *q = &sk_p[i];
                if (q->st == PS_FLOAT && fm_fabsf(q->x - sk_raft[j].x) < 3.0f + sk_sqrt(sk_raft[j].n) &&
                    fm_fabsf(q->y - sk_raft[j].y) < 2.5f && sk_raft[j].n < 70.0f) {
                    q->st = PS_FREE;
                    sk_raft[j].n += 1.0f;
                }
            }
        }
    {   /* a shower now and then (every 10..20 min): rises over ~12 s, falls a minute or two, eases off over ~15 s */
        float want = 0.0f;
        if (sk.rain_left > 0.0f) {
            sk.rain_left -= dt;
            want = 1.0f;
        } else if ((sk.rain_next -= dt) <= 0.0f) {
            sk.rain_left = 60.0f + 70.0f * sc_frand();
            sk.rain_next = 600.0f + 600.0f * sc_frand();
            sk.showers++;
        }
        sk.rain += (want - sk.rain) * fm_minf(1.0f, dt * (want > sk.rain ? 0.09f : 0.07f));
        if (sk.rain < 0.002f)
            sk.rain = 0.0f;
        fm_bed(FM_BED_RAIN, (int)(45.0f * sk.rain));
        fm_bed_mod(FM_BED_RAIN, (int)(35.0f * sk.rain));       /* (how many drops: ~80 a second at full rain) */
        sc_rain_step(sk_drop, SK_NDROP, &sk.drops, sk.rain, 50.0f, 230.0f, dt);   /* (each drop on its own; those
                                                       * in the air fall on out when it stops) */
        if (sk.rain > 0.01f) {
            if (sc_frand() < sk.drops * sk.drops * dt * 5.0f)   /* a drop's ring on the water (with the falling
                                                       * rain, a few first) */
                sk_ripple_at(4.0f + 232.0f * sc_frand(), SK_WATER + 6.0f + 64.0f * sc_frand(), 0.0f);
        }
    }
    /* a butterfly across (not in a shower) */
    if (sk.bf.on) {
        sk.bf.ph += dt;
        sk.bf.x += sk.bf.vx * dt;
        sk.bf.y = sk.bf.base + 9.0f * sk_sin(sk.bf.ph * 0.23f) + 2.0f * sk_sin(sk.bf.ph * 1.7f);
        if (sk.bf.x < -12.0f || sk.bf.x > 252.0f)
            sk.bf.on = 0;
    } else if ((sk.bf_next -= dt) <= 0.0f && sk.rain < 0.05f) {
        int left = sc_frand() < 0.5f;
        sk.bf.on = 1;
        sk.bf.x = left ? -10.0f : 250.0f;
        sk.bf.vx = (left ? 1.0f : -1.0f) * (9.0f + 7.0f * sc_frand());
        sk.bf.base = 60.0f + 80.0f * sc_frand();
        sk.bf.ph = 0.0f;
        sk.butterflies++;
        sk.bf_next = 90.0f + 120.0f * sc_frand();
    }
    /* a bush warbler: flies in to a branch's tip, perches a while and sings ("hoo ... hokekyo"), flies off */
    if (sk.bird.st) {
        sk.bird.t += dt;
        if (sk.bird.st == 1 || sk.bird.st == 3) {     /* flying: an arc from one point to the other in ~3 s */
            float u = fm_minf(1.0f, sk.bird.t / 3.0f), e = u * u * (3.0f - 2.0f * u);
            sk.bird.x = sk.bird.x0 + (sk.bird.x1 - sk.bird.x0) * e;
            sk.bird.y = sk.bird.y0 + (sk.bird.y1 - sk.bird.y0) * e - 18.0f * sk_sin(u * 0.5f);
            if (u >= 1.0f) {
                if (sk.bird.st == 1) {
                    sk.bird.st = 2;
                    sk.bird.t = 0.0f;
                    sk.bird.perch = 10.0f + 10.0f * sc_frand();
                    sk.bird.sing = 1.5f;
                } else {
                    sk.bird.st = 0;
                }
            }
        } else {                                      /* perched, swaying with its branch */
            sk.bird.x = sk.bird.x1;
            if ((sk.bird.sing -= dt) <= 0.0f) {      /* a song: a real bush warbler's "hoo ... hokekyo" (CC0,
                                                       * the whistle was "way too loud", not real), soft, as if
                                                       * from across the river */
                fm_bird(0, (int)(sc_rnd() % (uint32_t)fm_bird_count()), 40 + (int)(sc_rnd() % 10u),
                        cd_pan(sk.bird.x / 240.0f), 96 + (int)(sc_rnd() % 9u));
                sk.songs++;
                sk.bird.sing = 7.0f + 6.0f * sc_frand();
            }
            if ((sk.bird.perch -= dt) <= 0.0f) {      /* off again, away to the right */
                sk.bird.st = 3;
                sk.bird.t = 0.0f;
                sk.bird.x0 = sk.bird.x;
                sk.bird.y0 = sk.bird.y;
                sk.bird.x1 = 260.0f;
                sk.bird.y1 = 20.0f + 40.0f * sc_frand();
            }
        }
    } else if ((sk.bird_next -= dt) <= 0.0f && sk.rain < 0.05f && sk.ntip > 0) {
        int t, tries;
        for (tries = 0, t = 0; tries < 12; tries++) {  /* an outer tip, high in the crown */
            t = (int)(sc_rnd() % (uint32_t)sk.ntip);
            if (sk_tip[t].x > 90.0f && sk_tip[t].y < 120.0f)
                break;
        }
        sk.bird.st = 1;
        sk.bird.t = 0.0f;
        sk.bird.tip = t;
        sk.bird.x0 = 255.0f;
        sk.bird.y0 = 30.0f + 40.0f * sc_frand();
        sk.bird.x1 = sk_tip[t].x;
        sk.bird.y1 = sk_tip[t].y - 4.0f;
        sk.birds++;
        sk.bird_next = 150.0f + 150.0f * sc_frand();
    }
    sk_palette();
}

/* a new cloud at x: a long low bank of 4..6 puffs, at its own height and pace ("random clouds that
 * slowly move by in the background") */
SC_SMALL static void sk_cloud_new(int c, float x)
{
    sk_cs_ok[c] = 0;
    /* a fair-weather cumulus: round domes of mixed sizes heaped up over a flat base, the biggest toward the middle, a
     * turret or two on top; far clouds (the first two) small, low and slow, nearer ones larger and higher. They drift
     * slowly, the far ones slowest (a near one crosses the sky in 3-8 minutes) */
    int i, n0, nd, far = c < 2;
    float sc = far ? 0.42f + 0.18f * sc_frand() : 0.7f + 0.4f * sc_frand(), w;
    sk_c[c].x = x;
    sk_c[c].y = far ? 92.0f + 18.0f * sc_frand() : 24.0f + 50.0f * sc_frand();
    sk_c[c].v = far ? 0.2f + 0.3f * sc_frand() : 0.6f + 1.0f * sc_frand();
    sk_c[c].scale = sc;
    n0 = 4 + (int)(sc_rnd() % 3u);                    /* the base row: broad, sitting on the base */
    w = (float)(n0 - 1) * 10.0f * sc;
    sk_c[c].np = 0;
    for (i = 0; i < n0 && sk_c[c].np < SK_NPUFF; i++) {
        int k = sk_c[c].np++;
        sk_c[c].pr[k] = (8.0f + 3.5f * sc_frand()) * sc;
        sk_c[c].px[k] = -0.5f * w + (float)i * 10.0f * sc + (sc_frand() - 0.5f) * 4.0f * sc;
        sk_c[c].py[k] = -0.3f * sk_c[c].pr[k];
        sk_c[c].pp[k] = sc_frand();
    }
    nd = n0 - 2 + (int)(sc_rnd() % 2u);              /* domes on them, bigger toward the middle */
    for (i = 0; i < nd && sk_c[c].np < SK_NPUFF; i++) {
        int k = sk_c[c].np++;
        float u = ((float)i + 0.5f) / (float)nd, mid = 1.0f - fm_fabsf(u - 0.5f) * 2.0f;
        sk_c[c].px[k] = -0.4f * w + u * 0.8f * w + (sc_frand() - 0.5f) * 6.0f * sc;
        sk_c[c].py[k] = -(7.0f + 6.0f * mid + 2.0f * sc_frand()) * sc;
        sk_c[c].pr[k] = (6.0f + 5.0f * mid + 2.5f * sc_frand()) * sc;
        sk_c[c].pp[k] = sc_frand();
    }
    for (i = 0; i < 1 + (int)(sc_rnd() % 2u) && sk_c[c].np < SK_NPUFF; i++) {   /* a turret or two */
        int k = sk_c[c].np++;
        sk_c[c].px[k] = (sc_frand() - 0.5f) * 0.5f * w;
        sk_c[c].py[k] = -(16.0f + 3.0f * sc_frand()) * sc;
        sk_c[c].pr[k] = (4.0f + 2.5f * sc_frand()) * sc;
        sk_c[c].pp[k] = sc_frand();
    }
    sk_c[c].base = 2.0f * sc;                         /* the flat underside, a little below the center line */
    sk_c[c].top = -24.0f * sc;
}

/* how many near clouds the sky wants now: a few in the afternoon, fewer in the morning's haze and at golden hour, one
 * or none at night, more in a shower; a little different each time */
static int sk_cloud_want(void)
{
    float d = sk.day - fm_floorf(sk.day);
    int w = sk_night() > 0.5f ? 1 : d < 0.15f ? 2 : d < 0.42f ? 3 : 2;
    if (sk.rain > 0.05f)
        w = 3;
    if (sc_frand() < 0.3f)
        w--;
    return w < 0 ? 0 : w;
}
/* a cloud comes in (or the sky stays clear there a while): a far one now and then, a near one while fewer than wanted
 * are up */
SC_SMALL static void sk_cloud_come(int c, float x)
{
    int i, n = 0;
    sk_cloud_new(c, x);
    if (c < 2)
        sk_c[c].on = sc_frand() < (sk_night() > 0.5f ? 0.3f : 0.6f);
    else {
        for (i = 2; i < SK_NCLOUD; i++)
            n += i != c && sk_c[i].on;
        sk_c[c].on = n < sk_cloud_want();
    }
    if (!sk_c[c].on)
        sk_c[c].wait = 20.0f + 70.0f * sc_frand();
}

/* a cloud into the frame, only where the frame is sky (or cloud): the branch and the hills stay in front of it.
 * Shaded as one mass: its height in the cloud sets the shade (sunlit at the top, gray at the flat base), each puff a
 * touch brighter at its own upper edge; the edge thins into the sky; the puffs breathe slowly (the shape changes) */
static void sk_cloud_make(float k, int c)
{
    int i, far = c < 2, nsh = far ? 4 : 8;
    uint8_t c0 = far ? SK_CLOUDF : SK_CLOUD, *st = sk_cst[c];
    float base = (sk_c[c].y + sk_c[c].base) * k, top = (sk_c[c].y + sk_c[c].top) * k;
    int32_t ox = (int32_t)fm_floorf(sk_c[c].x * k) - SK_CSW / 2, oy = (int32_t)fm_floorf((sk_c[c].y - 31.0f) * k);
    for (i = 0; i < SK_CSW * SK_CSH; i++)
        st[i] = 0;
    sk_cso[c][0] = (int16_t)ox;
    sk_cso[c][1] = (int16_t)oy;
    sk_csx[c] = sk_c[c].x;
    sk_cs_ok[c] = 1;
    for (i = 0; i < sk_c[c].np; i++) {               /* the base row, then the domes and turrets over it */
        float br = 1.0f + 0.06f * sk_sin(sk.t * 0.02f + sk_c[c].pp[i]);
        float cx = (sk_c[c].x + sk_c[c].px[i] + 1.5f * sk_sin(sk.t * 0.013f + sk_c[c].pp[i] * 1.7f)) * k,
              cy = (sk_c[c].y + sk_c[c].py[i]) * k, r = sk_c[c].pr[i] * br * k, ry = r * 0.85f;
        int32_t x, y, x0 = (int32_t)(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f), y0 = (int32_t)(cy - ry - 1.0f),
                y1 = (int32_t)(cy + ry + 1.0f);
        if (x0 < ox)
            x0 = ox;
        if (y0 < oy)
            y0 = oy;
        if (x1 >= ox + SK_CSW)
            x1 = ox + SK_CSW - 1;
        if (y1 > (int32_t)(base + 2.0f * k))
            y1 = (int32_t)(base + 2.0f * k);
        if (y1 >= oy + SK_CSH)
            y1 = oy + SK_CSH - 1;
        for (y = y0; y <= y1; y++) {
            float dy = ((float)y + 0.5f - cy) / ry, hgt = fm_clampf(((float)y - top) / (base - top + 1e-3f), 0.0f, 1.0f),
                  ir = 1.0f / r, dy2 = dy * dy;
            uint8_t *row = st + (y - oy) * SK_CSW - ox;
            if (dy2 >= 1.0f)
                continue;
            for (x = x0; x <= x1; x++) {
                float dx = ((float)x + 0.5f - cx) * ir, d = dx * dx + dy2, lam, sh,
                      bx = base + (0.8f * sk_sin((float)x / k * 0.043f + sk_c[c].pp[0]) +
                                   0.4f * sk_sin((float)x / k * 0.11f + sk_c[c].pp[1])) * k;   /* (the base:
                                                       * level, wavering a pixel) */
                int f;
                if (d >= 1.0f || (float)y + 0.5f > bx)
                    continue;                          /* (below the base: the sky, cleanly) */
                if (d > 0.84f && dy > -0.35f &&       /* the sides soften into the sky; the sunlit tops stay crisp */
                    !r8_dith(x, y, (int)((1.0f - d) * (16.0f / 0.16f))))
                    continue;
                /* each dome lit from the sun high on the left: its own roundness, and the whole cloud darker toward
                 * its flat base, which is its darkest (a band along it) */
                lam = -0.42f * dx - 0.62f * dy + 0.66f * sk_sqrt(1.0f - d);
                sh = 0.38f * (1.0f - fm_clampf((lam + 0.3f) * (1.0f / 1.2f), 0.0f, 1.0f)) + 0.55f * hgt;   /* (the domes
                                                       * blend into one mass: the cloud's height counts most) */
                if (bx - (float)y < 1.8f * k)
                    sh = fm_maxf(sh, 0.9f);
                f = (int)(fm_clampf(sh, 0.0f, 0.99f) * (float)(nsh - 1) * 16.0f);   /* (dithered between steps) */
                row[x] = (uint8_t)(c0 + (f >> 4) + r8_dith(x, y, f & 15));   /* (the later puff in front) */
            }
        }
    }
}

/* a cloud into the frame, only where the frame is sky (or cloud): the branch and the hills stay in front of it.
 * Shaded as one mass: its height in the cloud sets the shade (sunlit at the top, gray at the flat base), each puff a
 * touch brighter at its own upper edge; the edge thins into the sky; the puffs breathe slowly (the shape changes:
 * the stamp made again, sk_cloud_make) */
static void sk_cloud_draw(surf_t *s, float k, int c)
{
    const uint8_t *st = sk_cst[c];
    int32_t sx, sy, ox, oy = sk_cso[c][1];
    if (!sk_c[c].on)
        return;
    if (!sk_cs_ok[c])
        sk_cloud_make(k, c);
    ox = sk_cso[c][0] + ((int32_t)fm_floorf(sk_c[c].x * k) - (int32_t)fm_floorf(sk_csx[c] * k));   /* (drifted since) */
    oy = sk_cso[c][1];
    if (ox >= s->w || ox + SK_CSW <= 0)
        return;
    for (sy = 0; sy < SK_CSH; sy++) {
        int32_t y = oy + sy, xa = ox < 0 ? -ox : 0, xb = ox + SK_CSW > s->w ? s->w - ox : SK_CSW;
        const uint8_t *src = st + sy * SK_CSW;
        uint8_t *row;
        if (y < 0 || y >= s->h)
            continue;
        row = s->px + y * s->w + ox;
        for (sx = xa; sx < xb; sx++) {
            uint8_t nv = src[sx], v;
            if (!nv)
                continue;
            v = row[sx];
            if (v < SK_MTN || (v >= SK_CLOUD && v < SK_CLOUDE + 4 && nv < v))
                row[sx] = nv;
        }
    }
}

/* a ring on the water: unbroken, and only on the water (rings went over the far bank; never over the
 * near bank or the trunk either) */
static void sk_ring(surf_t *s, float cx, float cy, float rx, float ry, uint8_t c)
{
    int32_t i, n = 8 + (int32_t)(fm_maxf(rx, ry) * 7.0f);
    if (n > 360)
        n = 360;
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        int32_t x = (int32_t)fm_floorf(cx + rx * sk_sin(t + 0.25f)), y = (int32_t)fm_floorf(cy + ry * sk_sin(t));
        uint8_t *q;
        if (x < 0 || y < 0 || x >= s->w || y >= s->h)
            continue;
        q = s->px + y * s->w + x;
        if (sk_wet(*q) || (*q >= SK_SHIM && *q < SK_SHIM + 8) || (*q >= SK_RIP && *q < SK_RIP + 8))
            *q = c;
    }
}

static float sk_daylight(void)                /* 1 by day .. 0 at night */
{
    return fm_clampf(sk_sin(sk.day) * 2.0f + 0.3f, 0.0f, 1.0f);
}

/* komorebi: the light through the tree (the sunbeams gone):
 *   the shade   a pool on the near water under the crown, its outline by column (shifted a little right, the sun
 *               high on the left) seen at an angle, soft-edged, gone well before the far bank; spots of sun drift
 *               slowly in it 
 *   glints      a wind on the open water: sparkles, more in a gust
 * By day, and less in a shower. Cheap: the shade only on its pool's rows, no float work per pixel there */
static void sk_light_draw(surf_t *s, float k)
{
    float day = sk_daylight() * (1.0f - sk.rain);
    int32_t W = s->w, wy = (int32_t)(SK_WATER * k) + 1;
    int i;
    if (day < 0.05f)
        return;
    {   /* the sun on the water: a path of glitter under it. High in the afternoon (on the left): broad and near; low at
         * golden hour (to the right, over the far hills): a narrow path reaching to the far bank. More in a gust */
        float d = sk.day - fm_floorf(sk.day), elev = fm_clampf(sk_sin(d), 0.0f, 1.0f),
              xs = 20.0f + 180.0f * fm_clampf((d - 0.04f) / 0.44f, 0.0f, 1.0f),
              gold = fm_clampf(1.0f - fm_fabsf(d - 0.45f) / 0.1f, 0.0f, 1.0f);
        int n = (int)(day * (2.0f + 7.0f * sk.gust + 2.0f * (float)sk.kv[SKK_ACT] * 0.01f + 7.0f * gold + 3.0f * elev));
        for (i = 0; i < n; i++) {
            float u = sc_frand(), yw = SK_WATER + 2.0f + (236.0f - SK_WATER) * (elev > 0.5f ? sk_sqrt(u) : u),
                  near = (yw - SK_WATER) / (240.0f - SK_WATER),
                  xw = xs + (sc_frand() + sc_frand() + sc_frand() - 1.5f) * 1.4f * (4.0f + 26.0f * near) * (0.35f + 1.3f * elev);
            int32_t gx = (int32_t)(xw * k), gy = (int32_t)(yw * k);
            uint8_t *p;
            if (gx < 0 || gx >= W || gy < wy || gy >= s->h)
                continue;
            p = s->px + gy * W + gx;
            if (sk_wet(*p) && sk_shade[(int)xw < 240 && xw >= 0.0f ? (int)xw : 239] < 60)
                *p = SK_SHIM + 7;
        }
    }
}

/* the visitors: a butterfly fluttering by; a bush warbler flying in to a branch, perched, singing */
static void sk_visitors_draw(surf_t *s, float k)
{
    if (sk.bf.on) {
        float fl = fm_fabsf(sk_sin(sk.bf.ph * 6.0f)), x = sk.bf.x, y = sk.bf.y, w = 1.0f + 2.6f * fl;
        r8_tri(s, x * k, y * k, (x - w) * k, (y - 2.2f) * k, (x - w * 0.8f) * k, (y + 0.6f) * k, SK_VIS);
        r8_tri(s, x * k, y * k, (x + w) * k, (y - 2.2f) * k, (x + w * 0.8f) * k, (y + 0.6f) * k, SK_VIS + 1);
        r8_pset(s, (int32_t)(x * k), (int32_t)(y * k), SK_VIS + 2);
    }
    if (sk.bird.st) {
        float x = sk.bird.x, y = sk.bird.y, dir = sk.bird.st == 3 || sk.bird.x1 > sk.bird.x0 ? 1.0f : -1.0f;
        if (sk.bird.st == 2)
            dir = sk.bird.x0 > sk.bird.x1 ? -1.0f : 1.0f;
        r8_limb(s, (x - dir * 3.5f) * k, (y + 0.5f) * k, 1.4f * k, (x - dir * 1.0f) * k, y * k, 1.8f * k, SK_VIS + 4);   /* tail */
        r8_disc(s, x * k, y * k, 2.2f * k, SK_VIS + 3);                                                              /* body */
        r8_disc(s, (x + dir * 0.4f) * k, (y + 0.9f) * k, 1.2f * k, SK_VIS + 5);                                      /* belly */
        r8_disc(s, (x + dir * 2.0f) * k, (y - 1.4f) * k, 1.4f * k, SK_VIS + 3);                                      /* head */
        r8_pset(s, (int32_t)((x + dir * 2.5f) * k), (int32_t)((y - 1.7f) * k), SK_VIS + 2);
        if (sk.bird.st != 2) {                        /* flying: its wings */
            float f = sk_sin(sk.bird.t * 9.0f) * 3.0f;
            r8_tri(s, (x - 1.0f) * k, (y - 0.5f) * k, (x + 1.5f) * k, (y - 0.5f) * k, x * k, (y - 1.0f - f) * k, SK_VIS + 4);
        }
    }
}

static void sk_rain_draw(surf_t *s, float k)
{
    int i;
    for (i = 0; i < SK_NDROP; i++) {                  /* a shower: short slanting streaks */
        float x = sk_drop[i].x, y = sk_drop[i].y;
        if (!sk_drop[i].on)
            continue;
        r8_limb(s, x * k, y * k, 0.6f, (x - 1.2f) * k, (y - 5.0f) * k, 0.5f, (uint8_t)(SK_VIS + 6 + (i & 1)));
    }
}

static void sk_reeds_draw(surf_t *s, float k)
{
    int i, j;
    for (i = 0; i < SK_NREED; i++) {
        float x = sk_reed[i].x, y = sk_reed[i].y, h = sk_reed[i].h,
              sw = fm_clampf(sk_wind(x, y - h) * 0.05f, -2.5f, 2.5f) + 0.5f * sk_sin(sk.t * 0.4f + x * 0.07f);
        for (j = 0; j < 2; j++) {                     /* two blades, the second shorter, leaning its own way */
            float hh = h * (j ? 0.7f : 1.0f), tx = x + (float)j * 1.2f + sk_reed[i].lean * (j ? -0.6f : 1.0f) + sw * hh / 12.0f,
                  mx = x + (float)j * 1.2f + (tx - x) * 0.35f;
            r8_limb(s, (x + (float)j * 1.2f) * k, y * k, 0.9f * k, mx * k, (y - hh * 0.5f) * k, 0.7f * k, (uint8_t)(SK_BANK + 3 - j));
            r8_limb(s, mx * k, (y - hh * 0.5f) * k, 0.7f * k, tx * k, (y - hh) * k, 0.35f * k, (uint8_t)(SK_BANK + 2 - j));
        }
    }
}
static void sk_rafts_draw(surf_t *s, float k)    /* a raft: its petals packed in a long patch along the flow */
{
    int i, j;
    for (i = 0; i < SK_NRAFT; i++) {
        float n = sk_raft[i].n, near = fm_clampf((sk_raft[i].y - SK_WATER) / (240.0f - SK_WATER), 0.0f, 1.0f),
              len = 3.0f + sk_sqrt(n) * (1.0f + 1.4f * near), hh = len * (0.1f + 0.16f * near);
        uint32_t h = sk_raft[i].seed;
        for (j = 0; j < (int)n; j++) {
            float u, v;
            int32_t px, py;
            h = h * 1664525u + 1013904223u;
            u = ((float)(h >> 24) / 255.0f - 0.5f) * 2.0f;
            v = ((float)((h >> 16) & 255u) / 255.0f - 0.5f) * 2.0f * (1.0f - 0.6f * u * u);
            px = (int32_t)((sk_raft[i].x + u * len * 0.5f) * k);
            py = (int32_t)((sk_raft[i].y + v * hh) * k);
            if (px < 0 || py < 0 || px >= s->w || py >= s->h || !sk_wet(s->px[py * s->w + px]))
                continue;
            s->px[py * s->w + px] = (uint8_t)(SK_PET + ((h >> 8) % 3u));   /* (nearly white on the water) */
            if (near > 0.2f && px + 1 < s->w && sk_wet(s->px[py * s->w + px + 1]))
                s->px[py * s->w + px + 1] = (uint8_t)(SK_PET + 1 + ((h >> 10) % 3u));
        }
    }
}
static void sk_draw(void)
{
    surf_t *s = &r8_fb;
    float k = sk_k();
    int i;
    SC_PT_TAGS("cwlfo");                             /* (testing builds: the HUD's parts) */
    SC_PT_START();
    r8_copy(s, &r8_bg);
    if (sk_cs_n++ % 6u == 0u)                         /* (one cloud made again every six frames: each about every
                                                       * second, as its puffs breathe) */
        sk_cs_ok[(sk_cs_n / 6u) % SK_NCLOUD] = 0;
    for (i = 0; i < SK_NCLOUD; i++)
        sk_cloud_draw(s, k, i);
    SC_PT(0);
    {   /* the river: soft bands of lighter and darker
         * water across it, sliding steadily downstream with the current, wider and quicker near; two sets at their
         * own pace, so they come and go as they pass through each other. And streaks of light drifting with the
         * current, fading in and out */
        int32_t W = s->w, wy = (int32_t)(SK_WATER * k) + 1, y, x, ya = (int32_t)(196.0f * k), yb = (int32_t)(236.0f * k),
                yr = (int32_t)((SK_WATER + SK_REFROWS) * k);
        float day = sk_daylight() * (1.0f - sk.rain);
        static int8_t ax[240];
        for (y = wy; y < yr && y < s->h; y++) {      /* the reflections waver: each row a pixel this way or that */
            int off = (int)fm_floorf(1.25f * sk_sin(((float)y / k) * 0.55f + sk.t * 0.35f) + 0.5f);
            const uint8_t *b = r8_bg.px + y * W;
            uint8_t *p = s->px + y * W;
            if (!off)
                continue;
            for (x = 0; x < W; x++) {                 /* (the whole row, the water behind the trunk too; the trunk and
                                                       * the bank left as they are) */
                int32_t sx = x - off;
                uint8_t c = b[sx < 0 ? 0 : sx >= W ? W - 1 : sx];
                if ((uint8_t)(p[x] - SK_WAT) >= 16u && (uint8_t)(p[x] - SK_REF) >= 16u)
                    continue;
                if (c >= SK_REF && c < SK_REF + 16)
                    p[x] = c;
                else if (p[x] >= SK_REF && p[x] < SK_REF + 16)
                    p[x] = (uint8_t)(SK_WAT + ((y - wy) * 15) / (s->h - wy));
            }
        }
        static uint8_t shx[240];                      /* the shade by screen column (no division a pixel) */
        for (x = 0; x < W && x < 240; x++) {          /* (the shade's pool in the same pass: each pixel read
                                                       * once, and only when it changes) */
            shx[x] = day < 0.05f ? 0 : sk_shade[(int)((float)x / k) < 240 ? (int)((float)x / k) : 239];
            ax[x] = (int8_t)(40.0f * (sk_sin(((float)x / k) * 0.05f + sk.t * 0.03f) +   /* (the sun's spots) */
                                      0.6f * sk_sin(((float)x / k) * 0.13f - sk.t * 0.05f)));
        }
        for (y = wy; y < s->h; y++) {
            float yw = ((float)y + 0.5f) / k, near = (yw - SK_WATER) / (240.0f - SK_WATER), lam = 16.0f + 34.0f * near,
                  v = 5.8f * (0.4f + 0.8f * near), wob = 0.12f * sk_sin(yw * 0.19f + sk.t * 0.07f),
                  p1 = -sk.t * v / lam + yw * 0.02f + wob, p2 = -sk.t * v * 1.12f / (lam * 0.62f) - yw * 0.03f - wob;
            uint32_t ph1, ph2, i1 = (uint32_t)(4294967296.0f / (lam * k)), i2 = (uint32_t)(4294967296.0f / (lam * 0.62f * k));
            int th = (int)(110.0f - 30.0f * near), dv = 0, by = 0, mk = 0, rw = SK_WAT + ((y - wy) * 15) / (s->h - wy);
            uint8_t *p = s->px + y * W;
            int32_t x0 = 0;                           /* (the whole row: only water is touched) */
            /* ripple marks: short dashes along the flow, finer far off, sliding downstream (lighter near, where they
             * catch the sky; darker far); a row's own phase so they never line up */
            float lm = 2.6f + 7.0f * near, pm = -sk.t * v * 0.9f / lm + (float)((uint32_t)y * 2654435761u >> 20) * (1.0f / 4096.0f);
            uint32_t ph3, i3 = (uint32_t)(4294967296.0f / (lm * k)) * 2u;
            int32_t pa = -1, pb = -1;                 /* a gust's dark patch on the water (a cat's paw): this row's part */
            pm -= fm_floorf(pm);
            ph3 = (uint32_t)(int32_t)((pm - 0.5f) * 4294967296.0f) + 0x80000000u + (uint32_t)x0 * (i3 >> 1);
            if (sk.gust > 0.05f) {
                float dyp = (yw - sk.paw_y) / (5.0f + 10.0f * near);
                if (dyp > -1.0f && dyp < 1.0f) {
                    float hw = (22.0f + 30.0f * near) * sk_sqrt(1.0f - dyp * dyp);
                    pa = (int32_t)((sk.paw_x - hw) * k);
                    pb = (int32_t)((sk.paw_x + hw) * k);
                }
            }
            if (y >= ya && y < yb) {                  /* the shade's pool: rows 196..236 of the world, densest at 214 */
                float vy = 1.0f - fm_fabsf(yw - 214.0f) / 22.0f;
                if (vy > 0.0f)
                    dv = (int)(vy * vy * day * 12.0f);   /* the pool's depth here (of 16) */
                by = (int)(40.0f * sk_sin(yw * 0.11f - sk.t * 0.02f));
            }
            p1 += (float)x0 / (lam * k);
            p2 += (float)x0 / (lam * 0.62f * k);
            p1 -= fm_floorf(p1);
            p2 -= fm_floorf(p2);
            ph1 = (uint32_t)(int32_t)((p1 - 0.5f) * 4294967296.0f) + 0x80000000u;
            ph2 = (uint32_t)(int32_t)((p2 - 0.5f) * 4294967296.0f) + 0x80000000u;
            i1 *= 2u;
            i2 *= 2u;
            for (x = x0; x < W; x += 2, ph1 += i1, ph2 += i2, ph3 += i3) {   /* (a band's value a pair of pixels at a
                                                       * time, and a pair with nothing to do passed at once) */
                int b = sk_sin8[ph1 >> 24] + (sk_sin8[ph2 >> 24] >> 1), t = 0, sg = 0, xx, paw = x >= pa && x <= pb;
                mk = sk_sin8[ph3 >> 24] > (paw ? 60 : 100) && sk_sin8[(ph2 >> 23) & 255u] > 0 ? (near > 0.45f ? -1 : 1) : 0;
                if (b > th) {
                    t = (b - th) >> 2;
                    sg = -1;                          /* lighter */
                } else if (b < -th) {
                    t = (-th - b) >> 2;
                    sg = 1;
                }
                if (!t && !dv && !mk && !paw)
                    continue;
                for (xx = x; xx < x + 2 && xx < W; xx++) {
                    int d = t && r8_dith(xx, y, t) ? sg : 0;
                    uint8_t c;
                    if (dv && shx[xx] >= 40 && ax[xx] * by <= 1100 && r8_dith(xx, y, (shx[xx] * dv) >> 8))
                        d += 2;                       /* (in the shade: the water's ramp darkens with the index) */
                    d += mk;                          /* (a mark: solid, a dash) */
                    if (paw && r8_dith(xx, y, (int)(sk.gust * 14.0f)))
                        d += 1;                       /* (the gust's patch: the water roughened, darker) */
                    if (!d)
                        continue;
                    c = p[xx];
                    if (c >= SK_REF && c < SK_REF + 16) {   /* a reflection: a crest or a mark breaks it */
                        if (d < 0 && (t >= 2 || mk < 0))   /* (only a strong crest: no hatching) */
                            p[xx] = (uint8_t)rw;
                        continue;
                    }
                    if (c < SK_WAT || c >= SK_WAT + 16)
                        continue;
                    c = (uint8_t)(c + d);
                    p[xx] = c < SK_WAT ? SK_WAT : c > SK_WAT + 15 ? SK_WAT + 15 : c;
                }
            }
        }
        for (i = 0; i < SK_NSTREAK; i++) {            /* a streak is the water a little lighter, more toward its middle,
                                                       * by as much as it has faded in: it grows out of the water and
                                                       * melts back into it (they blinked on and off) */
            float u = sk_s[i].life > 0.0f ? sk_s[i].age / sk_s[i].life : 1.0f, e, hl;
            int32_t x0, x1;
            if (u >= 1.0f)
                continue;
            e = sk_sin(u * 0.5f);                /* 0 .. 1 .. 0, smoothly */
            e *= e;
            hl = sk_s[i].len * 0.5f * (0.4f + 0.6f * e);
            y = (int32_t)(sk_s[i].y * k);
            if (y < 0 || y >= s->h)
                continue;
            x0 = (int32_t)((sk_s[i].x - hl) * k);
            x1 = (int32_t)((sk_s[i].x + hl) * k);
            for (x = x0 < 0 ? 0 : x0; x <= x1 && x < W; x++) {
                uint8_t *q = s->px + y * W + x;
                float m = 1.0f - fm_fabsf(((float)x + 0.5f) / k - sk_s[i].x) / (hl + 1e-3f);   /* 1 middle .. 0 ends */
                int lift = (int)(e * m * 5.0f * 16.0f);    /* shade steps, in 1/16 (dithered) */
                if (*q < SK_WAT || *q >= SK_WAT + 16)
                    continue;
                lift = (lift >> 4) + r8_dith(x, y, lift & 15);
                *q = (uint8_t)(*q - lift < SK_WAT ? SK_WAT : *q - lift);
                if (e * m > 0.8f && r8_dith(x, y, (int)((e * m - 0.8f) * 80.0f)))
                    *q = SK_SHIM + 7;                 /* the brightest glint, at the full of it */
            }
        }
    }
    {   /* the current meeting our bank at the waterline: short pale wakes trailing downstream from its edge, coming and
         * going. Only where the bank itself meets the water (the trunk stands in front of the river, not in it: along
         * its side a wake read as hatching) */
        int32_t W = s->w, wy = (int32_t)(SK_WATER * k) + 1, y, x;
        for (y = wy + 2; y < s->h && y < 240; y += 2) {
            int32_t x0 = sk_fgx[y], l;
            float yw = ((float)y + 0.5f) / k, f = 0.5f + 0.5f * sk_sin(sk.t * 0.3f + yw * 0.17f);
            uint8_t e;
            if (x0 <= 0 || f < 0.35f)
                continue;
            e = r8_bg.px[y * W + x0 - 1];
            if (e < SK_BANK || e >= SK_BANK + 8)
                continue;                             /* (the bank's grass or earth, not the trunk) */
            l = (int32_t)((1.0f + 4.0f * f * (yw - SK_WATER) / (240.0f - SK_WATER)) * k) + 1;
            for (x = x0 + 1; x <= x0 + l && x < W; x++)
                if (sk_wet(s->px[y * W + x]) && r8_dith(x, y, (int)(f * 14.0f)))
                    s->px[y * W + x] = (uint8_t)(SK_SHIM + 5 + (x == x0 + 1));
        }
    }
    sk_times_draw(s, k);                              /* (the morning's mist, the night's lights) */
    SC_PT(1);
    sk_light_draw(s, k);
    SC_PT(2);
    for (i = 0; i < SK_MAXR; i++)
        if (sk_r[i].t > 0.0f && sk_r[i].t < 1.8f) {
            float u = sk_r[i].t / 1.8f, near = fm_clampf((sk_r[i].y - SK_WATER) / (240.0f - SK_WATER), 0.0f, 1.0f),
                  rx = (2.0f + 20.0f * u) * (0.3f + 0.7f * near);   /* (smaller far off, and flatter: seen lower) */
            sk_ring(s, sk_r[i].x * k, sk_r[i].y * k, rx * k, rx * (0.16f + 0.14f * near) * k, (uint8_t)(SK_RIP + (int)(u * 7.0f)));
        }
    for (i = 0; i < SK_NSPK; i++)                     /* a black key's sparkle: a bright point, its four arms for the
                                                       * first half, only on the water */
        if (sk_spk[i].t > 0.0f) {
            static const int8_t ARM[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            int32_t cx = (int32_t)(sk_spk[i].x * k), cy = (int32_t)(sk_spk[i].y * k), j, n = sk_spk[i].t > 0.4f ? 5 : 1;
            for (j = 0; j < n; j++) {
                int32_t x = cx + ARM[j][0], y = cy + ARM[j][1];
                uint8_t *q;
                if (x < 0 || y < 0 || x >= s->w || y >= s->h)
                    continue;
                q = s->px + y * s->w + x;
                if ((*q >= SK_WAT && *q < SK_WAT + 16) || (*q >= SK_RIP && *q < SK_RIP + 8))
                    *q = (uint8_t)(SK_SHIM + (j ? 5 : 7));
            }
        }
    for (i = 0; i < SK_MAXP; i++) {
        petal_t *p = &sk_p[i];
        if (p->st == PS_FLOAT) {                      /* lying flat on the water, seen low: a thin oval, barely turned
                                                       * (they stood up) */
            uint8_t c = (uint8_t)(p->c + (p->life < 6.0f ? (int)((6.0f - p->life) * 0.5f) : 0));
            float near = (p->y - SK_WATER) / (240.0f - SK_WATER);
            r8_quad(s, p->x * k, p->y * k, fm_maxf((0.8f + 2.1f * near) * k, 0.6f), fm_maxf((0.3f + 0.5f * near) * k, 0.4f),
                    0.012f * sk_sin(p->a), c > SK_PET + 7 ? SK_PET + 7 : c);   /* (as small as it is far) */
        }
    }
    sk_rafts_draw(s, k);
    {   /* the trunk and the bank stand in front of the water: whatever is on the water behind them is hidden */
        int32_t W = s->w, wy = (int32_t)(SK_WATER * k), y, x;
        for (y = wy; y < s->h && y < 240; y++) {
            const uint8_t *b = r8_bg.px + y * W;
            uint8_t *d = s->px + y * W;
            for (x = 0; x < sk_fgx[y]; x++)
                if (!sk_wet(b[x]))                    /* (the water behind the trunk, its reflection too, moves) */
                    d[x] = b[x];
        }
    }
    {   /* the big tree's dappled shade on our grass (by day), shifting as the crown sways */
        float day = sk_daylight() * (1.0f - sk.rain);
        int32_t W = s->w, y, x, ya = (int32_t)(223.0f * k);
        static int8_t dx8[240];
        if (day > 0.1f) {
            for (x = 0; x < W && x < 240; x++)       /* (the spots along x, once a frame) */
                dx8[x] = (int8_t)(40.0f * sk_sin(((float)x / k) * 0.09f + sk.t * 0.04f));
            for (y = ya; y < s->h; y++) {
                float yw = ((float)y + 0.5f) / k;
                int by = (int)(40.0f * sk_sin(yw * 0.23f + sk.t * 0.05f));
                uint8_t *d = s->px + y * W;
                for (x = 0; x < W; x++) {
                    int xw = (int)((float)x / k), sh;
                    if (d[x] < SK_BANK + 1 || d[x] > SK_BANK + 5)
                        continue;
                    sh = sk_shade[xw < 240 ? xw : 239];
                    if (sh > 60 && dx8[x] * by <= 700 && r8_dith(x, y, (int)((float)sh * day) >> 4))
                        d[x] = (uint8_t)(d[x] + 2);
                }
            }
        }
    }
    sk_reeds_draw(s, k);
    SC_PT(3);
    for (i = 0; i < SK_MAXH; i++)                    /* the heavy falls: a little cluster of blossoms on its stem */
        if (sk_h[i].on) {
            int j;
            float cx = sk_h[i].x, cy = sk_h[i].y;
            r8_limb(s, cx * k, (cy - 1.0f) * k, 1.2f * k, (cx + 3.0f * sk_sin(sk_h[i].a)) * k, (cy - 6.0f) * k, 0.8f * k,
                    SK_BARK + 2);
            for (j = 0; j < 5; j++)
                r8_disc(s, (cx + 2.6f * sk_sin(sk_h[i].a + 0.2f * (float)j)) * k,
                        (cy + 1.8f * sk_sin(sk_h[i].a + 0.2f * (float)j + 0.25f)) * k, 2.2f * k, (uint8_t)SK_RUST(0, 1 + j));
        }
    for (i = 0; i < SK_MAXP; i++) {
        petal_t *p = &sk_p[i];
        if (p->st == PS_AIR) {                        /* tumbling: thin and in shadow edge-on, broad and bright
                                                       * face-on */
            float f = sk_sin(p->ph), nl = (p->land - SK_WATER) / (240.0f - SK_WATER),
                  g = fm_clampf((p->y - 90.0f) / (p->land - 90.0f + 1e-3f), 0.0f, 1.0f),
                  z = p->sz * (1.0f - g + g * (0.45f + 0.55f * nl));   /* (smaller as it goes off to land far away) */
            int c = p->c + (f < -0.2f ? 2 : 0);
            r8_quad(s, p->x * k, p->y * k, fm_maxf(2.5f * z * k, 0.6f), fm_maxf((0.7f + 1.0f * fm_fabsf(f)) * z * k, 0.4f), p->a,
                    (uint8_t)(c > SK_PET + 7 ? SK_PET + 7 : c));
        }
    }
    sk_visitors_draw(s, k);
    sk_rain_draw(s, k);                               /* (whatever drops are in the air) */
    SC_PT(4);
}

/* keys: a white key plays the koto's note at once (its own pitch, folded into the scene's range
 * by octaves) and shakes the blossoms above it; the first two of its petals land with the same note, softly. A black
 * key plays the harp, a note of its own, low to high, so a melody can be played on them (the gusts are the wind's) */
SC_SMALL static void sk_key(int key, int on)
{
    int b;
    float x = 8.0f + (float)key * (224.0f / 26.0f);
    if (!on)
        return;
    for (b = 0; b < FM_NBLACK; b++)
        if (BLACK_K[b] == key) {
            cd_note_key(SK_KLO + b, x / 240.0f, 0.55f, 900, 6, 0u);
            {   /* seen: a ring on the water with a sparkle of sun in its middle:
                 * anywhere on the open water, clear of the bank and the trunk and away from the last
                 * one, so a run scatters like raindrops */
                float rx = 0.0f, ry = 0.0f;
                int j, tries;
                for (tries = 0; tries < 8; tries++) {
                    rx = 96.0f + 134.0f * sc_frand();
                    ry = SK_WATER + 8.0f + 60.0f * sc_frand();
                    if ((rx - sk.harp_x) * (rx - sk.harp_x) + (ry - sk.harp_y) * (ry - sk.harp_y) > 30.0f * 30.0f)
                        break;
                }
                sk.harp_x = rx;
                sk.harp_y = ry;
                sk_ripple(rx, ry);
                for (j = 0; j < SK_NSPK; j++)
                    if (sk_spk[j].t <= 0.0f || j == SK_NSPK - 1) {
                        sk_spk[j].x = rx;
                        sk_spk[j].y = ry;
                        sk_spk[j].t = 0.8f;
                        break;
                    }
            }
            sk.harp_keys++;
            sk.key_note = SK_KLO + b;
            return;
        }
    for (b = 0; b < FM_NWHITE; b++)
        if (WHITE_K[b] == key) {
            int k = b + SK_KLO;                       /* (sixteen notes, each its own, rising) */
            cd_note_key(k, x / 240.0f, 0.12f, 1300, 1, 0u);  /* (0.6 then 0.3 too loud: velocity ~46) */
            sk.key_note = k;
            sk_spawn_near(x, 3, k);
            return;
        }
}

SC_SMALL static void sk_knob(int i, int32_t e)
{
    if (i == SKK_MOOD) {                               /* a mood: every knob to the sound, its time, key, scale */
        int m = sc_mood_base(sk.kv[SKK_MOOD], SK_NMOOD), j;   /* (the glue has put the mood's knobs in, as
                                                       * made or as the player left them: every one to the sound) */
        (void)e;
        for (j = 1; j < SC_NUSED; j++)
            if (j != SKK_SPACE)
                sk_knob(j, 0);
        sk.hold = SK_MOOD_DAY[m];                     /* (Natural: the day goes on from where it is) */
        cd_tune(SK_MOOD_SCALE[m], SK_MOOD_KEY[m]);
        cd_activity((float)sk.kv[SKK_ACT] * 0.01f);
        cd_echo_amount((float)sk.kv[SKK_ECHO] * 0.01f);
        cd_depth_send(sk.kv[SKK_DEPTH]);
        sk_mix();
        return;
    }
    if (i == SKK_DEPTH || i == SKK_ARC) {
        sc_knob_turn(&sk.kv[i], e, 0, 100, 1);
        cd_arc_amount((float)sk.kv[SKK_ARC] * 0.01f);
        cd_depth_send(sk.kv[SKK_DEPTH]);
        return;
    }
    if (i == SKK_MELODY) {
        sc_knob_wrap(&sk.kv[i], e, 0, 1);
        cd_melody(sk.kv[i]);
        return;
    }
    if (i == SKK_SPACE) {
        knob_set(P_REVERB, proj.par[P_REVERB] + (int)(e * sc_knob_mul));   /* (by 1) */
        return;
    }
    if (i < 0 || i >= SC_NKNOB || SK_KNOB[i][1] <= SK_KNOB[i][0])
        return;                                       /* (a knob still to come) */
    if (i == SKK_AIR) {                              /* (Soft, Breeze, Insects) */
        sc_air_wrap(&sk.kv[i], e);
        fm_bed_style(FM_BED_WIND, sk.kv[i]);
        return;
    }
    if (i == SKK_TONE || i == SKK_WATER || i == SKK_AIR || i == SKK_GUSTS)
        sc_knob_wrap(&sk.kv[i], e, SK_KNOB[i][0], SK_KNOB[i][1]);   /* a list: round past either end */
    else
        sc_knob_turn(&sk.kv[i], e, SK_KNOB[i][0], SK_KNOB[i][1], SK_KNOB[i][2]);
    switch (i) {
    case SKK_ACT: cd_activity((float)sk.kv[SKK_ACT] * 0.01f); break;
    case SKK_ECHO: cd_echo_amount((float)sk.kv[SKK_ECHO] * 0.01f); break;
    case SKK_WATER: fm_bed_style(FM_BED_STREAM, sk.kv[SKK_WATER]); break;
    case SKK_AIR: fm_bed_style(FM_BED_WIND, sk.kv[SKK_AIR]); break;
    case SKK_TONE: fm_part_patch(1, SK_TONE_SLOT[sk.kv[SKK_TONE]]); break;
    case SKK_GUSTS: fm_part_patch(3, sk.kv[SKK_GUSTS] ? FM_P_HARP : ZN_FURIN); break;
    case SKK_NOTES: case SKK_DRONE: case SKK_WIND: case SKK_STREAM: sk_mix(); break;
    default: break;
    }
}

SC_SMALL static void sk_knob_text(int i, char *name, char *val)
{
    static const char *const N[SC_NUSED] = {"Mood", "Activity", "Instrument", "Space", "Notes", "Drone", "Air", "Water",
                                            "Air Type", "Water Type", "Echo", "Warmth", "Clusters", "Gust Sound",
                                            "Phrases", "Tide"};
    static const char *const AIR[4] = {"Soft", "Breeze", "Breeze", "Insects"};
    if (i < 0 || i >= SC_NUSED || (i != SKK_SPACE && SK_KNOB[i][1] <= SK_KNOB[i][0])) {
        name[0] = val[0] = 0;
        return;
    }
    copy_str(name, N[i], 11);
    switch (i) {
    case SKK_TONE: copy_str(val, SK_TONE_NAME[sk.kv[SKK_TONE]], 11); break;
    case SKK_WATER: copy_str(val, SC_WATER_NAME[sk.kv[SKK_WATER] % FM_NWATER], 11); break;
    case SKK_AIR: copy_str(val, AIR[sk.kv[SKK_AIR] & 3], 11); break;
    case SKK_GUSTS: copy_str(val, sk.kv[SKK_GUSTS] ? "Harp" : "Chime", 11); break;
    case SKK_SPACE: itoa_u((uint32_t)proj.par[P_REVERB], val); break;
    case SKK_MOOD: copy_str(val, SK_MOOD_NAME[sc_mood_base(sk.kv[SKK_MOOD], SK_NMOOD)], 11); break;
    case SKK_MELODY: copy_str(val, sk.kv[SKK_MELODY] ? "Loops" : "Free", 11); break;
    default: itoa_u((uint32_t)sk.kv[i], val); break;
    }
}

/* a new tree (the glue fades through dark around this): the world goes on, petals and all */
SC_SMALL static void sk_regrow(uint32_t seed)
{
    sk.seed = seed | 1u;
    sk_background();
}

/* knobs saved by an older sound version: the newer defaults once (Gusts the harp, Air is the soft breeze, the brook at
 * 50); the rest as they were */
/* a mood as it was made (the glue keeps the player's tweaks): the defaults, then the mood's own */
SC_SMALL static void sk_mood_kv(int m, int16_t *kv)
{
    sk_defaults(kv);
    m = sc_mood_base(m, SK_NMOOD);
    kv[SKK_MOOD] = (int16_t)m;
    sc_mood_set(kv, SK_MOODS[m], 8);
}
static const char *sk_mood_name(int m) { return SK_MOOD_NAME[sc_mood_base(m, SK_NMOOD)]; }

static const scene_t SCENE_SAKURA = {"Sakura", 0, sk_defaults, sk_enter, sk_leave, sk_background, sk_step, sk_draw, sk_key, sk_knob,
                                     sk_knob_text, sk_regrow, SK_SOUND, 0, 0, SK_NMOOD, sk_mood_kv, sk_mood_name, 0};
