# Copyright 2019 Manna Harbour
# https://github.com/manna-harbour/miryoku


# --- ada: HID0 instrument panel OLEDs ---
OLED_ENABLE = yes
WPM_ENABLE = yes
SRC += ada_oled.c
# ada (2026-09-11): timeless home-row mods — achordion (Chordal Hold + Flow Tap
# on this pre-CHORDAL_HOLD base) and the policy that wires it in. See post_config.h.
SRC += achordion.c ada_tap_hold.c
