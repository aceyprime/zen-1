/* SPDX-License-Identifier: GPL-3.0-only */
/* FuMi-1 platform on the FM-1 (FoMni's): plat.h over Felucca's HAL, panel map, USB rings and storage
 * (and on the SMK-37, over hal/smk_input.h).
 * Part of the unity build (omni.c), after the HAL, usb.c, storage.c and panel.c. */

uint32_t plat_ms(void) { return fm1_ms; }

uint32_t plat_buttons(void)
{
    uint32_t raw = fm1_in.buttons, out = 0, i;
    for (i = 0; i < NB; i++)
        if (raw & (1u << panel.btn[i]))
            out |= 1u << i;
    return out;
}

#if BOARD_SMK37
/* the keybed's F3..G5, its outer keys C3..E3 and G#5..C6 an octave in (the same keys of the FM-1's 27), and the pads
 * as the white keys (the HAL keeps the key each pad pressed); a pad tapped and let go between two calls counts as held
 * for one (its latched press) */
uint32_t plat_keys(void)
{
    uint32_t k = fm1__lock();                    /* (64 bits, written by the scan ISR) */
    uint64_t kb = fm1_in.keys;
    uint32_t n = fm1_in.knotes | fm1_in.pnotes;
    fm1__unlock(k);
    n |= smk_pad_edges();
    n |= (uint32_t)(kb & 0x1Fu) << 7;            /* C3..E3 -> C4..E4 (keys 7..11) */
    n |= (uint32_t)(kb >> 32 & 0x1Fu) << 15;     /* G#5..C6 -> G#4..C5 (keys 15..19) */
    return n & ((1u << NKEYS) - 1u);
}
#else
uint32_t plat_keys(void) { return fm1_in.notes & ((1u << NKEYS) - 1u); }
#endif

int32_t plat_enc(int role)
{
    if (role < 0 || role >= NE)
        return 0;
    return fm1_enc_take(panel.enc[role]) * panel.dir[role];
}

static volatile uint32_t master_q12 = 2048;
uint32_t plat_master(void) { return master_q12; }
static volatile int32_t batt_raw16 = -1;            /* the battery's divider (ADC ch3), x16, smoothed; -1 not yet read */
int32_t plat_battery_raw(void) { return batt_raw16 < 0 ? -1 : (batt_raw16 + 8) / 16; }

#if BOARD_SMK37
/* ---- the SMK-37 build: hal/smk_input.h. Button LEDs: one flag per button id, no dim level (no glow, no dim set);
 * the keys have no LEDs: the pads light in colour (ui.c over plat_pad_rgb, lcd.c padled_*) */
static void led_pos_init(void) {}
void plat_glow(int on) { (void)on; }
void plat_leds_dim(uint32_t buttons, uint32_t keys) { (void)buttons; (void)keys; }
void plat_play_red(int red) { (void)red; }      /* SEQ PLAY has one LED */
void plat_leds(uint32_t buttons, uint32_t keys)
{
    uint8_t nl[SMK_NLED] = {0};
    uint32_t i;
    (void)keys;
    for (i = 0; i < NB; i++)
        if ((buttons >> i) & 1u)
            nl[panel.btn[i]] = 1;
    for (i = 0; i < SMK_NLED; i++)
        fm1_led[i] = nl[i];
}

uint32_t plat_smk_buttons(void)
{
    return (fm1_in.buttons >> SMK_BTN_KNOB_BANK & 1u) << SB_KNOB_BANK |
           (fm1_in.buttons >> SMK_BTN_FADER_BANK & 1u) << SB_FADER_BANK |
           (fm1_in.buttons >> SMK_BTN_LEFT & 1u) << SB_LEFT | (fm1_in.buttons >> SMK_BTN_RIGHT & 1u) << SB_RIGHT;
}
int plat_fader(int i) { return fm1_in.frames < 200u || (uint32_t)i > 3u ? -1 : (int)fm1_in.fader[i]; }
void plat_pad_rgb(int pad, uint8_t r, uint8_t g, uint8_t b)
{
    if ((uint32_t)pad < SMK_NPAD)
        padled_set((uint32_t)pad, r, g, b);
}

/* the wheels (mux C address 4 = pitch, 5 = mod), self-calibrating: the pitch wheel's rest is its centre (taken once
 * the readings settle, followed slowly while it rests), each wheel's range grows to what it has reached (the travel
 * is short: raw ~445..640 pitch, ~474..629 mod on a unit). Pitch: -127..127 with a dead band around the centre; mod:
 * 0..127, up (lower raw) = more. */
#define SMK_PITCH_MAX 127
int plat_wheels(int32_t *pitch, int32_t *modw)
{
    enum { DEAD = 12, SPAN0 = 80, MOD_LO = 485, MOD_HI = 615 };
    static int32_t pc, plo, phi, mlo, mhi, rest, last_b = -1, last_m = -1;
    int32_t p = fm1_in.wheel[0], m = fm1_in.wheel[1], d, b, mw;
    if (fm1_in.frames < 300u)                    /* (the readings settle) */
        return 0;
    if (!pc) {
        pc = p;
        plo = p - SPAN0;
        phi = p + SPAN0;
        mlo = m < MOD_LO ? m : MOD_LO;
        mhi = m > MOD_HI ? m : MOD_HI;
    }
    if (p < plo)
        plo = p;
    if (p > phi)
        phi = p;
    if (m < mlo)
        mlo = m;
    if (m > mhi)
        mhi = m;
    d = p - pc;
    if (d > -DEAD && d < DEAD) {
        b = 0;
        if (++rest > 200) {                      /* resting: the centre follows a slow drift */
            pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            rest = 0;
        }
    } else {
        rest = 0;
        b = d > 0 ? (d - DEAD) * SMK_PITCH_MAX / (phi - pc - DEAD > 1 ? phi - pc - DEAD : 1)
                  : (d + DEAD) * SMK_PITCH_MAX / (pc - plo - DEAD > 1 ? pc - plo - DEAD : 1);
        b = clamp(b, -SMK_PITCH_MAX, SMK_PITCH_MAX);
    }
    mw = mhi - m <= DEAD ? 0 : (mhi - m - DEAD) * 127 / (mhi - mlo - DEAD > 1 ? mhi - mlo - DEAD : 1);
    mw = clamp(mw, 0, 127);
    if (b == last_b && mw == last_m)
        return 0;
    last_b = b;
    last_m = mw;
    *pitch = b;
    *modw = mw;
    return 1;
}

/* the factory velocity curves of the keys and pads: USRTRIM (flash 0xC1000, "trimdat" at +0x224), read only,
 * over the SPI read (no XIP window needed), into a buffer on the stack (once, at power-on); smk_trim_load
 * ignores a record that does not check out */
static void smk_trim_init(void)
{
    uint8_t buf[0x22Cu] __attribute__((aligned(4)));
    if (flash_ok && st_read(SMK_TRIM_FLASH, buf, sizeof buf) == 0)
        smk_trim_load(buf);
}
#else
/* LEDs: the picture is built off-line and copied one byte per column (Felucca: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker) */
static uint8_t led_pos[FM1_NKEY];
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < FM1_NKEY; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
/* Glow: every button not lit is lit dim, on one scan frame in GLOW_DIV (the HAL's fm1_led_dim) */
#define GLOW_DIV 4u
static uint8_t glow_on = 1, dim_set;
static uint32_t dim_b, dim_k;
void plat_glow(int on)
{
    glow_on = (uint8_t)(on != 0);
    dim_set = 0;
}
void plat_leds_dim(uint32_t buttons, uint32_t keys)
{
    dim_b = buttons;
    dim_k = keys;
    dim_set = 1;
}
/* PLAY has two LEDs: its red at the button's own position, a green at the keyless matrix position
 * column 8, row PA5 (found on the device). Green, unless recording. */
static uint8_t play_red;
void plat_play_red(int red) { play_red = (uint8_t)(red != 0); }
void plat_leds(uint32_t buttons, uint32_t keys)
{
    uint8_t nl[FM1_NCOL] = {0}, dl[FM1_NCOL] = {0};
    uint32_t i, c;
    for (i = 0; i < NB + NKEYS; i++) {
        uint32_t id = i < NB ? panel.btn[i] : 14u + (i - NB);
        int on = i < NB ? (buttons >> i) & 1u : (keys >> (i - NB)) & 1u;
        uint8_t q = led_pos[id];
        if (i == B_PLAY && !play_red)            /* PLAY's green LED instead (keyless position col 8, row PA5) */
            q = (uint8_t)((8u << 3) | 1u);
        if (q == 0xFF)
            continue;
        if (on)
            nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
        else if (dim_set ? ((i < NB ? dim_b >> i : dim_k >> (i - NB)) & 1u) : (i < NB && glow_on))
            dl[q >> 3] |= (uint8_t)(1u << (q & 7u));
    }
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led[c] = nl[c];
        fm1_led_dim[c] = dl[c];
    }
    fm1_led_dim_div = glow_on || dim_set ? GLOW_DIV : 0u;
}

#endif

/* MIDI: Felucca's rings carry 4-byte USB-MIDI event packets, byte 0 = cable / CIN */
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = midi_in_q[mi_r % MQ];
    mi_r++;
    return 1;
}
void plat_midi_out(uint32_t pkt) { midi_out_event(pkt); }
/* the engine's MIDI out (from the render): a USB-MIDI packet, cable 0, CIN = the status's high nibble */
void fm_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return flash_ok ? st_load(obj, dst, max) : -1; }
/* the LEDs dark across a flash write. The scan lights one column a tick, and a write holds the interrupts
 * off for ~1 ms (an erase ~0.4 s): the column lit last stayed lit all that while, and in a dim frame that was every dim
 * key of it at full brightness. A save in the journal, a piece a frame, went across the keys as a flicker. Now the
 * lines go dark first: a write shows as a dark millisecond, which can't be seen. A long one (the panel's, an erase)
 * clears the picture, so the scan keeps them dark; leds() puts it back next frame. */
#if BOARD_SMK37
static void led_hush(void)                         /* (the SMK's button LEDs: no dim frame to catch; dark for an erase) */
{
    uint32_t c, f = irq_save();
    for (c = 0; c < SMK_NLED; c++)
        fm1_led[c] = 0;
    irq_restore(f);
}
#define fm1__led_lines(m) ((void)(m))
#else
static void led_hush(void)
{
    uint32_t c, f = irq_save();
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = fm1_led_dim[c] = 0;
    fm1__led_lines(0);
    irq_restore(f);
}
#endif

int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (!flash_ok)
        return -9;
    led_hush();
    return st_save(obj, src, len);
}

/* the save journal (plat.h): the platform's own range-checked flash access, inside its 8 sectors only (PLAT_JR_BASE) */
static uint32_t jr_prog_us_max;
static uint32_t audio_halves(void);                /* main_fm1.c: audio halves rendered, the last one's time */
static uint32_t audio_last_us(void);
#if BOARD_SMK37
_Static_assert(PLAT_JR_BASE >= ST_SMK_BASE + 2u * OBJ_NOBJ * ST_SECTOR &&
               PLAT_JR_BASE + PLAT_JR_SECTOR * PLAT_JR_NSECTOR <= ST_SMK_OBJ_HI,
               "SMK: the journal must lie in the store, after the objects, below the owner tag");
#endif
static int jr_in(uint32_t off, uint32_t n) { return off < PLAT_JR_SECTOR * PLAT_JR_NSECTOR && n <= PLAT_JR_SECTOR * PLAT_JR_NSECTOR - off; }
int plat_jr_read(uint32_t off, void *dst, uint32_t n)
{
    if (!flash_ok || !jr_in(off, n))
        return -9;
    return st_read(PLAT_JR_BASE + off, dst, n);
}
int plat_jr_prog(uint32_t off, const void *src, uint32_t n)
{
    uint32_t t0, us, h0;
    int rc;
    if (!flash_ok || !jr_in(off, n) || n == 0 || n > 256u || (off & 0xFFu) + n > 256u)
        return -9;
    h0 = audio_halves();
    t0 = plat_us();
    {                                              /* dark lines through it, no tick between (led_hush) */
        uint32_t f = irq_save();
        fm1__led_lines(0);
        rc = st_prog(PLAT_JR_BASE + off, src, n);
        irq_restore(f);
    }
    us = plat_us() - t0;
    if (audio_halves() != h0)                      /* the render the program held back ran inside the timing: */
        us = us > audio_last_us() ? us - audio_last_us() : 0u;   /* the interrupts were off only for the rest */
    if (us > jr_prog_us_max)
        jr_prog_us_max = us;
    return rc;
}
int plat_jr_erase(uint32_t sector)
{
    int rc;
    if (!flash_ok || sector >= PLAT_JR_NSECTOR)
        return -9;
    fm1_wdt_feed();                                    /* an erase can take ~0.4 s with the interrupts off */
    led_hush();
    rc = st_erase(PLAT_JR_BASE + sector * PLAT_JR_SECTOR);
    fm1_wdt_feed();
    return rc;
}
uint32_t plat_jr_prog_us_max(void) { return jr_prog_us_max; }

static volatile uint32_t audio_cpu_pct, audio_xruns;
uint32_t plat_cpu_pct(void) { return audio_cpu_pct; }
uint32_t plat_xruns(void) { return audio_xruns; }
uint32_t plat_us(void)                             /* main loop only: the 24 MHz ticks, carried across their wrap */
{
    static uint32_t last, us, rem;
    uint32_t t = fm1_ticks();
    rem += t - last;
    last = t;
    us += rem / FM1_TICKS_PER_US;
    rem %= FM1_TICKS_PER_US;
    return us;
}
