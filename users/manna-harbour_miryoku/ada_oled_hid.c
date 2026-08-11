// Copyright 2026 Khang Le & Ada
// SPDX-License-Identifier: GPL-2.0-or-later
//
// ada_oled.c — the HID0 instrument panel.
//
// Master half: "hid0" — live per-hand key-traffic seismograph (L/R mirrored
//   around a dotted axis), link LED, syn/ack burst tags, hex packet counters,
//   tiny WPM. Idle: traces decay to flatline, rst tag after 15s.
// Slave half:  "hid1" — current layer name (big) + live GACS mod row.
// Both: 2.2s Game of Life glider emblem at boot.
//
// All drawing is done with oled_write_pixel in OLED_ROTATION_270 logical
// space: x in [0,32), y in [0,128) — portrait, as physically mounted.

#include QMK_KEYBOARD_H

#ifdef OLED_ENABLE

// ---------------------------------------------------------------- micro font
// 3x5 glyphs, one byte per row, low 3 bits used (MSB-first: bit2=left pixel).
// Index: 0-25 = a-z, 26-35 = 0-9, 36 = ':'
static const uint8_t ada_font[37][5] = {
    {07, 05, 07, 05, 05}, // a
    {06, 05, 06, 05, 06}, // b
    {07, 04, 04, 04, 07}, // c
    {06, 05, 05, 05, 06}, // d
    {07, 04, 06, 04, 07}, // e
    {07, 04, 06, 04, 04}, // f
    {07, 04, 05, 05, 07}, // g
    {05, 05, 07, 05, 05}, // h
    {07, 02, 02, 02, 07}, // i
    {07, 01, 01, 05, 07}, // j
    {05, 05, 06, 05, 05}, // k
    {04, 04, 04, 04, 07}, // l
    {05, 07, 07, 05, 05}, // m
    {06, 05, 05, 05, 05}, // n
    {07, 05, 05, 05, 07}, // o
    {07, 05, 07, 04, 04}, // p
    {07, 05, 05, 07, 01}, // q
    {06, 05, 06, 05, 05}, // r
    {07, 04, 07, 01, 07}, // s
    {07, 02, 02, 02, 02}, // t
    {05, 05, 05, 05, 07}, // u
    {05, 05, 05, 05, 02}, // v
    {05, 05, 07, 07, 05}, // w
    {05, 05, 02, 05, 05}, // x
    {05, 05, 07, 01, 07}, // y
    {07, 01, 02, 04, 07}, // z
    {07, 05, 05, 05, 07}, // 0
    {02, 06, 02, 02, 07}, // 1
    {07, 01, 07, 04, 07}, // 2
    {07, 01, 07, 01, 07}, // 3
    {05, 05, 07, 01, 01}, // 4
    {07, 04, 07, 01, 07}, // 5
    {07, 04, 07, 05, 07}, // 6
    {07, 01, 02, 02, 02}, // 7
    {07, 05, 07, 05, 07}, // 8
    {07, 05, 07, 01, 07}, // 9
    {00, 02, 00, 02, 00}, // :
};

static int8_t glyph_index(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == ':') return 36;
    return -1; // space / unknown -> blank
}

static void ada_draw_char(uint8_t x0, uint8_t y0, char c, uint8_t scale, bool on) {
    int8_t gi = glyph_index(c);
    if (gi < 0) return;
    for (uint8_t ry = 0; ry < 5; ry++) {
        uint8_t bits = ada_font[gi][ry];
        for (uint8_t rx = 0; rx < 3; rx++) {
            if (bits & (04 >> rx)) {
                for (uint8_t sy = 0; sy < scale; sy++)
                    for (uint8_t sx = 0; sx < scale; sx++)
                        oled_write_pixel(x0 + rx * scale + sx, y0 + ry * scale + sy, on);
            }
        }
    }
}

static void ada_draw_str(uint8_t x0, uint8_t y0, const char *s, uint8_t scale, bool on) {
    for (uint8_t i = 0; s[i]; i++)
        ada_draw_char(x0 + i * (3 * scale + scale), y0, s[i], scale, on);
}

static void ada_fill_rect(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, bool on) {
    for (uint8_t y = y0; y < y0 + h; y++)
        for (uint8_t x = x0; x < x0 + w; x++)
            oled_write_pixel(x, y, on);
}

static void ada_dotted_hline(uint8_t y) {
    for (uint8_t x = 1; x < 31; x++)
        oled_write_pixel(x, y, x % 2);
}

static void ada_hex8(char *out, uint8_t v) {
    static const char hexd[] = "0123456789abcdef";
    out[0] = hexd[v >> 4];
    out[1] = hexd[v & 0xf];
    out[2] = 0;
}

// ---------------------------------------------------------------- state
#define ADA_G_LEN       88   // graph rows (history depth)
#define ADA_G_TOP       12   // first graph row
#define ADA_BUCKET_MS   350  // ms per graph row
#define ADA_MID         16   // center axis x
#define ADA_AMP_MAX     13   // max amplitude px per side
#define ADA_TAG_LIFE    55   // burst tag lifetime in buckets

static uint8_t  hist_l[ADA_G_LEN], hist_r[ADA_G_LEN]; // ring, head = newest
static uint8_t  g_head       = 0;
static uint8_t  bucket_l     = 0, bucket_r = 0;       // keys in current bucket
static uint16_t total_l      = 0, total_r  = 0;       // lifetime per hand
static uint32_t pkt_total    = 0;                      // lifetime packets
static uint16_t bucket_timer = 0;
static uint32_t last_key32   = 0;
static int8_t   tag_age_l    = -1, tag_age_r = -1;    // -1 = inactive
static const char *tag_txt_r = "ack";
static uint8_t  both_streak  = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        if (record->event.key.row < MATRIX_ROWS / 2) bucket_l++; else bucket_r++;
        pkt_total++;
        last_key32 = timer_read32();
    }
    return true;
}

static void ada_push_bucket(void) {
    if (bucket_l && bucket_r) both_streak++; else both_streak = 0;
    total_l += bucket_l;
    total_r += bucket_r;
    g_head = (g_head + 1) % ADA_G_LEN;
    hist_l[g_head] = bucket_l > 4 ? 4 : bucket_l;
    hist_r[g_head] = bucket_r > 4 ? 4 : bucket_r;
    // burst tags: a hot bucket pins a tag to this row; est on sustained flow
    if (tag_age_l >= 0 && ++tag_age_l > ADA_TAG_LIFE) tag_age_l = -1;
    if (tag_age_r >= 0 && ++tag_age_r > ADA_TAG_LIFE) tag_age_r = -1;
    if (bucket_l >= 3 && tag_age_l < 0) tag_age_l = 0;
    if (bucket_r >= 3 && tag_age_r < 0) { tag_age_r = 0; tag_txt_r = "ack"; }
    if (both_streak == 10 && tag_age_r < 0) { tag_age_r = 0; tag_txt_r = "est"; }
    bucket_l = bucket_r = 0;
}

// ---------------------------------------------------------------- screens
static void ada_render_boot_glider(void) {
    static const uint8_t cells[5][2] = {{1,0},{2,1},{0,2},{1,2},{2,2}};
    for (uint8_t i = 0; i < 5; i++) {
        ada_fill_rect(2 + cells[i][0] * 10, 46 + cells[i][1] * 10, 8, 8, true);
    }
    // corner ticks for the empty cells
    for (uint8_t cy = 0; cy < 3; cy++)
        for (uint8_t cx = 0; cx < 3; cx++) {
            bool filled = false;
            for (uint8_t i = 0; i < 5; i++)
                if (cells[i][0] == cx && cells[i][1] == cy) filled = true;
            if (!filled) {
                oled_write_pixel(2 + cx * 10, 46 + cy * 10, true);
                oled_write_pixel(2 + cx * 10 + 7, 46 + cy * 10 + 7, true);
            }
        }
}

static uint8_t ada_amp(uint8_t v) { // bucket count -> pixel amplitude
    uint8_t a = v * 3;
    return a > ADA_AMP_MAX ? ADA_AMP_MAX : a;
}

static void ada_render_instrument(void) {
    char buf[12];

    // header: hid0 + link LED
    ada_draw_str(1, 1, "hid0", 1, true);
    if (timer_elapsed32(last_key32) < 250) ada_fill_rect(28, 1, 2, 2, true);
    ada_dotted_hline(8);

    // graph: newest at top, scrolling down
    uint8_t prev_l = ada_amp(hist_l[g_head]), prev_r = ada_amp(hist_r[g_head]);
    for (uint8_t i = 0; i < ADA_G_LEN; i++) {
        uint8_t  y  = ADA_G_TOP + i;
        uint8_t  hi = (g_head + ADA_G_LEN - i) % ADA_G_LEN;
        uint8_t  al = ada_amp(hist_l[hi]), ar = ada_amp(hist_r[hi]);
        uint8_t  lx = ADA_MID - al, rx = ADA_MID + ar;
        // polyline: connect to previous row's edge
        uint8_t la = lx < ADA_MID - prev_l ? lx : ADA_MID - prev_l;
        uint8_t lb = lx > ADA_MID - prev_l ? lx : ADA_MID - prev_l;
        for (uint8_t x = la; x <= lb; x++) oled_write_pixel(x, y, true);
        uint8_t ra = rx < ADA_MID + prev_r ? rx : ADA_MID + prev_r;
        uint8_t rb = rx > ADA_MID + prev_r ? rx : ADA_MID + prev_r;
        for (uint8_t x = ra; x <= rb; x++) oled_write_pixel(x, y, true);
        prev_l = al; prev_r = ar;
        if (i % 3 == 0) oled_write_pixel(ADA_MID, y, true); // dotted axis
        if (i % 16 == 0) { oled_write_pixel(0, y, true); oled_write_pixel(31, y, true); }
    }

    // burst tags in the margins, background cleared so they read crisp
    if (tag_age_l >= 0) {
        uint8_t y = ADA_G_TOP + tag_age_l;
        if (y < ADA_G_TOP + ADA_G_LEN - 6) {
            ada_fill_rect(1, y, 12, 7, false);
            ada_draw_str(2, y + 1, "syn", 1, true);
        }
    }
    if (tag_age_r >= 0) {
        uint8_t y = ADA_G_TOP + tag_age_r;
        if (y < ADA_G_TOP + ADA_G_LEN - 6) {
            ada_fill_rect(19, y, 12, 7, false);
            ada_draw_str(20, y + 1, tag_txt_r, 1, true);
        }
    }

    // idle: rst tag, blinking slow
    if (timer_elapsed32(last_key32) > 15000 && (timer_read32() & 1024)) {
        ada_fill_rect(9, 56, 14, 9, false);
        ada_draw_str(11, 58, "rst", 1, true);
    }

    ada_dotted_hline(ADA_G_TOP + ADA_G_LEN + 2);

    // counters: l:XX r:XX / pkt:XXXX / NNwpm
    uint8_t base = ADA_G_TOP + ADA_G_LEN + 5;
    char hx[3];
    ada_hex8(hx, total_l & 0xff);
    buf[0]='l'; buf[1]=':'; buf[2]=hx[0]; buf[3]=hx[1]; buf[4]=0;
    ada_draw_str(1, base, buf, 1, true);
    ada_hex8(hx, total_r & 0xff);
    buf[0]='r'; buf[1]=':'; buf[2]=hx[0]; buf[3]=hx[1]; buf[4]=0;
    ada_draw_str(17, base, buf, 1, true);

    ada_hex8(hx, (pkt_total >> 8) & 0xff);
    buf[0]='p'; buf[1]='k'; buf[2]='t'; buf[3]=':'; buf[4]=hx[0]; buf[5]=hx[1];
    ada_hex8(hx, pkt_total & 0xff);
    buf[6]=hx[0]; buf[7]=hx[1]; buf[8]=0;
    ada_draw_str(1, base + 7, buf, 1, true);

    uint8_t wpm = get_current_wpm();
    buf[0] = '0' + (wpm / 100) % 10;
    buf[1] = '0' + (wpm / 10) % 10;
    buf[2] = '0' + wpm % 10;
    buf[3] = 'w'; buf[4] = 'p'; buf[5] = 'm'; buf[6] = 0;
    ada_draw_str(4, base + 14, wpm >= 100 ? buf : buf + 1, 1, true);
}

// layer index -> short display name (matches miryoku_layer_list.h order)
static const char *ada_layer_names[] = {
    "base", "ext", "tap", "btn", "nav", "mse", "med", "num", "sym", "fun",
};

static void ada_render_layer_screen(void) {
    // header: hid1 + link LED (synced activity)
    ada_draw_str(1, 1, "hid1", 1, true);
    if (last_input_activity_elapsed() < 250) ada_fill_rect(28, 1, 2, 2, true);
    ada_dotted_hline(8);

    // big layer name, centered, scale 2
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    const char *name = layer < 10 ? ada_layer_names[layer] : "???";
    uint8_t len = 0;
    while (name[len]) len++;
    uint8_t w  = len * 8 - 2;
    uint8_t x0 = w >= 32 ? 0 : (32 - w) / 2;
    ada_draw_str(x0, 40, name, 2, true);
    ada_dotted_hline(58);

    // GACS row: boxes light when the mod is actually held
    uint8_t mods = get_mods() | get_weak_mods() | get_oneshot_mods();
    static const char    mod_ch[4]   = {'g', 'a', 'c', 's'};
    static const uint8_t mod_mask[4] = {MOD_MASK_GUI, MOD_MASK_ALT, MOD_MASK_CTRL, MOD_MASK_SHIFT};
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t bx = 1 + i * 8;
        bool    on = mods & mod_mask[i];
        if (on) {
            ada_fill_rect(bx, 78, 7, 9, true);
            ada_draw_char(bx + 2, 80, mod_ch[i], 1, false); // carved
        } else {
            ada_draw_char(bx + 2, 80, mod_ch[i], 1, true);
        }
    }
}

// ---------------------------------------------------------------- hooks
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_270;
}

bool oled_task_user(void) {
    // boot: the emblem
    if (timer_read32() < 2200) {
        oled_clear();
        ada_render_boot_glider();
        return false;
    }

    if (is_keyboard_master()) {
        if (timer_elapsed(bucket_timer) >= ADA_BUCKET_MS) {
            bucket_timer = timer_read();
            ada_push_bucket();
        }
        oled_clear();
        ada_render_instrument();
    } else {
        oled_clear();
        ada_render_layer_screen();
    }
    return false;
}

#endif // OLED_ENABLE
