/* SPDX-License-Identifier: GPL-3.0-only */
/* ZEN-1's scene renderer: an 8-bit indexed framebuffer, a 256-color palette, and a blitter that turns it
 * into RGB565 strips on the way to the screen. Included by ui.c (after gfx.c), on the device and on the host.
 *
 *   surfaces   r8_fb (what is drawn each frame) and r8_bg (the background, drawn once and copied in at the start of
 *              a frame), both in pool RAM. A scene draws at 240 x 240 (painterly) or 120 x 120 (pixel art, doubled
 *              on the way out): r8_style picks.
 *   palette    r8_base[] (RGB888, what the scene sets) -> r8_pal[] (RGB565, byte-swapped as the panel takes it),
 *              through a tint (r8_pal_commit): dawn, dusk and night are a palette, not a redraw.
 *   blit       four 240 x 60 strips through the two halves of gfx.c's canvas, ping-pong: one strip is converted
 *              while the DMA sends the other. After each strip is converted, an overlay callback draws the UI on
 *              it in RGB565 with gfx.c's own text and shapes (anti-aliased over the scene), in screen coordinates.
 *
 * The hot loops take plain buffer pointers, so they can move to an -O2 unit if the device needs it. */

#define R8_W 240
#define R8_H 240
#define R8_STRIP 60                         /* rows per strip: 2 x 240 x 60 fits gfx.c's 240 x 144 canvas */
#define R8_NSTRIP (R8_H / R8_STRIP)

typedef struct {
    uint8_t *px;
    int32_t w, h;                           /* 240 x 240, or 120 x 120 in pixel art */
} surf_t;

static uint8_t r8_fb_px[R8_W * R8_H] OM_POOL;
static uint8_t r8_bg_px[R8_W * R8_H] OM_POOL;
static surf_t r8_fb = {r8_fb_px, R8_W, R8_H}, r8_bg = {r8_bg_px, R8_W, R8_H};
static uint32_t r8_base[256];               /* 0xRRGGBB */
static uint16_t r8_pal[4][256];             /* RGB565, byte-swapped; four of each: the color between two of
                                             * the panel's steps, each a quarter step on, shown in a 2 x 2 ordered
                                             * pattern (a pixel's x and y, odd or even), so a color that changes slowly
                                             * changes a quarter of its pixels at a time, evenly spread, not all at once
                                             * ("pops" in Hotaru's dark sky). (A 4 x 4 one, 8 KB, left the
                                             * pool too little room; with colors made in whole 8-bit levels, a green
                                             * level is a quarter step either way) */
static uint8_t r8_style;                    /* 0 painterly (240), 1 pixel art (120, doubled) */
static int32_t r8_fade = 256;               /* the whole palette's brightness, 0..256 (a fade through dark) */
static int32_t r8_sleep = 256;              /* and the sleep timer's dimming, 0..256 (ui.c amb_sleep_tick) */

static uint8_t r8_full = 1;                 /* everything to the screen at the next blit (r8_blit) */
static void r8_set_style(int pixel)
{
    int32_t n = pixel ? R8_W / 2 : R8_W;
    r8_full = 1;
    r8_style = (uint8_t)(pixel != 0);
    r8_fb.w = r8_fb.h = r8_bg.w = r8_bg.h = n;
}

/* ------------------------------------------------------------- palette --- */
static void r8_pal_set(int i, uint32_t rgb) { r8_base[i & 255] = rgb; }

/* two colors (0xRRGGBB), t 0..1 */
static uint32_t r8_mix(uint32_t a, uint32_t b, float t)
{
    uint32_t c = 0;
    int sh;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    for (sh = 0; sh < 24; sh += 8) {
        float x = (float)((a >> sh) & 255u), y = (float)((b >> sh) & 255u);
        c |= (uint32_t)(x + (y - x) * t + 0.5f) << sh;
    }
    return c;
}

/* n entries from i0, a -> b (both 0xRRGGBB) */
static void r8_pal_ramp(int i0, int n, uint32_t a, uint32_t b)
{
    int k;
    for (k = 0; k < n; k++) {
        int t = n > 1 ? k * 256 / (n - 1) : 0;
        uint32_t r = (((a >> 16) & 255u) * (uint32_t)(256 - t) + ((b >> 16) & 255u) * (uint32_t)t) >> 8,
                 g = (((a >> 8) & 255u) * (uint32_t)(256 - t) + ((b >> 8) & 255u) * (uint32_t)t) >> 8,
                 bl = ((a & 255u) * (uint32_t)(256 - t) + (b & 255u) * (uint32_t)t) >> 8;
        r8_base[(i0 + k) & 255] = r << 16 | g << 8 | bl;
    }
}

/* the FM-1's panel shows its darkest colors lighter than they are (Hotaru's night field
 * "washed out"). A scene may ask for them pressed down: r8_dark(pct) bends each channel v -> v * (v/255)^(pct/100),
 * which leaves white alone and darkens the darks the most. 0: off (the calibration screen measures the panel) */
/* 4 x 4 ordered dither thresholds, 0..15, row by row (the palette's patterns, r8_dith) */
static const uint8_t R8_BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static uint8_t r8_dark_lut[256], r8_dark_pct;
static int16_t r8_dark_scene, r8_dark_try = -1;    /* the scene's own curve; one to try instead (testing: GLO) */
static void r8_dark_set(int pct)
{
    int v;
    pct = pct < 0 ? 0 : pct > 200 ? 200 : pct;
    if (pct == r8_dark_pct && r8_dark_lut[255])
        return;
    r8_dark_pct = (uint8_t)pct;
    for (v = 0; v < 256; v++) {
        float u = (float)v * (1.0f / 255.0f);
        r8_dark_lut[v] = (uint8_t)((float)v * fm_exp2f((float)pct * 0.01f * fm_log2f(u > 1e-4f ? u : 1e-4f)) + 0.5f);
    }
}
/* a scene's curve. The calibration photo (the FM-1): the panel has almost no gamma: gray 96 looked
 * near white, 16..40 plain mid grays; a color wants roughly squaring (~100) to look as meant. Sakura was drawn on
 * the panel as it is (0) */
static void r8_dark(int pct)
{
    r8_dark_scene = (int16_t)pct;
    r8_dark_set(r8_dark_try >= 0 ? r8_dark_try : pct);
}

/* the palette the screen gets: each color multiplied by `mul` (0xRRGGBB, 255 = unchanged: night is a dark blue),
 * then mixed toward `wash` by amt/256 (a haze, a sunset glow), then the dark curve (r8_dark) */
static void r8_pal_commit_range(int i0, int n, uint32_t mul, uint32_t wash, int amt);
static void r8_pal_commit(uint32_t mul, uint32_t wash, int amt) { r8_pal_commit_range(0, 256, mul, wash, amt); }
/* a range of the palette committed on its own terms (what glows at night is not darkened with the rest) */
static void r8_pal_commit_range(int i0, int n, uint32_t mul, uint32_t wash, int amt)
{
    int i;
    uint32_t mr = (mul >> 16) & 255u, mg = (mul >> 8) & 255u, mb = mul & 255u;
    uint32_t wr = (wash >> 16) & 255u, wg = (wash >> 8) & 255u, wb = wash & 255u, a = (uint32_t)(amt < 0 ? 0 : amt > 256 ? 256 : amt);
    uint32_t f = (uint32_t)(r8_fade * r8_sleep) >> 8;
    for (i = i0; i < i0 + n && i < 256; i++) {
        uint32_t c = r8_base[i];
        uint32_t r = ((c >> 16) & 255u) * mr / 255u, g = ((c >> 8) & 255u) * mg / 255u, b = (c & 255u) * mb / 255u;
        r = ((r * (256u - a) + wr * a) >> 8) * f >> 8;
        g = ((g * (256u - a) + wg * a) >> 8) * f >> 8;
        b = ((b * (256u - a) + wb * a) >> 8) * f >> 8;
        if (r8_dark_pct) {
            r = r8_dark_lut[r > 255u ? 255u : r];
            g = r8_dark_lut[g > 255u ? 255u : g];
            b = r8_dark_lut[b > 255u ? 255u : b];
        }
        {
            int q;
            r = r > 255u ? 255u : r;
            g = g > 255u ? 255u : g;
            b = b > 255u ? 255u : b;
            for (q = 0; q < 4; q++) {                 /* (the 2 x 2 pattern, in eighths of a red or blue step, quarters
                                                       * of a green one) */
                static const uint8_t D[4] = {1, 5, 7, 3};
                uint32_t d = D[q], r5 = (r + d) >> 3, g6 = (2u * g + d) >> 3, b5 = (b + d) >> 3;
                r5 = r5 > 31u ? 31u : r5;
                g6 = g6 > 63u ? 63u : g6;
                b5 = b5 > 31u ? 31u : b5;
                r8_pal[q][i] = swap16((uint16_t)(r5 << 11 | g6 << 5 | b5));
            }
        }
    }
}

/* ---------------------------------------------------------- primitives --- */
/* 4 x 4 ordered dither: a pixel at (x, y) shows the upper color when t (0..16) is above its threshold */
static inline int r8_dith(int32_t x, int32_t y, int t) { return t > R8_BAYER[(y & 3) * 4 + (x & 3)]; }

static void r8_copy(surf_t *dst, const surf_t *src)
{
    uint32_t n = (uint32_t)(dst->w * dst->h), i;
    const uint32_t *s = (const uint32_t *)src->px;
    uint32_t *d = (uint32_t *)dst->px;
    for (i = 0; i < n / 4u; i++)
        d[i] = s[i];
}

static void r8_rect(surf_t *s, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c)
{
    int32_t x1 = x + w, y1 = y + h, i;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x1 > s->w)
        x1 = s->w;
    if (y1 > s->h)
        y1 = s->h;
    for (; y < y1; y++) {
        uint8_t *p = s->px + y * s->w;
        for (i = x; i < x1; i++)
            p[i] = c;
    }
}

static inline void r8_hspan(surf_t *s, int32_t y, int32_t x0, int32_t x1, uint8_t c)   /* x0 .. x1 inclusive */
{
    uint8_t *p;
    if ((uint32_t)y >= (uint32_t)s->h)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 >= s->w)
        x1 = s->w - 1;
    p = s->px + y * s->w;
    for (; x0 <= x1; x0++)
        p[x0] = c;
}

static inline void r8_pset(surf_t *s, int32_t x, int32_t y, uint8_t c)
{
    if ((uint32_t)x < (uint32_t)s->w && (uint32_t)y < (uint32_t)s->h)
        s->px[y * s->w + x] = c;
}

/* a vertical gradient over rows y0..y1-1 through palette entries c0 .. c0+n-1, dithered between steps */
static void r8_vgrad(surf_t *s, int32_t y0, int32_t y1, uint8_t c0, int n)
{
    int32_t y, x;
    if (y1 <= y0 || n < 1)
        return;
    for (y = y0 < 0 ? 0 : y0; y < y1 && y < s->h; y++) {
        int32_t f = (y - y0) * (n - 1) * 16 / (y1 - y0);   /* 1/16 steps */
        uint8_t *p = s->px + y * s->w;
        for (x = 0; x < s->w; x++)
            p[x] = (uint8_t)(c0 + (f >> 4) + r8_dith(x, y, f & 15));
    }
}

/* a vertical gradient dithered across `spread` entries, not one: a dark sky's neighboring entries are often the same
 * color on the panel (RGB565 has 32 steps a channel, and a night blue uses a handful), so a one-entry dither leaves
 * hard bands where the color does change; spread over several, each change melts across many rows */
static void r8_vgrad_wide(surf_t *s, int32_t y0, int32_t y1, uint8_t c0, int n, int spread)
{
    int32_t y, x;
    if (y1 <= y0 || n < 1)
        return;
    for (y = y0 < 0 ? 0 : y0; y < y1 && y < s->h; y++) {
        int32_t f = (y - y0) * (n - 1) * 16 / (y1 - y0);   /* 1/16 steps */
        uint8_t *p = s->px + y * s->w;
        for (x = 0; x < s->w; x++) {
            int32_t g = f + ((int32_t)R8_BAYER[(y & 3) * 4 + (x & 3)] * 2 - 15) * spread / 2;   /* +-spread/2 entries */
            g = g < 0 ? 0 : g > (n - 1) * 16 ? (n - 1) * 16 : g;
            p[x] = (uint8_t)(c0 + (g >> 4) + r8_dith(x + 1, y + 2, g & 15));
        }
    }
}

/* a filled disc, radius r (float: a soft-moving shape lands between pixels by its center) */
static void r8_disc(surf_t *s, float cx, float cy, float r, uint8_t c)
{
    int32_t y, y0 = (int32_t)fm_floorf(cy - r), y1 = (int32_t)fm_floorf(cy + r);
    float r2 = r * r;
    for (y = y0; y <= y1; y++) {
        float dy = (float)y + 0.5f - cy, w2 = r2 - dy * dy, w;
        if (w2 <= 0.0f)
            continue;
        w = fm_sqrtf(w2);
        r8_hspan(s, y, (int32_t)fm_floorf(cx - w + 0.5f), (int32_t)fm_floorf(cx + w - 0.5f), c);
    }
}

/* a disc whose edge is dithered toward the background color bg (a soft edge in painterly style) */
static void r8_disc_soft(surf_t *s, float cx, float cy, float r, uint8_t c, uint8_t edge)
{
    int32_t y, x, y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)fm_floorf(cy + r + 1.0f);
    for (y = y0; y <= y1; y++) {
        float dy = (float)y + 0.5f - cy;
        int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)fm_floorf(cx + r + 1.0f);
        if ((uint32_t)y >= (uint32_t)s->h)
            continue;
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, d = fm_sqrtf(dx * dx + dy * dy) - r;
            if (d < -1.0f)
                r8_pset(s, x, y, c);
            else if (d < 1.0f)
                r8_pset(s, x, y, r8_dith(x, y, (int)((1.0f - d) * 8.0f)) ? c : edge);
        }
    }
}

/* a filled triangle (scanline, the top-left fill rule): the petals and the koi's fins are made of these */
static void r8_tri(surf_t *s, float ax, float ay, float bx, float by, float cx, float cy, uint8_t c)
{
    float t;
    int32_t y, y0, y1;
    if (ay > by) { t = ax; ax = bx; bx = t; t = ay; ay = by; by = t; }
    if (ay > cy) { t = ax; ax = cx; cx = t; t = ay; ay = cy; cy = t; }
    if (by > cy) { t = bx; bx = cx; cx = t; t = by; by = cy; cy = t; }
    if (cy - ay < 1e-4f)
        return;
    y0 = (int32_t)fm_floorf(ay + 0.5f);
    y1 = (int32_t)fm_floorf(cy + 0.5f);
    if (y0 < 0)
        y0 = 0;
    if (y1 > s->h)
        y1 = s->h;
    for (y = y0; y < y1; y++) {
        float py = (float)y + 0.5f, xl = ax + (cx - ax) * (py - ay) / (cy - ay), xr;
        if (py < by)
            xr = by - ay > 1e-4f ? ax + (bx - ax) * (py - ay) / (by - ay) : bx;
        else
            xr = cy - by > 1e-4f ? bx + (cx - bx) * (py - by) / (cy - by) : bx;
        if (xl > xr) {
            t = xl;
            xl = xr;
            xr = t;
        }
        r8_hspan(s, y, (int32_t)fm_floorf(xl + 0.5f), (int32_t)fm_floorf(xr - 0.5f), c);
    }
}

/* a quad rotated by angle a (turns) around (cx, cy), half-sizes hw, hh: a petal, a leaf */
static void r8_quad(surf_t *s, float cx, float cy, float hw, float hh, float a, uint8_t c)
{
    float sn = fm_sin_turns(a), cs = fm_sin_turns(a + 0.25f);
    float ux = cs * hw, uy = sn * hw, vx = -sn * hh, vy = cs * hh;
    r8_tri(s, cx - ux - vx, cy - uy - vy, cx + ux - vx, cy + uy - vy, cx + ux + vx, cy + uy + vy, c);
    r8_tri(s, cx - ux - vx, cy - uy - vy, cx + ux + vx, cy + uy + vy, cx - ux + vx, cy - uy + vy, c);
}

/* an ellipse outline (a ripple seen at a low angle): points around it, close enough that the line is unbroken (a wide ring was dotted) */
static void r8_ring(surf_t *s, float cx, float cy, float rx, float ry, uint8_t c)
{
    int32_t i, n = 8 + (int32_t)(fm_maxf(rx, ry) * 7.0f);
    if (n > 360)
        n = 360;
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        r8_pset(s, (int32_t)fm_floorf(cx + rx * fm_sin_turns(t + 0.25f)), (int32_t)fm_floorf(cy + ry * fm_sin_turns(t)), c);
    }
}

/* a tapered segment from (x0, y0) width w0 to (x1, y1) width w1: a branch */
static void r8_limb(surf_t *s, float x0, float y0, float w0, float x1, float y1, float w1, uint8_t c)
{
    float dx = x1 - x0, dy = y1 - y0, l = fm_sqrtf(dx * dx + dy * dy), nx, ny;
    if (l < 1e-3f)
        return;
    nx = -dy / l * 0.5f;
    ny = dx / l * 0.5f;
    r8_tri(s, x0 + nx * w0, y0 + ny * w0, x1 + nx * w1, y1 + ny * w1, x1 - nx * w1, y1 - ny * w1, c);
    r8_tri(s, x0 + nx * w0, y0 + ny * w0, x1 - nx * w1, y1 - ny * w1, x0 - nx * w0, y0 - ny * w0, c);
    if (w1 > 1.5f)
        r8_disc(s, x1, y1, w1 * 0.5f, c);       /* a round joint for the next limb */
}

/* ---------------------------------------------------------------- blit --- */
/* only what changed goes to the screen (after Melodee's gfx.c "diff", GPL-3.0-only): the SPI was ~20 ms a
 * frame, the whole screen, more than any scene's drawing. Every 8 x 4 block is hashed (32 bits) against what was last
 * sent there; runs of dirty bands x runs of dirty blocks across them are the rectangles sent. A full-width one goes
 * straight from the strip; a narrower one through one of two slots in the canvas past the strips.
 *
 * only what changed is converted, too (photos: B 13-17 ms with a third sent; the palette's lookup
 * of every pixel and the hashing of every block were most of it). While the palette is as it was, a strip's blocks
 * are hashed in the frame's own 8-bit pixels (r8_sumf), and only the bands to be sent are converted. A strip whose
 * colors can't be known from its pixels alone, converts and hashes its colors whole (r8_sum), as before: when
 * the palette changed (the day, a fade), when the UI is drawn over it (the HUD, a message, the knobs' bar), or was the
 * frame before. Each table is kept true of what the screen shows, whichever way a strip went.
 *
 * And the buffers (black bands seen on the FM-1): a transfer goes on while the next strip is made, so before any
 * buffer is written (a half, a slot) the transfer reading it, if any, is waited for (r8_fly). Counting on the
 * halves taking turns is not enough: a strip with nothing to send breaks it.
 *
 * Everything goes whole when asked (r8_full: a scene's start, a style change, back from the instrument), and one 4-row
 * band a frame in turn regardless, so a change a hash missed is put right within 60 frames (~1.5 s) */
#define R8_BAND 4
#define R8_BLK 8
#define R8_NBAND (R8_H / R8_BAND)
#define R8_NBLK (R8_W / R8_BLK)
#define R8_SBAND (R8_STRIP / R8_BAND)
#define R8_NRUN 12                          /* runs of blocks sent a band at most (more: the last one reaches on) */
#define R8_SLOT 2880                        /* px a slot: 2 of them in cv_px past the two strips (240 x 24 there) */
static uint32_t r8_sum[R8_NBAND * R8_NBLK] OM_POOL;    /* the colors sent, by block */
static uint32_t r8_sumf[R8_NBAND * R8_NBLK] OM_POOL;   /* the 8-bit pixels they came from (the palette unchanged) */
static uint8_t r8_roll, r8_slot, r8_ovl;    /* (r8_ovl: the strips the UI was over, the frame before: a bit each) */
static int8_t r8_fly = -1;                  /* the buffer a transfer may still be reading: 0, 1 a half, 2, 3 a slot */
static uint32_t r8_palsum;
static uint32_t r8_sent;                    /* pixels sent, the last frame (the HUD's share) */
#ifdef OM_HOST
static uint32_t r8_stale;                   /* strips whose screen differed from what was drawn (the host's tests) */
#endif

static void r8_mine(int id)                 /* about to write buffer id: the transfer reading it done first */
{
    if (r8_fly == id) {
        lcd_sync();
        r8_fly = -1;
    }
}

static uint32_t r8_mix32(uint32_t h, uint32_t v)
{
    h = (h ^ v) * 2654435761u;
    return h ^ (h >> 15);                   /* (high bits back down: a multiply alone only carries them up, and the same
                                             * small change in several pixels could cancel) */
}
static uint32_t r8_hash(const uint16_t *p)  /* an 8 x 4 block of a strip's colors */
{
    const uint32_t *q = (const uint32_t *)p;
    uint32_t h = 2166136261u, r, i;
    for (r = 0; r < R8_BAND; r++, q += R8_W / 2)
        for (i = 0; i < R8_BLK / 2; i++)
            h = r8_mix32(h, q[i]);
    return h;
}
/* an 8 x 4 block of the screen in the frame's own pixels (pixel art: 4 x 2 of them, each shown twice) */
static uint32_t r8_hashf(int32_t sx, int32_t sy)
{
    uint32_t h = 2166136261u;
    int32_t r;
    if (r8_style) {
        const uint8_t *p = r8_fb.px + (sy >> 1) * (R8_W / 2) + (sx >> 1);
        for (r = 0; r < R8_BAND / 2; r++, p += R8_W / 2)
            h = r8_mix32(h, (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
    } else {
        const uint8_t *p = r8_fb.px + sy * R8_W + sx;
        for (r = 0; r < R8_BAND; r++, p += R8_W) {
            const uint32_t *q = (const uint32_t *)p;
            h = r8_mix32(r8_mix32(h, q[0]), q[1]);
        }
    }
    return h;
}

/* rows [r0, r1) of a strip (sy), through the palette, into its half (dst) */
static void r8_convert_rows(uint16_t *dst, int32_t sy, int32_t r0, int32_t r1)
{
    int32_t r, x;
    for (r = r0; r < r1; r++) {
        uint16_t *d = dst + r * R8_W;
        const uint16_t *pe = r8_pal[(sy + r) & 1], *po = r8_pal[2 + ((sy + r) & 1)];   /* (this row's two of the four,
                                                       * x even, odd: the pattern turned a quarter from the scenes' own
                                                       * dithers, r8_dith, so the two don't line up into stripes) */
        if (r8_style) {
            const uint8_t *p = r8_fb.px + ((sy + r) >> 1) * (R8_W / 2);
            for (x = 0; x < R8_W / 2; x++) {
                d[2 * x] = pe[p[x]];
                d[2 * x + 1] = po[p[x]];
            }
        } else {
            const uint8_t *p = r8_fb.px + (sy + r) * R8_W;
            for (x = 0; x < R8_W; x += 2) {
                d[x] = pe[p[x]];
                d[x + 1] = po[p[x + 1]];
            }
        }
    }
}

static void r8_send(const uint16_t *strip, int32_t sy, int32_t x, int32_t y, int32_t w, int32_t h, int half)
{
    const uint16_t *src = strip + (y - sy) * R8_W + x;
    int32_t r, i, rows;
    r8_sent += (uint32_t)(w * h);
    if (w == R8_W) {
        lcd_blit(0, (uint32_t)y, R8_W, (uint32_t)h, src);
        r8_fly = (int8_t)half;
        return;
    }
    rows = R8_SLOT / w;
    while (h > 0) {
        int32_t n = h < rows ? h : rows;
        uint16_t *slot = cv_px + 2 * R8_STRIP * R8_W + r8_slot * R8_SLOT;
        r8_mine(2 + r8_slot);
        for (r = 0; r < n; r++)
            for (i = 0; i < w; i++)
                slot[r * w + i] = src[r * R8_W + i];
        lcd_blit((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)n, slot);
        r8_fly = (int8_t)(2 + r8_slot);
        r8_slot ^= 1u;
        src += n * R8_W;
        y += n;
        h -= n;
    }
}

/* the frame to the screen. overlay(sy, h) draws the UI on each strip with gfx.c's cv_* calls, in screen
 * coordinates (clipped to the strip); over(sy, h): whether it draws anything there now. Returns with the last
 * transfer still running (r8_fly; lcd_sync waits for it) */
static void r8_blit(void (*overlay)(int32_t sy, int32_t h), int (*over)(int32_t sy, int32_t h))
{
    int32_t k;
    uint32_t ps = 2166136261u, i, ovl = 0;
    int palnew;
    for (i = 0; i < 512; i++)                       /* the palette as it is now (it changes with the day, the fades) */
        ps = r8_mix32(ps, ((const uint32_t *)r8_pal)[i]);
    palnew = ps != r8_palsum;
    r8_palsum = ps;
    r8_sent = 0;
    for (k = 0; k < R8_NSTRIP; k++) {
        int32_t sy = k * R8_STRIP, hb = (k & 1) * R8_STRIP, b, b0, c;
        uint16_t *half = cv_px + hb * R8_W;
        uint32_t mask[R8_SBAND], conv = 0;      /* (conv: the bands converted, a bit each) */
        int whole = r8_full, roll = (int32_t)(r8_roll % R8_NBAND) - sy / R8_BAND, ui = over && over(sy, R8_STRIP),
            slow = whole || palnew || ui || (r8_ovl >> k & 1u);   /* (its colors hashed whole: the older way) */
#ifdef R8_TEST_WHOLE
        whole = slow = 1;                    /* (the host's check: everything sent, to compare with) */
#endif
        if (ui)
            ovl |= 1u << k;
        r8_mine((int)(k & 1));                /* (the half: free of the transfer still reading it) */
        if (slow) {
            r8_convert_rows(half, sy, 0, R8_STRIP);
            conv = (1u << R8_SBAND) - 1u;
            cv_w = R8_W;                     /* gfx.c's canvas, set to this strip (cv_begin would clear it) */
            cv_h = 2 * R8_STRIP;
            cv_oy = hb - sy;
            cv_y0 = hb;
            cv_y1 = hb + R8_STRIP;
            if (overlay)
                overlay(sy, R8_STRIP);
        }
        for (b = 0; b < R8_SBAND; b++) {     /* the blocks changed since they were last sent */
            uint32_t *sum = r8_sum + (sy / R8_BAND + b) * R8_NBLK, *sumf = r8_sumf + (sy / R8_BAND + b) * R8_NBLK;
            mask[b] = 0;
            for (c = 0; c < R8_NBLK; c++) {
                uint32_t hf = r8_hashf(c * R8_BLK, sy + b * R8_BAND);
                if (slow) {
                    uint32_t h = r8_hash(half + b * R8_BAND * R8_W + c * R8_BLK);
                    if (h != sum[c] || whole || b == roll) {
                        sum[c] = h;
                        mask[b] |= 1u << c;
                    }
                } else if (hf != sumf[c] || b == roll) {
                    mask[b] |= 1u << c;
                }
                sumf[c] = hf;
            }
        }
        if (whole) {
            r8_sent += R8_W * R8_STRIP;
            lcd_blit(0, (uint32_t)sy, R8_W, R8_STRIP, half);
            r8_fly = (int8_t)(k & 1);
            continue;
        }
        for (b = 0; b < R8_SBAND;) {          /* runs of dirty bands: joined only where they changed in the same
                                               * blocks (a clean band between joined, and every band's blocks
                                               * sent across all of them, made scattered rain on Hotaru ~60 % of the
                                               * screen a frame, when ~8 % had changed) */
            uint32_t cols = 0;
            int32_t b1, run, c0[R8_NRUN], c1[R8_NRUN], n = 0;
            if (!mask[b]) {
                b++;
                continue;
            }
            b0 = b;
            b1 = b;
            while (b + 1 < R8_SBAND && mask[b + 1] == mask[b0]) {
                b++;
                b1 = b;
            }
            for (run = b0; run <= b1; run++)
                cols |= mask[run];
            if (!slow) {                      /* (the run's rows, converted now: only what is sent) */
                r8_convert_rows(half, sy, b0 * R8_BAND, (b1 + 1) * R8_BAND);
                for (run = b0; run <= b1; run++) {
                    uint32_t *sum = r8_sum + (sy / R8_BAND + run) * R8_NBLK;
                    conv |= 1u << run;
                    for (c = 0; c < R8_NBLK; c++)   /* (the colors sent: their hashes true for the next slow strip) */
                        sum[c] = r8_hash(half + run * R8_BAND * R8_W + c * R8_BLK);
                }
            }
            for (c = 0; c < R8_NBLK;) {          /* its runs of dirty blocks (one clean between joined, where
                                                  * sending it costs about what another send's setting up does),
                                                  * R8_NRUN at most */
                int32_t e;
                if (!(cols >> c & 1u)) {
                    c++;
                    continue;
                }
                e = c;
                while (e + 1 < R8_NBLK && ((cols >> (e + 1) & 1u) || (e + 2 < R8_NBLK && (cols >> (e + 2) & 1u))))
                    e++;
                while (!(cols >> e & 1u))
                    e--;
                if (n == R8_NRUN) {
                    c1[R8_NRUN - 1] = e;         /* (more: the last run reaches to here) */
                } else {
                    c0[n] = c;
                    c1[n] = e;
                    n++;
                }
                c = e + 1;
            }
            for (run = 0; run < n; run++)
                r8_send(half, sy, c0[run] * R8_BLK, sy + b0 * R8_BAND, (c1[run] - c0[run] + 1) * R8_BLK,
                        (b1 - b0 + 1) * R8_BAND, (int)(k & 1));
            b = b1 + 1;
        }
#ifdef OM_HOST
        {   /* the host's check: the screen as sent (once it has arrived) is the frame as drawn, every band (expect
             * stale); what wasn't converted this frame is compared as it would have been */
            static uint16_t want[R8_W * R8_STRIP];
            int32_t j;
            memcpy(want, half, sizeof want);
            for (b = 0; b < R8_SBAND; b++)
                if (!(conv >> b & 1u))
                    r8_convert_rows(want, sy, b * R8_BAND, (b + 1) * R8_BAND);
            for (j = 0; j < R8_W * R8_STRIP; j++)
                if (lcd_view_host((uint32_t)(sy * R8_W + j)) != want[j]) {
                    r8_stale++;
                    break;
                }
        }
#endif
    }
    r8_ovl = (uint8_t)ovl;
    r8_full = 0;
    r8_roll++;
}
