/* SPDX-License-Identifier: GPL-3.0-only */
/* FuMi-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the defaults;
 * every value is range-checked (a firmware must never crash on its own saved data). Part of the unity
 * build, after app.h.
 *
 * ZEN-1 adds a save journal (plat.h PLAT_JR_*): saving to OBJ_PROJ erases a flash sector, which stops the
 * interrupts (the audio and the key lights) for up to ~0.4 s, and a scene is never silent. So a save appends a
 * record to journal sectors erased at power-on instead (a few page programs, interrupts on in between), and
 * OBJ_PROJ is only the fallback when the journal has no erased slot left.
 *
 *   slot     512 bytes, 64 of them in the 8 sectors: the project from offset 0, then the commit record (magic,
 *            sequence, length, CRC) in the slot's last 16 bytes, written last. A record is valid only with its
 *            magic, a sane length and its CRC, and only if the project then passes project_valid().
 *   writing  only into a slot read back as all 0xFF first (NOR programming only clears bits; the data area may
 *            hold leftovers), page by page, then read back and checked. A failure skips the slot.
 *   boot     the newest valid record (by the project's save_seq) against OBJ_PROJ: the higher save_seq wins.
 *            Then the write position: just after the newest record if the rest of its sector is erased, else the
 *            next sector; up to JR_ERASE_AHEAD sectors from there are erased if they are not (never the one
 *            holding the newest record), before the audio starts, the watchdog fed around each.
 *   a cut    a power cut mid-record leaves no commit (or a bad CRC): the record before it stays the newest.
 *   full     the caller may reclaim a sector (project_reclaim) in a quiet moment: the whole sector after the newest
 *            record's (it holds only older records), so the journal goes on instead of every later save erasing
 *            OBJ_PROJ (on the FM-1 those erases were 22 dropouts in a session). */
project_t proj;

#define JR_SLOT 512u
#define JR_NSLOT ((int)(PLAT_JR_SECTOR * PLAT_JR_NSECTOR / JR_SLOT))   /* 64 */
#define JR_PER_SECTOR ((int)(PLAT_JR_SECTOR / JR_SLOT))                /* 8 */
#define JR_HDR_OFF (JR_SLOT - 16u)
#define JR_MAGIC 0x314A624Bu                                             /* "KbJ1" */
#define JR_CHUNK 64u                     /* bytes a program: each holds the interrupts (on the FM-1: W 5.1 ms,
                                          * that with the render it held back; smaller pieces, shorter holds) */
#define JR_ERASE_AHEAD 7                                                 /* >= 56 quick saves a power-on (4
                                                                          * gave 32; a session of 65 ran it out) */
typedef struct {
    uint32_t magic, seq, len, crc;
} jr_hdr_t;
typedef char jr_project_fits[(sizeof(project_t) <= JR_HDR_OFF && sizeof(jr_hdr_t) == 16u) ? 1 : -1];

static uint8_t jr_buf[JR_SLOT];                      /* RAM: the flash driver wants RAM sources */
static project_t jr_tmp;
static struct {
    int next, free;                                  /* the next slot to write, erased slots from it in a row */
    uint32_t seq;                                    /* the journal's own record count */
    uint32_t boot_erases, writes, fails, reclaims;
} jr;

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = fm_param_info(i)->def;
    proj.leds = 1;
    proj.mode = 1;                       /* a scene: ZEN-1's own (project_load) */
}

/* a format 2 project (before Kokyu, Hyoshigi and Mokugyo): the instrument keeps its slot's sound. Kokyu came in
 * before Taiko, the wood pair after it, so Taiko moves up one and FuMi's own kotos and the Harp three */
static void project_upgrade(project_t *p)
{
    if (p->magic == PROJ_MAGIC && p->format == 2u) {
        if (p->par[P_VOICE] >= 7)
            p->par[P_VOICE] += 3;
        else if (p->par[P_VOICE] == 6)
            p->par[P_VOICE] = FM_P_TAIKO;
        p->format = PROJ_FORMAT;
    }
}

static int project_valid(const project_t *p)
{
    int i;
    if (p->magic != PROJ_MAGIC || p->format != PROJ_FORMAT)
        return 0;
    for (i = 0; i < P_NPARAMS; i++) {
        const fm_param_t *pi = fm_param_info(i);
        if (p->par[i] < pi->lo || p->par[i] > pi->hi)
            return 0;
    }
    if (p->leds > 2u || p->mode > 1u || p->style > 1u)
        return 0;                        /* (proj.scene is clamped where it is used: scenes come and go) */
    return 1;
}

/* ------------------------------------------------------------ journal --- */
static uint32_t jr_crc(const uint8_t *p, uint32_t n)   /* CRC-32 (zlib's), a bit at a time: 512 bytes at most */
{
    uint32_t c = 0xFFFFFFFFu, i, k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8u; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static int jr_read_slot(int s)                        /* into jr_buf: 0 = read */
{
    return plat_jr_read((uint32_t)s * JR_SLOT, jr_buf, JR_SLOT);
}

static int jr_slot_erased(int s)
{
    uint32_t i;
    if (jr_read_slot(s) != 0)
        return 0;
    for (i = 0; i < JR_SLOT; i++)
        if (jr_buf[i] != 0xFFu)
            return 0;
    return 1;
}

static int jr_sector_erased(int sec)
{
    int i;
    for (i = 0; i < JR_PER_SECTOR; i++)
        if (!jr_slot_erased(sec * JR_PER_SECTOR + i))
            return 0;
    return 1;
}

/* slot s holds a valid record: its project into jr_tmp, its header into *h */
static int jr_slot_valid(int s, jr_hdr_t *h)
{
    if (jr_read_slot(s) != 0)
        return 0;
    memcpy(h, jr_buf + JR_HDR_OFF, sizeof *h);
    if (h->magic != JR_MAGIC || h->len < 8u || h->len > sizeof(project_t) || h->len > JR_HDR_OFF)
        return 0;
    if (jr_crc(jr_buf, h->len) != h->crc)
        return 0;
    memset(&jr_tmp, 0, sizeof jr_tmp);
    memcpy(&jr_tmp, jr_buf, h->len);                  /* (an older, shorter project: zeros after) */
    project_upgrade(&jr_tmp);
    return project_valid(&jr_tmp);
}

/* boot: the newest valid record into *out (1), or none (0); then the write position and the erases ahead */
static int jr_boot(project_t *out)
{
    int s, newest = -1, cursor, k, run;
    uint32_t best = 0, best_h = 0;
    jr_hdr_t h;
    memset(&jr, 0, sizeof jr);
    jr.next = -1;
    for (s = 0; s < JR_NSLOT; s++)
        if (jr_slot_valid(s, &h)) {
            if (h.seq > jr.seq)
                jr.seq = h.seq;
            if (newest < 0 || jr_tmp.save_seq > best || (jr_tmp.save_seq == best && h.seq > best_h)) {
                newest = s;                            /* the newest: the project's save count, then the journal's */
                best = jr_tmp.save_seq;
                best_h = h.seq;
                *out = jr_tmp;
            }
        }
    cursor = newest < 0 ? 0 : (newest + 1) % JR_NSLOT;
    if (cursor % JR_PER_SECTOR) {                     /* the rest of the newest record's sector: all erased? */
        for (k = cursor; k % JR_PER_SECTOR; k++)
            if (!jr_slot_erased(k))
                break;
        if (k % JR_PER_SECTOR)
            cursor = (cursor / JR_PER_SECTOR + 1) * JR_PER_SECTOR % JR_NSLOT;
    }
    for (k = 0; k < JR_ERASE_AHEAD; k++) {            /* whole sectors from the cursor's (or the next), erased */
        int sec = (cursor / JR_PER_SECTOR + k + (cursor % JR_PER_SECTOR ? 1 : 0)) % (int)PLAT_JR_NSECTOR;
        if (newest >= 0 && sec == newest / JR_PER_SECTOR)
            break;                                     /* never the newest record */
        if (!jr_sector_erased(sec)) {
            if (plat_jr_erase((uint32_t)sec) != 0)
                break;
            jr.boot_erases++;
        }
    }
    for (run = 0, s = cursor; run < JR_NSLOT; run++, s = (s + 1) % JR_NSLOT) {   /* erased slots from the cursor */
        if (newest >= 0 && s == newest)
            break;
        if (!jr_slot_erased(s))
            break;
    }
    jr.next = run ? cursor : -1;
    jr.free = run;
    return newest >= 0;
}

/* the project into the next erased slot: 0 = written and read back */
static int jr_write(void)
{
    jr_hdr_t h;
    uint32_t off, len = sizeof(project_t), base;
    int tries;
    for (tries = 0; tries < 3 && jr.free > 0; tries++) {
        int s = jr.next, ok = 1;
        jr.next = (jr.next + 1) % JR_NSLOT;
        jr.free--;
        if (!jr_slot_erased(s)) {                     /* someone else's bits: never program over them */
            jr.fails++;
            continue;
        }
        base = (uint32_t)s * JR_SLOT;
        memset(jr_buf, 0xFF, JR_SLOT);
        memcpy(jr_buf, &proj, len);
        h.magic = JR_MAGIC;
        h.seq = ++jr.seq;
        h.len = len;
        h.crc = jr_crc(jr_buf, len);
        memcpy(jr_buf + JR_HDR_OFF, &h, sizeof h);
        for (off = 0; off < len && ok; off += JR_CHUNK) {   /* the project, a piece at a time */
            uint32_t n = len - off > JR_CHUNK ? JR_CHUNK : len - off;
            ok = plat_jr_prog(base + off, jr_buf + off, n) == 0;
        }
        if (ok)                                        /* the commit record, last */
            ok = plat_jr_prog(base + JR_HDR_OFF, jr_buf + JR_HDR_OFF, sizeof h) == 0;
        if (ok && jr_slot_valid(s, &h) && memcmp(&jr_tmp, &proj, len) == 0) {
            jr.writes++;
            return 0;
        }
        jr.fails++;
    }
    return -1;
}

/* a journal save a piece at a time (the key lights went out a moment a few seconds after a change:
 * each program holds the interrupts, and the pieces back to back left the lights' scan no time between). The project
 * as it is now goes into jw's slot; then one piece each call (the scenes: one a frame), the commit record last, read
 * back. A power-off midway leaves the slot without its record: the save before it stands */
static struct {
    int s, active, tries;
    uint32_t off, len;
    jr_hdr_t h;
} jw;
static int jw_slot(void)                           /* the next erased slot, the project into jr_buf: 0, or -1 none */
{
    while (jw.tries < 3 && jr.free > 0) {
        int s = jr.next;
        jw.tries++;
        jr.next = (jr.next + 1) % JR_NSLOT;
        jr.free--;
        if (!jr_slot_erased(s)) {
            jr.fails++;
            continue;
        }
        jw.s = s;
        jw.off = 0;
        jw.len = sizeof(project_t);
        memset(jr_buf, 0xFF, JR_SLOT);
        memcpy(jr_buf, &proj, jw.len);
        jw.h.magic = JR_MAGIC;
        jw.h.seq = ++jr.seq;
        jw.h.len = jw.len;
        jw.h.crc = jr_crc(jr_buf, jw.len);
        memcpy(jr_buf + JR_HDR_OFF, &jw.h, sizeof jw.h);
        return 0;
    }
    return -1;
}
int project_save_begin(void)
{
    if (jr.free <= 0)
        return -1;
    proj.save_seq++;
    jw.tries = 0;
    if (jw_slot() != 0)
        return -1;
    jw.active = 1;
    return 0;
}
int project_save_step(void)
{
    uint32_t base = (uint32_t)jw.s * JR_SLOT;
    int ok;
    if (!jw.active)
        return -1;
    if (jw.off < jw.len) {                            /* a piece of the project */
        uint32_t n = jw.len - jw.off > JR_CHUNK ? JR_CHUNK : jw.len - jw.off;
        ok = plat_jr_prog(base + jw.off, jr_buf + jw.off, n) == 0;
        jw.off += JR_CHUNK;
        if (ok)
            return 1;
    } else {                                          /* the commit record, then read back */
        ok = plat_jr_prog(base + JR_HDR_OFF, jr_buf + JR_HDR_OFF, sizeof jw.h) == 0 && jr_slot_valid(jw.s, &jw.h) &&
             memcmp(&jr_tmp, jr_buf, jw.len) == 0;
        if (ok) {
            jr.writes++;
            jw.active = 0;
            return 0;
        }
    }
    jr.fails++;                                       /* (a slot gone bad: the next one, from the start) */
    if (jw_slot() == 0)
        return 1;
    jw.active = 0;
    return -1;
}

/* ------------------------------------------------------------- project --- */
/* ZEN-1 starts in ambient mode, every time: HOME held goes to the instrument. The mode
 * is still saved (the instrument's autosave keeps the rest of the project as it was), but not obeyed at boot */
uint8_t project_fresh;

int project_load(void)
{
    int n, have_obj, have_jr;
    static project_t from_jr;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n >= 8)
        project_upgrade(&proj);
    have_obj = n >= 8 && project_valid(&proj);
    have_jr = jr_boot(&from_jr);
    if (have_jr && (!have_obj || from_jr.save_seq > proj.save_seq))
        proj = from_jr;
    if (have_jr || have_obj) {
        proj.mode = 1;
        return 0;
    }
    project_defaults();
    project_fresh = 1;
    return -1;
}

int project_save_quick(void) { return jr.free > 0; }

/* the journal is full: erase the next sector that holds none of this session's newest record, and write on from
 * its start. It stops the interrupts for a moment (as any erase): call it in a quiet moment. 0 = room again */
int project_reclaim(void)
{
    int sec, i;
    if (jr.free > 0)
        return 0;
    if (jr.next < 0)
        return -1;                                     /* (no erased slot even at boot: the erase fails) */
    sec = jr.next / JR_PER_SECTOR;
    if (jr.next % JR_PER_SECTOR)                       /* the newest record is in this sector, before jr.next */
        sec = (sec + 1) % (int)PLAT_JR_NSECTOR;
    if (plat_jr_erase((uint32_t)sec) != 0)
        return -1;
    jr.reclaims++;
    jr.next = sec * JR_PER_SECTOR;
    jr.free = 0;
    for (i = 0; i < JR_PER_SECTOR && jr_slot_erased(jr.next + i); i++)
        jr.free++;
    return jr.free > 0 ? 0 : -1;
}

int project_save(void)
{
    proj.save_seq++;
    if (jr_write() == 0)
        return 0;
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);   /* the journal is full: the sector erase */
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        fm_set(i, proj.par[i]);
}
