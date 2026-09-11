// ada (2026-09-11): tap-hold engine overrides for Khang's Corne.
//
// WHY THIS FILE EXISTS: users/<name>/config.h includes custom_config.h FIRST and
// then hard-sets TAPPING_TERM 200 and IGNORE_MOD_TAP_INTERRUPT after it, so the
// custom hook can't override those. post_config.h is appended after EVERY
// config.h (builddefs/build_keyboard.mk:360 adds it, :454 places it last), so
// it wins. Miryoku doesn't ship one — this is a new file, zero canon edits.
//
// THE RECIPE — urob's "timeless" home-row mods, translated to this QMK base
// (which predates CHORDAL_HOLD and FLOW_TAP; achordion supplies both):
//
//   PERMISSIVE_HOLD    ZMK "balanced". A key pressed AND released while the
//                      mod-tap is still down settles it as a HOLD at once,
//                      timer be damned. This is exactly the hold-F-tap-I
//                      capitalisation gesture that was misfiring as "fi".
//                      In this tree it is an independent branch
//                      (action_tapping.c:205) — IGNORE_MOD_TAP_INTERRUPT can
//                      stay; it only gates the press-only path, which
//                      achordion vetoes anyway.
//   TAPPING_TERM 280   Loosened. With the two rules below the timer is a
//                      backstop for holding a mod alone, not the decision.
//   achordion          Chordal Hold on old QMK (ada_tap_hold.c): a same-hand
//                      next key settles the mod-tap as a TAP. Layer-taps are
//                      exempt — the layers put same-hand content under them.
//   ACHORDION_STREAK   Flow Tap / require-prior-idle: a mod-tap pressed inside
//                      a fast typing streak is a tap. Shift gets a SHORT
//                      window (ada_tap_hold.c) so fast capitalisation still
//                      works — urob's own footnote on the recipe.
//
// AUTO_SHIFT_TIMEOUT is defined in config.h as the literal token TAPPING_TERM,
// so it would silently follow the bump to 280. Pinned back to 200 — held-symbol
// auto-shift was not part of the complaint and should not move.

#pragma once

#undef TAPPING_TERM
#define TAPPING_TERM 280

#define PERMISSIVE_HOLD

#define ACHORDION_STREAK

#undef AUTO_SHIFT_TIMEOUT
#define AUTO_SHIFT_TIMEOUT 200
