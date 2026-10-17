/* SPDX-License-Identifier: GPL-3.0-only */
/* ZEN-1 app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/fumi.h"

#ifndef ZN_HUD                           /* the load readout in the header: dev builds (ZN_HUD=0|1 ./build.sh overrides) */
#ifdef FM_VERSION
#define ZN_HUD 0
#else
#define ZN_HUD 1
#endif
#endif
#ifndef FM_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define FM_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x314D624Bu           /* "KbM1": ZEN-1 (FuMi's "FuM1" saved setup is not loaded) */
#define PROJ_FORMAT 3u                   /* (3: FuMi 1.0.7's fourteen instruments; a 2 is brought up to it,
                                          * project_upgrade; one from before is not read) */

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* fumi.h P_* */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t mode;                        /* 0 the instrument (FuMi), 1 ambient (a scene) */
    uint8_t scene;                       /* which scene */
    uint8_t style;                       /* 0 painterly, 1 pixel art */
    uint8_t sound[4];                    /* each scene's sounds' version its knobs were saved with (scene.h) */
    uint8_t frogs;                       /* (Hotaru's frogs, recorded or synthesized; unused now) */
    uint8_t rsv[7];                      /* room to grow: an older, shorter project loads (zeros here) */
    uint8_t mood[4];                     /* each scene's mood (by its slot; 0 Natural) */
    uint8_t tweak[4];                    /* bit m: scene's mood m has the player's tweaks (mk), else as it was made */
    uint8_t mk[4][5][16];                /* each scene's moods' knobs as the player left them (each 0..255) */
    uint32_t save_seq;                   /* counts every save: at boot the newest of the journal and OBJ_PROJ wins */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
extern uint8_t project_fresh;            /* nothing was saved (a first start): the welcome card */
int project_save(void);                  /* 0 = saved and verified (the journal: no pause; else a sector erase) */
int project_save_quick(void);            /* 1: the next save goes to the journal (no pause in the sound) */
int project_save_begin(void);            /* a journal save a piece at a time: 0 begun, -1 no room */
int project_save_step(void);             /* its next piece: 1 more to come, 0 saved and verified, -1 failed */
int project_reclaim(void);               /* the journal full: erase a sector of it (a pause: call it in the quiet) */
void project_apply(void);                /* every value to the engine (boot, load) */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
