// Copyright 2019 Manna Harbour
// https://github.com/manna-harbour/miryoku

// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once


// --- ada: HID0 instrument panel OLEDs ---
#define OLED_TIMEOUT 900000          // 15 min; the flatline IS the idle screen
#define SPLIT_LAYER_STATE_ENABLE     // slave renders layer name
#define SPLIT_MODS_ENABLE            // slave renders live GACS
#define SPLIT_ACTIVITY_ENABLE        // slave link LED

// --- ada: TAP = the gaming layer (Corne outer columns, TAP layer ONLY) ---
// TAP has no GACS and no layer keys by design; thumbs already carry
// Esc/Spc/Tab/Ent/Bspc/Del. The six outer keys add only what's missing:
//   left  (WASD pinky):  Alt / Shift / Ctrl
//   right (mouse hand):  exit-to-base (double-tap) / CapsLock / Super
// The exit key makes TAP no longer a one-way door (replug rescue retired).
#define MIRYOKU_LAYERMAPPING_TAP( \
     K00, K01, K02, K03, K04,      K05, K06, K07, K08, K09, \
     K10, K11, K12, K13, K14,      K15, K16, K17, K18, K19, \
     K20, K21, K22, K23, K24,      K25, K26, K27, K28, K29, \
     N30, N31, K32, K33, K34,      K35, K36, K37, N38, N39 \
) \
LAYOUT_split_3x6_3( \
KC_LALT, K00, K01, K02, K03, K04,      K05, K06, K07, K08, K09, TD(U_TD_U_BASE), \
KC_LSFT, K10, K11, K12, K13, K14,      K15, K16, K17, K18, K19, KC_CAPS, \
KC_LCTL, K20, K21, K22, K23, K24,      K25, K26, K27, K28, K29, KC_LGUI, \
                   K32, K33, K34,      K35, K36, K37 \
)
