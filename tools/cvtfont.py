#!/usr/bin/env python3
"""Read a GEOS font from a CVT file (GEOS Convert 2.x, VLIR).

    cvtfont.py dump <font.cvt>            records, and each glyph as text

Used by tools/mkpieces.py. Reads only: the file is data.
"""
import sys


def records(data):
    """the VLIR records of a CVT file: {index: bytes}"""
    if data[21] != 1:
        raise ValueError("not a VLIR file")
    table = data[508:762]
    out = {}
    pos = 762
    for i in range(127):
        blocks, last = table[2 * i], table[2 * i + 1]
        if blocks == 0 and last == 0:
            break
        if blocks == 0 and last == 0xFF:
            continue
        n = (blocks - 1) * 254 + (last - 1)
        out[i] = data[pos:pos + n]
        pos += blocks * 254
    return out


def font(rec):
    """a GEOS font record: baseline, height, and {char: rows of '0'/'1'}"""
    baseline, setwidth, height = rec[0], rec[1] | rec[2] << 8, rec[3]
    index = rec[4] | rec[5] << 8
    bits = rec[6] | rec[7] << 8
    offs = [rec[index + 2 * i] | rec[index + 2 * i + 1] << 8 for i in range(97)]
    glyphs = {}
    for c in range(96):
        a, b = offs[c], offs[c + 1]
        if b <= a:
            continue
        rows = []
        for r in range(height):
            row = rec[bits + r * setwidth:bits + (r + 1) * setwidth]
            rows.append("".join("1" if row[x >> 3] & (0x80 >> (x & 7)) else "0" for x in range(a, b)))
        glyphs[c + 32] = rows
    return baseline, height, glyphs


def scale2x(rows):
    """EPX / Scale2x: each pixel to 2 x 2, diagonals smoothed"""
    h, w = len(rows), len(rows[0])

    def p(x, y):
        x = min(max(x, 0), w - 1)
        y = min(max(y, 0), h - 1)
        return rows[y][x]
    out = [[None] * (2 * w) for _ in range(2 * h)]
    for y in range(h):
        for x in range(w):
            e = p(x, y)
            b, d, f, hh = p(x, y - 1), p(x - 1, y), p(x + 1, y), p(x, y + 1)
            e0 = d if (d == b and b != f and d != hh) else e
            e1 = f if (b == f and b != d and f != hh) else e
            e2 = d if (d == hh and d != b and hh != f) else e
            e3 = f if (hh == f and d != hh and b != f) else e
            out[2 * y][2 * x], out[2 * y][2 * x + 1] = e0, e1
            out[2 * y + 1][2 * x], out[2 * y + 1][2 * x + 1] = e2, e3
    return ["".join(r) for r in out]


def main():
    if len(sys.argv) != 3 or sys.argv[1] != "dump":
        sys.exit(__doc__)
    data = open(sys.argv[2], "rb").read()
    recs = records(data)
    print("records", {k: len(v) for k, v in recs.items()})
    for k, rec in recs.items():
        base, h, g = font(rec)
        print("record", k, "baseline", base, "height", h)
        for c, rows in g.items():
            print("char %r width %d" % (chr(c), len(rows[0])))
            for r in rows:
                print("  " + r.replace("0", ".").replace("1", "#"))


if __name__ == "__main__":
    main()
