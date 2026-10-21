/* SPDX-License-Identifier: GPL-3.0-only */
/* ZEN-1's conductor: between a scene's world and the engine. A scene reports what happened (a petal landed
 * here, this hard; two touched; a gust began); the conductor decides what, if anything, is heard. It is what
 * keeps a scene relaxing however busy its world gets. Included by ui.c.
 *
 *   pitch     always a note of the instrument's tuning (fm_key_cents of a white key: 本数, the scale, 純正律),
 *             so nothing that touches is ever a wrong note. A position (0..1) picks a degree across a range;
 *             the harmony (below) pulls it toward the chord's tones.
 *   harmony   a chord: three of the five scale degrees, moving every one to three minutes. A drone (part 2)
 *             holds its root and fifth and glides when it moves.
 *   density   a token bucket (notes a second, set by the scene's activity), a minimum gap between notes, and
 *             no degree twice running: a burst of events becomes a few well-spaced notes, never a clatter.
 *   runs      a few notes in a row, queued at their own times (a gust's harp).
 *   breath    the drone is never a steady tone (on the FM-1 a steady one was "a fan whine"). Now (after
 *             AMB-1's pad, after Eno's tape loops of unequal length) it is three voices, each breathing on its
 *             own cycle (~11-19 s: in, held, out, a rest), each taking its chord note as it starts: the chord
 *             forms and dissolves. A chord's move: a voice on a note the new chord shares holds to the end of its
 *             breath, the others let go and come back on their new notes. Now and then a voice sits a breath out
 *             (never every one: a rest, more with the arc). Its voices are kept: nothing steals them
 *             (fm_part_keep).
 *   motif     the notes a scene plays come from a small set (5..7 of the range, the chord's tones among them),
 *             so phrases come back; one or two of them change when the chord moves. A position picks among them.
 *   echo      a note comes back, softer, some seconds later (now and then an octave up), and with a high echo
 *             again: the long delay of ambalek's "fall" (MIT), done with notes, since the FM-1 has no RAM for
 *             seconds of audio. Echoes spend no tokens, never steal a voice (dropped when the voices are full),
 *             and are counted apart (cd.echoes).
 *
 *   drift     every few dawns (the scene's day; Change: off, slow every third, often every one) the key moves a
 *             fourth or a fifth (never more than a fifth from the instrument's), or the scale turns to one of
 *             the others, and the drone glides there (ambalek's "fall" moves its root and scale too). The key is
 *             an offset here; the scale is the engine's, for the scene only: leaving puts the instrument's back,
 *             and turning SELECT or ALGORITHM makes the player's choice the place to drift from again.
 *   rests     (after AMB-1) every few minutes the notes go quiet for 8-18 s (15-30 s was
 *             long enough to wonder; the world goes on, unheard);
 *             more often the more the scene rests (cd.rest_amt: the arc, the activity)
 *   arc       (after AMB-1) a slow tide, 3-6 min end to end, between sparse (fewer notes, more echo, a
 *             darker sound: the scene adds cd_arc_depth to its Depth, more rests) and full; Arc is how far it swings
 *   loops     (after AMB-1, after Eno's tape loops) Melody: Loops: four loops of unequal length (13, 17, 23
 *             and 29 steps of ~0.75 s at Time 100), each with a few of the motif's notes: the scene's events say
 *             when, the loop at the event's place which (its note now, or its next): the melody comes back but never
 *             quite repeats. A chord's move changes a note of each
 *   swell     a note that blooms instead of plucking (part 4: distant things, the night); a heavy fall's low
 *             note is a swell too, from the chord's root in the lowest octave, outside the token count but never
 *             closer than 4 s to the last.
 *
 * Voice ids: the drone FM_VOICE_ID + 0..5 (three voices, two ids each, a breath on the other), plucks and swells
 * + 16..31 (round robin), runs + 32..39, heavy + 40..43.
 * Parts: 1 the plucks, 2 the drone, 3 the accents, 4 the swells, 5 a bowl (Sakura's heavy falls). */
#define CD_NRUN 8
#define CD_NDV 3                           /* the drone's voices */
#define CD_NLOOP 4
#define CD_LOOPMAX 29
static const uint8_t CD_LOOP_LEN[CD_NLOOP] = {13, 17, 23, 29};
#define CD_NECHO 12
#define CD_MOTIF 7
#define CD_DRONE (FM_VOICE_ID + 0)
#define CD_PLUCK (FM_VOICE_ID + 16)
#define CD_RUN (FM_VOICE_ID + 32)
#define CD_HEAVY (FM_VOICE_ID + 40)

static const uint8_t CD_CHORDS[5][3] = {{0, 2, 4}, {0, 3, 4}, {1, 3, 0}, {2, 4, 1}, {0, 2, 3}};   /* degrees 0..4 */

static struct {
    float tokens, rate;                    /* notes available, notes a second */
    uint32_t gap;                          /* the least time between two plucks, ms */
    uint32_t last_ms, now_ms;              /* the last note, the conductor's clock */
    int last_deg, pluck_n, run_n;
    int lo, n;                             /* the range: white keys lo .. lo+n-1 (0..15) */
    int chord, chord_left_ms;
    int drone_on;
    float drone_lvl;
    int drone_sent;
    struct {                               /* a drone voice: breathing on its own */
        int on, bank, sit;                 /* sounding; which of its two ids; sitting this breath out */
        float t, len, hold;                /* seconds into this breath, its length, how long it sounds */
        float cents;                       /* its note, sounding or the last */
    } dv[CD_NDV];
    float rest_amt;                        /* 0..1: how much the drone and the notes rest (the arc, the activity) */
    uint32_t dv_breaths, dv_sits;          /* (the host's tests count them) */
    float act;                             /* the activity, 0..1 */
    float arc_amt, arc_ph, arc_per, arc;   /* the arc: how far (0..1), where (turns), a tide's length (s), now (-1..1) */
    float quiet_next, quiet_left;          /* the notes' rest: seconds to the next, seconds left of this one */
    uint32_t quiets;
    int loops_on;                          /* Melody: Loops */
    int8_t loop[CD_NLOOP][CD_LOOPMAX];     /* each loop's notes (white keys), -1 a silent step */
    uint8_t loop_pos[CD_NLOOP];
    float loop_acc;                        /* seconds into the step */
    uint32_t loop_notes;
    int depth_sent;                        /* the filter last asked for (the scene's Depth, and the arc's) */
    struct {
        uint32_t at;
        int key, part, vel, pan, dur;
    } run[CD_NRUN];
    uint32_t notes;                        /* every note played (the host's tests count them), echoes apart */
    int motif[CD_MOTIF], nmotif;           /* white keys, ascending */
    float echo_amt;                        /* 0..1 (the scene's Echo knob) */
    uint32_t echo_ms;                      /* this harmony's echo time */
    struct {
        uint32_t at;
        float cents;
        int vel, pan, left, part;
    } echo[CD_NECHO];
    uint32_t heavy_ms;                     /* the last heavy note */
    float key_off;                         /* the drift's key, cents from the instrument's (0, +-500, +-700) */
    float m_key;                           /* the mood's key and scale (cd_tune), for the Key page's Mood again */
    int m_scale;
    int scale_cur, change_mode, dawns;     /* the drift's scale (the engine's for now), off/slow/often, dawns seen */
    int drone_wait;                        /* ms until the drone glides to a new key or scale (the engine's turn) */
    uint32_t light_since;                  /* Natural: since when the light's scale has differed (0: it does not) */
    int light_now;                         /* Natural just chosen: the light's scale at once */
    uint32_t changes;                      /* key or scale moves (the host's tests count them) */
    uint32_t memories, knocks;             /* (the host's tests count these too) */
    int heavy_n;
    uint32_t echoes;
} cd;

/* the Key page's key (proj.key 1..12: C..B): the scale's first note (white key 0, the drone's home) on it, the
 * nearest way, whatever the mood, 本数 or the drift; 0: the mood's (and the drift's) */
static float cd_koff(void)
{
    float c;
    if (!proj.key || proj.key > 12u)
        return cd.key_off;
    c = (float)proj.key * 100.0f - 1000.0f - fm_key_cents(0);   /* (C4 is 900 cents under A4) */
    return c - 1200.0f * fm_floorf(c / 1200.0f + 0.5f);
}
/* the Key page's scale (proj.scale 1..3: Yo, In, Min'yo), else s (the mood's, or the light's) */
static int cd_scale_of(int s)
{
    static const int8_t S[3] = {SC_YO, SC_IN, SC_MINYO};
    return proj.scale && proj.scale <= 3u ? S[proj.scale - 1u] : s;
}
static int cd_drift(void) { return proj.drift < 2u ? proj.drift ^ 1u : 2; }   /* 0 off, 1 slow, 2 often */

/* a note's pitch: white key k of the scale (beyond the 16 keys, by octaves: five notes an octave) */
static float cd_cents(int white)
{
    float o = 0.0f;
    while (white >= FM_NWHITE) {
        white -= 5;
        o += 1200.0f;
    }
    while (white < 0) {
        white += 5;
        o -= 1200.0f;
    }
    return fm_key_cents(white) + o + cd_koff();
}
static int cd_in_chord(int white)
{
    int d = white % 5, i;
    for (i = 0; i < 3; i++)
        if (CD_CHORDS[cd.chord][i] == d)
            return 1;
    return 0;
}

/* the drone's two notes, kept low and warm ("a little too high pitched at times"): the root
 * is the chord root's octave that falls in 190..300 Hz (the nearest to ~240 Hz if none does, whatever the key),
 * and the second note the lowest chord tone a minor third to a fifth above it, never above 400 Hz (failing that,
 * a scale tone in that span; failing that, the root again). Cents from A4: 190 Hz -1453, 300 Hz -663, 400 Hz -165 */
#define CD_DRONE_LO (-1453.0f)
#define CD_DRONE_HI (-663.0f)
#define CD_DRONE_TOP (-165.0f)
static void cd_drone_pick(int *lo, int *hi)
{
    int root = CD_CHORDS[cd.chord][0], k, best = root, pass;
    float bd = 1e9f, r;
    for (k = root; k < FM_NWHITE; k += 5) {
        float c = cd_cents(k), d = c < CD_DRONE_LO ? CD_DRONE_LO - c : c > CD_DRONE_HI ? c - CD_DRONE_HI : 0.0f;
        d = d * 4.0f + fm_fabsf(c + 1049.0f) * 0.001f;          /* in the window first, then nearest ~240 Hz */
        if (d < bd) {
            bd = d;
            best = k;
        }
    }
    *lo = best;
    *hi = best;
    r = cd_cents(best);
    for (pass = 0; pass < 2; pass++)                 /* a chord tone first, then any scale tone */
        for (k = best + 1; k < FM_NWHITE; k++) {
            float c = cd_cents(k);
            if (c - r < 250.0f)
                continue;
            if (c - r > 710.0f || c > CD_DRONE_TOP)
                break;
            if (pass == 1 || cd_in_chord(k)) {
                *hi = k;
                return;
            }
        }
}

/* the two pitches (cents from A4), in the window whatever the octave setting (OCT- / OCT+ move a scene's notes) */
static void cd_drone_cents(float *c)            /* c[0] the root, c[1] the upper note, c[2] a third note between */
{
    int lo, hi, k, mid = -1;
    cd_drone_pick(&lo, &hi);
    for (k = lo + 1; k < FM_NWHITE && k < hi; k++)   /* a chord tone between the two, if any */
        if (cd_in_chord(k)) {
            mid = k;
            break;
        }
    c[0] = cd_cents(lo);
    c[1] = cd_cents(hi);
    c[2] = mid >= 0 ? cd_cents(mid) : cd_cents(lo) + 1200.0f;   /* (else the root's octave, sung softer) */
    while (c[0] > CD_DRONE_HI) {                     /* whole octaves, all together */
        c[0] -= 1200.0f;
        c[1] -= 1200.0f;
        c[2] -= 1200.0f;
    }
    while (c[0] + 1200.0f <= CD_DRONE_HI && c[0] < CD_DRONE_LO) {
        c[0] += 1200.0f;
        c[1] += 1200.0f;
        c[2] += 1200.0f;
    }
    if (c[1] > CD_DRONE_TOP)
        c[1] = c[1] - 1200.0f > c[0] ? c[1] - 1200.0f : c[0];
    if (c[2] > CD_DRONE_TOP)
        c[2] = c[2] - 1200.0f > c[0] ? c[2] - 1200.0f : c[1];
}

/* a drone voice's new breath: its note (the chord's now), its length, whether it sits this one out */
static void cd_dv_breath(int i, const float *cents)
{
    int k, others = 0;
    for (k = 0; k < CD_NDV; k++)
        others += k != i && cd.dv[k].on;
    cd.dv[i].t = 0.0f;
    cd.dv[i].len = (11.0f + 2.5f * (float)i) * (0.85f + 0.3f * sc_frand());   /* (unequal: ~11, ~13.5, ~16 s, each
                                                                              * breath a little different) */
    cd.dv[i].hold = cd.dv[i].len * (0.55f + 0.1f * sc_frand());
    cd.dv[i].sit = others > 0 && sc_frand() < 0.08f + 0.3f * cd.rest_amt;   /* (a rest: never every voice) */
    cd.dv[i].cents = cents[i];
    cd.dv_breaths++;
    if (cd.dv[i].sit) {
        cd.dv_sits++;
        return;
    }
    cd.dv[i].bank ^= 1;
    fm_voice(CD_DRONE + 2 * i + cd.dv[i].bank, 2, cents[i], i == 2 ? 50 : 60 + (int)(sc_rnd() % 8u),
             i == 0 ? 64 : i == 1 ? 84 : 44, 0);
    cd.dv[i].on = 1;
}
static void cd_dv_off(int i)
{
    if (cd.dv[i].on)
        fm_voice_off(CD_DRONE + 2 * i + cd.dv[i].bank);   /* (part 2's release, ~4 s: it fades) */
    cd.dv[i].on = 0;
}

/* the drone to the chord now (breathing voices). change 0: the first breaths, staggered; 1: the chord (or
 * the key, the scale) moved: a voice whose note the new chord keeps holds on; the others let go and breathe in
 * again on their new notes in 1-3 s (a chord change never slides) */
static void cd_drone_set(int change)
{
    int i;
    float cents[3];
    if (!cd.drone_on)
        return;
    cd_drone_cents(cents);
    for (i = 0; i < CD_NDV; i++) {
        if (!change) {
            cd.dv[i].on = 0;
            cd.dv[i].cents = cents[i];
            cd.dv[i].t = 0.0f;
            cd.dv[i].len = 1.0f + 2.5f * (float)i;   /* (the first breaths staggered: 1, 3.5, 6 s) */
            cd.dv[i].hold = 0.0f;
            continue;
        }
        if (fm_fabsf(cd.dv[i].cents - cents[i]) < 1.0f)
            continue;                                 /* (the new chord keeps it) */
        cd_dv_off(i);
        cd.dv[i].cents = cents[i];
        cd.dv[i].t = 0.0f;
        cd.dv[i].hold = 0.0f;
        cd.dv[i].len = 1.0f + 2.0f * sc_frand();      /* (back in soon, on its new note) */
    }
    if (!change)
        cd_dv_breath(0, cents);                       /* (one at once: the scene is never without its drone) */
}

static void cd_activity(float a);

static void cd_motif_sort(void)
{
    int i, j;
    for (i = 1; i < cd.nmotif; i++)
        for (j = i; j > 0 && cd.motif[j - 1] > cd.motif[j]; j--) {
            int t = cd.motif[j];
            cd.motif[j] = cd.motif[j - 1];
            cd.motif[j - 1] = t;
        }
}
static int cd_motif_has(int k)
{
    int i;
    for (i = 0; i < cd.nmotif; i++)
        if (cd.motif[i] == k)
            return 1;
    return 0;
}
/* a note of the range not yet in the motif, a chord tone more often than not (bounded: 24 tries) */
static int cd_motif_candidate(void)
{
    int t, k = cd.lo;
    for (t = 0; t < 24; t++) {
        k = cd.lo + (int)(sc_rnd() % (uint32_t)cd.n);
        if (cd_motif_has(k))
            continue;
        if (cd_in_chord(k) || sc_frand() < 0.35f)
            return k;
    }
    return cd_motif_has(k) ? -1 : k;
}
static void cd_motif_new(void)
{
    int i, want = 5 + (int)(sc_rnd() % 3u);
    if (want > cd.n)
        want = cd.n;
    cd.nmotif = 0;
    for (i = 0; i < 48 && cd.nmotif < want; i++) {
        int k = cd_motif_candidate();
        if (k >= 0)
            cd.motif[cd.nmotif++] = k;
    }
    cd_motif_sort();
}
static void cd_motif_shift(void)            /* the chord moved: one or two notes change, the rest stay */
{
    int c = 1 + (int)(sc_rnd() & 1u), i;
    for (i = 0; i < c && cd.nmotif > 0; i++) {
        int k = cd_motif_candidate();
        if (k >= 0)
            cd.motif[sc_rnd() % (uint32_t)cd.nmotif] = k;
    }
    cd_motif_sort();
}

/* the instrument's key and scale again (leaving, or the player turning SELECT / ALGORITHM) */
static void cd_drift_home(void)
{
    cd.key_off = 0.0f;
    cd.scale_cur = proj.par[P_SCALE];
    fm_set(P_SCALE, cd.scale_cur);
    cd.drone_wait = 60;
}
static void cd_change_mode(int m) { cd.change_mode = m < 0 ? 0 : m > 2 ? 2 : m; }
/* a mood's key and scale (they come with the mood; SELECT and ALGORITHM turn Activity and
 * the Mood): a named mood holds its own scale (SC_*) and key (cents from the player's: 0, +-500, +-700); scale < 0
 * (Natural): from the player's own, drifting slowly as before */
static void cd_tune(int scale, float key_off)
{
    int j;
    cd.m_scale = scale;
    cd.m_key = key_off;
    if (scale < 0) {                          /* Natural: the key home (it drifts from there), the scale the
                                                * light's (cd_light) */
        cd_change_mode(1);
        cd.light_now = 1;
        if (cd.key_off != 0.0f) {
            cd.key_off = 0.0f;
            cd.drone_wait = 60;
        }
        return;
    }
    cd_change_mode(0);
    scale = cd_scale_of(scale);
    if (cd.key_off == key_off && cd.scale_cur == scale)
        return;
    cd.key_off = key_off;
    cd.scale_cur = scale;
    fm_set(P_SCALE, scale);
    for (j = 0; j < CD_NECHO; j++)            /* nothing of the old key echoes into the new */
        cd.echo[j].at = 0;
    cd.drone_wait = 60;
}

/* a dawn in the scene: perhaps a new key or scale */
static void cd_dawn(void)
{
    int j;
    if (!cd.change_mode || !cd_drift() || (proj.key && proj.key <= 12u))
        return;                               /* (no drift, or the Key page holds the key) */
    if (cd_drift() == 1 && ++cd.dawns % 3 != 0)
        return;
    if (sc_frand() < 0.6f) {                  /* the key: a fourth or fifth, back home more often than away */
        static const float MOVE[4] = {700.0f, -500.0f, 500.0f, -700.0f};
        float to = cd.key_off;
        if (cd.key_off != 0.0f && sc_frand() < 0.6f)
            to = 0.0f;
        else
            for (j = 0; j < 8; j++) {
                float t = cd.key_off + MOVE[sc_rnd() & 3u];
                if (t >= -700.0f && t <= 700.0f && t != cd.key_off) {
                    to = t;
                    break;
                }
            }
        cd.key_off = to;
    } else
        return;                               /* (the scale is the light's now: cd_light) */
    cd.changes++;
    for (j = 0; j < CD_NECHO; j++)            /* nothing of the old key echoes into the new */
        cd.echo[j].at = 0;
    cd.drone_wait = 60;                       /* the engine takes the scale at its next block: then the glide */
}

static void cd_init(uint32_t seed)
{
    memset(&cd, 0, sizeof cd);
    cd_activity(0.25f);
    cd.tokens = 2.0f;
    cd.lo = 3;
    cd.n = 10;
    cd.last_deg = -1;
    cd.chord = proj.chord ? 0 : (int)(seed % 5u);
    cd.m_scale = -1;
    cd.echo_ms = 6000u + seed % 2500u;
    cd_motif_new();
    cd.drone_lvl = 60.0f;
    cd.drone_sent = -1;
    cd.depth_sent = -1;
    cd.arc_amt = 0.3f;
    cd.arc_ph = (float)(seed % 1000u) * 0.001f;
    cd.arc_per = 180.0f + 180.0f * (float)(seed % 997u) / 997.0f;
    cd.quiet_next = 240.0f + 240.0f * (float)(seed % 991u) / 991.0f;
    cd.chord_left_ms = 60000 + (int)(seed % 60000u);
    cd.scale_cur = proj.par[P_SCALE];
    cd.light_now = 1;                         /* (Natural: the light's scale from the start) */
}

/* activity 0..1: from one note every five seconds or so, unhurried, up to ~3 a second (on the
 * FM-1: the default should leave room; busier is a turn of the knob) */
static void cd_activity(float a)
{
    cd.act = fm_clampf(a, 0.0f, 1.0f);
    cd.rate = 0.15f + 2.85f * a;
    cd.gap = 140u + (uint32_t)((1.0f - a) * (1.0f - a) * 1500.0f);
}
static void cd_range(int lo, int n)
{
    cd.lo = lo < 0 ? 0 : lo > 12 ? 12 : lo;
    cd.n = n < 3 ? 3 : cd.lo + n > FM_NWHITE ? FM_NWHITE - cd.lo : n;
    cd_motif_new();
}
static void cd_echo_amount(float a) { cd.echo_amt = fm_clampf(a, 0.0f, 1.0f); }

static void cd_drone_level(int pct) { cd.drone_lvl = (float)pct; }

static void cd_drone(int on)
{
    int k;
    if (on == cd.drone_on)
        return;
    cd.drone_on = on;
    fm_part_keep(2, on);
    if (on)
        cd_drone_set(0);
    else
        for (k = 0; k < 2 * CD_NDV; k++)
            fm_voice_off(CD_DRONE + k);
}

/* the loops: a few of the motif's notes on each, at random steps (three to five: the longer, the more) */
static void cd_loops_fill(void)
{
    int l, i, n;
    for (l = 0; l < CD_NLOOP; l++) {
        for (i = 0; i < CD_LOOPMAX; i++)
            cd.loop[l][i] = -1;
        n = 3 + l / 2 + (int)(sc_rnd() % 2u);
        for (i = 0; i < n; i++) {
            int s = (int)(sc_rnd() % CD_LOOP_LEN[l]);
            cd.loop[l][s] = (int8_t)(cd.nmotif > 0 ? cd.motif[sc_rnd() % (uint32_t)cd.nmotif] : cd.lo + (int)(sc_rnd() % (uint32_t)cd.n));
        }
        cd.loop_pos[l] = (uint8_t)(sc_rnd() % CD_LOOP_LEN[l]);
    }
}
static void cd_loops_shift(void)                /* the chord moved: a note of each loop to a note of the motif now */
{
    int l, i;
    for (l = 0; l < CD_NLOOP; l++)
        for (i = 0; i < CD_LOOP_LEN[l]; i++) {
            int s = (int)(sc_rnd() % CD_LOOP_LEN[l]);
            if (cd.loop[l][s] >= 0) {
                cd.loop[l][s] = (int8_t)(cd.nmotif > 0 ? cd.motif[sc_rnd() % (uint32_t)cd.nmotif] : cd.loop[l][s]);
                break;
            }
        }
}
static void cd_melody(int loops)                /* Melody: Free (0) or Loops (1) */
{
    if (loops && !cd.loops_on)
        cd_loops_fill();
    cd.loops_on = loops != 0;
}
static int cd_loop_pick(float pos)              /* the note of the loop at this place: its step now, or its next */
{
    int l = (int)(fm_clampf(pos, 0.0f, 0.999f) * (float)CD_NLOOP), i, s;
    for (i = 0; i < CD_LOOPMAX; i++) {
        s = (cd.loop_pos[l] + i) % CD_LOOP_LEN[l];
        if (cd.loop[l][s] >= 0) {
            cd.loop_notes++;
            return cd.loop[l][s];
        }
    }
    return cd.lo + cd.n / 2;
}
static void cd_arc_amount(float a) { cd.arc_amt = fm_clampf(a, 0.0f, 1.0f); }
static int cd_arc_depth(void) { return (int)(30.0f * fm_maxf(0.0f, -cd.arc)); }   /* the sparse end: darker */
static int cd_quiet(void) { return cd.quiet_left > 0.0f; }
static void cd_depth_send(int base)             /* a scene's Depth (its own, its weather's), and the arc's: when it moves */
{
    int d = base + cd_arc_depth();
    d = d > 100 ? 100 : d < 0 ? 0 : d;
    if (d != cd.depth_sent) {
        fm_depth(d);
        cd.depth_sent = d;
    }
}   /* the notes resting (the scene's own: unheard) */

/* the degree for a position: the motif's note there (never the last one again); without a motif, the nearest
 * white key in the range, moved to a chord tone next to it most of the time */
static int cd_pick(float pos)
{
    int k = cd.lo + (int)(pos * (float)cd.n), d;
    if (cd.loops_on)
        return cd_loop_pick(pos);
    if (cd.nmotif > 1) {
        int i = (int)(fm_clampf(pos, 0.0f, 0.999f) * (float)cd.nmotif);
        if (cd.motif[i] == cd.last_deg)
            i += (i + 1 < cd.nmotif) ? 1 : -1;
        return cd.motif[i];
    }
    if (k >= cd.lo + cd.n)
        k = cd.lo + cd.n - 1;
    if (k < cd.lo)
        k = cd.lo;
    if (!cd_in_chord(k) && sc_frand() < 0.7f) {
        for (d = 1; d < 3; d++) {
            if (k + d < cd.lo + cd.n && cd_in_chord(k + d)) {
                k += d;
                break;
            }
            if (k - d >= cd.lo && cd_in_chord(k - d)) {
                k -= d;
                break;
            }
        }
    }
    if (k == cd.last_deg)
        k += (k + 1 < cd.lo + cd.n) ? 1 : -1;
    return k;
}

static int cd_allow(uint32_t gap_ms)
{
    if (cd_quiet())
        return 0;
    if (cd.tokens < 1.0f || cd.now_ms - cd.last_ms < gap_ms)
        return 0;
    cd.tokens -= 1.0f;
    cd.last_ms = cd.now_ms;
    return 1;
}

static int cd_pan(float pos) { return (int)(10.0f + 107.0f * fm_clampf(pos, 0.0f, 1.0f)); }

static void cd_echo_add(float cents, int vel, int pan, int part)
{
    int j;
    float ea = fm_minf(1.0f, cd.echo_amt + 0.25f * fm_maxf(0.0f, -cd.arc));   /* (the arc's sparse end: more echo) */
    if (ea <= 0.0f)
        return;
    for (j = 0; j < CD_NECHO; j++)
        if (!cd.echo[j].at) {
            cd.echo[j].at = cd.now_ms + cd.echo_ms + (sc_rnd() % 300u);
            cd.echo[j].cents = cents;
            cd.echo[j].vel = (int)((float)vel * (0.4f + 0.3f * ea));
            cd.echo[j].pan = 127 - pan / 2 - 32;     /* from the other side, nearer the middle */
            cd.echo[j].left = ea > 0.6f ? 2 : 1;
            cd.echo[j].part = part;
            return;
        }
}

/* a note at a position (0..1 across), strength 0..1, on part 1 (a pluck) or 4 (a swell): 1 if it was played */
static int cd_note(float pos, float strength, int dur_ms, int part)
{
    int k, vel;
    float c;
    if (!cd_allow(cd.gap))
        return 0;
    k = cd_pick(pos);
    vel = 38 + (int)(70.0f * fm_clampf(strength, 0.0f, 1.0f));
    fm_voice(CD_PLUCK + (cd.pluck_n++ & 15), part, cd_cents(k), vel, cd_pan(pos), dur_ms);
    cd.last_deg = k;
    cd.notes++;
    c = cd_cents(k);
    if (sc_frand() < 0.3f * cd.echo_amt && k + 5 < FM_NWHITE)
        c = cd_cents(k + 5);                  /* the echo an octave up (five white keys) */
    cd_echo_add(c, vel, cd_pan(pos), part);
    return 1;
}
static int cd_pluck(float pos, float strength, int dur_ms) { return cd_note(pos, strength, dur_ms, 1); }

/* a note on a given white key, for a scene that sets its own pitch (the koi pond: the ring the food lay on, so a
 * note planted from the keyboard is the note heard). The food is the pacing here: no tokens, only a short gap, so a
 * feeding is a little flurry; echoed like any note */
static int cd_note_key(int k, float pos, float strength, int dur_ms, int part, uint32_t gap_ms)
{
    int vel;
    float c;
    if (cd.now_ms - cd.last_ms < gap_ms && cd.last_ms)
        return 0;
    if (gap_ms && cd_quiet())
        return 0;                             /* (the scene's own, while the notes rest; a key's are always heard) */
    cd.last_ms = cd.now_ms;
    k = k < 0 ? 0 : k > 25 ? 25 : k;
    vel = 38 + (int)(70.0f * fm_clampf(strength, 0.0f, 1.0f));
    if (!cd_in_chord(k)) {                    /* a note outside the chord: a passing note, softer and shorter */
        vel = vel * 2 / 3;
        dur_ms = dur_ms * 3 / 5;
    }
    c = cd_cents(k);
    fm_voice(CD_PLUCK + (cd.pluck_n++ & 15), part, c, vel, cd_pan(pos), dur_ms);
    cd.last_deg = k;
    cd.notes++;
    if (sc_frand() < 0.3f * cd.echo_amt && k + 5 <= 25)
        c = cd_cents(k + 5);
    cd_echo_add(c, vel, cd_pan(pos), part);
    return 1;
}

/* Natural: the scale the light asks for (a scene's, each step: Yo by day, Min'yo from golden hour through the night),
 * taken when the notes rest (within a minute anyway: or at once when Natural is just chosen), the drone gliding to it.
 * The three scales share three notes: only two move */
static void cd_light(int scale)
{
    int j;
    scale = cd_scale_of(scale);
    if (!cd.change_mode || scale == cd.scale_cur) {
        cd.light_since = 0;
        cd.light_now = 0;
        return;
    }
    if (!cd.light_since)
        cd.light_since = cd.now_ms | 1u;
    if (!cd.light_now && !cd_quiet() && cd.now_ms - cd.light_since < 60000u)
        return;
    cd.light_now = 0;
    cd.light_since = 0;
    cd.scale_cur = scale;
    fm_set(P_SCALE, scale);
    for (j = 0; j < CD_NECHO; j++)            /* nothing of the old scale echoes into the new */
        cd.echo[j].at = 0;
    cd.drone_wait = 60;
}
/* the scene's keys: white key b plays note b + shift (sixteen notes, each its own, rising) */

/* a remembered note (the koi pond's uneaten food): a soft swell, no echo, outside the gap */
static void cd_memory(int k, float pos)
{
    k = k < 0 ? 0 : k >= FM_NWHITE ? FM_NWHITE - 1 : k;
    fm_voice(CD_RUN + (cd.run_n++ & 7), 4, cd_cents(k), 30 + (int)(sc_rnd() % 14u), cd_pan(pos), 2200);
    cd.memories++;
}

/* a knock (the shishi-odoshi's bamboo on its stone): a recorded hollow bamboo-like "tok" (CC0; the FM one
 * "sounds bell-like"), a little faster or slower each time, on one-shot slot 0 */
static void cd_knock(int k, float pos)
{
    (void)k;
    fm_shot(0, FM_SHOT_KNOCK, (int)(sc_rnd() % (uint32_t)fm_shot_count(FM_SHOT_KNOCK)), 104, cd_pan(pos),
            90 + (int)(sc_rnd() % 14u));
    cd.knocks++;
}
static int cd_swell(float pos, float strength, int dur_ms) { return cd_note(pos, strength, dur_ms, 4); }

/* a heavy fall: the chord's root, low (its octave in 130..260 Hz, the lowest the FM-1's speaker carries through
 * the swell's upper partials), a swell; its own spacing, not the tokens */
static int cd_heavy(float pos, float strength)
{
    int vel;
    float c = cd_cents(CD_CHORDS[cd.chord][0]);
    if (cd.now_ms - cd.heavy_ms < 4000u && cd.heavy_ms)
        return 0;
    cd.heavy_ms = cd.now_ms;
    while (c > -875.0f)                       /* 260 Hz */
        c -= 1200.0f;
    while (c < -2075.0f)                      /* 130 Hz */
        c += 1200.0f;
    vel = 50 + (int)(50.0f * fm_clampf(strength, 0.0f, 1.0f));
    fm_voice(CD_HEAVY + (cd.heavy_n++ & 3), 4, c, vel, cd_pan(pos), 2500);
    fm_voice(FM_VOICE_ID + 45, 5, c + 1200.0f, vel * 3 / 4, cd_pan(pos), 2600);   /* and a soft, deep bowl an octave
                                                                                 * up, on part 5 (wanted it more special) */
    cd.notes++;
    cd_echo_add(c, vel, cd_pan(pos), 4);
    return 1;
}

/* two things touching: a soft note a chord tone above the last, quieter and on the accent part */
static int cd_touch(float pos, float strength)
{
    int k;
    if (cd.last_deg < 0 || !cd_allow(220u))
        return 0;
    k = cd.last_deg + 2;
    while (k >= FM_NWHITE)
        k -= 5;
    fm_voice(CD_RUN + (cd.run_n++ & 7), 3, cd_cents(k), 28 + (int)(40.0f * fm_clampf(strength, 0.0f, 1.0f)), cd_pan(pos), 400);
    cd.notes++;
    return 1;
}

/* a run of n notes, up (dir 1) or down (-1), from a position, step_ms apart, on part 3 (a gust's harp) */
static void cd_run(float pos, int dir, int n, uint32_t step_ms, float strength)
{
    int i, j, k = cd_pick(pos);
    for (i = 0, j = 0; i < n && j < CD_NRUN; j++) {
        if (cd.run[j].at)
            continue;
        cd.run[j].at = cd.now_ms + 1u + (uint32_t)i * step_ms;
        cd.run[j].key = k;
        cd.run[j].vel = 34 + (int)(36.0f * strength) - i * 3;
        cd.run[j].pan = cd_pan(pos + (float)(dir * i) * 0.06f);
        cd.run[j].dur = 700;
        do {
            k += dir;
        } while (k >= 0 && k < FM_NWHITE && !cd_in_chord(k) && sc_frand() < 0.5f);
        if (k < 0 || k >= FM_NWHITE)
            break;
        i++;
    }
}

/* a wind chime (a gust in Sakura's tree): n strikes at uneven gaps, each a chord tone from the upper half of the
 * range, soft, on part 3 (the run slots). A chime is struck by the wind, not played up a scale; one or two a gust
 * (a run of 3..5 was busy) */
static void cd_chime(float pos, int n, float strength)
{
    int i, j, t, k;
    uint32_t at = cd.now_ms + 1u;
    if (cd.n < 2)
        return;
    for (i = 0, j = 0; i < n && j < CD_NRUN; j++) {
        if (cd.run[j].at)
            continue;
        k = cd.lo + cd.n / 2;
        for (t = 0; t < 8; t++) {                    /* a chord tone, not the last note (bounded) */
            k = cd.lo + cd.n / 2 + (int)(sc_rnd() % (uint32_t)(cd.n - cd.n / 2));
            if (cd_in_chord(k) && k != cd.last_deg)
                break;
        }
        cd.run[j].at = at;
        cd.run[j].key = k;
        cd.run[j].vel = 30 + (int)(30.0f * fm_clampf(strength, 0.0f, 1.0f)) - i * 6;
        cd.run[j].pan = cd_pan(pos + (sc_frand() - 0.5f) * 0.2f);
        cd.run[j].dur = 900;
        at += 180u + sc_rnd() % 520u;
        i++;
    }
}

/* the conductor's time moves on: tokens refill, runs play, the harmony drifts */
static void cd_step(uint32_t dt_ms)
{
    int j;
    cd.now_ms += dt_ms;
    {   /* the arc, the rests, the loops' clock */
        float dt = (float)dt_ms * 0.001f;
        cd.arc_ph += dt / cd.arc_per;
        if (cd.arc_ph >= 1.0f) {
            cd.arc_ph -= 1.0f;
            cd.arc_per = 180.0f + 180.0f * sc_frand();
        }
        cd.arc = cd.arc_amt * fm_sin_turns(cd.arc_ph);
        cd.rest_amt = fm_clampf(0.12f + 0.6f * cd.arc_amt * (0.5f - 0.5f * fm_sin_turns(cd.arc_ph)) - 0.15f * cd.act,
                                0.0f, 1.0f);
        if (cd.quiet_left > 0.0f)
            cd.quiet_left -= dt;
        else if ((cd.quiet_next -= dt) <= 0.0f) {
            cd.quiet_left = 8.0f + 10.0f * sc_frand();
            cd.quiet_next = 240.0f + 240.0f * (1.0f - cd.rest_amt) * sc_frand() + 30.0f;
            cd.quiets++;
        }
        cd.loop_acc += dt;
        while (cd.loop_acc >= 0.75f) {
            int l;
            cd.loop_acc -= 0.75f;
            for (l = 0; l < CD_NLOOP; l++)
                cd.loop_pos[l] = (uint8_t)((cd.loop_pos[l] + 1) % CD_LOOP_LEN[l]);
        }
    }
    cd.tokens += cd.rate * (1.0f + 0.6f * cd.arc) * (float)dt_ms * 0.001f;
    if (cd.tokens > 3.0f)
        cd.tokens = 3.0f;
    for (j = 0; j < CD_NRUN; j++)
        if (cd.run[j].at && (int32_t)(cd.now_ms - cd.run[j].at) >= 0) {
            fm_voice(CD_RUN + (cd.run_n++ & 7), 3, cd_cents(cd.run[j].key), cd.run[j].vel < 12 ? 12 : cd.run[j].vel, cd.run[j].pan,
                     cd.run[j].dur);
            cd.run[j].at = 0;
            cd.notes++;
        }
    if (cd.drone_on) {                       /* the breaths: each voice in, held, out, a rest, on its own clock */
        float dt = (float)dt_ms * 0.001f, cents[3];
        int send = (int)(cd.drone_lvl + 0.5f), need = 0, sounding = 0;
        for (j = 0; j < CD_NDV; j++)
            sounding += cd.dv[j].on;
        for (j = 0; j < CD_NDV; j++) {
            cd.dv[j].t += dt;
            if (cd.dv[j].on && cd.dv[j].t >= cd.dv[j].hold) {
                if (sounding > 1 || cd.dv[j].t >= cd.dv[j].len) {
                    cd_dv_off(j);                     /* breathing out (the last one holds until another is in) */
                    sounding--;
                }
            }
            if (cd.dv[j].t >= cd.dv[j].len)
                need |= 1 << j;
        }
        if (need) {
            cd_drone_cents(cents);
            for (j = 0; j < CD_NDV; j++)
                if (need >> j & 1)
                    cd_dv_breath(j, cents);
        }
        if (send != cd.drone_sent) {
            fm_part_level(2, send);
            cd.drone_sent = send;
        }
    }
    for (j = 0; j < CD_NECHO; j++)
        if (cd.echo[j].at && (int32_t)(cd.now_ms - cd.echo[j].at) >= 0) {
            if (fm_nvoices < FM_NVOICE - 1 && cd.echo[j].vel >= 10) {   /* never steals: dropped when full */
                fm_voice(CD_PLUCK + (cd.pluck_n++ & 15), cd.echo[j].part, cd.echo[j].cents, cd.echo[j].vel, cd.echo[j].pan,
                         1200);
                cd.echoes++;
            }
            if (--cd.echo[j].left > 0) {
                cd.echo[j].at += cd.echo_ms;
                cd.echo[j].vel = cd.echo[j].vel * 55 / 100;
            } else {
                cd.echo[j].at = 0;
            }
        }
    if (cd.drone_wait > 0) {
        cd.drone_wait -= (int)dt_ms;
        if (cd.drone_wait <= 0)
            cd_drone_set(1);                  /* to the new key or scale: a crossfade */
    }
    cd.chord_left_ms -= (int)dt_ms;
    if (cd.chord_left_ms <= 0) {
        cd.chord = proj.chord ? 0 : (cd.chord + 1 + (int)(sc_rnd() % 3u)) % 5;   /* (Held: the home chord) */
        cd.chord_left_ms = 60000 + (int)(sc_rnd() % 120000u);
        cd_drone_set(1);
        cd_motif_shift();
        if (cd.loops_on)
            cd_loops_shift();
        cd.echo_ms = 6000u + sc_rnd() % 2500u;
    }
}

/* the Key page turned: the mood's key and scale again through it, the drone gliding there, nothing old echoing */
static void cd_retune(void)
{
    int j;
    if (proj.chord)
        cd.chord = 0;
    cd_tune(cd.m_scale, cd.m_key);
    for (j = 0; j < CD_NECHO; j++)
        cd.echo[j].at = 0;
    cd.drone_wait = 60;
}

static void cd_stop(void)
{
    int i;
    cd.key_off = 0.0f;
    fm_set(P_SCALE, proj.par[P_SCALE]);       /* the instrument's scale, whatever the drift did */
    cd.drone_on = 0;
    fm_part_keep(2, 0);
    fm_voice_all_off();                       /* every scene note, whoever started it: one command (96 would flood
                                               * the engine's queue when a scene leaves and the next one enters) */
    for (i = 0; i < CD_NRUN; i++)
        cd.run[i].at = 0;
    for (i = 0; i < CD_NECHO; i++)
        cd.echo[i].at = 0;
}
