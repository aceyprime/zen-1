/* SPDX-License-Identifier: GPL-3.0-only */
/* FuMi-1 platform on the FM-1 (FoMni's): plat.h over Felucca's HAL, panel map, USB rings and storage.
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

uint32_t plat_keys(void) { return fm1_in.notes & ((1u << NKEYS) - 1u); }

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
static void led_hush(void)
{
    uint32_t c, f = irq_save();
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = fm1_led_dim[c] = 0;
    fm1__led_lines(0);
    irq_restore(f);
}

int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (!flash_ok)
        return -9;
    led_hush();
    return st_save(obj, src, len);
}

/* the save journal (plat.h): the platform's own range-checked flash access, inside 0xC0000..0xC7FFF only */
static uint32_t jr_prog_us_max;
static uint32_t audio_halves(void);                /* main_fm1.c: audio halves rendered, the last one's time */
static uint32_t audio_last_us(void);
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
