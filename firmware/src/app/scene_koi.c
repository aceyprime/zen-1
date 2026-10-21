/* SPDX-License-Identifier: GPL-3.0-only */
/* Koi 鯉: a round pond seen from above. Koi swim in it, and the music is what they eat: a note sounds when a koi
 * takes a piece of food, and its pitch is how far from the pond's middle the food lay, deep and low in the middle,
 * shallow and high at the rim (after Dillbo's norns script KoiBoi2: food eaten as notes, the radius as the pitch,
 * uneaten food remembered; no code of it is used). The pond feeds itself a little (a few pellets, now and then a
 * cherry petal drifting in from Sakura's tree); a white key drops food on that key's own ring, so a melody can be
 * planted for the koi to find. Food left uneaten may sink into the pond's memory and come back later as soft swells.
 * Now and then a koi rises to the surface, two pass close, and the shishi-odoshi at the rim fills and tips its "tok".
 *
 * The pond's own sounds (Sakura keeps the koto): water voices for the bites (Drop, Mallet, Bell; the koto
 * still on the list), a singing bowl (Rin) for the soft events (a koi rising, a pebble, two passing, a memory), a
 * deeper drone (Deep Pad), a wooden knock (Bamboo), a trickle and insects for the beds.
 *
 *   KNOB 1 Activity (food falling, the koi's appetite)   2 Time   3 Tone (Drop, Mallet, Bell, Koto, Warm)   4 Space
 *   MIX: Notes, Drone, Air, Water.   MORE: Echo, Water is, Air is, Koi (how many).
 *   DRIFT: Change (the key / scale), Tape, Swim (lazy orbits that drift, or a free school), Knock (how often)
 *   White keys  food on that key's ring        Black keys  a pebble on a ring of its own, its own bowl note (the
 *                                                          lower ten, low to high); the top one: the knock, now
 *
 * The world is 240 x 240; drawing scales by r8_fb.w / 240 for pixel art. Every loop has a fixed bound. */
#define KP_CX 120.0f
#define KP_CY 124.0f
#define KP_R 98.0f                          /* the pond's radius as the notes know it (its rings); the water's own edge
                                             * is a new shape each picture, never inside the rings (kp_rim) */
#define KP_MAXK 9
#define KP_DARK 80                          /* the panel's dark curve (render8.c r8_dark):'s, every scene */
#define KP_SH_DY 186.0f                     /* the shishi-odoshi, moved down from the top right corner */
#define KP_NSEG 8
#define KP_MAXF 32
#define KP_MAXR 12
#define KP_MAXPAD 7
#define KP_NMEM 8
#define KP_KLO 6                            /* the pond plays white keys 6..15: the middle 6, the rim 15 (from 3:
                                             * the middle's notes were under the FM-1's speaker) */
#define KP_KN 10
#define KP_NTONE 5
#define KP_NWATER 8                         /* Water is: slow bubbles first (the default), then the others */
#define KP_SOUND 7                          /* the pond's sounds' version (scene_t.sound): 1 its own; 2 the
                                             * trickle, the Knock knob; 3 slow bubbles, mix set by ear;
                                             * 4 Mallet */
#define KP_KEY_ST 0.72f                    /* a white key's note: as loud as the pond's own (its koi's bites, measured on the
 * host: 0.72 on average, the same tone), matched to Sakura's; the low keys lifted (the speaker loses them): KP_KEY_LIFT at the lowest. Before: 0.55, then 0.17 */
#define KP_KEY_LIFT 0.3f
#define KP_MEMORY 0.35f                     /* uneaten food remembered (fixed now: its knob became Knock) */
enum { KP_DEEP = 0, KP_SHIM = 32, KP_GROUND = 40, KP_STONE = 48, KP_LILY = 56, KP_FLOWER = 64, KP_BAMBOO = 72, KP_WHITE = 80,
       KP_FOOD = 100, KP_PETAL = 104, KP_PEL = 108, KP_RIP = 112, KP_PLANT = 120,
       KP_MID = 128, KP_LOW = 148, KP_FROG = 168, KP_SUB = 176, KP_SKY = 184, KP_GRAV = 185, KP_FROG2 = 192, KP_REFL = 200,
       KP_NITE = 232, KP_TREE = 237, KP_LAMP = 245 };   /* (the lanterns' light: on the moss, three; on the water,
                                                       * two; a roof's lit rim; the flame) */
/* the koi's colors (KP_WHITE + these, 20 of them: KP_MID and KP_LOW hold them again under the water): cream, a deep
 * crimson-orange, lacquer black, metallic gold, an asagi's blue-gray and its net, orange, (spare: KC_BR0/1), yellow,
 * platinum, and the fins (pale, the water showing through) */
enum { KC_W0, KC_W1, KC_W2, KC_R0, KC_R1, KC_R2, KC_B0, KC_B1, KC_B2, KC_G0, KC_G1, KC_G2, KC_BL0, KC_BL1, KC_OR,
       KC_BR0, KC_BR1, KC_YE, KC_PL, KC_FIN };
static const uint8_t KC_HI[20] = {KC_W0, KC_W0, KC_W1, KC_R0, KC_R0, KC_R1, KC_B0, KC_B0, KC_B1, KC_G0, KC_G0, KC_G1,
                                  KC_BL0, KC_BL0, KC_OR, KC_BR0, KC_BR0, KC_YE, KC_PL, KC_FIN};
static const uint8_t KC_SH[20] = {KC_W1, KC_W2, KC_W2, KC_R1, KC_R2, KC_R2, KC_B1, KC_B2, KC_B2, KC_G1, KC_G2, KC_G2,
                                  KC_BL1, KC_BL1, KC_R1, KC_BR1, KC_BR1, KC_G1, KC_W2, KC_FIN};
/* the varieties, and how often each is in a pond (of 100) */
enum { KV_KOHAKU, KV_SANKE, KV_SHOWA, KV_TANCHO, KV_ASAGI, KV_OGON, KV_PLATINUM, KV_KIGOI, KV_N };
static const uint8_t KV_WEIGHT[KV_N] = {30, 16, 17, 6, 10, 10, 6, 5};   /* (the lantern's flame, its glow, its light on the water;
                                                     * the firebox, the moon on the water) */
#define KP_TORO_X 24.0f                             /* the stone lantern (toro), bottom left on the moss */
#define KP_TORO_Y 214.0f
/* a snow-viewing lantern (yukimi-doro), top right (under the maple when it is that side) */
#define KP_YUKI_X 214.0f
#define KP_YUKI_Y 30.0f
                                            /* (the second frog; the pond with a cloud's light on it) */   /* (the koi's colors a little
                                                                                * and further under the water; the
                                                                                * frogs; what lies under the water; the
                                                                                * sky on it) */
#define KP_NFROG 2
#define KP_FROGC(i) ((i) ? KP_FROG2 : KP_FROG)   /* a frog's 8 colors (KF_*), by its species */
enum { KF_BODY, KF_DARK, KF_THROAT, KF_PUPIL, KF_LINE, KF_STRIPE, KF_IRIS, KF_SPOT };
/* the frogs of a Japanese pond: each that hops on is one of these, by how common it is at a garden pond. Each has
 * a dark outline and bright eyes so it reads against the green pad */
#define KP_NSPECIES 6
static const struct {
    const char *name;
    uint32_t body, dark, throat, stripe, iris, spot;
    float size, rate;                       /* its size; its call's pitch (the recording's rate: bigger, lower) */
    uint8_t weight, stripes, spots, eyeline; /* how common (of 100); a pale line down the back; dark spots (2: many
                                             * small warts); a dark line through the eye */
} KP_SPECIES[KP_NSPECIES] = {
    {"tonosama", 0x86B24Cu, 0x5E8A36u, 0xEEEAD2u, 0xD8E8A0u, 0xD0A848u, 0x1C2414u, 1.15f, 1.00f, 25, 1, 1, 0},
    {"daruma", 0x94BC5Au, 0x6A9040u, 0xEEE6C8u, 0xC2D890u, 0xC89C48u, 0x34442Au, 1.00f, 1.05f, 20, 1, 1, 0},
    {"amagaeru", 0x82DC58u, 0x60B83Eu, 0xF6F6E6u, 0x82DC58u, 0xE0C878u, 0x2A3420u, 0.72f, 1.30f, 20, 0, 0, 1},
    {"shuregeru", 0x5ECC46u, 0x48A636u, 0xF2F0D0u, 0x5ECC46u, 0xF4C430u, 0x2A3420u, 0.92f, 1.20f, 10, 0, 0, 0},
    {"tsuchi", 0x7C6C4Au, 0x584830u, 0xDCD0B2u, 0x7C6C4Au, 0xB89048u, 0x3C3020u, 0.88f, 1.10f, 15, 0, 2, 0},
    {"ushigaeru", 0x7E8C40u, 0x5A642Cu, 0xF2EAC2u, 0x7E8C40u, 0xC09432u, 0x40442Au, 1.50f, 0.72f, 10, 0, 1, 0},
};
#define KP_NREFL 3
#define KP_NLIGHT 8                         /* the light on the floor: rippled lines */
/* the knobs (fewer, Mood first, named by what they do; by number; ui.c AMB_AT puts them on the four pages: Scene,
 * Volume, Nature, Music). Gone: Time (always 100 %), Change (the Key page's Drift), Tape, Swim (always the orbit) */
enum { KK_MOOD, KK_ACT, KK_TONE, KK_SPACE, KK_NOTES, KK_DRONE, KK_AIR, KK_WATER, KK_AIRIS, KK_WATERIS, KK_ECHO, KK_DEPTH,
       KK_KOI, KK_KNOCK, KK_MELODY, KK_ARC };
static const int16_t KP_KNOB[SC_NKNOB][4] = {   /* lo, hi, step, default; SPACE is the reverb (P_REVERB) */
    {0, 4, 1, 0}, {0, 100, 1, 30}, {0, KP_NTONE - 1, 1, 1}, {0, 0, 0, 0},   /* Mood (Natural), Activity,
                                                                              * Instrument (Mallet), Space */
    {0, 100, 1, 70}, {0, 100, 1, 15}, {0, 100, 1, 30}, {0, 100, 1, 30},   /* Volume: mix set by ear */
    {0, 3, 1, FM_AIR_INSECTS}, {0, KP_NWATER - 1, 1, 0}, {0, 100, 1, 40}, {0, 100, 1, 0},   /* Air Type
                                                                              * (crickets), Water Type, Echo, Warmth */
    {3, KP_MAXK, 1, 6}, {0, 100, 1, 50}, {0, 1, 1, 0}, {0, 100, 1, 50},   /* Nature: Koi Count (6: a garden pond's
                                                                          * look), Bamboo (about every
                                                                          * 1.5 min at 50), Phrases, Tide */
};
/* the moods (after Avonlea's presets; Natural first, the day going round; the others hold their time):
 * Feeding Time (midday), Still Pond (the afternoon), Rain on the Pond (the recorded rain: the eighth of KP_WATER) */
#define KP_NMOOD 5                                /* (Night, after Rain) */
static const char *const KP_MOOD_NAME[KP_NMOOD] = {"Natural", "Feeding", "Rain", "Still Pond", "Night"};   /* (in the
                                                       * order the day meets them) */
static const float KP_MOOD_DAY[KP_NMOOD] = {-1.0f, 0.25f, 0.30f, 0.34f, 0.70f};
static const int8_t KP_MOOD_SCALE[KP_NMOOD] = {-1, SC_YO, SC_MINYO, SC_MINYO, SC_MINYO};   /* (each its own key and scale) */
static const float KP_MOOD_KEY[KP_NMOOD] = {0.0f, 0.0f, 0.0f, 0.0f, -500.0f};
static const sc_set_t KP_MOODS[KP_NMOOD][10] = {
    {{KK_ACT, 30}, {KK_NOTES, 70}, {KK_DRONE, 15}, {KK_WATER, 30}, {KK_WATERIS, 0}, {KK_ECHO, 40}, {KK_KOI, 6},
     {KK_KNOCK, 50}, {KK_DEPTH, 0}, {KK_AIRIS, FM_AIR_INSECTS}},
    {{KK_ACT, 70}, {KK_NOTES, 75}, {KK_DRONE, 10}, {KK_WATER, 30}, {KK_WATERIS, 0}, {KK_ECHO, 35}, {KK_KOI, 8},
     {KK_KNOCK, 50}, {KK_DEPTH, 0}, {KK_AIRIS, FM_AIR_INSECTS}},
    {{KK_ACT, 15}, {KK_NOTES, 55}, {KK_DRONE, 25}, {KK_WATER, 45}, {KK_WATERIS, 7}, {KK_ECHO, 55}, {KK_KOI, 6},
     {KK_KNOCK, 30}, {KK_DEPTH, 30}, {KK_AIRIS, FM_AIR_INSECTS}},
    {{KK_ACT, 25}, {KK_NOTES, 70}, {KK_DRONE, 15}, {KK_WATER, 30}, {KK_WATERIS, 0}, {KK_ECHO, 40}, {KK_KOI, 6},
     {KK_KNOCK, 50}, {KK_DEPTH, 10}, {KK_AIRIS, FM_AIR_INSECTS}},
    {{KK_ACT, 12}, {KK_NOTES, 55}, {KK_DRONE, 25}, {KK_WATER, 25}, {KK_WATERIS, 0}, {KK_ECHO, 55}, {KK_KOI, 6},
     {KK_KNOCK, 30}, {KK_DEPTH, 40}, {KK_AIRIS, FM_AIR_INSECTS}},   /* (Night: soft, sparse, warm; crickets) */
};
static const uint8_t KP_WATER[KP_NWATER] = {FM_WATER_SLOW, FM_WATER_BUBBLES, FM_WATER_TRICKLE, FM_WATER_BROOK,
                                            FM_WATER_NOISE, FM_WATER_PINK, FM_WATER_DEEP, FM_WATER_RAIN};
static const uint8_t KP_TONE_SLOT[KP_NTONE] = {ZN_DROP, ZN_MALLET, ZN_BELL, FM_P_KOTO, FM_P_KOTO_WARM};
static const char *const KP_TONE_NAME[KP_NTONE] = {"Drop", "Mallet", "Bell", "Koto", "Warm"};
static const float KP_WIDTH[KP_NSEG] = {3.0f, 4.3f, 4.8f, 4.6f, 3.9f, 3.0f, 2.0f, 1.2f};   /* a koi's body, head to tail:
                                                       * a small rounded head, broad shoulders, a narrow tail stem */

typedef struct {
    float x[KP_NSEG], y[KP_NSEG];           /* the spine, head first */
    float h, v, size, ph;                   /* heading (turns), speed (px/s), size, the tail's phase */
    float pr, pr_v, dir, wander;            /* orbit: its depth, drifting; which way round; a school's wander */
    float cool_cross, look;                 /* seconds before another crossing note; before looking for food again */
    float full;                             /* seconds it is not hungry for, after eating (Activity: its appetite) */
    int target;                             /* the food it is after, or -1 */
    float z, zv, rise;                      /* how deep (0 the surface .. 1 the floor), drifting; rising to mouth at the
                                             * surface */
    float pitch;                            /* nose up (> 0) or down: the head deeper or shallower than the tail */
    float ix, iy, interest;                 /* a frog's plop, turned toward for a while */
    uint8_t on, var;                        /* its variety (KV_*) */
    uint8_t bfly;                           /* a butterfly koi: long, flowing fins and tail */
    uint8_t np, ns, pc[4];                  /* its patches (as many, each its color, KC_*) and its small black spots */
    float pu[4], pl[4], pv[4], pw[4];       /* a patch: where along it (0 the head .. 1 the tail) and how long; where
                                             * across (-1 .. 1) and how wide; (pl < 0: a round spot, radius -pl px) */
    float su[5], sv[5], sr[5];              /* a spot: where, its radius (px) */
    uint8_t st;                             /* what it is doing: 0 swimming somewhere, 1 hovering, 2 following another */
    float stt, wx, wy;                      /* for how long more; where it is going */
    int lead;                               /* the one it follows */
    float burst, fin, mouth;                /* a few strokes then a glide (> 0 stroking); its fins' flutter; its mouth
                                             * open at the surface (0..1) */
    float amp, bend, spread;                /* its body's wave (0 straight .. 1 stroking), its bend into a turn
                                             * (turns a second, eased), its pectoral fins (0 tucked .. 1 spread) */
} koi_t;
typedef struct {
    float x, y, vx, vy, age, life;
    int key, kind, on;                      /* kind: 0 a key's pellet, 1 a petal, 2 the pond's own (a smaller,
                                             * paler pellet) */
    int keyed;                              /* dropped by a white key: its note is the player's */
} food_t;

static koi_t kp_k[KP_MAXK];
static food_t kp_f[KP_MAXF];
static struct {
    float x, y, t;
} kp_r[KP_MAXR];
static struct {
    float x, y, r, notch;
    int flower;                             /* 0 none, 1 a white water lily, 2 a pink one */
    uint8_t shade, curl;                    /* its green; where its edge curls up (a lighter rim there) */
    float dip;                              /* a frog landing on it or leaving: it bobs, settling */
} kp_pad[KP_MAXPAD];
static struct {                             /* frogs on the lily pads */
    float x, y, x0, y0, x1, y1, t, sit, croak, puff;
    float fx, fy, rot, blink;               /* which way it faces; a turn still to make (turns); till it blinks */
    int st, pad;                            /* 0 in the water, 1 hopping on, 2 sitting, 3 hopping off */
    int sp;                                 /* its species (KP_SPECIES) */
    uint32_t mark;                          /* its own markings (where its spots are) */
} kp_fg[KP_NFROG];
static struct {                             /* the sky on the water: a cloud's reflection drifting over the pond, three
                                             * puffs (larger, whiter) */
    float x, y, rx, ry, v;
    float px[5], py[5], pr[5];
} kp_sk[KP_NREFL];
#define KP_NRAIN 18
static struct {                             /* rain on the pond: a drop's ring and splash; or a drop on a pad */
    float x, y, t, sz;
    int pad;                                /* on a lily pad: a splash, no ring */
} kp_rd[KP_NRAIN];
static struct {                             /* a line of light on the floor: its radius, where round it
                                             * starts and how far it runs (turns), its wobble, its slide, its phase */
    float r, th, len, kw, v, ph;
} kp_lt[KP_NLIGHT];
static struct {
    int key, left;
    float pos, at;
} kp_mem[KP_NMEM];
static struct {
    uint32_t seed;
    int16_t *kv;
    int npad;
    float t, day, food_acc, petal_next, surf_next, sh_fill, sh_tip, sh_period;
    int sh_back;                            /* the shishi-odoshi is swinging back up */
    float frog_next, br_x, br_y, br_t, br_dur, br_next;
    float lamp[2], lamp_to[2], lamp_next[2];   /* each lantern's light, breathing on its own (random, eased) */   /* a frog's next hop; a breeze's patch of ripples */
    float hold;                             /* the day a mood holds, or -1: it goes round (Natural) */
    sc_glide_t glide;                       /* the way there */
    uint32_t frogs, plops, breezes;
    float rain;                             /* how rainy (0..1, eased): the water's sound is rain */
    float feed, still;                      /* the Feeding, Still Pond moods (0..1, eased) */
    uint32_t raindrops;
    uint32_t eaten, knocks, expired, spawned, pebbles;
    int key_note;                           /* the note the last white key played (the host's tests) */
    float call_t, call_x, call_y;           /* someone at the pond's edge (a white key): the koi come over */
    float rimt[64];                         /* the water's edge: its radius by angle (kp_shape: a new shape each picture) */
} kp;
/* the overhanging branch (a maple, its spring green), over the pond from a corner: drawn over the koi each frame */
#define KP_NBL 6
#define KP_NBF 34
static struct {
    float x0, y0, x1, y1, w;
} kp_bl[KP_NBL];
static struct {
    float x, y, r;
    uint8_t c;
} kp_bf[KP_NBF];
static int kp_nbl, kp_nbf;
#define KP_NLEAF 2
static struct {                             /* leaves floating on the water, drifting, turning */
    float x, y, vx, vy, a, va;
    uint8_t c;
} kp_lf[KP_NLEAF];

/* ---------------------------------------------------------------- maths --- */
static float kp_atan2(float y, float x)       /* turns, -0.5 .. 0.5 (to ~0.002 turns: plenty for a koi's heading) */
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a, s, r;
    if (ax < 1e-6f && ay < 1e-6f)
        return 0.0f;
    a = ax > ay ? ay / ax : ax / ay;
    s = a * a;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan(a), radians */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = 3.14159274f - r;
    if (y < 0.0f)
        r = -r;
    return r * 0.159154943f;
}
static float kp_wrap(float t)                /* a turn difference into -0.5 .. 0.5 */
{
    t -= fm_floorf(t + 0.5f);
    return t;
}
static float kp_ring_r(int key)              /* the radius a key's food lies at */
{
    int i = key - KP_KLO;
    if (i < 0)
        i = 0;
    if (i >= KP_KN)
        i = KP_KN - 1;
    return ((float)i + 0.5f) / (float)KP_KN * (KP_R - 6.0f);
}
static int kp_key_at(float x, float y)       /* the key of a place: its distance from the middle */
{
    float dx = x - KP_CX, dy = y - KP_CY, r = fm_sqrtf(dx * dx + dy * dy);
    int i = (int)(r / (KP_R - 6.0f) * (float)KP_KN);
    return KP_KLO + (i < 0 ? 0 : i >= KP_KN ? KP_KN - 1 : i);
}
static float kp_scale(void) { return (float)r8_fb.w / 240.0f; }
/* the water's edge: a new irregular shape each picture (a garden pond's, never a circle), roughly round about the
 * middle and never inside the notes' rings (the shallowest at ~87 from the middle: the edge stays beyond ~93); kept
 * near round where the shishi-odoshi and the lantern stand */
static float kp_rim(float th)
{
    float u = (th - fm_floorf(th)) * 64.0f, f = u - fm_floorf(u);
    int i = (int)u & 63;
    return kp.rimt[i] + (kp.rimt[(i + 1) & 63] - kp.rimt[i]) * f;
}
static float kp_rim_at(float x, float y) { return kp_rim(kp_atan2(y - KP_CY, x - KP_CX)); }
SC_SMALL static void kp_shape(uint32_t seed)
{
    float a[4], ph[4];
    int i;
    sc_seed(seed * 2246822519u + 101u);
    a[0] = 4.0f + 6.0f * sc_frand();
    a[1] = 2.0f + 3.5f * sc_frand();
    a[2] = 1.0f + 2.0f * sc_frand();
    a[3] = 0.6f + 1.0f * sc_frand();
    for (i = 0; i < 4; i++)
        ph[i] = sc_frand();
    for (i = 0; i < 64; i++) {
        float th = (float)i / 64.0f, r = KP_R + 4.0f + a[0] * fm_sin_turns(2.0f * th + ph[0]) +
                   a[1] * fm_sin_turns(3.0f * th + ph[1]) + a[2] * fm_sin_turns(5.0f * th + ph[2]) +
                   a[3] * fm_sin_turns(7.0f * th + ph[3]);
        float ds = fm_fabsf(kp_wrap(th - 0.135f)), dt = fm_fabsf(kp_wrap(th - 0.37f));   /* (the shishi-odoshi; the
                                                       * lantern) */
        if (r < 97.0f)
            r = 97.0f - (97.0f - r) * 0.35f;          /* (softly: never inside the rings) */
        if (r > 112.0f)
            r = 112.0f + (r - 112.0f) * 0.3f;
        {
            float d = fm_minf(ds, dt);
            if (d < 0.08f)
                r = fm_minf(r, 100.0f + 120.0f * fm_maxf(0.0f, d - 0.035f));
        }
        kp.rimt[i] = r;
    }
}
static int kp_own_key(int key, float x)        /* the pond's own note: its ring's, or the loops' (Melody) */
{
    return kp.kv[KK_MELODY] ? cd_loop_pick(x / 240.0f) : key;
}

/* ---------------------------------------------------------------- world --- */
static void kp_ripple(float x, float y)
{
    int i, old = 0;
    for (i = 0; i < KP_MAXR; i++) {
        if (kp_r[i].t <= 0.0f || kp_r[i].t >= 2.0f) {
            old = i;
            break;
        }
        if (kp_r[i].t > kp_r[old].t)
            old = i;
    }
    kp_r[old].x = x;
    kp_r[old].y = y;
    kp_r[old].t = 0.001f;
}

static int kp_food_add(float x, float y, int kind, float life)
{
    int i;
    for (i = 0; i < KP_MAXF; i++)
        if (!kp_f[i].on) {
            food_t *f = &kp_f[i];
            f->on = 1;
            f->x = x;
            f->y = y;
            f->vx = (sc_frand() - 0.5f) * 0.6f;
            f->vy = (sc_frand() - 0.5f) * 0.6f;
            f->age = 0.0f;
            f->life = life;
            f->kind = kind;
            f->key = kp_key_at(x, y);
            kp.spawned++;
            {   /* the koi near see it fall: they look at once */
                int j;
                for (j = 0; j < KP_MAXK; j++) {
                    float ex = kp_k[j].x[0] - x, ey = kp_k[j].y[0] - y;
                    if (kp_k[j].on && ex * ex + ey * ey < 60.0f * 60.0f)
                        kp_k[j].look = 0.0f;
                }
            }
            return i;
        }
    return -1;
}

SC_SMALL static void kp_food_on_ring(int key, float side)   /* a white key: food on that key's ring, toward the near side of
                                                     * the pond (where you are: side -1 left .. 1 right); the note is
                                                     * the ring's, wherever round it */
{
    float a = 0.25f - side * 0.18f + (sc_frand() - 0.5f) * 0.12f, r = kp_ring_r(key);
    int i = kp_food_add(KP_CX + r * fm_sin_turns(a + 0.25f), KP_CY + r * fm_sin_turns(a), 0, 40.0f);
    if (i >= 0) {
        kp_f[i].key = key;                     /* exactly that key, whatever the rounding of its place */
        kp_f[i].keyed = 1;
        kp_ripple(kp_f[i].x, kp_f[i].y);
    }
}

static void kp_food_random(int kind, float life)   /* somewhere in the pond, clear of the rim */
{
    float a = sc_frand(), r = (KP_R - 10.0f) * fm_sqrtf(sc_frand());
    kp_food_add(KP_CX + r * fm_sin_turns(a + 0.25f), KP_CY + r * fm_sin_turns(a), kind, life);
}

SC_SMALL static void kp_memory_add(int key, float pos)
{
    int i, old = 0;
    for (i = 0; i < KP_NMEM; i++) {
        if (kp_mem[i].left <= 0) {
            old = i;
            break;
        }
        if (kp_mem[i].left < kp_mem[old].left)
            old = i;
    }
    kp_mem[old].key = key;
    kp_mem[old].pos = pos;
    kp_mem[old].left = 3;                      /* it comes back three times, then is forgotten */
    kp_mem[old].at = 8.0f + 10.0f * sc_frand();
}

/* a koi's patches: kohaku-like red on its body (a patch on the head often, then two or three down the back,
 * wrapping over the sides, white between) */
SC_SMALL static void kp_patches(koi_t *k, uint8_t col, float hv)
{
    float u0 = 0.24f + 0.06f * sc_frand();
    if (sc_frand() < 0.75f && k->np < 4) {            /* the head */
        k->pu[k->np] = 0.06f + 0.05f * sc_frand();
        k->pl[k->np] = 0.06f + 0.04f * sc_frand();
        k->pv[k->np] = (sc_frand() - 0.5f) * 0.3f;
        k->pw[k->np] = 0.75f + 0.3f * sc_frand();
        k->pc[k->np++] = col;
    }
    while (u0 < 0.82f && k->np < 4) {
        float hl = 0.07f + 0.09f * sc_frand();
        k->pu[k->np] = u0 + hl;
        k->pl[k->np] = hl;
        k->pv[k->np] = (sc_frand() - 0.5f) * 0.5f;
        k->pw[k->np] = hv * (0.75f + 0.45f * sc_frand());
        k->pc[k->np++] = col;
        u0 += 2.0f * hl + 0.05f + 0.1f * sc_frand();
    }
}
/* a koi's pattern, by its variety */
SC_SMALL static void kp_paint(koi_t *k, int v)
{
    int s;
    k->var = (uint8_t)v;
    k->np = k->ns = 0;
    if (v == KV_KOHAKU || v == KV_SANKE)
        kp_patches(k, KC_R1, 1.0f);
    else if (v == KV_SHOWA) {                         /* black, red and white wrapping round it */
        kp_patches(k, KC_R1, 1.2f);
        if (k->np < 4) {
            k->pu[k->np] = 0.3f + 0.4f * sc_frand();
            k->pl[k->np] = 0.06f + 0.06f * sc_frand();
            k->pv[k->np] = sc_frand() < 0.5f ? -0.65f : 0.65f;
            k->pw[k->np] = 0.4f + 0.2f * sc_frand();
            k->pc[k->np++] = KC_W1;
        }
    } else if (v == KV_TANCHO) {                      /* white, one red circle on the head */
        k->pu[0] = 0.08f;
        k->pl[0] = -(1.2f + 0.35f * sc_frand()) * k->size * 1.6f;
        k->pv[0] = 0.0f;
        k->pw[0] = 0.0f;
        k->pc[0] = KC_R1;
        k->np = 1;
    }
    if (v == KV_SANKE || v == KV_SHOWA) {             /* small black spots (sumi) on the back */
        int n = 3 + (int)(sc_rnd() % 3u);
        for (s = 0; s < n; s++) {
            k->su[s] = 0.18f + 0.62f * sc_frand();
            k->sv[s] = (sc_frand() - 0.5f) * 1.3f;
            k->sr[s] = (0.9f + 0.9f * sc_frand()) * k->size;
        }
        k->ns = (uint8_t)n;
    }
}
SC_SMALL static void kp_koi_new(int i, float x, float y)
{
    koi_t *k = &kp_k[i];
    int s, v;
    uint32_t w = sc_rnd() % 100u, acc = 0;
    for (v = 0; v < KV_N - 1; v++) {                  /* which variety: by how often each is kept */
        acc += KV_WEIGHT[v];
        if (w < acc)
            break;
    }
    k->on = 1;
    k->var = (uint8_t)v;
    k->bfly = 0;
    if (i > 0 && sc_frand() < 0.12f) {                /* (now and then one in a pond) */
        int j, has = 0;
        for (j = 0; j < KP_MAXK; j++)
            has |= j != i && kp_k[j].on && kp_k[j].bfly;
        k->bfly = !has;
    }
    k->h = sc_frand();
    k->size = i == 0 ? 1.18f + 0.14f * sc_frand() : 0.9f + 0.22f * sc_frand();   /* (a jumbo; the rest never small,
                                                       * none under 0.9: the pond never sparse) */
    k->v = 10.0f;
    k->ph = sc_frand();
    k->pr = 25.0f + (KP_R - 40.0f) * sc_frand();
    k->pr_v = 0.0f;
    k->dir = sc_frand() < 0.5f ? 1.0f : -1.0f;
    k->wander = 0.0f;
    k->cool_cross = 5.0f;
    k->look = sc_frand();
    k->target = -1;
    k->z = 0.2f + 0.35f * sc_frand();                 /* (each at its own depth, gliding up and down) */
    k->zv = 0.0f;
    k->rise = 0.0f;
    k->pitch = 0.0f;
    k->interest = 0.0f;
    k->st = 0;
    k->stt = 0.0f;
    k->wx = x;
    k->wy = y;
    k->lead = -1;
    k->burst = sc_frand() * 2.0f;
    k->amp = 0.5f;
    k->bend = 0.0f;
    k->spread = 0.6f;
    k->fin = sc_frand();
    k->mouth = 0.0f;
    kp_paint(k, v);
    for (s = 0; s < KP_NSEG; s++) {
        float sp = 3.0f * k->size;
        k->x[s] = x - (float)s * sp * fm_sin_turns(k->h + 0.25f);
        k->y[s] = y - (float)s * sp * fm_sin_turns(k->h);
    }
}

SC_SMALL static void kp_koi_count(int n)
{
    int i, guard;
    for (i = 0; i < KP_MAXK; i++) {
        if (i < n && !kp_k[i].on) {
            float a = sc_frand(), r = (KP_R - 20.0f) * sc_frand();
            kp_koi_new(i, KP_CX + r * fm_sin_turns(a + 0.25f), KP_CY + r * fm_sin_turns(a));
        } else if (i >= n) {
            kp_k[i].on = 0;
        }
    }
    for (guard = 0; guard < 6; guard++) {             /* at least three varieties in a pond (as many as there are koi,
                                                       * if fewer): the commonest repainted as one not there yet,
                                                       * never the jumbo */
        int cnt[KV_N] = {0}, distinct = 0, on = 0, most = 0, v, want;
        for (i = 0; i < KP_MAXK; i++)
            if (kp_k[i].on) {
                cnt[kp_k[i].var]++;
                on++;
            }
        for (v = 0; v < KV_N; v++) {
            distinct += cnt[v] > 0;
            if (cnt[v] > cnt[most])
                most = v;
        }
        if (distinct >= (on < 3 ? on : 3))
            break;
        do
            want = (int)(sc_rnd() % (uint32_t)KV_N);
        while (cnt[want] > 0);
        for (i = KP_MAXK - 1; i > 0; i--)
            if (kp_k[i].on && kp_k[i].var == most) {
                kp_paint(&kp_k[i], want);
                break;
            }
    }
}

/* ------------------------------------------------------------- picture --- */
/* how far into the night (as Sakura's): 0 by day .. 1 dark, the lantern lit */
static float kp_night(void) { return fm_clampf(-fm_sin_turns(kp.day) * 2.5f + 0.1f, 0.0f, 1.0f); }

/* a lantern's light at night: a warm wash on the moss round it, fading smoothly (three steps, each mixed only
 * with the next); on the nearest stones of the rim; and on the water toward it, in short broken streaks on the
 * ripples' crests that shimmer and drift, brightest nearest the lantern */
static void kp_toro_light(surf_t *s, float k, float x, float y, int n)
{
    float nt = kp_night(), cx = KP_CX + (x - KP_CX) * 0.64f, cy = KP_CY + (y - KP_CY) * 0.64f, lv = kp.lamp[n],
          ir = 1.0f / (784.0f * (0.86f + 0.14f * lv) * (0.86f + 0.14f * lv));   /* (its reach, breathing) */
    int32_t px, py;
    if (nt < 0.05f)
        return;
    for (py = (int32_t)((y - 28.0f) * k); py <= (int32_t)((y + 28.0f) * k); py++)   /* the moss and the stones */
        for (px = (int32_t)((x - 28.0f) * k); px <= (int32_t)((x + 28.0f) * k); px++) {
            float dx = ((float)px + 0.5f) / k - x, dy = ((float)py + 0.5f) / k - y - 4.0f,
                  a = 1.0f - (dx * dx + dy * dy) * ir, v;
            uint8_t *q;
            int lv;
            if (px < 0 || py < 0 || px >= s->w || py >= s->h || a <= 0.0f)
                continue;
            q = s->px + py * s->w + px;
            v = a * a * 3.2f;
            lv = (int)v;
            lv += r8_dith(px, py, (int)((v - (float)lv) * 16.0f));
            if (lv <= 0)
                continue;
            if (*q >= KP_GROUND && *q < KP_GROUND + 8)
                *q = (uint8_t)(KP_LAMP + (lv > 3 ? 3 : lv) - 1);
            else if (*q >= KP_STONE + 3 && *q < KP_STONE + 8 && lv >= 2 && a < 0.8f)
                *q = KP_LAMP + 5;                     /* (a stone's face toward it; not the lantern's own roof) */
        }
    for (py = (int32_t)((cy - 18.0f) * k); py <= (int32_t)((cy + 18.0f) * k); py++)   /* the water */
        for (px = (int32_t)((cx - 24.0f) * k); px <= (int32_t)((cx + 24.0f) * k); px++) {
            float xw = ((float)px + 0.5f) / k, yw = ((float)py + 0.5f) / k, dx = (xw - cx) / 24.0f, dy = (yw - cy) / 18.0f,
                  ex = (xw - x) * (1.0f / 70.0f), ey = (yw - y) * (1.0f / 70.0f),
                  a = (1.0f - dx * dx - dy * dy) * (1.2f - ex * ex - ey * ey) * (0.75f + 0.25f * lv),
                  crest = fm_sin_turns(yw * 0.42f + 0.6f * fm_sin_turns(xw * 0.045f + kp.t * 0.11f) - kp.t * 0.32f),
                  brk = fm_sin_turns(xw * 0.13f - yw * 0.05f + kp.t * 0.21f);
            uint8_t *q;
            if (px < 0 || py < 0 || px >= s->w || py >= s->h || a <= 0.12f || crest < 0.55f - 0.25f * a || brk < -0.4f)
                continue;
            q = s->px + py * s->w + px;
            if (*q < KP_DEEP + 32 || (*q >= KP_REFL && *q < KP_REFL + 32))
                *q = (uint8_t)(a > 0.5f && crest > 0.85f ? KP_LAMP + 4 : KP_LAMP + 3);
        }
}

/* a stone lantern on the moss, seen from above: its shadow; its roof (facets lit from the upper left, the corners
 * turned up, the jewel on top); its firebox under the roof toward us, lit at night (a flame in it, the roof's
 * underside warm along its near rim). Two kinds: the tall kasuga (eight facets) at the bottom left; the low
 * yukimi (a wide six-sided roof on three splayed legs) at the top right */
SC_SMALL static void kp_toro_draw(surf_t *s, float k, float x, float y, int yuki)
{
    float nt = kp_night(), r = yuki ? 10.0f : 7.5f, n = yuki ? 6.0f : 8.0f;
    int f;
    r8_disc(s, (x + 3.5f) * k, (y + 4.5f) * k, (r + 1.5f) * k, (uint8_t)(KP_GROUND + 7));   /* its shadow */
    if (yuki)
        for (f = 0; f < 3; f++) {                     /* its legs' feet, splayed out under the roof */
            float a = 0.18f + (float)f / 3.0f;
            r8_disc(s, (x + 11.5f * fm_sin_turns(a + 0.25f)) * k, (y + 11.5f * fm_sin_turns(a)) * k, 1.6f * k,
                    (uint8_t)(KP_STONE + 2));
        }
    r8_quad(s, x * k, (y + r - 0.5f) * k, 5.0f * k, 3.4f * k, 0.0f, (uint8_t)(KP_STONE + 1));   /* the firebox's
                                                       * frame, then its opening: stone by day, lit with the dark */
    r8_quad(s, x * k, (y + r - 0.5f) * k, 3.6f * k, 2.2f * k, 0.0f, (uint8_t)(KP_NITE + 3));
    if (nt > 0.3f)
        r8_disc(s, x * k, (y + r - 0.2f) * k, 0.9f * k, KP_LAMP + 6);   /* (the flame) */
    for (f = 0; f < (int)n; f++) {                    /* the roof (kasa) from above, facets rising to the jewel */
        float a0 = (float)f / n + 0.5f / n, a1 = a0 + 1.0f / n, am = a0 + 0.5f / n,
              lit = fm_sin_turns(am + 0.25f) + fm_sin_turns(am);   /* (facing the upper left: lit) */
        r8_tri(s, x * k, y * k, (x + r * fm_sin_turns(a0 + 0.25f)) * k, (y + r * fm_sin_turns(a0)) * k,
               (x + r * fm_sin_turns(a1 + 0.25f)) * k, (y + r * fm_sin_turns(a1)) * k,
               (uint8_t)(KP_STONE + (lit < -0.5f ? 1 : lit < 0.5f ? 3 : 5)));
        r8_pset(s, (int32_t)((x + (r + 0.7f) * fm_sin_turns(a0 + 0.25f)) * k), (int32_t)((y + (r + 0.7f) * fm_sin_turns(a0)) * k),
                KP_STONE + 2);
    }
    if (nt > 0.15f)                                   /* the roof's near rim, lit from under it */
        for (f = 0; f < 14; f++) {
            float a = 0.13f + 0.24f * (float)f / 13.0f;
            r8_pset(s, (int32_t)((x + (r - 0.6f) * fm_sin_turns(a + 0.25f)) * k), (int32_t)((y + (r - 0.6f) * fm_sin_turns(a)) * k),
                    KP_LAMP + 5);
        }
    r8_disc(s, x * k, y * k, (yuki ? 1.5f : 2.0f) * k, (uint8_t)(KP_STONE + 3));   /* the jewel on top (hoju) */
    r8_disc(s, (x - 0.5f) * k, (y - 0.6f) * k, (yuki ? 0.7f : 1.0f) * k, (uint8_t)(KP_STONE + 1));
}

/* the moon on the water at night (a Night mood): a pale disc, its rows wavering and broken by the
 * ripples, coming up with the dark (its color from the water's) */
static void kp_moon_draw(surf_t *s, float k)
{
    float cx = 152.0f, cy = 82.0f, r = 7.5f;
    int32_t y;
    if (kp_night() < 0.05f)
        return;
    for (y = (int32_t)((cy - r) * k); y <= (int32_t)((cy + r) * k); y++) {
        float wy = ((float)y + 0.5f) / k, dy = (wy - cy) / r, hw, ox;
        int32_t x, xa, xb;
        uint8_t *row;
        if (y < 0 || y >= s->h || dy * dy >= 1.0f || fm_sin_turns(wy * 0.31f + kp.t * 0.23f) < -0.75f)
            continue;                                 /* (a ripple's gap) */
        hw = r * fm_sqrtf(1.0f - dy * dy) * (1.0f + 0.15f * fm_sin_turns(wy * 0.17f - kp.t * 0.3f));
        ox = 1.3f * fm_sin_turns(wy * 0.42f + kp.t * 0.7f);
        xa = (int32_t)((cx + ox - hw) * k + 0.5f);
        xb = (int32_t)((cx + ox + hw) * k - 0.5f);
        row = s->px + y * s->w;
        for (x = xa < 0 ? 0 : xa; x <= xb && x < s->w; x++)
            if (row[x] < KP_DEEP + 32 || (row[x] >= KP_REFL && row[x] < KP_REFL + 32))
                row[x] = KP_NITE + 4;
    }
}

/* Feeding's bright midday: glints winking on the water here and there */
static void kp_glints_draw(surf_t *s, float k)
{
    int i, n = (int)(kp.feed * 16.0f);
    for (i = 0; i < n; i++) {
        float ph = kp.t * 0.9f + (float)i * 0.37f, u = ph - fm_floorf(ph);
        uint32_t h = r8_mix32((uint32_t)i * 2654435761u + 7u, (uint32_t)(int32_t)fm_floorf(ph));
        float a = (float)(h & 1023u) * (1.0f / 1024.0f), r = (KP_R - 8.0f) * fm_sqrtf((float)((h >> 10) & 1023u) * (1.0f / 1024.0f));
        int32_t gx = (int32_t)((KP_CX + r * fm_sin_turns(a + 0.25f)) * k), gy = (int32_t)((KP_CY + r * fm_sin_turns(a)) * k);
        uint8_t *q;
        if (u > 0.25f || gx < 2 || gy < 2 || gx >= s->w - 2 || gy >= s->h - 2)
            continue;
        q = s->px + gy * s->w + gx;
        if (*q >= KP_DEEP + 32 && !(*q >= KP_REFL && *q < KP_REFL + 32))
            continue;                                 /* (on open water only) */
        *q = KP_RIP;
        if (u > 0.06f && u < 0.18f) {                 /* (at its brightest: a little star) */
            q[-1] = q[1] = KP_RIP + 1;
            q[-s->w] = q[s->w] = KP_RIP + 1;
        }
    }
}

static void kp_palette(void)
{
    int i;
    r8_pal_ramp(KP_DEEP, 32, r8_mix(0x0D3A40u, 0x2C3A40u, 0.35f * kp.rain),   /* the deep middle to the green shallows
                                                                              * (a koi pond's clear green); grayer in
                                                                              * rain */
                r8_mix(0x6AAE98u, 0x7A8C90u, 0.4f * kp.rain));
    r8_pal_ramp(KP_GROUND, 8, 0x5C7A3Cu, 0x2E4220u);
    r8_pal_ramp(KP_STONE, 8, 0xB9B4A8u, 0x5E5A52u);
    r8_pal_ramp(KP_LILY, 8, 0x6FB04Au, 0x2E6328u);
    r8_pal_ramp(KP_FLOWER, 4, 0xFFE0EBu, 0xE07A9Cu);
    r8_pal_ramp(KP_FLOWER + 4, 4, 0xFFF0A0u, 0xE8C040u);
    r8_pal_ramp(KP_BAMBOO, 8, 0xD6E29Au, 0x3A521Cu);   /* green bamboo, its light .. its nodes */
    {   /* the koi (KC_*) */
        static const uint32_t KC[20] = {0xFFFDF4u, 0xF4EEE0u, 0xD8D0C0u, 0xF06A3Au, 0xD9452Au, 0xA82E1Cu, 0x3A3A40u,
                                        0x1C1C22u, 0x0C0C10u, 0xFFF0A8u, 0xEDB43Au, 0xC48A22u, 0x8AA2BCu, 0x7088A6u,
                                        0xE8743Au, 0xA27A4Cu, 0x7A5834u, 0xF2D248u, 0xEAEEF0u, 0xA6C2BCu};
        for (i = 0; i < 20; i++)
            r8_pal_set(KP_WHITE + i, KC[i]);
    }
    {   /* the garden: iris and sedge, ferns, an iris's flowers; the pond's gravel floor; the maple over it, a leaf */
        static const uint32_t PL[8] = {0x7CA84Eu, 0x557F36u, 0x35552Au, 0x7EA44Au, 0x4E7232u, 0x6A4EA8u, 0x9C86D8u,
                                       0xF2D24Au};
        static const uint32_t TR[8] = {0x4A3A2Eu, 0x6A5440u, 0xA8CC5Eu, 0x7EAA40u, 0x55822Eu, 0xC0943Au, 0x8AA840u,
                                       0x3A2E24u};
        for (i = 0; i < 8; i++) {
            r8_pal_set(KP_PLANT + i, PL[i]);
            r8_pal_set(KP_TREE + i, TR[i]);
        }
        r8_pal_set(KP_GRAV, r8_mix(0x5E9688u, 0x7A8C90u, 0.4f * kp.rain));
        r8_pal_set(KP_GRAV + 1, r8_mix(0x4E887Cu, 0x6A7C80u, 0.4f * kp.rain));
        r8_pal_set(KP_GRAV + 2, r8_mix(0x3E7068u, 0x56686Cu, 0.4f * kp.rain));
        r8_pal_set(KP_GRAV + 3, r8_mix(0x6CA08Eu, 0x8A9CA0u, 0.4f * kp.rain));
    }
    r8_pal_ramp(KP_FOOD, 4, 0xC8853Au, 0x8A5A26u);   /* pellets: warm brown, unlike the light on the water */
    r8_pal_ramp(KP_PETAL, 4, 0xFFD3E2u, 0xD95A85u);
    r8_pal_ramp(KP_PEL, 2, 0xD8AE74u, 0xA27A48u);   /* the pond's own pellets: paler, smaller (yours stand out) */
    r8_pal_ramp(KP_RIP, 8, 0xE6F6F4u, 0x4E8E92u);
    for (i = 0; i < 32; i++)                          /* the pond with a cloud's light on it */
        r8_pal_set(KP_REFL + i, r8_mix(r8_base[KP_DEEP + i], 0xE6F2F6u, 0.28f));   /* (fainter) */
    for (i = 0; i < 20; i++) {                        /* the koi under the water: their colors toward the pond's */
        r8_pal_set(KP_MID + i, r8_mix(r8_base[KP_WHITE + i], 0x1F5E6Au, 0.32f));
        r8_pal_set(KP_LOW + i, r8_mix(r8_base[KP_WHITE + i], 0x174C58u, 0.58f));
    }
    for (i = 0; i < KP_NFROG; i++) {                  /* each frog in its species' colors */
        int sp = kp_fg[i].sp, c = KP_FROGC(i);
        r8_pal_set(c + KF_BODY, KP_SPECIES[sp].body);
        r8_pal_set(c + KF_DARK, KP_SPECIES[sp].dark);
        r8_pal_set(c + KF_THROAT, KP_SPECIES[sp].throat);
        r8_pal_set(c + KF_PUPIL, 0x0C0C08u);
        r8_pal_set(c + KF_LINE, r8_mix(KP_SPECIES[sp].body, 0xFFFFF0u, 0.35f));   /* (its wet back's shine) */
        r8_pal_set(c + KF_STRIPE, KP_SPECIES[sp].stripe);
        r8_pal_set(c + KF_IRIS, KP_SPECIES[sp].iris);
        r8_pal_set(c + KF_SPOT, KP_SPECIES[sp].spot);
    }
    r8_pal_ramp(KP_SUB, 4, 0x3A6A70u, 0x2A5258u);     /* under the water: stones, then plants */
    r8_pal_ramp(KP_SUB + 4, 4, 0x2E6A50u, 0x1E4A3Au);
    r8_pal_set(KP_SKY, 0xC8DCE4u);                    /* the sky's light on the water (over a koi) */
    {   /* the day, as Sakura's */
        float sun = fm_sin_turns(kp.day), glow = 1.0f - fm_fabsf(sun);
        uint32_t m = sun >= 0.0f ? 0xFFFFFFu
                                 : ((uint32_t)(255.0f + 170.0f * sun) << 16 | (uint32_t)(255.0f + 150.0f * sun) << 8 |
                                    (uint32_t)(255.0f + 70.0f * sun));
        r8_pal_commit(m, 0xFFA066u, (int)(60.0f * glow * glow));
        {   /* the night: the koi pale shapes in the dark water; the lantern lit */
            float nt = kp_night();
            if (nt > 0.01f) {
                uint32_t mm = r8_mix(m, 0xFFFFFFu, 0.5f * nt), ml = r8_mix(m, 0xFFFFFFu, 0.2f * nt);
                r8_pal_commit_range(KP_DEEP, 32, ml, 0x7088B0u, (int)(22.0f * nt));   /* (moonlight on the water) */
                r8_pal_commit_range(KP_REFL, 32, ml, 0x7088B0u, (int)(22.0f * nt));
                r8_pal_commit_range(KP_GRAV, 4, ml, 0x7088B0u, (int)(22.0f * nt));
                r8_pal_commit_range(KP_STONE, 8, ml, 0x8090B0u, (int)(30.0f * nt));   /* (the stones catch it) */
                r8_pal_commit_range(KP_WHITE, 20, mm, 0xC4D0DCu, (int)(110.0f * nt));
                r8_pal_commit_range(KP_MID, 40, mm, 0xA8B8C8u, (int)(100.0f * nt));
            }
            {   /* (every light comes up with the dark, from the color it lies on: no switching) */
                static const uint32_t TO[5] = {0xFFD088u, 0x7A5230u, 0x9C7448u, 0xFFD088u, 0xE8E4D2u};
                uint32_t from[5];
                float f[5];
                int i;
                from[0] = from[3] = r8_base[KP_STONE + 6];
                from[1] = r8_base[KP_GROUND + 2];
                from[2] = from[4] = r8_base[KP_DEEP + 8];
                f[0] = f[1] = f[2] = f[3] = nt;
                f[4] = fm_clampf((nt - 0.2f) / 0.6f, 0.0f, 1.0f);
                for (i = 0; i < 5; i++) {
                    r8_pal_set(KP_NITE + i, r8_mix(from[i], TO[i], f[i]));
                    r8_pal_commit_range(KP_NITE + i, 1, r8_mix(m, 0xFFFFFFu, f[i]), 0xFFA066u,
                                        (int)(60.0f * glow * glow * (1.0f - f[i])));
                }
                {   /* the lanterns' light, its own steps: warm on the moss (three), on the water (two), on a roof's
                     * rim; the flame, breathing */
                    static const uint8_t FROM[7] = {KP_GROUND + 2, KP_GROUND + 2, KP_GROUND + 2, KP_DEEP + 8, KP_DEEP + 8,
                                                    KP_STONE + 5, KP_STONE + 6};
                    static const uint32_t LT[7] = {0x6E5A30u, 0x9A7A40u, 0xC89A58u, 0xA87A48u, 0xF0B868u, 0xD8A468u,
                                                   0xFFF0C0u};
                    static const uint8_t AMT[7] = {60, 75, 85, 70, 92, 70, 100};
                    for (i = 0; i < 7; i++) {
                        float a = nt * (float)AMT[i] * 0.01f;
                        r8_pal_set(KP_LAMP + i, r8_mix(r8_base[FROM[i]], LT[i], a));
                        r8_pal_commit_range(KP_LAMP + i, 1, r8_mix(m, 0xFFFFFFu, a), 0xFFA066u,
                                            (int)(60.0f * glow * glow * (1.0f - a)));
                    }
                }
            }
        }
    }
}

/* a stone seen from above, irregular (its outline wobbling with the angle), lit from the upper left; a flat-topped
 * one has a pale top; moss on some; wet and darker where it meets the water */
SC_SMALL static void kp_stone(surf_t *s, float k, float sx, float sy, float sz, int flat, int moss, uint32_t h)
{
    int32_t x, y, x0 = (int32_t)((sx - sz * 1.3f) * k), x1 = (int32_t)((sx + sz * 1.3f) * k),
            y0 = (int32_t)((sy - sz * 1.3f) * k), y1 = (int32_t)((sy + sz * 1.3f) * k);
    float a1 = (float)(h & 255u) / 256.0f, a2 = (float)((h >> 8) & 255u) / 256.0f;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = ((float)x + 0.5f) / k - sx, dy = ((float)y + 0.5f) / k - sy, d = fm_sqrtf(dx * dx + dy * dy),
                  th = kp_atan2(dy, dx), rr = sz * (1.0f + 0.16f * fm_sin_turns(3.0f * th + a1) + 0.09f * fm_sin_turns(5.0f * th + a2)),
                  lit, wr;
            uint8_t *p;
            int c;
            uint32_t hh;
            if (x < 0 || y < 0 || x >= s->w || y >= s->h || d >= rr)
                continue;
            p = s->px + y * s->w + x;
            lit = (dx + dy) / (rr * 1.4f) + (d / rr) * 0.3f;   /* (the upper left lit, the rim a little darker) */
            c = 2 + (int)(lit * 2.2f + 0.5f) + (flat && d < rr * 0.55f ? -1 : 0);
            hh = ((uint32_t)x * 73856093u) ^ ((uint32_t)y * 19349663u) ^ h;
            if ((hh >> 7) % 7u == 0u)
                c++;                                  /* (its grain) */
            wr = kp_rim_at(((float)x + 0.5f) / k, ((float)y + 0.5f) / k);
            {
                float ex = ((float)x + 0.5f) / k - KP_CX, ey = ((float)y + 0.5f) / k - KP_CY;
                if (ex * ex + ey * ey < (wr + 1.5f) * (wr + 1.5f))
                    c += 2;                           /* (wet at the waterline) */
            }
            c = c < 1 ? 1 : c > 7 ? 7 : c;
            *p = (uint8_t)(KP_STONE + c);
            if (moss && dy < -rr * 0.1f && d < rr * 0.85f &&   /* (moss on its top: soft patches) */
                fm_sin_turns(dx * 0.11f + a1) * fm_sin_turns(dy * 0.13f + a2) + 0.25f * fm_sin_turns((dx - dy) * 0.3f + a1) > 0.25f)
                *p = (uint8_t)(KP_GROUND + 1 + ((hh >> 13) % 5u == 0u));
        }
}
SC_SMALL static void kp_background(void)
{
    surf_t *s = &r8_bg;
    float k = kp_scale();
    int32_t x, y, W = s->w, H = s->h;
    int i, j;
    sc_seed(kp.seed);
    for (y = 0; y < H; y++)                           /* the pond, deep in the middle, a gravel floor in the shallows,
                                                       * the sky's sheen on it toward the upper left; the moss round it */
        for (x = 0; x < W; x++) {
            float wx = ((float)x + 0.5f) / k - KP_CX, wy = ((float)y + 0.5f) / k - KP_CY, r = fm_sqrtf(wx * wx + wy * wy),
                  R = kp_rim(kp_atan2(wy, wx));
            uint8_t c;
            if (r < R) {
                float u = r / R, f = u * u * 31.0f * 16.0f, sh = fm_clampf((-wx - wy) / (R * 1.5f), 0.0f, 1.0f);
                int fi = (int)(f + sh * sh * 30.0f);
                uint32_t hh = ((uint32_t)x * 2654435761u) ^ ((uint32_t)y * 40503u);
                c = (uint8_t)(KP_DEEP + (fi >> 4) + r8_dith(x, y, fi & 15));
                if (c > KP_DEEP + 31)
                    c = KP_DEEP + 31;
                (void)hh;
            } else {
                c = (uint8_t)(KP_GROUND + ((sc_rnd() & 7u) < 5u ? 2 : (int)(sc_rnd() % 6u)));
            }
            s->px[y * W + x] = c;
        }
    for (i = 0; i < 9; i++) {                         /* under the water near the edge: smooth stones, dim with depth */
        float an = sc_frand(), R = kp_rim(an), rr = R - 12.0f - 24.0f * sc_frand(), sz = 2.5f + 4.0f * sc_frand();
        float sx = KP_CX + rr * fm_sin_turns(an + 0.25f), sy = KP_CY + rr * fm_sin_turns(an);
        r8_disc(s, sx * k, sy * k, sz * k, (uint8_t)(KP_SUB + (int)(sc_rnd() % 4u)));
        r8_disc(s, (sx - sz * 0.3f) * k, (sy - sz * 0.3f) * k, sz * 0.45f * k, (uint8_t)KP_SUB);
    }
    for (i = 0; i < 26; i++) {                        /* pebbles in the shallows near the edge, clear through the water:
                                                       * each rounded, lit on its upper left */
        float an = sc_frand(), rr = kp_rim(an) - 5.0f - 12.0f * sc_frand(), sz = 1.0f + 1.3f * sc_frand();
        float sx = KP_CX + rr * fm_sin_turns(an + 0.25f), sy = KP_CY + rr * fm_sin_turns(an);
        r8_disc(s, (sx + 0.4f) * k, (sy + 0.5f) * k, sz * k, (uint8_t)(KP_GRAV + 2));
        r8_disc(s, sx * k, sy * k, sz * 0.8f * k, (uint8_t)(KP_GRAV + (int)(sc_rnd() % 2u)));
        if (sz > 1.6f)
            r8_disc(s, (sx - sz * 0.3f) * k, (sy - sz * 0.3f) * k, sz * 0.35f * k, (uint8_t)(KP_GRAV + 3));
    }
    for (i = 0; i < 9; i++) {                         /* and waterweed, leaning in from the edge */
        float an = sc_frand(), rr = kp_rim(an) - 4.0f, len = 6.0f + 7.0f * sc_frand(), bend = (sc_frand() - 0.5f) * 0.03f;
        float sx = KP_CX + rr * fm_sin_turns(an + 0.25f), sy = KP_CY + rr * fm_sin_turns(an);
        float ex = KP_CX + (rr - len) * fm_sin_turns(an + bend + 0.25f), ey = KP_CY + (rr - len) * fm_sin_turns(an + bend);
        r8_limb(s, sx * k, sy * k, 2.6f * k, ex * k, ey * k, 1.2f * k, (uint8_t)(KP_SUB + 5 + (int)(sc_rnd() % 3u)));
    }
    {   /* the stones round the edge: big flat-topped ones (some reaching over the water, their shadow on it), smaller
         * rounded ones and pebbles between; never in front of the shishi-odoshi or the lantern */
        float th = sc_frand() * 0.02f;
        for (i = 0; i < 60 && th < 1.0f; i++) {
            float u = sc_frand(), sz = u < 0.3f ? 7.5f + 4.0f * sc_frand() : u < 0.75f ? 4.5f + 3.0f * sc_frand() : 2.2f + 2.0f * sc_frand(),
                  R = kp_rim(th), over = sz > 7.0f && sc_frand() < 0.4f, rr = R + (over ? 1.0f : sz * 0.6f),
                  sx = KP_CX + rr * fm_sin_turns(th + 0.25f), sy = KP_CY + rr * fm_sin_turns(th);
            uint32_t h = sc_rnd();
            if (over) {                               /* its shadow on the water, under its lip */
                int32_t px, py;
                for (py = (int32_t)((sy - sz) * k); py <= (int32_t)((sy + sz + 3.0f) * k); py++)
                    for (px = (int32_t)((sx - sz) * k); px <= (int32_t)((sx + sz + 3.0f) * k); px++) {
                        float dx = ((float)px + 0.5f) / k - sx - 2.0f, dy = ((float)py + 0.5f) / k - sy - 2.5f;
                        uint8_t *p;
                        if (px < 0 || py < 0 || px >= W || py >= H || dx * dx + dy * dy > sz * sz)
                            continue;
                        p = s->px + py * W + px;
                        if (*p < KP_DEEP + 32 || (*p >= KP_GRAV && *p < KP_GRAV + 4))
                            *p = (uint8_t)(KP_DEEP + 3);
                    }
            }
            r8_disc(s, (sx + 1.2f) * k, (sy + 1.6f) * k, sz * 1.05f * k, (uint8_t)(KP_GROUND + 7));   /* its shadow on the moss */
            kp_stone(s, k, sx, sy, sz, sz > 7.0f, sz > 4.0f && sc_frand() < 0.45f, h);
            th += (sz * 1.55f + 0.5f + 1.5f * sc_frand()) / (6.2832f * R);
        }
    }
    {   /* the garden: clumps of iris and sedge at the edge (sword leaves, a flower or two), ferns; clear of the
         * shishi-odoshi and the lantern */
        static const float AT[4] = {0.62f, 0.86f, 0.03f, 0.47f};
        for (i = 0; i < 4; i++) {
            float th = AT[i] + (sc_frand() - 0.5f) * 0.05f, R = kp_rim(th) + 9.0f + 4.0f * sc_frand(),
                  bx = KP_CX + R * fm_sin_turns(th + 0.25f), by = KP_CY + R * fm_sin_turns(th);
            if (i < 3) {                              /* iris: sword leaves fanning out, some over the water */
                int n = 8 + (int)(sc_rnd() % 5u);
                for (j = 0; j < n; j++) {
                    float a = th + 0.5f + (sc_frand() - 0.5f) * 0.55f, l = 9.0f + 9.0f * sc_frand();
                    r8_limb(s, bx * k, by * k, 1.3f * k, (bx + l * fm_sin_turns(a + 0.25f)) * k, (by + l * fm_sin_turns(a)) * k,
                            0.35f * k, (uint8_t)(KP_PLANT + (int)(sc_rnd() % 3u)));
                }
                for (j = 0; j < 2 && sc_frand() < 0.7f; j++) {
                    float a = th + 0.5f + (sc_frand() - 0.5f) * 0.4f, l = 7.0f + 6.0f * sc_frand(),
                          fx = bx + l * fm_sin_turns(a + 0.25f), fy = by + l * fm_sin_turns(a);
                    r8_disc(s, fx * k, fy * k, 2.2f * k, KP_PLANT + 5);
                    r8_disc(s, (fx - 0.5f) * k, (fy - 0.5f) * k, 1.2f * k, KP_PLANT + 6);
                    r8_pset(s, (int32_t)(fx * k), (int32_t)(fy * k), KP_PLANT + 7);
                }
            } else {                                  /* a fern: fronds, leaflets along each */
                int n = 5 + (int)(sc_rnd() % 3u), m;
                for (j = 0; j < n; j++) {
                    float a = (float)j / (float)n + sc_frand() * 0.05f, l = 9.0f + 5.0f * sc_frand();
                    float ex = bx + l * fm_sin_turns(a + 0.25f), ey = by + l * fm_sin_turns(a);
                    r8_limb(s, bx * k, by * k, 0.8f * k, ex * k, ey * k, 0.3f * k, KP_PLANT + 4);
                    for (m = 1; m < 5; m++) {
                        float u = (float)m / 5.0f, cx = bx + (ex - bx) * u, cy = by + (ey - by) * u;
                        r8_disc(s, (cx - (ey - by) * 0.12f) * k, (cy + (ex - bx) * 0.12f) * k, (1.6f - u) * k, KP_PLANT + 3);
                        r8_disc(s, (cx + (ey - by) * 0.12f) * k, (cy - (ex - bx) * 0.12f) * k, (1.6f - u) * k, KP_PLANT + 3);
                    }
                }
            }
        }
    }
    {   /* the maple's branch, over the pond from the upper left or right corner: its limbs and leaf clusters kept
         * (drawn over the koi each frame), its shadow on the water here */
        float side = sc_frand() < 0.5f ? 1.0f : -1.0f, bx = side > 0.0f ? -6.0f : 246.0f, by = 8.0f + 20.0f * sc_frand(),
              ex = bx + side * (52.0f + 18.0f * sc_frand()), ey = by + 26.0f + 16.0f * sc_frand();
        kp_nbl = kp_nbf = 0;
        kp_bl[kp_nbl].x0 = bx;
        kp_bl[kp_nbl].y0 = by;
        kp_bl[kp_nbl].x1 = ex;
        kp_bl[kp_nbl].y1 = ey;
        kp_bl[kp_nbl++].w = 3.2f;
        for (i = 0; i < 4 && kp_nbl < KP_NBL; i++) {  /* twigs off it */
            float u = 0.35f + 0.6f * sc_frand(), sx = bx + (ex - bx) * u, sy = by + (ey - by) * u,
                  a = (side > 0.0f ? 0.0f : 0.5f) + (sc_frand() - 0.5f) * 0.35f + (i & 1 ? 0.08f : -0.08f), l = 14.0f + 14.0f * sc_frand();
            kp_bl[kp_nbl].x0 = sx;
            kp_bl[kp_nbl].y0 = sy;
            kp_bl[kp_nbl].x1 = sx + l * fm_sin_turns(a + 0.25f);
            kp_bl[kp_nbl].y1 = sy + l * fm_sin_turns(a);
            kp_bl[kp_nbl++].w = 1.4f;
        }
        for (i = 0; i < KP_NBF; i++) {                /* leaf clusters along the twigs and the branch's end */
            int b = 1 + (int)(sc_rnd() % (uint32_t)(kp_nbl - 1 > 0 ? kp_nbl - 1 : 1)), shade;
            float u = 0.3f + 0.8f * sc_frand();
            if (sc_frand() < 0.25f)
                b = 0, u = 0.6f + 0.45f * sc_frand();
            kp_bf[i].x = kp_bl[b].x0 + (kp_bl[b].x1 - kp_bl[b].x0) * u + (sc_frand() - 0.5f) * 8.0f;
            kp_bf[i].y = kp_bl[b].y0 + (kp_bl[b].y1 - kp_bl[b].y0) * u + (sc_frand() - 0.5f) * 8.0f;
            kp_bf[i].r = 1.6f + 1.6f * sc_frand();
            shade = 1 + (int)(sc_rnd() % 2u);
            kp_bf[i].c = (uint8_t)(KP_TREE + 2 + shade);
            kp_nbf++;
        }
        {   /* its shadow on the water (the sun high, upper left): every pixel under any leaf cluster's shadow made
             * darker once (overlapping leaves no darker) */
            float x0 = 999.0f, x1 = -999.0f, y0 = 999.0f, y1 = -999.0f;
            int32_t px, py;
            for (i = 0; i < kp_nbf; i++) {
                x0 = fm_minf(x0, kp_bf[i].x + 7.0f - kp_bf[i].r * 1.2f);
                x1 = fm_maxf(x1, kp_bf[i].x + 7.0f + kp_bf[i].r * 1.2f);
                y0 = fm_minf(y0, kp_bf[i].y + 10.0f - kp_bf[i].r * 1.2f);
                y1 = fm_maxf(y1, kp_bf[i].y + 10.0f + kp_bf[i].r * 1.2f);
            }
            for (py = (int32_t)(y0 * k); py <= (int32_t)(y1 * k); py++)
                for (px = (int32_t)(x0 * k); px <= (int32_t)(x1 * k); px++) {
                    float wx = ((float)px + 0.5f) / k, wy = ((float)py + 0.5f) / k;
                    uint8_t *p;
                    if (px < 0 || py < 0 || px >= W || py >= H)
                        continue;
                    p = s->px + py * W + px;
                    if (*p >= KP_DEEP + 32)
                        continue;
                    for (i = 0; i < kp_nbf; i++) {
                        float dx = wx - kp_bf[i].x - 7.0f, dy = wy - kp_bf[i].y - 10.0f, r = kp_bf[i].r * 1.2f;
                        if (dx * dx + dy * dy <= r * r) {
                            *p = (uint8_t)(*p > KP_DEEP + 3 ? *p - 3 : KP_DEEP);
                            break;
                        }
                    }
                }
        }
    }
    {   /* the shishi-odoshi's stone basin (tsukubai): a hand-cut stone, its carved bowl of water, a bamboo ladle
         * across it; the two bamboo posts (cut, hollow, seen from above), the stone the tube strikes */
        float o = KP_SH_DY, cx = 201.0f, cy = 35.0f + o;
        r8_disc(s, (cx + 2.0f) * k, (cy + 2.5f) * k, 12.0f * k, (uint8_t)(KP_GROUND + 7));
        kp_stone(s, k, cx, cy, 11.0f, 1, 0, 0x5A17u);
        r8_disc(s, cx * k, cy * k, 7.0f * k, KP_STONE + 6);   /* the bowl's lip */
        r8_disc(s, cx * k, cy * k, 6.0f * k, KP_DEEP + 10);   /* its water */
        r8_disc(s, (cx - 2.0f) * k, (cy - 2.0f) * k, 1.5f * k, KP_DEEP + 24);   /* a glint */
        r8_limb(s, (cx - 9.0f) * k, (cy + 5.0f) * k, 0.8f * k, (cx + 3.0f) * k, (cy - 1.0f) * k, 0.7f * k, KP_BAMBOO + 1);   /* the ladle */
        r8_disc(s, (cx + 4.0f) * k, (cy - 1.5f) * k, 2.0f * k, KP_BAMBOO + 3);
        r8_disc(s, (cx + 4.0f) * k, (cy - 1.5f) * k, 1.1f * k, KP_BAMBOO + 6);
        for (i = 0; i < 2; i++) {                     /* the posts, either side of the tube */
            float py = (i ? 9.0f : 23.0f) + o;
            r8_disc(s, 223.0f * k, (py + 1.5f) * k, 3.4f * k, (uint8_t)(KP_GROUND + 7));
            r8_disc(s, 222.0f * k, py * k, 3.2f * k, KP_BAMBOO + 3);
            r8_disc(s, 222.0f * k, py * k, 2.0f * k, KP_BAMBOO + 6);
            r8_disc(s, 222.0f * k, py * k, 1.2f * k, KP_BAMBOO + 7);
        }
        kp_stone(s, k, 234.0f, 17.0f + o, 4.0f, 1, 0, 0x2C31u);   /* the stone it knocks */
    }
    {   /* lily pads in a few clusters near the edge (drawn over the koi, each frame) */
        int nc = 2 + (int)(sc_rnd() % 2u);
        kp.npad = 0;
        for (i = 0; i < nc && kp.npad < KP_MAXPAD; i++) {
            float a = (float)i / (float)nc + sc_frand() * 0.15f, R = kp_rim(a), rr = R - 18.0f - 10.0f * sc_frand(),
                  cx = KP_CX + rr * fm_sin_turns(a + 0.25f), cy = KP_CY + rr * fm_sin_turns(a);
            int n = 1 + (int)(sc_rnd() % 3u);
            for (j = 0; j < n && kp.npad < KP_MAXPAD; j++) {
                float r = 6.0f + 6.0f * sc_frand(), oa = sc_frand();
                kp_pad[kp.npad].x = cx + (j ? (r + 4.0f) * fm_sin_turns(oa + 0.25f) : 0.0f);
                kp_pad[kp.npad].y = cy + (j ? (r + 4.0f) * fm_sin_turns(oa) : 0.0f);
                kp_pad[kp.npad].r = r;
                kp_pad[kp.npad].notch = sc_frand();
                kp_pad[kp.npad].flower = (sc_rnd() % 3u) == 0u ? 1 + (int)(sc_rnd() % 2u) : 0;
                kp_pad[kp.npad].shade = (uint8_t)(sc_rnd() % 3u);
                kp_pad[kp.npad].curl = (uint8_t)(sc_rnd() & 255u);
                kp_pad[kp.npad].dip = 0.0f;
                kp.npad++;
            }
        }
    }
    sc_seed(kp.seed * 2654435761u + 3u);              /* the world's randomness, apart from the picture's */
}

/* a koi's color at its depth: as it is near the surface, toward the pond's below. Between the three, a blend
 * ("they pop in and out"): *b shows on t16 of 16 pixels (dithered), *a on the rest */
static void kp_tints(uint8_t c, float z, uint8_t *a, uint8_t *b, int *t16)
{
    float lv = z < 0.2f ? 0.0f : z < 0.4f ? (z - 0.2f) * 5.0f : z < 0.52f ? 1.0f : z < 0.72f ? 1.0f + (z - 0.52f) * 5.0f : 2.0f;
    int l0 = (int)lv;
    static const uint8_t TIER[3] = {0, KP_MID - KP_WHITE, KP_LOW - KP_WHITE};
    if (c < KP_WHITE || c >= KP_WHITE + 20) {
        *a = *b = c;
        *t16 = 0;
        return;
    }
    if (l0 > 1)
        l0 = 1;
    *a = (uint8_t)(c + TIER[l0]);
    *b = (uint8_t)(c + TIER[l0 + 1]);
    *t16 = (int)((lv - (float)l0) * 16.0f);
}
static uint8_t kp_tint(uint8_t c, float z)       /* (one color: the nearer of the two) */
{
    uint8_t a, b;
    int t;
    kp_tints(c, z, &a, &b, &t);
    return t >= 8 ? b : a;
}
/* a disc in two colors, dithered (a koi's segment between two depths) */
static void kp_disc2(surf_t *s, float cx, float cy, float r, uint8_t a, uint8_t b, int t16)
{
    int32_t y, x, y0 = (int32_t)fm_floorf(cy - r), y1 = (int32_t)fm_floorf(cy + r);
    float r2 = r * r;
    if (t16 <= 0 || a == b) {
        r8_disc(s, cx, cy, r, a);
        return;
    }
    if (t16 >= 16) {
        r8_disc(s, cx, cy, r, b);
        return;
    }
    for (y = y0 < 0 ? 0 : y0; y <= y1 && y < s->h; y++) {
        float dy = (float)y + 0.5f - cy, w2 = r2 - dy * dy, w;
        int32_t xa, xb;
        uint8_t *p = s->px + y * s->w;
        if (w2 <= 0.0f)
            continue;
        w = fm_sqrtf(w2);
        xa = (int32_t)fm_floorf(cx - w + 0.5f);
        xb = (int32_t)fm_floorf(cx + w - 0.5f);
        for (x = xa < 0 ? 0 : xa; x <= xb && x < s->w; x++)
            p[x] = r8_dith(x, y, t16) ? b : a;
    }
}

/* a koi's color at a place on it: u along it (0 the snout .. 1 the tail's root), v across (-1 .. 1). Its variety's
 * ground, its patches and spots; rounded: its back a little lighter, its sides a little darker (KC_HI, KC_SH) */
static int kp_pat(const koi_t *q, float u, float v)
{
    float L = 21.0f * q->size, Wd = 4.2f * q->size, av = fm_fabsf(v);
    int c, i;
    switch (q->var) {
    case KV_SHOWA: c = KC_B1; break;
    case KV_ASAGI: {                                  /* blue-gray, a net of scales; orange-red along its sides; a pale
                                                       * head */
        float su = u * 20.0f, sv = v * 2.2f + ((int)su & 1) * 0.5f, fu = su - fm_floorf(su), fv = sv - fm_floorf(sv);
        c = fu < 0.18f || fv < 0.13f ? KC_BL1 : KC_BL0;   /* (the scales' edges, the rows staggered, close in color:
                                                       * a soft net, not a grid) */
        if (av > 0.68f && u > 0.12f)
            c = KC_OR;
        if (u < 0.13f)
            c = KC_W2;
        return c;
    }
    case KV_OGON: c = v > -0.3f && v < 0.15f ? KC_G0 : av > 0.8f ? KC_G2 : KC_G1; return c;   /* (the sheen down its
                                                       * back) */
    case KV_PLATINUM: c = v > -0.3f && v < 0.15f ? KC_W0 : av > 0.8f ? KC_W2 : KC_PL; return c;
    case KV_KIGOI: c = KC_YE; break;
    default: c = KC_W1; break;
    }
    for (i = 0; i < q->np; i++) {
        if (q->pl[i] < 0.0f) {                        /* a round spot (a tancho's) */
            float du = (u - q->pu[i]) * L, dv = (v - q->pv[i]) * Wd;
            if (du * du + dv * dv < q->pl[i] * q->pl[i])
                c = q->pc[i];
        } else {
            float hl = q->pl[i] * (1.0f + 0.14f * fm_sin_turns(v * 0.8f + q->pu[i] * 3.0f)),
                  du = (u - q->pu[i]) / hl, dv = (v - q->pv[i]) / (q->pw[i] + 1e-3f);
            if (du * du + dv * dv * 0.35f < 1.0f && dv * dv < 1.0f)
                c = q->pc[i];
        }
    }
    for (i = 0; i < q->ns; i++) {                     /* small black spots */
        float du = (u - q->su[i]) * L, dv = (v - q->sv[i]) * Wd;
        if (du * du + dv * dv < q->sr[i] * q->sr[i])
            c = KC_B1;
    }
    if (av > 0.8f)
        return KC_SH[c];
    if ((av < 0.25f && u > 0.14f && u < 0.62f) || u < 0.035f)
        return KC_HI[c];
    return c;
}
/* a fin: a triangle in the fin's color (pale, the water through it) at the depth's tint */
static void kp_fin(surf_t *s, float k, float ax, float ay, float bx, float by, float cx, float cy, int c, float z)
{
    r8_tri(s, ax * k, ay * k, bx * k, by * k, cx * k, cy * k, kp_tint((uint8_t)(KP_WHITE + c), z));
}
static void kp_draw_koi(surf_t *s, float k, koi_t *q, int shadow)
{
    int i;
    float off = shadow ? 1.0f + 4.5f * (1.0f - q->z) : 0.0f, zs = 1.0f - 0.15f * q->z,   /* (its shadow on the floor:
                                                                               * farther off the higher it swims) */
          sp = 3.0f * q->size, hov = q->st == 1 ? 1.0f : 0.0f;
    int finc = q->var == KV_OGON ? KC_G1 : q->var == KV_KIGOI ? KC_YE : KC_FIN;
    for (i = KP_NSEG - 1; i >= 0; i--) {              /* tail first, the head on top */
        int j = i ? i - 1 : 1;
        float dx = q->x[j] - q->x[i], dy = q->y[j] - q->y[i], l = fm_sqrtf(dx * dx + dy * dy) + 1e-3f;
        float u = (float)i / (float)(KP_NSEG - 1),   /* the wave: traveling back, growing with the square of the
                                                       * way along (the head steady, the tail driving), as big as
                                                       * the effort; the head a little the other way; the body bent
                                                       * into a turn, its tail swung out */
              sway = q->size * (2.4f * q->amp * fm_sin_turns(q->ph - (float)i * 0.13f) * u * u -
                                (i ? 0.0f : 0.35f * q->amp * fm_sin_turns(q->ph)) - 7.0f * fm_clampf(q->bend, -0.4f, 0.4f) * u * u);
        float px = q->x[i] + (-dy / l) * sway + off, py = q->y[i] + (dx / l) * sway + off * 1.3f;
        float w = KP_WIDTH[i] * q->size * zs, tx = i ? dx / l : -dx / l, ty = i ? dy / l : -dy / l, nx = -ty, ny = tx;
        float zi = fm_clampf(q->z - q->pitch * ((float)i / (float)(KP_NSEG - 1) - 0.5f), 0.0f, 1.0f);   /* (this
                                                                         * segment's own depth: head first) */
        if (i == KP_NSEG - 1) {                        /* the tail: two lobes, swinging, pale (shadow: one shape) */
            float sw = q->amp * (0.06f * fm_sin_turns(q->ph - 1.16f) + (q->bfly ? 0.04f * fm_sin_turns(q->ph - 1.36f) : 0.0f)) -
                       fm_clampf(q->bend, -0.4f, 0.4f),   /* (~22 degrees, a quarter beat behind the body: a flick) */
                  fl = (q->bfly ? 11.5f : 6.8f) * q->size, ax = -tx, ay = -ty, m;   /* (a butterfly's: long, trailing) */
            for (m = -1.0f; m <= 1.0f; m += 2.0f) {   /* (each lobe a fan of two: round, a shallow notch between) */
                float a1 = m * 0.15f + sw + 0.03f * q->amp * fm_sin_turns(q->ph - 1.4f), ca = fm_sin_turns(a1 + 0.25f),
                      sa = fm_sin_turns(a1),          /* (its tips trailing the root) */
                      ex = px + (ax * ca - ay * sa) * fl, ey = py + (ax * sa + ay * ca) * fl,
                      a2 = m * 0.02f + sw, cb = fm_sin_turns(a2 + 0.25f), sb = fm_sin_turns(a2),
                      mx = px + (ax * cb - ay * sb) * fl * 0.8f, my = py + (ax * sb + ay * cb) * fl * 0.8f,
                      a3 = m * 0.08f + sw, cc = fm_sin_turns(a3 + 0.25f), sc3 = fm_sin_turns(a3),
                      ox = px + (ax * cc - ay * sc3) * fl * 1.02f, oy = py + (ax * sc3 + ay * cc) * fl * 1.02f;
                if (shadow) {
                    r8_tri(s, px * k, py * k, ex * k, ey * k, ox * k, oy * k, KP_DEEP + 1);
                    r8_tri(s, px * k, py * k, ox * k, oy * k, mx * k, my * k, KP_DEEP + 1);
                } else {
                    kp_fin(s, k, px, py, ex, ey, ox, oy, finc, zi);
                    kp_fin(s, k, px, py, ox, oy, mx, my, finc, zi);
                }
            }
        }
        if (i == 2) {                                  /* the pectoral fins: large, pale, fluttering as it hovers */
            float bk = (q->bfly ? 5.0f : 3.2f) * q->size, m;
            for (m = -1.0f; m <= 1.0f; m += 2.0f) {   /* (tucked back along it when fast, spread when slow or
                                                       * braking, rowing left then right as it hovers) */
                float fl = ((q->bfly ? 6.0f : 3.6f) * (0.45f + 0.55f * q->spread) +
                            0.9f * hov * fm_sin_turns(q->fin + (m > 0.0f ? 0.0f : 0.5f))) * q->size,
                      ax = px + m * nx * w * 0.8f, ay = py + m * ny * w * 0.8f,
                      bx = ax + m * nx * fl - tx * bk * (1.6f - 0.8f * q->spread), by = ay + m * ny * fl - ty * bk * (1.6f - 0.8f * q->spread),
                      cx = ax - tx * bk, cy = ay - ty * bk;
                if (shadow)
                    r8_tri(s, ax * k, ay * k, bx * k, by * k, cx * k, cy * k, KP_DEEP + 1);
                else {
                    kp_fin(s, k, ax, ay, bx, by, cx, cy, finc, zi);
                    if (q->var == KV_SHOWA || q->var == KV_ASAGI)   /* (a showa's black fin
                                                       * root, an asagi's orange) */
                        kp_fin(s, k, ax, ay, ax + (bx - ax) * 0.45f, ay + (by - ay) * 0.45f, ax + (cx - ax) * 0.45f,
                               ay + (cy - ay) * 0.45f, q->var == KV_ASAGI ? KC_OR : KC_B1, zi);
                }
            }
        }
        if (shadow) {
            r8_disc(s, px * k, py * k, w * k, KP_DEEP + 1);
            continue;
        }
        {   /* its body here: each pixel its place on the koi (along, across), its pattern's color at its depth */
            int32_t x, y, y0 = (int32_t)fm_floorf((py - w) * k), y1 = (int32_t)fm_floorf((py + w) * k);
            float r2 = w * w, lv = zi < 0.2f ? 0.0f : zi < 0.4f ? (zi - 0.2f) * 5.0f : zi < 0.52f ? 1.0f : zi < 0.72f ? 1.0f + (zi - 0.52f) * 5.0f : 2.0f;
            int l0 = (int)lv > 1 ? 1 : (int)lv, t16 = (int)((lv - (float)l0) * 16.0f);
            static const uint8_t TIER[3] = {0, KP_MID - KP_WHITE, KP_LOW - KP_WHITE};
            for (y = y0 < 0 ? 0 : y0; y <= y1 && y < s->h; y++) {
                float oy = ((float)y + 0.5f) / k - py, w2 = r2 - oy * oy, ww;
                int32_t xa, xb;
                uint8_t *row = s->px + y * s->w;
                if (w2 <= 0.0f)
                    continue;
                ww = fm_sqrtf(w2);
                xa = (int32_t)fm_floorf((px - ww) * k + 0.5f);
                xb = (int32_t)fm_floorf((px + ww) * k - 0.5f);
                for (x = xa < 0 ? 0 : xa; x <= xb && x < s->w; x++) {
                    float ox = ((float)x + 0.5f) / k - px, along = ox * tx + oy * ty, acr = ox * nx + oy * ny,
                          u = ((float)i - along / sp) / (float)(KP_NSEG - 1);
                    int c = kp_pat(q, fm_clampf(u * 1.08f, 0.0f, 1.0f), fm_clampf(acr / (w + 1e-3f), -1.0f, 1.0f));
                    row[x] = (uint8_t)(KP_WHITE + c + TIER[r8_dith(x, y, t16) ? l0 + 1 : l0]);
                }
            }
        }
        if (i == 0) {                                  /* its eyes, and its mouth open at the surface */
            float m;
            for (m = -1.0f; m <= 1.0f; m += 2.0f)
                r8_pset(s, (int32_t)((px + tx * w * 0.35f + m * nx * w * 0.62f) * k), (int32_t)((py + ty * w * 0.35f + m * ny * w * 0.62f) * k),
                        kp_tint(KP_WHITE + KC_B2, zi));
            if (q->mouth > 0.3f)
                r8_disc(s, (px + tx * w * 0.95f) * k, (py + ty * w * 0.95f) * k, (0.6f + 0.6f * q->mouth) * q->size * k,
                        (uint8_t)(KP_DEEP + 1));
        }
    }
}

/* a lily pad's bob: what 0 its size, 1 x, 2 y; a little all the while, more in a breeze near it */
SC_SMALL static float kp_pad_bob(int i, int what)
{
    float bx = kp.br_t > 0.0f ? kp_pad[i].x - kp.br_x : 999.0f, by = kp.br_t > 0.0f ? kp_pad[i].y - kp.br_y : 999.0f,
          near = bx * bx + by * by < 2500.0f ? fm_sin_turns(0.5f * kp.br_t / kp.br_dur) : 0.0f;
    float dip = kp_pad[i].dip * fm_sin_turns(kp.t * 1.6f);   /* (a frog's weight: it rocks, settling) */
    if (what == 0)
        return 1.0f + 0.03f * fm_sin_turns(kp.t * 0.21f + (float)i * 0.31f) - 0.04f * kp_pad[i].dip;
    if (what == 1)
        return 0.5f * fm_sin_turns(kp.t * 0.13f + (float)i * 0.47f) + 1.6f * near * fm_sin_turns(kp.t * 0.6f + (float)i) + 0.8f * dip;
    return 0.4f * fm_sin_turns(kp.t * 0.17f + (float)i * 0.29f) + 0.8f * near * fm_sin_turns(kp.t * 0.5f + (float)i) + 0.6f * dip;
}

/* a frog's shadow: on the water (darker water) or on a pad (the pad's own shade); nothing else */
static void kp_frog_shadow(surf_t *s, float cx, float cy, float r)
{
    int32_t x, y;
    for (y = (int32_t)(cy - r); y <= (int32_t)(cy + r); y++)
        for (x = (int32_t)(cx - r); x <= (int32_t)(cx + r); x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            uint8_t *p;
            if (x < 0 || y < 0 || x >= s->w || y >= s->h || dx * dx + dy * dy > r * r)
                continue;
            p = s->px + y * s->w + x;
            if (*p < KP_DEEP + 32 || (*p >= KP_REFL && *p < KP_REFL + 32))
                *p = KP_DEEP + 1;
            else if (*p >= KP_LILY && *p < KP_LILY + 8)
                *p = KP_LILY + 7;
        }
}

/* a frog, seen from above, facing any way: on a pad (sitting, hind legs folded, its throat moving as it breathes
 * and puffing as it croaks, blinking now and then) or in the air (stretched, hind legs out behind, its shadow on the
 * water below). Shaded by its own colors, lit from the upper left (no outline): a darker green on its shadow side,
 * a wet shine on its back; raised eyes with a glint */
static void kp_frogs_draw(surf_t *s, float k)
{
    int i, j;
    for (i = 0; i < KP_NFROG; i++) {
        int sp = kp_fg[i].sp, air = kp_fg[i].st == 1 || kp_fg[i].st == 3;
        uint8_t c = (uint8_t)KP_FROGC(i);
        float x = kp_fg[i].x, y = kp_fg[i].y, fx = kp_fg[i].fx, fy = kp_fg[i].fy, S = 1.8f * KP_SPECIES[sp].size, lift = 0.0f,
              br = air ? 0.0f : 0.12f * fm_sin_turns(kp.t * 1.4f + (float)i);   /* (its flank, breathing) */
        uint32_t m = kp_fg[i].mark;
        if (!kp_fg[i].st)
            continue;
#define FP(a, b) (x + (a) * fx - (b) * fy) * k, (y + (a) * fy + (b) * fx) * k   /* along its body, across it */
        if (air) {                                    /* leaping: bigger at the top of the arc */
            float u = fm_minf(1.0f, kp_fg[i].t / 0.6f);
            lift = fm_sin_turns(u * 0.5f);
            kp_frog_shadow(s, (x + 1.0f + 3.0f * lift) * k, (y + 6.0f * lift + 1.5f + 3.0f * lift) * k,
                           2.4f * S * (1.0f - 0.25f * lift) * k);
            S *= 1.0f + 0.25f * lift;
            for (j = -1; j <= 1; j += 2) {            /* the hind legs stretched out behind, the front ones forward */
                float b = (float)j;
                r8_limb(s, FP(-4.2f * S, b * 1.9f * S), 0.6f * S * k, FP(-6.6f * S, b * 1.6f * S), 0.5f * S * k,
                        (uint8_t)(c + KF_DARK));
                r8_limb(s, FP(-1.4f * S, b * 1.3f * S), 1.0f * S * k, FP(-4.2f * S, b * 1.9f * S), 0.7f * S * k,
                        (uint8_t)(c + KF_BODY));
                r8_limb(s, FP(1.8f * S, b * 1.2f * S), 0.5f * S * k, FP(3.6f * S, b * 1.8f * S), 0.4f * S * k,
                        (uint8_t)(c + KF_BODY));
            }
        } else {
            kp_frog_shadow(s, (x - fx * 0.5f * S + 1.3f) * k, (y - fy * 0.5f * S + 1.5f) * k, 2.7f * S * k);
            for (j = -1; j <= 1; j += 2) {            /* the hind legs folded at its sides (thigh, then the foot back
                                                       * along it), the front ones */
                float b = (float)j;
                r8_limb(s, FP(0.4f * S, b * 2.9f * S), 0.55f * S * k, FP(-2.9f * S, b * 3.4f * S), 0.45f * S * k,
                        (uint8_t)(c + KF_DARK));      /* (the foot, under the thigh) */
                r8_limb(s, FP(-1.6f * S, b * 1.5f * S), 0.9f * S * k, FP(0.4f * S, b * 2.9f * S), 0.65f * S * k,
                        (uint8_t)(c + KF_BODY));
                r8_limb(s, FP(1.9f * S, b * 1.3f * S), 0.55f * S * k, FP(3.3f * S, b * 2.4f * S), 0.45f * S * k,
                        (uint8_t)(c + KF_BODY));
            }
        }
        r8_disc(s, (x - fx * 0.5f * S + (0.4f + br) * S) * k, (y - fy * 0.5f * S + (0.5f + br) * S) * k, 2.4f * S * k,
                (uint8_t)(c + KF_DARK));              /* its shadow side, then its back */
        r8_disc(s, FP(-0.5f * S, 0.0f), 2.4f * S * k, (uint8_t)(c + KF_BODY));
        r8_disc(s, (x + fx * 2.2f * S + 0.3f * S) * k, (y + fy * 2.2f * S + 0.35f * S) * k, 1.8f * S * k,
                (uint8_t)(c + KF_DARK));
        r8_disc(s, FP(2.2f * S, 0.0f), 1.8f * S * k, (uint8_t)(c + KF_BODY));
        r8_disc(s, FP(3.3f * S, 0.0f), 0.5f * S * k, (uint8_t)(c + KF_THROAT));   /* its pale lip */
        r8_disc(s, FP(2.5f * S, 0.0f), 1.3f * S * k, (uint8_t)(c + KF_BODY));
        if (KP_SPECIES[sp].stripes)                   /* a pale line down its back */
            r8_limb(s, FP(-2.6f * S, 0.0f), 0.7f * S * k, FP(3.4f * S, 0.0f), 0.5f * S * k, (uint8_t)(c + KF_STRIPE));
        if (KP_SPECIES[sp].spots) {                   /* its spots, its own (or a wrinkled frog's many warts) */
            int n = KP_SPECIES[sp].spots == 2 ? 9 : 5;
            float rs = KP_SPECIES[sp].spots == 2 ? 0.4f : 0.55f;
            for (j = 0; j < n; j++) {
                float a = (float)(m & 255u) * (1.0f / 256.0f), d = 0.35f + 0.6f * (float)((m >> 8) & 15u) * (1.0f / 16.0f),
                      sa = -0.5f * S + 2.0f * S * d * fm_sin_turns(a + 0.25f), sb = 2.0f * S * d * fm_sin_turns(a);
                m = m * 1664525u + 1013904223u;
                if (KP_SPECIES[sp].stripes && fm_fabsf(sb) < 0.6f * S)
                    continue;                         /* (clear of the line) */
                r8_disc(s, FP(sa, sb), rs * S * k, (uint8_t)(c + KF_SPOT));
            }
        }
        r8_disc(s, (x - fx * 0.8f * S - 0.7f * S) * k, (y - fy * 0.8f * S - 0.8f * S) * k, 0.6f * S * k,
                (uint8_t)(c + KF_LINE));              /* the light on its wet back */
        if (!air && kp_fg[i].puff > 0.05f) {          /* its throat, puffing as it croaks: a pond frog's two sacs at
                                                       * the corners of its mouth, a tree frog's or a bullfrog's one */
            float r = (0.5f + 1.2f * kp_fg[i].puff) * S;
            if (KP_SPECIES[sp].stripes)
                for (j = -1; j <= 1; j += 2)
                    r8_disc(s, FP(2.2f * S, (float)j * 1.9f * S), r * k, (uint8_t)(c + KF_THROAT));
            else
                r8_disc(s, FP(3.6f * S, 0.0f), r * 1.2f * k, (uint8_t)(c + KF_THROAT));
        }
        for (j = -1; j <= 1; j += 2) {                /* its eyes, raised on top of its head: a glint in each, shut
                                                       * for a moment when it blinks */
            float ea = 2.5f * S, eb = (float)j * 1.35f * S, ex = x + ea * fx - eb * fy, ey = y + ea * fy + eb * fx;
            if (KP_SPECIES[sp].eyeline)               /* a tree frog's dark line from its nose through its eye */
                r8_limb(s, FP(4.0f * S, (float)j * 0.9f * S), 0.5f * S * k, FP(0.8f * S, (float)j * 1.9f * S), 0.6f * S * k,
                        (uint8_t)(c + KF_SPOT));
            r8_disc(s, (ex + 0.25f * S) * k, (ey + 0.3f * S) * k, 0.95f * S * k, (uint8_t)(c + KF_DARK));
            if (!air && kp_fg[i].blink < 0.15f) {
                r8_disc(s, ex * k, ey * k, 0.8f * S * k, (uint8_t)(c + KF_BODY));   /* (its lid) */
                continue;
            }
            r8_disc(s, ex * k, ey * k, 0.75f * S * k, (uint8_t)(c + KF_IRIS));
            r8_pset(s, (int32_t)(ex * k), (int32_t)(ey * k), (uint8_t)(c + KF_PUPIL));
            if (S * k > 1.6f)
                r8_pset(s, (int32_t)((ex - 0.45f * S) * k), (int32_t)((ey - 0.45f * S) * k), (uint8_t)(c + KF_THROAT));
        }
#undef FP
    }
}

/* the sky on the water: the reflections of clouds drifting over the pond, the water a little lighter in them,
 * a few of a koi's pixels caught in the light; and a breeze's patch of fine ripples now and then */
static void kp_sky_draw(surf_t *s, float k)
{
    int i;
    int32_t x, y;
    for (i = 0; i < KP_NREFL; i++) {
        float cx = kp_sk[i].x * k, cy = kp_sk[i].y * k, rx = kp_sk[i].rx * k, ry = kp_sk[i].ry * k, irx[5], iry[5],
              qx[5], qy[5];
        int32_t y0 = (int32_t)(cy - ry * 1.6f), y1 = (int32_t)(cy + ry * 1.6f), j;   /* (every puff's rows) */
        for (j = 0; j < 5; j++) {
            qx[j] = cx + kp_sk[i].px[j] * rx;
            qy[j] = cy + kp_sk[i].py[j] * ry;
            irx[j] = 1.0f / (kp_sk[i].pr[j] * rx);
            iry[j] = 1.0f / (kp_sk[i].pr[j] * ry * 1.4f);
        }
        for (y = y0 < 0 ? 0 : y0; y <= y1 && y < s->h; y++) {
            uint8_t *row = s->px + y * s->w;
            float dy[5], u[5], u2[5];
            int32_t xa = s->w, xb = -1;
            for (j = 0; j < 5; j++) {                 /* (each puff's own span on this row: no work outside them) */
                dy[j] = ((float)y + 0.5f - qy[j]) * iry[j];
                dy[j] = 1.0f - dy[j] * dy[j];
                if (dy[j] > 0.0f) {
                    float w = fm_sqrtf(dy[j]) / irx[j];
                    int32_t a0 = (int32_t)(qx[j] - w), a1 = (int32_t)(qx[j] + w) + 1;
                    xa = a0 < xa ? a0 : xa;
                    xb = a1 > xb ? a1 : xb;
                }
            }
            if (xa < 0)
                xa = 0;
            if (xb >= s->w)
                xb = s->w - 1;
            for (j = 0; j < 5; j++) {                 /* (stepped across the row: adds, no work a pixel per puff) */
                u[j] = ((float)xa + 0.5f - qx[j]) * irx[j];
                u2[j] = u[j] * u[j];
            }
            for (x = xa; x <= xb; x++) {
                float a = 0.0f;
                for (j = 0; j < 5; j++) {             /* (the brightest of its puffs here) */
                    float bb = dy[j] - u2[j];
                    a = bb > a ? bb : a;
                    u2[j] += (2.0f * u[j] + irx[j]) * irx[j];
                    u[j] += irx[j];
                }
                if (a <= 0.0f)
                    continue;
                {
                    uint8_t v = row[x];
                    if (v < KP_DEEP + 32) {           /* the pond, whiter in the middle; toward the edge only a shade
                                                       * or two lighter, so it softens out without speckle */
                        int t = (int)(a * 2.6f * (1.0f - 0.6f * kp.rain) * 16.0f);   /* (dim in rain) */
                        if (t >= 16)
                            row[x] = (uint8_t)(KP_REFL + v);
                        else {
                            int lift = (t >> 3) + r8_dith(x, y, (t & 7) * 2);
                            row[x] = (uint8_t)(v + lift > KP_DEEP + 31 ? KP_DEEP + 31 : v + lift);
                        }
                    } else if (((v >= KP_WHITE && v < KP_WHITE + 20) || (v >= KP_MID && v < KP_LOW + 20)) &&
                               r8_dith(x, y, (int)(a * 3.0f))) {
                        row[x] = KP_SKY;              /* (a few of a koi's pixels: the sky over it) */
                    }
                }
            }
        }
    }
    if (kp.br_t > 0.0f) {                             /* a breeze: fine ripples across a drifting patch */
        float e = fm_sin_turns(0.5f * kp.br_t / kp.br_dur), cx = kp.br_x * k, cy = kp.br_y * k, rx = 30.0f * k, ry = 16.0f * k;
        int32_t x0 = (int32_t)(cx - rx), x1 = (int32_t)(cx + rx), y0 = (int32_t)(cy - ry), y1 = (int32_t)(cy + ry);
        int ph = (int)(kp.t * 6.0f);
        for (y = y0 < 0 ? 0 : y0; y <= y1 && y < s->h; y++) {
            float dy = ((float)y + 0.5f - cy) / ry;
            uint8_t *row = s->px + y * s->w;
            if ((((int)((float)y / k) + ph) % 3) != 0 || dy * dy >= 1.0f)
                continue;                             /* (a ripple every third row, the rows moving) */
            for (x = x0 < 0 ? 0 : x0; x <= x1 && x < s->w; x++) {
                float dx = ((float)x + 0.5f - cx) / rx, a = (1.0f - dx * dx - dy * dy) * e;
                if (a > 0.0f && row[x] < KP_DEEP + 30 && r8_dith(x, y, (int)(a * 12.0f)))
                    row[x] = (uint8_t)(row[x] + 2);
            }
        }
    }
}

static void kp_draw(void)
{
    surf_t *s = &r8_fb;
    float k = kp_scale();
    int i;
    SC_PT_TAGS("lkpso");                             /* (testing builds: the HUD's parts) */
    SC_PT_START();
    r8_copy(s, &r8_bg);
    for (i = 0; i < KP_NLIGHT; i++) {                 /* light on the floor (the patches "look like
                                                       * clouds"): thin wavy lines of light in the shallows near the rim,
                                                       * wobbling and sliding slowly round, unbroken (a point a pixel),
                                                       * fading at their ends: the ripples' light on a pond's bottom */
        float r0 = kp_lt[i].r, th0 = kp_lt[i].th, len = kp_lt[i].len, ph = kp_lt[i].ph, kw = kp_lt[i].kw,
              b = (0.7f + 0.3f * fm_sin_turns(kp.t * 0.04f + ph)) * (1.0f - 0.35f * kp.still - 0.5f * kp.rain);   /* (soft
                                                       * in Still Pond and rain) */
        int32_t j, n = (int32_t)(r0 * len * 6.2832f * k * 1.5f) + 2, lx = -1, ly = -1;
        for (j = 0; j <= n; j++) {
            float u = (float)j / (float)n, th = th0 + len * u,
                  r = r0 + 2.2f * fm_sin_turns(th * kw + ph) + 1.2f * fm_sin_turns(th * kw * 2.3f - ph * 1.7f),
                  e = fm_sin_turns(u * 0.5f) * b;     /* (fading at its ends) */
            int32_t x = (int32_t)((KP_CX + r * fm_sin_turns(th + 0.25f)) * k), y = (int32_t)((KP_CY + r * fm_sin_turns(th)) * k), t;
            uint8_t *q;
            if ((x == lx && y == ly) || x < 0 || y < 0 || x >= s->w || y >= s->h)
                continue;
            lx = x;
            ly = y;
            q = s->px + y * s->w + x;
            if (*q >= KP_DEEP + 29)
                continue;                             /* (the pond only) */
            t = (int)(e * 4.4f * 16.0f);                 /* (brighter could "just barely see" them) */
            t = *q + (t >> 4) + r8_dith(x, y, t & 15);
            *q = (uint8_t)(t > KP_DEEP + 31 ? KP_DEEP + 31 : t);
            if (e > 0.55f && y + 1 < s->h && s->px[(y + 1) * s->w + x] < KP_DEEP + 29)   /* (a pixel wider at its middle) */
                s->px[(y + 1) * s->w + x]++;
        }
    }
    SC_PT(0);
    for (i = 0; i < KP_MAXR; i++)
        if (kp_r[i].t > 0.0f && kp_r[i].t < 2.0f) {
            float u = kp_r[i].t / 2.0f, rr = 2.0f + 16.0f * u;
            r8_ring(s, kp_r[i].x * k, kp_r[i].y * k, rr * k, rr * k, (uint8_t)(KP_RIP + (int)(u * 7.0f)));
        }
    for (i = 0; i < KP_NRAIN; i++)                    /* rain: small rings from where drops land, a splash at
                                                       * each one's center for a moment */
        if (kp_rd[i].t > 0.0f && !kp_rd[i].pad) {
            float u = 1.0f - kp_rd[i].t / 0.9f, rr = (1.0f + 6.0f * u) * kp_rd[i].sz;
            r8_ring(s, kp_rd[i].x * k, kp_rd[i].y * k, rr * k, rr * k, (uint8_t)(KP_RIP + 1 + (int)(u * 6.0f)));
            if (u < 0.15f)
                r8_pset(s, (int32_t)(kp_rd[i].x * k), (int32_t)(kp_rd[i].y * k), KP_RIP);
        }
    {   /* the koi's shadows on the floor, then the koi, the deepest first (depth) */
        int ord[KP_MAXK], n = 0, a, b;
        for (i = 0; i < KP_MAXK; i++)
            if (kp_k[i].on)
                ord[n++] = i;
        for (a = 1; a < n; a++)
            for (b = a; b > 0 && kp_k[ord[b - 1]].z < kp_k[ord[b]].z; b--) {
                int t = ord[b];
                ord[b] = ord[b - 1];
                ord[b - 1] = t;
            }
        for (i = 0; i < n; i++)
            kp_draw_koi(s, k, &kp_k[ord[i]], 1);
        for (i = 0; i < n; i++)
            kp_draw_koi(s, k, &kp_k[ord[i]], 0);
    }
    SC_PT(1);
    for (i = 0; i < KP_NLEAF; i++)                    /* leaves floating on the water */
        r8_quad(s, kp_lf[i].x * k, kp_lf[i].y * k, 2.6f * k, 1.4f * k, kp_lf[i].a, kp_lf[i].c);
    for (i = 0; i < kp.npad; i++) {                   /* lily pads, over the koi (they swim under); bobbing, more in a
                                                       * breeze: its green, a lighter rim where its edge curls, veins
                                                       * from the middle, the notch; a white or pink water lily */
        float nx = fm_sin_turns(kp_pad[i].notch + 0.25f), ny = fm_sin_turns(kp_pad[i].notch), r = kp_pad[i].r * kp_pad_bob(i, 0),
              px = kp_pad[i].x + kp_pad_bob(i, 1), py = kp_pad[i].y + kp_pad_bob(i, 2), ca = (float)kp_pad[i].curl / 256.0f;
        int v;
        r8_disc(s, (px + 1.0f) * k, (py + 1.5f) * k, r * k, KP_DEEP + 2);
        r8_disc(s, (px + 0.6f * fm_sin_turns(ca + 0.25f)) * k, (py + 0.6f * fm_sin_turns(ca)) * k, r * k, (uint8_t)(KP_LILY + 1));
        r8_disc(s, px * k, py * k, r * 0.94f * k, (uint8_t)(KP_LILY + 2 + kp_pad[i].shade));
        for (v = 0; v < 7; v++) {
            float a = kp_pad[i].notch + 0.07f + (float)v * (0.86f / 6.0f);
            r8_limb(s, px * k, py * k, 0.35f * k, (px + r * 0.85f * fm_sin_turns(a + 0.25f)) * k, (py + r * 0.85f * fm_sin_turns(a)) * k,
                    0.25f * k, (uint8_t)(KP_LILY + 5));
        }
        r8_tri(s, px * k, py * k, (px + (nx - ny * 0.35f) * r * 1.1f) * k,
               (py + (ny + nx * 0.35f) * r * 1.1f) * k, (px + (nx + ny * 0.35f) * r * 1.1f) * k,
               (py + (ny - nx * 0.35f) * r * 1.1f) * k, (uint8_t)(KP_DEEP + 28));   /* the notch */
        if (kp_pad[i].flower) {
            float fx = px - r * 0.3f, fy = py + r * 0.1f;
            uint8_t pc = (uint8_t)(KP_FLOWER + (kp_pad[i].flower == 1 ? 0 : 2));
            for (v = 0; v < 8; v++) {
                float a = (float)v / 8.0f + 0.03f;
                r8_quad(s, (fx + 2.0f * fm_sin_turns(a + 0.25f)) * k, (fy + 2.0f * fm_sin_turns(a)) * k, 1.7f * k, 0.8f * k, a,
                        (uint8_t)(pc + (v & 1)));
            }
            r8_disc(s, fx * k, fy * k, 1.2f * k, KP_FLOWER + 5);
        }
    }
    kp_frogs_draw(s, k);                             /* frogs on the pads, or leaping */
    kp_toro_draw(s, k, KP_TORO_X, KP_TORO_Y, 0);     /* the lanterns (under the maple's branch) */
    kp_toro_draw(s, k, KP_YUKI_X, KP_YUKI_Y, 1);
    for (i = 0; i < kp_nbl; i++)                      /* the maple's branch over the pond, its leaves */
        r8_limb(s, kp_bl[i].x0 * k, kp_bl[i].y0 * k, kp_bl[i].w * k, kp_bl[i].x1 * k, kp_bl[i].y1 * k, kp_bl[i].w * 0.5f * k,
                (uint8_t)(KP_TREE + (i ? 1 : 0)));
    for (i = 0; i < kp_nbf; i++) {
        r8_disc(s, kp_bf[i].x * k, kp_bf[i].y * k, kp_bf[i].r * k, kp_bf[i].c);
        r8_disc(s, (kp_bf[i].x - kp_bf[i].r * 0.3f) * k, (kp_bf[i].y - kp_bf[i].r * 0.3f) * k, kp_bf[i].r * 0.45f * k,
                (uint8_t)(KP_TREE + 2));
    }
    for (i = 0; i < KP_NRAIN; i++)                    /* a drop on a pad: a tiny splash, no ring */
        if (kp_rd[i].t > 0.6f && kp_rd[i].pad) {
            static const int8_t SPL[5][2] = {{0, 0}, {-1, -1}, {1, -1}, {-2, 0}, {2, 0}};
            int j;
            for (j = 0; j < 5; j++)
                r8_pset(s, (int32_t)(kp_rd[i].x * k) + SPL[j][0], (int32_t)(kp_rd[i].y * k) + SPL[j][1],
                        j ? KP_RIP + 2 : KP_RIP);
        }
    SC_PT(2);
    for (i = 0; i < KP_MAXF; i++) {                   /* food on the surface */
        food_t *f = &kp_f[i];
        float fade = f->life - f->age;
        if (!f->on)
            continue;
        if (f->kind == 1)
            r8_quad(s, f->x * k, f->y * k, 2.6f * k, 1.2f * k, f->age * 0.02f, (uint8_t)(KP_PETAL + (fade < 4.0f ? 2 : 0)));
        else if (f->kind == 2)
            r8_disc(s, f->x * k, f->y * k, 1.5f * k, (uint8_t)(KP_PEL + (fade < 4.0f ? 1 : 0)));
        else {
            r8_disc(s, f->x * k, f->y * k, 2.0f * k, (uint8_t)(KP_FOOD + (fade < 4.0f ? 2 : 0)));
            r8_disc(s, (f->x - 0.6f) * k, (f->y - 0.6f) * k, 0.8f * k, KP_FOOD);
        }
    }
    {   /* the shishi-odoshi: the feed pipe (kakehi) pouring a thin stream into the tube's open end; the tube, green-
         * gold bamboo with its nodes and a light down its top, on its axle between the posts: open end up while it
         * fills (it tips further as it fills), then down into the basin, the far end knocking its stone, and slowly back */
        float a = 0.06f - 0.05f * kp.sh_fill - 0.14f * kp.sh_tip, px = 222.0f, py = 16.0f + KP_SH_DY;
        float ux = -fm_sin_turns(a + 0.25f), uy = -fm_sin_turns(a), ex = px + 26.0f * ux, ey = py + 26.0f * uy,
              bx = px - 8.0f * ux, by = py - 8.0f * uy, n;
        r8_limb(s, 226.0f * k, (KP_SH_DY - 12.0f) * k, 2.6f * k, 203.0f * k, (KP_SH_DY + 2.0f) * k, 2.4f * k, KP_BAMBOO + 3);   /* the
                                                       * pipe */
        r8_limb(s, 226.0f * k, (KP_SH_DY - 12.8f) * k, 0.8f * k, 203.0f * k, (KP_SH_DY + 1.2f) * k, 0.7f * k, KP_BAMBOO + 1);
        if (kp.sh_tip <= 0.0f)                        /* its stream, falling from the pipe into the tube */
            r8_limb(s, 203.0f * k, (KP_SH_DY + 2.0f) * k, 0.7f * k, ex * k, (ey - 1.0f) * k, 0.6f * k, KP_RIP + 2);
        if (kp.sh_tip <= 0.0f)
            for (n = 0.0f; n < 1.0f; n += 0.34f) {    /* (drops in it, falling) */
                float u = n + 0.34f * (kp.t * 6.0f - fm_floorf(kp.t * 6.0f));
                r8_pset(s, (int32_t)((203.0f + (ex - 203.0f) * u) * k), (int32_t)((KP_SH_DY + 2.0f + (ey - 1.0f - KP_SH_DY - 2.0f) * u) * k), KP_RIP);
            }
        r8_limb(s, (bx + 1.0f) * k, (by + 2.0f) * k, 4.8f * k, (ex + 1.0f) * k, (ey + 2.0f) * k, 4.2f * k, (uint8_t)(KP_GROUND + 7));   /* its
                                                       * shadow */
        r8_limb(s, bx * k, by * k, 4.8f * k, ex * k, ey * k, 4.2f * k, KP_BAMBOO + 4);   /* its shaded underside */
        r8_limb(s, (bx - uy * 0.9f) * k, (by + ux * 0.9f) * k, 3.6f * k, (ex - uy * 0.8f) * k, (ey + ux * 0.8f) * k, 3.0f * k, KP_BAMBOO + 2);
        r8_limb(s, (bx - uy * 1.8f) * k, (by + ux * 1.8f) * k, 0.9f * k, (ex - uy * 1.6f) * k, (ey + ux * 1.6f) * k, 0.7f * k, KP_BAMBOO);   /* the
                                                       * light down its top */
        for (n = 0.3f; n < 0.8f; n += 0.32f) {        /* its nodes */
            float nx2 = bx + (ex - bx) * n, ny2 = by + (ey - by) * n;
            r8_limb(s, (nx2 + uy * 4.4f) * k, (ny2 - ux * 4.4f) * k, 0.7f * k, (nx2 - uy * 4.4f) * k, (ny2 + ux * 4.4f) * k, 0.7f * k,
                    KP_BAMBOO + 6);
        }
        r8_disc(s, ex * k, ey * k, 3.6f * k, KP_BAMBOO + 5);   /* its open end, cut on the slant */
        r8_disc(s, (ex + ux * 0.8f) * k, (ey + uy * 0.8f) * k, 2.4f * k, kp.sh_fill > 0.3f ? KP_DEEP + 14 : KP_BAMBOO + 7);
        r8_limb(s, px * k, (py - 7.0f) * k, 0.8f * k, px * k, (py + 7.0f) * k, 0.8f * k, KP_BAMBOO + 7);   /* the axle */
        r8_disc(s, px * k, py * k, 1.4f * k, KP_BAMBOO + 6);
    }
    SC_PT(3);
    kp_sky_draw(s, k);                               /* the sky on the water, a breeze's ripples */
    kp_glints_draw(s, k);                            /* (Feeding's sparkle; the lantern, lit at night) */
    kp_moon_draw(s, k);
    kp_toro_light(s, k, KP_TORO_X, KP_TORO_Y, 0);    /* (their light last: on the moss, the stones, the water) */
    kp_toro_light(s, k, KP_YUKI_X, KP_YUKI_Y, 1);
    SC_PT(4);
}

/* ---------------------------------------------------------------- scene --- */
SC_SMALL static void kp_mix(void)
{
    fm_part_level(1, kp.kv[KK_NOTES]);
    fm_part_level(3, kp.kv[KK_NOTES] + 30 > 100 ? 100 : kp.kv[KK_NOTES] + 30);   /* the knock above the notes ("can't hear it at all") */
    fm_part_level(4, kp.kv[KK_NOTES]);
    cd_drone_level(kp.kv[KK_DRONE]);
    fm_bed(FM_BED_WIND, kp.kv[KK_AIR]);
    fm_bed(FM_BED_STREAM, kp.kv[KK_WATER]);
}

SC_SMALL static void kp_defaults(int16_t *kv)
{
    int i;
    for (i = 0; i < SC_NKNOB; i++)
        kv[i] = KP_KNOB[i][3];
}

SC_SMALL static void kp_enter(uint32_t seed, int16_t *kv)
{
    int i, j;
    memset(&kp, 0, sizeof kp);
    memset(kp_rd, 0, sizeof kp_rd);
    kp.kv = kv;
    for (i = 0; i < SC_NKNOB; i++)
        if (KP_KNOB[i][1] > KP_KNOB[i][0] && (kv[i] < KP_KNOB[i][0] || kv[i] > KP_KNOB[i][1]))
            kv[i] = KP_KNOB[i][3];
    for (i = 0; i < KP_MAXF; i++)
        kp_f[i].on = 0;
    for (i = 0; i < KP_MAXR; i++)
        kp_r[i].t = 0.0f;
    for (i = 0; i < KP_NMEM; i++)
        kp_mem[i].left = 0;
    for (i = 0; i < KP_MAXK; i++)
        kp_k[i].on = 0;
    kp.seed = seed | 1u;
    kp_shape(kp.seed);                                /* the pond's shape, this picture's */
    sc_seed(seed * 2654435761u + 9u);
    kp.day = 0.12f + 0.2f * (float)(seed % 100u) * 0.01f;
    kp.hold = KP_MOOD_DAY[sc_mood_base(kv[KK_MOOD], KP_NMOOD)];   /* (Custom: its mood's time) */
    if (kp.hold >= 0.0f)
        kp.day = kp.hold;
    sc_glide_at(&kp.glide, kp.hold);
    kp.petal_next = 25.0f + 30.0f * (float)(seed % 50u) * 0.02f;
    kp.surf_next = 30.0f;
    kp.sh_period = 70.0f;                             /* (the first: a little sooner than the knob's) */
    kp.frog_next = 25.0f + 20.0f * sc_frand();        /* a frog, a breeze, the sky's reflections */
    kp.br_next = 30.0f + 40.0f * sc_frand();
    kp.br_t = 0.0f;
    for (i = 0; i < KP_NFROG; i++)
        kp_fg[i].st = 0;
    for (i = 0; i < KP_NREFL; i++) {
        kp_sk[i].x = 240.0f * sc_frand();
        kp_sk[i].y = KP_CY - 70.0f + 140.0f * sc_frand();
        kp_sk[i].rx = 26.0f + 14.0f * sc_frand();     /* (smaller) */
        kp_sk[i].ry = 12.0f + 6.0f * sc_frand();
        kp_sk[i].v = 0.5f + 0.7f * sc_frand();         /* (slow, as Sakura's clouds) */
        for (j = 0; j < 5; j++) {                     /* its puffs: a cumulus seen in the water (mirrored: its flat base
                                                       * up), domes heaped toward its middle */
            float u = ((float)j - 2.0f) / 2.0f;
            kp_sk[i].px[j] = u * 0.62f + (sc_frand() - 0.5f) * 0.15f;
            kp_sk[i].py[j] = (j == 2 ? 0.28f : 0.08f + 0.12f * (1.0f - fm_fabsf(u))) + (sc_frand() - 0.5f) * 0.1f;
            kp_sk[i].pr[j] = j == 2 ? 0.6f + 0.12f * sc_frand() : 0.38f + 0.14f * sc_frand() * (1.0f - 0.4f * fm_fabsf(u));
        }
    }
    for (i = 0; i < KP_NLIGHT; i++) {                 /* the floor's light: lines round the shallows */
        kp_lt[i].r = KP_R * (0.64f + 0.28f * sc_frand());
        kp_lt[i].th = sc_frand();
        kp_lt[i].len = 0.07f + 0.08f * sc_frand();
        kp_lt[i].kw = 9.0f + 8.0f * sc_frand();
        kp_lt[i].v = (sc_frand() < 0.5f ? -1.0f : 1.0f) * (0.002f + 0.003f * sc_frand());
        kp_lt[i].ph = sc_frand();
    }
    for (i = 0; i < KP_NLEAF; i++) {                  /* a leaf or two afloat */
        float a = sc_frand(), r = 60.0f * fm_sqrtf(sc_frand());
        kp_lf[i].x = KP_CX + r * fm_sin_turns(a + 0.25f);
        kp_lf[i].y = KP_CY + r * fm_sin_turns(a);
        kp_lf[i].vx = 0.8f + 0.6f * sc_frand();
        kp_lf[i].vy = (sc_frand() - 0.5f) * 0.6f;
        kp_lf[i].a = sc_frand();
        kp_lf[i].va = (sc_frand() - 0.5f) * 0.03f;
        kp_lf[i].c = (uint8_t)(KP_TREE + 5 + (i & 1));
    }
    sc_seed(kp.seed * 2654435761u + 3u);
    kp_koi_count(kv[KK_KOI]);
    for (i = 0; i < 4; i++)                           /* a little food to start */
        kp_food_random(2, 25.0f);
    cd_init(seed);
    cd_activity((float)kv[KK_ACT] * 0.01f);
    cd_echo_amount((float)kv[KK_ECHO] * 0.01f);
    cd_change_mode(1);                                /* (the key's drift: always slow) */
    if (sc_mood_base(kv[KK_MOOD], KP_NMOOD) > 0) {    /* (a named mood: its own key and scale) */
        int b = sc_mood_base(kv[KK_MOOD], KP_NMOOD);
        cd_tune(KP_MOOD_SCALE[b], KP_MOOD_KEY[b]);
    }
    cd_range(KP_KLO, KP_KN);
    fm_part_patch(1, KP_TONE_SLOT[kv[KK_TONE] % KP_NTONE]);
    fm_part_patch(2, ZN_DEEP_PAD);                    /* the drone: deeper than Sakura's, as a pond is */
    fm_part_patch(3, ZN_DROP);                        /* a frog's plop (the knock is a recording now) */
    fm_part_patch(4, ZN_RIN);                         /* a singing bowl: rising, pebbles, passing, memories */
    fm_part_release(1, 2600);
    fm_part_release(2, 4000);
    fm_part_release(3, 900);
    fm_part_release(4, 3500);
    kp_mix();
    r8_dark(KP_DARK);                                 /* the pond deeper (the panel lifts the darks: it looked pale) */
    cd_drone(1);
    fm_tape(0);
    fm_bed_mod(FM_BED_WIND, 10);
    fm_bed_mod(FM_BED_STREAM, 10);
    fm_bed_style(FM_BED_STREAM, KP_WATER[kv[KK_WATERIS] % KP_NWATER]);
    fm_bed_style(FM_BED_WIND, kv[KK_AIRIS]);
    fm_bed(FM_BED_RAIN, 0);
    cd_melody(kv[KK_MELODY]);
    cd_arc_amount((float)kv[KK_ARC] * 0.01f);
    cd_depth_send(kv[KK_DEPTH]);
}

SC_SMALL static void kp_leave(void)
{
    int b;
    r8_dark(0);
    cd_stop();
    fm_tape(0);
    fm_depth(0);
    for (b = 0; b < FM_NBED; b++)
        fm_bed(b, 0);
}

/* what a koi does next: swim somewhere (along the edge, into the shade under a lily pad, across), hover a while,
 * or follow another */
SC_SMALL static void kp_next(koi_t *q, int me)
{
    float u = sc_frand();
    q->lead = -1;
    if (u < 0.2f) {
        q->st = 1;
        q->stt = 2.0f + 4.0f * sc_frand();
        return;
    }
    if (u < 0.38f) {
        int j, best = -1;
        float bd = 1e9f;
        for (j = 0; j < KP_MAXK; j++) {
            float ex, ey;
            if (j == me || !kp_k[j].on || kp_k[j].lead == me)
                continue;
            ex = kp_k[j].x[0] - q->x[0];
            ey = kp_k[j].y[0] - q->y[0];
            if (ex * ex + ey * ey < bd) {
                bd = ex * ex + ey * ey;
                best = j;
            }
        }
        if (best >= 0) {
            q->st = 2;
            q->lead = best;
            q->stt = 6.0f + 9.0f * sc_frand();
            return;
        }
    }
    q->st = 0;
    q->stt = 12.0f + 10.0f * sc_frand();
    u = sc_frand();
    if (u < 0.3f && kp.npad > 0) {                    /* the shade under a pad */
        int p = (int)(sc_rnd() % (uint32_t)kp.npad);
        q->wx = kp_pad[p].x + (sc_frand() - 0.5f) * 6.0f;
        q->wy = kp_pad[p].y + (sc_frand() - 0.5f) * 6.0f;
    } else {
        float a = sc_frand(), r = u < 0.75f ? kp_rim(a) - 16.0f - 10.0f * sc_frand() : 0.7f * (KP_R - 10.0f) * fm_sqrtf(sc_frand());
        q->wx = KP_CX + r * fm_sin_turns(a + 0.25f);
        q->wy = KP_CY + r * fm_sin_turns(a);
    }
}

/* a koi's wish: where it would head now (turns), and how eager (a speed factor) */
static float kp_steer(koi_t *q, int me, float *eager)
{
    float hx = q->x[0], hy = q->y[0], dx, dy;
    *eager = 1.0f;
    if (q->target >= 0 && (!kp_f[q->target].on))
        q->target = -1;
    if (q->target < 0 && q->interest > 0.0f) {        /* a frog's plop: turn to look */
        *eager = 1.25f;
        return kp_atan2(q->iy - hy, q->ix - hx);
    }
    if (q->target >= 0) {                             /* food: go to it */
        *eager = 1.6f;
        return kp_atan2(kp_f[q->target].y - hy, kp_f[q->target].x - hx);
    }
    dx = hx - KP_CX;
    dy = hy - KP_CY;
    if (kp.call_t > 0.0f && q->full <= 0.0f) {       /* someone at the edge: come over */
        float ex = kp.call_x - hx, ey = kp.call_y - hy;
        if (ex * ex + ey * ey > 14.0f * 14.0f) {
            *eager = 1.1f;
            return kp_atan2(ey, ex);
        }
    }
    if (q->st == 2 && q->lead >= 0 && kp_k[q->lead].on) {   /* following another, a body's length behind */
        koi_t *l = &kp_k[q->lead];
        float ex = l->x[KP_NSEG - 1] - hx, ey = l->y[KP_NSEG - 1] - hy;
        *eager = ex * ex + ey * ey < 100.0f ? 0.7f : 1.05f;
        return kp_atan2(ey, ex);
    }
    if (q->st == 1) {                                 /* hovering: barely moving, its nose turning a little */
        *eager = 0.12f;
        return q->h + 0.02f * fm_sin_turns(kp.t * 0.1f + q->fin);
    }
    return kp_atan2(q->wy - hy, q->wx - hx);           /* swimming somewhere: along the edge, under a pad, across */
    {   /* a free school: its own wander, a little toward the others and their heading, never too close, off the rim */
        float cx = 0.0f, cy = 0.0f, ax = 0.0f, ay = 0.0f, sx = 0.0f, sy = 0.0f, h;
        int i, n = 0;
        for (i = 0; i < KP_MAXK; i++) {
            float ex, ey, d2;
            if (i == me || !kp_k[i].on)
                continue;
            ex = kp_k[i].x[0] - hx;
            ey = kp_k[i].y[0] - hy;
            d2 = ex * ex + ey * ey;
            cx += kp_k[i].x[0];
            cy += kp_k[i].y[0];
            ax += fm_sin_turns(kp_k[i].h + 0.25f);
            ay += fm_sin_turns(kp_k[i].h);
            if (d2 < 400.0f && d2 > 1e-3f) {
                sx -= ex / d2 * 20.0f;
                sy -= ey / d2 * 20.0f;
            }
            n++;
        }
        h = q->h + q->wander;
        ax = fm_sin_turns(h + 0.25f) + (n ? 0.25f * ax / (float)n : 0.0f) + sx;
        ay = fm_sin_turns(h) + (n ? 0.25f * ay / (float)n : 0.0f) + sy;
        if (n) {
            ax += 0.004f * (cx / (float)n - hx);
            ay += 0.004f * (cy / (float)n - hy);
        }
        if (dx * dx + dy * dy > (KP_R - 22.0f) * (KP_R - 22.0f)) {   /* the rim: back toward the middle */
            ax -= dx * 0.03f;
            ay -= dy * 0.03f;
        }
        return kp_atan2(ay, ax);
    }
}

/* the shishi-odoshi's next fill, s (Knock: 100 ~35 s, 50 ~90 s, 1 ~4 min; each a third either way) */
static float kp_knock_period(void)
{
    float k = (float)(kp.kv[KK_KNOCK] < 1 ? 1 : kp.kv[KK_KNOCK]);
    return 35.0f * fm_exp2f((100.0f - k) * 0.02f * 1.36f) * (0.67f + 0.66f * sc_frand());
}

/* the small life's own randomness (a frog's blink and turns, the lanterns' breathing): apart from the scene's, so
 * the rest happens as it did */
static uint32_t kp_lr = 0x9E3779B9u;
static float kp_lrand(void)
{
    kp_lr = kp_lr * 1664525u + 1013904223u;
    return (float)(kp_lr >> 8) * (1.0f / 16777216.0f);
}

/* open water, clear of the rim's stones (the biggest reach ~11 over it): where a frog may jump from or dive in */
static int kp_open(float x, float y)
{
    float dx = x - KP_CX, dy = y - KP_CY, r = kp_rim_at(x, y) - 13.0f;
    return dx * dx + dy * dy < r * r;
}

/* the pond's life: frogs hopping onto the lily pads, croaking, plopping back in (the koi turn to look); the
 * sky's reflections drifting across; now and then a breeze's patch of ripples */
static void kp_life_step(float dt)
{
    int i, j;
    kp.feed += ((sc_mood_base(kp.kv[KK_MOOD], KP_NMOOD) == 1 ? 1.0f : 0.0f) - kp.feed) * fm_minf(1.0f, dt * 0.3f);   /* (the moods' looks) */
    kp.still += ((sc_mood_base(kp.kv[KK_MOOD], KP_NMOOD) == 3 ? 1.0f : 0.0f) - kp.still) * fm_minf(1.0f, dt * 0.3f);
    {   /* rain: whenever the water's sound is the rain (the Rain mood, or Water Type by hand); drops land at
         * random, each heard at its place (fm_drop); now and then one on a lily pad */
        int raining = KP_WATER[kp.kv[KK_WATERIS] % KP_NWATER] == FM_WATER_RAIN;
        kp.rain += ((raining ? 1.0f : 0.0f) - kp.rain) * fm_minf(1.0f, dt * 0.3f);
        for (i = 0; i < KP_NRAIN; i++)
            if (kp_rd[i].t > 0.0f)
                kp_rd[i].t -= dt;
        if (kp.rain > 0.05f && sc_frand() < kp.rain * kp.rain * dt * 7.0f)   /* (a few drops first) */
            for (i = 0; i < KP_NRAIN; i++)
                if (kp_rd[i].t <= 0.0f) {
                    float a = sc_frand(), r = (KP_R - 6.0f) * fm_sqrtf(sc_frand());
                    int pd = kp.npad > 0 && sc_frand() < 0.12f ? (int)(sc_rnd() % (uint32_t)kp.npad) : -1;
                    kp_rd[i].pad = pd >= 0;
                    kp_rd[i].x = pd >= 0 ? kp_pad[pd].x + (sc_frand() - 0.5f) * kp_pad[pd].r : KP_CX + r * fm_sin_turns(a + 0.25f);
                    kp_rd[i].y = pd >= 0 ? kp_pad[pd].y + (sc_frand() - 0.5f) * kp_pad[pd].r : KP_CY + r * fm_sin_turns(a);
                    kp_rd[i].sz = 0.7f + 0.6f * sc_frand();
                    kp_rd[i].t = 0.9f;
                    fm_drop((int)(40.0f + 60.0f * kp_rd[i].sz * (pd >= 0 ? 0.6f : 1.0f)), cd_pan(kp_rd[i].x / 240.0f), pd >= 0);
                    kp.raindrops++;
                    break;
                }
    }
    for (i = 0; i < KP_NLIGHT; i++) {                 /* the floor's light slides slowly round, wobbling */
        kp_lt[i].th += kp_lt[i].v * dt;
        kp_lt[i].ph += 0.05f * dt;
        if (kp_lt[i].th > 1.0f)
            kp_lt[i].th -= 1.0f;
        if (kp_lt[i].th < 0.0f)
            kp_lt[i].th += 1.0f;
    }
    for (i = 0; i < KP_NREFL; i++) {                  /* the sky drifts over */
        kp_sk[i].x += kp_sk[i].v * dt;
        if (kp_sk[i].x - kp_sk[i].rx > 245.0f) {
            kp_sk[i].x = -kp_sk[i].rx - 5.0f;
            kp_sk[i].y = KP_CY - 70.0f + 140.0f * sc_frand();
        }
    }
    if (kp.br_t > 0.0f) {                             /* a breeze crossing */
        kp.br_t += dt;
        kp.br_x += 12.0f * dt;
        kp.br_y += 3.0f * dt;
        if (kp.br_t >= kp.br_dur)
            kp.br_t = 0.0f;
    } else if ((kp.br_next -= dt) <= 0.0f) {
        kp.br_t = 0.001f;
        kp.br_dur = 6.0f + 4.0f * sc_frand();
        kp.br_x = KP_CX - 60.0f + 40.0f * sc_frand();
        kp.br_y = KP_CY - 50.0f + 100.0f * sc_frand();
        kp.br_next = 40.0f + 50.0f * sc_frand();
        kp.breezes++;
    }
    for (i = 0; i < kp.npad; i++)
        kp_pad[i].dip *= fm_maxf(0.0f, 1.0f - dt * 1.2f);
    for (i = 0; i < 2; i++) {                         /* each lantern's flame: a new strength now and then, eased to */
        if ((kp.lamp_next[i] -= dt) <= 0.0f) {
            kp.lamp_to[i] = kp_lrand();
            kp.lamp_next[i] = 0.8f + 2.6f * kp_lrand();
        }
        kp.lamp[i] += (kp.lamp_to[i] - kp.lamp[i]) * fm_minf(1.0f, dt * 0.9f);
    }
    if ((kp.frog_next -= dt) <= 0.0f) {               /* a frog hops onto a free pad */
        for (i = 0; i < KP_NFROG && kp_fg[i].st; i++)
            ;
        if (i < KP_NFROG && kp.npad > 0) {
            int pad = (int)(sc_rnd() % (uint32_t)kp.npad), used = 0;
            if (kp_pad[pad].flower)                   /* (a pad, not its flower, if it can) */
                pad = (pad + 1) % kp.npad;
            for (j = 0; j < KP_NFROG; j++)
                if (kp_fg[j].st && kp_fg[j].pad == pad)
                    used = 1;
            float an = sc_frand();
            for (j = 0; j < 8 && !kp_open(kp_pad[pad].x + 22.0f * fm_sin_turns(an + 0.25f),
                                          kp_pad[pad].y + 22.0f * fm_sin_turns(an)); j++)
                an = sc_frand();                      /* (up out of the water, never off the stones) */
            if (j == 8)
                used = 1;
            if (!used) {
                uint32_t w = sc_rnd() % 100u, acc = 0;
                for (j = 0; j < KP_NSPECIES - 1; j++) {   /* which frog: by how common */
                    acc += KP_SPECIES[j].weight;
                    if (w < acc)
                        break;
                }
                kp_fg[i].sp = j;
                kp_fg[i].mark = sc_rnd();
                kp_fg[i].st = 1;
                kp_fg[i].t = 0.0f;
                kp_fg[i].pad = pad;
                kp_fg[i].x0 = kp_pad[pad].x + 22.0f * fm_sin_turns(an + 0.25f);
                kp_fg[i].y0 = kp_pad[pad].y + 22.0f * fm_sin_turns(an);
                kp_fg[i].fx = -fm_sin_turns(an + 0.25f);   /* (facing the pad, as it jumps) */
                kp_fg[i].fy = -fm_sin_turns(an);
                kp_fg[i].rot = 0.0f;
                kp_fg[i].blink = 2.0f;
                kp_ripple(kp_fg[i].x0, kp_fg[i].y0);
                kp.frogs++;
            }
        }
        kp.frog_next = 30.0f + 50.0f * sc_frand();
    }
    for (i = 0; i < KP_NFROG; i++) {
        float u;
        switch (kp_fg[i].st) {
        case 1:                                       /* hopping on: an arc to the pad */
        case 3: {                                     /* hopping off: an arc into the water */
            float tx = kp_fg[i].st == 1 ? kp_pad[kp_fg[i].pad].x + kp_pad_bob(kp_fg[i].pad, 1) : kp_fg[i].x1,
                  ty = kp_fg[i].st == 1 ? kp_pad[kp_fg[i].pad].y + kp_pad_bob(kp_fg[i].pad, 2) : kp_fg[i].y1;
            kp_fg[i].t += dt;
            u = fm_minf(1.0f, kp_fg[i].t / 0.6f);
            kp_fg[i].x = kp_fg[i].x0 + (tx - kp_fg[i].x0) * u;
            kp_fg[i].y = kp_fg[i].y0 + (ty - kp_fg[i].y0) * u - 6.0f * fm_sin_turns(u * 0.5f);
            if (u >= 1.0f) {
                if (kp_fg[i].st == 1) {
                    kp_fg[i].st = 2;
                    kp_pad[kp_fg[i].pad].dip = 1.0f;
                    kp_fg[i].sit = 12.0f + 18.0f * sc_frand();
                    kp_fg[i].croak = 2.0f + 3.0f * sc_frand();
                } else {                              /* in: a plop, rings, and the koi near turn to look */
                    kp_fg[i].st = 0;
                    kp_ripple(kp_fg[i].x, kp_fg[i].y);
                    kp_ripple(kp_fg[i].x + 0.5f, kp_fg[i].y);
                    for (j = 0; j < KP_NRAIN; j++)    /* (its splash: a drop's, no sound of its own) */
                        if (kp_rd[j].t <= 0.0f) {
                            kp_rd[j].pad = 1;
                            kp_rd[j].x = kp_fg[i].x;
                            kp_rd[j].y = kp_fg[i].y;
                            kp_rd[j].t = 0.9f;
                            break;
                        }
                    fm_voice(FM_VOICE_ID + 46, 3, -700.0f + 200.0f * sc_frand(), 70, cd_pan(kp_fg[i].x / 240.0f), 140);
                    kp.plops++;
                    for (j = 0; j < KP_MAXK; j++) {
                        float ex = kp_k[j].x[0] - kp_fg[i].x, ey = kp_k[j].y[0] - kp_fg[i].y;
                        if (kp_k[j].on && ex * ex + ey * ey < 70.0f * 70.0f) {
                            kp_k[j].ix = kp_fg[i].x;
                            kp_k[j].iy = kp_fg[i].y;
                            kp_k[j].interest = 2.5f;
                        }
                    }
                }
            }
            break;
        }
        case 2:                                       /* sitting on its pad (bobbing with it); now and then a croak */
            kp_fg[i].x = kp_pad[kp_fg[i].pad].x + kp_pad_bob(kp_fg[i].pad, 1);
            kp_fg[i].y = kp_pad[kp_fg[i].pad].y + kp_pad_bob(kp_fg[i].pad, 2);
            kp_fg[i].puff *= fm_maxf(0.0f, 1.0f - dt * 2.5f);
            if ((kp_fg[i].blink -= dt) < 0.0f)
                kp_fg[i].blink = 3.0f + 5.0f * kp_lrand();
            if (kp_fg[i].rot != 0.0f) {               /* turning a little, slowly */
                float d = fm_clampf(kp_fg[i].rot, -0.12f * dt, 0.12f * dt), ca = fm_sin_turns(d + 0.25f), sa = fm_sin_turns(d),
                      nx = kp_fg[i].fx * ca - kp_fg[i].fy * sa;
                kp_fg[i].fy = kp_fg[i].fx * sa + kp_fg[i].fy * ca;
                kp_fg[i].fx = nx;
                kp_fg[i].rot -= d;
            } else if (kp_lrand() < dt * 0.15f)
                kp_fg[i].rot = (kp_lrand() - 0.5f) * 0.25f;
            if ((kp_fg[i].croak -= dt) <= 0.0f) {
                fm_shot(1 + i, FM_SHOT_FROG, (int)(sc_rnd() % (uint32_t)fm_shot_count(FM_SHOT_FROG)), 46 + (int)(sc_rnd() % 10u),
                        cd_pan(kp_fg[i].x / 240.0f),
                        (int)(100.0f * KP_SPECIES[kp_fg[i].sp].rate) + (int)(sc_rnd() % 10u));   /* (its own voice) */
                kp_fg[i].puff = 1.0f;
                kp_fg[i].croak = 4.0f + 6.0f * sc_frand();
            }
            if ((kp_fg[i].sit -= dt) <= 0.0f &&       /* off again, into the water (turned toward open water first:
                                                       * never onto the stones) */
                !kp_open(kp_fg[i].x + 24.0f * kp_fg[i].fx, kp_fg[i].y + 24.0f * kp_fg[i].fy)) {
                for (j = 0; j < 12; j++) {
                    float an = kp_lrand(), ex = kp_fg[i].x + 24.0f * fm_sin_turns(an + 0.25f), ey = kp_fg[i].y + 24.0f * fm_sin_turns(an);
                    if (kp_open(ex, ey)) {
                        float d = an - kp_atan2(kp_fg[i].fy, kp_fg[i].fx);
                        d -= fm_floorf(d + 0.5f);
                        kp_fg[i].rot = d;
                        kp_fg[i].sit = fm_fabsf(d) / 0.12f + 0.4f;
                        break;
                    }
                }
                if (j == 12)
                    kp_fg[i].sit = 2.0f;              /* (no way clear yet: it sits a while longer) */
            } else if (kp_fg[i].sit <= 0.0f) {
                kp_fg[i].st = 3;                      /* (the way it faces) */
                kp_fg[i].t = 0.0f;
                kp_fg[i].x0 = kp_fg[i].x;
                kp_fg[i].y0 = kp_fg[i].y;
                kp_fg[i].x1 = kp_fg[i].x + 24.0f * kp_fg[i].fx;
                kp_fg[i].y1 = kp_fg[i].y + 24.0f * kp_fg[i].fy;
                kp_pad[kp_fg[i].pad].dip = 0.6f;
            }
            break;
        default: break;
        }
    }
}

static void kp_step(uint32_t dt_ms)
{
    float dt = (float)dt_ms * 0.001f, a = (float)kp.kv[KK_ACT] * 0.01f;
    uint32_t cdt = dt_ms;
    int i, j;
    kp.t += dt;
    if (kp.hold < 0.0f) {                             /* the day goes round: half an hour (Natural) */
        kp.day += dt * (1.0f / 1800.0f);
        if (kp.day >= 1.0f) {
            kp.day -= 1.0f;
            cd_dawn();
        }
        sc_glide_at(&kp.glide, kp.hold);
    } else                                            /* a mood holds its time: there in 20 s, gently, forward */
        kp.day = sc_glide(&kp.glide, kp.day, kp.hold, dt);
    cd_step(cdt);
    {   /* Natural: the scale with the light: Yo from dawn through the afternoon, Min'yo from golden hour */
        float d = kp.day - fm_floorf(kp.day);
        cd_light(d > 0.97f || d < 0.4f ? SC_YO : SC_MINYO);
    }
    {   /* Natural: the sound with the time (the knobs as they are): livelier by day, quieter and warmer at night */
        float sun = fm_sin_turns(kp.day), lit = fm_clampf(sun * 1.5f + 0.3f, 0.0f, 1.0f);
        int nat = kp.kv[KK_MOOD] == SC_MOOD_NATURAL;
        cd_activity(a * (nat ? 0.55f + 0.55f * lit : 1.0f));
        cd_depth_send(kp.kv[KK_DEPTH] + (nat ? (int)(30.0f * (1.0f - lit)) : 0));   /* (and the arc's) */
    }
    if (kp.kv[KK_AIRIS] == FM_AIR_INSECTS)            /* the insects come with the night, as in Sakura (noon 0, midnight
                                                       * 1); a breeze or leaves stay calm */
        fm_bed_mod(FM_BED_WIND, (int)(100.0f * fm_clampf(0.5f - 0.5f * fm_sin_turns(kp.day), 0.0f, 1.0f)));
    else
        fm_bed_mod(FM_BED_WIND, 10);
    /* the pond feeds itself: pellets at the activity's rate, and now and then a petal from Sakura's tree */
    if (kp.call_t > 0.0f)
        kp.call_t -= dt;
    for (i = 0; i < KP_NLEAF; i++) {                  /* the leaves drift across, turning; one gone comes back in */
        float dx, dy, R;
        kp_lf[i].x += kp_lf[i].vx * dt;
        kp_lf[i].y += kp_lf[i].vy * dt;
        kp_lf[i].a += kp_lf[i].va * dt;
        dx = kp_lf[i].x - KP_CX;
        dy = kp_lf[i].y - KP_CY;
        R = kp_rim(kp_atan2(dy, dx)) - 5.0f;
        if (dx * dx + dy * dy > R * R) {
            float an = 0.5f + (sc_frand() - 0.5f) * 0.3f, r = kp_rim(an) - 8.0f;
            kp_lf[i].x = KP_CX + r * fm_sin_turns(an + 0.25f);
            kp_lf[i].y = KP_CY + r * fm_sin_turns(an);
        }
    }
    kp.food_acc += dt * (0.04f + 0.4f * a);
    while (kp.food_acc >= 1.0f) {
        kp.food_acc -= 1.0f;
        kp_food_random(2, 40.0f);
    }
    kp.petal_next -= dt;
    if (kp.petal_next <= 0.0f) {                     /* in at the rim, drifting across */
        float an = sc_frand();
        int f = kp_food_add(KP_CX + (KP_R - 6.0f) * fm_sin_turns(an + 0.25f), KP_CY + (KP_R - 6.0f) * fm_sin_turns(an), 1, 45.0f);
        if (f >= 0) {
            kp_f[f].vx = -fm_sin_turns(an + 0.25f) * 2.2f;
            kp_f[f].vy = -fm_sin_turns(an) * 2.2f;
        }
        kp.petal_next = 40.0f + 50.0f * sc_frand();
    }
    for (i = 0; i < KP_MAXF; i++) {                   /* food floats a little, ages, and may be remembered */
        food_t *f = &kp_f[i];
        if (!f->on)
            continue;
        f->x += f->vx * dt;
        f->y += f->vy * dt;
        f->age += dt;
        {
            float dx = f->x - KP_CX, dy = f->y - KP_CY, R = kp_rim(kp_atan2(dy, dx)) - 4.0f;
            if (dx * dx + dy * dy > R * R) {
                f->vx = -f->vx;
                f->vy = -f->vy;
            }
        }
        if (f->kind == 1)
            f->key = kp_key_at(f->x, f->y);           /* a drifting petal sings where it is eaten */
        if (f->age >= f->life) {
            f->on = 0;
            kp.expired++;
            if (sc_frand() < KP_MEMORY)
                kp_memory_add(f->key, f->x / 240.0f);
        }
    }
    for (i = 0; i < KP_NMEM; i++)                      /* the pond's memory: a soft swell now and then */
        if (kp_mem[i].left > 0) {
            kp_mem[i].at -= dt;
            if (kp_mem[i].at <= 0.0f) {
                cd_memory(kp_mem[i].key, kp_mem[i].pos);
                kp_mem[i].left--;
                kp_mem[i].at = 14.0f + 16.0f * sc_frand();
            }
        }
    for (i = 0; i < KP_MAXK; i++) {
        koi_t *q = &kp_k[i];
        float want, eager, turn, spd, sp;
        int s;
        if (!q->on)
            continue;
        q->look -= dt;
        if (q->full > 0.0f)
            q->full -= dt;
        if (q->target < 0 && q->look <= 0.0f && q->full <= 0.0f) {       /* food near? (some are slower to notice than others) */
            float best = (24.0f + 30.0f * a) * (24.0f + 30.0f * a);   /* (near enough: some food goes uneaten, and is
                                                                         * remembered) */
            q->look = 0.3f + 0.6f * sc_frand();
            for (j = 0; j < KP_MAXF; j++)
                if (kp_f[j].on) {
                    float ex = kp_f[j].x - q->x[0], ey = kp_f[j].y - q->y[0], d2 = ex * ex + ey * ey;
                    if (d2 < best) {
                        best = d2;
                        q->target = j;
                    }
                }
        }
        q->zv += (sc_frand() - 0.5f) * 0.12f * dt - q->zv * 0.25f * dt + (kp.rain * (0.5f - q->z) + kp.still * (0.42f - q->z) + kp.feed * (0.15f - q->z)) * 0.05f * dt;   /* (deeper
                                                       * in rain) */   /* how deep: a slow glide up and down; up to
                                                                         * the surface when rising, or after food */
        if (q->rise > 0.0f || q->target >= 0)
            q->zv -= (q->rise > 0.0f ? 0.5f : 0.15f) * dt;
        q->z = fm_clampf(q->z + q->zv * dt, 0.0f, 0.55f + 0.4f * kp_night());   /* (by day never deep enough to
                                                       * go dim; at night down to the floor) */
        if (q->z <= 0.0f || q->z >= 1.0f)
            q->zv = 0.0f;
        q->pitch += (fm_clampf(-q->zv * 2.2f, -0.45f, 0.45f) - q->pitch) * fm_minf(1.0f, 1.5f * dt);   /* (rising: the
                                                                         * head comes up first, the tail after it) */
        if (q->rise > 0.0f && q->z < 0.05f) {        /* at the surface: it mouths, a ring, a soft bowl */
            q->rise = 0.0f;
            q->zv = 0.12f;
            kp_ripple(q->x[0], q->y[0]);
            cd_note_key(kp_own_key(kp_key_at(q->x[0], q->y[0]), q->x[0]), q->x[0] / 240.0f, 0.4f, 2000, 4, 400u);
        }
        if (q->interest > 0.0f)
            q->interest -= dt;
        q->pr_v += (sc_frand() - 0.5f) * 6.0f * dt - q->pr_v * 0.2f * dt;   /* the orbit's depth drifts */
        q->pr = fm_clampf(q->pr + q->pr_v * dt, 18.0f, KP_R - 16.0f);
        q->wander = fm_clampf(q->wander + (sc_frand() - 0.5f) * 0.4f * dt, -0.08f, 0.08f);
        if (sc_frand() < dt * 0.006f)
            q->dir = -q->dir;                          /* now and then it turns about */
        if ((q->stt -= dt) <= 0.0f || (q->st == 0 && (q->x[0] - q->wx) * (q->x[0] - q->wx) + (q->y[0] - q->wy) * (q->y[0] - q->wy) < 64.0f))
            kp_next(q, i);
        if ((q->burst -= dt) <= 0.0f)                 /* a few strokes, then a glide */
            q->burst = q->burst < -1.5f - 2.0f * sc_frand() ? 0.8f + 0.6f * sc_frand() : q->burst;
        q->fin += dt * (q->st == 1 ? 1.4f : 0.4f);
        q->fin -= fm_floorf(q->fin);
        if (q->target >= 0) {                         /* food near: up to it, mouth open at the surface */
            float ex = kp_f[q->target].x - q->x[0], ey = kp_f[q->target].y - q->y[0], d2 = ex * ex + ey * ey;
            if (d2 < 22.0f * 22.0f)
                q->zv -= 0.6f * dt;
            q->mouth += ((d2 < 12.0f * 12.0f && q->z < 0.2f ? 1.0f : 0.0f) - q->mouth) * fm_minf(1.0f, 6.0f * dt);
        } else
            q->mouth *= fm_maxf(0.0f, 1.0f - 4.0f * dt);
        want = kp_steer(q, i, &eager);
        turn = kp_wrap(want - q->h);
        turn = fm_clampf(turn, -0.32f * dt * eager, 0.32f * dt * eager);
        q->bend += ((dt > 0.0f ? turn / dt : 0.0f) - q->bend) * fm_minf(1.0f, 3.0f * dt);   /* (its body bent into it) */
        if (fm_fabsf(q->bend) > 0.18f && q->burst <= 0.0f)
            q->burst = 0.6f;                          /* (a sharp turn: a stroke drives it through) */
        q->h += turn;
        q->h -= fm_floorf(q->h);
        spd = (7.0f + 7.0f * a) * eager * (0.85f + 0.3f * q->size) * (1.0f - 0.3f * kp.rain) *
              (q->target >= 0 ? 1.0f : q->burst > 0.0f ? 1.3f : 0.7f) *   /* (strokes and glides) */
              (1.0f + 0.25f * kp.feed - 0.3f * kp.still) * (1.0f - 0.3f * kp_night());   /* (calmer in rain, in Still
                                                       * Pond, at night; quick when Feeding) */
        q->v += (spd - q->v) * fm_minf(1.0f, dt);
        q->x[0] += q->v * dt * fm_sin_turns(q->h + 0.25f);
        q->y[0] += q->v * dt * fm_sin_turns(q->h);
        {
            float dx = q->x[0] - KP_CX, dy = q->y[0] - KP_CY, r2 = dx * dx + dy * dy, lim = kp_rim(kp_atan2(dy, dx)) - 9.0f;
            if (r2 > lim * lim) {                     /* never out of the pond */
                float r = fm_sqrtf(r2);
                q->x[0] = KP_CX + dx / r * lim;
                q->y[0] = KP_CY + dy / r * lim;
                q->h += 0.02f;
            }
        }
        sp = 3.0f * q->size;                          /* the body follows the head */
        for (s = 1; s < KP_NSEG; s++) {
            float ex = q->x[s - 1] - q->x[s], ey = q->y[s - 1] - q->y[s], l = fm_sqrtf(ex * ex + ey * ey);
            if (l > sp) {
                q->x[s] = q->x[s - 1] - ex / l * sp;
                q->y[s] = q->y[s - 1] - ey / l * sp;
            }
        }
        {   /* the tail's beat, slower the bigger the koi; its wave as big as the effort: stroking, after food,
             * hovering a little, gliding nearly straight; the fins tucked as it speeds, spread as it slows or brakes */
            float st = q->target >= 0 ? 1.0f : q->st == 1 ? 0.25f : q->burst > 0.0f ? 1.0f : 0.06f;
            q->amp += (st - q->amp) * fm_minf(1.0f, 1.3f * dt);
            q->ph += dt * (0.6f + q->v * 0.07f) * (q->target >= 0 ? 1.7f : q->st == 1 ? 0.6f : q->burst > 0.0f ? 1.4f : 0.6f) /
                     fm_sqrtf(q->size);
            q->spread += (fm_clampf(1.2f - q->v / 16.0f, 0.15f, 1.0f) + (q->mouth > 0.3f ? 0.4f : 0.0f) - q->spread) * fm_minf(1.0f, 2.0f * dt);
        }
        if (q->ph >= 1.0f)
            q->ph -= fm_floorf(q->ph);
        if (q->cool_cross > 0.0f)
            q->cool_cross -= dt;
        for (j = 0; j < KP_MAXF; j++) {               /* a bite: the note of the ring it lay on */
            food_t *f = &kp_f[j];
            float ex, ey;
            if (!f->on)
                continue;
            ex = f->x - q->x[0];
            ey = f->y - q->y[0];
            if (q->full <= 0.0f && ex * ex + ey * ey < (q->target == j ? 30.0f * q->size : 6.0f)) {   /* what it is after; or right at its
                                                                             * mouth (the rest may sink, remembered) */
                float st = fm_clampf(q->v / 22.0f, 0.2f, 1.0f) * (0.7f + 0.3f * q->size);
                if (cd_note_key(f->keyed ? f->key : kp_own_key(f->key, f->x), f->x / 240.0f, st, f->kind == 1 ? 1800 : 1300,
                                f->kind == 1 ? 4 : 1, 180u)) {
                    f->on = 0;
                    kp.eaten++;
                    q->full = (6.0f + 24.0f * (1.0f - a)) * (0.7f + 0.6f * sc_frand());   /* full for a while */
                    kp_ripple(f->x, f->y);
                    if (q->target == j)
                        q->target = -1;
                }
                break;
            }
        }
    }
    for (i = 0; i < KP_MAXK; i++)                     /* two passing close: a soft swell between their rings */
        for (j = i + 1; j < KP_MAXK; j++) {
            koi_t *p = &kp_k[i], *q = &kp_k[j];
            float ex, ey;
            if (!p->on || !q->on || p->cool_cross > 0.0f || q->cool_cross > 0.0f)
                continue;
            ex = p->x[0] - q->x[0];
            ey = p->y[0] - q->y[0];
            if (ex * ex + ey * ey < 70.0f) {
                cd_note_key(kp_own_key((kp_key_at(p->x[0], p->y[0]) + kp_key_at(q->x[0], q->y[0])) / 2, p->x[0]),
                            p->x[0] / 240.0f, 0.25f, 1800, 4, 600u);
                p->cool_cross = q->cool_cross = 12.0f;
            }
        }
    kp.surf_next -= dt;                               /* a koi rises to the surface: a ring and a soft swell */
    if (kp.surf_next <= 0.0f) {
        int n = (int)(sc_rnd() % KP_MAXK);
        for (i = 0; i < KP_MAXK && !kp_k[(n + i) % KP_MAXK].on; i++)
            ;
        if (i < KP_MAXK) {
            koi_t *q = &kp_k[(n + i) % KP_MAXK];
            q->rise = 1.0f;                           /* (it glides up first: the ring and the bowl at the surface) */
        }
        kp.surf_next = 35.0f + 45.0f * sc_frand();
    }
    {   /* the shishi-odoshi fills, tips, knocks its stone, rises again (Knock 0: it never fills) */
        if (kp.sh_tip <= 0.0f) {
            if (kp.kv[KK_KNOCK] > 0)
                kp.sh_fill += dt / kp.sh_period;
            if (kp.sh_fill >= 1.0f) {                 /* (full: by itself, or the top black key) */
                kp.sh_fill = 0.0f;
                kp.sh_tip = 0.001f;
                cd_knock(KP_KLO + 6, 0.92f);
                kp.knocks++;
                kp.sh_period = kp_knock_period();
            }
        } else if (!kp.sh_back) {                     /* down fast (~0.2 s) ... */
            kp.sh_tip += dt * 5.0f;
            if (kp.sh_tip >= 1.0f) {
                kp.sh_tip = 1.0f;
                kp.sh_back = 1;
            }
        } else {                                      /* ... and back slowly (~1.7 s). (Earlier the tip swung
                                                       * about its middle for good once a frame stepped past a narrow
                                                       * window: one knock a visit) */
            kp.sh_tip -= dt * 0.6f;
            if (kp.sh_tip <= 0.0f) {
                kp.sh_tip = 0.0f;
                kp.sh_back = 0;
            }
        }
    }
    for (i = 0; i < KP_MAXR; i++)
        if (kp_r[i].t > 0.0f && kp_r[i].t < 2.0f)
            kp_r[i].t += dt;
    kp_life_step(dt);
    kp_palette();
}

/* black keys: the lower ten toss a pebble onto a ring of their own, low to high, each with its own
 * bowl note, so a melody can be played on them; the top one tips the shishi-odoshi (a knock, now) */
SC_SMALL static void kp_key(int key, int on)
{
    int b;
    if (!on)
        return;
    for (b = 0; b < FM_NBLACK; b++)
        if (BLACK_K[b] == key) {
            if (b < FM_NBLACK - 1) {
                int ring = KP_KLO + (b * (KP_KN - 1) + (FM_NBLACK - 2) / 2) / (FM_NBLACK - 2);   /* 0..9 -> 3..15 */
                float an = sc_frand(), r = kp_ring_r(ring);
                float x = KP_CX + r * fm_sin_turns(an + 0.25f), y = KP_CY + r * fm_sin_turns(an);
                kp_ripple(x, y);
                kp_ripple(x + 0.5f, y);
                cd_note_key(ring, x / 240.0f, 0.5f, 2000, 4, 0u);
                kp.pebbles++;
            } else if (kp.sh_tip <= 0.0f) {
                kp.sh_fill = 1.0f;                    /* full: it tips on the next step */
                kp.sh_period = 1.0f;
            }
            return;
        }
    for (b = 0; b < FM_NWHITE; b++)
        if (WHITE_K[b] == key) {
            int kk = b + KP_KLO;                      /* (sixteen notes, each its own, rising from the pond's lowest: below it the
                                                       * speaker hardly plays) */
            cd_note_key(kk, (8.0f + (float)key * (224.0f / 26.0f)) / 240.0f,
                        KP_KEY_ST + KP_KEY_LIFT * (1.0f - (float)(kk - KP_KLO) / (float)(KP_KN - 1)), 1300, 1, 0u);   /* its note now
                                                                         *, and again when a koi eats it */
            kp.key_note = kk;
            {   /* someone at the edge: the koi come over toward them, and the food falls there */
                float side = fm_clampf(((float)key - 13.0f) / 13.0f, -1.0f, 1.0f);
                kp.call_t = 6.0f;
                kp.call_x = KP_CX + 45.0f * side;
                kp.call_y = KP_CY + kp_rim(0.25f - side * 0.18f) - 26.0f;
                kp_food_on_ring(kk, side);
            }
            return;
        }
}

SC_SMALL static void kp_knob(int i, int32_t e)
{
    if (i == KK_MOOD) {                               /* a mood: every knob to the sound, its time, key, scale */
        int m = sc_mood_base(kp.kv[KK_MOOD], KP_NMOOD), j;   /* (the glue has put the mood's knobs in, as
                                                       * made or as the player left them: every one to the sound) */
        (void)e;
        for (j = 1; j < SC_NUSED; j++)
            if (j != KK_SPACE)
                kp_knob(j, 0);
        kp.hold = KP_MOOD_DAY[m];
        cd_tune(KP_MOOD_SCALE[m], KP_MOOD_KEY[m]);
        fm_bed_style(FM_BED_WIND, kp.kv[KK_AIRIS]);
        cd_activity((float)kp.kv[KK_ACT] * 0.01f);
        cd_echo_amount((float)kp.kv[KK_ECHO] * 0.01f);
        fm_bed_style(FM_BED_STREAM, KP_WATER[kp.kv[KK_WATERIS] % KP_NWATER]);
        kp_koi_count(kp.kv[KK_KOI]);
        kp.sh_period = kp_knock_period();
        cd_depth_send(kp.kv[KK_DEPTH]);
        kp_mix();
        return;
    }
    if (i == KK_DEPTH || i == KK_ARC) {
        sc_knob_turn(&kp.kv[i], e, 0, 100, 1);
        cd_arc_amount((float)kp.kv[KK_ARC] * 0.01f);
        cd_depth_send(kp.kv[KK_DEPTH]);
        return;
    }
    if (i == KK_MELODY) {
        sc_knob_wrap(&kp.kv[i], e, 0, 1);
        cd_melody(kp.kv[i]);
        return;
    }
    if (i == KK_SPACE) {
        knob_set(P_REVERB, proj.par[P_REVERB] + (int)(e * sc_knob_mul));   /* (by 1) */
        return;
    }
    if (i < 0 || i >= SC_NKNOB || KP_KNOB[i][1] <= KP_KNOB[i][0])
        return;
    if (i == KK_AIRIS) {                              /* (Soft, Breeze, Insects) */
        sc_air_wrap(&kp.kv[i], e);
        fm_bed_style(FM_BED_WIND, kp.kv[i]);
        return;
    }
    if (i == KK_TONE || i == KK_WATERIS || i == KK_AIRIS)
        sc_knob_wrap(&kp.kv[i], e, KP_KNOB[i][0], KP_KNOB[i][1]);
    else
        sc_knob_turn(&kp.kv[i], e, KP_KNOB[i][0], KP_KNOB[i][1], KP_KNOB[i][2]);
    switch (i) {
    case KK_ACT: cd_activity((float)kp.kv[KK_ACT] * 0.01f); break;
    case KK_TONE: fm_part_patch(1, KP_TONE_SLOT[kp.kv[KK_TONE] % KP_NTONE]); break;
    case KK_ECHO: cd_echo_amount((float)kp.kv[KK_ECHO] * 0.01f); break;
    case KK_WATERIS: fm_bed_style(FM_BED_STREAM, KP_WATER[kp.kv[KK_WATERIS] % KP_NWATER]); break;
    case KK_AIRIS: fm_bed_style(FM_BED_WIND, kp.kv[KK_AIRIS]); break;
    case KK_KOI: kp_koi_count(kp.kv[KK_KOI]); break;
    case KK_KNOCK: kp.sh_period = kp_knock_period(); break;   /* (at once: the fill so far stays) */
    case KK_NOTES: case KK_DRONE: case KK_AIR: case KK_WATER: kp_mix(); break;
    default: break;
    }
}

SC_SMALL static void kp_knob_text(int i, char *name, char *val)
{
    static const char *const N[SC_NUSED] = {"Mood", "Activity", "Instrument", "Space", "Notes", "Drone", "Air", "Water",
                                            "Air Type", "Water Type", "Echo", "Warmth", "Koi Count", "Bamboo",
                                            "Phrases", "Tide"};
    static const char *const AIR[4] = {"Soft", "Breeze", "Breeze", "Insects"};
    if (i < 0 || i >= SC_NUSED || (i != KK_SPACE && KP_KNOB[i][1] <= KP_KNOB[i][0])) {
        name[0] = val[0] = 0;
        return;
    }
    copy_str(name, N[i], 11);
    switch (i) {
    case KK_TONE: copy_str(val, KP_TONE_NAME[kp.kv[KK_TONE] % KP_NTONE], 11); break;
    case KK_SPACE: itoa_u((uint32_t)proj.par[P_REVERB], val); break;
    case KK_MOOD: copy_str(val, KP_MOOD_NAME[sc_mood_base(kp.kv[KK_MOOD], KP_NMOOD)], 11); break;
    case KK_MELODY: copy_str(val, kp.kv[KK_MELODY] ? "Loops" : "Free", 11); break;
    case KK_WATERIS: copy_str(val, SC_WATER_NAME[KP_WATER[kp.kv[KK_WATERIS] % KP_NWATER]], 11); break;
    case KK_AIRIS: copy_str(val, AIR[kp.kv[KK_AIRIS] & 3], 11); break;
    default: itoa_u((uint32_t)kp.kv[i], val); break;
    }
}

/* a new pond (PLAY; the glue fades through dark): the stones, the pads, the moss and its light; the koi swim on
 * with their own colors */
SC_SMALL static void kp_regrow(uint32_t seed)
{
    int i;
    kp.seed = seed | 1u;
    kp_shape(kp.seed);                                /* (a new pond's shape) */
    kp_background();
    for (i = 0; i < KP_MAXK; i++)                     /* and new koi: as many, their varieties drawn again (at least
                                                       * three), each somewhere in the new pond */
        kp_k[i].on = 0;
    kp_koi_count(kp.kv[KK_KOI]);
}

/* knobs saved by an older sound version: before the pond had its own sounds, Tone meant a koto and the beds were Sakura's;
 * before that Water meant another list (the trickle is new, first), its level was for bubbles, and the last
 * knob was Memory (now Knock): those to the pond's defaults; the rest (the levels, the koi, ...) as they were */
/* a mood as it was made (the glue keeps the player's tweaks): the defaults, then the mood's own */
SC_SMALL static void kp_mood_kv(int m, int16_t *kv)
{
    kp_defaults(kv);
    m = sc_mood_base(m, KP_NMOOD);
    kv[KK_MOOD] = (int16_t)m;
    sc_mood_set(kv, KP_MOODS[m], 10);
}
static const char *kp_mood_name(int m) { return KP_MOOD_NAME[sc_mood_base(m, KP_NMOOD)]; }

static const scene_t SCENE_KOI = {"Koi", 2, kp_defaults, kp_enter, kp_leave, kp_background, kp_step, kp_draw, kp_key, kp_knob,
                                  kp_knob_text, kp_regrow, KP_SOUND, 0, 0, KP_NMOOD, kp_mood_kv, kp_mood_name, 0};
