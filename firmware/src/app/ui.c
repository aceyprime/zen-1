/* SPDX-License-Identifier: GPL-3.0-only */
/* FuMi-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too. The control map (docs/research/00-START-HERE.md):
 *
 *   white keys     mi fa la ti do x 3 and a top mi (the ST-50's lower row)
 *   black keys     the upper row fa# sol ti-flat do# re, or ornaments (SEQ switches)
 *   SELECT 本数    PRESETS 音色 (voice)    ALGORITHM scale IN / YO / MIN'YO
 *   KNOB 1-4       the page's four values; HOME: 余韻, trill rate, vibrato depth, reverb
 *   LFO ビブラート   ARP トリラー   GLO 単音/和音   SEL 調律 (tap: 平均律 / 純正律; hold: the tuning page)
 *   OCT- / OCT+    a note held: pitch bend down / up (sprung); none held: octave down / up
 *   HOME FX EDIT   the pages: HOME, SOUND, BEND.  SAVE saves (and it autosaves when quiet)
 *   OCT- + OCT+ held 5 s: update mode (main_fm1.c; never reassigned) */

enum { V_HOME, V_SOUND, V_BEND, V_TUNING, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Home", "Sound", "Bend", "Tuning"};
#define K_NONE 255
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_YOIN, P_TRILL_RATE, P_VIB_DEPTH, P_REVERB},
    {P_LOWCUT, P_HIGHCUT, P_CHARACTER, P_REV_SIZE},
    {P_BEND_UP, P_BEND_DOWN, P_BEND_TIME, P_SLIDE_TIME},
    {P_TUNING, P_DEPTH, P_FINE, P_A4},
};

static const uint8_t WHITE_K[FM_NWHITE] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[FM_NBLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
/* ornament mode: what each black key does (ORN_N = nothing yet) */
static const uint8_t BLACK_ORN[FM_NBLACK] = {ORN_VIB, ORN_TRILL, ORN_DAMP, ORN_STRONG, ORN_N,
                                             ORN_VIB, ORN_TRILL, ORN_DAMP, ORN_STRONG, ORN_N, ORN_N};
static const char *const DEG_NAME[5] = {"mi", "fa", "la", "ti", "do"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

/* the screen's colors: paper, ink, and one warm color for what sounds */
#define K_BG RGB(250, 246, 236)
#define K_PANEL RGB(236, 229, 214)
#define K_LINE RGB(214, 204, 186)
#define K_DIM RGB(150, 138, 120)
#define K_TEXT RGB(64, 52, 44)
#define K_WHITE RGB(255, 255, 255)
#define K_KOTO RGB(196, 72, 48)
#define K_KOTO_T RGB(240, 200, 184)
#define K_TONIC RGB(120, 100, 80)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint32_t sel_t;                    /* SEL pressed at (its hold opens the tuning page) */
    uint32_t home_t;                   /* HOME pressed at (its hold switches instrument <-> ambient) */
    uint32_t save_t;                   /* SAVE pressed at (held 3 s: everything back to the defaults) */
    uint8_t reset_shown, reset_done;
    uint8_t oct_role[2];               /* OCT- / OCT+ while held: 0 up, 1 bend, 2 octave */
    uint8_t tuning_prev;               /* the 純正律 to come back to after a tap to 平均律 */
    uint8_t dirty;
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];
} ui;

#if ZN_HUD
static struct {
    uint32_t peak, fps, frames, t0, saves;
    uint8_t show;                       /* over a scene: hidden at power-on, EDIT shows it */
} hud;
#endif

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
}
void ui_say(const char *a, const char *b) { say(a, b); }
int ui_dirty(void) { return ui.dirty != 0; }
static void mark_dirty(void) { ui.dirty = 1; }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void copy_str(char *d, const char *s, int max)
{
    int k = 0;
    while (s[k] && k < max) {
        d[k] = s[k];
        k++;
    }
    d[k] = 0;
}

/* ZEN-1's scenes (ambient mode): the renderer and the scenes, in this unit */
#include "../dsp/fastmath.h"
static void knob_set(int p, int v);
#include "render8.c"
#include "scene.h"
#include "../dsp/zen_patches.h"                 /* ZEN-1's own patch ids (ZN_SOFT_PAD ...) */
#include "conductor.c"
#include "scene_sakura.c"
#include "scene_koi.c"
#include "scene_hotaru.c"
static const scene_t *const SCENES[] = {&SCENE_SAKURA, &SCENE_KOI, &SCENE_HOTARU};   /* (the Bench load test went:
                                                                                     * "jarring" between scenes; its slot 1 is free) */
#define NSCENES ((int)(sizeof SCENES / sizeof SCENES[0]))

/* ------------------------------------------------------------- values --- */
static void knob_set(int p, int v)
{
    const fm_param_t *pi = fm_param_info(p);
    v = v < pi->lo ? pi->lo : v > pi->hi ? pi->hi : v;
    if (v == proj.par[p])
        return;
    proj.par[p] = (int16_t)v;
    fm_set(p, v);
    mark_dirty();
}

/* knob acceleration by speed: a slow turn is one step a detent, a quick one up to 6 */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (range <= 24 || !a || accel_off)
        return s;
    if (a > 1)
        dt /= a;
    m = dt < 15u ? 6u : dt < 30u ? 4u : dt < 60u ? 2u : 1u;
    return s * (int32_t)m;
}

/* a list (an instrument, a scale, a tuning, a bend target): turning past either end comes round to the other
 * ("rotate back to the start when they hit the last option"); a number stops at its ends */
static int param_is_list(int p) { return p == P_VOICE || p == P_SCALE || p == P_TUNING || p == P_BEND_UP; }
static void param_step(int p, int32_t e)
{
    const fm_param_t *pi = fm_param_info(p);
    if (param_is_list(p)) {
        int32_t n = pi->hi - pi->lo + 1, v = ((proj.par[p] - pi->lo + e) % n + n) % n;
        knob_set(p, pi->lo + (int)v);
    } else {
        knob_set(p, proj.par[p] + (int)e);
    }
}

static void turn(int role, int p, int32_t e)
{
    const fm_param_t *pi;
    if (!e || p == K_NONE)
        return;
    pi = fm_param_info(p);
    if (param_is_list(p))
        param_step(p, e);                          /* (one step a detent: no acceleration on a list) */
    else
        knob_set(p, proj.par[p] + accel(role, e, pi->hi - pi->lo));
}

static void toggle(int p, const char *what)
{
    knob_set(p, !proj.par[p]);
    say(what, proj.par[p] ? "ON" : "OFF");
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static int any_key_held(void) { return (ui.keys & ((1u << NKEYS) - 1u)) != 0; }

static void oct_press(int which)                   /* which: 0 OCT-, 1 OCT+ (bend.h BEND_DOWN / BEND_UP) */
{
    if (ui.oct_role[!which]) {                     /* the other is held: reserved (update mode, main_fm1.c) */
        ui.oct_role[which] = 2;
        fm_bend(which, 1, 0);                      /* the engine lets its bend go */
        return;
    }
    if (any_key_held()) {
        ui.oct_role[which] = 1;
        fm_bend(which, 1, 1);
        return;
    }
    ui.oct_role[which] = 2;
    knob_set(P_OCTAVE, proj.par[P_OCTAVE] + (which ? 1 : -1));
    {
        char t[12];
        fm_param_text(P_OCTAVE, proj.par[P_OCTAVE], t);
        say("OCTAVE", t);
    }
}

static void oct_release(int which)
{
    if (ui.oct_role[which] == 1)
        fm_bend(which, 0, any_key_held());
    ui.oct_role[which] = 0;
}

/* The three selectors move one step at a time, at most one every SEL_STEP_MS: on the FM-1 a single click of
 * PRESETS sometimes arrived as two counts and skipped an instrument (1.0.2). A person clicks a detented
 * encoder at well under 16 a second, so nothing deliberate is lost; a count inside the window is dropped. */
#define SEL_STEP_MS 60u
static int32_t sel_step(int role)
{
    static uint32_t next[NE];                      /* when the next step may be taken (0: at once) */
    int32_t e = plat_enc(role);
    uint32_t now = plat_ms();
    if (e == 0)
        return 0;
    if ((int32_t)(now - next[role]) < 0)
        return 0;
    next[role] = now + SEL_STEP_MS;
    return e > 0 ? 1 : -1;
}

static void sel_tap(void)                          /* 調律: 平均律 <-> 純正律 (whichever was on) */
{
    if (proj.par[P_TUNING] == TN_EQUAL) {
        knob_set(P_TUNING, ui.tuning_prev ? ui.tuning_prev : TN_SUIKO);
    } else {
        ui.tuning_prev = (uint8_t)proj.par[P_TUNING];
        knob_set(P_TUNING, TN_EQUAL);
    }
    say("TUNING", TN_NAME[proj.par[P_TUNING]]);
}

static void button(int b)                          /* on release, unless the press was used */
{
    switch (b) {
    case B_HOME: set_view(V_HOME); break;
    case B_FX: set_view(V_SOUND); break;
    case B_EDIT: set_view(V_BEND); break;
    case B_SEL: sel_tap(); break;
#if ZN_HUD
    case B_PLAY:
        hud.peak = 0;
        plat_cpu_max_reset();
        say("CPU PEAK", "CLEARED");
        break;
#endif
    case B_LFO: toggle(P_VIB_ON, "VIBRATO"); break;
    case B_ARP: toggle(P_TRILL_ON, "TRILL"); break;
    case B_GLO:
        knob_set(P_MONO, !proj.par[P_MONO]);
        say(proj.par[P_MONO] ? "MONO" : "POLY", proj.par[P_MONO] ? "(TANON)" : "(WAON)");
        break;
    case B_SEQ: {
        int i;
        knob_set(P_BLACK, !proj.par[P_BLACK]);
        for (i = 0; i < ORN_N; i++)                /* nothing stays held across the switch */
            fm_ornament(i, 0);
        for (i = 0; i < FM_NBLACK; i++)
            fm_key(FM_NWHITE + i, 0);
        say("BLACK KEYS", proj.par[P_BLACK] ? "ORNAMENTS" : "UPPER ROW");
        break;
    }
    case B_SAVE:
        if (!project_save_quick())
            project_reclaim();                     /* (asked for: the pause either way; then room for more) */
        if (project_save() == 0) {
            ui.dirty = 0;
#if ZN_HUD
            hud.saves++;
#endif
            say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): notes play the voice, chromatic, with their velocity */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xF0u, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0x90u && v)
            fm_note_vel(FM_NOTE_ID + (int)n, 1, 100.0f * ((float)n - 69.0f), (int)v);
        else if (st == 0x80u || (st == 0x90u && !v))
            fm_note(FM_NOTE_ID + (int)n, 0, 0.0f);
        else if (st == 0xB0u && (n == 123u || n == 120u))
            fm_panic();
    }
}

static void black_key(int b, int on)
{
    if (proj.par[P_BLACK]) {
        if (BLACK_ORN[b] < ORN_N)
            fm_ornament(BLACK_ORN[b], on);
    } else {
        fm_key(FM_NWHITE + b, on);
    }
}

#define HOME_HOLD_MS 600u
static int home_held(void)                         /* HOME held long enough, once per press */
{
    if ((ui.btn >> B_HOME & 1u) && !(ui.btn_used >> B_HOME & 1u) && plat_ms() - ui.home_t > HOME_HOLD_MS) {
        ui.btn_used |= 1u << B_HOME;
        return 1;
    }
    return 0;
}
static void mode_set(int m);
static int save_hold(void);

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < FM_NWHITE; i++)                /* keys first: a glissando is the fastest thing here */
        if (ch >> WHITE_K[i] & 1u)
            fm_key((int)i, (int)(keys >> WHITE_K[i] & 1u));
    for (i = 0; i < FM_NBLACK; i++)
        if (ch >> BLACK_K[i] & 1u)
            black_key((int)i, (int)(keys >> BLACK_K[i] & 1u));
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            if (i == B_OCTDN || i == B_OCTUP)
                oct_press(i == B_OCTUP);
            else if (i == B_SEL)
                ui.sel_t = plat_ms();
            else if (i == B_HOME)
                ui.home_t = plat_ms();
            else if (i == B_SAVE)
                ui.save_t = plat_ms();
        } else if (i == B_OCTDN || i == B_OCTUP) {
            oct_release(i == B_OCTUP);
        } else if (!(ui.btn_used & m)) {
            button((int)i);
        }
    }
    if ((btn >> B_SEL & 1u) && !(ui.btn_used >> B_SEL & 1u) && plat_ms() - ui.sel_t > 400u) {
        ui.btn_used |= 1u << B_SEL;                /* SEL held: the tuning page, no toggle on release */
        set_view(V_TUNING);
    }
    if (home_held()) {
        mode_set(1);
        return;
    }
    if (save_hold())
        return;
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = sel_step(EN_SELECT)) != 0)            /* a detent is a step: no acceleration on these three */
        knob_set(P_HON, proj.par[P_HON] + e);
    if ((e = sel_step(EN_PRESET)) != 0)          /* PRESETS picks the instrument, as players expect */
        param_step(P_VOICE, e);
    if ((e = sel_step(EN_ALGO)) != 0)
        param_step(P_SCALE, e);
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16, g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16,
        bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

#if ZN_HUD
/* the load readout (dev builds), across the header:
 *   CPU now/peak   the audio render's share of its budget, smoothed over ~90 ms, and the peak of that
 *   MX             the slowest single render (5.8 ms of audio), unsmoothed: a short spike shows here
 *   X              xruns since boot     V voices sounding     S saves to flash     F screen frames a second
 * PLAY clears the peaks. */

static void hud_tick(void)
{
    uint32_t c = plat_cpu_pct(), now = plat_ms();
    if (c > hud.peak)
        hud.peak = c;
    hud.frames++;
    if (now - hud.t0 >= 1000u) {
        hud.fps = hud.frames * 1000u / (now - hud.t0);
        hud.frames = 0;
        hud.t0 = now;
    }
}

static int32_t hud_num(char *b, int32_t k, const char *tag, uint32_t v)
{
    while (*tag)
        b[k++] = *tag++;
    itoa_u(v, b + k);
    while (b[k])
        k++;
    b[k++] = ' ';
    b[k] = 0;
    return k;
}

static uint32_t hud_sig(void)
{
    return hash(hash(hash(hash(plat_cpu_pct(), hud.peak), plat_xruns()), (uint32_t)fm_nvoices | hud.fps << 8),
                plat_cpu_max() | hud.saves << 16);
}

static void hud_line(char *b)
{
    int32_t k = 0;
    k = hud_num(b, k, "CPU ", plat_cpu_pct());
    b[k - 1] = '/';
    k = hud_num(b, k, "", hud.peak);
    k = hud_num(b, k, "MX ", plat_cpu_max());
    k = hud_num(b, k, "X ", plat_xruns());
    k = hud_num(b, k, "V ", fm_nvoices);
    k = hud_num(b, k, "S ", hud.saves);
    hud_num(b, k, "F ", hud.fps);
}

static void hud_draw(void)
{
    char b[64];
    hud_line(b);
    cv_text(6, 8, &FONT_XS, b, plat_xruns() ? K_KOTO : K_TEXT);
}
#endif

static void draw_header(void)
{
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(2166136261u, ui.view), (uint32_t)proj.par[P_VIB_ON] | (uint32_t)proj.par[P_TRILL_ON] << 1 |
                                                   (uint32_t)proj.par[P_MONO] << 2 | (uint32_t)proj.par[P_BLACK] << 3 |
                                                   (uint32_t)ui.dirty << 4 | (uint32_t)msg << 5);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
#if ZN_HUD
    h = hash(h, hud_sig());
#endif
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_KOTO);
    } else {
        static const char *const TAG[4] = {"VIB", "TRILL", "MONO", "ORN"};
        int on[4] = {proj.par[P_VIB_ON], proj.par[P_TRILL_ON], proj.par[P_MONO], proj.par[P_BLACK]}, i;
        int32_t x = 10;
#if ZN_HUD
        hud_draw();                            /* in place of the title and the tags (the button lights show those) */
        on[0] = on[1] = on[2] = on[3] = 0;
#else
        cv_text(x, 6, &FONT_B, ui.view == V_HOME ? "ZEN-1" : VIEW_NAME[ui.view], K_TEXT);
#endif
        x = 232;
        for (i = 3; i >= 0; i--) {
            int32_t w = text_w(&FONT_XS, TAG[i]) + 10;
            if (!on[i])
                continue;
            x -= w + 4;
            cv_round(x, 6, w, 16, 6, K_KOTO_T);
            cv_text(x + 5, 7, &FONT_XS, TAG[i], K_KOTO);
        }
        if (ui.dirty)
            cv_round(x - 10, 12, 5, 5, 2, K_DIM);
    }
    cv_blit(0, 0);
}

static void draw_keys(int32_t y0, int32_t h)
{
    int k;
    for (k = 0; k < FM_NWHITE; k++) {
        float lv = fm_key_level[k];
        int t = (int)(lv * 16.0f + 0.5f), x = 6 + k * 14, tonic = (k % 5) == 0;
        uint16_t c = mix(tonic ? K_LINE : K_PANEL, K_KOTO, t > 16 ? 16 : t);
        cv_round(x, y0, 12, h, 4, c);
        if (tonic)
            cv_round(x + 4, y0 + h - 7, 4, 4, 2, t > 2 ? K_WHITE : K_TONIC);
    }
}

/* L holds only the letters it is drawn with (tools/gen_font.py): a name sent from outside may need another */
static int l_has(const char *s)
{
    for (; *s; s++)
        if ((uint8_t)*s < FONT_L.first || (uint8_t)*s > FONT_L.last || !FONT_L.bw[(uint8_t)*s - FONT_L.first])
            return 0;
    return 1;
}

static void draw_main(void)
{
    char t[16];
    uint32_t h = hash(2166136261u, ui.view), s;
    int i;
    for (s = 0; s < FM_NWHITE; s++)
        h = hash(h, (uint32_t)(fm_key_level[s] * 24.0f));
    h = hash(hash(h, (uint32_t)proj.par[P_HON] | (uint32_t)proj.par[P_VOICE] << 8 | (uint32_t)proj.par[P_TUNING] << 16 |
                          (uint32_t)proj.par[P_SCALE] << 20 | (uint32_t)(proj.par[P_OCTAVE] + 1) << 24),
             (uint32_t)(fm_bend_cents * 4.0f + 1000.0f) | (uint32_t)fm_patch_custom << 16);
    for (s = 0; s < 10u && fm_patch_label[s]; s++)   /* the whole name: the render adopts a patch a frame after the
                                                      * slot changes, and Sho and Shakuhachi start alike */
        h = hash(h, (uint32_t)fm_patch_label[s] | s << 8);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    {   /* 本数, big, with the note 三 sounds; the voice; the tuning and the scale */
        int san = 53 + proj.par[P_HON] + 12 * proj.par[P_OCTAVE];
        int32_t x = 12;
        x = cv_text(x, 2, &FONT_L, TN_HON_ASCII[proj.par[P_HON]], K_TEXT);
        cv_text(x + 4, 20, &FONT_XS, "HON", K_DIM);
        t[0] = 0;
        {
            const char *n = NOTE_NAME[((san % 12) + 12) % 12];
            int k = 0;
            while (*n)
                t[k++] = *n++;
            t[k++] = (char)('0' + (san / 12 - 1));
            t[k] = 0;
        }
        cv_text(x + 4, 4, &FONT_S, t, K_DIM);
        if (fm_bend_cents != 0.0f) {           /* the bend: a small dial beside the key number */
            int32_t cx = 110, bw = (int32_t)(fm_bend_cents * 0.16f);
            cv_round(cx - 20, 14, 40, 6, 3, K_LINE);
            cv_round(bw < 0 ? cx + bw : cx, 14, (bw < 0 ? -bw : bw) + 4, 6, 3, K_KOTO);
        }
        {   /* the tuning and the scale, top right */
            char line[40];
            int k = 0;
            const char *a = TN_NAME[proj.par[P_TUNING]], *b = SC_NAME[proj.par[P_SCALE]];
            while (*a)
                line[k++] = *a++;
            line[k++] = ' ';
            line[k++] = '/';
            line[k++] = ' ';
            while (*b)
                line[k++] = *b++;
            line[k] = 0;
            cv_text(240 - 12 - text_w(&FONT_XS, line), 6, &FONT_XS, line, K_DIM);
        }
    }
    {   /* the instrument, large: its name at the left in the biggest face that fits, its kanji at the right
         * (Suiko-style), both centered on the band y 38..90; a patch sent from outside has no kanji */
        const char *name = (const char *)fm_patch_label, *ja = fm_patch_custom ? "" : FM_PATCH_JA_LCD[proj.par[P_VOICE]];
        int32_t jw = *ja ? text_w(&FONT_JPL, ja) : 0, room = 240 - 24 - (jw ? jw + 10 : 0);
        const felucca_font_t *f = l_has(name) && text_w(&FONT_L, name) <= room ? &FONT_L
                                : text_w(&FONT_M, name) <= room ? &FONT_M : &FONT_B;
        cv_text(12, 64 - f->h / 2, f, name, fm_patch_custom ? K_KOTO : K_TEXT);
        if (jw)
            cv_text(240 - 12 - jw, 64 - FONT_JPL.h / 2, &FONT_JPL, ja, K_TEXT);
    }
    draw_keys(98, 22);
    for (i = 0; i < FM_NWHITE; i++)              /* the degree under each key, the tonics' row marked */
        text_c(12 + i * 14, 124, &FONT_XS, DEG_NAME[i % 5], (i % 5) ? K_DIM : K_TEXT);
    cv_blit(0, MAIN_Y);
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[VIEW_KNOB[ui.view][i]]);
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    cv_round(4, 2, 232, KNB_H - 6, 6, K_PANEL);
    for (i = 0; i < 4; i++) {
        int p = VIEW_KNOB[ui.view][i], lo, hi, v;
        const fm_param_t *pi;
        int32_t x = (int32_t)i * 60, cx = x + 30, bw;
        int hot = now_touch && ui.touched == (int)i;
        char t[16];
        if (p == K_NONE)
            continue;
        pi = fm_param_info(p);
        lo = pi->lo;
        hi = pi->hi;
        v = proj.par[p];
        if (i)
            cv_rect(x, 14, 1, KNB_H - 30, K_LINE);
        text_c(cx, 6, &FONT_XS, pi->name, hot ? K_TEXT : K_DIM);
        fm_param_text(p, v, t);
        {
            const char *q = t;
            int lower = 0;
            for (; *q; q++)
                lower |= *q >= 'a' && *q <= 'z';
            if (lower || text_w(&FONT_M, t) > 56)
                text_c(cx, 26, &FONT_B, t, hot ? K_KOTO : K_TEXT);
            else
                text_c(cx, 22, &FONT_M, t, hot ? K_KOTO : K_TEXT);
        }
        bw = hi > lo ? (int32_t)(44 * (v - lo) / (hi - lo)) : 0;
        cv_round(cx - 22, 50, 44, 6, 3, K_LINE);
        if (lo < 0) {
            int32_t m = cx - 22 + 44 * (0 - lo) / (hi - lo), e = cx - 22 + bw;
            cv_round(e < m ? e : m, 50, (e < m ? m - e : e - m) + 6, 6, 3, hot ? K_KOTO : K_TONIC);
        } else if (bw) {
            cv_round(cx - 22, 50, bw < 6 ? 6 : bw, 6, 3, hot ? K_KOTO : K_TONIC);
        }
    }
    cv_blit(0, KNB_Y);
}

static int amb_leds(void);

static void leds(void)
{
    uint32_t b = 0, k = 0, i;
    if (proj.mode && amb_leds())
        return;
    static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_FX, B_EDIT, B_SEL};
    b |= 1u << VIEW_BTN[ui.view];
    if (proj.par[P_VIB_ON])
        b |= 1u << B_LFO;
    if (proj.par[P_TRILL_ON])
        b |= 1u << B_ARP;
    if (proj.par[P_MONO])
        b |= 1u << B_GLO;
    if (proj.par[P_BLACK])
        b |= 1u << B_SEQ;
    if (proj.leds) {
        for (i = 0; i < FM_NWHITE; i++)
            if (fm_key_level[i] > 0.15f || (i % 5) == 0)   /* the tonics: landmarks, always lit */
                k |= 1u << WHITE_K[i];
        for (i = 0; i < FM_NBLACK; i++)
            if (fm_key_level[FM_NWHITE + i] > 0.15f)
                k |= 1u << BLACK_K[i];
    }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, when nothing sounds and nothing is touched (a flash erase
 * silences the audio for a moment) */
#define AUTOSAVE_QUIET 20000u                  /* (was 4 s; a burst of turns is one save, and the
                                                * journal lasts: on the FM-1, 65 saves in a session ran it out) */
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || fm_nvoices)
        return;
    if (!project_save_quick())
        project_reclaim();                         /* the journal is full: a sector of it erased now, in the quiet
                                                    * (nothing sounds, nothing touched) */
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
#if ZN_HUD
        hud.saves++;
#endif
    }
}

/* ------------------------------------------------------------ ambient --- */
/* Ambient mode: a scene fills the screen (render8.c) and plays itself. The keys and knobs are the scene's
 *: PRESETS picks the scene (the one the next power-on starts in: saved 3 s after; never the load test),
 * SELECT turns Activity and ALGORITHM the Mood from any page (the key and scale come with the mood), FX the next
 * page of four knobs and SEL the one before, GLO switches painterly / pixel art, PLAY grows a new picture, SEQ the
 * sleep timer, SAVE saves (held: reset), HOME held goes back to the instrument. Only the buttons with a use glow dim (amb_leds). OCT- + OCT+ held 5 s is update
 * mode, as everywhere (main_fm1.c). A scene's knobs are kept in the project (proj.knob), so they are as they were
 * left, after a reboot too.
 *
 * The picture (a scene's tree) is grown from a seed: a new one each time the scene starts, PLAY grows another
 * now (through a fade to dark), and the scene asks for one each night. (no keeping one: REC has
 * no use here.)
 *
 * Saving: a save goes to the journal (project.c: page programs, no pause) while it has room; only when it is full
 * does a save erase a sector, which stops everything (the audio and the key lights' scan, with the interrupts)
 * for up to a second, and a scene is never silent: then the breath below. On the FM-1 an autosave 6 s after each change made a pause every time
 * the player switched the Water knob back and forth. So in ambient mode a save comes at most once, 90 s
 * after the last change (or at once for SAVE and REC), and it is a breath: the sound fades out (0.7 s), the
 * flash is written in the silence, and the sound fades back in (0.9 s). The lights still blink off while it
 * writes: nothing can scan them then. */
#define AMB_FADE_MS 450u
#define AMB_AUTOSAVE_MS 90000u                  /* a save that will erase a sector (the journal is full) */
#define AMB_AUTOSAVE_QUICK_MS 20000u           /* a journal save (no pause): soon after the last change */
#define AMB_SCENE_SAVE_MS 3000u                /* a new scene: saved this long after the last turn (journal only) */
#define AMB_PRESET_MS 500u                     /* PRESETS: the scene shown is switched to this long after the last click */
#define AMB_PRESET_QUIET_MS 400u               /* and PRESETS rests this long after a scene has loaded */
#define AMB_SAVE_OUT_MS 700u
#define AMB_SAVE_IN_MS 900u
/* the sleep timer (SEQ): each tap the next time, from now: OFF, 15 m .. 8 h. Over its last ten minutes
 * the picture dims to 40 %; at its time the sound and the picture fade out over half a minute, then all is dark and
 * quiet until a button or a key wakes it (that press only wakes; the timer is then off). Not kept: OFF at power-on */
#define AMB_NSLEEP 7
static const uint16_t AMB_SLEEP_MIN[AMB_NSLEEP] = {0, 15, 30, 60, 120, 240, 480};
static const char *const AMB_SLEEP_NAME[AMB_NSLEEP] = {"OFF", "15 m", "30 m", "1 h", "2 h", "4 h", "8 h"};
#define AMB_SLEEP_FADE_MS 30000u
#define AMB_SLEEP_DIM_MS 600000u
static struct {
    const scene_t *sc;
    uint32_t last_ms, t_sec, frames, draw_us, blit_us, d10, b10;
#if ZN_HUD
    uint32_t step_us, s10, pt10[SC_NPT];       /* the step, and the scene's parts (SC_PT), ms a frame x10 */
    uint32_t sent_px, sent_pct;                /* the screen sent a frame (render8.c r8_sent), % */
#endif
    int8_t touched;
    uint32_t touch_until;
    uint32_t seed, regrows;
    int8_t fade_dir;                    /* -1 to dark (then a new picture), +1 back, 0 still */
    uint32_t save_at, act_t;            /* a save asked for (non-zero), the last input */
    uint8_t save_say, save_phase;       /* the breath: 1 fading out, 2 written, fading back in */
    uint8_t jw, jw_say;                 /* a journal save under way, a piece a frame; SAVED when done */
    uint32_t status_until;              /* HOME tapped: the status card shown till then */
    uint32_t welcome_until;             /* a first start (or a reset): the welcome card till then, or a touch */
    int8_t reset_m;                     /* the Mood knob on "Reset" this mood (or -1), since */
    uint32_t reset_at;
    uint32_t batt_t, batt_warn_t, batt_icon_until;   /* the battery looked at; the last warning; its icon shown till */
    uint8_t batt_low;                   /* 1 under ~15 %, 2 under ~5 % (warnings, once calibrated) */
    float save_lvl;                     /* 1 .. 0 .. 1 */
    uint8_t sleep, asleep;              /* the sleep timer (AMB_SLEEP_MIN[]); run out: all dark and quiet */
    uint32_t sleep_end;                 /* when its fade begins */
    float sleep_f;                      /* the sound's level for it, 1 .. 0 */
    uint8_t page;                       /* the knobs' page: 0 the scene's, 1 MIX, 2 MORE, 3 DRIFT */
    uint8_t scene_save;                 /* the scene was changed: saved soon (it is the one the next power-on starts) */
    uint8_t calib;                      /* the panel's calibration screen is up (testing builds: ARP) */
    int8_t pre_to;                      /* PRESETS: the scene chosen, not yet switched to (-1: none) */
    uint32_t pre_at, pre_quiet;         /* its last click; PRESETS ignored until (a scene just loaded) */
    int16_t kv[SC_NKNOB];               /* the scene's knobs (written back to the project after each turn) */
} amb;

static int amb_index(void) { return proj.scene % NSCENES; }
static int amb_slot(void) { return SCENES[amb_index()]->slot & 3; }   /* the scene's own place in the project */

/* a scene's panel lights (no flash on the keys): the buttons with a use dim (FX, SEL, SEQ, GLO, PLAY, SAVE, HOME, OCT-
 * and OCT+; SEQ bright while the sleep timer runs), the
 * rest dark; all the keys dim (every white key sounds right in the scene's scale; each scene gives the black keys a
 * use) */
static int amb_leds(void)
{
    uint32_t b = 0, k = 0, db = 0, dk = 0, i;
    if (!amb.sc)
        return 0;
    db = (1u << B_FX) | (1u << B_SEL) | (1u << B_SEQ) | (1u << B_GLO) | (amb.sc->regrow ? 1u << B_PLAY : 0u) | (1u << B_SAVE) | (1u << B_HOME) |
         (1u << B_OCTDN) | (1u << B_OCTUP) | (ZN_HUD ? 1u << B_REC : 0u);   /* (REC: the testing builds'
                                                       * readout) */
    if (proj.leds) {
        for (i = 0; i < FM_NWHITE; i++)
            dk |= 1u << WHITE_K[i];
        for (i = 0; i < FM_NBLACK; i++)
            dk |= 1u << BLACK_K[i];
    }
    if (amb.sleep)
        b |= 1u << B_SEQ;                          /* (the sleep timer running: bright) */
    plat_leds_dim(proj.leds == 1 ? db & ~b : 0u, dk & ~k);
    plat_leds(b, k);
    return 1;
}

static uint32_t amb_new_seed(void)
{
    uint32_t s = (plat_ms() ^ amb.seed ^ 0x6B6F6D6Fu) * 2654435761u + amb.regrows;
#ifndef OM_HOST
    s ^= plat_us() * 2246822519u;      /* booting straight into a scene, the FM-1 gets here at the same ms every time
                                        * (the same tree each power-on): its microseconds differ (the host stays
                                        * repeatable for the tests) */
#endif
    return s;
}

/* every mood keeps the player's tweaks (Custom gone): a scene's knobs are its mood's, as
 * it was made (scene_t.mood_kv) until a knob of it is turned, then as the player left them (proj.mk, a bit in
 * proj.tweak), across power-offs. A tweaked mood's name has a dot; the first tweak says so; on the Mood knob, a
 * tweaked mood is followed by "Reset" it: stay there 2 s and it is as made again */
static int amb_mood(void) { return amb.sc && amb.kv[0] >= 0 && amb.kv[0] < amb.sc->nmood ? amb.kv[0] : 0; }
static int amb_tweaked(int m) { return (proj.tweak[amb_slot()] >> m & 1u) != 0; }
static void amb_kv_store(void)                     /* the scene's mood, and its tweaks if any, into the project */
{
    int i = amb_slot(), m = amb_mood(), j;
    proj.mood[i] = (uint8_t)m;
    if (amb_tweaked(m))
        for (j = 0; j < 16; j++)
            proj.mk[i][m][j] = (uint8_t)(amb.kv[j] < 0 ? 0 : amb.kv[j] > 255 ? 255 : amb.kv[j]);
}
static void amb_mood_kv(int m, int16_t *kv)        /* mood m's knobs: the player's, or as made */
{
    int i = amb_slot(), j;
    amb.sc->mood_kv(m, kv);                        /* (all 20: the unused ones too) */
    if (amb_tweaked(m))
        for (j = 1; j < 16; j++)
            kv[j] = (int16_t)proj.mk[i][m][j];
    kv[0] = (int16_t)m;
}
static void amb_mood_go(int m)                     /* to mood m: its knobs, every one to the sound */
{
    int16_t kv[SC_NKNOB];
    int j;
    amb_mood_kv(m, kv);
    for (j = 0; j < SC_NKNOB; j++)
        if (j != 3)                                /* (Space: the reverb, the instrument's own: not a mood's) */
            amb.kv[j] = kv[j];
    amb.sc->knob(0, 0);
    amb_kv_store();
    mark_dirty();
}
static void amb_mood_turn(int32_t e)               /* the Mood knob: each mood, a tweaked one followed by its Reset */
{
    int8_t mm[2 * SC_NMOOD_MAX];
    uint8_t rr[2 * SC_NMOOD_MAX];
    int n = 0, m, cur = 0, to;
    for (m = 0; m < amb.sc->nmood; m++) {          /* (numbered as the scene's day or night meets them) */
        mm[n] = (int8_t)m;
        rr[n++] = 0;
        if (amb_tweaked(m)) {
            mm[n] = (int8_t)m;
            rr[n++] = 1;
        }
    }
    for (m = 0; m < n; m++)
        if (mm[m] == amb_mood() && rr[m] == (amb.reset_m >= 0))
            cur = m;
    to = (int)(((cur + e) % n + n) % n);
    if (mm[to] != amb_mood())
        amb_mood_go(mm[to]);
    amb.reset_m = rr[to] ? mm[to] : -1;
    amb.reset_at = plat_ms();
}
static void amb_reset_tick(uint32_t now)           /* stayed on Reset 2 s: the mood as it was made */
{
    int m = amb.reset_m;
    if (m < 0 || now - amb.reset_at < 2000u)
        return;
    amb.reset_m = -1;
    proj.tweak[amb_slot()] &= (uint8_t)~(1u << m);
    amb_mood_go(m);
    say(amb.sc->mood_name(m), "as made");
    amb.page = 0;
    amb.touched = 0;
    amb.touch_until = now + 3000u;
}
static void amb_knob_text(int i, char *n, char *v) /* a knob's name and value, the Mood's Reset in its place */
{
    amb.sc->knob_text(i, n, v);
    if (i == 0 && amb.reset_m >= 0) {
        copy_str(n, "Reset", 11);
        copy_str(v, amb.sc->mood_name(amb.reset_m), 11);
    }
}

#define AMB_SCENE_GAIN 356                      /* the scenes +11 dB over the instrument (as loud at
                                                * 75 % MASTER as they were at full: the MASTER knob is squared, 0.75^2
                                                * = -5 dB; it was +6 dB) */
static void amb_enter(void)
{
    fm_scene_gain(AMB_SCENE_GAIN);
    r8_dark(0);                                    /* (a scene that wants a dark curve asks in its enter) */
    SC_PT_TAGS("");                                /* (its parts: the scene names them in its draw) */
    amb.sc = SCENES[amb_index()];
    r8_set_style(proj.style);
    r8_fade = r8_sleep = 256;
    amb.fade_dir = 0;
    amb.sleep = amb.asleep = 0;
    amb.sleep_f = 1.0f;
    amb.seed = amb_new_seed();
    amb.page = 0;
    amb.pre_to = -1;                               /* (no scene waiting to be switched to) */
    {
        int i = amb_slot();
        amb.reset_m = -1;
        amb_mood_kv(proj.mood[i] < amb.sc->nmood ? proj.mood[i] : 0, amb.kv);   /* (its mood: the player's, or as made) */
        proj.sound[i] = amb.sc->sound;
        amb_kv_store();
        amb.sc->enter(amb.seed, amb.kv);
    }
    amb.sc->background();
    amb.sc->step(0);                               /* the palette, before the first frame */
    amb.last_ms = amb.t_sec = amb.act_t = plat_ms();
    amb.frames = amb.draw_us = amb.blit_us = 0;
    amb.touched = -1;
    amb.touch_until = 0;
}

static void amb_leave(void)
{
    fm_scene_gain(100);
    if (amb.sc)
        amb.sc->leave();
    amb.sc = 0;
    while (amb.jw && project_save_step() > 0)      /* (a journal save under way: finished now) */
        ;
    amb.jw = 0;
    if (amb.save_at || amb.sleep_f < 1.0f) {       /* a save was on its way: the instrument's autosave takes it */
        fm_set(P_LEVEL, proj.par[P_LEVEL]);        /* (and the sleep timer's fade let go) */
        amb.save_at = 0;
        amb.save_phase = 0;
    }
    amb.sleep = amb.asleep = 0;
    amb.sleep_f = 1.0f;
    r8_fade = r8_sleep = 256;
    amb.fade_dir = 0;
    lcd_sync();
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;         /* the instrument's screens redraw from scratch */
    lcd_fill(0, 0, 240, 240, K_BG);
}

static void mode_set(int m)
{
    if (m == proj.mode)
        return;
    fm_panic();                                    /* nothing of one mode rings on into the other */
    if (proj.mode)
        amb_leave();
    proj.mode = (uint8_t)(m != 0);
    mark_dirty();
    if (proj.mode) {
        amb_enter();
        say("SCENE", amb.sc->name);
    } else {
        say("KOTO", "hold HOME: back");
    }
    ui.msg_until = plat_ms() + 3000u;
}

static void amb_quiet_save(int say_it)
{
    if (amb.save_at) {
        amb.save_say |= (uint8_t)say_it;
        return;
    }
    if (amb.jw) {                                  /* (one under way: it says SAVED, and saves again after) */
        amb.jw_say |= (uint8_t)say_it;
        return;
    }
    if (project_save_quick() && project_save_begin() == 0) {   /* the journal: a piece a frame (the lights
                                                    * scan between), no pause: no breath */
        ui.dirty = 0;                              /* (a change while it writes: dirty again, saved after) */
        amb.jw = 1;
        amb.jw_say = (uint8_t)say_it;
        return;
    }
    amb.save_at = plat_ms() | 1u;
    amb.save_phase = 1;
    amb.save_lvl = 1.0f;
    amb.save_say = (uint8_t)say_it;
}

/* the level for a point x (0..1) of the breath, even in decibels: -30 dB .. 0 (the level knob is squared) */
static int amb_save_level(float x)
{
    return (int)((float)proj.par[P_LEVEL] * fm_exp2f((x - 1.0f) * 2.4914f) * amb.sleep_f + 0.5f);
}

static void amb_save_tick(uint32_t now, uint32_t dt)
{
    if (amb.jw) {                                  /* a journal save: its next piece */
        int r = project_save_step();
        if (r > 0)
            return;
        amb.jw = 0;
        if (r == 0) {
#if ZN_HUD
            hud.saves++;
#endif
            if (amb.jw_say)
                say("SAVED", 0);
        } else {
            mark_dirty();                          /* (failed: the next save, as for a full journal) */
            say("NOT SAVED", "FLASH ERROR");
        }
        return;
    }
    if (amb.save_phase == 1) {                     /* fading out */
        amb.save_lvl -= (float)dt / (float)AMB_SAVE_OUT_MS;
        if (amb.save_lvl > 0.0f) {
            fm_set(P_LEVEL, amb_save_level(amb.save_lvl));
            return;
        }
        amb.save_lvl = 0.0f;
        fm_set(P_LEVEL, 0);
        amb.save_phase = 3;                        /* one more frame for the level to land at nothing */
        return;
    }
    if (amb.save_phase == 3) {                     /* silence: write (a full journal: a sector of it erased first) */
        int ok;
        if (!project_save_quick())
            project_reclaim();
        ok = project_save() == 0;
        amb.save_phase = 2;
        if (ok) {
            ui.dirty = 0;
#if ZN_HUD
            hud.saves++;
#endif
            if (amb.save_say)
                say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
        }
        return;
    }
    if (amb.save_phase == 2) {                     /* fading back in */
        amb.save_lvl += (float)dt / (float)AMB_SAVE_IN_MS;
        if (amb.save_lvl < 1.0f) {
            fm_set(P_LEVEL, amb_save_level(amb.save_lvl));
            return;
        }
        fm_set(P_LEVEL, amb_save_level(1.0f));
        amb.save_phase = 0;
        amb.save_at = 0;
    }
    if (!ui.dirty)
        amb.scene_save = 0;
    if (ui.dirty && !amb.save_at && now - amb.act_t > (project_save_quick() ? AMB_AUTOSAVE_QUICK_MS : AMB_AUTOSAVE_MS))
        amb_quiet_save(0);
    else if (amb.scene_save && !amb.save_at && project_save_quick() && now - amb.act_t > AMB_SCENE_SAVE_MS)
        amb_quiet_save(0);                         /* the scene, soon: a power-off now still starts in it (no pause:
                                                    * the journal only; else the save above, as ever) */
}

/* the sleep timer's dimming and fade (AMB_SLEEP_*); run out: dark and quiet (amb.asleep) */
static void amb_sleep_tick(uint32_t now)
{
    int32_t left;
    float d = 1.0f, f = 1.0f;
    if (!amb.sleep)
        return;
    left = (int32_t)(amb.sleep_end - now);
    if (left < (int32_t)AMB_SLEEP_DIM_MS)
        d = 0.4f + 0.6f * (left > 0 ? (float)left / (float)AMB_SLEEP_DIM_MS : 0.0f);
    if (left < 0) {
        f = 1.0f - (float)-left / (float)AMB_SLEEP_FADE_MS;
        if (f <= 0.0f) {
            f = 0.0f;
            amb.sleep = 0;
            amb.asleep = 1;                        /* (1: the dark frame still to send; 2: sent, nothing drawn) */
        }
    }
    r8_sleep = (int32_t)(256.0f * d * f);
    amb.sleep_f = f * f;
    if (left < 0 && !amb.save_phase)
        fm_set(P_LEVEL, amb_save_level(1.0f));
}

/* asleep, a button or a key wakes it (only wakes: the press does nothing else) */
static void amb_wake(void)
{
    amb.asleep = 0;
    amb.sleep_f = 1.0f;
    r8_sleep = 256;
    if (!amb.save_phase)
        fm_set(P_LEVEL, amb_save_level(1.0f));
    amb.touch_until = 0;
    amb.status_until = plat_ms() + 3000u;          /* (the status card: what is playing) */
}

static void amb_regrow(int asked)                  /* asked: PLAY (says why not); else the scene's night */
{
    if (!amb.sc || !amb.sc->regrow || amb.fade_dir)
        return;
    (void)asked;
    amb.fade_dir = -1;
}

static void amb_fade_tick(uint32_t dt)
{
    if (!amb.fade_dir)
        return;
    r8_fade += amb.fade_dir * (int32_t)(dt * 256u / AMB_FADE_MS) + amb.fade_dir;
    if (r8_fade <= 0) {                            /* dark: the new picture, then back up */
        r8_fade = 0;
        amb.seed = amb_new_seed();
        amb.regrows++;
        amb.sc->regrow(amb.seed);
        amb.fade_dir = 1;
    } else if (r8_fade >= 256) {
        r8_fade = 256;
        amb.fade_dir = 0;
    }
}

/* the knobs where the pages show them (page * 4 + column -> the scene's knob): Scene (Mood, Activity, Instrument,
 * Space), Volume (Notes, Drone, Air, Water), Nature (the scene's own two, then Air Type and Water Type, each under
 * its level), Music (Echo, Warmth, Phrases, Tide). The knobs keep their numbers (the project and the moods hold
 * them by number): only where they show is set here */
static const uint8_t AMB_AT[SC_NUSED] = {0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 8, 9, 10, 11, 14, 15};
static int amb_knob_at(int p)
{
    int n = AMB_AT[p];
    return amb.sc->own_swap && (n == 12 || n == 13) ? n ^ 1 : n;
}
static int amb_knob_pos(int n)
{
    int p;
    for (p = 0; p < SC_NUSED; p++)
        if (amb_knob_at(p) == n)
            return p;
    return 0;
}

static void amb_page(int p)                         /* a page of knobs, its bar shown at once */
{
    amb.page = (uint8_t)((p + SC_NPAGE) % SC_NPAGE);
    amb.touched = -1;
    amb.touch_until = plat_ms() + 3000u;
}

/* a scene's knob n turned e (enc: its encoder, for the speed-up; < 0 none): its page shown, it lit for 3 s */
static void amb_turn(int n, int enc, int32_t e)
{
    if (n == 0)
        amb_mood_turn(e);                          /* (the Mood: one a detent, round) */
    else {
        int16_t was[SC_NUSED];
        int j, moved = 0, m = amb_mood();
        for (j = 0; j < SC_NUSED; j++)
            was[j] = amb.kv[j];
        sc_knob_mul = enc >= 0 ? accel(enc, e, 100) / e : 1;   /* (a list turns one a detent whatever: sc_knob_wrap) */
        amb.sc->knob(n, e);
        sc_knob_mul = 1;
        for (j = 1; j < SC_NUSED; j++)
            moved |= j != 3 && amb.kv[j] != was[j];
        if (moved) {                               /* a tweak: the mood the player's now (a word the first time) */
            amb.reset_m = -1;
            if (!amb_tweaked(m)) {
                proj.tweak[amb_slot()] |= (uint8_t)(1u << m);
                say(amb.sc->mood_name(m), "your own now");
                ui.msg_until = plat_ms() + 2000u;
            }
        }
    }
    amb_kv_store();
    mark_dirty();                                  /* kept in the project: saved a few seconds after */
    amb.page = (uint8_t)(amb_knob_pos(n) / 4);
    amb.touched = (int8_t)(amb_knob_pos(n) % 4);
    amb.touch_until = plat_ms() + 3000u;
    amb.act_t = plat_ms();
    amb.welcome_until = 0;
}

static void amb_button(int b)                      /* on release, unless the press was used */
{
    switch (b) {
    case B_FX:                                     /* the pages: FX the next, round; with the bar not shown, FX
                                                    * shows it at the first page, the scene's. Tabs over the
                                                    * knobs' bar show them all */
        amb_page(plat_ms() < amb.touch_until ? amb.page + 1 : 0);
        break;
    case B_SEL:                                    /* the page before (with the bar not shown: the first, as FX) */
        amb_page(plat_ms() < amb.touch_until ? amb.page - 1 : 0);
        break;
    case B_SEQ:                                    /* the sleep timer: the next time, from now (8 h: OFF) */
        amb.sleep = (uint8_t)((amb.sleep + 1) % AMB_NSLEEP);
        amb.sleep_end = plat_ms() + (uint32_t)AMB_SLEEP_MIN[amb.sleep] * 60000u;
        say("SLEEP", AMB_SLEEP_NAME[amb.sleep]);
        break;
    case B_PLAY: amb_regrow(1); break;
#if ZN_HUD
    case B_REC:                                    /* testing builds: the load readout over the scene, or not */
        hud.show = (uint8_t)!hud.show;
        break;
#endif
    case B_GLO:                                    /* painterly / pixel art (was ENV) */
        proj.style = (uint8_t)!proj.style;
        mark_dirty();
        r8_set_style(proj.style);
        amb.sc->background();
        say("STYLE", proj.style ? "PIXEL ART" : "PAINTERLY");
        break;
    case B_SAVE: amb_quiet_save(1); break;
    case B_HOME:                                   /* a tap (held: the instrument): the status card for 3 s */
        amb.status_until = plat_ms() + 3000u;
        amb.touch_until = 0;
        break;
    default: break;
    }
}

#if ZN_HUD
/* the testing builds' tools, SAVE held with a button (their buttons are the pages now): EDIT the load
 * readout (REC alone too), ARP the calibration screen, GLO the dark curve to try, SEQ the CPU peak cleared */
static void amb_tool(int b)
{
    switch (b) {
    case B_GLO: {                                  /* the panel's dark curve to try: 0, 40 .. 160, the scene's own */
        static const int16_t TRY[6] = {0, 40, 80, 120, 160, -1};
        int i;
        char t[12];
        for (i = 0; i < 6 && TRY[i] != r8_dark_try; i++)
            ;
        r8_dark_try = TRY[(i + 1) % 6];
        r8_dark(r8_dark_scene);
        if (r8_dark_try < 0) {
            itoa_u((uint32_t)r8_dark_scene, t);
            say("DARK: SCENE'S", t);
        } else {
            itoa_u((uint32_t)r8_dark_try, t);
            say("DARK", t);
        }
        break;
    }
    case B_ARP:                                    /* the calibration screen: steps of gray and of the scenes' dark
                                                    * colors, to photograph (it sets r8_dark for the FM-1's panel) */
        amb.calib = (uint8_t)!amb.calib;
        say("CALIBRATION", amb.calib ? "PHOTO THIS" : "OFF");
        break;
    case B_EDIT:                                   /* the load readout over the scene, or not */
        hud.show = (uint8_t)!hud.show;             /* (every frame is drawn whole: nothing to clear) */
        break;
    case B_SEQ:
        hud.peak = 0;
        plat_cpu_max_reset();
        say("CPU PEAK", "CLEARED");
        break;
    default: break;
    }
}
#endif

static void amb_input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    if (amb.asleep && ((btn & ~ui.btn) || (keys & ~ui.keys))) {
        ui.btn_used |= btn;                        /* (no release action, no HOME hold) */
        ui.save_t = plat_ms();
        ui.btn = btn;
        ui.keys = keys;
        amb_wake();
        return;
    }
    if ((keys & ~ui.keys) || (btn & ~ui.btn))
        amb.welcome_until = 0;                     /* (a new press puts the welcome card away; not SAVE still held) */
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u)
            amb.sc->key((int)i, (int)(keys >> i & 1u));
    if (ch || btn != ui.btn || btn)
        amb.act_t = plat_ms();
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            if (i == B_OCTDN || i == B_OCTUP)
                oct_press(i == B_OCTUP);           /* the octave of the scene's notes; both held: reserved */
            else if (i == B_HOME)
                ui.home_t = plat_ms();
            else if (i == B_SAVE)
                ui.save_t = plat_ms();
        } else if (i == B_OCTDN || i == B_OCTUP) {
            oct_release(i == B_OCTUP);
        } else if (!(ui.btn_used & m)) {
#if ZN_HUD
            if ((btn >> B_SAVE & 1u) && i != B_SAVE) {   /* SAVE held: a testing tool, not the button's own */
                ui.btn_used |= 1u << B_SAVE;           /* (no save on SAVE's release, no reset count) */
                ui.save_t = plat_ms();
                amb_tool((int)i);
                continue;
            }
#endif
            amb_button((int)i);
        }
    }
    if (home_held()) {
        mode_set(0);
        return;
    }
    if (save_hold())
        return;
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0)            /* (SELECT Activity, ALGORITHM the */
        amb_turn(1, EN_SELECT, e);                 /* Mood, from any page; the key and scale come with the mood) */
    if ((e = sel_step(EN_ALGO)) != 0)
        amb_turn(0, -1, e);
    /* PRESETS: a click only shows the next scene's
     * name, the one playing goes on; half a second after the last click it switches to the one shown. Clicks that
     * came in while a scene was loading (it takes a moment), and for a little after, are let go: one click is one
     * scene */
    if ((e = sel_step(EN_PRESET)) != 0 && NSCENES > 1 && (int32_t)(plat_ms() - amb.pre_quiet) >= 0) {
        int from = amb.pre_to >= 0 ? amb.pre_to : amb_index();
        amb.pre_to = (int8_t)((from + (e > 0 ? 1 : NSCENES - 1)) % NSCENES);
        amb.pre_at = plat_ms();
        amb.welcome_until = 0;
        say("SCENE", SCENES[amb.pre_to]->name);
    }
    if (amb.pre_to >= 0 && plat_ms() - amb.pre_at >= AMB_PRESET_MS) {
        if (amb.pre_to != amb_index()) {
            amb.sc->leave();
            proj.scene = (uint8_t)amb.pre_to;
            mark_dirty();
            amb.scene_save = 1;
            amb_enter();
            say("SCENE", amb.sc->name);
            (void)plat_enc(EN_PRESET);             /* (what came in while it loaded: let go) */
            amb.pre_quiet = plat_ms() + AMB_PRESET_QUIET_MS;
        }
        amb.pre_to = -1;
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0)
            amb_turn(amb_knob_at(amb.page * 4 + (int)i), EN_K1 + (int)i, e);
}

static int amb_hits(int32_t sy, int32_t sh, int32_t y, int32_t h) { return y < sy + sh && sy < y + h; }

/* the battery (shown when asked, unobtrusive; warned when low). Its reading (plat_battery_raw) to a percentage
 * along a Li-ion cell's curve (its voltage falls slowly through the middle, quickly at either end), the readings
 * scaled from ZN_BATT_FULL, the reading when the charger shows full (green): 626 on the FM-1 (it showed 79 % with the
 * guess before, 527 .. 652). The empty end is still a guess (3.4 V on the same scale): the low warnings stay off
 * (ZN_BATT_CAL 0) until it is checked */
#define ZN_BATT_FULL 626
#define ZN_BATT_CAL 0
static int amb_batt_pct(void)
{
    static const uint16_t MV[11] = {4200, 4100, 4000, 3900, 3800, 3750, 3700, 3650, 3600, 3500, 3400};
    static const uint8_t PCT[11] = {100, 90, 78, 65, 50, 40, 28, 18, 10, 4, 0};
    int32_t raw = plat_battery_raw(), mv, i;
    if (raw < 0)
        return -1;
    mv = raw * 4200 / ZN_BATT_FULL;                /* (as millivolts of the cell) */
    if (mv >= MV[0])
        return 100;
    for (i = 1; i < 11; i++)
        if (mv >= MV[i])
            return PCT[i] + (int)((mv - MV[i]) * (PCT[i - 1] - PCT[i]) / (MV[i - 1] - MV[i]));
    return 0;
}
static void amb_batt_icon(int32_t x, int32_t y, int pct, uint16_t c)   /* 30 x 16, its nub to the right */
{
    int32_t f = pct < 0 ? 0 : (24 * pct + 50) / 100;
    cv_rect(x, y, 28, 2, c);
    cv_rect(x, y + 14, 28, 2, c);
    cv_rect(x, y, 2, 16, c);
    cv_rect(x + 26, y, 2, 16, c);
    cv_rect(x + 28, y + 5, 3, 6, c);
    if (f > 0)
        cv_rect(x + 3, y + 3, f > 22 ? 22 : f, 10, pct <= 15 ? K_KOTO : c);
}
/* low (once calibrated): under ~15 % a small battery at the bottom right for 5 s every 5 minutes; under ~5 % BATTERY
 * LOW for 4 s each minute, and the settings saved at once */
static void amb_batt_tick(uint32_t now)
{
    int pct, low;
    if (!ZN_BATT_CAL || now - amb.batt_t < 1000u)
        return;
    amb.batt_t = now;
    pct = amb_batt_pct();
    if (pct < 0)
        return;
    low = pct <= 5 ? 2 : pct <= 15 ? 1 : 0;
    if (amb.batt_low && pct >= 18)                 /* (charged again: hysteresis) */
        low = 0;
    else if (low < amb.batt_low && pct <= 7)
        low = amb.batt_low;
    if (low == 2 && amb.batt_low < 2 && ui.dirty)
        amb_quiet_save(0);                         /* (nothing lost if it goes off) */
    if (low != amb.batt_low)
        amb.batt_warn_t = now - 600000u;            /* (warned at once) */
    amb.batt_low = (uint8_t)low;
    if (low == 2 && now - amb.batt_warn_t >= 60000u) {
        amb.batt_warn_t = now;
        say("BATTERY LOW", 0);
        ui.msg_until = now + 4000u;
    } else if (low == 1 && now - amb.batt_warn_t >= 300000u) {
        amb.batt_warn_t = now;
        amb.batt_icon_until = now + 5000u;
    }
}

#if ZN_HUD
/* the calibration screen: sixteen grays, mostly dark (the panel lifts the darks), and eight of the scenes'
 * dark colors (Hotaru's night, Koi's deep water), each a square with its value under it; the raw panel (no dark
 * curve, no tint). The scene goes on underneath (its sound too), and redraws its palette the next frame. */
static const uint8_t CAL_GREY[16] = {0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 80, 96, 128, 176, 255};
static const uint32_t CAL_COL[8] = {0x0B120Eu, 0x1C2C22u, 0x0E1620u, 0x18263Au, 0x23305Eu, 0x0F3A4Au, 0x2E4220u, 0x5C7A3Cu};
static void amb_calib_draw(void)
{
    surf_t *s = &r8_fb;
    float k = (float)s->w / 240.0f;
    int i;
    r8_rect(s, 0, 0, s->w, s->h, 200);
    for (i = 0; i < 16; i++) {
        r8_pal_set(200 + 1 + i, (uint32_t)CAL_GREY[i] * 65793u);   /* (0x010101: a gray) */
        r8_rect(s, (int32_t)((float)(4 + (i % 4) * 59) * k), (int32_t)((float)(4 + (i / 4) * 44) * k), (int32_t)(54.0f * k),
                (int32_t)(30.0f * k), (uint8_t)(200 + 1 + i));
    }
    for (i = 0; i < 8; i++) {
        r8_pal_set(220 + i, CAL_COL[i]);
        r8_rect(s, (int32_t)((float)(4 + (i % 4) * 59) * k), (int32_t)((float)(180 + (i / 4) * 28) * k), (int32_t)(54.0f * k),
                (int32_t)(18.0f * k), (uint8_t)(220 + i));
    }
    r8_pal_set(200, 0x7F7F7Fu);
    {
        uint8_t d = r8_dark_pct;
        r8_dark_pct = 0;                               /* the panel itself */
        r8_pal_commit(0xFFFFFFu, 0, 0);
        r8_dark_pct = d;
    }
}
#endif

/* the UI over the scene, one strip at a time (render8.c r8_blit): the readout, a message, the knobs */
/* a knob's name in the bar: the small face (every name fits its 56 px, the host's textfit) */
static const felucca_font_t *amb_name_font(const char *n)
{
    (void)n;
    return &FONT_XS;
}

/* whether the UI is drawn over rows [sy, sy + sh) now (render8.c r8_blit: such a strip is converted whole) */
static int amb_over(int32_t sy, int32_t sh)
{
#if ZN_HUD
    if ((hud.show && amb_hits(sy, sh, 0, 49)) || amb.calib)
        return 1;
#endif
    return (plat_ms() < ui.msg_until && amb_hits(sy, sh, 40, 28)) ||
           ((plat_ms() < amb.touch_until || plat_ms() < amb.status_until) && amb_hits(sy, sh, 160, 76)) ||
           (plat_ms() < amb.welcome_until && amb_hits(sy, sh, 128, 108)) ||
           (plat_ms() < amb.batt_icon_until && amb_hits(sy, sh, 216, 20));
}

static void amb_overlay(int32_t sy, int32_t sh)
{
#if ZN_HUD
    if (hud.show && amb_hits(sy, sh, 0, 49)) {
        char b[64];
        int32_t k = 0;
        cv_rect(0, 0, 240, 49, K_BG);
        hud_line(b);
        cv_text(6, 2, &FONT_XS, b, plat_xruns() ? K_KOTO : K_TEXT);
        copy_str(b, amb.sc->name, 12);
        while (b[k])
            k++;
        b[k++] = ' ';
        k = hud_num(b, k, "D ", amb.d10 / 10u);    /* draw and blit, ms a frame (x.y) */
        b[k - 1] = '.';
        k = hud_num(b, k, "", amb.d10 % 10u);
        k = hud_num(b, k, "B ", amb.b10 / 10u);
        b[k - 1] = '.';
        k = hud_num(b, k, "", amb.b10 % 10u);
        k = hud_num(b, k, "U ", amb.sent_pct);     /* the share of the screen sent a frame, % */
        {   /* the save journal's longest page program so far, ms (x.y): it holds the interrupts */
            uint32_t w = (plat_jr_prog_us_max() + 50u) / 100u;
            k = hud_num(b, k, "W ", w / 10u);
            b[k - 1] = '.';
            k = hud_num(b, k, "", w % 10u);
        }
        hud_num(b, k, proj.style ? "PX" : "PT", 0);
        b[k + 2] = 0;
        cv_text(6, 17, &FONT_XS, b, K_TEXT);
        {   /* the step, then the scene's parts (SC_PT): where the time goes, ms a frame (x.y) */
            char tg[3] = {0, ' ', 0};
            int i;
            k = hud_num(b, 0, "s ", amb.s10 / 10u);
            b[k - 1] = '.';
            k = hud_num(b, k, "", amb.s10 % 10u);
            for (i = 0; i < SC_NPT && sc_pt_tag[i]; i++) {
                tg[0] = sc_pt_tag[i];
                k = hud_num(b, k, tg, amb.pt10[i] / 10u);
                b[k - 1] = '.';
                k = hud_num(b, k, "", amb.pt10[i] % 10u);
            }
            b[k] = 0;
            cv_text(6, 32, &FONT_XS, b, K_TEXT);
        }
    }
#endif
#if ZN_HUD
    if (amb.calib) {                               /* the values under the squares */
        int i;
        for (i = 0; i < 16; i++)
            if (amb_hits(sy, sh, 35 + (i / 4) * 44, 10)) {
                char t[8];
                itoa_u(CAL_GREY[i], t);
                cv_text(6 + (i % 4) * 59, 35 + (i / 4) * 44, &FONT_XS, t, K_TEXT);
            }
        for (i = 0; i < 8; i++)
            if (amb_hits(sy, sh, 199 + (i / 4) * 28, 10)) {
                char t[4] = {'C', (char)('1' + i), 0, 0};
                cv_text(6 + (i % 4) * 59, 199 + (i / 4) * 28, &FONT_XS, t, K_TEXT);
            }
    }
#endif
    if (plat_ms() < ui.msg_until && amb_hits(sy, sh, 40, 28)) {
        int32_t w = text_w(&FONT_B, ui.msg[0]) + (ui.msg[1][0] ? 8 + text_w(&FONT_B, ui.msg[1]) : 0), x = (240 - w) / 2;
        cv_round(x - 12, 40, w + 24, 26, 6, K_BG);
        x = cv_text(x, 45, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 45, &FONT_B, ui.msg[1], K_KOTO);
    }
    if (plat_ms() < amb.batt_icon_until && plat_ms() >= amb.status_until && plat_ms() >= amb.touch_until &&
        amb_hits(sy, sh, 216, 20)) {               /* low (~15 %): a small battery, bottom right, a few seconds */
        cv_round(190, 216, 44, 20, 5, K_PANEL);
        amb_batt_icon(196, 218, amb_batt_pct(), K_TEXT);
    }
    if (plat_ms() < amb.welcome_until && amb_hits(sy, sh, 128, 108)) {   /* the welcome card: the four to start with */
        static const char *const W[4][2] = {{"PRESETS", "scene"}, {"ALGORITHM", "mood"}, {"PLAY", "new picture"},
                                            {"FX", "the knobs"}};
        int i;
        cv_round(4, 128, 232, 108, 6, K_PANEL);
        text_c(120, 134, &FONT_B, "Welcome to ZEN-1", K_TEXT);
        for (i = 0; i < 4; i++) {
            cv_text(116 - text_w(&FONT_S, W[i][0]), 158 + 19 * i, &FONT_S, W[i][0], K_TEXT);
            cv_text(126, 158 + 19 * i, &FONT_S, W[i][1], K_KOTO);
        }
    } else if (plat_ms() < amb.status_until && amb_hits(sy, sh, 160, 76)) {   /* the status card */
        int pct = amb_batt_pct();
        char t[24], n[12], v[12];
        cv_round(4, 178, 232, 58, 6, K_PANEL);
        amb_batt_icon(14, 190, pct, K_TEXT);
        if (pct >= 0) {
            int e = 0;
            itoa_u((uint32_t)pct, t);
            while (t[e])
                e++;
            copy_str(t + e, "%", 2);
        } else
            copy_str(t, "--", 3);
        cv_text(52, 187, &FONT_B, t, pct >= 0 && pct <= 15 ? K_KOTO : K_TEXT);
        if (amb.sleep) {                           /* the sleep timer's time left: h:mm, under an hour m:ss */
            int32_t ms = (int32_t)(amb.sleep_end - plat_ms());
            uint32_t s = ms > 0 ? (uint32_t)ms / 1000u : 0u, a = s >= 3600u ? s / 3600u : s / 60u,
                     b = s >= 3600u ? s / 60u % 60u : s % 60u;
            int e = 6;
            copy_str(t, "sleep ", 7);
            itoa_u(a, t + e);
            while (t[e])
                e++;
            t[e++] = ':';
            t[e++] = (char)('0' + b / 10u);
            t[e++] = (char)('0' + b % 10u);
            t[e] = 0;
            cv_text(14, 214, &FONT_XS, t, K_DIM);
        }
#if ZN_HUD
        else {   /* (testing builds: the reading itself, to calibrate the scale) */
            char r[16];
            int32_t raw = plat_battery_raw();
            copy_str(r, "raw ", 5);
            if (raw >= 0)
                itoa_u((uint32_t)raw, r + 4);
            else
                copy_str(r + 4, "-", 2);
            cv_text(14, 214, &FONT_XS, r, K_DIM);
        }
#endif
        text_c(176, 187, &FONT_B, amb.sc->name, K_TEXT);
        amb_knob_text(0, n, v);                    /* (the Mood; a dot: the player's tweaks) */
        text_c(176, 211, &FONT_B, v, K_KOTO);
        if (amb_tweaked(amb_mood()) && amb.reset_m < 0)
            cv_round(176 + text_w(&FONT_B, v) / 2 + 3, 218, 5, 5, 2, K_KOTO);
    } else if (plat_ms() < amb.touch_until && amb_hits(sy, sh, 160, 76)) {
        int i;
        cv_round(4, 178, 232, 58, 6, K_PANEL);
        for (i = 0; i < SC_NPAGE; i++) {             /* the pages' tabs over the bar, the one shown lit */
            static const char *const PAGE[SC_NPAGE] = {0, "Volume", "Nature", "Music"};
            const char *pn = i ? PAGE[i] : amb.sc->name;
            int on = i == amb.page;
            cv_round(4 + i * 58, 161, 56, 16, 4, on ? K_PANEL : K_BG);
            text_c(4 + i * 58 + 28, 162, &FONT_XS, pn, on ? K_KOTO : K_DIM);
        }
        for (i = 0; i < 4; i++) {
            char n[12], v[12];
            int32_t cx = i * 60 + 30;
            amb_knob_text(amb_knob_at(amb.page * 4 + i), n, v);
            if (!n[0])
                continue;
            {   /* its name, centered, but inside the card */
                int32_t nw = text_w(amb_name_font(n), n), nx = cx - nw / 2;
                nx = nx < 7 ? 7 : nx + nw > 233 ? 233 - nw : nx;
                cv_text(nx, 184, amb_name_font(n), n, i == amb.touched ? K_TEXT : K_DIM);
            }
            {   /* the value in the biggest face that fits its 60 px column ("Bubbles" and "Insects" ran
                 * into each other); words with lower case keep it (the big face has none) */
                const felucca_font_t *f = &FONT_M;
                const char *q = v;
                int lower = 0, dot = amb.page == 0 && i == 0 && amb_tweaked(amb_mood()) && amb.reset_m < 0;
                int32_t w, x0;
                for (; *q; q++)
                    lower |= *q >= 'a' && *q <= 'z';
                if (lower || text_w(f, v) > 56)
                    f = &FONT_B;
                if (text_w(f, v) > 56)
                    f = &FONT_S;
                if (text_w(f, v) > 56)
                    f = &FONT_XS;
                w = text_w(f, v);
                x0 = cx - (w + (dot ? 6 : 0)) / 2;         /* centered, with the tweaked mood's dot after it, but */
                if (x0 < 7)                                /* inside the card ("Quiet Night" and its dot ran off) */
                    x0 = 7;
                if (x0 + w > 233)
                    x0 = 233 - w;
                cv_text(x0, 216 - f->h / 2, f, v, i == amb.touched ? K_KOTO : K_TEXT);
                if (dot)                                   /* (the player's tweaks) */
                    cv_round(x0 + w + 2, 214, 4, 4, 2, i == amb.touched ? K_KOTO : K_TEXT);
            }
        }
    }
}

static void amb_frame(void)
{
    uint32_t now = plat_ms(), dt = now - amb.last_ms, t0, t1, t2;
    amb.last_ms = now;
    if (dt > 50u)
        dt = 50u;
    amb_input();
    if (!proj.mode)
        return;                                    /* HOME held: back to the instrument */
    amb_save_tick(now, dt);
    amb_batt_tick(now);
    amb_reset_tick(now);
    amb_fade_tick(dt);
    amb_sleep_tick(now);
    t0 = plat_us();
    amb.sc->step(dt);
#if ZN_HUD
    amb.step_us += plat_us() - t0;
#endif
    if (amb.asleep == 2)
        return;                                    /* asleep: the screen dark, nothing drawn */
    if (amb.asleep)
        amb.asleep = 2;
    amb.sc->draw();
#if ZN_HUD
    if (amb.calib)
        amb_calib_draw();
#endif
    t1 = plat_us();
    r8_blit(amb_overlay, amb_over);
    t2 = plat_us();
    amb.draw_us += t1 - t0;
    amb.blit_us += t2 - t1;
#if ZN_HUD
    amb.sent_px += r8_sent;
#endif
    amb.frames++;
    if (now - amb.t_sec >= 1000u) {
        amb.d10 = amb.draw_us / (amb.frames * 100u);
        amb.b10 = amb.blit_us / (amb.frames * 100u);
#if ZN_HUD
        {
            int i;
            amb.s10 = amb.step_us / (amb.frames * 100u);
            amb.sent_pct = amb.sent_px / (amb.frames * (R8_W * R8_H / 100u));
            amb.sent_px = 0;
            for (i = 0; i < SC_NPT; i++) {
                amb.pt10[i] = sc_pt_us[i] / (amb.frames * 100u);
                sc_pt_us[i] = 0;
            }
            amb.step_us = 0;
        }
#endif
        amb.frames = amb.draw_us = amb.blit_us = 0;
        amb.t_sec = now;
    }
}

/* SAVE held 3 s: every setting back to the defaults: the knobs of every scene and the mix,
 * the kept pictures, the instrument, the tuning, the style. From 0.8 s the screen counts down; letting go before
 * the end cancels (and does not save). It starts again in a scene, as at power-on. */
#define RESET_SHOW_MS 800u
#define RESET_HOLD_MS 3000u
static void reset_all(void)
{
    int i;
    fm_panic();
    if (amb.sc) {
        amb.sc->leave();
        amb.sc = 0;
    }
    amb.save_at = 0;
    amb.save_phase = 0;
    {
        uint32_t seq = proj.save_seq;              /* the count goes on: the reset must be newer than every save */
        uint8_t scene = proj.scene, mode = proj.mode;
        project_defaults();                        /* before it, or the next power-on loads one of them */
        proj.save_seq = seq;
        proj.scene = scene;                        /* the scene stays */
        proj.mode = mode;
    }
    project_apply();
    r8_fade = 256;
    ui.view = V_HOME;
    for (i = 0; i < 3; i++)
        ui.sig[i] = 0;
    if (project_save() == 0)
        ui.dirty = 0;
#if ZN_HUD
    hud.saves++;
#endif
    lcd_sync();
    lcd_fill(0, 0, 240, 240, K_BG);
    if (proj.mode)
        amb_enter();
    say("RESET", "ALL DEFAULTS");
    amb.welcome_until = plat_ms() + 15000u;
}

static int save_hold(void)                         /* 1: everything was just reset */
{
    uint32_t held;
    if (!(ui.btn >> B_SAVE & 1u)) {
        if (ui.reset_shown && !ui.reset_done)
            say("RESET", "CANCELED");
        ui.reset_shown = ui.reset_done = 0;
        return 0;
    }
    held = plat_ms() - ui.save_t;
    if (ui.reset_done || held < RESET_SHOW_MS)
        return 0;
    ui.btn_used |= 1u << B_SAVE;                   /* no save on release now */
    if (held >= RESET_HOLD_MS) {
        reset_all();
        ui.reset_done = 1;
        return 1;
    }
    {
        uint32_t left = (RESET_HOLD_MS - held + 999u) / 1000u;
        if (left != ui.reset_shown) {
            char d[4] = {(char)('0' + left), '.', '.', 0};
            say("RESET ALL IN", d);
            ui.reset_shown = (uint8_t)left;
        }
    }
    return 0;
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.tuning_prev = TN_SUIKO;
    lcd_fill(0, 0, 240, 240, K_BG);
    amb.sc = 0;
    if (proj.mode)
        amb_enter();
    if (project_fresh)
        amb.welcome_until = plat_ms() + 15000u;
}

void ui_frame(void)
{
#if ZN_HUD
    hud_tick();
#endif
    if (proj.mode) {
        amb_frame();
        if (proj.mode) {
            leds();
            ui.frame++;
            return;
        }
    }
    input();
    if (proj.mode)
        return;                                    /* HOME held: the scene draws from the next frame */
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void)
{
    if (proj.mode)
        amb_input();
    else
        input();
}
