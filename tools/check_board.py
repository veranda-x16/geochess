#!/usr/bin/env python3
"""Check the board on the screen against a position (gates GC1, GC2).

    check_board.py <layout> <dump.bin> <POSITION> <font.cvt> [out.png]

layout: gc1 (upstream's 320x200 geometry), gc2 (the 640x480 board) or
gc2c (the same with the colour map: every square drawn light).
The dump is x16emu's (-dump V): the foreground bitmap is the first
38,400 bytes of VRAM, 80 bytes a row, bit 7 the left pixel, 1 = ink.
POSITION is the test build's (veranda/test/gc_test.c), the board as the
engine holds it. Each square is drawn here as geochess.c draws it, from
upstream's font (tools/cvtfont.py), and compared pixel for pixel with
the screen. out.png: the board region, expected | screen (needs Pillow).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cvtfont  # noqa: E402

VRAM = 131072
TAIL = VRAM + 256 + 512 + 14 + 1024

# the engine's piece types (piece & 7) -> geochess.h's display pieces
DISPLAY = [0, 6, 6, 1, 4, 3, 5, 2]          # -, P, p, K, N, B, R, Q
# GetPieceChar: [square colour][display piece] -> glyph
GLYPHS = ["\0ABCDEFMNOPQR", "\0STUVWXGHIJKL"]


def piece_glyph(code, light_dark):
    if code == 0:
        return None
    d = DISPLAY[code & 7]
    if code & 24 == 16:
        d += 6
    return GLYPHS[light_dark][d]


def screen(dump):
    d = open(dump, "rb").read()
    v = d[-TAIL:][:VRAM]

    def px(x, y):
        return (v[y * 80 + (x >> 3)] >> (7 - (x & 7))) & 1
    return px


def position(path):
    rows = open(path).read().split("\n")
    return [[int(b, 16) for b in rows[r].split()] for r in range(8)]


def expect_gc1(glyphs, base, pos):
    """{(x, y): bit}: InitBoard/DrawSquare at 320x200 (sc_width 1)"""
    want = {}
    for r in range(8):
        for c in range(8):
            colour = (r + c) & 1
            top, left = 34 + 18 * r, 27 + 20 * c
            for y in range(top, top + 17):
                for x in range(left, left + 19):
                    want[(x, y)] = colour
            g = piece_glyph(pos[r][c], colour)
            if g:
                gy = 50 + 18 * r - base
                for y, row in enumerate(glyphs[ord(g)]):
                    for x, b in enumerate(row):
                        want[(27 + 20 * c + x, gy + y)] = int(b)
    return want


# GC2: the 640x480 board (geochess.h): squares of 48 x 48 from (48, 48),
# the glyph scaled 2x (38 x 32) at (+5, +8) in its square
SQ2, X2, Y2, GX2, GY2 = 48, 48, 48, 5, 8


def square2(glyph_rows, colour):
    rows = [[colour] * SQ2 for _ in range(SQ2)]
    if glyph_rows:
        for y, row in enumerate(cvtfont.scale2x(glyph_rows)):
            for x, b in enumerate(row):
                rows[GY2 + y][GX2 + x] = int(b)
    return rows


def expect_gc2(glyphs, base, pos, colour_map=False):
    want = {}
    for r in range(8):
        for c in range(8):
            colour = 0 if colour_map else (r + c) & 1
            g = piece_glyph(pos[r][c], colour)
            sq = square2(glyphs[ord(g)] if g else None, colour)
            for y in range(SQ2):
                for x in range(SQ2):
                    want[(X2 + SQ2 * c + x, Y2 + SQ2 * r + y)] = sq[y][x]
    return want


def main():
    if len(sys.argv) not in (5, 6):
        sys.exit(__doc__)
    layout, dump, posf, fontf = sys.argv[1:5]
    base, _, glyphs = cvtfont.font(cvtfont.records(open(fontf, "rb").read())[16])
    pos = position(posf)
    if layout == "gc1":
        want = expect_gc1(glyphs, base, pos)
    else:
        want = expect_gc2(glyphs, base, pos, layout == "gc2c")
    px = screen(dump)
    bad = [(x, y) for (x, y), b in want.items() if px(x, y) != b]
    if len(sys.argv) == 6:
        from PIL import Image
        xs = [x for x, _ in want]
        ys = [y for _, y in want]
        x0, y0, w, h = min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        im = Image.new("1", (2 * w + 8, h), 1)
        for (x, y), b in want.items():
            im.putpixel((x - x0, y - y0), 1 - b)
            im.putpixel((w + 8 + x - x0, y - y0), 1 - px(x, y))
        im.save(sys.argv[5])
    if bad:
        print("check_board: %d of %d pixels differ, first at %s" % (len(bad), len(want), bad[:5]))
        sys.exit(1)
    print("check_board: the 64 squares equal the position (%d pixels)" % len(want))


if __name__ == "__main__":
    main()
