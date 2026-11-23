#!/usr/bin/env python3
"""Checks of a geoChess session's VRAM dump (x16emu -dump V) for GC2.

    check_extra.py bell <dump.bin> ringing|silent
        VERA's PSG voice 0 (VRAM $1F9C0): the bell's frequency word
        ($1274, about 1760 Hz), both channels, a triangle; ringing: the
        volume on its way down (1-62), silent: 0
    check_extra.py colours <dump.bin> on|off
        Veranda Look 2 (Veranda's DECISIONS V74, V77, V79): the colour map
        shown (DC_VIDEO layers 0 and 1), palette entries 0-15 Look 2's
        light ones (theme_palette), its paper per card: the board's 64
        squares 6 x 6 cards each from card (6, 6), light 3 (SIDEBAR) and
        dark 14 (SAGE) as geochess.h's COLOR_LIGHT, COLOR_DARK when on, 0
        (the normal paper) when off; the ring of cards round them (the
        board's frame: column 5 and 54, rows 5 and 54) the chrome (2); the
        menu bar's cards (rows 0-1) the chrome, the Veranda mark's two the
        cyan (11), none covered; the move log's window (cards 58-78, rows
        6-55): its title band (rows 6-7) the theme's accent (the title
        colour: 3, the default Sunroom theme's), its log the normal paper, its status bar (rows 54-55)
        the chrome; every other card 0 (a card under a dialog box: its
        own colour, kept in its tile index)
    check_extra.py tile <app.cvt> <tiles.txt> <name>
        the CVT carries a tile (its info block's flags bit 1; the tile
        block in its first block: the colour at 96, the 32 x 32 bitmap at
        97-224, Veranda's docs/APPFORMAT.md) equal to the tile <name> of
        Veranda's shell/tiles.txt, the one the shell shows for its class
"""
import sys

VRAM = 131072
TAIL = VRAM + 256 + 512 + 14 + 1024
MAP = 0x1AC00                    # 128 x 64 entries of 2 bytes (Veranda V30)
LIGHT, DARK, FRAME = 3, 14, 2       # Sunroom: mist and sage squares (geochess.h)
CHROME, CYAN, TITLE = 2, 11, 3      # TITLE: the default theme's accent (Sunroom, V132: the mist band)
PALETTE = [0xFFF, 0x223, 0xDDE, 0xEEF, 0x9CF, 0xACE, 0xFD5, 0xF87, 0x8D9, 0xB9F, 0xFA5, 0x6CE, 0x36A, 0x8BE,
           0x7A8, 0xDAC]


def parts(path):
    t = open(path, "rb").read()[-TAIL:]
    return t[:VRAM], t[VRAM:VRAM + 256]


def bell(path, want):
    v, _ = parts(path)
    lo, hi, lrvol, wave = v[0x1F9C0:0x1F9C4]
    vol = lrvol & 63
    bad = []
    if (lo, hi) != (0x74, 0x12):
        bad.append("frequency $%02X%02X, want $1274" % (hi, lo))
    if lrvol & 0xC0 != 0xC0:
        bad.append("channels $%02X, want both" % (lrvol & 0xC0))
    if wave & 0xC0 != 0x80:
        bad.append("waveform %d, want 2 (triangle)" % (wave >> 6))
    if want == "ringing" and not 0 < vol < 63:
        bad.append("volume %d, want on its way down" % vol)
    if want == "silent" and vol:
        bad.append("volume %d, want 0" % vol)
    for b in bad:
        print("check_extra bell:", b)
    if bad:
        sys.exit(1)
    print("check_extra: the bell, voice 0: $1274, both channels, a triangle, volume %d (%s)" % (vol, want))


def expected(cx, cy, board):
    """card cx, cy's own colour"""
    if cy < 2:
        return CYAN if cx < 2 else CHROME
    if 6 <= cx < 54 and 6 <= cy < 54:
        r, c = (cy - 6) // 6, (cx - 6) // 6
        return (DARK if (r + c) & 1 else LIGHT) if board else 0
    if 5 <= cx < 55 and 5 <= cy < 55:
        return FRAME
    if 58 <= cx < 79 and 6 <= cy < 56:
        return TITLE if cy < 8 else CHROME if cy >= 54 else 0
    return 0


def colours(path, want):
    v, comp = parts(path)
    on = comp[0] & 0x70 == 0x70
    if not on:
        sys.exit("check_extra colours: the colour map is not shown")
    t = open(path, "rb").read()[-TAIL:]
    p = t[VRAM + 256:VRAM + 256 + 32]
    pal = [p[2 * i] | p[2 * i + 1] << 8 for i in range(16)]
    if pal != PALETTE:
        sys.exit("check_extra colours: palette entries 0-15 %s, not Look 2's" % " ".join("$%03X" % c for c in pal))
    bad = 0
    for cy in range(60):
        for cx in range(80):
            tile, attr = v[MAP + (cy * 128 + cx) * 2:][:2]
            # a card under a menu or a box keeps its colour in its tile
            # index (colour << 4 | covers) and shows the normal paper
            # (Veranda V33): its own colour is GetCardColor's
            own = tile >> 4 if tile & 15 else attr >> 4
            exp = expected(cx, cy, want == "on")
            if own != exp or (cy < 2 and tile & 15):
                if bad < 5:
                    print("check_extra colours: card (%d, %d) has %d, want %d" % (cx, cy, own, exp))
                bad += 1
    if bad:
        sys.exit("check_extra colours: %d cards wrong" % bad)
    print("check_extra: the theme's palette; the colour map: the 64 squares %s, the board's frame %d, the bar %d "
          "(the mark %d), the move log's title band %d and status bar %d, every other card 0"
          % ("light %d and dark %d" % (LIGHT, DARK) if want == "on" else "the normal paper (0)", FRAME, CHROME,
             CYAN, TITLE, CHROME))


COLOURS = ['paper', 'ink', 'chrome', 'sidebar', 'select', 'title', 'yellow', 'red', 'green', 'purple',
           'orange', 'cyan', 'sky', 'sky2', 'hill', 'dusk']


def tile(cvt, tiles, name):
    rows, colour, cur = [], None, None
    for line in open(tiles, encoding="utf-8"):
        w = line.split(None, 1)
        if w and w[0] == "tile":
            cur = w[1].strip()
            continue
        if cur != name:
            continue
        if w and w[0] == "colour":
            colour = COLOURS.index(w[1].strip())
        elif len(line.rstrip()) == 32 and not set(line.rstrip()) - set("#."):
            rows.append(line.rstrip())
    if len(rows) != 32 or colour is None:
        sys.exit("check_extra tile: no tile %s in %s" % (name, tiles))
    want = bytes(sum(0x80 >> i for i, ch in enumerate(r[x:x + 8]) if ch == "#") for r in rows for x in range(0, 32, 8))
    d = open(cvt, "rb").read()
    if not d[254 + 141] & 2:
        sys.exit("check_extra tile: %s has no tile block" % cvt)
    if d[96] != colour or d[97:225] != want:
        sys.exit("check_extra tile: %s's tile (colour %d) is not the shell's %s (colour %d)" % (cvt, d[96], name, colour))
    print("check_extra: its tile is the shell's \"%s\" (%s, %d ink pixels)"
          % (name, COLOURS[colour], sum(bin(b).count("1") for b in want)))


def main():
    if len(sys.argv) == 5 and sys.argv[1] == "tile":
        return tile(*sys.argv[2:])
    if len(sys.argv) != 4 or sys.argv[1] not in ("bell", "colours"):
        sys.exit(__doc__)
    {"bell": bell, "colours": colours}[sys.argv[1]](sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    main()
