/* SPDX-License-Identifier: GPL-3.0-only */
/* FuMi-1: the instrument. A shigin conductor after the Suiko ST-50, on 6-operator FM (fm6_core.c).
 *
 *   Keys     white keys 0..15 = mi fa la ti do three times and a top mi (tuning.h); black keys 0..10 = the
 *            ST-50's upper row, or ornaments (the app decides: fm_key / fm_ornament).
 *   Pitch    tuning.c (本数, scale, 調律, 微調) -> a target in cents per voice -> smooth.c (no steps reach an
 *            oscillator) -> + vibrato -> fm6_logfreq_cents. The bend buttons (bend.c) move the most recent note.
 *   Voices   FM_NVOICE notes of the current patch (a 155-byte voice, fm6_patch.c); re-plucking a key
 *            re-excites its own voice; stealing retriggers without a phase reset (no click).
 *   余韻      a release gain per voice after key-up, 20 ms .. 8 s. トリラー re-plucks the held note at a rate with
 *            a little variation. ビブラート a sine on the pitch. 単音 last-note priority.
 *   Output   low cut, high cut, character (tilt and warmth), the plate reverb (reverb.c), soft clip.
 *
 * Threads: fm_render() runs in the audio interrupt; everything else posts commands that the render drains at
 * the start of a block. The state marked (UI) is written by the render and only read elsewhere. Float only,
 * no libm (fastmath.h), no double. */
#pragma once
#include <stdint.h>
#include "tuning.h"

#define FM_SR 44100.0f
#define FM_NVOICE 12                             /* 8 koto + reverb measured 38 % on the FM-1 (K0): 12 is ~60 % */
#define FM_NWHITE 16
#define FM_NBLACK 11
#define FM_NKEY (FM_NWHITE + FM_NBLACK)          /* fm_key ids: 0..15 white, 16..26 black */
#include "fumi_patch_ids.h"                   /* FM_P_KOTO ...: the built-in patches' slots */
#define FM_NOTE_ID 32                            /* fm_note ids start here (MIDI, the bench) */
#define FM_VOICE_ID 160                          /* fm_voice ids: 160..255 (ZEN-1's scenes) */
#define FM_NPART 8                               /* part 0: the instrument (P_VOICE); 1..7: a scene's layers */

/* parameters (the project saves them; ranges and defaults in fm_param_info) */
enum {
    P_HON, P_SCALE, P_TUNING, P_DEPTH, P_FINE, P_A4, P_OCTAVE,           /* 本数, scale, 調律, depth %, 微調, A, octave */
    P_VOICE, P_YOIN, P_LEVEL,                                            /* patch slot, 余韻, output level */
    P_VIB_ON, P_VIB_RATE, P_VIB_DEPTH,                                   /* ビブラート */
    P_TRILL_ON, P_TRILL_RATE, P_TRILL_VAR,                               /* トリラー: re-plucks at a rate, with variation */
    P_MONO, P_BLACK, P_BEND_UP, P_BEND_DOWN, P_BEND_TIME, P_SLIDE, P_SLIDE_TIME,
    P_LOWCUT, P_HIGHCUT, P_CHARACTER, P_REVERB, P_REV_SIZE, P_MIDI_OUT,
    P_USER0, P_USER1, P_USER2, P_USER3, P_USER4, P_USER5, P_USER6, P_USER7, P_USER8, P_USER9, P_USER10, P_USER11,
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} fm_param_t;
const fm_param_t *fm_param_info(int p);
void fm_param_text(int p, int v, char *buf);     /* as the screen shows it (12 bytes) */

enum { BEND_T_SEMI, BEND_T_WHOLE, BEND_T_SCALE };          /* P_BEND_UP */
enum { ORN_VIB, ORN_TRILL, ORN_DAMP, ORN_STRONG, ORN_N };   /* fm_ornament */

/* commands (main loop -> render) */
void fm_init(void);
void fm_set(int param, int value);
void fm_key(int key, int on);                    /* a panel key, FM_NKEY ids */
void fm_note(int id, int on, float cents);       /* any pitch (cents from A4): MIDI, the bench; id >= FM_NOTE_ID */
void fm_note_vel(int id, int on, float cents, int vel);   /* the same at a velocity (MIDI in: 1..127) */
void fm_ornament(int orn, int on);               /* momentary techniques on the sounding notes */
void fm_bend(int which, int down, int note_held);   /* OCT- / OCT+ (bend.h BEND_DOWN / BEND_UP) */
void fm_panic(void);
/* ZEN-1's parts: layers of a scene, each its own patch, release and level. Part 0 is the instrument and
 * follows P_VOICE and 余韻; fm_part_* on part 0 is ignored. */
void fm_part_patch(int part, int slot);          /* a built-in patch on part 1..4: P_VOICE numbering, or
                                                  * ZN_PATCH_BASE + n for ZEN-1's own (zen_patches.h) */
void fm_part_release(int part, int ms);          /* after a note's end: the time to -60 dB, 20 .. 8000 ms */
void fm_part_level(int part, int pct);           /* 0 .. 100 */
extern uint8_t fm_part_patch_set[FM_NPART];      /* the patch each part was last given (for tests) */
void fm_part_keep(int part, int on);             /* 1: the part's held notes are never stolen (a drone) */
/* a scene note: id FM_VOICE_ID..255 (the same id again re-plucks its voice); vel 1..127 (the patch's velocity
 * sensitivity: louder and brighter); pan 0 (left) .. 64 (center) .. 127 (right); dur_ms > 0 releases by itself
 * after that long, 0 holds until fm_voice_off */
void fm_voice(int id, int part, float cents, int vel, int pan, int dur_ms);
void fm_voice_off(int id);
void fm_voice_all_off(void);                     /* every scene note lets go: one command (a scene's leave) */
void fm_tape(int pct);                           /* tape warmth 0..100: softer highs, a ducked hiss, wow (scenes) */
void fm_depth(int pct);                          /* depth 0..100: the scene's sound bright to mellow */
void fm_scene_gain(int pct);
/* frogs (bed.c): slot 0..FM_NSHOT-1 each one call at a time (a new one replaces it): a recorded call (zen_samples.h
 * ZN_FROG_TAB[seg]) at rate_pct (100 as recorded; a few % up or down: another frog). vel 1..127, pan 0..127 */
void fm_frog(int slot, int seg, int vel, int pan, int rate_pct);
void fm_drop(int vel, int pan, int pad);         /* a raindrop seen landing, heard there (pan 0..127); pad: on a leaf, a tick */
int fm_frog_count(void);                         /* the recorded calls, and how long each is (ms) */
int fm_frog_ms(int seg);
void fm_bird(int slot, int seg, int vel, int pan, int rate_pct);   /* a bush warbler's song, as fm_frog */
enum { FM_SHOT_FROG, FM_SHOT_BIRD, FM_SHOT_KNOCK, FM_SHOT_OWL };    /* the recorded one-shots' sets */
#define FM_NSHOT 4                               /* one-shot slots, each one call at a time */
void fm_shot(int slot, int set, int seg, int vel, int pan, int rate_pct);   /* any set's call, as fm_frog */
int fm_shot_count(int set);
int fm_shot_ms(int set, int seg);
int fm_bird_count(void);
int fm_bird_ms(int seg);                     /* the scenes' loudness, % (25..400; 100 the instrument's own) */
int fm_voice_held(int id);                       /* main loop: 1 if scene note id sounds, not yet released (a glance) */
void fm_voice_glide(int id, float cents, int ms);   /* a sounding scene note moves to a new pitch, no new attack,
                                                     * there (within 1 % of the way) in about ms */
/* the nature beds (bed.c): level 0..100 glides in about a second; the amount (0..100) is the scene's: wind
 * gusts, the stream's rush, how hard it rains */
enum { FM_BED_WIND, FM_BED_STREAM, FM_BED_RAIN, FM_NBED };
void fm_bed(int which, int pct);
void fm_bed_mod(int which, int pct);
void fm_bed_style(int which, int style);          /* FM_BED_STREAM: FM_WATER_*; FM_BED_WIND: FM_AIR_* */
enum { FM_WATER_BUBBLES, FM_WATER_BROOK, FM_WATER_NOISE, FM_WATER_PINK, FM_WATER_DEEP, FM_WATER_RAIN, FM_WATER_TRICKLE,
       FM_WATER_SLOW, FM_NWATER };                                /* (the first three keep their numbers: saved knobs hold them) */
enum { FM_AIR_BREEZE_SYNTH, FM_AIR_BREEZE, FM_AIR_LEAVES, FM_AIR_INSECTS };
void fm_patch_set(const uint8_t *packed128);     /* the current patch (a DX7-format packed voice) */
void fm_patch_get(uint8_t *packed128);           /* main loop: the patch the render uses */
int fm_patch_count(void);                        /* FuMi's own patches (P_VOICE) */
const char *fm_patch_name(int i);
const char *fm_patch_ja(int i);                   /* its Japanese name, UTF-8 (the screen uses zen_kanji.h) */
const uint8_t *fm_patch_builtin(int i);          /* its packed record */
float fm_key_cents(int key);                     /* main loop: the pitch a panel key plays now, cents from A4 */

/* the render (audio ISR): n stereo frames (a multiple of 32), 24-bit in int32; gain Q12 (MASTER) */
void fm_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the platform sends it (status, data 1, data 2) */
void fm_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile float fm_key_level[FM_NKEY];     /* each key's voice level, 0..1 (lights) */
extern volatile float fm_bend_cents;             /* the bend now */
extern volatile float fm_peak;                   /* output peak of the last block before the soft clip (1 = full scale) */
extern volatile uint8_t fm_nvoices;              /* voices sounding */
extern volatile char fm_patch_label[11];         /* the current patch's name */
extern volatile uint8_t fm_patch_custom;         /* 1: a patch sent by fm_patch_set, not a P_VOICE slot */
