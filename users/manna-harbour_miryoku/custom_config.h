// Copyright 2019 Manna Harbour
// https://github.com/manna-harbour/miryoku

// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once


// --- ada: HID0 instrument panel OLEDs ---
#define OLED_TIMEOUT 900000          // 15 min; the flatline IS the idle screen
#define SPLIT_LAYER_STATE_ENABLE     // slave renders layer name
#define SPLIT_MODS_ENABLE            // slave renders live GACS
#define SPLIT_ACTIVITY_ENABLE        // slave link LED

// --- ada: cold-boot master detection (2026-09-11) ---
// The Corne has no handedness pin, so split QMK decides master-vs-slave by
// polling for a COMPLETED USB enumeration (USB_ACTIVE), once, at power-up,
// for SPLIT_USB_TIMEOUT ms (split_util.c usbIsActive; default 2000). On a cold
// boot the board gets power while the PC is still in POST and nobody is
// enumerating; two seconds pass, the connected half concludes it is the SLAVE,
// and that decision is a static that never re-evaluates until power is cut.
// Symptom: keys register on the OLED (local matrix + local screen) but the PC
// receives nothing until the board is unplugged and replugged. Suspend/resume
// does not trip it (the role is already decided). Two reset experiments on a
// RUNNING board (usbreset + full xHCI unbind/rebind, 2026-09-10) both survived,
// which isolated the fault to this power-up window.
//
// FIRST ATTEMPT, REVERTED (2026-09-11 01:38): SPLIT_USB_TIMEOUT 30000. Wrong
// shape. The MASTER exits the poll the instant the host enumerates it; the
// SLAVE never sees USB_ACTIVE at all, so it burns the ENTIRE timeout before
// concluding it is the slave. Result: the right half plays dead for 30 s on
// every hot-plug (Khang thought it had bricked). Stock 2000 restored. The
// correct fix is a different shape — a half that decided SLAVE but later sees
// USB_ACTIVE must re-run detection instead of sitting on the static forever.
//
// SECOND ATTEMPT (2026-09-11 pm): QMK already ships that shape. With
// SPLIT_WATCHDOG_ENABLE a half that decided SLAVE and is never pinged by a
// master within SPLIT_WATCHDOG_TIMEOUT (3000 ms default; static-asserted
// > SPLIT_USB_TIMEOUT) calls mcu_reset() and boots fresh — a new 2 s USB poll,
// by which time the PC has enumerated it and it becomes MASTER. The genuine
// slave (right half) resets alongside until a master appears, then is pinged
// once and never resets again (split_watchdog_done is one-shot on the slave,
// transactions.c watchdog_handlers_slave). Cost on hot-plug: none — the
// master pings within its first transaction. Does NOT cover a right half
// whose MCU never booted (OLED dark = ada_oled never ran; the slave draws its
// header from local state, no link needed) — that one is power/flash, not
// firmware. The master's OLED header now shows a hollow link box while the
// slave is unreachable (ada_oled.c), so the next occurrence tells us which.
#define SPLIT_WATCHDOG_ENABLE

// --- ada: BASE outer home-row keys = one-shot Shift (2026-09-11 pm) ---
// Miryoku leaves the 3x6 outer columns KC_NO. The left one sits where Shift
// lives on a normal board. OSM(MOD_LSFT): tap it, the next key is shifted —
// no tapping term, no hold, no roll to get wrong. It exists because the
// roll-release capital (Shift up as the letter goes down) is how a real Shift
// key is used and no mod-tap setting can honour it; both bibles (precondition
// § "Shift thumb keys", urob's smart-shift) move typing-Shift off the tap-hold.
// F/J keep their Shift for chords (Ctrl+Shift+x) and for a properly held
// Shift. Khang's call: the thumbs are all spoken for, so outer home row.
// Other outer keys stay KC_NO. (The OLED learning panel renders the five
// inner columns only; the outer key isn't drawn.)
#define MIRYOKU_LAYERMAPPING_BASE( \
     K00, K01, K02, K03, K04,      K05, K06, K07, K08, K09, \
     K10, K11, K12, K13, K14,      K15, K16, K17, K18, K19, \
     K20, K21, K22, K23, K24,      K25, K26, K27, K28, K29, \
     N30, N31, K32, K33, K34,      K35, K36, K37, N38, N39 \
) \
LAYOUT_split_3x6_3( \
KC_NO,         K00, K01, K02, K03, K04,      K05, K06, K07, K08, K09, KC_NO, \
OSM(MOD_LSFT), K10, K11, K12, K13, K14,      K15, K16, K17, K18, K19, OSM(MOD_LSFT), \
KC_NO,         K20, K21, K22, K23, K24,      K25, K26, K27, K28, K29, KC_NO, \
                    K32, K33, K34,      K35, K36, K37 \
)

// --- ada: EXTRA = GAME (Khang's design, 2026-08-23) ---
// Base with the clever parts disabled, but layers kept: plain QWERTY alphas
// (no HRM — Super->Start focus steals killed it for gaming), plain Space
// (no hold tax on the jump key), MEDIA and MOUSE dropped (physical volume
// knobs + an MMO mouse own those jobs). NAV moves Spc->Esc so Space is free.
// Outer columns: gamer edges left (Tab/LShift/LCtrl), plain mods right
// (Super/RShift/RCtrl — plain KC_LGUI is tap-AND-hold Super natively).
// 2026-09-11: Tab and Esc swapped. Tab is HELD in games (scoreboard, map), so
// it cannot carry a layer; it moves to the plain outer-top corner. Esc takes
// the thumb and the NAV hold — Esc is only ever tapped in a game.
// No dedicated exit: layers stay live, so the canon switch keys ride along —
// enter = NAV + double-tap e, exit = NAV + double-tap r (and u on SYM/NUM/
// FUN; p there is BOOT — the double-tap guard is what stands between a raid
// and the bootloader).
#define MIRYOKU_LAYER_EXTRA \
KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,              KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,              \
KC_A,              KC_S,              KC_D,              KC_F,              KC_G,              KC_H,              KC_J,              KC_K,              KC_L,              KC_QUOT,           \
KC_Z,              KC_X,              KC_C,              KC_V,              KC_B,              KC_N,              KC_M,              KC_COMM,           KC_DOT,            KC_SLSH,           \
U_NP,              U_NP,              KC_LALT,           KC_SPC,            LT(U_NAV,KC_ESC),  LT(U_SYM,KC_ENT),  LT(U_NUM,KC_BSPC), LT(U_FUN,KC_DEL),  U_NP,              U_NP

#define MIRYOKU_LAYERMAPPING_EXTRA( \
     K00, K01, K02, K03, K04,      K05, K06, K07, K08, K09, \
     K10, K11, K12, K13, K14,      K15, K16, K17, K18, K19, \
     K20, K21, K22, K23, K24,      K25, K26, K27, K28, K29, \
     N30, N31, K32, K33, K34,      K35, K36, K37, N38, N39 \
) \
LAYOUT_split_3x6_3( \
KC_TAB,  K00, K01, K02, K03, K04,      K05, K06, K07, K08, K09, KC_LGUI, \
KC_LSFT, K10, K11, K12, K13, K14,      K15, K16, K17, K18, K19, KC_RSFT, \
KC_LCTL, K20, K21, K22, K23, K24,      K25, K26, K27, K28, K29, KC_RCTL, \
                   K32, K33, K34,      K35, K36, K37 \
)

// --- ada: TAP = the all-plain layer (Corne outer columns, TAP layer ONLY) ---
// (Was the gaming layer until EXTRA=GAME above superseded it, 2026-08-23.)
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
