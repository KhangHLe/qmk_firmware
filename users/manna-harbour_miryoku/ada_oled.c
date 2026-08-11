// Copyright 2026 Khang Le & Ada
// SPDX-License-Identifier: GPL-2.0-or-later
//
// ada_oled.c — the learning panel (v2).
//
// Both halves render the SAME structure, each for its OWN hand:
//   header:   hand letter + layer name + link dot
//   tap grid: what each key types on the current layer (live, per-layer)
//   hold row: on BASE — what each key does when HELD (GACS mods, button
//             layer, thumb layer names). On other layers: the layer name
//             big, since holds mostly don't apply there.
//   GACS strip: globally-active mods (catches cross-hand chords)
//
// Pressed keys highlight live in the grid (inverted cell). This works
// per-half with zero transport: matrix_common.c debounces each half's own
// scan into matrix[] locally (matrix + thisHand), so matrix_get_row() on
// either half reads that half's real key state.
//
// Matrix -> display mapping (from LAYOUT_split_3x6_3 in crkbd/rev1/rev1.h):
//   left  rows 0-2: display col = matrix col   (col 0 = outer)
//   right rows 4-6: display col = 5 - matrix col (matrix col 0 = outer)
//   thumbs row 3/7, matrix cols 3..5; left idx = col-3, right idx = 5-col
//
// The v1 instrument panel (hid0 seismograph) is shelved intact in
// ada_oled_hid.c — swap SRC in custom_rules.mk to bring the packets back.
//
// All drawing in OLED_ROTATION_270 logical space: x [0,32), y [0,128).

#include QMK_KEYBOARD_H

#ifdef OLED_ENABLE

// ---------------------------------------------------------------- micro font
// 3x5 glyphs, one byte per row, low 3 bits used (MSB-first: bit2=left pixel).
// Index: 0-25 = a-z, 26-35 = 0-9, 36 = ':', 37+ = symbols (ada_sym_chars).
static const uint8_t ada_font[][5] = {
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
    // symbols, in ada_sym_chars order
    {06, 04, 04, 04, 06}, // [
    {03, 01, 01, 01, 03}, // ]
    {03, 02, 06, 02, 03}, // {
    {06, 02, 03, 02, 06}, // }
    {02, 04, 04, 04, 02}, // (
    {02, 01, 01, 01, 02}, // )
    {02, 05, 02, 05, 03}, // &
    {05, 02, 07, 02, 05}, // *
    {03, 06, 02, 03, 06}, // $
    {05, 01, 02, 04, 05}, // %
    {02, 05, 00, 00, 00}, // ^
    {00, 02, 07, 02, 00}, // +
    {00, 03, 06, 00, 00}, // ~
    {02, 02, 02, 00, 02}, // !
    {02, 05, 07, 04, 03}, // @
    {05, 07, 05, 07, 05}, // #
    {02, 02, 02, 02, 02}, // |
    {00, 00, 00, 00, 07}, // _
    {00, 02, 00, 02, 04}, // ;
    {00, 07, 00, 07, 00}, // =
    {04, 04, 02, 01, 01}, // backslash
    {04, 02, 00, 00, 00}, // `
    {00, 00, 00, 00, 02}, // .
    {00, 00, 07, 00, 00}, // -
    {00, 00, 00, 02, 04}, // ,
    {02, 02, 00, 00, 00}, // '
    {01, 01, 02, 04, 04}, // /
    {01, 02, 04, 02, 01}, // <
    {04, 02, 01, 02, 04}, // >
};

static const char ada_sym_chars[] = "[]{}()&*$%^+~!@#|_;=\\`.-,'/<>";

static int8_t glyph_index(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == ':') return 36;
    for (uint8_t i = 0; ada_sym_chars[i]; i++)
        if (ada_sym_chars[i] == c) return 37 + i;
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

// ---------------------------------------------------------------- key maps
// Display content per layer per hand. rows[] read display-left-to-right
// (col 0 = the outer column — blank everywhere except TAP).
// thumbs[] are 2-char labels, display order (left hand: Esc Spc Tab;
// right hand: Ent Bspc Del).
typedef struct {
    char rows[3][7];
    char thumbs[3][3];
} ada_hand_map_t;

// Layer order matches miryoku_layer_list.h:
// 0 base, 1 extra, 2 tap, 3 button, 4 nav, 5 mouse, 6 media, 7 num, 8 sym, 9 fun
#define ADA_QWERTY_L {" qwert", " asdfg", " zxcvb"}
#define ADA_QWERTY_R {"yuiop ", "hjkl' ", "nm,./ "}

static const ada_hand_map_t ada_tap_maps[10][2] = {
    // base
    {{ADA_QWERTY_L, {"es", "sp", "tb"}}, {ADA_QWERTY_R, {"en", "bs", "dl"}}},
    // extra (byte-identical to base in our build)
    {{ADA_QWERTY_L, {"es", "sp", "tb"}}, {ADA_QWERTY_R, {"en", "bs", "dl"}}},
    // tap (gaming: outer columns live)
    {{{"aqwert", "sasdfg", "czxcvb"}, {"es", "sp", "tb"}},
     {{"yuiopb", "hjkl'c", "nm,./g"}, {"en", "bs", "dl"}}},
    // button (mirrored clipboard + clicks)
    {{{" uxcpr", " gacs ", " uxcpr"}, {"mc", "lc", "rc"}},
     {{"rpcxu ", " scag ", "rpcxu "}, {"rc", "lc", "mc"}}},
    // nav (content on the right hand)
    {{{"      ", " gacs ", "  r   "}, {"  ", "  ", "  "}},
     {{"rpcxu ", "<v^>w ", "hduei "}, {"en", "bs", "dl"}}},
    // mouse
    {{{"      ", " gacs ", "  r   "}, {"  ", "  ", "  "}},
     {{"rpcxu ", "<v^>  ", "<v^>  "}, {"rc", "lc", "mc"}}},
    // media
    {{{"      ", " gacs ", "  r   "}, {"  ", "  ", "  "}},
     {{"      ", "<-+>  ", "      "}, {"st", "pl", "mu"}}},
    // num (content on the left hand)
    {{{" [789]", " ;456=", " `123\\"}, {". ", "0 ", "- "}},
     {{"      ", " scag ", "  r   "}, {"  ", "  ", "  "}}},
    // sym
    {{{" {&*(}", " :$%^+", " ~!@#|"}, {"( ", ") ", "_ "}},
     {{"      ", " scag ", "  r   "}, {"  ", "  ", "  "}}},
    // fun (F-keys: digits shown at numpad positions, F implied)
    {{{"  789p", "  456s", "  123 "}, {"mn", "sp", "tb"}},
     {{"      ", " scag ", "  r   "}, {"  ", "  ", "  "}}},
};

// Hold semantics — meaningful on BASE only: home-row mods, the BUTTON
// layer on z and /, and what each thumb opens.
static const ada_hand_map_t ada_hold_maps[2] = {
    {{"      ", " gacs ", " b    "}, {"md", "nv", "ms"}},
    {{"      ", " scag ", "    b "}, {"sy", "nu", "fn"}},
};

static const char *ada_layer_names[] = {
    "base", "ext", "tap", "btn", "nav", "mse", "med", "num", "sym", "fun",
};

// ---------------------------------------------------------------- this hand
// Which display cell is physically pressed right now, from this half's own
// locally-scanned matrix rows.
static uint8_t ada_hand(void) { return is_keyboard_left() ? 0 : 1; }

static bool ada_key_down(uint8_t disp_row, uint8_t disp_col) {
    uint8_t base_row = is_keyboard_left() ? 0 : MATRIX_ROWS / 2;
    if (disp_row < 3) { // alpha rows
        uint8_t mcol = is_keyboard_left() ? disp_col : 5 - disp_col;
        return matrix_get_row(base_row + disp_row) & ((matrix_row_t)1 << mcol);
    }
    // thumbs: disp_col 0..2, matrix cols 3..5
    uint8_t mcol = is_keyboard_left() ? disp_col + 3 : 5 - disp_col;
    return matrix_get_row(base_row + 3) & ((matrix_row_t)1 << mcol);
}

// ---------------------------------------------------------------- rendering
// Grid geometry: 6 cols, 5px pitch, glyphs at x = 1 + col*5.
// Alpha rows 9px pitch; thumb labels are 2 chars in 3 wider slots.
static void ada_render_grid(const ada_hand_map_t *map, uint8_t y0) {
    for (uint8_t r = 0; r < 3; r++) {
        uint8_t y = y0 + r * 9;
        for (uint8_t c = 0; c < 6; c++) {
            uint8_t x    = 1 + c * 5;
            bool    down = ada_key_down(r, c);
            char    ch   = map->rows[r][c];
            if (down) {
                ada_fill_rect(x - 1, y - 2, 5, 9, true);
                ada_draw_char(x, y, ch, 1, false); // carved
            } else {
                ada_draw_char(x, y, ch, 1, true);
            }
        }
    }
    // thumbs
    uint8_t ty = y0 + 27;
    for (uint8_t t = 0; t < 3; t++) {
        uint8_t x    = 3 + t * 10;
        bool    down = ada_key_down(3, t);
        if (down) {
            ada_fill_rect(x - 1, ty - 2, 9, 9, true);
            ada_draw_char(x, ty, map->thumbs[t][0], 1, false);
            ada_draw_char(x + 4, ty, map->thumbs[t][1], 1, false);
        } else {
            ada_draw_char(x, ty, map->thumbs[t][0], 1, true);
            ada_draw_char(x + 4, ty, map->thumbs[t][1], 1, true);
        }
    }
}

static void ada_render_learning(void) {
    uint8_t hand  = ada_hand();
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    if (layer > 9) layer = 0;

    // header: hand letter + layer name + link dot
    char hdr[8];
    hdr[0] = hand ? 'r' : 'l';
    hdr[1] = ':';
    uint8_t i = 0;
    for (; ada_layer_names[layer][i] && i < 4; i++) hdr[2 + i] = ada_layer_names[layer][i];
    hdr[2 + i] = 0;
    ada_draw_str(1, 1, hdr, 1, true);
    if (last_input_activity_elapsed() < 250) ada_fill_rect(29, 1, 2, 2, true);
    ada_dotted_hline(8);

    // tap grid
    ada_render_grid(&ada_tap_maps[layer][hand], 13);
    ada_dotted_hline(48);

    // hold grid on base/extra; big layer name otherwise
    if (layer <= 1) {
        ada_render_grid(&ada_hold_maps[hand], 53);
    } else {
        const char *name = ada_layer_names[layer];
        uint8_t len = 0;
        while (name[len]) len++;
        uint8_t w  = len * 8 - 2;
        uint8_t x0 = w >= 32 ? 0 : (32 - w) / 2;
        ada_draw_str(x0, 62, name, 2, true);
    }
    ada_dotted_hline(88);

    // GACS strip: boxes light when the mod is actually held (either hand)
    uint8_t mods = get_mods() | get_weak_mods() | get_oneshot_mods();
    static const char    mod_ch[4]   = {'g', 'a', 'c', 's'};
    static const uint8_t mod_mask[4] = {MOD_MASK_GUI, MOD_MASK_ALT, MOD_MASK_CTRL, MOD_MASK_SHIFT};
    for (uint8_t m = 0; m < 4; m++) {
        uint8_t bx = 1 + m * 8;
        bool    on = mods & mod_mask[m];
        if (on) {
            ada_fill_rect(bx, 93, 7, 9, true);
            ada_draw_char(bx + 2, 95, mod_ch[m], 1, false); // carved
        } else {
            ada_draw_char(bx + 2, 95, mod_ch[m], 1, true);
        }
    }

    // wpm, master only (not synced to the slave)
    if (is_keyboard_master()) {
        char buf[7];
        uint8_t wpm = get_current_wpm();
        buf[0] = '0' + (wpm / 100) % 10;
        buf[1] = '0' + (wpm / 10) % 10;
        buf[2] = '0' + wpm % 10;
        buf[3] = 'w'; buf[4] = 'p'; buf[5] = 'm'; buf[6] = 0;
        ada_draw_str(4, 112, wpm >= 100 ? buf : buf + 1, 1, true);
    }
}

// ---------------------------------------------------------------- boot
static void ada_render_boot_glider(void) {
    static const uint8_t cells[5][2] = {{1,0},{2,1},{0,2},{1,2},{2,2}};
    for (uint8_t i = 0; i < 5; i++) {
        ada_fill_rect(2 + cells[i][0] * 10, 46 + cells[i][1] * 10, 8, 8, true);
    }
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

// ---------------------------------------------------------------- hooks
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_270;
}

bool oled_task_user(void) {
    if (timer_read32() < 2200) {
        oled_clear();
        ada_render_boot_glider();
        return false;
    }
    oled_clear();
    ada_render_learning();
    return false;
}

#endif // OLED_ENABLE
