#!/usr/bin/env python3
"""Preview the Corne OLED boot glider exactly as ada_oled.c draws it.

The panel is 128x32 physically; ada_oled.c runs it at OLED_ROTATION_270, so
in drawing coordinates it is 32 wide (x) by 128 tall (y). This script renders
that framebuffer as half-block characters — one character per pixel column,
two pixel rows per character row — so the aspect on screen matches the glass.

Everything you might want to tweak is in the block below. The numbers are the
ones in ada_render_boot_glider() today; change them here, look, then port the
ones you like back into the C.

    python3 glider_preview.py            # the glider region with a margin
    python3 glider_preview.py --full     # the whole 32x128 framebuffer
"""

import sys

# ---- tweak these ----------------------------------------------------------
CELLS  = [(1, 0), (2, 1), (0, 2), (1, 2), (2, 2)]   # the five live cells (col, row)
CELL   = 8          # filled square, px
PITCH  = 10         # distance between cell origins, px (so the gap is PITCH - CELL)
OX, OY = 2, 46      # top-left of the 3x3 grid on the 32x128 canvas
CORNER = [(0, 0), (CELL - 1, CELL - 1)]   # marker pixels inside an EMPTY cell,
                                          # relative to that cell's origin
GRID   = 3
# ---------------------------------------------------------------------------

W, H = 32, 128


def framebuffer():
    fb = [[False] * W for _ in range(H)]

    def px(x, y):
        if 0 <= x < W and 0 <= y < H:
            fb[y][x] = True

    live = set(CELLS)
    for cy in range(GRID):
        for cx in range(GRID):
            x0, y0 = OX + cx * PITCH, OY + cy * PITCH
            if (cx, cy) in live:
                for dy in range(CELL):
                    for dx in range(CELL):
                        px(x0 + dx, y0 + dy)
            else:
                for dx, dy in CORNER:
                    px(x0 + dx, y0 + dy)
    return fb


def render(fb, y0, y1):
    # half blocks: ▀ top only, ▄ bottom only, █ both, space neither
    out = []
    for y in range(y0, y1, 2):
        row = []
        for x in range(W):
            top = fb[y][x]
            bot = fb[y + 1][x] if y + 1 < H else False
            row.append("█" if top and bot else "▀" if top else "▄" if bot else " ")
        out.append("".join(row))
    return out


if __name__ == "__main__":
    fb = framebuffer()
    if "--full" in sys.argv:
        y0, y1 = 0, H
    else:
        span = (GRID - 1) * PITCH + CELL
        y0 = max(0, OY - 4) & ~1            # even, so half-block pairs line up
        y1 = min(H, OY + span + 4 + 1) | 1
        y1 += 1
    lines = render(fb, y0, y1)
    print("┌" + "─" * W + "┐")
    for l in lines:
        print("│" + l + "│")
    print("└" + "─" * W + "┘")
    print(f"32×128 canvas, showing y={y0}..{y1 - 1} · cells {CELL}px on a {PITCH}px pitch at ({OX},{OY})")
