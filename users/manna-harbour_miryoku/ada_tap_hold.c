// ada (2026-09-11): tap-hold policy — wires achordion into Miryoku's userspace.
// Rides on the SRC += hook in custom_rules.mk; Miryoku's own .c defines neither
// process_record_user nor housekeeping_task_user, so both are free here.
// Tunables and the reasoning live in post_config.h.

#include QMK_KEYBOARD_H
#include "achordion.h"

bool process_record_user(uint16_t keycode, keyrecord_t* record) {
    if (!process_achordion(keycode, record)) {
        return false;
    }
    return true;
}

void housekeeping_task_user(void) {
    achordion_task();
}

// Chordal-hold rule: OFF (2026-09-11 pm, Khang's call after a day on it).
// The first flash settled a home-row mod as HOLD only against the OPPOSITE
// hand (achordion_opposite_hands). That killed every one-handed shortcut —
// Ctrl(D)+C/V/Z/X/A/S/W/T and Gui(A)+Esc/Space are all left-hand-with-left-
// mod, and Miryoku's answer ("use K for Ctrl") is a relearn he didn't ask
// for. Returning true here hands the tap/hold decision back to QMK's own
// PERMISSIVE_HOLD + TAPPING_TERM, and achordion stays loaded ONLY for the
// streak rule below. Same-hand nested rolls ("as" with S released before A)
// can now fire Gui+S again; the 200 ms streak still catches those mid-word
// and after a fast space.
bool achordion_chord(uint16_t tap_hold_keycode, keyrecord_t* tap_hold_record,
                     uint16_t other_keycode, keyrecord_t* other_record) {
    return true;
}

// Streak (Flow Tap): a mod-tap pressed within this many ms of the previous key
// event is settled as a TAP. 0 disables the rule for that key.
//   layer-taps  0    the thumbs are never "in a streak"; holding Space for NAV
//                    right after a word is the normal case.
//   Shift       0    OFF. First flash (2026-09-11 01:32) used 100 ms and it
//                    re-created the bug it was meant to fix: "fi thought",
//                    "jalso" — every miss was a capital right after a space
//                    at speed, where the space->Shift gap is 60-80 ms, inside
//                    the window. urob's footnote, paid for live: "shifting
//                    alphas is the one scenario where pressing a mod may
//                    conflict with require-prior-idle." Shift stays protected
//                    by PERMISSIVE_HOLD (the other key must be RELEASED while
//                    Shift is down; a real "fi" roll releases F first).
//                    The ROLL-release capital (J↓ S↓ J↑ S↑ → "js", how a
//                    real Shift key is used) is not fixable by any tap-hold
//                    setting without breaking "fo"→"O"; that gesture moves
//                    to the one-shot Shift on the outer home-row keys
//                    (custom_config.h, MIRYOKU_LAYERMAPPING_BASE).
//   others    200    Ctrl/Alt/Gui mid-word are almost always a misfire.
uint16_t achordion_streak_chord_timeout(uint16_t tap_hold_keycode,
                                        uint16_t next_keycode) {
    if (IS_QK_LAYER_TAP(tap_hold_keycode)) {
        return 0;
    }
    uint8_t mod = mod_config(QK_MOD_TAP_GET_MODS(tap_hold_keycode));
    if ((mod & MOD_LSFT) != 0) {
        return 0;
    }
    return 200;
}
