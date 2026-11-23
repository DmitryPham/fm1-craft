/* SPDX-License-Identifier: MPL-2.0
 * FM-1 CRAFT: the FM-1 as a controller for FigCraft (and Figma), on fm1-base.
 * Built from fm1-base's template (Copyright (C) 2026 Charles Vestal). This header is the app's half of the contract
 * with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "FM-1 CRAFT"
#define APP_TAGLINE "controller for FigCraft"
#define APP_SPLASH_RGB RGB(93, 202, 165)
#define APP_PALETTE 0u                   /* gfx.c PALETTES: green */
#define APP_BLOCK 256u
#define APP_SYSEX                        /* app_sysex: the companion script's frames (the screen's values) */

/* the store: the app's settings and the panel calibration (the platform's), and their flash offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}
#define APP_FILE_OBJS {{OBJ_SETTINGS, PLAT_FK_SETTINGS, PLAT_FF_READ | PLAT_FF_WRITE, "settings"}}
int app_file_busy(void);
void app_file_changed(uint32_t obj);

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
int app_sysex(const uint8_t *p, uint32_t n);

/* ------------------------------------------------------------------ the wire (docs/PROTOCOL.md) --- */
#define CRAFT_CH 15u                     /* MIDI channel 16 */
#define CRAFT_NOTE0 53u                  /* key 0 (F3) */
#define CRAFT_CC_ENC 20u                 /* K1..K4 = CC 20..23, SELECT 24, ALGORITHM 25, PRESETS 26: relative */
#define CRAFT_CC_MASTER 27u              /* MASTER: absolute 0..127 */
#define CRAFT_CC_PAGE 28u                /* the page shown: 0 POS, 1 SIZE, 2 STYLE, 3 TEXT */
#define CRAFT_CC_BTN 102u                /* button B_* = CC 102 + B_*: 127 down, 0 up */
#define CRAFT_SX_ID0 0x7Du               /* SysEx: F0 7D 46 43 <cmd> .. F7 ("FC") */
#define CRAFT_SX_ID1 0x46u
#define CRAFT_SX_ID2 0x43u

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
