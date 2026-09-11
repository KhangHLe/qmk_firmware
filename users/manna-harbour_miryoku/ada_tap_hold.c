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

// Chordal-hold rule. The six thumb layer-taps are always allowed to hold: the
// layers deliberately place same-hand content under them (the layer-lock tap
// dances on the top row, the one-shot mods on NAV's home row). Everything else
// — the eight home-row mod-taps — settles as a HOLD only against a key on the
// OPPOSITE hand. Same-hand rolls ("of", "as", "we") therefore can never fire a
// modifier, which is what let TAPPING_TERM loosen.
bool achordion_chord(uint16_t tap_hold_keycode, keyrecord_t* tap_hold_record,
                     uint16_t other_keycode, keyrecord_t* other_record) {
    if (IS_QK_LAYER_TAP(tap_hold_keycode)) {
        return true;
    }
    return achordion_opposite_hands(tap_hold_record, other_record);
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
//                    Shift is down; a real "fi" roll releases F first) and by
//                    the opposite-hands rule above.
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
