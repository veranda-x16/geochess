#!/usr/bin/env python3
"""geoChess's graphics for the Veranda port, from upstream's own files.

    mkpieces.py font  <font.cvt> <out.h>    the font's record 16 as a C array
                                            (LoadCharSet: GEOS's font format)
    mkpieces.py icon  <geochess.icon> <out.pbm>
                                            the 24x21 application icon
    mkpieces.py squares <font.cvt> <out.h>  the 640x480 board's squares

The squares: 48 x 48 pixels (6 cards), each glyph of the font scaled to
38 x 32 with Scale2x (tools/cvtfont.py) and placed at (5, 8) on its
square's fill (0 paper, 1 ink), as GEOS compacted bitmaps for WBitmapUp:
square_bits[colour][piece], colour 0 light, 1 dark; piece geochess.h's
display piece (0 empty, 1-6 white K Q B N R P, 7-12 black).

The files are read as data only (tools/cvtfont.py parses the CVT).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cvtfont  # noqa: E402


def c_array(name, data, out):
    out.write("static const unsigned char %s[%d] = {\n" % (name, len(data)))
    for i in range(0, len(data), 16):
        out.write("    " + ", ".join("0x%02X" % b for b in data[i:i + 16]) + ",\n")
    out.write("};\n")


def cmd_font(src, dst):
    rec = cvtfont.records(open(src, "rb").read())[16]
    with open(dst, "w") as f:
        f.write("/* %s, record 16 (tools/mkpieces.py font): a GEOS font */\n" % os.path.basename(src))
        c_array("chess_font", rec, f)


def cmd_icon(src, dst):
    d = open(src, "rb").read()[:63]
    with open(dst, "w") as f:
        f.write("P1\n# 24x21: geoChess (src/geochess.icon)\n24 21\n")
        for r in range(21):
            f.write(" ".join("1" if d[r * 3 + x // 8] & (0x80 >> (x % 8)) else "0" for x in range(24)) + "\n")


SQ, GX, GY = 48, 5, 8
GLYPHS = ["\0ABCDEFMNOPQR", "\0STUVWXGHIJKL"]     # geochess.c's GetPieceChar


def square(rows, colour):
    """48 x 48: the fill, the scaled glyph; 6 bytes a row"""
    px = [[colour] * SQ for _ in range(SQ)]
    if rows:
        for y, row in enumerate(cvtfont.scale2x(rows)):
            for x, b in enumerate(row):
                px[GY + y][GX + x] = int(b)
    out = bytearray()
    for y in range(SQ):
        for c in range(SQ // 8):
            v = 0
            for x in range(8):
                v = v << 1 | px[y][c * 8 + x]
            out.append(v)
    return bytes(out)


def compact(data):
    """GEOS's compacted bitmap: n (1-127) repeats the next byte n times,
    128 + n (n 1-91): n bytes follow as they are (220-255 are GEOS's
    BIGCOUNT, not used here)"""
    out = bytearray()
    i = 0
    lit = bytearray()

    def flush():
        while lit:
            n = min(len(lit), 91)
            out.append(128 + n)
            out.extend(lit[:n])
            del lit[:n]
    while i < len(data):
        j = i
        while j < len(data) and data[j] == data[i] and j - i < 127:
            j += 1
        if j - i >= 3:
            flush()
            out.append(j - i)
            out.append(data[i])
            i = j
        else:
            lit.append(data[i])
            i += 1
    flush()
    return bytes(out)


def uncompact(data, n):
    out = bytearray()
    i = 0
    while len(out) < n:
        c = data[i]
        if c < 128:
            out.extend([data[i + 1]] * c)
            i += 2
        else:
            out.extend(data[i + 1:i + 1 + c - 128])
            i += 1 + c - 128
    return bytes(out)


def cmd_squares(src, dst):
    _, _, glyphs = cvtfont.font(cvtfont.records(open(src, "rb").read())[16])
    total = 0
    with open(dst, "w") as f:
        f.write("/* the 640x480 board's squares (tools/mkpieces.py squares, from %s):\n"
                "   48 x 48, compacted; square_bits[colour][piece] */\n" % os.path.basename(src))
        names = [[], []]
        for colour in range(2):
            for piece in range(13):
                g = GLYPHS[colour][piece]
                raw = square(glyphs[ord(g)] if piece else None, colour)
                packed = compact(raw)
                assert uncompact(packed, len(raw)) == raw
                total += len(packed)
                name = "sq_%d_%d" % (colour, piece)
                c_array(name, packed, f)
                names[colour].append(name)
        f.write("static const unsigned char * const square_bits[2][13] = {\n")
        for colour in range(2):
            f.write("    { " + ", ".join(names[colour]) + " },\n")
        f.write("};\n/* %d bytes */\n" % total)


def main():
    cmds = {"font": cmd_font, "icon": cmd_icon, "squares": cmd_squares}
    if len(sys.argv) != 4 or sys.argv[1] not in cmds:
        sys.exit(__doc__)
    cmds[sys.argv[1]](sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    main()
