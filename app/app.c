/* SPDX-License-Identifier: MPL-2.0
 * FM-1 CRAFT: the FM-1 as a controller for FigCraft (and Figma).
 * Built from fm1-base's template (Copyright (C) 2026 Charles Vestal).
 *
 * Everything goes out over USB-MIDI on channel 16 (docs/PROTOCOL.md): the keys as notes, the knobs as relative
 * controllers, the buttons as controllers (127 down, 0 up), MASTER as an absolute one. The FM-1 makes no sound. The
 * companion script (fm1_figcraft.py) turns that into editor commands and sends back, as SysEx, what the screen shows:
 * the selected layer's name and the four values the knobs edit.
 *
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in scope. */

#define SET_MAGIC 0x43524631u            /* "CRF1" */
typedef struct {
    uint32_t magic;
    uint8_t page;                        /* 0 POS, 1 SIZE, 2 STYLE, 3 TEXT */
    uint8_t rsv[3];
} settings_t;
static settings_t set;

#define NPAGES 4u
static const char *const PAGE_NAME[NPAGES] = {"POS", "SIZE", "STYLE", "TEXT"};
static const char *const KNOB_LABEL[NPAGES][4] = {
    {"X", "Y", "SCALE", "OPACITY"},
    {"WIDTH", "HEIGHT", "ROTATE", "RADIUS"},
    {"HUE", "SAT", "LIGHT", "ALPHA"},
    {"SIZE", "LINE", "TRACK", "WEIGHT"},
};
/* the button that shows each page (its CC goes out too) */
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_ENV, B_LFO, B_ARP};

/* ------------------------------------------------------------------ sound: none --- */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    (void)master_q12;
    for (i = 0; i < 2u * n; i++)
        lr[i] = 0;
}
void app_panic(void) {}
int app_file_busy(void) { return 0; }

static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    set.page = 0;
    set.rsv[0] = set.rsv[1] = set.rsv[2] = 0;
}
void app_init(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.page >= NPAGES)
        settings_defaults();
}
void app_file_changed(uint32_t obj)
{
    if (obj == OBJ_SETTINGS)
        app_init();
}

/* ------------------------------------------------------------------ MIDI out --- */
static uint32_t sent_count;              /* messages out (the host checks) */
static void send3(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t cin = st >> 4;
    plat_midi_send(cin | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24, TO_USB);
    sent_count++;
}
static void send_cc(uint32_t cc, uint32_t v) { send3(0xB0u | CRAFT_CH, cc, v); }

static uint32_t enc_cc(uint32_t role)    /* K1..K4 first, then SELECT, ALGORITHM, PRESETS */
{
    return role >= EN_K1 ? CRAFT_CC_ENC + (role - EN_K1) : CRAFT_CC_ENC + 4u + role;
}
static void send_rel(uint32_t cc, int32_t d)   /* relative, two's complement: 1..63 up, 127..65 down */
{
    while (d) {
        int32_t s = clamp(d, -63, 63);
        send_cc(cc, (uint32_t)(s > 0 ? s : 128 + s));
        d -= s;
    }
}

/* ------------------------------------------------------------------ UI state --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint32_t keys, btn;                  /* held at the last input */
    uint32_t turned_at[NE];              /* when each encoder was last turned: its card lights a moment */
    int32_t turns[NE];                   /* detents since boot (the host checks) */
    uint32_t master_cc;                  /* MASTER's last controller value sent, 0xFF none yet */
    /* what the script sent (app_sysex) */
    uint32_t link_at;                    /* the last frame from the script (0: never) */
    char layer[28];                      /* the selected layer, "" none */
    char val[4][12];                     /* the four values as text */
    uint8_t bar[4];                      /* 0..100, 0xFF no bar */
    uint8_t redraw;
} ui;

static void text_copy(char *d, const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; i + 1u < max && s && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}
static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static int linked(void) { return ui.link_at && plat_ms() - ui.link_at < 3000u; }

void app_say(const char *a, const char *b)
{
    char t[48];
    text_copy(ui.msg[0], a, sizeof ui.msg[0]);
    text_copy(ui.msg[1], b, sizeof ui.msg[1]);
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void clear_values(void)
{
    uint32_t i;
    ui.layer[0] = 0;
    for (i = 0; i < 4u; i++) {
        ui.val[i][0] = 0;
        ui.bar[i] = 0xFFu;
    }
}

static void page_set(uint32_t p, int send)
{
    if (p >= NPAGES)
        return;
    if (p != set.page) {
        set.page = (uint8_t)p;
        clear_values();                  /* the script sends the new page's values */
        (void)plat_store_save(OBJ_SETTINGS, &set, sizeof set);
    }
    if (send)
        send_cc(CRAFT_CC_PAGE, p);
    ui.redraw = 1;
}

/* ------------------------------------------------------------------ SysEx from the script --- */
/* F0 7D 46 43 cmd .. F7; p is the bytes between F0 and F7.
 *   01 STATE  page, bar1..bar4 (0..100, 7F none), then NUL-ended strings: layer, value1..value4
 *   02 SAY    two NUL-ended strings: a message over the header
 *   03 PING   the script is there */
static uint32_t sx_str(const uint8_t *p, uint32_t n, uint32_t i, char *out, uint32_t max)
{
    uint32_t k = 0;
    while (i < n && p[i]) {
        if (k + 1u < max)
            out[k++] = p[i] >= 0x20u && p[i] < 0x7Fu ? (char)p[i] : '?';
        i++;
    }
    out[k] = 0;
    return i < n ? i + 1u : n;           /* past the NUL */
}
int app_sysex(const uint8_t *p, uint32_t n)
{
    uint32_t i, k;
    char a[24], b[24];
    if (n < 4u || p[0] != CRAFT_SX_ID0 || p[1] != CRAFT_SX_ID1 || p[2] != CRAFT_SX_ID2)
        return 0;
    ui.link_at = plat_ms() | 1u;
    switch (p[3]) {
    case 0x01:
        if (n < 9u)
            break;
        if (p[4] < NPAGES && p[4] != set.page)
            page_set(p[4], 0);
        for (k = 0; k < 4u; k++)
            ui.bar[k] = p[5 + k] <= 100u ? p[5 + k] : 0xFFu;
        i = sx_str(p, n, 9u, ui.layer, sizeof ui.layer);
        for (k = 0; k < 4u; k++)
            i = sx_str(p, n, i, ui.val[k], sizeof ui.val[k]);
        ui.redraw = 1;
        break;
    case 0x02:
        i = sx_str(p, n, 4u, a, sizeof a);
        (void)sx_str(p, n, i, b, sizeof b);
        app_say(a, b);
        break;
    default:
        break;
    }
    return 1;
}

/* ------------------------------------------------------------------ input --- */
static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k, m;
    int32_t e;
    for (k = 0; k < NKEYS; k++) {        /* the keys: notes, F3 = 53 */
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit))
            send3(0x90u | CRAFT_CH, CRAFT_NOTE0 + k, 100u);
        else if (!(keys & bit) && (ui.keys & bit))
            send3(0x80u | CRAFT_CH, CRAFT_NOTE0 + k, 0u);
    }
    for (k = 0; k < NB; k++) {           /* the buttons: a controller each, down and up */
        uint32_t bit = 1u << k;
        if ((btn & bit) == (ui.btn & bit))
            continue;
        send_cc(CRAFT_CC_BTN + k, (btn & bit) ? 127u : 0u);
        if (btn & bit) {
            for (m = 0; m < NPAGES; m++)
                if (PAGE_BTN[m] == k)
                    page_set(m, 1);
        }
    }
    for (k = 0; k < NE; k++) {           /* the encoders: relative controllers */
        if ((e = plat_enc((int)k)) == 0)
            continue;
        send_rel(enc_cc(k), e);
        ui.turns[k] += e;
        ui.turned_at[k] = plat_ms() | 1u;
        ui.redraw = 1;
    }
    m = plat_master() * 127u / 4096u;    /* MASTER: absolute, with a step of slack against jitter */
    if (ui.master_cc == 0xFFu || m + 1u < ui.master_cc || m > ui.master_cc + 1u || (m == 0u && ui.master_cc) ||
        (m == 127u && ui.master_cc != 127u)) {
        ui.master_cc = m;
        send_cc(CRAFT_CC_MASTER, m);
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    plat_leds(btn | 1u << PAGE_BTN[set.page], keys);
}
void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ screen --- */
static void frame_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* an outline */
{
    cv_rect(x, y, w, 1, c);
    cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c);
    cv_rect(x + w - 1, y, 1, h, c);
}

static void draw_header(void)
{
    uint32_t now = plat_ms();
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 7, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 7, &FONT_B, ui.msg[1], C_HI);
    } else {
        const char *st = linked() ? "FIGCRAFT" : "NO LINK";
        int32_t w = text_w(&FONT_XS, st) + 12;
        cv_text(10, 7, &FONT_B, APP_NAME, C_WHITE);
        cv_rect(230 - w, 8, w, 16, linked() ? C_HI : C_LINE);
        cv_text(236 - w, 10, &FONT_XS, st, linked() ? C_BLACK : C_GRAY);
    }
    cv_rect(0, 31, 240, 1, C_LINE);
    cv_blit(0, 0);
}

static void draw_page(void)
{
    uint32_t i;
    cv_begin(240, 46, C_BLACK);
    for (i = 0; i < NPAGES; i++) {       /* the tabs */
        int32_t x = 6 + (int32_t)i * 58, on = i == set.page;
        cv_rect(x, 2, 54, 18, on ? C_HI : C_LINE);
        cv_text(x + (54 - text_w(&FONT_XS, PAGE_NAME[i])) / 2, 4, &FONT_XS, PAGE_NAME[i], on ? C_BLACK : C_GRAY);
    }
    if (ui.layer[0])
        cv_text(8, 26, &FONT_B, ui.layer, C_WHITE);
    else
        cv_text(8, 28, &FONT_XS, linked() ? "NOTHING SELECTED" : "RUN START.BAT ON THE PC", C_DIM);
    cv_blit(0, 34);
}

static void draw_cards(void)
{
    uint32_t i, now = plat_ms();
    cv_begin(240, 110, C_BLACK);
    for (i = 0; i < 4u; i++) {
        int32_t x = 4 + (int32_t)(i % 2u) * 118, y = 2 + (int32_t)(i / 2u) * 54, w = 114, h = 52;
        int lit = ui.turned_at[EN_K1 + i] && now - ui.turned_at[EN_K1 + i] < 350u;
        char t[8] = "K";
        frame_rect(x, y, w, h, lit ? C_HI : C_LINE);
        int_text((int32_t)i + 1, t + 1);
        cv_text(x + 6, y + 4, &FONT_XS, t, C_DIM);
        cv_text(x + 22, y + 4, &FONT_XS, KNOB_LABEL[set.page][i], lit ? C_HI : C_GRAY);
        cv_text(x + 6, y + 19, &FONT_M, ui.val[i][0] ? ui.val[i] : "--", ui.val[i][0] ? C_WHITE : C_DIM);
        cv_rect(x + 6, y + h - 6, w - 12, 3, C_LINE);
        if (ui.bar[i] != 0xFFu)
            cv_rect(x + 6, y + h - 6, (w - 12) * ui.bar[i] / 100, 3, lit ? C_WHITE : C_HI);
    }
    cv_blit(0, 82);
}

static void draw_keys(void)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
    uint32_t i;
    cv_begin(240, 48, C_BLACK);
    for (i = 0; i < 16u; i++)
        cv_rect(8 + (int32_t)i * 14, 13, 13, 14, ui.keys >> WK[i] & 1u ? C_HI : C_LINE);
    for (i = 0; i < 11u; i++)
        cv_rect(8 + (int32_t)AT[i] * 14 + 8, 2, 11, 10, ui.keys >> BK[i] & 1u ? C_HI : C_DIM);
    cv_text(8, 31, &FONT_XS, "OCT -/+  LAYER", C_GRAY);
    cv_text(232 - text_w(&FONT_XS, "GLO  x10"), 31, &FONT_XS, "GLO  x10", C_GRAY);
    cv_blit(0, 192);
}

static void draw(void)
{
    draw_header();
    draw_page();
    draw_cards();
    draw_keys();
}

void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    clear_values();
    ui.master_cc = 0xFFu;
    ui.redraw = 1;
    send_cc(CRAFT_CC_PAGE, set.page);
}

void app_frame(void)
{
    static uint32_t lit_until, last_link;
    uint32_t i, now = plat_ms();
    input();
    ann_tick(now);
    for (i = 0; i < NE; i++)             /* a card's light goes out */
        if (ui.turned_at[i] && now - ui.turned_at[i] < 400u && now > lit_until)
            lit_until = ui.turned_at[i] + 360u;
    if (lit_until && now >= lit_until) {
        lit_until = 0;
        ui.redraw = 1;
    }
    if ((uint32_t)linked() != last_link) {   /* the script came or went */
        last_link = (uint32_t)linked();
        if (!last_link)
            clear_values();
        ui.redraw = 1;
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "page"))
        *got = set.page;
    else if (!strcmp(what, "linked"))
        *got = linked();
    else if (!strcmp(what, "sent"))
        *got = (int)sent_count;
    else if (!strcmp(what, "bar0"))
        *got = ui.bar[0];
    else if (!strncmp(what, "turns", 5))
        *got = ui.turns[atoi(what + 5) % NE];
    else
        return 0;
    return 1;
}
#endif
