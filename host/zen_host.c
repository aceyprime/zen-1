/* SPDX-License-Identifier: GPL-3.0-only */
/* FuMi-1 on the host: the whole app (UI, project, engine) against a simulated FM-1 driven by a script.
 * Audio goes to a WAV, the screen to PNGs, the lights to text, MIDI out to a log. The device-only
 * parts (HAL, USB, OTA, flash driver) are replaced by plat.h implemented here. (FoMni's, from X0X's.)
 *
 *   zen_host SCRIPT [OUTDIR]
 *
 * Script, one command per line (# comments):
 *   wait MS                      run the device for MS milliseconds
 *   press BTN | release BTN | tap BTN     (BTN: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+)
 *   key K down|up | tapkey K     (K: 0..26 = F3..G5, or w0..w15 white keys, b0..b10 black keys)
 *   strum A B [MS]               white keys A..B pressed in turn (down then up), MS apart (default 25)
 *   turn ENC N [MS]              (ENC: SELECT ALGO PRESET K1 K2 K3 K4; N detents, MS apart, default 80)
 *   spin ENC N                   N detents at once, exactly N (no acceleration)
 *   master N                     MASTER pot 0..4096
 *   midi B0 B1 B2                incoming USB MIDI message (hex bytes)
 *   wav FILE | wavstop           start / stop recording the output
 *   koifrog                      Koi: print each frog's state and place
 *   peakreset                    restart the output peak (expect peak_db_min / peak_db_max)
 *   shot FILE.png                save the screen
 *   leds                         print the lit buttons and keys
 *   expect WHAT VALUE            check state: exit 1 on mismatch; VALUE "<N" / ">N" is a bound
 *     WHAT: view parN (fumi.h P_*) dirty store_writes midi_out nvoices bend10 (cents x 10) keylevelK (x 100)
 *           custom peak_db_max peak_db_min jr_progs jr_erases save_seq mode style scnotes echoes voiceN landed
 *           kept regrows seed16 fade knobN dronetop dronelow (Hz, over every chord) scene soundN
 *   reboot                       re-run boot from the simulated flash (persistence test)
 *   oldsound N                   scene slot N's saved knobs as from before its sounds changed (proj.sound[N] = 0)
 *   jrtear N                     a power cut on the Nth save-journal page program from now (half of it written,
 *                                nothing after, until a reboot)
 */
#define OM_HOST 1
#define FM_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#define __attribute__(x)

/* ------------------------------------------------------------ screen --- */
static uint16_t fb[240 * 240];          /* RGB565, byte-swapped as the panel takes it (gfx.c) */
/* the transfer as the FM-1 makes it: a blit's pixels are read from its buffer later, while the next work goes
 * on, and arrive when the next screen access waits for it (lcd_sync, or another blit). A buffer written meanwhile is
 * what the screen gets (as on the device), and is counted: expect race 0 */
static struct {
    uint32_t x, y, w, h, on;
    const uint16_t *px;
    uint16_t snap[240 * 60];            /* what it held when the blit began */
} lcd_pend;
static uint32_t lcd_races;
static void lcd_flush_host(void)
{
    uint32_t i, j, n;
    if (!lcd_pend.on)
        return;
    n = lcd_pend.w * lcd_pend.h;
    if (n <= 240u * 60u && memcmp(lcd_pend.snap, lcd_pend.px, n * 2u))
        lcd_races++;
    for (j = 0; j < lcd_pend.h; j++)
        for (i = 0; i < lcd_pend.w; i++)
            if (lcd_pend.x + i < 240u && lcd_pend.y + j < 240u)
                fb[(lcd_pend.y + j) * 240u + lcd_pend.x + i] = lcd_pend.px[j * lcd_pend.w + i];
    lcd_pend.on = 0;
}
static void lcd_sync(void) { lcd_flush_host(); }
static uint16_t lcd_view_host(uint32_t i)   /* the screen at pixel i, the transfer running taken as arrived */
{
    uint32_t x = i % 240u, y = i / 240u;
    if (lcd_pend.on && x >= lcd_pend.x && x < lcd_pend.x + lcd_pend.w && y >= lcd_pend.y && y < lcd_pend.y + lcd_pend.h)
        return lcd_pend.snap[(y - lcd_pend.y) * lcd_pend.w + x - lcd_pend.x];
    return fb[i];
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    lcd_flush_host();
    uint16_t sc = (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = sc;
}
static uint32_t blits;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    blits++;
    lcd_flush_host();                   /* (the one before: done first, as lcd_data's sync) */
    if (w * h <= 240u * 60u) {          /* (left running: arrives at the next screen access) */
        lcd_pend.x = x;
        lcd_pend.y = y;
        lcd_pend.w = w;
        lcd_pend.h = h;
        lcd_pend.px = px;
        memcpy(lcd_pend.snap, px, w * h * 2u);
        lcd_pend.on = 1;
        return;
    }
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = px[j * w + i];
}
#include "../firmware/src/gfx.c"
#include "../firmware/src/app/zen_kanji.h"                  /* the patches' Japanese names for the screen (needs felucca_font_t) */


/* ---------------------------------------------------------- platform --- */
#include "../firmware/src/app/plat.h"

static uint32_t now_ms, held_btn, held_keys, master = 2048;
static int32_t enc_acc[NE];
static uint32_t lit_btn, lit_keys;

uint32_t plat_ms(void) { return now_ms; }
uint32_t plat_buttons(void) { return held_btn; }
uint32_t plat_keys(void) { return held_keys; }
int32_t plat_enc(int role)
{
    int32_t v = enc_acc[role];
    enc_acc[role] = 0;
    return v;
}
uint32_t plat_master(void) { return master; }
static int32_t host_batt = 600;                    /* (batt N: the battery's reading, for the tests) */
int32_t plat_battery_raw(void) { return host_batt; }
void plat_leds(uint32_t b, uint32_t k)
{
    lit_btn = b;
    lit_keys = k;
}
static int glow_on = 1;                 /* the device's button glow (plat_glow); the lights printed are the bright ones */
static uint32_t dim_btn, dim_keys;
static int dim_set;
void plat_glow(int on)
{
    glow_on = on;
    dim_set = 0;
}
void plat_leds_dim(uint32_t b, uint32_t k)   /* (a scene's panel: these dim) */
{
    dim_btn = b;
    dim_keys = k;
    dim_set = 1;
}
void plat_play_red(int red) { (void)red; }      /* the device's PLAY color: green, red while recording */

#define MQ 256
static uint32_t min_q[MQ], mi_w, mi_r;
static FILE *midi_log;
static uint32_t midi_out_count, clock_out_count;
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = min_q[mi_r++ % MQ];
    return 1;
}
void plat_midi_out(uint32_t pkt)
{
    midi_out_count++;
    if (((pkt >> 8) & 0xFF) == 0xF8) {
        clock_out_count++;
        return;
    }
    if (midi_log)
        fprintf(midi_log, "%u ms: %02X %02X %02X\n", now_ms, (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, (pkt >> 24) & 0xFF);
}
/* the engine's MIDI out, as the device's glue sends it (USB-MIDI packet, cable 0) */
void fm_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

/* flash: one buffer per object, kept across a simulated reboot */
static uint8_t store[OBJ_NOBJ][PLAT_STORE_MAX];
static int store_len[OBJ_NOBJ] = {-1, -1};
static uint32_t store_writes;
/* the save journal's flash, emulated as NOR: an erase sets 0xFF, a program can only clear bits. It starts full of
 * leftovers (as the data area may be, from the stock firmware), and outlives a reboot. "jrtear N": the Nth program
 * from now writes half its bytes and every later one nothing, until a reboot (a power cut mid-save). */
static uint8_t jr_flash[PLAT_JR_SECTOR * PLAT_JR_NSECTOR];
static uint32_t jr_progs, jr_erases;
static int jr_tear = -1, jr_dead;
static void jr_flash_init(void)
{
    uint32_t i, x = 0x2545F491u;
    for (i = 0; i < sizeof jr_flash; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        jr_flash[i] = (i / 4096u) % 3u == 1u ? 0xFFu : (uint8_t)x;   /* some sectors erased, most not */
    }
}
int plat_jr_read(uint32_t off, void *dst, uint32_t n)
{
#ifdef OM_WEB
    (void)off;
    (void)dst;
    (void)n;
    return -9;                                     /* the browser: no journal; saves take OBJ_PROJ, which the page keeps
                                                    * (localStorage) across a reload */
#endif
    if (off > sizeof jr_flash || n > sizeof jr_flash - off)
        return -9;
    memcpy(dst, jr_flash + off, n);
    return 0;
}
static int jr_progs_frame, jr_progs_max;
static double sent_tot;                            /* the screen sent, summed over frames (as the HUD's U) */
static long sent_frames;
static double send_us;                             /* the sends' time on the FM-1, estimated: 44 MHz SPI, 2 bytes a
                                                  * pixel (0.364 us), and ~12 us to set up each */
static uint32_t blits_was;
static long ht_jump_max;
static uint32_t gap_notes, gap_t, gap_max, gap_at;  /* the scene's notes: the longest stretch without one */                           /* the moon's biggest step a frame (x10 px), while it is up */
static float ht_jump_x = -1.0f, ht_jump_y;           /* journal programs in one frame, the most */
int plat_jr_prog(uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    if (++jr_progs_frame > jr_progs_max)
        jr_progs_max = jr_progs_frame;
#ifdef OM_WEB
    return -9;
#endif
    if (off > sizeof jr_flash || n > sizeof jr_flash - off || n == 0 || n > 256u || (off & 0xFFu) + n > 256u)
        return -9;
    if (jr_dead)
        return 0;                                  /* (the power is gone: the app does not know yet) */
    if (jr_tear == 0) {
        n /= 2;
        jr_dead = 1;
    }
    if (jr_tear >= 0)
        jr_tear--;
    for (i = 0; i < n; i++)
        jr_flash[off + i] &= ((const uint8_t *)src)[i];
    jr_progs++;
    return 0;
}
int plat_jr_erase(uint32_t sector)
{
#ifdef OM_WEB
    return -9;
#endif
    if (sector >= PLAT_JR_NSECTOR)
        return -9;
    memset(jr_flash + sector * PLAT_JR_SECTOR, 0xFF, PLAT_JR_SECTOR);
    jr_erases++;
    return 0;
}
uint32_t plat_jr_prog_us_max(void) { return 0; }

int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    int n;
    if (obj >= OBJ_NOBJ || (n = store_len[obj]) < 0)
        return -1;
    if ((uint32_t)n > max)
        n = (int)max;
    memcpy(dst, store[obj], (size_t)n);
    return n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    if (jr_dead)
        return 0;                                  /* a power cut (jrtear) stops every write, not only the journal's */
    memcpy(store[obj], src, len);
    store_len[obj] = (int)len;
    store_writes++;
    return 0;
}

static uint32_t cpu_pct, cpu_max;
uint32_t plat_cpu_pct(void) { return cpu_pct; }
uint32_t plat_cpu_max(void) { return cpu_max; }
void plat_cpu_max_reset(void) { cpu_max = 0; }
uint32_t plat_xruns(void) { return 0; }

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/ui.c"

/* ---------------------------------------------------------- outputs --- */
static void png_write(const char *path)
{
    /* RGB PNG with stored (uncompressed) deflate blocks: no zlib needed */
    static uint8_t raw[240 * (1 + 240 * 3)];
    FILE *f = fopen(path, "wb");
    uint32_t crc_t[256], i, j, k, n = 0, a = 1, b = 0, pos, len = sizeof raw;
    uint8_t hdr[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_t[i] = c;
    }
    for (j = 0; j < 240; j++) {
        raw[n++] = 0;
        for (i = 0; i < 240; i++) {
            uint16_t p = fb[j * 240 + i];
            p = (uint16_t)((p >> 8) | (p << 8));
            raw[n++] = (uint8_t)(((p >> 11) & 31) * 255 / 31);
            raw[n++] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
            raw[n++] = (uint8_t)((p & 31) * 255 / 31);
        }
    }
    for (i = 0; i < len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
#define PUT32(v) do { uint8_t q_[4] = {(uint8_t)((v) >> 24), (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v)}; fwrite(q_, 1, 4, f); } while (0)
    fwrite(hdr, 1, 8, f);
    {
        uint8_t ih[17] = {'I', 'H', 'D', 'R', 0, 0, 0, 240, 0, 0, 0, 240, 8, 2, 0, 0, 0};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(13u);
        fwrite(ih, 1, 17, f);
        for (i = 0; i < 17; i++)
            c = crc_t[(c ^ ih[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    {
        uint32_t nblk = (len + 65534) / 65535, zlen = 2 + len + nblk * 5 + 4, c = 0xFFFFFFFFu;
        uint8_t *z = malloc(zlen + 4), *q = z;
        memcpy(q, "IDAT", 4);
        q += 4;
        *q++ = 0x78;
        *q++ = 0x01;
        for (pos = 0; pos < len;) {
            uint32_t m = len - pos > 65535 ? 65535 : len - pos;
            *q++ = pos + m >= len ? 1 : 0;
            *q++ = (uint8_t)m;
            *q++ = (uint8_t)(m >> 8);
            *q++ = (uint8_t)~m;
            *q++ = (uint8_t)(~m >> 8);
            memcpy(q, raw + pos, m);
            q += m;
            pos += m;
        }
        *q++ = (uint8_t)(b >> 8);
        *q++ = (uint8_t)b;
        *q++ = (uint8_t)(a >> 8);
        *q++ = (uint8_t)a;
        PUT32(zlen);
        fwrite(z, 1, zlen + 4, f);
        for (i = 0; i < zlen + 4; i++)
            c = crc_t[(c ^ z[i]) & 255] ^ (c >> 8);
        PUT32(~c);
        free(z);
    }
    {
        uint8_t ie[4] = {'I', 'E', 'N', 'D'};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(0u);
        fwrite(ie, 1, 4, f);
        for (i = 0; i < 4; i++)
            c = crc_t[(c ^ ie[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    fclose(f);
}

static FILE *wav;
static uint32_t wav_frames;
static float peak_out;
static void wav_open(const char *path)
{
    static const uint8_t h[44] = {0};
    wav = fopen(path, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_frames = 0;
}
#if __has_include("zen_host_extra.h")   /* local extras, if present: more commands (host_extra_cmd) and a step each
                                         * simulated ms (host_extra_ms) */
#include "zen_host_extra.h"
#else
static void host_extra_ms(void) {}
static int host_extra_cmd(const char *cmd, const char *a, const char *b, const char *dir) { return 0; }
#endif
static void wav_close(void)
{
    uint32_t bytes = wav_frames * 4, v;
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    v = 36 + bytes;
    fwrite(&v, 4, 1, wav);
    fwrite("WAVEfmt ", 1, 8, wav);
    v = 16;
    fwrite(&v, 4, 1, wav);
    {
        uint16_t fmt[2] = {1, 2};
        uint32_t r[2] = {44100, 44100 * 4};
        uint16_t al[2] = {4, 16};
        fwrite(fmt, 2, 2, wav);
        fwrite(r, 4, 2, wav);
        fwrite(al, 2, 2, wav);
    }
    fwrite("data", 1, 4, wav);
    fwrite(&bytes, 4, 1, wav);
    fclose(wav);
    wav = 0;
}

/* --------------------------------------------------------- the device --- */
static double audio_due_ms, render_ns_total, render_budget_ns_total;
static uint32_t ui_due;
static double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}
uint32_t plat_us(void) { return (uint32_t)(uint64_t)(now_ns() / 1000.0); }   /* real time: the host's own speed */

static void boot(void)
{
    static int once;
    if (!once) {                                  /* the journal's flash: leftovers, once (it outlives reboots) */
        jr_flash_init();
        once = 1;
    }
    jr_dead = 0;                                  /* the power is back */
    jr_tear = -1;
    fm_init();
    project_load();
    project_apply();
    lcd_fill(0, 0, 240, 240, 0);
    ui_init();
}

static void run_ms(uint32_t ms)
{
    static int32_t blk[512];
    uint32_t i, k;
    for (i = 0; i < ms; i++) {
        now_ms++;
        while (audio_due_ms <= (double)now_ms) {      /* 256-frame blocks, as the device renders them */
            double t0 = now_ns(), dt;
            fm_render(blk, 256, master);
#ifdef OM_WEB
            web_audio(blk, 256);
#endif
            dt = now_ns() - t0;
            render_ns_total += dt;
            render_budget_ns_total += 256.0 * 1e9 / 44100.0;
            cpu_pct = (uint32_t)(100.0 * dt / (256.0 * 1e9 / 44100.0));
            if (cpu_pct > cpu_max)
                cpu_max = cpu_pct;
            for (k = 0; k < 256; k++) {
                float l = (float)blk[2 * k] / 8388608.0f, r = (float)blk[2 * k + 1] / 8388608.0f;
                int16_t s[2];
                if (fabsf(l) > peak_out)
                    peak_out = fabsf(l);
                if (fabsf(r) > peak_out)
                    peak_out = fabsf(r);
                s[0] = (int16_t)(l * 32767.0f);
                s[1] = (int16_t)(r * 32767.0f);
                if (wav) {
                    fwrite(s, 2, 2, wav);
                    wav_frames++;
                }
            }
            audio_due_ms += 256.0 * 1000.0 / 44100.0;
        }
        host_extra_ms();
        if (now_ms >= ui_due) {
            ui_due = now_ms + 16;
            jr_progs_frame = 0;
            ui_frame();
            if (proj.mode) {
                sent_tot += (double)r8_sent;
                send_us += 0.364 * (double)r8_sent + 12.0 * (double)(blits - blits_was);
                sent_frames++;
            }
            blits_was = blits;
            if (proj.mode) {
                if (cd.notes != gap_notes) {
                    gap_notes = cd.notes;
                    gap_t = now_ms;
                } else if (now_ms - gap_t > gap_max) {
                    gap_max = now_ms - gap_t;
                    gap_at = now_ms;
                }
            }
            if (proj.mode && amb.sc == &SCENE_HOTARU) {
                float mx = ht_moon_x(), my = ht_moon_y();
                if (ht_jump_x >= 0.0f && my < 200.0f && ht_jump_y < 200.0f) {
                    long d = (long)(10.0f * fm_sqrtf((mx - ht_jump_x) * (mx - ht_jump_x) + (my - ht_jump_y) * (my - ht_jump_y)));
                    if (d > ht_jump_max)
                        ht_jump_max = d;
                }
                ht_jump_x = mx;
                ht_jump_y = my;
            }
        } else {
            ui_input_only();
        }
    }
}

/* ---------------------------------------------------------- script --- */
static const char *const BTN_N[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                      "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_N[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

static int find_name(const char *const *t, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i], s))
            return i;
    return -1;
}

static int key_of(const char *s)
{
    if (s[0] == 'w')
        return WHITE_K[atoi(s + 1) & 15];
    if (s[0] == 'b')
        return BLACK_K[atoi(s + 1) % 11];
    return atoi(s);
}

static int expect(const char *what, const char *val)
{
    int got;
    if (!strcmp(what, "view"))
        got = ui.view;
    else if (!strcmp(what, "dirty"))
        got = ui.dirty;
    else if (!strcmp(what, "mode"))               /* 0 the instrument, 1 ambient */
        got = proj.mode;
    else if (!strcmp(what, "style"))
        got = proj.style;
    else if (!strcmp(what, "blinks"))             /* Hotaru: fireflies' blinks; the chords heard; frogs' calls; the
                                                  * flute's phrases; fireflies the keys released; scatters; how in step
                                                  * the field is (0..100); fireflies flying */
        got = (int)ht.blinks;
    else if (!strcmp(what, "pulses"))
        got = (int)ht.pulses;
    else if (!strcmp(what, "frogcalls"))
        got = (int)ht.frogcalls;
    else if (!strcmp(what, "raftpetals")) {       /* Sakura: petals in the rafts on the water now */
        int i;
        got = 0;
        for (i = 0; i < SK_NRAFT; i++)
            got += sk_raft[i].n > 0.0f && sk_raft[i].x > 0.0f && sk_raft[i].x < 240.0f ? (int)sk_raft[i].n : 0;
    }
    else if (!strcmp(what, "rafts"))              /* Sakura: petal rafts, butterflies, birds, songs, showers */
        got = (int)sk.rafts;
    else if (!strcmp(what, "butterflies"))
        got = (int)sk.butterflies;
    else if (!strcmp(what, "birds"))
        got = (int)sk.birds;
    else if (!strcmp(what, "songs"))
        got = (int)sk.songs;
    else if (!strcmp(what, "showers"))
        got = (int)sk.showers;
    else if (!strcmp(what, "keynote"))            /* Sakura: the note the last key played */
        got = sk.key_note;
    else if (!strncmp(what, "part", 4) && what[4] >= '0' && what[4] <= '9')   /* partN: the patch part N was given */
        got = fm_part_patch_set[(what[4] - '0') % FM_NPART];
    else if (!strcmp(what, "harpkeys"))           /* Sakura: harp notes the black keys played */
        got = (int)sk.harp_keys;
    else if (!strcmp(what, "frogs"))              /* Hotaru's frogs: 0 the recording, 1 synthesized (LFO) */
        got = proj.frogs;
    else if (!strcmp(what, "flashes"))            /* Hotaru: black-key notes (a firefly flashes) */
        got = (int)ht.key_flashes;
    else if (!strcmp(what, "relkey"))             /* Hotaru: the note of the last firefly a key released */
        got = ht.rel_key;
    else if (!strcmp(what, "owls"))               /* Hotaru: the owl's calls; shooting stars */
        got = (int)ht.owls;
    else if (!strcmp(what, "stars"))
        got = (int)ht.stars;
    else if (!strcmp(what, "released"))
        got = (int)ht.released;
    else if (!strcmp(what, "scatters"))
        got = (int)ht.scatters;
    else if (!strcmp(what, "lastkey"))            /* the white key of the last scene note (conductor.c) */
        got = cd.last_deg;
    else if (!strcmp(what, "insync"))
        got = (int)(ht.r_sync * 100.0f + 0.5f);
    else if (!strcmp(what, "insyncavg"))          /* the same, averaged over ~20 s (a few fireflies line up by chance) */
        got = (int)(ht.r_avg * 100.0f + 0.5f);
    else if (!strcmp(what, "insyncmin"))          /* the lowest of that average after the first minute: the field
                                                  * fell out of step at some point (it holds a pulse a while, then
                                                  * drifts apart) */
        got = (int)(ht.r_min * 100.0f + 0.5f);
    else if (!strcmp(what, "fireflies"))
        got = ht_alive();
    else if (!strcmp(what, "textfit")) {          /* every knob's name and every value it can show, in this
                                                  * scene, fit its quarter of the bar (56 px): the ones that don't */
        const int16_t (*kt)[4] = amb.sc == &SCENE_SAKURA ? SK_KNOB : amb.sc == &SCENE_KOI ? KP_KNOB : HT_KNOB;
        int i, v;
        got = 0;
        if (!amb.sc) {
            printf("  textfit: no scene playing\n");
            got = 99;
        }
        for (i = 0; amb.sc && i < SC_NUSED; i++) {
            int16_t keep = amb.kv[i];
            int lo = kt[i][0], hi = kt[i][1] > kt[i][0] ? kt[i][1] : 100;
            for (v = lo; v <= hi; v++) {
                char n[12], val[12];
                const char *q;
                int lower = 0;
                const felucca_font_t *f = &FONT_M;
                if (kt[i][1] > kt[i][0])
                    amb.kv[i] = (int16_t)v;
                else
                    proj.par[P_REVERB] = (int16_t)v;
                amb.sc->knob_text(i, n, val);
                for (q = val; *q; q++)
                    lower |= *q >= 'a' && *q <= 'z';
                if (lower || text_w(f, val) > 56)
                    f = &FONT_B;
                if (text_w(f, val) > 56)
                    f = &FONT_S;
                if (text_w(f, val) > 56)
                    f = &FONT_XS;
                if (text_w(f, val) > 56 || text_w(amb_name_font(n), n) > 56) {
                    if (got < 8)
                        printf("  textfit: %s knob %d \"%s\" = \"%s\" (%d, %d px)\n", amb.sc->name, i, n, val,
                               (int)text_w(amb_name_font(n), n), (int)text_w(f, val));
                    got++;
                }
            }
            amb.kv[i] = keep;
        }
    }
    else if (!strcmp(what, "race"))               /* screen transfers whose buffer was written before they ended */
        got = (long)lcd_races;
    else if (!strcmp(what, "stale"))              /* strips whose screen differed from the drawn frame (sent in part) */
        got = (long)r8_stale;
    else if (!strncmp(what, "led", 3)) {          /* ledNAME: a button's light: 2 bright, 1 dim, 0 off */
        int i;
        for (got = -1, i = 0; i < NB; i++)
            if (!strcmp(what + 3, BTN_N[i]))
                got = (lit_btn >> i & 1u) ? 2 : (dim_set ? (int)(dim_btn >> i & 1u) : glow_on);
    } else if (!strncmp(what, "keylight", 8)) {   /* keylightwN / keylightbN: a key's light, 2 bright, 1 dim, 0 off */
        int n = atoi(what + 9), id = what[8] == 'w' ? WHITE_K[n & 15] : BLACK_K[n % 11];
        got = (lit_keys >> id & 1u) ? 2 : (dim_set ? (int)(dim_keys >> id & 1u) : 0);
    }
    else if (!strcmp(what, "scene"))              /* which scene (SCENES[] in ui.c: 0 Sakura, 1 Koi, 2 Hotaru) */
        got = proj.scene;
    else if (!strncmp(what, "sound", 5))          /* soundN: scene slot N's sounds' version, as saved */
        got = proj.sound[atoi(what + 5) & 3];
    else if (!strcmp(what, "scnotes"))            /* notes the conductor has played (echoes apart) */
        got = (int)cd.notes;
    else if (!strcmp(what, "keyoff"))             /* the drift's key offset, cents */
        got = (int)cd.key_off;
    else if (!strcmp(what, "scalecur"))           /* the drift's scale (the engine's, in a scene) */
        got = cd.scale_cur;
    else if (!strcmp(what, "key1cents"))          /* the pitch white key 1 plays now (the engine's tuning), cents */
        got = (int)fm_key_cents(1);
    else if (!strcmp(what, "changes"))            /* key or scale moves so far */
        got = (int)cd.changes;
    else if (!strcmp(what, "drifted"))            /* 1 if the drift has moved the key or the scale */
        got = cd.key_off != 0.0f || cd.scale_cur != proj.par[P_SCALE];
    else if (!strcmp(what, "koieaten"))           /* Koi: food eaten */
        got = (int)kp.eaten;
    else if (!strcmp(what, "expired"))            /* Koi: food that sank uneaten */
        got = (int)kp.expired;
    else if (!strcmp(what, "spawned"))            /* Koi: food that has fallen in */
        got = (int)kp.spawned;
    else if (!strcmp(what, "koinote"))            /* Koi: the note the last white key played */
        got = kp.key_note;
    else if (!strcmp(what, "quiets"))             /* the notes' rests */
        got = (long)cd.quiets;
    else if (!strcmp(what, "loopnotes"))          /* notes the loops chose */
        got = (long)cd.loop_notes;
    else if (!strcmp(what, "arc"))                /* the arc now, -100 .. 100 */
        got = (long)(cd.arc * 100.0f);
    else if (!strcmp(what, "breaths"))            /* the drone's breaths, and those sat out */
        got = (long)cd.dv_breaths;
    else if (!strcmp(what, "sits"))
        got = (long)cd.dv_sits;
    else if (!strcmp(what, "dropson")) {          /* rain drops in the air now (Sakura, Hotaru) */
        int i;
        got = 0;
        if (amb.sc == &SCENE_HOTARU)
            for (i = 0; i < HT_NDROP; i++)
                got += ht_drop[i].on;
        else if (amb.sc == &SCENE_SAKURA)
            for (i = 0; i < SK_NDROP; i++)
                got += sk_drop[i].on;
    } else if (!strcmp(what, "page"))               /* the knobs' page shown (0 the scene's, Volume, Nature, 3 Music) */
        got = amb.page;
    else if (!strcmp(what, "tweaks"))             /* the scene's tweaked moods, a bit each */
        got = proj.tweak[SCENES[proj.scene % NSCENES]->slot & 3];
    else if (!strcmp(what, "resetm"))             /* the mood the Mood knob is on "Reset" of, or -1 */
        got = amb.reset_m;
    else if (!strcmp(what, "notegap")) {         /* the longest stretch with no scene note since the last look, s */
        printf("  (longest gap ended %.0f s in)\n", gap_at / 1000.0);
        got = (long)(gap_max / 1000u);
        gap_max = 0;
    } else if (!strcmp(what, "sentavg")) {         /* the share of the screen sent a frame, %, since the last look */
        got = sent_frames ? (long)(100.0 * sent_tot / (double)sent_frames / (240.0 * 240.0) + 0.5) : 0;
        printf("  (sends ~%.1f ms a frame)\n", sent_frames ? send_us / 1000.0 / (double)sent_frames : 0.0);
        sent_tot = 0.0;
        send_us = 0.0;
        sent_frames = 0;
    } else if (!strcmp(what, "moonjump"))           /* Hotaru: the most the moon moved in one frame (px x10),
                                                  * while up, since the last look */
        got = ht_jump_max, ht_jump_max = 0;
    else if (!strcmp(what, "progsframe"))         /* the most journal programs in one frame (a save a piece a
                                                  * frame in a scene, the key lights scanned between) */
        got = jr_progs_max;
    else if (!strcmp(what, "raindrops"))          /* Koi: drops seen and heard landing on the pond */
        got = (long)kp.raindrops;
    else if (!strcmp(what, "rainnow"))            /* how rainy the scene is now, 0 .. 100 (eased; Koi, Hotaru) */
        got = (long)((amb.sc == &SCENE_KOI ? kp.rain : ht.rain) * 100.0f + 0.5f);
    else if (!strcmp(what, "pings"))              /* Hotaru: star glints heard */
        got = (long)ht.pings;
    else if (!strcmp(what, "weathers"))           /* Hotaru: the weather chosen (each night, and changes) */
        got = (long)ht.weathers;
    else if (!strcmp(what, "wx"))                 /* Hotaru: the weather now: 0 clear, 1 mist, 2 rain */
        got = ht.wx;
    else if (!strcmp(what, "depthsent"))          /* Hotaru: the filter asked for (Depth and the weather's) */
        got = ht.depth_sent;
    else if (!strcmp(what, "koifrogs"))           /* Koi: frogs on the pads, their plops, breezes */
        got = (int)kp.frogs;
    else if (!strcmp(what, "plops"))
        got = (int)kp.plops;
    else if (!strcmp(what, "breezes"))
        got = (int)kp.breezes;
    else if (!strcmp(what, "pebbles"))            /* Koi: pebbles tossed (black keys) */
        got = (int)kp.pebbles;
    else if (!strcmp(what, "knocks"))             /* Koi: the shishi-odoshi's knocks */
        got = (int)kp.knocks;
    else if (!strcmp(what, "memories"))           /* memory notes played */
        got = (int)cd.memories;
    else if (!strcmp(what, "heavies"))            /* heavy falls the conductor has sounded */
        got = cd.heavy_n;
    else if (!strcmp(what, "echoes"))             /* echoes the conductor has played */
        got = (int)cd.echoes;
    else if (!strcmp(what, "drones")) {           /* the drone's notes held (three voices, two ids each: 0..5) */
        int k;
        for (got = 0, k = 0; k < 6; k++)
            got += fm_voice_held(FM_VOICE_ID + k);
    } else if (!strncmp(what, "voice", 5))          /* voiceN: 1 if the scene note FM_VOICE_ID + N sounds, held */
        got = fm_voice_held(FM_VOICE_ID + atoi(what + 5));
    else if (!strcmp(what, "landed"))             /* Sakura: petals that have landed */
        got = (int)sk.landed;
    else if (!strcmp(what, "kept"))               /* the scene's picture is kept (REC) */
        got = 0;                                   /* (nothing is kept) */
    else if (!strcmp(what, "regrows"))            /* new pictures grown in this scene (PLAY, nights) */
        got = (int)amb.regrows;
    else if (!strcmp(what, "seed16"))             /* the picture's seed, low 16 bits */
        got = (int)(amb.seed & 0xFFFFu);
    else if (!strcmp(what, "fade"))               /* the palette's brightness, 0..256 */
        got = (int)r8_fade;
    else if (!strcmp(what, "sleep"))              /* the sleep timer's setting (0 OFF, 1 15 m .. 6 8 h) */
        got = amb.sleep;
    else if (!strcmp(what, "asleep"))             /* the sleep timer ran out: dark and quiet (1 or 2) */
        got = amb.asleep;
    else if (!strcmp(what, "sleepdim"))           /* its dimming of the picture, 0..256 */
        got = (int)r8_sleep;
    else if (!strcmp(what, "sleeplevel"))         /* its sound level, % */
        got = (int)(amb.sleep_f * 100.0f + 0.5f);
    else if (!strcmp(what, "dronetop") || !strcmp(what, "dronelow")) {   /* over every chord, at the tuning now: */
        int c0 = cd.chord, c;                         /* the highest upper note / the lowest root, Hz */
        float top = 0.0f, low = 1e9f, dc[3];
        for (c = 0; c < 5; c++) {
            cd.chord = c;
            cd_drone_cents(dc);
            top = fmaxf(top, 440.0f * powf(2.0f, fmaxf(dc[1], dc[2]) / 1200.0f));
            low = fminf(low, 440.0f * powf(2.0f, dc[0] / 1200.0f));
        }
        cd.chord = c0;
        got = (int)(what[5] == 't' ? top : low);
    }
    else if (!strncmp(what, "knob", 4))           /* knobN: the current scene's saved knob N (0..7) */
        got = amb.kv[atoi(what + 4) % SC_NKNOB];
    else if (!strcmp(what, "jr_progs"))           /* page programs into the save journal */
        got = (int)jr_progs;
    else if (!strcmp(what, "jr_reclaims"))        /* journal sectors reclaimed when it was full */
        got = (int)jr.reclaims;
    else if (!strcmp(what, "jr_free"))            /* erased journal slots left in a row */
        got = jr.free;
    else if (!strcmp(what, "jr_erases"))          /* sector erases of the save journal (at boot only) */
        got = (int)jr_erases;
    else if (!strcmp(what, "save_seq"))           /* the project's save count */
        got = (int)proj.save_seq;
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "midi_out"))
        got = (int)midi_out_count;
    else if (!strcmp(what, "nvoices"))
        got = fm_nvoices;
    else if (!strcmp(what, "custom"))
        got = fm_patch_custom;
    else if (!strcmp(what, "bend10"))
        got = (int)(fm_bend_cents * 10.0f);
    else if (!strncmp(what, "par", 3))            /* parN: parameter N (fumi.h P_*) */
        got = proj.par[atoi(what + 3)];
    else if (!strncmp(what, "keylevel", 8))        /* keylevelK: key K's level x 100 */
        got = (int)(fm_key_level[atoi(what + 8)] * 100.0f);
    else if (!strcmp(what, "peak_db_max") || !strcmp(what, "peak_db_min")) {
        double db = 20.0 * log10(peak_out > 1e-9f ? peak_out : 1e-9f);
        int bad = what[8] == 'm' && what[9] == 'a' ? db > atof(val) : db < atof(val);
        printf("%s peak %.1f dBFS (%s %s)\n", bad ? "FAIL" : "  ok", db, what + 8, val);
        return bad;
    } else {
        printf("FAIL unknown expect %s\n", what);
        return 1;
    }
    if (val[0] == '<' || val[0] == '>') {
        int lim = (int)strtol(val + 1, 0, 0);
        if (val[0] == '<' ? got >= lim : got <= lim) {
            printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (got != (int)strtol(val, 0, 0)) {
        printf("FAIL expect %s == %s, got %d\n", what, val, got);
        return 1;
    }
    printf("  ok %s == %d\n", what, got);
    return 0;
}

#ifndef OM_WEB
int main(int argc, char **argv)
{
    FILE *sc;
    char line[512], out[400];
    const char *dir = argc > 2 ? argv[2] : ".";
    int lineno = 0, fails = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: zen_host SCRIPT [OUTDIR]\n");
        return 2;
    }
    sc = fopen(argv[1], "r");
    if (!sc) {
        perror(argv[1]);
        return 2;
    }
    snprintf(out, sizeof out, "%s/midi_out.txt", dir);
    midi_log = fopen(out, "w");
    boot();
    while (fgets(line, sizeof line, sc)) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0};
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c) < 1)
            continue;
        if (!strcmp(cmd, "wait"))
            run_ms((uint32_t)atoi(a));
        else if (!strcmp(cmd, "press") || !strcmp(cmd, "release") || !strcmp(cmd, "tap")) {
            int i = find_name(BTN_N, NB, a);
            if (i < 0) {
                printf("line %d: no button %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] != 'r')
                held_btn |= 1u << i;
            if (cmd[0] == 't')
                run_ms(40);
            if (cmd[0] != 'p')
                held_btn &= ~(1u << i);
            run_ms(cmd[0] == 't' ? 40 : 2);
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "tapkey")) {
            int k = key_of(a);
            if (cmd[0] == 't') {
                held_keys |= 1u << k;
                run_ms(60);
                held_keys &= ~(1u << k);
                run_ms(30);
            } else {
                if (!strcmp(b, "down"))
                    held_keys |= 1u << k;
                else
                    held_keys &= ~(1u << k);
                run_ms(2);
            }
        } else if (!strcmp(cmd, "strum")) {          /* strum A B [MS]: a finger across the white keys */
            int x = atoi(a), y = atoi(b), gap = c[0] ? atoi(c) : 25, s = y >= x ? 1 : -1;
            for (;; x += s) {
                held_keys |= 1u << WHITE_K[x & 15];
                run_ms((uint32_t)gap);
                held_keys &= ~(1u << WHITE_K[x & 15]);
                if (x == y)
                    break;
            }
            run_ms(2);
        } else if (!strcmp(cmd, "turn")) {
            int e = find_name(ENC_N, NE, a), n = atoi(b), s = n > 0 ? 1 : -1;
            uint32_t gap = c[0] ? (uint32_t)atoi(c) : 80u;
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            for (; n; n -= s) {
                enc_acc[e] += s;
                run_ms(gap);
            }
        } else if (!strcmp(cmd, "spin")) {
            int e = find_name(ENC_N, NE, a);
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            enc_acc[e] += atoi(b);
            accel_off = 1;
            run_ms(1);
            accel_off = 0;
        } else if (!strcmp(cmd, "master"))
            master = (uint32_t)atoi(a);
        else if (!strcmp(cmd, "midi")) {
            uint32_t s = (uint32_t)strtoul(a, 0, 16), d1 = (uint32_t)strtoul(b, 0, 16), d2 = (uint32_t)strtoul(c, 0, 16);
            uint32_t cin = s >= 0xF0 ? 0x0F : s >> 4;
            min_q[mi_w++ % MQ] = cin | s << 8 | d1 << 16 | d2 << 24;
        } else if (!strcmp(cmd, "wav")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            wav_open(out);
        } else if (!strcmp(cmd, "wavstop"))
            wav_close();
        else if (host_extra_cmd(cmd, a, b, dir))
            ;
        else if (!strcmp(cmd, "koifrog")) {     /* Koi: each frog's state (0 in the water, 1 hopping on, 2 sitting,
                                                     * 3 hopping off) and place */
            int i;
            for (i = 0; i < KP_NFROG; i++)
                printf("  koifrog %d: %d %.0f %.0f from-open %d into-open %d\n", i, kp_fg[i].st, kp_fg[i].x, kp_fg[i].y,
                       kp_open(kp_fg[i].x0, kp_fg[i].y0), kp_open(kp_fg[i].x1, kp_fg[i].y1));
        }
        else if (!strcmp(cmd, "peakreset"))
            peak_out = 0.0f;
        else if (!strcmp(cmd, "shot")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            lcd_flush_host();
            png_write(out);
        } else if (!strcmp(cmd, "leds")) {
            int i;
            printf("  leds: buttons");
            for (i = 0; i < NB; i++)
                if (lit_btn >> i & 1u)
                    printf(" %s", BTN_N[i]);
            printf(" | white ");
            for (i = 0; i < 16; i++)
                printf("%c", (lit_keys >> WHITE_K[i] & 1u) ? '#' : '.');
            printf(" black ");
            for (i = 0; i < 11; i++)
                printf("%c", (lit_keys >> BLACK_K[i] & 1u) ? '#' : '.');
            printf("\n");
        } else if (!strcmp(cmd, "expect"))
            fails += expect(a, b);
        else if (!strcmp(cmd, "drawbench")) {    /* drawbench: the scene's draw, timed on the host (ms a frame, 200
                                                  * frames): for comparing, not the FM-1's own figure */
            struct timespec t0, t1;
            int n;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (n = 0; n < 200; n++)
                amb.sc->draw();
            clock_gettime(CLOCK_MONOTONIC, &t1);
            printf("drawbench %s: %.3f ms a frame\n", amb.sc->name,
                   ((double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-6) / 200.0);
#if ZN_HUD
            {   /* the scene's parts (scene.h SC_PT), as the HUD's third line shows them on the FM-1 */
                int i;
                for (i = 0; i < SC_NPT; i++)
                    sc_pt_us[i] = 0;
                for (n = 0; n < 200; n++)
                    amb.sc->draw();
                for (i = 0; i < SC_NPT && sc_pt_tag[i]; i++)
                    printf("  %c %.4f ms\n", sc_pt_tag[i], (double)sc_pt_us[i] / 200e3);
            }
#endif
        } else if (!strcmp(cmd, "batt")) {          /* batt N: the battery's reading from now */
            host_batt = atoi(a);
        } else if (!strcmp(cmd, "keystep") || !strcmp(cmd, "scalestep")) {   /* keystep / scalestep D: the player's
                                                  * key or scale D steps (in a scene SELECT and ALGORITHM turn
                                                  * Activity and the Mood now: the tests' way to them) */
            if (cmd[0] == 'k')
                knob_set(P_HON, proj.par[P_HON] + atoi(a));
            else
                param_step(P_SCALE, atoi(a));
            cd_drift_home();
            run_ms(20);
        } else if (!strcmp(cmd, "dawns")) {         /* dawns N: N of the scene's dawns now (the key's slow drift, * a day is half an hour) */
            int n = atoi(a);
            while (n-- > 0)
                cd_dawn();
            run_ms(100);
        } else if (!strcmp(cmd, "dayat")) {         /* dayat F: the time of day now (0 dawn .. 0.25 noon .. 0.5 dusk; for
                                                  * looking at a time's picture) */
            sk.day = kp.day = (float)atof(a);
            run_ms(100);
        } else if (!strcmp(cmd, "koiset")) {       /* koiset I V X:Y: Koi's koi I, variety V (KV_*; +100 a butterfly
                                                  * koi), at X, Y, heading right, at the surface, hovering (a picture
                                                  * of the varieties: tools/koi_sheet) */
            int i = atoi(a) % KP_MAXK, v = atoi(b), sg;
            float x = 120.0f, y = 124.0f;
            sscanf(c, "%f:%f", &x, &y);
            kp_koi_new(i, x, y);
            kp_paint(&kp_k[i], v % 100);
            kp_k[i].bfly = (uint8_t)(v >= 100);
            kp_k[i].size = 1.0f;
            kp_k[i].h = 0.0f;
            kp_k[i].z = 0.05f;
            kp_k[i].zv = 0.0f;
            kp_k[i].v = 0.0f;
            kp_k[i].st = 1;
            kp_k[i].stt = 1e6f;
            kp_k[i].full = 1e6f;
            for (sg = 0; sg < KP_NSEG; sg++) {
                kp_k[i].x[sg] = x - (float)sg * 3.0f;
                kp_k[i].y[sg] = y;
            }
        } else if (!strcmp(cmd, "koilist")) {      /* koilist: Koi's koi, their varieties and sizes, printed */
            int i;
            printf("  koi:");
            for (i = 0; i < KP_MAXK; i++)
                if (kp_k[i].on)
                    printf(" %d(%.2f)", kp_k[i].var, kp_k[i].size);
            printf("\n");
        } else if (!strcmp(cmd, "koioff")) {       /* koioff: no koi, no food (before koiset) */
            int i;
            for (i = 0; i < KP_MAXK; i++)
                kp_k[i].on = 0;
            for (i = 0; i < KP_MAXF; i++)
                kp_f[i].on = 0;
            kp.call_t = 0.0f;
        } else if (!strcmp(cmd, "oldsound"))       /* oldsound N: scene slot N's knobs as if saved before its sounds
                                                  * changed (proj.sound[N] = 0): the next save and boot renew them */
            proj.sound[atoi(a) & 3] = 0;
        else if (!strcmp(cmd, "jrtear"))         /* jrtear N: a power cut on the Nth journal program from now */
            jr_tear = atoi(a) - 1;
        else if (!strcmp(cmd, "reboot")) {
            memset(&proj, 0, sizeof proj);
            boot();
        } else if (!strcmp(cmd, "echo"))
            printf("%s", line + 5);
        else {
            printf("line %d: unknown command %s\n", lineno, cmd);
            return 2;
        }
    }
    wav_close();
    if (midi_log)
        fclose(midi_log);
    printf("host: %u ms simulated, %u blits, render %.1f%% of real time (host CPU)%s\n", now_ms, blits,
           render_budget_ns_total > 0 ? 100.0 * render_ns_total / render_budget_ns_total : 0.0,
           fails ? ", EXPECTATIONS FAILED" : "");
    return fails ? 1 : 0;
}
#endif /* OM_WEB */
